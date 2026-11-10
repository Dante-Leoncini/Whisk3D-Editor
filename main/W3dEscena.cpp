// ============================================================================
//  W3dEscena.cpp — ver W3dEscena.h. El motor multi-escena COMPARTIDO por el
//  editor (Play) y el runtime compilado. C++03 puro. Comentarios sin acentos.
// ============================================================================
#include "W3dEscena.h"
#include "W3dPaletas.h"
#include "W3dNombres.h"   // LA regla de nombres unicos (compartida por todo el editor)
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Texto2D.h"    // los tipos con campos pal* (fix de referencias
#include "objects/Imagen2D.h"   // al borrar un color de las paletas)
#include "objects/Rect2D.h"
#include "objects/Slice9.h"
#include "objects/Boton2D.h"

extern "C" {
#include "lua.h"
}

#include <map>
#include <set>
#include "script/W3dScript.h"   // las capas: scripts, objetos nuevos y destruidos; los mensajes (W3dValorLua)
#include "w3dlog.h"

// ---------------------------------------------------------------------------
//  ESTADO del sistema (una sola simulacion/juego a la vez).
// ---------------------------------------------------------------------------
static std::map<std::string, UI*> gEscenas;   // nombre -> raiz UI (todas las escenas)
static UI*  gEscActiva = 0;                    // la escena que corre y se ve
static std::map<UI*, bool> gInited;            // escenas que YA corrieron inicio() (init perezoso)
static std::string gInicial;                   // nombre de la escena que arranca (ajuste del proyecto)
static bool gModoMulti = false;                // modo multi-escena declarado (informativo)
static W3dEscenaInitFn gInitFn = 0;            // el init de una escena (lo pone cada build)
// cambio de escena PENDIENTE (se aplica al final del frame)
static bool gHayCambio = false;
static bool gReiniciarPend = false;
static std::string gCambioNombre;

void W3dEscenaSetInit(W3dEscenaInitFn fn) { gInitFn = fn; }

// las ESCENAS 3D (ver W3dEscena.h): los pone main/W3dRaices.cpp, que solo existe con 3D
static bool (*gPedir3D)(const std::string&, bool) = 0;
static bool (*gAplicar3D)() = 0;
void W3dEscenaSet3D(bool (*pedir)(const std::string&, bool), bool (*aplicar)()) {
    gPedir3D = pedir; gAplicar3D = aplicar;
}

static bool EsCapa(Object* o);   // (abajo: las capas)
void W3dEscenaRegistrarTodas() {
    gEscenas.clear();
    // (una escena 2D cargada como capa ya corrio su inicio(): sigue iniciada)
    { std::map<UI*, bool> viejo; viejo.swap(gInited);
      for (std::map<UI*, bool>::iterator it = viejo.begin(); it != viejo.end(); ++it) if (EsCapa((Object*)it->first)) gInited[it->first] = true; }
    gEscActiva = 0;
    gHayCambio = false; gReiniciarPend = false; gCambioNombre.clear();
    if (!SceneCollection) return;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::ui) gEscenas[o->name] = (UI*)o;
    }
}

bool W3dEscenaHayEscenas() { return !gEscenas.empty(); }
UI*  W3dEscenaActiva()     { return gEscActiva; }

UI* W3dEscenaBuscar(const std::string& nombre) {
    std::map<std::string, UI*>::iterator it = gEscenas.find(nombre);
    return (it != gEscenas.end()) ? it->second : 0;
}

void W3dEscenaSetInicial(const std::string& nombre) { gInicial = nombre; }
const std::string& W3dEscenaInicial() { return gInicial; }
void W3dEscenaSetModo(bool multi) { gModoMulti = multi; }
bool W3dEscenaModo() { return gModoMulti; }

// muestra SOLO 'nueva' entre las escenas registradas (el resto se oculta: dejan de
// correr y de recibir input; el overlay no las dibuja por visible=false).
static void MostrarSolo(UI* nueva) {
    for (std::map<std::string, UI*>::iterator it = gEscenas.begin(); it != gEscenas.end(); ++it) {
        Object* u = it->second;
        if (u && !EsCapa(u)) u->visible = (u == (Object*)nueva);   // (una capa sigue como esta: encima)
    }
}

void W3dEscenaArrancar() {
    if (gEscenas.empty()) return;
    UI* ini = W3dEscenaBuscar(gInicial);
    if (ini && EsCapa((Object*)ini)) ini = 0;   // (una escena cargada encima ya corre: no es la que arranca)
    // fallback: la primera escena registrada que no sea una capa (sin ninguna: no hay escena 2D activa)
    for (std::map<std::string, UI*>::iterator it = gEscenas.begin(); it != gEscenas.end() && !ini; ++it)
        if (it->second && !EsCapa((Object*)it->second)) ini = it->second;
    if (!ini) return;
    MostrarSolo(ini);
    gEscActiva = ini;
    if (gInitFn) gInitFn(ini, false);            // init de la escena inicial (el resto, perezoso)
    gInited[ini] = true;
}

