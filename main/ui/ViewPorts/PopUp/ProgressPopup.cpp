#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "ProgressPopup.h"
#include "WhiskUI/core/UI.h"             // metricas (RowHeightGS/borderGS/marginGS/gapGS) + RenderBitmapText
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH (tamaño de pantalla)
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/draw/rectangle.h"     // Rec2D: el relleno de la barra (igual que el slider del color picker)
#include "WhiskUI/theme/colors.h"
#include "objects/Textures.h"
#include "render/RenderOptions.h"  // g_redraw (forzar redibujo del editor al cerrar)
#include "W3dDock.h"               // EXTRA: la misma barra en el icono del dock (Linux; no-op en el resto)
#include "W3dLang.h"               // T(): los renglones y el boton en el idioma del sistema
#include <cstdio>

ProgressPopup* progressPopup = NULL;
void (*LayoutSwapBuffers)() = NULL; // lo setea cada plataforma (PC: SDL_GL_SwapWindow)

static bool  g_progressActive = false; // entre Iniciar y Fin: ProgressUpdate dibuja; afuera es no-op
static float g_lastProgress   = -1.0f; // ultimo frac dibujado (throttle: no redibujar por cada cara)
static float g_rangeA = 0.0f, g_rangeB = 1.0f;   // (ProgressRange)
static bool  g_canceled = false;
// los tiempos de una animacion (ProgressFrame)
static int   g_frame = 0, g_frames = 0;
static unsigned g_tFrame = 0, g_tLast = 0, g_tSum = 0, g_done = 0;
static unsigned g_tPoll  = 0;
void (*W3dProgressPoll)() = NULL;
extern unsigned int w3dGetTicks();

ProgressPopup::ProgressPopup() : PopUpBase("Progress") {
    frac = 0.0f;
    withCancel = false; cancelX = cancelY = cancelW = cancelH = 0;
    barBg = new Card(NULL, 10, 10);
    rect  = new Rec2D();
}

ProgressPopup::~ProgressPopup() {
    delete barBg; delete rect;
}

// MISMO layout que el popup de borrar: centrado en PC / barra inferior ancho completo en Symbian.
int ProgressPopup::Padding() const { return borderGS + GlobalScale * 3; } // borde + ~3px de aire (antes marginGS+borderGS = demasiado)
int ProgressPopup::BarHeight()  const {
    int h = Padding() * 2 + RowHeightGS + gapGS + (RowHeightGS + GlobalScale * 2);
    h += (int)lines.size() * (RowHeightGS + gapGS);                        // (los renglones de la animacion)
    if (withCancel) h += gapGS * 2 + RowHeightGS + GlobalScale * 2;        // (el boton)
    return h;
}

void ProgressPopup::Layout() {
    int h = BarHeight();
#ifdef W3D_SYMBIAN
    int w = MenuScreenW;            // barra al fondo, ancho completo (como los soft keys)
    popUpWindow->Resize(w, h);
    x = 0;
    y = MenuScreenH - h;
#else
    int w = MenuScreenW / 2;        // dialogo centrado
    if (w < 200) w = MenuScreenW;
    popUpWindow->Resize(w, h);
    x = (MenuScreenW - w) / 2;
    y = (MenuScreenH - h) / 2;
#endif
}

// SOLO el contenido de la barra (sin initView): track OPACO (tapa la barra anterior, asi el relleno @0.55 no
// se acumula) + relleno verde (accent @ 0.55, look del slider del color) proporcional a frac + borde blanco.
void ProgressPopup::DrawBar() {
    const float* background = ColorList[static_cast<int>(ColorID::background)];
    const float* white      = ColorList[static_cast<int>(ColorID::white)];
    const float* green      = ColorList[static_cast<int>(ColorID::accent)];
    int w = popUpWindow->width;
    int pad = Padding();
    int contW = w - pad * 2;
    int barY = pad + RowHeightGS + gapGS;
    barBg->Resize(contW, RowHeightGS + GlobalScale * 2);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)pad, (GLfloat)barY, 0);
    w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
    barBg->Render(false); // track (tarjeta) OPACO
    float f = frac < 0 ? 0.0f : (frac > 1 ? 1.0f : frac);
    int fillW = (int)((contW - GlobalScale * 2) * f);
    if (fillW > 0) {
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Color4f(green[0], green[1], green[2], 0.55f);
        rect->SetSize((GLshort)GlobalScale, (GLshort)GlobalScale, (GLshort)fillW, (GLshort)RowHeightGS);
        rect->RenderObject(false);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }
    w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
    barBg->RenderBorder(false);
    w3dEngine::PopMatrix();
}

