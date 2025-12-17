#include "Instance.h"

// Constructor
Instance::Instance(Object* parent, Object* instance)
    : Object(parent, "Instance", {0,0,0}) 
{
    IconType = static_cast<size_t>(IconType::instance);

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

    // 1. Pre-cálculo de transformaciones base de la INSTANCIA
    Vector3 basePosDelta = pos;     // Traslación base (ej: 2.0m)
    Quaternion baseRot = rot;       // Rotación base (ej: 15°)
    
    // 2. Obtener las matrices de inicio (del Target)
    Matrix4 targetMatrix;
    target->GetMatrix(targetMatrix); // Matriz T * R * S del Target
    
    // Obtener la rotación del target (para aplicarla al vector delta)
    Quaternion targetRot = target->rot;
    
    Matrix4 deltaMatrix = target->BuildMatrix(basePosDelta, baseRot, Vector3(1,1,1));

    glPushMatrix();
    glMultMatrixf(targetMatrix.m);

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
        glMultMatrixf(T_rotada.m);
        glMultMatrixf(R_delta.m);

    }

    glPopMatrix(); // Vuelve a la Matriz del Padre/Mundo
}

// Destructor
Instance::~Instance() {}