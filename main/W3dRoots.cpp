// ============================================================================
//  W3dRoots.cpp — ver W3dRoots.h. Las escenas 3D, los juegos y los prefabs de un
//  proyecto como RAICES: el registro, el tipo de cada una, el CONTEXTO de cada raiz
//  (lo que se guarda mientras no es la activa), el cambio de raiz, la carga perezosa
//  y el cambiarEscena() de escenas/juegos 3D. Compila en el editor y en el runtime 3D.
//  C++03.
// ============================================================================
#include "W3dRoots.h"
#include "W3dScene.h"                   // cambiarEscena(): el pedido de una escena 3D llega por aca
#include "objects/Objects.h"
#include "objects/Scene.h"
#include "objects/Empty.h"               // el objeto raiz de un prefab nuevo
#include "objects/Light.h"               // las luces son POR RAIZ (el tope de 8 de GL)
#include "objects/Camera.h"              // CameraActive: una por raiz
#include "objects/Armature.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h"         // W3dMeshFolderNormalize: la misma regla de carpetas que los demas recursos
#include "animation/Animation.h"         // animaciones de escena: una lista por raiz
#include "animation/W3dAnimSet.h"        // las vistas de los clips de jerarquia (su clip, su version)
#include "animation/SkeletalAnimation.h" // g_mixScenes / g_animMix: el Mix de escenas (por raiz)
#include "script/W3dScript.h"            // W3dScriptData: una raiz con scripts es un JUEGO (proyectos viejos)
#include "render/RenderOptions.h"        // g_redraw
#include "io/JsonW3d.h"
#include "base/W3dNames.h"
#include "w3dGraphics.h"
#include "w3dlog.h"
#include <set>

// ============================================================================
//  EL CONTEXTO DE UNA RAIZ: lo que es "de la escena que se esta mirando" y vive en
//  globales del editor/Core. Mientras la raiz no es la activa, se guarda aca; al
//  volver se restaura tal cual (seleccion, camara, luces, animaciones).
// ============================================================================
struct W3dContextRoot {
    std::vector<Object*> selects;
    Object* active;
    Object* collection;
    Camera* camera;
    std::vector<SceneAnimation*> scenes;   // SceneAnimations
    std::vector<AnimationObject> curves;   // AnimationObjects (las de la animacion de escena activa)
    int activeScene;                       // ActiveSceneAnim
    int animKind;                          // ActiveAnimKind: el MODO del timeline de esta raiz (Juego, un
                                            // clip, su animacion de escena...); 1/3/4 apuntan a objetos suyos
    Armature* animArm;
    Mesh* animMesh;
    bool isGame;                             // AnimIsGame (el "Juego" de un JUEGO: timeline sin keyframes)
    bool mixMode;                            // g_animMix (el modo Mix es de la raiz que lo tiene armado)
    std::vector<W3dLayerAnim> mix;           // g_mixScenes
    int activeMix;                           // g_mixActiveScene
    std::vector<Light*> lights;              // Lights
    int start, end, fps;                     // StartFrame / EndFrame / AnimFPS
    int frame;                               // CurrentFrame: cada raiz con su cabezal (un juego, en su frame 1)
    W3dContextRoot() : active(0), collection(0), camera(0), activeScene(0), animKind(0), animArm(0),
                        animMesh(0), isGame(false), mixMode(false), activeMix(-1), start(1), end(250), fps(30),
                        frame(1) {}
};

static bool ObjectKind(int k) { return k == 1 || k == 3 || k == 4; }

// mueve las globales al contexto y las deja VACIAS (una raiz sin nada elegido)
static void SaveContext(W3dContextRoot* c) {
    // la vista de un clip de jerarquia que se estaba editando vuelve a su clip (otra raiz lo puede usar)
    W3dClipsViewsSync();
    c->selects.swap(ObjSelects); ObjSelects.clear();
    c->active = ActiveObj; ActiveObj = NULL;
    c->collection = CollectionActive; CollectionActive = NULL;
    c->camera = CameraActive; CameraActive = NULL;
    c->scenes.swap(SceneAnimations); SceneAnimations.clear();
    c->curves.swap(AnimationObjects); AnimationObjects.clear();
    c->activeScene = ActiveSceneAnim; ActiveSceneAnim = 0;
    c->animKind = ActiveAnimKind; c->animArm = ActiveAnimArm; c->animMesh = ActiveAnimMesh;
    c->isGame = AnimIsGame; c->mixMode = g_animMix;
    // el modo del timeline es de ESTA raiz (la que entra pone el suyo: RestoreContext / W3dRootUse)
    ActiveAnimKind = 0; AnimIsGame = false; g_animMix = false;
    ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
    c->mix.swap(g_mixScenes); g_mixScenes.clear();
    c->activeMix = g_mixActiveScene; g_mixActiveScene = -1;
    c->lights.swap(Lights); Lights.clear();
    c->start = StartFrame; c->end = EndFrame; c->fps = AnimFPS;
    c->frame = CurrentFrame;
}
// al reves: el contexto vuelve a las globales (las que habia se pisan: SaveContext las vacio)
static void RestoreContext(W3dContextRoot* c) {
    ObjSelects.swap(c->selects);
    ActiveObj = c->active;
    CollectionActive = c->collection ? c->collection : SceneCollection;
    CameraActive = c->camera;
    SceneAnimations.swap(c->scenes);
    AnimationObjects.swap(c->curves);
    ActiveSceneAnim = c->activeScene;
    // el modo del timeline tal cual se dejo (un clip cuyo objeto se borro mientras tanto vuelve a la
    // animacion de escena: UnlinkContexts le solto el puntero)
    ActiveAnimKind = c->animKind; ActiveAnimArm = c->animArm; ActiveAnimMesh = c->animMesh;
    if (ObjectKind(ActiveAnimKind) && !ActiveAnimArm && !ActiveAnimMesh) ActiveAnimKind = 0;
    AnimIsGame = c->isGame; g_animMix = c->mixMode;
    g_mixScenes.swap(c->mix);
    g_mixActiveScene = c->activeMix;
    Lights.swap(c->lights);
    StartFrame = c->start; EndFrame = c->end; AnimFPS = c->fps;
    CurrentFrame = c->frame;
    InitSceneAnimations();   // (una raiz recien creada no tiene ninguna: nace la "Scene")
    // la vista de un clip que se dejo elegida: si el clip cambio mientras tanto (se edito desde otra raiz,
    // es compartido), se re-ata
    if (W3dClipViewLive(ActiveSceneAnim) &&
        SceneAnimations[(size_t)ActiveSceneAnim]->versionView != SceneAnimations[(size_t)ActiveSceneAnim]->clipHier->version)
        W3dClipViewRead(ActiveSceneAnim);
}
// un contexto que se DESCARTA (la raiz se libera): sus animaciones de escena son suyas
static void DeleteContext(W3dContextRoot* c) {
    if (!c) return;
    for (size_t i = 0; i < c->scenes.size(); i++) delete c->scenes[i];
    delete c;
}

// ============================================================================
//  EL REGISTRO
// ============================================================================
static std::vector<W3dRootRow> gRows;
static int gActive = -1;
static std::string gInitial;
// una fila entro (delta +1) o salio (-1) en la posicion 'desde': los indices de fila que se guardan (la
// activa, el cambiarEscena pedido, la cinematica en curso) la siguen. Esta abajo, con el juego.
static void RunIndices(int from, int delta);

// la raiz actual es una Scene? (el editor y el runtime 3D arrancan con una; el runtime 2D no)
static Scene* SceneOf(Object* r) { return (r && scene && (Object*)scene == r) ? scene : NULL; }

