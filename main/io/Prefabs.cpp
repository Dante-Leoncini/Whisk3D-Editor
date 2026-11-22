// ============================================================================
//  Prefabs.cpp — ver Prefabs.h. La definicion de cada prefab (cacheada, con su
//  version), la generacion de los hijos de una instancia (con el lector de
//  siempre, import_w3d), los overrides, el alta de instancias y el instanciar()
//  de lua. Compila en el editor y en el runtime 3D (C++03).
// ============================================================================
#include "io/Prefabs.h"
#include "io/JsonW3d.h"
#include "io/Libraries.h"              // los prefabs y escenas de las LIBRERIAS (lo que generan los proxies)
#include "objects/InstancePrefab.h"
#include "objects/ProxyW3d.h"
#include "objects/Objects.h"
#include "animation/Animation.h"         // las curvas que nombran a lo que se suelta
#include "animation/W3dAnimSet.h"        // W3dHierNode: las rutas de los overrides de visible
#include "script/W3dScript.h"            // las propiedades que DECLARA un script (los overrides de valor)
#include "base/W3dInteractionState.h"    // la generacion no toca la seleccion de Edit/Pose
#include "W3dRoots.h"                    // el registro de prefabs y el cambio de raiz (regenerar en todas)
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <map>
#include <set>

bool (*W3dPrefabSerializeHook)(Object* rootPrefab, std::string& json) = 0;
bool (*W3dSceneSerializeHook)(Object* rootScene, const std::string& name, std::string& json) = 0;

// ---- ESCENAS COMO PREFAB ("escena:<nombre>", ver Prefabs.h) ----
static const char kPrefixScene[] = "scene:";
bool W3dPrefabIsScene(const std::string& key, std::string* scene) {
    if (key.compare(0, sizeof(kPrefixScene) - 1, kPrefixScene) != 0) return false;
    if (scene) *scene = key.substr(sizeof(kPrefixScene) - 1);
    return true;
}
std::string W3dPrefabKeyScene(const std::string& scene) { return std::string(kPrefixScene) + scene; }
void (*W3dPrefabFixHook)(Object* generated, Object* tmpl) = 0;

// ============================================================================
//  LAS DEFINICIONES
// ============================================================================
namespace {
struct DefPrefab {
    JVal* doc;                 // el documento parseado (dueno)
    JVal* root;                // su objeto raiz (adentro de doc); NULL = no se pudo leer
    unsigned version;          // 0 = no se pudo leer
    bool fromMemory;           // salio de la raiz CARGADA del editor (serializacion en seco)
    Object* tmpl;              // (fromMemory) el objeto raiz del prefab del que salio...
    unsigned templateSerial;   // ...y su serial (la direccion se puede reciclar)
    std::string lib;           // la LIBRERIA de la que salio ("" = del proyecto): lo que genera se arma en su
                                // contexto (rutas y nombres de recursos con su prefijo, io/Libraries.h)
    DefPrefab() : doc(0), root(0), version(0), fromMemory(false), tmpl(0), templateSerial(0) {}
    ~DefPrefab() { delete doc; }
private:
    DefPrefab(const DefPrefab&);
    DefPrefab& operator=(const DefPrefab&);
};
}
static std::map<std::string, DefPrefab*> gDefs;
static unsigned gNextVersion = 1;           // global y monotona: una version nunca se repite (ni entre prefabs)
static std::vector<std::string> gStack;     // los prefabs que se estan generando (anidados): el ciclo se corta
// las propiedades que DECLARA cada .lua (los overrides de valor, mas abajo): se olvidan con las definiciones (un
// script editado en el IDE puede declarar otras)
static std::map<std::string, std::set<std::string> > gDeclared;

void W3dPrefabStackEnter(const std::string& name) { gStack.push_back(name); }
void W3dPrefabStackExit() { if (!gStack.empty()) gStack.pop_back(); }
static bool InStack(const std::string& name) {
    for (size_t i = 0; i < gStack.size(); i++) if (gStack[i] == name) return true;
    return false;
}

static void ForgetDef(std::map<std::string, DefPrefab*>::iterator it) {
    delete it->second;
    gDefs.erase(it);
}
void W3dPrefabInvalidate(const std::string& name) {
    if (name.empty()) { W3dPrefabsForgetAll(); return; }
    gDeclared.clear();
    std::map<std::string, DefPrefab*>::iterator it = gDefs.find(name);
    if (it != gDefs.end()) ForgetDef(it);
}
void W3dPrefabsForgetAll() {
    for (std::map<std::string, DefPrefab*>::iterator it = gDefs.begin(); it != gDefs.end(); ++it) delete it->second;
    gDefs.clear();
    gStack.clear();
    gDeclared.clear();
}

static JVal* Parse(const char* data, size_t n, const std::string& who) {
    JParser p(data, n);
    JVal* v = p.Value();
    if (p.error || !v || v->type != 4) {
        w3dLogfE("[prefabs] la definicion de '%s' no parsea", who.c_str());
        delete v;
        return NULL;
    }
    return v;
}

