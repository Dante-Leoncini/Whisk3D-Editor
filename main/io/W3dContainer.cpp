// ============================================================================
//  W3dContainer.cpp — ver W3dContainer.h.
// ============================================================================
#include "io/W3dContainer.h"
#include "io/W3dZip.h"
#include "io/W3dStore.h"
#include "io/UI2DFormat.h"   // W3dPathUnderFolder: LA regla de "esta ruta cuelga de esta carpeta"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>   // std::sort
#ifdef _WIN32
    #include <direct.h>
    #define W3D_MKDIR(p) _mkdir(p)
#else
    #include <sys/stat.h>
    #define W3D_MKDIR(p) mkdir(p, 0755)
#endif

// ---------------------------------------------------------------------------
//  CRC32 (el mismo polinomio del zip): la HUELLA con la que se deduplica por
//  contenido. El del W3dZip.cpp es static; aca hay una copia chica en vez de
//  exportar simbolos nuevos del Core.
// ---------------------------------------------------------------------------
static unsigned gTable[256];
static bool     gListTable = false;
static void TableCrc() {
    if (gListTable) return;
    for (unsigned i = 0; i < 256; i++) {
        unsigned c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        gTable[i] = c;
    }
    gListTable = true;
}
static unsigned Crc32(const unsigned char* d, size_t n) {
    TableCrc();
    unsigned c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = gTable[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// ===========================================================================
//  ENTRADAS DE SERVICIO (mimetype / README.txt / project.json / EXTERNALS.txt)
// ===========================================================================
// 35 bytes EXACTOS, sin salto de linea final: es lo que queda en el offset 38
// del zip, que es donde file(1) y xdg-mime buscan la firma de un OpenDocument.
const char* const kW3dMimetype = "application/vnd.whisk3d.project+zip";

bool W3dIsServiceEntry(const std::string& n) {
    return n == "mimetype" || n == "README.txt" ||
           n == "project.json" || n == "EXTERNALS.txt";
}

// TEXTO FIJO: ni fecha, ni usuario, ni rutas de la maquina. Cualquier campo
// volatil aca rompe el round-trip byte a byte del guardado (y con el, el diff
// en git, el dedup entre versiones y los tests).
std::string W3dContainerReadme() {
    std::string s;
    s += "Esto es un proyecto de Whisk3D.\n";
    s += "\n";
    s += "El archivo .w3d es un ZIP comun: lo estas mirando con un descompresor y\n";
    s += "eso es a proposito. Adentro esta el proyecto ENTERO y nada mas.\n";
    s += "\n";
    s += "  mimetype        el tipo de archivo. Va primero y sin comprimir (igual\n";
    s += "                  que un .odt de OpenDocument). No lo edites.\n";
    s += "  README.txt      este texto. Se regenera solo en cada guardado.\n";
    s += "  project.json    EL PROYECTO: objetos, escenas, materiales, animaciones.\n";
    s += "                  Es texto y se puede leer y editar a mano.\n";
    s += "  scenes/         las escenas de interfaz (.w3dui)\n";
    s += "  meshes/         las mallas 3D del proyecto (.w3db binario, o .w3dm de\n";
    s += "                  texto si el proyecto lo pide): UNA por malla, compartida\n";
    s += "                  por todos los objetos que la usan (registro \"meshes\"\n";
    s += "                  del project.json; cada objeto dice \"mesh\": <nombre>)\n";
    s += "  animations/     los keyframes de vertices y los clips de esqueleto\n";
    s += "                  (.w3da: una biblioteca por esqueleto, binario)\n";
    s += "  scripts/        el codigo Lua\n";
    s += "  textures/  fonts/  sounds/  videos/    los assets, por tipo\n";
    s += "  models/         los .obj y .fbx originales que importaste\n";
    s += "  project/        el icono del juego y su arte fuente\n";
    s += "  extra/          lo que el editor no reconocio\n";
    s += "  EXTERNALS.txt   aparece SOLO si el proyecto usa archivos de afuera\n";
    s += "\n";
    s += "Cada carpeta dice QUE TIPO de cosa hay adentro, no de quien es: una\n";
    s += "textura usada por tres escenas es UNA sola entrada.\n";
    s += "\n";
    s += "Si agregas archivos tuyos aca adentro, Whisk3D los deja donde estan (no\n";
    s += "los borra al guardar), salvo en meshes/ y animations/, que son del\n";
    s += "editor: ahi se descarta lo que project.json no nombra. Una malla del\n";
    s += "registro \"meshes\" o una biblioteca de clips registrada se conserva\n";
    s += "aunque ningun objeto la use (huerfana); las mallas iguales de objetos\n";
    s += "distintos se guardan UNA sola vez.\n";
    return s;
}

// ===========================================================================
//  MONTAJE
// ===========================================================================
// ---------------------------------------------------------------------------
//  EL ALMACEN MONTADO = el zip del disco MAS las entradas editadas en caliente.
//
//  Sin esta capa, "guardar el .lua que estas editando" no tenia a donde ir: el
//  IDE hacia fopen("scripts/menu.lua") y eso crea un archivo RELATIVO AL
//  DIRECTORIO ACTUAL (una carpeta scripts/ al lado del binario del editor), o sea
//  que el cambio no llegaba nunca al proyecto y encima ensuciaba el disco.
//  Con el overlay, el zip sigue siendo de solo lectura y lo editado se sirve de
//  memoria: todo el editor lo ve por ReadFileBytes sin cambiar un solo call site.
// ---------------------------------------------------------------------------
class StoreContainer : public W3dStore {
public:
    virtual bool Open(const std::string& path) { return zip.Open(path); }
    virtual bool Exists(const std::string& e) const {
        return edited.find(e) != edited.end() || zip.Exists(e);
    }
    virtual bool Read(const std::string& e, std::vector<unsigned char>& out) {
        std::map<std::string, std::vector<unsigned char> >::const_iterator it = edited.find(e);
        if (it != edited.end()) { out = it->second; return true; }
        return zip.Read(e, out);
    }
    virtual bool ReadRange(const std::string& e, unsigned from, unsigned n, std::vector<unsigned char>& out) {
        std::map<std::string, std::vector<unsigned char> >::const_iterator it = edited.find(e);
        if (it == edited.end()) return zip.ReadRange(e, from, n, out);
        out.clear();
        if (from > it->second.size() || it->second.size() - from < n) return false;
        out.assign(it->second.begin() + from, it->second.begin() + from + n);
        return true;
    }
    virtual void List(std::vector<std::string>& out) const {
        zip.List(out);
        for (std::map<std::string, std::vector<unsigned char> >::const_iterator it = edited.begin();
             it != edited.end(); ++it) {
            bool already = false;
            for (size_t i = 0; i < out.size() && !already; i++) if (out[i] == it->first) already = true;
            if (!already) out.push_back(it->first);
        }
    }
    virtual unsigned Size(const std::string& e) const {
        std::map<std::string, std::vector<unsigned char> >::const_iterator it = edited.find(e);
        if (it != edited.end()) return (unsigned)it->second.size();
        return zip.Size(e);
    }
    virtual void Close() { zip.Close(); edited.clear(); }

    StoreZip zip;
    std::map<std::string, std::vector<unsigned char> > edited;
};

static StoreContainer* gStore = NULL;
static std::string  gMountedPath;
std::string W3dContainerMountedPath() { return gMountedPath; }

bool W3dContainerHasMounted() { return gStore != NULL; }
bool W3dContainerInMemory() { return gStore != NULL && gMountedPath.empty(); }
bool W3dContainerEnsure() {
    if (gStore) return true;
    gStore = new StoreContainer();   // (su zip no abre nada: todo lo que tiene son las entradas editadas)
    gMountedPath.clear();
    W3dStoreMount(gStore);
    w3dLogf("[W3D] contenedor en memoria: el proyecto todavia no es un archivo");
    return true;
}
// (el de memoria no tiene zip: para el guardado es un proyecto que se escribe por primera vez)
W3dZipReader* W3dContainerReader() { return (gStore && !gMountedPath.empty()) ? &gStore->zip.Reader() : NULL; }

bool W3dContainerWriteEntry(const std::string& entry, const void* data, size_t size) {
    if (!gStore || !W3dIsEntryName(entry)) return false;
    std::vector<unsigned char>& d = gStore->edited[entry];
    d.assign((const unsigned char*)data, (const unsigned char*)data + size);
    w3dLogf("[W3D] entrada editada en memoria: %s (%u bytes; se hornea al guardar el proyecto)",
            entry.c_str(), (unsigned)size);
    return true;
}
bool W3dContainerEntryEdited(const std::string& entry) {
    return gStore && gStore->edited.find(entry) != gStore->edited.end();
}
bool W3dContainerReadEdited(const std::string& entry, std::vector<unsigned char>& out) {
    if (!gStore) return false;
    std::map<std::string, std::vector<unsigned char> >::const_iterator it =
        gStore->edited.find(entry);
    if (it == gStore->edited.end()) return false;
    out = it->second;
    return true;
}
size_t W3dEditedContainerCount() { return gStore ? gStore->edited.size() : 0; }
bool W3dContainerMoveEdited(const std::string& old, const std::string& newValue) {
    if (!gStore || !W3dIsEntryName(newValue) || old == newValue) return false;
    std::map<std::string, std::vector<unsigned char> >::iterator it = gStore->edited.find(old);
    if (it == gStore->edited.end()) return false;
    std::vector<unsigned char> d;
    d.swap(it->second);                    // los bytes cambian de dueno, no se copian
    gStore->edited.erase(it);
    gStore->edited[newValue].swap(d);
    w3dLogf("[W3D] entrada editada mudada: %s -> %s", old.c_str(), newValue.c_str());
    return true;
}
bool W3dContainerExists(const std::string& entry) {
    return gStore && W3dIsEntryName(entry) && gStore->Exists(entry);
}

void W3dContainerListFolder(const std::string& folder, std::vector<std::string>& out) {
    out.clear();
    if (!gStore) return;
    std::vector<std::string> all;
    gStore->List(all);
    for (size_t i = 0; i < all.size(); i++)
        if (all[i].size() > folder.size() &&
            all[i].compare(0, folder.size(), folder) == 0)
            out.push_back(all[i]);
    // ORDEN ALFABETICO: la lista que ve el usuario no puede depender del orden del zip
    std::sort(out.begin(), out.end());
}

// crea la ruta de carpetas completa ("a/b/c"): mkdir por segmento (existente = ok)
static void MkdirRec(const std::string& dir) {
    for (size_t i = 1; i < dir.size(); i++)
        if (dir[i] == '/' || dir[i] == '\\') W3D_MKDIR(dir.substr(0, i).c_str());
    W3D_MKDIR(dir.c_str());
}

bool W3dContainerExtract(const std::string& entry, const std::string& diskPath) {
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(entry, data)) {
        w3dLogfE("[W3D] extraer: no pude leer la entrada %s", entry.c_str());
        return false;
    }
    // la carpeta del destino ("<dir>/texturas") se crea sola: el arbol de adentro
    // del .w3d se reproduce tal cual afuera
    { size_t sb = diskPath.find_last_of("/\\");
      if (sb != std::string::npos && sb > 0) MkdirRec(diskPath.substr(0, sb)); }
    // ATOMICO igual que todo lo que escribe este editor: primero el temporal
    // completo, despues UN rename. Un fallo a mitad no deja un archivo mocho.
    std::string tmp = diskPath + ".w3dtmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { w3dLogfE("[W3D] extraer: no pude escribir %s", tmp.c_str()); return false; }
    size_t esc = data.empty() ? 0 : fwrite(&data[0], 1, data.size(), f);
    bool ok = (fclose(f) == 0) && esc == data.size();
    if (!ok) { remove(tmp.c_str()); return false; }
    if (!w3dFileSystem::ReplaceFile(tmp, diskPath)) { remove(tmp.c_str()); return false; }
    w3dLogf("[W3D] extraido %s -> %s", entry.c_str(), diskPath.c_str());
    return true;
}

void W3dContainerUnmount() {
    if (!gStore) return;
    W3dStoreUnmount();
    delete gStore;
    gStore = NULL;
    gMountedPath.clear();
}

bool W3dContainerMount(const std::string& pathW3d) {
    W3dContainerUnmount();
    StoreContainer* a = new StoreContainer();
    if (!a->Open(pathW3d)) { delete a; return false; }
    // EL QUE MANDA para decidir si es un proyecto es project.json, NO el
    // mimetype: un .w3d escrito por el editor de antes del estilo OpenDocument
    // no lo tiene, y tiene que abrir igual (se migra en el primer guardado).
    if (!a->Exists("project.json")) {
        w3dLogfW("[W3D] %s es un zip pero no trae project.json (no es un contenedor v4)", pathW3d.c_str());
        delete a;
        return false;
    }
    {
        std::vector<std::string> ents;
        a->zip.Reader().List(ents);
        if (ents.empty() || ents[0] != "mimetype") {
            w3dLogf("[W3D] %s no trae la entrada 'mimetype' primera (formato anterior): "
                    "se migra al primer guardado", pathW3d.c_str());
        } else {
            std::vector<unsigned char> mt;
            if (a->zip.Reader().Read("mimetype", mt)) {
                std::string s(mt.begin(), mt.end());
                if (s != kW3dMimetype)
                    w3dLogfW("[W3D] %s dice ser '%s' y no '%s': se abre igual",
                             pathW3d.c_str(), s.c_str(), kW3dMimetype);
            }
        }
    }
    gStore = a;
    gMountedPath = pathW3d;
    W3dStoreMount(a);
    w3dLogf("[W3D] contenedor montado: %s", pathW3d.c_str());
    w3dLogMemory("contenedor montado");
    return true;
}

bool W3dContainerIs(const std::string& path) {
    if (gStore && gMountedPath == path) return true;
    W3dZipReader z;
    if (!z.Open(path)) return false;
    bool yes = z.Exists("project.json");
    z.Close();
    return yes;
}

// ---------------------------------------------------------------------------
//  EL ORIGEN de cada asset importado (ver W3dContainer.h)
// ---------------------------------------------------------------------------
static std::map<std::string, std::string> gOrigins;
void W3dAssetOriginAnnotate(const std::string& entry, const std::string& diskPath) {
    if (entry.empty() || diskPath.empty() || !W3dIsEntryName(entry)) return;
    gOrigins[entry] = diskPath;
}
std::string W3dAssetOrigin(const std::string& entry) {
    std::map<std::string, std::string>::const_iterator it = gOrigins.find(entry);
    return it == gOrigins.end() ? std::string() : it->second;
}
void W3dAssetOriginMove(const std::string& old, const std::string& newValue) {
    std::map<std::string, std::string>::iterator it = gOrigins.find(old);
    if (it == gOrigins.end() || old == newValue) return;
    const std::string r = it->second;
    gOrigins.erase(it);
    gOrigins[newValue] = r;
}
void W3dAssetOriginsClear() { gOrigins.clear(); }
void W3dAssetOriginsList(std::vector<std::pair<std::string, std::string> >& out) {
    out.assign(gOrigins.begin(), gOrigins.end());
}

// ---------------------------------------------------------------------------
//  IMPORTAR: la copia adentro pasa AHORA, no al guardar (ver W3dContainer.h)
// ---------------------------------------------------------------------------
std::string W3dImportAsset(const std::string& diskPath, const char* category) {
    if (diskPath.empty()) return diskPath;
    if (!gStore) return diskPath;                    // proyecto v3: sigue suelto
    if (W3dIsEntryName(diskPath)) return diskPath;   // ya esta adentro
    if (W3dRefIsExternal(diskPath)) return diskPath; // el usuario la quiso afuera
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(diskPath, data)) {
        // NO se inventa nada: si no se puede leer, la ref queda como estaba y el
        // guardado la va a registrar como externa que FALTA (con aviso).
        w3dLogfW("[W3D] importar: no pude leer %s (queda como referencia externa)", diskPath.c_str());
        return diskPath;
    }
    if (!category || !*category) category = W3dCategoryByExtension(diskPath);
    const unsigned size = (unsigned)data.size();
    // DEDUP POR CONTENIDO: la misma imagen importada dos veces es UNA entrada
    // (misma regla que el guardado; si no, cada "Cargar textura" engordaba el .w3d)
    {
        std::vector<std::string> all;
        gStore->List(all);
        for (size_t i = 0; i < all.size(); i++) {
            if (gStore->Size(all[i]) != size) continue;
            std::vector<unsigned char> other;   // (mismo tamano: decide la comparacion byte a byte)
            if (gStore->Read(all[i], other) && other == data) { W3dAssetOriginAnnotate(all[i], diskPath); return all[i]; }
        }
    }
    // nombre de entrada libre ("textures/piso.png", "textures/piso-2.png"...)
    std::string base = diskPath, ext;
    { size_t s = base.find_last_of("/\\"); if (s != std::string::npos) base = base.substr(s + 1); }
    size_t p = base.find_last_of('.');
    if (p != std::string::npos && p > 0) { ext = base.substr(p); base = base.substr(0, p); }
    std::string root = W3dSlugEntry(base);
    std::string suffix = ext.empty() ? std::string() : W3dSlugEntry(ext);
    if (!suffix.empty() && suffix[0] != '.') suffix = "." + suffix;
    std::string cand = std::string(category) + "/" + root + suffix;
    for (int n = 2; gStore->Exists(cand); n++) {
        char s[16]; snprintf(s, sizeof(s), "-%d", n);
        cand = std::string(category) + "/" + root + s + suffix;
    }
    gStore->edited[cand] = data;
    W3dAssetOriginAnnotate(cand, diskPath);
    w3dLogf("[W3D] importado %s -> %s (%u bytes, adentro del proyecto)",
            diskPath.c_str(), cand.c_str(), size);
    return cand;
}

