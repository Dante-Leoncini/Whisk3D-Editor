#ifndef CONTENEDOR2D_H
#define CONTENEDOR2D_H
#include "objects/Elemento2D.h"

// =====================================================================
//  Contenedor2D — un rectangulo INVISIBLE: no tiene color, solo sirve
//  para meterle elementos adentro y ordenarlos (anclas, padding,
//  filas/columnas, overflow). Como todos, tiene opacidad (la heredan
//  sus hijos).
// =====================================================================
class Contenedor2D : public Elemento2D {
public:
    Contenedor2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Contenedor", pos) {}
    ObjectType getType() override { return ObjectType::cont2d; }
};
#endif
