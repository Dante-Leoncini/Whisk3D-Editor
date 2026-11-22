// ============================================================================
//  EditorRoots.cpp — ver EditorRoots.h. El editor sobre las raices del proyecto
//  (escenas 3D, juegos y prefabs): el cambio de raiz con sus precondiciones, el
//  selector, el estado base de un juego y los proveedores de las vistas "Scenes" /
//  "Prefabs" del outliner.
// ============================================================================
#include "io/EditorRoots.h"
#include "W3dRoots.h"
#include "io/ProjectResources.h"
#include "io/ProjectMeshes.h"           // W3dMeshesTickEditor: publicar la malla que sale de edicion
#include "io/ProjectChanges.h"         // el '*' de una raiz sin guardar (el selector)
#include "io/Prefabs.h"                  // los usuarios de un prefab (sus instancias) y su nombre
#include "io/PrefabsEditor.h"            // volver de editar un prefab regenera sus instancias
#include "objects/InstancePrefab.h"      // los usuarios de un prefab ("Select Users") son sus instancias
#include "objects/Objects.h"
#include "objects/Camera.h"              // la camara activa y su lente son parte del estado base de un juego
#include "objects/Light.h"
#include "objects/Mesh.h"                // los pesos de las shape keys (los posan los clips)
#include "objects/Armature.h"            // "Convert to Game": el clip y el cabezal que el cuadro le pone a un esqueleto
#include "script/W3dScript.h"            // "Convert to Game": los scripts de la copia, tal cual (W3dScriptData)
#include "animation/Animation.h"         // los clips de un juego: sus animaciones de escena
#include "animation/SkeletalAnimation.h" // g_animMix / W3dMixScenesRelease
#include "base/W3dInteractionState.h"    // InteractionMode / estado
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"        // salir de la local view
#include "ViewPorts/Outliner.h"          // soltar el arrastre / el modo mover del arbol que se deja
#include "ViewPorts/LayoutTree.h"        // LayoutCompleteRoot
#include "ViewPorts/LayoutInput.h"       // LayoutModePick / UpdateActiveEditMesh
#include "ViewPorts/Notifications.h"
#include "ViewPorts/PopUp/PopUpBase.h"   // PopUpActive: un popup abierto apunta a objetos de la raiz
#include "undo/Undo.h"
#include "config/W3dLang.h"
#include "render/RenderOptions.h"       // g_redraw
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/draw/icons.h"
#include "base/W3dNames.h"
#include "w3dlog.h"
#include <vector>
#include <map>
#include <cstdio>

extern bool SimActive();
extern void W3dCurveEditClose();
extern void PropsForgetScene();
extern bool PlayAnimation;

// (una ESCENA se anima y se renderiza: la camara; un JUEGO simula: el control de juego. Los dos son
// iconos que ya existian, no hay arte propio para las raices)
int W3dRootIcon(int type) {
    return type == W3D_ROOT_PREFAB ? (int)IconType::prefab
         : type == W3D_ROOT_GAME  ? (int)IconType::gamepad : (int)IconType::camera;
}

// ============================================================================
//  EL ESTADO BASE DE UN JUEGO (su "frame 1")
//  Un juego no tiene animacion propia: lo que se ve en "Juego" es su estado inicial. Sus
//  animaciones de escena son CLIPS (los dispara Lua): elegir uno en el timeline lo muestra y
//  lo deja editar, y el scrub POSA a los objetos. Al volver a "Juego" (o al dejar la raiz) cada
//  objeto que los clips pueden posar recupera lo que tenia al salir de "Juego". La foto va por
//  SERIAL (nunca reciclado): un objeto borrado mientras tanto no se toca.
// ============================================================================
namespace {
struct BaseObj {
    Object* o; unsigned int serial;
    Vector3 pos, scale, rotEuler; Quaternion rot;
    bool visible, renderable;
    float fov, nearClip, farClip;                 // camara
    float dif[4], amb[4], spe[4];                 // luz
    std::vector<float> shapes;                    // pesos de las shape keys
};
}
static std::vector<BaseObj> gBase;
static Object* gBaseRoot = NULL;         // de que raiz es la foto
static Camera* gBaseCamera = NULL;       // la camara activa (los cortes de un clip la cambian)
static unsigned int gBaseCamSerial = 0;

static void BaseCollect(Object* o) {
    if (!o) return;
    if (o != SceneCollection) {
        BaseObj b;
        b.o = o; b.serial = o->serial;
        b.pos = o->pos; b.scale = o->scale; b.rotEuler = o->rotEuler; b.rot = o->Rot();
        b.visible = o->visible; b.renderable = o->renderable;
        b.fov = b.nearClip = b.farClip = 0.0f;
        for (int k = 0; k < 4; k++) b.dif[k] = b.amb[k] = b.spe[k] = 0.0f;
        if (o->getType() == ObjectType::camera) {
            Camera* c = (Camera*)o; b.fov = c->fov; b.nearClip = c->nearClip; b.farClip = c->farClip;
        } else if (o->getType() == ObjectType::light) {
            Light* l = (Light*)o;
            for (int k = 0; k < 4; k++) { b.dif[k] = l->diffuse[k]; b.amb[k] = l->ambient[k]; b.spe[k] = l->specular[k]; }
        } else if (o->getType() == ObjectType::mesh) {
            b.shapes = ((Mesh*)o)->shapeWeights;
        }
        gBase.push_back(b);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) BaseCollect(o->Childrens[i]);
}
static void BaseSet(const BaseObj& b) {
    Object* o = b.o;
    o->pos = b.pos; o->scale = b.scale; o->SetRotSnapshot(b.rot, b.rotEuler);
    o->visible = b.visible; o->renderable = b.renderable;
    if (o->getType() == ObjectType::camera) {
        Camera* c = (Camera*)o; c->fov = b.fov; c->nearClip = b.nearClip; c->farClip = b.farClip;
    } else if (o->getType() == ObjectType::light) {
        Light* l = (Light*)o;
        for (int k = 0; k < 4; k++) { l->diffuse[k] = b.dif[k]; l->ambient[k] = b.amb[k]; l->specular[k] = b.spe[k]; }
    } else if (o->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)o;
        for (size_t k = 0; k < b.shapes.size(); k++) m->SetShapeWeight((int)k, b.shapes[k]);
    }
}
// el objeto sigue vivo y es el mismo? (su direccion pudo reciclarse: el serial no)
static bool BaseLive(const BaseObj& b) {
    return SceneCollection && W3dRootOf(b.o) == SceneCollection && b.o->serial == b.serial;
}
static const BaseObj* BaseOf(Object* o) {
    if (!o || gBaseRoot != SceneCollection) return NULL;
    for (size_t i = 0; i < gBase.size(); i++)
        if (gBase[i].o == o && gBase[i].serial == o->serial) return &gBase[i];
    return NULL;
}
// los objetos que los CLIPS de la raiz activa pueden posar (sus animaciones de escena: la activa vive
// en AnimationObjects, las otras en su SceneAnimation)
static void ClipObjects(std::vector<Object*>& out) {
    out.clear();
    for (size_t i = 0; i < AnimationObjects.size(); i++) if (AnimationObjects[i].obj) out.push_back(AnimationObjects[i].obj);
    for (size_t e = 0; e < SceneAnimations.size(); e++) {
        if ((int)e == ActiveSceneAnim || !SceneAnimations[e]) continue;
        const std::vector<AnimationObject>& v = SceneAnimations[e]->objects;
        for (size_t i = 0; i < v.size(); i++) if (v[i].obj) out.push_back(v[i].obj);
    }
}

