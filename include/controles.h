#ifndef CONTROLES_H
#define CONTROLES_H

//para pantallas multitactiles
struct Finger {
    float x;
    float y;
};

std::map<SDL_FingerID, Finger> fingers;
float lastDistance = 0.0f;
float PINCHposY = 0.0f;

void InputUsuarioSDL3(SDL_Event &e);

// Funciones de control
void Contadores();
void InputUsuarioSDL3(SDL_Event &e);

#endif