// ============================================================================
//  Streaming.cpp — ver Streaming.h. Cargar y descargar en tiempo real las
//  instancias de prefab y los proxies DIFERIDOS ("carga": "distancia") segun la
//  distancia a su objetivo, de a tajadas por frame, por el almacen de recursos
//  (pedidos ASYNC + refcount) y por las rutas de instanciar()/destruir() de lua.
//  Compila en el editor y en el runtime 3D (C++03). Motor generico.
// ============================================================================
#include "io/Streaming.h"
#include "io/Prefabs.h"
#include "io/W3dResources.h"
#include "objects/InstancePrefab.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MeshResource.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Camera.h"            // CameraActive: el objetivo por defecto jugando
#include "objects/CameraBase.h"        // g_renderCamPos / g_viewBound: el de la vista previa del editor
#include "animation/W3dAnimSet.h"      // W3dAnimSetsEnsureOps (los animsets se piden antes de que nadie los use)
#include "script/W3dScript.h"          // las rutas de instanciar()/destruir() + el hook de lua + las fuentes
#include "importers/import_obj.h"      // las texturas DORMIDAS de un material (pedirlas, dormirlas otra vez)
#include "animation/Animation.h"      // w3dGetTicks: el reloj del presupuesto del frame (el juego compilado)
#include "render/RenderOptions.h"     // g_redraw: lo que entra o sale se dibuja (el editor dibuja por eventos)
#include "W3dRoots.h"
#include "w3dlog.h"
#include <map>
#include <set>
#include <algorithm>

W3dStreamingConfig g_w3dStreamingConfig = { 2, 2, 4, 0.10f, 4.0f };
bool g_w3dStreamingPreviewView = false;
double (*W3dStreamingClock)() = 0;

namespace {
// las instancias que mira el tick: las DIFERIDAS de la raiz activa (tambien las de adentro de lo generado), las que
// lua FIJO y las que estan sin cargar aunque sean de siempre. Se rearma cuando nacen objetos, cuando cambia la raiz activa o cuando algo la ensucia; una que se libera
// en el medio se reconoce por su serial (la direccion se recicla)
struct Token { InstancePrefab* ip; unsigned serial; };
std::vector<Token> gList;
// UN MUNDO GRANDE se decide POR TAJADAS: cada tick mira a lo sumo kByTick instancias (en ronda: todo el mapa en
// unos pocos ticks; las distancias de carga son de decenas de metros y la camara no las cruza en 0.3 s), y las que
// estan PIDIENDO (la bomba y la generacion) se atienden todos los ticks. Medir las ~800 celdas de Portland en cada
// tick -GetGlobalPosition sube por los padres- era la mayor parte de la logica del juego en el N95 (25-60 ms por tick).
// Una lista chica (los niveles de siempre, las pruebas) se mira entera, como antes
const size_t kByTick = 48;
size_t gCursor = 0;
std::vector<size_t> gRequesting;   // indices en gList de las que estan PIDIENDO (se rearma con la lista)
unsigned gSpawnedList = 0;
Object*  gRootList = NULL;
bool     gListDirty = true;
// las que RETIENEN pedidos (el cierre del proyecto los suelta todos)
std::set<InstancePrefab*> gConstraintRequests;
// las diferidas que el Play del editor SOLTO al arrancar (el Stop las vuelve a generar), con su raiz
struct Released { InstancePrefab* ip; unsigned serial; Object* root; };
std::vector<Released> gReleased;
// el objetivo por defecto FORZADO (el harness)
bool    gForced = false;
Vector3 gForcedPos;
W3dStreamingStats gStats;

typedef std::map<std::pair<Object*, std::string>, Object*> CacheTargets;
}

// ============================================================================
//  LA LISTA
// ============================================================================
static void CollectList(Object* o, std::vector<Token>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dIsInstanceType(h->getType())) {
            InstancePrefab* ip = (InstancePrefab*)h;
            // (tambien una de siempre que esta sin cargar: lua la descargo, o dejo de ser diferida jugando)
            if (ip->load == W3D_LOAD_DISTANCE || ip->pinnedStream != 0 || ip->streamState != W3D_STREAM_LOADED) {
                Token f; f.ip = ip; f.serial = ip->serial;
                out.push_back(f);
            }
        }
        CollectList(h, out);   // (lo generado tambien: una diferida anidada adentro de un prefab)
    }
}
static void Rebuild() {
    const unsigned spawned = W3dObjectsSpawned();
    if (!gListDirty && spawned == gSpawnedList && gRootList == SceneCollection) return;
    gList.clear();
    CollectList(SceneCollection, gList);
    gSpawnedList = spawned;
    gRootList = SceneCollection;
    gListDirty = false;
    // (el cursor de la tajada SIGUE donde estaba: la lista sale en el orden del arbol, el mismo de antes. Volverlo a 0
    //  en cada rearmado -y se rearma con cada generacion- dejaba sin mirar las de indice alto mientras se generaba: una
    //  instancia que la camara pasaba de largo no se pedia nunca. El tick lo acota si la lista se achico)
    gRequesting.clear();
    for (size_t i = 0; i < gList.size(); i++)
        if (gList[i].ip->streamState == W3D_STREAM_REQUESTING) gRequesting.push_back(i);
}

