// ============================================================================
//  ChangesPopup.cpp — "Se perderan los cambios en:" (ver el .h)
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "ChangesPopup.h"
#include "ViewPorts/LayoutInput.h"   // LayoutKey
#include "WhiskUI/core/UI.h"
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/draw/icons.h"
#include "objects/Textures.h"
#include "render/RenderOptions.h"   // g_redraw
#include <cstdio>

ChangesPopup* changesPopup = NULL;

ChangesPopup::ChangesPopup() : PopUpBase("Changes") {
    action = W3D_CHANGES_EXIT;
    focus = 0; scroll = 0; visibleRows = 1;
    listX = listY = listW = listH = 0;
    dragging = false; dragY0 = 0; scroll0 = 0;
    for (int i = 0; i < 3; i++) { btn[i] = new Button(""); btn[i]->adapt = false; btn[i]->centered = true; }
}
ChangesPopup::~ChangesPopup() { for (int i = 0; i < 3; i++) delete btn[i]; }

void ChangesPopup::Open(int a) {
    action = a;
    W3dChangesList(changes);
    focus = 0; scroll = 0; dragging = false;
    // los textos, en el idioma de ahora y segun lo que se queria hacer
    btn[0]->text = T(a == W3D_CHANGES_EXIT ? "Save and quit" : a == W3D_CHANGES_OPEN ? "Save and open" :
                     a == W3D_CHANGES_CLOSE_SCENE ? "Save and close" : "Save and continue");
    btn[1]->text = T(a == W3D_CHANGES_EXIT ? "Quit without saving" : a == W3D_CHANGES_OPEN ? "Open without saving" :
                     a == W3D_CHANGES_CLOSE_SCENE ? "Close without saving" : "Continue without saving");
    btn[2]->text = T("Cancel");
    PopUpActive = this;
    g_redraw = true;
}
void ChangesPopupOpen(int a) {
    if (!changesPopup) changesPopup = new ChangesPopup();
    if (PopUpActive && PopUpActive != changesPopup) PopUpActive->Close();
    changesPopup->Open(a);
}

// centrado, 2/3 del ancho (toda la pantalla si es angosta: el N95), la lista con el alto que entre
void ChangesPopup::Layout() {
    int w = MenuScreenW * 2 / 3;
    if (w < 240 * GlobalScale || MenuScreenW < 400) w = MenuScreenW;
    const int pad = borderGS + GlobalScale * 3;
    const int row = (int)RowHeightGS;
    int maxRows = (MenuScreenH * 2 / 3 - 3 * row - 2 * pad) / row;
    if (maxRows < 3) maxRows = 3;
    visibleRows = (int)changes.size() < maxRows ? (int)changes.size() : maxRows;
    if (visibleRows < 1) visibleRows = 1;
    const int h = pad * 2 + row /*titulo*/ + gapGS + visibleRows * row + gapGS + (RowHeightGS + bordersGS);
    popUpWindow->Resize(w, h);
    x = (MenuScreenW - w) / 2;
    y = (MenuScreenH - h) / 2;
    if (y < 0) y = 0;
    listX = x + pad; listY = y + pad + row + gapGS; listW = w - 2 * pad; listH = visibleRows * row;
    const int maxScroll = (int)changes.size() - visibleRows;
    if (scroll > maxScroll) scroll = maxScroll;
    if (scroll < 0) scroll = 0;
}

