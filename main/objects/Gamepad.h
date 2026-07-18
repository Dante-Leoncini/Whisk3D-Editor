#ifndef GAMEPAD_H
#define GAMEPAD_H

#ifdef _WIN32
    #define NOMINMAX
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <SDL2/SDL.h>
#include <cmath>
#include <GL/gl.h>

#include "WhiskUI/draw/icons.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "Target.h"
#include "animation/VertexAnimation.h"

// Valores globales para controles
extern float axisState[SDL_CONTROLLER_AXIS_MAX];
extern bool buttonState[SDL_CONTROLLER_BUTTON_MAX];
extern GLfloat deadzone;
//extern GLfloat velocidad;

// Función para refrescar inputs del gamepad
void RefreshInputControllerSDL(SDL_Event &e);

class Gamepad : public Object, public Target {
    public:
        Gamepad(Object* parent, GLfloat velocidad = 0.03f, 
            GLfloat piso = 0.0f, 
            GLfloat gravedad = 0.03f, 
            GLfloat limiteIzquierdo = -2.0f, 
            GLfloat limiteDerecho = 2.0f, 
            GLfloat limiteFondo = -2.0f, 
            GLfloat limiteFrente = 2.0f);

        VertexAnimationActive* targetAnim = NULL;

        ObjectType getType() override;
        Vector3 velocity;
        bool onGround;
        bool wasGrounded;
        GLfloat velocidad;
        GLfloat piso;
        GLfloat gravedad;
        GLfloat limiteIzquierdo;
        GLfloat limiteDerecho;
        GLfloat limiteFondo;
        GLfloat limiteFrente;
        GLfloat velocidadMaximaCaida;
        GLfloat potenciaSalto;

        GLfloat deadzone = 0.20f;
        void Reload() override;
        void RenderObject() override;
        void Update();  // Solo declaración

        ~Gamepad();
};

#endif