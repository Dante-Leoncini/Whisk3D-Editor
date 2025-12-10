#include "Constraint.h"

// ------------------- Constraint -------------------
Constraint::Constraint(Object* parent, Vector3 pos)
    : Object(parent, "Constraint", pos)
{
    IconType = static_cast<size_t>(IconType::constraint);
}

ObjectType Constraint::getType() {
    return ObjectType::constraint;
}

void Constraint::Reload() {
    ReloadTarget(this);
}

void Constraint::RenderObject() {
    if (!target && target != this) return;

    // Posición acumulada
    /*GLfloat totalZ = posZ;
    GLfloat rX = 0, rY = 0, rZ = 0;
    Object* p = Parent;
    while (p != nullptr) {
        totalZ += p->posZ;
        rX += p->rotX;
        rY += p->rotY;
        rZ += p->rotZ;
        p = p->Parent;
    }
    target->rotX = rX - rotYGlobal;
    target->rotY = rY;
    target->rotZ = rZ - rotXGlobal;*/
}

Constraint::~Constraint() {
    delete name;
}