// ============================================================================
//  NewTexturePopup.cpp - el formulario de "Nueva textura" (ver NewTexturePopup.h).
//  Misma tarjeta de propiedades que el panel de Add (RedoMeshPanel): filas navegables con el
//  teclado, tap/click en cada campo, arrastre en los numericos y tap para escribir el valor.
//  OJO con las coordenadas: el popup dibuja con su propio viewport (initView), asi que los
//  botones guardan sx/sy LOCALES al popup; el hit-test se hace con (mx - x, my - y).
// ============================================================================
#include "w3dGraphics.h"
#include "app/W3dKeyboard.h"   // W3dKeyboardOpen: el teclado del celular (y el layout que se adapta)
#include "W3dLang.h"
#include "NewTexturePopup.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropColor.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/bitmapText.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "ColorPicker.h"
#include "NumPad.h"
#include "io/GeneratedTexture.h"
#include "W3dNotice.h"                       // Notificar
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>                   // SDL_StartTextInput (teclado del celular para el nombre)
#endif

extern bool g_redraw;

static NewTexturePopup* gNT = NULL;
extern void (*g_newCreatedTextureHook)(const std::string& path);
static float gNTWidth = 256.0f, gNTHeight = 256.0f;
static float gNTcolor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
static bool  gNTalpha = true;
static int   gNTType = TexGenUVGrid;

static void NTClose() { if (gNT) gNT->Close(); }
static void NTCancel() { NTClose(); }
static void NTCreate();
static void NTOpenList() { if (gNT) gNT->OpenList(); }
// el COLOR y el ALFA son del "Color plano" (las grillas tienen los suyos): con otro tipo no se ven
static void NTTypeRows() {
    if (!gNT) return;
    const bool plane = (gNTType == TexGenBlank);
    if (gNT->pColor) gNT->pColor->value = plane ? gNTcolor : NULL;
    if (gNT->pAlpha) gNT->pAlpha->value = plane ? &gNTalpha : NULL;
}
static void NTsetType(int type) {
    if (type < 0 || type >= TexGenTypes) return;
    gNTType = type;
    if (gNT && gNT->pType) gNT->pType->button->text = T(TexGenTypeName(gNTType));
    NTTypeRows();
    if (gNT) { gNT->ResizeGroup(); gNT->Relocate(); }
    g_redraw = true;
}

NewTexturePopup::NewTexturePopup()
    : PopUpBase(T("New Texture")), group(NULL), pName(NULL), pWidth(NULL), pHeight(NULL), pColor(NULL), pAlpha(NULL),
      pType(NULL), pButtons(NULL), dragField(NULL), dragMoved(false), lastDragMx(0),
      pickerOpen(false), listOpen(false), listSel(0), list(NULL) {
    group = new GroupPropertie(T("New Texture"));
    group->valueWidth = 0.55f;
    group->arrow = false;   // ventana flotante: sin la flecha de plegar
    pName = new PropText(T("Name"), "Texture");
    group->properties.push_back(pName);
    pWidth = new PropFloat(T("Width"), "px");
    pWidth->value = &gNTWidth; pWidth->integer = true; pWidth->centered = true; pWidth->arrows = true;
    pWidth->fineStep = 1.0f; pWidth->thickStep = 1.0f; pWidth->dragStep = 1.0f; pWidth->SetRange(8.0f, 4096.0f);
    group->properties.push_back(pWidth);
    pHeight = new PropFloat(T("Height"), "px");
    pHeight->value = &gNTHeight; pHeight->integer = true; pHeight->centered = true; pHeight->arrows = true;
    pHeight->fineStep = 1.0f; pHeight->thickStep = 1.0f; pHeight->dragStep = 1.0f; pHeight->SetRange(8.0f, 4096.0f);
    group->properties.push_back(pHeight);
    pColor = new PropColor(T("Color"));
    pColor->value = gNTcolor;
    group->properties.push_back(pColor);
    pAlpha = new PropBool(T("Alpha"));
    pAlpha->value = &gNTalpha;
    group->properties.push_back(pAlpha);
    pType = new PropButton(T("Type"));
    pType->withLabel = true; pType->button->text = T(TexGenTypeName(gNTType)); pType->action = NTOpenList;
    group->properties.push_back(pType);
    pButtons = new PropButtonRow();
    pButtons->Add(T("Cancel"), NTCancel);
    pButtons->Add(T("Create"), NTCreate);
    pButtons->active = 1;
    group->properties.push_back(pButtons);
    group->selectIndex = 0;
    list = new Card();
    ResizeGroup();
    Relocate();
}
NewTexturePopup::~NewTexturePopup() { delete group; delete list; }

