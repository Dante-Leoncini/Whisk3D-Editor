// ============================================================================
//  w3d_dreamcast.cpp -- lo propio de la Dreamcast que no tiene un lugar en el codigo compartido.
//
//  - popen/pclose: la consola no lanza procesos (los usa "Compilar juego", que llama a cmake/emcc en la PC). Fallan
//    siempre, asi el editor muestra el error de siempre ("no se pudo ejecutar").
//  - W3dRunScript: el harness de pruebas (main/test, --script) no entra en la Dreamcast.
//  - W3dDcDrawCursor: el cursor del mouse (la consola no tiene uno del sistema).
//  - W3dDcControlAsMouse: el control de la Dreamcast maneja el cursor fuera del modo juego.
//  - OnFinish: si una excepcion de C++ queda sin atrapar, antes de abortar escribe por la consola serie cual fue y la
//    pila (las direcciones se traducen con sh-elf-addr2line sobre whisk3d_debug.elf). Y al salir, la consola se reinicia.
//  - W3dDcDrawnFrame: lo llama main.cpp despues de cada cuadro. Con W3D_DC_CAPTURE=n (make CAPTURA=n), en el cuadro
//    n la pantalla sale por la consola serie en base64 (entre "W3DCAP INICIO" y "W3DCAP FIN"): asi se ve en la PC lo
//    que dibuja la consola sin tener una (captura_flycast.sh la pasa a PNG). Sin CAPTURA no hace nada.
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string>

extern "C" FILE* popen(const char*, const char*) { errno = ENOSYS; return NULL; }
extern "C" int pclose(FILE*) { errno = ENOSYS; return -1; }

bool W3dRunScript(const std::string&) { return false; }

// ----------------------------------------------------------------------------------------------------------------
//  EXCEPCION SIN ATRAPAR: en la consola no hay depurador ni volcado. Se anota que excepcion fue y la pila de llamadas
//  (la desenrolla el mismo mecanismo de las excepciones, no hacen falta punteros de marco) y recien ahi se aborta
// ----------------------------------------------------------------------------------------------------------------
#include <exception>
#include <unwind.h>

static _Unwind_Reason_Code AnnotateCall(struct _Unwind_Context* ctx, void* count) {
    int* n = (int*)count;
    printf(" 0x%08lx", (unsigned long)_Unwind_GetIP(ctx));
    return ++*n < 32 ? _URC_NO_REASON : _URC_END_OF_STACK;
}
static void OnFinish() {
    printf("[cuelgue] excepcion de C++ sin atrapar");
    std::exception_ptr e = std::current_exception();
    if (e) {
        try { std::rethrow_exception(e); }
        catch (const std::exception& x) { printf(": %s", x.what()); }
        catch (...) {}
    }
    printf("\n[cuelgue] pila (sh-elf-addr2line -Cfpe whisk3d_debug.elf ...):");
    int n = 0;
    _Unwind_Backtrace(AnnotateCall, &n);
    printf("\n");
    fflush(stdout);
    abort();
}
// AL SALIR (una excepcion, un exit() de GLdc, cerrar el editor): reiniciar la consola. Por defecto KallistiOS vuelve a
// quien lo arranco; sin un cargador (dcload) la BIOS emulada de Flycast volvia a correr el programa con la memoria del
// anterior, y arrancaba roto (texturas en blanco, un "Bad file number" al leer el config.start). Con dcload se deja como
// esta: el que depura quiere volver al cargador
#include <arch/arch.h>
#include <dc/fs_dcload.h>
static struct InstallOnFinish {
    InstallOnFinish() {
        std::set_terminate(OnFinish);
        if (dcload_type == DCLOAD_TYPE_NONE) arch_set_exit_path(ARCH_EXIT_REBOOT);
    }
} sInstallOnFinish;

