#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"
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
#include <string>
#include "WhiskUI/draw/glesdraw.h"
#include "ui/W3dColors.h" // W3dColores: colores del editor (piso, ejes de transformacion)
#include "render/OpcionesRender.h" // flags del overlay de normales
#include "render/EscenaRender.h"   // EL pase 3D COMPARTIDO con el runtime del juego compilado
#include "objects/Mesh.h"          // overlay de estadisticas (vertsAgrupados, faces3d)
#include "objects/EditMesh.h"      // foco al centro de la seleccion en edit mode
#include "objects/Armature.h"      // dibujar huesos del esqueleto encima de todo
#include "objects/Particulas.h"    // pase de particulas: se dibujan DESPUES de los opacos
#include "objects/UI.h"            // igualQueRender: el HUD del juego elige lienzo/letterbox
#include "animation/SkeletalAnimation.h" // EvaluarPoseEsqueleto (pose al reproducir)
#include "animation/Animation.h"          // CurrentFrame
// la sim del juego (scripts lua) esta cargada? (play O pausa; false tras Stop)
static bool JuegoSimActiva() { extern bool SimActiva(); return SimActiva(); }
#include "ViewPorts/LayoutInput.h" // LayoutDeleteEdit (menu Delete en edit mode)
#include "ViewPorts/UVEditor.h"    // roles TBR_Pincel*/TBR_Grupo (toolbar del modo Weight Paint, compartidos con el UV)
#include "edit/WeightPaint.h"      // pincel + escritura de pesos (modo Weight Paint)
#include "edit/BoneEdit.h"         // Edit Mode de ARMATURE: ops de huesos + grab de head/tail (Fase 3)
#include "ViewPorts/PopUp/NumPad.h" // NumPadAbrirTransform (teclado tactil sobre la barra de estado)
#include "ViewPorts/TransformUI.h"  // UI compartida del transform (W3dFmtFloat / TextoNum / barra de info)
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup al borrar con la tecla)
#include "ViewPorts/PopUp/ProgressPopup.h"  // barra "Rendering..." durante el render por tiles (clave en N95)
#include "ViewPorts/Notificaciones.h"       // aviso "Loading textures..." al dar Play
#include "importers/import_obj.h"           // TexturasPendientes / CargarTodasTexturasPendientes
void RebindMaterialMeshPart(); // (def en Properties.cpp) refresca el panel de material tras undo/redo
#include "W3dProfile.h" // g_profShow (ms por categoria) para el overlay Statistics; se define en config/W3dProfile.cpp
#ifdef W3D_SYMBIAN
extern int W3dPantallaAlto; // los headers GLES + tipos GL los da GeometriaUI.h (via ViewPort3D.h)
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_COORD_REPLACE
#define GL_COORD_REPLACE 0x8862
#endif
#endif

// HARNESS (hudanimpx): si esta puesto, el proximo RenderUI junta aca las posiciones
// RESUELTAS de los elementos del HUD (el MISMO vector de UI2D_DibujarOverlay). Asi la
// prueba lee el pixel del elemento EN EL FRAME REAL (3D + HUD), no en una pantalla
// sintetica. NULL (siempre, salvo mientras el harness mide) = cero costo.
std::vector<UI2DPos>* g_hudCapturaPos = NULL;

PopupMenu* MenuAdd = NULL;
PopupMenu* MenuImports = NULL; // submenu "Add > Imports": OBJ / FBX / glTF / GLB
PopupMenu* MenuMallas  = NULL; // submenu "Add > Mesh": las primitivas (8 de las 19 filas del menu)
PopupMenu* MenuSelect = NULL;    // seleccion: All / None / Invert
PopupMenu* MenuObject = NULL;    // operaciones de objeto (solo si hay seleccion)
PopupMenu* MenuAnimation = NULL; // "Animation": keyframes del objeto + Motion Trail (solo con seleccion)
PopupMenu* MenuMesh = NULL;      // edit mode: operaciones de malla (Transform + Extrude)
PopupMenu* MenuTransform = NULL; // submenu de Object: Move / Rotate / Scale
PopupMenu* MenuPose = NULL;      // menu "Pose" (Pose Mode): Insert Keyframe + Transform
PopupMenu* MenuSetOrigin = NULL; // submenu de Object: Geometry/Origin/Cursor
PopupMenu* MenuApply = NULL;     // submenu de Object: Apply Location/Rotation/Scale/All (Ctrl A)
PopupMenu* MenuView = NULL;      // boton "View" (antes de Select): submenus Cameras + Viewpoint
MenuItem* MenuItemLockOrbit = NULL; // el item "Lock Orbit" del menu View (para refrescar su tilde al abrir)
PopupMenu* MenuViewpoint = NULL; // submenu de View: Camera/Top/Bottom/Front/Back/Right/Left (numpad)
PopupMenu* MenuCameras = NULL;   // submenu de View: Set Active Object as Camera / Active Camera
PopupMenu* MenuOverlays = NULL;  // overlays del viewport (checkboxes)
PopupMenu* MenuRender = NULL;    // modo de vista: Render/Material/Solid/Wireframe
PopupMenu* MenuOrient = NULL;    // orientacion de transform: Global/Local/View
PopupMenu* MenuMode = NULL;      // modo del objeto activo: Object/Edit/Vertex/Weight/Texture
PopupMenu* MenuSelMode = NULL;   // edit mode: sub-elemento Vertex/Edge/Face

// busca un boton de la barra por su ROL (no por indice) -> reordenar la barra no rompe nada.
Button* BarRolBtn(std::vector<Button*>& B, int rol){
    for (size_t i = 0; i < B.size(); i++) if (B[i] && B[i]->rol == rol) return B[i];
    return NULL;
}
int BarRolIdx(std::vector<Button*>& B, int rol){
    for (size_t i = 0; i < B.size(); i++) if (B[i] && B[i]->rol == rol) return (int)i;
    return -1;
}

// "rotar desde la vista" (trackball): el angulo lo da la posicion del mouse
// alrededor del pivot EN pantalla. gTrackballCap = ya capturamos el angulo
// inicial; los Track* son endpoints (coords de pantalla del viewport) de la
// linea punteada pivot->mouse.
// gTrackballCap es GLOBAL (variables.h): SetRotacion() lo resetea al arrancar
static float gTrackballAng0 = 0.0f;
static float gTrackPivX = 0, gTrackPivY = 0, gTrackMouseX = 0, gTrackMouseY = 0;
static bool  gLineaValida = false; // hay endpoints validos para la linea punteada

Viewport3D::Viewport3D(Vector3 pos){
    // barra: [0] tipo de viewport (BarCrear), [1] Select, [2] Add,
    //        [3] Object (solo con algo seleccionado), [4] Overlays
    // barra: [0] tipo de viewport, [1] Mode (selector de modo, con icono),
    // [2] SelMode (Vertex/Edge/Face, SOLO en edit mode, al lado de Mode),
    // [3] Select, [4] Add, [5] Object, [6] Overlays, [7] Render, [8] Orient.
    BarCrear(); // [0] = icono de tipo/split
    // Cada boton lleva su ROL estable; el dispatch los busca por rol (BarRolBtn) -> el ORDEN
    // VISUAL de aca se puede cambiar libremente sin tocar el dispatch. Orient va ANTES de Select.
    Button* b;
    b = new Button(T("Object Mode"), (int)IconType::object); b->rol = BR_Mode;
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo si el activo es malla
    b = new Button("", (int)IconType::selVertex); b->rol = BR_SelMode;     // SOLO icono (Vertex/Edge/Face)
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo en edit mode
    b = new Button("", (int)IconType::pivotMedian); b->rol = BR_Pivot;     // Transform Pivot (solo icono)
        b->desplegable = true; BarButtons.push_back(b);
    b = new Button(T("Orient")); b->rol = BR_Orient;                          // orientacion del transform
        b->desplegable = true; BarButtons.push_back(b);                    // (movido ANTES de Select)
    b = new Button("", IconType::snap); b->rol = BR_Snap;                  // SNAP (imanta al mover): verde si esta ON
        b->desplegable = true; BarButtons.push_back(b);
    // "View" es un ICONO (monitor): el texto ocupaba ancho de barra, que es lo que escasea (sobre todo en el N95).
    b = new Button("", IconType::monitor); b->rol = BR_View; b->desplegable = true; BarButtons.push_back(b);
    // "Select" y "Add" son ICONOS sin texto (el titulo va en el menu desplegable): el texto
    // "Seleccionar"/"Anadir" comia ancho de barra (escaso, sobre todo en el N95).
    b = new Button("", IconType::seleccion); b->rol = BR_Select; b->desplegable = true; BarButtons.push_back(b);
    b = new Button("", IconType::mas); b->rol = BR_Add; b->desplegable = true; BarButtons.push_back(b);
    b = new Button(T("Mesh")); b->rol = BR_Mesh;                               // Transform/Snap/Delete comun
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // SOLO en edit mode
    b = new Button("", IconType::keyframe); b->rol = BR_Animation;   // icono rombo (keyframe)
    b->desplegable = true;
    BarButtons.push_back(b);
    b = new Button("", IconType::object); b->rol = BR_Object;              // cubo
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo con seleccion (no edit)
    // Overlays y Render son ICONOS: el texto comia ancho de barra. El OJO del overlay ademas dice el estado
    // (abierto = se ven; cerrado = apagados), asi que se elige por frame mas abajo, no aca.
    b = new Button("", IconType::visible); b->rol = BR_Overlays; b->desplegable = true; BarButtons.push_back(b);
    b = new Button("", IconType::camera);  b->rol = BR_Render;   b->desplegable = true; BarButtons.push_back(b);
    // MODO JUEGO: transporte minimo (visible SOLO con la animacion "Juego" activa).
    // Sin ellos, un layout con solo el viewport 3D no tenia forma de pausar/parar.
    b = new Button(T("Stop")); b->rol = BR_JuegoStop; b->visible = false; BarButtons.push_back(b);
    b = new Button("Play");    b->rol = BR_JuegoPlay;  b->visible = false; BarButtons.push_back(b);
    b = new Button("UV"); b->rol = BR_UV;                                  // Mark Seam + proyecciones
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // SOLO en edit
    barAlpha = 0.5f; // en el 3D la barra deja ver la escena detras
    // barra de HERRAMIENTAS (abajo): MISMOS Button que la barra de arriba. El mecanismo (layout/
    // scroll/hit/render) es el COMPARTIDO de ViewportBase (ToolbarBase.cpp); la visibilidad/colores
    // se setean por frame en ToolbarSincronizar (contextual: historial vs controles del transform).
    // Undo / Redo: SIEMPRE presentes (PC y tactil), primeros (izquierda) para que no se pierdan.
    // Flecha izquierda = deshacer (como el "atras" del navegador); derecha = rehacer.
    // (cuadrado = solo icono, ancho = alto: antes lo forzaba el layout de la toolbar a mano)
    b = new Button("", (int)IconType::arrowRight); b->rol = TBR_Undo; b->iconFlip = 1; b->centrado = true; b->cuadrado = true; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::arrowRight); b->rol = TBR_Redo; b->centrado = true; b->cuadrado = true; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::notifOk);    b->rol = TBR_Aceptar;  b->centrado = true; ToolButtons.push_back(b);
    // "Repeat" (solo en extrude): acepta el extrude y vuelve a extruir la seleccion, sin buscar el boton Extrude.
    b = new Button(T("Repeat")); b->rol = TBR_Repeat; b->centrado = true; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::notifError); b->rol = TBR_Cancelar; b->centrado = true; ToolButtons.push_back(b);
    // modificadores TACTILES (Android/WebGL sin teclado): Shift y Ctrl. Toggle -> quedan encendidos (verde) y
    // togglean LShiftPressed/LCtrlPressed (multi-seleccion, shortest path). Solo visibles con pantalla tactil.
    // Van ANTES de orientacion/ejes: sino durante un transform quedaban al final y se salian de la pantalla.
    b = new Button("Shift"); b->rol = TBR_Shift; b->centrado = true; ToolButtons.push_back(b);
    b = new Button("Ctrl");  b->rol = TBR_Ctrl;  b->centrado = true; ToolButtons.push_back(b);
    // "View" (toggle): en Edit Mode, con 1 dedo orbitar/panear/zoom aunque haya una operacion en curso.
    b = new Button(T("View"));  b->rol = TBR_View;  b->centrado = true; ToolButtons.push_back(b);
    b = new Button("Global"); b->rol = TBR_Orient; b->desplegable = true; ToolButtons.push_back(b);
    b = new Button("X"); b->rol = TBR_EjeX; b->centrado = true; b->cuadrado = true; ToolButtons.push_back(b);
    b = new Button("Y"); b->rol = TBR_EjeY; b->centrado = true; b->cuadrado = true; ToolButtons.push_back(b);
    b = new Button("Z"); b->rol = TBR_EjeZ; b->centrado = true; b->cuadrado = true; ToolButtons.push_back(b);
    // PINCEL (modo Weight Paint): tam / fuerza / sumar-restar / grupo activo. Los ROLES son los
    // reservados en UVEditor.h (TBR_Pincel*/TBR_Grupo): la MISMA toolbar contextual que el UV en
    // modo pintura. Textos por frame en ToolbarSincronizar (labels de WeightPaintLabels).
    b = new Button("40px");  b->rol = TBR_PincelTam;    b->desplegable = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("100%");  b->rol = TBR_PincelFuerza; b->desplegable = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("+");     b->rol = TBR_PincelModo;   b->centrado = true; b->cuadrado = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("Group"); b->rol = TBR_Grupo;        b->desplegable = true; b->visible = false; ToolButtons.push_back(b);
    // "editar solo lo seleccionado" (mascara de pintura): toggle GLOBAL compartido con la
    // toolbar del UV editor (WeightPaintSoloSel); icono de seleccion, tinte accent = ON
    b = new Button("", (int)IconType::seleccion); b->rol = TBR_SoloSel; b->centrado = true; b->cuadrado = true; b->visible = false; ToolButtons.push_back(b);
    for (int i = 0; i < 8; i++){ // historial de acciones (MRU, hasta 8)
        b = new Button(""); b->rol = TBR_Hist + i; b->visible = false; ToolButtons.push_back(b);
    }
    if (!MenuAdd){
        // el menu Add es DECLARATIVO: la lista de items (texto/icono/accion) vive en una tabla en LayoutInput
        // (LayoutConstruirMenuAdd). Aca solo se crean los objetos-menu; el contenido lo pone el builder.
        MenuAdd = new PopupMenu();
        MenuImports = new PopupMenu();
        MenuMallas = new PopupMenu();
        extern void LayoutConstruirMenuAdd();
        LayoutConstruirMenuAdd();
        MenuAdd->titulo = T("Add");   // el boton es un icono sin texto -> el menu lleva titulo
    }
    if (!MenuSelect){
        MenuSelect = new PopupMenu();
        MenuSelect->titulo = T("Select");   // el boton es un icono sin texto -> el menu lleva titulo
        MenuSelect->Agregar(T("All"), 0)->atajo = "A";
        MenuSelect->Agregar(T("None"), 1)->atajo = "Alt A";
        MenuSelect->Agregar(T("Invert"), 2)->atajo = "Ctrl I";
    }
    if (!MenuObject){
        // submenu Transform (Move/Rotate/Scale = G/R/S)
        MenuTransform = new PopupMenu();
        MenuTransform->Agregar(T("Move"), 100)->atajo = "G";
        MenuTransform->Agregar(T("Rotate"), 101)->atajo = "R";
        MenuTransform->Agregar(T("Scale"), 102)->atajo = "S";
        // submenu Set Origin (mueve el origen y/o la geometria)
        MenuSetOrigin = new PopupMenu();
        MenuSetOrigin->Agregar(T("Geometry to Origin"), 200);
        MenuSetOrigin->Agregar(T("Origin to Geometry"), 201);
        MenuSetOrigin->Agregar(T("Origin to 3D Cursor"), 202);
        // "Clear Original File": borra Mesh::origen (el .obj/.fbx del que se importo). Es lo
        // UNICO que apaga el aviso "N archivo(s) externo(s) NO estan" cuando el usuario movio
        // o borro ese archivo; la geometria no se toca (viaja horneada en el .w3dm). Va aca,
        // en el submenu que ya habla del origen del objeto. Ver OlvidarOrigenSeleccionadas.
        MenuSetOrigin->Agregar(T("Clear Original File"), 203);
        // submenu Apply (Ctrl A): hornea el transform en la malla (ids 220-223 -> LayoutAccionObject -> AplicarTransform)
        MenuApply = new PopupMenu();
        MenuApply->titulo = T("Apply"); // titulo visible tambien cuando se abre standalone con Ctrl+A
        MenuApply->Agregar(T("Location"), 220);
        MenuApply->Agregar(T("Rotation"), 221);
        MenuApply->Agregar(T("Scale"), 222);
        MenuApply->Agregar(T("All Transforms"), 223);
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido.
        MenuObject = new PopupMenu(); MenuObject->titulo = T("Object");
        MenuObject->Agregar(T("Transform"), 0, -1, MenuTransform);    // abre submenu
        MenuObject->Agregar(T("Set Origin"), 4, -1, MenuSetOrigin);  // abre submenu
        MenuObject->Agregar(T("Apply"), 6, -1, MenuApply)->atajo = "Ctrl A"; // submenu Location/Rotation/Scale/All
        MenuObject->Agregar(T("Duplicate Objects"), 1)->atajo = "Shift D";
        MenuObject->Agregar(T("Duplicate Linked"), 2)->atajo = "Alt D";
        MenuObject->Agregar(T("Join"), 5)->atajo = "Ctrl J"; // une las mallas seleccionadas en el objeto activo
        // Set/Clear Parent (Ctrl P / Ctrl Alt P): submenus construidos en LayoutInput.cpp (reusan los mismos que
        // los atajos). Solo aparecen aca -> solo en Object Mode (MenuObject no se muestra en Edit).
        MenuObject->Agregar(T("Set Parent"),   0, -1, LayoutSubmenuSetParent())->atajo = "Ctrl P";
        MenuObject->Agregar(T("Clear Parent"), 0, -1, LayoutSubmenuClearParent())->atajo = "Ctrl Alt P";
        MenuObject->Agregar(T("Delete"), 3)->atajo = "X";
        // menu "Animation" (barra, Object Mode): keyframes del TRANSFORM del objeto + Motion Trail. Antes era un
        // SUBMENU de "Object"; es su propio menu de la barra (queda a un click, no a dos).
        // SUBMENU de canales de Insert Keyframe (compartido por el menu Animation, el menu Pose, el
        // menu Animation del UV y la tecla I). Los ids 530-533 burbujean hasta la action del menu
        // TOP (PopupMenu::Click) -> los despacha el mismo LayoutAccionObject, sin plomeria nueva.
        // Se aplica a OBJETOS y a POSES de huesos (3D y 2D); en vertices/UV ni se abre.
        extern PopupMenu* MenuInsertKey;
        MenuInsertKey = new PopupMenu(); MenuInsertKey->titulo = T("Insert Keyframe");
        MenuInsertKey->Agregar(T("All"), 530, IconType::keyframe);  // localizacion + rotacion + escala
        MenuInsertKey->Agregar(T("Location Only"), 531);
        MenuInsertKey->Agregar(T("Rotation Only"), 532);
        MenuInsertKey->Agregar(T("Scale Only"), 533);
        MenuAnimation = new PopupMenu();
        MenuAnimation->titulo = T("Animation");   // el boton es un icono sin texto -> el menu lleva titulo
        MenuAnimation->Agregar(T("Insert Keyframe"), 510, IconType::keyframe, MenuInsertKey)->atajo = "I";
        // CAPAS de la animacion del objeto: "vertex animation", "normal animation" y "animar los UV"
        // son la MISMA animacion con capas distintas. Este submenu deja elegir cual capturar en vez de
        // dejar que la decida el editor desde el que insertaste (que es el default y sigue estando).
        static PopupMenu* MenuKeyCapa = NULL;
        MenuKeyCapa = new PopupMenu();
        MenuKeyCapa->Agregar(T("Vertices"), 540, IconType::mesh);
        MenuKeyCapa->Agregar(T("Normals"),  541, IconType::normalVertex);
        MenuKeyCapa->Agregar(T("UV"),       542, IconType::textura);
        MenuAnimation->Agregar(T("Insert Keyframe Layer"), 0, IconType::keyframe, MenuKeyCapa);
        MenuAnimation->Agregar(T("Delete Keyframe"), 511, IconType::borrar);
        MenuAnimation->Agregar(T("Clear Keyframe"),  512);
        // capa de NORMALES de la vertex anim activa: borrarla entera (modelos sin iluminacion). No es un
        // keyframe: es una CAPA de la animacion, por eso va abajo de todo y no en el submenu de keyframes.
        MenuAnimation->Agregar(T("Delete Normals Layer"), 514, IconType::normalVertex);
        MenuAnimation->AgregarCheck(T("Motion Trail"), 513, &MotionTrailOn);
        // menu "Pose" (Pose Mode): Insert Keyframe + submenu Transform (Move/Rotate/Scale). Reusa MenuTransform.
        extern PopupMenu* MenuPose;
        // (regla de los titulos: el boton de contexto es un icono sin texto -> su menu lleva titulo)
        MenuPose = new PopupMenu(); MenuPose->titulo = "Pose";
        MenuPose->Agregar(T("Insert Keyframe"), 500, IconType::armature, MenuInsertKey)->atajo = "I"; // submenu de canales
        MenuPose->Agregar(T("Transform"), 0, -1, MenuTransform); // Move(G)/Rotate(R)/Scale(S) -> ids 100/101/102
        // Clear Transform: resetea la pose de los huesos SELECCIONADOS a rest. All (T+R+S) / Translation / Rotation / Scale.
        static PopupMenu* MenuClearPose = NULL;
        MenuClearPose = new PopupMenu();
        MenuClearPose->Agregar(T("All"), 520, IconType::armature);          // T+R+S -> PoseClearTransform(0)
        MenuClearPose->Agregar(T("Translation"), 521)->atajo = "Alt G";     // PoseClearTransform(1)
        MenuClearPose->Agregar(T("Rotation"), 522)->atajo = "Alt R";        // PoseClearTransform(2)
        MenuClearPose->Agregar(T("Scale"), 523)->atajo = "Alt S";           // PoseClearTransform(3)
        MenuPose->Agregar(T("Clear Transform"), 0, -1, MenuClearPose);
        // menu "Mesh" (Edit Mode): comun a vertice/borde/cara. Transform (arriba), Snap y Delete (abajo). Los
        // submenus Snap/Delete se reusan de LayoutInput.cpp. La accion (LayoutAccionMesh) se asigna al abrirlo.
        // submenu Transform de EDIT (como el de objeto pero + Shrink/Fatten, que solo tiene sentido en malla)
        static PopupMenu* MenuTransformEdit = NULL;
        MenuTransformEdit = new PopupMenu();
        MenuTransformEdit->Agregar(T("Move"), 100)->atajo = "G";
        MenuTransformEdit->Agregar(T("Rotate"), 101)->atajo = "R";
        MenuTransformEdit->Agregar(T("Scale"), 102)->atajo = "S";
        MenuTransformEdit->Agregar(T("Shrink/Fatten"), 103)->atajo = "Alt S"; // cada vert por su normal
        MenuMesh = new PopupMenu();
        MenuMesh->titulo = T("Mesh");
        MenuMesh->Agregar(T("Transform"), 0, -1, MenuTransformEdit);        // arriba de todo (Move/Rotate/Scale/Shrink)
        MenuMesh->Agregar(T("Duplicate"), 314)->atajo = "Shift D";          // comun a vertice/borde/cara
        MenuMesh->Agregar(T("Separate"), 316)->atajo = "P";                 // caras selec -> mesh NUEVO
        MenuMesh->Agregar(T("Merge"), 0, -1, LayoutSubmenuMerge())->atajo = "M"; // suelda verts (limpia duplicados)
        // Auto Merge (opt-in, OFF por defecto): al confirmar un move suelda los verts movidos con los que queden
        // a <= Threshold. Con el auto merge apagado, "Threshold" se ve en gris (->gris = &g_autoMerge).
        // ids 390/391: fuera del rango 0-7 (que LayoutAccionMesh rutea a Snap) y sin caso en LayoutAccionObject
        // (no-op). El toggle del check y el set del slider los hace el propio item; la accion no tiene que hacer nada.
        MenuMesh->AgregarCheck(T("Auto Merge"), 390, &g_autoMerge);
        MenuMesh->AgregarFloat("Threshold", 391, &g_autoMergeThreshold, 0.0f, 0.1f)->gris = &g_autoMerge;
        MenuMesh->Agregar(T("Normals"), 0, -1, LayoutSubmenuNormals()); // Recalculate Normals + Flip
        MenuMesh->Agregar("Snap", 0, -1, LayoutSubmenuSnap())->atajo = "Shift S";
        MenuMesh->Agregar(T("Delete"), 360, -1, LayoutSubmenuDelete())->atajo = "X"; // abajo de todo
    }
    if (!MenuView){
        // boton "View" (antes de Select): submenu Viewpoint con los 7 puntos de vista (mismos atajos del numpad).
        // ids 400-406 -> LayoutAccionView -> Viewport3DActive->SetViewpoint(...). Antes eran atajos "ocultos".
        MenuViewpoint = new PopupMenu();
        MenuViewpoint->Agregar(T("Camera"), 400)->atajo = "Num 0";
        MenuViewpoint->Agregar(T("Top"),    401)->atajo = "Num 7";
        MenuViewpoint->Agregar(T("Bottom"), 402)->atajo = "Ctrl Num 7";
        MenuViewpoint->Agregar(T("Front"),  403)->atajo = "Num 1";
        MenuViewpoint->Agregar(T("Back"),   404)->atajo = "Ctrl Num 1";
        MenuViewpoint->Agregar(T("Right"),  405)->atajo = "Num 3";
        MenuViewpoint->Agregar(T("Left"),   406)->atajo = "Ctrl Num 3";
        // submenu Cameras: setear el objeto activo como camara / ver desde la camara activa
        MenuCameras = new PopupMenu();
        MenuCameras->Agregar(T("Set Active Object as Camera"), 410)->atajo = "Ctrl Num 0";
        MenuCameras->Agregar(T("Active Camera"),               411)->atajo = "Num 0";
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido. El boton View ahora es un icono -> titulo.
        MenuView = new PopupMenu(); MenuView->titulo = T("View");
        MenuView->Agregar(T("Cameras"),   0, -1, MenuCameras);   // abre submenu (antes de Viewpoint, como Blender)
        MenuView->Agregar(T("Viewpoint"), 0, -1, MenuViewpoint); // abre submenu
        MenuView->Agregar(T("Frame Selected"), 420)->atajo = "Numpad ."; // enfocar la seleccion (EnfocarObject)
        MenuView->Agregar(T("Perspective/Ortho"), 407)->atajo = "Num 5"; // alterna perspectiva/ortografica
        // Lock Orbit: item REGULAR (no checkbox) -> al tocarlo togglea Y CIERRA el menu. El
        // tilde (verde) se refresca al abrir el menu, con el estado del viewport activo (LayoutInput).
        MenuItemLockOrbit = MenuView->Agregar(T("Lock Orbit"), 421);
    }
    if (!MenuRender){
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido.
        MenuRender = new PopupMenu(); MenuRender->titulo = "Render";
        MenuRender->Agregar(T("Render Preview"), 0);
        MenuRender->Agregar(T("Material Preview"), 1);
        MenuRender->Agregar(T("Solid Preview"), 2);
        MenuRender->Agregar(T("Wireframe Preview"), 3);
        MenuRender->Agregar(T("ZBuffer Preview"), 4);
        MenuRender->Agregar(T("Normal View"), 5); // simula normal map con 3 luces R/G/B
        MenuRender->Agregar(T("Alpha Preview"), 6); // matte blanco/negro por alpha (debug del pase alpha)
        // wireframe SOLO de lo que se dibuja (default ON): con visibilidad por
        // triangulo activa (modificador Culling) el alambre muestra la lista
        // ACTIVA de la celda, no todas las aristas. El flag es global del Core
        // (w3dGraphics.h): el checkbox lo togglea directo.
        MenuRender->AgregarCheck(T("Wireframe: Only Visible"), 7, &w3dRenderWireframeSoloVisible);
    }
    if (!MenuOrient){
        // orientacion usada al constrenir a un eje (X/Y/Z) y por el extrude. Default Global.
        MenuOrient = new PopupMenu();
        MenuOrient->titulo = T("Transform Orientations");
        MenuOrient->Agregar("Global", 0);
        MenuOrient->Agregar("Local", 1);
        MenuOrient->Agregar("Normal", 3); // = la direccion de la normal (lo que hace el extrude)
        MenuOrient->Agregar(T("View"), 2);
    }
    if (!MenuMode){
        // modo del objeto activo (solo malla). Edit y los Paint todavia no
        // hacen nada: dejamos la opcion para mas adelante.
        MenuMode = new PopupMenu();
        MenuMode->Agregar(T("Object Mode"), 0, IconType::object); // icono de objeto
        MenuMode->Agregar(T("Edit Mode"), 1, IconType::mesh);     // icono de malla 3d
        MenuMode->Agregar(T("Vertex Paint"), 2, IconType::mesh);
        MenuMode->Agregar(T("Weight Paint"), 3, IconType::mesh);
        MenuMode->Agregar(T("Texture Paint"), 4, IconType::mesh);
    }
    if (!MenuSelMode){
        // sub-elemento de Edit Mode (la seleccion -todo/nada/invertir- y el render
        // se refieren a esto). Vertex es el default.
        MenuSelMode = new PopupMenu();
        MenuSelMode->Agregar(T("Vertex"), SelVertex, IconType::selVertex);
        MenuSelMode->Agregar(T("Edge"),   SelEdge,   IconType::selEdge);
        MenuSelMode->Agregar(T("Face"),   SelFace,   IconType::selFace);
    }
    // (eran inicializadores de clase: C++03)
    orthographic = false;
    ViewFromCameraActive = false;
    camFrameOn = false; camFrameNX = 1.0f; camFrameNY = 1.0f; camFrameLetterbox = false;
    letterboxNegro = false;   // editor: bandas ATENUADAS por default (auditar el culling)
    statTrisFrame = 0; statDrawsFrame = 0; statBindsFrame = 0; statEstadosFrame = 0;
    camViewZoom = 1.0f; camViewPanX = 0.0f; camViewPanY = 0.0f;
    hudX0 = 0.0f; hudY0 = 0.0f; hudW = 0.0f; hudH = 0.0f; hudEsc = 1.0f; // aun sin HUD dibujado
    hudOverride = false;
    showOverlays = true;
    showFloor = true;
    showYaxis = true;
    showXaxis = true;
    CameraToView = false;
    showOrigins = true;
    showArmature = true;
    showLights = true;
    showCamera = true;
    showEmpty = true;
    show3DCursor = true;
    ShowRelantionshipsLines = true;
    limpiarPantalla = true;
    lockOrbit = false; // por defecto se orbita normal; el usuario lo activa para modo tablero 2D
    view = RenderType::MaterialPreview;
    nearClip = 0.01f;
    farClip = 1000.0f;
    aspect = 1.0f;
    bgSolido[0] = bgSolido[1] = bgSolido[2] = bgSolido[3] = -1.0f; // sentinel: usar el color del tema
    viewRot = Quaternion::FromEulerYXZ(-30.0f, -23.0f, 0.0f);
    orbitDistance = 10.0f;

    RecalcOrbitPosition();
}

