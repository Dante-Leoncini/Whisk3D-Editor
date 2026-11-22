#ifndef PRIMITIVES_H
#define PRIMITIVES_H

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

// Constantes
#define CubeVertexSize 24 * 3
#define CubeFacesSize 12 * 3
#define CubeEdgesSize 12 * 2
#define CubeUvSize 24 * 2

#define PlaneVertexSize 4 * 3
#define PlaneUvSize 4 * 2
#define PlaneFacesSize 2 * 3
#define PlaneEdgesSize 4 * 2

// Declaraciones externas (extern) para que solo haya una definición en el cpp
extern const GLushort PlaneTriangles[PlaneFacesSize];
extern const GLshort PlaneVertices[PlaneVertexSize];
extern const GLbyte PlaneUV[PlaneUvSize];
extern const GLushort PlaneBorders[PlaneEdgesSize];

extern const GLbyte CubeNormals[CubeVertexSize];
extern const GLushort CubeTriangles[CubeFacesSize];
extern const GLfloat CubeVertices[CubeVertexSize];
extern const GLushort CubeBorders[CubeEdgesSize];
extern const GLbyte CubeUV[CubeUvSize];

// una PRIMITIVA nueva (Add): el editor le da su malla 3D de la biblioteca en el acto (io/ProjectMeshes.h pone el
// gancho; en el juego compilado queda NULL)
extern void (*W3dMeshSpawnedHook)();

#endif