// ----------------------------------------------------------------------------------------------------------------
//  CURSOR DEL MOUSE: la Dreamcast no tiene cursor del sistema. Con un mouse conectado (en cuanto se mueve) se dibuja
//  una flecha blanca con borde negro arriba de todo, al final de cada cuadro (main.cpp, antes del swap).
// ----------------------------------------------------------------------------------------------------------------
#include <SDL.h>
#include "graphics/w3dGraphics.h"
namespace gfx = w3dEngine;

#ifdef W3D_DC_CAPTURE
static bool gSeenCursor = true;    // (la captura de prueba lo muestra sin mover un mouse)
#else
static bool gSeenCursor = false;
#endif
void W3dMovedDcMouse() { gSeenCursor = true; }

#ifdef W3D_DC_TEST
double W3dNowMs();
static double gPrevOfSwap = 0.0;   // (la prueba mide cuanto tarda el swap: desde aca hasta W3dDcDrawnFrame)
#endif
void W3dDcDrawCursor(int screenWidth, int screenHeight) {
#ifdef W3D_DC_TEST
    gPrevOfSwap = W3dNowMs();
#endif
    if (!gSeenCursor) return;
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    // la flecha (px): punta arriba-izquierda, en triangulos (la muesca de la cola es concava)
    static const float kTip[7][2] = { {0, 0}, {0, 16}, {4, 12}, {7, 18}, {9, 17}, {6, 11}, {11, 11} };
    static const int kTri[5][3] = { {0, 1, 2}, {0, 2, 5}, {0, 5, 6}, {2, 3, 4}, {2, 4, 5} };
    static float v[2][15 * 2];
    for (int layer = 0; layer < 2; layer++) {   // 0 = el borde (un poco mas grande), 1 = el relleno
        const float esc = layer == 0 ? 1.22f : 1.0f, d = layer == 0 ? -1.0f : 0.0f;
        int k = 0;
        for (int t = 0; t < 5; t++)
            for (int j = 0; j < 3; j++) {
                v[layer][k++] = mx + d + kTip[kTri[t][j]][0] * esc;
                v[layer][k++] = my + d + kTip[kTri[t][j]][1] * esc;
            }
    }
    gfx::Viewport(0, 0, screenWidth, screenHeight);
    gfx::Disable(gfx::ScissorTest);
    gfx::Disable(gfx::DepthTest);
    gfx::Disable(gfx::Texture2D);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog);
    gfx::Disable(gfx::CullFace);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::MatrixMode(gfx::Projection); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Ortho(0.0f, (float)screenWidth, (float)screenHeight, 0.0f, -1.0f, 1.0f);
    gfx::MatrixMode(gfx::ModelView); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Color4f(0.0f, 0.0f, 0.0f, 1.0f);
    gfx::VertexPointer2f(0, v[0]);
    gfx::DrawTrianglesArray(15);
    gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    gfx::VertexPointer2f(0, v[1]);
    gfx::DrawTrianglesArray(15);
    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
}

// ----------------------------------------------------------------------------------------------------------------
//  EL CONTROL COMO MOUSE (fuera del modo juego). Sin mouse se edita igual: el analogico mueve el cursor (mas rapido
//  cuanto mas se inclina, hasta 240 px/s), la cruceta lo mueve de a un pixel por toque (y acelera si se mantiene),
//  A = click izquierdo (mantenido arrastra), B = click derecho, X = boton del medio (mantenido + analogico: orbitar la
//  vista), gatillos R / L = rueda (zoom), Y = Enter, Start = Esc. Con el juego corriendo no hace nada: el control es
//  del juego.
//  Lo que genera son eventos de SDL comunes (los mismos de un mouse y un teclado), asi el editor no se entera.
// ----------------------------------------------------------------------------------------------------------------
extern SDL_GameController* controller;   // el primer control abierto (controls.cpp)
extern bool g_redraw;

