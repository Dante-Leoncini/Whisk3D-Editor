// ============================================================================
//  W3dAtajos.cpp - la tabla de atajos reasignables y su traductor (ver W3dAtajos.h).
//  C++03: compila tambien en el telefono.
// ============================================================================
#include "W3dAtajos.h"
#include "ViewPorts/W3dInput.h"
#include "ViewPorts/ViewPorts.h"       // viewPortActive
#include "ViewPorts/LayoutInput.h"     // EditXformActivo
#include "ViewPorts/Timeline.h"        // DopeXformActivo
#include "edit/BoneEdit.h"             // BoneXformModo / BoneGrabActivo
#include "base/W3dInteractionState.h"  // estado (transform de objetos en curso)
#include "w3dFilesystem.h"
#include "W3dLang.h"
#include "W3dAviso.h"                  // Notificar
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

extern bool LShiftPressed, LCtrlPressed, LAltPressed;
extern int g_poseModo;

// ---------------------------------------------------------------------------------------------------------------
//  LA TABLA. Dentro de un mismo contexto ninguna combinacion por defecto se repite: la primera de cada fila es la
//  que escucha el handler, y si dos funciones compartieran una no habria forma de separarlas.
//  (La prueba prueba_atajos.w3s lo verifica con 'atajoscheck'.)
// ---------------------------------------------------------------------------------------------------------------
static const W3dAtajoDef kAtajos[] = {
    // ---- en cualquier viewport ----
    { "global.guardar",       W3dATC_GLOBAL,   "Save project",               "Ctrl+S" },
    { "global.deshacer",      W3dATC_GLOBAL,   "Undo",                       "Ctrl+Z", "C+Left", NULL },
    { "global.rehacer",       W3dATC_GLOBAL,   "Redo",                       "Ctrl+Y, Ctrl+Shift+Z", "C+Right", NULL },
    { "global.modo",          W3dATC_GLOBAL,   "Object / Edit Mode",         "Tab", "OK", NULL },
    { "global.snap",          W3dATC_GLOBAL,   "Toggle Snap",                "Shift+Tab" },
    { "global.menuSnap",      W3dATC_GLOBAL,   "Snap menu",                  "Shift+S", NULL, "Shift S" },
    { "global.play",          W3dATC_GLOBAL,   "Play / Pause",               "Space", "Green+Left soft", NULL },
    { "global.barraMenu",     W3dATC_GLOBAL,   "Menu bar",                   "M", "Left soft", NULL },
    { "global.emparentar",    W3dATC_GLOBAL,   "Parent",                     "Ctrl+P", NULL, "Ctrl P" },
    { "global.desemparentar", W3dATC_GLOBAL,   "Clear Parent",               "Alt+P", NULL, "Alt P" },
    { "global.pantalla",      W3dATC_GLOBAL,   "Fullscreen",                 "F11" },
    { "global.render",        W3dATC_GLOBAL,   "Render Image",               "F12" },
    { "global.renderAnim",    W3dATC_GLOBAL,   "Render Animation",           "Ctrl+F12" },

    // ---- vista 3D ----
    { "3d.seleccionarTodo",   W3dATC_3D,       "Select All",                 "A", "Pencil+Up", "A" },
    { "3d.deseleccionar",     W3dATC_3D,       "Deselect All",               "Alt+A", "Pencil+Up", "A" },
    { "3d.invertir",          W3dATC_3D,       "Invert Selection",           "Ctrl+I" },
    { "3d.caja",              W3dATC_3D,       "Box Select",                 "B" },
    { "3d.vinculado",         W3dATC_3D,       "Select Linked",              "L" },
    { "3d.agregar",           W3dATC_3D,       "Add",                        "Shift+A" },
    { "3d.aplicar",           W3dATC_3D_OBJ,   "Apply",                      "Ctrl+A" },
    { "3d.mover",             W3dATC_3D,       "Move",                       "G", "1", "G" },
    { "3d.rotar",             W3dATC_3D,       "Rotate",                     "R", "2", "R" },
    { "3d.escalar",           W3dATC_3D,       "Scale",                      "S", "3", "S" },
    { "3d.limpiarPos",        W3dATC_3D,       "Clear Location (Pose)",      "Alt+G" },
    { "3d.limpiarRot",        W3dATC_3D,       "Clear Rotation (Pose)",      "Alt+R" },
    { "3d.limpiarEsc",        W3dATC_3D,       "Shrink/Fatten / Clear Scale", "Alt+S" },
    { "3d.duplicar",          W3dATC_3D,       "Duplicate",                  "Shift+D", NULL, "Shift D" },
    { "3d.instancia",         W3dATC_3D,       "Duplicate Linked",           "Alt+D", NULL, "Alt D" },
    { "3d.borrar",            W3dATC_3D,       "Delete",                     "X, Delete", "C", "X" },
    { "3d.ocultar",           W3dATC_3D,       "Hide / Show",                "H" },
    { "3d.unir",              W3dATC_3D_OBJ,   "Join",                       "Ctrl+J" },
    { "3d.keyframe",          W3dATC_3D_OBJ,   "Insert Keyframe",            "I" },
    { "3d.keyframeTodo",      W3dATC_3D_OBJ,   "Insert Keyframe (all)",      "Shift+I" },
    { "3d.proporcional",      W3dATC_3D,       "Proportional Editing",       "O" },
    { "3d.overlays",          W3dATC_3D,       "Overlays",                   "K" },
    { "3d.vistaLocal",        W3dATC_3D,       "Local View",                 "/" },
    { "3d.encuadrar",         W3dATC_3D,       "Frame Selected",             "Num.", "0", "." },
    { "3d.vistaFrente",       W3dATC_3D,       "Front View",                 "Num1", "#+Left/Right", NULL },
    { "3d.vistaAtras",        W3dATC_3D,       "Back View",                  "Ctrl+Num1", "#+Left/Right", NULL },
    { "3d.vistaDerecha",      W3dATC_3D,       "Right View",                 "Num3", "#+Left/Right", NULL },
    { "3d.vistaIzquierda",    W3dATC_3D,       "Left View",                  "Ctrl+Num3", "#+Left/Right", NULL },
    { "3d.vistaArriba",       W3dATC_3D,       "Top View",                   "Num7", "#+Up", NULL },
    { "3d.vistaAbajo",        W3dATC_3D,       "Bottom View",                "Ctrl+Num7", "#+Down", NULL },
    { "3d.perspectiva",       W3dATC_3D,       "Perspective / Orthographic", "Num5", "#", NULL },
    { "3d.modoVista",         W3dATC_3D,       "Next Viewport Shading",      "Z", "6", NULL },
    { "3d.rodarIzq",          W3dATC_3D,       "Roll Left",                  "Num4" },
    { "3d.rodarDer",          W3dATC_3D,       "Roll Right",                 "Num6" },
    { "3d.vistaCamara",       W3dATC_3D,       "Camera View",                "Num0", "8", NULL },
    { "3d.camaraActiva",      W3dATC_3D,       "Set Active Camera",          "Ctrl+Num0" },
    { "3d.extruir",           W3dATC_3D_EDIT,  "Extrude",                    "E", "7", NULL },
    { "3d.cara",              W3dATC_3D_EDIT,  "New Face from Vertices",     "F" },
    { "3d.normales",          W3dATC_3D_EDIT,  "Recalculate Normals",        "Shift+N" },
    { "3d.loopcut",           W3dATC_3D_EDIT,  "Loop Cut and Slide",         "Ctrl+R", "8", NULL },
    { "3d.conectar",          W3dATC_3D_EDIT,  "Connect Vertex Path",        "J" },
    { "3d.rip",               W3dATC_3D_EDIT,  "Rip",                        "V" },
    { "3d.slide",             W3dATC_3D_EDIT,  "Vertex Slide",               "Shift+V" },
    { "3d.separar",           W3dATC_3D_EDIT,  "Separate",                   "P" },
    { "3d.merge",             W3dATC_3D_EDIT,  "Merge",                      "M" },
    { "3d.menuContexto",      W3dATC_3D,       "Context menu",               "W" },
    { "3d.triangular",        W3dATC_3D_EDIT,  "Triangulate",                "Ctrl+T" },
    { "3d.menuUV",            W3dATC_3D_EDIT,  "UV menu",                    "U" },
    // ---- vista 3D: lo que hacia falta y sale del teclado del handler (estaba solo como texto en el menu) ----
    { "3d.inset",             W3dATC_3D_EDIT,  "Inset Faces",                "I" },
    { "3d.bisel",             W3dATC_3D_EDIT,  "Bevel Edges",                "Ctrl+B" },
    { "3d.biselVert",         W3dATC_3D_EDIT,  "Bevel Vertices",             "Ctrl+Shift+B" },
    { "3d.cortar",            W3dATC_3D_OBJ,   "Cut",                        "Ctrl+X" },
    { "3d.copiar",            W3dATC_3D_OBJ,   "Copy",                       "Ctrl+C" },
    { "3d.pegar",             W3dATC_3D_OBJ,   "Paste",                      "Ctrl+V" },

    // ---- vista 3D: operaciones SIN tecla de fabrica (las de los menus; el usuario les pone una si quiere) ----
    { "3d.geoAOrigen",        W3dATC_3D_OBJ,   "Geometry to Origin",         "", NULL, NULL, "obj:200" },
    { "3d.origenAGeo",        W3dATC_3D_OBJ,   "Origin to Geometry",         "", NULL, NULL, "obj:201" },
    { "3d.origenACursor",     W3dATC_3D_OBJ,   "Origin to 3D Cursor",        "", NULL, NULL, "obj:202" },
    { "3d.aplicarPos",        W3dATC_3D_OBJ,   "Apply Location",             "", NULL, NULL, "obj:220" },
    { "3d.aplicarRot",        W3dATC_3D_OBJ,   "Apply Rotation",             "", NULL, NULL, "obj:221" },
    { "3d.aplicarEsc",        W3dATC_3D_OBJ,   "Apply Scale",                "", NULL, NULL, "obj:222" },
    { "3d.aplicarTodo",       W3dATC_3D_OBJ,   "Apply All Transforms",       "", NULL, NULL, "obj:223" },
    { "3d.keyPos",            W3dATC_3D_OBJ,   "Insert Location Keyframe",   "", NULL, NULL, "obj:531" },
    { "3d.keyRot",            W3dATC_3D_OBJ,   "Insert Rotation Keyframe",   "", NULL, NULL, "obj:532" },
    { "3d.keyEsc",            W3dATC_3D_OBJ,   "Insert Scale Keyframe",      "", NULL, NULL, "obj:533" },
    { "3d.crearPrefab",       W3dATC_3D_OBJ,   "Create Prefab",              "", NULL, NULL, "obj:700" },
    { "3d.desarmarPrefab",    W3dATC_3D_OBJ,   "Unpack Prefab",              "", NULL, NULL, "obj:701" },
    { "3d.suave",             W3dATC_3D_EDIT,  "Shade Smooth",               "", NULL, NULL, "obj:320" },
    { "3d.plano",             W3dATC_3D_EDIT,  "Shade Flat",                 "", NULL, NULL, "obj:321" },
    { "3d.voltearNormales",   W3dATC_3D_EDIT,  "Flip Normals",               "", NULL, NULL, "obj:324" },
    { "3d.marcarAfilado",     W3dATC_3D_EDIT,  "Mark Sharp",                 "", NULL, NULL, "obj:330" },
    { "3d.limpiarAfilado",    W3dATC_3D_EDIT,  "Clear Sharp",                "", NULL, NULL, "obj:331" },
    { "3d.marcarCostura",     W3dATC_3D_EDIT,  "Mark Seam",                  "", NULL, NULL, "obj:358" },
    { "3d.limpiarCostura",    W3dATC_3D_EDIT,  "Clear Seam",                 "", NULL, NULL, "obj:359" },
    { "3d.unirCentro",        W3dATC_3D_EDIT,  "Merge at Center",            "", NULL, NULL, "obj:380" },
    { "3d.unirCursor",        W3dATC_3D_EDIT,  "Merge at Cursor",            "", NULL, NULL, "obj:381" },
    { "3d.colapsar",          W3dATC_3D_EDIT,  "Collapse",                   "", NULL, NULL, "obj:382" },
    { "3d.unirDistancia",     W3dATC_3D_EDIT,  "Merge by Distance",          "", NULL, NULL, "obj:383" },
    { "3d.borrarVerts",       W3dATC_3D_EDIT,  "Delete Vertices",            "", NULL, NULL, "obj:361" },
    { "3d.borrarAristas",     W3dATC_3D_EDIT,  "Delete Edges",               "", NULL, NULL, "obj:362" },
    { "3d.borrarCaras",       W3dATC_3D_EDIT,  "Delete Faces",               "", NULL, NULL, "obj:363" },
    { "3d.borrarLoops",       W3dATC_3D_EDIT,  "Delete Edge Loops",          "", NULL, NULL, "obj:364" },
    { "3d.unwrap",            W3dATC_3D_EDIT,  "Unwrap Angle Based",         "", NULL, NULL, "obj:350" },
    { "3d.smartUV",           W3dATC_3D_EDIT,  "Smart UV Project",           "", NULL, NULL, "obj:351" },
    { "3d.proyVista",         W3dATC_3D_EDIT,  "Project from View",          "", NULL, NULL, "obj:356" },
    { "3d.bloquearOrbita",    W3dATC_3D,       "Lock Orbit",                 "", NULL, NULL, "vista3d:421" },
    { "3d.bloquearCamara",    W3dATC_3D,       "Lock the camera to the view", "", NULL, NULL, "vista3d:423" },
    { "3d.verRender",         W3dATC_3D,       "Render Preview",             "", NULL, NULL, "sombreado:0" },
    { "3d.verMaterial",       W3dATC_3D,       "Material Preview",           "", NULL, NULL, "sombreado:1" },
    { "3d.verSolido",         W3dATC_3D,       "Solid Preview",              "", NULL, NULL, "sombreado:2" },
    { "3d.verAlambre",        W3dATC_3D,       "Wireframe Preview",          "", NULL, NULL, "sombreado:3" },

    // ---- con un transform en curso ----
    { "xf.confirmar",         W3dATC_XFORM,    "Confirm",                    "Enter, NumEnter", "OK", "Enter" },
    { "xf.cancelar",          W3dATC_XFORM,    "Cancel",                     "Esc", "C", "Esc" },
    { "xf.ejeX",              W3dATC_XFORM,    "X axis",                     "X", "1", NULL },
    { "xf.ejeY",              W3dATC_XFORM,    "Y axis",                     "Y", "2", NULL },
    { "xf.ejeZ",              W3dATC_XFORM,    "Z axis",                     "Z", "3", NULL },
    { "xf.planoX",            W3dATC_XFORM,    "Exclude X (plane)",          "Shift+X" },
    { "xf.planoY",            W3dATC_XFORM,    "Exclude Y (plane)",          "Shift+Y" },
    { "xf.planoZ",            W3dATC_XFORM,    "Exclude Z (plane)",          "Shift+Z" },
    { "xf.radioMas",          W3dATC_XFORM,    "Proportional radius +",      "PageUp" },
    { "xf.radioMenos",        W3dATC_XFORM,    "Proportional radius -",      "PageDown" },

    // ---- editor UV ----
    { "uv.seleccionarTodo",   W3dATC_UV,       "Select All",                 "A" },
    { "uv.deseleccionar",     W3dATC_UV,       "Deselect All",               "Alt+A" },
    { "uv.invertir",          W3dATC_UV,       "Invert Selection",           "Ctrl+I" },
    { "uv.vinculado",         W3dATC_UV,       "Select Linked",              "L" },
    { "uv.caja",              W3dATC_UV,       "Box Select",                 "B" },
    { "uv.mover",             W3dATC_UV,       "Move",                       "G", "1", NULL },
    { "uv.rotar",             W3dATC_UV,       "Rotate",                     "R", "2", NULL },
    { "uv.escalar",           W3dATC_UV,       "Scale",                      "S", "3", NULL },
    { "uv.keyframe",          W3dATC_UV,       "Insert Keyframe",            "I" },
    { "uv.keyframeTodo",      W3dATC_UV,       "Insert Keyframe (all)",      "Shift+I" },
    { "uv.extruir",           W3dATC_UV,       "Extrude",                    "E" },
    { "uv.duplicar",          W3dATC_UV,       "Duplicate",                  "Shift+D" },
    { "uv.borrar",            W3dATC_UV,       "Delete",                     "X, Delete" },
    { "uv.encuadrar",         W3dATC_UV,       "Frame Selected",             "Num." },
    { "uv.encuadrarTodo",     W3dATC_UV,       "Frame All",                  "", NULL, NULL, "uvvista:11" },
    { "uv.cursorASel",        W3dATC_UV,       "Cursor to Selection",        "", NULL, NULL, "uvcursor:0" },
    { "uv.selACursor",        W3dATC_UV,       "Selection to Cursor",        "", NULL, NULL, "uvcursor:1" },
    { "uv.cursorCentro",      W3dATC_UV,       "Cursor to Center",           "", NULL, NULL, "uvcursor:2" },

    // ---- timeline ----
    { "tl.seleccionarTodo",   W3dATC_TIMELINE, "Select All",                 "A" },
    { "tl.deseleccionar",     W3dATC_TIMELINE, "Deselect All",               "Alt+A" },
    { "tl.invertir",          W3dATC_TIMELINE, "Invert Selection",           "Ctrl+I" },
    { "tl.mover",             W3dATC_TIMELINE, "Move",                       "G", "1", NULL },
    { "tl.escalar",           W3dATC_TIMELINE, "Scale",                      "S", "3", NULL },
    { "tl.rotar",             W3dATC_TIMELINE, "Rotate handles",             "R", "2", NULL },
    { "tl.duplicar",          W3dATC_TIMELINE, "Duplicate",                  "Ctrl+D, Shift+D" },
    { "tl.cortar",            W3dATC_TIMELINE, "Cut",                        "Ctrl+X" },
    { "tl.copiar",            W3dATC_TIMELINE, "Copy",                       "Ctrl+C" },
    { "tl.pegar",             W3dATC_TIMELINE, "Paste",                      "Ctrl+V" },
    { "tl.borrar",            W3dATC_TIMELINE, "Delete",                     "X" },
    { "tl.interp",            W3dATC_TIMELINE, "Interpolation",              "E, T" },
    { "tl.easing",            W3dATC_TIMELINE, "Easing Type",                "Ctrl+E" },
    { "tl.handle",            W3dATC_TIMELINE, "Handle type",                "V" },
    { "tl.encuadrar",         W3dATC_TIMELINE, "Frame Selected",             "Num.", "0", NULL },
    { "tl.frameAnt",          W3dATC_TIMELINE, "Previous frame",             "Left" },
    { "tl.frameSig",          W3dATC_TIMELINE, "Next frame",                 "Right" },
    { "tl.keyAnt",            W3dATC_TIMELINE, "Previous keyframe",          "Down" },
    { "tl.keySig",            W3dATC_TIMELINE, "Next keyframe",              "Up" },
    { "tl.inicio",            W3dATC_TIMELINE, "Go to start",                "Home" },
    { "tl.fin",               W3dATC_TIMELINE, "Go to end",                  "End" },
    { "tl.verDope",           W3dATC_TIMELINE, "Dope Sheet",                 "", NULL, NULL, "tlvista:6" },
    { "tl.verCurvas",         W3dATC_TIMELINE, "Curves",                     "", NULL, NULL, "tlvista:7" },
    { "tl.panel",             W3dATC_TIMELINE, "Show Panel",                 "", NULL, NULL, "tlvista:100" },
    { "tl.verHandles",        W3dATC_TIMELINE, "Show Handles",               "", NULL, NULL, "tlvista:101" },
    { "tl.smartEuler",        W3dATC_TIMELINE, "Smart Euler",                "", NULL, NULL, "tlkey:2" },
    { "tl.interpConstante",   W3dATC_TIMELINE, "Interpolation: Constant",    "", NULL, NULL, "tlkey:20" },
    { "tl.interpLineal",      W3dATC_TIMELINE, "Interpolation: Linear",      "", NULL, NULL, "tlkey:21" },
    { "tl.interpBezier",      W3dATC_TIMELINE, "Interpolation: Bezier",      "", NULL, NULL, "tlkey:22" },
    { "tl.handleLibre",       W3dATC_TIMELINE, "Handles: Free",              "", NULL, NULL, "tlkey:40" },
    { "tl.handleAlineado",    W3dATC_TIMELINE, "Handles: Aligned",           "", NULL, NULL, "tlkey:41" },
    { "tl.handleVector",      W3dATC_TIMELINE, "Handles: Vector",            "", NULL, NULL, "tlkey:42" },
    { "tl.handleAuto",        W3dATC_TIMELINE, "Handles: Automatic",         "", NULL, NULL, "tlkey:43" },
    { "tl.handleAutoLimite",  W3dATC_TIMELINE, "Handles: Auto Clamped",      "", NULL, NULL, "tlkey:44" },
    { "tl.handlePlano",       W3dATC_TIMELINE, "Handles: Flat",              "", NULL, NULL, "tlkey:45" },

    // ---- outliner ----
    { "out.seleccionarTodo",  W3dATC_OUTLINER, "Select All",                 "A" },
    { "out.deseleccionar",    W3dATC_OUTLINER, "Deselect All",               "Alt+A" },
    { "out.invertir",         W3dATC_OUTLINER, "Invert Selection",           "Ctrl+I" },
    { "out.mover",            W3dATC_OUTLINER, "Move (reorder)",             "G", "1", NULL },
    { "out.renombrar",        W3dATC_OUTLINER, "Rename",                     "F2" },
    { "out.duplicar",         W3dATC_OUTLINER, "Duplicate",                  "Shift+D" },
    { "out.instancia",        W3dATC_OUTLINER, "Duplicate Linked",           "Alt+D" },
    { "out.ocultar",          W3dATC_OUTLINER, "Hide / Show",                "H", "2", NULL },
    { "out.borrar",           W3dATC_OUTLINER, "Delete",                     "X, Delete", "C", NULL },
    { "out.encuadrar",        W3dATC_OUTLINER, "Show active",                "Num." },

    // ---- editor 2D ----
    { "2d.seleccionarTodo",   W3dATC_2D,       "Select All",                 "A" },
    { "2d.deseleccionar",     W3dATC_2D,       "Deselect All",               "Alt+A" },
    { "2d.invertir",          W3dATC_2D,       "Invert Selection",           "Ctrl+I" },
    { "2d.mover",             W3dATC_2D,       "Move",                       "G", "1", NULL },
    { "2d.rotar",             W3dATC_2D,       "Rotate",                     "R", "2", NULL },
    { "2d.escalar",           W3dATC_2D,       "Scale",                      "S", "3", NULL },
    { "2d.duplicar",          W3dATC_2D,       "Duplicate",                  "Shift+D" },
    { "2d.borrar",            W3dATC_2D,       "Delete",                     "X, Delete" },
    { "2d.encuadrar",         W3dATC_2D,       "Frame Selected",             "Num." },
    { "2d.vistaJuego",        W3dATC_2D,       "Game View",                  "Num0" },
    { "2d.emparentar",        W3dATC_2D,       "Set Parent",                 "", NULL, NULL, "2dobj:40" },
    { "2d.desemparentar",     W3dATC_2D,       "Clear Parent",               "", NULL, NULL, "2dobj:41" },
    { "2d.zoom1",             W3dATC_2D,       "Zoom 1:1",                   "", NULL, NULL, "2dvista:1" },
    { "2d.zoom2",             W3dATC_2D,       "Zoom 2:1",                   "", NULL, NULL, "2dvista:2" },

    // ---- propiedades (lista de pasos de una rutina) ----
    { "props.agarrar",        W3dATC_PROPS,    "Grab step",                  "G" },
    { "props.borrar",         W3dATC_PROPS,    "Delete step",                "X, Delete" },
    { "props.keyframe",       W3dATC_PROPS,    "Insert Keyframe",            "I" },

    // ---- editor de scripts ----
    { "ide.guardar",          W3dATC_IDE,      "Save script",                "Ctrl+S" },
    { "ide.deshacer",         W3dATC_IDE,      "Undo",                       "Ctrl+Z" },
    { "ide.copiar",           W3dATC_IDE,      "Copy",                       "Ctrl+C" },
    { "ide.cortar",           W3dATC_IDE,      "Cut",                        "Ctrl+X" },
    { "ide.pegar",            W3dATC_IDE,      "Paste",                      "Ctrl+V" },
    { "ide.todo",             W3dATC_IDE,      "Select All",                 "Ctrl+A" },
    { "ide.rehacer",          W3dATC_IDE,      "Redo",                       "Ctrl+Y, Ctrl+Shift+Z" },
    { "ide.buscar",           W3dATC_IDE,      "Find",                       "Ctrl+F" },
    { "ide.reemplazar",       W3dATC_IDE,      "Replace",                    "Ctrl+H" },
};
static const int kNAtajos = (int)(sizeof(kAtajos) / sizeof(kAtajos[0]));