// ---- una ESCENA del proyecto como definicion ----
// lo que es DE LA ESCENA y no viaja con sus instancias (Prefabs.h): las UI 2D, las rutinas constructor y las que
// limpian la pantalla
static bool IsFromSceneJson(JVal* o) {
    if (!o || o->type != 4) return false;
    const std::string t = JS(o, "type", "");
    if (t == "ui") return true;
    if (t != "routine") return false;
    if (JB(o, "constructor", false)) return true;
    JVal* steps = JChild(o, "steps", 5);
    if (steps)
        for (size_t i = 0; i < steps->list.size(); i++) {
            const std::string p = JS(steps->list[i], "step", "");
            if (p == "clear" || p == "clearColor") return true;
        }
    return false;
}
static JVal* TextJ(const std::string& t) { JVal* v = new JVal(); v->type = 2; v->str = t; return v; }
// el documento {"raiz": {"tipo": "objeto", "nombre": <escena>, "hijos": [...]}} con los objetos de 'objs' (se los
// queda) menos los que son de la escena
static JVal* WrapScene(JVal* objs, const std::string& name) {
    JVal* r = new JVal(); r->type = 4;
    r->obj["type"] = TextJ("object");
    r->obj["name"] = TextJ(name);
    JVal* children = new JVal(); children->type = 5;
    if (objs && objs->type == 5) {
        for (size_t i = 0; i < objs->list.size(); i++) {
            if (IsFromSceneJson(objs->list[i])) delete objs->list[i];
            else children->list.push_back(objs->list[i]);
        }
        objs->list.clear();
    }
    delete objs;
    r->obj["children"] = children;
    JVal* doc = new JVal(); doc->type = 4;
    doc->obj["root"] = r;
    return doc;
}
// saca 'clave' de un objeto JSON sin borrarla (pasa a ser del que llama)
static JVal* Start(JVal* o, const char* key) {
    if (!o || o->type != 4) return NULL;
    std::map<std::string, JVal*>::iterator it = o->obj.find(key);
    if (it == o->obj.end()) return NULL;
    JVal* v = it->second;
    o->obj.erase(it);
    return v;
}
// la escena de su ENTRADA (el juego compilado, o el editor con la escena sin cargar): su .w3de ({"objetos": [...]})
// o, la principal, el "escena" de project.json
static JVal* ReadEntryScene(const W3dRootRow& f, const std::string& name) {
    const std::string path = f.block ? std::string("project.json") : f.entry;
    if (path.empty()) return NULL;
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(path, data) || data.empty()) {
        w3dLogfE("[prefabs] no pude leer '%s' (la escena '%s')", path.c_str(), name.c_str());
        return NULL;
    }
    JVal* doc = Parse((const char*)&data[0], data.size(), name);
    if (!doc) return NULL;
    JVal* objs = f.block ? Start(JChild(doc, "scene", 4), "objects") : Start(doc, "objects");
    delete doc;
    return WrapScene(objs, name);
}

// la definicion de 'nombre': la cacheada o la que se lee ahora (de la memoria del editor si su raiz esta
// cargada, si no de su entrada). Nunca NULL para un nombre del registro: una que no se pudo leer queda
// cacheada con version 0 (no se reintenta en cada instancia)
static DefPrefab* Definition(const std::string& name) {
    std::map<std::string, DefPrefab*>::iterator it = gDefs.find(name);
    if (it != gDefs.end()) {
        // una de MEMORIA cuya plantilla ya no existe (la raiz del prefab se descargo despues de guardar): lo que
        // la serializacion en seco no lleva ya no se puede tomar de ahi, se relee de su entrada (es lo mismo)
        if (!it->second->fromMemory || W3dLiveObjectSerial(it->second->tmpl, it->second->templateSerial))
            return it->second;
        ForgetDef(it);
    }
    // UN ELEMENTO DE UNA LIBRERIA ("lib:<libreria>/prefab|scene/<elemento>", lo que genera un proxy o un prefab
    // anidado adentro de una libreria): su definicion sale de la libreria (nunca de la memoria del editor: la
    // libreria no se edita aca). Una que no se puede leer queda cacheada con version 0, como las del proyecto
    if (name.compare(0, 4, "lib:") == 0) {
        DefPrefab* d = new DefPrefab();
        std::string lib, elem;
        int type = W3D_LIB_PREFAB;
        if (W3dLibsByKey(name, &lib, &type, &elem)) {
            d->lib = lib;
            d->doc = W3dLibsReadElement(lib, type, elem, &d->root);
            if (!d->doc) d->root = NULL;
        }
        if (d->root) d->version = gNextVersion++;
        gDefs[name] = d;
        return d;
    }
    // UNA ESCENA DEL PROYECTO ("escena:<nombre>"): de la memoria del editor si esta cargada, si no de su entrada
    {
        std::string esc;
        if (W3dPrefabIsScene(name, &esc)) {
            DefPrefab* d = new DefPrefab();
            const int ie = W3dRootFind(W3D_ROOT_SCENE, esc);   // (la clase escena: escenas y juegos)
            if (ie >= 0) {
                const W3dRootRow& f = W3dRoots()[(size_t)ie];
                if (f.root && W3dSceneSerializeHook) {
                    std::string json;
                    if (W3dSceneSerializeHook(f.root, esc, json) && !json.empty()) {
                        d->doc = Parse(json.data(), json.size(), name);
                        d->fromMemory = true;
                        d->tmpl       = f.root;
                        d->templateSerial = f.root->serial;
                    }
                } else {
                    d->doc = ReadEntryScene(f, esc);
                }
            }
            if (d->doc) d->root = JChild(d->doc, "root", 4);
            if (d->root) d->version = gNextVersion++;
            else w3dLogfW("[prefabs] la escena '%s' no se pudo leer: sus instancias quedan vacias", esc.c_str());
            gDefs[name] = d;
            return d;
        }
    }
    const int idx = W3dRootFind(W3D_ROOT_PREFAB, name);
    if (idx < 0) return NULL;
    const W3dRootRow& f = W3dRoots()[(size_t)idx];
    DefPrefab* d = new DefPrefab();
    Object* objRoot = (f.root && !f.root->Childrens.empty()) ? f.root->Childrens[0] : NULL;
    if (objRoot && W3dPrefabSerializeHook) {
        std::string json;
        if (W3dPrefabSerializeHook(objRoot, json) && !json.empty()) {
            d->doc = Parse(json.data(), json.size(), name);
            d->fromMemory = true;
            d->tmpl       = objRoot;
            d->templateSerial = objRoot->serial;
        }
    } else if (!f.entry.empty()) {
        std::vector<unsigned char> data;
        if (w3dFileSystem::ReadFileBytes(f.entry, data) && !data.empty())
            d->doc = Parse((const char*)&data[0], data.size(), name);
        else w3dLogfE("[prefabs] no pude leer '%s' (el prefab '%s')", f.entry.c_str(), name.c_str());
    }
    if (d->doc) d->root = JChild(d->doc, "root", 4);
    if (d->root) d->version = gNextVersion++;
    else w3dLogfW("[prefabs] el prefab '%s' no tiene objeto raiz: sus instancias quedan vacias", name.c_str());
    gDefs[name] = d;
    return d;
}

unsigned W3dPrefabVersion(const std::string& name) {
    DefPrefab* d = Definition(name);
    return d ? d->version : 0;
}

