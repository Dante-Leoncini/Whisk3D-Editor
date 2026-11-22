// ============================================================================
//  Libraries.cpp — ver Libraries.h. El registro de las librerias externas, su
//  montaje (un almacen con nombre por libreria), sus recursos con prefijo y sus
//  elementos (los prefabs y escenas que un proxy genera). Compila en el editor y
//  en el runtime 3D (C++03).
// ============================================================================
#include "io/Libraries.h"
#include "io/JsonW3d.h"
#include "objects/Objects.h"           // Object::serial: las referencias pendientes van por serial
#include "io/W3dStore.h"               // StoreZip + el registro de almacenes con nombre
#include "io/UI2DFormat.h"             // g_w3dDirProject / W3dPathUnderFolder: la base de las rutas "ext:"
#include "objects/Materials.h"
#include "script/W3dScript.h"       // W3dScriptLoosePathHook: lo que un script de una libreria nombra al lado de su .w3d
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <map>
#include <set>

// ============================================================================
//  EL REGISTRO Y LO MONTADO
// ============================================================================
namespace {
// lo que se sabe de una libreria que se USO (vive hasta cerrar el proyecto, aunque se desvincule: sus proxies y
// lo que ya se cargo de ella siguen andando hasta entonces)
struct MountedLib {
    std::string path;               // el .w3d de disco que se monto ("" = empaquetada: el juego compilado)
    StoreZip* store;                // su almacen con nombre (NULL = empaquetada o no se pudo abrir)
    bool registered;                // sus recursos ya se registraron (con su prefijo)
    bool failed;                    // no se pudo leer (no se reintenta hasta que cambie su ruta)
    std::string reason;
    // sus ELEMENTOS: nombre -> entrada ("" = la escena del bloque "escena" de su project.json)
    std::vector<std::string> prefabs, prefabEntries;
    std::vector<std::string> scenes, sceneEntries;
    // OCULTA: la monto OTRA libreria (una de adentro, W3dLibsNested): no esta en el registro del proyecto. Su nombre
    // lleva '~' ("Personajes~Armas"), que un nombre del registro no puede tener: un proxy o un recurso del PROYECTO
    // nunca la nombra por error
    bool hidden;
    std::vector<W3dLibRow> sub;       // SU registro "librerias" (las de adentro), resuelto contra SU carpeta
    MountedLib() : store(0), registered(false), failed(false), hidden(false) {}
};
}
static std::vector<W3dLibRow> gRows;
static std::map<std::string, MountedLib> gMounted;
static bool gMountFromDisk = true;       // false = el juego compilado (sus librerias vienen empaquetadas)
static std::map<std::string, Material*> gMatLib;   // "Personajes/Piel" -> el material de la libreria
static std::vector<std::string> gContext;          // la pila del contexto de carga
// LIBRERIA ADENTRO DE LIBRERIA: "<de>\n<nombre>" -> el nombre con que vive en memoria. En el editor es un cache
// (se valida contra los archivos en cada uso); en el juego compilado es EL mapeo ("libs/_nested.json")
static std::map<std::string, std::string> gAlias;
static std::map<std::string, std::string> gAliasGame;
const char* const kW3dNestedLibsEntry = "libs/_nested.json";
// LAS REFERENCIAS PENDIENTES del proyecto a librerias que no estan: (serial del objeto, tipo) -> la referencia
static std::map<std::pair<unsigned, int>, W3dLibRef> gRefsPend;

int W3dLibsCount() { return (int)gRows.size(); }
const W3dLibRow& W3dLibsRow(int i) {
    static const W3dLibRow empty;
    return (i >= 0 && i < (int)gRows.size()) ? gRows[(size_t)i] : empty;
}
int W3dLibsFind(const std::string& name) {
    for (size_t i = 0; i < gRows.size(); i++) if (gRows[i].name == name) return (int)i;
    return -1;
}

static bool IsAbsolute(const std::string& r) {
    return !r.empty() && (r[0] == '/' || r[0] == '\\' || (r.size() > 1 && r[1] == ':'));
}
static void Segments(const std::string& r, std::vector<std::string>& out);   // (mas abajo)
// una ruta de disco COMPARABLE: separadores '/', sin "." ni ".." (dos rutas al mismo archivo dan lo mismo)
static std::string Normalize(const std::string& r) {
    if (r.empty()) return r;
    std::vector<std::string> t, o;
    Segments(r, t);
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == ".." && !o.empty() && o.back() != "..") o.pop_back();
        else o.push_back(t[i]);
    }
    std::string n = (r[0] == '/' || r[0] == '\\') ? "/" : "";
    for (size_t i = 0; i < o.size(); i++) { if (i) n += '/'; n += o[i]; }
    return n;
}
// la carpeta de un archivo ("" si no tiene)
static std::string FolderOf(const std::string& r) {
    const size_t b = r.find_last_of("/\\");
    return (b == std::string::npos) ? std::string() : r.substr(0, b);
}
// "ext:..." -> la ruta de disco, contra la carpeta del .w3d abierto
// (normalizada: sin "." ni "..", asi el "ext:" que se recalcula al guardar en otra carpeta sale limpio)
static std::string ResolveJson(const std::string& j) {
    std::string r = (j.compare(0, 4, "ext:") == 0) ? j.substr(4) : j;
    if (!IsAbsolute(r) && !g_w3dDirProject.empty()) return w3dFileSystem::JoinPath(g_w3dDirProject, r);
    return r;
}
std::string W3dLibsDiskPath(int i) {
    if (i < 0 || i >= (int)gRows.size()) return std::string();
    const W3dLibRow& f = gRows[(size_t)i];
    return f.diskPath.empty() ? ResolveJson(f.jsonPath) : f.diskPath;
}

