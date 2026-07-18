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
#include "ViewPorts/PopUp/NumPad.h" // NumPadAbrirTransform (teclado tactil sobre la barra de estado)
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
// ============================================================================
extern bool g_redraw;
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controles.cpp: hubo input tactil en la sesion
static bool ToolbarUsaTactil(){ return g_lastFingerTicks != 0; }
#else
static bool ToolbarUsaTactil(){ return false; } // (cuando el N8 tenga touch, cablear aca)
#endif

// historial MRU por modo. Arranca con Move/Rotate/Scale (defaults); usar una accion la sube adelante.
static std::vector<int> gToolHistObj;
static std::vector<int> gToolHistEdit;
static std::vector<int>& ToolbarHist(){
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
    std::vector<int>& h = ToolbarHist();
    for (size_t i = 0; i < h.size(); i++)
        if (h[i] == id){ h.erase(h.begin() + i); break; } // sin repetir
    h.insert(h.begin(), id);                              // la ultima usada, primera
    if (h.size() > 8) h.pop_back();                       // hasta 8
    g_redraw = true;
}
static const char* ToolbarLabel(int id){
    switch (id){
        case TBMove:    return T("Move");
        case TBRotate:  return T("Rotate");
        case TBScale:   return T("Scale");
        case TBExtrude: return T("Extrude");
        case TBLoopCut: return T("Loop Cut");
        case TBDelete:  return T("Delete");
    }
    return "?";
}
static void ToolbarEjecutar(int id){
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
    if (estado != editNavegacion) ReestablecerEstado(false); // re-aplica el transform con el eje nuevo
    g_redraw = true;
}

static bool ToolbarTransformando(){
    extern int g_poseModo;
    return (estado == translacion || estado == rotacion || estado == EditScale) &&
           (InteractionMode == ObjectMode || (InteractionMode == EditMode && EditXformActivo()) ||
            (InteractionMode == PoseMode && g_poseModo)); // POSE: tambien muestra tilde/cruz + ejes en el tactil
}

bool Viewport3D::ToolbarVisible() const {
#ifdef W3D_SYMBIAN
    return cfg.nuevoUsuario; // el N95 va a teclas; un N8 tactil la prende por config
#else
    return true; // PC/Android/Web: SIEMPRE (Undo/Redo tienen que estar; el experimentado ve solo esos)
#endif
}
int  Viewport3D::ToolbarHeight() const { return BarHeight(); }

bool Viewport3D::OnToolbar(int px, int py){
    if (!ToolbarVisible()) return false;
    int barH = ToolbarHeight();
    int yBar = y + height - barH; // pegada abajo (la barra de menu esta arriba)
    return px >= x && px < x + width && py >= yBar && py < yBar + barH;
}

// colores de los ejes (X rojo / Y verde / Z azul) + fondos "iluminados" (se llenan por frame)
static const float kTbEje[3][3] = { {0.90f,0.25f,0.25f}, {0.30f,0.85f,0.30f}, {0.35f,0.55f,1.00f} };
static float sTbEjeBg[3][3];
static const float kTbRojo[3]   = { 0.92f, 0.28f, 0.24f };
static const float kTbRojoBg[3] = { 0.41f, 0.13f, 0.11f };
static float sTbVerdeBg[3];