// ============================================================================
//  GENERAR
// ============================================================================
static void CollectSubtree(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectSubtree(o->Childrens[i], out);
}
static void RemoveCurves(std::vector<AnimationObject>& v, const std::set<Object*>& s) {
    for (size_t i = v.size(); i-- > 0; ) if (s.count(v[i].obj)) v.erase(v.begin() + (long)i);
}

// suelta lo que la instancia genero (sus hijos): lo que los nombra por puntero (seleccion, activo, las curvas
// de las animaciones de escena de la raiz activa) primero, despues se liberan (~Object hace el resto: refs de
// otros objetos, undo, contextos de otras raices, clips que sonaban)
static void ReleaseGenerated(InstancePrefab* ip) {
    ip->base.clear();
    if (ip->Childrens.empty()) return;
    std::vector<Object*> sub;
    for (size_t i = 0; i < ip->Childrens.size(); i++) CollectSubtree(ip->Childrens[i], sub);
    std::set<Object*> all(sub.begin(), sub.end());
    for (size_t i = ObjSelects.size(); i-- > 0; ) if (all.count(ObjSelects[i])) ObjSelects.erase(ObjSelects.begin() + (long)i);
    if (all.count(ActiveObj)) ActiveObj = NULL;
    if (all.count(CollectionActive)) CollectionActive = SceneCollection;
    RemoveCurves(AnimationObjects, all);
    for (size_t e = 0; e < SceneAnimations.size(); e++) if (SceneAnimations[e]) RemoveCurves(SceneAnimations[e]->objects, all);
    std::vector<Object*> children;
    children.swap(ip->Childrens);
    for (size_t i = 0; i < children.size(); i++) W3dFreeSubtree(children[i]);
}

// la SELECCION del usuario no cambia porque se genere algo (los constructores eligen a cada objeto nuevo)
namespace {
struct GuardSelection {
    std::vector<Object*> sel;
    std::vector<unsigned> serials;
    Object* active;
    unsigned activeSerial;
    unsigned serial;
    int mode;
    GuardSelection() : active(ActiveObj), activeSerial(ActiveObj ? ActiveObj->serial : 0),
                        serial(W3dSelectionSerial), mode(InteractionMode) {
        sel = ObjSelects;
        for (size_t i = 0; i < sel.size(); i++) serials.push_back(sel[i] ? sel[i]->serial : 0);
        InteractionMode = ObjectMode;   // (en Edit/Pose, deseleccionar tocaria los vertices/huesos)
    }
    void Restore(Object* generated) {
        std::vector<Object*> sub;
        CollectSubtree(generated, sub);
        for (size_t i = 0; i < sub.size(); i++) sub[i]->select = false;
        ObjSelects.clear();
        for (size_t i = 0; i < sel.size(); i++)
            if (W3dLiveObjectSerial(sel[i], serials[i])) { sel[i]->select = true; ObjSelects.push_back(sel[i]); }
        ActiveObj = W3dLiveObjectSerial(active, activeSerial) ? active : NULL;
        W3dSelectionSerial = serial;
        InteractionMode = mode;
    }
};
}

// el objeto de la PLANTILLA con ese serial (la definicion de memoria anota el serial de cada nodo)
static void MapSerials(Object* o, std::map<unsigned, Object*>& out) {
    if (!o) return;
    out[o->serial] = o;
    for (size_t i = 0; i < o->Childrens.size(); i++) MapSerials(o->Childrens[i], out);
}

static void TakeBase(InstancePrefab* ip);   // (la foto de lo generado antes de los overrides: mas abajo)

bool W3dPrefabGenerate(InstancePrefab* ip) {
    if (!ip) return false;
    ReleaseGenerated(ip);
    ip->generatedVersion = 0;
    ip->notGenerated = true;
    // (para el streaming, io/Streaming.h, queda CARGADA: tiene lo que su definicion da, aunque sea nada)
    ip->streamState = W3D_STREAM_LOADED;
    if (ip->prefab.empty()) return false;
    if (InStack(ip->prefab)) {
        w3dLogfW("[prefabs] '%s': el prefab '%s' se contiene a si mismo (queda vacia)", ip->name.c_str(), ip->prefab.c_str());
        return false;
    }
    DefPrefab* d = Definition(ip->prefab);
    if (!d || !d->root) {
        if (!d) w3dLogfW("[prefabs] '%s': no hay un prefab '%s' en el proyecto (queda vacia)", ip->name.c_str(), ip->prefab.c_str());
        else if (!d->lib.empty() || ip->prefab.compare(0, 4, "lib:") == 0)
            w3dLogfW("[prefabs] '%s': '%s' no se pudo leer de su libreria (queda vacio)", ip->name.c_str(), ip->prefab.c_str());
        return false;
    }
    { const std::string milestone = "generando el prefab '" + ip->prefab + "'"; w3dLogMemory(milestone.c_str()); }
    GuardSelection guard;
    std::vector<std::pair<Object*, unsigned> > origins;
    W3dPrefabStackEnter(ip->prefab);
    Object* r = NULL;
    {
        // lo de una LIBRERIA se arma en su contexto: sus rutas son entradas de ella y sus recursos van con su prefijo
        W3dLibContextGuard ctx(d->lib);
        r = W3dPrefabBuildChildren(ip, d->root, d->fromMemory ? &origins : NULL);
    }
    W3dPrefabStackExit();
    if (!r) { guard.Restore(ip); return false; }
    // lo que se resuelve por nombre, por SCOPE (primero adentro de esta instancia): targets, rieles, constraints
    ip->ReloadAll();
    W3dConstraintsResolveNames(ip);
    // una definicion de MEMORIA (el editor): lo que la serializacion en seco no lleva sale de la plantilla
    if (d->fromMemory && W3dPrefabFixHook && W3dLiveObjectSerial(d->tmpl, d->templateSerial)) {
        std::map<unsigned, Object*> tpl;
        MapSerials(d->tmpl, tpl);
        for (size_t i = 0; i < origins.size(); i++) {
            std::map<unsigned, Object*>::iterator t = tpl.find(origins[i].second);
            if (t != tpl.end() && origins[i].first) W3dPrefabFixHook(origins[i].first, t->second);
        }
    }
    guard.Restore(ip);
    ip->generatedVersion = d->version;
    ip->notGenerated = false;
    TakeBase(ip);   // (antes de los overrides: los overrides son lo que difiere de esto)
    W3dPrefabApplyOverrides(ip);
    return true;
}

