// ============================================================================
//  FindPopup.cpp - Buscar / Reemplazar del editor de scripts (ver FindPopup.h). Coordenadas LOCALES al panel en
//  el hit-test, con el desplazamiento de ScrollablePanel cuando no entra en la mitad de la pantalla.
// ============================================================================
#include "w3dGraphics.h"
#include "ViewPorts/PopUp/NumPad.h"   // KeyboardTextOpen / KeyboardNumberOpen: el teclado en pantalla (Ajustes)
#include "W3dLang.h"
#include "FindPopup.h"
#include "ViewPorts/IDE.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropBool.h"
#include "WhiskUI/Propieties/PropLabel.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/widgets/TextField.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "W3dNotice.h"                      // Notificar
#ifndef W3D_SYMBIAN
#include "app/W3dKeyboard.h"                 // W3dKeyboardOpen
#endif
#include <stdio.h>

extern bool g_redraw;

static FindPopup* gBP = NULL;
static std::string gText, gPreviousText, gReplacement;
static bool gCaps = false, gWord = false;
static int gAnchorLin = 0, gAnchorCol = 0;   // desde donde se busca mientras se tipea (el cursor al abrir)
static IdeSearch gActive;

static std::string Num(int n) { char b[16]; sprintf(b, "%d", n); return b; }

static void PrevAcc()  { if (gBP && gBP->ide) { gBP->Sync(); gBP->ide->GoToMatch(gText, gCaps, gWord, -1); } }
static void NextAcc() { if (gBP && gBP->ide) { gBP->Sync(); gBP->ide->GoToMatch(gText, gCaps, gWord, +1); } }
static void AccReplace() {
    if (!gBP || !gBP->ide || gText.empty()) return;
    gBP->Sync();
    IDE* ide = gBP->ide;
    // la seleccion ES una coincidencia: se reemplaza (un paso de deshacer) y se pasa a la siguiente; si no, primero se va a una
    if (ide->SelectionIsMatch(gText, gCaps)) ide->InsertText(gReplacement);
    ide->GoToMatch(gText, gCaps, gWord, +1);
}
static void AccReplaceAll() {
    if (!gBP || !gBP->ide || gText.empty()) return;
    gBP->Sync();
    const int n = gBP->ide->ReplaceAll(gText, gReplacement, gCaps, gWord);
    Notify(Num(n) + " " + T("replacements"), false);
}
static void AccClose() { if (gBP) gBP->Close(); }

FindPopup::FindPopup(IDE* ide_, bool replace_)
    : PopUpBase(""), ide(ide_), replace(replace_), group(NULL), pFind(NULL), pReplacement(NULL), pCount(NULL), pNav(NULL),
      bar(NULL), findFocused(false) {
    bar = new Card();
    Build();
}
FindPopup::~FindPopup() { delete group; delete bar; }

void FindPopup::Build() {
    delete group;
    group = new GroupPropertie(T(replace ? "Find and Replace" : "Find"));
    group->valueWidth = 0.62f;
    group->arrow = false;
    pFind = new PropText(T("Find"), gText);
    group->properties.push_back(pFind);
    pCount = new PropLabel(" ");
    group->properties.push_back(pCount);
    PropBool* m = new PropBool(T("Match case"));
    m->value = &gCaps;
    group->properties.push_back(m);
    PropBool* w = new PropBool(T("Whole word"));
    w->value = &gWord;
    group->properties.push_back(w);
    pNav = new PropButtonRow();
    pNav->Add(T("Previous"), PrevAcc);
    pNav->Add(T("Next"), NextAcc);
    pNav->active = 1;
    group->properties.push_back(pNav);
    pReplacement = NULL;
    if (replace) {
        pReplacement = new PropText(T("Replace"), gReplacement);
        group->properties.push_back(pReplacement);
        PropButtonRow* r = new PropButtonRow();
        r->Add(T("Replace"), AccReplace);
        r->Add(T("Replace all"), AccReplaceAll);
        group->properties.push_back(r);
    }
    PropButtonRow* c = new PropButtonRow();
    c->Add(T("Close"), AccClose);
    group->properties.push_back(c);
    group->selectIndex = 0;
    Relocate();
}