// los tramos de una ruta ('/' o '\\'), sin vacios ni "."
static void Segments(const std::string& r, std::vector<std::string>& out) {
    out.clear();
    std::string t;
    for (size_t i = 0; i <= r.size(); i++) {
        const char c = (i < r.size()) ? r[i] : '/';
        if (c == '/' || c == '\\') { if (!t.empty() && t != ".") out.push_back(t); t.clear(); }
        else t += c;
    }
}
std::string W3dLibsPathForJson(const std::string& diskPath) {
    // adentro de la carpeta del .w3d: relativa (la misma regla que las referencias externas del contenedor)
    std::string rel;
    if (W3dPathUnderFolder(diskPath, g_w3dDirProject, rel)) return "ext:" + rel;
    // al lado o cerca (una carpeta hermana, "../personajes.w3d"): relativa con "..", asi el nivel y su libreria
    // se mudan juntos. Si no comparten nada mas que la raiz (otra unidad, otro arbol), absoluta
    if (IsAbsolute(diskPath) && IsAbsolute(g_w3dDirProject) && diskPath[0] == g_w3dDirProject[0]) {
        std::vector<std::string> a, b;
        Segments(g_w3dDirProject, a);
        Segments(diskPath, b);
        size_t k = 0;
        while (k < a.size() && k + 1 < b.size() && a[k] == b[k]) k++;
        if (k >= 1 && !(a.size() > 0 && a[0].size() == 2 && a[0][1] == ':' && k == 1)) {
            std::string r;
            for (size_t i = k; i < a.size(); i++) r += "../";
            for (size_t i = k; i < b.size(); i++) { r += b[i]; if (i + 1 < b.size()) r += '/'; }
            return "ext:" + r;
        }
    }
    return "ext:" + diskPath;
}

// un nombre que ya usa una libreria (vinculada, o montada en esta sesion: una desvinculada o una oculta siguen
// montadas hasta cerrar el proyecto con lo que se cargo de ellas, y OTRO archivo con su nombre las pisaria). La que
// se desvinculo y vuelve a vincularse con el MISMO archivo ('ruta') recupera su nombre (sus proxies la nombran asi)
static bool NameUsed(const std::string& n, const std::string& path = std::string()) {
    if (W3dLibsFind(n) >= 0) return true;
    std::map<std::string, MountedLib>::const_iterator it = gMounted.find(n);
    if (it == gMounted.end()) return false;
    return it->second.hidden || path.empty() || it->second.path.empty() || Normalize(it->second.path) != Normalize(path);
}

std::string W3dLibsFreeName(const std::string& base0, const std::string& path) {
    std::string base;
    for (size_t i = 0; i < base0.size(); i++) {
        const char c = base0[i];
        // ('~' es de las OCULTAS: las librerias de adentro de otra)
        base += (c == '/' || c == '\\' || c == ':' || c == '"' || c == '~') ? '_' : c;
    }
    if (base.empty()) base = "Lib";
    std::string n = base;
    for (int k = 1; NameUsed(n, path); k++) {
        char b[16]; sprintf(b, ".%03d", k);
        n = base + b;
    }
    return n;
}

// vincular (otra vez) una libreria es volver a intentar leerla: su archivo pudo volver a disco despues de un intento
// fallido (que, si no, se recuerda hasta que cambie su ruta)
static void RetryIfFailed(const std::string& name) {
    std::map<std::string, MountedLib>::iterator it = gMounted.find(name);
    if (it != gMounted.end() && it->second.failed) { it->second.failed = false; it->second.reason.clear(); }
}
int W3dLibsAdd(const std::string& name, const std::string& diskPath) {
    if (name.empty() || W3dLibsFind(name) >= 0) return -1;
    W3dLibRow f; f.name = name; f.diskPath = diskPath; f.jsonPath = W3dLibsPathForJson(diskPath);
    gRows.push_back(f);
    RetryIfFailed(name);
    return (int)gRows.size() - 1;
}
bool W3dLibsRemove(int i) {
    if (i < 0 || i >= (int)gRows.size()) return false;
    gRows.erase(gRows.begin() + (long)i);
    return true;
}
std::vector<W3dLibRow> W3dLibsRows() { return gRows; }
void W3dLibsSet(const std::vector<W3dLibRow>& rows) {
    gRows = rows;
    for (size_t i = 0; i < gRows.size(); i++) RetryIfFailed(gRows[i].name);
}

