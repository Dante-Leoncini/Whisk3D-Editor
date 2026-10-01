#ifndef SCENE_H
#define SCENE_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "objects/Objects.h"
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>  // GLfloat
#else
    #include <GL/gl.h>
    #include "WhiskUI/draw/icons.h"
#endif

// Forward declaration
class Scene;

// Variable global
extern Scene* scene;

#ifdef W3D_SYMBIAN
// crea la raiz de la escena (SceneCollection) en orden controlado
void W3dModelInit();
#endif

class Scene : public Object {
public:
    GLfloat backgroundColor[4];

    Scene(Vector3 pos = Vector3(0,0,0));  // constructor

    void SetBackground(GLfloat R, GLfloat G, GLfloat B, GLfloat A);

    ObjectType getType() W3D_OVERRIDE;

    ~Scene();
};

extern Object* SceneCollection;

// una RAIZ NUEVA (una escena 3D o un prefab mas del proyecto, ver main/W3dRaices.h): una Scene
// que NO se cuelga de la raiz activa, no toca la seleccion ni el global 'scene' y queda con su
// fondo por defecto. No la registra: eso lo hace quien la crea.
Scene* W3dSceneNuevaRaiz();

#endif