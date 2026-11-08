// ============================================================================
//  W3dTeclado.cpp - teclado en pantalla de Android + pantalla completa automatica (ver W3dTeclado.h).
// ============================================================================
#include "app/W3dTeclado.h"
#ifndef W3D_SYMBIAN
#include <SDL2/SDL.h>
#include "ViewPorts/ViewPorts.h"     // viewPortActive / rootViewport
#include "ViewPorts/LayoutInput.h"   // LayoutMaximizar / LayoutEstaMaximizado
#include "ViewPorts/PopUp/PopUpBase.h" // PopUpActive (con un popup no se maximiza nada: el popup se recentra solo)
#include "w3dlog.h"
#ifdef __ANDROID__
    #include <jni.h>
#endif

extern bool g_redraw;

static int  gAnchoRef = 0;          // ancho de la ventana con el que se midio el alto completo (rotar = de nuevo)
static int  gAltoCompleto = 0;      // alto de la ventana SIN teclado
static bool gVisible = false;
static bool gMaximizado = false;    // el layout se maximizo POR el teclado (y se restaura al cerrarlo)
static bool gFullAuto = false;      // la app se puso sola en pantalla completa (no hay notch)
static bool gSalioDeFull = false;   // se salio de pantalla completa para mostrar el teclado
static SDL_Window* gVentana = NULL;

bool W3dTecladoVisible() { return gVisible; }

void W3dTecladoAbrir() {
#if defined(W3D_PSP)
    // (la PSP: el QWERTY de Whisk3D, que abre w3d_psp.cpp cuando un campo toma el foco. El teclado del sistema que abre
    // SDL dibuja por su cuenta en el GE y no deja pasar la ñ)
#else
#ifdef __ANDROID__
    if (gFullAuto && !gSalioDeFull && gVentana) { SDL_SetWindowFullscreen(gVentana, 0); gSalioDeFull = true; }
#endif
    SDL_StartTextInput();
#endif
}

void W3dTecladoRedimension(int w, int h) {
#ifdef __ANDROID__
    if (w != gAnchoRef || gAltoCompleto <= 0) {   // primera medida o se roto la pantalla
        if (gMaximizado && LayoutEstaMaximizado()) LayoutMaximizar();
        gMaximizado = false; gVisible = false;
        gAnchoRef = w; gAltoCompleto = h;
        return;
    }
    if (h > gAltoCompleto) gAltoCompleto = h;
    const bool visible = h < (int)(gAltoCompleto * 0.8f);   // la barra de estado sola no llega al 20%
    if (visible && !gVisible) {
        gVisible = true;
        if (!PopUpActive && !LayoutEstaMaximizado() && viewPortActive && viewPortActive != rootViewport && viewPortActive->isLeaf()) {
            LayoutMaximizar();   // el viewport que pidio el teclado ocupa la parte visible; los otros se ocultan
            gMaximizado = true;
        }
        g_redraw = true;
    } else if (!visible && gVisible) {
        gVisible = false;
        if (gMaximizado && LayoutEstaMaximizado()) LayoutMaximizar();   // vuelve el layout de antes
        gMaximizado = false;
        if (gSalioDeFull && gVentana) { gSalioDeFull = false; SDL_SetWindowFullscreen(gVentana, SDL_WINDOW_FULLSCREEN_DESKTOP); }
        g_redraw = true;
    }
#else
    (void)w; (void)h;
#endif
}

#ifdef __ANDROID__
// Whisk3DActivity.tieneNotch(): la pantalla tiene recorte (notch / camara)?
static bool AndroidTieneNotch() {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject act = (jobject)SDL_AndroidGetActivity();
    if (!env || !act) return true;
    bool r = true;
    jclass cls = env->GetObjectClass(act);
    jmethodID m = cls ? env->GetMethodID(cls, "tieneNotch", "()Z") : NULL;
    if (m) r = env->CallBooleanMethod(act, m) ? true : false;
    if (env->ExceptionCheck()) { env->ExceptionClear(); r = true; }
    if (cls) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(act);
    return r;
}
#endif

void W3dPantallaCompletaAuto(SDL_Window* win) {
    gVentana = win;
#ifdef __ANDROID__
    const bool notch = AndroidTieneNotch();
    w3dLogf("[pantalla] notch=%s -> %s", notch ? "si" : "no", notch ? "con la barra de estado" : "pantalla completa");
    if (!notch && win) { SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP); gFullAuto = true; }
#endif
}
#else
// Symbian (N95): no hay teclado en pantalla ni ventana SDL; el teclado es fisico. Los campos de texto del
// editor (IDE, Properties, popups) igual llaman a W3dTecladoAbrir: sin estos stubs el link falla (L6218E).
bool W3dTecladoVisible() { return false; }
void W3dTecladoAbrir() {}
void W3dTecladoRedimension(int, int) {}
void W3dPantallaCompletaAuto(SDL_Window*) {}
#endif // !W3D_SYMBIAN
