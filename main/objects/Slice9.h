#ifndef SLICE9_H
#define SLICE9_H
#include "objects/Element2D.h"

// =====================================================================
//  Slice9 — una imagen con BORDES FIJOS (9 pedazos): las esquinas no se
//  deforman, los lados se estiran en un solo eje y el centro llena el
//  resto. Lo clasico para marcos, botones y paneles (como los bordes
//  del propio Whisk3D).
// =====================================================================
class Slice9 : public Element2D {
public:
    std::string texture;  // ruta del archivo de imagen
    // grosor del borde EN LA TEXTURA (px del archivo), por eje: las esquinas suelen ser
    // cuadradas pero pueden ser rectangulares. Minimo 1; maximo la mitad menos 1 (en una
    // imagen de 5, ceil(5/2)-1 = 2: esquinas de 2x2 y el pixel del medio estirado).
    float borderX, borderY;
    float color[4];       // TINTE (el arte de UI suele ser blanco: se tine, como el editor)
    int   palTint;        // indice en la paleta del UI (-1 = tinte propio)
    float borderScale;    // grosor dibujado = borde * esto (2 = borde el doble de gordo)
    bool  filtered;       // false: sin filtro (NEAREST, pixel-perfect)

    Slice9(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Element2D(parent, "Slice9", pos) {
        borderX = 8.0f; borderY = 8.0f;
        borderScale = 1.0f;
        color[0] = color[1] = color[2] = color[3] = 1.0f;
        filtered = true;
        palTint  = -1;
    }
    ObjectType getType() W3D_OVERRIDE { return ObjectType::slice9; }
};
#endif
