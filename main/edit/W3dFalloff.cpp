// ============================================================================
//  W3dFalloff: la curva de caida de una herramienta con radio. Ver W3dFalloff.h
//  para el contrato. C++03 (compila en Symbian).
// ============================================================================
#include "edit/W3dFalloff.h"
#include <math.h>   // sqrtf

// nombres EN EL ORDEN del enum (claves de traduccion; T() les da el idioma)
static const char* kNombres[FoTotal] = {
    "Custom", "Smooth", "Smoother", "Sphere", "Root",
    "Sharp", "Linear", "Sharper", "Inverse Square", "Constant"
};
// icono del skin por tipo. Tres COMPARTEN arte con su pariente porque la curva se dibuja
// igual a 10x10 px y un icono casi-identico se lee peor que uno repetido: Smooth usa el de
// Smoother, Sharper el de Sharp y Inverse Square el de Sphere (las dos son convexas, caen
// despacio y despues de golpe). "" = sin arte -> el item cae a texto solo.
// Agregar arte = tirar el png en res/Skins/Whisk3D/atlas/iconos/ con este nombre,
// sumarlo al enum IconType (WhiskUI/draw/icons.h) y poner el nombre aca.
static const char* kIconos[FoTotal] = {
    "falloff_custom",   // Custom
    "falloff_smoother", // Smooth        (comparte con Smoother)
    "falloff_smoother", // Smoother
    "falloff_sphere",   // Sphere
    "falloff_root",     // Root
    "falloff_sharp",    // Sharp
    "falloff_linear",   // Linear
    "falloff_sharp",    // Sharper       (comparte con Sharp)
    "falloff_sphere",   // Inverse Square(comparte con Sphere)
    "falloff_constant"  // Constant
};

const char* W3dFalloffNombre(int tipo) {
    return (tipo >= 0 && tipo < FoTotal) ? kNombres[tipo] : "";
}
const char* W3dFalloffIcono(int tipo) {
    return (tipo >= 0 && tipo < FoTotal) ? kIconos[tipo] : "";
}

W3dFalloff::W3dFalloff() {
    tipo = FoSmooth;   // el que tenia clavado el pincel antes de que esto existiera
    CurvaDefault();
}

void W3dFalloff::CurvaDefault() {
    puntos.clear();
    puntos.push_back(W3dFalloffPunto(0.0f, 1.0f)); // centro: intensidad completa
    puntos.push_back(W3dFalloffPunto(1.0f, 0.0f)); // borde: cero
}

// ---------------------------------------------------------------------------
//  Evaluacion
// ---------------------------------------------------------------------------
// Todos los presets se escriben sobre s = 1 - t (s = 1 en el CENTRO), que es como
// se leen de corrido: "s" es cuanto queda de pincel. Son las mismas formulas que
// usa Blender para sus presets homonimos, asi que un artista que viene de ahi
// encuentra lo que espera.
static float EvalPreset(int tipo, float t) {
    const float s = 1.0f - t;
    switch (tipo) {
        case FoSmooth:     return s * s * (3.0f - 2.0f * s);
        case FoSmoother:   return s * s * s * (s * (s * 6.0f - 15.0f) + 10.0f);
        case FoSphere:     return sqrtf(2.0f * s - s * s);
        case FoRoot:       return sqrtf(s);
        case FoSharp:      return s * s;
        case FoLinear:     return s;
        case FoSharper:    return s * s * s * s;
        case FoInvSquare:  return s * (2.0f - s);
        case FoConstant:   return 1.0f;   // sin caida: todo el radio a full
    }
    return s;
}

