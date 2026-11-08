#pragma once
// ============================================================================
//  GL/gl.h de la PSP: la PSP no tiene OpenGL (el PSPGL de pspdev no se usa). Los encabezados compartidos incluyen
//  <GL/gl.h> por los tipos (GLfloat, GLubyte...) y algunos enums; el Core llama directo a unas pocas funciones
//  (texturas y luces), que estan hechas sobre el GE en libs/Whisk3DCore/graphics/gu/w3dGLgu.cpp. El dibujo va por w3dEngine
//  (graphics/gu/w3dGraphicsGu.cpp). Las declaraciones son las de GL 1.1 que trae SDL (copia del gl.h de Mesa): las que nadie
//  llama no se linkean.
// ============================================================================
#include "SDL_opengl.h"

#ifdef __cplusplus
extern "C" {
#endif
void glGenerateMipmap(GLenum target);
#ifdef __cplusplus
}
#endif
