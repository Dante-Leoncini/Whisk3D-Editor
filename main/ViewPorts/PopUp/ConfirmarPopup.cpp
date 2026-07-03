#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "ConfirmarPopup.h"
#include "ViewPorts/LayoutInput.h"  // LayoutKey
#include "WhiskUI/UI.h"             // metricas (RenglonHeightGS/borderGS/marginGS/gapGS/...) + RenderBitmapText
#include "WhiskUI/bitmapText.h"
#include "WhiskUI/Button.h"
#include "objects/Textures.h"
#include "objects/ObjectMode.h"     // Eliminar
#include "variables.h"              // InteractionMode, ObjActivo, ObjSelects
#include <cstdio>

ConfirmarPopup* confirmarPopup = NULL;

static const float ROJO_BORRAR[4] = { 0.92f, 0.26f, 0.26f, 1.0f }; // rojo del boton de borrar (fijo: la paleta no tiene un rojo claro)

ConfirmarPopup::ConfirmarPopup() : PopUpBase("Confirmar") {
    onSi = NULL;
    foco = 1; // por defecto resaltado en "Si" (la accion principal)
    btnSi = new Button("Yes");
    btnNo = new Button("No");
    btnSi->adaptar = false; btnSi->centrado = true;
    btnNo->adaptar = false; btnNo->centrado = true;
    btnSi->colorTexto = ROJO_BORRAR; // borrar = ROJO
}

ConfirmarPopup::~ConfirmarPopup() {
    delete btnSi; delete btnNo;
}

void ConfirmarPopup::Abrir(const std::string& msg, void (*cb)()) {
    mensaje = msg;
    onSi = cb;
    foco = 1;
    PopUpActive = this; // el layout (ancho/posicion) se calcula en Render con las metricas ACTUALES
}

// BARRA al fondo, ANCHO COMPLETO de la pantalla (simula los soft keys de Symbian). Se recalcula cada frame
// para adaptarse al tamaño/orientacion actual (y porque al abrir las metricas pueden no estar listas).
// padding UNIFORME en los 4 lados (el contenido = fila del mensaje + gap + fila de botones)
int ConfirmarPopup::Padding() const { return borderGS + GlobalScale * 3; } // borde + ~3px de aire (antes marginGS+borderGS = demasiado)
int ConfirmarPopup::AltoBarra()  const { return Padding() * 2 + RenglonHeightGS + gapGS + (RenglonHeightGS + bordersGS); }

void ConfirmarPopup::Layout() {
    int h = AltoBarra();
#ifdef W3D_SYMBIAN
    // SYMBIAN (keypad / soft keys): barra al FONDO, ancho COMPLETO (ahi estan los soft keys)
    int w = MenuPantallaW;
    popUpWindow->Resize(w, h);
    x = 0;
    y = MenuPantallaH - h;
#else
    // PC / pantallas TACTILES: dialogo CENTRADO en la ventana
    int w = MenuPantallaW / 2;
    if (w < 200) w = MenuPantallaW; // pantallas muy chicas: ancho completo
    popUpWindow->Resize(w, h);
    x = (MenuPantallaW - w) / 2;
    y = (MenuPantallaH - h) / 2;
#endif
}

void ConfirmarPopup::Render() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);

    const float* fondo = ListaColores[static_cast<int>(ColorID::background)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];

    // fondo + BORDE VERDE
    w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    popUpWindow->RenderBorder(false);

    int w = popUpWindow->width;
    int pad = Padding(); // mismo padding arriba/abajo/lados

    // mensaje, centrado, en su fila (arriba)
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)pad, (GLfloat)pad, 0);
    w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
    RenderBitmapText(mensaje.c_str(), textAlign::center, w - pad * 2);
    w3dEngine::PopMatrix();

    // botones: No a la IZQUIERDA, Si a la DERECHA (ancho 50% cada uno)
    int izq = pad;
    int contW = w - pad * 2;
    int mitad = (contW - gapGS) / 2;
    int btnY = pad + RenglonHeightGS + gapGS;

    btnNo->Resize(mitad);
    btnSi->Resize(mitad); // Si queda ROJO siempre (colorTexto), sin resaltado verde de foco (es el de borrar)

    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)izq, (GLfloat)btnY, 0);
    btnNo->Render();
    w3dEngine::PopMatrix();
    btnNo->sx = x + izq; btnNo->sy = y + btnY;

    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(izq + mitad + gapGS), (GLfloat)btnY, 0);
    btnSi->Render();
    w3dEngine::PopMatrix();
    btnSi->sx = x + izq + mitad + gapGS; btnSi->sy = y + btnY;

    endView();
}

bool ConfirmarPopup::Motion(int mx, int my) {
    Layout();
    btnNo->hover = btnNo->Contains(mx, my); // ilumina el boton al pasar el mouse (igual que cualquier boton)
    btnSi->hover = btnSi->Contains(mx, my);
    return true;
}

bool ConfirmarPopup::Click(int mx, int my) {
    Layout(); // asegura posicion/tamaño actuales para el hit-test
    if (btnNo->Contains(mx, my)) { Cerrar(); return true; }
    if (btnSi->Contains(mx, my)) { void (*cb)() = onSi; Cerrar(); if (cb) cb(); return true; }
    // click adentro de la barra (sin botón): lo consume (modal); afuera -> el caller cierra
    return Contains(mx, my);
}

bool ConfirmarPopup::Tecla(int tecla) {
    switch (tecla) {
        case LayoutKey::Right:   // flecha derecha / soft key derecho = Si
        case LayoutKey::Accept:  // OK / Enter = Si (borrar)
            { void (*cb)() = onSi; Cerrar(); if (cb) cb(); } return true;
        case LayoutKey::Left:    // flecha izquierda / soft key izquierdo = No
        case LayoutKey::Cancel:  // Esc / C = No (cancela)
            Cerrar(); return true;
    }
    return true; // modal: no le roban teclas
}

// ----------------------------------------------------------------

static void ConfirmarBorradoSi() { Eliminar(false); } // el borrado real (con su undo) al confirmar

void AbrirConfirmarBorrado() {
    if (InteractionMode != ObjectMode) return;          // Edit Mode tiene su propio menu Delete
    if (!HayObjetosSeleccionados(false)) return;

    int n = 0; Object* uno = NULL;
    for (size_t i = 0; i < ObjSelects.size(); i++)
        if (ObjSelects[i] && ObjSelects[i]->select) { n++; uno = ObjSelects[i]; }
    if (n == 0) return;

    char buf[128];
    if (n == 1 && uno) sprintf(buf, "Delete \"%s\"?", uno->name.c_str());
    else               sprintf(buf, "Delete %d objects?", n);

    if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
    confirmarPopup->Abrir(buf, ConfirmarBorradoSi);
}
