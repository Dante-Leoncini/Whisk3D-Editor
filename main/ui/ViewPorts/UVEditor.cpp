#include "ViewPorts/UVEditor.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "W3dNombres.h"            // LA regla de nombres unicos (rename de hueso 2D: las dos puntas)
#include "objects/Mesh.h"          // Mesh, MaterialGroup, Material, Texture
#include "objects/EditMesh.h"      // EditMesh (seleccion de caras para el Sync Selection)
#include "objects/Textures.h"      // Textures[] (atlas de iconos = Textures[0])
#include "w3dGraphics.h"           // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"      // W3dPantallaAlto + helpers de dibujo
#include "WhiskUI/theme/colores.h"       // ListaColores / ColorID
#include "WhiskUI/draw/icons.h"         // IconType (botones-icono SelMode / Pivot)
#include "ViewPorts/Properties.h"  // PropsActivo (parte activa seleccionada)
#include "WhiskUI/Propieties/PropList.h" // PropListMeshParts
#include "render/OpcionesRender.h" // g_redraw (render event-driven)
#include "w3dTexture.h"             // w3dEngine::TextureSize (aspect ratio de la textura del UV editor)
#include "PopUp/PopUpBase.h"       // PopUpActive (file browser / popups modales tienen prioridad)
#include "edit/WeightPaint.h"      // pincel + escritura de pesos (modo UVModoPesos)
#include "ViewPorts/ViewPort3D.h"  // roles TBR_Undo/TBR_Redo de la toolbar (mismos que el 3D)
#include "ViewPorts/TransformUI.h" // UI compartida del transform (barra de info / numerico / tilde-cruz-ejes)
#include "ViewPorts/NumInput.h"    // entrada numerica compartida (NumInputActivo/Reset)
#include "ViewPorts/Parent.h"      // menu Alt+P de huesos (Disconnect Bone / Clear Parent) del boton Disconnect
#include "ViewPorts/Notificaciones.h" // toast al renombrar el vertex group junto al hueso 2D
#include "W3dAviso.h"                 // ...armado SIN desbordar (ver W3dAviso.h)
#include "Undo.h"                  // botones Deshacer/Rehacer de la toolbar del UV
#include "ViewPorts/LayoutInput.h" // InsertarKeyframeContexto / LayoutMenuInsertKeyframe (la I de Insert Keyframe)
#include "animation/Armature2DAnimation.h" // clips del armature 2D (auto key de la pose 2D al soltar)
#include "ViewPorts/Timeline.h"    // DopeRemapIndiceClave: borrar un hueso 2D CORRE los indices que
                                   // guarda la seleccion del dope ("arm2d:<malla>/a<n>/b<IDX>")
#include "animation/Animation.h"   // KfCanal* / ActiveAnimKind
#include <set>
#include <vector>
#include <algorithm> // std::sort (agrupar los bordes UV al buscar la isla del Select Linked)
#include <math.h> // atan2f/cosf/sinf/sqrtf (transform 2D)

namespace gfx = w3dEngine;

// REGISTRO de editores UV vivos: la tarjeta "Armature 2D" del panel Properties y los atajos
// Ctrl+P / Alt+P necesitan saber si ALGUN editor UV esta en modo de huesos, sin recorrer el
// arbol de viewports (los tests crean editores sueltos, fuera del arbol). El ctor registra,
// el dtor desregistra.
static std::vector<UVEditor*> gUVEditores;
UVEditor* UVEditorEnModoHuesos() {
    for (size_t i = 0; i < gUVEditores.size(); i++)
        if (gUVEditores[i] && (gUVEditores[i]->uvModo == UVModoHuesos || gUVEditores[i]->uvModo == UVModoPose))
            return gUVEditores[i];
    return NULL;
}
// algun editor UV VIVO parado en un modo que TRABAJA CON EL RIG 2D: Edit Bones, Pose o el MODO
// OBJETO. El modo objeto entra en la lista porque es el DEFAULT y es justo donde se elige entre
// rigs: la tarjeta del panel (lista de armatures + Add/Rename/Delete) tiene que verse ahi, sino
// para renombrar o borrar un rig habia que entrar antes a huesos. Las filas de HUESO del panel
// siguen atadas a UVEditorEnModoHuesos (en modo objeto no se edita ningun hueso).
UVEditor* UVEditorConRig2D() {
    for (size_t i = 0; i < gUVEditores.size(); i++)
        if (gUVEditores[i] && (gUVEditores[i]->uvModo == UVModoHuesos || gUVEditores[i]->uvModo == UVModoPose ||
                               gUVEditores[i]->uvModo == UVModoObjeto))
            return gUVEditores[i];
    return NULL;
}
// SOLO el modo POSE (no Edit Bones): es el contexto en el que "Insert Keyframe" keyframea la
// POSE 2D (clips del armature 2D). Lo consulta InsertarKeyframeContexto para decidir a cual de
// los cuatro caminos de insert va la 'i' / el menu Animation.
UVEditor* UVEditorEnModoPose() {
    for (size_t i = 0; i < gUVEditores.size(); i++)
        if (gUVEditores[i] && gUVEditores[i]->uvModo == UVModoPose) return gUVEditores[i];
    return NULL;
}
// algun editor UV en modo EDICION de UVs (el contexto donde la 'i' keyframea la capa uv)
UVEditor* UVEditorEnModoEdicionUV() {
    for (size_t i = 0; i < gUVEditores.size(); i++)
        if (gUVEditores[i] && gUVEditores[i]->uvModo == UVModoEdicion) return gUVEditores[i];
    return NULL;
}

// TAB sobre un viewport cualquiera: se lo queda un UV editor OPERATIVO (ViewportKind 4) cuya malla
// ya tiene algun armature 2D, O que este en MODO OBJETO (sino no habria como salir del modo objeto;
// ver el ciclo completo en UVEditor.h). PREDICADO puro: el toggle lo hace TabToggleHuesos cuando la
// tecla llega al viewport por el ruteo normal.
bool UVEditorTomaTab(ViewportBase* vp) {
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 4) return false;
    UVEditor* uv = (UVEditor*)vp;
    if (!uv->EnEdicionUV()) return false;
    return ((Mesh*)ObjActivo)->TieneArm2D() || uv->uvModo == UVModoObjeto;
}

UVEditor::UVEditor() {
    zoom = 1.0f;
    panX = 0.0f; panY = 0.0f;
    // SYNC SELECTION OFF por DEFAULT (semantica de Blender; ver el bloque grande del header):
    // el 3D FILTRA que caras se ven en el UV y la seleccion del UV es PROPIA y POR RENDER-VERT.
    // Con sync ON un click en una copia UV agarraba TODAS las copias del mismo vertice 3D (las
    // costuras) porque el pick iba a la seleccion del 3D y volvia expandido por posRep: inusable
    // para editar UVs (bug reportado por el dueno). El modo espejo sigue disponible en el menu View.
    syncSelection = false;
    filtroSig = 0; filtroSerial = 0; filtroSync = false; // sin inicializar (el 1er frame inicializa)
    repeatTexture = false;
    mostrarChromeUV = false; // off por defecto: no gasta CPU dibujando el overlay
    lastMx = 0; lastMy = 0;
    uvXform = 0; uvXAxis = 0; uvXPivotU = uvXPivotV = uvXStartU = uvXStartV = 0.0f;
    uvXCurU = uvXCurV = 0.0f;                                 // cursor virtual (wrap-safe)
    uvXValU = uvXValV = uvXValAng = 0.0f; uvXValFac = 1.0f;   // valores para la barra de info
    uvCursorU = 0.5f; uvCursorV = 0.5f; // cursor 2D al centro por defecto
    uvSelMode = SelVertex;              // modo de seleccion propio del UV (default vertices)
    uvModo = UVModoObjeto;              // DEFAULT: modo OBJETO (elegir geometria / armature 2D con el click)
    uvModoPrevio = UVModoObjeto;        // a donde vuelve el Tab al salir de Edit Bones
    uvObjArm = false;                   // objeto activo del UV: la GEOMETRIA (hasta que se clickee un hueso)
    uvBoxArmado = false; uvBoxSel = false; uvBoxAdd = false; // box select (B) sin gesto en curso
    uvBoxX0 = uvBoxY0 = uvBoxX1 = uvBoxY1 = 0;
    BarCrear();
    // botones de barra (ademas del icono [0]), cada uno con su ROL estable (BarRolUV): el dispatch
    // (LayoutClickBarraUV) y la visibilidad (SyncBarra) los buscan por rol, NO por indice.
    //
    // ORDEN = EL MISMO QUE LA BARRA DEL VIEWPORT 3D (ViewPort3D.cpp): Mode, SelMode, Pivot, Snap,
    // View, Select, Add, <menus del contexto>, ... Antes el UV ponia SelMode/Pivot DESPUES de
    // View/Select y Add segundo: los dedos iban al lugar equivocado al saltar de un viewport al
    // otro. Reordenar es GRATIS porque el dispatch y la visibilidad van por ROL, no por indice.
    Button* b;
    b = new Button(T("Edit Mode"), (int)IconType::mesh); b->rol = BRUV_Modo; // selector de modo (icono = el modo, como el 3D)
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo operativo (Edit Mode)
    b = new Button("", (int)IconType::selVertex); b->rol = BRUV_SelMode; // SelMode UV (SOLO icono)
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo donde hay sub-seleccion
    b = new Button("", (int)IconType::pivotMedian); b->rol = BRUV_Pivot; // Pivot (SOLO icono, = el 3D)
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo donde hay transform
    // ops del CURSOR 2D. Se llama "Cursor" (no "Snap"): el "Snap" del 3D es el IMAN. Ver BRUV_Snap.
    b = new Button(T("Cursor"), (int)IconType::pivotCursor); b->rol = BRUV_Snap;
        b->desplegable = true; b->visible = false; BarButtons.push_back(b);
    b = new Button("", IconType::monitor); b->rol = BRUV_View;        // checkboxes (Sync/Repeat/Chrome) + Frame
        BarButtons.push_back(b);
    // menu Select: MISMO icono y MISMO lugar (pegado a View) que el del viewport 3D, para que la
    // barra del UV no diverja de la del 3D. Solo operativo y fuera de la pintura de pesos.
    b = new Button("", IconType::seleccion); b->rol = BRUV_Select;    // All / None / Invert / Select Linked
        b->desplegable = true; b->visible = false; BarButtons.push_back(b);
    b = new Button("", IconType::mas); b->rol = BRUV_Add;             // menu Add: Armature 2D / Bone
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo operativo (Edit Mode)
    b = new Button(T("Armature")); b->rol = BRUV_Armature;            // menu Armature: Extrude/Duplicate/Delete/Parent
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo en UVModoHuesos
    b = new Button(T("Texture")); b->rol = BRUV_Texture;              // dropdown: elegir que textura ver
        b->desplegable = true; BarButtons.push_back(b);
    b = new Button("", IconType::keyframe); b->rol = BRUV_Animation;  // menu Animation (icono rombo, = el 3D)
        b->desplegable = true; b->visible = false; BarButtons.push_back(b); // solo operativo (Edit Mode)
    // TOOLBAR inferior (mecanismo compartido de ViewportBase): DESHACER/REHACER primero (siempre,
    // pensando en pantallas tactiles; mismos roles/iconos que el 3D: TBR_Undo/TBR_Redo), despues el
    // historial MRU con Mover/Rotar/Escalar (starters de IniciarXform en edicion, del transform de
    // huesos en Huesos/Pose). Cursor/snap NO van aca: ya estan arriba. En modo PINTURA muestra los
    // controles del PINCEL (roles TBR_Pincel*/TBR_Grupo, los mismos que la toolbar del 3D en Weight
    // Paint) y en modo HUESOS las acciones de hueso (Extrude/Duplicate/Delete/Disconnect).
    b = new Button("", (int)IconType::arrowRight); b->rol = TBR_Undo; b->iconFlip = 1; b->centrado = true; b->cuadrado = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::arrowRight); b->rol = TBR_Redo; b->centrado = true; b->cuadrado = true; b->visible = false; ToolButtons.push_back(b);
    for (int i = 0; i < 3; i++){
        b = new Button(""); b->rol = TBR_UVHist + i; b->visible = false; ToolButtons.push_back(b);
    }
    b = new Button("40px");  b->rol = TBR_PincelTam;    b->desplegable = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("100%");  b->rol = TBR_PincelFuerza; b->desplegable = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("+");     b->rol = TBR_PincelModo;   b->centrado = true; b->cuadrado = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("Group"); b->rol = TBR_Grupo;        b->desplegable = true; b->visible = false; ToolButtons.push_back(b);
    // "editar solo lo seleccionado" (mascara de pintura): toggle GLOBAL compartido con la
    // toolbar del 3D (WeightPaintSoloSel); icono de seleccion, tinte accent cuando esta ON
    b = new Button("", (int)IconType::seleccion); b->rol = TBR_SoloSel; b->centrado = true; b->cuadrado = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Extrude"));    b->rol = TBR_BoneExtrude; b->centrado = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Duplicate"));  b->rol = TBR_BoneDup;     b->centrado = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::borrar); b->rol = TBR_BoneDel; b->centrado = true; b->cuadrado = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Disconnect")); b->rol = TBR_BoneDesc;    b->centrado = true; b->visible = false; ToolButtons.push_back(b);
    // botones de TRANSFORM compartidos (tilde/cruz/X/Y, roles TBR_* del 3D): solo se ven
    // durante un G/R/S propio (ToolbarSincronizarTransform2D los maneja, como en el 3D)
    ToolbarCrearTransform2D(ToolButtons);
    gUVEditores.push_back(this); // registro de editores vivos (ver UVEditorEnModoHuesos)
}

// modo de seleccion EFECTIVO del editor UV: en sync sigue al 3D; si no, el propio.
int UVEditor::ModoUV() const { return syncSelection ? EditSelectMode : uvSelMode; }

// el UV esta OPERATIVO: el activo es una malla y esta en edicion (mismo criterio que el wireframe).
bool UVEditor::EnEdicionUV() const {
    return ObjActivo && ObjActivo->getType() == ObjectType::mesh && (Object*)ObjActivo == g_editMesh;
}

UVEditor::~UVEditor() {
    for (size_t i = 0; i < gUVEditores.size(); i++)
        if (gUVEditores[i] == this) { gUVEditores.erase(gUVEditores.begin() + i); break; }
}

// ---- TOOLBAR inferior (compartida): historial G/R/S por editor ----
// MRU PROPIO del UV editor (compartido entre todos los viewports UV, como el del 3D por modo).
static std::vector<int> gToolHistUV;
static std::vector<int>& UVToolHist(){
    if (gToolHistUV.empty()){ gToolHistUV.push_back(TBMove); gToolHistUV.push_back(TBRotate); gToolHistUV.push_back(TBScale); }
    return gToolHistUV;
}

// TOOLBAR SIEMPRE PRESENTE en PC (mismo criterio que la del 3D, ViewPort3D_Toolbar.cpp: "Undo/Redo
// tienen que estar"). Antes desaparecia con el UV fuera de Edit Mode y con ella se iban Deshacer y
// Rehacer justo cuando mas se los busca (volviste a Object Mode y te arrepentiste). El CONTENIDO
// sigue siendo contextual: sin malla en edicion solo quedan Undo/Redo (ToolbarSincronizar).
// En Symbian se deja como estaba (solo operativo): ahi el ancho de pantalla es el recurso escaso.
bool UVEditor::ToolbarVisible() const {
#ifdef W3D_SYMBIAN
    return EnEdicionUV();
#else
    return true;
#endif
}

void RebindMaterialMeshPart(); // (def en Properties.cpp) refresca el panel de material tras undo/redo

// eje bloqueado del transform de HUESOS 2D de ESTE editor, para los botones X/Y de la toolbar:
// 0 libre / 1 X / 2 Y, o -1 = "sin ejes" (rotar, o no hay transform de huesos aca).
// Se define abajo, junto al estado gB2D (aca todavia no existe).
static int B2DEjeUI(const UVEditor* uv);

// visibilidad CONTEXTUAL de la toolbar: DESHACER/REHACER siempre (menos con un transform en curso,
// como el 3D); modo EDICION/HUESOS/POSE = historial Move/Rotate/Scale (+ las acciones de HUESO en
// Huesos); modo PINTURA = controles del PINCEL (tam/fuerza/modo/grupo, roles compartidos con el 3D).
void UVEditor::ToolbarSincronizar(){
    const bool operativo = EnEdicionUV();
    const bool edicion = (uvModo == UVModoEdicion) && operativo;
    const bool pintura = (uvModo == UVModoPesos) && operativo;
    const bool huesos  = (uvModo == UVModoHuesos) && operativo;
    const bool pose    = (uvModo == UVModoPose) && operativo;
    const bool transformando = XformEnCurso(); // G/R/S de UVs o transform de huesos 2D de ESTE editor
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
    std::vector<int>& h = UVToolHist();
    std::string lTam, lFuerza, lModo, lGrupo;
    if (pintura){
        Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        WeightPaintLabelsUV(m, lTam, lFuerza, lModo, lGrupo); // el boton de grupo = UV group activo
    }
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        int rol = btn->rol;
        int hi = rol - TBR_UVHist;
        if (rol == TBR_Undo || rol == TBR_Redo){
            // siempre visibles (tactil), MENOS durante una edicion en curso; atenuados si no hay
            // nada. NO dependen de 'operativo': Deshacer/Rehacer tienen que estar tambien con el UV
            // fuera de Edit Mode (mismo criterio que la toolbar del 3D).
            btn->visible = !transformando;
            bool hay = (rol == TBR_Undo) ? UndoHayAlgo() : UndoHayRedo();
            btn->tinte = NULL; btn->colorTexto = hay ? blanco : grisUI;
        } else if (hi >= 0 && hi < 3){
            btn->visible = (edicion || huesos || pose) && !transformando && hi < (int)h.size();
            if (btn->visible) btn->text = ToolbarAccionLabel(h[hi]);
        } else if (rol >= TBR_PincelTam && rol <= TBR_Grupo){
            btn->visible = pintura;
            if (rol == TBR_PincelTam)         btn->text = lTam;
            else if (rol == TBR_PincelFuerza) btn->text = lFuerza;
            else if (rol == TBR_PincelModo)   btn->text = lModo;
            else                              btn->text = lGrupo;
        } else if (rol == TBR_SoloSel){
            // "editar solo lo seleccionado": toggle global compartido con el 3D; accent = ON
            btn->visible = pintura;
            bool on = WeightPaintSoloSel();
            btn->tinte = on ? TbVerdeBg() : NULL;
            btn->colorTexto = on ? ListaColores[static_cast<int>(ColorID::accent)] : blanco;
        } else if (rol >= TBR_BoneExtrude && rol <= TBR_BoneDesc){
            // acciones de HUESO 2D: solo editando huesos (en pose se posa, no se re-estructura)
            btn->visible = huesos && !transformando;
        }
    }
    // TRANSFORM en curso: tilde/cruz/X/Y compartidos (mismas reglas y colores que el 3D).
    // REGLA DE EJES (la misma para el G/R/S de UVs y para el de HUESOS 2D, documentada):
    //   G (mover)   -> botones X / Y visibles (bloqueo por eje en espacio de la vista)
    //   S (escalar) -> X / Y visibles (escala solo ese eje; el otro queda en 1.0)
    //   R (rotar)   -> OCULTOS: una rotacion 2D no tiene eje (el unico eje seria el Z de la
    //                 pantalla, que es justamente lo que ya se rota) -> -1
    const int ejeUI = uvXform ? ((uvXform == 2) ? -1 : uvXAxis) : B2DEjeUI(this);
    ToolbarSincronizarTransform2D(ToolButtons, transformando && operativo, ejeUI);
}

void UVEditor::ToolbarAccionRol(int rol){
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (rol == TBR_Undo){ UndoDeshacer(); RebindMaterialMeshPart(); g_redraw = true; return; }
    if (rol == TBR_Redo){ UndoRehacer(); RebindMaterialMeshPart(); g_redraw = true; return; }
    // botones de TRANSFORM compartidos (tilde/cruz/X/Y): mismo camino que las teclas
    if (XformEnCurso() && ToolbarAccionTransform2D(this, rol)) return;
    if (uvModo == UVModoPesos){
        // "editar solo lo seleccionado": toggle global (compartido con la toolbar del 3D)
        if (rol == TBR_SoloSel){ WeightPaintSoloSel() = !WeightPaintSoloSel(); g_redraw = true; return; }
        // PINTURA: controles del pincel (mismo dispatch que la toolbar del 3D en Weight Paint)
        if (rol < TBR_PincelTam || rol > TBR_Grupo) return;
        Button* b = BarRolBtn(ToolButtons, rol);
        int bx = b ? b->sx : x, byTop = y + height - ToolbarHeight();
        if (rol == TBR_PincelModo)        { BrushGet().modo = BrushGet().modo ? 0 : 1; g_redraw = true; }
        else if (rol == TBR_PincelTam)    WeightPaintMenuTam(bx, byTop);
        else if (rol == TBR_PincelFuerza) WeightPaintMenuFuerza(bx, byTop);
        else if (m)                       WeightPaintMenuUVGroup(m, bx, byTop); // TBR_Grupo: UV groups
        return;
    }
    if (!m || (Object*)m != g_editMesh) return;
    // ===== modos HUESOS/POSE (armature 2D del mesh) =====
    if (uvModo == UVModoHuesos || uvModo == UVModoPose){
        if (Bone2DXformActivo()) { XformConfirmar(); return; } // un transform en curso se CONFIRMA (con gate numerico)
        if (rol == TBR_BoneExtrude && uvModo == UVModoHuesos){
            int nb = Bone2DExtruir(m);
            if (nb >= 0) Bone2DDragEnd(m, nb, 2, false); // el tail nuevo queda agarrado al mouse
            return;
        }
        if (rol == TBR_BoneDup  && uvModo == UVModoHuesos){
            if (Bone2DDuplicar(m) >= 0) Bone2DXformStart(m, 1); // lo duplicado queda agarrado (mover)
            return;
        }
        if (rol == TBR_BoneDel  && uvModo == UVModoHuesos){ Bone2DBorrar(m); return; }
        if (rol == TBR_BoneDesc && uvModo == UVModoHuesos){
            // el MISMO popup de Alt+P (Disconnect Bone / Clear Parent), bajo el boton
            Button* b = BarRolBtn(ToolButtons, TBR_BoneDesc);
            LayoutMenuBoneAltP2D(m, b ? b->sx : x, y + height - ToolbarHeight());
            return;
        }
        int hi = rol - TBR_UVHist;
        std::vector<int>& h = UVToolHist();
        if (hi >= 0 && hi < (int)h.size()){
            int id = h[hi];
            Bone2DXformStart(m, (id == TBMove) ? 1 : (id == TBRotate) ? 2 : 3);
        }
        return;
    }
    if (uvXform) { XformConfirmar(); return; } // como cualquier click: un transform en curso se CONFIRMA (con gate)
    int hi = rol - TBR_UVHist;
    std::vector<int>& h = UVToolHist();
    if (hi >= 0 && hi < (int)h.size()){
        int id = h[hi];
        IniciarXform(m, (id == TBMove) ? 1 : (id == TBRotate) ? 2 : 3); // (registra el MRU adentro)
    }
}

void UVEditor::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH); // el borde sigue el tamano (como Outliner/Properties)
}

// la mesh part SELECCIONADA en el panel de propiedades activo; si no hay, la 0.
static int UVParteActiva(Mesh* m) {
    if (PropsActivo && PropsActivo->propMeshParts &&
        !PropsActivo->propMeshParts->properties.empty()) {
        PropListMeshParts* lst =
            static_cast<PropListMeshParts*>(PropsActivo->propMeshParts->properties[0]);
        if (lst && lst->mesh == m && lst->selectIndex >= 0 &&
            lst->selectIndex < (int)m->materialsGroup.size())
            return lst->selectIndex;
    }
    return 0;
}

// --- override MANUAL de la parte/textura a mostrar (dropdown "Texture" de la barra). -1 = auto (la parte activa).
// El override CAE al cambiar de malla o de seleccion de parte: asi se respeta el auto-cambio al seleccionar objeto/
// meshpart/material, pero mientras tanto podes elegir a mano CUALQUIER textura del modelo desde el dropdown.
static int   g_uvTexOverride = -1;
// la malla del override va por SERIAL (Object::serial, Objects.h), NO por puntero: borrar una
// malla y crear otra recicla la direccion, y con "g_uvTexOvrMesh == m" el override se quedaba
// pegado a la malla NUEVA (la textura que ve el editor UV era la de una malla que ya no existe).
// Nunca se desreferencia: es identidad y nada mas. 0 = ninguna.
static unsigned int g_uvTexOvrSerial = 0;
static int   g_uvAutoAtOvr   = -1;   // UVParteActiva al fijar el override (detecta cambio de seleccion)
// ...y la IDENTIDAD de la parte elegida (nombre + material). El override es un INDICE a
// Mesh::materialsGroup, o sea un miembro (c) de los de Undo.h: reordenar los mesh parts (los botones
// subir/bajar de la tarjeta) corre los indices y el override pasaba a mostrar la textura de OTRA parte.
// No corrompe nada -es solo que texture se ve en el editor UV- pero es la misma clase de bug, y como
// el estado es de este archivo se cierra aca en vez de pedirle un aviso al que reordena: si el
// elemento que hay en esa posicion ya no es el que se eligio, el override CAE al auto.
static std::string g_uvTexOvrNombre;
static Material*   g_uvTexOvrMat = NULL;
static bool UVOverrideSigueSiendoElMismo(Mesh* m){
    if (g_uvTexOverride < 0 || !m || g_uvTexOverride >= (int)m->materialsGroup.size()) return false;
    const MaterialGroup& g = m->materialsGroup[g_uvTexOverride];
    return g.name == g_uvTexOvrNombre && g.material == g_uvTexOvrMat;
}
static int UVParteEfectiva(Mesh* m){
    if (!m) return -1;
    int autoPart = UVParteActiva(m);
    if (g_uvTexOverride >= 0 && (g_uvTexOvrSerial != m->serial || autoPart != g_uvAutoAtOvr ||
                                 g_uvTexOverride >= (int)m->materialsGroup.size() ||
                                 !UVOverrideSigueSiendoElMismo(m))){
        g_uvTexOverride = -1; g_uvTexOvrSerial = 0; // cambio la seleccion/malla/orden -> vuelve al auto
        g_uvTexOvrNombre.clear(); g_uvTexOvrMat = NULL;
    }
    return (g_uvTexOverride >= 0) ? g_uvTexOverride : autoPart;
}
// lo llama el dropdown de la barra (LayoutInput.cpp): fija a mano la parte/textura a ver.
void UVSetTexOverride(Mesh* m, int part){
    g_uvTexOverride = part; g_uvTexOvrSerial = m ? m->serial : 0; g_uvAutoAtOvr = UVParteActiva(m);
    g_uvTexOvrNombre.clear(); g_uvTexOvrMat = NULL;
    if (m && part >= 0 && part < (int)m->materialsGroup.size()){
        g_uvTexOvrNombre = m->materialsGroup[part].name;
        g_uvTexOvrMat    = m->materialsGroup[part].material;
    }
}
int  UVParteMostrada(Mesh* m){ return UVParteEfectiva(m); } // para la barra (nombre del boton)

// un punto (u,v) del espacio UV -> pixel del viewport. V=0 va ARRIBA: la convencion del engine
// es V=0 = arriba de la imagen (stb top-first + el importador OBJ hace 1-v), asi la textura
// se ve DERECHA (no dada vuelta) y el wireframe queda alineado.
// aspecto de la textura activa (1,1 = cuadrada). Lo setea ParamsUV/Render segun la textura de la parte
// activa -> la 0..1 UV se dibuja como RECTANGULO con la proporcion real (ej 32x128 = alto), no siempre cuadrada.
static float g_uvAspU = 1.0f, g_uvAspV = 1.0f;
static inline void UVtoScreen(float u, float v, float cx, float cy, float s,
                              float& sx, float& sy) {
    sx = cx + (u - 0.5f) * s * g_uvAspU;
    sy = cy + (v - 0.5f) * s * g_uvAspV;
}
// calcula el aspecto (g_uvAspU/V) de la textura de la mesh part activa de m. mayor lado = 1.0.
static void CalcAspectoUV(class Mesh* m, int part) {
    g_uvAspU = g_uvAspV = 1.0f;
    if (!m) return;
    Material* mat = (part >= 0 && part < (int)m->materialsGroup.size()) ? m->materialsGroup[part].material : NULL;
    int tw = 0, th = 0;
    if (mat && mat->texture && mat->texture->iID && w3dEngine::TextureSize(mat->texture->iID, tw, th) && tw > 0 && th > 0) {
        if (tw >= th) g_uvAspV = (float)th / (float)tw; // ancha -> achica el alto
        else          g_uvAspU = (float)tw / (float)th; // alta  -> achica el ancho
    }
}

// ===================================================================================================
//  PINTURA DE PESOS en el editor UV (UVModoPesos): el MISMO pincel que el 3D (edit/WeightPaint.cpp)
//  con UVtoScreen como proyector, pero sobre la OTRA entidad: los UV GROUPS (pesos por CORNER /
//  render-vert, Mesh::uvGroups). Los vertex groups del 3D no se tocan.
// ===================================================================================================
static UVEditor* gUVPintando = NULL; // editor UV con un trazo del pincel en curso (NULL = ninguno)

