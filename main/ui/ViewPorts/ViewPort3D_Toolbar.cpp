// ===================================================================================================
//  BARRA DE HERRAMIENTAS tactil del viewport 3D (abajo). Metodos Viewport3D::Toolbar* + helpers.
//  Extraido de ViewPort3D.cpp (Fase 2 del reorg). Los metodos ya estan declarados en Viewport3D (ViewPort3D.h).
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
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
#include "objects/Mesh.h"          // overlay de estadisticas (vertsAgrupados, faces3d)
#include "objects/EditMesh.h"      // foco al centro de la seleccion en edit mode
#include "objects/Armature.h"      // dibujar huesos del esqueleto encima de todo
#include "animation/SkeletalAnimation.h" // EvaluarPoseEsqueleto (pose al reproducir)
#include "animation/Animation.h"          // CurrentFrame
#include "ViewPorts/LayoutInput.h" // LayoutDeleteEdit (menu Delete en edit mode)
#include "ViewPorts/UVEditor.h"    // roles TBR_Pincel*/TBR_Grupo (pincel de Weight Paint, compartidos con el UV)
#include "edit/WeightPaint.h"      // estado del pincel + menus deslizables + labels de la toolbar
#include "edit/BoneEdit.h"         // Edit Mode de ARMATURE: toolbar contextual Extrude/Move/Delete (Fase 3)
#include "ViewPorts/PopUp/NumPad.h" // NumPadAbrirTransform (teclado tactil sobre la barra de estado)
#include "ViewPorts/TransformUI.h"  // compartidos: ToolbarUsaTactil + colores Tb* (los usan los 3 editores)
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup al borrar con la tecla)
#include "ViewPorts/PopUp/ProgressPopup.h"  // barra "Rendering..." durante el render por tiles (clave en N95)
#include "W3dProfile.h" // profiler del frame (ms por categoria) para el overlay Statistics

void RebindMaterialMeshPart(); // (def en Properties.cpp) refresca el panel de material tras undo/redo

// ============================================================================
//  BARRA DE HERRAMIENTAS (abajo del viewport 3D). Solo si cfg.nuevoUsuario
//  (el experimentado usa atajos; en Symbian default off, un N8 tactil la prende).
//  [tilde verde / cruz roja: aceptar-cancelar el transform, SOLO tactil]
//  [orientacion: Global/Local/View/Normal] [X][Y][Z: constrenir ejes, combinables]
//  [historial de acciones MRU (max 8, sin repetir, separado por modo)]
//  El MECANISMO (layout/scroll/hit tolerante/render/MRU) es el COMPARTIDO de
//  ViewportBase (ToolbarBase.cpp); aca queda SOLO lo contextual del 3D:
//  ToolbarSincronizar (visibilidad/colores) + ToolbarAccionRol (el dispatch).
// ============================================================================
extern bool g_redraw;
// (ToolbarUsaTactil vive en el compartido TransformUI.cpp: tambien la usan el UV y el 2D)

