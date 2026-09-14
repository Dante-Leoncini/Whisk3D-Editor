#ifndef PROPORCIONAL_H
#define PROPORCIONAL_H
// ============================================================================
//  Proporcional.h - PROPORTIONAL EDITING (tecla O): al mover/rotar/escalar, lo seleccionado va
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
enum { PropSmooth = 0, PropSphere, PropRoot, PropInvSquare, PropSharp, PropLinear, PropConstant, PropRandom, PropTipos };

struct W3dProporcional {
    bool  on;          // tecla O / menu / boton de la barra
    bool  conectado;   // Connected Only: la distancia se mide POR LAS ARISTAS (islas sueltas no se arrastran)
    int   tipo;        // Prop*
    float radio;       // radio de influencia, en unidades de MUNDO
    W3dFalloff curva;  // la curva del tipo (para Eval)
    W3dProporcional();
};
extern W3dProporcional g_prop;

const char* ProporcionalTipoNombre(int tipo);      // clave de traduccion ("Smooth", "Random", ...)
int         ProporcionalTipoIcono(int tipo);       // IconType del icono de la curva (nunca -1)
void  ProporcionalSetTipo(int tipo);
float ProporcionalPeso(float dist, int semilla);   // 0..1 segun la distancia a lo seleccionado (0 = fuera del radio)
void  ProporcionalRadioSet(float r);
void  ProporcionalRadioEscalar(float factor);      // rueda del mouse: x1.1 / :1.1
bool  ProporcionalAplicable();                     // prendido y en Edit Mode u Object Mode
void  ProporcionalReaplicar();                     // cambio el radio con un transform en curso: pesos + reescritura
void  ProporcionalRender(Viewport3D* vp);          // el circulo de influencia (overlay, durante el transform)
#endif // PROPORCIONAL_H