static std::vector< std::vector<W3dCombo> > gDefecto;   // parseadas una vez
static std::vector< std::vector<W3dCombo> > gActual;    // las del usuario
static bool gCargado = false;
static std::string gRuta;                               // "" = res/atajos.ini
int g_atajoCapturando = -1;

// ---------------------------------------------------------------------------------------------------------------
//  Nombres de las teclas. En ingles y cortos: son los que se leen en el teclado, y el archivo tiene que leerse igual
//  en cualquier idioma del editor. ',' y '+' tienen nombre propio porque separan la lista y la combinacion.
// ---------------------------------------------------------------------------------------------------------------
struct NombreTecla { int tecla; const char* nombre; };
static const NombreTecla kNombres[] = {
    { W3dK_RETURN, "Enter" }, { W3dK_ESCAPE, "Esc" }, { W3dK_BACKSPACE, "Backspace" }, { W3dK_DELETE, "Delete" },
    { W3dK_TAB, "Tab" }, { W3dK_HOME, "Home" }, { W3dK_END, "End" }, { W3dK_PGUP, "PageUp" }, { W3dK_PGDN, "PageDown" },
    { W3dK_LEFT, "Left" }, { W3dK_RIGHT, "Right" }, { W3dK_UP, "Up" }, { W3dK_DOWN, "Down" },
    { W3dK_SPACE, "Space" }, { ',', "Comma" }, { '+', "Plus" },
    { W3dK_KP_0, "Num0" }, { W3dK_KP_1, "Num1" }, { W3dK_KP_2, "Num2" }, { W3dK_KP_3, "Num3" }, { W3dK_KP_4, "Num4" },
    { W3dK_KP_5, "Num5" }, { W3dK_KP_6, "Num6" }, { W3dK_KP_7, "Num7" }, { W3dK_KP_8, "Num8" }, { W3dK_KP_9, "Num9" },
    { W3dK_KP_ENTER, "NumEnter" }, { W3dK_KP_PERIOD, "Num." },
    { W3dK_F1, "F1" }, { W3dK_F2, "F2" }, { W3dK_F3, "F3" }, { W3dK_F4, "F4" }, { W3dK_F5, "F5" }, { W3dK_F6, "F6" },
    { W3dK_F7, "F7" }, { W3dK_F8, "F8" }, { W3dK_F9, "F9" }, { W3dK_F10, "F10" }, { W3dK_F11, "F11" }, { W3dK_F12, "F12" },
};
static const int kNNombres = (int)(sizeof(kNombres) / sizeof(kNombres[0]));

