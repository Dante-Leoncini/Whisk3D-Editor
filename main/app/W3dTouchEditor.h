#ifndef W3D_TOUCH_EDITOR_H
#define W3D_TOUCH_EDITOR_H

// ---------------------------------------------------------------------------------------------------------------
//  La pantalla tactil de las plataformas SIN SDL (Symbian) en el EDITOR. Los dedos los sigue el Core
//  (libs/Whisk3DCore/input/W3dTouch.h: los mismos eventos que SDL arma en Android); aca solo se los vuelve
//  SDL_Event y entran por la misma puerta que en Android y la web (InputUserSDL3, controls.cpp). Los gestos del
//  telefono son los de Android tal cual: una mejora en uno llega al otro.
// ---------------------------------------------------------------------------------------------------------------

// el evento que esta procesando el editor vino de un DEDO
bool W3dEventTouch();
// ...o de un dedo o del mouse Bluetooth (W3dRatonEditor): los dos entran por controls.cpp, como en la PC. En Symbian el
// viewport 3D y los paneles solo corren su camino de PC con ellos: el cursor virtual y el keypad del N95 siguen por el suyo.
bool W3dEventByControls();

// al principio de un handler de input de PC que en Symbian solo debe correr con el dedo o el mouse
#ifdef W3D_SYMBIAN
#define W3D_ONLY_BY_CONTROLS() if (!W3dEventByControls()) return
#else
#define W3D_ONLY_BY_CONTROLS()
#endif

#endif