std::string W3dLoadExternal(const std::string& diskPath, const char* category) {
    if (diskPath.empty() || W3dIsEntryName(diskPath)) return diskPath;   // (ya es del proyecto)
#ifdef __EMSCRIPTEN__
    return W3dImportAsset(diskPath, category);
#else
    (void)category;
    if (gStore) W3dExternalRefMark(diskPath);   // (proyecto v3, sin contenedor: todo es suelto igual)
    w3dLogf("[W3D] cargado %s (queda afuera: referencia externa)", diskPath.c_str());
    return diskPath;
#endif
}

// ===========================================================================
//  REFERENCIAS EXTERNAS
// ===========================================================================
static std::set<std::string> gExternal;

// TEST del harness (ver W3dContainer.h)
bool g_w3dContForgetOne = false;

void W3dExternalRefMark(const std::string& r) { if (!r.empty()) gExternal.insert(r); }
void W3dExternalRefUnmark(const std::string& r) { gExternal.erase(r); }
bool W3dRefIsExternal(const std::string& r)     { return !r.empty() && gExternal.count(r) != 0; }
void W3dExternalRefClear()                   { gExternal.clear(); }
void W3dExternalRefList(std::vector<std::string>* out) {
    if (!out) return;
    for (std::set<std::string>::const_iterator it = gExternal.begin(); it != gExternal.end(); ++it)
        out->push_back(*it);
}

