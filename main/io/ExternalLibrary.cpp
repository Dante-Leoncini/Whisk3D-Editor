// ============================================================================
//  ExternalLibrary.cpp — las librerias externas vinculadas (ver el .h)
// ============================================================================
#include "io/ExternalLibrary.h"
#include "io/Libraries.h"              // EL registro de las librerias (el mismo que usan los proxies y el juego)
#include "io/W3dZip.h"
#include "io/JsonW3d.h"
#include "io/UI2DFormat.h"            // g_w3dDirProject: la base de las rutas "ext:" relativas
#include "io/W3dContainer.h"          // W3dContainerIs, W3dCosmeticPathJson
#include "undo/Undo.h"
#include "config/W3dLang.h"
#include "W3dRoots.h"                 // W3dRootIcon / W3dRootTypeByKey: el icono de una escena o un juego
#include "io/EditorRoots.h"
#include "io/PrefabsEditor.h"        // W3dProxiesLibChange: lo que depende de una libreria se regenera
#include "io/ProjectMeshes.h"         // W3dMeshNewCopy: desvincular deja a las mallas del proyecto con su copia
#include "objects/Mesh.h"
#include "objects/MeshResource.h"
#include "WhiskUI/draw/icons.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <map>
#include <set>
#include <cstdio>

// lo LEIDO de una libreria vinculada para explorarla (cache; el REGISTRO es el de io/Libraries.h). Paralelo al
// registro por indice; se vuelve a leer si la ruta de esa fila cambio (un undo, otra libreria en su lugar)
struct LinkedLib {
    std::string path;       // la de disco con la que se leyo
    bool read, ok;
    std::string error;
    std::vector<W3dResourceItem> items;
    std::vector<std::string> folders;
    LinkedLib() : read(false), ok(false) {}
};
static std::vector<LinkedLib> gLibs;

void W3dExtLibsInvalidate() {
    for (size_t i = 0; i < gLibs.size(); i++) { gLibs[i].read = false; gLibs[i].items.clear(); gLibs[i].folders.clear(); }
}
std::string W3dExtLibsSignature() {
    std::string f;
    for (int i = 0; i < W3dLibsCount(); i++) { f += W3dLibsRow(i).name; f += '='; f += W3dLibsDiskPath(i); f += '\n'; }
    return f;
}

extern bool SimActive();
// las librerias que CAMBIARON entre dos registros (entro, salio, u otro archivo con su nombre)
static std::set<std::string> Changed(const std::vector<W3dLibRow>& a, const std::vector<W3dLibRow>& b) {
    std::map<std::string, std::string> ma, mb;
    for (size_t i = 0; i < a.size(); i++) ma[a[i].name] = a[i].diskPath;
    for (size_t i = 0; i < b.size(); i++) mb[b[i].name] = b[i].diskPath;
    std::set<std::string> c;
    for (std::map<std::string, std::string>::iterator it = ma.begin(); it != ma.end(); ++it) {
        std::map<std::string, std::string>::iterator o = mb.find(it->first);
        if (o == mb.end() || o->second != it->second) c.insert(it->first);
    }
    for (std::map<std::string, std::string>::iterator it = mb.begin(); it != mb.end(); ++it)
        if (!ma.count(it->first)) c.insert(it->first);
    return c;
}

// ---- el undo del registro: el paso guarda la lista de antes y la de despues ----
// Lo que DEPENDE de las librerias que cambian (sus proxies) se regenera: en la raiz activa con sus propios pasos,
// agrupados con este (W3dProxiesLibChange); aca, al ir y volver, las OTRAS raices cargadas
struct UndoLibs { std::vector<W3dLibRow> other; };
static void UndoLibsApply(void* d) {
    UndoLibs* u = (UndoLibs*)d;
    std::vector<W3dLibRow> now = W3dLibsRows();
    W3dLibsSet(u->other);
    u->other.swap(now);
    W3dExtLibsInvalidate();
    const std::set<std::string> c = Changed(u->other, W3dLibsRows());
    for (std::set<std::string>::const_iterator it = c.begin(); it != c.end(); ++it) W3dProxiesLibChange(*it, false);
}
static void UndoLibsFree(void* d) { delete (UndoLibs*)d; }
extern int g_undoNoRoot;   // (undo/Undo.cpp: el registro de librerias no es una edicion de la raiz activa)
static void PushUndoLibs(const std::vector<W3dLibRow>& prev) {
    UndoLibs* u = new UndoLibs();
    u->other = prev;
    UndoExternal f; f.apply = UndoLibsApply; f.free = UndoLibsFree;
    g_undoNoRoot++;
    UndoPushExternal(f, u);
    g_undoNoRoot--;
}

