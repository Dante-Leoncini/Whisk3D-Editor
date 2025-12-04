#ifndef SCENE_H
#define SCENE_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>

#include "UI/icons.h"
#include "Objects/Objects.h"

// Forward declaration
class Scene;

// Variable global
extern Scene* scene;

class Scene : public Object {
public:
    bool limpiarPantalla;
    GLfloat backgroundColor[4];

    Scene();  // constructor

    void SetLimpiarPantalla();
    void SetBackground(GLfloat R, GLfloat G, GLfloat B, GLfloat A);

    ObjectType getType() override;

    ~Scene();
};

extern Object* SceneCollection;

#endif