// ===========================================================================
//  NOMBRES DE ENTRADA
// ===========================================================================
static const char* const kFolders[] = {
    "scenes/", "scripts/", "textures/", "fonts/", "sounds/", "videos/",
    "meshes/", "animations/", "models/", "project/", "extra/", "prefabs/", "shaders/", 0
};

bool W3dIsEntryName(const std::string& r) {
    if (r.empty() || r[0] == '/' || r[0] == '\\') return false;
    if (r.size() > 1 && r[1] == ':') return false;                 // "C:\..." o "ext:..."
    if (r.compare(0, 12, "project.json") == 0 && r.size() == 12) return true;
    for (int i = 0; kFolders[i]; i++) {
        size_t n = strlen(kFolders[i]);
        if (r.size() > n && r.compare(0, n, kFolders[i]) == 0) return true;
    }
    return false;
}

// translitera un byte UTF-8 conocido a ASCII. Devuelve cuantos bytes consumio
// (0 = no es una secuencia que conozcamos).
static int Translitera(const std::string& s, size_t i, char* out) {
    unsigned char a = (unsigned char)s[i];
    if (a < 0x80) { *out = (char)a; return 1; }
    if (i + 1 >= s.size()) return 0;
    unsigned char b = (unsigned char)s[i + 1];
    unsigned cp = 0;
    if ((a & 0xE0) == 0xC0) cp = ((unsigned)(a & 0x1F) << 6) | (b & 0x3F);
    else return 0;   // 3+ bytes: no hay tabla, cae al '_'
    switch (cp) {
        case 0xE1: case 0xE0: case 0xE4: case 0xE2: case 0xC1: case 0xC0: case 0xC4: case 0xC2: *out='a'; return 2;
        case 0xE9: case 0xE8: case 0xEB: case 0xEA: case 0xC9: case 0xC8: case 0xCB: case 0xCA: *out='e'; return 2;
        case 0xED: case 0xEC: case 0xEF: case 0xEE: case 0xCD: case 0xCC: case 0xCF: case 0xCE: *out='i'; return 2;
        case 0xF3: case 0xF2: case 0xF6: case 0xF4: case 0xD3: case 0xD2: case 0xD6: case 0xD4: *out='o'; return 2;
        case 0xFA: case 0xF9: case 0xFC: case 0xFB: case 0xDA: case 0xD9: case 0xDC: case 0xDB: *out='u'; return 2;
        case 0xF1: case 0xD1: *out='n'; return 2;
        case 0xE7: case 0xC7: *out='c'; return 2;
        case 0xBF: case 0xA1: *out=0;   return 2;   // '¿' y '¡' se van
    }
    return 0;
}