// historial MRU por modo. Arranca con Move/Rotate/Scale (defaults); usar una accion la sube adelante.
static std::vector<int> gToolHistObj;
static std::vector<int> gToolHistEdit;
static std::vector<int> gToolHistBone; // Edit de ARMATURE: Extrude / Move / Delete (contextual, Fase 3)
static std::vector<int>& ToolbarHist(){
    // EDIT MODE de ARMATURE: la barra ofrece las acciones de HUESOS (extruir/mover/rotar/escalar/borrar)
    if (BoneEditActivo()){
        std::vector<int>& hb = gToolHistBone;
        if (hb.empty()){ hb.push_back(TBExtrude); hb.push_back(TBMove); hb.push_back(TBRotate); hb.push_back(TBScale); hb.push_back(TBDelete); }
        return hb;
    }
    std::vector<int>& h = (InteractionMode == EditMode) ? gToolHistEdit : gToolHistObj;
    if (h.empty()){
        h.push_back(TBMove); h.push_back(TBRotate); h.push_back(TBScale);
        if (InteractionMode == EditMode){
            // edit: las mas usadas a mano por defecto -> Extrude, Loop Cut, Delete
            h.push_back(TBExtrude); h.push_back(TBLoopCut); h.push_back(TBDelete);
        } else {
            h.push_back(TBDelete); // objeto: Delete a mano (ultima opcion al arrancar)
        }
    }
    return h;
}
void ToolbarRegistrarAccion(int id){
    ToolbarMRU(ToolbarHist(), id); // MRU compartido (ToolbarBase.cpp): al frente, sin repetir, max 8
}
// (el texto de cada accion sale del compartido ToolbarAccionLabel, ToolbarBase.cpp)
// icono de la accion (o -1 = solo texto). Con icono el boton va sin texto -> la barra queda
// mas chica (iconos en vez de textos gigantes). Por ahora solo Delete tiene arte.
static int ToolbarIcono(int id){
    if (id == TBDelete) return (int)IconType::borrar;
    return -1;
}
static void ToolbarEjecutar(int id){
    // EDIT MODE de ARMATURE: las acciones operan sobre los HUESOS (mismos starters que E / G / R / S / X)
    if (BoneEditActivo()){
        Armature* a = BoneEditArm();
        if (id == TBExtrude)      BoneEditExtruirInteractivo(a, true);
        else if (id == TBMove)  { if (!BoneGrabActivo()) BoneXformStart(a, 1); }
        else if (id == TBRotate){ if (!BoneGrabActivo()) BoneXformStart(a, 2); }
        else if (id == TBScale) { if (!BoneGrabActivo()) BoneXformStart(a, 3); }
        else if (id == TBDelete)  BoneEditBorrar(a);
        return;
    }
    switch (id){ // mismos starters que las teclas G/R/S/E (edit mode primero; sino objeto)
        case TBMove:    if (!EditXformStart(translacion, ViewAxis)) SetPosicion(); break;
        case TBRotate:  if (!EditXformStart(rotacion,    ViewAxis)) SetRotacion(); break;
        case TBScale:   if (!EditXformStart(EditScale,   XYZ))      SetEscala();   break;
        case TBExtrude: LayoutExtrudeFaces(); break;
        case TBLoopCut: LayoutLoopCutDesdeActivo(); break; // sobre el borde/quad ACTIVO (quad -> elegir direccion)
        case TBDelete:
            if (InteractionMode == EditMode) LayoutDeleteEdit(lastMouseX, lastMouseY); // menu Delete (verts/edges/faces/loops)
            else                             AbrirConfirmarBorrado();                  // objeto: confirmar y borrar
            break;
    }
}

// ejes como mascara de bits (x=1,y=2,z=4) <-> axisSelect. Dos ejes prendidos = el PLANO que
// los contiene (excluye el tercero); ninguno (o los 3) = libre.
static int ToolbarEjesMask(){
    switch (axisSelect){
        case X: return 1;      case Y: return 2;      case Z: return 4;
        case PlaneZ: return 3; case PlaneY: return 5; case PlaneX: return 6;
    }
    return 0; // XYZ / ViewAxis / OrbitalAxis = libre
}
static void ToolbarToggleEje(int bit){
    int m = ToolbarEjesMask() ^ bit;
    if (gEVuseCustom){ gEVuseCustom = false; transformOrientation = GlobalOrient; } // extrude/Normal -> eje comun
    switch (m){
        case 1: axisSelect = X; break;      case 2: axisSelect = Y; break;      case 4: axisSelect = Z; break;
        case 3: axisSelect = PlaneZ; break; case 5: axisSelect = PlaneY; break; case 6: axisSelect = PlaneX; break;
        default: axisSelect = (estado == EditScale) ? XYZ : ViewAxis; break; // libre
    }
    if (BoneXformModo()) BoneXformReaplicar();                    // G/R/S de huesos: re-aplica con el eje nuevo
    else if (estado != editNavegacion) ReestablecerEstado(false); // re-aplica el transform con el eje nuevo
    g_redraw = true;
}

