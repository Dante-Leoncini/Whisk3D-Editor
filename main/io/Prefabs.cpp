// ============================================================================
//  Prefabs.cpp — ver Prefabs.h. La definicion de cada prefab (cacheada, con su
//  version), la generacion de los hijos de una instancia (con el lector de
//  siempre, import_w3d), los overrides, el alta de instancias y el instanciar()
//  de lua. Compila en el editor y en el runtime 3D (C++03).
// ============================================================================
#include "io/Prefabs.h"
#include "io/JsonW3d.h"
#include "io/Librerias.h"              // los prefabs y escenas de las LIBRERIAS (lo que generan los proxies)
#include "objects/InstanciaPrefab.h"
#include "objects/ProxyW3d.h"
#include "objects/Objects.h"
#include "animation/Animation.h"         // las curvas que nombran a lo que se suelta
#include "animation/W3dAnimSet.h"        // W3dJerNodo: las rutas de los overrides de visible
#include "script/W3dScript.h"            // las propiedades que DECLARA un script (los overrides de valor)
#include "base/W3dInteractionState.h"    // la generacion no toca la seleccion de Edit/Pose
#include "W3dRaices.h"                   // el registro de prefabs y el cambio de raiz (regenerar en todas)
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <map>
#include <set>

bool (*W3dPrefabSerializarHook)(Object* raizPrefab, std::string& json) = 0;
bool (*W3dEscenaSerializarHook)(Object* raizEscena, const std::string& nombre, std::string& json) = 0;

// ---- ESCENAS COMO PREFAB ("escena:<nombre>", ver Prefabs.h) ----
static const char kPrefijoEscena[] = "escena:";
bool W3dPrefabEsEscena(const std::string& clave, std::string* escena) {
    if (clave.compare(0, sizeof(kPrefijoEscena) - 1, kPrefijoEscena) != 0) return false;
    if (escena) *escena = clave.substr(sizeof(kPrefijoEscena) - 1);
    return true;
}
std::string W3dPrefabClaveEscena(const std::string& escena) { return std::string(kPrefijoEscena) + escena; }
void (*W3dPrefabArreglarHook)(Object* generado, Object* plantilla) = 0;

// ============================================================================
//  LAS DEFINICIONES
// ============================================================================
namespace {
struct DefPrefab {
    JVal* doc;                  // el documento parseado (dueno)
    JVal* raiz;                 // su objeto raiz (adentro de doc); NULL = no se pudo leer
    unsigned version;           // 0 = no se pudo leer
    bool deMemoria;             // salio de la raiz CARGADA del editor (serializacion en seco)
    Object* plantilla;          // (deMemoria) el objeto raiz del prefab del que salio...
    unsigned plantillaSerial;   // ...y su serial (la direccion se puede reciclar)
    std::string libreria;       // la LIBRERIA de la que salio ("" = del proyecto): lo que genera se arma en su
                                // contexto (rutas y nombres de recursos con su prefijo, io/Librerias.h)
    DefPrefab() : doc(0), raiz(0), version(0), deMemoria(false), plantilla(0), plantillaSerial(0) {}
    ~DefPrefab() { delete doc; }
private:
    DefPrefab(const DefPrefab&);
    DefPrefab& operator=(const DefPrefab&);
};
}
static std::map<std::string, DefPrefab*> gDefs;
static unsigned gVersionProx = 1;          // global y monotona: una version nunca se repite (ni entre prefabs)
static std::vector<std::string> gPila;     // los prefabs que se estan generando (anidados): el ciclo se corta
// las propiedades que DECLARA cada .lua (los overrides de valor, mas abajo): se olvidan con las definiciones (un
// script editado en el IDE puede declarar otras)
static std::map<std::string, std::set<std::string> > gDeclaradas;

void W3dPrefabPilaEntrar(const std::string& nombre) { gPila.push_back(nombre); }
void W3dPrefabPilaSalir() { if (!gPila.empty()) gPila.pop_back(); }
static bool EnPila(const std::string& nombre) {
    for (size_t i = 0; i < gPila.size(); i++) if (gPila[i] == nombre) return true;
    return false;
}

static void OlvidarDef(std::map<std::string, DefPrefab*>::iterator it) {
    delete it->second;
    gDefs.erase(it);
}
void W3dPrefabInvalidar(const std::string& nombre) {
    if (nombre.empty()) { W3dPrefabsOlvidarTodo(); return; }
    gDeclaradas.clear();
    std::map<std::string, DefPrefab*>::iterator it = gDefs.find(nombre);
    if (it != gDefs.end()) OlvidarDef(it);
}
void W3dPrefabsOlvidarTodo() {
    for (std::map<std::string, DefPrefab*>::iterator it = gDefs.begin(); it != gDefs.end(); ++it) delete it->second;
    gDefs.clear();
    gPila.clear();
    gDeclaradas.clear();
}

static JVal* Parsear(const char* datos, size_t n, const std::string& quien) {
    JParser p(datos, n);
    JVal* v = p.Valor();
    if (p.error || !v || v->tipo != 4) {
        w3dLogfE("[prefabs] la definicion de '%s' no parsea", quien.c_str());
        delete v;
        return NULL;
    }
    return v;
}

