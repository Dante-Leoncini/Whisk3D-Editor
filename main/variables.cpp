#include "variables.h"

#ifndef W3D_SYMBIAN
SDL_Window* window = NULL;
SDL_GameController* controller = NULL;
//SDL_Gamepad* gamepad = NULL;
SDL_GLContext glContext = NULL;
#endif

int winW = 640;
int winH = 480;

// Inicialización de variables
int axisSelect = X;
int transformOrientation = GlobalOrient;
bool gTrackballCap = false;

float fovDeg = 45.0f;

int nextLightId = GL_LIGHT1;

float angle = 55.0f;
int estado;
bool g_xformPrimerMov = false;
bool gEVuseCustom = false;             // transform constrenido a la NORMAL (extrude / orientacion Normal)
Vector3 gTransformNormal(0, 1, 0);     // esa normal, en MUNDO
int InteractionMode;
int navegacionMode;


Config cfg;

std::string w3dPath = "";
std::string exeDir = "";

// Variables para el Mouse
bool leftMouseDown = false;
bool middleMouseDown = false;
bool MouseWheel = false;
int lastMouseX = 0;
int lastMouseY = 0;

// Cámara
bool ViewPortClickDown = false;

// Viewport3D valores globales
bool showOverlayGlobal = false;
bool ViewFromCameraActiveGlobal = false;

Quaternion rotGlobal;
Vector3 viewPosGlobal;
Vector3 camRight;
Vector3 camUp;
Vector3 camForward;
Vector3 TransformPivotPoint;

Cursor3D cursor3D;

// Mouse (en el N95 el cursor vive en w3dmouse.cpp)
#ifndef W3D_SYMBIAN
GLshort mouseX = 0;
GLshort mouseY = 0;
bool mouseVisible = false;
#endif

// globales compartidos por los 4 OS (input/transform). Antes el N95 los
// redefinia en Whisk3D.cpp; ahora viven aca para todos (limpieza final).
int ShiftCount = 0;
int valorRotacion = 0;
float gAnguloTransform = 0.0f;

#ifndef W3D_SYMBIAN
int NumTexturasWhisk3D = 0;
#endif

#ifndef W3D_SYMBIAN
// Cursores SDL
SDL_Cursor* cursorDefault = NULL;
SDL_Cursor* cursorRotate = NULL;
SDL_Cursor* cursorScaleVertical = NULL;
SDL_Cursor* cursorScaleHorizontal = NULL;
SDL_Cursor* cursorTranslate = NULL;

// Función
void InitCursors() {
    #if SDL_MAJOR_VERSION == 2
        cursorDefault         = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        cursorTranslate       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
        cursorRotate          = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        cursorScaleVertical   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
        cursorScaleHorizontal = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    #elif SDL_MAJOR_VERSION == 3
        cursorDefault         = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        cursorTranslate       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
        cursorRotate          = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        cursorScaleVertical   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
        cursorScaleHorizontal = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
    #endif
}
#endif // !W3D_SYMBIAN
