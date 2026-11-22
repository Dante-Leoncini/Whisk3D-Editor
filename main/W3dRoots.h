#ifndef W3D_ROOTS_H
#define W3D_ROOTS_H
// ============================================================================
//  W3dRoots — las RAICES EDITABLES de un proyecto: sus ESCENAS 3D, sus JUEGOS y sus
//  PREFABS.
//
//  Cada escena, juego y prefab es una RAIZ propia (un objeto Scene). Una sola es la
//  ACTIVA: SceneCollection (lo que se dibuja, se edita, se recorre y se guarda como
//  "la escena"). Las demas viven FUERA del arbol activo y cada una guarda su CONTEXTO
//  mientras no se la mira: seleccion, objeto activo, coleccion activa, camara activa,
//  sus luces (el tope de 8 de GL es por raiz: las de una raiz inactiva no ocupan
//  lugar), sus animaciones de escena (curvas, activa, rango), la animacion elegida del
//  timeline (y el modo: Juego, Mix, un clip) y las capas del Mix de escenas.
//
//  LOS DOS TIPOS DE RAIZ 3D (mismo espacio de nombres: cambiarEscena() las nombra igual):
//    ESCENA  "cine": tiene animacion, keyframes, linea de tiempo y rango de render
//            Inicio/Fin a mano. Se anima y se renderiza. En un juego compilado, pedirla
//            con cambiarEscena() reproduce su animacion (una cinematica) desde el inicio.
//    JUEGO   una simulacion: su estado editable es el "frame 1" (lo que se ve en el editor
//            es el estado inicial). NO tiene keyframes propios: el timeline en "Juego" no
//            los muestra ni deja insertarlos. Play arranca la simulacion (scripts, fisica,
//            particulas, hitbox...) y lo que queda es el CACHE DE ESTADOS (el rewind de
//            SimJuego): se recorre con el cabezal y es lo que se RENDERIZA (el rango de render
//            se adapta a lo cacheado). Sus animaciones de escena son CLIPS que dispara Lua
//            (animEscena("cinematica")): se editan eligiendolos en el timeline, y al volver a
//            "Juego" todo vuelve a su frame 1.
//    PREFAB  (la "clase" que una instancia genera, fase 7): se edita aislado, como escena.
//
//  EL PROYECTO (project.json):
//    "escenas3d": [{"nombre": "Nivel 1", "tipo": "juego"}, {"nombre": "Intro", "tipo": "escena",
//                   "entrada": "scenes/intro.w3de", "carpeta": "Cine"}],
//    "escena3dInicial": "Intro",
//    "prefabs": [{"nombre": "Enemigo", "entrada": "prefabs/enemigo.w3dp", "carpeta": ""}]
//  La raiz SIN entrada es la del bloque "escena" de siempre (la primera): un proyecto
//  viejo es exactamente eso, una sola raiz ("Scene") y sin registro; su tipo sale de su
//  contenido: "juego" si tiene scripts, si no "escena" (asi un proyecto que ya era un juego
//  con cinematicas sigue siendo un juego con sus clips). Cada otra vive en su entrada
//  .w3de = {"nombre", "objetos": [...], "animaciones": {...}, "mix"} (el tipo va en el registro); cada prefab en
//  su .w3dp = {"nombre", "raiz": <objeto con hijos>, "sueltos": [...]?, "animaciones"?}.
//  "raiz" es el objeto que una instancia (fase 7) va a generar; en el editor el prefab se
//  edita AISLADO: su raiz Scene tiene como hijo ese objeto (lo que el usuario deje suelto
//  al lado, al primer nivel, se conserva en "sueltos").
//
//  CARGA PEREZOSA: al abrir se arma solo la raiz del bloque (y la de la sesion si era
//  otra); el resto se carga desde su entrada recien cuando se la abre (editor) o se la
//  pide con cambiarEscena() (juego). En el EDITOR, guardar y las operaciones sobre los
//  recursos del proyecto (renombrar/borrar/purgar) cargan antes TODAS las raices: las
//  referencias entre objetos y recursos van por NOMBRE, y una escena sin abrir quedaria
//  nombrando lo que ya no existe (ver W3dRootsLoadAll).
//
//  EL JUEGO: cambiarEscena(nombre) tambien acepta escenas y juegos 3D (W3dScene.h le pasa
//  el pedido a este modulo). Se aplica al final del frame con el callback que registra cada
//  build: el runtime compilado DESCARGA la raiz actual (destruye su arbol) y CARGA la
//  pedida desde su entrada; el Play del editor cambia de raiz sin destruir nada y el Stop
//  vuelve a la de antes con todo como estaba. Un JUEGO arranca su simulacion; una ESCENA
//  ademas reproduce su animacion desde el inicio (W3dRootStartCinematic).
//
//  Motor generico, C++03 (Symbian). Compila en el editor y en el runtime 3D; el editor
//  le pone la UI encima en io/RaicesEditor.
// ============================================================================
#include <string>
#include <vector>

