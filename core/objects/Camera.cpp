#include "Camera.h"

// Geometría de la cámara
GLfloat CameraVertices[CameraVertexSize] = {    
    0, 0, 0, // origen
    0.35f*2,  0.144f*2,  0.192f*2,
    0.35f*2, -0.144f*2,  0.192f*2,
    0.35f*2, -0.144f*2, -0.192f*2,
    0.35f*2,  0.144f*2, -0.192f*2,
    0.35f*2,  0.17f*2,  0.128f*2,
    0.35f*2,  0.17f*2, -0.128f*2,
    0.35f*2,  0.31f*2,  0,
};

const GLushort CameraFaceActive[3] = {5, 6, 7};

const GLushort CameraEdges[CameraEdgesSize] = {
    0, 1,
    0, 2,
    0, 3,
    0, 4,
    1, 2,
    2, 3,
    3, 4,
    4, 1,
    5, 6,
    6, 7,
    7, 5,
};

Camera* CameraActive = nullptr;

// ------------------- MÉTODOS -------------------

Camera::Camera(Object* parent, Vector3 pos, Vector3 Rot)
    : Object(parent, "Camera", pos, Rot) 
{
    IconType = static_cast<size_t>(IconType::camera);

    if (!CameraActive){
        CameraActive = this;
    }
}

ObjectType Camera::getType() {
    return ObjectType::camera;
}

void Camera::Reload() {
    ReloadTarget(this);
}

#include <cmath> // Necesario para std::abs() y std::sqrt() si no está en Vector3.h

void Camera::UpdateLookAt() {
    if (!target) return;

    // 1. Calcular el vector de dirección (Pos -> Target)
    Vector3 toTarget = (target->pos - pos);
    // Asumimos que aquí manejas la distancia mínima (Dot/LengthSq)
    // if (toTarget.Dot(toTarget) < 0.000001f) return;
    
    Vector3 forward = toTarget.Normalized(); 

    Vector3 worldUp(0, 1, 0); 
    
    // --- Anti-Roll/Gimbal Lock Check ---
    // Si forward es vertical, usamos un worldUp alternativo (prevención de roll)
    if (std::abs(forward.Dot(worldUp)) > 0.999f) { 
        worldUp = Vector3(0, 0, 1);
    }

    // 2. Eje Right (X local)
    // Cross(forward, worldUp) para asegurar Yaw y ortogonalidad
    Vector3 right = Vector3::Cross(forward, worldUp).Normalized();

    // 3. Eje Up (Y local)
    // Cross(right, forward) para asegurar Pitch y ortogonalidad
    Vector3 up = Vector3::Cross(right, forward).Normalized();
    
    // -----------------------------------------------------
    // 4. ENSAMBLAJE DE LA MATRIZ FINAL (Corrección de Inversiones)
    // -----------------------------------------------------
    Matrix4 M;
    M.Identity();

    // Columna X (Right): Lo invertimos para corregir la Inversión Horizontal (Yaw)
    M.m[0]  = -right.x;   M.m[1]  = -right.y;   M.m[2]  = -right.z;
    
    // Columna Y (Up): Lo invertimos para corregir la Inversión Vertical (Pitch)
    M.m[4]  = -up.x;      M.m[5]  = -up.y;      M.m[6]  = -up.z;
    
    // Columna Z (Forward): Mantenemos la inversión que ya funcionaba bien (la que mira a -Z)
    M.m[8]  = -forward.x; M.m[9]  = -forward.y; M.m[10] = -forward.z;

    // Convertir matriz → quaternion
    rot = Quaternion::FromMatrix(M);
}

void Camera::RenderObject() {
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;

    glPushMatrix();    
    glRotatef(90.0f, 0, 1, 0);

    if (ObjActivo == this && select){
        glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0],
                  ListaColores[static_cast<int>(ColorID::accent)][1],
                  ListaColores[static_cast<int>(ColorID::accent)][2],
                  ListaColores[static_cast<int>(ColorID::accent)][3]);
    }
    else if (select){
        glColor4f(ListaColores[static_cast<int>(ColorID::accentDark)][0],
                  ListaColores[static_cast<int>(ColorID::accentDark)][1],
                  ListaColores[static_cast<int>(ColorID::accentDark)][2],
                  ListaColores[static_cast<int>(ColorID::accentDark)][3]);
    }
    else {
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0],
                  ListaColores[static_cast<int>(ColorID::grisUI)][1],
                  ListaColores[static_cast<int>(ColorID::grisUI)][2],
                  ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D); 
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glLineWidth(1);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisable(GL_COLOR_MATERIAL);
    glDisableClientState(GL_COLOR_ARRAY);
    glDepthMask(GL_TRUE);

    glVertexPointer(3, GL_FLOAT, 0, CameraVertices);
    glDrawElements(GL_LINES, CameraEdgesSize, GL_UNSIGNED_SHORT, CameraEdges);

    if (CameraActive == this){		
        glDisable(GL_CULL_FACE);	
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, CameraFaceActive);	
    }
    glPopMatrix();
}

Camera::~Camera() {
    delete name;
}