void W3dPrefabRelease(InstancePrefab* ip) {
    if (!ip) return;
    ReleaseGenerated(ip);
    ip->generatedVersion = 0;
    ip->notGenerated = false;   // (no es un prefab que falta: esta descargada)
    ip->streamState = W3D_STREAM_UNLOADED;
}

// ---- LOS RECURSOS QUE PIDE GENERARLA (el streaming los pide antes, io/Streaming.h) ----
static void ResourcesOf(const std::string& key, std::set<std::string>& seen, std::vector<W3dLoadsItem>& out,
                       std::vector<MeshResource*>* meshes) {
    if (key.empty() || !seen.insert(key).second) return;   // (cada definicion una vez: tambien corta un ciclo)
    DefPrefab* d = Definition(key);
    if (!d || !d->root) return;
    std::vector<std::string> nested;
    {
        // lo de una LIBRERIA se resuelve en su contexto, igual que al generarlo (nombres con prefijo, rutas "lib:")
        W3dLibContextGuard ctx(d->lib);
        W3dPrefabResourcesJson(d->root, out, meshes, nested);
    }
    for (size_t i = 0; i < nested.size(); i++) ResourcesOf(nested[i], seen, out, meshes);
}
bool W3dPrefabResources(InstancePrefab* ip, std::vector<W3dLoadsItem>& out, std::vector<MeshResource*>* meshes) {
    out.clear();
    if (meshes) meshes->clear();
    if (!ip || ip->prefab.empty()) return false;
    DefPrefab* d = Definition(ip->prefab);
    if (!d || !d->root) return false;
    std::set<std::string> seen;
    ResourcesOf(ip->prefab, seen, out, meshes);
    return true;
}

// ============================================================================
//  OVERRIDES
// ============================================================================
// las propiedades que DECLARA un .lua (su tabla 'propiedades', gDeclared): se leen UNA vez por archivo (leerlas
// corre el script en un estado aparte)
static bool Declara(const std::string& path, const std::string& prop) {
    std::map<std::string, std::set<std::string> >::iterator it = gDeclared.find(path);
    if (it == gDeclared.end()) {
        std::vector<W3dScriptProp> props;
        std::set<std::string> s;
        if (W3dScriptReadProperties(path, &props))
            for (size_t i = 0; i < props.size(); i++) s.insert(props[i].name);
        it = gDeclared.insert(std::make_pair(path, s)).first;
    }
    return it->second.count(prop) > 0;
}
// el valor de una propiedad en UN script: se pisa si ya tenia uno; si no, se agrega si el script la declara
static void SetProp(W3dScriptEntry& e, const std::string& prop, const std::string& value) {
    for (size_t r = 0; r < e.refs.size(); r++)
        if (e.refs[r].first == prop) { e.refs[r].second = value; return; }
    if (Declara(e.path, prop)) e.refs.push_back(std::make_pair(prop, value));
}
// (tambien adentro de una instancia ANIDADA: lo que genera es parte de lo que genera esta, y el override de
//  afuera manda sobre los suyos, que ya vienen puestos desde la definicion del prefab de afuera)
static void SetPropRec(Object* o, const std::string& prop, const std::string& value) {
    if (!o) return;
    if (o->scriptData)
        for (size_t i = 0; i < o->scriptData->scripts.size(); i++) SetProp(o->scriptData->scripts[i], prop, value);
    for (size_t i = 0; i < o->Childrens.size(); i++) SetPropRec(o->Childrens[i], prop, value);
}

// la ruta pasa por una instancia ANIDADA que esta DESCARGADA (el streaming todavia no la genero, o la descargo): lo
// que nombra no existe AHORA, pero su override sigue valiendo (se le aplica cuando se genere)
static bool PathByNestedUnloaded(Object* root, const std::string& path) {
    if (!root) return false;
    size_t p = 0;
    while ((p = path.find('/', p)) != std::string::npos) {
        Object* n = W3dHierNode(root, path.substr(0, p));
        if (!n) return false;
        if (W3dIsInstanceType(n->getType()) && n->Childrens.empty()) return true;
        p++;
    }
    return false;
}
// tiene adentro (en lo generado) alguna instancia anidada DESCARGADA?
static bool HasNestedUnloaded(Object* o) {
    for (size_t i = 0; o && i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dIsInstanceType(h->getType()) && h->Childrens.empty() && !((InstancePrefab*)h)->notGenerated) return true;
        if (HasNestedUnloaded(h)) return true;
    }
    return false;
}

void W3dPrefabApplyOverrides(InstancePrefab* ip) {
    if (!ip) return;
    Object* root = ip->RootGenerated();
    if (!root) return;
    for (std::map<std::string, bool>::const_iterator it = ip->overVisible.begin(); it != ip->overVisible.end(); ++it) {
        Object* n = W3dHierNode(root, it->first);
        if (n) n->visible = it->second;
        else if (!PathByNestedUnloaded(root, it->first))   // (una anidada diferida: se le aplica al generarse)
            w3dLogfW("[prefabs] '%s': el override de visible de '%s' no encuentra ese hijo", ip->name.c_str(), it->first.c_str());
    }
    for (std::map<std::string, std::string>::const_iterator it = ip->overProps.begin(); it != ip->overProps.end(); ++it)
        for (size_t i = 0; i < ip->Childrens.size(); i++) SetPropRec(ip->Childrens[i], it->first, it->second);
    // los CAMPOS (posicion, rotacion, escala, las casillas) cambiados en este proyecto
    for (std::map<std::string, std::map<std::string, float> >::const_iterator it = ip->overFields.begin(); it != ip->overFields.end(); ++it) {
        Object* n = W3dHierNode(root, it->first);
        if (n) W3dFieldsInstSet(n, it->second);
        else if (!PathByNestedUnloaded(root, it->first))
            w3dLogfW("[prefabs] '%s': el override de '%s' no encuentra ese hijo", ip->name.c_str(), it->first.c_str());
    }
}

void W3dPrefabOverrideProp(InstancePrefab* ip, const std::string& prop, const std::string& value) {
    if (!ip || prop.empty()) return;
    if (value.empty()) { ip->overProps.erase(prop); W3dPrefabGenerate(ip); return; }   // (vuelve el del prefab)
    ip->overProps[prop] = value;
    for (size_t i = 0; i < ip->Childrens.size(); i++) SetPropRec(ip->Childrens[i], prop, value);
}