struct WPUVCtx { Mesh* m; float cx, cy, s; };
static bool WPUVProyectar(void* ctx, int i, float& sx, float& sy) {
    WPUVCtx* c = (WPUVCtx*)ctx;
    if (i < 0 || i >= c->m->vertexSize || !c->m->uv) return false;
    UVtoScreen(c->m->uv[i*2], c->m->uv[i*2+1], c->cx, c->cy, c->s, sx, sy);
    return true;
}

// una pasada del pincel en la posicion actual del mouse (lastMx/lastMy, coords locales).
// NO es static a proposito: la declara UVEditor.h para que los tests puedan dar una pasada de
// pincel sin simular SDL (el click y el drag reales siguen siendo los unicos que la llaman).
void UVPintarPesos(UVEditor* uv, Mesh* m) {
    if (!uv || !m || !m->uv) return;
    WPUVCtx c; c.m = m;
    uv->ParamsUV(c.cx, c.cy, c.s); // mismo mapeo UV->pantalla que el Render (el pick coincide)
    BrushEstado& br = BrushGet();
    // mascara "editar solo lo seleccionado" (TBR_SoloSel): SIEMPRE las caras seleccionadas en
    // EDIT MODE (el 3D), en los dos modos de sync -> pintar aca o en el viewport 3D da lo mismo
    // (el toggle es global, WeightPaintSoloSel). Se pasa NULL y el pincel la deriva de la
    // edit mesh. OJO: NO se usa la seleccion PROPIA del UV: es de sub-elementos (podes tener UN
    // vertice UV clickeado) y enmascarar el pincel con eso dejaria de pintar casi todo.
    // POR CORNER (render-vert) SIEMPRE: el pincel del UV escribe el UV GROUP activo -> pesar los
    // 4 corners de UNA cara del cubo no toca las otras caras que comparten esos vertices 3D (que
    // es lo que se ve como islas UV separadas) ni los vertex groups del 3D.
    if (PincelAplicarUV(m, m->uvGrupoActivo, (float)(uv->lastMx - uv->x), (float)(uv->lastMy - uv->y),
                        br.radioPx, br.fuerza, br.modo == 0, WPUVProyectar, &c, NULL)) {
        // PINTAR NO MUEVE UVs. Solo se re-evalua el skinning 2D si hay una POSE REAL que dependa
        // de los pesos que se acaban de cambiar; con la pose en identidad (o sin armature 2D) no
        // hay nada que re-deformar y llamar al skinning seria pisar uv[] al pedo.
        // El rest esta en sync (invariante uv = f(uv2dRest, pose), ver Mesh.h) -> re-aplicar NO
        // pierde las ediciones de UV hechas antes de pintar.
        // El gate mira TODOS los armatures (Arm2DAlgunHueso), no los del ACTIVO: el skinning aplica
        // todos (Armature2DAplicar) y el chequeo de pose tambien, asi que preguntar por el activo
        // dejaba de re-deformar al pintar un UV group de un SEGUNDO rig posado (o con el rig activo
        // vacio) -- justo el caso multi-armature.
        if (m->Arm2DAlgunHueso() && !m->Armature2DPoseIdentidad()) {
            m->Armature2DAplicar();
            m->skinGeomVersion++; // los UV cambiaron -> re-subir el VBO de uv (sino el viewport
                                  // 3D seguia con el uv viejo). Igual que los otros caminos que
                                  // llaman a Armature2DAplicar (ver mas abajo y LayoutInput).
        }
    }
    g_redraw = true; // el relleno por peso se rearma por frame -> feedback en vivo
}

// ===================================================================================================
//  ARMATURE 2D DEL MESH. CORRECCION DE SPEC (el dueño): NO es mostrar el armature de ESCENA en el
//  UV (eso fue un malentendido y se REVIRTIO; el Edit/Pose de huesos 3D sigue viviendo SOLO en el
//  viewport 3D). El armature 2D es un objeto que se CREA DENTRO del editor UV y VIVE DENTRO del
//  MESH (Mesh::armatures2d, ver W3dBone2D / Armature2D en el Core): huesos-linea 2D en el espacio UV cuyo
//  proposito es emparentar y ANIMAR los UVs. Binding POR NOMBRE contra el UV GROUP homonimo (pesos
//  por CORNER; los vertex groups son la otra entidad, la del rig 3D -- ver Mesh.h). Los pesos se
//  pintan con el Weight Paint de siempre. La POSE 2D (G/R/S en X-Y) deforma mesh->uv via
//  Mesh::Armature2DAplicar (skinning 2D, corre en editor Y runtime); la animacion en el tiempo va
//  por los CLIPS PROPIOS de CADA armature (Armature2D::anims, curva por hueso y canal) que llenan el
//  Auto Key / Insert Keyframe. RETROCOMPAT: un rig sin clips propios sigue horneando en la VERTEX
//  ANIMATION UV existente (VertexFrame::uvs), que es como andaban los proyectos viejos.
//  Los modos del UV editor UVModoHuesos (editar rest) y UVModoPose (posar) manejan el input.
// ===================================================================================================
static inline float DistPtSeg2(float px, float py, float ax, float ay, float bx, float by); // (definida mas abajo)

// mouse (global) -> UV, con el MISMO mapeo (y aspecto) que UVtoScreen: el drag sigue al cursor exacto
static void UVMouseAUV(UVEditor* uv, int mx, int my, float& u, float& v) {
    float cx, cy, s; uv->ParamsUV(cx, cy, s);
    float au = (g_uvAspU > 1e-6f) ? g_uvAspU : 1.0f, av = (g_uvAspV > 1e-6f) ? g_uvAspV : 1.0f;
    u = ((float)(mx - uv->x) - cx) / (s * au) + 0.5f;
    v = ((float)(my - uv->y) - cy) / (s * av) + 0.5f;
}

// avanza el cursor VIRTUAL de un transform (en UV) con el DELTA del mouse, SALTEANDO el salto
// del warp: el cursor real se envuelve en el borde del viewport (CheckWarpMouseInViewport,
// como el 3D) y ese teleport no debe sumar. Las cuentas del transform usan este cursor, nunca
// la posicion real (mismo esquema wrap-safe que los acum* del Editor 2D). Mismo mapeo px->UV
// (con el aspecto de la textura) que UVMouseAUV.
static void UVXformAvanzarCursor(UVEditor* uv, int mx, int my, float& curU, float& curV) {
    int jx = mx - uv->lastMx, jy = my - uv->lastMy;
    bool salto = (jx > uv->width / 2 || jx < -uv->width / 2 ||
                  jy > uv->height / 2 || jy < -uv->height / 2);
    if (salto) return;
    float cx, cy, s; uv->ParamsUV(cx, cy, s);
    float au = (g_uvAspU > 1e-6f) ? g_uvAspU : 1.0f, av = (g_uvAspV > 1e-6f) ? g_uvAspV : 1.0f;
    curU += (float)jx / (s * au);
    curV += (float)jy / (s * av);
}

// ---- estado del transform MODAL de huesos 2D (uno solo, con el editor dueño) ----
struct B2DXformEstado {
    UVEditor* uv;            // editor dueño (NULL = nada en curso)
    int   modo;              // 1=mover 2=rotar 3=escalar
    int   eje;               // BLOQUEO DE EJE (teclas X/Y y botones de la toolbar): 0=libre 1=X 2=Y.
                             // Espacio de la VISTA, igual que el uvXAxis del transform de UVs (aca
                             // no hay local/global). En ROTAR no aplica (una rotacion 2D no tiene eje).
    bool  porClick;          // arranco por click (suelta = confirma); false = modal (G/E)
    bool  enPose;            // true = edita la POSE (UVModoPose); false = edita el REST (UVModoHuesos)
    float startU, startV;    // mouse (UV) al arrancar
    float curU, curV;        // cursor VIRTUAL (acumula deltas filtrados; wrap-safe, como uvXCur*)
    float pivU, pivV;        // pivote (mediana de las PUNTAS en movimiento / heads posados en pose)
    float valU, valV;        // delta aplicado (para la barra de INFO)
    float valAng, valFac;    // angulo (grados) / factor aplicados (barra de INFO)
    std::vector<int>   bones;// huesos afectados
    std::vector<int>   masks;// paralelo a bones: que PUNTA se mueve de cada uno (1=head 2=tail 3=ambas).
                             // En pose no aplica (siempre el hueso entero). Viene de la SELECCION por
                             // puntas + la soldadura (Bone2DSeleccionMasks), como el 3D.
    std::vector<float> snap; // snapshot: rest -> 4 por hueso (headU,headV,tailU,tailV);
                             // pose -> 5 por hueso (poseTU,poseTV,poseRot,poseSX,poseSY)
    B2DXformEstado() : uv(NULL), modo(0), eje(0), porClick(false),
                       enPose(false), startU(0), startV(0), curU(0), curV(0),
                       pivU(0), pivV(0), valU(0), valV(0), valAng(0), valFac(1) {}
};
static B2DXformEstado gB2D;
// nucleo del apply de huesos (definido mas abajo): tambien lo usa la entrada numerica (XformNumValor)
static void B2DAplicarValores(Mesh* m, float du, float dv, float ang, float f);
// eje para los botones X/Y de la toolbar durante el transform de huesos de ESTE editor (ver la
// declaracion arriba de ToolbarSincronizar): rotar 2D no tiene eje -> -1 (los oculta).
static int B2DEjeUI(const UVEditor* uv) {
    if (gB2D.modo == 0 || gB2D.uv != uv) return -1;
    return (gB2D.modo == 2) ? -1 : gB2D.eje;
}

// nombre unico para un hueso 2D ("Bone", "Bone.001", ...) dentro del MESH (todos sus
// armatures 2D a la vez: el binding es POR NOMBRE contra el UV group, que es del mesh,
// asi que dos huesos homonimos -aunque sean de rigs distintos- se pelearian los pesos).
// Delega en Mesh::NombreLibreBone2D -> LA regla comun (base/W3dNombres.h). El bucle
// viejo tenia tope 999 y al llegar devolvia la BASE OCUPADA (duplicado en silencio).
static std::string Bone2DNombreUnico(Mesh* m, const std::string& base) {
    return m ? m->NombreLibreBone2D(base, -1, -1) : base;
}
// garantiza el UV GROUP del hueso 2D (binding por nombre: 2D = hueso <-> UV group, ver Mesh.h) y
// lo deja activo (para pintar sus pesos por corner ya). NO toca los vertex groups: esos son del
// armature 3D.
static void Bone2DAsegurarGrupo(Mesh* m, const std::string& nombre) {
    for (size_t g = 0; g < m->uvGroups.size(); g++)
        if (m->uvGroups[g] && m->uvGroups[g]->nombre == nombre) { m->uvGrupoActivo = (int)g; return; }
    m->uvGroups.push_back(new UVGroup(nombre));
    m->uvGrupoActivo = (int)m->uvGroups.size() - 1;
}
// ===== SELECCION por puntas/huesos del armature 2D (calca BoneSel* del 3D, BoneEdit.cpp) =====
// conectado = flag 'conectado' prendido Y head del hijo pegado al tail del padre (el epsilon
// tolera redondeos de guardado/carga; en UV las distancias son ~1 -> mismo 1e-8 que el 3D).
// SOLO la geometria: el head del hueso cae en el tail de su padre? (sin mirar el flag). Aca vive LA
// tolerancia: el flag y el saneador la comparten para no poder discrepar entre si.
static bool Bone2DPuntasSoldadas(Mesh* m, int b) {
    if (!m || b < 0 || b >= (int)m->Arm2DHuesos().size()) return false;
    const W3dBone2D& hb = m->Arm2DHuesos()[b];
    if (hb.padre < 0 || hb.padre >= (int)m->Arm2DHuesos().size()) return false;
    const W3dBone2D& p = m->Arm2DHuesos()[hb.padre];
    const float du = hb.headU - p.tailU, dv = hb.headV - p.tailV;
    return du * du + dv * dv < 1e-8f;
}

bool Bone2DEsConectado(Mesh* m, int b) {
    if (!m || b < 0 || b >= (int)m->Arm2DHuesos().size()) return false;
    if (!m->Arm2DHuesos()[b].conectado) return false;
    return Bone2DPuntasSoldadas(m, b);
}

// INVARIANTE del flag: 'conectado' puede estar en true SOLO si el hueso tiene padre y su head cae en el
// tail de ese padre. Toda op que RE-PARENTE sin mover el hueso (borrar el hueso del medio -> los hijos
// pasan al abuelo, Clear Parent, duplicar, el desplegable Parent, Ctrl+P) puede dejar el flag prendido con
// las puntas separadas: el hueso "dice" que esta soldado y no lo esta. Esto lo restaura de una pasada.
// NO MUEVE NADA: solo apaga el flag donde miente (el hueso que si coincide lo conserva).
// Se llama al final de esas ops para que el invariante no dependa de acordarse en cada una.
bool Bone2DSanearConectado(Mesh* m) {
    if (!m) return false;
    bool cambio = false;
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++) {
        if (!m->Arm2DHuesos()[i].conectado) continue;
        if (Bone2DPuntasSoldadas(m, (int)i)) continue;   // sigue soldado de verdad -> queda
        m->Arm2DHuesos()[i].conectado = false;
        cambio = true;
    }
    return cambio;
}
void Bone2DSelLimpiar(Mesh* m) {
    if (!m) return;
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++) {
        m->Arm2DHuesos()[i].select = false;
        m->Arm2DHuesos()[i].selHead = false;
        m->Arm2DHuesos()[i].selTail = false;
    }
}
void Bone2DSelHueso(Mesh* m, int b, bool sel) {
    if (!m || b < 0 || b >= (int)m->Arm2DHuesos().size()) return;
    m->Arm2DHuesos()[b].select  = sel;
    m->Arm2DHuesos()[b].selHead = sel;
    m->Arm2DHuesos()[b].selTail = sel;
}
// prende/apaga el flag crudo de la punta (sin sincronizar la soldada)
static void B2DSelPuntaRaw(Mesh* m, int b, int punta, bool sel) {
    if (b < 0 || b >= (int)m->Arm2DHuesos().size()) return;
    if (punta == 1) m->Arm2DHuesos()[b].selHead = sel; else m->Arm2DHuesos()[b].selTail = sel;
    if (!sel) m->Arm2DHuesos()[b].select = false; // punta apagada a mano: el hueso ya no esta "entero"
}
// selecciona la punta Y todas sus soldadas: el tail del padre + el head de CADA hijo conectado a
// ese mismo punto (misma regla que BoneSelPunta del 3D).
void Bone2DSelPunta(Mesh* m, int b, int punta, bool sel) {
    if (!m || b < 0 || b >= (int)m->Arm2DHuesos().size()) return;
    B2DSelPuntaRaw(m, b, punta, sel);
    int dueTail = -1; // el hueso cuyo TAIL es esta punta compartida (si existe)
    if (punta == 2) dueTail = b;
    else if (Bone2DEsConectado(m, b)) dueTail = m->Arm2DHuesos()[b].padre;
    if (dueTail < 0) return;              // head raiz/suelto: punta propia, nada mas
    B2DSelPuntaRaw(m, dueTail, 2, sel);
    for (size_t c = 0; c < m->Arm2DHuesos().size(); c++)
        if (m->Arm2DHuesos()[c].padre == dueTail && Bone2DEsConectado(m, (int)c))
            B2DSelPuntaRaw(m, (int)c, 1, sel);
}
// arma bones/masks del transform desde la SELECCION (hueso entero = 3; puntas = 1/2). Sin
// seleccion cae al activo entero. Luego SUELDA: todo tail que se mueve arrastra el head de sus
// hijos conectados (regla one-way del 3D: BoneSeleccionMasks).
static bool Bone2DSeleccionMasks(Mesh* m, std::vector<int>& bones, std::vector<int>& masks) {
    bones.clear(); masks.clear();
    if (!m) return false;
    const int n = (int)m->Arm2DHuesos().size();
    std::vector<int> mask(n, 0);
    for (int i = 0; i < n; i++) {
        const W3dBone2D& b = m->Arm2DHuesos()[i];
        if (b.select) mask[i] = 3;
        else mask[i] = (b.selHead ? 1 : 0) | (b.selTail ? 2 : 0);
    }
    bool hay = false;
    for (int i = 0; i < n; i++) if (mask[i]) { hay = true; break; }
    if (!hay) {
        if (m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < n) mask[m->Arm2DBoneActivo()] = 3;
        else return false;
    }
    bool cambio = true; int guard = 0;
    while (cambio && guard++ <= n) {
        cambio = false;
        for (int c = 0; c < n; c++) {
            int p = m->Arm2DHuesos()[c].padre;
            if (p < 0 || p >= n || !(mask[p] & 2)) continue;
            if (!Bone2DEsConectado(m, c)) continue;
            if (!(mask[c] & 1)) { mask[c] |= 1; cambio = true; }
        }
    }
    for (int i = 0; i < n; i++)
        if (mask[i]) { bones.push_back(i); masks.push_back(mask[i]); }
    return !bones.empty();
}

// extremos EFECTIVOS del hueso i para dibujar/pickear: rest, o POSADOS (matrices FK) en pose.
// 'huesos' es la lista de UN armature y M sus matrices (Armature2DMatricesDe).
static void Bone2DExtremosDe(const std::vector<W3dBone2D>& huesos, const std::vector<float>& M,
                             size_t i, bool pose, float& hu, float& hv, float& tu, float& tv) {
    const W3dBone2D& b = huesos[i];
    hu = b.headU; hv = b.headV; tu = b.tailU; tv = b.tailV;
    if (pose && M.size() >= (i + 1) * 6) {
        const float* W = &M[i * 6];
        float u0 = hu, v0 = hv;
        hu = W[0] * u0 + W[1] * v0 + W[2]; hv = W[3] * u0 + W[4] * v0 + W[5];
        u0 = tu; v0 = tv;
        tu = W[0] * u0 + W[1] * v0 + W[2]; tv = W[3] * u0 + W[4] * v0 + W[5];
    }
}
// idem sobre el armature ACTIVO de la malla (lo que usan el pick y el transform de siempre)
static void Bone2DExtremos(Mesh* m, const std::vector<float>& M, size_t i, bool pose,
                           float& hu, float& hv, float& tu, float& tv) {
    Bone2DExtremosDe(m->Arm2DHuesos(), M, i, pose, hu, hv, tu, tv);
}

// AZUL del armature 2D: el MISMO que la fila del outliner (Outliner.cpp kArm2DAzul). Que sea
// literal y no una constante compartida es a proposito: el outliner no incluye este header.
static const float kArm2DAzul[3] = { 0.28f, 0.55f, 1.00f };

// dibuja los huesos 2D DEL MESH con el lenguaje visual del 3D: sin seleccionar azul, seleccionado
// accent, activo blanco; punteada tail(padre)->head si el hueso esta emparentado con gap (MISMO
// estilo que la relationship line de objetos: textura de puntos chicos + gris); puntos
// (agarraderos) POR PUNTA en UVModoHuesos (cada punta resalta sola, como el Edit del 3D).
//
// PEDIDO DEL DUENO: los armatures 2D se ven SIEMPRE (no solo en Huesos/Pose) y son AZULES. Se
// dibujan TODOS los de la malla:
//   - el armature ACTIVO en azul PLENO (y mas grueso todavia si es el objeto activo del UV);
//   - los demas en azul TENUE (para no tapar el mapa, pero visibles);
//   - solo el ACTIVO y solo en Edit Bones / Pose muestra la seleccion por hueso/punta (accent /
//     blanco) y los agarraderos: en los otros modos el rig es una referencia, no algo editable.
static void UVRenderBones2D(UVEditor* uv, Mesh* m, float cx, float cy, float s) {
    if (!m->TieneArm2D()) return;
    const bool pose = (uv->uvModo == UVModoPose);
    const bool editando = (uv->uvModo == UVModoHuesos || uv->uvModo == UVModoPose);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    // buf = azul del armature activo; bufTenue = los otros armatures; bufSel/bufAct = seleccion
    std::vector<float> buf, bufTenue, bufSel, bufAct, dash, pts, ptsSel;
    for (size_t a = 0; a < m->armatures2d.size(); a++) {
        const Armature2D* arm = m->armatures2d[a];
        if (!arm || arm->huesos.empty()) continue;
        const bool esActivo = ((int)a == m->armature2dActivo);
        const bool conSel   = esActivo && editando;   // seleccion por hueso: solo el que se edita
        std::vector<float> M;
        if (pose) m->Armature2DMatricesDe(arm, M); // TODOS los rigs se ven POSADOS (no solo el activo)
        for (size_t i = 0; i < arm->huesos.size(); i++) {
            const W3dBone2D& b = arm->huesos[i];
            float hu, hv, tu, tv;
            Bone2DExtremosDe(arm->huesos, M, i, pose, hu, hv, tu, tv);
            float ax, ay, bx, by;
            UVtoScreen(hu, hv, cx, cy, s, ax, ay);
            UVtoScreen(tu, tv, cx, cy, s, bx, by);
            std::vector<float>* dst = esActivo ? &buf : &bufTenue;
            if (conSel) {
                if ((int)i == arm->boneActivo) dst = &bufAct;      // activo = blanco
                else if (b.select)             dst = &bufSel;      // seleccionado = accent
            }
            dst->push_back(ax); dst->push_back(ay); dst->push_back(bx); dst->push_back(by);
            if (conSel && !pose) { // edicion: agarraderos POR PUNTA (la seleccionada resalta sola, como el 3D)
                std::vector<float>& PH = b.selHead ? ptsSel : pts;
                PH.push_back(ax); PH.push_back(ay);
                std::vector<float>& PT = b.selTail ? ptsSel : pts;
                PT.push_back(bx); PT.push_back(by);
            }
            // gap con el padre (emparentado pero "separado"): punteada tail(padre)->head. Guardamos el
            // segmento EN PANTALLA y se dibuja despues con la textura de la relationship line.
            if (b.padre >= 0 && b.padre < (int)arm->huesos.size()) {
                float phu, phv, pu, pv;
                Bone2DExtremosDe(arm->huesos, M, (size_t)b.padre, pose, phu, phv, pu, pv); // pu,pv = tail del padre
                float du = hu - pu, dv = hv - pv;
                if (du * du + dv * dv > 0.0001f * 0.0001f) { // hay gap (en UV)
                    float qx, qy;
                    UVtoScreen(pu, pv, cx, cy, s, qx, qy);
                    dash.push_back(qx); dash.push_back(qy); dash.push_back(ax); dash.push_back(ay);
                }
            }
        }
    }
    const float* ac = ListaColores[static_cast<int>(ColorID::accent)];
    // el armature ACTIVO se resalta MAS todavia cuando ademas es el OBJETO ACTIVO del UV
    // (modo objeto): mismo azul, mas ancho y con brillo.
    const bool destacado = uv->uvObjArm;
    gfx::LineWidth(2.0f);
    // 1) los OTROS armatures: azul tenue (visibles pero sin tapar el mapa)
    if (!bufTenue.empty()) {
        gfx::Enable(gfx::Blend); gfx::BlendAlpha();
        gfx::LineWidth(1.5f);
        gfx::Color4f(kArm2DAzul[0], kArm2DAzul[1], kArm2DAzul[2], 0.40f);
        gfx::VertexPointer2f(0, &bufTenue[0]); gfx::DrawLines((int)(bufTenue.size()/2));
        gfx::Disable(gfx::Blend);
    }
    // 2) el armature ACTIVO en azul pleno
    if (!buf.empty()) {
        gfx::LineWidth(destacado ? 3.5f : 2.0f);
        if (destacado) gfx::Color4f(0.45f, 0.72f, 1.0f, 1.0f);   // resaltado (objeto activo del UV)
        else           gfx::Color4f(kArm2DAzul[0], kArm2DAzul[1], kArm2DAzul[2], 1.0f);
        gfx::VertexPointer2f(0, &buf[0]); gfx::DrawLines((int)(buf.size()/2));
    }
    if (!bufSel.empty()) { gfx::LineWidth(3.0f); gfx::Color4f(ac[0], ac[1], ac[2], 1.0f); gfx::VertexPointer2f(0, &bufSel[0]); gfx::DrawLines((int)(bufSel.size()/2)); }
    if (!bufAct.empty()) { gfx::LineWidth(3.5f); gfx::Color4f(1, 1, 1, 1); gfx::VertexPointer2f(0, &bufAct[0]); gfx::DrawLines((int)(bufAct.size()/2)); }
    // LINEA DE PARENTESCO punteada: el MISMO estilo que la relationship line de objetos
    // (Textures[3] repetida a lo largo + gris UI): puntos CHICOS (un punto+hueco cada ~8px), y
    // de OTRO color que los huesos para que no se confundan (pedido del dueno). Un draw por
    // linea: cada una lleva su largo en la coord V de la textura.
    if (!dash.empty() && (int)Textures.size() > 3 && Textures[3] && Textures[3]->iID) {
        gfx::Enable(gfx::Texture2D);
        gfx::EnableArray(gfx::TexCoordArray);
        gfx::Enable(gfx::Blend); gfx::BlendAlpha();
        gfx::BindTexture(Textures[3]->iID);
        gfx::TexFilter(false);
        gfx::TexWrap(true);
        const float* gr = ListaColores[static_cast<int>(ColorID::grisUI)];
        gfx::Color4f(gr[0], gr[1], gr[2], 1.0f);
        gfx::LineWidth(1.0f);
        const float periodo = 8.0f * (float)GlobalScale; // punto + hueco cada ~8px (como los objetos)
        for (size_t k = 0; k + 3 < dash.size(); k += 4) {
            float dx = dash[k+2] - dash[k], dy = dash[k+3] - dash[k+1];
            float len = sqrtf(dx * dx + dy * dy);
            float tuv[4] = { 0.0f, 0.0f, 0.0f, len / periodo };
            gfx::VertexPointer2f(0, &dash[k]);
            gfx::TexCoordPointer2f(0, tuv);
            gfx::DrawLines(2);
        }
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
        gfx::Disable(gfx::Blend);
    }
    if (!pts.empty() || !ptsSel.empty()) {
        gfx::PointSize((float)GlobalScale * 3.0f);
        if (!pts.empty())    { gfx::Color4f(kArm2DAzul[0], kArm2DAzul[1], kArm2DAzul[2], 1.0f); gfx::VertexPointer2f(0, &pts[0]);    gfx::DrawPoints((int)(pts.size()/2)); }
        if (!ptsSel.empty()) { gfx::PointSize((float)GlobalScale * 4.0f); gfx::Color4f(1, 1, 1, 1); gfx::VertexPointer2f(0, &ptsSel[0]); gfx::DrawPoints((int)(ptsSel.size()/2)); }
        gfx::PointSize(1.0f);
    }
    gfx::LineWidth(1.0f);
}

