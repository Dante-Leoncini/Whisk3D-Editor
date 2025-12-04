#ifndef VARIABLES_H
#define VARIABLES_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <SDL.h>
#include <GL/gl.h>
#include <string>

extern SDL_Window* window;
extern SDL_GameController* controller;
extern SDL_GLContext glContext;

extern int winW; 
extern int winH;

// Enumeraciones
enum { Constant, Linear, EaseInOut, EaseIn, EaseOut };
enum { top, front, right, cameraView };
enum { ObjectMode };
enum { pointLight, sunLight };
enum { editNavegacion, EdgeMove, FaceMove, timelineMove, rotacion, EditScale, translacion };
enum { Orbit, Fly, Apuntar };
enum { vertexSelect, edgeSelect, faceSelect };
typedef enum { X, Y, Z, XYZ, ViewAxis } Axis;

// Declaraciones de variables (extern)
extern int axisSelect;
extern GLshort TransformPivotPoint[3];
extern GLfloat TransformPivotPointFloat[3];
extern float fovDeg;
extern int nextLightId;
extern float angle;
extern int estado;
extern int InteractionMode;
extern int navegacionMode;
extern std::string w3dPath;

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
extern GLfloat Cursor3DposX;
extern GLfloat Cursor3DposZ;
extern GLfloat Cursor3DposY;

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