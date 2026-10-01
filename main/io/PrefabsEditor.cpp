// ============================================================================
//  PrefabsEditor.cpp — ver PrefabsEditor.h. El editor sobre las instancias de
//  prefab: lo generado no se toca, Add > Prefab, soltar un prefab, cambiar el
//  prefab de una instancia, "Create Prefab", "Unpack" y la regeneracion al volver
//  de editar un prefab. Solo editor (C++03).
// ============================================================================
#include "io/PrefabsEditor.h"
#include "io/Prefabs.h"
#include "io/RaicesEditor.h"             // W3dActivarRaiz
#include "io/RecursosProyecto.h"         // W3dRecursosVistaInvalidar (los usuarios de un prefab cambian)
#include "io/CambiosProyecto.h"          // la raiz que cambio queda sucia
#include "W3dRaices.h"
#include "objects/InstanciaPrefab.h"
#include "objects/ProxyW3d.h"           // los PROXIES de las librerias: se crean y se cambian igual que una instancia
#include "io/Librerias.h"
#include "io/Streaming.h"                // COMO SE CARGA una instancia (la tarjeta, con Ctrl+Z)
#include "objects/Objects.h"
#include "objects/ObjectMode.h"          // ReparentKeepTransform / MoverJuntoA / ScaleGlobalDe
#include "objects/Mesh.h"
#include "objects/MallaRecurso.h"   // MallaRecurso::libreria (una malla de una libreria: solo lectura)
#include "objects/Armature.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "animation/Animation.h"
#include "animation/VertexAnimation.h"   // los frames de las vertex anims de un prefab sin guardar
#include "animation/W3dAnimSet.h"        // los clips compartidos de un armature de un prefab sin guardar
#include "base/W3dInteractionState.h"
#include "undo/Undo.h"
#include "ViewPorts/Notificaciones.h"
#include "ViewPorts/Properties.h"        // PropsOlvidarEscena
#include "config/W3dLang.h"
#include "render/OpcionesRender.h"       // g_redraw
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/draw/icons.h"
#include "variables.h"                   // cursor3D
#include "W3dEscena.h"                // W3dEscenaBuscarRef: como resuelve una ref de script (Unpack)
#include "script/W3dScript.h"         // W3dScriptEntrada::refs (Unpack)
#include "w3dlog.h"
#include <cstdio>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

extern bool SimActiva();
extern void DopeSoltarVertexAnim();   // (ViewPorts/Timeline.h) la anim activa cambio: soltar las filas de vertex anim

bool g_w3dPrefabDesempaquetando = false;

// ============================================================================
//  LO QUE LA SERIALIZACION EN SECO NO LLEVA (io/Prefabs.h: W3dPrefabArreglarHook)
// ============================================================================
static void ArreglarGenerado(Object* gen, Object* tpl) {
    if (!gen || !tpl) return;
    if (gen->getType() == ObjectType::armature && tpl->getType() == ObjectType::armature) {
        Armature* ga = (Armature*)gen;
        Armature* ta = (Armature*)tpl;
        // los MISMOS clips en memoria que el armature del prefab (como un Alt+D): 50 instancias = 1 animset
        if (!ta->animations.empty() && ga->animations.empty()) {
            W3dArmatureAnimsVincular(ga, ta);
            ga->animActiva = (ta->animActiva >= 0 && ta->animActiva < (int)ga->animations.size()) ? ta->animActiva : -1;
        }
    } else if (gen->getType() == ObjectType::mesh && tpl->getType() == ObjectType::mesh) {
        Mesh* gm = (Mesh*)gen;
        Mesh* tm = (Mesh*)tpl;
        // las VERTEX ANIMS (sus frames no tienen entrada hasta el guardado): los MISMOS frames que la plantilla
        // (VertexAnimCompartir: pasan al almacen y cada instancia toma una ref; editar la plantilla le hace su
        // copia -COW- y las instancias se regeneran al volver). 50 instancias = 1 juego de frames
        if (gm->animations.empty())
            for (size_t i = 0; i < tm->animations.size(); i++)
                if (tm->animations[i]) NewActiveVertexAnimation(gm, VertexAnimCompartir(tm->animations[i], gm));
    }
}
namespace {
struct RegistrarArreglo { RegistrarArreglo() { W3dPrefabArreglarHook = ArreglarGenerado; } } gRegistrarArreglo;
}

// ============================================================================
//  LO GENERADO NO SE TOCA
// ============================================================================
bool W3dPrefabEsGeneradoAviso(Object* o, bool avisar) {
    if (!o || !W3dEsGenerado(o)) return false;
    if (avisar) Notificar(std::string(T("It belongs to a prefab instance: edit the prefab or unpack the instance")), true);
    return true;
}

// ---- lo que es de una LIBRERIA externa (solo lectura) ----
bool W3dGeneradoPorProxy(const Object* o) {
    for (Object* p = W3dInstanciaDe(o); p; p = W3dInstanciaDe(p))
        if (p->getType() == ObjectType::proxy) return true;
    return false;
}
bool W3dMallaSoloLectura(const Mesh* m, bool recurso) {
    if (!m) return false;
    if (recurso && m->malla && !m->malla->libreria.empty()) return true;
    return W3dGeneradoPorProxy(m);
}
bool W3dMallaSoloLecturaAviso(const Mesh* m, bool recurso) {
    if (!W3dMallaSoloLectura(m, recurso)) return false;
    Notificar(std::string(T("It belongs to a library (read-only): open the library to edit it")), true);
    w3dLogfW("[librerias] '%s' es de una libreria (solo lectura): no se edita", m->name.c_str());
    return true;
}

bool W3dPrefabCruceValido(Object* obj, Object* nuevoPadre) {
    if (!obj || !nuevoPadre) return true;
    if (g_w3dPrefabDesempaquetando) return true;
    const bool sale = W3dEsGenerado(obj);
    const bool entra = W3dEsTipoInstancia(nuevoPadre->getType()) || W3dEsGenerado(nuevoPadre);
    if (!sale && !entra) return true;
    Notificar(std::string(T(sale ? "It belongs to a prefab instance: edit the prefab or unpack the instance"
                                 : "A prefab instance only has what its prefab generates")), true);
    w3dLogfW("[prefabs] reparent bloqueado: '%s' -> '%s' (%s)", obj->name.c_str(), nuevoPadre->name.c_str(),
             sale ? "es generado por una instancia" : "adentro de una instancia");
    return false;
}

// ============================================================================
//  CREAR / CAMBIAR
// ============================================================================
// el prefab que se esta EDITANDO (la raiz activa es un prefab): "" = ninguno
static std::string PrefabEditado() {
    const int a = W3dRaizActiva();
    if (a < 0 || a >= (int)W3dRaices().size()) return std::string();
    const W3dRaizFila& f = W3dRaices()[(size_t)a];
    return f.tipo == W3D_RAIZ_PREFAB ? f.nombre : std::string();
}
// meter una instancia de 'nombre' en lo que se edita armaria un ciclo?
static bool SeriaCiclo(const std::string& nombre) {
    const std::string ed = PrefabEditado();
    return !ed.empty() && W3dPrefabContiene(nombre, ed);
}

bool W3dPrefabSePuedeAgregar(const std::string& nombre, std::string* motivo) {
    if (W3dRaizBuscar(W3D_RAIZ_PREFAB, nombre) < 0) { if (motivo) *motivo = "There is no such prefab"; return false; }
    if (SeriaCiclo(nombre)) { if (motivo) *motivo = "A prefab can't contain itself"; return false; }
    if (InteractionMode != ObjectMode || estado != editNavegacion) { if (motivo) *motivo = "Only in Object Mode"; return false; }
    return true;
}

InstanciaPrefab* W3dPrefabAgregar(const std::string& nombre, const Vector3& pos, std::string* motivo) {
    if (!W3dPrefabSePuedeAgregar(nombre, motivo)) return NULL;
    // editando un PREFAB, la instancia va ADENTRO de su objeto raiz (un prefab anidado: es parte de lo que el
    // prefab genera); suelta al lado no la generaria nadie
    Object* padre = NULL;
    Vector3 local = pos;
    if (!PrefabEditado().empty() && SceneCollection && !SceneCollection->Childrens.empty()) {
        padre = SceneCollection->Childrens[0];
        Matrix4 w; padre->GetWorldMatrixBase(w);
        local = w.Inverse() * pos;
    }
    InstanciaPrefab* ip = W3dPrefabCrearInstancia(nombre, padre, local, 0.0f);
    if (!ip) { if (motivo) *motivo = "There is no such prefab"; return NULL; }
    DeseleccionarTodo();
    ip->Seleccionar();
    UndoCapturarCreacion();   // Ctrl+Z la saca con lo que genero (Ctrl+Y la devuelve)
    W3dRecursosVistaInvalidar();   // (el prefab tiene un usuario mas)
    g_redraw = true;
    w3dLogf("[prefabs] instancia '%s' de '%s' en (%.2f, %.2f, %.2f)", ip->name.c_str(), nombre.c_str(), pos.x, pos.y, pos.z);
    return ip;
}

