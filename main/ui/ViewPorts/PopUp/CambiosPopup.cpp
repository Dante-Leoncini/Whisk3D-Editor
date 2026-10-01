// ============================================================================
//  CambiosPopup.cpp — "Se perderan los cambios en:" (ver el .h)
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "CambiosPopup.h"
#include "ViewPorts/LayoutInput.h"   // LayoutKey
#include "WhiskUI/core/UI.h"
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/draw/icons.h"
#include "objects/Textures.h"
#include "render/OpcionesRender.h"   // g_redraw
#include <cstdio>

CambiosPopup* cambiosPopup = NULL;

CambiosPopup::CambiosPopup() : PopUpBase("Cambios") {
    accion = W3D_CAMBIOS_SALIR;
    foco = 0; scroll = 0; filasVisibles = 1;
    listaX = listaY = listaW = listaH = 0;
    arrastrando = false; arrastreY0 = 0; scroll0 = 0;
    for (int i = 0; i < 3; i++) { btn[i] = new Button(""); btn[i]->adaptar = false; btn[i]->centrado = true; }
}
CambiosPopup::~CambiosPopup() { for (int i = 0; i < 3; i++) delete btn[i]; }

void CambiosPopup::Abrir(int a) {
    accion = a;
    W3dCambiosListar(cambios);
    foco = 0; scroll = 0; arrastrando = false;
    // los textos, en el idioma de ahora y segun lo que se queria hacer
    btn[0]->text = T(a == W3D_CAMBIOS_SALIR ? "Save and quit" : a == W3D_CAMBIOS_ABRIR ? "Save and open" : "Save and continue");
    btn[1]->text = T(a == W3D_CAMBIOS_SALIR ? "Quit without saving" : a == W3D_CAMBIOS_ABRIR ? "Open without saving" : "Continue without saving");
    btn[2]->text = T("Cancel");
    PopUpActive = this;
    g_redraw = true;
}
void CambiosPopupAbrir(int a) {
    if (!cambiosPopup) cambiosPopup = new CambiosPopup();
    if (PopUpActive && PopUpActive != cambiosPopup) PopUpActive->Cerrar();
    cambiosPopup->Abrir(a);
}

// centrado, 2/3 del ancho (toda la pantalla si es angosta: el N95), la lista con el alto que entre
void CambiosPopup::Layout() {
    int w = MenuPantallaW * 2 / 3;
    if (w < 240 * GlobalScale || MenuPantallaW < 400) w = MenuPantallaW;
    const int pad = borderGS + GlobalScale * 3;
    const int fila = (int)RenglonHeightGS;
    int maxFilas = (MenuPantallaH * 2 / 3 - 3 * fila - 2 * pad) / fila;
    if (maxFilas < 3) maxFilas = 3;
    filasVisibles = (int)cambios.size() < maxFilas ? (int)cambios.size() : maxFilas;
    if (filasVisibles < 1) filasVisibles = 1;
    const int h = pad * 2 + fila /*titulo*/ + gapGS + filasVisibles * fila + gapGS + (RenglonHeightGS + bordersGS);
    popUpWindow->Resize(w, h);
    x = (MenuPantallaW - w) / 2;
    y = (MenuPantallaH - h) / 2;
    if (y < 0) y = 0;
    listaX = x + pad; listaY = y + pad + fila + gapGS; listaW = w - 2 * pad; listaH = filasVisibles * fila;
    const int maxScroll = (int)cambios.size() - filasVisibles;
    if (scroll > maxScroll) scroll = maxScroll;
    if (scroll < 0) scroll = 0;
}