class Object;
class Scene;
class SceneAnimation;
struct JVal;
struct W3dContextRoot;   // lo que guarda una raiz mientras no es la activa (W3dRoots.cpp)

// (los valores van al CONTEXTO y a las pruebas, nunca a un archivo: ahi van por su clave)
enum W3dRootType { W3D_ROOT_SCENE = 0, W3D_ROOT_PREFAB = 1, W3D_ROOT_GAME = 2 };
// la CLASE de un tipo: escenas y juegos comparten el registro "escenas3d" y el espacio de nombres
// (cambiarEscena() nombra cualquiera de las dos); los prefabs van aparte
inline int  W3dRootClass(int type) { return type == W3D_ROOT_PREFAB ? W3D_ROOT_PREFAB : W3D_ROOT_SCENE; }
inline bool W3dRootEs3D(int type) { return type == W3D_ROOT_SCENE || type == W3D_ROOT_GAME; }
const char* W3dRootTypeKey(int type);                  // "escena" / "juego" / "prefab" (archivo y log)
int         W3dRootTypeByKey(const std::string& key, int defaultValue);

// una fila del registro (una escena o un prefab del proyecto)
struct W3dRootRow {
    int type;               // W3dRootType
    bool typeExplicit;      // el archivo dijo su tipo; si no (proyecto viejo) sale de su contenido al cargarla
    // el ARCHIVO escribio su tipo (el registro "escenas3d" lo traia). Entonces se vuelve a escribir siempre: el tipo que se
    // deduce depende de lo que haya cargado (sin una libreria en disco sus proxies abren vacios, sin scripts), y si solo
    // se escribiera cuando difiere, una escena con scripts de una libreria que falto volvia como juego al reabrirla
    bool typeInFile;
    std::string name;       // unico entre las de su CLASE (escenas + juegos / prefabs)
    std::string entry;      // su entrada del contenedor ("" = el bloque "escena" o todavia sin guardar)
    std::string folder;     // carpeta COSMETICA del outliner
    Object* root;           // su raiz Scene; NULL = no se cargo todavia (perezosa)
    Scene* scene;           // la misma raiz vista como Scene (fondo/niebla: el global 'scene')
    W3dContextRoot* ctx;    // su contexto guardado (NULL mientras es la activa o si no cargo)
    bool block;             // la escena del bloque "escena" de project.json (hay UNA)
    bool loadFailed;        // su entrada no se pudo leer: no se guarda encima (se conserva el archivo)
    W3dRootRow() : type(W3D_ROOT_SCENE), typeExplicit(false), typeInFile(false), root(0), scene(0), ctx(0), block(false),
                    loadFailed(false) {}
};
// (el ORDEN de las filas es el del proyecto: primero las escenas y los juegos, despues los prefabs. Es el
//  orden en que project.json las escribe y las lee, asi re-guardar sin cambios recorre las raices -sus
//  mallas, sus animsets, sus texturas- en el mismo orden y da los mismos bytes)

// ---- EL REGISTRO -----------------------------------------------------------
// todas las filas, en el orden del proyecto. Si no hay ninguna, se arma la de la raiz
// actual (una escena "Scene", la del bloque): un proyecto viejo es eso.
const std::vector<W3dRootRow>& W3dRoots();
int  W3dActiveRoot();                                      // indice de la activa (-1 = sin raiz)
// la CAMARA ACTIVA de una raiz (un Camera*): la de la activa es CameraActive; la de otra, la que guarda
// su contexto. NULL = no eligio ninguna o no esta cargada (la miniatura de la biblioteca mira por ella)
Object* W3dRootActiveCamera(int idx);
// -1 = no esta. Busca en la CLASE de 'tipo': W3D_ROOT_SCENE o W3D_ROOT_GAME encuentran cualquier
// escena o juego con ese nombre (comparten nombres); W3D_ROOT_PREFAB, los prefabs
int  W3dRootFind(int type, const std::string& name);
// la escena (o el juego) del bloque "escena". Si se borro, la primera escena o juego pasa a serlo
int  W3dRootBlock();
int  W3dObjectRoot(Object* root);                        // la fila de esa raiz (-1)
// las raices CARGADAS en el orden del REGISTRO (no el de cual esta activa): lo que el guardado
// recorre para escribir listas del proyecto (mallas, animsets) siempre en el mismo orden
void W3dRootsInOrder(std::vector<Object*>& out);
const char* W3dRootDefaultName(int type);             // "Scene" / "Game" / "Prefab" (en ingles: es un dato)
// nombre libre entre las de la CLASE de 'tipo' ('excepto' conserva el suyo; -1 = ninguna)
std::string W3dRootFreeName(int type, const std::string& base, int except);
// hay algo que escribir en project.json? (mas de una fila, o la unica con otro nombre, carpeta,
// inicial o un tipo distinto del que se deduciria de su contenido). Un proyecto de UNA raiz sin
// tocar sale byte a byte como antes.
bool W3dRootsHasRegistry();

