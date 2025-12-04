#ifndef CONTROLES_H
#define CONTROLES_H

#include <SDL.h>  // Para SDL_Event
#include "sdl_key_compat.h"

#include "Objects/ObjectMode.h"
#include "Objects/Gamepad.h"

// Funciones de control
void Contadores();
void InputUsuarioSDL3(SDL_Event &e);

#endif