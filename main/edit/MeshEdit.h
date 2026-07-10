#ifndef MESHEDIT_H
#define MESHEDIT_H

#include <vector>
#include "objects/Mesh.h" // Mesh, UVMap, ColorLayer, CornerSrc

// Edicion de MESH PARTS (materialsGroup): son del EDITOR, no del render. Editan
// faces3d.mat + materialsGroup y rehacen el index buffer (Mesh::ReagruparMeshParts).
int  NuevoMeshPart(Mesh* m);                    // agrega un mesh part vacio; devuelve su indice
void BorrarMeshPart(Mesh* m, int idx);          // borra; huerfanas -> anterior; siempre queda >=1
void MoverMeshPart(Mesh* m, int idx, int dir);  // reordena (dir -1 sube/+1 baja) = orden de dibujado

// CAPAS por-corner (UV maps / color layers): ops de edicion que duplican, reordenan
// o reconstruyen los datos de todas las capas de la malla (no son del render).
void DuplicarUVMapActivo(Mesh* m);
void DuplicarColorLayerActivo(Mesh* m);
void BorrarUVMapActivo(Mesh* m);       // borra la UV map activa (queda >=1)
void MoverUVMapActivo(Mesh* m, int dir);   // reordena la UV map activa (dir=-1 up / +1 down)
void BorrarColorLayerActivo(Mesh* m);  // borra la capa de color activa (queda >=1)
void MoverColorLayerActivo(Mesh* m, int dir);
void CrearVertexGroup(Mesh* m);        // crea un grupo de vertices vacio (nombre unico) y lo deja activo
void BorrarVertexGroupActivo(Mesh* m); // borra el grupo de vertices activo (puede quedar 0)
void MoverVertexGroupActivo(Mesh* m, int dir);
void ReverseCapasDeCorner(Mesh* m, int L, int count);
void AgregarCornerCapas(Mesh* m, int srcL);
void CompactarCapas(Mesh* m, const std::vector<int>& survCorner);
void ReconstruirCapasDesde(Mesh* m, const std::vector<CornerSrc>& src);

#endif // MESHEDIT_H
