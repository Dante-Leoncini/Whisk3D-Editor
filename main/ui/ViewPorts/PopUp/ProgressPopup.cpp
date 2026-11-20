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
#include "W3dDock.h"                // EXTRA: la misma barra en el icono del dock (Linux; no-op en el resto)
#include "W3dLang.h"                // T(): los renglones y el boton en el idioma del sistema
#include <cstdio>

ProgressPopup* progressPopup = NULL;
void (*LayoutSwapBuffers)() = NULL; // lo setea cada plataforma (PC: SDL_GL_SwapWindow)

static bool  g_progresoActivo = false; // entre Iniciar y Fin: ProgresoActualizar dibuja; afuera es no-op
static float g_progresoUltimo = -1.0f; // ultimo frac dibujado (throttle: no redibujar por cada cara)
static float g_rangoA = 0.0f, g_rangoB = 1.0f;   // (ProgresoRango)
static bool  g_cancelado = false;
// los tiempos de una animacion (ProgresoCuadro)
static int   g_cuadro = 0, g_cuadros = 0;
static unsigned g_tCuadro = 0, g_tUltimo = 0, g_tSuma = 0, g_hechos = 0;
static unsigned g_tSondeo = 0;
void (*W3dProgresoSondear)() = NULL;
extern unsigned int w3dGetTicks();

ProgressPopup::ProgressPopup() : PopUpBase("Progress") {
    frac = 0.0f;
    conCancelar = false; cancX = cancY = cancW = cancH = 0;
    barBg = new Card(NULL, 10, 10);
    rect  = new Rec2D();
}

ProgressPopup::~ProgressPopup() {
    delete barBg; delete rect;
}

// MISMO layout que el popup de borrar: centrado en PC / barra inferior ancho completo en Symbian.
int ProgressPopup::Padding() const { return borderGS + GlobalScale * 3; } // borde + ~3px de aire (antes marginGS+borderGS = demasiado)
int ProgressPopup::AltoBarra()  const {
    int h = Padding() * 2 + RenglonHeightGS + gapGS + (RenglonHeightGS + GlobalScale * 2);
    h += (int)lineas.size() * (RenglonHeightGS + gapGS);                        // (los renglones de la animacion)
    if (conCancelar) h += gapGS * 2 + RenglonHeightGS + GlobalScale * 2;        // (el boton)
    return h;
}

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
    Dibujar();
}

// el CONTENIDO completo del popup (fondo + borde + mensaje + % + barra) en la posicion
// x/y ACTUAL. Lo comparten Render() (modal, centrado por Layout) y el overlay del build
// (no-modal, anclado abajo). No hace clear ni swap: dibuja y nada mas.
void ProgressPopup::Dibujar() {
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
    // los RENGLONES (cuadro N de M, tiempos) y el boton CANCELAR
    {
        int yy = pad + RenglonHeightGS + gapGS + RenglonHeightGS + GlobalScale * 2 + gapGS;
        for (size_t i = 0; i < lineas.size(); i++) {
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)pad, (GLfloat)yy, 0);
            w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            RenderBitmapText(lineas[i].c_str(), textAlign::center, contW);
            w3dEngine::PopMatrix();
            yy += RenglonHeightGS + gapGS;
        }
        if (conCancelar) {
            yy += gapGS;
            const int bw = contW / 2 > 60 ? contW / 2 : contW, bh = RenglonHeightGS + GlobalScale * 2;
            const int bx = pad + (contW - bw) / 2;
            barBg->Resize(bw, bh);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)bx, (GLfloat)yy, 0);
            w3dEngine::Color4f(fondo[0] * 0.6f, fondo[1] * 0.6f, fondo[2] * 0.6f, 1.0f);
            barBg->Render(false);
            w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            barBg->RenderBorder(false);
            w3dEngine::Translatef(0, (GLfloat)GlobalScale, 0);
#ifdef W3D_SYMBIAN
            RenderBitmapText(T("Cancel (right key)"), textAlign::center, bw);
#else
            RenderBitmapText(T("Cancel (Esc)"), textAlign::center, bw);
