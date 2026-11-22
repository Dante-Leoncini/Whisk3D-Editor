#ifndef MESHEDIT_H
#define MESHEDIT_H

#include <vector>
#include "objects/Mesh.h" // Mesh, UVMap, ColorLayer, CornerSrc

// Edicion de MESH PARTS (materialsGroup): son del EDITOR, no del render. Editan
// faces3d.mat + materialsGroup y rehacen el index buffer (Mesh::RegroupMeshParts).
int  NewMeshPart(Mesh* m);                     // agrega un mesh part vacio; devuelve su indice
void DeleteMeshPart(Mesh* m, int idx);         // borra; huerfanas -> anterior; siempre queda >=1
void MoveMeshPart(Mesh* m, int idx, int dir);  // reordena (dir -1 sube/+1 baja) = orden de dibujado

// CAPAS por-corner (UV maps / color layers): ops de edicion que duplican, reordenan
// o reconstruyen los datos de todas las capas de la malla (no son del render).
void DuplicateActiveUVMap(Mesh* m);
void DuplicateActiveColorLayer(Mesh* m);
void DeleteActiveUVMap(Mesh* m);       // borra la UV map activa (queda >=1)
void MoveActiveUVMap(Mesh* m, int dir);   // reordena la UV map activa (dir=-1 up / +1 down)
void DeleteActiveColorLayer(Mesh* m);  // borra la capa de color activa (queda >=1)
void MoveActiveColorLayer(Mesh* m, int dir);
// VERTEX GROUPS (pesos por CONTROL-POINT: viewport 3D + armature 3D)
void CreateVertexGroup(Mesh* m);        // crea un grupo de vertices vacio (nombre unico) y lo deja activo
// RENAME de un vertex group: uniquifica en la malla Y arrastra el HUESO 3D homonimo del rig
// ligado (binding POR NOMBRE), en UN SOLO paso de undo. Devuelve el nombre que quedo.
std::string VertexGroupRename(Mesh* m, int idx, const std::string& request);
void DeleteActiveVertexGroup(Mesh* m); // borra el grupo de vertices activo (puede quedar 0)
void MoveActiveVertexGroup(Mesh* m, int dir);
// UV GROUPS (pesos por CORNER: editor UV + armature 2D). Entidad DISTINTA de los vertex groups
// (ver el bloque VertexGroup/UVGroup en Mesh.h): mismas ops, listas separadas.
void CreateUVGroup(Mesh* m, const std::string& base = "UV Group"); // vacio, nombre unico, queda activo
// RENAME de un UV group: uniquifica en la malla Y arrastra el HUESO 2D homonimo, en 1 paso de undo.
std::string UVGroupRename(Mesh* m, int idx, const std::string& request);
void DeleteActiveUVGroup(Mesh* m);
void MoveActiveUVGroup(Mesh* m, int dir);
void ReverseCornerLayers(Mesh* m, int L, int count);
void AddCornerLayers(Mesh* m, int srcL);
void CompactLayers(Mesh* m, const std::vector<int>& survCorner);
void RebuildLayersFrom(Mesh* m, const std::vector<CornerSrc>& src);

// ===== PVS: modificador "Culling" POR TRIANGULO (sectores precalculados estilo PS1) =====
// Los datos viven en el sidecar <origen sin ext>.pvs.json = {"sectores": M, "tris": [[...]]}
// (tri = indice 0-based en el ORDEN DE CARAS del OBJ). Ver el bloque en MeshEdit.cpp.
class Modifier;
bool W3dPVSLoad(Mesh* m, Modifier* mod);    // lee el sidecar -> mod->pvsSectors (tolerante)
void W3dPVSSync(Mesh* m);                   // stack+sector deseado -> override de indices armado (idempotente)
void W3dPVSInvalidate(Mesh* m);             // faces[] cambio (reagrupado/topologia): re-armar el sector activo
bool W3dPVSApplySector(Mesh* m, int s);     // fija el sector activo (1-based; 0/fuera de rango = completa)
bool W3dPVSSetSectorObj(Object* o, int s);  // wrapper del bind lua setSector (valida tipo mesh)
int  W3dPVSRecalculate(Mesh* m);            // re-lee el sidecar + re-arma; devuelve sectores (-1 = sin modificador)

// ===== metodo "Celdas": el mismo modificador con un VisSet (`.w3dvis`, formato/w3dvis.md) =====
// Celdas de a miles, listas CON ORDEN (far->near = orden del pintor) y deltas entre
// vecinas. El CONTRATO con el render no cambia: el resultado es Mesh::pvsFaces/pvsGroups
// (la lista de la celda, materializada por mesh part) + Mesh::pvsVersion como contador.
bool W3dVisSetCell(Mesh* m, int cell);    // = W3dPVSApplySector con el nombre del contrato
int  W3dVisInfo(Mesh* m, int* activeCell, int* trisList, bool* sorted); // nCells (-1 = sin metodo celdas)

// el STACK de la malla GENERA malla? = hay algun modificador que corre (arma poligonos con
// BuildPolyMesh): ni el Armature (deforma por frame) ni el PVS/Oclusion (elige indices), y
// encendido en el viewport (en Edit Mode, ademas, en edit). La MISMA regla que usa el stack; la
// carga del juego la usa para saber si una malla .w3db necesita sus bloques de edicion ya.
bool W3dStackGeneratesMesh(const Mesh* m);

