#include "Gamepad.h"
#include "objects/Camera.h"            // CameraActive
#include "animation/Animation.h"          // CurrentFrame / estado de anim
#include "animation/SkeletalAnimation.h"  // pose del esqueleto que maneja el gamepad


// Inicialización de variables globales
float axisState[SDL_CONTROLLER_AXIS_MAX] = {0.0f};
bool buttonState[SDL_CONTROLLER_BUTTON_MAX] = {false};
GLfloat deadzone = 0.20f;
//GLfloat velocidad = 0.05f;

// Función para refrescar inputs del gamepad
void RefreshInputControllerSDL(SDL_Event &e) {    
    if (e.type == SDL_CONTROLLERAXISMOTION) {
        int axis = e.caxis.axis;
        float value = e.caxis.value / 32767.0f;
        axisState[axis] = (fabs(value) < deadzone) ? 0.0f : value;
    }
    else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        buttonState[e.cbutton.button] = true;
    }
    else if (e.type == SDL_CONTROLLERBUTTONUP) {
        buttonState[e.cbutton.button] = false;
    }
}

// ------------------- Gamepad -------------------

Gamepad::Gamepad(Object* parent, GLfloat velocidad, 
            GLfloat piso, 
            GLfloat gravedad, 
            GLfloat limiteIzquierdo, 
            GLfloat limiteDerecho, 
            GLfloat limiteFondo, 
            GLfloat limiteFrente
) : Object(parent, "Gamepad", Vector3(0,0,0))
{
    velocity = Vector3(0,0,0);
    onGround = true;
    wasGrounded = false;
    velocidad = velocidad;
}

ObjectType Gamepad::getType() {
    return ObjectType::gamepad;
}

void Gamepad::Reload() {
    ReloadTarget(this);   // ← acá se resuelve el Mesh*

    targetAnim = NULL;

    // --- solo si el target ES un mesh ---
    if (target && target->getType() == ObjectType::mesh) {

        Mesh* mesh = static_cast<Mesh*>(target);

        targetAnim = FindTargetAnim(mesh);

        //std::cout << "targetAnim: "<< targetAnim << "\n";
    }
}

void Gamepad::RenderObject() {
    Update();
}

Gamepad::~Gamepad() {
}

// La FISICA que vivia aca (mover a Crash: stick + gravedad + salto + limites +
// animaciones) era EXCLUSIVA de ese juego y no tenia nada que hacer colgada del
// editor: ahora es un SCRIPT LUA del proyecto (CrashMadero.lua, junto al .w3d).
// El objeto quedo como "Script": un colgadero de scripts con un target opcional.
#ifndef W3D_SYMBIAN
void Gamepad::Update() {}
#endif // !W3D_SYMBIAN
