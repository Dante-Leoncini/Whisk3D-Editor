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

// Gamepad::Update vivia mal ubicado dentro de ViewPort3D.cpp; es un metodo de Gamepad -> su casa es aca.
#ifndef W3D_SYMBIAN // el gamepad usa SDL: PC/Android
void Gamepad::Update() {
    if (!target || !Viewport3DActive || !CameraActive || !targetAnim) return;

    // --- 1. CONFIGURACIÓN DE VECTORES DE MOVIMIENTO ---
    Vector3 camForward = CameraActive->forwardVector;
    Vector3 camRight   = CameraActive->rightVector;

    // Proyectar los vectores al plano XZ (el piso) y normalizar.
    Vector3 Fwd_XZ = Vector3(camForward.x, 0.0f, camForward.z).Normalized();
    Vector3 Rgt_XZ = Vector3(camRight.x, 0.0f, camRight.z).Normalized();

    float inputY = axisState[SDL_CONTROLLER_AXIS_LEFTY]; // Adelante/Atrás
    float inputX = axisState[SDL_CONTROLLER_AXIS_LEFTX]; // Lateral/Giro

    bool botonSalto = SDL_GameControllerGetButton(
        controller,
        SDL_CONTROLLER_BUTTON_A
    );

    // Inversión de ejes para el control intuitivo
    float correctedInputY = -inputY;
    float correctedInputX = -inputX;

    // Umbral para ignorar el ruido del stick
    const float DeadZone = 0.1f;

    bool grounded = (target->pos.y <= piso + 0.001f);

    bool justLanded = (!wasGrounded && grounded);
    wasGrounded = grounded;

    // =========================
    // 1. INPUT → HORIZONTAL VELOCITY (INERCIA)
    // =========================

    Vector3 desiredMove(0,0,0);

    if (std::abs(inputY) > DeadZone)
        desiredMove += Fwd_XZ * (-inputY);

    if (std::abs(inputX) > DeadZone)
        desiredMove += Rgt_XZ * (-inputX);

    float accel = velocidad;

    // inercia suave
    velocity.x += desiredMove.x * accel;
    velocity.z += desiredMove.z * accel;

    // freno rápido (clave)
    velocity.x *= 0.85f;
    velocity.z *= 0.85f;

    // =========================
    // 2. GRAVEDAD
    // =========================

    velocity.y -= gravedad;

    // clamp caída
    if (velocity.y < -velocidadMaximaCaida)
        velocity.y = -velocidadMaximaCaida;

    // =========================
    // 3. SALTO
    // =========================

    if (grounded && botonSalto) {
        velocity.y = potenciaSalto;
    }

    // =========================
    // 4. APLICAR MOVIMIENTO
    // =========================

    target->pos += velocity;

    // =========================
    // 5. PISO
    // =========================

    if (target->pos.y <= piso) {
        target->pos.y = piso;
        velocity.y = 0;
        grounded = true;
    }

    // =========================
    // 6. LIMITES XZ
    // =========================

    if (target->pos.x < limiteIzquierdo) target->pos.x = limiteIzquierdo;
    if (target->pos.x > limiteDerecho)   target->pos.x = limiteDerecho;
    if (target->pos.z < limiteFrente)    target->pos.z = limiteFrente;
    if (target->pos.z > limiteFondo)     target->pos.z = limiteFondo;

    // =========================
    // 7. ROTACIÓN SOLO SI ESTÁ EN TIERRA
    // =========================

    Vector3 horizontalVel = velocity;
    horizontalVel.y = 0;

    bool moving = horizontalVel.LengthSq() > 0.001f;

    if (moving) {
        Vector3 dir = horizontalVel.Normalized();

        Quaternion rot = Quaternion::FromDirection(dir, Vector3(0,1,0));
        target->rot = Quaternion::Slerp(target->rot, rot, 0.2f);

        if (grounded){
            targetAnim->nextAnim = 1; // correr
        }
        else if (targetAnim->nextAnim != 2){
            targetAnim->nextAnim = 3; // gira en el aire
        }
    }
    else if (justLanded) {
        targetAnim->nextAnim = 4;
    }
    else if (!grounded && targetAnim->nextAnim != 3) {
        targetAnim->nextAnim = 2; // caer
    }
    else if (grounded && targetAnim->nextAnim != 4){
        targetAnim->nextAnim = 0; // idle
    }
}
#endif // !W3D_SYMBIAN