// ============================================================================
//  EL OBJETIVO
// ============================================================================
// el punto por defecto: el forzado; sin partida (la vista previa del editor), la vista que dibuja; jugando, la
// camara activa. false = no hay ninguno (nada contra que medir: todo queda cargado)
static bool PosDefault(Vector3& out) {
    if (gForced) { out = gForcedPos; return true; }
    if (!W3dScriptHasSession() && g_viewBound) { out = g_renderCamPos; return true; }
    if (CameraActive) { out = ((Object*)CameraActive)->GetGlobalPosition(); return true; }
    if (g_viewBound) { out = g_renderCamPos; return true; }
    return false;
}
// el de una instancia: su 'objetivo' por nombre (por SCOPE: desde donde vive la instancia, despues todo el arbol; uno
// por tick y por scope en el cache) o el de defecto si no nombra ninguno o el nombrado no esta
static bool PosTarget(InstancePrefab* ip, CacheTargets& cache, bool hasDef, const Vector3& def, Vector3& out) {
    if (!ip->target.empty()) {
        const std::pair<Object*, std::string> key(W3dInstanceOf(ip), ip->target);
        CacheTargets::iterator it = cache.find(key);
        Object* o = NULL;
        if (it != cache.end()) o = it->second;
        else {
            o = W3dFindNameFrom(ip->Parent ? ip->Parent : (Object*)ip, ip->target);
            cache[key] = o;
        }
        if (o) { out = o->GetGlobalPosition(); return true; }
    }
    if (!hasDef) return false;
    out = def;
    return true;
}
// la quiere cargada? (lua manda; una de siempre, si; una diferida, por la distancia CON histeresis: descargada carga
// a menos de 'distancia', cargada -o pidiendo- se descarga a mas de distancia * (1 + histeresis))
static bool Wants(const InstancePrefab* ip, bool hasPos, float d2) {
    if (ip->pinnedStream > 0) return true;
    if (ip->pinnedStream < 0) return false;
    if (ip->load != W3D_LOAD_DISTANCE || !hasPos) return true;
    float lim = ip->distance;
    if (ip->streamState != W3D_STREAM_UNLOADED) lim *= 1.0f + g_w3dStreamingConfig.hysteresis;
    return d2 <= lim * lim;
}