// ---- una ESCENA del proyecto como definicion ----
// lo que es DE LA ESCENA y no viaja con sus instancias (Prefabs.h): las UI 2D, las rutinas constructor y las que
// limpian la pantalla
static bool EsDeLaEscenaJson(JVal* o) {
    if (!o || o->tipo != 4) return false;
    const std::string t = JS(o, "tipo", "");
    if (t == "ui") return true;
    if (t != "rutina") return false;
    if (JB(o, "constructor", false)) return true;
    JVal* pasos = JHijo(o, "pasos", 5);
    if (pasos)
        for (size_t i = 0; i < pasos->lista.size(); i++) {
            const std::string p = JS(pasos->lista[i], "paso", "");
            if (p == "limpiar" || p == "colorLimpieza") return true;
        }
    return false;
}
static JVal* TextoJ(const std::string& t) { JVal* v = new JVal(); v->tipo = 2; v->str = t; return v; }
// el documento {"raiz": {"tipo": "objeto", "nombre": <escena>, "hijos": [...]}} con los objetos de 'objs' (se los
// queda) menos los que son de la escena
static JVal* EnvolverEscena(JVal* objs, const std::string& nombre) {
    JVal* r = new JVal(); r->tipo = 4;
    r->obj["tipo"] = TextoJ("objeto");
    r->obj["nombre"] = TextoJ(nombre);
    JVal* hijos = new JVal(); hijos->tipo = 5;
    if (objs && objs->tipo == 5) {
        for (size_t i = 0; i < objs->lista.size(); i++) {
            if (EsDeLaEscenaJson(objs->lista[i])) delete objs->lista[i];
            else hijos->lista.push_back(objs->lista[i]);
        }
        objs->lista.clear();
    }
    delete objs;
    r->obj["hijos"] = hijos;
    JVal* doc = new JVal(); doc->tipo = 4;
    doc->obj["raiz"] = r;
    return doc;
}
// saca 'clave' de un objeto JSON sin borrarla (pasa a ser del que llama)
static JVal* Arrancar(JVal* o, const char* clave) {
    if (!o || o->tipo != 4) return NULL;
    std::map<std::string, JVal*>::iterator it = o->obj.find(clave);
    if (it == o->obj.end()) return NULL;
    JVal* v = it->second;
    o->obj.erase(it);
    return v;
}
// la escena de su ENTRADA (el juego compilado, o el editor con la escena sin cargar): su .w3de ({"objetos": [...]})
// o, la principal, el "escena" de proyecto.json
static JVal* LeerEscenaDeEntrada(const W3dRaizFila& f, const std::string& nombre) {
    const std::string ruta = f.bloque ? std::string("proyecto.json") : f.entrada;
    if (ruta.empty()) return NULL;
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(ruta, datos) || datos.empty()) {
        w3dLogfE("[prefabs] no pude leer '%s' (la escena '%s')", ruta.c_str(), nombre.c_str());
        return NULL;
    }
    JVal* doc = Parsear((const char*)&datos[0], datos.size(), nombre);
    if (!doc) return NULL;
    JVal* objs = f.bloque ? Arrancar(JHijo(doc, "escena", 4), "objetos") : Arrancar(doc, "objetos");
    delete doc;
    return EnvolverEscena(objs, nombre);
}

// la definicion de 'nombre': la cacheada o la que se lee ahora (de la memoria del editor si su raiz esta
// cargada, si no de su entrada). Nunca NULL para un nombre del registro: una que no se pudo leer queda
// cacheada con version 0 (no se reintenta en cada instancia)
static DefPrefab* Definicion(const std::string& nombre) {
    std::map<std::string, DefPrefab*>::iterator it = gDefs.find(nombre);
    if (it != gDefs.end()) {
        // una de MEMORIA cuya plantilla ya no existe (la raiz del prefab se descargo despues de guardar): lo que
        // la serializacion en seco no lleva ya no se puede tomar de ahi, se relee de su entrada (es lo mismo)
        if (!it->second->deMemoria || W3dObjetoVivoSerial(it->second->plantilla, it->second->plantillaSerial))
            return it->second;
        OlvidarDef(it);
    }
    // UN ELEMENTO DE UNA LIBRERIA ("lib:<libreria>/prefab|escena/<elemento>", lo que genera un proxy o un prefab
    // anidado adentro de una libreria): su definicion sale de la libreria (nunca de la memoria del editor: la
    // libreria no se edita aca). Una que no se puede leer queda cacheada con version 0, como las del proyecto
    if (nombre.compare(0, 4, "lib:") == 0) {
        DefPrefab* d = new DefPrefab();
        std::string lib, elem;
        int tipo = W3D_LIB_PREFAB;
        if (W3dLibsDeClave(nombre, &lib, &tipo, &elem)) {
            d->libreria = lib;
            d->doc = W3dLibsLeerElemento(lib, tipo, elem, &d->raiz);
            if (!d->doc) d->raiz = NULL;
        }
        if (d->raiz) d->version = gVersionProx++;
        gDefs[nombre] = d;
        return d;
    }
    // UNA ESCENA DEL PROYECTO ("escena:<nombre>"): de la memoria del editor si esta cargada, si no de su entrada
    {
        std::string esc;
        if (W3dPrefabEsEscena(nombre, &esc)) {
            DefPrefab* d = new DefPrefab();
            const int ie = W3dRaizBuscar(W3D_RAIZ_ESCENA, esc);   // (la clase escena: escenas y juegos)
            if (ie >= 0) {
                const W3dRaizFila& f = W3dRaices()[(size_t)ie];
                if (f.raiz && W3dEscenaSerializarHook) {
                    std::string json;
                    if (W3dEscenaSerializarHook(f.raiz, esc, json) && !json.empty()) {
                        d->doc = Parsear(json.data(), json.size(), nombre);
                        d->deMemoria = true;
                        d->plantilla = f.raiz;
                        d->plantillaSerial = f.raiz->serial;
                    }
                } else {
                    d->doc = LeerEscenaDeEntrada(f, esc);
                }
            }
            if (d->doc) d->raiz = JHijo(d->doc, "raiz", 4);
            if (d->raiz) d->version = gVersionProx++;
            else w3dLogfW("[prefabs] la escena '%s' no se pudo leer: sus instancias quedan vacias", esc.c_str());
            gDefs[nombre] = d;
            return d;
        }
    }
    const int idx = W3dRaizBuscar(W3D_RAIZ_PREFAB, nombre);
    if (idx < 0) return NULL;
    const W3dRaizFila& f = W3dRaices()[(size_t)idx];
    DefPrefab* d = new DefPrefab();
    Object* objRaiz = (f.raiz && !f.raiz->Childrens.empty()) ? f.raiz->Childrens[0] : NULL;
    if (objRaiz && W3dPrefabSerializarHook) {
        std::string json;
        if (W3dPrefabSerializarHook(objRaiz, json) && !json.empty()) {
            d->doc = Parsear(json.data(), json.size(), nombre);
            d->deMemoria = true;
            d->plantilla = objRaiz;
            d->plantillaSerial = objRaiz->serial;
        }
    } else if (!f.entrada.empty()) {
        std::vector<unsigned char> datos;
        if (w3dFileSystem::ReadFileBytes(f.entrada, datos) && !datos.empty())
            d->doc = Parsear((const char*)&datos[0], datos.size(), nombre);
        else w3dLogfE("[prefabs] no pude leer '%s' (el prefab '%s')", f.entrada.c_str(), nombre.c_str());
    }
    if (d->doc) d->raiz = JHijo(d->doc, "raiz", 4);
    if (d->raiz) d->version = gVersionProx++;
    else w3dLogfW("[prefabs] el prefab '%s' no tiene objeto raiz: sus instancias quedan vacias", nombre.c_str());
    gDefs[nombre] = d;
    return d;
}