// estado de la BARRA superior por ROL (visibilidad/iconos/textos). SIN GL: separado del Render
// para que el comando de test 'uvbar' pueda verificarlo headless. Render la llama cada frame.
void UVEditor::SyncBarra() {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    const bool enEditUV = (m && (Object*)m == g_editMesh);
    Button* bModo = BarRolBtn(BarButtons, BRUV_Modo);
    Button* bAdd  = BarRolBtn(BarButtons, BRUV_Add);
    Button* bSel  = BarRolBtn(BarButtons, BRUV_SelMode);
    Button* bPiv  = BarRolBtn(BarButtons, BRUV_Pivot);
    Button* bSnap = BarRolBtn(BarButtons, BRUV_Snap);
    Button* bTex  = BarRolBtn(BarButtons, BRUV_Texture);
    Button* bAnim = BarRolBtn(BarButtons, BRUV_Animation);
    // MODOS en los que cada boton REALMENTE opera algo. Un boton visible que no hace nada es la
    // peor clase de UI: el 3D ya oculta SelMode fuera de Edit Mode y Add fuera de Object Mode
    // (ViewPort3D.cpp) y aca se aplica el MISMO criterio (lo pedia la auditoria de consistencia).
    //   sub-seleccion (Vertex/Edge/Face) -> solo editando UVs
    //   transform (Pivot / ops del Cursor 2D) -> donde hay G/R/S: edicion de UVs, Edit Bones, Pose
    //   keyframes (Animation) -> donde hay algo que keyframear: edicion de UVs (capa uv) y Pose (pose 2D)
    const bool conSubSel = enEditUV && (uvModo == UVModoEdicion);
    const bool conXform  = enEditUV && (uvModo == UVModoEdicion || uvModo == UVModoHuesos || uvModo == UVModoPose);
    const bool conKeys   = enEditUV && (uvModo == UVModoEdicion || uvModo == UVModoPose);
    // selector de MODO del UV: el texto Y EL ICONO dicen el modo ACTUAL, igual que el Mode del 3D
    // (que cicla object/mesh/armature). "Pose Mode" (no "Pose") = el mismo label que el 3D;
    // "Edit Bones" en cambio SI difiere a proposito: en el UV conviven la edicion de la GEOMETRIA
    // (Edit Mode = los UVs) y la de los HUESOS 2D en el mismo viewport, y hay que poder distinguirlas.
    if (bModo) {
        bModo->visible = enEditUV;
        bModo->text = (uvModo == UVModoPesos)  ? T("Weight Paint") :
                      (uvModo == UVModoHuesos) ? T("Edit Bones") :
                      (uvModo == UVModoPose)   ? T("Pose Mode") :
                      (uvModo == UVModoObjeto) ? T("Object Mode") : T("Edit Mode");
        bModo->icon = (uvModo == UVModoHuesos || uvModo == UVModoPose) ? (int)IconType::armature :
                      (uvModo == UVModoObjeto)                        ? (int)IconType::object :
                                                                        (int)IconType::mesh;
    }
    // Add (Armature 2D / Bone): solo operativo, como el resto de los botones propios
    if (bAdd) bAdd->visible = enEditUV;
    // menu Armature (Extrude/Duplicate/Delete/Set Parent/Clear Parent): SOLO editando huesos
    { Button* bArm = BarRolBtn(BarButtons, BRUV_Armature);
      if (bArm) bArm->visible = enEditUV && uvModo == UVModoHuesos && m && !m->Arm2DHuesos().empty(); }
    // menu Select: sin seleccion que operar en PINTURA (se pinta) ni en OBJETO (el click elige la
    // entidad y listo) -> se oculta en los dos.
    // menu Select (All/None/Invert/Select Linked): con el UV operativo en TODOS los modos que
    // tienen seleccion (edicion de UVs, Edit Bones y Pose). En PINTURA DE PESOS se OCULTA: ahi no
    // se selecciona nada, se pinta con el pincel (la mascara "solo lo seleccionado" usa la
    // seleccion del 3D, que se opera desde el menu Select del viewport 3D).
    { Button* bSelMenu = BarRolBtn(BarButtons, BRUV_Select);
      if (bSelMenu) bSelMenu->visible = enEditUV && uvModo != UVModoPesos && uvModo != UVModoObjeto; }
    // SelMode/Pivot/Cursor/Animation: SOLO en los modos donde operan (ver el bloque de arriba);
    // los iconos dicen el sub-modo / el pivote actual.
    if (bSel) {
        bSel->visible = conSubSel;
        int mUV = ModoUV();
        bSel->icon = (mUV == SelEdge) ? (int)IconType::selEdge :
                     (mUV == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
    }
    if (bPiv) {
        bPiv->visible = conXform;
        // el icono dice el pivote EFECTIVO del UV, no el elegido: "Active Element" no existe en
        // 2D (el editor UV no tiene elemento activo) y el transform cae en la MEDIANA -> mostrar
        // el icono de pivotActive era un boton que mentia. El menu 2D ya no ofrece esa opcion
        // (LayoutMenuPivotUV), pero el estado es GLOBAL y puede venir puesto desde el 3D.
        bPiv->icon = (g_transformPivot == PivotCursor3D)   ? (int)IconType::pivotCursor :
                     (g_transformPivot == PivotIndividual) ? (int)IconType::pivotIndividual :
                                                             (int)IconType::pivotMedian;
    }
    if (bSnap) bSnap->visible = conXform; // ops del cursor 2D (pivote alternativo del transform)
    if (bAnim) bAnim->visible = conKeys;  // menu Animation: keyframes de la vertex anim / de la pose 2D
    // Texture: visible si la malla tiene >=2 partes; el texto = nombre de archivo de la TEXTURA mostrada.
    if (bTex) {
        bool hayTex = (m && m->materialsGroup.size() >= 2);
        bTex->visible = hayTex;
        if (hayTex) {
            int p = UVParteEfectiva(m);
            Material* mm = (p >= 0 && p < (int)m->materialsGroup.size()) ? m->materialsGroup[p].material : NULL;
            std::string lbl = "Texture";
            if (mm && mm->texture && !mm->texture->path.empty()) {
                const std::string& pt = mm->texture->path;
                size_t sl = pt.find_last_of("/\\");
                lbl = (sl == std::string::npos) ? pt : pt.substr(sl + 1);
            }
            bTex->text = lbl;
        }
    }
}

// distancia^2 de un punto al segmento ab (en pixeles) — para el pick de arista.
static inline float DistPtSeg2(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay; float l2 = dx*dx + dy*dy;
    float t = (l2 > 1e-6f) ? ((px-ax)*dx + (py-ay)*dy) / l2 : 0.0f;
    if (t < 0) t = 0; if (t > 1) t = 1;
    float qx = ax + t*dx, qy = ay + t*dy, ex = px - qx, ey = py - qy;
    return ex*ex + ey*ey;
}

// FILTRO 3D: caras LOGICAS (faces3d) seleccionadas en el viewport 3D. Son las UNICAS que el UV
// dibuja y deja editar con sync OFF (la seleccion del 3D actua como MASCARA). Traduce
// edit->faceSel (indexada por cara del EDIT mesh) a indices de faces3d via faceSrc. UN solo
// lugar: lo usan el Render, el pick, la inicializacion de la seleccion propia y el modo espejo.
static void UVCarasFiltro3D(Mesh* m, std::vector<char>& fsel) {
    if (!m) { fsel.clear(); return; }
    fsel.assign(m->faces3d.size(), 0);
    if ((Object*)m != g_editMesh) return;   // solo en Edit Mode de esta malla
    m->EnsureEdit();
    if (!m->edit) return;
    if (EditSelectMode == SelFace) {
        // modo CARA del 3D: la seleccion de caras TAL CUAL (edit->faceSel -> faces3d via faceSrc)
        for (size_t f = 0; f < m->edit->faceSel.size(); f++)
            if (m->edit->faceSel[f] && f < m->edit->faceSrc.size()) {
                int f3 = m->edit->faceSrc[f];
                if (f3 >= 0 && f3 < (int)m->faces3d.size()) fsel[f3] = 1;
            }
        return;
    }
    // modo VERTICE/BORDE del 3D: una cara entra al filtro si TODOS sus corners estan seleccionados
    // (la misma regla del resaltado del edit mesh y de Delete > Faces). Asi seleccionar verts o
    // bordes en el 3D tambien "abre" sus caras en el UV, sin obligar a pasar a modo cara.
    m->CarasSelPorModo(fsel);
}

// punto dentro del poligono (ray casting) — para el pick de cara (quads/ngons).
static bool PointInPoly(float px, float py, const float* pts, int n) {
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float xi = pts[i*2], yi = pts[i*2+1], xj = pts[j*2], yj = pts[j*2+1];
        if (((yi > py) != (yj > py)) && (px < (xj-xi)*(py-yi)/(yj-yi) + xi)) in = !in;
    }
    return in;
}

void UVEditor::Render() {
    const int glY = W3dPantallaAlto - y - height;

    // fondo
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    const float* bg = ListaColores[static_cast<int>(ColorID::background)];
    gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);

    gfx::Viewport(x, glY, width, height);
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Blend);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);

    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    const float cx = width * 0.5f + panX;
    const float cy = top + ch * 0.5f + panY;
    float baseSize = (float)(width < ch ? width : ch) * 0.8f;
    float s = baseSize * zoom; if (s < 1.0f) s = 1.0f;

    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh)
                  ? (Mesh*)ObjActivo : NULL;
    CalcAspectoUV(m, m ? UVParteEfectiva(m) : -1); // aspect ratio de la textura MOSTRADA (auto o la elegida en el dropdown)
    // barra superior por ROL (visibilidad/iconos/textos): estado puro, extraido a SyncBarra (testeable)
    const bool enEditUV = (m && (Object*)m == g_editMesh);
    SyncBarra();

    // contorno del cuadrado UV (0..1): referencia de los limites de la imagen
    {
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
        float c0x,c0y,c1x,c1y,c2x,c2y,c3x,c3y;
        UVtoScreen(0,0, cx,cy,s, c0x,c0y); UVtoScreen(1,0, cx,cy,s, c1x,c1y);
        UVtoScreen(1,1, cx,cy,s, c2x,c2y); UVtoScreen(0,1, cx,cy,s, c3x,c3y);
        float sq[16] = { c0x,c0y, c1x,c1y,  c1x,c1y, c2x,c2y,
                         c2x,c2y, c3x,c3y,  c3x,c3y, c0x,c0y };
        const float* gris = ListaColores[static_cast<int>(ColorID::grisUI)];
        gfx::Color4f(gris[0], gris[1], gris[2], 1.0f);
        gfx::LineWidth(1.0f);
        gfx::VertexPointer2f(0, sq);
        gfx::DrawLines(8);
    }

    if (m) {
        const int part = UVParteEfectiva(m); // auto (parte activa) o la elegida a mano en el dropdown "Texture"
        Material* mat = (part < (int)m->materialsGroup.size())
                            ? m->materialsGroup[part].material : NULL;

        // --- la textura de la parte activa, centrada ---
        if (mat && mat->texture && mat->texture->iID) {
            gfx::Enable(gfx::Texture2D);
            gfx::EnableArray(gfx::TexCoordArray);
            gfx::BindTexture(mat->texture->iID);
            gfx::TexWrap(repeatTexture);
            gfx::TexFilter(mat->filtrado);
            gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
            const float lo = repeatTexture ? -3.0f : 0.0f;
            const float hi = repeatTexture ?  4.0f : 1.0f;
            float aX,aY,bX,bY,cX,cY,dX,dY;
            UVtoScreen(lo,lo, cx,cy,s, aX,aY); UVtoScreen(hi,lo, cx,cy,s, bX,bY);
            UVtoScreen(hi,hi, cx,cy,s, cX,cY); UVtoScreen(lo,hi, cx,cy,s, dX,dY);
            float P[12] = { aX,aY, bX,bY, cX,cY,  aX,aY, cX,cY, dX,dY };
            float T[12] = { lo,lo, hi,lo, hi,hi,  lo,lo, hi,hi, lo,hi };
            gfx::VertexPointer2f(0, P);
            gfx::TexCoordPointer2f(0, T);
            gfx::DrawTrianglesArray(6);
            gfx::Disable(gfx::Texture2D);
            gfx::DisableArray(gfx::TexCoordArray);
        }

        // --- el wireframe de las UV encima: caras LOGICAS (quads/ngons), SIN triangular ---
        // SYNC SELECTION: ON -> dibuja TODO; las caras seleccionadas en 3D van verde (accent) y el
        // resto gris. OFF -> SOLO las caras seleccionadas. En Object Mode (sin seleccion) = todo verde.
        // en OBJECT MODE no se dibuja el wireframe de la malla sobre la textura: solo se ve
        // la textura. El wireframe (para editar las UV) aparece solo en EDIT MODE de esta malla.
        const bool enEdit = ((Object*)m == g_editMesh);

        // --- RELLENO por PESO (UVModoPesos): caras logicas trianguladas (fan) y coloreadas con la
        // MISMA rampa que el 3D (weightPaintColor, azul 0 -> amarillo 0.5 -> rojo 1), DEBAJO del
        // wireframe. Lo que se muestra aca es el UV GROUP activo (pesos por CORNER); el viewport
        // 3D muestra el VERTEX GROUP activo (por control-point): son dos grupos distintos. ---
        if (uvModo == UVModoPesos && enEdit && m->uv && !m->faces3d.empty()) {
            m->ConstruirColorPesoUV(m->uvGrupoActivo); // por frame (barato): refleja el trazo en vivo
            const int nVw = m->vertexSize;
            // MISMO FILTRO DEL 3D QUE EL WIREFRAME: fuera de sync el UV solo muestra (y solo deja
            // pintar) las caras seleccionadas en el viewport 3D. El relleno por peso recorria
            // TODAS las caras -> aparecia color donde no habia wireframe, o sea color en zonas
            // donde el pincel no puede pintar. Ahora los dos dibujan exactamente lo mismo.
            std::vector<char> selPeso; UVCarasFiltro3D(m, selPeso);
            if ((int)m->weightPaintColor.size() == nVw * 4) {
                std::vector<float> P; std::vector<unsigned char> C;
                for (size_t f = 0; f < m->faces3d.size(); f++) {
                    if (!syncSelection && !(f < selPeso.size() && selPeso[f])) continue;
                    const std::vector<int>& id = m->faces3d[f].idx;
                    const int nc = (int)id.size();
                    for (int c = 1; c + 1 < nc; c++) {  // fan (0, c, c+1): tri/quad/ngon
                        int tri[3]; tri[0] = id[0]; tri[1] = id[c]; tri[2] = id[c+1];
                        if (tri[0] < 0 || tri[0] >= nVw || tri[1] < 0 || tri[1] >= nVw ||
                            tri[2] < 0 || tri[2] >= nVw) continue;
                        for (int k = 0; k < 3; k++) {
                            int gi = tri[k];
                            float px2, py2; UVtoScreen(m->uv[gi*2], m->uv[gi*2+1], cx, cy, s, px2, py2);
                            P.push_back(px2); P.push_back(py2);
                            C.push_back(m->weightPaintColor[gi*4]);   C.push_back(m->weightPaintColor[gi*4+1]);
                            C.push_back(m->weightPaintColor[gi*4+2]); C.push_back(m->weightPaintColor[gi*4+3]);
                        }
                    }
                }
                if (!P.empty()) {
                    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
                    gfx::EnableArray(gfx::ColorArray);
                    gfx::ColorPointer4ub(&C[0]);
                    gfx::VertexPointer2f(0, &P[0]);
                    gfx::DrawTrianglesArray((int)(P.size() / 2));
                    gfx::DisableArray(gfx::ColorArray);
                }
            }
        }
        if (m->uv && !m->faces3d.empty() && enEdit) {
            const int nVerts = m->vertexSize;
            // relacion 3D <-> UV (UN solo punto): en sync espeja la seleccion del 3D; fuera de
            // sync solo INICIALIZA la seleccion propia cuando cambia el filtro (ver el header).
            // Devuelve de paso el FILTRO (caras seleccionadas en el 3D) que usa el dibujo.
            std::vector<char> sel3d;
            SincronizarFiltro3D(m, &sel3d);
            std::vector<float> Lsel, Luns;          // ARISTAS: cara sel en 3D (accent) / no (gris)
            std::vector<float> Psel, Puns;          // PUNTOS del sub-modo (vertex/face)
            std::vector<float> Lwhite, Pwhite;      // SELECCION del editor UV (blanco): aristas / puntos
            // SUB-ELEMENTOS (puntitos de vertice/cara, aristas blancas de la seleccion UV): NO se
            // dibujan en MODO OBJETO. Ahi no hay sub-seleccion que operar (el click elige la
            // entidad: geometria o armature) y el 3D tampoco muestra los verts en Object Mode.
            // Antes quedaban los puntos BLANCOS de la ultima edicion flotando sobre el mapa.
            const bool subEl = enEdit && (uvModo != UVModoObjeto);
            const bool haySelUV = subEl && (int)m->uvSelVert.size() == nVerts;
            const int modoUV = ModoUV();            // vertex/edge/face efectivo (propio o sincronizado)
            for (size_t f = 0; f < m->faces3d.size(); f++) {
                const std::vector<int>& id = m->faces3d[f].idx;
                const int nc = (int)id.size();
                if (nc < 2) continue;
                // en Object Mode todo cuenta como "seleccionado" (no hay sub-seleccion)
                const bool selFace = enEdit ? (f < sel3d.size() && sel3d[f]) : true;
                if (!syncSelection && !selFace) continue; // OFF: solo las seleccionadas
                std::vector<float>& L = selFace ? Lsel : Luns;
                std::vector<float>& P = selFace ? Psel : Puns;
                float fcu = 0, fcv = 0; int fn = 0;     // para el centro de la cara (modo face)
                bool faceAllSel = haySelUV;             // modo face: TODOS los verts UV seleccionados?
                for (int c = 0; c < nc; c++) {            // arista c -> c+1 (cierra el poligono)
                    int ka = id[c], kb = id[(c+1) % nc];
                    if (ka < 0 || ka >= nVerts || kb < 0 || kb >= nVerts) { faceAllSel = false; continue; }
                    float ax,ay,bx,by;
                    UVtoScreen(m->uv[ka*2], m->uv[ka*2+1], cx,cy,s, ax,ay); L.push_back(ax); L.push_back(ay);
                    UVtoScreen(m->uv[kb*2], m->uv[kb*2+1], cx,cy,s, bx,by); L.push_back(bx); L.push_back(by);
                    // sub-modo VERTEX: un punto en cada UV-vert (blanco si esta seleccionado en el editor UV)
                    if (subEl && modoUV == SelVertex) {
                        P.push_back(ax); P.push_back(ay);
                        if (haySelUV && m->uvSelVert[ka]) { Pwhite.push_back(ax); Pwhite.push_back(ay); }
                    } else if (subEl && modoUV == SelEdge) {
                        // sub-modo EDGE: arista blanca si sus 2 extremos UV estan seleccionados
                        if (haySelUV && m->uvSelVert[ka] && m->uvSelVert[kb]) {
                            Lwhite.push_back(ax); Lwhite.push_back(ay); Lwhite.push_back(bx); Lwhite.push_back(by);
                        }
                    }
                    if (haySelUV && !m->uvSelVert[ka]) faceAllSel = false;
                    fcu += m->uv[ka*2]; fcv += m->uv[ka*2+1]; fn++;
                }
                // sub-modo FACE: un punto en el centro UV de la cara (blanco si TODA la cara esta seleccionada)
                if (subEl && modoUV == SelFace && fn > 0) {
                    float vx,vy; UVtoScreen(fcu/fn, fcv/fn, cx,cy,s, vx,vy);
                    P.push_back(vx); P.push_back(vy);
                    if (faceAllSel) { Pwhite.push_back(vx); Pwhite.push_back(vy); }
                }
            }
            const float* gr = ListaColores[static_cast<int>(ColorID::grisUI)];
            const float* ac = ListaColores[static_cast<int>(ColorID::accent)];
            // MODO OBJETO con la GEOMETRIA como objeto activo (uvObjArm == false): el wireframe va
            // MAS GRUESO y todo en accent -> la misma pista visual que el rig resaltado cuando el
            // objeto activo es un armature (ver UVRenderBones2D). Sin esto el modo objeto no daba
            // NINGUNA senal de que lo seleccionado era la malla.
            const bool geoDestacada = (uvModo == UVModoObjeto && !uvObjArm);
            gfx::LineWidth(geoDestacada ? 2.0f : 1.0f);
            if (!Luns.empty()) {
                if (geoDestacada) gfx::Color4f(ac[0],ac[1],ac[2],1.0f);
                else              gfx::Color4f(gr[0],gr[1],gr[2],1.0f);
                gfx::VertexPointer2f(0,&Luns[0]); gfx::DrawLines((int)(Luns.size()/2));
            }
            if (!Lsel.empty()) { gfx::Color4f(ac[0],ac[1],ac[2],1.0f); gfx::VertexPointer2f(0,&Lsel[0]); gfx::DrawLines((int)(Lsel.size()/2)); }
            gfx::LineWidth(1.0f);
            // SELECCION del editor UV en BLANCO, encima de todo (aristas mas gruesas)
            if (!Lwhite.empty()) { gfx::LineWidth(2.0f); gfx::Color4f(1,1,1,1); gfx::VertexPointer2f(0,&Lwhite[0]); gfx::DrawLines((int)(Lwhite.size()/2)); gfx::LineWidth(1.0f); }
            // los puntos del sub-modo (vertex/face) ENCIMA de las aristas; los blancos arriba de todo
            if (!Puns.empty() || !Psel.empty() || !Pwhite.empty()) {
                gfx::PointSize((float)GlobalScale * 3.0f);
                if (!Puns.empty()) { gfx::Color4f(gr[0],gr[1],gr[2],1.0f); gfx::VertexPointer2f(0,&Puns[0]); gfx::DrawPoints((int)(Puns.size()/2)); }
                if (!Psel.empty()) { gfx::Color4f(ac[0],ac[1],ac[2],1.0f); gfx::VertexPointer2f(0,&Psel[0]); gfx::DrawPoints((int)(Psel.size()/2)); }
                if (!Pwhite.empty()) { gfx::PointSize((float)GlobalScale * 4.0f); gfx::Color4f(1,1,1,1); gfx::VertexPointer2f(0,&Pwhite[0]); gfx::DrawPoints((int)(Pwhite.size()/2)); }
            }
        }

        // --- OVERLAY del CHROME UV (equirect) en TIEMPO REAL ---
        // El reflejo recalcula chromeExpUV al orbitar (cache del 3D); aca lo dibujamos como wireframe CYAN.
        // Se ve morphear en vivo el mapeo del reflejo. off = 0 CPU extra (no calcula nada de mas). Demo/debug.
        if (mostrarChromeUV && m->chromeExpUV && m->chromeExpCount >= 3) {
            gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
            std::vector<float> CL;
            for (int t = 0; t + 2 < m->chromeExpCount; t += 3) {
                float xx[3], yy[3];
                for (int k = 0; k < 3; k++)
                    UVtoScreen(m->chromeExpUV[(t+k)*2], m->chromeExpUV[(t+k)*2+1], cx,cy,s, xx[k], yy[k]);
                for (int e = 0; e < 3; e++) { int a = e, b = (e+1)%3;
                    CL.push_back(xx[a]); CL.push_back(yy[a]); CL.push_back(xx[b]); CL.push_back(yy[b]); }
            }
            if (!CL.empty()) {
                gfx::LineWidth(1.0f); gfx::Color4f(0.2f, 0.9f, 0.95f, 1.0f); // cyan
                gfx::VertexPointer2f(0, &CL[0]);
                gfx::DrawLines((int)(CL.size()/2));
            }
        }
    }

    // ===== ARMATURES 2D DEL MESH: SIEMPRE visibles (pedido del dueno: "NO OCULTARSE"). Se dibujan
    // TODOS, en azul, en CUALQUIER modo del UV -- en Pose salen posados, en el resto en rest. El
    // activo va en azul pleno (mas grueso si ademas es el objeto activo) y los otros en tenue. =====
    if (m && enEditUV)
        UVRenderBones2D(this, m, cx, cy, s);

    // LINEA DE EJE del transform con BLOQUEO (X/Y): como el 3D, una linea del color del eje que
    // cruza el viewport pasando por el pivote (en el UV el eje va SIEMPRE en espacio de la vista).
    if (uvXform && uvXAxis) {
        float px, py; UVtoScreen(uvXPivotU, uvXPivotV, cx, cy, s, px, py);
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        float L[4];
        if (uvXAxis == 1) { L[0] = 0; L[1] = py; L[2] = (float)width; L[3] = py;   // X = horizontal, rojo
            gfx::Color4f(0.90f, 0.25f, 0.25f, 1.0f); }
        else              { L[0] = px; L[1] = 0; L[2] = px; L[3] = (float)height;  // Y = vertical, verde
            gfx::Color4f(0.30f, 0.85f, 0.30f, 1.0f); }
        gfx::LineWidth(1.0f);
        gfx::VertexPointer2f(0, L);
        gfx::DrawLines(2);
    }

    // CURSOR 2D (en el espacio UV): cruz roja con halo blanco (estilo cursor 3D). Pivot opcional + snap.
    if (enEditUV) {
        float ccx, ccy; UVtoScreen(uvCursorU, uvCursorV, cx,cy,s, ccx,ccy);
        float r = (float)GlobalScale * 3.0f;
        float cross[8] = { ccx-r,ccy, ccx+r,ccy,  ccx,ccy-r, ccx,ccy+r };
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        gfx::VertexPointer2f(0, cross);
        gfx::LineWidth(3.0f); gfx::Color4f(1,1,1,1); gfx::DrawLines(4);            // halo blanco
        gfx::LineWidth(1.0f); gfx::Color4f(0.9f,0.15f,0.15f,1.0f); gfx::DrawLines(4); // cruz roja
    }

    // RECTANGULO del BOX SELECT (tecla B + arrastre): contorno blanco fino, mismo lenguaje que
    // los demas overlays del UV. Se dibuja arriba de todo menos del pincel.
    if (uvBoxSel) {
        float bx0 = (float)(uvBoxX0 < uvBoxX1 ? uvBoxX0 : uvBoxX1);
        float bx1 = (float)(uvBoxX0 < uvBoxX1 ? uvBoxX1 : uvBoxX0);
        float by0 = (float)(uvBoxY0 < uvBoxY1 ? uvBoxY0 : uvBoxY1);
        float by1 = (float)(uvBoxY0 < uvBoxY1 ? uvBoxY1 : uvBoxY0);
        float R[16] = { bx0,by0, bx1,by0,  bx1,by0, bx1,by1,
                        bx1,by1, bx0,by1,  bx0,by1, bx0,by0 };
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        gfx::VertexPointer2f(0, R);
        gfx::LineWidth(1.0f); gfx::Color4f(1,1,1,1); gfx::DrawLines(8);
    }

    // circulo del PINCEL (solo en modo pintura), siguiendo al mouse sobre el CONTENIDO
    // (no sobre la barra/toolbar ni con un popup modal abierto)
    if (enEditUV && uvModo == UVModoPesos && !PopUpActive &&
        lastMx >= x && lastMx < x + width && lastMy >= y && lastMy < y + height &&
        !OnBar(lastMx, lastMy) && !OnToolbar(lastMx, lastMy))
        BrushDibujarCirculo((float)(lastMx - x), (float)(lastMy - y), BrushGet().radioPx);

    gfx::Disable(gfx::ScissorTest);
    // RenderBar y DibujarBordes dibujan quads/bordes TEXTURIZADOS (iconos del atlas):
    // hay que dejarles el estado prendido (el wireframe de arriba lo apago) Y re-bindear el
    // ATLAS de iconos (Textures[0]) -> sino usan la textura del mesh que quedo bindeada y la
    // barra/borde salen con la textura del modelo encima.
    gfx::Enable(gfx::Texture2D);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::EnableArray(gfx::VertexArray);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    // durante un G/R/S propio la barra de menu se vuelve barra de INFO (operacion + eje +
    // delta en unidades UV), igual que el 3D. El dibujo es el compartido de TransformUI.
    if (XformEnCurso()) RenderBarraInfo(XformTextoBarra());
    else RenderBar();
    RenderToolbar();     // toolbar inferior compartida (G/R/S en edicion; tilde/cruz/X/Y en transform)
    DibujarBordes(this); // borde del viewport (verde si es el activo)
}

void UVEditor::event_mouse_motion(int mx, int my) {
    // si hay un popup modal (file browser, etc.) el UV editor CEDE el input: no transforma
    // ni panea (sino un transform activo seguia robando el mouse con el browser abierto).
    if (PopUpActive) { lastMx = mx; lastMy = my; return; }
    // BOX SELECT en curso: el arrastre agranda el rectangulo (el aplicar es al soltar).
    // Si el boton ya no esta apretado es un "up perdido" (se solto fuera del viewport): se
    // cierra igual, como hacen el trazo del pincel y el drag de huesos.
    if (uvBoxSel) {
        lastMx = mx; lastMy = my;
        if (!leftMouseDown) { BoxSelectSoltar(); return; }
        uvBoxX1 = mx - x; uvBoxY1 = my - y;
        g_redraw = true;
        return;
    }
    // ===== ARMATURE 2D DEL MESH: transform de huesos en curso EN ESTE editor (edicion o pose) =====
    if (Bone2DXformActivo() && gB2D.uv == this) {
        Mesh* mb = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (!mb) { lastMx = mx; lastMy = my; return; }
        if (gB2D.porClick && !leftMouseDown) { lastMx = mx; lastMy = my; Bone2DXformConfirm(mb); return; } // up perdido
        if (NumInputActivo()) { lastMx = mx; lastMy = my; return; } // tipeando un valor exacto: el mouse no interfiere
        // cursor VIRTUAL wrap-safe: avanza por DELTAS filtrando el salto del warp (el cursor
        // se envuelve en el borde del viewport, como el 3D; mismo esquema que el G/R/S de UVs)
        UVXformAvanzarCursor(this, mx, my, gB2D.curU, gB2D.curV);
        lastMx = mx; lastMy = my;
        Bone2DXformDelta(mb, gB2D.curU, gB2D.curV);
        return;
    }
    // PINTURA DE PESOS: el circulo del pincel sigue al mouse (redraw) y, con un trazo en
    // curso (mouse apretado), cada motion pinta otra pasada y consume el evento.
    if (uvModo == UVModoPesos) g_redraw = true;
    if (gUVPintando == this) {
        lastMx = mx; lastMy = my;
        Mesh* mp = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (leftMouseDown && mp) { UVPintarPesos(this, mp); return; }
        gUVPintando = NULL; WeightPaintTrazoFin(); // up perdido (solto fuera): commit igual
        return;
    }
    if (uvXform) {                         // transform en curso: mover la seleccion en vivo
        Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (m) {
            if (NumInputActivo()) { lastMx = mx; lastMy = my; return; } // tipeando: el mouse no interfiere
            // cursor VIRTUAL wrap-safe: acumula los deltas del mouse FILTRANDO el salto del
            // warp (CheckWarpMouseInViewport envuelve el cursor en el borde, como el 3D).
            // El mapeo px->UV es el mismo de UVMouseAUV (CON el aspecto de la textura).
            UVXformAvanzarCursor(this, mx, my, uvXCurU, uvXCurV);
            AplicarXform(m, uvXCurU, uvXCurV);
        }
    } else if (middleMouseDown) {          // boton del medio: paneo
        // SALTEAR el salto del warp: el cursor se envuelve en el borde del viewport
        // (CheckWarpMouseInViewport) y mx/my pegan un salto del ancho/alto. Sin el
        // filtro, ese delta gigante revertia el paneo acumulado (mismo fix que el
        // paneo del Editor 2D).
        int jx = mx - lastMx, jy = my - lastMy;
        bool salto = (jx > width / 2 || jx < -width / 2 || jy > height / 2 || jy < -height / 2);
        if (!salto) {
            panX += (float)jx;
            panY += (float)jy;
        }
        g_redraw = true;
    }
    lastMx = mx; lastMy = my;
}

