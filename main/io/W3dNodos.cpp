#include "io/W3dNodos.h"
#include <math.h>                  // floorf (celda = 1 + floor(indice), como la VisZona)
#include "objects/Mesh.h"
#include "objects/Objects.h"
#include "objects/Camera.h"        // CameraActive + FindObjectByName (riel de la camara)
#include "objects/Curve.h"         // el path "de verdad": el riel (FindNearest)
#include "objects/Culling.h"       // metodo Riel: hijos-objetos por nodo (listaRender cacheada)
#include "edit/Modifier.h"         // pathNombre / soloCamaraActiva / sectorPVS
#include "edit/MeshEdit.h"         // W3dPVSSincronizar (materializa el sector, idempotente)
#include "UI/ViewPorts/ViewPort3D.h" // Viewport3DActive->VistaCam(): la vista libre del editor

// nodo (0-based) mas cercano al ojo sobre el path. Curve: FindNearest (lineal, ~1000 nodos
// = trivial). Malla de aristas (Add > Path): lineal sobre sus vertices, mismo criterio.
// El ojo se lleva al espacio del path restando su posicion global (mismo contrato que
// Camera::UpdatePosition con el riel: traslacion, sin rotacion).
static int NodoMasCercano(Object* path, const Vector3& ojo, const std::vector<char>* ramasOn) {
    Vector3 local = ojo - path->GetGlobalPosition();
    if (path->getType() == ObjectType::curve)
        return ((Curve*)path)->FindNearestFiltrado(local, ramasOn);   // solo las ramas habilitadas
    if (path->getType() == ObjectType::mesh) {
        Mesh* pm = (Mesh*)path;
        if (!pm->vertex || pm->vertexSize < 1) return -1;
        int mejor = -1; float md = 3.4e38f;
        for (int i = 0; i < pm->vertexSize; i++) {
            float dx = pm->vertex[i*3] - local.x, dy = pm->vertex[i*3+1] - local.y, dz = pm->vertex[i*3+2] - local.z;
            float d = dx*dx + dy*dy + dz*dz;
            if (d < md) { md = d; mejor = i; }
        }
        return mejor;
    }
    return -1;
}

// El nodo del path para un OJO que es LA CAMARA ACTIVA: si la camara viaja sobre ESE
// mismo riel, su rielIndice es la verdad exacta (el que UpdatePosition dejo al pararse;
// mismo contrato que la VisZona vieja: celda = 1 + floor(indice)). La busqueda por
// cercania queda de fallback (camara en otro riel, vista libre, path de aristas): la
// posicion real lleva offsetRiel y el nearest puede errarle por nodos ENTEROS justo
// en los bordes fino/lejos de la SLST -> triangulos de mas y agujeros.
static int NodoDeCamaraEnPath(Object* path) {
    if (!CameraActive || !CameraActive->Riel) return -1;
    if ((Object*)CameraActive->Riel != path) return -1;
    float idx = CameraActive->rielIndice;
    if (idx < 0.0f) return -1;
    return (int)floorf(idx);
}

static bool OclusionPaso(Object* o) {
    bool cambio = false;
    if (!o) return false;
    // CULLING metodo Riel: la celda del nodo dice que HIJOS-objetos dibujar (estilo NSD).
    // Mismo criterio de ojo que el modificador: jugando manda la camara activa; con
    // soloCamaraActiva apagado manda la vista libre (demo en vivo).
    if (o->getType() == ObjectType::culling) {
        Culling* cu = (Culling*)o;
        if (cu->metodo == Culling::Riel && !cu->rielNombre.empty()) {
            extern bool PlayAnimation;
            bool porCamara = (cu->soloCamaraActiva || PlayAnimation);
            bool tengoOjo = false; Vector3 ojo;
            if (porCamara) {
                if (CameraActive) { ojo = CameraActive->GetGlobalPosition(); tengoOjo = true; }
            } else if (Viewport3DActive) {
                ojo = Viewport3DActive->VistaCam().pos; tengoOjo = true;
            }
            if (tengoOjo) {
                Object* riel = FindObjectByName(SceneCollection, cu->rielNombre);
                if (riel && riel->getType() == ObjectType::curve) {
                    int nodo = porCamara ? NodoDeCamaraEnPath(riel) : -1;
                    if (nodo < 0)
                        nodo = ((Curve*)riel)->FindNearestFiltrado(ojo - riel->GetGlobalPosition(), NULL);
                    if (nodo >= 0 && cu->RielAplicarNodo(nodo + 1)) cambio = true;
                }
            }
        }
    }
    if (o->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)o;
        for (size_t k = 0; k < m->modificadores.size(); k++) {
            Modifier* mod = m->modificadores[k];
            if (!mod || mod->tipo != ModifierType::CullingTri || mod->pathNombre.empty()) continue;
            if (!mod->mostrarViewport) continue;                     // modificador apagado: no elige nada
            // el OJO que manda (ver W3dNodos.h): camara activa jugando, o la vista libre
            extern bool PlayAnimation;
            bool porCamara = false;
            bool tengoOjo = false; Vector3 ojo;
            if (mod->soloCamaraActiva) {
                if (PlayAnimation && CameraActive) { ojo = CameraActive->GetGlobalPosition(); tengoOjo = true; porCamara = true; }
                // en pausa: sin auto (el nodo se edita a mano en la card del modificador)
            } else if (Viewport3DActive) {
                ojo = Viewport3DActive->VistaCam().pos; tengoOjo = true;   // volar libre = demo en vivo
            }
            if (!tengoOjo) continue;
            Object* path = FindObjectByName(SceneCollection, mod->pathNombre);
            if (!path) continue;                                     // path borrado/renombrado: queda el sector manual
            int nodo = porCamara ? NodoDeCamaraEnPath(path) : -1;
            if (nodo < 0) nodo = NodoMasCercano(path, ojo, &mod->ramasOn);
            if (nodo < 0) continue;
            if (mod->sectorPVS != nodo + 1) {                        // sectores 1-based (0 = malla completa)
                mod->sectorPVS = nodo + 1;
                W3dPVSSincronizar(m);                                // idempotente: materializa solo si cambio
                cambio = true;
            }
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (OclusionPaso(o->Childrens[i])) cambio = true;
    return cambio;
}

bool W3dOclusionTick() {
    return OclusionPaso(SceneCollection);
}