void W3dGameBaseSave() {
    gBase.clear();
    gBaseRoot = SceneCollection;
    gBaseCamera = CameraActive;
    gBaseCamSerial = CameraActive ? ((Object*)CameraActive)->serial : 0;
    BaseCollect(SceneCollection);
}
bool W3dGameBaseHas() { return gBaseRoot != NULL && gBaseRoot == SceneCollection && !gBase.empty(); }
void W3dGameBaseForget() { gBase.clear(); gBaseRoot = NULL; gBaseCamera = NULL; gBaseCamSerial = 0; }
bool W3dGameBaseRestore() {
    if (!W3dGameBaseHas()) { W3dGameBaseForget(); return false; }
    std::vector<Object*> clips;
    ClipObjects(clips);
    int n = 0;
    for (size_t i = 0; i < clips.size(); i++) {
        const BaseObj* b = BaseOf(clips[i]);
        if (b && BaseLive(*b)) { BaseSet(*b); n++; }
    }
    // los CORTES de camara de un clip cambian la camara activa: vuelve la del juego
    if (gBaseCamera && W3dRootOf((Object*)gBaseCamera) == SceneCollection && ((Object*)gBaseCamera)->serial == gBaseCamSerial)
        CameraActive = gBaseCamera;
    w3dLogf("[raices] el juego vuelve a su estado base: %d objeto(s) de sus clips repuestos", n);
    W3dGameBaseForget();
    W3dAnimCurvesInvalidate();   // (si despues se elige un clip en este mismo frame, tiene que posar)
    g_redraw = true;
    return true;
}
void W3dGameBaseApplyTo(Object* o) {
    const BaseObj* b = BaseOf(o);
    if (b && BaseLive(*b)) BaseSet(*b);
}
// ~Object: una foto no puede quedar con un puntero a un objeto liberado (el serial ya lo cubre, pero
// ademas asi no crece con basura)
static void BaseUnlink(Object* o) {
    for (size_t i = gBase.size(); i-- > 0; ) if (gBase[i].o == o) gBase.erase(gBase.begin() + (long)i);
    if ((Object*)gBaseCamera == o) gBaseCamera = NULL;
    if (gBaseRoot == o) W3dGameBaseForget();
}
namespace {
struct RegisterBase { RegisterBase() { W3dUnlinkRegister(BaseUnlink); } } gRegisterBase;
}

// ============================================================================
//  EL CAMBIO DE RAIZ
// ============================================================================
static void CollectLeaves(ViewportBase* n, std::vector<ViewportBase*>& out) {
    if (!n) return;
    if (n->isLeaf()) { out.push_back(n); return; }
    if (n->ContainerKind() == 1) { CollectLeaves(((ViewportRow*)n)->childA, out); CollectLeaves(((ViewportRow*)n)->childB, out); }
    else { CollectLeaves(((ViewportColumn*)n)->childA, out); CollectLeaves(((ViewportColumn*)n)->childB, out); }
}

// lo que la UI tiene de la raiz que se deja: la local view (su conjunto nombra objetos de alla), el
// modo mover y el arrastre del outliner, y el objeto que Properties tenia bindeado
static void ReleaseUIOfRoot() {
    std::vector<ViewportBase*> leaves;
    CollectLeaves(LayoutCompleteRoot(), leaves);
    for (size_t i = 0; i < leaves.size(); i++) {
        ViewportBase* v = leaves[i];
        if (v->ViewportKind() == 1) {
            Viewport3D* v3 = (Viewport3D*)v;
            if (v3->localViewActive) v3->LocalViewExit();
        } else if (VpIsOutliner(v)) {
            Outliner* o = (Outliner*)v;
            if (o->moving) o->MoveCancel();
            o->dragObject = NULL; o->dragging = false; o->dropZone = -2; o->dropRow = -1;
            o->hoverRow = -1;
        }
    }
    PropsForgetScene();
}
// la UI de la raiz nueva: el outliner arranca arriba y rearma su scroll ('keepScroll': entre escenas abiertas,
// que el outliner muestra todas juntas, no se mueve)
static void RefreshUI(bool keepScroll = false) {
    std::vector<ViewportBase*> leaves;
    CollectLeaves(LayoutCompleteRoot(), leaves);
    for (size_t i = 0; i < leaves.size(); i++)
        if (VpIsOutliner(leaves[i])) {
            Outliner* o = (Outliner*)leaves[i];
            if (o->view == W3D_VIEW_SCENE && !keepScroll) { o->PosY = 0; o->PosX = 0; }
            o->lastContentRows = -1;
            if (o->width > 0 && o->height > 0) o->Resize(o->width, o->height);
        }
    W3dResourcesViewInvalidate();
    g_redraw = true;
}

