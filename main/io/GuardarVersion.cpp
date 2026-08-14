// ============================================================================
//  GuardarVersion.cpp — ver GuardarVersion.h (guardado por versiones del .w3d).
//
//  UNA VERSION ES UNA COPIA DEL ARCHIVO. Con el formato v4 el .w3d es un
//  CONTENEDOR (un zip con el proyecto entero adentro), asi que copiarlo es
//  copiar la version completa: escenas, scripts, texturas e icono incluidos.
//
//  Las versiones se AGRUPAN en una carpeta 'versiones/' al lado del proyecto,
//  conservando el nombre con sufijo:
//      miProyecto.w3d
//      versiones/miProyecto_v01.w3d
//      versiones/miProyecto_v02.w3d
//
//  CAMBIO DELIBERADO (pedido del dueno, textual: "y si quiero que hagas la
//  carpeta versiones y adentro metas el test1_v01.w3d"). Antes los _vNN.w3d
//  quedaban SUELTOS al lado del proyecto y a la decima version la carpeta de
//  trabajo era ilegible. OJO: esto NO es la vuelta a la carpeta 'versiones/vN/'
//  con assets.zip adentro que el dueno rechazo; el archivo sigue siendo UNO solo
//  y autocontenido, lo unico que cambia es DONDE vive. La numeracion mira los
//  archivos que ya hay ADENTRO de versiones/.
//
//  Se fue con esto la maquinaria vieja de "adiviná qué string es un asset"
//  (VerColectarDeJVal / VerResolverReferencia / VerExtensionDeAsset) y el
//  assets.zip que aparecia al lado y que el dueno reporto. Y de paso se arregla
//  un bug de silencio: una textura importada desde ~/Imagenes quedaba con ruta
//  ABSOLUTA y el colector la descartaba SIN AVISAR, asi que la version vN no se
//  podia restaurar completa. Con el contenedor eso no puede pasar: la version es
//  el archivo entero.
//
//  Lo que SIGUE afuera son las referencias "ext:" (las que el usuario decidio
//  dejar afuera a proposito): se avisa por notificacion y estan listadas en el
//  EXTERNOS.txt de adentro del .w3d.
// ============================================================================
#include "io/GuardarVersion.h"

#ifdef W3D_SYMBIAN
// En el build Symbian el guardado de proyecto (GuardarW3D) y el W3dZip del Core
// no estan en el .mmp todavia: el guardado por versiones queda como stub no-op
// (el boton de la tarjeta Archivo no existe en esa UI). El .cpp igual se compila
// (regla del repo: todo .cpp nuevo de main/ entra al Whisk3D.mmp).
int GuardarVersionSiguienteN() { return 1; }
std::string GuardarVersionLabel() { return "Guardar version v1"; }
bool GuardarVersionEjecutar() { return false; }
void GuardarVersionColectarDe(const std::string&, const std::string&,
                              const std::string&, std::set<std::string>*) {}
#else

#include "io/GuardarW3D.h"            // GuardarW3D (el guardado normal) + g_proyIcono
#include "io/JsonW3d.h"               // parser JSON minimo (colectar referencias)

#include "ViewPorts/Notificaciones.h"
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
static std::string VerCarpeta(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : r.substr(0, s);
}
static std::string VerBase(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? r : r.substr(s + 1);
}

// crea la ruta de carpetas completa ("a/b/c"): mkdir por segmento (existente = ok)
static void VerMkdirRec(const std::string& dir) {
    for (size_t i = 1; i < dir.size(); i++)
        if (dir[i] == '/' || dir[i] == '\\')
            W3D_MKDIR(dir.substr(0, i).c_str());
    W3D_MKDIR(dir.c_str());
}

// copia BYTES src -> dst creando las carpetas del destino. false si fallo.
static bool VerCopiarArchivo(const std::string& src, const std::string& dst) {
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(src, datos)) return false;
    VerMkdirRec(VerCarpeta(dst));
    FILE* f = fopen(dst.c_str(), "wb");
    if (!f) return false;
    size_t esc = datos.empty() ? 0 : fwrite(&datos[0], 1, datos.size(), f);
    // fclose PRIMERO y sin corto: es el que hace el flush, o sea donde aparece el disco lleno
    bool cerro = (fclose(f) == 0);
    return cerro && esc == datos.size();
}

