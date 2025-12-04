#ifndef PRIMITIVAS_H
#define PRIMITIVAS_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>

// Constantes
#define CuboVertexSize 24 * 3
#define CuboFacesSize 12 * 3
#define CuboEdgesSize 12 * 2
#define CuboUvSize 24 * 2

#define PlaneVertexSize 4 * 3
#define PlaneUvSize 4 * 2
#define PlaneFacesSize 2 * 3
#define PlaneEdgesSize 4 * 2

// Declaraciones externas (extern) para que solo haya una definición en el cpp
extern const GLushort PlaneTriangles[PlaneFacesSize];
extern const GLshort PlaneVertices[PlaneVertexSize];
extern const GLbyte PlaneUV[PlaneUvSize];
extern const GLushort PlaneBordes[PlaneEdgesSize];

extern const GLbyte CuboNormals[CuboVertexSize];
extern const GLushort CuboTriangles[CuboFacesSize];
extern const GLfloat CuboVertices[CuboVertexSize];
extern const GLushort CuboBordes[CuboEdgesSize];
extern const GLbyte CuboUV[CuboUvSize];

#endif