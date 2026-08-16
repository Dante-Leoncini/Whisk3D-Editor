#ifndef VIEWPORT3D_H
#define VIEWPORT3D_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#include "render/OpcionesRender.h"   // RenderType: se usa en las dos plataformas
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    // (variables.h REAL llega via ViewPorts.h)
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
#include "objects/CameraBase.h" // la vista del core: VistaCam() la devuelve por valor
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

#include "WhiskUI/widgets/PopupMenu.h"
extern PopupMenu* MenuAdd;      // el desplegable del boton "Add"
extern PopupMenu* MenuImports;  // submenu "Add > Imports": OBJ / FBX / glTF / GLB
extern PopupMenu* MenuMallas;   // submenu "Add > Mesh": las primitivas (plano, cubo, esfera...)
extern PopupMenu* MenuSelect;   // el desplegable del boton "Select"
extern PopupMenu* MenuObject;
extern PopupMenu* MenuAnimation; // el desplegable del boton "Animation"   // el desplegable del boton "Object"
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
    BR_Object, BR_Overlays, BR_Render, BR_Orient, BR_UV, BR_View, BR_Snap,
    BR_JuegoStop, BR_JuegoPlay,   // transporte del MODO JUEGO (Play/Pausa es UN boton)
    BR_Mesh, // menu "Mesh" de Edit Mode (Transform/Snap/Delete), comun a vertice/borde/cara
    BR_Animation // menu "Animation": keyframes del objeto + Motion Trail. Solo con algo seleccionado.
};
// roles de la barra de HERRAMIENTAS (abajo) del 3D. TBR_Hist+i = boton i del historial de acciones.
enum ToolbarRol3D {
    TBR_Aceptar = 100, TBR_Cancelar, TBR_Orient, TBR_EjeX, TBR_EjeY, TBR_EjeZ,
    TBR_Shift, TBR_Ctrl, // modificadores TACTILES (sin teclado): togglean LShiftPressed / LCtrlPressed
    TBR_Undo, TBR_Redo,  // deshacer / rehacer: SIEMPRE visibles (PC y tactil), a la izquierda de la barra
    TBR_Repeat = 120,    // "Repeat" (solo en extrude): acepta el extrude y vuelve a extruir la seleccion
    TBR_View = 121,      // "View" (toggle, Edit Mode): 1 dedo orbita/panea/zoom aunque haya una operacion en curso
    TBR_Hist = 110 // .. TBR_Hist+7
};
// (BarRolBtn / BarRolIdx se declaran en ViewPorts.h: los usan todas las barras, no solo la del 3D)
// MODO JUEGO: prepara los viewports 3D al dar play (reset de la vista de camara;
// apagarOverlays solo al INICIAR el juego)
void JuegoPrepararViewports(bool apagarOverlays);

