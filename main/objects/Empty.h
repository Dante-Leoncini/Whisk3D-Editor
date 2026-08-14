#ifndef EMPTY_H
#define EMPTY_H
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner

// objeto vacio (eje/pivote), compartido PC/Symbian
class Empty : public Object {
public:
    Empty(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "Empty", pos) {
    }
    ObjectType getType() override { return ObjectType::empty; }
    void RenderObject() override;
};
#endif