static bool ToolbarTransformando(){
    extern int g_poseModo;
    return (estado == translacion || estado == rotacion || estado == EditScale) &&
           (InteractionMode == ObjectMode || (InteractionMode == EditMode && EditXformActivo()) ||
            (InteractionMode == EditMode && BoneXformModo()) || // G/R/S de huesos: idem (ejes + tilde/cruz)
            (InteractionMode == PoseMode && g_poseModo)); // POSE: tambien muestra tilde/cruz + ejes en el tactil
}

bool Viewport3D::ToolbarVisible() const {
    // MODO JUEGO: sin toolbar (RenderToolbar tampoco la dibuja). Sin esto la barra
    // INVISIBLE seguia consumiendo los clicks (ToolbarClick "consumia igual") y le
    // restaba su franja al lienzo del juego.
    { extern bool SimActiva(); if (AnimEsJuego && SimActiva()) return false; }
#ifdef W3D_SYMBIAN
    return cfg.nuevoUsuario; // el N95 va a teclas; un N8 tactil la prende por config
#else
    return true; // PC/Android/Web: SIEMPRE (Undo/Redo tienen que estar; el experimentado ve solo esos)
#endif
}
// (ToolbarHeight / OnToolbar / ToolbarScrollBy: compartidos en ViewportBase, ToolbarBase.cpp)

// colores de los ejes (X/Y/Z), rojos del cancelar y verde del aceptar: COMPARTIDOS en
// TransformUI.cpp (TbEjeColor / TbEjeBg / TbRojo / TbRojoBg / TbVerdeBg); los usan los 3 editores.

