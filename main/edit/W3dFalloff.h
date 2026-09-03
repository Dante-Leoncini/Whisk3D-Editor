#ifndef W3DFALLOFF_H
#define W3DFALLOFF_H

#include <vector>

// ============================================================================
//  FALLOFF: como cae la intensidad de una herramienta con la DISTANCIA.
//
//  Entrada: t = distancia NORMALIZADA al centro (0 = centro, 1 = borde).
//  Salida:  factor 0..1 (1 = intensidad completa).
//
//  Nace para el pincel de pesos (reemplaza el smoothstep que estaba clavado en
//  PincelAplicar), pero NO sabe nada del pincel a proposito: es una curva y su
//  evaluacion. Lo mismo lo va a usar el vertex color, la escultura, el degrade
//  de una particula o cualquier herramienta con radio -- y el POPUP que lo edita
//  (FalloffEditor) recibe un W3dFalloff* de quien sea, como el ColorPicker recibe
//  un GLfloat*.
//
//  C++03 (compila en Symbian): sin auto, sin range-for, sin initializer lists.
// ============================================================================

// Los 9 presets analiticos + la curva a mano. El ORDEN es el de la lista del
// popup (y el de los iconos falloff_*), asi que agregar uno nuevo es agregarlo
// al final -- no en el medio -- para no correr los valores ya guardados.
enum W3dFalloffTipo {
    FoCustom = 0,   // la curva de 'puntos' (editable a mano)
    FoSmooth,       // smoothstep: el clasico, sin escalones
    FoSmoother,     // smootherstep (derivada segunda continua)
    FoSphere,       // perfil de esfera
    FoRoot,         // raiz: cae despacio al principio
    FoSharp,        // cuadratica: concentra en el centro
    FoLinear,       // recta
    FoSharper,      // cuarta: mucho mas concentrada
    FoInvSquare,    // inversa del cuadrado
    FoConstant,     // sin caida: 1 en TODO el radio (borde duro)
    FoTotal
};

// nombre estable del tipo (clave de traduccion: "Smooth", "Sharper", ...)
const char* W3dFalloffNombre(int tipo);
// nombre del icono del skin para ese tipo ("falloff_smooth"...); "" = todavia no
// hay arte y el item se dibuja con texto (los iconos van llegando de a uno).
const char* W3dFalloffIcono(int tipo);

// un punto de la curva custom, en el cuadrado 0..1 (x = distancia, y = factor)
struct W3dFalloffPunto {
    float x, y;
    W3dFalloffPunto() : x(0.0f), y(0.0f) {}
    W3dFalloffPunto(float X, float Y) : x(X), y(Y) {}
};

struct W3dFalloff {
    int tipo;                              // un W3dFalloffTipo
    std::vector<W3dFalloffPunto> puntos;   // curva custom, SIEMPRE ordenada por x

    W3dFalloff();                          // Smooth + la curva custom por defecto

    // EL metodo: factor 0..1 para una distancia normalizada 0..1. Fuera de rango
    // clampea (t<0 = centro, t>1 = borde) para que ningun caller tenga que cuidarlo.
    float Eval(float t) const;

    // ---- curva custom ----
    void CurvaDefault();                   // (0,1) -> (1,0): lleno en el centro, cero en el borde
    int  CurvaAgregar(float x, float y);   // inserta ordenado; devuelve su indice
    void CurvaBorrar(int i);               // los EXTREMOS no se borran (siempre quedan >= 2 puntos)
    void CurvaMover(int i, float x, float y); // mueve el punto i (clampea al cuadrado y entre vecinos)
    int  CurvaCercano(float x, float y, float radio) const; // punto mas cercano (-1 si ninguno cerca)
};

#endif // W3DFALLOFF_H