Viewport3D::~Viewport3D() {
    // si este viewport era el dueno del lienzo del juego (modo juego con UI
    // dinamica), soltarlo: un override colgado apuntaria a un viewport muerto
    UI2D_OverrideVentanaQuitar(this);
};

void Viewport3D::AbrirMenuOverlays(int x, int y){
    if (!MenuOverlays){
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido.
        MenuOverlays = new PopupMenu(); MenuOverlays->titulo = T("Overlays");
    }
    // se reconstruye en cada apertura apuntando a los flags de ESTE viewport
    // (cada instancia 3D tiene los suyos: split view = overlays independientes).
    // Sin titulo: el primer item ES el master "Show Overlays". Si esta off el
    // resto se ve en gris (->gris = &showOverlays) porque sin overlay no se ven.
    MenuOverlays->Limpiar();
    MenuOverlays->AgregarCheck(T("Show Overlays"), 0, &showOverlays);
    MenuOverlays->AgregarCheck(T("Floor"), 1, &showFloor)->gris = &showOverlays;
    MenuOverlays->AgregarCheck(T("X Axis"), 2, &showXaxis)->gris = &showOverlays;
    MenuOverlays->AgregarCheck(T("Y Axis"), 3, &showYaxis)->gris = &showOverlays;
    MenuOverlays->AgregarCheck(T("Origins"), 4, &showOrigins)->gris = &showOverlays;
    // submenu "Objects": mostrar/ocultar el overlay de cada tipo de objeto (esqueleto / luces / camaras / empties)
    static PopupMenu* MenuOverlayObjects = NULL;
    if (!MenuOverlayObjects) MenuOverlayObjects = new PopupMenu(); // sin titulo (ya sabes que es al abrirlo)
    MenuOverlayObjects->Limpiar();
    MenuOverlayObjects->AgregarCheck(T("Armature"), 13, &showArmature, IconType::armature);
    MenuOverlayObjects->AgregarCheck(T("Lights"),   14, &showLights,   IconType::light);
    MenuOverlayObjects->AgregarCheck(T("Camera"),   15, &showCamera,   IconType::camera);
    MenuOverlayObjects->AgregarCheck(T("Empty"),    16, &showEmpty,    IconType::empty);
    MenuOverlays->Agregar(T("Objects"), 13, IconType::object, MenuOverlayObjects)->gris = &showOverlays;
    MenuOverlays->AgregarCheck(T("3D Cursor"), 5, &show3DCursor)->gris = &showOverlays;
    MenuOverlays->AgregarCheck(T("Relationship Lines"), 6, &ShowRelantionshipsLines)->gris = &showOverlays;
    // submenu "Normals" (solo en meshes seleccionadas): 3 toggles + slider de tamano. Sin titulo (ya sabes que es al abrirlo).
    static PopupMenu* MenuOverlayNormals = NULL;
    if (!MenuOverlayNormals) MenuOverlayNormals = new PopupMenu();
    MenuOverlayNormals->Limpiar();
    MenuOverlayNormals->AgregarCheck(T("Vertex Normal"), 7, &OverlayVertexNormal, IconType::normalVertex);
    MenuOverlayNormals->AgregarCheck(T("Custom Normal"), 8, &OverlayCustomNormal, IconType::normalCustom);
    MenuOverlayNormals->AgregarCheck(T("Face Normal"),   9, &OverlayFaceNormal,   IconType::normalFace);
    MenuOverlayNormals->AgregarFloat("Normal Size",  10, &OverlayNormalSize, 0.0f, 1.0f);
    MenuOverlays->Agregar(T("Normals"), 7, IconType::normalVertex, MenuOverlayNormals)->gris = &showOverlays;
    // submenu "Statistics": texto blanco arriba a la derecha del viewport. Cada linea es un toggle independiente. Sin titulo.
    static PopupMenu* MenuOverlayStats = NULL;
    if (!MenuOverlayStats) MenuOverlayStats = new PopupMenu();
    MenuOverlayStats->Limpiar();
    MenuOverlayStats->AgregarCheck("FPS",      12, &OverlayFps);
    MenuOverlayStats->AgregarCheck(T("Vertices"), 17, &OverlayStatVertices);
    MenuOverlayStats->AgregarCheck(T("Faces"),    18, &OverlayStatFaces);
    MenuOverlayStats->AgregarCheck("GL Calls", 21, &OverlayStatGL); // llamadas GL por frame (draw/bind/estado)
    MenuOverlayStats->AgregarCheck("Modgen",   19, &OverlayStatModgen);
    MenuOverlayStats->AgregarCheck(T("Times"),    20, &OverlayStatTimes);
    MenuOverlays->Agregar(T("Statistics"), 11, -1, MenuOverlayStats)->gris = &showOverlays;
    // Clear Screen: limpia el framebuffer (glClear) cada frame. NO es un overlay (no se grisa con
    // Show Overlays). Apagarlo gana rendimiento en juegos/renders donde la escena llena la pantalla.
    // ON por defecto (limpiarPantalla = true en el ctor).
    MenuOverlays->AgregarCheck(T("Clear Screen"), 13, &limpiarPantalla);
    // X-Ray (retopologia): la malla EN EDICION se dibuja semitransparente (30%) sin z-test y sus bordes/vertices
    // siempre encima -> se ven y se pueden SELECCIONAR los verts/aristas de atras (los tapados por las caras).
    // Modo propio, NO se grisa con Show Overlays. Global (una sola malla en edicion a la vez).
    MenuOverlays->AgregarCheck(T("X-Ray"), 14, &g_xray);
    // Letterbox negro: con encuadre declarado + juego corriendo, las bandas de afuera
    // del marco se pintan OPACAS como en el dispositivo. OFF (default) = atenuadas,
    // para ver que hay afuera y auditar el culling. NO se grisa con Show Overlays
    // (es parte de la vista de juego, no un overlay de edicion).
    MenuOverlays->AgregarCheck(T("Letterbox Negro"), 15, &letterboxNegro);
    MenuOverlays->action = NULL; // el toggle lo hace el propio item (checkbox)
    MenuOverlays->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = MenuOverlays;
}

#ifndef W3D_SYMBIAN
void Viewport3D::event_mouse_wheel(float dy, int mx, int my) {
    // rueda sobre la BARRA superior = scroll horizontal (en PC no entran todos los botones); sino zoom.
    // Unificado con la barra de propiedades (BarScrollHorizontal).
    if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return;
    if (OnToolbar(mx, my)) { ToolbarScrollBy((int)(dy * 40)); return; } // barra de HERRAMIENTAS (abajo)
    Zoom(dy * 2.0f); // Zoom() ya distingue: viewport normal = distancia; vista de camara = inspeccion
}
#endif

// suma al radio del foco el bounding de cada objeto seleccionado (esfera centrada en 'c' que
// envuelve la seleccion entera): max(|c - PuntoFoco| + RadioFoco) sobre los seleccionados.
static void CalcRadioFocoRec(Object* obj, const Vector3& c, float& r){
    if (obj->select){
        float d = (obj->PuntoFoco() - c).Length() + obj->RadioFoco();
        if (d > r) r = d;
    }
    for (size_t i = 0; i < obj->Childrens.size(); i++) CalcRadioFocoRec(obj->Childrens[i], c, r);
}