// ============================================================================
//  LOS PEDIDOS AL ALMACEN
// ============================================================================
// (un pedido cuyo descriptor ya no existe -la biblioteca purgo esa textura, con "Delete anyway"- no se toca: su
//  referencia murio con el)
static void ReleaseOne(const W3dRequestStream& p) {
    if (W3dResourceCurrent(p.r, p.series)) W3dResourceRelease(p.r, W3DREC_PERMANENT);
}
static void Annotate(InstancePrefab* ip, W3dResource* r) {
    W3dRequestStream p; p.r = r; p.series = r->series;
    ip->streamRequests.push_back(p);
}
static void ReleaseRequests(InstancePrefab* ip) {
    std::vector<W3dRequestStream> v;
    v.swap(ip->streamRequests);
    for (size_t i = 0; i < v.size(); i++) ReleaseOne(v[i]);
    gConstraintRequests.erase(ip);
    ip->streamPhase = 0;
}
// las mallas, los animsets y las fuentes de los scripts de su definicion (y de las anidadas que genera con ella)
static void RequestResources(InstancePrefab* ip, int mode) {
    W3dMeshesEnsureOps();
    W3dAnimSetsEnsureOps();
    W3dScriptSourcesEnsureOps();
    std::vector<W3dLoadsItem> items;
    W3dPrefabResources(ip, items, NULL);
    for (size_t i = 0; i < items.size(); i++) {
#ifndef W3D_NO_EDITOR
        // (las FUENTES de los scripts solo se traen antes en el juego compilado, donde no cambian: el editor las lee
        //  frescas -el IDE las edita jugando-, ver RunFile en script/W3dScript.cpp)
        if (items[i].type == W3DREC_SCRIPT) continue;
#endif
        W3dResource* r = W3dResourceAcquire(items[i].type, items[i].id, W3DREC_PERMANENT, mode, "streaming");
        if (r && r->state != W3DREC_FAILED) Annotate(ip, r);   // (un fallo no suma referencia)
    }
    if (!ip->streamRequests.empty()) gConstraintRequests.insert(ip);   // (solo las que retienen algo: ver ReleaseRequests)
    ip->streamPhase = 0;
}
// con las mallas LISTAS: las texturas de los materiales que dibujan (las DORMIDAS, o la base que espera en la cola
// diferida; las que ya estan en una ranura no hacen falta)
static void RequestTextures(InstancePrefab* ip, int mode) {
    std::vector<W3dLoadsItem> items;
    std::vector<MeshResource*> meshes;
    W3dPrefabResources(ip, items, &meshes);
    std::vector<std::string> paths;
    for (size_t i = 0; i < meshes.size(); i++)
        for (size_t k = 0; k < meshes[i]->materials.size(); k++) {
            Material* m = meshes[i]->materials[k];
            if (!m) continue;
            const AsleepTex* d = AsleepTexturesOf(m);
            if (d) {
                paths.push_back(d->base);
                paths.push_back(d->normal);
                for (size_t c = 0; c < d->layers.size(); c++) paths.push_back(d->layers[c].texture);
            } else if (!m->texture) {
                const std::string p = PendingTextureOf(m);
                if (!p.empty()) paths.push_back(p);
            }
        }
    std::set<std::string> views;
    for (size_t i = 0; i < paths.size(); i++) {
        if (paths[i].empty() || !views.insert(paths[i]).second) continue;
        W3dResource* r = TextureRequest(paths[i], mode);
        if (r) Annotate(ip, r);
    }
    if (!ip->streamRequests.empty()) gConstraintRequests.insert(ip);
    ip->streamPhase = 1;
}
static bool RequestsReady(const InstancePrefab* ip) {
    for (size_t i = 0; i < ip->streamRequests.size(); i++) {
        const W3dRequestStream& p = ip->streamRequests[i];
        if (!W3dResourceCurrent(p.r, p.series)) continue;   // (ya no existe: no hay que esperarlo)
        if (p.r->state == W3DREC_IN_FLIGHT || p.r->state == W3DREC_NOT_LOADED) return false;
    }
    return true;
}

