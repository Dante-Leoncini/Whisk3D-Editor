#ifndef CLIP_REGION_H
#define CLIP_REGION_H
#include "objects/Objects.h"
#include <string>

// ============================================================================
//  Objetos de COMPOSICION de la pantalla, puestos en el ARBOL (como la Niebla):
//  actuan en el momento en que el recorrido del render llega a ellos.
//
//  ClearZ — borra el BUFFER DE PROFUNDIDAD en ese punto del arbol. Todo lo que
//  se dibuje despues queda ADELANTE de lo anterior, sin importar la distancia:
//  un arma en primera persona, un personaje en una ventana de dialogo, un
//  objeto 3D encima de un fondo. Si hay un Recorte activo (padre), solo borra
//  adentro de su rectangulo.
//
//  Recorte — sus HIJOS se dibujan SOLO adentro de un rectangulo de la pantalla.
//  Todo en fracciones de la pantalla, con el ORIGEN EN EL CENTRO:
//    x, y        centro del rectangulo (0,0 = centro de la pantalla; +x derecha,
//                +y arriba; 0.5 = el borde)
//    ancho, alto fraccion de la pantalla (1 = toda)
//    camara      (opcional) nombre de una Camara: los hijos se dibujan DESDE esa
//                camara, con el aspecto del rectangulo (una ventana con otra
//                vista: dos personajes que hablan cada uno en su cuadro, un
//                retrovisor, una camara de seguridad). Sin camara solo RECORTA
//                (la misma vista de la escena, sin dibujar afuera del rectangulo).
//    clearZ    borra la profundidad adentro del rectangulo al entrar
//    fondo       pinta el rectangulo con 'color' al entrar (clearZ incluido)
//  Recortes anidados se intersectan. Un Recorte oculto (ojito) no dibuja nada
//  de lo que tiene adentro; uno inactivo dibuja sus hijos normalmente.
// ============================================================================
class ClearZ : public Object {
public:
    bool active;
    ClearZ(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ObjectType getType() W3D_OVERRIDE { return ObjectType::clearz; }
    void RenderObject() W3D_OVERRIDE;
};

class ClipRegion : public Object {
public:
    bool  active;
    float x, y, width, height;
    std::string camera;
    bool  clearZ;
    bool  background;
    float color[4];

    ClipRegion(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ObjectType getType() W3D_OVERRIDE { return ObjectType::clip; }
    void RenderObject() W3D_OVERRIDE {}
    void RenderChildren() W3D_OVERRIDE;
};

#endif
