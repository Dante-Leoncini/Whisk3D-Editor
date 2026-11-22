// ============================================================================
//  ExportUVPopup.cpp - el formulario de "Export UV Layout" (ver ExportUVPopup.h). Misma mecanica que el de
//  "Nueva textura": filas navegables con el teclado, tap/click en cada campo, arrastre en los numericos y tap
//  para escribir el valor (con el teclado en pantalla en tactil). Coordenadas LOCALES al popup en el hit-test.
// ============================================================================
#include "w3dGraphics.h"
#include "app/W3dKeyboard.h"   // W3dKeyboardOpen: el teclado del celular (y el layout que se adapta)
#include "W3dLang.h"
#include "ExportUVPopup.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "ViewPorts/UVEditor.h"             // UVShownTexture
#include "FileBrowser.h"                    // OpenFileBrowser / W3dPathInFolder
#include "ConfirmPopup.h"                   // sobrescribir un PNG que ya existe
#include "NumPad.h"
#include "io/UVExport.h"
#include "io/w3dFilesystem.h"
#include "objects/Mesh.h"
#include "W3dNotice.h"                      // Notificar
#include "variables.h"                      // ActiveObj
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>                   // SDL_StartTextInput (teclado del celular para el nombre)
#endif

extern bool g_redraw;

static ExportUVPopup* gEU = NULL;
static std::string gEUName, gEUFolder;
static float gEUWidth = 512.0f, gEUHeight = 512.0f;
static bool gEUKeep = false;   // reabrir tras elegir carpeta: no pisar lo tipeado con los defaults

static Mesh* EUMesh() { return (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL; }
static void EUSaveFields() {   // lo tipeado -> estado (sobrevive a cerrar/reabrir el formulario)
    if (!gEU) return;
    if (gEU->pName) gEUName = gEU->pName->field.text;
    if (gEU->pFolder) gEUFolder = gEU->pFolder->field.text;
}
static void EUClose() { if (gEU) gEU->Close(); }
static void EUExport();
static void EUPickedFolder(const std::string& picked) {
    if (!picked.empty()) {
        if (w3dFileSystem::IsDir(picked)) gEUFolder = picked;
        else {   // eligio un PNG: su carpeta + su nombre (sobrescribirlo)
            gEUFolder = w3dFileSystem::ParentPath(picked);
            const size_t sl = picked.find_last_of("/\\");
            gEUName = (sl == std::string::npos) ? picked : picked.substr(sl + 1);
        }
    }
    gEUKeep = true;
    OpenExportUVPopup();
}
static void EUPickFolder() {
    EUSaveFields();
    EUClose();
    OpenFileBrowser(T("Export UV Layout"), T("Use this folder"), ".png", EUPickedFolder, true);
}

ExportUVPopup::ExportUVPopup()
    : PopUpBase(T("Export UV Layout")), group(NULL), pName(NULL), pFolder(NULL), pWidth(NULL), pHeight(NULL),
      pButtons(NULL), dragField(NULL), dragMoved(false), lastDragMx(0) {
    group = new GroupPropertie(T("Export UV Layout"));
    group->valueWidth = 0.62f;
    group->arrow = false;   // ventana flotante: sin la flecha de plegar
    pName = new PropText(T("Name"), gEUName);
    group->properties.push_back(pName);
    pFolder = new PropText(T("Folder"), gEUFolder);
    pFolder->onClick = EUPickFolder;   // la carpeta se elige con el explorador (no se tipea)
    group->properties.push_back(pFolder);
    pWidth = new PropFloat(T("Width"), "px");
    pWidth->value = &gEUWidth; pWidth->integer = true; pWidth->centered = true; pWidth->arrows = true;
    pWidth->fineStep = 1.0f; pWidth->thickStep = 1.0f; pWidth->dragStep = 1.0f; pWidth->SetRange(8.0f, 8192.0f);
    group->properties.push_back(pWidth);
    pHeight = new PropFloat(T("Height"), "px");
    pHeight->value = &gEUHeight; pHeight->integer = true; pHeight->centered = true; pHeight->arrows = true;
    pHeight->fineStep = 1.0f; pHeight->thickStep = 1.0f; pHeight->dragStep = 1.0f; pHeight->SetRange(8.0f, 8192.0f);
    group->properties.push_back(pHeight);
    pButtons = new PropButtonRow();
    pButtons->Add(T("Cancel"), EUClose);
    pButtons->Add(T("Export"), EUExport);
    pButtons->active = 1;
    group->properties.push_back(pButtons);
    group->selectIndex = 0;
    ResizeGroup();
    Relocate();
}
ExportUVPopup::~ExportUVPopup() { delete group; }

void ExportUVPopup::ResizeGroup() {
    int w = 260 * GlobalScale;
    const int maxW = MenuScreenW - gapGS * 4;
    if (w > maxW) w = maxW;
    group->Resize(w, 0);
    popUpWindow->Resize(w, group->height);
}
void ExportUVPopup::Relocate() {   // centrado; con el teclado en pantalla abierto ENCIMA, el formulario sube
    x = (MenuScreenW - popUpWindow->width) / 2;
    y = (MenuScreenH - group->height) / 2;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + group->height + gapGS * 2)
        y = PopUpActive->y - group->height - gapGS * 2;
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}
int ExportUVPopup::RowY(int idx) const {
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)group->properties.size(); j++) yRow += group->properties[j]->Resize(group->width);
    return yRow;
}