// ============================================================================
//  REGENERAR UNA INSTANCIA CON Ctrl+Z (cambiarle el prefab, "Reset Overrides")
//
//  Regenerar LIBERA lo que la instancia genero, y el historial puede tener pasos que lo nombran por puntero
//  (mover o elegir un hijo generado, ocultarlo con H): el Ctrl+Z escribia despues en memoria liberada. Por
//  eso ACA lo generado NO se libera: el paso se queda con la generacion vieja DESCOLGADA (como un borrado,
//  que tampoco libera) y su Ctrl+Z la vuelve a colgar, intercambiandola con la nueva, que queda en el paso.
//  Todo objeto que nombra un paso del historial sigue vivo mientras el paso exista y, por LIFO, esta colgado
//  cuando ese paso se aplica: los de abajo nombran la generacion vieja (colgada al deshacer esto) y los de
//  arriba la nueva (colgada al rehacerlo). Lo descolgado se libera cuando el paso se cae del historial.
// ============================================================================
static void JuntarSub(Object* o, std::vector<Object*>& out);   // (el subarbol en preorden: mas abajo)
static InstanciaPrefab* InstanciaPorSerial(Object* o, unsigned serial) {
    if (!o) return NULL;
    if (o->serial == serial && W3dEsTipoInstancia(o->getType())) return (InstanciaPrefab*)o;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        InstanciaPrefab* r = InstanciaPorSerial(o->Childrens[i], serial);
        if (r) return r;
    }
    return NULL;
}
static void QuitarCurvasDe(std::vector<AnimationObject>& v, const std::set<Object*>& s) {
    for (size_t i = v.size(); i-- > 0; ) if (s.count(v[i].obj)) v.erase(v.begin() + (long)i);
}
// las LUCES de un subarbol dejan de ser (o vuelven a ser) de la raiz activa: descolgadas no iluminan
static void LucesFuera(Object* o) {
    if (o->getType() == ObjectType::light)
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == (Light*)o) { Lights.erase(Lights.begin() + (long)i); break; }
    for (size_t i = 0; i < o->Childrens.size(); i++) LucesFuera(o->Childrens[i]);
}
static void LucesDentro(Object* o) {
    if (o->getType() == ObjectType::light) {
        bool ya = false;
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == (Light*)o) { ya = true; break; }
        if (!ya) Lights.push_back((Light*)o);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) LucesDentro(o->Childrens[i]);
}
// DESCUELGA lo que genero la instancia SIN liberarlo: 'fuera' se queda con sus hijos. Lo que los nombra por
// puntero desde afuera del arbol (la seleccion, el activo, la camara y la animacion activas, las curvas de las
// animaciones de escena -las de lo generado no se guardan-, las luces de la raiz) se suelta ahora
static void DescolgarGenerados(InstanciaPrefab* ip, std::vector<Object*>& fuera) {
    fuera.clear();
    if (ip->Childrens.empty()) return;
    W3dClipsVistasSincronizar();   // (la vista de un clip sobre lo generado se escribe en su clip antes de irse)
    std::vector<Object*> sub;
    for (size_t i = 0; i < ip->Childrens.size(); i++) JuntarSub(ip->Childrens[i], sub);
    std::set<Object*> todos(sub.begin(), sub.end());
    for (size_t i = ObjSelects.size(); i-- > 0; )
        if (todos.count(ObjSelects[i])) { ObjSelects[i]->select = false; ObjSelects.erase(ObjSelects.begin() + (long)i); }
    for (size_t i = 0; i < sub.size(); i++) sub[i]->select = false;
    if (todos.count(ObjActivo)) ObjActivo = NULL;
    if (todos.count(CollectionActive)) CollectionActive = SceneCollection;
    if (CameraActive && todos.count((Object*)CameraActive)) CameraActive = NULL;
    if ((ActiveAnimArm && todos.count((Object*)ActiveAnimArm)) || (ActiveAnimMesh && todos.count((Object*)ActiveAnimMesh))) {
        ActiveAnimArm = NULL; ActiveAnimMesh = NULL; ActiveAnimKind = 0;
        DopeSoltarVertexAnim();   // (las filas de vertex anim del dope se resuelven por la anim activa)
    }
    QuitarCurvasDe(AnimationObjects, todos);
    for (size_t e = 0; e < SceneAnimations.size(); e++) if (SceneAnimations[e]) QuitarCurvasDe(SceneAnimations[e]->objetos, todos);
    for (size_t i = 0; i < ip->Childrens.size(); i++) LucesFuera(ip->Childrens[i]);
    fuera.swap(ip->Childrens);
}
// y la vuelve a colgar (el paso deja de tenerla)
static void ColgarGenerados(InstanciaPrefab* ip, std::vector<Object*>& hijos) {
    for (size_t i = 0; i < hijos.size(); i++) {
        hijos[i]->Parent = ip;
        ip->Childrens.push_back(hijos[i]);
        LucesDentro(hijos[i]);
    }
    hijos.clear();
}
// lo de la escena que nombra por NOMBRE a algo generado (una camara que mira a un hijo de la instancia) lo
// vuelve a buscar: ahora es otra generacion
static void RefrescarTrasRegenerar() {
    if (SceneCollection) { SceneCollection->ReloadAll(); W3dConstraintsResolverNombres(SceneCollection); }
    PropsOlvidarEscena();
    W3dRecursosVistaInvalidar();
    g_redraw = true;
}

// el paso: lo guardado es el estado que NO esta puesto (Aplicar lo intercambia con el vivo). La instancia va
// por SERIAL (nunca se recicla): si ya no esta en la raiz activa, el paso no hace nada
struct UndoRegen {
    unsigned serial;
    std::string prefab;
    std::map<std::string, std::string> props;
    std::map<std::string, bool> vis;
    std::vector<InstanciaBase> base;
    unsigned version;
    bool noGenerada;
    std::vector<Object*> hijos;   // la generacion DESCOLGADA (del paso: se libera con el)
    bool liberando;
    UndoRegen() : serial(0), version(0), noGenerada(false), liberando(false) {}
};
static void UndoRegenAplicar(void* d) {
    UndoRegen* u = (UndoRegen*)d;
    InstanciaPrefab* ip = InstanciaPorSerial(SceneCollection, u->serial);
    if (!ip) { w3dLogfW("[prefabs] Ctrl+Z/Ctrl+Y: la instancia del paso ya no esta en la escena (no se hace nada)"); return; }
    std::vector<Object*> ahora;
    DescolgarGenerados(ip, ahora);
    ColgarGenerados(ip, u->hijos);
    u->hijos.swap(ahora);
    std::swap(ip->prefab, u->prefab);
    ip->overProps.swap(u->props);
    ip->overVisible.swap(u->vis);
    ip->base.swap(u->base);
    std::swap(ip->versionGenerada, u->version);
    std::swap(ip->noGenerada, u->noGenerada);
    RefrescarTrasRegenerar();
}
// un objeto se libera de verdad: lo descolgado del paso no puede quedar apuntandolo
static void UndoRegenDesvincular(void* d, Object* borrado) {
    UndoRegen* u = (UndoRegen*)d;
    if (!borrado || u->liberando) return;
    for (size_t i = 0; i < u->hijos.size(); i++) {
        std::vector<Object*> sub;
        JuntarSub(u->hijos[i], sub);
        for (size_t k = 0; k < sub.size(); k++) if (sub[k] != borrado) sub[k]->DesvincularDe(borrado);
    }
}
static void UndoRegenLiberar(void* d) {
    UndoRegen* u = (UndoRegen*)d;
    u->liberando = true;
    for (size_t i = 0; i < u->hijos.size(); i++) W3dLiberarSubarbol(u->hijos[i]);
    delete u;
}

// regenera 'ip' con Ctrl+Z: con otro prefab (nuevo != NULL) y/o sin sus overrides
static void RegenerarConUndo(InstanciaPrefab* ip, const std::string* nuevo, bool sinOverrides) {
    // lo que el usuario cambio en lo generado desde el ultimo cuadro va al paso (el Ctrl+Z lo devuelve)
    W3dPrefabSincronizarOverrides(ip);
    UndoRegen* u = new UndoRegen();
    u->serial = ip->serial;
    u->prefab = ip->prefab;
    u->props = ip->overProps;
    u->vis = ip->overVisible;
    u->base = ip->base;
    u->version = ip->versionGenerada;
    u->noGenerada = ip->noGenerada;
    DescolgarGenerados(ip, u->hijos);
    if (nuevo) ip->prefab = *nuevo;
    if (sinOverrides) { ip->overProps.clear(); ip->overVisible.clear(); }
    W3dPrefabGenerar(ip);   // (ya no tiene hijos: no libera nada)
    UndoExterno f;
    f.aplicar = UndoRegenAplicar; f.desvincular = UndoRegenDesvincular; f.liberar = UndoRegenLiberar;
    UndoPushExterno(f, u);
    RefrescarTrasRegenerar();
}

