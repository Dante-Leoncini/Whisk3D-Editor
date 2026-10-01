// ============================================================================
//  BibliotecaExterna.cpp — las librerias externas vinculadas (ver el .h)
// ============================================================================
#include "io/BibliotecaExterna.h"
#include "io/Librerias.h"              // EL registro de las librerias (el mismo que usan los proxies y el juego)
#include "io/W3dZip.h"
#include "io/JsonW3d.h"
#include "io/UI2DFormato.h"            // g_w3dDirProyecto: la base de las rutas "ext:" relativas
#include "io/W3dContenedor.h"          // W3dContenedorEs, W3dRutaCosmeticaJson
#include "undo/Undo.h"
#include "config/W3dLang.h"
#include "W3dRaices.h"                 // W3dRaizIcono / W3dRaizTipoDeClave: el icono de una escena o un juego
#include "io/RaicesEditor.h"
#include "io/PrefabsEditor.h"        // W3dProxiesLibreriaCambio: lo que depende de una libreria se regenera
#include "io/MallasProyecto.h"         // W3dMallaNuevaCopia: desvincular deja a las mallas del proyecto con su copia
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"
#include "WhiskUI/draw/icons.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <map>
#include <set>
#include <cstdio>

// lo LEIDO de una libreria vinculada para explorarla (cache; el REGISTRO es el de io/Librerias.h). Paralelo al
// registro por indice; se vuelve a leer si la ruta de esa fila cambio (un undo, otra libreria en su lugar)
struct LibVinculada {
    std::string ruta;       // la de disco con la que se leyo
    bool leida, ok;
    std::string error;
    std::vector<W3dRecursoItem> items;
    std::vector<std::string> carpetas;
    LibVinculada() : leida(false), ok(false) {}
};
static std::vector<LibVinculada> gLibs;

int W3dLibreriasCantidad() { return W3dLibsCantidad(); }
std::string W3dLibreriaRuta(int i) { return W3dLibsRutaDisco(i); }
std::string W3dLibreriaNombre(int i) { return W3dLibsFila(i).nombre; }
void W3dLibreriasInvalidar() {
    for (size_t i = 0; i < gLibs.size(); i++) { gLibs[i].leida = false; gLibs[i].items.clear(); gLibs[i].carpetas.clear(); }
}
std::string W3dLibreriasFirma() {
    std::string f;
    for (int i = 0; i < W3dLibsCantidad(); i++) { f += W3dLibsFila(i).nombre; f += '='; f += W3dLibsRutaDisco(i); f += '\n'; }
    return f;
}

