#include "GeometryUI.h"

// Sprites y primitivas de UI

const GLshort pointVertex[3] = {0, 0, 0};

const int LineLightVertexSize = 2 * 3;
const int LineEdgeSize = 1 * 2;

GLfloat LineLightVertex[LineLightVertexSize] = {
    0.0f, 0.0f, 0.0f,
    0.0f, -3.0f, 0.0f,
};

GLfloat lineUV[4] = {
    0.0f,  0.0f,
    0.0f,  0.0f
};

const GLushort LineEdge[2] = {
    0, 1
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
