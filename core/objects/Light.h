#ifndef LIGHT_H
#define LIGHT_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>
#include <vector>

//#include "variables.h" 
#include "GeometriaUI/GeometriaUI.h"
#include "UI/icons.h" 
#include "UI/colores.h"
#include "objects/Objects.h" 
#include "render/OpcionesRender.h" 

class Light : public Object {
public:
    GLenum LightID;
    GLfloat position[4];
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4];

    static Light* Create(Object* parent = nullptr, GLfloat x = 0, GLfloat y = 0, GLfloat z = 0);

    ObjectType getType() override;

    void SetDiffuse(GLfloat r = 1.0f, GLfloat g = 1.0f, GLfloat b = 1.0f);
    void SetLightID(GLenum ID);
    void RenderObject() override;

    ~Light();

private:
    Light(Object* parent, GLfloat x, GLfloat y, GLfloat z);
};

// Contenedor global de luces
extern std::vector<Light*> Lights;

// Máximo de luces
const int MAX_LIGHTS = 8;

#endif