static bool IgualSinMayus(const std::string& a, const char* b) {
    size_t n = strlen(b);
    if (a.size() != n) return false;
    for (size_t i = 0; i < n; i++) if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}
static std::string Recortar(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}

bool W3dEsTeclaModificador(int tecla) {
    return tecla == W3dK_LSHIFT || tecla == W3dK_LCTRL || tecla == W3dK_LALT;
}

std::string W3dComboTexto(W3dCombo c) {
    if (c.Vacia()) return std::string();
    std::string s;
    if (c.mods & W3dMOD_CTRL)  s += "Ctrl+";
    if (c.mods & W3dMOD_SHIFT) s += "Shift+";
    if (c.mods & W3dMOD_ALT)   s += "Alt+";
    for (int i = 0; i < kNNombres; i++) if (kNombres[i].tecla == c.tecla) return s + kNombres[i].nombre;
    if (c.tecla > 32 && c.tecla < 127) {
        char b[2]; b[0] = (char)toupper(c.tecla); b[1] = 0;
        return s + b;
    }
    char b[16]; snprintf(b, sizeof(b), "#%d", c.tecla);   // una tecla sin nombre igual se puede guardar y releer
    return s + b;
}

bool W3dComboDesdeTexto(const std::string& texto, W3dCombo& c) {
    std::string s = Recortar(texto);
    c = W3dCombo();
    if (s.empty()) return false;
    int mods = 0;
    for (;;) {   // los prefijos "Ctrl+", "Shift+", "Alt+" (la tecla es lo que queda: "Ctrl+Plus", "Num.", "/")
        size_t mas = s.find('+');
        if (mas == std::string::npos || mas + 1 >= s.size()) break;
        const std::string pre = Recortar(s.substr(0, mas));
        if (IgualSinMayus(pre, "ctrl") || IgualSinMayus(pre, "control")) mods |= W3dMOD_CTRL;
        else if (IgualSinMayus(pre, "shift")) mods |= W3dMOD_SHIFT;
        else if (IgualSinMayus(pre, "alt")) mods |= W3dMOD_ALT;
        else break;
        s = Recortar(s.substr(mas + 1));
    }
    if (s.empty()) return false;
    for (int i = 0; i < kNNombres; i++) if (IgualSinMayus(s, kNombres[i].nombre)) { c = W3dCombo(kNombres[i].tecla, mods); return true; }
    if (s[0] == '#' && s.size() > 1) { c = W3dCombo(atoi(s.c_str() + 1), mods); return c.tecla > 0; }
    if (s.size() == 1 && (unsigned char)s[0] > 32 && (unsigned char)s[0] < 127) {
        c = W3dCombo(tolower((unsigned char)s[0]), mods);   // las letras viajan en minuscula (W3dK_G == 'g')
        return true;
    }
    return false;
}

