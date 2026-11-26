#include "ViewPorts/UVEditor.h"
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "W3dNames.h"              // LA regla de nombres unicos (rename de hueso 2D: las dos puntas)
#include "objects/Mesh.h"          // Mesh, MaterialGroup, Material, Texture
#include "objects/EditMesh.h"      // EditMesh (seleccion de caras para el Sync Selection)
#include "objects/Textures.h"      // Textures[] (atlas de iconos = Textures[0])
#include "io/Texture2D.h"          // Texture2DGet: la UI del JUEGO en el dropdown Texture
#include "w3dGraphics.h"           // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"      // W3dScreenHeight + helpers de dibujo
#include "WhiskUI/theme/colors.h"       // ColorList / ColorID
#include "WhiskUI/draw/icons.h"         // IconType (botones-icono SelMode / Pivot)
#include "io/EditedTexture.h"           // pintura de TEXTURA (UVModeTexture): pixeles en memoria + guardado
#include "Undo.h"                        // UndoTextureStart/Confirmar (un trazo = un paso)
#include "PopUp/ColorPicker.h"           // el color del pincel de textura
#include "ViewPorts/Properties.h"  // PropsActive (parte activa seleccionada)
#include "WhiskUI/Propieties/PropList.h" // PropListMeshParts
#include "render/RenderOptions.h" // g_redraw (render event-driven)
#include "w3dTexture.h"             // w3dEngine::TextureSize (aspect ratio de la textura del UV editor)
#include "PopUp/PopUpBase.h"       // PopUpActive (file browser / popups modales tienen prioridad)
#include "edit/WeightPaint.h"
#include "ViewPorts/PopUp/FalloffEditor.h" // editor de falloff (el mismo popup que el 3D)
#include "WhiskUI/draw/icons.h"            // IconIndex      // pincel + escritura de pesos (modo UVModeWeights)
#include "ViewPorts/ViewPort3D.h"  // roles TBR_Undo/TBR_Redo de la toolbar (mismos que el 3D)
#include "ViewPorts/TransformUI.h" // UI compartida del transform (barra de info / numerico / tilde-cruz-ejes)
#include "ViewPorts/NumInput.h"    // entrada numerica compartida (NumInputActive/Reset)
#include "ViewPorts/Parent.h"      // menu Alt+P de huesos (Disconnect Bone / Clear Parent) del boton Disconnect
#include "ViewPorts/Notifications.h" // toast al renombrar el vertex group junto al hueso 2D
#include "W3dNotice.h"               // ...armado SIN desbordar (ver W3dNotice.h)
#include "Undo.h"                  // botones Deshacer/Rehacer de la toolbar del UV
#include "ViewPorts/LayoutInput.h" // InsertKeyframeContext / LayoutMenuInsertKeyframe (la I de Insert Keyframe)
#include "animation/Armature2DAnimation.h" // clips del armature 2D (auto key de la pose 2D al soltar)
#include "ViewPorts/Timeline.h"    // DopeRemapIndexKey: borrar un hueso 2D CORRE los indices que
                                   // guarda la seleccion del dope ("arm2d:<malla>/a<n>/b<IDX>")
#include "animation/Animation.h"   // KfCanal* / ActiveAnimKind
#include "objects/MeshResource.h"   // W3dMeshMarkEdited: la malla de un recurso se publica tras editar sus UV
#include <set>
#include <vector>
#include <algorithm> // std::sort (agrupar los bordes UV al buscar la isla del Select Linked)
#include <math.h> // atan2f/cosf/sinf/sqrtf (transform 2D)

namespace gfx = w3dEngine;

// REGISTRO de editores UV vivos: la tarjeta "Armature 2D" del panel Properties y los atajos
// Ctrl+P / Alt+P necesitan saber si ALGUN editor UV esta en modo de huesos, sin recorrer el
// arbol de viewports (los tests crean editores sueltos, fuera del arbol). El ctor registra,
// el dtor desregistra.
static std::vector<UVEditor*> gUVEditors;
UVEditor* UVEditorInBoneMode() {
    for (size_t i = 0; i < gUVEditors.size(); i++)
        if (gUVEditors[i] && (gUVEditors[i]->uvMode == UVModeBones || gUVEditors[i]->uvMode == UVModePose))
            return gUVEditors[i];
    return NULL;
}
// algun editor UV VIVO parado en un modo que TRABAJA CON EL RIG 2D: Edit Bones, Pose o el MODO
// OBJETO. El modo objeto entra en la lista porque es el DEFAULT y es justo donde se elige entre
// rigs: la tarjeta del panel (lista de armatures + Add/Rename/Delete) tiene que verse ahi, sino
// para renombrar o borrar un rig habia que entrar antes a huesos. Las filas de HUESO del panel
// siguen atadas a UVEditorInBoneMode (en modo objeto no se edita ningun hueso).
UVEditor* UVEditorWithRig2D() {
    for (size_t i = 0; i < gUVEditors.size(); i++)
        if (gUVEditors[i] && (gUVEditors[i]->uvMode == UVModeBones || gUVEditors[i]->uvMode == UVModePose ||
                               gUVEditors[i]->uvMode == UVModeObject))
            return gUVEditors[i];
    return NULL;
}
// SOLO el modo POSE (no Edit Bones): es el contexto en el que "Insert Keyframe" keyframea la
// POSE 2D (clips del armature 2D). Lo consulta InsertKeyframeContext para decidir a cual de
// los cuatro caminos de insert va la 'i' / el menu Animation.
UVEditor* UVEditorInPoseMode() {
    for (size_t i = 0; i < gUVEditors.size(); i++)
        if (gUVEditors[i] && gUVEditors[i]->uvMode == UVModePose) return gUVEditors[i];
    return NULL;
}
// algun editor UV PINTANDO la textura (UVModeTexture): mientras haya uno, las texturas en edicion no sueltan sus pixeles
bool UVEditorPaintingTexture() {
    for (size_t i = 0; i < gUVEditors.size(); i++)
        if (gUVEditors[i] && gUVEditors[i]->uvMode == UVModeTexture) return true;
    return false;
}
// algun editor UV en modo EDICION de UVs (el contexto donde la 'i' keyframea la capa uv)
UVEditor* UVEditorInEditModeUV() {
    for (size_t i = 0; i < gUVEditors.size(); i++)
        if (gUVEditors[i] && gUVEditors[i]->uvMode == UVModeEdit) return gUVEditors[i];
    return NULL;
}

// TAB sobre un viewport cualquiera: se lo queda un UV editor OPERATIVO (ViewportKind 4) cuya malla
// ya tiene algun armature 2D, O que este en MODO OBJETO (sino no habria como salir del modo objeto;
// ver el ciclo completo en UVEditor.h). PREDICADO puro: el toggle lo hace TabToggleBones cuando la
// tecla llega al viewport por el ruteo normal.
bool UVEditorTakeTab(ViewportBase* vp) {
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 4) return false;
    UVEditor* uv = (UVEditor*)vp;
    if (!uv->InEditUV()) return false;
    return ((Mesh*)ActiveObj)->HasArm2D() || uv->uvMode == UVModeObject;
}

UVEditor::UVEditor() {
    zoom = 1.0f;
    panX = 0.0f; panY = 0.0f;
    // SYNC SELECTION OFF por DEFAULT (ver el bloque grande del header):
    // el 3D FILTRA que caras se ven en el UV y la seleccion del UV es PROPIA y POR RENDER-VERT.
    // Con sync ON un click en una copia UV agarraba TODAS las copias del mismo vertice 3D (las
    // costuras) porque el pick iba a la seleccion del 3D y volvia expandido por posRep: inusable
    // para editar UVs (bug reportado por el dueno). El modo espejo sigue disponible en el menu View.
    syncSelection = false;
    nextFilter = 0; filterSerial = 0; filterSync = false; // sin inicializar (el 1er frame inicializa)
    repeatTexture = false;
    showChromeUV = false; // off por defecto: no gasta CPU dibujando el overlay
    lastMx = 0; lastMy = 0;
    uvXform = 0; uvXAxis = 0; uvXPivotU = uvXPivotV = uvXStartU = uvXStartV = 0.0f;
    uvXCurU = uvXCurV = 0.0f;                                 // cursor virtual (wrap-safe)
    uvXValU = uvXValV = uvXValAng = 0.0f; uvXValFac = 1.0f;   // valores para la barra de info
    uvCursorU = 0.5f; uvCursorV = 0.5f; // cursor 2D al centro por defecto
    uvSelMode = SelVertex;              // modo de seleccion propio del UV (default vertices)
    uvMode = UVModeObject;              // DEFAULT: modo OBJETO (elegir geometria / armature 2D con el click)
    uvPrevModeTex = UVModeObject;
    uvPreviousMode = UVModeObject;        // a donde vuelve el Tab al salir de Edit Bones
    uvObjArm = false;                     // objeto activo del UV: la GEOMETRIA (hasta que se clickee un hueso)
    uvBoxBuilt = false; uvBoxSel = false; uvBoxAdd = false; // box select (B) sin gesto en curso
    uvBoxX0 = uvBoxY0 = uvBoxX1 = uvBoxY1 = 0;
    BarCreate();
    // botones de barra (ademas del icono [0]), cada uno con su ROL estable (BarRoleUV): el dispatch
    // (LayoutClickBarUV) y la visibilidad (SyncBar) los buscan por rol, NO por indice.
    //
    // ORDEN = EL MISMO QUE LA BARRA DEL VIEWPORT 3D (ViewPort3D.cpp): Mode, SelMode, Pivot, Snap,
    // View, Select, Add, <menus del contexto>, ... Antes el UV ponia SelMode/Pivot DESPUES de
    // View/Select y Add segundo: los dedos iban al lugar equivocado al saltar de un viewport al
    // otro. Reordenar es GRATIS porque el dispatch y la visibilidad van por ROL, no por indice.
    Button* b;
    b = new Button(T("Edit Mode"), (int)IconType::mesh); b->role = BRUV_Mode; // selector de modo (icono = el modo, como el 3D)
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo operativo (Edit Mode)
    b = new Button("", (int)IconType::selVertex); b->role = BRUV_SelMode; // SelMode UV (SOLO icono)
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo donde hay sub-seleccion
    b = new Button("", (int)IconType::pivotMedian); b->role = BRUV_Pivot; // Pivot (SOLO icono, = el 3D)
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo donde hay transform
    // ops del CURSOR 2D. Se llama "Cursor" (no "Snap"): el "Snap" del 3D es el IMAN. Ver BRUV_Snap.
    b = new Button(T("Cursor"), (int)IconType::pivotCursor); b->role = BRUV_Snap;
        b->dropdown = true; b->visible = false; BarButtons.push_back(b);
    b = new Button("", IconType::monitor); b->role = BRUV_View;        // checkboxes (Sync/Repeat/Chrome) + Frame
        BarButtons.push_back(b);
    // menu Select: MISMO icono y MISMO lugar (pegado a View) que el del viewport 3D, para que la
    // barra del UV no diverja de la del 3D. Solo operativo y fuera de la pintura de pesos.
    b = new Button("", IconType::selection); b->role = BRUV_Select;    // All / None / Invert / Select Linked
        b->help = "Select";
        b->dropdown = true; b->visible = false; BarButtons.push_back(b);
    b = new Button("", IconType::plus); b->role = BRUV_Add;             // menu Add: Armature 2D / Bone
        b->help = "Add";
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo operativo (Edit Mode)
    b = new Button(T("Armature")); b->role = BRUV_Armature;            // menu Armature: Extrude/Duplicate/Delete/Parent
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo en UVModeBones
    b = new Button("UVMap"); b->role = BRUV_UVMap;                     // dropdown: que mapa UV se edita (+ uno nuevo)
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // con una malla (el texto: el mapa activo)
    b = new Button(T("Texture")); b->role = BRUV_Texture;              // dropdown: elegir que textura ver
        b->dropdown = true; BarButtons.push_back(b);
    b = new Button(T("Mipmap")); b->role = BRUV_Mipmap;                   // inspector de la piramide (Auto / nivel)
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // visible con textura mostrada (SyncBar)
    b = new Button(T("Edit")); b->role = BRUV_Edit; b->visible = false; BarButtons.push_back(b);   // pintura de textura
    b = new Button("", IconType::save); b->role = BRUV_Save;    // Save Texture / Make External / Make Internal
        b->help = "Save Texture";
        b->dropdown = true; b->visible = false; BarButtons.push_back(b);
    b = new Button("", IconType::keyframe); b->role = BRUV_Animation;  // menu Animation (icono rombo, = el 3D)
        b->dropdown = true; b->visible = false; BarButtons.push_back(b); // solo operativo (Edit Mode)
    // TOOLBAR inferior (mecanismo compartido de ViewportBase): DESHACER/REHACER primero (siempre,
    // pensando en pantallas tactiles; mismos roles/iconos que el 3D: TBR_Undo/TBR_Redo), despues el
    // historial MRU con Mover/Rotar/Escalar (starters de StartXform en edicion, del transform de
    // huesos en Huesos/Pose). Cursor/snap NO van aca: ya estan arriba. En modo PINTURA muestra los
    // controles del PINCEL (roles TBR_Pincel*/TBR_Group, los mismos que la toolbar del 3D en Weight
    // Paint) y en modo HUESOS las acciones de hueso (Extrude/Duplicate/Delete/Disconnect).
    b = new Button("", (int)IconType::undo); b->role = TBR_Undo; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::redo); b->role = TBR_Redo; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    for (int i = 0; i < 3; i++){
        b = new Button(""); b->role = TBR_UVHist + i; b->visible = false; ToolButtons.push_back(b);
    }
    // (radio y valor no tienen boton: son la fila de barras deslizables de arriba de la toolbar,
    //  ViewportBase::RenderBrushBar -- la misma que el viewport 3D, con el mismo pincel detras)
    b = new Button("+");     b->role = TBR_BrushMode;   b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Group")); b->role = TBR_Group;        b->dropdown = true; b->visible = false; ToolButtons.push_back(b);
    // marcas + falloff: los MISMOS del 3D (el pincel es uno solo). En el UV las marcas van por
    // CORNER, que es la unidad que pinta este editor (los uv groups son por render-vert).
    b = new Button("", (int)IconType::mesh); b->role = TBR_Marks; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Smooth")); b->role = TBR_Falloff; b->dropdown = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(""); b->role = TBR_ColorBrush; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b); // color del pincel de textura
    // "editar solo lo seleccionado" (mascara de pintura): toggle GLOBAL compartido con la
    // toolbar del 3D (WeightPaintOnlySel); icono de seleccion, tinte accent cuando esta ON
    b = new Button("", (int)IconType::selection); b->role = TBR_OnlySel; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Extrude"));    b->role = TBR_BoneExtrude; b->centered = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Duplicate"));  b->role = TBR_BoneDup;     b->centered = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button("", (int)IconType::trash); b->role = TBR_BoneDel; b->centered = true; b->square = true; b->visible = false; ToolButtons.push_back(b);
    b = new Button(T("Disconnect")); b->role = TBR_BoneDesc;    b->centered = true; b->visible = false; ToolButtons.push_back(b);
    // botones de TRANSFORM compartidos (tilde/cruz/X/Y, roles TBR_* del 3D): solo se ven
    // durante un G/R/S propio (ToolbarSyncTransform2D los maneja, como en el 3D)
    ToolbarCreateTransform2D(ToolButtons);
    gUVEditors.push_back(this); // registro de editores vivos (ver UVEditorInBoneMode)
}

// modo de seleccion EFECTIVO del editor UV: en sync sigue al 3D; si no, el propio.
int UVEditor::UVMode() const { return syncSelection ? EditSelectMode : uvSelMode; }

// el UV esta OPERATIVO: el activo es una malla y esta en edicion (mismo criterio que el wireframe).
bool UVEditor::InEditUV() const {
    return ActiveObj && ActiveObj->getType() == ObjectType::mesh && (Object*)ActiveObj == g_editMesh;
}

static void UVForgetEditor(UVEditor* uv); // el estado de trazo/transform que apunta a este editor (mas abajo)
UVEditor::~UVEditor() {
    for (size_t i = 0; i < gUVEditors.size(); i++)
        if (gUVEditors[i] == this) { gUVEditors.erase(gUVEditors.begin() + i); break; }
    UVForgetEditor(this);
}

// FILA DE BARRAS del pincel (radio | valor): el UV la muestra en modo PESOS, con el mismo
// BrushState que el viewport 3D (mover una barra aca mueve el pincel alla: es UN pincel).
bool UVEditor::BrushBarVisible() const { return (uvMode == UVModeWeights && InEditUV()) || uvMode == UVModeTexture; }

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
// sigue siendo contextual: sin malla en edicion solo quedan Undo/Redo (ToolbarSync).
// En Symbian se deja como estaba (solo operativo): ahi el ancho de pantalla es el recurso escaso.
bool UVEditor::ToolbarVisible() const {
#ifdef W3D_SYMBIAN
    return InEditUV();
#else
    return true;
#endif
}

void RebindMaterialMeshPart(); // (def en Properties.cpp) refresca el panel de material tras undo/redo

// eje bloqueado del transform de HUESOS 2D de ESTE editor, para los botones X/Y de la toolbar:
// 0 libre / 1 X / 2 Y, o -1 = "sin ejes" (rotar, o no hay transform de huesos aca).
// Se define abajo, junto al estado gB2D (aca todavia no existe).
static int B2DAxisUI(const UVEditor* uv);

// visibilidad CONTEXTUAL de la toolbar: DESHACER/REHACER siempre (menos con un transform en curso,
// como el 3D); modo EDICION/HUESOS/POSE = historial Move/Rotate/Scale (+ las acciones de HUESO en
// Huesos); modo PINTURA = controles del PINCEL (tam/fuerza/modo/grupo, roles compartidos con el 3D).
void UVEditor::ToolbarSync(){
    const bool operational = InEditUV();
    const bool edit  = (uvMode == UVModeEdit) && operational;
    const bool paint = (uvMode == UVModeWeights) && operational;
    const bool texpaint = (uvMode == UVModeTexture);   // pintura de textura: color + curva del pincel
    const bool bones  = (uvMode == UVModeBones) && operational;
    const bool pose   = (uvMode == UVModePose) && operational;
    const bool transforming = XformInProgress(); // G/R/S de UVs o transform de huesos 2D de ESTE editor
    const float* white  = ColorList[static_cast<int>(ColorID::white)];
    const float* grayUI = ColorList[static_cast<int>(ColorID::grayUI)];
    std::vector<int>& h = UVToolHist();
    std::string lSize, lForce, lMode, lGroup;
    if (paint){
        Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        WeightPaintLabelsUV(m, lSize, lForce, lMode, lGroup); // el boton de grupo = UV group activo
    }
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        int role = btn->role;
        int hi = role - TBR_UVHist;
        if (role == TBR_Undo || role == TBR_Redo){
            // siempre visibles (tactil), MENOS durante una edicion en curso; atenuados si no hay
            // nada. NO dependen de 'operativo': Deshacer/Rehacer tienen que estar tambien con el UV
            // fuera de Edit Mode (mismo criterio que la toolbar del 3D).
            btn->visible = !transforming;
            bool has = (role == TBR_Undo) ? UndoHasSomething() : UndoHasRedo();
            btn->tint = NULL; btn->textColor = has ? white : grayUI;
        } else if (hi >= 0 && hi < 3){
            btn->visible = (edit || bones || pose) && !transforming && hi < (int)h.size();
            if (btn->visible) btn->text = ToolbarActionLabel(h[hi]);
        } else if (role >= TBR_BrushSize && role <= TBR_Group){
            btn->visible = paint;
            btn->tint = NULL; btn->textColor = NULL;
            if (role == TBR_BrushMode){
                btn->text = lMode;                          // "+" / "-" / "=" (mismo pincel que el 3D)
                int md = BrushGet().mode;
                btn->tint      = (md == WPSubtract) ? TbRedBg() : (md == WPEqualize) ? TbGreenBg() : NULL;
                btn->textColor = (md == WPSubtract) ? TbRed()
                                : (md == WPEqualize) ? ColorList[static_cast<int>(ColorID::accent)] : NULL;
            }
            else                              btn->text = lGroup;
        } else if (role == TBR_Marks){
            btn->visible = paint;
            const bool onM = (BrushGet().marks != MarksOff);
            btn->tint = onM ? TbGreenBg() : NULL;
            btn->textColor = onM ? ColorList[static_cast<int>(ColorID::accent)] : white;
        } else if (role == TBR_ColorBrush){
            btn->visible = texpaint;
            btn->tint = BrushGet().color;   // el boton ES la muestra del color
            btn->textColor = NULL;
        } else if (role == TBR_Falloff){
            btn->visible = paint || texpaint;
            const int typeF = BrushGet().falloff.type;
            btn->icon = IconIndex(W3dFalloffIcon(typeF));
            btn->text = T(W3dFalloffName(typeF));
            btn->tint = NULL; btn->textColor = NULL;
        } else if (role == TBR_OnlySel){
            // "editar solo lo seleccionado": toggle global compartido con el 3D; accent = ON
            btn->visible = paint;
            bool on = WeightPaintOnlySel();
            btn->tint = on ? TbGreenBg() : NULL;
            btn->textColor = on ? ColorList[static_cast<int>(ColorID::accent)] : white;
        } else if (role >= TBR_BoneExtrude && role <= TBR_BoneDesc){
            // acciones de HUESO 2D: solo editando huesos (en pose se posa, no se re-estructura)
            btn->visible = bones && !transforming;
        }
    }
    // TRANSFORM en curso: tilde/cruz/X/Y compartidos (mismas reglas y colores que el 3D).
    // REGLA DE EJES (la misma para el G/R/S de UVs y para el de HUESOS 2D, documentada):
    //   G (mover)   -> botones X / Y visibles (bloqueo por eje en espacio de la vista)
    //   S (escalar) -> X / Y visibles (escala solo ese eje; el otro queda en 1.0)
    //   R (rotar)   -> OCULTOS: una rotacion 2D no tiene eje (el unico eje seria el Z de la
    //                 pantalla, que es justamente lo que ya se rota) -> -1
    const int axisUI = uvXform ? ((uvXform == 2) ? -1 : uvXAxis) : B2DAxisUI(this);
    ToolbarSyncTransform2D(ToolButtons, transforming && operational, axisUI);
}

void UVEditor::ToolbarActionRole(int role){
    if (uvMode == UVModeTexture){   // PINTURA DE TEXTURA: la curva y el color del pincel (el resto no aplica)
        if (role == TBR_Falloff){
            Button* bf = BarRoleBtn(ToolButtons, TBR_Falloff);
            FalloffEditorOpen(&BrushGet().falloff, bf ? bf->sx : x, y + height - ToolbarHeight());
            return;
        }
        if (role == TBR_ColorBrush){
            Button* bc = BarRoleBtn(ToolButtons, TBR_ColorBrush);
            if (!colorPicker) colorPicker = new ColorPicker();
            colorPicker->Open(BrushGet().color, bc ? bc->sx : x, y + height - ToolbarHeight());
            return;
        }
    }
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (role == TBR_Undo){ UndoUndo(); RebindMaterialMeshPart(); g_redraw = true; return; }
    if (role == TBR_Redo){ UndoRedo(); RebindMaterialMeshPart(); g_redraw = true; return; }
    // botones de TRANSFORM compartidos (tilde/cruz/X/Y): mismo camino que las teclas
    if (XformInProgress() && ToolbarActionTransform2D(this, role)) return;
    if (uvMode == UVModeWeights){
        // "editar solo lo seleccionado": toggle global (compartido con la toolbar del 3D)
        if (role == TBR_OnlySel){ WeightPaintOnlySel() = !WeightPaintOnlySel(); g_redraw = true; return; }
        if (role == TBR_Marks){ // el UV pinta POR CORNER (uv groups) -> sus marcas son las de corner
            BrushGet().marks = (BrushGet().marks != MarksOff) ? MarksOff : MarksCorner;
            g_redraw = true; return;
        }
        if (role == TBR_Falloff){ // el MISMO popup reutilizable que abre el 3D, sobre el MISMO falloff
            Button* bf = BarRoleBtn(ToolButtons, TBR_Falloff);
            FalloffEditorOpen(&BrushGet().falloff, bf ? bf->sx : x, y + height - ToolbarHeight());
            return;
        }
        // PINTURA: controles del pincel (mismo dispatch que la toolbar del 3D en Weight Paint)
        if (role < TBR_BrushSize || role > TBR_Group) return;
        Button* b = BarRoleBtn(ToolButtons, role);
        int bx = b ? b->sx : x, byTop = y + height - ToolbarHeight();
        if (role == TBR_BrushMode)   { BrushGet().mode = (BrushGet().mode + 1) % 3; g_redraw = true; } // + -> - -> =
        else if (role == TBR_Group && m) WeightPaintMenuUVGroup(m, bx, byTop); // UV groups
        return;
    }
    if (!m || (Object*)m != g_editMesh) return;
    // ===== modos HUESOS/POSE (armature 2D del mesh) =====
    if (uvMode == UVModeBones || uvMode == UVModePose){
        if (Bone2DXformActive()) { XformConfirm(); return; } // un transform en curso se CONFIRMA (con gate numerico)
        if (role == TBR_BoneExtrude && uvMode == UVModeBones){
            int nb = Bone2DExtrude(m);
            if (nb >= 0) Bone2DDragEnd(m, nb, 2, false); // el tail nuevo queda agarrado al mouse
            return;
        }
        if (role == TBR_BoneDup  && uvMode == UVModeBones){
            if (Bone2DDuplicate(m) >= 0) Bone2DXformStart(m, 1); // lo duplicado queda agarrado (mover)
            return;
        }
        if (role == TBR_BoneDel  && uvMode == UVModeBones){ Bone2DDelete(m); return; }
        if (role == TBR_BoneDesc && uvMode == UVModeBones){
            // el MISMO popup de Alt+P (Disconnect Bone / Clear Parent), bajo el boton
            Button* b = BarRoleBtn(ToolButtons, TBR_BoneDesc);
            LayoutMenuBoneAltP2D(m, b ? b->sx : x, y + height - ToolbarHeight());
            return;
        }
        int hi = role - TBR_UVHist;
        std::vector<int>& h = UVToolHist();
        if (hi >= 0 && hi < (int)h.size()){
            int id = h[hi];
            Bone2DXformStart(m, (id == TBMove) ? 1 : (id == TBRotate) ? 2 : 3);
        }
        return;
    }
    if (uvXform) { XformConfirm(); return; } // como cualquier click: un transform en curso se CONFIRMA (con gate)
    int hi = role - TBR_UVHist;
    std::vector<int>& h = UVToolHist();
    if (hi >= 0 && hi < (int)h.size()){
        int id = h[hi];
        StartXform(m, (id == TBMove) ? 1 : (id == TBRotate) ? 2 : 3); // (registra el MRU adentro)
    }
}

