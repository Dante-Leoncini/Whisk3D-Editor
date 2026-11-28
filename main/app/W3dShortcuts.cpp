// ============================================================================
//  W3dShortcuts.cpp - la tabla de atajos reasignables y su traductor (ver W3dShortcuts.h).
//  C++03: compila tambien en el telefono.
// ============================================================================
#include "W3dShortcuts.h"
#include "base/w3dlog.h"
#include <set>
#include "ViewPorts/W3dInput.h"
#include "ViewPorts/ViewPorts.h"       // viewPortActive
#include "ViewPorts/LayoutInput.h"     // EditXformActive
#include "ViewPorts/Timeline.h"        // DopeXformActive
#include "edit/BoneEdit.h"             // BoneXformMode / BoneGrabActive
#include "base/W3dInteractionState.h"  // estado (transform de objetos en curso)
#include "w3dFilesystem.h"
#include "W3dLang.h"
#include "W3dNotice.h"                  // Notificar
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

extern bool LShiftPressed, LCtrlPressed, LAltPressed;
extern int g_poseMode;

// ---------------------------------------------------------------------------------------------------------------
//  LA TABLA. Dentro de un mismo contexto ninguna combinacion por defecto se repite: la primera de cada fila es la
//  que escucha el handler, y si dos funciones compartieran una no habria forma de separarlas.
//  (La prueba prueba_atajos.w3s lo verifica con 'atajoscheck'.)
// ---------------------------------------------------------------------------------------------------------------
static const W3dShortcutDef kShortcuts[] = {
    // ---- en cualquier viewport ----
    { "global.save",       W3dATC_GLOBAL,   "Save project",               "Ctrl+S" },
    { "global.undo",      W3dATC_GLOBAL,   "Undo",                       "Ctrl+Z", "C+Left", NULL },
    { "global.redo",       W3dATC_GLOBAL,   "Redo",                       "Ctrl+Y, Ctrl+Shift+Z", "C+Right", NULL },
    { "global.mode",          W3dATC_GLOBAL,   "Object / Edit Mode",         "Tab", "OK", NULL },
    { "global.snap",          W3dATC_GLOBAL,   "Toggle Snap",                "Shift+Tab" },
    { "global.menuSnap",      W3dATC_GLOBAL,   "Snap menu",                  "Shift+S", NULL, "Shift S" },
    { "global.play",          W3dATC_GLOBAL,   "Play / Pause",               "Space", "Green+Left soft", NULL },
    { "global.menuBar",     W3dATC_GLOBAL,   "Menu bar",                   "M", "Left soft", NULL },
    { "global.parent",    W3dATC_GLOBAL,   "Parent",                     "Ctrl+P", NULL, "Ctrl P" },
    { "global.clearParent", W3dATC_GLOBAL,   "Clear Parent",               "Alt+P", NULL, "Alt P" },
    { "global.screen",      W3dATC_GLOBAL,   "Fullscreen",                 "F11" },
    { "global.render",        W3dATC_GLOBAL,   "Render Image",               "F12" },
    { "global.renderAnim",    W3dATC_GLOBAL,   "Render Animation",           "Ctrl+F12" },

    // ---- vista 3D ----
    { "3d.selectAll",   W3dATC_3D,       "Select All",                 "A", "Pencil+Up", "A" },
    { "3d.deselect",     W3dATC_3D,       "Deselect All",               "Alt+A", "Pencil+Up", "A" },
    { "3d.invert",          W3dATC_3D,       "Invert Selection",           "Ctrl+I" },
    { "3d.box",              W3dATC_3D,       "Box Select",                 "B" },
    { "3d.linked",         W3dATC_3D,       "Select Linked",              "L" },
    { "3d.add",           W3dATC_3D,       "Add",                        "Shift+A" },
    { "3d.apply",           W3dATC_3D_OBJ,   "Apply",                      "Ctrl+A" },
    { "3d.move",             W3dATC_3D,       "Move",                       "G", "1", "G" },
    { "3d.rotate",             W3dATC_3D,       "Rotate",                     "R", "2", "R" },
    { "3d.scale",           W3dATC_3D,       "Scale",                      "S", "3", "S" },
    { "3d.clearPos",        W3dATC_3D,       "Clear Location (Pose)",      "Alt+G" },
    { "3d.clearRot",        W3dATC_3D,       "Clear Rotation (Pose)",      "Alt+R" },
    { "3d.clearScale",        W3dATC_3D,       "Shrink/Fatten / Clear Scale", "Alt+S" },
    { "3d.duplicate",          W3dATC_3D,       "Duplicate",                  "Shift+D", NULL, "Shift D" },
    { "3d.instance",         W3dATC_3D,       "Duplicate Linked",           "Alt+D", NULL, "Alt D" },
    { "3d.delete",            W3dATC_3D,       "Delete",                     "X, Delete", "C", "X" },
    { "3d.hide",           W3dATC_3D_OBJ,   "Hide / Show",                "H" },   // (en Edit Mode la H oculta la seleccion: 3d.hideSelected)
    { "3d.join",              W3dATC_3D_OBJ,   "Join",                       "Ctrl+J" },
    { "3d.keyframe",          W3dATC_3D_OBJ,   "Insert Keyframe",            "I" },
    { "3d.keyframeAll",      W3dATC_3D_OBJ,   "Insert Keyframe (all)",      "Shift+I" },
    { "3d.proportional",      W3dATC_3D,       "Proportional Editing",       "O" },
    { "3d.overlays",          W3dATC_3D_OBJ,   "Overlays",                   "K" },   // (en Edit Mode de una malla la K es el cuchillo: 3d.knife)
    { "3d.viewLocal",        W3dATC_3D,       "Local View",                 "/" },
    { "3d.focus",         W3dATC_3D,       "Frame Selected",             "Num.", "0", "." },
    { "3d.viewFront",       W3dATC_3D,       "Front View",                 "Num1", "#+Left/Right", NULL },
    { "3d.viewBack",        W3dATC_3D,       "Back View",                  "Ctrl+Num1", "#+Left/Right", NULL },
    { "3d.viewRight",      W3dATC_3D,       "Right View",                 "Num3", "#+Left/Right", NULL },
    { "3d.viewLeft",    W3dATC_3D,       "Left View",                  "Ctrl+Num3", "#+Left/Right", NULL },
    { "3d.viewTop",       W3dATC_3D,       "Top View",                   "Num7", "#+Up", NULL },
    { "3d.viewBottom",        W3dATC_3D,       "Bottom View",                "Ctrl+Num7", "#+Down", NULL },
    { "3d.perspective",       W3dATC_3D,       "Perspective / Orthographic", "Num5", "#", NULL },
    { "3d.viewMode",         W3dATC_3D,       "Next Viewport Shading",      "Z", "6", NULL },   // (mantenida: el menu de los modos)
    { "3d.xray",             W3dATC_3D,       "X-Ray",                      "Alt+Z" },   // (aparte del modo de vista)
    { "3d.rollLeft",          W3dATC_3D,       "Roll Left",                  "Num4" },
    { "3d.rollRight",          W3dATC_3D,       "Roll Right",                 "Num6" },
    { "3d.viewCamera",       W3dATC_3D,       "Camera View",                "Num0", "8", NULL },
    { "3d.activeCamera",      W3dATC_3D,       "Set Active Camera",          "Ctrl+Num0" },
    { "3d.extrude",           W3dATC_3D_EDIT,  "Extrude",                    "E", "7", NULL },
    { "3d.face",              W3dATC_3D_EDIT,  "New Face from Vertices",     "F" },
    { "3d.normals",          W3dATC_3D_EDIT,  "Recalculate Normals",        "Shift+N" },
    { "3d.hideSelected",     W3dATC_3D_EDIT,  "Hide Selected",              "H" },
    { "3d.hideUnselected",   W3dATC_3D_EDIT,  "Hide Unselected",            "Shift+H" },
    { "3d.reveal",           W3dATC_3D_EDIT,  "Reveal Hidden",              "Alt+H" },
    { "3d.loopcut",           W3dATC_3D_EDIT,  "Loop Cut and Slide",         "Ctrl+R", "8", NULL },
    { "3d.knife",             W3dATC_3D_EDIT,  "Knife",                      "K" },   // (fuera de Edit Mode, K = Overlays)
    { "3d.connect",          W3dATC_3D_EDIT,  "Connect Vertex Path",        "J" },
    { "3d.rip",               W3dATC_3D_EDIT,  "Rip",                        "V" },
    { "3d.slide",             W3dATC_3D_EDIT,  "Vertex Slide",               "Shift+V" },
    { "3d.separate",           W3dATC_3D_EDIT,  "Separate",                   "P" },
    { "3d.merge",             W3dATC_3D_EDIT,  "Merge",                      "M" },
    { "3d.contextMenu",      W3dATC_3D,       "Context menu",               "W" },
    { "3d.triangulate",        W3dATC_3D_EDIT,  "Triangulate",                "Ctrl+T" },
    { "3d.menuUV",            W3dATC_3D_EDIT,  "UV menu",                    "U" },
    { "3d.cyclic",            W3dATC_3D_EDIT,  "Toggle Cyclic (curve)",      "Alt+C" },
    // ---- vista 3D: lo que hacia falta y sale del teclado del handler (estaba solo como texto en el menu) ----
    { "3d.inset",             W3dATC_3D_EDIT,  "Inset Faces",                "I" },
    { "3d.bevel",             W3dATC_3D_EDIT,  "Bevel Edges",                "Ctrl+B" },
    { "3d.bevelVertices",         W3dATC_3D_EDIT,  "Bevel Vertices",             "Ctrl+Shift+B" },
    { "3d.cut",            W3dATC_3D_OBJ,   "Cut",                        "Ctrl+X" },
    { "3d.copy",            W3dATC_3D_OBJ,   "Copy",                       "Ctrl+C" },
    { "3d.paste",             W3dATC_3D_OBJ,   "Paste",                      "Ctrl+V" },

    // ---- vista 3D: operaciones SIN tecla de fabrica (las de los menus; el usuario les pone una si quiere) ----
    { "3d.geoToOrigin",        W3dATC_3D_OBJ,   "Geometry to Origin",         "", NULL, NULL, "obj:200" },
    { "3d.originToGeo",        W3dATC_3D_OBJ,   "Origin to Geometry",         "", NULL, NULL, "obj:201" },
    { "3d.originToCursor",     W3dATC_3D_OBJ,   "Origin to 3D Cursor",        "", NULL, NULL, "obj:202" },
    { "3d.applyPos",        W3dATC_3D_OBJ,   "Apply Location",             "", NULL, NULL, "obj:220" },
    { "3d.applyRot",        W3dATC_3D_OBJ,   "Apply Rotation",             "", NULL, NULL, "obj:221" },
    { "3d.applyScale",        W3dATC_3D_OBJ,   "Apply Scale",                "", NULL, NULL, "obj:222" },
    { "3d.applyAll",       W3dATC_3D_OBJ,   "Apply All Transforms",       "", NULL, NULL, "obj:223" },
    { "3d.keyPos",            W3dATC_3D_OBJ,   "Insert Location Keyframe",   "", NULL, NULL, "obj:531" },
    { "3d.keyRot",            W3dATC_3D_OBJ,   "Insert Rotation Keyframe",   "", NULL, NULL, "obj:532" },
    { "3d.keyEsc",            W3dATC_3D_OBJ,   "Insert Scale Keyframe",      "", NULL, NULL, "obj:533" },
    { "3d.createPrefab",       W3dATC_3D_OBJ,   "Create Prefab",              "", NULL, NULL, "obj:700" },
    { "3d.unpackPrefab",    W3dATC_3D_OBJ,   "Unpack Prefab",              "", NULL, NULL, "obj:701" },
    { "3d.smooth",             W3dATC_3D_EDIT,  "Shade Smooth",               "", NULL, NULL, "obj:320" },
    { "3d.plane",             W3dATC_3D_EDIT,  "Shade Flat",                 "", NULL, NULL, "obj:321" },
    { "3d.flipNormals",   W3dATC_3D_EDIT,  "Flip Normals",               "", NULL, NULL, "obj:324" },
    { "3d.markSharp",     W3dATC_3D_EDIT,  "Mark Sharp",                 "", NULL, NULL, "obj:330" },
    { "3d.clearSharp",    W3dATC_3D_EDIT,  "Clear Sharp",                "", NULL, NULL, "obj:331" },
    { "3d.markSeam",     W3dATC_3D_EDIT,  "Mark Seam",                  "", NULL, NULL, "obj:358" },
    { "3d.clearSeam",    W3dATC_3D_EDIT,  "Clear Seam",                 "", NULL, NULL, "obj:359" },
    { "3d.joinCenter",        W3dATC_3D_EDIT,  "Merge at Center",            "", NULL, NULL, "obj:380" },
    { "3d.joinCursor",        W3dATC_3D_EDIT,  "Merge at Cursor",            "", NULL, NULL, "obj:381" },
    { "3d.collapse",          W3dATC_3D_EDIT,  "Collapse",                   "", NULL, NULL, "obj:382" },
    { "3d.joinDistance",     W3dATC_3D_EDIT,  "Merge by Distance",          "", NULL, NULL, "obj:383" },
    { "3d.deleteVerts",       W3dATC_3D_EDIT,  "Delete Vertices",            "", NULL, NULL, "obj:361" },
    { "3d.deleteEdges",     W3dATC_3D_EDIT,  "Delete Edges",               "", NULL, NULL, "obj:362" },
    { "3d.deleteFaces",       W3dATC_3D_EDIT,  "Delete Faces",               "", NULL, NULL, "obj:363" },
    { "3d.deleteLoops",       W3dATC_3D_EDIT,  "Delete Edge Loops",          "", NULL, NULL, "obj:364" },
    { "3d.unwrap",            W3dATC_3D_EDIT,  "Unwrap Angle Based",         "", NULL, NULL, "obj:350" },
    { "3d.smartUV",           W3dATC_3D_EDIT,  "Smart UV Project",           "", NULL, NULL, "obj:351" },
    { "3d.projectFromView",         W3dATC_3D_EDIT,  "Project from View",          "", NULL, NULL, "obj:356" },
    { "3d.lockOrbit",    W3dATC_3D,       "Lock Orbit",                 "", NULL, NULL, "view3d:421" },
    { "3d.lockCamera",    W3dATC_3D,       "Lock the camera to the view", "", NULL, NULL, "view3d:423" },
    { "3d.showRender",         W3dATC_3D,       "Render Preview",             "", NULL, NULL, "shading:0" },
    { "3d.showMaterial",       W3dATC_3D,       "Material Preview",           "", NULL, NULL, "shading:1" },
    { "3d.showSolid",         W3dATC_3D,       "Solid Preview",              "", NULL, NULL, "shading:2" },
    { "3d.showWire",        W3dATC_3D,       "Wireframe Preview",          "", NULL, NULL, "shading:3" },

    // ---- con un transform en curso ----
    { "xf.confirm",         W3dATC_XFORM,    "Confirm",                    "Enter, NumEnter", "OK", "Enter" },
    { "xf.cancel",          W3dATC_XFORM,    "Cancel",                     "Esc", "C", "Esc" },
    { "xf.axisX",              W3dATC_XFORM,    "X axis",                     "X", "1", NULL },
    { "xf.axisY",              W3dATC_XFORM,    "Y axis",                     "Y", "2", NULL },
    { "xf.axisZ",              W3dATC_XFORM,    "Z axis",                     "Z", "3", NULL },
    { "xf.planeX",            W3dATC_XFORM,    "Exclude X (plane)",          "Shift+X" },
    { "xf.planeY",            W3dATC_XFORM,    "Exclude Y (plane)",          "Shift+Y" },
    { "xf.planeZ",            W3dATC_XFORM,    "Exclude Z (plane)",          "Shift+Z" },
    { "xf.radiusPlus",          W3dATC_XFORM,    "Proportional radius +",      "PageUp" },
    { "xf.radiusMinus",        W3dATC_XFORM,    "Proportional radius -",      "PageDown" },

    // ---- editor UV ----
    { "uv.selectAll",   W3dATC_UV,       "Select All",                 "A" },
    { "uv.deselect",     W3dATC_UV,       "Deselect All",               "Alt+A" },
    { "uv.invert",          W3dATC_UV,       "Invert Selection",           "Ctrl+I" },
    { "uv.linked",         W3dATC_UV,       "Select Linked",              "L" },
    { "uv.box",              W3dATC_UV,       "Box Select",                 "B" },
    { "uv.move",             W3dATC_UV,       "Move",                       "G", "1", NULL },
    { "uv.rotate",             W3dATC_UV,       "Rotate",                     "R", "2", NULL },
    { "uv.scale",           W3dATC_UV,       "Scale",                      "S", "3", NULL },
    { "uv.keyframe",          W3dATC_UV,       "Insert Keyframe",            "I" },
    { "uv.keyframeAll",      W3dATC_UV,       "Insert Keyframe (all)",      "Shift+I" },
    { "uv.extrude",           W3dATC_UV,       "Extrude",                    "E" },
    { "uv.duplicate",          W3dATC_UV,       "Duplicate",                  "Shift+D" },
    { "uv.delete",            W3dATC_UV,       "Delete",                     "X, Delete" },
    { "uv.focus",         W3dATC_UV,       "Frame Selected",             "Num." },
    { "uv.focusAll",     W3dATC_UV,       "Frame All",                  "", NULL, NULL, "uvvista:11" },
    { "uv.cursorASel",        W3dATC_UV,       "Cursor to Selection",        "", NULL, NULL, "uvcursor:0" },
    { "uv.selACursor",        W3dATC_UV,       "Selection to Cursor",        "", NULL, NULL, "uvcursor:1" },
    { "uv.cursorCenter",      W3dATC_UV,       "Cursor to Center",           "", NULL, NULL, "uvcursor:2" },

    // ---- timeline ----
    { "tl.selectAll",   W3dATC_TIMELINE, "Select All",                 "A" },
    { "tl.deselect",     W3dATC_TIMELINE, "Deselect All",               "Alt+A" },
    { "tl.invert",          W3dATC_TIMELINE, "Invert Selection",           "Ctrl+I" },
    { "tl.move",             W3dATC_TIMELINE, "Move",                       "G", "1", NULL },
    { "tl.scale",           W3dATC_TIMELINE, "Scale",                      "S", "3", NULL },
    { "tl.rotate",             W3dATC_TIMELINE, "Rotate handles",             "R", "2", NULL },
    { "tl.duplicate",          W3dATC_TIMELINE, "Duplicate",                  "Ctrl+D, Shift+D" },
    { "tl.cut",            W3dATC_TIMELINE, "Cut",                        "Ctrl+X" },
    { "tl.copy",            W3dATC_TIMELINE, "Copy",                       "Ctrl+C" },
    { "tl.paste",             W3dATC_TIMELINE, "Paste",                      "Ctrl+V" },
    { "tl.delete",            W3dATC_TIMELINE, "Delete",                     "X" },
    { "tl.interp",            W3dATC_TIMELINE, "Interpolation",              "E, T" },
    { "tl.easing",            W3dATC_TIMELINE, "Easing Type",                "Ctrl+E" },
    { "tl.handle",            W3dATC_TIMELINE, "Handle type",                "V" },
    { "tl.focus",         W3dATC_TIMELINE, "Frame Selected",             "Num.", "0", NULL },
    { "tl.prevFrame",          W3dATC_TIMELINE, "Previous frame",             "Left" },
    { "tl.nextFrame",          W3dATC_TIMELINE, "Next frame",                 "Right" },
    { "tl.prevKey",            W3dATC_TIMELINE, "Previous keyframe",          "Down" },
    { "tl.nextKey",            W3dATC_TIMELINE, "Next keyframe",              "Up" },
    { "tl.start",            W3dATC_TIMELINE, "Go to start",                "Home" },
    { "tl.end",               W3dATC_TIMELINE, "Go to end",                  "End" },
    { "tl.showDope",           W3dATC_TIMELINE, "Dope Sheet",                 "", NULL, NULL, "tlvista:6" },
    { "tl.showCurves",         W3dATC_TIMELINE, "Curves",                     "", NULL, NULL, "tlvista:7" },
    { "tl.panel",             W3dATC_TIMELINE, "Show Panel",                 "", NULL, NULL, "tlvista:100" },
    { "tl.showHandles",        W3dATC_TIMELINE, "Show Handles",               "", NULL, NULL, "tlvista:101" },
    { "tl.smartEuler",        W3dATC_TIMELINE, "Smart Euler",                "", NULL, NULL, "tlkey:2" },
    { "tl.constantInterp",   W3dATC_TIMELINE, "Interpolation: Constant",    "", NULL, NULL, "tlkey:20" },
    { "tl.linearInterp",      W3dATC_TIMELINE, "Interpolation: Linear",      "", NULL, NULL, "tlkey:21" },
    { "tl.interpBezier",      W3dATC_TIMELINE, "Interpolation: Bezier",      "", NULL, NULL, "tlkey:22" },
    { "tl.freeHandle",       W3dATC_TIMELINE, "Handles: Free",              "", NULL, NULL, "tlkey:40" },
    { "tl.alignedHandle",    W3dATC_TIMELINE, "Handles: Aligned",           "", NULL, NULL, "tlkey:41" },
    { "tl.handleVector",      W3dATC_TIMELINE, "Handles: Vector",            "", NULL, NULL, "tlkey:42" },
    { "tl.handleAuto",        W3dATC_TIMELINE, "Handles: Automatic",         "", NULL, NULL, "tlkey:43" },
    { "tl.autoClampedHandle",  W3dATC_TIMELINE, "Handles: Auto Clamped",      "", NULL, NULL, "tlkey:44" },
    { "tl.flatHandle",       W3dATC_TIMELINE, "Handles: Flat",              "", NULL, NULL, "tlkey:45" },

    // ---- outliner ----
    { "out.selectAll",  W3dATC_OUTLINER, "Select All",                 "A" },
    { "out.deselect",    W3dATC_OUTLINER, "Deselect All",               "Alt+A" },
    { "out.invert",         W3dATC_OUTLINER, "Invert Selection",           "Ctrl+I" },
    { "out.move",            W3dATC_OUTLINER, "Move (reorder)",             "G", "1", NULL },
    { "out.rename",        W3dATC_OUTLINER, "Rename",                     "F2" },
    { "out.duplicate",         W3dATC_OUTLINER, "Duplicate",                  "Shift+D" },
    { "out.instance",        W3dATC_OUTLINER, "Duplicate Linked",           "Alt+D" },
    { "out.hide",          W3dATC_OUTLINER, "Hide / Show",                "H", "2", NULL },
    { "out.delete",           W3dATC_OUTLINER, "Delete",                     "X, Delete", "C", NULL },
    { "out.focus",        W3dATC_OUTLINER, "Show active",                "Num." },

    // ---- editor 2D ----
    { "2d.selectAll",   W3dATC_2D,       "Select All",                 "A" },
    { "2d.deselect",     W3dATC_2D,       "Deselect All",               "Alt+A" },
    { "2d.invert",          W3dATC_2D,       "Invert Selection",           "Ctrl+I" },
    { "2d.move",             W3dATC_2D,       "Move",                       "G", "1", NULL },
    { "2d.rotate",             W3dATC_2D,       "Rotate",                     "R", "2", NULL },
    { "2d.scale",           W3dATC_2D,       "Scale",                      "S", "3", NULL },
    { "2d.duplicate",          W3dATC_2D,       "Duplicate",                  "Shift+D" },
    { "2d.delete",            W3dATC_2D,       "Delete",                     "X, Delete" },
    { "2d.focus",         W3dATC_2D,       "Frame Selected",             "Num." },
    { "2d.gameView",        W3dATC_2D,       "Game View",                  "Num0" },
    { "2d.parent",        W3dATC_2D,       "Set Parent",                 "", NULL, NULL, "2dobj:40" },
    { "2d.clearParent",     W3dATC_2D,       "Clear Parent",               "", NULL, NULL, "2dobj:41" },
    { "2d.zoom1",             W3dATC_2D,       "Zoom 1:1",                   "", NULL, NULL, "2dvista:1" },
    { "2d.zoom2",             W3dATC_2D,       "Zoom 2:1",                   "", NULL, NULL, "2dvista:2" },

    // ---- propiedades (lista de pasos de una rutina) ----
    { "props.grab",        W3dATC_PROPS,    "Grab step",                  "G" },
    { "props.delete",         W3dATC_PROPS,    "Delete step",                "X, Delete" },
    { "props.keyframe",       W3dATC_PROPS,    "Insert Keyframe",            "I" },

    // ---- editor de scripts ----
    { "ide.save",          W3dATC_IDE,      "Save script",                "Ctrl+S" },
    { "ide.undo",         W3dATC_IDE,      "Undo",                       "Ctrl+Z" },
    { "ide.copy",           W3dATC_IDE,      "Copy",                       "Ctrl+C" },
    { "ide.cut",           W3dATC_IDE,      "Cut",                        "Ctrl+X" },
    { "ide.paste",            W3dATC_IDE,      "Paste",                      "Ctrl+V" },
    { "ide.all",             W3dATC_IDE,      "Select All",                 "Ctrl+A" },
    { "ide.redo",          W3dATC_IDE,      "Redo",                       "Ctrl+Y, Ctrl+Shift+Z" },
    { "ide.find",           W3dATC_IDE,      "Find",                       "Ctrl+F" },
    { "ide.replace",       W3dATC_IDE,      "Replace",                    "Ctrl+H" },
};
static const int kNShortcuts = (int)(sizeof(kShortcuts) / sizeof(kShortcuts[0]));

