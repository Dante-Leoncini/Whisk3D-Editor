#ifndef MESH_H
#define MESH_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <GL/gl.h>
#include <GL/glext.h>

#include "UI/colores.h"
#include "UI/icons.h"
#include "Primitivas.h"
#include "Materials.h"
#include "Objects.h"

// ===================================================
// Enumeraciones y estructuras auxiliares
// ===================================================
enum class MeshType { cube, UVsphere, IcoSphere, plane, vertice, circle };

class VertexGroup { 
public:
    std::vector<int> indices;
};

class MaterialGroup { 
public:
    int start = 0;              // índice del primer triángulo real
    int count = 0;              // cantidad de triángulos reales
    int startDrawn = 0;         // índice del primer triángulo a dibujar
    int indicesDrawnCount = 0;  // cantidad de vertices a dibujar
    Material* material = nullptr;
};

// ===================================================
// Clase Mesh
// ===================================================
class Mesh : public Object { 
public:
    int vertexSize = 0;
    GLfloat* vertex = nullptr;

    std::vector<VertexGroup> vertexGroups;
    GLubyte* vertexColor = nullptr;
    GLbyte* normals = nullptr;
    GLfloat* uv = nullptr;

    int facesSize = 0;
    GLushort* faces = nullptr;
    std::vector<MaterialGroup> materialsGroup;

    Mesh(Object* parent = nullptr, GLfloat px = 0.0f, GLfloat py = 0.0f, GLfloat pz = 0.0f);

    ~Mesh();

    ObjectType getType() override;

    void LiberarMemoria();
    void RenderObject() override;
};

// ===================================================
// Funciones globales
// ===================================================
Object* NewMesh(MeshType type = MeshType::cube, Object* parent = nullptr, bool query = false);
int DuplicateMesh(int meshIndex);

#endif