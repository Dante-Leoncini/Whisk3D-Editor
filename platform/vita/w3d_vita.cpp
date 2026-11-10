// ============================================================================
//  w3d_vita.cpp -- lo propio de la PS Vita que no tiene un lugar en el codigo compartido.
//
//  - popen/pclose: la consola no lanza procesos (los usan "Compilar juego", que llama a cmake/emcc en la PC, y la vista
//    previa de los videos, que llama a ffmpeg).
//  - W3dRunScript: el harness de pruebas (main/test, --script) no entra en la Vita.
//  - W3dVitaPuedeDibujar: vitaGL arma los shaders del pipeline fijo en la consola, con el compilador de shaders de
//    Sony (libshacccg.suprx), que no viene con la Vita ni con el kit libre: se instala aparte (ver el README). Sin el no
//    hay dibujo; se avisa en vez de colgarse en el primer cuadro.
//  - W3dVitaEventoValido: el panel tactil de atras no es la pantalla (sus dedos no tocan el editor).
//  - W3dVitaControles: los botones y los sticks en el editor (orbitar, panear, atajos). El zoom y el paneo con dos
//    dedos van por la pantalla, que es multitactil (como en Android).
//  - W3D_VITA_CAPTURA=n (make CAPTURA=n): despues del cuadro n la pantalla se guarda como
//    ux0:data/Whisk3D/captura.ppm (en el emulador la saca captura_vita3k.sh).
// ============================================================================
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <SDL.h>
#include "w3dGraphics.h"

extern "C" FILE* popen(const char*, const char*) { errno = ENOSYS; return NULL; }
extern "C" int pclose(FILE*) { errno = ENOSYS; return -1; }

bool W3dRunScript(const std::string&) { return false; }

