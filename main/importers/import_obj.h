#ifndef IMPORTOBJ_H
#define IMPORTOBJ_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#include <string>
#include <fstream>
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    #include <map>
#else
    #include <GL/gl.h>
    #include <unordered_map>
#endif

#ifdef __ANDROID__
#endif
#include "objects/Mesh.h"

struct VertexKey {
    int pos, normal, uv, color;
    bool operator==(const VertexKey &other) const;
    bool operator<(const VertexKey &other) const; // para std::map (RVCT)
};

#ifndef W3D_SYMBIAN
namespace std {
    template <>
    struct hash<VertexKey> {
        size_t operator()(const VertexKey &k) const;
    };
}
#endif

// Clase Wavefront
class Wavefront {
public:
    std::vector<GLfloat> vertex;
    std::vector<GLubyte> vertexColor;
    std::vector<GLubyte> cornerColors; // palette de color POR ESQUINA (lineas 'vc' del .obj)
    std::vector<GLbyte> normals;
    std::vector<GLfloat> uv;
    std::vector<Face> faces;
    std::vector<int> looseEdges; // pares de indices de POSICION (0-based local) de las lineas 'l' (aristas sin cara)
    int facesSize;
    int facesCount;
    std::vector<MaterialGroup> materialsGroup;

    Wavefront() { Reset(); }

    void Reset();
    // vertToCP (opcional): por cada vertice de render que se crea, empuja el indice de CONTROL-POINT (fc.vertex) del
    // que salio. Lo usa el importador FBX para mapear los pesos del skin (indexados por control-point) a los vertices
    // de render (weight paint). NULL = no se arma (OBJ normal no lo necesita).
    void ConvertToES1(Mesh* TempMesh, int* acumuladoVertices, int* acumuladoNormales, int* acumuladoUVs,
                      std::vector<int>* vertToCP = NULL);
    void ConvertToES1_NoMerge(Mesh* TempMesh);
};

// extraer nombre base del filename (sin path ni extensión)
std::string ExtractBaseName(const std::string& filepath);

// Lee UN objeto desde las lineas ya cargadas (idx avanza). Robusto: sin
// tellg/seekg (que en modo texto fallaba el split multi-objeto).
bool LeerOBJ(const std::vector<const char*>& lines, // punteros a un buffer del archivo (sin std::string por linea)
             size_t& idx,
             const std::string& filename,
             int* acumuladoVertices,
             int* acumuladoNormales,
             int* acumuladoUVs,
             int* acumuladoColores,
             bool NoMerge);

// Función para leer archivos MTL y cargar materiales
bool LeerMTL(const std::string& filepath, int objetosCargados);

// Función principal para importar un OBJ
bool ImportOBJ(const std::string& filepath, bool NoMerge);

// encola una textura (material, ruta) para la carga diferida (1 por frame). Compartida con el importador FBX.
void EncolarTextura(Material* mat, const std::string& path);

// detecta si un mesh IMPORTADO (con normales) es smooth o flat, para setear meshSmooth (sino al editar un vertice
// se recalcula todo flat). Se llama DESPUES de CalcularBordes (usa posRep). Compartida OBJ + FBX.
bool MeshShadingImportadoEsSmooth(Mesh* m);

// EXPORTAR a OBJ + MTL (geometria, normales, vertex color, UV, texturas y los
// extras de material de Whisk3D). selectedOnly = solo los objetos seleccionados.
// applyModifiers = exporta la malla GENERADA por los modificadores (mirror, etc.) en vez de la editable.
// applyTransforms = hornea el transform del objeto (posiciones/normales a MUNDO); si false, exporta en LOCAL.
// AMBOS afectan SOLO el archivo exportado (la escena queda intacta). Escribe el .mtl al lado. false si no hay mallas.
bool ExportOBJ(const std::string& filepath, bool selectedOnly, bool applyModifiers = true, bool applyTransforms = true);

// CARGA DIFERIDA DE TEXTURAS: el import encola las texturas del MTL (no las decodifica) -> el modelo aparece
// enseguida. El loop principal (PC main.cpp + Symbian DrawCallBack) llama esto 1 vez por frame: decodifica+sube
// UNA textura por frame (hilo principal) y la asigna a su material. No-op si la cola esta vacia.
void CargarTexturasPendientes();

#endif