#ifndef IMPORTOBJ_H
#define IMPORTOBJ_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <GL/gl.h>
#include <string>
#include <fstream>
#include <unordered_map>

#include <filesystem>

#ifdef __ANDROID__
//#include "render/GLES_Android_helpers.h"
#endif
#include "objects/Mesh.h"

struct VertexKey {
    int pos, normal, uv, color;
    bool operator==(const VertexKey &other) const;
};

namespace std {
    template <>
    struct hash<VertexKey> {
        size_t operator()(const VertexKey &k) const;
    };
}

// Clase Wavefront
class Wavefront {
public:
    std::vector<GLfloat> vertex;    
    std::vector<GLubyte> vertexColor;
    std::vector<GLbyte> normals;
    std::vector<GLfloat> uv;
    std::vector<Face> faces;
    int facesSize;
    int facesCount;
    std::vector<MaterialGroup> materialsGroup;

    Wavefront() { Reset(); }

    void Reset();
    void ConvertToES1(Mesh* TempMesh, int* acumuladoVertices, int* acumuladoNormales, int* acumuladoUVs);
    void ConvertToES1_NoMerge(Mesh* TempMesh);
};

// extraer nombre base del filename (sin path ni extensión)
std::string ExtractBaseName(const std::string& filepath);

// Función para leer un OBJ desde un ifstream
bool LeerOBJ(std::ifstream& file,
             const std::string& filename,
             std::streampos& startPos,
             int* acumuladoVertices,
             int* acumuladoNormales,
             int* acumuladoUVs,
             bool NoMerge);

// Función para leer archivos MTL y cargar materiales
bool LeerMTL(const std::string& filepath, int objetosCargados);

// Función principal para importar un OBJ
bool ImportOBJ(const std::string& filepath, bool NoMerge);

#endif