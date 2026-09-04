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
// VERTEX GROUPS (pesos por CONTROL-POINT: viewport 3D + armature 3D)
void CrearVertexGroup(Mesh* m);        // crea un grupo de vertices vacio (nombre unico) y lo deja activo
// RENAME de un vertex group: uniquifica en la malla Y arrastra el HUESO 3D homonimo del rig
// ligado (binding POR NOMBRE), en UN SOLO paso de undo. Devuelve el nombre que quedo.
std::string VertexGroupRenombrar(Mesh* m, int idx, const std::string& pedido);
void BorrarVertexGroupActivo(Mesh* m); // borra el grupo de vertices activo (puede quedar 0)
void MoverVertexGroupActivo(Mesh* m, int dir);
// UV GROUPS (pesos por CORNER: editor UV + armature 2D). Entidad DISTINTA de los vertex groups
// (ver el bloque VertexGroup/UVGroup en Mesh.h): mismas ops, listas separadas.
void CrearUVGroup(Mesh* m, const std::string& base = "UV Group"); // vacio, nombre unico, queda activo
// RENAME de un UV group: uniquifica en la malla Y arrastra el HUESO 2D homonimo, en 1 paso de undo.
std::string UVGroupRenombrar(Mesh* m, int idx, const std::string& pedido);
void BorrarUVGroupActivo(Mesh* m);
void MoverUVGroupActivo(Mesh* m, int dir);
void ReverseCapasDeCorner(Mesh* m, int L, int count);
void AgregarCornerCapas(Mesh* m, int srcL);
void CompactarCapas(Mesh* m, const std::vector<int>& survCorner);
void ReconstruirCapasDesde(Mesh* m, const std::vector<CornerSrc>& src);

// ===== PVS: modificador "Culling" POR TRIANGULO (sectores precalculados estilo PS1) =====
// Los datos viven en el sidecar <origen sin ext>.pvs.json = {"sectores": M, "tris": [[...]]}
// (tri = indice 0-based en el ORDEN DE CARAS del OBJ). Ver el bloque en MeshEdit.cpp.
class Modifier;
bool W3dPVSCargar(Mesh* m, Modifier* mod);  // lee el sidecar -> mod->pvsSectores (tolerante)
void W3dPVSSincronizar(Mesh* m);            // stack+sector deseado -> override de indices armado (idempotente)
void W3dPVSInvalidar(Mesh* m);              // faces[] cambio (reagrupado/topologia): re-armar el sector activo
bool W3dPVSAplicarSector(Mesh* m, int s);   // fija el sector activo (1-based; 0/fuera de rango = completa)
bool W3dPVSSetSectorObj(Object* o, int s);  // wrapper del bind lua setSector (valida tipo mesh)
int  W3dPVSRecalcular(Mesh* m);             // re-lee el sidecar + re-arma; devuelve sectores (-1 = sin modificador)

// ===== metodo "Celdas": el mismo modificador con un VisSet (`.w3dvis`, formato/w3dvis.md) =====
// Celdas de a miles, listas CON ORDEN (lejos->cerca = orden del pintor) y deltas entre
// vecinas. El CONTRATO con el render no cambia: el resultado es Mesh::pvsFaces/pvsGroups
// (la lista de la celda, materializada por mesh part) + Mesh::pvsVersion como contador.
bool W3dVisSetCelda(Mesh* m, int celda);    // = W3dPVSAplicarSector con el nombre del contrato
int  W3dVisInfo(Mesh* m, int* celdaActiva, int* trisLista, bool* ordenado); // nCeldas (-1 = sin metodo celdas)

// CONNECT VERTEX PATH (tecla J / menu Vertex). Con 2 vertices seleccionados (el activo es el
// destino) traza la linea RECTA entre los dos EN PANTALLA y la corta sobre la superficie: cada
// cara que la linea atraviesa se parte en dos, y donde cruza una arista se crea un vertice nuevo
// sobre esa arista (interpolado: uv, color, normal y vertex anim como el loop cut). Si la linea
// pasa justo por un vertice se engancha en el. Si ya estan unidos por una arista, no hace nada.
// proy: vertice (posicion LOCAL) -> pantalla; NULL = vista frontal ortografica (x, y), para tests.
// msg = aviso para el usuario (clave de traduccion). Deja paso de undo y la seleccion en el corte.
typedef bool (*W3dProy2D)(void* ctx, const Vector3& local, float& sx, float& sy);
bool ConectarVerticesEdit(Mesh* m, W3dProy2D proy, void* ctx, std::string& msg);

// render con un vert POR CORNER (sin fusionar corners iguales): lo prende el editor mientras se pinta
// vertex color por corner, para que cada corner se pueda pintar y VER por separado. Vale para el
// proximo GenerarRender de cualquier malla; apagarlo y regenerar vuelve a fusionar.
void W3dRenderCornersSeparados(bool on);
bool W3dRenderCornersSeparadosActivo();

// cara (ngon) -> triangulos para un index buffer: abanico si es convexa, ear clipping si es concava
// (la "U" que deja el Boolean alrededor de un agujero). pos = float[3] por indice. Siempre m-2 triangulos. Lo usan el
// index buffer del render, el loop cut, el export glTF y el snap a cara: TODOS ven la misma cara.
void W3dTriangularCara(const float* pos, const std::vector<int>& idx, std::vector<MeshIndex>& tris);

#endif // MESHEDIT_H
