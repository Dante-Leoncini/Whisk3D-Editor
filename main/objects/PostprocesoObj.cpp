// ============================================================================
//  PostprocesoObj.cpp — ver PostprocesoObj.h
// ============================================================================
#include "objects/PostprocesoObj.h"

void (*g_postEnArbol)(Postproceso* p) = 0;

Postproceso::Postproceso(Object* parent, Vector3 pos) : Object(parent, "Postproceso", pos) {
    sinTransformacion = true;   // (no se mueve: actua en su lugar del arbol, sin matriz propia)
    opc.on = true;
}

void Postproceso::RenderObject() {
    if (g_postEnArbol) g_postEnArbol(this);
}

void W3dPostprocesosDe(Object* raiz, std::vector<Postproceso*>& out) {
    if (!raiz || !raiz->visible) return;
    if (raiz->getType() == ObjectType::postproceso) out.push_back((Postproceso*)raiz);
    for (size_t i = 0; i < raiz->Childrens.size(); i++) W3dPostprocesosDe(raiz->Childrens[i], out);
}
