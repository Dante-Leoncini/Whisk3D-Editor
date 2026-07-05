#pragma once
// ============================================================================
//  Cursor VIRTUAL de Symbian.
//
//  El telefono no tiene mouse fisico: la posicion la mueven los handlers de
//  teclado / HID bluetooth, y se dibuja un sprite encima de todo. Es una cosa
//  de la PLATAFORMA (Symbian sin mouse), no del editor Whisk3D — por eso vive
//  en su propio archivo y no en Whisk3D.cpp.
// ============================================================================

// la posicion del cursor (mouseX/mouseY/mouseVisible) son globales COMPARTIDOS
// con el resto del editor: viven en variables.cpp (los lee/escribe tambien el
// ruteo de input). Aca solo se dibuja y se mueve.
#include "variables.h"

// sprite del cursor: mouse.png acolchada a una celda 32x32 (point sprite)
void W3dMouseBuildTex(const unsigned char* aRGBA, int aW, int aH);
unsigned int W3dMouseTex();

// mover el cursor virtual (el "warp" del ruteo compartido LayoutInput)
void W3dMouseWarp(int aX, int aY);

// mover el cursor con las FLECHAS del telefono (cuando el mouse virtual esta visible):
// clamp a la pantalla + el viewport bajo el cursor pasa a activo (como mover el mouse BT)
void W3dMouseMoverFlechas(int aDx, int aDy);
