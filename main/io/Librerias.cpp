// ============================================================================
//  Librerias.cpp — ver Librerias.h. El registro de las librerias externas, su
//  montaje (un almacen con nombre por libreria), sus recursos con prefijo y sus
//  elementos (los prefabs y escenas que un proxy genera). Compila en el editor y
//  en el runtime 3D (C++03).
// ============================================================================
#include "io/Librerias.h"
#include "io/JsonW3d.h"
#include "objects/Objects.h"           // Object::serial: las referencias pendientes van por serial
#include "io/W3dAlmacen.h"             // AlmacenZip + el registro de almacenes con nombre
#include "io/UI2DFormato.h"            // g_w3dDirProyecto / W3dRutaBajoCarpeta: la base de las rutas "ext:"
#include "objects/Materials.h"
#include "script/W3dScript.h"       // W3dScriptRutaSueltaHook: lo que un script de una libreria nombra al lado de su .w3d
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
struct LibMontada {
    std::string ruta;                  // el .w3d de disco que se monto ("" = empaquetada: el juego compilado)
    AlmacenZip* almacen;               // su almacen con nombre (NULL = empaquetada o no se pudo abrir)
    bool registrada;                   // sus recursos ya se registraron (con su prefijo)
    bool fallo;                        // no se pudo leer (no se reintenta hasta que cambie su ruta)
    std::string motivo;
    // sus ELEMENTOS: nombre -> entrada ("" = la escena del bloque "escena" de su proyecto.json)
    std::vector<std::string> prefabs, prefabEntradas;
    std::vector<std::string> escenas, escenaEntradas;
    // OCULTA: la monto OTRA libreria (una de adentro, W3dLibsAnidada): no esta en el registro del proyecto. Su nombre
    // lleva '~' ("Personajes~Armas"), que un nombre del registro no puede tener: un proxy o un recurso del PROYECTO
    // nunca la nombra por error
    bool oculta;
    std::vector<W3dLibFila> sub;       // SU registro "librerias" (las de adentro), resuelto contra SU carpeta
    LibMontada() : almacen(0), registrada(false), fallo(false), oculta(false) {}
};
}
static std::vector<W3dLibFila> gFilas;
static std::map<std::string, LibMontada> gMontadas;
static bool gMontarDeDisco = true;       // false = el juego compilado (sus librerias vienen empaquetadas)
static std::map<std::string, Material*> gMatLib;   // "Personajes/Piel" -> el material de la libreria
static std::vector<std::string> gContexto;         // la pila del contexto de carga
// LIBRERIA ADENTRO DE LIBRERIA: "<de>\n<nombre>" -> el nombre con que vive en memoria. En el editor es un cache
// (se valida contra los archivos en cada uso); en el juego compilado es EL mapeo ("librerias/_anidadas.json")
static std::map<std::string, std::string> gAlias;
static std::map<std::string, std::string> gAliasJuego;
const char* const kW3dLibsAnidadasEntrada = "librerias/_anidadas.json";
// LAS REFERENCIAS PENDIENTES del proyecto a librerias que no estan: (serial del objeto, tipo) -> la referencia
static std::map<std::pair<unsigned, int>, W3dLibRef> gRefsPend;

int W3dLibsCantidad() { return (int)gFilas.size(); }
const W3dLibFila& W3dLibsFila(int i) {
    static const W3dLibFila vacia;
    return (i >= 0 && i < (int)gFilas.size()) ? gFilas[(size_t)i] : vacia;
}
int W3dLibsBuscar(const std::string& nombre) {
    for (size_t i = 0; i < gFilas.size(); i++) if (gFilas[i].nombre == nombre) return (int)i;
    return -1;
}

static bool EsAbsoluta(const std::string& r) {
    return !r.empty() && (r[0] == '/' || r[0] == '\\' || (r.size() > 1 && r[1] == ':'));
}
static void Tramos(const std::string& r, std::vector<std::string>& out);   // (mas abajo)
// una ruta de disco COMPARABLE: separadores '/', sin "." ni ".." (dos rutas al mismo archivo dan lo mismo)
static std::string Normalizar(const std::string& r) {
    if (r.empty()) return r;
    std::vector<std::string> t, o;
    Tramos(r, t);
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == ".." && !o.empty() && o.back() != "..") o.pop_back();
        else o.push_back(t[i]);
    }
    std::string n = (r[0] == '/' || r[0] == '\\') ? "/" : "";
    for (size_t i = 0; i < o.size(); i++) { if (i) n += '/'; n += o[i]; }
    return n;
}
// la carpeta de un archivo ("" si no tiene)
static std::string CarpetaDe(const std::string& r) {
    const size_t b = r.find_last_of("/\\");
    return (b == std::string::npos) ? std::string() : r.substr(0, b);
}
// "ext:..." -> la ruta de disco, contra la carpeta del .w3d abierto
// (normalizada: sin "." ni "..", asi el "ext:" que se recalcula al guardar en otra carpeta sale limpio)
static std::string ResolverJson(const std::string& j) {
    std::string r = (j.compare(0, 4, "ext:") == 0) ? j.substr(4) : j;
    if (!EsAbsoluta(r) && !g_w3dDirProyecto.empty()) return w3dFileSystem::JoinPath(g_w3dDirProyecto, r);
    return r;
}
std::string W3dLibsRutaDisco(int i) {
    if (i < 0 || i >= (int)gFilas.size()) return std::string();
    const W3dLibFila& f = gFilas[(size_t)i];
    return f.rutaDisco.empty() ? ResolverJson(f.rutaJson) : f.rutaDisco;
}