// ============================================================================
//  GENERAR / DESCARGAR
// ============================================================================
// genera 'ip' y, si es una ANIDADA (adentro de lo que genero otra), le pone los overrides de las de afuera y suma lo
// nuevo a su base (io/Prefabs.h: W3dPrefabNestedGenerated). Toda generacion del streaming pasa por aca
static void GenerateIn(InstancePrefab* ip) {
    W3dPrefabGenerate(ip);
    W3dPrefabNestedGenerated(ip);
}
static void Generate(InstancePrefab* ip) {
    GenerateIn(ip);          // (deja streamState en CARGADA; sus materiales despiertan sin esperar la cola)
    gStats.generatedTotal++;
    g_redraw = true;        // (el editor dibuja por eventos: la vista previa tiene que verse)
    // con la partida andando, lo generado arranca como lo que crea instanciar(): sus scripts, sus cuerpos, sus hitbox
    // (en el Play del editor queda ademas anotado como creado jugando: el Stop lo libera)
    Object* root = ip->RootGenerated();
    if (root) W3dScriptNotifyNewObject(root);
}
// los materiales que dibujan las mallas de 'o' hacia abajo
static void CollectMaterials(Object* o, std::set<Material*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::mesh) {
            const Mesh* m = (const Mesh*)h;
            for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                if (m->materialsGroup[g].material) out.insert(m->materialsGroup[g].material);
                for (size_t c = 0; c < m->materialsGroup[g].layersMat.size(); c++)   // (las capas de material de encima)
                    if (m->materialsGroup[g].layersMat[c]) out.insert(m->materialsGroup[g].layersMat[c]);
            }
            for (size_t g = 0; g < m->genMaterialsGroup.size(); g++) if (m->genMaterialsGroup[g].material) out.insert(m->genMaterialsGroup[g].material);
        }
        CollectMaterials(h, out);
    }
}
static void CollectUsed(Object* o, std::set<const Material*>& out) {
    std::set<Material*> s;
    CollectMaterials(o, s);
    out.insert(s.begin(), s.end());
}
// los materiales de lo descargado que ya ninguna malla de las raices cargadas dibuja DUERMEN sus texturas (la que
// ningun otro material retiene sale de la GPU)
static int SleepUnused(const std::set<Material*>& candidates) {
    if (candidates.empty()) return 0;
    std::set<const Material*> used;
    std::vector<Object*> roots;
    W3dRootsInOrder(roots);
    bool active = false;
    for (size_t i = 0; i < roots.size(); i++) { CollectUsed(roots[i], used); if (roots[i] == SceneCollection) active = true; }
    if (!active) CollectUsed(SceneCollection, used);
    int n = 0;
    for (std::set<Material*>::const_iterator it = candidates.begin(); it != candidates.end(); ++it)
        if (!used.count(*it) && TexturesSleepMaterial(*it)) n++;
    return n;
}
static void Unload(InstancePrefab* ip, std::set<Material*>& mats) {
    CollectMaterials(ip, mats);
    if (W3dScriptHasSession()) {
        // la RUTA de destruir() de lua, ya (el tick corre fuera de los actualizar()): sus scripts, su fisica, sus refs y
        // sus curvas se van y se LIBERA (la partida lo saca antes de sus listas y fotos). En el Play del editor tambien
        // lo que el usuario tenia generado desde antes (una de siempre que lua descargo): se anota para que el Stop la
        // vuelva a generar
        std::vector<Object*> children = ip->Childrens;
        for (size_t i = 0; i < children.size(); i++) if (W3dObjectLive(children[i])) W3dScriptFreeNow(children[i]);
        // (la foto de lo generado se va con lo generado: sus overrides quedan anotados en la instancia)
        std::vector<InstanceBase>().swap(ip->base);
        ip->generatedVersion = 0;
        bool annotated = false;
        for (size_t i = 0; i < gReleased.size() && !annotated; i++) annotated = (gReleased[i].ip == ip);
        if (!annotated) { Released s; s.ip = ip; s.serial = ip->serial; s.root = SceneCollection; gReleased.push_back(s); }
    } else {
        // sin partida (la vista previa del editor): lo que el usuario cambio en lo generado (el ojo de un hijo, un
        // valor de un script) se anota ANTES de soltarlo, en la instancia de AFUERA de todo (si es una anidada, lo de
        // adentro es override de la de afuera; como al regenerar); despues se suelta
        Object* top = ip;
        while (W3dInstanceOf(top)) top = W3dInstanceOf(top);
        W3dPrefabSyncOverrides((InstancePrefab*)top);
        W3dPrefabRelease(ip);
    }
    ReleaseRequests(ip);
    ip->streamState = W3D_STREAM_UNLOADED;
    gStats.unloadedTotal++;
    g_redraw = true;
}

// ============================================================================
//  EL TICK
// ============================================================================
// el reloj del PRESUPUESTO (ms): el de alta resolucion del editor, o el del Core (el juego compilado)
static double Now() { return W3dStreamingClock ? W3dStreamingClock() : (double)w3dGetTicks(); }
static bool Exceeded(double from) {
    return g_w3dStreamingConfig.budgetMs > 0.0f && Now() - from >= (double)g_w3dStreamingConfig.budgetMs;
}

