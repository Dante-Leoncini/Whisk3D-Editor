#include "GeometriaUI.h"

// Sprites y primitivas de UI

const GLshort pointVertex[3] = {0, 0, 0};

const int LineaLightVertexSize = 2 * 3;
const int LineaEdgeSize = 1 * 2;

GLfloat LineaLightVertex[LineaLightVertexSize] = {    
    0.0f, 0.0f, 0.0f,
    0.0f, -3.0f, 0.0f,
};

GLfloat LineaLinkChild[LineaLightVertexSize] = {    
    0.0f, 0.0f, 0.0f,
    0.0f, 1000.0f, 0.0f,
};

GLfloat lineUV[4] = {
    0.0f,  0.0f,
    0.0f,  0.0f
};

const GLshort LineaTimeline[6] = {    
    0, 0, 0,
    0, 16, 0,
};

const GLushort LineaEdge[2] = {
    0, 1
};

const int EmptyVertexSize = 6 * 3;
const int EmptyEdgesSize = 3 * 2;

const GLshort EmptyVertices[EmptyVertexSize] = {
    0, 0, -2000,
    0, 0, 2000,
    0, -2000, 0,
    0, 2000, 0,
    -2000, 0, 0,
    2000, 0, 0,
};

const GLushort EmptyEdges[EmptyEdgesSize] = {
    0, 1,
    2, 3,
    4, 5
};

const int Cursor3DVertexSize = 12 * 3;
const int Cursor3DEdgesSize = 6 * 2;

const GLfloat Cursor3DVertices[Cursor3DVertexSize] = {
    0.0f,  0.0f, -0.75f/2,
    0.0f,  0.0f, -0.225f/2,
    0.0f,  0.0f,  0.75f/2,
    0.0f,  0.0f,  0.225f/2,
    0.0f, -0.75f/2,  0.0f,
    0.0f, -0.225f/2, 0.0f,
    0.0f,  0.75f/2,  0.0f,
    0.0f,  0.225f/2, 0.0f,
    -0.75f/2,  0.0f, 0.0f,
    -0.225f/2, 0.0f, 0.0f,
    0.75f/2,  0.0f, 0.0f,
    0.225f/2, 0.0f, 0.0f,
};

const GLushort Cursor3DEdges[Cursor3DEdgesSize] = {
    0, 1,
    2, 3,
    4, 5,
    6, 7,
    8, 9,
    10,11
};

const int SpriteVertexSize = 16 * 3;
const int SpriteFacesSize = 18 * 3;
const int SpriteUvSize = 16 * 2;

const GLushort SpriteFaces[SpriteFacesSize] = {
    0,1,2, 2,3,0,
    1,4,2, 2,5,4,
    4,5,6, 5,6,7,
    3,2,8, 8,2,9,
    2,9,5, 9,10,5,
    10,5,7, 10,11,7,
    8,12,9, 12,9,13,
    13,9,10, 13,10,14,
    14,10,11, 14,11,15
};