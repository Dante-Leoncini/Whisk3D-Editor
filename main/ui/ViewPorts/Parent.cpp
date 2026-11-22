// ===================================================================================================
//  EMPARENTAR / DESEMPARENTAR (Ctrl+P / Alt+P). Extraido de LayoutInput.cpp (Fase 2). Ver Parent.h.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmPopup.h" // OpenConfirmDelete (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
#include "ViewPorts/Notifications.h" // toasts (extraido a su propio archivo)
#include "ViewPorts/NumInput.h" // entrada numerica/formulas (extraido a su propio archivo)
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/UVEditor.h"
#include "ViewPorts/Timeline.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h" // el velo del modo foco
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Materials.h" // Material (mat->texture) para el dropdown "Texture" del UV editor
#include "objects/Textures.h"  // Texture (path) para las etiquetas del dropdown
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertKeyframeSkeleton (Pose Mode: Insert Keyframe)
#include "objects/Instance.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "objects/Primitives.h"
#include "variables.h"
#include "render/RenderOptions.h" // g_currentFps
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/theme/colors.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
#include "edit/BoneEdit.h"  // Ctrl+P / Alt+P en EDICION de huesos 3D (BoneEditActive + ops)
#include "ViewPorts/Parent.h"

// ====================================================================
// menus de emparentar (ctrl+P / alt+P)
// ====================================================================

static PopupMenu* gMenuSetParent = NULL;
static PopupMenu* gMenuClearParent = NULL;

// los seleccionados (sin contar al activo / a los ya-raiz)
static void CollectSelected(Object* aNode, std::vector<Object*>& aSel,
                                    Object* aExclude) {
    if (!aNode) return;
    for (size_t i = 0; i < aNode->Childrens.size(); i++) {
        Object* o = aNode->Childrens[i];
        if (o->select && o != aExclude) aSel.push_back(o);
        CollectSelected(o, aSel, aExclude);
    }
}

// 0 Object, 1 Keep Transform, 2 Without Inverse, 3 Keep T. Without Inv.
// (sin matrices de inversa propias: 0==2 y 1==3 en nuestro modelo)
void ActionSetParent(int aId) {
    if (!ActiveObj || !SceneCollection) return;
    std::vector<Object*> sel;
    CollectSelected(SceneCollection, sel, ActiveObj);
    // emparentar N objetos es UNA accion: los N pasos de undo se funden en uno (ObjectMode.h)
    ReparentGroupStart();
    for (size_t i = 0; i < sel.size(); i++) {
        if (aId == 1 || aId == 3) ReparentKeepTransform(sel[i], ActiveObj);
        else ReparentSimple(sel[i], ActiveObj);
    }
    ReparentGroupEnd();
}

// 0 Clear Parent, 1 Clear and Keep Transformation, 2 Clear Parent Inverse
void ActionClearParent(int aId) {
    if (!SceneCollection) return;
    std::vector<Object*> sel;
    CollectSelected(SceneCollection, sel, NULL);
    ReparentGroupStart();   // idem: N desemparentados = UN Ctrl+Z
    for (size_t i = 0; i < sel.size(); i++) {
        if (!sel[i]->Parent || sel[i]->Parent == SceneCollection) continue;
        if (aId == 1) ReparentKeepTransform(sel[i], SceneCollection);
        else ReparentSimple(sel[i], SceneCollection);
    }
    ReparentGroupEnd();
}