// parametros del mapeo UV->pantalla (identicos al Render/PickUV): centro + escala.
void UVEditor::ParamsUV(float& cx, float& cy, float& s) const {
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    cx = width * 0.5f + panX;
    cy = top + ch * 0.5f + panY;
    float baseSize = (float)(width < ch ? width : ch) * 0.8f;
    s = baseSize * zoom; if (s < 1.0f) s = 1.0f;
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    CalcAspectoUV(m, m ? UVParteEfectiva(m) : -1); // mismo aspecto que el Render (incluido el
                                                   // override del dropdown Texture) -> el pick coincide
}

// caras LOGICAS seleccionadas para el transform en modo CARA. En sync = las caras seleccionadas
// en el 3D (el filtro, lo mismo que resalta el render); fuera de sync = las caras cuyos corners
// estan TODOS en uvSelVert (la seleccion PROPIA del UV, por copia).
static void UVCarasSel(UVEditor* uv, Mesh* m, std::vector<char>& fsel) {
    const bool enEdit = ((Object*)m == g_editMesh);
    if (uv->syncSelection && enEdit) { UVCarasFiltro3D(m, fsel); return; }
    fsel.assign(m->faces3d.size(), 0);
    if ((int)m->uvSelVert.size() != m->vertexSize) return;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        if (id.empty()) continue;
        bool all = true;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c];
            if (k < 0 || k >= m->vertexSize || !m->uvSelVert[k]) { all = false; break; }
        }
        if (all) fsel[f] = 1;
    }
}

// ===================================================================================================
//  FIX "mover una cara arrastra a la vecina" (bug reportado por el dueño). CAUSA REAL: uv[] es POR
//  RENDER-VERT y GenerarRender MERGEA los corners con la MISMA (pos + uv + normal + color) -> dos
//  caras vecinas con el mismo UV en una esquina COMPARTEN el render-vert, y mover una cara movia esa
//  esquina de la otra. FIX: al arrancar un transform en modo CARA, cada render-vert compartido entre
//  una cara SELECCIONADA y una NO seleccionada se DUPLICA para las seleccionadas (split de esquina):
//  mover una cara afecta EXACTAMENTE sus UVs. Se mantienen coherentes los arrays paralelos
//  (posRep / vertCtrlPoint / uvSelVert / uv2dRest) y los keyframes de vertex anim (RemapVertexAnims:
//  el vert duplicado SIGUE la animacion de su origen). Si se CANCELA el transform, los duplicados
//  quedan con el MISMO uv que su origen y el proximo GenerarRender los re-mergea (inofensivo).
// ===================================================================================================
static void UVSepararCarasSel(Mesh* m, const std::vector<char>& fsel) {
    const int oldN = m->vertexSize;
    if (oldN <= 0) return;
    std::vector<char> usaSel(oldN, 0), usaUnsel(oldN, 0);
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        std::vector<char>& dst = (f < fsel.size() && fsel[f]) ? usaSel : usaUnsel;
        for (size_t c = 0; c < id.size(); c++)
            if (id[c] >= 0 && id[c] < oldN) dst[id[c]] = 1;
    }
    std::vector<int> nuevoDe(oldN, -1); // render-vert compartido -> su copia para las caras sel
    std::vector<int> dupSrc;            // origen de cada vert nuevo
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        if (f >= fsel.size() || !fsel[f]) continue;
        std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c];
            if (k < 0 || k >= oldN || !usaUnsel[k]) continue; // no compartido: queda como esta
            if (nuevoDe[k] < 0) { nuevoDe[k] = oldN + (int)dupSrc.size(); dupSrc.push_back(k); }
            id[c] = nuevoDe[k];
        }
    }
    if (dupSrc.empty()) return; // nada compartido: no habia nada que separar
    const int newN = oldN + (int)dupSrc.size();
    // arrays de render: realloc copiando el origen de cada duplicado
    GLfloat* nvert = new GLfloat[newN * 3];
    GLbyte*  nnor  = m->normals     ? new GLbyte[newN * 3]  : NULL;
    GLfloat* nuv   = new GLfloat[newN * 2];
    GLubyte* ncol  = m->vertexColor ? new GLubyte[newN * 4] : NULL;
    for (int i = 0; i < oldN * 3; i++) nvert[i] = m->vertex[i];
    if (nnor)  for (int i = 0; i < oldN * 3; i++) nnor[i] = m->normals[i];
    for (int i = 0; i < oldN * 2; i++) nuv[i] = m->uv[i];
    if (ncol)  for (int i = 0; i < oldN * 4; i++) ncol[i] = m->vertexColor[i];
    for (size_t d = 0; d < dupSrc.size(); d++) {
        int src = dupSrc[d], dst = oldN + (int)d;
        for (int k = 0; k < 3; k++) nvert[dst*3+k] = m->vertex[src*3+k];
        if (nnor) for (int k = 0; k < 3; k++) nnor[dst*3+k] = m->normals[src*3+k];
        nuv[dst*2] = m->uv[src*2]; nuv[dst*2+1] = m->uv[src*2+1];
        if (ncol) for (int k = 0; k < 4; k++) ncol[dst*4+k] = m->vertexColor[src*4+k];
    }
    delete[] m->vertex;      m->vertex = nvert;
    delete[] m->normals;     m->normals = nnor;
    delete[] m->uv;          m->uv = nuv;
    delete[] m->vertexColor; m->vertexColor = ncol;
    // arrays paralelos: el duplicado hereda el grupo por posicion (posRep) y el control-point
    // (mismos PESOS de vertex group / skinning que su origen)
    if ((int)m->posRep.size() == oldN)
        for (size_t d = 0; d < dupSrc.size(); d++) m->posRep.push_back(m->posRep[dupSrc[d]]);
    if ((int)m->vertCtrlPoint.size() >= oldN)
        for (size_t d = 0; d < dupSrc.size(); d++) m->vertCtrlPoint.push_back(m->vertCtrlPoint[dupSrc[d]]);
    if ((int)m->uvSelVert.size() == oldN) {
        m->uvSelVert.resize(newN, 1);         // los duplicados SON la seleccion (la cara que se mueve)
        for (size_t d = 0; d < dupSrc.size(); d++)
            m->uvSelVert[dupSrc[d]] = 0;      // el original queda con las caras NO seleccionadas
    }
    if ((int)m->uv2dRest.size() == oldN * 2)  // rest del skinning 2D: acompania el split
        for (size_t d = 0; d < dupSrc.size(); d++) {
            m->uv2dRest.push_back(m->uv2dRest[dupSrc[d]*2]);
            m->uv2dRest.push_back(m->uv2dRest[dupSrc[d]*2+1]);
        }
    // UV GROUPS (pesos por corner / render-vert): el duplicado hereda el peso de su origen, igual
    // que el control-point y los keyframes de vertex anim. Sin esto, mover una cara en el UV (que
    // la separa por esquinas) le borraba los pesos pintados encima.
    for (size_t g = 0; g < m->uvGroups.size(); g++) {
        UVGroup* ug = m->uvGroups[g];
        if (!ug || ug->verts.empty()) continue;
        std::vector<float> wViejo((size_t)oldN, 0.0f);
        for (size_t k = 0; k < ug->verts.size() && k < ug->pesos.size(); k++) {
            int rv = ug->verts[k];
            if (rv >= 0 && rv < oldN) wViejo[(size_t)rv] = ug->pesos[k];
        }
        for (size_t d = 0; d < dupSrc.size(); d++) {
            float w = wViejo[(size_t)dupSrc[d]];
            if (w > 0.0f) { ug->verts.push_back(oldN + (int)d); ug->pesos.push_back(w); }
        }
    }
    m->vertexSize = newN;
    // keyframes de vertex anim: el vert duplicado SIGUE la animacion de su origen (posiciones,
    // normales y UV se copian keyframe a keyframe)
    { std::vector<int> n2o(newN);
      for (int i = 0; i < oldN; i++) n2o[i] = i;
      for (size_t d = 0; d < dupSrc.size(); d++) n2o[oldN + (int)d] = dupSrc[d];
      RemapVertexAnims(m, &n2o[0], newN, oldN); }
    m->ReagruparMeshParts(); // rearma faces[] desde faces3d (y bumpea skinGeomVersion -> VBO/CSR)
}

// habra SPLIT de esquinas? (mismo criterio que UVSepararCarasSel: algun render-vert compartido
// entre una cara SELECCIONADA y una NO seleccionada). Se decide ANTES de mutar nada para elegir
// el TIPO de undo del transform de cara: con split va MeshGeoUndo COMPLETO (cambia la topologia
// de render, el snapshot liviano de la capa uv no puede deshacer eso); sin split alcanza el
// liviano (ver la decision documentada en Undo.cpp).
static bool UVSplitNecesario(Mesh* m, const std::vector<char>& fsel) {
    const int oldN = m->vertexSize;
    if (oldN <= 0) return false;
    std::vector<char> usaSel(oldN, 0), usaUnsel(oldN, 0);
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        std::vector<char>& dst = (f < fsel.size() && fsel[f]) ? usaSel : usaUnsel;
        for (size_t c = 0; c < id.size(); c++)
            if (id[c] >= 0 && id[c] < oldN) dst[id[c]] = 1;
    }
    for (int i = 0; i < oldN; i++) if (usaSel[i] && usaUnsel[i]) return true;
    return false;
}

// snapshot de la seleccion + pivot + mouse inicial. modo: 1=mover 2=rotar 3=escalar.
// PIVOT (correccion del dueño): la MEDIANA de la seleccion = el promedio de las posiciones UV
// UNICAS de los verts seleccionados. Antes se promediaban TODOS los render-verts seleccionados:
// las esquinas duplicadas por splits (flat shading / costuras) contaban DOBLE y corrian el pivote.
void UVEditor::IniciarXform(Mesh* m, int modo) {
    if (uvModo != UVModoEdicion) return; // pintura/huesos/pose: el G/R/S de UVs ignora el input
    if (!m || !m->uv) return;
    SincronizarSelDesde3D(m);   // sync: mover lo SELECCIONADO en el 3D (uvSelVert espeja al 3D)
    uvXIdx.clear(); uvXOrig.clear(); uvXPivots.clear(); uvXAxis = 0;
    const int modoUV = ModoUV();
    std::vector<char> fsel;
    if (modoUV == SelFace) {
        // MODO CARA: el transform afecta EXACTAMENTE las esquinas de las caras seleccionadas
        // (con split previo de las esquinas compartidas con caras NO seleccionadas; ver arriba).
        UVCarasSel(this, m, fsel);
        bool hay = false; for (size_t f = 0; f < fsel.size(); f++) if (fsel[f]) { hay = true; break; }
        if (!hay) return;
        // UNDO (pendiente hasta confirmar): el tipo de snapshot se decide ANTES de mutar. Con
        // split de esquinas el liviano no alcanza -> MeshGeoUndo completo PRE-split.
        if (UVSplitNecesario(m, fsel)) UndoUVIniciarCompleto(m); else UndoUVIniciar(m);
        UVSepararCarasSel(m, fsel);
        std::vector<char> visto(m->vertexSize, 0);
        for (size_t f = 0; f < fsel.size(); f++) {
            if (!fsel[f]) continue;
            const std::vector<int>& id = m->faces3d[f].idx;
            for (size_t c = 0; c < id.size(); c++) {
                int k = id[c];
                if (k < 0 || k >= m->vertexSize || visto[k]) continue;
                visto[k] = 1;
                uvXIdx.push_back(k);
                uvXOrig.push_back(m->uv[k*2]); uvXOrig.push_back(m->uv[k*2+1]);
            }
        }
    } else {
        if ((int)m->uvSelVert.size() != m->vertexSize) return;
        for (int i = 0; i < m->vertexSize; i++) if (m->uvSelVert[i]) {
            uvXIdx.push_back(i);
            uvXOrig.push_back(m->uv[i*2]); uvXOrig.push_back(m->uv[i*2+1]);
        }
    }
    if (uvXIdx.empty()) { UndoUVCancelar(); return; } // nada seleccionado -> no arranca (descarta el pendiente)
    // UNDO liviano para verts/bordes: recien aca se sabe que el transform ARRANCA (nada muto todavia).
    // El modo cara ya inicio el suyo arriba (antes del split).
    if (modoUV != SelFace) UndoUVIniciar(m);
    // PIVOT segun el menu (mismo g_transformPivot que el 3D): 3D Cursor = el cursor 2D;
    // Individual (en modo cara) = cada cara alrededor de su centro; el resto = MEDIANA.
    if (g_transformPivot == PivotCursor3D) { uvXPivotU = uvCursorU; uvXPivotV = uvCursorV; }
    else {
        // MEDIANA: promedio de las posiciones UV UNICAS (los splits en el mismo lugar cuentan 1 vez)
        std::set< std::pair<float,float> > unicos;
        double su = 0, sv = 0;
        for (size_t i = 0; i < uvXIdx.size(); i++) {
            std::pair<float,float> p(uvXOrig[i*2], uvXOrig[i*2+1]);
            if (unicos.insert(p).second) { su += p.first; sv += p.second; }
        }
        uvXPivotU = (float)(su / (double)unicos.size());
        uvXPivotV = (float)(sv / (double)unicos.size());
    }
    if (g_transformPivot == PivotIndividual && modoUV == SelFace) {
        // ORIGENES INDIVIDUALES (modo cara): el pivote de cada vert = el promedio de los centros
        // de las caras seleccionadas que lo usan (una esquina en 2 caras sel no se desgarra).
        std::vector<float> acU(m->vertexSize, 0.0f), acV(m->vertexSize, 0.0f);
        std::vector<int>   acN(m->vertexSize, 0);
        for (size_t f = 0; f < fsel.size(); f++) {
            if (!fsel[f]) continue;
            const std::vector<int>& id = m->faces3d[f].idx;
            float fu = 0, fv = 0; int fn = 0;
            for (size_t c = 0; c < id.size(); c++) { int k = id[c];
                if (k >= 0 && k < m->vertexSize) { fu += m->uv[k*2]; fv += m->uv[k*2+1]; fn++; } }
            if (!fn) continue;
            fu /= fn; fv /= fn;
            for (size_t c = 0; c < id.size(); c++) { int k = id[c];
                if (k >= 0 && k < m->vertexSize) { acU[k] += fu; acV[k] += fv; acN[k]++; } }
        }
        uvXPivots.resize(uvXIdx.size() * 2);
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            uvXPivots[i*2]   = acN[k] ? acU[k] / acN[k] : uvXPivotU;
            uvXPivots[i*2+1] = acN[k] ? acV[k] / acN[k] : uvXPivotV;
        }
    }
    UVMouseAUV(this, lastMx, lastMy, uvXStartU, uvXStartV); // mouse actual = arranque (sin salto)
    uvXCurU = uvXStartU; uvXCurV = uvXStartV;               // cursor virtual (wrap-safe)
    uvXValU = uvXValV = uvXValAng = 0.0f; uvXValFac = 1.0f; // valores de la barra de info
    NumInputReset();                                        // sin restos de una entrada anterior
    uvXform = modo;
    // historial MRU de la toolbar del UV: teclado (G/R/S) y toolbar alimentan el mismo historial
    ToolbarMRU(UVToolHist(), (modo == 1) ? TBMove : (modo == 2) ? TBRotate : TBScale);
    g_redraw = true;
}

// aplica el transform en vivo desde la base ORIGINAL (no acumula): deriva delta/angulo/factor
// de la posicion del cursor (virtual) y delega en AplicarXformValores (que aplica el lock).
void UVEditor::AplicarXform(Mesh* m, float curU, float curV) {
    if (!uvXform || uvXIdx.empty() || !m || !m->uv) return;
    float dU = 0.0f, dV = 0.0f, ang = 0.0f, f = 1.0f;
    if (uvXform == 1) {                    // MOVER
        dU = curU - uvXStartU; dV = curV - uvXStartV;
    } else if (uvXform == 2) {             // ROTAR alrededor del pivot
        float a0 = atan2f(uvXStartV - uvXPivotV, uvXStartU - uvXPivotU);
        float a1 = atan2f(curV - uvXPivotV, curU - uvXPivotU);
        ang = a1 - a0;
    } else if (uvXform == 3) {             // ESCALAR desde el pivot
        float d0 = sqrtf((uvXStartU-uvXPivotU)*(uvXStartU-uvXPivotU) + (uvXStartV-uvXPivotV)*(uvXStartV-uvXPivotV));
        float d1 = sqrtf((curU-uvXPivotU)*(curU-uvXPivotU) + (curV-uvXPivotV)*(curV-uvXPivotV));
        f = (d0 > 1e-5f) ? d1 / d0 : 1.0f;
    }
    AplicarXformValores(m, dU, dV, ang, f);
}

// NUCLEO del apply: mover/rotar/escalar respecto al pivot (o al pivote POR VERT si Individual)
// con los VALORES ya resueltos; uvXAxis (X/Y) restringe ADENTRO (en espacio de la vista). Lo
// comparten el mouse (AplicarXform) y la entrada numerica exacta (XformNumValor). El angulo
// va en RADIANES; los valores aplicados quedan en uvXVal* para la barra de INFO.
void UVEditor::AplicarXformValores(Mesh* m, float dU, float dV, float ang, float f) {
    if (!uvXform || uvXIdx.empty() || !m || !m->uv) return;
    const bool porVert = (uvXPivots.size() == uvXIdx.size() * 2);
    if (uvXform == 1) {                    // MOVER
        if (uvXAxis == 1) dV = 0.0f;       // bloqueo X: solo horizontal
        if (uvXAxis == 2) dU = 0.0f;       // bloqueo Y: solo vertical
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            m->uv[k*2]   = uvXOrig[i*2]   + dU;
            m->uv[k*2+1] = uvXOrig[i*2+1] + dV; }
    } else if (uvXform == 2) {             // ROTAR alrededor del pivot (el eje no aplica en 2D)
        float c = cosf(ang), s2 = sinf(ang);
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            float pu = porVert ? uvXPivots[i*2] : uvXPivotU, pv = porVert ? uvXPivots[i*2+1] : uvXPivotV;
            float ru = uvXOrig[i*2] - pu, rv = uvXOrig[i*2+1] - pv;
            m->uv[k*2]   = pu + ru*c - rv*s2;
            m->uv[k*2+1] = pv + ru*s2 + rv*c; }
    } else if (uvXform == 3) {             // ESCALAR desde el pivot (con bloqueo de eje opcional)
        float fx = (uvXAxis == 2) ? 1.0f : f;   // bloqueo Y: X no escala
        float fy = (uvXAxis == 1) ? 1.0f : f;   // bloqueo X: Y no escala
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            float pu = porVert ? uvXPivots[i*2] : uvXPivotU, pv = porVert ? uvXPivots[i*2+1] : uvXPivotV;
            m->uv[k*2]   = pu + (uvXOrig[i*2]   - pu) * fx;
            m->uv[k*2+1] = pv + (uvXOrig[i*2+1] - pv) * fy; }
    }
    uvXValU = dU; uvXValV = dV;                 // valores para la barra de INFO
    uvXValAng = ang * 57.29578f;                // en grados
    uvXValFac = f;
    g_redraw = true;
}

void UVEditor::ConfirmarXform() {          // deja el cambio aplicado
    bool huboXform = (uvXform != 0);
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    // hubo cambio REAL? comparar los uv vivos contra la base del transform (uvXOrig). Un
    // confirmar sin mover (o un split de esquinas sin mover) NO pushea un paso de undo.
    bool cambio = false;
    if (huboXform && m && m->uv)
        for (size_t i = 0; i < uvXIdx.size() && !cambio; i++) { int k = uvXIdx[i];
            if (k >= 0 && k < m->vertexSize &&
                (m->uv[k*2] != uvXOrig[i*2] || m->uv[k*2+1] != uvXOrig[i*2+1])) cambio = true; }
    uvXform = 0; uvXAxis = 0; uvXIdx.clear(); uvXOrig.clear(); uvXPivots.clear(); g_redraw = true;
    NumInputReset(); // la entrada numerica muere con el transform (como el Aceptar del 3D)
    if (!huboXform) return;
    UndoUVConfirmar(cambio); // pushea el pendiente (liviano o completo); sin cambio lo descarta
    // ARMATURE 2D - INVARIANTE uv = f(uv2dRest, pose) (ver Mesh.h): editar los UV a mano REDEFINE
    // el rest. Con la pose en identidad el rest ES el uv; con una pose puesta se invierte el
    // skinning para que la edicion quede donde el usuario la solto y SOBREVIVA al re-evaluar.
    // Antes esto solo limpiaba el rest con pose identidad: con una pose activa (o con el rest
    // capturado y el armature borrado) la edicion se perdia en el proximo Armature2DAplicar.
    if (m) m->Armature2DRestDesdeUV();
    // AUTO KEY de UV: si hay una vertex anim de ESTA malla activa en el timeline y Auto Key
    // esta prendido, editar el mapeo captura un keyframe EN LA CURVA UV (separada de la de
    // vertices: VertexAnimInsertarKeyframeUV guarda SOLO el mapeo de la pose actual).
    extern bool AutoKeyOn; extern int ActiveAnimKind; extern Mesh* ActiveAnimMesh;
    extern void VertexAnimInsertarKeyframeUV();
    if (AutoKeyOn && ActiveAnimKind == 3 && m && (Object*)m == g_editMesh && (Mesh*)ActiveAnimMesh == m)
        VertexAnimInsertarKeyframeUV();
}

void UVEditor::CancelarXform(Mesh* m) {    // restaura los uv originales
    if (uvXform) UndoUVCancelar();         // el pendiente de undo se descarta (no hubo operacion)
    if (uvXform && m && m->uv)
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            if (k >= 0 && k < m->vertexSize) { m->uv[k*2] = uvXOrig[i*2]; m->uv[k*2+1] = uvXOrig[i*2+1]; } }
    uvXform = 0; uvXAxis = 0; uvXIdx.clear(); uvXOrig.clear(); uvXPivots.clear(); g_redraw = true;
    NumInputReset(); // la entrada numerica muere con el transform (como la cruz del 3D)
}

// SELECCION EFECTIVA para los consumidores de AFUERA del editor UV (tarjeta "Transform UV" del
// panel Properties). Ver la decision documentada en el header: si hay un editor UV VIVO con
// seleccion PROPIA (sync OFF) y esa seleccion no esta vacia, manda ESA (es lo que el usuario ve
// marcado en blanco, y respeta las copias por costura de a una); si no, la del 3D expandida por
// posRep (VertsSelPorModo), como siempre.
bool UVVertsSelEfectivos(Mesh* m, std::vector<char>& sv) {
    sv.clear();
    if (!m || m->vertexSize <= 0) return false;
    const int nV = m->vertexSize;
    bool hayEditorPropio = false;
    for (size_t i = 0; i < gUVEditores.size(); i++)
        if (gUVEditores[i] && !gUVEditores[i]->syncSelection) { hayEditorPropio = true; break; }
    if (hayEditorPropio && (int)m->uvSelVert.size() == nV) {
        bool hay = false;
        for (int i = 0; i < nV && !hay; i++) if (m->uvSelVert[i]) hay = true;
        if (hay) {
            sv.assign((size_t)nV, 0);
            for (int i = 0; i < nV; i++) sv[i] = m->uvSelVert[i] ? 1 : 0;
            return true;
        }
    }
    m->VertsSelPorModo(sv);
    if ((int)sv.size() != nV) { sv.clear(); return false; }
    for (int i = 0; i < nV; i++) if (sv[i]) return true;
    return false;
}

// NUCLEO COMPARTIDO (tarjeta "Transform UV" del panel Properties): mueve (dU,dV) los UV de la
// SELECCION EFECTIVA (UVVertsSelEfectivos: la propia del UV si la hay, si no la del 3D), con el
// mismo tratamiento que el confirmar de un G de UVs: undo LIVIANO (UndoUV*, punto 1),
// invalidacion del rest 2D con pose identidad y re-subida del VBO. No hace split de caras (solo
// cambia valores de uv -> el liviano alcanza SIEMPRE aca).
bool UVMoverSeleccionEdit(Mesh* m, float dU, float dV) {
    if (!m || !m->uv || m->vertexSize <= 0) return false;
    if ((Object*)m != g_editMesh) return false;              // solo en Edit Mode de esta malla
    if (dU == 0.0f && dV == 0.0f) return false;              // no-op: ni undo ni redraw
    std::vector<char> sv;
    if (!UVVertsSelEfectivos(m, sv)) return false;
    UndoUVIniciar(m);                                        // snapshot ANTES (pendiente)
    for (int i = 0; i < m->vertexSize; i++) if (sv[i]) { m->uv[i*2] += dU; m->uv[i*2+1] += dV; }
    UndoUVConfirmar(true);                                   // un paso de undo por edicion de campo
    // igual que ConfirmarXform: editar los UV a mano REDEFINE el rest del skinning 2D
    // (invariante uv = f(uv2dRest, pose), ver Mesh.h)
    m->Armature2DRestDesdeUV();
    m->skinGeomVersion++;                                    // re-subir el VBO de uv
    g_redraw = true;
    return true;
}

// ===== hooks del TRANSFORM UI COMPARTIDO (ver TransformUI.h): el UV editor los implementa
// para que la barra de info, la entrada numerica, el warp del cursor y el tilde/cruz/X/Y de
// la toolbar sean EL MISMO mecanismo que en el 3D y el Editor 2D, sin duplicar codigo. =====

// hay un G/R/S modal propio? (de UVs, o de huesos 2D arrancado EN este editor)
bool UVEditor::XformEnCurso() const {
    return uvXform != 0 || (Bone2DXformActivo() && gB2D.uv == this);
}

// valor EXACTO tipeado (NumInput): mover = v en unidades UV (1.0 = un tile completo: con
// textura repetida queda "igual pero corrido", clave para animacion UV); el lock X/Y filtra.
// Sin lock aplica en los dos ejes (como el translate libre del 3D, que aplica en los 3).
// Rotar = v en grados; escalar = factor v. Reusa los nucleos *AplicarValores (cero loops nuevos).
void UVEditor::XformNumValor(float v) {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m) return;
    const float DEG = 3.14159265358979f / 180.0f;
    if (uvXform) {
        if (uvXform == 1)      AplicarXformValores(m, v, v, 0.0f, 1.0f);
        else if (uvXform == 2) AplicarXformValores(m, 0.0f, 0.0f, v * DEG, 1.0f);
        else                   AplicarXformValores(m, 0.0f, 0.0f, 0.0f, v);
        return;
    }
    // HUESOS 2D: MISMA semantica (el lock X/Y lo aplica B2DAplicarValores adentro -> con un eje
    // bloqueado el delta queda EXACTO en ese eje y CERO en el otro; sin lock aplica en los dos).
    if (Bone2DXformActivo() && gB2D.uv == this) {
        if (gB2D.modo == 1)      B2DAplicarValores(m, v, v, 0.0f, 1.0f);
        else if (gB2D.modo == 2) B2DAplicarValores(m, 0.0f, 0.0f, v, 1.0f); // (grados)
        else                     B2DAplicarValores(m, 0.0f, 0.0f, 0.0f, v);
    }
}

// confirmar (Enter / click / tilde): una expresion tipeada INVALIDA no confirma (la barra
// ya muestra "= ?" como feedback). Es el funnel comun de TODOS los caminos de confirmar.
void UVEditor::XformConfirmar() {
    if (!TransformUIPuedeConfirmar()) { g_redraw = true; return; }
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (Bone2DXformActivo() && gB2D.uv == this) { if (m) Bone2DXformConfirm(m); return; }
    if (uvXform) ConfirmarXform();
}

void UVEditor::XformCancelar() {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (Bone2DXformActivo() && gB2D.uv == this) { if (m) Bone2DXformCancel(m); NumInputReset(); return; }
    if (uvXform && m) CancelarXform(m);
}