// ---------------------------------------------------------------------------
//  colector de referencias: strings del JSON que son rutas relativas a assets
// ---------------------------------------------------------------------------
static bool VerExtensionDeAsset(const std::string& r) {
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
// VerCarpeta, que devuelve "." para poder concatenar rutas de disco)
static std::string VerCarpetaRel(const std::string& r) {
    size_t s = r.find_last_of("/\\");
    return (s == std::string::npos) ? std::string() : r.substr(0, s);
}

// un string candidato es referencia si: es relativo, tiene extension de asset,
// existe en disco bajo el proyecto y no apunta a versiones/ (evita meter
// versiones viejas dentro de una version nueva). Se resuelve PRIMERO contra la
// carpeta del archivo que lo referencia ('baseRel', relativa al proyecto; "" =
// la raiz) y despues contra la raiz del proyecto (mismo fallback que RutaJson
// al abrir). Devuelve la ruta relativa AL PROYECTO ("" = no es referencia).
static std::string VerResolverReferencia(const std::string& s, const std::string& baseRel,
                                         const std::string& dir) {
    if (s.empty() || s[0] == '/' || s[0] == '\\') return std::string();
    if (s.size() > 2 && s[1] == ':') return std::string();         // "C:\..." (Windows)
    if (!VerExtensionDeAsset(s)) return std::string();
    if (!baseRel.empty()) {
        std::string rel = baseRel + "/" + s;
        if (rel.compare(0, 10, "versiones/") != 0 && w3dFileSystem::FileExists(dir + "/" + rel))
            return rel;
    }
    if (s.compare(0, 10, "versiones/") == 0) return std::string();
    if (w3dFileSystem::FileExists(dir + "/" + s)) return s;
    return std::string();
}

static void VerColectarDeJVal(JVal* v, const std::string& baseRel, const std::string& dir,
                              std::set<std::string>* refs, std::vector<std::string>* w3duiNuevos) {
    if (!v) return;
    if (v->tipo == 2) {
        std::string rel = VerResolverReferencia(v->str, baseRel, dir);
        if (!rel.empty() && refs->insert(rel).second) {
            // las escenas .w3dui referencian sus propios assets: se parsean tambien
            size_t p = rel.find_last_of('.');
            if (p != std::string::npos && rel.substr(p) == ".w3dui")
                w3duiNuevos->push_back(rel);
        }
        return;
    }
    for (std::map<std::string, JVal*>::iterator it = v->obj.begin(); it != v->obj.end(); ++it)
        VerColectarDeJVal(it->second, baseRel, dir, refs, w3duiNuevos);
    for (size_t i = 0; i < v->lista.size(); i++)
        VerColectarDeJVal(v->lista[i], baseRel, dir, refs, w3duiNuevos);
}

// parsea un JSON del disco y colecta sus referencias (el .w3d o un .w3dui)
static void VerColectarDeArchivo(const std::string& ruta, const std::string& baseRel,
                                 const std::string& dir,
                                 std::set<std::string>* refs, std::vector<std::string>* w3duiNuevos) {
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(ruta, datos) || datos.empty()) return;
    JParser parser((const char*)&datos[0], datos.size());
    JVal* raiz = parser.Valor();
    if (!parser.error) VerColectarDeJVal(raiz, baseRel, dir, refs, w3duiNuevos);
    delete raiz;
}

// API publica (la reusa CompilarJuego): TODO lo relativo que 'rutaArchivo' (un
// .w3d o un .w3dui en disco) y sus escenas referencian, como rutas relativas
// al proyecto. Los .w3dui encadenados se parsean con SU carpeta como base.
void GuardarVersionColectarDe(const std::string& rutaArchivo, const std::string& baseRel,
                              const std::string& dirProyecto, std::set<std::string>* refs) {
    std::vector<std::string> pendientes;   // .w3dui por parsear (los agrega el colector)
    VerColectarDeArchivo(rutaArchivo, baseRel, dirProyecto, refs, &pendientes);
    // los .w3dui pueden referenciar otros .w3dui (se procesan en cadena, sin repetir:
    // el set 'refs' ya deduplica y solo lo NUEVO entra a 'pendientes')
    for (size_t i = 0; i < pendientes.size(); i++)
        VerColectarDeArchivo(dirProyecto + "/" + pendientes[i], VerCarpetaRel(pendientes[i]),
                             dirProyecto, refs, &pendientes);
}

// ---------------------------------------------------------------------------
//  NUMERACION: versiones/<proyecto>_v01.w3d, _v02.w3d, ...
//
//  Se escanea la carpeta versiones/ buscando "<base>_vNN.w3d". Asi el numero no
//  depende de ningun estado guardado (borrar una version vieja no rompe la
//  cuenta) y el label del boton nunca miente.
// ---------------------------------------------------------------------------

