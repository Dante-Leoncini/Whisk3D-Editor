// ============================================================================
//  PropsPrefab.cpp — la tarjeta "Prefab Instance" del panel de propiedades
//  (objects/InstanciaPrefab.h). Vive aparte de Properties.cpp: el panel solo la
//  ARMA en su lugar (PropsPrefabConstruir) y le pide el bindeo por frame
//  (PropsPrefabActualizar).
//    - en una INSTANCIA (pestania 2): su prefab (desplegable con los del proyecto;
//      acepta un prefab arrastrado de la biblioteca), cuantos overrides tiene, "Edit
//      Prefab" (abre la raiz del prefab), "Unpack" y "Reset Overrides";
//    - en un objeto GENERADO por una instancia (pestania 1, arriba del transform): de
//      que instancia es y "Select Instance" (lo generado no se edita: se edita el
//      prefab, o se desempaqueta la instancia).
//    - en un PROXY W3D (objects/ProxyW3d.h, pestania 2; la tarjeta se llama "Proxy
//      W3D"): su LIBRERIA (desplegable con las vinculadas + "Add Library..." con el
//      explorador de archivos), su ELEMENTO (desplegable con los prefabs y las escenas
//      de esa libreria; acepta uno arrastrado de la libreria en el outliner), cuantos
//      overrides tiene y "Reset Overrides". No se edita ni se desempaqueta: es de solo
//      lectura (se edita abriendo la libreria).
//    - en las dos, el STREAMING (io/Streaming.h): COMO se carga (desplegable "Always" / "By
//      Distance"), a que DISTANCIA y contra que OBJETIVO (un objeto por nombre; vacio = la camara
//      activa), el estado de su carga en la etiqueta, y la VISTA PREVIA del streaming en el editor
//      (una opcion del PROYECTO: se guarda en proyecto.json).
//  Todo con Ctrl+Z salvo abrir el prefab (cambiar de raiz limpia el historial) y la vista previa.
// ============================================================================
#include "ViewPorts/Properties.h"
#include "objects/InstanciaPrefab.h"
#include "objects/ProxyW3d.h"
#include "io/Librerias.h"
#include "io/BibliotecaExterna.h"   // "Add Library...": vincular otro .w3d (con undo)
#include "ViewPorts/PopUp/FileBrowser.h"
#include "io/Prefabs.h"
#include "io/PrefabsEditor.h"
#include "io/RaicesEditor.h"
#include "W3dRaices.h"
#include "undo/Undo.h"
#include "ViewPorts/Notificaciones.h"
#include "render/OpcionesRender.h"   // g_redraw
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"
#include "W3dLang.h"                 // T(): los textos salen en el idioma del sistema
#include "io/Streaming.h"            // como se carga la instancia (carga / distancia / objetivo / vista previa)
#include "io/CambiosProyecto.h"      // la vista previa es del proyecto: lo deja con cambios
#include <cstdio>

extern PopupMenu* MenuAbierto;
void PropsLayoutSucio();   // (Properties.cpp: re-medir el panel el proximo cuadro)

static InstanciaPrefab* PfActiva() {
    return (ObjActivo && W3dEsTipoInstancia(ObjActivo->getType())) ? (InstanciaPrefab*)ObjActivo : NULL;
}
static ProxyW3d* PxActivo() {
    return (ObjActivo && ObjActivo->getType() == ObjectType::proxy) ? (ProxyW3d*)ObjActivo : NULL;
}

