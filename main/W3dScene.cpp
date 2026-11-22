// ============================================================================
//  W3dScene.cpp — ver W3dScene.h. El motor multi-escena COMPARTIDO por el
//  editor (Play) y el runtime compilado. C++03 puro. Comentarios sin acentos.
// ============================================================================
#include "W3dScene.h"
#include "W3dPalettes.h"
#include "W3dNames.h"   // LA regla de nombres unicos (compartida por todo el editor)
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Text2D.h"    // los tipos con campos pal* (fix de referencias
#include "objects/Image2D.h"   // al borrar un color de las paletas)
#include "objects/Rect2D.h"
#include "objects/Slice9.h"
#include "objects/Button2D.h"

extern "C" {
#include "lua.h"
}

#include <map>
#include <set>
#include "script/W3dScript.h"   // las capas: scripts, objetos nuevos y destruidos; los mensajes (W3dValueLua)
#include "w3dlog.h"

// ---------------------------------------------------------------------------
//  ESTADO del sistema (una sola simulacion/juego a la vez).
// ---------------------------------------------------------------------------
static std::map<std::string, UI*> gScenes;   // nombre -> raiz UI (todas las escenas)
static UI*  gActiveEsc = 0;                    // la escena que corre y se ve
static std::map<UI*, bool> gInited;            // escenas que YA corrieron start() (init perezoso)
static std::string gInitial;                   // nombre de la escena que arranca (ajuste del proyecto)
static bool gMultiMode = false;                // modo multi-escena declarado (informativo)
static W3dSceneInitFn gInitFn = 0;             // el init de una escena (lo pone cada build)
// cambio de escena PENDIENTE (se aplica al final del frame)
static bool gHasChange = false;
static bool gRestartPend = false;
static std::string gChangeName;

void W3dSceneSetInit(W3dSceneInitFn fn) { gInitFn = fn; }

// las ESCENAS 3D (ver W3dScene.h): los pone main/W3dRoots.cpp, que solo existe con 3D
static bool (*gRequest3D)(const std::string&, bool) = 0;
static bool (*gApply3D)() = 0;
void W3dSceneSet3D(bool (*request)(const std::string&, bool), bool (*apply)()) {
    gRequest3D = request; gApply3D = apply;
}

static bool IsLayer(Object* o);   // (abajo: las capas)
void W3dSceneRegisterAll() {
    gScenes.clear();
    // (una escena 2D cargada como capa ya corrio su start(): sigue iniciada)
    { std::map<UI*, bool> old; old.swap(gInited);
      for (std::map<UI*, bool>::iterator it = old.begin(); it != old.end(); ++it) if (IsLayer((Object*)it->first)) gInited[it->first] = true; }
    gActiveEsc = 0;
    gHasChange = false; gRestartPend = false; gChangeName.clear();
    if (!SceneCollection) return;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::ui) gScenes[o->name] = (UI*)o;
    }
}

bool W3dSceneHasScenes() { return !gScenes.empty(); }
UI*  W3dActiveScene()     { return gActiveEsc; }

UI* W3dSceneFind(const std::string& name) {
    std::map<std::string, UI*>::iterator it = gScenes.find(name);
    return (it != gScenes.end()) ? it->second : 0;
}

void W3dSceneSetInitial(const std::string& name) { gInitial = name; }
const std::string& W3dInitialScene() { return gInitial; }
void W3dSceneSetMode(bool multi) { gMultiMode = multi; }
bool W3dSceneMode() { return gMultiMode; }

// muestra SOLO 'nueva' entre las escenas registradas (el resto se oculta: dejan de
// correr y de recibir input; el overlay no las dibuja por visible=false).
static void ShowOnly(UI* newValue) {
    for (std::map<std::string, UI*>::iterator it = gScenes.begin(); it != gScenes.end(); ++it) {
        Object* u = it->second;
        if (u && !IsLayer(u)) u->visible = (u == (Object*)newValue);   // (una capa sigue como esta: encima)
    }
}

void W3dSceneStart() {
    if (gScenes.empty()) return;
    UI* start = W3dSceneFind(gInitial);
    if (start && IsLayer((Object*)start)) start = 0;   // (una escena cargada encima ya corre: no es la que arranca)
    // fallback: la primera escena registrada que no sea una capa (sin ninguna: no hay escena 2D activa)
    for (std::map<std::string, UI*>::iterator it = gScenes.begin(); it != gScenes.end() && !start; ++it)
        if (it->second && !IsLayer((Object*)it->second)) start = it->second;
    if (!start) return;
    ShowOnly(start);
    gActiveEsc = start;
    if (gInitFn) gInitFn(start, false);            // init de la escena inicial (el resto, perezoso)
    gInited[start] = true;
}

