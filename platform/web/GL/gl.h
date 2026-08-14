// ============================================================================
//  Shim de <GL/gl.h> para WebGL / Emscripten
// ----------------------------------------------------------------------------
//  El editor incluye <GL/gl.h> en muchos headers, pero SOLO por los TIPOS
//  (GLfloat/GLushort/GLenum...) y algunas constantes: todo el dibujo real ya
//  pasa por w3dEngine (backend ES2 gles2/w3dGraphicsGLES2.cpp). En WebGL no
//  existe <GL/gl.h>; con -Iplatform/web este archivo lo reemplaza y redirige
//  al header ES2, que trae los tipos y las constantes core (GL_TEXTURE_2D,
//  GL_LINES, GL_FLOAT, GL_REPEAT, GL_RGBA, ...).
//
//  Las constantes de PIPELINE FIJO que ES2 no define (GL_LIGHT0.., atributos
//  de luz, texenv) las declaramos aca con su valor clasico de desktop: el
//  editor las usa como IDENTIFICADORES (ej. LightID = GL_LIGHT0 + n) o quedan
//  en ramas que en web no se ejecutan. Cada una va con #ifndef por las dudas.
// ============================================================================
#pragma once

#include <GLES2/gl2.h>

// --- luces del pipeline fijo (el editor maneja hasta 8; en ES2 el shader usa 1) ---
#ifndef GL_LIGHT0
#define GL_LIGHT0 0x4000
#define GL_LIGHT1 0x4001
#define GL_LIGHT2 0x4002
#define GL_LIGHT3 0x4003
#define GL_LIGHT4 0x4004
#define GL_LIGHT5 0x4005
#define GL_LIGHT6 0x4006
#define GL_LIGHT7 0x4007
#endif
#ifndef GL_LIGHTING
#define GL_LIGHTING 0x0B50
#endif

// --- parametros de luz (solo referidos en ramas desktop; se definen por robustez) ---
#ifndef GL_AMBIENT
#define GL_AMBIENT               0x1200
#define GL_DIFFUSE               0x1201
#define GL_SPECULAR              0x1202
#define GL_POSITION              0x1203
#define GL_SPOT_DIRECTION        0x1204
#define GL_SPOT_EXPONENT         0x1205
#define GL_SPOT_CUTOFF           0x1206
#define GL_CONSTANT_ATTENUATION  0x1207
#define GL_LINEAR_ATTENUATION    0x1208
#define GL_QUADRATIC_ATTENUATION 0x1209
#endif

// --- texenv / point sprite del pipeline fijo (no existen en ES2) ---
#ifndef GL_MODULATE
#define GL_MODULATE 0x2100
#endif
#ifndef GL_REPLACE
#define GL_REPLACE 0x1E01
#endif
#ifndef GL_SPHERE_MAP
#define GL_SPHERE_MAP 0x2402
#endif
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_COORD_REPLACE
#define GL_COORD_REPLACE 0x8862
#endif

// tipo de desktop que ES2 no define (por si algun header lo usa en una firma)
#ifndef GLdouble
typedef double GLdouble;
#endif