// una fila de un registro "librerias" (el del proyecto o el de adentro de una libreria): su nombre (sin nombre, el
// del archivo sin ".w3d") y su "archivo" como vino. false = no es una fila
static bool JsonRow(JVal* e, W3dLibRow& f) {
    if (!e || e->type != 4) return false;
    std::string r = JS(e, "file", "");
    if (r.empty()) r = JS(e, "path", "");   // (la primera escritura del registro, sin nombre)
    if (r.empty()) return false;
    std::string n = JS(e, "name", "");
    if (n.empty()) {
        std::string b = (r.compare(0, 4, "ext:") == 0) ? r.substr(4) : r;
        const size_t s = b.find_last_of("/\\");
        if (s != std::string::npos) b = b.substr(s + 1);
        const size_t p = b.find_last_of('.');
        if (p != std::string::npos && p > 0) b = b.substr(0, p);
        n = b;
    }
    f.name = n; f.jsonPath = r; f.diskPath.clear();
    return true;
}

// el mapeo de las librerias de adentro del JUEGO COMPILADO ("libs/_nested.json", lo escribe Compilar juego):
// {"anidadas": [{"libreria": "<de>", "nombre": "<la de adentro>", "como": "<el nombre con que vive>"}]}
static void ReadNestedGame() {
    gAliasGame.clear();
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(kW3dNestedLibsEntry, d) || d.empty()) return;
    JParser p((const char*)&d[0], d.size());
    JVal* v = p.Value();
    JVal* l = (!p.error && v && v->type == 4) ? JChild(v, "nested", 5) : NULL;
    if (l)
        for (size_t i = 0; i < l->list.size(); i++) {
            JVal* e = l->list[i];
            if (!e || e->type != 4) continue;
            const std::string of = JS(e, "lib", ""), n = JS(e, "name", ""), as = JS(e, "as", "");
            if (!of.empty() && !n.empty() && !as.empty()) gAliasGame[of + "\n" + n] = as;
        }
    delete v;
    if (!gAliasGame.empty()) w3dLogf("[librerias] %d libreria(s) de adentro de otras (empaquetadas)", (int)gAliasGame.size());
}

void W3dLibsReadJson(JVal* root, bool mountFromDisk) {
    gRows.clear();
    gMountFromDisk = mountFromDisk;
    gAlias.clear();
    if (!mountFromDisk) ReadNestedGame();
    JVal* l = root ? JChild(root, "libs", 5) : NULL;
    if (!l) return;
    for (size_t i = 0; i < l->list.size(); i++) {
        W3dLibRow f;
        if (!JsonRow(l->list[i], f)) continue;
        if (W3dLibsFind(f.name) >= 0) {
            w3dLogfW("[librerias] '%s' esta dos veces en el registro (queda la primera)", f.name.c_str());
            continue;
        }
        f.diskPath = mountFromDisk ? ResolveJson(f.jsonPath) : std::string();   // (la carpeta del .w3d ya esta fijada)
        gRows.push_back(f);
    }
}

static void JText(std::string& s, const std::string& t) {
    s += '"';
    for (size_t k = 0; k < t.size(); k++) { if (t[k] == '"' || t[k] == '\\') s += '\\'; s += t[k]; }
    s += '"';
}
void W3dLibsSaveJson(std::string& s) {
    if (gRows.empty()) return;
    s += "  \"libs\": [";
    for (size_t i = 0; i < gRows.size(); i++) {
        s += (i ? ", " : " ");
        // (el "ext:" se recalcula contra la carpeta del .w3d que se esta guardando: puede ser otra)
        const std::string file = gRows[i].diskPath.empty() ? gRows[i].jsonPath : W3dLibsPathForJson(gRows[i].diskPath);
        s += "{ \"name\": "; JText(s, gRows[i].name);
        s += ", \"file\": "; JText(s, file);
        s += " }";
    }
    s += " ],\n";
}

void W3dLibsCloseProject() {
    for (std::map<std::string, MountedLib>::iterator it = gMounted.begin(); it != gMounted.end(); ++it) {
        if (!it->second.store) continue;
        W3dStoreRegister(it->first, NULL);
        delete it->second.store;
        it->second.store = 0;
    }
    gMounted.clear();
    gRows.clear();
    gMatLib.clear();   // (los materiales se liberan con el proyecto: MaterialsFreeScene)
    gContext.clear();
    gAlias.clear();
    gAliasGame.clear();
    gRefsPend.clear();
    gMountFromDisk = true;
}