// Como se ve en un menu: espacios en vez de '+' ("Ctrl Shift D") y el keypad separado ("Num 0", "Num .").
std::string W3dComboTextoMenu(W3dCombo c) {
    std::string t = W3dComboTexto(c), out;
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == '+' && i + 1 < t.size()) out += ' ';
        else out += t[i];
    }
    if (out.size() >= 4 && out.compare(out.size() - 4, 3, "Num") == 0) out.insert(out.size() - 1, " ");   // "Num0" -> "Num 0"
    else if (out.size() == 4 && out.compare(0, 3, "Num") == 0) out.insert(3, " ");
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
//  El hint de los menus. Que se muestra depende de con que se esta manejando el editor:
//    PC / Android / Web       -> la combinacion que el usuario tiene asignada (la primera)
//    telefono sin Bluetooth   -> la tecla del keypad (1/2/3, C, 0...), o nada si no tiene
//    telefono con teclado BT  -> la tecla que entiende ese teclado; si no entiende esta funcion, la del keypad
//  Los gestos de mouse ("Ctrl Click") se muestran donde hay mouse.
// ---------------------------------------------------------------------------------------------------------------
#ifdef W3D_SYMBIAN
bool W3dSymbianTecladoBT();   // platform/symbian/src/w3dmouse.cpp: llego alguna tecla del teclado Bluetooth
bool W3dSymbianMouseBT();     // idem, el mouse Bluetooth se movio
#endif

