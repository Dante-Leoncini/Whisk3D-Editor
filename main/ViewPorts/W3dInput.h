#ifndef W3DINPUT_H
#define W3DINPUT_H

// Teclado y mouse PROPIOS del editor: un viewport no tiene por que saber que existe SDL.
//
// Las 4 firmas de input (event_key_down / event_key_up / event_mouse_wheel / mouse_button_up) tomaban SDL_Event.
// Ese unico tipo las ataba a SDL, asi que en Symbian -que no tiene SDL- se compilaban AFUERA: ni una tecla, ni la
// rueda, ni el soltar del boton llegaban jamas a un viewport. De ahi salio toda la escalera de casos especiales del
// telefono, que ademas mandaba los comandos al viewport equivocado (apretabas una tecla en el timeline y se movia el
// modelo). Con el evento propio los 4 metodos compilan en las dos plataformas y el ruteo por viewport activo es UNO
// SOLO, el mismo que ya andaba en PC.
//
// Cada plataforma traduce lo suyo a esto:
//   PC       -> main/controles.cpp                     (desde SDL_Event)
//   Symbian  -> platform/symbian/src/Whisk3DContainer.cpp (desde TKeyEvent)
//
// Los imprimibles VALEN SU ASCII (W3dK_G == 'g'): se leen igual que se tipean, y en PC la traduccion es directa
// porque SDLK_g ya es 'g'. Los que no tienen ASCII arrancan en 0x101.

enum W3dTecla {
    W3dK_NADA = 0,

    // --- imprimibles: su propio ASCII ---
    W3dK_SPACE = ' ',
    W3dK_0 = '0', W3dK_1 = '1', W3dK_2 = '2', W3dK_3 = '3', W3dK_4 = '4',
    W3dK_5 = '5', W3dK_6 = '6', W3dK_7 = '7', W3dK_8 = '8', W3dK_9 = '9',
    W3dK_A = 'a', W3dK_B = 'b', W3dK_C = 'c', W3dK_D = 'd', W3dK_E = 'e', W3dK_F = 'f',
    W3dK_G = 'g', W3dK_H = 'h', W3dK_I = 'i', W3dK_J = 'j', W3dK_K = 'k', W3dK_L = 'l',
    W3dK_M = 'm', W3dK_N = 'n', W3dK_O = 'o', W3dK_P = 'p', W3dK_Q = 'q', W3dK_R = 'r',
    W3dK_S = 's', W3dK_T = 't', W3dK_U = 'u', W3dK_V = 'v', W3dK_W = 'w', W3dK_X = 'x',
    W3dK_Y = 'y', W3dK_Z = 'z',

    // --- sin ASCII ---
    W3dK_RETURN = 0x101, W3dK_ESCAPE, W3dK_BACKSPACE, W3dK_DELETE, W3dK_TAB,
    W3dK_HOME, W3dK_END,
    W3dK_LEFT, W3dK_RIGHT, W3dK_UP, W3dK_DOWN,
    W3dK_LSHIFT, W3dK_LCTRL, W3dK_LALT,

    // --- teclado numerico (en el telefono, el keypad ES el teclado) ---
    W3dK_KP_0 = 0x120, W3dK_KP_1, W3dK_KP_2, W3dK_KP_3, W3dK_KP_4,
    W3dK_KP_5, W3dK_KP_6, W3dK_KP_7, W3dK_KP_8, W3dK_KP_9,
    W3dK_KP_ENTER, W3dK_KP_PERIOD
};

// Botones del mouse. En el telefono no hay mouse fisico: el cursor virtual manda estos mismos, asi que el "ok" es un
// click de verdad (down + up) y mantenerlo apretado es un arrastre, sin nada especial del otro lado.
enum W3dBotonMouse { W3dMB_NINGUNO = 0, W3dMB_IZQ = 1, W3dMB_MEDIO = 2, W3dMB_DER = 3 };

#endif
