// ============================================================================
//  NamePopup.cpp - la ventanita que pide un nombre (ver el .h). Misma mecanica que "Export UV Layout": una tarjeta
//  con sus filas, coordenadas LOCALES al popup en el hit-test.
// ============================================================================
#include "w3dGraphics.h"
#include "ViewPorts/PopUp/NumPad.h"   // KeyboardTextOpen / KeyboardNumberOpen: el teclado en pantalla (Ajustes)
#include "app/W3dKeyboard.h"   // W3dKeyboardOpen: el teclado del celular
#include "W3dLang.h"
#include "NamePopup.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropText.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch

extern bool g_redraw;
NamePopup* namePopup = NULL;

static void NPCancel() { if (namePopup) namePopup->Close(); }
static void NPAccept() { if (namePopup) namePopup->Accept(); }

NamePopup::NamePopup(const std::string& title, const std::string& button, const std::string& initial,
                         void (*accept)(const std::string&))
    : PopUpBase(title), group(NULL), pName(NULL), pButtons(NULL), onAccept(accept) {
    group = new GroupPropertie(title);
    group->valueWidth = 0.66f;
    group->arrow = false;   // ventana flotante: sin la flecha de plegar
    pName = new PropText(T("Name"), initial);
    group->properties.push_back(pName);
    pButtons = new PropButtonRow();
    pButtons->Add(T("Cancel"), NPCancel);
    pButtons->Add(button, NPAccept);
    pButtons->active = 1;
    group->properties.push_back(pButtons);
    group->selectIndex = 0;
    Relocate();
}
NamePopup::~NamePopup() { delete group; }

void NamePopup::Relocate() {   // al medio; con el teclado en pantalla abierto ENCIMA, sube
    int w = 240 * GlobalScale;
    const int maxW = MenuScreenW - gapGS * 4;
    if (w > maxW) w = maxW;
    group->Resize(w, 0);
    popUpWindow->Resize(w, group->height);
    x = (MenuScreenW - popUpWindow->width) / 2;
    y = (MenuScreenH - group->height) / 3;
    if (PopUpActive && PopUpActive != this && PopUpActive->y < y + group->height + gapGS * 2)
        y = PopUpActive->y - group->height - gapGS * 2;
    if (x < gapGS) x = gapGS;
    if (y < gapGS) y = gapGS;
}

void NamePopup::Render() {
    Relocate();
    initView();
    w3dEngine::PushMatrix();
    group->Render();
    w3dEngine::PopMatrix();
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    group->card->RenderBorder(false);
    endView();
}

bool NamePopup::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera -> el caller lo cierra (cancela)
    const int myL = my - y, lx = mx - x;
    const int boxStart = group->colLabel + 2 * borderGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h) {
            group->selectIndex = (int)j;
            if (p == pName) {
                pName->EditPropertie();
                if (ToolbarUsesTouch()) KeyboardTextOpen();
            } else if (p == pButtons) {
                // las celdas por geometria (los botones no guardan su rect al dibujarse dentro del popup)
                const int cw = pButtons->CellWidth(group->width), x0 = boxStart - group->colLabel;
                int vis = 0;
                for (size_t b = 0; b < pButtons->buttons.size(); b++) {
                    if (!pButtons->buttons[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lx >= cx && lx < cx + cw) {
                        pButtons->active = (int)b;
                        if (pButtons->actions[b]) pButtons->actions[b]();
                        return true;
                    }
                    vis++;
                }
            }
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;   // adentro pero fuera de un campo: consumir
}

bool NamePopup::Key(int key) {
    switch (key) {
        case LayoutKey::Left:
        case LayoutKey::Right:
            pButtons->active += (key == LayoutKey::Right) ? 1 : -1;
            pButtons->ActiveClamp();
            break;
        case LayoutKey::Up: group->selectIndex = 0; break;
        case LayoutKey::Down: group->selectIndex = 1; break;
        case LayoutKey::Enter:
        case LayoutKey::Accept:
            if (group->selectIndex == 1 && pButtons->active == 0) Close();
            else Accept();
            return true;
        case LayoutKey::Cancel: Close(); return true;
    }
    g_redraw = true;
    return true;
}

bool NamePopup::KeyInText(bool enter) {
    if (g_activeTextField != &pName->field) return false;
    if (enter) Accept(); else Close();
    return true;
}

void NamePopup::Accept() {
    std::string n = pName->field.text;
    const size_t a = n.find_first_not_of(" \t");
    n = (a == std::string::npos) ? std::string() : n.substr(a);
    while (!n.empty() && (n[n.size() - 1] == ' ' || n[n.size() - 1] == '\t')) n.erase(n.size() - 1);
    if (n.empty()) { pName->error = true; g_redraw = true; return; }
    void (*cb)(const std::string&) = onAccept;
    Close();
    if (cb) cb(n);
}

void NamePopup::Close() {
    g_activeTextField = NULL;
    PopUpBase::Close();
    g_redraw = true;
}

void OpenNamePopup(const std::string& title, const std::string& button, const std::string& initial,
                      void (*accept)(const std::string& name)) {
    if (namePopup) { if (PopUpActive == namePopup) PopUpActive = NULL; delete namePopup; namePopup = NULL; }
    namePopup = new NamePopup(title, button, initial, accept);
    if (PopUpActive && PopUpActive != namePopup) PopUpActive->Close();
    PopUpActive = namePopup;
    // el campo ya enfocado: se escribe directo (todo elegido: tipear reemplaza el nombre que se propone)
    TextFieldFocus(&namePopup->pName->field);
    namePopup->pName->field.SelectAll();
    if (ToolbarUsesTouch()) KeyboardTextOpen();
    g_redraw = true;
}