void ExportUVPopup::Render() {
    if (!group) return;
    ResizeGroup(); Relocate();
    initView();
    w3dEngine::PushMatrix();
    group->Render();
    w3dEngine::PopMatrix();
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    group->card->RenderBorder(false);
    endView();
}

bool ExportUVPopup::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera -> el caller lo cierra (cancela)
    const int myL = my - y, lx = mx - x;
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
                // flechas: mitad / doble (las texturas van por potencias de 2); el valor exacto se tipea
                if (lx >= boxStart && lx < boxStart + aw) { pf->Set(*pf->value * 0.5f); g_redraw = true; return true; }
                if (lx >= boxStart + boxW - aw && lx < boxStart + boxW) { pf->Set(*pf->value * 2.0f); g_redraw = true; return true; }
                dragField = pf; dragMoved = false; lastDragMx = mx;
            } else if (type == PropertyType::Text) {
                g_activeTextField = NULL;
                const bool isFolder = (p == pFolder);
                p->EditPropertie();   // la carpeta abre el explorador (cierra este formulario); el nombre se edita
                if (isFolder) return true;
                #ifndef W3D_SYMBIAN
                if (ToolbarUsesTouch()) KeyboardTextOpen();
                #endif
            } else if (type == PropertyType::ButtonRow) {
                // las celdas por geometria (los botones no guardan su rect al dibujarse dentro del popup)
                PropButtonRow* row = static_cast<PropButtonRow*>(p);
                const int cw = row->CellWidth(group->width), x0 = boxStart - group->colLabel;
                int cell = -1, vis = 0;
                for (size_t b = 0; b < row->buttons.size(); b++) {
                    if (!row->buttons[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lx >= cx && lx < cx + cw) { cell = (int)b; break; }
                    vis++;
                }
                if (cell >= 0) { row->active = cell; if (row->actions[(size_t)cell]) row->actions[(size_t)cell](); }
                return true;
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;   // adentro pero fuera de un campo: consumir
}
bool ExportUVPopup::Motion(int mx, int my) {
    (void)my;
    if (dragField && dragField->value) {
        const int dmx = mx - lastDragMx; lastDragMx = mx;
        if (dmx != 0) dragMoved = true;
        dragField->Set(*dragField->value + dmx * dragField->dragStep);
        g_redraw = true;
    }
    return true;
}
bool ExportUVPopup::Key(int key) {
    if (!group || group->properties.empty()) return true;
    const int n = (int)group->properties.size();
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
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value * (key == LayoutKey::Right ? 2.0f : 0.5f)); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->active += (key == LayoutKey::Right) ? 1 : -1; row->ActiveClamp(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ActiveClamp();
                if (row->active >= 0 && row->active < (int)row->actions.size() && row->actions[row->active]) row->actions[row->active]();
                return true;
            } else if (sel && sel->GetType() == PropertyType::Text) {
                sel->EditPropertie();
                if (sel == pFolder) return true;   // abrio el explorador
            } else { EUExport(); return true; }    // OK sobre cualquier otro campo: exportar
            break;
        }
        case LayoutKey::Cancel: EUClose(); return true;
    }
    g_redraw = true;
    return true;
}
void ExportUVPopup::Release() {
    if (dragField && !dragMoved && dragField->value) {
        dragField->StartEditText();            // mouse: se escribe directo en el campo
        if (ToolbarUsesTouch()) KeyboardNumberOpen();       // tactil: ademas el teclado en pantalla (el formulario sube)
    }
    dragField = NULL;
}
bool ExportUVPopup::Dragging() { return dragField != NULL; }
void ExportUVPopup::Close() {
    g_activeTextField = NULL;
    PopUpBase::Close();
    g_redraw = true;
}

