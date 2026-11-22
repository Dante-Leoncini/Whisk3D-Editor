#ifndef EXPAND2_D_H
#define EXPAND2_D_H
#include "objects/Element2D.h"

// =====================================================================
//  Expand2D — un RESORTE invisible para filas/columnas con ajuste
//  MINIMO: absorbe todo el espacio libre (repartido por peso si hay
//  varios). [boton][expandir][boton] = uno en cada punta;
//  [expandir][boton][expandir] = el boton centrado.
// =====================================================================
class Expand2D : public Element2D {
public:
    Expand2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Element2D(parent, "Expand", pos) {}
    ObjectType getType() W3D_OVERRIDE { return ObjectType::expand2d; }
};
#endif
