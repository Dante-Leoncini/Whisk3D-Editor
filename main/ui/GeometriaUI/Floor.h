#ifndef FLOOR_H
#define FLOOR_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif

// MACROS
#define objFacesFloor 96

// Declaraciones de arrays como externos
extern const GLfloat objVertexdataFloor[102*3];
extern const GLushort EjeVerde[2];
extern const GLushort EjeRojo[2];
extern const GLushort EjeAzul[2];
extern const GLushort objFacedataFloor[48*2];

#endif