// toggle del BLOQUEO de eje (1=X 2=Y), en espacio de la vista: teclas X/Y y botones de la
// toolbar entran por aca (UN solo camino) TANTO para el G/R/S de UVs como para el transform
// modal de HUESOS 2D. Re-aplica al instante con el eje nuevo (o con el valor tipeado).
void UVEditor::XformToggleEje(int eje) {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (uvXform) {
        uvXAxis = (eje == 1) ? ((uvXAxis == 1) ? 0 : 1) : ((uvXAxis == 2) ? 0 : 2);
        if (m) {
            float v;
            if (NumInputActivo() && NumInputValor(v)) XformNumValor(v);  // el valor tipeado, al eje nuevo
            else AplicarXform(m, uvXCurU, uvXCurV);                      // re-aplica YA (cursor virtual)
        }
        g_redraw = true;
        return;
    }
    // HUESOS 2D: mismo comportamiento (mover y escalar aceptan X/Y; rotar no tiene eje en 2D)
    if (!(Bone2DXformActivo() && gB2D.uv == this)) return;
    if (gB2D.modo == 2) return;                                          // rotacion 2D: sin eje
    gB2D.eje = (eje == 1) ? ((gB2D.eje == 1) ? 0 : 1) : ((gB2D.eje == 2) ? 0 : 2);
    if (m) {
        float v;
        if (NumInputActivo() && NumInputValor(v)) XformNumValor(v);      // el valor tipeado, al eje nuevo
        else Bone2DXformDelta(m, gB2D.curU, gB2D.curV);                  // re-aplica YA (cursor virtual)
    }
    g_redraw = true;
}

// texto de la barra de INFO (formato compartido con el 3D y el 2D; valores en unidades UV)
std::string UVEditor::XformTextoBarra() {
    if (uvXform) return TransformUITexto2D(uvXform, uvXValU, uvXValV, uvXValAng, uvXValFac,
                                           uvXAxis, "X", "Y", "");
    if (Bone2DXformActivo() && gB2D.uv == this)
        return TransformUITexto2D(gB2D.modo, gB2D.valU, gB2D.valV, gB2D.valAng, gB2D.valFac,
                                  gB2D.eje, "X", "Y", "");
    return std::string();
}

// paneo de la vista UV (compartido PC/Symbian). dx>0 mueve el contenido a la derecha (revela la izquierda).
void UVEditor::Panear(float dx, float dy) { panX += dx; panY += dy; g_redraw = true; }

// TOUCH: 1 dedo sobre el CONTENIDO = panear la vista UV. La barra la maneja el gesto lockeado.
bool UVEditor::event_finger_scroll(int px, int py, int dx, int dy){
    Panear((float)dx, (float)dy);
    return true;
}
// TOUCH: 2 dedos = zoom (pinch) + paneo del centroide.
void UVEditor::event_finger_gesture(float zoomDelta, float panDx, float panDy){
    if (zoomDelta > 1.0f)       ZoomCentro(1);
    else if (zoomDelta < -1.0f) ZoomCentro(-1);
    if (panDx != 0.0f || panDy != 0.0f) Panear(panDx, panDy);
}

// zoom CENTRADO en el viewport (sin cursor; para el teclado 0+arriba/abajo de Symbian). Es el zoom de la rueda
// con el "cursor" en el centro: ahi (curX-uvCx) = -panX -> panX queda *= f (el centro de pantalla no se mueve).
void UVEditor::ZoomCentro(int dir) {
    float f = (dir > 0) ? 1.05f : (1.0f / 1.05f); // suave por frame (la rueda usa 1.1 por notch)
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    f = nz / zoom;                 // factor real tras el clamp
    zoom = nz; panX *= f; panY *= f;
    g_redraw = true;
}

#ifndef W3D_SYMBIAN
// teclas del editor UV: flechas = paneo (cualquier modo); G/R/S inician mover/rotar/escalar; ESC/ENTER transform.
void UVEditor::event_key_down(int tecla, bool repeticion) {
    if (PopUpActive) return;                    // un popup modal abierto (file browser) tiene prioridad
    const int k = tecla;
    // TAB = entrar/salir de la EDICION DE HUESOS 2D (ciclo documentado en UVEditor.h). UNICO lugar
    // que togglea: el Tab global (Object<->Edit Mode del 3D) no corre porque controles.cpp consulta
    // UVEditorTomaTab ANTES y, si el UV se la queda, deja que la tecla llegue hasta aca por el
    // ruteo normal de teclas al viewport activo (foco por hover).
    if (k == W3dK_TAB) { TabToggleHuesos(); return; }
    // BOX SELECT armado o en curso: ESC lo cancela ANTES que cualquier otra cosa (mismo reflejo
    // que cancelar un transform).
    if ((uvBoxArmado || uvBoxSel) && k == W3dK_ESCAPE) { BoxSelectCancelar(); return; }
    // NUMPAD . = ENCUADRAR la seleccion. MISMO atajo y mismo significado que en el viewport 3D
    // (Frame Selected), el Editor 2D y el Timeline.
    if (k == W3dK_KP_PERIOD) { EncuadrarUV(false); return; }
    // PANEO de la vista con las flechas (en cualquier modo): la flecha revela ese lado
    const float pp = (float)GlobalScale * 16.0f;
    if (k == W3dK_LEFT)  { Panear(+pp, 0); return; }
    if (k == W3dK_RIGHT) { Panear(-pp, 0); return; }
    if (k == W3dK_UP)    { Panear(0, +pp); return; }
    if (k == W3dK_DOWN)  { Panear(0, -pp); return; }
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || (Object*)m != g_editMesh) return; // el resto solo en Edit Mode sobre esta malla
    // B = BOX SELECT (mismo atajo que Blender y que el resto de la app a futuro): ARMA el gesto y
    // el proximo arrastre con el boton izquierdo dibuja el rectangulo. Solo en los modos con
    // seleccion (en pintura se pinta y en objeto el click elige la entidad, no hay caja que hacer).
    if (k == W3dK_B && !XformEnCurso() &&
        (uvModo == UVModoEdicion || uvModo == UVModoHuesos || uvModo == UVModoPose)) {
        BoxSelectArmar();
        return;
    }
    // ===== ARMATURE 2D DEL MESH: atajos en los modos Huesos/Pose (mismas teclas que el 3D) =====
    if (uvModo == UVModoHuesos || uvModo == UVModoPose) {
        const Uint8* ks = SDL_GetKeyboardState(NULL);
        bool shift = ks && (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]);
        bool alt   = ks && (ks[SDL_SCANCODE_LALT]   || ks[SDL_SCANCODE_RALT]);
        if (Bone2DXformActivo()) {      // transform de huesos en curso: Enter confirma / Esc cancela
            // (via los hooks compartidos: mismo funnel que el tilde/cruz de la toolbar, con
            // el gate de la entrada numerica invalida y el reset del NumInput)
            if (k == W3dK_ESCAPE) XformCancelar();
            else if (k == W3dK_RETURN || k == W3dK_KP_ENTER) XformConfirmar();
            // BLOQUEO DE EJE: mismas teclas y mismo hook que el transform de UVs (en rotar es
            // un no-op). Ojo: X aca NO borra huesos (el borrado solo llega fuera del transform).
            else if (k == W3dK_X || k == W3dK_Y) XformToggleEje((k == W3dK_X) ? 1 : 2);
            return;
        }
        bool ctrl  = ks && (ks[SDL_SCANCODE_LCTRL]  || ks[SDL_SCANCODE_RCTRL]);
        // A = seleccionar TODOS los huesos / Alt+A = deseleccionar todos (como el 3D). Con la
        // seleccion completa, las ops (G/R/S, X, Shift+D, Ctrl+P) operan sobre todo.
        if (k == W3dK_A) { Bone2DSeleccionarTodos(m, !alt); return; }
        // Ctrl+I = invertir / L = Select Linked (la CADENA conectada). Mismos atajos que el menu
        // Select de la barra y que el del viewport 3D. Ctrl+I va ANTES que la I de Insert Keyframe.
        if (k == W3dK_I && ctrl) { Bone2DInvertirSeleccion(m); return; }
        if (k == W3dK_L) { Bone2DSeleccionarVinculado(m, shift); return; }
        if (k == W3dK_G || k == W3dK_R || k == W3dK_S) {
            // G/R/S: en POSE edita poseT/poseRot/poseS (los UV pesados siguen al hueso en vivo);
            // en HUESOS mueve/rota/escala el REST (head/tail) de los seleccionados
            Bone2DXformStart(m, (k == W3dK_G) ? 1 : (k == W3dK_R) ? 2 : 3);
            return;
        }
        if (uvModo == UVModoPose) {
            // I = Insert Keyframe de la POSE 2D: abre el MENU DE CANALES (Todos / Localizacion /
            // Rotacion / Escala) y keyframea posU/posV/rot/escalaX/escalaY de los huesos elegidos
            // en el CLIP del armature 2D (curvas propias por hueso; ya no se hornea el array uv
            // entero en la capa uv de la vertex anim). Shift+I = todos los canales sin menu.
            if (k == W3dK_I) {
                if (shift) InsertarKeyframeContexto(KfCanalTodos, true);
                else       LayoutMenuInsertKeyframe(lastMx, lastMy, true);
            }
            return;
        }
        // EDICION de huesos: E extruye (tail agarrado), Shift+D duplica, X/Supr borra. Ctrl+P /
        // Alt+P (emparentar / menu Disconnect-Clear) entran por el atajo GLOBAL (controles.cpp ->
        // LayoutParentHuesos); aca queda el fallback por si el evento llega directo al viewport.
        if (k == W3dK_E) {
            int nb = Bone2DExtruir(m);
            if (nb >= 0) Bone2DDragEnd(m, nb, 2, false); // el tail nuevo queda agarrado al mouse
        } else if (k == W3dK_D && shift) {
            if (Bone2DDuplicar(m) >= 0) Bone2DXformStart(m, 1); // lo duplicado queda agarrado
        } else if (k == W3dK_X || k == W3dK_DELETE || k == W3dK_BACKSPACE) {
            Bone2DBorrar(m);
        } else if (k == W3dK_P && alt) {
            LayoutMenuBoneAltP2D(m, lastMx, lastMy); // menu Disconnect Bone / Clear Parent
        }
        return;
    }
    if (uvXform) {                          // dentro de un transform: confirmar/cancelar/eje
        // todo via los hooks compartidos (TransformUI): mismo camino que la toolbar y el
        // teclado numerico. El Enter con una expresion invalida NO confirma (feedback "?").
        if (k == W3dK_ESCAPE) { XformCancelar(); return; }
        if (k == W3dK_RETURN || k == W3dK_KP_ENTER) { XformConfirmar(); return; }
        // BLOQUEO DE EJE (X/Y): restringe al eje EN ESPACIO DE LA VISTA (aca no hay local/global).
        // Tocar de nuevo el mismo eje lo libera; el otro eje cambia el bloqueo (como el 3D).
        if (k == W3dK_X || k == W3dK_Y) XformToggleEje((k == W3dK_X) ? 1 : 2);
        return;
    }
    // A / Alt+A en EDICION de UVs: seleccionar todo / deseleccionar todo (coherencia con los
    // modos de huesos y con el 3D). En SYNC la seleccion del UV es un espejo de la del 3D ->
    // se toca la del 3D y se re-deriva; fuera de sync se escribe uvSelVert (la propia).
    if (k == W3dK_A && uvModo == UVModoEdicion) {
        const Uint8* ksA = SDL_GetKeyboardState(NULL);
        bool altA = ksA && (ksA[SDL_SCANCODE_LALT] || ksA[SDL_SCANCODE_RALT]);
        SeleccionarTodoUV(m, !altA);
        return;
    }
    // Ctrl+I = INVERTIR y L = SELECT LINKED (la isla UV), los mismos atajos que el menu Select de
    // la barra y que el del viewport 3D. La L arranca de lo que hay BAJO EL CURSOR (como la L del
    // 3D, que agarra la isla bajo el mouse); shift SUMA la isla a la seleccion en vez de reemplazar.
    if (uvModo == UVModoEdicion && (k == W3dK_I || k == W3dK_L)) {
        const Uint8* ksS = SDL_GetKeyboardState(NULL);
        bool ctrlS  = ksS && (ksS[SDL_SCANCODE_LCTRL]  || ksS[SDL_SCANCODE_RCTRL]);
        bool shiftS = ksS && (ksS[SDL_SCANCODE_LSHIFT] || ksS[SDL_SCANCODE_RSHIFT]);
        if (k == W3dK_I && ctrlS) { InvertirSeleccionUV(m); return; }   // (I sin ctrl = Insert Keyframe, abajo)
        if (k == W3dK_L) { UVSeleccionarVinculado(m, VertBajoCursorUV(m), shiftS); return; }
    }
    // (en modo PINTURA DE PESOS IniciarXform es un no-op: el G/R/S ignora el input ahi)
    if (k == W3dK_G) IniciarXform(m, 1);    // G = mover  (Symbian 1)
    else if (k == W3dK_R) IniciarXform(m, 2); // R = rotar  (Symbian 2)
    else if (k == W3dK_S) IniciarXform(m, 3); // S = escalar (Symbian 3)
    else if (k == W3dK_I && uvModo == UVModoEdicion) {
        // I = Insert Keyframe UV en EDICION de UVs (pedido del dueno; en Pose 2D ya existia,
        // bloque de arriba). MISMO camino que el menu Animation de la barra: valida la vertex
        // anim activa (kind 3) y notifica si no hay. Llega aca por el foco de teclado por
        // HOVER (controles.cpp rutea al viewport bajo el mouse; el IDE y las cajas de texto
        // capturan antes). Durante un transform NO llega (el bloque uvXform retorna arriba).
        extern void VertexAnimInsertarKeyframeUV();
        VertexAnimInsertarKeyframeUV();
    }
}

void UVEditor::event_mouse_wheel(float dy, int mx, int my) {
    if (PopUpActive) return;                    // dejar que el popup (file browser) maneje la rueda
    {
      if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; } // sobre la barra -> scroll horizontal
    if (OnToolbar(mx, my)) { ToolbarScrollBy((int)(dy * 40)); return; } // toolbar de abajo: scroll horizontal
    float f = (dy > 0) ? 1.1f : (1.0f / 1.1f);
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    f = nz / zoom;                         // factor real tras el clamp
    // zoom HACIA EL CURSOR: el punto UV bajo el mouse queda fijo
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    float uvCx = width * 0.5f + panX;
    float uvCy = top + ch * 0.5f + panY;
    float curX = (float)(lastMx - x);      // cursor en coords LOCALES del viewport
    float curY = (float)(lastMy - y);
    panX += (curX - uvCx) * (1.0f - f);
    panY += (curY - uvCy) * (1.0f - f);
    zoom = nz;
    g_redraw = true;
}

// al soltar el mouse: liberar el "click sostenido" para que viewPortActive vuelva a seguir
// al mouse (sino el borde verde queda clavado aca y no se pueden mover los splitters).
void UVEditor::mouse_button_up(int boton) {
    (void)boton;
    // BOX SELECT: el soltar CIERRA el rectangulo y aplica la seleccion
    if (uvBoxSel) {
        BoxSelectSoltar();
        ViewPortClickDown = false;   // (imprescindible: sino viewPortActive queda congelado)
        return;
    }
    if (gUVPintando == this) { gUVPintando = NULL; WeightPaintTrazoFin(); } // fin del trazo -> commit del undo
    // ARMATURE 2D del mesh: los drags arrancados por CLICK terminan al SOLTAR
    if (Bone2DXformActivo() && gB2D.porClick && gB2D.uv == this) {
        Mesh* mb = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (mb) Bone2DXformConfirm(mb);
    }
    ViewPortClickDown = false;
    g_redraw = true;
}
#endif

// click izquierdo en el editor UV = seleccionar el sub-elemento (vertex/edge/face) bajo el cursor.
void UVEditor::button_left() {
#ifdef W3D_SYMBIAN
    return; // Symbian: el pick por OK/lapiz se cablea aparte (llama directo a PickUV)
#else
    if (PopUpActive) return;                    // un popup modal abierto (file browser) tiene prioridad
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || !m->uv || (Object*)m != g_editMesh) return; // solo en Edit Mode sobre esta malla
    // ===== BOX SELECT ARMADO (tecla B): este press ABRE el rectangulo; el arrastre lo agranda y
    // el soltar (mouse_button_up) lo aplica. Consume el click: no pickea nada. =====
    if (uvBoxArmado) {
        const Uint8* ksB = SDL_GetKeyboardState(NULL);
        uvBoxAdd = ksB && (ksB[SDL_SCANCODE_LSHIFT] || ksB[SDL_SCANCODE_RSHIFT]); // shift = sumar
        uvBoxArmado = false; uvBoxSel = true;
        uvBoxX0 = uvBoxX1 = lastMx - x; uvBoxY0 = uvBoxY1 = lastMy - y;
        g_redraw = true;
        return;
    }
    // ===== MODO OBJETO: el click ELIGE el objeto del UV (armature 2D bajo el cursor o, si no
    // pegaste a ningun hueso, la geometria). No edita nada: para eso esta el Tab. =====
    if (uvModo == UVModoObjeto) {
        PickObjetoUV(m, (float)(lastMx - x), (float)(lastMy - y));
        return;
    }
    // ===== ARMATURE 2D DEL MESH (modos Huesos/Pose): click = confirmar transform en curso,
    // o pickear hueso (+ arrancar el drag de la punta en Huesos / el traslado en Pose) =====
    if (uvModo == UVModoHuesos || uvModo == UVModoPose) {
        if (Bone2DXformActivo()) { if (!gB2D.porClick) XformConfirmar(); return; } // modal (G/E): click confirma (con gate)
        const Uint8* ks = SDL_GetKeyboardState(NULL);
        bool add = ks && (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]);
        bool pose = (uvModo == UVModoPose);
        int mask = 3;
        int b = Bone2DPick(m, (float)(lastMx - x), (float)(lastMy - y), pose, &mask);
        if (!add) Bone2DSelLimpiar(m);
        if (b >= 0) {
            // seleccion por PUNTAS como el Edit del 3D (BoneEditClick3D): click cerca de una punta
            // selecciona SOLO esa punta (con su soldada); en el cuerpo, el hueso ENTERO. Shift
            // sobre algo ya seleccionado lo saca.
            bool ya = (mask == 3) ? m->Arm2DHuesos()[b].select
                    : (mask == 1) ? m->Arm2DHuesos()[b].selHead : m->Arm2DHuesos()[b].selTail;
            bool nuevo = !(add && ya);
            if (mask == 3) Bone2DSelHueso(m, b, nuevo);
            else           Bone2DSelPunta(m, b, mask, nuevo);
            m->Arm2DBoneActivo() = nuevo ? b : -1;
            if (nuevo) {
                if (pose) Bone2DXformStart(m, 1);            // POSE: arrastrar = trasladar (G/R/S p/rotar/escalar)
                else      Bone2DDragSeleccion(m, true);      // HUESOS: drag de LO SELECCIONADO (con soldadura)
                if (Bone2DXformActivo()) gB2D.porClick = true; // arranco por click: suelta = confirma
            }
        } else if (!add) m->Arm2DBoneActivo() = -1;
        g_redraw = true;
        return;
    }
    // PINTURA DE PESOS: el click arranca el TRAZO (snapshot de undo + grupo automatico si no
    // hay) y pinta la 1ra pasada. El drag sigue en event_mouse_motion; el commit al soltar.
    if (uvModo == UVModoPesos) {
        // CTRL+CLICK SOBRE UN HUESO 2D = ese hueso pasa a ser el GRUPO ACTIVO que se pinta.
        // Es el gesto de Blender (en Weight Paint, Ctrl+click selecciona el hueso) y cierra el
        // flujo de rigging 2D: hasta ahora el UNICO camino para cambiar de grupo era el
        // desplegable "Group" de la toolbar, aunque los huesos SE DIBUJAN mientras se pinta.
        // Sale casi gratis porque el binding hueso <-> UV group es POR NOMBRE.
        // Se pide Ctrl a proposito: con el click pelado, los huesos (radio de pick ~14px) se
        // comerian los trazos justo donde mas se pinta.
        {
            const Uint8* ksP = SDL_GetKeyboardState(NULL);
            const bool ctrlP = ksP && (ksP[SDL_SCANCODE_LCTRL] || ksP[SDL_SCANCODE_RCTRL]);
            if (ctrlP && Bone2DElegirGrupoPorClick(m, (float)(lastMx - x), (float)(lastMy - y)))
                return;   // el ctrl+click elige grupo: NO pinta
        }
        if (WeightPaintTrazoIniciarUV(m) >= 0) { gUVPintando = this; UVPintarPesos(this, m); }
        return;
    }
    if (uvXform) { XformConfirmar(); return; } // si hay transform en curso, el click lo CONFIRMA (con gate)
    const Uint8* ks = SDL_GetKeyboardState(NULL);
    bool add = ks && (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]); // shift = sumar/toggle
    PickUV(m, lastMx - x, lastMy - y, add); // lastMx/My son GLOBALES; - origen del viewport = local
    g_redraw = true;
#endif
}

// click derecho: cancela el transform en curso; si no hay, COLOCA el cursor 2D en el mouse.
void UVEditor::button_right() {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    // ARMATURE 2D del mesh: el derecho cancela el transform de huesos en curso
    if (Bone2DXformActivo() && gB2D.uv == this) { XformCancelar(); g_redraw = true; return; }
    if (uvXform) { XformCancelar(); return; }
    UVMouseAUV(this, lastMx, lastMy, uvCursorU, uvCursorV); // mismo mapeo (con aspecto) que el render
    g_redraw = true;
}

// --- SNAP (menu Snap del UV editor) ---
void UVEditor::SnapCursorToSel() {         // cursor 2D -> centro de la seleccion
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || !m->uv) return;
    const int nV = m->vertexSize;
    if ((int)m->uvSelVert.size() != nV) return;
    double su = 0, sv = 0; int n = 0;
    for (int i = 0; i < nV; i++) if (m->uvSelVert[i]) { su += m->uv[i*2]; sv += m->uv[i*2+1]; n++; }
    if (n > 0) { uvCursorU = (float)(su/n); uvCursorV = (float)(sv/n); g_redraw = true; }
}

// mueve la seleccion para que su centro caiga en el cursor. Escribe uv[] -> va con SU paso de undo
// (UndoUVIniciar/Confirmar, igual que el confirmar de un G de UVs y que UVMoverSeleccionEdit). Sin
// eso el Ctrl+Z de despues deshacia la operacion ANTERIOR y el snap quedaba pegado.
void UVEditor::SnapSelToCursor() {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    if (!m || !m->uv) return;
    std::vector<char> sel;                 // seleccion EFECTIVA (la propia del UV, o la del 3D en sync)
    if (!UVVertsSelEfectivos(m, sel)) return;
    const int nV = m->vertexSize;
    double su = 0, sv = 0; int n = 0;
    for (int i = 0; i < nV; i++) if (sel[i]) { su += m->uv[i*2]; sv += m->uv[i*2+1]; n++; }
    if (n == 0) return;
    float dU = uvCursorU - (float)(su/n), dV = uvCursorV - (float)(sv/n);
    if (dU == 0.0f && dV == 0.0f) return;  // ya esta en el cursor: ni undo ni redraw
    UndoUVIniciar(m);                      // snapshot ANTES de escribir (liviano: no hay split de caras)
    for (int i = 0; i < nV; i++) if (sel[i]) { m->uv[i*2] += dU; m->uv[i*2+1] += dV; }
    UndoUVConfirmar(true);                 // un paso de undo por snap
    m->Armature2DRestDesdeUV(); // escribio uv[] a mano -> re-derivar el rest (invariante, ver Mesh.h)
    m->skinGeomVersion++;       // re-subir el VBO de uv
    g_redraw = true;
}

void UVEditor::CursorToCenter() { uvCursorU = 0.5f; uvCursorV = 0.5f; g_redraw = true; }

// =====================================================================================
//  ENCUADRAR (View > Frame Selected / Frame All, Numpad .)
//  El UV era el UNICO viewport sin encuadrar: el 3D, el Editor 2D y el Timeline lo tienen
//  todos con el mismo label y el mismo atajo. Se calcula el bbox en espacio UV y despues
//  zoom+pan para que llene el ~80% del viewport (la misma proporcion que usa el zoom base
//  de ParamsUV y que Editor2DEncuadrarSeleccion).
// =====================================================================================
bool UVEditor::BBoxUV(Mesh* m, bool todo, float& u0, float& v0, float& u1, float& v1) const {
    if (!m) return false;
    u0 = v0 = 1e9f; u1 = v1 = -1e9f;
    bool hay = false;
    const bool huesos = (uvModo == UVModoHuesos || uvModo == UVModoPose);
    // ---- HUESOS: encuadra el rig (los seleccionados; con 'todo' o sin seleccion, TODOS los
    // huesos del armature ACTIVO). En rest: el bbox de un rig posado se movia con la pose y
    // encuadrar dos veces daba resultados distintos.
    if (huesos && m->TieneArm2D()) {
        const std::vector<W3dBone2D>& H = m->Arm2DHuesos();
        for (size_t i = 0; i < H.size(); i++) {
            if (!todo && !H[i].select) continue;
            const float pu[2] = { H[i].headU, H[i].tailU };
            const float pv[2] = { H[i].headV, H[i].tailV };
            for (int k = 0; k < 2; k++) {
                if (pu[k] < u0) u0 = pu[k];  if (pu[k] > u1) u1 = pu[k];
                if (pv[k] < v0) v0 = pv[k];  if (pv[k] > v1) v1 = pv[k];
            }
            hay = true;
        }
        if (hay) return true;
        if (!todo) return BBoxUV(m, true, u0, v0, u1, v1); // sin seleccion -> todo el rig
        return false;
    }
    // ---- UVs: SIEMPRE dentro del filtro del 3D (lo que no se ve no se encuadra, misma regla
    // que el pick, el All y el Invert del UV).
    if (!m->uv || m->faces3d.empty()) return false;
    const int nV = m->vertexSize;
    std::vector<char> fsel; UVCarasFiltro3D(m, fsel);
    const bool haySel = ((int)m->uvSelVert.size() == nV);
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const bool visible = (f < fsel.size() && fsel[f]);
        if (!syncSelection && !visible) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c]; if (k < 0 || k >= nV) continue;
            if (!todo && !(haySel && m->uvSelVert[k])) continue;
            float uu = m->uv[k*2], vv = m->uv[k*2+1];
            if (uu < u0) u0 = uu;  if (uu > u1) u1 = uu;
            if (vv < v0) v0 = vv;  if (vv > v1) v1 = vv;
            hay = true;
        }
    }
    if (!hay && !todo) return BBoxUV(m, true, u0, v0, u1, v1); // sin seleccion -> encuadra todo
    return hay;
}

bool UVEditor::EncuadrarUV(bool todo) {
    Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
    float u0, v0, u1, v1;
    if (!BBoxUV(m, todo, u0, v0, u1, v1)) return false;
    float cx, cy, s; ParamsUV(cx, cy, s);   // (de paso deja g_uvAspU/V con el aspecto de la textura)
    const float au = (g_uvAspU > 1e-6f) ? g_uvAspU : 1.0f;
    const float av = (g_uvAspV > 1e-6f) ? g_uvAspV : 1.0f;
    const int top = BarTopOffset();
    float ch = (float)(height - top); if (ch < 1.0f) ch = 1.0f;
    // tamano del bbox en "unidades de s" (ya con el aspecto). Un solo vertice (extent 0) no puede
    // dar zoom infinito: se le pone un piso chico para que quede un acercamiento razonable.
    float ew = (u1 - u0) * au; if (ew < 0.02f) ew = 0.02f;
    float eh = (v1 - v0) * av; if (eh < 0.02f) eh = 0.02f;
    float sNew = ((float)width * 0.8f) / ew;
    float sy   = (ch * 0.8f) / eh;
    if (sy < sNew) sNew = sy;
    float baseSize = (float)(width < (int)ch ? width : (int)ch) * 0.8f;
    if (baseSize < 1.0f) baseSize = 1.0f;
    float z = sNew / baseSize;
    if (z < 0.05f) z = 0.05f;
    if (z > 50.0f) z = 50.0f;
    zoom = z;
    sNew = baseSize * z; if (sNew < 1.0f) sNew = 1.0f;
    // centrar: UVtoScreen(uc) = width*0.5 -> panX = -(uc-0.5)*s*aspecto (idem en Y con el centro
    // del area de contenido, que es donde ParamsUV pone cy).
    panX = -((u0 + u1) * 0.5f - 0.5f) * sNew * au;
    panY = -((v0 + v1) * 0.5f - 0.5f) * sNew * av;
    g_redraw = true;
    return true;
}

// =====================================================================================
//  BOX SELECT (tecla B + arrastre con el izquierdo, como Blender)
//  Era LA ausencia mas cara del editor: media isla se seleccionaba vertice por vertice.
//  El rectangulo es en PIXELES de pantalla y se prueba contra la MISMA proyeccion que usa
//  el render/pick (UVtoScreen), asi que lo que se ve encerrado es exactamente lo que entra.
//  Respeta las mismas reglas que el pick: el filtro del 3D (fuera de sync solo se puede tocar
//  lo visible) y el sub-modo Vertex/Edge/Face. En Edit Bones / Pose opera los HUESOS 2D.
// =====================================================================================
static int UVRenderVertAEdit(Mesh* m, int gi); // (definida mas abajo, junto al pick)
void UVEditor::BoxSelectArmar() { uvBoxArmado = true; uvBoxSel = false; g_redraw = true; }
void UVEditor::BoxSelectCancelar() { uvBoxArmado = false; uvBoxSel = false; g_redraw = true; }

