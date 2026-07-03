#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "Empty.h"
#ifdef W3D_SYMBIAN
#include <GLES/gl.h>
#else
#include <GL/gl.h>
#endif

void Empty::RenderObject() {
    static const GLfloat cruz[] = {
        -0.5f,0,0,  0.5f,0,0,   0,-0.5f,0,  0,0.5f,0,   0,0,-0.5f,  0,0,0.5f };
    GLboolean luzEstaba = w3dEngine::IsEnabled(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::Color4f(0.85f, 0.85f, 0.85f, 1.0f);
    w3dEngine::VertexPointer3f(0, cruz);
    w3dEngine::DrawLines(6);
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (luzEstaba) w3dEngine::Enable(w3dEngine::Lighting);
}