unsigned W3dPrefabVersion(const std::string& nombre) {
    DefPrefab* d = Definicion(nombre);
    return d ? d->version : 0;
}

// ============================================================================
//  GENERAR
// ============================================================================
static void JuntarSubarbol(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarSubarbol(o->Childrens[i], out);
}
static void QuitarCurvas(std::vector<AnimationObject>& v, const std::set<Object*>& s) {
    for (size_t i = v.size(); i-- > 0; ) if (s.count(v[i].obj)) v.erase(v.begin() + (long)i);
}

// suelta lo que la instancia genero (sus hijos): lo que los nombra por puntero (seleccion, activo, las curvas
// de las animaciones de escena de la raiz activa) primero, despues se liberan (~Object hace el resto: refs de
// otros objetos, undo, contextos de otras raices, clips que sonaban)
static void SoltarGenerados(InstanciaPrefab* ip) {
    ip->base.clear();
    if (ip->Childrens.empty()) return;
    std::vector<Object*> sub;
    for (size_t i = 0; i < ip->Childrens.size(); i++) JuntarSubarbol(ip->Childrens[i], sub);
    std::set<Object*> todos(sub.begin(), sub.end());
    for (size_t i = ObjSelects.size(); i-- > 0; ) if (todos.count(ObjSelects[i])) ObjSelects.erase(ObjSelects.begin() + (long)i);
    if (todos.count(ObjActivo)) ObjActivo = NULL;
    if (todos.count(CollectionActive)) CollectionActive = SceneCollection;
    QuitarCurvas(AnimationObjects, todos);
    for (size_t e = 0; e < SceneAnimations.size(); e++) if (SceneAnimations[e]) QuitarCurvas(SceneAnimations[e]->objetos, todos);
    std::vector<Object*> hijos;
    hijos.swap(ip->Childrens);
    for (size_t i = 0; i < hijos.size(); i++) W3dLiberarSubarbol(hijos[i]);
}

// la SELECCION del usuario no cambia porque se genere algo (los constructores eligen a cada objeto nuevo)
namespace {
struct GuardaSeleccion {
    std::vector<Object*> sel;
    std::vector<unsigned> seriales;
    Object* activo;
    unsigned activoSerial;
    unsigned serial;
    int modo;
    GuardaSeleccion() : activo(ObjActivo), activoSerial(ObjActivo ? ObjActivo->serial : 0),
                        serial(W3dSeleccionSerial), modo(InteractionMode) {
        sel = ObjSelects;
        for (size_t i = 0; i < sel.size(); i++) seriales.push_back(sel[i] ? sel[i]->serial : 0);
        InteractionMode = ObjectMode;   // (en Edit/Pose, deseleccionar tocaria los vertices/huesos)
    }
    void Reponer(Object* generado) {
        std::vector<Object*> sub;
        JuntarSubarbol(generado, sub);
        for (size_t i = 0; i < sub.size(); i++) sub[i]->select = false;
        ObjSelects.clear();
        for (size_t i = 0; i < sel.size(); i++)
            if (W3dObjetoVivoSerial(sel[i], seriales[i])) { sel[i]->select = true; ObjSelects.push_back(sel[i]); }
        ObjActivo = W3dObjetoVivoSerial(activo, activoSerial) ? activo : NULL;
        W3dSeleccionSerial = serial;
        InteractionMode = modo;
    }
};
}

// el objeto de la PLANTILLA con ese serial (la definicion de memoria anota el serial de cada nodo)
static void MapaSeriales(Object* o, std::map<unsigned, Object*>& out) {
    if (!o) return;
    out[o->serial] = o;
    for (size_t i = 0; i < o->Childrens.size(); i++) MapaSeriales(o->Childrens[i], out);
}

static void TomarBase(InstanciaPrefab* ip);   // (la foto de lo generado antes de los overrides: mas abajo)

bool W3dPrefabGenerar(InstanciaPrefab* ip) {
    if (!ip) return false;
    SoltarGenerados(ip);
    ip->versionGenerada = 0;
    ip->noGenerada = true;
    // (para el streaming, io/Streaming.h, queda CARGADA: tiene lo que su definicion da, aunque sea nada)
    ip->streamEstado = W3D_STREAM_CARGADA;
    if (ip->prefab.empty()) return false;
    if (EnPila(ip->prefab)) {
        w3dLogfW("[prefabs] '%s': el prefab '%s' se contiene a si mismo (queda vacia)", ip->name.c_str(), ip->prefab.c_str());
        return false;
    }
    DefPrefab* d = Definicion(ip->prefab);
    if (!d || !d->raiz) {
        if (!d) w3dLogfW("[prefabs] '%s': no hay un prefab '%s' en el proyecto (queda vacia)", ip->name.c_str(), ip->prefab.c_str());
        else if (!d->libreria.empty() || ip->prefab.compare(0, 4, "lib:") == 0)
            w3dLogfW("[prefabs] '%s': '%s' no se pudo leer de su libreria (queda vacio)", ip->name.c_str(), ip->prefab.c_str());
        return false;
    }
    GuardaSeleccion guarda;
    std::vector<std::pair<Object*, unsigned> > origenes;
    W3dPrefabPilaEntrar(ip->prefab);
    Object* r = NULL;
    {
        // lo de una LIBRERIA se arma en su contexto: sus rutas son entradas de ella y sus recursos van con su prefijo
        W3dLibContextoGuarda ctx(d->libreria);
        r = W3dPrefabConstruirHijos(ip, d->raiz, d->deMemoria ? &origenes : NULL);
    }
    W3dPrefabPilaSalir();
    if (!r) { guarda.Reponer(ip); return false; }
    // lo que se resuelve por nombre, por SCOPE (primero adentro de esta instancia): targets, rieles, constraints
    ip->ReloadAll();
    W3dConstraintsResolverNombres(ip);
    // una definicion de MEMORIA (el editor): lo que la serializacion en seco no lleva sale de la plantilla
    if (d->deMemoria && W3dPrefabArreglarHook && W3dObjetoVivoSerial(d->plantilla, d->plantillaSerial)) {
        std::map<unsigned, Object*> tpl;
        MapaSeriales(d->plantilla, tpl);
        for (size_t i = 0; i < origenes.size(); i++) {
            std::map<unsigned, Object*>::iterator t = tpl.find(origenes[i].second);
            if (t != tpl.end() && origenes[i].first) W3dPrefabArreglarHook(origenes[i].first, t->second);
        }
    }
    guarda.Reponer(ip);
    ip->versionGenerada = d->version;
    ip->noGenerada = false;
    TomarBase(ip);   // (antes de los overrides: los overrides son lo que difiere de esto)
    W3dPrefabAplicarOverrides(ip);
    return true;
}

