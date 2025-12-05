#ifndef GEOMETRIAUI_H
#define GEOMETRIAUI_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>

// Sprites y primitivas de UI

extern const GLshort pointVertex[3];

extern const int LineaLightVertexSize;
extern const int LineaEdgeSize;

extern GLfloat LineaLightVertex[];
extern GLfloat LineaLinkChild[];
extern GLfloat lineUV[];

extern const GLshort LineaTimeline[6];
extern const GLushort LineaEdge[2];

extern const int EmptyVertexSize;
extern const int EmptyEdgesSize;
extern const GLshort EmptyVertices[];
extern const GLushort EmptyEdges[];

extern const int Cursor3DVertexSize;
extern const int Cursor3DEdgesSize;
extern const GLfloat Cursor3DVertices[];
extern const GLushort Cursor3DEdges[];

extern const int SpriteVertexSize;
extern const int SpriteFacesSize;
extern const int SpriteUvSize;
extern const GLushort SpriteFaces[];

#endif