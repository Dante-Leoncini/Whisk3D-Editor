// ============================================================================
//  Streaming.cpp — ver Streaming.h. Cargar y descargar en tiempo real las
//  instancias de prefab y los proxies DIFERIDOS ("carga": "distancia") segun la
//  distancia a su objetivo, de a tajadas por frame, por el almacen de recursos
//  (pedidos ASYNC + refcount) y por las rutas de instanciar()/destruir() de lua.
//  Compila en el editor y en el runtime 3D (C++03). Motor generico.
// ============================================================================
#include "io/Streaming.h"
#include "io/Prefabs.h"
#include "io/W3dRecursos.h"
#include "objects/InstanciaPrefab.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Camera.h"            // CameraActive: el objetivo por defecto jugando
#include "objects/CameraBase.h"        // g_renderCamPos / g_vistaBindeada: el de la vista previa del editor
#include "animation/W3dAnimSet.h"      // W3dAnimSetsAsegurarOps (los animsets se piden antes de que nadie los use)
#include "script/W3dScript.h"          // las rutas de instanciar()/destruir() + el hook de lua + las fuentes
#include "importers/import_obj.h"      // las texturas DORMIDAS de un material (pedirlas, dormirlas otra vez)
#include "animation/Animation.h"      // w3dGetTicks: el reloj del presupuesto del frame (el juego compilado)
#include "render/OpcionesRender.h"     // g_redraw: lo que entra o sale se dibuja (el editor dibuja por eventos)
#include "W3dRaices.h"
#include "w3dlog.h"
#include <map>
#include <set>
#include <algorithm>

W3dStreamingConfig g_w3dStreamingConfig = { 2, 2, 4, 0.10f, 4.0f };
bool g_w3dStreamingVistaPrevia = false;
double (*W3dStreamingReloj)() = 0;

namespace {
// las instancias que mira el tick: las DIFERIDAS de la raiz activa (tambien las de adentro de lo generado), las que
// lua FIJO y las que estan sin cargar aunque sean de siempre. Se rearma cuando nacen objetos, cuando cambia la raiz activa o cuando algo la ensucia; una que se libera
// en el medio se reconoce por su serial (la direccion se recicla)
struct Ficha { InstanciaPrefab* ip; unsigned serial; };
std::vector<Ficha> gLista;
unsigned gListaNacidos = 0;
Object*  gListaRaiz = NULL;
bool     gListaSucia = true;
// las que RETIENEN pedidos (el cierre del proyecto los suelta todos)
std::set<InstanciaPrefab*> gConPedidos;
// las diferidas que el Play del editor SOLTO al arrancar (el Stop las vuelve a generar), con su raiz
struct Soltada { InstanciaPrefab* ip; unsigned serial; Object* raiz; };
std::vector<Soltada> gSoltadas;
// el objetivo por defecto FORZADO (el harness)
bool    gForzado = false;
Vector3 gForzadoPos;
W3dStreamingStats gStats;

typedef std::map<std::pair<Object*, std::string>, Object*> CacheObjetivos;
}

// ============================================================================
//  LA LISTA
// ============================================================================
static void JuntarLista(Object* o, std::vector<Ficha>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dEsTipoInstancia(h->getType())) {
            InstanciaPrefab* ip = (InstanciaPrefab*)h;
            // (tambien una de siempre que esta sin cargar: lua la descargo, o dejo de ser diferida jugando)
            if (ip->carga == W3D_CARGA_DISTANCIA || ip->streamFijado != 0 || ip->streamEstado != W3D_STREAM_CARGADA) {
                Ficha f; f.ip = ip; f.serial = ip->serial;
                out.push_back(f);
            }
        }
        JuntarLista(h, out);   // (lo generado tambien: una diferida anidada adentro de un prefab)
    }
}
static void Reconstruir() {
    const unsigned nacidos = W3dObjetosNacidos();
    if (!gListaSucia && nacidos == gListaNacidos && gListaRaiz == SceneCollection) return;
    gLista.clear();
    JuntarLista(SceneCollection, gLista);
    gListaNacidos = nacidos;
    gListaRaiz = SceneCollection;
    gListaSucia = false;
}