void W3dPrefabSoltar(InstanciaPrefab* ip) {
    if (!ip) return;
    SoltarGenerados(ip);
    ip->versionGenerada = 0;
    ip->noGenerada = false;   // (no es un prefab que falta: esta descargada)
    ip->streamEstado = W3D_STREAM_DESCARGADA;
}

// ---- LOS RECURSOS QUE PIDE GENERARLA (el streaming los pide antes, io/Streaming.h) ----
static void RecursosDe(const std::string& clave, std::set<std::string>& vistos, std::vector<W3dCargasItem>& out,
                       std::vector<MallaRecurso*>* mallas) {
    if (clave.empty() || !vistos.insert(clave).second) return;   // (cada definicion una vez: tambien corta un ciclo)
    DefPrefab* d = Definicion(clave);
    if (!d || !d->raiz) return;
    std::vector<std::string> anidados;
    {
        // lo de una LIBRERIA se resuelve en su contexto, igual que al generarlo (nombres con prefijo, rutas "lib:")
        W3dLibContextoGuarda ctx(d->libreria);
        W3dPrefabRecursosJson(d->raiz, out, mallas, anidados);
    }
    for (size_t i = 0; i < anidados.size(); i++) RecursosDe(anidados[i], vistos, out, mallas);
}
bool W3dPrefabRecursos(InstanciaPrefab* ip, std::vector<W3dCargasItem>& out, std::vector<MallaRecurso*>* mallas) {
    out.clear();
    if (mallas) mallas->clear();
    if (!ip || ip->prefab.empty()) return false;
    DefPrefab* d = Definicion(ip->prefab);
    if (!d || !d->raiz) return false;
    std::set<std::string> vistos;
    RecursosDe(ip->prefab, vistos, out, mallas);
    return true;
}

// ============================================================================
//  OVERRIDES
// ============================================================================
// las propiedades que DECLARA un .lua (su tabla 'propiedades', gDeclaradas): se leen UNA vez por archivo (leerlas
// corre el script en un estado aparte)
static bool Declara(const std::string& ruta, const std::string& prop) {
    std::map<std::string, std::set<std::string> >::iterator it = gDeclaradas.find(ruta);
    if (it == gDeclaradas.end()) {
        std::vector<W3dScriptProp> props;
        std::set<std::string> s;
        if (W3dScriptLeerPropiedades(ruta, &props))
            for (size_t i = 0; i < props.size(); i++) s.insert(props[i].nombre);
        it = gDeclaradas.insert(std::make_pair(ruta, s)).first;
    }
    return it->second.count(prop) > 0;
}
// el valor de una propiedad en UN script: se pisa si ya tenia uno; si no, se agrega si el script la declara
static void PonerProp(W3dScriptEntrada& e, const std::string& prop, const std::string& valor) {
    for (size_t r = 0; r < e.refs.size(); r++)
        if (e.refs[r].first == prop) { e.refs[r].second = valor; return; }
    if (Declara(e.ruta, prop)) e.refs.push_back(std::make_pair(prop, valor));
}
// (tambien adentro de una instancia ANIDADA: lo que genera es parte de lo que genera esta, y el override de
//  afuera manda sobre los suyos, que ya vienen puestos desde la definicion del prefab de afuera)
static void PonerPropRec(Object* o, const std::string& prop, const std::string& valor) {
    if (!o) return;
    if (o->scriptDatos)
        for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++) PonerProp(o->scriptDatos->scripts[i], prop, valor);
    for (size_t i = 0; i < o->Childrens.size(); i++) PonerPropRec(o->Childrens[i], prop, valor);
}

// la ruta pasa por una instancia ANIDADA que esta DESCARGADA (el streaming todavia no la genero, o la descargo): lo
// que nombra no existe AHORA, pero su override sigue valiendo (se le aplica cuando se genere)
static bool RutaPorAnidadaDescargada(Object* raiz, const std::string& ruta) {
    if (!raiz) return false;
    size_t p = 0;
    while ((p = ruta.find('/', p)) != std::string::npos) {
        Object* n = W3dJerNodo(raiz, ruta.substr(0, p));
        if (!n) return false;
        if (W3dEsTipoInstancia(n->getType()) && n->Childrens.empty()) return true;
        p++;
    }
    return false;
}
// tiene adentro (en lo generado) alguna instancia anidada DESCARGADA?
static bool TieneAnidadaDescargada(Object* o) {
    for (size_t i = 0; o && i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dEsTipoInstancia(h->getType()) && h->Childrens.empty() && !((InstanciaPrefab*)h)->noGenerada) return true;
        if (TieneAnidadaDescargada(h)) return true;
    }
    return false;
}