// Los menus Set/Clear Parent se usan de DOS formas: standalone (Ctrl+P / Ctrl+Alt+P) y como SUBMENUS del menu
// "Object" de la barra. Para que anden igual en ambos casos usan ids UNICOS (230-233 / 240-242) + la accion
// LayoutActionObject (que rutea esos ids a ActionSetParent/ActionClearParent), como el submenu Apply/Delete.
PopupMenu* LayoutSubmenuSetParent() {
    if (!gMenuSetParent) {
        gMenuSetParent = new PopupMenu();
        gMenuSetParent->title  = T("Set Parent To");
        gMenuSetParent->action = LayoutActionObject;
        gMenuSetParent->Add(T("Object"), 230);
        gMenuSetParent->Add(T("Object (Keep Transform)"), 231);
        gMenuSetParent->Add(T("Object (Without Inverse)"), 232);
        gMenuSetParent->Add(T("Object (Keep Transform Without Inverse)"), 233);
    }
    return gMenuSetParent;
}
PopupMenu* LayoutSubmenuClearParent() {
    if (!gMenuClearParent) {
        gMenuClearParent = new PopupMenu();
        gMenuClearParent->title  = T("Clear Parent");
        gMenuClearParent->action = LayoutActionObject;
        gMenuClearParent->Add(T("Clear Parent"), 240);
        gMenuClearParent->Add(T("Clear and Keep Transformation"), 241);
        gMenuClearParent->Add(T("Clear Parent Inverse"), 242);
    }
    return gMenuClearParent;
}

void LayoutMenuParent(bool aClear, int mx, int my) {
    if (aClear) {
        PopupMenu* m = LayoutSubmenuClearParent();
        m->Open(mx, my, MenuScreenW, MenuScreenH);
        OpenMenu = m;
    } else {
        if (!ActiveObj) return; // no hay a quien emparentar
        PopupMenu* m = LayoutSubmenuSetParent();
        m->Open(mx, my, MenuScreenW, MenuScreenH);
        OpenMenu = m;
    }
}

// ====================================================================
// EMPARENTAR HUESOS (Ctrl+P / Alt+P en EDICION de huesos, 3D y armature 2D del UV editor).
// Alt+P abre EL MISMO popup de 2 opciones en ambos editores:
//   - "Disconnect Bone": sigue emparentado pero SIN soldar (al separarlo queda la linea punteada)
//   - "Clear Parent":    hueso libre (padre = -1)
// Ctrl+P abre OTRO popup de 2 opciones (mismo patron):
//   - "Keep Offset": el hueso queda donde esta (linea punteada al padre)
//   - "Connected":   el hueso se MUEVE para pegar su head al tail del padre (soldado)
// El contexto (armature 3D vs mesh 2D) se fija al ABRIR el menu; la accion despacha segun eso.
// Los dos menus + el checkbox "Connected" del panel operan el MISMO flag 'conectado'
// (Bone/Bone2D SetConectado); no hay caminos paralelos.
// ====================================================================
static PopupMenu* gMenuBoneAltP  = NULL;
static PopupMenu* gMenuBoneCtrlP = NULL;
static int   g_boneAltPCtx  = 0;    // 1 = huesos 3D (Edit de armature); 2 = huesos 2D (UV editor)
static Mesh* g_boneAltPMesh = NULL; // el mesh del contexto 2D

static void ActionBoneAltP(int aId) { // 0 = Disconnect Bone; 1 = Clear Parent
    if (g_boneAltPCtx == 1) {
        Armature* a = BoneEditArm();
        if (!a) return;
        if (aId == 0) BoneEditDisconnectConnected(a);
        else          BoneEditDisconnect(a);
    } else if (g_boneAltPCtx == 2 && g_boneAltPMesh) {
        if (aId == 0) Bone2DDisconnectSel(g_boneAltPMesh);
        else          Bone2DClearParentSel(g_boneAltPMesh);
    }
    g_redraw = true;
}

// Ctrl+P: 0 = Keep Offset (lo de siempre), 1 = Connected (soldado al tail del padre)
static void ActionBoneCtrlP(int aId) {
    const bool connect = (aId == 1);
    if (g_boneAltPCtx == 1) {
        Armature* a = BoneEditArm();
        if (a) BoneParentSelectionToActive(a, connect);
    } else if (g_boneAltPCtx == 2 && g_boneAltPMesh) {
        Bone2DParentSelectionToActive(g_boneAltPMesh, connect);
    }
    g_redraw = true;
}

