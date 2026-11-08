// ============================================================================
//  w3d_psp.cpp -- lo propio de la PSP que no tiene un lugar en el codigo compartido.
//
//  - popen/pclose: la consola no lanza procesos (los usa "Compilar juego", que llama a cmake/emcc en la PC).
//  - W3dRunScript: el harness de pruebas (main/test, --script) no entra en la PSP.
//  - La memoria: la pila del hilo principal (1 MB) y lo que el heap le deja libre al sistema para los hilos de SDL.
//  - W3dPspControles: el control como mouse (la PSP no tiene pantalla tactil), con la interfaz tactil del editor (la
//    barra de herramientas con sus botones, el teclado numerico en pantalla) y el teclado QWERTY de Whisk3D cuando un
//    campo de texto toma el foco: no hay teclado.
//  - W3dPspDibujarCursor: el cursor (la consola no tiene uno del sistema).
//  - W3D_PSP_CAPTURA=n (make CAPTURA=n): despues del cuadro n la pantalla queda en captura.ppm, al lado del EBOOT.PBP
//    (para la consola de verdad; en el emulador captura_ppsspp.sh captura la ventana).
//  - W3D_PSP_PRUEBA="x,y x,y>x2,y2" (make PRUEBA=...): clicks y arrastres del cursor, uno por segundo despues de
//    arrancar (en pixeles de la pantalla, 0..479 x 0..271), para probar sin tocar el control (captura_ppsspp.sh).
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <pspmoduleinfo.h>

extern "C" FILE* popen(const char*, const char*) { errno = ENOSYS; return NULL; }
extern "C" int pclose(FILE*) { errno = ENOSYS; return -1; }

bool W3dRunScript(const std::string&) { return false; }

// SDL2 de la PSP trae el contexto de OpenGL de PSPGL (lo arma con SDL_WINDOW_OPENGL); Whisk3D no lo usa: dibuja con
// el GE directo (graphics/gu/w3dGraphicsGu.cpp). Sin PSPGL en el link, las funciones de EGL que nombra SDL fallan siempre
extern "C" {
int eglGetError(void) { return 0x3001; }   // (EGL_NOT_INITIALIZED)
void* eglGetDisplay(void*) { return NULL; }
unsigned eglInitialize(void*, int*, int*) { return 0; }
unsigned eglTerminate(void*) { return 0; }
unsigned eglChooseConfig(void*, const int*, void*, int, int*) { return 0; }
unsigned eglGetConfigAttrib(void*, void*, int, int*) { return 0; }
void* eglCreateContext(void*, void*, void*, const int*) { return NULL; }
unsigned eglDestroyContext(void*, void*) { return 0; }
void* eglCreateWindowSurface(void*, void*, void*, const int*) { return NULL; }
unsigned eglMakeCurrent(void*, void*, void*, void*) { return 0; }
unsigned eglSwapBuffers(void*, void*) { return 0; }
unsigned eglSwapInterval(void*, int) { return 0; }
void* eglGetProcAddress(const char*) { return NULL; }
}

// (la informacion del modulo y los atributos del hilo principal los pone SDL2main)
extern "C" {
PSP_MAIN_THREAD_STACK_SIZE_KB(1024);
PSP_HEAP_THRESHOLD_SIZE_KB(1536);   // (el heap toma toda la memoria libre menos esto: las pilas de los hilos de SDL)
}

#include <SDL.h>
#include "graphics/w3dGraphics.h"
#include "graphics/gu/w3dGu.h"
#include "undo/Undo.h"
#include "TextField.h"
#include "ViewPorts/PopUp/PopUpBase.h"
#include "w3dlog.h"
namespace gfx = w3dEngine;

extern SDL_Window* window;               // (variables.cpp)
extern SDL_GameController* controller;   // el primer control abierto (controles.cpp): en la PSP, la consola
extern bool g_redraw;
extern Uint32 g_lastFingerTicks;          // (controles.cpp: con algo, la interfaz es la tactil)
void QwertyAbrir();