void Viewport3D::EnfocarObject() {
    // el foco PREGUNTA donde esta lo seleccionado (PuntoFoco/RadioFoco -> matrices de mundo) y esa
    // respuesta depende de cual es la vista bindeada. Enfocar puede dispararse desde el menu o el
    // atajo sin haber dibujado ESTE viewport ultimo, asi que primero se bindea el suyo.
    BindVista();
    // Edit Mode: enfocar la SELECCION de sub-elementos (vertices/aristas/caras): centro + radio
    // -> centra Y ajusta el zoom. Si no hay nada seleccionado, cae al foco del objeto.
    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        float cx, cy, cz, rL;
        if (m->edit && m->edit->CentroRadioSeleccion(cx, cy, cz, rL)) {
            Vector3 c = m->LocalAMundo(Vector3(cx, cy, cz));
            EncuadrarRadio(c, m->EscalarRadioLocal(Vector3(cx, cy, cz), rL)); // radio local -> mundo
            return;
        }
    }
    if (!ObjSelects.empty()) {
        // el FOCO mira el CENTRO GEOMETRICO de la seleccion (las mallas aportan su centro de
        // vertices), NO el pivote de transform. El radio envuelve toda la seleccion (con su tamaño).
        Vector3 c = CentroFocoSeleccion();
        float r = 0.0f;
        if (SceneCollection)
            for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
                CalcRadioFocoRec(SceneCollection->Childrens[i], c, r);
        EncuadrarRadio(c, r);
    }
}

void Viewport3D::EncuadrarRadio(const Vector3& centro, float radio){
    pivot = centro;
    if (radio < 0.05f) radio = 0.05f; // un vertice/punto: radio minimo (zoom cercano pero no degenerado)
    // distancia para que una esfera de 'radio' entre en el FOV. Se usa el menor half-FOV (vertical vs
    // horizontal, segun el aspect) para que entre en las DOS direcciones. *1.25 = padding pedido.
    float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    float halfV = fovDeg * 0.5f * 3.14159265f / 180.0f;
    float halfH = atanf(tanf(halfV) * aspect);
    float ang = halfV < halfH ? halfV : halfH;
    float s = sinf(ang); if (s < 0.01f) s = 0.01f;
    orbitDistance = radio / s * 1.25f;
    if (orbitDistance < 0.02f)     orbitDistance = 0.02f;
    if (orbitDistance > farClip)   orbitDistance = farClip;
    RecalcOrbitPosition();
}