bool W3dPrefabCambiar(InstanciaPrefab* ip, const std::string& nuevo, std::string* motivo) {
    if (!ip) return false;
    if (ip->getType() == ObjectType::proxy) { if (motivo) *motivo = "It is a W3D proxy: choose its library element"; return false; }
    // (con el Play andando lo generado tiene scripts, cuerpos y fotos de la partida: regenerarlo no se puede)
    if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return false; }
    if (W3dEsGenerado(ip)) { if (motivo) *motivo = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
    if (W3dRaizBuscar(W3D_RAIZ_PREFAB, nuevo) < 0) { if (motivo) *motivo = "There is no such prefab"; return false; }
    if (SeriaCiclo(nuevo)) { if (motivo) *motivo = "A prefab can't contain itself"; return false; }
    if (ip->prefab == nuevo) return true;
    RegenerarConUndo(ip, &nuevo, false);
    return true;
}

bool W3dPrefabResetOverrides(InstanciaPrefab* ip, std::string* motivo) {
    if (!ip) return false;
    if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return false; }
    // (una instancia anidada no guarda overrides propios: los de lo que genera son de la de afuera)
    if (W3dEsGenerado(ip)) { if (motivo) *motivo = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
    W3dPrefabSincronizarOverrides(ip);
    if (ip->overProps.empty() && ip->overVisible.empty()) return true;
    RegenerarConUndo(ip, NULL, true);   // (lo generado vuelve a ser exactamente el prefab)
    return true;
}

// ============================================================================
//  LOS PROXIES (un prefab o una escena de una LIBRERIA externa)
// ============================================================================
bool W3dProxySePuedeAgregar(const std::string& lib, int tipo, const std::string& elem, std::string* motivo) {
    if (InteractionMode != ObjectMode || estado != editNavegacion) { if (motivo) *motivo = "Only in Object Mode"; return false; }
    if (lib.empty()) return true;   // (un proxy vacio: se elige despues en su tarjeta)
    std::string m;
    if (!W3dLibsAsegurar(lib, &m)) { if (motivo) *motivo = m; return false; }
    if (!elem.empty() && !W3dLibsTieneElemento(lib, tipo, elem)) { if (motivo) *motivo = "The library has no such element"; return false; }
    return true;
}

ProxyW3d* W3dProxyAgregar(const std::string& lib, int tipo, const std::string& elem, const Vector3& pos, std::string* motivo) {
    if (!W3dProxySePuedeAgregar(lib, tipo, elem, motivo)) return NULL;
    // editando un PREFAB, el proxy va ADENTRO de su objeto raiz (como una instancia anidada)
    Object* padre = NULL;
    Vector3 local = pos;
    if (!PrefabEditado().empty() && SceneCollection && !SceneCollection->Childrens.empty()) {
        padre = SceneCollection->Childrens[0];
        Matrix4 w; padre->GetWorldMatrixBase(w);
        local = w.Inverse() * pos;
    }
    ProxyW3d* px = W3dProxyCrear(lib, tipo, elem, padre, local, 0.0f);
    DeseleccionarTodo();
    px->Seleccionar();
    UndoCapturarCreacion();        // Ctrl+Z lo saca con lo que genero (Ctrl+Y lo devuelve)
    W3dRecursosVistaInvalidar();
    g_redraw = true;
    w3dLogf("[librerias] proxy '%s' de '%s/%s' en (%.2f, %.2f, %.2f)", px->name.c_str(), lib.c_str(), elem.c_str(), pos.x, pos.y, pos.z);
    return px;
}

bool W3dProxyCambiar(ProxyW3d* px, const std::string& lib, int tipo, const std::string& elem, std::string* motivo) {
    if (!px) return false;
    if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return false; }
    if (W3dEsGenerado(px)) { if (motivo) *motivo = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
    if (!lib.empty()) {
        std::string m;
        if (!W3dLibsAsegurar(lib, &m)) { if (motivo) *motivo = m; return false; }
        if (!elem.empty() && !W3dLibsTieneElemento(lib, tipo, elem)) { if (motivo) *motivo = "The library has no such element"; return false; }
    }
    // la CLAVE nueva (la misma que arma el proxy: una parcial si hay libreria y todavia no elemento)
    const std::string clave = W3dProxyClave(lib, tipo, elem);
    if (clave == px->prefab) return true;
    // (otro elemento: los overrides del anterior no aplican; con Ctrl+Z vuelven)
    RegenerarConUndo(px, &clave, true);
    return true;
}

// las REFERENCIAS DEL PROYECTO a una libreria que no estaba (io/Librerias.h): los objetos con ese serial, en las
// raices cargadas (tambien adentro de lo que genera un proxy: un proxy de OTRA libreria no las tiene, pero no cuesta)
static void JuntarPorSerial(Object* o, const std::set<unsigned>& s, std::vector<Object*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (s.count(h->serial)) out.push_back(h);
        JuntarPorSerial(h, s, out);
    }
}
int W3dLibRefsResolver(const std::string& lib) {
    if (lib.empty() || W3dLibsBuscar(lib) < 0) return 0;
    std::vector<unsigned> sm, sa;
    W3dLibRefsSeriales(lib, W3D_LIBREF_MALLA, sm);
    W3dLibRefsSeriales(lib, W3D_LIBREF_ANIMSET, sa);
    if (sm.empty() && sa.empty()) return 0;
    if (!W3dLibsAsegurar(lib, NULL)) return 0;   // (vinculada pero sin su archivo: siguen pendientes)
    std::set<unsigned> todos(sm.begin(), sm.end());
    todos.insert(sa.begin(), sa.end());
    std::vector<Object*> raices, objs;
    W3dRaicesEnOrden(raices);
    for (size_t r = 0; r < raices.size(); r++) JuntarPorSerial(raices[r], todos, objs);
    int n = 0;
    for (size_t i = 0; i < objs.size(); i++) {
        Object* o = objs[i];
        if (o->getType() == ObjectType::mesh && !((Mesh*)o)->malla) {
            Mesh* m = (Mesh*)o;
            const W3dLibRef* ref = W3dLibRefDe(m, W3D_LIBREF_MALLA);
            MallaRecurso* rec = ref ? W3dMallaRecursoPorNombre(ref->nombre) : NULL;
            if (!rec || !W3dMallaVincular(m, rec)) continue;
            if (!m->modificadores.empty()) m->GenerarMallaModificada();
            W3dLibRefOlvidar(m, W3D_LIBREF_MALLA);
            n++;
        } else if (o->getType() == ObjectType::armature && ((Armature*)o)->animations.empty()) {
            Armature* a = (Armature*)o;
            const W3dLibRef* p = W3dLibRefDe(a, W3D_LIBREF_ANIMSET);
            if (!p) continue;
            const W3dLibRef ref = *p;   // (copia: olvidarla la borra)
            std::string motivo;
            if (!W3dArmatureAsignarAnimSet(a, ref.nombre, ref.conClips ? &ref.clips : NULL, &motivo)) {
                w3dLogfW("[librerias] '%s': el animset '%s' sigue sin poder usarse: %s", a->name.c_str(), ref.nombre.c_str(),
                         motivo.c_str());
                continue;
            }
            W3dLibRefOlvidar(a, W3D_LIBREF_ANIMSET);
            n++;
        }
    }
    if (n) w3dLogf("[librerias] '%s' volvio: %d referencia(s) del proyecto resueltas", lib.c_str(), n);
    return n;
}

// el REGISTRO DE LIBRERIAS cambio para 'lib' (se vinculo, se desvinculo, un Ctrl+Z la devolvio): lo que depende de
// ella se regenera (ver el .h)
int W3dProxiesLibreriaCambio(const std::string& lib, bool activaConUndo) {
    if (lib.empty()) return 0;
    W3dPrefabInvalidarLibreria(lib);
    // (lo que el PROYECTO nombraba de ella y quedo pendiente se resuelve: una malla, un animset)
    const int resueltas = W3dLibRefsResolver(lib);
    // las OTRAS raices cargadas: liberando (cambiar de raiz vacia el historial: ningun paso nombra lo suyo)
    int n = W3dPrefabRegenerarDeLibreria(lib, false);
    // la ACTIVA: un paso por instancia, sin liberar (el Ctrl+Z vuelve a colgar lo que generaba)
    if (activaConUndo && SceneCollection) {
        std::vector<InstanciaPrefab*> v;
        W3dPrefabDependenDeLibreria(SceneCollection, lib, v);
        for (size_t i = 0; i < v.size(); i++) { RegenerarConUndo(v[i], NULL, false); n++; }
    }
    if (n || resueltas) {
        PropsOlvidarEscena();
        W3dRecursosVistaInvalidar();
        g_redraw = true;
        w3dLogf("[librerias] '%s' cambio en el registro: %d proxy(s)/instancia(s) regenerados", lib.c_str(), n);
    }
    return n + resueltas;
}