void W3dEscenaPedirCambio(const std::string& nombre, bool reiniciar) {
    // no es una escena UI: puede ser una ESCENA 3D del proyecto (se descarga la actual y se
    // carga esa, al final del frame). Un nombre inexistente se ignora: no romper el juego por
    // un typo del script.
    if (!W3dEscenaBuscar(nombre)) { if (gPedir3D) gPedir3D(nombre, reiniciar); return; }
    gHayCambio = true; gCambioNombre = nombre; gReiniciarPend = reiniciar;
}

static void CapasEntregarMensajes();
static void CapasAplicar();
void W3dEscenaAplicarPendiente() {
    // primero los MENSAJES (un "guardar" llega antes de que la escena se vaya) y las CAPAS
    CapasEntregarMensajes();
    CapasAplicar();
    // una ESCENA 3D pedida cambia el arbol ENTERO (sus escenas UI incluidas): un cambio de UI
    // pendiente era de la escena que se fue
    if (gAplicar3D && gAplicar3D()) { gHayCambio = false; gReiniciarPend = false; return; }
    if (!gHayCambio) return;
    gHayCambio = false;
    UI* nueva = W3dEscenaBuscar(gCambioNombre);
    if (!nueva) { gReiniciarPend = false; return; }
    bool yaInit = (gInited.find(nueva) != gInited.end() && gInited[nueva]);
    MostrarSolo(nueva);            // ocultar la activa anterior, mostrar la nueva
    gEscActiva = nueva;
    // init PEREZOSO (primera vez) o REINICIO forzado (cambiarEscena(n, true))
    if (!yaInit || gReiniciarPend) {
        if (gInitFn) gInitFn(nueva, gReiniciarPend && yaInit);   // reiniciar solo si ya estaba iniciada
        gInited[nueva] = true;
    }
    gReiniciarPend = false;
}

Object* W3dEscenaRaizDe(Object* o) {
    for (Object* p = o; p; p = p->Parent)
        if (p->getType() == ObjectType::ui) return p;
    return SceneCollection;   // fuera de toda escena UI (3D): la busqueda de refs sigue igual
}

Object* W3dEscenaBuscarRef(Object* o, const std::string& nombre) {
    Object* raiz = W3dEscenaRaizDe(o);
    if (raiz && raiz->getType() == ObjectType::ui) return FindObjectByName(raiz, nombre);
    return W3dBuscarNombreDesde(o, nombre, raiz);
}

static int CapaDe(Object* o);   // (abajo: las capas)
static bool CapaCorre(int i);
bool W3dEscenaEsDeActiva(Object* o) {
    if (!o) return false;
    if (!o->visible) return false;        // invisible: no corre (regla del editor de siempre)
    // UNA CAPA corre aunque no sea la escena activa (si no esta en pausa); la BASE en pausa no corre
    { const int c = CapaDe(o);
      if (c >= 0) { for (Object* p = o; p; p = p->Parent) { if (!p->visible) return false; if (EsCapa(p)) break; }
                    return CapaCorre(c); }
      if (W3dEscenaBasePausada()) return false; }
    if (gEscenas.empty()) return true;    // sin multi-escena: solo importa visible
    Object* raiz = W3dEscenaRaizDe(o);
    if (!raiz || raiz->getType() != ObjectType::ui) return true;  // fuera de escenas UI (3D): regla vieja
    // un juego 3D con su HUD: NO hay escena UI activa (manda la escena 3D) y la UI se dibuja encima como overlay;
    // sus scripts tienen que correr igual (antes el HUD se veia pero su logica nunca corria)
    if (!gEscActiva) return true;
    return raiz == (Object*)gEscActiva;   // pertenece a la escena activa?
}

static bool gDibujar3D = true;     // lua dibujar3D(on): false = el pase 3D no se hace
bool W3dEscena3DActiva() { return gDibujar3D; }
void W3dEscena3DSetActiva(bool on) { gDibujar3D = on; }

void W3dEscenaLimpiar() {
    gDibujar3D = true;     // el editor vuelve a ver la escena aunque el juego la haya apagado
    // el editor vuelve a EDICION: deshacer el MostrarSolo del Play (que dejo OCULTAS las
    // escenas no-activas con visible=false). Al cargar, TODAS las ventanas UI nacen visibles
    // (el .w3dui no guarda "visible" de la ventana), asi que restaurar a visible=true deja el
    // outliner como recien abierto y el editor puede volver a mostrar/editar cualquier escena.
    for (std::map<std::string, UI*>::iterator it = gEscenas.begin(); it != gEscenas.end(); ++it)
        if (it->second) it->second->visible = true;
    gEscenas.clear();
    gInited.clear();
    gEscActiva = 0;
    gHayCambio = false; gReiniciarPend = false; gCambioNombre.clear();
    // OJO: NO se toca gInicial (es un ajuste del proyecto, lo fija el importador/runtime)
}

