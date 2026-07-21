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

// SceneCollection lo DEFINE el Core (objects/Objects.cpp) y arranca en 0; aca solo se llena con
// la raiz del editor. En Symbian eso ya se hacia asi, por el orden de inicializacion estatica
// entre unidades: el ctor de Scene usa globals de Objects.cpp (ObjSelects).
void W3dModelInit() {
    if (!SceneCollection) {
        SceneCollection = new Scene();
    }
}

#ifndef W3D_SYMBIAN
// en PC se arma sola en el arranque estatico, como antes (el editor no llama a W3dModelInit)
namespace { struct CrearRaiz { CrearRaiz() { W3dModelInit(); } } g_crearRaiz; }
#endif