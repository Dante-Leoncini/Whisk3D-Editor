#ifndef EMPTY_H
#define EMPTY_H
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner

// COMO SE DIBUJA un Vacio. Es SOLO la vista: no cambia el objeto, ni su transformacion, ni a sus hijos.
// (Pedido del duenio, 2026-11-09: "se puede cambiar la forma a cruz, cuadrado, circulo o esfera con 3 lineas... esto
// solo es como se ve".) Los numeros viajan en el .w3d ("forma"): no cambiar el orden.
enum { VacioCruz = 0, VacioCuadrado, VacioCirculo, VacioEsfera, VacioCubo, VacioFormas };   // (el cubo, al final: ver arriba)

// objeto vacio (eje/pivote), compartido PC/Symbian
class Empty : public Object {
public:
    // nace CIRCULO acostado en el piso: se ve mejor que la cruz (la cruz se perdia contra la grilla)
    int     forma;        // VacioCruz / VacioCuadrado / VacioCirculo / VacioEsfera / VacioCubo
    float   tamDibujo;    // el tamanio del dibujo: 1 = 1 metro de punta a punta (la cruz de siempre)
    Vector3 rotDibujo;    // el giro del dibujo, en grados, como Propiedades (X, Y, Z con Z arriba)

    Empty(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "Empty", pos), forma(VacioCirculo), tamDibujo(1.0f), rotDibujo(0, 0, 0) {
    }
    ObjectType getType() W3D_OVERRIDE { return ObjectType::empty; }
    void RenderObject() W3D_OVERRIDE;
    bool DibujaAlgo() W3D_OVERRIDE;   // su dibujo, solo con el overlay "Empty"
    void CopiarDibujo(const Empty* o) { forma = o->forma; tamDibujo = o->tamDibujo; rotDibujo = o->rotDibujo; }
};
#endif