void Viewport3D::Zoom(float delta){
    // vista de camara: el zoom es de INSPECCION (escala la vista + el marco), NO mueve la camara.
    if (ViewFromCameraActive){
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
CameraBase Viewport3D::VistaCam() const {
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
void Viewport3D::BindVista() {
    W3dVistaBind(VistaCam());
}

void Viewport3D::UpdateViewOrbit() {
    if (CameraActive){
        CameraActive->UpdatePosition();
        CameraActive->UpdateLookAt();
    }

    W3dVista3D v; v.cam = VistaCam();

    // luz del NORMAL MAPPING: en RENDER preview usa la luz de ESCENA (Lights[0]) -> el relieve responde a la
    // lampara y su color. En material/solid preview usa la CAMARA (headlight) en blanco -> el material preview NO
    // depende de las luces de escena (no se tienen que ver las luces de escena en material preview).
    Vector3 luzPos = v.cam.pos, luzCol(1, 1, 1);   // headlight neutro
    if (view == RenderType::Rendered && !Lights.empty() && Lights[0]) {
        Matrix4 LW; Lights[0]->GetWorldMatrix(LW);
        luzPos = Vector3(LW.m[12], LW.m[13], LW.m[14]);
        luzCol = Vector3(Lights[0]->diffuse[0], Lights[0]->diffuse[1], Lights[0]->diffuse[2]);
    }
    // publicar la vista al core + cargar la matriz de vista: el MISMO camino que el
    // runtime del juego (EscenaRender). De aca salen el chrome equirect (reflejo
    // respecto de esta camara) y la base right/up/forward del MATCAP por software.
    W3dEscena3DCamara(v, luzPos, luzCol);
}

void Viewport3D::RotateOrbit() {
    // vista de camara: NO se orbita (la camara define la mirada); el arrastre PANEA la inspeccion (ver detalle).
    if (ViewFromCameraActive) { Pan(); return; }
    // "Lock Orbit" ON: NUNCA gira el orbital. Todo lo que orbitaria (arrastre de mouse, 1 dedo tactil,
    // flechas via OrbitarFlecha) PANEA en su lugar. Usa el mismo dx/dy que ya seteo el que llama. El zoom
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

void Viewport3D::OrbitarFlecha(int ndx, int ndy){
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

// paneo por TECLADO (mismo patron que OrbitarFlecha): setea dx/dy, panea, restaura. Lo usa el keypad del N95.
void Viewport3D::PanFlecha(int ndx, int ndy){
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
        float odx = dx, ody = dy;       // Pan() usa dx/dy; los guardamos y restauramos (como PanFlecha)
        dx = panDx; dy = panDy;
        Pan();
        dx = odx; dy = ody;
    }
}

// El viewport 3D no scrollea CONTENIDO: el gesto de 1 dedo fuera de la barra orbita (lo maneja
// controles.cpp). La barra superior la agarra el gesto lockeado por OnBar/BarScrollBy.
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

void Viewport3D::RecalcOrbitPosition(){
    Vector3 forward = viewRot * Vector3(0,0,-1);
    viewPos = pivot - forward * orbitDistance;

    // Extraer ejes locales desde el quaternion de la vista
    camRight   = viewRot * Vector3(1, 0, 0);
    camUp      = viewRot * Vector3(0, 1, 0);
    camForward = viewRot * Vector3(0, 0, -1);
}

bool Viewport3D::ProyectarPunto(const Vector3& p, float& sx, float& sy, float* outW){
    // ejes de camara de ESTE viewport (no los globals, que pisa el ultimo
    // renderizado en multi-3D)
    Vector3 cr = viewRot * Vector3(1, 0, 0);
    Vector3 cu = viewRot * Vector3(0, 1, 0);
    Vector3 cf = viewRot * Vector3(0, 0, -1); // hacia la escena
    Vector3 rel = p - viewPos;
    float ez = rel.Dot(cf);                   // distancia hacia adelante
    if (ez < 0.0001f) return false;           // detras de la camara
    // divisor de perspectiva: en perspectiva es la profundidad (para interpolar perspective-correct); en
    // ortografica NO hay division -> 1.0 (la proyeccion es afin, el baricentrico de pantalla ya es el real).
    if (outW) *outW = orthographic ? 1.0f : ez;
    float ex = rel.Dot(cr);
    float ey = rel.Dot(cu);
    float aspectR = (height > 0) ? (float)width / (float)height : 1.0f;
    float ndcX, ndcY;
    if (orthographic) {
        // ORTOGRAFICA: sin division por ez. MISMO extent que Render(): size = orbitDistance*tan(fov/2) (zoom).
        float size = orbitDistance * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
        if (size < 0.001f) size = 0.001f;
        ndcX = ex / (size * aspectR);
        ndcY = ey / size;
    } else {
        float fRad = fovDeg * 3.14159265f / 180.0f;
        float f = 1.0f / tanf(fRad * 0.5f);
        ndcX = (ex * (f / aspectR)) / ez;
        ndcY = (ey * f) / ez;
    }
    sx = (ndcX * 0.5f + 0.5f) * (float)width;
    sy = (1.0f - (ndcY * 0.5f + 0.5f)) * (float)height; // pantalla: Y hacia abajo
    return true;
}

// PICK de HUESO (Pose Mode): proyecta cada hueso (poseHead->poseTail, en world) a pantalla y devuelve el indice
// del mas cercano al click (lmx,lmy en coords LOCALES del viewport), o -1 si ninguno esta dentro del umbral (px).
int Viewport3D::PickBone(Armature* arm, float lmx, float lmy){
    if (!arm || arm->bones.empty()) return -1;
    EvaluarPoseEsqueleto(arm, CurrentFrame); // asegurar la pose al frame actual (poseHead/poseTail)
    Matrix4 W; arm->GetWorldMatrix(W);
    int best = -1; float bestD = 14.0f; // umbral de seleccion en pixeles
    for (size_t i = 0; i < arm->bones.size(); i++){
        Vector3 h = W * arm->bones[i].poseHead, t = W * arm->bones[i].poseTail;
        float hx,hy,tx,ty;
        if (!ProyectarPunto(h,hx,hy) || !ProyectarPunto(t,tx,ty)) continue;
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
// modo AplicarPivotATransform sale antes de tocar a nadie, o sea que ese punto NO escribe.
static Vector3 GizmoPivot(){
    if (g_transformPivot == PivotIndividual && ObjActivo) return ObjActivo->GetGlobalPosition();
    return TransformPivotPoint;
}

// VELOCIDAD de arrastre en MUNDO por pixel de pantalla, a la PROFUNDIDAD del pivot de transform.
// Con esto, al arrastrar N pixeles (mouse o flechas) lo agarrado se mueve N pixeles EN PANTALLA a
// cualquier zoom -> se siente "pegado" al cursor. Antes era un 0.01 FIJO que ignoraba el zoom (de
// cerca movia muchisimo, de lejos poquito). Es la inversa exacta de ProyectarPunto (misma base/FOV).
float Viewport3D::VelocidadArrastreMundo(){
    if (height <= 0) return 0.01f;                // guarda (no deberia pasar)
    if (orthographic) {
        // mismo extent que Render() y ProyectarPunto(): size = orbitDistance*tan(fov/2) (sigue el zoom)
        float size = orbitDistance * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
        if (size < 0.001f) size = 0.001f;
        return 2.0f * size / (float)height;      // ortho: mundo-por-pixel constante (no depende de z)
    }
    Vector3 cf = viewRot * Vector3(0, 0, -1);    // hacia la escena (igual que ProyectarPunto)
    float ez = (GizmoPivot() - viewPos).Dot(cf); // profundidad (eye-space) del pivot de transform
    if (ez < nearClip) ez = nearClip;            // no detras de la camara
    float fRad = fovDeg * 3.14159265f / 180.0f;
    // alto visible del frustum a esa profundidad, repartido en 'height' pixeles = mundo por pixel
    return 2.0f * ez * tanf(fRad * 0.5f) / (float)height;
}

void Viewport3D::ActualizarLineaTransform(int mx, int my){
    if ((estado == rotacion || estado == EditScale) && ObjActivo){
        float px, py;
        if (ProyectarPunto(GizmoPivot(), px, py)){
            gTrackPivX = px; gTrackPivY = py;
            gTrackMouseX = (float)mx - (float)x;
            gTrackMouseY = (float)my - (float)y;
            gLineaValida = true;
        }
    }
}

void Viewport3D::RotarDesdeVista(int mx, int my){
    if (!ObjActivo) return;
    float px, py;
    if (!ProyectarPunto(GizmoPivot(), px, py)) return; // el angulo se mide desde el PIVOTE
    float lmx = (float)mx - (float)x, lmy = (float)my - (float)y;
    float ang = 180.0f - atan2f(lmy - py, lmx - px) * 180.0f / 3.14159265f;
    if (!gTrackballCap) { gTrackballCap = true; gTrackballAng0 = ang; }
    float delta = ang - gTrackballAng0;
    gAnguloTransform = delta; // barra de estado (angulo del mouse)
    Vector3 cf = viewRot * Vector3(0, 0, -1);
    // -delta: el usuario lo quiere al reves (mouse horario -> objeto horario)
    // Edit Mode: el trackball gira los VERTICES seleccionados (rotacion absoluta).
    if (InteractionMode == EditMode && EditXformActivo()) {
        EditXformRotAbs(Quaternion::FromAxisAngle(cf, -delta));
        return;
    }
    for (size_t o = 0; o < estadoObjetos.size(); o++) {
        Object& ob = *estadoObjetos[o].obj;
        // por la puerta: antes se derivaba el euler a mano con ToEulerYXZ mientras el resto del editor
        // usa XYZ -> lo que se guardaba NO era lo que se veia. SetRot lo deriva bien y conserva las vueltas.
        ob.SetRot(Quaternion::FromAxisAngle(cf, -delta) * estadoObjetos[o].rot);
    }
    AplicarPivotATransform(); // gira las posiciones alrededor del pivote
    { extern void SnapAjustarObjRot(); SnapAjustarObjRot(); } // imanta: el activo apunta al target (si snap ON)
}

// ---------------------------------------------------------------------------------------------------------------
//  VISTAS POR CUADRANTE (el telefono no tiene numpad: es su 1/3/7 de PC).
//  Toda vista ortogonal es pitch * yaw, con yaw en {0,90,180,270} (front/right/back/left) y pitch en {-90,0,90}
//  (top/lado/bottom). O sea: la vista es DOS ENTEROS, y moverse es sumarles.
//    - Si estabas orbitando libre, la primera flecha SOLO ENCUADRA al cuadrante mas cercano (no te saltea uno).
//    - izq/der: giran el yaw 90. Desde el top o el bottom tambien, que es "rotar la vista" desde arriba.
//    - arriba/abajo: suben/bajan el pitch, con TOPE en top y en bottom (de arriba no se sigue girando).
// ---------------------------------------------------------------------------------------------------------------
static Quaternion W3dVistaQuat(int yaw90, int pitch){   // yaw90 en pasos de 90; pitch en {-1,0,1}
    return Quaternion::FromAxisAngle(Vector3(1,0,0), (float)pitch * -90.0f)
         * Quaternion::FromAxisAngle(Vector3(0,1,0), (float)(yaw90 * 90));
}
// El cuadrante mas parecido a la orientacion actual. 'exacta' = ya estabas EN el (no hace falta encuadrar).
void Viewport3D::VistaCuadranteActual(int& yaw90, int& pitch, bool& exacta) const {
    // las 12 combinaciones (4 yaw x 3 pitch) son 12 orientaciones DISTINTAS: top girado 90 no es lo mismo que right.
    // Se prueban todas y gana la mas parecida.
    float mejor = -1.0f; yaw90 = 0; pitch = 0;
    for (int p = -1; p <= 1; p++){
        for (int y = 0; y < 4; y++){
            Quaternion q = W3dVistaQuat(y, p);
            // |dot| entre unitarios: 1 = misma orientacion (q y -q son la misma rotacion, de ahi el abs)
            float d = q.x*viewRot.x + q.y*viewRot.y + q.z*viewRot.z + q.w*viewRot.w;
            if (d < 0.0f) d = -d;
            if (d > mejor){ mejor = d; yaw90 = y; pitch = p; }
        }
    }
    exacta = (mejor > 0.9999f);   // coseno del medio angulo: pegado al cuadrante
}
void Viewport3D::VistaCuadranteNav(int dx, int dy){
    int yaw90, pitch; bool exacta;
    VistaCuadranteActual(yaw90, pitch, exacta);
    if (exacta){                       // ya encuadrado: la flecha MUEVE
        if (dx > 0) yaw90 = (yaw90 + 1) & 3;
        else if (dx < 0) yaw90 = (yaw90 + 3) & 3;
        if (dy < 0 && pitch < 1) pitch++;    // arriba: hacia el top, y ahi se planta
        else if (dy > 0 && pitch > -1) pitch--;
    }
    // si NO estabas encuadrado, la flecha solo te lleva al cuadrante mas cercano
    ViewFromCameraActive = false; camFrameOn = false; CameraToView = false;
    camViewZoom = 1.0f; camViewPanX = 0.0f; camViewPanY = 0.0f;
    viewRot = W3dVistaQuat(yaw90, pitch);
    viewRot.normalize();
    RecalcOrbitPosition();
    g_redraw = true;
}

void Viewport3D::SetViewpoint(Viewpoint value) {
    ViewFromCameraActive = false;
    camFrameOn = false; camFrameNX = 1.0f; camFrameNY = 1.0f;
    camViewZoom = 1.0f; camViewPanX = 0.0f; camViewPanY = 0.0f;
    CameraToView = false;

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
    // (`&& !W3dJuegoCorriendo()`) que apagaba los overlays al dar Play pasara lo que
    // pasara: el checkbox quedaba mentiroso (marcado y sin efecto) y no habia manera
    // de mirar la Curve del riel, los gizmos o los contornos con el juego andando,
    // que es justo cuando se los quiere ver. "El juego se ve limpio" ya lo resuelve
    // JuegoPrepararViewports(true), que apaga showOverlays AL INICIAR la partida: el
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
    W3dEscena3DLuces((int)(RenderType::Enum)view);
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
void Viewport3D::SetShowOverlays(bool valor) {
    showOverlays = valor;
}

// WEIGHT PAINT: en modo Weight Paint, prende el degradado de peso en el mesh ACTIVO (y lo apaga en el que se dejo de
// pintar). Recalcula el color cada frame (barato y refleja el cambio de grupo activo al instante). Fuera del modo,
// apaga el ultimo. Se llama antes de renderizar la escena.
static Mesh* g_wpMesh = NULL;
static void WeightPaintActualizar() {
    Mesh* target = NULL;
    if (InteractionMode == WeightPaint && ObjActivo && ObjActivo->getType() == ObjectType::mesh)
        target = (Mesh*)ObjActivo;
    if (g_wpMesh && g_wpMesh != target) g_wpMesh->weightPaintOn = false; // apaga el anterior
    g_wpMesh = target;
    if (target) {
        WeightPaintAsegurarMapa(target); // malla del editor recien creada: mapa render-vert -> control-point
        target->weightPaintOn = true; target->ConstruirColorPeso(target->grupoActivo);
    }
}

// ============================================================================
//  PINTURA DE PESOS en el viewport 3D (modo Weight Paint): click + drag pinta con
//  el pincel sobre el grupo ACTIVO del mesh activo. El trabajo real (falloff,
//  clamp, sparse, undo por trazo) vive en edit/WeightPaint.cpp; aca solo se
//  proyectan los vertices a pantalla (ProyectarPunto) y se rutean los eventos.
// ============================================================================
static bool g_wp3dPintando = false; // hay un trazo en curso en un viewport 3D (mouse apretado)

// posicion VIVA del cursor en modo pintura (-1 = todavia no se movio). lastMouseX/Y
// solo se refrescan al CLICKEAR (GuardarMousePos) o durante un drag (CheckWarpMouse):
// hoverando sin boton el circulo del pincel quedaba CONGELADO en el ultimo click.
// event_mouse_motion los actualiza en cada motion (solo en modo pintura).
static int g_wpCursorX = -1, g_wpCursorY = -1;

// modos de PINTURA del viewport 3D: el circulo del pincel sigue al cursor -> cada motion
// guarda la posicion y redibuja. Extensible: sumar aca vertex/texture paint cuando pinten
// con el mismo pincel. (El UV editor en UVModoPesos lleva su propio cursor lastMx/lastMy.)
static bool WP3DModoPintura() {
    return InteractionMode == WeightPaint;
}

struct WP3DCtx { Viewport3D* vp; Mesh* m; Matrix4 W; const GLfloat* pos; };

// proyector del 3D: posicion en pantalla del render-vert i (sobre la POSE visible, skinVertex
// si hay esqueleto). Solo vertices FRONT-FACING: el test barato normal-en-mundo vs direccion
// camara->vert (dot > 0 = mira para el otro lado -> no se pinta lo de atras). Con la malla
// posada la normal usada es la de BIND (no la rotada): aproximacion asumida, alcanza para
// descartar la cara de atras y evita rotar normales por hueso en cada pasada.
static bool WP3DProyectar(void* ctx, int i, float& sx, float& sy) {
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
    return c->vp->ProyectarPunto(wpos, sx, sy);
}

// una pasada del pincel en (mx,my) GLOBALES de pantalla (se pasan a locales del viewport)
static void WP3DPintar(Viewport3D* vp, int mx, int my) {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m) return;
    WP3DCtx c; c.vp = vp; c.m = m;
    // EFECTIVA a proposito (no GetWorldMatrixBase): el pincel decide que vertice toca
    // PROYECTANDOLO A PANTALLA, asi que la matriz tiene que ser la MISMA con la que se dibujo
    // la malla o el circulo pinta vertices que no estan abajo del cursor. Y como es efectiva
    // hay que bindear ESTA vista: WP3DProyectar ya usa vp->viewPos y vp->ProyectarPunto, y si
    // la matriz saliera de la vista que dibujo ultimo (el otro viewport) los pesos -- que se
    // serializan en el .w3d -- se escribirian en los vertices equivocados.
    vp->BindVista();
    m->GetWorldMatrix(c.W);
    c.pos = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex; // pintar sobre la pose visible
    BrushEstado& br = BrushGet();
    PincelAplicar(m, m->grupoActivo, (float)(mx - vp->x), (float)(my - vp->y),
                  br.radioPx, br.fuerza, br.modo == 0, WP3DProyectar, &c);
    g_redraw = true; // WeightPaintActualizar recalcula el color por frame -> feedback inmediato
}

// circulo del pincel siguiendo al mouse, SOLO en modo pintura y con el cursor sobre el
// CONTENIDO del viewport (no sobre la barra/toolbar ni con un menu/popup abierto).
static void WP3DRenderPincel(Viewport3D* vp) {
    if (!WP3DModoPintura()) return;
    if (PopUpActive || LayoutMenuAbierto()) return;
    // cursor VIVO (actualizado en cada motion); si todavia no se movio, el ultimo click
    int mx = (g_wpCursorX >= 0) ? g_wpCursorX : (int)lastMouseX;
    int my = (g_wpCursorY >= 0) ? g_wpCursorY : (int)lastMouseY;
    if (mx < vp->x || mx >= vp->x + vp->width || my < vp->y || my >= vp->y + vp->height) return;
    if (vp->OnBar(mx, my) || vp->OnToolbar(mx, my)) return;
    namespace gfx = w3dEngine;
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, vp->width, vp->height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Blend);
    BrushDibujarCirculo((float)(mx - vp->x), (float)(my - vp->y), BrushGet().radioPx);
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray); // restaurar para la UI que sigue
    gfx::Invalidate();
}

void Viewport3D::Render() {
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
        float ra = W3dAspectoJuego(); // declarado por la camara, o el del render (ver Camera.h)
        extern bool AnimEsJuego; extern bool PlayAnimation;
        camFrameLetterbox = (CameraActive->AspectoDeclarado() > 0.01f) &&
                            AnimEsJuego && (PlayAnimation || JuegoSimActiva());
        const float margin = camFrameLetterbox ? 1.0f : 0.92f; // jugando encuadrado: sin aire
        // LA cuenta del encuadre vive en main/render/EscenaRender.cpp: el runtime del
        // juego compilado llama a la MISMA, asi el .deb/APK encuadra igual que el Play.
        W3dEncuadreMarco(va, ra, margin, &camNX, &camNY);
        camFrameOn = true; camFrameNX = camNX; camFrameNY = camNY;
    }

    // al MIRAR POR LA CAMARA se usa SU lente (fov + ortografica PROPIOS de la camara, editables/animables);
    // en la orbita del visor se usa la del viewport (fovDeg global / orthographic del visor).
    float projFov = camFrame ? CameraActive->fov : fovDeg;
    bool  projOrtho = camFrame ? CameraActive->orthographic : orthographic;
    // distancia MINIMA/MAXIMA de dibujado: la de la camara al mirar por ella (animables), la del visor en la orbita
    float projNear = camFrame ? CameraActive->nearClip : nearClip;
    float projFar  = camFrame ? CameraActive->farClip  : farClip;

    // publicar la LENTE de esta vista al core (acompania al W3dVistaBind de UpdateViewOrbit):
    // el objeto Culling arma su frustum en CPU con estos numeros. Mirando por la camara se
    // publica el frustum del RENDER (fov de la camara + aspecto del render): es el del juego;
    // el aire del passepartout queda afuera a proposito (se cull-ea lo que el juego cull-earia).
    // LA PROYECCION la arma EscenaRender (compartida con el runtime del juego): asi el
    // encuadre, el frustum y la lente publicada al Core son bit a bit los mismos en el
    // Play del editor y en el .deb/APK compilado. Lo unico propio del editor que entra
    // aca es la INSPECCION (zoom/pan de la vista de camara), que en el juego vale 1/0/0.
    // En ORTOGRAFICA la distancia camara->objeto NO cambia el tamaño aparente: el "zoom" es la escala del
    // volumen visible (glOrtho), no la distancia. Atamos el half-height a orbitDistance*tan(fov/2) para que
    // zoomear agrande/achique igual que en perspectiva (y al alternar perspectiva/orto el objeto quede del
    // mismo tamaño). El far se corre con la distancia asi el objeto no se corta por el far al alejarse.
    W3dVista3D vst;
    vst.cam           = VistaCam();
    vst.fov           = projFov;
    vst.nearC         = projNear;
    vst.farC          = projOrtho ? (orbitDistance + projFar) : projFar;
    vst.orto          = projOrtho;
    vst.ortoSize      = orbitDistance * tanf(projFov * 0.5f * 3.14159265f / 180.0f);
    if (vst.ortoSize < 0.001f) vst.ortoSize = 0.001f;
    vst.aspectoVista  = (aspect > 1e-4f) ? aspect : 1.0f;
    vst.aspectoImagen = camFrame ? W3dAspectoJuego() : vst.aspectoVista;
    vst.marcoNX       = camNX;
    vst.marcoNY       = camNY;
    if (camFrame) { vst.zoom = camViewZoom; vst.panX = camViewPanX; vst.panY = camViewPanY; }
    W3dEscena3DProyeccion(vst);   // (deja el MatrixMode en ModelView con identidad)

    // Limpiar pantalla
    const int glY = W3dPantallaAlto - y - height; // arbol arriba-izq -> GL
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, width, height);

    // ZBuffer: fog de profundidad con rango FIJO en unidades de mundo (NO depende de la escena ni del
    // zoom). Es SOLO la distancia camara->objeto: en la camara = blanco, a fogFar o mas lejos = negro.
    // Al mover/zoomear la camara cambian las distancias -> TODO se aclara al acercarse y se oscurece al
    // alejarse por igual (incluido el objeto mas cercano). Para afinar el contraste, cambiar fogFar.
    const float fogNear = nearClip; // ~camara = blanco
    const float fogFar  = 40.0f;    // a 40 de la camara (o mas lejos) = negro

    if (view == RenderType::ZBuffer) {
        w3dEngine::Enable(w3dEngine::Fog);
        w3dEngine::FogMode(true);
        w3dEngine::FogStart(fogNear); // rango FIJO: solo la distancia a la camara define el tono
        w3dEngine::FogEnd(fogFar);
        GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        w3dEngine::FogColor(fogColor);
    } else {
        w3dEngine::Disable(w3dEngine::Fog);
        w3dEngine::ClearColor(ListaColores[static_cast<int>(ColorID::background)][0], ListaColores[static_cast<int>(ColorID::background)][1],
                     ListaColores[static_cast<int>(ColorID::background)][2], ListaColores[static_cast<int>(ColorID::background)][3]);
    }

    if (limpiarPantalla) {
        if (view == RenderType::ZBuffer || view == RenderType::Alpha) {
            w3dEngine::ClearColor(0.0f, 0.0f, 0.0f, 1.0f); // ZBuffer y Alpha: fondo NEGRO
        } else if (view == RenderType::NormalView) {
            // Normal: fondo = AZUL de normal-map. Una normal que mira de frente a la camara es
            // (0,0,1) en view-space, que el Core codifica como (0.5,0.5,1.0). Asi el "vacio" se lee
            // como una superficie de frente, igual que la base de un normal map.
            w3dEngine::ClearColor(0.5f, 0.5f, 1.0f, 1.0f);
        } else if (view == RenderType::Rendered) {
            w3dEngine::ClearColor(g_renderBg[0], g_renderBg[1], g_renderBg[2], g_renderBg[3]); // fondo GLOBAL del render
        } else {
            // solid / wireframe / material / normal: fondo POR-VIEWPORT (bgSolido). Sentinel alpha<0 = tema.
            if (bgSolido[3] >= 0.0f)
                w3dEngine::ClearColor(bgSolido[0], bgSolido[1], bgSolido[2], bgSolido[3]);
            else
                w3dEngine::ClearColor(ListaColores[static_cast<int>(ColorID::background)][0], ListaColores[static_cast<int>(ColorID::background)][1],
                             ListaColores[static_cast<int>(ColorID::background)][2], ListaColores[static_cast<int>(ColorID::background)][3]);
        }
        w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);
    } else {
        w3dEngine::Clear(w3dEngine::DepthBuffer);
    }

    w3dEngine::Disable(w3dEngine::ScissorTest);
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
    W3dEscena3DModo((int)(RenderType::Enum)view);

    UpdateViewOrbit();

    w3dEngine::Enable(w3dEngine::DepthTest);

    // Dibujar overlays
    if (showOverlays) {
            w3dEngine::Material(w3dEngine::MatDiffuse,  ListaColores[static_cast<int>(ColorID::negro)]);
            w3dEngine::Material(w3dEngine::MatAmbient,  ListaColores[static_cast<int>(ColorID::negro)]);
            w3dEngine::Material(w3dEngine::MatSpecular, ListaColores[static_cast<int>(ColorID::negro)]);

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
        if (showFloor || showXaxis || showYaxis) RenderFloor();
        w3dEngine::DepthMask(true);
    }

    // Edit Mode: marcar la malla ACTIVA para que su render dibuje el overlay de
    // edicion (vertices como puntos + bordes encima) en vez del contorno de objeto.
    // (misma derivacion COMPARTIDA que usa el cambio de modo; PC la refresca por frame
    //  para seguir a ObjActivo, Symbian la setea al cambiar de modo)
    ActualizarEditMeshActivo();

    // master de overlays para el render del CORE (Mesh::Render lee g_mostrarOverlays para no dibujar
    // contornos de seleccion ni el overlay de edit). Se setea por frame = el showOverlays del viewport,
    // SIN cortarlo por el modo juego: "Show Overlays" significa lo mismo jugando que editando (ver el
    // comentario de ReloadLights). Al dar Play, JuegoPrepararViewports(true) ya lo deja apagado; si el
    // usuario lo prende, ve las curvas, los empties, los huesos, los gizmos y los contornos.
    const bool ovl = showOverlays;
    g_mostrarOverlays = ovl;
    w3dRenderOverlays = ovl; // el Core lo lee (Mesh::RenderObject); g_mostrarOverlays sigue siendo del editor
    // overlays por tipo (submenu "Objects"): del viewport -> globales que lee el traversal del Core (Empty/Camera/luz)
    g_showLights = showLights && ovl; g_showCamera = showCamera && ovl; g_showEmpty = showEmpty && ovl;

    // (los flags de dibujo del Core los dejo W3dEscena3DModo mas arriba, junto con la
    //  posicion de la luz: tienen que quedar puestos con la modelview en identidad.)

    // ZBuffer: RE-seteamos el fog de profundidad JUSTO antes de dibujar las mallas (RenderFloor lo pisa
    // con su propio fog). Asi las mallas se sombrean por profundidad: cerca claro -> lejos oscuro.
    if (view == RenderType::ZBuffer) {
        w3dEngine::Enable(w3dEngine::Fog);
        w3dEngine::FogMode(true);
        w3dEngine::FogStart(fogNear); // mismo rango FIJO que arriba
        w3dEngine::FogEnd(fogFar);
        GLfloat fogZ[] = {0.0f, 0.0f, 0.0f, 1.0f};
        w3dEngine::FogColor(fogZ);
    }

    WeightPaintActualizar(); // prende/apaga el degradado de peso en el mesh activo (modo Weight Paint)

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
      // EL PASE DE ESCENA vive en main/render/EscenaRender.cpp y lo llama TAMBIEN el
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
      W3dEscena3DPasada();
      // (P1) auditoria del cache de estado al CERRAR el pase de escena (solo si el
      // harness la prendio con 'glaudit'; en el frame normal cuesta cero): si algo
      // del pase toco GL crudo, aca se detecta y se acumula el desync.
      if (w3dEngine::g_auditarEscena) w3dEngine::g_auditDesyncs += w3dEngine::AuditarEstado();
      // cierre de las ESTADISTICAS del pase de escena de ESTE viewport (overlay)
      statTrisFrame    = (w3dEngine::g_statIndices      - _stTris0) / 3;
      statDrawsFrame   =  w3dEngine::g_statDrawTris     - _stDraw0;
      statBindsFrame   =  w3dEngine::g_statTexBinds     - _stBind0;
      statEstadosFrame =  w3dEngine::g_statStateChanges - _stEst0;
      g_prof.scene += W3dNowMs() - _tScn0; } // profiler: escena (skinning + modelos)

    // huesos encima de todo (ignoran z-buffer). Es OVERLAY del editor: se apaga con "Show Overlays",
    // con su propio toggle "Armature" del menu de overlays, o jugando (ovl ya corta por modo juego).
    if (ovl && showArmature) RenderArmaturasEncima(SceneCollection);
    // Motion Trail: mismo criterio (overlay del editor, en x-ray). Lo prende el menu Animation.
    if (ovl) RenderMotionTrail();

    LoopCutRenderPreview(); // preview del corte (loop cut) encima de la escena

    if (ovl) RenderOverlay();
    RenderCamPassepartout(); // marco de camara (lo que se va a renderizar) + oscurecido afuera. Antes de la UI (queda debajo).
    // SIEMPRE: RenderUI es el chrome del area (toolbar/menus/bordes) Y el reseteo del
    // estado GL 2D tras las mallas. La opcion ShowUi que lo salteaba se dio de baja:
    // sin chrome el viewport quedaba inusable y el estado sucio (CULL_FACE + arrays
    // prendidos) contaminaba el proximo dibujo 2D (los errores graficos reportados).
    RenderUI();
    RenderSnapIndicador(); // recuadro verde en el target de snap (encima de todo)
    WP3DRenderPincel(this); // circulo del pincel (modo Weight Paint), siguiendo al mouse
    g_prof.viewport3d += W3dNowMs() - _tVp0; // profiler: cierra el tiempo de este viewport 3D
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
    static std::vector<GLfloat> claro, oscuro, puntos;
    claro.clear(); oscuro.clear(); puntos.clear();
    std::vector<Vector3> pts; std::vector<int> keys; int desde, hasta;
    // ---- POSE MODE: el camino de los HUESOS seleccionados (el trail del objeto armature no dice nada: lo que
    //      se mueve son los huesos). Los puntos vienen en espacio NODO -> se pasan a mundo con el world del
    //      armature, igual que se dibujan los huesos.
    if (InteractionMode == PoseMode && ObjActivo && ObjActivo->getType() == ObjectType::armature){
        Armature* a = (Armature*)ObjActivo;
        Matrix4 AW; a->GetWorldMatrix(AW);
        for (size_t b = 0; b < a->bones.size(); b++){
            if (!(a->bones[b].select || (int)b == a->boneActivo)) continue;
            if (!MotionTrailHuesoNodo(a, (int)b, pts, keys, desde, hasta)) continue;
            for (size_t i = 0; i < pts.size(); i++) pts[i] = AW * pts[i];
            for (size_t i = 1; i < pts.size(); i++){
                std::vector<GLfloat>& dst = ((i & 1) ? claro : oscuro);
                dst.push_back(pts[i-1].x); dst.push_back(pts[i-1].y); dst.push_back(pts[i-1].z);
                dst.push_back(pts[i].x);   dst.push_back(pts[i].y);   dst.push_back(pts[i].z);
            }
            for (size_t k = 0; k < keys.size(); k++){
                int idx = keys[k] - desde;
                if (idx < 0 || idx >= (int)pts.size()) continue;
                puntos.push_back(pts[idx].x); puntos.push_back(pts[idx].y); puntos.push_back(pts[idx].z);
            }
        }
    }
    else
    for (size_t s2 = 0; s2 < ObjSelects.size(); s2++){
        if (!MotionTrailDe(ObjSelects[s2], pts, keys, desde, hasta)) continue;
        // tramos: uno por FRAME, alternando el buffer -> claro/oscuro/claro...
        for (size_t i = 1; i < pts.size(); i++){
            std::vector<GLfloat>& dst = ((i & 1) ? claro : oscuro);
            dst.push_back(pts[i-1].x); dst.push_back(pts[i-1].y); dst.push_back(pts[i-1].z);
            dst.push_back(pts[i].x);   dst.push_back(pts[i].y);   dst.push_back(pts[i].z);
        }
        // un punto por KEYFRAME (no por frame)
        for (size_t k = 0; k < keys.size(); k++){
            int idx = keys[k] - desde;
            if (idx < 0 || idx >= (int)pts.size()) continue;
            puntos.push_back(pts[idx].x); puntos.push_back(pts[idx].y); puntos.push_back(pts[idx].z);
        }
    }
    if (claro.empty() && oscuro.empty()) return;

    GLboolean luz = gfx::IsEnabled(gfx::Lighting);
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D);
    gfx::Disable(gfx::DepthTest);        // X-RAY: encima de todo, como los huesos
    gfx::DisableArray(gfx::NormalArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::LineWidth(2.0f);
    if (!oscuro.empty()){
        gfx::Color4f(0.30f, 0.30f, 0.35f, 1.0f);
        gfx::VertexPointer3f(0, &oscuro[0]);
        gfx::DrawLines((int)(oscuro.size() / 3));
    }
    if (!claro.empty()){
        gfx::Color4f(0.95f, 0.95f, 1.00f, 1.0f);
        gfx::VertexPointer3f(0, &claro[0]);
        gfx::DrawLines((int)(claro.size() / 3));
    }
    if (!puntos.empty()){                // los KEYFRAMES: mismo tamaño que los vertices del Edit Mode
        gfx::PointSize(6.0f);   // el MISMO que los vertices del Edit Mode
        const float* acc = ListaColores[static_cast<int>(ColorID::accent)];
        gfx::Color4f(acc[0], acc[1], acc[2], 1.0f);
        gfx::VertexPointer3f(0, &puntos[0]);
        gfx::DrawPoints((int)(puntos.size() / 3));
        gfx::PointSize(1.0f);
    }
    gfx::LineWidth(1.0f);
    gfx::Enable(gfx::DepthTest);
    gfx::EnableArray(gfx::NormalArray);
    if (luz) gfx::Enable(gfx::Lighting);
    gfx::Invalidate();
}

