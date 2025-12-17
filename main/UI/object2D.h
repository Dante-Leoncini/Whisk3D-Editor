#ifndef OBJECT2D_H
#define OBJECT2D_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>

#ifdef __ANDROID__
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif

enum class Object2DType {
    text,
    empty,
    Image,
    Rectangle
};

class Object2D {
public:
    Object2D* Parent = nullptr;
    bool visible = true;
    int x = 0, y = 0;
    GLshort width = 10;
    GLshort height = 10;
    int scaleX = 1;
    int scaleY = 1;
    GLubyte opacity = 255;

    GLubyte color[3] = { 255, 255, 255 };

    std::vector<Object2D*> Childrens;

    Object2D(Object2D* parent = nullptr);

    virtual Object2DType getType();
    virtual void RenderObject(bool usarColorPropio = true);
    virtual ~Object2D();

    void Render(bool usarColorPropio = true);
};

#endif