std::string W3dSlugEntry(const std::string& name) {
    std::string s;
    for (size_t i = 0; i < name.size(); ) {
        char c = 0;
        int used = Translitera(name, i, &c);
        if (used == 0) { s += '_'; i++; continue; }
        i += used;
        if (c == 0) continue;                       // caracter que se descarta
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '-' || c == '_';
        s += ok ? c : '_';
    }
    // colapsar '_' repetidos
    std::string t;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '_' && !t.empty() && t[t.size() - 1] == '_') continue;
        t += s[i];
    }
    // sacar '.', '_' y '-' de los bordes
    size_t start = 0, end = t.size();
    while (start < end && (t[start] == '.' || t[start] == '_' || t[start] == '-')) start++;
    while (end > start && (t[end-1] == '.' || t[end-1] == '_' || t[end-1] == '-')) end--;
    t = t.substr(start, end - start);
    if (t.empty()) t = "unnamed";
    // nombres reservados de Windows (con o sin extension)
    {
        std::string root = t;
        size_t p = root.find('.');
        if (p != std::string::npos) root = root.substr(0, p);
        static const char* const res[] = { "con","prn","aux","nul",
            "com1","com2","com3","com4","com5","com6","com7","com8","com9",
            "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9", 0 };
        for (int i = 0; res[i]; i++) if (root == res[i]) { t = "_" + t; break; }
    }
    if (t.size() > 60) t = t.substr(0, 60);
    return t;
}

const char* W3dCategoryByExtension(const std::string& r) {
    size_t p = r.find_last_of('.');
    if (p == std::string::npos) return "extra";
    std::string e = r.substr(p + 1);
    for (size_t i = 0; i < e.size(); i++) if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] + 32);
    if (e == "w3dui" || e == "w3de") return "scenes";   // escenas UI y escenas 3D (W3dRoots.h)
    if (e == "w3dp") return "prefabs";
    if (e == "lua" || e == "luac") return "scripts";
    if (e == "glsl") return "shaders";   // los shaders escritos de los materiales (objects/WrittenShader.h)
    if (e == "png" || e == "jpg" || e == "jpeg" || e == "webp" || e == "gif" ||
        e == "svg" || e == "bmp" || e == "tga") return "textures";
    if (e == "ttf" || e == "otf" || e == "w3dfnt") return "fonts";
    if (e == "ogg" || e == "wav" || e == "mp3") return "sounds";
    if (e == "mp4" || e == "webm" || e == "mov" || e == "avi") return "videos";
    // "w3db" (binario, el default) y "w3dm" (texto por lineas) son el formato de
    // geometria PROPIO; glb/gltf quedan por los proyectos guardados antes de que existiera
    if (e == "w3db" || e == "w3dm" || e == "glb" || e == "gltf" || e == "w3dmesh") return "meshes";
    // "w3da": las bibliotecas de clips de esqueleto (animsets, animation/W3dAnimSet.h)
    if (e == "bin" || e == "w3danim" || e == "w3da") return "animations";
    if (e == "obj" || e == "wobj" || e == "fbx" || e == "mtl" || e == "stl") return "models";
    return "extra";
}

// ===========================================================================
//  ESCRITOR
// ===========================================================================
static std::string FolderOf(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : r.substr(0, s);
}
static std::string BaseOf(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? r : r.substr(s + 1);
}

W3dContainerWriter::W3dContainerWriter() : old(NULL), tmpCounter(0) {}
W3dContainerWriter::~W3dContainerWriter() { ClearTemp(); }

void W3dContainerWriter::Start(const std::string& finalPath, W3dZipReader* v) {
    destinationPath = finalPath;
    dirDestination  = FolderOf(finalPath);
    old             = v;
    tmpCounter      = 0;
    pend.clear(); byName.clear(); byFootprint.clear();
    alreadyResolved.clear(); external.clear(); forgotten.clear();
}

std::string W3dContainerWriter::TempPath(const char* label) {
    char b[64];
    snprintf(b, sizeof(b), ".%s%d.w3dtmp", label ? label : "t", tmpCounter++);
    return destinationPath + b;
}

bool W3dContainerWriter::Has(const std::string& name) const {
    return byName.find(name) != byName.end();
}

void W3dContainerWriter::Ingested(std::vector<std::pair<std::string, std::string> >& out) const {
    out.clear();
    for (std::map<std::string, std::string>::const_iterator it = alreadyResolved.begin(); it != alreadyResolved.end(); ++it)
        if (it->second.compare(0, 4, "ext:") != 0) out.push_back(*it);
}

