#include "Curve.h"

// ===================================================
// Constructor
// ===================================================
Curve::Curve(Object* parent, Vector3 pos)
    : Object(parent, "Curve", pos)
{
    IconType = static_cast<size_t>(IconType::curve);
}

// ===================================================
// Tipo de objeto
// ===================================================
ObjectType Curve::getType() {
    return ObjectType::curve;
}

// ===================================================
// Destructor
// ===================================================
Curve::~Curve() {
    delete[] vertex;
    delete name;
}

void Curve::RenderObject() {
}
