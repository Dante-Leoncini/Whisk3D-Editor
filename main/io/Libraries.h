#ifndef LIBRARIES_H
#define LIBRARIES_H
// ============================================================================
//  Librerias — las LIBRERIAS EXTERNAS del proyecto: otros .w3d, de SOLO LECTURA, de
//  los que el proyecto usa PREFABS y ESCENAS (con un PROXY W3D, objects/ProxyW3d.h) o
//  recursos sueltos (una malla, un material, una textura, un animset).
//  Personajes y enemigos reutilizados en varios niveles viven UNA vez, en su libreria.
//
//  EL REGISTRO (project.json):
//      "librerias": [{"nombre": "Personajes", "archivo": "ext:../personajes.w3d"}]
//  El NOMBRE es el PREFIJO de todo lo que viene de esa libreria (es lo que guarda el
//  proyecto al nombrar algo suyo); el archivo va con el "ext:" de toda referencia
//  externa (relativo a la carpeta del .w3d, o absoluto).
//
//  MONTAJE: la libreria se MONTA (su .w3d como almacen con su nombre, io/W3dStore.h)
//  la primera vez que se la usa, AL LADO del proyecto (el proyecto sigue montado). Sus
//  entradas se nombran "lib:<nombre>/<entrada>" y ReadFileBytes las lee de ella, asi
//  que todo lo que carga tarde (la cola de texturas, los scripts al dar Play, los
//  .w3db y .w3da de los recursos) sigue andando sin saber de librerias.
//
//  RECURSOS CON PREFIJO (se registran al montarla, desde SU project.json):
//    materiales "Personajes/Piel"   (su propio espacio de nombres, solo lectura; los del
//                                    proyecto no se pisan ni se renumeran)
//    mallas     "Personajes/Cuerpo" (objects/MeshResource.h: W3dMeshResourceNewLib)
//    animsets   "Personajes/Caminar" (animation/W3dAnimSet.h: W3dAnimSetsLibAdd)
//    texturas   por su ruta "lib:Personajes/textures/piel.png" (la clave del cache)
//  Dos librerias no chocan entre si ni con el proyecto, y N proxies del mismo personaje
//  comparten memoria (una malla, un animset, un material, una textura).
//  Nada de eso se guarda en el proyecto: el que lo usa guarda solo su nombre.
//
//  EL JUEGO COMPILADO: "Compilar juego" empaqueta las librerias usadas (sus entradas)
//  en "libs/<nombre>/..." y el runtime las lee de ahi con las mismas rutas
//  "lib:<nombre>/..." (sin montar nada: ver W3dLibPathPacked). Lo que una
//  libreria deja AFUERA de su .w3d a proposito ("ext:", relativo a SU carpeta) viaja en
//  "libs/<nombre>/_ext/..." (W3dLibsExternalEntry), y lo que sus scripts nombran
//  por ruta al lado de su .w3d, en "libs/<nombre>/<ruta>".
//
//  LIBRERIA ADENTRO DE LIBRERIA: una libreria puede vincular OTRAS (su propio registro
//  "librerias", con los "ext:" relativos a SU carpeta). Lo que su contenido nombra de
//  una de ellas (un proxy, un recurso con prefijo, una ruta "lib:") se resuelve con SU
//  registro (W3dLibsNested): si el nivel ya tiene vinculado ese mismo archivo, es esa
//  (comparten memoria); si no, se monta OCULTA (no esta en el registro del proyecto ni
//  se guarda) con su nombre, o con uno libre si ese ya lo usa otra. El juego compilado
//  lleva ese mapeo en "libs/_nested.json" (lo escribe Compilar juego).
//
//  Compila en el editor y en el runtime 3D (C++03). Motor generico.
// ============================================================================
#include <string>
#include <vector>

struct JVal;
class Material;

// una libreria del registro del proyecto
struct W3dLibRow {
    std::string name;      // el prefijo ("Personajes")
    std::string jsonPath;  // como vino de project.json ("ext:../personajes.w3d")
    std::string diskPath;  // el .w3d en DISCO, resuelto al abrir o al vincular (el guardado recalcula su "ext:"
                            // contra la carpeta NUEVA: guardar el nivel en otra carpeta no rompe la referencia)
};

