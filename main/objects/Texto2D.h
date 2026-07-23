#ifndef TEXTO2D_H
#define TEXTO2D_H
#include "objects/Objects.h"
#include <string>

// =====================================================================
//  Texto2D — un elemento de TEXTO de una interfaz 2D. Vive SI o SI como
//  hijo de un objeto UI (el outliner es el orden de dibujado: la UI se
//  dibuja al final, sobre la escena). Se edita en el viewport Editor 2D.
//
//  Aunque el mundo es 2D tiene X,Y,Z (profundidad para meter elementos
//  3D en la UI) y sus hijos heredan su transformacion (asi se arman
//  cosas como la rotacion de portadas del casino).
// =====================================================================
class Texto2D : public Object {
public:
    std::string texto;    // lo que dice ("Texto" por defecto)
    float tam;            // alto de fuente, en px del lienzo
    int   alignH;         // 0 = izquierda, 1 = centro, 2 = derecha  (ancla en X)
    int   alignV;         // 0 = arriba,    1 = centro, 2 = abajo    (ancla en Y)
    float color[4];       // RGBA 0..1 (lo edita el ColorPicker en vivo)
    std::string fuente;   // ruta de un .ttf; "" = la fuente de Whisk3D (Inter)

    Texto2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "Texto", pos) {
        texto = "Texto";
        tam = 64.0f;
        alignH = 1; alignV = 1;   // centrado: lo natural para armar un HUD
        color[0] = color[1] = color[2] = color[3] = 1.0f;
    }
    ObjectType getType() override { return ObjectType::texto2d; }
    void RenderObject() override {}   // nada en el 3D: se dibuja en el Editor 2D
};
#endif
