// ============================================================================
//  W3dKeyboard.cpp - teclado en pantalla de Android + pantalla completa automatica (ver W3dKeyboard.h).
// ============================================================================
#include "app/W3dKeyboard.h"
#ifndef W3D_SYMBIAN
#include <SDL2/SDL.h>
#include "ViewPorts/ViewPorts.h"     // viewPortActive / rootViewport
#include "ViewPorts/LayoutInput.h"   // LayoutMaximize / LayoutIsMaximized
#include "ViewPorts/PopUp/PopUpBase.h" // PopUpActive (con un popup no se maximiza nada: el popup se recentra solo)
#include "w3dlog.h"
#include "app/variables.h"         // cfg.keyboardSystem (Ajustes)
#ifdef __ANDROID__
    #include <jni.h>
#endif
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <string.h>
#endif

extern bool g_redraw;

#ifdef __EMSCRIPTEN__
// ============================================================================
//  EL TECLADO DEL NAVEGADOR en un telefono o una tablet (Safari, Chrome de Android). En la web SDL_StartTextInput no
//  muestra nada: el teclado del sistema solo sale al ENFOCAR una caja de texto de la pagina. Se usa una ESCONDIDA: lo
//  que se tipea en ella llega a la app como el texto del teclado (SDL_TEXTINPUT), borrar como Backspace y el Enter
//  como Enter. Safari solo abre el teclado si la caja se enfoca DENTRO de un toque: si el primer intento no lo abre, el
//  proximo toque en la pantalla la enfoca, mientras la app lo siga pidiendo (W3dKeyboardWebWatch lo suelta)
// ============================================================================
extern "C" EMSCRIPTEN_KEEPALIVE void W3dWebKeyboardText(const char* utf8) {
    if (!utf8 || !*utf8) return;
    // (de a pedazos: el texto de un evento SDL tiene lugar para 31 bytes; sin cortar una letra UTF-8 al medio)
    const size_t n = strlen(utf8);
    size_t i = 0;
    while (i < n) {
        size_t k = n - i < 31 ? n - i : 31;
        while (k > 0 && i + k < n && (((unsigned char)utf8[i + k]) & 0xC0) == 0x80) k--;
        if (k == 0) k = 1;
        SDL_Event e; SDL_zero(e); e.type = SDL_TEXTINPUT;
        memcpy(e.text.text, utf8 + i, k); e.text.text[k] = 0;
        SDL_PushEvent(&e);
        i += k;
    }
}
extern "C" EMSCRIPTEN_KEEPALIVE void W3dWebKeyboardKey(int enter) {
    SDL_Event e; SDL_zero(e);
    e.type = SDL_KEYDOWN; e.key.state = SDL_PRESSED;
    e.key.keysym.sym = enter ? SDLK_RETURN : SDLK_BACKSPACE;
    e.key.keysym.scancode = enter ? SDL_SCANCODE_RETURN : SDL_SCANCODE_BACKSPACE;
    SDL_PushEvent(&e);
    e.type = SDL_KEYUP; e.key.state = SDL_RELEASED;
    SDL_PushEvent(&e);
}
EM_JS(void, W3dWebKeyboardRequest, (int open), {
  var inp = Module.w3dKeyboardInput;
  if (!inp) {
    inp = document.createElement('input');
    inp.type = 'text';
    inp.setAttribute('autocapitalize', 'off'); inp.setAttribute('autocomplete', 'off');
    inp.setAttribute('autocorrect', 'off'); inp.setAttribute('spellcheck', 'false');
    // (16px: con menos Safari agranda la pagina al enfocarla)
    inp.style.cssText = 'position:fixed;left:0;bottom:0;width:2px;height:2px;opacity:0;font-size:16px;border:0;padding:0;margin:0;';
    inp.value = ' ';
    document.body.appendChild(inp);
    Module.w3dKeyboardInput = inp;
    var restore = function() { inp.value = ' '; try { inp.setSelectionRange(1, 1); } catch (e) {} };
    // lo tipeado: lo que hay despues del espacio de guardia; si el espacio se borro, fue un Backspace
    inp.addEventListener('input', function(e) {
      var v = inp.value;
      if (v.length === 0) Module.ccall('W3dWebKeyboardKey', null, ['number'], [0]);
      else if (v.length > 1) Module.ccall('W3dWebKeyboardText', null, ['string'], [v.substring(1)]);
      restore();
      e.stopPropagation();
    });
    // (las teclas de la caja no le llegan tambien a SDL: si no, un teclado fisico escribiria dos veces)
    inp.addEventListener('keydown', function(e) {
      if (e.key === 'Enter') { Module.ccall('W3dWebKeyboardKey', null, ['number'], [1]); e.preventDefault(); }
      else if (e.key === 'Backspace' && inp.value === ' ') { Module.ccall('W3dWebKeyboardKey', null, ['number'], [0]); e.preventDefault(); }
      e.stopPropagation();
    });
    inp.addEventListener('keyup', function(e) { e.stopPropagation(); });
    inp.addEventListener('keypress', function(e) { e.stopPropagation(); });
    var focus = function() { if (Module.w3dKeyboardWants && document.activeElement !== inp) { restore(); inp.focus(); } };
    document.addEventListener('touchend', focus, true);
    document.addEventListener('mouseup', focus, true);
  }
  Module.w3dKeyboardWants = !!open;
  if (open) { if (document.activeElement !== inp) { inp.value = ' '; try { inp.focus(); inp.setSelectionRange(1, 1); } catch (e) {} } }
  else if (document.activeElement === inp) inp.blur();
});
static bool gWebKeyboardRequest = false;
// (solo con pantalla tactil: en una PC con teclado fisico la caja escondida no hace falta)
static bool WebWithTouch() {
    return EM_ASM_INT({ return (window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0; }) != 0;
}
void W3dKeyboardWebWatch(bool hasField) {
    if (gWebKeyboardRequest && !hasField) { W3dWebKeyboardRequest(0); gWebKeyboardRequest = false; }
}
#else
void W3dKeyboardWebWatch(bool) {}
#endif

static int  gWidthRef = 0;            // ancho de la ventana con el que se midio el alto completo (rotar = de nuevo)
static int  gCompleteHeight = 0;      // alto de la ventana SIN teclado
static bool gVisible = false;
static bool gMaximized = false;    // el layout se maximizo POR el teclado (y se restaura al cerrarlo)
static bool gFullAuto = false;     // la app se puso sola en pantalla completa (no hay notch)
static bool gFullExited = false;   // se salio de pantalla completa para mostrar el teclado
static SDL_Window* gWindow = NULL;

bool W3dKeyboardVisible() { return gVisible; }
bool W3dSystemKeyboard() { return cfg.keyboardSystem; }

// ---- LO QUE TAPA EL TECLADO DEL SISTEMA sin achicar la ventana (ver W3dKeyboard.h) ----
// Java lo avisa desde el hilo de la interfaz de Android (nativeKeyboardCovered): se guarda y se despierta al loop con un
// evento propio, que reparte la ventana de nuevo (main.cpp)
static volatile int gCoveredJava = 0;
static Uint32 gEventKeyboard = 0;   // (SDL_RegisterEvents al arrancar; 0 = todavia no)
static int gHeightIDE = 0;
void (*g_windowRearrangeHook)() = 0;
static void CoveredKeyboardNotice(int px) {
    gCoveredJava = px > 0 ? px : 0;
    if (gEventKeyboard) { SDL_Event e; SDL_zero(e); e.type = gEventKeyboard; SDL_PushEvent(&e); }
}
#ifdef __ANDROID__
// (el mismo aviso para las dos actividades: la de siempre y la de Android 2.3)
extern "C" JNIEXPORT void JNICALL Java_com_whisk3d_Whisk3DActivity_nativeKeyboardCovered(JNIEnv*, jclass, jint px) {
    CoveredKeyboardNotice((int)px);
}
extern "C" JNIEXPORT void JNICALL Java_com_whisk3d_viejo_Whisk3DActivity_nativeKeyboardCovered(JNIEnv*, jclass, jint px) {
    CoveredKeyboardNotice((int)px);
}
#endif
bool W3dKeyboardIsEvent(const SDL_Event& e) { return gEventKeyboard != 0 && e.type == gEventKeyboard; }
static int gCoveredForced = -1;   // (el harness: un teclado del sistema que tapa, sin Android)
void W3dKeyboardCoveredForce(int px) { gCoveredForced = px; }
int W3dKeyboardCovered() {
    if (gCoveredForced >= 0) return gCoveredForced;
#ifdef __ANDROID__
    // (solo con un campo escribiendo: la barra de navegacion o un cambio de layout no son el teclado)
    if (SDL_IsTextInputActive()) return gCoveredJava;
#endif
    return 0;
}
void W3dKeyboardIDEHeight(int h) { gHeightIDE = h > 0 ? h : 0; }
int W3dKeyboardIDECurrentHeight() { return gHeightIDE; }
void W3dKeyboardRearrange() { if (g_windowRearrangeHook) g_windowRearrangeHook(); g_redraw = true; }

void W3dKeyboardOpen() {
#if defined(W3D_PSP)
    // (la PSP: el QWERTY de Whisk3D, que abre w3d_psp.cpp cuando un campo toma el foco. El teclado del sistema que abre
    // SDL dibuja por su cuenta en el GE y no deja pasar la ñ)
#else
#ifdef __ANDROID__
    if (gFullAuto && !gFullExited && gWindow) { SDL_SetWindowFullscreen(gWindow, 0); gFullExited = true; }
#endif
#ifdef __EMSCRIPTEN__
    if (WebWithTouch()) { W3dWebKeyboardRequest(1); gWebKeyboardRequest = true; }   // (el del navegador: ver arriba)
#endif
    SDL_StartTextInput();
#endif
}

void W3dKeyboardResize(int w, int h, int covered) {
    // el teclado del sistema esta abierto si ACHICO la ventana (Android con adjustResize: en la PC una ventana mas baja
    // es el usuario) o si la TAPA sin achicarla (lo que avisa Java, W3dKeyboardCovered)
    bool shrunk = false;
#ifdef __ANDROID__
    if (w != gWidthRef || gCompleteHeight <= 0) {   // primera medida o se roto la pantalla
        if (gMaximized && LayoutIsMaximized()) LayoutMaximize();
        gMaximized = false; gVisible = false;
        gWidthRef = w; gCompleteHeight = h;
    } else {
        if (h > gCompleteHeight) gCompleteHeight = h;
        shrunk = h < (int)(gCompleteHeight * 0.8f);   // la barra de estado sola no llega al 20%
    }
#else
    (void)w; (void)h;
#endif
    const bool visible = shrunk || covered > 0;
    if (visible && !gVisible) {
        gVisible = true;
        if (!PopUpActive && !LayoutIsMaximized() && viewPortActive && viewPortActive != rootViewport && viewPortActive->isLeaf()) {
            LayoutMaximize();   // el viewport que pidio el teclado ocupa la parte visible; los otros se ocultan
            gMaximized = true;
        }
        g_redraw = true;
    } else if (!visible && gVisible) {
        gVisible = false;
        if (gMaximized && LayoutIsMaximized()) LayoutMaximize();   // vuelve el layout de antes
        gMaximized = false;
        if (gFullExited && gWindow) { gFullExited = false; SDL_SetWindowFullscreen(gWindow, SDL_WINDOW_FULLSCREEN_DESKTOP); }
        g_redraw = true;
    }
}

#ifdef __ANDROID__
// Whisk3DActivity.hasNotch(): la pantalla tiene recorte (notch / camara)?
static bool AndroidHasNotch() {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject act = (jobject)SDL_AndroidGetActivity();
    if (!env || !act) return true;
    bool r = true;
    jclass cls = env->GetObjectClass(act);
    jmethodID m = cls ? env->GetMethodID(cls, "hasNotch", "()Z") : NULL;
    if (m) r = env->CallBooleanMethod(act, m) ? true : false;
    if (env->ExceptionCheck()) { env->ExceptionClear(); r = true; }
    if (cls) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(act);
    return r;
}
#endif

void W3dFullscreenAuto(SDL_Window* win) {
    gWindow = win;
    if (!gEventKeyboard) { const Uint32 t = SDL_RegisterEvents(1); if (t != (Uint32)-1) gEventKeyboard = t; }
#ifdef __ANDROID__
    const bool notch = AndroidHasNotch();
    w3dLogf("[pantalla] notch=%s -> %s", notch ? "si" : "no", notch ? "con la barra de estado" : "pantalla completa");
    if (!notch && win) { SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP); gFullAuto = true; }
#endif
}
#else
// Symbian (N95): no hay teclado en pantalla ni ventana SDL; el teclado es fisico. Los campos de texto del
// editor (IDE, Properties, popups) igual llaman a W3dKeyboardOpen: sin estos stubs el link falla (L6218E).
bool W3dKeyboardVisible() { return false; }
bool W3dSystemKeyboard() { return false; }   // (no hay teclado del sistema en pantalla: el de Whisk3D)
void W3dKeyboardOpen() {}
void W3dKeyboardResize(int, int, int) {}
void W3dKeyboardCoveredForce(int) {}
void W3dFullscreenAuto(SDL_Window*) {}
int  W3dKeyboardCovered() { return 0; }
bool W3dKeyboardIsEvent(const SDL_Event&) { return false; }
void W3dKeyboardIDEHeight(int) {}
int  W3dKeyboardIDECurrentHeight() { return 0; }
void (*g_windowRearrangeHook)() = 0;
void W3dKeyboardRearrange() {}
void W3dKeyboardWebWatch(bool) {}
#endif // !W3D_SYMBIAN