// arriba a la derecha del IDE, debajo de su barra (donde lo esperan los editores de codigo)
void FindPopup::Relocate() {
    int w = 240 * GlobalScale;
    if (ide && w > ide->width - gapGS * 4) w = ide->width - gapGS * 4;
    if (w < 140 * GlobalScale) w = 140 * GlobalScale;
    group->Resize(w, 0);
    dz.Measure(group->height);
    popUpWindow->Resize(w, dz.visible);
    if (ide) {
        x = ide->x + ide->width - w - gapGS * 2;
        y = ide->y + ide->BarTopOffset() + gapGS;
    } else { x = MenuScreenW - w - gapGS * 2; y = gapGS * 2; }
    if (x < gapGS) x = gapGS;
    if (y + dz.visible > MenuScreenH - gapGS) y = MenuScreenH - gapGS - dz.visible;
    if (y < gapGS) y = gapGS;
}
int FindPopup::RowY(int idx) const {
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)group->properties.size(); j++) yRow += group->properties[j]->Resize(group->width);
    return yRow;
}

void FindPopup::Sync() {
    if (!ide) return;
    gText = pFind->field.text;
    if (pReplacement) gReplacement = pReplacement->field.text;
    // mientras se tipea: la primera coincidencia desde donde estaba el cursor al abrir
    if (gText != gPreviousText) {
        gPreviousText = gText;
        ide->CursorA(gAnchorLin, gAnchorCol, false);
        if (!gText.empty()) ide->GoToMatch(gText, gCaps, gWord, +1);
    }
    // el campo Buscar solto el foco (Enter): el proximo Enter es "Siguiente"
    const bool focused = (g_activeTextField == &pFind->field);
    if (findFocused && !focused) {
        for (size_t j = 0; j < group->properties.size(); j++) if (group->properties[j] == pNav) group->selectIndex = (int)j;
        pNav->active = 1;
    }
    findFocused = focused;
    // "3 de 7"
    std::vector<int> L, C;
    ide->Matches(gText, gCaps, gWord, &L, &C);
    std::string count;
    if (gText.empty()) count = " ";
    else if (L.empty()) count = T("No results");
    else {
        int current = 0;
        if (ide->hasSel) {
            int a, b, c, d; ide->NormalizeSel(&a, &b, &c, &d);
            for (size_t i = 0; i < L.size(); i++) if (L[i] == a && C[i] == b) current = (int)i + 1;
        }
        count = current ? Num(current) + " " + T("of") + " " + Num((int)L.size()) : Num((int)L.size()) + " " + T("results");
    }
    pCount->name = count;
    gActive.ide = ide; gActive.text = gText; gActive.caps = gCaps; gActive.word = gWord;
}

void FindPopup::Render() {
    if (!group) return;
    Sync();
    Relocate();
    initView();
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)(-dz.scroll), 0);
    group->Render();
    w3dEngine::PopMatrix();
    // borde VERDE: es la ventana con el foco
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    popUpWindow->RenderBorder(false);
    if (dz.Scrollable()) {   // la barrita que dice cuanto falta
        const int height = dz.visible * dz.visible / (dz.total > 0 ? dz.total : 1);
        const int ty = (dz.total - dz.visible > 0) ? (dz.visible - height) * dz.scroll / (dz.total - dz.visible) : 0;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(popUpWindow->width - 3 * GlobalScale), (GLfloat)ty, 0);
        SetColorID(ColorID::accent, 0.7f);
        bar->Resize(2 * GlobalScale, height); bar->RenderObject(false);
        w3dEngine::PopMatrix();
    }
    endView();
}

void FindPopup::ClickInRow(int mx, int my) {
    const int myL = my - y + dz.scroll, lx = mx - x;
    const int boxStart = group->colLabel + 2 * borderGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h) {
            group->selectIndex = (int)j;
            const PropertyType::Enum type = p->GetType();
            if (type == PropertyType::Text) {
                g_activeTextField = NULL;
                p->EditPropertie();
#ifndef W3D_SYMBIAN
                if (ToolbarUsesTouch()) KeyboardTextOpen();
#endif
            } else if (type == PropertyType::Bool || type == PropertyType::Button) {
                g_activeTextField = NULL;
                p->EditPropertie();
            } else if (type == PropertyType::ButtonRow) {
                g_activeTextField = NULL;
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
            }
            g_redraw = true;
            return;
        }
        yRow += h;
    }
}
bool FindPopup::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera: NO se cierra (no es modal); el click sigue al IDE
    dz.Down(mx, my);
    if (!dz.Scrollable()) { dz.pressed = false; ClickInRow(mx, my); }   // entra entero: click directo
    return true;
}
bool FindPopup::Motion(int mx, int my) { if (dz.Move(mx, my)) g_redraw = true; return true; }
void FindPopup::Release() { if (dz.Up()) ClickInRow(dz.pressX, dz.pressY); }
bool FindPopup::Dragging() { return dz.pressed; }
void FindPopup::Wheel(int delta) { dz.Wheel(delta); g_redraw = true; }