// popup COMPLETO: fondo + borde verde + mensaje + barra. Se dibuja UNA vez (al iniciar).
void ProgressPopup::Render() {
    Layout();
    Draw();
}

// el CONTENIDO completo del popup (fondo + borde + mensaje + % + barra) en la posicion
// x/y ACTUAL. Lo comparten Render() (modal, centrado por Layout) y el overlay del build
// (no-modal, anclado abajo). No hace clear ni swap: dibuja y nada mas.
void ProgressPopup::Draw() {
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);
    const float* background = ColorList[static_cast<int>(ColorID::background)];
    const float* white      = ColorList[static_cast<int>(ColorID::white)];
    const float* green      = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    popUpWindow->RenderBorder(false);
    int pad = Padding();
    int contW = popUpWindow->width - pad * 2;
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)pad, (GLfloat)pad, 0);
    w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
    // mensaje + porcentaje EN VIVO (ej: "Importing model...  45%")
    int pct = (int)((frac < 0 ? 0.0f : (frac > 1 ? 1.0f : frac)) * 100.0f + 0.5f);
    char suf[8]; snprintf(suf, sizeof(suf), "  %d%%", pct);
    std::string line = message + suf;
    RenderBitmapText(line.c_str(), textAlign::center, contW);
    w3dEngine::PopMatrix();
    DrawBar();
    // los RENGLONES (cuadro N de M, tiempos) y el boton CANCELAR
    {
        int yy = pad + RowHeightGS + gapGS + RowHeightGS + GlobalScale * 2 + gapGS;
        for (size_t i = 0; i < lines.size(); i++) {
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)pad, (GLfloat)yy, 0);
            w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            RenderBitmapText(lines[i].c_str(), textAlign::center, contW);
            w3dEngine::PopMatrix();
            yy += RowHeightGS + gapGS;
        }
        if (withCancel) {
            yy += gapGS;
            const int bw = contW / 2 > 60 ? contW / 2 : contW, bh = RowHeightGS + GlobalScale * 2;
            const int bx = pad + (contW - bw) / 2;
            barBg->Resize(bw, bh);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)bx, (GLfloat)yy, 0);
            w3dEngine::Color4f(background[0] * 0.6f, background[1] * 0.6f, background[2] * 0.6f, 1.0f);
            barBg->Render(false);
            w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            barBg->RenderBorder(false);
            w3dEngine::Translatef(0, (GLfloat)GlobalScale, 0);
#ifdef W3D_SYMBIAN
            RenderBitmapText(T("Cancel (right key)"), textAlign::center, bw);
#else
            RenderBitmapText(T("Cancel (Esc)"), textAlign::center, bw);
#endif
            w3dEngine::PopMatrix();
            cancelX = x + bx; cancelY = y + yy; cancelW = bw; cancelH = bh;
        } else cancelW = cancelH = 0;
    }
    endView();
}

// SOLO la barra (sin fondo/borde/mensaje): es lo unico que se redibuja en cada update. NO limpia el GL.
void ProgressPopup::RenderBar() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);
    DrawBar();
    endView();
}

// ---------------- API compartida ----------------