// ============================================================================
//  VOLVER DE EDITAR UN PREFAB: su definicion cambio -> TODAS sus instancias se regeneran
// ============================================================================
void W3dPrefabDejoDeEditarse(const std::string& nombre) {
    if (nombre.empty()) return;
    W3dPrefabInvalidar(nombre);
    const int n = W3dPrefabRegenerarInstancias(nombre);
    if (n > 0) {
        PropsOlvidarEscena();
        W3dRecursosVistaInvalidar();
        g_redraw = true;
    }
    // lo que quedo SUELTO al lado del objeto raiz (importado, pegado, sacado de el) no es parte del prefab: sus
    // instancias solo generan el objeto raiz y lo que cuelga de el. Se avisa (el archivo lo conserva igual)
    const int idx = W3dRaizBuscar(W3D_RAIZ_PREFAB, nombre);
    Object* raiz = (idx >= 0) ? W3dRaices()[(size_t)idx].raiz : NULL;
    if (raiz && raiz->Childrens.size() > 1) {
        const int sueltos = (int)raiz->Childrens.size() - 1;
        char b[400];
        snprintf(b, sizeof(b), T("Prefab \"%s\": %d object(s) are outside its root object and its instances don't show them"),
                 nombre.c_str(), sueltos);
        Notificar(std::string(b), true);
        w3dLogfW("[prefabs] '%s': %d objeto(s) sueltos al lado del objeto raiz (las instancias no los generan)", nombre.c_str(), sueltos);
    }
}

// ============================================================================
//  Add EDITANDO UN PREFAB: lo nuevo va ADENTRO de su objeto raiz (suelto al lado no lo generaria ninguna
//  instancia), en el mismo lugar del mundo. Una UI (una escena 2D) no: queda como la creo el Add
// ============================================================================
void W3dPrefabAdoptarNuevo(Object* nuevo) {
    if (!nuevo || PrefabEditado().empty() || !SceneCollection || SceneCollection->Childrens.empty()) return;
    if (nuevo->getType() == ObjectType::ui) return;
    Object* raiz = SceneCollection->Childrens[0];
    if (raiz == nuevo) return;   // (un prefab vacio: lo primero que se agrega ES su objeto raiz)
    // solo lo que quedo suelto en el primer nivel (una coleccion activa, un padre elegido: ya estan adentro)
    Object* padre = nuevo->Parent ? nuevo->Parent : SceneCollection;
    if (padre != SceneCollection) return;
    const Vector3 wpos = nuevo->GetGlobalPositionBase();
    const Quaternion wrot = RotGlobalDe(nuevo);
    const Vector3 wesc = ScaleGlobalDe(nuevo);
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++)
        if (SceneCollection->Childrens[i] == nuevo) { SceneCollection->Childrens.erase(SceneCollection->Childrens.begin() + (long)i); break; }
    nuevo->Parent = raiz;
    raiz->Childrens.push_back(nuevo);
    Matrix4 w; raiz->GetWorldMatrixBase(w);
    nuevo->pos = w.Inverse() * wpos;
    const Quaternion prg = RotGlobalDe(raiz);
    nuevo->SetRot(prg.Inverted() * wrot);
    const Vector3 psg = ScaleGlobalDe(raiz);
    nuevo->scale = Vector3(psg.x != 0.0f ? wesc.x / psg.x : wesc.x, psg.y != 0.0f ? wesc.y / psg.y : wesc.y,
                           psg.z != 0.0f ? wesc.z / psg.z : wesc.z);
    w3dLogf("[prefabs] Add editando el prefab: '%s' va adentro de su objeto raiz '%s'", nuevo->name.c_str(), raiz->name.c_str());
}

// ============================================================================
//  UNPACK
// ============================================================================
// una REFERENCIA POR NOMBRE de lo que se desempaqueta que apunta ADENTRO de lo mismo (el "cuerpo" del script de
// SU esqueleto, el target de SU camara): adentro de la instancia se resolvia por su scope; afuera, lo que choca se
// renumera ("Cuerpo" -> "Cuerpo.001") y la referencia tiene que seguir a SU objeto, no al primer homonimo de la
// escena (el cubo del usuario, el cuerpo de otro enemigo desempaquetado antes)
namespace {
struct RefInterna {
    W3dRenameDest dest;   // donde vive el nombre (identidad estable: ver Undo.h)
    Object* dueno;        // el objeto que la tiene
    bool deScript;        // una ref de script (se resuelve como en el Play) o un target/constraint
    Object* obj;          // a quien apunta (de adentro)
    std::string nombre;   // el nombre con el que lo nombra
};
}
static void JuntarRefsInternas(Object* g, std::vector<RefInterna>& out) {
    out.clear();
    std::vector<Object*> sub;
    JuntarSub(g, sub);
    std::set<Object*> dentro(sub.begin(), sub.end());
    for (size_t i = 0; i < sub.size(); i++) {
        Object* o = sub[i];
        if (o->scriptDatos)
            for (size_t s = 0; s < o->scriptDatos->scripts.size(); s++) {
                const std::vector<std::pair<std::string, std::string> >& refs = o->scriptDatos->scripts[s].refs;
                for (size_t r = 0; r < refs.size(); r++) {
                    if (refs[r].second.empty()) continue;
                    Object* t = W3dEscenaBuscarRef(o, refs[r].second);
                    if (!t || !dentro.count(t)) continue;
                    RefInterna ri; ri.dest = W3dDestRefLua(o, (int)s, (int)r); ri.dueno = o; ri.deScript = true;
                    ri.obj = t; ri.nombre = refs[r].second;
                    out.push_back(ri);
                }
            }
        const int n = o->RefsObjeto();
        for (int k = 0; k < n; k++) {
            std::string* p = o->RefObjetoNombre(k);
            if (!p || p->empty()) continue;
            Object* t = o->RefObjeto(k);
            if (!t) t = W3dBuscarNombreDesde(o, *p);
            if (!t || !dentro.count(t) || t->name != *p) continue;
            RefInterna ri; ri.dest = W3dDestNombre(p); ri.dueno = o; ri.deScript = false; ri.obj = t; ri.nombre = *p;
            out.push_back(ri);
        }
    }
}
// despues de la mudanza: cada ref interna pasa a decir el nombre que le quedo a SU objeto (en el mismo paso de
// undo que la mudanza). Devuelve cuantas, igual, resuelven a OTRO objeto (un homonimo de otro scope que la
// busqueda de la escena encuentra antes): se avisa
static int ArrastrarRefsInternas(const std::vector<RefInterna>& refs) {
    std::vector<W3dRenameDest> destinos;
    std::vector<std::string> nuevos;
    for (size_t i = 0; i < refs.size(); i++) {
        if (!W3dObjetoVivo(refs[i].obj) || refs[i].obj->name == refs[i].nombre) continue;
        destinos.push_back(refs[i].dest);
        nuevos.push_back(refs[i].obj->name);
    }
    if (!destinos.empty()) {
        UndoCapturarRenames(destinos);
        for (size_t i = 0; i < destinos.size(); i++)
            if (std::string* p = W3dDestResolver(destinos[i])) *p = nuevos[i];
        w3dLogf("[prefabs] unpack: %d referencia(s) internas siguen a su objeto renumerado", (int)destinos.size());
    }
    int otras = 0;
    for (size_t i = 0; i < refs.size(); i++) {
        Object* t = refs[i].obj;
        if (!W3dObjetoVivo(t) || !W3dObjetoVivo(refs[i].dueno)) continue;
        Object* ahora = refs[i].deScript ? W3dEscenaBuscarRef(refs[i].dueno, t->name) : W3dBuscarNombreDesde(refs[i].dueno, t->name);
        if (ahora != t) {
            otras++;
            w3dLogfW("[prefabs] unpack: la referencia '%s' de '%s' encuentra otro objeto con ese nombre", t->name.c_str(), refs[i].dueno->name.c_str());
        }
    }
    return otras;
}