// "textures/pausa.png" libre, o "textures/pausa-2.png" si ese nombre ya lo ocupa
// OTRO contenido (el sufijo es deliberadamente distinto del ".001" del editor:
// si ves "-2" sabes que es una desambiguacion de ARCHIVO, no un nombre de objeto)
std::string W3dContainerWriter::FreeName(const char* category, const std::string& base) {
    std::string b = base;
    std::string ext;
    size_t p = b.find_last_of('.');
    if (p != std::string::npos && p > 0) { ext = b.substr(p); b = b.substr(0, p); }
    std::string root = W3dSlugEntry(b);
    std::string suffix = ext.empty() ? std::string() : W3dSlugEntry(ext);
    if (!suffix.empty() && suffix[0] != '.') suffix = "." + suffix;
    std::string cand = std::string(category) + "/" + root + suffix;
    for (int n = 2; Has(cand); n++) {
        char s[16]; snprintf(s, sizeof(s), "-%d", n);
        cand = std::string(category) + "/" + root + s + suffix;
    }
    return cand;
}

// ---------------------------------------------------------------------------
//  DEDUP POR CONTENIDO DE VERDAD (no por huella)
//
//  La huella "crc:tam" es un INDICE para no comparar contra todas las entradas;
//  la decision la toma la comparacion BYTE A BYTE. CRC32 son 32 bits y ademas es
//  LINEAL: fabricar dos archivos distintos del mismo tamano con el mismo CRC es
//  aritmetica de secundaria, no suerte. Con el dedup por huella sola, esos dos
//  archivos colapsaban en UNO y el proyecto abria con una textura donde iba otra
//  (o con el .lua de otra escena). El otro camino del mismo repo ya lo hacia
//  bien: W3dImportAsset compara los bytes.
// ---------------------------------------------------------------------------
bool W3dContainerWriter::EntryBytes(size_t idx, std::vector<unsigned char>& out) const {
    out.clear();
    if (idx >= pend.size()) return false;
    const W3dEntryPend& e = pend[idx];
    if (e.origin == 0) { out = e.bytes; return true; }
    if (e.origin == 1) return w3dFileSystem::ReadFileBytes(e.source, out);
    if (e.origin == 2 && old) return old->Read(e.source.empty() ? e.name : e.source, out);
    return false;
}

size_t W3dContainerWriter::FindByContent(const std::string& footprint,
                                                 const std::vector<unsigned char>& data) const {
    std::map<std::string, std::vector<size_t> >::const_iterator it = byFootprint.find(footprint);
    if (it == byFootprint.end()) return (size_t)-1;
    for (size_t k = 0; k < it->second.size(); k++) {
        size_t idx = it->second[k];
        if (idx >= pend.size() || pend[idx].size != (unsigned)data.size()) continue;
        std::vector<unsigned char> other;
        if (!EntryBytes(idx, other)) continue;   // no se pudo releer: NO se aliasa
        if (other == data) return idx;
    }
    return (size_t)-1;
}

// la huella "crc:tam" del indice por contenido (ver byFootprint en el .h)
static std::string Footprint(unsigned crc, unsigned size) {
    char h[40]; snprintf(h, sizeof(h), "%08x:%u", crc, size);
    return h;
}

void W3dContainerWriter::AddPend(const W3dEntryPend& e, bool dedup) {
    byName[e.name] = pend.size();
    if (dedup) byFootprint[Footprint(e.crc, e.size)].push_back(pend.size());
    pend.push_back(e);
}

bool W3dContainerWriter::PendEdited(const std::string& name, bool dedup) {
    W3dEntryPend e;
    e.name = name; e.origin = 0;
    if (!W3dContainerReadEdited(name, e.bytes)) return false;
    e.size = (unsigned)e.bytes.size();
    e.crc  = e.bytes.empty() ? 0u : Crc32(&e.bytes[0], e.bytes.size());
    AddPend(e, dedup);
    return true;
}

// 'named' = la NOMBRA lo que se esta guardando (Ingest): entra al indice por contenido y, si esta ROTA en el zip
// viejo, se anota igual (crc 0) para que el guardado se corte al copiarla (CopyFrom dice cual fue). Las que solo se
// PRESERVAN no van al indice y, rotas, no se anotan
bool W3dContainerWriter::PendOld(const std::string& name, bool named) {
    if (!old) return false;
    W3dEntryPend e;
    e.name = name; e.origin = 2; e.source = name;
    if (!old->Info(name, &e.crc, &e.size)) {
        if (!named) return false;
        e.crc = 0; e.size = old->Size(name);
    }
    AddPend(e, named);
    return true;
}

std::vector<W3dExternal> g_w3dExternalMissing;
void W3dContainerWriter::MissingExternals(std::vector<W3dExternal>& out) const {
    out.clear();
    for (size_t i = 0; i < external.size(); i++) if (!external[i].exists) out.push_back(external[i]);
}
void W3dContainerWriter::RegisterExternal(const std::string& ref, const std::string& who, bool exists, const std::string& original) {
    for (size_t i = 0; i < external.size(); i++)
        if (external[i].ref == ref) return;
    W3dExternal e; e.ref = ref; e.who = who; e.exists = exists; e.original = original;
    external.push_back(e);
}

// ver el comentario del .h: una referencia que es el PREFIJO de una secuencia no se
// puede comprobar con FileExists, y el que sabe cual es el archivo real lo dice aca.
void W3dContainerWriter::ExternalCorrectExists(const std::string& ref, bool exists) {
    for (size_t i = 0; i < external.size(); i++)
        if (external[i].ref == ref) { external[i].exists = exists; return; }
}