void W3dStreamingTick() {
    const double t0 = W3dStreamingClock ? W3dStreamingClock() : 0.0;
    const double tPres = Now();
    Rebuild();
    if (!gList.empty()) {
        Vector3 def;
        const bool hasDef = PosDefault(def);
        CacheTargets cache;
        std::set<Material*> mats;
        std::vector<std::pair<float, size_t> > candidate;   // (distancia^2, indice): las que esperan generarse
        bool inFlight = false;
        int unloaded = 0;
        // QUE se mira este tick: la lista entera si es chica; si no, la tajada que toca + las que estan pidiendo
        std::vector<size_t> look;
        const size_t n = gList.size();
        if (n <= kByTick) {
            for (size_t i = 0; i < n; i++) look.push_back(i);
        } else {
            if (gCursor >= n) gCursor = 0;
            for (size_t k = 0; k < kByTick; k++) look.push_back((gCursor + k) % n);
            gCursor = (gCursor + kByTick) % n;
            for (size_t k = 0; k < gRequesting.size(); k++) look.push_back(gRequesting[k]);
            std::sort(look.begin(), look.end());
            look.erase(std::unique(look.begin(), look.end()), look.end());
        }
        // 1) DECIDIR: pedir lo que se acerca, soltar lo que se aleja
        for (size_t m = 0; m < look.size(); m++) {
            const size_t i = look[m];
            InstancePrefab* ip = gList[i].ip;
            if (!W3dLiveObjectSerial(ip, gList[i].serial)) continue;   // (se libero: una descarga de afuera, un borrado)
            Vector3 obj;
            const bool hasPos = PosTarget(ip, cache, hasDef, def, obj);
            float d2 = 0.0f;
            if (hasPos) {
                const Vector3 p = ip->GetGlobalPosition();
                const float dx = p.x - obj.x, dy = p.y - obj.y, dz = p.z - obj.z;
                d2 = dx * dx + dy * dy + dz * dz;
            }
            if (Wants(ip, hasPos, d2)) {
                if (ip->streamState == W3D_STREAM_UNLOADED) {
                    RequestResources(ip, W3DREC_ASYNC);
                    ip->streamState = W3D_STREAM_REQUESTING;
                }
                if (ip->streamState == W3D_STREAM_REQUESTING) {
                    if (!RequestsReady(ip)) inFlight = true;
                    candidate.push_back(std::make_pair(d2, i));
                }
            } else if (ip->streamState == W3D_STREAM_REQUESTING) {
                ReleaseRequests(ip);   // (se alejo antes de llegar a generarse: lo que se cargo se suelta)
                ip->streamState = W3D_STREAM_UNLOADED;
            } else if (ip->streamState == W3D_STREAM_LOADED && unloaded < g_w3dStreamingConfig.unloadPerFrame) {
                Unload(ip, mats);
                unloaded++;
            }
        }
        // 2) BOMBEAR: de a tajadas (la bomba carga UN recurso por llamada; pasado el presupuesto del frame no se empieza
        //    otra -la primera va siempre-)
        if (inFlight)
            for (int k = 0; k < g_w3dStreamingConfig.loadsPerFrame; k++) {
                if (k > 0 && Exceeded(tPres)) break;
                W3dResourcesPump();
            }
        // 3) GENERAR las que tienen todo LISTO, las mas cercanas primero (orden determinista: distancia e indice)
        std::sort(candidate.begin(), candidate.end());
        int generated = 0;
        for (size_t k = 0; k < candidate.size() && generated < g_w3dStreamingConfig.generatePerFrame; k++) {
            if (generated > 0 && Exceeded(tPres)) break;   // (el frame ya se gasto: la proxima, en el siguiente)
            const Token& f = gList[candidate[k].second];
            InstancePrefab* ip = f.ip;
            if (!W3dLiveObjectSerial(ip, f.serial) || ip->streamState != W3D_STREAM_REQUESTING || !RequestsReady(ip)) continue;
            if (ip->streamPhase == 0) {   // (las mallas ya estan: ahora se saben sus materiales, y sus texturas)
                RequestTextures(ip, W3DREC_ASYNC);
                if (!RequestsReady(ip)) continue;
            }
            Generate(ip);
            generated++;
        }
        // 4) los materiales que quedaron sin quien los dibuje duermen sus texturas
        SleepUnused(mats);
        // las que siguen PIDIENDO, para el proximo tick (una generacion rearma la lista entera: Reconstruir)
        if (!gListDirty && gSpawnedList == W3dObjectsSpawned()) {
            gRequesting.clear();
            for (size_t m = 0; m < look.size(); m++)
                if (W3dLiveObjectSerial(gList[look[m]].ip, gList[look[m]].serial) &&
                    gList[look[m]].ip->streamState == W3D_STREAM_REQUESTING) gRequesting.push_back(look[m]);
        }
    }
    if (W3dStreamingClock) {
        gStats.tickMs = W3dStreamingClock() - t0;
        if (gStats.tickMs > gStats.tickWorstMs) gStats.tickWorstMs = gStats.tickMs;
    }
}

void W3dStreamingTickEditor() {
    if (g_w3dStreamingPreviewView && !W3dScriptHasSession()) W3dStreamingTick();
}

// ============================================================================
//  LA CARGA DE UN NIVEL
// ============================================================================
bool W3dStreamingDeferOnLoad(const InstancePrefab* ip) {
    if (!ip || ip->load != W3D_LOAD_DISTANCE) return false;
#if defined(W3D_NO_EDITOR) || defined(W3D_SYMBIAN)
    // el juego compilado: lo diferido espera al streaming, siempre. El editor del telefono tambien: un mundo
    // abierto generado entero al abrir no entra en su memoria (se cierra antes de mostrar nada)
    return true;
#else
    // el editor: con la vista previa, o con una partida andando (lo que genera el streaming jugando trae sus
    // anidadas diferidas igual que en el juego compilado); el editor que juega como juego (g_gameMode) tambien
    extern bool g_gameMode;
    return g_w3dStreamingPreviewView || W3dScriptHasSession() || g_gameMode;
#endif
}

