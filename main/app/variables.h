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
#include "base/W3dInteractionState.h" // InteractionMode/estado + sus enums: viven en el MOTOR (capa correcta)

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
    // AUTO KEY (el boton rojo del timeline): es un MODO DE TRABAJO del editor, no dato del
    // proyecto. Grabar o no grabar mientras transformas es como estar en modo insercion: el
    // que lo usa lo quiere prendido SIEMPRE, en cualquier proyecto que abra, y el que no, nunca.
    // Por eso vive en el config y no en el .w3d (que si guarda el frame, la seleccion y el modo
    // del timeline: eso es "donde estaba trabajando EN ESTE proyecto").
    bool autoKey;
    std::string SkinName;
    std::string graphicsAPI;
    // Raiz del repo Whisk3D (con libs/Whisk3DCore) que Compilar necesita para el runtime. Cuando el editor
    // corre desde el arbol de codigo se encuentra sola subiendo carpetas; cuando corre INSTALADO (junto al
    // binario no hay repo) el que compila juegos la fija a mano en Ajustes y queda guardada aca.
    std::string repoPath;
    Config()
        : fullscreen(false), enableAntialiasing(false),
          width(800), height(600), displayIndex(0),
#ifdef W3D_SYMBIAN
          scale(1), nuevoUsuario(false),   // N95: UI a escala 1 (240x320) + usuario experimentado (va a teclas)
#else
          scale(3), nuevoUsuario(true),
#endif
          autoKey(false),
          SkinName("Whisk3D"), graphicsAPI("opengl"), repoPath("") {}
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
// InteractionMode (ObjectMode/EditMode/PoseMode...) y estado (editNavegacion/rotacion...) se
// declaran en el MOTOR: base/W3dInteractionState.h (incluido arriba).
enum { pointLight, sunLight };
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
// true = recien arranco un transform (G/R/S/extrude): el primer motion debe IGNORAR el
// delta dx/dy (que todavia es del frame anterior) para que el transform arranque en CERO
extern bool g_xformPrimerMov;
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
// overlays por TIPO de objeto (submenu "Objects"): el viewport los sincroniza desde sus miembros antes de
// renderizar la escena; los rinde el traversal del CORE (Empty/Camera/gizmo de luz) que no ve al viewport.
extern bool g_showLights;
extern bool g_showCamera;
extern bool g_showEmpty;
extern bool ViewFromCameraActiveGlobal;
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