// curva custom: hermite entre los dos puntos que rodean a x, con las tangentes
// sacadas de los vecinos (Catmull-Rom). Da una curva suave que PASA por todos los
// puntos, que es lo que se espera al arrastrarlos. En los extremos la tangente se
// toma del unico lado que hay (sin inventar puntos fantasma afuera del 0..1).
static float EvalCurva(const std::vector<W3dFalloffPunto>& p, float x) {
    const int n = (int)p.size();
    if (n == 0) return 0.0f;
    if (n == 1) return p[0].y;
    if (x <= p[0].x) return p[0].y;                    // antes del primero: plano
    if (x >= p[n-1].x) return p[n-1].y;                // despues del ultimo: plano

    int i = 0;                                          // tramo [i, i+1] que contiene x
    while (i < n - 2 && x > p[i+1].x) i++;
    const float x0 = p[i].x, y0 = p[i].y;
    const float x1 = p[i+1].x, y1 = p[i+1].y;
    const float dx = x1 - x0;
    if (dx <= 1e-6f) return y1;                         // dos puntos pegados: sin tramo
    const float u = (x - x0) / dx;

    // tangentes (en unidades de y por tramo) de Catmull-Rom
    const float yPrev = (i > 0)     ? p[i-1].y : y0;
    const float yNext = (i+2 < n)   ? p[i+2].y : y1;
    const float m0 = 0.5f * (y1 - yPrev);
    const float m1 = 0.5f * (yNext - y0);

    const float u2 = u * u, u3 = u2 * u;
    float y = (2.0f*u3 - 3.0f*u2 + 1.0f) * y0
            + (u3 - 2.0f*u2 + u)         * m0
            + (-2.0f*u3 + 3.0f*u2)       * y1
            + (u3 - u2)                  * m1;
    return y;
}

float W3dFalloff::Eval(float t) const {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float f = (tipo == FoCustom) ? EvalCurva(puntos, t) : EvalPreset(tipo, t);
    // el hermite puede pasarse de 0..1 entre puntos muy verticales: se clampea aca
    // (una sola vez) para que NINGUN caller tenga que desconfiar del resultado.
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return f;
}

// ---------------------------------------------------------------------------
//  Edicion de la curva custom (la usa el popup; tambien el harness de tests)
// ---------------------------------------------------------------------------
static float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v; }

int W3dFalloff::CurvaAgregar(float x, float y) {
    x = Clamp01(x); y = Clamp01(y);
    size_t i = 0;
    while (i < puntos.size() && puntos[i].x < x) i++;
    puntos.insert(puntos.begin() + i, W3dFalloffPunto(x, y));
    return (int)i;
}

void W3dFalloff::CurvaBorrar(int i) {
    // con menos de 2 puntos no hay curva que evaluar, y borrar un EXTREMO deja la
    // curva sin arranque o sin final (plana a partir de ahi): se prohiben los dos.
    if (puntos.size() <= 2) return;
    if (i <= 0 || i >= (int)puntos.size() - 1) return;
    puntos.erase(puntos.begin() + i);
}

void W3dFalloff::CurvaMover(int i, float x, float y) {
    if (i < 0 || i >= (int)puntos.size()) return;
    x = Clamp01(x); y = Clamp01(y);
    // los puntos no se pasan de sus vecinos: la curva SIEMPRE queda ordenada por x
    // (Eval asume eso). Los extremos quedan clavados en x=0 y x=1: si se pudieran
    // mover en x, la curva dejaria de cubrir todo el radio.
    if (i == 0) x = 0.0f;
    else if (i == (int)puntos.size() - 1) x = 1.0f;
    else {
        const float minX = puntos[i-1].x + 0.001f;
        const float maxX = puntos[i+1].x - 0.001f;
        if (x < minX) x = minX;
        if (x > maxX) x = maxX;
    }
    puntos[i].x = x;
    puntos[i].y = y;
}

int W3dFalloff::CurvaCercano(float x, float y, float radio) const {
    int mejor = -1;
    float mejorD2 = radio * radio;
    for (size_t i = 0; i < puntos.size(); i++) {
        const float dx = puntos[i].x - x, dy = puntos[i].y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= mejorD2) { mejorD2 = d2; mejor = (int)i; }
    }
    return mejor;
}