static void Ensure() {
    if (!gRows.empty() || !SceneCollection) return;
    W3dRootRow f;
    f.type = W3D_ROOT_SCENE;   // (sin typeExplicit: escena o juego segun tenga scripts, ver W3dRootTypeOf)
    f.name = W3dRootDefaultName(W3D_ROOT_SCENE);
    f.root = SceneCollection;
    f.scene = SceneOf(SceneCollection);
    f.block = true;
    gRows.push_back(f);
    gActive = 0;
    W3dRootRegister(SceneCollection);
}

const std::vector<W3dRootRow>& W3dRoots() { Ensure(); return gRows; }
int W3dActiveRoot() { Ensure(); return gActive; }
Object* W3dRootActiveCamera(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size() || !gRows[(size_t)idx].root) return NULL;
    if (idx == gActive) return (Object*)CameraActive;
    const W3dContextRoot* c = gRows[(size_t)idx].ctx;
    return c ? (Object*)c->camera : NULL;
}
const char* W3dRootDefaultName(int type) {
    return type == W3D_ROOT_PREFAB ? "Prefab" : (type == W3D_ROOT_GAME ? "Game" : "Scene");
}
const char* W3dRootTypeKey(int type) {
    return type == W3D_ROOT_PREFAB ? "prefab" : (type == W3D_ROOT_GAME ? "game" : "scene");
}
int W3dRootTypeByKey(const std::string& c, int defaultValue) {
    if (c == "scene") return W3D_ROOT_SCENE;
    if (c == "game")  return W3D_ROOT_GAME;
    if (c == "prefab") return W3D_ROOT_PREFAB;
    return defaultValue;
}

int W3dRootFind(int type, const std::string& name) {
    Ensure();
    const int cls = W3dRootClass(type);
    for (size_t i = 0; i < gRows.size(); i++)
        if (W3dRootClass(gRows[i].type) == cls && gRows[i].name == name) return (int)i;
    return -1;
}

// ---- EL TIPO ----
static bool HasScripts(Object* o) {
    if (!o) return false;
    if (o->scriptData && !o->scriptData->scripts.empty()) return true;
    for (size_t i = 0; i < o->Childrens.size(); i++) if (HasScripts(o->Childrens[i])) return true;
    return false;
}
int W3dRootTypeDeduced(Object* root) { return HasScripts(root) ? W3D_ROOT_GAME : W3D_ROOT_SCENE; }
// DEDUCIR UNA SOLA VEZ: las escenas sin tipo guardado (un proyecto nuevo, o un .w3d viejo sin registro) toman el de su
// contenido AL ABRIRSE (o al crearse la de fabrica) y desde ahi queda FIJO. Antes se recalculaba en cada consulta, y una
// escena a la que se le agregaba un script pasaba sola a ser un juego (y en el menu dejaba de estar "Convertir en
// juego"). Una escena PUEDE tener scripts (pedido del dueno, 2026-11-11): para animar o simular. El .w3d viejo con
// scripts sigue abriendo como juego, como siempre. Lo llaman OpenProjectNow y W3dCreateDefaultScene.
void W3dRootsSetTypesDeduced() {
    Ensure();
    for (size_t i = 0; i < gRows.size(); i++) {
        W3dRootRow& f = gRows[i];
        if (f.typeExplicit || !W3dRootEs3D(f.type) || !f.root) continue;
        f.type = W3dRootTypeDeduced(f.root);
        f.typeExplicit = true;
    }
}
int W3dRootTypeOf(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return -1;
    W3dRootRow& f = gRows[(size_t)idx];
    // sin tipo explicito y cargada: el de su contenido (y la fila lo recuerda para quien la mire)
    if (!f.typeExplicit && W3dRootEs3D(f.type) && f.root) f.type = W3dRootTypeDeduced(f.root);
    return f.type;
}
int W3dRootActiveType() {
    Ensure();
    return gActive >= 0 ? W3dRootTypeOf(gActive) : W3D_ROOT_SCENE;
}

void W3dRootModeByType(bool force) {
#ifndef W3D_NO_EDITOR
    // (el runtime compilado no tiene timeline: siempre esta jugando)
    const int t = W3dRootActiveType();
    if (t == W3D_ROOT_GAME) {
        if (!force && ActiveAnimKind != 2) return;   // el juego estaba editando uno de sus clips: se respeta
        ActiveAnimKind = 2; AnimIsGame = true; g_animMix = false;
        ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
        StartFrame = 1;
        // sin partida el juego esta en su estado base: su frame 1 (con partida, el cabezal es el tick)
        extern bool SimActive();
        if (!SimActive() || CurrentFrame < StartFrame) CurrentFrame = StartFrame;
    } else if (ActiveAnimKind == 2 || AnimIsGame) {
        // una escena (o un prefab) no simula: su timeline es su animacion de escena
        ActiveAnimKind = 0; AnimIsGame = false;
        InitSceneAnimations();
        AnimLoadActiveRange();
        if (CurrentFrame < StartFrame || CurrentFrame > EndFrame) CurrentFrame = StartFrame;
    }
#else
    (void)force;
#endif
}

bool W3dRootSetType(int idx, int type) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return false;
    W3dRootRow& f = gRows[(size_t)idx];
    if (!W3dRootEs3D(f.type) || !W3dRootEs3D(type)) return false;
    f.type = type;
    f.typeExplicit = true;
    if (idx == gActive) W3dRootModeByType(true);
    return true;
}
int W3dObjectRoot(Object* root) {
    if (!root) return -1;
    Ensure();
    for (size_t i = 0; i < gRows.size(); i++) if (gRows[i].root == root) return (int)i;
    return -1;
}
void W3dRootsInOrder(std::vector<Object*>& out) {
    out.clear();
    Ensure();
    for (size_t i = 0; i < gRows.size(); i++) if (gRows[i].root) out.push_back(gRows[i].root);
    if (out.empty() && SceneCollection) out.push_back(SceneCollection);
}
int W3dRootBlock() {
    Ensure();
    for (size_t i = 0; i < gRows.size(); i++) if (gRows[i].block) return (int)i;
    // se borro la del bloque: la primera escena O JUEGO pasa a serlo (su entrada se descarta al guardar). Un
    // juego tambien: el lector acepta cualquiera de los dos como la del bloque (la fila de "escenas3d" sin
    // entrada), y sin ninguna el guardado escribia la raiz activa en el bloque "escena" Y en su entrada (al
    // reabrir aparecia una escena "Scene" fantasma con sus objetos repetidos, y el juego arrancaba en ella)
    for (size_t i = 0; i < gRows.size(); i++)
        if (W3dRootEs3D(gRows[i].type)) {
            gRows[i].block = true;
            if (gInitial == gRows[i].name) gInitial.clear();   // (la del bloque ES la inicial por defecto)
            return (int)i;
        }
    return -1;
}

std::string W3dRootFreeName(int type, const std::string& base, int except) {
    std::vector<std::string> taken;
    const int cls = W3dRootClass(type);
    for (size_t i = 0; i < gRows.size(); i++)
        if (W3dRootClass(gRows[i].type) == cls && (int)i != except) taken.push_back(gRows[i].name);
    const char* def = W3dRootDefaultName(type);
    return W3dUniqueNameInValues(W3dNameNormalize(base, def), def, taken, -1);
}

bool W3dRootsHasRegistry() {
    if (gRows.empty()) return false;
    if (gRows.size() > 1) return true;
    const W3dRootRow& f = gRows[0];
    if (!W3dRootEs3D(f.type) || f.name != W3dRootDefaultName(W3D_ROOT_SCENE) ||
        !f.folder.empty() || !gInitial.empty()) return true;
    // su tipo hace falta escribirlo si NO es el que se deduciria al abrir, o si el archivo ya lo traia (ver typeInFile)
    return f.typeExplicit && (f.typeInFile || (f.root && f.type != W3dRootTypeDeduced(f.root)));
}