// TODO lo generado de 'o' hacia abajo, INCLUIDO lo que generan las instancias anidadas: el prefab de afuera los
// trae adentro, asi que un cambio ahi (el ojo de un hijo, un valor de su script) es un override de ESTA instancia
// (la anidada es a su vez un objeto generado: no se escribe y sus overrides propios no tendrian donde guardarse)
static void CollectScope(Object* o, std::vector<Object*>& out) {
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        out.push_back(h);
        CollectScope(h, out);
    }
}
// la foto de UN objeto generado como esta ahora (ver InstanceBase); 'raiz' = el objeto raiz generado de la instancia
static InstanceBase BaseOf(Object* o, Object* root) {
    InstanceBase b;
    b.o = o; b.serial = o->serial; b.visible = o->visible;
    for (int k = 0; k < W3D_FIELDS_INST; k++) b.fields[k] = W3dFieldInstRead(o, k);
    if (root) b.path = W3dHierPath(root, o);
    if (o->scriptData)
        for (size_t k = 0; k < o->scriptData->scripts.size(); k++) {
            b.refs.push_back(o->scriptData->scripts[k].refs);
            b.paths.push_back(o->scriptData->scripts[k].path);
        }
    return b;
}
// la foto de lo generado ANTES de los overrides (ver InstanceBase)
static void TakeBase(InstancePrefab* ip) {
    ip->base.clear();
    std::vector<Object*> v;
    CollectScope(ip, v);
    for (size_t i = 0; i < v.size(); i++) ip->base.push_back(BaseOf(v[i], ip->RootGenerated()));
}

void W3dPrefabNestedGenerated(InstancePrefab* nested) {
    if (!nested || !W3dInstanceOf(nested)) return;
    std::vector<Object*> newValue;
    CollectScope(nested, newValue);   // (lo que genero; la anidada misma ya era de la base de las de afuera)
    // de la mas cercana a la de afuera de todo: la base de cada una es lo generado con los overrides de las de adentro
    // ya puestos (como cuando la de afuera genera todo junto), y despues mandan los suyos
    for (Object* a = W3dInstanceOf(nested); a; a = W3dInstanceOf(a)) {
        InstancePrefab* ip = (InstancePrefab*)a;
        Object* root = ip->RootGenerated();
        for (size_t i = 0; i < newValue.size(); i++) {
            InstanceBase b = BaseOf(newValue[i], root);
            // la entrada MUERTA de esa misma ruta (la generacion anterior de la anidada) deja su lugar
            if (!b.path.empty())
                for (size_t k = ip->base.size(); k-- > 0; )
                    if (ip->base[k].path == b.path && !W3dLiveObjectSerial(ip->base[k].o, ip->base[k].serial))
                        ip->base.erase(ip->base.begin() + (long)k);
            ip->base.push_back(b);
        }
        W3dPrefabApplyOverrides(ip);
    }
}
// el valor de la propiedad 'prop' en una lista de refs ("" + false = no la tiene)
static bool ValueOf(const std::vector<std::pair<std::string, std::string> >& refs, const std::string& prop, std::string& out) {
    for (size_t i = 0; i < refs.size(); i++) if (refs[i].first == prop) { out = refs[i].second; return true; }
    return false;
}
// 'o' lo genero 'ip' (directamente, o una instancia anidada adentro de lo que genero)?
bool W3dPrefabFieldAltered(Object* o, int k) {
    if (!o || k < 0 || k >= W3D_FIELDS_INST) return false;
    // (la instancia de AFUERA de todo: su base tiene todo lo que genera, tambien lo de las anidadas)
    Object* top = W3dInstanceOf(o);
    if (!top) return false;
    for (Object* i = W3dInstanceOf(top); i; i = W3dInstanceOf(i)) top = i;
    const InstancePrefab* ip = (const InstancePrefab*)top;
    for (size_t i = 0; i < ip->base.size(); i++) {
        const InstanceBase& b = ip->base[i];
        if (b.o == o && b.serial == o->serial) return W3dFieldInstRead(o, k) != b.fields[k];
    }
    return false;
}
static bool GeneratedBy(const Object* o, const InstancePrefab* ip) {
    for (Object* i = W3dInstanceOf(o); i; i = W3dInstanceOf(i)) if (i == (const Object*)ip) return true;
    return false;
}
// el script de la BASE que corresponde al k-esimo script de ahora: el de la MISMA ruta (y, si el objeto trae dos
// veces el mismo .lua, el de la misma ocurrencia). -1 = la definicion no lo trae (lo agregaron a mano: no es un
// override, lo generado no se guarda)
static int ScriptInBase(const InstanceBase& b, const std::vector<W3dScriptEntry>& now, size_t k) {
    int occurrence = 0;
    for (size_t i = 0; i < k; i++) if (now[i].path == now[k].path) occurrence++;
    for (size_t i = 0; i < b.paths.size() && i < b.refs.size(); i++)
        if (b.paths[i] == now[k].path && occurrence-- == 0) return (int)i;
    return -1;
}
bool W3dPrefabSyncOverrides(InstancePrefab* ip) {
    if (!ip || ip->base.empty()) return false;
    // una instancia ANIDADA (la genero otra) no recalcula los suyos: son los que trae la definicion de la de afuera, y
    // lo que el usuario cambia en lo que ella genera es override de la DE AFUERA. Recalcularlos aca se llevaba a la
    // anidada -que no se guarda- un override de la de afuera (el ojo que la de afuera le aplico parecia propio)
    if (W3dIsGenerated(ip)) return false;
    Object* root = ip->RootGenerated();
    std::map<std::string, bool> vis;
    std::map<std::string, std::string> props;
    std::map<std::string, std::map<std::string, float> > fields;
    for (size_t i = 0; i < ip->base.size(); i++) {
        const InstanceBase& b = ip->base[i];
        if (!W3dLiveObjectSerial(b.o, b.serial)) {
            // lo que YA NO ESTA (una instancia anidada DIFERIDA que el streaming descargo; lo que un script destruyo
            // jugando) no deja de tener sus overrides: se conservan los que habia (su visible por su ruta, y las
            // propiedades de sus scripts por nombre). Sin esto, guardar con una anidada descargada los perdia
            if (!b.path.empty()) {
                std::map<std::string, bool>::const_iterator ov = ip->overVisible.find(b.path);
                if (ov != ip->overVisible.end()) vis[ov->first] = ov->second;
                std::map<std::string, std::map<std::string, float> >::const_iterator oc = ip->overFields.find(b.path);
                if (oc != ip->overFields.end()) fields[oc->first] = oc->second;
            }
            for (size_t k = 0; k < b.refs.size(); k++)
                for (size_t r = 0; r < b.refs[k].size(); r++) {
                    std::map<std::string, std::string>::const_iterator op = ip->overProps.find(b.refs[k][r].first);
                    if (op != ip->overProps.end()) props[op->first] = op->second;
                }
            continue;
        }
        if (!GeneratedBy(b.o, ip)) continue;
        if (b.o->visible != b.visible && root) {
            const std::string path = W3dHierPath(root, b.o);
            if (!path.empty()) vis[path] = b.o->visible;
        }
        // los CAMPOS que difieren de como se genero (lo que se cambio en este proyecto). Lo que mueve una ANIMACION (el
        // playhead lo poso) o el JUEGO corriendo no es un cambio del usuario: esos campos quedan como estaban
        if (root) {
            const std::string path = W3dHierPath(root, b.o);
            const bool moved = b.o->posedByCurves || b.o->posedByClip || (AnimIsGame && PlayAnimation);
            std::map<std::string, std::map<std::string, float> >::const_iterator prev = ip->overFields.find(path);
            for (int k = 0; k < W3D_FIELDS_INST && !path.empty(); k++) {
                if (moved && k < 10) {   // (la transformacion y Renderizable: lo que escriben los clips)
                    if (prev != ip->overFields.end()) {
                        std::map<std::string, float>::const_iterator c = prev->second.find(W3dFieldInstName(k));
                        if (c != prev->second.end()) fields[path][c->first] = c->second;
                    }
                    continue;
                }
                const float v = W3dFieldInstRead(b.o, k);
                if (v != b.fields[k]) fields[path][W3dFieldInstName(k)] = v;
            }
        }
        if (!b.o->scriptData) continue;
        const std::vector<W3dScriptEntry>& scripts = b.o->scriptData->scripts;
        for (size_t k = 0; k < scripts.size(); k++) {
            const int kb = ScriptInBase(b, scripts, k);
            if (kb < 0) continue;
            const std::vector<std::pair<std::string, std::string> >& now = scripts[k].refs;
            for (size_t r = 0; r < now.size(); r++) {
                std::string prev;
                const bool had = ValueOf(b.refs[(size_t)kb], now[r].first, prev);
                if (!had || prev != now[r].second) props[now[r].first] = now[r].second;
            }
        }
    }
    // lo de una instancia ANIDADA que el streaming tiene DESCARGADA no esta en la base (nunca se genero, o ya no esta):
    // sus overrides se conservan tal cual (sin esto, guardar con la vista previa prendida los perdia)
    if (root) {
        for (std::map<std::string, bool>::const_iterator it = ip->overVisible.begin(); it != ip->overVisible.end(); ++it)
            if (!vis.count(it->first) && PathByNestedUnloaded(root, it->first)) vis[it->first] = it->second;
        for (std::map<std::string, std::map<std::string, float> >::const_iterator it = ip->overFields.begin(); it != ip->overFields.end(); ++it)
            if (!fields.count(it->first) && PathByNestedUnloaded(root, it->first)) fields[it->first] = it->second;
        if (HasNestedUnloaded(ip))
            for (std::map<std::string, std::string>::const_iterator it = ip->overProps.begin(); it != ip->overProps.end(); ++it)
                if (!props.count(it->first)) props[it->first] = it->second;
    }
    const bool change = (vis != ip->overVisible) || (props != ip->overProps) || (fields != ip->overFields);
    ip->overVisible.swap(vis);
    ip->overProps.swap(props);
    ip->overFields.swap(fields);
    return change;
}