// ============================================================================
//  LA PARTIDA
// ============================================================================
// las diferidas de 'o' hacia abajo, las de AFUERA (soltar una de afuera suelta las de adentro); adentro de lo que
// genera una de siempre si se baja
static void CollectDeferedOfOutside(Object* o, std::vector<InstancePrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dIsInstanceType(h->getType()) && ((InstancePrefab*)h)->load == W3D_LOAD_DISTANCE) {
            out.push_back((InstancePrefab*)h);
            continue;
        }
        CollectDeferedOfOutside(h, out);
    }
}
static void CollectInstancesOfOutside(Object* o, std::vector<InstancePrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dIsInstanceType(h->getType())) { out.push_back((InstancePrefab*)h); continue; }
        CollectInstancesOfOutside(h, out);
    }
}
void W3dStreamingSessionPrepare() {
    if (!SceneCollection) return;
    // los OVERRIDES de todas las de afuera, ANTES de soltar nada: lo que el usuario cambio en lo generado (tambien
    // adentro de una diferida anidada) queda anotado en su instancia
    std::vector<InstancePrefab*> tops;
    CollectInstancesOfOutside(SceneCollection, tops);
    for (size_t i = 0; i < tops.size(); i++) W3dPrefabSyncOverrides(tops[i]);
    std::vector<InstancePrefab*> v;
    CollectDeferedOfOutside(SceneCollection, v);
    int n = 0;
    std::set<Material*> mats;
    for (size_t i = 0; i < v.size(); i++) {
        InstancePrefab* ip = v[i];
        if (ip->streamState == W3D_STREAM_LOADED) {
            Released s; s.ip = ip; s.serial = ip->serial; s.root = SceneCollection;
            gReleased.push_back(s);
            CollectMaterials(ip, mats);
            W3dPrefabRelease(ip);
            n++;
        }
        ReleaseRequests(ip);
        ip->streamState  = W3D_STREAM_UNLOADED;
        ip->pinnedStream = 0;
    }
    // (y los materiales que ya nadie dibuja duermen sus texturas: la partida arranca como el juego compilado recien abierto)
    SleepUnused(mats);
    gListDirty = true;
    if (n) w3dLogf("[streaming] la partida arranca: %d instancia(s) diferida(s) sueltan lo generado (vuelven al Stop)", n);
}

void W3dStreamingSessionStart() {
    gListDirty = true;
    Rebuild();
    Vector3 def;
    const bool hasDef = PosDefault(def);
    CacheTargets cache;
    int n = 0;
    // (por indice y revalidando: lo que se genera puede traer anidadas, que entran a la lista en el proximo tick)
    const std::vector<Token> list = gList;
    for (size_t i = 0; i < list.size(); i++) {
        InstancePrefab* ip = list[i].ip;
        if (!W3dLiveObjectSerial(ip, list[i].serial) || ip->streamState == W3D_STREAM_LOADED) continue;
        Vector3 obj;
        const bool hasPos = PosTarget(ip, cache, hasDef, def, obj);
        float d2 = 0.0f;
        if (hasPos) {
            const Vector3 p = ip->GetGlobalPosition();
            const float dx = p.x - obj.x, dy = p.y - obj.y, dz = p.z - obj.z;
            d2 = dx * dx + dy * dy + dz * dz;
        }
        if (!Wants(ip, hasPos, d2)) continue;
        // BLOQUEANTE: el primer frame de la partida ya las tiene (igual en el Play y en el juego compilado)
        if (ip->streamState == W3D_STREAM_UNLOADED) RequestResources(ip, W3DREC_BLOCKING);
        RequestTextures(ip, W3DREC_BLOCKING);
        Generate(ip);
        n++;
    }
    if (n) w3dLogf("[streaming] la partida arranca con %d instancia(s) diferida(s) cargada(s)", n);
}