// ---- EL REGISTRO ----
// (la base de las rutas "ext:" relativas es la carpeta del .w3d abierto: g_w3dDirProject)
int  W3dLibsCount();
const W3dLibRow& W3dLibsRow(int i);
int  W3dLibsFind(const std::string& name);          // -1 = no esta vinculada
// la ruta del .w3d en DISCO (la resuelta al abrir o al vincular)
std::string W3dLibsDiskPath(int i);
// la ruta como va en project.json para un archivo de disco ("ext:" + relativa a la carpeta del .w3d si esta
// adentro de ella; si no, absoluta)
std::string W3dLibsPathForJson(const std::string& diskPath);
// un nombre de libreria LIBRE para 'base' (unico entre las vinculadas y las montadas en la sesion; sin '/', ':' ni
// '~'). Con 'ruta' (el .w3d que se vincula): una que se desvinculo con ESE archivo recupera su nombre
std::string W3dLibsFreeName(const std::string& base, const std::string& path = std::string());
// agrega una al registro (sin undo: el editor lo pone) con su archivo de disco. -1 = nombre vacio o repetido
int  W3dLibsAdd(const std::string& name, const std::string& diskPath);
// la saca del registro (lo que ya se cargo de ella sigue vivo hasta cerrar el proyecto, pero deja de servir para
// generar: sus proxies se regeneran vacios, ver W3dProxiesLibChange de io/PrefabsEditor.h)
bool W3dLibsRemove(int i);
// la lista entera de una vez (el undo del editor)
std::vector<W3dLibRow> W3dLibsRows();
void W3dLibsSet(const std::vector<W3dLibRow>& rows);
// project.json: el registro "librerias" (reemplaza el anterior). 'mountFromDisk' = false en el JUEGO
// COMPILADO (sus librerias vienen empaquetadas: no se abre ningun .w3d)
void W3dLibsReadJson(JVal* root, bool mountFromDisk);
// "librerias": [...] con coma final (nada si no hay)
void W3dLibsSaveJson(std::string& s);
// cierre del proyecto: desmonta todas y olvida lo registrado (los recursos se liberan con el proyecto)
void W3dLibsCloseProject();

// ---- USARLA ----
// deja la libreria LISTA: montada (su .w3d, en el editor) y sus recursos registrados con su prefijo. Idempotente.
// false + motivo (en ingles) si no esta vinculada (ni montada OCULTA por otra libreria: W3dLibsNested) o no se puede
// leer. Una que se DESVINCULO ya no sirve para generar (lo que ya se cargo de ella sigue vivo hasta cerrar el proyecto)
bool W3dLibsEnsure(const std::string& name, std::string* reason);
// la libreria de un nombre CON PREFIJO ("Personajes/Cuerpo" -> "Personajes") si es de una vinculada (y la deja
// lista). "" = no es de ninguna
std::string W3dLibsByName(const std::string& nameWithPrefix);
// el nombre con el prefijo de la libreria
inline std::string W3dLibsPrefix(const std::string& lib, const std::string& name) {
    return (lib.empty() || name.empty()) ? name : lib + "/" + name;
}
// el .w3d en DISCO de una libreria por su NOMBRE: una vinculada o una montada oculta (adentro de otra). "" = no se
// sabe (o el juego compilado: vienen empaquetadas)
std::string W3dLibsDiskPathOf(const std::string& name);
// true en el EDITOR (las librerias se montan de su .w3d); false en el JUEGO COMPILADO (vienen empaquetadas)
bool W3dLibsFromDisk();
// la ENTRADA empaquetada de un archivo que una libreria deja AFUERA de su .w3d ("ext:<rel>", relativo a su carpeta):
// "_ext/<rel>" normalizada (sin "." ni ".."; un ".." que sale de su carpeta queda como "__"). La usan Compilar juego
// (donde lo copia) y el lector del juego compilado (de donde lo lee: "lib:<libreria>/_ext/<rel>")
std::string W3dLibsExternalEntry(const std::string& rel);
// un archivo SUELTO al lado del .w3d de la libreria (lo que sus scripts nombran por ruta y no esta adentro): la ruta
// de disco si existe (solo el editor); "" = no hay
std::string W3dLibsLoosePath(const std::string& lib, const std::string& rel);

// ---- LIBRERIA ADENTRO DE LIBRERIA ----
// el registro "librerias" de la libreria 'lib' (la deja lista): sus librerias de adentro, con su archivo de disco
// resuelto contra SU carpeta (vacio en el juego compilado)
void W3dLibsSubRows(const std::string& lib, std::vector<W3dLibRow>& out);
// la libreria 'nombre' DE ADENTRO de 'de' (su registro) -> el nombre con que vive en memoria: la del nivel con ese
// mismo archivo, o una montada OCULTA (su nombre, o uno libre). Si 'de' no la vincula, 'nombre' tal cual (la del
// nivel con ese nombre, si hay)
std::string W3dLibsNested(const std::string& of, const std::string& name);
// un nombre CON PREFIJO ("Armas/Culata", o una ruta "Armas/textures/x.png") adentro del contenido de 'de': si el
// prefijo es una libreria de SU registro, el mismo con el prefijo con que vive en memoria (y la deja lista); "" = no
std::string W3dLibsNameNested(const std::string& of, const std::string& withPrefix);
// el mapeo de las de adentro (lo escribe Compilar juego en el juego: "libs/_nested.json")
extern const char* const kW3dNestedLibsEntry;

// ---- LOS MATERIALES DE LAS LIBRERIAS (su espacio de nombres) ----
// el material de libreria con ese nombre CON prefijo (NULL = no hay)
Material* W3dLibMaterial(const std::string& nameWithPrefix);
// anota uno (lo llama el lector al registrar la libreria)
void W3dLibMaterialRegister(const std::string& nameWithPrefix, Material* m);

