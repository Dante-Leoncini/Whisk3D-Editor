#ifndef CONTAINER2_D_H
#define CONTAINER2_D_H
#include "objects/Element2D.h"

// =====================================================================
//  Container2D — un rectangulo INVISIBLE: no tiene color, solo sirve
//  para meterle elementos adentro y ordenarlos (anclas, padding,
//  filas/columnas, overflow). Como todos, tiene opacidad (la heredan
//  sus hijos).
// =====================================================================
class Container2D : public Element2D {
public:
    Container2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Element2D(parent, "Container", pos) {}
    ObjectType getType() W3D_OVERRIDE { return ObjectType::cont2d; }
};
#endif