// ============================================================================
//  LAS ESCENAS ABIERTAS A LA VEZ (W3dRoots.h): tildar en la lista abre; destildar cierra y libera la memoria
// ============================================================================
// cerrarla de verdad: fuera de la lista, sin sus pasos de undo ni su marca de "sin guardar", y fuera de la memoria
// (salvo la del proyecto -el bloque- y una que todavia no se guardo: quedan cargadas, solo se cierran)
static void UnloadClosed(const std::string& name) {
    const int idx = W3dRootFind(W3D_ROOT_SCENE, name);
    const std::vector<W3dRootRow>& fs = W3dRoots();
    if (idx < 0 || idx >= (int)fs.size()) return;
    Object* r = fs[(size_t)idx].root;
    W3dRootRemoveFromOpen(idx);
    if (r) { UndoForgetRoot(r); W3dChangesForgetRoot(r); }
    std::vector<int> v(1, idx);
    if (r && W3dRootsUnload(v) == 0)
        w3dLogf("[raices] '%s' cerrada (queda en memoria: %s)", name.c_str(),
                fs[(size_t)idx].block ? "es la del bloque del proyecto" : "todavia no se guardo");
    else w3dLogf("[raices] '%s' cerrada y fuera de la memoria", name.c_str());
    RefreshUI(true);
}
bool W3dRootCheck(int idx, bool open, std::string* reason) {
    const std::vector<W3dRootRow>& fs = W3dRoots();
    if (idx < 0 || idx >= (int)fs.size() || !W3dRootEs3D(fs[(size_t)idx].type)) {
        if (reason) *reason = "It doesn't exist anymore";
        return false;
    }
    if (SimActive()) { if (reason) *reason = "Stop the game first"; return false; }
    if (open) {
        if (!W3dRootOpen(idx)) { if (reason) *reason = "It could not be loaded"; return false; }
        w3dLogf("[raices] '%s' abierta", fs[(size_t)idx].name.c_str());
        RefreshUI(true);
        return true;
    }
    std::vector<int> ab;
    W3dOpenRoots(ab);
    bool is = false;
    for (size_t i = 0; i < ab.size(); i++) if (ab[i] == idx) is = true;
    if (!is) return true;
    if (ab.size() <= 1) { if (reason) *reason = "At least one scene stays open"; return false; }
    const std::string name = fs[(size_t)idx].name;
    // la ACTIVA no se cierra: primero pasa a ser activa la siguiente abierta
    if (idx == W3dActiveRoot()) {
        int other = -1;
        for (size_t i = 0; i < ab.size() && other < 0; i++) if (ab[i] != idx) other = ab[i];
        if (!W3dActivateRoot(other, reason)) return false;
    }
    // con cambios sin guardar: el cartel (guardar y cerrar / cerrar sin guardar / cancelar) lo termina
    if (W3dChangesAskCloseScene(name)) return true;
    UnloadClosed(name);
    return true;
}
struct RootsHookClose { RootsHookClose() { W3dChangesCloseSceneHook = UnloadClosed; } };
static RootsHookClose gRootsHookClose;
// el objeto de la raiz de una escena abierta pasa a ser la activa (un click en el outliner o en el 3D sobre algo suyo)
bool W3dActivateRootOf(Object* root) {
    if (!root || root == SceneCollection) return true;
    const std::vector<W3dRootRow>& fs = W3dRoots();
    for (size_t i = 0; i < fs.size(); i++)
        if (fs[i].root == root) {
            std::string reason;
            if (W3dActivateRoot((int)i, &reason)) return true;
            if (!reason.empty()) Notify(T(reason.c_str()), true);
            return false;
        }
    return false;
}

bool W3dActivateRoot(int idx, std::string* reason) {
    const std::vector<W3dRootRow>& fs = W3dRoots();
    if (idx < 0 || idx >= (int)fs.size()) { if (reason) *reason = "It doesn't exist anymore"; return false; }
    if (idx == W3dActiveRoot()) return true;
    // ENTRE ESCENAS ABIERTAS (varias a la vez en el editor): el historial sigue (cada paso de undo sabe de que raiz es
    // y la activa antes de aplicarse: Undo.cpp) y el outliner no se mueve. Con un prefab de por medio (se edita
    // aislado) es como siempre: historial limpio
    // (la que no estaba abierta -el selector, "New Scene"- REEMPLAZA a la activa en la lista: la otra sale de la lista y
    //  queda en memoria, como siempre; ahi el historial se limpia)
    const int oldAct = W3dActiveRoot();
    const bool alreadyOpen = W3dRootIsOpen(idx);
    const bool oldEs3D     = oldAct >= 0 && oldAct < (int)fs.size() && W3dRootEs3D(fs[(size_t)oldAct].type);
    const bool betweenOpen = alreadyOpen && W3dRootEs3D(fs[(size_t)idx].type) && oldEs3D;
    int posInList = -1;
    if (!alreadyOpen && oldEs3D) {
        std::vector<int> ab;
        W3dOpenRoots(ab);
        for (size_t i = 0; i < ab.size(); i++) if (ab[i] == oldAct) posInList = (int)i;
    }
    // PRECONDICIONES: el Play tiene su snapshot de ESTA raiz (el Stop la restaura) y un transform en
    // curso tiene los objetos seleccionados en objectState
    if (SimActive()) { if (reason) *reason = "Stop the game before changing the scene"; return false; }
    if (state != editNavigation) { if (reason) *reason = "Finish the current transform first"; return false; }
    // salir de lo que apunta a objetos de la raiz que se deja: un popup abierto, la edicion de una
    // curva, Edit/Pose/Paint (la malla que sale de edicion se PUBLICA en su recurso)
    if (PopUpActive) PopUpActive->Close();
    // un JUEGO se deja SIEMPRE en su estado base ("Juego"): si se estaba editando uno de sus clips,
    // los objetos vuelven a su frame 1 (la foto de la base es de ESTA raiz), y una escena que se
    // volvio juego al ponerle el primer script queda en "Juego". Al volver, eso es lo que se ve.
    if (W3dGameBaseHas()) W3dGameBaseRestore();
    W3dGameBaseForget();
    if (W3dActiveRootIsGame()) W3dRootModeByType(true);
    PlayAnimation = false;   // (reproducir la animacion de una no sigue en la otra)
    W3dCurveEditClose();
    if (InteractionMode != ObjectMode) LayoutModePick(ObjectMode);
    InteractionMode = ObjectMode;
    UpdateActiveEditMesh();
    W3dMeshesTickEditor();
    // el Mix de escenas movio objetos de ESTA raiz: vuelven a su base (sus capas quedan con ella)
    if (g_animMix) W3dMixScenesRelease();
    ReleaseUIOfRoot();
    // UNDO LIMPIO (si no es entre escenas abiertas): el historial nombra objetos de la raiz que se deja (validar contra
    // la otra convertiria sus pasos en no-ops o, peor, re-insertaria un borrado en la raiz equivocada)
    if (!betweenOpen) {
        UndoClear();
        // (sin historial ya no hay pasos que nombren una vista por su indice: las HUERFANAS -la vista de un clip
        //  que se borro- se pueden sacar de la lista)
        W3dClipsViewsPurge();
    }
    const std::string prev = (W3dActiveRoot() >= 0) ? fs[(size_t)W3dActiveRoot()].name : std::string();
    const bool wasPrefab = (W3dActiveRoot() >= 0 && fs[(size_t)W3dActiveRoot()].type == W3D_ROOT_PREFAB);
    if (!W3dRootChangeActive(idx)) { if (reason) *reason = "It could not be loaded"; return false; }
    if (!betweenOpen) W3dClipsViewsPurge();
    if (posInList >= 0) W3dRootRemoveFromOpen(oldAct);
    W3dRootOpen(idx);   // (una escena que se activa queda abierta)
    if (posInList >= 0) W3dRootMoveOpen(idx, posInList);
    W3dMeshesTickEditor();   // (la activa que se miraba era de la otra raiz: se suelta)
    // se DEJO DE EDITAR un prefab: sus instancias (de todas las raices cargadas) se regeneran con lo que quedo. Una
    // ESCENA tambien: se puede instanciar en otra ("escena:<nombre>", io/Prefabs.h)
    if (wasPrefab) W3dPrefabStoppedEditing(prev);
    else if (!prev.empty()) W3dPrefabStoppedEditing(W3dPrefabKeyScene(prev));
    RefreshUI(betweenOpen);
    w3dLogf("[raices] editando %s '%s' (antes '%s')", W3dRootTypeKey(W3dRootTypeOf(idx)),
            fs[(size_t)idx].name.c_str(), prev.c_str());
    return true;
}

