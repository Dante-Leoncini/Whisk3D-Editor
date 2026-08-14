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

void W3dEscenaRegistrarTodas() {
    gEscenas.clear();
    gInited.clear();
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
        if (u) u->visible = (u == (Object*)nueva);
    }
}

void W3dEscenaArrancar() {
    if (gEscenas.empty()) return;
    UI* ini = W3dEscenaBuscar(gInicial);
    if (!ini) ini = gEscenas.begin()->second;   // fallback: la primera escena registrada
    MostrarSolo(ini);
    gEscActiva = ini;
    if (gInitFn) gInitFn(ini, false);            // init de la escena inicial (el resto, perezoso)
    gInited[ini] = true;
}

void W3dEscenaPedirCambio(const std::string& nombre, bool reiniciar) {
    // ignorar un nombre inexistente: no romper el juego por un typo del script
    if (!W3dEscenaBuscar(nombre)) return;
    gHayCambio = true; gCambioNombre = nombre; gReiniciarPend = reiniciar;
}

void W3dEscenaAplicarPendiente() {
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

bool W3dEscenaEsDeActiva(Object* o) {
    if (!o) return false;
    if (!o->visible) return false;        // invisible: no corre (regla del editor de siempre)
    if (gEscenas.empty()) return true;    // sin multi-escena: solo importa visible
    Object* raiz = W3dEscenaRaizDe(o);
    if (!raiz || raiz->getType() != ObjectType::ui) return true;  // fuera de escenas UI (3D): regla vieja
    return raiz == (Object*)gEscActiva;   // pertenece a la escena activa?
}

void W3dEscenaLimpiar() {
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
//  BIND lua: cambiarEscena(nombre[, reiniciar]).
// ---------------------------------------------------------------------------
static int LCambiarEscena(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    bool re = lua_toboolean(L, 2) != 0;
    if (n) W3dEscenaPedirCambio(n, re);
    return 0;
}
void W3dEscenaRegistrarBind(void* Lv) {
    lua_State* L = (lua_State*)Lv;
    lua_pushcfunction(L, LCambiarEscena); lua_setglobal(L, "cambiarEscena");
}
