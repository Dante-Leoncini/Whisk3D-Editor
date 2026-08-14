#ifndef ELEMENTO2D_H
#define ELEMENTO2D_H
#include "objects/Objects.h"
#include <string>

// =====================================================================
//  Elemento2D — base COMUN de los elementos de una interfaz 2D (texto,
//  imagen, rectangulo, contenedor). Todo lo que comparten vive aca:
//  el rectangulo, el ancla, la rotacion, la opacidad y el layout de
//  sus hijos (padding/filas/columnas/gap/peso/overflow/scroll).
//
//  POSICION: pos.x/pos.y son RELATIVAS al rect de referencia del ancla
//  (1.0 = todo el ancho). pos.z es la profundidad, en px.
// =====================================================================
// unidades del ancho/alto de un elemento (tamModo)
enum {
    TAM2D_FRACCION = 0,   // fraccion del rect del padre (0.5 = la mitad)
    TAM2D_PX       = 1,   // pixeles del lienzo, tal cual (default)
    TAM2D_ESCALADO = 2    // px de REFERENCIA x la escala de pantalla (min(w,h)/480,
                          // la misma del bind escala()): tipo dpi, mide LO MISMO
                          // fisico en vertical y en horizontal
};

// distribucion del eje principal en filas/columnas (estilo css). Solo aplica con
// layoutAjuste MINIMO: con ESTIRAR los hijos ya ocupan el 100% y no hay sobrante.
enum {
    DIST2D_GAP     = 0,   // comportamiento clasico: gap fijo + align (default)
    DIST2D_BETWEEN = 1,   // extremos pegados, el sobrante repartido entre hijos
    DIST2D_AROUND  = 2,   // media unidad en cada extremo, una entera entre hijos
    DIST2D_EVENLY  = 3    // unidades iguales, extremos incluidos
};

class Elemento2D : public Object {
public:
    float ancho, alto;    // el rectangulo (en la unidad que diga tamModo)
    int   tamModo;        // TAM2D_*: fraccion del padre / px / px escalados (dpi).
                          // Antes era el bool tamPx (px vs fraccion): en el formato
                          // se sigue escribiendo tamPx para retrocompat.
    int   ancla;          // 0=centro 1..4=bordes 5..8=esquinas (agarra por su CENTRO)
    float rot2d;          // rotacion en grados (los hijos la heredan)
    float opacidad;       // 0..1; multiplica a la de los padres
    float peso;           // reparto del espacio cuando el PADRE esta en filas/columnas
    // MARGEN exterior (como CSS): aire alrededor del elemento cuando esta en una
    // fila/columna del padre. En px si el padre usa padGapPx; si no, proporcional
    // (fraccion del lado menor del area del padre). En layout libre no aplica.
    float margIzq, margDer, margArr, margAba;
    bool  margUni;        // el panel edita los 4 margenes con UN solo valor
    bool  padUni;         // idem para el padding (tarjeta Hijos)
    bool  expandir;       // en una fila/columna ABSORBE el espacio sobrante (ademas de su
                          // tamano natural), como el elemento Expandir: aprovecha el hueco

    // ---- los HIJOS ----
    float padIzq, padDer; // el padding POR LADO: encoge el rect donde se anclan los hijos
    float padArr, padAba;
    int   layoutHijos;    // 0 = libremente, 1 = filas, 2 = columnas
    int   layoutAjuste;   // 0 = ESTIRAR (se reparten el 100% por peso); 1 = MINIMO (cada
                          // uno su tamano natural; los "Expandir" absorben el sobrante)
    int   layoutAlign;    // con ajuste MINIMO y sin expandir: 0 = inicio, 1 = centro, 2 = fin
    int   distribucion;   // DIST2D_*: reparto css del sobrante del eje principal
                          // (solo con ajuste MINIMO; con gap clasico vale el align)
    float gap;            // espacio entre hijos en filas/columnas
    bool  padGapPx;       // true: padding/gap en px; false: proporcional al LADO MENOR
    bool  recortaX;       // overflow: recortar lo que se sale del area, por eje
    bool  recortaY;
    bool  conScroll;      // permitir scrollear el contenido recortado
    float scrollX, scrollY;   // desplazamiento del contenido (px de lienzo)

    Elemento2D(Object* parent, const std::string& nombre, Vector3 pos)
        : Object(parent, nombre, pos) {
        ancho = 200.0f; alto = 150.0f; tamModo = TAM2D_PX;
        ancla = 0; rot2d = 0.0f; opacidad = 1.0f; peso = 1.0f;
        margIzq = margDer = margArr = margAba = 0.0f;
        margUni = true; padUni = true; expandir = false;
        padIzq = padDer = padArr = padAba = 0.0f;
        layoutHijos = 0; layoutAjuste = 0; layoutAlign = 0; distribucion = 0;
        gap = 0.0f; padGapPx = true;
        recortaX = false; recortaY = false;
        conScroll = false; scrollX = 0.0f; scrollY = 0.0f;
    }
    void RenderObject() W3D_OVERRIDE {}   // nada en el 3D: se dibujan en el Editor 2D
};
#endif