static void PushButton(Uint8 button, bool down) {
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    SDL_Window* w = SDL_GetMouseFocus();
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.windowID = w ? SDL_GetWindowID(w) : 0;
    e.button.which = 0;
    e.button.button = button;
    e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.button.clicks = 1;
    e.button.x = x; e.button.y = y;
    SDL_PushEvent(&e);
}
static void PushWheel(int dy) {
    SDL_Window* w = SDL_GetMouseFocus();
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEWHEEL;
    e.wheel.windowID = w ? SDL_GetWindowID(w) : 0;
    e.wheel.which = 0;
    e.wheel.y = dy;
    e.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    SDL_PushEvent(&e);
}
static void PushKey(SDL_Scancode sc, bool down) {
    SDL_Window* w = SDL_GetMouseFocus();
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.windowID = w ? SDL_GetWindowID(w) : 0;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.scancode = sc;
    e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    e.key.keysym.mod = SDL_GetModState();
    SDL_PushEvent(&e);
}
// el eje con zona muerta, proporcional a lo que se inclina: -1..1
static float AxisCurve(Sint16 v) {
    const float zm = 0.18f;
    float f = v / 32767.0f;
    const float a = f < 0.0f ? -f : f;
    if (a <= zm) return 0.0f;
    const float t = (a - zm) / (1.0f - zm);
    return f < 0.0f ? -t : t;
}

void W3dDcControlAsMouse(bool inGame) {
    struct SendButton { SDL_GameControllerButton b; bool prev; };
    static SendButton buttons[5] = { { SDL_CONTROLLER_BUTTON_A, false }, { SDL_CONTROLLER_BUTTON_B, false },
                                { SDL_CONTROLLER_BUTTON_X, false }, { SDL_CONTROLLER_BUTTON_Y, false },
                                { SDL_CONTROLLER_BUTTON_START, false } };
    static Uint32 tPrev = 0, dPad0 = 0, wheelT = 0;
    static float restX = 0.0f, restY = 0.0f;
    const Uint32 now = SDL_GetTicks();
    float dt = tPrev ? (now - tPrev) / 1000.0f : 0.0f;
    if (dt > 0.1f) dt = 0.1f;   // (tras una espera larga no pega un salto)
    tPrev = now;
    SDL_GameController* gc = controller;
    if (!gc || !SDL_GameControllerGetAttached(gc)) return;

    // EL JUEGO MANDA: se suelta lo que estuviera apretado (que no quede un click pegado) y no se hace nada mas
    if (inGame) {
        for (int i = 0; i < 5; i++) {
            if (!buttons[i].prev) continue;
            buttons[i].prev = false;
            if (i == 0) PushButton(SDL_BUTTON_LEFT, false);
            else if (i == 1) PushButton(SDL_BUTTON_RIGHT, false);
            else if (i == 2) PushButton(SDL_BUTTON_MIDDLE, false);
            else if (i == 3) PushKey(SDL_SCANCODE_RETURN, false);
            else PushKey(SDL_SCANCODE_ESCAPE, false);
        }
        return;
    }

    // ---- mover el cursor: analogico + cruceta ----
    const float ax = AxisCurve(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX));
    const float ay = AxisCurve(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY));
    int cx = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) - SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    int cy = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) - SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP);
    // la cruceta es la PUNTERIA: cada toque mueve 1 pixel (para agarrar un borde de 2 pixeles); mantenida, despues de
    // un momento avanza despacio y mas tarde rapido
    int stepX = 0, stepY = 0;
    float velDPad = 0.0f;   // px/s
    if (cx || cy) {
        if (!dPad0) { dPad0 = now; stepX = cx; stepY = cy; }
        else if (now - dPad0 > 1000) velDPad = 200.0f;
        else if (now - dPad0 > 300) velDPad = 40.0f;
    } else dPad0 = 0;
    // el analogico al fondo va como la cruceta mantenida; a medias, a media velocidad
    restX += ax * 240.0f * dt + cx * velDPad * dt;
    restY += ay * 240.0f * dt + cy * velDPad * dt;
    const int mx = (int)restX + stepX, my = (int)restY + stepY;
    restX -= (int)restX; restY -= (int)restY;
    if (mx || my) {
        int x = 0, y = 0;
        SDL_GetMouseState(&x, &y);
        x += mx; y += my;
        if (x < 0) x = 0; if (x > 639) x = 639;
        if (y < 0) y = 0; if (y > 479) y = 479;
        SDL_WarpMouseInWindow(SDL_GetMouseFocus(), x, y);   // (sin warp propio, SDL lo manda como movimiento del mouse)
    }
    const bool moving = ax != 0.0f || ay != 0.0f || cx || cy;

    // ---- botones: clicks y teclas ----
    for (int i = 0; i < 5; i++) {
        const bool nowDown = SDL_GameControllerGetButton(gc, buttons[i].b) != 0;
        if (nowDown == buttons[i].prev) continue;
        buttons[i].prev = nowDown;
        if (i == 0) PushButton(SDL_BUTTON_LEFT, nowDown);
        else if (i == 1) PushButton(SDL_BUTTON_RIGHT, nowDown);
        else if (i == 2) PushButton(SDL_BUTTON_MIDDLE, nowDown);
        else if (i == 3) PushKey(SDL_SCANCODE_RETURN, nowDown);
        else PushKey(SDL_SCANCODE_ESCAPE, nowDown);
    }

    // ---- gatillos: la rueda (un paso enseguida y despues uno cada 120 ms mientras se mantiene) ----
    const int r = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    const int l = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    const int wheel = (r > 16000) ? 1 : (l > 16000) ? -1 : 0;
    if (wheel) {
        if (!wheelT || now - wheelT >= 120) { PushWheel(wheel); wheelT = now; }
    } else wheelT = 0;

    // mientras algo sigue apretado el bucle no se duerme (si no, el cursor avanzaria a los saltos)
    if (moving || wheel) g_redraw = true;
}

