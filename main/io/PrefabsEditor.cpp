// ============================================================================
//  PrefabsEditor.cpp — ver PrefabsEditor.h. El editor sobre las instancias de
//  prefab: lo generado no se toca, Add > Prefab, soltar un prefab, cambiar el
//  prefab de una instancia, "Create Prefab", "Unpack" y la regeneracion al volver
//  de editar un prefab. Solo editor (C++03).
// ============================================================================
#include "io/PrefabsEditor.h"
#include "io/Prefabs.h"
#include "io/EditorRoots.h"              // W3dActivateRoot
#include "io/ProjectResources.h"         // W3dResourcesViewInvalidate (los usuarios de un prefab cambian)
#include "io/ProjectChanges.h"           // la raiz que cambio queda sucia
#include "W3dRoots.h"
#include "objects/InstancePrefab.h"
#include "objects/ProxyW3d.h"           // los PROXIES de las librerias: se crean y se cambian igual que una instancia
#include "io/Libraries.h"
#include "io/Streaming.h"                // COMO SE CARGA una instancia (la tarjeta, con Ctrl+Z)
#include "objects/Objects.h"
#include "objects/ObjectMode.h"          // ReparentKeepTransform / MoveNextTo / ScaleGlobalOf
#include "objects/Mesh.h"
#include "objects/MeshResource.h"   // MeshResource::libreria (una malla de una libreria: solo lectura)
#include "objects/Armature.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "animation/Animation.h"
#include "animation/VertexAnimation.h"   // los frames de las vertex anims de un prefab sin guardar
#include "animation/W3dAnimSet.h"        // los clips compartidos de un armature de un prefab sin guardar
#include "base/W3dInteractionState.h"
#include "undo/Undo.h"
#include "ViewPorts/Notifications.h"
#include "ViewPorts/Properties.h"        // PropsForgetScene
#include "config/W3dLang.h"
#include "render/RenderOptions.h"       // g_redraw
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"
#include "variables.h"                   // cursor3D
#include "W3dScene.h"                 // W3dSceneFindRef: como resuelve una ref de script (Unpack)
#include "script/W3dScript.h"         // W3dScriptEntry::refs (Unpack)
#include "w3dlog.h"
#include <cstdio>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

extern bool SimActive();
extern void DopeReleaseVertexAnim();   // (ViewPorts/Timeline.h) la anim activa cambio: soltar las filas de vertex anim

bool g_w3dPrefabUnpacking = false;

// ============================================================================
//  LO QUE LA SERIALIZACION EN SECO NO LLEVA (io/Prefabs.h: W3dPrefabFixHook)
// ============================================================================
static void FixGenerated(Object* gen, Object* tpl) {
    if (!gen || !tpl) return;
    if (gen->getType() == ObjectType::armature && tpl->getType() == ObjectType::armature) {
        Armature* ga = (Armature*)gen;
        Armature* ta = (Armature*)tpl;
        // los MISMOS clips en memoria que el armature del prefab (como un Alt+D): 50 instancias = 1 animset
        if (!ta->animations.empty() && ga->animations.empty()) {
            W3dArmatureAnimsLink(ga, ta);
            ga->activeAnim = (ta->activeAnim >= 0 && ta->activeAnim < (int)ga->animations.size()) ? ta->activeAnim : -1;
        }
    } else if (gen->getType() == ObjectType::mesh && tpl->getType() == ObjectType::mesh) {
        Mesh* gm = (Mesh*)gen;
        Mesh* tm = (Mesh*)tpl;
        // las VERTEX ANIMS (sus frames no tienen entrada hasta el guardado): los MISMOS frames que la plantilla
        // (VertexAnimShare: pasan al almacen y cada instancia toma una ref; editar la plantilla le hace su
        // copia -COW- y las instancias se regeneran al volver). 50 instancias = 1 juego de frames
        if (gm->animations.empty())
            for (size_t i = 0; i < tm->animations.size(); i++)
                if (tm->animations[i]) NewActiveVertexAnimation(gm, VertexAnimShare(tm->animations[i], gm));
    }
}
namespace {
struct RegisterArray { RegisterArray() { W3dPrefabFixHook = FixGenerated; } } gRegisterArray;
}

// ============================================================================
//  LO GENERADO NO SE TOCA
// ============================================================================
bool W3dPrefabIsGeneratedNotice(Object* o, bool notify) {
    if (!o || !W3dIsGenerated(o)) return false;
    if (notify) Notify(std::string(T("It belongs to a prefab instance: edit the prefab or unpack the instance")), true);
    return true;
}

// ---- lo que es de una LIBRERIA externa (solo lectura) ----
bool W3dGeneratedByProxy(const Object* o) {
    for (Object* p = W3dInstanceOf(o); p; p = W3dInstanceOf(p))
        if (p->getType() == ObjectType::proxy) return true;
    return false;
}
bool W3dMeshReadOnly(const Mesh* m, bool resource) {
    if (!m) return false;
    if (resource && m->mesh && !m->mesh->lib.empty()) return true;
    return W3dGeneratedByProxy(m);
}
bool W3dReadOnlyMeshNotice(const Mesh* m, bool resource) {
    if (!W3dMeshReadOnly(m, resource)) return false;
    Notify(std::string(T("It belongs to a library (read-only): open the library to edit it")), true);
    w3dLogfW("[librerias] '%s' es de una libreria (solo lectura): no se edita", m->name.c_str());
    return true;
}

bool W3dPrefabCrossingValid(Object* obj, Object* newParent) {
    if (!obj || !newParent) return true;
    if (g_w3dPrefabUnpacking) return true;
    const bool sale = W3dIsGenerated(obj);
    const bool enters = W3dIsInstanceType(newParent->getType()) || W3dIsGenerated(newParent);
    if (!sale && !enters) return true;
    Notify(std::string(T(sale ? "It belongs to a prefab instance: edit the prefab or unpack the instance"
                                 : "A prefab instance only has what its prefab generates")), true);
    w3dLogfW("[prefabs] reparent bloqueado: '%s' -> '%s' (%s)", obj->name.c_str(), newParent->name.c_str(),
             sale ? "es generado por una instancia" : "adentro de una instancia");
    return false;
}

// ============================================================================
//  CREAR / CAMBIAR
// ============================================================================
// el prefab que se esta EDITANDO (la raiz activa es un prefab): "" = ninguno
static std::string PrefabEdited() {
    const int a = W3dActiveRoot();
    if (a < 0 || a >= (int)W3dRoots().size()) return std::string();
    const W3dRootRow& f = W3dRoots()[(size_t)a];
    return f.type == W3D_ROOT_PREFAB ? f.name : std::string();
}
// la CLAVE de definicion de lo que se edita: el prefab, o la escena ("escena:<nombre>": tambien se instancia)
static std::string KeyEdited() {
    const int a = W3dActiveRoot();
    if (a < 0 || a >= (int)W3dRoots().size()) return std::string();
    const W3dRootRow& f = W3dRoots()[(size_t)a];
    return f.type == W3D_ROOT_PREFAB ? f.name : W3dPrefabKeyScene(f.name);
}
// meter una instancia de 'nombre' en lo que se edita armaria un ciclo?
static bool SerialCycle(const std::string& name) {
    const std::string ed = KeyEdited();
    return !ed.empty() && W3dPrefabContains(name, ed);
}
// la definicion existe? (un prefab del registro o una escena del proyecto)
static bool ExistsDefinition(const std::string& key) {
    std::string esc;
    if (W3dPrefabIsScene(key, &esc)) return W3dRootFind(W3D_ROOT_SCENE, esc) >= 0;
    return W3dRootFind(W3D_ROOT_PREFAB, key) >= 0;
}

bool W3dPrefabCanAdd(const std::string& name, std::string* reason) {
    const bool esc = W3dPrefabIsScene(name);
    if (!ExistsDefinition(name)) { if (reason) *reason = esc ? "There is no such scene" : "There is no such prefab"; return false; }
    if (SerialCycle(name)) { if (reason) *reason = esc ? "A scene can't contain itself" : "A prefab can't contain itself"; return false; }
    if (InteractionMode != ObjectMode || state != editNavigation) { if (reason) *reason = "Only in Object Mode"; return false; }
    return true;
}