PopupMenu* LayoutSubmenuBoneCtrlP() {
    if (!gMenuBoneCtrlP) {
        gMenuBoneCtrlP = new PopupMenu();
        gMenuBoneCtrlP->title  = T("Set Parent To"); // el titulo del Ctrl+P
        gMenuBoneCtrlP->action = ActionBoneCtrlP;
        gMenuBoneCtrlP->Add(T("Keep Offset"), 0);
        gMenuBoneCtrlP->Add(T("Connected"), 1);
    }
    return gMenuBoneCtrlP;
}

// abre el popup Ctrl+P sobre los huesos 2D de ESTE mesh (tambien lo usa el menu Armature de la
// barra del UV editor). Calca LayoutMenuBoneAltP2D.
void LayoutMenuBoneCtrlP2D(Mesh* m, int mx, int my) {
    if (!m) return;
    BoneAltPContext2D(m);
    PopupMenu* mn = LayoutSubmenuBoneCtrlP();
    if (OpenMenu && OpenMenu != mn) OpenMenu->Close();
    mn->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = mn;
}

PopupMenu* LayoutSubmenuBoneAltP() {
    if (!gMenuBoneAltP) {
        gMenuBoneAltP = new PopupMenu();
        gMenuBoneAltP->title  = T("Clear Parent"); // el titulo del Alt+P
        gMenuBoneAltP->action = ActionBoneAltP;    // action PROPIA: funciona standalone Y como submenu
        gMenuBoneAltP->Add(T("Disconnect Bone"), 0);
        gMenuBoneAltP->Add(T("Clear Parent"), 1);
    }
    return gMenuBoneAltP;
}
void BoneAltPContext3D()        { g_boneAltPCtx = 1; g_boneAltPMesh = NULL; }
void BoneAltPContext2D(Mesh* m) { g_boneAltPCtx = 2; g_boneAltPMesh = m; }

// abre el popup Alt+P sobre los huesos 2D de ESTE mesh (tambien lo usa el boton Disconnect
// de la toolbar del UV editor). Calca el patron de los otros menus contextuales.
void LayoutMenuBoneAltP2D(Mesh* m, int mx, int my) {
    if (!m) return;
    BoneAltPContext2D(m);
    PopupMenu* mn = LayoutSubmenuBoneAltP();
    if (OpenMenu && OpenMenu != mn) OpenMenu->Close();
    mn->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = mn;
}

// contexto de HUESOS 2D activo: el viewport con foco es un editor UV en Edit Bones, sobre la
// malla activa en Edit Mode (la misma condicion de visibilidad del menu Armature de su barra).
static Mesh* MeshBones2DActive() {
    if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 4) return NULL;
    UVEditor* uv = (UVEditor*)viewPortActive;
    if (uv->uvMode != UVModeBones) return NULL;
    Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
    return (m && (Object*)m == g_editMesh) ? m : NULL;
}

// Ctrl+P / Alt+P con huesos en edicion: opera sobre HUESOS (nunca el popup de emparentar
// OBJETOS, que es de Object Mode -- era el bug reportado). false = no hay contexto de huesos.
bool LayoutParentBones(bool aAlt, int mx, int my) {
    if (BoneEditActive()) {              // EDIT de armature en el 3D
        BoneAltPContext3D();
        if (aAlt || !BoneGrabActive()) {
            PopupMenu* mn = aAlt ? LayoutSubmenuBoneAltP() : LayoutSubmenuBoneCtrlP();
            if (OpenMenu && OpenMenu != mn) OpenMenu->Close();
            mn->Open(mx, my, MenuScreenW, MenuScreenH);
            OpenMenu = mn;
        }
        g_redraw = true;
        return true;
    }
    Mesh* m2 = MeshBones2DActive();     // EDIT de huesos 2D en el UV editor
    if (m2) {
        if (aAlt) LayoutMenuBoneAltP2D(m2, mx, my);
        else      LayoutMenuBoneCtrlP2D(m2, mx, my);
        g_redraw = true;
        return true;
    }
    return false;
}