std::string W3dContainerWriter::Ingest(std::string& path, const char* category,
                                           const std::string& who) {
    if (path.empty()) return path;
    // UNA ENTRADA DE UNA LIBRERIA EXTERNA ("lib:<libreria>/textures/x.png", io/Libraries.h): es de la libreria, no
    // del proyecto. Se escribe TAL CUAL (una referencia) y no se copia adentro
    if (path.compare(0, 4, "lib:") == 0) return path;
    std::map<std::string, std::string>::iterator already = alreadyResolved.find(path);
    if (already != alreadyResolved.end()) {
        std::string written = already->second;
        // la ruta EN MEMORIA queda en el nombre de entrada solo si es interna
        if (written.compare(0, 4, "ext:") != 0) path = written;
        return written;
    }
    const std::string original = path;
    if (!category || !*category) category = W3dCategoryByExtension(path);

    // --- 1) EXTERNA DELIBERADA: la ref vino con "ext:" y asi se re-escribe ---
    if (W3dRefIsExternal(path)) {
        std::string rel = path, tmp;
        if (W3dPathUnderFolder(path, dirDestination, tmp)) rel = tmp;
        std::string written = "ext:" + rel;
        bool has = w3dFileSystem::FileExists(path);
        RegisterExternal(written, who, has, original);
        alreadyResolved[original] = written;
        return written;
    }

    // --- 1.2) IDEMPOTENCIA: LA MISMA RUTA, YA REESCRITA, LLEGANDO OTRA VEZ ---
    //  Ingest() deja 'ruta' con el NOMBRE DE ENTRADA, y ese std::string puede
    //  estar COMPARTIDO entre varios duenos: el cache de texturas por ruta le da
    //  UN solo Texture* a todos los materiales que nombran el mismo png, asi que
    //  un .mtl con dos materiales y el mismo `map_Kd` (caballero / caballero-
    //  sombra) manda la MISMA cadena dos veces: la primera vuelve como
    //  "textures/caballero.png"... y la segunda ENTRA con ese valor.
    //  Sin esto: 'alreadyResolved' se busca por la ruta ORIGINAL y falla, no hay
    //  contenedor viejo donde mirar (primer guardado desde el .w3d de texto),
    //  ReadFileBytes no encuentra "textures/caballero.png" en disco y la textura
    //  quedaba anotada como EXTERNA QUE FALTA. Resultado: del SEGUNDO material
    //  en adelante se perdia la textura (7 materiales en el caso medido).
    //  El nombre de entrada YA ingerido se resuelve a si mismo -- pero SOLO si de
    //  verdad es el mismo archivo: un nombre generado ("textures/caballero.png")
    //  puede coincidir con un archivo REAL del disco que no tiene nada que ver, y
    //  ese caso tiene que seguir por el camino normal (FreeName le da "-2").
    if (Has(path)) {
        std::vector<unsigned char> inDisk, inEntry;
        bool hasInDisk = w3dFileSystem::ReadFileBytes(path, inDisk);
        if (!hasInDisk ||
            (EntryBytes(byName[path], inEntry) && inEntry == inDisk)) {
            alreadyResolved[original] = path;
            return path;
        }
    }

    // --- 1.5) ENTRADA EDITADA EN CALIENTE (el IDE guardo este .lua): van los
    //          bytes NUEVOS. Si esto faltara, el reenvasado byte a byte del paso
    //          siguiente copiaria los del zip VIEJO y guardar el proyecto tiraria
    //          silenciosamente lo que el usuario acaba de escribir.
    if (W3dIsEntryName(path) && W3dContainerEntryEdited(path)) {
        if (!Has(path)) PendEdited(path, true);
        alreadyResolved[original] = path;
        return path;
    }

    // --- 2) YA ES UNA ENTRADA del contenedor viejo: se reenvasa tal cual ---
    if (W3dIsEntryName(path) && old && old->Exists(path)) {
        // TEST: "olvidarse" de esta entrada (el JSON la va a nombrar igual). Lo tiene
        // que atajar Verify() ANTES de renombrar nada.
        if (g_w3dContForgetOne) {
            g_w3dContForgetOne = false;
            forgotten.insert(path);   // ...y que tampoco la salve la preservacion
            alreadyResolved[original] = path;
            return path;
        }
        if (!Has(path)) PendOld(path, true);
        alreadyResolved[original] = path;
        return path;
    }

    // --- 2.6) HERMANO DERIVADO de una entrada, clasificado en OTRA carpeta ---
    // El guardado de una fontBitmap deriva el .json de glifos del nombre del
    // png ("textures/x.json"), pero el contenedor clasifica los .json bajo
    // "extra/": el nombre derivado no existe ni como entrada ni en disco y el
    // re-guardado lo anotaba como referencia externa FALTA -> el round-trip
    // dejaba de ser byte a byte. Misma tolerancia que el LECTOR (Font2D.cpp,
    // busca "extra/<base>"): se reenvasa ESA entrada bajo su nombre real.
    // Si ese hermano esta EDITADO en el overlay (renombrar la textura de una fuente bitmap muda su
    // .json a "extra/<nombre nuevo>.json" en memoria) van esos bytes: son los vigentes.
    if (W3dIsEntryName(path) && !(old && old->Exists(path))) {
        std::string alt = "extra/" + BaseOf(path);
        if (alt != path && W3dContainerEntryEdited(alt)) {
            if (!Has(alt)) PendEdited(alt, true);
            alreadyResolved[original] = alt;
            return alt;
        }
    }
    if (W3dIsEntryName(path) && old && !old->Exists(path)) {
        std::string alt = "extra/" + BaseOf(path);
        if (alt != path && old->Exists(alt)) {
            if (!Has(alt)) PendOld(alt, true);
            alreadyResolved[original] = alt;
            return alt;
        }
    }

    // --- 3) ARCHIVO DE DISCO (o entrada del contenedor montado): se INGIERE ---
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(path, data)) {
        // referencia ROTA: NUNCA se borra (un disco desmontado o un pendrive
        // sacado no puede convertirse en perdida de configuracion). Queda como
        // externa que FALTA y se avisa.
        std::string rel = path, tmp;
        if (W3dPathUnderFolder(path, dirDestination, tmp)) rel = tmp;
        std::string written = "ext:" + rel;
        RegisterExternal(written, who, false, original);
        W3dExternalRefMark(path);
        alreadyResolved[original] = written;
        w3dLogfW("[W3D] no pude leer %s (%s): queda como referencia externa", path.c_str(), who.c_str());
        return written;
    }
    unsigned crc  = data.empty() ? 0u : Crc32(&data[0], data.size());
    unsigned size = (unsigned)data.size();
    const std::string h = Footprint(crc, size);
    // DEDUP POR CONTENIDO: el mismo archivo referenciado desde tres escenas es
    // UNA entrada (y sonidos.lua no tiene que "pertenecerle" a nadie).
    // "Por CONTENIDO" es literal: se comparan LOS BYTES. La huella crc:tam solo
    // elige contra quien comparar (ver FindByContent).
    {
        size_t dup = FindByContent(h, data);
        if (dup != (size_t)-1) {
            std::string name = pend[dup].name;
            alreadyResolved[original] = name;
            path = name;
            return name;
        }
    }
    // TEST: "olvidarse" de esta entrada (el JSON la va a nombrar igual). Lo tiene
    // que atajar Verify() ANTES de renombrar nada.
    if (g_w3dContForgetOne) {
        g_w3dContForgetOne = false;
        std::string name = FreeName(category, BaseOf(path));
        forgotten.insert(name);
        alreadyResolved[original] = name;
        path = name;
        return name;
    }
    W3dEntryPend e;
    e.name   = FreeName(category, BaseOf(path));
    e.origin = 1;
    e.source = path;
    e.temp = false;
    e.crc = crc; e.size = size;
    AddPend(e, true);
    alreadyResolved[original] = e.name;
    path = e.name;
    return e.name;
}

