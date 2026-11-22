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
#include "ui/W3dColors.h" // W3dColors: colores del editor (piso, ejes de transformacion)
#include "render/RenderOptions.h" // flags del overlay de normales
#include "objects/Mesh.h"         // overlay de estadisticas (groupedVerts, faces3d)
#include "objects/EditMesh.h"     // foco al centro de la seleccion en edit mode
#include "objects/Armature.h"     // dibujar huesos del esqueleto encima de todo
#include "animation/SkeletalAnimation.h" // EvaluatePoseSkeleton (pose al reproducir)
#include "animation/Animation.h"          // CurrentFrame
#include "ViewPorts/LayoutInput.h" // LayoutDeleteEdit (menu Delete en edit mode)
#include "ViewPorts/Pick3D.h"   // insertar caras / bevel: tilde, cruz y sus opciones en la barra
#include "edit/MeshEdit.h"      // InsetOptions / BevelOptions (las opciones prendidas)
#include "ViewPorts/UVEditor.h"    // roles TBR_Pincel*/TBR_Group (pincel de Weight Paint, compartidos con el UV)
#include "edit/WeightPaint.h"      // estado del pincel + menus deslizables + labels de la toolbar
#include "edit/Proportional.h"           // proportional editing: boton de la toolbar + barra de influencia
#include "ViewPorts/PopUp/FalloffEditor.h" // el editor de falloff (popup reutilizable)
#include "ViewPorts/PopUp/ColorPicker.h"   // el color del pincel de vertex paint
#include "WhiskUI/draw/icons.h"    // IconIndex: el icono del falloff activo
#include "edit/BoneEdit.h"         // Edit Mode de ARMATURE: toolbar contextual Extrude/Move/Delete (Fase 3)
#include "ViewPorts/PopUp/NumPad.h" // NumPadOpenTransform (teclado tactil sobre la barra de estado)
#include "ViewPorts/TransformUI.h"  // compartidos: ToolbarUsesTouch + colores Tb* (los usan los 3 editores)
#include "ViewPorts/PopUp/ConfirmPopup.h"  // OpenConfirmDelete (popup al borrar con la tecla)
#include "ViewPorts/PopUp/ProgressPopup.h" // barra "Rendering..." durante el render por tiles (clave en N95)
#include "W3dProfile.h" // profiler del frame (ms por categoria) para el overlay Statistics

void RebindMaterialMeshPart(); // (def en Properties.cpp) refresca el panel de material tras undo/redo

// ============================================================================
//  BARRA DE HERRAMIENTAS (abajo del viewport 3D). Solo si cfg.newUser
//  (el experimentado usa atajos; en Symbian default off, un N8 tactil la prende).
//  [tilde verde / cruz roja: aceptar-cancelar el transform, SOLO tactil]
//  [orientacion: Global/Local/View/Normal] [X][Y][Z: constrenir ejes, combinables]
//  [historial de acciones MRU (max 8, sin repetir, separado por modo)]
//  El MECANISMO (layout/scroll/hit tolerante/render/MRU) es el COMPARTIDO de
//  ViewportBase (ToolbarBase.cpp); aca queda SOLO lo contextual del 3D:
//  ToolbarSync (visibilidad/colores) + ToolbarActionRole (el dispatch).
// ============================================================================
extern bool g_redraw;
// (ToolbarUsesTouch vive en el compartido TransformUI.cpp: tambien la usan el UV y el 2D)

