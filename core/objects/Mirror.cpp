#include "Mirror.h"

// Constructor
Mirror::Mirror(Object* parent, Vector3 pos, bool x, bool y, bool z)
    : Object(parent, "Mirror", pos)
{
    IconType = static_cast<size_t>(IconType::mirror);
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

    glFrontFace(GL_CW); // invertir frente mientras renderea el espejo

    glPushMatrix();

    // Obtener matriz del target
    target->GetMatrix(M);

    // Invertir la posición del target en el eje espejo
    if (mirrorZ) {
        glScalef(1, -1, 1); // espejo en Z

        glMultMatrixf(M.m);
        target->RenderObject();

        if (RenderChildrens) {
            for (size_t c = 0; c < target->Childrens.size(); c++) {
                target->Childrens[c]->Render();
            }
        }
    }

    glPopMatrix();

    glFrontFace(GL_CCW); // restaurar
}

// Destructor
Mirror::~Mirror() {
    delete name;
}