// ============================================================================
//  USARLA
// ============================================================================
// sus prefabs y escenas, del project.json de la libreria
static void ReadElements(MountedLib& L, JVal* root) {
    L.prefabs.clear(); L.prefabEntries.clear(); L.scenes.clear(); L.sceneEntries.clear();
    JVal* jp = JChild(root, "prefabs", 5);
    if (jp)
        for (size_t i = 0; i < jp->list.size(); i++) {
            JVal* e = jp->list[i];
            if (!e || e->type != 4) continue;
            const std::string n = JS(e, "name", ""), ent = JS(e, "entry", "");
            if (n.empty() || ent.empty()) continue;
            L.prefabs.push_back(n); L.prefabEntries.push_back(ent);
        }
    JVal* je = JChild(root, "scenes3d", 5);
    if (je) {
        for (size_t i = 0; i < je->list.size(); i++) {
            JVal* e = je->list[i];
            if (!e || e->type != 4) continue;
            const std::string n = JS(e, "name", "");
            if (n.empty()) continue;
            L.scenes.push_back(n); L.sceneEntries.push_back(JS(e, "entry", ""));   // "" = la del bloque
        }
    } else if (JChild(root, "scene", 4)) {
        // un proyecto de UNA escena (sin registro): la del bloque, con el nombre por defecto
        L.scenes.push_back("Scene"); L.sceneEntries.push_back(std::string());
    }
}

// SU registro "librerias" (las de adentro): los "ext:" relativos son relativos a SU carpeta
static void ReadSub(MountedLib& L, JVal* root) {
    L.sub.clear();
    JVal* l = JChild(root, "libs", 5);
    if (!l) return;
    const std::string dir = FolderOf(L.path);
    for (size_t i = 0; i < l->list.size(); i++) {
        W3dLibRow f;
        if (!JsonRow(l->list[i], f)) continue;
        bool rep = false;
        for (size_t k = 0; k < L.sub.size() && !rep; k++) rep = (L.sub[k].name == f.name);
        if (rep) continue;
        if (gMountFromDisk && !L.path.empty()) {
            const std::string x = (f.jsonPath.compare(0, 4, "ext:") == 0) ? f.jsonPath.substr(4) : f.jsonPath;
            f.diskPath = Normalize(IsAbsolute(x) ? x : (dir.empty() ? x : dir + "/" + x));
        }
        L.sub.push_back(f);
    }
}

// monta la libreria 'nombre' (su .w3d, como almacen con su nombre) si hace falta y se puede
static bool Mount(const std::string& name, MountedLib& L, std::string* reason) {
    const int i = W3dLibsFind(name);
    const std::string path = (i >= 0) ? W3dLibsDiskPath(i) : L.path;
    if (L.store && L.path == path) return true;
    if (L.store) {   // (la ruta cambio: se vuelve a montar)
        W3dStoreRegister(name, NULL);
        delete L.store;
        L.store = 0;
    }
    L.path = path;
    if (!gMountFromDisk || path.empty()) return true;   // (empaquetada: la lee el fallback de ReadFileBytes)
    StoreZip* a = new StoreZip();
    if (!a->Open(path) || !a->Exists("project.json")) {
        delete a;
        if (reason) *reason = "The library file can't be opened";
        w3dLogfE("[librerias] no pude montar '%s' (%s)", name.c_str(), path.c_str());
        return false;
    }
    L.store = a;
    W3dStoreRegister(name, a);
    w3dLogf("[librerias] montada '%s' (%s)", name.c_str(), path.c_str());
    return true;
}

// W3dStoreRequestHook (io/W3dStore.h): alguien lee "lib:<nombre>/..." de una libreria VINCULADA que todavia no
// se monto (la textura de un material del proyecto que nombra una suya): solo se MONTA (sus recursos se registran
// cuando algo los pide por nombre). En el juego compilado no se monta nada: la copia empaquetada
static bool RequestStore(const std::string& name) {
    if (!gMountFromDisk) return false;
    std::map<std::string, MountedLib>::iterator it = gMounted.find(name);
    const bool hidden = W3dLibsFind(name) < 0 && it != gMounted.end() && it->second.hidden;
    if (W3dLibsFind(name) < 0 && !hidden) return false;
    MountedLib& L = gMounted[name];
    if (L.failed && L.path == W3dLibsDiskPathOf(name)) return false;
    std::string m;
    if (!Mount(name, L, &m)) { L.failed = true; L.reason = m; return false; }
    return L.store != NULL;
}
namespace {
struct RegisterRequestStore {
    RegisterRequestStore() {
        W3dStoreRequestHook = RequestStore;
        W3dScriptLoosePathHook = W3dLibsLoosePath;   // (un archivo suelto al lado del .w3d de una libreria)
    }
} gRegisterRequestStore;
}