// historial MRU por modo. Arranca con Move/Rotate/Scale (defaults); usar una accion la sube adelante.
static std::vector<int> gToolHistObj;
static std::vector<int> gToolHistEdit;
static std::vector<int> gToolHistBone; // Edit de ARMATURE: Extrude / Move / Delete (contextual, Fase 3)
static std::vector<int>& ToolbarHist(){
    // EDIT MODE de ARMATURE: la barra ofrece las acciones de HUESOS (extruir/mover/rotar/escalar/borrar)
    if (BoneEditActive()){
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
void ToolbarRegisterAction(int id){
    ToolbarMRU(ToolbarHist(), id); // MRU compartido (ToolbarBase.cpp): al frente, sin repetir, max 8
}
// (el texto de cada accion sale del compartido ToolbarActionLabel, ToolbarBase.cpp)
// icono de la accion (o -1 = solo texto). Con icono el boton va sin texto -> la barra queda
// mas chica (iconos en vez de textos gigantes). Por ahora solo Delete tiene arte.
static int ToolbarIcon(int id){
    if (id == TBDelete) return (int)IconType::trash;
    return -1;
}
static void ToolbarRun(int id){
    // EDIT MODE de ARMATURE: las acciones operan sobre los HUESOS (mismos starters que E / G / R / S / X)
    if (BoneEditActive()){
        Armature* a = BoneEditArm();
        if (id == TBExtrude)      BoneEditExtrudeInteractive(a, true);
        else if (id == TBMove)  { if (!BoneGrabActive()) BoneXformStart(a, 1); }
        else if (id == TBRotate){ if (!BoneGrabActive()) BoneXformStart(a, 2); }
        else if (id == TBScale) { if (!BoneGrabActive()) BoneXformStart(a, 3); }
        else if (id == TBDelete)  BoneEditDelete(a);
        return;
    }
    switch (id){ // mismos starters que las teclas G/R/S/E (edit mode primero; sino objeto)
        case TBMove:    if (!EditXformStart(translation, ViewAxis)) BeginTranslate(); break;
        case TBRotate:  if (!EditXformStart(rotation,    ViewAxis)) BeginRotate(); break;
        case TBScale:   if (!EditXformStart(EditScale,   XYZ))      BeginScale();   break;
        case TBExtrude: LayoutExtrudeFaces(); break;
        case TBLoopCut: LayoutLoopCutFromActive(); break; // sobre el borde/quad ACTIVO (quad -> elegir direccion)
        case TBDelete:
            if (InteractionMode == EditMode) LayoutDeleteEdit(lastMouseX, lastMouseY); // menu Delete (verts/edges/faces/loops)
            else                             OpenConfirmDelete();                      // objeto: confirmar y borrar
            break;
    }
}

// ejes como mascara de bits (x=1,y=2,z=4) <-> axisSelect. Dos ejes prendidos = el PLANO que
// los contiene (excluye el tercero); ninguno (o los 3) = libre.
static int ToolbarAxesMask(){
    switch (axisSelect){
        case X: return 1;      case Y: return 2;      case Z: return 4;
        case PlaneZ: return 3; case PlaneY: return 5; case PlaneX: return 6;
    }
    return 0; // XYZ / ViewAxis / OrbitalAxis = libre
}
static void ToolbarToggleAxis(int bit){
    int m = ToolbarAxesMask() ^ bit;
    if (gEVuseCustom){ gEVuseCustom = false; transformOrientation = GlobalOrient; } // extrude/Normal -> eje comun
    switch (m){
        case 1: axisSelect = X; break;      case 2: axisSelect = Y; break;      case 4: axisSelect = Z; break;
        case 3: axisSelect = PlaneZ; break; case 5: axisSelect = PlaneY; break; case 6: axisSelect = PlaneX; break;
        default: axisSelect = (state == EditScale) ? XYZ : ViewAxis; break; // libre
    }
    if (BoneXformMode()) BoneXformReapply();               // G/R/S de huesos: re-aplica con el eje nuevo
    else if (state != editNavigation) RestoreState(false); // re-aplica el transform con el eje nuevo
    g_redraw = true;
}

static bool ToolbarTransforming(){
    extern int g_poseMode;
    if (InsetActive() || BevelActive()) return true;   // insertar caras / bevel: tilde, cruz y sus opciones
    return (state == translation || state == rotation || state == EditScale) &&
           (InteractionMode == ObjectMode || (InteractionMode == EditMode && EditXformActive()) ||
            (InteractionMode == EditMode && BoneXformMode()) || // G/R/S de huesos: idem (ejes + tilde/cruz)
            (InteractionMode == PoseMode && g_poseMode)); // POSE: tambien muestra tilde/cruz + ejes en el tactil
}

// las capturas de los tutoriales (main/test/W3dCaptures.cpp, 'vista3d barra 0'): la Vista 3D como en el N95, sin la
// barra de abajo. Solo la toca el harness.
bool g_captureNoBar3D = false;

bool Viewport3D::ToolbarVisible() const {
    if (g_captureNoBar3D) return false;
    // MODO JUEGO: sin toolbar (RenderToolbar tampoco la dibuja). Sin esto la barra
    // INVISIBLE seguia consumiendo los clicks (ToolbarClick "consumia igual") y le
    // restaba su franja al lienzo del juego.
    { extern bool SimActive(); if (AnimIsGame && SimActive()) return false; }
    // el panel "ajustar la ultima operacion" ocupa su lugar mientras esta abierto
    { extern bool TransformRedoOpen(const Viewport3D*); if (TransformRedoOpen(this)) return false; }
#ifdef W3D_SYMBIAN
    return cfg.newUser; // el N95 va a teclas; un N8 tactil la prende por config
#else
    return true; // PC/Android/Web: SIEMPRE (Undo/Redo tienen que estar; el experimentado ve solo esos)
#endif
}
// FILA DE BARRAS del pincel (radio | valor, arriba de la toolbar): solo pintando pesos. A
// diferencia de la toolbar NO depende de cfg.newUser: sin estas dos barras no hay forma de
// cambiar el radio ni el valor (misma excepcion que ya tenian los controles del pincel).
bool Viewport3D::BrushBarVisible() const {
    { extern bool SimActive(); if (AnimIsGame && SimActive()) return false; } // modo juego: pantalla limpia
    return InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint;
}
bool Viewport3D::PropBarVisible() const {
    { extern bool SimActive(); if (AnimIsGame && SimActive()) return false; } // modo juego: pantalla limpia
    // SOLO mientras se mueve/rota/escala (al confirmar o cancelar se va): fuera del transform el radio se ve en el
    // menu de la curva y prender/apagar es el boton de la toolbar o la tecla O
    return g_prop.on && (InteractionMode == EditMode || InteractionMode == ObjectMode) &&
           Viewport3DActive == this && ToolbarTransforming();
}

// (ToolbarHeight / OnToolbar / ToolbarScrollBy: compartidos en ViewportBase, ToolbarBase.cpp)

// colores de los ejes (X/Y/Z), rojos del cancelar y verde del aceptar: COMPARTIDOS en
// TransformUI.cpp (TbAxisColor / TbAxisBg / TbRed / TbRedBg / TbGreenBg); los usan los 3 editores.

// visibilidad CONTEXTUAL + colores (el layout lo hace el compartido ToolbarUpdate).
// Sin transform: SOLO el historial. Durante un transform: orientacion + ejes (View: sin Y,
// que es la profundidad de la vista) + tilde/cruz SIEMPRE (mouse incluido). CAMBIO DE
// COMPORTAMIENTO (pedido del dueno): antes tilde/cruz solo aparecian en sesion tactil;
// ahora se muestran en cualquier transform, en el 3D, el UV y el 2D (paridad y
// descubribilidad; los botones de eje y el resto siguen su regla de siempre).
// Mismos Button de arriba.
void Viewport3D::ToolbarSync(){
    bool transforming = (Viewport3DActive == this) && ToolbarTransforming();
    bool touch = ToolbarUsesTouch();
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    const float* white  = ColorList[static_cast<int>(ColorID::white)];
    const float* grayUI = ColorList[static_cast<int>(ColorID::grayUI)];
    const bool inset = InsetActive(), bevel = BevelActive();   // (sin orientacion ni ejes: sus opciones)
    int mask = (transforming && !inset && !bevel) ? ToolbarAxesMask() : 0;
    std::vector<int>& h = ToolbarHist();
    // modo WEIGHT PAINT: la toolbar muestra los CONTROLES DEL PINCEL (tam/fuerza/modo/grupo)
    // en vez del historial de acciones. Labels con el estado actual del pincel.
    const bool brush = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
    const bool color = (InteractionMode == VertexPaint);    // pintando COLOR, no pesos
    const bool texture = (InteractionMode == TexturePaint); // pintando la TEXTURA: color + curva, sin grupos ni marcas
    std::string lSize, lForce, lMode, lGroup;
    Mesh* wpm = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (brush){
        WeightPaintLabels(wpm, lSize, lForce, lMode, lGroup);
        if (color){
            // en vertex paint el desplegable elige la CAPA DE COLOR (la unidad que se pinta),
            // no el vertex group
            lGroup = T("Color");
            if (wpm && wpm->activeColor >= 0 && wpm->activeColor < (int)wpm->colorLayers.size())
                lGroup = wpm->colorLayers[wpm->activeColor]->name;
        }
    }

    // visibilidad + contenido + colores
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        int role = btn->role;
        if (role == TBR_Undo || role == TBR_Redo){
            // Siempre visibles, MENOS durante una edicion en curso (transform): ahi la barra es
            // aceptar/cancelar/ejes y undo/redo estorban. Se atenuan (gris) si no hay nada que des/rehacer.
            btn->visible = !transforming;
            bool has = (role == TBR_Undo) ? UndoHasSomething() : UndoHasRedo();
            btn->tint = NULL; btn->textColor = has ? white : grayUI;
        } else if (role == TBR_Accept){
            btn->visible = transforming; // tilde SIEMPRE durante el transform (antes solo tactil)
            btn->tint = TbGreenBg(); btn->textColor = accent;
        } else if (role == TBR_Repeat){
            // solo durante un EXTRUDE (tactil): acepta y vuelve a extruir. Verde como el OK.
            btn->visible = transforming && touch && ExtrudeInProgress();
            btn->tint = TbGreenBg(); btn->textColor = accent;
        } else if (role == TBR_Cancel){
            btn->visible = transforming; // cruz SIEMPRE durante el transform (antes solo tactil)
            btn->tint = TbRedBg(); btn->textColor = TbRed();
        } else if (role >= TBR_InsetIndividual && role <= TBR_InsetDepth){
            const InsetOptions& o = InsetCurrentOptions();
            const bool on = role == TBR_InsetIndividual ? o.individual : role == TBR_InsetBoundary ? o.border :
                            role == TBR_InsetOutset ? o.outside : InsetDepthDrag();
            btn->visible = inset;
            btn->tint = on ? TbGreenBg() : NULL; btn->textColor = on ? accent : white;
        } else if (role >= TBR_BevelSegLess && role <= TBR_BevelVertices){
            const bool on = role == TBR_BevelVertices && BevelCurrentOptions().affects == 0;
            btn->visible = bevel;
            btn->tint = on ? TbGreenBg() : NULL; btn->textColor = on ? accent : white;
        } else if (role == TBR_Orient){
            btn->visible = transforming && !inset && !bevel;
            btn->text = T((transformOrientation == LocalOrient)  ? "Local"  :
                          (transformOrientation == ViewOrient)   ? "View"   :
                          (transformOrientation == NormalOrient) ? "Normal" : "Global");
        } else if (role >= TBR_AxisX && role <= TBR_AxisZ){
            int e = role - TBR_AxisX;
            // en orientacion VIEW no hay eje Y (es la profundidad de la vista): solo X y Z
            btn->visible = transforming && !inset && !bevel && !(transformOrientation == ViewOrient && e == 1);
            bool on = (mask & (1 << e)) != 0;
            btn->tint = on ? TbAxisBg(e) : NULL;     // encendido: fondo de SU color
            btn->textColor = on ? white : TbAxisColor(e); // apagado: la letra en su color
        } else if (role == TBR_Shift || role == TBR_Ctrl){
            // modificadores tactiles: visibles con pantalla tactil, MENOS durante una edicion en curso
            // (transform/extrude/strip) -> ahi estorban. Encendidos (verde) = LShift/LCtrlPressed.
            // ...y tampoco en PINTURA (pesos / vertex color): ahi Shift/Ctrl no sirven; en su lugar va el toggle "vista"
            const bool paintTb = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
            btn->visible = touch && !transforming && !paintTb;
            bool on = (role == TBR_Shift) ? LShiftPressed : LCtrlPressed;
            btn->tint = on ? TbGreenBg() : NULL;       // ON: fondo verde accent
            btn->textColor = on ? accent : white;    // ON: texto verde; OFF: blanco
        } else if (role == TBR_View){
            // toggle VIEW: solo en Edit Mode y con pantalla tactil (en PC el mouse orbita distinto). Verde = ON.
            // ...y en PINTURA tactil: ON = el dedo navega la camara; OFF = el dedo pinta (con la espera anti-gesto)
            btn->visible = touch && (InteractionMode == EditMode || InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
            btn->tint = g_viewEditMode ? TbGreenBg() : NULL;
            btn->textColor = g_viewEditMode ? accent : white;
        } else if (role == TBR_Snap){
            // el IMAN: toggle de un click (Object / Edit Mode, tambien con un transform en curso: prenderlo a mitad de un
            // movimiento lo engancha). Verde = prendido. En el telefono se llega con el softkey derecho (foco de la barra)
            btn->visible = !brush && (InteractionMode == EditMode || InteractionMode == ObjectMode);
            btn->tint = g_snap.enabled ? TbGreenBg() : NULL;
            btn->textColor = g_snap.enabled ? accent : white;
        } else if (role == TBR_Proportional){
            // PROPORTIONAL EDITING: toggle de un click (Edit/Object Mode, fuera de un transform y del pincel)
            btn->visible = !transforming && !brush && (InteractionMode == EditMode || InteractionMode == ObjectMode);
            btn->icon = ProportionalIconType(g_prop.type);
            btn->tint = g_prop.on ? TbGreenBg() : NULL;
            btn->textColor = g_prop.on ? accent : white;
        } else if (role >= TBR_BrushSize && role <= TBR_Group){
            // PINCEL (Weight Paint): modo (+/-/=) y grupo activo. El RADIO y el VALOR ya no son
            // botones: viven en la fila de barras deslizables de arriba (RenderBrushBar).
            btn->visible = brush && !transforming && !(texture && role == TBR_Group);   // textura: sin grupos
            btn->tint = NULL; btn->textColor = NULL;
            if (role == TBR_BrushMode){
                // sumar/restar/igualar es de PESOS: a un color no se le suma ni se le resta.
                // En vertex paint el boton no va (la intensidad la da la barra "valor").
                btn->visible = brush && !transforming && !color && !texture;
                btn->text = lMode;                          // "+" / "-" / "="
                int md = BrushGet().mode;
                // restar: rojizo (SACA peso, como el cancelar). igualar: accent, porque no acumula
                // nada -- PISA el peso con el valor exacto, y conviene que se note de un vistazo.
                btn->tint      = (md == WPSubtract) ? TbRedBg() : (md == WPEqualize) ? TbGreenBg() : NULL;
                btn->textColor = (md == WPSubtract) ? TbRed()   : (md == WPEqualize) ? accent      : NULL;
            }
            else btn->text = lGroup;
        } else if (role == TBR_ColorBrush){
            btn->visible = (color || texture) && !transforming;
            btn->tint = BrushGet().color;   // el boton ES la muestra de color (se tinta en vivo)
            btn->textColor = NULL;
        } else if (role == TBR_Marks){
            // los cuadraditos por punto pintable: toggle, accent cuando esta ON
            btn->visible = brush && !transforming && !texture;
            const bool on = (BrushGet().marks != MarksOff);
            btn->tint = on ? TbGreenBg() : NULL;
            btn->textColor = on ? accent : white;
        } else if (role == TBR_PerVertex){
            // vertex color: por VERTICE (accent) o por face corner. Solo pintando color.
            btn->visible = color && !transforming;
            const bool on = BrushGet().perVertex;
            btn->tint = on ? TbGreenBg() : NULL;
            btn->textColor = on ? accent : white;
        } else if (role == TBR_Falloff){
            // muestra el falloff ACTIVO (icono si ya tiene arte + nombre traducido)
            btn->visible = brush && !transforming;
            const int type = BrushGet().falloff.type;
            btn->icon = IconIndex(W3dFalloffIcon(type));
            btn->text = T(W3dFalloffName(type));
            btn->tint = NULL; btn->textColor = NULL;
        } else if (role == TBR_OnlySel){
            // "editar solo lo seleccionado" (mascara de pintura): solo en Weight Paint.
            // Toggle GLOBAL compartido con el UV editor; tinte accent cuando esta ON.
            btn->visible = brush && !transforming && !texture;
            bool on = WeightPaintOnlySel();
            btn->tint = on ? TbGreenBg() : NULL;
            btn->textColor = on ? accent : white;
        } else { // historial de acciones (solo FUERA de un transform y fuera del modo pincel)
            int hi = role - TBR_Hist;
            btn->visible = !transforming && !brush && hi >= 0 && hi < (int)h.size();
            if (btn->visible){
                int ic = ToolbarIcon(h[hi]);
                btn->icon = ic;                                   // icono (o -1)
                btn->text = (ic >= 0) ? "" : ToolbarActionLabel(h[hi]); // con icono, sin texto (barra mas chica)
            }
        }
        // usuario EXPERIMENTADO (cfg.newUser=false): barra MINIMA con SOLO Undo/Redo (que
        // "siempre tienen que estar"); el resto se oculta. EXCEPTO durante un transform (controles
        // tilde/cruz/ejes) y EXCEPTO los controles del pincel en Weight Paint (sin ellos no se
        // puede ajustar el pincel: no hay atajos de teclado todavia). El IMAN tambien queda (en el telefono es la unica
        // forma de prenderlo sin abrir el menu).
        if (!cfg.newUser && !transforming && role != TBR_Undo && role != TBR_Redo && role != TBR_Snap &&
            !(brush && role >= TBR_BrushSize && role <= TBR_ColorBrush)) btn->visible = false;
    }
}

// accion de un boton de la toolbar (la busqueda tolerante + el consumo los hace el
// ToolbarClick COMPARTIDO de ViewportBase; aca llega solo el ROL del boton pulsado).
void Viewport3D::ToolbarActionRole(int role){
    // INSERTAR CARAS / BEVEL: el tilde y la cruz son los suyos; sus opciones, las mismas teclas de la PC
    if (InsetActive() || BevelActive()){
        const bool inset = InsetActive();
        if (role == TBR_Accept)      { if (inset) InsetConfirm(); else BevelConfirm(); }
        else if (role == TBR_Cancel) { if (inset) InsetCancel(); else BevelCancel(); }
        else if (role == TBR_InsetIndividual) InsetKey(W3dK_I);
        else if (role == TBR_InsetBoundary)   InsetKey(W3dK_B);
        else if (role == TBR_InsetOutset)     InsetKey(W3dK_O);
        else if (role == TBR_InsetDepth)      InsetDepthDrag() = !InsetDepthDrag();   // (el proximo toque arranca desde ahi)
        else if (role == TBR_BevelSegLess)    BevelWheel(-1);
        else if (role == TBR_BevelSegMore)    BevelWheel(+1);
        else if (role == TBR_BevelVertices)   BevelKey(W3dK_V);
        g_redraw = true;
        return;
    }
    if (role == TBR_Undo){ UndoUndo(); RebindMaterialMeshPart(); }       // flecha izquierda: deshacer
    else if (role == TBR_Redo){ UndoRedo(); RebindMaterialMeshPart(); }  // flecha derecha: rehacer
    else if (role == TBR_Accept) Accept();               // tilde verde: confirma el transform
    else if (role == TBR_Repeat){                        // "Repeat": acepta el extrude y vuelve a extruir
        Accept();               // confirma el extrude actual (deja la tapa seleccionada)
        LayoutExtrudeFaces();   // y extruye de nuevo esa seleccion + arranca el move
    }
    else if (role == TBR_Cancel){                       // cruz roja: cancela (mismo camino que el click derecho)
        if (BoneGrabActive()) BoneGrabCancel();         // transform de huesos: restaura las puntas
        else if (InteractionMode == EditMode && EditXformActive()) EditXformCancel();
        else Cancel();
        NumInputReset();
    }
    else if (role == TBR_Orient){
        Button* b = BarRoleBtn(ToolButtons, TBR_Orient);
        LayoutMenuOrientToolbar(b ? b->sx : x, y + height - ToolbarHeight());
    }
    else if (role >= TBR_AxisX && role <= TBR_AxisZ) ToolbarToggleAxis(1 << (role - TBR_AxisX)); // combinables
    else if (role >= TBR_BrushSize && role <= TBR_Group){
        // PINCEL (Weight Paint): el boton de modo CICLA sumar -> restar -> igualar -> sumar;
        // grupo abre su dropdown. (radio y valor son la fila de barras, no pasan por aca)
        Button* b = BarRoleBtn(ToolButtons, role);
        int bx = b ? b->sx : x, byTop = y + height - ToolbarHeight();
        if (role == TBR_BrushMode) BrushGet().mode = (BrushGet().mode + 1) % 3;
        else if (role == TBR_Group){ // la UNIDAD que se pinta: vertex group (pesos) o capa (color)
            Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
            if (!m) return;
            if (InteractionMode == VertexPaint) WeightPaintMenuLayerColor(m, bx, byTop);
            else                                WeightPaintMenuGroup(m, bx, byTop);
        }
    }
    else if (role == TBR_OnlySel){ // "editar solo lo seleccionado": toggle global (compartido con el UV)
        WeightPaintOnlySel() = !WeightPaintOnlySel(); g_redraw = true;
    }
    else if (role == TBR_Marks){ // cuadraditos por punto pintable (y el pincel pasa a trabajar sobre ellos)
        // en PESOS el punto pintable es el VERTICE (el skinning lee un peso por vertice)
        BrushGet().marks = (BrushGet().marks != MarksOff) ? MarksOff : MarksVertex;
        g_redraw = true;
    }
    else if (role == TBR_PerVertex){ // vertex color: por vertice <-> por face corner
        BrushGet().perVertex = !BrushGet().perVertex; g_redraw = true;
    }
    else if (role == TBR_Falloff){ // el editor de falloff, que es el popup REUTILIZABLE
        Button* b = BarRoleBtn(ToolButtons, TBR_Falloff);
        FalloffEditorOpen(&BrushGet().falloff, b ? b->sx : x, y + height - ToolbarHeight());
    }
    else if (role == TBR_ColorBrush){ // el color del pincel: el ColorPicker de siempre
        Button* b = BarRoleBtn(ToolButtons, TBR_ColorBrush);
        if (!colorPicker) colorPicker = new ColorPicker();
        colorPicker->Open(BrushGet().color, b ? b->sx : x, y + height - ToolbarHeight());
        // pestania "Pal": elegir un color de la paleta EFECTIVA del objeto (o "Propio" para
        // color libre). Lo pintado con indice sigue a la paleta -> palette-swap en el modelo.
        colorPicker->SetPalette(&BrushGet().palIdx, ActiveObj, NULL);
    }
    else if (role == TBR_Shift) LShiftPressed = !LShiftPressed; // modificador tactil: queda encendido (verde)
    else if (role == TBR_Ctrl)  LCtrlPressed  = !LCtrlPressed;
    else if (role == TBR_View)  g_viewEditMode = !g_viewEditMode; // toggle: orbitar/panear con el dedo durante una operacion
    else if (role == TBR_Proportional) g_prop.on = !g_prop.on;     // proportional editing: un click (la barra de influencia sale arriba)
    else if (role == TBR_Snap) SnapToggle();                       // el iman: un click (avisa "Snap: ON / OFF")
    else if (role >= TBR_Hist && role < TBR_Hist + 8){
        std::vector<int>& h = ToolbarHist();
        int hi = role - TBR_Hist;
        if (hi < (int)h.size()) ToolbarRun(h[hi]); // historial: arranca la accion
    }
}

void Viewport3D::RenderToolbar(){
    // MODO JUEGO: sin barra de herramientas inferior (undo/redo/shift/historial
    // son de EDICION; el juego se mira limpio). Se ocultan para que el hit-test
    // tampoco los agarre.
    { extern bool SimActive();
      if (AnimIsGame && SimActive()) {
          for (size_t i = 0; i < ToolButtons.size(); i++) ToolButtons[i]->visible = false;
          return;
      } }
    ViewportBase::RenderToolbar(); // el dibujo compartido (fondo + botones, ToolbarBase.cpp)
}

// la barra de menu (arriba) O la de herramientas (abajo): el gesto de arrastre queda lockeado
// a la que se toco (toolGesture) para que BarScrollBy scrollee la correcta.
