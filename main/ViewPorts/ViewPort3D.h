#ifndef VIEWPORT3D_H
#define VIEWPORT3D_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    // (variables.h REAL llega via ViewPorts.h)
    #include "render/OpcionesRender.h"

#else
    #include <GL/gl.h>
    #include <GL/glu.h>
    #ifndef _WIN32
    #include <GL/glext.h>
    #endif
    #include <SDL2/SDL.h>
    #include "variables.h"
    #include "sdl_key_compat.h"
#endif
#include "math/Vector3.h"
#include "math/Matrix4.h"
#include "objects/Light.h"
#ifndef W3D_SYMBIAN
#include "objects/Gamepad.h"
#endif
#include "objects/Camera.h"
#include "objects/Scene.h"
#include "objects/ObjectMode.h"
#include "ViewPorts/ViewPorts.h"
#include "GeometriaUI/Floor.h"
#include "render/render.h"
#ifndef W3D_SYMBIAN
#include "lectura-escritura.h"
#endif
#include "WithBorder.h"

extern GLfloat LastRotX;
extern GLfloat LastRotY;
extern GLfloat LastRotZ;
extern GLfloat LastPivotX;
extern GLfloat LastPivotY;
extern GLfloat LastPivotZ;

#include "WhiskUI/PopupMenu.h"
extern PopupMenu* MenuAdd;      // el desplegable del boton "Add"
extern PopupMenu* MenuSelect;   // el desplegable del boton "Select"
extern PopupMenu* MenuObject;   // el desplegable del boton "Object"
extern PopupMenu* MenuApply;    // submenu de "Object": Apply Location/Rotation/Scale/All (Ctrl A)
extern PopupMenu* MenuView;     // el desplegable del boton "View" (antes de Select): submenu Viewpoint
extern PopupMenu* MenuMesh;     // edit mode: reemplaza a "Object" (Transform + Extrude)
extern PopupMenu* MenuOverlays; // el desplegable del boton "Overlays"
extern PopupMenu* MenuRender;   // el desplegable del boton "Render" (modos)
extern PopupMenu* MenuOrient;   // el desplegable del boton "Orient" (transform)
extern PopupMenu* MenuMode;     // el desplegable del boton de modo (Object/Edit/Paint)
extern PopupMenu* MenuSelMode;  // edit mode: sub-elemento Vertex/Edge/Face

// ROLES estables de los botones de la barra del 3D. El dispatch/visibilidad los buscan por ROL
// (BarRolBtn / BarRolIdx), NO por indice -> reordenar la barra (mover botones) NO rompe nada.
enum BarRol3D {
    BR_Mode = 1, BR_SelMode, BR_Pivot, BR_Select, BR_Add,
    BR_Object, BR_Overlays, BR_Render, BR_Orient, BR_UV, BR_View
};
class Button;
Button* BarRolBtn(std::vector<Button*>& B, int rol); // el boton con ese rol (NULL si no esta)
int     BarRolIdx(std::vector<Button*>& B, int rol); // su INDICE (para la nav izq/der), -1 si no esta

class Viewport3D : public ViewportBase, public WithBorder {
    public:
        int ViewportKind() const { return 1; } // (menu de tipo)
        bool orthographic; 
        bool ViewFromCameraActive; 
        bool showOverlays; 
        bool ShowUi; 
        bool showFloor; 
        bool showYaxis; 
        bool showXaxis; 
        bool CameraToView; 
        bool showOrigins; 
        bool show3DCursor; 
        bool ShowRelantionshipsLines; 
        bool limpiarPantalla; 
        RenderType view; 

        GLfloat nearClip; 
        GLfloat farClip; 
        GLfloat aspect; 

        // --- Rotación orbital / libre ---
        Quaternion viewRot; // rotación de la cámara
        Vector3    viewPos;                  // posición de la cámara precalculada
        Vector3    pivot;                    // punto de interés a orbitar
        float      orbitDistance; // distancia al pivote (zoom)

        Viewport3D(Vector3 pos = Vector3(0,0,0));

        virtual ~Viewport3D();

        void ReloadLights();
        void ChangeViewType();
        void Resize(int newW, int newH) override;
        void SetShowOverlays(bool valor);
        // abre el menu de overlays (checkboxes) apuntando a los flags de ESTA
        // instancia; lo llama el click del boton "Overlays" de la barra
        void AbrirMenuOverlays(int x, int y);
        void Render() override;

        void RenderFloor();
        void RenderAllAxisTransform();
        void RenderOverlay();

        void UpdateViewOrbit();
        void RotateOrbit();
        // orbita con un paso fijo (flechas del keypad en Symbian): setea dx/dy
        // y aplica RotateOrbit. ndx>0 = der, ndy>0 = abajo.
        void OrbitarFlecha(int ndx, int ndy);
        void RollOrbit(float angleDeg);
        void RecalcOrbitPosition();
        void Pan();
        void PanFlecha(int ndx, int ndy);          // paneo por teclado (keypad N95: * + flechas)
        void MirarPrimeraPersona(int ndx, int ndy); // primera persona: gira la mirada sin mover la camara (# + flechas)
        void Zoom(float delta);
        // proyecta un punto del mundo a coords de PANTALLA del viewport (0..w,
        // 0..h, y hacia abajo). false si esta detras de la camara. Lo usa el
        // "rotar desde la vista" (trackball) para el angulo del mouse al pivot.
        bool ProyectarPunto(const Vector3& p, float& sx, float& sy);
        // actualiza los endpoints de la linea punteada pivot->mouse (rotar/
        // escalar). mx,my = mouse en coords de ventana. Compartido PC/Symbian.
        void ActualizarLineaTransform(int mx, int my);
        // "rotar desde la vista" (trackball): rota alrededor del eje de camara
        // segun el angulo del mouse al pivot EN pantalla. Compartido PC/Symbian.
        void RotarDesdeVista(int mx, int my);

        void RenderRelantionshipsLines();
        void Render3Dcursor();
        void RenderUI();
        // durante un transform reemplaza la barra de botones por una barra de
        // estado (Translate/Scale/Rotate con los valores en vivo)
        void RenderBarraTransform();
        // overlay de estadisticas/fps (texto blanco arriba a la derecha)
        void RenderEstadisticas();
        void EnfocarObject();
        // centra el pivote en 'centro' y ajusta orbitDistance para que una esfera de 'radio' entre
        // en el FOV con un pequeño padding (zoom-to-fit del foco '.').
        void EncuadrarRadio(const Vector3& centro, float radio);
        bool RecalcViewPos();
        void SetViewpoint(Viewpoint value);
        void RestaurarViewport();
        void ChangePerspective();
        void SetCursor3D();
        void Aceptar();
        void button_left() override;
#ifndef W3D_SYMBIAN
        void mouse_button_up(SDL_Event &e) override;
#endif
#ifndef W3D_SYMBIAN
        void event_mouse_wheel(SDL_Event &e) override;
#endif
        void event_mouse_motion(int mx, int my) override;
        void TeclaDerecha();
        void TeclaIzquierda();
        void TeclaArriba();
        void TeclaAbajo();
        void SetEje(int eje);
        void SetViewFromCameraActive(bool value);
#ifndef W3D_SYMBIAN
        void event_key_down(SDL_Event &e) override;
#endif
#ifndef W3D_SYMBIAN
        void event_key_up(SDL_Event &e) override;
#endif
        void key_down_return();
        void SetLimpiarPantalla(bool value);
};

#endif