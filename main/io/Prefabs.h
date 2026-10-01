#ifndef W3D_PREFABS_H
#define W3D_PREFABS_H
// ============================================================================
//  Prefabs — las INSTANCIAS de los prefabs del proyecto (objects/InstanciaPrefab.h):
//  la DEFINICION de cada prefab, la GENERACION de los hijos de una instancia, sus
//  OVERRIDES, y el instanciar()/destruir() de lua.
//
//  LA DEFINICION de un prefab es el JSON de su objeto raiz (la "raiz" de su .w3dp), leido una
//  vez y CACHEADO: generar N instancias no relee ni reprocesa nada (las mallas, los animsets,
//  los materiales y las texturas son recursos compartidos: instanciar solo arma los nodos).
//  Sale de:
//    - su ENTRADA prefabs/<slug>.w3dp (el juego compilado, y el editor si el prefab no se abrio);
//    - en el EDITOR, si su raiz esta CARGADA (se abrio para editarlo): de la MEMORIA, con una
//      serializacion "en seco" (sin contenedor, W3dPrefabSerializarHook) -- lo que el usuario
//      cambio sin guardar es lo que ven sus instancias.
//  Cada definicion tiene una VERSION: al volver de editar el prefab (o al renombrarlo/borrarlo) se
//  invalida y TODAS sus instancias se regeneran.
//
//  GENERAR una instancia: se sueltan sus hijos viejos, se arma el objeto raiz del prefab con el
//  MISMO lector de siempre (import_w3d: JsonObjeto) bajo el nodo de la instancia, con los nombres
//  CRUDOS (la instancia es frontera de scope: nada se renumera), se resuelve lo que va por nombre
//  (modArmature, targets, constraints) primero ADENTRO de la instancia, y se aplican sus overrides.
//  Los prefabs anidados (una instancia adentro de un prefab) se generan igual; un prefab que se
//  contiene a si mismo (directa o indirectamente) se corta con un aviso.
//
//  LUA (script/W3dScript.h):
//    instanciar("Enemigo", x, y, z [, rotY])  -> el nodo de la instancia (con sus hijos ya generados,
//        sus scripts cargados e iniciados, sus cuerpos rigidos y sus hitbox participando). Lo que se
//        crea jugando en el EDITOR se borra al Stop.
//    destruir(obj)  -> DIFERIDO al final del frame: descarga los scripts del subarbol, olvida sus
//        cuerpos rigidos / hitbox / pares, lo saca de la lista de scripts y de las fotos del Play,
//        y los punteros que lua tenga guardados dejan de valer (W3dObjetoVivo).
//
//  Compila en el editor y en el runtime 3D (C++03). Motor generico.
// ============================================================================
#include <string>
#include <vector>
#include "math/Vector3.h"
#include "io/W3dRecursos.h"   // W3dCargasItem: lo que el streaming pide antes de generar

class Object;
class InstanciaPrefab;
struct JVal;
struct MallaRecurso;

// ---- LA DEFINICION ----
// la version ACTUAL de la definicion de 'nombre' (0 = no hay tal prefab o no se pudo leer). Lee y cachea.
unsigned W3dPrefabVersion(const std::string& nombre);
// la definicion de 'nombre' cambio: la proxima generacion la vuelve a leer ("" = todas)
void W3dPrefabInvalidar(const std::string& nombre);
// cierre del proyecto: olvida todas las definiciones cacheadas
void W3dPrefabsOlvidarTodo();