void NewTexturePopup::ResizeGroup() {
    int w = 240 * GlobalScale;
    int maxW = MenuScreenW - gapGS * 4;
    if (w > maxW) w = maxW;
    group->Resize(w, 0);
    int height = group->height;
    if (listOpen) {   // la ventana (y su scissor) tiene que abarcar el desplegable, que cuelga por debajo de la tarjeta
        int lx, ly, lw, lh; ListRect(lx, ly, lw, lh);
        if (ly + lh + borderGS > height) height = ly + lh + borderGS;
    }
    popUpWindow->Resize(w, height);
}
void NewTexturePopup::Relocate() {   // centrado en la pantalla (por la tarjeta: el desplegable no la mueve)
    x = (MenuScreenW - popUpWindow->width) / 2;
    y = (MenuScreenH - group->height) / 2;
    // con el teclado numerico (u otro popup) abierto ENCIMA, el formulario se corre hacia arriba para no quedar tapado
    if (PopUpActive && PopUpActive != this && PopUpActive != colorPicker && PopUpActive->y < y + group->height + gapGS * 2)
        y = PopUpActive->y - group->height - gapGS * 2;
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}
// y local del renglon idx (misma cuenta que el hit-test: titulo + filas anteriores)
int NewTexturePopup::RowY(int idx) const {
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)group->properties.size(); j++) yRow += group->properties[j]->Resize(group->width);
    return yRow;
}
// el desplegable del tipo: pegado debajo de la fila Type, del ancho de la caja de valores
void NewTexturePopup::ListRect(int& lx, int& ly, int& lw, int& lh) const {
    int idx = 0; for (size_t j = 0; j < group->properties.size(); j++) if (group->properties[j] == pType) idx = (int)j;
    lx = group->colLabel + 2 * borderGS;
    lw = group->propertiBox->width - bordersGS;
    ly = RowY(idx) + RowHeightGS + gapGS;
    lh = TexGenTypes * (RowHeightGS + gapGS) + gapGS;
}
void NewTexturePopup::OpenList() { listOpen = !listOpen; listSel = gNTType; g_redraw = true; }

void NewTexturePopup::Render() {
    if (!group) return;
    ResizeGroup(); Relocate();
    initView();
    w3dEngine::PushMatrix();
    group->Render();
    w3dEngine::PopMatrix();
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    group->card->RenderBorder(false);
    if (listOpen) {   // el desplegable del tipo, encima de lo que haya debajo
        int lx, ly, lw, lh; ListRect(lx, ly, lw, lh);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)lx, (GLfloat)ly, 0);
        SetColorID(ColorID::headerColor, 1.0f);
        list->Resize(lw, lh); list->RenderObject(false);
        w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
        list->RenderBorder(false);
        for (int t = 0; t < TexGenTypes; t++) {
            const int ry = gapGS + t * (RowHeightGS + gapGS);
            if (t == listSel) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)borderGS, (GLfloat)ry, 0);
                SetColorID(ColorID::accent, 0.35f);
                list->Resize(lw - bordersGS, RowHeightGS); list->RenderObject(false);
                w3dEngine::PopMatrix();
            }
            SetColorID((t == gNTType) ? ColorID::accent : ColorID::white, 1.0f);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(ry + (RowHeightGS - LetterHeightGS) / 2), 0);
            RenderBitmapText(T(TexGenTypeName(t)), textAlign::left, lw - bordersGS - gapGS);
            w3dEngine::PopMatrix();
        }
        w3dEngine::PopMatrix();
    }
    endView();
    if (pickerOpen && colorPicker) colorPicker->Render();   // el selector de color, encima del formulario
}