// ===========================================================================
//  W3dPaletas — las paletas de colores a NIVEL PROYECTO (ver W3dPaletas.h).
//  Viven aca porque W3dEscena.cpp compila en el editor Y en todos los
//  runtimes standalone (consola/calculadora/whiskpaddle, linux/web/android).
// ===========================================================================
static std::vector<Paleta> gPaletas;
static bool gPaletasInit = false;

std::vector<Paleta>& W3dPaletas() {
    if (!gPaletasInit) { gPaletas.reserve(8); gPaletasInit = true; }
    return gPaletas;
}

void W3dPaletasLimpiar() { W3dPaletas().clear(); }

Paleta* W3dPaletaPorNombre(const std::string& n) {
    if (n.empty()) return 0;
    std::vector<Paleta>& ps = W3dPaletas();
    for (size_t i = 0; i < ps.size(); i++)
        if (ps[i].nombre == n) return &ps[i];
    return 0;
}

// INVARIANTE 1 al adoptar: completar las paletas CORTAS para que todas midan
// igual. El color que falta en el indice c se copia de la primera paleta que
// lo tenga (mismo nombre y valor: un default razonable).
static void PalNormalizarLargos() {
    std::vector<Paleta>& ps = W3dPaletas();
    size_t maxN = 0;
    for (size_t i = 0; i < ps.size(); i++)
        if (ps[i].colores.size() > maxN) maxN = ps[i].colores.size();
    for (size_t i = 0; i < ps.size(); i++)
        for (size_t c = ps[i].colores.size(); c < maxN; c++) {
            PaletaColor pc;
            pc.nombre = "Color";
            pc.rgba[0] = pc.rgba[1] = pc.rgba[2] = pc.rgba[3] = 1.0f;
            for (size_t j = 0; j < ps.size(); j++)
                if (c < ps[j].colores.size()) { pc = ps[j].colores[c]; break; }
            ps[i].colores.push_back(pc);
        }
}

void W3dPaletasAdoptar(const std::vector<Paleta>& nuevas) {
    std::vector<Paleta>& ps = W3dPaletas();
    for (size_t i = 0; i < nuevas.size(); i++) {
        if (ps.size() >= 8) break;                          // tope (reserve del store)
        if (W3dPaletaPorNombre(nuevas[i].nombre)) continue; // merge: la primera gana
        Paleta pa;
        pa.nombre = nuevas[i].nombre;
        ps.push_back(pa);
        // reserve sobre el vector VIVO: los punteros a los rgba quedan estables
        ps.back().colores.reserve(32);
        for (size_t c = 0; c < nuevas[i].colores.size() && c < 32; c++)
            ps.back().colores.push_back(nuevas[i].colores[c]);
    }
    PalNormalizarLargos();
}

Paleta* W3dPaletaEfectiva(Object* o) {
    for (Object* p = o; p; p = p->Parent) {
        if (!p->paleta.empty()) {
            Paleta* pa = W3dPaletaPorNombre(p->paleta);
            if (pa) return pa;   // nombre roto: se sigue subiendo (tolerante)
        }
        if (p->getType() == ObjectType::ui && p->paleta.empty()) {
            // RETROCOMPAT: una raiz UI sin seleccion propia cae a su
            // paletaActiva de siempre (-1 = deseleccionada -> propio). Si el
            // proyecto tiene una paleta con ese nombre, manda la del proyecto
            // (la copia local del UI puede estar vieja).
            UI* u = (UI*)p;
            if (u->paletaActiva >= 0 && u->paletaActiva < (int)u->paletas.size()) {
                Paleta* local = &u->paletas[u->paletaActiva];
                Paleta* proy = W3dPaletaPorNombre(local->nombre);
                return proy ? proy : local;
            }
            return 0;
        }
    }
    return 0;   // nadie eligio: colores propios
}

std::vector<PaletaColor>* W3dColoresEfectivos(Object* o) {
    Paleta* pa = W3dPaletaEfectiva(o);
    return pa ? &pa->colores : 0;
}

// nombre libre de COLOR de paleta. Los nombres van por INDICE y son los MISMOS en todas
// las paletas (invariante), asi que el espacio de nombres es la lista de la paleta 0.
// 'excepto' = indice que puede conservar el suyo (-1 = ninguno).
static bool PalColorExiste(const std::string& n, void* ctx) {
    int excepto = *(int*)ctx;
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.empty()) return false;
    for (size_t i = 0; i < ps[0].colores.size(); i++)
        if ((int)i != excepto && ps[0].colores[i].nombre == n) return true;
    return false;
}
std::string W3dPaletaColorNombreLibre(const std::string& base, int excepto) {
    return W3dNombreUnico(base, "Color", PalColorExiste, &excepto);
}

