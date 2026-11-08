// ============================================================================
//  w3d_3ds.cpp -- lo propio de la Nintendo 3DS que no tiene un lugar en el codigo compartido.
//
//  - popen/pclose: la consola no lanza procesos (los usa "Compilar juego", que llama a cmake/emcc en la PC).
//  - sigaction: SDL la pide para atajar Ctrl+C; la consola no tiene senales.
//  - W3dRunScript: el harness de pruebas (main/test, --script) no entra en la 3DS.
//  - W3d3dsPantallaArriba: la pantalla de arriba (la grande): la vista 3D activa. El editor va en la de abajo, la
//    tactil.
//  - W3d3dsControles: los botones y los sticks en el editor (orbitar, panear, zoom, atajos).
//  - W3D_3DS_CAPTURA=n (make CAPTURA=n): despues del cuadro n la pantalla de abajo se guarda en la tarjeta SD como
//    sdmc:/whisk3d_captura.ppm (para la consola de verdad; en el emulador captura_azahar.sh saca las dos pantallas).
//  - W3D_3DS_PRUEBA=n (make PRUEBA=n): las pruebas (1 = rendimiento: la vista gira sin parar y el log anota cuanto
//    tarda cada cuadro y en que).
// ============================================================================
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include <string.h>
#include "graphics/citro3d/w3dC3D.h"
#include "w3dGraphics.h"

extern "C" FILE* popen(const char*, const char*) { errno = ENOSYS; return NULL; }
extern "C" int pclose(FILE*) { errno = ENOSYS; return -1; }
// (SDL instala manejadores de senales si newlib declara sigaction; la 3DS no tiene senales)
struct sigaction;
extern "C" int sigaction(int, const struct sigaction*, struct sigaction*) { errno = ENOSYS; return -1; }

bool W3dRunScript(const std::string&) { return false; }

// LA PANTALLA DE ARRIBA: la vista 3D activa en grande (400x240), solo la escena (la barra y los menus quedan abajo,
// que es donde se toca). Va al empezar el cuadro, antes que el editor: el viewport se dibuja con el tamano de arriba
// y vuelve al suyo, y lo ultimo que queda calculado (para tocar y seleccionar) es lo de la pantalla de abajo
#include "ViewPorts/ViewPort3D.h"
extern Viewport3D* Viewport3DActive;
extern int W3dPantallaAlto;
void W3d3dsPantallaArriba() {
    namespace gfx = w3dEngine;
    gfx::C3DPantalla(gfx::PantallaArriba);
    Viewport3D* vp = Viewport3DActive;
    if (vp && vp->width > 0 && vp->height > 0) {
        const int x = vp->x, y = vp->y, w = vp->width, h = vp->height, alto = W3dPantallaAlto;
        const GLfloat aspecto = vp->aspect;
        vp->x = 0; vp->y = 0; vp->width = 400; vp->height = 240; vp->aspect = 400.0f / 240.0f;
        W3dPantallaAlto = 240;
        vp->soloEscena = true;
        vp->Render();
        vp->soloEscena = false;
        vp->x = x; vp->y = y; vp->width = w; vp->height = h; vp->aspect = aspecto;
        W3dPantallaAlto = alto;
        gfx::Disable(gfx::ScissorTest);
    }
    gfx::C3DPantalla(gfx::PantallaAbajo);
}