void UVEditor::BoxSelectSoltar() {
    if (!uvBoxSel) return;
    uvBoxSel = false;
    int dx = uvBoxX1 - uvBoxX0, dy = uvBoxY1 - uvBoxY0;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx >= 3 && dy >= 3) {   // click sin arrastrar = cancelar (no borra la seleccion)
        Mesh* mb = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (mb && (Object*)mb == g_editMesh) BoxSelectAplicar(mb, uvBoxAdd);
    }
    g_redraw = true;
}

int UVEditor::BoxSelectAplicar(Mesh* m, bool add) {
    if (!m) return 0;
    float rx0 = (float)(uvBoxX0 < uvBoxX1 ? uvBoxX0 : uvBoxX1);
    float rx1 = (float)(uvBoxX0 < uvBoxX1 ? uvBoxX1 : uvBoxX0);
    float ry0 = (float)(uvBoxY0 < uvBoxY1 ? uvBoxY0 : uvBoxY1);
    float ry1 = (float)(uvBoxY0 < uvBoxY1 ? uvBoxY1 : uvBoxY0);
    float cx, cy, s; ParamsUV(cx, cy, s);
    int tocados = 0;
    // ---------- HUESOS 2D (Edit Bones / Pose) ----------
    if ((uvModo == UVModoHuesos || uvModo == UVModoPose) && m->TieneArm2D()) {
        std::vector<W3dBone2D>& H = m->Arm2DHuesos();
        const bool pose = (uvModo == UVModoPose);
        std::vector<float> M;
        if (pose) m->Armature2DMatrices(M);
        if (!add) Bone2DSelLimpiar(m);
        for (size_t i = 0; i < H.size(); i++) {
            float hu, hv, tu, tv;
            Bone2DExtremos(m, M, i, pose, hu, hv, tu, tv);
            float ax, ay, bx, by;
            UVtoScreen(hu, hv, cx, cy, s, ax, ay);
            UVtoScreen(tu, tv, cx, cy, s, bx, by);
            const bool inA = (ax >= rx0 && ax <= rx1 && ay >= ry0 && ay <= ry1);
            const bool inB = (bx >= rx0 && bx <= rx1 && by >= ry0 && by <= ry1);
            if (!inA && !inB) continue;
            // POSE selecciona el hueso ENTERO (ahi no hay puntas que editar); en EDICION cada
            // punta encerrada se selecciona sola, igual que el click por punta.
            if (pose || (inA && inB)) { Bone2DSelHueso(m, (int)i, true); }
            else if (inA)             { Bone2DSelPunta(m, (int)i, 1, true); }
            else                      { Bone2DSelPunta(m, (int)i, 2, true); }
            m->Arm2DBoneActivo() = (int)i;
            tocados++;
        }
        g_redraw = true;
        return tocados;
    }
    // ---------- UVs (modo edicion) ----------
    if (!m->uv || m->faces3d.empty()) return 0;
    const int nV = m->vertexSize;
    if (nV <= 0) return 0;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign(nV, 0);
    std::vector<char> fsel; UVCarasFiltro3D(m, fsel);
    const int modoUV = ModoUV();
    std::vector<char> nueva(nV, 0);
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const bool visible = (f < fsel.size() && fsel[f]);
        if (!syncSelection && !visible) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        const int nc = (int)id.size();
        if (nc < 2) continue;
        if (modoUV == SelFace) {
            // CARA: entra si su CENTRO cae adentro (mismo criterio que el punto que se dibuja
            // y que el pick por cara) -> se marcan todos sus corners.
            float fu = 0, fv = 0; int fn = 0; bool ok = true;
            for (int c = 0; c < nc; c++) { int k = id[c];
                if (k < 0 || k >= nV) { ok = false; break; }
                fu += m->uv[k*2]; fv += m->uv[k*2+1]; fn++; }
            if (!ok || !fn) continue;
            float px, py; UVtoScreen(fu/fn, fv/fn, cx, cy, s, px, py);
            if (px < rx0 || px > rx1 || py < ry0 || py > ry1) continue;
            for (int c = 0; c < nc; c++) { int k = id[c]; if (k >= 0 && k < nV && !nueva[k]) { nueva[k] = 1; tocados++; } }
        } else if (modoUV == SelEdge) {
            // ARISTA: entra si sus DOS extremos caen adentro (el borde queda blanco entero)
            for (int c = 0; c < nc; c++) {
                int ka = id[c], kb = id[(c+1) % nc];
                if (ka < 0 || ka >= nV || kb < 0 || kb >= nV) continue;
                float ax, ay, bx, by;
                UVtoScreen(m->uv[ka*2], m->uv[ka*2+1], cx, cy, s, ax, ay);
                UVtoScreen(m->uv[kb*2], m->uv[kb*2+1], cx, cy, s, bx, by);
                if (ax < rx0 || ax > rx1 || ay < ry0 || ay > ry1) continue;
                if (bx < rx0 || bx > rx1 || by < ry0 || by > ry1) continue;
                if (!nueva[ka]) { nueva[ka] = 1; tocados++; }
                if (!nueva[kb]) { nueva[kb] = 1; tocados++; }
            }
        } else { // SelVertex: cada corner por su cuenta (seleccion POR RENDER-VERT, como el pick)
            for (int c = 0; c < nc; c++) {
                int k = id[c]; if (k < 0 || k >= nV || nueva[k]) continue;
                float px, py; UVtoScreen(m->uv[k*2], m->uv[k*2+1], cx, cy, s, px, py);
                if (px < rx0 || px > rx1 || py < ry0 || py > ry1) continue;
                nueva[k] = 1; tocados++;
            }
        }
    }
    if (!add) m->uvSelVert.assign(nV, 0);
    for (int i = 0; i < nV; i++) if (nueva[i]) m->uvSelVert[i] = 1;
    // SYNC ON (modo espejo): la seleccion del UV es la del 3D -> hay que escribirla ALLA y
    // re-derivar, sino el proximo frame la pisa SincronizarFiltro3D.
    if (syncSelection && (Object*)m == g_editMesh && m->edit) {
        if (!add) m->edit->SeleccionarTodo(false);
        for (int i = 0; i < nV; i++) {
            if (!nueva[i]) continue;
            int ek = UVRenderVertAEdit(m, i);
            if (ek >= 0 && ek < (int)m->edit->vertSel.size()) m->edit->vertSel[ek] = 1;
        }
        m->edit->Recolorear();
        SincronizarSelDesde3D(m);
    }
    g_redraw = true;
    return tocados;
}

// map: render-vert gi -> indice de EDIT-vert (via su representante posRep). -1 si no hay.
// O(nEditVerts) por lookup: se llama 1 vez por click (pick), no por frame.
static int UVRenderVertAEdit(Mesh* m, int gi) {
    if (!m || !m->edit || gi < 0 || gi >= m->vertexSize) return -1;
    int rep = ((int)m->posRep.size() == m->vertexSize) ? m->posRep[gi] : gi;
    const std::vector<int>& ev = m->edit->editVerts;
    for (size_t k = 0; k < ev.size(); k++) if (ev[k] == rep) return (int)k;
    return -1;
}

// SYNC SELECTION: uvSelVert es un ESPEJO de la seleccion del 3D. Re-deriva desde la malla
// (VertsSelPorModo) para que seleccionar una cara/vert/borde en el viewport 3D quede resaltada
// y editable en el UV. No-op fuera de sync (ahi uvSelVert es la seleccion PROPIA del UV).
void UVEditor::SincronizarSelDesde3D(Mesh* m) {
    if (!syncSelection || !m || m->vertexSize <= 0) return;
    if ((Object*)m != g_editMesh) return;   // solo en Edit Mode de esta malla
    std::vector<char> sv; m->VertsSelPorModo(sv);
    const int nV = m->vertexSize;
    if ((int)sv.size() != nV) return;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign(nV, 0);
    for (int i = 0; i < nV; i++) m->uvSelVert[i] = sv[i] ? 1 : 0;
}

// FIRMA del filtro: identifica el CONJUNTO de caras visibles con un solo entero (hash FNV de sus
// indices). Nunca da 0 (0 = "sin inicializar"), asi "ninguna cara" tambien tiene firma propia.
static unsigned UVFiltroFirma(const std::vector<char>& fsel) {
    unsigned h = 2166136261u;
    for (size_t f = 0; f < fsel.size(); f++)
        if (fsel[f]) { h ^= (unsigned)(f + 1); h *= 16777619u; }
    return h ? h : 1u;
}

// UN punto de entrada por frame para la relacion 3D <-> UV (ver el bloque SYNC SELECTION del
// header). En SYNC espeja y listo. Fuera de sync la seleccion del UV es PROPIA: aca SOLO se la
// inicializa cuando cambia el FILTRO (las caras seleccionadas en 3D), la malla o el toggle de
// sync -> las esquinas que ACABAN de aparecer entran seleccionadas (como Blender: lo que se ve,
// se puede mover ya). De ahi en mas no se toca nada: manda el click del UV (PickUV), que es por
// RENDER-VERT. Nunca corre con un transform en curso (no pisa lo que se esta moviendo).
void UVEditor::SincronizarFiltro3D(Mesh* m, std::vector<char>* outFiltro) {
    if (outFiltro) outFiltro->clear();
    if (!m || m->vertexSize <= 0) return;
    std::vector<char> fsel; UVCarasFiltro3D(m, fsel);  // el filtro, UNA vez por frame
    if (outFiltro) *outFiltro = fsel;
    if (syncSelection) {                       // modo espejo: la seleccion del mesh manda, por frame
        filtroSync = true; filtroSerial = m->serial;
        SincronizarSelDesde3D(m);
        return;
    }
    if ((Object*)m != g_editMesh) return;      // sin Edit Mode no hay filtro que seguir
    if (XformEnCurso()) return;                // transform en curso: la seleccion esta en uso
    const int nV = m->vertexSize;
    const unsigned sig = UVFiltroFirma(fsel);
    const bool tamMal = ((int)m->uvSelVert.size() != nV); // la topologia cambio por afuera
    if (!tamMal && filtroSig == sig && filtroSerial == m->serial && !filtroSync) return; // nada cambio
    filtroSig = sig; filtroSerial = m->serial; filtroSync = false;
    // INICIALIZAR: seleccionadas TODAS las esquinas visibles (las de las caras del filtro), y
    // NADA de lo que no se ve (sino un G movia UVs invisibles).
    m->uvSelVert.assign((size_t)nV, 0);
    for (size_t f = 0; f < m->faces3d.size() && f < fsel.size(); f++) {
        if (!fsel[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) { int k = id[c];
            if (k >= 0 && k < nV) m->uvSelVert[k] = 1; }
    }
}

// pick: encuentra el elemento mas cercano (segun EditSelectMode) y actualiza Mesh::uvSelVert.
void UVEditor::PickUV(Mesh* m, int lx, int ly, bool add) {
    if (!m || !m->uv) return;
    const int nV = m->vertexSize;
    if (nV <= 0) return;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign(nV, 0);

    // mismos parametros de transform que el Render (cx,cy,s)
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    const float cx = width * 0.5f + panX;
    const float cy = top + ch * 0.5f + panY;
    float baseSize = (float)(width < ch ? width : ch) * 0.8f;
    float s = baseSize * zoom; if (s < 1.0f) s = 1.0f;

    // caras visibles (= las que dibuja el Render): el FILTRO del 3D. Con sync OFF solo se puede
    // pickear ADENTRO de el (lo que no se ve no se selecciona).
    const bool enEdit = ((Object*)m == g_editMesh);
    std::vector<char> sel3d; UVCarasFiltro3D(m, sel3d);

    const int modoUV = ModoUV();                       // vertex/edge/face efectivo
    const float clx = (float)lx, cly = (float)ly;
    const float rad = (float)GlobalScale * 5.0f;       // radio de pick (px)
    int hitVert = -1; float bestVD = rad*rad;
    int hitEA = -1, hitEB = -1; float bestED = rad*rad;
    int hitFace = -1;

    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        const int nc = (int)id.size();
        if (nc < 2) continue;
        const bool selFace = enEdit ? (f < sel3d.size() && sel3d[f]) : true;
        if (!syncSelection && !selFace) continue;
        if (modoUV == SelVertex) {
            for (int c = 0; c < nc; c++) { int ka = id[c]; if (ka < 0 || ka >= nV) continue;
                float sx,sy; UVtoScreen(m->uv[ka*2], m->uv[ka*2+1], cx,cy,s, sx,sy);
                float d = (sx-clx)*(sx-clx) + (sy-cly)*(sy-cly);
                if (d < bestVD) { bestVD = d; hitVert = ka; } }
        } else if (modoUV == SelEdge) {
            for (int c = 0; c < nc; c++) { int ka = id[c], kb = id[(c+1)%nc]; if (ka<0||ka>=nV||kb<0||kb>=nV) continue;
                float ax,ay,bx,by; UVtoScreen(m->uv[ka*2],m->uv[ka*2+1],cx,cy,s,ax,ay);
                UVtoScreen(m->uv[kb*2],m->uv[kb*2+1],cx,cy,s,bx,by);
                float d = DistPtSeg2(clx,cly, ax,ay, bx,by);
                if (d < bestED) { bestED = d; hitEA = ka; hitEB = kb; } }
        } else { // SelFace: click DENTRO del poligono (el ultimo que contenga = el de mas "encima")
            std::vector<float> poly; bool ok = true;
            for (int c = 0; c < nc; c++) { int ka = id[c]; if (ka<0||ka>=nV) { ok=false; break; }
                float sx,sy; UVtoScreen(m->uv[ka*2],m->uv[ka*2+1],cx,cy,s,sx,sy); poly.push_back(sx); poly.push_back(sy); }
            if (ok && PointInPoly(clx,cly, &poly[0], nc)) hitFace = (int)f;
        }
    }

    // SYNC ON (modo ESPEJO): el pick va a la seleccion del 3D (BIDIRECCIONAL) via la API del
    // EditMesh y despues uvSelVert se re-espeja. Seleccionar en el UV tambien selecciona en el
    // viewport 3D. OJO: el mapeo render-vert -> edit-vert es POR POSICION (UVRenderVertAEdit usa
    // posRep) y VertsSelPorModo devuelve expandido -> en una COSTURA quedan seleccionadas TODAS
    // las copias UV de ese vertice 3D. Es lo ESPERADO de este modo (por eso el default es OFF).
    if (syncSelection && enEdit && m->edit) {
        const bool soloEste = !add;   // sin shift = reemplaza
        if (modoUV == SelVertex) {
            int ek = UVRenderVertAEdit(m, hitVert);
            if (ek >= 0) m->edit->TogglearVert(ek, soloEste);
            else if (soloEste) m->edit->SeleccionarTodo(false); // click al vacio: deselecciona
        } else if (modoUV == SelEdge) {
            int ea = UVRenderVertAEdit(m, hitEA), eb = UVRenderVertAEdit(m, hitEB), eEdge = -1;
            if (ea >= 0 && eb >= 0)
                for (size_t e2 = 0; e2*2+1 < m->edit->lineIdx.size(); e2++) {
                    int la = m->edit->lineIdx[e2*2], lb = m->edit->lineIdx[e2*2+1];
                    if ((la==ea && lb==eb) || (la==eb && lb==ea)) { eEdge = (int)e2; break; }
                }
            if (eEdge >= 0) m->edit->TogglearEdge(eEdge, soloEste);
            else if (soloEste) m->edit->SeleccionarTodo(false);
        } else { // SelFace
            int efe = -1;
            if (hitFace >= 0)
                for (size_t fe = 0; fe < m->edit->faceSrc.size(); fe++)
                    if (m->edit->faceSrc[fe] == hitFace) { efe = (int)fe; break; }
            if (efe >= 0) m->edit->TogglearFace(efe, soloEste);
            else if (soloEste) m->edit->SeleccionarTodo(false);
        }
        m->edit->Recolorear();       // el viewport 3D refleja el cambio
        SincronizarSelDesde3D(m);    // y el UV re-espeja la seleccion
        g_redraw = true;
        return;
    }

    // SYNC OFF (DEFAULT): seleccion PROPIA del UV y POR RENDER-VERT. Se escribe EXACTAMENTE el
    // vert (o los 2 del borde, o los corners de la cara) que se clickeo: las otras copias UV del
    // mismo vertice 3D (costuras) NO se tocan, y la seleccion del viewport 3D tampoco.
    if (!add) m->uvSelVert.assign(nV, 0); // sin shift: reemplaza
    if (modoUV == SelVertex && hitVert >= 0) {
        m->uvSelVert[hitVert] = add ? (m->uvSelVert[hitVert] ? 0 : 1) : 1;
    } else if (modoUV == SelEdge && hitEA >= 0) {
        unsigned char nv = add ? ((m->uvSelVert[hitEA] && m->uvSelVert[hitEB]) ? 0 : 1) : 1;
        m->uvSelVert[hitEA] = nv; m->uvSelVert[hitEB] = nv;
    } else if (modoUV == SelFace && hitFace >= 0) {
        const std::vector<int>& id = m->faces3d[hitFace].idx;
        bool allSel = true;
        for (size_t c = 0; c < id.size(); c++) { int k = id[c]; if (k>=0 && k<nV && !m->uvSelVert[k]) { allSel = false; break; } }
        unsigned char nv = add ? (allSel ? 0 : 1) : 1;
        for (size_t c = 0; c < id.size(); c++) { int k = id[c]; if (k>=0 && k<nV) m->uvSelVert[k] = nv; }
    }
}

// ===================================================================================================
//  ARMATURE 2D DEL MESH: creacion/edicion de huesos + transform modal (huesos y pose 2D).
//  Todo opera sobre el armature ACTIVO de Mesh::armatures2d (los rigs VIVEN en la malla y viajan
//  con ella en el .w3d; una malla puede tener varios, cada uno con sus huesos y sus clips).
// ===================================================================================================

// TAB sobre el UV editor = ENTRAR / SALIR de la edicion de huesos 2D. Ver el ciclo documentado en
// UVEditor.h (Blender: Tab alterna Edit Bones con el modo del que venis). Requisitos para consumir
// la tecla: UV operativo (malla activa en Edit Mode) y la malla YA con armature 2D; si no, false y
// el Tab GLOBAL hace lo de siempre. Un transform a medio hacer se CANCELA (como el Tab del 3D, que
// no deja un grab colgado). No toca InteractionMode: el modo del UV es propio del viewport.
bool UVEditor::TabToggleHuesos() {
    if (!EnEdicionUV()) return false;                       // el UV no esta operativo: Tab global
    Mesh* m = (Mesh*)ObjActivo;                             // EnEdicionUV ya garantizo mesh + g_editMesh
    // sin rig 2D el Tab solo es del UV si hay que SALIR del modo objeto (ver el ciclo en UVEditor.h)
    if (!m->TieneArm2D() && uvModo != UVModoObjeto) return false;
    if (Bone2DXformActivo()) Bone2DXformCancel(m);          // G/R/S de huesos/pose colgado -> cancelar
    if (uvXform) CancelarXform(m);                          // idem el G/R/S de UVs
    if (uvModo == UVModoObjeto) {
        // ENTRAR a editar el OBJETO ACTIVO del UV: el armature 2D activo (Edit Bones) o la
        // geometria (edicion de UVs). Sin armatures siempre es la geometria.
        const bool alArmature = uvObjArm && m->TieneArm2D();
        uvModoPrevio = UVModoObjeto;                        // el Tab de vuelta devuelve a Objeto
        uvModo = alArmature ? UVModoHuesos : UVModoEdicion;
        if (alArmature) PropsIrAArmature2D();               // el panel se para en la pestania del rig 2D
    } else if (uvModo == UVModoHuesos) {                    // SALIR: vuelve al modo del que se entro
        uvModo = (uvModoPrevio == UVModoHuesos) ? UVModoObjeto : uvModoPrevio;
    } else if (uvModo == UVModoEdicion) {                   // edicion de UVs -> Objeto (Tab de Blender)
        uvModo = UVModoObjeto;
        uvObjArm = false;                                   // se venia editando la GEOMETRIA
    } else {                                                // ENTRAR a Edit Bones (desde Pesos / Pose)
        uvModoPrevio = uvModo;
        uvModo = UVModoHuesos;
        uvObjArm = true;
        PropsIrAArmature2D();                               // el panel se para en la pestania del rig 2D
    }
    g_redraw = true;
    return true;
}

// hueso mas cercano al punto (px LOCALES) mirando TODOS los armatures 2D (no solo el activo): es
// el pick del MODO OBJETO. Devuelve el indice del hueso dentro de su armature (outArm), o -1.
int UVEditor::Bone2DPickTodos(Mesh* m, float lx, float ly, int* outArm) const {
    if (outArm) *outArm = -1;
    if (!m || !m->TieneArm2D()) return -1;
    float cx, cy, s; ParamsUV(cx, cy, s);
    const bool pose = (uvModo == UVModoPose);
    int bestB = -1, bestA = -1;
    float bestD = 14.0f * 14.0f;      // mismo radio que el pick de cuerpo de hueso / de vertices
    for (size_t a = 0; a < m->armatures2d.size(); a++) {
        const Armature2D* arm = m->armatures2d[a];
        if (!arm || arm->huesos.empty()) continue;
        std::vector<float> M;
        if (pose) m->Armature2DMatricesDe(arm, M);
        for (size_t i = 0; i < arm->huesos.size(); i++) {
            float hu, hv, tu, tv;
            Bone2DExtremosDe(arm->huesos, M, i, pose, hu, hv, tu, tv);
            float ax, ay, bx, by;
            UVtoScreen(hu, hv, cx, cy, s, ax, ay);
            UVtoScreen(tu, tv, cx, cy, s, bx, by);
            float d2 = DistPtSeg2(lx, ly, ax, ay, bx, by);
            if (d2 < bestD) { bestD = d2; bestB = (int)i; bestA = (int)a; }
        }
    }
    if (outArm) *outArm = bestA;
    return bestB;
}

// CTRL+CLICK EN WEIGHT PAINT: el hueso 2D bajo el cursor (de CUALQUIER armature, no solo el
// activo) pasa a ser el hueso activo de SU rig, ese rig pasa a ser el ACTIVO de la malla y su UV
// group homonimo el grupo que se pinta. Es el gesto de Blender (en Weight Paint, Ctrl+click
// selecciona el hueso) y cierra el flujo de rigging 2D: sin el, el UNICO camino para cambiar de
// grupo era el desplegable "Group" de la toolbar, aunque los huesos SE DIBUJAN mientras se pinta.
// Sale casi gratis porque el binding hueso <-> UV group es POR NOMBRE. Se pide Ctrl a proposito:
// con el click pelado, los huesos (radio de pick ~14px) se comerian los trazos justo donde mas se
// pinta. Devuelve true si engancho un hueso (el click NO tiene que pintar).
// Vive aparte de button_left (que es codigo SDL) para poder testear el camino sin simular teclas.
bool UVEditor::Bone2DElegirGrupoPorClick(Mesh* m, float lx, float ly) {
    if (!m || !m->TieneArm2D()) return false;
    int armIdx = -1;
    int bh = Bone2DPickTodos(m, lx, ly, &armIdx);
    if (bh < 0 || armIdx < 0 || armIdx >= (int)m->armatures2d.size()) return false;
    Arm2DSetActivo(m, armIdx);   // cambiar de rig recarga el rango del timeline (kind 4)
    Armature2D* arm = m->armatures2d[armIdx];
    if (arm && bh < (int)arm->huesos.size()) {
        arm->boneActivo = bh;
        const std::string& nom = arm->huesos[bh].nombre;
        int g = -1;
        for (size_t gi = 0; gi < m->uvGroups.size(); gi++)
            if (m->uvGroups[gi] && m->uvGroups[gi]->nombre == nom) { g = (int)gi; break; }
        if (g >= 0) { m->uvGrupoActivo = g; Notificar(nom, false); }
        else        Notificar(T("No UV Group for this bone"), false);
    }
    g_redraw = true;
    return true;
}

// MODO OBJETO: el click elige QUE se edita. Si cae sobre un hueso de cualquier armature 2D, ESE
// armature pasa a ser el activo de la malla y el objeto activo del UV; si no, queda la GEOMETRIA.
int UVEditor::PickObjetoUV(Mesh* m, float lx, float ly) {
    if (!m) return -1;
    int arm = -1;
    int b = Bone2DPickTodos(m, lx, ly, &arm);
    if (b >= 0 && arm >= 0) {
        Arm2DSetActivo(m, arm); // el rig elegido manda: si el timeline esta en kind 4, recarga su rango
        // el hueso clickeado queda como ACTIVO del armature (asi el panel muestra ese) y unico
        // seleccionado: al entrar con Tab a Edit Bones se sigue trabajando sobre lo que se eligio.
        Bone2DSelLimpiar(m);
        Bone2DSelHueso(m, b, true);
        m->Arm2DBoneActivo() = b;
        uvObjArm = true;
        g_redraw = true;
        return arm;
    }
    uvObjArm = false;   // nada de rig bajo el cursor: el objeto activo es la GEOMETRIA
    g_redraw = true;
    return -1;
}

// crea el armature 2D del mesh (si no existe): 1 hueso vertical en el centro del cuadrado UV,
// con su UV GROUP homonimo (binding por nombre), y entra al modo de edicion de huesos.
void UVEditor::Armature2DCrear(Mesh* m) {
    if (!m) return;
    if (m->Arm2DHuesos().empty()) {
        // el CONTENEDOR (armature 0) va ANTES del snapshot: el undo de huesos guarda los huesos de
        // UN armature concreto, asi que tiene que existir cuando se captura (sino el undo no aplica).
        // Si el contenedor lo creamos NOSOTROS va con su propio paso de undo (queda ABAJO del paso
        // del hueso): Ctrl+Z saca el hueso, otro Ctrl+Z saca el armature -> no queda un rig vacio.
        const bool creoArm = m->armatures2d.empty();
        m->Arm2DAsegurar();
        if (creoArm) UndoArm2DAgregado(m, m->armature2dActivo);
        UndoBones2DCapturar(m); // snapshot ANTES de mutar (patron BonesUndo: op discreta = push directo)
        W3dBone2D b;
        b.nombre = Bone2DNombreUnico(m, "Bone");
        b.headU = 0.5f; b.headV = 0.5f;   // el tail "para arriba" en pantalla (V crece para abajo)
        b.tailU = 0.5f; b.tailV = 0.3f;
        b.select = true; b.selHead = true; b.selTail = true; // hueso entero seleccionado
        m->Arm2DHuesos().push_back(b);
        m->Arm2DBoneActivo() = (int)m->Arm2DHuesos().size() - 1;
        Bone2DAsegurarGrupo(m, b.nombre);
    }
    if (uvModo != UVModoHuesos) uvModoPrevio = uvModo;  // el Tab vuelve al modo del que se venia
    uvModo = UVModoHuesos;
    uvObjArm = true;        // se pasa a editar el rig: el objeto activo del UV es el armature
    PropsIrAArmature2D();   // recien creado: el panel se para en la pestania del rig 2D
    g_redraw = true;
}

// ARMATURE 2D NUEVO (menu Add > Armature 2D con la malla ya rigueada, y el boton Add de la lista
// del panel): crea OTRO armature independiente (sus huesos, sus clips) con 1 hueso, lo deja ACTIVO
// y entra a editarlo. Devuelve el indice del armature nuevo.
// UNDO en DOS pasos (a proposito, cada uno es una op discreta): el primer Ctrl+Z saca el hueso y
// deja el armature vacio; el segundo saca el armature de la lista (UndoArm2DAgregado).
int UVEditor::Armature2DNuevo(Mesh* m) {
    if (!m) return -1;
    int idx = m->Arm2DAgregar("Armature 2D");
    UndoArm2DAgregado(m, idx); // el ALTA es el paso de undo: Ctrl+Z saca el armature entero
    UndoBones2DCapturar(m); // snapshot del armature NUEVO (ya activo): el undo del hueso, no del alta
    W3dBone2D b;
    b.nombre = Bone2DNombreUnico(m, "Bone");
    b.headU = 0.5f; b.headV = 0.5f;
    b.tailU = 0.5f; b.tailV = 0.3f;
    b.select = true; b.selHead = true; b.selTail = true;
    m->Arm2DHuesos().push_back(b);
    m->Arm2DBoneActivo() = 0;
    Bone2DAsegurarGrupo(m, b.nombre);
    if (uvModo != UVModoHuesos) uvModoPrevio = uvModo;
    uvModo = UVModoHuesos;
    uvObjArm = true;
    PropsIrAArmature2D();
    g_redraw = true;
    return idx;
}

// hueso RAIZ nuevo (menu Add > Bone): en el centro, seleccionado y activo, con su vertex group.
int UVEditor::Bone2DAgregar(Mesh* m) {
    if (!m) return -1;
    const bool creoArm = m->armatures2d.empty();
    m->Arm2DAsegurar();     // sin armature 2D no hay donde meter el hueso: se crea el contenedor
    if (creoArm) UndoArm2DAgregado(m, m->armature2dActivo); // el contenedor nuevo, con su propio paso
    UndoBones2DCapturar(m); // snapshot ANTES de mutar (op discreta)
    W3dBone2D b;
    b.nombre = Bone2DNombreUnico(m, "Bone");
    b.headU = 0.5f; b.headV = 0.5f; b.tailU = 0.5f; b.tailV = 0.3f;
    b.select = true; b.selHead = true; b.selTail = true;
    Bone2DSelLimpiar(m);
    m->Arm2DHuesos().push_back(b);
    m->Arm2DBoneActivo() = (int)m->Arm2DHuesos().size() - 1;
    Bone2DAsegurarGrupo(m, b.nombre);
    g_redraw = true;
    return m->Arm2DBoneActivo();
}