void UVEditor::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH); // el borde sigue el tamano (como Outliner/Properties)
}

// la mesh part SELECCIONADA en el panel de propiedades activo; si no hay, la 0.
static int UVActivePart(Mesh* m) {
    if (PropsActive && PropsActive->propMeshParts &&
        !PropsActive->propMeshParts->properties.empty()) {
        PropListMeshParts* lst =
            static_cast<PropListMeshParts*>(PropsActive->propMeshParts->properties[0]);
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
static int   g_uvAutoAtOvr   = -1;   // UVActivePart al fijar el override (detecta cambio de seleccion)
// ...y la IDENTIDAD de la parte elegida (nombre + material). El override es un INDICE a
// Mesh::materialsGroup, o sea un miembro (c) de los de Undo.h: reordenar los mesh parts (los botones
// subir/bajar de la tarjeta) corre los indices y el override pasaba a mostrar la textura de OTRA parte.
// No corrompe nada -es solo que texture se ve en el editor UV- pero es la misma clase de bug, y como
// el estado es de este archivo se cierra aca en vez de pedirle un aviso al que reordena: si el
// elemento que hay en esa posicion ya no es el que se eligio, el override CAE al auto.
static std::string g_uvTexOvrName;
static Material*   g_uvTexOvrMat = NULL;
static bool UVOverrideStillSame(Mesh* m){
    if (g_uvTexOverride < 0 || !m || g_uvTexOverride >= (int)m->materialsGroup.size()) return false;
    const MaterialGroup& g = m->materialsGroup[g_uvTexOverride];
    return g.name == g_uvTexOvrName && g.material == g_uvTexOvrMat;
}
static int UVEffectivePart(Mesh* m){
    if (!m) return -1;
    int autoPart = UVActivePart(m);
    if (g_uvTexOverride >= 0 && (g_uvTexOvrSerial != m->serial || autoPart != g_uvAutoAtOvr ||
                                 g_uvTexOverride >= (int)m->materialsGroup.size() ||
                                 !UVOverrideStillSame(m))){
        g_uvTexOverride = -1; g_uvTexOvrSerial = 0; // cambio la seleccion/malla/orden -> vuelve al auto
        g_uvTexOvrName.clear(); g_uvTexOvrMat = NULL;
    }
    return (g_uvTexOverride >= 0) ? g_uvTexOverride : autoPart;
}
// lo llama el dropdown de la barra (LayoutInput.cpp): fija a mano la parte/textura a ver.
void UVSetTexOverride(Mesh* m, int part){
    g_uvTexOverride = part; g_uvTexOvrSerial = m ? m->serial : 0; g_uvAutoAtOvr = UVActivePart(m);
    g_uvTexOvrName.clear(); g_uvTexOvrMat = NULL;
    if (m && part >= 0 && part < (int)m->materialsGroup.size()){
        g_uvTexOvrName = m->materialsGroup[part].name;
        g_uvTexOvrMat  = m->materialsGroup[part].material;
    }
}
int  UVShownPart(Mesh* m){ return UVEffectivePart(m); } // para la barra (nombre del boton)
int  UVTexOverridePart(){ return g_uvTexOverride; }     // -1 = auto (para el tilde del menu)

// --- override de PROYECTO: ver CUALQUIER textura cargada en la escena (auditar cuantas
// hay y como calzan estas UV sobre otra textura/aspecto). A diferencia del override de
// parte NO cae al cambiar de seleccion: comparar contra otra textura es justamente
// pasearse por mallas. Va por RUTA (el puntero muere al cerrar/recargar el proyecto);
// si la textura ya no esta cargada, cae solo al auto.
static std::string g_uvTexProject;
Texture* UVTexActiveProject(){
    if (g_uvTexProject.empty()) return NULL;
    for (size_t i = 0; i < Textures.size(); i++)
        if (Textures[i] && Textures[i]->iID && Textures[i]->path == g_uvTexProject)
            return Textures[i];
    return NULL;
}
// el ID GL de la textura de proyecto elegida: la del registro 3D (Textures) o, si
// no esta ahi, la del cache 2D del juego (HUD/Image2D via Texture2DGet, que
// la carga si hace falta). 0 = sin override.
unsigned int UVTexProjectId(){
    if (g_uvTexProject.empty()) return 0;
    Texture* t = UVTexActiveProject();
    if (t) return t->iID;
    return Texture2DGet(g_uvTexProject);
}
void UVSetTexProject(const std::string& path){ g_uvTexProject = path; }
const std::string& UVTexProjectPath(){ return g_uvTexProject; }

// --- inspector de MIPMAPS (menu "Mipmap" de la barra): ver UN nivel puntual de la
// piramide de la textura mostrada. -1 = Auto (el filtro elige por zoom, el
// comportamiento normal del render). Solo visualizacion, no toca la textura.
static int g_uvMipLevel = -1;
int  UVMipLevel(){ return g_uvMipLevel; }
void UVSetMipLevel(int level){ g_uvMipLevel = level; }
unsigned int UVShownTextureId(Mesh* m){
    unsigned int idp = UVTexProjectId();
    if (idp) return idp;
    if (!m) return 0;
    int p = UVEffectivePart(m);
    Material* mm = (p >= 0 && p < (int)m->materialsGroup.size()) ? m->materialsGroup[p].material : NULL;
    return (mm && mm->texture) ? mm->texture->iID : 0;
}

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
// calcula el aspecto (g_uvAspU/V) de la textura MOSTRADA: la del override de proyecto si
// hay, o la de la mesh part activa de m. mayor lado = 1.0.
static void CalcAspectUV(class Mesh* m, int part) {
    g_uvAspU = g_uvAspV = 1.0f;
    int tw = 0, th = 0;
    unsigned int idp = UVTexProjectId();
    if (idp && w3dEngine::TextureSize(idp, tw, th) && tw > 0 && th > 0) {
        if (tw >= th) g_uvAspV = (float)th / (float)tw;
        else          g_uvAspU = (float)tw / (float)th;
        return;
    }
    if (!m) return;
    Material* mat = (part >= 0 && part < (int)m->materialsGroup.size()) ? m->materialsGroup[part].material : NULL;
    if (mat && mat->texture && mat->texture->iID && w3dEngine::TextureSize(mat->texture->iID, tw, th) && tw > 0 && th > 0) {
        if (tw >= th) g_uvAspV = (float)th / (float)tw; // ancha -> achica el alto
        else          g_uvAspU = (float)tw / (float)th; // alta  -> achica el ancho
    }
}

// ===================================================================================================
//  PINTURA DE PESOS en el editor UV (UVModeWeights): el MISMO pincel que el 3D (edit/WeightPaint.cpp)
//  con UVtoScreen como proyector, pero sobre la OTRA entidad: los UV GROUPS (pesos por CORNER /
//  render-vert, Mesh::uvGroups). Los vertex groups del 3D no se tocan.
// ===================================================================================================
static UVEditor* gUVPainting = NULL; // editor UV con un trazo del pincel en curso (NULL = ninguno)

struct WPUVCtx { Mesh* m; float cx, cy, s; };
static bool WPUVProject(void* ctx, int i, float& sx, float& sy) {
    WPUVCtx* c = (WPUVCtx*)ctx;
    if (i < 0 || i >= c->m->vertexSize || !c->m->uv) return false;
    UVtoScreen(c->m->uv[i*2], c->m->uv[i*2+1], c->cx, c->cy, c->s, sx, sy);
    return true;
}

// una pasada del pincel en la posicion actual del mouse (lastMx/lastMy, coords locales).
// NO es static a proposito: la declara UVEditor.h para que los tests puedan dar una pasada de
// pincel sin simular SDL (el click y el drag reales siguen siendo los unicos que la llaman).
// la textura que el editor esta MOSTRANDO: el override de proyecto, o la del material de la parte mostrada
Texture* UVShownTexture(Mesh* m) {
    Texture* t = UVTexActiveProject();
    if (t) return t;
    if (!m) return NULL;
    const int p = UVEffectivePart(m);
    if (p < 0 || p >= (int)m->materialsGroup.size()) return NULL;
    Material* mm = m->materialsGroup[(size_t)p].material;
    return (mm && mm->texture && mm->texture->iID) ? mm->texture : NULL;
}
void UVToggleEditTexture(UVEditor* uv) {
    if (!uv) return;
    if (uv->uvMode == UVModeTexture) uv->uvMode = uv->uvPrevModeTex;
    else { uv->uvPrevModeTex = uv->uvMode; uv->uvMode = UVModeTexture; }
    g_redraw = true;
}
// ---------------------------------------------------------------------------
//  PINTURA DE TEXTURA en el editor UV: el toque va a los TEXELS bajo el cursor (el mismo mapeo UV->screen
//  del Render, invertido). Los toques se interpolan desde el anterior para que un trazo rapido no deje huecos.
// ---------------------------------------------------------------------------
static UVEditor* gUVPaintingTex = NULL;
static TextureEditable* gUVTexEd = NULL;
static bool  gUVTexChange = false;
static float gUVTexPrevX = -1e9f, gUVTexPrevY = 0.0f;
static bool UVTexStrokeStart(UVEditor* uv) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    Texture* t = UVShownTexture(m);
    if (!t) return false;
    gUVTexEd = TexEditGet(t);
    if (!gUVTexEd) return false;
    UndoTextureStart(gUVTexEd);
    gUVTexChange = false; gUVTexPrevX = -1e9f;
    (void)uv;
    return true;
}
static void UVPaintTexture(UVEditor* uv) {
    if (!uv || !gUVTexEd) return;
    float cx, cy, s; uv->ParamsUV(cx, cy, s);
    const float lx = (float)(uv->lastMx - uv->x), ly = (float)(uv->lastMy - uv->y);
    const float u = 0.5f + (lx - cx) / (s * g_uvAspU), v = 0.5f + (ly - cy) / (s * g_uvAspV);
    const float tx = u * gUVTexEd->w, ty = v * gUVTexEd->h;
    BrushState& br = BrushGet();
    const float rTex = br.radiusPx / (s * g_uvAspU) * gUVTexEd->w;   // radio del pincel, en texels
    unsigned char rgba[4];
    for (int q = 0; q < 4; q++) { float c = br.color[q]; if (c < 0) c = 0; if (c > 1) c = 1; rgba[q] = (unsigned char)(c * 255.0f + 0.5f); }
    const W3dFalloff& fo = EffectiveBrushFalloff();
    int rect[4] = { -1, -1, -1, -1 };
    if (gUVTexPrevX > -1e8f) {
        const float dx = tx - gUVTexPrevX, dy = ty - gUVTexPrevY, dist = sqrtf(dx*dx + dy*dy);
        float step = rTex / 3.0f; if (step < 1.0f) step = 1.0f;
        int n = (int)ceilf(dist / step); if (n < 1) n = 1;
        for (int i = 1; i <= n; i++) { const float f = (float)i / (float)n; TexEditDab(gUVTexEd, gUVTexPrevX + dx * f, gUVTexPrevY + dy * f, rTex, rgba, br.force, fo, rect); }
    } else TexEditDab(gUVTexEd, tx, ty, rTex, rgba, br.force, fo, rect);
    gUVTexPrevX = tx; gUVTexPrevY = ty;
    if (rect[0] >= 0) { TexEditUpload(gUVTexEd, rect[0], rect[1], rect[2], rect[3], true); gUVTexChange = true; }
    g_redraw = true;
}
static void UVTexStrokeEnd() { TexEditEndStroke(gUVTexEd); UndoTextureConfirm(gUVTexChange); gUVTexEd = NULL; gUVTexChange = false; }

void UVPaintWeights(UVEditor* uv, Mesh* m) {
    if (!uv || !m || !m->uv) return;
    WPUVCtx c; c.m = m;
    uv->ParamsUV(c.cx, c.cy, c.s); // mismo mapeo UV->screen que el Render (el pick coincide)
    BrushState& br = BrushGet();
    // mascara "editar solo lo seleccionado" (TBR_OnlySel): SIEMPRE las caras seleccionadas en
    // EDIT MODE (el 3D), en los dos modos de sync -> pintar aca o en el viewport 3D da lo mismo
    // (el toggle es global, WeightPaintOnlySel). Se pasa NULL y el pincel la deriva de la
    // edit mesh. OJO: NO se usa la seleccion PROPIA del UV: es de sub-elementos (podes tener UN
    // vertice UV clickeado) y enmascarar el pincel con eso dejaria de pintar casi todo.
    // POR CORNER (render-vert) SIEMPRE: el pincel del UV escribe el UV GROUP activo -> pesar los
    // 4 corners de UNA cara del cubo no toca las otras caras que comparten esos vertices 3D (que
    // es lo que se ve como islas UV separadas) ni los vertex groups del 3D.
    if (BrushApplyUV(m, m->uvActiveGroup, (float)(uv->lastMx - uv->x), (float)(uv->lastMy - uv->y),
                        br.radiusPx, br.force, (WPMode)br.mode, WPUVProject, &c, NULL,
                        &EffectiveBrushFalloff())) {
        // PINTAR NO MUEVE UVs. Solo se re-evalua el skinning 2D si hay una POSE REAL que dependa
        // de los pesos que se acaban de cambiar; con la pose en identidad (o sin armature 2D) no
        // hay nada que re-deformar y llamar al skinning seria pisar uv[] al pedo.
        // El rest esta en sync (invariante uv = f(uv2dRest, pose), ver Mesh.h) -> re-aplicar NO
        // pierde las ediciones de UV hechas antes de pintar.
        // El gate mira TODOS los armatures (Arm2DAnyBone), no los del ACTIVO: el skinning aplica
        // todos (Armature2DApply) y el chequeo de pose tambien, asi que preguntar por el activo
        // dejaba de re-deformar al pintar un UV group de un SEGUNDO rig posado (o con el rig activo
        // vacio) -- justo el caso multi-armature.
        if (m->Arm2DAnyBone() && !m->Armature2DPoseIdentity()) {
            m->Armature2DApply();
            m->skinGeomVersion++; // los UV cambiaron -> re-subir el VBO de uv (sino el viewport
                                  // 3D seguia con el uv viejo). Igual que los otros caminos que
                                  // llaman a Armature2DApply (ver mas abajo y LayoutInput).
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
//  Mesh::Armature2DApply (skinning 2D, corre en editor Y runtime); la animacion en el tiempo va
//  por los CLIPS PROPIOS de CADA armature (Armature2D::anims, curva por hueso y canal) que llenan el
//  Auto Key / Insert Keyframe. RETROCOMPAT: un rig sin clips propios sigue horneando en la VERTEX
//  ANIMATION UV existente (VertexFrame::uvs), que es como andaban los proyectos viejos.
//  Los modos del UV editor UVModeBones (editar rest) y UVModePose (posar) manejan el input.
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
static void UVXformAdvanceCursor(UVEditor* uv, int mx, int my, float& curU, float& curV) {
    int jx = mx - uv->lastMx, jy = my - uv->lastMy;
    bool jump = (jx > uv->width / 2 || jx < -uv->width / 2 ||
                  jy > uv->height / 2 || jy < -uv->height / 2);
    if (jump) return;
    float cx, cy, s; uv->ParamsUV(cx, cy, s);
    float au = (g_uvAspU > 1e-6f) ? g_uvAspU : 1.0f, av = (g_uvAspV > 1e-6f) ? g_uvAspV : 1.0f;
    curU += (float)jx / (s * au);
    curV += (float)jy / (s * av);
}

// ---- estado del transform MODAL de huesos 2D (uno solo, con el editor dueño) ----
struct B2DXformState {
    UVEditor* uv;            // editor dueño (NULL = nada en curso)
    int   mode;              // 1=mover 2=rotar 3=escalar
    int   axis;              // BLOQUEO DE EJE (teclas X/Y y botones de la toolbar): 0=libre 1=X 2=Y.
                             // Espacio de la VISTA, igual que el uvXAxis del transform de UVs (aca
                             // no hay local/global). En ROTAR no aplica (una rotacion 2D no tiene eje).
    bool  byClick;          // arranco por click (suelta = confirma); false = modal (G/E)
    bool  inPose;            // true = edita la POSE (UVModePose); false = edita el REST (UVModeBones)
    float startU, startV;    // mouse (UV) al arrancar
    float curU, curV;        // cursor VIRTUAL (acumula deltas filtrados; wrap-safe, como uvXCur*)
    float pivU, pivV;        // pivote (mediana de las PUNTAS en movimiento / heads posados en pose)
    float valU, valV;        // delta aplicado (para la barra de INFO)
    float valAng, valFac;    // angulo (grados) / factor aplicados (barra de INFO)
    std::vector<int>   bones;// huesos afectados
    std::vector<int>   masks;// paralelo a bones: que PUNTA se mueve de cada uno (1=head 2=tail 3=ambas).
                             // En pose no aplica (siempre el hueso entero). Viene de la SELECCION por
                             // puntas + la soldadura (Bone2DSelectionMasks), como el 3D.
    std::vector<float> snap; // snapshot: rest -> 4 por hueso (headU,headV,tailU,tailV);
                             // pose -> 5 por hueso (poseTU,poseTV,poseRot,poseSX,poseSY)
    B2DXformState() : uv(NULL), mode(0), axis(0), byClick(false),
                       inPose(false), startU(0), startV(0), curU(0), curV(0),
                       pivU(0), pivV(0), valU(0), valV(0), valAng(0), valFac(1) {}
};
static B2DXformState gB2D;
// el editor MUERE (abrir un proyecto libera el layout anterior; cambiar el tipo del viewport lo
// borra) con un trazo o un transform de huesos en curso: soltar los punteros. El trazo no se
// cierra (su malla puede haber muerto antes, con la escena): solo no queda colgando del editor.
static void UVForgetEditor(UVEditor* uv) {
    if (gUVPainting == uv)    gUVPainting = NULL;
    if (gUVPaintingTex == uv) gUVPaintingTex = NULL;
    if (gB2D.uv == uv)        gB2D.uv = NULL;
}
// nucleo del apply de huesos (definido mas abajo): tambien lo usa la entrada numerica (XformNumValue)
static void B2DApplyValues(Mesh* m, float du, float dv, float ang, float f);
// eje para los botones X/Y de la toolbar durante el transform de huesos de ESTE editor (ver la
// declaracion arriba de ToolbarSync): rotar 2D no tiene eje -> -1 (los oculta).
static int B2DAxisUI(const UVEditor* uv) {
    if (gB2D.mode == 0 || gB2D.uv != uv) return -1;
    return (gB2D.mode == 2) ? -1 : gB2D.axis;
}

// nombre unico para un hueso 2D ("Bone", "Bone.001", ...) dentro del MESH (todos sus
// armatures 2D a la vez: el binding es POR NOMBRE contra el UV group, que es del mesh,
// asi que dos huesos homonimos -aunque sean de rigs distintos- se pelearian los pesos).
// Delega en Mesh::FreeNameBone2D -> LA regla comun (base/W3dNames.h). El bucle
// viejo tenia tope 999 y al llegar devolvia la BASE OCUPADA (duplicado en silencio).
static std::string Bone2DUniqueName(Mesh* m, const std::string& base) {
    return m ? m->FreeNameBone2D(base, -1, -1) : base;
}
// garantiza el UV GROUP del hueso 2D (binding por nombre: 2D = hueso <-> UV group, ver Mesh.h) y
// lo deja activo (para pintar sus pesos por corner ya). NO toca los vertex groups: esos son del
// armature 3D.
static void Bone2DEnsureGroup(Mesh* m, const std::string& name) {
    for (size_t g = 0; g < m->uvGroups.size(); g++)
        if (m->uvGroups[g] && m->uvGroups[g]->name == name) { m->uvActiveGroup = (int)g; return; }
    m->uvGroups.push_back(new UVGroup(name));
    m->uvActiveGroup = (int)m->uvGroups.size() - 1;
}
// ===== SELECCION por puntas/huesos del armature 2D (calca BoneSel* del 3D, BoneEdit.cpp) =====
// conectado = flag 'conectado' prendido Y head del hijo pegado al tail del padre (el epsilon
// tolera redondeos de guardado/carga; en UV las distancias son ~1 -> mismo 1e-8 que el 3D).
// SOLO la geometria: el head del hueso cae en el tail de su padre? (sin mirar el flag). Aca vive LA
// tolerancia: el flag y el saneador la comparten para no poder discrepar entre si.
static bool Bone2DTipsWelded(Mesh* m, int b) {
    if (!m || b < 0 || b >= (int)m->Arm2DBones().size()) return false;
    const W3dBone2D& hb = m->Arm2DBones()[b];
    if (hb.parent < 0 || hb.parent >= (int)m->Arm2DBones().size()) return false;
    const W3dBone2D& p = m->Arm2DBones()[hb.parent];
    const float du = hb.headU - p.tailU, dv = hb.headV - p.tailV;
    return du * du + dv * dv < 1e-8f;
}

bool Bone2DIsConnected(Mesh* m, int b) {
    if (!m || b < 0 || b >= (int)m->Arm2DBones().size()) return false;
    if (!m->Arm2DBones()[b].connected) return false;
    return Bone2DTipsWelded(m, b);
}

// INVARIANTE del flag: 'conectado' puede estar en true SOLO si el hueso tiene padre y su head cae en el
// tail de ese padre. Toda op que RE-PARENTE sin mover el hueso (borrar el hueso del medio -> los hijos
// pasan al abuelo, Clear Parent, duplicar, el desplegable Parent, Ctrl+P) puede dejar el flag prendido con
// las puntas separadas: el hueso "dice" que esta soldado y no lo esta. Esto lo restaura de una pasada.
// NO MUEVE NADA: solo apaga el flag donde miente (el hueso que si coincide lo conserva).
// Se llama al final de esas ops para que el invariante no dependa de acordarse en cada una.
bool Bone2DSanitizeConnected(Mesh* m) {
    if (!m) return false;
    bool change = false;
    for (size_t i = 0; i < m->Arm2DBones().size(); i++) {
        if (!m->Arm2DBones()[i].connected) continue;
        if (Bone2DTipsWelded(m, (int)i)) continue;   // sigue soldado de verdad -> queda
        m->Arm2DBones()[i].connected = false;
        change = true;
    }
    return change;
}
void Bone2DSelClear(Mesh* m) {
    if (!m) return;
    for (size_t i = 0; i < m->Arm2DBones().size(); i++) {
        m->Arm2DBones()[i].select = false;
        m->Arm2DBones()[i].selHead = false;
        m->Arm2DBones()[i].selTail = false;
    }
}
void Bone2DSelBone(Mesh* m, int b, bool sel) {
    if (!m || b < 0 || b >= (int)m->Arm2DBones().size()) return;
    m->Arm2DBones()[b].select  = sel;
    m->Arm2DBones()[b].selHead = sel;
    m->Arm2DBones()[b].selTail = sel;
}
// prende/apaga el flag crudo de la punta (sin sincronizar la soldada)
static void B2DSelTipRaw(Mesh* m, int b, int tip, bool sel) {
    if (b < 0 || b >= (int)m->Arm2DBones().size()) return;
    if (tip == 1) m->Arm2DBones()[b].selHead = sel; else m->Arm2DBones()[b].selTail = sel;
    if (!sel) m->Arm2DBones()[b].select = false; // punta apagada a mano: el hueso ya no esta "entero"
}
// selecciona la punta Y todas sus soldadas: el tail del padre + el head de CADA hijo conectado a
// ese mismo punto (misma regla que BoneSelTip del 3D).
void Bone2DSelTip(Mesh* m, int b, int tip, bool sel) {
    if (!m || b < 0 || b >= (int)m->Arm2DBones().size()) return;
    B2DSelTipRaw(m, b, tip, sel);
    int dueTail = -1; // el hueso cuyo TAIL es esta punta compartida (si existe)
    if (tip == 2) dueTail = b;
    else if (Bone2DIsConnected(m, b)) dueTail = m->Arm2DBones()[b].parent;
    if (dueTail < 0) return;              // head raiz/suelto: punta propia, nada mas
    B2DSelTipRaw(m, dueTail, 2, sel);
    for (size_t c = 0; c < m->Arm2DBones().size(); c++)
        if (m->Arm2DBones()[c].parent == dueTail && Bone2DIsConnected(m, (int)c))
            B2DSelTipRaw(m, (int)c, 1, sel);
}
// arma bones/masks del transform desde la SELECCION (hueso entero = 3; puntas = 1/2). Sin
// seleccion cae al activo entero. Luego SUELDA: todo tail que se mueve arrastra el head de sus
// hijos conectados (regla one-way del 3D: BoneSelectionMasks).
static bool Bone2DSelectionMasks(Mesh* m, std::vector<int>& bones, std::vector<int>& masks) {
    bones.clear(); masks.clear();
    if (!m) return false;
    const int n = (int)m->Arm2DBones().size();
    std::vector<int> mask(n, 0);
    for (int i = 0; i < n; i++) {
        const W3dBone2D& b = m->Arm2DBones()[i];
        if (b.select) mask[i] = 3;
        else mask[i] = (b.selHead ? 1 : 0) | (b.selTail ? 2 : 0);
    }
    bool has = false;
    for (int i = 0; i < n; i++) if (mask[i]) { has = true; break; }
    if (!has) {
        if (m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < n) mask[m->Arm2DActiveBone()] = 3;
        else return false;
    }
    bool change = true; int guard = 0;
    while (change && guard++ <= n) {
        change = false;
        for (int c = 0; c < n; c++) {
            int p = m->Arm2DBones()[c].parent;
            if (p < 0 || p >= n || !(mask[p] & 2)) continue;
            if (!Bone2DIsConnected(m, c)) continue;
            if (!(mask[c] & 1)) { mask[c] |= 1; change = true; }
        }
    }
    for (int i = 0; i < n; i++)
        if (mask[i]) { bones.push_back(i); masks.push_back(mask[i]); }
    return !bones.empty();
}

// extremos EFECTIVOS del hueso i para dibujar/pickear: rest, o POSADOS (matrices FK) en pose.
// 'huesos' es la lista de UN armature y M sus matrices (Armature2DMatricesOf).
static void Bone2DEndsOf(const std::vector<W3dBone2D>& bones, const std::vector<float>& M,
                             size_t i, bool pose, float& hu, float& hv, float& tu, float& tv) {
    const W3dBone2D& b = bones[i];
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
static void Bone2DEnds(Mesh* m, const std::vector<float>& M, size_t i, bool pose,
                           float& hu, float& hv, float& tu, float& tv) {
    Bone2DEndsOf(m->Arm2DBones(), M, i, pose, hu, hv, tu, tv);
}

// AZUL del armature 2D: el MISMO que la fila del outliner (Outliner.cpp kArm2DBlue). Que sea
// literal y no una constante compartida es a proposito: el outliner no incluye este header.
static const float kArm2DBlue[3] = { 0.28f, 0.55f, 1.00f };

// dibuja los huesos 2D DEL MESH con el lenguaje visual del 3D: sin seleccionar azul, seleccionado
// accent, activo blanco; punteada tail(padre)->head si el hueso esta emparentado con gap (MISMO
// estilo que la relationship line de objetos: textura de puntos chicos + gris); puntos
// (agarraderos) POR PUNTA en UVModeBones (cada punta resalta sola, como el Edit del 3D).
//
// PEDIDO DEL DUENO: los armatures 2D se ven SIEMPRE (no solo en Huesos/Pose) y son AZULES. Se
// dibujan TODOS los de la malla:
//   - el armature ACTIVO en azul PLENO (y mas grueso todavia si es el objeto activo del UV);
//   - los demas en azul TENUE (para no tapar el mapa, pero visibles);
//   - solo el ACTIVO y solo en Edit Bones / Pose muestra la seleccion por hueso/punta (accent /
//     blanco) y los agarraderos: en los otros modos el rig es una referencia, no algo editable.
static void UVRenderBones2D(UVEditor* uv, Mesh* m, float cx, float cy, float s) {
    if (!m->HasArm2D()) return;
    const bool pose = (uv->uvMode == UVModePose);
    const bool editing = (uv->uvMode == UVModeBones || uv->uvMode == UVModePose);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    // buf = azul del armature activo; dimBuf = los otros armatures; bufSel/bufAct = seleccion
    std::vector<float> buf, dimBuf, bufSel, bufAct, dash, pts, ptsSel;
    for (size_t a = 0; a < m->armatures2d.size(); a++) {
        const Armature2D* arm = m->armatures2d[a];
        if (!arm || arm->bones.empty()) continue;
        const bool isActive = ((int)a == m->armature2dActive);
        const bool withSel  = isActive && editing;   // seleccion por hueso: solo el que se edita
        std::vector<float> M;
        if (pose) m->Armature2DMatricesOf(arm, M); // TODOS los rigs se ven POSADOS (no solo el activo)
        for (size_t i = 0; i < arm->bones.size(); i++) {
            const W3dBone2D& b = arm->bones[i];
            float hu, hv, tu, tv;
            Bone2DEndsOf(arm->bones, M, i, pose, hu, hv, tu, tv);
            float ax, ay, bx, by;
            UVtoScreen(hu, hv, cx, cy, s, ax, ay);
            UVtoScreen(tu, tv, cx, cy, s, bx, by);
            std::vector<float>* dst = isActive ? &buf : &dimBuf;
            if (withSel) {
                if ((int)i == arm->activeBone) dst = &bufAct;      // activo = blanco
                else if (b.select)             dst = &bufSel;      // seleccionado = accent
            }
            dst->push_back(ax); dst->push_back(ay); dst->push_back(bx); dst->push_back(by);
            if (withSel && !pose) { // edicion: agarraderos POR PUNTA (la seleccionada resalta sola, como el 3D)
                std::vector<float>& PH = b.selHead ? ptsSel : pts;
                PH.push_back(ax); PH.push_back(ay);
                std::vector<float>& PT = b.selTail ? ptsSel : pts;
                PT.push_back(bx); PT.push_back(by);
            }
            // gap con el padre (emparentado pero "separado"): punteada tail(padre)->head. Guardamos el
            // segmento EN PANTALLA y se dibuja despues con la textura de la relationship line.
            if (b.parent >= 0 && b.parent < (int)arm->bones.size()) {
                float phu, phv, pu, pv;
                Bone2DEndsOf(arm->bones, M, (size_t)b.parent, pose, phu, phv, pu, pv); // pu,pv = tail del padre
                float du = hu - pu, dv = hv - pv;
                if (du * du + dv * dv > 0.0001f * 0.0001f) { // hay gap (en UV)
                    float qx, qy;
                    UVtoScreen(pu, pv, cx, cy, s, qx, qy);
                    dash.push_back(qx); dash.push_back(qy); dash.push_back(ax); dash.push_back(ay);
                }
            }
        }
    }
    const float* ac = ColorList[static_cast<int>(ColorID::accent)];
    // el armature ACTIVO se resalta MAS todavia cuando ademas es el OBJETO ACTIVO del UV
    // (modo objeto): mismo azul, mas ancho y con brillo.
    const bool highlighted = uv->uvObjArm;
    gfx::LineWidth(2.0f);
    // 1) los OTROS armatures: azul tenue (visibles pero sin tapar el mapa)
    if (!dimBuf.empty()) {
        gfx::Enable(gfx::Blend); gfx::BlendAlpha();
        gfx::LineWidth(1.5f);
        gfx::Color4f(kArm2DBlue[0], kArm2DBlue[1], kArm2DBlue[2], 0.40f);
        gfx::VertexPointer2f(0, &dimBuf[0]); gfx::DrawLines((int)(dimBuf.size()/2));
        gfx::Disable(gfx::Blend);
    }
    // 2) el armature ACTIVO en azul pleno
    if (!buf.empty()) {
        gfx::LineWidth(highlighted ? 3.5f : 2.0f);
        if (highlighted) gfx::Color4f(0.45f, 0.72f, 1.0f, 1.0f);   // resaltado (objeto activo del UV)
        else           gfx::Color4f(kArm2DBlue[0], kArm2DBlue[1], kArm2DBlue[2], 1.0f);
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
        const float* gr = ColorList[static_cast<int>(ColorID::grayUI)];
        gfx::Color4f(gr[0], gr[1], gr[2], 1.0f);
        gfx::LineWidth(1.0f);
        const float period = 8.0f * (float)GlobalScale; // punto + hueco cada ~8px (como los objetos)
        for (size_t k = 0; k + 3 < dash.size(); k += 4) {
            float dx = dash[k+2] - dash[k], dy = dash[k+3] - dash[k+1];
            float len = sqrtf(dx * dx + dy * dy);
            float tuv[4] = { 0.0f, 0.0f, 0.0f, len / period };
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
        if (!pts.empty())    { gfx::Color4f(kArm2DBlue[0], kArm2DBlue[1], kArm2DBlue[2], 1.0f); gfx::VertexPointer2f(0, &pts[0]);    gfx::DrawPoints((int)(pts.size()/2)); }
        if (!ptsSel.empty()) { gfx::PointSize((float)GlobalScale * 4.0f); gfx::Color4f(1, 1, 1, 1); gfx::VertexPointer2f(0, &ptsSel[0]); gfx::DrawPoints((int)(ptsSel.size()/2)); }
        gfx::PointSize(1.0f);
    }
    gfx::LineWidth(1.0f);
}

// estado de la BARRA superior por ROL (visibilidad/iconos/textos). SIN GL: separado del Render
// para que el comando de test 'uvbar' pueda verificarlo headless. Render la llama cada frame.
void UVEditor::SyncBar() {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    const bool inEditUV = (m && (Object*)m == g_editMesh);
    Button* bMode = BarRoleBtn(BarButtons, BRUV_Mode);
    Button* bAdd  = BarRoleBtn(BarButtons, BRUV_Add);
    Button* bSel  = BarRoleBtn(BarButtons, BRUV_SelMode);
    Button* bPiv  = BarRoleBtn(BarButtons, BRUV_Pivot);
    Button* bSnap = BarRoleBtn(BarButtons, BRUV_Snap);
    Button* bTex  = BarRoleBtn(BarButtons, BRUV_Texture);
    Button* bAnim = BarRoleBtn(BarButtons, BRUV_Animation);
    // MODOS en los que cada boton REALMENTE opera algo. Un boton visible que no hace nada es la
    // peor clase de UI: el 3D ya oculta SelMode fuera de Edit Mode y Add fuera de Object Mode
    // (ViewPort3D.cpp) y aca se aplica el MISMO criterio (lo pedia la auditoria de consistencia).
    //   sub-seleccion (Vertex/Edge/Face) -> solo editando UVs
    //   transform (Pivot / ops del Cursor 2D) -> donde hay G/R/S: edicion de UVs, Edit Bones, Pose
    //   keyframes (Animation) -> donde hay algo que keyframear: edicion de UVs (capa uv) y Pose (pose 2D)
    const bool withSubSel = inEditUV && (uvMode == UVModeEdit);
    const bool withXform  = inEditUV && (uvMode == UVModeEdit || uvMode == UVModeBones || uvMode == UVModePose);
    const bool withKeys   = inEditUV && (uvMode == UVModeEdit || uvMode == UVModePose);
    // selector de MODO del UV: el texto Y EL ICONO dicen el modo ACTUAL, igual que el Mode del 3D
    // (que cicla object/mesh/armature). "Pose Mode" (no "Pose") = el mismo label que el 3D;
    // "Edit Bones" en cambio SI difiere a proposito: en el UV conviven la edicion de la GEOMETRIA
    // (Edit Mode = los UVs) y la de los HUESOS 2D en el mismo viewport, y hay que poder distinguirlas.
    if (bMode) {
        bMode->visible = inEditUV;
        bMode->text = (uvMode == UVModeWeights)  ? T("Weight Paint") :
                      (uvMode == UVModeTexture) ? T("Texture Paint") :
                      (uvMode == UVModeBones) ? T("Edit Bones") :
                      (uvMode == UVModePose)   ? T("Pose Mode") :
                      (uvMode == UVModeObject) ? T("Object Mode") : T("Edit Mode");
        bMode->icon = (uvMode == UVModeBones || uvMode == UVModePose) ? (int)IconType::armature :
                      (uvMode == UVModeObject)                        ? (int)IconType::object :
                                                                        (int)IconType::mesh;
    }
    // Add (Armature 2D / Bone): solo operativo, como el resto de los botones propios
    if (bAdd) bAdd->visible = inEditUV;
    // menu Armature (Extrude/Duplicate/Delete/Set Parent/Clear Parent): SOLO editando huesos
    { Button* bArm = BarRoleBtn(BarButtons, BRUV_Armature);
      if (bArm) bArm->visible = inEditUV && uvMode == UVModeBones && m && !m->Arm2DBones().empty(); }
    // menu Select: sin seleccion que operar en PINTURA (se pinta) ni en OBJETO (el click elige la
    // entidad y listo) -> se oculta en los dos.
    // menu Select (All/None/Invert/Select Linked): con el UV operativo en TODOS los modos que
    // tienen seleccion (edicion de UVs, Edit Bones y Pose). En PINTURA DE PESOS se OCULTA: ahi no
    // se selecciona nada, se pinta con el pincel (la mascara "solo lo seleccionado" usa la
    // seleccion del 3D, que se opera desde el menu Select del viewport 3D).
    { Button* bSelMenu = BarRoleBtn(BarButtons, BRUV_Select);
      if (bSelMenu) bSelMenu->visible = inEditUV && uvMode != UVModeWeights && uvMode != UVModeObject; }
    // SelMode/Pivot/Cursor/Animation: SOLO en los modos donde operan (ver el bloque de arriba);
    // los iconos dicen el sub-modo / el pivote actual.
    if (bSel) {
        bSel->visible = withSubSel;
        int mUV = UVMode();
        bSel->icon = (mUV == SelEdge) ? (int)IconType::selEdge :
                     (mUV == SelFace) ? (int)IconType::selFace : (int)IconType::selVertex;
    }
    if (bPiv) {
        bPiv->visible = withXform;
        // el icono dice el pivote EFECTIVO del UV, no el elegido: "Active Element" no existe en
        // 2D (el editor UV no tiene elemento activo) y el transform cae en la MEDIANA -> mostrar
        // el icono de pivotActive era un boton que mentia. El menu 2D ya no ofrece esa opcion
        // (LayoutMenuPivotUV), pero el estado es GLOBAL y puede venir puesto desde el 3D.
        bPiv->icon = (g_transformPivot == PivotCursor3D)   ? (int)IconType::pivotCursor :
                     (g_transformPivot == PivotIndividual) ? (int)IconType::pivotIndividual :
                                                             (int)IconType::pivotMedian;
    }
    if (bSnap) bSnap->visible = withXform; // ops del cursor 2D (pivote alternativo del transform)
    if (bAnim) bAnim->visible = withKeys;  // menu Animation: keyframes de la vertex anim / de la pose 2D
    // Texture: SIEMPRE visible, independiente de la seleccion (pedido del dueno): el
    // dropdown lista TODAS las texturas del proyecto ademas de las partes de la malla
    // activa. El texto = nombre de archivo de la MOSTRADA.
    if (bTex) {
        bTex->visible = (uvMode != UVModeTexture);
        std::string lbl = T("Texture");
        const std::string* path = NULL;
        if (!UVTexProjectPath().empty()) path = &UVTexProjectPath();
        else if (m) {
            int p = UVEffectivePart(m);
            Material* mm = (p >= 0 && p < (int)m->materialsGroup.size()) ? m->materialsGroup[p].material : NULL;
            if (mm && mm->texture && !mm->texture->path.empty()) path = &mm->texture->path;
        }
        if (path) {
            size_t sl = path->find_last_of("/\\");
            lbl = (sl == std::string::npos) ? *path : path->substr(sl + 1);
            Texture* tm = UVShownTexture(m);
            if (tm && TexEditUnsaved(tm)) lbl += "*";   // en memoria / con cambios sin guardar
        }
        bTex->text = lbl;
    }
    { Button* bEd = BarRoleBtn(BarButtons, BRUV_Edit);
      Button* bGu = BarRoleBtn(BarButtons, BRUV_Save);
      Texture* tm = UVShownTexture(m);
      const bool texpaint = (uvMode == UVModeTexture);
      if (bEd) {
          bEd->visible = (tm != NULL) || texpaint;
          const float* acc = ColorList[static_cast<int>(ColorID::accent)];
          static float greenEd[3]; for (int i = 0; i < 3; i++) greenEd[i] = acc[i] * 0.4f;
          bEd->tint = texpaint ? greenEd : NULL;
          bEd->textColor = texpaint ? acc : NULL;
      }
      if (bGu) bGu->visible = true;   // siempre: Export UV Layout no necesita textura (Save Texture sale solo si hay una en edicion)
    }
    // el MAPA UV que se ve y se edita (el activo de la malla): su nombre en el boton
    { Button* bMap = BarRoleBtn(BarButtons, BRUV_UVMap);
      if (bMap) {
          bMap->visible = (m != NULL) && uvMode != UVModeTexture;
          if (m && m->uvActiveMap >= 0 && m->uvActiveMap < (int)m->uvMaps.size()) bMap->text = m->uvMaps[m->uvActiveMap]->name;
          else bMap->text = "UVMap";   // (todavia sin capas: la del render, que es la primera)
      }
    }
    // Mipmap: inspector de la piramide de la textura mostrada. Visible cuando hay
    // textura; el texto dice que se esta viendo (Auto o el tamano del nivel).
    { Button* bMip = BarRoleBtn(BarButtons, BRUV_Mipmap);
      if (bMip) {
          unsigned int tid = UVShownTextureId(m);
          bMip->visible = (tid != 0) && uvMode != UVModeTexture;
          if (tid) {
              int level = UVMipLevel();
              if (level < 0 || !w3dEngine::TexHasMips(tid)) bMip->text = std::string("Mip: ") + T("Auto");
              else {
                  int tw = 0, th = 0;
                  w3dEngine::TextureSize(tid, tw, th);
                  for (int k = 0; k < level; k++) { if (tw > 1) tw /= 2; if (th > 1) th /= 2; }
                  char buf[32]; sprintf(buf, "Mip: %dx%d", tw, th);
                  bMip->text = buf;
              }
          }
      } }
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
static void UVFacesFilter3D(Mesh* m, std::vector<char>& fsel) {
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
    m->FacesSelByMode(fsel);
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
    const int glY = W3dScreenHeight - y - height;

    // fondo
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    const float* bg = ColorList[static_cast<int>(ColorID::background)];
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

    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh)
                  ? (Mesh*)ActiveObj : NULL;
    CalcAspectUV(m, m ? UVEffectivePart(m) : -1); // aspect ratio de la textura MOSTRADA (auto o la elegida en el dropdown)
    // barra superior por ROL (visibilidad/iconos/textos): estado puro, extraido a SyncBar (testeable)
    const bool inEditUV = (m && (Object*)m == g_editMesh);
    SyncBar();

    // contorno del cuadrado UV (0..1): referencia de los limites de la imagen
    {
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
        float c0x,c0y,c1x,c1y,c2x,c2y,c3x,c3y;
        UVtoScreen(0,0, cx,cy,s, c0x,c0y); UVtoScreen(1,0, cx,cy,s, c1x,c1y);
        UVtoScreen(1,1, cx,cy,s, c2x,c2y); UVtoScreen(0,1, cx,cy,s, c3x,c3y);
        float sq[16] = { c0x,c0y, c1x,c1y,  c1x,c1y, c2x,c2y,
                         c2x,c2y, c3x,c3y,  c3x,c3y, c0x,c0y };
        const float* gray = ColorList[static_cast<int>(ColorID::grayUI)];
        gfx::Color4f(gray[0], gray[1], gray[2], 1.0f);
        gfx::LineWidth(1.0f);
        gfx::VertexPointer2f(0, sq);
        gfx::DrawLines(8);
    }

    // sin malla activa el override de PROYECTO se muestra igual (el dropdown es
    // independiente de la seleccion): la textura sola, sin wireframe encima
    if (!m) {
        unsigned int idProj = UVTexProjectId();
        if (idProj) {
            gfx::Enable(gfx::Texture2D);
            gfx::EnableArray(gfx::TexCoordArray);
            gfx::BindTexture(idProj);
            gfx::TexWrap(repeatTexture);
            gfx::TexFilter(true);
            const bool mipShow = (g_uvMipLevel >= 0 && gfx::TexHasMips(idProj));
            if (mipShow) gfx::TexBaseLevel(g_uvMipLevel);
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
            if (mipShow) gfx::TexBaseLevel(0);
            gfx::Disable(gfx::Texture2D);
            gfx::DisableArray(gfx::TexCoordArray);
        }
    }

    if (m) {
        const int part = UVEffectivePart(m); // auto (parte activa) o la elegida a mano en el dropdown "Texture"
        Material* mat = (part < (int)m->materialsGroup.size())
                            ? m->materialsGroup[part].material : NULL;

        // --- la textura MOSTRADA, centrada: la del proyecto elegida en el dropdown
        // (3D o UI del juego via el cache 2D), o la de la parte activa ---
        unsigned int idProj = UVTexProjectId();
        unsigned int texId = idProj ? idProj
                                    : ((mat && mat->texture) ? mat->texture->iID : 0);
        if (texId) {
            gfx::Enable(gfx::Texture2D);
            gfx::EnableArray(gfx::TexCoordArray);
            gfx::BindTexture(texId);
            gfx::TexWrap(repeatTexture);
            gfx::TexFilter(mat ? mat->filtered : true);
            const bool mipShow = (g_uvMipLevel >= 0 && gfx::TexHasMips(texId));
            if (mipShow) gfx::TexBaseLevel(g_uvMipLevel);
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
            if (mipShow) gfx::TexBaseLevel(0);
            gfx::Disable(gfx::Texture2D);
            gfx::DisableArray(gfx::TexCoordArray);
        }

        // --- el wireframe de las UV encima: caras LOGICAS (quads/ngons), SIN triangular ---
        // SYNC SELECTION: ON -> dibuja TODO; las caras seleccionadas en 3D van verde (accent) y el
        // resto gris. OFF -> SOLO las caras seleccionadas. En Object Mode (sin seleccion) = todo verde.
        // en OBJECT MODE no se dibuja el wireframe de la malla sobre la textura: solo se ve
        // la textura. El wireframe (para editar las UV) aparece solo en EDIT MODE de esta malla.
        const bool inEdit = ((Object*)m == g_editMesh);

        // --- RELLENO por PESO (UVModeWeights): caras logicas trianguladas (fan) y coloreadas con la
        // MISMA rampa que el 3D (weightPaintColor, azul 0 -> amarillo 0.5 -> rojo 1), DEBAJO del
        // wireframe. Lo que se muestra aca es el UV GROUP activo (pesos por CORNER); el viewport
        // 3D muestra el VERTEX GROUP activo (por control-point): son dos grupos distintos. ---
        if (uvMode == UVModeWeights && inEdit && m->uv && !m->faces3d.empty()) {
            m->BuildWeightColorUV(m->uvActiveGroup); // por frame (barato): refleja el trazo en vivo
            const int nVw = m->vertexSize;
            // MISMO FILTRO DEL 3D QUE EL WIREFRAME: fuera de sync el UV solo muestra (y solo deja
            // pintar) las caras seleccionadas en el viewport 3D. El relleno por peso recorria
            // TODAS las caras -> aparecia color donde no habia wireframe, o sea color en zonas
            // donde el pincel no puede pintar. Ahora los dos dibujan exactamente lo mismo.
            std::vector<char> selWeight; UVFacesFilter3D(m, selWeight);
            if ((int)m->weightPaintColor.size() == nVw * 4) {
                std::vector<float> P; std::vector<unsigned char> C;
                for (size_t f = 0; f < m->faces3d.size(); f++) {
                    if (!syncSelection && !(f < selWeight.size() && selWeight[f])) continue;
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
        if (m->uv && !m->faces3d.empty() && inEdit) {
            const int nVerts = m->vertexSize;
            // relacion 3D <-> UV (UN solo punto): en sync espeja la seleccion del 3D; fuera de
            // sync solo INICIALIZA la seleccion propia cuando cambia el filtro (ver el header).
            // Devuelve de paso el FILTRO (caras seleccionadas en el 3D) que usa el dibujo.
            std::vector<char> sel3d;
            SyncFilter3D(m, &sel3d);
            std::vector<float> Lsel, Luns;          // ARISTAS: cara sel en 3D (accent) / no (gris)
            std::vector<float> Psel, Puns;          // PUNTOS del sub-modo (vertex/face)
            std::vector<float> Lwhite, Pwhite;      // SELECCION del editor UV (blanco): aristas / puntos
            // SUB-ELEMENTOS (puntitos de vertice/cara, aristas blancas de la seleccion UV): NO se
            // dibujan en MODO OBJETO. Ahi no hay sub-seleccion que operar (el click elige la
            // entidad: geometria o armature) y el 3D tampoco muestra los verts en Object Mode.
            // Antes quedaban los puntos BLANCOS de la ultima edicion flotando sobre el mapa.
            const bool subEl = inEdit && (uvMode != UVModeObject);
            const bool hasSelUV = subEl && (int)m->uvSelVert.size() == nVerts;
            const int curUVMode = UVMode();            // vertex/edge/face efectivo (propio o sincronizado)
            for (size_t f = 0; f < m->faces3d.size(); f++) {
                const std::vector<int>& id = m->faces3d[f].idx;
                const int nc = (int)id.size();
                if (nc < 2) continue;
                // en Object Mode todo cuenta como "seleccionado" (no hay sub-seleccion)
                const bool selFace = inEdit ? (f < sel3d.size() && sel3d[f]) : true;
                if (!syncSelection && !selFace) continue; // OFF: solo las seleccionadas
                std::vector<float>& L = selFace ? Lsel : Luns;
                std::vector<float>& P = selFace ? Psel : Puns;
                float fcu = 0, fcv = 0; int fn = 0;     // para el centro de la cara (modo face)
                bool faceAllSel = hasSelUV;             // modo face: TODOS los verts UV seleccionados?
                for (int c = 0; c < nc; c++) {            // arista c -> c+1 (cierra el poligono)
                    int ka = id[c], kb = id[(c+1) % nc];
                    if (ka < 0 || ka >= nVerts || kb < 0 || kb >= nVerts) { faceAllSel = false; continue; }
                    float ax,ay,bx,by;
                    UVtoScreen(m->uv[ka*2], m->uv[ka*2+1], cx,cy,s, ax,ay); L.push_back(ax); L.push_back(ay);
                    UVtoScreen(m->uv[kb*2], m->uv[kb*2+1], cx,cy,s, bx,by); L.push_back(bx); L.push_back(by);
                    // sub-modo VERTEX: un punto en cada UV-vert (blanco si esta seleccionado en el editor UV)
                    if (subEl && curUVMode == SelVertex) {
                        P.push_back(ax); P.push_back(ay);
                        if (hasSelUV && m->uvSelVert[ka]) { Pwhite.push_back(ax); Pwhite.push_back(ay); }
                    } else if (subEl && curUVMode == SelEdge) {
                        // sub-modo EDGE: arista blanca si sus 2 extremos UV estan seleccionados
                        if (hasSelUV && m->uvSelVert[ka] && m->uvSelVert[kb]) {
                            Lwhite.push_back(ax); Lwhite.push_back(ay); Lwhite.push_back(bx); Lwhite.push_back(by);
                        }
                    }
                    if (hasSelUV && !m->uvSelVert[ka]) faceAllSel = false;
                    fcu += m->uv[ka*2]; fcv += m->uv[ka*2+1]; fn++;
                }
                // sub-modo FACE: un punto en el centro UV de la cara (blanco si TODA la cara esta seleccionada)
                if (subEl && curUVMode == SelFace && fn > 0) {
                    float vx,vy; UVtoScreen(fcu/fn, fcv/fn, cx,cy,s, vx,vy);
                    P.push_back(vx); P.push_back(vy);
                    if (faceAllSel) { Pwhite.push_back(vx); Pwhite.push_back(vy); }
                }
            }
            const float* gr = ColorList[static_cast<int>(ColorID::grayUI)];
            const float* ac = ColorList[static_cast<int>(ColorID::accent)];
            // MODO OBJETO con la GEOMETRIA como objeto activo (uvObjArm == false): el wireframe va
            // MAS GRUESO y todo en accent -> la misma pista visual que el rig resaltado cuando el
            // objeto activo es un armature (ver UVRenderBones2D). Sin esto el modo objeto no daba
            // NINGUNA senal de que lo seleccionado era la malla.
            const bool geoHighlighted = (uvMode == UVModeObject && !uvObjArm);
            gfx::LineWidth(geoHighlighted ? 2.0f : 1.0f);
            if (!Luns.empty()) {
                if (geoHighlighted) gfx::Color4f(ac[0],ac[1],ac[2],1.0f);
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
        if (showChromeUV && m->chromeExpUV && m->chromeExpCount >= 3) {
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
    if (m && inEditUV)
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
    if (inEditUV) {
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

    // MARCAS del pincel: un cuadradito por punto pintable. En el UV el punto es el CORNER
    // (render-vert), que es la unidad que escribe su pincel (uv groups). No dependen del mouse
    // -- son un display, igual que en el viewport 3D.
    if (inEditUV && uvMode == UVModeWeights && BrushGet().marks != MarksOff && m && m->uv) {
        WPUVCtx cm; cm.m = m;
        ParamsUV(cm.cx, cm.cy, cm.s);   // el MISMO mapeo UV->screen que usa el pincel
        const unsigned char* colorRV =
            ((int)m->weightPaintColor.size() >= m->vertexSize * 4) ? &m->weightPaintColor[0] : NULL;
        BrushDrawMarks(m, BrushGet().marks, 7.0f * (float)GlobalScale, colorRV, WPUVProject, &cm);
    }
    // circulo del PINCEL (solo en modo pintura), siguiendo al mouse sobre el CONTENIDO
    // (no sobre la barra/toolbar ni con un popup modal abierto)
    if (((inEditUV && uvMode == UVModeWeights) || uvMode == UVModeTexture) && !PopUpActive &&
        lastMx >= x && lastMx < x + width && lastMy >= y && lastMy < y + height &&
        !OnBar(lastMx, lastMy) && !OnToolbar(lastMx, lastMy))
        BrushDrawCircle((float)(lastMx - x), (float)(lastMy - y), BrushGet().radiusPx);
    // el GIZMO 2D sobre los UVs elegidos (solo en modo edicion; ver Gizmo2DPivot)
    Gizmo2DRender(this);

    gfx::Disable(gfx::ScissorTest);
    // RenderBar y DrawBorders dibujan quads/bordes TEXTURIZADOS (iconos del atlas):
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
    if (XformInProgress()) RenderBarInfo(XformTextBar());
    else RenderBar();
    RenderToolbar();   // toolbar inferior compartida (G/R/S en edicion; tilde/cruz/X/Y en transform)
    DrawBorders(this); // borde del viewport (verde si es el activo)
}

void UVEditor::event_mouse_motion(int mx, int my) {
    // si hay un popup modal (file browser, etc.) el UV editor CEDE el input: no transforma
    // ni panea (sino un transform activo seguia robando el mouse con el browser abierto).
    if (PopUpActive) { lastMx = mx; lastMy = my; return; }
    if (Gizmo2DDragging()) { Gizmo2DMotion(this, mx, my); lastMx = mx; lastMy = my; return; }   // el gizmo 2D agarrado
    // BOX SELECT en curso: el arrastre agranda el rectangulo (el aplicar es al soltar).
    // Si el boton ya no esta apretado es un "up perdido" (se solto fuera del viewport): se
    // cierra igual, como hacen el trazo del pincel y el drag de huesos.
    if (uvBoxSel) {
        lastMx = mx; lastMy = my;
        if (!leftMouseDown) { BoxSelectRelease(); return; }
        uvBoxX1 = mx - x; uvBoxY1 = my - y;
        g_redraw = true;
        return;
    }
    // ===== ARMATURE 2D DEL MESH: transform de huesos en curso EN ESTE editor (edicion o pose) =====
    if (Bone2DXformActive() && gB2D.uv == this) {
        Mesh* mb = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        if (!mb) { lastMx = mx; lastMy = my; return; }
        if (gB2D.byClick && !leftMouseDown) { lastMx = mx; lastMy = my; Bone2DXformConfirm(mb); return; } // up perdido
        if (NumInputActive()) { lastMx = mx; lastMy = my; return; } // tipeando un valor exacto: el mouse no interfiere
        // cursor VIRTUAL wrap-safe: avanza por DELTAS filtrando el salto del warp (el cursor
        // se envuelve en el borde del viewport, como el 3D; mismo esquema que el G/R/S de UVs)
        UVXformAdvanceCursor(this, mx, my, gB2D.curU, gB2D.curV);
        lastMx = mx; lastMy = my;
        Bone2DXformDelta(mb, gB2D.curU, gB2D.curV);
        return;
    }
    // PINTURA DE PESOS: el circulo del pincel sigue al mouse (redraw) y, con un trazo en
    // curso (mouse apretado), cada motion pinta otra pasada y consume el evento.
    if (uvMode == UVModeWeights || uvMode == UVModeTexture) g_redraw = true;   // el circulo del pincel sigue al cursor
    if (gUVPaintingTex == this) {   // trazo de PINTURA DE TEXTURA en curso
        lastMx = mx; lastMy = my;
        if (leftMouseDown) { UVPaintTexture(this); return; }
        gUVPaintingTex = NULL; UVTexStrokeEnd();   // up perdido: commit igual
        return;
    }
    if (gUVPainting == this) {
        lastMx = mx; lastMy = my;
        Mesh* mp = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        if (leftMouseDown && mp) { UVPaintWeights(this, mp); return; }
        gUVPainting = NULL; WeightPaintStrokeEnd(); // up perdido (solto fuera): commit igual
        return;
    }
    if (uvXform) {                         // transform en curso: mover la seleccion en vivo
        Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        if (m) {
            if (NumInputActive()) { lastMx = mx; lastMy = my; return; } // tipeando: el mouse no interfiere
            // cursor VIRTUAL wrap-safe: acumula los deltas del mouse FILTRANDO el salto del
            // warp (CheckWarpMouseInViewport envuelve el cursor en el borde, como el 3D).
            // El mapeo px->UV es el mismo de UVMouseAUV (CON el aspecto de la textura).
            UVXformAdvanceCursor(this, mx, my, uvXCurU, uvXCurV);
            ApplyXform(m, uvXCurU, uvXCurV);
        }
    } else if (middleMouseDown) {          // boton del medio: paneo
        // SALTEAR el salto del warp: el cursor se envuelve en el borde del viewport
        // (CheckWarpMouseInViewport) y mx/my pegan un salto del ancho/alto. Sin el
        // filtro, ese delta gigante revertia el paneo acumulado (mismo fix que el
        // paneo del Editor 2D).
        int jx = mx - lastMx, jy = my - lastMy;
        bool jump = (jx > width / 2 || jx < -width / 2 || jy > height / 2 || jy < -height / 2);
        if (!jump) {
            panX += (float)jx;
            panY += (float)jy;
        }
        g_redraw = true;
    }
    lastMx = mx; lastMy = my;
}

// parametros del mapeo UV->screen (identicos al Render/PickUV): centro + escala.
void UVEditor::ParamsUV(float& cx, float& cy, float& s) const {
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    cx = width * 0.5f + panX;
    cy = top + ch * 0.5f + panY;
    float baseSize = (float)(width < ch ? width : ch) * 0.8f;
    s = baseSize * zoom; if (s < 1.0f) s = 1.0f;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    CalcAspectUV(m, m ? UVEffectivePart(m) : -1); // mismo aspecto que el Render (incluido el
                                                   // override del dropdown Texture) -> el pick coincide
}

// caras LOGICAS seleccionadas para el transform en modo CARA. En sync = las caras seleccionadas
// en el 3D (el filtro, lo mismo que resalta el render); fuera de sync = las caras cuyos corners
// estan TODOS en uvSelVert (la seleccion PROPIA del UV, por copia).
static void UVFacesSel(UVEditor* uv, Mesh* m, std::vector<char>& fsel) {
    const bool inEdit = ((Object*)m == g_editMesh);
    if (uv->syncSelection && inEdit) { UVFacesFilter3D(m, fsel); return; }
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
//  RENDER-VERT y GenerateRender MERGEA los corners con la MISMA (pos + uv + normal + color) -> dos
//  caras vecinas con el mismo UV en una esquina COMPARTEN el render-vert, y mover una cara movia esa
//  esquina de la otra. FIX: al arrancar un transform en modo CARA, cada render-vert compartido entre
//  una cara SELECCIONADA y una NO seleccionada se DUPLICA para las seleccionadas (split de esquina):
//  mover una cara afecta EXACTAMENTE sus UVs. Se mantienen coherentes los arrays paralelos
//  (posRep / vertCtrlPoint / uvSelVert / uv2dRest) y los keyframes de vertex anim (RemapVertexAnims:
//  el vert duplicado SIGUE la animacion de su origen). Si se CANCELA el transform, los duplicados
//  quedan con el MISMO uv que su origen y el proximo GenerateRender los re-mergea (inofensivo).
// ===================================================================================================
static void UVSeparateFacesSel(Mesh* m, const std::vector<char>& fsel) {
    const int oldN = m->vertexSize;
    if (oldN <= 0) return;
    // COW: arma arrays nuevos LEYENDO los viejos y despues los libera: si son de un recurso
    // compartido, primero se copian (los de los demas usuarios no se tocan)
    m->UninstantiateData(W3DMD_ALL);
    std::vector<char> usesSel(oldN, 0), usesUnsel(oldN, 0);
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        std::vector<char>& dst = (f < fsel.size() && fsel[f]) ? usesSel : usesUnsel;
        for (size_t c = 0; c < id.size(); c++)
            if (id[c] >= 0 && id[c] < oldN) dst[id[c]] = 1;
    }
    std::vector<int> newOf(oldN, -1); // render-vert compartido -> su copia para las caras sel
    std::vector<int> dupSrc;          // origen de cada vert nuevo
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        if (f >= fsel.size() || !fsel[f]) continue;
        std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c];
            if (k < 0 || k >= oldN || !usesUnsel[k]) continue; // no compartido: queda como esta
            if (newOf[k] < 0) { newOf[k] = oldN + (int)dupSrc.size(); dupSrc.push_back(k); }
            id[c] = newOf[k];
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
        std::vector<float> wOld((size_t)oldN, 0.0f);
        for (size_t k = 0; k < ug->verts.size() && k < ug->weights.size(); k++) {
            int rv = ug->verts[k];
            if (rv >= 0 && rv < oldN) wOld[(size_t)rv] = ug->weights[k];
        }
        for (size_t d = 0; d < dupSrc.size(); d++) {
            float w = wOld[(size_t)dupSrc[d]];
            if (w > 0.0f) { ug->verts.push_back(oldN + (int)d); ug->weights.push_back(w); }
        }
    }
    m->vertexSize = newN;
    // keyframes de vertex anim: el vert duplicado SIGUE la animacion de su origen (posiciones,
    // normales y UV se copian keyframe a keyframe)
    { std::vector<int> n2o(newN);
      for (int i = 0; i < oldN; i++) n2o[i] = i;
      for (size_t d = 0; d < dupSrc.size(); d++) n2o[oldN + (int)d] = dupSrc[d];
      RemapVertexAnims(m, &n2o[0], newN, oldN); }
    m->RegroupMeshParts(); // rearma faces[] desde faces3d (y bumpea skinGeomVersion -> VBO/CSR)
}

// habra SPLIT de esquinas? (mismo criterio que UVSeparateFacesSel: algun render-vert compartido
// entre una cara SELECCIONADA y una NO seleccionada). Se decide ANTES de mutar nada para elegir
// el TIPO de undo del transform de cara: con split va MeshGeoUndo COMPLETO (cambia la topologia
// de render, el snapshot liviano de la capa uv no puede deshacer eso); sin split alcanza el
// liviano (ver la decision documentada en Undo.cpp).
static bool UVSplitNeeded(Mesh* m, const std::vector<char>& fsel) {
    const int oldN = m->vertexSize;
    if (oldN <= 0) return false;
    std::vector<char> usesSel(oldN, 0), usesUnsel(oldN, 0);
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        std::vector<char>& dst = (f < fsel.size() && fsel[f]) ? usesSel : usesUnsel;
        for (size_t c = 0; c < id.size(); c++)
            if (id[c] >= 0 && id[c] < oldN) dst[id[c]] = 1;
    }
    for (int i = 0; i < oldN; i++) if (usesSel[i] && usesUnsel[i]) return true;
    return false;
}

// snapshot de la seleccion + pivot + mouse inicial. modo: 1=mover 2=rotar 3=escalar.
// PIVOT (correccion del dueño): la MEDIANA de la seleccion = el promedio de las posiciones UV
// UNICAS de los verts seleccionados. Antes se promediaban TODOS los render-verts seleccionados:
// las esquinas duplicadas por splits (flat shading / costuras) contaban DOBLE y corrian el pivote.
void UVEditor::StartXform(Mesh* m, int mode) {
    if (uvMode != UVModeEdit) return; // pintura/huesos/pose: el G/R/S de UVs ignora el input
    if (!m || !m->uv) return;
    m->UninstantiateData(W3DMD_UV);   // COW: el transform escribe uv[] en el lugar (recurso compartido)
    SyncSelFrom3D(m);   // sync: mover lo SELECCIONADO en el 3D (uvSelVert espeja al 3D)
    uvXIdx.clear(); uvXOrig.clear(); uvXPivots.clear(); uvXAxis = 0;
    const int curUVMode = UVMode();
    std::vector<char> fsel;
    if (curUVMode == SelFace) {
        // MODO CARA: el transform afecta EXACTAMENTE las esquinas de las caras seleccionadas
        // (con split previo de las esquinas compartidas con caras NO seleccionadas; ver arriba).
        UVFacesSel(this, m, fsel);
        bool has = false; for (size_t f = 0; f < fsel.size(); f++) if (fsel[f]) { has = true; break; }
        if (!has) return;
        // UNDO (pendiente hasta confirmar): el tipo de snapshot se decide ANTES de mutar. Con
        // split de esquinas el liviano no alcanza -> MeshGeoUndo completo PRE-split.
        if (UVSplitNeeded(m, fsel)) UndoUVStartComplete(m); else UndoUVStart(m);
        UVSeparateFacesSel(m, fsel);
        std::vector<char> seen(m->vertexSize, 0);
        for (size_t f = 0; f < fsel.size(); f++) {
            if (!fsel[f]) continue;
            const std::vector<int>& id = m->faces3d[f].idx;
            for (size_t c = 0; c < id.size(); c++) {
                int k = id[c];
                if (k < 0 || k >= m->vertexSize || seen[k]) continue;
                seen[k] = 1;
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
    if (uvXIdx.empty()) { UndoUVCancel(); return; } // nada seleccionado -> no arranca (descarta el pendiente)
    // UNDO liviano para verts/bordes: recien aca se sabe que el transform ARRANCA (nada muto todavia).
    // El modo cara ya inicio el suyo arriba (antes del split).
    if (curUVMode != SelFace) UndoUVStart(m);
    // PIVOT segun el menu (mismo g_transformPivot que el 3D): 3D Cursor = el cursor 2D;
    // Individual (en modo cara) = cada cara alrededor de su centro; el resto = MEDIANA.
    if (g_transformPivot == PivotCursor3D) { uvXPivotU = uvCursorU; uvXPivotV = uvCursorV; }
    else {
        // MEDIANA: promedio de las posiciones UV UNICAS (los splits en el mismo lugar cuentan 1 vez)
        std::set< std::pair<float,float> > unique;
        double su = 0, sv = 0;
        for (size_t i = 0; i < uvXIdx.size(); i++) {
            std::pair<float,float> p(uvXOrig[i*2], uvXOrig[i*2+1]);
            if (unique.insert(p).second) { su += p.first; sv += p.second; }
        }
        uvXPivotU = (float)(su / (double)unique.size());
        uvXPivotV = (float)(sv / (double)unique.size());
    }
    if (g_transformPivot == PivotIndividual && curUVMode == SelFace) {
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
    uvXform = mode;
    // historial MRU de la toolbar del UV: teclado (G/R/S) y toolbar alimentan el mismo historial
    ToolbarMRU(UVToolHist(), (mode == 1) ? TBMove : (mode == 2) ? TBRotate : TBScale);
    g_redraw = true;
}

// aplica el transform en vivo desde la base ORIGINAL (no acumula): deriva delta/angulo/factor
// de la posicion del cursor (virtual) y delega en ApplyXformValues (que aplica el lock).
void UVEditor::ApplyXform(Mesh* m, float curU, float curV) {
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
    ApplyXformValues(m, dU, dV, ang, f);
}

// NUCLEO del apply: mover/rotar/escalar respecto al pivot (o al pivote POR VERT si Individual)
// con los VALORES ya resueltos; uvXAxis (X/Y) restringe ADENTRO (en espacio de la vista). Lo
// comparten el mouse (ApplyXform) y la entrada numerica exacta (XformNumValue). El angulo
// va en RADIANES; los valores aplicados quedan en uvXVal* para la barra de INFO.
void UVEditor::ApplyXformValues(Mesh* m, float dU, float dV, float ang, float f) {
    if (!uvXform || uvXIdx.empty() || !m || !m->uv) return;
    const bool byVert = (uvXPivots.size() == uvXIdx.size() * 2);
    if (uvXform == 1) {                    // MOVER
        if (uvXAxis == 1) dV = 0.0f;       // bloqueo X: solo horizontal
        if (uvXAxis == 2) dU = 0.0f;       // bloqueo Y: solo vertical
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            m->uv[k*2]   = uvXOrig[i*2]   + dU;
            m->uv[k*2+1] = uvXOrig[i*2+1] + dV; }
    } else if (uvXform == 2) {             // ROTAR alrededor del pivot (el eje no aplica en 2D)
        float c = cosf(ang), s2 = sinf(ang);
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            float pu = byVert ? uvXPivots[i*2] : uvXPivotU, pv = byVert ? uvXPivots[i*2+1] : uvXPivotV;
            float ru = uvXOrig[i*2] - pu, rv = uvXOrig[i*2+1] - pv;
            m->uv[k*2]   = pu + ru*c - rv*s2;
            m->uv[k*2+1] = pv + ru*s2 + rv*c; }
    } else if (uvXform == 3) {             // ESCALAR desde el pivot (con bloqueo de eje opcional)
        float fx = (uvXAxis == 2) ? 1.0f : f;   // bloqueo Y: X no escala
        float fy = (uvXAxis == 1) ? 1.0f : f;   // bloqueo X: Y no escala
        for (size_t i = 0; i < uvXIdx.size(); i++) { int k = uvXIdx[i];
            float pu = byVert ? uvXPivots[i*2] : uvXPivotU, pv = byVert ? uvXPivots[i*2+1] : uvXPivotV;
            m->uv[k*2]   = pu + (uvXOrig[i*2]   - pu) * fx;
            m->uv[k*2+1] = pv + (uvXOrig[i*2+1] - pv) * fy; }
    }
    uvXValU = dU; uvXValV = dV;                 // valores para la barra de INFO
    uvXValAng = ang * 57.29578f;                // en grados
    uvXValFac = f;
    g_redraw = true;
}

void UVEditor::ConfirmXform() {          // deja el cambio aplicado
    bool hadXform = (uvXform != 0);
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    // hubo cambio REAL? comparar los uv vivos contra la base del transform (uvXOrig). Un
    // confirmar sin mover (o un split de esquinas sin mover) NO pushea un paso de undo.
    bool change = false;
    if (hadXform && m && m->uv)
        for (size_t i = 0; i < uvXIdx.size() && !change; i++) { int k = uvXIdx[i];
            if (k >= 0 && k < m->vertexSize &&
                (m->uv[k*2] != uvXOrig[i*2] || m->uv[k*2+1] != uvXOrig[i*2+1])) change = true; }
    uvXform = 0; uvXAxis = 0; uvXIdx.clear(); uvXOrig.clear(); uvXPivots.clear(); g_redraw = true;
    NumInputReset(); // la entrada numerica muere con el transform (como el Aceptar del 3D)
    if (!hadXform) return;
    UndoUVConfirm(change); // pushea el pendiente (liviano o completo); sin cambio lo descarta
    if (change && m) W3dMeshMarkEdited(m);   // la malla de un RECURSO: la ven todos sus objetos
    // ARMATURE 2D - INVARIANTE uv = f(uv2dRest, pose) (ver Mesh.h): editar los UV a mano REDEFINE
    // el rest. Con la pose en identidad el rest ES el uv; con una pose puesta se invierte el
    // skinning para que la edicion quede donde el usuario la solto y SOBREVIVA al re-evaluar.
    // Antes esto solo limpiaba el rest con pose identidad: con una pose activa (o con el rest
    // capturado y el armature borrado) la edicion se perdia en el proximo Armature2DApply.
    if (m) m->Armature2DRestFromUV();
    // AUTO KEY de UV: si hay una vertex anim de ESTA malla activa en el timeline y Auto Key
    // esta prendido, editar el mapeo captura un keyframe EN LA CURVA UV (separada de la de
    // vertices: VertexAnimInsertKeyframeUV guarda SOLO el mapeo de la pose actual).
    extern bool AutoKeyOn; extern int ActiveAnimKind; extern Mesh* ActiveAnimMesh;
    extern void VertexAnimInsertKeyframeUV();
    if (AutoKeyOn && ActiveAnimKind == 3 && m && (Object*)m == g_editMesh && (Mesh*)ActiveAnimMesh == m)
        VertexAnimInsertKeyframeUV();
}

void UVEditor::CancelXform(Mesh* m) {    // restaura los uv originales
    if (uvXform) UndoUVCancel();         // el pendiente de undo se descarta (no hubo operacion)
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
// posRep (VertsSelByMode), como siempre.
bool UVVertsSelEffective(Mesh* m, std::vector<char>& sv) {
    sv.clear();
    if (!m || m->vertexSize <= 0) return false;
    const int nV = m->vertexSize;
    bool hasEditorOwn = false;
    for (size_t i = 0; i < gUVEditors.size(); i++)
        if (gUVEditors[i] && !gUVEditors[i]->syncSelection) { hasEditorOwn = true; break; }
    if (hasEditorOwn && (int)m->uvSelVert.size() == nV) {
        bool has = false;
        for (int i = 0; i < nV && !has; i++) if (m->uvSelVert[i]) has = true;
        if (has) {
            sv.assign((size_t)nV, 0);
            for (int i = 0; i < nV; i++) sv[i] = m->uvSelVert[i] ? 1 : 0;
            return true;
        }
    }
    m->VertsSelByMode(sv);
    if ((int)sv.size() != nV) { sv.clear(); return false; }
    for (int i = 0; i < nV; i++) if (sv[i]) return true;
    return false;
}

// NUCLEO COMPARTIDO (tarjeta "Transform UV" del panel Properties): mueve (dU,dV) los UV de la
// SELECCION EFECTIVA (UVVertsSelEffective: la propia del UV si la hay, si no la del 3D), con el
// mismo tratamiento que el confirmar de un G de UVs: undo LIVIANO (UndoUV*, punto 1),
// invalidacion del rest 2D con pose identidad y re-subida del VBO. No hace split de caras (solo
// cambia valores de uv -> el liviano alcanza SIEMPRE aca).
bool UVMoveSelectionEdit(Mesh* m, float dU, float dV) {
    if (!m || !m->uv || m->vertexSize <= 0) return false;
    if ((Object*)m != g_editMesh) return false;              // solo en Edit Mode de esta malla
    if (dU == 0.0f && dV == 0.0f) return false;              // no-op: ni undo ni redraw
    m->UninstantiateData(W3DMD_UV);                          // COW (recurso compartido)
    W3dMeshMarkEdited(m);
    std::vector<char> sv;
    if (!UVVertsSelEffective(m, sv)) return false;
    UndoUVStart(m);                                        // snapshot ANTES (pendiente)
    for (int i = 0; i < m->vertexSize; i++) if (sv[i]) { m->uv[i*2] += dU; m->uv[i*2+1] += dV; }
    UndoUVConfirm(true);                                   // un paso de undo por edicion de campo
    // igual que ConfirmXform: editar los UV a mano REDEFINE el rest del skinning 2D
    // (invariante uv = f(uv2dRest, pose), ver Mesh.h)
    m->Armature2DRestFromUV();
    m->skinGeomVersion++;                                    // re-subir el VBO de uv
    g_redraw = true;
    return true;
}

// ===== hooks del TRANSFORM UI COMPARTIDO (ver TransformUI.h): el UV editor los implementa
// para que la barra de info, la entrada numerica, el warp del cursor y el tilde/cruz/X/Y de
// la toolbar sean EL MISMO mecanismo que en el 3D y el Editor 2D, sin duplicar codigo. =====

// hay un G/R/S modal propio? (de UVs, o de huesos 2D arrancado EN este editor)
bool UVEditor::XformInProgress() const {
    return uvXform != 0 || (Bone2DXformActive() && gB2D.uv == this);
}

// valor EXACTO tipeado (NumInput): mover = v en unidades UV (1.0 = un tile completo: con
// textura repetida queda "igual pero corrido", clave para animacion UV); el lock X/Y filtra.
// Sin lock aplica en los dos ejes (como el translate libre del 3D, que aplica en los 3).
// Rotar = v en grados; escalar = factor v. Reusa los nucleos *AplicarValores (cero loops nuevos).
void UVEditor::XformNumValue(float v) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m) return;
    const float DEG = 3.14159265358979f / 180.0f;
    if (uvXform) {
        if (uvXform == 1)      ApplyXformValues(m, v, v, 0.0f, 1.0f);
        else if (uvXform == 2) ApplyXformValues(m, 0.0f, 0.0f, v * DEG, 1.0f);
        else                   ApplyXformValues(m, 0.0f, 0.0f, 0.0f, v);
        return;
    }
    // HUESOS 2D: MISMA semantica (el lock X/Y lo aplica B2DApplyValues adentro -> con un eje
    // bloqueado el delta queda EXACTO en ese eje y CERO en el otro; sin lock aplica en los dos).
    if (Bone2DXformActive() && gB2D.uv == this) {
        if (gB2D.mode == 1)      B2DApplyValues(m, v, v, 0.0f, 1.0f);
        else if (gB2D.mode == 2) B2DApplyValues(m, 0.0f, 0.0f, v, 1.0f); // (grados)
        else                     B2DApplyValues(m, 0.0f, 0.0f, 0.0f, v);
    }
}

// confirmar (Enter / click / tilde): una expresion tipeada INVALIDA no confirma (la barra
// ya muestra "= ?" como feedback). Es el funnel comun de TODOS los caminos de confirmar.
void UVEditor::XformConfirm() {
    if (!TransformUICanConfirm()) { g_redraw = true; return; }
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (Bone2DXformActive() && gB2D.uv == this) { if (m) Bone2DXformConfirm(m); return; }
    if (uvXform) ConfirmXform();
}

void UVEditor::XformCancel() {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (Bone2DXformActive() && gB2D.uv == this) { if (m) Bone2DXformCancel(m); NumInputReset(); return; }
    if (uvXform && m) CancelXform(m);
}

// toggle del BLOQUEO de eje (1=X 2=Y), en espacio de la vista: teclas X/Y y botones de la
// toolbar entran por aca (UN solo camino) TANTO para el G/R/S de UVs como para el transform
// modal de HUESOS 2D. Re-aplica al instante con el eje nuevo (o con el valor tipeado).
void UVEditor::XformToggleAxis(int axis) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (uvXform) {
        uvXAxis = (axis == 1) ? ((uvXAxis == 1) ? 0 : 1) : ((uvXAxis == 2) ? 0 : 2);
        if (m) {
            float v;
            if (NumInputActive() && NumInputValue(v)) XformNumValue(v);  // el valor tipeado, al eje nuevo
            else ApplyXform(m, uvXCurU, uvXCurV);                        // re-aplica YA (cursor virtual)
        }
        g_redraw = true;
        return;
    }
    // HUESOS 2D: mismo comportamiento (mover y escalar aceptan X/Y; rotar no tiene eje en 2D)
    if (!(Bone2DXformActive() && gB2D.uv == this)) return;
    if (gB2D.mode == 2) return;                                          // rotacion 2D: sin eje
    gB2D.axis = (axis == 1) ? ((gB2D.axis == 1) ? 0 : 1) : ((gB2D.axis == 2) ? 0 : 2);
    if (m) {
        float v;
        if (NumInputActive() && NumInputValue(v)) XformNumValue(v);      // el valor tipeado, al eje nuevo
        else Bone2DXformDelta(m, gB2D.curU, gB2D.curV);                  // re-aplica YA (cursor virtual)
    }
    g_redraw = true;
}

// texto de la barra de INFO (formato compartido con el 3D y el 2D; valores en unidades UV)
std::string UVEditor::XformTextBar() {
    if (uvXform) return TransformUIText2D(uvXform, uvXValU, uvXValV, uvXValAng, uvXValFac,
                                           uvXAxis, "X", "Y", "");
    if (Bone2DXformActive() && gB2D.uv == this)
        return TransformUIText2D(gB2D.mode, gB2D.valU, gB2D.valV, gB2D.valAng, gB2D.valFac,
                                  gB2D.axis, "X", "Y", "");
    return std::string();
}

// paneo de la vista UV (compartido PC/Symbian). dx>0 mueve el contenido a la derecha (revela la izquierda).
void UVEditor::Pan(float dx, float dy) { panX += dx; panY += dy; g_redraw = true; }

// TOUCH: 1 dedo sobre el CONTENIDO = panear la vista UV. La barra la maneja el gesto lockeado.
bool UVEditor::event_finger_scroll(int px, int py, int dx, int dy){
    if (Gizmo2DDragging()) return false;   // el dedo tiene agarrado el gizmo: el arrastre es del gizmo, no panea
    Pan((float)dx, (float)dy);
    return true;
}
// TOUCH: 2 dedos = zoom (pinch) + paneo del centroide.
void UVEditor::event_finger_gesture(float zoomDelta, float panDx, float panDy){
    if (zoomDelta > 1.0f)       ZoomCenter(1);
    else if (zoomDelta < -1.0f) ZoomCenter(-1);
    if (panDx != 0.0f || panDy != 0.0f) Pan(panDx, panDy);
}

// ================================================================== GIZMO 2D (Gizmo.h)
static Mesh* UVMeshGizmo(const UVEditor* uv) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || !m->uv || (Object*)m != g_editMesh || uv->uvMode != UVModeEdit) return NULL;
    return m;
}
// el pivote del G/R/S (el mismo que StartXform): el cursor 2D con Pivot = "3D Cursor"; si no, la MEDIANA de las
// posiciones UV elegidas (cada posicion una vez: los splits en el mismo lugar no pesan doble)
bool UVEditor::Gizmo2DPivot(float& lx, float& ly) {
    Mesh* m = UVMeshGizmo(this);
    if (!m || uvBoxSel || uvBoxBuilt || Bone2DXformActive()) return false;
    if ((int)m->uvSelVert.size() != m->vertexSize) return false;
    std::set< std::pair<float,float> > unique;
    double su = 0, sv = 0;
    for (int i = 0; i < m->vertexSize; i++) {
        if (!m->uvSelVert[i]) continue;
        std::pair<float,float> p(m->uv[i*2], m->uv[i*2+1]);
        if (unique.insert(p).second) { su += p.first; sv += p.second; }
    }
    if (unique.empty()) return false;
    float pu = (float)(su / (double)unique.size()), pv = (float)(sv / (double)unique.size());
    if (g_transformPivot == PivotCursor3D) { pu = uvCursorU; pv = uvCursorV; }
    float cx, cy, s; ParamsUV(cx, cy, s);
    const float au = (g_uvAspU > 1e-6f) ? g_uvAspU : 1.0f, av = (g_uvAspV > 1e-6f) ? g_uvAspV : 1.0f;
    lx = cx + (pu - 0.5f) * s * au;
    ly = cy + (pv - 0.5f) * s * av;
    return true;
}
bool UVEditor::Gizmo2DInZone(float lx, float ly) const {
    UVEditor* yo = const_cast<UVEditor*>(this);
    const int gx = x + (int)lx, gy = y + (int)ly;
    return ly >= (float)BarTopOffset() && !yo->OnToolbar(gx, gy) && !yo->OnBrushBar(gx, gy);
}
bool UVEditor::Gizmo2DStart(int mode, int axis) {
    Mesh* m = UVMeshGizmo(this); if (!m) return false;
    StartXform(m, mode + 1);
    if (!uvXform) return false;
    uvXAxis = axis;
    return true;
}
void UVEditor::Gizmo2DApply(float dx, float dy, float ang, float f) {
    Mesh* m = UVMeshGizmo(this); if (!m || !uvXform) return;
    float cx, cy, s; ParamsUV(cx, cy, s);
    const float au = (g_uvAspU > 1e-6f) ? g_uvAspU : 1.0f, av = (g_uvAspV > 1e-6f) ? g_uvAspV : 1.0f;
    // (la V crece hacia ABAJO en pantalla, como la Y: el angulo de pantalla vale tal cual)
    ApplyXformValues(m, dx / (s * au), dy / (s * av), ang, f);
}
void UVEditor::Gizmo2DConfirm() { if (uvXform) ConfirmXform(); }
void UVEditor::Gizmo2DCancel() {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (uvXform) CancelXform(m);
}

// 2 DEDOS: paneo + zoom ANCLADO al punto medio de los dedos y proporcional a cuanto se abren (antes: pasos fijos de 5%
// desde el centro, que el paneo de dos dedos disparaba solo)
bool UVEditor::event_finger_pinch(float cx, float cy, float panDx, float panDy, float sx0, float sy0, float sx1, float sy1){
    if (panDx != 0.0f || panDy != 0.0f) Pan(panDx, panDy);
    const float d0 = sqrtf(sx0*sx0 + sy0*sy0), d1 = sqrtf(sx1*sx1 + sy1*sy1);
    const float minSep = 24.0f * (float)GlobalScale;
    if (d0 >= minSep && d1 >= minSep) {
        float f = d1 / d0;
        float nz = zoom * f;
        if (nz < 0.05f) nz = 0.05f;
        if (nz > 50.0f) nz = 50.0f;
        f = nz / zoom;
        const int top = BarTopOffset();
        int ch = height - top; if (ch < 1) ch = 1;
        const float uvCx = width * 0.5f + panX, uvCy = top + ch * 0.5f + panY;
        const float curX = cx - (float)x, curY = cy - (float)y;   // el punto medio de los dedos, LOCAL
        panX += (curX - uvCx) * (1.0f - f);
        panY += (curY - uvCy) * (1.0f - f);
        zoom = nz;
    }
    g_redraw = true;
    return true;
}

// zoom CENTRADO en el viewport (sin cursor; para el teclado 0+arriba/abajo de Symbian). Es el zoom de la rueda
// con el "cursor" en el centro: ahi (curX-uvCx) = -panX -> panX queda *= f (el centro de pantalla no se mueve).
void UVEditor::ZoomCenter(int dir) {
    float f = (dir > 0) ? 1.05f : (1.0f / 1.05f); // suave por frame (la rueda usa 1.1 por notch)
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    f = nz / zoom;                 // factor real tras el clamp
    zoom = nz; panX *= f; panY *= f;
    g_redraw = true;
}

// teclas del editor UV: flechas = paneo (cualquier modo); G/R/S inician mover/rotar/escalar; ESC/ENTER transform.
extern bool LShiftPressed, LCtrlPressed, LAltPressed;
void UVEditor::event_key_down(int keyCode, bool repeat) {
    if (PopUpActive) return;                    // un popup modal abierto (file browser) tiene prioridad
    const int k = keyCode;
    // TAB = entrar/salir de la EDICION DE HUESOS 2D (ciclo documentado en UVEditor.h). UNICO lugar
    // que togglea: el Tab global (Object<->Edit Mode del 3D) no corre porque controls.cpp consulta
    // UVEditorTakeTab ANTES y, si el UV se la queda, deja que la tecla llegue hasta aca por el
    // ruteo normal de teclas al viewport activo (foco por hover).
    if (k == W3dK_TAB) { TabToggleBones(); return; }
    // BOX SELECT armado o en curso: ESC lo cancela ANTES que cualquier otra cosa (mismo reflejo
    // que cancelar un transform).
    if ((uvBoxBuilt || uvBoxSel) && k == W3dK_ESCAPE) { BoxSelectCancel(); return; }
    // NUMPAD . = ENCUADRAR la seleccion. MISMO atajo y mismo significado que en el viewport 3D
    // (Frame Selected), el Editor 2D y el Timeline.
    if (k == W3dK_KP_PERIOD) { FrameUV(false); return; }
    // PANEO de la vista con las flechas (en cualquier modo): la flecha revela ese lado
    const float pp = (float)GlobalScale * 16.0f;
    if (k == W3dK_LEFT)  { Pan(+pp, 0); return; }
    if (k == W3dK_RIGHT) { Pan(-pp, 0); return; }
    if (k == W3dK_UP)    { Pan(0, +pp); return; }
    if (k == W3dK_DOWN)  { Pan(0, -pp); return; }
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || (Object*)m != g_editMesh) return; // el resto solo en Edit Mode sobre esta malla
    // B = BOX SELECT: ARMA el gesto y
    // el proximo arrastre con el boton izquierdo dibuja el rectangulo. Solo en los modos con
    // seleccion (en pintura se pinta y en objeto el click elige la entidad, no hay caja que hacer).
    if (k == W3dK_B && !XformInProgress() &&
        (uvMode == UVModeEdit || uvMode == UVModeBones || uvMode == UVModePose)) {
        BoxSelectBuild();
        return;
    }
    // ===== ARMATURE 2D DEL MESH: atajos en los modos Huesos/Pose (mismas teclas que el 3D) =====
    if (uvMode == UVModeBones || uvMode == UVModePose) {
        // (los modificadores salen de los flags globales y no del teclado fisico: un atajo reasignado llega con
        //  los modificadores de SU combinacion por defecto, ver app/W3dShortcuts.h)
        bool shift = LShiftPressed;
        bool alt   = LAltPressed;
        if (Bone2DXformActive()) {      // transform de huesos en curso: Enter confirma / Esc cancela
            // (via los hooks compartidos: mismo funnel que el tilde/cruz de la toolbar, con
            // el gate de la entrada numerica invalida y el reset del NumInput)
            if (k == W3dK_ESCAPE) XformCancel();
            else if (k == W3dK_RETURN || k == W3dK_KP_ENTER) XformConfirm();
            // BLOQUEO DE EJE: mismas teclas y mismo hook que el transform de UVs (en rotar es
            // un no-op). Ojo: X aca NO borra huesos (el borrado solo llega fuera del transform).
            else if (k == W3dK_X || k == W3dK_Y) XformToggleAxis((k == W3dK_X) ? 1 : 2);
            return;
        }
        bool ctrl  = LCtrlPressed;
        // A = seleccionar TODOS los huesos / Alt+A = deseleccionar todos (como el 3D). Con la
        // seleccion completa, las ops (G/R/S, X, Shift+D, Ctrl+P) operan sobre todo.
        if (k == W3dK_A) { Bone2DSelectAll(m, !alt); return; }
        // Ctrl+I = invertir / L = Select Linked (la CADENA conectada). Mismos atajos que el menu
        // Select de la barra y que el del viewport 3D. Ctrl+I va ANTES que la I de Insert Keyframe.
        if (k == W3dK_I && ctrl) { Bone2DInvertSelection(m); return; }
        if (k == W3dK_L) { Bone2DSelectLinked(m, shift); return; }
        if (k == W3dK_G || k == W3dK_R || k == W3dK_S) {
            // G/R/S: en POSE edita poseT/poseRot/poseS (los UV pesados siguen al hueso en vivo);
            // en HUESOS mueve/rota/escala el REST (head/tail) de los seleccionados
            Bone2DXformStart(m, (k == W3dK_G) ? 1 : (k == W3dK_R) ? 2 : 3);
            return;
        }
        if (uvMode == UVModePose) {
            // I = Insert Keyframe de la POSE 2D: abre el MENU DE CANALES (Todos / Localizacion /
            // Rotacion / Escala) y keyframea posU/posV/rot/escalaX/escalaY de los huesos elegidos
            // en el CLIP del armature 2D (curvas propias por hueso; ya no se hornea el array uv
            // entero en la capa uv de la vertex anim). Shift+I = todos los canales sin menu.
            if (k == W3dK_I) {
                if (shift) InsertKeyframeContext(KfChannelAll, true);
                else       LayoutMenuInsertKeyframe(lastMx, lastMy, true);
            }
            return;
        }
        // EDICION de huesos: E extruye (tail agarrado), Shift+D duplica, X/Supr borra. Ctrl+P /
        // Alt+P (emparentar / menu Disconnect-Clear) entran por el atajo GLOBAL (controls.cpp ->
        // LayoutParentBones); aca queda el fallback por si el evento llega directo al viewport.
        if (k == W3dK_E) {
            int nb = Bone2DExtrude(m);
            if (nb >= 0) Bone2DDragEnd(m, nb, 2, false); // el tail nuevo queda agarrado al mouse
        } else if (k == W3dK_D && shift) {
            if (Bone2DDuplicate(m) >= 0) Bone2DXformStart(m, 1); // lo duplicado queda agarrado
        } else if (k == W3dK_X || k == W3dK_DELETE || k == W3dK_BACKSPACE) {
            Bone2DDelete(m);
        } else if (k == W3dK_P && alt) {
            LayoutMenuBoneAltP2D(m, lastMx, lastMy); // menu Disconnect Bone / Clear Parent
        }
        return;
    }
    if (uvXform) {                          // dentro de un transform: confirmar/cancelar/eje
        // todo via los hooks compartidos (TransformUI): mismo camino que la toolbar y el
        // teclado numerico. El Enter con una expresion invalida NO confirma (feedback "?").
        if (k == W3dK_ESCAPE) { XformCancel(); return; }
        if (k == W3dK_RETURN || k == W3dK_KP_ENTER) { XformConfirm(); return; }
        // BLOQUEO DE EJE (X/Y): restringe al eje EN ESPACIO DE LA VISTA (aca no hay local/global).
        // Tocar de nuevo el mismo eje lo libera; el otro eje cambia el bloqueo (como el 3D).
        if (k == W3dK_X || k == W3dK_Y) XformToggleAxis((k == W3dK_X) ? 1 : 2);
        return;
    }
    // A / Alt+A en EDICION de UVs: seleccionar todo / deseleccionar todo (coherencia con los
    // modos de huesos y con el 3D). En SYNC la seleccion del UV es un espejo de la del 3D ->
    // se toca la del 3D y se re-deriva; fuera de sync se escribe uvSelVert (la propia).
    if (k == W3dK_A && uvMode == UVModeEdit) {
        bool altA = LAltPressed;
        SelectAllUV(m, !altA);
        return;
    }
    // Ctrl+I = INVERTIR y L = SELECT LINKED (la isla UV), los mismos atajos que el menu Select de
    // la barra y que el del viewport 3D. La L arranca de lo que hay BAJO EL CURSOR (como la L del
    // 3D, que agarra la isla bajo el mouse); shift SUMA la isla a la seleccion en vez de reemplazar.
    if (uvMode == UVModeEdit && (k == W3dK_I || k == W3dK_L)) {
        bool ctrlS  = LCtrlPressed;
        bool shiftS = LShiftPressed;
        if (k == W3dK_I && ctrlS) { InvertSelectionUV(m); return; }   // (I sin ctrl = Insert Keyframe, abajo)
        if (k == W3dK_L) { UVSelectLinked(m, VertUnderCursorUV(m), shiftS); return; }
    }
    // (en modo PINTURA DE PESOS StartXform es un no-op: el G/R/S ignora el input ahi)
    if (k == W3dK_G) StartXform(m, 1);    // G = mover  (Symbian 1)
    else if (k == W3dK_R) StartXform(m, 2); // R = rotar  (Symbian 2)
    else if (k == W3dK_S) StartXform(m, 3); // S = escalar (Symbian 3)
    else if (k == W3dK_I && uvMode == UVModeEdit) {
        // I = Insert Keyframe UV en EDICION de UVs (pedido del dueno; en Pose 2D ya existia,
        // bloque de arriba). MISMO camino que el menu Animation de la barra: valida la vertex
        // anim activa (kind 3) y notifica si no hay. Llega aca por el foco de teclado por
        // HOVER (controls.cpp rutea al viewport bajo el mouse; el IDE y las cajas de texto
        // capturan antes). Durante un transform NO llega (el bloque uvXform retorna arriba).
        extern void VertexAnimInsertKeyframeUV();
        VertexAnimInsertKeyframeUV();
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
void UVEditor::mouse_button_up(int button) {
    (void)button;
    if (Gizmo2DRelease(this)) { ViewPortClickDown = false; g_redraw = true; return; }   // (confirma / modo siguiente)
    // BOX SELECT: el soltar CIERRA el rectangulo y aplica la seleccion
    if (uvBoxSel) {
        BoxSelectRelease();
        ViewPortClickDown = false;   // (imprescindible: sino viewPortActive queda congelado)
        return;
    }
    if (gUVPainting == this) { gUVPainting = NULL; WeightPaintStrokeEnd(); } // fin del trazo -> commit del undo
    if (gUVPaintingTex == this) { gUVPaintingTex = NULL; UVTexStrokeEnd(); } // fin del trazo de textura -> commit del undo
    // ARMATURE 2D del mesh: los drags arrancados por CLICK terminan al SOLTAR
    if (Bone2DXformActive() && gB2D.byClick && gB2D.uv == this) {
        Mesh* mb = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        if (mb) Bone2DXformConfirm(mb);
    }
    ViewPortClickDown = false;
    g_redraw = true;
}

// click izquierdo en el editor UV = seleccionar el sub-elemento (vertex/edge/face) bajo el cursor.
void UVEditor::button_left() {
    // (tambien en Symbian: el mouse Bluetooth llega por el mismo camino que el de la PC; el OK/lapiz del keypad
    //  sigue llamando directo a PickUV. Los modificadores, de los globales que mantienen las dos plataformas)
    if (PopUpActive) return;                    // un popup modal abierto (file browser) tiene prioridad
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || !m->uv || (Object*)m != g_editMesh) return; // solo en Edit Mode sobre esta malla
    // ===== BOX SELECT ARMADO (tecla B): este press ABRE el rectangulo; el arrastre lo agranda y
    // el soltar (mouse_button_up) lo aplica. Consume el click: no pickea nada. =====
    if (uvBoxBuilt) {
                uvBoxAdd = LShiftPressed; // shift = sumar
        uvBoxBuilt = false; uvBoxSel = true;
        uvBoxX0 = uvBoxX1 = lastMx - x; uvBoxY0 = uvBoxY1 = lastMy - y;
        g_redraw = true;
        return;
    }
    // ===== MODO OBJETO: el click ELIGE el objeto del UV (armature 2D bajo el cursor o, si no
    // pegaste a ningun hueso, la geometria). No edita nada: para eso esta el Tab. =====
    if (uvMode == UVModeObject) {
        PickObjectUV(m, (float)(lastMx - x), (float)(lastMy - y));
        return;
    }
    // ===== ARMATURE 2D DEL MESH (modos Huesos/Pose): click = confirmar transform en curso,
    // o pickear hueso (+ arrancar el drag de la punta en Huesos / el traslado en Pose) =====
    if (uvMode == UVModeBones || uvMode == UVModePose) {
        if (Bone2DXformActive()) { if (!gB2D.byClick) XformConfirm(); return; } // modal (G/E): click confirma (con gate)
        bool add = LShiftPressed;
        bool pose = (uvMode == UVModePose);
        int mask = 3;
        int b = Bone2DPick(m, (float)(lastMx - x), (float)(lastMy - y), pose, &mask);
        if (!add) Bone2DSelClear(m);
        if (b >= 0) {
            // seleccion por PUNTAS como el Edit del 3D (BoneEditClick3D): click cerca de una punta
            // selecciona SOLO esa punta (con su soldada); en el cuerpo, el hueso ENTERO. Shift
            // sobre algo ya seleccionado lo saca.
            bool already = (mask == 3) ? m->Arm2DBones()[b].select
                    : (mask == 1) ? m->Arm2DBones()[b].selHead : m->Arm2DBones()[b].selTail;
            bool newValue = !(add && already);
            if (mask == 3) Bone2DSelBone(m, b, newValue);
            else           Bone2DSelTip(m, b, mask, newValue);
            m->Arm2DActiveBone() = newValue ? b : -1;
            if (newValue) {
                if (pose) Bone2DXformStart(m, 1);            // POSE: arrastrar = trasladar (G/R/S p/rotar/escalar)
                else      Bone2DDragSelection(m, true);      // HUESOS: drag de LO SELECCIONADO (con soldadura)
                if (Bone2DXformActive()) gB2D.byClick = true; // arranco por click: suelta = confirma
            }
        } else if (!add) m->Arm2DActiveBone() = -1;
        g_redraw = true;
        return;
    }
    // PINTURA DE PESOS: el click arranca el TRAZO (snapshot de undo + grupo automatico si no
    // hay) y pinta la 1ra pasada. El drag sigue en event_mouse_motion; el commit al soltar.
    if (uvMode == UVModeTexture) {   // PINTURA DE TEXTURA: el click arranca el trazo (un toque ya pinta)
        if (UVTexStrokeStart(this)) { gUVPaintingTex = this; UVPaintTexture(this); }
        return;
    }
    if (uvMode == UVModeWeights) {
        // CTRL+CLICK SOBRE UN HUESO 2D = ese hueso pasa a ser el GRUPO ACTIVO que se pinta.
        // (en Weight Paint, Ctrl+click selecciona el hueso) y cierra el
        // flujo de rigging 2D: hasta ahora el UNICO camino para cambiar de grupo era el
        // desplegable "Group" de la toolbar, aunque los huesos SE DIBUJAN mientras se pinta.
        // Sale casi gratis porque el binding hueso <-> UV group es POR NOMBRE.
        // Se pide Ctrl a proposito: con el click pelado, los huesos (radio de pick ~14px) se
        // comerian los trazos justo donde mas se pinta.
        {
            const bool ctrlP = LCtrlPressed;
            if (ctrlP && Bone2DPickGroupByClick(m, (float)(lastMx - x), (float)(lastMy - y)))
                return;   // el ctrl+click elige grupo: NO pinta
        }
        if (WeightPaintStrokeStartUV(m) >= 0) { gUVPainting = this; UVPaintWeights(this, m); }
        return;
    }
    if (uvXform) { XformConfirm(); return; } // si hay transform en curso, el click lo CONFIRMA (con gate)
    // el GIZMO 2D: apretar una manija la agarra (soltar sin arrastrar = el modo siguiente)
    if (Gizmo2DDown(this, lastMx, lastMy)) { ViewPortClickDown = true; return; }
    bool add = LShiftPressed; // shift = sumar/toggle
    PickUV(m, lastMx - x, lastMy - y, add); // lastMx/My son GLOBALES; - origen del viewport = local
    g_redraw = true;
}

// click derecho: cancela el transform en curso; si no hay, COLOCA el cursor 2D en el mouse.
void UVEditor::button_right() {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    // ARMATURE 2D del mesh: el derecho cancela el transform de huesos en curso
    if (Bone2DXformActive() && gB2D.uv == this) { XformCancel(); g_redraw = true; return; }
    if (uvXform) { XformCancel(); return; }
    UVMouseAUV(this, lastMx, lastMy, uvCursorU, uvCursorV); // mismo mapeo (con aspecto) que el render
    g_redraw = true;
}

// --- SNAP (menu Snap del UV editor) ---
void UVEditor::SnapCursorToSel() {         // cursor 2D -> centro de la seleccion
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || !m->uv) return;
    const int nV = m->vertexSize;
    if ((int)m->uvSelVert.size() != nV) return;
    double su = 0, sv = 0; int n = 0;
    for (int i = 0; i < nV; i++) if (m->uvSelVert[i]) { su += m->uv[i*2]; sv += m->uv[i*2+1]; n++; }
    if (n > 0) { uvCursorU = (float)(su/n); uvCursorV = (float)(sv/n); g_redraw = true; }
}

// mueve la seleccion para que su centro caiga en el cursor. Escribe uv[] -> va con SU paso de undo
// (UndoUVStart/Confirmar, igual que el confirmar de un G de UVs y que UVMoveSelectionEdit). Sin
// eso el Ctrl+Z de despues deshacia la operacion ANTERIOR y el snap quedaba pegado.
void UVEditor::SnapSelToCursor() {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    if (!m || !m->uv) return;
    m->UninstantiateData(W3DMD_UV);   // COW (recurso compartido)
    W3dMeshMarkEdited(m);
    std::vector<char> sel;                 // seleccion EFECTIVA (la propia del UV, o la del 3D en sync)
    if (!UVVertsSelEffective(m, sel)) return;
    const int nV = m->vertexSize;
    double su = 0, sv = 0; int n = 0;
    for (int i = 0; i < nV; i++) if (sel[i]) { su += m->uv[i*2]; sv += m->uv[i*2+1]; n++; }
    if (n == 0) return;
    float dU = uvCursorU - (float)(su/n), dV = uvCursorV - (float)(sv/n);
    if (dU == 0.0f && dV == 0.0f) return;  // ya esta en el cursor: ni undo ni redraw
    UndoUVStart(m);                        // snapshot ANTES de escribir (liviano: no hay split de caras)
    for (int i = 0; i < nV; i++) if (sel[i]) { m->uv[i*2] += dU; m->uv[i*2+1] += dV; }
    UndoUVConfirm(true);                 // un paso de undo por snap
    m->Armature2DRestFromUV(); // escribio uv[] a mano -> re-derivar el rest (invariante, ver Mesh.h)
    m->skinGeomVersion++;      // re-subir el VBO de uv
    g_redraw = true;
}

void UVEditor::CursorToCenter() { uvCursorU = 0.5f; uvCursorV = 0.5f; g_redraw = true; }

// =====================================================================================
//  ENCUADRAR (View > Frame Selected / Frame All, Numpad .)
//  El UV era el UNICO viewport sin encuadrar: el 3D, el Editor 2D y el Timeline lo tienen
//  todos con el mismo label y el mismo atajo. Se calcula el bbox en espacio UV y despues
//  zoom+pan para que llene el ~80% del viewport (la misma proporcion que usa el zoom base
//  de ParamsUV y que Editor2DFrameSelection).
// =====================================================================================
bool UVEditor::BBoxUV(Mesh* m, bool all, float& u0, float& v0, float& u1, float& v1) const {
    if (!m) return false;
    u0 = v0 = 1e9f; u1 = v1 = -1e9f;
    bool has = false;
    const bool bones = (uvMode == UVModeBones || uvMode == UVModePose);
    // ---- HUESOS: encuadra el rig (los seleccionados; con 'todo' o sin seleccion, TODOS los
    // huesos del armature ACTIVO). En rest: el bbox de un rig posado se movia con la pose y
    // encuadrar dos veces daba resultados distintos.
    if (bones && m->HasArm2D()) {
        const std::vector<W3dBone2D>& H = m->Arm2DBones();
        for (size_t i = 0; i < H.size(); i++) {
            if (!all && !H[i].select) continue;
            const float pu[2] = { H[i].headU, H[i].tailU };
            const float pv[2] = { H[i].headV, H[i].tailV };
            for (int k = 0; k < 2; k++) {
                if (pu[k] < u0) u0 = pu[k];  if (pu[k] > u1) u1 = pu[k];
                if (pv[k] < v0) v0 = pv[k];  if (pv[k] > v1) v1 = pv[k];
            }
            has = true;
        }
        if (has) return true;
        if (!all) return BBoxUV(m, true, u0, v0, u1, v1); // sin seleccion -> todo el rig
        return false;
    }
    // ---- UVs: SIEMPRE dentro del filtro del 3D (lo que no se ve no se encuadra, misma regla
    // que el pick, el All y el Invert del UV).
    if (!m->uv || m->faces3d.empty()) return false;
    const int nV = m->vertexSize;
    std::vector<char> fsel; UVFacesFilter3D(m, fsel);
    const bool hasSel = ((int)m->uvSelVert.size() == nV);
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const bool visible = (f < fsel.size() && fsel[f]);
        if (!syncSelection && !visible) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c]; if (k < 0 || k >= nV) continue;
            if (!all && !(hasSel && m->uvSelVert[k])) continue;
            float uu = m->uv[k*2], vv = m->uv[k*2+1];
            if (uu < u0) u0 = uu;  if (uu > u1) u1 = uu;
            if (vv < v0) v0 = vv;  if (vv > v1) v1 = vv;
            has = true;
        }
    }
    if (!has && !all) return BBoxUV(m, true, u0, v0, u1, v1); // sin seleccion -> encuadra todo
    return has;
}

bool UVEditor::FrameUV(bool all) {
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    float u0, v0, u1, v1;
    if (!BBoxUV(m, all, u0, v0, u1, v1)) return false;
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
//  BOX SELECT (tecla B + arrastre con el izquierdo)
//  Era LA ausencia mas cara del editor: media isla se seleccionaba vertice por vertice.
//  El rectangulo es en PIXELES de pantalla y se prueba contra la MISMA proyeccion que usa
//  el render/pick (UVtoScreen), asi que lo que se ve encerrado es exactamente lo que entra.
//  Respeta las mismas reglas que el pick: el filtro del 3D (fuera de sync solo se puede tocar
//  lo visible) y el sub-modo Vertex/Edge/Face. En Edit Bones / Pose opera los HUESOS 2D.
// =====================================================================================
static int UVRenderVertAEdit(Mesh* m, int gi); // (definida mas abajo, junto al pick)
void UVEditor::BoxSelectBuild() { uvBoxBuilt = true; uvBoxSel = false; g_redraw = true; }
void UVEditor::BoxSelectCancel() { uvBoxBuilt = false; uvBoxSel = false; g_redraw = true; }

void UVEditor::BoxSelectRelease() {
    if (!uvBoxSel) return;
    uvBoxSel = false;
    int dx = uvBoxX1 - uvBoxX0, dy = uvBoxY1 - uvBoxY0;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx >= 3 && dy >= 3) {   // click sin arrastrar = cancelar (no borra la seleccion)
        Mesh* mb = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        if (mb && (Object*)mb == g_editMesh) BoxSelectApply(mb, uvBoxAdd);
    }
    g_redraw = true;
}

int UVEditor::BoxSelectApply(Mesh* m, bool add) {
    if (!m) return 0;
    float rx0 = (float)(uvBoxX0 < uvBoxX1 ? uvBoxX0 : uvBoxX1);
    float rx1 = (float)(uvBoxX0 < uvBoxX1 ? uvBoxX1 : uvBoxX0);
    float ry0 = (float)(uvBoxY0 < uvBoxY1 ? uvBoxY0 : uvBoxY1);
    float ry1 = (float)(uvBoxY0 < uvBoxY1 ? uvBoxY1 : uvBoxY0);
    float cx, cy, s; ParamsUV(cx, cy, s);
    int touched = 0;
    // ---------- HUESOS 2D (Edit Bones / Pose) ----------
    if ((uvMode == UVModeBones || uvMode == UVModePose) && m->HasArm2D()) {
        std::vector<W3dBone2D>& H = m->Arm2DBones();
        const bool pose = (uvMode == UVModePose);
        std::vector<float> M;
        if (pose) m->Armature2DMatrices(M);
        if (!add) Bone2DSelClear(m);
        for (size_t i = 0; i < H.size(); i++) {
            float hu, hv, tu, tv;
            Bone2DEnds(m, M, i, pose, hu, hv, tu, tv);
            float ax, ay, bx, by;
            UVtoScreen(hu, hv, cx, cy, s, ax, ay);
            UVtoScreen(tu, tv, cx, cy, s, bx, by);
            const bool inA = (ax >= rx0 && ax <= rx1 && ay >= ry0 && ay <= ry1);
            const bool inB = (bx >= rx0 && bx <= rx1 && by >= ry0 && by <= ry1);
            if (!inA && !inB) continue;
            // POSE selecciona el hueso ENTERO (ahi no hay puntas que editar); en EDICION cada
            // punta encerrada se selecciona sola, igual que el click por punta.
            if (pose || (inA && inB)) { Bone2DSelBone(m, (int)i, true); }
            else if (inA)             { Bone2DSelTip(m, (int)i, 1, true); }
            else                      { Bone2DSelTip(m, (int)i, 2, true); }
            m->Arm2DActiveBone() = (int)i;
            touched++;
        }
        g_redraw = true;
        return touched;
    }
    // ---------- UVs (modo edicion) ----------
    if (!m->uv || m->faces3d.empty()) return 0;
    const int nV = m->vertexSize;
    if (nV <= 0) return 0;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign(nV, 0);
    std::vector<char> fsel; UVFacesFilter3D(m, fsel);
    const int curUVMode = UVMode();
    std::vector<char> newValue(nV, 0);
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const bool visible = (f < fsel.size() && fsel[f]);
        if (!syncSelection && !visible) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        const int nc = (int)id.size();
        if (nc < 2) continue;
        if (curUVMode == SelFace) {
            // CARA: entra si su CENTRO cae adentro (mismo criterio que el punto que se dibuja
            // y que el pick por cara) -> se marcan todos sus corners.
            float fu = 0, fv = 0; int fn = 0; bool ok = true;
            for (int c = 0; c < nc; c++) { int k = id[c];
                if (k < 0 || k >= nV) { ok = false; break; }
                fu += m->uv[k*2]; fv += m->uv[k*2+1]; fn++; }
            if (!ok || !fn) continue;
            float px, py; UVtoScreen(fu/fn, fv/fn, cx, cy, s, px, py);
            if (px < rx0 || px > rx1 || py < ry0 || py > ry1) continue;
            for (int c = 0; c < nc; c++) { int k = id[c]; if (k >= 0 && k < nV && !newValue[k]) { newValue[k] = 1; touched++; } }
        } else if (curUVMode == SelEdge) {
            // ARISTA: entra si sus DOS extremos caen adentro (el borde queda blanco entero)
            for (int c = 0; c < nc; c++) {
                int ka = id[c], kb = id[(c+1) % nc];
                if (ka < 0 || ka >= nV || kb < 0 || kb >= nV) continue;
                float ax, ay, bx, by;
                UVtoScreen(m->uv[ka*2], m->uv[ka*2+1], cx, cy, s, ax, ay);
                UVtoScreen(m->uv[kb*2], m->uv[kb*2+1], cx, cy, s, bx, by);
                if (ax < rx0 || ax > rx1 || ay < ry0 || ay > ry1) continue;
                if (bx < rx0 || bx > rx1 || by < ry0 || by > ry1) continue;
                if (!newValue[ka]) { newValue[ka] = 1; touched++; }
                if (!newValue[kb]) { newValue[kb] = 1; touched++; }
            }
        } else { // SelVertex: cada corner por su cuenta (seleccion POR RENDER-VERT, como el pick)
            for (int c = 0; c < nc; c++) {
                int k = id[c]; if (k < 0 || k >= nV || newValue[k]) continue;
                float px, py; UVtoScreen(m->uv[k*2], m->uv[k*2+1], cx, cy, s, px, py);
                if (px < rx0 || px > rx1 || py < ry0 || py > ry1) continue;
                newValue[k] = 1; touched++;
            }
        }
    }
    if (!add) m->uvSelVert.assign(nV, 0);
    for (int i = 0; i < nV; i++) if (newValue[i]) m->uvSelVert[i] = 1;
    // SYNC ON (modo espejo): la seleccion del UV es la del 3D -> hay que escribirla ALLA y
    // re-derivar, sino el proximo frame la pisa SyncFilter3D.
    if (syncSelection && (Object*)m == g_editMesh && m->edit) {
        if (!add) m->edit->SelectAll(false);
        for (int i = 0; i < nV; i++) {
            if (!newValue[i]) continue;
            int ek = UVRenderVertAEdit(m, i);
            if (ek >= 0 && ek < (int)m->edit->vertSel.size()) m->edit->vertSel[ek] = 1;
        }
        m->edit->Recolor();
        SyncSelFrom3D(m);
    }
    g_redraw = true;
    return touched;
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
// (VertsSelByMode) para que seleccionar una cara/vert/borde en el viewport 3D quede resaltada
// y editable en el UV. No-op fuera de sync (ahi uvSelVert es la seleccion PROPIA del UV).
void UVEditor::SyncSelFrom3D(Mesh* m) {
    if (!syncSelection || !m || m->vertexSize <= 0) return;
    if ((Object*)m != g_editMesh) return;   // solo en Edit Mode de esta malla
    std::vector<char> sv; m->VertsSelByMode(sv);
    const int nV = m->vertexSize;
    if ((int)sv.size() != nV) return;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign(nV, 0);
    for (int i = 0; i < nV; i++) m->uvSelVert[i] = sv[i] ? 1 : 0;
}

// FIRMA del filtro: identifica el CONJUNTO de caras visibles con un solo entero (hash FNV de sus
// indices). Nunca da 0 (0 = "sin inicializar"), asi "ninguna cara" tambien tiene firma propia.
static unsigned UVFilterSignature(const std::vector<char>& fsel) {
    unsigned h = 2166136261u;
    for (size_t f = 0; f < fsel.size(); f++)
        if (fsel[f]) { h ^= (unsigned)(f + 1); h *= 16777619u; }
    return h ? h : 1u;
}

// UN punto de entrada por frame para la relacion 3D <-> UV (ver el bloque SYNC SELECTION del
// header). En SYNC espeja y listo. Fuera de sync la seleccion del UV es PROPIA: aca SOLO se la
// inicializa cuando cambia el FILTRO (las caras seleccionadas en 3D), la malla o el toggle de
// sync -> las esquinas que ACABAN de aparecer entran seleccionadas (lo que se ve,
// se puede mover ya). De ahi en mas no se toca nada: manda el click del UV (PickUV), que es por
// RENDER-VERT. Nunca corre con un transform en curso (no pisa lo que se esta moviendo).
void UVEditor::SyncFilter3D(Mesh* m, std::vector<char>* outFilter) {
    if (outFilter) outFilter->clear();
    if (!m || m->vertexSize <= 0) return;
    std::vector<char> fsel; UVFacesFilter3D(m, fsel);  // el filtro, UNA vez por frame
    if (outFilter) *outFilter = fsel;
    if (syncSelection) {                       // modo espejo: la seleccion del mesh manda, por frame
        filterSync = true; filterSerial = m->serial;
        SyncSelFrom3D(m);
        return;
    }
    if ((Object*)m != g_editMesh) return;      // sin Edit Mode no hay filtro que seguir
    if (XformInProgress()) return;             // transform en curso: la seleccion esta en uso
    const int nV = m->vertexSize;
    const unsigned next = UVFilterSignature(fsel);
    const bool sizeBad = ((int)m->uvSelVert.size() != nV); // la topologia cambio por afuera
    if (!sizeBad && nextFilter == next && filterSerial == m->serial && !filterSync) return; // nada cambio
    nextFilter = next; filterSerial = m->serial; filterSync = false;
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
    const bool inEdit = ((Object*)m == g_editMesh);
    std::vector<char> sel3d; UVFacesFilter3D(m, sel3d);

    const int curUVMode = UVMode();                       // vertex/edge/face efectivo
    const float clx = (float)lx, cly = (float)ly;
    const float rad = (float)GlobalScale * 5.0f;       // radio de pick (px)
    int hitVert = -1; float bestVD = rad*rad;
    int hitEA = -1, hitEB = -1; float bestED = rad*rad;
    int hitFace = -1;

    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        const int nc = (int)id.size();
        if (nc < 2) continue;
        const bool selFace = inEdit ? (f < sel3d.size() && sel3d[f]) : true;
        if (!syncSelection && !selFace) continue;
        if (curUVMode == SelVertex) {
            for (int c = 0; c < nc; c++) { int ka = id[c]; if (ka < 0 || ka >= nV) continue;
                float sx,sy; UVtoScreen(m->uv[ka*2], m->uv[ka*2+1], cx,cy,s, sx,sy);
                float d = (sx-clx)*(sx-clx) + (sy-cly)*(sy-cly);
                if (d < bestVD) { bestVD = d; hitVert = ka; } }
        } else if (curUVMode == SelEdge) {
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
    // posRep) y VertsSelByMode devuelve expandido -> en una COSTURA quedan seleccionadas TODAS
    // las copias UV de ese vertice 3D. Es lo ESPERADO de este modo (por eso el default es OFF).
    if (syncSelection && inEdit && m->edit) {
        const bool onlyThis = !add;   // sin shift = reemplaza
        if (curUVMode == SelVertex) {
            int ek = UVRenderVertAEdit(m, hitVert);
            if (ek >= 0) m->edit->ToggleVert(ek, onlyThis);
            else if (onlyThis) m->edit->SelectAll(false); // click al vacio: deselecciona
        } else if (curUVMode == SelEdge) {
            int ea = UVRenderVertAEdit(m, hitEA), eb = UVRenderVertAEdit(m, hitEB), eEdge = -1;
            if (ea >= 0 && eb >= 0)
                for (size_t e2 = 0; e2*2+1 < m->edit->lineIdx.size(); e2++) {
                    int la = m->edit->lineIdx[e2*2], lb = m->edit->lineIdx[e2*2+1];
                    if ((la==ea && lb==eb) || (la==eb && lb==ea)) { eEdge = (int)e2; break; }
                }
            if (eEdge >= 0) m->edit->ToggleEdge(eEdge, onlyThis);
            else if (onlyThis) m->edit->SelectAll(false);
        } else { // SelFace
            int efe = -1;
            if (hitFace >= 0)
                for (size_t fe = 0; fe < m->edit->faceSrc.size(); fe++)
                    if (m->edit->faceSrc[fe] == hitFace) { efe = (int)fe; break; }
            if (efe >= 0) m->edit->ToggleFace(efe, onlyThis);
            else if (onlyThis) m->edit->SelectAll(false);
        }
        m->edit->Recolor();    // el viewport 3D refleja el cambio
        SyncSelFrom3D(m);      // y el UV re-espeja la seleccion
        g_redraw = true;
        return;
    }

    // SYNC OFF (DEFAULT): seleccion PROPIA del UV y POR RENDER-VERT. Se escribe EXACTAMENTE el
    // vert (o los 2 del borde, o los corners de la cara) que se clickeo: las otras copias UV del
    // mismo vertice 3D (costuras) NO se tocan, y la seleccion del viewport 3D tampoco.
    if (!add) m->uvSelVert.assign(nV, 0); // sin shift: reemplaza
    if (curUVMode == SelVertex && hitVert >= 0) {
        m->uvSelVert[hitVert] = add ? (m->uvSelVert[hitVert] ? 0 : 1) : 1;
    } else if (curUVMode == SelEdge && hitEA >= 0) {
        unsigned char nv = add ? ((m->uvSelVert[hitEA] && m->uvSelVert[hitEB]) ? 0 : 1) : 1;
        m->uvSelVert[hitEA] = nv; m->uvSelVert[hitEB] = nv;
    } else if (curUVMode == SelFace && hitFace >= 0) {
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
// UVEditor.h (Tab alterna Edit Bones con el modo del que venis). Requisitos para consumir
// la tecla: UV operativo (malla activa en Edit Mode) y la malla YA con armature 2D; si no, false y
// el Tab GLOBAL hace lo de siempre. Un transform a medio hacer se CANCELA (como el Tab del 3D, que
// no deja un grab colgado). No toca InteractionMode: el modo del UV es propio del viewport.
bool UVEditor::TabToggleBones() {
    if (!InEditUV()) return false;                       // el UV no esta operativo: Tab global
    Mesh* m = (Mesh*)ActiveObj;                          // InEditUV ya garantizo mesh + g_editMesh
    // sin rig 2D el Tab solo es del UV si hay que SALIR del modo objeto (ver el ciclo en UVEditor.h)
    if (!m->HasArm2D() && uvMode != UVModeObject) return false;
    if (Bone2DXformActive()) Bone2DXformCancel(m);          // G/R/S de huesos/pose colgado -> cancelar
    if (uvXform) CancelXform(m);                            // idem el G/R/S de UVs
    if (uvMode == UVModeObject) {
        // ENTRAR a editar el OBJETO ACTIVO del UV: el armature 2D activo (Edit Bones) o la
        // geometria (edicion de UVs). Sin armatures siempre es la geometria.
        const bool toArmature = uvObjArm && m->HasArm2D();
        uvPreviousMode = UVModeObject;                        // el Tab de vuelta devuelve a Objeto
        uvMode = toArmature ? UVModeBones : UVModeEdit;
        if (toArmature) PropsGoToArmature2D();               // el panel se para en la pestania del rig 2D
    } else if (uvMode == UVModeBones) {                      // SALIR: vuelve al modo del que se entro
        uvMode = (uvPreviousMode == UVModeBones) ? UVModeObject : uvPreviousMode;
    } else if (uvMode == UVModeEdit) {                   // edicion de UVs -> Objeto
    //         uvMode = UVModeObject;
        uvObjArm = false;                                   // se venia editando la GEOMETRIA
    } else {                                                // ENTRAR a Edit Bones (desde Pesos / Pose)
        uvPreviousMode = uvMode;
        uvMode = UVModeBones;
        uvObjArm = true;
        PropsGoToArmature2D();                               // el panel se para en la pestania del rig 2D
    }
    g_redraw = true;
    return true;
}

// hueso mas cercano al punto (px LOCALES) mirando TODOS los armatures 2D (no solo el activo): es
// el pick del MODO OBJETO. Devuelve el indice del hueso dentro de su armature (outArm), o -1.
int UVEditor::Bone2DPickAll(Mesh* m, float lx, float ly, int* outArm) const {
    if (outArm) *outArm = -1;
    if (!m || !m->HasArm2D()) return -1;
    float cx, cy, s; ParamsUV(cx, cy, s);
    const bool pose = (uvMode == UVModePose);
    int bestB = -1, bestA = -1;
    float bestD = 14.0f * 14.0f;      // mismo radio que el pick de cuerpo de hueso / de vertices
    for (size_t a = 0; a < m->armatures2d.size(); a++) {
        const Armature2D* arm = m->armatures2d[a];
        if (!arm || arm->bones.empty()) continue;
        std::vector<float> M;
        if (pose) m->Armature2DMatricesOf(arm, M);
        for (size_t i = 0; i < arm->bones.size(); i++) {
            float hu, hv, tu, tv;
            Bone2DEndsOf(arm->bones, M, i, pose, hu, hv, tu, tv);
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
// group homonimo el grupo que se pinta. (en Weight Paint, Ctrl+click
// selecciona el hueso) y cierra el flujo de rigging 2D: sin el, el UNICO camino para cambiar de
// grupo era el desplegable "Group" de la toolbar, aunque los huesos SE DIBUJAN mientras se pinta.
// Sale casi gratis porque el binding hueso <-> UV group es POR NOMBRE. Se pide Ctrl a proposito:
// con el click pelado, los huesos (radio de pick ~14px) se comerian los trazos justo donde mas se
// pinta. Devuelve true si engancho un hueso (el click NO tiene que pintar).
// Vive aparte de button_left (que es codigo SDL) para poder testear el camino sin simular teclas.
bool UVEditor::Bone2DPickGroupByClick(Mesh* m, float lx, float ly) {
    if (!m || !m->HasArm2D()) return false;
    int armIdx = -1;
    int bh = Bone2DPickAll(m, lx, ly, &armIdx);
    if (bh < 0 || armIdx < 0 || armIdx >= (int)m->armatures2d.size()) return false;
    Arm2DSetActive(m, armIdx);   // cambiar de rig recarga el rango del timeline (kind 4)
    Armature2D* arm = m->armatures2d[armIdx];
    if (arm && bh < (int)arm->bones.size()) {
        arm->activeBone = bh;
        const std::string& name = arm->bones[bh].name;
        int g = -1;
        for (size_t gi = 0; gi < m->uvGroups.size(); gi++)
            if (m->uvGroups[gi] && m->uvGroups[gi]->name == name) { g = (int)gi; break; }
        if (g >= 0) { m->uvActiveGroup = g; Notify(name, false); }
        else        Notify(T("No UV Group for this bone"), false);
    }
    g_redraw = true;
    return true;
}

// MODO OBJETO: el click elige QUE se edita. Si cae sobre un hueso de cualquier armature 2D, ESE
// armature pasa a ser el activo de la malla y el objeto activo del UV; si no, queda la GEOMETRIA.
int UVEditor::PickObjectUV(Mesh* m, float lx, float ly) {
    if (!m) return -1;
    int arm = -1;
    int b = Bone2DPickAll(m, lx, ly, &arm);
    if (b >= 0 && arm >= 0) {
        Arm2DSetActive(m, arm); // el rig elegido manda: si el timeline esta en kind 4, recarga su rango
        // el hueso clickeado queda como ACTIVO del armature (asi el panel muestra ese) y unico
        // seleccionado: al entrar con Tab a Edit Bones se sigue trabajando sobre lo que se eligio.
        Bone2DSelClear(m);
        Bone2DSelBone(m, b, true);
        m->Arm2DActiveBone() = b;
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
void UVEditor::Armature2DCreate(Mesh* m) {
    if (!m) return;
    if (m->Arm2DBones().empty()) {
        // el CONTENEDOR (armature 0) va ANTES del snapshot: el undo de huesos guarda los huesos de
        // UN armature concreto, asi que tiene que existir cuando se captura (sino el undo no aplica).
        // Si el contenedor lo creamos NOSOTROS va con su propio paso de undo (queda ABAJO del paso
        // del hueso): Ctrl+Z saca el hueso, otro Ctrl+Z saca el armature -> no queda un rig vacio.
        const bool createdArm = m->armatures2d.empty();
        m->Arm2DEnsure();
        if (createdArm) UndoArm2DAdded(m, m->armature2dActive);
        UndoBones2DCapture(m); // snapshot ANTES de mutar (patron BonesUndo: op discreta = push directo)
        W3dBone2D b;
        b.name = Bone2DUniqueName(m, "Bone");
        b.headU = 0.5f; b.headV = 0.5f;   // el tail "para arriba" en pantalla (V crece para abajo)
        b.tailU = 0.5f; b.tailV = 0.3f;
        b.select = true; b.selHead = true; b.selTail = true; // hueso entero seleccionado
        m->Arm2DBones().push_back(b);
        m->Arm2DActiveBone() = (int)m->Arm2DBones().size() - 1;
        Bone2DEnsureGroup(m, b.name);
    }
    if (uvMode != UVModeBones) uvPreviousMode = uvMode;  // el Tab vuelve al modo del que se venia
    uvMode = UVModeBones;
    uvObjArm = true;         // se pasa a editar el rig: el objeto activo del UV es el armature
    PropsGoToArmature2D();   // recien creado: el panel se para en la pestania del rig 2D
    g_redraw = true;
}

// ARMATURE 2D NUEVO (menu Add > Armature 2D con la malla ya rigueada, y el boton Add de la lista
// del panel): crea OTRO armature independiente (sus huesos, sus clips) con 1 hueso, lo deja ACTIVO
// y entra a editarlo. Devuelve el indice del armature nuevo.
// UNDO en DOS pasos (a proposito, cada uno es una op discreta): el primer Ctrl+Z saca el hueso y
// deja el armature vacio; el segundo saca el armature de la lista (UndoArm2DAdded).
int UVEditor::Armature2DNew(Mesh* m) {
    if (!m) return -1;
    int idx = m->Arm2DAdd("Armature 2D");
    UndoArm2DAdded(m, idx); // el ALTA es el paso de undo: Ctrl+Z saca el armature entero
    UndoBones2DCapture(m); // snapshot del armature NUEVO (ya activo): el undo del hueso, no del alta
    W3dBone2D b;
    b.name = Bone2DUniqueName(m, "Bone");
    b.headU = 0.5f; b.headV = 0.5f;
    b.tailU = 0.5f; b.tailV = 0.3f;
    b.select = true; b.selHead = true; b.selTail = true;
    m->Arm2DBones().push_back(b);
    m->Arm2DActiveBone() = 0;
    Bone2DEnsureGroup(m, b.name);
    if (uvMode != UVModeBones) uvPreviousMode = uvMode;
    uvMode = UVModeBones;
    uvObjArm = true;
    PropsGoToArmature2D();
    g_redraw = true;
    return idx;
}

// hueso RAIZ nuevo (menu Add > Bone): en el centro, seleccionado y activo, con su vertex group.
int UVEditor::Bone2DAdd(Mesh* m) {
    if (!m) return -1;
    const bool createdArm = m->armatures2d.empty();
    m->Arm2DEnsure();     // sin armature 2D no hay donde meter el hueso: se crea el contenedor
    if (createdArm) UndoArm2DAdded(m, m->armature2dActive); // el contenedor nuevo, con su propio paso
    UndoBones2DCapture(m); // snapshot ANTES de mutar (op discreta)
    W3dBone2D b;
    b.name = Bone2DUniqueName(m, "Bone");
    b.headU = 0.5f; b.headV = 0.5f; b.tailU = 0.5f; b.tailV = 0.3f;
    b.select = true; b.selHead = true; b.selTail = true;
    Bone2DSelClear(m);
    m->Arm2DBones().push_back(b);
    m->Arm2DActiveBone() = (int)m->Arm2DBones().size() - 1;
    Bone2DEnsureGroup(m, b.name);
    g_redraw = true;
    return m->Arm2DActiveBone();
}

// E: hueso nuevo desde el TAIL del activo (hijo CONECTADO: head = tail del padre). El tail nuevo
// arranca pegado al head (el caller lo deja agarrado al mouse con Bone2DDragEnd). -1 si no hay activo.
int UVEditor::Bone2DExtrude(Mesh* m) {
    if (!m) return -1;
    int act = m->Arm2DActiveBone();
    if (act < 0 || act >= (int)m->Arm2DBones().size()) return -1;
    UndoBones2DCapture(m); // snapshot ANTES de mutar (op discreta; el drag posterior es OTRO paso)
    W3dBone2D b;
    b.name = Bone2DUniqueName(m, "Bone");
    b.parent = act;                                // hijo del activo (padre < hijo: se apendea)
    b.connected = true;                            // extrude = CONECTADO (head soldado al tail del padre)
    b.headU = m->Arm2DBones()[act].tailU; b.headV = m->Arm2DBones()[act].tailV;
    b.tailU = b.headU; b.tailV = b.headV;
    b.selTail = true;                              // el tip nuevo queda seleccionado
    Bone2DSelClear(m);
    m->Arm2DBones().push_back(b);
    m->Arm2DActiveBone() = (int)m->Arm2DBones().size() - 1;
    Bone2DEnsureGroup(m, b.name);
    g_redraw = true;
    return m->Arm2DActiveBone();
}

// Shift+D: duplica los huesos seleccionados como RAICES sueltas (sin conectar al original), con
// nombre/vertex group nuevos y un offset chico para verlos. Devuelve el indice del activo nuevo.
int UVEditor::Bone2DDuplicate(Mesh* m) {
    if (!m) return -1;
    std::vector<int> sel;
    for (size_t i = 0; i < m->Arm2DBones().size(); i++) if (m->Arm2DBones()[i].select) sel.push_back((int)i);
    if (sel.empty() && m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < (int)m->Arm2DBones().size())
        sel.push_back(m->Arm2DActiveBone());
    if (sel.empty()) return -1;
    UndoBones2DCapture(m); // snapshot ANTES de mutar (op discreta)
    Bone2DSelClear(m);
    int first = -1;
    for (size_t k = 0; k < sel.size(); k++) {
        W3dBone2D b = m->Arm2DBones()[sel[k]];       // copia (pose incluida)
        b.name = Bone2DUniqueName(m, m->Arm2DBones()[sel[k]].name);
        b.parent = -1;                              // suelto (como el duplicate del 3D)
        b.headU += 0.02f; b.headV += 0.02f; b.tailU += 0.02f; b.tailV += 0.02f;
        b.select = true; b.selHead = true; b.selTail = true;
        m->Arm2DBones().push_back(b);
        if (first < 0) first = (int)m->Arm2DBones().size() - 1;
        Bone2DEnsureGroup(m, b.name);
    }
    m->Arm2DActiveBone() = first;
    Bone2DSanitizeConnected(m);   // la copia quedo RAIZ (padre = -1) y arrastraba el flag del original
    g_redraw = true;
    return first;
}

// Bone2DTrack::bone es un INDICE a los huesos del armature, asi que TODA compactacion o reorden de los
// huesos 2D tiene que remapear tambien los tracks de los clips de ESE armature (Armature2D::anims,
// que aca se leen por el accesor del ACTIVO Arm2DAnims()). Sin esto, borrar
// el hueso 0 dejaba sus curvas manejando al hueso 1 y las del 1 huerfanas (y se seguian guardando
// en el .w3d). Es el calco de lo que hace BoneEditDelete con los clips del armature 3D.
//   newIdx[viejo] = indice nuevo, o -1 si ese hueso se borro (su track se va con el).
static void Bone2DRemapTracks(Mesh* m, const std::vector<int>& newIdx) {
    if (!m) return;
    const int n = (int)newIdx.size();
    for (size_t a = 0; a < m->Arm2DAnims().size(); a++) {
        Armature2DAnimation* clip = m->Arm2DAnims()[a];
        if (!clip) continue;
        for (size_t t = 0; t < clip->tracks.size(); ) {
            int b = clip->tracks[t].bone;
            int nb = (b >= 0 && b < n) ? newIdx[b] : -1;
            if (b >= 0 && nb < 0) clip->tracks.erase(clip->tracks.begin() + t); // hueso borrado (o track colgado)
            else { clip->tracks[t].bone = nb; ++t; }
        }
    }
    m->last2dFrame = -999999; m->last2dAnim = -999; m->pose2dDirty = true; // forzar re-evaluacion
}

// X/Supr: borra los seleccionados. Los hijos se re-parentan al ABUELO (o quedan raiz). Los vertex
// groups del mismo nombre QUEDAN (los pesos pintados no se pierden; se pueden re-bindear creando
// un hueso con ese nombre). Se re-mapean los indices padre Y los tracks de los clips 2D.
bool UVEditor::Bone2DDelete(Mesh* m) {
    if (!m || m->Arm2DBones().empty()) return false;
    std::vector<char> deleteValue(m->Arm2DBones().size(), 0);
    bool has = false;
    for (size_t i = 0; i < m->Arm2DBones().size(); i++)
        if (m->Arm2DBones()[i].select) { deleteValue[i] = 1; has = true; }
    if (!has && m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < (int)m->Arm2DBones().size()) {
        deleteValue[m->Arm2DActiveBone()] = 1; has = true;
    }
    if (!has) return false;
    UndoBones2DCapture(m); // snapshot ANTES de mutar (incluye la capa uv: el borrado re-aplica el skinning)
    // 1) re-parentar: el padre EFECTIVO de cada hueso = el primer ancestro NO borrado
    const int n = (int)m->Arm2DBones().size();
    for (int i = 0; i < n; i++) {
        if (deleteValue[i]) continue;
        int p = m->Arm2DBones()[i].parent;
        while (p >= 0 && p < n && deleteValue[p]) p = m->Arm2DBones()[p].parent;
        m->Arm2DBones()[i].parent = p;
    }
    // 2) compactar + re-mapear indices
    std::vector<int> newIdx(n, -1);
    std::vector<W3dBone2D> newValue;
    for (int i = 0; i < n; i++) if (!deleteValue[i]) { newIdx[i] = (int)newValue.size(); newValue.push_back(m->Arm2DBones()[i]); }
    for (size_t i = 0; i < newValue.size(); i++) {
        int p = newValue[i].parent;
        newValue[i].parent = (p >= 0 && p < n) ? newIdx[p] : -1;
    }
    Bone2DRemapTracks(m, newIdx); // las curvas siguen a SU hueso (y las del borrado se van)
    m->Arm2DBones().swap(newValue);
    m->Arm2DActiveBone() = -1;
    // GEMELO 2D del aviso de BoneEditDelete: la seleccion del dope guarda el indice del hueso
    // dentro de su clave ("arm2d:#<serial malla>/a<ARM>/k<CLIP>/b<IDX>"). De arriba hacia abajo (ver
    // BoneEdit.cpp) y POR CADA CLIP del rig 2D: el hueso es del ARMATURE, no de un clip.
    {
        const size_t nClips = m->Arm2DAnims().size();
        for (size_t c = 0; c < nClips; c++){
            char sa[48]; snprintf(sa, sizeof sa, "/a%d/k%d/b", m->armature2dActive, (int)c);
            const std::string pref = "arm2d:" + DopeIdOwner(m) + sa;  // POR SERIAL: el nombre se recicla
            for (int i = n - 1; i >= 0; i--) if (deleteValue[i]) DopeRemapIndexKey(pref, i, -1);
        }
    }
    // el hijo que subio al ABUELO conserva su posicion: su head ya no coincide con el tail del padre nuevo,
    // asi que el flag 'conectado' heredado quedaria mintiendo. El saneador lo apaga (y lo deja prendido en
    // el caso raro en que las puntas SI coincidan). No mueve nada.
    Bone2DSanitizeConnected(m);
    // la pose de lo que queda sigue valida: re-aplicar el skinning (si habia deformacion)
    m->Armature2DApply();
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
    for (size_t i = 0; i < m->Arm2DBones().size() && !va; i++)
        if (m->Arm2DBones()[i].select && m->Arm2DBones()[i].parent >= 0) va = true;
    if (!va && m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < (int)m->Arm2DBones().size() &&
        m->Arm2DBones()[m->Arm2DActiveBone()].parent >= 0) va = true;
    if (!va) return false;
    UndoBones2DCapture(m); // snapshot ANTES de mutar (op discreta)
    bool any = false;
    for (size_t i = 0; i < m->Arm2DBones().size(); i++)
        if (m->Arm2DBones()[i].select && m->Arm2DBones()[i].parent >= 0) { m->Arm2DBones()[i].parent = -1; any = true; }
    if (!any && m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < (int)m->Arm2DBones().size() &&
        m->Arm2DBones()[m->Arm2DActiveBone()].parent >= 0) { m->Arm2DBones()[m->Arm2DActiveBone()].parent = -1; any = true; }
    if (any) { Bone2DSanitizeConnected(m); g_redraw = true; } // sin padre no hay tail al que estar soldado
    return any;
}
bool UVEditor::Bone2DDisconnect(Mesh* m) { return Bone2DClearParentSel(m); }

// Alt+P > Disconnect Bone: los seleccionados (o el activo) SIGUEN emparentados pero pierden la
// soldadura (conectado = false): la punta compartida deja de ser una sola y al separarla queda
// la linea punteada con el padre. No mueve nada.
bool Bone2DDisconnectSel(Mesh* m) {
    if (!m) return false;
    bool va = false;
    for (size_t i = 0; i < m->Arm2DBones().size() && !va; i++)
        if ((m->Arm2DBones()[i].select || (int)i == m->Arm2DActiveBone()) &&
            m->Arm2DBones()[i].parent >= 0 && m->Arm2DBones()[i].connected) va = true;
    if (!va) return false;
    UndoBones2DCapture(m); // snapshot ANTES de mutar (op discreta)
    for (size_t i = 0; i < m->Arm2DBones().size(); i++)
        if ((m->Arm2DBones()[i].select || (int)i == m->Arm2DActiveBone()) && m->Arm2DBones()[i].parent >= 0)
            m->Arm2DBones()[i].connected = false;
    g_redraw = true;
    return true;
}

// 'b' es el mismo 'ancestro' o desciende de el? (para excluir candidatos que armarian un ciclo)
bool Bone2DIsDescendant(Mesh* m, int ancestor, int b) {
    if (!m || b < 0 || b >= (int)m->Arm2DBones().size()) return false;
    if (b == ancestor) return true;
    int p = m->Arm2DBones()[b].parent, guard = 0;
    while (p >= 0 && p < (int)m->Arm2DBones().size() && guard++ < (int)m->Arm2DBones().size()) {
        if (p == ancestor) return true;
        p = m->Arm2DBones()[p].parent;
    }
    return false;
}

// restaura el invariante padre < hijo (una sola pasada de FK en Armature2DMatrices) con un
// REORDEN topologico ESTABLE: los padres van antes que sus hijos conservando el orden relativo.
// Remapea los indices 'padre', bone2dActivo Y los tracks de los clips 2D (Bone2DTrack::bone es
// un indice a los huesos del armature). Solo hace falta tras RE-PARENTAR (extrude/borrar ya lo conservan);
// sin ciclos garantizado (el caller los rechaza antes).
static void Bone2DReorderTopo(Mesh* m) {
    const int n = (int)m->Arm2DBones().size();
    if (n <= 1) return;
    bool broken = false;
    for (int i = 0; i < n && !broken; i++)
        if (m->Arm2DBones()[i].parent >= i) broken = (m->Arm2DBones()[i].parent != -1);
    if (!broken) return; // el invariante ya vale: no tocar nada (indices estables)
    std::vector<int> order;         // indices VIEJOS en el orden nuevo
    std::vector<int> newIdx(n, -1); // viejo -> nuevo
    bool prog = true;
    while ((int)order.size() < n && prog) {
        prog = false;
        for (int i = 0; i < n; i++) {
            if (newIdx[i] >= 0) continue;
            int p = m->Arm2DBones()[i].parent;
            if (p < 0 || p >= n || newIdx[p] >= 0) {
                newIdx[i] = (int)order.size(); order.push_back(i); prog = true;
            }
        }
    }
    for (int i = 0; i < n; i++) // ciclo residual (no deberia): se apendea tal cual
        if (newIdx[i] < 0) { newIdx[i] = (int)order.size(); order.push_back(i); }
    std::vector<W3dBone2D> newValue;
    newValue.reserve(n);
    for (int k = 0; k < n; k++) {
        W3dBone2D b = m->Arm2DBones()[order[k]];
        b.parent = (b.parent >= 0 && b.parent < n) ? newIdx[b.parent] : -1;
        newValue.push_back(b);
    }
    Bone2DRemapTracks(m, newIdx); // las curvas viajan con su hueso al nuevo indice
    m->Arm2DBones().swap(newValue);
    if (m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < n) m->Arm2DActiveBone() = newIdx[m->Arm2DActiveBone()];
}

// re-parenta 'bone' a 'newParent' (-1 = raiz), keep offset (no mueve nada; queda SIN soldar ->
// linea punteada). Rechaza ciclos. Lo usa el desplegable "Parent" de la tarjeta Armature 2D.
bool Bone2DReparent(Mesh* m, int bone, int newParent) {
    if (!m || bone < 0 || bone >= (int)m->Arm2DBones().size()) return false;
    if (newParent >= (int)m->Arm2DBones().size()) return false;
    if (newParent < 0) newParent = -1;
    if (newParent == m->Arm2DBones()[bone].parent) return false;
    if (newParent >= 0 && Bone2DIsDescendant(m, bone, newParent)) return false; // ciclo
    UndoBones2DCapture(m);
    m->Arm2DBones()[bone].parent = newParent;
    if (newParent >= 0) m->Arm2DBones()[bone].connected = false; // keep offset (sin soldar)
    Bone2DSanitizeConnected(m);   // y si quedo raiz (-1) el flag tampoco puede sobrevivir
    Bone2DReorderTopo(m); // el nuevo padre puede tener indice mayor -> reordena
    g_redraw = true;
    return true;
}

// SOLDAR (sin undo): mueve el hueso ENTERO para que su head caiga EXACTO en el tail del padre y
// prende el flag. Los descendientes CONECTADOS se mudan con el mismo delta (sino se romperia la
// soldadura de la cadena); los sueltos conservan su offset, como el keep-offset del resto.
static void Bone2DWeldRaw(Mesh* m, int bone) {
    W3dBone2D& b = m->Arm2DBones()[bone];
    if (b.parent < 0 || b.parent >= (int)m->Arm2DBones().size()) return;
    const float du = m->Arm2DBones()[b.parent].tailU - b.headU;
    const float dv = m->Arm2DBones()[b.parent].tailV - b.headV;
    b.connected = true;
    if (du == 0.0f && dv == 0.0f) return;
    const int n = (int)m->Arm2DBones().size();
    std::vector<char> move(n, 0);
    move[bone] = 1;
    // cadena soldada hacia abajo (padre < hijo NO esta garantizado tras re-parentar -> iterar)
    bool change = true; int guard = 0;
    while (change && guard++ <= n) {
        change = false;
        for (int c = 0; c < n; c++) {
            if (move[c]) continue;
            int p = m->Arm2DBones()[c].parent;
            if (p >= 0 && p < n && move[p] && m->Arm2DBones()[c].connected) { move[c] = 1; change = true; }
        }
    }
    for (int i = 0; i < n; i++) if (move[i]) {
        W3dBone2D& q = m->Arm2DBones()[i];
        q.headU += du; q.headV += dv; q.tailU += du; q.tailV += dv;
    }
}

// ===== UNICO camino de CONECTAR / DESCONECTAR un hueso 2D de su padre (el flag 'conectado').
// Lo comparten el checkbox "Connected" de la tarjeta Armature 2D, el Ctrl+P > Connected y el
// Alt+P > Disconnect Bone. Conectar SUELDA (mueve el hueso para pegar su head al tail del padre);
// desconectar solo apaga el flag (no mueve nada). Un paso de undo. false = no cambio nada. =====
bool Bone2DSetConnected(Mesh* m, int bone, bool with) {
    if (!m || bone < 0 || bone >= (int)m->Arm2DBones().size()) return false;
    if (m->Arm2DBones()[bone].parent < 0) return false;         // sin padre el flag no tiene sentido
    if (Bone2DIsConnected(m, bone) == with) return false;     // ya esta en ese estado EFECTIVO
    UndoBones2DCapture(m);
    if (with) Bone2DWeldRaw(m, bone);
    else     m->Arm2DBones()[bone].connected = false;
    g_redraw = true;
    return true;
}

// Ctrl+P: los huesos SELECCIONADOS se emparentan al ACTIVO (sin ciclos; UN paso de undo para toda
// la operacion). conectar=false -> "Keep Offset" (el hueso no se mueve, queda la linea punteada);
// conectar=true -> "Connected" (se MUEVE para que su head quede pegado al tail del activo y queda
// soldado). false = no habia nada que emparentar.
bool Bone2DParentSelectionToActive(Mesh* m, bool connect) {
    if (!m) return false;
    int act = m->Arm2DActiveBone();
    if (act < 0 || act >= (int)m->Arm2DBones().size()) return false;
    std::vector<int> aParent;
    for (size_t i = 0; i < m->Arm2DBones().size(); i++) {
        if ((int)i == act || !m->Arm2DBones()[i].select) continue;
        if (Bone2DIsDescendant(m, (int)i, act)) continue;  // el activo desciende de i -> ciclo
        if (!connect && m->Arm2DBones()[i].parent == act && !m->Arm2DBones()[i].connected) continue; // ya esta asi
        if (connect && m->Arm2DBones()[i].parent == act && Bone2DIsConnected(m, (int)i)) continue; // ya esta asi
        aParent.push_back((int)i);
    }
    if (aParent.empty()) return false;
    UndoBones2DCapture(m);
    for (size_t k = 0; k < aParent.size(); k++) {
        m->Arm2DBones()[aParent[k]].parent = act;
        m->Arm2DBones()[aParent[k]].connected = false;       // keep offset (sin soldar)
        if (connect) Bone2DWeldRaw(m, aParent[k]);      // Connected: pega el head al tail del padre
    }
    Bone2DSanitizeConnected(m);   // red de seguridad del invariante (los descendientes tambien se movieron)
    Bone2DReorderTopo(m);
    g_redraw = true;
    return true;
}

// renombra el hueso 2D (nombre unico en el armature) Y el UV GROUP homonimo del MISMO mesh
// (binding por nombre, como BoneRename del 3D con su vertex group). Los vertex groups NO se
// tocan: el hueso 2D no los usa. Undo en 2 pasos (huesos + rename del grupo).
bool Bone2DRename(Mesh* m, int idx, const std::string& newValue) {
    if (!m || idx < 0 || idx >= (int)m->Arm2DBones().size() || newValue.empty()) return false;
    std::string old = m->Arm2DBones()[idx].name;
    if (old == newValue) return false;
    // el propio hueso conserva su nombre (renombrarlo al mismo valor no debe dar .001).
    // Y el nombre tiene que quedar libre en LAS DOS PUNTAS del binding: uniquificar solo entre
    // huesos 2D dejaba DOS UV groups homonimos cuando el nombre pedido ya era de otro UV group.
    // Los UV groups solo cuentan si el rename los va a ARRASTRAR (hay uno homonimo): sin arrastre,
    // ponerle a un hueso el nombre de un UV group existente es la forma de LIGARLOS a mano.
    bool drags = false;
    for (size_t g = 0; g < m->uvGroups.size() && !drags; g++)
        if (m->uvGroups[g] && m->uvGroups[g]->name == old) drags = true;
    std::vector<std::string> taken;
    for (size_t a = 0; a < m->armatures2d.size(); a++) {
        Armature2D* arm = m->armatures2d[a]; if (!arm) continue;
        for (size_t i = 0; i < arm->bones.size(); i++) {
            if ((int)a == m->armature2dActive && (int)i == idx) continue;   // el propio hueso
            taken.push_back(arm->bones[i].name);
        }
    }
    if (drags)
        for (size_t g = 0; g < m->uvGroups.size(); g++)
            if (m->uvGroups[g] && m->uvGroups[g]->name != old) taken.push_back(m->uvGroups[g]->name);
    std::string unique = W3dUniqueNameInValues(newValue, "Bone", taken, -1);
    // UN SOLO paso de undo para las dos puntas del binding (hueso + UV groups homonimos). En dos
    // pasos separados, el primer Ctrl+Z devolvia el nombre del hueso y dejaba los grupos con el
    // nuevo -> binding roto (el rig dejaba de deformar) hasta el segundo Ctrl+Z.
    // (malla, lista UVGroup, INDICE), NO std::string*: un puntero al nombre del grupo daba un
    // destino 'Directo', que W3dPointerNameLive no reconoce nunca -> el RenameUndo quedaba
    // MUERTO y el Ctrl+Z devolvia el nombre del HUESO dejando el del GRUPO con el nuevo (las dos
    // puntas con nombres distintos = binding roto, y sin arreglo posible: el viejo se perdia).
    std::vector<W3dRenameDest> aRename;
    for (size_t g = 0; g < m->uvGroups.size(); g++)
        if (m->uvGroups[g] && m->uvGroups[g]->name == old)
            aRename.push_back(W3dDestLayerMesh(m, W3dRenameDest::UVGroup, (int)g));
    UndoBone2DRenameCapture(m, aRename); // snapshot de huesos (trae el nombre viejo) + esos grupos
    m->Arm2DBones()[idx].name = unique;
    const int nvg = (int)aRename.size();
    for (size_t k = 0; k < aRename.size(); k++)
        if (std::string* p = W3dDestResolve(aRename[k])) *p = unique;
    if (nvg > 0) W3dNoticeDrag(nvg, T("UV group(s)"), old, unique);   // ver W3dNotice.h
    g_redraw = true;
    return true;
}

// mueve (du,dv) las puntas SELECCIONADAS (con la soldadura de Bone2DSelectionMasks) en UN paso
// de undo. Lo usa la fila Pos de la tarjeta "Armature 2D" del panel Properties.
bool Bone2DMoveSelection(Mesh* m, float du, float dv) {
    if (!m || (du == 0.0f && dv == 0.0f)) return false;
    std::vector<int> bones, masks;
    if (!Bone2DSelectionMasks(m, bones, masks)) return false;
    UndoBones2DCapture(m);
    for (size_t k = 0; k < bones.size(); k++) {
        W3dBone2D& b = m->Arm2DBones()[bones[k]];
        if (masks[k] & 1) { b.headU += du; b.headV += dv; }
        if (masks[k] & 2) { b.tailU += du; b.tailV += dv; }
    }
    g_redraw = true;
    return true;
}

bool UVEditor::Bone2DXformActive() const { return gB2D.mode != 0; }

// A / Alt+A en los modos Huesos/Pose: TODOS los huesos (con sus dos puntas) o ninguno. Con la
// seleccion completa las ops (G/R/S, X, Shift+D, Ctrl+P) toman todo. Publico: lo llaman el
// teclado y el harness.
void UVEditor::Bone2DSelectAll(Mesh* m, bool sel) {
    if (!m) return;
    if (!sel) { Bone2DSelClear(m); m->Arm2DActiveBone() = -1; }
    else {
        for (size_t i = 0; i < m->Arm2DBones().size(); i++) Bone2DSelBone(m, (int)i, true);
        if (m->Arm2DActiveBone() < 0 && !m->Arm2DBones().empty()) m->Arm2DActiveBone() = 0; // que haya activo
    }
    g_redraw = true;
}

// A / Alt+A en EDICION de UVs: seleccionar / deseleccionar TODO. En SYNC la seleccion del UV es
// un espejo de la del 3D -> se toca la del 3D y se re-deriva. Fuera de sync se escribe uvSelVert
// pero SOLO lo VISIBLE (las esquinas de las caras del filtro 3D): "todo" es todo lo que se ve, no
// el mapa entero (seleccionar UVs invisibles y despues moverlos sin verlos no tiene sentido).
void UVEditor::SelectAllUV(Mesh* m, bool sel) {
    if (!m || m->vertexSize <= 0) return;
    if (syncSelection) { m->EditSelectAll(sel); SyncSelFrom3D(m); g_redraw = true; return; }
    const int nV = m->vertexSize;
    m->uvSelVert.assign((size_t)nV, 0);
    if (sel) {
        std::vector<char> fsel; UVFacesFilter3D(m, fsel);
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
// misma regla que el "todo" de SelectAllUV y que la inicializacion del filtro).
void UVEditor::InvertSelectionUV(Mesh* m) {
    if (!m || m->vertexSize <= 0) return;
    if (syncSelection) { InvertSelection(); SyncSelFrom3D(m); g_redraw = true; return; }
    const int nV = m->vertexSize;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign((size_t)nV, 0);
    std::vector<char> fsel; UVFacesFilter3D(m, fsel);
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
static const float UV_ISLAND_QUANT = 16384.0f; // ~6e-5 de tolerancia al comparar las UV de un borde
static inline int UVQuant(float x) { return (int)floorf(x * UV_ISLAND_QUANT + 0.5f); }

// un borde de UNA cara con su clave de union (el borde 3D + las UV de sus dos esquinas)
struct UVBorderIsland {
    int rA, rB;           // vertices 3D (posRep) del borde, ordenados: rA <= rB
    int uA, vA, uB, vB;   // UV cuantizadas de la esquina de rA y de la de rB (en ese orden)
    int face;             // que cara aporto este borde
};
static bool UVBorderSameKey(const UVBorderIsland& a, const UVBorderIsland& b) {
    return a.rA == b.rA && a.rB == b.rB && a.uA == b.uA && a.vA == b.vA && a.uB == b.uB && a.vB == b.vB;
}
static bool UVBorderLower(const UVBorderIsland& a, const UVBorderIsland& b) {
    if (a.rA != b.rA) return a.rA < b.rA;
    if (a.rB != b.rB) return a.rB < b.rB;
    if (a.uA != b.uA) return a.uA < b.uA;
    if (a.vA != b.vA) return a.vA < b.vA;
    if (a.uB != b.uB) return a.uB < b.uB;
    return a.vB < b.vB;
}

bool UVEditor::UVSelectLinked(Mesh* m, int seed, bool extend) {
    if (!m || m->vertexSize <= 0 || m->faces3d.empty() || !m->uv) return false;
    // SYNC ON (modo ESPEJO): la seleccion del UV es la del 3D, asi que el "vinculado" es el DEL 3D
    // (EditMesh::SelectLinked, la isla por aristas de la malla) sobre el elemento ACTIVO --
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
        e->SelectLinked(k, EditSelectMode, !extend);
        SyncSelFrom3D(m);
        g_redraw = true;
        return true;
    }
    const int nV = m->vertexSize;
    const int nF = (int)m->faces3d.size();
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign((size_t)nV, 0);
    // solo se recorre lo VISIBLE (el filtro del 3D): una isla no se propaga por caras que el UV
    // no dibuja, y nunca queda seleccionado algo que no se ve. Sin Edit Mode no hay filtro que
    // seguir -> se ve todo el mapa (misma regla que el pick).
    std::vector<char> fsel; UVFacesFilter3D(m, fsel);
    if ((Object*)m != g_editMesh) fsel.assign((size_t)nF, 1);
    const bool hasRep = ((int)m->posRep.size() == nV);

    // 1) todos los bordes de las caras visibles, con su clave
    std::vector<UVBorderIsland> borders;
    for (int f = 0; f < nF; f++) {
        if (f < (int)fsel.size() && !fsel[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        const int nc = (int)id.size();
        if (nc < 2) continue;
        for (int c = 0; c < nc; c++) {
            int ka = id[c], kb = id[(c + 1) % nc];
            if (ka < 0 || ka >= nV || kb < 0 || kb >= nV) continue;
            int ra = hasRep ? m->posRep[ka] : ka;
            int rb = hasRep ? m->posRep[kb] : kb;
            if (ra == rb) continue;                 // borde degenerado
            UVBorderIsland e2;
            e2.face = f;
            if (ra <= rb) { e2.rA = ra; e2.rB = rb;
                            e2.uA = UVQuant(m->uv[ka*2]); e2.vA = UVQuant(m->uv[ka*2+1]);
                            e2.uB = UVQuant(m->uv[kb*2]); e2.vB = UVQuant(m->uv[kb*2+1]); }
            else          { e2.rA = rb; e2.rB = ra;
                            e2.uA = UVQuant(m->uv[kb*2]); e2.vA = UVQuant(m->uv[kb*2+1]);
                            e2.uB = UVQuant(m->uv[ka*2]); e2.vB = UVQuant(m->uv[ka*2+1]); }
            borders.push_back(e2);
        }
    }
    // 2) mismas claves juntas -> esas caras son vecinas EN EL UV (comparten el borde ya cosido)
    std::sort(borders.begin(), borders.end(), UVBorderLower);
    std::vector<std::vector<int> > ady((size_t)nF);
    for (size_t i = 0; i < borders.size(); ) {
        size_t j = i + 1;
        while (j < borders.size() && UVBorderSameKey(borders[i], borders[j])) j++;
        for (size_t a = i; a < j; a++)              // grupos de 2 (o mas, en no-manifold)
            for (size_t b = a + 1; b < j; b++)
                if (borders[a].face != borders[b].face) {
                    ady[borders[a].face].push_back(borders[b].face);
                    ady[borders[b].face].push_back(borders[a].face);
                }
        i = j;
    }
    // 3) SEMILLA: la(s) face(s) del render-vert pedido (tecla L, el de abajo del cursor) o, si no
    //    se paso ninguno (menu Select), todas las caras visibles con alguna esquina seleccionada.
    std::vector<unsigned char> inIsland((size_t)nF, 0);
    std::vector<int> stack;
    for (int f = 0; f < nF; f++) {
        if (f < (int)fsel.size() && !fsel[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        bool sem = false;
        for (size_t c = 0; c < id.size() && !sem; c++) {
            int k = id[c]; if (k < 0 || k >= nV) continue;
            sem = (seed >= 0) ? (k == seed) : (m->uvSelVert[k] != 0);
        }
        if (sem && !inIsland[f]) { inIsland[f] = 1; stack.push_back(f); }
    }
    if (stack.empty()) return false;                 // sin semilla no hay isla que buscar
    // 4) flood fill por los bordes UV cosidos
    while (!stack.empty()) {
        int f = stack.back(); stack.pop_back();
        const std::vector<int>& vec = ady[f];
        for (size_t j = 0; j < vec.size(); j++)
            if (!inIsland[vec[j]]) { inIsland[vec[j]] = 1; stack.push_back(vec[j]); }
    }
    // 5) resultado: TODAS las esquinas de las caras de la isla (la seleccion del UV es por
    //    render-vert; el modo vertex/edge/face solo cambia como se pickea, no como se guarda)
    if (!extend) m->uvSelVert.assign((size_t)nV, 0);
    for (int f = 0; f < nF; f++) {
        if (!inIsland[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) { int k = id[c];
            if (k >= 0 && k < nV) m->uvSelVert[k] = 1; }
    }
    g_redraw = true;
    return true;
}

// render-vert VISIBLE mas cercano al cursor: la semilla del Select Linked por TECLA. Sin radio
// maximo a proposito (la L del 3D tambien agarra la isla mas cercana, no exige apuntar fino).
int UVEditor::VertUnderCursorUV(Mesh* m) const {
    if (!m || !m->uv || m->vertexSize <= 0) return -1;
    float cx, cy, s; ParamsUV(cx, cy, s);
    const float clx = (float)(lastMx - x), cly = (float)(lastMy - y);
    std::vector<char> fsel; UVFacesFilter3D(m, fsel);
    const bool inEdit = ((Object*)m == g_editMesh);
    int best = -1; float bestD = 0.0f;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const bool vis = inEdit ? (f < fsel.size() && fsel[f] != 0) : true;
        if (!syncSelection && !vis) continue;       // fuera de sync solo cuenta lo que se dibuja
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c]; if (k < 0 || k >= m->vertexSize) continue;
            float sx, sy; UVtoScreen(m->uv[k*2], m->uv[k*2+1], cx, cy, s, sx, sy);
            float d = (sx - clx)*(sx - clx) + (sy - cly)*(sy - cly);
            if (best < 0 || d < bestD) { bestD = d; best = k; }
        }
    }
    return best;
}

// Ctrl+I en Huesos/Pose: invierte la seleccion de huesos 2D. Un hueso cuenta como SELECCIONADO si
// tiene algo prendido (el hueso entero o alguna punta) -> queda deseleccionado; el resto entra
// ENTERO (con sus dos puntas), que es lo que esperan las ops (G/R/S, X, Shift+D, Ctrl+P).
void UVEditor::Bone2DInvertSelection(Mesh* m) {
    if (!m || m->Arm2DBones().empty()) return;
    const int n = (int)m->Arm2DBones().size();
    std::vector<unsigned char> prev((size_t)n, 0);
    for (int i = 0; i < n; i++) {
        const W3dBone2D& b = m->Arm2DBones()[i];
        prev[i] = (b.select || b.selHead || b.selTail) ? 1 : 0;
    }
    Bone2DSelClear(m);
    for (int i = 0; i < n; i++) if (!prev[i]) Bone2DSelBone(m, i, true);
    // el activo que quedo deseleccionado se apaga (igual que el Invert de Pose Mode del 3D)
    if (m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < n && !m->Arm2DBones()[m->Arm2DActiveBone()].select)
        m->Arm2DActiveBone() = -1;
    g_redraw = true;
}

// Select Linked (L) en Huesos/Pose: la CADENA conectada. Recorre el ARBOL del armature 2D en los
// DOS sentidos (al padre y a los hijos) desde cada hueso con algo seleccionado (o desde el activo
// si no hay seleccion) -> agarra la cadena entera y NO las otras cadenas del mismo armature.
// El flag 'conectado' (soldadura de puntas) NO importa: un hijo emparentado pero separado sigue
// siendo parte de la misma cadena (es lo que dice el arbol, que es lo que se anima).
bool UVEditor::Bone2DSelectLinked(Mesh* m, bool extend) {
    if (!m || m->Arm2DBones().empty()) return false;
    const int n = (int)m->Arm2DBones().size();
    std::vector<unsigned char> inChain((size_t)n, 0);
    std::vector<int> stack;
    for (int i = 0; i < n; i++) {
        const W3dBone2D& b = m->Arm2DBones()[i];
        if (b.select || b.selHead || b.selTail) { inChain[i] = 1; stack.push_back(i); }
    }
    if (stack.empty() && m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < n) {
        inChain[m->Arm2DActiveBone()] = 1; stack.push_back(m->Arm2DActiveBone());
    }
    if (stack.empty()) return false;
    while (!stack.empty()) {
        int b = stack.back(); stack.pop_back();
        int p = m->Arm2DBones()[b].parent;                       // hacia arriba
        if (p >= 0 && p < n && !inChain[p]) { inChain[p] = 1; stack.push_back(p); }
        for (int c = 0; c < n; c++)                            // hacia abajo
            if (m->Arm2DBones()[c].parent == b && !inChain[c]) { inChain[c] = 1; stack.push_back(c); }
    }
    if (!extend) Bone2DSelClear(m);
    for (int i = 0; i < n; i++) if (inChain[i]) Bone2DSelBone(m, i, true);
    if (m->Arm2DActiveBone() < 0) {                                 // que quede un activo de la cadena
        for (int i = 0; i < n; i++) if (inChain[i]) { m->Arm2DActiveBone() = i; break; }
    }
    g_redraw = true;
    return true;
}

// snapshot comun del transform modal de huesos (edicion o pose segun el modo del editor).
// masks (solo edicion): que punta de cada hueso se mueve; el PIVOTE es la mediana de las puntas
// EN MOVIMIENTO sin contar dos veces la compartida soldada (como BoneGrabMedian del 3D).
static void B2DSnapshot(UVEditor* uv, Mesh* m, const std::vector<int>& bones,
                        const std::vector<int>& masks, int mode, bool byClick) {
    gB2D.uv = uv; gB2D.mode = mode; gB2D.byClick = byClick;
    gB2D.axis = 0;                       // cada transform arranca SIN bloqueo (como el de UVs)
    gB2D.inPose = (uv->uvMode == UVModePose);
    gB2D.bones = bones;
    gB2D.masks = masks;
    if (gB2D.masks.size() != bones.size()) gB2D.masks.assign(bones.size(), 3);
    gB2D.snap.clear();
    double su = 0, sv = 0; int np = 0;
    if (gB2D.inPose) {
        std::vector<float> M; m->Armature2DMatrices(M);
        for (size_t k = 0; k < bones.size(); k++) {
            const W3dBone2D& b = m->Arm2DBones()[bones[k]];
            gB2D.snap.push_back(b.poseTU); gB2D.snap.push_back(b.poseTV);
            gB2D.snap.push_back(b.poseRot);
            gB2D.snap.push_back(b.poseSX); gB2D.snap.push_back(b.poseSY);
            float hu, hv, tu, tv; Bone2DEnds(m, M, (size_t)bones[k], true, hu, hv, tu, tv);
            su += hu; sv += hv; np++;      // pivote de pose: mediana de los HEAD posados
        }
    } else {
        // que TAILS se mueven (para no contar doble la punta compartida soldada en la mediana)
        std::vector<char> tailMoves(m->Arm2DBones().size(), 0);
        for (size_t k = 0; k < bones.size(); k++)
            if (gB2D.masks[k] & 2) tailMoves[bones[k]] = 1;
        for (size_t k = 0; k < bones.size(); k++) {
            const W3dBone2D& b = m->Arm2DBones()[bones[k]];
            gB2D.snap.push_back(b.headU); gB2D.snap.push_back(b.headV);
            gB2D.snap.push_back(b.tailU); gB2D.snap.push_back(b.tailV);
            if (gB2D.masks[k] & 1) {
                int p = b.parent;
                bool shared = Bone2DIsConnected(m, bones[k]) &&
                                  p >= 0 && p < (int)m->Arm2DBones().size() && tailMoves[p];
                if (!shared) { su += b.headU; sv += b.headV; np++; }
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
void UVEditor::Bone2DXformStart(Mesh* m, int mode) {
    if (!m || m->Arm2DBones().empty() || (uvMode != UVModeBones && uvMode != UVModePose)) return;
    std::vector<int> bones, masks;
    if (uvMode == UVModePose) {
        // POSE: siempre huesos ENTEROS (la pose es del hueso, no de una punta)
        for (size_t i = 0; i < m->Arm2DBones().size(); i++) if (m->Arm2DBones()[i].select) bones.push_back((int)i);
        if (bones.empty() && m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < (int)m->Arm2DBones().size())
            bones.push_back(m->Arm2DActiveBone());
        masks.assign(bones.size(), 3);
    } else if (!Bone2DSelectionMasks(m, bones, masks)) return; // edicion: puntas + soldadura
    if (bones.empty()) return;
    UndoBones2DStart(m); // pendiente hasta confirmar (ANTES del RestCapturar: snapshot pre-op)
    if (uvMode == UVModePose) m->Armature2DRestCapture(); // el rest queda fijo ANTES de posar
    B2DSnapshot(this, m, bones, masks, mode, false);
    // MRU de la toolbar: teclado y toolbar alimentan el mismo historial (como en edicion)
    ToolbarMRU(UVToolHist(), (mode == 1) ? TBMove : (mode == 2) ? TBRotate : TBScale);
}

// drag de LO SELECCIONADO (puntas/huesos con soldadura): lo arranca el click de seleccion.
void UVEditor::Bone2DDragSelection(Mesh* m, bool byClick) {
    if (!m || uvMode != UVModeBones) return;
    std::vector<int> bones, masks;
    if (!Bone2DSelectionMasks(m, bones, masks)) return;
    UndoBones2DStart(m); // pendiente hasta confirmar (el drag es UN paso de undo)
    B2DSnapshot(this, m, bones, masks, 1, byClick);
}

// deja seleccionada SOLO esa punta (mask 1=head 2=tail; 3 = hueso entero) y arranca su drag.
// Lo usan el extrude (tail nuevo agarrado al mouse) y el harness.
void UVEditor::Bone2DDragEnd(Mesh* m, int bone, int mask, bool byClick) {
    if (!m || uvMode != UVModeBones || bone < 0 || bone >= (int)m->Arm2DBones().size()) return;
    Bone2DSelClear(m);
    if (mask == 1 || mask == 2) Bone2DSelTip(m, bone, mask, true);
    else                        Bone2DSelBone(m, bone, true);
    Bone2DDragSelection(m, byClick);
}

// NUCLEO del apply de huesos: recibe los VALORES ya resueltos (delta / angulo en grados /
// factor) y los aplica desde el snapshot. Lo comparten el mouse (Bone2DXformDelta) y la
// entrada numerica exacta (XformNumValue), sin duplicar los loops.
// El BLOQUEO DE EJE (gB2D.axis) se aplica ACA ADENTRO, igual que el uvXAxis del transform de
// UVs (ApplyXformValues): mover filtra la componente del eje libre, escalar deja el otro
// eje en 1.0 y rotar lo ignora (una rotacion 2D no tiene eje).
static void B2DApplyValues(Mesh* m, float du, float dv, float ang, float f) {
    if (!gB2D.mode || !m) return;
    if (gB2D.mode == 1) {                        // MOVER: el lock anula el otro eje
        if (gB2D.axis == 1) dv = 0.0f;
        if (gB2D.axis == 2) du = 0.0f;
    }
    // ESCALAR: factor POR EJE (bloqueo X -> Y no escala, y al reves). Mismo criterio que el UV.
    const float fx = (gB2D.mode == 3 && gB2D.axis == 2) ? 1.0f : f;
    const float fy = (gB2D.mode == 3 && gB2D.axis == 1) ? 1.0f : f;
    gB2D.valU = du; gB2D.valV = dv; gB2D.valAng = ang; gB2D.valFac = f; // barra de INFO
    const float DEG = 3.14159265358979f / 180.0f;
    float c = cosf(ang * DEG), s2 = sinf(ang * DEG);
    for (size_t k = 0; k < gB2D.bones.size(); k++) {
        int bi = gB2D.bones[k];
        if (bi < 0 || bi >= (int)m->Arm2DBones().size()) continue;
        W3dBone2D& b = m->Arm2DBones()[bi];
        const float* sn = &gB2D.snap[k * (gB2D.inPose ? 5 : 4)];
        if (gB2D.inPose) {
            // POSE 2D: G suma traslacion, R suma rotacion (alrededor del head), S multiplica escala
            b.poseTU = sn[0]; b.poseTV = sn[1]; b.poseRot = sn[2]; b.poseSX = sn[3]; b.poseSY = sn[4];
            if (gB2D.mode == 1)      { b.poseTU += du; b.poseTV += dv; }
            else if (gB2D.mode == 2) { b.poseRot += ang; }
            else                     { b.poseSX *= fx; b.poseSY *= fy; }
        } else {
            // EDICION del rest: mueve/rota/escala SOLO las puntas del mask (la seleccion por
            // puntas + soldadura), alrededor del pivote (mediana de las puntas en movimiento)
            const int mk = (k < gB2D.masks.size()) ? gB2D.masks[k] : 3;
            float hu = sn[0], hv = sn[1], tu = sn[2], tv = sn[3];
            if (gB2D.mode == 1) {
                if (mk & 1) { hu += du; hv += dv; }
                if (mk & 2) { tu += du; tv += dv; }
            } else if (gB2D.mode == 2) {
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
    if (gB2D.inPose) { m->Armature2DRestCapture(); m->Armature2DApply(); } // los UV pesados siguen al hueso
    g_redraw = true;
}

// aplica el transform EN VIVO desde el snapshot (delta absoluto: no acumula drift): deriva
// delta/angulo/factor de la posicion del cursor (virtual) y delega en B2DApplyValues.
void UVEditor::Bone2DXformDelta(Mesh* m, float curU, float curV) {
    if (!gB2D.mode || !m) return;
    float du = curU - gB2D.startU, dv = curV - gB2D.startV;
    float ang = 0.0f, f = 1.0f;
    if (gB2D.mode == 2 || gB2D.mode == 3) {
        float a0 = atan2f(gB2D.startV - gB2D.pivV, gB2D.startU - gB2D.pivU);
        float a1 = atan2f(curV - gB2D.pivV, curU - gB2D.pivU);
        ang = (a1 - a0) * 180.0f / 3.14159265f;
        float d0 = sqrtf((gB2D.startU-gB2D.pivU)*(gB2D.startU-gB2D.pivU) + (gB2D.startV-gB2D.pivV)*(gB2D.startV-gB2D.pivV));
        float d1 = sqrtf((curU-gB2D.pivU)*(curU-gB2D.pivU) + (curV-gB2D.pivV)*(curV-gB2D.pivV));
        f = (d0 > 1e-5f) ? d1 / d0 : 1.0f;
    }
    B2DApplyValues(m, du, dv, ang, f);
}

void UVEditor::Bone2DXformConfirm(Mesh* m) {
    if (!gB2D.mode) return;
    UndoBones2DConfirm(); // pushea el pendiente (descarta si no cambio nada: G+click sin mover)
    bool inPose = gB2D.inPose;
    // el snapshot de la pose de ANTES del transform (5 floats por hueso) lo necesita el AUTO KEY por
    // canal, y abajo se limpia: copiarlo primero.
    std::vector<int>   b0 = gB2D.bones;
    std::vector<float> s0 = gB2D.snap;
    gB2D.mode = 0; gB2D.axis = 0; gB2D.uv = NULL; gB2D.byClick = false;
    gB2D.bones.clear(); gB2D.masks.clear(); gB2D.snap.clear();
    if (m && inPose) {
        m->skinGeomVersion++; // los UV cambiaron -> re-subir el VBO de uv en Object Mode
        // AUTO KEY de la POSE 2D. DOS caminos, y el orden importa:
        //  1) la malla ya tiene CLIPS del armature 2D (o no tiene ninguna animacion todavia): se
        //     keyframean las CURVAS POR HUESO (posU/posV/rot/escalaX/escalaY), solo los canales que
        //     cambiaron. Es el camino nuevo: 5 floats por hueso movido en vez del array uv entero.
        //  2) RETROCOMPAT: proyectos viejos donde la pose 2D se horneaba en la capa uv de la vertex
        //     anim ACTIVA (kind 3). Si eso es lo que esta activo en el timeline y la malla NO tiene
        //     clips 2D, se sigue horneando igual que siempre -> los .w3d de antes siguen andando.
        extern bool AutoKeyOn; extern int ActiveAnimKind; extern Mesh* ActiveAnimMesh;
        extern void VertexAnimInsertKeyframeUV();
        extern void AnimSelArm2D(Mesh*, int);
        if (AutoKeyOn && (Object*)m == g_editMesh) {
            bool bake = (ActiveAnimKind == 3 && (Mesh*)ActiveAnimMesh == m && m->Arm2DAnims().empty());
            if (bake) {
                VertexAnimInsertKeyframeUV();
            } else if (!b0.empty() && s0.size() >= b0.size() * 5 && AutoKeyArm2DPrep(m)) {
                int n = 0;
                for (size_t k = 0; k < b0.size(); k++)
                    n += AutoKeyBone2D(m, b0[k], s0[k*5+0], s0[k*5+1], s0[k*5+2], s0[k*5+3], s0[k*5+4]);
                if (n > 0) {
                    AutoKeyArm2DEnd(m);
                    if (ActiveAnimKind != 4 || ActiveAnimMesh != m) AnimSelArm2D(m, m->Arm2DActiveAnim());
                }
            }
        }
    }
    g_redraw = true;
}

void UVEditor::Bone2DXformCancel(Mesh* m) {
    if (!gB2D.mode) return;
    if (m) { // restaurar el snapshot
        for (size_t k = 0; k < gB2D.bones.size(); k++) {
            int bi = gB2D.bones[k];
            if (bi < 0 || bi >= (int)m->Arm2DBones().size()) continue;
            W3dBone2D& b = m->Arm2DBones()[bi];
            const float* sn = &gB2D.snap[k * (gB2D.inPose ? 5 : 4)];
            if (gB2D.inPose) { b.poseTU = sn[0]; b.poseTV = sn[1]; b.poseRot = sn[2]; b.poseSX = sn[3]; b.poseSY = sn[4]; }
            else             { b.headU = sn[0]; b.headV = sn[1]; b.tailU = sn[2]; b.tailV = sn[3]; }
        }
        if (gB2D.inPose) m->Armature2DApply(); // los UV vuelven a la pose previa
    }
    UndoBones2DCancel(); // el pendiente se descarta (no hubo operacion)
    gB2D.mode = 0; gB2D.axis = 0; gB2D.uv = NULL; gB2D.byClick = false;
    gB2D.bones.clear(); gB2D.masks.clear(); gB2D.snap.clear();
    g_redraw = true;
}

// pick de hueso 2D bajo el mouse (px LOCALES del viewport). pose=true usa los extremos POSADOS
// y devuelve el hueso ENTERO (en pose no hay puntas). En EDICION porta el modelo del 3D
// (BonePickEdit): las PUNTAS tienen prioridad (radio 12px) sobre el cuerpo (14px) -> click cerca
// de una punta selecciona SOLO esa punta. outMask: 1=head 2=tail 3=cuerpo/entero.
int UVEditor::Bone2DPick(Mesh* m, float lx, float ly, bool pose, int* outMask) {
    if (outMask) *outMask = 3;
    if (!m || m->Arm2DBones().empty()) return -1;
    float cx, cy, s; ParamsUV(cx, cy, s);
    std::vector<float> M;
    if (pose) m->Armature2DMatrices(M);
    int bestP = -1, bestPMask = 0;
    float bestPD = 12.0f * 12.0f;    // mejor PUNTA (radio 12 px; distancias^2)
    int bestB = -1;
    float bestBD = 14.0f * 14.0f;    // mejor CUERPO (14 px, como el pick de vertices)
    for (size_t i = 0; i < m->Arm2DBones().size(); i++) {
        float hu, hv, tu, tv;
        Bone2DEnds(m, M, i, pose, hu, hv, tu, tv);
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
