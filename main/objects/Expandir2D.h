#ifndef EXPANDIR2D_H
#define EXPANDIR2D_H
#include "objects/Elemento2D.h"

// =====================================================================
//  Expandir2D — un RESORTE invisible para filas/columnas con ajuste
//  MINIMO: absorbe todo el espacio libre (repartido por peso si hay
//  varios). [boton][expandir][boton] = uno en cada punta;
//  [expandir][boton][expandir] = el boton centrado.
// =====================================================================
class Expandir2D : public Elemento2D {
public:
    Expandir2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Expandir", pos) {}
    ObjectType getType() override { return ObjectType::expandir2d; }
};
#endif