bool W3dPrefabDesempaquetar(InstanciaPrefab* ip, std::string* motivo) {
    if (!ip) return false;
    // (un PROXY es de solo lectura: lo que genera es de su libreria)
    if (ip->getType() == ObjectType::proxy) { if (motivo) *motivo = "A W3D proxy is read-only: it can't be unpacked"; return false; }
    if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return false; }
    if (InteractionMode != ObjectMode || estado != editNavegacion) { if (motivo) *motivo = "Only in Object Mode"; return false; }
    if (W3dEsGenerado(ip)) { if (motivo) *motivo = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
    // una DIFERIDA que la vista previa del streaming descargo: se genera primero (lo que se desempaqueta es lo suyo)
    if (ip->streamEstado != W3D_STREAM_CARGADA && ip->Childrens.empty()) W3dPrefabGenerar(ip);
    Object* g = ip->RaizGenerada();
    if (!g) { if (motivo) *motivo = "The instance has nothing to unpack"; return false; }
    Object* padre = ip->Parent ? ip->Parent : SceneCollection;
    const std::string nombreG = g->name;
    // las refs por nombre de adentro que apuntan adentro, como se resuelven AHORA (en el scope de la instancia)
    std::vector<RefInterna> internas;
    JuntarRefsInternas(g, internas);
    // UN Ctrl+Z, SECUENCIAL: deshacer devuelve primero la instancia y despues le cuelga otra vez lo suyo
    UndoGrupoIniciar();
    g_w3dPrefabDesempaquetando = true;
    // lo generado sale a donde esta la instancia (mismo lugar en el mundo, en el scope de afuera: lo que choque
    // se renumera) y queda justo antes de ella
    ReparentKeepTransform(g, padre);
    if (g->Parent == padre) MoverJuntoA(g, ip, false);
    g_w3dPrefabDesempaquetando = false;
    const bool salio = (g->Parent == padre);
    const int refsAjenas = salio ? ArrastrarRefsInternas(internas) : 0;
    if (salio && ip->Childrens.empty()) {
        DeseleccionarTodo();
        ip->Seleccionar();
        UndoCapturarBorrado(false);   // la instancia (ya vacia) se va
        // al salir, el nombre del objeto raiz pudo chocar con el de la instancia misma (casi siempre se llaman
        // igual): con la instancia afuera, vuelve a tener el suyo si quedo libre
        if (g->name != nombreG && g->NombreLibre(nombreG) == nombreG) W3dRenombrarObjeto(g, nombreG, false);
    }
    UndoGrupoFinSecuencial();
    if (!salio) { if (motivo) *motivo = "The instance could not be unpacked"; return false; }
    DeseleccionarTodo();
    g->Seleccionar();
    PropsOlvidarEscena();
    W3dRecursosVistaInvalidar();
    g_redraw = true;
    if (refsAjenas > 0) {
        char b[300];
        snprintf(b, sizeof(b), T("%d reference(s) of the unpacked objects now find another object with the same name"), refsAjenas);
        Notificar(std::string(b), true);
    }
    w3dLogf("[prefabs] instancia desempaquetada: '%s' ahora es de la escena", g->name.c_str());
    return true;
}

// ============================================================================
//  CREATE PREFAB (desde la seleccion), con Ctrl+Z
//
//  Es UN paso de undo SECUENCIAL de dos partes:
//    1) la MUDANZA (UndoMudanza, abajo): lo elegido sale de la escena y pasa a ser el contenido de la raiz
//       del prefab nuevo, que entra al registro;
//    2) la INSTANCIA que queda en su lugar: una creacion comun (UndoCapturarCreacion).
//  Ctrl+Z deshace 2 y despues 1: la instancia se va, lo mudado vuelve TAL CUAL (mismo padre, mismo lugar
//  entre sus hermanos, misma transform, sus luces, sus curvas de las animaciones de escena, la camara y el
//  clip activos) y la fila del prefab SALE del registro (W3dRaizSacar: su raiz queda en el paso). Ctrl+Y
//  rehace 1 y despues 2. Lo que se muda son los MISMOS objetos en los dos sentidos, asi los pasos de antes
//  del historial que los nombran siguen valiendo: por LIFO, cuando se deshacen ya volvieron a su lugar.
//  (abrir el prefab u otra raiz vacia el historial, como siempre: el paso nunca aplica en otra raiz activa)
// ============================================================================
static void JuntarSub(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarSub(o->Childrens[i], out);
}
static int IndiceEnPadre(Object* o) {
    Object* p = o->Parent ? o->Parent : SceneCollection;
    for (size_t i = 0; p && i < p->Childrens.size(); i++) if (p->Childrens[i] == o) return (int)i;
    return -1;
}
static void Descolgar(Object* o) {
    Object* p = o->Parent ? o->Parent : SceneCollection;
    if (!p) return;
    for (size_t i = 0; i < p->Childrens.size(); i++) if (p->Childrens[i] == o) { p->Childrens.erase(p->Childrens.begin() + (long)i); break; }
}
// el preorden del arbol activo (para dejar las raices elegidas en el orden de la escena)
static void Preorden(Object* o, std::map<Object*, int>& orden, int& n) {
    if (!o) return;
    orden[o] = n++;
    for (size_t i = 0; i < o->Childrens.size(); i++) Preorden(o->Childrens[i], orden, n);
}
namespace {
struct PorOrden {
    const std::map<Object*, int>* orden;
    bool operator()(Object* a, Object* b) const { return orden->find(a)->second < orden->find(b)->second; }
};

// una RAIZ de lo elegido: de donde sale en la escena y como queda en el prefab
struct RaizMudada {
    Object* obj;
    Object* padreEsc;                                  // su padre en la escena (la raiz de la escena si es de primer nivel)
    int idxEsc;                                        // su lugar entre los hermanos de alla
    Vector3 posEsc, escEsc, eulEsc; Quaternion rotEsc; // su transform en la escena
    Vector3 posPre, escPre, eulPre; Quaternion rotPre; // y en el prefab
    RaizMudada() : obj(0), padreEsc(0), idxEsc(-1) {}
};
struct LuzMudada { Light* luz; int idx; LuzMudada() : luz(0), idx(0) {} };
// una curva de una animacion de escena de lo mudado: la escena va por NOMBRE (unico en su raiz) y 'pos' es
// solo el orden que tenia en su lista (se acota al volver)
struct CurvaMudada { std::string escena; int pos; AnimationObject a; CurvaMudada() : pos(0) {} };

// el paso de undo de la MUDANZA (parte 1 del "Create Prefab")
struct UndoMudanza {
    std::string prefab;                 // la fila del prefab (por nombre: su indice se corre)
    bool fuera;                         // true = deshecha: lo elegido volvio a la escena y la fila la tiene 's'
    bool muerto;                        // uno de sus objetos se libero de verdad: el paso ya no hace nada
    W3dRaizSacada s;
    Object* contenedor;                 // de quien cuelgan en el prefab (el vacio con su nombre; NULL = la raiz misma)
    std::vector<RaizMudada> raices;     // en el orden de la escena
    std::vector<LuzMudada> luces;       // las luces que se fueron (su lugar en Lights de la escena)
    std::vector<CurvaMudada> curvas;    // las curvas de las animaciones de escena de lo que se fue
    Camera* camara;                     // la camara activa, si era de lo que se fue
    bool animSeFue; int animKind; Armature* animArm; Mesh* animMesh;   // el clip activo, si era de lo que se fue
    Object* instancia; unsigned instanciaSerial;   // la instancia que queda en su lugar (sigue al prefab si se renombra)
    std::vector<Object*> selPrev; Object* activoPrev;   // la seleccion de antes (vuelve con Ctrl+Z)
    UndoMudanza() : fuera(false), muerto(false), contenedor(0), camara(0), animSeFue(false), animKind(0),
                    animArm(0), animMesh(0), instancia(0), instanciaSerial(0), activoPrev(0) {}
};
bool PorIdxEsc(const RaizMudada* a, const RaizMudada* b) { return a->idxEsc < b->idxEsc; }
bool PorIdxLuz(const LuzMudada& a, const LuzMudada& b) { return a.idx < b.idx; }
}

// todo lo que se muda (las raices con sus subarboles)
static void JuntarMudado(const UndoMudanza* u, std::vector<Object*>& todos) {
    todos.clear();
    for (size_t i = 0; i < u->raices.size(); i++) JuntarSub(u->raices[i].obj, todos);
}
// la lista VIVA de curvas de la animacion de escena 'nombre' de la raiz activa (NULL = ya no esta)
static std::vector<AnimationObject>* CurvasDeEscenaNombre(const std::string& nombre) {
    for (size_t e = 0; e < SceneAnimations.size(); e++)
        if (SceneAnimations[e] && SceneAnimations[e]->name == nombre)
            return ((int)e == SceneAnimActiva) ? &AnimationObjects : &SceneAnimations[e]->objetos;
    return NULL;
}