const std::string& W3dInitialRoot() { return gInitial; }
bool W3dRootSetInitial(const std::string& name) {
    if (name.empty()) { gInitial.clear(); return true; }
    const int i = W3dRootFind(W3D_ROOT_SCENE, name);
    if (i < 0) return false;
    // la del bloque ES la inicial por defecto: no hace falta nombrarla
    gInitial = gRows[(size_t)i].block ? std::string() : gRows[(size_t)i].name;
    return true;
}
int W3dInitialRootIdx() {
    const int i = gInitial.empty() ? -1 : W3dRootFind(W3D_ROOT_SCENE, gInitial);
    return i >= 0 ? i : W3dRootBlock();
}

int W3dRootObjects(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size() || !gRows[(size_t)idx].root) return -1;
    std::vector<Object*> stack(1, gRows[(size_t)idx].root);
    int n = -1;   // (la raiz misma no cuenta)
    while (!stack.empty()) {
        Object* o = stack.back(); stack.pop_back(); n++;
        for (size_t i = 0; i < o->Childrens.size(); i++) stack.push_back(o->Childrens[i]);
    }
    return n;
}

void W3dRootsListsAnim(std::vector<std::vector<SceneAnimation*>*>& out) {
    out.clear();
    out.push_back(&SceneAnimations);
    for (size_t i = 0; i < gRows.size(); i++)
        if (gRows[i].ctx && (int)i != gActive) out.push_back(&gRows[i].ctx->scenes);
}

// ============================================================================
//  CAMBIO DE RAIZ
// ============================================================================
bool W3dRootUse(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return false;
    if (idx == gActive) return true;
    W3dRootRow& f = gRows[(size_t)idx];
    if (!f.root) return false;
    W3dContextRoot* c = new W3dContextRoot();
    SaveContext(c);
    if (gActive >= 0 && gActive < (int)gRows.size()) gRows[(size_t)gActive].ctx = c;
    else DeleteContext(c);
    gActive = idx;
    SceneCollection = f.root;
    if (f.scene) scene = f.scene;
    if (f.ctx) {
        RestoreContext(f.ctx);
        DeleteContext(f.ctx);   // (vacio: sus vectores ya volvieron a las globales)
        f.ctx = NULL;
        // (tal cual: el guardado y la carga usan este swap para ir y volver. El cambio "de verdad",
        //  W3dRootChangeActive, ademas acomoda el modo del timeline al tipo)
    } else {
        // una raiz que nunca fue la activa (recien creada o cargada): su coleccion activa es ella
        // misma y el timeline arranca en el modo de su tipo (un juego en "Juego", una escena en su
        // animacion de escena "Scene")
        CollectionActive = SceneCollection;
        InitSceneAnimations();
        AnimLoadActiveRange();
        CurrentFrame = StartFrame;
        W3dRootModeByType(true);
    }
    W3dAnimCurvesInvalidate();   // (las curvas de la otra raiz: el editor vuelve a posar aunque el frame sea el mismo)
    return true;
}

// las luces GL que dejo encendidas la raiz anterior: la nueva enciende las suyas al dibujar
static void TurnOffLightsGL() {
    for (int i = 0; i < MAX_LIGHTS; i++) w3dEngine::SetLightEnabled(GL_LIGHT0 + (unsigned int)i, false);
}

bool W3dRootChangeActive(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return false;
    if (idx == gActive) return true;
    if (!gRows[(size_t)idx].root && !W3dRootLoad(idx)) return false;
    if (!W3dRootUse(idx)) return false;
    // una escena que quedo en "Juego" (la visito el Play de otra raiz) vuelve a su animacion; un juego
    // que se dejo editando un clip lo sigue editando
    W3dRootModeByType(false);
    TurnOffLightsGL();
    g_redraw = true;
    return true;
}

// ============================================================================
//  CARGA PEREZOSA
// ============================================================================
bool W3dRootLoad(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return false;
    if (gRows[(size_t)idx].root) return true;
    Scene* r = W3dSceneNewRoot();
    W3dRootRegister(r);
    gRows[(size_t)idx].root = r;
    gRows[(size_t)idx].scene = r;
    const int prev = gActive;
    const unsigned int selSerial = W3dSelectionSerial;   // (cargar no es ELEGIR: ver W3dSelectionSerial)
    W3dRootUse(idx);                        // la carga arma el arbol sobre SceneCollection
    const bool ok = W3dRootReadContent(gRows[(size_t)idx]);
    gRows[(size_t)idx].loadFailed = !ok;
    DeselectAll(); ActiveObj = NULL;   // (los constructores fueron seleccionando)
    if (prev >= 0) W3dRootUse(prev);
    W3dSelectionSerial = selSerial;          // (el recurso activo de la biblioteca sigue siendo el activo)
    w3dLogf("[raices] %s '%s' cargada desde '%s'%s", W3dRootTypeKey(W3dRootTypeOf(idx)),
            gRows[(size_t)idx].name.c_str(), gRows[(size_t)idx].entry.c_str(), ok ? "" : " (NO SE PUDO LEER)");
    return true;   // la raiz existe (vacia si no se pudo leer: loadFailed frena el guardado encima)
}

int W3dRootsLoadAll(std::vector<int>* loaded) {
    Ensure();
    if (loaded) loaded->clear();
    int n = 0;
    for (size_t i = 0; i < gRows.size(); i++)
        if (!gRows[i].root) {
            W3dRootLoad((int)i);
            n++;
            if (loaded) loaded->push_back((int)i);
        }
    return n;
}

// ============================================================================
//  ALTA / BAJA / NOMBRE / CARPETA
// ============================================================================
int W3dNewRoot(int type, const std::string& name) {
    Ensure();
    W3dRootRow f;
    f.type = (type == W3D_ROOT_PREFAB || type == W3D_ROOT_GAME) ? type : W3D_ROOT_SCENE;
    f.typeExplicit = true;   // nacio con "New Scene" / "New Game" / "New Prefab": es lo que el usuario pidio
    f.name   = W3dRootFreeName(f.type, name, -1);
    Scene* r = W3dSceneNewRoot();
    W3dRootRegister(r);
    f.root = r;
    f.scene = r;
    // EN EL ORDEN DEL PROYECTO (ver W3dRoots.h): una escena o un juego va despues de las escenas y juegos
    // que ya hay y ANTES de los prefabs. Al final de todo, un prefab creado antes que una escena cambiaba de
    // lugar al reabrir y el re-guardado recorria las raices en otro orden (el registro "mallas" salia distinto)
    int idx = (int)gRows.size();
    if (f.type != W3D_ROOT_PREFAB)
        for (size_t i = 0; i < gRows.size(); i++) if (gRows[i].type == W3D_ROOT_PREFAB) { idx = (int)i; break; }
    gRows.insert(gRows.begin() + idx, f);
    RunIndices(idx, +1);
    if (f.type == W3D_ROOT_PREFAB) {
        // el OBJETO RAIZ del prefab (lo que una instancia va a generar): un vacio con su nombre,
        // creado ADENTRO de la raiz nueva (el constructor se cuelga de SceneCollection)
        const int prev = gActive;
        W3dRootUse(idx);
        const bool nc = W3dNamesLoading;
        W3dNamesLoading = true;           // el nombre tal cual (es el unico objeto de la raiz)
        Empty* e = new Empty(NULL, Vector3(0, 0, 0));
        W3dNamesLoading = nc;
        e->name = gRows[(size_t)idx].name;
        CollectionActive = e;                // lo que se importe va adentro del prefab
        if (prev >= 0) W3dRootUse(prev);
    }
    return idx;
}