void ChangesPopup::Render() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);
    const float* background = ColorList[static_cast<int>(ColorID::background)];
    const float* green      = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    popUpWindow->RenderBorder(false);
    const int w = popUpWindow->width;
    const int pad = borderGS + GlobalScale * 3;
    const int row = (int)RowHeightGS;
    // el titulo
    SetColorID(ColorID::white);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)pad, (GLfloat)pad, 0);
    RenderBitmapText(T("Changes will be lost in:"), textAlign::left, w - 2 * pad);
    w3dEngine::PopMatrix();
    // la LISTA (icono + tipo + nombre), recortada a su area
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(listX, W3dScreenHeight - listY - listH, listW, listH);
    const int ly = listY - y;
    const int colType = IconSizeGS + gapGS * 2;
    const int colName = colType + 12 * CharacterWidthGS;
    for (int i = 0; i < visibleRows && scroll + i < (int)changes.size(); i++) {
        const W3dChange& c = changes[(size_t)(scroll + i)];
        const int fy = ly + i * row;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)fy, 0);
        SetColorID(ColorID::accent);
        if (c.icon >= 0 && c.icon < (int)IconsUV.size()) W3dDrawStrip4(IconMesh, IconsUV[(size_t)c.icon]->uvs);
        w3dEngine::Translatef((GLfloat)colType, 0, 0);
        SetColorID(ColorID::grayUI);
        RenderBitmapText(c.type, textAlign::left, colName - colType - gapGS);
        w3dEngine::Translatef((GLfloat)(colName - colType), 0, 0);
        SetColorID(ColorID::white);
        RenderBitmapText(c.name + "*", textAlign::left, listW - colName);
        w3dEngine::PopMatrix();
    }
    w3dEngine::Disable(w3dEngine::ScissorTest);
    // la barrita de scroll (si no entra todo)
    if ((int)changes.size() > visibleRows) {
        char b[48]; snprintf(b, sizeof(b), "%d-%d / %d", scroll + 1, scroll + visibleRows, (int)changes.size());
        SetColorID(ColorID::grayUI, 0.7f);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)pad, 0);
        RenderBitmapText(b, textAlign::right, w - 2 * pad);
        w3dEngine::PopMatrix();
    }
    // los tres botones
    const int contW = w - pad * 2;
    const int third = (contW - 2 * gapGS) / 3;
    const int btnY = (listY - y) + listH + gapGS;
    for (int i = 0; i < 3; i++) {
        btn[i]->Resize(third);
        btn[i]->focusMenu = (i == focus);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(pad + i * (third + gapGS)), (GLfloat)btnY, 0);
        btn[i]->Render();
        w3dEngine::PopMatrix();
        btn[i]->sx = x + pad + i * (third + gapGS); btn[i]->sy = y + btnY;
    }
    endView();
}

void ChangesPopup::Respond(int r) {
    Close();
    W3dChangesRespond(r);
    g_redraw = true;
}

bool ChangesPopup::Motion(int mx, int my) {
    Layout();
    for (int i = 0; i < 3; i++) btn[i]->hover = btn[i]->Contains(mx, my);
    if (dragging) {
        // arrastrar la lista (mouse o dedo) = scroll
        const int d = (dragY0 - my) / (int)RowHeightGS;
        scroll = scroll0 + d;
        Layout();
        g_redraw = true;
    }
    return true;
}
bool ChangesPopup::Click(int mx, int my) {
    Layout();
    for (int i = 0; i < 3; i++) if (btn[i]->Contains(mx, my)) { Respond(i); return true; }
    if (mx >= listX && mx < listX + listW && my >= listY && my < listY + listH) {
        dragging = true; dragY0 = my; scroll0 = scroll;
    }
    return true;   // modal: afuera tampoco se cierra (hay que elegir)
}
void ChangesPopup::Release() { dragging = false; }
void ChangesPopup::Wheel(int delta) { scroll -= delta; Layout(); g_redraw = true; }
bool ChangesPopup::Key(int key) {
    switch (key) {
        case LayoutKey::Left:  if (focus > 0) focus--; g_redraw = true; return true;
        case LayoutKey::Right: if (focus < 2) focus++; g_redraw = true; return true;
        case LayoutKey::Up:    scroll--; Layout(); g_redraw = true; return true;
        case LayoutKey::Down:  scroll++; Layout(); g_redraw = true; return true;
        case LayoutKey::Enter:
        case LayoutKey::Accept: Respond(focus); return true;
        case LayoutKey::Cancel: Respond(2); return true;
    }
    return true;
}
