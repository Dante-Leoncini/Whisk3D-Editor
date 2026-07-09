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
extern PopupMenu* MenuMesh;     // edit mode: menu "Mesh" comun (Transform arriba, Snap, Delete abajo)
extern PopupMenu* MenuOverlays; // el desplegable del boton "Overlays"
extern PopupMenu* MenuRender;   // el desplegable del boton "Render" (modos)
extern PopupMenu* MenuOrient;   // el desplegable del boton "Orient" (transform)
extern PopupMenu* MenuMode;     // el desplegable del boton de modo (Object/Edit/Paint)
extern PopupMenu* MenuSelMode;  // edit mode: sub-elemento Vertex/Edge/Face

// ROLES estables de los botones de la barra del 3D. El dispatch/visibilidad los buscan por ROL
// (BarRolBtn / BarRolIdx), NO por indice -> reordenar la barra (mover botones) NO rompe nada.
enum BarRol3D {
    BR_Mode = 1, BR_SelMode, BR_Pivot, BR_Select, BR_Add,
    BR_Object, BR_Overlays, BR_Render, BR_Orient, BR_UV, BR_View,
    BR_Mesh // menu "Mesh" de Edit Mode (Transform/Snap/Delete), comun a vertice/borde/cara
};
// roles de la barra de HERRAMIENTAS (abajo). TBR_Hist+i = boton i del historial de acciones.
enum ToolbarRol3D {
    TBR_Aceptar = 100, TBR_Cancelar, TBR_Orient, TBR_EjeX, TBR_EjeY, TBR_EjeZ,
    TBR_Shift, TBR_Ctrl, // modificadores TACTILES (sin teclado): togglean LShiftPressed / LCtrlPressed
    TBR_Undo, TBR_Redo,  // deshacer / rehacer: SIEMPRE visibles (PC y tactil), a la izquierda de la barra
    TBR_Repeat = 120,    // "Repeat" (solo en extrude): acepta el extrude y vuelve a extruir la seleccion
    TBR_View = 121,      // "View" (toggle, Edit Mode): 1 dedo orbita/panea/zoom aunque haya una operacion en curso
    TBR_Hist = 110 // .. TBR_Hist+7
};
class Button;
Button* BarRolBtn(std::vector<Button*>& B, int rol); // el boton con ese rol (NULL si no esta)
int     BarRolIdx(std::vector<Button*>& B, int rol); // su INDICE (para la nav izq/der), -1 si no esta

class Viewport3D : public ViewportBase, public WithBorder {
    public:
        int ViewportKind() const { return 1; } // (menu de tipo)
        bool orthographic;
        bool ViewFromCameraActive;
        // PASSEPARTOUT de camara: cuando se mira DESDE la camara, el marco (aspecto del render) encuadrado en el
        // viewport. camFrameOn = hay marco; camFrameNX/NY = medias-extensiones del marco en NDC (para el overlay 2D).
        bool camFrameOn; float camFrameNX; float camFrameNY;
        void RenderCamPassepartout(); // dibuja el borde blanco + oscurece afuera del marco (lo que NO sale en el render)
        // INSPECCION en vista de camara: paneo/zoom de la VISTA (no mueve la camara) para ver con detalle dentro y
        // fuera del marco. Se aplica a la proyeccion + al marco. La camara NO se toca (el render no cambia).
        float camViewZoom; float camViewPanX; float camViewPanY;
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
        bool lockOrbit; // "Lock Orbit": si ON, TODO lo que orbita (mouse/touch/flechas) PANEA en su lugar
                        // (el orbital nunca gira). Para usar el viewport como tablero 2D. El zoom no cambia.
        RenderType view;

        GLfloat nearClip;
        GLfloat farClip;
        GLfloat aspect;

        // color de fondo del viewport en modo solid/wireframe/material (POR-VIEWPORT, editable en codigo).
        // Sentinel alpha < 0 = usar el color del tema (ListaColores[background]), que es el comportamiento
        // por defecto. Es OTRO color que el fondo del RENDER (g_renderBg, global). Sin UI por ahora.
        float bgSolido[4];

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

        // render de la escena a un PNG de outW x outH (puede ser mayor que la ventana; por tiles,
        // anda hasta en el N95). Sin overlay. pass = Rendered / ZBuffer / NormalView / Alpha. true si guardo.
        // progBase/progTotal: para la barra de progreso (tiles previos / total). 0 = sin barra.
        int  TilesNecesarios(int outW, int outH) const; // tiles de un render (para el total de la barra)
        bool RenderAPNG(int outW, int outH, RenderType::Enum pass, const char* filename,
                        int progBase = 0, int progTotal = 0);

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
        // mundo-por-pixel al arrastrar (mover/extrude): a la PROFUNDIDAD del pivot de transform, para
        // que lo agarrado se mueva 1:1 con el mouse/flechas en pantalla a cualquier zoom.
        float VelocidadArrastreMundo();
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
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) override; // 2 dedos: zoom + paneo
        bool event_finger_scroll(int px, int py, int dx, int dy) override; // 1 dedo sobre el TOOLBAR = scroll horiz
        void TeclaDerecha();
        void TeclaIzquierda();
        void TeclaArriba();
        void TeclaAbajo();
        void SetEje(int eje);
        void SetViewFromCameraActive(bool value);

        // ---- BARRA DE HERRAMIENTAS (abajo): mismos Button que la barra de arriba. CONTEXTUAL:
        // sin transform = historial de acciones; durante un transform = orientacion + ejes X/Y/Z
        // (+ aceptar/cancelar si es tactil). Solo si cfg.nuevoUsuario (Symbian default: off). ----
        std::vector<Button*> ToolButtons; // botones persistentes (roles TBR_*)
        int  toolScroll;                  // scroll horizontal manual de la barra
        bool toolGesto;                   // el gesto de arrastre actual arranco sobre ESTA barra (no la de arriba)
        bool ToolbarVisible() const;      // cfg.nuevoUsuario
        int  ToolbarHeight() const;       // = BarHeight()
        bool OnToolbar(int px, int py);   // (px,py) cae en la barra de herramientas?
        void ToolbarScrollBy(int delta);
        void ToolbarActualizar();          // visibilidad contextual + colores + layout (sx/sy con scroll)
        bool ToolbarClick(int mx, int my); // teclas de la barra (true = consumido)
        bool ClickBarraTransform(int mx, int my); // tap TACTIL en la barra de estado del transform -> abre el teclado numerico
        void RenderToolbar();
        bool OnBar(int px, int py) override;      // barra de arriba O la de herramientas (setea toolGesto)
        void BarScrollBy(int delta) override;     // rutea el scroll a la barra donde arranco el gesto
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