// ============================================================================
//  EL OBJETIVO
// ============================================================================
// el punto por defecto: el forzado; sin partida (la vista previa del editor), la vista que dibuja; jugando, la
// camara activa. false = no hay ninguno (nada contra que medir: todo queda cargado)
static bool PosDefecto(Vector3& out) {
    if (gForzado) { out = gForzadoPos; return true; }
    if (!W3dScriptHayPartida() && g_vistaBindeada) { out = g_renderCamPos; return true; }
    if (CameraActive) { out = ((Object*)CameraActive)->GetGlobalPosition(); return true; }
    if (g_vistaBindeada) { out = g_renderCamPos; return true; }
    return false;
}
// el de una instancia: su 'objetivo' por nombre (por SCOPE: desde donde vive la instancia, despues todo el arbol; uno
// por tick y por scope en el cache) o el de defecto si no nombra ninguno o el nombrado no esta
static bool PosObjetivo(InstanciaPrefab* ip, CacheObjetivos& cache, bool hayDef, const Vector3& def, Vector3& out) {
    if (!ip->objetivo.empty()) {
        const std::pair<Object*, std::string> clave(W3dInstanciaDe(ip), ip->objetivo);
        CacheObjetivos::iterator it = cache.find(clave);
        Object* o = NULL;
        if (it != cache.end()) o = it->second;
        else {
            o = W3dBuscarNombreDesde(ip->Parent ? ip->Parent : (Object*)ip, ip->objetivo);
            cache[clave] = o;
        }
        if (o) { out = o->GetGlobalPosition(); return true; }
    }
    if (!hayDef) return false;
    out = def;
    return true;
}
// la quiere cargada? (lua manda; una de siempre, si; una diferida, por la distancia CON histeresis: descargada carga
// a menos de 'distancia', cargada -o pidiendo- se descarga a mas de distancia * (1 + histeresis))
static bool Quiere(const InstanciaPrefab* ip, bool hayPos, float d2) {
    if (ip->streamFijado > 0) return true;
    if (ip->streamFijado < 0) return false;
    if (ip->carga != W3D_CARGA_DISTANCIA || !hayPos) return true;
    float lim = ip->distancia;
    if (ip->streamEstado != W3D_STREAM_DESCARGADA) lim *= 1.0f + g_w3dStreamingConfig.histeresis;
    return d2 <= lim * lim;
}

// ============================================================================
//  LOS PEDIDOS AL ALMACEN
// ============================================================================
// (un pedido cuyo descriptor ya no existe -la biblioteca purgo esa textura, con "Delete anyway"- no se toca: su
//  referencia murio con el)
static void SoltarUno(const W3dPedidoStream& p) {
    if (W3dRecursoVigente(p.r, p.serie)) W3dRecursoSoltar(p.r, W3DREC_PERMANENTE);
}
static void Anotar(InstanciaPrefab* ip, W3dRecurso* r) {
    W3dPedidoStream p; p.r = r; p.serie = r->serie;
    ip->streamPedidos.push_back(p);
}
static void SoltarPedidos(InstanciaPrefab* ip) {
    std::vector<W3dPedidoStream> v;
    v.swap(ip->streamPedidos);
    for (size_t i = 0; i < v.size(); i++) SoltarUno(v[i]);
    gConPedidos.erase(ip);
    ip->streamFase = 0;
}
// las mallas, los animsets y las fuentes de los scripts de su definicion (y de las anidadas que genera con ella)
static void PedirRecursos(InstanciaPrefab* ip, int modo) {
    W3dMallasAsegurarOps();
    W3dAnimSetsAsegurarOps();
    W3dScriptFuentesAsegurarOps();
    std::vector<W3dCargasItem> items;
    W3dPrefabRecursos(ip, items, NULL);
    for (size_t i = 0; i < items.size(); i++) {
#ifndef W3D_SIN_EDITOR
        // (las FUENTES de los scripts solo se traen antes en el juego compilado, donde no cambian: el editor las lee
        //  frescas -el IDE las edita jugando-, ver CorrerArchivo en script/W3dScript.cpp)
        if (items[i].tipo == W3DREC_SCRIPT) continue;
#endif
        W3dRecurso* r = W3dRecursoAdquirir(items[i].tipo, items[i].id, W3DREC_PERMANENTE, modo, "streaming");
        if (r && r->estado != W3DREC_FALLO) Anotar(ip, r);   // (un fallo no suma referencia)
    }
    if (!ip->streamPedidos.empty()) gConPedidos.insert(ip);   // (solo las que retienen algo: ver SoltarPedidos)
    ip->streamFase = 0;
}
// con las mallas LISTAS: las texturas de los materiales que dibujan (las DORMIDAS, o la base que espera en la cola
// diferida; las que ya estan en una ranura no hacen falta)
static void PedirTexturas(InstanciaPrefab* ip, int modo) {
    std::vector<W3dCargasItem> items;
    std::vector<MallaRecurso*> mallas;
    W3dPrefabRecursos(ip, items, &mallas);
    std::vector<std::string> rutas;
    for (size_t i = 0; i < mallas.size(); i++)
        for (size_t k = 0; k < mallas[i]->materiales.size(); k++) {
            Material* m = mallas[i]->materiales[k];
            if (!m) continue;
            const TexDormida* d = TexturasDormidasDe(m);
            if (d) {
                rutas.push_back(d->base);
                rutas.push_back(d->normal);
                for (size_t c = 0; c < d->capas.size(); c++) rutas.push_back(d->capas[c].textura);
            } else if (!m->texture) {
                const std::string p = TexturaPendienteDe(m);
                if (!p.empty()) rutas.push_back(p);
            }
        }
    std::set<std::string> vistas;
    for (size_t i = 0; i < rutas.size(); i++) {
        if (rutas[i].empty() || !vistas.insert(rutas[i]).second) continue;
        W3dRecurso* r = TexturaPedir(rutas[i], modo);
        if (r) Anotar(ip, r);
    }
    if (!ip->streamPedidos.empty()) gConPedidos.insert(ip);
    ip->streamFase = 1;
}
static bool PedidosListos(const InstanciaPrefab* ip) {
    for (size_t i = 0; i < ip->streamPedidos.size(); i++) {
        const W3dPedidoStream& p = ip->streamPedidos[i];
        if (!W3dRecursoVigente(p.r, p.serie)) continue;   // (ya no existe: no hay que esperarlo)
        if (p.r->estado == W3DREC_EN_VUELO || p.r->estado == W3DREC_NO_CARGADO) return false;
    }
    return true;
}