std::string W3dAtajoEtiquetaMenu(const std::string& hint) {
    if (hint.empty()) return std::string();
#ifdef W3D_SYMBIAN
    const bool tecladoBT = W3dSymbianTecladoBT(), mouseBT = W3dSymbianMouseBT();
#endif
    if (hint[0] != '@') {                       // texto tal cual: un gesto de mouse ("Ctrl Click") o una marca ("inicial")
#ifdef W3D_SYMBIAN
        if (!mouseBT && hint.find("Click") != std::string::npos) return std::string();   // sin mouse no hay click
#endif
        return hint;
    }
    if (hint.compare(0, 5, "@tel:") == 0) {     // tecla que solo existe en el keypad del telefono
#ifdef W3D_SYMBIAN
        return T(hint.c_str() + 5);
#else
        return std::string();
#endif
    }
    const int i = W3dAtajoBuscar(hint.c_str() + 1);
    if (i < 0) return std::string();
#ifdef W3D_SYMBIAN
    // el teclado BT del N95 tiene su propio mapa fijo (w3dmouse.cpp): lo que se reasigna en PC no lo toca
    if (tecladoBT && kAtajos[i].bt) return kAtajos[i].bt;
    return kAtajos[i].telefono ? std::string(T(kAtajos[i].telefono)) : std::string();
#else
    const std::vector<W3dCombo>& v = W3dAtajoCombos(i);
    return v.empty() ? std::string() : W3dComboTextoMenu(v[0]);
#endif
}

