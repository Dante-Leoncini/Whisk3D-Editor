#include "Scene.h"

// Definición de la variable global
Scene* scene = nullptr;

// Constructor
Scene::Scene(Vector3 pos)
    : Object(nullptr, "Scene Collection", pos), limpiarPantalla(true) 
{
    // Color por defecto negro transparente
    backgroundColor[0] = 0.0f;
    backgroundColor[1] = 0.0f;
    backgroundColor[2] = 0.0f;
    backgroundColor[3] = 0.0f;

    IconType = static_cast<size_t>(IconType::archive);
    scene = this;
}

// Métodos
void Scene::SetLimpiarPantalla() {
    limpiarPantalla = !limpiarPantalla;
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
    delete name;
}

Object* SceneCollection = new Scene();