//para los sprites
static const GLshort pointVertex[3]={0, 0, 0};

static const int LineaLightVertexSize = 2 * 3;
static const int LineaEdgeSize = 1 * 2;

static GLfloat LineaLightVertex[LineaLightVertexSize] = {    
    0.0f, 0.0f, 0.0f,
    0.0f, -3.0f, 0.0f,
};

static GLshort LineaLinkChild[LineaLightVertexSize] = {    
    0, 0, 0,
    0, 30000, 0,
};

static GLfloat lineUV[4] = {
    0.0f,  0.0f,
    0.0f,  0.0f
};

static const GLshort LineaTimeline[6] = {    
    0, 0, 0,
    0, 16, 0,
};

// Define los índices de las líneas
static const GLushort LineaEdge[2] = {
    0, 1
};

static const int EmptyVertexSize = 6 * 3;
static const int EmptyEdgesSize = 3 * 2;

static const GLshort EmptyVertices[EmptyVertexSize] = {
    //eje1
    0, 0, -2000,
    0, 0, 2000,
    //eje2
    0, -2000, 0,
    0, 2000, 0,
    //eje3
    -2000, 0, 0,
    2000, 0, 0,
};

static const GLushort EmptyEdges[EmptyEdgesSize]={
	0, 1,
	2, 3,
	4, 5
};

static const int Cursor3DVertexSize = 12 * 3;
static const int Cursor3DEdgesSize = 6 * 2;

static const GLfloat Cursor3DVertices[Cursor3DVertexSize] = {
    //eje1
    0.0f,  0.0f, -0.75f,
    0.0f,  0.0f, -0.225f,
    0.0f,  0.0f,  0.75f,
    0.0f,  0.0f,  0.225f,
    //eje2
    0.0f, -0.75f,  0.0f,
    0.0f, -0.225f, 0.0f,
    0.0f,  0.75f,  0.0f,
    0.0f,  0.225f, 0.0f,
    //eje3
    -0.75f,  0.0f, 0.0f,
    -0.225f, 0.0f, 0.0f,
     0.75f,  0.0f, 0.0f,
     0.225f, 0.0f, 0.0f,
};

static const GLushort Cursor3DEdges[Cursor3DEdgesSize]={
	0, 1,
	2, 3,
	4, 5,
	6, 7,
	8, 9,
	10,11
};

static const int SpriteVertexSize = 16 * 3;
static const int SpriteFacesSize = 18 * 3;
static const int SpriteUvSize = 16 * 2;

/* Define vertice coordinates for the ground plane */
//en el sentido del reloj empezando de la esquina superior izquierda
/*static GLshort SpriteVertices[SpriteVertexSize] = {
	0,    0, 0,
	256,    0, 0,
    256, 256, 0,
    0, 256, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
};*/

/** los dos triangulos */
static const GLushort SpriteFaces[SpriteFacesSize]={
    //1
	0,1,2,
	2,3,0,
    //2
	1,4,2,
	2,5,4,
    //3
    4,5,6,
    5,6,7,
    //4
    3,2,8,
    8,2,9,
    //5
    2,9,5,
    9,10,5,
    //6
    10,5,7,
    10,11,7,
    //7
    8,12,9,
    12,9,13,
    //8
    13,9,10,
    13,10,14,
    //9
    14,10,11,
    14,11,15
};

/* textura UV */
//en el sentido del reloj empezando de la esquina superior izquierda
/*static GLbyte SpriteUV[SpriteUvSize] ={
    -128,  -128,
    127,   -128,
    127,    127,
    -128,   127,
    0,0,
    0,0,
    0, 0, 
    0, 0, 
    0, 0, 
    0, 0, 
    0, 0, 
    0, 0, 
    0, 0, 
    0, 0, 
    0, 0, 
    0, 0 
};*/
//f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3 ...
