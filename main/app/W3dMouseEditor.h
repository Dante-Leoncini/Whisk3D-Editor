#ifndef W3D_MOUSE_EDITOR_H
#define W3D_MOUSE_EDITOR_H

// ---------------------------------------------------------------------------------------------------------------
//  EL MOUSE DE LAS PLATAFORMAS SIN SDL (Symbian: el mouse Bluetooth del driver bthid-s60, que manda el apretar y el
//  soltar de verdad). Se vuelve el MISMO evento de mouse que SDL entrega en la PC y entra por la misma puerta
//  (InputUserSDL3, controls.cpp): click derecho = menu contextual, arrastrar = seleccion de caja, pintar, rueda =
//  zoom, el boton del medio arrastrado = orbitar. Lo que mejore en la PC, mejora en el telefono.
//
//  BOTON DEL MEDIO EMULADO: muchos mouses Bluetooth no lo mandan. Izquierdo y derecho apretados casi juntos (menos de
//  kMouseChordMs) son el del medio; por eso el apretar de cada uno espera ese tiempo antes de salir (W3dMouseTick).
//
//  La plataforma informa el movimiento (relativo, como lo da el driver), los botones (1 izq, 2 medio, 3 der) y la
//  rueda; el puntero lo lleva la plataforma (en Symbian, el cursor virtual: mouseX / mouseY).
// ---------------------------------------------------------------------------------------------------------------

enum { kMouseChordMs = 80 };

// el puntero se movio a (x, y) (dx, dy desde donde estaba); la plataforma ya lo puso ahi
void W3dMouseMoved(int x, int y, int dx, int dy);
// un boton (1 izq, 2 medio, 3 der) bajo o subio, con el puntero en (x, y)
void W3dMouseButton(int button, bool down, int x, int y);
// la rueda (positivo = para arriba / adelante)
void W3dMouseWheel(int steps, int x, int y);
// una vez por cuadro: suelta el apretar que esperaba por si era el boton del medio
void W3dMouseTick();
// el evento que esta procesando el editor vino de este mouse
bool W3dEventMouse();
// hay un boton apretado (o uno que espera el acorde del boton del medio)
bool W3dAnyMousePressed();
// la plataforma pone el puntero en (x, y) (el envolver del cursor dentro del viewport durante un mover/rotar/escalar,
// como SDL_WarpMouseInWindow en la PC). NULL = la plataforma no puede
extern void (*W3dMouseTeleport)(int x, int y);

// ---- EL TECLADO de las plataformas sin SDL (Symbian: el teclado Bluetooth, que manda los codigos de uso USB HID). Igual
//      que el mouse: se vuelve el evento de teclado de SDL y entra por InputUserSDL3 -> los mismos atajos que la PC
//      (E extruir, Shift+R, Ctrl+click...), sin una lista aparte que se queda atras.
// una tecla bajo o subio (aUse = codigo de uso HID: a=4, Enter=40, flechas=79..82, Ctrl izq=224...). Las imprimibles,
// al bajar, ademas mandan su texto (SDL_TEXTINPUT, distribucion de EEUU): los campos de texto y los valores exactos
void W3dKeyboardKeyHID(int aUse, bool down);
// el evento que esta procesando el editor vino de este teclado
bool W3dEventKeyboard();

#endif