// INVARIANTE 1: el color nuevo va a TODAS las paletas (mismo nombre y valor)
int W3dPaletaAgregarColor(const std::string& nombre, const float* rgba) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.empty()) return -1;                      // sin paletas no hay donde
    if (ps[0].colores.size() >= 32) return -1;      // tope del reserve (punteros estables)
    const std::string unico = W3dPaletaColorNombreLibre(nombre, -1);
    for (size_t i = 0; i < ps.size(); i++) {
        PaletaColor pc;
        pc.nombre = unico;
        for (int q = 0; q < 4; q++) pc.rgba[q] = rgba ? rgba[q] : 1.0f;
        ps[i].colores.push_back(pc);
    }
    return (int)ps[0].colores.size() - 1;
}

// corrige UNA referencia pal* contra el borrado del indice 'idx' (regla 2):
// mayores se corren -1; la que apuntaba a idx pasa a color PROPIO con el
// valor EFECTIVO horneado (nada queda apuntando a memoria basura). Se llama
// ANTES de borrar el color de las paletas (el valor a hornear aun existe).
static void PalFixRef(Object* o, int* pal, float* propio, int idx) {
    if (*pal < 0) return;
    if (*pal == idx) {
        std::vector<PaletaColor>* cs = W3dColoresEfectivos(o);
        if (cs && idx < (int)cs->size())
            for (int q = 0; q < 4; q++) propio[q] = (*cs)[idx].rgba[q];
        *pal = -1;
    } else if (*pal > idx) {
        (*pal)--;
    }
}

// recorre TODO el arbol (todas las escenas del proyecto) corrigiendo los
// campos pal* de cada tipo de elemento 2D
static void PalFixRefsRec(Object* o, int idx) {
    if (!o) return;
    ObjectType t = o->getType();
    if (t == ObjectType::texto2d) {
        Texto2D* tx = (Texto2D*)o;
        PalFixRef(o, &tx->palColor, tx->color, idx);
    } else if (t == ObjectType::rect2d) {
        Rect2D* r = (Rect2D*)o;
        PalFixRef(o, &r->palColor, r->color, idx);
    } else if (t == ObjectType::imagen2d) {
        Imagen2D* im = (Imagen2D*)o;
        PalFixRef(o, &im->palTinte, im->color, idx);
    } else if (t == ObjectType::slice9) {
        Slice9* s9 = (Slice9*)o;
        PalFixRef(o, &s9->palTinte, s9->color, idx);
    } else if (t == ObjectType::boton2d) {
        Boton2D* b = (Boton2D*)o;
        PalFixRef(o, &b->palFondo, b->colorFondo, idx);
        PalFixRef(o, &b->palTexto, b->colorTexto, idx);
        PalFixRef(o, &b->palBorde, b->colorBorde, idx);
        PalFixRef(o, &b->palHover, b->colorHover, idx);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalFixRefsRec(o->Childrens[i], idx);
}

void W3dPaletaBorrarColor(int idx) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.empty() || idx < 0 || idx >= (int)ps[0].colores.size()) return;
    // regla 2, paso 1: PRIMERO corregir las referencias del proyecto entero
    // (el valor horneado se lee de la paleta efectiva ANTES de borrar)
    PalFixRefsRec(SceneCollection, idx);
    // paso 2: borrar el MISMO indice en TODAS las paletas (invariante 1)
    for (size_t i = 0; i < ps.size(); i++)
        if (idx < (int)ps[i].colores.size())
            ps[i].colores.erase(ps[i].colores.begin() + idx);
    // regla 3: sin colores -> DESELECCIONAR las paletas activas legacy de las
    // raices UI (todo ya quedo en color propio por el paso 1)
    if (!ps.empty() && ps[0].colores.empty() && SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
            Object* o = SceneCollection->Childrens[i];
            if (o && o->getType() == ObjectType::ui) ((UI*)o)->paletaActiva = -1;
        }
}

// nombre libre de PALETA (espacio GLOBAL de proyecto; el nombre es la CLAVE que usan
// Object::paleta y W3dPaletaEfectiva). 'excepto' = indice que conserva el suyo.
static bool PalNombreExiste(const std::string& n, void* ctx) {
    int excepto = *(int*)ctx;
    std::vector<Paleta>& ps = W3dPaletas();
    for (size_t i = 0; i < ps.size(); i++)
        if ((int)i != excepto && ps[i].nombre == n) return true;
    return false;
}
std::string W3dPaletaNombreLibre(const std::string& base, int excepto) {
    return W3dNombreUnico(base, "Paleta", PalNombreExiste, &excepto);
}

