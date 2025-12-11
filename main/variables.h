#ifndef VARIABLES_H
#define VARIABLES_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <string>
#include "math/Quaternion.h"

extern SDL_Window* window;
extern SDL_GameController* controller;
extern SDL_GLContext glContext;

extern int winW; 
extern int winH;

struct Config {
    bool fullscreen = false;
    bool enableAntialiasing = false;
    int width = 800;
    int height = 600;
	int displayIndex = 0; // monitor 1
	std::string SkinName = "Whisk3D"; // monitor 1
    std::string graphicsAPI = "opengl";
};
extern Config cfg;

struct Cursor3D {
    Vector3 pos;
    Quaternion rot;
};
extern Cursor3D cursor3D;

// Enumeraciones
enum class Viewpoint {top, bottom, front, back, left, right, camera};

enum { Constant, Linear, EaseInOut, EaseIn, EaseOut };
enum { ObjectMode };
enum { pointLight, sunLight };
enum { editNavegacion, EdgeMove, FaceMove, timelineMove, rotacion, EditScale, translacion };
enum { Orbit, Fly, Apuntar };
enum { vertexSelect, edgeSelect, faceSelect };
typedef enum { X, Y, Z, XYZ, ViewAxis } Axis;

// Declaraciones de variables (extern)
extern int axisSelect;
extern Vector3 TransformPivotPoint;
extern float fovDeg;
extern int nextLightId;
extern float angle;
extern int estado;
extern int InteractionMode;
extern int navegacionMode;
extern std::string w3dPath;
extern std::string exeDir;

// Mouse
extern bool leftMouseDown;
extern bool middleMouseDown;
extern bool MouseWheel;
extern int lastMouseX;
extern int lastMouseY;

// Cámara
extern bool ViewPortClickDown;

// Viewport 3D
extern bool showOverlayGlobal;
extern bool ViewFromCameraActiveGlobal;
extern Quaternion rotGlobal;
extern Vector3 viewPosGlobal;
extern Vector3 camRight;
extern Vector3 camUp;
extern Vector3 camForward;

// Mouse
extern GLshort mouseX;
extern GLshort mouseY;
extern bool mouseVisible;
extern int ShiftCount;
extern int valorRotacion;
extern int NumTexturasWhisk3D;

// Cursores SDL
extern SDL_Cursor* cursorDefault;
extern SDL_Cursor* cursorRotate;
extern SDL_Cursor* cursorScaleVertical;
extern SDL_Cursor* cursorScaleHorizontal;
extern SDL_Cursor* cursorTranslate;

// Funciones
void InitCursors();

#endif