bool W3dContainerWriter::AddBytes(const std::string& name, const std::string& text,
                                         bool dedup) {
    if (Has(name)) {
        w3dLogfE("[W3D] entrada duplicada en el contenedor: %s", name.c_str());
        return false;
    }
    W3dEntryPend e;
    e.name = name; e.origin = 0;
    e.bytes.assign(text.begin(), text.end());
    e.size = (unsigned)e.bytes.size();
    e.crc  = e.bytes.empty() ? 0u : Crc32(&e.bytes[0], e.bytes.size());
    AddPend(e, dedup);
    return true;
}

// ---------------------------------------------------------------------------
//  ESTILO OPENDOCUMENT: "mimetype" (que Write() manda PRIMERA) y "README.txt".
//  Las dos se REGENERAN en cada guardado y NINGUNA entra al indice por contenido
//  (ver el 'dedup' de AddBytes): son servicio, no datos del proyecto.
// ---------------------------------------------------------------------------
void W3dContainerWriter::WriteHeaderOdf() {
    if (!Has("mimetype"))  AddBytes("mimetype", kW3dMimetype, false);
    if (!Has("README.txt")) AddBytes("README.txt", W3dContainerReadme(), false);
}

bool W3dContainerWriter::AddTemp(const std::string& name, const std::string& tmpPath) {
    if (Has(name)) {
        w3dLogfE("[W3D] entrada duplicada en el contenedor: %s", name.c_str());
        return false;
    }
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(tmpPath, data)) {
        w3dLogfE("[W3D] no pude releer el temporal %s", tmpPath.c_str());
        return false;
    }
    W3dEntryPend e;
    e.name = name; e.origin = 1; e.source = tmpPath; e.temp = true;
    e.size = (unsigned)data.size();
    e.crc  = data.empty() ? 0u : Crc32(&data[0], data.size());
    AddPend(e, true);
    return true;
}

// ---------------------------------------------------------------------------
//  VERIFICACION: se recorre el TEXTO de cada entrada generada (project.json y
//  los .w3dui) buscando strings que arranquen con una carpeta reservada, o sea
//  referencias INTERNAS, y se exige que cada una tenga su entrada.
//
//  Es a proposito un escaneo de strings y no un parseo del esquema: un campo de
//  ruta NUEVO (que manana alguien agregue en el .w3dui) queda cubierto igual, y
//  esta verificacion existe justamente para atajar lo que el esquema no sabe.
//
//  LOS NOMBRES NO SON REFERENCIAS. Un recurso o un objeto puede LLAMARSE como una
//  entrada (un material "texturas/piel", una malla "mallas/rocas", un objeto
//  "scripts/jefe"): su nombre va en el JSON bajo la clave "nombre"/"name" y, en
//  cada punta que lo nombra ("material", "malla", "target"...), como un string
//  pelado. El escaneo lo tomaba por una referencia interna a una entrada que no
//  existe y ABORTABA el guardado. Por eso se anota cada VALOR de "nombre"/"name"
//  (el esquema nunca guarda una ruta bajo esas claves: es siempre un nombre) y
//  una referencia que falta y es EXACTAMENTE uno de esos nombres es un vinculo
//  POR NOMBRE, no una entrada perdida. Una entrada que falta de verdad y no se
//  llama como nada del proyecto sigue abortando igual que siempre.
// ---------------------------------------------------------------------------
static bool IsNameKey(const std::string& k) { return k == "name"; }
static void TextRefs(const std::vector<unsigned char>& b, std::set<std::string>& out,
                         std::set<std::string>& names) {
    std::string s, key;
    bool inside = false;
    for (size_t i = 0; i < b.size(); i++) {
        char c = (char)b[i];
        if (!inside) { if (c == '"') { inside = true; s.clear(); } continue; }
        if (c == '\\') { if (i + 1 < b.size()) { s += (char)b[i + 1]; i++; } continue; }
        if (c == '"') {
            inside = false;
            // es una CLAVE si lo que sigue (salteando blancos) es ':'
            size_t k = i + 1;
            while (k < b.size() && (b[k] == ' ' || b[k] == '\t' || b[k] == '\n' || b[k] == '\r')) k++;
            const bool isKey = (k < b.size() && b[k] == ':');
            if (!isKey && IsNameKey(key)) { names.insert(s); continue; }   // un NOMBRE
            if (W3dIsEntryName(s)) out.insert(s);
            if (isKey) key = s;
            continue;
        }
        s += c;
    }
}