// ---- GENERAR ----
// (re)genera los hijos de la instancia desde la definicion ACTUAL de su prefab (suelta los que tenia) y
// aplica sus overrides. true = genero. La instancia tiene que estar colgada de su arbol (su raiz). No
// arranca scripts: la partida lo hace (instanciar() de lua ya lo hace solo).
// OJO: LIBERA lo que la instancia tenia generado. Con un historial de undo que lo pueda nombrar (el editor) se
// regenera con un paso propio que descuelga en vez de liberar (io/PrefabsEditor.h: cambiar el prefab, Reset
// Overrides); aca solo llegan la carga, una instancia recien creada y el cambio de raiz (que vacia el historial).
// No sincroniza los overrides antes: el que regenera algo que el usuario ya pudo tocar lo hace primero.
bool W3dPrefabGenerar(InstanciaPrefab* ip);
// regenera TODAS las instancias de 'nombre' ("" = las de todos) en TODAS las raices cargadas (cada una
// con su raiz como la activa mientras tanto, igual que la carga). Una anidada adentro de lo que genera otra se
// regenera con la de AFUERA. Sincroniza los overrides de cada una antes (lo que el usuario cambio en lo generado
// no se pierde). Libera lo generado (ver W3dPrefabGenerar). Devuelve cuantas regenero.
int  W3dPrefabRegenerarInstancias(const std::string& nombre);
// las instancias de 'nombre' ("" = de cualquiera) en las raices CARGADAS (los "usuarios" del prefab)
void W3dPrefabInstancias(const std::string& nombre, std::vector<InstanciaPrefab*>& out);
int  W3dPrefabContarInstancias(const std::string& nombre);
// el prefab 'viejo' ahora se llama 'nuevo': las instancias de las raices cargadas lo siguen (y se olvidan TODAS
// las definiciones: la de un prefab que lo contiene nombraba al viejo)
int  W3dPrefabRenombrado(const std::string& viejo, const std::string& nuevo);
// aplica los overrides de la instancia a lo que genero (visible por ruta; propiedades de script)
void W3dPrefabAplicarOverrides(InstanciaPrefab* ip);
// los OVERRIDES salen de lo que el usuario cambio en lo generado (tambien lo de las instancias anidadas): el
// visible de un hijo (el ojo, H) y los valores de las propiedades de sus scripts (la tarjeta Scripts; los scripts
// se emparejan por su .lua, uno que la definicion no trae no cuenta). Se recalculan comparando lo generado de
// ahora con como salio de la definicion (InstanciaPrefab::base). Lo llaman el guardado (antes de escribir la
// instancia), la tarjeta de la instancia (para contarlos) y todo lo que regenera o copia una instancia (antes de
// soltar lo generado: regenerar, Shift+D, cambiar el prefab). true = cambiaron
bool W3dPrefabSincronizarOverrides(InstanciaPrefab* ip);
// un override de PROPIEDAD de script: lo anota en la instancia y lo escribe en todos sus scripts que la
// declaran. "" = quitarlo (vuelve el valor del prefab al regenerar)
void W3dPrefabOverrideProp(InstanciaPrefab* ip, const std::string& prop, const std::string& valor);
// true si 'nombre' (un prefab) contiene, directa o indirectamente, una instancia de 'otro' (para no
// armar un ciclo al elegirlo en una instancia que vive adentro de 'otro')
bool W3dPrefabContiene(const std::string& nombre, const std::string& otro);

// ---- EL STREAMING (io/Streaming.h) ----
// los RECURSOS que necesita generar 'ip': los que nombra su definicion y la de los prefabs anidados que se generan
// con ella (uno anidado DIFERIDO no: lo pide su propio streaming) -- las mallas (W3DREC_MALLA), los animsets
// (W3DREC_ANIMSET) y las fuentes de los scripts (W3DREC_SCRIPT), cada uno una vez, en el orden de la definicion.
// 'mallas' (opcional) recibe los recursos de malla (sus materiales dicen que TEXTURAS hacen falta). false = no hay
// definicion (un prefab que falta: la generacion lo avisa)
bool W3dPrefabRecursos(InstanciaPrefab* ip, std::vector<W3dCargasItem>& out, std::vector<MallaRecurso*>* mallas);
// suelta lo que la instancia genero SIN regenerar (queda vacia y sin generar). Es la descarga del streaming cuando
// no corre ninguna partida (la vista previa del editor, el Play que arranca): con la partida andando la descarga
// va por la ruta de destruir() de lua
void W3dPrefabSoltar(InstanciaPrefab* ip);
// una instancia ANIDADA (adentro de lo que genero otra) se genero POR SU CUENTA -el streaming la cargo-: lo nuevo entra
// a la base de las de afuera (lo que el usuario cambie ahi sigue siendo override de ellas) y se le aplican sus overrides
// (mandan sobre los de la anidada). No hace nada con una que no esta adentro de otra
void W3dPrefabAnidadaGenerada(InstanciaPrefab* anidada);

// ---- CREAR una instancia ----
// una instancia NUEVA de 'nombre' colgada de 'padre' (NULL = la raiz activa), en 'pos' (local al padre) y
// girada 'rotYGrados' en Y, con su nombre libre ("Enemigo", "Enemigo.001"...) y sus hijos ya generados.
// NULL = no hay tal prefab (no crea nada).
InstanciaPrefab* W3dPrefabCrearInstancia(const std::string& nombre, Object* padre, const Vector3& pos, float rotYGrados);