// dibuja el popup COMPLETO sobre un fondo limpio (clear) + swap. Solo al INICIAR.
// "1 h 05 min" / "3 min 20 s" / "12.3 s"
static std::string Duration(unsigned ms) {
    char b[48];
    const unsigned s = ms / 1000;
    if (s >= 3600) snprintf(b, sizeof b, "%u h %02u min", s / 3600, (s / 60) % 60);
    else if (s >= 60) snprintf(b, sizeof b, "%u min %02u s", s / 60, s % 60);
    else snprintf(b, sizeof b, "%.1f s", ms / 1000.0);
    return b;
}
static void BuildLines() {
    if (!progressPopup) return;
    std::vector<std::string>& L = progressPopup->lines;
    L.clear();
    if (g_frames <= 0) return;
    char b[160];
    snprintf(b, sizeof b, T("Frame %d of %d"), g_frame, g_frames);
    L.push_back(b);
    if (g_done > 0) {
        const unsigned prom = g_tSum / g_done;
        std::string s = std::string(T("Last: ")) + Duration(g_tLast) + "   " + T("Average: ") + Duration(prom);
        L.push_back(s);
        const unsigned inProgress = w3dGetTicks() - g_tFrame;
        const long remain = (long)prom * (long)(g_frames - g_frame + 1) - (long)inProgress;
        L.push_back(std::string(T("Remaining: ~")) + Duration(remain > 0 ? (unsigned)remain : 0u));
    } else {
        L.push_back(std::string(T("This frame: ")) + Duration(w3dGetTicks() - g_tFrame));
    }
}
static void ProgressDrawAll() {
    if (!progressPopup) return;
    // la ENTRADA pendiente (el boton Cancelar, Escape, la tecla del telefono), a lo sumo cada 1/5 de segundo
    if (W3dProgressPoll) {
        const unsigned t = w3dGetTicks();
        if (t - g_tPoll >= 200u) { g_tPoll = t; W3dProgressPoll(); }
    }
    BuildLines();
    // (entre franja y franja de un render: la escena dejo luces, niebla, profundidad, texturas... -en el N95 por GL
    // crudo, que el cache de estado no ve-. El popup salia BLANCO o parpadeaba: el estado de base, de verdad)
    w3dEngine::Invalidate();
    w3dEngine::BaseState();
    w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest); w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::DisableArray(w3dEngine::NormalArray); w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DepthMask(true);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Viewport(0, 0, MenuScreenW, MenuScreenH);
    // limpiar a NEGRO: asi el popup resalta y NO se ve nada viejo (file browser/escena)
    // alrededor. Se hace en CADA update -> el frame mostrado siempre es negro+popup, sin importar cuantos
    // buffers tenga el swap-chain (2 o 3): eso elimina el parpadeo con el file browser.
    w3dEngine::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    progressPopup->Render(); // hace su propio initView/endView (scissor a la ventana)
    if (LayoutSwapBuffers) LayoutSwapBuffers();
}

void ProgressStart(const std::string& msg) {
    if (!LayoutSwapBuffers) return; // sin hook de swap (ej: harness sin GUI): no-op
    W3dDockProgress(0.0f);          // EXTRA: barra en el icono del dock (mismo avance, no una cuenta aparte)
    if (!progressPopup) progressPopup = new ProgressPopup();
    progressPopup->message = msg;
    progressPopup->frac = 0.0f;
    g_progressActive = true;
    g_lastProgress   = 0.0f;
    g_rangeA = 0.0f; g_rangeB = 1.0f;
    g_canceled = false;
    g_frame = g_frames = 0; g_tLast = g_tSum = g_done = 0; g_tFrame = w3dGetTicks();
    progressPopup->lines.clear();
    progressPopup->withCancel = false;
    // dibujamos el popup completo DOS veces (un swap entre medio) para que LOS DOS buffers del doble-buffer
    // tengan el popup. Asi despues alcanza con redibujar SOLO la barra (sin clear): el resto ya esta en ambos.
    ProgressDrawAll();
    ProgressDrawAll();
}

void ProgressUpdate(float frac) {
    if (!g_progressActive || !progressPopup) return;          // no-op fuera de una operacion con barra
    frac = g_rangeA + (g_rangeB - g_rangeA) * (frac < 0 ? 0.0f : (frac > 1 ? 1.0f : frac));   // (ProgressRange)
    if (frac < 1.0f && frac < g_lastProgress + 0.015f) return; // throttle: ~1 frame cada 1.5% (no por cada cara)
    g_lastProgress = frac;
    progressPopup->frac = frac;
    W3dDockProgress(frac);
    // clear NEGRO + popup COMPLETO (fondo + mensaje + % + barra) + swap. Redibujar TODO cada update (no solo
    // la barra) evita el parpadeo del file browser: no dependemos de que los N buffers esten pre-dibujados.
    ProgressDrawAll();
}