// ---- el desplegable: los prefabs del proyecto ----
static PopupMenu* gMenuPf = NULL;
static std::vector<std::string> gMenuPfNombres;   // id - 1 -> nombre (se rearma en cada apertura)
static void AccionPfElegido(int id) {
    InstanciaPrefab* ip = PfActiva();
    if (!ip || id < 1 || id > (int)gMenuPfNombres.size()) return;
    std::string motivo;
    if (!W3dPrefabCambiar(ip, gMenuPfNombres[(size_t)(id - 1)], &motivo) && !motivo.empty())
        Notificar(std::string(T(motivo.c_str())), true);
    PropsLayoutSucio();
    g_redraw = true;
}
// ---- PROXY: los desplegables de su LIBRERIA y de su ELEMENTO ----
static PopupMenu* gMenuPxLib = NULL;
static std::vector<std::string> gMenuPxElem;   // id - 1 -> clave del elemento (W3dLibsClave)
static void ProxyAPrimerElemento(const std::string& lib) {
    ProxyW3d* px = PxActivo();
    if (!px) return;
    std::vector<std::string> ps;
    int tipo = W3D_LIB_PREFAB;
    W3dLibsElementos(lib, W3D_LIB_PREFAB, ps);
    if (ps.empty()) { tipo = W3D_LIB_ESCENA; W3dLibsElementos(lib, W3D_LIB_ESCENA, ps); }
    std::string motivo;
    if (!W3dProxyCambiar(px, lib, tipo, ps.empty() ? std::string() : ps[0], &motivo) && !motivo.empty())
        Notificar(std::string(T(motivo.c_str())), true);
    PropsLayoutSucio();
    g_redraw = true;
}
// "Add Library...": el .w3d elegido se VINCULA (con undo) y el proxy pasa a generar su primer elemento
static void PxLibreriaElegida(const std::string& ruta) {
    std::string motivo;
    if (!W3dLibreriaVincular(ruta, &motivo)) {
        // (una ya vinculada: se usa esa)
        for (int i = 0; i < W3dLibsCantidad(); i++)
            if (W3dLibsRutaDisco(i) == ruta) { ProxyAPrimerElemento(W3dLibsFila(i).nombre); return; }
        Notificar(std::string(T("Not linked")) + ": " + T(motivo.c_str()), true);
        return;
    }
    if (W3dLibsCantidad() > 0) ProxyAPrimerElemento(W3dLibsFila(W3dLibsCantidad() - 1).nombre);
}
static void AccionPxLibElegida(int id) {
    ProxyW3d* px = PxActivo();
    if (!px) return;
    if (id == 1000) { AbrirFileBrowser(T("Link .w3d..."), T("Link"), ".w3d", PxLibreriaElegida, false); return; }
    if (id < 1 || id > W3dLibsCantidad()) return;
    const std::string lib = W3dLibsFila(id - 1).nombre;
    if (lib != px->Libreria()) ProxyAPrimerElemento(lib);
}
static void AccionPxLibMenu() {
    ProxyW3d* px = PxActivo();
    if (!px || !PropsActivo || !PropsActivo->propPxLib) return;
    if (!gMenuPxLib) { gMenuPxLib = new PopupMenu(); gMenuPxLib->action = AccionPxLibElegida; }
    gMenuPxLib->Limpiar();
    for (int i = 0; i < W3dLibsCantidad(); i++) {
        const bool actual = (W3dLibsFila(i).nombre == px->Libreria());
        MenuItem* it = gMenuPxLib->Agregar(W3dLibsFila(i).nombre, i + 1, actual ? (int)IconType::notifOk : (int)IconType::libreria);
        if (it) it->verde = actual;
    }
    gMenuPxLib->Agregar(T("Add Library..."), 1000, (int)IconType::libreria);
    Button* b = PropsActivo->propPxLib->button;
    gMenuPxLib->Resize();
    gMenuPxLib->Abrir(b->sx + b->width - gMenuPxLib->width, b->sy + b->height - GlobalScale, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuPxLib;
}
// el desplegable del ELEMENTO de un proxy (usa el de la instancia: propPfSel)
static void AccionPxElemElegido(int id) {
    ProxyW3d* px = PxActivo();
    if (!px || id < 1 || id > (int)gMenuPxElem.size()) return;
    std::string lib, elem; int tipo = W3D_LIB_PREFAB;
    if (!W3dLibsDeClave(gMenuPxElem[(size_t)(id - 1)], &lib, &tipo, &elem)) return;
    std::string motivo;
    if (!W3dProxyCambiar(px, lib, tipo, elem, &motivo) && !motivo.empty()) Notificar(std::string(T(motivo.c_str())), true);
    PropsLayoutSucio();
    g_redraw = true;
}
static PopupMenu* gMenuPxElemPop = NULL;
static void AccionPxElemMenu(ProxyW3d* px) {
    if (!gMenuPxElemPop) { gMenuPxElemPop = new PopupMenu(); gMenuPxElemPop->action = AccionPxElemElegido; }
    gMenuPxElemPop->Limpiar();
    gMenuPxElem.clear();
    const std::string lib = px->Libreria();
    for (int t = 0; t < 2 && !lib.empty(); t++) {
        const int tipo = (t == 0) ? W3D_LIB_PREFAB : W3D_LIB_ESCENA;
        std::vector<std::string> es;
        W3dLibsElementos(lib, tipo, es);
        for (size_t i = 0; i < es.size(); i++) {
            const std::string clave = W3dLibsClave(lib, tipo, es[i]);
            gMenuPxElem.push_back(clave);
            const bool actual = (clave == px->prefab);
            MenuItem* it = gMenuPxElemPop->Agregar(es[i], (int)gMenuPxElem.size(),
                                                   actual ? (int)IconType::notifOk : (tipo == W3D_LIB_PREFAB ? (int)IconType::prefab : (int)IconType::camera));
            if (it) it->verde = actual;
        }
    }
    if (gMenuPxElem.empty()) gMenuPxElemPop->Agregar(T("No prefabs yet"), 0, -1);
    Button* b = PropsActivo->propPfSel->button;
    gMenuPxElemPop->Resize();
    gMenuPxElemPop->Abrir(b->sx + b->width - gMenuPxElemPop->width, b->sy + b->height - GlobalScale, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuPxElemPop;
}

static void AccionPfMenu() {
    InstanciaPrefab* ip = PfActiva();
    if (!ip || !PropsActivo || !PropsActivo->propPfSel) return;
    if (PxActivo()) { AccionPxElemMenu(PxActivo()); return; }
    if (!gMenuPf) { gMenuPf = new PopupMenu(); gMenuPf->action = AccionPfElegido; }
    gMenuPf->Limpiar();
    gMenuPfNombres.clear();
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    for (size_t i = 0; i < fs.size(); i++) {
        if (fs[i].tipo != W3D_RAIZ_PREFAB) continue;
        gMenuPfNombres.push_back(fs[i].nombre);
        const bool actual = (fs[i].nombre == ip->prefab);
        MenuItem* it = gMenuPf->Agregar(fs[i].nombre, (int)gMenuPfNombres.size(), actual ? (int)IconType::notifOk : (int)IconType::prefab);
        if (it) it->verde = actual;
    }
    Button* b = PropsActivo->propPfSel->button;
    gMenuPf->Resize();
    gMenuPf->Abrir(b->sx + b->width - gMenuPf->width, b->sy + b->height - GlobalScale, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuPf;
}
static void AccionPfEditar() {
    InstanciaPrefab* ip = PfActiva();
    if (!ip) return;
    const int idx = W3dRaizBuscar(W3D_RAIZ_PREFAB, ip->prefab);
    std::string motivo;
    if (idx < 0) { Notificar(std::string(T("There is no such prefab")), true); return; }
    if (!W3dActivarRaiz(idx, &motivo)) Notificar(std::string(T(motivo.c_str())), true);
}
static void AccionPfUnpack() {
    InstanciaPrefab* ip = PfActiva();
    std::string motivo;
    if (ip && !W3dPrefabDesempaquetar(ip, &motivo) && !motivo.empty()) Notificar(std::string(T(motivo.c_str())), true);
    PropsLayoutSucio();
    g_redraw = true;
}
// Reset Overrides con Ctrl+Z (io/PrefabsEditor.h: regenera sin liberar lo generado, el paso se queda con la
// generacion que sale; no con el Play andando)
static void AccionPfReset() {
    InstanciaPrefab* ip = PfActiva();
    if (!ip) return;
    std::string motivo;
    if (!W3dPrefabResetOverrides(ip, &motivo) && !motivo.empty()) Notificar(std::string(T(motivo.c_str())), true);
    PropsLayoutSucio();
    g_redraw = true;
}
static void AccionPfInstancia() {
    Object* inst = ObjActivo ? W3dInstanciaDe(ObjActivo) : NULL;
    // (la de AFUERA de todo: la que el usuario puso en la escena)
    while (inst && W3dInstanciaDe(inst)) inst = W3dInstanciaDe(inst);
    if (!inst) return;
    DeseleccionarTodo();
    inst->Seleccionar();
    PropsLayoutSucio();
    g_redraw = true;
}

// ---- el STREAMING de la instancia: COMO se carga ----
static PopupMenu* gMenuCarga = NULL;
static void AccionCargaElegida(int id) {
    InstanciaPrefab* ip = PfActiva();
    if (!ip || id < 1 || id > 2) return;
    const int c = (id == 2) ? W3D_CARGA_DISTANCIA : W3D_CARGA_SIEMPRE;
    if (c != ip->carga) W3dInstanciaCargaCambiar(ip, c, ip->distancia, ip->objetivo);
    PropsLayoutSucio();
    g_redraw = true;
}
static void AccionCargaMenu() {
    InstanciaPrefab* ip = PfActiva();
    if (!ip || !PropsActivo || !PropsActivo->propPfCarga) return;
    if (!gMenuCarga) { gMenuCarga = new PopupMenu(); gMenuCarga->action = AccionCargaElegida; }
    gMenuCarga->Limpiar();
    MenuItem* a = gMenuCarga->Agregar(T("Always"), 1, ip->carga == W3D_CARGA_SIEMPRE ? (int)IconType::notifOk : -1);
    if (a) a->verde = (ip->carga == W3D_CARGA_SIEMPRE);
    MenuItem* d = gMenuCarga->Agregar(T("By Distance"), 2, ip->carga == W3D_CARGA_DISTANCIA ? (int)IconType::notifOk : -1);
    if (d) d->verde = (ip->carga == W3D_CARGA_DISTANCIA);
    Button* b = PropsActivo->propPfCarga->button;
    gMenuCarga->Resize();
    gMenuCarga->Abrir(b->sx + b->width - gMenuCarga->width, b->sy + b->height - GlobalScale, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuCarga;
}
// la VISTA PREVIA (opcion del proyecto): al apagarla, el editor vuelve a ver el nivel entero
static void AccionVistaPrevia() {
    W3dStreamingVistaPreviaFijar(g_w3dStreamingVistaPrevia);
    W3dCambiosTocado();   // (es del proyecto: el '*' del titulo; se guarda en proyecto.json)
    g_redraw = true;
}

void PropsPrefabConstruir(Properties* p) {
    if (!p) return;
    p->propPrefab = new GroupPropertie(T("Prefab Instance"));
    p->propPrefab->icono = (int)IconType::prefab;
    // (un PROXY: su libreria, arriba del elemento)
    p->propPxLib = new PropButton("", IconType::libreria);
    p->propPxLib->button->desplegable = true;
    p->propPxLib->action = AccionPxLibMenu;
    p->propPxLib->oculto = true;
    p->propPrefab->properties.push_back(p->propPxLib);
    p->propPfSel = new PropButton("", IconType::prefab);
    p->propPfSel->button->desplegable = true;
    p->propPfSel->action = AccionPfMenu;
    p->propPrefab->properties.push_back(p->propPfSel);
    p->propPfInfo = new PropLabel("");
    p->propPrefab->properties.push_back(p->propPfInfo);
    // el STREAMING: como se carga, a que distancia, contra que objetivo; y la vista previa del proyecto
    p->propPfCarga = new PropButton(T("Loading mode"));
    p->propPfCarga->conLabel = true;   // label a la izquierda, el desplegable a la derecha
    p->propPfCarga->button->desplegable = true;
    p->propPfCarga->action = AccionCargaMenu;
    p->propPfCarga->oculto = true;
    p->propPrefab->properties.push_back(p->propPfCarga);
    p->propPfDistancia = new PropFloat(T("Distance"), "m");
    p->propPfDistancia->SetRango(0.0f, 1.0e7f);
    p->propPfDistancia->stepFino = 1.0f; p->propPfDistancia->stepGrueso = 10.0f;
    p->propPrefab->properties.push_back(p->propPfDistancia);
    p->propPfObjetivo = new PropText(T("Target"), "");
    p->propPfObjetivo->oculto = true;
    p->propPrefab->properties.push_back(p->propPfObjetivo);
    p->propPfVista = new PropBool(T("Streaming preview"));
    p->propPfVista->onChange = AccionVistaPrevia;
    p->propPrefab->properties.push_back(p->propPfVista);
    p->propPfEditar = new PropButton(T("Edit Prefab"), IconType::prefab);
    p->propPfEditar->action = AccionPfEditar;
    p->propPrefab->properties.push_back(p->propPfEditar);
    p->propPfUnpack = new PropButton(T("Unpack"), IconType::carpeta);
    p->propPfUnpack->action = AccionPfUnpack;
    p->propPrefab->properties.push_back(p->propPfUnpack);
    p->propPfReset = new PropButton(T("Reset Overrides"), IconType::borrar);
    p->propPfReset->action = AccionPfReset;
    p->propPrefab->properties.push_back(p->propPfReset);
    p->propPfInstancia = new PropButton(T("Select Instance"), IconType::prefab);
    p->propPfInstancia->action = AccionPfInstancia;
    p->propPrefab->properties.push_back(p->propPfInstancia);
    p->GroupProperties.push_back(p->propPrefab);
}

// ---- el STREAMING de la tarjeta, con Ctrl+Z: los valores se editan EN EL LUGAR (el numero se arrastra, el objetivo
// se tipea) y el paso se anota cuando la edicion TERMINA (mouse suelto, sin tipear): uno por arrastre, no uno por
// pixel. 'gVisto' es como estaba la instancia la ultima vez que se anoto
namespace {
struct CargaVista { InstanciaPrefab* ip; unsigned serial; int carga; float distancia; std::string objetivo;
                    CargaVista() : ip(NULL), serial(0), carga(0), distancia(0.0f) {} };
CargaVista gVisto;
std::string gObjetivoTipeado;
}
static void VerCarga(InstanciaPrefab* ip) {
    gVisto.ip = ip; gVisto.serial = ip ? ip->serial : 0;
    if (ip) { gVisto.carga = ip->carga; gVisto.distancia = ip->distancia; gVisto.objetivo = ip->objetivo; }
}
static bool CargaDifiere(const InstanciaPrefab* ip) {
    return ip->carga != gVisto.carga || ip->distancia != gVisto.distancia || ip->objetivo != gVisto.objetivo;
}
static void SincronizarCarga(Properties* p, InstanciaPrefab* ip) {
    extern bool leftMouseDown;
    // un paso anotado por otro lado o un Ctrl+Z / Ctrl+Y de uno: lo de ahora es lo visto (no es una edicion nueva)
    static unsigned version = 0;
    if (version != g_w3dCargaVersion) {
        version = g_w3dCargaVersion;
        if (gVisto.ip == ip) VerCarga(ip);
    }
    // la instancia que se miraba cambio (otro objeto activo; o se libero y otra nacio en su direccion): lo que quedo
    // editado en la anterior se anota igual
    if (gVisto.ip != ip || (ip && gVisto.serial != ip->serial)) {
        if (gVisto.ip && W3dObjetoVivoSerial(gVisto.ip, gVisto.serial) && CargaDifiere(gVisto.ip))
            W3dInstanciaCargaUndo(gVisto.ip, gVisto.carga, gVisto.distancia, gVisto.objetivo);
        VerCarga(ip);
        gObjetivoTipeado.clear();
    }
    if (!ip) return;
    // el OBJETIVO: lo tipeado se escribe en vivo; sin foco muestra el valor real (un Ctrl+Z, otro objeto)
    const bool tipeando = p->propPfObjetivo && TextFieldEnVivo(&p->propPfObjetivo->field);
    if (p->propPfObjetivo) {
        if (tipeando && p->propPfObjetivo->field.text != gObjetivoTipeado) {
            ip->objetivo = p->propPfObjetivo->field.text;
            gObjetivoTipeado = ip->objetivo;
        }
        if (!tipeando) {
            gObjetivoTipeado.clear();
            if (p->propPfObjetivo->field.text != ip->objetivo) { p->propPfObjetivo->field.SetText(ip->objetivo); g_redraw = true; }
        }
    }
    const bool editando = leftMouseDown || tipeando || (g_propFloatEditando && g_propFloatEditando == p->propPfDistancia);
    if (!editando && CargaDifiere(ip)) {
        W3dInstanciaCargaUndo(ip, gVisto.carga, gVisto.distancia, gVisto.objetivo);
        VerCarga(ip);
    }
}

// por frame (compara antes de asignar: sin redibujado infinito)
void PropsPrefabActualizar(Properties* p, int pestania) {
    if (!p || !p->propPrefab) return;
    InstanciaPrefab* ip = (pestania == 2) ? PfActiva() : NULL;
    Object* gen = (pestania == 1 && ObjActivo && W3dEsGenerado(ObjActivo)) ? ObjActivo : NULL;
    const bool vis = (ip != NULL) || (gen != NULL);
    if (p->propPrefab->visible != vis) { p->propPrefab->visible = vis; PropsLayoutSucio(); }
    SincronizarCarga(p, ip);   // (tambien sin tarjeta: lo que quedo editado en la instancia anterior se anota)
    if (!vis) {
        if (p->propPfDistancia) p->propPfDistancia->value = NULL;
        if (p->propPfVista) p->propPfVista->value = NULL;
        return;
    }
    bool layout = false;
    const bool esInst = (ip != NULL);
    ProxyW3d* px = (ip && ip->getType() == ObjectType::proxy) ? (ProxyW3d*)ip : NULL;
    const bool esProxy = (px != NULL);
    // la tarjeta de un PROXY se llama distinto y no tiene Edit Prefab ni Unpack (es de solo lectura)
    {
        const std::string titulo = esProxy ? std::string(T("Proxy W3D")) : std::string(T("Prefab Instance"));
        const int icono = esProxy ? (int)IconType::libreria : (int)IconType::prefab;
        if (p->propPrefab->name != titulo || p->propPrefab->icono != icono) {
            p->propPrefab->name = titulo; p->propPrefab->icono = icono; layout = true;
        }
    }
    if (p->propPxLib && p->propPxLib->oculto == esProxy) { p->propPxLib->oculto = !esProxy; layout = true; }
    if (p->propPfSel->oculto == esInst) { p->propPfSel->oculto = !esInst; layout = true; }
    if (p->propPfEditar->oculto == (esInst && !esProxy)) { p->propPfEditar->oculto = !(esInst && !esProxy); layout = true; }
    if (p->propPfUnpack->oculto == (esInst && !esProxy)) { p->propPfUnpack->oculto = !(esInst && !esProxy); layout = true; }
    if (p->propPfInstancia->oculto != esInst) { p->propPfInstancia->oculto = esInst; layout = true; }
    // el STREAMING (solo en una instancia o un proxy): la distancia y el objetivo solo si es por distancia
    {
        const bool dist = esInst && ip->carga == W3D_CARGA_DISTANCIA;
        if (p->propPfCarga && p->propPfCarga->oculto == esInst) { p->propPfCarga->oculto = !esInst; layout = true; }
        if (p->propPfObjetivo && p->propPfObjetivo->oculto == dist) { p->propPfObjetivo->oculto = !dist; layout = true; }
        float* dv = dist ? &ip->distancia : NULL;
        if (p->propPfDistancia && p->propPfDistancia->value != dv) { p->propPfDistancia->value = dv; layout = true; }
        bool* vv = esInst ? &g_w3dStreamingVistaPrevia : NULL;
        if (p->propPfVista && p->propPfVista->value != vv) { p->propPfVista->value = vv; layout = true; }
        if (esInst && p->propPfCarga) {
            const std::string t = (ip->carga == W3D_CARGA_DISTANCIA) ? std::string(T("By Distance")) : std::string(T("Always"));
            if (p->propPfCarga->button->text != t) { p->propPfCarga->button->text = t; g_redraw = true; }
        }
    }
    std::string info;
    if (ip) {
        W3dPrefabSincronizarOverrides(ip);   // (lo que el usuario cambio en lo generado desde el ultimo cuadro)
        std::string sel = ip->prefab.empty() ? std::string(T("(none)")) : ip->prefab;
        if (px) {
            // el proxy: su libreria (con "(!)" si no esta vinculada) y su elemento
            std::string lib = px->Libreria();
            if (lib.empty()) lib = T("(none)");
            else if (W3dLibsBuscar(lib) < 0) lib += " (!)";
            if (p->propPxLib->button->text != lib) { p->propPxLib->button->text = lib; g_redraw = true; }
            sel = px->Elemento().empty() ? std::string(T("(none)")) : px->Elemento();
            if (ip->noGenerada && !px->Elemento().empty()) sel += " (!)";
        } else if (ip->noGenerada && !ip->prefab.empty()) sel += " (!)";
        if (p->propPfSel->button->text != sel) { p->propPfSel->button->text = sel; g_redraw = true; }
        const int n = (int)(ip->overProps.size() + ip->overVisible.size());
        char b[160];
        // (una diferida dice ademas en que esta su carga)
        if (ip->carga == W3D_CARGA_DISTANCIA || ip->streamEstado != W3D_STREAM_CARGADA)
            snprintf(b, sizeof(b), "%s: %d. %s", T("Overrides"), n,
                     ip->streamEstado == W3D_STREAM_CARGADA ? T("Loaded") :
                     ip->streamEstado == W3D_STREAM_PIDIENDO ? T("Loading...") : T("Unloaded"));
        else snprintf(b, sizeof(b), "%s: %d", T("Overrides"), n);
        info = b;
        const bool sinOv = (n == 0);
        if (p->propPfReset->oculto != sinOv) { p->propPfReset->oculto = sinOv; layout = true; }
    } else {
        Object* inst = W3dInstanciaDe(gen);
        // (lo generado no se edita: lo unico propio que se guarda es su visible y los valores de sus scripts)
        info = std::string(T("Generated by")) + " " + (inst ? inst->name : std::string("?")) + ". " +
               T("Only its visibility and script values are saved");
        if (!p->propPfReset->oculto) { p->propPfReset->oculto = true; layout = true; }
    }
    if (p->propPfInfo->name != info) { p->propPfInfo->name = info; g_redraw = true; }
    if (layout) PropsLayoutSucio();
}