// los tramos de una ruta ('/' o '\\'), sin vacios ni "."
static void Tramos(const std::string& r, std::vector<std::string>& out) {
    out.clear();
    std::string t;
    for (size_t i = 0; i <= r.size(); i++) {
        const char c = (i < r.size()) ? r[i] : '/';
        if (c == '/' || c == '\\') { if (!t.empty() && t != ".") out.push_back(t); t.clear(); }
        else t += c;
    }
}
std::string W3dLibsRutaParaJson(const std::string& rutaDisco) {
    // adentro de la carpeta del .w3d: relativa (la misma regla que las referencias externas del contenedor)
    std::string rel;
    if (W3dRutaBajoCarpeta(rutaDisco, g_w3dDirProyecto, rel)) return "ext:" + rel;
    // al lado o cerca (una carpeta hermana, "../personajes.w3d"): relativa con "..", asi el nivel y su libreria
    // se mudan juntos. Si no comparten nada mas que la raiz (otra unidad, otro arbol), absoluta
    if (EsAbsoluta(rutaDisco) && EsAbsoluta(g_w3dDirProyecto) && rutaDisco[0] == g_w3dDirProyecto[0]) {
        std::vector<std::string> a, b;
        Tramos(g_w3dDirProyecto, a);
        Tramos(rutaDisco, b);
        size_t k = 0;
        while (k < a.size() && k + 1 < b.size() && a[k] == b[k]) k++;
        if (k >= 1 && !(a.size() > 0 && a[0].size() == 2 && a[0][1] == ':' && k == 1)) {
            std::string r;
            for (size_t i = k; i < a.size(); i++) r += "../";
            for (size_t i = k; i < b.size(); i++) { r += b[i]; if (i + 1 < b.size()) r += '/'; }
            return "ext:" + r;
        }
    }
    return "ext:" + rutaDisco;
}

// un nombre que ya usa una libreria (vinculada, o montada en esta sesion: una desvinculada o una oculta siguen
// montadas hasta cerrar el proyecto con lo que se cargo de ellas, y OTRO archivo con su nombre las pisaria). La que
// se desvinculo y vuelve a vincularse con el MISMO archivo ('ruta') recupera su nombre (sus proxies la nombran asi)
static bool NombreUsado(const std::string& n, const std::string& ruta = std::string()) {
    if (W3dLibsBuscar(n) >= 0) return true;
    std::map<std::string, LibMontada>::const_iterator it = gMontadas.find(n);
    if (it == gMontadas.end()) return false;
    return it->second.oculta || ruta.empty() || it->second.ruta.empty() || Normalizar(it->second.ruta) != Normalizar(ruta);
}

std::string W3dLibsNombreLibre(const std::string& base0, const std::string& ruta) {
    std::string base;
    for (size_t i = 0; i < base0.size(); i++) {
        const char c = base0[i];
        // ('~' es de las OCULTAS: las librerias de adentro de otra)
        base += (c == '/' || c == '\\' || c == ':' || c == '"' || c == '~') ? '_' : c;
    }
    if (base.empty()) base = "Libreria";
    std::string n = base;
    for (int k = 1; NombreUsado(n, ruta); k++) {
        char b[16]; sprintf(b, ".%03d", k);
        n = base + b;
    }
    return n;
}

