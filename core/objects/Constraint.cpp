#include "Constraint.h"

// ------------------- Constraint -------------------
Constraint::Constraint(Object* parent)
    : Object(parent, "Constraint")
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

    
}

Constraint::~Constraint() {
    delete name;
}