void W3dPrefabAplicarOverrides(InstanciaPrefab* ip) {
    if (!ip) return;
    Object* raiz = ip->RaizGenerada();
    if (!raiz) return;
    for (std::map<std::string, bool>::const_iterator it = ip->overVisible.begin(); it != ip->overVisible.end(); ++it) {
        Object* n = W3dJerNodo(raiz, it->first);
        if (n) n->visible = it->second;
        else if (!RutaPorAnidadaDescargada(raiz, it->first))   // (una anidada diferida: se le aplica al generarse)
            w3dLogfW("[prefabs] '%s': el override de visible de '%s' no encuentra ese hijo", ip->name.c_str(), it->first.c_str());
    }
    for (std::map<std::string, std::string>::const_iterator it = ip->overProps.begin(); it != ip->overProps.end(); ++it)
        for (size_t i = 0; i < ip->Childrens.size(); i++) PonerPropRec(ip->Childrens[i], it->first, it->second);
}

void W3dPrefabOverrideProp(InstanciaPrefab* ip, const std::string& prop, const std::string& valor) {
    if (!ip || prop.empty()) return;
    if (valor.empty()) { ip->overProps.erase(prop); W3dPrefabGenerar(ip); return; }   // (vuelve el del prefab)
    ip->overProps[prop] = valor;
    for (size_t i = 0; i < ip->Childrens.size(); i++) PonerPropRec(ip->Childrens[i], prop, valor);
}

// TODO lo generado de 'o' hacia abajo, INCLUIDO lo que generan las instancias anidadas: el prefab de afuera los
// trae adentro, asi que un cambio ahi (el ojo de un hijo, un valor de su script) es un override de ESTA instancia
// (la anidada es a su vez un objeto generado: no se escribe y sus overrides propios no tendrian donde guardarse)
static void JuntarScope(Object* o, std::vector<Object*>& out) {
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        out.push_back(h);
        JuntarScope(h, out);
    }
}
// la foto de UN objeto generado como esta ahora (ver InstanciaBase); 'raiz' = el objeto raiz generado de la instancia
static InstanciaBase BaseDe(Object* o, Object* raiz) {
    InstanciaBase b;
    b.o = o; b.serial = o->serial; b.visible = o->visible;
    if (raiz) b.ruta = W3dJerRuta(raiz, o);
    if (o->scriptDatos)
        for (size_t k = 0; k < o->scriptDatos->scripts.size(); k++) {
            b.refs.push_back(o->scriptDatos->scripts[k].refs);
            b.rutas.push_back(o->scriptDatos->scripts[k].ruta);
        }
    return b;
}
// la foto de lo generado ANTES de los overrides (ver InstanciaBase)
static void TomarBase(InstanciaPrefab* ip) {
    ip->base.clear();
    std::vector<Object*> v;
    JuntarScope(ip, v);
    for (size_t i = 0; i < v.size(); i++) ip->base.push_back(BaseDe(v[i], ip->RaizGenerada()));
}

void W3dPrefabAnidadaGenerada(InstanciaPrefab* anidada) {
    if (!anidada || !W3dInstanciaDe(anidada)) return;
    std::vector<Object*> nuevos;
    JuntarScope(anidada, nuevos);   // (lo que genero; la anidada misma ya era de la base de las de afuera)
    // de la mas cercana a la de afuera de todo: la base de cada una es lo generado con los overrides de las de adentro
    // ya puestos (como cuando la de afuera genera todo junto), y despues mandan los suyos
    for (Object* a = W3dInstanciaDe(anidada); a; a = W3dInstanciaDe(a)) {
        InstanciaPrefab* ip = (InstanciaPrefab*)a;
        Object* raiz = ip->RaizGenerada();
        for (size_t i = 0; i < nuevos.size(); i++) {
            InstanciaBase b = BaseDe(nuevos[i], raiz);
            // la entrada MUERTA de esa misma ruta (la generacion anterior de la anidada) deja su lugar
            if (!b.ruta.empty())
                for (size_t k = ip->base.size(); k-- > 0; )
                    if (ip->base[k].ruta == b.ruta && !W3dObjetoVivoSerial(ip->base[k].o, ip->base[k].serial))
                        ip->base.erase(ip->base.begin() + (long)k);
            ip->base.push_back(b);
        }
        W3dPrefabAplicarOverrides(ip);
    }
}
// el valor de la propiedad 'prop' en una lista de refs ("" + false = no la tiene)
static bool ValorDe(const std::vector<std::pair<std::string, std::string> >& refs, const std::string& prop, std::string& out) {
    for (size_t i = 0; i < refs.size(); i++) if (refs[i].first == prop) { out = refs[i].second; return true; }
    return false;
}
// 'o' lo genero 'ip' (directamente, o una instancia anidada adentro de lo que genero)?
static bool GeneradoPor(const Object* o, const InstanciaPrefab* ip) {
    for (Object* i = W3dInstanciaDe(o); i; i = W3dInstanciaDe(i)) if (i == (const Object*)ip) return true;
    return false;
}
// el script de la BASE que corresponde al k-esimo script de ahora: el de la MISMA ruta (y, si el objeto trae dos
// veces el mismo .lua, el de la misma ocurrencia). -1 = la definicion no lo trae (lo agregaron a mano: no es un
// override, lo generado no se guarda)
static int ScriptEnBase(const InstanciaBase& b, const std::vector<W3dScriptEntrada>& ahora, size_t k) {
    int ocurrencia = 0;
    for (size_t i = 0; i < k; i++) if (ahora[i].ruta == ahora[k].ruta) ocurrencia++;
    for (size_t i = 0; i < b.rutas.size() && i < b.refs.size(); i++)
        if (b.rutas[i] == ahora[k].ruta && ocurrencia-- == 0) return (int)i;
    return -1;
}
bool W3dPrefabSincronizarOverrides(InstanciaPrefab* ip) {
    if (!ip || ip->base.empty()) return false;
    // una instancia ANIDADA (la genero otra) no recalcula los suyos: son los que trae la definicion de la de afuera, y
    // lo que el usuario cambia en lo que ella genera es override de la DE AFUERA. Recalcularlos aca se llevaba a la
    // anidada -que no se guarda- un override de la de afuera (el ojo que la de afuera le aplico parecia propio)
    if (W3dEsGenerado(ip)) return false;
    Object* raiz = ip->RaizGenerada();
    std::map<std::string, bool> vis;
    std::map<std::string, std::string> props;
    for (size_t i = 0; i < ip->base.size(); i++) {
        const InstanciaBase& b = ip->base[i];
        if (!W3dObjetoVivoSerial(b.o, b.serial)) {
            // lo que YA NO ESTA (una instancia anidada DIFERIDA que el streaming descargo; lo que un script destruyo
            // jugando) no deja de tener sus overrides: se conservan los que habia (su visible por su ruta, y las
            // propiedades de sus scripts por nombre). Sin esto, guardar con una anidada descargada los perdia
            if (!b.ruta.empty()) {
                std::map<std::string, bool>::const_iterator ov = ip->overVisible.find(b.ruta);
                if (ov != ip->overVisible.end()) vis[ov->first] = ov->second;
            }
            for (size_t k = 0; k < b.refs.size(); k++)
                for (size_t r = 0; r < b.refs[k].size(); r++) {
                    std::map<std::string, std::string>::const_iterator op = ip->overProps.find(b.refs[k][r].first);
                    if (op != ip->overProps.end()) props[op->first] = op->second;
                }
            continue;
        }
        if (!GeneradoPor(b.o, ip)) continue;
        if (b.o->visible != b.visible && raiz) {
            const std::string ruta = W3dJerRuta(raiz, b.o);
            if (!ruta.empty()) vis[ruta] = b.o->visible;
        }
        if (!b.o->scriptDatos) continue;
        const std::vector<W3dScriptEntrada>& scripts = b.o->scriptDatos->scripts;
        for (size_t k = 0; k < scripts.size(); k++) {
            const int kb = ScriptEnBase(b, scripts, k);
            if (kb < 0) continue;
            const std::vector<std::pair<std::string, std::string> >& ahora = scripts[k].refs;
            for (size_t r = 0; r < ahora.size(); r++) {
                std::string antes;
                const bool habia = ValorDe(b.refs[(size_t)kb], ahora[r].first, antes);
                if (!habia || antes != ahora[r].second) props[ahora[r].first] = ahora[r].second;
            }
        }
    }
    // lo de una instancia ANIDADA que el streaming tiene DESCARGADA no esta en la base (nunca se genero, o ya no esta):
    // sus overrides se conservan tal cual (sin esto, guardar con la vista previa prendida los perdia)
    if (raiz) {
        for (std::map<std::string, bool>::const_iterator it = ip->overVisible.begin(); it != ip->overVisible.end(); ++it)
            if (!vis.count(it->first) && RutaPorAnidadaDescargada(raiz, it->first)) vis[it->first] = it->second;
        if (TieneAnidadaDescargada(ip))
            for (std::map<std::string, std::string>::const_iterator it = ip->overProps.begin(); it != ip->overProps.end(); ++it)
                if (!props.count(it->first)) props[it->first] = it->second;
    }
    const bool cambio = (vis != ip->overVisible) || (props != ip->overProps);
    ip->overVisible.swap(vis);
    ip->overProps.swap(props);
    return cambio;
}