// ============================================================================
//  LAS INSTANCIAS DEL PROYECTO
// ============================================================================
// las instancias de 'nombre' en el arbol 'o' ("" = de cualquiera). onlyOfOutside: las que no estan adentro de lo
// generado por otra (regenerar la de afuera ya regenera las de adentro)
static void CollectInstances(Object* o, const std::string& name, bool onlyOfOutside, std::vector<InstancePrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dIsInstanceType(h->getType())) {   // (una instancia o un proxy: los dos generan)
            InstancePrefab* ip = (InstancePrefab*)h;
            if (name.empty() || ip->prefab == name) out.push_back(ip);
            if (onlyOfOutside) continue;
        }
        CollectInstances(h, name, onlyOfOutside, out);
    }
}

void W3dPrefabInstances(const std::string& name, std::vector<InstancePrefab*>& out) {
    out.clear();
    std::vector<Object*> roots;
    W3dRootsInOrder(roots);
    for (size_t r = 0; r < roots.size(); r++) {
        // (las de adentro de un PREFAB cargado cuentan: una instancia anidada es un usuario de verdad; lo que genera
        //  una instancia no: es de ella)
        std::vector<InstancePrefab*> v;
        CollectInstances(roots[r], name, true, v);
        out.insert(out.end(), v.begin(), v.end());
    }
}
int W3dPrefabCountInstances(const std::string& name) {
    std::vector<InstancePrefab*> v;
    W3dPrefabInstances(name, v);
    return (int)v.size();
}

// la instancia de AFUERA de todo que genero a 'ip' (ella misma si no es generada)
static InstancePrefab* FromOutside(InstancePrefab* ip) {
    Object* top = ip;
    for (Object* i = W3dInstanceOf(top); i; i = W3dInstanceOf(i)) top = i;
    return (InstancePrefab*)top;
}

