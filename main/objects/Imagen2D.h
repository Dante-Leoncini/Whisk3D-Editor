#ifndef IMAGEN2D_H
#define IMAGEN2D_H
#include "objects/Elemento2D.h"

// =====================================================================
//  Imagen2D — un elemento de IMAGEN de una interfaz 2D: una textura
//  dentro del rectangulo del elemento, con modo de ajuste.
// =====================================================================
class Imagen2D : public Elemento2D {
public:
    std::string textura;  // ruta del archivo de imagen ("" = sin textura: rect gris)
    // como se acomoda la TEXTURA dentro del rectangulo del elemento:
    // 0 = estirar (deforma para llenar), 1 = ajustar (entera, con bandas),
    // 2 = cover (llena el rect recortando lo que sobra)
    int   modo;
    float color[4];       // TINTE de la textura (blanco = tal cual)
    int   palTinte;       // indice en la paleta del UI (-1 = tinte propio)
    bool  usarAlpha;      // false: ignora el canal alpha de la textura (se dibuja opaca)
    bool  filtrado;       // false: sin filtro (NEAREST, pixel-perfect)

    Imagen2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Imagen", pos) {
        modo = 0;
        ancho = 200.0f; alto = 200.0f;   // al elegir textura toma el tamano real del archivo
        color[0] = color[1] = color[2] = color[3] = 1.0f;
        usarAlpha = true;
        palTinte = -1;
        filtrado = true;
    }
    ObjectType getType() override { return ObjectType::imagen2d; }
};
#endif
