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

        Gamepad(Object* parent);

        // las animaciones del target
        std::vector<VertexAnimation*> animations;        

        ObjectType getType() override;
        void Reload() override;
        void RenderObject() override;
        void Update();  // Solo declaración

        void UpdateAnimation(){
            blendStep++;
            if (blendStep > 4){
                blendStep = 0;
                currentAnim = nextAnim;
                currentFrame = nextFrame;                
                nextFrame++;

                if (nextFrame > animations[nextAnim]->frames.size() -1){
                    nextFrame = 0;
                }
            }
            ApplyVertexFrame(*animations[currentAnim], currentFrame);
        }

        ~Gamepad();
};

#endif