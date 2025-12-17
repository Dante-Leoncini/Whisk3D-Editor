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
    ReloadRiel(this);
}

#include <cmath> // Necesario para std::abs() y std::sqrt() si no está en Vector3.h

void Camera::UpdateLookAt() {
    if (!target) return;

    //std::cout << "Camara pos X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z << std::endl;
    //std::cout << "Target X: " << target->pos.x << " Y: " << target->pos.y << " Z: " << target->pos.z << std::endl;

    forwardVector = (target->pos - pos).Normalized();
    
    // CORRECCIÓN 1: Usar WorldUp POSITIVO (Y-Up estándar)
    Vector3 worldUp(0, 1, 0); 

    // Opcional pero recomendado: Anti-Gimbal Lock
    if (std::abs(forwardVector.Dot(worldUp)) > 0.999f) { 
        worldUp = Vector3(1, 0, 0); 
    }

    // 2. Cálculo del Eje Right (X local) - Orden para que funcione el Yaw (no invertido)
    // Orden Canónico: Cross(WorldUp, Dir)
    rightVector = Vector3::Cross(worldUp, forwardVector).Normalized();

    // 3. Cálculo del Eje Up (Y local)
    // El orden Cross(Right, Dir) es el estándar.
    Vector3 up = Vector3::Cross(rightVector, forwardVector).Normalized(); 
    
    // 4. Forward (Eje Z cámara de Vista)
    Vector3 forward = (-forwardVector).Normalized();
    
    // 5. Construcción de matriz (column-major)
    // Los signos deben ser: [-Right | -Up | +Forward] o [+Right | +Up | -Forward]
    Matrix4 M;
    M.Identity();

    // Columna X (Right): Invertimos el signo para corregir el Yaw.
    M.m[0] = -rightVector.x;
    M.m[1] = -rightVector.y;
    M.m[2] = -rightVector.z;

    // Columna Y (Up):
    M.m[4] = -up.x;
    M.m[5] = -up.y;
    M.m[6] = -up.z;

    // Columna Z (Forward): Positivo (mira a lo largo de -Z)
    M.m[8]  = forward.x;
    M.m[9]  = forward.y;
    M.m[10] = forward.z;

    rot = Quaternion::FromMatrix(M);
}

void Camera::SetRiel(const std::string& NewValue) {
    RielName = NewValue;
    Riel = nullptr;
}

// Busca y asigna el riel según RielName
void Camera::ReloadRiel(Object* me) {
    Riel = nullptr;
    Object* obj = FindObjectByName(SceneCollection, RielName);
    Curve* RielTarget = dynamic_cast<Curve*>(obj);
    if (!RielTarget){
        std::cout << "RielTarget '"<< RielName <<"' no encontrado...\n";
        return;
    }

    // 1) Evitar apuntarse a sí mismo
    if (RielTarget == me) {
        std::cout << "ERROR: Target es el mismo objeto → prohibido\n";
        return;
    }

    // 2) Evitar apuntar a ancestros
    if (IsMyAncestor(me, RielTarget)) {
        std::cout << "ERROR: Target es un ancestro → generaría recursion\n";
        return;
    }

    // 3) Asignar correctamente
    Riel = RielTarget;
    std::cout << "Riel '"<< RielName <<"' encontrado!\n";
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

void Camera::UpdatePosition() {
    if (!Riel || !target) return;

    Vector3 P = target->GetGlobalPosition();

    int idx = Riel->FindNearest(P);
    if (idx < 0) return;

    // índices vecinos
    int i1 = idx;
    int i0 = std::max(0,     i1 - 1);
    int i2 = std::min(Riel->vertexSize - 1, i1 + 1);

    // obtener puntos reales
    Vector3 A = Riel->GetPoint(i0);
    Vector3 B = Riel->GetPoint(i1);
    Vector3 C = Riel->GetPoint(i2);

    float tAB, tBC;
    Vector3 pAB = ClosestPointOnSegment(A, B, P, tAB);
    Vector3 pBC = ClosestPointOnSegment(B, C, P, tBC);

    // distancias para elegir cuál segmento es mejor
    float dAB = (pAB - P).LengthSq();
    float dBC = (pBC - P).LengthSq();

    Vector3 finalPos;
    float finalIndexFloat;

    if (dAB < dBC) {
        finalPos       = pAB;
        finalIndexFloat = (float)i0 + tAB;  // ejemplo: 12.35 entre 12 y 13
    } else {
        finalPos       = pBC;
        finalIndexFloat = (float)i1 + tBC;
    }

    // aplicar offset hacia atrás
    float offsetIndex = finalIndexFloat - (float)offsetRiel;

    if (offsetIndex < 0.0f) offsetIndex = 0.0f;
    int baseIndex = (int)floor(offsetIndex);
    float t = offsetIndex - baseIndex;

    // asegurar límites
    int nextIndex = std::min(baseIndex + 1, Riel->vertexSize - 1);

    Vector3 P0 = Riel->GetPoint(baseIndex);
    Vector3 P1 = Riel->GetPoint(nextIndex);

    Vector3 camPos = P0 * (1.0f - t) + P1 * t;

    // sumar posición global de la curva
    camPos += Riel->GetGlobalPosition();

    this->pos = camPos;
}

Camera::~Camera() {
}