void CambiosPopup::Render() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);
    const float* fondo = ListaColores[static_cast<int>(ColorID::background)];
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    popUpWindow->RenderBorder(false);
    const int w = popUpWindow->width;
    const int pad = borderGS + GlobalScale * 3;
    const int fila = (int)RenglonHeightGS;
    // el titulo
    SetColorID(ColorID::blanco);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)pad, (GLfloat)pad, 0);
    RenderBitmapText(T("Changes will be lost in:"), textAlign::left, w - 2 * pad);
    w3dEngine::PopMatrix();
    // la LISTA (icono + tipo + nombre), recortada a su area
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(listaX, W3dPantallaAlto - listaY - listaH, listaW, listaH);
    const int ly = listaY - y;
    const int colTipo = IconSizeGS + gapGS * 2;
    const int colNombre = colTipo + 12 * CharacterWidthGS;
    for (int i = 0; i < filasVisibles && scroll + i < (int)cambios.size(); i++) {
        const W3dCambio& c = cambios[(size_t)(scroll + i)];
        const int fy = ly + i * fila;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)fy, 0);
        SetColorID(ColorID::accent);
        if (c.icono >= 0 && c.icono < (int)IconsUV.size()) W3dDrawStrip4(IconMesh, IconsUV[(size_t)c.icono]->uvs);
        w3dEngine::Translatef((GLfloat)colTipo, 0, 0);
        SetColorID(ColorID::grisUI);
        RenderBitmapText(c.tipo, textAlign::left, colNombre - colTipo - gapGS);
        w3dEngine::Translatef((GLfloat)(colNombre - colTipo), 0, 0);
        SetColorID(ColorID::blanco);
        RenderBitmapText(c.nombre + "*", textAlign::left, listaW - colNombre);
        w3dEngine::PopMatrix();
    }
    w3dEngine::Disable(w3dEngine::ScissorTest);
    // la barrita de scroll (si no entra todo)
    if ((int)cambios.size() > filasVisibles) {
        char b[48]; snprintf(b, sizeof(b), "%d-%d / %d", scroll + 1, scroll + filasVisibles, (int)cambios.size());
        SetColorID(ColorID::grisUI, 0.7f);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)pad, 0);
        RenderBitmapText(b, textAlign::right, w - 2 * pad);
        w3dEngine::PopMatrix();
    }
    // los tres botones
    const int contW = w - pad * 2;
    const int tercio = (contW - 2 * gapGS) / 3;
    const int btnY = (listaY - y) + listaH + gapGS;
    for (int i = 0; i < 3; i++) {
        btn[i]->Resize(tercio);
        btn[i]->focoMenu = (i == foco);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(pad + i * (tercio + gapGS)), (GLfloat)btnY, 0);
        btn[i]->Render();
        w3dEngine::PopMatrix();
        btn[i]->sx = x + pad + i * (tercio + gapGS); btn[i]->sy = y + btnY;
    }
    endView();
}

void CambiosPopup::Responder(int r) {
    Cerrar();
    W3dCambiosResponder(r);
    g_redraw = true;
}

bool CambiosPopup::Motion(int mx, int my) {
    Layout();
    for (int i = 0; i < 3; i++) btn[i]->hover = btn[i]->Contains(mx, my);
    if (arrastrando) {
        // arrastrar la lista (mouse o dedo) = scroll
        const int d = (arrastreY0 - my) / (int)RenglonHeightGS;
        scroll = scroll0 + d;
        Layout();
        g_redraw = true;
    }
    return true;
}
bool CambiosPopup::Click(int mx, int my) {
    Layout();
    for (int i = 0; i < 3; i++) if (btn[i]->Contains(mx, my)) { Responder(i); return true; }
    if (mx >= listaX && mx < listaX + listaW && my >= listaY && my < listaY + listaH) {
        arrastrando = true; arrastreY0 = my; scroll0 = scroll;
    }
    return true;   // modal: afuera tampoco se cierra (hay que elegir)
}
void CambiosPopup::Soltar() { arrastrando = false; }
void CambiosPopup::Wheel(int delta) { scroll -= delta; Layout(); g_redraw = true; }
bool CambiosPopup::Tecla(int tecla) {
    switch (tecla) {
        case LayoutKey::Left:  if (foco > 0) foco--; g_redraw = true; return true;
        case LayoutKey::Right: if (foco < 2) foco++; g_redraw = true; return true;
        case LayoutKey::Up:    scroll--; Layout(); g_redraw = true; return true;
        case LayoutKey::Down:  scroll++; Layout(); g_redraw = true; return true;
        case LayoutKey::Enter:
        case LayoutKey::Accept: Responder(foco); return true;
        case LayoutKey::Cancel: Responder(2); return true;
    }
    return true;
}