// LA ESCENA -> EL PREFAB (hacer y Ctrl+Y). La fila del prefab ya esta en el registro. Las transforms del
// prefab (posPre...) ya estan calculadas; las de la escena, los lugares entre hermanos, las luces y las
// curvas se toman ACA (son las del estado de ahora)
static bool MudarAlPrefab(UndoMudanza* u) {
    const int pidx = W3dRaizBuscar(W3D_RAIZ_PREFAB, u->prefab);
    if (pidx < 0 || !W3dRaices()[(size_t)pidx].raiz) return false;
    Object* praiz = W3dRaices()[(size_t)pidx].raiz;
    Object* dest = u->contenedor ? u->contenedor : praiz;
    std::vector<Object*> todos;
    JuntarMudado(u, todos);
    std::set<Object*> setTodos(todos.begin(), todos.end());
    // lo que la UI y las globales tienen de lo que se va (como un borrado)
    W3dClipsVistasSincronizar();
    PropsOlvidarEscena();
    for (size_t i = ObjSelects.size(); i-- > 0; ) if (setTodos.count(ObjSelects[i])) { ObjSelects[i]->select = false; ObjSelects.erase(ObjSelects.begin() + (long)i); }
    if (setTodos.count(ObjActivo)) ObjActivo = NULL;
    u->camara = ((Object*)CameraActive && setTodos.count((Object*)CameraActive)) ? CameraActive : NULL;
    if (u->camara) CameraActive = NULL;
    u->animSeFue = (ActiveAnimArm && setTodos.count((Object*)ActiveAnimArm)) || (ActiveAnimMesh && setTodos.count((Object*)ActiveAnimMesh));
    if (u->animSeFue) {
        u->animKind = ActiveAnimKind; u->animArm = ActiveAnimArm; u->animMesh = ActiveAnimMesh;
        ActiveAnimArm = NULL; ActiveAnimMesh = NULL;
        if (ActiveAnimKind == 1 || ActiveAnimKind == 3 || ActiveAnimKind == 4) ActiveAnimKind = 0;
    }
    // las curvas de animacion de escena de lo que se va (nombrarian objetos de otra raiz): se las lleva el paso
    u->curvas.clear();
    InitSceneAnimations();
    for (size_t e = 0; e < SceneAnimations.size(); e++) {
        if (!SceneAnimations[e]) continue;
        std::vector<AnimationObject>& v = ((int)e == SceneAnimActiva) ? AnimationObjects : SceneAnimations[e]->objetos;
        int sacadas = 0;
        for (size_t i = 0; i < v.size(); ) {
            if (setTodos.count(v[i].obj)) {
                CurvaMudada c; c.escena = SceneAnimations[e]->name; c.pos = (int)i + sacadas; c.a = v[i];
                u->curvas.push_back(c);
                v.erase(v.begin() + (long)i);
                sacadas++;
            } else i++;
        }
    }
    if (setTodos.count(CollectionActive)) CollectionActive = SceneCollection;
    // las luces que se van dejan de ser de la raiz activa (el tope de 8 de GL es por raiz)
    u->luces.clear();
    for (size_t k = 0; k < Lights.size(); k++)
        if (setTodos.count((Object*)Lights[k])) { LuzMudada l; l.luz = Lights[k]; l.idx = (int)k; u->luces.push_back(l); }
    for (size_t k = Lights.size(); k-- > 0; ) if (setTodos.count((Object*)Lights[k])) Lights.erase(Lights.begin() + (long)k);
    // ---- la mudanza (los lugares entre hermanos, TODOS antes de descolgar ninguno) ----
    for (size_t i = 0; i < u->raices.size(); i++) {
        RaizMudada& r = u->raices[i];
        r.padreEsc = r.obj->Parent ? r.obj->Parent : SceneCollection;
        r.idxEsc = IndiceEnPadre(r.obj);
        r.posEsc = r.obj->pos; r.rotEsc = r.obj->Rot(); r.eulEsc = r.obj->rotEuler; r.escEsc = r.obj->scale;
    }
    for (size_t i = 0; i < u->raices.size(); i++) Descolgar(u->raices[i].obj);
    for (size_t i = 0; i < u->raices.size(); i++) {
        RaizMudada& r = u->raices[i];
        r.obj->Parent = dest;
        dest->Childrens.push_back(r.obj);
        r.obj->pos = r.posPre;
        r.obj->SetRotSnapshot(r.rotPre, r.eulPre);
        r.obj->scale = r.escPre;
    }
    // sus luces pasan a ser del prefab (se encienden cuando se lo edita)
    if (!u->luces.empty()) {
        const int act = W3dRaizActiva();
        if (W3dRaizUsar(pidx)) {
            for (size_t i = 0; i < u->luces.size(); i++) Lights.push_back(u->luces[i].luz);
            W3dRaizUsar(act);
        }
    }
    W3dPrefabInvalidar(u->prefab);   // (su definicion sale de lo que acaba de llegar)
    return true;
}

// EL PREFAB -> LA ESCENA (Ctrl+Z): todo vuelve a donde estaba. La fila del prefab la saca el que llama
static bool MudarALaEscena(UndoMudanza* u) {
    const int pidx = W3dRaizBuscar(W3D_RAIZ_PREFAB, u->prefab);
    if (pidx < 0 || !W3dRaices()[(size_t)pidx].raiz) return false;
    Object* praiz = W3dRaices()[(size_t)pidx].raiz;
    Object* dest = u->contenedor ? u->contenedor : praiz;
    // las luces salen del prefab y vuelven a su lugar en la escena
    if (!u->luces.empty()) {
        const int act = W3dRaizActiva();
        if (W3dRaizUsar(pidx)) {
            for (size_t i = 0; i < u->luces.size(); i++)
                for (size_t k = 0; k < Lights.size(); k++) if (Lights[k] == u->luces[i].luz) { Lights.erase(Lights.begin() + (long)k); break; }
            W3dRaizUsar(act);
        }
        std::vector<LuzMudada> v = u->luces;
        std::sort(v.begin(), v.end(), PorIdxLuz);
        for (size_t i = 0; i < v.size(); i++) {
            const int p = v[i].idx < (int)Lights.size() ? v[i].idx : (int)Lights.size();
            Lights.insert(Lights.begin() + p, v[i].luz);
        }
    }
    // las raices: fuera del prefab, y a su lugar entre los hermanos de la escena (de a una en orden creciente de
    // lugar: cada una deja listo el hueco de la siguiente)
    for (size_t i = 0; i < u->raices.size(); i++) {
        Object* o = u->raices[i].obj;
        for (size_t k = 0; k < dest->Childrens.size(); k++) if (dest->Childrens[k] == o) { dest->Childrens.erase(dest->Childrens.begin() + (long)k); break; }
    }
    std::vector<RaizMudada*> orden;
    for (size_t i = 0; i < u->raices.size(); i++) orden.push_back(&u->raices[i]);
    std::stable_sort(orden.begin(), orden.end(), PorIdxEsc);
    for (size_t i = 0; i < orden.size(); i++) {
        RaizMudada& r = *orden[i];
        Object* p = r.padreEsc;
        r.obj->Parent = p;
        const int at = (r.idxEsc >= 0 && r.idxEsc <= (int)p->Childrens.size()) ? r.idxEsc : (int)p->Childrens.size();
        p->Childrens.insert(p->Childrens.begin() + at, r.obj);
        r.obj->pos = r.posEsc;
        r.obj->SetRotSnapshot(r.rotEsc, r.eulEsc);
        r.obj->scale = r.escEsc;
    }
    // sus curvas vuelven a su animacion de escena (en orden creciente de lugar dentro de cada una)
    InitSceneAnimations();
    for (size_t i = 0; i < u->curvas.size(); i++) {
        std::vector<AnimationObject>* v = CurvasDeEscenaNombre(u->curvas[i].escena);
        if (!v) { w3dLogfW("[prefabs] Ctrl+Z: la animacion de escena '%s' ya no esta (una curva no vuelve)", u->curvas[i].escena.c_str()); continue; }
        const int p = u->curvas[i].pos < (int)v->size() ? u->curvas[i].pos : (int)v->size();
        v->insert(v->begin() + (p < 0 ? 0 : p), u->curvas[i].a);
    }
    u->curvas.clear();
    if (u->camara) CameraActive = u->camara;
    if (u->animSeFue) { ActiveAnimKind = u->animKind; ActiveAnimArm = u->animArm; ActiveAnimMesh = u->animMesh; }
    return true;
}

// lo de la escena que nombra a lo que se mudo (camaras, espejos, instancias, constraints) lo vuelve a buscar
static void RefrescarTrasMudanza() {
    SceneCollection->ReloadAll();
    W3dConstraintsResolverNombres(SceneCollection);
    PropsOlvidarEscena();
    W3dRecursosVistaInvalidar();
    g_redraw = true;
}

