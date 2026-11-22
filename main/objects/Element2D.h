#ifndef ELEMENT2_D_H
#define ELEMENT2_D_H
#include "objects/Objects.h"
#include <string>

// =====================================================================
//  Element2D — base COMUN de los elementos de una interfaz 2D (texto,
//  imagen, rectangulo, contenedor). Todo lo que comparten vive aca:
//  el rectangulo, el ancla, la rotacion, la opacidad y el layout de
//  sus hijos (padding/filas/columnas/gap/peso/overflow/scroll).
//
//  POSICION: pos.x/pos.y son RELATIVAS al rect de referencia del ancla
//  (1.0 = todo el ancho). pos.z es la profundidad, en px.
// =====================================================================
// unidades del ancho/alto de un elemento (sizeMode)
enum {
    TAM2D_FRACTION = 0,   // fraccion del rect del padre (0.5 = la mitad)
    TAM2D_PX       = 1,   // pixeles del lienzo, tal cual (default)
    TAM2D_SCALED   = 2    // px de REFERENCIA x la escala de pantalla (min(w,h)/480,
                          // la misma del bind scale()): tipo dpi, mide LO MISMO
                          // fisico en vertical y en horizontal
};

// distribucion del eje principal en filas/columnas (estilo css). Solo aplica con
// layoutFit MINIMO: con ESTIRAR los hijos ya ocupan el 100% y no hay sobrante.
enum {
    DIST2D_GAP     = 0,   // comportamiento clasico: gap fijo + align (default)
    DIST2D_BETWEEN = 1,   // extremos pegados, el sobrante repartido entre hijos
    DIST2D_AROUND  = 2,   // media unidad en cada extremo, una entera entre hijos
    DIST2D_EVENLY  = 3    // unidades iguales, extremos incluidos
};

class Element2D : public Object {
public:
    float width, height;    // el rectangulo (en la unidad que diga sizeMode)
    int   sizeMode;         // TAM2D_*: fraccion del padre / px / px escalados (dpi).
                          // Antes era el bool tamPx (px vs fraccion): en el formato
                          // se sigue escribiendo tamPx para retrocompat.
    int   anchor;        // 0=centro 1..4=bordes 5..8=esquinas (agarra por su CENTRO)
    float rot2d;         // rotacion en grados (los hijos la heredan)
    float opacity;       // 0..1; multiplica a la de los padres
    float weight;        // reparto del espacio cuando el PADRE esta en filas/columnas
    // MARGEN exterior (como CSS): aire alrededor del elemento cuando esta en una
    // fila/columna del padre. En px si el padre usa padGapPx; si no, proporcional
    // (fraccion del lado menor del area del padre). En layout libre no aplica.
    float marginLeft, marginRight, marginTop, marginBottom;
    bool  marginUni;       // el panel edita los 4 margenes con UN solo valor
    bool  padUni;          // idem para el padding (tarjeta Hijos)
    bool  expand;          // en una fila/columna ABSORBE el espacio sobrante (ademas de su
                          // tamano natural), como el elemento Expandir: aprovecha el hueco

    // ---- los HIJOS ----
    float padLeft, padRight; // el padding POR LADO: encoge el rect donde se anclan los hijos
    float padArr, padBottom;
    int   layoutChildren;   // 0 = libremente, 1 = filas, 2 = columnas
    int   layoutFit;        // 0 = ESTIRAR (se reparten el 100% por peso); 1 = MINIMO (cada
                          // uno su tamano natural; los "Expandir" absorben el sobrante)
    int   layoutAlign;    // con ajuste MINIMO y sin expandir: 0 = inicio, 1 = centro, 2 = fin
    int   distribution;   // DIST2D_*: reparto css del sobrante del eje principal
                          // (solo con ajuste MINIMO; con gap clasico vale el align)
    float gap;            // espacio entre hijos en filas/columnas
    bool  padGapPx;       // true: padding/gap en px; false: proporcional al LADO MENOR
    bool  cropsX;         // overflow: recortar lo que se sale del area, por eje
    bool  cropsY;
    bool  withScroll;      // permitir scrollear el contenido recortado
    float scrollX, scrollY;   // desplazamiento del contenido (px de lienzo)

    Element2D(Object* parent, const std::string& name, Vector3 pos)
        : Object(parent, name, pos) {
        width  = 200.0f; height = 150.0f; sizeMode = TAM2D_PX;
        anchor = 0; rot2d = 0.0f; opacity = 1.0f; weight = 1.0f;
        marginLeft = marginRight = marginTop = marginBottom = 0.0f;
        marginUni  = true; padUni = true; expand = false;
        padLeft = padRight = padArr = padBottom = 0.0f;
        layoutChildren = 0; layoutFit = 0; layoutAlign = 0; distribution = 0;
        gap = 0.0f; padGapPx = true;
        cropsX = false; cropsY = false;
        withScroll = false; scrollX = 0.0f; scrollY = 0.0f;
    }
    void RenderObject() W3D_OVERRIDE {}   // nada en el 3D: se dibujan en el Editor 2D
    bool DrawsSomething() W3D_OVERRIDE { return false; }   // (el pase 3D ni los apila)
};
#endif
