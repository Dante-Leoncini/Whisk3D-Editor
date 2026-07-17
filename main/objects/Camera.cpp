#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "Camera.h"
#include <math.h>
#include <algorithm>
#include "WhiskUI/colores.h"
#include "render/OpcionesRender.h" // g_renderAspect (la geometria de la camara sigue el aspecto del render)
#ifdef W3D_SYMBIAN
    #endif

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

const MeshIndex CameraFaceActive[3] = {5, 6, 7};

// RESPONSIVE: reescribe CameraVertices para que el rectangulo del "film" y el triangulito de arriba sigan el
// ASPECTO del render (ancho/alto). Se FIJA la ALTURA y se escala el ANCHO (z) por el aspecto: 1:1 = cuadrada,
// 4:3 = 4:3, 16:9 = ancha, etc. Barato -> se llama por frame antes de dibujar el gizmo. Devuelve true si cambio.
static float gCamGeomAspect = -1.0f; // ultimo aspecto con el que se armo la geometria
bool ActualizarGeometriaCamara() {
    float aspect = g_renderAspect;
    if (aspect < 0.05f) aspect = 0.05f; if (aspect > 20.0f) aspect = 20.0f;
    if (aspect == gCamGeomAspect) return false; // sin cambios
    gCamGeomAspect = aspect;
    const float x = 0.7f;      // profundidad del film (0.35*2, como el original)
    const float H = 0.288f;    // MEDIA-altura fija del rectangulo (0.144*2)
    float Wd = H * aspect;     // MEDIA-ancho segun el aspecto (4:3 -> 0.384, = original)
    float Zt = Wd * (0.256f / 0.384f); // el triangulito escala con el ancho (misma proporcion que el original)
    GLfloat v[CameraVertexSize] = {
        0, 0, 0,          // origen (apex del frustum)
        x,  H,  Wd,       // 1  esquinas del rectangulo (film)
        x, -H,  Wd,       // 2
        x, -H, -Wd,       // 3
        x,  H, -Wd,       // 4
        x, 0.34f,  Zt,    // 5  triangulito de arriba (camara activa): base
        x, 0.34f, -Zt,    // 6
        x, 0.62f, 0,      // 7  apice del triangulito
    };
    for (int i = 0; i < CameraVertexSize; i++) CameraVertices[i] = v[i];
    return true;
}

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

Camera* CameraActive = NULL;

// ------------------- MÉTODOS -------------------