// visibilidad CONTEXTUAL + colores + layout (sx/sy absolutos con el scroll aplicado).
// Sin transform: SOLO el historial. Durante un transform: orientacion + ejes (View: sin Y,
// que es la profundidad de la vista) + tilde/cruz si el input es tactil. Mismos Button de arriba.
void Viewport3D::ToolbarActualizar(){
    bool transformando = (Viewport3DActive == this) && ToolbarTransformando();
    bool tactil = ToolbarUsaTactil();
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
    for (int i = 0; i < 3; i++){
        sTbVerdeBg[i] = accent[i] * 0.40f;
        for (int e = 0; e < 3; e++) sTbEjeBg[e][i] = kTbEje[e][i] * 0.45f;
    }
    int mask = transformando ? ToolbarEjesMask() : 0;
    std::vector<int>& h = ToolbarHist();

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
            btn->visible = transformando && tactil;
            btn->tinte = sTbVerdeBg; btn->colorTexto = accent;
        } else if (rol == TBR_Repeat){
            // solo durante un EXTRUDE (tactil): acepta y vuelve a extruir. Verde como el OK.
            btn->visible = transformando && tactil && ExtrudeEnCurso();
            btn->tinte = sTbVerdeBg; btn->colorTexto = accent;
        } else if (rol == TBR_Cancelar){
            btn->visible = transformando && tactil;
            btn->tinte = kTbRojoBg; btn->colorTexto = kTbRojo;
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
            btn->tinte = on ? sTbEjeBg[e] : NULL;     // encendido: fondo de SU color
            btn->colorTexto = on ? blanco : kTbEje[e]; // apagado: la letra en su color
        } else if (rol == TBR_Shift || rol == TBR_Ctrl){
            // modificadores tactiles: visibles con pantalla tactil, MENOS durante una edicion en curso
            // (transform/extrude/strip) -> ahi estorban (pedido Dante). Encendidos (verde) = LShift/LCtrlPressed.
            btn->visible = tactil && !transformando;
            bool on = (rol == TBR_Shift) ? LShiftPressed : LCtrlPressed;
            btn->tinte = on ? sTbVerdeBg : NULL;       // ON: fondo verde accent
            btn->colorTexto = on ? accent : blanco;    // ON: texto verde; OFF: blanco
        } else if (rol == TBR_View){
            // toggle VIEW: solo en Edit Mode y con pantalla tactil (en PC el mouse orbita distinto). Verde = ON.
            btn->visible = tactil && (InteractionMode == EditMode);
            btn->tinte = g_viewEditMode ? sTbVerdeBg : NULL;
            btn->colorTexto = g_viewEditMode ? accent : blanco;
        } else { // historial de acciones (solo FUERA de un transform)
            int hi = rol - TBR_Hist;
            btn->visible = !transformando && hi >= 0 && hi < (int)h.size();
            if (btn->visible) btn->text = ToolbarLabel(h[hi]);
        }
        // usuario EXPERIMENTADO (cfg.nuevoUsuario=false): barra MINIMA con SOLO Undo/Redo (que
        // "siempre tienen que estar"); el resto se oculta. EXCEPTO durante un transform: ahi hay que
        // dejar los controles (tilde/cruz/ejes) para poder confirmar/cancelar en tactil.
        if (!cfg.nuevoUsuario && !transformando && rol != TBR_Undo && rol != TBR_Redo) btn->visible = false;
    }

    // layout: igual que la barra de arriba (Resize + posiciones acumuladas), en dos pasadas
    // (primero el ancho total para clampear el scroll, despues los sx/sy absolutos)
    int barH = ToolbarHeight();
    int yBar = y + height - barH;
    int btnGap = gapGS / 2 + 1;
    int total = gapGS;
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible) continue;
        btn->Resize(width - gapGS * 2);
        if ((btn->rol >= TBR_EjeX && btn->rol <= TBR_EjeZ) ||
            btn->rol == TBR_Undo || btn->rol == TBR_Redo){ // X/Y/Z y Undo/Redo: cuadrados (solo icono)
            btn->width = btn->height;
            btn->card->Resize(btn->width, btn->height);
        }
        total += btn->width + btnGap;
    }
    int maxS = total - width; if (maxS < 0) maxS = 0;
    if (toolScroll > maxS) toolScroll = maxS;
    int bx = gapGS - toolScroll;
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible){ btn->sx = -10000; btn->sy = -10000; btn->hover = false; continue; }
        btn->sx = x + bx;
        btn->sy = yBar + (barH - btn->height) / 2;
        btn->hover = btn->Contains(lastMouseX, lastMouseY);
        bx += btn->width + btnGap;
    }
}

void Viewport3D::ToolbarScrollBy(int delta){
    toolScroll -= delta;
    if (toolScroll < 0) toolScroll = 0;
    ToolbarActualizar(); // re-clampea contra el ancho total actual
    g_redraw = true;
}