// E: hueso nuevo desde el TAIL del activo (hijo CONECTADO: head = tail del padre). El tail nuevo
// arranca pegado al head (el caller lo deja agarrado al mouse con Bone2DDragEnd). -1 si no hay activo.
int UVEditor::Bone2DExtruir(Mesh* m) {
    if (!m) return -1;
    int act = m->Arm2DBoneActivo();
    if (act < 0 || act >= (int)m->Arm2DHuesos().size()) return -1;
    UndoBones2DCapturar(m); // snapshot ANTES de mutar (op discreta; el drag posterior es OTRO paso)
    W3dBone2D b;
    b.nombre = Bone2DNombreUnico(m, "Bone");
    b.padre = act;                                 // hijo del activo (padre < hijo: se apendea)
    b.conectado = true;                            // extrude = CONECTADO (head soldado al tail del padre)
    b.headU = m->Arm2DHuesos()[act].tailU; b.headV = m->Arm2DHuesos()[act].tailV;
    b.tailU = b.headU; b.tailV = b.headV;
    b.selTail = true;                              // el tip nuevo queda seleccionado (como Blender)
    Bone2DSelLimpiar(m);
    m->Arm2DHuesos().push_back(b);
    m->Arm2DBoneActivo() = (int)m->Arm2DHuesos().size() - 1;
    Bone2DAsegurarGrupo(m, b.nombre);
    g_redraw = true;
    return m->Arm2DBoneActivo();
}

// Shift+D: duplica los huesos seleccionados como RAICES sueltas (sin conectar al original), con
// nombre/vertex group nuevos y un offset chico para verlos. Devuelve el indice del activo nuevo.
int UVEditor::Bone2DDuplicar(Mesh* m) {
    if (!m) return -1;
    std::vector<int> sel;
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++) if (m->Arm2DHuesos()[i].select) sel.push_back((int)i);
    if (sel.empty() && m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < (int)m->Arm2DHuesos().size())
        sel.push_back(m->Arm2DBoneActivo());
    if (sel.empty()) return -1;
    UndoBones2DCapturar(m); // snapshot ANTES de mutar (op discreta)
    Bone2DSelLimpiar(m);
    int primero = -1;
    for (size_t k = 0; k < sel.size(); k++) {
        W3dBone2D b = m->Arm2DHuesos()[sel[k]];       // copia (pose incluida)
        b.nombre = Bone2DNombreUnico(m, m->Arm2DHuesos()[sel[k]].nombre);
        b.padre = -1;                              // suelto (como el duplicate del 3D)
        b.headU += 0.02f; b.headV += 0.02f; b.tailU += 0.02f; b.tailV += 0.02f;
        b.select = true; b.selHead = true; b.selTail = true;
        m->Arm2DHuesos().push_back(b);
        if (primero < 0) primero = (int)m->Arm2DHuesos().size() - 1;
        Bone2DAsegurarGrupo(m, b.nombre);
    }
    m->Arm2DBoneActivo() = primero;
    Bone2DSanearConectado(m);   // la copia quedo RAIZ (padre = -1) y arrastraba el flag del original
    g_redraw = true;
    return primero;
}

// Bone2DTrack::bone es un INDICE a los huesos del armature, asi que TODA compactacion o reorden de los
// huesos 2D tiene que remapear tambien los tracks de los clips de ESE armature (Armature2D::anims,
// que aca se leen por el accesor del ACTIVO Arm2DAnims()). Sin esto, borrar
// el hueso 0 dejaba sus curvas manejando al hueso 1 y las del 1 huerfanas (y se seguian guardando
// en el .w3d). Es el calco de lo que hace BoneEditBorrar con los clips del armature 3D.
//   nuevoIdx[viejo] = indice nuevo, o -1 si ese hueso se borro (su track se va con el).
static void Bone2DRemapTracks(Mesh* m, const std::vector<int>& nuevoIdx) {
    if (!m) return;
    const int n = (int)nuevoIdx.size();
    for (size_t a = 0; a < m->Arm2DAnims().size(); a++) {
        Armature2DAnimation* clip = m->Arm2DAnims()[a];
        if (!clip) continue;
        for (size_t t = 0; t < clip->tracks.size(); ) {
            int b = clip->tracks[t].bone;
            int nb = (b >= 0 && b < n) ? nuevoIdx[b] : -1;
            if (b >= 0 && nb < 0) clip->tracks.erase(clip->tracks.begin() + t); // hueso borrado (o track colgado)
            else { clip->tracks[t].bone = nb; ++t; }
        }
    }
    m->last2dFrame = -999999; m->last2dAnim = -999; m->pose2dDirty = true; // forzar re-evaluacion
}

// X/Supr: borra los seleccionados. Los hijos se re-parentan al ABUELO (o quedan raiz). Los vertex
// groups del mismo nombre QUEDAN (los pesos pintados no se pierden; se pueden re-bindear creando
// un hueso con ese nombre). Se re-mapean los indices padre Y los tracks de los clips 2D.
bool UVEditor::Bone2DBorrar(Mesh* m) {
    if (!m || m->Arm2DHuesos().empty()) return false;
    std::vector<char> borrar(m->Arm2DHuesos().size(), 0);
    bool hay = false;
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++)
        if (m->Arm2DHuesos()[i].select) { borrar[i] = 1; hay = true; }
    if (!hay && m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < (int)m->Arm2DHuesos().size()) {
        borrar[m->Arm2DBoneActivo()] = 1; hay = true;
    }
    if (!hay) return false;
    UndoBones2DCapturar(m); // snapshot ANTES de mutar (incluye la capa uv: el borrado re-aplica el skinning)
    // 1) re-parentar: el padre EFECTIVO de cada hueso = el primer ancestro NO borrado
    const int n = (int)m->Arm2DHuesos().size();
    for (int i = 0; i < n; i++) {
        if (borrar[i]) continue;
        int p = m->Arm2DHuesos()[i].padre;
        while (p >= 0 && p < n && borrar[p]) p = m->Arm2DHuesos()[p].padre;
        m->Arm2DHuesos()[i].padre = p;
    }
    // 2) compactar + re-mapear indices
    std::vector<int> nuevoIdx(n, -1);
    std::vector<W3dBone2D> nuevos;
    for (int i = 0; i < n; i++) if (!borrar[i]) { nuevoIdx[i] = (int)nuevos.size(); nuevos.push_back(m->Arm2DHuesos()[i]); }
    for (size_t i = 0; i < nuevos.size(); i++) {
        int p = nuevos[i].padre;
        nuevos[i].padre = (p >= 0 && p < n) ? nuevoIdx[p] : -1;
    }
    Bone2DRemapTracks(m, nuevoIdx); // las curvas siguen a SU hueso (y las del borrado se van)
    m->Arm2DHuesos().swap(nuevos);
    m->Arm2DBoneActivo() = -1;
    // GEMELO 2D del aviso de BoneEditBorrar: la seleccion del dope guarda el indice del hueso
    // dentro de su clave ("arm2d:#<serial malla>/a<ARM>/k<CLIP>/b<IDX>"). De arriba hacia abajo (ver
    // BoneEdit.cpp) y POR CADA CLIP del rig 2D: el hueso es del ARMATURE, no de un clip.
    {
        const size_t nClips = m->Arm2DAnims().size();
        for (size_t c = 0; c < nClips; c++){
            char sa[48]; snprintf(sa, sizeof sa, "/a%d/k%d/b", m->armature2dActivo, (int)c);
            const std::string pref = "arm2d:" + DopeIdDueno(m) + sa;  // POR SERIAL: el nombre se recicla
            for (int i = n - 1; i >= 0; i--) if (borrar[i]) DopeRemapIndiceClave(pref, i, -1);
        }
    }
    // el hijo que subio al ABUELO conserva su posicion: su head ya no coincide con el tail del padre nuevo,
    // asi que el flag 'conectado' heredado quedaria mintiendo. El saneador lo apaga (y lo deja prendido en
    // el caso raro en que las puntas SI coincidan). No mueve nada.
    Bone2DSanearConectado(m);
    // la pose de lo que queda sigue valida: re-aplicar el skinning (si habia deformacion)
    m->Armature2DAplicar();
    g_redraw = true;
    return true;
}

// Clear Parent (Alt+P > Clear Parent): los seleccionados quedan sin padre (raiz). No mueve nada.
bool Bone2DClearParentSel(Mesh* m) {
    if (!m) return false;
    // pre-scan: va a cambiar algo? (el snapshot de undo se toma ANTES de mutar, sin pasos
    // vacios). Mismo criterio que la mutacion de abajo: seleccionados con padre, o el
    // fallback al activo cuando ningun seleccionado tenia padre.
    bool va = false;
    for (size_t i = 0; i < m->Arm2DHuesos().size() && !va; i++)
        if (m->Arm2DHuesos()[i].select && m->Arm2DHuesos()[i].padre >= 0) va = true;
    if (!va && m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < (int)m->Arm2DHuesos().size() &&
        m->Arm2DHuesos()[m->Arm2DBoneActivo()].padre >= 0) va = true;
    if (!va) return false;
    UndoBones2DCapturar(m); // snapshot ANTES de mutar (op discreta)
    bool alguno = false;
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++)
        if (m->Arm2DHuesos()[i].select && m->Arm2DHuesos()[i].padre >= 0) { m->Arm2DHuesos()[i].padre = -1; alguno = true; }
    if (!alguno && m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < (int)m->Arm2DHuesos().size() &&
        m->Arm2DHuesos()[m->Arm2DBoneActivo()].padre >= 0) { m->Arm2DHuesos()[m->Arm2DBoneActivo()].padre = -1; alguno = true; }
    if (alguno) { Bone2DSanearConectado(m); g_redraw = true; } // sin padre no hay tail al que estar soldado
    return alguno;
}
bool UVEditor::Bone2DDesconectar(Mesh* m) { return Bone2DClearParentSel(m); }

// Alt+P > Disconnect Bone: los seleccionados (o el activo) SIGUEN emparentados pero pierden la
// soldadura (conectado = false): la punta compartida deja de ser una sola y al separarla queda
// la linea punteada con el padre. No mueve nada.
bool Bone2DDesconectarSel(Mesh* m) {
    if (!m) return false;
    bool va = false;
    for (size_t i = 0; i < m->Arm2DHuesos().size() && !va; i++)
        if ((m->Arm2DHuesos()[i].select || (int)i == m->Arm2DBoneActivo()) &&
            m->Arm2DHuesos()[i].padre >= 0 && m->Arm2DHuesos()[i].conectado) va = true;
    if (!va) return false;
    UndoBones2DCapturar(m); // snapshot ANTES de mutar (op discreta)
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++)
        if ((m->Arm2DHuesos()[i].select || (int)i == m->Arm2DBoneActivo()) && m->Arm2DHuesos()[i].padre >= 0)
            m->Arm2DHuesos()[i].conectado = false;
    g_redraw = true;
    return true;
}

// 'b' es el mismo 'ancestro' o desciende de el? (para excluir candidatos que armarian un ciclo)
bool Bone2DEsDescendiente(Mesh* m, int ancestro, int b) {
    if (!m || b < 0 || b >= (int)m->Arm2DHuesos().size()) return false;
    if (b == ancestro) return true;
    int p = m->Arm2DHuesos()[b].padre, guard = 0;
    while (p >= 0 && p < (int)m->Arm2DHuesos().size() && guard++ < (int)m->Arm2DHuesos().size()) {
        if (p == ancestro) return true;
        p = m->Arm2DHuesos()[p].padre;
    }
    return false;
}

// restaura el invariante padre < hijo (una sola pasada de FK en Armature2DMatrices) con un
// REORDEN topologico ESTABLE: los padres van antes que sus hijos conservando el orden relativo.
// Remapea los indices 'padre', bone2dActivo Y los tracks de los clips 2D (Bone2DTrack::bone es
// un indice a los huesos del armature). Solo hace falta tras RE-PARENTAR (extrude/borrar ya lo conservan);
// sin ciclos garantizado (el caller los rechaza antes).
static void Bone2DReordenarTopo(Mesh* m) {
    const int n = (int)m->Arm2DHuesos().size();
    if (n <= 1) return;
    bool roto = false;
    for (int i = 0; i < n && !roto; i++)
        if (m->Arm2DHuesos()[i].padre >= i) roto = (m->Arm2DHuesos()[i].padre != -1);
    if (!roto) return; // el invariante ya vale: no tocar nada (indices estables)
    std::vector<int> orden;           // indices VIEJOS en el orden nuevo
    std::vector<int> nuevoIdx(n, -1); // viejo -> nuevo
    bool prog = true;
    while ((int)orden.size() < n && prog) {
        prog = false;
        for (int i = 0; i < n; i++) {
            if (nuevoIdx[i] >= 0) continue;
            int p = m->Arm2DHuesos()[i].padre;
            if (p < 0 || p >= n || nuevoIdx[p] >= 0) {
                nuevoIdx[i] = (int)orden.size(); orden.push_back(i); prog = true;
            }
        }
    }
    for (int i = 0; i < n; i++) // ciclo residual (no deberia): se apendea tal cual
        if (nuevoIdx[i] < 0) { nuevoIdx[i] = (int)orden.size(); orden.push_back(i); }
    std::vector<W3dBone2D> nuevos;
    nuevos.reserve(n);
    for (int k = 0; k < n; k++) {
        W3dBone2D b = m->Arm2DHuesos()[orden[k]];
        b.padre = (b.padre >= 0 && b.padre < n) ? nuevoIdx[b.padre] : -1;
        nuevos.push_back(b);
    }
    Bone2DRemapTracks(m, nuevoIdx); // las curvas viajan con su hueso al nuevo indice
    m->Arm2DHuesos().swap(nuevos);
    if (m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < n) m->Arm2DBoneActivo() = nuevoIdx[m->Arm2DBoneActivo()];
}

// re-parenta 'bone' a 'nuevoPadre' (-1 = raiz), keep offset (no mueve nada; queda SIN soldar ->
// linea punteada). Rechaza ciclos. Lo usa el desplegable "Parent" de la tarjeta Armature 2D.
bool Bone2DReparentar(Mesh* m, int bone, int nuevoPadre) {
    if (!m || bone < 0 || bone >= (int)m->Arm2DHuesos().size()) return false;
    if (nuevoPadre >= (int)m->Arm2DHuesos().size()) return false;
    if (nuevoPadre < 0) nuevoPadre = -1;
    if (nuevoPadre == m->Arm2DHuesos()[bone].padre) return false;
    if (nuevoPadre >= 0 && Bone2DEsDescendiente(m, bone, nuevoPadre)) return false; // ciclo
    UndoBones2DCapturar(m);
    m->Arm2DHuesos()[bone].padre = nuevoPadre;
    if (nuevoPadre >= 0) m->Arm2DHuesos()[bone].conectado = false; // keep offset (sin soldar)
    Bone2DSanearConectado(m);   // y si quedo raiz (-1) el flag tampoco puede sobrevivir
    Bone2DReordenarTopo(m); // el nuevo padre puede tener indice mayor -> reordena
    g_redraw = true;
    return true;
}

// SOLDAR (sin undo): mueve el hueso ENTERO para que su head caiga EXACTO en el tail del padre y
// prende el flag. Los descendientes CONECTADOS se mudan con el mismo delta (sino se romperia la
// soldadura de la cadena); los sueltos conservan su offset, como el keep-offset del resto.
static void Bone2DSoldarRaw(Mesh* m, int bone) {
    W3dBone2D& b = m->Arm2DHuesos()[bone];
    if (b.padre < 0 || b.padre >= (int)m->Arm2DHuesos().size()) return;
    const float du = m->Arm2DHuesos()[b.padre].tailU - b.headU;
    const float dv = m->Arm2DHuesos()[b.padre].tailV - b.headV;
    b.conectado = true;
    if (du == 0.0f && dv == 0.0f) return;
    const int n = (int)m->Arm2DHuesos().size();
    std::vector<char> mover(n, 0);
    mover[bone] = 1;
    // cadena soldada hacia abajo (padre < hijo NO esta garantizado tras re-parentar -> iterar)
    bool cambio = true; int guard = 0;
    while (cambio && guard++ <= n) {
        cambio = false;
        for (int c = 0; c < n; c++) {
            if (mover[c]) continue;
            int p = m->Arm2DHuesos()[c].padre;
            if (p >= 0 && p < n && mover[p] && m->Arm2DHuesos()[c].conectado) { mover[c] = 1; cambio = true; }
        }
    }
    for (int i = 0; i < n; i++) if (mover[i]) {
        W3dBone2D& q = m->Arm2DHuesos()[i];
        q.headU += du; q.headV += dv; q.tailU += du; q.tailV += dv;
    }
}

// ===== UNICO camino de CONECTAR / DESCONECTAR un hueso 2D de su padre (el flag 'conectado').
// Lo comparten el checkbox "Connected" de la tarjeta Armature 2D, el Ctrl+P > Connected y el
// Alt+P > Disconnect Bone. Conectar SUELDA (mueve el hueso para pegar su head al tail del padre);
// desconectar solo apaga el flag (no mueve nada). Un paso de undo. false = no cambio nada. =====
bool Bone2DSetConectado(Mesh* m, int bone, bool con) {
    if (!m || bone < 0 || bone >= (int)m->Arm2DHuesos().size()) return false;
    if (m->Arm2DHuesos()[bone].padre < 0) return false;         // sin padre el flag no tiene sentido
    if (Bone2DEsConectado(m, bone) == con) return false;     // ya esta en ese estado EFECTIVO
    UndoBones2DCapturar(m);
    if (con) Bone2DSoldarRaw(m, bone);
    else     m->Arm2DHuesos()[bone].conectado = false;
    g_redraw = true;
    return true;
}

// Ctrl+P: los huesos SELECCIONADOS se emparentan al ACTIVO (sin ciclos; UN paso de undo para toda
// la operacion). conectar=false -> "Keep Offset" (el hueso no se mueve, queda la linea punteada);
// conectar=true -> "Connected" (se MUEVE para que su head quede pegado al tail del activo y queda
// soldado). false = no habia nada que emparentar.
bool Bone2DParentarSeleccionAlActivo(Mesh* m, bool conectar) {
    if (!m) return false;
    int act = m->Arm2DBoneActivo();
    if (act < 0 || act >= (int)m->Arm2DHuesos().size()) return false;
    std::vector<int> aParentar;
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++) {
        if ((int)i == act || !m->Arm2DHuesos()[i].select) continue;
        if (Bone2DEsDescendiente(m, (int)i, act)) continue;  // el activo desciende de i -> ciclo
        if (!conectar && m->Arm2DHuesos()[i].padre == act && !m->Arm2DHuesos()[i].conectado) continue; // ya esta asi
        if (conectar && m->Arm2DHuesos()[i].padre == act && Bone2DEsConectado(m, (int)i)) continue; // ya esta asi
        aParentar.push_back((int)i);
    }
    if (aParentar.empty()) return false;
    UndoBones2DCapturar(m);
    for (size_t k = 0; k < aParentar.size(); k++) {
        m->Arm2DHuesos()[aParentar[k]].padre = act;
        m->Arm2DHuesos()[aParentar[k]].conectado = false;       // keep offset (sin soldar)
        if (conectar) Bone2DSoldarRaw(m, aParentar[k]);      // Connected: pega el head al tail del padre
    }
    Bone2DSanearConectado(m);   // red de seguridad del invariante (los descendientes tambien se movieron)
    Bone2DReordenarTopo(m);
    g_redraw = true;
    return true;
}

// renombra el hueso 2D (nombre unico en el armature) Y el UV GROUP homonimo del MISMO mesh
// (binding por nombre, como BoneRenombrar del 3D con su vertex group). Los vertex groups NO se
// tocan: el hueso 2D no los usa. Undo en 2 pasos (huesos + rename del grupo).
bool Bone2DRenombrar(Mesh* m, int idx, const std::string& nuevo) {
    if (!m || idx < 0 || idx >= (int)m->Arm2DHuesos().size() || nuevo.empty()) return false;
    std::string viejo = m->Arm2DHuesos()[idx].nombre;
    if (viejo == nuevo) return false;
    // el propio hueso conserva su nombre (renombrarlo al mismo valor no debe dar .001).
    // Y el nombre tiene que quedar libre en LAS DOS PUNTAS del binding: uniquificar solo entre
    // huesos 2D dejaba DOS UV groups homonimos cuando el nombre pedido ya era de otro UV group.
    // Los UV groups solo cuentan si el rename los va a ARRASTRAR (hay uno homonimo): sin arrastre,
    // ponerle a un hueso el nombre de un UV group existente es la forma de LIGARLOS a mano.
    bool arrastra = false;
    for (size_t g = 0; g < m->uvGroups.size() && !arrastra; g++)
        if (m->uvGroups[g] && m->uvGroups[g]->nombre == viejo) arrastra = true;
    std::vector<std::string> tomados;
    for (size_t a = 0; a < m->armatures2d.size(); a++) {
        Armature2D* arm = m->armatures2d[a]; if (!arm) continue;
        for (size_t i = 0; i < arm->huesos.size(); i++) {
            if ((int)a == m->armature2dActivo && (int)i == idx) continue;   // el propio hueso
            tomados.push_back(arm->huesos[i].nombre);
        }
    }
    if (arrastra)
        for (size_t g = 0; g < m->uvGroups.size(); g++)
            if (m->uvGroups[g] && m->uvGroups[g]->nombre != viejo) tomados.push_back(m->uvGroups[g]->nombre);
    std::string unico = W3dNombreUnicoEnValores(nuevo, "Bone", tomados, -1);
    // UN SOLO paso de undo para las dos puntas del binding (hueso + UV groups homonimos). En dos
    // pasos separados, el primer Ctrl+Z devolvia el nombre del hueso y dejaba los grupos con el
    // nuevo -> binding roto (el rig dejaba de deformar) hasta el segundo Ctrl+Z.
    // (malla, lista UVGroup, INDICE), NO std::string*: un puntero al nombre del grupo daba un
    // destino 'Directo', que W3dNombrePunteroVivo no reconoce nunca -> el RenameUndo quedaba
    // MUERTO y el Ctrl+Z devolvia el nombre del HUESO dejando el del GRUPO con el nuevo (las dos
    // puntas con nombres distintos = binding roto, y sin arreglo posible: el viejo se perdia).
    std::vector<W3dRenameDest> aRenombrar;
    for (size_t g = 0; g < m->uvGroups.size(); g++)
        if (m->uvGroups[g] && m->uvGroups[g]->nombre == viejo)
            aRenombrar.push_back(W3dDestCapaMalla(m, W3dRenameDest::UVGroup, (int)g));
    UndoBone2DRenameCapturar(m, aRenombrar); // snapshot de huesos (trae el nombre viejo) + esos grupos
    m->Arm2DHuesos()[idx].nombre = unico;
    const int nvg = (int)aRenombrar.size();
    for (size_t k = 0; k < aRenombrar.size(); k++)
        if (std::string* p = W3dDestResolver(aRenombrar[k])) *p = unico;
    if (nvg > 0) W3dAvisoArrastre(nvg, "UV group(s)", viejo, unico);   // ver W3dAviso.h
    g_redraw = true;
    return true;
}

// mueve (du,dv) las puntas SELECCIONADAS (con la soldadura de Bone2DSeleccionMasks) en UN paso
// de undo. Lo usa la fila Pos de la tarjeta "Armature 2D" del panel Properties.
bool Bone2DMoverSeleccion(Mesh* m, float du, float dv) {
    if (!m || (du == 0.0f && dv == 0.0f)) return false;
    std::vector<int> bones, masks;
    if (!Bone2DSeleccionMasks(m, bones, masks)) return false;
    UndoBones2DCapturar(m);
    for (size_t k = 0; k < bones.size(); k++) {
        W3dBone2D& b = m->Arm2DHuesos()[bones[k]];
        if (masks[k] & 1) { b.headU += du; b.headV += dv; }
        if (masks[k] & 2) { b.tailU += du; b.tailV += dv; }
    }
    g_redraw = true;
    return true;
}

bool UVEditor::Bone2DXformActivo() const { return gB2D.modo != 0; }

// A / Alt+A en los modos Huesos/Pose: TODOS los huesos (con sus dos puntas) o ninguno. Con la
// seleccion completa las ops (G/R/S, X, Shift+D, Ctrl+P) toman todo. Publico: lo llaman el
// teclado y el harness.
void UVEditor::Bone2DSeleccionarTodos(Mesh* m, bool sel) {
    if (!m) return;
    if (!sel) { Bone2DSelLimpiar(m); m->Arm2DBoneActivo() = -1; }
    else {
        for (size_t i = 0; i < m->Arm2DHuesos().size(); i++) Bone2DSelHueso(m, (int)i, true);
        if (m->Arm2DBoneActivo() < 0 && !m->Arm2DHuesos().empty()) m->Arm2DBoneActivo() = 0; // que haya activo
    }
    g_redraw = true;
}

// A / Alt+A en EDICION de UVs: seleccionar / deseleccionar TODO. En SYNC la seleccion del UV es
// un espejo de la del 3D -> se toca la del 3D y se re-deriva. Fuera de sync se escribe uvSelVert
// pero SOLO lo VISIBLE (las esquinas de las caras del filtro 3D): "todo" es todo lo que se ve, no
// el mapa entero (seleccionar UVs invisibles y despues moverlos sin verlos no tiene sentido).
void UVEditor::SeleccionarTodoUV(Mesh* m, bool sel) {
    if (!m || m->vertexSize <= 0) return;
    if (syncSelection) { m->EditSeleccionarTodo(sel); SincronizarSelDesde3D(m); g_redraw = true; return; }
    const int nV = m->vertexSize;
    m->uvSelVert.assign((size_t)nV, 0);
    if (sel) {
        std::vector<char> fsel; UVCarasFiltro3D(m, fsel);
        for (size_t f = 0; f < m->faces3d.size() && f < fsel.size(); f++) {
            if (!fsel[f]) continue;
            const std::vector<int>& id = m->faces3d[f].idx;
            for (size_t c = 0; c < id.size(); c++) { int k = id[c];
                if (k >= 0 && k < nV) m->uvSelVert[k] = 1; }
        }
    }
    g_redraw = true;
}