bool W3dRootRename(int idx, const std::string& newValue, std::string* final) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return false;
    W3dRootRow& f = gRows[(size_t)idx];
    const std::string n = W3dRootFreeName(f.type, newValue, idx);
    if (final) *final = n;
    if (n == f.name) return true;
    if (W3dRootEs3D(f.type) && gInitial == f.name) gInitial = n;   // la inicial la sigue
    if (W3dRootEs3D(f.type)) { extern void W3dOpenRootsRename(const std::string&, const std::string&);
                               W3dOpenRootsRename(f.name, n); }   // (y la lista de abiertas)
    f.name = n;
    return true;
}

bool W3dRootSetFolder(int idx, const std::string& folder) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return false;
    gRows[(size_t)idx].folder = folder;
    return true;
}

// libera el arbol de una raiz que NO es la activa. Se olvida del registro ANTES (asi ~Object no
// la recorre) y se destruye con ella como SceneCollection: cada ~Object recorre solo lo que se va.
static void FreeRoot(W3dRootRow& f) {
    DeleteContext(f.ctx); f.ctx = NULL;
    if (!f.root) return;
    Object* r = f.root;
    f.root = NULL; f.scene = NULL;
    W3dRootForget(r);
    Object* active = SceneCollection;
    SceneCollection = r;
    W3dFreeSubtree(r);
    SceneCollection = active;
}

// las reglas de un borrado (W3dRootDelete y W3dRootRemove): no la activa ni la ultima escena/juego
static bool CanDelete(int idx, std::string* reason) {
    if (idx < 0 || idx >= (int)gRows.size()) { if (reason) *reason = "It doesn't exist anymore"; return false; }
    if (idx == gActive) { if (reason) *reason = "It is open: switch to another scene first"; return false; }
    if (W3dCinematicActive() && (idx == W3dCinematicScene() || idx == W3dCinematicGame())) {
        if (reason) *reason = "Stop the game first";
        return false;
    }
    if (W3dRootEs3D(gRows[(size_t)idx].type)) {
        int scenes = 0;   // (escenas y juegos: el proyecto necesita al menos una raiz 3D)
        for (size_t i = 0; i < gRows.size(); i++) if (W3dRootEs3D(gRows[i].type)) scenes++;
        if (scenes <= 1) { if (reason) *reason = "A project needs at least one scene"; return false; }
    }
    return true;
}

bool W3dRootDelete(int idx, std::string* reason) {
    Ensure();
    if (!CanDelete(idx, reason)) return false;
    // (sobre la fila del registro, no una copia: ~Object mira los contextos mientras se libera)
    FreeRoot(gRows[(size_t)idx]);
    const W3dRootRow f = gRows[(size_t)idx];
    if (W3dRootEs3D(f.type) && gInitial == f.name) gInitial.clear();
    gRows.erase(gRows.begin() + idx);
    RunIndices(idx, -1);
    if (f.block) W3dRootBlock();   // otra escena pasa a ser la del bloque
    w3dLogf("[raices] %s '%s' borrada", W3dRootTypeKey(f.type), f.name.c_str());
    return true;
}

bool W3dRootRemove(int idx, W3dRemovedRoot* out, std::string* reason) {
    Ensure();
    if (!out || !CanDelete(idx, reason)) return false;
    if (!gRows[(size_t)idx].root) W3dRootLoad(idx);   // (el Ctrl+Z la devuelve de memoria)
    out->row = gRows[(size_t)idx];
    out->pos = idx;
    out->initial = W3dRootEs3D(out->row.type) && !gInitial.empty() && gInitial == out->row.name;
    if (out->initial) gInitial.clear();
    gRows.erase(gRows.begin() + idx);
    RunIndices(idx, -1);
    if (out->row.block) W3dRootBlock();   // otra escena pasa a ser la del bloque (hasta el Ctrl+Z)
    // (su arbol sigue REGISTRADO: ver W3dRoots.h)
    w3dLogf("[raices] %s '%s' borrada (con undo)", W3dRootTypeKey(out->row.type), out->row.name.c_str());
    return true;
}

bool W3dRootReturn(W3dRemovedRoot& s) {
    Ensure();
    if (s.pos < 0) return false;
    W3dRootRow f = s.row;
    // (entre el borrado y el Ctrl+Z nadie puede tomar su nombre sin cambiar de raiz -lo que vacia el
    //  historial-, pero dos filas con el mismo nombre no pueden quedar)
    f.name = W3dRootFreeName(f.type, f.name, -1);
    // vuelve a ser la del bloque: la que se promovio al sacarla deja de serlo
    if (f.block) for (size_t i = 0; i < gRows.size(); i++) gRows[i].block = false;
    // su lugar, acotado al orden del proyecto (escenas y juegos antes que los prefabs)
    int pos = s.pos > (int)gRows.size() ? (int)gRows.size() : s.pos;
    if (f.type == W3D_ROOT_PREFAB) { while (pos < (int)gRows.size() && gRows[(size_t)pos].type != W3D_ROOT_PREFAB) pos++; }
    else { while (pos > 0 && gRows[(size_t)(pos - 1)].type == W3D_ROOT_PREFAB) pos--; }
    gRows.insert(gRows.begin() + pos, f);
    RunIndices(pos, +1);
    if (f.root) W3dRootRegister(f.root);
    if (s.initial && !f.block) gInitial = f.name;
    w3dLogf("[raices] %s '%s' vuelve (Ctrl+Z)", W3dRootTypeKey(f.type), f.name.c_str());
    s.row = W3dRootRow(); s.pos = -1; s.initial = false;   // (ya no es de quien la saco)
    return true;
}

void W3dRemovedRootFree(W3dRemovedRoot& s) {
    if (s.pos < 0) return;
    FreeRoot(s.row);
    s.row = W3dRootRow(); s.pos = -1;
}

