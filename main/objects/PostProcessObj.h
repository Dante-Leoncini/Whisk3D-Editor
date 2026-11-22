#ifndef POST_PROCESS_OBJ_H
#define POST_PROCESS_OBJ_H
// ============================================================================
//  Postproceso (objeto) — EL POSTPROCESADO puesto en el ARBOL, como la Niebla o el Limpiar Z: actua en el momento en que
//  el recorrido del render llega a el. Sus efectos (render/PostProcess.h: grano, desenfoque de campo, de movimiento,
//  bloom...) se aplican a lo que YA SE DIBUJO; lo que sigue en el arbol se dibuja limpio y ADELANTE (la profundidad se
//  borra, como en el Limpiar Z). Asi el outliner es el orden: un cubo, el postprocesado y despues la UI sin efectos; o
//  varios postprocesados distintos para distintas partes.
//  En el editor se ve en la vista Rendered mirando por la camara activa, adentro de su marco (ver
//  Viewport3D::RunPostProcess). El render a imagen los aplica, en el orden del arbol, sobre la imagen entera.
// ============================================================================
#include "objects/Objects.h"
#include "render/PostProcess.h"

class PostProcess : public Object {
public:
    W3dPostOptions opt;   // prendido + la pila de efectos (en orden) + el elegido en el panel
    PostProcess(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ObjectType getType() W3D_OVERRIDE { return ObjectType::postProcess; }
    void RenderObject() W3D_OVERRIDE;
};

// lo que hace el que DIBUJA cuando el recorrido llega a un Postproceso (lo pone el viewport del editor mientras dibuja
// su escena; NULL = nada, ej: el pick o el pase de datos del mismo postprocesado)
extern void (*g_postInTree)(PostProcess* p);

// los Postproceso del arbol, en el orden del recorrido (visibles)
void W3dPostProcessesOf(Object* root, std::vector<PostProcess*>& out);

#endif
