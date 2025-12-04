#ifndef MATERIALS_H
#define MATERIALS_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <GL/gl.h>
#include <string>

#include "UI/text.h"
#include "OpcionesRender.h"

// Declaración adelantada de Material
class Material;

// Vector global de materiales
extern std::vector<Material*> Materials;
extern Material* MaterialDefecto;

// ===================================================
// Clase Material
// ===================================================
class Material { 
public:
    bool textura = false;
    bool transparent = false;
    bool vertexColor = false;
    bool lighting = true;
    bool repeat = true;
    bool uv8bit = false;
    bool culling = true;
    int interpolacion = 0; // definir lineal en cpp
    GLuint textureID = 0;
    GLfloat diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat specular[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    Text* name = nullptr;

    Material(const std::string& nombre, bool MaterialDefecto = false, bool TieneVertexColor = false);
    ~Material();
};

// ===================================================
// Funciones auxiliares
// ===================================================
Material* BuscarMaterialPorNombre(const std::string& name);
int DuplicateMaterial(int srcId);

#endif