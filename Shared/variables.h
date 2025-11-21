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

typedef enum { X, Y, Z, XYZ, ViewAxis } Axis;

int axisSelect = X;

GLshort TransformPivotPoint[3] = {0,0,0};
GLfloat TransformPivotPointFloat[3] = {0.0f,0.0f,0.0f};

float fovDeg = 45.0f;

int nextLightId = GL_LIGHT1;

float angle = 55.0f;
        
int estado;
int InteractionMode;
int navegacionMode;

// Variables para el Mouse
bool leftMouseDown = false;
bool middleMouseDown = false;
bool MouseWheel = false;
int lastMouseX, lastMouseY;

//camara
bool ViewPortClickDown = false;

//viewport3D valores globales
bool showOverlayGlobal = false;

GLfloat Cursor3DposX = 0.0f;
GLfloat Cursor3DposZ = 0.0f;
GLfloat Cursor3DposY = 0.0f;

//mouse
GLshort mouseX = 0;
GLshort mouseY = 0;
bool mouseVisible = false;

int ShiftCount = 0;
int valorRotacion = 0;

int NumTexturasWhisk3D = 0;

//para los cursores de SDL3, puede que no sea bueno tenerlo aca...
SDL_Cursor* cursorDefault;
SDL_Cursor* cursorRotate;
SDL_Cursor* cursorScaleVertical;
SDL_Cursor* cursorScaleHorizontal;
SDL_Cursor* cursorTranslate;

void InitCursors() {
    cursorDefault   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    cursorTranslate = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    cursorRotate    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    cursorScaleVertical   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
	cursorScaleHorizontal = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
}

/*inline float FIXED_TO_FLOAT(GLfixed x) {
    return static_cast<float>(x) / 65536.0f; // porque Q16.16
}*/