int W3dPrefabRegenerateInstances(const std::string& name) {
    const int act = W3dActiveRoot();
    const std::vector<W3dRootRow>& fs = W3dRoots();
    int n = 0;
    for (size_t k = 0; k < fs.size(); k++) {
        if (!fs[k].root) continue;
        std::vector<InstancePrefab*> v;
        // (con un nombre: tambien las anidadas adentro de otras instancias; con "", solo las de afuera: las de
        //  adentro se regeneran con su contenedora)
        CollectInstances(fs[k].root, name, name.empty(), v);
        if (v.empty()) continue;
        // una anidada adentro de lo que genero OTRA instancia se regenera con la de AFUERA: los overrides de la de
        // afuera pueden tocar lo que la anidada genera (y solo la de afuera los vuelve a poner)
        std::vector<InstancePrefab*> tops;
        std::set<InstancePrefab*> views;
        for (size_t i = 0; i < v.size(); i++) {
            InstancePrefab* t = FromOutside(v[i]);
            if (views.insert(t).second) tops.push_back(t);
        }
        // la raiz de la instancia es la ACTIVA mientras se genera (las luces, las animaciones y el nombre libre
        // son por raiz), igual que la carga de una raiz
        if ((int)k != W3dActiveRoot()) W3dRootUse((int)k);
        for (size_t i = 0; i < tops.size(); i++) {
            // lo que el usuario cambio en lo generado (el ojo de un hijo, un valor de su script) se anota ANTES de
            // soltarlo: los overrides salen de comparar lo generado con su base, y regenerar suelta las dos cosas
            W3dPrefabSyncOverrides(tops[i]);
            W3dPrefabGenerate(tops[i]);
            n++;
        }
    }
    if (act >= 0 && W3dActiveRoot() != act) W3dRootUse(act);
    if (n) w3dLogf("[prefabs] %d instancia(s) de '%s' regeneradas", n, name.empty() ? "*" : name.c_str());
    return n;
}

// la clave nombra un elemento de la libreria 'lib' ("lib:<lib>/prefab|scene/...", tambien la parcial)
static bool LibKey(const std::string& key, const std::string& lib) {
    return !lib.empty() && key.size() > lib.size() + 5 && key.compare(0, 4, "lib:") == 0 &&
           key.compare(4, lib.size(), lib) == 0 && key[4 + lib.size()] == '/';
}
void W3dPrefabInvalidateLib(const std::string& lib) {
    for (std::map<std::string, DefPrefab*>::iterator it = gDefs.begin(); it != gDefs.end(); ) {
        std::map<std::string, DefPrefab*>::iterator next = it; ++next;
        if (LibKey(it->first, lib)) ForgetDef(it);
        it = next;
    }
    gDeclared.clear();
}
void W3dPrefabDependsOnLib(Object* root, const std::string& lib, std::vector<InstancePrefab*>& out) {
    out.clear();
    std::vector<InstancePrefab*> all;
    CollectInstances(root, std::string(), false, all);   // (tambien las de adentro de lo generado)
    std::set<InstancePrefab*> views;
    for (size_t i = 0; i < all.size(); i++) {
        if (!LibKey(all[i]->prefab, lib)) continue;
        InstancePrefab* t = FromOutside(all[i]);
        if (views.insert(t).second) out.push_back(t);
    }
}
int W3dPrefabRegenerateFromLib(const std::string& lib, bool alsoActive) {
    const int act = W3dActiveRoot();
    const std::vector<W3dRootRow>& fs = W3dRoots();
    int n = 0;
    for (size_t k = 0; k < fs.size(); k++) {
        if (!fs[k].root || (!alsoActive && (int)k == act)) continue;
        std::vector<InstancePrefab*> v;
        W3dPrefabDependsOnLib(fs[k].root, lib, v);
        if (v.empty()) continue;
        if ((int)k != W3dActiveRoot()) W3dRootUse((int)k);
        for (size_t i = 0; i < v.size(); i++) {
            W3dPrefabSyncOverrides(v[i]);
            W3dPrefabGenerate(v[i]);
            n++;
        }
    }
    if (act >= 0 && W3dActiveRoot() != act) W3dRootUse(act);
    if (n) w3dLogf("[librerias] %d instancia(s)/proxy(s) que dependen de '%s' regenerados", n, lib.c_str());
    return n;
}

int W3dRenamedPrefab(const std::string& old, const std::string& newValue) {
    if (old == newValue) return 0;
    // TODAS las definiciones se olvidan, no solo la del renombrado: la de un prefab que lo CONTIENE (una
    // instancia anidada) tiene en su JSON el nombre viejo ("prefab": "<viejo>"), y cada instancia nueva,
    // duplicada o regenerada de ese contenedor generaria la anidada vacia. Se releen de la memoria (el nodo de
    // la anidada ya se renombro abajo: las raices se cargaron antes del renombre) o de su entrada
    W3dPrefabsForgetAll();
    std::vector<Object*> roots;
    W3dRootsInOrder(roots);
    int n = 0;
    for (size_t r = 0; r < roots.size(); r++) {
        std::vector<InstancePrefab*> v;
        CollectInstances(roots[r], old, false, v);
        for (size_t i = 0; i < v.size(); i++) { v[i]->prefab = newValue; n++; }
    }
    return n;
}

// el prefab 'nombre' tiene adentro (en su definicion, o en la de un prefab anidado) una instancia de 'otro'?
static bool ContainsRec(JVal* j, const std::string& other, std::set<std::string>& seen) {
    if (!j || j->type != 4) return false;
    if (JS(j, "type", "") == "prefab") {
        const std::string p = JS(j, "prefab", "");
        if (p == other) return true;
        if (!p.empty() && !seen.count(p)) {
            seen.insert(p);
            DefPrefab* d = Definition(p);
            if (d && d->root && ContainsRec(d->root, other, seen)) return true;
        }
    }
    JVal* h = JChild(j, "children", 5);
    if (h) for (size_t i = 0; i < h->list.size(); i++) if (ContainsRec(h->list[i], other, seen)) return true;
    return false;
}
bool W3dPrefabContains(const std::string& name, const std::string& other) {
    if (name == other) return true;
    DefPrefab* d = Definition(name);
    if (!d || !d->root) return false;
    std::set<std::string> seen;
    seen.insert(name);
    return ContainsRec(d->root, other, seen);
}