// como ProgressUpdate, pero redibuja el popup ENTERO (clear + fondo + borde + mensaje + barra) + swap.
// El render por TILES pisa todo el framebuffer en cada tile -> la barra sola no alcanza (el fondo del popup
// ya no esta). Sin throttle: cada tile es un paso grueso (en el N95, lento, se ve el avance bien).
void ProgressUpdateFull(float frac) {
    if (!g_progressActive || !progressPopup) return;
    g_lastProgress = frac;
    progressPopup->frac = frac;
    W3dDockProgress(frac);
    ProgressDrawAll();
}

void ProgressRange(float a, float b) { g_rangeA = a; g_rangeB = b; if (g_lastProgress > a) g_lastProgress = a; }
void ProgressFrame(int current, int total) {
    const unsigned t = w3dGetTicks();
    if (g_frames > 0 && current > g_frame && g_frame > 0) {   // (termino el anterior)
        g_tLast = t - g_tFrame; g_tSum += g_tLast; g_done++;
    }
    g_frame = current; g_frames = total; g_tFrame = t;
    if (g_progressActive && progressPopup) ProgressDrawAll();
}
void ProgressWithCancel(bool yes) { if (progressPopup) progressPopup->withCancel = yes; }
void ProgressCancel() { if (g_progressActive) g_canceled = true; }
bool ProgressCanceled() {
    // (lo largo pregunta seguido: que la entrada se atienda tambien entre dibujos de la barra)
    if (g_progressActive && W3dProgressPoll) {
        const unsigned t = w3dGetTicks();
        if (t - g_tPoll >= 200u) { g_tPoll = t; W3dProgressPoll(); }
    }
    return g_canceled;
}
bool ProgressButtonCancelIn(int px, int py) {
    if (!progressPopup || !progressPopup->withCancel || progressPopup->cancelW <= 0) return false;
    const ProgressPopup* p = progressPopup;
    return px >= p->cancelX && px < p->cancelX + p->cancelW && py >= p->cancelY && py < p->cancelY + p->cancelH;
}

void ProgressEnd() {
    g_progressActive = false;
    g_rangeA = 0.0f; g_rangeB = 1.0f;
    g_frames = 0;
    g_lastProgress = -1.0f;
    W3dDockProgressEnd(); // apagar la barra del dock SIEMPRE (termine bien, mal o cancelada)
    g_redraw = true; // el editor se redibuja en el proximo frame (tapa la barra)
}

bool ProgressActive() { return g_progressActive; }

// ---------------- la barra de "Compilar juego" ----------------
// El export de modelos usa esta barra en modo MODAL-bloqueante (clear negro + swap propio) porque esa operacion congela
// el hilo de UI. El build de juegos corre en un hilo worker y el editor sigue dibujando: aca se reusa el MISMO widget
// (Card + borde verde + mensaje con % + barra accent) dentro del frame normal del editor (sin clear ni swap), anclado
// ABAJO-centro, sobre el editor OSCURECIDO: mientras compila el editor no se usa ni se guarda (los eventos los descarta
// ProcessEvent, en main.cpp) y el velo lo deja a la vista.
void ProgressOverlayRender(const std::string& stage, float frac) {
    if (!progressPopup) progressPopup = new ProgressPopup();
    {   // el velo: toda la ventana, atenuada
        static Rec2D* veil = NULL;
        if (!veil) veil = new Rec2D();
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
        veil->SetSize(0, 0, (GLshort)MenuScreenW, (GLshort)MenuScreenH);
        veil->RenderObject(false);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }
    progressPopup->message = stage;
    progressPopup->frac = frac;
    progressPopup->Layout();          // mismo tamano/posicion X que el modal
#ifndef W3D_SYMBIAN
    // en PC el modal va centrado; el overlay va ABAJO (en Symbian Layout ya lo deja abajo)
    progressPopup->y = MenuScreenH - progressPopup->popUpWindow->height - marginGS;
#endif
    progressPopup->Draw();
    W3dDockProgress(frac); // EXTRA: el mismo % del build, en el icono del dock (se ve minimizado)
}