// MEMORIA: la Dreamcast tiene 16 MB para todo (el ejecutable, el heap y la pila). Al dibujar el primer cuadro se
// anota cuanto quedo libre, para saber el margen que tiene la escena
#include <malloc.h>
#include "w3dlog.h"
extern "C" char end;   // (el fin del ejecutable cargado: lo define el linker)
static void AnnotateMemory() {
    const struct mallinfo mi = mallinfo();
    const unsigned exe = (unsigned)((unsigned long)&end - 0x8c010000ul);
    const unsigned free = 16u * 1024u * 1024u - exe - (unsigned)mi.arena + (unsigned)mi.fordblks;
    w3dLogf("[memoria] ejecutable %u KB, heap en uso %u KB, libre ~%u KB (de 16384)",
            exe / 1024u, (unsigned)mi.uordblks / 1024u, free / 1024u);
}

#ifdef W3D_DC_CAPTURE
#include <dc/video.h>
#include "W3dProfile.h"
#if W3D_DC_TEST == 7
#include "W3dInteractionState.h"
void LayoutModePick(int mode);
#endif
#if W3D_DC_TEST == 6
#include "render/RayTracer.h"
#include "ViewPorts/ViewPort3D.h"
extern Viewport3D* Viewport3DActive;
#endif

static void PrintBase64(const unsigned char* d, size_t n) {
    static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char line[80];
    int col = 0;
    for (size_t i = 0; i < n; i += 3) {
        const unsigned v = (unsigned)d[i] << 16 | (i + 1 < n ? (unsigned)d[i + 1] << 8 : 0u) | (i + 2 < n ? d[i + 2] : 0u);
        line[col++] = k[(v >> 18) & 63];
        line[col++] = k[(v >> 12) & 63];
        line[col++] = i + 1 < n ? k[(v >> 6) & 63] : '=';
        line[col++] = i + 2 < n ? k[v & 63] : '=';
        if (col >= 76) { line[col] = 0; puts(line); col = 0; }
    }
    if (col) { line[col] = 0; puts(line); }
}

