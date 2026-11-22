#ifndef PROPORTIONAL_H
#define PROPORTIONAL_H
// ============================================================================
//  Proportional.h - PROPORTIONAL EDITING (tecla O): al mover/rotar/escalar, lo seleccionado va
//  entero y lo NO seleccionado se arrastra segun su distancia a lo seleccionado (a la cosa
//  seleccionada mas cercana, no al centro), con una curva de caida y un radio en unidades de
//  mundo. Vale para los vertices de Edit Mode y para los objetos de Object Mode.
//
//  El estado es uno solo (g_prop). Los pesos se PRECALCULAN al arrancar el transform (una
//  distancia por vertice/objeto guardada en memoria: en el N95 no se puede volver a medir por
//  frame) y solo se re-evaluan cuando cambia el radio (rueda / barra de influencia).
// ============================================================================
#include "edit/W3dFalloff.h"
class Viewport3D;

// curvas de caida (el orden es el del menu). Random = lineal por un azar fijo por vertice.
enum { PropSmooth = 0, PropSphere, PropRoot, PropInvSquare, PropSharp, PropLinear, PropConstant, PropRandom, PropTypes };

struct W3dProportional {
    bool  on;          // tecla O / menu / boton de la barra
    bool  connected;   // Connected Only: la distancia se mide POR LAS ARISTAS (islas sueltas no se arrastran)
    int   type;        // Prop*
    float radius;      // radio de influencia, en unidades de MUNDO
    W3dFalloff curve;  // la curva del tipo (para Eval)
    W3dProportional();
};
extern W3dProportional g_prop;

const char* ProportionalTypeName(int type);      // clave de traduccion ("Smooth", "Random", ...)
int         ProportionalIconType(int type);      // IconType del icono de la curva (nunca -1)
void  ProportionalSetType(int type);
float ProportionalWeight(float dist, int seed);   // 0..1 segun la distancia a lo seleccionado (0 = fuera del radio)
void  ProportionalRadiusSet(float r);
void  ProportionalRadiusScale(float factor);      // rueda del mouse: x1.1 / :1.1
bool  ProportionalApplicable();                   // prendido y en Edit Mode u Object Mode
void  ProportionalReapply();                      // cambio el radio con un transform en curso: pesos + reescritura
void  ProportionalRender(Viewport3D* vp);         // el circulo de influencia (overlay, durante el transform)
#endif // PROPORTIONAL_H
