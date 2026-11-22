#ifndef BUTTON2_D_H
#define BUTTON2_D_H
#include "objects/Element2D.h"

// =====================================================================
//  Button2D — un BOTON de interfaz al estilo Whisk3D: una card solida
//  con borde de 1px, texto (fuente pixel por defecto) y/o un icono a
//  la izquierda. Su tamano NATURAL sale del contenido (padding + icono
//  + texto); en una fila/columna con ajuste MINIMO ocupa justo eso.
// =====================================================================
class Button2D : public Element2D {
public:
    std::string text;     // "" = boton de solo icono
    std::string icon;     // ruta de un png ("" = sin icono)
    std::string source;   // "" = la fuente pixel de Whisk3D
    float size;           // alto de fuente (px de lienzo; con sizeMode ESCALADO
                          // se multiplica por la escala de pantalla, min(w,h)/480)
    float pad;                 // aire interno alrededor del contenido (px; escala igual que tam)
    float backgroundColor[4];  // la card
    float textColor[4];        // texto e icono (el icono se tine)
    float borderColor[4];      // el borde de 1px alrededor
    // borde de HOVER (~2px) cuando el MOUSE esta encima del boton (feedback de
    // mouse-over; en tactil no hay hover). Default: el verde accent de Whisk3D.
    float colorHover[4];
    int   palBackground, palText, palBorder;   // indices en la paleta efectiva (-1 = propio)
    int   palHover;                            // idem para el borde de hover
    // FONDO con textura (9 pedazos, como el slice9): si hay textura, la card y el borde
    // salen de ahi (tenida con backgroundColor); sin textura, card plana + borde de 1px
    std::string textureBackground;
    float borderTexX, borderTexY;   // el borde EN el archivo (px)
    float borderScaleTex;           // el borde dibujado = borde * esto

    Button2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Element2D(parent, "Button", pos) {
        text = "Button";
        size = 11.0f;
        pad  = 4.0f;
        backgroundColor[0] = backgroundColor[1] = backgroundColor[2] = 0.19f; backgroundColor[3] = 1.0f;
        textColor[0]       = textColor[1] = textColor[2] = 0.78f; textColor[3] = 1.0f;
        borderColor[0]     = borderColor[1] = borderColor[2] = 0.05f; borderColor[3] = 1.0f;
        colorHover[0]      = 0.0f; colorHover[1] = 1.0f; colorHover[2] = 0.0f; colorHover[3] = 1.0f;
        palBackground = palText = palBorder = palHover = -1;
        borderTexX = borderTexY = 4.0f;
        borderScaleTex = 1.0f;
    }
    ObjectType getType() W3D_OVERRIDE { return ObjectType::button2d; }
};
#endif