bool FindPopup::Key(int key) {
    if (!group || group->properties.empty()) return true;
    const int n = (int)group->properties.size();
    PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[(size_t)group->selectIndex] : NULL;
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (key == LayoutKey::Down) ? +1 : -1;
            int s = group->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (group->properties[(size_t)s]->Selectable()) break; }
            group->selectIndex = s;
            dz.ToView(RowY(s), group->properties[(size_t)s]->Resize(group->width));
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right:
            if (sel && sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->active += (key == LayoutKey::Right) ? 1 : -1; row->ActiveClamp(); }
            else if (sel && sel->GetType() == PropertyType::Bool) sel->EditPropertie();
            break;
        case LayoutKey::Enter:
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ActiveClamp();
                if (row->active >= 0 && row->active < (int)row->actions.size() && row->actions[(size_t)row->active]) row->actions[(size_t)row->active]();
                return true;
            }
            if (sel) { g_activeTextField = NULL; sel->EditPropertie(); }   // OK: edita el campo SIN cerrar el panel
            break;
        case LayoutKey::Cancel: Close(); return true;   // Esc / Supr / C: se cierra
        default: break;
    }
    g_redraw = true;
    return true;
}
void FindPopup::Close() {
    if (g_activeTextField == &pFind->field || (pReplacement && g_activeTextField == &pReplacement->field)) g_activeTextField = NULL;
    PopUpBase::Close();
    g_redraw = true;
}

void IDEOpenFind(IDE* ide, bool replace) {
    if (!ide) return;
    // lo seleccionado (si es de una linea) arranca como texto a buscar, como en los editores de codigo
    if (ide->hasSel && ide->selLin == ide->curLin) gText = ide->TextSelected();
    int a = ide->curLin, b = ide->curCol;
    if (ide->hasSel) { int c, d; ide->NormalizeSel(&a, &b, &c, &d); }
    gAnchorLin = a; gAnchorCol = b;
    gPreviousText = gText;   // abrir no mueve el cursor: recien cambia al tipear
    if (gBP) { if (PopUpActive == gBP) PopUpActive = NULL; delete gBP; gBP = NULL; }
    gBP = new FindPopup(ide, replace);
    PopUpActive = gBP;
    // el foco al campo Buscar, con todo seleccionado: lo que se tipea lo reemplaza
    gBP->pFind->EditPropertie();
    gBP->pFind->field.SelectAll();
    gBP->findFocused = true;
#ifndef W3D_SYMBIAN
    if (ToolbarUsesTouch()) KeyboardTextOpen();
#endif
    g_redraw = true;
}
bool IDEFindOpen() { return gBP && PopUpActive == gBP; }
const IdeSearch* IDEActiveSearch() {
    if (!IDEFindOpen() || gActive.text.empty()) return NULL;
    return &gActive;
}
void IDEFindSet(const std::string& text, const std::string& replacement, int caps, int word) {
    if (!gBP) return;
    if (caps >= 0) gCaps = (caps != 0);
    if (word >= 0) gWord = (word != 0);
    gBP->pFind->field.SetText(text);
    if (gBP->pReplacement) gBP->pReplacement->field.SetText(replacement);
    g_activeTextField = NULL;
    gBP->Sync();
}
void IDEFindButton(int which) {
    switch (which) {
        case 0: PrevAcc(); break;
        case 1: NextAcc(); break;
        case 2: AccReplace(); break;
        case 3: AccReplaceAll(); break;
        case 4: AccClose(); break;
    }
    if (gBP && PopUpActive == gBP) gBP->Sync();
}
std::string IDEFindCount() { return (gBP && gBP->pCount) ? gBP->pCount->name : std::string(); }