// ============================================================================
//  PROYECTO
// ============================================================================
void W3dRootsReadJson(JVal* root) {
    // (el cierre del proyecto anterior ya libero las raices y vacio el registro)
    gRows.clear(); gActive = -1; gInitial.clear();
    std::vector<W3dRootRow> rows;
    int block = -1;
    JVal* je = root ? JChild(root, "scenes3d", 5) : NULL;
    if (je)
        for (size_t i = 0; i < je->list.size(); i++) {
            JVal* e = je->list[i];
            if (!e || e->type != 4) continue;
            W3dRootRow f;
            // "tipo": "escena" | "juego". Sin tipo (un registro escrito a mano): el de su contenido
            const std::string tc = JS(e, "type", "");
            f.type = W3dRootTypeByKey(tc, W3D_ROOT_SCENE);
            if (f.type == W3D_ROOT_PREFAB) f.type = W3D_ROOT_SCENE;   // (un prefab no va en esta lista)
            f.typeExplicit = !tc.empty() && W3dRootTypeByKey(tc, -1) >= 0;
            f.typeInFile   = f.typeExplicit;
            f.name = JS(e, "name", "");
            f.entry  = JS(e, "entry", "");
            f.folder = W3dMeshFolderNormalize(JS(e, "folder", ""));
            // la del bloque "escena" es la que NO tiene entrada (la primera, si viene mas de una)
            if (f.entry.empty()) { if (block < 0) { block = (int)rows.size(); f.block = true; } else continue; }
            rows.push_back(f);
        }
    if (block < 0) {
        // proyecto viejo (o un registro sin la del bloque, escrito a mano): el bloque es una escena mas
        W3dRootRow f;
        f.type = W3D_ROOT_SCENE; f.block = true;
        rows.insert(rows.begin(), f);
        block = 0;
    }
    JVal* jp = root ? JChild(root, "prefabs", 5) : NULL;
    if (jp)
        for (size_t i = 0; i < jp->list.size(); i++) {
            JVal* e = jp->list[i];
            if (!e || e->type != 4) continue;
            W3dRootRow f;
            f.type = W3D_ROOT_PREFAB;
            f.typeExplicit = true;
            f.name = JS(e, "name", "");
            f.entry  = JS(e, "entry", "");
            f.folder = W3dMeshFolderNormalize(JS(e, "folder", ""));
            if (f.entry.empty()) continue;   // un prefab sin entrada no tiene de donde salir
            rows.push_back(f);
        }
    // NOMBRES UNICOS por tipo (un archivo escrito a mano puede repetir o dejar vacio)
    for (size_t i = 0; i < rows.size(); i++) {
        gRows.push_back(rows[i]);
        gRows.back().name = W3dRootFreeName(rows[i].type, rows[i].name, (int)gRows.size() - 1);
    }
    gActive = block;
    gRows[(size_t)block].root = SceneCollection;
    gRows[(size_t)block].scene = SceneOf(SceneCollection);
    if (SceneCollection) W3dRootRegister(SceneCollection);
    W3dRootSetInitial(root ? JS(root, "initialScene3d", "") : std::string());
    // las escenas que el editor tenia ABIERTAS (y las del ojo cerrado): se cargan al terminar de abrir el proyecto
    { extern void W3dOpenRootsRead(JVal* root); W3dOpenRootsRead(root); }
    if (gRows.size() > 1)
        w3dLogf("[raices] el proyecto trae %d escena(s)/juego(s)/prefab(s); inicial '%s'", (int)gRows.size(),
                gInitial.empty() ? gRows[(size_t)block].name.c_str() : gInitial.c_str());
}

void W3dRootsCloseProject() {
    // primero los CONTEXTOS (asi ~Object no los recorre objeto por objeto) y despues los arboles
    for (size_t i = 0; i < gRows.size(); i++) { DeleteContext(gRows[i].ctx); gRows[i].ctx = NULL; }
    for (size_t i = 0; i < gRows.size(); i++) {
        if ((int)i == gActive) continue;
        FreeRoot(gRows[i]);
    }
    // la activa sigue siendo SceneCollection (RestartScene la vacia); sale del registro
    if (SceneCollection) W3dRootForget(SceneCollection);
    gRows.clear(); gActive = -1; gInitial.clear();
    { extern void W3dOpenRootsForget(); W3dOpenRootsForget(); }
}

void W3dRootsSaved(const std::vector<std::string>& entries) {
    for (size_t i = 0; i < gRows.size() && i < entries.size(); i++) {
        gRows[i].entry = entries[i];
        gRows[i].loadFailed = false;
    }
}

// ============================================================================
//  EL JUEGO: cargar otra escena EN la raiz activa (runtime compilado)
// ============================================================================
bool W3dRootLoadInActive(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size() || !SceneCollection) return false;
    // lo que apunta al arbol que se va: seleccion, camara, animaciones de escena y el Mix
    DeselectAll(); ObjSelects.clear(); ActiveObj = NULL;
    for (size_t i = 0; i < SceneAnimations.size(); i++) delete SceneAnimations[i];
    SceneAnimations.clear(); AnimationObjects.clear(); ActiveSceneAnim = 0;
    g_mixScenes.clear(); g_mixActiveScene = -1;
    W3dMixScenesForget();
    if (ObjectKind(ActiveAnimKind)) ActiveAnimKind = AnimIsGame ? 2 : 0;
    ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
    // LAS CAPAS (escenas cargadas encima: cargarEscena, W3dScene.h) sobreviven al cambio: se descuelgan antes y se
    // cuelgan arriba de todo en la nueva
    std::vector<Object*> layers;
    W3dSceneLayersDetach(true, layers);
    // la escena actual se DESTRUYE entera (la raiz queda: la pedida se arma en ella)
    while (!SceneCollection->Childrens.empty()) {
        Object* o = SceneCollection->Childrens.back();
        SceneCollection->Childrens.pop_back();
        W3dFreeSubtree(o);
    }
    CollectionActive = SceneCollection;
    TurnOffLightsGL();
    if (gActive >= 0 && gActive < (int)gRows.size() && gActive != idx) {
        gRows[(size_t)gActive].root = NULL; gRows[(size_t)gActive].scene = NULL;
    }
    gRows[(size_t)idx].root = SceneCollection;
    gRows[(size_t)idx].scene = SceneOf(SceneCollection);
    gActive = idx;
    const bool ok = W3dRootReadContent(gRows[(size_t)idx]);
    InitSceneAnimations();
    W3dSceneLayersHang(SceneCollection, layers);
    DeselectAll(); ActiveObj = NULL;
    w3dLogf("[raices] escena '%s' cargada en la raiz activa%s", gRows[(size_t)idx].name.c_str(),
            ok ? "" : " (NO SE PUDO LEER)");
    g_redraw = true;
    return ok;
}