// ---- EL TIPO (escena / juego) ---------------------------------------------------
// El tipo de una fila es EXPLICITO si lo dijo el archivo, si la raiz nacio con "New Scene"/"New
// Game" o si se convirtio a mano. Si no (un proyecto viejo, o el de arranque del editor), es el que
// se DEDUCE de su contenido en ese momento: agregarle el primer script lo vuelve un juego.
int  W3dRootTypeOf(int idx);                               // el tipo EFECTIVO de la fila (-1 = no existe)
int  W3dRootActiveType();                                  // el de la activa
inline bool W3dActiveRootIsGame() { return W3dRootActiveType() == W3D_ROOT_GAME; }
// el tipo que se DEDUCE del contenido de una raiz 3D (proyectos viejos): JUEGO si algun objeto
// tiene scripts, si no ESCENA
int  W3dRootTypeDeduced(Object* root);
void W3dRootsSetTypesDeduced();   // deducir el tipo de las escenas sin tipo UNA vez y dejarlo fijo (al abrir)
// convierte una escena en juego o al reves (los prefabs no). Si es la ACTIVA, el modo del timeline
// pasa al de su tipo nuevo (ver W3dRootModeByType). false = no es una raiz 3D.
bool W3dRootSetType(int idx, int type);
// el MODO del timeline que corresponde al tipo de la raiz activa: un JUEGO en "Juego" (kind 2,
// AnimIsGame); una escena o un prefab nunca en "Juego" (si lo estaba, pasa a su animacion de
// escena). 'forzar' = aunque el juego estuviera editando un clip (vuelve a "Juego").
void W3dRootModeByType(bool force);

// ---- LAS ESCENAS ABIERTAS EN EL EDITOR (varias a la vez) -------------------------
// Las escenas y juegos TILDADOS en la lista: estan en memoria, se ven juntos en el viewport y cada una es un nodo del
// outliner con su ojo. Una es la ACTIVA (SceneCollection: ahi van los objetos nuevos). El ORDEN de la lista es su
// prioridad: el Play del editor arranca con la primera y carga las demas encima (cargarEscena). Las que no estan
// abiertas no ocupan memoria hasta que se abren. Se guarda en el proyecto ("escenasAbiertas" / "escenasOcultas": cosa
// del editor, el juego no las mira). Con un prefab activo (se edita aislado) la lista es solo el.
void W3dOpenRoots(std::vector<int>& out);       // las filas abiertas, en orden (la activa incluida)
bool W3dRootIsOpen(int idx);
bool W3dRootOpen(int idx);                       // la carga si hace falta y la agrega al final de la lista
void W3dRootRemoveFromOpen(int idx);             // la saca de la lista (no la descarga: eso es W3dRootsUnload)
bool W3dRootMoveOpen(int idx, int pos);          // la pone en la posicion 'pos' de la lista
bool W3dOpenRootVisible(int idx);                // el ojo del outliner
// las OTRAS abiertas que se ven (cargadas y con el ojo prendido), en su orden: el editor las dibuja y las pickea junto a
// la activa
void W3dOtherVisibleRoots(std::vector<Object*>& out);
void W3dRootSetOpenVisible(int idx, bool v);
void W3dOpenRootsLoad();                     // al abrir el proyecto: carga las que quedaron abiertas
void W3dOpenRootsWrite(std::string& s);      // las dos claves para project.json (vacio si no hace falta)

// ---- ESCENA INICIAL (la que arranca el juego compilado; "" = la del bloque) ----
const std::string& W3dInitialRoot();
bool W3dRootSetInitial(const std::string& name);       // false = no es una escena del proyecto
int  W3dInitialRootIdx();                              // la fila (la del bloque si no hay)