// ============================================================================
//  GENERAR / DESCARGAR
// ============================================================================
// genera 'ip' y, si es una ANIDADA (adentro de lo que genero otra), le pone los overrides de las de afuera y suma lo
// nuevo a su base (io/Prefabs.h: W3dPrefabAnidadaGenerada). Toda generacion del streaming pasa por aca
static void GenerarEn(InstanciaPrefab* ip) {
    W3dPrefabGenerar(ip);
    W3dPrefabAnidadaGenerada(ip);
}
static void Generar(InstanciaPrefab* ip) {
    GenerarEn(ip);          // (deja streamEstado en CARGADA; sus materiales despiertan sin esperar la cola)
    gStats.generadasTotal++;
    g_redraw = true;        // (el editor dibuja por eventos: la vista previa tiene que verse)
    // con la partida andando, lo generado arranca como lo que crea instanciar(): sus scripts, sus cuerpos, sus hitbox
    // (en el Play del editor queda ademas anotado como creado jugando: el Stop lo libera)
    Object* raiz = ip->RaizGenerada();
    if (raiz) W3dScriptAvisarObjetoNuevo(raiz);
}
// los materiales que dibujan las mallas de 'o' hacia abajo
static void JuntarMateriales(Object* o, std::set<Material*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::mesh) {
            const Mesh* m = (const Mesh*)h;
            for (size_t g = 0; g < m->materialsGroup.size(); g++) if (m->materialsGroup[g].material) out.insert(m->materialsGroup[g].material);
            for (size_t g = 0; g < m->genMaterialsGroup.size(); g++) if (m->genMaterialsGroup[g].material) out.insert(m->genMaterialsGroup[g].material);
        }
        JuntarMateriales(h, out);
    }
}
static void JuntarUsados(Object* o, std::set<const Material*>& out) {
    std::set<Material*> s;
    JuntarMateriales(o, s);
    out.insert(s.begin(), s.end());
}
// los materiales de lo descargado que ya ninguna malla de las raices cargadas dibuja DUERMEN sus texturas (la que
// ningun otro material retiene sale de la GPU)
static int DormirSinUso(const std::set<Material*>& candidatos) {
    if (candidatos.empty()) return 0;
    std::set<const Material*> usados;
    std::vector<Object*> raices;
    W3dRaicesEnOrden(raices);
    bool activa = false;
    for (size_t i = 0; i < raices.size(); i++) { JuntarUsados(raices[i], usados); if (raices[i] == SceneCollection) activa = true; }
    if (!activa) JuntarUsados(SceneCollection, usados);
    int n = 0;
    for (std::set<Material*>::const_iterator it = candidatos.begin(); it != candidatos.end(); ++it)
        if (!usados.count(*it) && TexturasAdormecer(*it)) n++;
    return n;
}
static void Descargar(InstanciaPrefab* ip, std::set<Material*>& mats) {
    JuntarMateriales(ip, mats);
    if (W3dScriptHayPartida()) {
        // la RUTA de destruir() de lua, ya (el tick corre fuera de los actualizar()): sus scripts, su fisica, sus refs y
        // sus curvas se van y se LIBERA (la partida lo saca antes de sus listas y fotos). En el Play del editor tambien
        // lo que el usuario tenia generado desde antes (una de siempre que lua descargo): se anota para que el Stop la
        // vuelva a generar
        std::vector<Object*> hijos = ip->Childrens;
        for (size_t i = 0; i < hijos.size(); i++) if (W3dObjetoVivo(hijos[i])) W3dScriptLiberarYa(hijos[i]);
        // (la foto de lo generado se va con lo generado: sus overrides quedan anotados en la instancia)
        std::vector<InstanciaBase>().swap(ip->base);
        ip->versionGenerada = 0;
        bool anotada = false;
        for (size_t i = 0; i < gSoltadas.size() && !anotada; i++) anotada = (gSoltadas[i].ip == ip);
        if (!anotada) { Soltada s; s.ip = ip; s.serial = ip->serial; s.raiz = SceneCollection; gSoltadas.push_back(s); }
    } else {
        // sin partida (la vista previa del editor): lo que el usuario cambio en lo generado (el ojo de un hijo, un
        // valor de un script) se anota ANTES de soltarlo, en la instancia de AFUERA de todo (si es una anidada, lo de
        // adentro es override de la de afuera; como al regenerar); despues se suelta
        Object* top = ip;
        while (W3dInstanciaDe(top)) top = W3dInstanciaDe(top);
        W3dPrefabSincronizarOverrides((InstanciaPrefab*)top);
        W3dPrefabSoltar(ip);
    }
    SoltarPedidos(ip);
    ip->streamEstado = W3D_STREAM_DESCARGADA;
    gStats.descargadasTotal++;
    g_redraw = true;
}