InstancePrefab* W3dPrefabAdd(const std::string& name, const Vector3& pos, std::string* reason) {
    if (!W3dPrefabCanAdd(name, reason)) return NULL;
    // editando un PREFAB, la instancia va ADENTRO de su objeto raiz (un prefab anidado: es parte de lo que el
    // prefab genera); suelta al lado no la generaria nadie
    Object* parent = NULL;
    Vector3 local  = pos;
    if (!PrefabEdited().empty() && SceneCollection && !SceneCollection->Childrens.empty()) {
        parent = SceneCollection->Childrens[0];
        Matrix4 w; parent->GetWorldMatrixBase(w);
        local = w.Inverse() * pos;
    }
    InstancePrefab* ip = W3dPrefabCreateInstance(name, parent, local, 0.0f);
    if (!ip) { if (reason) *reason = "There is no such prefab"; return NULL; }
    DeselectAll();
    ip->Select();
    UndoCaptureCreation();   // Ctrl+Z la saca con lo que genero (Ctrl+Y la devuelve)
    W3dResourcesViewInvalidate();   // (el prefab tiene un usuario mas)
    g_redraw = true;
    w3dLogf("[prefabs] instancia '%s' de '%s' en (%.2f, %.2f, %.2f)", ip->name.c_str(), name.c_str(), pos.x, pos.y, pos.z);
    return ip;
}

// ============================================================================
//  REGENERAR UNA INSTANCIA CON Ctrl+Z (cambiarle el prefab, "Reset Overrides")
//
//  Regenerar LIBERA lo que la instancia genero, y el historial puede tener pasos que lo nombran por puntero
//  (mover o elegir un hijo generado, ocultarlo con H): el Ctrl+Z escribia despues en memoria liberada. Por
//  eso ACA lo generado NO se libera: el paso se queda con la generacion vieja DESCOLGADA (como un borrado,
//  que tampoco libera) y su Ctrl+Z la vuelve a colgar, intercambiandola con la nueva, que queda en el paso.
//  Todo objeto que nombra un paso del historial sigue vivo mientras el paso exista y, por LIFO, esta colgado
//  cuando ese paso se aplica: los de abajo nombran la generacion vieja (colgada al deshacer esto) y los de
//  arriba la nueva (colgada al rehacerlo). Lo descolgado se libera cuando el paso se cae del historial.
// ============================================================================
static void CollectSub(Object* o, std::vector<Object*>& out);   // (el subarbol en preorden: mas abajo)
static InstancePrefab* InstanceBySerial(Object* o, unsigned serial) {
    if (!o) return NULL;
    if (o->serial == serial && W3dIsInstanceType(o->getType())) return (InstancePrefab*)o;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        InstancePrefab* r = InstanceBySerial(o->Childrens[i], serial);
        if (r) return r;
    }
    return NULL;
}
static void RemoveCurvesOf(std::vector<AnimationObject>& v, const std::set<Object*>& s) {
    for (size_t i = v.size(); i-- > 0; ) if (s.count(v[i].obj)) v.erase(v.begin() + (long)i);
}
// las LUCES de un subarbol dejan de ser (o vuelven a ser) de la raiz activa: descolgadas no iluminan
static void LightsOutside(Object* o) {
    if (o->getType() == ObjectType::light)
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == (Light*)o) { Lights.erase(Lights.begin() + (long)i); break; }
    for (size_t i = 0; i < o->Childrens.size(); i++) LightsOutside(o->Childrens[i]);
}
static void LightsInside(Object* o) {
    if (o->getType() == ObjectType::light) {
        bool already = false;
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == (Light*)o) { already = true; break; }
        if (!already) Lights.push_back((Light*)o);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) LightsInside(o->Childrens[i]);
}
// DESCUELGA lo que genero la instancia SIN liberarlo: 'fuera' se queda con sus hijos. Lo que los nombra por
// puntero desde afuera del arbol (la seleccion, el activo, la camara y la animacion activas, las curvas de las
// animaciones de escena -las de lo generado no se guardan-, las luces de la raiz) se suelta ahora
static void UnhookGenerated(InstancePrefab* ip, std::vector<Object*>& outside) {
    outside.clear();
    if (ip->Childrens.empty()) return;
    W3dClipsViewsSync();   // (la vista de un clip sobre lo generado se escribe en su clip antes de irse)
    std::vector<Object*> sub;
    for (size_t i = 0; i < ip->Childrens.size(); i++) CollectSub(ip->Childrens[i], sub);
    std::set<Object*> all(sub.begin(), sub.end());
    for (size_t i = ObjSelects.size(); i-- > 0; )
        if (all.count(ObjSelects[i])) { ObjSelects[i]->select = false; ObjSelects.erase(ObjSelects.begin() + (long)i); }
    for (size_t i = 0; i < sub.size(); i++) sub[i]->select = false;
    if (all.count(ActiveObj)) ActiveObj = NULL;
    if (all.count(CollectionActive)) CollectionActive = SceneCollection;
    if (CameraActive && all.count((Object*)CameraActive)) CameraActive = NULL;
    if ((ActiveAnimArm && all.count((Object*)ActiveAnimArm)) || (ActiveAnimMesh && all.count((Object*)ActiveAnimMesh))) {
        ActiveAnimArm = NULL; ActiveAnimMesh = NULL; ActiveAnimKind = 0;
        DopeReleaseVertexAnim();   // (las filas de vertex anim del dope se resuelven por la anim activa)
    }
    RemoveCurvesOf(AnimationObjects, all);
    for (size_t e = 0; e < SceneAnimations.size(); e++) if (SceneAnimations[e]) RemoveCurvesOf(SceneAnimations[e]->objects, all);
    for (size_t i = 0; i < ip->Childrens.size(); i++) LightsOutside(ip->Childrens[i]);
    outside.swap(ip->Childrens);
}
// y la vuelve a colgar (el paso deja de tenerla)
static void HangGenerated(InstancePrefab* ip, std::vector<Object*>& children) {
    for (size_t i = 0; i < children.size(); i++) {
        children[i]->Parent = ip;
        ip->Childrens.push_back(children[i]);
        LightsInside(children[i]);
    }
    children.clear();
}
// lo de la escena que nombra por NOMBRE a algo generado (una camara que mira a un hijo de la instancia) lo
// vuelve a buscar: ahora es otra generacion
static void RefreshAfterRegenerate() {
    if (SceneCollection) { SceneCollection->ReloadAll(); W3dConstraintsResolveNames(SceneCollection); }
    PropsForgetScene();
    W3dResourcesViewInvalidate();
    g_redraw = true;
}

// el paso: lo guardado es el estado que NO esta puesto (Aplicar lo intercambia con el vivo). La instancia va
// por SERIAL (nunca se recicla): si ya no esta en la raiz activa, el paso no hace nada
struct UndoRegen {
    unsigned serial;
    std::string prefab;
    std::map<std::string, std::string> props;
    std::map<std::string, bool> vis;
    std::vector<InstanceBase> base;
    unsigned version;
    bool notGenerated;
    std::vector<Object*> children;   // la generacion DESCOLGADA (del paso: se libera con el)
    bool freeing;
    UndoRegen() : serial(0), version(0), notGenerated(false), freeing(false) {}
};
static void UndoRegenApply(void* d) {
    UndoRegen* u = (UndoRegen*)d;
    InstancePrefab* ip = InstanceBySerial(SceneCollection, u->serial);
    if (!ip) { w3dLogfW("[prefabs] Ctrl+Z/Ctrl+Y: la instancia del paso ya no esta en la escena (no se hace nada)"); return; }
    std::vector<Object*> now;
    UnhookGenerated(ip, now);
    HangGenerated(ip, u->children);
    u->children.swap(now);
    std::swap(ip->prefab, u->prefab);
    ip->overProps.swap(u->props);
    ip->overVisible.swap(u->vis);
    ip->base.swap(u->base);
    std::swap(ip->generatedVersion, u->version);
    std::swap(ip->notGenerated, u->notGenerated);
    RefreshAfterRegenerate();
}
// un objeto se libera de verdad: lo descolgado del paso no puede quedar apuntandolo
static void UndoRegenUnlink(void* d, Object* deleted) {
    UndoRegen* u = (UndoRegen*)d;
    if (!deleted || u->freeing) return;
    for (size_t i = 0; i < u->children.size(); i++) {
        std::vector<Object*> sub;
        CollectSub(u->children[i], sub);
        for (size_t k = 0; k < sub.size(); k++) if (sub[k] != deleted) sub[k]->UnlinkFrom(deleted);
    }
}
static void UndoRegenFree(void* d) {
    UndoRegen* u = (UndoRegen*)d;
    u->freeing   = true;
    for (size_t i = 0; i < u->children.size(); i++) W3dFreeSubtree(u->children[i]);
    delete u;
}

// regenera 'ip' con Ctrl+Z: con otro prefab (nuevo != NULL) y/o sin sus overrides
static void RegenerateWithUndo(InstancePrefab* ip, const std::string* newValue, bool noOverrides) {
    // lo que el usuario cambio en lo generado desde el ultimo cuadro va al paso (el Ctrl+Z lo devuelve)
    W3dPrefabSyncOverrides(ip);
    UndoRegen* u = new UndoRegen();
    u->serial = ip->serial;
    u->prefab = ip->prefab;
    u->props = ip->overProps;
    u->vis = ip->overVisible;
    u->base = ip->base;
    u->version = ip->generatedVersion;
    u->notGenerated = ip->notGenerated;
    UnhookGenerated(ip, u->children);
    if (newValue) ip->prefab = *newValue;
    if (noOverrides) { ip->overProps.clear(); ip->overVisible.clear(); }
    W3dPrefabGenerate(ip);   // (ya no tiene hijos: no libera nada)
    UndoExternal f;
    f.apply = UndoRegenApply; f.unlink = UndoRegenUnlink; f.free = UndoRegenFree;
    UndoPushExternal(f, u);
    RefreshAfterRegenerate();
}

