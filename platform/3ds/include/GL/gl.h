#pragma once
// ============================================================================
//  GL/gl.h de la Nintendo 3DS: la 3DS no tiene OpenGL. Los encabezados compartidos incluyen <GL/gl.h> por los tipos
//  (GLfloat, GLubyte...) y algunos enums; el Core llama directo a unas pocas funciones (texturas y luces), que estan
//  hechas sobre citro3d en libs/Whisk3DCore/graphics/citro3d/w3dGLc3d.cpp. El dibujo va por w3dEngine (w3dGraphicsC3D.cpp).
//  Las declaraciones son las de GL 1.1 que trae SDL (copia del gl.h de Mesa): las que nadie llama no se linkean.
// ============================================================================
#include "SDL_opengl.h"

#ifdef __cplusplus
extern "C" {
#endif
void glGenerateMipmap(GLenum target);
#ifdef __cplusplus
}
#endif