static bool ListaDesdeTexto(const char* texto, std::vector<W3dCombo>& out) {
    out.clear();
    std::string s = texto ? texto : "";
    bool ok = true;
    size_t ini = 0;
    while (ini <= s.size()) {
        size_t coma = s.find(',', ini);
        const std::string parte = Recortar(s.substr(ini, coma == std::string::npos ? std::string::npos : coma - ini));
        if (!parte.empty()) {
            W3dCombo c;
            if (W3dComboDesdeTexto(parte, c)) {
                bool repetida = false;
                for (size_t k = 0; k < out.size(); k++) if (out[k] == c) repetida = true;
                if (!repetida) out.push_back(c);
            } else ok = false;
        }
        if (coma == std::string::npos) break;
        ini = coma + 1;
    }
    return ok;
}

static void Preparar() {
    if (!gDefecto.empty()) return;
    gDefecto.resize((size_t)kNAtajos);
    for (int i = 0; i < kNAtajos; i++) ListaDesdeTexto(kAtajos[i].defecto, gDefecto[(size_t)i]);
    gActual = gDefecto;
}

int W3dAtajosCantidad() { return kNAtajos; }
const W3dAtajoDef& W3dAtajoInfo(int i) { return kAtajos[(i >= 0 && i < kNAtajos) ? i : 0]; }
const std::vector<W3dCombo>& W3dAtajoCombos(int i) {
    Preparar();
    return gActual[(size_t)((i >= 0 && i < kNAtajos) ? i : 0)];
}
std::string W3dAtajoTexto(int i) {
    const std::vector<W3dCombo>& v = W3dAtajoCombos(i);
    std::string s;
    for (size_t k = 0; k < v.size(); k++) { if (k) s += ", "; s += W3dComboTexto(v[k]); }
    return s;
}
int W3dAtajoBuscar(const char* id) {
    if (!id) return -1;
    for (int i = 0; i < kNAtajos; i++) if (strcmp(kAtajos[i].id, id) == 0) return i;
    return -1;
}
bool W3dAtajoEsDefecto(int i) {
    Preparar();
    if (i < 0 || i >= kNAtajos) return true;
    const std::vector<W3dCombo>& a = gActual[(size_t)i];
    const std::vector<W3dCombo>& d = gDefecto[(size_t)i];
    if (a.size() != d.size()) return false;
    for (size_t k = 0; k < a.size(); k++) if (!(a[k] == d[k])) return false;
    return true;
}
const char* W3dAtajoCtxNombre(int ctx) {
    switch (ctx) {
        case W3dATC_GLOBAL:   return "General";
        case W3dATC_3D:       return "3D Viewport";
        case W3dATC_XFORM:    return "During a transform";
        case W3dATC_UV:       return "UV Editor";
        case W3dATC_TIMELINE: return "Timeline";
        case W3dATC_OUTLINER: return "Outliner";
        case W3dATC_2D:       return "2D Editor";
        case W3dATC_PROPS:    return "Properties";
        case W3dATC_IDE:      return "Script Editor";
        case W3dATC_3D_OBJ:   return "Object Mode";
        case W3dATC_3D_EDIT:  return "Edit Mode";
    }
    return "?";
}

bool W3dAtajoMismoAlcance(int a, int b) {
    if (a == b) return true;
    const bool modoA = (a == W3dATC_3D_OBJ || a == W3dATC_3D_EDIT), modoB = (b == W3dATC_3D_OBJ || b == W3dATC_3D_EDIT);
    return (a == W3dATC_3D && modoB) || (b == W3dATC_3D && modoA);
}