bool W3dLibsEnsure(const std::string& name, std::string* reason) {
    if (name.empty()) { if (reason) *reason = "No library"; return false; }
    std::map<std::string, MountedLib>::iterator it = gMounted.find(name);
    const bool linked = W3dLibsFind(name) >= 0;
    const bool hidden = !linked && it != gMounted.end() && it->second.hidden;
    // una DESVINCULADA ya no sirve para generar nada (lo que ya se cargo de ella sigue vivo hasta cerrar el proyecto:
    // sus proxies se regeneran vacios, igual que al reabrir el nivel sin ella)
    if (!linked && !hidden) { if (reason) *reason = "The library isn't linked"; return false; }
    if (linked && it != gMounted.end()) it->second.hidden = false;
    // YA LISTA
    if (it != gMounted.end() && it->second.registered) {
        if (linked && W3dLibsDiskPath(W3dLibsFind(name)) != it->second.path && gMountFromDisk) {
            // (la misma libreria con OTRO archivo: se vuelve a leer)
            it->second.registered = false;
            it->second.failed = false;
        } else return true;
    }
    if (it == gMounted.end()) it = gMounted.insert(std::make_pair(name, MountedLib())).first;
    MountedLib& L = it->second;
    if (L.failed && L.path == W3dLibsDiskPathOf(name)) { if (reason) *reason = L.reason; return false; }
    L.failed = false;
    if (!Mount(name, L, reason)) { L.failed = true; L.reason = reason ? *reason : std::string(); return false; }
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes("lib:" + name + "/project.json", data) || data.empty()) {
        L.failed = true; L.reason = "The library has no project.json";
        if (reason) *reason = L.reason;
        w3dLogfE("[librerias] '%s': no pude leer su project.json", name.c_str());
        return false;
    }
    JParser p((const char*)&data[0], data.size());
    JVal* root = p.Value();
    if (p.error || !root || root->type != 4) {
        delete root;
        L.failed = true; L.reason = "The library's project.json can't be read";
        if (reason) *reason = L.reason;
        w3dLogfE("[librerias] '%s': su project.json no parsea", name.c_str());
        return false;
    }
    ReadElements(L, root);
    ReadSub(L, root);   // (sus librerias de adentro: lo que su contenido nombra de ellas se resuelve con ESTE registro)
    // los materiales, las mallas y los animsets, con su prefijo (el lector: import_w3d)
    W3dLibRegisterResources(name, root);
    delete root;
    L.registered = true;
    w3dLogf("[librerias] '%s' lista: %d prefab(s), %d escena(s)", name.c_str(), (int)L.prefabs.size(),
            (int)L.scenes.size());
    w3dLogMemory("libreria lista");
    return true;
}

std::string W3dLibsByName(const std::string& n) {
    const size_t bar = n.find('/');
    if (bar == std::string::npos || bar == 0) return std::string();
    const std::string lib = n.substr(0, bar);
    if (W3dLibsFind(lib) < 0 && gMounted.find(lib) == gMounted.end()) return std::string();
    return W3dLibsEnsure(lib, NULL) ? lib : std::string();
}

std::string W3dLibsDiskPathOf(const std::string& name) {
    const int i = W3dLibsFind(name);
    if (i >= 0) return W3dLibsDiskPath(i);
    std::map<std::string, MountedLib>::const_iterator it = gMounted.find(name);
    return (it == gMounted.end()) ? std::string() : it->second.path;
}
bool W3dLibsFromDisk() { return gMountFromDisk; }

std::string W3dLibsExternalEntry(const std::string& rel) {
    std::vector<std::string> t, o;
    Segments(rel, t);
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == ".." && !o.empty() && o.back() != "__") o.pop_back();
        else o.push_back(t[i] == ".." ? std::string("__") : t[i]);
    }
    std::string e = "_ext";
    for (size_t i = 0; i < o.size(); i++) e += "/" + o[i];
    return e;
}

std::string W3dLibsLoosePath(const std::string& lib, const std::string& rel) {
    if (!gMountFromDisk || lib.empty() || rel.empty() || IsAbsolute(rel)) return std::string();
    const std::string dir = FolderOf(W3dLibsDiskPathOf(lib));
    if (dir.empty()) return std::string();
    const std::string r = Normalize(dir + "/" + rel);
    // (nunca afuera de su carpeta: lo que un script nombra de al lado de su libreria, no del disco entero)
    std::string inside;
    if (!W3dPathUnderFolder(r, Normalize(dir), inside)) return std::string();
    return w3dFileSystem::FileExists(r) ? r : std::string();
}

// ============================================================================
//  LIBRERIA ADENTRO DE LIBRERIA
// ============================================================================
void W3dLibsSubRows(const std::string& lib, std::vector<W3dLibRow>& out) {
    out.clear();
    if (!W3dLibsEnsure(lib, NULL)) return;
    out = gMounted[lib].sub;
}

// el alias cacheado sigue valiendo: 'g' es la vinculada con ese archivo, o una oculta montada con el
static bool AliasVale(const std::string& g, const std::string& path) {
    const int i = W3dLibsFind(g);
    if (i >= 0) return Normalize(W3dLibsDiskPath(i)) == path;
    std::map<std::string, MountedLib>::const_iterator it = gMounted.find(g);
    return it != gMounted.end() && it->second.hidden && Normalize(it->second.path) == path;
}

