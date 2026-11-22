// ============================================================================
//  SaveVersion.cpp — ver SaveVersion.h (guardado por versiones del .w3d).
//
//  UNA VERSION ES UNA COPIA DEL ARCHIVO. Con el formato v4 el .w3d es un
//  CONTENEDOR (un zip con el proyecto entero adentro), asi que copiarlo es
//  copiar la version completa: escenas, scripts, texturas e icono incluidos.
//
//  Las versiones se AGRUPAN en una carpeta 'versions/' al lado del proyecto,
//  conservando el nombre con sufijo:
//      miProyecto.w3d
//      versions/miProyecto_v01.w3d
//      versions/miProyecto_v02.w3d
//
//  CAMBIO DELIBERADO (pedido del dueno, textual: "y si quiero que hagas la
//  carpeta versiones y adentro metas el test1_v01.w3d"). Antes los _vNN.w3d
//  quedaban SUELTOS al lado del proyecto y a la decima version la carpeta de
//  trabajo era ilegible. OJO: esto NO es la vuelta a la carpeta 'versions/vN/'
//  con assets.zip adentro que el dueno rechazo; el archivo sigue siendo UNO solo
//  y autocontenido, lo unico que cambia es DONDE vive. La numeracion mira los
//  archivos que ya hay ADENTRO de versions/.
//
//  Se fue con esto la maquinaria vieja de "adiviná qué string es un asset"
//  (ShowCollectFromJVal / ShowResolveReference / ShowAssetExtension) y el
//  assets.zip que aparecia al lado y que el dueno reporto. Y de paso se arregla
//  un bug de silencio: una textura importada desde ~/Imagenes quedaba con ruta
//  ABSOLUTA y el colector la descartaba SIN AVISAR, asi que la version vN no se
//  podia restaurar completa. Con el contenedor eso no puede pasar: la version es
//  el archivo entero.
//
//  Lo que SIGUE afuera son las referencias "ext:" (las que el usuario decidio
//  dejar afuera a proposito): se avisa por notificacion y estan listadas en el
//  EXTERNALS.txt de adentro del .w3d.
// ============================================================================
#include "io/W3dWeb.h"   // navegador: guardar = descargar
#include "W3dLang.h"   // T(): los avisos salen en el idioma del editor
#include "io/SaveVersion.h"

// (El stub W3D_SYMBIAN que vivia aca ya no hace falta: SaveW3D.cpp y el W3dZip del
//  Core SI estan en el Whisk3D.mmp, asi que el guardado por versiones compila entero
//  tambien en el N95 -- el boton "Guardar version" de la tarjeta Archivo ahora hace
//  lo mismo que en PC. Todo el camino es C++03 + fopen/mkdir de Open C.)

#include "io/SaveW3D.h"            // SaveW3D (el guardado normal) + g_projIcon
#include "io/JsonW3d.h"            // parser JSON minimo (colectar referencias)

#include "ViewPorts/Notifications.h"
#include "variables.h"                // w3dPath (el proyecto abierto)
#include "w3dFilesystem.h"            // ReadFileBytes / FileExists / ListDir
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <set>
#include <vector>
#ifdef _WIN32
    #include <direct.h>
    #define W3D_MKDIR(p) _mkdir(p)
#else
    #include <sys/stat.h>
    #define W3D_MKDIR(p) mkdir(p, 0755)
#endif

// ---------------------------------------------------------------------------
//  helpers de rutas/archivos (C++03; todo via w3dFileSystem para ser portable)
// ---------------------------------------------------------------------------
static std::string ShowFolder(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : r.substr(0, s);
}
static std::string ShowBase(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? r : r.substr(s + 1);
}

// crea la ruta de carpetas completa ("a/b/c"): mkdir por segmento (existente = ok)
static void ShowMkdirRec(const std::string& dir) {
    for (size_t i = 1; i < dir.size(); i++)
        if (dir[i] == '/' || dir[i] == '\\')
            W3D_MKDIR(dir.substr(0, i).c_str());
    W3D_MKDIR(dir.c_str());
}

// copia BYTES src -> dst creando las carpetas del destino. false si fallo.
static bool ShowCopyFile(const std::string& src, const std::string& dst) {
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(src, data)) return false;
    ShowMkdirRec(ShowFolder(dst));
    FILE* f = fopen(dst.c_str(), "wb");
    if (!f) return false;
    size_t esc = data.empty() ? 0 : fwrite(&data[0], 1, data.size(), f);
    // fclose PRIMERO y sin corto: es el que hace el flush, o sea donde aparece el disco lleno
    bool closed = (fclose(f) == 0);
    return closed && esc == data.size();
}

