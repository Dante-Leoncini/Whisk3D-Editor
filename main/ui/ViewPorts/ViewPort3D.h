#ifndef VIEWPORT3D_H
#define VIEWPORT3D_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <vector>
#include "render/RenderOptions.h"   // RenderType: se usa en las dos plataformas
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
#include "objects/CameraBase.h" // la vista del core: ViewCam() la devuelve por valor
#include "objects/Light.h"
#ifndef W3D_SYMBIAN
#include "objects/Gamepad.h"
#endif
#include "objects/Camera.h"
#include "objects/Scene.h"
#include "objects/ObjectMode.h"
#include "ViewPorts/ViewPorts.h"
#include "GeometryUI/Floor.h"
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
extern PopupMenu* MenuMeshes;   // submenu "Add > Mesh": las primitivas (plano, cubo, esfera...)
extern PopupMenu* MenuCore;     // submenu "Add > Core": las rutinas (pasos del render)
extern PopupMenu* MenuModifiers;   // submenu "Add > Modifier Objects": espejo, array, instancia
extern PopupMenu* MenuLightsAdd;   // submenu "Add > Lights": luz, sol, foco y la sonda de luz
extern PopupMenu* MenuPrefabsAdd; // submenu "Add > Prefab": los prefabs del proyecto (se rearma al abrir el Add)
extern PopupMenu* MenuScenesAdd;  // submenu "Add > Scene": las otras escenas del proyecto (se rearma al abrir el Add)
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
// (BarRoleBtn / BarRoleIdx), NO por indice -> reordenar la barra (mover botones) NO rompe nada.
enum BarRole3D {
    BR_Mode = 1, BR_SelMode, BR_Pivot, BR_Select, BR_Add,
    BR_Object, BR_Overlays, BR_Render, BR_Orient, BR_UV, BR_View, BR_Snap,
    BR_GameStop, BR_GamePlay,   // transporte del MODO JUEGO (Play/Pausa es UN boton)
    BR_Mesh, // menu "Mesh" de Edit Mode (Transform/Snap/Delete), comun a vertice/borde/cara
    BR_Animation, // menu "Animation": keyframes del objeto + Motion Trail. Solo con algo seleccionado.
    BR_Proportional, // PROPORTIONAL EDITING (tecla O): el icono de la curva, verde si esta ON; abre su menu
    BR_Root,         // la ESCENA 3D o el PREFAB que se edita (W3dRoots.h): su menu cambia de raiz / crea otra
    BR_TexSave,      // PINTURA DE TEXTURA: guardar la textura pintada (Guardar / Guardar como / dentro o fuera del w3d)
    BR_SelEdge, BR_SelFace   // el selector de VERTICE / ARISTA / CARA es un boton alargado de 3 (BR_SelMode = el de vertice)
};
// roles de la barra de HERRAMIENTAS (abajo) del 3D. TBR_Hist+i = boton i del historial de acciones.
enum ToolbarRole3D {
    TBR_Accept = 100, TBR_Cancel, TBR_Orient, TBR_AxisX, TBR_AxisY, TBR_AxisZ,
    TBR_Shift, TBR_Ctrl, // modificadores TACTILES (sin teclado): togglean LShiftPressed / LCtrlPressed
    TBR_Undo, TBR_Redo,  // deshacer / rehacer: SIEMPRE visibles (PC y tactil), a la izquierda de la barra
    TBR_Repeat = 120,    // "Repeat" (solo en extrude): acepta el extrude y vuelve a extruir la seleccion
    TBR_View = 121,      // "View" (toggle, Edit Mode): 1 dedo orbita/panea/zoom aunque haya una operacion en curso
    TBR_Proportional = 122, // proportional editing: toggle de UN click (Edit/Object Mode); la barra de influencia sale arriba
    TBR_Snap = 123,      // el IMAN: prender / apagar (Object / Edit Mode, tambien con un transform en curso). En el telefono
                         // se llega con el softkey derecho (el foco de la barra) y OK
    // las OPCIONES de la operacion en curso (insertar caras / bevel), junto al tilde y la cruz: las mismas teclas de la PC
    TBR_InsetIndividual = 124, TBR_InsetBoundary, TBR_InsetOutset, TBR_InsetDepth,   // I / B / O / el dedo = profundidad
    TBR_BevelSegLess, TBR_BevelSegMore, TBR_BevelVertices,                           // segmentos - / + y V
    TBR_Hist = 110 // .. TBR_Hist+7
};
// (BarRoleBtn / BarRoleIdx se declaran en ViewPorts.h: los usan todas las barras, no solo la del 3D)
// MODO JUEGO: prepara los viewports 3D al dar play (reset de la vista de camara;
// turnOffOverlays solo al INICIAR el juego)
void GamePrepareViewports(bool turnOffOverlays);