// vincular (otra vez) una libreria es volver a intentar leerla: su archivo pudo volver a disco despues de un intento
// fallido (que, si no, se recuerda hasta que cambie su ruta)
static void ReintentarSiFallo(const std::string& nombre) {
    std::map<std::string, LibMontada>::iterator it = gMontadas.find(nombre);
    if (it != gMontadas.end() && it->second.fallo) { it->second.fallo = false; it->second.motivo.clear(); }
}
int W3dLibsAgregar(const std::string& nombre, const std::string& rutaDisco) {
    if (nombre.empty() || W3dLibsBuscar(nombre) >= 0) return -1;
    W3dLibFila f; f.nombre = nombre; f.rutaDisco = rutaDisco; f.rutaJson = W3dLibsRutaParaJson(rutaDisco);
    gFilas.push_back(f);
    ReintentarSiFallo(nombre);
    return (int)gFilas.size() - 1;
}
bool W3dLibsQuitar(int i) {
    if (i < 0 || i >= (int)gFilas.size()) return false;
    gFilas.erase(gFilas.begin() + (long)i);
    return true;
}
std::vector<W3dLibFila> W3dLibsFilas() { return gFilas; }
void W3dLibsFijar(const std::vector<W3dLibFila>& filas) {
    gFilas = filas;
    for (size_t i = 0; i < gFilas.size(); i++) ReintentarSiFallo(gFilas[i].nombre);
}

// una fila de un registro "librerias" (el del proyecto o el de adentro de una libreria): su nombre (sin nombre, el
// del archivo sin ".w3d") y su "archivo" como vino. false = no es una fila
static bool FilaDeJson(JVal* e, W3dLibFila& f) {
    if (!e || e->tipo != 4) return false;
    std::string r = JS(e, "archivo", "");
    if (r.empty()) r = JS(e, "ruta", "");   // (la primera escritura del registro, sin nombre)
    if (r.empty()) return false;
    std::string n = JS(e, "nombre", "");
    if (n.empty()) {
        std::string b = (r.compare(0, 4, "ext:") == 0) ? r.substr(4) : r;
        const size_t s = b.find_last_of("/\\");
        if (s != std::string::npos) b = b.substr(s + 1);
        const size_t p = b.find_last_of('.');
        if (p != std::string::npos && p > 0) b = b.substr(0, p);
        n = b;
    }
    f.nombre = n; f.rutaJson = r; f.rutaDisco.clear();
    return true;
}

// el mapeo de las librerias de adentro del JUEGO COMPILADO ("librerias/_anidadas.json", lo escribe Compilar juego):
// {"anidadas": [{"libreria": "<de>", "nombre": "<la de adentro>", "como": "<el nombre con que vive>"}]}
static void LeerAnidadasJuego() {
    gAliasJuego.clear();
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(kW3dLibsAnidadasEntrada, d) || d.empty()) return;
    JParser p((const char*)&d[0], d.size());
    JVal* v = p.Valor();
    JVal* l = (!p.error && v && v->tipo == 4) ? JHijo(v, "anidadas", 5) : NULL;
    if (l)
        for (size_t i = 0; i < l->lista.size(); i++) {
            JVal* e = l->lista[i];
            if (!e || e->tipo != 4) continue;
            const std::string de = JS(e, "libreria", ""), n = JS(e, "nombre", ""), como = JS(e, "como", "");
            if (!de.empty() && !n.empty() && !como.empty()) gAliasJuego[de + "\n" + n] = como;
        }
    delete v;
    if (!gAliasJuego.empty()) w3dLogf("[librerias] %d libreria(s) de adentro de otras (empaquetadas)", (int)gAliasJuego.size());
}

void W3dLibsLeerJson(JVal* raiz, bool montarDeDisco) {
    gFilas.clear();
    gMontarDeDisco = montarDeDisco;
    gAlias.clear();
    if (!montarDeDisco) LeerAnidadasJuego();
    JVal* l = raiz ? JHijo(raiz, "librerias", 5) : NULL;
    if (!l) return;
    for (size_t i = 0; i < l->lista.size(); i++) {
        W3dLibFila f;
        if (!FilaDeJson(l->lista[i], f)) continue;
        if (W3dLibsBuscar(f.nombre) >= 0) {
            w3dLogfW("[librerias] '%s' esta dos veces en el registro (queda la primera)", f.nombre.c_str());
            continue;
        }
        f.rutaDisco = montarDeDisco ? ResolverJson(f.rutaJson) : std::string();   // (la carpeta del .w3d ya esta fijada)
        gFilas.push_back(f);
    }
}

static void JTexto(std::string& s, const std::string& t) {
    s += '"';
    for (size_t k = 0; k < t.size(); k++) { if (t[k] == '"' || t[k] == '\\') s += '\\'; s += t[k]; }
    s += '"';
}
void W3dLibsGuardarJson(std::string& s) {
    if (gFilas.empty()) return;
    s += "  \"librerias\": [";
    for (size_t i = 0; i < gFilas.size(); i++) {
        s += (i ? ", " : " ");
        // (el "ext:" se recalcula contra la carpeta del .w3d que se esta guardando: puede ser otra)
        const std::string archivo = gFilas[i].rutaDisco.empty() ? gFilas[i].rutaJson : W3dLibsRutaParaJson(gFilas[i].rutaDisco);
        s += "{ \"nombre\": "; JTexto(s, gFilas[i].nombre);
        s += ", \"archivo\": "; JTexto(s, archivo);
        s += " }";
    }
    s += " ],\n";
}