// el nombre de una libreria: el de su archivo sin ".w3d" (unico entre las vinculadas: es el PREFIJO de sus recursos)
static std::string FileName(const std::string& path) {
    std::string r = path;
    const size_t b = r.find_last_of("/\\");
    if (b != std::string::npos) r = r.substr(b + 1);
    const size_t p = r.find_last_of('.');
    if (p != std::string::npos && p > 0) r = r.substr(0, p);
    return r;
}

bool W3dExtLibLink(const std::string& path, std::string* reason) {
    if (path.empty()) { if (reason) *reason = "No file"; return false; }
    for (int i = 0; i < W3dLibsCount(); i++)
        if (W3dLibsDiskPath(i) == path) { if (reason) *reason = "It is already linked"; return false; }
    if (!W3dContainerIs(path)) { if (reason) *reason = "It isn't a .w3d project"; return false; }
    // (el proyecto abierto no es una libreria de si mismo: sus prefabs se instancian directo)
    { extern std::string w3dPath; if (!w3dPath.empty() && w3dPath == path) { if (reason) *reason = "It is the open project"; return false; } }
    const std::vector<W3dLibRow> prev = W3dLibsRows();
    // (una que se desvinculo y vuelve con el MISMO archivo recupera su nombre: sus proxies la nombran asi)
    const std::string name = W3dLibsFreeName(FileName(path), path);
    if (W3dLibsAdd(name, path) < 0) { if (reason) *reason = "It can't be linked"; return false; }
    // UN Ctrl+Z: el registro + lo que se regenera (los proxies que la nombraban y estaban vacios)
    UndoGroupStart();
    PushUndoLibs(prev);
    W3dExtLibsInvalidate();
    if (!SimActive()) W3dProxiesLibChange(name, true);
    UndoGroupSequentialEnd();
    w3dLogf("[biblioteca] libreria vinculada: '%s' (%s)", name.c_str(), path.c_str());
    return true;
}
// las mallas del PROYECTO (sin entrar a lo que genera una instancia o un proxy) que usan una malla de 'lib'
static void LibMeshes(Object* o, const std::string& lib, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h || W3dIsInstanceType(h->getType())) continue;
        if (h->getType() == ObjectType::mesh && ((Mesh*)h)->mesh && ((Mesh*)h)->mesh->lib == lib) out.push_back((Mesh*)h);
        LibMeshes(h, lib, out);
    }
}
int W3dExtLibProjectUses(int i) {
    if (i < 0 || i >= W3dLibsCount() || !SceneCollection) return 0;
    std::vector<Mesh*> v;
    LibMeshes(SceneCollection, W3dLibsRow(i).name, v);
    return (int)v.size();
}
bool W3dExtLibUnlink(int i, int* copies) {
    if (copies) *copies = 0;
    if (i < 0 || i >= W3dLibsCount()) return false;
    const std::vector<W3dLibRow> prev = W3dLibsRows();
    const std::string name = W3dLibsRow(i).name;
    w3dLogf("[biblioteca] libreria desvinculada: '%s' (%s)", name.c_str(), W3dLibsDiskPath(i).c_str());
    // UN Ctrl+Z: las copias de sus mallas + el registro + sus proxies, que se regeneran VACIOS (igual que al reabrir el
    // nivel sin ella; lo que ya se cargo de ella sigue vivo hasta cerrar el proyecto: el Ctrl+Z los vuelve a colgar)
    UndoGroupStart();
    // LO QUE EL PROYECTO USABA DE ELLA no queda colgando: cada malla de la raiz activa que usa una malla suya (soltada
    // desde su vista) pasa a una COPIA PROPIA del proyecto, con sus datos de hoy (New Copy: sin el prefijo, se edita y
    // se guarda como cualquier otra). Va ANTES de sacarla del registro: la copia lee la malla de su almacen
    if (!SimActive() && SceneCollection) {
        std::vector<Mesh*> uses;
        LibMeshes(SceneCollection, name, uses);
        int n = 0;
        for (size_t k = 0; k < uses.size(); k++) if (W3dMeshNewCopy(uses[k])) n++;
        if (n) w3dLogf("[biblioteca] %d malla(s) del proyecto usaban mallas de '%s': ahora tienen su copia", n, name.c_str());
        if (copies) *copies = n;
    }
    W3dLibsRemove(i);
    PushUndoLibs(prev);
    W3dExtLibsInvalidate();
    if (!SimActive()) W3dProxiesLibChange(name, true);
    UndoGroupSequentialEnd();
    return true;
}