static void UndoMudanzaAplicar(void* d) {
    UndoMudanza* u = (UndoMudanza*)d;
    if (u->muerto) { w3dLogfW("[prefabs] Ctrl+Z/Ctrl+Y de 'Create Prefab': un objeto del paso ya no existe (no se hace nada)"); return; }
    if (!u->fuera) {
        // Ctrl+Z: lo mudado vuelve a la escena y la fila del prefab sale del registro (el paso se queda con ella)
        if (!MudarALaEscena(u)) return;
        std::string motivo;
        if (!W3dRaizSacar(W3dRaizBuscar(W3D_RAIZ_PREFAB, u->prefab), &u->s, &motivo))
            w3dLogfW("[prefabs] Ctrl+Z de 'Create Prefab': el prefab '%s' no pudo salir del registro (%s)", u->prefab.c_str(), motivo.c_str());
        else u->fuera = true;
        W3dPrefabInvalidar(u->prefab);
        // la seleccion de antes del "Create Prefab"
        DeseleccionarTodo();
        for (size_t i = 0; i < u->selPrev.size(); i++)
            if (W3dObjetoVivo(u->selPrev[i]) && W3dRaizDe(u->selPrev[i]) == SceneCollection) u->selPrev[i]->Seleccionar();
        if (W3dObjetoVivo(u->activoPrev) && W3dRaizDe(u->activoPrev) == SceneCollection) ObjActivo = u->activoPrev;
    } else {
        // Ctrl+Y: la fila vuelve (con el nombre que tenia, si sigue libre) y lo elegido se muda otra vez
        if (u->fuera) {
            Object* raiz = u->s.fila.raiz;
            if (!W3dRaizDevolver(u->s)) return;
            u->fuera = false;
            const int i = W3dRaizDeObjeto(raiz);
            const std::string quedo = (i >= 0) ? W3dRaices()[(size_t)i].nombre : u->prefab;
            if (quedo != u->prefab && W3dObjetoVivoSerial(u->instancia, u->instanciaSerial))
                ((InstanciaPrefab*)u->instancia)->prefab = quedo;
            u->prefab = quedo;
        }
        DeseleccionarTodo();
        MudarAlPrefab(u);
    }
    RefrescarTrasMudanza();
}
static void UndoMudanzaDesvincular(void* d, Object* borrado) {
    UndoMudanza* u = (UndoMudanza*)d;
    if (!borrado) return;
    for (size_t i = u->selPrev.size(); i-- > 0; ) if (u->selPrev[i] == borrado) u->selPrev.erase(u->selPrev.begin() + (long)i);
    if (u->activoPrev == borrado) u->activoPrev = NULL;
    if (u->instancia == borrado) u->instancia = NULL;
    bool mio = (borrado == u->contenedor || borrado == (Object*)u->camara || borrado == (Object*)u->animArm || borrado == (Object*)u->animMesh);
    for (size_t i = 0; i < u->raices.size() && !mio; i++) mio = (borrado == u->raices[i].obj || borrado == u->raices[i].padreEsc);
    for (size_t i = 0; i < u->luces.size() && !mio; i++) mio = (borrado == (Object*)u->luces[i].luz);
    for (size_t i = 0; i < u->curvas.size() && !mio; i++) mio = (borrado == u->curvas[i].a.obj);
    if (mio && !u->muerto) {
        u->muerto = true;
        w3dLogfW("[prefabs] el paso de 'Create Prefab' de '%s' queda sin efecto: '%s' se libero", u->prefab.c_str(), borrado->name.c_str());
    }
}
static void UndoMudanzaLiberar(void* d) {
    UndoMudanza* u = (UndoMudanza*)d;
    if (u->fuera) W3dRaizSacadaLiberar(u->s);   // (deshecho: la raiz del prefab es del paso)
    delete u;
}

bool W3dPrefabCrearDesdeSeleccion(std::string* nombreOut, std::string* motivo) {
    if (SimActiva()) { if (motivo) *motivo = "Stop the game first"; return false; }
    if (InteractionMode != ObjectMode || estado != editNavegacion) { if (motivo) *motivo = "Only in Object Mode"; return false; }
    if (!SceneCollection) return false;
    // las RAICES de lo elegido (un elegido adentro de otro elegido viaja con el)
    std::vector<Object*> sel;
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || !o->select || o == SceneCollection) continue;
        if (W3dEsGenerado(o)) { if (motivo) *motivo = "It belongs to a prefab instance: edit the prefab or unpack the instance"; return false; }
        if (o->getType() == ObjectType::ui || W3dRaizDe(o) != SceneCollection) continue;
        bool dentro = false;
        for (Object* p = o->Parent; p && !dentro; p = p->Parent) if (p->select) dentro = true;
        if (!dentro) sel.push_back(o);
    }
    if (sel.empty()) { if (motivo) *motivo = "Select the objects of the prefab"; return false; }
    std::map<Object*, int> orden; int n = 0;
    Preorden(SceneCollection, orden, n);
    PorOrden cmp; cmp.orden = &orden;
    std::sort(sel.begin(), sel.end(), cmp);
    const std::string ed = PrefabEditado();
    // (dentro de un prefab: lo elegido pasa a un prefab NUEVO que queda anidado en el que se edita)
    UndoMudanza* u = new UndoMudanza();
    u->selPrev = ObjSelects;
    u->activoPrev = ObjActivo;
    // la instancia va donde estaba el PRIMERO (su padre, su lugar entre hermanos)
    Object* padre = sel[0]->Parent ? sel[0]->Parent : SceneCollection;
    const int idx = IndiceEnPadre(sel[0]);
    // ---- el prefab NUEVO (su raiz, con un vacio como objeto raiz) ----
    const bool uno = (sel.size() == 1);
    const std::string pedido = uno ? sel[0]->name : std::string("Prefab");
    const int pidx = W3dRaizNueva(W3D_RAIZ_PREFAB, pedido);
    const std::string nombre = W3dRaices()[(size_t)pidx].nombre;
    Object* praiz = W3dRaices()[(size_t)pidx].raiz;
    Object* vacio = praiz->Childrens.empty() ? NULL : praiz->Childrens[0];
    u->prefab = nombre;
    // la transform de la instancia: la del objeto (uno solo) o ninguna (varios: quedan donde estaban en el
    // espacio del padre de la instancia)
    Vector3 ipos(0, 0, 0), iesc(1, 1, 1), ieul(0, 0, 0);
    Quaternion irot;
    if (uno) { ipos = sel[0]->pos; irot = sel[0]->Rot(); ieul = sel[0]->rotEuler; iesc = sel[0]->scale; }
    // ---- como queda cada raiz en el prefab ----
    u->raices.resize(sel.size());
    if (uno) {
        // el objeto ES la raiz del prefab (el vacio automatico sobra) y queda en el origen: su transform es la
        // de la instancia
        RaizMudada& r = u->raices[0];
        r.obj = sel[0];
        r.posPre = Vector3(0, 0, 0); r.rotPre = Quaternion(); r.eulPre = Vector3(0, 0, 0); r.escPre = Vector3(1, 1, 1);
        if (vacio) {
            for (size_t k = 0; k < praiz->Childrens.size(); k++) if (praiz->Childrens[k] == vacio) { praiz->Childrens.erase(praiz->Childrens.begin() + (long)k); break; }
            W3dLiberarSubarbol(vacio);
        }
        u->contenedor = NULL;
    } else {
        // (las posiciones de cada raiz en el espacio del PADRE de la instancia, con el mundo de ahora)
        const Quaternion prg = RotGlobalDe(padre);
        const Vector3 psg = ScaleGlobalDe(padre);
        const Vector3 ppg = padre->GetGlobalPositionBase();
        const Quaternion ipr = prg.Inverted();
        for (size_t i = 0; i < sel.size(); i++) {
            RaizMudada& r = u->raices[i];
            r.obj = sel[i];
            const Quaternion rg = RotGlobalDe(sel[i]);
            const Vector3 sg = ScaleGlobalDe(sel[i]);
            const Vector3 d = ipr * (sel[i]->GetGlobalPositionBase() - ppg);
            r.posPre = Vector3(psg.x != 0.0f ? d.x / psg.x : d.x, psg.y != 0.0f ? d.y / psg.y : d.y, psg.z != 0.0f ? d.z / psg.z : d.z);
            // (el euler que deriva SetRot, para que el Ctrl+Y deje exactamente lo mismo)
            const Vector3 eulAntes = sel[i]->rotEuler; const Quaternion rotAntes = sel[i]->Rot();
            sel[i]->SetRot(ipr * rg);
            r.rotPre = sel[i]->Rot(); r.eulPre = sel[i]->rotEuler;
            sel[i]->SetRotSnapshot(rotAntes, eulAntes);
            r.escPre = Vector3(psg.x != 0.0f ? sg.x / psg.x : sg.x, psg.y != 0.0f ? sg.y / psg.y : sg.y, psg.z != 0.0f ? sg.z / psg.z : sg.z);
        }
        u->contenedor = vacio;
    }
    // ---- la mudanza ----
    MudarAlPrefab(u);
    // ---- la instancia en su lugar ----
    ObjSelects.clear(); ObjActivo = NULL;
    InstanciaPrefab* ip = new InstanciaPrefab(padre == SceneCollection ? NULL : padre, ipos);
    Descolgar(ip);
    ip->Parent = padre;
    if (idx >= 0 && idx <= (int)padre->Childrens.size()) padre->Childrens.insert(padre->Childrens.begin() + idx, ip);
    else padre->Childrens.push_back(ip);
    ip->SetNameObj(nombre);
    ip->prefab = nombre;
    if (uno) { ip->SetRotSnapshot(irot, ieul); ip->scale = iesc; }
    W3dPrefabGenerar(ip);
    u->instancia = ip; u->instanciaSerial = ip->serial;
    // lo de la escena que nombraba a lo que se fue (camaras, espejos, instancias, constraints): ahora lo
    // encuentra en lo que genera la instancia (mismos nombres)
    SceneCollection->ReloadAll();
    W3dConstraintsResolverNombres(SceneCollection);
    // las CURVAS de las animaciones de escena de lo que se fue (se las lleva el paso: nombrarian objetos de otra
    // raiz, y las de lo generado no se guardan). Con UN objeto la instancia ocupa su lugar con su transform: sus
    // curvas de transform/visible/render pasan a la INSTANCIA (la animacion de la escena la sigue moviendo); el
    // Ctrl+Z las saca con la instancia y devuelve las originales. Lo demas se AVISA: no se pierde en silencio
    int curvasFuera = 0;
    for (size_t i = 0; i < u->curvas.size(); i++) {
        const CurvaMudada& c = u->curvas[i];
        AnimationObject a = c.a;
        std::vector<AnimProperty> props;
        if (uno && c.a.obj == sel[0])
            for (size_t k = 0; k < a.Propertys.size(); k++) {
                const int p = a.Propertys[k].Property;
                if (p == AnimPosition || p == AnimRotation || p == AnimScale || p == AnimVisible || p == AnimRender)
                    props.push_back(a.Propertys[k]);
            }
        const int total = (int)a.Propertys.size();
        std::vector<AnimationObject>* v = props.empty() ? NULL : CurvasDeEscenaNombre(c.escena);
        if (!v) { curvasFuera += total; continue; }
        curvasFuera += total - (int)props.size();
        a.obj = ip;
        a.Propertys = props;
        a.UpdateFirstLastFrame();
        const int p = (c.pos >= 0 && c.pos < (int)v->size()) ? c.pos : (int)v->size();
        v->insert(v->begin() + p, a);
    }
    if (curvasFuera > 0) {
        char b[300];
        snprintf(b, sizeof(b), T("%d animation curve(s) of the selection no longer animate anything (Ctrl+Z brings them back)"), curvasFuera);
        Notificar(std::string(b), true);
        w3dLogfW("[prefabs] Create Prefab: %d curva(s) de las animaciones de escena quedan en el paso de undo", curvasFuera);
    }
    DeseleccionarTodo();
    ip->Seleccionar();
    // ---- el paso de undo: la mudanza + la creacion de la instancia, UN solo Ctrl+Z (secuencial) ----
    UndoGrupoIniciar();
    UndoExterno f; f.aplicar = UndoMudanzaAplicar; f.desvincular = UndoMudanzaDesvincular; f.liberar = UndoMudanzaLiberar;
    UndoPushExterno(f, u);
    UndoCapturarCreacion();
    UndoGrupoFinSecuencial();
    W3dCambiosTocado();
    W3dRecursosVistaInvalidar();
    g_redraw = true;
    if (nombreOut) *nombreOut = nombre;
    std::vector<Object*> todos;
    JuntarMudado(u, todos);
    w3dLogf("[prefabs] prefab '%s' creado con %d objeto(s) de la seleccion%s; queda una instancia '%s'",
            nombre.c_str(), (int)todos.size(), ed.empty() ? "" : " (anidado en el prefab que se edita)", ip->name.c_str());
    return true;
}