void W3dLibsCerrarProyecto() {
    for (std::map<std::string, LibMontada>::iterator it = gMontadas.begin(); it != gMontadas.end(); ++it) {
        if (!it->second.almacen) continue;
        W3dAlmacenRegistrar(it->first, NULL);
        delete it->second.almacen;
        it->second.almacen = 0;
    }
    gMontadas.clear();
    gFilas.clear();
    gMatLib.clear();   // (los materiales se liberan con el proyecto: MaterialesLiberarEscena)
    gContexto.clear();
    gAlias.clear();
    gAliasJuego.clear();
    gRefsPend.clear();
    gMontarDeDisco = true;
}

// ============================================================================
//  USARLA
// ============================================================================
// sus prefabs y escenas, del proyecto.json de la libreria
static void LeerElementos(LibMontada& L, JVal* raiz) {
    L.prefabs.clear(); L.prefabEntradas.clear(); L.escenas.clear(); L.escenaEntradas.clear();
    JVal* jp = JHijo(raiz, "prefabs", 5);
    if (jp)
        for (size_t i = 0; i < jp->lista.size(); i++) {
            JVal* e = jp->lista[i];
            if (!e || e->tipo != 4) continue;
            const std::string n = JS(e, "nombre", ""), ent = JS(e, "entrada", "");
            if (n.empty() || ent.empty()) continue;
            L.prefabs.push_back(n); L.prefabEntradas.push_back(ent);
        }
    JVal* je = JHijo(raiz, "escenas3d", 5);
    if (je) {
        for (size_t i = 0; i < je->lista.size(); i++) {
            JVal* e = je->lista[i];
            if (!e || e->tipo != 4) continue;
            const std::string n = JS(e, "nombre", "");
            if (n.empty()) continue;
            L.escenas.push_back(n); L.escenaEntradas.push_back(JS(e, "entrada", ""));   // "" = la del bloque
        }
    } else if (JHijo(raiz, "escena", 4)) {
        // un proyecto de UNA escena (sin registro): la del bloque, con el nombre por defecto
        L.escenas.push_back("Scene"); L.escenaEntradas.push_back(std::string());
    }
}

// SU registro "librerias" (las de adentro): los "ext:" relativos son relativos a SU carpeta
static void LeerSub(LibMontada& L, JVal* raiz) {
    L.sub.clear();
    JVal* l = JHijo(raiz, "librerias", 5);
    if (!l) return;
    const std::string dir = CarpetaDe(L.ruta);
    for (size_t i = 0; i < l->lista.size(); i++) {
        W3dLibFila f;
        if (!FilaDeJson(l->lista[i], f)) continue;
        bool rep = false;
        for (size_t k = 0; k < L.sub.size() && !rep; k++) rep = (L.sub[k].nombre == f.nombre);
        if (rep) continue;
        if (gMontarDeDisco && !L.ruta.empty()) {
            const std::string x = (f.rutaJson.compare(0, 4, "ext:") == 0) ? f.rutaJson.substr(4) : f.rutaJson;
            f.rutaDisco = Normalizar(EsAbsoluta(x) ? x : (dir.empty() ? x : dir + "/" + x));
        }
        L.sub.push_back(f);
    }
}

// monta la libreria 'nombre' (su .w3d, como almacen con su nombre) si hace falta y se puede
static bool Montar(const std::string& nombre, LibMontada& L, std::string* motivo) {
    const int i = W3dLibsBuscar(nombre);
    const std::string ruta = (i >= 0) ? W3dLibsRutaDisco(i) : L.ruta;
    if (L.almacen && L.ruta == ruta) return true;
    if (L.almacen) {   // (la ruta cambio: se vuelve a montar)
        W3dAlmacenRegistrar(nombre, NULL);
        delete L.almacen;
        L.almacen = 0;
    }
    L.ruta = ruta;
    if (!gMontarDeDisco || ruta.empty()) return true;   // (empaquetada: la lee el fallback de ReadFileBytes)
    AlmacenZip* a = new AlmacenZip();
    if (!a->Abrir(ruta) || !a->Existe("proyecto.json")) {
        delete a;
        if (motivo) *motivo = "The library file can't be opened";
        w3dLogfE("[librerias] no pude montar '%s' (%s)", nombre.c_str(), ruta.c_str());
        return false;
    }
    L.almacen = a;
    W3dAlmacenRegistrar(nombre, a);
    w3dLogf("[librerias] montada '%s' (%s)", nombre.c_str(), ruta.c_str());
    return true;
}