// la carpeta donde viven las versiones de 'w3d' (no se crea aca: la crea la copia)
static std::string VerDirVersiones(const std::string& w3d) {
    return VerCarpeta(w3d) + "/versiones";
}
static std::string VerBaseSinExt(const std::string& r) {
    std::string b = VerBase(r);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}

// "miProyecto_v03.w3d" -> 3;  0 = no es una version de ESTE proyecto
static int VerNumeroDe(const std::string& archivo, const std::string& base) {
    const std::string pref = base + "_v";
    if (archivo.size() <= pref.size() + 4) return 0;                 // + al menos 1 digito + ".w3d"
    if (archivo.compare(0, pref.size(), pref) != 0) return 0;
    size_t p = archivo.size() - 4;
    if (archivo.compare(p, 4, ".w3d") != 0) return 0;
    if (p <= pref.size()) return 0;
    for (size_t i = pref.size(); i < p; i++)
        if (archivo[i] < '0' || archivo[i] > '9') return 0;
    return atoi(archivo.c_str() + pref.size());
}

// el nombre EXACTO de la version N ("miProyecto_v01.w3d"). Dos digitos hasta la
// 99 para que el explorador de archivos las ordene bien.
static std::string VerNombreDe(const std::string& base, int n) {
    char b[32]; snprintf(b, sizeof(b), "_v%02d.w3d", n);
    return base + b;
}

static int VerMaxVersionEn(const std::string& dir, const std::string& base) {
    std::vector<w3dFileSystem::DirEntry> ents;
    if (!w3dFileSystem::ListDir(dir, ents)) return 0;
    int maxN = 0;
    for (size_t i = 0; i < ents.size(); i++) {
        if (ents[i].isDir) continue;
        int n = VerNumeroDe(ents[i].name, base);
        if (n > maxN) maxN = n;
    }
    return maxN;
}

// el numero de la version que el boton VA A CREAR (el label no debe mentir):
// 1 si no hay ninguna, sino max + 1. Secuencial simple.
int GuardarVersionSiguienteN() {
    if (w3dPath.empty()) return 1;
    // adentro de versiones/. Si la carpeta todavia no existe, ListDir falla y
    // VerMaxVersionEn devuelve 0 -> la primera version es la v1, como corresponde.
    return VerMaxVersionEn(VerDirVersiones(w3dPath), VerBaseSinExt(w3dPath)) + 1;
}

std::string GuardarVersionLabel() {
    char b[48];
    snprintf(b, sizeof(b), "Guardar version v%d", GuardarVersionSiguienteN());
    return std::string(b);
}

// ---------------------------------------------------------------------------
//  el flujo completo del boton: guardar normal + UNA COPIA del .w3d al lado
// ---------------------------------------------------------------------------
bool GuardarVersionEjecutar() {
    if (w3dPath.empty()) {
        Notificar("Guardar version: el proyecto no tiene ubicacion (usa Guardar primero)", true);
        return false;
    }
    std::string dir  = VerDirVersiones(w3dPath);   // <carpeta del proyecto>/versiones
    std::string base = VerBaseSinExt(w3dPath);
    // el N del snapshot: el mismo que mostraba el boton (label y archivo creado
    // siempre coinciden). Un click = UN archivo nuevo, exactamente _vNN.
    int n = GuardarVersionSiguienteN();
    // 1) guardado NORMAL de siempre (la ubicacion actual queda con lo ultimo)
    if (!GuardarW3D(w3dPath)) return false;   // GuardarW3D ya notifico el motivo
    // 2) la version ES una copia del archivo recien guardado, adentro de versiones/
    //    (VerCopiarArchivo crea la carpeta del destino si no existe)
    std::string destino = dir + "/" + VerNombreDe(base, n);
    char b[400];
    if (VerCopiarArchivo(w3dPath, destino)) {
        snprintf(b, sizeof(b), "Version v%d guardada: versiones/%s", n, VerNombreDe(base, n).c_str());
        Notificar(b, false);
        w3dLogf("[Version] v%d = copia de %s en %s", n, VerBase(w3dPath).c_str(), destino.c_str());
    } else {
        remove(destino.c_str());   // una copia a medias no es una version
        snprintf(b, sizeof(b), "Guardado OK, pero fallo la copia de la version v%d (ver log)", n);
        Notificar(b, true);
        w3dLogfE("[Version] no pude copiar %s a %s", w3dPath.c_str(), destino.c_str());
    }
    return true;
}

#endif // !W3D_SYMBIAN