// ---- SUS ELEMENTOS (lo que un proxy genera) ----
enum { W3D_LIB_PREFAB = 0, W3D_LIB_SCENE = 1 };
// los prefabs o escenas de la libreria (la deja lista). Vacio si no se puede leer
void W3dLibsElements(const std::string& lib, int type, std::vector<std::string>& out);
bool W3dLibsHasElement(const std::string& lib, int type, const std::string& elem);
// la CLAVE de un elemento en la cache de definiciones de los prefabs (io/Prefabs.h):
// "lib:<lib>/prefab/<elem>" o "lib:<lib>/scene/<elem>". La usan el proxy (su 'prefab') y los prefabs anidados
// de adentro de una libreria.
std::string W3dLibsKey(const std::string& lib, int type, const std::string& elem);
// LOS OBJETOS DE UNA ESCENA de la libreria (el explorador del proxy: la escena se recorre como una carpeta). Un proxy puede
// generar UN OBJETO de una escena con todo lo que cuelga de el: su elemento es "Escena#Padre/Hijo" (tipo escena)
struct W3dLibObject { std::string name, type; bool withChildren; W3dLibObject() : withChildren(false) {} };
bool W3dLibsSceneObjects(const std::string& lib, const std::string& scene, const std::string& path, std::vector<W3dLibObject>& out);
bool W3dLibsByKey(const std::string& key, std::string* lib, int* type, std::string* elem);
// lee la DEFINICION de un elemento: devuelve el documento (el que llama es su dueno) y en *rootObj el objeto
// que se genera (un prefab: su "raiz"; una escena: un vacio con el nombre de la escena que tiene como hijos sus
// objetos de primer nivel). NULL = no se pudo (motivo en el log)
JVal* W3dLibsReadElement(const std::string& lib, int type, const std::string& elem, JVal** rootObj);

// ---- EL CONTEXTO DE CARGA (lo mira el lector, importers/import_w3d.cpp) ----
// Mientras se arma contenido de una libreria (lo que genera un proxy), las rutas del JSON son entradas de ESA
// libreria ("lib:<lib>/...") y los nombres de sus recursos llevan su prefijo. Es una pila (un prefab anidado).
void W3dLibContextEnter(const std::string& lib);
void W3dLibContextExit();
const std::string& W3dLibContext();   // "" = el proyecto
// guarda de alcance (C++03)
struct W3dLibContextGuard {
    bool active;
    explicit W3dLibContextGuard(const std::string& lib) : active(!lib.empty()) { if (active) W3dLibContextEnter(lib); }
    ~W3dLibContextGuard() { if (active) W3dLibContextExit(); }
private:
    W3dLibContextGuard(const W3dLibContextGuard&);
    W3dLibContextGuard& operator=(const W3dLibContextGuard&);
};

// ---- REFERENCIAS DEL PROYECTO A UNA LIBRERIA QUE NO ESTA ----
// Lo que el PROYECTO nombra de una libreria que hoy no se puede usar (se desvinculo, o su .w3d falta en disco): la malla
// de un objeto o el animset de un esqueleto, con su prefijo ("Personajes/Tronco"). El objeto queda VACIO (sin geometria /
// sin clips) pero la REFERENCIA no se pierde ni frena el guardado: se escribe tal cual, y se vuelve a resolver al
// vincular otra vez la libreria (en la sesion: W3dLibRefsResolve de io/PrefabsEditor.h; o al reabrir el nivel).
// Por SERIAL del objeto (nunca un puntero colgado); se olvidan al cerrar el proyecto.
enum { W3D_LIBREF_MESH = 0, W3D_LIBREF_ANIMSET = 1 };
struct W3dLibRef {
    std::string name;              // con el prefijo de la libreria
    std::vector<int> clips;        // (animset) los clips que usa el esqueleto, como venian en el archivo
    bool withClips;                // (animset) el archivo traia la lista de clips
    W3dLibRef() : withClips(false) {}
};
class Object;
void W3dLibRefAnnotate(const Object* o, int type, const W3dLibRef& ref);
const W3dLibRef* W3dLibRefOf(const Object* o, int type);   // NULL = no tiene
void W3dLibRefForget(const Object* o, int type);
// los seriales de los objetos con una referencia pendiente de ese tipo a la libreria 'lib' ("" = a cualquiera)
void W3dLibRefsSerials(const std::string& lib, int type, std::vector<unsigned>& out);
// la libreria que nombra un nombre de recurso con prefijo ("Personajes/Tronco" -> "Personajes"); "" = no tiene prefijo
std::string W3dLibPrefixOf(const std::string& name);

// ---- implementado por el lector (importers/import_w3d.cpp) ----
// registra los recursos de la libreria desde su project.json: materiales, mallas y animsets (con su prefijo)
bool W3dLibRegisterResources(const std::string& lib, JVal* root);

#endif
