#ifndef CAMERA_H
#define CAMERA_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include "variables.h"
#include "UI/icons.h"
#include "UI/colores.h"
#include "Objects/Objects.h"
#include <GL/gl.h>

// Constantes para la geometría de la cámara
static const int CameraVertexSize = 8 * 3;
static const int CameraEdgesSize = 11 * 2;

extern GLfloat CameraVertices[CameraVertexSize];
extern const GLshort CameraFaceActive[3];
extern const GLushort CameraEdges[CameraEdgesSize];

extern Object* CameraActive;

class Camera : public Object {
public:
    Camera(Object* parent = nullptr, GLfloat x = 0.0f, GLfloat y = 0.0f, GLfloat z = 0.0f,
           GLfloat rx = 0.0f, GLfloat ry = 0.0f, GLfloat rz = 0.0f, GLfloat scale = 1.0f);

    ObjectType getType() override;

    void RenderObject() override;

    ~Camera();
};

#endif