void W3dDcDrawnFrame() {
    static int frame = 0;
    if (++frame == 1) AnnotateMemory();
#ifdef W3D_DC_TEST
    // cuanto tardan los cuadros y en que (el perfilador del editor, W3dProfile.h): el promedio cada 8 cuadros, en una
    // linea (la consola serie es lenta: una linea por cuadro ya agregaba varios ms)
    static Uint32 tPrev = 0;
    static double sum[5] = { 0, 0, 0, 0, 0 };
    static int nProm = 0;
    const Uint32 t = SDL_GetTicks();
    if (tPrev && frame > 2) {
        sum[0] += t - tPrev; sum[1] += g_prof.logic; sum[2] += g_prof.viewport3d; sum[3] += g_prof.render;
        sum[4] += W3dNowMs() - gPrevOfSwap;
        if (++nProm == 8) {
            const gfx::FrameStats& st = gfx::g_statFrame;
            printf("[prueba] cuadros %d-%d: %.1f ms (logica %.1f, viewport 3D %.1f, render %.1f, swap %.1f) "
                   "draws %d, triangulos %d\n", frame - 7, frame, sum[0] / 8, sum[1] / 8, sum[2] / 8, sum[3] / 8,
                   sum[4] / 8, st.draws, st.tris);
            for (int k = 0; k < 5; k++) sum[k] = 0;
            nProm = 0;
        }
    }
    tPrev = t;
#endif
#if W3D_DC_TEST == 1
    // PRUEBA DE ENTRADA (make CAPTURA=n PRUEBA=1): los mismos eventos que genera el control como mouse, para ver en la
    // captura que el editor responde en la consola: Tab (pasa a modo edicion) y un click en el menu del modo
    switch (frame) {
        case 3: PushKey(SDL_SCANCODE_TAB, true); break;
        case 4: PushKey(SDL_SCANCODE_TAB, false); break;
        case 5: SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 70, 11); break;
        case 6: PushButton(SDL_BUTTON_LEFT, true); break;
        case 7: PushButton(SDL_BUTTON_LEFT, false); break;
        default: break;
    }
#elif W3D_DC_TEST == 3
    // PRUEBA DE SELECCION (PRUEBA=3): click en el vacio (deselecciona), en la luz y en el cubo. La seleccion por color
    // se dibuja por software en la consola (ReadPixelsBegin en w3dGraphics.cpp)
    switch (frame) {
        case 3: SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 100, 350); break;
        case 4: PushButton(SDL_BUTTON_LEFT, true); break;
        case 5: PushButton(SDL_BUTTON_LEFT, false); break;
        case 6: SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 352, 137); break;
        case 7: PushButton(SDL_BUTTON_LEFT, true); break;
        case 8: PushButton(SDL_BUTTON_LEFT, false); break;
        case 9: SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 250, 250); break;
        case 10: PushButton(SDL_BUTTON_LEFT, true); break;
        case 11: PushButton(SDL_BUTTON_LEFT, false); break;
        default: break;
    }
#elif W3D_DC_TEST == 4
    // PRUEBA DE ARRASTRE (PRUEBA=4): el borde entre el viewport 3D y los paneles de la derecha, agarrado con el mismo
    // boton que manda el control (A) y llevado a la izquierda
    if (frame == 3) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 461, 300);
    else if (frame == 4) PushButton(SDL_BUTTON_LEFT, true);
    else if (frame >= 5 && frame <= 10) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 461 - (frame - 4) * 12, 300);
    else if (frame == 11) PushButton(SDL_BUTTON_LEFT, false);
    else if (frame == 12) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 300, 200);
#elif W3D_DC_TEST == 5
    // PRUEBA DE SELECCION EN MODO EDICION (PRUEBA=5): Tab y click sobre un vertice del cubo
    if (frame == 3) PushKey(SDL_SCANCODE_TAB, true);
    else if (frame == 4) PushKey(SDL_SCANCODE_TAB, false);
    else if (frame == 5) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 165, 157);
    else if (frame == 6) PushButton(SDL_BUTTON_LEFT, true);
    else if (frame == 7) PushButton(SDL_BUTTON_LEFT, false);
    else if (frame >= 8 && frame <= 14) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 330 + (frame & 1), 330);