// W3dAlmacenPedirHook (io/W3dAlmacen.h): alguien lee "lib:<nombre>/..." de una libreria VINCULADA que todavia no
// se monto (la textura de un material del proyecto que nombra una suya): solo se MONTA (sus recursos se registran
// cuando algo los pide por nombre). En el juego compilado no se monta nada: la copia empaquetada
static bool PedirAlmacen(const std::string& nombre) {
    if (!gMontarDeDisco) return false;
    std::map<std::string, LibMontada>::iterator it = gMontadas.find(nombre);
    const bool oculta = W3dLibsBuscar(nombre) < 0 && it != gMontadas.end() && it->second.oculta;
    if (W3dLibsBuscar(nombre) < 0 && !oculta) return false;
    LibMontada& L = gMontadas[nombre];
    if (L.fallo && L.ruta == W3dLibsRutaDiscoDe(nombre)) return false;
    std::string m;
    if (!Montar(nombre, L, &m)) { L.fallo = true; L.motivo = m; return false; }
    return L.almacen != NULL;
}
namespace {
struct RegistrarPedirAlmacen {
    RegistrarPedirAlmacen() {
        W3dAlmacenPedirHook = PedirAlmacen;
        W3dScriptRutaSueltaHook = W3dLibsRutaSuelta;   // (un archivo suelto al lado del .w3d de una libreria)
    }
} gRegistrarPedirAlmacen;
}

bool W3dLibsAsegurar(const std::string& nombre, std::string* motivo) {
    if (nombre.empty()) { if (motivo) *motivo = "No library"; return false; }
    std::map<std::string, LibMontada>::iterator it = gMontadas.find(nombre);
    const bool vinculada = W3dLibsBuscar(nombre) >= 0;
    const bool oculta = !vinculada && it != gMontadas.end() && it->second.oculta;
    // una DESVINCULADA ya no sirve para generar nada (lo que ya se cargo de ella sigue vivo hasta cerrar el proyecto:
    // sus proxies se regeneran vacios, igual que al reabrir el nivel sin ella)
    if (!vinculada && !oculta) { if (motivo) *motivo = "The library isn't linked"; return false; }
    if (vinculada && it != gMontadas.end()) it->second.oculta = false;
    // YA LISTA
    if (it != gMontadas.end() && it->second.registrada) {
        if (vinculada && W3dLibsRutaDisco(W3dLibsBuscar(nombre)) != it->second.ruta && gMontarDeDisco) {
            // (la misma libreria con OTRO archivo: se vuelve a leer)
            it->second.registrada = false;
            it->second.fallo = false;
        } else return true;
    }
    if (it == gMontadas.end()) it = gMontadas.insert(std::make_pair(nombre, LibMontada())).first;
    LibMontada& L = it->second;
    if (L.fallo && L.ruta == W3dLibsRutaDiscoDe(nombre)) { if (motivo) *motivo = L.motivo; return false; }
    L.fallo = false;
    if (!Montar(nombre, L, motivo)) { L.fallo = true; L.motivo = motivo ? *motivo : std::string(); return false; }
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes("lib:" + nombre + "/proyecto.json", datos) || datos.empty()) {
        L.fallo = true; L.motivo = "The library has no proyecto.json";
        if (motivo) *motivo = L.motivo;
        w3dLogfE("[librerias] '%s': no pude leer su proyecto.json", nombre.c_str());
        return false;
    }
    JParser p((const char*)&datos[0], datos.size());
    JVal* raiz = p.Valor();
    if (p.error || !raiz || raiz->tipo != 4) {
        delete raiz;
        L.fallo = true; L.motivo = "The library's proyecto.json can't be read";
        if (motivo) *motivo = L.motivo;
        w3dLogfE("[librerias] '%s': su proyecto.json no parsea", nombre.c_str());
        return false;
    }
    LeerElementos(L, raiz);
    LeerSub(L, raiz);   // (sus librerias de adentro: lo que su contenido nombra de ellas se resuelve con ESTE registro)
    // los materiales, las mallas y los animsets, con su prefijo (el lector: import_w3d)
    W3dLibRegistrarRecursos(nombre, raiz);
    delete raiz;
    L.registrada = true;
    w3dLogf("[librerias] '%s' lista: %d prefab(s), %d escena(s)", nombre.c_str(), (int)L.prefabs.size(),
            (int)L.escenas.size());
    return true;
}

std::string W3dLibsDeNombre(const std::string& n) {
    const size_t barra = n.find('/');
    if (barra == std::string::npos || barra == 0) return std::string();
    const std::string lib = n.substr(0, barra);
    if (W3dLibsBuscar(lib) < 0 && gMontadas.find(lib) == gMontadas.end()) return std::string();
    return W3dLibsAsegurar(lib, NULL) ? lib : std::string();
}