// ----------------------------------------------------------------------------------------------------------------
//  EL CURSOR: una flecha blanca con borde negro arriba de todo, al final de cada cuadro (main.cpp, antes de mostrarlo).
//  Con el juego corriendo no se dibuja: el control es del juego
// ----------------------------------------------------------------------------------------------------------------
static bool gEnJuego = false;
void W3dPspDibujarCursor(int anchoPantalla, int altoPantalla) {
    if (gEnJuego) return;
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    // la flecha (px): punta arriba-izquierda, en triangulos (la muesca de la cola es concava)
    static const float kPunta[7][2] = { {0, 0}, {0, 16}, {4, 12}, {7, 18}, {9, 17}, {6, 11}, {11, 11} };
    static const int kTri[5][3] = { {0, 1, 2}, {0, 2, 5}, {0, 5, 6}, {2, 3, 4}, {2, 4, 5} };
    static float v[2][15 * 2];
    for (int capa = 0; capa < 2; capa++) {   // 0 = el borde (un poco mas grande), 1 = el relleno
        const float esc = capa == 0 ? 1.22f : 1.0f, d = capa == 0 ? -1.0f : 0.0f;
        int k = 0;
        for (int t = 0; t < 5; t++)
            for (int j = 0; j < 3; j++) {
                v[capa][k++] = mx + d + kPunta[kTri[t][j]][0] * esc;
                v[capa][k++] = my + d + kPunta[kTri[t][j]][1] * esc;
            }
    }
    gfx::Viewport(0, 0, anchoPantalla, altoPantalla);
    gfx::Disable(gfx::ScissorTest);
    gfx::Disable(gfx::DepthTest);
    gfx::Disable(gfx::Texture2D);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog);
    gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Blend);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::MatrixMode(gfx::Projection); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Ortho(0.0f, (float)anchoPantalla, (float)altoPantalla, 0.0f, -1.0f, 1.0f);
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
//  EL CONTROL COMO MOUSE (fuera del modo juego), como en la Dreamcast:
//    analogico: mueve el cursor (mas rapido cuanto mas se inclina)     cruceta: el cursor de a un pixel (punteria)
//    cruz: click izquierdo (mantenida, arrastra: en la vista 3D orbita) circulo: click derecho
//    cuadrado: boton del medio (mantenido + analogico: orbitar)        triangulo: Enter (confirmar)
//    L / R: la rueda (alejar / acercar)                                Start: Esc (cancelar)
//    Select: deshacer
//  Lo que genera son eventos de SDL comunes (los mismos de un mouse y un teclado), asi el editor no se entera. Con el
//  juego corriendo no hace nada: el control es del juego.
// ----------------------------------------------------------------------------------------------------------------
static SDL_Window* Ventana() { SDL_Window* w = SDL_GetMouseFocus(); return w ? w : window; }
static void EmpujarBoton(Uint8 boton, bool abajo) {
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    SDL_Window* w = Ventana();
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = abajo ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.windowID = w ? SDL_GetWindowID(w) : 0;
    e.button.which = 0;
    e.button.button = boton;
    e.button.state = abajo ? SDL_PRESSED : SDL_RELEASED;
    e.button.clicks = 1;
    e.button.x = x; e.button.y = y;
    SDL_PushEvent(&e);
}
static void EmpujarRueda(int dy) {
    SDL_Window* w = Ventana();
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEWHEEL;
    e.wheel.windowID = w ? SDL_GetWindowID(w) : 0;
    e.wheel.which = 0;
    e.wheel.y = dy;
    e.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    SDL_PushEvent(&e);
}
static void EmpujarTecla(SDL_Scancode sc, bool abajo) {
    SDL_Window* w = Ventana();
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = abajo ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.windowID = w ? SDL_GetWindowID(w) : 0;
    e.key.state = abajo ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.scancode = sc;
    e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    e.key.keysym.mod = SDL_GetModState();
    SDL_PushEvent(&e);
}
// el cursor a (x, y): SDL lo manda como un movimiento del mouse (y la ventana queda con el foco del mouse)
static void MoverCursor(int x, int y) {
    if (x < 0) x = 0; if (x > gfx::GuAncho - 1) x = gfx::GuAncho - 1;
    if (y < 0) y = 0; if (y > gfx::GuAlto - 1) y = gfx::GuAlto - 1;
    SDL_WarpMouseInWindow(Ventana(), x, y);
}
// el eje con zona muerta, proporcional a lo que se inclina: -1..1 (el analogico de la PSP no vuelve justo al centro)
static float EjeCurva(Sint16 v) {
    const float zm = 0.25f;
    float f = v / 32767.0f;
    const float a = f < 0.0f ? -f : f;
    if (a <= zm) return 0.0f;
    const float t = (a - zm) / (1.0f - zm);
    return f < 0.0f ? -t : t;
}

// EL TECLADO: cuando un campo de texto toma el foco, el QWERTY de Whisk3D (se escribe con el cursor). Si se lo cierra
// con el campo todavia enfocado, no vuelve a abrirse hasta que el foco cambie
static void TecladoSiHaceFalta() {
    static TextField* campoAntes = NULL;
    if (g_textFieldActivo == campoAntes) return;
    campoAntes = g_textFieldActivo;
    if (g_textFieldActivo && !(PopUpActive && PopUpActive->EsTeclado())) QwertyAbrir();
}

#ifdef W3D_PSP_PRUEBA
static void PruebaTick();
#endif