void W3dSceneRequestChange(const std::string& name, bool restart) {
    // no es una escena UI: puede ser una ESCENA 3D del proyecto (se descarga la actual y se
    // carga esa, al final del frame). Un nombre inexistente se ignora: no romper el juego por
    // un typo del script.
    if (!W3dSceneFind(name)) { if (gRequest3D) gRequest3D(name, restart); return; }
    gHasChange = true; gChangeName = name; gRestartPend = restart;
}

static void LayersDeliverMessages();
static void LayersApply();
void W3dSceneApplyPending() {
    // primero los MENSAJES (un "guardar" llega antes de que la escena se vaya) y las CAPAS
    LayersDeliverMessages();
    LayersApply();
    // una ESCENA 3D pedida cambia el arbol ENTERO (sus escenas UI incluidas): un cambio de UI
    // pendiente era de la escena que se fue
    if (gApply3D && gApply3D()) { gHasChange = false; gRestartPend = false; return; }
    if (!gHasChange) return;
    gHasChange = false;
    UI* newValue = W3dSceneFind(gChangeName);
    if (!newValue) { gRestartPend = false; return; }
    bool alreadyInit = (gInited.find(newValue) != gInited.end() && gInited[newValue]);
    ShowOnly(newValue);            // ocultar la activa anterior, mostrar la nueva
    gActiveEsc = newValue;
    // init PEREZOSO (primera vez) o REINICIO forzado (changeScene(n, true))
    if (!alreadyInit || gRestartPend) {
        if (gInitFn) gInitFn(newValue, gRestartPend && alreadyInit);   // reiniciar solo si ya estaba iniciada
        gInited[newValue] = true;
    }
    gRestartPend = false;
}

Object* W3dSceneRootOf(Object* o) {
    for (Object* p = o; p; p = p->Parent)
        if (p->getType() == ObjectType::ui) return p;
    return SceneCollection;   // fuera de toda escena UI (3D): la busqueda de refs sigue igual
}

Object* W3dSceneFindRef(Object* o, const std::string& name) {
    Object* root = W3dSceneRootOf(o);
    if (root && root->getType() == ObjectType::ui) return FindObjectByName(root, name);
    return W3dFindNameFrom(o, name, root);
}

static int LayerOf(Object* o);   // (abajo: las capas)
static bool LayerRuns(int i);
bool W3dSceneIsFromActive(Object* o) {
    if (!o) return false;
    if (!o->visible) return false;        // invisible: no corre (regla del editor de siempre)
    // UNA CAPA corre aunque no sea la escena activa (si no esta en pausa); la BASE en pausa no corre
    { const int c = LayerOf(o);
      if (c >= 0) { for (Object* p = o; p; p = p->Parent) { if (!p->visible) return false; if (IsLayer(p)) break; }
                    return LayerRuns(c); }
      if (W3dSceneBasePaused()) return false; }
    if (gScenes.empty()) return true;    // sin multi-escena: solo importa visible
    Object* root = W3dSceneRootOf(o);
    if (!root || root->getType() != ObjectType::ui) return true;  // fuera de escenas UI (3D): regla vieja
    // un juego 3D con su HUD: NO hay escena UI activa (manda la escena 3D) y la UI se dibuja encima como overlay;
    // sus scripts tienen que correr igual (antes el HUD se veia pero su logica nunca corria)
    if (!gActiveEsc) return true;
    return root == (Object*)gActiveEsc;   // pertenece a la escena activa?
}

static bool gDraw3D = true;     // lua draw3D(on): false = el pase 3D no se hace
bool W3dScene3DActive() { return gDraw3D; }
void W3dScene3DSetActive(bool on) { gDraw3D = on; }

void W3dSceneClear() {
    gDraw3D = true;     // el editor vuelve a ver la escena aunque el juego la haya apagado
    // el editor vuelve a EDICION: deshacer el ShowOnly del Play (que dejo OCULTAS las
    // escenas no-activas con visible=false). Al cargar, TODAS las ventanas UI nacen visibles
    // (el .w3dui no guarda "visible" de la ventana), asi que restaurar a visible=true deja el
    // outliner como recien abierto y el editor puede volver a show/editar cualquier escena.
    for (std::map<std::string, UI*>::iterator it = gScenes.begin(); it != gScenes.end(); ++it)
        if (it->second) it->second->visible = true;
    gScenes.clear();
    gInited.clear();
    gActiveEsc = 0;
    gHasChange = false; gRestartPend = false; gChangeName.clear();
    // OJO: NO se toca gInitial (es un ajuste del proyecto, lo fija el importador/runtime)
}

