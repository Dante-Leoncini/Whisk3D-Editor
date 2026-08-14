#ifndef BOTON2D_H
#define BOTON2D_H
#include "objects/Elemento2D.h"

// =====================================================================
//  Boton2D — un BOTON de interfaz al estilo Whisk3D: una card solida
//  con borde de 1px, texto (fuente pixel por defecto) y/o un icono a
//  la izquierda. Su tamano NATURAL sale del contenido (padding + icono
//  + texto); en una fila/columna con ajuste MINIMO ocupa justo eso.
// =====================================================================
class Boton2D : public Elemento2D {
public:
    std::string texto;    // "" = boton de solo icono
    std::string icono;    // ruta de un png ("" = sin icono)
    std::string fuente;   // "" = la fuente pixel de Whisk3D
    float tam;            // alto de fuente (px de lienzo; con tamModo ESCALADO
                          // se multiplica por la escala de pantalla, min(w,h)/480)
    float pad;            // aire interno alrededor del contenido (px; escala igual que tam)
    float colorFondo[4];  // la card
    float colorTexto[4];  // texto e icono (el icono se tine)
    float colorBorde[4];  // el borde de 1px alrededor
    // borde de HOVER (~2px) cuando el MOUSE esta encima del boton (feedback de
    // mouse-over; en tactil no hay hover). Default: el verde accent de Whisk3D.
    float colorHover[4];
    int   palFondo, palTexto, palBorde;   // indices en la paleta efectiva (-1 = propio)
    int   palHover;                       // idem para el borde de hover
    // FONDO con textura (9 pedazos, como el slice9): si hay textura, la card y el borde
    // salen de ahi (tenida con colorFondo); sin textura, card plana + borde de 1px
    std::string texturaFondo;
    float bordeTexX, bordeTexY;   // el borde EN el archivo (px)
    float escalaBordeTex;         // el borde dibujado = borde * esto

    Boton2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Boton", pos) {
        texto = "Boton";
        tam = 11.0f;
        pad = 4.0f;
        colorFondo[0] = colorFondo[1] = colorFondo[2] = 0.19f; colorFondo[3] = 1.0f;
        colorTexto[0] = colorTexto[1] = colorTexto[2] = 0.78f; colorTexto[3] = 1.0f;
        colorBorde[0] = colorBorde[1] = colorBorde[2] = 0.05f; colorBorde[3] = 1.0f;
        colorHover[0] = 0.0f; colorHover[1] = 1.0f; colorHover[2] = 0.0f; colorHover[3] = 1.0f;
        palFondo = palTexto = palBorde = palHover = -1;
        bordeTexX = bordeTexY = 4.0f;
        escalaBordeTex = 1.0f;
    }
    ObjectType getType() override { return ObjectType::boton2d; }
};
#endif