extern bool SimActiva();
// las librerias que CAMBIARON entre dos registros (entro, salio, u otro archivo con su nombre)
static std::set<std::string> Cambiadas(const std::vector<W3dLibFila>& a, const std::vector<W3dLibFila>& b) {
    std::map<std::string, std::string> ma, mb;
    for (size_t i = 0; i < a.size(); i++) ma[a[i].nombre] = a[i].rutaDisco;
    for (size_t i = 0; i < b.size(); i++) mb[b[i].nombre] = b[i].rutaDisco;
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
// agrupados con este (W3dProxiesLibreriaCambio); aca, al ir y volver, las OTRAS raices cargadas
struct UndoLibs { std::vector<W3dLibFila> otra; };
static void UndoLibsAplicar(void* d) {
    UndoLibs* u = (UndoLibs*)d;
    std::vector<W3dLibFila> ahora = W3dLibsFilas();
    W3dLibsFijar(u->otra);
    u->otra.swap(ahora);
    W3dLibreriasInvalidar();
    const std::set<std::string> c = Cambiadas(u->otra, W3dLibsFilas());
    for (std::set<std::string>::const_iterator it = c.begin(); it != c.end(); ++it) W3dProxiesLibreriaCambio(*it, false);
}
static void UndoLibsLiberar(void* d) { delete (UndoLibs*)d; }
extern int g_undoSinRaiz;   // (undo/Undo.cpp: el registro de librerias no es una edicion de la raiz activa)
static void PushUndoLibs(const std::vector<W3dLibFila>& antes) {
    UndoLibs* u = new UndoLibs();
    u->otra = antes;
    UndoExterno f; f.aplicar = UndoLibsAplicar; f.liberar = UndoLibsLiberar;
    g_undoSinRaiz++;
    UndoPushExterno(f, u);
    g_undoSinRaiz--;
}

// el nombre de una libreria: el de su archivo sin ".w3d" (unico entre las vinculadas: es el PREFIJO de sus recursos)
static std::string NombreDeArchivo(const std::string& ruta) {
    std::string r = ruta;
    const size_t b = r.find_last_of("/\\");
    if (b != std::string::npos) r = r.substr(b + 1);
    const size_t p = r.find_last_of('.');
    if (p != std::string::npos && p > 0) r = r.substr(0, p);
    return r;
}

bool W3dLibreriaVincular(const std::string& ruta, std::string* motivo) {
    if (ruta.empty()) { if (motivo) *motivo = "No file"; return false; }
    for (int i = 0; i < W3dLibsCantidad(); i++)
        if (W3dLibsRutaDisco(i) == ruta) { if (motivo) *motivo = "It is already linked"; return false; }
    if (!W3dContenedorEs(ruta)) { if (motivo) *motivo = "It isn't a .w3d project"; return false; }
    // (el proyecto abierto no es una libreria de si mismo: sus prefabs se instancian directo)
    { extern std::string w3dPath; if (!w3dPath.empty() && w3dPath == ruta) { if (motivo) *motivo = "It is the open project"; return false; } }
    const std::vector<W3dLibFila> antes = W3dLibsFilas();
    // (una que se desvinculo y vuelve con el MISMO archivo recupera su nombre: sus proxies la nombran asi)
    const std::string nombre = W3dLibsNombreLibre(NombreDeArchivo(ruta), ruta);
    if (W3dLibsAgregar(nombre, ruta) < 0) { if (motivo) *motivo = "It can't be linked"; return false; }
    // UN Ctrl+Z: el registro + lo que se regenera (los proxies que la nombraban y estaban vacios)
    UndoGrupoIniciar();
    PushUndoLibs(antes);
    W3dLibreriasInvalidar();
    if (!SimActiva()) W3dProxiesLibreriaCambio(nombre, true);
    UndoGrupoFinSecuencial();
    w3dLogf("[biblioteca] libreria vinculada: '%s' (%s)", nombre.c_str(), ruta.c_str());
    return true;
}
// las mallas del PROYECTO (sin entrar a lo que genera una instancia o un proxy) que usan una malla de 'lib'
static void MallasDeLib(Object* o, const std::string& lib, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h || W3dEsTipoInstancia(h->getType())) continue;
        if (h->getType() == ObjectType::mesh && ((Mesh*)h)->malla && ((Mesh*)h)->malla->libreria == lib) out.push_back((Mesh*)h);
        MallasDeLib(h, lib, out);
    }
}
int W3dLibreriaUsosProyecto(int i) {
    if (i < 0 || i >= W3dLibsCantidad() || !SceneCollection) return 0;
    std::vector<Mesh*> v;
    MallasDeLib(SceneCollection, W3dLibsFila(i).nombre, v);
    return (int)v.size();
}
bool W3dLibreriaDesvincular(int i, int* copias) {
    if (copias) *copias = 0;
    if (i < 0 || i >= W3dLibsCantidad()) return false;
    const std::vector<W3dLibFila> antes = W3dLibsFilas();
    const std::string nombre = W3dLibsFila(i).nombre;
    w3dLogf("[biblioteca] libreria desvinculada: '%s' (%s)", nombre.c_str(), W3dLibsRutaDisco(i).c_str());
    // UN Ctrl+Z: las copias de sus mallas + el registro + sus proxies, que se regeneran VACIOS (igual que al reabrir el
    // nivel sin ella; lo que ya se cargo de ella sigue vivo hasta cerrar el proyecto: el Ctrl+Z los vuelve a colgar)
    UndoGrupoIniciar();
    // LO QUE EL PROYECTO USABA DE ELLA no queda colgando: cada malla de la raiz activa que usa una malla suya (soltada
    // desde su vista) pasa a una COPIA PROPIA del proyecto, con sus datos de hoy (New Copy: sin el prefijo, se edita y
    // se guarda como cualquier otra). Va ANTES de sacarla del registro: la copia lee la malla de su almacen
    if (!SimActiva() && SceneCollection) {
        std::vector<Mesh*> usos;
        MallasDeLib(SceneCollection, nombre, usos);
        int n = 0;
        for (size_t k = 0; k < usos.size(); k++) if (W3dMallaNuevaCopia(usos[k])) n++;
        if (n) w3dLogf("[biblioteca] %d malla(s) del proyecto usaban mallas de '%s': ahora tienen su copia", n, nombre.c_str());
        if (copias) *copias = n;
    }
    W3dLibsQuitar(i);
    PushUndoLibs(antes);
    W3dLibreriasInvalidar();
    if (!SimActiva()) W3dProxiesLibreriaCambio(nombre, true);
    UndoGrupoFinSecuencial();
    return true;
}