bool W3dContainerWriter::Verify(std::string& missing) {
    std::set<std::string> refs;      // de lo que GENERO este guardado -> abortar
    std::set<std::string> oldRefs;// de lo que se PRESERVO del zip viejo -> avisar
    std::set<std::string> names;   // los NOMBRES declarados (ver TextRefs): no son entradas
    for (size_t i = 0; i < pend.size(); i++) {
        const W3dEntryPend& e = pend[i];
        // solo lo GENERADO por este guardado describe referencias: el JSON raiz
        // y las escenas. Un png no referencia nada.
        bool isJson = (e.name == "project.json");
        bool isUi   = (e.name.size() > 6 && e.name.compare(e.name.size() - 6, 6, ".w3dui") == 0);
        if (!isJson && !isUi) continue;
        if (e.origin == 0) { TextRefs(e.bytes, refs, names); continue; }
        std::vector<unsigned char> data;
        if (e.origin == 1 && w3dFileSystem::ReadFileBytes(e.source, data)) TextRefs(data, refs, names);
        // ORIGEN 2 = una escena que venia del .w3d viejo y que este guardado
        // PRESERVA sin haberla cargado (PreserveTransient). Antes no se escaneaba
        // NUNCA, o sea que era el unico .w3dui del archivo que podia quedar
        // apuntando a una entrada que ya no esta. Se escanea, pero NO aborta el
        // guardado: no la generamos nosotros y una escena vieja a medias del
        // usuario no puede dejarlo sin poder guardar. Sale por el log y sigue.
        if (e.origin == 2 && old && old->Read(e.source.empty() ? e.name : e.source, data))
            TextRefs(data, oldRefs, names);
    }
    for (std::set<std::string>::iterator it = refs.begin(); it != refs.end(); ++it)
        if (!Has(*it) && !names.count(*it)) { missing = *it; return false; }
    for (std::set<std::string>::iterator it = oldRefs.begin(); it != oldRefs.end(); ++it)
        if (!Has(*it) && !names.count(*it))
            w3dLogfW("[W3D] una escena PRESERVADA del .w3d anterior nombra '%s', que ya no esta adentro "
                     "(no la genero este guardado: no aborto, pero queda dicho)", it->c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  LIMITE CONOCIDO Y MEDIDO DE Verify(): SOLO SE ESCANEA project.json Y LOS
//  .w3dui. NINGUN .lua se mira.
//
//  O sea que un asset nombrado UNICAMENTE desde un script -"textures/logo.png"
//  armado en Lua, o un require de otro .lua- no se copia adentro ni se reporta:
//  para el guardado ese archivo no existe. No es un descuido, es una decision:
//  un .lua puede construir el nombre en tiempo de ejecucion (".."), asi que un
//  escaneo de strings daria falsos negativos igual, y los falsos POSITIVOS
//  (cualquier string del script que PAREZCA un nombre de entrada) abortarian el
//  guardado del usuario por un comentario. La regla real es la otra: para que un
//  asset viaje adentro tiene que estar referenciado desde la ESCENA o el JSON
//  (que es lo que el editor sabe ingerir), y el IDE importa los assets al
//  agregarlos, no al nombrarlos.
// ---------------------------------------------------------------------------

bool W3dContainerWriter::CopyFromOld(const std::string& n) {
    if (!old || n.empty() || Has(n)) return false;
    return PendOld(n, false);
}

void W3dContainerWriter::PreserveTransient() {
    std::vector<std::string> all;
    if (old) old->List(all);   // (sin zip viejo -el primer guardado- quedan solo las editadas, mas abajo)
    for (size_t i = 0; i < all.size(); i++) {
        const std::string& n = all[i];
        if (Has(n)) continue;
        if (forgotten.count(n)) continue;                 // hook de test
        // las entradas de SERVICIO las escribe este guardado (mimetype y
        // README.txt se regeneran; preservar el LEEME viejo congelaria un texto
        // que ya no describe el archivo)
        if (W3dIsServiceEntry(n)) continue;
        // meshes/ y animations/ son EXCLUSIVAS del editor: lo que no se
        // referencia ahi son huerfanos de un guardado anterior y se descartan
        if (n.compare(0, 7, "meshes/") == 0) continue;
        if (n.compare(0, 11, "animations/") == 0) continue;
        // EDITADA EN CALIENTE y que nadie referencia (un .lua suelto que el usuario
        // abrio y guardo desde el IDE): se preserva lo NUEVO, no lo del zip viejo.
        if (W3dContainerEntryEdited(n)) PendEdited(n, false);
        else                            PendOld(n, false);
    }
    // LO IMPORTADO EN ESTA SESION que nadie referencia (una imagen que se trajo al proyecto y
    // todavia no se asigno): vive solo en el overlay de editadas y no esta en el zip viejo, asi
    // que el recorrido de arriba no lo ve. Es un HUERFANO del proyecto (el outliner por recursos
    // lo lista y lo purga a pedido): se conserva igual que los que ya estaban en el archivo.
    std::vector<std::string> mounted;
    W3dContainerListFolder("", mounted);
    for (size_t i = 0; i < mounted.size(); i++) {
        const std::string& n = mounted[i];
        if (Has(n) || forgotten.count(n) || W3dIsServiceEntry(n)) continue;
        if (n.compare(0, 7, "meshes/") == 0 || n.compare(0, 11, "animations/") == 0) continue;
        if (!W3dContainerEntryEdited(n) || (old && old->Exists(n))) continue;
        PendEdited(n, false);
    }
}

size_t W3dContainerWriter::MissingExternalCount() const {
    size_t n = 0;
    for (size_t i = 0; i < external.size(); i++) if (!external[i].exists) n++;
    return n;
}

static bool ExternalByRef(const W3dExternal& a, const W3dExternal& b) { return a.ref < b.ref; }
void W3dContainerWriter::WriteExternal() {
    if (external.empty()) return;
    // ORDEN ALFABETICO: el archivo tiene que ser determinista igual que el zip (RegisterExternal no repite refs)
    std::sort(external.begin(), external.end(), ExternalByRef);
    std::string s;
    s += "# Archivos que este proyecto usa pero que NO estan adentro del .w3d.\n";
    s += "# Si mandas este .w3d a otra maquina, estos archivos NO viajan.\n";
    for (size_t i = 0; i < external.size(); i++) {
        s += external[i].ref;
        s += "  | ";
        s += external[i].who.empty() ? std::string("(sin dueno)") : external[i].who;
        s += "  | ";
        s += external[i].exists ? "EXISTE" : "FALTA";
        s += "\n";
    }
    AddBytes("EXTERNALS.txt", s);
}

bool W3dContainerWriter::Write(const std::string& tmpPathZip) {
    // ORDEN DETERMINISTA (estilo OpenDocument): "mimetype" PRIMERA, despues
    // project.json, despues todo lo demas ALFABETICO. Un hexdump de los
    // primeros 74 bytes dice:
    //   PK\3\4 ......... mimetype application/vnd.whisk3d.project+zip
    // porque el local header del writer mide 30 bytes fijos (campo extra en
    // cero), asi que el contenido del mimetype cae justo en el offset 38.
    // Junto con la fecha fija y el metodo STORE, dos guardados sin cambios dan
    // BYTES IDENTICOS.
    static const char* const kFirst[] = { "mimetype", "project.json", 0 };
    std::vector<size_t> order;
    for (int p = 0; kFirst[p]; p++)
        for (size_t i = 0; i < pend.size(); i++)
            if (pend[i].name == kFirst[p]) order.push_back(i);
    std::vector<std::string> names;
    for (size_t i = 0; i < pend.size(); i++) {
        bool fixed = false;
        for (int p = 0; kFirst[p]; p++) if (pend[i].name == kFirst[p]) fixed = true;
        if (!fixed) names.push_back(pend[i].name);
    }
    std::sort(names.begin(), names.end());   // (los nombres no se repiten: son las claves de byName)
    for (size_t i = 0; i < names.size(); i++) order.push_back(byName[names[i]]);

    W3dZipWriter w;
    if (!w.OpenWriting(tmpPathZip)) return false;
    for (size_t i = 0; i < order.size(); i++) {
        const W3dEntryPend& e = pend[order[i]];
        bool ok = true;
        if (e.origin == 0) {
            ok = w.Write(e.name, e.bytes.empty() ? (const void*)"" : (const void*)&e.bytes[0],
                            e.bytes.size());
        } else if (e.origin == 1) {
            std::vector<unsigned char> data;
            if (!w3dFileSystem::ReadFileBytes(e.source, data)) {
                w3dLogfE("[W3D] no pude releer %s para meterlo en el .w3d", e.source.c_str());
                ok = false;
            } else {
                ok = w.Write(e.name, data.empty() ? (const void*)"" : (const void*)&data[0],
                                data.size());
            }
        } else {
            ok = old && w.CopyFrom(*old, e.source);
        }
        if (!ok) { w.Abort(); return false; }
    }
    return w.Close();
}

void W3dContainerWriter::ClearTemp() {
    for (size_t i = 0; i < pend.size(); i++)
        if (pend[i].origin == 1 && pend[i].temp) remove(pend[i].source.c_str());
}