// ============================================================================
//  EL TICK
// ============================================================================
// el reloj del PRESUPUESTO (ms): el de alta resolucion del editor, o el del Core (el juego compilado)
static double Ahora() { return W3dStreamingReloj ? W3dStreamingReloj() : (double)w3dGetTicks(); }
static bool Excedido(double desde) {
    return g_w3dStreamingConfig.presupuestoMs > 0.0f && Ahora() - desde >= (double)g_w3dStreamingConfig.presupuestoMs;
}

void W3dStreamingTick() {
    const double t0 = W3dStreamingReloj ? W3dStreamingReloj() : 0.0;
    const double tPres = Ahora();
    Reconstruir();
    if (!gLista.empty()) {
        Vector3 def;
        const bool hayDef = PosDefecto(def);
        CacheObjetivos cache;
        std::set<Material*> mats;
        std::vector<std::pair<float, size_t> > candidatas;   // (distancia^2, indice): las que esperan generarse
        bool enVuelo = false;
        int descargadas = 0;
        // 1) DECIDIR: pedir lo que se acerca, soltar lo que se aleja
        for (size_t i = 0; i < gLista.size(); i++) {
            InstanciaPrefab* ip = gLista[i].ip;
            if (!W3dObjetoVivoSerial(ip, gLista[i].serial)) continue;   // (se libero: una descarga de afuera, un borrado)
            Vector3 obj;
            const bool hayPos = PosObjetivo(ip, cache, hayDef, def, obj);
            float d2 = 0.0f;
            if (hayPos) {
                const Vector3 p = ip->GetGlobalPosition();
                const float dx = p.x - obj.x, dy = p.y - obj.y, dz = p.z - obj.z;
                d2 = dx * dx + dy * dy + dz * dz;
            }
            if (Quiere(ip, hayPos, d2)) {
                if (ip->streamEstado == W3D_STREAM_DESCARGADA) {
                    PedirRecursos(ip, W3DREC_ASYNC);
                    ip->streamEstado = W3D_STREAM_PIDIENDO;
                }
                if (ip->streamEstado == W3D_STREAM_PIDIENDO) {
                    if (!PedidosListos(ip)) enVuelo = true;
                    candidatas.push_back(std::make_pair(d2, i));
                }
            } else if (ip->streamEstado == W3D_STREAM_PIDIENDO) {
                SoltarPedidos(ip);   // (se alejo antes de llegar a generarse: lo que se cargo se suelta)
                ip->streamEstado = W3D_STREAM_DESCARGADA;
            } else if (ip->streamEstado == W3D_STREAM_CARGADA && descargadas < g_w3dStreamingConfig.descargarPorFrame) {
                Descargar(ip, mats);
                descargadas++;
            }
        }
        // 2) BOMBEAR: de a tajadas (la bomba carga UN recurso por llamada; pasado el presupuesto del frame no se empieza
        //    otra -la primera va siempre-)
        if (enVuelo)
            for (int k = 0; k < g_w3dStreamingConfig.cargasPorFrame; k++) {
                if (k > 0 && Excedido(tPres)) break;
                W3dRecursosPump();
            }
        // 3) GENERAR las que tienen todo LISTO, las mas cercanas primero (orden determinista: distancia e indice)
        std::sort(candidatas.begin(), candidatas.end());
        int generadas = 0;
        for (size_t k = 0; k < candidatas.size() && generadas < g_w3dStreamingConfig.generarPorFrame; k++) {
            if (generadas > 0 && Excedido(tPres)) break;   // (el frame ya se gasto: la proxima, en el siguiente)
            const Ficha& f = gLista[candidatas[k].second];
            InstanciaPrefab* ip = f.ip;
            if (!W3dObjetoVivoSerial(ip, f.serial) || ip->streamEstado != W3D_STREAM_PIDIENDO || !PedidosListos(ip)) continue;
            if (ip->streamFase == 0) {   // (las mallas ya estan: ahora se saben sus materiales, y sus texturas)
                PedirTexturas(ip, W3DREC_ASYNC);
                if (!PedidosListos(ip)) continue;
            }
            Generar(ip);
            generadas++;
        }
        // 4) los materiales que quedaron sin quien los dibuje duermen sus texturas
        DormirSinUso(mats);
    }
    if (W3dStreamingReloj) {
        gStats.tickMs = W3dStreamingReloj() - t0;
        if (gStats.tickMs > gStats.tickPeorMs) gStats.tickPeorMs = gStats.tickMs;
    }
}