// ============================================================================
//  LAS INSTANCIAS DEL PROYECTO
// ============================================================================
// las instancias de 'nombre' en el arbol 'o' ("" = de cualquiera). soloDeAfuera: las que no estan adentro de lo
// generado por otra (regenerar la de afuera ya regenera las de adentro)
static void JuntarInstancias(Object* o, const std::string& nombre, bool soloDeAfuera, std::vector<InstanciaPrefab*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (W3dEsTipoInstancia(h->getType())) {   // (una instancia o un proxy: los dos generan)
            InstanciaPrefab* ip = (InstanciaPrefab*)h;
            if (nombre.empty() || ip->prefab == nombre) out.push_back(ip);
            if (soloDeAfuera) continue;
        }
        JuntarInstancias(h, nombre, soloDeAfuera, out);
    }
}

void W3dPrefabInstancias(const std::string& nombre, std::vector<InstanciaPrefab*>& out) {
    out.clear();
    std::vector<Object*> raices;
    W3dRaicesEnOrden(raices);
    for (size_t r = 0; r < raices.size(); r++) {
        // (las de adentro de un PREFAB cargado cuentan: una instancia anidada es un usuario de verdad; lo que genera
        //  una instancia no: es de ella)
        std::vector<InstanciaPrefab*> v;
        JuntarInstancias(raices[r], nombre, true, v);
        out.insert(out.end(), v.begin(), v.end());
    }
}
int W3dPrefabContarInstancias(const std::string& nombre) {
    std::vector<InstanciaPrefab*> v;
    W3dPrefabInstancias(nombre, v);
    return (int)v.size();
}

// la instancia de AFUERA de todo que genero a 'ip' (ella misma si no es generada)
static InstanciaPrefab* DeAfuera(InstanciaPrefab* ip) {
    Object* top = ip;
    for (Object* i = W3dInstanciaDe(top); i; i = W3dInstanciaDe(i)) top = i;
    return (InstanciaPrefab*)top;
}

int W3dPrefabRegenerarInstancias(const std::string& nombre) {
    const int act = W3dRaizActiva();
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    int n = 0;
    for (size_t k = 0; k < fs.size(); k++) {
        if (!fs[k].raiz) continue;
        std::vector<InstanciaPrefab*> v;
        // (con un nombre: tambien las anidadas adentro de otras instancias; con "", solo las de afuera: las de
        //  adentro se regeneran con su contenedora)
        JuntarInstancias(fs[k].raiz, nombre, nombre.empty(), v);
        if (v.empty()) continue;
        // una anidada adentro de lo que genero OTRA instancia se regenera con la de AFUERA: los overrides de la de
        // afuera pueden tocar lo que la anidada genera (y solo la de afuera los vuelve a poner)
        std::vector<InstanciaPrefab*> tops;
        std::set<InstanciaPrefab*> vistas;
        for (size_t i = 0; i < v.size(); i++) {
            InstanciaPrefab* t = DeAfuera(v[i]);
            if (vistas.insert(t).second) tops.push_back(t);
        }
        // la raiz de la instancia es la ACTIVA mientras se genera (las luces, las animaciones y el nombre libre
        // son por raiz), igual que la carga de una raiz
        if ((int)k != W3dRaizActiva()) W3dRaizUsar((int)k);
        for (size_t i = 0; i < tops.size(); i++) {
            // lo que el usuario cambio en lo generado (el ojo de un hijo, un valor de su script) se anota ANTES de
            // soltarlo: los overrides salen de comparar lo generado con su base, y regenerar suelta las dos cosas
            W3dPrefabSincronizarOverrides(tops[i]);
            W3dPrefabGenerar(tops[i]);
            n++;
        }
    }
    if (act >= 0 && W3dRaizActiva() != act) W3dRaizUsar(act);
    if (n) w3dLogf("[prefabs] %d instancia(s) de '%s' regeneradas", n, nombre.empty() ? "*" : nombre.c_str());
    return n;
}

