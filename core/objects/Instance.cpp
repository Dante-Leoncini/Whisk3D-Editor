#include "Instance.h"

// Constructor
Instance::Instance(Object* parent, Object* instance)
    : Object(parent, "Instance", {0,0,0}) 
{
    IconType = static_cast<size_t>(IconType::instance);

    if (!instance) return;

    target = instance;
    pos = instance->pos;
    rot = instance->rot;
    scale = instance->scale;
}

// Devuelve el tipo de objeto
ObjectType Instance::getType() {
    return ObjectType::instance;
}

// Recarga la instancia
void Instance::Reload() {
    ReloadTarget(this);
}

// Renderizado de la instancia
void Instance::RenderObject() {
    if (!target && target != this) return;

    glPushMatrix();
    //glTranslatef(target->posX, target->posZ, target->posY);
    /*glRotatef(target->rotX, 1, 0, 0);
    glRotatef(target->rotZ, 0, 1, 0);
    glRotatef(target->rotY, 0, 0, 1);*/

    for(size_t i = 0; i < count; i++){
        //glScalef(scale.x, scale.y, scale.z);
        target->RenderObject();

        if(RenderChildrens){
            for(size_t c = 0; c < target->Childrens.size(); c++){
                // cancela la rotación y traslación del target
                //glTranslatef(-target->posX, -target->posZ, -target->posY);
                /*glRotatef(target->rotX, -1, 0, 0);
                glRotatef(target->rotZ, 0, -1, 0);
                glRotatef(target->rotY, 0, 0, -1);*/

                target->Childrens[c]->Render();
            }
        }

        //glTranslatef(posX, posZ, posY);
        /*glRotatef(rotX, 1, 0, 0);
        glRotatef(rotZ, 0, 1, 0);
        glRotatef(rotY, 0, 0, 1);*/
    }

    glPopMatrix();
}

// Destructor
Instance::~Instance() {
    delete name;
}