// ----------------------------------------------------------------------------------------------------------------
//  LOS BOTONES EN EL EDITOR (fuera del modo juego; con el juego corriendo el control es del juego). Lo que se toca va
//  por la pantalla tactil; los botones son lo que el dedo no puede hacer solo (la 3DS tiene un solo punto de toque:
//  no hay pellizco) y los atajos de siempre:
//    circle pad: orbitar la vista          C-stick (New 3DS): panear          ZL / ZR: alejar / acercar
//    cruceta: las flechas                  L: Shift (mantenido)               R: Ctrl (mantenido)
//    A: Enter (confirmar)                  B: Escape (cancelar)               X: Tab (modo objeto / edicion)
//    Y: deshacer                           Select: rehacer
// ----------------------------------------------------------------------------------------------------------------
#include <SDL.h>
#include "undo/Undo.h"
extern SDL_GameController* controller;   // el primer control abierto (controles.cpp): en la 3DS, la consola
extern bool g_redraw;
static Uint16 gMods = KMOD_NONE;          // lo que L y R tienen apretado (va en cada tecla que se manda)
static void EmpujarTecla(SDL_Scancode sc, bool abajo) {
    SDL_Window* w = SDL_GetMouseFocus();
    SDL_Event e;
    SDL_memset(&e, 0, sizeof(e));
    e.type = abajo ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.windowID = w ? SDL_GetWindowID(w) : 0;
    e.key.state = abajo ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.scancode = sc;
    e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    e.key.keysym.mod = gMods;   // (los atajos leen el mod de la tecla: el teclado de SDL no sabe de L y R)
    SDL_PushEvent(&e);
}
// el eje con zona muerta, proporcional a lo que se inclina: -1..1
static float EjeCurva(Sint16 v) {
    const float zm = 0.18f;
    float f = v / 32767.0f;
    const float a = f < 0.0f ? -f : f;
    if (a <= zm) return 0.0f;
    const float t = (a - zm) / (1.0f - zm);
    return f < 0.0f ? -t : t;
}
void W3d3dsControles(bool enJuego) {
    struct Boton { SDL_GameControllerButton b; SDL_Scancode tecla; bool antes; };
    static Boton botones[] = {
        { SDL_CONTROLLER_BUTTON_A, SDL_SCANCODE_RETURN, false },        { SDL_CONTROLLER_BUTTON_B, SDL_SCANCODE_ESCAPE, false },
        { SDL_CONTROLLER_BUTTON_X, SDL_SCANCODE_TAB, false },           { SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_SCANCODE_UP, false },
        { SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_SCANCODE_DOWN, false },  { SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_SCANCODE_LEFT, false },
        { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_SCANCODE_RIGHT, false },
        { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_SCANCODE_LSHIFT, false }, { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_SCANCODE_LCTRL, false },
    };
    const int nBotones = (int)(sizeof(botones) / sizeof(botones[0]));
    static bool yAntes = false, selectAntes = false;
    static Uint32 tAntes = 0;
    static float restoOX = 0.0f, restoOY = 0.0f, restoPX = 0.0f, restoPY = 0.0f;
    const Uint32 ahora = SDL_GetTicks();
    float dt = tAntes ? (ahora - tAntes) / 1000.0f : 0.0f;
    if (dt > 0.1f) dt = 0.1f;   // (tras una espera larga no pega un salto)
    tAntes = ahora;
    SDL_GameController* gc = controller;
    if (!gc || !SDL_GameControllerGetAttached(gc)) return;

    // EL JUEGO MANDA: se suelta lo que estuviera apretado (que no quede una tecla pegada) y no se hace nada mas
    if (enJuego) {
        for (int i = 0; i < nBotones; i++) if (botones[i].antes) { botones[i].antes = false; EmpujarTecla(botones[i].tecla, false); }
        gMods = KMOD_NONE;
        return;
    }

    // ---- botones: teclas ----
    for (int i = 0; i < nBotones; i++) {
        const bool abajo = SDL_GameControllerGetButton(gc, botones[i].b) != 0;
        if (abajo == botones[i].antes) continue;
        botones[i].antes = abajo;
        if (botones[i].tecla == SDL_SCANCODE_LSHIFT) gMods = abajo ? (gMods | KMOD_LSHIFT) : (gMods & ~KMOD_LSHIFT);
        if (botones[i].tecla == SDL_SCANCODE_LCTRL) gMods = abajo ? (gMods | KMOD_LCTRL) : (gMods & ~KMOD_LCTRL);
        EmpujarTecla(botones[i].tecla, abajo);
    }
    const bool y = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_Y) != 0;
    if (y && !yAntes) { UndoDeshacer(); g_redraw = true; }
    yAntes = y;
    const bool sel = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK) != 0;
    if (sel && !selectAntes) { UndoRehacer(); g_redraw = true; }
    selectAntes = sel;

    // ---- la vista: orbitar, panear y zoom (en pixeles por segundo, como si se arrastrara el dedo) ----
    Viewport3D* vp = Viewport3DActive;
    if (!vp || dt <= 0.0f) return;
    const float ox = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX));
    const float oy = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY));
    const float px = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX));
    const float py = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY));
    const float zoom = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) -
                       EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    restoOX += ox * 220.0f * dt; restoOY += oy * 220.0f * dt;
    restoPX += px * 220.0f * dt; restoPY += py * 220.0f * dt;
    const int odx = (int)restoOX, ody = (int)restoOY, pdx = (int)restoPX, pdy = (int)restoPY;
    restoOX -= odx; restoOY -= ody; restoPX -= pdx; restoPY -= pdy;
    if (odx || ody) { vp->OrbitarFlecha(odx, ody); g_redraw = true; }
    if (pdx || pdy) { vp->PanFlecha(pdx, pdy); g_redraw = true; }
    if (zoom != 0.0f) { vp->Zoom(zoom * 12.0f * dt); g_redraw = true; }   // (al fondo: la distancia a la mitad por segundo)
    // (mientras se mantiene un eje la vista se mueve sola: el loop no se queda esperando eventos)
    if (ox != 0.0f || oy != 0.0f || px != 0.0f || py != 0.0f || zoom != 0.0f) g_redraw = true;
}