// ===========================================================================
//  W3dPalettes — las paletas de colores a NIVEL PROYECTO (ver W3dPalettes.h).
//  Viven aca porque W3dScene.cpp compila en el editor Y en todos los
//  runtimes standalone (consola/calculadora/whiskpaddle, linux/web/android).
// ===========================================================================
static std::vector<Palette> gPalettes;
static bool gPalettesInit = false;

std::vector<Palette>& W3dPalettes() {
    if (!gPalettesInit) { gPalettes.reserve(8); gPalettesInit = true; }
    return gPalettes;
}

void W3dPalettesClear() { W3dPalettes().clear(); }

Palette* W3dPaletteByName(const std::string& n) {
    if (n.empty()) return 0;
    std::vector<Palette>& ps = W3dPalettes();
    for (size_t i = 0; i < ps.size(); i++)
        if (ps[i].name == n) return &ps[i];
    return 0;
}

// INVARIANTE 1 al adoptar: completar las paletas CORTAS para que todas midan
// igual. El color que falta en el indice c se copia de la primera paleta que
// lo tenga (mismo nombre y valor: un default razonable).
static void PalNormalizeLengths() {
    std::vector<Palette>& ps = W3dPalettes();
    size_t maxN = 0;
    for (size_t i = 0; i < ps.size(); i++)
        if (ps[i].colors.size() > maxN) maxN = ps[i].colors.size();
    for (size_t i = 0; i < ps.size(); i++)
        for (size_t c = ps[i].colors.size(); c < maxN; c++) {
            PaletteColor pc;
            pc.name = "Color";
            pc.rgba[0] = pc.rgba[1] = pc.rgba[2] = pc.rgba[3] = 1.0f;
            for (size_t j = 0; j < ps.size(); j++)
                if (c < ps[j].colors.size()) { pc = ps[j].colors[c]; break; }
            ps[i].colors.push_back(pc);
        }
}

void W3dPalettesAdopt(const std::vector<Palette>& newValue) {
    std::vector<Palette>& ps = W3dPalettes();
    for (size_t i = 0; i < newValue.size(); i++) {
        if (ps.size() >= 8) break;                        // tope (reserve del store)
        if (W3dPaletteByName(newValue[i].name)) continue; // merge: la primera gana
        Palette pa;
        pa.name = newValue[i].name;
        ps.push_back(pa);
        // reserve sobre el vector VIVO: los punteros a los rgba quedan estables
        ps.back().colors.reserve(32);
        for (size_t c = 0; c < newValue[i].colors.size() && c < 32; c++)
            ps.back().colors.push_back(newValue[i].colors[c]);
    }
    PalNormalizeLengths();
}

Palette* W3dEffectivePalette(Object* o) {
    for (Object* p = o; p; p = p->Parent) {
        if (!p->palette.empty()) {
            Palette* pa = W3dPaletteByName(p->palette);
            if (pa) return pa;   // nombre roto: se sigue subiendo (tolerante)
        }
        if (p->getType() == ObjectType::ui && p->palette.empty()) {
            // RETROCOMPAT: una raiz UI sin seleccion propia cae a su
            // activePalette de siempre (-1 = deseleccionada -> propio). Si el
            // proyecto tiene una paleta con ese nombre, manda la del proyecto
            // (la copia local del UI puede estar vieja).
            UI* u = (UI*)p;
            if (u->activePalette >= 0 && u->activePalette < (int)u->palettes.size()) {
                Palette* local = &u->palettes[u->activePalette];
                Palette* proj = W3dPaletteByName(local->name);
                return proj ? proj : local;
            }
            return 0;
        }
    }
    return 0;   // nadie eligio: colores propios
}

std::vector<PaletteColor>* W3dEffectiveColors(Object* o) {
    Palette* pa = W3dEffectivePalette(o);
    return pa ? &pa->colors : 0;
}

// nombre libre de COLOR de paleta. Los nombres van por INDICE y son los MISMOS en todas
// las paletas (invariante), asi que el espacio de nombres es la lista de la paleta 0.
// 'excepto' = indice que puede conservar el suyo (-1 = ninguno).
static bool PalColorExists(const std::string& n, void* ctx) {
    int except = *(int*)ctx;
    std::vector<Palette>& ps = W3dPalettes();
    if (ps.empty()) return false;
    for (size_t i = 0; i < ps[0].colors.size(); i++)
        if ((int)i != except && ps[0].colors[i].name == n) return true;
    return false;
}
std::string W3dPaletteFreeNameColor(const std::string& base, int except) {
    return W3dUniqueName(base, "Color", PalColorExists, &except);
}