// ---- CARGA -------------------------------------------------------------------
// carga la raiz desde su entrada (la activa no cambia). true si ya estaba o cargo.
bool W3dRootLoad(int idx);
// carga TODAS las que falten (antes de guardar / de renombrar o purgar recursos: ver arriba).
// Devuelve cuantas cargo en esta llamada; 'cargadas' (opcional) = sus filas.
int  W3dRootsLoadAll(std::vector<int>* loaded = 0);
// DESCARGA esas filas (las que el guardado cargo solo para escribirlas: ya estan en sus entradas nuevas y
// se vuelven a leer de ahi si se las abre). Se saltea la activa, la del bloque, las que no tienen entrada,
// las que no se pudieron leer y las de una cinematica en curso. Devuelve cuantas libero.
int  W3dRootsUnload(const std::vector<int>& rows);
// DESCARGA una raiz que la PARTIDA cargo (cambiarEscena) y que la partida deja: en un telefono no entran dos niveles
// a la vez. Con las mismas reglas que W3dRootsUnload (no la activa, ni la del bloque, ni una sin entrada); la
// proxima vez se vuelve a leer de su entrada. true = la libero
bool W3dRootUnloadFromGame(int idx);
// el CONTENIDO de una raiz desde su entrada: objetos (y su prefab), animaciones de escena y
// capas del Mix, sobre SceneCollection (que en ese momento ES esa raiz). Lo implementa el
// lector del proyecto (importers/import_w3d.cpp); false = no se pudo leer.
bool W3dRootReadContent(const W3dRootRow& row);

// ---- CAMBIO DE RAIZ (sin UI ni undo: eso lo agrega el editor) ------------------
// SWAP puro: guarda el contexto de la activa, pone 'idx' como SceneCollection y restaura el
// suyo. No carga (false si no esta cargada) ni toca GL. Lo usan el guardado y la carga.
bool W3dRootUse(int idx);
// cambio "de verdad": carga si hace falta, usa, apaga las luces GL de la anterior y redibuja
bool W3dRootChangeActive(int idx);

// ---- ALTA / BAJA / NOMBRE / CARPETA -------------------------------------------
// una raiz NUEVA en memoria (sin entrada hasta el guardado). Un prefab nace con su objeto
// raiz (un vacio con el nombre del prefab). No la activa. Devuelve su indice (una escena o un
// juego va antes de los prefabs: ver el orden de las filas arriba).
int  W3dNewRoot(int type, const std::string& name);   // (un JUEGO nace vacio, igual que una escena)
bool W3dRootRename(int idx, const std::string& newValue, std::string* final);
// borra la fila y libera su arbol. No se puede la ACTIVA ni la ultima escena. 'motivo' en ingles.
bool W3dRootDelete(int idx, std::string* reason);
// EL BORRADO CON UNDO (el editor: la vista Scenes/Prefabs del outliner), en dos mitades como el de los
// objetos. La fila SALE del registro con las mismas reglas que W3dRootDelete pero su arbol NO se libera:
// queda CARGADO (si no lo estaba se carga antes: el Ctrl+Z no puede depender de una entrada que el guardado
// descarta) y REGISTRADO como raiz viva (W3dRootRegister), asi los recursos que solo ella usa siguen en uso
// y un objeto que se libera en otra raiz se le desvincula, como a los objetos de un borrado con Ctrl+Z. Ya no
// es del proyecto: no se lista, no se guarda y no se puede abrir.
struct W3dRemovedRoot {
    W3dRootRow row;        // la fila tal cual (con su arbol y su contexto: ahora son de quien la saco)
    int pos;               // donde estaba en el registro
    bool initial;          // era la escena con la que arranca el juego
    W3dRemovedRoot() : pos(-1), initial(false) {}
};
bool W3dRootRemove(int idx, W3dRemovedRoot* out, std::string* reason);
// la vuelve a su lugar tal cual (la del bloque si lo era, la inicial si lo era). Queda vacia.
bool W3dRootReturn(W3dRemovedRoot& s);
// el paso de undo se descarta con la fila afuera: libera su arbol y su contexto
void W3dRemovedRootFree(W3dRemovedRoot& s);
bool W3dRootSetFolder(int idx, const std::string& folder);
// cuantos objetos tiene (recursivo; -1 si no esta cargada)
int  W3dRootObjects(int idx);
// TODAS las listas de animaciones de escena del proyecto: la de la raiz activa (SceneAnimations) y las que
// guardan los contextos de las demas raices cargadas. Para quien cambia algo que nombran las vistas de los
// clips de jerarquia de cualquier raiz (el guardado, al juntar dos bibliotecas iguales)
void W3dRootsListsAnim(std::vector<std::vector<SceneAnimation*>*>& out);