void W3dPspControles(bool enJuego) {
    struct Boton { SDL_GameControllerButton b; bool antes; };
    static Boton botones[6] = { { SDL_CONTROLLER_BUTTON_A, false }, { SDL_CONTROLLER_BUTTON_B, false },
                                { SDL_CONTROLLER_BUTTON_X, false }, { SDL_CONTROLLER_BUTTON_Y, false },
                                { SDL_CONTROLLER_BUTTON_START, false }, { SDL_CONTROLLER_BUTTON_BACK, false } };
    static Uint32 tAntes = 0, cruceta0 = 0, ruedaT = 0;
    static float restoX = 0.0f, restoY = 0.0f;
    static bool primera = true;
    if (primera) {
        primera = false;
        g_lastFingerTicks = 1;   // (la interfaz tactil: sin teclado, los atajos van por los botones de la pantalla)
        MoverCursor(gfx::GuAncho / 2, gfx::GuAlto / 2);
    }
    gEnJuego = enJuego;
    const Uint32 ahora = SDL_GetTicks();
    float dt = tAntes ? (ahora - tAntes) / 1000.0f : 0.0f;
    if (dt > 0.1f) dt = 0.1f;   // (tras una espera larga no pega un salto)
    tAntes = ahora;
    TecladoSiHaceFalta();
#ifdef W3D_PSP_PRUEBA
    PruebaTick();
#endif
    SDL_GameController* gc = controller;
    if (!gc || !SDL_GameControllerGetAttached(gc)) return;

    // EL JUEGO MANDA: se suelta lo que estuviera apretado (que no quede un click pegado) y no se hace nada mas
    if (enJuego) {
        for (int i = 0; i < 6; i++) {
            if (!botones[i].antes) continue;
            botones[i].antes = false;
            if (i == 0) EmpujarBoton(SDL_BUTTON_LEFT, false);
            else if (i == 1) EmpujarBoton(SDL_BUTTON_RIGHT, false);
            else if (i == 2) EmpujarBoton(SDL_BUTTON_MIDDLE, false);
            else if (i == 3) EmpujarTecla(SDL_SCANCODE_RETURN, false);
            else if (i == 4) EmpujarTecla(SDL_SCANCODE_ESCAPE, false);
        }
        return;
    }

    // ---- mover el cursor: analogico + cruceta ----
    const float ax = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX));
    const float ay = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY));
    int cx = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) - SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    int cy = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) - SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP);
    // la cruceta es la PUNTERIA: cada toque mueve 1 pixel; mantenida, despues de un momento avanza despacio y mas
    // tarde rapido
    int pasoX = 0, pasoY = 0;
    float velCruceta = 0.0f;   // px/s
    if (cx || cy) {
        if (!cruceta0) { cruceta0 = ahora; pasoX = cx; pasoY = cy; }
        else if (ahora - cruceta0 > 1000) velCruceta = 160.0f;
        else if (ahora - cruceta0 > 300) velCruceta = 30.0f;
    } else cruceta0 = 0;
    // el analogico al fondo cruza la pantalla en un par de segundos
    restoX += ax * 200.0f * dt + cx * velCruceta * dt;
    restoY += ay * 200.0f * dt + cy * velCruceta * dt;
    const int mx = (int)restoX + pasoX, my = (int)restoY + pasoY;
    restoX -= (int)restoX; restoY -= (int)restoY;
    if (mx || my) {
        int x = 0, y = 0;
        SDL_GetMouseState(&x, &y);
        MoverCursor(x + mx, y + my);
    }
    const bool moviendo = ax != 0.0f || ay != 0.0f || cx || cy;

    // ---- botones: clicks y teclas ----
    for (int i = 0; i < 6; i++) {
        const bool ahoraAbajo = SDL_GameControllerGetButton(gc, botones[i].b) != 0;
        if (ahoraAbajo == botones[i].antes) continue;
        botones[i].antes = ahoraAbajo;
        if (i == 0) EmpujarBoton(SDL_BUTTON_LEFT, ahoraAbajo);
        else if (i == 1) EmpujarBoton(SDL_BUTTON_RIGHT, ahoraAbajo);
        else if (i == 2) EmpujarBoton(SDL_BUTTON_MIDDLE, ahoraAbajo);
        else if (i == 3) EmpujarTecla(SDL_SCANCODE_RETURN, ahoraAbajo);
        else if (i == 4) EmpujarTecla(SDL_SCANCODE_ESCAPE, ahoraAbajo);
        else if (ahoraAbajo) { UndoDeshacer(); g_redraw = true; }
    }

    // ---- L / R: la rueda (un paso enseguida y despues uno cada 120 ms mientras se mantiene) ----
    const bool r = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
    const bool l = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
    const int rueda = r ? 1 : (l ? -1 : 0);
    if (rueda) {
        if (!ruedaT || ahora - ruedaT >= 120) { EmpujarRueda(rueda); ruedaT = ahora; }
    } else ruedaT = 0;

    // mientras algo sigue apretado el bucle no se duerme (si no, el cursor avanzaria a los saltos)
    if (moviendo || rueda) g_redraw = true;
}

