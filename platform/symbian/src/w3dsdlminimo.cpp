// Las funciones de main/app/W3dSdlMinimo.h en el telefono: lo poco de SDL que lee controles.cpp (el reloj, el mouse
// -que en el telefono es el cursor/el dedo-, los modificadores y el tamaño de la pantalla).
#include <e32std.h>
#include <hal.h>
#include "app/W3dSdlMinimo.h"
#include "app/variables.h"   // winW/winH, mouseX/mouseY

extern bool LShiftPressed;
extern bool LCtrlPressed;
extern bool LAltPressed;

// la unica "ventana": tiene que ser distinta de NULL (controles.cpp pregunta if (window) antes de medirla)
static int gVentanaTelefono = 0;
SDL_Window* window = reinterpret_cast<SDL_Window*>(&gVentanaTelefono);

// milisegundos: el tick del nanokernel no siempre es de 1 ms (el emulador va a 5 ms)
Uint32 SDL_GetTicks() {
    static TInt periodoUs = 0;
    if (periodoUs <= 0) {
        if (HAL::Get(HAL::ENanoTickPeriod, periodoUs) != KErrNone || periodoUs <= 0) periodoUs = 1000;
    }
    return (Uint32)(((TUint64)User::NTickCount() * (TUint64)periodoUs) / 1000);
}

Uint32 SDL_GetMouseState(int* x, int* y) {
    if (x) *x = mouseX;
    if (y) *y = mouseY;
    return 0;
}

Uint16 SDL_GetModState() {
    return (Uint16)((LShiftPressed ? KMOD_LSHIFT : 0) | (LCtrlPressed ? KMOD_LCTRL : 0) | (LAltPressed ? KMOD_LALT : 0));
}

SDL_Window* SDL_GetMouseFocus() { return window; }

void SDL_GetWindowSize(SDL_Window*, int* ancho, int* alto) {
    if (ancho) *ancho = winW;
    if (alto) *alto = winH;
}

int SDL_SetWindowFullscreen(SDL_Window*, Uint32) { return 0; }   // (la app ya ocupa toda la pantalla)
