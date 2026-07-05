#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "Mirror.h"

// Constructor
Mirror::Mirror(Object* parent, Vector3 pos, bool x, bool y, bool z)
    : Object(parent, "Mirror", pos)
{
    mirrorX = x;
    mirrorY = y;
    mirrorZ = z;
}

// Devuelve el tipo de objeto
ObjectType Mirror::getType() {
    return ObjectType::mirror;
}

// Recarga el target
void Mirror::Reload() {
    ReloadTarget(this);
}

// Renderizado del mirror
void Mirror::RenderObject() {
    if (!target && target != this || (!mirrorX && !mirrorY && !mirrorZ)) return;

    w3dEngine::FrontFace(false); // invertir frente mientras renderea el espejo

    w3dEngine::PushMatrix();

    // Obtener matriz del target
    target->GetMatrix(M);

    // Invertir la posición del target en el eje espejo
    if (mirrorZ) {
        w3dEngine::Scalef(1, -1, 1); // espejo en Z

        w3dEngine::MultMatrix(M.m);
        target->RenderObject();

        if (RenderChildrens) {
            for (size_t c = 0; c < target->Childrens.size(); c++) {
                target->Childrens[c]->Render();
            }
        }
    }

    w3dEngine::PopMatrix();

    w3dEngine::FrontFace(true); // restaurar
}

// Destructor
Mirror::~Mirror() {
}