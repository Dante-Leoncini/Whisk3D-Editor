#ifndef CAMERA_H
#define CAMERA_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include "variables.h"
#include "UI/icons.h"
#include "UI/colores.h"
#include "Target.h"
#include "Curve.h"
#include "Objects.h"
//#include "math/Vector3.h"
#include <GL/gl.h>

// Constantes para la geometría de la cámara
static const int CameraVertexSize = 8 * 3;
static const int CameraEdgesSize = 11 * 2;

extern GLfloat CameraVertices[CameraVertexSize];
extern const GLushort CameraFaceActive[3];
extern const GLushort CameraEdges[CameraEdgesSize];

// Forward declarations
class Camera;

extern Camera* CameraActive;

class Camera : public Object, public Target {
    public:
        std::string RielName = "";
        int offsetRiel = 0;
        Curve* Riel = nullptr;

        // ¡Nuevas variables para guardar los ejes ya calculados!
        Vector3 rightVector;
        Vector3 forwardVector;

        Camera(Object* parent = nullptr, Vector3 pos = Vector3(0,0,0), Vector3 Rot = Vector3(0,0,0));

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