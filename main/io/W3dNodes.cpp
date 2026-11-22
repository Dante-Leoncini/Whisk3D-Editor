#include "io/W3dNodes.h"
#include <math.h>                  // floorf (celda = 1 + floor(indice), como la VisZone)
#include "objects/Mesh.h"
#include "objects/Objects.h"
#include "objects/Camera.h"        // CameraActive + FindObjectByName (riel de la camara)
#include "objects/Curve.h"         // el path "de verdad": el riel (FindNearest)
#include "objects/Culling.h"       // metodo Riel: hijos-objetos por nodo (listRender cacheada)
#include "edit/Modifier.h"         // pathName / onlyCameraActive / sectorPVS
#include "edit/MeshEdit.h"         // W3dPVSSync (materializa el sector, idempotente)
#include "ViewPorts/ViewPort3D.h" // Viewport3DActive->ViewCam(): la vista libre del editor

// nodo (0-based) mas cercano al ojo sobre el path. Curve: FindNearest (lineal, ~1000 nodos
// = trivial). Malla de aristas (Add > Path): lineal sobre sus vertices, mismo criterio.
// El ojo se lleva al espacio del path restando su posicion global (mismo contrato que
// Camera::UpdatePosition con el riel: traslacion, sin rotacion).
static int NearestNode(Object* path, const Vector3& eye, const std::vector<char>* branchesOn) {
    Vector3 local = eye - path->GetGlobalPosition();
    if (path->getType() == ObjectType::curve)
        return ((Curve*)path)->FindNearestFiltered(local, branchesOn);   // solo las ramas habilitadas
    if (path->getType() == ObjectType::mesh) {
        Mesh* pm = (Mesh*)path;
        if (!pm->vertex || pm->vertexSize < 1) return -1;
        int best = -1; float md = 3.4e38f;
        for (int i = 0; i < pm->vertexSize; i++) {
            float dx = pm->vertex[i*3] - local.x, dy = pm->vertex[i*3+1] - local.y, dz = pm->vertex[i*3+2] - local.z;
            float d = dx*dx + dy*dy + dz*dz;
            if (d < md) { md = d; best = i; }
        }
        return best;
    }
    return -1;
}

// El nodo del path para un OJO que es LA CAMARA ACTIVA: si la camara viaja sobre ESE
// mismo riel, su railIndex es la verdad exacta (el que UpdatePosition dejo al pararse;
// mismo contrato que la VisZone vieja: celda = 1 + floor(indice)). La busqueda por
// cercania queda de fallback (camara en otro riel, vista libre, path de aristas): la
// posicion real lleva offsetRail y el nearest puede errarle por nodos ENTEROS justo
// en los bordes fino/lejos de la SLST -> triangulos de mas y agujeros.
static int CameraNodeInPath(Object* path) {
    if (!CameraActive || !CameraActive->Rail) return -1;
    if ((Object*)CameraActive->Rail != path) return -1;
    float idx = CameraActive->railIndex;
    if (idx < 0.0f) return -1;
    return (int)floorf(idx);
}

static bool OcclusionStep(Object* o) {
    bool change = false;
    if (!o) return false;
    // CULLING metodo Riel: la celda del nodo dice que HIJOS-objetos dibujar (estilo NSD).
    // Mismo criterio de ojo que el modificador: jugando manda la camara activa; con
    // onlyCameraActive apagado manda la vista libre (demo en vivo).
    if (o->getType() == ObjectType::culling) {
        Culling* cu = (Culling*)o;
        if (cu->method == Culling::Rail && !cu->railName.empty()) {
            extern bool PlayAnimation;
            bool byCamera = (cu->onlyCameraActive || PlayAnimation);
            bool haveEye = false; Vector3 eye;
            if (byCamera) {
                if (CameraActive) { eye = CameraActive->GetGlobalPosition(); haveEye = true; }
            } else if (Viewport3DActive) {
                eye = Viewport3DActive->ViewCam().pos; haveEye = true;
            }
            if (haveEye) {
                Object* rail = W3dFindNameFrom(cu, cu->railName, SceneCollection);   // (por scope)
                if (rail && rail->getType() == ObjectType::curve) {
                    int node = byCamera ? CameraNodeInPath(rail) : -1;
                    if (node < 0)
                        node = ((Curve*)rail)->FindNearestFiltered(eye - rail->GetGlobalPosition(), NULL);
                    if (node >= 0 && cu->RailApplyNode(node + 1)) change = true;
                }
            }
        }
    }
    if (o->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)o;
        for (size_t k = 0; k < m->modifiers.size(); k++) {
            Modifier* mod = m->modifiers[k];
            if (!mod || mod->type != ModifierType::CullingTri || mod->pathName.empty()) continue;
            if (!mod->showViewport) continue;                     // modificador apagado: no elige nada
            // el OJO que manda (ver W3dNodes.h): camara activa jugando, o la vista libre
            extern bool PlayAnimation;
            bool byCamera = false;
            bool haveEye = false; Vector3 eye;
            if (mod->onlyCameraActive) {
                if (PlayAnimation && CameraActive) { eye = CameraActive->GetGlobalPosition(); haveEye = true; byCamera = true; }
                // en pausa: sin auto (el nodo se edita a mano en la card del modificador)
            } else if (Viewport3DActive) {
                eye = Viewport3DActive->ViewCam().pos; haveEye = true;   // volar libre = demo en vivo
            }
            if (!haveEye) continue;
            Object* path = W3dFindNameFrom(m, mod->pathName, SceneCollection);   // (por scope)
            if (!path) continue;                                     // path borrado/renombrado: queda el sector manual
            int node = byCamera ? CameraNodeInPath(path) : -1;
            if (node < 0) node = NearestNode(path, eye, &mod->branchesOn);
            if (node < 0) continue;
            if (mod->sectorPVS != node + 1) {                        // sectores 1-based (0 = malla completa)
                mod->sectorPVS = node + 1;
                W3dPVSSync(m);                                // idempotente: materializa solo si cambio
                change = true;
            }
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (OcclusionStep(o->Childrens[i])) change = true;
    return change;
}

bool W3dOcclusionTick() {
    return OcclusionStep(SceneCollection);
}