void W3dStreamingSessionEnd() {
    // los pedidos de la partida se sueltan y lo fijado por lua se olvida (en TODAS las raices cargadas: cambiarEscena
    // pudo dejar otras con lo suyo)
    std::vector<Object*> roots;
    W3dRootsInOrder(roots);
    bool active = false;
    for (size_t r = 0; r < roots.size(); r++) if (roots[r] == SceneCollection) active = true;
    if (!active && SceneCollection) roots.push_back(SceneCollection);
    std::vector<InstancePrefab*> regenerate;   // (de la raiz activa: las de otras raices quedan como el Stop las dejo)
    for (size_t r = 0; r < roots.size(); r++) {
        // (todas las instancias, no solo las diferidas: lua pudo fijar una de siempre)
        std::vector<Object*> stack(1, roots[r]);
        while (!stack.empty()) {
            Object* o = stack.back(); stack.pop_back();
            if (!o) continue;
            for (size_t i = 0; i < o->Childrens.size(); i++) stack.push_back(o->Childrens[i]);
            if (o != roots[r] && W3dIsInstanceType(o->getType())) {
                InstancePrefab* ip = (InstancePrefab*)o;
                ReleaseRequests(ip);
                ip->pinnedStream = 0;
                if (ip->streamState == W3D_STREAM_REQUESTING) ip->streamState = W3D_STREAM_UNLOADED;
                // (lo que el streaming genero jugando era creado en el Play: el Stop ya lo libero)
                if (ip->Childrens.empty() && !ip->notGenerated) ip->streamState = W3D_STREAM_UNLOADED;
                // (lo generado que el Play descolgo -un unload() de lua sobre una de siempre- y el Stop devolvio: vuelve
                //  a estar cargada. Si jugando se volvio a generar, su base es la de lo que el Stop libero: se regenera,
                //  asi lo que el usuario cambie despues se sigue viendo como override)
                if (!ip->Childrens.empty()) {
                    ip->streamState = W3D_STREAM_LOADED;
                    bool baseLive = ip->base.empty();
                    for (size_t b = 0; b < ip->base.size() && !baseLive; b++)
                        baseLive = W3dLiveObjectSerial(ip->base[b].o, ip->base[b].serial);
                    if (!baseLive && roots[r] == SceneCollection) regenerate.push_back(ip);
                }
            }
        }
    }
    for (size_t i = 0; i < regenerate.size(); i++) if (W3dObjectLive(regenerate[i])) GenerateIn(regenerate[i]);
    // las diferidas que el Play solto vuelven a generarse, cada una con SU raiz como la activa (como la carga)
    const int act = W3dActiveRoot();
    int n = 0;
    std::vector<Released> released;
    released.swap(gReleased);
    for (size_t i = 0; i < released.size(); i++) {
        InstancePrefab* ip = released[i].ip;
        if (!W3dLiveObjectSerial(ip, released[i].serial) || ip->streamState == W3D_STREAM_LOADED) continue;
        if (released[i].root != SceneCollection) {
            const int k = W3dObjectRoot(released[i].root);
            if (k < 0 || !W3dRootUse(k)) continue;
        }
        GenerateIn(ip);
        n++;
    }
    if (act >= 0 && W3dActiveRoot() != act) W3dRootUse(act);
    gListDirty = true;
    if (n) w3dLogf("[streaming] Stop: %d instancia(s) diferida(s) vuelven a generarse como antes del Play", n);
}

// ============================================================================
//  LUA / CAMBIOS / CIERRE
// ============================================================================
int W3dStreamingRequest(Object* o, int request) {
    if (!o || !W3dIsInstanceType(o->getType())) return -1;
    InstancePrefab* ip = (InstancePrefab*)o;
    if (request == 1)       ip->pinnedStream = 1;
    else if (request == -1) ip->pinnedStream = -1;
    else if (request == 2)  ip->pinnedStream = 0;
    if (request != 0) gListDirty = true;
    return ip->streamState;
}

// (true cuando el programa ya termina: las listas de aca pudieron destruirse antes que el ultimo objeto -el orden de
//  destruccion de lo estatico entre archivos no esta definido-; ver gProgramEnd al final del archivo)
static bool gFinishing = false;
void W3dStreamingForgetInstance(InstancePrefab* ip) {
    if (!ip || gFinishing) return;
    // (solo las que retienen algo estan en gConstraintRequests: una que nunca pidio nada no toca nada)
    if (!ip->streamRequests.empty()) ReleaseRequests(ip);
    for (size_t i = gReleased.size(); i-- > 0; ) if (gReleased[i].ip == ip) gReleased.erase(gReleased.begin() + (long)i);
    gListDirty = true;
}

