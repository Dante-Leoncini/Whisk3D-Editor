#ifndef CURVE_H
#define CURVE_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <GL/gl.h>
#include <GL/glext.h>

#include "UI/colores.h"
#include "UI/icons.h"
#include "Objects.h"

class Curve : public Object { 
    public:
        int vertexSize = 0;
        GLfloat* vertex = nullptr;

        Curve(Object* parent = nullptr, Vector3 pos = Vector3(0,0,0));

        ~Curve();

        ObjectType getType() override;

        void RenderObject() override;
};

#endif