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

typedef GLshort Edge[2];

class FaceCorner {
	public:
		int vertex;
		int uv;
		int normal;
		//seria genial agregar color despues
};

class Face { 
	public:
		std::vector<FaceCorner> corners;
};

class VertexAnimation;

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
        std::string name = "Mesh";
        //podria ser triangulos GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN
        //lineas: GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP
        //puntos: GL_POINTS
        GLenum drawMode = GL_TRIANGLES;
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

        //animacion
        std::vector<VertexAnimation*> animations;   

        Mesh(Object* parent = nullptr, Vector3 pos = Vector3(0,0,0));

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

#include "animation/VertexAnimation.h"

#endif