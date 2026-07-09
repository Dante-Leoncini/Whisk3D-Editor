#ifndef VARIABLES_H
#define VARIABLES_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <SDL2/SDL.h>
    #include <GL/gl.h>
#endif
#include <string>
#include "math/Quaternion.h"

#ifndef W3D_SYMBIAN
extern SDL_Window* window;
extern SDL_GameController* controller;
extern SDL_GLContext glContext;
#endif

extern int winW;
extern int winH;

struct Config {
    bool fullscreen;
    bool enableAntialiasing;
    int width;
    int height;
    int displayIndex;
    int scale;          // escala global de la UI (3 = PC; 1 = chico, estilo N95 240x320)
    // false = usuario EXPERIMENTADO (atajos de teclado, menos cosas en pantalla): oculta la barra de
    // HERRAMIENTAS de abajo del viewport 3D. Default: visible en PC/Android/Web; oculta en Symbian
    // (el N95 va a teclas; un N8 tactil puede prenderla por config).
    bool nuevoUsuario;
    std::string SkinName;
    std::string graphicsAPI;
    Config()
        : fullscreen(false), enableAntialiasing(false),
          width(800), height(600), displayIndex(0), scale(3),
#ifdef W3D_SYMBIAN
          nuevoUsuario(false),
#else
          nuevoUsuario(true),
#endif
          SkinName("Whisk3D"), graphicsAPI("opengl") {}
};
extern Config cfg;

struct Cursor3D {
    Vector3 pos;
    Quaternion rot;
};
extern Cursor3D cursor3D;

// Enumeraciones
// dialecto C++03 compartido
struct Viewpoint {
    enum Enum { top, bottom, front, back, left, right, camera };
    Enum v;
    Viewpoint(Enum e) : v(e) {}
    operator Enum() const { return v; }
};

enum { Constant, Linear, EaseInOut, EaseIn, EaseOut };
// modos del viewport (selector estilo Blender; solo con una MALLA activa). Edit
// y los Paint todavia no estan implementados: por ahora el selector solo cambia
// InteractionMode (la edicion/pintura de malla viene despues).
enum { ObjectMode, EditMode, VertexPaint, WeightPaint, TexturePaint };
enum { pointLight, sunLight };
enum { editNavegacion, EdgeMove, FaceMove, timelineMove, rotacion, EditScale, translacion };
enum { Orbit, Fly, Apuntar };
enum { vertexSelect, edgeSelect, faceSelect };
// X/Y/Z = constreñido a un eje; XYZ/ViewAxis = libre (3 ejes); PlaneX/Y/Z =
// constreñido a un PLANO (Shift+eje: mueve en los OTROS dos, excluye ese eje).
// OrbitalAxis = rotacion libre ORBITAL/gimbal: izq/der gira sobre el eje
// vertical de la vista (camUp), arr/ab sobre el horizontal (camRight).
typedef enum { X, Y, Z, XYZ, ViewAxis, PlaneX, PlaneY, PlaneZ, OrbitalAxis } Axis;
// orientacion de la transformacion (eje constrenido X/Y/Z): mundo, local al
// objeto, o relativa a la vista. La cicla X/Y/Z (re-apretar) y el menu.
// NormalOrient = la direccion de la NORMAL de la seleccion (lo que hace el extrude por defecto).
typedef enum { GlobalOrient, LocalOrient, ViewOrient, NormalOrient } TransformOrient;

// barra de HERRAMIENTAS (abajo del viewport 3D): ids del historial de acciones (MRU, max 8,
// separado por modo objeto/edicion). ToolbarRegistrarAccion la llaman los starters (G/R/S/E...).
enum { TBMove, TBRotate, TBScale, TBExtrude, TBLoopCut, TBDelete };
void ToolbarRegistrarAccion(int id); // def en ViewPort3D.cpp

// Declaraciones de variables (extern)
extern int axisSelect;
extern int transformOrientation; // TransformOrient: global/local/view/normal
extern bool gTrackballCap;       // "rotar desde la vista": ya capturo el angulo inicial
// orientacion NORMAL unificada: el extrude Y el menu "Normal" usan ESTO (sin codigo repetido).
// gEVuseCustom = el transform en curso esta constrenido a gTransformNormal (la normal en MUNDO).
extern bool gEVuseCustom;
extern Vector3 gTransformNormal;
extern Vector3 TransformPivotPoint;
extern float fovDeg;
extern int nextLightId;
extern float angle;
extern int estado;
// true = recien arranco un transform (G/R/S/extrude): el primer motion debe IGNORAR el
// delta dx/dy (que todavia es del frame anterior) para que el transform arranque en CERO
extern bool g_xformPrimerMov;
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
extern float gAnguloTransform; // angulo acumulado durante una rotacion (display)
extern int NumTexturasWhisk3D;

#ifndef W3D_SYMBIAN
// Cursores SDL
extern SDL_Cursor* cursorDefault;
extern SDL_Cursor* cursorRotate;
extern SDL_Cursor* cursorScaleVertical;
extern SDL_Cursor* cursorScaleHorizontal;
extern SDL_Cursor* cursorTranslate;

// Funciones
void InitCursors();
#endif

#endif