// una FRANJA del render a archivo: 'alto' filas RGBA de arriba hacia abajo; 'y0' = la primera, contando desde arriba.
// false = no se pudo escribir (corta el render)
typedef bool (*W3dStripeFn)(void* ctx, const unsigned char* rgba, int y0, int height);

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
        // su cuenta) o detras del toggle 'letterboxBlack' (menu Overlays /
        // comando letterboxnegro del harness), que es lo que usan las pruebas de
        // pixeles del encuadre (encuadrepx).
        bool camFrameLetterbox;
        bool letterboxBlack; // toggle: bandas OPACAS como en el dispositivo (default false = atenuadas)
        void RenderCamPassepartout(); // dibuja el borde blanco + oscurece afuera del marco (lo que NO sale en el render)
        void RenderMotionTrail();
        void RTGizmosOnTop(Object* o);   // camaras y luces encima de la imagen trazada     // Motion Trail: el camino de los objetos animados, en x-ray
        void RenderSnapIndicator();   // recuadro verde en el target de snap bajo el cursor
        void RenderArmaturesOnTop(Object* node); // huesos del esqueleto (lineas azules) encima de todo
        int  PickBone(class Armature* arm, float lmx, float lmy); // Pose Mode: hueso mas cercano al click (px), o -1
        // INSPECCION en vista de camara: paneo/zoom de la VISTA (no mueve la camara) para ver con detalle dentro y
        // fuera del marco. Se aplica a la proyeccion + al marco. La camara NO se toca (el render no cambia).
        float camViewZoom; float camViewPanX; float camViewPanY;
        // PANTALLA DEL JUEGO dentro de este viewport: el rect LOCAL donde RenderUI dibujo
        // el HUD (mirando por la camara es el MARCO del passepartout; si no, el area util)
        // + la escala canvas->screen (uniforme SIEMPRE: nada se deforma). Lo lee el
        // input del Play (SimJuego::MapToGame) para que el toque caiga EXACTAMENTE
        // donde se dibuja. hudW = 0 -> este viewport aun no dibujo su HUD.
        float hudX0, hudY0, hudW, hudH, hudEsc;
        // true = el HUD se dibujo con el LIENZO OVERRIDEADO a esta pantalla (modo juego
        // con UI "igual que el render"): el input lo re-fija antes de mapear, como el
        // Editor2D en vista de juego (coherencia con varios viewports).
        bool hudOverride;
        bool showOverlays;
        // SOLO LA ESCENA: Render dibuja el 3D sin el chrome del area (barra, menus, pincel, caja de seleccion) ni el
        // mapa de oclusion de la pintura. Para mostrar la vista en otra pantalla (la de arriba de la 3DS)
        bool onlyScene;
        // los 6 flags del overlay "Statistics" -> PER-VIEWPORT (antes globales en OpcionesRender). MISMO nombre que
        // los globales viejos a proposito: RenderStats y el menu Overlays>Statistics (metodos de Viewport3D)
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
        // alrededor del pase de escena; las lee RenderStats (overlay
        // "faces: dibujadas/total" y "gl: llamadas por frame").
        int statTrisFrame;   // triangulos emitidos (indices/3) por el pase de escena
        int statDrawsFrame;  // draw calls de triangulos
        int statBindsFrame;  // glBindTexture REALES (los cacheados no cuentan)
        int statStatesFrame; // cambios de estado que llegaron al driver
        int statOtherFrame;  // matrices, glGet, punteros y binds de VBO (w3dEngine::g_otherStat)
        // (aca vivia ShowUi, que salteaba RenderUI entero: dada de baja. RenderUI no es un
        //  overlay, es el chrome del area + el reseteo del estado GL 2D; sin el, viewport
        //  inusable y estado sucio. El lector de .w3d IGNORA la prop si viene guardada.)
        bool showFloor;
        bool showYaxis; 
        bool showXaxis; 
        // el CANDADO de la vista de camara (abajo al centro): cerrado = orbitar / panear / zoom MUEVEN la camara
        // activa; abierto (de fabrica) = orbitar te saca de la vista de camara (el paneo y el zoom son de inspeccion)
        bool CameraToView; 
        bool showOrigins;
        bool showArmature; // overlays por tipo de objeto (submenu "Objects" del menu de overlays)
        bool showLights;
        bool showCamera;
        bool showEmpty;
        bool showParticles;   // overlay "Objects": gizmo (flecha) de los emisores de particulas. Default ON.
        bool showCurves;      // overlay "Objects": la LINEA de las curvas/rieles. Default ON (se puede ocultar).
        bool showSounds;      // overlay "Objects": el icono de parlante de los Sonido (oculto = tampoco se clickea)
        // LAS SONDAS en esta vista (menu Overlays): apagadas, las de luz no alumbran / las de reflejo no reflejan aca (el
        // antes y despues, sin tocar la escena ni las otras vistas). No dependen de "Show Overlays": cambian la luz
        bool useLightProbes;
        bool useReflectionProbes;
        bool showHitbox;      // overlay "Objects": el alambre de los hitbox. Default ON (oculto = tampoco se clickea).
        bool show3DCursor;
        bool ShowRelationshipsLines; 
        bool clearScreen;
        bool lockOrbit; // "Lock Orbit": si ON, TODO lo que orbita (mouse/touch/flechas) PANEA en su lugar
                        // (el orbital nunca gira). Para usar el viewport como tablero 2D. El zoom no cambia.
        RenderType view;

        GLfloat nearClip;
        float NearView() const;   // el plano cercano con que se dibuja la orbita (en el telefono, crece con la distancia)
        GLfloat farClip;
        GLfloat aspect;

        // color de fondo del viewport en modo solid/wireframe/material (POR-VIEWPORT, editable en codigo).
        // Sentinel alpha < 0 = usar el color del tema (ColorList[background]), que es el comportamiento
        // por defecto. Es OTRO color que el fondo del RENDER (g_renderBg, global). Sin UI por ahora.
        float bgSolid[4];

        // --- Rotación orbital / libre ---
        Quaternion viewRot; // rotación de la cámara
        Vector3    viewPos;                  // posición de la cámara precalculada
        Vector3    pivot;                    // punto de interés a orbitar
        float      orbitDistance; // distancia al pivote (zoom)

        // --- LOCAL VIEW (aislar la seleccion en ESTE viewport, tecla "/" / menu View) ---
        bool localViewActive;
        std::set<Object*> localViewSet;   // objetos visibles mientras dura el aislamiento
        std::set<Object*> localViewStep;  // sus PADRES: se atraviesan (por la matriz) pero no se dibujan
        Quaternion lvViewRot; Vector3 lvPivot; float lvOrbitDistance;   // camara guardada al entrar (se restaura al salir)
        bool lvOrtho; bool lvFromCam; float lvZoom, lvPanX, lvPanY;
        unsigned int lvSpawned;   // W3dObjectsSpawned() la ultima vez que se miro: lo que nace despues entra a la vista local
        void LocalViewToggle();
        void LocalViewEnter();
        void LocalViewExit();

        Viewport3D(Vector3 pos = Vector3(0,0,0));

        virtual ~Viewport3D();

        void ReloadLights();
        void ChangeViewType();
        void Resize(int newW, int newH) W3D_OVERRIDE;
        void SetShowOverlays(bool value);
        // abre el menu de overlays (checkboxes) apuntando a los flags de ESTA
        // instancia; lo llama el click del boton "Overlays" de la barra
        void OpenMenuOverlays(int x, int y);
        void Render() W3D_OVERRIDE;

        // render de la escena a un PNG de outW x outH (puede ser mayor que la ventana; por tiles,
        // anda hasta en el N95). Sin overlay. pass = Rendered / ZBuffer / NormalView / Alpha. true si guardo.
        // progBase/progTotal: para la barra de progreso (tiles previos / total). 0 = sin barra.
        int  NeededTiles(int outW, int outH) const; // tiles de un render (para el total de la barra)
        bool RenderAPNG(int outW, int outH, RenderType::Enum pass, const char* filename,
                        int progBase = 0, int progTotal = 0);
        // lo mismo a un buffer RGBA de outW x outH con las filas de ARRIBA hacia abajo (sin guardar)
        bool RenderABuffer(int outW, int outH, RenderType::Enum pass, std::vector<unsigned char>& rgba,
                           int progBase = 0, int progTotal = 0);
        // LAS FRANJAS (ver el .cpp): la imagen de arriba hacia abajo, de a una fila de tiles, a 'fn' (sin la imagen entera)
        bool RenderStripes(int outW, int outH, RenderType::Enum pass, W3dStripeFn fn, void* ctx,
                           int progBase = 0, int progTotal = 0);
        // la matriz PROYECCION x VISTA de la imagen entera de outW x outH de RenderAPNG (desde la camara activa si hay)
        void LensImage(bool& ortho, float& fov, float& n, float& f) const;   // el lente del render (la camara activa)
        void MatrixImage(int outW, int outH, float* VP16);
        void VectorView();   // la vista "Motion Vectors": la escena con el color de su movimiento desde el cuadro anterior
        void MotionView(bool data, float far);   // (la de arriba, o el pase de DATOS del postprocesado: + profundidad)
        void DatumView(bool depth, bool ownZ = false);   // ownZ: sin la escena dibujada (de antemano)
        bool RectPost(int glY, int& rx, int& ry, int& w, int& h) const;   // el marco de la camara (GL) del postprocesado
        void PostPreviousData(int glY);   // la profundidad / el movimiento de los Postproceso del arbol, ANTES de la escena   // un pase de datos del postprocesado (la profundidad o el movimiento): lo limpia y lo dibuja
        void RunPostProcess(int glY);   // EL POSTPROCESADO de la vista Rendered (render/PostProcess.h): solo mirando por la
                                      // camara activa y adentro de su marco (en el modo juego puro, todo el viewport)
        void PassDepthCPU();    // (sin shaders) la escena en ZBuffer sobre este viewport, para leerla
        float FarPost() const { return farClip < 200.0f ? farClip : 200.0f; }

        void RenderFloor();
        void RenderAllAxisTransform();
        void RenderOverlay();

        void UpdateViewOrbit();
        // la vista de ESTE viewport como camara del core (la Camera activa si se mira por ella, o la orbita).
        CameraBase ViewCam() const;
        // publica esta vista al core (W3dViewBind) y NADA MAS: no mueve la camara de escena, no toca las
        // luces y no carga la matriz de GL. Es lo que llama el que solo necesita PREGUNTAR "como se ve desde
        // aca" fuera del dibujo (el foco, el input), porque lo que se ve depende de cual fue el ultimo bind.
        void BindView();
        void RotateOrbit();
        // orbita con un paso fijo (flechas del keypad en Symbian): setea dx/dy
        // y aplica RotateOrbit. ndx>0 = der, ndy>0 = abajo.
        void OrbitArrow(int ndx, int ndy);
        void RollOrbit(float angleDeg);
        void RecalcOrbitPosition();
        void Pan();
        void PanArrow(int ndx, int ndy);          // paneo por teclado (keypad N95: * + flechas)
        void Zoom(float delta);
        // proyecta un punto del mundo a coords de PANTALLA del viewport (0..w,
        // 0..h, y hacia abajo). false si esta detras de la camara. Lo usa el
        // "rotar desde la vista" (trackball) para el angulo del mouse al pivot.
        // outW (opcional): el divisor de perspectiva del punto (profundidad eye-space en perspectiva, 1.0 en
        // ortografica). Sirve para interpolar dentro de un triangulo de forma PERSPECTIVE-CORRECT (bary/outW).
        bool ProjectPoint(const Vector3& p, float& sx, float& sy, float* outW = 0);
        // LA LENTE con la que se dibuja este viewport, para las cuentas en CPU (proyectar, rayo desde un pixel,
        // tamanio del gizmo). Mirando por la camara es la de la CAMARA: su pos/rot/fov/orto, el aspecto de la
        // imagen, el marco del passepartout y el zoom/pan de inspeccion (ver W3dScene3DProjection). Antes estas
        // cuentas usaban SIEMPRE la orbita del visor, asi que desde la camara el gizmo cambiaba de tamanio mal.
        // ndc = (e.xy / (sx,sy)) [/ ez en perspectiva] * zoom + pan
        struct Lens { Vector3 pos, right, arr, fwd; bool ortho; float sx, sy, zoom, panX, panY; };
        Lens CurrentLens() const;
        void LoadProjectionView();   // la proyeccion con la que se dibuja (Render y los picks: ver el .cpp)
        // mundo-por-pixel al arrastrar (mover/extrude): a la PROFUNDIDAD del pivot de transform, para
        // que lo agarrado se mueva 1:1 con el mouse/flechas en pantalla a cualquier zoom.
        float DragVelocityWorld();
        // actualiza los endpoints de la linea punteada pivot->mouse (rotar/
        // escalar). mx,my = mouse en coords de ventana. Compartido PC/Symbian.
        void UpdateLineTransform(int mx, int my);
        // "rotar desde la vista" (trackball): rota alrededor del eje de camara
        // segun el angulo del mouse al pivot EN pantalla. Compartido PC/Symbian.
        void RotateFromView(int mx, int my);

        void RenderRelationshipsLines();
        void Render3Dcursor();
        void RenderUI();
        void DrawHUDGame();   // la interfaz 2D del juego sobre la pantalla del juego (RenderUI o en su lugar del arbol)
        // la PANTALLA DIVIDIDA de un juego (setVistas(2)): la escena una vez por mitad, cada una con su camara, y el
        // HUD una vez encima. false = no corresponde (se dibuja como siempre)
        bool RenderSplit();
        void State2D();   // el estado 2D completo tras las mallas (lo primero que hace RenderUI)
        // durante un transform reemplaza la barra de botones por una barra de
        // estado (Translate/Scale/Rotate con los valores en vivo)
        void RenderBarTransform();
        // overlay de estadisticas/fps (texto blanco arriba a la derecha)
        void RenderStats();
        void FocusObject();
        // centra el pivote en 'centro' y ajusta orbitDistance para que una esfera de 'radio' entre
        // en el FOV con un pequeño padding (zoom-to-fit del foco '.').
        void FrameRadius(const Vector3& center, float radius);
        void SetViewpoint(Viewpoint value);
        // Vistas por cuadrante: el 1/3/7 del numpad para el telefono (# + flechas). Si estabas orbitando libre, la
        // primera flecha solo ENCUADRA al mas cercano; despues izq/der giran 90 y arriba/abajo van al top/bottom
        // (con tope). Toda vista ortogonal es pitch*yaw -> moverse es sumarle a dos enteros.
        // El boton de contexto de la barra (Object/Pose/Vertex/Edge/Face): decide su icono y si se ve. Es ESTADO,
        // no dibujo -> vive afuera del render y se puede testear.
        void SyncButtonContext();
        void QuadrantViewNav(int dx, int dy);
        void CurrentQuadrantView(int& yaw90, int& pitch, bool& exact) const;
        void RestoreViewport();
        void ChangePerspective();
        void Accept();
        void button_left() W3D_OVERRIDE;
        void mouse_button_up(int button) W3D_OVERRIDE;   // (Symbian: solo con el dedo, W3dTactil)
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE;
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) W3D_OVERRIDE; // 2 dedos: zoom + paneo
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // 1 dedo sobre el TOOLBAR = scroll horiz
        bool ArrowCommon(int fx, int fy);   // las flechas de la vista: orbitar / transform / el iman (ViewPort3D.cpp)
        void KeyRight();
        void KeyLeft();
        void KeyUp();
        void KeyDown();
        void SetAxis(int axis);
        void SetViewFromCameraActive(bool value);

        // ---- BARRA DE HERRAMIENTAS (abajo): el MECANISMO (ToolButtons/scroll/hit/render) vive en
        // ViewportBase (ToolbarBase.cpp, compartido con UV/2D). Aca queda SOLO lo contextual del 3D:
        // sin transform = historial de acciones; durante un transform = orientacion + ejes X/Y/Z
        // (+ aceptar/cancelar si es tactil). Solo si cfg.newUser (Symbian default: off). ----
        bool ToolbarVisible() const W3D_OVERRIDE;   // cfg.newUser
        bool BrushBarVisible() const W3D_OVERRIDE;  // fila radio/valor: solo en Weight Paint
        bool PropBarVisible() const W3D_OVERRIDE;   // barra de influencia: proportional editing ON (Edit/Object)
        void ToolbarSync() W3D_OVERRIDE;            // visibilidad contextual + colores (estado puro)
        void ToolbarActionRole(int role) W3D_OVERRIDE; // que hace cada boton (roles TBR_*)
        bool ClickBarTransform(int mx, int my); // tap TACTIL en la barra de estado del transform -> abre el teclado numerico
        // el CANDADO de la vista de camara (ver CameraToView): si se ve, su rect (local) y el click (pantalla)
        bool LockCameraVisible() const;
        void LockCameraRect(int& rx, int& ry, int& side) const;
        bool LockCameraClick(int mx, int my);
        void RenderLockCamera();
        // la ORIENTACION de lo que se ve: la de la camara activa mirando por ella, la de la orbita si no. Es la que
        // usan mover / rotar / escalar (los ejes de la vista) y la publican camRight / camUp / camForward
        Quaternion RotView() const;
        void PublishAxesView();
        void OrbitFromCamera();   // la orbita del viewport toma la pose de la camara activa (el pivot, adelante)
        void CameraFromOrbit();   // y al reves: la camara activa toma la pose de la orbita (candado cerrado)
        void RenderToolbar() W3D_OVERRIDE;            // + ocultarla en MODO JUEGO (el juego se mira limpio)
        void event_key_down(int keyCode, bool repeat) W3D_OVERRIDE;
        void event_key_up(int keyCode) W3D_OVERRIDE;
        void key_down_return();
};

#endif