void W3dStreamingCloseProject() {
    std::set<InstancePrefab*> all;
    all.swap(gConstraintRequests);
    for (std::set<InstancePrefab*>::iterator it = all.begin(); it != all.end(); ++it) {
        InstancePrefab* ip = *it;
        std::vector<W3dRequestStream> v;
        v.swap(ip->streamRequests);
        for (size_t i = 0; i < v.size(); i++) ReleaseOne(v[i]);
        ip->streamPhase = 0;
    }
    gReleased.clear();
    gList.clear();
    gListDirty = true;
    gRootList = NULL;
    g_w3dStreamingPreviewView = false;   // (es una opcion del proyecto: el que abre trae la suya)
}

void W3dStreamingChangedInstance(InstancePrefab* ip) {
    gListDirty = true;
    if (!ip) return;
    // dejo de ser diferida (o cambio su distancia) con la vista previa apagada y sin jugar: el editor la quiere
    // generada, como cualquier instancia de siempre
    if (ip->streamState != W3D_STREAM_LOADED && !W3dScriptHasSession() &&
        (ip->load != W3D_LOAD_DISTANCE || !g_w3dStreamingPreviewView)) {
        ReleaseRequests(ip);
        GenerateIn(ip);
    }
}

void W3dStreamingPreviewViewSet(bool on) {
    g_w3dStreamingPreviewView = on;
    gListDirty = true;
    if (on || W3dScriptHasSession() || !SceneCollection) return;
    // APAGADA: el editor vuelve a ver el nivel entero (las descargadas se generan) y nada queda retenido de mas
    std::vector<Token> v;
    CollectList(SceneCollection, v);
    int n = 0;
    for (size_t i = 0; i < v.size(); i++) {
        InstancePrefab* ip = v[i].ip;
        if (!W3dLiveObjectSerial(ip, v[i].serial)) continue;
        ReleaseRequests(ip);
        ip->pinnedStream = 0;
        if (ip->streamState != W3D_STREAM_LOADED) { GenerateIn(ip); n++; }
    }
    if (n) w3dLogf("[streaming] vista previa apagada: %d instancia(s) diferida(s) generadas", n);
}

void W3dStreamingResourcesRetained(int type, std::vector<std::string>& ids) {
    for (std::set<InstancePrefab*>::const_iterator it = gConstraintRequests.begin(); it != gConstraintRequests.end(); ++it)
        for (size_t i = 0; i < (*it)->streamRequests.size(); i++) {
            const W3dRequestStream& p = (*it)->streamRequests[i];
            if (W3dResourceCurrent(p.r, p.series) && p.r->type == type) ids.push_back(p.r->id);
        }
}

void W3dStreamingForcedTarget(bool on, const Vector3& pos) {
    gForced = on;
    gForcedPos = pos;
}

void W3dStreamingGetStats(W3dStreamingStats& st) {
    Rebuild();
    const W3dStreamingStats accum = gStats;
    st = W3dStreamingStats();
    st.generatedTotal = accum.generatedTotal;
    st.unloadedTotal = accum.unloadedTotal;
    st.tickMs = accum.tickMs;
    st.tickWorstMs = accum.tickWorstMs;
    for (size_t i = 0; i < gList.size(); i++) {
        const InstancePrefab* ip = gList[i].ip;
        if (!W3dLiveObjectSerial(ip, gList[i].serial)) continue;
        st.instances++;
        if (ip->streamState == W3D_STREAM_LOADED) st.loaded++;
        else if (ip->streamState == W3D_STREAM_REQUESTING) st.requesting++;
        else st.unloaded++;
    }
    for (std::set<InstancePrefab*>::const_iterator it = gConstraintRequests.begin(); it != gConstraintRequests.end(); ++it)
        st.liveRequests += (int)(*it)->streamRequests.size();
}
void W3dStreamingStatsReset() {
    gStats.generatedTotal = 0;
    gStats.unloadedTotal = 0;
    gStats.tickMs = 0.0;
    gStats.tickWorstMs = 0.0;
}

// el hook de lua (cargar/descargar/cargaAuto/cargado) y el reloj del editor, al arrancar
#ifndef W3D_NO_EDITOR
extern double W3dNowMs();
#endif
namespace {
// la ULTIMA estatica del archivo: se destruye PRIMERO (orden inverso), antes que las listas de arriba
struct ProgramEnd { ~ProgramEnd() { gFinishing = true; } } gProgramEnd;
struct RegisterStreaming {
    RegisterStreaming() {
        W3dStreamingLuaHook = W3dStreamingRequest;
#ifndef W3D_NO_EDITOR
        W3dStreamingClock = W3dNowMs;
#endif
    }
} gRegisterStreaming;
}