int W3dPaletaNueva(const std::string& nombre, int baseIdx) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.size() >= 8) return -1;
    Paleta pa;
    // antes RECHAZABA el nombre tomado (return -1). Ahora SUFIJA, como todo el resto
    // del editor (pedido del dueno: "ponele el .001 si queres repetirlo").
    pa.nombre = W3dPaletaNombreLibre(nombre, -1);
    ps.push_back(pa);
    ps.back().colores.reserve(32);                  // punteros estables (ver arriba)
    // COPIA de la paleta base (invariante 1: mismos largos y nombres)
    if (baseIdx < 0 || baseIdx >= (int)ps.size() - 1) baseIdx = 0;
    if ((int)ps.size() > 1)
        for (size_t c = 0; c < ps[baseIdx].colores.size(); c++)
            ps.back().colores.push_back(ps[baseIdx].colores[c]);
    return (int)ps.size() - 1;
}

// los objetos que ELEGIAN la paleta borrada vuelven a heredar ("")
static void PalLimpiarSelRec(Object* o, const std::string& nombre) {
    if (!o) return;
    if (o->paleta == nombre) o->paleta.clear();
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalLimpiarSelRec(o->Childrens[i], nombre);
}

void W3dPaletaBorrarPaleta(int idx) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (idx < 0 || idx >= (int)ps.size()) return;
    std::string nombre = ps[idx].nombre;
    ps.erase(ps.begin() + idx);
    PalLimpiarSelRec(SceneCollection, nombre);
}

static void PalRenombrarSelRec(Object* o, const std::string& de, const std::string& a) {
    if (!o) return;
    if (o->paleta == de) o->paleta = a;
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalRenombrarSelRec(o->Childrens[i], de, a);
}

void W3dPaletaRenombrar(int idx, const std::string& nuevo) {
    std::vector<Paleta>& ps = W3dPaletas();
    if (idx < 0 || idx >= (int)ps.size()) return;
    // antes DESCARTABA el rename si el nombre estaba tomado (se perdia en silencio);
    // ahora sufija con LA regla comun. El vinculo objeto->paleta (Object::paleta, por
    // NOMBRE) se arrastra igual que siempre con PalRenombrarSelRec.
    const std::string unico = W3dPaletaNombreLibre(nuevo, idx);
    if (ps[idx].nombre == unico) return;
    std::string viejo = ps[idx].nombre;
    ps[idx].nombre = unico;
    PalRenombrarSelRec(SceneCollection, viejo, unico);
}

// ---------------------------------------------------------------------------
// ============================================================================
//  LAS CAPAS y LOS MENSAJES (ver W3dEscena.h)
// ============================================================================
struct W3dCapa { std::string nombre; Object* raiz; unsigned serial; bool es3D; bool pausada; };
static std::vector<W3dCapa> gCapas;                 // en el orden en que se cargaron (la ultima, arriba)
enum { CapaCargar = 0, CapaDescargar, CapaMostrar, CapaOcultar, CapaPausar, CapaReanudar };
struct W3dCapaPedido { int op; std::string nombre; };
static std::vector<W3dCapaPedido> gCapaPedidos;
struct W3dMensajeEscena { std::string destino, texto, de; W3dValorLua datos; };
static std::vector<W3dMensajeEscena> gMensajes;
static bool gBasePausada = false;
static Object* (*gCapa3DCrear)(const std::string&) = 0;
static std::string (*gNombreBase3D)() = 0;
void W3dEscenaSetCapas3D(Object* (*crear)(const std::string&), std::string (*nombreBase)()) {
    gCapa3DCrear = crear; gNombreBase3D = nombreBase;
}

