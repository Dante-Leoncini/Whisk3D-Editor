#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "Empty.h"
#include "objects/RenderColors.h" // gRenderColors / RC_selActive (color de seleccion, igual que el resto)
#ifdef W3D_SYMBIAN
#include <GLES/gl.h>
#else
#include <GL/gl.h>
#endif

extern bool g_showEmpty; // toggle del submenu "Objects" (overlays)

void Empty::RenderObject() {
    if (!g_showEmpty) return; // oculto por el toggle "Empty" del menu de overlays
    static const GLfloat cruz[] = {
        -0.5f,0,0,  0.5f,0,0,   0,-0.5f,0,  0,0.5f,0,   0,0,-0.5f,  0,0,0.5f };
    GLboolean luzEstaba = w3dEngine::IsEnabled(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    // color de seleccion igual que el resto de los objetos: ACTIVO = verde (selActive); seleccionado no-activo =
    // verde secundario (selInactive); sin seleccionar = gris.
    if (select){
        const float* c = gRenderColors[(this == ObjActivo) ? RC_selActive : RC_selInactive];
        w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    } else {
        w3dEngine::Color4f(0.85f, 0.85f, 0.85f, 1.0f);
    }
    w3dEngine::VertexPointer3f(0, cruz);
    w3dEngine::DrawLines(6);
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (luzEstaba) w3dEngine::Enable(w3dEngine::Lighting);
}