// la clave nombra un elemento de la libreria 'lib' ("lib:<lib>/prefab|escena/...", tambien la parcial)
static bool ClaveDeLibreria(const std::string& clave, const std::string& lib) {
    return !lib.empty() && clave.size() > lib.size() + 5 && clave.compare(0, 4, "lib:") == 0 &&
           clave.compare(4, lib.size(), lib) == 0 && clave[4 + lib.size()] == '/';
}
void W3dPrefabInvalidarLibreria(const std::string& lib) {
    for (std::map<std::string, DefPrefab*>::iterator it = gDefs.begin(); it != gDefs.end(); ) {
        std::map<std::string, DefPrefab*>::iterator sig = it; ++sig;
        if (ClaveDeLibreria(it->first, lib)) OlvidarDef(it);
        it = sig;
    }
    gDeclaradas.clear();
}
void W3dPrefabDependenDeLibreria(Object* raiz, const std::string& lib, std::vector<InstanciaPrefab*>& out) {
    out.clear();
    std::vector<InstanciaPrefab*> todas;
    JuntarInstancias(raiz, std::string(), false, todas);   // (tambien las de adentro de lo generado)
    std::set<InstanciaPrefab*> vistas;
    for (size_t i = 0; i < todas.size(); i++) {
        if (!ClaveDeLibreria(todas[i]->prefab, lib)) continue;
        InstanciaPrefab* t = DeAfuera(todas[i]);
        if (vistas.insert(t).second) out.push_back(t);
    }
}
int W3dPrefabRegenerarDeLibreria(const std::string& lib, bool tambienActiva) {
    const int act = W3dRaizActiva();
    const std::vector<W3dRaizFila>& fs = W3dRaices();
    int n = 0;
    for (size_t k = 0; k < fs.size(); k++) {
        if (!fs[k].raiz || (!tambienActiva && (int)k == act)) continue;
        std::vector<InstanciaPrefab*> v;
        W3dPrefabDependenDeLibreria(fs[k].raiz, lib, v);
        if (v.empty()) continue;
        if ((int)k != W3dRaizActiva()) W3dRaizUsar((int)k);
        for (size_t i = 0; i < v.size(); i++) {
            W3dPrefabSincronizarOverrides(v[i]);
            W3dPrefabGenerar(v[i]);
            n++;
        }
    }
    if (act >= 0 && W3dRaizActiva() != act) W3dRaizUsar(act);
    if (n) w3dLogf("[librerias] %d instancia(s)/proxy(s) que dependen de '%s' regenerados", n, lib.c_str());
    return n;
}

int W3dPrefabRenombrado(const std::string& viejo, const std::string& nuevo) {
    if (viejo == nuevo) return 0;
    // TODAS las definiciones se olvidan, no solo la del renombrado: la de un prefab que lo CONTIENE (una
    // instancia anidada) tiene en su JSON el nombre viejo ("prefab": "<viejo>"), y cada instancia nueva,
    // duplicada o regenerada de ese contenedor generaria la anidada vacia. Se releen de la memoria (el nodo de
    // la anidada ya se renombro abajo: las raices se cargaron antes del renombre) o de su entrada
    W3dPrefabsOlvidarTodo();
    std::vector<Object*> raices;
    W3dRaicesEnOrden(raices);
    int n = 0;
    for (size_t r = 0; r < raices.size(); r++) {
        std::vector<InstanciaPrefab*> v;
        JuntarInstancias(raices[r], viejo, false, v);
        for (size_t i = 0; i < v.size(); i++) { v[i]->prefab = nuevo; n++; }
    }
    return n;
}

// el prefab 'nombre' tiene adentro (en su definicion, o en la de un prefab anidado) una instancia de 'otro'?
static bool ContieneRec(JVal* j, const std::string& otro, std::set<std::string>& vistos) {
    if (!j || j->tipo != 4) return false;
    if (JS(j, "tipo", "") == "prefab") {
        const std::string p = JS(j, "prefab", "");
        if (p == otro) return true;
        if (!p.empty() && !vistos.count(p)) {
            vistos.insert(p);
            DefPrefab* d = Definicion(p);
            if (d && d->raiz && ContieneRec(d->raiz, otro, vistos)) return true;
        }
    }
    JVal* h = JHijo(j, "hijos", 5);
    if (h) for (size_t i = 0; i < h->lista.size(); i++) if (ContieneRec(h->lista[i], otro, vistos)) return true;
    return false;
}
bool W3dPrefabContiene(const std::string& nombre, const std::string& otro) {
    if (nombre == otro) return true;
    DefPrefab* d = Definicion(nombre);
    if (!d || !d->raiz) return false;
    std::set<std::string> vistos;
    vistos.insert(nombre);
    return ContieneRec(d->raiz, otro, vistos);
}

// ============================================================================
//  CREAR
// ============================================================================
InstanciaPrefab* W3dPrefabCrearInstancia(const std::string& nombre, Object* padre, const Vector3& pos, float rotYGrados) {
    std::string esc;
    const bool esEscena = W3dPrefabEsEscena(nombre, &esc);   // (una ESCENA del proyecto: "escena:<nombre>")
    if (esEscena ? W3dRaizBuscar(W3D_RAIZ_ESCENA, esc) < 0 : W3dRaizBuscar(W3D_RAIZ_PREFAB, nombre) < 0) return NULL;
    InstanciaPrefab* ip = new InstanciaPrefab(padre, pos);   // (el constructor lo cuelga y lo elige)
    ip->SetNameObj(esEscena ? esc : nombre);                  // "Enemigo", "Enemigo.001"... en su scope
    ip->prefab = nombre;
    if (rotYGrados != 0.0f) ip->SetRotEuler(Vector3(0.0f, rotYGrados, 0.0f));
    W3dPrefabGenerar(ip);
    return ip;
}