// ---- LOS PROXIES (objects/ProxyW3d.h): un prefab o una escena de una LIBRERIA externa ----
// un PROXY nuevo de 'elemento' (W3D_LIB_PREFAB / W3D_LIB_ESCENA de io/Librerias.h) de 'libreria', colgado de 'padre'
// (NULL = la raiz activa) en 'pos' (local al padre) y girado en Y, con su nombre libre y sus hijos ya generados
// (con 'libreria' o 'elemento' vacios queda vacio, sin elegir). Nunca NULL.
class ProxyW3d;
ProxyW3d* W3dProxyCrear(const std::string& libreria, int tipo, const std::string& elemento, Object* padre,
                        const Vector3& pos, float rotYGrados);
// "Libreria/Elemento" (el nombre que acepta instanciar() de lua) -> la libreria vinculada, el tipo y el elemento.
// false = no es un elemento de una libreria vinculada
bool W3dProxyNombre(const std::string& nombre, std::string* libreria, int* tipo, std::string* elemento);
// ---- lo que DEPENDE de una libreria (se vinculo, se desvinculo o cambio su archivo) ----
// olvida las definiciones cacheadas de sus elementos (la proxima generacion las vuelve a leer, o falla si ya no esta)
void W3dPrefabInvalidarLibreria(const std::string& lib);
// las instancias y proxies de AFUERA de 'raiz' cuya generacion depende de 'lib': un proxy suyo, o uno que tiene
// ADENTRO (en lo que genera) un proxy suyo -se regenera el de afuera-
void W3dPrefabDependenDeLibreria(Object* raiz, const std::string& lib, std::vector<InstanciaPrefab*>& out);
// las regenera (LIBERANDO lo generado, como W3dPrefabRegenerarInstancias) en las raices cargadas; sin
// 'tambienActiva', salvo en la activa (el editor la regenera con su paso de undo). Devuelve cuantas
int  W3dPrefabRegenerarDeLibreria(const std::string& lib, bool tambienActiva);

// ---- LA PILA DE GENERACION (prefabs anidados) ----
// el lector marca el prefab cuya raiz esta armando (se abre para editarlo): una instancia de el mismo
// adentro no se genera (seria infinito)
void W3dPrefabPilaEntrar(const std::string& nombre);
void W3dPrefabPilaSalir();

// ---- GANCHOS DEL EDITOR (NULL en el juego compilado) ----
// serializa "en seco" (sin contenedor) el objeto raiz de un prefab CARGADO al JSON de siempre, con un
// "_origen" (el serial del objeto de la plantilla) en cada nodo. false = no se pudo.
extern bool (*W3dPrefabSerializarHook)(Object* raizPrefab, std::string& json);
// despues de generar desde una definicion de MEMORIA, por cada objeto generado con su objeto de la
// PLANTILLA: lo que la serializacion en seco no lleva (los clips compartidos de un armature, los frames de
// las vertex anims) se toma de la plantilla
extern void (*W3dPrefabArreglarHook)(Object* generado, Object* plantilla);

// ---- el LECTOR (importers/import_w3d.cpp) ----
// los recursos que nombra el subarbol del objeto 'j' (ver W3dPrefabRecursos), resueltos como los resuelve el lector
// (en el contexto de libreria que este puesto: nombres con su prefijo, rutas "lib:"). Agrega a 'out' sin repetir
// (por tipo e id) y a 'anidados' las CLAVES de los prefabs anidados que se generan con el (no las diferidas)
void W3dPrefabRecursosJson(JVal* j, std::vector<W3dCargasItem>& out, std::vector<MallaRecurso*>* mallas,
                           std::vector<std::string>& anidados);
// arma el subarbol del objeto 'j' colgado de 'padre' con el lector de siempre (nombres crudos, rutas de
// entrada), resuelve lo pendiente de ESE subarbol (modArmature, targets de modificadores) y devuelve el
// objeto creado (NULL = no se creo). 'origenes' (opcional) recibe cada objeto creado con el "_origen" de su
// nodo (0 si no tiene).
Object* W3dPrefabConstruirHijos(Object* padre, JVal* j, std::vector<std::pair<Object*, unsigned> >* origenes);

#endif