void Viewport3D::RenderArmaturasEncima(Object* node){
    if (!node) return;
    if (node->visible && node->getType() == ObjectType::armature){
        Armature* arm = static_cast<Armature*>(node);
        if (!arm->bones.empty()){
            namespace gfx = w3dEngine;
            EvaluarPoseEsqueleto(arm, CurrentFrame); // pose animada (o rest si no hay clip) -> poseHead/poseTail
            Matrix4 W; arm->GetWorldMatrix(W);
            gfx::MatrixMode(gfx::ModelView);
            gfx::PushMatrix();
            gfx::MultMatrix(W.m);
            GLboolean luz = gfx::IsEnabled(gfx::Lighting);
            gfx::Disable(gfx::Lighting);
            gfx::Disable(gfx::Texture2D);
            gfx::Disable(gfx::DepthTest);   // encima de todo
            gfx::DisableArray(gfx::NormalArray);
            gfx::DisableArray(gfx::ColorArray);
            gfx::DisableArray(gfx::TexCoordArray);
            // huesos: linea SOLIDA head->tail (azul). Ademas, para cada hueso cuyo head NO coincide con el tail de su
            // padre (hueso emparentado pero "separado"), una linea PUNTEADA padre.tail->head para que quede clara la
            // conexion en la jerarquia (igual que Blender). El punteado se fabrica a mano (GLES no tiene line stipple).
            // POSE MODE: cada hueso se colorea segun seleccion -> sin seleccionar AZUL, seleccionado VERDE, activo BLANCO
            // (multi-seleccion). En Object Mode todos van a 'buf' con el color del armature (verde/azul de objeto).
            // EDIT MODE de armature (Fase 3): mismos colores por seleccion PERO sobre head/tail CRUDOS (el rest que
            // se edita, no la pose) + PUNTOS en cada extremo (los agarraderos del drag).
            bool poseMode = (InteractionMode == PoseMode && arm == (Armature*)ObjActivo);
            bool editArm  = (InteractionMode == EditMode && arm == (Armature*)ObjActivo);
            std::vector<GLfloat> buf, dash, bufSel, bufAct; buf.reserve(arm->bones.size() * 6);
            std::vector<GLfloat> pts, ptsSel; // extremos (solo editArm): normal / del hueso seleccionado-activo
            for (size_t i = 0; i < arm->bones.size(); i++){
                const W3dBone& b = arm->bones[i];
                Vector3 H = editArm ? b.head : b.poseHead;
                Vector3 T = editArm ? b.tail : b.poseTail;
                std::vector<GLfloat>* dst = &buf;
                if (poseMode || editArm){
                    if ((int)i == arm->boneActivo) dst = &bufAct;   // activo = blanco
                    else if (b.select)             dst = &bufSel;   // seleccionado = verde
                    // sino queda en buf (azul, ver mas abajo)
                }
                dst->push_back(H.x); dst->push_back(H.y); dst->push_back(H.z);
                dst->push_back(T.x); dst->push_back(T.y); dst->push_back(T.z);
                if (editArm){ // agarraderos: un punto por punta. Se resalta CADA punta seleccionada
                    // (seleccion por puntas estilo Blender): hueso entero = las dos prendidas.
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
            bool activo = (arm == (Armature*)ObjActivo);
            const float* col;
            if (poseMode || editArm){ static const float azulH[4] = {0.28f, 0.55f, 1.0f, 1.0f}; col = azulH; } // sin seleccionar = AZUL (el negro se perdia en el fondo)
            else if (activo)         col = ListaColores[(int)ColorID::accent];
            else if (arm->select)    col = ListaColores[(int)ColorID::accentDark];
            else { static const float azul[4] = {0.28f, 0.55f, 1.0f, 1.0f}; col = azul; }
            gfx::LineWidth(2.0f);
            if (!buf.empty()){
                gfx::Color4f(col[0], col[1], col[2], 1.0f);
                gfx::VertexPointer3f(0, &buf[0]);
                gfx::DrawLines((int)(buf.size() / 3));
            }
            // huesos SELECCIONADOS (Pose Mode): VERDE, mas gruesos, encima.
            if (!bufSel.empty()){
                const float* sc = ListaColores[(int)ColorID::accent];
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
            // OBJETOS (RenderRelantionshipsLines): la misma textura de puntos chicos
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
                SetColorID(ColorID::grisUI);
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
            if (luz) gfx::Enable(gfx::Lighting);
            gfx::PopMatrix();
            gfx::Invalidate();
        }
    }
    for (size_t c = 0; c < node->Childrens.size(); c++)
        RenderArmaturasEncima(node->Childrens[c]);
}

// SNAP: recuadro verde (solo borde) en el target bajo el cursor mientras se mueve con snap ON. Asi sabes
// que esta enganchando y donde. g_snapSx/Sy son coords del viewport (las setea SnapBuscarTarget/SnapAjustar).
void Viewport3D::RenderSnapIndicador(){
    if (!g_snap.enabled || !g_snapHit) return;
    if (!(Viewport3DActive == this && (estado==translacion||estado==rotacion||estado==EditScale))) return;
    namespace gfx = w3dEngine;
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);
    float h = 6.0f; // recuadro de ~12px (6 de medio)
    float x0 = g_snapSx - h, x1 = g_snapSx + h, y0 = g_snapSy - h, y1 = g_snapSy + h;
    gfx::Color4f(0.15f, 1.0f, 0.2f, 1.0f); // verde
    gfx::LineWidth(2.0f);
    GLfloat b[] = { x0,y0, x1,y0,  x1,y0, x1,y1,  x1,y1, x0,y1,  x0,y1, x0,y0 };
    gfx::VertexPointer2f(0, b);
    gfx::DrawLines(8);
    gfx::LineWidth(1.0f);
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
    gfx::Invalidate();
}

// PASSEPARTOUT de camara: al mirar DESDE la camara, dibuja el BORDE BLANCO del render y OSCURECE afuera (0.5 negro).
// Lo de adentro del marco es EXACTAMENTE lo que va a salir en el render. Se adapta al viewport y al aspecto del render.
void Viewport3D::RenderCamPassepartout(){
    if (!camFrameOn) return;
    namespace gfx = w3dEngine;
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
    // runtime del juego (main/render/EscenaRender.cpp). EN EL EDITOR van ATENUADAS
    // (negro 0.5: se ve que hay afuera y se puede auditar el culling, reporte del
    // dueno); NEGRO OPACO -lo que ve el jugador en el aparato, y lo que el juego
    // compilado dibuja siempre- solo con el toggle 'letterboxNegro' prendido
    // (preview del dispositivo; lo usan las pruebas encuadrepx).
    W3dEscena3DBandas(W, H, camFrameNX, camFrameNY, camViewZoom, camViewPanX, camViewPanY,
                      camFrameLetterbox && letterboxNegro);
    // el marco sigue el zoom/pan de INSPECCION (misma transform que la proyeccion). NDC->pixeles (y hacia abajo).
    float cx = (camViewPanX * 0.5f + 0.5f) * W;
    float cy = (0.5f - camViewPanY * 0.5f) * H;
    float hw = camFrameNX * camViewZoom * W * 0.5f, hh = camFrameNY * camViewZoom * H * 0.5f;
    float x0 = cx - hw, x1 = cx + hw, y0 = cy - hh, y1 = cy + hh;
    // BORDE BLANCO del marco = lo que se va a renderizar. Con el JUEGO corriendo
    // no se dibuja (el juego se ve limpio); con Stop vuelve.
    if (!(AnimEsJuego && JuegoSimActiva())) {
        gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
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
int Viewport3D::TilesNecesarios(int outW, int outH) const {
    if (outW <= 0 || outH <= 0) return 0;
    int capW = (width  > 0) ? width  : outW;
    int capH = (height > 0) ? height : outH;
    int tileW = (outW < capW) ? outW : capW;
    int tileH = (outH < capH) ? outH : capH;
    if (tileW <= 0 || tileH <= 0) return 0;
    return ((outW + tileW - 1) / tileW) * ((outH + tileH - 1) / tileH);
}

#ifdef __EMSCRIPTEN__
extern "C" void WebDescargarArchivo(const char* path, const char* name); // main.cpp (EM_JS): baja un archivo del FS al disco
#endif
bool Viewport3D::RenderAPNG(int outW, int outH, RenderType::Enum pass, const char* filename, int progBase, int progTotal){
    if (outW <= 0 || outH <= 0 || !SceneCollection) return false;

    // tope de tile = lo que entra seguro en el framebuffer (el viewport ya entra en la ventana).
    int capW = (width  > 0) ? width  : outW;
    int capH = (height > 0) ? height : outH;
    int tileW = (outW < capW) ? outW : capW;
    int tileH = (outH < capH) ? outH : capH;
    if (tileW <= 0 || tileH <= 0) return false;

    unsigned char* full = new unsigned char[(size_t)outW * outH * 4];
    unsigned char* tile = new unsigned char[(size_t)tileW * tileH * 4];

    // extension del frustum de la imagen COMPLETA (aspect = outW/outH, NO el del viewport)
    float aspectR = (float)outW / (float)outH;
    float top, bottom, left, right;
    if (orthographic){
        // igual que el viewport: el tamaño del volumen orto sigue el zoom (orbitDistance), no un valor fijo
        float size = orbitDistance * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
        if (size < 0.001f) size = 0.001f;
        top = size; bottom = -size; right = size * aspectR; left = -right;
    } else {
        top = nearClip * tanf(fovDeg * 0.5f * 3.14159265f / 180.0f);
        bottom = -top; right = top * aspectR; left = -right;
    }

    // fijar el PASE (sin overlay): modo + luces + flags del Core, una sola vez
    RenderType viewPrev = view; bool overlaysPrev = showOverlays;
    bool camPrev = ViewFromCameraActive;
    // el render se hace DESDE la camara activa (como F12 en Blender): si hay camara, forzamos su POV.
    // Sin camara queda la vista del viewport (orbita). La proyeccion sigue con fovDeg (la Camera del editor
    // no tiene lente propio todavia). El gizmo de la camara no sale (showOverlays=false + este flag).
    if (CameraActive) ViewFromCameraActive = true;
    view = pass; showOverlays = false;
    ReloadLights();
    g_mostrarOverlays = false; w3dRenderOverlays = false;
    w3dRenderWireframe   = false;
    w3dRenderSolido      = (pass == RenderType::Solid);
    w3dRenderNormalColor = (pass == RenderType::NormalView);
    w3dRenderSinLuz      = (pass == RenderType::ZBuffer);
    w3dRenderLuces       = (pass == RenderType::Rendered);
    w3dRenderAlpha       = (pass == RenderType::Alpha);

    // color de fondo del pase
    float bg[4];
    if (pass == RenderType::ZBuffer || pass == RenderType::Alpha){ bg[0]=bg[1]=bg[2]=0.0f; bg[3]=1.0f; } // NEGRO (matte blanco sobre negro)
    else if (pass == RenderType::NormalView){ bg[0]=0.5f; bg[1]=0.5f; bg[2]=1.0f; bg[3]=1.0f; } // AZUL normal-map: normal (0,0,1) de frente = (0.5,0.5,1.0)
    else if (pass == RenderType::Rendered){
        bg[0]=g_renderBg[0]; bg[1]=g_renderBg[1]; bg[2]=g_renderBg[2]; bg[3]=g_renderBg[3]; // color de fondo GLOBAL del render
    } else { bg[0]=bg[1]=bg[2]=0.0f; bg[3]=0.0f; }                                       // resto: transparente (composicion)

    // tiles en coords BOTTOM-LEFT (como GL); el flip vertical lo hace SavePNG al final
    int tileIdx = 0; // para la barra: frac = (progBase + tiles hechos) / progTotal
    for (int ty = 0; ty < outH; ty += tileH){
        int th = (ty + tileH <= outH) ? tileH : (outH - ty);
        for (int tx = 0; tx < outW; tx += tileW){
            int tw = (tx + tileW <= outW) ? tileW : (outW - tx);

            float l = left   + (right  - left)   * (float)tx        / (float)outW;
            float r = left   + (right  - left)   * (float)(tx + tw) / (float)outW;
            float b = bottom + (top    - bottom) * (float)ty        / (float)outH;
            float t = bottom + (top    - bottom) * (float)(ty + th) / (float)outH;

            w3dEngine::MatrixMode(w3dEngine::Projection);
            w3dEngine::LoadIdentity();
            if (orthographic) w3dEngine::Ortho(l, r, b, t, nearClip, orbitDistance + farClip); // far corrido: no cortar el objeto al alejar
            else              w3dEngine::Frustum(l, r, b, t, nearClip, farClip);

            w3dEngine::Viewport(0, 0, tw, th); // el tile en la esquina del framebuffer

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
                w3dEngine::Enable(w3dEngine::Fog); w3dEngine::FogMode(true);
                w3dEngine::FogStart(nearClip); w3dEngine::FogEnd(40.0f);
                GLfloat fz[4] = {0.0f, 0.0f, 0.0f, 1.0f}; w3dEngine::FogColor(fz);
            } else {
                w3dEngine::Disable(w3dEngine::Fog);
            }

            w3dEngine::ClearColor(bg[0], bg[1], bg[2], bg[3]);
            w3dEngine::Clear(w3dEngine::ColorBuffer | w3dEngine::DepthBuffer);

            W3dParticulasLimpiarPendientes();
    W3dDecalesLimpiarPendientes();   // las sombras sueltas van al final del frame
    W3dLucesLimpiarPendientes();     // idem las chispas/halos aditivos
            w3dEngine::Invalidate(); // (P1) resync una vez por pase, como en el viewport
            w3dLoteStamp++;          // (P4) sello del pase (lote estatico)
            SceneCollection->Render();
            // CALCOMANIAS SUELTAS (sombras): recien ahora, con toda la escena opaca ya
    // dibujada. Antes se dibujaban en su lugar del arbol y cualquier opaco
    // posterior (troncos, EscenarioAlpha) las borraba: no escriben z a proposito.
    W3dDecalesDibujarPendientes();
    W3dLucesDibujarPendientes();        // chispas/halos aditivos (tambien en el render a PNG)
    W3dParticulasDibujarPendientes();   // translucidas despues de los opacos (tambien en el render a PNG)

            w3dEngine::ReadPixelsRGBA(0, 0, tw, th, tile);
            for (int row = 0; row < th; row++){
                memcpy(full + (size_t)((ty + row) * outW + tx) * 4,
                       tile + (size_t)(row * tw) * 4,
                       (size_t)tw * 4);
            }
            // barra de progreso: redibujo COMPLETO por tile (el render pisa el framebuffer). progTotal
            // cuenta pases x tiles (lo arma AccionRenderImage); progBase = tiles de los pases anteriores.
            ++tileIdx;
            if (progTotal > 0) ProgresoActualizarFull((float)(progBase + tileIdx) / (float)progTotal);
        }
    }

    // restaurar el estado del viewport (los flags de pase se re-setean solos en el proximo Render,
    // pero los limpiamos ya por las dudas de que algo dibuje antes)
    view = viewPrev; showOverlays = overlaysPrev; ViewFromCameraActive = camPrev;
    w3dRenderAlpha = false; w3dRenderSinLuz = false; w3dRenderNormalColor = false;
    ReloadLights();

    bool ok = w3dEngine::SavePNG(filename, full, outW, outH, true);
#ifdef __EMSCRIPTEN__
    if (ok) {
        // web: bajar el PNG al disco del usuario (el FS de emscripten es virtual)
        std::string nm = filename;
        size_t sl = nm.find_last_of("/\\"); if (sl != std::string::npos) nm = nm.substr(sl + 1);
        WebDescargarArchivo(filename, nm.c_str());
    }
#endif
    delete[] full;
    delete[] tile;
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
    w3dEngine::FogMode(true);
    w3dEngine::FogStart(nearClip);
    w3dEngine::FogEnd(30.0f);

    if (view == RenderType::Rendered) {
        w3dEngine::FogColor(scene->backgroundColor);
    } else {
        w3dEngine::FogColor(ListaColores[static_cast<int>(ColorID::background)]);
    }

    w3dEngine::LineWidth(1);
    w3dEngine::VertexPointer3f(0, objVertexdataFloor);

    // la grilla solo si "Floor" esta activo (antes se dibujaba siempre: por eso
    // apagar Floor no la ocultaba)
    if (showFloor) {
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPiso][0],
            W3dColores[W3dColor_LineaPiso][1],
            W3dColores[W3dColor_LineaPiso][2],
            W3dColores[W3dColor_LineaPiso][3]
        );
        W3dDrawLinesF(objVertexdataFloor, objFacedataFloor, objFacesFloor);
    }

    // Linea Roja
    if (showXaxis) {
        w3dEngine::LineWidth(2);
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPisoRoja][0],
            W3dColores[W3dColor_LineaPisoRoja][1],
            W3dColores[W3dColor_LineaPisoRoja][2],
            W3dColores[W3dColor_LineaPisoRoja][3]
        );
        W3dDrawLinesF(objVertexdataFloor, EjeRojo, 2);
        w3dEngine::LineWidth(1);
    } else if (showFloor) {
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPiso][0],
            W3dColores[W3dColor_LineaPiso][1],
            W3dColores[W3dColor_LineaPiso][2],
            W3dColores[W3dColor_LineaPiso][3]
        );
        W3dDrawLinesF(objVertexdataFloor, EjeRojo, 2);
    }

    // Linea Verde
    if (showYaxis) {
        w3dEngine::LineWidth(2);
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPisoVerde][0],
            W3dColores[W3dColor_LineaPisoVerde][1],
            W3dColores[W3dColor_LineaPisoVerde][2],
            W3dColores[W3dColor_LineaPisoVerde][3]
        );
        W3dDrawLinesF(objVertexdataFloor, EjeVerde, 2);
        w3dEngine::LineWidth(1);
    } else if (showFloor) {
        w3dEngine::Color4f(
            W3dColores[W3dColor_LineaPiso][0],
            W3dColores[W3dColor_LineaPiso][1],
            W3dColores[W3dColor_LineaPiso][2],
            W3dColores[W3dColor_LineaPiso][3]
        );
        W3dDrawLinesF(objVertexdataFloor, EjeVerde, 2);
    }

    w3dEngine::Disable(w3dEngine::Fog);
}

// linea-guia por 'c' a lo largo de 'dir' (world space, larga a ambos lados)
static void DibujarLineaEjeMundo(const Vector3& c, const Vector3& dir, int colorIdx) {
    const float L = 1000.0f;
    GLfloat v[6] = {
        c.x - dir.x * L, c.y - dir.y * L, c.z - dir.z * L,
        c.x + dir.x * L, c.y + dir.y * L, c.z + dir.z * L
    };
    w3dEngine::Color4fv(W3dColores[colorIdx]);
    w3dEngine::VertexPointer3f(0, v);
    w3dEngine::DrawLines(2);
}

void Viewport3D::RenderAllAxisTransform() {
    if (!ObjActivo) return;
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
        DibujarLineaEjeMundo(c, gTransformNormal, W3dColor_ColorTransformZ);
        return;
    }
    int a = axisSelect;
    bool drawX = (a == X || a == XYZ || a == PlaneY || a == PlaneZ);
    bool drawY = (a == Y || a == XYZ || a == PlaneX || a == PlaneZ);
    bool drawZ = (a == Z || a == XYZ || a == PlaneX || a == PlaneY);
    // EDIT de ARMATURE (G/R/S de huesos): los ejes son los del transform de puntas (en "Local", los del
    // hueso ACTIVO con Y a lo largo del hueso). La guia dibuja EXACTAMENTE el eje con el que se mueve.
    { Vector3 bex, bey, bez;
      if (BoneEjesMundo(bex, bey, bez)){
        if (drawX) DibujarLineaEjeMundo(c, bex, W3dColor_ColorTransformX);
        if (drawY) DibujarLineaEjeMundo(c, bey, W3dColor_ColorTransformY);
        if (drawZ) DibujarLineaEjeMundo(c, bez, W3dColor_ColorTransformZ);
        return;
      } }
    // POSE MODE: los ejes son los del HUESO (en "Local" el eje local es el del hueso, NO el del objeto armature).
    // Se piden los MISMOS que usa el transform de pose -> la guia coincide siempre con la rotacion real.
    { extern bool PoseEjesMundo(Vector3&, Vector3&, Vector3&);
      Vector3 pex, pey, pez;
      if (PoseEjesMundo(pex, pey, pez)){
        if (drawX) DibujarLineaEjeMundo(c, pex, W3dColor_ColorTransformX);
        if (drawY) DibujarLineaEjeMundo(c, pey, W3dColor_ColorTransformY);
        if (drawZ) DibujarLineaEjeMundo(c, pez, W3dColor_ColorTransformZ);
        return;
      } }
    if (drawX) DibujarLineaEjeMundo(c, EjeOrientado(*ObjActivo, X), W3dColor_ColorTransformX);
    if (drawY) DibujarLineaEjeMundo(c, EjeOrientado(*ObjActivo, Y), W3dColor_ColorTransformY);
    if (drawZ) DibujarLineaEjeMundo(c, EjeOrientado(*ObjActivo, Z), W3dColor_ColorTransformZ);
}