bool NewTexturePopup::Click(int mx, int my) {
    // el selector de color abierto se queda con TODOS los clicks: adentro los maneja el, afuera lo cierra
    if (pickerOpen && colorPicker) {
        if (!colorPicker->Click(mx, my)) colorPicker->Close();
        if (!colorPicker->target) pickerOpen = false;   // Close() suelta el target: ya no esta
        g_redraw = true;
        return true;
    }
    const int myL = my - y, lx = mx - x;
    if (listOpen) {   // el desplegable del tipo: elegir o cerrarlo
        int ax, ay, aw, ah; ListRect(ax, ay, aw, ah);
        listOpen = false;
        if (lx >= ax && lx < ax + aw && myL >= ay && myL < ay + ah) {
            const int t = (myL - ay - gapGS) / (RowHeightGS + gapGS);
            NTsetType(t);
            return true;
        }
        g_redraw = true;
    }
    if (!Contains(mx, my)) return false;   // afuera -> el caller lo cierra (cancela)
    const int boxStart = group->colLabel + 2 * borderGS;
    const int boxW = group->propertiBox->width - bordersGS;
    const int aw = RowHeightGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h) {
            group->selectIndex = (int)j;
            const PropertyType::Enum type = p->GetType();
            if (type != PropertyType::Text) g_activeTextField = NULL;   // otro control: el nombre deja de editarse (queda lo escrito)
            if (type == PropertyType::Float) {
                PropFloat* pf = static_cast<PropFloat*>(p);
                // las flechas del tamano van por POTENCIAS DE 2 (8, 16, ... 4096): lo que se usa en texturas
                if (lx >= boxStart && lx < boxStart + aw) { pf->Set(*pf->value * 0.5f); g_redraw = true; return true; }
                if (lx >= boxStart + boxW - aw && lx < boxStart + boxW) { pf->Set(*pf->value * 2.0f); g_redraw = true; return true; }
                dragField = pf; dragMoved = false; lastDragMx = mx;
            } else if (type == PropertyType::Bool || type == PropertyType::Button) {
                p->EditPropertie();   // el boton "Type" abre el desplegable (NTOpenList)
            } else if (type == PropertyType::Text) {
                g_activeTextField = NULL;
                p->EditPropertie();
                #ifndef W3D_SYMBIAN
                if (ToolbarUsesTouch()) KeyboardTextOpen();
                #endif
            } else if (type == PropertyType::Color) {
                if (!colorPicker) colorPicker = new ColorPicker();
                // a la derecha del formulario si entra, sino encima. Open() lo hace popup activo: se
                // vuelve a poner ESTE, que le reenvia los eventos mientras esta abierto
                int px = x + popUpWindow->width + gapGS * 2;
                if (px + 200 * GlobalScale > MenuScreenW) px = x + gapGS * 4;
                colorPicker->Open(gNTcolor, px, y);
                PopUpActive = this;
                pickerOpen = true;
            } else if (type == PropertyType::ButtonRow) {
                // los botones no guardan su rect al dibujarse dentro del popup: se calcula la celda por geometria
                // (misma cuenta que PropButtonRow::RenderPropertiBox: arrancan en el borde izquierdo del cuerpo)
                PropButtonRow* row = static_cast<PropButtonRow*>(p);
                const int cw = row->CellWidth(group->width), x0 = boxStart - group->colLabel;
                int cell = -1; int vis = 0;
                for (size_t b = 0; b < row->buttons.size(); b++) {
                    if (!row->buttons[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lx >= cx && lx < cx + cw) { cell = (int)b; break; }
                    vis++;
                }
                if (cell >= 0) { row->active = cell; if (row->actions[(size_t)cell]) row->actions[(size_t)cell](); }
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;   // adentro pero fuera de un campo: consumir
}
bool NewTexturePopup::Motion(int mx, int my) {
    if (pickerOpen && colorPicker) { colorPicker->Motion(mx, my); return true; }
    if (dragField && dragField->value) {
        const int dmx = mx - lastDragMx; lastDragMx = mx;
        if (dmx != 0) dragMoved = true;
        dragField->Set(*dragField->value + dmx * dragField->dragStep);
        g_redraw = true;
    }
    return true;
}
bool NewTexturePopup::Key(int key) {
    if (pickerOpen && colorPicker) {
        colorPicker->Key(key);
        if (!colorPicker->target) pickerOpen = false;
        g_redraw = true;
        return true;
    }
    if (!group || group->properties.empty()) return true;
    const int n = (int)group->properties.size();
    if (listOpen) {   // navegar el desplegable del tipo
        switch (key) {
            case LayoutKey::Up:   listSel = (listSel + TexGenTypes - 1) % TexGenTypes; break;
            case LayoutKey::Down: listSel = (listSel + 1) % TexGenTypes; break;
            case LayoutKey::Enter: NTsetType(listSel); listOpen = false; break;
            case LayoutKey::Cancel: listOpen = false; break;
            default: break;
        }
        g_redraw = true;
        return true;
    }
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (key == LayoutKey::Down) ? +1 : -1;
            int s = group->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (group->properties[s]->Selectable()) break; }
            group->selectIndex = s;
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            if (group->selectIndex < 0) group->selectIndex = 0;
            PropertieBase* sel = (group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (!sel) break;
            const PropertyType::Enum type = sel->GetType();
            if (type == PropertyType::Bool) sel->EditPropertie();
            else if (type == PropertyType::Button) NTsetType((gNTType + (key == LayoutKey::Right ? 1 : TexGenTypes - 1)) % TexGenTypes);
            else if (type == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value * (key == LayoutKey::Right ? 2.0f : 0.5f)); }
            else if (type == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->active += (key == LayoutKey::Right) ? 1 : -1; row->ActiveClamp(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ActiveClamp();
                if (row->active >= 0 && row->active < (int)row->actions.size() && row->actions[row->active]) row->actions[row->active]();
            } else if (sel && (sel->GetType() == PropertyType::Text || sel->GetType() == PropertyType::Bool || sel->GetType() == PropertyType::Button)) {
                sel->EditPropertie();
            } else if (sel && sel->GetType() == PropertyType::Color) {
                Click(x + group->colLabel + 3 * borderGS, y + RowY(group->selectIndex) + 1);   // como un click en la fila: abre el selector
            } else NTCreate();   // OK sobre cualquier otro campo: crear
            break;
        }
        case LayoutKey::Cancel: NTClose(); break;
    }
    g_redraw = true;
    return true;
}
bool NewTexturePopup::KeyRepeat(int key) {
    if (pickerOpen && colorPicker) return colorPicker->KeyRepeat(key);
    return true;
}
void NewTexturePopup::Wheel(int delta) { if (pickerOpen && colorPicker) colorPicker->Wheel(delta); }
void NewTexturePopup::Release() {
    if (pickerOpen && colorPicker) { colorPicker->Release(); return; }
    if (dragField && !dragMoved && dragField->value) {
        dragField->StartEditText();            // mouse: se escribe directo en el campo
        if (ToolbarUsesTouch()) KeyboardNumberOpen();       // tactil: ademas el teclado en pantalla (se abre abajo; el popup sube)
    }
    dragField = NULL;
}
bool NewTexturePopup::Dragging() { return (pickerOpen && colorPicker) ? colorPicker->Dragging() : (dragField != NULL); }
void NewTexturePopup::Close() {
    if (pickerOpen && colorPicker) { colorPicker->Close(); pickerOpen = false; }
    listOpen = false;
    g_activeTextField = NULL;
    PopUpBase::Close();
    g_redraw = true;
}