// ---------------------------------------------------------------------------
//  colector de referencias: strings del JSON que son rutas relativas a assets
// ---------------------------------------------------------------------------
static bool ShowAssetExtension(const std::string& r) {
    size_t p = r.find_last_of('.');
    if (p == std::string::npos) return false;
    std::string e = r.substr(p + 1);
    for (size_t i = 0; i < e.size(); i++)
        if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] + 32);
    static const char* exts[] = {
        "w3dui", "lua", "luac", "png", "jpg", "jpeg", "bmp", "tga", "gif", "svg",
        "ttf", "otf", "glb", "gltf", "bin", "obj", "wobj", "mtl", "fbx",
        "mp4", "webm", "mov", "avi", "ogg", "wav", "mp3", "w3dfnt", 0 };
    for (int i = 0; exts[i]; i++)
        if (e == exts[i]) return true;
    return false;
}

// la carpeta de una ruta RELATIVA al proyecto ("" = la raiz; a diferencia de
// ShowFolder, que devuelve "." para poder concatenar rutas de disco)
static std::string ShowFolderRel(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string() : r.substr(0, s);
}

// un string candidato es referencia si: es relativo, tiene extension de asset,
// existe en disco bajo el proyecto y no apunta a versions/ (evita meter
// versiones viejas dentro de una version nueva). Se resuelve PRIMERO contra la
// carpeta del archivo que lo referencia ('baseRel', relativa al proyecto; "" =
// la raiz) y despues contra la raiz del proyecto (mismo fallback que JsonPath
// al abrir). Devuelve la ruta relativa AL PROYECTO ("" = no es referencia).
static std::string ShowResolveReference(const std::string& s, const std::string& baseRel,
                                         const std::string& dir) {
    if (s.empty() || s[0] == '/' || s[0] == '\\') return std::string();
    if (s.size() > 2 && s[1] == ':') return std::string();         // "C:\..." (Windows)
    if (!ShowAssetExtension(s)) return std::string();
    if (!baseRel.empty()) {
        std::string rel = baseRel + "/" + s;
        if (rel.compare(0, 9, "versions/") != 0 && w3dFileSystem::FileExists(dir + "/" + rel))
            return rel;
    }
    if (s.compare(0, 9, "versions/") == 0) return std::string();
    if (w3dFileSystem::FileExists(dir + "/" + s)) return s;
    return std::string();
}

static void ShowCollectFromJVal(JVal* v, const std::string& baseRel, const std::string& dir,
                              std::set<std::string>* refs, std::vector<std::string>* w3duiNew) {
    if (!v) return;
    if (v->type == 2) {
        std::string rel = ShowResolveReference(v->str, baseRel, dir);
        if (!rel.empty() && refs->insert(rel).second) {
            // las escenas .w3dui referencian sus propios assets: se parsean tambien
            size_t p = rel.find_last_of('.');
            if (p != std::string::npos && rel.substr(p) == ".w3dui")
                w3duiNew->push_back(rel);
        }
        return;
    }
    for (std::map<std::string, JVal*>::iterator it = v->obj.begin(); it != v->obj.end(); ++it)
        ShowCollectFromJVal(it->second, baseRel, dir, refs, w3duiNew);
    for (size_t i = 0; i < v->list.size(); i++)
        ShowCollectFromJVal(v->list[i], baseRel, dir, refs, w3duiNew);
}

// parsea un JSON del disco y colecta sus referencias (el .w3d o un .w3dui)
static void ShowCollectFromFile(const std::string& path, const std::string& baseRel,
                                 const std::string& dir,
                                 std::set<std::string>* refs, std::vector<std::string>* w3duiNew) {
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(path, data) || data.empty()) return;
    JParser parser((const char*)&data[0], data.size());
    JVal* root = parser.Value();
    if (!parser.error) ShowCollectFromJVal(root, baseRel, dir, refs, w3duiNew);
    delete root;
}

// API publica (la reusa CompileGame): TODO lo relativo que 'filePath' (un
// .w3d o un .w3dui en disco) y sus escenas referencian, como rutas relativas
// al proyecto. Los .w3dui encadenados se parsean con SU carpeta como base.
void SaveVersionCollectFrom(const std::string& filePath, const std::string& baseRel,
                              const std::string& dirProject, std::set<std::string>* refs) {
    std::vector<std::string> pending;   // .w3dui por parsear (los agrega el colector)
    ShowCollectFromFile(filePath, baseRel, dirProject, refs, &pending);
    // los .w3dui pueden referenciar otros .w3dui (se procesan en cadena, sin repetir:
    // el set 'refs' ya deduplica y solo lo NUEVO entra a 'pendientes')
    for (size_t i = 0; i < pending.size(); i++)
        ShowCollectFromFile(dirProject + "/" + pending[i], ShowFolderRel(pending[i]),
                             dirProject, refs, &pending);
}

// ---------------------------------------------------------------------------
//  NUMERACION: versions/<proyecto>_v01.w3d, _v02.w3d, ...
//
//  Se escanea la carpeta versions/ buscando "<base>_vNN.w3d". Asi el numero no
//  depende de ningun estado guardado (borrar una version vieja no rompe la
//  cuenta) y el label del boton nunca miente.
// ---------------------------------------------------------------------------