// ---- PROYECTO ----------------------------------------------------------------
// lee el registro de project.json (lo llama el lector ANTES de armar los objetos). La raiz
// actual (SceneCollection) pasa a ser la de la escena del bloque.
void W3dRootsReadJson(JVal* root);
// cierre del proyecto: libera las raices que no son la activa y vacia el registro (la activa
// sigue siendo SceneCollection: la vacia RestartScene, como siempre)
void W3dRootsCloseProject();
// el guardado salio: las filas pasan a nombrar las entradas nuevas ('entradas' paralelo a las filas)
void W3dRootsSaved(const std::vector<std::string>& entries);

// ---- EL JUEGO (runtime y Play del editor) ---------------------------------------
// carga 'idx' EN LA RAIZ ACTIVA: destruye su arbol (y las animaciones de escena que lo
// nombran) y arma el de 'idx' desde su entrada. Es el cambiarEscena() del runtime compilado.
bool W3dRootLoadInActive(int idx);
// el callback que APLICA un cambiarEscena() de escena 3D al final del frame (lo registra
// cada build: SimJuego en el editor, w3drun en el juego compilado). NULL = sin juego.
typedef void (*W3dRootChangeGameFn)(int idx);
void W3dRootsSetChangeGame(W3dRootChangeGameFn fn);
// la animacion de escena que una ESCENA reproduce cuando se la pide jugando (su activa; -1 = no tiene)
int  W3dRootCinematic();
// si la raiz activa es una ESCENA, arranca su animacion (una sola vez, sin loop) desde su inicio con
// el reproductor del juego (W3dAnimScenePlay). true = arranco. Un JUEGO no hace nada: arranca su
// simulacion. Lo llaman el Play del editor y el runtime compilado al entrar a una raiz.
bool W3dRootStartCinematic();

// ---- CINEMATICAS EN UN JUEGO: reproducirEscena() (el primero de los DOS CAMINOS) ----
// Un JUEGO muestra una raiz ESCENA como cinematica y despues sigue EXACTAMENTE donde estaba:
//   reproducirEscena("Cinematica 1" [, "alTerminar"])  (lua; se aplica al final del frame)
// La escena pasa a ser la raiz activa (sus luces, camaras, niebla, objetos) y su animacion suena de su
// Inicio a su Fin; mientras tanto la simulacion del juego queda en PAUSA (sus scripts, su fisica, sus
// hitbox y sus animaciones no corren: el arbol del juego ni se toca). Corren los scripts de la ESCENA (un
// "saltar" puede llamar pararEscena3D()). Al terminar (o con pararEscena3D) la escena vuelve a como estaba,
// el juego vuelve a ser la raiz activa con su contexto (camara, luces, la animEscena que sonaba) y se
// llama alTerminar() en los scripts del objeto que la pidio. En el juego compilado una escena que no
// estaba cargada se carga de su entrada para la cinematica y se libera al volver.
// El otro camino (varias animaciones a la vez: animObjeto/escenaCapa) vive en animation/Animation.h.
//
// lo que cada build hace en cada momento (SimJuego en el editor, w3drun en el compilado):
//   W3D_CINEMATIC_BEFORE   la escena ya es la activa y todavia no se animo: foto de su estado (el editor la repone)
//   W3D_CINEMATIC_ENTERED   su animacion arranco (cuadro de inicio posado): cargar e iniciar los scripts de la escena
//   W3D_CINEMATIC_EXITS    la escena todavia es la activa: descargar sus scripts y devolverla a como estaba
enum { W3D_CINEMATIC_BEFORE = 0, W3D_CINEMATIC_ENTERED = 1, W3D_CINEMATIC_EXITS = 2 };
typedef void (*W3dRootCinematicFn)(int moment);
void W3dRootsSetCinematic(W3dRootCinematicFn fn);   // NULL = sin juego (el pedido se ignora)
bool W3dCinematicActive();                          // se esta reproduciendo una escena (el juego en pausa)
int  W3dCinematicScene();                           // su fila (-1 = ninguna)
int  W3dCinematicGame();                            // la fila del juego que la pidio (-1 = ninguna)
// un tick de la cinematica: avanza su animacion (y la da por terminada al llegar a su Fin; la vuelta al
// juego se aplica al final del frame con W3dSceneApplyPending, como cambiarEscena)
void W3dCinematicTick(float dt);
// vuelve al juego YA, sin llamar alTerminar (el Stop del editor, un cambiarEscena en medio, el rewind)
void W3dCinematicAbort();

#endif // W3D_ROOTS_H
