#include "Constraint.h"

// ------------------- Constraint -------------------
Constraint::Constraint(Object* parent, bool horizontal, bool pitch, Vector3 pos)
    : Object(parent, "Constraint", pos), useHorizontal(horizontal), usePitch(pitch)
{
    IconType = static_cast<size_t>(IconType::constraint);
}

ObjectType Constraint::getType() {
    return ObjectType::constraint;
}

void Constraint::Reload() {
    ReloadTarget(this);
}

void Constraint::SetCopyPosition(bool X, bool Y, bool Z) {
    copyPosX = X;
    copyPosY = Y;
    copyPosZ = Z;
}

void Constraint::RenderObject() {
    if (!target) return;

    // ------------------- COPY POSITION -------------------
    if (copyPosX || copyPosY || copyPosZ) {
        if (copyPosX) target->pos.x = viewPosGlobal.x;
        if (copyPosY) target->pos.y = viewPosGlobal.y;
        if (copyPosZ) target->pos.z = viewPosGlobal.z;
    }

    // ------------------- COPY ROTATION (BILLBOARD) -------------------
    if (useHorizontal || usePitch){
        // Eje "forward" de la cámara
        Vector3 forward = rotGlobal * Vector3(0,0,-1);

        float pitchAngle = 0.0f;
        float horizAngle = 0.0f;

        // --- Horizontal (yaw) ---
        if (useHorizontal) {
            Vector3 forwardXZ = Vector3(forward.x, 0, forward.z).Normalized();
            horizAngle = atan2f(forwardXZ.x, forwardXZ.z) * (180.0f / M_PI);
        }

        // --- Inclinación (pitch) ---
        if (usePitch) {
            pitchAngle = asinf(forward.y) * (180.0f / M_PI);
        }

        // Construir quaternion final (roll siempre 0)
        target->rot = Quaternion::FromEuler(-pitchAngle, horizAngle, 0.0f);
    }
}

Constraint::~Constraint() {
    delete name;
}