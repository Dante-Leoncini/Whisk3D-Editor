#ifndef BOOLEANMOD_H
#define BOOLEANMOD_H

#include "edit/PolyMesh.h"

// ============================================================================
//  BOOLEAN (CSG) sobre POLIGONOS. La operacion del modificador Boolean.
//
//  ALGORITMO: corte por la CURVA DE INTERSECCION real (ver BooleanMod.cpp).
//  Cada cara de una malla se interseca cara contra cara con la otra (el tramo de
//  la recta comun de los dos planos que cae adentro de las dos), los tramos de
//  cada cara se encadenan y la cara se parte SOLO por esas cadenas:
//    - cadena ABIERTA (cruza la cara de borde a borde): la parte en dos, y el
//      punto donde toca el borde queda tambien en la cara vecina (que recibe el
//      mismo tramo) -> sin T-junctions, se pueden mover vertices sin grietas;
//    - cadena CERRADA (un agujero adentro de la cara): DOS cortes, de dos esquinas
//      vecinas de la cara a dos vertices vecinos del agujero. Quedan un "cuadrado"
//      (2 esquinas + 2 del agujero) y una "U" con todo el resto (todas las esquinas
//      + todos los vertices del agujero: 4 + 8 = 12 en el cubo con el cilindro).
//      Sin vertices nuevos ni repetidos: las conexiones van de los vertices del
//      agujero a los preexistentes. (Lo propuso el dueno en lugar de una "O" con
//      una linea: un vertice repetido en la cara rompia triangulacion y aristas.)
//  Un BSP por los planos de cada malla queda SOLO para clasificar un punto
//  adentro/afuera. Una cara que la otra malla no toca sale INTACTA (un quad
//  sigue quad, un ngon sigue ngon); nada se triangula. Es lo que pidio el dueno
//  y lo que deja una malla editable al hacer Apply. Los ngons concavos que salen
//  los triangula W3dTriangularCara (ear clipping) al armar el index buffer.
//  LIMITE conocido: pedazos coplanares vecinos no se fusionan (la union de dos
//  cajas deja la tapa en 3 quads); es un pase aparte (pendiente).
//
//  ENTRADA: A (se MODIFICA en el lugar) y B, las dos en el MISMO espacio (el
//  llamador ya paso B al local de A) y con el winding hacia AFUERA (el mismo
//  criterio que usa el render para el back-face). Las mallas tienen que ser
//  CERRADAS para que "adentro/afuera" signifique algo.
//  op: 0 = Intersect (lo que esta en las dos), 1 = Union, 2 = Difference (A - B).
//  matB: mesh part (de A) para las caras que vienen de B.
//  uv y color POR CORNER se conservan (interpolados donde se corta).
// ============================================================================
void BooleanPoly(PolyMesh& A, const PolyMesh& B, int op, int matB);

// las 3 operaciones, en el ORDEN de la fila de botones del panel (y del archivo)
struct BoolOp { enum Enum { Intersect = 0, Union = 1, Difference = 2 }; };

#endif // BOOLEANMOD_H