#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include "w3dlog.h"
// vitaGL carga el compilador de shaders de UN solo lugar: ur0:data/external/libshacccg.suprx (donde lo deja ShaRKBR33D
// hoy). Las versiones viejas de ShaRKBR33D lo dejaban en ur0:data/libshacccg.suprx: si esta ahi (o en el sistema, en las
// consolas de desarrollo), se copia al lugar nuevo una sola vez. false = no esta en ningun lado o no se pudo copiar
// ('copiaFallo' = estaba en un lugar viejo)
static bool CopiarArchivoVita(const char* de, const char* a) {
    const SceUID in = sceIoOpen(de, SCE_O_RDONLY, 0);
    if (in < 0) return false;
    const SceUID out = sceIoOpen(a, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (out < 0) { sceIoClose(in); return false; }
    static char buf[64 * 1024];
    bool ok = true;
    for (;;) {
        const int n = sceIoRead(in, buf, sizeof(buf));
        if (n < 0) { ok = false; break; }
        if (n == 0) break;
        if (sceIoWrite(out, buf, n) != n) { ok = false; break; }
    }
    sceIoClose(in);
    sceIoClose(out);
    if (!ok) sceIoRemove(a);   // (un modulo a medias no se deja: vitaGL lo cargaria igual)
    return ok;
}
static bool CompiladorShadersVita(bool* copiaFallo) {
    static const char* const kNueva = "ur0:data/external/libshacccg.suprx";
    static const char* const kViejas[] = { "ur0:data/libshacccg.suprx", "vs0:sys/external/libshacccg.suprx" };
    if (copiaFallo) *copiaFallo = false;
    SceIoStat st;
    if (sceIoGetstat(kNueva, &st) >= 0) return true;
    for (int i = 0; i < 2; i++) {
        if (sceIoGetstat(kViejas[i], &st) < 0) continue;
        sceIoMkdir("ur0:data/external", 0777);
        if (CopiarArchivoVita(kViejas[i], kNueva)) {
            w3dLogf("[vita] el compilador de shaders estaba en %s: copiado a %s", kViejas[i], kNueva);
            return true;
        }
        w3dLogfE("[vita] el compilador de shaders esta en %s y no se pudo copiar a %s", kViejas[i], kNueva);
        if (copiaFallo) *copiaFallo = true;
        return false;
    }
    return false;
}
bool W3dVitaPuedeDibujar() {
    bool copiaFallo = false;
    if (CompiladorShadersVita(&copiaFallo)) return true;
    if (copiaFallo) {
        const char* m = "libshacccg.suprx esta en ur0:data/, pero vitaGL lo busca en ur0:data/external/ y no se pudo copiar ahi.\n\n"
                        "Copialo con VitaShell a ur0:data/external/libshacccg.suprx.\n\n"
                        "libshacccg.suprx is in ur0:data/ but vitaGL needs it in ur0:data/external/: copy it there with VitaShell.";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Whisk3D", m, NULL);
        return false;
    }
    const char* msg = "Whisk3D necesita el compilador de shaders de la Vita (libshacccg.suprx) y no esta instalado.\n\n"
                      "Se instala una sola vez, con la aplicacion ShaRKBR33D: queda en ur0:data/external/libshacccg.suprx.\n\n"
                      "Whisk3D needs the Vita shader compiler (libshacccg.suprx). Install it once with ShaRKBR33D.";
    w3dLogfE("%s", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Whisk3D", msg, NULL);
    return false;
}

// el panel de atras: SDL lo entrega como otro dispositivo tactil (indirecto). La pantalla es el directo
bool W3dVitaEventoValido(const SDL_Event& e) {
    if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION)
        return SDL_GetTouchDeviceType(e.tfinger.touchId) == SDL_TOUCH_DEVICE_DIRECT;
    return true;
}

// ----------------------------------------------------------------------------------------------------------------
//  LOS BOTONES EN EL EDITOR (fuera del modo juego; con el juego corriendo el control es del juego). Lo que se toca va
//  por la pantalla; los botones son los atajos de siempre y la vista con los sticks:
//    stick izquierdo: orbitar la vista     stick derecho: panear          (zoom: pellizcar la pantalla)
//    cruceta: las flechas                  L: Shift (mantenido)           R: Ctrl (mantenido)
//    cruz: Enter (confirmar)               circulo: Escape (cancelar)     cuadrado: Tab (modo objeto / edicion)
//    triangulo: deshacer                   Select: rehacer
// ----------------------------------------------------------------------------------------------------------------
#include "ViewPorts/ViewPort3D.h"
#include "undo/Undo.h"
extern Viewport3D* Viewport3DActive;
extern SDL_GameController* controller;   // el primer control abierto (controles.cpp): en la Vita, la consola
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
void W3dVitaControles(bool enJuego) {
    struct Boton { SDL_GameControllerButton b; SDL_Scancode tecla; bool antes; };
    // (SDL nombra los botones por la posicion, como en un control de Xbox: A = cruz, B = circulo, X = cuadrado,
    // Y = triangulo)
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

    // ---- la vista: orbitar y panear (en pixeles por segundo, como si se arrastrara el dedo) ----
    Viewport3D* vp = Viewport3DActive;
    if (!vp || dt <= 0.0f) return;
    const float ox = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX));
    const float oy = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY));
    const float px = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX));
    const float py = EjeCurva(SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY));
    restoOX += ox * 300.0f * dt; restoOY += oy * 300.0f * dt;
    restoPX += px * 300.0f * dt; restoPY += py * 300.0f * dt;
    const int odx = (int)restoOX, ody = (int)restoOY, pdx = (int)restoPX, pdy = (int)restoPY;
    restoOX -= odx; restoOY -= ody; restoPX -= pdx; restoPY -= pdy;
    if (odx || ody) { vp->OrbitarFlecha(odx, ody); g_redraw = true; }
    if (pdx || pdy) { vp->PanFlecha(pdx, pdy); g_redraw = true; }
    // (mientras se mantiene un eje la vista se mueve sola: el loop no se queda esperando eventos)
    if (ox != 0.0f || oy != 0.0f || px != 0.0f || py != 0.0f) g_redraw = true;
}

#ifdef W3D_VITA_CAPTURA
// la pantalla tal como se dibujo (antes de mostrarla), a un PPM en ux0:data/Whisk3D. GL lee de abajo hacia arriba
#include <vector>
#include <sys/stat.h>
extern int winW, winH;
static void GuardarCaptura() {
    const int W = winW, H = winH;
    std::vector<unsigned char> px((size_t)W * H * 4);
    w3dEngine::ReadPixelsBegin(0, 0, W, H);
    w3dEngine::ReadPixelsRGBA(0, 0, W, H, &px[0]);
    mkdir("ux0:data/Whisk3D", 0777);
    FILE* f = fopen("ux0:data/Whisk3D/captura.ppm", "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = H - 1; y >= 0; y--)
        for (int x = 0; x < W; x++) fwrite(&px[((size_t)y * W + x) * 4], 1, 3, f);
    fclose(f);
    printf("W3DCAP ux0:data/Whisk3D/captura.ppm\n");
}
#endif

// lo llama main.cpp con cada cuadro dibujado, antes de mostrarlo
void W3dVitaCuadroDibujado() {
#ifdef W3D_VITA_CAPTURA
    static int cuadro = 0;
    if (++cuadro == W3D_VITA_CAPTURA) GuardarCaptura();
#endif
}
