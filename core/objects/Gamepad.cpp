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

    Mesh* mesh = dynamic_cast<Mesh*>(target);
    if (!mesh) return;
    meshToAnim = mesh;

    for (auto* anim : animations) {
        anim->target = meshToAnim;

        if (anim->frames.empty()) {
            anim->LoadFrames();
            std::cout << "Anim '"<< anim->name <<"' con " << anim->frames.size() << " frames\n";
            std::cout << "Animar Normals: "<< anim->UseNormals << "\n";
        }
    }
}

void Gamepad::UpdateAnimation(){
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

void Gamepad::RenderObject() {
    Update();
}

Gamepad::~Gamepad() {
}