#ifdef W3D_3DS_CAPTURA
// la pantalla de abajo tal como se ve (el framebuffer que muestra la consola), a un PPM en la SD. El framebuffer esta
// girado: 240 de ancho por 320 de alto, de abajo hacia arriba
static void GuardarCaptura() {
    u16 fw = 0, fh = 0;
    const u8* fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);   // fw = 240, fh = 320
    if (!fb) return;
    FILE* f = fopen("sdmc:/whisk3d_captura.ppm", "wb");
    if (!f) return;
    const int W = fh, H = fw;   // apaisada: 320 x 240
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            // (columna x de la pantalla = fila x del framebuffer; la fila y de arriba = la columna H-1-y)
            const u8* p = fb + ((size_t)x * fw + (size_t)(H - 1 - y)) * 4;   // RGBA8 guardado como A, B, G, R
            const u8 rgb[3] = { p[3], p[2], p[1] };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
    printf("W3DCAP sdmc:/whisk3d_captura.ppm\n");
}
#endif

#ifdef W3D_3DS_PRUEBA
#include <citro3d.h>
#include "config/W3dProfile.h"
extern bool g_redraw;
#endif

// lo llama main.cpp despues de cada cuadro dibujado
void W3d3dsCuadroDibujado() {
    static int cuadro = 0;
    ++cuadro;
#ifdef W3D_3DS_PRUEBA
    // cuanto tardan los cuadros y en que (el perfilador del editor, W3dProfile.h, y los tiempos de citro3d: lo que la
    // CPU tardo en armar los comandos y lo que tardo la GPU en dibujarlos), el promedio de cada 30 cuadros en el log
    static u64 tAntes = 0;
    static double suma[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    if (cuadro == 1) w3dEngine::C3DMedir(true);
    static int nProm = 0;
    const u64 t = svcGetSystemTick();
    if (tAntes && cuadro > 2) {
        suma[0] += (double)(t - tAntes) / CPU_TICKS_PER_MSEC; suma[1] += g_prof.logic; suma[2] += g_prof.viewport3d;
        suma[3] += g_prof.render; suma[4] += g_prof.swap; suma[5] += C3D_GetProcessingTime(); suma[6] += C3D_GetDrawingTime();
        suma[7] += w3dEngine::C3DTiempoDrawsMs(); suma[8] += g_prof.scene;
        if (++nProm == 30) {
            const w3dEngine::StatsDeCuadro& st = w3dEngine::g_statCuadro;
            char linea[256];
            snprintf(linea, sizeof(linea), "[prueba] cuadros %d-%d: %.1f ms (logica %.1f, escena %.1f, viewport 3D %.1f, "
                     "render %.1f, swap %.1f; citro3d cpu %.1f gpu %.1f; draws del backend %.1f) draws %d, triangulos %d",
                     cuadro - 29, cuadro, suma[0] / 30, suma[1] / 30, suma[8] / 30, suma[2] / 30, suma[3] / 30, suma[4] / 30,
                     suma[5] / 30, suma[6] / 30, suma[7] / 30, st.draws, st.tris);
            svcOutputDebugString(linea, strlen(linea));
            for (int k = 0; k < 9; k++) suma[k] = 0;
            nProm = 0;
        }
    }
    tAntes = t;
#endif
#if W3D_3DS_PRUEBA == 1
    // PRUEBA DE RENDIMIENTO (make PRUEBA=1): la vista gira sin parar (un cuadro nuevo cada vez, como al orbitar con
    // el dedo)
    if (cuadro > 30 && Viewport3DActive) { Viewport3DActive->OrbitarFlecha(4, 0); g_redraw = true; }
#endif
#ifdef W3D_3DS_CAPTURA
    if (cuadro == W3D_3DS_CAPTURA) {
        gspWaitForVBlank();   // (que la transferencia a la pantalla haya terminado)
        GuardarCaptura();
        svcOutputDebugString("W3DCAP FIN", 10);
    }
#endif
}
