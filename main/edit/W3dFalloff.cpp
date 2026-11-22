// ============================================================================
//  W3dFalloff: la curva de caida de una herramienta con radio. Ver W3dFalloff.h
//  para el contrato. C++03 (compila en Symbian).
// ============================================================================
#include "edit/W3dFalloff.h"
#include <math.h>   // sqrtf

// nombres EN EL ORDEN del enum (claves de traduccion; T() les da el idioma)
static const char* kNames[FoTotal] = {
    "Custom", "Smooth", "Smoother", "Sphere", "Root",
    "Sharp", "Linear", "Sharper", "Inverse Square", "Constant"
};
// icono del skin por tipo. Tres COMPARTEN arte con su pariente porque la curva se dibuja
// igual a 10x10 px y un icono casi-identico se lee peor que uno repetido: Smooth usa el de
// Smoother, Sharper el de Sharp y Inverse Square el de Sphere (las dos son convexas, caen
// despacio y despues de golpe). "" = sin arte -> el item cae a texto solo.
// Agregar arte = tirar el png en res/Skins/Whisk3D/atlas/icons/ con este nombre,
// sumarlo al enum IconType (WhiskUI/draw/icons.h) y poner el nombre aca.
static const char* kIcons[FoTotal] = {
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

const char* W3dFalloffName(int type) {
    return (type >= 0 && type < FoTotal) ? kNames[type] : "";
}
const char* W3dFalloffIcon(int type) {
    return (type >= 0 && type < FoTotal) ? kIcons[type] : "";
}

W3dFalloff::W3dFalloff() {
    type = FoSmooth;   // el que tenia clavado el pincel antes de que esto existiera
    CurveDefault();
}

void W3dFalloff::CurveDefault() {
    points.clear();
    points.push_back(W3dFalloffPoint(0.0f, 1.0f)); // centro: intensidad completa
    points.push_back(W3dFalloffPoint(1.0f, 0.0f)); // borde: cero
}

// ---------------------------------------------------------------------------
//  Evaluacion
// ---------------------------------------------------------------------------
// Todos los presets se escriben sobre s = 1 - t (s = 1 en el CENTRO), que es como
// se leen de corrido: "s" es cuanto queda de pincel. Son las curvas de caida
// clasicas (smoothstep, smootherstep, esfera, raiz, cuadratica, lineal, cuarta,
// cuadrado inverso, constante), con los nombres con que las conoce cualquiera
// que haya pintado pesos.
static float EvalPreset(int type, float t) {
    const float s = 1.0f - t;
    switch (type) {
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
static float EvalCurve(const std::vector<W3dFalloffPoint>& p, float x) {
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
    float f = (type == FoCustom) ? EvalCurve(points, t) : EvalPreset(type, t);
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

int W3dFalloff::CurveAdd(float x, float y) {
    x = Clamp01(x); y = Clamp01(y);
    size_t i = 0;
    while (i < points.size() && points[i].x < x) i++;
    points.insert(points.begin() + i, W3dFalloffPoint(x, y));
    return (int)i;
}

void W3dFalloff::CurveDelete(int i) {
    // con menos de 2 puntos no hay curva que evaluar, y borrar un EXTREMO deja la
    // curva sin arranque o sin final (plana a partir de ahi): se prohiben los dos.
    if (points.size() <= 2) return;
    if (i <= 0 || i >= (int)points.size() - 1) return;
    points.erase(points.begin() + i);
}

void W3dFalloff::CurveMove(int i, float x, float y) {
    if (i < 0 || i >= (int)points.size()) return;
    x = Clamp01(x); y = Clamp01(y);
    // los puntos no se pasan de sus vecinos: la curva SIEMPRE queda ordenada por x
    // (Eval asume eso). Los extremos quedan clavados en x=0 y x=1: si se pudieran
    // mover en x, la curva dejaria de cubrir todo el radio.
    if (i == 0) x = 0.0f;
    else if (i == (int)points.size() - 1) x = 1.0f;
    else {
        const float minX = points[i-1].x + 0.001f;
        const float maxX = points[i+1].x - 0.001f;
        if (x < minX) x = minX;
        if (x > maxX) x = maxX;
    }
    points[i].x = x;
    points[i].y = y;
}

int W3dFalloff::NearCurve(float x, float y, float radius) const {
    int best = -1;
    float bestD2 = radius * radius;
    for (size_t i = 0; i < points.size(); i++) {
        const float dx = points[i].x - x, dy = points[i].y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= bestD2) { bestD2 = d2; best = (int)i; }
    }
    return best;
}
