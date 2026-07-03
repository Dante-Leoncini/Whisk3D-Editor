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
    #include "WhiskUI/icons.h"
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

    ObjectType getType() override;

    ~Scene();
};

extern Object* SceneCollection;

#endif