static bool CapaViva(int i) {
    return i >= 0 && i < (int)gCapas.size() && W3dObjetoVivoSerial(gCapas[(size_t)i].raiz, gCapas[(size_t)i].serial);
}
static bool EsCapa(Object* o) {
    if (!o) return false;
    for (size_t i = 0; i < gCapas.size(); i++) if (gCapas[i].raiz == o && CapaViva((int)i)) return true;
    return false;
}
static int CapaDe(Object* o) {
    if (gCapas.empty()) return -1;
    for (Object* p = o; p; p = p->Parent)
        for (size_t i = 0; i < gCapas.size(); i++) if (gCapas[i].raiz == p && CapaViva((int)i)) return (int)i;
    return -1;
}
static bool CapaCorre(int i) {
    if (!CapaViva(i)) return false;
    const W3dCapa& c = gCapas[(size_t)i];
    return c.raiz->visible && !c.pausada;
}
static int CapaPorNombre(const std::string& n) {
    for (size_t i = 0; i < gCapas.size(); i++) if (gCapas[i].nombre == n && CapaViva((int)i)) return (int)i;
    return -1;
}
// la BASE: la escena (o el juego) 3D que corre, o la escena 2D activa en un juego solo 2D
static std::string NombreBase() {
    if (gNombreBase3D) { std::string b = gNombreBase3D(); if (!b.empty()) return b; }
    return gEscActiva ? gEscActiva->name : std::string();
}
int     W3dEscenaCapas() { return (int)gCapas.size(); }
Object* W3dEscenaCapaRaiz(int i) { return CapaViva(i) ? gCapas[(size_t)i].raiz : 0; }
bool    W3dEscenaCapaEs3D(int i) { return CapaViva(i) && gCapas[(size_t)i].es3D; }
bool    W3dEscenaEsCapaRaiz(Object* o) { return EsCapa(o); }
bool    W3dEscenaEnCapa(Object* o) { return CapaDe(o) >= 0; }
bool    W3dEscenaBasePausada() { return gBasePausada; }
std::string W3dEscenaDe(Object* o) {
    const int c = CapaDe(o);
    if (c >= 0) return gCapas[(size_t)c].nombre;
    for (Object* p = o; p; p = p->Parent)
        if (p->getType() == ObjectType::ui && p != (Object*)gEscActiva) return p->name;   // una escena 2D (no la base)
    return NombreBase();
}
void W3dEscenaCapasDesprender(bool conUI, std::vector<Object*>& sueltas) {
    sueltas.clear();
    for (size_t i = 0; i < gCapas.size(); i++) {
        if (!CapaViva((int)i)) continue;
        Object* o = gCapas[i].raiz;
        if (!conUI && !gCapas[i].es3D) continue;
        if (o->Parent) {
            std::vector<Object*>& ch = o->Parent->Childrens;
            for (size_t k = 0; k < ch.size(); k++) if (ch[k] == o) { ch.erase(ch.begin() + (long)k); break; }
            o->Parent = 0;
        }
        sueltas.push_back(o);
    }
}
void W3dEscenaCapasColgar(Object* raiz, const std::vector<Object*>& sueltas) {
    if (!raiz) return;
    for (size_t i = 0; i < sueltas.size(); i++) {
        if (!sueltas[i] || sueltas[i]->Parent) continue;
        sueltas[i]->Parent = raiz;
        raiz->Childrens.push_back(sueltas[i]);
    }
}
bool W3dEscenaCapaAdentroDe(Object* raiz) {
    for (size_t i = 0; i < gCapas.size(); i++) {
        if (!CapaViva((int)i)) continue;
        for (Object* p = gCapas[i].raiz; p; p = p->Parent) if (p == raiz) return true;
    }
    return false;
}
void W3dEscenaCapasLimpiar() {
    for (size_t i = 0; i < gCapas.size(); i++)
        if (CapaViva((int)i) && !gCapas[i].es3D) gCapas[i].raiz->visible = true;   // (en edicion las UI se ven)
    gCapas.clear();
    gCapaPedidos.clear();
    gMensajes.clear();
    gBasePausada = false;
}

// los scripts de un subarbol (los de una escena 2D que se descarga)
static void DescargarScriptsDe(Object* o) {
    if (!o) return;
    if (o->scriptDatos && !o->scriptDatos->scripts.empty()) W3dScriptDescargarDe(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) DescargarScriptsDe(o->Childrens[i]);
}
static void CapasAplicar() {
    if (gCapaPedidos.empty()) return;
    std::vector<W3dCapaPedido> lista;
    lista.swap(gCapaPedidos);
    for (size_t k = 0; k < lista.size(); k++) {
        const std::string& n = lista[k].nombre;
        const int i = CapaPorNombre(n);
        switch (lista[k].op) {
            case CapaCargar: {
                if (i >= 0) break;                                    // ya esta cargada
                if (n == NombreBase()) { w3dLogfW("[escenas] cargarEscena('%s'): es la escena que corre", n.c_str()); break; }
                W3dCapa c; c.nombre = n; c.raiz = 0; c.serial = 0; c.es3D = false; c.pausada = false;
                UI* u = W3dEscenaBuscar(n);
                if (u) {
                    c.raiz = u; c.serial = u->serial;
                    gCapas.push_back(c);
                    u->visible = true;
                    if (gInited.find(u) == gInited.end() || !gInited[u]) {
                        if (gInitFn) gInitFn(u, false);
                        gInited[u] = true;
                    }
                } else if (gCapa3DCrear) {
                    // (se anota ANTES de crearla: lo que su inicio() pida ya la ve como capa)
                    Object* o = gCapa3DCrear(n);
                    if (o) { c.raiz = o; c.serial = o->serial; c.es3D = true; gCapas.push_back(c); }
                }
                if (c.raiz) w3dLogf("[escenas] '%s' cargada encima (%s)", n.c_str(), c.es3D ? "3D" : "2D");
                else w3dLogfW("[escenas] cargarEscena('%s'): no hay ninguna escena con ese nombre", n.c_str());
                break;
            }
            case CapaDescargar: {
                if (i < 0) { w3dLogfW("[escenas] descargarEscena('%s'): no esta cargada encima", n.c_str()); break; }
                W3dCapa c = gCapas[(size_t)i];
                gCapas.erase(gCapas.begin() + i);
                if (c.es3D) W3dScriptDestruirAhora(c.raiz);
                else {
                    DescargarScriptsDe(c.raiz);
                    c.raiz->visible = false;
                    gInited.erase((UI*)c.raiz);
                }
                w3dLogf("[escenas] '%s' descargada", n.c_str());
                break;
            }
            case CapaMostrar: case CapaOcultar:
                if (i >= 0) gCapas[(size_t)i].raiz->visible = (lista[k].op == CapaMostrar);
                else w3dLogfW("[escenas] %s('%s'): solo una escena cargada encima", lista[k].op == CapaMostrar ?
                              "mostrarEscena" : "ocultarEscena", n.c_str());
                break;
            case CapaPausar: case CapaReanudar: {
                const bool p = (lista[k].op == CapaPausar);
                if (i >= 0) gCapas[(size_t)i].pausada = p;
                else if (n == NombreBase()) gBasePausada = p;
                else w3dLogfW("[escenas] %s('%s'): no esta cargada", p ? "pausarEscena" : "reanudarEscena", n.c_str());
                break;
            }
        }
    }
}