// ============================================================================
//  LAS ESCENAS ABIERTAS EN EL EDITOR (ver W3dRoots.h). Por NOMBRE (las filas corren sus indices al crear o borrar)
// ============================================================================
static std::vector<std::string> gOpen;   // en orden de prioridad
static void JsonEsc(std::string& s, const std::string& t) {
    s += '"';
    for (size_t i = 0; i < t.size(); i++) {
        const char c = t[i];
        if (c == '"' || c == '\\') { s += '\\'; s += c; }
        else if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", (unsigned char)c); s += b; }
        else s += c;
    }
    s += '"';
}
static std::set<std::string> gHidden;       // las abiertas con el ojo cerrado
void W3dOpenRootsForget() { gOpen.clear(); gHidden.clear(); }
void W3dOpenRootsRename(const std::string& of, const std::string& a) {
    for (size_t i = 0; i < gOpen.size(); i++) if (gOpen[i] == of) gOpen[i] = a;
    if (gHidden.erase(of)) gHidden.insert(a);
}
static bool Contains(const std::vector<int>& v, int x) {
    for (size_t i = 0; i < v.size(); i++) if (v[i] == x) return true;
    return false;
}
// la lista DEL USUARIO (las que tildo), sin las que ya no existen
static void OpenValid(std::vector<int>& out) {
    out.clear();
    for (size_t k = 0; k < gOpen.size(); ) {
        const int i = W3dRootFind(W3D_ROOT_SCENE, gOpen[k]);
        if (i < 0 || !W3dRootEs3D(gRows[(size_t)i].type)) { gOpen.erase(gOpen.begin() + (long)k); continue; }
        if (!Contains(out, i)) out.push_back(i);
        k++;
    }
}
static bool ActiveEs3D() {
    return gActive >= 0 && gActive < (int)gRows.size() && W3dRootEs3D(gRows[(size_t)gActive].type);
}
void W3dOpenRoots(std::vector<int>& out) {
    Ensure();
    // sin lista todavia (un proyecto nuevo, o uno de antes de las escenas abiertas): la activa
    if (gOpen.empty() && ActiveEs3D()) gOpen.push_back(gRows[(size_t)gActive].name);
    OpenValid(out);
    // la activa que NO esta en la lista es la unica: un PREFAB (se edita aislado) o la escena a la que paso la PARTIDA
    // (cambiarEscena, una cinematica). No entra a la lista del usuario: el Stop vuelve y la lista sigue como estaba
    if (gActive >= 0 && gActive < (int)gRows.size() && !Contains(out, gActive)) {
        out.clear();
        out.push_back(gActive);
    }
}
bool W3dRootIsOpen(int idx) {
    std::vector<int> v;
    W3dOpenRoots(v);
    return Contains(v, idx);
}
bool W3dRootOpen(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size() || !W3dRootEs3D(gRows[(size_t)idx].type)) return false;
    if (!gRows[(size_t)idx].root && !W3dRootLoad(idx)) return false;
    const std::string& n = gRows[(size_t)idx].name;
    bool is = false;
    for (size_t i = 0; i < gOpen.size(); i++) if (gOpen[i] == n) is = true;
    if (!is) gOpen.push_back(n);
    gRows[(size_t)idx].root->visible = !gHidden.count(n);
    g_redraw = true;
    return true;
}
void W3dRootRemoveFromOpen(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return;
    const std::string& n = gRows[(size_t)idx].name;
    for (size_t i = gOpen.size(); i-- > 0; ) if (gOpen[i] == n) gOpen.erase(gOpen.begin() + (long)i);
    gHidden.erase(n);
    if (gRows[(size_t)idx].root) gRows[(size_t)idx].root->visible = true;   // (cerrada, como recien abierta)
    // la que queda SOLA se ve: con una sola abierta el outliner no muestra su fila (ni su ojo para volver a prenderla)
    std::vector<int> v;
    W3dOpenRoots(v);
    if (v.size() == 1) W3dRootSetOpenVisible(v[0], true);
    g_redraw = true;
}
bool W3dRootMoveOpen(int idx, int pos) {
    std::vector<int> v;
    W3dOpenRoots(v);
    if (!Contains(v, idx) || !W3dRootEs3D(gRows[(size_t)idx].type)) return false;
    const std::string n = gRows[(size_t)idx].name;
    for (size_t i = gOpen.size(); i-- > 0; ) if (gOpen[i] == n) gOpen.erase(gOpen.begin() + (long)i);
    if (pos < 0) pos = 0;
    if (pos > (int)gOpen.size()) pos = (int)gOpen.size();
    gOpen.insert(gOpen.begin() + pos, n);
    g_redraw = true;
    return true;
}
void W3dOtherVisibleRoots(std::vector<Object*>& out) {
    out.clear();
    std::vector<int> v;
    W3dOpenRoots(v);
    for (size_t i = 0; i < v.size(); i++) {
        Object* r = gRows[(size_t)v[i]].root;
        if (r && r != SceneCollection && r->visible) out.push_back(r);
    }
}
bool W3dOpenRootVisible(int idx) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return false;
    if (gRows[(size_t)idx].root) return gRows[(size_t)idx].root->visible;
    return !gHidden.count(gRows[(size_t)idx].name);
}
void W3dRootSetOpenVisible(int idx, bool v) {
    Ensure();
    if (idx < 0 || idx >= (int)gRows.size()) return;
    if (gRows[(size_t)idx].root) gRows[(size_t)idx].root->visible = v;
    if (v) gHidden.erase(gRows[(size_t)idx].name); else gHidden.insert(gRows[(size_t)idx].name);
    g_redraw = true;
}
void W3dOpenRootsRead(JVal* root) {
    gOpen.clear(); gHidden.clear();
    if (!root) return;
    JVal* a = JChild(root, "openScenes", 5);
    if (a) for (size_t i = 0; i < a->list.size(); i++) if (a->list[i] && a->list[i]->type == 2) gOpen.push_back(a->list[i]->str);
    JVal* o = JChild(root, "hiddenScenes", 5);
    if (o) for (size_t i = 0; i < o->list.size(); i++) if (o->list[i] && o->list[i]->type == 2) gHidden.insert(o->list[i]->str);
}
void W3dOpenRootsLoad() {
    const std::vector<std::string> names = gOpen;
    for (size_t k = 0; k < names.size(); k++) {
        const int i = W3dRootFind(W3D_ROOT_SCENE, names[k]);
        if (i >= 0) W3dRootOpen(i);
    }
    // la que se abre (la del bloque, hasta que la sesion elija otra) es de la lista: si no estaba, va adelante
    if (!gOpen.empty() && ActiveEs3D()) {
        const std::string& n = gRows[(size_t)gActive].name;
        bool is = false;
        for (size_t i = 0; i < gOpen.size(); i++) if (gOpen[i] == n) is = true;
        if (!is) gOpen.insert(gOpen.begin(), n);
    }
    // (la activa: su ojo tambien)
    if (gActive >= 0 && gActive < (int)gRows.size() && gRows[(size_t)gActive].root)
        gRows[(size_t)gActive].root->visible = !gHidden.count(gRows[(size_t)gActive].name);
}
void W3dOpenRootsWrite(std::string& s) {
    std::vector<int> v;
    W3dOpenRoots(v);
    OpenValid(v);   // (la lista del usuario: guardando con un prefab activo, las escenas que tenia abiertas)
    // el ojo es el 'visible' de cada raiz: lo que dice ahora (cualquiera lo pudo cambiar)
    for (size_t i = 0; i < v.size(); i++) {
        const W3dRootRow& f = gRows[(size_t)v[i]];
        if (!f.root) continue;
        if (f.root->visible) gHidden.erase(f.name); else gHidden.insert(f.name);
    }
    if (v.size() <= 1 && gHidden.empty()) return;   // (una sola: lo de siempre, nada que guardar)
    s += "  \"openScenes\": [";
    for (size_t i = 0; i < v.size(); i++) { if (i) s += ", "; JsonEsc(s, gRows[(size_t)v[i]].name); }
    s += "],\n";
    if (!gHidden.empty()) {
        s += "  \"hiddenScenes\": [";
        bool first = true;
        for (std::set<std::string>::iterator it = gHidden.begin(); it != gHidden.end(); ++it) {
            if (!first) s += ", ";
            first = false;
            JsonEsc(s, *it);
        }
        s += "],\n";
    }
}

// ============================================================================
//  LAS CAPAS 3D (cargarEscena, W3dScene.h): una escena o un juego 3D del proyecto cargado ENCIMA de la base es
//  una instancia de "escena:<nombre>" (como instanciar(): en el editor sale de la escena en memoria, en el juego
//  compilado de su entrada), con sus scripts arrancados por la partida. La base es la raiz activa
// ============================================================================
static Object* CreateLayer3D(const std::string& name) {
    if (W3dRootFind(W3D_ROOT_SCENE, name) < 0 || !W3dPrefabInstantiateHook) return 0;   // (escenas y juegos)
    W3dPrefabInstantiateParent = 0;   // (en la raiz activa)
    Object* o = W3dPrefabInstantiateHook(("scene:" + name).c_str(), 0, 0.0f);
    if (o) W3dScriptNotifyNewObject(o);   // sus scripts y sus cuerpos: los arranca la partida
    return o;
}
static std::string BaseName3D() {
    Ensure();
    if (gActive >= 0 && gActive < (int)gRows.size()) return gRows[(size_t)gActive].name;
    return SceneCollection ? SceneCollection->name : std::string();
}
struct W3dRootsHookLayers { W3dRootsHookLayers() { W3dSceneSetLayers3D(CreateLayer3D, BaseName3D); } };
static W3dRootsHookLayers gRootsHookLayers;

// ============================================================================
//  cambiarEscena() CON ESCENAS 3D (el pedido llega de W3dEscena, que primero mira las UI)
// ============================================================================
static W3dRootChangeGameFn gChangeGame = 0;
static int gRequest = -1;
void W3dRootsSetChangeGame(W3dRootChangeGameFn fn) { gChangeGame = fn; gRequest = -1; }