// ----------------------------------------------------------------------------------------------------------------
//  LA PRUEBA SIN CONTROL (make PRUEBA="x,y x,y>x2,y2 z-3"): un paso por segundo desde los 3 segundos de arrancar. "x,y"
//  es un click ahi; "x,y>x2,y2", apretar en el primero, llevar el cursor al segundo y soltar; "z+n" / "z-n", n pasos de
//  la rueda donde este el cursor (acercar / alejar)
// ----------------------------------------------------------------------------------------------------------------
#ifdef W3D_PSP_PRUEBA
static void PruebaTick() {
    static const char* p = W3D_PSP_PRUEBA;
    static Uint32 t0 = 0;
    static int fase = 0;               // (dentro de un arrastre: 0 apretar, 1..20 mover, 21 soltar)
    static int ax = 0, ay = 0, bx = 0, by = 0;
    static bool arrastre = false;
    const Uint32 ahora = SDL_GetTicks();
    if (!t0) t0 = ahora + 3000;
    if (ahora < t0) { g_redraw = true; return; }
    if (fase == 0) {
        while (*p == ' ') p++;
        if (!*p) return;
        int pasos = 0;
        if (*p == 'z' && sscanf(p + 1, "%d", &pasos) == 1) {   // (la rueda, en el cursor: z+3 acerca, z-3 aleja)
            for (int i = 0; i < (pasos < 0 ? -pasos : pasos); i++) EmpujarRueda(pasos < 0 ? -1 : 1);
            while (*p && *p != ' ') p++;
            t0 = ahora + 1000;
            g_redraw = true;
            return;
        }
        if (sscanf(p, "%d,%d>%d,%d", &ax, &ay, &bx, &by) == 4) arrastre = true;
        else if (sscanf(p, "%d,%d", &ax, &ay) == 2) arrastre = false;
        else { p = ""; return; }
        while (*p && *p != ' ') p++;
        MoverCursor(ax, ay);
        EmpujarBoton(SDL_BUTTON_LEFT, true);
        fase = 1;
    } else if (arrastre && fase <= 20) {
        MoverCursor(ax + (bx - ax) * fase / 20, ay + (by - ay) * fase / 20);
        fase++;
    } else if (fase < 1000) {
        EmpujarBoton(SDL_BUTTON_LEFT, false);
        fase = 1000;
        t0 = ahora + 1000;   // (el proximo paso, en un segundo)
        return;
    } else fase = 0;
    g_redraw = true;
    if (fase != 0) t0 = ahora + 30;
}
#endif

// ----------------------------------------------------------------------------------------------------------------
//  LA MEMORIA: al dibujar el primer cuadro se anota cuanto usa el heap y cuanto queda en el sistema (la PSP 1000 tiene
//  24 MB para el programa; las otras, con MEMSIZE=1 en el PARAM.SFO, 52)
//  LA CAPTURA (make CAPTURA=n): la pantalla que se ve, a captura.ppm al lado del EBOOT.PBP, despues del cuadro n
// ----------------------------------------------------------------------------------------------------------------
#include <malloc.h>
#include <pspsysmem.h>
void W3dPspCuadroDibujado() {
    static int cuadro = 0;
    ++cuadro;
    if (cuadro == 1) {
        const struct mallinfo mi = mallinfo();
        w3dLogf("[memoria] primer cuadro: heap en uso %u KB (pedidos al sistema %u KB), libre fuera del heap %u KB",
                (unsigned)mi.uordblks / 1024u, (unsigned)mi.arena / 1024u, (unsigned)sceKernelTotalFreeMemSize() / 1024u);
    }
#ifdef W3D_PSP_CAPTURA
    if (cuadro != W3D_PSP_CAPTURA) return;
    const unsigned char* fb = gfx::GuPantallaMostrada();
    FILE* f = fopen("captura.ppm", "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", (int)gfx::GuAncho, (int)gfx::GuAlto);
    for (int y = 0; y < gfx::GuAlto; y++)
        for (int x = 0; x < gfx::GuAncho; x++) {
            const unsigned char* q = fb + ((size_t)y * gfx::GuPasoPantalla() + x) * 4;   // (RGBA; el alfa es el estencil)
            fwrite(q, 1, 3, f);
        }
    fclose(f);
    w3dLogf("[psp] captura: captura.ppm (cuadro %d)", cuadro);
#endif
}