#elif W3D_DC_TEST == 6
    // PRUEBA DEL TRAZADO DE RAYOS (PRUEBA=6): la casilla "Ray Tracing" de la tarjeta Render y el viewport en Rendered.
    // Despues el trazador pide cuadros solo hasta terminar (sube la imagen a la textura de a tiles)
    if (frame == 3 && Viewport3DActive) { g_rt.on = true; Viewport3DActive->view = RenderType::Rendered; g_redraw = true; }
#elif W3D_DC_TEST == 7
    // PRUEBA DE PINTURA DE TEXTURA (PRUEBA=7): el modo por la misma puerta que el menu, y un trazo arrastrado sobre el
    // cubo (la textura se actualiza de a pedazos)
    if (frame == 3) LayoutModePick(TexturePaint);
    else if (frame == 4) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 215, 230);
    else if (frame == 5) PushButton(SDL_BUTTON_LEFT, true);
    else if (frame >= 6 && frame <= 14) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 215 + (frame - 5) * 9, 230 + (frame - 5) * 4);
    else if (frame == 15) PushButton(SDL_BUTTON_LEFT, false);
    else if (frame >= 16 && frame <= 22) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 380 + (frame & 1), 330);
#elif W3D_DC_TEST == 8
    // PRUEBA DE ORBITA CON EL CONTROL (PRUEBA=8): lo mismo que hace el control como mouse al orbitar: X (el boton del
    // medio) mantenido y el cursor movido de a 4 pixeles por cuadro hacia la derecha, hasta pasar varias veces el borde
    // del viewport (ahi el editor lleva el cursor al otro borde para que el arrastre siga)
    if (frame == 3) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 230, 240);
    else if (frame == 4) PushButton(SDL_BUTTON_MIDDLE, true);
    else if (frame >= 5 && frame < 400) {
        int x = 0, y = 0;
        SDL_GetMouseState(&x, &y);
        x += 4; if (x > 639) x = 639;
        SDL_WarpMouseInWindow(SDL_GetMouseFocus(), x, y);
        if (frame % 5 == 0) { printf("[prueba8] cuadro %d cursor %d,%d\n", frame, x, y); fflush(stdout); }
    } else if (frame == 400) PushButton(SDL_BUTTON_MIDDLE, false);
#elif W3D_DC_TEST == 2
    // PRUEBA DE MOVER (PRUEBA=2): G sobre el cubo seleccionado, el mouse a la derecha, X (el eje: la linea larga del
    // eje en pantalla), mas mouse y un click que confirma
    if (frame == 3) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 300, 240);
    else if (frame == 4) PushKey(SDL_SCANCODE_G, true);
    else if (frame == 5) PushKey(SDL_SCANCODE_G, false);
    else if (frame >= 6 && frame <= 9) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 300 + (frame - 5) * 20, 240);
    else if (frame == 10) PushKey(SDL_SCANCODE_X, true);
    else if (frame == 11) PushKey(SDL_SCANCODE_X, false);
    else if (frame >= 12 && frame <= 17) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 380 + (frame - 11) * 15, 250);
    else if (frame == 18) PushButton(SDL_BUTTON_LEFT, true);
    else if (frame == 19) PushButton(SDL_BUTTON_LEFT, false);
    else if (frame >= 20 && frame <= 30) SDL_WarpMouseInWindow(SDL_GetMouseFocus(), 200 + frame * 4, 300);   // (cada evento hace un cuadro)
#endif
    if (frame != W3D_DC_CAPTURE) return;
    unsigned char* d = NULL;
    const size_t n = vid_screen_shot_data(&d);   // RGB de 24 bits del framebuffer que se esta mostrando
    if (!n || !d) { puts("W3DCAP ERROR"); fflush(stdout); return; }
    printf("W3DCAP INICIO %d %d %u\n", vid_mode->width, vid_mode->height, (unsigned)n);
    PrintBase64(d, n);
    puts("W3DCAP FIN");
    fflush(stdout);
    free(d);
}
#else
void W3dDcDrawnFrame() {
    static bool first = true;
    if (first) { first = false; AnnotateMemory(); }
}
#endif