static bool RequestScene3D(const std::string& name, bool restart) {
    (void)restart;   // una escena/juego 3D pedido SIEMPRE arranca de cero (como abrir un nivel)
    const int i = W3dRootFind(W3D_ROOT_SCENE, name);   // (escenas y juegos)
    if (i < 0 || !gChangeGame) return false;
    gRequest = i;
    return true;
}
static void CinematicApplyPending();   // (las cinematicas, abajo)
static bool ApplyScene3D() {
    // una cinematica que termino, un pararEscena3D() o un reproducirEscena() pedidos en este frame
    CinematicApplyPending();
    if (gRequest < 0 || !gChangeGame) { gRequest = -1; return false; }
    const int i = gRequest;
    gRequest = -1;
    W3dCinematicAbort();   // (un cambiarEscena en medio de una cinematica: se deja la cinematica sin su alTerminar)
    gChangeGame(i);
    return true;
}

// ---- la CINEMATICA de una ESCENA pedida jugando ----
int W3dRootCinematic() {
    if (SceneAnimations.empty()) return -1;
    // la ANIMACION DE ESCENA elegida (un clip de objeto no es "la" animacion de la raiz: si lo que se
    // estaba editando era uno, la primera animacion de escena)
    if (ActiveSceneAnim >= 0 && ActiveSceneAnim < (int)SceneAnimations.size() && !W3dAnimIsClip(ActiveSceneAnim))
        return ActiveSceneAnim;
    for (size_t i = 0; i < SceneAnimations.size(); i++) if (!W3dAnimIsClip((int)i)) return (int)i;
    return -1;
}
bool W3dRootStartCinematic() {
    if (W3dRootActiveType() != W3D_ROOT_SCENE) return false;   // un juego simula; un prefab no se juega
    const int a = W3dRootCinematic();
    if (a < 0 || !W3dAnimScenePlay(a, false)) return false;
    W3dAnimSceneTick(0.0f);   // el cuadro de inicio ya posado (los scripts de su start() lo ven)
    w3dLogf("[raices] escena '%s': reproduce su animacion '%s' desde el frame %d",
            gActive >= 0 ? gRows[(size_t)gActive].name.c_str() : "?", SceneAnimations[(size_t)a]->name.c_str(),
            SceneAnimations[(size_t)a]->startFrame);
    return true;
}

// ============================================================================
//  CINEMATICAS: reproducirEscena() (ver W3dRoots.h). La ESCENA pasa a ser la raiz
//  activa con W3dRootUse (el juego guarda su contexto y su arbol queda intacto,
//  fuera del arbol activo: nada suyo corre) y vuelve con el mismo swap.
// ============================================================================
namespace {
struct W3dCinematicState {
    bool active;
    int scene, game;            // filas del registro
    Object* caller;             // el objeto cuyo script la pidio (lo suelta ~Object)
    std::string fn;             // su funcion global a llamar al volver ("" = ninguna)
    bool loadedFor;             // la escena no estaba cargada: se cargo para esto (el runtime la libera)
    bool ended;                 // su animacion llego al Fin (se vuelve al final del frame)
    W3dAnimSceneState played;   // la animEscena() que sonaba en el juego (sigue al volver)
    W3dCinematicState() : active(false), scene(-1), game(-1), caller(0), loadedFor(false), ended(false) {}
};
struct W3dCinematicRequest {
    bool has, stop;
    int scene;
    Object* caller;
    std::string fn;
    W3dCinematicRequest() : has(false), stop(false), scene(-1), caller(0) {}
};
}
static W3dCinematicState gCinematic;
static W3dCinematicRequest gCinematicRequest;
static W3dRootCinematicFn gCinematicFn = 0;

void W3dRootsSetCinematic(W3dRootCinematicFn fn) { gCinematicFn = fn; gCinematicRequest = W3dCinematicRequest(); }
bool W3dCinematicActive() { return gCinematic.active; }
int  W3dCinematicScene() { return gCinematic.active ? gCinematic.scene : -1; }
int  W3dCinematicGame()  { return gCinematic.active ? gCinematic.game : -1; }

// la vuelta al juego (con o sin su alTerminar)
static void CinematicReturn(bool call) {
    if (!gCinematic.active) return;
    W3dCinematicState c = gCinematic;
    gCinematic = W3dCinematicState();
    // la ESCENA todavia es la activa: el build descarga sus scripts y la devuelve a como estaba
    if (gCinematicFn) gCinematicFn(W3D_CINEMATIC_EXITS);
    W3dAnimSceneReset();
    // el modo del timeline de la escena: el que tenia (lo que se le forzo para jugarla no es suyo)
    if (c.scene >= 0 && c.scene < (int)gRows.size() && W3dActiveRoot() == c.scene) {
        ActiveAnimKind = 0; AnimIsGame = false;
    }
    if (c.game >= 0 && c.game < (int)gRows.size() && W3dRootUse(c.game)) {
        TurnOffLightsGL();
        W3dAnimSceneRestoreState(c.played);   // la animEscena() del juego sigue donde iba
    } else w3dLogfE("[raices] cinematica: no pude volver al juego (fila %d)", c.game);
#ifdef W3D_NO_EDITOR
    // el juego compilado no guarda lo que no usa: una escena cargada solo para esto se libera
    if (c.loadedFor && c.scene >= 0 && c.scene < (int)gRows.size() && c.scene != gActive)
        FreeRoot(gRows[(size_t)c.scene]);
#endif
    w3dLogf("[raices] cinematica '%s' %s: vuelve el juego '%s'%s%s",
            (c.scene >= 0 && c.scene < (int)gRows.size()) ? gRows[(size_t)c.scene].name.c_str() : "?",
            call ? "terminada" : "cortada",
            (gActive >= 0 && gActive < (int)gRows.size()) ? gRows[(size_t)gActive].name.c_str() : "?",
            (call && c.caller && !c.fn.empty()) ? ", llama " : "", (call && c.caller) ? c.fn.c_str() : "");
    g_redraw = true;
    // alTerminar() en los scripts del objeto que la pidio (ya en el juego: puede pedir la siguiente)
    if (call && c.caller && !c.fn.empty()) W3dScriptEvent(c.caller, c.fn.c_str(), (Object* const*)NULL, 0, NULL, 0);
}

static bool CinematicBegin(int idx, Object* caller, const std::string& fn) {
    const int game = gActive;
    if (idx < 0 || idx >= (int)gRows.size() || idx == game) return false;
    const bool loaded = (gRows[(size_t)idx].root != NULL);
    if (!loaded && !W3dRootLoad(idx)) return false;
    W3dCinematicState c;
    c.scene = idx; c.game = game; c.caller = caller; c.fn = fn; c.loadedFor = !loaded;
    // lo que sonaba en el juego se deja en pausa (se retoma al volver)
    W3dAnimSceneSaveState(&c.played);
    W3dAnimSceneReset();
    if (!W3dRootUse(idx)) { W3dAnimSceneRestoreState(c.played); return false; }
    TurnOffLightsGL();
    // se JUEGA (no se edita): el timeline queda en "Juego" mientras dure (el editor sigue llamando al tick
    // de la partida y los esqueletos reproducen sus clips con su cabezal, como en el juego compilado)
    ActiveAnimKind = 2; AnimIsGame = true;
    ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
    gCinematic = c;
    gCinematic.active = true;
    if (gCinematicFn) gCinematicFn(W3D_CINEMATIC_BEFORE);
    // su animacion de escena, de su Inicio a su Fin (una pasada)
    const int a = W3dRootCinematic();
    if (a >= 0 && W3dAnimScenePlay(a, false)) {
        W3dAnimSceneTick(0.0f);   // el cuadro de inicio ya posado (sus scripts lo ven en start())
        CurrentFrame = W3dAnimSceneFrame();
    }
    if (gCinematicFn) gCinematicFn(W3D_CINEMATIC_ENTERED);
    w3dLogf("[raices] cinematica: el juego '%s' queda en pausa y se reproduce la escena '%s' (%s, frames %d..%d)",
            (game >= 0 && game < (int)gRows.size()) ? gRows[(size_t)game].name.c_str() : "?",
            gRows[(size_t)idx].name.c_str(), a >= 0 ? SceneAnimations[(size_t)a]->name.c_str() : "sin animacion",
            a >= 0 ? SceneAnimations[(size_t)a]->startFrame : 0, a >= 0 ? SceneAnimations[(size_t)a]->endFrame : 0);
    g_redraw = true;
    return true;
}