// a cada script del subarbol: alRecibir(texto, datos, de). 'sinCapas': sin entrar a las capas (la base)
static void EntregarEn(Object* o, const W3dMensajeEscena& m, bool sinCapas, std::set<Object*>& vistos) {
    if (!o || vistos.count(o)) return;
    if (sinCapas && EsCapa(o)) return;
    vistos.insert(o);
    if (o->scriptDatos && !o->scriptDatos->scripts.empty())
        W3dScriptLlamarConDatos(o, "alRecibir", m.texto, m.datos, m.de);
    for (size_t i = 0; i < o->Childrens.size(); i++) EntregarEn(o->Childrens[i], m, sinCapas, vistos);
}
static void CapasEntregarMensajes() {
    if (gMensajes.empty()) return;
    std::vector<W3dMensajeEscena> lista;
    lista.swap(gMensajes);   // (lo que alRecibir mande va al frame siguiente)
    for (size_t k = 0; k < lista.size(); k++) {
        const W3dMensajeEscena& m = lista[k];
        std::set<Object*> vistos;
        if (m.destino.empty() || m.destino == "*") {                 // a todas
            EntregarEn(SceneCollection, m, false, vistos);
            for (size_t i = 0; i < gCapas.size(); i++) if (CapaViva((int)i)) EntregarEn(gCapas[i].raiz, m, false, vistos);
            continue;
        }
        const int c = CapaPorNombre(m.destino);
        if (c >= 0) { EntregarEn(gCapas[(size_t)c].raiz, m, false, vistos); continue; }
        if (m.destino == NombreBase()) { EntregarEn(SceneCollection, m, true, vistos); continue; }
        UI* u = W3dEscenaBuscar(m.destino);
        if (u) { EntregarEn(u, m, false, vistos); continue; }
        w3dLogfW("[escenas] enviar('%s', '%s'): no hay ninguna escena cargada con ese nombre", m.destino.c_str(),
                 m.texto.c_str());
    }
}

