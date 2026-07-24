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
class Elemento2D : public Object {
public:
    float ancho, alto;    // el rectangulo (px de lienzo, o fraccion del padre si !tamPx)
    bool  tamPx;          // true: ancho/alto en PIXELES (default); false: relativos al
                          // rect del padre (0.5 = la mitad del ancho / del alto)
    int   ancla;          // 0=centro 1..4=bordes 5..8=esquinas (agarra por su CENTRO)
    float rot2d;          // rotacion en grados (los hijos la heredan)
    float opacidad;       // 0..1; multiplica a la de los padres
    float peso;           // reparto del espacio cuando el PADRE esta en filas/columnas

    // ---- los HIJOS ----
    float padIzq, padDer; // el padding POR LADO: encoge el rect donde se anclan los hijos
    float padArr, padAba;
    int   layoutHijos;    // 0 = libremente, 1 = filas, 2 = columnas
    int   layoutAjuste;   // 0 = ESTIRAR (se reparten el 100% por peso); 1 = MINIMO (cada
                          // uno su tamano natural; los "Expandir" absorben el sobrante)
    int   layoutAlign;    // con ajuste MINIMO y sin expandir: 0 = inicio, 1 = centro, 2 = fin
    float gap;            // espacio entre hijos en filas/columnas
    bool  padGapPx;       // true: padding/gap en px; false: proporcional al LADO MENOR
    bool  recortaX;       // overflow: recortar lo que se sale del area, por eje
    bool  recortaY;
    bool  conScroll;      // permitir scrollear el contenido recortado
    float scrollX, scrollY;   // desplazamiento del contenido (px de lienzo)

    Elemento2D(Object* parent, const std::string& nombre, Vector3 pos)
        : Object(parent, nombre, pos) {
        ancho = 200.0f; alto = 150.0f; tamPx = true;
        ancla = 0; rot2d = 0.0f; opacidad = 1.0f; peso = 1.0f;
        padIzq = padDer = padArr = padAba = 0.0f;
        layoutHijos = 0; layoutAjuste = 0; layoutAlign = 0;
        gap = 0.0f; padGapPx = true;
        recortaX = false; recortaY = false;
        conScroll = false; scrollX = 0.0f; scrollY = 0.0f;
    }
    void RenderObject() override {}   // nada en el 3D: se dibujan en el Editor 2D
};
#endif