// CONNECT VERTEX PATH (tecla J / menu Vertex). Con 2 vertices seleccionados (el activo es el
// destino) traza la linea RECTA entre los dos EN PANTALLA y la corta sobre la superficie: cada
// cara que la linea atraviesa se parte en dos, y donde cruza una arista se crea un vertice nuevo
// sobre esa arista (interpolado: uv, color, normal y vertex anim como el loop cut). Si la linea
// pasa justo por un vertice se engancha en el. Si ya estan unidos por una arista, no hace nada.
// proy: vertice (posicion LOCAL) -> pantalla; NULL = vista frontal ortografica (x, y), para tests.
// msg = aviso para el usuario (clave de traduccion). Deja paso de undo y la seleccion en el corte.
typedef bool (*W3dProj2D)(void* ctx, const Vector3& local, float& sx, float& sy);
bool ConnectVerticesEdit(Mesh* m, W3dProj2D proj, void* ctx, std::string& msg);

// render con un vert POR CORNER (sin fusionar corners iguales): lo prende el editor mientras se pinta
// vertex color por corner, para que cada corner se pueda pintar y VER por separado. Vale para el
// proximo GenerateRender de cualquier malla; apagarlo y regenerar vuelve a fusionar.
void W3dRenderCornersSeparated(bool on);
bool W3dRenderSeparatedCornersActive();

// cara (ngon) -> triangulos para un index buffer: abanico si es convexa, ear clipping si es concava
// (la "U" que deja el Boolean alrededor de un agujero). pos = float[3] por indice. Siempre m-2 triangulos. Lo usan el
// index buffer del render, el loop cut, el export glTF y el snap a cara: TODOS ven la misma cara.
void W3dTriangulateFace(const float* pos, const std::vector<int>& idx, std::vector<MeshIndex>& tris);

// las opciones de INSET FACES (Mesh::InsetEdit), las del panel "Inset Faces". Por defecto, las de Blender
struct InsetOptions {
    float thickness;    // Thickness: cuanto se corre el contorno hacia adentro
    float depth;        // Depth: cuanto sube (o baja) lo de adentro por la normal
    bool border;        // Boundary: tambien el contorno abierto de la malla (sin: se desliza por el)
    bool even;          // Offset Even: el grosor parejo en las esquinas (no se afina en los angulos)
    bool relative;      // Offset Relative: el grosor escala con el largo de las aristas de alrededor
    bool rail;          // Edge Rail: el vertice nuevo corre por la arista de adentro que ya habia
    bool outside;       // Outset: hacia afuera en vez de hacia adentro
    bool selOutside;    // Select Outer: queda seleccionado el anillo nuevo, no lo de adentro
    bool individual;    // Individual: cada cara por su lado (no la region entera)
    bool interpolate;   // Interpolate: los UV / colores de lo de adentro se achican con la cara
    InsetOptions() : thickness(0.0f), depth(0.0f), border(true), even(true), relative(false), rail(false),
                      outside(false), selOutside(false), individual(false), interpolate(true) {}
};

// las opciones del BEVEL (Mesh::BevelEdit), las del panel "Bevel". Por defecto, las de Blender
struct BevelOptions {
    int affects;        // Affect: 0 = vertices, 1 = aristas
    int widthType;      // Width Type: 0 Offset, 1 Width, 2 Depth, 3 Percent, 4 Absolute
    float width;        // Width
    int segments;       // Segments
    float profile;      // Profile Shape (0.5 = circular)
    int material;       // Material Index (-1 = el de la cara de al lado)
    bool harden;        // Harden Normals (aca: las caras nuevas, suaves)
    bool limitOverlap;  // Clamp Overlap: ningun punto corre mas de la mitad de su arista
    bool slide;         // Loop Slide: los puntos corren POR las aristas de al lado
    bool markSeams;     // Mark Seams: los bordes de las tiras
    bool markSharp;     // Mark Sharp: idem
    int miterOutside;   // Miter Outer: 0 Sharp, 1 Patch, 2 Arc (hoy: Sharp)
    int miterInside;    // Miter Inner: 0 Sharp, 1 Arc (hoy: Sharp)
    int intersection;   // Intersection Type: 0 Grid Fill, 1 Cutoff
    int forceFace;      // Face Strength: 0 None, 1 New, 2 Affected, 3 All (para Weighted Normal: hoy no hace nada)
    int profileType;    // Profile Type: 0 Superellipse, 1 Custom (hoy: superelipse)
    BevelOptions() : affects(1), widthType(0), width(0.0f), segments(1), profile(0.5f), material(-1), harden(false),
                      limitOverlap(false), slide(true), markSeams(false), markSharp(false), miterOutside(0),
                      miterInside(0), intersection(0), forceFace(0), profileType(0) {}
};

// EL INDEX BUFFER CANONICO de una malla, SIN tocarla: el que armaria RegroupMeshParts si
// la malla se guardara en .w3dm y se volviera a abrir (triangulos por mesh part + Forsyth,
// con el MISMO codigo). Lo hornea el .w3db al guardar (io/W3dMeshBin.h): abrir no triangula.
struct W3dMeshBinIndices;
void W3dMeshCanonicalIndices(const Mesh* m, W3dMeshBinIndices& out);

#endif // MESHEDIT_H
