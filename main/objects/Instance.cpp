#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "Instance.h"

// Constructor
Instance::Instance(Object* parent, Object* instance)
    : Object(parent, "Instance", Vector3(0,0,0))
{
    RenderChildrens = true; // (eran inicializadores de clase: C++03)
    count = 1;
    mirror = false;
    mirrorEje = 0;

    if (!instance) return;

    target = instance;
    pos = instance->pos;
    rot = instance->rot;
    scale = instance->scale;
}

// Devuelve el tipo de objeto
ObjectType Instance::getType() {
    return ObjectType::instance;
}

// Recarga la instancia
void Instance::Reload() {
    ReloadTarget(this);
}

// Renderizado CORREGIDO (Acumulación en el bucle)
void Instance::RenderObject() {
    if (!target) return;

    // Mirror: renderiza el target ESPEJADO sobre un eje (el objeto original
    // sigue en la escena; esta muestra su reflejo). El espejo invierte el
    // sentido de las caras, asi que damos vuelta el frontface mientras dura.
    if (mirror) {
        Matrix4 m;
        target->GetMatrix(m);
        w3dEngine::PushMatrix();
        w3dEngine::MultMatrix(m.m);
        w3dEngine::Scalef(mirrorEje == 0 ? -1.0f : 1.0f,
                 mirrorEje == 1 ? -1.0f : 1.0f,
                 mirrorEje == 2 ? -1.0f : 1.0f);
        w3dEngine::FrontFace(false);
        target->RenderObject();
        w3dEngine::FrontFace(true);
        w3dEngine::PopMatrix();
        return;
    }

    // 1. Pre-cálculo de transformaciones base de la INSTANCIA
    Vector3 basePosDelta = pos;     // Traslación base (ej: 2.0m)
    Quaternion baseRot = rot;       // Rotación base (ej: 15°)

    // 2. Obtener las matrices de inicio (del Target)
    Matrix4 targetMatrix;
    target->GetMatrix(targetMatrix); // Matriz T * R * S del Target

    // Obtener la rotación del target (para aplicarla al vector delta)
    Quaternion targetRot = target->rot;

    Matrix4 deltaMatrix = target->BuildMatrix(basePosDelta, baseRot, Vector3(1,1,1));

    w3dEngine::PushMatrix();
    w3dEngine::MultMatrix(targetMatrix.m);

    for(size_t i = 0; i < count; i++){
        target->RenderObject();

        if (i > 0) {
            targetRot = targetRot * baseRot;
            targetRot.normalize();
        }

        Vector3 rotatedDeltaPos = targetRot * basePosDelta;

        Matrix4 T_rotada; T_rotada.Identity();
        T_rotada.m[12] = rotatedDeltaPos.x;
        T_rotada.m[13] = rotatedDeltaPos.y;
        T_rotada.m[14] = rotatedDeltaPos.z;

        Matrix4 R_delta = baseRot.ToMatrix();
        w3dEngine::MultMatrix(T_rotada.m);
        w3dEngine::MultMatrix(R_delta.m);

    }

    w3dEngine::PopMatrix(); // Vuelve a la Matriz del Padre/Mundo
}

// Destructor
Instance::~Instance() {}