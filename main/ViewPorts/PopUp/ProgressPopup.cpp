#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "ProgressPopup.h"
#include "WhiskUI/UI.h"             // metricas (RenglonHeightGS/borderGS/marginGS/gapGS) + RenderBitmapText
#include "WhiskUI/PopupMenu.h"      // MenuPantallaW / MenuPantallaH (tamaño de pantalla)
#include "WhiskUI/bitmapText.h"
#include "WhiskUI/card.h"
#include "WhiskUI/rectangle.h"     // Rec2D: el relleno de la barra (igual que el slider del color picker)
#include "WhiskUI/colores.h"
#include "objects/Textures.h"
#include "render/OpcionesRender.h"  // g_redraw (forzar redibujo del editor al cerrar)

ProgressPopup* progressPopup = NULL;
void (*LayoutSwapBuffers)() = NULL; // lo setea cada plataforma (PC: SDL_GL_SwapWindow)

static bool  g_progresoActivo = false; // entre Iniciar y Fin: ProgresoActualizar dibuja; afuera es no-op
static float g_progresoUltimo = -1.0f; // ultimo frac dibujado (throttle: no redibujar por cada cara)

ProgressPopup::ProgressPopup() : PopUpBase("Progress") {
    frac = 0.0f;
    barBg = new Card(NULL, 10, 10);
    rect  = new Rec2D();
}

ProgressPopup::~ProgressPopup() {
    delete barBg; delete rect;
}

// MISMO layout que el popup de borrar: centrado en PC / barra inferior ancho completo en Symbian.
int ProgressPopup::Padding() const { return borderGS + GlobalScale * 3; } // borde + ~3px de aire (antes marginGS+borderGS = demasiado)
int ProgressPopup::AltoBarra()  const { return Padding() * 2 + RenglonHeightGS + gapGS + (RenglonHeightGS + GlobalScale * 2); }

void ProgressPopup::Layout() {
    int h = AltoBarra();
#ifdef W3D_SYMBIAN
    int w = MenuPantallaW;            // barra al fondo, ancho completo (como los soft keys)
    popUpWindow->Resize(w, h);
    x = 0;
    y = MenuPantallaH - h;
#else
    int w = MenuPantallaW / 2;        // dialogo centrado
    if (w < 200) w = MenuPantallaW;
    popUpWindow->Resize(w, h);
    x = (MenuPantallaW - w) / 2;
    y = (MenuPantallaH - h) / 2;
#endif
}

// SOLO el contenido de la barra (sin initView): track OPACO (tapa la barra anterior, asi el relleno @0.55 no
// se acumula) + relleno verde (accent @ 0.55, look del slider del color) proporcional a frac + borde blanco.
void ProgressPopup::DibujarBarra() {
    const float* fondo  = ListaColores[static_cast<int>(ColorID::background)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* verde  = ListaColores[static_cast<int>(ColorID::accent)];
    int w = popUpWindow->width;
    int pad = Padding();
    int contW = w - pad * 2;
    int barY = pad + RenglonHeightGS + gapGS;
    barBg->Resize(contW, RenglonHeightGS + GlobalScale * 2);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)pad, (GLfloat)barY, 0);
    w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
    barBg->Render(false); // track (tarjeta) OPACO
    float f = frac < 0 ? 0.0f : (frac > 1 ? 1.0f : frac);
    int rellenoW = (int)((contW - GlobalScale * 2) * f);
    if (rellenoW > 0) {
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Color4f(verde[0], verde[1], verde[2], 0.55f);
        rect->SetSize((GLshort)GlobalScale, (GLshort)GlobalScale, (GLshort)rellenoW, (GLshort)RenglonHeightGS);
        rect->RenderObject(false);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }
    w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
    barBg->RenderBorder(false);
    w3dEngine::PopMatrix();
}

// popup COMPLETO: fondo + borde verde + mensaje + barra. Se dibuja UNA vez (al iniciar).
void ProgressPopup::Render() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);
    const float* fondo  = ListaColores[static_cast<int>(ColorID::background)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* verde  = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    popUpWindow->RenderBorder(false);
    int pad = Padding();
    int contW = popUpWindow->width - pad * 2;
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)pad, (GLfloat)pad, 0);
    w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
    RenderBitmapText(mensaje.c_str(), textAlign::center, contW);
    w3dEngine::PopMatrix();
    DibujarBarra();
    endView();
}

// SOLO la barra (sin fondo/borde/mensaje): es lo unico que se redibuja en cada update. NO limpia el GL.
void ProgressPopup::RenderBar() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);
    DibujarBarra();
    endView();
}

// ---------------- API compartida ----------------

// dibuja el popup COMPLETO sobre un fondo limpio (clear) + swap. Solo al INICIAR.
static void ProgresoDibujarTodo() {
    if (!progressPopup) return;
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Viewport(0, 0, MenuPantallaW, MenuPantallaH);
    const float* bg = ListaColores[static_cast<int>(ColorID::background)];
    w3dEngine::ClearColor(bg[0], bg[1], bg[2], 1.0f);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    progressPopup->Render(); // hace su propio initView/endView (scissor a la ventana)
    if (LayoutSwapBuffers) LayoutSwapBuffers();
}

void ProgresoIniciar(const std::string& msg) {
    if (!LayoutSwapBuffers) return; // sin hook de swap (ej: harness sin GUI): no-op
    if (!progressPopup) progressPopup = new ProgressPopup();
    progressPopup->mensaje = msg;
    progressPopup->frac = 0.0f;
    g_progresoActivo = true;
    g_progresoUltimo = 0.0f;
    // dibujamos el popup completo DOS veces (un swap entre medio) para que LOS DOS buffers del doble-buffer
    // tengan el popup. Asi despues alcanza con redibujar SOLO la barra (sin clear): el resto ya esta en ambos.
    ProgresoDibujarTodo();
    ProgresoDibujarTodo();
}

void ProgresoActualizar(float frac) {
    if (!g_progresoActivo || !progressPopup) return;          // no-op fuera de una operacion con barra
    if (frac < 1.0f && frac < g_progresoUltimo + 0.015f) return; // throttle: ~1 frame cada 1.5% (no por cada cara)
    g_progresoUltimo = frac;
    progressPopup->frac = frac;
    // SIMPLE (pedido Dante): NO se limpia el GL ni se redibuja fondo/borde/mensaje. SOLO la barra. El track
    // opaco tapa la barra anterior. El popup ya esta dibujado en ambos buffers desde ProgresoIniciar.
    progressPopup->RenderBar();
    if (LayoutSwapBuffers) LayoutSwapBuffers();
}

void ProgresoFin() {
    g_progresoActivo = false;
    g_progresoUltimo = -1.0f;
    g_redraw = true; // el editor se redibuja en el proximo frame (tapa la barra)
}
