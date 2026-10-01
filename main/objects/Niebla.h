#ifndef NIEBLA_H
#define NIEBLA_H
#include "objects/Objects.h"

// ============================================================================
//  Objeto NIEBLA (fog): la llamada de niebla de OpenGL, puesta en el ARBOL.
//
//  Funciona como las luces "en orden": el recorrido de la escena la aplica en
//  el momento en que llega a ella, y de AHI EN ADELANTE todo lo que se dibuja
//  sale con esa niebla, hasta la proxima Niebla del arbol. Se pueden poner
//  varias en distintas partes del outliner (ej: una niebla densa para el
//  escenario y una Niebla apagada antes del HUD 3D, o distinta por zona).
//
//  SOLO EN MODO RENDER (como las luces de escena): en Solid / Material /
//  Wireframe no hace nada, asi se sigue viendo lo que se edita. El pase de la
//  escena la APAGA al empezar y al terminar (nada se escapa al overlay ni a la UI).
//
//  Propiedades (el panel y el .w3d hablan de lo mismo):
//    activa     false = APAGA la niebla desde aca (glDisable(GL_FOG))
//    modo       0 lineal (inicio/fin) | 1 exp | 2 exp2 (densidad)
//    inicio/fin distancia a la camara donde empieza / donde ya es todo niebla (lineal)
//    densidad   exp / exp2: f = e^-(d*z) / e^-(d*z)^2
//    color      RGB de la niebla
//    fondo      true = el FONDO del render toma el color de la niebla (estilo de
//               muchos juegos: la niebla y el fondo son el mismo color). Rige desde el frame siguiente.
//
//  Costo: GL fijo calcula la niebla por VERTICE (en el N95 es casi gratis); el
//  backend GLES2 la replica en el shader. Un objeto Niebla oculto (ojito) no hace nada.
// ============================================================================
class Niebla : public Object {
public:
    bool  activa;
    int   modo;          // 0 lineal, 1 exp, 2 exp2
    float inicio, fin;
    float densidad;
    float color[4];
    bool  fondo;

    Niebla(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ObjectType getType() W3D_OVERRIDE { return ObjectType::niebla; }
    void RenderObject() W3D_OVERRIDE;
};

// el pase de la escena: apaga la niebla al empezar y al terminar (la deja como la encontro la UI)
void W3dNieblaReiniciar();

#endif