#endif
            w3dEngine::PopMatrix();
            cancX = x + bx; cancY = y + yy; cancW = bw; cancH = bh;
        } else cancW = cancH = 0;
    }
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
// "1 h 05 min" / "3 min 20 s" / "12.3 s"
static std::string Duracion(unsigned ms) {
    char b[48];
    const unsigned s = ms / 1000;
    if (s >= 3600) snprintf(b, sizeof b, "%u h %02u min", s / 3600, (s / 60) % 60);
    else if (s >= 60) snprintf(b, sizeof b, "%u min %02u s", s / 60, s % 60);
    else snprintf(b, sizeof b, "%.1f s", ms / 1000.0);
    return b;
}
static void ArmarLineas() {
    if (!progressPopup) return;
    std::vector<std::string>& L = progressPopup->lineas;
    L.clear();
    if (g_cuadros <= 0) return;
    char b[160];
    snprintf(b, sizeof b, T("Frame %d of %d"), g_cuadro, g_cuadros);
    L.push_back(b);
    if (g_hechos > 0) {
        const unsigned prom = g_tSuma / g_hechos;
        std::string s = std::string(T("Last: ")) + Duracion(g_tUltimo) + "   " + T("Average: ") + Duracion(prom);
        L.push_back(s);
        const unsigned enCurso = w3dGetTicks() - g_tCuadro;
        const long quedan = (long)prom * (long)(g_cuadros - g_cuadro + 1) - (long)enCurso;
        L.push_back(std::string(T("Remaining: ~")) + Duracion(quedan > 0 ? (unsigned)quedan : 0u));
    } else {
        L.push_back(std::string(T("This frame: ")) + Duracion(w3dGetTicks() - g_tCuadro));
    }
}
static void ProgresoDibujarTodo() {
    if (!progressPopup) return;
    // la ENTRADA pendiente (el boton Cancelar, Escape, la tecla del telefono), a lo sumo cada 1/5 de segundo
    if (W3dProgresoSondear) {
        const unsigned t = w3dGetTicks();
        if (t - g_tSondeo >= 200u) { g_tSondeo = t; W3dProgresoSondear(); }
    }
    ArmarLineas();
    // (entre franja y franja de un render: la escena dejo luces, niebla, profundidad, texturas... -en el N95 por GL
    // crudo, que el cache de estado no ve-. El popup salia BLANCO o parpadeaba: el estado de base, de verdad)
    w3dEngine::Invalidate();
    w3dEngine::EstadoBase();
    w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest); w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::DisableArray(w3dEngine::NormalArray); w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DepthMask(true);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Viewport(0, 0, MenuPantallaW, MenuPantallaH);
    // limpiar a NEGRO: asi el popup resalta y NO se ve nada viejo (file browser/escena)
    // alrededor. Se hace en CADA update -> el frame mostrado siempre es negro+popup, sin importar cuantos
    // buffers tenga el swap-chain (2 o 3): eso elimina el parpadeo con el file browser.
    w3dEngine::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    progressPopup->Render(); // hace su propio initView/endView (scissor a la ventana)
    if (LayoutSwapBuffers) LayoutSwapBuffers();
}

void ProgresoIniciar(const std::string& msg) {
    if (!LayoutSwapBuffers) return; // sin hook de swap (ej: harness sin GUI): no-op
    W3dDockProgreso(0.0f);          // EXTRA: barra en el icono del dock (mismo avance, no una cuenta aparte)
    if (!progressPopup) progressPopup = new ProgressPopup();
    progressPopup->mensaje = msg;
    progressPopup->frac = 0.0f;
    g_progresoActivo = true;
    g_progresoUltimo = 0.0f;
    g_rangoA = 0.0f; g_rangoB = 1.0f;
    g_cancelado = false;
    g_cuadro = g_cuadros = 0; g_tUltimo = g_tSuma = g_hechos = 0; g_tCuadro = w3dGetTicks();
    progressPopup->lineas.clear();
    progressPopup->conCancelar = false;
    // dibujamos el popup completo DOS veces (un swap entre medio) para que LOS DOS buffers del doble-buffer
    // tengan el popup. Asi despues alcanza con redibujar SOLO la barra (sin clear): el resto ya esta en ambos.
    ProgresoDibujarTodo();
    ProgresoDibujarTodo();
}