// ---- LA LECTURA (sin montar: el proyecto abierto sigue montado) ----
static std::string Base(const std::string& r) {
    const size_t p = r.find_last_of('/');
    return (p == std::string::npos) ? r : r.substr(p + 1);
}
static void Item(std::vector<W3dRecursoItem>& out, int tipo, const std::string& id, const std::string& nombre,
                 const std::string& carpeta, const std::string& entrada, int icono) {
    W3dRecursoItem it;
    it.tipo = tipo; it.id = id; it.nombre = nombre; it.entrada = entrada;
    it.carpeta = W3dCarpetaNormalizar(carpeta);
    it.icono = icono >= 0 ? icono : W3dVistaIcono(tipo);
    it.usuarios = 0;
    it.soloLectura = true;
    it.renombrable = false;
    it.info = T("read-only library");
    out.push_back(it);
}
// un registro {"nombre", "entrada", "carpeta"} de proyecto.json
static void Registro(JVal* raiz, const char* clave, int tipo, std::vector<W3dRecursoItem>& out) {
    JVal* l = JHijo(raiz, clave, 5);
    if (!l) return;
    for (size_t i = 0; i < l->lista.size(); i++) {
        JVal* e = l->lista[i];
        if (!e || e->tipo != 4) continue;
        const std::string n = JS(e, "nombre", "");
        if (n.empty()) continue;
        int icono = -1;
        if (tipo == W3D_VISTA_ESCENAS) icono = W3dRaizIcono(W3dRaizTipoDeClave(JS(e, "tipo", "escena"), W3D_RAIZ_ESCENA));
        Item(out, tipo, n, n, JS(e, "carpeta", ""), JS(e, "entrada", ""), icono);
    }
}
static bool Leer(LibVinculada& l) {
    l.leida = true; l.ok = false; l.items.clear(); l.carpetas.clear(); l.error.clear();
    W3dZipLector z;
    if (!z.Abrir(l.ruta)) { l.error = "It can't be opened"; return false; }
    std::vector<unsigned char> js;
    if (!z.Leer("proyecto.json", js) || js.empty()) { l.error = "It has no proyecto.json"; return false; }
    JParser p((const char*)&js[0], js.size());
    JVal* raiz = p.Valor();
    if (!raiz || raiz->tipo != 4) { delete raiz; l.error = "Its proyecto.json can't be read"; return false; }
    // los registros
    Registro(raiz, "mallas", W3D_VISTA_MALLAS, l.items);
    Registro(raiz, "materiales", W3D_VISTA_MATERIALES, l.items);
    Registro(raiz, "animsets", W3D_VISTA_ANIMACIONES, l.items);
    Registro(raiz, "escenas3d", W3D_VISTA_ESCENAS, l.items);
    Registro(raiz, "prefabs", W3D_VISTA_PREFABS, l.items);
    if (!JHijo(raiz, "escenas3d", 5) && JHijo(raiz, "escena", 4))   // un proyecto de UNA escena (sin registro)
        Item(l.items, W3D_VISTA_ESCENAS, "Scene", "Scene", "", "", W3dRaizIcono(W3D_RAIZ_ESCENA));
    // las carpetas de los ARCHIVOS (texturas y los demas) y las entradas de sus carpetas del contenedor
    std::map<std::string, std::string> carpetaDe;
    const char* regs[2] = { "texturas", "archivos" };
    for (int k = 0; k < 2; k++) {
        JVal* l2 = JHijo(raiz, regs[k], 5);
        if (!l2) continue;
        for (size_t i = 0; i < l2->lista.size(); i++) {
            JVal* e = l2->lista[i];
            if (e && e->tipo == 4) carpetaDe[JS(e, "entrada", "")] = JS(e, "carpeta", "");
        }
    }
    std::vector<std::string> ents;
    z.Listar(ents);
    for (size_t i = 0; i < ents.size(); i++) {
        const std::string& e = ents[i];
        int tipo = -1;
        if (e.compare(0, 9, "texturas/") == 0) tipo = W3D_VISTA_TEXTURAS;
        else if (e.compare(0, 8, "sonidos/") == 0) tipo = W3D_VISTA_SONIDOS;
        else if (e.compare(0, 8, "scripts/") == 0) tipo = W3D_VISTA_SCRIPTS;
        else if (e.compare(0, 8, "fuentes/") == 0) tipo = W3D_VISTA_FUENTES;
        else if (e.compare(0, 7, "videos/") == 0) tipo = W3D_VISTA_VIDEOS;
        if (tipo < 0 || e[e.size() - 1] == '/') continue;
        std::map<std::string, std::string>::iterator c = carpetaDe.find(e);
        Item(l.items, tipo, e, Base(e), c != carpetaDe.end() ? c->second : std::string(), e, -1);
    }
    // el arbol de carpetas (el unico, o el viejo por tipo)
    std::set<std::string> cs;
    JVal* jc = JHijo(raiz, "carpetas", 5);
    if (jc) {
        for (size_t i = 0; i < jc->lista.size(); i++)
            if (jc->lista[i] && jc->lista[i]->tipo == 2) cs.insert(W3dCarpetaNormalizar(jc->lista[i]->str));
    } else if ((jc = JHijo(raiz, "carpetas", 4)) != NULL) {
        for (std::map<std::string, JVal*>::iterator it = jc->obj.begin(); it != jc->obj.end(); ++it)
            if (it->second && it->second->tipo == 5)
                for (size_t i = 0; i < it->second->lista.size(); i++)
                    if (it->second->lista[i] && it->second->lista[i]->tipo == 2) cs.insert(W3dCarpetaNormalizar(it->second->lista[i]->str));
    }
    cs.erase(std::string());
    l.carpetas.assign(cs.begin(), cs.end());
    delete raiz;
    l.ok = true;
    w3dLogf("[biblioteca] libreria %s: %d recursos, %d carpetas", l.ruta.c_str(), (int)l.items.size(), (int)l.carpetas.size());
    return true;
}

