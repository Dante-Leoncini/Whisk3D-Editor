#ifndef W3DSDLMINIMO_H
#define W3DSDLMINIMO_H

// ---------------------------------------------------------------------------------------------------------------
//  EL PEDAZO DE SDL QUE NECESITA controles.cpp, para las plataformas que NO tienen SDL (Symbian).
//
//  El tactil del editor (tap / arrastre / pinch / doble tap de 2 dedos / pulsacion larga / scroll de paneles) vive
//  en controles.cpp y habla SDL_Event: es lo que llega en PC, Android y la web. En vez de reescribirlo para el
//  telefono, el Core sigue los dedos y arma los MISMOS eventos (libs/Whisk3DCore/input/W3dTactil.h, main/app/W3dTactilEditor.cpp) y
//  entran por la misma puerta
//  (InputUsuarioSDL3). Una mejora del tactil en Android llega sola al N8 y al reves.
//
//  Solo los tipos y constantes que usa controles.cpp, con los valores de SDL2. Las funciones las implementa cada
//  plataforma (Symbian: platform/symbian/src/w3dsdlminimo.cpp).
// ---------------------------------------------------------------------------------------------------------------

typedef unsigned char  Uint8;
typedef unsigned short Uint16;
typedef unsigned int   Uint32;
typedef signed int     Sint32;
typedef long long      Sint64;

#define SDL_MAJOR_VERSION 2
#define SDL_VERSION_ATLEAST(X, Y, Z) 0

typedef Sint32 SDL_Keycode;
typedef Sint64 SDL_TouchID;
typedef Sint64 SDL_FingerID;

// ---- eventos (los numeros de SDL2) ----
enum {
    SDL_WINDOWEVENT = 0x200,
    SDL_KEYDOWN = 0x300, SDL_KEYUP, SDL_TEXTEDITING, SDL_TEXTINPUT,
    SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL,
    SDL_CONTROLLERDEVICEADDED = 0x653, SDL_CONTROLLERDEVICEREMOVED,
    SDL_FINGERDOWN = 0x700, SDL_FINGERUP, SDL_FINGERMOTION
};
enum { SDL_WINDOWEVENT_FOCUS_LOST = 13 };
enum { SDL_RELEASED = 0, SDL_PRESSED = 1 };

#define SDL_TOUCH_MOUSEID ((Uint32)-1)
#define SDL_BUTTON_LEFT   1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT  3
#define SDL_BUTTON_LMASK  (1u << (SDL_BUTTON_LEFT - 1))

// ---- teclas: los imprimibles valen su ASCII; el resto, el scancode con el bit 30 (como SDL2) ----
#define W3D_SDLK_SC(x) ((SDL_Keycode)((x) | (1 << 30)))
enum {
    SDLK_UNKNOWN = 0, SDLK_BACKSPACE = 8, SDLK_TAB = 9, SDLK_RETURN = 13, SDLK_ESCAPE = 27, SDLK_SPACE = ' ',
    SDLK_0 = '0', SDLK_a = 'a', SDLK_g = 'g', SDLK_m = 'm', SDLK_p = 'p', SDLK_s = 's', SDLK_t = 't', SDLK_w = 'w',
    SDLK_x = 'x', SDLK_y = 'y', SDLK_z = 'z', SDLK_DELETE = 127
};
#define SDLK_F1        W3D_SDLK_SC(58)
#define SDLK_F2        W3D_SDLK_SC(59)
#define SDLK_F3        W3D_SDLK_SC(60)
#define SDLK_F4        W3D_SDLK_SC(61)
#define SDLK_F5        W3D_SDLK_SC(62)
#define SDLK_F6        W3D_SDLK_SC(63)
#define SDLK_F7        W3D_SDLK_SC(64)
#define SDLK_F8        W3D_SDLK_SC(65)
#define SDLK_F9        W3D_SDLK_SC(66)
#define SDLK_F10       W3D_SDLK_SC(67)
#define SDLK_F11       W3D_SDLK_SC(68)
#define SDLK_F12       W3D_SDLK_SC(69)
#define SDLK_HOME      W3D_SDLK_SC(74)
#define SDLK_PAGEUP    W3D_SDLK_SC(75)
#define SDLK_END       W3D_SDLK_SC(77)
#define SDLK_PAGEDOWN  W3D_SDLK_SC(78)
#define SDLK_RIGHT     W3D_SDLK_SC(79)
#define SDLK_LEFT      W3D_SDLK_SC(80)
#define SDLK_DOWN      W3D_SDLK_SC(81)
#define SDLK_UP        W3D_SDLK_SC(82)
#define SDLK_KP_ENTER  W3D_SDLK_SC(88)
#define SDLK_KP_1      W3D_SDLK_SC(89)
#define SDLK_KP_2      W3D_SDLK_SC(90)
#define SDLK_KP_3      W3D_SDLK_SC(91)
#define SDLK_KP_4      W3D_SDLK_SC(92)
#define SDLK_KP_5      W3D_SDLK_SC(93)
#define SDLK_KP_6      W3D_SDLK_SC(94)
#define SDLK_KP_7      W3D_SDLK_SC(95)
#define SDLK_KP_8      W3D_SDLK_SC(96)
#define SDLK_KP_9      W3D_SDLK_SC(97)
#define SDLK_KP_0      W3D_SDLK_SC(98)
#define SDLK_KP_PERIOD W3D_SDLK_SC(99)
#define SDLK_LCTRL     W3D_SDLK_SC(224)
#define SDLK_LSHIFT    W3D_SDLK_SC(225)
#define SDLK_LALT      W3D_SDLK_SC(226)
#define SDLK_RCTRL     W3D_SDLK_SC(228)
#define SDLK_RSHIFT    W3D_SDLK_SC(229)

