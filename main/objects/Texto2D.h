#ifndef TEXTO2D_H
#define TEXTO2D_H
#include "objects/Elemento2D.h"

// =====================================================================
//  Texto2D — un elemento de TEXTO de una interfaz 2D. Vive dentro de
//  un UI (o de otro elemento); se edita en el viewport Editor 2D.
// =====================================================================
class Texto2D : public Elemento2D {
public:
    std::string texto;    // lo que dice ("Texto" por defecto)
    float tam;            // alto de fuente, en px del lienzo (ignorado con autoTam)
    int   alignH;         // 0 = izquierda, 1 = centro, 2 = derecha (tambien alinea las lineas)
    int   alignV;         // 0 = arriba,    1 = centro, 2 = abajo
    float color[4];       // RGBA 0..1 (lo edita el ColorPicker en vivo)
    int   palColor;       // indice en la paleta del UI (-1 = color propio)
    std::string fuente;   // ruta de un .ttf; "" = la fuente de Whisk3D (Inter)
    // TIPO del contenido: 0 = string (tal cual), 1 = number (entero), 2 = float (con
    // 'decimales' a la derecha; 0 decimales = no se ven). Para los HUD numericos.
    int   tipo;
    float decimales;
    // LINEAS: 0 = todo en una sola linea; 1 = salta de linea por PALABRAS (espacios);
    // 2 = salta desde cualquier parte. El ancho disponible es el rect de referencia.
    int   lineas;
    bool  autoTam;        // ajustar el tamano de fuente al area disponible

    Texto2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Texto", pos) {
        texto = "Texto";
        tam = 64.0f;
        alignH = 1; alignV = 1;   // centrado: lo natural para armar un HUD
        color[0] = color[1] = color[2] = color[3] = 1.0f;
        tipo = 0; decimales = 2.0f;
        palColor = -1;
        lineas = 0; autoTam = false;
    }
    ObjectType getType() override { return ObjectType::texto2d; }
};
#endif