std::string W3dLibsRutaDiscoDe(const std::string& nombre) {
    const int i = W3dLibsBuscar(nombre);
    if (i >= 0) return W3dLibsRutaDisco(i);
    std::map<std::string, LibMontada>::const_iterator it = gMontadas.find(nombre);
    return (it == gMontadas.end()) ? std::string() : it->second.ruta;
}
bool W3dLibsDesdeDisco() { return gMontarDeDisco; }

std::string W3dLibsEntradaExterna(const std::string& rel) {
    std::vector<std::string> t, o;
    Tramos(rel, t);
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == ".." && !o.empty() && o.back() != "__") o.pop_back();
        else o.push_back(t[i] == ".." ? std::string("__") : t[i]);
    }
    std::string e = "_ext";
    for (size_t i = 0; i < o.size(); i++) e += "/" + o[i];
    return e;
}

std::string W3dLibsRutaSuelta(const std::string& lib, const std::string& rel) {
    if (!gMontarDeDisco || lib.empty() || rel.empty() || EsAbsoluta(rel)) return std::string();
    const std::string dir = CarpetaDe(W3dLibsRutaDiscoDe(lib));
    if (dir.empty()) return std::string();
    const std::string r = Normalizar(dir + "/" + rel);
    // (nunca afuera de su carpeta: lo que un script nombra de al lado de su libreria, no del disco entero)
    std::string dentro;
    if (!W3dRutaBajoCarpeta(r, Normalizar(dir), dentro)) return std::string();
    return w3dFileSystem::FileExists(r) ? r : std::string();
}

// ============================================================================
//  LIBRERIA ADENTRO DE LIBRERIA
// ============================================================================
void W3dLibsSubFilas(const std::string& lib, std::vector<W3dLibFila>& out) {
    out.clear();
    if (!W3dLibsAsegurar(lib, NULL)) return;
    out = gMontadas[lib].sub;
}

// el alias cacheado sigue valiendo: 'g' es la vinculada con ese archivo, o una oculta montada con el
static bool AliasVale(const std::string& g, const std::string& ruta) {
    const int i = W3dLibsBuscar(g);
    if (i >= 0) return Normalizar(W3dLibsRutaDisco(i)) == ruta;
    std::map<std::string, LibMontada>::const_iterator it = gMontadas.find(g);
    return it != gMontadas.end() && it->second.oculta && Normalizar(it->second.ruta) == ruta;
}

std::string W3dLibsAnidada(const std::string& de, const std::string& nombre) {
    if (de.empty() || nombre.empty()) return nombre;
    const std::string clave = de + "\n" + nombre;
    if (!gMontarDeDisco) {
        // EL JUEGO COMPILADO: el mapeo que dejo Compilar juego (una oculta queda conocida para asegurarla)
        std::map<std::string, std::string>::const_iterator a = gAliasJuego.find(clave);
        if (a == gAliasJuego.end()) return nombre;
        if (W3dLibsBuscar(a->second) < 0) gMontadas[a->second].oculta = true;
        return a->second;
    }
    // EL EDITOR: por el ARCHIVO de su registro
    if (!W3dLibsAsegurar(de, NULL)) return nombre;
    const std::vector<W3dLibFila>& sub = gMontadas[de].sub;
    std::string ruta;
    bool esta = false;
    for (size_t i = 0; i < sub.size() && !esta; i++)
        if (sub[i].nombre == nombre) { esta = true; ruta = sub[i].rutaDisco; }
    if (!esta || ruta.empty()) return nombre;   // (no es de su registro: la del nivel con ese nombre, si hay)
    std::string g;
    // 1) el NIVEL vincula ese mismo archivo: es esa (comparten memoria). Se mira SIEMPRE antes que el cache: el nivel
    //    la puede haber vinculado despues de que se monto oculta (lo que se genere desde ahora usa la del nivel)
    for (size_t i = 0; i < gFilas.size() && g.empty(); i++)
        if (Normalizar(W3dLibsRutaDisco((int)i)) == ruta) g = gFilas[i].nombre;
    if (!g.empty()) { gAlias[clave] = g; return g; }
    std::map<std::string, std::string>::const_iterator c = gAlias.find(clave);
    if (c != gAlias.end() && AliasVale(c->second, ruta)) return c->second;
    // 2) ya hay una OCULTA con ese archivo (otra libreria la trae tambien): esa
    for (std::map<std::string, LibMontada>::iterator it = gMontadas.begin(); it != gMontadas.end() && g.empty(); ++it)
        if (it->second.oculta && W3dLibsBuscar(it->first) < 0 && Normalizar(it->second.ruta) == ruta) g = it->first;
    // 3) una OCULTA nueva: "<de>~<nombre>" (o uno libre)
    if (g.empty()) {
        const std::string base = de + "~" + nombre;
        g = base;
        for (int k = 1; NombreUsado(g); k++) { char b[16]; sprintf(b, ".%03d", k); g = base + b; }
        LibMontada& N = gMontadas[g];
        N.oculta = true;
        N.ruta = ruta;
        w3dLogf("[librerias] '%s' de adentro de '%s' se monta oculta como '%s' (%s)", nombre.c_str(), de.c_str(),
                g.c_str(), ruta.c_str());
    }
    gAlias[clave] = g;
    return g;
}