enum {
    KMOD_NONE = 0, KMOD_LSHIFT = 0x1, KMOD_RSHIFT = 0x2, KMOD_LCTRL = 0x40, KMOD_RCTRL = 0x80,
    KMOD_LALT = 0x100, KMOD_RALT = 0x200,
    KMOD_SHIFT = KMOD_LSHIFT | KMOD_RSHIFT, KMOD_CTRL = KMOD_LCTRL | KMOD_RCTRL, KMOD_ALT = KMOD_LALT | KMOD_RALT
};

// ---- los structs de evento (solo los campos que se leen) ----
struct SDL_Keysym { SDL_Keycode sym; Uint16 mod; };
struct SDL_WindowEvent { Uint32 type; Uint32 timestamp; Uint8 event; Sint32 data1, data2; };
struct SDL_KeyboardEvent { Uint32 type; Uint32 timestamp; Uint8 state; Uint8 repeat; SDL_Keysym keysym; };
struct SDL_TextInputEvent { Uint32 type; Uint32 timestamp; char text[32]; };
struct SDL_MouseMotionEvent { Uint32 type; Uint32 timestamp; Uint32 which; Uint32 state; Sint32 x, y, xrel, yrel; };
struct SDL_MouseButtonEvent { Uint32 type; Uint32 timestamp; Uint32 which; Uint8 button, state, clicks; Sint32 x, y; };
struct SDL_MouseWheelEvent { Uint32 type; Uint32 timestamp; Uint32 which; Sint32 x, y; };
struct SDL_ControllerDeviceEvent { Uint32 type; Uint32 timestamp; Sint32 which; };
struct SDL_TouchFingerEvent {
    Uint32 type; Uint32 timestamp; SDL_TouchID touchId; SDL_FingerID fingerId;
    float x, y, dx, dy, pressure;   // x/y NORMALIZADOS 0..1 (como SDL)
};
union SDL_Event {
    Uint32 type;
    SDL_WindowEvent window;
    SDL_KeyboardEvent key;
    SDL_TextInputEvent text;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    SDL_ControllerDeviceEvent cdevice;
    SDL_TouchFingerEvent tfinger;
    Uint8 padding[56];
};

// ---- los MANDOS (SDL_GameController): el telefono pone los joysticks de bthid en los mismos lugares (Gamepad.cpp) ----
enum {
    SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_CONTROLLER_AXIS_MAX
};
enum {
    SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_GUIDE, SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_MAX
};

// ---- la "ventana" (una sola: la pantalla del telefono) ----
struct SDL_Window;
extern SDL_Window* window;
#define SDL_WINDOW_FULLSCREEN_DESKTOP 0x1001

Uint32 SDL_GetTicks();
Uint32 SDL_GetMouseState(int* x, int* y);
Uint16 SDL_GetModState();
SDL_Window* SDL_GetMouseFocus();
void SDL_GetWindowSize(SDL_Window* w, int* ancho, int* alto);
int SDL_SetWindowFullscreen(SDL_Window* w, Uint32 flags);

#endif