// visibilidad CONTEXTUAL + colores (el layout lo hace el compartido ToolbarActualizar).
// Sin transform: SOLO el historial. Durante un transform: orientacion + ejes (View: sin Y,
// que es la profundidad de la vista) + tilde/cruz SIEMPRE (mouse incluido). CAMBIO DE
// COMPORTAMIENTO (pedido del dueno): antes tilde/cruz solo aparecian en sesion tactil;
// ahora se muestran en cualquier transform, en el 3D, el UV y el 2D (paridad y
// descubribilidad; los botones de eje y el resto siguen su regla de siempre).
// Mismos Button de arriba.
void Viewport3D::ToolbarSincronizar(){
    bool transformando = (Viewport3DActive == this) && ToolbarTransformando();
    bool tactil = ToolbarUsaTactil();
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
    int mask = transformando ? ToolbarEjesMask() : 0;
    std::vector<int>& h = ToolbarHist();
    // modo WEIGHT PAINT: la toolbar muestra los CONTROLES DEL PINCEL (tam/fuerza/modo/grupo)
    // en vez del historial de acciones. Labels con el estado actual del pincel.
    const bool pincel = (InteractionMode == WeightPaint);
    std::string lTam, lFuerza, lModo, lGrupo;
    if (pincel){
        Mesh* wpm = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        WeightPaintLabels(wpm, lTam, lFuerza, lModo, lGrupo);
    }

    // visibilidad + contenido + colores
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        int rol = btn->rol;
        if (rol == TBR_Undo || rol == TBR_Redo){
            // Siempre visibles, MENOS durante una edicion en curso (transform): ahi la barra es
            // aceptar/cancelar/ejes y undo/redo estorban. Se atenuan (gris) si no hay nada que des/rehacer.
            btn->visible = !transformando;
            bool hay = (rol == TBR_Undo) ? UndoHayAlgo() : UndoHayRedo();
            btn->tinte = NULL; btn->colorTexto = hay ? blanco : grisUI;
        } else if (rol == TBR_Aceptar){
            btn->visible = transformando; // tilde SIEMPRE durante el transform (antes solo tactil)
            btn->tinte = TbVerdeBg(); btn->colorTexto = accent;
        } else if (rol == TBR_Repeat){
            // solo durante un EXTRUDE (tactil): acepta y vuelve a extruir. Verde como el OK.
            btn->visible = transformando && tactil && ExtrudeEnCurso();
            btn->tinte = TbVerdeBg(); btn->colorTexto = accent;
        } else if (rol == TBR_Cancelar){
            btn->visible = transformando; // cruz SIEMPRE durante el transform (antes solo tactil)
            btn->tinte = TbRojoBg(); btn->colorTexto = TbRojo();
        } else if (rol == TBR_Orient){
            btn->visible = transformando;
            btn->text = (transformOrientation == LocalOrient)  ? "Local"  :
                        (transformOrientation == ViewOrient)   ? "View"   :
                        (transformOrientation == NormalOrient) ? "Normal" : "Global";
        } else if (rol >= TBR_EjeX && rol <= TBR_EjeZ){
            int e = rol - TBR_EjeX;
            // en orientacion VIEW no hay eje Y (es la profundidad de la vista): solo X y Z
            btn->visible = transformando && !(transformOrientation == ViewOrient && e == 1);
            bool on = (mask & (1 << e)) != 0;
            btn->tinte = on ? TbEjeBg(e) : NULL;     // encendido: fondo de SU color
            btn->colorTexto = on ? blanco : TbEjeColor(e); // apagado: la letra en su color
        } else if (rol == TBR_Shift || rol == TBR_Ctrl){
            // modificadores tactiles: visibles con pantalla tactil, MENOS durante una edicion en curso
            // (transform/extrude/strip) -> ahi estorban. Encendidos (verde) = LShift/LCtrlPressed.
            btn->visible = tactil && !transformando;
            bool on = (rol == TBR_Shift) ? LShiftPressed : LCtrlPressed;
            btn->tinte = on ? TbVerdeBg() : NULL;       // ON: fondo verde accent
            btn->colorTexto = on ? accent : blanco;    // ON: texto verde; OFF: blanco
        } else if (rol == TBR_View){
            // toggle VIEW: solo en Edit Mode y con pantalla tactil (en PC el mouse orbita distinto). Verde = ON.
            btn->visible = tactil && (InteractionMode == EditMode);
            btn->tinte = g_viewEditMode ? TbVerdeBg() : NULL;
            btn->colorTexto = g_viewEditMode ? accent : blanco;
        } else if (rol >= TBR_PincelTam && rol <= TBR_Grupo){
            // PINCEL (Weight Paint): tam / fuerza / sumar-restar / grupo activo. El modo restar
            // se marca en verde (esta "activado" respecto del sumar por defecto).
            btn->visible = pincel && !transformando;
            btn->tinte = NULL; btn->colorTexto = NULL;
            if (rol == TBR_PincelTam)         btn->text = lTam;
            else if (rol == TBR_PincelFuerza) btn->text = lFuerza;
            else if (rol == TBR_PincelModo){
                btn->text = lModo;
                bool restar = (BrushGet().modo != 0);
                btn->tinte = restar ? TbRojoBg() : NULL;    // restar: fondo rojizo (como Cancelar)
                btn->colorTexto = restar ? TbRojo() : NULL;
            }
            else btn->text = lGrupo;
        } else if (rol == TBR_SoloSel){
            // "editar solo lo seleccionado" (mascara de pintura): solo en Weight Paint.
            // Toggle GLOBAL compartido con el UV editor; tinte accent cuando esta ON.
            btn->visible = pincel && !transformando;
            bool on = WeightPaintSoloSel();
            btn->tinte = on ? TbVerdeBg() : NULL;
            btn->colorTexto = on ? accent : blanco;
        } else { // historial de acciones (solo FUERA de un transform y fuera del modo pincel)
            int hi = rol - TBR_Hist;
            btn->visible = !transformando && !pincel && hi >= 0 && hi < (int)h.size();
            if (btn->visible){
                int ic = ToolbarIcono(h[hi]);
                btn->icon = ic;                                   // icono (o -1)
                btn->text = (ic >= 0) ? "" : ToolbarAccionLabel(h[hi]); // con icono, sin texto (barra mas chica)
            }
        }
        // usuario EXPERIMENTADO (cfg.nuevoUsuario=false): barra MINIMA con SOLO Undo/Redo (que
        // "siempre tienen que estar"); el resto se oculta. EXCEPTO durante un transform (controles
        // tilde/cruz/ejes) y EXCEPTO los controles del pincel en Weight Paint (sin ellos no se
        // puede ajustar el pincel: no hay atajos de teclado todavia).
        if (!cfg.nuevoUsuario && !transformando && rol != TBR_Undo && rol != TBR_Redo &&
            !(pincel && rol >= TBR_PincelTam && rol <= TBR_SoloSel)) btn->visible = false;
    }
}

