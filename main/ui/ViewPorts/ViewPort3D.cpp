#include "objects/LightProbe.h"        // LightProbeNewPass: las sondas de luz tambien en el render a archivo
#include "render/RealTimeShadows.h"    // W3dShadowsPass / Fin: las sombras en tiempo real en el render a archivo
#include "objects/Routine.h"   // g_w3dOnlyGame / W3dHasConstructor: quien limpia la pantalla
#include "W3dScene.h"          // W3dScene3DActive (lua dibujar3D(false): el "Clear" del proyecto no corre)
#include "io/EditorRoots.h"   // el selector de escena/prefab de la barra (BR_Root)
#include "objects/Light.h" // W3dLightsPrePass
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"
#include "edit/MeshEdit.h"   // W3dRenderCornersSeparated
#include "ui/ViewPorts/Gizmo.h"   // gizmo de mover
#include "edit/Proportional.h"   // proportional editing: tecla O, rueda = radio, circulo de influencia
#include "io/EditedTexture.h"
#include "io/UVExport.h"        // pintura de textura: si las UV se pisan, se despliegan antes de pintar
#include "edit/UVUnwrap.h"         // W3dSmartUVProjectAll
#include "io/GeneratedTexture.h"   // la textura para pintar (del color del material)
#include "objects/Materials.h"     // MaterialDefault    // TEXTURE PAINT: los pixeles de la textura de la parte activa
#include "ui/ViewPorts/UVEditor.h"  // UVShownPart: que parte (material) se pinta
#include "W3dNotice.h"
#include <stdlib.h>   // getenv (debug de oclusion)
#include "render/UIOverlay.h"   // la UI 2D dibujada sobre el viewport (simula la ventana)   // T(): los textos salen en el idioma del sistema
#include "ViewPorts/ViewPort3D.h"
#include "Undo.h" // Ctrl+Z: confirmar transform
#include "objects/CameraBase.h" // camara base del core (la vista)
#include "w3dTexture.h" // w3dEngine::SavePNG (render a PNG)
#include <cmath>
#include <cstring> // memcpy (stitch de tiles del render)
#include <cstdio>  // sprintf (formateo portable de la barra de estado)
#ifdef W3D_SYMBIAN
#include <e32std.h> // User::NTickCount() (reloj del profiler en el N95; ~ms, el mismo que usa LayoutTickFPS)
#endif
#include "ViewPorts/Pick3D.h"   // InsetStart (I en Edit Mode)
#include "app/W3dTouchEditor.h"   // W3dEventTouch: en Symbian el camino de PC del 3D corre solo con el dedo
#include <string>
#include "WhiskUI/draw/glesdraw.h"
#include "ui/W3dColors.h" // W3dColors: colores del editor (piso, ejes de transformacion)
#include "render/RenderOptions.h" // flags del overlay de normales
#include "render/RayTracer.h"
#include "ViewPorts/PopUp/TransformRedoPanel.h"   // ajustar la ultima operacion (mover/rotar/escalar)      // vista Rendered + "Ray Tracing": la imagen trazada por CPU encima de la escena GL
#include "render/SceneRender.h"   // EL pase 3D COMPARTIDO con el runtime del juego compilado
#include "objects/Mesh.h"         // overlay de estadisticas (groupedVerts, faces3d)
#include "objects/EditMesh.h"     // foco al centro de la seleccion en edit mode
#include "objects/Armature.h"     // dibujar huesos del esqueleto encima de todo
#include "objects/Particles.h"    // pase de particulas: se dibujan DESPUES de los opacos
#include "objects/UI.h"           // sameAsRender: el HUD del juego elige lienzo/letterbox
#include "animation/SkeletalAnimation.h" // EvaluatePoseSkeleton (pose al reproducir)
#include "animation/Animation.h"          // CurrentFrame
#include "w3dlog.h"                      // w3dLogMemory: la memoria antes / despues de las texturas del Play
// la sim del juego (scripts lua) esta cargada? (play O pausa; false tras Stop)
static bool GameSimActive() { extern bool SimActive(); return SimActive(); }
// dibujando una MITAD de la pantalla dividida (RenderSplit): la mitad es la imagen entera (sin encuadre ni bandas)
// y el Render no vuelve a partir
static bool gSessionView = false;
extern int g_pureGame;
// el aspecto del MARCO de la camara en un viewport de aspecto 'va'. En el MODO JUEGO PURO (VERDE+0, sin la UI) el marco
// es la pantalla: en COVER (2) siempre; con BANDAS (1) solo si la camara declara un aspecto (ese es el del juego: lo de
// afuera, negro). Sin declarar, el aspecto del render no importa ahi: el juego llena la pantalla
static float FrameAspect(float va) {
    if (g_pureGame == 2 || gSessionView) return va;
    if (g_pureGame && !(CameraActive && CameraActive->DeclaredAspect() > 0.01f)) return va;
    const float ra = W3dGameAspect();
    return (ra < 1e-4f) ? va : ra;
}
#include "ViewPorts/LayoutInput.h" // LayoutDeleteEdit (menu Delete en edit mode)
#include "ViewPorts/UVEditor.h"    // roles TBR_Pincel*/TBR_Group (toolbar del modo Weight Paint, compartidos con el UV)
#include "edit/WeightPaint.h"      // pincel + escritura de pesos (modo Weight Paint)
#include "edit/BoxSelect.h"        // B: caja de seleccion (compartida con los otros editores)
#include "edit/BoneEdit.h"         // Edit Mode de ARMATURE: ops de huesos + grab de head/tail (Fase 3)
#include "ViewPorts/PopUp/NumPad.h" // NumPadOpenTransform (teclado tactil sobre la barra de estado)
#include "ViewPorts/TransformUI.h"  // UI compartida del transform (W3dFmtFloat / TextoNum / barra de info)
#include "ViewPorts/PopUp/ConfirmPopup.h"  // OpenConfirmDelete (popup al borrar con la tecla)
#include "ViewPorts/PopUp/ProgressPopup.h" // barra "Rendering..." durante el render por tiles (clave en N95)
#include "ViewPorts/Notifications.h"       // aviso "Loading textures..." al dar Play
#include "importers/import_obj.h"          // PendingTextures / LoadAllPendingTextures
void RebindMaterialMeshPart(); // (def en Properties.cpp) refresca el panel de material tras undo/redo
#include "W3dProfile.h" // g_profShow (ms por categoria) para el overlay Statistics; se define en config/W3dProfile.cpp
#include "render/MotionVectors.h"   // el pase de vectores de movimiento del render a PNG
#include "render/PostProcess.h"     // el postprocesado de la vista Rendered
#include "objects/PostProcessObj.h" // ...puesto en el arbol (el objeto Postproceso)
#include "script/W3dScript.h"   // la pantalla dividida (W3dVistas*: setVistas / setVistaCamara de lua)
#ifdef W3D_SYMBIAN
extern int W3dScreenHeight; // los headers GLES + tipos GL los da GeometryUI.h (via ViewPort3D.h)
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_COORD_REPLACE
#define GL_COORD_REPLACE 0x8862
#endif
#endif

// HARNESS (hudanimpx): si esta puesto, el proximo RenderUI junta aca las posiciones
// RESUELTAS de los elementos del HUD (el MISMO vector de UI2D_DrawOverlay). Asi la
// prueba lee el pixel del elemento EN EL FRAME REAL (3D + HUD), no en una pantalla
// sintetica. NULL (siempre, salvo mientras el harness mide) = cero costo.
std::vector<UI2DPos>* g_hudCapturePos = NULL;

PopupMenu* MenuAdd = NULL;
PopupMenu* MenuImports = NULL; // submenu "Add > Imports": OBJ / FBX / glTF / GLB
PopupMenu* MenuMeshes  = NULL; // submenu "Add > Mesh": las primitivas (8 de las 19 filas del menu)
PopupMenu* MenuPrefabsAdd = NULL; // submenu "Add > Prefab": los prefabs del proyecto (io/PrefabsEditor.h)
PopupMenu* MenuScenesAdd  = NULL; // submenu "Add > Scene": las otras escenas instanciadas (io/PrefabsEditor.h)
PopupMenu* MenuCore = NULL;      // submenu "Add > Core": las RUTINAS (objetos avanzados: pasos del render)
PopupMenu* MenuModifiers = NULL;   // submenu "Add > Modifier Objects": espejo, array, instancia
PopupMenu* MenuLightsAdd = NULL;   // submenu "Add > Lights": luz, sol, foco y la sonda de luz
PopupMenu* MenuSelect = NULL;    // seleccion: All / None / Invert
PopupMenu* MenuObject = NULL;    // operaciones de objeto (solo si hay seleccion)
PopupMenu* MenuAnimation = NULL; // "Animation": keyframes del objeto + Motion Trail (solo con seleccion)
// los submenus de las capas de la animacion de una malla (vertices, normales, UV): insertar un keyframe y borrar la capa
static PopupMenu* MenuKeyMesh = NULL;
static PopupMenu* MenuDeleteLayerMesh = NULL;
// El menu "Animation" se ARMA AL ABRIRLO (LayoutInput.cpp), segun el contexto: lo de las mallas (sus keyframes de
// vertices, normales y UV, y borrar una de esas capas) solo con una MALLA activa; en el Modo Pose, los keyframes de
// los huesos y Reset Pose (los huesos elegidos vuelven a como estan en el Modo Edicion). El item 0 es SIEMPRE Insert
// Keyframe (LayoutSyncInsertKeySubmenu le pone o le saca el submenu de canales).
void MenuAnimationBuild(){
    if (!MenuAnimation) return;
    extern PopupMenu* MenuInsertKey;
    MenuAnimation->Clear();
    MenuAnimation->Add(T("Insert Keyframe"), 510, IconType::keyframe, MenuInsertKey)->shortcut = "@3d.keyframe";
    const bool pose = (InteractionMode == PoseMode);
    const bool mesh = !pose && ActiveObj && ActiveObj->getType() == ObjectType::mesh;
    if (mesh) MenuAnimation->Add(T("Insert Mesh Keyframe"), 0, IconType::mesh, MenuKeyMesh);
    // (en el Modo Pose, Delete/Clear Keyframe son los de los HUESOS: el dispatch de 511/512 lo ve)
    MenuAnimation->Add(T("Delete Keyframe"), 511, IconType::trash);
    MenuAnimation->Add(T("Clear Keyframe"),  512);
    if (pose) MenuAnimation->Add(T("Reset Pose"), 520, IconType::armature);   // PoseClearTransform(0)
    // una CAPA entera de la animacion de la malla (p. ej. las normales de un modelo sin iluminacion). No es un
    // keyframe: es una capa de la animacion, por eso va en su propio submenu.
    if (mesh) MenuAnimation->Add(T("Delete Mesh Layer"), 0, IconType::trash, MenuDeleteLayerMesh);
    MenuAnimation->AddCheck(T("Motion Trail"), 513, &MotionTrailOn);
}
PopupMenu* MenuMesh = NULL;      // edit mode: operaciones de malla (Transform + Extrude)
PopupMenu* MenuTransform = NULL; // submenu de Object: Move / Rotate / Scale
PopupMenu* MenuPose = NULL;      // menu "Pose" (Pose Mode): Insert Keyframe + Transform
PopupMenu* MenuSetOrigin = NULL; // submenu de Object: Geometry/Origin/Cursor
PopupMenu* MenuApply = NULL;     // submenu de Object: Apply Location/Rotation/Scale/All (Ctrl A)
PopupMenu* MenuView = NULL;      // boton "View" (antes de Select): submenus Cameras + Viewpoint
MenuItem* MenuItemLockOrbit = NULL; // el item "Lock Orbit" del menu View (para refrescar su tilde al abrir)
MenuItem* MenuItemLockedCamera = NULL; // "Mover la camara con la vista" (el candado de la vista de camara) en el menu View
MenuItem* MenuItemLocalView = NULL; // el item "Local View" del menu View (tilde = local view activo en el viewport)
PopupMenu* MenuViewpoint = NULL; // submenu de View: Camera/Top/Bottom/Front/Back/Right/Left (numpad)
PopupMenu* MenuCameras = NULL;   // submenu de View: Set Active Object as Camera / Active Camera
PopupMenu* MenuOverlays = NULL;  // overlays del viewport (checkboxes)
PopupMenu* MenuRender = NULL;    // modo de vista: Render/Material/Solid/Wireframe
PopupMenu* MenuOrient = NULL;    // orientacion de transform: Global/Local/View
PopupMenu* MenuMode = NULL;      // modo del objeto activo: Object/Edit/Vertex/Weight/Texture
PopupMenu* MenuSelMode = NULL;   // edit mode: sub-elemento Vertex/Edge/Face

// busca un boton de la barra por su ROL (no por indice) -> reordenar la barra no rompe nada.
Button* BarRoleBtn(std::vector<Button*>& B, int role){
    for (size_t i = 0; i < B.size(); i++) if (B[i] && B[i]->role == role) return B[i];
    return NULL;
}
int BarRoleIdx(std::vector<Button*>& B, int role){
    for (size_t i = 0; i < B.size(); i++) if (B[i] && B[i]->role == role) return (int)i;
    return -1;
}

// "rotar desde la vista" (trackball): el angulo lo da la posicion del mouse
// alrededor del pivot EN pantalla. gTrackballCap = ya capturamos el angulo
// inicial; los Track* son endpoints (coords de pantalla del viewport) de la
// linea punteada pivot->mouse.
// gTrackballCap es GLOBAL (variables.h): BeginRotate() lo resetea al arrancar
static float gTrackballAng0 = 0.0f;
static float gTrackPivX = 0, gTrackPivY = 0, gTrackMouseX = 0, gTrackMouseY = 0;
static bool  gLineValid = false; // hay endpoints validos para la linea punteada

Viewport3D::Viewport3D(Vector3 pos){
    // barra: [0] tipo de viewport (BarCreate), [1] Select, [2] Add,
    //        [3] Object (solo con algo seleccionado), [4] Overlays
    // barra: [0] tipo de viewport, [1] Mode (selector de modo, con icono),
    // [2] SelMode (Vertex/Edge/Face, SOLO en edit mode, al lado de Mode),
    // [3] Select, [4] Add, [5] Object, [6] Overlays, [7] Render, [8] Orient.
    BarCreate(); // [0] = icono de tipo/split
    // Cada boton lleva su ROL estable; el dispatch los busca por rol (BarRoleBtn) -> el ORDEN
    // VISUAL de aca se puede cambiar libremente sin tocar el dispatch. Orient va ANTES de Select.
    Button* b;
    b = new Button(T("Object Mode"), (int)IconType::object); b->role = BR_Mode;
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo si el activo es malla
    b = new Button("", (int)IconType::selVertex); b->role = BR_SelMode;     // SOLO icono (Vertex/Edge/Face)
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo en edit mode
    b = new Button("", (int)IconType::pivotMedian); b->role = BR_Pivot;     // Transform Pivot (solo icono)
        b->dropdown = true; BarButtons.push_back(b);
    b = new Button(T("Orient")); b->role = BR_Orient;                          // orientacion del transform
        b->dropdown = true; BarButtons.push_back(b);                        // (movido ANTES de Select)
    b = new Button("", IconType::snap); b->role = BR_Snap;                  // SNAP (imanta al mover): verde si esta ON
        b->help = "Snap";
        b->dropdown = true; BarButtons.push_back(b);
    b = new Button("", (int)IconType::curve); b->role = BR_Proportional;    // PROPORTIONAL EDITING (O): la curva, verde si ON
        b->help = "Proportional Editing";
        b->dropdown = true; BarButtons.push_back(b);
    // "View" es un ICONO (monitor): el texto ocupaba ancho de barra, que es lo que escasea (sobre todo en el N95).
    b = new Button("", IconType::monitor); b->role = BR_View; b->dropdown = true; b->help = "View"; BarButtons.push_back(b);
    // "Select" y "Add" son ICONOS sin texto (el titulo va en el menu desplegable): el texto
    // "Seleccionar"/"Anadir" comia ancho de barra (escaso, sobre todo en el N95).
    b = new Button("", IconType::selection); b->role = BR_Select; b->dropdown = true; b->help = "Select"; BarButtons.push_back(b);
    b = new Button("", IconType::plus); b->role = BR_Add; b->dropdown = true; b->help = "Add"; BarButtons.push_back(b);
    b = new Button(T("Mesh")); b->role = BR_Mesh;                               // Transform/Snap/Delete comun
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // SOLO en edit mode
    b = new Button("", (int)IconType::save); b->role = BR_TexSave;   // pintura de textura: guardar lo pintado
        b->help = "Save Texture";
        b->dropdown = true; b->visible = false; BarButtons.push_back(b);
    b = new Button("", IconType::keyframe); b->role = BR_Animation;   // icono rombo (keyframe)
        b->help = "Animation";
    b->dropdown = true;
    BarButtons.push_back(b);
    b = new Button("", IconType::object); b->role = BR_Object;              // cubo
        b->help = "Object";
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo con seleccion (no edit)
    // Overlays y Render son ICONOS: el texto comia ancho de barra. El OJO del overlay ademas dice el estado
    // (abierto = se ven; cerrado = apagados), asi que se elige por frame mas abajo, no aca.
    b = new Button("", IconType::visible); b->role = BR_Overlays; b->dropdown = true; b->help = "Overlays"; BarButtons.push_back(b);
    b = new Button("", IconType::camera);  b->role = BR_Render;   b->dropdown = true; b->help = "Render"; BarButtons.push_back(b);
    // la ESCENA / el JUEGO / el PREFAB que se edita: icono + nombre, desplegable (escenas, juegos, prefabs,
    // Nueva escena, Nuevo juego, Nuevo prefab). El icono lo pone W3dRootsButtonSync segun el tipo
    b = new Button("", (int)IconType::camera); b->role = BR_Root; b->dropdown = true; b->caretMenu = true;
        BarButtons.push_back(b);
    // MODO JUEGO: transporte minimo (visible SOLO con la animacion "Juego" activa).
    // Sin ellos, un layout con solo el viewport 3D no tenia forma de pausar/parar.
    b = new Button(T("Stop")); b->role = BR_GameStop; b->visible = false; BarButtons.push_back(b);
    b = new Button(T("Play")); b->role = BR_GamePlay;  b->visible = false; BarButtons.push_back(b);
    b = new Button(T("UV")); b->role = BR_UV;                                  // Mark Seam + proyecciones
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // SOLO en edit
    barAlpha = 0.5f; // en el 3D la barra deja ver la escena detras
    // barra de HERRAMIENTAS (abajo): MISMOS Button que la barra de arriba. El mecanismo (layout/
    // scroll/hit/render) es el COMPARTIDO de ViewportBase (ToolbarBase.cpp); la visibilidad/colores
    // se setean por frame en ToolbarSync (contextual: historial vs controles del transform).
    // Undo / Redo: SIEMPRE presentes (PC y tactil), primeros (izquierda) para que no se pierdan.
    // Flecha izquierda = deshacer (como el "atras" del navegador); derecha = rehacer.
    // (cuadrado = solo icono, ancho = alto: antes lo forzaba el layout de la toolbar a mano)
    b = new Button("", (int)IconType::undo); b->role = TBR_Undo; b->centered = true; b->square = true; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::redo); b->role = TBR_Redo; b->centered = true; b->square = true; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::notifOk);    b->role = TBR_Accept;  b->centered = true; ToolButtons.push_back(b);
    // "Repeat" (solo en extrude): acepta el extrude y vuelve a extruir la seleccion, sin buscar el boton Extrude.
    b = new Button(T("Repeat")); b->role = TBR_Repeat; b->centered = true; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::notifError); b->role = TBR_Cancel; b->centered = true; ToolButtons.push_back(b);
    // modificadores TACTILES (Android/WebGL sin teclado): Shift y Ctrl. Toggle -> quedan encendidos (verde) y
    // togglean LShiftPressed/LCtrlPressed (multi-seleccion, shortest path). Solo visibles con pantalla tactil.
    // Van ANTES de orientacion/ejes: sino durante un transform quedaban al final y se salian de la pantalla.
    b = new Button("Shift"); b->role = TBR_Shift; b->centered = true; ToolButtons.push_back(b);
    b = new Button("Ctrl");  b->role = TBR_Ctrl;  b->centered = true; ToolButtons.push_back(b);
    // "View" (toggle): en Edit Mode, con 1 dedo orbitar/panear/zoom aunque haya una operacion en curso.
    b = new Button("", (int)IconType::monitor); b->role = TBR_View; b->centered = true; b->square = true; ToolButtons.push_back(b); // "vista": el mismo monitor del menu View de arriba
    b = new Button("", (int)IconType::curve); b->role = TBR_Proportional; b->centered = true; b->square = true; ToolButtons.push_back(b); // proportional editing: toggle de un click
    b = new Button("", (int)IconType::snap); b->role = TBR_Snap; b->centered = true; b->square = true; b->help = "Snap"; ToolButtons.push_back(b); // el iman: toggle de un click
    b = new Button(T("Global")); b->role = TBR_Orient; b->dropdown = true; ToolButtons.push_back(b);
    // las opciones de INSERTAR CARAS y del BEVEL (solo mientras dura cada uno; prendidas = en verde)
    b = new Button(T("Individual"));   b->role = TBR_InsetIndividual; b->centered = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Boundary"));     b->role = TBR_InsetBoundary;   b->centered = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Outset"));       b->role = TBR_InsetOutset;     b->centered = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Depth"));        b->role = TBR_InsetDepth;      b->centered = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("-");               b->role = TBR_BevelSegLess;    b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("+");               b->role = TBR_BevelSegMore;    b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Vertices"));     b->role = TBR_BevelVertices;   b->centered = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("X"); b->role = TBR_AxisX; b->centered = true; b->square = true; ToolButtons.push_back(b);
    b = new Button("Y"); b->role = TBR_AxisY; b->centered = true; b->square = true; ToolButtons.push_back(b);
    b = new Button("Z"); b->role = TBR_AxisZ; b->centered = true; b->square = true; ToolButtons.push_back(b);
    // PINCEL (modo Weight Paint): modo (+/-/=) y grupo activo. Los ROLES son los reservados en
    // UVEditor.h (TBR_Pincel*/TBR_Group): la MISMA toolbar contextual que el UV en modo pintura.
    // Textos por frame en ToolbarSync (labels de WeightPaintLabels).
    // El RADIO y el VALOR no tienen boton: son la fila de barras deslizables de arriba de la
    // toolbar (ViewportBase::RenderBrushBar), que se arrastran sin abrir ningun desplegable.
    b = new Button("+");     b->role = TBR_BrushMode;   b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Group")); b->role = TBR_Group;        b->dropdown = true; b->visible = false; ToolButtons.push_back(b);
    // MARCAS: icono de malla. Prendido = se ven los cuadraditos pintables y el pincel trabaja
    // sobre ELLOS (al valor de la barra, sin falloff), no sobre la superficie.
    b = new Button("", (int)IconType::mesh); b->role = TBR_Marks; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::selVertex); b->role = TBR_PerVertex; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b); // vertex color: por vertice / por corner
    // FALLOFF: abre el editor (lista de presets + curva custom). El texto/icono del boton
    // muestran el falloff ACTIVO, asi se ve cual esta puesto sin abrir nada.
    b = new Button(T("Smooth")); b->role = TBR_Falloff; b->dropdown = true; b->visible = false; ToolButtons.push_back(b);
    // VERTEX PAINT: el color del pincel, como cuadradito relleno (el boton se tinta con el color
    // vivo). Lo abre el ColorPicker de siempre -> color libre o color de PALETA por indice.
    b = new Button(""); b->role = TBR_ColorBrush; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    // "editar solo lo seleccionado" (mascara de pintura): toggle GLOBAL compartido con la
    // toolbar del UV editor (WeightPaintOnlySel); icono de seleccion, tinte accent = ON
    b = new Button("", (int)IconType::selection); b->role = TBR_OnlySel; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    for (int i = 0; i < 8; i++){ // historial de acciones (MRU, hasta 8)
        b = new Button(""); b->role = TBR_Hist + i; b->visible = false; ToolButtons.push_back(b);
    }
    if (!MenuAdd){
        // el menu Add es DECLARATIVO: la lista de items (texto/icono/accion) vive en una tabla en LayoutInput
        // (LayoutBuildMenuAdd). Aca solo se crean los objetos-menu; el contenido lo pone el builder.
        MenuAdd = new PopupMenu();
        MenuImports = new PopupMenu();
        MenuMeshes = new PopupMenu();
        MenuPrefabsAdd = new PopupMenu();
        MenuScenesAdd  = new PopupMenu();
        MenuCore = new PopupMenu();
        MenuModifiers = new PopupMenu();
        MenuLightsAdd = new PopupMenu();
        extern void LayoutBuildMenuAdd();
        LayoutBuildMenuAdd();
        MenuAdd->title = T("Add");   // el boton es un icono sin texto -> el menu lleva titulo
    }
    if (!MenuSelect){
        MenuSelect = new PopupMenu();
        MenuSelect->title = T("Select");   // el boton es un icono sin texto -> el menu lleva titulo
        MenuSelect->Add(T("All"), 0)->shortcut = "@3d.seleccionarTodo";
        MenuSelect->Add(T("None"), 1)->shortcut = "@3d.deseleccionar";
        MenuSelect->Add(T("Invert"), 2)->shortcut = "@3d.invertir";
    }
    if (!MenuObject){
        // submenu Transform (Move/Rotate/Scale = G/R/S)
        MenuTransform = new PopupMenu();
        MenuTransform->Add(T("Move"), 100)->shortcut = "@3d.mover";
        MenuTransform->Add(T("Rotate"), 101)->shortcut = "@3d.rotar";
        MenuTransform->Add(T("Scale"), 102)->shortcut = "@3d.escalar";
        // submenu Set Origin (mueve el origen y/o la geometria)
        MenuSetOrigin = new PopupMenu();
        MenuSetOrigin->Add(T("Geometry to Origin"), 200);
        MenuSetOrigin->Add(T("Origin to Geometry"), 201);
        MenuSetOrigin->Add(T("Origin to 3D Cursor"), 202);
        // "Clear Original File": borra Mesh::origen (el .obj/.fbx del que se importo). Es lo
        // UNICO que apaga el aviso "N file(s) external(s) NO estan" cuando el usuario movio
        // o borro ese archivo; la geometria no se toca (viaja horneada en el .w3dm). Va aca,
        // en el submenu que ya habla del origen del objeto. Ver ForgetOriginSelected.
        MenuSetOrigin->Add(T("Clear Original File"), 203);
        // submenu Apply (Ctrl A): hornea el transform en la malla (ids 220-223 -> LayoutActionObject -> ApplyTransform)
        MenuApply = new PopupMenu();
        MenuApply->title = T("Apply"); // titulo visible tambien cuando se abre standalone con Ctrl+A
        MenuApply->Add(T("Location"), 220);
        MenuApply->Add(T("Rotation"), 221);
        MenuApply->Add(T("Scale"), 222);
        MenuApply->Add(T("All Transforms"), 223);
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido.
        MenuObject = new PopupMenu(); MenuObject->title = T("Object");
        MenuObject->Add(T("Transform"), 0, -1, MenuTransform);    // abre submenu
        MenuObject->Add(T("Set Origin"), 4, -1, MenuSetOrigin);  // abre submenu
        MenuObject->Add(T("Apply"), 6, -1, MenuApply)->shortcut = "@3d.aplicar"; // submenu Location/Rotation/Scale/All
        // IMAN (Shift+S): el cursor 3D a lo seleccionado / al origen, lo seleccionado al cursor... En Modo Objeto solo
        // se abria con el atajo: en pantallas tactiles y en el N95 no habia forma de mover el cursor 3D. Sus opciones
        // se ejecutan en el propio submenu (ActionSnap): los ids 0-7 no chocan con los de este menu.
        { extern PopupMenu* LayoutSubmenuSnap(); MenuObject->Add(T("Snap"), 8, -1, LayoutSubmenuSnap())->shortcut = "@global.menuSnap"; }
        // CORTAR / COPIAR / PEGAR (tambien entre ventanas de Whisk3D: LayoutObjectsCopy), separados con una linea
        MenuObject->Add(T("Cut"), 720, IconType::scissors)->shortcut = "Ctrl+X";
        MenuObject->Add(T("Copy"), 721, IconType::copy)->shortcut = "Ctrl+C";
        MenuObject->Add(T("Paste"), 722, IconType::paste)->shortcut = "Ctrl+V";
        MenuObject->Add(T("Duplicate Objects"), 1)->shortcut = "@3d.duplicar";
        MenuObject->items[MenuObject->items.size() - 1]->lineUp = true;
        MenuObject->Add(T("Duplicate Linked"), 2)->shortcut = "@3d.instancia";
        MenuObject->Add(T("Join"), 5)->shortcut = "@3d.unir"; // une las mallas seleccionadas en el objeto activo
        // Set/Clear Parent (Ctrl P / Alt P): submenus construidos en LayoutInput.cpp (reusan los mismos que
        // los atajos). Solo aparecen aca -> solo en Object Mode (MenuObject no se muestra en Edit).
        MenuObject->Add(T("Set Parent"),   0, -1, LayoutSubmenuSetParent())->shortcut = "@global.emparentar";
        MenuObject->Add(T("Clear Parent"), 0, -1, LayoutSubmenuClearParent())->shortcut = "@global.desemparentar";
        MenuObject->Add(T("Delete"), 3, IconType::trash)->shortcut = "@3d.borrar";
        // PREFABS (io/PrefabsEditor.h): lo elegido pasa a un prefab nuevo (queda una instancia) / la instancia
        // activa se desempaqueta en objetos comunes
        MenuObject->Add(T("Create Prefab"), 700, IconType::prefab);
        MenuObject->Add(T("Unpack Prefab"), 701, IconType::prefab);
        // menu "Animation" (barra, Object Mode): keyframes del TRANSFORM del objeto + Motion Trail. Antes era un
        // SUBMENU de "Object"; es su propio menu de la barra (queda a un click, no a dos).
        // SUBMENU de canales de Insert Keyframe (compartido por el menu Animation, el menu Pose, el
        // menu Animation del UV y la tecla I). Los ids 530-533 burbujean hasta la action del menu
        // TOP (PopupMenu::Click) -> los despacha el mismo LayoutActionObject, sin plomeria nueva.
        // Se aplica a OBJETOS y a POSES de huesos (3D y 2D); en vertices/UV ni se abre.
        extern PopupMenu* MenuInsertKey;
        MenuInsertKey = new PopupMenu(); MenuInsertKey->title = T("Insert Keyframe");
        // su accion DESDE YA: un submenu ejecuta con la suya (PopupMenu::Enter / Click). Antes se ponia recien al
        // abrirlo con la tecla I -> entrando por el menu Animation (el N95, sin la I) elegir "All" no hacia nada
        { extern void LayoutActionObject(int); MenuInsertKey->action = LayoutActionObject; }
        MenuInsertKey->Add(T("All"), 530, IconType::keyframe);  // localizacion + rotacion + escala
        MenuInsertKey->Add(T("Location Only"), 531);
        MenuInsertKey->Add(T("Rotation Only"), 532);
        MenuInsertKey->Add(T("Scale Only"), 533);
        MenuAnimation = new PopupMenu();
        MenuAnimation->title = T("Animation");   // el boton es un icono sin texto -> el menu lleva titulo
        // KEYFRAMES DE LA MALLA: "vertex animation", "normal animation" y "animar los UV" son la MISMA
        // animacion con capas distintas. Este submenu deja elegir cual capturar en vez de dejar que la
        // decida el editor desde el que insertaste (que es el default y sigue estando).
        MenuKeyMesh = new PopupMenu();
        { extern void LayoutActionObject(int); MenuKeyMesh->action = LayoutActionObject; }   // (ids 540-542: ver MenuInsertKey)
        MenuKeyMesh->Add(T("Vertices"), 540, IconType::mesh);
        MenuKeyMesh->Add(T("Normals"),  541, IconType::normalVertex);
        MenuKeyMesh->Add(T("UV"),       542, IconType::texture);
        MenuDeleteLayerMesh = new PopupMenu();
        { extern void LayoutActionObject(int); MenuDeleteLayerMesh->action = LayoutActionObject; }   // (ids 550-552)
        MenuDeleteLayerMesh->Add(T("Vertices"), 550, IconType::mesh);
        MenuDeleteLayerMesh->Add(T("Normals"),  551, IconType::normalVertex);
        MenuDeleteLayerMesh->Add(T("UV"),       552, IconType::texture);
        MenuAnimationBuild();   // (se vuelve a armar cada vez que se abre: depende del modo y del objeto activo)
        // menu "Pose" (Pose Mode): Insert Keyframe + submenu Transform (Move/Rotate/Scale). Reusa MenuTransform.
        extern PopupMenu* MenuPose;
        // (regla de los titulos: el boton de contexto es un icono sin texto -> su menu lleva titulo)
        MenuPose = new PopupMenu(); MenuPose->title = T("Pose");
        MenuPose->Add(T("Insert Keyframe"), 500, IconType::armature, MenuInsertKey)->shortcut = "@3d.keyframe"; // submenu de canales
        MenuPose->Add(T("Transform"), 0, -1, MenuTransform); // Move(G)/Rotate(R)/Scale(S) -> ids 100/101/102
        // Clear Transform: resetea la pose de los huesos SELECCIONADOS a rest. All (T+R+S) / Translation / Rotation / Scale.
        static PopupMenu* MenuClearPose = NULL;
        MenuClearPose = new PopupMenu();
        { extern void LayoutActionObject(int); MenuClearPose->action = LayoutActionObject; }   // (ids 520-523: ver MenuInsertKey)
        MenuClearPose->Add(T("All"), 520, IconType::armature);          // T+R+S -> PoseClearTransform(0)
        MenuClearPose->Add(T("Translation"), 521)->shortcut = "@3d.limpiarPos";     // PoseClearTransform(1)
        MenuClearPose->Add(T("Rotation"), 522)->shortcut = "@3d.limpiarRot";        // PoseClearTransform(2)
        MenuClearPose->Add(T("Scale"), 523)->shortcut = "@3d.limpiarEsc";           // PoseClearTransform(3)
        MenuPose->Add(T("Clear Transform"), 0, -1, MenuClearPose);
        // menu "Mesh" (Edit Mode): comun a vertice/borde/cara. Transform (arriba), Snap y Delete (abajo). Los
        // submenus Snap/Delete se reusan de LayoutInput.cpp. La accion (LayoutActionMesh) se asigna al abrirlo.
        // submenu Transform de EDIT (como el de objeto pero + Shrink/Fatten, que solo tiene sentido en malla)
        static PopupMenu* MenuTransformEdit = NULL;
        MenuTransformEdit = new PopupMenu();
        MenuTransformEdit->Add(T("Move"), 100)->shortcut = "@3d.mover";
        MenuTransformEdit->Add(T("Rotate"), 101)->shortcut = "@3d.rotar";
        MenuTransformEdit->Add(T("Scale"), 102)->shortcut = "@3d.escalar";
        MenuTransformEdit->Add(T("Shrink/Fatten"), 103)->shortcut = "@3d.limpiarEsc"; // cada vert por su normal
        MenuMesh = new PopupMenu();
        MenuMesh->title = T("Mesh");
        MenuMesh->Add(T("Transform"), 0, -1, MenuTransformEdit);        // arriba de todo (Move/Rotate/Scale/Shrink)
        MenuMesh->Add(T("Vertex"), 0, -1, LayoutSubmenuEditVertex());   // los de W, los tres (como las barras de Blender)
        MenuMesh->Add(T("Edge"), 0, -1, LayoutSubmenuEditEdge());
        MenuMesh->Add(T("Face"), 0, -1, LayoutSubmenuEditFace());
        MenuMesh->Add(T("Duplicate"), 314)->shortcut = "@3d.duplicar";          // comun a vertice/borde/cara
        MenuMesh->Add(T("Separate"), 316)->shortcut = "@3d.separar";                 // caras selec -> mesh NUEVO
        MenuMesh->Add(T("Merge"), 0, -1, LayoutSubmenuMerge())->shortcut = "@3d.merge"; // suelda verts (limpia duplicados)
        // Auto Merge (opt-in, OFF por defecto): al confirmar un move suelda los verts movidos con los que queden
        // a <= Threshold. Con el auto merge apagado, "Threshold" se ve en gris (->gray = &g_autoMerge).
        // ids 390/391: fuera del rango 0-7 (que LayoutActionMesh rutea a Snap) y sin caso en LayoutActionObject
        // (no-op). El toggle del check y el set del slider los hace el propio item; la accion no tiene que hacer nada.
        MenuMesh->AddCheck(T("Auto Merge"), 390, &g_autoMerge);
        MenuMesh->AddFloat(T("Threshold"), 391, &g_autoMergeThreshold, 0.0f, 0.1f)->gray = &g_autoMerge;
        MenuMesh->Add(T("Normals"), 0, -1, LayoutSubmenuNormals()); // Recalculate Normals + Flip
        MenuMesh->Add(T("Snap"), 0, -1, LayoutSubmenuSnap())->shortcut = "@global.menuSnap";
        MenuMesh->Add(T("Delete"), 360, -1, LayoutSubmenuDelete())->shortcut = "@3d.borrar"; // abajo de todo
    }
    if (!MenuView){
        // boton "View" (antes de Select): submenu Viewpoint con los 7 puntos de vista (mismos atajos del numpad).
        // ids 400-406 -> LayoutActionView -> Viewport3DActive->SetViewpoint(...). Antes eran atajos "ocultos".
        MenuViewpoint = new PopupMenu();
        MenuViewpoint->Add(T("Camera"), 400)->shortcut = "@3d.vistaCamara";
        MenuViewpoint->Add(T("Top"),    401)->shortcut = "@3d.vistaArriba";
        MenuViewpoint->Add(T("Bottom"), 402)->shortcut = "@3d.vistaAbajo";
        MenuViewpoint->Add(T("Front"),  403)->shortcut = "@3d.vistaFrente";
        MenuViewpoint->Add(T("Back"),   404)->shortcut = "@3d.vistaAtras";
        MenuViewpoint->Add(T("Right"),  405)->shortcut = "@3d.vistaDerecha";
        MenuViewpoint->Add(T("Left"),   406)->shortcut = "@3d.vistaIzquierda";
        // submenu Cameras: setear el objeto activo como camara / ver desde la camara activa
        MenuCameras = new PopupMenu();
        MenuCameras->Add(T("Set Active Object as Camera"), 410)->shortcut = "@3d.camaraActiva";
        MenuCameras->Add(T("Active Camera"),               411)->shortcut = "@3d.vistaCamara";
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido. El boton View ahora es un icono -> titulo.
        MenuView = new PopupMenu(); MenuView->title = T("View");
        MenuView->Add(T("Cameras"),   0, -1, MenuCameras);   // abre submenu (antes de Viewpoint)
        MenuView->Add(T("Viewpoint"), 0, -1, MenuViewpoint); // abre submenu
        MenuView->Add(T("Frame Selected"), 420)->shortcut = "@3d.encuadrar"; // enfocar la seleccion (FocusObject)
        MenuItemLocalView = MenuView->Add(T("Local View"), 422); // aisla la seleccion en ESTE viewport (tilde = activo)
        MenuItemLocalView->shortcut = "@3d.vistaLocal";
        MenuView->Add(T("Perspective/Ortho"), 407)->shortcut = "@3d.perspectiva"; // alterna perspectiva/ortografica
        // Lock Orbit: item REGULAR (no checkbox) -> al tocarlo togglea Y CIERRA el menu. El
        // tilde (verde) se refresca al abrir el menu, con el estado del viewport activo (LayoutInput).
        MenuItemLockOrbit = MenuView->Add(T("Lock Orbit"), 421);
        MenuItemLockOrbit->shortcut = "@tel:9";   // solo el keypad del N95 tiene tecla para esto
        // el CANDADO de la vista de camara tambien desde el menu: en el N95 sin mouse no hay como tocarlo
        MenuItemLockedCamera = MenuView->Add(T("Lock the camera to the view"), 423);
    }
    if (!MenuRender){
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido.
        MenuRender = new PopupMenu(); MenuRender->title = T("Render");
        MenuRender->Add(T("Render Preview"), 0);
        MenuRender->Add(T("Material Preview"), 1);
        MenuRender->Add(T("Solid Preview"), 2);
        MenuRender->Add(T("Wireframe Preview"), 3);
        MenuRender->Add(T("ZBuffer Preview"), 4);
        MenuRender->Add(T("Normal View"), 5); // simula normal map con 3 luces R/G/B
        MenuRender->Add(T("Alpha Preview"), 6); // matte blanco/negro por alpha (debug del pase alpha)
        MenuRender->Add(T("Motion vectors"), 8); // lo que se mueve cada pixel desde el cuadro anterior
        // wireframe SOLO de lo que se dibuja (default ON): con visibilidad por
        // triangulo activa (modificador Culling) el alambre muestra la lista
        // ACTIVA de la celda, no todas las aristas. El flag es global del Core
        // (w3dGraphics.h): el checkbox lo togglea directo.
        MenuRender->AddCheck(T("Wireframe: Only Visible"), 7, &w3dRenderWireframeOnlyVisible);
    }
    if (!MenuOrient){
        // orientacion usada al constrenir a un eje (X/Y/Z) y por el extrude. Default Global.
        MenuOrient = new PopupMenu();
        MenuOrient->title = T("Transform Orientations");
        MenuOrient->Add(T("Global"), 0);
        MenuOrient->Add(T("Local"), 1);
        MenuOrient->Add(T("Normal"), 3); // = la direccion de la normal (lo que hace el extrude)
        MenuOrient->Add(T("View"), 2);
    }
    if (!MenuMode){
        // modo del objeto activo (solo malla). Edit y los Paint todavia no
        // hacen nada: dejamos la opcion para mas adelante.
        MenuMode = new PopupMenu();
        MenuMode->Add(T("Object Mode"), 0, IconType::object); // icono de objeto
        MenuMode->Add(T("Edit Mode"), 1, IconType::mesh);     // icono de malla 3d
        MenuMode->Add(T("Vertex Paint"), 2, IconType::mesh);
        MenuMode->Add(T("Weight Paint"), 3, IconType::mesh);
        MenuMode->Add(T("Texture Paint"), 4, IconType::mesh);
    }
    if (!MenuSelMode){
        // sub-elemento de Edit Mode (la seleccion -todo/nada/invertir- y el render
        // se refieren a esto). Vertex es el default.
        MenuSelMode = new PopupMenu();
        MenuSelMode->Add(T("Vertex"), SelVertex, IconType::selVertex);
        MenuSelMode->Add(T("Edge"),   SelEdge,   IconType::selEdge);
        // Face se DESHABILITA (gris) editando una malla sin caras (un path/riel)
        { extern bool g_selFaceAvailable;
          MenuSelMode->Add(T("Face"), SelFace, IconType::selFace)->gray = &g_selFaceAvailable; }
    }
    // (eran inicializadores de clase: C++03)
    orthographic = false;
    ViewFromCameraActive = false;
    camFrameOn = false; camFrameNX = 1.0f; camFrameNY = 1.0f; camFrameLetterbox = false;
    letterboxBlack = false;   // editor: bandas ATENUADAS por default (auditar el culling)
    statTrisFrame = 0; statDrawsFrame = 0; statBindsFrame = 0; statStatesFrame = 0; statOtherFrame = 0;
    camViewZoom = 1.0f; camViewPanX = 0.0f; camViewPanY = 0.0f;
    hudX0 = 0.0f; hudY0 = 0.0f; hudW = 0.0f; hudH = 0.0f; hudEsc = 1.0f; // aun sin HUD dibujado
    hudOverride = false;
    showOverlays = true;
    onlyScene = false;
    // stats por-viewport apagados por defecto (igual que los globales viejos; --stats o el menu los prende)
    OverlayFps = false; OverlayStatVertices = false; OverlayStatFaces = false;
    OverlayStatGL = false; OverlayStatModgen = false; OverlayStatTimes = false;
    showFloor = true;
    showYaxis = true;
    showXaxis = true;
    CameraToView = false;
    showOrigins = true;
    showArmature = true;
    showLights = true;
    showCamera = true;
    showEmpty = true;
    showSounds = true;
    showParticles = true;   // los emisores de particulas SE VEN (al dar Play, GamePrepareViewports apaga los overlays)
    showCurves = true;      // las curvas/rieles se ven por defecto; el checkbox permite ocultarlas
    showHitbox = true;      // el alambre de los hitbox (sin apagar los demas overlays ni el hitbox)
    show3DCursor = true;
    ShowRelationshipsLines = true;
    clearScreen = true;
    lockOrbit = false; // por defecto se orbita normal; el usuario lo activa para modo tablero 2D
    localViewActive = false;   // local view ("/") apagado
    lvSpawned = 0;
    view = RenderType::MaterialPreview;
    nearClip = 0.01f;
    farClip = 1000.0f;
    aspect = 1.0f;
    bgSolid[0] = bgSolid[1] = bgSolid[2] = bgSolid[3] = -1.0f; // sentinel: usar el color del tema
    viewRot = Quaternion::FromEulerYXZ(-30.0f, -23.0f, 0.0f);
    orbitDistance = 10.0f;

    RecalcOrbitPosition();
}

Viewport3D::~Viewport3D() {
    // si este viewport era el dueno del lienzo del juego (modo juego con UI
    // dinamica), soltarlo: un override colgado apuntaria a un viewport muerto
    UI2D_OverrideWindowRemove(this);
    // idem el 3D ACTIVO: abrir un proyecto libera el layout anterior y cambiar el tipo de
    // un viewport lo borra. El primer render de un 3D vivo lo re-engancha (Render).
    if (Viewport3DActive == this) Viewport3DActive = NULL;
};

void Viewport3D::OpenMenuOverlays(int x, int y){
    if (!MenuOverlays){
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido.
        MenuOverlays = new PopupMenu(); MenuOverlays->title = T("Overlays");
    }
    // se reconstruye en cada apertura apuntando a los flags de ESTE viewport
    // (cada instancia 3D tiene los suyos: split view = overlays independientes).
    // Sin titulo: el primer item ES el master "Show Overlays". Si esta off el
    // resto se ve en gris (->gray = &showOverlays) porque sin overlay no se ven.
    MenuOverlays->Clear();
    MenuOverlays->AddCheck(T("Show Overlays"), 0, &showOverlays);
    MenuOverlays->AddCheck(T("Floor"), 1, &showFloor)->gray = &showOverlays;
    MenuOverlays->AddCheck(T("X Axis"), 2, &showXaxis)->gray = &showOverlays;
    MenuOverlays->AddCheck(T("Y Axis"), 3, &showYaxis)->gray = &showOverlays;
    MenuOverlays->AddCheck(T("Origins"), 4, &showOrigins)->gray = &showOverlays;
    MenuOverlays->AddCheck(T("Gizmo"), 45, &g_gizmoOn)->gray = &showOverlays;   // el gizmo de mover (Symbian: OFF por defecto)
    // submenu "Objects": mostrar/ocultar el overlay de cada tipo de objeto (esqueleto / luces / camaras / empties)
    static PopupMenu* MenuOverlayObjects = NULL;
    if (!MenuOverlayObjects) MenuOverlayObjects = new PopupMenu(); // sin titulo (ya sabes que es al abrirlo)
    MenuOverlayObjects->Clear();
    MenuOverlayObjects->AddCheck(T("Armature"), 13, &showArmature, IconType::armature);
    MenuOverlayObjects->AddCheck(T("Lights"),   14, &showLights,   IconType::light);
    MenuOverlayObjects->AddCheck(T("Camera"),   15, &showCamera,   IconType::camera);
    MenuOverlayObjects->AddCheck(T("Empty"),    16, &showEmpty,    IconType::empty);
    MenuOverlayObjects->AddCheck(T("Particles"),17, &showParticles, IconType::arrow);
    MenuOverlayObjects->AddCheck(T("Curves"),   18, &showCurves,   IconType::curve);
    MenuOverlayObjects->AddCheck(T("Hitbox"),   19, &showHitbox,   IconType::hitbox);
    MenuOverlayObjects->AddCheck(T("Sounds"),   20, &showSounds,  IconType::sound);
    MenuOverlays->Add(T("Objects"), 13, IconType::object, MenuOverlayObjects)->gray = &showOverlays;
    MenuOverlays->AddCheck(T("3D Cursor"), 5, &show3DCursor)->gray = &showOverlays;
    MenuOverlays->AddCheck(T("Relationship Lines"), 6, &ShowRelationshipsLines)->gray = &showOverlays;
    // submenu "Normals" (solo en meshes seleccionadas): 3 toggles + slider de tamano. Sin titulo (ya sabes que es al abrirlo).
    static PopupMenu* MenuOverlayNormals = NULL;
    if (!MenuOverlayNormals) MenuOverlayNormals = new PopupMenu();
    MenuOverlayNormals->Clear();
    MenuOverlayNormals->AddCheck(T("Vertex Normal"), 7, &OverlayVertexNormal, IconType::normalVertex);
    MenuOverlayNormals->AddCheck(T("Custom Normal"), 8, &OverlayCustomNormal, IconType::normalCustom);
    MenuOverlayNormals->AddCheck(T("Face Normal"),   9, &OverlayFaceNormal,   IconType::normalFace);
    MenuOverlayNormals->AddFloat(T("Normal Size"),  10, &OverlayNormalSize, 0.0f, 1.0f);
    MenuOverlays->Add(T("Normals"), 7, IconType::normalVertex, MenuOverlayNormals)->gray = &showOverlays;
    // submenu "Statistics": texto blanco arriba a la derecha del viewport. Cada linea es un toggle independiente. Sin titulo.
    static PopupMenu* MenuOverlayStats = NULL;
    if (!MenuOverlayStats) MenuOverlayStats = new PopupMenu();
    MenuOverlayStats->Clear();
    MenuOverlayStats->AddCheck(T("FPS"),      12, &OverlayFps);
    MenuOverlayStats->AddCheck(T("Vertices"), 17, &OverlayStatVertices);
    MenuOverlayStats->AddCheck(T("Faces"),    18, &OverlayStatFaces);
    MenuOverlayStats->AddCheck(T("GL Calls"), 21, &OverlayStatGL); // llamadas GL por frame (draw/bind/estado)
    MenuOverlayStats->AddCheck("Modgen",   19, &OverlayStatModgen);
    MenuOverlayStats->AddCheck(T("Times"),    20, &OverlayStatTimes);
    MenuOverlays->Add(T("Statistics"), 11, -1, MenuOverlayStats)->gray = &showOverlays;
    // (aca estaba "Clear Screen": limpiar la pantalla ahora es PROGRAMABLE. Todo proyecto trae arriba de todo el
    // "Clear color" en el constructor y, despues de el, la rutina "Limpiar pantalla" (Routine.h, W3dDefaultRoutines): quitarla o
    // apagarla = no limpiar, tambien en el editor (ver "QUIEN LIMPIA" en Render).)
    // X-Ray (retopologia): la malla EN EDICION se dibuja semitransparente (30%) sin z-test y sus bordes/vertices
    // siempre encima -> se ven y se pueden SELECCIONAR los verts/aristas de atras (los tapados por las caras).
    // Modo propio, NO se grisa con Show Overlays. Global (una sola malla en edicion a la vez).
    MenuOverlays->AddCheck(T("X-Ray"), 14, &g_xray);
    // Letterbox negro: con encuadre declarado + juego corriendo, las bandas de afuera
    // del marco se pintan OPACAS como en el dispositivo. OFF (default) = atenuadas,
    // para ver que hay afuera y auditar el culling. NO se grisa con Show Overlays
    // (es parte de la vista de juego, no un overlay de edicion).
    MenuOverlays->AddCheck(T("Black Letterbox"), 15, &letterboxBlack);
    MenuOverlays->action = NULL; // el toggle lo hace el propio item (checkbox)
    MenuOverlays->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = MenuOverlays;
}

void Viewport3D::event_mouse_wheel(float dy, int mx, int my) {
    // rueda sobre la BARRA superior = scroll horizontal (en PC no entran todos los botones); sino zoom.
    // Unificado con la barra de propiedades (BarScrollHorizontal).
    if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return;
    if (OnToolbar(mx, my)) { ToolbarScrollBy((int)(dy * 40)); return; } // barra de HERRAMIENTAS (abajo)
    // PROPORTIONAL EDITING: con un transform en curso la rueda cambia el RADIO de influencia, no el zoom
    if (ProportionalApplicable() && state != editNavigation) {
        ProportionalRadiusScale(dy > 0 ? 1.1f : 1.0f / 1.1f); ProportionalReapply(); g_redraw = true; return;
    }
    Zoom(dy * 2.0f); // Zoom() ya distingue: viewport normal = distancia; vista de camara = inspeccion
}

// EL Z DE 16 BITS (N8 / N95 / PSP): en perspectiva la precision cae con d^2/near. Con el near de 0.01 a 5 metros el z
// distinguia de a ~4 cm y la jaula de edicion de ATRAS se veia a traves de la malla (bordes, vertices); en la PSP, con la
// vista alejada, la grilla del piso se veia a traves del cubo. En la orbita se usa un near proporcional a la distancia al
// pivot (2%: a 5 m, 10 cm -> ~4 mm de precision): lo que quede a menos de eso de la camara se recorta, como en cualquier
// visor. La PC (z de 24) queda con el de siempre. Lo usa tambien el pick
float Viewport3D::NearView() const {
#if defined(W3D_SYMBIAN) || defined(W3D_PSP)
    const float n = orbitDistance * 0.02f;
    return (!orthographic && n > nearClip) ? n : nearClip;
#else
    return nearClip;
#endif
}

// suma al radio del foco el bounding de cada objeto seleccionado (esfera centrada en 'c' que
// envuelve la seleccion entera): max(|c - PointFocus| + FocusRadius) sobre los seleccionados.
static void CalcFocusRadiusRec(Object* obj, const Vector3& c, float& r){
    if (obj->select){
        float d = (obj->PointFocus() - c).Length() + obj->FocusRadius();
        if (d > r) r = d;
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) CalcFocusRadiusRec(obj->Childrens[i], c, r);
}

void Viewport3D::FocusObject() {
    // el foco PREGUNTA donde esta lo seleccionado (PointFocus/FocusRadius -> matrices de mundo) y esa
    // respuesta depende de cual es la vista bindeada. Enfocar puede dispararse desde el menu o el
    // atajo sin haber dibujado ESTE viewport ultimo, asi que primero se bindea el suyo.
    BindView();
    // Edit Mode: enfocar la SELECCION de sub-elementos (vertices/aristas/caras): centro + radio
    // -> centra Y ajusta el zoom. Si no hay nada seleccionado, cae al foco del objeto.
    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        float cx, cy, cz, rL;
        if (m->edit && m->edit->SelectionCenterRadius(cx, cy, cz, rL)) {
            Vector3 c = m->LocalToWorld(Vector3(cx, cy, cz));
            FrameRadius(c, m->ScaleRadiusLocal(Vector3(cx, cy, cz), rL)); // radio local -> mundo
            return;
        }
    }
    if (!ObjSelects.empty()) {
        // el FOCO mira el CENTRO GEOMETRICO de la seleccion (las mallas aportan su centro de
        // vertices), NO el pivote de transform. El radio envuelve toda la seleccion (con su tamaño).
        Vector3 c = FocusCenterSelection();
        float r = 0.0f;
        if (SceneCollection)
            for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
                CalcFocusRadiusRec(SceneCollection->Childrens[i], c, r);
        FrameRadius(c, r);
    }
}

void Viewport3D::FrameRadius(const Vector3& center, float radius){
    // MIRANDO POR LA CAMARA ACTIVA: con el CANDADO cerrado se encuadra moviendo la CAMARA (sin girarla: se acerca o se
    // aleja a lo largo de su mirada hasta que lo elegido entra en SU encuadre); abierto, como orbitar: se sale de la
    // vista de camara (desde donde estaba la camara) y se encuadra con la orbita. Sin la UI (juego puro), nada
    const bool byCamera = ViewFromCameraActive && CameraActive;
    if (byCamera && g_pureGame) return;
    const bool moveCamera = byCamera && CameraToView;
    if (byCamera) { OrbitFromCamera(); if (!moveCamera) SetViewFromCameraActive(false); }
    pivot = center;
    if (radius < 0.05f) radius = 0.05f; // un vertice/punto: radio minimo (zoom cercano pero no degenerado)
    // distancia para que una esfera de 'radio' entre en el FOV. Se usa el menor half-FOV (vertical vs
    // horizontal, segun el aspect) para que entre en las DOS direcciones. *1.25 = padding pedido.
    // (moviendo la camara: SU lente y el aspecto de SU marco)
    float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    if (moveCamera) aspect = FrameAspect(aspect);
    float halfV = (moveCamera ? CameraActive->fov : fovDeg) * 0.5f * 3.14159265f / 180.0f;
    float halfH = atanf(tanf(halfV) * aspect);
    float ang = halfV < halfH ? halfV : halfH;
    float s = sinf(ang); if (s < 0.01f) s = 0.01f;
    orbitDistance = radius / s * 1.25f;
    if (orbitDistance < 0.02f)     orbitDistance = 0.02f;
    if (orbitDistance > farClip)   orbitDistance = farClip;
    RecalcOrbitPosition();
    if (moveCamera) CameraFromOrbit();
}

// ---- LOCAL VIEW ("/"): aisla la seleccion en este viewport (solo dibuja los seleccionados + sus hijos),
// encuadra, y al re-tocar restaura vista + visibilidad. ----
static void LV_AddSubtree(Object* o, std::set<Object*>& s){
    if (!o) return;
    s.insert(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) LV_AddSubtree(o->Childrens[i], s);
}
static void LV_AddAncestors(Object* o, std::set<Object*>& s){
    for (Object* p = o->Parent; p; p = p->Parent) s.insert(p);   // padres: para que la recursion ALCANCE al hijo
}
// los objetos que NACIERON con serial >= desde (Separar, Shift+D, Aniadir, Pegar, importar...): entran a la vista local
static void LV_AddSpawned(Object* root, unsigned int from, std::set<Object*>& vis, std::set<Object*>& step){
    if (!root) return;
    for (size_t i = 0; i < root->Childrens.size(); i++){
        Object* o = root->Childrens[i];
        if (o->serial >= from) { LV_AddSubtree(o, vis); LV_AddAncestors(o, step); }
        else LV_AddSpawned(o, from, vis, step);
    }
}
void Viewport3D::LocalViewEnter(){
    localViewSet.clear(); localViewStep.clear();
    for (size_t i = 0; i < ObjSelects.size(); i++){
        LV_AddSubtree(ObjSelects[i], localViewSet);    // el seleccionado + su descendencia
        LV_AddAncestors(ObjSelects[i], localViewStep); // + su cadena de padres: solo de paso (no se dibujan)
    }
    if (SceneCollection) localViewSet.insert(SceneCollection); // la raiz siempre traversable
    lvSpawned = W3dObjectsSpawned();   // desde aca, lo que nazca en esta vista entra a la vista local
    // snapshot de la camara para restaurarla al salir
    lvViewRot = viewRot; lvPivot = pivot; lvOrbitDistance = orbitDistance;
    lvOrtho = orthographic; lvFromCam = ViewFromCameraActive;
    lvZoom = camViewZoom; lvPanX = camViewPanX; lvPanY = camViewPanY;
    localViewActive = true;
    if (ViewFromCameraActive) SetViewFromCameraActive(false);   // (por la camara no se encuadra: se vuelve al salir)
    FocusObject();   // Frame Selected: encuadra la seleccion aislada
    g_redraw = true;
}
void Viewport3D::LocalViewExit(){
    localViewActive = false;
    localViewSet.clear(); localViewStep.clear();
    viewRot = lvViewRot; pivot = lvPivot; orbitDistance = lvOrbitDistance;
    orthographic = lvOrtho; ViewFromCameraActive = lvFromCam;
    camViewZoom = lvZoom; camViewPanX = lvPanX; camViewPanY = lvPanY;
    RecalcOrbitPosition();   // rederiva viewPos desde viewRot/pivot/orbitDistance
    g_redraw = true;
}
void Viewport3D::LocalViewToggle(){
    if (localViewActive) LocalViewExit();
    else if (!ObjSelects.empty()) LocalViewEnter();   // sin seleccion no hace nada
}

void Viewport3D::Zoom(float delta){
    // vista de camara: el zoom es de INSPECCION (escala la vista + el marco), NO mueve la camara.
    if (ViewFromCameraActive){
        if (g_pureGame) return;   // (sin la UI se ve el juego tal cual: nada de inspeccion)
        if (CameraToView && CameraActive) {   // candado cerrado: la CAMARA avanza / retrocede hacia su punto
            OrbitFromCamera();
            const bool vc = ViewFromCameraActive; ViewFromCameraActive = false;
            Zoom(delta);
            ViewFromCameraActive = vc;
            CameraFromOrbit();
            return;
        }
        camViewZoom *= expf(delta * 0.03f);
        if (camViewZoom < 0.2f) camViewZoom = 0.2f; if (camViewZoom > 20.0f) camViewZoom = 20.0f;
        g_redraw = true; return;
    }
    // zoom PROPORCIONAL a la distancia (multiplicativo/exponencial): el paso se adapta al tamaño de
    // lo que se ve. Cerca de algo chico el paso es chico (suave, usable en vertices/triangulos);
    // lejos de algo grande el paso es grande (fuerte). delta>0 = acercar; ~12% por notch de rueda.
    orbitDistance *= expf(-delta * 0.06f);
    if (orbitDistance < 0.02f)   orbitDistance = 0.02f;
    if (orbitDistance > farClip) orbitDistance = farClip;
    RecalcOrbitPosition();
}

// LA VISTA DE ESTE VIEWPORT como camara BASE del core (CameraBase): la camara activa de la
// escena si se esta mirando "por" ella, o la orbita propia del viewport. UNA sola fuente: la
// usan el bind al core y la matriz de vista de GL, que no pueden salirse una de la otra.
CameraBase Viewport3D::ViewCam() const {
    CameraBase cam;
    if (ViewFromCameraActive && CameraActive) {
        cam.pos = CameraActive->pos;
        cam.rot = CameraActive->Rot();
    } else {
        cam.pos = viewPos;
        cam.rot = viewRot;
    }
    return cam;
}

// publica ESTA vista al core y NADA MAS (ver ViewPort3D.h). Contraste con UpdateViewOrbit, que
// ademas ESCRIBE la camara de escena (UpdatePosition/UpdateLookAt le pisan pos/rot al objeto
// Camera), recalcula las luces y carga la matriz de GL: eso es cosa del que va a DIBUJAR.
void Viewport3D::BindView() {
    W3dViewBind(ViewCam());
}

void Viewport3D::UpdateViewOrbit() {
    if (CameraActive){
        CameraActive->UpdatePosition();
        CameraActive->UpdateLookAt();
    }

    W3dView3D v; v.cam = ViewCam();
    // los ejes con los que se mueve / rota / escala son los de lo que se VE en el viewport activo: mirando por la camara
    // activa, los de la camara (antes quedaban los de la orbita y el objeto se iba para cualquier lado)
    if (Viewport3DActive == this) PublishAxesView();

    // luz del NORMAL MAPPING: en RENDER preview usa la luz de ESCENA (Lights[0]) -> el relieve responde a la
    // lampara y su color. En material/solid preview usa la CAMARA (headlight) en blanco -> el material preview NO
    // depende de las luces de escena (no se tienen que ver las luces de escena en material preview).
    Vector3 lightPos = v.cam.pos, lightCol(1, 1, 1);   // headlight neutro
    if (view == RenderType::Rendered && !Lights.empty() && Lights[0]) {
        Matrix4 LW; Lights[0]->GetWorldMatrix(LW);
        lightPos = Vector3(LW.m[12], LW.m[13], LW.m[14]);
        lightCol = Vector3(Lights[0]->diffuse[0], Lights[0]->diffuse[1], Lights[0]->diffuse[2]);
    }
    // publicar la vista al core + cargar la matriz de vista: el MISMO camino que el
    // runtime del juego (EscenaRender). De aca salen el chrome equirect (reflejo
    // respecto de esta camara) y la base right/up/forward del MATCAP por software.
    W3dScene3DCamera(v, lightPos, lightCol);
}

void Viewport3D::RotateOrbit() {
    // VISTA DE CAMARA: con el CANDADO cerrado se orbita la CAMARA (alrededor de un punto adelante suyo); abierto, orbitar
    // te SACA de la vista de camara y sigue como la orbita de siempre desde donde estaba la camara. Sin la UI (juego
    // puro) nada; con "Lock Orbit" el arrastre panea la inspeccion, como antes
    if (ViewFromCameraActive && CameraActive) {
        if (g_pureGame) return;
        if (CameraToView) {
            OrbitFromCamera();
            const bool vc = ViewFromCameraActive; ViewFromCameraActive = false;
            RotateOrbit();
            ViewFromCameraActive = vc;
            CameraFromOrbit();
            return;
        }
        if (lockOrbit) { Pan(); return; }
        OrbitFromCamera();
        SetViewFromCameraActive(false);
    }
    // "Lock Orbit" ON: NUNCA gira el orbital. Todo lo que orbitaria (arrastre de mouse, 1 dedo tactil,
    // flechas via OrbitArrow) PANEA en su lugar. Usa el mismo dx/dy que ya seteo el que llama. El zoom
    // (rueda / pinch) no pasa por aca, asi que sigue igual. Ideal para usar el viewport como tablero 2D.
    if (lockOrbit) { Pan(); return; }

    float sens = 0.3f;

    // Usamos dx y dy como deltas directos
    float deltaYaw = -dx * sens;
    float deltaPitch = -dy * sens;

    // 1. Crear cuaternión de Yaw (Eje Y Global)
    Quaternion qYaw = Quaternion::FromAxisAngle(Vector3(0, 1, 0), deltaYaw);

    // 2. Crear cuaternión de Pitch (Eje X Local)
    // Nota: Usamos (1,0,0) puro porque al multiplicar a la derecha,
    // el cuaternión interpreta esto como el eje X de la propia cámara.
    Quaternion qPitch = Quaternion::FromAxisAngle(Vector3(1, 0, 0), deltaPitch);

    // 3. Aplicar las rotaciones en orden "Sandwich":
    // Yaw Global (Izquierda) * viewRot Actual * Pitch Local (Derecha)
    viewRot = qYaw * viewRot * qPitch;

    // 4. Normalizar siempre para evitar deformaciones por errores de flotantes
    viewRot.normalize();

    RecalcOrbitPosition();
}

void Viewport3D::OrbitArrow(int ndx, int ndy){
    float odx = dx, ody = dy;        // preservar el delta del mouse
    // Lock Orbit ON: las flechas PANEAN (RotateOrbit deriva a Pan). El pan por flechas de '*' (W3dNewPan) NIEGA el
    // delta ("las flechas iban al reves"); aca hay que negar igual, sino el mismo Pan() quedaba INVERTIDO con lock
    // respecto al de '*'. El mouse no pasa por aca (usa RotateOrbit directo, estilo "agarrar", sin negar).
    if (lockOrbit) { ndx = -ndx; ndy = -ndy; }
    dx = (float)ndx; dy = (float)ndy;
    RotateOrbit();
    dx = odx; dy = ody;
}

void Viewport3D::Pan(){
    // vista de camara: el paneo es de INSPECCION (mueve la VISTA + el marco), NO mueve la camara.
    if (ViewFromCameraActive){
        if (g_pureGame) return;   // (sin la UI se ve el juego tal cual: nada de inspeccion)
        if (CameraToView && CameraActive) {   // candado cerrado: se mueve la CAMARA
            OrbitFromCamera();
            const bool vc = ViewFromCameraActive; ViewFromCameraActive = false;
            Pan();
            ViewFromCameraActive = vc;
            CameraFromOrbit();
            return;
        }
        camViewPanX += 2.0f * dx / (float)(width  > 0 ? width  : 1);
        camViewPanY -= 2.0f * dy / (float)(height > 0 ? height : 1);
        g_redraw = true; return;
    }
    ShiftCount = 100;
    const float speed = orbitDistance * 0.002f;

    // mover en el plano de la cámara
    Vector3 right = viewRot * Vector3(1,0,0);
    Vector3 up    = viewRot * Vector3(0,1,0);

    pivot = pivot - right * (dx * speed);
    pivot = pivot + up    * (dy * speed);

    RecalcOrbitPosition();
}

// paneo por TECLADO (mismo patron que OrbitArrow): setea dx/dy, panea, restaura. Lo usa el keypad del N95.
void Viewport3D::PanArrow(int ndx, int ndy){
    float odx = dx, ody = dy;
    dx = (float)ndx; dy = (float)ndy;
    Pan();
    dx = odx; dy = ody;
}

// GESTO DE 2 DEDOS (web / movil): pinch = ZOOM (abrir dedos = acercar); arrastrar el punto medio = PANEO.
// El 1 dedo (orbitar / seleccionar) lo maneja el mouse sintetizado desde el touch; esto es solo 2 dedos.
void Viewport3D::event_finger_gesture(float zoomDelta, float panDx, float panDy){
    if (zoomDelta != 0.0f) Zoom(zoomDelta);
    if (panDx != 0.0f || panDy != 0.0f){
        float odx = dx, ody = dy;       // Pan() usa dx/dy; los guardamos y restauramos (como PanArrow)
        dx = panDx; dy = panDy;
        Pan();
        dx = odx; dy = ody;
    }
}

// El viewport 3D no scrollea CONTENIDO: el gesto de 1 dedo fuera de la barra orbita (lo maneja
// controls.cpp). La barra superior la agarra el gesto lockeado por OnBar/BarScrollBy.
bool Viewport3D::event_finger_scroll(int px, int py, int dx, int dy){
    return false;
}

void Viewport3D::RollOrbit(float angleDeg) {
    // Eje Z local (0, 0, 1) o (0, 0, -1).
    // Usamos -1 para que sea consistente con la dirección de la vista (Forward).
    Quaternion qRoll = Quaternion::FromAxisAngle(Vector3(0, 0, -1), angleDeg);

    // Multiplicar por la DERECHA = Rotación Local
    viewRot = viewRot * qRoll;

    viewRot.normalize();
    RecalcOrbitPosition();
}

Quaternion Viewport3D::RotView() const { return ViewCam().rot; }
void Viewport3D::PublishAxesView(){
    const Quaternion r = RotView();
    camRight   = r * Vector3(1, 0, 0);
    camUp      = r * Vector3(0, 1, 0);
    camForward = r * Vector3(0, 0, -1);
}
void Viewport3D::OrbitFromCamera(){
    if (!CameraActive) return;
    viewRot = CameraActive->Rot();
    viewRot.normalize();
    if (orbitDistance < 0.02f) orbitDistance = 0.02f;
    pivot = CameraActive->pos + (viewRot * Vector3(0, 0, -1)) * orbitDistance;
    RecalcOrbitPosition();
}
void Viewport3D::CameraFromOrbit(){
    if (!CameraActive) return;
    CameraActive->pos = viewPos;
    CameraActive->SetRot(viewRot);
    extern void W3dTouchedChanges();
    W3dTouchedChanges();
    RTInvalidate();
    g_redraw = true;
}
bool Viewport3D::LockCameraVisible() const {
    extern bool AnimIsGame;
    return ViewFromCameraActive && CameraActive && camFrameOn && showOverlays && !g_pureGame &&
           !(AnimIsGame && GameSimActive());
}
// el BOTON del candado: un boton de la UI (fondo + borde), cuadrado, con el icono rojo (abierto) o verde (cerrado)
static Button* gButtonLock = NULL;
static Button* ButtonLock(bool closed) {
    if (!gButtonLock) {
        gButtonLock = new Button("", (int)IconType::lockOpen, true);
        gButtonLock->square = true; gButtonLock->centered = true;
        gButtonLock->help = "Lock the camera to the view";
    }
    static const float red[3] = { 0.90f, 0.25f, 0.22f }, green[3] = { 0.30f, 0.85f, 0.35f };
    gButtonLock->icon = closed ? (int)IconType::lock : (int)IconType::lockOpen;
    gButtonLock->textColor = closed ? green : red;
    gButtonLock->Resize(10000);
    return gButtonLock;
}
// DEBAJO del marco de la camara (afuera, centrado con el), sin taparse con la barra de herramientas (ni con la de menu
// si esta abajo); si abajo del marco no entra, lo mas abajo posible
void Viewport3D::LockCameraRect(int& rx, int& ry, int& side) const {
    Button* b = ButtonLock(CameraToView);
    side = b->height > b->width ? b->height : b->width;
    const float W = (float)width, H = (float)height;
    const float cx = (camViewPanX * 0.5f + 0.5f) * W, cy = (0.5f - camViewPanY * 0.5f) * H;   // (RenderCamPassepartout)
    const float hh = camFrameNY * camViewZoom * H * 0.5f;
    int down = (barCard && barDown) ? BarHeight() : 0;
    if (ToolbarVisible()) down += ToolbarHeight();
    rx = (int)cx - side / 2;
    ry = (int)(cy + hh) + gapGS;
    const int limit = height - down - side - gapGS;
    if (ry > limit) ry = limit;
    if (rx < gapGS) rx = gapGS; if (rx > width - side - gapGS) rx = width - side - gapGS;
}
bool Viewport3D::LockCameraClick(int mx, int my){
    if (!LockCameraVisible()) return false;
    int rx, ry, side; LockCameraRect(rx, ry, side);
    const int lx = mx - x, ly = my - y;
    if (lx < rx || lx >= rx + side || ly < ry || ly >= ry + side) return false;
    CameraToView = !CameraToView;
    g_redraw = true;
    return true;
}
void Viewport3D::RenderLockCamera(){
    if (!LockCameraVisible()) return;
    int rx, ry, side; LockCameraRect(rx, ry, side);
    Button* b = ButtonLock(CameraToView);
    b->sx = x + rx; b->sy = y + ry;   // (rect absoluto: el hover y el cartel de ayuda)
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)rx, (GLfloat)ry, 0);
    b->Render();
    w3dEngine::PopMatrix();
}

void Viewport3D::RecalcOrbitPosition(){
    Vector3 forward = viewRot * Vector3(0,0,-1);
    viewPos = pivot - forward * orbitDistance;

    // los ejes de la VISTA (mirando por la camara activa: los de la camara, ver RotView)
    PublishAxesView();
}

Viewport3D::Lens Viewport3D::CurrentLens() const {
    Lens L;
    const CameraBase cam = ViewCam();        // ESTE viewport (no los globals, que pisa el ultimo render en multi-3D)
    L.pos   = cam.pos;
    L.right = cam.rot * Vector3(1, 0, 0);
    L.arr   = cam.rot * Vector3(0, 1, 0);
    L.fwd   = cam.rot * Vector3(0, 0, -1);      // hacia la escena
    const bool camFrame = (ViewFromCameraActive && CameraActive);
    const float va = (height > 0) ? (float)width / (float)height : 1.0f;
    float fov = fovDeg, ra = va, nx = 1.0f, ny = 1.0f;
    L.ortho = orthographic; L.zoom = 1.0f; L.panX = 0.0f; L.panY = 0.0f;
    if (camFrame) {                            // la MISMA cuenta que Render() + W3dScene3DProjection
        fov = CameraActive->fov; L.ortho = CameraActive->orthographic;
        ra = FrameAspect(va);
        if (camFrameOn) { nx = camFrameNX; ny = camFrameNY; }
        if (nx < 1e-4f) nx = 1.0f; if (ny < 1e-4f) ny = 1.0f;
        L.zoom = camViewZoom; L.panX = camViewPanX; L.panY = camViewPanY;
    }
    // media-extension visible: en orto el volumen (size = orbitDistance*tan(fov/2), sigue el zoom); en
    // perspectiva tan(fov/2) (se divide por la profundidad)
    float h = tanf(fov * 0.5f * 3.14159265f / 180.0f);
    if (L.ortho) { h *= orbitDistance; if (h < 0.001f) h = 0.001f; }
    L.sx = h * ra / nx; L.sy = h / ny;
    return L;
}

bool Viewport3D::ProjectPoint(const Vector3& p, float& sx, float& sy, float* outW){
    const Lens L = CurrentLens();
    Vector3 rel = p - L.pos;
    float ez = rel.Dot(L.fwd);                // distancia hacia adelante
    if (ez < 0.0001f) return false;           // detras de la camara
    // divisor de perspectiva: en perspectiva es la profundidad (para interpolar perspective-correct); en
    // ortografica NO hay division -> 1.0 (la proyeccion es afin, el baricentrico de pantalla ya es el real).
    if (outW) *outW = L.ortho ? 1.0f : ez;
    const float d = L.ortho ? 1.0f : ez;
    float ndcX = rel.Dot(L.right) / (L.sx * d) * L.zoom + L.panX;
    float ndcY = rel.Dot(L.arr) / (L.sy * d) * L.zoom + L.panY;
    sx = (ndcX * 0.5f + 0.5f) * (float)width;
    sy = (1.0f - (ndcY * 0.5f + 0.5f)) * (float)height; // pantalla: Y hacia abajo
    return true;
}

// PICK de HUESO (Pose Mode): proyecta cada hueso (poseHead->poseTail, en world) a pantalla y devuelve el indice
// del mas cercano al click (lmx,lmy en coords LOCALES del viewport), o -1 si ninguno esta dentro del umbral (px).
int Viewport3D::PickBone(Armature* arm, float lmx, float lmy){
    if (!arm || arm->bones.empty()) return -1;
    EvaluatePoseSkeleton(arm, CurrentFrame); // asegurar la pose al frame actual (poseHead/poseTail)
    Matrix4 W; arm->GetWorldMatrix(W);
    int best = -1; float bestD = 14.0f; // umbral de seleccion en pixeles
    for (size_t i = 0; i < arm->bones.size(); i++){
        Vector3 h = W * arm->bones[i].poseHead, t = W * arm->bones[i].poseTail;
        float hx,hy,tx,ty;
        if (!ProjectPoint(h,hx,hy) || !ProjectPoint(t,tx,ty)) continue;
        // distancia del click al SEGMENTO 2D (hx,hy)-(tx,ty)
        float dx=tx-hx, dy=ty-hy; float len2=dx*dx+dy*dy;
        float u = (len2>1e-6f) ? ((lmx-hx)*dx+(lmy-hy)*dy)/len2 : 0.0f;
        if (u<0) u=0; else if (u>1) u=1;
        float px=hx+u*dx, py=hy+u*dy;
        float d = sqrtf((lmx-px)*(lmx-px)+(lmy-py)*(lmy-py));
        if (d < bestD){ bestD=d; best=(int)i; }
    }
    return best;
}

// pivote del GIZMO de transform (linea punteada + lineas de eje X/Y/Z): es el
// TransformPivotPoint (median / 3D cursor / active), NO el objeto activo (que solo
// coincide con el pivote en modo Active o si esta en el median). En Individual
// Origins cae al origen del activo (no hay un pivote unico).
//
// CONDUCTA CONOCIDA con constraints: TransformPivotPoint se calcula en BASE a proposito
// (SetTransformPivotPoint, main/objects/ObjectMode.cpp), porque de el sale lo que se ESCRIBE en
// pos al rotar/escalar. Sobre un objeto con Copy Location eso quiere decir que el gizmo se dibuja
// en el ORIGEN REAL del objeto y no donde el constraint lo esta mostrando. Se ve raro y es lo
// correcto: el gizmo tiene que estar donde de verdad pivota la cuenta que va al .w3d, no donde
// esta el dibujo. La rama de Individual Origins si usa la efectiva, y no contradice nada: con ese
// modo ApplyPivotToTransform sale antes de tocar a nadie, o sea que ese punto NO escribe.
static Vector3 GizmoPivot(){
    if (g_transformPivot == PivotIndividual && ActiveObj) return ActiveObj->GetGlobalPosition();
    return TransformPivotPoint;
}

// VELOCIDAD de arrastre en MUNDO por pixel de pantalla, a la PROFUNDIDAD del pivot de transform.
// Con esto, al arrastrar N pixeles (mouse o flechas) lo agarrado se mueve N pixeles EN PANTALLA a
// cualquier zoom -> se siente "pegado" al cursor. Antes era un 0.01 FIJO que ignoraba el zoom (de
// cerca movia muchisimo, de lejos poquito). Es la inversa exacta de ProjectPoint (misma base/FOV).
float Viewport3D::DragVelocityWorld(){
    if (height <= 0) return 0.01f;                // guarda (no deberia pasar)
    // la MISMA lente que ProjectPoint (mirando por la camara: la de la camara, con marco y zoom)
    const Lens L = CurrentLens();
    float ez = 1.0f;                             // ortho: mundo-por-pixel constante (no depende de z)
    if (!L.ortho) {
        ez = (GizmoPivot() - L.pos).Dot(L.fwd);  // profundidad (eye-space) del pivot de transform
        if (ez < nearClip) ez = nearClip;        // no detras de la camara
    }
    // alto visible a esa profundidad, repartido en 'height' pixeles = mundo por pixel
    return 2.0f * ez * L.sy / (L.zoom > 1e-4f ? L.zoom : 1.0f) / (float)height;
}

void Viewport3D::UpdateLineTransform(int mx, int my){
    if ((state == rotation || state == EditScale) && ActiveObj){
        float px, py;
        if (ProjectPoint(GizmoPivot(), px, py)){
            gTrackPivX = px; gTrackPivY = py;
            gTrackMouseX = (float)mx - (float)x;
            gTrackMouseY = (float)my - (float)y;
            gLineValid   = true;
        }
    }
}

void Viewport3D::RotateFromView(int mx, int my){
    if (!ActiveObj) return;
    float px, py;
    if (!ProjectPoint(GizmoPivot(), px, py)) return; // el angulo se mide desde el PIVOTE
    float lmx = (float)mx - (float)x, lmy = (float)my - (float)y;
    float ang = 180.0f - atan2f(lmy - py, lmx - px) * 180.0f / 3.14159265f;
    if (!gTrackballCap) { gTrackballCap = true; gTrackballAng0 = ang; }
    float delta = ang - gTrackballAng0;
    gAngleTransform = delta; // barra de estado (angulo del mouse)
    Vector3 cf = RotView() * Vector3(0, 0, -1);
    // -delta: el usuario lo quiere al reves (mouse horario -> objeto horario)
    // Edit Mode: el trackball gira los VERTICES seleccionados (rotacion absoluta).
    if (InteractionMode == EditMode && EditXformActive()) {
        EditXformRotAbs(Quaternion::FromAxisAngle(cf, -delta));
        return;
    }
    for (size_t o = 0; o < objectState.size(); o++) {
        Object& ob = *objectState[o].obj;
        // por la puerta: antes se derivaba el euler a mano con ToEulerYXZ mientras el resto del editor
        // usa XYZ -> lo que se guardaba NO era lo que se veia. SetRot lo deriva bien y conserva las vueltas.
        // W3dRotateWorldOver: el eje de la vista es DE MUNDO y el snapshot es LOCAL -> se conjuga por el
        // padre (sin eso, rotar desde la vista un objeto emparentado y rotado giraba por cualquier lado).
        ob.SetRot(W3dRotateWorldOver(ob, Quaternion::FromAxisAngle(cf, -delta * objectState[o].weight), objectState[o].rot)); // peso: proportional
    }
    ApplyPivotToTransform(); // gira las posiciones alrededor del pivote
    { extern void SnapFitObjRot(); SnapFitObjRot(); } // imanta: el activo apunta al target (si snap ON)
}

// ---------------------------------------------------------------------------------------------------------------
//  VISTAS POR CUADRANTE (el telefono no tiene numpad: es su 1/3/7 de PC).
//  Toda vista ortogonal es pitch * yaw, con yaw en {0,90,180,270} (front/right/back/left) y pitch en {-90,0,90}
//  (top/lado/bottom). O sea: la vista es DOS ENTEROS, y moverse es sumarles.
//    - Si estabas orbitando libre, la primera flecha SOLO ENCUADRA al cuadrante mas cercano (no te saltea uno).
//    - izq/der: giran el yaw 90. Desde el top o el bottom tambien, que es "rotar la vista" desde arriba.
//    - arriba/abajo: suben/bajan el pitch, con TOPE en top y en bottom (de arriba no se sigue girando).
// ---------------------------------------------------------------------------------------------------------------
static Quaternion W3dViewQuat(int yaw90, int pitch){   // yaw90 en pasos de 90; pitch en {-1,0,1}
    return Quaternion::FromAxisAngle(Vector3(1,0,0), (float)pitch * -90.0f)
         * Quaternion::FromAxisAngle(Vector3(0,1,0), (float)(yaw90 * 90));
}
// El cuadrante mas parecido a la orientacion actual. 'exacta' = ya estabas EN el (no hace falta encuadrar).
void Viewport3D::CurrentQuadrantView(int& yaw90, int& pitch, bool& exact) const {
    // las 12 combinaciones (4 yaw x 3 pitch) son 12 orientaciones DISTINTAS: top girado 90 no es lo mismo que right.
    // Se prueban todas y gana la mas parecida.
    float best = -1.0f; yaw90 = 0; pitch = 0;
    for (int p = -1; p <= 1; p++){
        for (int y = 0; y < 4; y++){
            Quaternion q = W3dViewQuat(y, p);
            // |dot| entre unitarios: 1 = misma orientacion (q y -q son la misma rotacion, de ahi el abs)
            float d = q.x*viewRot.x + q.y*viewRot.y + q.z*viewRot.z + q.w*viewRot.w;
            if (d < 0.0f) d = -d;
            if (d > best){ best = d; yaw90 = y; pitch = p; }
        }
    }
    exact = (best > 0.9999f);   // coseno del medio angulo: pegado al cuadrante
}
void Viewport3D::QuadrantViewNav(int dx, int dy){
    int yaw90, pitch; bool exact;
    CurrentQuadrantView(yaw90, pitch, exact);
    if (exact){                       // ya encuadrado: la flecha MUEVE
        if (dx > 0) yaw90 = (yaw90 + 1) & 3;
        else if (dx < 0) yaw90 = (yaw90 + 3) & 3;
        if (dy < 0 && pitch < 1) pitch++;    // arriba: hacia el top, y ahi se planta
        else if (dy > 0 && pitch > -1) pitch--;
    }
    // si NO estabas encuadrado, la flecha solo te lleva al cuadrante mas cercano
    ViewFromCameraActive = false; camFrameOn = false;
    camViewZoom = 1.0f; camViewPanX = 0.0f; camViewPanY = 0.0f;
    viewRot = W3dViewQuat(yaw90, pitch);
    viewRot.normalize();
    RecalcOrbitPosition();
    g_redraw = true;
}

void Viewport3D::SetViewpoint(Viewpoint value) {
    ViewFromCameraActive = false;
    camFrameOn = false; camFrameNX = 1.0f; camFrameNY = 1.0f;
    camViewZoom = 1.0f; camViewPanX = 0.0f; camViewPanY = 0.0f;

    switch (value) {
        case Viewpoint::front: {
            // Front: Mirando hacia -Z. Up es +Y. (Identidad)
            viewRot = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
            break;
        }
        case Viewpoint::back: {
            // Back: Mirando hacia +Z. Rotamos 180 en Y.
            viewRot = Quaternion::FromAxisAngle(Vector3(0,1,0), 180.0f);
            break;
        }
        case Viewpoint::right: {
            // Right: Mirando hacia -X. Rotamos 90 grados a la derecha (Y).
            // Nota: Dependiendo de tu convención puede ser 90 o -90.
            // Generalmente +90 en Y convierte el vector Forward (-Z) en (-X).
            viewRot = Quaternion::FromAxisAngle(Vector3(0,1,0), 90.0f);
            break;
        }
        case Viewpoint::left: {
            viewRot = Quaternion::FromAxisAngle(Vector3(0,1,0), -90.0f);
            break;
        }
        case Viewpoint::top: {
            // Top: Mirando hacia -Y. Up es -Z (para que el top de la pantalla sea Norte).
            // Esto es rotar X en 90 grados (pitch down).
            viewRot = Quaternion::FromAxisAngle(Vector3(1,0,0), -90.0f);
            break;
        }
        case Viewpoint::bottom: {
            // Bottom: Mirando hacia +Y.
            viewRot = Quaternion::FromAxisAngle(Vector3(1,0,0), 90.0f);
            break;
        }
    }

    // IMPORTANTE:
    // 1. Normalizar por seguridad (aunque los hardcodeados ya son unitarios)
    viewRot.normalize();

    // 2. Recalcular la posición física de la cámara basada en el nuevo ángulo y el pivote existente
    RecalcOrbitPosition();
}

// Ejemplo de implementación de un método
void Viewport3D::ReloadLights() {
    ::view = view;
    // "Show Overlays" MANDA, tambien con el juego corriendo. Aca habia un gate duro
    // (`&& !W3dGameRunning()`) que apagaba los overlays al dar Play pasara lo que
    // pasara: el checkbox quedaba mentiroso (marcado y sin efecto) y no habia manera
    // de mirar la Curve del riel, los gizmos o los contornos con el juego andando,
    // que es justo cuando se los quiere ver. "El juego se ve limpio" ya lo resuelve
    // GamePrepareViewports(true), que apaga showOverlays AL INICIAR la partida: el
    // default sigue siendo limpio y ahora el usuario puede volver a prenderlos.
    ::showOverlayGlobal = showOverlays;
    ::ViewFromCameraActiveGlobal = ViewFromCameraActive;
    // OJO: el ACTIVO lo decide el hover del mouse, no el render (con dos
    // viewports 3D, el ultimo dibujado robaba la orbita y el G/R/S)
    if (!Viewport3DActive) Viewport3DActive = this;

#ifdef W3D_SYMBIAN
    // N95: SOLO las luces de la escena. Apagar GL_LIGHT0..7 "a lo bestia" (aunque no existan) CUELGA el driver
    // GLES1 del MBX al arrancar (crasheaba). Esto es lo que hacia el codigo original -> seguro en el N95.
    for (size_t l = 0; l < Lights.size(); l++) {
        w3dEngine::SetLightEnabled(Lights[l]->LightID, false);
    }
    // (en el N95 el resto de la preparacion de luces del modo la hace igual la compartida)
#endif
    // LAS LUCES DEL PASE (apagar las 8 de GL + la luz frontal de los modos de preview)
    // viven en EscenaRender, compartidas con el runtime del juego compilado.
    W3dScene3DLights((int)(RenderType::Enum)view);
}

// Cicla los modos de vista: MaterialPreview -> Solid -> NormalView -> Wireframe -> ZBuffer -> Alpha -> Rendered -> (vuelve)
void Viewport3D::ChangeViewType(){
    if      (view == RenderType::MaterialPreview) view = RenderType::Solid;
    else if (view == RenderType::Solid)           view = RenderType::NormalView;
    else if (view == RenderType::NormalView)      view = RenderType::Wireframe;
    else if (view == RenderType::Wireframe)       view = RenderType::ZBuffer;
    else if (view == RenderType::ZBuffer)         view = RenderType::Alpha;
    else if (view == RenderType::Alpha)           view = RenderType::Rendered;
    else                                          view = RenderType::MaterialPreview; // Rendered/otro -> vuelve
}

// Redimensiona el viewport
void Viewport3D::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH); // Llama a la función base
    ResizeBorder(newW, newH);         // Ajusta los bordes
    aspect = (float)newW / (float)newH;
}

// Mostrar u ocultar overlays
void Viewport3D::SetShowOverlays(bool value) {
    showOverlays = value;
}

// WEIGHT PAINT: en modo Weight Paint, prende el degradado de peso en el mesh ACTIVO (y lo apaga en el que se dejo de
// pintar). Recalcula el color cada frame (barato y refleja el cambio de grupo activo al instante). Fuera del modo,
// apaga el ultimo. Se llama antes de renderizar la escena.
static Mesh* g_wpMesh = NULL;
static int WP3DLayerColor(Mesh* m);   // (definida mas abajo, con el pincel de color)
static void WeightPaintUpdate();
void WP3DSyncPaintMode() { WeightPaintUpdate(); }   // harness: lo que hace el frame al cambiar de modo
static void WeightPaintUpdate() {
    Mesh* target = NULL;
    const bool isWeight = (InteractionMode == WeightPaint);
    const bool isColor  = (InteractionMode == VertexPaint);
    // las MARCAS del pincel se recuerdan POR MODO: pintando pesos arrancan apagadas (pincel
    // clasico); pintando color arrancan con los cuadraditos de face corner prendidos, que es
    // como se pinta un corner exacto. Al cambiar de modo cada uno recupera lo suyo.
    {
        static int marksWeight = MarksOff, marksColor = MarksCorner, prevMode = -1;
        const int mode = isWeight ? 1 : isColor ? 2 : 0;
        if (mode != prevMode) {
            if (prevMode == 1) marksWeight  = BrushGet().marks;
            if (prevMode == 2) marksColor = BrushGet().marks;
            if (mode == 1) BrushGet().marks = marksWeight;
            if (mode == 2) BrushGet().marks = marksColor;
            prevMode = mode;
        }
    }
    if ((isWeight || isColor) && ActiveObj && ActiveObj->getType() == ObjectType::mesh)
        target = (Mesh*)ActiveObj;
    // VERTEX PAINT: la malla pintada se renderiza con un vert POR CORNER (sino dos corners fusionados
    // no se pueden pintar por separado, ver W3dRenderCornersSeparated). Al salir, o al cambiar de
    // malla, la anterior se vuelve a fusionar.
    {
        const bool separate = isColor && (target != NULL);
        const bool change = (separate != W3dRenderSeparatedCornersActive()) || (separate && target != g_wpMesh);
        if (change) {
            Mesh* prev = g_wpMesh;
            W3dRenderCornersSeparated(separate);
            if (prev && (prev != target || !separate)) prev->GenerateRender();   // vuelve a fusionar
            if (separate) target->GenerateRender();                                        // separa
        }
    }
    if (g_wpMesh && g_wpMesh != target) {       // apaga el anterior (los DOS pases)
        g_wpMesh->weightPaintOn = false;
        g_wpMesh->vertexPaintOn = false;
    }
    g_wpMesh = target;
    if (!target) return;
    target->weightPaintOn = isWeight;
    target->vertexPaintOn = isColor;
    if (isWeight) {
        WeightPaintEnsureMap(target); // malla del editor recien creada: mapa render-vert -> control-point
        target->BuildWeightColor(target->activeGroup);
    } else if (isColor) {
        // preparar la capa AL ENTRAR al modo, no al primer trazo: es lo que hace que la malla
        // se vea con su vertex color (blanco si nunca tuvo) desde el momento en que entras.
        // Sin esto la malla seguia con su material y luces, y no se entendia que ibas a pintar.
        if (WP3DLayerColor(target) < 0) target->vertexPaintOn = false;
    }
}

// ============================================================================
//  PINTURA DE PESOS en el viewport 3D (modo Weight Paint): click + drag pinta con
//  el pincel sobre el grupo ACTIVO del mesh activo. El trabajo real (falloff,
//  clamp, sparse, undo por trazo) vive en edit/WeightPaint.cpp; aca solo se
//  proyectan los vertices a pantalla (ProjectPoint) y se rutean los eventos.
// ============================================================================
static bool g_wp3dPainting = false; // hay un trazo en curso en un viewport 3D (mouse apretado)
// TRAZO PENDIENTE (pantalla tactil): al apoyar un dedo NO se pinta enseguida; se espera un ratito
// (o que el dedo se mueva) por si viene un 2do dedo, que es un gesto de camara. Asi no se pintan
// pesos/colores sin querer al querer orbitar o hacer zoom. Un tap solo (levantar sin mover) pinta
// UNA pasada en ese punto al soltar.
static bool   g_wpPend = false;
static int    g_wpPendX = 0, g_wpPendY = 0;
static unsigned int g_wpPendTicks = 0;
static const unsigned int kWpWaitMs = 180;   // ventana para que aparezca el 2do dedo
extern bool g_lastDownTouch;             // controls.cpp: el ultimo mouse-down fue un dedo
extern int  W3dActiveFingers();          // controls.cpp: dedos apoyados
#ifndef W3D_SYMBIAN
static unsigned int WP3DTicksMs() { return SDL_GetTicks(); }
#else
static unsigned int WP3DTicksMs() { return (unsigned int)User::NTickCount(); } // ~ms (mismo reloj que LayoutTickFPS)
#endif
void WP3DPaintCancelPending() { g_wpPend = false; }
int  WP3DPaintState() { return g_wp3dPainting ? 2 : (g_wpPend ? 1 : 0); }   // harness: 0 nada, 1 esperando, 2 pintando

// posicion VIVA del cursor en modo pintura (-1 = todavia no se movio). lastMouseX/Y
// solo se refrescan al CLICKEAR (SaveMousePos) o durante un drag (CheckWarpMouse):
// hoverando sin boton el circulo del pincel quedaba CONGELADO en el ultimo click.
// event_mouse_motion los actualiza en cada motion (solo en modo pintura).
static int g_wpCursorX = -1, g_wpCursorY = -1;

// modos de PINTURA del viewport 3D: el circulo del pincel sigue al cursor -> cada motion
// guarda la posicion y redibuja. Extensible: sumar aca vertex/texture paint cuando pinten
// con el mismo pincel. (El UV editor en UVModeWeights lleva su propio cursor lastMx/lastMy.)
static bool WP3DPaintMode() {
    // los DOS pinceles: el de pesos y el de vertex color. Comparten TODO (circulo, radio,
    // valor, falloff, marcas, mascara "solo lo seleccionado") y se diferencian solo en QUE
    // escriben, asi que todo el andamiaje del viewport es el mismo.
    return InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint;
}

struct WP3DCtx { Viewport3D* vp; Mesh* m; Matrix4 W; const GLfloat* pos; };

// proyector del 3D: posicion en pantalla del render-vert i (sobre la POSE visible, skinVertex
// si hay esqueleto). Solo vertices FRONT-FACING: el test barato normal-en-mundo vs direccion
// camera->vert (dot > 0 = mira para el otro lado -> no se pinta lo de atras). Con la malla
// posada la normal usada es la de BIND (no la rotada): aproximacion asumida, alcanza para
// descartar la cara de atras y evita rotar normales por hueso en cada pasada.
static bool WP3DProject(void* ctx, int i, float& sx, float& sy) {
    WP3DCtx* c = (WP3DCtx*)ctx;
    Vector3 local(c->pos[i*3], c->pos[i*3+1], c->pos[i*3+2]);
    Vector3 wpos = c->W * local;
    if (c->m->normals) {
        const Matrix4& W = c->W;
        float nx = c->m->normals[i*3] / 127.0f, ny = c->m->normals[i*3+1] / 127.0f, nz = c->m->normals[i*3+2] / 127.0f;
        Vector3 wn(W.m[0]*nx + W.m[4]*ny + W.m[8]*nz,     // normal local -> mundo (sin traslacion)
                   W.m[1]*nx + W.m[5]*ny + W.m[9]*nz,
                   W.m[2]*nx + W.m[6]*ny + W.m[10]*nz);
        Vector3 dir = wpos - c->vp->viewPos;              // camara -> vert
        if (wn.Dot(dir) > 0.0f) return false;             // back-facing: no pintable
    }
    return c->vp->ProjectPoint(wpos, sx, sy);
}

// una pasada del pincel en (mx,my) GLOBALES de pantalla (se pasan a locales del viewport)
static void WP3DPaint(Viewport3D* vp, int mx, int my) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m) return;
    WP3DCtx c; c.vp = vp; c.m = m;
    // EFECTIVA a proposito (no GetWorldMatrixBase): el pincel decide que vertice toca
    // PROYECTANDOLO A PANTALLA, asi que la matriz tiene que ser la MISMA con la que se dibujo
    // la malla o el circulo pinta vertices que no estan abajo del cursor. Y como es efectiva
    // hay que bindear ESTA vista: WP3DProject ya usa vp->viewPos y vp->ProjectPoint, y si
    // la matriz saliera de la vista que dibujo ultimo (el otro viewport) los pesos -- que se
    // serializan en el .w3d -- se escribirian en los vertices equivocados.
    vp->BindView();
    m->GetWorldMatrix(c.W);
    c.pos = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex; // pintar sobre la pose visible
    BrushState& br = BrushGet();
    BrushApply(m, m->activeGroup, (float)(mx - vp->x), (float)(my - vp->y),
                  br.radiusPx, br.force, (WPMode)br.mode, WP3DProject, &c,
                  NULL, &EffectiveBrushFalloff());
    g_redraw = true; // WeightPaintUpdate recalcula el color por frame -> feedback inmediato
}

// asegura una capa de color USABLE y devuelve su indice (-1 si la malla no da). Es el
// equivalente del "grupo automatico al primer trazo" del pincel de pesos: pintar tiene que
// funcionar sin obligar a preparar nada a mano. Una malla que nunca tuvo vertex color
// arranca en BLANCO (que es como se ve hoy: sin color = sin tinte).
static int WP3DLayerColor(Mesh* m) {
    if (!m || m->vertexSize <= 0) return -1;
    const int nC = m->CountCorners();
    if (nC <= 0) return -1;
    if (!m->vertexColor) {
        // COW: los buffers de render pueden estar COMPARTIDOS entre instancias (MeshData.h).
        // Escribir sin desinstanciar pintaria tambien a las otras copias.
        m->UninstantiateData(W3DMD_COL);
        m->vertexColor = new GLubyte[(size_t)m->vertexSize * 4];
        for (int i = 0; i < m->vertexSize * 4; i++) m->vertexColor[i] = 255;
    }
    if (m->colorLayers.empty()) m->PopulateLayers();
    if (m->colorLayers.empty()) return -1;
    if (m->activeColor < 0 || m->activeColor >= (int)m->colorLayers.size()) m->activeColor = 0;
    ColorLayer* cl = m->colorLayers[m->activeColor];
    if ((int)cl->color.size() != nC * 4) cl->color.assign((size_t)nC * 4, 255);
    return m->activeColor;
}

static bool g_wpChangeColor = false;  // el trazo de color cambio algo (decide si va al undo)

// una pasada del pincel de COLOR (misma cuenta de contexto que WP3DPaint)
static void WP3DPaintColor(Viewport3D* vp, int mx, int my) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m) return;
    const int layer = WP3DLayerColor(m);
    if (layer < 0) return;
    WP3DCtx c; c.vp = vp; c.m = m;
    vp->BindView();                 // idem pesos: la matriz EFECTIVA depende de ESTA vista
    m->GetWorldMatrix(c.W);
    c.pos = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex;
    BrushState& br = BrushGet();
    unsigned char rgba[4];
    for (int q = 0; q < 4; q++) {
        float v = br.color[q];
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        rgba[q] = (unsigned char)(v * 255.0f + 0.5f);
    }
    if (BrushApplyColor(m, layer, (float)(mx - vp->x), (float)(my - vp->y),
                           br.radiusPx, br.force, rgba, br.palIdx,
                           WP3DProject, &c, NULL, &EffectiveBrushFalloff()))
        g_wpChangeColor = true;
    g_redraw = true;
}

// UNA pasada, la que corresponda al modo. El drag y el commit no tienen que saber cual es.
// ---------------------------------------------------------------------------
//  TEXTURE PAINT desde el 3D (pintura por PROYECCION): para cada cara de la parte activa que mira a la camara
//  y cae bajo el pincel, se recorren los TEXELS de sus triangulos (en el espacio UV de la textura), cada texel
//  se lleva a su punto 3D por baricentricas y se PROYECTA a la pantalla con la perspectiva de la vista: si
//  cae dentro del circulo del pincel y no lo tapa otra cara (el mapa de oclusion de los modos de pintura),
//  se pinta con la caida del pincel segun su distancia en pantalla. Asi el trazo queda donde se ve, por
//  mas que la cara este en escorzo.
// ---------------------------------------------------------------------------
static TextureEditable* g_wpTexEd = NULL;
static bool  g_wpTexChange = false;
static float g_wpTexPrevX = -1e9f, g_wpTexPrevY = 0.0f;
static Texture* WP3DActiveTexture(Mesh* m, int& part) {
    part = UVShownPart(m);
    if (part < 0 || part >= (int)m->materialsGroup.size()) return NULL;
    Material* mt = m->materialsGroup[(size_t)part].material;
    return (mt && mt->texture && mt->texture->iID) ? mt->texture : NULL;
}
// PINTAR SOBRE EL OBJETO aunque todavia no tenga textura (el cubo por defecto): al primer trazo se arma lo que hace falta
//  - un material PROPIO para la parte (el por defecto es compartido: pintarlo pintaria todos los objetos)
//  - UV que no se pisen (sin UV, o todas las caras en el mismo lugar como el cubo por defecto): Smart UV de todas
//  - una textura EN MEMORIA del color del material (con "*": no se guarda hasta "Guardar" desde la barra o el editor UV)
static bool WP3DCreateTexturePaint(Mesh* m) {
    if (!m || m->materialsGroup.empty()) return false;
    int part = UVShownPart(m);
    if (part < 0 || part >= (int)m->materialsGroup.size()) part = 0;
    Material* mt = m->materialsGroup[(size_t)part].material;
    if (!mt || mt == MaterialDefault) { extern Material* NewMaterialInMeshPart(Mesh*, int); mt = NewMaterialInMeshPart(m, part); }
    if (!mt) return false;
    if (!m->uv || UVOverlappedFraction(m) > 0.05f) {
        std::string msg;
        if (W3dSmartUVProjectAll(m, 66.0f, 0.02f, msg)) Notify(T("Automatic UVs were generated to paint"), false);
    }
    if (!m->uv) return false;
    // 256x256 en todas las plataformas: alcanza para pintar y en el telefono/Android una mas grande era lentisima
    // (pedido del dueno). El que quiera mas definicion crea la textura a mano en el editor UV (Nueva Textura).
    const int side = 256;
    // la textura arranca con el color del material, y el material pasa a blanco: el color queda en la textura (sino
    // se multiplicaria dos veces y lo pintado se veria oscuro)
    const float col[4] = { mt->diffuse[0], mt->diffuse[1], mt->diffuse[2], 1.0f };
    std::vector<unsigned char> px; W3dGenerateTexture(TexGenBlank, side, side, col, false, px);
    Texture* tex = TexEditCreate(m->name + "_paint", px, side, side, false);
    if (!tex) return false;
    mt->texture = tex; mt->textureOn = true;
    mt->diffuse[0] = mt->diffuse[1] = mt->diffuse[2] = 1.0f;
    { extern void RebindMaterialMeshPart(); RebindMaterialMeshPart(); }
    Notify(std::string(T("New paint texture")) + ": " + TexEditLabel(tex), false);
    return true;
}
Texture* WP3DPaintedTexture() {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    int part = -1; return m ? WP3DActiveTexture(m, part) : NULL;
}
static bool WP3DTexStrokeStart(Mesh* m) {
    int part = -1;
    Texture* t = WP3DActiveTexture(m, part);
    if (!t && WP3DCreateTexturePaint(m)) t = WP3DActiveTexture(m, part);
    if (!t || !m->uv) { Notify(T("Texture Paint: the active part has no texture"), true); return false; }
    g_wpTexEd = TexEditGet(t);
    if (!g_wpTexEd) return false;
    UndoTextureStart(g_wpTexEd);
    g_wpTexChange = false; g_wpTexPrevX = -1e9f;
    return true;
}
static void WP3DDabTexture(Viewport3D* vp, Mesh* m, const Matrix4& W, const GLfloat* pos, int part,
                           float bx, float by, float radiusPx, const unsigned char* rgba, float force, const W3dFalloff& fo, int* rect) {
    TextureEditable* te = g_wpTexEd;
    const int tw = te->w, th = te->h;
    const float r2 = radiusPx * radiusPx;
    const bool occlusion = WPOcclusionActive(m);
    std::vector<Vector3> wp; std::vector<float> sx, sy, sw;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const MeshFace& F = m->faces3d[f];
        const int n = (int)F.idx.size();
        if (F.mat != part || n < 3) continue;
        wp.resize((size_t)n); sx.resize((size_t)n); sy.resize((size_t)n); sw.resize((size_t)n);
        bool ok = true; float mnx = 1e30f, mny = 1e30f, mxx = -1e30f, mxy = -1e30f;
        for (int c = 0; c < n && ok; c++) {
            const int rv = F.idx[(size_t)c];
            if (rv < 0 || rv >= m->vertexSize) { ok = false; break; }
            wp[(size_t)c] = W * Vector3(pos[rv*3], pos[rv*3+1], pos[rv*3+2]);
            if (!vp->ProjectPoint(wp[(size_t)c], sx[(size_t)c], sy[(size_t)c], &sw[(size_t)c])) { ok = false; break; }
            if (sx[(size_t)c] < mnx) mnx = sx[(size_t)c]; if (sx[(size_t)c] > mxx) mxx = sx[(size_t)c];
            if (sy[(size_t)c] < mny) mny = sy[(size_t)c]; if (sy[(size_t)c] > mxy) mxy = sy[(size_t)c];
        }
        if (!ok) continue;
        if (mxx < bx - radiusPx || mnx > bx + radiusPx || mxy < by - radiusPx || mny > by + radiusPx) continue;   // lejos del pincel
        Vector3 nrm(0, 0, 0);   // normal de mundo (Newell): las caras de espaldas no se pintan
        for (int c = 0; c < n; c++) { const Vector3& a = wp[(size_t)c]; const Vector3& b = wp[(size_t)((c + 1) % n)];
            nrm.x += (a.y - b.y) * (a.z + b.z); nrm.y += (a.z - b.z) * (a.x + b.x); nrm.z += (a.x - b.x) * (a.y + b.y); }
        if (nrm.Dot(wp[0] - vp->viewPos) > 0.0f) continue;
        for (int t = 1; t + 1 < n; t++) {   // abanico de triangulos en el espacio de la textura
            const int i0 = F.idx[0], i1 = F.idx[(size_t)t], i2 = F.idx[(size_t)t + 1];
            const float u0 = m->uv[i0*2] * tw, v0 = m->uv[i0*2+1] * th;
            const float u1 = m->uv[i1*2] * tw, v1 = m->uv[i1*2+1] * th;
            const float u2 = m->uv[i2*2] * tw, v2 = m->uv[i2*2+1] * th;
            const float e1x = u1 - u0, e1y = v1 - v0, e2x = u2 - u0, e2y = v2 - v0;
            const float det = e1x * e2y - e2x * e1y;
            if (fabsf(det) < 1e-9f) continue;
            const float inv = 1.0f / det;
            int x0 = (int)floorf(u0 < u1 ? (u0 < u2 ? u0 : u2) : (u1 < u2 ? u1 : u2)), x1 = (int)ceilf(u0 > u1 ? (u0 > u2 ? u0 : u2) : (u1 > u2 ? u1 : u2));
            int y0 = (int)floorf(v0 < v1 ? (v0 < v2 ? v0 : v2) : (v1 < v2 ? v1 : v2)), y1 = (int)ceilf(v0 > v1 ? (v0 > v2 ? v0 : v2) : (v1 > v2 ? v1 : v2));
            if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > tw - 1) x1 = tw - 1; if (y1 > th - 1) y1 = th - 1;
            const float S0x = sx[0], S0y = sy[0], S1x = sx[(size_t)t], S1y = sy[(size_t)t], S2x = sx[(size_t)t + 1], S2y = sy[(size_t)t + 1];
            const float w0 = sw[0], w1 = sw[(size_t)t], w2 = sw[(size_t)t + 1];
            // SOLO LO QUE CAE BAJO EL PINCEL. Antes se recorria el triangulo ENTERO en la textura (con un cubo y una
            // textura grande, cientos de miles de texels por toque, y un trazo rapido son decenas de toques): en
            // Android pintar era lentisimo. El circulo del pincel se lleva al espacio UV (con perspectiva: pesos l/w)
            // y el recorrido se recorta a su caja. El test exacto de abajo sigue igual.
            {
                const float ax = S1x - S0x, ay = S1y - S0y, cx = S2x - S0x, cy = S2y - S0y;
                const float dets = ax * cy - cx * ay;
                if (fabsf(dets) > 1e-6f) {
                    const float invs = 1.0f / dets;
                    float umin = 1e30f, umax = -1e30f, vmin = 1e30f, vmax = -1e30f;
                    bool box = true;
                    for (int k = 0; k <= 16 && box; k++) {
                        const float ang = (float)k * (6.2831853f / 16.0f);
                        const float px = (k == 16) ? bx : bx + cosf(ang) * radiusPx, py = (k == 16) ? by : by + sinf(ang) * radiusPx;
                        const float qx = px - S0x, qy = py - S0y;
                        const float l1 = (qx * cy - cx * qy) * invs, l2 = (ax * qy - qx * ay) * invs, l0 = 1.0f - l1 - l2;
                        const float q0 = l0 / w0, q1 = l1 / w1, q2 = l2 / w2, den = q0 + q1 + q2;
                        if (den <= 1e-12f) { box = false; break; }   // detras del horizonte: sin recorte (todo el triangulo)
                        const float g0 = q0 / den, g1 = q1 / den, g2 = q2 / den;
                        const float uu = g0 * u0 + g1 * u1 + g2 * u2, vv = g0 * v0 + g1 * v1 + g2 * v2;
                        if (uu < umin) umin = uu; if (uu > umax) umax = uu;
                        if (vv < vmin) vmin = vv; if (vv > vmax) vmax = vv;
                    }
                    if (box) {
                        const float side = (umax - umin > vmax - vmin) ? umax - umin : vmax - vmin;
                        const float mg = 2.0f + 0.05f * side;   // margen: el circulo se muestreo en 16 puntos
                        const int cx0 = (int)floorf(umin - mg), cx1 = (int)ceilf(umax + mg);
                        const int cy0 = (int)floorf(vmin - mg), cy1 = (int)ceilf(vmax + mg);
                        if (cx0 > x0) x0 = cx0; if (cx1 < x1) x1 = cx1;
                        if (cy0 > y0) y0 = cy0; if (cy1 < y1) y1 = cy1;
                        if (x0 > x1 || y0 > y1) continue;   // el pincel no toca este triangulo
                    }
                }
            }
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
                const float dx = x + 0.5f - u0, dy = y + 0.5f - v0;
                const float b1 = (dx * e2y - e2x * dy) * inv, b2 = (e1x * dy - dx * e1y) * inv, b0 = 1.0f - b1 - b2;
                if (b0 < -0.02f || b1 < -0.02f || b2 < -0.02f) continue;   // (un poquito afuera tambien: sin huecos en los bordes)
                // donde cae en pantalla: el promedio de las esquinas pesado por b*w es EXACTAMENTE la proyeccion del
                // punto (numerador y profundidad son lineales en el triangulo), sin proyectar texel por texel
                const float pw0 = b0 * w0, pw1 = b1 * w1, pw2 = b2 * w2, pw = pw0 + pw1 + pw2;
                if (pw <= 1e-9f) continue;
                const float ssx = (pw0 * S0x + pw1 * S1x + pw2 * S2x) / pw, ssy = (pw0 * S0y + pw1 * S1y + pw2 * S2y) / pw;
                const float ddx = ssx - bx, ddy = ssy - by, d2 = ddx * ddx + ddy * ddy;
                if (d2 > r2) continue;
                if (occlusion) { const int fv = WPOcclusionFaceIn(ssx, ssy); if (fv >= 0 && fv != (int)f) continue; }   // tapado
                const float a = force * fo.Eval(sqrtf(d2) / radiusPx);
                if (a <= 0.0f) continue;
                TexEditMix(te, x, y, rgba, a);
                TexEditRectJoin(rect, x, y);
            }
        }
    }
}
static void WP3DPaintTexture(Viewport3D* vp, int mx, int my) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || !g_wpTexEd || !m->uv) return;
    int part = -1; Texture* t = WP3DActiveTexture(m, part);
    if (!t || t != g_wpTexEd->tex) return;
    vp->BindView();
    Matrix4 W; m->GetWorldMatrix(W);
    const GLfloat* pos = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex;
    BrushState& br = BrushGet();
    unsigned char rgba[4];
    for (int q = 0; q < 4; q++) { float c = br.color[q]; if (c < 0) c = 0; if (c > 1) c = 1; rgba[q] = (unsigned char)(c * 255.0f + 0.5f); }
    const W3dFalloff& fo = EffectiveBrushFalloff();
    const float bx = (float)(mx - vp->x), by = (float)(my - vp->y);
    int rect[4] = { -1, -1, -1, -1 };
    if (g_wpTexPrevX > -1e8f) {   // toques interpolados desde el anterior (sin huecos en un trazo rapido)
        const float dx = bx - g_wpTexPrevX, dy = by - g_wpTexPrevY, dist = sqrtf(dx*dx + dy*dy);
        float step = br.radiusPx / 3.0f; if (step < 2.0f) step = 2.0f;
        int n = (int)ceilf(dist / step); if (n < 1) n = 1; if (n > 64) n = 64;
        for (int i = 1; i <= n; i++) { const float f = (float)i / (float)n; WP3DDabTexture(vp, m, W, pos, part, g_wpTexPrevX + dx * f, g_wpTexPrevY + dy * f, br.radiusPx, rgba, br.force, fo, rect); }
    } else WP3DDabTexture(vp, m, W, pos, part, bx, by, br.radiusPx, rgba, br.force, fo, rect);
    g_wpTexPrevX = bx; g_wpTexPrevY = by;
    if (rect[0] >= 0) { TexEditUpload(g_wpTexEd, rect[0], rect[1], rect[2], rect[3], true); g_wpTexChange = true; }
    g_redraw = true;
}
// (prueba del harness) un toque de pintura de textura en (mx,my) de pantalla, con su trazo completo
bool W3dTexPaint3DHarness(Viewport3D* vp, int mx, int my) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!vp || !m || !WP3DTexStrokeStart(m)) return false;
    WP3DPaintTexture(vp, mx, my);
    const bool change = g_wpTexChange;
    TexEditEndStroke(g_wpTexEd); UndoTextureConfirm(g_wpTexChange); g_wpTexEd = NULL; g_wpTexChange = false;
    return change;
}
// (prueba del harness) un TRAZO de n toques seguidos sin soltar, como el dedo arrastrando en circulo alrededor del
// centro (radio en px). Devuelve los ms que tardo la pintura (sin el deshacer del final), o -1.
double W3dTexPaint3DStrokeMs(Viewport3D* vp, int n, float radius) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!vp || !m || n < 1 || !WP3DTexStrokeStart(m)) return -1.0;
    const float cx = (float)(vp->x + vp->width / 2), cy = (float)(vp->y + vp->height / 2);
    const unsigned int t0 = WP3DTicksMs();   // portable: SDL en PC, NTickCount en Symbian
    for (int i = 0; i < n; i++) {
        const float a = (float)i * 0.15f;
        WP3DPaintTexture(vp, (int)(cx + cosf(a) * radius), (int)(cy + sinf(a) * radius));
    }
    const double ms = (double)(WP3DTicksMs() - t0);
    TexEditEndStroke(g_wpTexEd); UndoTextureConfirm(g_wpTexChange); g_wpTexEd = NULL; g_wpTexChange = false;
    return ms;
}
static void WP3DPass(Viewport3D* vp, int mx, int my) {
    if (InteractionMode == VertexPaint)       WP3DPaintColor(vp, mx, my);
    else if (InteractionMode == TexturePaint) WP3DPaintTexture(vp, mx, my);
    else                                      WP3DPaint(vp, mx, my);
}
// arranca el trazo (snapshot de undo). false = no hay nada que pintar en esta malla.
static bool WP3DStrokeStart(Mesh* m) {
    if (InteractionMode == TexturePaint) return WP3DTexStrokeStart(m);
    if (InteractionMode == VertexPaint) {
        const int layer = WP3DLayerColor(m);
        if (layer < 0) return false;
        g_wpChangeColor = false;
        BrushStrokeReset();          // el tope por trazo arranca limpio (ver WeightPaint.h)
        UndoColorStart(m, layer);
        return true;
    }
    return WeightPaintStrokeStart(m) >= 0;
}
// cierra el trazo: un paso de undo por trazo, como en pesos (se descarta si no cambio nada)
static bool WP3DStrokeStart(Mesh* m);
static void WP3DPass(Viewport3D* vp, int mx, int my);
// resuelve el trazo pendiente: 2 dedos -> se descarta; se movio / paso la espera / forzar (soltar) -> arranca
static void WP3DPendingResolve(Viewport3D* vp, int mx, int my, bool force) {
    if (!g_wpPend) return;
    if (W3dActiveFingers() >= 2) { g_wpPend = false; return; }
    const int threshold = 6 * GlobalScale;
    const bool moved    = (mx - g_wpPendX > threshold) || (g_wpPendX - mx > threshold) || (my - g_wpPendY > threshold) || (g_wpPendY - my > threshold);
    const bool step     = (WP3DTicksMs() - g_wpPendTicks) >= kWpWaitMs;
    if (!force && !moved && !step) return;
    g_wpPend = false;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || !WP3DStrokeStart(m)) return;
    g_wp3dPainting = true;
    WP3DPass(vp, g_wpPendX, g_wpPendY);
    if (moved) WP3DPass(vp, mx, my);
}
static void WP3DStrokeEnd() {
    if (InteractionMode == TexturePaint) { TexEditEndStroke(g_wpTexEd); UndoTextureConfirm(g_wpTexChange); g_wpTexEd = NULL; g_wpTexChange = false; return; }
    if (InteractionMode == VertexPaint) { UndoColorConfirm(g_wpChangeColor); g_wpChangeColor = false; }
    else                                  WeightPaintStrokeEnd();
}

// CRUZ GUIA / CAJA del box select. Mismo ortho 2D local que el circulo del pincel: es un
// overlay de pantalla, no geometria de la escena.
static void BoxSelectRender3D(Viewport3D* vp) {
    if (!BoxSelectActive()) return;
    if (Viewport3DActive != vp) return;   // la caja es de UN viewport (el activo)
    namespace gfx = w3dEngine;
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, vp->width, vp->height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting);
    BoxSelectDraw(vp->x, vp->y, vp->width, vp->height);
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray); // restaurar para la UI
    gfx::Invalidate();
}

// circulo del pincel siguiendo al mouse, SOLO en modo pintura y con el cursor sobre el
// CONTENIDO del viewport (no sobre la barra/toolbar ni con un menu/popup abierto).
// ============================================================================
//  OCLUSION para pintar: al empezar el frame (modos de pintura, sin X-Ray) se dibuja la malla
//  pintada con un COLOR POR CARA (ID) en el area del viewport, se lee el resultado y se le deja a
//  WeightPaint (WPOcclusionSet). Las marcas y los pinceles preguntan "que cara hay en este pixel":
//  un corner de una cara que mira a camara pero esta tapada por otra ya no aparece ni se pinta.
//  El render normal pisa este dibujo enseguida (viene el Clear de la escena). Costo: un draw de
//  la malla + un ReadPixels del viewport por frame, solo mientras se pinta.
// ============================================================================
static std::vector<unsigned char> g_wpOclBuf;
static void WP3DOcclusionPass(Viewport3D* vp) {
    Mesh* m = (WP3DPaintMode() && ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || g_xray || m->vertexSize <= 0 || vp->width <= 0 || vp->height <= 0) { WPOcclusionClear(); return; }
#ifdef W3D_DREAMCAST
    // la PVR arma el cuadro recien al cerrarlo: este dibujo no se puede leer y el Clear no lo borra, asi que quedaba
    // ENCIMA de la escena (el cubo pintado se veia negro, solo las lineas). Sin mapa vale el test de back-facing, como
    // con X-Ray
    WPOcclusionClear();
    return;
#endif
    namespace gfx = w3dEngine;
    const int glY = W3dScreenHeight - vp->y - vp->height;
    // la PROYECCION ya la cargo el frame (W3dScene3DProjection); la VISTA la aplica la escena mas
    // adelante, asi que aca se carga a mano y al final se deja el modelview en identidad como estaba
    vp->BindView();
    gfx::MatrixMode(gfx::ModelView);
    gfx::LoadMatrix(vp->ViewCam().ViewMatrix().m);
    gfx::Viewport(vp->x, glY, vp->width, vp->height);
    gfx::Scissor(vp->x, glY, vp->width, vp->height);
    gfx::Enable(gfx::ScissorTest);
    gfx::Disable(gfx::Dither); gfx::Disable(gfx::Multisample);   // el ID tiene que llegar EXACTO al pixel
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Blend);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::TexCoordArray);
    gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::ColorArray);
    gfx::Enable(gfx::DepthTest); gfx::DepthMask(true);
    gfx::ReadPixelsBegin(vp->x, glY, vp->width, vp->height);   // (la 3DS lo dibuja aparte, donde no se ve)
    gfx::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
    // triangulos de cada cara (la misma triangulacion que el render), en MUNDO, con el ID de la cara
    const GLfloat* pos = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex;
    Matrix4 W; m->GetWorldMatrix(W);
    static std::vector<GLfloat> tp; static std::vector<GLubyte> tc; static std::vector<MeshIndex> tri;
    tp.clear(); tc.clear();
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        tri.clear(); W3dTriangulateFace(pos, m->faces3d[f].idx, tri);
        const int id = (int)f + 1;
        const GLubyte r = (GLubyte)((id & 0x1F) << 3), g = (GLubyte)(((id >> 5) & 0x3F) << 2), b = (GLubyte)(((id >> 11) & 0x1F) << 3);
        for (size_t t = 0; t < tri.size(); t++) {
            const int v = (int)tri[t]; if (v < 0 || v >= m->vertexSize) continue;
            const Vector3 wp = W * Vector3(pos[v*3], pos[v*3+1], pos[v*3+2]);
            tp.push_back(wp.x); tp.push_back(wp.y); tp.push_back(wp.z);
            tc.push_back(r); tc.push_back(g); tc.push_back(b); tc.push_back(255);
        }
    }
    if (!tp.empty()) {
        gfx::VertexPointer3f(0, &tp[0]);
        gfx::ColorPointer4ub(&tc[0]);
        gfx::DrawTrianglesArray((int)(tp.size() / 3));
    }
    g_wpOclBuf.resize((size_t)vp->width * (size_t)vp->height * 4);
    gfx::ReadPixelsRGBA(vp->x, glY, vp->width, vp->height, &g_wpOclBuf[0]);
    WPOcclusionSet(&g_wpOclBuf[0], vp->x, glY, vp->width, vp->height, vp->x, vp->y, W3dScreenHeight, m);
    gfx::Enable(gfx::Dither); gfx::Enable(gfx::Multisample);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();   // como lo dejo W3dScene3DProjection
    gfx::Invalidate();
}

// harness: cuantos face corners de la malla quedan visibles (marcas/pincel) en el viewport 3D activo
int WP3DVisibleCorners(Mesh* m) {
    if (!m || !viewPortActive || viewPortActive->ViewportKind() != 1) return -1;
    Viewport3D* vp = (Viewport3D*)viewPortActive;
    WP3DCtx c; c.vp = vp; c.m = m;
    vp->BindView();
    m->GetWorldMatrix(c.W);
    c.pos = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex;
    std::vector<float> cx, cy; std::vector<char> ok;
    WPCornersInScreen(m, WP3DProject, &c, cx, cy, ok);
    int n = 0; for (size_t i = 0; i < ok.size(); i++) if (ok[i]) n++;
    if (getenv("W3D_OCLDBG")) {
        size_t notBlack = 0; int idMax = -1;
        for (size_t i = 0; i + 3 < g_wpOclBuf.size(); i += 4) if (g_wpOclBuf[i] | g_wpOclBuf[i+1] | g_wpOclBuf[i+2]) { notBlack++;
            const int id = (g_wpOclBuf[i] >> 3) | ((g_wpOclBuf[i+1] >> 2) << 5) | ((g_wpOclBuf[i+2] >> 3) << 11); if (id > idMax) idMax = id; }
        printf("      [ocldbg] vp=(%d,%d %dx%d) screenHeight=%d buf=%d px no-black=%d idMax=%d\n", vp->x, vp->y, vp->width, vp->height, W3dScreenHeight, (int)(g_wpOclBuf.size()/4), (int)notBlack, idMax);
        int L = 0;
        for (size_t f = 0; f < m->faces3d.size() && L < 8; f++) for (size_t k = 0; k < m->faces3d[f].idx.size() && L < 8; k++, L++) {
            float sx = 0, sy = 0; const bool p = WP3DProject(&c, m->faces3d[f].idx[k], sx, sy);
            printf("      [ocldbg] corner %d cara %d: vert->(%.0f,%.0f) proj=%d cuadradito=(%.0f,%.0f) idInPixel=%d ok=%d\n", L, (int)f, sx, sy, p ? 1 : 0, cx[(size_t)L], cy[(size_t)L], WPOcclusionFaceIn(cx[(size_t)L], cy[(size_t)L]), ok[(size_t)L]);
        }
    }
    return n;
}

void WP3DPaintTick(Viewport3D* vp) {   // por frame (lo llama el render): la espera vence aunque el dedo no se mueva
    if (!g_wpPend) return;
    if (!leftMouseDown) { g_wpPend = false; return; }
    WP3DPendingResolve(vp, g_wpCursorX, g_wpCursorY, false);
    g_redraw = true;   // seguir redibujando hasta resolver
}
// VISTA PREVIA DEL RADIO: al cambiar el radio del pincel (arrastrando la barra, con las flechas o tipeando el valor)
// se ve en el CENTRO del viewport el area que va a tener, en su tamano real; se desvanece un rato despues. Detecta el
// cambio comparando contra el ultimo radio visto: cualquier camino que lo cambie la dispara.
static float  g_wpSeenRadius = -1.0f;
static double g_wpChangeRadiusMs = -1.0e9;
static int    g_wpFrameRadius = 0;   // sin reloj (N95: W3dNowMs = 0) se cuenta en cuadros
static float WP3DPreviewAlphaRadius() {
    const float r = BrushGet().radiusPx;
    const double now = W3dNowMs();
    if (g_wpSeenRadius < 0.0f) g_wpSeenRadius = r;   // la primera vez no es un cambio
    const bool change = (r != g_wpSeenRadius) || BrushBarDraggingRadius();
    if (change) { g_wpSeenRadius = r; g_wpChangeRadiusMs = now; g_wpFrameRadius = 40; }
    float alpha = 0.0f;
    if (now > 0.0) {
        const double t = now - g_wpChangeRadiusMs;
        alpha = (t < 900.0) ? 1.0f : (t < 1300.0) ? (float)(1.0 - (t - 900.0) / 400.0) : 0.0f;
    } else if (g_wpFrameRadius > 0) {
        alpha = (g_wpFrameRadius > 10) ? 1.0f : (float)g_wpFrameRadius / 10.0f;
        g_wpFrameRadius--;
    }
    if (alpha > 0.0f) g_redraw = true;   // el desvanecido necesita los cuadros siguientes
    return alpha;
}
static void WP3DDrawPreviewRadius(Viewport3D* vp, float alpha) {
    namespace gfx = w3dEngine;
    const float r = BrushGet().radiusPx;
    if (r <= 0.0f || alpha <= 0.0f) return;
    const float cx = vp->width * 0.5f, cy = vp->height * 0.5f;
    const int N = 64;
    static float tri[N * 6], hoop[N * 4];
    for (int i = 0; i < N; i++) {
        const float a0 = 6.2831853f * (float)i / (float)N, a1 = 6.2831853f * (float)(i + 1) / (float)N;
        const float x0 = cx + cosf(a0) * r, y0 = cy + sinf(a0) * r, x1 = cx + cosf(a1) * r, y1 = cy + sinf(a1) * r;
        tri[i*6]  = cx; tri[i*6+1] = cy; tri[i*6+2] = x0; tri[i*6+3] = y0; tri[i*6+4] = x1; tri[i*6+5] = y1;
        hoop[i*4] = x0; hoop[i*4+1] = y0; hoop[i*4+2] = x1; hoop[i*4+3] = y1;
    }
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray); gfx::EnableArray(gfx::VertexArray);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    // el AREA (relleno tenue) y su borde: halo oscuro + linea blanca, como el circulo del cursor
    const float* acc = ColorList[static_cast<int>(ColorID::accent)];
    gfx::Color4f(acc[0], acc[1], acc[2], 0.18f * alpha);
    gfx::VertexPointer2f(0, tri);
    gfx::DrawTrianglesArray(N * 3);
    gfx::VertexPointer2f(0, hoop);
    gfx::LineWidth(3.0f * GlobalScale); gfx::Color4f(0.0f, 0.0f, 0.0f, 0.8f * alpha); gfx::DrawLines(N * 2);
    gfx::LineWidth(1.5f * GlobalScale); gfx::Color4f(1.0f, 1.0f, 1.0f, alpha); gfx::DrawLines(N * 2);
    gfx::LineWidth(1.0f);
    gfx::Disable(gfx::Blend);
}

static void WP3DRenderBrush(Viewport3D* vp) {
    WP3DPaintTick(vp);
    if (!WP3DPaintMode()) return;
    const float alphaPreview = WP3DPreviewAlphaRadius();
    namespace gfx = w3dEngine;
    // DOS COSAS DISTINTAS, con reglas distintas:
    //  - el CIRCULO es el cursor del pincel: sigue al mouse y solo se ve con el mouse sobre el
    //    CONTENIDO de este viewport (no sobre las barras) y sin un menu/popup abierto.
    //  - las MARCAS son un DISPLAY: muestran donde se puede pintar. No dependen del mouse (si
    //    dependieran, desaparecerian al ir a la barra a cambiar el radio, que es justo cuando
    //    se las quiere ver) ni de que haya un popup abierto (el de falloff, sin ir mas lejos).
    int mx = (g_wpCursorX >= 0) ? g_wpCursorX : (int)lastMouseX;   // cursor VIVO (o el ultimo click)
    int my = (g_wpCursorY >= 0) ? g_wpCursorY : (int)lastMouseY;
    const bool cursorInside =
        !PopUpActive && !LayoutMenuOpen() &&
        mx >= vp->x && mx < vp->x + vp->width && my >= vp->y && my < vp->y + vp->height &&
        !vp->OnBar(mx, my) && !vp->OnToolbar(mx, my) && !vp->OnBrushBar(mx, my);
    if (!cursorInside && BrushGet().marks == MarksOff && !WeightPaintOnlySel() && alphaPreview <= 0.0f) return; // nada que dibujar
    // el contexto del proyector se arma ANTES del ortho 2D: GetWorldMatrix es la matriz
    // EFECTIVA (billboards/constraints miran a la camara), asi que necesita ESTA vista
    // bindeada -- y bindearla despues pisaria el ortho con el que se dibuja.
    Mesh* mm = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    WP3DCtx c; bool hasCtx = false;
    const bool hasMarks = (mm && BrushGet().marks != MarksOff && mm->vertexSize > 0);
    if (mm && mm->vertexSize > 0 && (hasMarks || WeightPaintOnlySel())) {
        c.vp = vp; c.m = mm;
        vp->BindView();
        mm->GetWorldMatrix(c.W);
        c.pos = (mm->skinArmature && mm->skinVertex) ? mm->skinVertex : mm->vertex; // la pose visible
        hasCtx = true;
    }
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, vp->width, vp->height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Blend);
    // VELO sobre lo no pintable (mascara "solo lo seleccionado"): antes de las marcas, para que
    // los cuadraditos queden ARRIBA del velo y se sigan viendo.
    if (hasCtx && WeightPaintOnlySel())
        BrushDrawFacesLocked(mm, WP3DProject, &c);
    if (hasCtx && hasMarks) {
        // relleno = el color del PESO que ya tiene el punto (la misma rampa que tinta la malla):
        // el cuadradito muestra lo que vale, no solo donde esta. Si el color todavia no se armo
        // para esta malla, van en gris (NULL) y no se dibuja nada de mas.
        const unsigned char* colorRV =
            (InteractionMode == VertexPaint)
                ? mm->vertexColor                                   // vertex color: el color real del punto
                : (((int)mm->weightPaintColor.size() >= mm->vertexSize * 4)
                       ? &mm->weightPaintColor[0] : NULL);          // pesos: la rampa blue->red
        // el MODO sale del pincel, pero en VERTEX COLOR manda la capa: una capa por corner
        // muestra un cuadrado por corner (corridos hacia el centro de su cara) y una capa
        // Per-Vertex los muestra sobre el vertice. Son ubicaciones distintas a proposito:
        // es lo unico que distingue a simple vista en que unidad estas pintando.
        int markMode = BrushGet().marks;
        if (InteractionMode == VertexPaint && markMode != MarksOff) {
            const ColorLayer* cl = (mm->activeColor >= 0 && mm->activeColor < (int)mm->colorLayers.size())
                                 ? mm->colorLayers[mm->activeColor] : NULL;
            markMode = ((cl && cl->perVertex) || BrushGet().perVertex) ? MarksVertex : MarksCorner;
        }
        BrushDrawMarks(mm, markMode, 7.0f * (float)GlobalScale, colorRV, WP3DProject, &c);
    }
    if (cursorInside) BrushDrawCircle((float)(mx - vp->x), (float)(my - vp->y), BrushGet().radiusPx);
    if (alphaPreview > 0.0f) WP3DDrawPreviewRadius(vp, alphaPreview);   // el radio que se esta ajustando, en el centro
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray); // restaurar para la UI que sigue
    gfx::Invalidate();
}

// EL POSTPROCESADO EN EL ARBOL: cuando el pase de escena de un viewport llega a un objeto Postproceso, sus efectos se
// aplican a lo dibujado HASTA AHI (RunPostProcess: mirando por la camara, adentro de su marco) y lo que sigue se dibuja
// limpio y adelante (la profundidad se borra, como el Limpiar Z). Con el trazado de rayos la imagen trazada se pega
// DESPUES de la escena: ahi se aplican al final, en el mismo orden
static Viewport3D* gVpPost = NULL;   // el viewport que esta dibujando su escena (NULL: nadie, ej. el pick)
static int gVpPostGlY = 0;
static bool gInPost = false;         // (los pases de datos del postprocesado vuelven a dibujar la escena: no re-entrar)
static std::vector<PostProcess*> gDeferedPost;
static bool gCpuProfReady = false, gCpuVecReady = false;   // (los datos de antemano por CPU: PostPreviousData)
// LA UI EN SU LUGAR DEL ARBOL (UI::inTree): cuando el recorrido llega a la UI que se muestra, se dibuja AHI como el overlay
// de siempre (DrawHUDGame); lo que sigue en el outliner se dibuja encima (un Limpiar Z y un cubo: adelante de la UI)
static Viewport3D* gVpUI = NULL;
static int gVpUIglY = 0;
static bool gHudInTree = false;   // (este cuadro ya se dibujo en el arbol: RenderUI no lo repite)
static void UIInTree(UI* u) {
    Viewport3D* vp = gVpUI;
    if (!vp || gInPost || u != UI2D_EditorUI()) return;
    namespace gfx = w3dEngine;
    float P[16], MV[16];
    gfx::GetMatrix(gfx::Projection, P);
    gfx::GetMatrix(gfx::ModelView, MV);
    // el 2D del viewport (lo mismo que deja Render para su UI)
    gfx::Viewport(vp->x, gVpUIglY, vp->width, vp->height);
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, (float)vp->width, (float)vp->height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    vp->State2D();
    vp->DrawHUDGame();
    gHudInTree = true;
    // la escena sigue como estaba
    gfx::Viewport(vp->x, gVpUIglY, vp->width, vp->height);
    gfx::Enable(gfx::ScissorTest); gfx::Scissor(vp->x, gVpUIglY, vp->width, vp->height);
    gfx::MatrixMode(gfx::Projection); gfx::LoadMatrix(P);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadMatrix(MV);
    W3dScene3DMode((int)(RenderType::Enum)vp->view);
    gfx::Enable(gfx::DepthTest); gfx::DepthMask(true); gfx::DepthFunc(gfx::DepthLess);
    gfx::Invalidate();
}
static void PostInTree(PostProcess* p) {
    Viewport3D* vp = gVpPost;
    if (!vp || gInPost || !p->opt.on) return;
    W3dPostUse(&p->opt);
    const bool does = W3dPostHas();
    W3dPostUse(NULL);
    if (!does) return;   // (prendido sin efectos que hagan algo: ni lo toca)
    if (g_rt.on) { gDeferedPost.push_back(p); return; }
    namespace gfx = w3dEngine;
    float P[16], MV[16];
    gfx::GetMatrix(gfx::Projection, P);
    gfx::GetMatrix(gfx::ModelView, MV);
    gInPost = true;
    W3dPostUse(&p->opt);
    vp->RunPostProcess(gVpPostGlY);
    W3dPostUse(NULL);
    gInPost = false;
    // la escena sigue como estaba: la lente, la matriz donde iba el recorrido, el modo y las luces de la vista
    gfx::Viewport(vp->x, gVpPostGlY, vp->width, vp->height);
    gfx::MatrixMode(gfx::Projection); gfx::LoadMatrix(P);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadMatrix(MV);
    W3dScene3DMode((int)(RenderType::Enum)vp->view);
    gfx::Enable(gfx::DepthTest); gfx::DepthMask(true);
    // (la profundidad y el movimiento ya estan de antemano -PostPreviousData-: la escena no se redibujo y la
    //  profundidad sigue siendo la de lo dibujado. Lo que sigue en el arbol se dibuja contra ella, normal)
    gfx::Invalidate();
}

// LA PROYECCION de este viewport (la que usa Render y la que tienen que usar los PICKS para ver exactamente lo mismo):
// mirando por la camara, SU lente con su marco y la inspeccion; en la orbita, la del visor. Usa el marco que calculo el
// ultimo Render (camFrameOn / camFrameNX / camFrameNY). Deja el MatrixMode en ModelView con identidad
void Viewport3D::LoadProjectionView() {
    const bool camFrame = (ViewFromCameraActive && CameraActive);
    const float camNX = camFrameOn ? camFrameNX : 1.0f, camNY = camFrameOn ? camFrameNY : 1.0f;
    // al MIRAR POR LA CAMARA se usa SU lente (fov + ortografica PROPIOS de la camara, editables/animables);
    // en la orbita del visor se usa la del viewport (fovDeg global / orthographic del visor).
    float projFov = camFrame ? CameraActive->fov : fovDeg;
    bool  projOrtho = camFrame ? CameraActive->orthographic : orthographic;
    // distancia MINIMA/MAXIMA de dibujado: la de la camara al mirar por ella (animables), la del visor en la orbita
    float projNear = camFrame ? CameraActive->nearClip : NearView();
    float projFar  = camFrame ? CameraActive->farClip  : farClip;

    // publicar la LENTE de esta vista al core (acompania al W3dViewBind de UpdateViewOrbit):
    // el objeto Culling arma su frustum en CPU con estos numeros. Mirando por la camara se
    // publica el frustum del RENDER (fov de la camara + aspecto del render): es el del juego;
    // el aire del passepartout queda afuera a proposito (se cull-ea lo que el juego cull-earia).
    // LA PROYECCION la arma EscenaRender (compartida con el runtime del juego): asi el
    // encuadre, el frustum y la lente publicada al Core son bit a bit los mismos en el
    // Play del editor y en el .deb/APK compilado. Lo unico propio del editor que entra
    // aca es la INSPECCION (zoom/pan de la vista de camara), que en el juego vale 1/0/0.
    // En ORTOGRAFICA la distancia camera->object NO cambia el tamaño aparente: el "zoom" es la escala del
    // volumen visible (glOrtho), no la distancia. Atamos el half-height a orbitDistance*tan(fov/2) para que
    // zoomear agrande/achique igual que en perspectiva (y al alternar perspectiva/orto el objeto quede del
    // mismo tamaño). El far se corre con la distancia asi el objeto no se corta por el far al alejarse.
    W3dView3D vst;
    vst.cam            = ViewCam();
    vst.fov            = projFov;
    vst.nearC          = projNear;
    vst.farC           = projOrtho ? (orbitDistance + projFar) : projFar;
    vst.ortho          = projOrtho;
    vst.orthoSize      = orbitDistance * tanf(projFov * 0.5f * 3.14159265f / 180.0f);
    if (vst.orthoSize < 0.001f) vst.orthoSize = 0.001f;
    vst.viewAspect  = (aspect > 1e-4f) ? aspect : 1.0f;
    // en COVER (juego puro 2) la imagen es la del VIEWPORT: sin encuadre declarado
    vst.imageAspect   = camFrame ? FrameAspect(vst.viewAspect) : vst.viewAspect;
    vst.frameNX       = camNX;
    vst.frameNY       = camNY;
    vst.imageHeightPx = (float)height * ((camNY > 1e-4f) ? camNY : 1.0f) * ((camFrame && camViewZoom > 0.0f) ? camViewZoom : 1.0f);
    if (camFrame) { vst.zoom = camViewZoom; vst.panX = camViewPanX; vst.panY = camViewPanY; }
    W3dScene3DProjection(vst);   // (deja el MatrixMode en ModelView con identidad)
}

void Viewport3D::Render() {
    // la PANTALLA DIVIDIDA (setVistas(2) de un juego): las dos mitades y el HUD, ver RenderSplit
    if (!gSessionView && W3dViewsCount() > 1 && RenderSplit()) return;
    double _tVp0 = W3dNowMs(); // profiler: tiempo total de este viewport 3D
    ReloadLights();

    // Configuración de la matriz de proyección
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    // PASSEPARTOUT: mirando DESDE la camara, el viewport muestra EXACTAMENTE lo que sale en el render (aspecto
    // g_renderAspect) ENCUADRADO adentro (letterbox/pillarbox). Se ajusta el frustum para que el render ocupe un
    // sub-rectangulo centrado del viewport; afuera se oscurece. Asi sabes que se va a renderizar. Responsive.
    camFrameOn = false;
    bool camFrame = (ViewFromCameraActive && CameraActive);
    float camNX = 1.0f, camNY = 1.0f; // medias-extensiones NDC del marco (1 = todo el viewport)
    // ENCUADRE DECLARADO + LETTERBOX DURO: si la camara declara un aspecto (propio o
    // heredado del riel, ver Camera::aspecto), ESE es el aspecto del marco -- no el del
    // render -- y con el juego corriendo el marco ocupa todo el viewport (sin aire) y
    // lo de afuera se tapa con BARRAS NEGRAS opacas: el jugador ve EXACTAMENTE el
    // frustum declarado, como en el dispositivo. Sin declarar, todo como siempre.
    camFrameLetterbox = false;
    if (camFrame) {
        float va = (aspect > 1e-4f) ? aspect : 1.0f; // aspecto del viewport
        float ra = W3dGameAspect(); // declarado por la camara, o el del render (ver Camera.h)
        extern bool AnimIsGame; extern bool PlayAnimation;
        // MODO JUEGO PURO "COVER" (VERDE+0 dos veces, g_pureGame == 2): el marco
        // ES el viewport (ra = va -> NX = NY = 1): sin bandas, el frustum llena la
        // pantalla (vertical u horizontal) y el culling publicado es EXACTAMENTE lo
        // que se ve -- nada se esfuma en los bordes. La UI del juego se encaja al
        // marco, asi que se adapta sola al viewport y los contadores quedan visibles.
        // (una mitad de la pantalla dividida: igual, la mitad entera). Ver FrameAspect
        ra = FrameAspect(va);
        // en el modo juego puro (sin la UI) la inspeccion no va: el juego tal cual, sin zoom ni paneo
        if (g_pureGame) { camViewZoom = 1.0f; camViewPanX = 0.0f; camViewPanY = 0.0f; }
        camFrameLetterbox = (CameraActive->DeclaredAspect() > 0.01f) && g_pureGame != 2 && !gSessionView &&
                            (g_pureGame || (AnimIsGame && (PlayAnimation || GameSimActive())));
        // jugando encuadrado o sin la UI: sin aire (el marco toca los bordes)
        const float margin = (camFrameLetterbox || g_pureGame || gSessionView) ? 1.0f : 0.92f;
        // LA cuenta del encuadre vive en main/render/SceneRender.cpp: el runtime del
        // juego compilado llama a la MISMA, asi el .deb/APK encuadra igual que el Play.
        W3dFramingFrame(va, ra, margin, &camNX, &camNY);
        camFrameOn = true; camFrameNX = camNX; camFrameNY = camNY;
    }

    LoadProjectionView();   // (la lente de la vista: ver abajo; deja el MatrixMode en ModelView con identidad)
    // los DATOS de los Postproceso del arbol (profundidad / movimiento), ANTES de la escena: el Clear de abajo los tapa
    if (!onlyScene && view == RenderType::Rendered && camFrameOn) PostPreviousData(W3dScreenHeight - y - height);

    // Limpiar pantalla
    const int glY = W3dScreenHeight - y - height; // arbol arriba-izq -> GL
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, width, height);
    if (!onlyScene) WP3DOcclusionPass(this);   // modos de pintura: mapa "que cara se ve en cada pixel" (lo pisa el Clear de abajo)

    // ZBuffer: fog de profundidad con rango FIJO en unidades de mundo (NO depende de la escena ni del
    // zoom). Es SOLO la distancia camera->object: en la camara = blanco, a fogFar o mas lejos = negro.
    // Al mover/zoomear la camara cambian las distancias -> TODO se aclara al acercarse y se oscurece al
    // alejarse por igual (incluido el objeto mas cercano). Para afinar el contraste, cambiar fogFar.
    const float fogNear = nearClip; // ~camara = blanco
    const float fogFar  = 40.0f;    // a 40 de la camara (o mas lejos) = negro

    if (view == RenderType::ZBuffer) {
        w3dEngine::Enable(w3dEngine::Fog);
        w3dEngine::FogMode(0);
        w3dEngine::FogStart(fogNear); // rango FIJO: solo la distancia a la camara define el tono
        w3dEngine::FogEnd(fogFar);
        GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        w3dEngine::FogColor(fogColor);
    } else {
        w3dEngine::Disable(w3dEngine::Fog);
        w3dEngine::ClearColor(ColorList[static_cast<int>(ColorID::background)][0], ColorList[static_cast<int>(ColorID::background)][1],
                     ColorList[static_cast<int>(ColorID::background)][2], ColorList[static_cast<int>(ColorID::background)][3]);
    }

    // QUIEN LIMPIA. Un proyecto con CONSTRUCTOR limpia el solo, con sus pasos "Clear color" y "Clear" (sin un "Clear"
    // no se limpia nada, tambien aca): el viewport no limpia, deja el SCISSOR puesto en su rectangulo mientras se
    // dibuja la escena (un "Clear" del proyecto no borra el resto de la ventana) y dibuja la grilla DESPUES de la
    // escena (un "Clear" la borraria). Los modos de ANALISIS (Z-Buffer, Alfa, Normales: el fondo dice algo) limpian el
    // color con su fondo fijo y ahi el proyecto solo limpia profundidad y estencil. Un proyecto viejo (sin
    // constructor) lo limpia el viewport, como siempre. En el modo juego puro la escena es la pantalla entera: sin
    // scissor (el "Clear" del proyecto la limpia toda, como en el juego compilado)
    const bool analysisMode = (view == RenderType::ZBuffer || view == RenderType::Alpha || view == RenderType::NormalView ||
                               view == RenderType::Vectors);
    const bool projectClean = W3dHasConstructor() && W3dScene3DActive();   // (lua dibujar3D(false): no corre)
    const bool cleanProject = projectClean && !analysisMode;
    g_w3dClearColorFixed = projectClean && analysisMode;
    // el "Clear color" del proyecto en AUTO: en la vista Render (y jugando) el fondo del render; en las otras, el fondo de
    // este visor (con el que lo limpiaria un proyecto sin constructor). Se suelta despues del pase de la escena
    if (cleanProject && view != RenderType::Rendered && !g_pureGame)
        W3dBackgroundAutoSet(bgSolid[3] >= 0.0f ? bgSolid : ColorList[static_cast<int>(ColorID::background)]);
    else W3dBackgroundAutoSet(NULL);
    // (una mitad de la pantalla dividida recorta siempre: el "Clear" del proyecto no borra la otra)
    const bool cropScene = projectClean && (!g_pureGame || gSessionView);
    const bool floorAfter = cleanProject;
    if (cleanProject) {
        // (nada: ni color ni profundidad)
    } else {
        if (view == RenderType::ZBuffer || view == RenderType::Alpha) {
            w3dEngine::ClearColor(0.0f, 0.0f, 0.0f, 1.0f); // ZBuffer y Alpha: fondo NEGRO
        } else if (view == RenderType::NormalView) {
            // Normal: fondo = AZUL de normal-map. Una normal que mira de frente a la camara es
            // (0,0,1) en view-space, que el Core codifica como (0.5,0.5,1.0). Asi el "vacio" se lee
            // como una superficie de frente, igual que la base de un normal map.
            w3dEngine::ClearColor(0.5f, 0.5f, 1.0f, 1.0f);
        } else if (view == RenderType::Vectors) {
            float bg[4]; W3dVecBackgroundColor(bg); w3dEngine::ClearColor(bg[0], bg[1], bg[2], bg[3]);   // quieto, sin geometria
        } else if (view == RenderType::Rendered) {
            w3dEngine::ClearColor(g_renderBg[0], g_renderBg[1], g_renderBg[2], g_renderBg[3]); // fondo GLOBAL del render
        } else {
            // solid / wireframe / material / normal: fondo POR-VIEWPORT (bgSolid). Sentinel alpha<0 = tema.
            if (bgSolid[3] >= 0.0f)
                w3dEngine::ClearColor(bgSolid[0], bgSolid[1], bgSolid[2], bgSolid[3]);
            else
                w3dEngine::ClearColor(ColorList[static_cast<int>(ColorID::background)][0], ColorList[static_cast<int>(ColorID::background)][1],
                             ColorList[static_cast<int>(ColorID::background)][2], ColorList[static_cast<int>(ColorID::background)][3]);
        }
        // el clear respeta la mascara: si lo ultimo dibujado (un popup, un menu) dejo la escritura de profundidad
        // apagada, la profundidad no se limpiaba y la escena quedaba rechazada entera (el N8)
        w3dEngine::DepthMask(true);
        w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    }

    if (!cropScene) w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Viewport(x, glY, width, height);

    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Disable(w3dEngine::ColorMaterial);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    // MODO DE DIBUJO del pase (flags del Core + la luz frontal de los modos de
    // preview). Va ACA, con la MODELVIEW en identidad: la posicion de una luz de GL
    // se transforma por la modelview del momento, asi que ponerla despues de cargar
    // la matriz de vista la deja en otro espacio y la escena se dibuja oscura.
    // La derivacion vive en EscenaRender, compartida con el runtime del juego: si el
    // juego eligiera otra, el .deb se veria distinto del Play.
    // MODO DE DIBUJO del pase: los flags que lee el Core + la POSICION de la luz
    // frontal de los modos de preview. Va ACA, con la MODELVIEW en identidad: GL
    // transforma la posicion de una luz por la modelview del momento, asi que
    // ponerla despues de cargar la matriz de vista deja la escena oscura.
    // La derivacion vive en EscenaRender, compartida con el runtime del juego: si el
    // juego eligiera otra, el .deb se veria distinto del Play.
    W3dScene3DMode((int)(RenderType::Enum)view);

    UpdateViewOrbit();

    w3dEngine::Enable(w3dEngine::DepthTest);

    // Dibujar overlays (en MODO JUEGO PURO ni la grilla/piso: cero editor)
    if (showOverlays && !g_pureGame) {
            w3dEngine::Material(w3dEngine::MatDiffuse,  ColorList[static_cast<int>(ColorID::black)]);
            w3dEngine::Material(w3dEngine::MatAmbient,  ColorList[static_cast<int>(ColorID::black)]);
            w3dEngine::Material(w3dEngine::MatSpecular, ColorList[static_cast<int>(ColorID::black)]);

        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Disable(w3dEngine::Lighting);
        w3dEngine::Disable(w3dEngine::Blend);

        w3dEngine::TexFilter(false);
        w3dEngine::TexFilter(false);
        w3dEngine::BlendAlpha();

        w3dEngine::Enable(w3dEngine::DepthTest);
        w3dEngine::Disable(w3dEngine::Texture2D);

        w3dEngine::DisableArray(w3dEngine::TexCoordArray);
        w3dEngine::DisableArray(w3dEngine::NormalArray);

        w3dEngine::Disable(w3dEngine::ColorMaterial);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        //w3dEngine::DepthMask(true);

        // el piso/grilla va por el MISMO gate que el resto (ya estamos dentro de
        // `if (showOverlays)`). Antes el piso quedaba fuera del corte de modo juego y
        // jugando se veia la grilla pero no los contornos: "Mostrar Superposiciones"
        // significaba dos cosas distintas segun la rama.
        if (!floorAfter && (showFloor || showXaxis || showYaxis)) RenderFloor();
        w3dEngine::DepthMask(true);
    }

    // Edit Mode: marcar la malla ACTIVA para que su render dibuje el overlay de
    // edicion (vertices como puntos + bordes encima) en vez del contorno de objeto.
    // (misma derivacion COMPARTIDA que usa el cambio de modo; PC la refresca por frame
    //  para seguir a ActiveObj, Symbian la setea al cambiar de modo)
    UpdateActiveEditMesh();

    // master de overlays para el render del CORE (Mesh::Render lee g_showOverlays para no dibujar
    // contornos de seleccion ni el overlay de edit). Se setea por frame = el showOverlays del viewport,
    // SIN cortarlo por el modo juego: "Show Overlays" significa lo mismo jugando que editando (ver el
    // comentario de ReloadLights). Al dar Play, GamePrepareViewports(true) ya lo deja apagado; si el
    // usuario lo prende, ve las curvas, los empties, los huesos, los gizmos y los contornos.
    // EXCEPCION: el MODO JUEGO PURO (VERDE+0) apaga TODO lo del editor a la fuerza.
    const bool ovl = showOverlays && !g_pureGame;
    g_showOverlays    = ovl;
    w3dRenderOverlays = ovl; // el Core lo lee (Mesh::RenderObject); g_showOverlays sigue siendo del editor
    // overlays por tipo (submenu "Objects"): del viewport -> globales que lee el traversal del Core (Empty/Camera/luz)
    g_showLights = showLights && ovl; g_showCamera = showCamera && ovl; g_showEmpty = showEmpty && ovl;
    g_showParticles = showParticles && ovl; g_showCurves = showCurves && ovl;
    { extern bool g_showHitbox; g_showHitbox = showHitbox && ovl; }
    { extern bool g_showSounds; g_showSounds = showSounds && ovl; }   // objects/Sound.cpp (icono, esferas y pick)   // objects/Hitbox.cpp (dibujo y pick)
    // LOCAL VIEW de ESTE viewport -> globals que lee Object::Render (mismo patron que g_showOverlays). El set
    // vive en el viewport; otro viewport sin local view lo deja en false/NULL al publicar SU estado.
    // lo que NACE mientras dura la vista local (Separar con P, Shift+D, Aniadir, Pegar...) entra a la vista local de la
    // Vista 3D donde se esta trabajando, como en Blender. Antes el objeto nuevo desaparecia y parecia un error (duenio,
    // 2026-11-09). Las OTRAS Vistas 3D en vista local solo se ponen al dia (no lo suman)
    if (localViewActive) {
        const unsigned int spawned = W3dObjectsSpawned();
        if (spawned != lvSpawned) {
            if (Viewport3DActive == this) LV_AddSpawned(SceneCollection, lvSpawned, localViewSet, localViewStep);
            lvSpawned = spawned;
        }
    }
    { g_localViewActive = localViewActive; g_visibleLocalView = localViewActive ? &localViewSet : 0;
      g_localViewStep = localViewActive ? &localViewStep : 0; }

    // (los flags de dibujo del Core los dejo W3dScene3DMode mas arriba, junto con la
    //  posicion de la luz: tienen que quedar puestos con la modelview en identidad.)

    // ZBuffer: RE-seteamos el fog de profundidad JUSTO antes de dibujar las mallas (RenderFloor lo pisa
    // con su propio fog). Asi las mallas se sombrean por profundidad: cerca claro -> lejos oscuro.
    if (view == RenderType::ZBuffer) {
        w3dEngine::Enable(w3dEngine::Fog);
        w3dEngine::FogMode(0);
        w3dEngine::FogStart(fogNear); // mismo rango FIJO que arriba
        w3dEngine::FogEnd(fogFar);
        GLfloat fogZ[] = {0.0f, 0.0f, 0.0f, 1.0f};
        w3dEngine::FogColor(fogZ);
    }

    WeightPaintUpdate(); // prende/apaga el degradado de peso en el mesh activo (modo Weight Paint)

    const unsigned clears0 = g_w3dClearsColor;   // (si el proyecto limpio el color en este cuadro: las barras)
    // Renderiza la escena recursivamente
    { double _tScn0 = W3dNowMs();
      // base para las ESTADISTICAS del frame de este viewport (overlay "faces"/"gl"):
      // el DELTA de los contadores del Core alrededor del pase de escena es lo que
      // ESTE pase emitio de verdad (misma fuente que 'bench'; no se resetea nada,
      // asi bench/presupuesto siguen midiendo lo suyo sin interferencia).
      const int _stTris0 = w3dEngine::g_statIndices;
      const int _stDraw0 = w3dEngine::g_statDrawTris;
      const int _stBind0 = w3dEngine::g_statTexBinds;
      const int _stEst0  = w3dEngine::g_statStateChanges;
      const int _stOtr0  = w3dEngine::g_otherStat;
      // EL PASE DE ESCENA vive en main/render/SceneRender.cpp y lo llama TAMBIEN el
      // runtime del juego compilado: recorrido del arbol (que adentro resuelve culling,
      // LOD, visibilidad por celdas, espejos, instancias, luces y lotes) y despues los
      // tres diferidos, en este orden:
      //   CALCOMANIAS SUELTAS (sombras): con toda la escena opaca ya dibujada. Antes se
      //     dibujaban en su lugar del arbol y cualquier opaco posterior las borraba (no
      //     escriben z a proposito).
      //   LUCES SUELTAS (mallas 100% aditivas: chispas, halos, rayos): por la misma razon.
      //   PARTICULAS (translucidas): con el z-buffer de los opacos ya escrito, y ANTES de
      //     los overlays del editor (huesos, motion trail, passepartout, UI). La modelview
      //     quedo en la matriz de VISTA -> se dibujan en coords de MUNDO.
      // (El resync del cache de estado y el sello de lote tambien pasaron para alla.)
      if (view == RenderType::Vectors) VectorView();   // (en vez de la escena: el color de su movimiento)
      else {
          // (los objetos Postproceso actuan durante el recorrido: ver PostInTree)
          const bool withPost = (view == RenderType::Rendered) && camFrameOn;
          if (withPost) { gVpPost = this; gVpPostGlY = glY; g_postInTree = PostInTree; gDeferedPost.clear(); }
          // la UI del juego se ve mirando por la camara o jugando (ver RenderUI): ahi puede ir en su lugar del arbol
          gHudInTree = false;
          const bool withUI = !onlyScene && (camFrameOn || g_pureGame || AnimIsGame);
          if (withUI) { gVpUI = this; gVpUIglY = glY; g_uiInTree = UIInTree; }
          W3dScene3DPass();
          W3dBackgroundAutoSet(NULL);   // (el color de Auto de este visor: lo que se dibuje despues usa el del render)
          if (withPost) { gVpPost = NULL; g_postInTree = 0; }
          if (withUI) { gVpUI = NULL; g_uiInTree = 0; }
      }
      // (P1) auditoria del cache de estado al CERRAR el pase de escena (solo si el
      // harness la prendio con 'glaudit'; en el frame normal cuesta cero): si algo
      // del pase toco GL crudo, aca se detecta y se acumula el desync.
      if (w3dEngine::g_auditScene) w3dEngine::g_auditDesyncs += w3dEngine::AuditState();
      // cierre de las ESTADISTICAS del pase de escena de ESTE viewport (overlay)
      statTrisFrame   = (w3dEngine::g_statIndices      - _stTris0) / 3;
      statDrawsFrame  =  w3dEngine::g_statDrawTris     - _stDraw0;
      statBindsFrame  =  w3dEngine::g_statTexBinds     - _stBind0;
      statStatesFrame =  w3dEngine::g_statStateChanges - _stEst0;
      statOtherFrame  =  w3dEngine::g_otherStat        - _stOtr0;
      // exponer caras/draws del viewport ACTIVO como globals planos (para el [PERF] del juego).
      { extern int g_renderFaces, g_renderDraws;
        if (Viewport3DActive == this) { g_renderFaces = statTrisFrame; g_renderDraws = statDrawsFrame; } }
      g_prof.scene += W3dNowMs() - _tScn0; } // profiler: escena (skinning + modelos)

    // la GRILLA de un proyecto que limpia el solo: despues de la escena (su "Clear" la habria borrado), con el estado
    // que espera (la escena dejo cualquiera) y contra el z de la escena
    if (floorAfter && showOverlays && !g_pureGame && (showFloor || showXaxis || showYaxis)) {
        w3dEngine::UnbindVBOs();   // (la grilla apunta a RAM)
        w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::Disable(w3dEngine::Blend); w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Disable(w3dEngine::ColorMaterial); w3dEngine::AlphaTest(0.0f);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::DisableArray(w3dEngine::TexCoordArray); w3dEngine::DisableArray(w3dEngine::NormalArray);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::Enable(w3dEngine::DepthTest); w3dEngine::DepthFunc(w3dEngine::DepthLess);   // (la de GL por defecto)
        RenderFloor();
        w3dEngine::DepthMask(true);
    }
    if (cropScene) w3dEngine::Disable(w3dEngine::ScissorTest);

    // TRAZADO DE RAYOS (vista Rendered con "Ray Tracing" tildado): avanza unos tiles y pega la imagen trazada
    // sobre la escena GL (una textura del tamano del viewport). Los overlays del editor siguen encima con GL.
    if (view == RenderType::Rendered && g_rt.on) RTViewportStep(this);
    // EL POSTPROCESADO con el trazado de rayos: los objetos Postproceso que el recorrido encontro, sobre la imagen
    // trazada (ya pegada), en el orden del arbol
    if (!gDeferedPost.empty()) {
        for (size_t i = 0; i < gDeferedPost.size(); i++) {
            W3dPostUse(&gDeferedPost[i]->opt); gInPost = true;
            RunPostProcess(glY);
            gInPost = false; W3dPostUse(NULL);
        }
        gDeferedPost.clear();
    }
    W3dPostDataForget(); gCpuProfReady = gCpuVecReady = false;
    // la imagen trazada tapa lo que el pase de escena dibujo como overlay (la piramide de las CAMARAS, la linea de las
    // LUCES): se dibujan otra vez encima, contra el z de la escena. (Sin el trazado, el postprocesado los deja como
    // estaban en la imagen: redibujarlos encima duplicaba los semitransparentes)
    if (view == RenderType::Rendered && g_rt.on && ovl) RTGizmosOnTop(SceneCollection);

    // huesos encima de todo (ignoran z-buffer). Es OVERLAY del editor: se apaga con "Show Overlays",
    // con su propio toggle "Armature" del menu de overlays, o jugando (ovl ya corta por modo juego).
    if (ovl && showArmature) RenderArmaturesOnTop(SceneCollection);
    // Motion Trail: mismo criterio (overlay del editor, en x-ray). Lo prende el menu Animation.
    if (ovl) RenderMotionTrail();

    LoopCutRenderPreview(); // preview del corte (loop cut) encima de la escena

    if (ovl) RenderOverlay();
    RenderCamPassepartout(); // marco de camara (lo que se va a renderizar) + oscurecido afuera. Antes de la UI (queda debajo).
    // un proyecto que NO limpio el color en este cuadro (sin "Clear", o solo profundidad): el 3D deja estela, que es lo
    // pedido, pero las BARRAS del area son translucidas sobre el 3D y se encimaban con las del cuadro anterior (texto
    // fantasma). Sus franjas se limpian con el fondo del tema: la interfaz del editor no se ensucia
    if (cleanProject && !g_pureGame && g_w3dClearsColor == clears0) {
        const int arri = barDown ? 0 : BarTopOffset();
        const int abaj = (ToolbarVisible() ? ToolbarHeight() : 0) + (barDown ? BarTopOffset() : 0);
        const float* bg = ColorList[static_cast<int>(ColorID::background)];
        w3dEngine::ClearColor(bg[0], bg[1], bg[2], 1.0f);
        w3dEngine::Enable(w3dEngine::ScissorTest);
        if (arri > 0) { w3dEngine::Scissor(x, glY + height - arri, width, arri); w3dEngine::Clear(w3dEngine::ColorBuffer); }
        if (abaj > 0) { w3dEngine::Scissor(x, glY, width, abaj); w3dEngine::Clear(w3dEngine::ColorBuffer); }
        w3dEngine::Disable(w3dEngine::ScissorTest);
    }
    // SIEMPRE: RenderUI es el chrome del area (toolbar/menus/bordes) Y el reseteo del
    // estado GL 2D tras las mallas. La opcion ShowUi que lo salteaba se dio de baja:
    // sin chrome el viewport quedaba inusable y el estado sucio (CULL_FACE + arrays
    // prendidos) contaminaba el proximo dibujo 2D (los errores graficos reportados).
    if (onlyScene) { State2D(); g_prof.viewport3d += W3dNowMs() - _tVp0; return; }
    RenderUI();
    RenderSnapIndicator(); // recuadro verde en el target de snap (encima de todo)
    WP3DRenderBrush(this); // circulo del pincel (modo Weight Paint), siguiendo al mouse
    BoxSelectRender3D(this); // cruz guia / caja de seleccion (tecla B)
    g_prof.viewport3d += W3dNowMs() - _tVp0; // profiler: cierra el tiempo de este viewport 3D
}

// los GIZMOS de las camaras y las luces sobre la imagen trazada (ver arriba): cada uno con su matriz de mundo, sobre
// la modelview de la VISTA que dejo el pase de escena
void Viewport3D::RTGizmosOnTop(Object* o) {
    if (!o) return;
    if (o != (Object*)SceneCollection && !o->visible) return;
    if (localViewActive && o != (Object*)SceneCollection && !localViewSet.count(o)) {   // (vista local: solo lo aislado;
        if (!localViewStep.count(o)) return;                                         //  por sus padres solo se pasa)
        for (size_t i = 0; i < o->Childrens.size(); i++) RTGizmosOnTop(o->Childrens[i]);
        return;
    }
    const ObjectType t = o->getType();
    if (t == ObjectType::camera || t == ObjectType::light) {
        extern void (*g_lightOverlayHook)(Light*);
        Matrix4 W; o->GetWorldMatrix(W);
        w3dEngine::PushMatrix();
        w3dEngine::MultMatrix(W.m);
        if (t == ObjectType::camera) o->RenderObject();
        else if (g_lightOverlayHook) g_lightOverlayHook((Light*)o);
        w3dEngine::PopMatrix();
    }
    if (t == ObjectType::sound) {   // las ESFERAS del alcance de un sonido 3D (lo dibuja el propio Sonido)
        Matrix4 W; o->GetWorldMatrix(W);
        // (en X-RAY: el alcance es una ayuda, se ve aunque este detras de la escena; contra el z no se veia)
        w3dEngine::Disable(w3dEngine::DepthTest);
        w3dEngine::PushMatrix(); w3dEngine::MultMatrix(W.m); o->RenderObject(); w3dEngine::PopMatrix();
        w3dEngine::Enable(w3dEngine::DepthTest);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) RTGizmosOnTop(o->Childrens[i]);
}

// ARMATURE: dibuja los huesos de TODAS las armatures de la escena como lineas AZULES, encima de todo (z-test OFF),
// asi se ven a traves del mesh. Se hace DESPUES de renderizar la escena (no dentro del recorrido del arbol) para que
// el mesh hijo no las tape. Usa la matriz de MUNDO de cada armature: head/tail estan en su espacio local (rest pose).

// ============================================================================
//  MOTION TRAIL: el camino que recorre el origen de los objetos SELECCIONADOS que tienen animacion de posicion.
//  Se dibuja en X-RAY (sin depth test), como los huesos: es una ayuda de edicion, tiene que verse siempre.
//  La linea alterna CLARO/OSCURO en cada FRAME. Eso es lo que la hace util: no se dibuja un punto por frame (con
//  20 frames entre dos keyframes serian 20 puntos y no se ve nada), pero contando los tramos claros/oscuros sabes
//  cuantos frames hay entre un keyframe y el siguiente. Donde la animacion va rapido los tramos son largos; donde
//  va lenta, cortos.
//  Los KEYFRAMES si llevan punto, con el mismo tamaño que los vertices del Edit Mode.
// ============================================================================
void Viewport3D::RenderMotionTrail(){
    namespace gfx = w3dEngine;
    if (!MotionTrailOn || ObjSelects.empty()) return;
    static std::vector<GLfloat> bright, dark, points;
    bright.clear(); dark.clear(); points.clear();
    std::vector<Vector3> pts; std::vector<int> keys; int from, until;
    // ---- POSE MODE: el camino de los HUESOS seleccionados (el trail del objeto armature no dice nada: lo que
    //      se mueve son los huesos). Los puntos vienen en espacio NODO -> se pasan a mundo con el world del
    //      armature, igual que se dibujan los huesos.
    if (InteractionMode == PoseMode && ActiveObj && ActiveObj->getType() == ObjectType::armature){
        Armature* a = (Armature*)ActiveObj;
        Matrix4 AW; a->GetWorldMatrix(AW);
        for (size_t b = 0; b < a->bones.size(); b++){
            if (!(a->bones[b].select || (int)b == a->activeBone)) continue;
            if (!MotionTrailBoneNode(a, (int)b, pts, keys, from, until)) continue;
            for (size_t i = 0; i < pts.size(); i++) pts[i] = AW * pts[i];
            for (size_t i = 1; i < pts.size(); i++){
                std::vector<GLfloat>& dst = ((i & 1) ? bright : dark);
                dst.push_back(pts[i-1].x); dst.push_back(pts[i-1].y); dst.push_back(pts[i-1].z);
                dst.push_back(pts[i].x);   dst.push_back(pts[i].y);   dst.push_back(pts[i].z);
            }
            for (size_t k = 0; k < keys.size(); k++){
                int idx = keys[k] - from;
                if (idx < 0 || idx >= (int)pts.size()) continue;
                points.push_back(pts[idx].x); points.push_back(pts[idx].y); points.push_back(pts[idx].z);
            }
        }
    }
    else
    for (size_t s2 = 0; s2 < ObjSelects.size(); s2++){
        if (!MotionTrailOf(ObjSelects[s2], pts, keys, from, until)) continue;
        // tramos: uno por FRAME, alternando el buffer -> claro/oscuro/claro...
        for (size_t i = 1; i < pts.size(); i++){
            std::vector<GLfloat>& dst = ((i & 1) ? bright : dark);
            dst.push_back(pts[i-1].x); dst.push_back(pts[i-1].y); dst.push_back(pts[i-1].z);
            dst.push_back(pts[i].x);   dst.push_back(pts[i].y);   dst.push_back(pts[i].z);
        }
        // un punto por KEYFRAME (no por frame)
        for (size_t k = 0; k < keys.size(); k++){
            int idx = keys[k] - from;
            if (idx < 0 || idx >= (int)pts.size()) continue;
            points.push_back(pts[idx].x); points.push_back(pts[idx].y); points.push_back(pts[idx].z);
        }
    }
    if (bright.empty() && dark.empty()) return;

    GLboolean light = gfx::IsEnabled(gfx::Lighting);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D);
    gfx::Disable(gfx::DepthTest);        // X-RAY: encima de todo, como los huesos
    gfx::DisableArray(gfx::NormalArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::LineWidth(2.0f);
    if (!dark.empty()){
        gfx::Color4f(0.30f, 0.30f, 0.35f, 1.0f);
        gfx::VertexPointer3f(0, &dark[0]);
        gfx::DrawLines((int)(dark.size() / 3));
    }
    if (!bright.empty()){
        gfx::Color4f(0.95f, 0.95f, 1.00f, 1.0f);
        gfx::VertexPointer3f(0, &bright[0]);
        gfx::DrawLines((int)(bright.size() / 3));
    }
    if (!points.empty()){                // los KEYFRAMES: mismo tamaño que los vertices del Edit Mode
        gfx::PointSize(6.0f);   // el MISMO que los vertices del Edit Mode
        const float* acc = ColorList[static_cast<int>(ColorID::accent)];
        gfx::Color4f(acc[0], acc[1], acc[2], 1.0f);
        gfx::VertexPointer3f(0, &points[0]);
        gfx::DrawPoints((int)(points.size() / 3));
        gfx::PointSize(1.0f);
    }
    gfx::LineWidth(1.0f);
    gfx::Enable(gfx::DepthTest);
    gfx::EnableArray(gfx::NormalArray);
    if (light) gfx::Enable(gfx::Lighting);
    gfx::Invalidate();
}

void Viewport3D::RenderArmaturesOnTop(Object* node){
    if (!node) return;
    // VISTA LOCAL: solo los huesos de lo aislado (antes se veian TODOS los esqueletos, aunque no estuvieran elegidos)
    // (tampoco los del armature PADRE de una malla elegida: por los padres solo se pasa, para llegar a lo aislado)
    if (localViewActive && node != (Object*)SceneCollection && !localViewSet.count(node)) {
        if (!localViewStep.count(node)) return;
        for (size_t c = 0; c < node->Childrens.size(); c++) RenderArmaturesOnTop(node->Childrens[c]);
        return;
    }
    if (node->visible && node->getType() == ObjectType::armature){
        Armature* arm = static_cast<Armature*>(node);
        if (!arm->bones.empty()){
            namespace gfx = w3dEngine;
            EvaluatePoseSkeleton(arm, CurrentFrame); // pose animada (o rest si no hay clip) -> poseHead/poseTail
            Matrix4 W; arm->GetWorldMatrix(W);
            gfx::MatrixMode(gfx::ModelView);
            gfx::PushMatrix();
            gfx::MultMatrix(W.m);
            GLboolean light = gfx::IsEnabled(gfx::Lighting);
            gfx::Disable(gfx::Lighting);
            gfx::Disable(gfx::Texture2D);
            gfx::Disable(gfx::DepthTest);   // encima de todo
            gfx::DisableArray(gfx::NormalArray);
            gfx::DisableArray(gfx::ColorArray);
            gfx::DisableArray(gfx::TexCoordArray);
            // huesos: linea SOLIDA head->tail (azul). Ademas, para cada hueso cuyo head NO coincide con el tail de su
            // padre (hueso emparentado pero "separado"), una linea PUNTEADA padre.tail->head para que quede clara la
            // conexion en la jerarquia. El punteado se fabrica a mano (GLES no tiene line stipple).
            // POSE MODE: cada hueso se colorea segun seleccion -> sin seleccionar AZUL, seleccionado VERDE, activo BLANCO
            // (multi-seleccion). En Object Mode todos van a 'buf' con el color del armature (verde/azul de objeto).
            // EDIT MODE de armature (Fase 3): mismos colores por seleccion PERO sobre head/tail CRUDOS (el rest que
            // se edita, no la pose) + PUNTOS en cada extremo (los agarraderos del drag).
            bool poseMode = (InteractionMode == PoseMode && arm == (Armature*)ActiveObj);
            bool editArm  = (InteractionMode == EditMode && arm == (Armature*)ActiveObj);
            std::vector<GLfloat> buf, dash, bufSel, bufAct; buf.reserve(arm->bones.size() * 6);
            std::vector<GLfloat> pts, ptsSel; // extremos (solo editArm): normal / del hueso seleccionado-activo
            for (size_t i = 0; i < arm->bones.size(); i++){
                const W3dBone& b = arm->bones[i];
                Vector3 H = editArm ? b.head : b.poseHead;
                Vector3 T = editArm ? b.tail : b.poseTail;
                std::vector<GLfloat>* dst = &buf;
                if (poseMode || editArm){
                    if ((int)i == arm->activeBone) dst = &bufAct;   // activo = blanco
                    else if (b.select)             dst = &bufSel;   // seleccionado = verde
                    // sino queda en buf (azul, ver mas abajo)
                }
                dst->push_back(H.x); dst->push_back(H.y); dst->push_back(H.z);
                dst->push_back(T.x); dst->push_back(T.y); dst->push_back(T.z);
                if (editArm){ // agarraderos: un punto por punta. Se resalta CADA punta seleccionada
                    // (seleccion por puntas ): hueso entero = las dos prendidas.
                    std::vector<GLfloat>& PH = b.selHead ? ptsSel : pts;
                    PH.push_back(H.x); PH.push_back(H.y); PH.push_back(H.z);
                    std::vector<GLfloat>& PT = b.selTail ? ptsSel : pts;
                    PT.push_back(T.x); PT.push_back(T.y); PT.push_back(T.z);
                }
                if (b.parent >= 0 && b.parent < (int)arm->bones.size()){
                    Vector3 A = editArm ? arm->bones[b.parent].tail : arm->bones[b.parent].poseTail;
                    Vector3 B = H;
                    if ((B - A).LengthSq() > 1e-6f){ // no coinciden -> gap: linea de parentesco A->B
                        // se guarda el SEGMENTO entero; el punteado lo pone la TEXTURA de la
                        // relationship line (ver el pase de abajo), no guiones a mano
                        dash.push_back(A.x); dash.push_back(A.y); dash.push_back(A.z);
                        dash.push_back(B.x); dash.push_back(B.y); dash.push_back(B.z);
                    }
                }
            }
            // color de 'buf': en Pose/Edit = AZUL (huesos sin seleccionar); en Object Mode = color del armature
            // (ACTIVO verde / seleccionado verde-secundario / no seleccionado azul, como el resto de objetos).
            bool active = (arm == (Armature*)ActiveObj);
            const float* col;
            if (poseMode || editArm){ static const float blueH[4] = {0.28f, 0.55f, 1.0f, 1.0f}; col = blueH; } // sin seleccionar = AZUL (el negro se perdia en el fondo)
            else if (active)         col = ColorList[(int)ColorID::accent];
            else if (arm->select)    col = ColorList[(int)ColorID::accentDark];
            else { static const float blue[4] = {0.28f, 0.55f, 1.0f, 1.0f}; col = blue; }
            gfx::LineWidth(2.0f);
            if (!buf.empty()){
                gfx::Color4f(col[0], col[1], col[2], 1.0f);
                gfx::VertexPointer3f(0, &buf[0]);
                gfx::DrawLines((int)(buf.size() / 3));
            }
            // huesos SELECCIONADOS (Pose Mode): VERDE, mas gruesos, encima.
            if (!bufSel.empty()){
                const float* sc = ColorList[(int)ColorID::accent];
                gfx::Color4f(sc[0], sc[1], sc[2], 1.0f);
                gfx::LineWidth(3.0f);
                gfx::VertexPointer3f(0, &bufSel[0]);
                gfx::DrawLines((int)(bufSel.size() / 3));
            }
            // hueso ACTIVO (Pose Mode): BLANCO, el mas grueso, encima de todo.
            if (!bufAct.empty()){
                gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
                gfx::LineWidth(3.5f);
                gfx::VertexPointer3f(0, &bufAct[0]);
                gfx::DrawLines((int)(bufAct.size() / 3));
            }
            // LINEA DE PARENTESCO (hueso emparentado con gap): PUNTEADA COMO LA DE EMPARENTAR
            // OBJETOS (RenderRelationshipsLines): la misma textura de puntos chicos
            // (relationshipLine.png repetida a lo largo, ~8 repeticiones por unidad de mundo) y
            // el mismo GRIS UI -- de OTRO color que el hueso, que con el punteado azul a mano se
            // confundia con una linea cortada (reporte del dueno). Un draw por linea (cada una
            // lleva su largo en la coord V); los huesos son pocos, no pesa.
            if (!dash.empty() && (int)Textures.size() > 3 && Textures[3] && Textures[3]->iID){
                gfx::Enable(gfx::Texture2D);
                gfx::EnableArray(gfx::TexCoordArray);
                gfx::Enable(gfx::Blend); gfx::BlendAlpha();
                gfx::BindTexture(Textures[3]->iID);
                gfx::TexFilter(false);
                gfx::TexWrap(true);
                SetColorID(ColorID::grayUI);
                gfx::LineWidth(1.0f);
                for (size_t k = 0; k + 5 < dash.size(); k += 6){
                    // largo en MUNDO (el espaciado de los puntos = el de la relationship line,
                    // que mide la distancia global): W * A vs W * B
                    Vector3 Aw = W * Vector3(dash[k],   dash[k+1], dash[k+2]);
                    Vector3 Bw = W * Vector3(dash[k+3], dash[k+4], dash[k+5]);
                    GLfloat tuv[4] = { 0.0f, 0.0f, 0.0f, (Bw - Aw).Length() * 8.0f };
                    gfx::VertexPointer3f(0, &dash[k]);
                    gfx::TexCoordPointer2f(0, tuv);
                    gfx::DrawLines(2);
                }
                gfx::Disable(gfx::Texture2D);
                gfx::DisableArray(gfx::TexCoordArray);
                gfx::Disable(gfx::Blend);
            }
            // EDIT de armature: los EXTREMOS (head/tail) como puntos, el agarradero del drag.
            if (!pts.empty() || !ptsSel.empty()){
                gfx::PointSize(6.0f);
                if (!pts.empty()){
                    gfx::Color4f(0.28f, 0.55f, 1.0f, 1.0f);
                    gfx::VertexPointer3f(0, &pts[0]);
                    gfx::DrawPoints((int)(pts.size() / 3));
                }
                if (!ptsSel.empty()){
                    gfx::PointSize(7.0f);
                    gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
                    gfx::VertexPointer3f(0, &ptsSel[0]);
                    gfx::DrawPoints((int)(ptsSel.size() / 3));
                }
                gfx::PointSize(1.0f);
            }
            gfx::LineWidth(1.0f);
            gfx::Enable(gfx::DepthTest);
            gfx::EnableArray(gfx::NormalArray);
            if (light) gfx::Enable(gfx::Lighting);
            gfx::PopMatrix();
            gfx::Invalidate();
        }
    }
    for (size_t c = 0; c < node->Childrens.size(); c++)
        RenderArmaturesOnTop(node->Childrens[c]);
}

// SNAP: recuadro verde (solo borde) en el target bajo el cursor mientras se mueve con snap ON. Asi sabes
// que esta enganchando y donde. g_snapSx/Sy son coords del viewport (las setea SnapFindTarget/SnapAjustar).
void Viewport3D::RenderSnapIndicator(){
    if (!g_snap.enabled || !g_snapHit) return;
    if (!(Viewport3DActive == this && (state==translation||state==rotation||state==EditScale))) return;
    namespace gfx = w3dEngine;
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);
    // el recuadro crece con la escala de la UI (a escala 2, 16 px) y con el DEDO es mas grande todavia: chico, con la yema
    // encima, ni se veia. El borde, de GlobalScale px (varios recuadros pegados: LineWidth > 1 no anda en todos los GLES)
    extern bool ToolbarUsesTouch();
    const int gs = GlobalScale > 0 ? GlobalScale : 1;
    const float h = (float)((ToolbarUsesTouch() ? 9 : 4) * gs);   // medio lado
    gfx::Color4f(0.15f, 1.0f, 0.2f, 1.0f); // verde
    for (int k = 0; k < gs; k++) {
        const float x0 = g_snapSx - h - k, x1 = g_snapSx + h + k, y0 = g_snapSy - h - k, y1 = g_snapSy + h + k;
        GLfloat b[] = { x0,y0, x1,y0,  x1,y0, x1,y1,  x1,y1, x0,y1,  x0,y1, x0,y0 };
        gfx::VertexPointer2f(0, b);
        gfx::DrawLines(8);
    }
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
    gfx::Invalidate();
}

// PASSEPARTOUT de camara: al mirar DESDE la camara, dibuja el BORDE BLANCO del render y OSCURECE afuera (0.5 negro).
// Lo de adentro del marco es EXACTAMENTE lo que va a salir en el render. Se adapta al viewport y al aspecto del render.
void Viewport3D::RenderCamPassepartout(){
    if (!camFrameOn) return;
    namespace gfx = w3dEngine;
    // con el JUEGO corriendo no va el borde blanco: si ademas el marco ocupa todo el viewport (el aspecto del juego
    // es el de la pantalla) no hay nada que dibujar. Ni el 2D ni el Invalidate del final (en el N95 son ~40 llamadas
    // por cuadro: la UI que sigue arma su estado 2D completo)
    if (AnimIsGame && GameSimActive() &&
        W3dScene3DNoBands((float)width, (float)height, camFrameNX, camFrameNY, camViewZoom, camViewPanX, camViewPanY))
        return;
    // 2D local del viewport (0..width, 0..height ; y hacia abajo)
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);
    float W = (float)width, H = (float)height;
    // afuera del marco: 4 bandas (arriba/abajo/izq/der), las MISMAS que dibuja el
    // runtime del juego (main/render/SceneRender.cpp). EN EL EDITOR van ATENUADAS
    // (negro 0.5: se ve que hay afuera y se puede auditar el culling, reporte del
    // dueno); NEGRO OPACO -lo que ve el jugador en el aparato, y lo que el juego
    // compilado dibuja siempre- solo con el toggle 'letterboxBlack' prendido
    // (preview del dispositivo; lo usan las pruebas encuadrepx).
    W3dScene3DBands(W, H, camFrameNX, camFrameNY, camViewZoom, camViewPanX, camViewPanY,
                      camFrameLetterbox && (letterboxBlack || g_pureGame));
    if (g_pureGame) { gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray); gfx::Invalidate(); return; }   // (sin borde)
    // el marco sigue el zoom/pan de INSPECCION (misma transform que la proyeccion). NDC->pixeles (y hacia abajo).
    float cx = (camViewPanX * 0.5f + 0.5f) * W;
    float cy = (0.5f - camViewPanY * 0.5f) * H;
    float hw = camFrameNX * camViewZoom * W * 0.5f, hh = camFrameNY * camViewZoom * H * 0.5f;
    float x0 = cx - hw, x1 = cx + hw, y0 = cy - hh, y1 = cy + hh;
    // BORDE BLANCO del marco = lo que se va a renderizar. Con el JUEGO corriendo
    // no se dibuja (el juego se ve limpio); con Stop vuelve.
    // VERDE con la camara activa SELECCIONADA (se sabe que lo que se edita es esa camara)
    if (!(AnimIsGame && GameSimActive())) {
        if (CameraActive->select) gfx::Color4f(0.30f, 0.85f, 0.35f, 1.0f);
        else gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        gfx::LineWidth(1.0f);
        GLfloat border[] = { x0,y0, x1,y0,  x1,y0, x1,y1,  x1,y1, x0,y1,  x0,y1, x0,y0 };
        gfx::VertexPointer2f(0, border);
        gfx::DrawLines(8);
    }
    // restaurar textura + texcoords para la UI que sigue (texto/botones)
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
    gfx::Invalidate();
}

// ============================================================================
//  RENDER A PNG por TILES — dibuja la escena a un PNG de outW x outH (puede ser
//  MAS GRANDE que la ventana/pantalla). Sin overlay ni UI. Anda en todos lados
//  (incluido el N95): la imagen se rinde en pedazos que entran en el framebuffer
//  (tope = tamano del viewport), cada tile con su SUB-FRUSTUM, se lee con
//  glReadPixels y se pega en la imagen final. 'pass' = Rendered / ZBuffer /
//  NormalView. glReadPixels es bottom-left -> SavePNG hace el flip vertical.
// ============================================================================
// cuantos TILES hacen falta para un render de outW x outH (para el total de la barra de progreso).
// Mismo tope de tile que RenderAPNG (el tamano del viewport).
// EL ANTIALIASING del render rasterizado (g_render.antialias): cada tile se dibuja 'S' veces mas grande y se promedia
static int SuperSampled() { const int s = g_render.antialias; return s < 1 ? 1 : (s > 4 ? 4 : s); }
int Viewport3D::NeededTiles(int outW, int outH) const {
    if (outW <= 0 || outH <= 0) return 0;
    const int S = SuperSampled();
    int capW = (width  > 0) ? width / S : outW;
    int capH = (height > 0) ? height / S : outH;
    if (capW < 1) capW = 1;
    if (capH < 1) capH = 1;
    int tileW = (outW < capW) ? outW : capW;
    int tileH = (outH < capH) ? outH : capH;
    if (tileW <= 0 || tileH <= 0) return 0;
    return ((outW + tileW - 1) / tileW) * ((outH + tileH - 1) / tileH);
}

#ifdef __EMSCRIPTEN__
extern "C" void WebUnloadFile(const char* path, const char* name); // main.cpp (EM_JS): baja un archivo del FS al disco
#endif
// la EXTENSION del frustum de la imagen COMPLETA (aspect = outW/outH, NO el del viewport) y su matriz
static void ExtensionImage(bool ortho, float fov, float orbitDist, float nearC, int outW, int outH,
                            float& left, float& right, float& bottom, float& top) {
    const float aspectR = (float)outW / (float)outH;
    if (ortho){
        // igual que el viewport: el tamano del volumen orto sigue el zoom (orbitDistance), no un valor fijo
        float size = orbitDist * tanf(fov * 0.5f * 3.14159265f / 180.0f);
        if (size < 0.001f) size = 0.001f;
        top = size; bottom = -size; right = size * aspectR; left = -right;
    } else {
        top = nearC * tanf(fov * 0.5f * 3.14159265f / 180.0f);
        bottom = -top; right = top * aspectR; left = -right;
    }
}
static void MatMul16(const float* a, const float* b, float* o) {   // o = a * b (column-major)
    for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) {
        float s = 0.0f; for (int k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k];
        o[c * 4 + r] = s;
    }
}
static void MatProjection(bool ortho, float l, float r, float b, float t, float n, float f, float* m) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    if (ortho) {
        m[0] = 2.0f / (r - l); m[5] = 2.0f / (t - b); m[10] = -2.0f / (f - n);
        m[12] = -(r + l) / (r - l); m[13] = -(t + b) / (t - b); m[14] = -(f + n) / (f - n); m[15] = 1.0f;
    } else {
        m[0] = 2.0f * n / (r - l); m[5] = 2.0f * n / (t - b); m[8] = (r + l) / (r - l); m[9] = (t + b) / (t - b);
        m[10] = -(f + n) / (f - n); m[11] = -1.0f; m[14] = -2.0f * f * n / (f - n);
    }
}
// EL LENTE de la imagen del render: el de la CAMARA activa (su fov, ortografica, distancias minima y maxima: lo mismo
// que la vista al mirar por ella, LoadProjectionView); sin camara, el del visor. Antes el render usaba siempre el
// del visor (lejos = 1000): lo que la camara dibujaba mas lejos -el cielo de un escenario- salia negro en el archivo
void Viewport3D::LensImage(bool& ortho, float& fov, float& n, float& f) const {
    if (CameraActive) {
        ortho = CameraActive->orthographic; fov = CameraActive->fov;
        n = CameraActive->nearClip; f = CameraActive->farClip;
    } else { ortho = orthographic; fov = fovDeg; n = nearClip; f = farClip; }
    if (n <= 0.0f) n = 0.001f;
    if (f <= n) f = n + 1.0f;
    if (ortho) f += orbitDistance;   // (far corrido: no cortar el objeto al alejar)
}
void Viewport3D::MatrixImage(int outW, int outH, float* VP16){
    float left, right, bottom, top;
    bool lOrtho; float lFov, lNear, lFar; LensImage(lOrtho, lFov, lNear, lFar);
    ExtensionImage(lOrtho, lFov, orbitDistance, lNear, outW > 0 ? outW : 1, outH > 0 ? outH : 1, left, right, bottom, top);
    float P[16], V[16];
    MatProjection(lOrtho, left, right, bottom, top, lNear, lFar, P);
    const bool camPrev = ViewFromCameraActive;
    if (CameraActive) ViewFromCameraActive = true;   // (como RenderAPNG: desde la camara activa)
    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    UpdateViewOrbit();
    w3dEngine::GetMatrix(w3dEngine::ModelView, V);
    ViewFromCameraActive = camPrev;
    MatMul16(P, V, VP16);
}
// LA VISTA "MOTION VECTORS": en cada dibujo, la escena en el cuadro ANTERIOR (y la camara, si se mira por ella) se
// recuerda en la ranura del viewport y se vuelve al actual para pintar cada malla con su movimiento en la pantalla
// (lo mismo que el pase _vectores del render). Llamar con la proyeccion y la VISTA de este viewport cargadas
void Viewport3D::VectorView() { MotionView(false, 1.0f); }
void Viewport3D::DatumView(bool depth, bool ownZ) {
    namespace gfx = w3dEngine;
    float bg[4]; if (depth) W3dVecBackgroundColorData(bg); else W3dVecBackgroundColor(bg);
    gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
    if (ownZ) {
        // DE ANTEMANO (antes de la escena): con SU profundidad, escrita y probada como cualquier dibujo
        gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
        gfx::Enable(gfx::DepthTest); gfx::DepthFunc(gfx::DepthLEqual); gfx::DepthMask(true);
        MotionView(depth, FarPost());
        gfx::DepthFunc(gfx::DepthLess);
        return;
    }
    gfx::Clear(gfx::ColorBuffer);
    // LA PROFUNDIDAD DE LA ESCENA QUEDA COMO ESTA: el pase se dibuja contra ella (LEQUAL, un poco adelante y sin
    // escribirla), asi pasa justo lo visible y lo que se dibuja despues (la grilla, los overlays) la sigue encontrando
    // igual. Limpiarla y volver a escribirla con las posiciones de mundo dejaba diferencias de precision: la grilla
    // asomaba a traves de los objetos
    gfx::Enable(gfx::DepthTest); gfx::DepthFunc(gfx::DepthLEqual); gfx::DepthMask(false);
    gfx::Enable(gfx::PolygonOffsetFill); gfx::PolygonOffset(-1.0f, -4.0f);
    MotionView(depth, FarPost());
    gfx::PolygonOffset(0.0f, 0.0f); gfx::Disable(gfx::PolygonOffsetFill);
    gfx::DepthMask(true); gfx::DepthFunc(gfx::DepthLess);
}
void Viewport3D::MotionView(bool data, float far) {
    namespace gfx = w3dEngine;
    extern int CurrentFrame; extern void ApplyAnimationObjects();
    float P[16], V[16], VP[16], Vant[16], VPant[16];
    gfx::GetMatrix(gfx::Projection, P);
    gfx::GetMatrix(gfx::ModelView, V);
    MatMul16(P, V, VP);
    const bool byCamera = ViewFromCameraActive && CameraActive;
    const int f = CurrentFrame;
    CurrentFrame = f - 1; ApplyAnimationObjects();
    if (byCamera) { gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity(); UpdateViewOrbit(); gfx::GetMatrix(gfx::ModelView, Vant); }
    else memcpy(Vant, V, sizeof(Vant));
    MatMul16(P, Vant, VPant);
    W3dVecUseSlot(1);
    W3dVecRemember(SceneCollection, VPant);
    CurrentFrame = f; ApplyAnimationObjects();
    if (byCamera) { gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity(); UpdateViewOrbit(); }
    gfx::Invalidate();
    if (data) W3dVecDrawData(SceneCollection, VP, width, height, far);
    else W3dVecDraw(SceneCollection, VP, width, height);
    W3dVecUseSlot(0);
}

// ---------------------------------------------------------------------------
//  EL POSTPROCESADO de la vista Rendered (render/PostProcess.h). Con shaders, en la GPU sobre copias de la pantalla
//  (el pase de datos lo dibuja este viewport); sin shaders (GLES 1.1), la imagen se lee, la CPU le hace los efectos
//  (con la profundidad de un pase ZBuffer y el movimiento del pase de vectores, si hacen falta) y se pega encima
// ---------------------------------------------------------------------------
// (el pase de datos va con la proyeccion del viewport ENTERO; el postprocesado, sobre el rectangulo del marco)
static int gPostVp[4], gPostRect[4];
static void PostDataCB(void* ctx, bool depth) {
    w3dEngine::Viewport(gPostVp[0], gPostVp[1], gPostVp[2], gPostVp[3]);
    ((Viewport3D*)ctx)->DatumView(depth);
    w3dEngine::Viewport(gPostRect[0], gPostRect[1], gPostRect[2], gPostRect[3]);
}
void Viewport3D::PassDepthCPU() {
    namespace gfx = w3dEngine;
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    W3dScene3DLights((int)RenderType::ZBuffer); W3dScene3DMode((int)RenderType::ZBuffer);
    UpdateViewOrbit();
    gfx::Enable(gfx::Fog); gfx::FogMode(0); gfx::FogStart(nearClip); gfx::FogEnd(40.0f);
    { GLfloat fz[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; gfx::FogColor(fz); }
    gfx::ClearColor(0.0f, 0.0f, 0.0f, 1.0f); gfx::DepthMask(true);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
    gfx::Enable(gfx::DepthTest);
    W3dScene3DPass();
    gfx::Disable(gfx::Fog);
    // (la vista de nuevo en su modo)
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    W3dScene3DLights((int)(RenderType::Enum)view); W3dScene3DMode((int)(RenderType::Enum)view);
    UpdateViewOrbit();
}
static void GiveTurnRows(unsigned char* p, int w, int h) {
    std::vector<unsigned char> row((size_t)w * 4);
    for (int y = 0; y < h / 2; y++) {
        unsigned char* a = p + (size_t)y * w * 4; unsigned char* b = p + (size_t)(h - 1 - y) * w * 4;
        memcpy(&row[0], a, row.size()); memcpy(a, b, row.size()); memcpy(b, &row[0], row.size());
    }
}
// el MARCO de la camara en coords de GL: donde va el postprocesado (en el modo juego puro, el viewport entero)
bool Viewport3D::RectPost(int glY, int& rx, int& ry, int& w, int& h) const {
    if (!camFrameOn || width <= 0 || height <= 0) return false;
    rx = x; ry = glY; w = width; h = height;
    if (!g_pureGame) {
        const float W = (float)width, H = (float)height;
        const float cx = (camViewPanX * 0.5f + 0.5f) * W, cy = (0.5f - camViewPanY * 0.5f) * H;   // (RenderCamPassepartout)
        const float hw = camFrameNX * camViewZoom * W * 0.5f, hh = camFrameNY * camViewZoom * H * 0.5f;
        int x0 = (int)(cx - hw + 0.5f), x1 = (int)(cx + hw + 0.5f), y0 = (int)(cy - hh + 0.5f), y1 = (int)(cy + hh + 0.5f);
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > width) x1 = width; if (y1 > height) y1 = height;
        if (x1 - x0 < 2 || y1 - y0 < 2) return false;
        rx = x + x0; w = x1 - x0; h = y1 - y0;
        ry = glY + (height - y1);   // (GL: desde abajo)
    }
    return true;
}
// los DATOS de antemano (los objetos Postproceso que los piden): ver W3dPostDataPrevious. Por CPU se leen a estos
static std::vector<float> gCpuProf; static std::vector<unsigned char> gCpuVec;
static void GiveTurnRows(unsigned char* p, int w, int h);
static void PostDataPrevCB(void* ctx, bool depth) { ((Viewport3D*)ctx)->DatumView(depth, true); }
void Viewport3D::PostPreviousData(int glY) {
    namespace gfx = w3dEngine;
    gCpuProfReady = gCpuVecReady = false;
    W3dPostDataForget();
    bool prof = false, mov = false;
    { std::vector<PostProcess*> ps; W3dPostProcessesOf(SceneCollection, ps);
      for (size_t i = 0; i < ps.size(); i++) {
          if (!ps[i]->opt.on) continue;
          W3dPostUse(&ps[i]->opt);
          if (W3dPostHas()) { prof = prof || W3dPostNeedsDepth(); mov = mov || W3dPostNeedsMotion(); }
      }
      W3dPostUse(NULL); }
    if (!prof && !mov) return;
    int rx, ry, w, h;
    if (!RectPost(glY, rx, ry, w, h)) return;
    gfx::Viewport(x, glY, width, height);
    gfx::Enable(gfx::ScissorTest); gfx::Scissor(rx, ry, w, h);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    W3dScene3DLights((int)(RenderType::Enum)view); W3dScene3DMode((int)(RenderType::Enum)view);
    UpdateViewOrbit();
    if (!W3dPostDataPrevious(rx, ry, w, h, PostDataPrevCB, this, prof, mov)) {
        // POR CPU: los mismos pases, leidos a memoria
        std::vector<unsigned char> aux((size_t)w * h * 4);
        if (mov) {
            DatumView(false, true);
            gCpuVec.resize((size_t)w * h * 4);
            gfx::ReadPixelsRGBA(rx, ry, w, h, &gCpuVec[0]);
            GiveTurnRows(&gCpuVec[0], w, h);
            gCpuVecReady = true;
        }
        if (prof) {
            PassDepthCPU();
            gfx::ReadPixelsRGBA(rx, ry, w, h, &aux[0]);
            GiveTurnRows(&aux[0], w, h);
            gCpuProf.resize((size_t)w * h);
            const float n0 = nearClip, range = 40.0f - nearClip;
            for (size_t i = 0; i < gCpuProf.size(); i++) gCpuProf[i] = n0 + (1.0f - aux[i * 4] / 255.0f) * range;
            gCpuProfReady = true;
        }
    }
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Invalidate();
}
void Viewport3D::RunPostProcess(int glY) {
    namespace gfx = w3dEngine;
    extern int CurrentFrame;
    if (!W3dPostHas() || width <= 0 || height <= 0) return;
    // los efectos son los de la CAMARA: solo mirando por la camara activa, y solo adentro de su marco (lo que sale en
    // el render). En el modo juego puro (sin la UI) el marco es la pantalla
    int rx, ry, w, h;
    if (!RectPost(glY, rx, ry, w, h)) return;
    const unsigned seed = (unsigned)CurrentFrame * 7919u + 17u;
    gfx::Enable(gfx::ScissorTest); gfx::Scissor(rx, ry, w, h);
    gPostVp[0] = x; gPostVp[1] = glY; gPostVp[2] = width; gPostVp[3] = height;
    gPostRect[0] = rx; gPostRect[1] = ry; gPostRect[2] = w; gPostRect[3] = h;
    gfx::Viewport(rx, ry, w, h);
    const bool gpu = W3dPostGPU(rx, ry, w, h, PostDataCB, this, FarPost(), seed);
    gfx::Viewport(x, glY, width, height);
    if (gpu) {
        gfx::Disable(gfx::ScissorTest);
        gfx::Invalidate();
        return;
    }
    // ---- POR CPU ---- (los pases de datos dibujan el viewport entero; el tijeretazo los deja en el marco)
    static std::vector<unsigned char> img, aux, vec;
    static std::vector<float> prof;
    const bool prevOk = (gCpuProfReady ? gCpuProf.size() == (size_t)w * h : true) && (gCpuVecReady ? gCpuVec.size() == (size_t)w * h * 4 : true);
    img.resize((size_t)w * h * 4);
    gfx::ReadPixelsRGBA(rx, ry, w, h, &img[0]);
    GiveTurnRows(&img[0], w, h);   // (GL lee de abajo hacia arriba)
    const float* profP = NULL; const unsigned char* vecP = NULL;
    // (el movimiento PRIMERO: va contra la profundidad de la escena sin tocarla; el de profundidad redibuja la escena
    // entera y deja el z-buffer como estaba)
    if (W3dPostNeedsMotion() && gCpuVecReady && prevOk) vecP = &gCpuVec[0];   // (de antemano)
    else if (W3dPostNeedsMotion()) {
        DatumView(false);
        vec.resize((size_t)w * h * 4);
        gfx::ReadPixelsRGBA(rx, ry, w, h, &vec[0]);
        GiveTurnRows(&vec[0], w, h);
        vecP = &vec[0];
    }
    if (W3dPostNeedsDepth() && gCpuProfReady && prevOk) profP = &gCpuProf[0];   // (de antemano)
    else if (W3dPostNeedsDepth()) {
        PassDepthCPU();
        aux.resize((size_t)w * h * 4);
        gfx::ReadPixelsRGBA(rx, ry, w, h, &aux[0]);
        GiveTurnRows(&aux[0], w, h);
        prof.resize((size_t)w * h);
        const float n0 = nearClip, range = 40.0f - nearClip;
        for (size_t i = 0; i < prof.size(); i++) prof[i] = n0 + (1.0f - aux[i * 4] / 255.0f) * range;   // (la niebla: blanco cerca)
        profP = &prof[0];
    }
    W3dPostCPU(&img[0], w, h, profP, vecP, seed);
    // la imagen de vuelta, en una textura del tamanio de la vista (potencia de 2: el N95)
    static unsigned int tex = 0; static int tw = 0, th = 0;
    int pw = 64, ph = 64; while (pw < w) pw <<= 1; while (ph < h) ph <<= 1;
    if (!tex || pw != tw || ph != th) {
        if (tex) gfx::DeleteTexture(tex);
        std::vector<unsigned char> empty((size_t)pw * ph * 4, 255);
        tex = gfx::UploadRGBA(&empty[0], pw, ph, false, false);
        tw = pw; th = ph;
    }
    if (!tex) { gfx::Disable(gfx::ScissorTest); return; }
#ifdef W3D_SYMBIAN
    // EL MBX DEL N95 dibuja en diferido (por baldosas, terminando el cuadro anterior mientras la CPU sigue): pisar la
    // textura que todavia esta leyendo dejaba baldosas con la imagen vieja y otras con la nueva -los PARPADEOS de una
    // parte de la pantalla mientras se mueve-. Se espera a que termine (mas lento, sin parpadeos: lo pidio el dueno)
    gfx::Finish();
#endif
    gfx::UpdateRGBA(tex, 0, 0, w, h, &img[0], false);
    gfx::Viewport(rx, ry, w, h);   // (el quad, sobre el marco)
    gfx::MatrixMode(gfx::Projection); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Ortho(0, (float)w, (float)h, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Fog); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Blend); gfx::Enable(gfx::Texture2D);
    gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::Color4f(1, 1, 1, 1);
    gfx::BindTexture(tex);
    const float su = (float)w / (float)tw, sv = (float)h / (float)th;
    const GLshort v[8] = { 0, 0, (GLshort)w, 0, 0, (GLshort)h, (GLshort)w, (GLshort)h };
    const GLfloat uv[8] = { 0, 0, su, 0, 0, sv, su, sv };
    W3dDrawStrip4(v, uv);
    gfx::BindTexture(0);
    gfx::Viewport(x, glY, width, height);
    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
    gfx::Enable(gfx::DepthTest);
    gfx::Disable(gfx::ScissorTest);
}
bool Viewport3D::RenderAPNG(int outW, int outH, RenderType::Enum pass, const char* filename, int progBase, int progTotal){
    std::vector<unsigned char> full;
    if (!RenderABuffer(outW, outH, pass, full, progBase, progTotal)) return false;
    bool ok = w3dEngine::SavePNG(filename, &full[0], outW, outH, false);
#ifdef __EMSCRIPTEN__
    if (ok) {
        // web: bajar el PNG al disco del usuario (el FS de emscripten es virtual)
        std::string nm = filename;
        size_t sl = nm.find_last_of("/\\"); if (sl != std::string::npos) nm = nm.substr(sl + 1);
        WebUnloadFile(filename, nm.c_str());
    }
#endif
    return ok;
}
// la imagen entera en memoria (las franjas, una debajo de la otra): para lo que necesita el cuadro entero (el
// postprocesado, el desenfoque por vectores, el alfa de la composicion final, el JPG y los videos)
namespace { struct W3dCollectStripes {
    std::vector<unsigned char>* s; int w;
    static bool Fn(void* c, const unsigned char* px, int y0, int n) {
        W3dCollectStripes* j = (W3dCollectStripes*)c;
        memcpy(&(*j->s)[(size_t)y0 * j->w * 4], px, (size_t)n * j->w * 4);
        return true;
    }
}; }
bool Viewport3D::RenderABuffer(int outW, int outH, RenderType::Enum pass, std::vector<unsigned char>& output, int progBase, int progTotal){
    if (outW <= 0 || outH <= 0 || !SceneCollection) return false;
    output.resize((size_t)outW * outH * 4);
    W3dCollectStripes j; j.s = &output; j.w = outW;
    return RenderStripes(outW, outH, pass, W3dCollectStripes::Fn, &j, progBase, progTotal);
}
// LAS FRANJAS: la imagen de outW x outH se dibuja por TILES (puede ser mas grande que la ventana) y se entrega de a
// FRANJAS de arriba hacia abajo (una fila de tiles), con sus filas al derecho. Quien la recibe puede escribirla al
// archivo y soltarla: asi la imagen entera no esta nunca en memoria (el N95 y el N8 renderizan en 4K escribiendo en la
// tarjeta). false si fn falla (la escritura) o no hay escena
bool Viewport3D::RenderStripes(int outW, int outH, RenderType::Enum pass, W3dStripeFn fn, void* ctx, int progBase, int progTotal){
    if (outW <= 0 || outH <= 0 || !SceneCollection || !fn) return false;

    // tope de tile = lo que entra seguro en el framebuffer (el viewport ya entra en la ventana). Con ANTIALIASING ('S'
    // muestras por lado; no en los vectores, que son datos) cada tile se dibuja S veces mas grande y se promedia
    const int S = (pass == RenderType::Vectors) ? 1 : SuperSampled();
    int capW = (width  > 0) ? width / S : outW;
    int capH = (height > 0) ? height / S : outH;
    if (capW < 1) capW = 1;
    if (capH < 1) capH = 1;
    int tileW = (outW < capW) ? outW : capW;
    int tileH = (outH < capH) ? outH : capH;
    if (tileW <= 0 || tileH <= 0) return false;

    unsigned char* stripe = new unsigned char[(size_t)outW * tileH * 4];   // (una fila de tiles: lo unico entero en memoria)
    unsigned char* tile = new unsigned char[(size_t)tileW * tileH * 4];
    unsigned char* big = (S > 1) ? new unsigned char[(size_t)tileW * S * tileH * S * 4] : NULL;   // (el tile sin promediar)
    bool ok = true;

    // extension del frustum de la imagen COMPLETA (aspect = outW/outH, NO el del viewport)
    float top, bottom, left, right;
    bool lOrtho; float lFov, lNear, lFar; LensImage(lOrtho, lFov, lNear, lFar);   // (el de la camara activa)
    ExtensionImage(lOrtho, lFov, orbitDistance, lNear, outW, outH, left, right, bottom, top);
    // el pase de VECTORES cuenta en pixeles de la imagen entera: su matriz (desde la misma camara)
    float VPImage[16];
    if (pass == RenderType::Vectors) MatrixImage(outW, outH, VPImage);

    // fijar el PASE (sin overlay): modo + luces + flags del Core, una sola vez
    RenderType viewPrev = view; bool overlaysPrev = showOverlays;
    bool camPrev = ViewFromCameraActive;
    // el render se hace DESDE la camara activa: si hay camara, forzamos su POV.
    // Sin camara queda la vista del viewport (orbita). La proyeccion sigue con fovDeg (la Camera del editor
    // no tiene lente propio todavia). El gizmo de la camara no sale (showOverlays=false + este flag).
    if (CameraActive) ViewFromCameraActive = true;
    view = pass; showOverlays = false;
    ReloadLights();
    g_showOverlays = false; w3dRenderOverlays = false;
    w3dRenderWireframe   = false;
    w3dRenderSolid       = (pass == RenderType::Solid);
    w3dRenderNormalColor = (pass == RenderType::NormalView);
    w3dRenderNoLight     = (pass == RenderType::ZBuffer);
    w3dRenderLights      = (pass == RenderType::Rendered);
    w3dRenderAlpha       = (pass == RenderType::Alpha);
    const bool passVec   = (pass == RenderType::Vectors);
    // los pases de ANALISIS van con SU fondo (el de abajo) aunque el proyecto limpie solo (CONSTRUCTOR + "Limpiar
    // pantalla"): su "Clear" limpia la profundidad pero no el color, como en la vista (QUIEN LIMPIA, en Render). Sin
    // esto el pase de alfa salia con el "Clear color" del proyecto (rosa) en vez de negro
    const bool clearFixedPrev = g_w3dClearColorFixed;
    g_w3dClearColorFixed = W3dHasConstructor() && (pass == RenderType::ZBuffer || pass == RenderType::Alpha ||
                                                    pass == RenderType::NormalView || passVec);

    // color de fondo del pase
    float bg[4];
    if (pass == RenderType::ZBuffer || pass == RenderType::Alpha){ bg[0]=bg[1]=bg[2]=0.0f; bg[3]=1.0f; } // NEGRO (matte blanco sobre negro)
    else if (passVec) W3dVecBackgroundColor(bg);                                                                  // quieto: (128,128,0)
    else if (pass == RenderType::NormalView){ bg[0]=0.5f; bg[1]=0.5f; bg[2]=1.0f; bg[3]=1.0f; } // AZUL normal-map: normal (0,0,1) de frente = (0.5,0.5,1.0)
    else if (pass == RenderType::Rendered){
        bg[0]=g_renderBg[0]; bg[1]=g_renderBg[1]; bg[2]=g_renderBg[2]; bg[3]=g_renderBg[3]; // color de fondo GLOBAL del render
    } else { bg[0]=bg[1]=bg[2]=0.0f; bg[3]=0.0f; }                                       // resto: transparente (composicion)

    // las filas de tiles de ARRIBA hacia abajo ('arriba' = la primera fila de la franja contando desde arriba); cada
    // tile en coords BOTTOM-LEFT (como GL: ty)
    int tileIdx = 0; // para la barra: frac = (progBase + tiles hechos) / progTotal
    for (int up = 0; up < outH && ok && !ProgressCanceled(); up += tileH){
        const int th = (up + tileH <= outH) ? tileH : (outH - up);
        const int ty = outH - up - th;
        for (int tx = 0; tx < outW; tx += tileW){
            int tw = (tx + tileW <= outW) ? tileW : (outW - tx);

            float l = left   + (right  - left)   * (float)tx        / (float)outW;
            float r = left   + (right  - left)   * (float)(tx + tw) / (float)outW;
            float b = bottom + (top    - bottom) * (float)ty        / (float)outH;
            float t = bottom + (top    - bottom) * (float)(ty + th) / (float)outH;

            // (en el N95, la franja en una superficie APARTE: dibujada en la ventana se veia entre la barra de progreso)
            const bool outsideTile = W3dOutsideScreenBegin && W3dOutsideScreenBegin(tileW * S, tileH * S);
            w3dEngine::MatrixMode(w3dEngine::Projection);
            w3dEngine::LoadIdentity();
            if (lOrtho) w3dEngine::Ortho(l, r, b, t, lNear, lFar);
            else       w3dEngine::Frustum(l, r, b, t, lNear, lFar);

            w3dEngine::Viewport(0, 0, tw * S, th * S); // el tile en la esquina del framebuffer (S veces mas grande con antialiasing)

            w3dEngine::MatrixMode(w3dEngine::ModelView);
            w3dEngine::LoadIdentity();
            UpdateViewOrbit(); // carga la matriz de vista de la camara del viewport

            w3dEngine::Disable(w3dEngine::Texture2D);
            w3dEngine::Disable(w3dEngine::Blend);
            w3dEngine::Disable(w3dEngine::ColorMaterial);
            w3dEngine::EnableArray(w3dEngine::VertexArray);
            w3dEngine::DisableArray(w3dEngine::TexCoordArray);
            w3dEngine::DisableArray(w3dEngine::NormalArray);
            w3dEngine::Enable(w3dEngine::DepthTest);

            if (pass == RenderType::ZBuffer){
                w3dEngine::Enable(w3dEngine::Fog); w3dEngine::FogMode(0);
                w3dEngine::FogStart(lNear); w3dEngine::FogEnd(40.0f);
                GLfloat fz[4] = {0.0f, 0.0f, 0.0f, 1.0f}; w3dEngine::FogColor(fz);
            } else {
                w3dEngine::Disable(w3dEngine::Fog);
            }

            w3dEngine::ClearColor(bg[0], bg[1], bg[2], bg[3]);
            w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

            if (passVec) {   // VECTORES DE MOVIMIENTO: cada malla con el color de su movimiento (render/MotionVectors.h)
                w3dEngine::Invalidate();
                W3dVecDraw(SceneCollection, VPImage, outW, outH);
            } else {
            W3dParticlesClearPending();
    W3dDecalsClearPending();         // las sombras sueltas van al final del frame
    W3dLightsClearPending();         // idem las chispas/halos aditivos
            w3dEngine::Invalidate(); // (P1) resync una vez por pase, como en el viewport
            w3dBatchStamp++;         // (P4) sello del pase (lote estatico)
            // LA IMAGEN FINAL lleva lo mismo que la vista (render/SceneRender.cpp, W3dScene3DPass): las SONDAS DE LUZ
            // y las SOMBRAS EN TIEMPO REAL. Sin esto el render a archivo (PNG, JPG, el MP4 del telefono) salia sin ellas.
            // (Los pases de analisis -profundidad, normales, alfa- no: son datos)
            const bool imageFinal = (pass == RenderType::Rendered);
            if (imageFinal) { LightProbeNewPass(); W3dShadowsPass(); W3dScene3DBackgroundFile(); }
            W3dLightsPrePass(SceneCollection);   // las luces ANTES de la geometria (el fogonazo ilumina todo)
            SceneCollection->Render();
            W3dLightsPrePassEnd();
            // PROXY de edicion de Curve (riel/path): vive FUERA del arbol de escena a proposito
            // (no aparece en el outliner, no se guarda, no lo toca nada) y se dibuja explicito
            // aca, solo mientras se edita. Ver CurveEnterEdit (LayoutInput.cpp).
            { extern Object* W3dActiveCurveProxy();
              Object* px = W3dActiveCurveProxy(); if (px) px->Render(); }
            // CALCOMANIAS SUELTAS (sombras): recien ahora, con toda la escena opaca ya
    // dibujada. Antes se dibujaban en su lugar del arbol y cualquier opaco
    // posterior (troncos, EscenarioAlpha) las borraba: no escriben z a proposito.
    W3dDecalsDrawPending();
    W3dLightsDrawPending();      // chispas/halos aditivos (tambien en el render a PNG)
    W3dParticlesDrawPending();   // translucidas despues de los opacos (tambien en el render a PNG)
            if (imageFinal) { W3dShadowsEnd(); W3dScene3DBackgroundFileEnd(); }   // (como en la vista: despues de las calcomanias y las particulas)
            }

            if (S > 1) {
                // el ANTIALIASING: cada pixel, el promedio de sus S x S muestras
                w3dEngine::ReadPixelsRGBA(0, 0, tw * S, th * S, big);
                const int gw = tw * S, n = S * S;
                for (int y = 0; y < th; y++)
                    for (int x = 0; x < tw; x++) {
                        unsigned int acc[4] = { 0, 0, 0, 0 };
                        for (int sy = 0; sy < S; sy++) {
                            const unsigned char* q = big + ((size_t)(y * S + sy) * gw + (size_t)x * S) * 4;
                            for (int sx = 0; sx < S; sx++, q += 4) { acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]; acc[3] += q[3]; }
                        }
                        unsigned char* o = tile + ((size_t)y * tw + x) * 4;
                        for (int k = 0; k < 4; k++) o[k] = (unsigned char)((acc[k] + (unsigned)n / 2) / (unsigned)n);
                    }
            } else w3dEngine::ReadPixelsRGBA(0, 0, tw, th, tile);
            if (outsideTile) W3dOutsideScreenFinish();   // (la barra de progreso, en la ventana)
            for (int row = 0; row < th; row++){   // (GL lee de ABAJO hacia arriba: la franja va al derecho)
                memcpy(stripe + (size_t)((th - 1 - row) * outW + tx) * 4,
                       tile + (size_t)(row * tw) * 4,
                       (size_t)tw * 4);
            }
            // barra de progreso: redibujo COMPLETO por tile (el render pisa el framebuffer). progTotal
            // cuenta pases x tiles (lo arma ActionRenderImage); progBase = tiles de los pases anteriores.
            ++tileIdx;
            if (progTotal > 0) ProgressUpdateFull((float)(progBase + tileIdx) / (float)progTotal);
        }
        if (!fn(ctx, stripe, up, th)) ok = false;
    }

    // restaurar el estado del viewport (los flags de pase se re-setean solos en el proximo Render,
    // pero los limpiamos ya por las dudas de que algo dibuje antes)
    view = viewPrev; showOverlays = overlaysPrev; ViewFromCameraActive = camPrev;
    w3dRenderAlpha = false; w3dRenderNoLight = false; w3dRenderNormalColor = false;
    g_w3dClearColorFixed = clearFixedPrev;
    ReloadLights();

    delete[] stripe;
    delete[] tile;
    delete[] big;
    return ok;
}

void Viewport3D::RenderFloor() {
    //hay un error en la malla 3d!!!
    //explico... resulta que el fog se calcula en el vertice
    //pero como el vertice esta fuera del nearClip y el farClip se ve mal casi siempre
    //excepto en ciertos angulos... parece un glich. pero es asi openGL viejo
    //la solucion seria poner un vertice en el medio de la linea. eso arreglaria bastante el problema
    //por ahora el fog se quita... triste
    w3dEngine::Enable(w3dEngine::Fog);
    w3dEngine::FogMode(0);
    w3dEngine::FogStart(nearClip);
    w3dEngine::FogEnd(30.0f);

    if (view == RenderType::Rendered) {
        w3dEngine::FogColor(scene->backgroundColor);
    } else {
        w3dEngine::FogColor(ColorList[static_cast<int>(ColorID::background)]);
    }

    w3dEngine::LineWidth(1);
    w3dEngine::VertexPointer3f(0, objVertexdataFloor);

    // la grilla solo si "Floor" esta activo (antes se dibujaba siempre: por eso
    // apagar Floor no la ocultaba)
    if (showFloor) {
        w3dEngine::Color4f(
            W3dColors[W3dColor_LineFloor][0],
            W3dColors[W3dColor_LineFloor][1],
            W3dColors[W3dColor_LineFloor][2],
            W3dColors[W3dColor_LineFloor][3]
        );
        W3dDrawLinesF(objVertexdataFloor, objFacedataFloor, objFacesFloor);
    }

    // Linea Roja
    if (showXaxis) {
        w3dEngine::LineWidth(2);
        w3dEngine::Color4f(
            W3dColors[W3dColor_LineFloorRed][0],
            W3dColors[W3dColor_LineFloorRed][1],
            W3dColors[W3dColor_LineFloorRed][2],
            W3dColors[W3dColor_LineFloorRed][3]
        );
        W3dDrawLinesF(objVertexdataFloor, RedAxis, 2);
        w3dEngine::LineWidth(1);
    } else if (showFloor) {
        w3dEngine::Color4f(
            W3dColors[W3dColor_LineFloor][0],
            W3dColors[W3dColor_LineFloor][1],
            W3dColors[W3dColor_LineFloor][2],
            W3dColors[W3dColor_LineFloor][3]
        );
        W3dDrawLinesF(objVertexdataFloor, RedAxis, 2);
    }

    // Linea Verde
    if (showYaxis) {
        w3dEngine::LineWidth(2);
        w3dEngine::Color4f(
            W3dColors[W3dColor_LineFloorGreen][0],
            W3dColors[W3dColor_LineFloorGreen][1],
            W3dColors[W3dColor_LineFloorGreen][2],
            W3dColors[W3dColor_LineFloorGreen][3]
        );
        W3dDrawLinesF(objVertexdataFloor, GreenAxis, 2);
        w3dEngine::LineWidth(1);
    } else if (showFloor) {
        w3dEngine::Color4f(
            W3dColors[W3dColor_LineFloor][0],
            W3dColors[W3dColor_LineFloor][1],
            W3dColors[W3dColor_LineFloor][2],
            W3dColors[W3dColor_LineFloor][3]
        );
        W3dDrawLinesF(objVertexdataFloor, GreenAxis, 2);
    }

    w3dEngine::Disable(w3dEngine::Fog);
}

// linea-guia por 'c' a lo largo de 'dir' (world space, larga a ambos lados)
static void DrawLineAxisWorld(const Vector3& c, const Vector3& dir, int colorIdx) {
    const float L = 1000.0f;
    GLfloat v[6] = {
        c.x - dir.x * L, c.y - dir.y * L, c.z - dir.z * L,
        c.x + dir.x * L, c.y + dir.y * L, c.z + dir.z * L
    };
    w3dEngine::Color4fv(W3dColors[colorIdx]);
    w3dEngine::VertexPointer3f(0, v);
    w3dEngine::DrawLines(2);
}

void Viewport3D::RenderAllAxisTransform() {
    if (!ActiveObj) return;
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::DisableArray(w3dEngine::NormalArray);        // arrays colgados: N95 quisquilloso
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::LineWidth(2);

    // las guias se dibujan en WORLD space, a lo largo del eje EN LA ORIENTACION
    // elegida (global/local/view). Un solo eje = una linea; plano = las dos del
    // plano (excluye la del shift); XYZ = las tres. Centradas en el PIVOTE (no en
    // el objeto activo, que solo coincide en modo Active).
    Vector3 c = GizmoPivot();
    // EXTRUDE (o move con orientacion NORMAL via gEVuseCustom): la direccion REAL es gTransformNormal (la normal en
    // mundo), NO un eje X/Y/Z. Sin esto se dibujaba la linea del eje viejo de axisSelect (rojo/X por defecto) aunque la
    // transformacion vaya por la normal. UNA linea a lo largo de la normal (color Z: la normal es el "Z" de su orientacion).
    // Al constreñir a un eje real (tecla X/Y/Z) gEVuseCustom se apaga -> vuelve a la linea del eje. Compartido 4 OS.
    if (gEVuseCustom) {
        DrawLineAxisWorld(c, gTransformNormal, W3dColor_ColorTransformZ);
        return;
    }
    int a = axisSelect;
    bool drawX = (a == X || a == XYZ || a == PlaneY || a == PlaneZ);
    bool drawY = (a == Y || a == XYZ || a == PlaneX || a == PlaneZ);
    bool drawZ = (a == Z || a == XYZ || a == PlaneX || a == PlaneY);
    // EDIT de ARMATURE (G/R/S de huesos): los ejes son los del transform de puntas (en "Local", los del
    // hueso ACTIVO con Y a lo largo del hueso). La guia dibuja EXACTAMENTE el eje con el que se mueve.
    { Vector3 bex, bey, bez;
      if (BoneAxesWorld(bex, bey, bez)){
        if (drawX) DrawLineAxisWorld(c, bex, W3dColor_ColorTransformX);
        if (drawY) DrawLineAxisWorld(c, bey, W3dColor_ColorTransformY);
        if (drawZ) DrawLineAxisWorld(c, bez, W3dColor_ColorTransformZ);
        return;
      } }
    // POSE MODE: los ejes son los del HUESO (en "Local" el eje local es el del hueso, NO el del objeto armature).
    // Se piden los MISMOS que usa el transform de pose -> la guia coincide siempre con la rotacion real.
    { extern bool PoseAxesWorld(Vector3&, Vector3&, Vector3&);
      Vector3 pex, pey, fish;
      if (PoseAxesWorld(pex, pey, fish)){
        if (drawX) DrawLineAxisWorld(c, pex, W3dColor_ColorTransformX);
        if (drawY) DrawLineAxisWorld(c, pey, W3dColor_ColorTransformY);
        if (drawZ) DrawLineAxisWorld(c, fish, W3dColor_ColorTransformZ);
        return;
      } }
    if (drawX) DrawLineAxisWorld(c, AxisOriented(*ActiveObj, X), W3dColor_ColorTransformX);
    if (drawY) DrawLineAxisWorld(c, AxisOriented(*ActiveObj, Y), W3dColor_ColorTransformY);
    if (drawZ) DrawLineAxisWorld(c, AxisOriented(*ActiveObj, Z), W3dColor_ColorTransformZ);
}

void Viewport3D::RenderOverlay() {
    w3dEngine::Material(w3dEngine::MatDiffuse,  ColorList[static_cast<int>(ColorID::black)]);
    w3dEngine::Material(w3dEngine::MatAmbient,  ColorList[static_cast<int>(ColorID::black)]);
    w3dEngine::Material(w3dEngine::MatSpecular, ColorList[static_cast<int>(ColorID::black)]);

    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Texture2D);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    if (!SceneCollection->Childrens.empty()) {
        w3dEngine::LineWidth(1);

        if (ShowRelationshipsLines) RenderRelationshipsLines();
        RenderIcons3D();
        // "Ver en 3D" de la UI: los elementos 2D metidos EN la escena, con su profundidad Z
        if (UI2D_HasShowIn3D()) {
            UI2D_DrawInWorld();
            w3dEngine::BlendAlpha();
            w3dEngine::Disable(w3dEngine::Texture2D);
            w3dEngine::EnableArray(w3dEngine::VertexArray);
            w3dEngine::DisableArray(w3dEngine::TexCoordArray);
        }

        w3dEngine::Disable(w3dEngine::DepthTest);

        if (showLights) RenderLightLines(); // linea de cada luz al piso (espacio mundo); respeta el toggle "Lights"
        if (showOrigins) RenderOrigins();

        w3dEngine::Disable(w3dEngine::Blend);
        w3dEngine::Disable(w3dEngine::Texture2D);

        if (!SceneCollection->Childrens.empty() &&
            (state == translation || state == rotation || state == EditScale))
            RenderAllAxisTransform();
    }

    // en los modos de pintura el cursor 3D no sirve para nada (no se agrega ni se transforma): se oculta
    const bool paintCursor = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
    if (show3DCursor && !paintCursor) Render3Dcursor();
    if (showOverlays && !paintCursor) GizmoRender(this);   // gizmo de mover (Overlays > Gizmo)
    if (showOverlays && !paintCursor) ProportionalRender(this);   // circulo de influencia del proportional editing

    // (la barra de botones 2D NO se dibuja aca: es chrome del area, no un
    //  overlay. Va en RenderUI() junto con los bordes para que se vea aunque
    //  "Show Overlays" este off.)

#ifdef W3D_SYMBIAN
    // baseline que asumen el outliner/properties/cursor en el N95:
    // texcoords habilitados, sin luz, sin scissor, depth para el proximo
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::PointSprite);
    w3dEngine::DepthMask(true);
#endif
}

void Viewport3D::RenderRelationshipsLines() {
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::DepthMask(false);
    w3dEngine::TexCoordPointer2f(0, lineUV);
    SetColorID(ColorID::grayUI);
    w3dEngine::BindTexture(Textures[3]->iID);
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
    w3dEngine::TexWrap(true);
    w3dEngine::TexWrap(true);

    RenderLinkLines(SceneCollection);

    w3dEngine::DepthMask(true);
}

void Viewport3D::Render3Dcursor() {
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::PushMatrix();
    // SIN swap Y/Z: cursor3D.pos esta en convencion de OBJETO (la usan los snaps,
    // el add-at-cursor y el pivot PivotCursor3D). Antes swapeaba (x,z,y) y el cursor
    // se veia en otro lado que su pivote/los snaps.
    w3dEngine::Translatef(cursor3D.pos.x, cursor3D.pos.y, cursor3D.pos.z);

    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::PointSprite);
    w3dEngine::PointSize(16 * GlobalScale); // cursor 3D: 16 relativo a la UI
    w3dEngine::VertexPointer3s(0, pointVertex);
    w3dEngine::BindTexture(Textures[2]->iID);
#ifndef W3D_SYMBIAN
    // pixel perfect: sin filtrado
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    w3dEngine::PointSpriteCoordReplace(true);
    SetColorID(ColorID::accent);
    w3dEngine::DrawPoints(1);
    w3dEngine::PointSpriteCoordReplace(false);

    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::PointSprite);
    w3dEngine::Disable(w3dEngine::Blend);

    w3dEngine::VertexPointer3f(0, Cursor3DVertices);
    w3dEngine::LineWidth(2);
    SetColorID(ColorID::grayUI);
    W3dDrawLinesF(Cursor3DVertices, Cursor3DEdges, Cursor3DEdgesSize);

    w3dEngine::PopMatrix();
}

// El boton de CONTEXTO de la barra: es Object / Pose / Vertex / Edge / Face segun donde estes. Sin texto (comia
// ancho de barra): lo dice el ICONO, y el titulo de su menu lo confirma al abrirlo.
// FUERA del render a proposito: esto es ESTADO, no dibujo. Mientras vivio adentro de RenderUI -que es GL y no corre
// headless- era intesteable, y de hecho el texto se seguia reescribiendo por frame pisando al que lo sacaba.
void Viewport3D::SyncButtonContext(){
    Button* bObj = BarRoleBtn(BarButtons, BR_Object);
    if (!bObj) return;
    bool isMesh  = (ActiveObj && ActiveObj->getType() == ObjectType::mesh);
    bool isArm   = (ActiveObj && ActiveObj->getType() == ObjectType::armature);
    bool inEdit  = (isMesh && InteractionMode == EditMode);
    bool poseM   = (InteractionMode == PoseMode && isArm);
    bool editArm = (InteractionMode == EditMode && isArm); // Edit de huesos: el boton abre el menu "Armature"
    // en los modos de PINTURA el menu Object no va (no se puede seleccionar ni transformar): oculto
    const bool paint = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
    bObj->visible = (HasObjectsSelected() || poseM) && !paint;
    bObj->text.clear();
    bObj->icon = (poseM || editArm) ? (int)IconType::armature :
        !inEdit ? (int)IconType::object :
        (EditSelectMode == SelEdge) ? (int)IconType::selEdge :
        (EditSelectMode == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
}

void Viewport3D::State2D() {
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, width, height, 0, -1, 1);
    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();
    // estado 2D COMPLETO (no asumir lo que dejo el render de la escena:
    // antes la barra iba al final de RenderOverlay que ya lo seteaba; con
    // overlays off corre justo despues de los meshes 3D que dejan CULL_FACE
    // y NORMAL/COLOR arrays activos -> los botones se veian medio culeados
    // (un solo triangulo) y el texto desaparecia).
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::PointSprite);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
}

// LA PANTALLA DIVIDIDA de un juego (setVistas(2), script/W3dScript.h): el cuadro del juego -- el marco de la camara,
// encajado como siempre -- partido en dos (lado a lado o una arriba de la otra, con una raya negra en el medio), la
// escena una vez en cada mitad con la camara de esa vista (antes, el script reorienta lo suyo: antesDeVista) y el HUD
// UNA vez encima del cuadro entero (los medidores de los dos jugadores ya van uno a cada lado). Cada mitad es su
// imagen entera: el aspecto de la mitad, sin bandas. Solo el viewport activo, jugando y mirando por la camara
bool Viewport3D::RenderSplit() {
    extern bool AnimIsGame; extern bool PlayAnimation;
    if (Viewport3DActive != this || !ViewFromCameraActive || !CameraActive || !AnimIsGame) return false;
    if (!(PlayAnimation || GameSimActive()) || width < 8 || height < 8) return false;
    // el marco del juego en este viewport (la cuenta de Render: el aspecto declarado, sin aire)
    const float va = (aspect > 1e-4f) ? aspect : 1.0f;
    const float ra = FrameAspect(va);
    float nx = 1.0f, ny = 1.0f;
    W3dFramingFrame(va, ra, 1.0f, &nx, &ny);
    const int fw = (int)((float)width * nx + 0.5f), fh = (int)((float)height * ny + 0.5f);
    const int fx = x + (width - fw) / 2, fy = y + (height - fh) / 2;
    // negro en todo el viewport: las bandas y la raya entre las mitades
    const int glY = W3dScreenHeight - y - height;
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, width, height);
    w3dEngine::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    // las dos mitades (cada una se dibuja con el Render de siempre, achicado a su rectangulo y con su camara)
    const int x0 = x, y0 = y, w0 = width, h0 = height;
    const GLfloat a0 = aspect;
    Camera* cam0 = CameraActive;
    const bool side = W3dViewsMode() == 0;
    const int stripe = 1;   // (media raya: 2 px entre las dos)
    gSessionView = true;
    for (int v = 0; v < 2; v++) {
        if (side) { width = fw / 2 - stripe; height = fh; x = v ? fx + fw - width : fx; y = fy; }
        else      { width = fw; height = fh / 2 - stripe; x = fx; y = v ? fy + fh - height : fy; }
        aspect = (GLfloat)width / (GLfloat)(height > 0 ? height : 1);
        Object* c = W3dCameraView(v);
        CameraActive = (c && c->getType() == ObjectType::camera) ? (Camera*)c : cam0;
        W3dViewBefore(v);
        // (el "Limpiar" del proyecto, solo en esta mitad)
        g_w3dClipViewOn = true;
        g_w3dClipView[0] = x; g_w3dClipView[1] = W3dScreenHeight - y - height;
        g_w3dClipView[2] = width; g_w3dClipView[3] = height;
        onlyScene = true;
        Render();
        onlyScene = false;
    }
    g_w3dClipViewOn = false;
    gSessionView = false;
    W3dViewAfter();
    x = x0; y = y0; width = w0; height = h0; aspect = a0;
    CameraActive = cam0;
    // el HUD sobre el cuadro entero: el marco de este viewport, como lo dejaria Render
    camFrameOn = true; camFrameNX = nx; camFrameNY = ny;
    w3dEngine::Viewport(x, glY, width, height);
    RenderUI();
    return true;
}

// la INTERFAZ 2D del juego sobre la pantalla del juego (el marco de la camara o el viewport): ver RenderUI. Con el
// estado 2D del viewport ya puesto (State2D y la ortho local 0..width, 0..height)
void Viewport3D::DrawHUDGame() {
        { int arri = barDown ? 0 : BarTopOffset();
          // la toolbar solo resta area si de verdad se dibuja (en modo juego se oculta)
          int abaj = (ToolbarVisible() ? ToolbarHeight() : 0) + (barDown ? BarTopOffset() : 0);
          // 1) la PANTALLA: mirando por la camara es el MARCO del passepartout (el render
          //    encuadrado, aspecto g_renderAspect: el HUD queda ALINEADO con la imagen del
          //    juego y con pantallaDe()); si no, el area util del viewport.
          float sx0 = 0.0f, sy0 = (float)arri;
          float sw = (float)width, sh = (float)(height - arri - abaj);
          if (camFrameOn) {
              float cfx = (camViewPanX * 0.5f + 0.5f) * (float)width;   // misma cuenta que
              float cfy = (0.5f - camViewPanY * 0.5f) * (float)height;  // RenderCamPassepartout
              float chw = camFrameNX * camViewZoom * (float)width  * 0.5f;
              float chh = camFrameNY * camViewZoom * (float)height * 0.5f;
              if (chw > 1.0f && chh > 1.0f) { sx0 = cfx - chw; sy0 = cfy - chh; sw = chw * 2.0f; sh = chh * 2.0f; }
          }
          UI2D_BaseClip(x, y, width, height);   // el overflow recorta con scissor absoluto
          UI* uGame = UI2D_EditorUI();
          bool dynamic = (!uGame || uGame->sameAsRender);
          if (dynamic && AnimIsGame) {
              // 2a) MODO JUEGO con UI "igual que el render": la pantalla ES el lienzo
              //     (override, exactamente la rama sameAsRender de w3drun donde la
              //     ventana real es el lienzo) -> mapeo 1:1, sizeMode 2 escala con
              //     min(w,h)/480 de ESTA pantalla (en apaisado: la ALTURA / 480).
              //     (P5) ESTADO POR VIEWPORT: el override PERSISTENTE -- el que leen
              //     los binds del juego (screen()/pantallaDe) entre frames -- lo
              //     publica SOLO el viewport ACTIVO; el dibujo de ESTE viewport usa
              //     un override CON ALCANCE que al salir repone lo que habia. Antes
              //     cada viewport lo pisaba al dibujar (el ultimo ganaba) y con dos
              //     viewports 3D de tamanos distintos la UI del juego se re-armaba a
              //     cada rato con una pantalla distinta ("se agranda y achica como
              //     loca, independiente por cada viewport3d").
              if (Viewport3DActive == this) UI2D_OverrideWindow(this, sw, sh);
              UI2D_OverrideWindowScope window(this, sw, sh);
              hudX0 = sx0; hudY0 = sy0; hudW = sw; hudH = sh; hudEsc = 1.0f; hudOverride = true;
              { extern float g_hudMarginX, g_hudMarginY, g_hudFrame[4];
                if (Viewport3DActive == this) {
                    g_hudMarginX = g_hudMarginY = 0.0f;
                    g_hudFrame[0] = g_hudFrame[1] = 0.0f; g_hudFrame[2] = sw; g_hudFrame[3] = sh;
                } }
              UI2D_DrawOverlay(sx0, sy0, sw, sh, 1.0f, g_hudCapturePos, true);
              W3dCapturesDoWithHud();   // (capturarPantalla(nombre, true): con el HUD ya dibujado)
          } else {
              // 2b) lienzo con tamano propio, o EDICION (el lienzo sigue siendo el del
              //     render, asi el Editor 2D no cambia): ENCAJARLO (letterbox) con escala
              //     UNIFORME min(sw/lw, sh/lh), la misma cuenta que w3drun / Editor2D.
              UI2D_OverrideWindowRemove(this);
              float lw, lh; UI2D_CanvasSize(&lw, &lh);
              if (lw < 1.0f) lw = 1.0f; if (lh < 1.0f) lh = 1.0f;
              float esc = sw / lw; if (sh / lh < esc) esc = sh / lh;
              if (esc < 0.0001f) esc = 0.0001f;
              float dw = lw * esc, dh = lh * esc;
              float dx0 = sx0 + (sw - dw) * 0.5f, dy0 = sy0 + (sh - dh) * 0.5f;
              hudX0 = dx0; hudY0 = dy0; hudW = dw; hudH = dh; hudEsc = esc; hudOverride = false;
              // (margenPantalla() de lua) cuanto se extiende el viewport mas alla del HUD encajado, en unidades del
              // LIENZO, hasta los bordes del VIEWPORT (no del cuadro de la camara: sus bandas tambien son pantalla): en
              // vertical con el lienzo 4:3, las franjas de arriba y de abajo donde se pega lo que va en los bordes.
              // (marcoPantalla()) el cuadro de la imagen 3D en unidades del lienzo, desde su centro
              { extern float g_hudMarginX, g_hudMarginY, g_hudFrame[4];
                if (Viewport3DActive == this) {
                    // (en el modo juego puro no hay barras del editor: la pantalla es el viewport entero)
                    const float ux0 = 0.0f, uw = (float)width;
                    const float uy0 = g_pureGame ? 0.0f : (float)arri;
                    const float uh = g_pureGame ? (float)height : (float)(height - arri - abaj);
                    g_hudMarginX = (dx0 - ux0) / esc; g_hudMarginY = (dy0 - uy0) / esc;
                    if ((ux0 + uw) - (dx0 + dw) < dx0 - ux0) g_hudMarginX = ((ux0 + uw) - (dx0 + dw)) / esc;
                    if ((uy0 + uh) - (dy0 + dh) < dy0 - uy0) g_hudMarginY = ((uy0 + uh) - (dy0 + dh)) / esc;
                    if (g_hudMarginX < 0.0f) g_hudMarginX = 0.0f;
                    if (g_hudMarginY < 0.0f) g_hudMarginY = 0.0f;
                    g_hudFrame[0] = (sx0 - (dx0 + dw * 0.5f)) / esc; g_hudFrame[1] = (sy0 - (dy0 + dh * 0.5f)) / esc;
                    g_hudFrame[2] = sw / esc; g_hudFrame[3] = sh / esc;
                } }
              UI2D_DrawOverlay(dx0, dy0, dw, dh, esc, g_hudCapturePos, true);
              W3dCapturesDoWithHud();   // (capturarPantalla(nombre, true): con el HUD ya dibujado)
          } }
}

void Viewport3D::RenderUI() {
    // barra de botones 2D del area: NO es un overlay, es chrome del area (como
    // los bordes). Se dibuja SIEMPRE, aunque "Show Overlays" este off (la opcion
    // ShowUi que lo salteaba se dio de baja: dejaba el viewport sin chrome y el
    // estado GL 2D sin resetear). (Usa el viewport ya seteado en Render().)
    {
        State2D();
        // la INTERFAZ 2D del juego/programa, dibujada sobre la PANTALLA DEL JUEGO (no sobre
        // el viewport entero, que era lo que la dejaba "aplastada": estirar el lienzo al
        // rect del viewport escala X e Y con factores distintos y ademas descuadra el HUD
        // del render 3D, que SI se letterboxea al aspecto del render). El mapeo de aca es
        // SIEMPRE uniforme: un elemento de W x H px conserva su proporcion en cualquier
        // resolucion/aspecto y crece con la ALTURA de la pantalla (como la UI de PS1).
        // SOLO MIRANDO POR LA CAMARA ACTIVA, o JUGANDO (el Play del juego, el modo juego puro: un juego 2D no tiene
        // camara y su UI es todo lo que hay): EDITANDO en el espacio 3D libre, la vida, el puntaje y los menus del juego
        // flotando sobre la escena no significan nada.
        const bool showUIGame = camFrameOn || g_pureGame || AnimIsGame;   // (camFrameOn: hay marco de la camara)
        if (!showUIGame) W3dCapturesDoWithHud();   // (las capturas que esperaban el HUD no se quedan colgadas)
        // (el HUD ya se dibujo EN SU LUGAR DEL ARBOL si la UI lo pide: ver UIInTree)
        if (showUIGame && !gHudInTree) DrawHUDGame();
        // MODO JUEGO PURO (VERDE+0): el HUD del juego ya se dibujo; todo lo que
        // sigue es CHROME del editor (barras, botones, toolbar, estadisticas).
        // Un corte temprano = cero draws y cero texto del editor por frame.
        if (g_pureGame) { if (!W3dHasConstructor()) w3dEngine::Invalidate(); return; }
        // el OVERLAY DEL JUEGO pudo dejar CUALQUIER estado 2D: una imagen/video
        // sin canal alpha APAGA Blend (UIOverlay::DrawImageRect), el texto
        // deja mezcla premultiplicada, un elemento con profundidad prende el
        // z-test. La UI del viewport no ASUME nada de eso: AFIRMA su estado
        // completo (mismo criterio que W3dDecalsDrawPending). Era el bug
        // de los botones de pausa con BORDES NEGROS jugando: el HUD del juego
        // terminaba en una imagen opaca, Blend quedaba apagado de verdad (el
        // cache lo sabia, asi que este bloque -- que solo reponia la FUNCION de
        // mezcla -- no lo volvia a prender) y la barra translucida y las
        // esquinas de los botones salian opacas/negras. Con el cache fino,
        // re-afirmar lo que ya estaba puesto cuesta cero llamadas de driver. Tambien el estado de MATERIAL que dejo la
        // escena: un alpha test prendido borraba la barra translucida, la UI de la camara y las filas tenues
        w3dEngine::BaseState();
        w3dEngine::Disable(w3dEngine::DepthTest);
        w3dEngine::Disable(w3dEngine::CullFace);
        w3dEngine::Enable(w3dEngine::Blend);
        w3dEngine::BlendAlpha();               // el texto deja mezcla premultiplicada
        w3dEngine::Enable(w3dEngine::Texture2D);
        w3dEngine::EnableArray(w3dEngine::VertexArray);
        w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        w3dEngine::DisableArray(w3dEngine::NormalArray);
        w3dEngine::DisableArray(w3dEngine::ColorArray);
        w3dEngine::BindTexture(Textures[0]->iID);
        w3dEngine::TexFilter(false);
        w3dEngine::TexFilter(false);
        // durante un transform (mover/rotar/escalar, o ubicar un duplicado) la
        // barra de botones se reemplaza por una barra de estado con los valores
        extern int g_poseMode;
        bool transforming = (Viewport3DActive == this &&
            (((state == translation || state == rotation || state == EditScale) &&
              (InteractionMode == ObjectMode ||
               (InteractionMode == EditMode && EditXformActive()) ||
               (InteractionMode == EditMode && BoneXformMode()) ||   // G/R/S de huesos (Edit de armature)
               (InteractionMode == PoseMode && g_poseMode))) ||
             InsetActive() || BevelActive()));                     // insertar caras / bevel: su valor
        if (transforming) {
            RenderBarTransform();
        } else if (AnimIsGame && GameSimActive()) {
            // ===== MODO JUEGO (sim corriendo): barra MINIMA. Se puede: parar/
            // pausar/reanudar, y los menus View (cambiar camara) / Overlays /
            // Render. Nada de editar. Con STOP la interfaz vuelve a la normal. =====
            static const int kHide[] = { BR_Mode, BR_SelMode, BR_Pivot, BR_Orient, BR_Snap, BR_Proportional,
                                            BR_Select, BR_Add, BR_Mesh, BR_Animation, BR_Object, BR_UV, BR_Root };
            for (size_t i = 0; i < sizeof(kHide) / sizeof(kHide[0]); i++) {
                Button* bo = BarRoleBtn(BarButtons, kHide[i]);
                if (bo) bo->visible = false;
            }
            Button* bs = BarRoleBtn(BarButtons, BR_GameStop);
            Button* bl = BarRoleBtn(BarButtons, BR_GamePlay);
            if (bs) bs->visible = true;
            if (bl) {
                bl->visible = true;
                // UN SOLO boton play/pausa: dice lo que va a HACER el click
                bl->text = PlayAnimation ? T("Pause") : T("Play");
                // verde mientras el juego CORRE (mismo estilo que Snap ON)
                static float playGreen[3]; const float* acc = ColorList[static_cast<int>(ColorID::accent)];
                for (int i = 0; i < 3; i++) playGreen[i] = acc[i] * 0.4f;
                bl->tint = PlayAnimation ? playGreen : NULL;
                bl->textColor = PlayAnimation ? acc : NULL;
            }
            for (size_t i = 0; i < BarButtons.size(); i++)
                if (BarButtons[i]->role == BR_Overlays)
                    BarButtons[i]->icon = showOverlays ? (int)IconType::visible : (int)IconType::hidden;
            RenderBar();
        } else {
            // el boton [1] (modo) para MALLA (Object/Edit/Paint) o ARMATURE (Object/Edit/Pose);
            // muestra el modo actual con su icono.
            { Button* bt;   // sin sim: barra normal; el Play queda si estamos en "Juego"
              if ((bt = BarRoleBtn(BarButtons, BR_GameStop)))  bt->visible = false;
              if ((bt = BarRoleBtn(BarButtons, BR_GamePlay))) {
                  bt->visible = AnimIsGame;
                  bt->text = T("Play");
                  bt->tint = NULL; bt->textColor = NULL;
              } }
            bool isMesh = ActiveObj && ActiveObj->getType() == ObjectType::mesh;
            bool isArm  = ActiveObj && ActiveObj->getType() == ObjectType::armature;
            // la escena/prefab que se edita (en un juego empaquetado no se cambia de escena a mano)
            { Button* bR = BarRoleBtn(BarButtons, BR_Root);
              if (bR) { extern bool g_gameMode; bR->visible = !g_gameMode; W3dRootsButtonSync(bR); } }
            // se buscan por ROL (no por indice) -> reordenar la barra no rompe esto.
            Button* bMode = BarRoleBtn(BarButtons, BR_Mode);
            if (bMode) {
                bMode->visible = isMesh || isArm;
                if (!isMesh && !isArm) InteractionMode = ObjectMode;
                // un armature no tiene Paint: si quedo en un modo de malla, volver a Object
                if (isArm && InteractionMode != EditMode && InteractionMode != PoseMode) InteractionMode = ObjectMode;
                // una malla no tiene Pose: si venia de un armature en Pose, volver a Object
                if (isMesh && InteractionMode == PoseMode) InteractionMode = ObjectMode;
                bMode->text = (InteractionMode == EditMode)     ? T("Edit Mode") :
                              (InteractionMode == PoseMode)     ? T("Pose Mode") :
                              (InteractionMode == VertexPaint)  ? T("Vertex Paint") :
                              (InteractionMode == WeightPaint)  ? T("Weight Paint") :
                              (InteractionMode == TexturePaint) ? T("Texture Paint") : T("Object Mode");
                bMode->icon = (InteractionMode == ObjectMode) ? (int)IconType::object :
                              isArm                            ? (int)IconType::armature : (int)IconType::mesh;
            }
            bool inEdit = (isMesh && InteractionMode == EditMode);
            Button* bSelM = BarRoleBtn(BarButtons, BR_SelMode); // sub-elemento, SOLO en edit (icono)
            if (bSelM) {
                bSelM->visible = inEdit;
                bSelM->icon = (EditSelectMode == SelEdge) ? (int)IconType::selEdge :
                              (EditSelectMode == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
            }
            Button* bUV = BarRoleBtn(BarButtons, BR_UV);       // UV: SOLO en edit
            if (bUV) bUV->visible = inEdit;
            Button* bMesh = BarRoleBtn(BarButtons, BR_Mesh);   // Mesh (Transform/Snap/Delete): SOLO en edit
            if (bMesh) bMesh->visible = inEdit;
            Button* bAdd = BarRoleBtn(BarButtons, BR_Add);     // Add: SOLO en Object Mode
            if (bAdd) bAdd->visible = (InteractionMode == ObjectMode);
            // PINTURA (pesos / color / textura): no se puede cambiar la seleccion ni transformar nada, asi
            // que los menus Select y Object, el snap, la orientacion (Global) y el pivot no tienen sentido
            // en la barra: se ocultan (pedido del dueno).
            const bool paint = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
            { Button* b;
              if ((b = BarRoleBtn(BarButtons, BR_Select))) b->visible = !paint;
              if ((b = BarRoleBtn(BarButtons, BR_Object))) b->visible = !paint; }
            Button* bPiv = BarRoleBtn(BarButtons, BR_Pivot);   // Pivot: solo icono = el modo actual
            if (bPiv) bPiv->visible = !paint;
            if (bPiv) bPiv->icon = (g_transformPivot == PivotCursor3D)   ? (int)IconType::pivotCursor :
                                   (g_transformPivot == PivotIndividual) ? (int)IconType::pivotIndividual :
                                   (g_transformPivot == PivotActive)     ? (int)IconType::pivotActive :
                                                                           (int)IconType::pivotMedian;
            SyncButtonContext();
            // "Animation": keyframes del objeto + Motion Trail. Solo con algo seleccionado, en Object Mode y en
            // Pose Mode (ahi: Insert Keyframe y Reset Pose).
            Button* bAnim = BarRoleBtn(BarButtons, BR_Animation);
            if (bAnim) bAnim->visible = HasObjectsSelected() &&
                ((InteractionMode == ObjectMode) || (InteractionMode == PoseMode) ||
                 (InteractionMode == EditMode && ActiveAnimKind == 3)); // editando una vertex anim: Insert Keyframe a mano
            Button* bOri = BarRoleBtn(BarButtons, BR_Orient);  // muestra la orientacion actual
            if (bOri) bOri->visible = !paint;
            if (bOri) bOri->text = T((transformOrientation == LocalOrient)  ? "Local" :
                                     (transformOrientation == ViewOrient)   ? "View"  :
                                     (transformOrientation == NormalOrient) ? "Normal" : "Global");
            // SNAP: el boton se ilumina VERDE cuando esta ON (para saber que el imantado esta activo)
            Button* bSnap = BarRoleBtn(BarButtons, BR_Snap);
            if (bSnap) bSnap->visible = !paint;
            if (bSnap){
                static float snapGreen[3]; const float* acc = ColorList[static_cast<int>(ColorID::accent)];
                for (int i=0;i<3;i++) snapGreen[i]=acc[i]*0.4f;
                bSnap->tint = g_snap.enabled ? snapGreen : NULL;
                bSnap->textColor = g_snap.enabled ? acc : NULL;
            }
            // PINTURA DE TEXTURA: el diskette (la textura pintada vive en RAM hasta guardarla); con "*" pendiente, verde
            { Button* bTG = BarRoleBtn(BarButtons, BR_TexSave);
              if (bTG) {
                  bTG->visible = (InteractionMode == TexturePaint);
                  Mesh* mT = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
                  int pT = -1; Texture* tT = mT ? WP3DActiveTexture(mT, pT) : NULL;
                  const bool pend = tT && TexEditUnsaved(tT);
                  static float tgGreen[3]; const float* accT = ColorList[static_cast<int>(ColorID::accent)];
                  for (int i=0;i<3;i++) tgGreen[i]=accT[i]*0.4f;
                  bTG->tint = pend ? tgGreen : NULL;
                  bTG->textColor = pend ? accT : NULL;
              } }
            // PROPORTIONAL EDITING: el icono de la curva de caida elegida, VERDE cuando esta ON (tecla O)
            Button* bProp = BarRoleBtn(BarButtons, BR_Proportional);
            if (bProp){
                bProp->visible = !paint && (InteractionMode == EditMode || InteractionMode == ObjectMode);
                bProp->icon = ProportionalIconType(g_prop.type);
                static float propGreen[3]; const float* accP = ColorList[static_cast<int>(ColorID::accent)];
                for (int i=0;i<3;i++) propGreen[i]=accP[i]*0.4f;
                bProp->tint = g_prop.on ? propGreen : NULL;
                bProp->textColor = g_prop.on ? accP : NULL;
            }
            // el OJO del overlay dice si estan prendidos: abierto = se ven, cerrado = apagados. Va aca (por
            // frame) y no al construirlo, porque el estado cambia.
            for (size_t i = 0; i < BarButtons.size(); i++)
                if (BarButtons[i]->role == BR_Overlays)
                    BarButtons[i]->icon = showOverlays ? (int)IconType::visible : (int)IconType::hidden;
            RenderBar();
        }
        // barra de HERRAMIENTAS (abajo): historial + orientacion + ejes + aceptar/cancelar tactil.
        // Se dibuja TAMBIEN durante un transform (ahi viven el tilde/cruz y los ejes X/Y/Z).
        RenderToolbar();
        // estadisticas/fps (texto blanco arriba a la derecha; misma ortho 2D)
        RenderStats();
        // el CANDADO de la vista de camara (abajo al centro)
        RenderLockCamera();
        // "Ray Tracing: 3/16" .. "done" (arriba a la izquierda, misma ortho 2D)
        // (en la VISTA LOCAL el cartel lo dice: "Local View", o adelante del del trazado)
        if (!(view == RenderType::Rendered && RTRenderState(this)) && localViewActive) W3dDialogView(this, T("Local View"));
    }

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    w3dEngine::Viewport(x, W3dScreenHeight - y - height, width, height);
    w3dEngine::Ortho(0, width, height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::ColorMaterial);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::BlendAlpha();

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BindTexture(Textures[0]->iID);
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    DrawBorders(this);
}

// float -> texto portable (sin %f, que no anda en Symbian). El formateo vive en el
// compartido TransformUI.cpp (W3dFmtFloat): tambien lo usan el UV editor y el Editor 2D.
static std::string W3dFmtF(float v, int dec){ return W3dFmtFloat(v, dec); }

// busca el estado guardado (pos/rot/scale al iniciar el transform) del objeto
static SaveState* W3dSavedState(Object* o){
    for (size_t i = 0; i < objectState.size(); i++)
        if (objectState[i].obj == o) return &objectState[i];
    return NULL;
}

// el texto de la barra de estado: operacion + valores en vivo del objeto activo
// (mismo mapeo de ejes que el panel Properties: Location Y=z, Z=y; Scale x/y/z)
static const char* W3dOrientWord(){
    if (transformOrientation == LocalOrient) return T("local");
    if (transformOrientation == ViewOrient)  return T("view");
    return T("global");
}

// arma el texto: un solo eje muestra SOLO ese eje ("X: .. along global X"); un
// plano muestra los dos del plano; libre muestra los tres (sin sufijo).
static std::string W3dLineTransform(const char* title, float vx, float vy, float vz){
    int a = axisSelect;
    std::string ori = W3dOrientWord();
    if (a == X || a == Y || a == Z){
        const char* L = (a == X) ? "X" : (a == Y) ? "Y" : "Z";
        float v = (a == X) ? vx : (a == Y) ? vy : vz;
        return std::string(title) + " " + L + ": " + W3dFmtF(v, 4) + "  " + T("along") + " " + ori + " " + L;
    }
    if (a == PlaneX || a == PlaneY || a == PlaneZ){
        std::string s = title, axes;
        if (a != PlaneX){ s += "  X: " + W3dFmtF(vx, 4); axes += "X"; }
        if (a != PlaneY){ s += "  Y: " + W3dFmtF(vy, 4); if (!axes.empty()) axes += "-"; axes += "Y"; }
        if (a != PlaneZ){ s += "  Z: " + W3dFmtF(vz, 4); if (!axes.empty()) axes += "-"; axes += "Z"; }
        return s + "  " + T("along") + " " + ori + " " + axes;
    }
    // libre (3 ejes)
    return std::string(title) + "  X: " + W3dFmtF(vx, 4) +
                         "  Y: " + W3dFmtF(vy, 4) + "  Z: " + W3dFmtF(vz, 4);
}

// sufijo "  along <orient> <ejes>" para un valor numerico, segun axisSelect
static std::string W3dSuffixAxes(){
    int a = axisSelect;
    std::string ori = W3dOrientWord();
    if (a==X||a==Y||a==Z) return std::string("  ") + T("along") + " " + ori + " " + ((a==X)?"X":(a==Y)?"Y":"Z");
    if (a==PlaneX) return std::string("  ") + T("along") + " " + ori + " Y-Z";
    if (a==PlaneY) return std::string("  ") + T("along") + " " + ori + " X-Z";
    if (a==PlaneZ) return std::string("  ") + T("along") + " " + ori + " X-Y";
    return ""; // libre
}

static std::string W3dTextTransform(){
    // INSERTAR CARAS / BEVEL: su valor (o lo tipeado). Tocar la barra abre el teclado numerico para escribirlo exacto
    if (InsetActive() || BevelActive()){
        const char* op = InsetActive() ? T("Inset Faces") : T("Bevel");
        if (NumInputActive()) return std::string(op) + ": " + TransformUITextNum("");
        if (BevelActive()){
            const BevelOptions& b = BevelCurrentOptions();
            char seg[16]; snprintf(seg, sizeof(seg), "%d", b.segments);
            return std::string(op) + ": " + T("Width") + " " + W3dFmtF(b.width, 3) + "  " + T("Segments") + " " + seg;
        }
        const InsetOptions& o = InsetCurrentOptions();
        std::string s = std::string(op) + ": " + T("Thickness") + " " + W3dFmtF(o.thickness, 3);
        if (o.depth != 0.0f || InsetDepthDrag()) s += std::string("  ") + T("Depth") + " " + W3dFmtF(o.depth, 3);
        return s;
    }
    // ENTRADA NUMERICA: muestra la EXPRESION tipeada + su resultado (
    // "Move: [(2*3)+3] = 9  along global X"). Vale para objetos y malla. El armado
    // "[expr|] = valor" es el compartido de TransformUI (mismo formato en UV / 2D).
    if (NumInputActive()){
        const char* op = (state==rotation) ? T("Rotate") : (state==EditScale) ? (EditShrinkActive() ? T("Shrink/Fatten") : T("Scale")) : T("Move");
        const char* unit = (state==rotation) ? "\xC2\xB0" : "";
        std::string suf = (state==rotation && (axisSelect==ViewAxis||axisSelect==XYZ||axisSelect==OrbitalAxis)) ? "" : W3dSuffixAxes();
        return std::string(op) + ": " + TransformUITextNum(unit) + suf;
    }
    // Edit Mode: los valores son los del TRANSFORM DE MALLA (los vertices), no del
    // objeto (que no se mueve). Mismos labels de eje/orientacion. La rotacion usa
    // gAngleTransform igual que objetos, asi que cae al codigo de abajo.
    const bool edit = (InteractionMode == EditMode && EditXformActive());
    if (edit){
        if (state == translation){
            Vector3 d = EditXformTransDelta(); // mundo (engine xyz) -> user X/Y/Z (swap z/y)
            return W3dLineTransform(T("Translate"), d.x, d.z, d.y);
        }
        if (state == EditScale){
            if (EditShrinkActive()) return std::string(T("Shrink/Fatten")) + ": " + W3dFmtF(EditXformShrinkAmt(), 4);
            float f = EditXformScaleFactor();
            return W3dLineTransform(T("Scale"), f, f, f);
        }
        // rotacion: cae al bloque comun (gAngleTransform + axisSelect/orientacion)
    }
    // EDIT de ARMATURE (G/R/S de huesos): translate/scale muestran el valor del transform de puntas.
    // Rotate cae al bloque comun (usa gAngleTransform, que BoneXformApply setea).
    if (InteractionMode == EditMode && BoneXformMode()){
        int bm = BoneXformMode();
        if (bm == 1) return std::string(T("Translate")) + ": " + W3dFmtF(BoneXformHeaderValue(), 4) + W3dSuffixAxes();
        if (bm == 3) return std::string(T("Scale")) + ": " + W3dFmtF(BoneXformHeaderValue(), 3);
    }
    // POSE Mode: translate/scale muestran el valor de la POSE (no el del objeto armature). Rotate cae al bloque comun
    // (usa gAngleTransform, que el transform de pose setea). El numerico ya lo cubre el bloque de arriba.
    { extern int PoseHeaderMode(); extern float PoseHeaderValue();
      if (InteractionMode == PoseMode && PoseHeaderMode()){
        int pm = PoseHeaderMode();
        if (pm == 1) return std::string(T("Translate")) + ": " + W3dFmtF(PoseHeaderValue(), 4) + W3dSuffixAxes();
        if (pm == 3) return std::string(T("Scale")) + ": " + W3dFmtF(PoseHeaderValue(), 3);
      } }
    if (!ActiveObj) return "";
    SaveState* st = W3dSavedState(ActiveObj);
    if (state == translation){
        Vector3 p = ActiveObj->pos;
        Vector3 p0 = st ? st->pos : p;
        // X/Y/Z del usuario = engine x / z / y (swap del modelo)
        return W3dLineTransform(T("Translate"), p.x - p0.x, p.z - p0.z, p.y - p0.y);
    }
    if (state == EditScale){
        Vector3 s = ActiveObj->scale;
        Vector3 s0 = st ? st->scale : Vector3(1,1,1);
        float fx = (s0.x != 0.0f) ? s.x / s0.x : s.x;
        float fy = (s0.y != 0.0f) ? s.y / s0.y : s.y;
        float fz = (s0.z != 0.0f) ? s.z / s0.z : s.z;
        return W3dLineTransform(T("Scale"), fx, fz, fy); // Y=scale.z, Z=scale.y
    }
    if (state == rotation){
        std::string r = std::string(T("Rotate")) + "  " + W3dFmtF(gAngleTransform, 1) + "\xC2\xB0";
        int a = axisSelect;
        if (a == X || a == Y || a == Z){
            const char* L = (a == X) ? "X" : (a == Y) ? "Y" : "Z";
            r += std::string("  ") + T("along") + " " + W3dOrientWord() + " " + L;
        }
        else if (a == OrbitalAxis) r += std::string("  ") + T("Orbital");
        return r;
    }
    return "";
}

// linea PUNTEADA 2D (coords de pantalla del viewport, Y abajo) entre dos puntos.
// (la usa tambien el timeline: la misma linea pivote -> mouse al rotar / escalar keyframes)
void DrawDashedLine2D(float ax, float ay, float bx, float by){
    float dx = bx - ax, dy = by - ay;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 2.0f) return;
    float ux = dx / len, uy = dy / len;
    const float dash = 6.0f, gap = 5.0f, step = dash + gap;
    GLfloat v[4 * 128]; // hasta 128 guiones
    int n = 0;
    for (float t = 0.0f; t < len && n < 128; t += step){
        float t2 = t + dash; if (t2 > len) t2 = len;
        v[n*4+0] = ax + ux*t;  v[n*4+1] = ay + uy*t;
        v[n*4+2] = ax + ux*t2; v[n*4+3] = ay + uy*t2;
        n++;
    }
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    // verde como el acento de la UI
    SetColorID(ColorID::accent, 0.9f);
    w3dEngine::LineWidth(1);
    w3dEngine::VertexPointer2f(0, v);
    w3dEngine::DrawLines(n * 2);
    // RESTAURAR textura: si no, el fondo/texto de la barra (que sigue) salen
    // como bloques solidos sin textura ni alpha.
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
}

void Viewport3D::RenderBarTransform(){
    if (!barCard) return;

    // linea punteada VERDE del pivot al mouse: en ROTAR y ESCALAR (no translate). La barra es chrome
    // (se ve aunque Show Overlays este off), pero la linea punteada SI es overlay del transform: con
    // overlays off NO se dibuja (igual que los ejes de constraint, que viven en RenderOverlay).
    if (showOverlays && (state == rotation || state == EditScale) && gLineValid){
        DrawDashedLine2D(gTrackPivX, gTrackPivY, gTrackMouseX, gTrackMouseY);
    }

    // fondo + texto: el dibujo COMPARTIDO de la barra de info (TransformUI.cpp), el
    // mismo que usan el UV editor y el Editor 2D durante sus transforms.
    RenderBarInfo(W3dTextTransform());
}

// TACTIL: un tap sobre la barra de estado del transform (la que dice "Move: ... = ...")
// abre el teclado numerico en modo transform para editar el valor EXACTO como en PC.
// Solo cuando hay un transform en curso y este es el viewport activo.
bool Viewport3D::ClickBarTransform(int mx, int my){
    if (Viewport3DActive != this) return false;
    const bool modalMesh = InsetActive() || BevelActive();   // insertar caras / bevel: el grosor / el ancho exacto
    if (!modalMesh && !(state == translation || state == rotation || state == EditScale)) return false;
    if (!modalMesh && !(InteractionMode == ObjectMode ||
          (InteractionMode == EditMode && EditXformActive()) ||
          (InteractionMode == EditMode && BoneXformMode()))) return false; // G/R/S de huesos: valor exacto tactil
    int barH = BarHeight();
    int yBar = barDown ? (y + height - barH) : y; // misma franja que RenderBarTransform
    if (mx < x || mx >= x + width || my < yBar || my >= yBar + barH) return false;
    NumPadOpenTransform();
    return true;
}

// El overlay de estadisticas (arriba-derecha: vertices/caras/modgen/ms/fps) vive en ViewPort3D_Stats.cpp
// junto a su helper W3dCountMeshes. Solo lee globales ya expuestos (profiler, contadores de OpcionesRender).

void Viewport3D::ChangePerspective(){
    orthographic = !orthographic;
}

// SYMBIAN: el camino de PC del viewport 3D (aceptar, click, soltar, arrastre) corre solo con el DEDO
// (W3D_ONLY_BY_CONTROLS, app/W3dTouchEditor.h). El cursor virtual, el mouse BT y el keypad del N95 siguen por el
// suyo (w3dnewscene / HID), como antes.
void Viewport3D::Accept() {
    W3D_ONLY_BY_CONTROLS();

    // Mostrar el cursor
    #if !defined(W3D_SYMBIAN) && SDL_MAJOR_VERSION == 2
        SDL_ShowCursor(SDL_ENABLE);
    #elif !defined(W3D_SYMBIAN) && SDL_MAJOR_VERSION == 3
        SDL_ShowCursor();
    #endif
    //si no hay objetos (liberando el foco: los otros returns de Aceptar tambien lo hacen)
    if (SceneCollection->Childrens.empty()){ ViewPortClickDown = false; return; }

    // EDIT de ARMATURE: el tilde verde / Enter confirman el transform de huesos en curso
    if (BoneGrabActive()){
        BoneGrabConfirm();
        NumInputReset();
        ViewPortClickDown = false;
        return;
    }
    // AJUSTAR LA ULTIMA OPERACION: lo que se hizo se lee ANTES de confirmar (hace falta el snapshot) y el panel se abre
    // DESPUES (cuando ya esta su paso de undo)
    TransformRedoCapture(this);
    if ( InteractionMode == ObjectMode ){
        if (state != editNavigation){
            // AUTO KEY: va ANTES de UndoTransformConfirm, que es lo que puede limpiar objectState (el
            // snapshot contra el que se mide QUE canal cambio). Y antes tambien para que los keyframes que
            // guarda entren en el MISMO comando de undo que el transform.
            UndoGroupStart();       // un solo Ctrl+Z: el transform Y los keyframes que dejo el auto key
            UndoKeyframesStart(); AutoKeyObjects(); UndoKeyframesConfirm();
            UndoTransformConfirm(); // Ctrl+Z: el transform se ACEPTO -> pushea el undo pendiente
            UndoGroupEnd();
            state = editNavigation;
        }
    } else if (InteractionMode == EditMode && EditXformActive()){
        EditXformConfirm(); // fija el transform de malla (recalcula bordes+normales)
    } else if (InteractionMode == PoseMode){
        extern int g_poseMode; extern void PoseXformConfirm();
        if (g_poseMode) PoseXformConfirm(); // fija la pose editada (Enter / tick del toolbar); idempotente con el click
    }
    NumInputReset(); // termina el transform -> limpia el valor numerico tipeado
    ViewPortClickDown = false;
    TransformRedoAfterConfirm();
}

void Viewport3D::button_left(){
    W3D_ONLY_BY_CONTROLS();

    // EDIT de ARMATURE: un grab MODAL de huesos en curso (E/G/toolbar) se CONFIRMA con el click,
    // como cualquier transform. Se marca el click como consumido para que el ScenePick3D del mismo
    // evento no re-pickee un hueso.
    if (BoneGrabActive() && !BoneGrabByClick()){
        BoneGrabConfirm();
        BoneGrabMarkConsumedClick();
        return;
    }
    if (state == translation || state == EditScale || state == rotation){
        Accept();
    }
    // WEIGHT PAINT: el click sobre el contenido PINTA (arranca el TRAZO: snapshot de undo +
    // primera pasada del pincel). El drag sigue en event_mouse_motion; el commit al soltar.
    else if (WP3DPaintMode() && !PopUpActive){
        Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        SaveMousePos();
        if (m && g_lastDownTouch) {
            // dedo: el trazo queda PENDIENTE hasta que se mueva, pase la espera o se suelte (ver arriba)
            g_wpPend = true; g_wpPendX = lastMouseX; g_wpPendY = lastMouseY; g_wpPendTicks = WP3DTicksMs();
            g_redraw = true;
        } else if (m && WP3DStrokeStart(m)){
            g_wp3dPainting = true;
            WP3DPass(this, lastMouseX, lastMouseY);
        }
    }
    else {
        SaveMousePos();
    }
}

void Viewport3D::mouse_button_up(int button){
    W3D_ONLY_BY_CONTROLS();
    if (GizmoDragging()) { GizmoRelease(this); ViewPortClickDown = false; return; }   // soltar la manija = confirmar
    if (g_wpPend) WP3DPendingResolve(this, g_wpPendX, g_wpPendY, true);   // tap sin mover: UNA pasada ahi
    if (g_wp3dPainting){ g_wp3dPainting = false; WP3DStrokeEnd(); } // fin del trazo -> commit del undo
    // EDIT de ARMATURE: el drag de head/tail arrancado por el click termina al SOLTAR (commit del undo)
    if (BoneGrabActive() && BoneGrabByClick()) BoneGrabConfirm();
    ViewPortClickDown = false;
}

extern bool g_altNavActive;   // controls.cpp: arrastre izquierdo con Alt = navegar (orbitar / con Shift desplazar)
void Viewport3D::event_mouse_motion(int mx, int my){
    // el viewport 3D bajo el mouse pasa a ser el ACTIVO (multi-viewport)
    Viewport3DActive = this;
    // POSE MODE: si hay un transform de huesos en curso (G/R/S), el arrastre lo aplica a la pose y NO orbita/transforma
    // objetos. Usa mx/my (delta con signo real), NO las globales dx/dy que no se actualizan durante el transform de hueso.
    { extern int g_poseMode; if (g_poseMode){ extern void PoseXformMotion(int,int); PoseXformMotion(mx, my); return; } }

    W3D_ONLY_BY_CONTROLS();

    // MODO PINTURA: el circulo del pincel sigue al mouse -> guardar la posicion VIVA del
    // cursor (lastMouseX/Y solo se refrescan al clickear: el circulo quedaba congelado
    // hoverando) y redibujar en cada motion. Con un trazo en curso (mouse apretado), cada
    // motion PINTA otra pasada y consume el evento (asi el izquierdo no orbita en
    // Android/web mientras se pinta).
    if (WP3DPaintMode()) { g_wpCursorX = mx; g_wpCursorY = my; g_redraw = true; }
    if (g_wpPend){
        if (!leftMouseDown) { g_wpPend = false; }                  // up perdido
        else { WP3DPendingResolve(this, mx, my, false); if (g_wpPend) return; }   // sigue esperando: ni pinta ni orbita
    }
    if (g_wp3dPainting){
        if (leftMouseDown){ WP3DPass(this, mx, my); return; }
        g_wp3dPainting = false; WP3DStrokeEnd(); // up perdido (solto fuera del viewport): commit igual
    }
    // EDIT de ARMATURE: drag de head/tail (o grab modal de E/G) en curso -> el arrastre mueve los extremos
    if (BoneGrabActive()){
        if (BoneGrabByClick() && !leftMouseDown){ BoneGrabConfirm(); } // up perdido: commit igual
        else { BoneGrabMotion3D(this, mx, my); return; }
    }

    // si no estamos rotando, se re-captura el angulo del trackball al volver.
    // la linea punteada solo vale en rotar/escalar (no translate ni navegacion).
    if (state != rotation) gTrackballCap = false;
    if (state != rotation && state != EditScale) gLineValid = false;

    //boton del medio del mouse (en Android/WebGL tambien el IZQUIERDO: el touch sintetiza click izq ->
    // 1 dedo arrastrado tiene que orbitar. El tap-vs-drag distingue seleccionar de orbitar.
    // OJO: durante un TRANSFORM el izquierdo NO orbita: el dedo apretado esta MOVIENDO el transform
    // (en tactil se confirma con el tilde de la barra de herramientas, no soltando).
    // modo VIEW (toggle de Edit Mode): con 1 dedo se ORBITA aunque haya una operacion en curso (mover/rotar/
    // extrude/strip) -> se puede mirar desde otro angulo sin cancelar; se apaga View y sigue editando.
    // (la 3DS y la Vita tambien: su "mouse" es siempre el dedo de la pantalla tactil. Y la PSP, que no tiene: su cursor
    // es el control, con la interfaz tactil, ver platform/psp/w3d_psp.cpp)
    // En las demas (PC, Symbian) decide el DEDO, no la plataforma: una pantalla tactil orbita igual que Android; el
    // mouse (el de la PC, el Bluetooth del telefono) sigue con el boton del medio / Alt y arrastrar es la caja.
    #if defined(__ANDROID__) || defined(__EMSCRIPTEN__) || defined(W3D_3DS) || defined(W3D_VITA) || defined(W3D_PSP)
        const bool leftOrbit = true;
    #else
        const bool leftOrbit = g_lastDownTouch;
    #endif
        bool viewOrbit = (g_viewEditMode && (InteractionMode == EditMode || WP3DPaintMode()));   // pintura: vista ON = el dedo orbita
        // Alt + arrastre izquierdo = lo mismo que el boton del medio (controls.cpp decide click / arrastre)
        if (middleMouseDown || (leftMouseDown && (g_altNavActive || (leftOrbit && (state == editNavigation || viewOrbit))))) {
        ViewPortClickDown = true;
        // Chequear si Shift está presionado
        #if defined(W3D_SYMBIAN)
            bool shiftHeld = LShiftPressed;   // (el lapiz / shift del teclado del telefono)
        #else
        #if SDL_MAJOR_VERSION == 2
            const Uint8* mouseState = SDL_GetKeyboardState(NULL);
        #elif SDL_MAJOR_VERSION == 3
            const bool* state = SDL_GetKeyboardState(NULL);
        #endif
        bool shiftHeld = mouseState[SDL_SCANCODE_LSHIFT] || mouseState[SDL_SCANCODE_RSHIFT] || (SDL_GetModState() & KMOD_SHIFT) != 0;
        #endif

        if (shiftHeld) {
            /*float radY = rotY * M_PI / 180.0f; // Yaw
            float radX = rotX * M_PI / 180.0f; // Pitch

            float factor = 0.01f;

            float cosX = cos(radX);
            float sinX = sin(radX);
            float cosY = cos(radY);
            float sinY = sin(radY);

            PivotZ -= dy * factor * cosY;
            PivotX += dx * factor * cosX - dy * factor * sinY * sinX;
            PivotY += dx * factor * sinX + dy * factor * sinY * cosX;*/
            Pan();
        #if !defined(W3D_SYMBIAN)
            LShiftPressed = false;
        #endif   // (en Symbian LShiftPressed ES el shift del teclado BT: apagarlo cortaba el paneo al primer movimiento)

        }
        else {
            RotateOrbit();
        }
    }
    else if (state == translation || state == rotation || state == EditScale){
        // PRIMER motion de un transform nuevo: el delta dx/dy todavia es el del frame
        // anterior (CheckWarpMouseInViewport lo recalcula DESPUES de esto) -> lo ignoro
        // para que el transform arranque en CERO y no pegue un salto.
        // El flag se consume SOLO con un delta REAL (dx/dy != 0). Una motion ESPURIA de delta 0
        // (pasa a veces tras el click del menu de extrude) NO debe limpiarlo, sino el primer move
        // de verdad (que arrastra el delta viejo del menu al viewport) salta. (bug intermitente).
        const bool newScale = g_xformFirstMov;   // (la escala toma aca su distancia de referencia al pivote)
        if (g_xformFirstMov) {
            if (dx != 0 || dy != 0) g_xformFirstMov = false;
            dx = 0; dy = 0; // este motion arranca SIEMPRE en cero
        }
        g_snapCurX = mx; g_snapCurY = my; // cursor para el SNAP (busca el target bajo el mouse)
        // si el usuario esta tipeando un valor EXACTO, el mouse NO pisa el transform
        if (NumInputActive()) { UpdateLineTransform(mx, my); return; }
        // Ocultar el cursor
        //SDL_HideCursor();
        // Edit Mode: el transform actua sobre los VERTICES seleccionados (no el
        // objeto). Mismo eje/orientacion/pivot, otro target (modulo en LayoutInput).
        const bool edit = (InteractionMode == EditMode && EditXformActive());
        switch (state) {
            case translation:
                // GIZMO: el punto agarrado sigue al puntero (absoluto, por rayo), no el arrastre por velocidad
                if (GizmoDragging() && GizmoMotion(this, mx, my)) break;
                // el slide (G-G / Shift+V) reusa el estado del move: mismo modal, otro recorrido
                if (edit && EditSlideActive()) EditSlideMouse(dx, dy);
                else if (edit) EditXformTranslation(dx, dy, DragVelocityWorld());
                else      SetTranslationObjects(dx, dy, DragVelocityWorld());
                break;
            case rotation:
                // GIZMO de rotar: el semicirculo de un eje gira por el angulo alrededor del centro (absoluto)
                if (GizmoDragging() && GizmoMotion(this, mx, my)) break;
                if (axisSelect == ViewAxis) {
                    RotateFromView(mx, my); // trackball (ramifica a edit adentro)
                } else if (axisSelect == OrbitalAxis) {
                    gTrackballCap = false;   // orbital: izq/der=camUp, arr/ab=camRight
                    if (edit) EditXformRotOrbital(dx, dy);
                    else      RotateOrbital(dx, dy);
                } else {
                    gTrackballCap = false;   // constreñido a un eje: incremental
                    if (edit) EditXformRotAxis(dx, dy);
                    else      SetRotation(dx, dy);
                }
                break;
            case EditScale:
                // GIZMO de escalar: la manija agarrada sigue al puntero (proporcion de distancias desde el centro)
                if (GizmoDragging() && GizmoMotion(this, mx, my)) break;
                // Shrink/Fatten (edit): la distancia por la normal es en MUNDO -> velocidad tipo "mover"
                // para que se sienta pegado al dedo/mouse. Escala normal: factor chico de siempre.
                if (edit && EditShrinkActive()) { EditXformScale(dx, dy, DragVelocityWorld()); break; }
                // ESCALA por la DISTANCIA AL PIVOTE EN PANTALLA (como Blender): factor = distancia del puntero al
                // pivote / la de cuando empezo. Es proporcional a lo que se ve: alejado con el zoom (el objeto chico),
                // poco movimiento escala mucho; antes era un paso fijo por pixel y de lejos no avanzaba. El puntero
                // se lleva VIRTUAL (inicio + deltas): el salto del cursor en el borde del viewport no lo rompe
                {
                    static float sVx = 0, sVy = 0, sPx = 0, sPy = 0, sD0 = 0;
                    static bool sOk = false;
                    if (newScale) {
                        sOk = false;
                        const Vector3 piv = edit ? EditXformPivotWorld() : TransformPivotPoint;
                        float px, py;
                        if (ProjectPoint(piv, px, py)) {
                            sPx = px; sPy = py; sVx = (float)mx; sVy = (float)my;
                            sD0 = sqrtf((sVx - sPx) * (sVx - sPx) + (sVy - sPy) * (sVy - sPy));
                            sOk = sD0 > 8.0f;   // (encima del pivote no hay distancia de referencia: paso fijo)
                        }
                    }
                    if (sOk) {
                        sVx += (float)dx; sVy += (float)dy;
                        const float f = sqrtf((sVx - sPx) * (sVx - sPx) + (sVy - sPy) * (sVy - sPy)) / sD0;
                        if (edit) EditXformScaleSet(f);
                        else      SetScaleFactor(f);
                        break;
                    }
                }
                if (edit) EditXformScale(dx, dy, 0.001f);
                else      SetScale(dx, dy, 0.001f);
                break;
            default:
                // por si no coincide con nada
                break;
        }
        // linea punteada VERDE del pivot al mouse (rotar/escalar, no translate)
        UpdateLineTransform(mx, my);
    }
}


// LAS FLECHAS DE LA VISTA 3D, iguales en la PC y en el telefono (el keypad del N95 llega por W3dNewOrbit /
// W3dNewTransformMove; un teclado -el de la PC, uno Bluetooth en Symbian- llega aca):
//  - navegando: ORBITAN (el cuadro anterior / siguiente es del Timeline: sus izq/der, y arr/ab saltan de keyframe)
//  - con un transform y el IMAN prendido, moviendo: SALTAN al siguiente nivel de la geometria (SnapStepArrow); sin nada
//    mas adelante, se quedan donde estan
//  - si no: mueven / rotan / escalan de a un paso; la malla en Edit Mode (el objeto sigue en cada flecha)
// true = la flecha ya se uso
bool Viewport3D::ArrowCommon(int fx, int fy){
    if (BoneGrabActive()) return true;   // (transform de huesos: las flechas no tocan los objetos)
    if (state == editNavigation){ OrbitArrow(fx * 8, fy * 8); g_redraw = true; return true; }
    if (state != translation && state != rotation && state != EditScale) return false;
    if (SnapByArrows()){ SnapStepArrow(fx, fy); return true; }
    if (InteractionMode == EditMode && EditXformActive()){
        const bool positive = (fx > 0 || fy < 0);   // derecha / arriba
        if (state == translation) EditXformTranslation(fx * 5, fy * 5, DragVelocityWorld());
        else if (state == rotation){
            if (axisSelect == OrbitalAxis) EditXformRotOrbital(fx * 20, fy * 20);
            else EditXformRotAxis(positive ? -20 : 20, 0);   // (~2 grados por toque, como el objeto)
        }
        else EditXformScale(positive ? 2 : -2, 0, 0.001f);
        g_redraw = true;
        return true;
    }
    return false;
}
void Viewport3D::KeyRight(){
    if (ArrowCommon(1, 0)) return;
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else
    // transform de HUESOS en curso: las flechas no deben tocar los OBJETOS (comparten `estado`)
    if (BoneGrabActive()) return;

    //mueve el mouse
    /*if (mouseVisible){
        mouseX++;
        if (mouseX > iScreenWidth-11){mouseX = iScreenWidth-11;};
    }*/

    //rotX -= fixedMul( 1, aDeltaTimeSecs );
    if (state == editNavigation){
        if (navigationMode == Orbit){
            if (CameraActive && ViewFromCameraActive && CameraToView){
                Object& obj = *CameraActive;
                // Convertir el angulo de rotX a radianes
                /*GLfloat radRotX = obj.rotX * M_PI / 180.0;

                obj.posX-= 30 * cos(radRotX);
                obj.posY+= 30 * sin(radRotX);*/
            }
            else {
                if (ViewFromCameraActive){
                    SetViewFromCameraActive(false);
                }
                //rotX+= 0.5;
            }
        }
        else if (navigationMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            // Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
            GLfloat leftX = cos(radRotX);
            GLfloat leftY = sin(radRotX);*/

            // Mover hacia la izquierda
            //PivotX -= 30 * leftX;
            //PivotY -= 30 * leftY;
        }
    }
    else if (state == translation){
        SetTranslationObjects(5, 0, DragVelocityWorld());
    }
    else if (state == rotation){
        if (axisSelect == OrbitalAxis) RotateOrbital(20, 0); // orbital: yaw (camUp)
        else SetRotation(-20, 0); // ~2 grados por toque
    }
    else if (state == EditScale){
        SetScale(2,0);
    }
    else if (state == timelineMove){
        CurrentFrame++;
        if (!PlayAnimation){
        }
    }
}
#endif

void Viewport3D::KeyLeft(){
    if (ArrowCommon(-1, 0)) return;
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else
    // transform de HUESOS en curso: las flechas no deben tocar los OBJETOS (comparten `estado`)
    if (BoneGrabActive()) return;

    //mueve el mouse
    if (mouseVisible){
        mouseX--;
        if (mouseX < 0){mouseX = 0;};
    }

    //rotX += fixedMul( 0.1, aDeltaTimeSecs );
    if (state == editNavigation){
        if (navigationMode == Orbit){
            if (CameraActive && ViewFromCameraActive && CameraToView){
                Object& obj = *CameraActive;
                // Convertir el angulo de rotX a radianes
                /*GLfloat radRotX = obj.rotX * M_PI / 180.0;

                obj.posX+= 30 * cos(radRotX);
                obj.posY-= 30 * sin(radRotX);*/
            }
            else {
                if (ViewFromCameraActive){
                    SetViewFromCameraActive(false);
                }
                //rotX-= 0.5;
            }
        }
        else if (navigationMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            // Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
            GLfloat leftX = cos(radRotX);
            GLfloat leftY = sin(radRotX);*/

            // Mover hacia la izquierda
            //PivotX += 30 * leftX;
            //PivotY += 30 * leftY;
        }
    }
    else if (state == translation){
        SetTranslationObjects(-5, 0, DragVelocityWorld());
    }
    else if (state == rotation){
        if (axisSelect == OrbitalAxis) RotateOrbital(-20, 0); // orbital: yaw (camUp)
        else SetRotation(20, 0); // ~2 grados por toque
    }
    else if (state == EditScale){
        SetScale(-2,0);
    }
    else if (state == timelineMove){
        CurrentFrame--;
        // wrap del scrub: al pasar el inicio, vuelve al final (antes PISABA
        // StartFrame con EndFrame: destruia el rango de la animacion)
        if (CurrentFrame < StartFrame) CurrentFrame = EndFrame;
    }
}
#endif

void Viewport3D::KeyUp(){
    if (ArrowCommon(0, -1)) return;
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else
    // transform de HUESOS en curso: las flechas no deben tocar los OBJETOS (comparten `estado`)
    if (BoneGrabActive()) return;

    //mueve el mouse
    if (mouseVisible){
        mouseY--;
        if (mouseY < 0){mouseY = 0;};
    }

    if (state == editNavigation){
        if (navigationMode == Orbit){
            if (CameraActive && ViewFromCameraActive && CameraToView){
                Object& obj = *CameraActive;
                // Convertir el angulo de rotX a radianes
                /*GLfloat radRotX = obj.rotX * M_PI / 180.0;
                GLfloat radRotY = obj.rotY * M_PI / 180.0;
                //GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

                obj.posX+= 30 * sin(radRotX);
                //obj.posY-= 30 * cos(radRotX);
                obj.posZ+= 30 * cos(radRotY);*/
            }
            else {
                if (ViewFromCameraActive){
                    SetViewFromCameraActive(false);
                }
                //rotY-= 0.5;
            }
        }
        else if (navigationMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            PivotY+= 30 * cos(radRotX);
            PivotX-= 30 * sin(radRotX);*/
        }
    }
    else if (state == EditScale){
        SetScale(2,0);
    }
    else if (state == rotation && axisSelect == OrbitalAxis){
        RotateOrbital(0, -20); // orbital: pitch (camRight) hacia arriba
    }
    else if (state == translation){
        SetTranslationObjects(0, -5, DragVelocityWorld());
    }
}
#endif

void Viewport3D::KeyDown(){
    if (ArrowCommon(0, 1)) return;
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else
    // transform de HUESOS en curso: las flechas no deben tocar los OBJETOS (comparten `estado`)
    if (BoneGrabActive()) return;

    //mueve el mouse
    /*if (mouseVisible){
        mouseY++;
        if (mouseY > iScreenHeight-17){mouseY = iScreenHeight-17;};
    }*/

    if (state == editNavigation){
        if (navigationMode == Orbit){
            if (CameraActive && ViewFromCameraActive && CameraToView){
                Object& obj = *CameraActive;
                // Convertir el angulo de rotX a radianes
                //GLfloat radRotX = obj.rotX * M_PI / 180.0;
                //GLfloat radRotY = obj.rotY * M_PI / 180.0;
                //GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

                /*obj.posX-= 30 * sin(radRotX);
                //obj.posY-= 30 * cos(radRotX);
                obj.posZ-= 30 * cos(radRotY);*/
            }
            else {
                if (ViewFromCameraActive){
                    SetViewFromCameraActive(false);
                }
                //rotY+= 0.5;
            }
        }
        else if (navigationMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            PivotY-= 30 * cos(radRotX);
            PivotX+= 30 * sin(radRotX);*/
        }
    }
    else if (state == EditScale){
        SetScale(-2,0);
    }
    else if (state == rotation && axisSelect == OrbitalAxis){
        RotateOrbital(0, 20); // orbital: pitch (camRight) hacia abajo
    }
    else if (state == translation){
        SetTranslationObjects(0, 5, DragVelocityWorld());
    }
}
#endif

// (OnBar / BarScrollBy: la base ya contempla la toolbar de abajo -- ver ViewPorts.cpp/ToolbarBase.cpp)

void Viewport3D::SetAxis(int axis){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    if (state != editNavigation){
        RestoreState(false);
        axisSelect = axis;
    }
}
#endif

void Viewport3D::SetViewFromCameraActive(bool value){
    if (!CameraActive) return;
    camViewZoom = 1.0f; camViewPanX = 0.0f; camViewPanY = 0.0f; // al entrar/salir de vista de camara, resetea la inspeccion

    if (value){
        /*LastPosX = posX;
        LastPosY = posY;
        LastPosZ = posZ;
        LastZoom = zoom;*/
    }
    else {
        /*posX = LastPosX;
        posY = LastPosY;
        posZ = LastPosZ;
        zoom = LastZoom;*/
    }
    ViewFromCameraActive = value;
}

void Viewport3D::event_key_down(int keyCode, bool repeat){
    const int key = keyCode;
    // Los menus que abren EN EL CURSOR (Shift+A, U, X, W, Ctrl+R...) usan lastMouseX/Y, que solo se
    // refrescaban en el CLICK -> el menu salia de la ULTIMA posicion clickeada, no del mouse. Aca
    // (con SDL_GetMouseState = posicion ACTUAL) se ponen donde esta el mouse. No durante un transform.
    if (state == editNavigation) SaveMousePos();
    // las FLECHAS se repiten al mantenerlas (mover/orbitar continuo); el resto
    // de las teclas ignora el auto-repeat.
    bool isArrow = (key == W3dK_UP || key == W3dK_DOWN || key == W3dK_LEFT || key == W3dK_RIGHT);
    if (isArrow && repeat != 0) {
        if (key == W3dK_RIGHT)     KeyRight();
        else if (key == W3dK_LEFT) KeyLeft();
        else if (key == W3dK_UP)   KeyUp();
        else                       KeyDown();
        return;
    }
    // Ctrl+C / Ctrl+X / Ctrl+V en Object Mode: copiar / cortar / pegar OBJETOS (tambien entre ventanas de Whisk3D)
    if (repeat == 0 && LCtrlPressed && !LShiftPressed && InteractionMode == ObjectMode && state == editNavigation &&
        (key == W3dK_C || key == W3dK_X || key == W3dK_V)) {
        extern void LayoutObjectsCopy(bool cut); extern void LayoutObjectsPaste();
        if (key == W3dK_V) LayoutObjectsPaste(); else LayoutObjectsCopy(key == W3dK_X);
        return;
    }
    if (repeat == 0) {
        switch (key) {
            case W3dK_LSHIFT:
                ShiftCount = 0;
                LShiftPressed = true;
                break;
            case W3dK_LALT:
                LAltPressed = true;
                break;
            case W3dK_RETURN:  // Enter
                if (BoneGrabActive()){ BoneGrabConfirm(); break; } // grab de huesos: Enter confirma
                key_down_return();
                break;
            case W3dK_RIGHT:   // Flecha derecha
                KeyRight();
                break;
            case W3dK_LEFT:    // Flecha izquierda
                KeyLeft();
                break;
            case W3dK_UP:
                KeyUp();
                break;
            case W3dK_DOWN:
                KeyDown();
                break;
            case W3dK_A:
                // simple: las funciones ya saben si es object o edit mode
                if (LCtrlPressed) {                     // Ctrl+A: menu Apply (Location/Rotation/Scale/All) en object
                    if (InteractionMode == ObjectMode) LayoutApplyMenu(lastMouseX, lastMouseY);
                }
                else if (LShiftPressed) {               // Shift+A: menu Add en el cursor (object)
                    if (InteractionMode == ObjectMode) LayoutMenuAdd(lastMouseX, lastMouseY);
                }
                // EDIT MODE de ARMATURE: A / Alt+A operan sobre los HUESOS (SelectAll*
                // solo sabe de mallas/objetos/pose -> ahi no hacian nada). Con la seleccion
                // completa, las ops de huesos (G/R/S, E, X, Shift+D, Ctrl+P) toman todo.
                else if (BoneEditActive() && !BoneGrabActive())
                    BoneEditSelectAll(BoneEditArm(), !LAltPressed);
                else if (LAltPressed) DeselectAll();   // Alt+A: deseleccionar todo
                else SelectAllForced();          // A: seleccionar todo
                break;
            case W3dK_I:
                // I = Insert Keyframe. Abre el MENU DE CANALES (Todos / Localizacion / Rotacion /
                // Escala) en Object Mode y en Pose Mode; en Edit Mode (vertices) inserta DIRECTO
                // sin menu (una pose de vertices no tiene canales). Todo el ruteo esta en
                // LayoutMenuInsertKeyframe / InsertKeyframeContext (LayoutInput.cpp).
                // Shift+I = insertar TODOS los canales de una, sin abrir el menu (para el que ya
                // sabe lo que quiere y no quiere el click de mas).
                // Edit Mode de malla: I = INSET FACES (como Blender); los keyframes de vertices siguen en el menu Animation
                if (!LCtrlPressed && !LShiftPressed && InteractionMode == EditMode && g_editMesh && state == editNavigation) {
                    InsetStart(lastMouseX, lastMouseY); break;
                }
                if (LCtrlPressed) InvertSelection();       // Ctrl+I: invertir seleccion
                else if (LShiftPressed) InsertKeyframeContext(KfChannelAll);
                else LayoutMenuInsertKeyframe(lastMouseX, lastMouseY);
                break;
            case W3dK_D: {
                if (LAltPressed){
                    NewInstance();
                }
                else if (LShiftPressed){
                    // Edit Mode: duplica la seleccion de malla (o los HUESOS si se edita un armature); si no, los objetos
                    if (BoneEditActive() && !BoneGrabActive()) BoneEditDuplicateInteractive(BoneEditArm(), true);
                    else if (InteractionMode == EditMode && g_editMesh) LayoutDuplicateEdit();
                    else DuplicatedObject();
                }
                break;
            }
            case W3dK_F:
                // F: "New Edge/Face from Vertices" (solo Edit Mode con malla)
                if (state == editNavigation && InteractionMode == EditMode && g_editMesh)
                    LayoutNewFaceEdit();
                break;
            case W3dK_N:
                // Shift+N: Recalculate Normals (Edit Mode), igual que el menu Face
                if (LShiftPressed && state == editNavigation && InteractionMode == EditMode && g_editMesh)
                    LayoutRecalcNormals();
                break;
            case W3dK_B:
                // Ctrl+B (Edit Mode de malla): BEVEL de las aristas; Ctrl+Shift+B: de los vertices (como Blender)
                if (LCtrlPressed && InteractionMode == EditMode && g_editMesh && state == editNavigation) {
                    BevelStart(lastMouseX, lastMouseY, LShiftPressed ? 0 : 1); break;
                }
                // B: BOX SELECT. Anda en Object Mode y en Edit Mode. Arma la cruz guia y
                // espera el arrastre (el paso 2). Ver edit/BoxSelect.h.
                if (state == editNavigation) { BoxSelectBuild(); BoxSelectMove(lastMouseX, lastMouseY); }
                break;
            case W3dK_L:
                // L: Select Linked (la isla conectada bajo el mouse)
                if (state == editNavigation && InteractionMode == EditMode && g_editMesh)
                    LayoutSelectLinked(lastMouseX, lastMouseY);
                break;
            case W3dK_PGUP: case W3dK_PGDN:
                // Re Pag / Av Pag: RADIO del proportional editing durante un transform (teclado sin rueda, N95)
                if (ProportionalApplicable() && state != editNavigation) {
                    ProportionalRadiusScale(key == W3dK_PGUP ? 1.1f : 1.0f / 1.1f); ProportionalReapply(); g_redraw = true;
                }
                break;
            case W3dK_O:
                // O: PROPORTIONAL EDITING on/off (Edit y Object Mode). En medio de un transform tambien
                // vale: los vecinos vuelven a su lugar (off) o se arrastran desde el proximo transform (on).
                if (InteractionMode == EditMode || InteractionMode == ObjectMode) {
                    g_prop.on = !g_prop.on;
                    Notify(T(g_prop.on ? "Proportional Editing: on" : "Proportional Editing: off"), false);
                    ProportionalReapply();
                    g_redraw = true;
                }
                break;
            case W3dK_U:
                // Edit Mode: U abre el menu UV (Mark Seam + proyecciones).
                // Object Mode: U NO hace nada (la "limpieza de pantalla" / Show Overlays se maneja
                // SOLO desde el checkbox del menu Overlays, no por tecla).
                if (InteractionMode == EditMode && g_editMesh && state == editNavigation)
                    LayoutMenuUV(lastMouseX, lastMouseY);
                break;
            case W3dK_J:
                // Ctrl+J: une las mallas seleccionadas en el objeto activo (Join).
                // J sola en Edit Mode: Connect Vertex Path (corta las caras entre los 2 vertices).
                if (LCtrlPressed && state == editNavigation && InteractionMode == ObjectMode)
                    JoinObjects();
                else if (!LCtrlPressed && state == editNavigation && InteractionMode == EditMode && g_editMesh)
                    LayoutConnectVerticesEdit();
                break;
            case W3dK_H:
                UndoCaptureVisibility();   // Ctrl+Z: guarda el 'visible' PREVIO antes de togglear
                ChangeVisibilityObj();
                break;
            case W3dK_K:
                SetShowOverlays(!showOverlays);
                break;
            case W3dK_X:
                // EDIT de ARMATURE con transform modal (G/R/S): X constrinie al eje (Shift+X = plano);
                // re-apretar cicla Global->Local->View->free, como Object/Pose.
                if (BoneXformMode()){
                    if (LShiftPressed) BoneCyclePlane(0); else BoneCycleAxis(0);
                    break;
                }
                // EDIT de ARMATURE: X = borrar los huesos seleccionados (re-parenta los hijos al abuelo)
                if (BoneEditActive() && !BoneGrabActive() && state == editNavigation){
                    BoneEditDelete(BoneEditArm());
                    break;
                }
                // Pose Mode: X = eje X (Shift+X = plano YZ); cicla Global/Local/View al re-apretar
                if (InteractionMode == PoseMode){ extern int g_poseMode; extern void PoseCycleAxis(int); extern void PoseCyclePlane(int); if (g_poseMode){ if (LShiftPressed) PoseCyclePlane(0); else PoseCycleAxis(0); break; } }
                // durante un transform: X = eje X (re-apretar cicla
                // Global->Local->View->free); Shift+X = plano (mueve en Y,Z).
                if (state != editNavigation){
                    if (LShiftPressed) CyclePlaneTransform(X);
                    else CycleAxisTransform(X);
                    if (EditXformActive()) EditXformRestart(); // restaura y re-aplica el nuevo eje
                }
                // Edit Mode: menu Delete cerca del cursor; Object Mode: POPUP de confirmar borrado
                else if (!LayoutDeleteEdit(lastMouseX, lastMouseY)) OpenConfirmDelete();
                break;
            case W3dK_BACKSPACE:
            case W3dK_DELETE:
                if (BoneEditActive() && !BoneGrabActive() && state == editNavigation){ // Supr = borrar huesos
                    BoneEditDelete(BoneEditArm());
                    break;
                }
                if (state == editNavigation && !LayoutDeleteEdit(lastMouseX, lastMouseY))
                    OpenConfirmDelete();
                break;
            case W3dK_Y:
                if (BoneXformMode()){ if (LShiftPressed) BoneCyclePlane(1); else BoneCycleAxis(1); break; } // modal de huesos: eje Y
                if (InteractionMode == PoseMode){ extern int g_poseMode; extern void PoseCycleAxis(int); extern void PoseCyclePlane(int); if (g_poseMode){ if (LShiftPressed) PoseCyclePlane(1); else PoseCycleAxis(1); break; } }
                if (state != editNavigation){
                    if (LShiftPressed) CyclePlaneTransform(Y);
                    else CycleAxisTransform(Y);
                    if (EditXformActive()) EditXformRestart();
                }
                break;
            case W3dK_Z:
                if (BoneXformMode()){ if (LShiftPressed) BoneCyclePlane(2); else BoneCycleAxis(2); break; } // modal de huesos: eje Z
                if (InteractionMode == PoseMode){ extern int g_poseMode; extern void PoseCycleAxis(int); extern void PoseCyclePlane(int); if (g_poseMode){ if (LShiftPressed) PoseCyclePlane(2); else PoseCycleAxis(2); break; } }
                if (state != editNavigation){
                    if (LShiftPressed) CyclePlaneTransform(Z);
                    else CycleAxisTransform(Z);
                    if (EditXformActive()) EditXformRestart();
                }
                // sin transform: Z = el siguiente MODO DE VISTA (Material Preview -> Solid -> ... -> Rendered), el
                // mismo ciclo que el 6 del keypad del telefono (Ctrl+Z es deshacer: lo agarra controles antes)
                else if (!LCtrlPressed && !LAltPressed) { ChangeViewType(); g_redraw = true; }
                break;
            case W3dK_R:
                // Ctrl+R: Loop Cut and Slide (Edit Mode con malla)
                if (LCtrlPressed && state == editNavigation && InteractionMode == EditMode && g_editMesh){
                    LoopCutStart(lastMouseX, lastMouseY);
                    break;
                }
                // EDIT de ARMATURE: R rota la seleccion (puntas/huesos) alrededor del pivote del
                // viewport; R de nuevo (ya rotando) cicla la orientacion Global->Local->View.
                if (BoneEditActive()){
                    if (BoneXformMode() == 2) BoneCycleOrient();
                    else if (!LCtrlPressed && !BoneGrabActive() && state == editNavigation) BoneXformStart(BoneEditArm(), 2);
                    break;
                }
                // POSE MODE: R rota los huesos seleccionados; R de nuevo (ya rotando) cicla la orientacion View->Global->Local.
                if (InteractionMode == PoseMode){ extern int g_poseMode; extern void PoseXformStart(int); extern void PoseCycleOrient(); extern void PoseClearTransform(int);
                    if (LAltPressed) PoseClearTransform(2);                       // Alt+R: Clear Rotation
                    else if (g_poseMode == 2) PoseCycleOrient(); else PoseXformStart(2); break; }
                // R arranca la rotacion (trackball); R de nuevo alterna
                // trackball <-> orbital/gimbal (sin tener que ciclar X/Y/Z).
                // En Edit Mode el transform actua sobre los vertices seleccionados.
                if (state == rotation) ToggleRotationOrbital();
                else { // EditXformStart en Edit Mode CAPTURA el undo (Ctrl+Z); en Object Mode -> BeginRotate
                    valueRotation = 0;
                    if (!EditXformStart(rotation, ViewAxis)) BeginRotate();
                }
                break;
            case W3dK_G:
                // EDIT de ARMATURE: G agarra la seleccion (puntas o huesos enteros; la punta compartida
                // arrastra a su soldada). Modal: click/Enter confirma, Esc cancela, X/Y/Z constrinien.
                if (BoneEditActive() && state == editNavigation){
                    if (!BoneGrabActive()) BoneXformStart(BoneEditArm(), 1);
                    break;
                }
                if (InteractionMode == PoseMode){ extern void PoseXformStart(int); extern void PoseClearTransform(int);
                    if (LAltPressed) PoseClearTransform(1); else PoseXformStart(1); break; } // POSE: Alt+G limpia translacion; G mueve
                // G con un move YA en curso (Edit Mode): la segunda G pasa a SLIDE, la tercera vuelve a
                // trasladar. No confirma nada: es el mismo modal cambiando de recorrido.
                if (InteractionMode == EditMode && state == translation && EditXformActive()){
                    EditSlideToggle(); break;
                }
                // EditXformStart (no EditXformSnapshot directo) -> en Edit Mode CAPTURA el undo del move (Ctrl+Z)
                if (!EditXformStart(translation, ViewAxis)) BeginTranslate();
                break;
            case W3dK_S:
                // EDIT de ARMATURE: S escala la seleccion alrededor del pivote (X/Y/Z constrinien)
                if (BoneEditActive()){
                    if (!BoneGrabActive() && state == editNavigation) BoneXformStart(BoneEditArm(), 3);
                    break;
                }
                if (InteractionMode == PoseMode){ extern void PoseXformStart(int); extern void PoseClearTransform(int);
                    if (LAltPressed) PoseClearTransform(3); else PoseXformStart(3); break; } // POSE: Alt+S limpia escala; S escala
                // Alt+S en Edit Mode = Shrink/Fatten (cada vert por su normal). Sin Alt = Scale.
                if (LAltPressed && InteractionMode == EditMode && g_editMesh) LayoutShrinkFatten();
                else if (!EditXformStart(EditScale, XYZ)) BeginScale();
                break;
            case W3dK_E:
                // EDIT de ARMATURE: E extruye un hueso desde CADA punta seleccionada (hueso entero =
                // desde su tail) y deja los tails nuevos agarrados al mouse (click/Enter confirma).
                if (BoneEditActive() && !BoneGrabActive() && state == editNavigation){
                    BoneEditExtrudeInteractive(BoneEditArm(), true);
                    break;
                }
                // Edit Mode: extrude de las caras seleccionadas (arranca el move por
                // la normal). En Object Mode no hace nada.
                if (state == editNavigation && InteractionMode == EditMode && g_editMesh)
                    LayoutExtrudeFaces();
                break;
            case W3dK_V:
                // Edit Mode: Shift+V = SLIDE de los vertices seleccionados; V sola = RIP (separa la malla).
                if (state == editNavigation && InteractionMode == EditMode && g_editMesh){
                    if (LShiftPressed) LayoutSlideVerticesEdit();
                    else               LayoutRipEdit();
                }
                break;
            case W3dK_P:
                // EDIT de ARMATURE: Alt+P = menu Disconnect Bone / Clear Parent (punto 5). En PC
                // el atajo global (controls.cpp) lo consume antes; esto es el fallback si el
                // evento llega directo al viewport.
                if (LAltPressed && BoneEditActive() && !BoneGrabActive() && state == editNavigation){
                    LayoutParentBones(true, lastMouseX, lastMouseY);
                    break;
                }
                // Edit Mode: SEPARATE -> mueve las caras seleccionadas a un mesh NUEVO (P > Selection)
                if (state == editNavigation && InteractionMode == EditMode && g_editMesh)
                    LayoutSeparateEdit();
                break;
            case W3dK_W:
                // Edit Mode: menu Mark/Clear Sharp en el cursor (bordes filosos)
                if (state == editNavigation && InteractionMode == EditMode && g_editMesh)
                    LayoutMenuSharp(lastMouseX, lastMouseY);
                break;
            // Numpad (Ctrl = la vista OPUESTA)
            case W3dK_KP_1: SetViewpoint(LCtrlPressed ? Viewpoint::back : Viewpoint::front); break;
            //case W3dK_KP_2: numpad('2'); break;
            case W3dK_KP_3: SetViewpoint(LCtrlPressed ? Viewpoint::left : Viewpoint::right); break;
            case W3dK_KP_4: {
                RollOrbit(-15);
                break;
            }
            case W3dK_KP_5: {
                ChangePerspective();
                break;
            };
            case W3dK_KP_6: {
                RollOrbit(15);
                break;
            }
            case W3dK_KP_7: SetViewpoint(LCtrlPressed ? Viewpoint::bottom : Viewpoint::top); break;
#ifndef W3D_SYMBIAN   // (herramientas de desarrollo de la PC)
            case W3dK_KP_8: FindVertexAnimation(); break;
            case W3dK_KP_9: open(); break;
#endif
            case W3dK_KP_0:
                if (LCtrlPressed) SetActiveObjectAsCamera();          // Ctrl+Num 0: activo -> camara activa (SIN cambiar la vista)
                else SetViewFromCameraActive(!ViewFromCameraActive);  // Num 0: ver desde la camara (toggle)
                break;
            case W3dK_KP_PERIOD: {
                FocusObject();
                break;
            }
            case W3dK_SLASH:   // Local View: aisla la seleccion (frame + oculta el resto); re-tocar restaura todo
                LocalViewToggle();
                break;
            // si querés, agregá más teclas aquí
            case W3dK_ESCAPE:  // Esc
                // box select armado o en curso: lo cancela sin tocar la seleccion
                if (BoxSelectActive()){ BoxSelectCancel(); break; }
                // grab de huesos en curso: lo cancela (restaura head/tail del snapshot)
                if (BoneGrabActive()){ BoneGrabCancel(); NumInputReset(); break; }
                // loop cut en curso: lo descarta (restaura la geometria pre-corte)
                if (LoopCutActive()) LoopCutCancel();
                // Edit Mode con transform de malla en curso: descarta (restaura el
                // snapshot de vertices); si no, cancela el transform de objetos.
                else if (InteractionMode == EditMode && EditXformActive()) EditXformCancel();
                else Cancel();
                NumInputReset();
                break;
        }
    }
    else {
        // Evento repetido por mantener apretada
        switch (key) {
            case W3dK_RETURN:  // Enter
                key_down_return();
                break;
            case W3dK_RIGHT:   // Flecha derecha
                KeyRight();
                break;
            case W3dK_LEFT:    // Flecha izquierda
                KeyLeft();
                break;
            case W3dK_UP:
                KeyUp();
                break;
            case W3dK_DOWN:
                KeyDown();
                break;
            case W3dK_A:
                if (BoneEditActive() && !BoneGrabActive())
                    BoneEditSelectAll(BoneEditArm(), !LAltPressed); // Edit de armature: huesos
                else if (LAltPressed) DeselectAll();
                else SelectAllForced();
                break;
            // Numpad (Ctrl = la vista OPUESTA)
            case W3dK_KP_1: {
                SetViewpoint(LCtrlPressed ? Viewpoint::back : Viewpoint::front);
                break;
            }
            //case W3dK_KP_2: numpad('2'); break;
            case W3dK_KP_3: {
                SetViewpoint(LCtrlPressed ? Viewpoint::left : Viewpoint::right);
                break;
            }
            case W3dK_KP_7: {
                SetViewpoint(LCtrlPressed ? Viewpoint::bottom : Viewpoint::top);
                break;
            }
#ifndef W3D_SYMBIAN   // (herramientas de desarrollo de la PC)
            case W3dK_KP_8: FindVertexAnimation(); break;
            case W3dK_KP_9: open(); break;
#endif
            //case W3dK_KP_0: numpad('0'); break;
            case W3dK_KP_PERIOD: {
                FocusObject();
                break;
            }
            // si querés, agregá más teclas aquí
            case W3dK_ESCAPE:  // Esc
                // grab de huesos en curso: lo cancela (mantener Esc apretado no debe caer al Cancelar de objetos)
                if (BoneGrabActive()){ BoneGrabCancel(); NumInputReset(); break; }
                // Edit Mode con transform de malla en curso: descarta (restaura el
                // snapshot de vertices); si no, cancela el transform de objetos.
                if (InteractionMode == EditMode && EditXformActive()) EditXformCancel();
                else Cancel();
                NumInputReset();
                break;
        }
    }
}

void Viewport3D::event_key_up(int keyCode){
    const int key = keyCode;
    switch (key) {
        case W3dK_LSHIFT:
            // (el TAP de Shift que ciclaba la seleccion era un idioma del TELEFONO -sin mouse,
            //  la tecla cicla- que se filtro a PC: "aprieto shift y me cambia la seleccion, eso
            //  no va" -el dueno-. En PC/Android/WebGL Shift es SOLO modificador; el ciclado del
            //  N95 vive en su propio handler de Symbian.)
            ShiftCount = 0;      // el gesto termino: el proximo arranca limpio (el outliner
                                 // ya lo hacia, Outliner.cpp; aca faltaba y el estado quedaba
                                 // sucio entre gestos)
            LShiftPressed = false;
            break;
        case W3dK_LALT:
            LAltPressed = false;
            break;
    }
}

void Viewport3D::key_down_return(){
    Accept();
}

Viewport3D* Viewport3DActive = NULL;
// rect (coords de ventana SDL, origen arriba-izquierda) del viewport 3D que dibuja el juego: lo usa la captura del
// mouse de main.cpp para teletransportar el cursor al centro cada frame. false si todavia no se dibujo ninguno.
bool GameViewportRect(int* x, int* y, int* w, int* h) {
    Viewport3D* v = Viewport3DActive;
    if (!v || v->width < 8 || v->height < 8) return false;
    *x = v->x; *y = v->y; *w = v->width; *h = v->height;
    return true;
}

//precalculos
bool recalculateCamera = true;
GLfloat radY = 0.0f;
GLfloat radX = 0.0f;

//GLfloat factor = 0.03f;

GLfloat cosX = 0.0f;
GLfloat sinX = 0.0f;
GLfloat cosY = 0.0f;
GLfloat sinY = 0.0f;


// ============================================================================
//  MODO JUEGO: al DAR PLAY los viewports 3D arrancan como un juego de verdad.
// ============================================================================
static void GamePrepareViewportsRec(ViewportBase* v, bool turnOffOverlays) {
    if (!v) return;
    if (v->ViewportKind() == 1) {
        Viewport3D* v3 = (Viewport3D*)v;
        // DAR PLAY YA NO APAGA LOS OVERLAYS: reporte del dueno "al darle play me quitas los overlays, eso no va".
        // Se respeta el showOverlays que el usuario tenga (grilla/gizmos + gate del overlay de stats fps/gl/faces).
        (void)turnOffOverlays; // (vestigial: antes forzaba v3->showOverlays=false)
        // DAR PLAY NO TOCA LA VISTA DEL USUARIO. Reporte del dueno, textual:
        // "cuando aprieto play no quiero que me modifiques el paneo/zoom de la
        // camara activa". Aca se reseteaba camViewZoom/camViewPanX/camViewPanY
        // (la INSPECCION de la vista de camara) "para que el render vuelva a su
        // marco completo": el encuadre que el usuario se armo a mano se perdia en
        // cada Play. El LOD y el Culling YA no dependen de esto -- miden desde la
        // Camera del juego por su cuenta (LOD::MeasureEye / Culling::RenderChildren
        // con W3dGameRunning) sin escribir una sola variable del viewport.
        (void)v3;
    }
    // por ContainerKind, NO dynamic_cast: el arbol esta disenado sin RTTI (el
    // compilador del N95 no lo tiene; ver ViewPorts.h) y este .cpp compila alla
    if (v->ContainerKind() == 1) {
        ViewportRow* r = (ViewportRow*)v;
        GamePrepareViewportsRec(r->childA, turnOffOverlays);
        GamePrepareViewportsRec(r->childB, turnOffOverlays);
    } else if (v->ContainerKind() == 2) {
        ViewportColumn* c = (ViewportColumn*)v;
        GamePrepareViewportsRec(c->childA, turnOffOverlays);
        GamePrepareViewportsRec(c->childB, turnOffOverlays);
    }
}
// turnOffOverlays: true SOLO al INICIAR el juego (si el usuario los prende en
// pausa, reanudar no se los vuelve a apagar)
void GamePrepareViewports(bool turnOffOverlays) {
    GamePrepareViewportsRec(rootViewport, turnOffOverlays);
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  NO SE DA PLAY CON TEXTURAS A MEDIO CARGAR (reporte del dueno: "no se puede
//  dar play hasta que carguen todas las texturas").
//
//  La carga de texturas es DIFERIDA: el import encola (material, ruta) y el
//  loop decodifica UNA por frame, asi un proyecto grande abre en el acto y las
//  texturas "aparecen". Bueno para editar; malo para JUGAR: el primer segundo
//  de partida salia gris y encima el primer frame de cada textura nueva pega un
//  tiron. Ahora el Play, si queda algo en la cola, la VACIA DE UNA (bloqueando,
//  con un cartel) y recien despues arranca. Es una sola puerta: la usan el boton
//  Play de la barra del viewport y el del timeline.
//
//  Devuelve true si se puede arrancar (siempre, salvo que algo falle): es una
//  ESPERA, no un rechazo -- rechazar obligaria al usuario a apretar Play dos
//  veces sin saber por que.
// ---------------------------------------------------------------------------
bool GameWaitTextures() {
    // un material DORMIDO (huerfano al abrir) que una malla paso a usar hace un momento: se despierta
    // ya, asi el juego tampoco arranca con esa malla en gris (los huerfanos siguen sin cargarse)
    TexturesWakeUsed();
    const int pend = PendingTextures();
    if (pend <= 0) return true;
    char msg[192];
    snprintf(msg, sizeof(msg), T("Loading %d texture(s) before Play..."), pend);
    Notify(msg, false);
    w3dLogMemory("antes de las texturas del Play");
    LoadAllPendingTextures();
    w3dLogMemory("texturas del Play cargadas");
    g_redraw = true;
    return PendingTextures() == 0;
}

// ---- QUIEN ESCUCHA los sonidos 3D en el editor: la vista 3D activa (por la camara si se mira por ella) ----
#include "objects/Sound.h"
static bool ListenerView(Vector3& pos, Vector3& right, Vector3& forward) {
    if (!Viewport3DActive) return false;
    const Viewport3D::Lens L = Viewport3DActive->CurrentLens();
    pos = L.pos; right = L.right; forward = L.fwd;
    return true;
}
namespace { struct RegListener { RegListener() { W3dSoundListenerHook = ListenerView; } } gRegListener; }
