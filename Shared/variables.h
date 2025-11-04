enum { Constant, Linear, EaseInOut, EaseIn, EaseOut };

enum{
	top,
	front,
	right,
    cameraView
};

enum{
	ObjectMode
};

enum { pointLight, sunLight };

enum {
	editNavegacion,
    EdgeMove,
	FaceMove,
	timelineMove,
	rotacion,
	EditScale,
	translacion
};

enum{
	Orbit,
	Fly,
	Apuntar
};

enum{
	vertexSelect, edgeSelect, faceSelect
};

enum{
	cubo, esfera, cilindro, plane, vacio, camara,
    cad, luz, vertice, circle
};

typedef enum { X, Y, Z, XYZ, ViewAxis } Axis;

int axisSelect = X;

//luz
//GLfloat light_pos[] = { -5000.0f, 5000.0f, 5000.0f, 1.0f };
//GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
//GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

GLfloat lightDiffuseLamp[4]   = { 0.9, 0.9, 0.9, 1.0 };
GLfloat lightSpecularLamp[4]   = { 1.0, 1.0, 1.0, 1.0 };

GLfloat sunLightPosition[4]  = {-50, 500, 500, 0 }; // y, z, x, si es direccional o puntual

//para las luces puntuales
GLfloat positionPuntualLight[4] = {0, 0, 0, 1};

GLshort TransformPivotPoint[3] = {0,0,0};
GLfloat TransformPivotPointFloat[3] = {0.0f,0.0f,0.0f};

float fovDeg = 45.0f;

int nextLightId = GL_LIGHT1;

float angle = 55.0f;
        
int estado;
int InteractionMode;
int navegacionMode;

// Variables para el Mouse
bool middleMouseDown = false;
int lastMouseX, lastMouseY;

//camara
bool CameraToView = false;

int SelectCount = 0;

bool ViewPortClickDown = false;

bool showOverlays = true;
bool ShowUi = true;
bool showFloor = true;
bool showYaxis = true;
bool showXaxis = true;
bool ShowRelantionshipsLines = true;

bool showOrigins = true;
bool show3DCursor = true;
GLfloat Cursor3DposX = 0.0f;
GLfloat Cursor3DposZ = 0.0f;
GLfloat Cursor3DposY = 0.0f;

//mouse
GLshort mouseX = 0;
GLshort mouseY = 0;
bool mouseVisible = false;

int ShiftCount = 0;
int valorRotacion = 0;

//listados
std::vector<ShapeKeyAnimation> ShapeKeyAnimations;

int NumTexturasWhisk3D = 0;