Camera::Camera(Object* parent, Vector3 pos, Vector3 Rot)
    : Object(parent, T("Camera"), pos, Rot), offsetRiel(0), Riel(NULL)
{

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
    if (fabsf(forwardVector.Dot(worldUp)) > 0.999f) { 
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
    Riel = NULL;
}

// Busca y asigna el riel según RielName
void Camera::ReloadRiel(Object* me) {
    Riel = NULL;
    Object* obj = FindObjectByName(SceneCollection, RielName);
#ifdef W3D_SYMBIAN
    // sin RTTI en RVCT: el tipo se chequea por getType()
    Curve* RielTarget = (obj && obj->getType() == ObjectType::curve)
                        ? (Curve*)obj : NULL;
    if (!RielTarget){
        return;
    }
#else
    Curve* RielTarget = dynamic_cast<Curve*>(obj);
    if (!RielTarget){
#ifndef W3D_SYMBIAN
        std::cout << "RielTarget '"<< RielName <<"' no encontrado...\n";
#endif
        return;
    }
#endif

    // 1) Evitar apuntarse a sí mismo
    if (RielTarget == me) {
#ifndef W3D_SYMBIAN
        std::cout << "ERROR: Target es el mismo objeto → prohibido\n";
#endif
        return;
    }

    // 2) Evitar apuntar a ancestros
    if (IsMyAncestor(me, RielTarget)) {
#ifndef W3D_SYMBIAN
        std::cout << "ERROR: Target es un ancestro → generaría recursion\n";
#endif
        return;
    }

    // 3) Asignar correctamente
    Riel = RielTarget;
#ifndef W3D_SYMBIAN
    std::cout << "Riel '"<< RielName <<"' encontrado!\n";
#endif
}

extern bool g_showCamera; // toggle del submenu "Objects" (overlays)

void Camera::RenderObject() {
    if (!g_showCamera) return; // oculto por el toggle "Camera" del menu de overlays
#ifdef W3D_SYMBIAN
    // MISMO gate que PC: sin overlays (render a PNG / limpieza de pantalla) o mirando DESDE la camara
    // -> no dibujar el gizmo. Antes Symbian lo dibujaba SIEMPRE (se veia en el render del N95).
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;
    bool geomCambio = ActualizarGeometriaCamara(); // geometria responsive al aspecto del render
    // gizmo de PC (piramide de lineas + triangulo si es la activa) pero con
    // drawArrays: vertices expandidos una sola vez desde CameraEdges (re-expande si cambio el aspecto)
    static GLfloat lineV[CameraEdgesSize * 3];
    static GLfloat faceV[9];
    static bool armado = false;
    if (!armado || geomCambio) {
        for (int i = 0; i < CameraEdgesSize; i++) {
            int vi = CameraEdges[i];
            lineV[i*3+0] = CameraVertices[vi*3+0];
            lineV[i*3+1] = CameraVertices[vi*3+1];
            lineV[i*3+2] = CameraVertices[vi*3+2];
        }
        for (int i = 0; i < 3; i++) {
            int vi = CameraFaceActive[i];
            faceV[i*3+0] = CameraVertices[vi*3+0];
            faceV[i*3+1] = CameraVertices[vi*3+1];
            faceV[i*3+2] = CameraVertices[vi*3+2];
        }
        armado = true;
    }
    w3dEngine::PushMatrix();
    w3dEngine::Rotatef(90.0f, 0, 1, 0);
    const float* c;
    if (ObjActivo == this && select) c = ListaColores[static_cast<int>(ColorID::accent)];
    else if (select)                 c = ListaColores[static_cast<int>(ColorID::accentDark)];
    else                             c = ListaColores[static_cast<int>(ColorID::grisUI)];
    w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    GLboolean luzEstaba = w3dEngine::IsEnabled(w3dEngine::Lighting); // restaurar al salir!
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::LineWidth(1);
    w3dEngine::VertexPointer3f(0, lineV);
    w3dEngine::TexCoordPointer2f(12, lineV); // dummy valido
    w3dEngine::DrawLines(CameraEdgesSize);
    if (CameraActive == this) {
        // triangulo SOLIDO arriba = camara activa (como Blender); PC
        // tambien apaga el culling porque la cara se ve de ambos lados
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::VertexPointer3f(0, faceV);
        w3dEngine::TexCoordPointer2f(12, faceV);
        w3dEngine::DrawTrianglesArray(3);
    }
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (luzEstaba) w3dEngine::Enable(w3dEngine::Lighting);
    w3dEngine::PopMatrix();
    return;
#else
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;
    ActualizarGeometriaCamara(); // geometria responsive al aspecto del render (usa CameraVertices directo)

    w3dEngine::PushMatrix();
    w3dEngine::Rotatef(90.0f, 0, 1, 0);

    if (ObjActivo == this && select){
        w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::accent)][0],
                  ListaColores[static_cast<int>(ColorID::accent)][1],
                  ListaColores[static_cast<int>(ColorID::accent)][2],
                  ListaColores[static_cast<int>(ColorID::accent)][3]);
    }
    else if (select){
        w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::accentDark)][0],
                  ListaColores[static_cast<int>(ColorID::accentDark)][1],
                  ListaColores[static_cast<int>(ColorID::accentDark)][2],
                  ListaColores[static_cast<int>(ColorID::accentDark)][3]);
    }
    else {
        w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::grisUI)][0],
                  ListaColores[static_cast<int>(ColorID::grisUI)][1],
                  ListaColores[static_cast<int>(ColorID::grisUI)][2],
                  ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    }

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Texture2D); 
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::LineWidth(1);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::Disable(w3dEngine::ColorMaterial);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DepthMask(true);

    w3dEngine::VertexPointer3f(0, CameraVertices);
    w3dEngine::DrawLinesIndexed(CameraEdgesSize, CameraEdges);

    if (CameraActive == this){		
        w3dEngine::Disable(w3dEngine::CullFace);	
        w3dEngine::DrawTriangles(3, CameraFaceActive);	
    }
    w3dEngine::PopMatrix();
#endif // !W3D_SYMBIAN
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
    // si esta camara era la ACTIVA, soltar el puntero global: sino queda
    // COLGADO y el render (CameraActive->UpdatePosition/UpdateLookAt) crashea
    // al borrar la camara. (mismo problema que tenia la luz)
    if (CameraActive == this) CameraActive = NULL;
}