static void NTCreate() {
    if (!gNT) return;
    std::string name = gNT->pName ? gNT->pName->field.text : std::string("Texture");
    if (name.find_first_not_of(" \t") == std::string::npos) { if (gNT->pName) gNT->pName->error = true; g_redraw = true; return; }
    const int w = (int)(gNTWidth + 0.5f), h = (int)(gNTHeight + 0.5f);
    std::string msg;
    const std::string path = W3dCreateTextureProject(name, gNTType, w, h, gNTcolor, gNTType == TexGenBlank && gNTalpha, msg);
    if (path.empty()) { Notify(T(msg.c_str()), true); return; }
    Notify(std::string(T("New Texture")) + ": " + name, false);
    if (g_newCreatedTextureHook) { void (*h)(const std::string&) = g_newCreatedTextureHook; g_newCreatedTextureHook = NULL; h(path); }
    NTClose();
}

// quien abrio el popup quiere la textura nueva (la tarjeta "Bake Lighting"): se llama una vez, al crearla
void (*g_newCreatedTextureHook)(const std::string& path) = NULL;
void OpenNewTexturePopup() {
    if (gNT) { if (PopUpActive == gNT) PopUpActive = NULL; delete gNT; gNT = NULL; }
    gNT = new NewTexturePopup();
    NTTypeRows();
    gNT->ResizeGroup(); gNT->Relocate();
    PopUpActive = gNT;
    g_redraw = true;
}