// la carpeta donde viven las versiones de 'w3d' (no se crea aca: la crea la copia)
static std::string ShowDirVersions(const std::string& w3d) {
    return ShowFolder(w3d) + "/versions";
}
static std::string ShowBaseNoExt(const std::string& r) {
    std::string b = ShowBase(r);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}

// "miProyecto_v03.w3d" -> 3;  0 = no es una version de ESTE proyecto
static int ShowNumberOf(const std::string& file, const std::string& base) {
    const std::string pref = base + "_v";
    if (file.size() <= pref.size() + 4) return 0;                 // + al menos 1 digito + ".w3d"
    if (file.compare(0, pref.size(), pref) != 0) return 0;
    size_t p = file.size() - 4;
    if (file.compare(p, 4, ".w3d") != 0) return 0;
    if (p <= pref.size()) return 0;
    for (size_t i = pref.size(); i < p; i++)
        if (file[i] < '0' || file[i] > '9') return 0;
    return atoi(file.c_str() + pref.size());
}

// el nombre EXACTO de la version N ("miProyecto_v01.w3d"). Dos digitos hasta la
// 99 para que el explorador de archivos las ordene bien.
static std::string ShowNameOf(const std::string& base, int n) {
    char b[32]; snprintf(b, sizeof(b), "_v%02d.w3d", n);
    return base + b;
}

static int ShowMaxVersionIn(const std::string& dir, const std::string& base) {
    std::vector<w3dFileSystem::DirEntry> ents;
    if (!w3dFileSystem::ListDir(dir, ents)) return 0;
    int maxN = 0;
    for (size_t i = 0; i < ents.size(); i++) {
        if (ents[i].isDir) continue;
        int n = ShowNumberOf(ents[i].name, base);
        if (n > maxN) maxN = n;
    }
    return maxN;
}

// el numero de la version que el boton VA A CREAR (el label no debe mentir):
// 1 si no hay ninguna, sino max + 1. Secuencial simple.
int SaveNextVersionN() {
    if (w3dPath.empty()) return 1;
    // adentro de versions/. Si la carpeta todavia no existe, ListDir falla y
    // ShowMaxVersionIn devuelve 0 -> la primera version es la v1, como corresponde.
    return ShowMaxVersionIn(ShowDirVersions(w3dPath), ShowBaseNoExt(w3dPath)) + 1;
}

std::string SaveVersionLabel() {
    char b[128];   // (el label sale en el idioma del editor: en cirilico ocupa el doble de bytes)
    snprintf(b, sizeof(b), T("Save version v%d"), SaveNextVersionN());
    return std::string(b);
}

// ---------------------------------------------------------------------------
//  el flujo completo del boton: guardar normal + UNA COPIA del .w3d al lado
// ---------------------------------------------------------------------------
bool SaveVersionRun() {
    if (w3dPath.empty()) {
        Notify(T("Save version: the project has no location (use Save first)"), true);
        return false;
    }
    std::string dir  = ShowDirVersions(w3dPath);   // <carpeta del proyecto>/versiones
    std::string base = ShowBaseNoExt(w3dPath);
    // el N del snapshot: el mismo que mostraba el boton (label y archivo creado
    // siempre coinciden). Un click = UN archivo nuevo, exactamente _vNN.
    int n = SaveNextVersionN();
    // 1) guardado NORMAL de siempre (la ubicacion actual queda con lo ultimo)
    if (!SaveW3D(w3dPath)) return false;   // SaveW3D ya notifico el motivo
    // 2) la version ES una copia del archivo recien guardado, adentro de versions/
    //    (ShowCopyFile crea la carpeta del destino si no existe)
    std::string destination = dir + "/" + ShowNameOf(base, n);
    // (el aviso sale en el idioma del editor y lleva el nombre del archivo, sin cota: el buffer se mide con los dos)
    const std::string nameShow = ShowNameOf(base, n);
    std::vector<char> b(nameShow.size() + 512);
    if (ShowCopyFile(w3dPath, destination)) {
        W3dDownloadIfWeb(destination);   // navegador: la copia de la version tambien baja a la PC
        snprintf(&b[0], b.size(), T("Version v%d saved: versiones/%s"), n, nameShow.c_str());
        Notify(&b[0], false);
        w3dLogf("[Version] v%d = copia de %s en %s", n, ShowBase(w3dPath).c_str(), destination.c_str());
    } else {
        remove(destination.c_str());   // una copia a medias no es una version
        snprintf(&b[0], b.size(), T("Saved OK, but copying version v%d failed (see the log)"), n);
        Notify(&b[0], true);
        w3dLogfE("[Version] no pude copiar %s a %s", w3dPath.c_str(), destination.c_str());
    }
    return true;
}
