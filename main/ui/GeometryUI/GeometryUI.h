#ifndef GEOMETRY_UI_H
#define GEOMETRY_UI_H

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

// Sprites y primitivas de UI

extern const GLshort pointVertex[3];

extern const int LineLightVertexSize;
extern const int LineEdgeSize;

extern GLfloat LineLightVertex[];
extern GLfloat lineUV[];

extern const GLushort LineEdge[2];

extern const int Cursor3DVertexSize;
extern const int Cursor3DEdgesSize;
extern const GLfloat Cursor3DVertices[];
extern const GLushort Cursor3DEdges[];

#endif