ProxyW3d* W3dProxyCrear(const std::string& libreria, int tipo, const std::string& elemento, Object* padre,
                        const Vector3& pos, float rotYGrados) {
    ProxyW3d* px = new ProxyW3d(padre, pos);   // (el constructor lo cuelga y lo elige)
    px->SetNameObj(elemento.empty() ? std::string("Proxy") : elemento);
    px->Fijar(libreria, tipo, elemento);
    if (rotYGrados != 0.0f) px->SetRotEuler(Vector3(0.0f, rotYGrados, 0.0f));
    if (!libreria.empty() && !elemento.empty()) W3dPrefabGenerar(px);
    return px;
}

bool W3dProxyNombre(const std::string& nombre, std::string* libreria, int* tipo, std::string* elemento) {
    const size_t barra = nombre.find('/');
    if (barra == std::string::npos || barra == 0 || barra + 1 >= nombre.size()) return false;
    const std::string lib = nombre.substr(0, barra), elem = nombre.substr(barra + 1);
    if (W3dLibsBuscar(lib) < 0) return false;
    int t = W3D_LIB_PREFAB;
    if (!W3dLibsTieneElemento(lib, t, elem)) {
        t = W3D_LIB_ESCENA;
        if (!W3dLibsTieneElemento(lib, t, elem)) return false;
    }
    if (libreria) *libreria = lib;
    if (tipo) *tipo = t;
    if (elemento) *elemento = elem;
    return true;
}

// instanciar() de lua (script/W3dScript.h): en la raiz ACTIVA, sin tocar la seleccion del editor. La partida
// arranca lo nuevo (W3dScriptSetObjetoNuevo). "Libreria/Elemento" (un prefab o una escena de una libreria
// vinculada) crea un PROXY de ese elemento
// la LIBRERIA de lo que genero 'o' ("" = del proyecto): la de la instancia o el proxy mas cercano que la nombra
// (un prefab anidado adentro de una libreria tambien es de ella). Tambien es la de sus SCRIPTS (W3dScriptLibreriaHook)
static std::string LibreriaDeGenerado(Object* o) {
    std::string lib;
    for (Object* i = W3dObjetoVivo(o) ? W3dInstanciaDe(o) : NULL; i && lib.empty(); i = W3dInstanciaDe(i))
        W3dLibsDeClave(((InstanciaPrefab*)i)->prefab, &lib, NULL, NULL);
    return lib;
}

static Object* HookInstanciar(const char* nombre, const float* pos, float rotY) {
    if (!nombre || !SceneCollection) return NULL;
    const Vector3 p = pos ? Vector3(pos[0], pos[1], pos[2]) : Vector3(0, 0, 0);
    std::vector<Object*> sel = ObjSelects;
    Object* activo = ObjActivo;
    const unsigned serial = W3dSeleccionSerial;
    InstanciaPrefab* ip = NULL;
    // un script de una LIBRERIA (su dueno lo genero un proxy suyo, o un prefab anidado adentro de ella) nombra los
    // prefabs de SU libreria: "Bala" es el de ella antes que uno del proyecto
    {
        const std::string libDue = LibreriaDeGenerado(W3dPrefabInstanciarDuenio);
        const std::string n = nombre;
        if (!libDue.empty() && n.find('/') == std::string::npos && W3dLibsTieneElemento(libDue, W3D_LIB_PREFAB, n))
            ip = W3dProxyCrear(libDue, W3D_LIB_PREFAB, n, NULL, p, rotY);
        // "Otra/Rifle" desde un script de una libreria que vincula a "Otra" (una de adentro): la de SU registro
        const size_t barra = n.find('/');
        if (!ip && !libDue.empty() && barra != std::string::npos && barra > 0) {
            const std::string an = W3dLibsNombreAnidado(libDue, n);
            if (!an.empty()) {
                const std::string g = an.substr(0, an.size() - (n.size() - barra)), elem = n.substr(barra + 1);
                if (W3dLibsTieneElemento(g, W3D_LIB_PREFAB, elem)) ip = W3dProxyCrear(g, W3D_LIB_PREFAB, elem, NULL, p, rotY);
                else if (W3dLibsTieneElemento(g, W3D_LIB_ESCENA, elem)) ip = W3dProxyCrear(g, W3D_LIB_ESCENA, elem, NULL, p, rotY);
            }
        }
    }
    if (!ip) ip = W3dPrefabCrearInstancia(nombre, NULL, p, rotY);
    // una ESCENA del proyecto por su nombre ("Auto": si no hay un prefab que se llame asi)
    if (!ip && W3dRaizBuscar(W3D_RAIZ_ESCENA, nombre) >= 0) ip = W3dPrefabCrearInstancia(W3dPrefabClaveEscena(nombre), NULL, p, rotY);
    if (!ip) {
        std::string lib, elem; int tipo = W3D_LIB_PREFAB;
        if (W3dProxyNombre(nombre, &lib, &tipo, &elem)) ip = W3dProxyCrear(lib, tipo, elem, NULL, p, rotY);
    }
    if (!ip) return NULL;
    ip->select = false;
    ObjSelects.clear();
    for (size_t i = 0; i < sel.size(); i++) if (W3dObjetoVivo(sel[i])) { sel[i]->select = true; ObjSelects.push_back(sel[i]); }
    ObjActivo = W3dObjetoVivo(activo) ? activo : NULL;
    W3dSeleccionSerial = serial;
    return ip;
}
namespace {
struct RegistrarHookPrefabs {
    RegistrarHookPrefabs() {
        W3dPrefabInstanciarHook = HookInstanciar;
        W3dScriptLibreriaHook = LibreriaDeGenerado;   // (los scripts de lo que genera un proxy son de su libreria)
    }
} gRegistrarHookPrefabs;
}