// ---- convertir escena <-> juego (con undo: el paso vuelve al tipo anterior) ----
struct UndoRootType { std::string name; int other; };
static void UndoRootTypeApply(void* d) {
    UndoRootType* u = (UndoRootType*)d;
    const int i = W3dRootFind(W3D_ROOT_SCENE, u->name);
    if (i < 0) return;
    const int current = W3dRootTypeOf(i);
    if (SimActive() || !W3dRootSetType(i, u->other)) return;
    u->other = current;
    W3dResourcesViewInvalidate();
    g_redraw = true;
}
static void UndoRootTypeFree(void* d) { delete (UndoRootType*)d; }

bool W3dRootConvertActive(int type, std::string* reason) {
    const int act = W3dActiveRoot();
    if (act < 0) { if (reason) *reason = "It doesn't exist anymore"; return false; }
    if (SimActive()) { if (reason) *reason = "Stop the game first"; return false; }
    const int prev = W3dRootTypeOf(act);
    if (!W3dRootEs3D(prev) || !W3dRootEs3D(type)) { if (reason) *reason = "A prefab can't be converted"; return false; }
    if (prev == type) return true;
    // un juego editando un clip vuelve antes a su base (lo que queda es su frame 1)
    if (W3dGameBaseHas()) W3dGameBaseRestore();
    W3dGameBaseForget();
    PlayAnimation = false;
    if (!W3dRootSetType(act, type)) { if (reason) *reason = "It can't be converted"; return false; }
    UndoRootType* u = new UndoRootType();
    u->name = W3dRoots()[(size_t)act].name; u->other = prev;
    UndoExternal f; f.apply = UndoRootTypeApply; f.free = UndoRootTypeFree;
    UndoPushExternal(f, u);
    W3dResourcesViewInvalidate();
    w3dLogf("[raices] '%s' ahora es %s", u->name.c_str(), type == W3D_ROOT_GAME ? "un juego" : "una escena");
    g_redraw = true;
    return true;
}

int W3dRootCreateAndOpen(int type, const std::string& name) {
    const int idx = W3dNewRoot(type, name.empty() ? std::string(W3dRootDefaultName(type)) : name);
    std::string reason;
    if (!W3dActivateRoot(idx, &reason)) {
        Notify(std::string(T(reason.c_str())), true);
        W3dResourcesViewInvalidate();
        return -1;
    }
    return idx;
}

// ============================================================================
//  "CONVERT TO GAME": LA ESCENA SE DUPLICA COMO UN JUEGO NUEVO (ver EditorRoots.h)
//  El juego arranca con lo que se ve en el cuadro actual y sin las animaciones de la escena. La copia sale del
//  lector de siempre: la raiz escrita EN SECO (io/SaveW3D.cpp, sin el reposo del guardado: la pose del cuadro
//  actual es lo que se copia) y leida en la raiz nueva (importers/import_w3d.cpp). Nombres, scripts, fisica, hitbox,
//  constraints, instancias de prefab... viajan como en un guardado; las mallas, los materiales y las bibliotecas de
//  clips de jerarquia son los MISMOS recursos (van por su nombre). Las animaciones de escena (keyframes, cortes de
//  camara, tiras de esqueleto, capas del Mix) no se escriben: el juego nace con la "Scene" vacia de siempre.
//  Lo que el JSON en seco no lleva se toma del ORIGINAL (por el "_origen" de cada nodo):
//    - los clips de un esqueleto y las vertex anims de una malla: COMPARTIDOS, como entre las instancias de un prefab
//      (W3dPrefabFixHook: los mismos clips en memoria, los mismos frames con copia al escribir);
//    - los scripts tal cual (el orden de sus valores y cual esta elegido en el panel), como en Shift+D;
//    - lo que la animacion le puso en ESE cuadro y el archivo no guarda: los pesos de las formas de una malla y el
//      clip + cabezal de un esqueleto (el de su tira en la animacion de escena, o el clip que se edita en el
//      timeline: en "Juego" cada esqueleto muestra su clip activo desde su cabezal);
//    - la camara activa (la del corte de ese cuadro) y la coleccion activa.
// ============================================================================
extern bool W3dRootSerializeDry(Object* root, std::string& json);   // io/SaveW3D.cpp
extern bool W3dRootReadCopyDry(const std::string& json,              // importers/import_w3d.cpp
                                 std::vector<std::pair<Object*, unsigned> >* origins);