void W3dStreamingTickEditor() {
    if (g_w3dStreamingVistaPrevia && !W3dScriptHayPartida()) W3dStreamingTick();
}

// ============================================================================
//  LA CARGA DE UN NIVEL
// ============================================================================
bool W3dStreamingDiferirAlCargar(const InstanciaPrefab* ip) {
    if (!ip || ip->carga != W3D_CARGA_DISTANCIA) return false;
#ifdef W3D_SIN_EDITOR
    return true;   // el juego compilado: lo diferido espera al streaming, siempre
#else
    // el editor: con la vista previa, o con una partida andando (lo que genera el streaming jugando trae sus
    // anidadas diferidas igual que en el juego compilado); el editor que juega como juego (g_modoJuego) tambien
    extern bool g_modoJuego;
    return g_w3dStreamingVistaPrevia || W3dScriptHayPartida() || g_modoJuego;
#endif
}

// ============================================================================
//  LA PARTIDA
// ============================================================================
// las diferidas de 'o' hacia abajo, las de AFUERA (soltar una de afuera suelta las de adentro); adentro de lo que
// genera una de siempre si se baja
static void JuntarDiferidasDeAfuera(Object* o, std::vector<InstanciaPrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dEsTipoInstancia(h->getType()) && ((InstanciaPrefab*)h)->carga == W3D_CARGA_DISTANCIA) {
            out.push_back((InstanciaPrefab*)h);
            continue;
        }
        JuntarDiferidasDeAfuera(h, out);
    }
}
static void JuntarInstanciasDeAfuera(Object* o, std::vector<InstanciaPrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dEsTipoInstancia(h->getType())) { out.push_back((InstanciaPrefab*)h); continue; }
        JuntarInstanciasDeAfuera(h, out);
    }
}
void W3dStreamingPartidaPreparar() {
    if (!SceneCollection) return;
    // los OVERRIDES de todas las de afuera, ANTES de soltar nada: lo que el usuario cambio en lo generado (tambien
    // adentro de una diferida anidada) queda anotado en su instancia
    std::vector<InstanciaPrefab*> tops;
    JuntarInstanciasDeAfuera(SceneCollection, tops);
    for (size_t i = 0; i < tops.size(); i++) W3dPrefabSincronizarOverrides(tops[i]);
    std::vector<InstanciaPrefab*> v;
    JuntarDiferidasDeAfuera(SceneCollection, v);
    int n = 0;
    std::set<Material*> mats;
    for (size_t i = 0; i < v.size(); i++) {
        InstanciaPrefab* ip = v[i];
        if (ip->streamEstado == W3D_STREAM_CARGADA) {
            Soltada s; s.ip = ip; s.serial = ip->serial; s.raiz = SceneCollection;
            gSoltadas.push_back(s);
            JuntarMateriales(ip, mats);
            W3dPrefabSoltar(ip);
            n++;
        }
        SoltarPedidos(ip);
        ip->streamEstado = W3D_STREAM_DESCARGADA;
        ip->streamFijado = 0;
    }
    // (y los materiales que ya nadie dibuja duermen sus texturas: la partida arranca como el juego compilado recien abierto)
    DormirSinUso(mats);
    gListaSucia = true;
    if (n) w3dLogf("[streaming] la partida arranca: %d instancia(s) diferida(s) sueltan lo generado (vuelven al Stop)", n);
}

