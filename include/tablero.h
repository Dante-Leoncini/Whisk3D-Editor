#ifndef TABLERO_H
#define TABLERO_H

// MACROS
//#define MATERIAL_MAX 1
#define objVerticesModel    4
#define objFacesModel       2

/* Define vertice coordinates for the ground plane */
static const GLshort planeVertices[4 * 3] =
	{
    +120,0,-120,
    +120,0,+120,
    -120,0,+120,
    -120,0,-120
	};

/* Define indices for drawing the triangles of the ground plane */
static const GLubyte planeTriangles[2 * 3] = {
    0,2,1,
    0,3,2
};

/** Vertices for the Model object. */
static const GLshort objVertexdataModel[4 * 3]={
    3750, -5000, -3750,
    3750, -5000, 3750,
    -3750, -5000, -3750,
    -3750, -5000, 3750
};

/* Define normals for the cube */
static const GLbyte objNormaldataModel[4 * 3]={
    0,127,0,
    0,127,0,
    0,127,0,
    0,127,0
};

/** Indices to the Model object vertices. */
static const GLushort objFacedataModel[2 * 3]={
    3,0,2,
    3,1,0
};

/* textura UV */
static const GLbyte objTexdataModel[4 * 2] ={
    127,-127,
    127,127,
    -127,-127,
    -127,127
};

static const GLfloat objTexdataModelF[4 * 2] ={
    1.0f, 0.0f,   // esquina inferior derecha
    1.0f, 1.0f,   // esquina superior derecha
    0.0f, 0.0f,   // esquina inferior izquierda
    0.0f, 1.0f    // esquina superior izquierda
};

#endif