// ---- LA LECTURA (sin montar: el proyecto abierto sigue montado) ----
static std::string Base(const std::string& r) {
    const size_t p = r.find_last_of('/');
    return (p == std::string::npos) ? r : r.substr(p + 1);
}
static void Item(std::vector<W3dResourceItem>& out, int type, const std::string& id, const std::string& name,
                 const std::string& folder, const std::string& entry, int icon) {
    W3dResourceItem it;
    it.type = type; it.id = id; it.name = name; it.entry = entry;
    it.folder = W3dFolderNormalize(folder);
    it.icon = icon >= 0 ? icon : W3dIconView(type);
    it.users = 0;
    it.readOnly  = true;
    it.renamable = false;
    it.info = T("read-only library");
    out.push_back(it);
}
// un registro {"nombre", "entrada", "carpeta"} de project.json
static void Registry(JVal* root, const char* key, int type, std::vector<W3dResourceItem>& out) {
    JVal* l = JChild(root, key, 5);
    if (!l) return;
    for (size_t i = 0; i < l->list.size(); i++) {
        JVal* e = l->list[i];
        if (!e || e->type != 4) continue;
        const std::string n = JS(e, "name", "");
        if (n.empty()) continue;
        int icon = -1;
        if (type == W3D_VIEW_SCENES) icon = W3dRootIcon(W3dRootTypeByKey(JS(e, "type", "scene"), W3D_ROOT_SCENE));
        Item(out, type, n, n, JS(e, "folder", ""), JS(e, "entry", ""), icon);
    }
}
static bool Read(LinkedLib& l) {
    l.read = true; l.ok = false; l.items.clear(); l.folders.clear(); l.error.clear();
    W3dZipReader z;
    if (!z.Open(l.path)) { l.error = "It can't be opened"; return false; }
    std::vector<unsigned char> js;
    if (!z.Read("project.json", js) || js.empty()) { l.error = "It has no project.json"; return false; }
    JParser p((const char*)&js[0], js.size());
    JVal* root = p.Value();
    if (!root || root->type != 4) { delete root; l.error = "Its project.json can't be read"; return false; }
    // los registros
    Registry(root, "meshes", W3D_VIEW_MESHES, l.items);
    Registry(root, "materials", W3D_VIEW_MATERIALS, l.items);
    Registry(root, "animsets", W3D_VIEW_ANIMATIONS, l.items);
    Registry(root, "scenes3d", W3D_VIEW_SCENES, l.items);
    Registry(root, "prefabs", W3D_VIEW_PREFABS, l.items);
    if (!JChild(root, "scenes3d", 5) && JChild(root, "scene", 4))   // un proyecto de UNA escena (sin registro)
        Item(l.items, W3D_VIEW_SCENES, "Scene", "Scene", "", "", W3dRootIcon(W3D_ROOT_SCENE));
    // las carpetas de los ARCHIVOS (texturas y los demas) y las entradas de sus carpetas del contenedor
    std::map<std::string, std::string> folderOf;
    const char* regs[2] = { "textures", "files" };
    for (int k = 0; k < 2; k++) {
        JVal* l2 = JChild(root, regs[k], 5);
        if (!l2) continue;
        for (size_t i = 0; i < l2->list.size(); i++) {
            JVal* e = l2->list[i];
            if (e && e->type == 4) folderOf[JS(e, "entry", "")] = JS(e, "folder", "");
        }
    }
    std::vector<std::string> ents;
    z.List(ents);
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& e = ents[i];
        int type = -1;
        if (e.compare(0, 9, "textures/") == 0) type = W3D_VIEW_TEXTURES;
        else if (e.compare(0, 7, "sounds/") == 0) type = W3D_VIEW_SOUNDS;
        else if (e.compare(0, 8, "scripts/") == 0) type = W3D_VIEW_SCRIPTS;
        else if (e.compare(0, 6, "fonts/") == 0) type = W3D_VIEW_FONTS;
        else if (e.compare(0, 7, "videos/") == 0) type = W3D_VIEW_VIDEOS;
        else if (e.compare(0, 8, "shaders/") == 0) type = W3D_VIEW_SHADERS;
        if (type < 0 || e[e.size() - 1] == '/') continue;
        std::map<std::string, std::string>::iterator c = folderOf.find(e);
        Item(l.items, type, e, Base(e), c != folderOf.end() ? c->second : std::string(), e, -1);
    }
    // el arbol de carpetas (el unico, o el viejo por tipo)
    std::set<std::string> cs;
    JVal* jc = JChild(root, "folders", 5);
    if (jc) {
        for (size_t i = 0; i < jc->list.size(); i++)
            if (jc->list[i] && jc->list[i]->type == 2) cs.insert(W3dFolderNormalize(jc->list[i]->str));
    } else if ((jc = JChild(root, "folders", 4)) != NULL) {
        for (std::map<std::string, JVal*>::iterator it = jc->obj.begin(); it != jc->obj.end(); ++it)
            if (it->second && it->second->type == 5)
                for (size_t i = 0; i < it->second->list.size(); i++)
                    if (it->second->list[i] && it->second->list[i]->type == 2) cs.insert(W3dFolderNormalize(it->second->list[i]->str));
    }
    cs.erase(std::string());
    l.folders.assign(cs.begin(), cs.end());
    delete root;
    l.ok = true;
    w3dLogf("[biblioteca] libreria %s: %d recursos, %d carpetas", l.path.c_str(), (int)l.items.size(), (int)l.folders.size());
    return true;
}