std::string W3dLibsNombreAnidado(const std::string& de, const std::string& n) {
    if (de.empty()) return std::string();
    const size_t barra = n.find('/');
    if (barra == std::string::npos || barra == 0 || barra + 1 >= n.size()) return std::string();
    const std::string p = n.substr(0, barra);
    bool esDeSuRegistro = false;
    if (!gMontarDeDisco) esDeSuRegistro = gAliasJuego.find(de + "\n" + p) != gAliasJuego.end();
    else if (W3dLibsAsegurar(de, NULL)) {
        const std::vector<W3dLibFila>& sub = gMontadas[de].sub;
        for (size_t i = 0; i < sub.size() && !esDeSuRegistro; i++) esDeSuRegistro = (sub[i].nombre == p);
    }
    if (!esDeSuRegistro) return std::string();
    const std::string g = W3dLibsAnidada(de, p);
    W3dLibsAsegurar(g, NULL);   // (sus recursos se registran al usarla)
    return g + n.substr(barra);
}

// ============================================================================
//  LOS MATERIALES DE LAS LIBRERIAS
// ============================================================================
Material* W3dLibMaterial(const std::string& n) {
    std::map<std::string, Material*>::const_iterator it = gMatLib.find(n);
    return it == gMatLib.end() ? NULL : it->second;
}
void W3dLibMaterialRegistrar(const std::string& n, Material* m) {
    if (n.empty()) return;
    if (m) gMatLib[n] = m; else gMatLib.erase(n);
}

// ============================================================================
//  SUS ELEMENTOS
// ============================================================================
void W3dLibsElementos(const std::string& lib, int tipo, std::vector<std::string>& out) {
    out.clear();
    if (!W3dLibsAsegurar(lib, NULL)) return;
    const LibMontada& L = gMontadas[lib];
    out = (tipo == W3D_LIB_ESCENA) ? L.escenas : L.prefabs;
}
bool W3dLibsTieneElemento(const std::string& lib, int tipo, const std::string& elem) {
    std::vector<std::string> v;
    W3dLibsElementos(lib, tipo, v);
    for (size_t i = 0; i < v.size(); i++) if (v[i] == elem) return true;
    return false;
}

std::string W3dLibsClave(const std::string& lib, int tipo, const std::string& elem) {
    if (lib.empty() || elem.empty()) return std::string();
    return "lib:" + lib + (tipo == W3D_LIB_ESCENA ? "/escena/" : "/prefab/") + elem;
}
bool W3dLibsDeClave(const std::string& c, std::string* lib, int* tipo, std::string* elem) {
    if (c.size() < 6 || c.compare(0, 4, "lib:") != 0) return false;
    const size_t b1 = c.find('/', 4);
    if (b1 == std::string::npos || b1 == 4) return false;
    const size_t b2 = c.find('/', b1 + 1);
    if (b2 == std::string::npos || b2 + 1 >= c.size()) return false;
    const std::string t = c.substr(b1 + 1, b2 - b1 - 1);
    if (t != "prefab" && t != "escena") return false;
    if (lib) *lib = c.substr(4, b1 - 4);
    if (tipo) *tipo = (t == "escena") ? W3D_LIB_ESCENA : W3D_LIB_PREFAB;
    if (elem) *elem = c.substr(b2 + 1);
    return true;
}

static JVal* Parsear(const std::string& ruta) {
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(ruta, d) || d.empty()) {
        w3dLogfE("[librerias] no pude leer '%s'", ruta.c_str());
        return NULL;
    }
    JParser p((const char*)&d[0], d.size());
    JVal* v = p.Valor();
    if (p.error || !v || v->tipo != 4) {
        w3dLogfE("[librerias] '%s' no parsea", ruta.c_str());
        delete v;
        return NULL;
    }
    return v;
}
// saca 'clave' de un objeto JSON sin borrarla (pasa a ser de quien la pide)
static JVal* Arrancar(JVal* o, const char* clave) {
    if (!o || o->tipo != 4) return NULL;
    std::map<std::string, JVal*>::iterator it = o->obj.find(clave);
    if (it == o->obj.end()) return NULL;
    JVal* v = it->second;
    o->obj.erase(it);
    return v;
}
static JVal* Texto(const std::string& t) { JVal* v = new JVal(); v->tipo = 2; v->str = t; return v; }