// ============================================================================
//  Add > Prefab
// ============================================================================
static std::vector<std::string> gMenuAdd;   // id - 1 -> el nombre del prefab (se rearma en cada apertura)
// (solo los prefabs DEL PROYECTO: los de una libreria vinculada -io/BibliotecaExterna.h- no se instancian
//  como una InstanciaPrefab sino como un proxy de libreria, de solo lectura; cuando exista, sus filas van
//  aca debajo de las del proyecto, con su propio rango de ids)
void W3dPrefabMenuAddArmar(PopupMenu* m) {
    if (!m) return;
    m->Limpiar();
    gMenuAdd.clear();
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    for (size_t i = 0; i < fs.size(); i++) {
        if (fs[i].tipo != W3D_RAIZ_PREFAB) continue;
        if (SeriaCiclo(fs[i].nombre)) continue;   // (el que se edita y los que lo contienen)
        gMenuAdd.push_back(fs[i].nombre);
        m->Agregar(fs[i].nombre, (int)gMenuAdd.size(), (int)IconType::prefab);
    }
    // los de las LIBRERIAS vinculadas ("Personajes/Enemigo"): crean un PROXY (la fila guarda su clave)
    for (int l = 0; l < W3dLibsCantidad(); l++) {
        const std::string& lib = W3dLibsFila(l).nombre;
        std::vector<std::string> ps;
        W3dLibsElementos(lib, W3D_LIB_PREFAB, ps);
        for (size_t i = 0; i < ps.size(); i++) {
            gMenuAdd.push_back(W3dLibsClave(lib, W3D_LIB_PREFAB, ps[i]));
            m->Agregar(lib + "/" + ps[i], (int)gMenuAdd.size(), (int)IconType::libreria);
        }
    }
    if (gMenuAdd.empty()) m->Agregar(T("No prefabs yet"), 0, -1);
}
std::string W3dPrefabMenuAddNombre(int id) {
    return (id >= 1 && id <= (int)gMenuAdd.size()) ? gMenuAdd[(size_t)(id - 1)] : std::string();
}
void W3dPrefabMenuAddAccion(int id) {
    const std::string n = W3dPrefabMenuAddNombre(id);
    if (n.empty()) return;
    std::string motivo;
    std::string lib, elem; int tipo = W3D_LIB_PREFAB;
    if (W3dLibsDeClave(n, &lib, &tipo, &elem)) {   // (uno de una libreria: un proxy en el cursor 3D)
        if (!W3dProxyAgregar(lib, tipo, elem, cursor3D.pos, &motivo)) Notificar(std::string(T(motivo.c_str())), true);
        return;
    }
    if (!W3dPrefabAgregar(n, cursor3D.pos, &motivo)) Notificar(std::string(T(motivo.c_str())), true);
}

// ============================================================================
//  COMO SE CARGA UNA INSTANCIA (io/Streaming.h): "carga", "distancia" y "objetivo", con Ctrl+Z
// ============================================================================
namespace {
struct UndoCarga { Object* ip; unsigned serial; int carga; float distancia; std::string objetivo; };
}
unsigned g_w3dCargaVersion = 0;
static void UndoCargaAplicar(void* d) {
    UndoCarga* u = (UndoCarga*)d;
    if (!u->ip || !W3dObjetoVivoSerial(u->ip, u->serial)) return;
    InstanciaPrefab* ip = (InstanciaPrefab*)u->ip;
    std::swap(ip->carga, u->carga);
    std::swap(ip->distancia, u->distancia);
    ip->objetivo.swap(u->objetivo);
    g_w3dCargaVersion++;
    W3dStreamingInstanciaCambiada(ip);
    W3dCambiosTocado();
    g_redraw = true;
}
static void UndoCargaDesvincular(void* d, Object* borrado) { UndoCarga* u = (UndoCarga*)d; if (u->ip == borrado) u->ip = NULL; }
static void UndoCargaLiberar(void* d) { delete (UndoCarga*)d; }
void W3dInstanciaCargaUndo(InstanciaPrefab* ip, int carga, float distancia, const std::string& objetivo) {
    if (!ip) return;
    if (ip->carga == carga && ip->distancia == distancia && ip->objetivo == objetivo) return;   // (nada cambio)
    UndoCarga* u = new UndoCarga();
    u->ip = ip; u->serial = ip->serial; u->carga = carga; u->distancia = distancia; u->objetivo = objetivo;
    UndoExterno f; f.aplicar = UndoCargaAplicar; f.desvincular = UndoCargaDesvincular; f.liberar = UndoCargaLiberar;
    UndoPushExterno(f, u);
    g_w3dCargaVersion++;
    W3dStreamingInstanciaCambiada(ip);
    W3dCambiosTocado();
}
void W3dInstanciaCargaCambiar(InstanciaPrefab* ip, int carga, float distancia, const std::string& objetivo) {
    if (!ip) return;
    const int c0 = ip->carga; const float d0 = ip->distancia; const std::string o0 = ip->objetivo;
    ip->carga = (carga == W3D_CARGA_DISTANCIA) ? W3D_CARGA_DISTANCIA : W3D_CARGA_SIEMPRE;
    ip->distancia = (distancia >= 0.0f) ? distancia : 0.0f;
    ip->objetivo = objetivo;
    W3dInstanciaCargaUndo(ip, c0, d0, o0);
    g_redraw = true;
}