// Ctrl+I en EDICION de UVs: INVERTIR la seleccion. En SYNC se invierte la del 3D (la del UV es su
// espejo) y se re-deriva. Fuera de sync se invierte uvSelVert PERO SOLO DENTRO DE LO VISIBLE: las
// esquinas que no pasan el filtro del 3D quedan SIEMPRE en 0 (lo que no se ve no se selecciona,
// misma regla que el "todo" de SeleccionarTodoUV y que la inicializacion del filtro).
void UVEditor::InvertirSeleccionUV(Mesh* m) {
    if (!m || m->vertexSize <= 0) return;
    if (syncSelection) { InvertirSeleccion(); SincronizarSelDesde3D(m); g_redraw = true; return; }
    const int nV = m->vertexSize;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign((size_t)nV, 0);
    std::vector<char> fsel; UVCarasFiltro3D(m, fsel);
    // sin Edit Mode no hay filtro: se ve TODO el mapa (misma regla que el pick)
    if ((Object*)m != g_editMesh) fsel.assign(m->faces3d.size(), 1);
    std::vector<unsigned char> visible((size_t)nV, 0);
    for (size_t f = 0; f < m->faces3d.size() && f < fsel.size(); f++) {
        if (!fsel[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) { int k = id[c];
            if (k >= 0 && k < nV) visible[k] = 1; }
    }
    for (int i = 0; i < nV; i++)
        m->uvSelVert[i] = visible[i] ? (m->uvSelVert[i] ? 0 : 1) : 0;
    g_redraw = true;
}

// ============================================================================================
//  SELECT LINKED (L / menu Select) en EDICION de UVs: la ISLA UV conectada.
//
//  ADYACENCIA POR BORDE UV, NUNCA por posicion 3D. Dos caras estan en la MISMA isla si comparten
//  un borde de la malla (el mismo par de vertices 3D) Y ADEMAS las UV de esas dos esquinas
//  COINCIDEN en las dos caras. Si no coinciden hay una COSTURA: el mapa se ABRE ahi y son islas
//  DISTINTAS aunque compartan los vertices 3D. Es el mismo modelo de seleccion propio del UV
//  (una copia de una costura no arrastra a sus hermanas).
//
//  Por que la clave NO puede ser "comparten el indice de render-vert": en una malla FLAT (un cubo)
//  cada cara tiene sus PROPIAS copias de render de cada esquina (24 verts / 6 caras) -> ningun par
//  de caras compartiria indice y cada cara seria una isla suelta. Por eso la clave del borde se
//  arma con el VERTICE 3D (posRep) + las DOS UV, cuantizadas para no pelearse con el ultimo bit
//  del float (dos esquinas de la misma costura tienen el UV copiado, no calculado).
// ============================================================================================
static const float UV_ISLA_CUANT = 16384.0f; // ~6e-5 de tolerancia al comparar las UV de un borde
static inline int UVCuant(float x) { return (int)floorf(x * UV_ISLA_CUANT + 0.5f); }

// un borde de UNA cara con su clave de union (el borde 3D + las UV de sus dos esquinas)
struct UVBordeIsla {
    int rA, rB;           // vertices 3D (posRep) del borde, ordenados: rA <= rB
    int uA, vA, uB, vB;   // UV cuantizadas de la esquina de rA y de la de rB (en ese orden)
    int cara;             // que cara aporto este borde
};
static bool UVBordeMismaClave(const UVBordeIsla& a, const UVBordeIsla& b) {
    return a.rA == b.rA && a.rB == b.rB && a.uA == b.uA && a.vA == b.vA && a.uB == b.uB && a.vB == b.vB;
}
static bool UVBordeMenor(const UVBordeIsla& a, const UVBordeIsla& b) {
    if (a.rA != b.rA) return a.rA < b.rA;
    if (a.rB != b.rB) return a.rB < b.rB;
    if (a.uA != b.uA) return a.uA < b.uA;
    if (a.vA != b.vA) return a.vA < b.vA;
    if (a.uB != b.uB) return a.uB < b.uB;
    return a.vB < b.vB;
}

bool UVEditor::UVSeleccionarVinculado(Mesh* m, int semilla, bool extender) {
    if (!m || m->vertexSize <= 0 || m->faces3d.empty() || !m->uv) return false;
    // SYNC ON (modo ESPEJO): la seleccion del UV es la del 3D, asi que el "vinculado" es el DEL 3D
    // (EditMesh::SeleccionarLinked, la isla por aristas de la malla) sobre el elemento ACTIVO --
    // el mismo criterio que los Loop Select del menu del viewport 3D. Despues se re-espeja.
    if (syncSelection) {
        m->EnsureEdit();
        if (!m->edit) return false;
        EditMesh* e = m->edit;
        int k = e->activeIdx;
        if (k < 0) { // sin activo: el primer elemento seleccionado del modo del 3D
            const std::vector<unsigned char>& sel = (EditSelectMode == SelFace) ? e->faceSel :
                                                    (EditSelectMode == SelEdge) ? e->edgeSel : e->vertSel;
            for (size_t i = 0; i < sel.size(); i++) if (sel[i]) { k = (int)i; break; }
        }
        if (k < 0) return false;
        e->SeleccionarLinked(k, EditSelectMode, !extender);
        SincronizarSelDesde3D(m);
        g_redraw = true;
        return true;
    }
    const int nV = m->vertexSize;
    const int nF = (int)m->faces3d.size();
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign((size_t)nV, 0);
    // solo se recorre lo VISIBLE (el filtro del 3D): una isla no se propaga por caras que el UV
    // no dibuja, y nunca queda seleccionado algo que no se ve. Sin Edit Mode no hay filtro que
    // seguir -> se ve todo el mapa (misma regla que el pick).
    std::vector<char> fsel; UVCarasFiltro3D(m, fsel);
    if ((Object*)m != g_editMesh) fsel.assign((size_t)nF, 1);
    const bool hayRep = ((int)m->posRep.size() == nV);

    // 1) todos los bordes de las caras visibles, con su clave
    std::vector<UVBordeIsla> bordes;
    for (int f = 0; f < nF; f++) {
        if (f < (int)fsel.size() && !fsel[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        const int nc = (int)id.size();
        if (nc < 2) continue;
        for (int c = 0; c < nc; c++) {
            int ka = id[c], kb = id[(c + 1) % nc];
            if (ka < 0 || ka >= nV || kb < 0 || kb >= nV) continue;
            int ra = hayRep ? m->posRep[ka] : ka;
            int rb = hayRep ? m->posRep[kb] : kb;
            if (ra == rb) continue;                 // borde degenerado
            UVBordeIsla e2;
            e2.cara = f;
            if (ra <= rb) { e2.rA = ra; e2.rB = rb;
                            e2.uA = UVCuant(m->uv[ka*2]); e2.vA = UVCuant(m->uv[ka*2+1]);
                            e2.uB = UVCuant(m->uv[kb*2]); e2.vB = UVCuant(m->uv[kb*2+1]); }
            else          { e2.rA = rb; e2.rB = ra;
                            e2.uA = UVCuant(m->uv[kb*2]); e2.vA = UVCuant(m->uv[kb*2+1]);
                            e2.uB = UVCuant(m->uv[ka*2]); e2.vB = UVCuant(m->uv[ka*2+1]); }
            bordes.push_back(e2);
        }
    }
    // 2) mismas claves juntas -> esas caras son vecinas EN EL UV (comparten el borde ya cosido)
    std::sort(bordes.begin(), bordes.end(), UVBordeMenor);
    std::vector<std::vector<int> > ady((size_t)nF);
    for (size_t i = 0; i < bordes.size(); ) {
        size_t j = i + 1;
        while (j < bordes.size() && UVBordeMismaClave(bordes[i], bordes[j])) j++;
        for (size_t a = i; a < j; a++)              // grupos de 2 (o mas, en no-manifold)
            for (size_t b = a + 1; b < j; b++)
                if (bordes[a].cara != bordes[b].cara) {
                    ady[bordes[a].cara].push_back(bordes[b].cara);
                    ady[bordes[b].cara].push_back(bordes[a].cara);
                }
        i = j;
    }
    // 3) SEMILLA: la(s) cara(s) del render-vert pedido (tecla L, el de abajo del cursor) o, si no
    //    se paso ninguno (menu Select), todas las caras visibles con alguna esquina seleccionada.
    std::vector<unsigned char> enIsla((size_t)nF, 0);
    std::vector<int> pila;
    for (int f = 0; f < nF; f++) {
        if (f < (int)fsel.size() && !fsel[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        bool sem = false;
        for (size_t c = 0; c < id.size() && !sem; c++) {
            int k = id[c]; if (k < 0 || k >= nV) continue;
            sem = (semilla >= 0) ? (k == semilla) : (m->uvSelVert[k] != 0);
        }
        if (sem && !enIsla[f]) { enIsla[f] = 1; pila.push_back(f); }
    }
    if (pila.empty()) return false;                 // sin semilla no hay isla que buscar
    // 4) flood fill por los bordes UV cosidos
    while (!pila.empty()) {
        int f = pila.back(); pila.pop_back();
        const std::vector<int>& vec = ady[f];
        for (size_t j = 0; j < vec.size(); j++)
            if (!enIsla[vec[j]]) { enIsla[vec[j]] = 1; pila.push_back(vec[j]); }
    }
    // 5) resultado: TODAS las esquinas de las caras de la isla (la seleccion del UV es por
    //    render-vert; el modo vertex/edge/face solo cambia como se pickea, no como se guarda)
    if (!extender) m->uvSelVert.assign((size_t)nV, 0);
    for (int f = 0; f < nF; f++) {
        if (!enIsla[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) { int k = id[c];
            if (k >= 0 && k < nV) m->uvSelVert[k] = 1; }
    }
    g_redraw = true;
    return true;
}

// render-vert VISIBLE mas cercano al cursor: la semilla del Select Linked por TECLA. Sin radio
// maximo a proposito (la L del 3D tambien agarra la isla mas cercana, no exige apuntar fino).
int UVEditor::VertBajoCursorUV(Mesh* m) const {
    if (!m || !m->uv || m->vertexSize <= 0) return -1;
    float cx, cy, s; ParamsUV(cx, cy, s);
    const float clx = (float)(lastMx - x), cly = (float)(lastMy - y);
    std::vector<char> fsel; UVCarasFiltro3D(m, fsel);
    const bool enEdit = ((Object*)m == g_editMesh);
    int mejor = -1; float mejorD = 0.0f;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const bool vis = enEdit ? (f < fsel.size() && fsel[f] != 0) : true;
        if (!syncSelection && !vis) continue;       // fuera de sync solo cuenta lo que se dibuja
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c]; if (k < 0 || k >= m->vertexSize) continue;
            float sx, sy; UVtoScreen(m->uv[k*2], m->uv[k*2+1], cx, cy, s, sx, sy);
            float d = (sx - clx)*(sx - clx) + (sy - cly)*(sy - cly);
            if (mejor < 0 || d < mejorD) { mejorD = d; mejor = k; }
        }
    }
    return mejor;
}

// Ctrl+I en Huesos/Pose: invierte la seleccion de huesos 2D. Un hueso cuenta como SELECCIONADO si
// tiene algo prendido (el hueso entero o alguna punta) -> queda deseleccionado; el resto entra
// ENTERO (con sus dos puntas), que es lo que esperan las ops (G/R/S, X, Shift+D, Ctrl+P).
void UVEditor::Bone2DInvertirSeleccion(Mesh* m) {
    if (!m || m->Arm2DHuesos().empty()) return;
    const int n = (int)m->Arm2DHuesos().size();
    std::vector<unsigned char> antes((size_t)n, 0);
    for (int i = 0; i < n; i++) {
        const W3dBone2D& b = m->Arm2DHuesos()[i];
        antes[i] = (b.select || b.selHead || b.selTail) ? 1 : 0;
    }
    Bone2DSelLimpiar(m);
    for (int i = 0; i < n; i++) if (!antes[i]) Bone2DSelHueso(m, i, true);
    // el activo que quedo deseleccionado se apaga (igual que el Invert de Pose Mode del 3D)
    if (m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < n && !m->Arm2DHuesos()[m->Arm2DBoneActivo()].select)
        m->Arm2DBoneActivo() = -1;
    g_redraw = true;
}

// Select Linked (L) en Huesos/Pose: la CADENA conectada. Recorre el ARBOL del armature 2D en los
// DOS sentidos (al padre y a los hijos) desde cada hueso con algo seleccionado (o desde el activo
// si no hay seleccion) -> agarra la cadena entera y NO las otras cadenas del mismo armature.
// El flag 'conectado' (soldadura de puntas) NO importa: un hijo emparentado pero separado sigue
// siendo parte de la misma cadena (es lo que dice el arbol, que es lo que se anima).
bool UVEditor::Bone2DSeleccionarVinculado(Mesh* m, bool extender) {
    if (!m || m->Arm2DHuesos().empty()) return false;
    const int n = (int)m->Arm2DHuesos().size();
    std::vector<unsigned char> enCadena((size_t)n, 0);
    std::vector<int> pila;
    for (int i = 0; i < n; i++) {
        const W3dBone2D& b = m->Arm2DHuesos()[i];
        if (b.select || b.selHead || b.selTail) { enCadena[i] = 1; pila.push_back(i); }
    }
    if (pila.empty() && m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < n) {
        enCadena[m->Arm2DBoneActivo()] = 1; pila.push_back(m->Arm2DBoneActivo());
    }
    if (pila.empty()) return false;
    while (!pila.empty()) {
        int b = pila.back(); pila.pop_back();
        int p = m->Arm2DHuesos()[b].padre;                       // hacia arriba
        if (p >= 0 && p < n && !enCadena[p]) { enCadena[p] = 1; pila.push_back(p); }
        for (int c = 0; c < n; c++)                            // hacia abajo
            if (m->Arm2DHuesos()[c].padre == b && !enCadena[c]) { enCadena[c] = 1; pila.push_back(c); }
    }
    if (!extender) Bone2DSelLimpiar(m);
    for (int i = 0; i < n; i++) if (enCadena[i]) Bone2DSelHueso(m, i, true);
    if (m->Arm2DBoneActivo() < 0) {                                 // que quede un activo de la cadena
        for (int i = 0; i < n; i++) if (enCadena[i]) { m->Arm2DBoneActivo() = i; break; }
    }
    g_redraw = true;
    return true;
}

// snapshot comun del transform modal de huesos (edicion o pose segun el modo del editor).
// masks (solo edicion): que punta de cada hueso se mueve; el PIVOTE es la mediana de las puntas
// EN MOVIMIENTO sin contar dos veces la compartida soldada (como BoneGrabMediana del 3D).
static void B2DSnapshot(UVEditor* uv, Mesh* m, const std::vector<int>& bones,
                        const std::vector<int>& masks, int modo, bool porClick) {
    gB2D.uv = uv; gB2D.modo = modo; gB2D.porClick = porClick;
    gB2D.eje = 0;                       // cada transform arranca SIN bloqueo (como el de UVs)
    gB2D.enPose = (uv->uvModo == UVModoPose);
    gB2D.bones = bones;
    gB2D.masks = masks;
    if (gB2D.masks.size() != bones.size()) gB2D.masks.assign(bones.size(), 3);
    gB2D.snap.clear();
    double su = 0, sv = 0; int np = 0;
    if (gB2D.enPose) {
        std::vector<float> M; m->Armature2DMatrices(M);
        for (size_t k = 0; k < bones.size(); k++) {
            const W3dBone2D& b = m->Arm2DHuesos()[bones[k]];
            gB2D.snap.push_back(b.poseTU); gB2D.snap.push_back(b.poseTV);
            gB2D.snap.push_back(b.poseRot);
            gB2D.snap.push_back(b.poseSX); gB2D.snap.push_back(b.poseSY);
            float hu, hv, tu, tv; Bone2DExtremos(m, M, (size_t)bones[k], true, hu, hv, tu, tv);
            su += hu; sv += hv; np++;      // pivote de pose: mediana de los HEAD posados
        }
    } else {
        // que TAILS se mueven (para no contar doble la punta compartida soldada en la mediana)
        std::vector<char> tailMueve(m->Arm2DHuesos().size(), 0);
        for (size_t k = 0; k < bones.size(); k++)
            if (gB2D.masks[k] & 2) tailMueve[bones[k]] = 1;
        for (size_t k = 0; k < bones.size(); k++) {
            const W3dBone2D& b = m->Arm2DHuesos()[bones[k]];
            gB2D.snap.push_back(b.headU); gB2D.snap.push_back(b.headV);
            gB2D.snap.push_back(b.tailU); gB2D.snap.push_back(b.tailV);
            if (gB2D.masks[k] & 1) {
                int p = b.padre;
                bool compartida = Bone2DEsConectado(m, bones[k]) &&
                                  p >= 0 && p < (int)m->Arm2DHuesos().size() && tailMueve[p];
                if (!compartida) { su += b.headU; sv += b.headV; np++; }
            }
            if (gB2D.masks[k] & 2) { su += b.tailU; sv += b.tailV; np++; }
        }
    }
    gB2D.pivU = np ? (float)(su / np) : 0.5f;
    gB2D.pivV = np ? (float)(sv / np) : 0.5f;
    UVMouseAUV(uv, uv->lastMx, uv->lastMy, gB2D.startU, gB2D.startV);
    gB2D.curU = gB2D.startU; gB2D.curV = gB2D.startV;       // cursor virtual (wrap-safe)
    gB2D.valU = gB2D.valV = gB2D.valAng = 0.0f; gB2D.valFac = 1.0f;
    NumInputReset(); // sin restos de una entrada numerica anterior
    g_redraw = true;
}

// G/R/S sobre la SELECCION (puntas y/o huesos enteros, con soldadura; en pose los huesos
// seleccionados enteros). Modal: motion aplica, click/Enter confirma.
void UVEditor::Bone2DXformStart(Mesh* m, int modo) {
    if (!m || m->Arm2DHuesos().empty() || (uvModo != UVModoHuesos && uvModo != UVModoPose)) return;
    std::vector<int> bones, masks;
    if (uvModo == UVModoPose) {
        // POSE: siempre huesos ENTEROS (la pose es del hueso, no de una punta)
        for (size_t i = 0; i < m->Arm2DHuesos().size(); i++) if (m->Arm2DHuesos()[i].select) bones.push_back((int)i);
        if (bones.empty() && m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < (int)m->Arm2DHuesos().size())
            bones.push_back(m->Arm2DBoneActivo());
        masks.assign(bones.size(), 3);
    } else if (!Bone2DSeleccionMasks(m, bones, masks)) return; // edicion: puntas + soldadura
    if (bones.empty()) return;
    UndoBones2DIniciar(m); // pendiente hasta confirmar (ANTES del RestCapturar: snapshot pre-op)
    if (uvModo == UVModoPose) m->Armature2DRestCapturar(); // el rest queda fijo ANTES de posar
    B2DSnapshot(this, m, bones, masks, modo, false);
    // MRU de la toolbar: teclado y toolbar alimentan el mismo historial (como en edicion)
    ToolbarMRU(UVToolHist(), (modo == 1) ? TBMove : (modo == 2) ? TBRotate : TBScale);
}

// drag de LO SELECCIONADO (puntas/huesos con soldadura): lo arranca el click de seleccion.
void UVEditor::Bone2DDragSeleccion(Mesh* m, bool porClick) {
    if (!m || uvModo != UVModoHuesos) return;
    std::vector<int> bones, masks;
    if (!Bone2DSeleccionMasks(m, bones, masks)) return;
    UndoBones2DIniciar(m); // pendiente hasta confirmar (el drag es UN paso de undo)
    B2DSnapshot(this, m, bones, masks, 1, porClick);
}

// deja seleccionada SOLO esa punta (mask 1=head 2=tail; 3 = hueso entero) y arranca su drag.
// Lo usan el extrude (tail nuevo agarrado al mouse) y el harness.
void UVEditor::Bone2DDragEnd(Mesh* m, int bone, int mask, bool porClick) {
    if (!m || uvModo != UVModoHuesos || bone < 0 || bone >= (int)m->Arm2DHuesos().size()) return;
    Bone2DSelLimpiar(m);
    if (mask == 1 || mask == 2) Bone2DSelPunta(m, bone, mask, true);
    else                        Bone2DSelHueso(m, bone, true);
    Bone2DDragSeleccion(m, porClick);
}

// NUCLEO del apply de huesos: recibe los VALORES ya resueltos (delta / angulo en grados /
// factor) y los aplica desde el snapshot. Lo comparten el mouse (Bone2DXformDelta) y la
// entrada numerica exacta (XformNumValor), sin duplicar los loops.
// El BLOQUEO DE EJE (gB2D.eje) se aplica ACA ADENTRO, igual que el uvXAxis del transform de
// UVs (AplicarXformValores): mover filtra la componente del eje libre, escalar deja el otro
// eje en 1.0 y rotar lo ignora (una rotacion 2D no tiene eje).
static void B2DAplicarValores(Mesh* m, float du, float dv, float ang, float f) {
    if (!gB2D.modo || !m) return;
    if (gB2D.modo == 1) {                        // MOVER: el lock anula el otro eje
        if (gB2D.eje == 1) dv = 0.0f;
        if (gB2D.eje == 2) du = 0.0f;
    }
    // ESCALAR: factor POR EJE (bloqueo X -> Y no escala, y al reves). Mismo criterio que el UV.
    const float fx = (gB2D.modo == 3 && gB2D.eje == 2) ? 1.0f : f;
    const float fy = (gB2D.modo == 3 && gB2D.eje == 1) ? 1.0f : f;
    gB2D.valU = du; gB2D.valV = dv; gB2D.valAng = ang; gB2D.valFac = f; // barra de INFO
    const float DEG = 3.14159265358979f / 180.0f;
    float c = cosf(ang * DEG), s2 = sinf(ang * DEG);
    for (size_t k = 0; k < gB2D.bones.size(); k++) {
        int bi = gB2D.bones[k];
        if (bi < 0 || bi >= (int)m->Arm2DHuesos().size()) continue;
        W3dBone2D& b = m->Arm2DHuesos()[bi];
        const float* sn = &gB2D.snap[k * (gB2D.enPose ? 5 : 4)];
        if (gB2D.enPose) {
            // POSE 2D: G suma traslacion, R suma rotacion (alrededor del head), S multiplica escala
            b.poseTU = sn[0]; b.poseTV = sn[1]; b.poseRot = sn[2]; b.poseSX = sn[3]; b.poseSY = sn[4];
            if (gB2D.modo == 1)      { b.poseTU += du; b.poseTV += dv; }
            else if (gB2D.modo == 2) { b.poseRot += ang; }
            else                     { b.poseSX *= fx; b.poseSY *= fy; }
        } else {
            // EDICION del rest: mueve/rota/escala SOLO las puntas del mask (la seleccion por
            // puntas + soldadura), alrededor del pivote (mediana de las puntas en movimiento)
            const int mk = (k < gB2D.masks.size()) ? gB2D.masks[k] : 3;
            float hu = sn[0], hv = sn[1], tu = sn[2], tv = sn[3];
            if (gB2D.modo == 1) {
                if (mk & 1) { hu += du; hv += dv; }
                if (mk & 2) { tu += du; tv += dv; }
            } else if (gB2D.modo == 2) {
                if (mk & 1) { float ru = hu - gB2D.pivU, rv = hv - gB2D.pivV;
                              hu = gB2D.pivU + ru*c - rv*s2; hv = gB2D.pivV + ru*s2 + rv*c; }
                if (mk & 2) { float ru = tu - gB2D.pivU, rv = tv - gB2D.pivV;
                              tu = gB2D.pivU + ru*c - rv*s2; tv = gB2D.pivV + ru*s2 + rv*c; }
            } else {
                if (mk & 1) { hu = gB2D.pivU + (hu - gB2D.pivU) * fx; hv = gB2D.pivV + (hv - gB2D.pivV) * fy; }
                if (mk & 2) { tu = gB2D.pivU + (tu - gB2D.pivU) * fx; tv = gB2D.pivV + (tv - gB2D.pivV) * fy; }
            }
            b.headU = hu; b.headV = hv; b.tailU = tu; b.tailV = tv;
        }
    }
    if (gB2D.enPose) { m->Armature2DRestCapturar(); m->Armature2DAplicar(); } // los UV pesados siguen al hueso
    g_redraw = true;
}

// aplica el transform EN VIVO desde el snapshot (delta absoluto: no acumula drift): deriva
// delta/angulo/factor de la posicion del cursor (virtual) y delega en B2DAplicarValores.
void UVEditor::Bone2DXformDelta(Mesh* m, float curU, float curV) {
    if (!gB2D.modo || !m) return;
    float du = curU - gB2D.startU, dv = curV - gB2D.startV;
    float ang = 0.0f, f = 1.0f;
    if (gB2D.modo == 2 || gB2D.modo == 3) {
        float a0 = atan2f(gB2D.startV - gB2D.pivV, gB2D.startU - gB2D.pivU);
        float a1 = atan2f(curV - gB2D.pivV, curU - gB2D.pivU);
        ang = (a1 - a0) * 180.0f / 3.14159265f;
        float d0 = sqrtf((gB2D.startU-gB2D.pivU)*(gB2D.startU-gB2D.pivU) + (gB2D.startV-gB2D.pivV)*(gB2D.startV-gB2D.pivV));
        float d1 = sqrtf((curU-gB2D.pivU)*(curU-gB2D.pivU) + (curV-gB2D.pivV)*(curV-gB2D.pivV));
        f = (d0 > 1e-5f) ? d1 / d0 : 1.0f;
    }
    B2DAplicarValores(m, du, dv, ang, f);
}

void UVEditor::Bone2DXformConfirm(Mesh* m) {
    if (!gB2D.modo) return;
    UndoBones2DConfirmar(); // pushea el pendiente (descarta si no cambio nada: G+click sin mover)
    bool enPose = gB2D.enPose;
    // el snapshot de la pose de ANTES del transform (5 floats por hueso) lo necesita el AUTO KEY por
    // canal, y abajo se limpia: copiarlo primero.
    std::vector<int>   b0 = gB2D.bones;
    std::vector<float> s0 = gB2D.snap;
    gB2D.modo = 0; gB2D.eje = 0; gB2D.uv = NULL; gB2D.porClick = false;
    gB2D.bones.clear(); gB2D.masks.clear(); gB2D.snap.clear();
    if (m && enPose) {
        m->skinGeomVersion++; // los UV cambiaron -> re-subir el VBO de uv en Object Mode
        // AUTO KEY de la POSE 2D. DOS caminos, y el orden importa:
        //  1) la malla ya tiene CLIPS del armature 2D (o no tiene ninguna animacion todavia): se
        //     keyframean las CURVAS POR HUESO (posU/posV/rot/escalaX/escalaY), solo los canales que
        //     cambiaron. Es el camino nuevo: 5 floats por hueso movido en vez del array uv entero.
        //  2) RETROCOMPAT: proyectos viejos donde la pose 2D se horneaba en la capa uv de la vertex
        //     anim ACTIVA (kind 3). Si eso es lo que esta activo en el timeline y la malla NO tiene
        //     clips 2D, se sigue horneando igual que siempre -> los .w3d de antes siguen andando.
        extern bool AutoKeyOn; extern int ActiveAnimKind; extern Mesh* ActiveAnimMesh;
        extern void VertexAnimInsertarKeyframeUV();
        extern void AnimSelArm2D(Mesh*, int);
        if (AutoKeyOn && (Object*)m == g_editMesh) {
            bool horneado = (ActiveAnimKind == 3 && (Mesh*)ActiveAnimMesh == m && m->Arm2DAnims().empty());
            if (horneado) {
                VertexAnimInsertarKeyframeUV();
            } else if (!b0.empty() && s0.size() >= b0.size() * 5 && AutoKeyArm2DPrep(m)) {
                int n = 0;
                for (size_t k = 0; k < b0.size(); k++)
                    n += AutoKeyHueso2D(m, b0[k], s0[k*5+0], s0[k*5+1], s0[k*5+2], s0[k*5+3], s0[k*5+4]);
                if (n > 0) {
                    AutoKeyArm2DFin(m);
                    if (ActiveAnimKind != 4 || ActiveAnimMesh != m) AnimSelArm2D(m, m->Arm2DAnimActiva());
                }
            }
        }
    }
    g_redraw = true;
}

void UVEditor::Bone2DXformCancel(Mesh* m) {
    if (!gB2D.modo) return;
    if (m) { // restaurar el snapshot
        for (size_t k = 0; k < gB2D.bones.size(); k++) {
            int bi = gB2D.bones[k];
            if (bi < 0 || bi >= (int)m->Arm2DHuesos().size()) continue;
            W3dBone2D& b = m->Arm2DHuesos()[bi];
            const float* sn = &gB2D.snap[k * (gB2D.enPose ? 5 : 4)];
            if (gB2D.enPose) { b.poseTU = sn[0]; b.poseTV = sn[1]; b.poseRot = sn[2]; b.poseSX = sn[3]; b.poseSY = sn[4]; }
            else             { b.headU = sn[0]; b.headV = sn[1]; b.tailU = sn[2]; b.tailV = sn[3]; }
        }
        if (gB2D.enPose) m->Armature2DAplicar(); // los UV vuelven a la pose previa
    }
    UndoBones2DCancelar(); // el pendiente se descarta (no hubo operacion)
    gB2D.modo = 0; gB2D.eje = 0; gB2D.uv = NULL; gB2D.porClick = false;
    gB2D.bones.clear(); gB2D.masks.clear(); gB2D.snap.clear();
    g_redraw = true;
}

// pick de hueso 2D bajo el mouse (px LOCALES del viewport). pose=true usa los extremos POSADOS
// y devuelve el hueso ENTERO (en pose no hay puntas). En EDICION porta el modelo del 3D
// (BonePickEdit): las PUNTAS tienen prioridad (radio 12px) sobre el cuerpo (14px) -> click cerca
// de una punta selecciona SOLO esa punta. outMask: 1=head 2=tail 3=cuerpo/entero.
int UVEditor::Bone2DPick(Mesh* m, float lx, float ly, bool pose, int* outMask) {
    if (outMask) *outMask = 3;
    if (!m || m->Arm2DHuesos().empty()) return -1;
    float cx, cy, s; ParamsUV(cx, cy, s);
    std::vector<float> M;
    if (pose) m->Armature2DMatrices(M);
    int bestP = -1, bestPMask = 0;
    float bestPD = 12.0f * 12.0f;    // mejor PUNTA (radio 12 px; distancias^2)
    int bestB = -1;
    float bestBD = 14.0f * 14.0f;    // mejor CUERPO (14 px, como el pick de vertices)
    for (size_t i = 0; i < m->Arm2DHuesos().size(); i++) {
        float hu, hv, tu, tv;
        Bone2DExtremos(m, M, i, pose, hu, hv, tu, tv);
        float ax, ay, bx, by;
        UVtoScreen(hu, hv, cx, cy, s, ax, ay);
        UVtoScreen(tu, tv, cx, cy, s, bx, by);
        if (!pose) { // en edicion las puntas se pickean SOLAS (prioritarias)
            float dH = (lx - ax) * (lx - ax) + (ly - ay) * (ly - ay);
            float dT = (lx - bx) * (lx - bx) + (ly - by) * (ly - by);
            if (dH < bestPD) { bestPD = dH; bestP = (int)i; bestPMask = 1; }
            if (dT < bestPD) { bestPD = dT; bestP = (int)i; bestPMask = 2; }
        }
        float d2 = DistPtSeg2(lx, ly, ax, ay, bx, by);
        if (d2 < bestBD) { bestBD = d2; bestB = (int)i; }
    }
    if (bestP >= 0) { if (outMask) *outMask = bestPMask; return bestP; }
    if (outMask) *outMask = 3;
    return bestB;
}
