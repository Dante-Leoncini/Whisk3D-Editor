#ifndef SCENE_H
#define SCENE_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>

#include "UI/icons.h"
#include "Objects.h"

// Forward declaration
class Scene;

// Variable global
extern Scene* scene;

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