// ---- los binds de lua ----
static void PedirCapa(lua_State* L, int op) {
    const char* n = lua_tostring(L, 1);
    if (!n || !*n) return;
    W3dCapaPedido p; p.op = op; p.nombre = n;
    gCapaPedidos.push_back(p);
}
void W3dEscenaPedirCarga(const std::string& nombre) {
    if (nombre.empty()) return;
    W3dCapaPedido p; p.op = CapaCargar; p.nombre = nombre;
    gCapaPedidos.push_back(p);
}
static int LCargarEscena(lua_State* L)    { PedirCapa(L, CapaCargar); return 0; }
static int LDescargarEscena(lua_State* L) { PedirCapa(L, CapaDescargar); return 0; }
static int LMostrarEscena(lua_State* L)   { PedirCapa(L, CapaMostrar); return 0; }
static int LOcultarEscena(lua_State* L)   { PedirCapa(L, CapaOcultar); return 0; }
static int LPausarEscena(lua_State* L)    { PedirCapa(L, CapaPausar); return 0; }
static int LReanudarEscena(lua_State* L)  { PedirCapa(L, CapaReanudar); return 0; }
static int LEscenaCargada(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    lua_pushboolean(L, n && (CapaPorNombre(n) >= 0 || NombreBase() == n) ? 1 : 0);
    return 1;
}
// enviar(escena, mensaje [, datos]): escena nil o "*" = a todas
static int LEnviar(lua_State* L) {
    W3dMensajeEscena m;
    const char* d = lua_tostring(L, 1);
    const char* t = lua_tostring(L, 2);
    if (!t) return 0;
    m.destino = d ? d : "";
    m.texto = t;
    if (lua_gettop(L) >= 3) m.datos = W3dValorLuaLeer(L, 3);
    lua_getfield(L, LUA_REGISTRYINDEX, "w3d_duenio");
    Object* yo = lua_islightuserdata(L, -1) ? (Object*)lua_touserdata(L, -1) : 0;
    lua_pop(L, 1);
    m.de = yo ? W3dEscenaDe(yo) : std::string();
    gMensajes.push_back(m);
    return 0;
}
//  BIND lua: cambiarEscena(nombre[, reiniciar]).
// ---------------------------------------------------------------------------
static int LCambiarEscena(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    bool re = lua_toboolean(L, 2) != 0;
    if (n) W3dEscenaPedirCambio(n, re);
    return 0;
}
// ---------------------------------------------------------------------------
//  BINDS lua de las CINEMATICAS (ver W3dEscena.h / W3dRaices.h):
//    reproducirEscena("Cinematica 1" [, "alTerminar"]) -> true si la escena existe: se muestra esa
//        escena 3D (sus luces, camaras, objetos y su animacion de Inicio a Fin) con el juego en pausa
//        y al terminar se vuelve al juego tal cual estaba; ahi se llama alTerminar() del script que la
//        pidio. Se aplica al final del frame, como cambiarEscena().
//    pararEscena3D()          corta la cinematica (se vuelve al juego y se llama alTerminar igual)
//    escenaReproduciendo()    el nombre de la escena que se esta reproduciendo (nil si ninguna)
// ---------------------------------------------------------------------------
static bool (*gCineReproducir)(const std::string&, Object*, const std::string&) = 0;
static void (*gCineParar)() = 0;
static const char* (*gCineActual)() = 0;
void W3dEscenaSetCine(bool (*reproducir)(const std::string&, Object*, const std::string&), void (*parar)(),
                      const char* (*actual)()) {
    gCineReproducir = reproducir; gCineParar = parar; gCineActual = actual;
}
static int LReproducirEscena(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    const char* fn = lua_tostring(L, 2);
    // el objeto del script que la pide (el mismo registro que yo()): a sus scripts se les llama 'fn'
    lua_getfield(L, LUA_REGISTRYINDEX, "w3d_duenio");
    Object* llamador = lua_islightuserdata(L, -1) ? (Object*)lua_touserdata(L, -1) : 0;
    lua_pop(L, 1);
    const bool ok = n && gCineReproducir && gCineReproducir(n, llamador, fn ? std::string(fn) : std::string());
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}
static int LPararEscena3D(lua_State* L) {
    (void)L;
    if (gCineParar) gCineParar();
    return 0;
}
static int LEscenaReproduciendo(lua_State* L) {
    const char* a = gCineActual ? gCineActual() : 0;
    if (a && *a) lua_pushstring(L, a); else lua_pushnil(L);
    return 1;
}
// dibujar3D(on): prende/apaga el pase 3D entero (ver W3dEscena.h). Sin argumento devuelve el estado.
static int LDibujar3D(lua_State* L) {
    if (lua_gettop(L) >= 1) gDibujar3D = lua_toboolean(L, 1) != 0;
    lua_pushboolean(L, gDibujar3D ? 1 : 0);
    return 1;
}
void W3dEscenaRegistrarBind(void* Lv) {
    lua_State* L = (lua_State*)Lv;
    lua_pushcfunction(L, LCambiarEscena); lua_setglobal(L, "cambiarEscena");
    lua_pushcfunction(L, LReproducirEscena);    lua_setglobal(L, "reproducirEscena");
    lua_pushcfunction(L, LPararEscena3D);       lua_setglobal(L, "pararEscena3D");
    lua_pushcfunction(L, LEscenaReproduciendo); lua_setglobal(L, "escenaReproduciendo");
    lua_pushcfunction(L, LDibujar3D);           lua_setglobal(L, "dibujar3D");
    // las capas y los mensajes entre escenas
    lua_pushcfunction(L, LCargarEscena);        lua_setglobal(L, "cargarEscena");
    lua_pushcfunction(L, LDescargarEscena);     lua_setglobal(L, "descargarEscena");
    lua_pushcfunction(L, LMostrarEscena);       lua_setglobal(L, "mostrarEscena");
    lua_pushcfunction(L, LOcultarEscena);       lua_setglobal(L, "ocultarEscena");
    lua_pushcfunction(L, LPausarEscena);        lua_setglobal(L, "pausarEscena");
    lua_pushcfunction(L, LReanudarEscena);      lua_setglobal(L, "reanudarEscena");
    lua_pushcfunction(L, LEscenaCargada);       lua_setglobal(L, "escenaCargada");
    lua_pushcfunction(L, LEnviar);              lua_setglobal(L, "enviar");
}
