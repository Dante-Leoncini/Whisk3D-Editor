#ifndef CONTROLES_H
#define CONTROLES_H

#include <map>
#ifdef W3D_SYMBIAN
#include "W3dSdlMinimo.h"   // sin SDL: el telefono arma los mismos eventos (W3dTactilEditor.cpp)
#endif
#include "sdl_key_compat.h"
#include "variables.h"
#include "objects/Gamepad.h"
#include "objects/ObjectMode.h"
#include "ViewPorts/ViewPorts.h"

//para pantallas multitactiles
struct Finger {
    float x;
    float y;
};

extern std::map<SDL_FingerID, Finger> fingers;
extern float lastDistance;
extern float PINCHposY;

void SetFullScreen(bool value);
void InputUsuarioSDL3(SDL_Event &e);

// Funciones de control
void Contadores();
void InputUsuarioSDL3(SDL_Event &e);

#endif