// INVARIANTE 1: el color nuevo va a TODAS las paletas (mismo nombre y valor)
int W3dPaletteAddColor(const std::string& name, const float* rgba) {
    std::vector<Palette>& ps = W3dPalettes();
    if (ps.empty()) return -1;                     // sin paletas no hay donde
    if (ps[0].colors.size() >= 32) return -1;      // tope del reserve (punteros estables)
    const std::string unique = W3dPaletteFreeNameColor(name, -1);
    for (size_t i = 0; i < ps.size(); i++) {
        PaletteColor pc;
        pc.name = unique;
        for (int q = 0; q < 4; q++) pc.rgba[q] = rgba ? rgba[q] : 1.0f;
        ps[i].colors.push_back(pc);
    }
    return (int)ps[0].colors.size() - 1;
}

// corrige UNA referencia pal* contra el borrado del indice 'idx' (regla 2):
// mayores se corren -1; la que apuntaba a idx pasa a color PROPIO con el
// valor EFECTIVO horneado (nada queda apuntando a memoria basura). Se llama
// ANTES de borrar el color de las paletas (el valor a hornear aun existe).
static void PalFixRef(Object* o, int* pal, float* own, int idx) {
    if (*pal < 0) return;
    if (*pal == idx) {
        std::vector<PaletteColor>* cs = W3dEffectiveColors(o);
        if (cs && idx < (int)cs->size())
            for (int q = 0; q < 4; q++) own[q] = (*cs)[idx].rgba[q];
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
    if (t == ObjectType::text2d) {
        Text2D* tx = (Text2D*)o;
        PalFixRef(o, &tx->palColor, tx->color, idx);
    } else if (t == ObjectType::rect2d) {
        Rect2D* r = (Rect2D*)o;
        PalFixRef(o, &r->palColor, r->color, idx);
    } else if (t == ObjectType::image2d) {
        Image2D* im = (Image2D*)o;
        PalFixRef(o, &im->palTint, im->color, idx);
    } else if (t == ObjectType::slice9) {
        Slice9* s9 = (Slice9*)o;
        PalFixRef(o, &s9->palTint, s9->color, idx);
    } else if (t == ObjectType::button2d) {
        Button2D* b = (Button2D*)o;
        PalFixRef(o, &b->palBackground, b->backgroundColor, idx);
        PalFixRef(o, &b->palText, b->textColor, idx);
        PalFixRef(o, &b->palBorder, b->borderColor, idx);
        PalFixRef(o, &b->palHover, b->colorHover, idx);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalFixRefsRec(o->Childrens[i], idx);
}

void W3dPaletteDeleteColor(int idx) {
    std::vector<Palette>& ps = W3dPalettes();
    if (ps.empty() || idx < 0 || idx >= (int)ps[0].colors.size()) return;
    // regla 2, paso 1: PRIMERO corregir las referencias del proyecto entero
    // (el valor horneado se lee de la paleta efectiva ANTES de borrar)
    PalFixRefsRec(SceneCollection, idx);
    // paso 2: borrar el MISMO indice en TODAS las paletas (invariante 1)
    for (size_t i = 0; i < ps.size(); i++)
        if (idx < (int)ps[i].colors.size())
            ps[i].colors.erase(ps[i].colors.begin() + idx);
    // regla 3: sin colores -> DESELECCIONAR las paletas activas legacy de las
    // raices UI (todo ya quedo en color propio por el paso 1)
    if (!ps.empty() && ps[0].colors.empty() && SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
            Object* o = SceneCollection->Childrens[i];
            if (o && o->getType() == ObjectType::ui) ((UI*)o)->activePalette = -1;
        }
}

// nombre libre de PALETA (espacio GLOBAL de proyecto; el nombre es la CLAVE que usan
// Object::paleta y W3dEffectivePalette). 'excepto' = indice que conserva el suyo.
static bool PalNameExists(const std::string& n, void* ctx) {
    int except = *(int*)ctx;
    std::vector<Palette>& ps = W3dPalettes();
    for (size_t i = 0; i < ps.size(); i++)
        if ((int)i != except && ps[i].name == n) return true;
    return false;
}
std::string W3dPaletteFreeName(const std::string& base, int except) {
    return W3dUniqueName(base, "Palette", PalNameExists, &except);
}

int W3dNewPalette(const std::string& name, int baseIdx) {
    std::vector<Palette>& ps = W3dPalettes();
    if (ps.size() >= 8) return -1;
    Palette pa;
    // antes RECHAZABA el nombre tomado (return -1). Ahora SUFIJA, como todo el resto
    // del editor (pedido del dueno: "ponele el .001 si queres repetirlo").
    pa.name = W3dPaletteFreeName(name, -1);
    ps.push_back(pa);
    ps.back().colors.reserve(32);                  // punteros estables (ver arriba)
    // COPIA de la paleta base (invariante 1: mismos largos y nombres)
    if (baseIdx < 0 || baseIdx >= (int)ps.size() - 1) baseIdx = 0;
    if ((int)ps.size() > 1)
        for (size_t c = 0; c < ps[baseIdx].colors.size(); c++)
            ps.back().colors.push_back(ps[baseIdx].colors[c]);
    return (int)ps.size() - 1;
}

// los objetos que ELEGIAN la paleta borrada vuelven a heredar ("")
static void PalClearSelRec(Object* o, const std::string& name) {
    if (!o) return;
    if (o->palette == name) o->palette.clear();
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalClearSelRec(o->Childrens[i], name);
}

void W3dPaletteDeletePalette(int idx) {
    std::vector<Palette>& ps = W3dPalettes();
    if (idx < 0 || idx >= (int)ps.size()) return;
    std::string name = ps[idx].name;
    ps.erase(ps.begin() + idx);
    PalClearSelRec(SceneCollection, name);
}

static void PalRenameSelRec(Object* o, const std::string& of, const std::string& a) {
    if (!o) return;
    if (o->palette == of) o->palette = a;
    for (size_t i = 0; i < o->Childrens.size(); i++)
        PalRenameSelRec(o->Childrens[i], of, a);
}

void W3dPaletteRename(int idx, const std::string& newValue) {
    std::vector<Palette>& ps = W3dPalettes();
    if (idx < 0 || idx >= (int)ps.size()) return;
    // antes DESCARTABA el rename si el nombre estaba tomado (se perdia en silencio);
    // ahora sufija con LA regla comun. El vinculo object->palette (Object::paleta, por
    // NOMBRE) se arrastra igual que siempre con PalRenameSelRec.
    const std::string unique = W3dPaletteFreeName(newValue, idx);
    if (ps[idx].name == unique) return;
    std::string old = ps[idx].name;
    ps[idx].name = unique;
    PalRenameSelRec(SceneCollection, old, unique);
}

// ---------------------------------------------------------------------------
// ============================================================================
//  LAS CAPAS y LOS MENSAJES (ver W3dScene.h)
// ============================================================================
struct W3dLayer { std::string name; Object* root; unsigned serial; bool es3D; bool paused; };
static std::vector<W3dLayer> gLayers;                 // en el orden en que se cargaron (la ultima, arriba)
enum { LayerLoad = 0, LayerUnload, LayerShow, LayerHide, LayerPause, LayerResume };
struct W3dLayerRequest { int op; std::string name; };
static std::vector<W3dLayerRequest> gLayerRequests;
struct W3dMessageScene { std::string destination, text, of; W3dValueLua data; };
static std::vector<W3dMessageScene> gMessages;
static bool gBasePaused = false;
static Object* (*gLayer3DCreate)(const std::string&) = 0;
static std::string (*gBaseName3D)() = 0;
void W3dSceneSetLayers3D(Object* (*create)(const std::string&), std::string (*baseName)()) {
    gLayer3DCreate = create; gBaseName3D = baseName;
}

static bool LayerLive(int i) {
    return i >= 0 && i < (int)gLayers.size() && W3dLiveObjectSerial(gLayers[(size_t)i].root, gLayers[(size_t)i].serial);
}
static bool IsLayer(Object* o) {
    if (!o) return false;
    for (size_t i = 0; i < gLayers.size(); i++) if (gLayers[i].root == o && LayerLive((int)i)) return true;
    return false;
}
static int LayerOf(Object* o) {
    if (gLayers.empty()) return -1;
    for (Object* p = o; p; p = p->Parent)
        for (size_t i = 0; i < gLayers.size(); i++) if (gLayers[i].root == p && LayerLive((int)i)) return (int)i;
    return -1;
}
static bool LayerRuns(int i) {
    if (!LayerLive(i)) return false;
    const W3dLayer& c = gLayers[(size_t)i];
    return c.root->visible && !c.paused;
}
static int LayerByName(const std::string& n) {
    for (size_t i = 0; i < gLayers.size(); i++) if (gLayers[i].name == n && LayerLive((int)i)) return (int)i;
    return -1;
}
// la BASE: la escena (o el juego) 3D que corre, o la escena 2D activa en un juego solo 2D
static std::string BaseName() {
    if (gBaseName3D) { std::string b = gBaseName3D(); if (!b.empty()) return b; }
    return gActiveEsc ? gActiveEsc->name : std::string();
}
int     W3dSceneLayers() { return (int)gLayers.size(); }
Object* W3dSceneLayerRoot(int i) { return LayerLive(i) ? gLayers[(size_t)i].root : 0; }
bool    W3dSceneLayerEs3D(int i) { return LayerLive(i) && gLayers[(size_t)i].es3D; }
bool    W3dSceneIsLayerRoot(Object* o) { return IsLayer(o); }
bool    W3dSceneInLayer(Object* o) { return LayerOf(o) >= 0; }
bool    W3dSceneBasePaused() { return gBasePaused; }
std::string W3dSceneOf(Object* o) {
    const int c = LayerOf(o);
    if (c >= 0) return gLayers[(size_t)c].name;
    for (Object* p = o; p; p = p->Parent)
        if (p->getType() == ObjectType::ui && p != (Object*)gActiveEsc) return p->name;   // una escena 2D (no la base)
    return BaseName();
}
void W3dSceneLayersDetach(bool withUI, std::vector<Object*>& loose) {
    loose.clear();
    for (size_t i = 0; i < gLayers.size(); i++) {
        if (!LayerLive((int)i)) continue;
        Object* o = gLayers[i].root;
        if (!withUI && !gLayers[i].es3D) continue;
        if (o->Parent) {
            std::vector<Object*>& ch = o->Parent->Childrens;
            for (size_t k = 0; k < ch.size(); k++) if (ch[k] == o) { ch.erase(ch.begin() + (long)k); break; }
            o->Parent = 0;
        }
        loose.push_back(o);
    }
}
void W3dSceneLayersHang(Object* root, const std::vector<Object*>& loose) {
    if (!root) return;
    for (size_t i = 0; i < loose.size(); i++) {
        if (!loose[i] || loose[i]->Parent) continue;
        loose[i]->Parent = root;
        root->Childrens.push_back(loose[i]);
    }
}
bool W3dSceneLayerInsideOf(Object* root) {
    for (size_t i = 0; i < gLayers.size(); i++) {
        if (!LayerLive((int)i)) continue;
        for (Object* p = gLayers[i].root; p; p = p->Parent) if (p == root) return true;
    }
    return false;
}
void W3dSceneLayersClear() {
    for (size_t i = 0; i < gLayers.size(); i++)
        if (LayerLive((int)i) && !gLayers[i].es3D) gLayers[i].root->visible = true;   // (en edicion las UI se ven)
    gLayers.clear();
    gLayerRequests.clear();
    gMessages.clear();
    gBasePaused = false;
}

// los scripts de un subarbol (los de una escena 2D que se descarga)
static void UnloadScriptsOf(Object* o) {
    if (!o) return;
    if (o->scriptData && !o->scriptData->scripts.empty()) W3dScriptUnloadFrom(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) UnloadScriptsOf(o->Childrens[i]);
}
static void LayersApply() {
    if (gLayerRequests.empty()) return;
    std::vector<W3dLayerRequest> list;
    list.swap(gLayerRequests);
    for (size_t k = 0; k < list.size(); k++) {
        const std::string& n = list[k].name;
        const int i = LayerByName(n);
        switch (list[k].op) {
            case LayerLoad: {
                if (i >= 0) break;                                    // ya esta cargada
                if (n == BaseName()) { w3dLogfW("[escenas] cargarEscena('%s'): es la escena que corre", n.c_str()); break; }
                W3dLayer c; c.name = n; c.root = 0; c.serial = 0; c.es3D = false; c.paused = false;
                UI* u = W3dSceneFind(n);
                if (u) {
                    c.root = u; c.serial = u->serial;
                    gLayers.push_back(c);
                    u->visible = true;
                    if (gInited.find(u) == gInited.end() || !gInited[u]) {
                        if (gInitFn) gInitFn(u, false);
                        gInited[u] = true;
                    }
                } else if (gLayer3DCreate) {
                    // (se anota ANTES de crearla: lo que su start() pida ya la ve como capa)
                    Object* o = gLayer3DCreate(n);
                    if (o) { c.root = o; c.serial = o->serial; c.es3D = true; gLayers.push_back(c); }
                }
                if (c.root) w3dLogf("[escenas] '%s' cargada encima (%s)", n.c_str(), c.es3D ? "3D" : "2D");
                else w3dLogfW("[escenas] cargarEscena('%s'): no hay ninguna escena con ese nombre", n.c_str());
                break;
            }
            case LayerUnload: {
                if (i < 0) { w3dLogfW("[escenas] descargarEscena('%s'): no esta cargada encima", n.c_str()); break; }
                W3dLayer c = gLayers[(size_t)i];
                gLayers.erase(gLayers.begin() + i);
                if (c.es3D) W3dScriptDestroyNow(c.root);
                else {
                    UnloadScriptsOf(c.root);
                    c.root->visible = false;
                    gInited.erase((UI*)c.root);
                }
                w3dLogf("[escenas] '%s' descargada", n.c_str());
                break;
            }
            case LayerShow: case LayerHide:
                if (i >= 0) gLayers[(size_t)i].root->visible = (list[k].op == LayerShow);
                else w3dLogfW("[escenas] %s('%s'): solo una escena cargada encima", list[k].op == LayerShow ?
                              "showScene" : "hideScene", n.c_str());
                break;
            case LayerPause: case LayerResume: {
                const bool p = (list[k].op == LayerPause);
                if (i >= 0) gLayers[(size_t)i].paused = p;
                else if (n == BaseName()) gBasePaused = p;
                else w3dLogfW("[escenas] %s('%s'): no esta cargada", p ? "pauseScene" : "resumeScene", n.c_str());
                break;
            }
        }
    }
}

// a cada script del subarbol: onReceive(texto, datos, de). 'noLayers': sin entrar a las capas (la base)
static void DeliverIn(Object* o, const W3dMessageScene& m, bool noLayers, std::set<Object*>& seen) {
    if (!o || seen.count(o)) return;
    if (noLayers && IsLayer(o)) return;
    seen.insert(o);
    if (o->scriptData && !o->scriptData->scripts.empty())
        W3dScriptCallWithData(o, "onReceive", m.text, m.data, m.of);
    for (size_t i = 0; i < o->Childrens.size(); i++) DeliverIn(o->Childrens[i], m, noLayers, seen);
}
static void LayersDeliverMessages() {
    if (gMessages.empty()) return;
    std::vector<W3dMessageScene> list;
    list.swap(gMessages);   // (lo que alRecibir mande va al frame siguiente)
    for (size_t k = 0; k < list.size(); k++) {
        const W3dMessageScene& m = list[k];
        std::set<Object*> seen;
        if (m.destination.empty() || m.destination == "*") {                 // a todas
            DeliverIn(SceneCollection, m, false, seen);
            for (size_t i = 0; i < gLayers.size(); i++) if (LayerLive((int)i)) DeliverIn(gLayers[i].root, m, false, seen);
            continue;
        }
        const int c = LayerByName(m.destination);
        if (c >= 0) { DeliverIn(gLayers[(size_t)c].root, m, false, seen); continue; }
        if (m.destination == BaseName()) { DeliverIn(SceneCollection, m, true, seen); continue; }
        UI* u = W3dSceneFind(m.destination);
        if (u) { DeliverIn(u, m, false, seen); continue; }
        w3dLogfW("[escenas] enviar('%s', '%s'): no hay ninguna escena cargada con ese nombre", m.destination.c_str(),
                 m.text.c_str());
    }
}

// ---- los binds de lua ----
static void RequestLayer(lua_State* L, int op) {
    const char* n = lua_tostring(L, 1);
    if (!n || !*n) return;
    W3dLayerRequest p; p.op = op; p.name = n;
    gLayerRequests.push_back(p);
}
void W3dSceneRequestLoad(const std::string& name) {
    if (name.empty()) return;
    W3dLayerRequest p; p.op = LayerLoad; p.name = name;
    gLayerRequests.push_back(p);
}
static int LLoadScene(lua_State* L)    { RequestLayer(L, LayerLoad); return 0; }
static int LUnloadScene(lua_State* L) { RequestLayer(L, LayerUnload); return 0; }
static int LShowScene(lua_State* L)   { RequestLayer(L, LayerShow); return 0; }
static int LHideScene(lua_State* L)   { RequestLayer(L, LayerHide); return 0; }
static int LPauseScene(lua_State* L)    { RequestLayer(L, LayerPause); return 0; }
static int LResumeScene(lua_State* L)  { RequestLayer(L, LayerResume); return 0; }
static int LLoadedScene(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    lua_pushboolean(L, n && (LayerByName(n) >= 0 || BaseName() == n) ? 1 : 0);
    return 1;
}
// send(escena, mensaje [, datos]): escena nil o "*" = a todas
static int LSend(lua_State* L) {
    W3dMessageScene m;
    const char* d = lua_tostring(L, 1);
    const char* t = lua_tostring(L, 2);
    if (!t) return 0;
    m.destination = d ? d : "";
    m.text = t;
    if (lua_gettop(L) >= 3) m.data = W3dValueLuaRead(L, 3);
    lua_getfield(L, LUA_REGISTRYINDEX, "w3d_owner");
    Object* yo = lua_islightuserdata(L, -1) ? (Object*)lua_touserdata(L, -1) : 0;
    lua_pop(L, 1);
    m.of = yo ? W3dSceneOf(yo) : std::string();
    gMessages.push_back(m);
    return 0;
}
//  BIND lua: changeScene(nombre[, reiniciar]).
// ---------------------------------------------------------------------------
static int LChangeScene(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    bool re = lua_toboolean(L, 2) != 0;
    if (n) W3dSceneRequestChange(n, re);
    return 0;
}
// ---------------------------------------------------------------------------
//  BINDS lua de las CINEMATICAS (ver W3dScene.h / W3dRoots.h):
//    playScene("Cinematica 1" [, "alTerminar"]) -> true si la escena existe: se muestra esa
//        escena 3D (sus luces, camaras, objetos y su animacion de Inicio a Fin) con el juego en pausa
//        y al terminar se vuelve al juego tal cual estaba; ahi se llama onFinish() del script que la
//        pidio. Se aplica al final del frame, como changeScene().
//    stopScene3D()          corta la cinematica (se vuelve al juego y se llama alTerminar igual)
//    scenePlaying()    el nombre de la escena que se esta reproduciendo (nil si ninguna)
// ---------------------------------------------------------------------------
static bool (*gCinematicPlay)(const std::string&, Object*, const std::string&) = 0;
static void (*gCinematicStop)() = 0;
static const char* (*gCurrentCinematic)() = 0;
void W3dSceneSetCinematic(bool (*play)(const std::string&, Object*, const std::string&), void (*stop)(),
                      const char* (*current)()) {
    gCinematicPlay = play; gCinematicStop = stop; gCurrentCinematic = current;
}
static int LPlayScene(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    const char* fn = lua_tostring(L, 2);
    // el objeto del script que la pide (el mismo registro que self()): a sus scripts se les llama 'fn'
    lua_getfield(L, LUA_REGISTRYINDEX, "w3d_owner");
    Object* caller = lua_islightuserdata(L, -1) ? (Object*)lua_touserdata(L, -1) : 0;
    lua_pop(L, 1);
    const bool ok = n && gCinematicPlay && gCinematicPlay(n, caller, fn ? std::string(fn) : std::string());
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}
static int LStopScene3D(lua_State* L) {
    (void)L;
    if (gCinematicStop) gCinematicStop();
    return 0;
}
static int LScenePlaying(lua_State* L) {
    const char* a = gCurrentCinematic ? gCurrentCinematic() : 0;
    if (a && *a) lua_pushstring(L, a); else lua_pushnil(L);
    return 1;
}
// draw3D(on): prende/apaga el pase 3D entero (ver W3dScene.h). Sin argumento devuelve el estado.
static int LDraw3D(lua_State* L) {
    if (lua_gettop(L) >= 1) gDraw3D = lua_toboolean(L, 1) != 0;
    lua_pushboolean(L, gDraw3D ? 1 : 0);
    return 1;
}
void W3dSceneRegisterBind(void* Lv) {
    lua_State* L = (lua_State*)Lv;
    lua_pushcfunction(L, LChangeScene); lua_setglobal(L, "changeScene");
    lua_pushcfunction(L, LPlayScene);    lua_setglobal(L, "playScene");
    lua_pushcfunction(L, LStopScene3D);       lua_setglobal(L, "stopScene3D");
    lua_pushcfunction(L, LScenePlaying); lua_setglobal(L, "scenePlaying");
    lua_pushcfunction(L, LDraw3D);           lua_setglobal(L, "draw3D");
    // las capas y los mensajes entre escenas
    lua_pushcfunction(L, LLoadScene);        lua_setglobal(L, "loadScene");
    lua_pushcfunction(L, LUnloadScene);     lua_setglobal(L, "unloadScene");
    lua_pushcfunction(L, LShowScene);       lua_setglobal(L, "showScene");
    lua_pushcfunction(L, LHideScene);       lua_setglobal(L, "hideScene");
    lua_pushcfunction(L, LPauseScene);        lua_setglobal(L, "pauseScene");
    lua_pushcfunction(L, LResumeScene);      lua_setglobal(L, "resumeScene");
    lua_pushcfunction(L, LLoadedScene);       lua_setglobal(L, "sceneLoaded");
    lua_pushcfunction(L, LSend);              lua_setglobal(L, "send");
}