void W3dCinematicTick(float dt) {
    if (!gCinematic.active) return;
    W3dAnimSceneTick(dt);
    if (W3dAnimSceneFrame() >= 0) CurrentFrame = W3dAnimSceneFrame();   // (el cabezal del timeline la sigue)
    if (W3dAnimSceneEnded()) gCinematic.ended = true;
}

void W3dCinematicAbort() {
    gCinematicRequest = W3dCinematicRequest();
    CinematicReturn(false);
}

// al final del frame (W3dSceneApplyPending): primero la vuelta (termino / pararEscena3D) con su
// alTerminar, y despues un reproducirEscena() pedido (tambien el que pidio ese alTerminar: encadenar)
static void CinematicApplyPending() {
    if (gCinematic.active && (gCinematic.ended || gCinematicRequest.stop)) {
        gCinematicRequest.stop = false;
        CinematicReturn(true);
    }
    gCinematicRequest.stop = false;
    if (gCinematicRequest.has) {
        const W3dCinematicRequest p = gCinematicRequest;
        gCinematicRequest = W3dCinematicRequest();
        // pedida desde ADENTRO de otra cinematica: la anterior se deja (sin su alTerminar)
        if (gCinematic.active) CinematicReturn(false);
        if (!CinematicBegin(p.scene, p.caller, p.fn))
            w3dLogfE("[raices] reproducirEscena: no pude reproducir '%s'",
                     (p.scene >= 0 && p.scene < (int)gRows.size()) ? gRows[(size_t)p.scene].name.c_str() : "?");
    }
}

static void RunIndices(int from, int delta) {
    // salio una fila: la que la nombraba ya no nombra nada (no puede ser la activa ni la de una cinematica:
    // CanDelete); entro una: los de ahi en adelante se corren uno
    int* idx[5] = { &gActive, &gRequest, &gCinematic.scene, &gCinematic.game, &gCinematicRequest.scene };
    for (int k = 0; k < 5; k++) {
        int& i = *idx[k];
        if (i < 0) continue;
        if (delta < 0 && i == from) i = -1;
        else if (i >= from + (delta < 0 ? 1 : 0)) i += delta;
    }
}

int W3dRootsUnload(const std::vector<int>& rows) {
    int n = 0;
    for (size_t k = 0; k < rows.size(); k++) {
        const int i = rows[k];
        if (i < 0 || i >= (int)gRows.size() || i == gActive) continue;
        W3dRootRow& f = gRows[(size_t)i];
        if (!f.root || f.block || f.entry.empty() || f.loadFailed) continue;
        if (W3dCinematicActive() && (i == gCinematic.scene || i == gCinematic.game)) continue;
        FreeRoot(f);
        n++;
    }
    if (n) w3dLogf("[raices] %d raiz(es) que se cargaron para guardar vuelven a quedar sin cargar", n);
    return n;
}

bool W3dRootUnloadFromGame(int idx) {
    if (idx < 0 || idx >= (int)gRows.size() || idx == gActive) return false;
    W3dRootRow& f = gRows[(size_t)idx];
    if (!f.root || f.block || f.entry.empty() || f.loadFailed) return false;
    if (W3dCinematicActive() && (idx == gCinematic.scene || idx == gCinematic.game)) return false;
    FreeRoot(f);
    w3dLogf("[raices] '%s' se descarga: la partida se fue de ahi (vuelve a leerse de '%s')", f.name.c_str(), f.entry.c_str());
    return true;
}

// ---- los binds (W3dEscena los registra; ver W3dSceneSetCinematic) ----
static bool CinematicRequestBind(const std::string& name, Object* caller, const std::string& fn) {
    if (!gCinematicFn) return false;   // (sin partida: el editor fuera del Play)
    const int i = W3dRootFind(W3D_ROOT_SCENE, name);
    if (i < 0) { w3dLogfW("reproducirEscena(): no hay una escena '%s'", name.c_str()); return false; }
    if (W3dRootTypeOf(i) != W3D_ROOT_SCENE) {
        w3dLogfW("reproducirEscena(): '%s' es un juego, no una escena (para ir a otro juego: cambiarEscena)", name.c_str());
        return false;
    }
    const int game = gCinematic.active ? gCinematic.game : gActive;
    if (i == game) { w3dLogfW("reproducirEscena(): '%s' es la raiz que esta corriendo", name.c_str()); return false; }
    gCinematicRequest.has = true; gCinematicRequest.stop = false;
    gCinematicRequest.scene = i; gCinematicRequest.caller = caller; gCinematicRequest.fn = fn;
    return true;
}
static void CinematicStopBind() { if (gCinematic.active) gCinematicRequest.stop = true; }
static const char* CurrentCinematicBind() {
    return (gCinematic.active && gCinematic.scene >= 0 && gCinematic.scene < (int)gRows.size()) ? gRows[(size_t)gCinematic.scene].name.c_str() : "";
}

// ============================================================================
//  ~Object: lo que el contexto de una raiz INACTIVA guarda de un objeto que se borra
//  (la seleccion, la camara, sus luces, las curvas de sus animaciones de escena)
// ============================================================================
static void RemoveFromCurves(std::vector<AnimationObject>& v, Object* o) {
    for (size_t i = v.size(); i-- > 0; ) if (v[i].obj == o) v.erase(v.begin() + (long)i);
}
static void UnlinkContexts(Object* o) {
    for (size_t k = 0; k < gRows.size(); k++) {
        W3dContextRoot* c = gRows[k].ctx;
        if (!c) continue;
        for (size_t i = c->selects.size(); i-- > 0; ) if (c->selects[i] == o) c->selects.erase(c->selects.begin() + (long)i);
        if (c->active == o) c->active = NULL;
        if (c->collection == o) c->collection = NULL;
        if ((Object*)c->camera == o) c->camera = NULL;
        for (size_t i = c->lights.size(); i-- > 0; ) if ((Object*)c->lights[i] == o) c->lights.erase(c->lights.begin() + (long)i);
        if ((Object*)c->animArm == o || (Object*)c->animMesh == o) { c->animArm = NULL; c->animMesh = NULL; c->animKind = 0; }
        RemoveFromCurves(c->curves, o);
        for (size_t e = 0; e < c->scenes.size(); e++) {
            if (!c->scenes[e]) continue;
            RemoveFromCurves(c->scenes[e]->objects, o);
            if (c->scenes[e]->isClip && c->scenes[e]->owner == o) c->scenes[e]->owner = NULL;   // clip huerfano
        }
    }
    // el que pidio una cinematica ya no esta: al volver no hay a quien llamar
    if (gCinematic.caller == o) gCinematic.caller = NULL;
    if (gCinematicRequest.caller == o) gCinematicRequest.caller = NULL;
}

namespace {
struct RegisterHooks {
    RegisterHooks() {
        W3dUnlinkRegister(UnlinkContexts);
        W3dSceneSet3D(RequestScene3D, ApplyScene3D);
        W3dSceneSetCinematic(CinematicRequestBind, CinematicStopBind, CurrentCinematicBind);
    }
} gRegisterHooks;
}
