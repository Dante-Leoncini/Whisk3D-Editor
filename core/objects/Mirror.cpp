#include "Mirror.h"

// Constructor
Mirror::Mirror(Object* parent, bool x, bool y, bool z)
    : Object(parent, "Mirror")
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
    if (!target && target != this) return;

    glFrontFace(GL_CW); // invertir frente mientras renderea el espejo

    if (mirrorZ) {
        glPushMatrix();

        glScalef(1, -1, 1);
        glScalef(scaleX, scaleZ, scaleY);
        glTranslatef(target->posX, target->posZ, target->posY);
        glRotatef(rotX, 1, 0, 0);
        glRotatef(rotZ, 0, 1, 0);
        glRotatef(rotY, 0, 0, 1);

        target->RenderObject();

        if (RenderChildrens) {
            for (size_t c = 0; c < target->Childrens.size(); c++) {
                target->Childrens[c]->Render();
            }
        }

        glPopMatrix();
    }

    glFrontFace(GL_CCW); // restaurar
}

// Destructor
Mirror::~Mirror() {
    delete name;
}