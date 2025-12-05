#include "object2D.h"

// Constructor
Object2D::Object2D(Object2D* parent)
    : Parent(parent)
{}

// Tipo por defecto
Object2DType Object2D::getType() {
    return Object2DType::empty;
}

// Render del objeto base
void Object2D::RenderObject(bool usarColorPropio) {
    // vacío: los hijos lo implementan
}

// Destructor
Object2D::~Object2D() {
    for (Object2D* c : Childrens) {
        delete c;
    }
    Childrens.clear();
}

// Render general (incluye los hijos)
void Object2D::Render(bool usarColorPropio) {
    if (!visible) return;

    RenderObject(usarColorPropio);

    for (Object2D* child : Childrens) {
        child->Render(usarColorPropio);
    }
}