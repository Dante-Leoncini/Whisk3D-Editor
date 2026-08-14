#ifndef SLICE9_H
#define SLICE9_H
#include "objects/Elemento2D.h"

// =====================================================================
//  Slice9 — una imagen con BORDES FIJOS (9 pedazos): las esquinas no se
//  deforman, los lados se estiran en un solo eje y el centro llena el
//  resto. Lo clasico para marcos, botones y paneles (como los bordes
//  del propio Whisk3D).
// =====================================================================
class Slice9 : public Elemento2D {
public:
    std::string textura;  // ruta del archivo de imagen
    // grosor del borde EN LA TEXTURA (px del archivo), por eje: las esquinas suelen ser
    // cuadradas pero pueden ser rectangulares. Minimo 1; maximo la mitad menos 1 (en una
    // imagen de 5, ceil(5/2)-1 = 2: esquinas de 2x2 y el pixel del medio estirado).
    float bordeX, bordeY;
    float color[4];       // TINTE (el arte de UI suele ser blanco: se tine, como el editor)
    int   palTinte;       // indice en la paleta del UI (-1 = tinte propio)
    float escalaBorde;    // grosor dibujado = borde * esto (2 = borde el doble de gordo)
    bool  filtrado;       // false: sin filtro (NEAREST, pixel-perfect)

    Slice9(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Slice9", pos) {
        bordeX = 8.0f; bordeY = 8.0f;
        escalaBorde = 1.0f;
        color[0] = color[1] = color[2] = color[3] = 1.0f;
        filtrado = true;
        palTinte = -1;
    }
    ObjectType getType() override { return ObjectType::slice9; }
};
#endif
