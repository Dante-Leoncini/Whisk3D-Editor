#ifndef RECT2D_H
#define RECT2D_H
#include "objects/Elemento2D.h"

// =====================================================================
//  Rect2D — un RECTANGULO de color solido (o transparente via alpha)
//  de una interfaz 2D. Para paneles de color; para SOLO ordenar hijos
//  esta Contenedor2D (igual pero sin color).
// =====================================================================
class Rect2D : public Elemento2D {
public:
    float color[4];       // RGBA del relleno (alpha 0 = invisible)
    int   palColor;       // indice en la paleta del UI (-1 = color propio)

    Rect2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Rectangulo", pos) {
        color[0] = color[1] = color[2] = 1.0f; color[3] = 1.0f;
        palColor = -1;
    }
    ObjectType getType() override { return ObjectType::rect2d; }
};
#endif
