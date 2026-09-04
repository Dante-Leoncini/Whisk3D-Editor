#ifndef POLYMESH_H
#define POLYMESH_H

#include <vector>
#include <utility>
#include "math/Vector3.h"

class Mesh;

// ============================================================================
//  POLYMESH: la malla de POLIGONOS sobre la que corre el stack de modificadores
//  (Subdivision, Screw, Mirror, Boolean). Es la representacion intermedia del
//  EDITOR: caras de N lados con uv y color POR CORNER, sobre un pool de posiciones
//  topologicas (unicas por lugar). Al final del stack se baja a render-verts; al
//  hacer Apply se baja a faces3d MANTENIENDO los poligonos (quads/ngons intactos).
//  Vivia adentro de MeshEdit.cpp; salio a un header para que el Boolean (que es
//  un archivo propio, por tamano) pueda operar sobre ella.
// ============================================================================
struct PolyMesh {
    std::vector<Vector3>                    P;    // posiciones topologicas (unicas por lugar)
    std::vector<std::vector<int> >          F;    // caras: indices a P
    std::vector<int>                        Fmat; // material (mesh part) por cara
    std::vector<int>                        Fsmooth; // shading por cara: -1 hereda (global / del modificador), 0 flat, 1 smooth
    std::vector<std::vector<float> >        Fuv;  // uv POR CORNER (2 por corner)
    std::vector<std::vector<unsigned char> >Fcol; // color POR CORNER (4 por corner)
    std::vector<std::pair<int,int> >        E;    // aristas topologicas (perfil) -> las usa el Screw para barrer
    int                                     Emat; // material para las caras que genera el Screw desde las aristas
    PolyMesh() : Emat(0) {}
};

// arma la PolyMesh desde faces3d de la malla (dedupe de verts de render por posicion).
// Def en MeshEdit.cpp.
void ConstruirPolyMesh(Mesh* m, PolyMesh& W);
// shading de la cara f (tolera un PolyMesh sin Fsmooth: -1 = hereda)
inline int PolySmoothDe(const PolyMesh& W, size_t f) { return (f < W.Fsmooth.size()) ? W.Fsmooth[f] : -1; }

#endif // POLYMESH_H
