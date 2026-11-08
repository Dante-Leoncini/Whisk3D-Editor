#pragma once
// ============================================================================
//  GL/gl.h de la PS Vita: el OpenGL es vitaGL (el pipeline fijo emulado sobre la GPU de la Vita, sceGxm). Su header
//  trae los tipos, los enums y las funciones de GL 1.x / ES; los encabezados compartidos incluyen <GL/gl.h> y caen
//  aca. Lo que vitaGL no define y el Core nombra va abajo.
// ============================================================================
#include <vitaGL.h>

// las luces: vitaGL solo tiene la forma con puntero (glLightfv / glLightModelfv). Los focos (GL_SPOT_*) y la luz de
// las dos caras no los hace: los valores van igual (vitaGL los ignora) y la luz queda como una luz de punto
#ifndef GL_SPOT_EXPONENT
    #define GL_SPOT_EXPONENT 0x1205
#endif
#ifndef GL_SPOT_CUTOFF
    #define GL_SPOT_CUTOFF 0x1206
#endif
#ifndef GL_SPOT_DIRECTION
    #define GL_SPOT_DIRECTION 0x1204
#endif
#ifndef GL_LIGHT_MODEL_TWO_SIDE
    #define GL_LIGHT_MODEL_TWO_SIDE 0x0B52
#endif
#ifndef GL_DITHER
    #define GL_DITHER 0x0BD0   // (la Vita dibuja en 8 bits por canal: no hay tramado que prender)
#endif
static inline void glLightf(GLenum luz, GLenum que, GLfloat v) { glLightfv(luz, que, &v); }
static inline void glLightModelf(GLenum que, GLfloat v) { glLightModelfv(que, &v); }

#ifdef __cplusplus
// (vitaGL declara el parametro sin const; GL lo declara const y el Core pasa arreglos const)
static inline void glTexEnvfv(GLenum t, GLenum que, const GLfloat* v) { glTexEnvfv(t, que, (GLfloat*)v); }
#endif