// ============================================================================
//  CREAR
// ============================================================================
InstancePrefab* W3dPrefabCreateInstance(const std::string& name, Object* parent, const Vector3& pos, float rotYDegrees) {
    std::string esc;
    const bool isScene = W3dPrefabIsScene(name, &esc);   // (una ESCENA del proyecto: "escena:<nombre>")
    if (isScene ? W3dRootFind(W3D_ROOT_SCENE, esc) < 0 : W3dRootFind(W3D_ROOT_PREFAB, name) < 0) return NULL;
    InstancePrefab* ip = new InstancePrefab(parent, pos);   // (el constructor lo cuelga y lo elige)
    ip->SetNameObj(isScene ? esc : name);                  // "Enemigo", "Enemigo.001"... en su scope
    ip->prefab = name;
    if (rotYDegrees != 0.0f) ip->SetRotEuler(Vector3(0.0f, rotYDegrees, 0.0f));
    W3dPrefabGenerate(ip);
    return ip;
}

ProxyW3d* W3dProxyCreate(const std::string& lib, int type, const std::string& element, Object* parent,
                        const Vector3& pos, float rotYDegrees) {
    ProxyW3d* px = new ProxyW3d(parent, pos);   // (el constructor lo cuelga y lo elige)
    px->SetNameObj(element.empty() ? std::string("Proxy") : element);
    px->Set(lib, type, element);
    if (rotYDegrees != 0.0f) px->SetRotEuler(Vector3(0.0f, rotYDegrees, 0.0f));
    if (!lib.empty() && !element.empty()) W3dPrefabGenerate(px);
    return px;
}

bool W3dProxyName(const std::string& name, std::string* outLib, int* type, std::string* element) {
    const size_t bar = name.find('/');
    if (bar == std::string::npos || bar == 0 || bar + 1 >= name.size()) return false;
    const std::string lib = name.substr(0, bar), elem = name.substr(bar + 1);
    if (W3dLibsFind(lib) < 0) return false;
    int t = W3D_LIB_PREFAB;
    if (!W3dLibsHasElement(lib, t, elem)) {
        t = W3D_LIB_SCENE;
        if (!W3dLibsHasElement(lib, t, elem)) return false;
    }
    if (outLib) *outLib = lib;
    if (type) *type = t;
    if (element) *element = elem;
    return true;
}

// instanciar() de lua (script/W3dScript.h): en la raiz ACTIVA, sin tocar la seleccion del editor. La partida
// arranca lo nuevo (W3dScriptSetNewObject). "Libreria/Elemento" (un prefab o una escena de una libreria
// vinculada) crea un PROXY de ese elemento
// la LIBRERIA de lo que genero 'o' ("" = del proyecto): la de la instancia o el proxy mas cercano que la nombra
// (un prefab anidado adentro de una libreria tambien es de ella). Tambien es la de sus SCRIPTS (W3dScriptLibHook)
static std::string GeneratedLib(Object* o) {
    std::string lib;
    for (Object* i = W3dObjectLive(o) ? W3dInstanceOf(o) : NULL; i && lib.empty(); i = W3dInstanceOf(i))
        W3dLibsByKey(((InstancePrefab*)i)->prefab, &lib, NULL, NULL);
    return lib;
}

static Object* HookInstantiate(const char* name, const float* pos, float rotY) {
    if (!name || !SceneCollection) return NULL;
    const Vector3 p = pos ? Vector3(pos[0], pos[1], pos[2]) : Vector3(0, 0, 0);
    Object* parent = W3dObjectLive(W3dPrefabInstantiateParent) ? W3dPrefabInstantiateParent : NULL;   // (NULL = la raiz)
    std::vector<Object*> sel = ObjSelects;
    Object* active = ActiveObj;
    const unsigned serial = W3dSelectionSerial;
    InstancePrefab* ip = NULL;
    // un script de una LIBRERIA (su dueno lo genero un proxy suyo, o un prefab anidado adentro de ella) nombra los
    // prefabs de SU libreria: "Bala" es el de ella antes que uno del proyecto
    {
        const std::string libDue = GeneratedLib(W3dPrefabInstantiateOwner);
        const std::string n = name;
        if (!libDue.empty() && n.find('/') == std::string::npos && W3dLibsHasElement(libDue, W3D_LIB_PREFAB, n))
            ip = W3dProxyCreate(libDue, W3D_LIB_PREFAB, n, parent, p, rotY);
        // "Otra/Rifle" desde un script de una libreria que vincula a "Otra" (una de adentro): la de SU registro
        const size_t bar = n.find('/');
        if (!ip && !libDue.empty() && bar != std::string::npos && bar > 0) {
            const std::string an = W3dLibsNameNested(libDue, n);
            if (!an.empty()) {
                const std::string g = an.substr(0, an.size() - (n.size() - bar)), elem = n.substr(bar + 1);
                if (W3dLibsHasElement(g, W3D_LIB_PREFAB, elem)) ip = W3dProxyCreate(g, W3D_LIB_PREFAB, elem, parent, p, rotY);
                else if (W3dLibsHasElement(g, W3D_LIB_SCENE, elem)) ip = W3dProxyCreate(g, W3D_LIB_SCENE, elem, parent, p, rotY);
            }
        }
    }
    if (!ip) ip = W3dPrefabCreateInstance(name, parent, p, rotY);
    // una ESCENA del proyecto por su nombre ("Auto": si no hay un prefab que se llame asi)
    if (!ip && W3dRootFind(W3D_ROOT_SCENE, name) >= 0) ip = W3dPrefabCreateInstance(W3dPrefabKeyScene(name), parent, p, rotY);
    if (!ip) {
        std::string lib, elem; int type = W3D_LIB_PREFAB;
        if (W3dProxyName(name, &lib, &type, &elem)) ip = W3dProxyCreate(lib, type, elem, parent, p, rotY);
    }
    if (!ip) return NULL;
    ip->select = false;
    ObjSelects.clear();
    for (size_t i = 0; i < sel.size(); i++) if (W3dObjectLive(sel[i])) { sel[i]->select = true; ObjSelects.push_back(sel[i]); }
    ActiveObj = W3dObjectLive(active) ? active : NULL;
    W3dSelectionSerial = serial;
    return ip;
}
namespace {
struct RegisterHookPrefabs {
    RegisterHookPrefabs() {
        W3dPrefabInstantiateHook = HookInstantiate;
        W3dScriptLibHook = GeneratedLib;   // (los scripts de lo que genera un proxy son de su libreria)
    }
} gRegisterHookPrefabs;
}