bool W3dLibreriaListar(int i, std::vector<W3dRecursoItem>& items, std::vector<std::string>& carpetas, std::string* error) {
    items.clear(); carpetas.clear();
    if (i < 0 || i >= W3dLibsCantidad()) { if (error) *error = "It isn't linked"; return false; }
    if (gLibs.size() < (size_t)W3dLibsCantidad()) gLibs.resize((size_t)W3dLibsCantidad());
    LibVinculada& l = gLibs[(size_t)i];
    const std::string ruta = W3dLibsRutaDisco(i);
    if (l.ruta != ruta) { l.ruta = ruta; l.leida = false; }
    if (!l.leida) Leer(l);
    if (!l.ok) { if (error) *error = l.error; return false; }
    items = l.items;
    carpetas = l.carpetas;
    return true;
}

// ---- proyecto.json (el registro lo escribe y lo lee io/Librerias.h: aca solo se delega) ----
void W3dLibreriasGuardarJson(std::string& s) { W3dLibsGuardarJson(s); }
void W3dLibreriasCerrarProyecto() {
    gLibs.clear();
    W3dLibsCerrarProyecto();
}

// ---- ARRASTRAR su contenido (el proxy y las referencias a sus recursos) ----
std::string W3dLibreriaIdGlobal(int i, int tipo, const std::string& id) {
    if (i < 0 || i >= W3dLibsCantidad() || id.empty()) return std::string();
    const std::string& lib = W3dLibsFila(i).nombre;
    switch (tipo) {
        case W3D_VISTA_MALLAS: case W3D_VISTA_MATERIALES: case W3D_VISTA_ANIMACIONES:
            // (su malla, material o animset con el prefijo de la libreria: registrado al asegurarla)
            if (id.find('/') != std::string::npos && tipo == W3D_VISTA_ANIMACIONES) return std::string();   // (un clip suelto)
            return W3dLibsAsegurar(lib, NULL) ? lib + "/" + id : std::string();
        case W3D_VISTA_TEXTURAS:   return W3dLibsAsegurar(lib, NULL) ? "lib:" + lib + "/" + id : std::string();
        case W3D_VISTA_PREFABS:    return W3dLibsClave(lib, W3D_LIB_PREFAB, id);
        case W3D_VISTA_ESCENAS:    return W3dLibsClave(lib, W3D_LIB_ESCENA, id);
    }
    return std::string();
}