bool Viewport3D::ToolbarClick(int mx, int my){
    if (!OnToolbar(mx, my)) return false;
    ToolbarActualizar(); // sx/sy frescos
    // Buscar el boton: match EXACTO, o si el toque cayo al lado (dedo gordo sobre botones chicos) el MAS
    // CERCANO en X dentro de una tolerancia. Asi un tap un poco desviado igual pulsa el boton que se queria
    // en vez de caer en el gap y no hacer nada. La barra es una franja horizontal -> decide el X.
    Button* target = NULL;
    for (size_t i = 0; i < ToolButtons.size(); i++){       // 1) match exacto
        Button* btn = ToolButtons[i];
        if (btn->visible && btn->Contains(mx, my)) { target = btn; break; }
    }
    if (!target){                                          // 2) sin exacto: el mas cercano en X
        int mejorD = 1 << 30;
        for (size_t i = 0; i < ToolButtons.size(); i++){
            Button* b = ToolButtons[i];
            if (!b->visible) continue;
            int cx = b->sx + b->width / 2;
            int d = (mx > cx) ? (mx - cx) : (cx - mx);
            if (d < mejorD) { mejorD = d; target = b; }
        }
        if (target && mejorD > target->width) target = NULL; // gap real (lejos): sin accion
    }
    if (target){
        int rol = target->rol;
        if (rol == TBR_Undo){ UndoDeshacer(); RebindMaterialMeshPart(); }      // flecha izquierda: deshacer
        else if (rol == TBR_Redo){ UndoRehacer(); RebindMaterialMeshPart(); }  // flecha derecha: rehacer
        else if (rol == TBR_Aceptar) Aceptar();               // tilde verde: confirma el transform
        else if (rol == TBR_Repeat){                          // "Repeat": acepta el extrude y vuelve a extruir
            Aceptar();              // confirma el extrude actual (deja la tapa seleccionada)
            LayoutExtrudeFaces();   // y extruye de nuevo esa seleccion + arranca el move
        }
        else if (rol == TBR_Cancelar){                        // cruz roja: cancela (mismo camino que el click derecho)
            if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
            else Cancelar();
            NumInputReset();
        }
        else if (rol == TBR_Orient) LayoutMenuOrientToolbar(target->sx, y + height - ToolbarHeight());
        else if (rol >= TBR_EjeX && rol <= TBR_EjeZ) ToolbarToggleEje(1 << (rol - TBR_EjeX)); // combinables
        else if (rol == TBR_Shift) LShiftPressed = !LShiftPressed; // modificador tactil: queda encendido (verde)
        else if (rol == TBR_Ctrl)  LCtrlPressed  = !LCtrlPressed;
        else if (rol == TBR_View)  g_viewEditMode = !g_viewEditMode; // toggle: orbitar/panear con el dedo durante una operacion
        else ToolbarEjecutar( ToolbarHist()[rol - TBR_Hist] ); // historial: arranca la accion
        g_redraw = true;
    }
    return true; // dentro de la barra: consumir igual (no pasa al 3D)
}

void Viewport3D::RenderToolbar(){
    if (!ToolbarVisible()) return;
    ToolbarActualizar();
    int barH = ToolbarHeight();

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)(height - barH), 0);
    // fondo translucido como la barra de arriba
    const float* gris = ListaColores[static_cast<int>(ColorID::gris)];
    w3dEngine::Color4f(gris[0], gris[1], gris[2], barAlpha);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);
    // botones en sus sx/sy (ya con scroll). Local = sx-x, sy-y-(height-barH). Mismo patron que RenderBar.
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible) continue;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(btn->sx - x), (GLfloat)(btn->sy - y - (height - barH)), 0);
        btn->Render();
        w3dEngine::PopMatrix();
    }
    w3dEngine::PopMatrix();
}

// la barra de menu (arriba) O la de herramientas (abajo): el gesto de arrastre queda lockeado
// a la que se toco (toolGesto) para que BarScrollBy scrollee la correcta.