void W3dStreamingPartidaInicio() {
    gListaSucia = true;
    Reconstruir();
    Vector3 def;
    const bool hayDef = PosDefecto(def);
    CacheObjetivos cache;
    int n = 0;
    // (por indice y revalidando: lo que se genera puede traer anidadas, que entran a la lista en el proximo tick)
    const std::vector<Ficha> lista = gLista;
    for (size_t i = 0; i < lista.size(); i++) {
        InstanciaPrefab* ip = lista[i].ip;
        if (!W3dObjetoVivoSerial(ip, lista[i].serial) || ip->streamEstado == W3D_STREAM_CARGADA) continue;
        Vector3 obj;
        const bool hayPos = PosObjetivo(ip, cache, hayDef, def, obj);
        float d2 = 0.0f;
        if (hayPos) {
            const Vector3 p = ip->GetGlobalPosition();
            const float dx = p.x - obj.x, dy = p.y - obj.y, dz = p.z - obj.z;
            d2 = dx * dx + dy * dy + dz * dz;
        }
        if (!Quiere(ip, hayPos, d2)) continue;
        // BLOQUEANTE: el primer frame de la partida ya las tiene (igual en el Play y en el juego compilado)
        if (ip->streamEstado == W3D_STREAM_DESCARGADA) PedirRecursos(ip, W3DREC_BLOQUEANTE);
        PedirTexturas(ip, W3DREC_BLOQUEANTE);
        Generar(ip);
        n++;
    }
    if (n) w3dLogf("[streaming] la partida arranca con %d instancia(s) diferida(s) cargada(s)", n);
}