extern int g_undoNoRoot;   // undo/Undo.cpp: el paso que se apila no es de la raiz activa

namespace {
// el clip y el cabezal que muestra un esqueleto en el cuadro actual (los casos de EvaluatePoseSkeleton fuera de
// "Juego"). false = se ve en reposo, o lo posan sus capas del Mix (esas viajan con el: el juego tambien las usa)
bool ClipInScreen(Armature* a, int& clip, float& playhead) {
    if (!a->layers.empty()) return false;
    int c = -1, f = CurrentFrame;
    if (ActiveAnimKind == 1 && ActiveAnimArm == a) c = a->activeAnim;   // el clip que se edita en el timeline
    else if (ActiveAnimKind == 0 && ActiveSceneAnim >= 0 && ActiveSceneAnim < (int)SceneAnimations.size()) {
        // su TIRA en la animacion de escena activa: el frame 'frame' de la escena es el primero del clip
        const SceneAnimation* e = SceneAnimations[(size_t)ActiveSceneAnim];
        if (e && !e->isClip)
            for (size_t t = 0; t < e->skeletons.size(); t++) {
                if (e->skeletons[t].armature != a->name) continue;
                c = W3dArmatureClipByName(a, e->skeletons[t].clip);
                if (c >= 0) f = a->animations[(size_t)c]->startFrame + (CurrentFrame - e->skeletons[t].frame);
                break;
            }
    }
    if (c < 0 || c >= (int)a->animations.size() || !a->animations[(size_t)c]) return false;
    const SkeletalAnimation* cl = a->animations[(size_t)c];
    if (f > cl->endFrame) f = cl->endFrame;
    if (f < cl->startFrame) f = cl->startFrame;
    clip = c;
    playhead = (float)(f - cl->startFrame);
    return true;
}
struct PoseSkeleton { int clip; float playhead; };
void CollectPoses(Object* o, std::map<unsigned, PoseSkeleton>& out) {
    if (!o) return;
    if (o->getType() == ObjectType::armature) {
        PoseSkeleton p;
        if (ClipInScreen((Armature*)o, p.clip, p.playhead)) out[o->serial] = p;
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectPoses(o->Childrens[i], out);
}
void MapSerials(Object* o, std::map<unsigned, Object*>& out) {
    if (!o) return;
    out[o->serial] = o;
    for (size_t i = 0; i < o->Childrens.size(); i++) MapSerials(o->Childrens[i], out);
}
// lo que el JSON en seco no lleva, de 'o' (el original) a 'g' (su copia)
void CompleteCopy(Object* g, Object* o, const std::map<unsigned, PoseSkeleton>& poses) {
    if (!g || !o || g->getType() != o->getType()) return;
    if (W3dPrefabFixHook) W3dPrefabFixHook(g, o);   // clips de esqueleto y vertex anims, compartidos
    if (o->scriptData) { delete g->scriptData; g->scriptData = new W3dScriptData(*o->scriptData); }
    if (o->getType() == ObjectType::mesh) {
        Mesh* gm = (Mesh*)g;
        const Mesh* om = (const Mesh*)o;
        for (size_t k = 0; k < om->shapeWeights.size(); k++) gm->SetShapeWeight((int)k, om->shapeWeights[k]);
    } else if (o->getType() == ObjectType::armature) {
        std::map<unsigned, PoseSkeleton>::const_iterator p = poses.find(o->serial);
        Armature* ga = (Armature*)g;
        if (p != poses.end() && p->second.clip < (int)ga->animations.size()) {
            ga->activeAnim = p->second.clip;
            ga->gameFrame  = p->second.playhead;
            ga->gameEnded = false;
        }
    }
}
}

// el paso de undo de "Convert to Game": Ctrl+Z vuelve a la escena y SACA el juego (su arbol queda en el paso, como en
// un borrado de la vista Scenes); Ctrl+Y lo devuelve y lo vuelve a abrir. No es de ninguna raiz (g_undoNoRoot): con
// el juego afuera el undo no podria activarlo antes de aplicar el paso, el paso cambia de raiz solo. (Pasar a una
// escena que no estaba abierta vacia el historial, como siempre: el paso sobrevive porque en ese momento no esta en
// ninguna pila -el undo lo saco para aplicarlo- y queda solo)
struct UndoToGame { std::string scene, game; bool outside; W3dRemovedRoot s; };
static void UndoToGameApply(void* d) {
    UndoToGame* u = (UndoToGame*)d;
    std::string reason;
    if (!u->outside) {
        const int ie = W3dRootFind(W3D_ROOT_SCENE, u->scene);
        const int ij = W3dRootFind(W3D_ROOT_GAME, u->game);
        if (ie < 0 || ij < 0 || ie == ij) return;
        if (W3dActiveRoot() == ij && !W3dActivateRoot(ie, &reason)) {
            if (!reason.empty()) Notify(std::string(T(reason.c_str())), true);
            return;
        }
        // (activar la escena pudo correr los indices: se busca de nuevo)
        const int j = W3dRootFind(W3D_ROOT_GAME, u->game);
        W3dRootRemoveFromOpen(j);
        if (!W3dRootRemove(j, &u->s, &reason)) {
            if (!reason.empty()) Notify(std::string(T(reason.c_str())), true);
            return;
        }
        u->outside = true;
    } else {
        Object* root = u->s.row.root;
        if (!W3dRootReturn(u->s)) return;
        u->outside = false;
        const int ij = W3dObjectRoot(root);
        if (ij < 0) return;
        u->game = W3dRoots()[(size_t)ij].name;   // (el nombre con el que quedo: el Ctrl+Z lo busca por el)
        if (!W3dActivateRoot(ij, &reason) && !reason.empty()) Notify(std::string(T(reason.c_str())), true);
    }
    W3dResourcesViewInvalidate();
    g_redraw = true;
}
static void UndoToGameFree(void* d) {
    UndoToGame* u = (UndoToGame*)d;
    if (u->outside) W3dRemovedRootFree(u->s);
    delete u;
}

int W3dRootConvertInGame(std::string* reason) {
    const int act = W3dActiveRoot();
    if (act < 0 || act >= (int)W3dRoots().size() || !SceneCollection) { if (reason) *reason = "It doesn't exist anymore"; return -1; }
    if (W3dRootTypeOf(act) != W3D_ROOT_SCENE) { if (reason) *reason = "It can't be converted"; return -1; }
    if (SimActive()) { if (reason) *reason = "Stop the game first"; return -1; }
    if (state != editNavigation) { if (reason) *reason = "Finish the current transform first"; return -1; }
    // lo que esta a medio editar se cierra ANTES de la foto, como al dejar la raiz (W3dActivateRoot): la malla que sale
    // de Edit Mode se publica en su recurso, la vista de un clip de jerarquia se escribe en su clip, y toda malla
    // tiene su recurso (la copia la nombra por el)
    PlayAnimation = false;
    W3dCurveEditClose();
    if (InteractionMode != ObjectMode) LayoutModePick(ObjectMode);
    InteractionMode = ObjectMode;
    UpdateActiveEditMesh();
    W3dMeshesTickEditor();
    W3dMeshesEnsureResources();
    W3dClipsViewsSync();
    // LA FOTO DEL CUADRO ACTUAL (con la escena todavia activa: su animacion y su timeline son los globales)
    Object* original = SceneCollection;
    std::string json;
    if (!W3dRootSerializeDry(original, json)) { if (reason) *reason = "It can't be converted"; return -1; }
    std::map<unsigned, PoseSkeleton> poses;
    CollectPoses(original, poses);
    const unsigned camSerial   = CameraActive ? ((Object*)CameraActive)->serial : 0u;
    const unsigned colSerial   = (CollectionActive && CollectionActive != SceneCollection) ? CollectionActive->serial : 0u;
    const std::string sceneObj = W3dRoots()[(size_t)act].name;
    const int frame = CurrentFrame;
    // EL JUEGO NUEVO, con el nombre de una copia de la escena ("Intro" -> "Intro.001": escenas y juegos comparten
    // nombres). Se arma como la carga de una raiz (W3dRootLoad): mientras se lo llena es la activa
    const int newIdx = W3dNewRoot(W3D_ROOT_GAME, sceneObj);
    Object* game = W3dRoots()[(size_t)newIdx].root;
    const int prev = W3dActiveRoot();   // (la fila nueva pudo correr los indices)
    const unsigned selSerial = W3dSelectionSerial;   // (armar la copia no es ELEGIR: ver W3dSelectionSerial)
    W3dRootUse(newIdx);
    std::vector<std::pair<Object*, unsigned> > origins;
    const bool read = W3dRootReadCopyDry(json, &origins);
    std::map<unsigned, Object*> orig;
    MapSerials(original, orig);
    for (size_t i = 0; i < origins.size(); i++) {
        std::map<unsigned, Object*>::iterator t = orig.find(origins[i].second);
        if (t == orig.end()) continue;
        Object* g = origins[i].first;
        CompleteCopy(g, t->second, poses);
        if (camSerial && t->first == camSerial && g->getType() == ObjectType::camera) CameraActive = (Camera*)g;
        if (colSerial && t->first == colSerial) CollectionActive = g;
    }
    DeselectAll(); ActiveObj = NULL;   // (los constructores fueron seleccionando)
    W3dRootUse(prev);
    W3dSelectionSerial = selSerial;
    if (!read) {
        W3dRootDelete(W3dObjectRoot(game), NULL);
        if (reason) *reason = "It can't be converted";
        return -1;
    }
    w3dLogf("[raices] la escena '%s' se duplico como el juego '%s' con su cuadro %d (%d objeto(s), sin sus animaciones "
            "de escena; la escena queda como estaba)", sceneObj.c_str(), W3dRoots()[(size_t)W3dObjectRoot(game)].name.c_str(),
            frame, (int)origins.size());
    // y pasa a ser la activa, como un "New Game" (la escena sale de la lista de abiertas y queda en memoria)
    if (!W3dActivateRoot(W3dObjectRoot(game), reason)) { W3dResourcesViewInvalidate(); return -1; }
    // EL PASO DE UNDO, despues de activarlo (pasar a una raiz que no estaba abierta vacio el historial)
    UndoToGame* u = new UndoToGame();
    u->scene = sceneObj;
    u->game    = W3dRoots()[(size_t)W3dObjectRoot(game)].name;
    u->outside = false;
    UndoExternal f; f.apply = UndoToGameApply; f.free = UndoToGameFree;
    g_undoNoRoot++;
    UndoPushExternal(f, u);
    g_undoNoRoot--;
    W3dResourcesViewInvalidate();
    g_redraw = true;
    return W3dObjectRoot(game);
}

// ============================================================================
//  EL SELECTOR
// ============================================================================
// los tres "New ..." (el selector y el "+" del outliner). 'typeIcon': con el icono de lo que crean
// (el "+", cuyo titulo ya dice que se agrega algo) o con el "+" (el selector: ahi el icono de tipo los
// confundiria con las escenas/juegos/prefabs de la lista)
static void AddNew(PopupMenu* m, bool typeIcon) {
    m->Add(T("New Scene"), W3D_ROOT_MENU_NEW_SCENE, typeIcon ? W3dRootIcon(W3D_ROOT_SCENE) : (int)IconType::plus);
    m->Add(T("New Game"), W3D_ROOT_MENU_NEW_GAME, typeIcon ? W3dRootIcon(W3D_ROOT_GAME) : (int)IconType::plus);
    m->Add(T("New Prefab"), W3D_ROOT_MENU_NEW_PREFAB, typeIcon ? W3dRootIcon(W3D_ROOT_PREFAB) : (int)IconType::plus);
}

void W3dRootsMenuBuildNew(PopupMenu* m) {
    if (!m) return;
    m->Clear();
    AddNew(m, true);
}

void W3dRootsMenuBuild(PopupMenu* m) {
    if (!m) return;
    m->Clear();
    const std::vector<W3dRootRow>& fs = W3dRoots();
    const int act   = W3dActiveRoot();
    const int start = W3dInitialRootIdx();
    // escenas y juegos juntos (en el orden del proyecto: comparten nombres y el juego salta entre
    // ellos con cambiarEscena), despues los prefabs; cada uno con el icono de su tipo
    static const int kOrder[2] = { W3D_ROOT_SCENE, W3D_ROOT_PREFAB };
    for (int k = 0; k < 2; k++)
        for (size_t i = 0; i < fs.size(); i++) {
            if (W3dRootClass(fs[i].type) != kOrder[k]) continue;
            const int type = W3dRootTypeOf((int)i);
            MenuItem* it = m->Add(fs[i].name, 1000 + (int)i, W3dRootIcon(type));
            if (!it) continue;
            it->green = ((int)i == act);
            // la escena/juego con la que ARRANCA el juego compilado
            if (W3dRootEs3D(type) && (int)i == start && fs.size() > 1) it->shortcut = T("start");
        }
    AddNew(m, false);
    const int tAct = (act >= 0 && act < (int)fs.size()) ? W3dRootTypeOf(act) : -1;
    // la escena/juego que se edita pasa a ser la que arranca el juego
    if (W3dRootEs3D(tAct) && act != start)
        m->Add(T("Set as Start Scene"), W3D_ROOT_MENU_INITIAL, W3dRootIcon(tAct));
    // una escena se DUPLICA como un juego nuevo (la foto de su cuadro actual, sin sus animaciones de escena: un juego
    // simula y no tiene keyframes propios); un juego vuelve a ser escena en el lugar
    if (tAct == W3D_ROOT_SCENE) m->Add(T("Convert to Game"), W3D_ROOT_MENU_TO_GAME, W3dRootIcon(W3D_ROOT_GAME));
    if (tAct == W3D_ROOT_GAME)  m->Add(T("Convert to Scene"), W3D_ROOT_MENU_TO_SCENE, W3dRootIcon(W3D_ROOT_SCENE));
}

void W3dRootsMenuAction(int id) {
    std::string reason;
    if (id == W3D_ROOT_MENU_NEW_SCENE) { W3dRootCreateAndOpen(W3D_ROOT_SCENE, std::string()); return; }
    if (id == W3D_ROOT_MENU_NEW_GAME)  { W3dRootCreateAndOpen(W3D_ROOT_GAME, std::string()); return; }
    if (id == W3D_ROOT_MENU_NEW_PREFAB) { W3dRootCreateAndOpen(W3D_ROOT_PREFAB, std::string()); return; }
    if (id == W3D_ROOT_MENU_TO_GAME) {
        if (W3dRootConvertInGame(&reason) < 0 && !reason.empty()) Notify(std::string(T(reason.c_str())), true);
        return;
    }
    if (id == W3D_ROOT_MENU_TO_SCENE) {
        if (!W3dRootConvertActive(W3D_ROOT_SCENE, &reason)) Notify(std::string(T(reason.c_str())), true);
        return;
    }
    if (id == W3D_ROOT_MENU_INITIAL) {
        const int act = W3dActiveRoot();
        if (act >= 0 && act < (int)W3dRoots().size()) W3dRootSetInitial(W3dRoots()[(size_t)act].name);
        g_redraw = true;
        return;
    }
    if (id >= 1000 && id < 1000 + (int)W3dRoots().size())
        if (!W3dActivateRoot(id - 1000, &reason)) Notify(std::string(T(reason.c_str())), true);
}

bool W3dRootsButtonSync(Button* b) {
    if (!b) return false;
    const std::vector<W3dRootRow>& fs = W3dRoots();
    const int act = W3dActiveRoot();
    if (act < 0 || act >= (int)fs.size()) return false;
    const int icon = W3dRootIcon(W3dRootTypeOf(act));
    bool change = false;
    // (un '*' si la raiz tiene cambios sin guardar: io/ProjectChanges.h)
    const std::string t = fs[(size_t)act].name + (W3dChangesRootDirty(act) ? "*" : "");
    if (b->text != t) { b->text = t; change = true; }
    if (b->icon != icon) { b->icon = icon; change = true; }
    if (change) g_redraw = true;
    return change;
}

// ============================================================================
//  LOS PROVEEDORES DE LAS VISTAS "Scenes" y "Prefabs" DEL OUTLINER
// ============================================================================
// el paso de UNDO de un renombre (ida y vuelta: aplicar intercambia 'actual' y 'otro')
struct UndoRowRoot { int type; std::string current, other; };
// un PREFAB renombrado: sus instancias lo nombran por nombre. TODAS las raices se cargan antes (una que no se
// abrio lo seguiria nombrando con el viejo y al abrirla la instancia quedaria vacia), igual que al renombrar un
// recurso de la biblioteca
static void RenamedPrefab(int type, const std::string& old, const std::string& newValue) {
    if (type != W3D_ROOT_PREFAB || old == newValue) return;
    const int n = W3dRenamedPrefab(old, newValue);
    if (n) w3dLogf("[prefabs] %d instancia(s) siguen al prefab '%s' -> '%s'", n, old.c_str(), newValue.c_str());
}
static void UndoRowRootApply(void* d) {
    UndoRowRoot* u = (UndoRowRoot*)d;
    const int i = W3dRootFind(u->type, u->current);
    std::string remained;
    if (u->type == W3D_ROOT_PREFAB) W3dRootsLoadAll();
    if (i < 0 || !W3dRootRename(i, u->other, &remained)) return;
    RenamedPrefab(u->type, u->current, remained);
    W3dActiveRenamedResource(u->type == W3D_ROOT_PREFAB ? W3D_VIEW_PREFABS : W3D_VIEW_SCENES, u->current, remained);
    u->other = u->current;
    u->current = remained;
    g_redraw = true;
}
static void UndoRowRootFree(void* d) { delete (UndoRowRoot*)d; }
// el paso de undo de un BORRADO (W3dRootRemove): Ctrl+Z la devuelve con su arbol y su contexto, Ctrl+Y la vuelve a
// sacar. Mientras esta afuera su arbol es del paso (se libera si el paso se cae del historial).
struct UndoDeleteRoot { int type; std::string name; bool outside; W3dRemovedRoot s; };
static void UndoDeleteRootApply(void* d) {
    UndoDeleteRoot* u = (UndoDeleteRoot*)d;
    if (u->outside) {
        Object* root = u->s.row.root;
        if (!W3dRootReturn(u->s)) return;
        u->outside = false;
        // el nombre con el que quedo (el Ctrl+Y la busca por el)
        const int i = W3dObjectRoot(root);
        if (i >= 0) u->name = W3dRoots()[(size_t)i].name;
    } else {
        std::string reason;
        if (!W3dRootRemove(W3dRootFind(u->type, u->name), &u->s, &reason)) return;
        u->outside = true;
    }
    W3dResourcesViewInvalidate();
    g_redraw = true;
}
static void UndoDeleteRootFree(void* d) {
    UndoDeleteRoot* u = (UndoDeleteRoot*)d;
    if (u->outside) W3dRemovedRootFree(u->s);
    delete u;
}

class RootProvider : public W3dResourceProvider {
public:
    int type;
    explicit RootProvider(int t) : type(t) {}
    void List(std::vector<W3dResourceItem>& out) {
        out.clear();
        const std::vector<W3dRootRow>& fs = W3dRoots();
        const int act = W3dActiveRoot();
        for (size_t i = 0; i < fs.size(); i++) {
            if (W3dRootClass(fs[i].type) != type) continue;   // (la vista Scenes lista escenas Y juegos)
            const int t = W3dRootTypeOf((int)i);
            W3dResourceItem it;
            it.id = it.name = fs[i].name;
            it.folder = fs[i].folder;
            it.entry  = fs[i].entry;
            it.icon = W3dRootIcon(t);
            // los usuarios de un prefab son sus INSTANCIAS (en las raices cargadas); una escena no tiene
            it.users    = (t == W3D_ROOT_PREFAB) ? W3dPrefabCountInstances(fs[i].name) : 0;
            const int n = W3dRootObjects((int)i);
            char b[32]; snprintf(b, sizeof(b), "%d ", n);
            it.info = std::string(T(t == W3D_ROOT_GAME ? "game" : t == W3D_ROOT_PREFAB ? "prefab" : "scene")) + ", ";
            it.info += (n < 0) ? std::string(T("not loaded"))
                               : std::string(b) + T(n == 1 ? "object" : "objects");
            if ((int)i == act) it.info += std::string(", ") + T("active");
            if (W3dRootEs3D(t) && (int)i == W3dInitialRootIdx() && fs.size() > 1)
                it.info += std::string(", ") + T("start");
            // la CASILLA: abierta en el editor (cargada, a la vista y en el outliner). Los prefabs no (se editan solos)
            if (W3dRootEs3D(t)) it.check = W3dRootIsOpen((int)i) ? 1 : 0;
            it.loaded = (fs[i].root != NULL);
            out.push_back(it);
        }
    }
    bool SetFolder(const std::string& id, const std::string& folder) {
        return W3dRootSetFolder(W3dRootFind(type, id), folder);
    }
    std::string IdRequest(const std::string& id, const std::string& newValue) {
        (void)id; return W3dNameNormalize(newValue, W3dRootDefaultName(type));
    }
    bool Rename(const std::string& id, const std::string& newValue, std::string* final) {
        const int i = W3dRootFind(type, id);
        std::string remained;
        if (i >= 0 && type == W3D_ROOT_PREFAB) W3dRootsLoadAll();   // (ver RenamedPrefab)
        if (i < 0 || !W3dRootRename(i, newValue, &remained)) return false;
        RenamedPrefab(type, id, remained);
        if (final) *final = remained;
        if (remained != id) {
            UndoRowRoot* u = new UndoRowRoot();
            u->type = type; u->current = remained; u->other = id;
            UndoExternal f; f.apply = UndoRowRootApply; f.free = UndoRowRootFree;
            UndoPushExternal(f, u);
            g_redraw = true;
        }
        return true;
    }
    // con Ctrl+Z (W3dRootRemove: su arbol queda en el paso de undo)
    bool Delete(const std::string& id, std::string* reason) {
        if (SimActive()) { if (reason) *reason = "Stop the game first"; return false; }
        UndoDeleteRoot* u = new UndoDeleteRoot();
        u->type = type; u->name = id; u->outside = false;
        if (!W3dRootRemove(W3dRootFind(type, id), &u->s, reason)) { delete u; return false; }
        u->outside = true;
        if (type == W3D_ROOT_PREFAB) W3dPrefabInvalidate(id);   // (sus instancias quedan con lo que ya generaron)
        UndoExternal f; f.apply = UndoDeleteRootApply; f.free = UndoDeleteRootFree;
        UndoPushExternal(f, u);
        W3dResourcesViewInvalidate();
        g_redraw = true;
        return true;
    }
    // un PREFAB EN USO se borra IGUAL si el usuario lo confirma (el cartel dice cuantas instancias lo usan): sus
    // instancias quedan SIN el -conservan lo que ya generaron hasta que se regeneren; guardadas, abren vacias con
    // su cruz y sus overrides- y el Ctrl+Z lo devuelve. Es el mismo borrado (con su paso de undo)
    bool DeleteInUse(const std::string& id, std::string* reason) {
        if (type != W3D_ROOT_PREFAB) { if (reason) *reason = "It is in use"; return false; }
        return Delete(id, reason);
    }
    bool KnowsDeleteInUse() const { return type == W3D_ROOT_PREFAB; }
    // los USUARIOS de un prefab son sus instancias (las de las raices cargadas; "Select Users" elige las de la
    // raiz activa)
    void Users(const std::string& id, std::vector<Object*>& out) {
        out.clear();
        if (type != W3D_ROOT_PREFAB) return;
        std::vector<InstancePrefab*> v;
        W3dPrefabInstances(id, v);
        for (size_t i = 0; i < v.size(); i++) out.push_back((Object*)v[i]);
    }
    bool Purgeable() const { return false; }
    // tildar = abrirla en el editor; destildar = cerrarla y sacarla de la memoria (ver W3dRootCheck)
    bool Check(const std::string& id, bool on, std::string* reason) {
        if (type != W3D_ROOT_SCENE) { if (reason) *reason = "It has no checkbox"; return false; }
        return W3dRootCheck(W3dRootFind(type, id), on, reason);
    }
};

static RootProvider gProvScenes(W3D_ROOT_SCENE);
static RootProvider gProvPrefabs(W3D_ROOT_PREFAB);
namespace {
struct RegisterProviders {
    RegisterProviders() {
        W3dResourcesViewRegister(W3D_VIEW_SCENES, &gProvScenes);
        W3dResourcesViewRegister(W3D_VIEW_PREFABS, &gProvPrefabs);
    }
} gRegisterProviders;
}