bool W3dExtLibList(int i, std::vector<W3dResourceItem>& items, std::vector<std::string>& folders, std::string* error) {
    items.clear(); folders.clear();
    if (i < 0 || i >= W3dLibsCount()) { if (error) *error = "It isn't linked"; return false; }
    if (gLibs.size() < (size_t)W3dLibsCount()) gLibs.resize((size_t)W3dLibsCount());
    LinkedLib& l = gLibs[(size_t)i];
    const std::string path = W3dLibsDiskPath(i);
    if (l.path != path) { l.path = path; l.read = false; }
    if (!l.read) Read(l);
    if (!l.ok) { if (error) *error = l.error; return false; }
    items = l.items;
    folders = l.folders;
    return true;
}

// ---- project.json (el registro lo escribe y lo lee io/Libraries.h: aca solo se delega) ----
void W3dExtLibsCloseProject() {
    gLibs.clear();
    W3dLibsCloseProject();
}

// ---- ARRASTRAR su contenido (el proxy y las referencias a sus recursos) ----
std::string W3dExtLibGlobalId(int i, int type, const std::string& id) {
    if (i < 0 || i >= W3dLibsCount() || id.empty()) return std::string();
    const std::string& lib = W3dLibsRow(i).name;
    switch (type) {
        case W3D_VIEW_MESHES: case W3D_VIEW_MATERIALS: case W3D_VIEW_ANIMATIONS:
            // (su malla, material o animset con el prefijo de la libreria: registrado al asegurarla)
            if (id.find('/') != std::string::npos && type == W3D_VIEW_ANIMATIONS) return std::string();   // (un clip suelto)
            return W3dLibsEnsure(lib, NULL) ? lib + "/" + id : std::string();
        case W3D_VIEW_TEXTURES:   return W3dLibsEnsure(lib, NULL) ? "lib:" + lib + "/" + id : std::string();
        case W3D_VIEW_PREFABS:    return W3dLibsKey(lib, W3D_LIB_PREFAB, id);
        case W3D_VIEW_SCENES:    return W3dLibsKey(lib, W3D_LIB_SCENE, id);
    }
    return std::string();
}