void W3dStreamingPartidaFin() {
    // los pedidos de la partida se sueltan y lo fijado por lua se olvida (en TODAS las raices cargadas: cambiarEscena
    // pudo dejar otras con lo suyo)
    std::vector<Object*> raices;
    W3dRaicesEnOrden(raices);
    bool activa = false;
    for (size_t r = 0; r < raices.size(); r++) if (raices[r] == SceneCollection) activa = true;
    if (!activa && SceneCollection) raices.push_back(SceneCollection);
    std::vector<InstanciaPrefab*> regenerar;   // (de la raiz activa: las de otras raices quedan como el Stop las dejo)
    for (size_t r = 0; r < raices.size(); r++) {
        // (todas las instancias, no solo las diferidas: lua pudo fijar una de siempre)
        std::vector<Object*> pila(1, raices[r]);
        while (!pila.empty()) {
            Object* o = pila.back(); pila.pop_back();
            if (!o) continue;
            for (size_t i = 0; i < o->Childrens.size(); i++) pila.push_back(o->Childrens[i]);
            if (o != raices[r] && W3dEsTipoInstancia(o->getType())) {
                InstanciaPrefab* ip = (InstanciaPrefab*)o;
                SoltarPedidos(ip);
                ip->streamFijado = 0;
                if (ip->streamEstado == W3D_STREAM_PIDIENDO) ip->streamEstado = W3D_STREAM_DESCARGADA;
                // (lo que el streaming genero jugando era creado en el Play: el Stop ya lo libero)
                if (ip->Childrens.empty() && !ip->noGenerada) ip->streamEstado = W3D_STREAM_DESCARGADA;
                // (lo generado que el Play descolgo -un descargar() de lua sobre una de siempre- y el Stop devolvio: vuelve
                //  a estar cargada. Si jugando se volvio a generar, su base es la de lo que el Stop libero: se regenera,
                //  asi lo que el usuario cambie despues se sigue viendo como override)
                if (!ip->Childrens.empty()) {
                    ip->streamEstado = W3D_STREAM_CARGADA;
                    bool baseViva = ip->base.empty();
                    for (size_t b = 0; b < ip->base.size() && !baseViva; b++)
                        baseViva = W3dObjetoVivoSerial(ip->base[b].o, ip->base[b].serial);
                    if (!baseViva && raices[r] == SceneCollection) regenerar.push_back(ip);
                }
            }
        }
    }
    for (size_t i = 0; i < regenerar.size(); i++) if (W3dObjetoVivo(regenerar[i])) GenerarEn(regenerar[i]);
    // las diferidas que el Play solto vuelven a generarse, cada una con SU raiz como la activa (como la carga)
    const int act = W3dRaizActiva();
    int n = 0;
    std::vector<Soltada> soltadas;
    soltadas.swap(gSoltadas);
    for (size_t i = 0; i < soltadas.size(); i++) {
        InstanciaPrefab* ip = soltadas[i].ip;
        if (!W3dObjetoVivoSerial(ip, soltadas[i].serial) || ip->streamEstado == W3D_STREAM_CARGADA) continue;
        if (soltadas[i].raiz != SceneCollection) {
            const int k = W3dRaizDeObjeto(soltadas[i].raiz);
            if (k < 0 || !W3dRaizUsar(k)) continue;
        }
        GenerarEn(ip);
        n++;
    }
    if (act >= 0 && W3dRaizActiva() != act) W3dRaizUsar(act);
    gListaSucia = true;
    if (n) w3dLogf("[streaming] Stop: %d instancia(s) diferida(s) vuelven a generarse como antes del Play", n);
}

// ============================================================================
//  LUA / CAMBIOS / CIERRE
// ============================================================================
int W3dStreamingPedido(Object* o, int pedido) {
    if (!o || !W3dEsTipoInstancia(o->getType())) return -1;
    InstanciaPrefab* ip = (InstanciaPrefab*)o;
    if (pedido == 1)       ip->streamFijado = 1;
    else if (pedido == -1) ip->streamFijado = -1;
    else if (pedido == 2)  ip->streamFijado = 0;
    if (pedido != 0) gListaSucia = true;
    return ip->streamEstado;
}

// (true cuando el programa ya termina: las listas de aca pudieron destruirse antes que el ultimo objeto -el orden de
//  destruccion de lo estatico entre archivos no esta definido-; ver gFinDelPrograma al final del archivo)
static bool gTerminando = false;
void W3dStreamingOlvidarInstancia(InstanciaPrefab* ip) {
    if (!ip || gTerminando) return;
    // (solo las que retienen algo estan en gConPedidos: una que nunca pidio nada no toca nada)
    if (!ip->streamPedidos.empty()) SoltarPedidos(ip);
    for (size_t i = gSoltadas.size(); i-- > 0; ) if (gSoltadas[i].ip == ip) gSoltadas.erase(gSoltadas.begin() + (long)i);
    gListaSucia = true;
}

void W3dStreamingCerrarProyecto() {
    std::set<InstanciaPrefab*> todas;
    todas.swap(gConPedidos);
    for (std::set<InstanciaPrefab*>::iterator it = todas.begin(); it != todas.end(); ++it) {
        InstanciaPrefab* ip = *it;
        std::vector<W3dPedidoStream> v;
        v.swap(ip->streamPedidos);
        for (size_t i = 0; i < v.size(); i++) SoltarUno(v[i]);
        ip->streamFase = 0;
    }
    gSoltadas.clear();
    gLista.clear();
    gListaSucia = true;
    gListaRaiz = NULL;
    g_w3dStreamingVistaPrevia = false;   // (es una opcion del proyecto: el que abre trae la suya)
}

