#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "ProgressPopup.h"
#include "WhiskUI/core/UI.h"             // metricas (RenglonHeightGS/borderGS/marginGS/gapGS) + RenderBitmapText
#include "WhiskUI/widgets/PopupMenu.h"      // MenuPantallaW / MenuPantallaH (tamaño de pantalla)
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/draw/rectangle.h"     // Rec2D: el relleno de la barra (igual que el slider del color picker)
#include "WhiskUI/theme/colores.h"
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
    // mensaje + porcentaje EN VIVO (ej: "Importing model...  45%")
    int pct = (int)((frac < 0 ? 0.0f : (frac > 1 ? 1.0f : frac)) * 100.0f + 0.5f);
    char suf[8]; snprintf(suf, sizeof(suf), "  %d%%", pct);
    std::string linea = mensaje + suf;
    RenderBitmapText(linea.c_str(), textAlign::center, contW);
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
    // limpiar a NEGRO (pedido Dante): asi el popup resalta y NO se ve nada viejo (file browser/escena)
    // alrededor. Se hace en CADA update -> el frame mostrado siempre es negro+popup, sin importar cuantos
    // buffers tenga el swap-chain (2 o 3): eso elimina el parpadeo con el file browser.
    w3dEngine::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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
    // clear NEGRO + popup COMPLETO (fondo + mensaje + % + barra) + swap. Redibujar TODO cada update (no solo
    // la barra) evita el parpadeo del file browser: no dependemos de que los N buffers esten pre-dibujados.
    ProgresoDibujarTodo();
}

// como ProgresoActualizar, pero redibuja el popup ENTERO (clear + fondo + borde + mensaje + barra) + swap.
// El render por TILES pisa todo el framebuffer en cada tile -> la barra sola no alcanza (el fondo del popup
// ya no esta). Sin throttle: cada tile es un paso grueso (en el N95, lento, se ve el avance bien).
void ProgresoActualizarFull(float frac) {
    if (!g_progresoActivo || !progressPopup) return;
    g_progresoUltimo = frac;
    progressPopup->frac = frac;
    ProgresoDibujarTodo();
}

void ProgresoFin() {
    g_progresoActivo = false;
    g_progresoUltimo = -1.0f;
    g_redraw = true; // el editor se redibuja en el proximo frame (tapa la barra)
}
