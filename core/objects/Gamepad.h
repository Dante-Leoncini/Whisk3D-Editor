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
#include "Mesh.h"
#include "Target.h"

#include "VertexAnimation.h"

// Valores globales para controles
extern float axisState[SDL_CONTROLLER_AXIS_MAX];
extern bool buttonState[SDL_CONTROLLER_BUTTON_MAX];
extern GLfloat deadzone;
extern GLfloat velocidad;

// Función para refrescar inputs del gamepad
void RefreshInputControllerSDL(SDL_Event &e);

class Gamepad : public Object, public Target {
    public:
        int currentAnim = 0;   // animación activa (idle/run o la que sea)
        int nextAnim    = 0;  // animación a la que queremos ir

        int currentFrame = 0;
        int nextFrame    = 1;

        int blendStep = 0;

        Mesh* meshToAnim = nullptr;

        Gamepad(Object* parent);

        // las animaciones del target
        std::vector<VertexAnimation*> animations;        

        ObjectType getType() override;
        void Reload() override;
        void RenderObject() override;
        void Update();  // Solo declaración

        void UpdateAnimation(){
            if (!meshToAnim) return;
            // Avanza el blend
            blendStep++;

            float blendT = blendStep / 5.0f;
            if (blendT > 1.0f) blendT = 1.0f;

            // Mezcla entre frame actual y el siguiente (puede ser otra anim)
            BlendVertexAnimations(
                *animations[currentAnim],
                *animations[nextAnim],
                currentFrame,
                nextFrame,
                blendT,
                meshToAnim
            );

            // Si terminó el blend
            if (blendStep >= 5) {
                blendStep = 0;

                // Consolidamos estado
                currentAnim  = nextAnim;
                currentFrame = nextFrame;

                // Avanzar frame SOLO de la anim activa
                nextFrame++;

                if (nextFrame >= animations[currentAnim]->frames.size()) {
                    nextFrame = 0;
                }
            }
        }

        ~Gamepad();
};

#endif