// el nombre con .png y la ruta final (carpeta + nombre)
static std::string EUPath() {
    std::string name = gEUName;
    const size_t a = name.find_first_not_of(" \t"); name = (a == std::string::npos) ? std::string() : name.substr(a);
    while (!name.empty() && (name[name.size() - 1] == ' ' || name[name.size() - 1] == '\t')) name.erase(name.size() - 1);
    if (name.empty()) return std::string();
    if (name.size() < 4 || name.substr(name.size() - 4) != ".png") name += ".png";
    std::string folder = gEUFolder.empty() ? w3dFileSystem::GetDefaultOutputDir() : gEUFolder;
    return W3dPathInFolder(folder, name, "");
}
bool ExportUVNow(std::string& pathOut, std::string& msg) {
    EUSaveFields();
    pathOut = EUPath();
    if (pathOut.empty()) { msg = "Name"; return false; }
    Mesh* m = EUMesh();
    const int w = (int)(gEUWidth + 0.5f), h = (int)(gEUHeight + 0.5f);
    return UVExportPNG(m, UVShownTexture(m), w, h, pathOut, msg);
}
static void EUDoExport() {
    std::string path, msg;
    if (ExportUVNow(path, msg)) Notify(std::string(T("UV layout saved")) + ": " + path, false);
    else                            Notify(T(msg.c_str()), true);
    g_redraw = true;
}
static void EUExport() {
    EUSaveFields();
    if (EUPath().empty()) { if (gEU && gEU->pName) gEU->pName->error = true; g_redraw = true; return; }
    if (!EUMesh() || !EUMesh()->uv) { Notify(T("No UVs to export"), true); return; }
    const std::string path = EUPath();
    EUClose();
#ifndef W3D_SYMBIAN
    if (w3dFileSystem::FileExists(path)) {   // ya existe: preguntar antes de pisarlo
        if (!confirmPopup) confirmPopup = new ConfirmPopup();
        confirmPopup->Open(std::string(T("The file already exists. Do you want to replace it?")) + "\n" + path, EUDoExport);
        return;
    }
#endif
    EUDoExport();
}

void OpenExportUVPopup() {
    if (!gEUKeep) {   // apertura normal: nombre y tamano de la textura mostrada (o los defaults)
        Mesh* m = EUMesh();
        Texture* t = UVShownTexture(m);
        int w = 512, h = 512; UVExportDefaultSize(t, w, h);
        gEUWidth = (float)w; gEUHeight = (float)h;
        gEUName = UVExportDefaultName(m, t);
        if (gEUFolder.empty()) gEUFolder = w3dFileSystem::GetDefaultOutputDir();
    }
    gEUKeep = false;
    if (gEU) { if (PopUpActive == gEU) PopUpActive = NULL; delete gEU; gEU = NULL; }
    gEU = new ExportUVPopup();
    PopUpActive = gEU;
    g_redraw = true;
}