void W3dStreamingInstanciaCambiada(InstanciaPrefab* ip) {
    gListaSucia = true;
    if (!ip) return;
    // dejo de ser diferida (o cambio su distancia) con la vista previa apagada y sin jugar: el editor la quiere
    // generada, como cualquier instancia de siempre
    if (ip->streamEstado != W3D_STREAM_CARGADA && !W3dScriptHayPartida() &&
        (ip->carga != W3D_CARGA_DISTANCIA || !g_w3dStreamingVistaPrevia)) {
        SoltarPedidos(ip);
        GenerarEn(ip);
    }
}

void W3dStreamingVistaPreviaFijar(bool on) {
    g_w3dStreamingVistaPrevia = on;
    gListaSucia = true;
    if (on || W3dScriptHayPartida() || !SceneCollection) return;
    // APAGADA: el editor vuelve a ver el nivel entero (las descargadas se generan) y nada queda retenido de mas
    std::vector<Ficha> v;
    JuntarLista(SceneCollection, v);
    int n = 0;
    for (size_t i = 0; i < v.size(); i++) {
        InstanciaPrefab* ip = v[i].ip;
        if (!W3dObjetoVivoSerial(ip, v[i].serial)) continue;
        SoltarPedidos(ip);
        ip->streamFijado = 0;
        if (ip->streamEstado != W3D_STREAM_CARGADA) { GenerarEn(ip); n++; }
    }
    if (n) w3dLogf("[streaming] vista previa apagada: %d instancia(s) diferida(s) generadas", n);
}

void W3dStreamingRecursosRetenidos(int tipo, std::vector<std::string>& ids) {
    for (std::set<InstanciaPrefab*>::const_iterator it = gConPedidos.begin(); it != gConPedidos.end(); ++it)
        for (size_t i = 0; i < (*it)->streamPedidos.size(); i++) {
            const W3dPedidoStream& p = (*it)->streamPedidos[i];
            if (W3dRecursoVigente(p.r, p.serie) && p.r->tipo == tipo) ids.push_back(p.r->id);
        }
}

void W3dStreamingObjetivoForzado(bool on, const Vector3& pos) {
    gForzado = on;
    gForzadoPos = pos;
}

void W3dStreamingEstadisticas(W3dStreamingStats& st) {
    Reconstruir();
    const W3dStreamingStats acum = gStats;
    st = W3dStreamingStats();
    st.generadasTotal = acum.generadasTotal;
    st.descargadasTotal = acum.descargadasTotal;
    st.tickMs = acum.tickMs;
    st.tickPeorMs = acum.tickPeorMs;
    for (size_t i = 0; i < gLista.size(); i++) {
        const InstanciaPrefab* ip = gLista[i].ip;
        if (!W3dObjetoVivoSerial(ip, gLista[i].serial)) continue;
        st.instancias++;
        if (ip->streamEstado == W3D_STREAM_CARGADA) st.cargadas++;
        else if (ip->streamEstado == W3D_STREAM_PIDIENDO) st.pidiendo++;
        else st.descargadas++;
    }
    for (std::set<InstanciaPrefab*>::const_iterator it = gConPedidos.begin(); it != gConPedidos.end(); ++it)
        st.pedidosVivos += (int)(*it)->streamPedidos.size();
}
void W3dStreamingStatsReset() {
    gStats.generadasTotal = 0;
    gStats.descargadasTotal = 0;
    gStats.tickMs = 0.0;
    gStats.tickPeorMs = 0.0;
}

// el hook de lua (cargar/descargar/cargaAuto/cargado) y el reloj del editor, al arrancar
#ifndef W3D_SIN_EDITOR
extern double W3dNowMs();
#endif
namespace {
// la ULTIMA estatica del archivo: se destruye PRIMERO (orden inverso), antes que las listas de arriba
struct FinDelPrograma { ~FinDelPrograma() { gTerminando = true; } } gFinDelPrograma;
struct RegistrarStreaming {
    RegistrarStreaming() {
        W3dStreamingLuaHook = W3dStreamingPedido;
#ifndef W3D_SIN_EDITOR
        W3dStreamingReloj = W3dNowMs;
#endif
    }
} gRegistrarStreaming;
}