void Viewport3D::RenderOverlay() {
    w3dEngine::Material(w3dEngine::MatDiffuse,  ListaColores[static_cast<int>(ColorID::negro)]);
    w3dEngine::Material(w3dEngine::MatAmbient,  ListaColores[static_cast<int>(ColorID::negro)]);
    w3dEngine::Material(w3dEngine::MatSpecular, ListaColores[static_cast<int>(ColorID::negro)]);

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

        if (ShowRelantionshipsLines) RenderRelantionshipsLines();
        RenderIcons3D();
        // "Ver en 3D" de la UI: los elementos 2D metidos EN la escena, con su profundidad Z
        if (UI2D_HayVerEn3D()) {
            UI2D_DibujarEnMundo();
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
            (estado == translacion || estado == rotacion || estado == EditScale))
            RenderAllAxisTransform();
    }

    if (show3DCursor) Render3Dcursor();

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

void Viewport3D::RenderRelantionshipsLines() {
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::DepthMask(false);
    w3dEngine::TexCoordPointer2f(0, lineUV);
    SetColorID(ColorID::grisUI);
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
    SetColorID(ColorID::grisUI);
    W3dDrawLinesF(Cursor3DVertices, Cursor3DEdges, Cursor3DEdgesSize);

    w3dEngine::PopMatrix();
}

