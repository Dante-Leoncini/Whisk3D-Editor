#ifndef GAMEPAD_H
#define GAMEPAD_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <SDL2/SDL.h>
#include <cmath>
#include <GL/gl.h>

#include "UI/icons.h"
#include "Objects.h"
#include "Modifier.h"

// Valores globales para controles
extern float axisState[SDL_CONTROLLER_AXIS_MAX];
extern bool buttonState[SDL_CONTROLLER_BUTTON_MAX];
extern GLfloat deadzone;
extern GLfloat velocidad;

// Función para refrescar inputs del gamepad
void RefreshInputControllerSDL(SDL_Event &e);

class Gamepad : public Object, public Modifier {
public:
    Gamepad(Object* parent);

    ObjectType getType() override;
    void Reload() override;
    void RenderObject() override;
    void Update();  // Solo declaración

    ~Gamepad();
};

#endif