bool W3dPrefabChange(InstancePrefab* ip, const std::string& newValue, std::string* reason) {
    if (!ip) return false;
    if (ip->getType() == ObjectType::proxy) { if (reason) *reason = "It is a W3D proxy: choose its library element"; return false; }
    // (con el Play andando lo generado tiene scripts, cuerpos y fotos de la partida: regenerarlo no se puede)
    if (SimActive()) { if (reason) *reason = "Stop the game first"; return false; }
    if (W3dIsGenerated(ip)) { if (reason) *reason = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
    if (!ExistsDefinition(newValue)) { if (reason) *reason = "There is no such prefab"; return false; }
    if (SerialCycle(newValue)) { if (reason) *reason = "A prefab can't contain itself"; return false; }
    if (ip->prefab == newValue) return true;
    RegenerateWithUndo(ip, &newValue, false);
    return true;
}

bool W3dPrefabResetOverrides(InstancePrefab* ip, std::string* reason) {
    if (!ip) return false;
    if (SimActive()) { if (reason) *reason = "Stop the game first"; return false; }
    // (una instancia anidada no guarda overrides propios: los de lo que genera son de la de afuera)
    if (W3dIsGenerated(ip)) { if (reason) *reason = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
    W3dPrefabSyncOverrides(ip);
    if (ip->overProps.empty() && ip->overVisible.empty()) return true;
    RegenerateWithUndo(ip, NULL, true);   // (lo generado vuelve a ser exactamente el prefab)
    return true;
}

// ============================================================================
//  LOS PROXIES (un prefab o una escena de una LIBRERIA externa)
// ============================================================================
bool W3dProxyCanAdd(const std::string& lib, int type, const std::string& elem, std::string* reason) {
    if (InteractionMode != ObjectMode || state != editNavigation) { if (reason) *reason = "Only in Object Mode"; return false; }
    if (lib.empty()) return true;   // (un proxy vacio: se elige despues en su tarjeta)
    std::string m;
    if (!W3dLibsEnsure(lib, &m)) { if (reason) *reason = m; return false; }
    if (!elem.empty() && !W3dLibsHasElement(lib, type, elem)) { if (reason) *reason = "The library has no such element"; return false; }
    return true;
}

ProxyW3d* W3dProxyAdd(const std::string& lib, int type, const std::string& elem, const Vector3& pos, std::string* reason) {
    if (!W3dProxyCanAdd(lib, type, elem, reason)) return NULL;
    // editando un PREFAB, el proxy va ADENTRO de su objeto raiz (como una instancia anidada)
    Object* parent = NULL;
    Vector3 local  = pos;
    if (!PrefabEdited().empty() && SceneCollection && !SceneCollection->Childrens.empty()) {
        parent = SceneCollection->Childrens[0];
        Matrix4 w; parent->GetWorldMatrixBase(w);
        local = w.Inverse() * pos;
    }
    ProxyW3d* px = W3dProxyCreate(lib, type, elem, parent, local, 0.0f);
    DeselectAll();
    px->Select();
    UndoCaptureCreation();        // Ctrl+Z lo saca con lo que genero (Ctrl+Y lo devuelve)
    W3dResourcesViewInvalidate();
    g_redraw = true;
    w3dLogf("[librerias] proxy '%s' de '%s/%s' en (%.2f, %.2f, %.2f)", px->name.c_str(), lib.c_str(), elem.c_str(), pos.x, pos.y, pos.z);
    return px;
}

bool W3dProxyChange(ProxyW3d* px, const std::string& lib, int type, const std::string& elem, std::string* reason) {
    if (!px) return false;
    if (SimActive()) { if (reason) *reason = "Stop the game first"; return false; }
    if (W3dIsGenerated(px)) { if (reason) *reason = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
    if (!lib.empty()) {
        std::string m;
        if (!W3dLibsEnsure(lib, &m)) { if (reason) *reason = m; return false; }
        if (!elem.empty() && !W3dLibsHasElement(lib, type, elem)) { if (reason) *reason = "The library has no such element"; return false; }
    }
    // la CLAVE nueva (la misma que arma el proxy: una parcial si hay libreria y todavia no elemento)
    const std::string key = W3dProxyKey(lib, type, elem);
    if (key == px->prefab) return true;
    // (otro elemento: los overrides del anterior no aplican; con Ctrl+Z vuelven)
    RegenerateWithUndo(px, &key, true);
    return true;
}

// las REFERENCIAS DEL PROYECTO a una libreria que no estaba (io/Libraries.h): los objetos con ese serial, en las
// raices cargadas (tambien adentro de lo que genera un proxy: un proxy de OTRA libreria no las tiene, pero no cuesta)
static void CollectBySerial(Object* o, const std::set<unsigned>& s, std::vector<Object*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (s.count(h->serial)) out.push_back(h);
        CollectBySerial(h, s, out);
    }
}
int W3dLibRefsResolve(const std::string& lib) {
    if (lib.empty() || W3dLibsFind(lib) < 0) return 0;
    std::vector<unsigned> sm, sa;
    W3dLibRefsSerials(lib, W3D_LIBREF_MESH, sm);
    W3dLibRefsSerials(lib, W3D_LIBREF_ANIMSET, sa);
    if (sm.empty() && sa.empty()) return 0;
    if (!W3dLibsEnsure(lib, NULL)) return 0;   // (vinculada pero sin su archivo: siguen pendientes)
    std::set<unsigned> all(sm.begin(), sm.end());
    all.insert(sa.begin(), sa.end());
    std::vector<Object*> roots, objs;
    W3dRootsInOrder(roots);
    for (size_t r = 0; r < roots.size(); r++) CollectBySerial(roots[r], all, objs);
    int n = 0;
    for (size_t i = 0; i < objs.size(); i++) {
        Object* o = objs[i];
        if (o->getType() == ObjectType::mesh && !((Mesh*)o)->mesh) {
            Mesh* m = (Mesh*)o;
            const W3dLibRef* ref = W3dLibRefOf(m, W3D_LIBREF_MESH);
            MeshResource* rec = ref ? W3dMeshResourceByName(ref->name) : NULL;
            if (!rec || !W3dMeshLink(m, rec)) continue;
            if (!m->modifiers.empty()) m->GenerateMeshModified();
            W3dLibRefForget(m, W3D_LIBREF_MESH);
            n++;
        } else if (o->getType() == ObjectType::armature && ((Armature*)o)->animations.empty()) {
            Armature* a = (Armature*)o;
            const W3dLibRef* p = W3dLibRefOf(a, W3D_LIBREF_ANIMSET);
            if (!p) continue;
            const W3dLibRef ref = *p;   // (copia: olvidarla la borra)
            std::string reason;
            if (!W3dArmatureAssignAnimSet(a, ref.name, ref.withClips ? &ref.clips : NULL, &reason)) {
                w3dLogfW("[librerias] '%s': el animset '%s' sigue sin poder usarse: %s", a->name.c_str(), ref.name.c_str(),
                         reason.c_str());
                continue;
            }
            W3dLibRefForget(a, W3D_LIBREF_ANIMSET);
            n++;
        }
    }
    if (n) w3dLogf("[librerias] '%s' volvio: %d referencia(s) del proyecto resueltas", lib.c_str(), n);
    return n;
}

// el REGISTRO DE LIBRERIAS cambio para 'lib' (se vinculo, se desvinculo, un Ctrl+Z la devolvio): lo que depende de
// ella se regenera (ver el .h)
int W3dProxiesLibChange(const std::string& lib, bool activeWithUndo) {
    if (lib.empty()) return 0;
    W3dPrefabInvalidateLib(lib);
    // (lo que el PROYECTO nombraba de ella y quedo pendiente se resuelve: una malla, un animset)
    const int resolved = W3dLibRefsResolve(lib);
    // las OTRAS raices cargadas: liberando (cambiar de raiz vacia el historial: ningun paso nombra lo suyo)
    int n = W3dPrefabRegenerateFromLib(lib, false);
    // la ACTIVA: un paso por instancia, sin liberar (el Ctrl+Z vuelve a colgar lo que generaba)
    if (activeWithUndo && SceneCollection) {
        std::vector<InstancePrefab*> v;
        W3dPrefabDependsOnLib(SceneCollection, lib, v);
        for (size_t i = 0; i < v.size(); i++) { RegenerateWithUndo(v[i], NULL, false); n++; }
    }
    if (n || resolved) {
        PropsForgetScene();
        W3dResourcesViewInvalidate();
        g_redraw = true;
        w3dLogf("[librerias] '%s' cambio en el registro: %d proxy(s)/instancia(s) regenerados", lib.c_str(), n);
    }
    return n + resolved;
}

// ============================================================================
//  VOLVER DE EDITAR UN PREFAB: su definicion cambio -> TODAS sus instancias se regeneran
// ============================================================================
void W3dPrefabStoppedEditing(const std::string& name) {
    if (name.empty()) return;
    W3dPrefabInvalidate(name);
    const int n = W3dPrefabRegenerateInstances(name);
    if (n > 0) {
        PropsForgetScene();
        W3dResourcesViewInvalidate();
        g_redraw = true;
    }
    // lo que quedo SUELTO al lado del objeto raiz (importado, pegado, sacado de el) no es parte del prefab: sus
    // instancias solo generan el objeto raiz y lo que cuelga de el. Se avisa (el archivo lo conserva igual)
    const int idx = W3dRootFind(W3D_ROOT_PREFAB, name);
    Object* root = (idx >= 0) ? W3dRoots()[(size_t)idx].root : NULL;
    if (root && root->Childrens.size() > 1) {
        const int loose = (int)root->Childrens.size() - 1;
        char b[400];
        snprintf(b, sizeof(b), T("Prefab \"%s\": %d object(s) are outside its root object and its instances don't show them"),
                 name.c_str(), loose);
        Notify(std::string(b), true);
        w3dLogfW("[prefabs] '%s': %d objeto(s) sueltos al lado del objeto raiz (las instancias no los generan)", name.c_str(), loose);
    }
}

// ============================================================================
//  Add EDITANDO UN PREFAB: lo nuevo va ADENTRO de su objeto raiz (suelto al lado no lo generaria ninguna
//  instancia), en el mismo lugar del mundo. Una UI (una escena 2D) no: queda como la creo el Add
// ============================================================================
void W3dPrefabAdoptNew(Object* newValue) {
    if (!newValue || PrefabEdited().empty() || !SceneCollection || SceneCollection->Childrens.empty()) return;
    if (newValue->getType() == ObjectType::ui) return;
    Object* root = SceneCollection->Childrens[0];
    if (root == newValue) return;   // (un prefab vacio: lo primero que se agrega ES su objeto raiz)
    // solo lo que quedo suelto en el primer nivel (una coleccion activa, un padre elegido: ya estan adentro)
    Object* parent = newValue->Parent ? newValue->Parent : SceneCollection;
    if (parent != SceneCollection) return;
    const Vector3 wpos = newValue->GetGlobalPositionBase();
    const Quaternion wrot = RotGlobalOf(newValue);
    const Vector3 wesc = ScaleGlobalOf(newValue);
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
        if (SceneCollection->Childrens[i] == newValue) { SceneCollection->Childrens.erase(SceneCollection->Childrens.begin() + (long)i); break; }
    newValue->Parent = root;
    root->Childrens.push_back(newValue);
    Matrix4 w; root->GetWorldMatrixBase(w);
    newValue->pos = w.Inverse() * wpos;
    const Quaternion prg = RotGlobalOf(root);
    newValue->SetRot(prg.Inverted() * wrot);
    const Vector3 psg = ScaleGlobalOf(root);
    newValue->scale = Vector3(psg.x != 0.0f ? wesc.x / psg.x : wesc.x, psg.y != 0.0f ? wesc.y / psg.y : wesc.y,
                           psg.z != 0.0f ? wesc.z / psg.z : wesc.z);
    w3dLogf("[prefabs] Add editando el prefab: '%s' va adentro de su objeto raiz '%s'", newValue->name.c_str(), root->name.c_str());
}

// ============================================================================
//  UNPACK
// ============================================================================
// una REFERENCIA POR NOMBRE de lo que se desempaqueta que apunta ADENTRO de lo mismo (el "cuerpo" del script de
// SU esqueleto, el target de SU camara): adentro de la instancia se resolvia por su scope; afuera, lo que choca se
// renumera ("Cuerpo" -> "Cuerpo.001") y la referencia tiene que seguir a SU objeto, no al primer homonimo de la
// escena (el cubo del usuario, el cuerpo de otro enemigo desempaquetado antes)
namespace {
struct InternalRef {
    W3dRenameDest dest;   // donde vive el nombre (identidad estable: ver Undo.h)
    Object* owner;        // el objeto que la tiene
    bool fromScript;      // una ref de script (se resuelve como en el Play) o un target/constraint
    Object* obj;          // a quien apunta (de adentro)
    std::string name;     // el nombre con el que lo nombra
};
}
static void CollectInternalRefs(Object* g, std::vector<InternalRef>& out) {
    out.clear();
    std::vector<Object*> sub;
    CollectSub(g, sub);
    std::set<Object*> inside(sub.begin(), sub.end());
    for (size_t i = 0; i < sub.size(); i++) {
        Object* o = sub[i];
        if (o->scriptData)
            for (size_t s = 0; s < o->scriptData->scripts.size(); s++) {
                const std::vector<std::pair<std::string, std::string> >& refs = o->scriptData->scripts[s].refs;
                for (size_t r = 0; r < refs.size(); r++) {
                    if (refs[r].second.empty()) continue;
                    Object* t = W3dSceneFindRef(o, refs[r].second);
                    if (!t || !inside.count(t)) continue;
                    InternalRef ri; ri.dest = W3dDestRefLua(o, (int)s, (int)r); ri.owner = o; ri.fromScript = true;
                    ri.obj = t; ri.name = refs[r].second;
                    out.push_back(ri);
                }
            }
        const int n = o->RefsObject();
        for (int k = 0; k < n; k++) {
            std::string* p = o->RefObjectName(k);
            if (!p || p->empty()) continue;
            Object* t = o->RefObject(k);
            if (!t) t = W3dFindNameFrom(o, *p);
            if (!t || !inside.count(t) || t->name != *p) continue;
            InternalRef ri; ri.dest = W3dDestName(p); ri.owner = o; ri.fromScript = false; ri.obj = t; ri.name = *p;
            out.push_back(ri);
        }
    }
}
// despues de la mudanza: cada ref interna pasa a decir el nombre que le quedo a SU objeto (en el mismo paso de
// undo que la mudanza). Devuelve cuantas, igual, resuelven a OTRO objeto (un homonimo de otro scope que la
// busqueda de la escena encuentra antes): se avisa
static int DragInternalRefs(const std::vector<InternalRef>& refs) {
    std::vector<W3dRenameDest> destinations;
    std::vector<std::string> newValue;
    for (size_t i = 0; i < refs.size(); i++) {
        if (!W3dObjectLive(refs[i].obj) || refs[i].obj->name == refs[i].name) continue;
        destinations.push_back(refs[i].dest);
        newValue.push_back(refs[i].obj->name);
    }
    if (!destinations.empty()) {
        UndoCaptureRenames(destinations);
        for (size_t i = 0; i < destinations.size(); i++)
            if (std::string* p = W3dDestResolve(destinations[i])) *p = newValue[i];
        w3dLogf("[prefabs] unpack: %d referencia(s) internas siguen a su objeto renumerado", (int)destinations.size());
    }
    int other = 0;
    for (size_t i = 0; i < refs.size(); i++) {
        Object* t = refs[i].obj;
        if (!W3dObjectLive(t) || !W3dObjectLive(refs[i].owner)) continue;
        Object* now = refs[i].fromScript ? W3dSceneFindRef(refs[i].owner, t->name) : W3dFindNameFrom(refs[i].owner, t->name);
        if (now != t) {
            other++;
            w3dLogfW("[prefabs] unpack: la referencia '%s' de '%s' encuentra otro objeto con ese nombre", t->name.c_str(), refs[i].owner->name.c_str());
        }
    }
    return other;
}

bool W3dPrefabUnpack(InstancePrefab* ip, std::string* reason) {
    if (!ip) return false;
    // (un PROXY es de solo lectura: lo que genera es de su libreria)
    if (ip->getType() == ObjectType::proxy) { if (reason) *reason = "A W3D proxy is read-only: it can't be unpacked"; return false; }
    if (SimActive()) { if (reason) *reason = "Stop the game first"; return false; }
    if (InteractionMode != ObjectMode || state != editNavigation) { if (reason) *reason = "Only in Object Mode"; return false; }
    if (W3dIsGenerated(ip)) { if (reason) *reason = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
    // una DIFERIDA que la vista previa del streaming descargo: se genera primero (lo que se desempaqueta es lo suyo)
    if (ip->streamState != W3D_STREAM_LOADED && ip->Childrens.empty()) W3dPrefabGenerate(ip);
    Object* g = ip->RootGenerated();
    if (!g) { if (reason) *reason = "The instance has nothing to unpack"; return false; }
    Object* parent = ip->Parent ? ip->Parent : SceneCollection;
    const std::string nameG = g->name;
    // las refs por nombre de adentro que apuntan adentro, como se resuelven AHORA (en el scope de la instancia)
    std::vector<InternalRef> internal;
    CollectInternalRefs(g, internal);
    // UN Ctrl+Z, SECUENCIAL: deshacer devuelve primero la instancia y despues le cuelga otra vez lo suyo
    UndoGroupStart();
    g_w3dPrefabUnpacking = true;
    // lo generado sale a donde esta la instancia (mismo lugar en el mundo, en el scope de afuera: lo que choque
    // se renumera) y queda justo antes de ella
    ReparentKeepTransform(g, parent);
    if (g->Parent == parent) MoveNextTo(g, ip, false);
    g_w3dPrefabUnpacking = false;
    const bool exited = (g->Parent == parent);
    const int foreignRefs = exited ? DragInternalRefs(internal) : 0;
    if (exited && ip->Childrens.empty()) {
        DeselectAll();
        ip->Select();
        UndoCaptureDeleted(false);   // la instancia (ya vacia) se va
        // al salir, el nombre del objeto raiz pudo chocar con el de la instancia misma (casi siempre se llaman
        // igual): con la instancia afuera, vuelve a tener el suyo si quedo libre
        if (g->name != nameG && g->FreeName(nameG) == nameG) W3dRenameObject(g, nameG, false);
    }
    UndoGroupSequentialEnd();
    if (!exited) { if (reason) *reason = "The instance could not be unpacked"; return false; }
    DeselectAll();
    g->Select();
    PropsForgetScene();
    W3dResourcesViewInvalidate();
    g_redraw = true;
    if (foreignRefs > 0) {
        char b[300];
        snprintf(b, sizeof(b), T("%d reference(s) of the unpacked objects now find another object with the same name"), foreignRefs);
        Notify(std::string(b), true);
    }
    w3dLogf("[prefabs] instancia desempaquetada: '%s' ahora es de la escena", g->name.c_str());
    return true;
}

// ============================================================================
//  CREATE PREFAB (desde la seleccion), con Ctrl+Z
//
//  Es UN paso de undo SECUENCIAL de dos partes:
//    1) la MUDANZA (UndoRelocation, abajo): lo elegido sale de la escena y pasa a ser el contenido de la raiz
//       del prefab nuevo, que entra al registro;
//    2) la INSTANCIA que queda en su lugar: una creacion comun (UndoCaptureCreation).
//  Ctrl+Z deshace 2 y despues 1: la instancia se va, lo mudado vuelve TAL CUAL (mismo padre, mismo lugar
//  entre sus hermanos, misma transform, sus luces, sus curvas de las animaciones de escena, la camara y el
//  clip activos) y la fila del prefab SALE del registro (W3dRootRemove: su raiz queda en el paso). Ctrl+Y
//  rehace 1 y despues 2. Lo que se muda son los MISMOS objetos en los dos sentidos, asi los pasos de antes
//  del historial que los nombran siguen valiendo: por LIFO, cuando se deshacen ya volvieron a su lugar.
//  (abrir el prefab u otra raiz vacia el historial, como siempre: el paso nunca aplica en otra raiz activa)
// ============================================================================
static void CollectSub(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectSub(o->Childrens[i], out);
}
static int IndexInParent(Object* o) {
    Object* p = o->Parent ? o->Parent : SceneCollection;
    for (size_t i = 0; p && i < p->Childrens.size(); i++) if (p->Childrens[i] == o) return (int)i;
    return -1;
}
static void Unhook(Object* o) {
    Object* p = o->Parent ? o->Parent : SceneCollection;
    if (!p) return;
    for (size_t i = 0; i < p->Childrens.size(); i++) if (p->Childrens[i] == o) { p->Childrens.erase(p->Childrens.begin() + (long)i); break; }
}
// el preorden del arbol activo (para dejar las raices elegidas en el orden de la escena)
static void Preorder(Object* o, std::map<Object*, int>& order, int& n) {
    if (!o) return;
    order[o] = n++;
    for (size_t i = 0; i < o->Childrens.size(); i++) Preorder(o->Childrens[i], order, n);
}
namespace {
struct ByOrder {
    const std::map<Object*, int>* order;
    bool operator()(Object* a, Object* b) const { return order->find(a)->second < order->find(b)->second; }
};

// una RAIZ de lo elegido: de donde sale en la escena y como queda en el prefab
struct RelocatedRoot {
    Object* obj;
    Object* parentEsc;                                 // su padre en la escena (la raiz de la escena si es de primer nivel)
    int idxEsc;                                        // su lugar entre los hermanos de alla
    Vector3 posEsc, escEsc, eulEsc; Quaternion rotEsc; // su transform en la escena
    Vector3 posPre, escPre, eulPre; Quaternion rotPre; // y en el prefab
    RelocatedRoot() : obj(0), parentEsc(0), idxEsc(-1) {}
};
struct RelocatedLight { Light* light; int idx; RelocatedLight() : light(0), idx(0) {} };
// una curva de una animacion de escena de lo mudado: la escena va por NOMBRE (unico en su raiz) y 'pos' es
// solo el orden que tenia en su lista (se acota al volver)
struct RelocatedCurve { std::string scene; int pos; AnimationObject a; RelocatedCurve() : pos(0) {} };

// el paso de undo de la MUDANZA (parte 1 del "Create Prefab")
struct UndoRelocation {
    std::string prefab;                 // la fila del prefab (por nombre: su indice se corre)
    bool outside;                       // true = deshecha: lo elegido volvio a la escena y la fila la tiene 's'
    bool dead;                          // uno de sus objetos se libero de verdad: el paso ya no hace nada
    W3dRemovedRoot s;
    Object* container;                     // de quien cuelgan en el prefab (el vacio con su nombre; NULL = la raiz misma)
    std::vector<RelocatedRoot> roots;      // en el orden de la escena
    std::vector<RelocatedLight> lights;    // las luces que se fueron (su lugar en Lights de la escena)
    std::vector<RelocatedCurve> curves;    // las curvas de las animaciones de escena de lo que se fue
    Camera* camera;                        // la camara activa, si era de lo que se fue
    bool animWas; int animKind; Armature* animArm; Mesh* animMesh;   // el clip activo, si era de lo que se fue
    Object* instance; unsigned instanceSerial;   // la instancia que queda en su lugar (sigue al prefab si se renombra)
    std::vector<Object*> selPrev; Object* activePrev;   // la seleccion de antes (vuelve con Ctrl+Z)
    UndoRelocation() : outside(false), dead(false), container(0), camera(0), animWas(false), animKind(0),
                    animArm(0), animMesh(0), instance(0), instanceSerial(0), activePrev(0) {}
};
bool ByIdxEsc(const RelocatedRoot* a, const RelocatedRoot* b) { return a->idxEsc < b->idxEsc; }
bool ByIdxLight(const RelocatedLight& a, const RelocatedLight& b) { return a.idx < b.idx; }
}

// todo lo que se muda (las raices con sus subarboles)
static void CollectMoved(const UndoRelocation* u, std::vector<Object*>& all) {
    all.clear();
    for (size_t i = 0; i < u->roots.size(); i++) CollectSub(u->roots[i].obj, all);
}
// la lista VIVA de curvas de la animacion de escena 'nombre' de la raiz activa (NULL = ya no esta)
static std::vector<AnimationObject>* SceneNameCurves(const std::string& name) {
    for (size_t e = 0; e < SceneAnimations.size(); e++)
        if (SceneAnimations[e] && SceneAnimations[e]->name == name)
            return ((int)e == ActiveSceneAnim) ? &AnimationObjects : &SceneAnimations[e]->objects;
    return NULL;
}

// LA ESCENA -> EL PREFAB (hacer y Ctrl+Y). La fila del prefab ya esta en el registro. Las transforms del
// prefab (posPre...) ya estan calculadas; las de la escena, los lugares entre hermanos, las luces y las
// curvas se toman ACA (son las del estado de ahora)
static bool MoveToPrefab(UndoRelocation* u) {
    const int pidx = W3dRootFind(W3D_ROOT_PREFAB, u->prefab);
    if (pidx < 0 || !W3dRoots()[(size_t)pidx].root) return false;
    Object* praiz = W3dRoots()[(size_t)pidx].root;
    Object* dest = u->container ? u->container : praiz;
    std::vector<Object*> all;
    CollectMoved(u, all);
    std::set<Object*> setAll(all.begin(), all.end());
    // lo que la UI y las globales tienen de lo que se va (como un borrado)
    W3dClipsViewsSync();
    PropsForgetScene();
    for (size_t i = ObjSelects.size(); i-- > 0; ) if (setAll.count(ObjSelects[i])) { ObjSelects[i]->select = false; ObjSelects.erase(ObjSelects.begin() + (long)i); }
    if (setAll.count(ActiveObj)) ActiveObj = NULL;
    u->camera = ((Object*)CameraActive && setAll.count((Object*)CameraActive)) ? CameraActive : NULL;
    if (u->camera) CameraActive = NULL;
    u->animWas = (ActiveAnimArm && setAll.count((Object*)ActiveAnimArm)) || (ActiveAnimMesh && setAll.count((Object*)ActiveAnimMesh));
    if (u->animWas) {
        u->animKind = ActiveAnimKind; u->animArm = ActiveAnimArm; u->animMesh = ActiveAnimMesh;
        ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
        if (ActiveAnimKind == 1 || ActiveAnimKind == 3 || ActiveAnimKind == 4) ActiveAnimKind = 0;
    }
    // las curvas de animacion de escena de lo que se va (nombrarian objetos de otra raiz): se las lleva el paso
    u->curves.clear();
    InitSceneAnimations();
    for (size_t e = 0; e < SceneAnimations.size(); e++) {
        if (!SceneAnimations[e]) continue;
        std::vector<AnimationObject>& v = ((int)e == ActiveSceneAnim) ? AnimationObjects : SceneAnimations[e]->objects;
        int removed = 0;
        for (size_t i = 0; i < v.size(); ) {
            if (setAll.count(v[i].obj)) {
                RelocatedCurve c; c.scene = SceneAnimations[e]->name; c.pos = (int)i + removed; c.a = v[i];
                u->curves.push_back(c);
                v.erase(v.begin() + (long)i);
                removed++;
            } else i++;
        }
    }
    if (setAll.count(CollectionActive)) CollectionActive = SceneCollection;
    // las luces que se van dejan de ser de la raiz activa (el tope de 8 de GL es por raiz)
    u->lights.clear();
    for (size_t k = 0; k < Lights.size(); k++)
        if (setAll.count((Object*)Lights[k])) { RelocatedLight l; l.light = Lights[k]; l.idx = (int)k; u->lights.push_back(l); }
    for (size_t k = Lights.size(); k-- > 0; ) if (setAll.count((Object*)Lights[k])) Lights.erase(Lights.begin() + (long)k);
    // ---- la mudanza (los lugares entre hermanos, TODOS antes de descolgar ninguno) ----
    for (size_t i = 0; i < u->roots.size(); i++) {
        RelocatedRoot& r = u->roots[i];
        r.parentEsc = r.obj->Parent ? r.obj->Parent : SceneCollection;
        r.idxEsc = IndexInParent(r.obj);
        r.posEsc = r.obj->pos; r.rotEsc = r.obj->Rot(); r.eulEsc = r.obj->rotEuler; r.escEsc = r.obj->scale;
    }
    for (size_t i = 0; i < u->roots.size(); i++) Unhook(u->roots[i].obj);
    for (size_t i = 0; i < u->roots.size(); i++) {
        RelocatedRoot& r = u->roots[i];
        r.obj->Parent    = dest;
        dest->Childrens.push_back(r.obj);
        r.obj->pos = r.posPre;
        r.obj->SetRotSnapshot(r.rotPre, r.eulPre);
        r.obj->scale = r.escPre;
    }
    // sus luces pasan a ser del prefab (se encienden cuando se lo edita)
    if (!u->lights.empty()) {
        const int act = W3dActiveRoot();
        if (W3dRootUse(pidx)) {
            for (size_t i = 0; i < u->lights.size(); i++) Lights.push_back(u->lights[i].light);
            W3dRootUse(act);
        }
    }
    W3dPrefabInvalidate(u->prefab);   // (su definicion sale de lo que acaba de llegar)
    return true;
}

// EL PREFAB -> LA ESCENA (Ctrl+Z): todo vuelve a donde estaba. La fila del prefab la saca el que llama
static bool MoveToScene(UndoRelocation* u) {
    const int pidx = W3dRootFind(W3D_ROOT_PREFAB, u->prefab);
    if (pidx < 0 || !W3dRoots()[(size_t)pidx].root) return false;
    Object* praiz = W3dRoots()[(size_t)pidx].root;
    Object* dest = u->container ? u->container : praiz;
    // las luces salen del prefab y vuelven a su lugar en la escena
    if (!u->lights.empty()) {
        const int act = W3dActiveRoot();
        if (W3dRootUse(pidx)) {
            for (size_t i = 0; i < u->lights.size(); i++)
                for (size_t k = 0; k < Lights.size(); k++) if (Lights[k] == u->lights[i].light) { Lights.erase(Lights.begin() + (long)k); break; }
            W3dRootUse(act);
        }
        std::vector<RelocatedLight> v = u->lights;
        std::sort(v.begin(), v.end(), ByIdxLight);
        for (size_t i = 0; i < v.size(); i++) {
            const int p = v[i].idx < (int)Lights.size() ? v[i].idx : (int)Lights.size();
            Lights.insert(Lights.begin() + p, v[i].light);
        }
    }
    // las raices: fuera del prefab, y a su lugar entre los hermanos de la escena (de a una en orden creciente de
    // lugar: cada una deja listo el hueco de la siguiente)
    for (size_t i = 0; i < u->roots.size(); i++) {
        Object* o = u->roots[i].obj;
        for (size_t k = 0; k < dest->Childrens.size(); k++) if (dest->Childrens[k] == o) { dest->Childrens.erase(dest->Childrens.begin() + (long)k); break; }
    }
    std::vector<RelocatedRoot*> order;
    for (size_t i = 0; i < u->roots.size(); i++) order.push_back(&u->roots[i]);
    std::stable_sort(order.begin(), order.end(), ByIdxEsc);
    for (size_t i = 0; i < order.size(); i++) {
        RelocatedRoot& r = *order[i];
        Object* p = r.parentEsc;
        r.obj->Parent = p;
        const int at = (r.idxEsc >= 0 && r.idxEsc <= (int)p->Childrens.size()) ? r.idxEsc : (int)p->Childrens.size();
        p->Childrens.insert(p->Childrens.begin() + at, r.obj);
        r.obj->pos = r.posEsc;
        r.obj->SetRotSnapshot(r.rotEsc, r.eulEsc);
        r.obj->scale = r.escEsc;
    }
    // sus curvas vuelven a su animacion de escena (en orden creciente de lugar dentro de cada una)
    InitSceneAnimations();
    for (size_t i = 0; i < u->curves.size(); i++) {
        std::vector<AnimationObject>* v = SceneNameCurves(u->curves[i].scene);
        if (!v) { w3dLogfW("[prefabs] Ctrl+Z: la animacion de escena '%s' ya no esta (una curva no vuelve)", u->curves[i].scene.c_str()); continue; }
        const int p = u->curves[i].pos < (int)v->size() ? u->curves[i].pos : (int)v->size();
        v->insert(v->begin() + (p < 0 ? 0 : p), u->curves[i].a);
    }
    u->curves.clear();
    if (u->camera) CameraActive = u->camera;
    if (u->animWas) { ActiveAnimKind = u->animKind; ActiveAnimArm = u->animArm; ActiveAnimMesh = u->animMesh; }
    return true;
}

// lo de la escena que nombra a lo que se mudo (camaras, espejos, instancias, constraints) lo vuelve a buscar
static void RefreshAfterRelocation() {
    SceneCollection->ReloadAll();
    W3dConstraintsResolveNames(SceneCollection);
    PropsForgetScene();
    W3dResourcesViewInvalidate();
    g_redraw = true;
}

static void UndoRelocationApply(void* d) {
    UndoRelocation* u = (UndoRelocation*)d;
    if (u->dead) { w3dLogfW("[prefabs] Ctrl+Z/Ctrl+Y de 'Create Prefab': un objeto del paso ya no existe (no se hace nada)"); return; }
    if (!u->outside) {
        // Ctrl+Z: lo mudado vuelve a la escena y la fila del prefab sale del registro (el paso se queda con ella)
        if (!MoveToScene(u)) return;
        std::string reason;
        if (!W3dRootRemove(W3dRootFind(W3D_ROOT_PREFAB, u->prefab), &u->s, &reason))
            w3dLogfW("[prefabs] Ctrl+Z de 'Create Prefab': el prefab '%s' no pudo salir del registro (%s)", u->prefab.c_str(), reason.c_str());
        else u->outside = true;
        W3dPrefabInvalidate(u->prefab);
        // la seleccion de antes del "Create Prefab"
        DeselectAll();
        for (size_t i = 0; i < u->selPrev.size(); i++)
            if (W3dObjectLive(u->selPrev[i]) && W3dRootOf(u->selPrev[i]) == SceneCollection) u->selPrev[i]->Select();
        if (W3dObjectLive(u->activePrev) && W3dRootOf(u->activePrev) == SceneCollection) ActiveObj = u->activePrev;
    } else {
        // Ctrl+Y: la fila vuelve (con el nombre que tenia, si sigue libre) y lo elegido se muda otra vez
        if (u->outside) {
            Object* root = u->s.row.root;
            if (!W3dRootReturn(u->s)) return;
            u->outside = false;
            const int i = W3dObjectRoot(root);
            const std::string remained = (i >= 0) ? W3dRoots()[(size_t)i].name : u->prefab;
            if (remained != u->prefab && W3dLiveObjectSerial(u->instance, u->instanceSerial))
                ((InstancePrefab*)u->instance)->prefab = remained;
            u->prefab = remained;
        }
        DeselectAll();
        MoveToPrefab(u);
    }
    RefreshAfterRelocation();
}
static void UndoRelocationUnlink(void* d, Object* deleted) {
    UndoRelocation* u = (UndoRelocation*)d;
    if (!deleted) return;
    for (size_t i = u->selPrev.size(); i-- > 0; ) if (u->selPrev[i] == deleted) u->selPrev.erase(u->selPrev.begin() + (long)i);
    if (u->activePrev == deleted) u->activePrev = NULL;
    if (u->instance == deleted) u->instance = NULL;
    bool mine = (deleted == u->container || deleted == (Object*)u->camera || deleted == (Object*)u->animArm || deleted == (Object*)u->animMesh);
    for (size_t i = 0; i < u->roots.size() && !mine; i++) mine = (deleted == u->roots[i].obj || deleted == u->roots[i].parentEsc);
    for (size_t i = 0; i < u->lights.size() && !mine; i++) mine = (deleted == (Object*)u->lights[i].light);
    for (size_t i = 0; i < u->curves.size() && !mine; i++) mine = (deleted == u->curves[i].a.obj);
    if (mine && !u->dead) {
        u->dead = true;
        w3dLogfW("[prefabs] el paso de 'Create Prefab' de '%s' queda sin efecto: '%s' se libero", u->prefab.c_str(), deleted->name.c_str());
    }
}
static void UndoRelocationFree(void* d) {
    UndoRelocation* u = (UndoRelocation*)d;
    if (u->outside) W3dRemovedRootFree(u->s);   // (deshecho: la raiz del prefab es del paso)
    delete u;
}

bool W3dPrefabCreateFromSelection(std::string* nameOut, std::string* reason) {
    if (SimActive()) { if (reason) *reason = "Stop the game first"; return false; }
    if (InteractionMode != ObjectMode || state != editNavigation) { if (reason) *reason = "Only in Object Mode"; return false; }
    if (!SceneCollection) return false;
    // las RAICES de lo elegido (un elegido adentro de otro elegido viaja con el)
    std::vector<Object*> sel;
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || !o->select || o == SceneCollection) continue;
        if (W3dIsGenerated(o)) { if (reason) *reason = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
        if (o->getType() == ObjectType::ui || W3dRootOf(o) != SceneCollection) continue;
        bool inside = false;
        for (Object* p = o->Parent; p && !inside; p = p->Parent) if (p->select) inside = true;
        if (!inside) sel.push_back(o);
    }
    if (sel.empty()) { if (reason) *reason = "Select the objects of the prefab"; return false; }
    std::map<Object*, int> order; int n = 0;
    Preorder(SceneCollection, order, n);
    ByOrder cmp; cmp.order = &order;
    std::sort(sel.begin(), sel.end(), cmp);
    const std::string ed = PrefabEdited();
    // (dentro de un prefab: lo elegido pasa a un prefab NUEVO que queda anidado en el que se edita)
    UndoRelocation* u = new UndoRelocation();
    u->selPrev = ObjSelects;
    u->activePrev = ActiveObj;
    // la instancia va donde estaba el PRIMERO (su padre, su lugar entre hermanos)
    Object* parent = sel[0]->Parent ? sel[0]->Parent : SceneCollection;
    const int idx  = IndexInParent(sel[0]);
    // ---- el prefab NUEVO (su raiz, con un vacio como objeto raiz) ----
    const bool one = (sel.size() == 1);
    const std::string request = one ? sel[0]->name : std::string("Prefab");
    const int pidx = W3dNewRoot(W3D_ROOT_PREFAB, request);
    const std::string name = W3dRoots()[(size_t)pidx].name;
    Object* praiz = W3dRoots()[(size_t)pidx].root;
    Object* empty = praiz->Childrens.empty() ? NULL : praiz->Childrens[0];
    u->prefab = name;
    // la transform de la instancia: la del objeto (uno solo) o ninguna (varios: quedan donde estaban en el
    // espacio del padre de la instancia)
    Vector3 ipos(0, 0, 0), iesc(1, 1, 1), ieul(0, 0, 0);
    Quaternion irot;
    if (one) { ipos = sel[0]->pos; irot = sel[0]->Rot(); ieul = sel[0]->rotEuler; iesc = sel[0]->scale; }
    // ---- como queda cada raiz en el prefab ----
    u->roots.resize(sel.size());
    if (one) {
        // el objeto ES la raiz del prefab (el vacio automatico sobra) y queda en el origen: su transform es la
        // de la instancia
        RelocatedRoot& r = u->roots[0];
        r.obj = sel[0];
        r.posPre = Vector3(0, 0, 0); r.rotPre = Quaternion(); r.eulPre = Vector3(0, 0, 0); r.escPre = Vector3(1, 1, 1);
        if (empty) {
            for (size_t k = 0; k < praiz->Childrens.size(); k++) if (praiz->Childrens[k] == empty) { praiz->Childrens.erase(praiz->Childrens.begin() + (long)k); break; }
            W3dFreeSubtree(empty);
        }
        u->container = NULL;
    } else {
        // (las posiciones de cada raiz en el espacio del PADRE de la instancia, con el mundo de ahora)
        const Quaternion prg = RotGlobalOf(parent);
        const Vector3 psg = ScaleGlobalOf(parent);
        const Vector3 ppg = parent->GetGlobalPositionBase();
        const Quaternion ipr = prg.Inverted();
        for (size_t i = 0; i < sel.size(); i++) {
            RelocatedRoot& r = u->roots[i];
            r.obj = sel[i];
            const Quaternion rg = RotGlobalOf(sel[i]);
            const Vector3 sg = ScaleGlobalOf(sel[i]);
            const Vector3 d = ipr * (sel[i]->GetGlobalPositionBase() - ppg);
            r.posPre = Vector3(psg.x != 0.0f ? d.x / psg.x : d.x, psg.y != 0.0f ? d.y / psg.y : d.y, psg.z != 0.0f ? d.z / psg.z : d.z);
            // (el euler que deriva SetRot, para que el Ctrl+Y deje exactamente lo mismo)
            const Vector3 eulPrev = sel[i]->rotEuler; const Quaternion rotPrev = sel[i]->Rot();
            sel[i]->SetRot(ipr * rg);
            r.rotPre = sel[i]->Rot(); r.eulPre = sel[i]->rotEuler;
            sel[i]->SetRotSnapshot(rotPrev, eulPrev);
            r.escPre = Vector3(psg.x != 0.0f ? sg.x / psg.x : sg.x, psg.y != 0.0f ? sg.y / psg.y : sg.y, psg.z != 0.0f ? sg.z / psg.z : sg.z);
        }
        u->container = empty;
    }
    // ---- la mudanza ----
    MoveToPrefab(u);
    // ---- la instancia en su lugar ----
    ObjSelects.clear(); ActiveObj = NULL;
    InstancePrefab* ip = new InstancePrefab(parent == SceneCollection ? NULL : parent, ipos);
    Unhook(ip);
    ip->Parent = parent;
    if (idx >= 0 && idx <= (int)parent->Childrens.size()) parent->Childrens.insert(parent->Childrens.begin() + idx, ip);
    else parent->Childrens.push_back(ip);
    ip->SetNameObj(name);
    ip->prefab = name;
    if (one) { ip->SetRotSnapshot(irot, ieul); ip->scale = iesc; }
    W3dPrefabGenerate(ip);
    u->instance = ip; u->instanceSerial = ip->serial;
    // lo de la escena que nombraba a lo que se fue (camaras, espejos, instancias, constraints): ahora lo
    // encuentra en lo que genera la instancia (mismos nombres)
    SceneCollection->ReloadAll();
    W3dConstraintsResolveNames(SceneCollection);
    // las CURVAS de las animaciones de escena de lo que se fue (se las lleva el paso: nombrarian objetos de otra
    // raiz, y las de lo generado no se guardan). Con UN objeto la instancia ocupa su lugar con su transform: sus
    // curvas de transform/visible/render pasan a la INSTANCIA (la animacion de la escena la sigue moviendo); el
    // Ctrl+Z las saca con la instancia y devuelve las originales. Lo demas se AVISA: no se pierde en silencio
    int curvesOutside = 0;
    for (size_t i = 0; i < u->curves.size(); i++) {
        const RelocatedCurve& c = u->curves[i];
        AnimationObject a = c.a;
        std::vector<AnimProperty> props;
        if (one && c.a.obj == sel[0])
            for (size_t k = 0; k < a.Properties.size(); k++) {
                const int p = a.Properties[k].Property;
                if (p == AnimPosition || p == AnimRotation || p == AnimScale || p == AnimVisible || p == AnimRender)
                    props.push_back(a.Properties[k]);
            }
        const int total = (int)a.Properties.size();
        std::vector<AnimationObject>* v = props.empty() ? NULL : SceneNameCurves(c.scene);
        if (!v) { curvesOutside += total; continue; }
        curvesOutside += total - (int)props.size();
        a.obj = ip;
        a.Properties = props;
        a.UpdateFirstLastFrame();
        const int p = (c.pos >= 0 && c.pos < (int)v->size()) ? c.pos : (int)v->size();
        v->insert(v->begin() + p, a);
    }
    if (curvesOutside > 0) {
        char b[300];
        snprintf(b, sizeof(b), T("%d animation curve(s) of the selection no longer animate anything (Undo brings them back)"), curvesOutside);
        Notify(std::string(b), true);
        w3dLogfW("[prefabs] Create Prefab: %d curva(s) de las animaciones de escena quedan en el paso de undo", curvesOutside);
    }
    DeselectAll();
    ip->Select();
    // ---- el paso de undo: la mudanza + la creacion de la instancia, UN solo Ctrl+Z (secuencial) ----
    UndoGroupStart();
    UndoExternal f; f.apply = UndoRelocationApply; f.unlink = UndoRelocationUnlink; f.free = UndoRelocationFree;
    UndoPushExternal(f, u);
    UndoCaptureCreation();
    UndoGroupSequentialEnd();
    W3dTouchedChanges();
    W3dResourcesViewInvalidate();
    g_redraw = true;
    if (nameOut) *nameOut = name;
    std::vector<Object*> all;
    CollectMoved(u, all);
    w3dLogf("[prefabs] prefab '%s' creado con %d objeto(s) de la seleccion%s; queda una instancia '%s'",
            name.c_str(), (int)all.size(), ed.empty() ? "" : " (anidado en el prefab que se edita)", ip->name.c_str());
    return true;
}

// ============================================================================
//  Add > Prefab
// ============================================================================
static std::vector<std::string> gMenuAdd;   // id - 1 -> el nombre del prefab (se rearma en cada apertura)
// (solo los prefabs DEL PROYECTO: los de una libreria vinculada -io/ExternalLibrary.h- no se instancian
//  como una InstancePrefab sino como un proxy de libreria, de solo lectura; cuando exista, sus filas van
//  aca debajo de las del proyecto, con su propio rango de ids)
void W3dPrefabMenuAddBuild(PopupMenu* m) {
    if (!m) return;
    m->Clear();
    gMenuAdd.clear();
    const std::vector<W3dRootRow>& fs = W3dRoots();
    for (size_t i = 0; i < fs.size(); i++) {
        if (fs[i].type != W3D_ROOT_PREFAB) continue;
        if (SerialCycle(fs[i].name)) continue;   // (el que se edita y los que lo contienen)
        gMenuAdd.push_back(fs[i].name);
        m->Add(fs[i].name, (int)gMenuAdd.size(), (int)IconType::prefab);
    }
    // los de las LIBRERIAS vinculadas ("Personajes/Enemigo"): crean un PROXY (la fila guarda su clave)
    for (int l = 0; l < W3dLibsCount(); l++) {
        const std::string& lib = W3dLibsRow(l).name;
        std::vector<std::string> ps;
        W3dLibsElements(lib, W3D_LIB_PREFAB, ps);
        for (size_t i = 0; i < ps.size(); i++) {
            gMenuAdd.push_back(W3dLibsKey(lib, W3D_LIB_PREFAB, ps[i]));
            m->Add(lib + "/" + ps[i], (int)gMenuAdd.size(), (int)IconType::lib);
        }
    }
    if (gMenuAdd.empty()) m->Add(T("No prefabs yet"), 0, -1);
}
// ============================================================================
//  Add > Scene: una ESCENA del proyecto instanciada en la que se edita (io/Prefabs.h: "escena:<nombre>")
// ============================================================================
static std::vector<std::string> gMenuAddScene;
void W3dSceneMenuAddBuild(PopupMenu* m) {
    if (!m) return;
    m->Clear();
    gMenuAddScene.clear();
    const std::vector<W3dRootRow>& fs = W3dRoots();
    for (size_t i = 0; i < fs.size(); i++) {
        if (!W3dRootEs3D(fs[i].type) || (int)i == W3dActiveRoot()) continue;   // (la que se edita no)
        const std::string key = W3dPrefabKeyScene(fs[i].name);
        if (SerialCycle(key)) continue;                                           // (ni las que la contienen)
        gMenuAddScene.push_back(key);
        m->Add(fs[i].name, (int)gMenuAddScene.size(), W3dRootIcon(fs[i].type));
    }
    if (gMenuAddScene.empty()) m->Add(T("No other scenes"), 0, -1);
}
void W3dSceneMenuAddAction(int id) {
    if (id < 1 || id > (int)gMenuAddScene.size()) return;
    std::string reason;
    if (!W3dPrefabAdd(gMenuAddScene[(size_t)(id - 1)], cursor3D.pos, &reason)) Notify(std::string(T(reason.c_str())), true);
}

std::string W3dPrefabMenuAddName(int id) {
    return (id >= 1 && id <= (int)gMenuAdd.size()) ? gMenuAdd[(size_t)(id - 1)] : std::string();
}
void W3dPrefabMenuAddAction(int id) {
    const std::string n = W3dPrefabMenuAddName(id);
    if (n.empty()) return;
    std::string reason;
    std::string lib, elem; int type = W3D_LIB_PREFAB;
    if (W3dLibsByKey(n, &lib, &type, &elem)) {   // (uno de una libreria: un proxy en el cursor 3D)
        if (!W3dProxyAdd(lib, type, elem, cursor3D.pos, &reason)) Notify(std::string(T(reason.c_str())), true);
        return;
    }
    if (!W3dPrefabAdd(n, cursor3D.pos, &reason)) Notify(std::string(T(reason.c_str())), true);
}

// ============================================================================
//  COMO SE CARGA UNA INSTANCIA (io/Streaming.h): "carga", "distancia" y "objetivo", con Ctrl+Z
// ============================================================================
namespace {
struct UndoLoad { Object* ip; unsigned serial; int load; float distance; std::string target; };
}
unsigned g_w3dLoadVersion = 0;
static void UndoLoadApply(void* d) {
    UndoLoad* u = (UndoLoad*)d;
    if (!u->ip || !W3dLiveObjectSerial(u->ip, u->serial)) return;
    InstancePrefab* ip = (InstancePrefab*)u->ip;
    std::swap(ip->load, u->load);
    std::swap(ip->distance, u->distance);
    ip->target.swap(u->target);
    g_w3dLoadVersion++;
    W3dStreamingChangedInstance(ip);
    W3dTouchedChanges();
    g_redraw = true;
}
static void UndoLoadUnlink(void* d, Object* deleted) { UndoLoad* u = (UndoLoad*)d; if (u->ip == deleted) u->ip = NULL; }
static void UndoLoadFree(void* d) { delete (UndoLoad*)d; }
void W3dInstanceLoadUndo(InstancePrefab* ip, int load, float distance, const std::string& target) {
    if (!ip) return;
    if (ip->load == load && ip->distance == distance && ip->target == target) return;   // (nada cambio)
    UndoLoad* u = new UndoLoad();
    u->ip = ip; u->serial = ip->serial; u->load = load; u->distance = distance; u->target = target;
    UndoExternal f; f.apply = UndoLoadApply; f.unlink = UndoLoadUnlink; f.free = UndoLoadFree;
    UndoPushExternal(f, u);
    g_w3dLoadVersion++;
    W3dStreamingChangedInstance(ip);
    W3dTouchedChanges();
}
void W3dInstanceLoadChange(InstancePrefab* ip, int load, float distance, const std::string& target) {
    if (!ip) return;
    const int c0 = ip->load; const float d0 = ip->distance; const std::string o0 = ip->target;
    ip->load = (load == W3D_LOAD_DISTANCE) ? W3D_LOAD_DISTANCE : W3D_LOAD_ALWAYS;
    ip->distance = (distance >= 0.0f) ? distance : 0.0f;
    ip->target = target;
    W3dInstanceLoadUndo(ip, c0, d0, o0);
    g_redraw = true;
}