static std::vector< std::vector<W3dCombo> > gDefault;   // parseadas una vez
static std::vector< std::vector<W3dCombo> > gCurrent;   // las del usuario
static bool gLoaded = false;
static std::string gPath;                               // "" = res/shortcuts.start
int g_shortcutCapturing = -1;

// ---------------------------------------------------------------------------------------------------------------
//  Nombres de las teclas. En ingles y cortos: son los que se leen en el teclado, y el archivo tiene que leerse igual
//  en cualquier idioma del editor. ',' y '+' tienen nombre propio porque separan la lista y la combinacion.
// ---------------------------------------------------------------------------------------------------------------
struct KeyName { int key; const char* name; };
static const KeyName kNames[] = {
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
static const int kNNames = (int)(sizeof(kNames) / sizeof(kNames[0]));

static bool EqualNoCaps(const std::string& a, const char* b) {
    size_t n = strlen(b);
    if (a.size() != n) return false;
    for (size_t i = 0; i < n; i++) if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}
static std::string Crop(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}

bool W3dIsKeyModifier(int key) {
    return key == W3dK_LSHIFT || key == W3dK_LCTRL || key == W3dK_LALT;
}

std::string W3dComboText(W3dCombo c) {
    if (c.Empty()) return std::string();
    std::string s;
    if (c.mods & W3dMOD_CTRL)  s += "Ctrl+";
    if (c.mods & W3dMOD_SHIFT) s += "Shift+";
    if (c.mods & W3dMOD_ALT)   s += "Alt+";
    for (int i = 0; i < kNNames; i++) if (kNames[i].key == c.key) return s + kNames[i].name;
    if (c.key > 32 && c.key < 127) {
        char b[2]; b[0] = (char)toupper(c.key); b[1] = 0;
        return s + b;
    }
    char b[16]; sprintf(b, "#%d", c.key);   // una tecla sin nombre igual se puede guardar y releer
    return s + b;
}

bool W3dComboFromText(const std::string& text, W3dCombo& c) {
    std::string s = Crop(text);
    c = W3dCombo();
    if (s.empty()) return false;
    int mods = 0;
    for (;;) {   // los prefijos "Ctrl+", "Shift+", "Alt+" (la tecla es lo que queda: "Ctrl+Plus", "Num.", "/")
        size_t more = s.find('+');
        if (more == std::string::npos || more + 1 >= s.size()) break;
        const std::string pre = Crop(s.substr(0, more));
        if (EqualNoCaps(pre, "ctrl") || EqualNoCaps(pre, "control")) mods |= W3dMOD_CTRL;
        else if (EqualNoCaps(pre, "shift")) mods |= W3dMOD_SHIFT;
        else if (EqualNoCaps(pre, "alt")) mods |= W3dMOD_ALT;
        else break;
        s = Crop(s.substr(more + 1));
    }
    if (s.empty()) return false;
    for (int i = 0; i < kNNames; i++) if (EqualNoCaps(s, kNames[i].name)) { c = W3dCombo(kNames[i].key, mods); return true; }
    if (s[0] == '#' && s.size() > 1) { c = W3dCombo(atoi(s.c_str() + 1), mods); return c.key > 0; }
    if (s.size() == 1 && (unsigned char)s[0] > 32 && (unsigned char)s[0] < 127) {
        c = W3dCombo(tolower((unsigned char)s[0]), mods);   // las letras viajan en minuscula (W3dK_G == 'g')
        return true;
    }
    return false;
}

// Como se ve en un menu: espacios en vez de '+' ("Ctrl Shift D") y el keypad separado ("Num 0", "Num .").
std::string W3dComboTextMenu(W3dCombo c) {
    std::string t = W3dComboText(c), out;
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
bool W3dSymbianKeyboardBT();   // platform/symbian/src/w3dmouse.cpp: llego alguna tecla del teclado Bluetooth
bool W3dSymbianMouseBT();      // idem, el mouse Bluetooth se movio
#endif

std::string W3dShortcutMenuLabel(const std::string& hint) {
    if (hint.empty()) return std::string();
#ifdef W3D_SYMBIAN
    const bool keyboardBT = W3dSymbianKeyboardBT(), mouseBT = W3dSymbianMouseBT();
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
    const int i = W3dShortcutFind(hint.c_str() + 1);
    if (i < 0) {
        // (una clave que no esta en la tabla: el menu queda sin su atajo y nadie se entera. Paso con 83 opciones cuando
        //  la tabla paso a ingles y los menus seguian pidiendo "@3d.mover": se avisa en el log, una vez por clave)
        static std::set<std::string> told;
        if (told.insert(hint).second) w3dLogfW("[atajos] un menu pide '%s', que no esta en la tabla (W3dShortcuts.cpp)", hint.c_str());
        return std::string();
    }
#ifdef W3D_SYMBIAN
    // el teclado BT del N95 tiene su propio mapa fijo (w3dmouse.cpp): lo que se reasigna en PC no lo toca
    if (keyboardBT && kShortcuts[i].bt) return kShortcuts[i].bt;
    return kShortcuts[i].phone ? std::string(T(kShortcuts[i].phone)) : std::string();
#else
    const std::vector<W3dCombo>& v = W3dShortcutCombos(i);
    return v.empty() ? std::string() : W3dComboTextMenu(v[0]);
#endif
}

static bool ListFromText(const char* text, std::vector<W3dCombo>& out) {
    out.clear();
    std::string s = text ? text : "";
    bool ok = true;
    size_t start = 0;
    while (start <= s.size()) {
        size_t comma = s.find(',', start);
        const std::string part = Crop(s.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!part.empty()) {
            W3dCombo c;
            if (W3dComboFromText(part, c)) {
                bool repeated = false;
                for (size_t k = 0; k < out.size(); k++) if (out[k] == c) repeated = true;
                if (!repeated) out.push_back(c);
            } else ok = false;
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return ok;
}

static void Prepare() {
    if (!gDefault.empty()) return;
    gDefault.resize((size_t)kNShortcuts);
    for (int i = 0; i < kNShortcuts; i++) ListFromText(kShortcuts[i].defaultValue, gDefault[(size_t)i]);
    gCurrent = gDefault;
}

int W3dShortcutsCount() { return kNShortcuts; }
const W3dShortcutDef& W3dShortcutInfo(int i) { return kShortcuts[(i >= 0 && i < kNShortcuts) ? i : 0]; }
const std::vector<W3dCombo>& W3dShortcutCombos(int i) {
    Prepare();
    return gCurrent[(size_t)((i >= 0 && i < kNShortcuts) ? i : 0)];
}
std::string W3dShortcutText(int i) {
    const std::vector<W3dCombo>& v = W3dShortcutCombos(i);
    std::string s;
    for (size_t k = 0; k < v.size(); k++) { if (k) s += ", "; s += W3dComboText(v[k]); }
    return s;
}
int W3dShortcutFind(const char* id) {
    if (!id) return -1;
    for (int i = 0; i < kNShortcuts; i++) if (strcmp(kShortcuts[i].id, id) == 0) return i;
    return -1;
}
bool W3dShortcutIsDefault(int i) {
    Prepare();
    if (i < 0 || i >= kNShortcuts) return true;
    const std::vector<W3dCombo>& a = gCurrent[(size_t)i];
    const std::vector<W3dCombo>& d = gDefault[(size_t)i];
    if (a.size() != d.size()) return false;
    for (size_t k = 0; k < a.size(); k++) if (!(a[k] == d[k])) return false;
    return true;
}
const char* W3dShortcutCtxName(int ctx) {
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

bool W3dShortcutSameScope(int a, int b) {
    if (a == b) return true;
    const bool modeA = (a == W3dATC_3D_OBJ || a == W3dATC_3D_EDIT), modeB = (b == W3dATC_3D_OBJ || b == W3dATC_3D_EDIT);
    return (a == W3dATC_3D && modeB) || (b == W3dATC_3D && modeA);
}

// ---------------------------------------------------------------------------------------------------------------
//  Las acciones de las operaciones sin tecla de fabrica. Una tabla chica de nombre -> funcion (la llenan los que
//  arman los menus, una vez al arrancar). Vive en una funcion para no depender del orden de inicializacion.
// ---------------------------------------------------------------------------------------------------------------
struct DispatchShortcut { char name[16]; W3dShortcutDispatch fn; };
static DispatchShortcut* Dispatches(int*& n) {
    static DispatchShortcut table[24];
    static int count = 0;
    n = &count;
    return table;
}
void W3dShortcutRegisterDispatch(const char* name, W3dShortcutDispatch fn) {
    int* n; DispatchShortcut* t = Dispatches(n);
    if (!name) return;
    for (int i = 0; i < *n; i++) if (strcmp(t[i].name, name) == 0) { t[i].fn = fn; return; }
    if (*n >= 24) return;
    strncpy(t[*n].name, name, 15); t[*n].name[15] = 0;
    t[*n].fn = fn;
    (*n)++;
}
// "obj:201" -> la funcion de "obj" y el 201
static W3dShortcutDispatch FindDispatch(const char* action, int& id) {
    id = 0;
    if (!action) return NULL;
    const char* dp = strchr(action, ':');
    if (!dp) return NULL;
    const size_t length = (size_t)(dp - action);
    id = atoi(dp + 1);
    int* n; DispatchShortcut* t = Dispatches(n);
    for (int i = 0; i < *n; i++)
        if (strlen(t[i].name) == length && strncmp(t[i].name, action, length) == 0) return t[i].fn;
    return NULL;
}
bool W3dShortcutHasDispatch(const char* action) { int id; return FindDispatch(action, id) != NULL; }
bool W3dShortcutRun(int i) {
    if (i < 0 || i >= kNShortcuts || !kShortcuts[i].action) return false;
    int id; W3dShortcutDispatch fn = FindDispatch(kShortcuts[i].action, id);
    if (!fn) return false;
    fn(id);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
//  Cambios
// ---------------------------------------------------------------------------------------------------------------
bool W3dShortcutAdd(int i, W3dCombo c, int* removedOf) {
    Prepare();
    if (removedOf) *removedOf = -1;
    if (i < 0 || i >= kNShortcuts || c.Empty() || W3dIsKeyModifier(c.key)) return false;
    for (int j = 0; j < kNShortcuts; j++) {
        if (j == i || !W3dShortcutSameScope(kShortcuts[j].ctx, kShortcuts[i].ctx)) continue;
        std::vector<W3dCombo>& v = gCurrent[(size_t)j];
        for (size_t k = 0; k < v.size(); k++) if (v[k] == c) { v.erase(v.begin() + (long)k); if (removedOf) *removedOf = j; break; }
    }
    std::vector<W3dCombo>& mine = gCurrent[(size_t)i];
    for (size_t k = 0; k < mine.size(); k++) if (mine[k] == c) { W3dShortcutsSave(); return true; }
    mine.push_back(c);
    W3dShortcutsSave();
    return true;
}
void W3dShortcutEmpty(int i) {
    Prepare();
    if (i < 0 || i >= kNShortcuts) return;
    gCurrent[(size_t)i].clear();
    W3dShortcutsSave();
}
void W3dShortcutReset(int i) {
    Prepare();
    if (i < 0 || i >= kNShortcuts) return;
    // las por defecto vuelven a su funcion: si otra del mismo contexto las habia tomado, se las saca
    const std::vector<W3dCombo>& d = gDefault[(size_t)i];
    for (int j = 0; j < kNShortcuts; j++) {
        if (j == i || !W3dShortcutSameScope(kShortcuts[j].ctx, kShortcuts[i].ctx)) continue;
        std::vector<W3dCombo>& v = gCurrent[(size_t)j];
        for (size_t k = 0; k < v.size();) {
            bool collides = false;
            for (size_t q = 0; q < d.size(); q++) if (v[k] == d[q]) collides = true;
            if (collides) v.erase(v.begin() + (long)k); else k++;
        }
    }
    gCurrent[(size_t)i] = d;
    W3dShortcutsSave();
}
void W3dShortcutsResetAll() {
    Prepare();
    gCurrent = gDefault;
    W3dShortcutsSave();
}
bool W3dShortcutsSet(int i, const char* list) {
    Prepare();
    if (i < 0 || i >= kNShortcuts) return false;
    std::vector<W3dCombo> v;
    const bool ok = ListFromText(list, v);
    gCurrent[(size_t)i] = v;
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------
//  Archivo: shortcuts.start en la carpeta del usuario (junto al config.start). Solo se escriben las funciones que el usuario cambio, asi una
//  version nueva del editor con otros atajos por defecto no queda pisada por un archivo viejo.
//      3d.move = G, Ctrl+M
//      3d.deleteValue =            (sin combinacion)
// ---------------------------------------------------------------------------------------------------------------
// la carpeta ESCRIBIBLE del usuario (res/ es de solo lectura en el AppImage, el .deb y el APK). Sin carpeta (el
// telefono no la define), la ruta relativa de siempre.
std::string W3dShortcutsPath() {
    if (!gPath.empty()) return gPath;
    const std::string& dir = w3dFileSystem::GetUserDataDir();
    return dir.empty() ? std::string("shortcuts.ini") : dir + "/shortcuts.ini";
}
void W3dShortcutsPathSet(const std::string& path) {
    Prepare();
    gPath = path;
    gCurrent = gDefault;
    gLoaded = true;   // el que fija la ruta decide cuando cargar (W3dShortcutsLoad)
}

bool W3dShortcutsLoad() {
    Prepare();
    gLoaded = true;
    gCurrent = gDefault;
    FILE* f  = fopen(W3dShortcutsPath().c_str(), "rb");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string l = Crop(line);
        if (l.empty() || l[0] == '#' || l[0] == ';') continue;
        const size_t ig = l.find('=');
        if (ig == std::string::npos) continue;
        const int i = W3dShortcutFind(Crop(l.substr(0, ig)).c_str());
        if (i < 0) continue;   // una funcion que ya no existe: se ignora
        ListFromText(l.substr(ig + 1).c_str(), gCurrent[(size_t)i]);
    }
    fclose(f);
    return true;
}

bool W3dShortcutsSave() {
    Prepare();
    FILE* f = fopen(W3dShortcutsPath().c_str(), "wb");
    if (!f) return false;
    fprintf(f, "# Whisk3D: atajos de teclado cambiados (los que no figuran usan el de fabrica).\n");
    fprintf(f, "# funcion = combinacion, otra combinacion   (vacio = sin atajo)\n");
    for (int i = 0; i < kNShortcuts; i++)
        if (!W3dShortcutIsDefault(i)) fprintf(f, "%s = %s\n", kShortcuts[i].id, W3dShortcutText(i).c_str());
    bool ok = (ferror(f) == 0);
    if (fclose(f) != 0) ok = false;
    extern void W3dWebPersist(); W3dWebPersist();   // web: IndexedDB (W3dConfigSave.cpp)
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------
//  El traductor
// ---------------------------------------------------------------------------------------------------------------
static bool Has(const std::vector<W3dCombo>& v, const W3dCombo& c) {
    for (size_t k = 0; k < v.size(); k++) if (v[k] == c) return true;
    return false;
}

bool W3dShortcutTranslate(const int* ctxs, int n, int& key, int& mods, bool repeat) {
    if (!gLoaded) W3dShortcutsLoad();
    if (key == W3dK_NOTHING || W3dIsKeyModifier(key) || n <= 0) return true;
    // en el editor de scripts lo que se tipea es TEXTO: solo se traducen las combinaciones con Ctrl/Alt y las F
    if (ctxs[0] == W3dATC_IDE && !(mods & (W3dMOD_CTRL | W3dMOD_ALT)) &&
        !(key == W3dK_F1 || key == W3dK_F2 || (key >= W3dK_F3 && key <= W3dK_F12))) return true;
    const W3dCombo p(key, mods);
    // contexto por contexto, del mas especifico al general: si ahi la combinacion es de una funcion, manda la
    // por defecto de esa funcion; si era la por defecto de alguna que ya no la tiene, queda libre (no hace nada)
    for (int c = 0; c < n; c++) {
        for (int i = 0; i < kNShortcuts; i++) {
            if (kShortcuts[i].ctx != ctxs[c] || !Has(gCurrent[(size_t)i], p)) continue;
            if (gDefault[(size_t)i].empty()) {
                // sin tecla de fabrica: ningun handler la escucha -> corre la accion del menu (y la tecla no sigue)
                if (repeat && kShortcuts[i].action) return false;   // mantenerla apretada no la repite
                if (W3dShortcutRun(i)) return false;
                return true;
            }
            key  = gDefault[(size_t)i][0].key;
            mods = gDefault[(size_t)i][0].mods;
            return true;
        }
        for (int i = 0; i < kNShortcuts; i++)
            if (kShortcuts[i].ctx == ctxs[c] && Has(gDefault[(size_t)i], p)) return false;
    }
    return true;
}

int W3dCurrentMods() {
    return (LShiftPressed ? W3dMOD_SHIFT : 0) | (LCtrlPressed ? W3dMOD_CTRL : 0) | (LAltPressed ? W3dMOD_ALT : 0);
}

W3dTempMods::W3dTempMods() : active(false), shift(false), ctrl(false), alt(false) {}
W3dTempMods::~W3dTempMods() {
    if (active) { LShiftPressed = shift; LCtrlPressed = ctrl; LAltPressed = alt; }
}
void W3dTempMods::Set(int m) {
    if (!active) { shift = LShiftPressed; ctrl = LCtrlPressed; alt = LAltPressed; active = true; }
    LShiftPressed = (m & W3dMOD_SHIFT) != 0;
    LCtrlPressed  = (m & W3dMOD_CTRL) != 0;
    LAltPressed   = (m & W3dMOD_ALT) != 0;
}

int W3dShortcutContexts(int* c) {
    int n = 0;
    ViewportBase* vp = viewPortActive;
    const int kind = (vp && vp->isLeaf()) ? vp->ViewportKind() : 0;
    bool xf = false;
    if (kind == 1)      xf = (state != editNavigation) || EditXformActive() || BoneXformMode() != 0 || BoneGrabActive() || g_poseMode != 0;
    else if (kind == 4 || kind == 6) xf = vp->XformInProgress();
    else if (kind == 5) xf = DopeXformActive();
    if (xf) c[n++] = W3dATC_XFORM;
    switch (kind) {
        case 1:
            // el MODO primero (lo suyo tapa a lo comun): Edicion de malla o de huesos, si no Objeto (y Pose)
            c[n++] = (InteractionMode == EditMode || BoneEditActive()) ? W3dATC_3D_EDIT : W3dATC_3D_OBJ;
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
bool W3dShortcutCapture(int key, int mods) {
    if (g_shortcutCapturing < 0) return false;
    if (key == W3dK_NOTHING || W3dIsKeyModifier(key)) return true;   // sigue esperando la tecla de verdad
    const int i = g_shortcutCapturing;
    g_shortcutCapturing = -1;
    if (key == W3dK_ESCAPE && mods == 0) { Notify(T("Cancelled"), false); return true; }
    const W3dCombo c(key, mods);
    int removed = -1;
    if (!W3dShortcutAdd(i, c, &removed)) return true;
    std::string msg = W3dComboText(c) + " -> " + T(kShortcuts[i].name);
    if (removed >= 0) msg += std::string("  (") + T("removed from") + " " + T(kShortcuts[removed].name) + ")";
    Notify(msg, false);
    return true;
}
