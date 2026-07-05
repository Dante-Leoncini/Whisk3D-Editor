#ifndef CAMERA_H
#define CAMERA_H

#ifdef _WIN32
    #define NOMINMAX
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "Target.h"
#include "Curve.h"
#include "objects/Objects.h"
#include "WhiskUI/icons.h" // portable: iconos compartidos
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include "variables.h"
    #include "WhiskUI/colores.h"
    #include <GL/gl.h>
#endif

// Constantes para la geometría de la cámara
static const int CameraVertexSize = 8 * 3;
static const int CameraEdgesSize = 11 * 2;

extern GLfloat CameraVertices[CameraVertexSize];
extern const MeshIndex CameraFaceActive[3];
extern const GLushort CameraEdges[CameraEdgesSize];

// Forward declarations
class Camera;

extern Camera* CameraActive;

class Camera : public Object, public Target {
    public:
        std::string RielName;   // (inicializados en el constructor: C++03)
        int offsetRiel;
        Curve* Riel;

        // ¡Nuevas variables para guardar los ejes ya calculados!
        Vector3 rightVector;
        Vector3 forwardVector;

        Camera(Object* parent = NULL, Vector3 pos = Vector3(0,0,0), Vector3 Rot = Vector3(0,0,0));

        ObjectType getType() override;

        void Reload() override;
        
        void UpdateLookAt();

        void RenderObject() override;

        void UpdatePosition();

        void SetRiel(const std::string& NewValue);
        void ReloadRiel(Object* me);

        ~Camera();
};

#endif