std::string W3dLibsNested(const std::string& of, const std::string& name) {
    if (of.empty() || name.empty()) return name;
    const std::string key = of + "\n" + name;
    if (!gMountFromDisk) {
        // EL JUEGO COMPILADO: el mapeo que dejo Compilar juego (una oculta queda conocida para asegurarla)
        std::map<std::string, std::string>::const_iterator a = gAliasGame.find(key);
        if (a == gAliasGame.end()) return name;
        if (W3dLibsFind(a->second) < 0) gMounted[a->second].hidden = true;
        return a->second;
    }
    // EL EDITOR: por el ARCHIVO de su registro
    if (!W3dLibsEnsure(of, NULL)) return name;
    const std::vector<W3dLibRow>& sub = gMounted[of].sub;
    std::string path;
    bool is = false;
    for (size_t i = 0; i < sub.size() && !is; i++)
        if (sub[i].name == name) { is = true; path = sub[i].diskPath; }
    if (!is || path.empty()) return name;   // (no es de su registro: la del nivel con ese nombre, si hay)
    std::string g;
    // 1) el NIVEL vincula ese mismo archivo: es esa (comparten memoria). Se mira SIEMPRE antes que el cache: el nivel
    //    la puede haber vinculado despues de que se monto oculta (lo que se genere desde ahora usa la del nivel)
    for (size_t i = 0; i < gRows.size() && g.empty(); i++)
        if (Normalize(W3dLibsDiskPath((int)i)) == path) g = gRows[i].name;
    if (!g.empty()) { gAlias[key] = g; return g; }
    std::map<std::string, std::string>::const_iterator c = gAlias.find(key);
    if (c != gAlias.end() && AliasVale(c->second, path)) return c->second;
    // 2) ya hay una OCULTA con ese archivo (otra libreria la trae tambien): esa
    for (std::map<std::string, MountedLib>::iterator it = gMounted.begin(); it != gMounted.end() && g.empty(); ++it)
        if (it->second.hidden && W3dLibsFind(it->first) < 0 && Normalize(it->second.path) == path) g = it->first;
    // 3) una OCULTA nueva: "<de>~<nombre>" (o uno libre)
    if (g.empty()) {
        const std::string base = of + "~" + name;
        g = base;
        for (int k = 1; NameUsed(g); k++) { char b[16]; sprintf(b, ".%03d", k); g = base + b; }
        MountedLib& N = gMounted[g];
        N.hidden = true;
        N.path = path;
        w3dLogf("[librerias] '%s' de adentro de '%s' se monta oculta como '%s' (%s)", name.c_str(), of.c_str(),
                g.c_str(), path.c_str());
    }
    gAlias[key] = g;
    return g;
}

std::string W3dLibsNameNested(const std::string& of, const std::string& n) {
    if (of.empty()) return std::string();
    const size_t bar = n.find('/');
    if (bar == std::string::npos || bar == 0 || bar + 1 >= n.size()) return std::string();
    const std::string p   = n.substr(0, bar);
    bool isFromSuRegistry = false;
    if (!gMountFromDisk) isFromSuRegistry = gAliasGame.find(of + "\n" + p) != gAliasGame.end();
    else if (W3dLibsEnsure(of, NULL)) {
        const std::vector<W3dLibRow>& sub = gMounted[of].sub;
        for (size_t i = 0; i < sub.size() && !isFromSuRegistry; i++) isFromSuRegistry = (sub[i].name == p);
    }
    if (!isFromSuRegistry) return std::string();
    const std::string g = W3dLibsNested(of, p);
    W3dLibsEnsure(g, NULL);   // (sus recursos se registran al usarla)
    return g + n.substr(bar);
}

// ============================================================================
//  LOS MATERIALES DE LAS LIBRERIAS
// ============================================================================
Material* W3dLibMaterial(const std::string& n) {
    std::map<std::string, Material*>::const_iterator it = gMatLib.find(n);
    return it == gMatLib.end() ? NULL : it->second;
}
void W3dLibMaterialRegister(const std::string& n, Material* m) {
    if (n.empty()) return;
    if (m) gMatLib[n] = m; else gMatLib.erase(n);
}

// ============================================================================
//  SUS ELEMENTOS
// ============================================================================
void W3dLibsElements(const std::string& lib, int type, std::vector<std::string>& out) {
    out.clear();
    if (!W3dLibsEnsure(lib, NULL)) return;
    const MountedLib& L = gMounted[lib];
    out = (type == W3D_LIB_SCENE) ? L.scenes : L.prefabs;
}
bool W3dLibsHasElement(const std::string& lib, int type, const std::string& elem) {
    // (un OBJETO de una escena: "Escena#Padre/Hijo" -> la escena tiene que estar; el objeto lo busca la generacion)
    const size_t h = (type == W3D_LIB_SCENE) ? elem.find('#') : std::string::npos;
    const std::string e = (h == std::string::npos) ? elem : elem.substr(0, h);
    std::vector<std::string> v;
    W3dLibsElements(lib, type, v);
    for (size_t i = 0; i < v.size(); i++) if (v[i] == e) return true;
    return false;
}

std::string W3dLibsKey(const std::string& lib, int type, const std::string& elem) {
    if (lib.empty() || elem.empty()) return std::string();
    return "lib:" + lib + (type == W3D_LIB_SCENE ? "/scene/" : "/prefab/") + elem;
}
bool W3dLibsByKey(const std::string& c, std::string* lib, int* type, std::string* elem) {
    if (c.size() < 6 || c.compare(0, 4, "lib:") != 0) return false;
    const size_t b1 = c.find('/', 4);
    if (b1 == std::string::npos || b1 == 4) return false;
    const size_t b2 = c.find('/', b1 + 1);
    if (b2 == std::string::npos || b2 + 1 >= c.size()) return false;
    const std::string t = c.substr(b1 + 1, b2 - b1 - 1);
    if (t != "prefab" && t != "scene") return false;
    if (lib) *lib = c.substr(4, b1 - 4);
    if (type) *type = (t == "scene") ? W3D_LIB_SCENE : W3D_LIB_PREFAB;
    if (elem) *elem = c.substr(b2 + 1);
    return true;
}