// ---------------------------------------------------------------------------------------------------------------
//  Las acciones de las operaciones sin tecla de fabrica. Una tabla chica de nombre -> funcion (la llenan los que
//  arman los menus, una vez al arrancar). Vive en una funcion para no depender del orden de inicializacion.
// ---------------------------------------------------------------------------------------------------------------
struct DespachoAtajo { char nombre[16]; W3dAtajoDespacho fn; };
static DespachoAtajo* Despachos(int*& n) {
    static DespachoAtajo tabla[24];
    static int cant = 0;
    n = &cant;
    return tabla;
}
void W3dAtajoRegistrarDespacho(const char* nombre, W3dAtajoDespacho fn) {
    int* n; DespachoAtajo* t = Despachos(n);
    if (!nombre) return;
    for (int i = 0; i < *n; i++) if (strcmp(t[i].nombre, nombre) == 0) { t[i].fn = fn; return; }
    if (*n >= 24) return;
    strncpy(t[*n].nombre, nombre, 15); t[*n].nombre[15] = 0;
    t[*n].fn = fn;
    (*n)++;
}
// "obj:201" -> la funcion de "obj" y el 201
static W3dAtajoDespacho BuscarDespacho(const char* accion, int& id) {
    id = 0;
    if (!accion) return NULL;
    const char* dp = strchr(accion, ':');
    if (!dp) return NULL;
    const size_t largo = (size_t)(dp - accion);
    id = atoi(dp + 1);
    int* n; DespachoAtajo* t = Despachos(n);
    for (int i = 0; i < *n; i++)
        if (strlen(t[i].nombre) == largo && strncmp(t[i].nombre, accion, largo) == 0) return t[i].fn;
    return NULL;
}
bool W3dAtajoTieneDespacho(const char* accion) { int id; return BuscarDespacho(accion, id) != NULL; }
bool W3dAtajoEjecutar(int i) {
    if (i < 0 || i >= kNAtajos || !kAtajos[i].accion) return false;
    int id; W3dAtajoDespacho fn = BuscarDespacho(kAtajos[i].accion, id);
    if (!fn) return false;
    fn(id);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
//  Cambios
// ---------------------------------------------------------------------------------------------------------------
bool W3dAtajoAgregar(int i, W3dCombo c, int* quitadaDe) {
    Preparar();
    if (quitadaDe) *quitadaDe = -1;
    if (i < 0 || i >= kNAtajos || c.Vacia() || W3dEsTeclaModificador(c.tecla)) return false;
    for (int j = 0; j < kNAtajos; j++) {
        if (j == i || !W3dAtajoMismoAlcance(kAtajos[j].ctx, kAtajos[i].ctx)) continue;
        std::vector<W3dCombo>& v = gActual[(size_t)j];
        for (size_t k = 0; k < v.size(); k++) if (v[k] == c) { v.erase(v.begin() + (long)k); if (quitadaDe) *quitadaDe = j; break; }
    }
    std::vector<W3dCombo>& mia = gActual[(size_t)i];
    for (size_t k = 0; k < mia.size(); k++) if (mia[k] == c) { W3dAtajosGuardar(); return true; }
    mia.push_back(c);
    W3dAtajosGuardar();
    return true;
}
void W3dAtajoVaciar(int i) {
    Preparar();
    if (i < 0 || i >= kNAtajos) return;
    gActual[(size_t)i].clear();
    W3dAtajosGuardar();
}
void W3dAtajoRestablecer(int i) {
    Preparar();
    if (i < 0 || i >= kNAtajos) return;
    // las por defecto vuelven a su funcion: si otra del mismo contexto las habia tomado, se las saca
    const std::vector<W3dCombo>& d = gDefecto[(size_t)i];
    for (int j = 0; j < kNAtajos; j++) {
        if (j == i || !W3dAtajoMismoAlcance(kAtajos[j].ctx, kAtajos[i].ctx)) continue;
        std::vector<W3dCombo>& v = gActual[(size_t)j];
        for (size_t k = 0; k < v.size();) {
            bool choca = false;
            for (size_t q = 0; q < d.size(); q++) if (v[k] == d[q]) choca = true;
            if (choca) v.erase(v.begin() + (long)k); else k++;
        }
    }
    gActual[(size_t)i] = d;
    W3dAtajosGuardar();
}
void W3dAtajosRestablecerTodo() {
    Preparar();
    gActual = gDefecto;
    W3dAtajosGuardar();
}
bool W3dAtajosFijar(int i, const char* lista) {
    Preparar();
    if (i < 0 || i >= kNAtajos) return false;
    std::vector<W3dCombo> v;
    const bool ok = ListaDesdeTexto(lista, v);
    gActual[(size_t)i] = v;
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------
//  Archivo: atajos.ini en la carpeta del usuario (junto al config.ini). Solo se escriben las funciones que el usuario cambio, asi una
//  version nueva del editor con otros atajos por defecto no queda pisada por un archivo viejo.
//      3d.mover = G, Ctrl+M
//      3d.borrar =            (sin combinacion)
// ---------------------------------------------------------------------------------------------------------------
// la carpeta ESCRIBIBLE del usuario (res/ es de solo lectura en el AppImage, el .deb y el APK). Sin carpeta (el
// telefono no la define), la ruta relativa de siempre.
std::string W3dAtajosRuta() {
    if (!gRuta.empty()) return gRuta;
    const std::string& dir = w3dFileSystem::GetUserDataDir();
    return dir.empty() ? std::string("atajos.ini") : dir + "/atajos.ini";
}
void W3dAtajosRutaFijar(const std::string& ruta) {
    Preparar();
    gRuta = ruta;
    gActual = gDefecto;
    gCargado = true;   // el que fija la ruta decide cuando cargar (W3dAtajosCargar)
}

bool W3dAtajosCargar() {
    Preparar();
    gCargado = true;
    gActual = gDefecto;
    FILE* f = fopen(W3dAtajosRuta().c_str(), "rb");
    if (!f) return false;
    char linea[512];
    while (fgets(linea, sizeof(linea), f)) {
        std::string l = Recortar(linea);
        if (l.empty() || l[0] == '#' || l[0] == ';') continue;
        const size_t ig = l.find('=');
        if (ig == std::string::npos) continue;
        const int i = W3dAtajoBuscar(Recortar(l.substr(0, ig)).c_str());
        if (i < 0) continue;   // una funcion que ya no existe: se ignora
        ListaDesdeTexto(l.substr(ig + 1).c_str(), gActual[(size_t)i]);
    }
    fclose(f);
    return true;
}

bool W3dAtajosGuardar() {
    Preparar();
    FILE* f = fopen(W3dAtajosRuta().c_str(), "wb");
    if (!f) return false;
    fprintf(f, "# Whisk3D: atajos de teclado cambiados (los que no figuran usan el de fabrica).\n");
    fprintf(f, "# funcion = combinacion, otra combinacion   (vacio = sin atajo)\n");
    for (int i = 0; i < kNAtajos; i++)
        if (!W3dAtajoEsDefecto(i)) fprintf(f, "%s = %s\n", kAtajos[i].id, W3dAtajoTexto(i).c_str());
    bool ok = (ferror(f) == 0);
    if (fclose(f) != 0) ok = false;
    extern void W3dWebPersistir(); W3dWebPersistir();   // web: IndexedDB (W3dConfigGuardar.cpp)
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------
//  El traductor
// ---------------------------------------------------------------------------------------------------------------
static bool Tiene(const std::vector<W3dCombo>& v, const W3dCombo& c) {
    for (size_t k = 0; k < v.size(); k++) if (v[k] == c) return true;
    return false;
}

bool W3dAtajoTraducir(const int* ctxs, int n, int& tecla, int& mods, bool repeticion) {
    if (!gCargado) W3dAtajosCargar();
    if (tecla == W3dK_NADA || W3dEsTeclaModificador(tecla) || n <= 0) return true;
    // en el editor de scripts lo que se tipea es TEXTO: solo se traducen las combinaciones con Ctrl/Alt y las F
    if (ctxs[0] == W3dATC_IDE && !(mods & (W3dMOD_CTRL | W3dMOD_ALT)) &&
        !(tecla == W3dK_F1 || tecla == W3dK_F2 || (tecla >= W3dK_F3 && tecla <= W3dK_F12))) return true;
    const W3dCombo p(tecla, mods);
    // contexto por contexto, del mas especifico al general: si ahi la combinacion es de una funcion, manda la
    // por defecto de esa funcion; si era la por defecto de alguna que ya no la tiene, queda libre (no hace nada)
    for (int c = 0; c < n; c++) {
        for (int i = 0; i < kNAtajos; i++) {
            if (kAtajos[i].ctx != ctxs[c] || !Tiene(gActual[(size_t)i], p)) continue;
            if (gDefecto[(size_t)i].empty()) {
                // sin tecla de fabrica: ningun handler la escucha -> corre la accion del menu (y la tecla no sigue)
                if (repeticion && kAtajos[i].accion) return false;   // mantenerla apretada no la repite
                if (W3dAtajoEjecutar(i)) return false;
                return true;
            }
            tecla = gDefecto[(size_t)i][0].tecla;
            mods  = gDefecto[(size_t)i][0].mods;
            return true;
        }
        for (int i = 0; i < kNAtajos; i++)
            if (kAtajos[i].ctx == ctxs[c] && Tiene(gDefecto[(size_t)i], p)) return false;
    }
    return true;
}

int W3dModsActuales() {
    return (LShiftPressed ? W3dMOD_SHIFT : 0) | (LCtrlPressed ? W3dMOD_CTRL : 0) | (LAltPressed ? W3dMOD_ALT : 0);
}

W3dModsTemporales::W3dModsTemporales() : activo(false), shift(false), ctrl(false), alt(false) {}
W3dModsTemporales::~W3dModsTemporales() {
    if (activo) { LShiftPressed = shift; LCtrlPressed = ctrl; LAltPressed = alt; }
}
void W3dModsTemporales::Fijar(int m) {
    if (!activo) { shift = LShiftPressed; ctrl = LCtrlPressed; alt = LAltPressed; activo = true; }
    LShiftPressed = (m & W3dMOD_SHIFT) != 0;
    LCtrlPressed  = (m & W3dMOD_CTRL) != 0;
    LAltPressed   = (m & W3dMOD_ALT) != 0;
}

int W3dAtajoContextos(int* c) {
    int n = 0;
    ViewportBase* vp = viewPortActive;
    const int kind = (vp && vp->isLeaf()) ? vp->ViewportKind() : 0;
    bool xf = false;
    if (kind == 1)      xf = (estado != editNavegacion) || EditXformActivo() || BoneXformModo() != 0 || BoneGrabActivo() || g_poseModo != 0;
    else if (kind == 4 || kind == 6) xf = vp->XformEnCurso();
    else if (kind == 5) xf = DopeXformActivo();
    if (xf) c[n++] = W3dATC_XFORM;
    switch (kind) {
        case 1:
            // el MODO primero (lo suyo tapa a lo comun): Edicion de malla o de huesos, si no Objeto (y Pose)
            c[n++] = (InteractionMode == EditMode || BoneEditActivo()) ? W3dATC_3D_EDIT : W3dATC_3D_OBJ;
            c[n++] = W3dATC_3D; break;
        case 2: case 9: c[n++] = W3dATC_OUTLINER; break;   // (la biblioteca es un outliner)
        case 3: c[n++] = W3dATC_PROPS; break;
        case 4: c[n++] = W3dATC_UV; break;
        case 5: c[n++] = W3dATC_TIMELINE; break;
        case 6: c[n++] = W3dATC_2D; break;
        case 8: c[n++] = W3dATC_IDE; break;
        default: break;
    }
    c[n++] = W3dATC_GLOBAL;
    return n;
}

// ---------------------------------------------------------------------------------------------------------------
//  Captura (la UI de reasignar)
// ---------------------------------------------------------------------------------------------------------------
bool W3dAtajoCapturar(int tecla, int mods) {
    if (g_atajoCapturando < 0) return false;
    if (tecla == W3dK_NADA || W3dEsTeclaModificador(tecla)) return true;   // sigue esperando la tecla de verdad
    const int i = g_atajoCapturando;
    g_atajoCapturando = -1;
    if (tecla == W3dK_ESCAPE && mods == 0) { Notificar(T("Cancelled"), false); return true; }
    const W3dCombo c(tecla, mods);
    int quitada = -1;
    if (!W3dAtajoAgregar(i, c, &quitada)) return true;
    std::string msg = W3dComboTexto(c) + " -> " + T(kAtajos[i].nombre);
    if (quitada >= 0) msg += std::string("  (") + T("removed from") + " " + T(kAtajos[quitada].nombre) + ")";
    Notificar(msg, false);
    return true;
}
