#ifndef W3D_KEYBOARD_H
#define W3D_KEYBOARD_H
// ============================================================================
//  W3dKeyboard.h - el TECLADO EN PANTALLA de Android (y la pantalla completa automatica).
//    - W3dKeyboardOpen(): levanta el teclado del sistema (SDL_StartTextInput). Todos los campos de texto pasan por
//      aca en vez de llamar a SDL directo.
//    - Cuando el teclado aparece, Android achica la ventana (adjustResize): el viewport que lo pidio (el IDE, el
//      panel de propiedades...) se MAXIMIZA en la parte visible y los demas se ocultan; al cerrarse vuelve el layout.
//    - Pantalla completa automatica: sin notch la app ocupa toda la pantalla (la barra de arriba come espacio); con
//      notch se deja la barra. En pantalla completa Android no achica la ventana con el teclado, asi que al abrirlo
//      se sale de pantalla completa y se vuelve a entrar al cerrarlo.
//  En PC / web es solo SDL_StartTextInput (el teclado es fisico).
// ============================================================================
struct SDL_Window;
void W3dKeyboardOpen();
// al repartir la ventana (main.cpp), ANTES de redimensionar el layout: el alto de la VENTANA y lo que tapa el teclado
void W3dKeyboardResize(int w, int h, int covered);
bool W3dKeyboardVisible();
// Ajustes > "Use system keyboard" (cfg.keyboardSystem): true = el teclado del sistema; false = los de Whisk3D. Lo
// consultan KeyboardTextOpen / KeyboardNumberOpen (ViewPorts/PopUp/NumPad.h), por donde pasan todos los campos
bool W3dSystemKeyboard();
// Android: consulta si la pantalla tiene notch y, si no tiene, pone la ventana en pantalla completa
void W3dFullscreenAuto(SDL_Window* win);
// ---- EL AREA UTIL CON UN TECLADO ABIERTO ----
// Lo que el teclado del SISTEMA tapa abajo de la ventana SIN achicarla (Android en pantalla completa, el Galaxy Ace: SDL
// 2.0.7 no sale de pantalla completa y ahi Android no achica la ventana). Lo mide Java (Whisk3DActivity, lo que el
// teclado le tapa a la superficie de SDL) y lo avisa con un evento (W3dKeyboardIsEvent). 0 = no tapa nada (la ventana
// se achico, o no hay teclado). La ventana se reparte con esto: ver W3dVentanaReacomodar en main.cpp
union SDL_Event;
int  W3dKeyboardCovered();
void W3dKeyboardCoveredForce(int px);   // (el harness: -1 = el de verdad)
bool W3dKeyboardIsEvent(const SDL_Event& e);
// el alto del teclado de WHISK3D abierto para el IDE (0 = cerrado): el IDE se maximiza y ocupa lo de arriba
void W3dKeyboardIDEHeight(int h);
int  W3dKeyboardIDECurrentHeight();
// volver a repartir la ventana (el layout y el area de los popups) con lo que tapan los teclados ahora. Lo pone main.cpp
extern void (*g_windowRearrangeHook)();
void W3dKeyboardRearrange();
// (la web en un telefono) en cada cuadro: sin nada que escriba (ni un campo ni el IDE), el teclado del navegador se va
void W3dKeyboardWebWatch(bool hasField);
#endif // W3D_KEYBOARD_H
