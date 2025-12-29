#include "Gamepad.h"

// Inicialización de variables globales
float axisState[SDL_CONTROLLER_AXIS_MAX] = {0.0f};
bool buttonState[SDL_CONTROLLER_BUTTON_MAX] = {false};
GLfloat deadzone = 0.20f;
GLfloat velocidad = 0.05f;

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

Gamepad::Gamepad(Object* parent)
    : Object(parent, "Gamepad", Vector3(0,0,0))
{
    IconType = static_cast<size_t>(IconType::gamepad);
}

ObjectType Gamepad::getType() {
    return ObjectType::gamepad;
}

void Gamepad::Reload() {
    ReloadTarget(this);   // ← acá se resuelve el Mesh*

    targetAnim = nullptr;

    // --- solo si el target ES un mesh ---
    if (target && target->getType() == ObjectType::mesh) {

        Mesh* mesh = static_cast<Mesh*>(target);

        targetAnim = FindTargetAnim(mesh);
    }
}

void Gamepad::RenderObject() {
    Update();
}

Gamepad::~Gamepad() {
}