class Viewport3D : public ViewportBase, public WithBorder {
    public:
        int ViewportKind() const { return 1; } // (menu de tipo)
        bool orthographic;
        bool ViewFromCameraActive;
        // PASSEPARTOUT de camara: cuando se mira DESDE la camara, el marco (aspecto del render) encuadrado en el
        // viewport. camFrameOn = hay marco; camFrameNX/NY = medias-extensiones del marco en NDC (para el overlay 2D).
        bool camFrameOn; float camFrameNX; float camFrameNY;
        // LETTERBOX DURO (encuadre declarado + juego corriendo): el marco ocupa el
        // viewport sin aire y muestra EXACTAMENTE el frustum declarado (ver
        // Camera::aspecto). Las bandas de afuera, EN EL EDITOR, son ATENUADAS
        // (mismo oscurecido 0.5 del passepartout): reporte del dueno, textual:
        // "ahora alrededor de la camara se ve completamente negro y ya no tengo
        // forma de saber si el culling se esta haciendo bien". El negro OPACO del
        // dispositivo queda para el runtime compilado (que dibuja sus barras por
        // su cuenta) o detras del toggle 'letterboxNegro' (menu Overlays /
        // comando letterboxnegro del harness), que es lo que usan las pruebas de
        // pixeles del encuadre (encuadrepx).
        bool camFrameLetterbox;
        bool letterboxNegro; // toggle: bandas OPACAS como en el dispositivo (default false = atenuadas)
        void RenderCamPassepartout(); // dibuja el borde blanco + oscurece afuera del marco (lo que NO sale en el render)
        void RenderMotionTrail();     // Motion Trail: el camino de los objetos animados, en x-ray
        void RenderSnapIndicador();   // recuadro verde en el target de snap bajo el cursor
        void RenderArmaturasEncima(Object* node); // huesos del esqueleto (lineas azules) encima de todo
        int  PickBone(class Armature* arm, float lmx, float lmy); // Pose Mode: hueso mas cercano al click (px), o -1
        // INSPECCION en vista de camara: paneo/zoom de la VISTA (no mueve la camara) para ver con detalle dentro y
        // fuera del marco. Se aplica a la proyeccion + al marco. La camara NO se toca (el render no cambia).
        float camViewZoom; float camViewPanX; float camViewPanY;
        // PANTALLA DEL JUEGO dentro de este viewport: el rect LOCAL donde RenderUI dibujo
        // el HUD (mirando por la camara es el MARCO del passepartout; si no, el area util)
        // + la escala lienzo->pantalla (uniforme SIEMPRE: nada se deforma). Lo lee el
        // input del Play (SimJuego::MapearAJuego) para que el toque caiga EXACTAMENTE
        // donde se dibuja. hudW = 0 -> este viewport aun no dibujo su HUD.
        float hudX0, hudY0, hudW, hudH, hudEsc;
        // true = el HUD se dibujo con el LIENZO OVERRIDEADO a esta pantalla (modo juego
        // con UI "igual que el render"): el input lo re-fija antes de mapear, como el
        // Editor2D en vista de juego (coherencia con varios viewports).
        bool hudOverride;
        bool showOverlays;
        // los 6 flags del overlay "Statistics" -> PER-VIEWPORT (antes globales en OpcionesRender). MISMO nombre que
        // los globales viejos a proposito: RenderEstadisticas y el menu Overlays>Statistics (metodos de Viewport3D)
        // los referencian sin calificar -> pasan a bindear this->flag sin tocar esos sitios. Stats por viewport.
        bool OverlayFps;
        bool OverlayStatVertices;
        bool OverlayStatFaces;
        bool OverlayStatGL;
        bool OverlayStatModgen;
        bool OverlayStatTimes;
        // ESTADISTICAS DEL FRAME de ESTE viewport: lo que el pase de ESCENA que acaba
        // de dibujar Render() EMITIO de verdad (con frustum culling, LOD, Culling y
        // celdas por triangulo ya aplicados). Render() las mide como DELTA de los
        // contadores del Core (g_stat*, la misma fuente que 'bench'/'presupuesto')
        // alrededor del pase de escena; las lee RenderEstadisticas (overlay
        // "faces: dibujadas/total" y "gl: llamadas por frame").
        int statTrisFrame;    // triangulos emitidos (indices/3) por el pase de escena
        int statDrawsFrame;   // draw calls de triangulos
        int statBindsFrame;   // glBindTexture REALES (los cacheados no cuentan)
        int statEstadosFrame; // cambios de estado que llegaron al driver
        // (aca vivia ShowUi, que salteaba RenderUI entero: dada de baja. RenderUI no es un
        //  overlay, es el chrome del area + el reseteo del estado GL 2D; sin el, viewport
        //  inusable y estado sucio. El lector de .w3d IGNORA la prop si viene guardada.)
        bool showFloor;
        bool showYaxis; 
        bool showXaxis; 
        bool CameraToView; 
        bool showOrigins;
        bool showArmature; // overlays por tipo de objeto (submenu "Objects" del menu de overlays)
        bool showLights;
        bool showCamera;
        bool showEmpty;
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
        void Resize(int newW, int newH) W3D_OVERRIDE;
        void SetShowOverlays(bool valor);
        // abre el menu de overlays (checkboxes) apuntando a los flags de ESTA
        // instancia; lo llama el click del boton "Overlays" de la barra
        void AbrirMenuOverlays(int x, int y);
        void Render() W3D_OVERRIDE;

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
        // la vista de ESTE viewport como camara del core (la Camera activa si se mira por ella, o la orbita).
        CameraBase VistaCam() const;
        // publica esta vista al core (W3dVistaBind) y NADA MAS: no mueve la camara de escena, no toca las
        // luces y no carga la matriz de GL. Es lo que llama el que solo necesita PREGUNTAR "como se ve desde
        // aca" fuera del dibujo (el foco, el input), porque lo que se ve depende de cual fue el ultimo bind.
        void BindVista();
        void RotateOrbit();
        // orbita con un paso fijo (flechas del keypad en Symbian): setea dx/dy
        // y aplica RotateOrbit. ndx>0 = der, ndy>0 = abajo.
        void OrbitarFlecha(int ndx, int ndy);
        void RollOrbit(float angleDeg);
        void RecalcOrbitPosition();
        void Pan();
        void PanFlecha(int ndx, int ndy);          // paneo por teclado (keypad N95: * + flechas)
        void Zoom(float delta);
        // proyecta un punto del mundo a coords de PANTALLA del viewport (0..w,
        // 0..h, y hacia abajo). false si esta detras de la camara. Lo usa el
        // "rotar desde la vista" (trackball) para el angulo del mouse al pivot.
        // outW (opcional): el divisor de perspectiva del punto (profundidad eye-space en perspectiva, 1.0 en
        // ortografica). Sirve para interpolar dentro de un triangulo de forma PERSPECTIVE-CORRECT (bary/outW).
        bool ProyectarPunto(const Vector3& p, float& sx, float& sy, float* outW = 0);
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
        void SetViewpoint(Viewpoint value);
        // Vistas por cuadrante: el 1/3/7 del numpad para el telefono (# + flechas). Si estabas orbitando libre, la
        // primera flecha solo ENCUADRA al mas cercano; despues izq/der giran 90 y arriba/abajo van al top/bottom
        // (con tope). Toda vista ortogonal es pitch*yaw -> moverse es sumarle a dos enteros.
        // El boton de contexto de la barra (Object/Pose/Vertex/Edge/Face): decide su icono y si se ve. Es ESTADO,
        // no dibujo -> vive afuera del render y se puede testear.
        void SyncBotonContexto();
        void VistaCuadranteNav(int dx, int dy);
        void VistaCuadranteActual(int& yaw90, int& pitch, bool& exacta) const;
        void RestaurarViewport();
        void ChangePerspective();
        void Aceptar();
        void button_left() W3D_OVERRIDE;
#ifndef W3D_SYMBIAN
        void mouse_button_up(int boton) W3D_OVERRIDE;
#endif
#ifndef W3D_SYMBIAN
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
#endif
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE;
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) W3D_OVERRIDE; // 2 dedos: zoom + paneo
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // 1 dedo sobre el TOOLBAR = scroll horiz
        void TeclaDerecha();
        void TeclaIzquierda();
        void TeclaArriba();
        void TeclaAbajo();
        void SetEje(int eje);
        void SetViewFromCameraActive(bool value);

        // ---- BARRA DE HERRAMIENTAS (abajo): el MECANISMO (ToolButtons/scroll/hit/render) vive en
        // ViewportBase (ToolbarBase.cpp, compartido con UV/2D). Aca queda SOLO lo contextual del 3D:
        // sin transform = historial de acciones; durante un transform = orientacion + ejes X/Y/Z
        // (+ aceptar/cancelar si es tactil). Solo si cfg.nuevoUsuario (Symbian default: off). ----
        bool ToolbarVisible() const W3D_OVERRIDE;   // cfg.nuevoUsuario
        void ToolbarSincronizar() W3D_OVERRIDE;     // visibilidad contextual + colores (estado puro)
        void ToolbarAccionRol(int rol) W3D_OVERRIDE; // que hace cada boton (roles TBR_*)
        bool ClickBarraTransform(int mx, int my); // tap TACTIL en la barra de estado del transform -> abre el teclado numerico
        void RenderToolbar() W3D_OVERRIDE;            // + ocultarla en MODO JUEGO (el juego se mira limpio)
#ifndef W3D_SYMBIAN
        void event_key_down(int tecla, bool repeticion) W3D_OVERRIDE;
#endif
#ifndef W3D_SYMBIAN
        void event_key_up(int tecla) W3D_OVERRIDE;
#endif
        void key_down_return();
};

#endif