#include "Scene.h"

// Definición de la variable global
Scene* scene = NULL;

// Constructor
Scene::Scene(Vector3 pos)
    : Object(NULL, "Scene Collection", pos)
{
    // Color por defecto negro transparente
    backgroundColor[0] = 0.0f;
    backgroundColor[1] = 0.0f;
    backgroundColor[2] = 0.0f;
    backgroundColor[3] = 0.0f;

#ifndef W3D_SYMBIAN
#endif
    scene = this;
}

void Scene::SetBackground(GLfloat R, GLfloat G, GLfloat B, GLfloat A) {
    backgroundColor[0] = R;
    backgroundColor[1] = G;
    backgroundColor[2] = B;
    backgroundColor[3] = A;
}

ObjectType Scene::getType() {
    return ObjectType::collection;
}

// Destructor
Scene::~Scene() {
}

#ifdef W3D_SYMBIAN
// en Symbian la raiz se crea explicitamente (W3dModelInit, llamado desde el
// container): el orden de inicializacion estatica entre TUs no esta
// garantizado y el ctor de Scene usa globals de Objects.cpp (ObjSelects)
Object* SceneCollection = NULL;
void W3dModelInit() {
    if (!SceneCollection) {
        SceneCollection = new Scene();
    }
}
#else
Object* SceneCollection = new Scene();
#endif