// El boton de CONTEXTO de la barra: es Object / Pose / Vertex / Edge / Face segun donde estes. Sin texto (comia
// ancho de barra): lo dice el ICONO, y el titulo de su menu lo confirma al abrirlo.
// FUERA del render a proposito: esto es ESTADO, no dibujo. Mientras vivio adentro de RenderUI -que es GL y no corre
// headless- era intesteable, y de hecho el texto se seguia reescribiendo por frame pisando al que lo sacaba.
void Viewport3D::SyncBotonContexto(){
    Button* bObj = BarRolBtn(BarButtons, BR_Object);
    if (!bObj) return;
    bool esMesh  = (ObjActivo && ObjActivo->getType() == ObjectType::mesh);
    bool esArm   = (ObjActivo && ObjActivo->getType() == ObjectType::armature);
    bool enEdit  = (esMesh && InteractionMode == EditMode);
    bool poseM   = (InteractionMode == PoseMode && esArm);
    bool editArm = (InteractionMode == EditMode && esArm); // Edit de huesos: el boton abre el menu "Armature"
    bObj->visible = HayObjetosSeleccionados() || poseM;
    bObj->text.clear();
    bObj->icon = (poseM || editArm) ? (int)IconType::armature :
        !enEdit ? (int)IconType::object :
        (EditSelectMode == SelEdge) ? (int)IconType::selEdge :
        (EditSelectMode == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
}

void Viewport3D::RenderUI() {
    // barra de botones 2D del area: NO es un overlay, es chrome del area (como
    // los bordes). Se dibuja SIEMPRE, aunque "Show Overlays" este off (la opcion
    // ShowUi que lo salteaba se dio de baja: dejaba el viewport sin chrome y el
    // estado GL 2D sin resetear). (Usa el viewport ya seteado en Render().)
    {
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
        // la INTERFAZ 2D del juego/programa, dibujada sobre la PANTALLA DEL JUEGO (no sobre
        // el viewport entero, que era lo que la dejaba "aplastada": estirar el lienzo al
        // rect del viewport escala X e Y con factores distintos y ademas descuadra el HUD
        // del render 3D, que SI se letterboxea al aspecto del render). El mapeo de aca es
        // SIEMPRE uniforme: un elemento de W x H px conserva su proporcion en cualquier
        // resolucion/aspecto y crece con la ALTURA de la pantalla (como la UI de PS1).
        { int arri = barAbajo ? 0 : BarTopOffset();
          // la toolbar solo resta area si de verdad se dibuja (en modo juego se oculta)
          int abaj = (ToolbarVisible() ? ToolbarHeight() : 0) + (barAbajo ? BarTopOffset() : 0);
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
          UI2D_BaseRecorte(x, y, width, height);   // el overflow recorta con scissor absoluto
          UI* uJuego = UI2D_UIDelEditor();
          bool dinamica = (!uJuego || uJuego->igualQueRender);
          if (dinamica && AnimEsJuego) {
              // 2a) MODO JUEGO con UI "igual que el render": la pantalla ES el lienzo
              //     (override, exactamente la rama igualQueRender de w3drun donde la
              //     ventana real es el lienzo) -> mapeo 1:1, tamModo 2 escala con
              //     min(w,h)/480 de ESTA pantalla (en apaisado: la ALTURA / 480).
              //     (P5) ESTADO POR VIEWPORT: el override PERSISTENTE -- el que leen
              //     los binds del juego (pantalla()/pantallaDe) entre frames -- lo
              //     publica SOLO el viewport ACTIVO; el dibujo de ESTE viewport usa
              //     un override CON ALCANCE que al salir repone lo que habia. Antes
              //     cada viewport lo pisaba al dibujar (el ultimo ganaba) y con dos
              //     viewports 3D de tamanos distintos la UI del juego se re-armaba a
              //     cada rato con una pantalla distinta ("se agranda y achica como
              //     loca, independiente por cada viewport3d").
              if (Viewport3DActive == this) UI2D_OverrideVentana(this, sw, sh);
              UI2D_OverrideVentanaScope ventana(this, sw, sh);
              hudX0 = sx0; hudY0 = sy0; hudW = sw; hudH = sh; hudEsc = 1.0f; hudOverride = true;
              UI2D_DibujarOverlay(sx0, sy0, sw, sh, 1.0f, g_hudCapturaPos, true);
          } else {
              // 2b) lienzo con tamano propio, o EDICION (el lienzo sigue siendo el del
              //     render, asi el Editor 2D no cambia): ENCAJARLO (letterbox) con escala
              //     UNIFORME min(sw/lw, sh/lh), la misma cuenta que w3drun / Editor2D.
              UI2D_OverrideVentanaQuitar(this);
              float lw, lh; UI2D_TamanoLienzo(&lw, &lh);
              if (lw < 1.0f) lw = 1.0f; if (lh < 1.0f) lh = 1.0f;
              float esc = sw / lw; if (sh / lh < esc) esc = sh / lh;
              if (esc < 0.0001f) esc = 0.0001f;
              float dw = lw * esc, dh = lh * esc;
              float dx0 = sx0 + (sw - dw) * 0.5f, dy0 = sy0 + (sh - dh) * 0.5f;
              hudX0 = dx0; hudY0 = dy0; hudW = dw; hudH = dh; hudEsc = esc; hudOverride = false;
              UI2D_DibujarOverlay(dx0, dy0, dw, dh, esc, g_hudCapturaPos, true);
          } }
        // el OVERLAY DEL JUEGO pudo dejar CUALQUIER estado 2D: una imagen/video
        // sin canal alpha APAGA Blend (UIOverlay::DibujarImagenRect), el texto
        // deja mezcla premultiplicada, un elemento con profundidad prende el
        // z-test. La UI del viewport no ASUME nada de eso: AFIRMA su estado
        // completo (mismo criterio que W3dDecalesDibujarPendientes). Era el bug
        // de los botones de pausa con BORDES NEGROS jugando: el HUD del juego
        // terminaba en una imagen opaca, Blend quedaba apagado de verdad (el
        // cache lo sabia, asi que este bloque -- que solo reponia la FUNCION de
        // mezcla -- no lo volvia a prender) y la barra translucida y las
        // esquinas de los botones salian opacas/negras. Con el cache fino,
        // re-afirmar lo que ya estaba puesto cuesta cero llamadas de driver.
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
        extern int g_poseModo;
        bool transformando = (Viewport3DActive == this &&
            (estado == translacion || estado == rotacion || estado == EditScale) &&
            (InteractionMode == ObjectMode ||
             (InteractionMode == EditMode && EditXformActivo()) ||
             (InteractionMode == EditMode && BoneXformModo()) ||   // G/R/S de huesos (Edit de armature)
             (InteractionMode == PoseMode && g_poseModo)));
        if (transformando) {
            RenderBarraTransform();
        } else if (AnimEsJuego && JuegoSimActiva()) {
            // ===== MODO JUEGO (sim corriendo): barra MINIMA. Se puede: parar/
            // pausar/reanudar, y los menus View (cambiar camara) / Overlays /
            // Render. Nada de editar. Con STOP la interfaz vuelve a la normal. =====
            static const int kOcultar[] = { BR_Mode, BR_SelMode, BR_Pivot, BR_Orient, BR_Snap,
                                            BR_Select, BR_Add, BR_Mesh, BR_Animation, BR_Object, BR_UV };
            for (size_t i = 0; i < sizeof(kOcultar) / sizeof(kOcultar[0]); i++) {
                Button* bo = BarRolBtn(BarButtons, kOcultar[i]);
                if (bo) bo->visible = false;
            }
            Button* bs = BarRolBtn(BarButtons, BR_JuegoStop);
            Button* bl = BarRolBtn(BarButtons, BR_JuegoPlay);
            if (bs) bs->visible = true;
            if (bl) {
                bl->visible = true;
                // UN SOLO boton play/pausa: dice lo que va a HACER el click
                bl->text = PlayAnimation ? "Pausa" : "Play";
                // verde mientras el juego CORRE (mismo estilo que Snap ON)
                static float playVerde[3]; const float* acc = ListaColores[static_cast<int>(ColorID::accent)];
                for (int i = 0; i < 3; i++) playVerde[i] = acc[i] * 0.4f;
                bl->tinte = PlayAnimation ? playVerde : NULL;
                bl->colorTexto = PlayAnimation ? acc : NULL;
            }
            for (size_t i = 0; i < BarButtons.size(); i++)
                if (BarButtons[i]->rol == BR_Overlays)
                    BarButtons[i]->icon = showOverlays ? (int)IconType::visible : (int)IconType::hidden;
            RenderBar();
        } else {
            // el boton [1] (modo) para MALLA (Object/Edit/Paint) o ARMATURE (Object/Edit/Pose);
            // muestra el modo actual con su icono.
            { Button* bt;   // sin sim: barra normal; el Play queda si estamos en "Juego"
              if ((bt = BarRolBtn(BarButtons, BR_JuegoStop)))  bt->visible = false;
              if ((bt = BarRolBtn(BarButtons, BR_JuegoPlay))) {
                  bt->visible = AnimEsJuego;
                  bt->text = "Play";
                  bt->tinte = NULL; bt->colorTexto = NULL;
              } }
            bool esMesh = ObjActivo && ObjActivo->getType() == ObjectType::mesh;
            bool esArm  = ObjActivo && ObjActivo->getType() == ObjectType::armature;
            // se buscan por ROL (no por indice) -> reordenar la barra no rompe esto.
            Button* bMode = BarRolBtn(BarButtons, BR_Mode);
            if (bMode) {
                bMode->visible = esMesh || esArm;
                if (!esMesh && !esArm) InteractionMode = ObjectMode;
                // un armature no tiene Paint: si quedo en un modo de malla, volver a Object
                if (esArm && InteractionMode != EditMode && InteractionMode != PoseMode) InteractionMode = ObjectMode;
                // una malla no tiene Pose: si venia de un armature en Pose, volver a Object
                if (esMesh && InteractionMode == PoseMode) InteractionMode = ObjectMode;
                bMode->text = (InteractionMode == EditMode)     ? T("Edit Mode") :
                              (InteractionMode == PoseMode)     ? T("Pose Mode") :
                              (InteractionMode == VertexPaint)  ? T("Vertex Paint") :
                              (InteractionMode == WeightPaint)  ? T("Weight Paint") :
                              (InteractionMode == TexturePaint) ? T("Texture Paint") : T("Object Mode");
                bMode->icon = (InteractionMode == ObjectMode) ? (int)IconType::object :
                              esArm                            ? (int)IconType::armature : (int)IconType::mesh;
            }
            bool enEdit = (esMesh && InteractionMode == EditMode);
            Button* bSelM = BarRolBtn(BarButtons, BR_SelMode); // sub-elemento, SOLO en edit (icono)
            if (bSelM) {
                bSelM->visible = enEdit;
                bSelM->icon = (EditSelectMode == SelEdge) ? (int)IconType::selEdge :
                              (EditSelectMode == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
            }
            Button* bUV = BarRolBtn(BarButtons, BR_UV);       // UV: SOLO en edit
            if (bUV) bUV->visible = enEdit;
            Button* bMesh = BarRolBtn(BarButtons, BR_Mesh);   // Mesh (Transform/Snap/Delete): SOLO en edit
            if (bMesh) bMesh->visible = enEdit;
            Button* bAdd = BarRolBtn(BarButtons, BR_Add);     // Add: SOLO en Object Mode
            if (bAdd) bAdd->visible = (InteractionMode == ObjectMode);
            Button* bPiv = BarRolBtn(BarButtons, BR_Pivot);   // Pivot: solo icono = el modo actual
            if (bPiv) bPiv->icon = (g_transformPivot == PivotCursor3D)   ? (int)IconType::pivotCursor :
                                   (g_transformPivot == PivotIndividual) ? (int)IconType::pivotIndividual :
                                   (g_transformPivot == PivotActive)     ? (int)IconType::pivotActive :
                                                                           (int)IconType::pivotMedian;
            SyncBotonContexto();
            // "Animation": keyframes del objeto + Motion Trail. Solo con algo seleccionado y en Object Mode
            // (en Pose Mode los keyframes van por el menu "Pose").
            Button* bAnim = BarRolBtn(BarButtons, BR_Animation);
            if (bAnim) bAnim->visible = HayObjetosSeleccionados() &&
                ((InteractionMode == ObjectMode) ||
                 (InteractionMode == EditMode && ActiveAnimKind == 3)); // editando una vertex anim: Insert Keyframe a mano
            Button* bOri = BarRolBtn(BarButtons, BR_Orient);  // muestra la orientacion actual
            if (bOri) bOri->text = (transformOrientation == LocalOrient)  ? "Local" :
                                   (transformOrientation == ViewOrient)   ? "View"  :
                                   (transformOrientation == NormalOrient) ? "Normal" : "Global";
            // SNAP: el boton se ilumina VERDE cuando esta ON (para saber que el imantado esta activo)
            Button* bSnap = BarRolBtn(BarButtons, BR_Snap);
            if (bSnap){
                static float snapVerde[3]; const float* acc = ListaColores[static_cast<int>(ColorID::accent)];
                for (int i=0;i<3;i++) snapVerde[i]=acc[i]*0.4f;
                bSnap->tinte = g_snap.enabled ? snapVerde : NULL;
                bSnap->colorTexto = g_snap.enabled ? acc : NULL;
            }
            // el OJO del overlay dice si estan prendidos: abierto = se ven, cerrado = apagados. Va aca (por
            // frame) y no al construirlo, porque el estado cambia.
            for (size_t i = 0; i < BarButtons.size(); i++)
                if (BarButtons[i]->rol == BR_Overlays)
                    BarButtons[i]->icon = showOverlays ? (int)IconType::visible : (int)IconType::hidden;
            RenderBar();
        }
        // barra de HERRAMIENTAS (abajo): historial + orientacion + ejes + aceptar/cancelar tactil.
        // Se dibuja TAMBIEN durante un transform (ahi viven el tilde/cruz y los ejes X/Y/Z).
        RenderToolbar();
        // estadisticas/fps (texto blanco arriba a la derecha; misma ortho 2D)
        RenderEstadisticas();
    }

    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    w3dEngine::Viewport(x, W3dPantallaAlto - y - height, width, height);
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

    DibujarBordes(this);
}

// float -> texto portable (sin %f, que no anda en Symbian). El formateo vive en el
// compartido TransformUI.cpp (W3dFmtFloat): tambien lo usan el UV editor y el Editor 2D.
static std::string W3dFmtF(float v, int dec){ return W3dFmtFloat(v, dec); }

// busca el estado guardado (pos/rot/scale al iniciar el transform) del objeto
static SaveState* W3dEstadoGuardado(Object* o){
    for (size_t i = 0; i < estadoObjetos.size(); i++)
        if (estadoObjetos[i].obj == o) return &estadoObjetos[i];
    return NULL;
}

// el texto de la barra de estado: operacion + valores en vivo del objeto activo
// (mismo mapeo de ejes que el panel Properties: Location Y=z, Z=y; Scale x/y/z)
static const char* W3dOrientPalabra(){
    if (transformOrientation == LocalOrient) return "local";
    if (transformOrientation == ViewOrient)  return "view";
    return "global";
}

// arma el texto: un solo eje muestra SOLO ese eje ("X: .. along global X"); un
// plano muestra los dos del plano; libre muestra los tres (sin sufijo).
static std::string W3dLineaTransform(const char* titulo, float vx, float vy, float vz){
    int a = axisSelect;
    std::string ori = W3dOrientPalabra();
    if (a == X || a == Y || a == Z){
        const char* L = (a == X) ? "X" : (a == Y) ? "Y" : "Z";
        float v = (a == X) ? vx : (a == Y) ? vy : vz;
        return std::string(titulo) + " " + L + ": " + W3dFmtF(v, 4) + "  along " + ori + " " + L;
    }
    if (a == PlaneX || a == PlaneY || a == PlaneZ){
        std::string s = titulo, ejes;
        if (a != PlaneX){ s += "  X: " + W3dFmtF(vx, 4); ejes += "X"; }
        if (a != PlaneY){ s += "  Y: " + W3dFmtF(vy, 4); if (!ejes.empty()) ejes += "-"; ejes += "Y"; }
        if (a != PlaneZ){ s += "  Z: " + W3dFmtF(vz, 4); if (!ejes.empty()) ejes += "-"; ejes += "Z"; }
        return s + "  along " + ori + " " + ejes;
    }
    // libre (3 ejes)
    return std::string(titulo) + "  X: " + W3dFmtF(vx, 4) +
                         "  Y: " + W3dFmtF(vy, 4) + "  Z: " + W3dFmtF(vz, 4);
}

// sufijo "  along <orient> <ejes>" para un valor numerico, segun axisSelect
static std::string W3dSufijoEjes(){
    int a = axisSelect;
    std::string ori = W3dOrientPalabra();
    if (a==X||a==Y||a==Z) return std::string("  along ") + ori + " " + ((a==X)?"X":(a==Y)?"Y":"Z");
    if (a==PlaneX) return std::string("  along ") + ori + " Y-Z";
    if (a==PlaneY) return std::string("  along ") + ori + " X-Z";
    if (a==PlaneZ) return std::string("  along ") + ori + " X-Y";
    return ""; // libre
}

static std::string W3dTextoTransform(){
    // ENTRADA NUMERICA: muestra la EXPRESION tipeada + su resultado (estilo Blender
    // "Move: [(2*3)+3] = 9  along global X"). Vale para objetos y malla. El armado
    // "[expr|] = valor" es el compartido de TransformUI (mismo formato en UV / 2D).
    if (NumInputActivo()){
        const char* op = (estado==rotacion) ? T("Rotate") : (estado==EditScale) ? (EditShrinkActivo() ? T("Shrink/Fatten") : T("Scale")) : T("Move");
        const char* unit = (estado==rotacion) ? "\xC2\xB0" : "";
        std::string suf = (estado==rotacion && (axisSelect==ViewAxis||axisSelect==XYZ||axisSelect==OrbitalAxis)) ? "" : W3dSufijoEjes();
        return std::string(op) + ": " + TransformUITextoNum(unit) + suf;
    }
    // Edit Mode: los valores son los del TRANSFORM DE MALLA (los vertices), no del
    // objeto (que no se mueve). Mismos labels de eje/orientacion. La rotacion usa
    // gAnguloTransform igual que objetos, asi que cae al codigo de abajo.
    const bool edit = (InteractionMode == EditMode && EditXformActivo());
    if (edit){
        if (estado == translacion){
            Vector3 d = EditXformTransDelta(); // mundo (engine xyz) -> user X/Y/Z (swap z/y)
            return W3dLineaTransform("Translate", d.x, d.z, d.y);
        }
        if (estado == EditScale){
            if (EditShrinkActivo()) return std::string("Shrink/Fatten: ") + W3dFmtF(EditXformShrinkAmt(), 4);
            float f = EditXformScaleFactor();
            return W3dLineaTransform(T("Scale"), f, f, f);
        }
        // rotacion: cae al bloque comun (gAnguloTransform + axisSelect/orientacion)
    }
    // EDIT de ARMATURE (G/R/S de huesos): translate/scale muestran el valor del transform de puntas.
    // Rotate cae al bloque comun (usa gAnguloTransform, que BoneXformAplicar setea).
    if (InteractionMode == EditMode && BoneXformModo()){
        int bm = BoneXformModo();
        if (bm == 1) return std::string("Translate: ") + W3dFmtF(BoneXformHeaderValor(), 4) + W3dSufijoEjes();
        if (bm == 3) return std::string("Scale: ") + W3dFmtF(BoneXformHeaderValor(), 3);
    }
    // POSE Mode: translate/scale muestran el valor de la POSE (no el del objeto armature). Rotate cae al bloque comun
    // (usa gAnguloTransform, que el transform de pose setea). El numerico ya lo cubre el bloque de arriba.
    { extern int PoseHeaderModo(); extern float PoseHeaderValor();
      if (InteractionMode == PoseMode && PoseHeaderModo()){
        int pm = PoseHeaderModo();
        if (pm == 1) return std::string("Translate: ") + W3dFmtF(PoseHeaderValor(), 4) + W3dSufijoEjes();
        if (pm == 3) return std::string("Scale: ") + W3dFmtF(PoseHeaderValor(), 3);
      } }
    if (!ObjActivo) return "";
    SaveState* st = W3dEstadoGuardado(ObjActivo);
    if (estado == translacion){
        Vector3 p = ObjActivo->pos;
        Vector3 p0 = st ? st->pos : p;
        // X/Y/Z del usuario = engine x / z / y (swap del modelo)
        return W3dLineaTransform("Translate", p.x - p0.x, p.z - p0.z, p.y - p0.y);
    }
    if (estado == EditScale){
        Vector3 s = ObjActivo->scale;
        Vector3 s0 = st ? st->scale : Vector3(1,1,1);
        float fx = (s0.x != 0.0f) ? s.x / s0.x : s.x;
        float fy = (s0.y != 0.0f) ? s.y / s0.y : s.y;
        float fz = (s0.z != 0.0f) ? s.z / s0.z : s.z;
        return W3dLineaTransform(T("Scale"), fx, fz, fy); // Y=scale.z, Z=scale.y
    }
    if (estado == rotacion){
        std::string r = "Rotate  " + W3dFmtF(gAnguloTransform, 1) + "\xC2\xB0";
        int a = axisSelect;
        if (a == X || a == Y || a == Z){
            const char* L = (a == X) ? "X" : (a == Y) ? "Y" : "Z";
            r += std::string("  along ") + W3dOrientPalabra() + " " + L;
        }
        else if (a == OrbitalAxis) r += "  Orbital";
        return r;
    }
    return "";
}

// linea PUNTEADA 2D (coords de pantalla del viewport, Y abajo) entre dos puntos.
static void DibujarLineaPunteada2D(float ax, float ay, float bx, float by){
    float dx = bx - ax, dy = by - ay;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 2.0f) return;
    float ux = dx / len, uy = dy / len;
    const float dash = 6.0f, gap = 5.0f, paso = dash + gap;
    GLfloat v[4 * 128]; // hasta 128 guiones
    int n = 0;
    for (float t = 0.0f; t < len && n < 128; t += paso){
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

void Viewport3D::RenderBarraTransform(){
    if (!barCard) return;

    // linea punteada VERDE del pivot al mouse: en ROTAR y ESCALAR (no translate). La barra es chrome
    // (se ve aunque Show Overlays este off), pero la linea punteada SI es overlay del transform: con
    // overlays off NO se dibuja (igual que los ejes de constraint, que viven en RenderOverlay).
    if (showOverlays && (estado == rotacion || estado == EditScale) && gLineaValida){
        DibujarLineaPunteada2D(gTrackPivX, gTrackPivY, gTrackMouseX, gTrackMouseY);
    }

    // fondo + texto: el dibujo COMPARTIDO de la barra de info (TransformUI.cpp), el
    // mismo que usan el UV editor y el Editor 2D durante sus transforms.
    RenderBarraInfo(W3dTextoTransform());
}

// TACTIL: un tap sobre la barra de estado del transform (la que dice "Move: ... = ...")
// abre el teclado numerico en modo transform para editar el valor EXACTO como en PC.
// Solo cuando hay un transform en curso y este es el viewport activo.
bool Viewport3D::ClickBarraTransform(int mx, int my){
    if (Viewport3DActive != this) return false;
    if (!(estado == translacion || estado == rotacion || estado == EditScale)) return false;
    if (!(InteractionMode == ObjectMode ||
          (InteractionMode == EditMode && EditXformActivo()) ||
          (InteractionMode == EditMode && BoneXformModo()))) return false; // G/R/S de huesos: valor exacto tactil
    int barH = BarHeight();
    int yBar = barAbajo ? (y + height - barH) : y; // misma franja que RenderBarraTransform
    if (mx < x || mx >= x + width || my < yBar || my >= yBar + barH) return false;
    NumPadAbrirTransform();
    return true;
}

// El overlay de estadisticas (arriba-derecha: vertices/caras/modgen/ms/fps) vive en ViewPort3D_Stats.cpp
// junto a su helper W3dContarMallas. Solo lee globales ya expuestos (profiler, contadores de OpcionesRender).

void Viewport3D::ChangePerspective(){
    orthographic = !orthographic;
}

void Viewport3D::Aceptar() {
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    // Mostrar el cursor
    #if SDL_MAJOR_VERSION == 2
        SDL_ShowCursor(SDL_ENABLE);
    #elif SDL_MAJOR_VERSION == 3
        SDL_ShowCursor();
    #endif
    //si no hay objetos (liberando el foco: los otros returns de Aceptar tambien lo hacen)
    if (SceneCollection->Childrens.empty()){ ViewPortClickDown = false; return; }

    // EDIT de ARMATURE: el tilde verde / Enter confirman el transform de huesos en curso
    if (BoneGrabActivo()){
        BoneGrabConfirmar();
        NumInputReset();
        ViewPortClickDown = false;
        return;
    }
    if ( InteractionMode == ObjectMode ){
        if (estado != editNavegacion){
            // AUTO KEY: va ANTES de UndoTransformConfirmar, que es lo que puede limpiar estadoObjetos (el
            // snapshot contra el que se mide QUE canal cambio). Y antes tambien para que los keyframes que
            // guarda entren en el MISMO comando de undo que el transform.
            AutoKeyObjetos();
            UndoTransformConfirmar(); // Ctrl+Z: el transform se ACEPTO -> pushea el undo pendiente
            estado = editNavegacion;
        }
    } else if (InteractionMode == EditMode && EditXformActivo()){
        EditXformConfirmar(); // fija el transform de malla (recalcula bordes+normales)
    } else if (InteractionMode == PoseMode){
        extern int g_poseModo; extern void PoseXformConfirm();
        if (g_poseModo) PoseXformConfirm(); // fija la pose editada (Enter / tick del toolbar); idempotente con el click
    }
    NumInputReset(); // termina el transform -> limpia el valor numerico tipeado
    ViewPortClickDown = false;
}
#endif

void Viewport3D::button_left(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    // EDIT de ARMATURE: un grab MODAL de huesos en curso (E/G/toolbar) se CONFIRMA con el click,
    // como cualquier transform. Se marca el click como consumido para que el ScenePick3D del mismo
    // evento no re-pickee un hueso.
    if (BoneGrabActivo() && !BoneGrabPorClick()){
        BoneGrabConfirmar();
        BoneGrabMarcarClickConsumido();
        return;
    }
    if (estado == translacion || estado == EditScale || estado == rotacion){
        Aceptar();
    }
    // WEIGHT PAINT: el click sobre el contenido PINTA (arranca el TRAZO: snapshot de undo +
    // primera pasada del pincel). El drag sigue en event_mouse_motion; el commit al soltar.
    else if (InteractionMode == WeightPaint && !PopUpActive){
        Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        GuardarMousePos();
        if (m && WeightPaintTrazoIniciar(m) >= 0){
            g_wp3dPintando = true;
            WP3DPintar(this, lastMouseX, lastMouseY);
        }
    }
    else {
        GuardarMousePos();
    }
}
#endif

#ifndef W3D_SYMBIAN
void Viewport3D::mouse_button_up(int boton){
    if (g_wp3dPintando){ g_wp3dPintando = false; WeightPaintTrazoFin(); } // fin del trazo -> commit del undo
    // EDIT de ARMATURE: el drag de head/tail arrancado por el click termina al SOLTAR (commit del undo)
    if (BoneGrabActivo() && BoneGrabPorClick()) BoneGrabConfirmar();
    ViewPortClickDown = false;
}
#endif

void Viewport3D::event_mouse_motion(int mx, int my){
    // el viewport 3D bajo el mouse pasa a ser el ACTIVO (multi-viewport)
    Viewport3DActive = this;
    // POSE MODE: si hay un transform de huesos en curso (G/R/S), el arrastre lo aplica a la pose y NO orbita/transforma
    // objetos. Usa mx/my (delta con signo real), NO las globales dx/dy que no se actualizan durante el transform de hueso.
    { extern int g_poseModo; if (g_poseModo){ extern void PoseXformMotion(int,int); PoseXformMotion(mx, my); return; } }

#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    // MODO PINTURA: el circulo del pincel sigue al mouse -> guardar la posicion VIVA del
    // cursor (lastMouseX/Y solo se refrescan al clickear: el circulo quedaba congelado
    // hoverando) y redibujar en cada motion. Con un trazo en curso (mouse apretado), cada
    // motion PINTA otra pasada y consume el evento (asi el izquierdo no orbita en
    // Android/web mientras se pinta).
    if (WP3DModoPintura()) { g_wpCursorX = mx; g_wpCursorY = my; g_redraw = true; }
    if (g_wp3dPintando){
        if (leftMouseDown){ WP3DPintar(this, mx, my); return; }
        g_wp3dPintando = false; WeightPaintTrazoFin(); // up perdido (solto fuera del viewport): commit igual
    }
    // EDIT de ARMATURE: drag de head/tail (o grab modal de E/G) en curso -> el arrastre mueve los extremos
    if (BoneGrabActivo()){
        if (BoneGrabPorClick() && !leftMouseDown){ BoneGrabConfirmar(); } // up perdido: commit igual
        else { BoneGrabMotion3D(this, mx, my); return; }
    }

    // si no estamos rotando, se re-captura el angulo del trackball al volver.
    // la linea punteada solo vale en rotar/escalar (no translate ni navegacion).
    if (estado != rotacion) gTrackballCap = false;
    if (estado != rotacion && estado != EditScale) gLineaValida = false;

    //boton del medio del mouse (en Android/WebGL tambien el IZQUIERDO: el touch sintetiza click izq ->
    // 1 dedo arrastrado tiene que orbitar. El tap-vs-drag distingue seleccionar de orbitar.
    // OJO: durante un TRANSFORM el izquierdo NO orbita: el dedo apretado esta MOVIENDO el transform
    // (en tactil se confirma con el tilde de la barra de herramientas, no soltando).
    // modo VIEW (toggle de Edit Mode): con 1 dedo se ORBITA aunque haya una operacion en curso (mover/rotar/
    // extrude/strip) -> se puede mirar desde otro angulo sin cancelar; se apaga View y sigue editando.
    #if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
        bool viewOrbita = (g_viewEditMode && InteractionMode == EditMode);
        if (middleMouseDown || (leftMouseDown && (estado == editNavegacion || viewOrbita))) {
    #else
        if (middleMouseDown) {
    #endif
        ViewPortClickDown = true;
        // Chequear si Shift está presionado
        #if SDL_MAJOR_VERSION == 2
            const Uint8* state = SDL_GetKeyboardState(NULL);
        #elif SDL_MAJOR_VERSION == 3
            const bool* state = SDL_GetKeyboardState(NULL);
        #endif
        bool shiftHeld = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];

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
            LShiftPressed = false;

        }
        else {
            RotateOrbit();
        }
    }
    else if (estado == translacion || estado == rotacion || estado == EditScale){
        // PRIMER motion de un transform nuevo: el delta dx/dy todavia es el del frame
        // anterior (CheckWarpMouseInViewport lo recalcula DESPUES de esto) -> lo ignoro
        // para que el transform arranque en CERO y no pegue un salto.
        // El flag se consume SOLO con un delta REAL (dx/dy != 0). Una motion ESPURIA de delta 0
        // (pasa a veces tras el click del menu de extrude) NO debe limpiarlo, sino el primer move
        // de verdad (que arrastra el delta viejo del menu al viewport) salta. (bug intermitente).
        if (g_xformPrimerMov) {
            if (dx != 0 || dy != 0) g_xformPrimerMov = false;
            dx = 0; dy = 0; // este motion arranca SIEMPRE en cero
        }
        g_snapCurX = mx; g_snapCurY = my; // cursor para el SNAP (busca el target bajo el mouse)
        // si el usuario esta tipeando un valor EXACTO, el mouse NO pisa el transform
        if (NumInputActivo()) { ActualizarLineaTransform(mx, my); return; }
        // Ocultar el cursor
        //SDL_HideCursor();
        // Edit Mode: el transform actua sobre los VERTICES seleccionados (no el
        // objeto). Mismo eje/orientacion/pivot, otro target (modulo en LayoutInput).
        const bool edit = (InteractionMode == EditMode && EditXformActivo());
        switch (estado) {
            case translacion:
                if (edit) EditXformTraslacion(dx, dy, VelocidadArrastreMundo());
                else      SetTranslacionObjetos(dx, dy, VelocidadArrastreMundo());
                break;
            case rotacion:
                if (axisSelect == ViewAxis) {
                    RotarDesdeVista(mx, my); // trackball (ramifica a edit adentro)
                } else if (axisSelect == OrbitalAxis) {
                    gTrackballCap = false;   // orbital: izq/der=camUp, arr/ab=camRight
                    if (edit) EditXformRotOrbital(dx, dy);
                    else      RotarOrbital(dx, dy);
                } else {
                    gTrackballCap = false;   // constreñido a un eje: incremental
                    if (edit) EditXformRotEje(dx, dy);
                    else      SetRotacion(dx, dy);
                }
                break;
            case EditScale:
                // Shrink/Fatten (edit): la distancia por la normal es en MUNDO -> velocidad tipo "mover"
                // para que se sienta pegado al dedo/mouse. Escala normal: factor chico de siempre.
                if (edit) EditXformScale(dx, dy, EditShrinkActivo() ? VelocidadArrastreMundo() : 0.001f);
                else      SetScale(dx, dy, 0.001f);
                break;
            default:
                // por si no coincide con nada
                break;
        }
        // linea punteada VERDE del pivot al mouse (rotar/escalar, no translate)
        ActualizarLineaTransform(mx, my);
    }
}
#endif

void Viewport3D::TeclaDerecha(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else
    // transform de HUESOS en curso: las flechas no deben tocar los OBJETOS (comparten `estado`)
    if (BoneGrabActivo()) return;

    //mueve el mouse
    /*if (mouseVisible){
        mouseX++;
        if (mouseX > iScreenWidth-11){mouseX = iScreenWidth-11;};
    }*/

    //rotX -= fixedMul( 1, aDeltaTimeSecs );
    if (estado == editNavegacion){
        if (navegacionMode == Orbit){
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
        else if (navegacionMode == Fly){
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
    else if (estado == translacion){
        SetTranslacionObjetos(5, 0, VelocidadArrastreMundo());
    }
    else if (estado == rotacion){
        if (axisSelect == OrbitalAxis) RotarOrbital(20, 0); // orbital: yaw (camUp)
        else SetRotacion(-20, 0); // ~2 grados por toque
    }
    else if (estado == EditScale){
        SetScale(2,0);
    }
    else if (estado == timelineMove){
        CurrentFrame++;
        if (!PlayAnimation){
        }
    }
}
#endif

void Viewport3D::TeclaIzquierda(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else
    // transform de HUESOS en curso: las flechas no deben tocar los OBJETOS (comparten `estado`)
    if (BoneGrabActivo()) return;

    //mueve el mouse
    if (mouseVisible){
        mouseX--;
        if (mouseX < 0){mouseX = 0;};
    }

    //rotX += fixedMul( 0.1, aDeltaTimeSecs );
    if (estado == editNavegacion){
        if (navegacionMode == Orbit){
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
        else if (navegacionMode == Fly){
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
    else if (estado == translacion){
        SetTranslacionObjetos(-5, 0, VelocidadArrastreMundo());
    }
    else if (estado == rotacion){
        if (axisSelect == OrbitalAxis) RotarOrbital(-20, 0); // orbital: yaw (camUp)
        else SetRotacion(20, 0); // ~2 grados por toque
    }
    else if (estado == EditScale){
        SetScale(-2,0);
    }
    else if (estado == timelineMove){
        CurrentFrame--;
        // wrap del scrub: al pasar el inicio, vuelve al final (antes PISABA
        // StartFrame con EndFrame: destruia el rango de la animacion)
        if (CurrentFrame < StartFrame) CurrentFrame = EndFrame;
    }
}
#endif

void Viewport3D::TeclaArriba(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else
    // transform de HUESOS en curso: las flechas no deben tocar los OBJETOS (comparten `estado`)
    if (BoneGrabActivo()) return;

    //mueve el mouse
    if (mouseVisible){
        mouseY--;
        if (mouseY < 0){mouseY = 0;};
    }

    if (estado == editNavegacion){
        if (navegacionMode == Orbit){
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
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            PivotY+= 30 * cos(radRotX);
            PivotX-= 30 * sin(radRotX);*/
        }
    }
    else if (estado == EditScale){
        SetScale(2,0);
    }
    else if (estado == rotacion && axisSelect == OrbitalAxis){
        RotarOrbital(0, -20); // orbital: pitch (camRight) hacia arriba
    }
    else if (estado == translacion){
        SetTranslacionObjetos(0, -5, VelocidadArrastreMundo());
    }
}
#endif

void Viewport3D::TeclaAbajo(){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else
    // transform de HUESOS en curso: las flechas no deben tocar los OBJETOS (comparten `estado`)
    if (BoneGrabActivo()) return;

    //mueve el mouse
    /*if (mouseVisible){
        mouseY++;
        if (mouseY > iScreenHeight-17){mouseY = iScreenHeight-17;};
    }*/

    if (estado == editNavegacion){
        if (navegacionMode == Orbit){
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
        else if (navegacionMode == Fly){
            // Convertir el angulo de rotX a radianes
            /*GLfloat radRotX = rotX * M_PI / 180.0;

            PivotY-= 30 * cos(radRotX);
            PivotX+= 30 * sin(radRotX);*/
        }
    }
    else if (estado == EditScale){
        SetScale(-2,0);
    }
    else if (estado == rotacion && axisSelect == OrbitalAxis){
        RotarOrbital(0, 20); // orbital: pitch (camRight) hacia abajo
    }
    else if (estado == translacion){
        SetTranslacionObjetos(0, 5, VelocidadArrastreMundo());
    }
}
#endif

// (OnBar / BarScrollBy: la base ya contempla la toolbar de abajo -- ver ViewPorts.cpp/ToolbarBase.cpp)

void Viewport3D::SetEje(int eje){
#ifdef W3D_SYMBIAN
    // estado/input de PC (variables.h): el N95 maneja esto via HID por ahora
    return;
}
#else

    if (estado != editNavegacion){
        ReestablecerEstado(false);
        axisSelect = eje;
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

#ifndef W3D_SYMBIAN
void Viewport3D::event_key_down(int tecla, bool repeticion){
    const int key = tecla;
    // Los menus que abren EN EL CURSOR (Shift+A, U, X, W, Ctrl+R...) usan lastMouseX/Y, que solo se
    // refrescaban en el CLICK -> el menu salia de la ULTIMA posicion clickeada, no del mouse. Aca
    // (con SDL_GetMouseState = posicion ACTUAL) se ponen donde esta el mouse. No durante un transform.
#ifndef W3D_SYMBIAN
    if (estado == editNavegacion) GuardarMousePos();
#endif
    // las FLECHAS se repiten al mantenerlas (mover/orbitar continuo); el resto
    // de las teclas ignora el auto-repeat.
    bool esFlecha = (key == W3dK_UP || key == W3dK_DOWN || key == W3dK_LEFT || key == W3dK_RIGHT);
    if (esFlecha && repeticion != 0) {
        if (key == W3dK_RIGHT)     TeclaDerecha();
        else if (key == W3dK_LEFT) TeclaIzquierda();
        else if (key == W3dK_UP)   TeclaArriba();
        else                       TeclaAbajo();
        return;
    }
    if (repeticion == 0) {
        switch (key) {
            case W3dK_LSHIFT:
                ShiftCount = 0;
                LShiftPressed = true;
                break;
            case W3dK_LALT:
                LAltPressed = true;
                break;
            case W3dK_RETURN:  // Enter
                if (BoneGrabActivo()){ BoneGrabConfirmar(); break; } // grab de huesos: Enter confirma
                key_down_return();
                break;
            case W3dK_RIGHT:   // Flecha derecha
                TeclaDerecha();
                break;
            case W3dK_LEFT:    // Flecha izquierda
                TeclaIzquierda();
                break;
            case W3dK_UP:
                TeclaArriba();
                break;
            case W3dK_DOWN:
                TeclaAbajo();
                break;
            case W3dK_A:
                // simple: las funciones ya saben si es object o edit mode
                if (LCtrlPressed) {                     // Ctrl+A: menu Apply (Location/Rotation/Scale/All) en object
                    if (InteractionMode == ObjectMode) LayoutApplyMenu(lastMouseX, lastMouseY);
                }
                else if (LShiftPressed) {               // Shift+A: menu Add en el cursor (object)
                    if (InteractionMode == ObjectMode) LayoutMenuAdd(lastMouseX, lastMouseY);
                }
                // EDIT MODE de ARMATURE: A / Alt+A operan sobre los HUESOS (SeleccionarTodo*
                // solo sabe de mallas/objetos/pose -> ahi no hacian nada). Con la seleccion
                // completa, las ops de huesos (G/R/S, E, X, Shift+D, Ctrl+P) toman todo.
                else if (BoneEditActivo() && !BoneGrabActivo())
                    BoneEditSeleccionarTodos(BoneEditArm(), !LAltPressed);
                else if (LAltPressed) DeseleccionarTodo();   // Alt+A: deseleccionar todo
                else SeleccionarTodoForzado();          // A: seleccionar todo
                break;
            case W3dK_I:
                // I = Insert Keyframe. Abre el MENU DE CANALES (Todos / Localizacion / Rotacion /
                // Escala) en Object Mode y en Pose Mode; en Edit Mode (vertices) inserta DIRECTO
                // sin menu (una pose de vertices no tiene canales). Todo el ruteo esta en
                // LayoutMenuInsertKeyframe / InsertarKeyframeContexto (LayoutInput.cpp).
                // Shift+I = insertar TODOS los canales de una, sin abrir el menu (para el que ya
                // sabe lo que quiere y no quiere el click de mas).
                if (LCtrlPressed) InvertirSeleccion();       // Ctrl+I: invertir seleccion
                else if (LShiftPressed) InsertarKeyframeContexto(KfCanalTodos);
                else LayoutMenuInsertKeyframe(lastMouseX, lastMouseY);
                break;
            case W3dK_D: {
                if (LAltPressed){
                    NewInstance();
                }
                else if (LShiftPressed){
                    // Edit Mode: duplica la seleccion de malla (o los HUESOS si se edita un armature); si no, los objetos
                    if (BoneEditActivo() && !BoneGrabActivo()) BoneEditDuplicarInteractivo(BoneEditArm(), true);
                    else if (InteractionMode == EditMode && g_editMesh) LayoutDuplicarEdit();
                    else DuplicatedObject();
                }
                break;
            }
            case W3dK_F:
                // F: "New Edge/Face from Vertices" (solo Edit Mode con malla)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutNewFaceEdit();
                break;
            case W3dK_N:
                // Shift+N: Recalculate Normals (Edit Mode), igual que el menu Face
                if (LShiftPressed && estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutRecalcNormales();
                break;
            case W3dK_L:
                // L: Select Linked (la isla conectada bajo el mouse)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutSelectLinked(lastMouseX, lastMouseY);
                break;
            case W3dK_U:
                // Edit Mode: U abre el menu UV (Mark Seam + proyecciones).
                // Object Mode: U NO hace nada (la "limpieza de pantalla" / Show Overlays se maneja
                // SOLO desde el checkbox del menu Overlays, no por tecla).
                if (InteractionMode == EditMode && g_editMesh && estado == editNavegacion)
                    LayoutMenuUV(lastMouseX, lastMouseY);
                break;
            case W3dK_J:
                // Ctrl+J: une las mallas seleccionadas en el objeto activo (Join). J sola: por ahora NADA
                // (reservada para el futuro en el viewport 3D; antes cambiaba el render view, sacado a pedido).
                if (LCtrlPressed && estado == editNavegacion && InteractionMode == ObjectMode)
                    JoinObjetos();
                break;
            case W3dK_H:
                ChangeVisibilityObj();
                break;
            case W3dK_K:
                SetShowOverlays(!showOverlays);
                break;
            case W3dK_X:
                // EDIT de ARMATURE con transform modal (G/R/S): X constrinie al eje (Shift+X = plano);
                // re-apretar cicla Global->Local->View->libre, como Object/Pose.
                if (BoneXformModo()){
                    if (LShiftPressed) BoneCiclarPlano(0); else BoneCiclarEje(0);
                    break;
                }
                // EDIT de ARMATURE: X = borrar los huesos seleccionados (re-parenta los hijos al abuelo)
                if (BoneEditActivo() && !BoneGrabActivo() && estado == editNavegacion){
                    BoneEditBorrar(BoneEditArm());
                    break;
                }
                // Pose Mode: X = eje X (Shift+X = plano YZ); cicla Global/Local/View al re-apretar
                if (InteractionMode == PoseMode){ extern int g_poseModo; extern void PoseCiclarEje(int); extern void PoseCiclarPlano(int); if (g_poseModo){ if (LShiftPressed) PoseCiclarPlano(0); else PoseCiclarEje(0); break; } }
                // durante un transform: X = eje X (re-apretar cicla
                // Global->Local->View->libre); Shift+X = plano (mueve en Y,Z).
                if (estado != editNavegacion){
                    if (LShiftPressed) CiclarPlanoTransform(X);
                    else CiclarEjeTransform(X);
                    if (EditXformActivo()) EditXformReiniciar(); // restaura y re-aplica el nuevo eje
                }
                // Edit Mode: menu Delete cerca del cursor; Object Mode: POPUP de confirmar borrado
                else if (!LayoutDeleteEdit(lastMouseX, lastMouseY)) AbrirConfirmarBorrado();
                break;
            case W3dK_BACKSPACE:
            case W3dK_DELETE:
                if (BoneEditActivo() && !BoneGrabActivo() && estado == editNavegacion){ // Supr = borrar huesos
                    BoneEditBorrar(BoneEditArm());
                    break;
                }
                if (estado == editNavegacion && !LayoutDeleteEdit(lastMouseX, lastMouseY))
                    AbrirConfirmarBorrado();
                break;
            case W3dK_Y:
                if (BoneXformModo()){ if (LShiftPressed) BoneCiclarPlano(1); else BoneCiclarEje(1); break; } // modal de huesos: eje Y
                if (InteractionMode == PoseMode){ extern int g_poseModo; extern void PoseCiclarEje(int); extern void PoseCiclarPlano(int); if (g_poseModo){ if (LShiftPressed) PoseCiclarPlano(1); else PoseCiclarEje(1); break; } }
                if (estado != editNavegacion){
                    if (LShiftPressed) CiclarPlanoTransform(Y);
                    else CiclarEjeTransform(Y);
                    if (EditXformActivo()) EditXformReiniciar();
                }
                break;
            case W3dK_Z:
                if (BoneXformModo()){ if (LShiftPressed) BoneCiclarPlano(2); else BoneCiclarEje(2); break; } // modal de huesos: eje Z
                if (InteractionMode == PoseMode){ extern int g_poseModo; extern void PoseCiclarEje(int); extern void PoseCiclarPlano(int); if (g_poseModo){ if (LShiftPressed) PoseCiclarPlano(2); else PoseCiclarEje(2); break; } }
                if (estado != editNavegacion){
                    if (LShiftPressed) CiclarPlanoTransform(Z);
                    else CiclarEjeTransform(Z);
                    if (EditXformActivo()) EditXformReiniciar();
                }
                break;
            case W3dK_R:
                // Ctrl+R: Loop Cut and Slide (Edit Mode con malla)
                if (LCtrlPressed && estado == editNavegacion && InteractionMode == EditMode && g_editMesh){
                    LoopCutIniciar(lastMouseX, lastMouseY);
                    break;
                }
                // EDIT de ARMATURE: R rota la seleccion (puntas/huesos) alrededor del pivote del
                // viewport; R de nuevo (ya rotando) cicla la orientacion Global->Local->View.
                if (BoneEditActivo()){
                    if (BoneXformModo() == 2) BoneCiclarOrient();
                    else if (!LCtrlPressed && !BoneGrabActivo() && estado == editNavegacion) BoneXformStart(BoneEditArm(), 2);
                    break;
                }
                // POSE MODE: R rota los huesos seleccionados; R de nuevo (ya rotando) cicla la orientacion View->Global->Local.
                if (InteractionMode == PoseMode){ extern int g_poseModo; extern void PoseXformStart(int); extern void PoseCiclarOrient(); extern void PoseClearTransform(int);
                    if (LAltPressed) PoseClearTransform(2);                       // Alt+R: Clear Rotation
                    else if (g_poseModo == 2) PoseCiclarOrient(); else PoseXformStart(2); break; }
                // R arranca la rotacion (trackball); R de nuevo alterna
                // trackball <-> orbital/gimbal (sin tener que ciclar X/Y/Z).
                // En Edit Mode el transform actua sobre los vertices seleccionados.
                if (estado == rotacion) ToggleRotacionOrbital();
                else { // EditXformStart en Edit Mode CAPTURA el undo (Ctrl+Z); en Object Mode -> SetRotacion
                    valorRotacion = 0;
                    if (!EditXformStart(rotacion, ViewAxis)) SetRotacion();
                }
                break;
            case W3dK_G:
                // EDIT de ARMATURE: G agarra la seleccion (puntas o huesos enteros; la punta compartida
                // arrastra a su soldada). Modal: click/Enter confirma, Esc cancela, X/Y/Z constrinien.
                if (BoneEditActivo() && estado == editNavegacion){
                    if (!BoneGrabActivo()) BoneXformStart(BoneEditArm(), 1);
                    break;
                }
                if (InteractionMode == PoseMode){ extern void PoseXformStart(int); extern void PoseClearTransform(int);
                    if (LAltPressed) PoseClearTransform(1); else PoseXformStart(1); break; } // POSE: Alt+G limpia translacion; G mueve
                // EditXformStart (no EditXformIniciar directo) -> en Edit Mode CAPTURA el undo del move (Ctrl+Z)
                if (!EditXformStart(translacion, ViewAxis)) SetPosicion();
                break;
            case W3dK_S:
                // EDIT de ARMATURE: S escala la seleccion alrededor del pivote (X/Y/Z constrinien)
                if (BoneEditActivo()){
                    if (!BoneGrabActivo() && estado == editNavegacion) BoneXformStart(BoneEditArm(), 3);
                    break;
                }
                if (InteractionMode == PoseMode){ extern void PoseXformStart(int); extern void PoseClearTransform(int);
                    if (LAltPressed) PoseClearTransform(3); else PoseXformStart(3); break; } // POSE: Alt+S limpia escala; S escala
                // Alt+S en Edit Mode = Shrink/Fatten (cada vert por su normal). Sin Alt = Scale.
                if (LAltPressed && InteractionMode == EditMode && g_editMesh) LayoutShrinkFatten();
                else if (!EditXformStart(EditScale, XYZ)) SetEscala();
                break;
            case W3dK_E:
                // EDIT de ARMATURE: E extruye un hueso desde CADA punta seleccionada (hueso entero =
                // desde su tail) y deja los tails nuevos agarrados al mouse (click/Enter confirma).
                if (BoneEditActivo() && !BoneGrabActivo() && estado == editNavegacion){
                    BoneEditExtruirInteractivo(BoneEditArm(), true);
                    break;
                }
                // Edit Mode: extrude de las caras seleccionadas (arranca el move por
                // la normal). En Object Mode no hace nada.
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutExtrudeFaces();
                break;
            case W3dK_V:
                // Edit Mode: RIP -> separa la malla a lo largo de la seleccion (loop de bordes / verts / caras)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutRipEdit();
                break;
            case W3dK_P:
                // EDIT de ARMATURE: Alt+P = menu Disconnect Bone / Clear Parent (punto 5). En PC
                // el atajo global (controles.cpp) lo consume antes; esto es el fallback si el
                // evento llega directo al viewport.
                if (LAltPressed && BoneEditActivo() && !BoneGrabActivo() && estado == editNavegacion){
                    LayoutParentHuesos(true, lastMouseX, lastMouseY);
                    break;
                }
                // Edit Mode: SEPARATE -> mueve las caras seleccionadas a un mesh NUEVO (como Blender P > Selection)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutSepararEdit();
                break;
            case W3dK_W:
                // Edit Mode: menu Mark/Clear Sharp en el cursor (bordes filosos)
                if (estado == editNavegacion && InteractionMode == EditMode && g_editMesh)
                    LayoutMenuSharp(lastMouseX, lastMouseY);
                break;
            // Numpad (Ctrl = la vista OPUESTA, como Blender)
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
            case W3dK_KP_8: BuscarVertexAnimation(); break;
            case W3dK_KP_9: abrir(); break;
            case W3dK_KP_0:
                if (LCtrlPressed) SetActiveObjectAsCamera();          // Ctrl+Num 0: activo -> camara activa (SIN cambiar la vista)
                else SetViewFromCameraActive(!ViewFromCameraActive);  // Num 0: ver desde la camara (toggle)
                break;
            case W3dK_KP_PERIOD: {
                EnfocarObject();
                break;
            }
            // si querés, agregá más teclas aquí
            case W3dK_ESCAPE:  // Esc
                // grab de huesos en curso: lo cancela (restaura head/tail del snapshot)
                if (BoneGrabActivo()){ BoneGrabCancelar(); NumInputReset(); break; }
                // loop cut en curso: lo descarta (restaura la geometria pre-corte)
                if (LoopCutActivo()) LoopCutCancelar();
                // Edit Mode con transform de malla en curso: descarta (restaura el
                // snapshot de vertices); si no, cancela el transform de objetos.
                else if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
                else Cancelar();
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
                TeclaDerecha();
                break;
            case W3dK_LEFT:    // Flecha izquierda
                TeclaIzquierda();
                break;
            case W3dK_UP:
                TeclaArriba();
                break;
            case W3dK_DOWN:
                TeclaAbajo();
                break;
            case W3dK_A:
                if (BoneEditActivo() && !BoneGrabActivo())
                    BoneEditSeleccionarTodos(BoneEditArm(), !LAltPressed); // Edit de armature: huesos
                else if (LAltPressed) DeseleccionarTodo();
                else SeleccionarTodoForzado();
                break;
            // Numpad (Ctrl = la vista OPUESTA, como Blender)
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
            case W3dK_KP_8: BuscarVertexAnimation(); break;
            case W3dK_KP_9: abrir(); break;
            //case W3dK_KP_0: numpad('0'); break;
            case W3dK_KP_PERIOD: {
                EnfocarObject();
                break;
            }
            // si querés, agregá más teclas aquí
            case W3dK_ESCAPE:  // Esc
                // grab de huesos en curso: lo cancela (mantener Esc apretado no debe caer al Cancelar de objetos)
                if (BoneGrabActivo()){ BoneGrabCancelar(); NumInputReset(); break; }
                // Edit Mode con transform de malla en curso: descarta (restaura el
                // snapshot de vertices); si no, cancela el transform de objetos.
                if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
                else Cancelar();
                NumInputReset();
                break;
        }
    }
}
#endif

#ifndef W3D_SYMBIAN
void Viewport3D::event_key_up(int tecla){
    const int key = tecla;
    switch (key) {
        case W3dK_LSHIFT:
            if (ShiftCount < 20){
                changeSelect(SelectMode::NextSingle);
            }
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
#endif

void Viewport3D::key_down_return(){
    Aceptar();
}

Viewport3D* Viewport3DActive = NULL;

//precalculos
bool recalcularCamara = true;
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
static void JuegoPrepararViewportsRec(ViewportBase* v, bool apagarOverlays) {
    if (!v) return;
    if (v->ViewportKind() == 1) {
        Viewport3D* v3 = (Viewport3D*)v;
        if (apagarOverlays) v3->showOverlays = false;   // sin gizmos/grilla por defecto
        // DAR PLAY NO TOCA LA VISTA DEL USUARIO. Reporte del dueno, textual:
        // "cuando aprieto play no quiero que me modifiques el paneo/zoom de la
        // camara activa". Aca se reseteaba camViewZoom/camViewPanX/camViewPanY
        // (la INSPECCION de la vista de camara) "para que el render vuelva a su
        // marco completo": el encuadre que el usuario se armo a mano se perdia en
        // cada Play. El LOD y el Culling YA no dependen de esto -- miden desde la
        // Camera del juego por su cuenta (LOD::OjoDeMedida / Culling::RenderHijos
        // con W3dJuegoCorriendo) sin escribir una sola variable del viewport.
        (void)v3;
    }
    // por ContainerKind, NO dynamic_cast: el arbol esta disenado sin RTTI (el
    // compilador del N95 no lo tiene; ver ViewPorts.h) y este .cpp compila alla
    if (v->ContainerKind() == 1) {
        ViewportRow* r = (ViewportRow*)v;
        JuegoPrepararViewportsRec(r->childA, apagarOverlays);
        JuegoPrepararViewportsRec(r->childB, apagarOverlays);
    } else if (v->ContainerKind() == 2) {
        ViewportColumn* c = (ViewportColumn*)v;
        JuegoPrepararViewportsRec(c->childA, apagarOverlays);
        JuegoPrepararViewportsRec(c->childB, apagarOverlays);
    }
}
// apagarOverlays: true SOLO al INICIAR el juego (si el usuario los prende en
// pausa, reanudar no se los vuelve a apagar)
void JuegoPrepararViewports(bool apagarOverlays) {
    JuegoPrepararViewportsRec(rootViewport, apagarOverlays);
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
bool JuegoEsperarTexturas() {
    const int pend = TexturasPendientes();
    if (pend <= 0) return true;
    char msg[96];
    snprintf(msg, sizeof(msg), "Loading %d texture(s) before Play...", pend);
    Notificar(msg, false);
    CargarTodasTexturasPendientes();
    g_redraw = true;
    return TexturasPendientes() == 0;
}