void ProgresoActualizar(float frac) {
    if (!g_progresoActivo || !progressPopup) return;          // no-op fuera de una operacion con barra
    frac = g_rangoA + (g_rangoB - g_rangoA) * (frac < 0 ? 0.0f : (frac > 1 ? 1.0f : frac));   // (ProgresoRango)
    if (frac < 1.0f && frac < g_progresoUltimo + 0.015f) return; // throttle: ~1 frame cada 1.5% (no por cada cara)
    g_progresoUltimo = frac;
    progressPopup->frac = frac;
    W3dDockProgreso(frac);
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
    W3dDockProgreso(frac);
    ProgresoDibujarTodo();
}

void ProgresoRango(float a, float b) { g_rangoA = a; g_rangoB = b; if (g_progresoUltimo > a) g_progresoUltimo = a; }
void ProgresoCuadro(int actual, int total) {
    const unsigned t = w3dGetTicks();
    if (g_cuadros > 0 && actual > g_cuadro && g_cuadro > 0) {   // (termino el anterior)
        g_tUltimo = t - g_tCuadro; g_tSuma += g_tUltimo; g_hechos++;
    }
    g_cuadro = actual; g_cuadros = total; g_tCuadro = t;
    if (g_progresoActivo && progressPopup) ProgresoDibujarTodo();
}
void ProgresoConCancelar(bool si) { if (progressPopup) progressPopup->conCancelar = si; }
void ProgresoCancelar() { if (g_progresoActivo) g_cancelado = true; }
bool ProgresoCancelado() {
    // (lo largo pregunta seguido: que la entrada se atienda tambien entre dibujos de la barra)
    if (g_progresoActivo && W3dProgresoSondear) {
        const unsigned t = w3dGetTicks();
        if (t - g_tSondeo >= 200u) { g_tSondeo = t; W3dProgresoSondear(); }
    }
    return g_cancelado;
}
bool ProgresoBotonCancelarEn(int px, int py) {
    if (!progressPopup || !progressPopup->conCancelar || progressPopup->cancW <= 0) return false;
    const ProgressPopup* p = progressPopup;
    return px >= p->cancX && px < p->cancX + p->cancW && py >= p->cancY && py < p->cancY + p->cancH;
}

void ProgresoFin() {
    g_progresoActivo = false;
    g_rangoA = 0.0f; g_rangoB = 1.0f;
    g_cuadros = 0;
    g_progresoUltimo = -1.0f;
    W3dDockProgresoFin(); // apagar la barra del dock SIEMPRE (termine bien, mal o cancelada)
    g_redraw = true; // el editor se redibuja en el proximo frame (tapa la barra)
}

bool ProgresoActivo() { return g_progresoActivo; }

// ---------------- la barra de "Compilar juego" ----------------
// El export de modelos usa esta barra en modo MODAL-bloqueante (clear negro + swap propio) porque esa operacion congela
// el hilo de UI. El build de juegos corre en un hilo worker y el editor sigue dibujando: aca se reusa el MISMO widget
// (Card + borde verde + mensaje con % + barra accent) dentro del frame normal del editor (sin clear ni swap), anclado
// ABAJO-centro, sobre el editor OSCURECIDO: mientras compila el editor no se usa ni se guarda (los eventos los descarta
// ProcesarEvento, en main.cpp) y el velo lo deja a la vista.
void ProgresoOverlayRender(const std::string& etapa, float frac) {
    if (!progressPopup) progressPopup = new ProgressPopup();
    {   // el velo: toda la ventana, atenuada
        static Rec2D* velo = NULL;
        if (!velo) velo = new Rec2D();
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
        velo->SetSize(0, 0, (GLshort)MenuPantallaW, (GLshort)MenuPantallaH);
        velo->RenderObject(false);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }
    progressPopup->mensaje = etapa;
    progressPopup->frac = frac;
    progressPopup->Layout();          // mismo tamano/posicion X que el modal
#ifndef W3D_SYMBIAN
    // en PC el modal va centrado; el overlay va ABAJO (en Symbian Layout ya lo deja abajo)
    progressPopup->y = MenuPantallaH - progressPopup->popUpWindow->height - marginGS;
#endif
    progressPopup->Dibujar();
    W3dDockProgreso(frac); // EXTRA: el mismo % del build, en el icono del dock (se ve minimizado)
}