static JVal* Parse(const std::string& path) {
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(path, d) || d.empty()) {
        w3dLogfE("[librerias] no pude leer '%s'", path.c_str());
        return NULL;
    }
    JParser p((const char*)&d[0], d.size());
    JVal* v = p.Value();
    if (p.error || !v || v->type != 4) {
        w3dLogfE("[librerias] '%s' no parsea", path.c_str());
        delete v;
        return NULL;
    }
    return v;
}
// saca 'clave' de un objeto JSON sin borrarla (pasa a ser de quien la pide)
static JVal* Start(JVal* o, const char* key) {
    if (!o || o->type != 4) return NULL;
    std::map<std::string, JVal*>::iterator it = o->obj.find(key);
    if (it == o->obj.end()) return NULL;
    JVal* v = it->second;
    o->obj.erase(it);
    return v;
}
static JVal* Text(const std::string& t) { JVal* v = new JVal(); v->type = 2; v->str = t; return v; }

// UN OBJETO de una lista de objetos del .w3d por su RUTA de nombres ("Padre/Hijo"): se SACA de la lista (pasa a ser de
// quien lo pide). NULL = no esta
static JVal* StartObject(JVal* list, const std::string& path) {
    if (!list || list->type != 5 || path.empty()) return NULL;
    const size_t b = path.find('/');
    const std::string name = (b == std::string::npos) ? path : path.substr(0, b);
    for (size_t i = 0; i < list->list.size(); i++) {
        JVal* o = list->list[i];
        if (!o || o->type != 4) continue;
        std::map<std::string, JVal*>::iterator n = o->obj.find("name");
        if (n == o->obj.end() || !n->second || n->second->str != name) continue;
        if (b == std::string::npos) { list->list.erase(list->list.begin() + (long)i); return o; }
        std::map<std::string, JVal*>::iterator h = o->obj.find("children");
        return (h == o->obj.end()) ? NULL : StartObject(h->second, path.substr(b + 1));
    }
    return NULL;
}
// los objetos de primer nivel de la escena 'escena' de la libreria (NULL = no se pudo leer). Es de quien la pide
static JVal* SceneObjects(const std::string& lib, const std::string& entry) {
    JVal* doc = Parse("lib:" + lib + "/" + (entry.empty() ? std::string("project.json") : entry));
    if (!doc) return NULL;
    JVal* objs = NULL;
    if (entry.empty()) { JVal* esc = JChild(doc, "scene", 4); objs = Start(esc, "objects"); }
    else objs = Start(doc, "objects");
    delete doc;
    return objs;
}
// los nombres (y si tienen hijos) de los objetos de una escena de la libreria en la ruta 'ruta' ("" = los de primer nivel)
static void CollectObjects(JVal* list, const std::string& path, std::vector<W3dLibObject>& out) {
    if (!list || list->type != 5) return;
    const size_t b = path.find('/');
    const std::string name = (b == std::string::npos) ? path : path.substr(0, b);
    for (size_t i = 0; i < list->list.size(); i++) {
        JVal* o = list->list[i];
        if (!o || o->type != 4) continue;
        std::map<std::string, JVal*>::iterator n = o->obj.find("name");
        const std::string objName = (n != o->obj.end() && n->second) ? n->second->str : std::string();
        std::map<std::string, JVal*>::iterator h = o->obj.find("children");
        JVal* children = (h != o->obj.end() && h->second && h->second->type == 5) ? h->second : NULL;
        if (path.empty()) {
            W3dLibObject lo; lo.name = objName;
            std::map<std::string, JVal*>::iterator t = o->obj.find("type");
            lo.type = (t != o->obj.end() && t->second) ? t->second->str : std::string("object");
            lo.withChildren = children && !children->list.empty();
            out.push_back(lo);
        } else if (objName == name) {
            CollectObjects(children, b == std::string::npos ? std::string() : path.substr(b + 1), out);
            return;
        }
    }
}
bool W3dLibsSceneObjects(const std::string& lib, const std::string& scene, const std::string& path, std::vector<W3dLibObject>& out) {
    out.clear();
    std::string reason;
    if (!W3dLibsEnsure(lib, &reason)) return false;
    const MountedLib& L = gMounted[lib];
    int k = -1;
    for (size_t i = 0; i < L.scenes.size(); i++) if (L.scenes[i] == scene) { k = (int)i; break; }
    if (k < 0) return false;
    JVal* objs = SceneObjects(lib, L.sceneEntries[(size_t)k]);
    if (!objs) return false;
    CollectObjects(objs, path, out);
    delete objs;
    return true;
}