// accion de un boton de la toolbar (la busqueda tolerante + el consumo los hace el
// ToolbarClick COMPARTIDO de ViewportBase; aca llega solo el ROL del boton pulsado).
void Viewport3D::ToolbarAccionRol(int rol){
    if (rol == TBR_Undo){ UndoDeshacer(); RebindMaterialMeshPart(); }      // flecha izquierda: deshacer
    else if (rol == TBR_Redo){ UndoRehacer(); RebindMaterialMeshPart(); }  // flecha derecha: rehacer
    else if (rol == TBR_Aceptar) Aceptar();               // tilde verde: confirma el transform
    else if (rol == TBR_Repeat){                          // "Repeat": acepta el extrude y vuelve a extruir
        Aceptar();              // confirma el extrude actual (deja la tapa seleccionada)
        LayoutExtrudeFaces();   // y extruye de nuevo esa seleccion + arranca el move
    }
    else if (rol == TBR_Cancelar){                        // cruz roja: cancela (mismo camino que el click derecho)
        if (BoneGrabActivo()) BoneGrabCancelar();         // transform de huesos: restaura las puntas
        else if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
        else Cancelar();
        NumInputReset();
    }
    else if (rol == TBR_Orient){
        Button* b = BarRolBtn(ToolButtons, TBR_Orient);
        LayoutMenuOrientToolbar(b ? b->sx : x, y + height - ToolbarHeight());
    }
    else if (rol >= TBR_EjeX && rol <= TBR_EjeZ) ToolbarToggleEje(1 << (rol - TBR_EjeX)); // combinables
    else if (rol >= TBR_PincelTam && rol <= TBR_Grupo){
        // PINCEL (Weight Paint): +- togglea; tam/fuerza abren su menu deslizable; grupo el dropdown
        Button* b = BarRolBtn(ToolButtons, rol);
        int bx = b ? b->sx : x, byTop = y + height - ToolbarHeight();
        if (rol == TBR_PincelModo) BrushGet().modo = BrushGet().modo ? 0 : 1;
        else if (rol == TBR_PincelTam)    WeightPaintMenuTam(bx, byTop);
        else if (rol == TBR_PincelFuerza) WeightPaintMenuFuerza(bx, byTop);
        else { // TBR_Grupo: vertex group activo del mesh activo
            Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
            if (m) WeightPaintMenuGrupo(m, bx, byTop);
        }
    }
    else if (rol == TBR_SoloSel){ // "editar solo lo seleccionado": toggle global (compartido con el UV)
        WeightPaintSoloSel() = !WeightPaintSoloSel(); g_redraw = true;
    }
    else if (rol == TBR_Shift) LShiftPressed = !LShiftPressed; // modificador tactil: queda encendido (verde)
    else if (rol == TBR_Ctrl)  LCtrlPressed  = !LCtrlPressed;
    else if (rol == TBR_View)  g_viewEditMode = !g_viewEditMode; // toggle: orbitar/panear con el dedo durante una operacion
    else if (rol >= TBR_Hist && rol < TBR_Hist + 8){
        std::vector<int>& h = ToolbarHist();
        int hi = rol - TBR_Hist;
        if (hi < (int)h.size()) ToolbarEjecutar(h[hi]); // historial: arranca la accion
    }
}

void Viewport3D::RenderToolbar(){
    // MODO JUEGO: sin barra de herramientas inferior (undo/redo/shift/historial
    // son de EDICION; el juego se mira limpio). Se ocultan para que el hit-test
    // tampoco los agarre.
    { extern bool SimActiva();
      if (AnimEsJuego && SimActiva()) {
          for (size_t i = 0; i < ToolButtons.size(); i++) ToolButtons[i]->visible = false;
          return;
      } }
    ViewportBase::RenderToolbar(); // el dibujo compartido (fondo + botones, ToolbarBase.cpp)
}

// la barra de menu (arriba) O la de herramientas (abajo): el gesto de arrastre queda lockeado
// a la que se toco (toolGesto) para que BarScrollBy scrollee la correcta.