JVal* W3dLibsLeerElemento(const std::string& lib, int tipo, const std::string& elem, JVal** raizObj) {
    if (raizObj) *raizObj = NULL;
    std::string motivo;
    if (!W3dLibsAsegurar(lib, &motivo)) {
        w3dLogfW("[librerias] '%s/%s': %s", lib.c_str(), elem.c_str(), motivo.c_str());
        return NULL;
    }
    const LibMontada& L = gMontadas[lib];
    const std::vector<std::string>& ns = (tipo == W3D_LIB_ESCENA) ? L.escenas : L.prefabs;
    const std::vector<std::string>& es = (tipo == W3D_LIB_ESCENA) ? L.escenaEntradas : L.prefabEntradas;
    int k = -1;
    for (size_t i = 0; i < ns.size(); i++) if (ns[i] == elem) { k = (int)i; break; }
    if (k < 0) {
        w3dLogfW("[librerias] la libreria '%s' no tiene %s '%s'", lib.c_str(),
                 tipo == W3D_LIB_ESCENA ? "la escena" : "el prefab", elem.c_str());
        return NULL;
    }
    const std::string entrada = es[(size_t)k];
    if (tipo == W3D_LIB_PREFAB) {
        JVal* doc = Parsear("lib:" + lib + "/" + entrada);
        if (!doc) return NULL;
        JVal* r = JHijo(doc, "raiz", 4);
        if (!r) { w3dLogfW("[librerias] el prefab '%s/%s' no tiene objeto raiz", lib.c_str(), elem.c_str()); delete doc; return NULL; }
        if (raizObj) *raizObj = r;
        return doc;
    }
    // UNA ESCENA: sus objetos de primer nivel cuelgan de un VACIO con el nombre de la escena (lo que genera el
    // proxy es UN objeto raiz, como un prefab: los overrides por ruta y la frontera de scope andan igual)
    JVal* doc = Parsear("lib:" + lib + "/" + (entrada.empty() ? std::string("proyecto.json") : entrada));
    if (!doc) return NULL;
    JVal* objs = NULL;
    if (entrada.empty()) { JVal* esc = JHijo(doc, "escena", 4); objs = Arrancar(esc, "objetos"); }
    else objs = Arrancar(doc, "objetos");
    delete doc;
    JVal* nuevo = new JVal(); nuevo->tipo = 4;
    JVal* r = new JVal(); r->tipo = 4;
    r->obj["tipo"] = Texto("objeto");
    r->obj["nombre"] = Texto(elem);
    if (objs && objs->tipo == 5) r->obj["hijos"] = objs;
    else delete objs;
    nuevo->obj["raiz"] = r;
    if (raizObj) *raizObj = r;
    return nuevo;
}

// ============================================================================
//  EL CONTEXTO DE CARGA
// ============================================================================
void W3dLibContextoEntrar(const std::string& lib) { gContexto.push_back(lib); }
void W3dLibContextoSalir() { if (!gContexto.empty()) gContexto.pop_back(); }
const std::string& W3dLibContexto() {
    static const std::string vacio;
    return gContexto.empty() ? vacio : gContexto.back();
}

// ============================================================================
//  LAS REFERENCIAS DEL PROYECTO A UNA LIBRERIA QUE NO ESTA
// ============================================================================
void W3dLibRefAnotar(const Object* o, int tipo, const W3dLibRef& ref) {
    if (o && !ref.nombre.empty()) gRefsPend[std::make_pair(o->serial, tipo)] = ref;
}
const W3dLibRef* W3dLibRefDe(const Object* o, int tipo) {
    if (!o || gRefsPend.empty()) return NULL;
    std::map<std::pair<unsigned, int>, W3dLibRef>::const_iterator it = gRefsPend.find(std::make_pair(o->serial, tipo));
    return (it == gRefsPend.end()) ? NULL : &it->second;
}
void W3dLibRefOlvidar(const Object* o, int tipo) {
    if (o) gRefsPend.erase(std::make_pair(o->serial, tipo));
}
void W3dLibRefsSeriales(const std::string& lib, int tipo, std::vector<unsigned>& out) {
    out.clear();
    for (std::map<std::pair<unsigned, int>, W3dLibRef>::const_iterator it = gRefsPend.begin(); it != gRefsPend.end(); ++it)
        if (it->first.second == tipo && (lib.empty() || W3dLibPrefijoDe(it->second.nombre) == lib)) out.push_back(it->first.first);
}
std::string W3dLibPrefijoDe(const std::string& n) {
    const size_t barra = n.find('/');
    if (barra == std::string::npos || barra == 0 || barra + 1 >= n.size()) return std::string();
    return n.substr(0, barra);
}