JVal* W3dLibsReadElement(const std::string& lib, int type, const std::string& elem0, JVal** rootObj) {
    if (rootObj) *rootObj = NULL;
    // UN OBJETO de una escena ("Escena#Padre/Hijo"): ese objeto con todo lo que cuelga de el (sus hijos, su malla, sus
    // materiales, su esqueleto y sus animaciones, que se buscan por nombre en la libreria)
    std::string elem = elem0, objPath;
    if (type == W3D_LIB_SCENE) {
        const size_t h = elem0.find('#');
        if (h != std::string::npos) { elem = elem0.substr(0, h); objPath = elem0.substr(h + 1); }
    }
    std::string reason;
    if (!W3dLibsEnsure(lib, &reason)) {
        w3dLogfW("[librerias] '%s/%s': %s", lib.c_str(), elem.c_str(), reason.c_str());
        return NULL;
    }
    const MountedLib& L = gMounted[lib];
    const std::vector<std::string>& ns = (type == W3D_LIB_SCENE) ? L.scenes : L.prefabs;
    const std::vector<std::string>& is = (type == W3D_LIB_SCENE) ? L.sceneEntries : L.prefabEntries;
    int k = -1;
    for (size_t i = 0; i < ns.size(); i++) if (ns[i] == elem) { k = (int)i; break; }
    if (k < 0) {
        w3dLogfW("[librerias] la libreria '%s' no tiene %s '%s'", lib.c_str(),
                 type == W3D_LIB_SCENE ? "la escena" : "el prefab", elem.c_str());
        return NULL;
    }
    const std::string entry = is[(size_t)k];
    if (type == W3D_LIB_PREFAB) {
        JVal* doc = Parse("lib:" + lib + "/" + entry);
        if (!doc) return NULL;
        JVal* r = JChild(doc, "root", 4);
        if (!r) { w3dLogfW("[librerias] el prefab '%s/%s' no tiene objeto raiz", lib.c_str(), elem.c_str()); delete doc; return NULL; }
        if (rootObj) *rootObj = r;
        return doc;
    }
    // UNA ESCENA: sus objetos de primer nivel cuelgan de un VACIO con el nombre de la escena (lo que genera el
    // proxy es UN objeto raiz, como un prefab: los overrides por ruta y la frontera de scope andan igual)
    JVal* objs = SceneObjects(lib, entry);
    if (!objs) return NULL;
    if (!objPath.empty()) {
        JVal* o = StartObject(objs, objPath);
        delete objs;
        if (!o) { w3dLogfW("[librerias] la escena '%s/%s' no tiene el objeto '%s'", lib.c_str(), elem.c_str(), objPath.c_str()); return NULL; }
        JVal* newValue = new JVal(); newValue->type = 4;
        newValue->obj["root"] = o;
        if (rootObj) *rootObj = o;
        return newValue;
    }
    JVal* newValue = new JVal(); newValue->type = 4;
    JVal* r = new JVal(); r->type = 4;
    r->obj["type"] = Text("object");
    r->obj["name"] = Text(elem);
    if (objs && objs->type == 5) r->obj["children"] = objs;
    else delete objs;
    newValue->obj["root"] = r;
    if (rootObj) *rootObj = r;
    return newValue;
}

// ============================================================================
//  EL CONTEXTO DE CARGA
// ============================================================================
void W3dLibContextEnter(const std::string& lib) { gContext.push_back(lib); }
void W3dLibContextExit() { if (!gContext.empty()) gContext.pop_back(); }
const std::string& W3dLibContext() {
    static const std::string empty;
    return gContext.empty() ? empty : gContext.back();
}

// ============================================================================
//  LAS REFERENCIAS DEL PROYECTO A UNA LIBRERIA QUE NO ESTA
// ============================================================================
void W3dLibRefAnnotate(const Object* o, int type, const W3dLibRef& ref) {
    if (o && !ref.name.empty()) gRefsPend[std::make_pair(o->serial, type)] = ref;
}
const W3dLibRef* W3dLibRefOf(const Object* o, int type) {
    if (!o || gRefsPend.empty()) return NULL;
    std::map<std::pair<unsigned, int>, W3dLibRef>::const_iterator it = gRefsPend.find(std::make_pair(o->serial, type));
    return (it == gRefsPend.end()) ? NULL : &it->second;
}
void W3dLibRefForget(const Object* o, int type) {
    if (o) gRefsPend.erase(std::make_pair(o->serial, type));
}
void W3dLibRefsSerials(const std::string& lib, int type, std::vector<unsigned>& out) {
    out.clear();
    for (std::map<std::pair<unsigned, int>, W3dLibRef>::const_iterator it = gRefsPend.begin(); it != gRefsPend.end(); ++it)
        if (it->first.second == type && (lib.empty() || W3dLibPrefixOf(it->second.name) == lib)) out.push_back(it->first.first);
}
std::string W3dLibPrefixOf(const std::string& n) {
    const size_t bar = n.find('/');
    if (bar == std::string::npos || bar == 0 || bar + 1 >= n.size()) return std::string();
    return n.substr(0, bar);
}
