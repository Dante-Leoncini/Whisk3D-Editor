#include "variables.h"

SDL_Window* window = nullptr;
SDL_GameController* controller = nullptr;
//SDL_Gamepad* gamepad = nullptr;
SDL_GLContext glContext = nullptr;

int winW = 640;
int winH = 480;

// Inicialización de variables
int axisSelect = X;

float fovDeg = 45.0f;

int nextLightId = GL_LIGHT1;

float angle = 55.0f;
int estado;
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

// Mouse
GLshort mouseX = 0;
GLshort mouseY = 0;
bool mouseVisible = false;

int ShiftCount = 0;
int valorRotacion = 0;
int NumTexturasWhisk3D = 0;

// Cursores SDL
SDL_Cursor* cursorDefault = nullptr;
SDL_Cursor* cursorRotate = nullptr;
SDL_Cursor* cursorScaleVertical = nullptr;
SDL_Cursor* cursorScaleHorizontal = nullptr;
SDL_Cursor* cursorTranslate = nullptr;

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