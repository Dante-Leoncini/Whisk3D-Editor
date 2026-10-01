#ifndef W3D_LIBRERIAS_H
#define W3D_LIBRERIAS_H
// ============================================================================
//  Librerias — las LIBRERIAS EXTERNAS del proyecto: otros .w3d, de SOLO LECTURA, de
//  los que el proyecto usa PREFABS y ESCENAS (con un PROXY W3D, objects/ProxyW3d.h) o
//  recursos sueltos (una malla, un material, una textura, un animset).
//  Personajes y enemigos reutilizados en varios niveles viven UNA vez, en su libreria.
//
//  EL REGISTRO (proyecto.json):
//      "librerias": [{"nombre": "Personajes", "archivo": "ext:../personajes.w3d"}]
//  El NOMBRE es el PREFIJO de todo lo que viene de esa libreria (es lo que guarda el
//  proyecto al nombrar algo suyo); el archivo va con el "ext:" de toda referencia
//  externa (relativo a la carpeta del .w3d, o absoluto).
//
//  MONTAJE: la libreria se MONTA (su .w3d como almacen con su nombre, io/W3dAlmacen.h)
//  la primera vez que se la usa, AL LADO del proyecto (el proyecto sigue montado). Sus
//  entradas se nombran "lib:<nombre>/<entrada>" y ReadFileBytes las lee de ella, asi
//  que todo lo que carga tarde (la cola de texturas, los scripts al dar Play, los
//  .w3db y .w3da de los recursos) sigue andando sin saber de librerias.
//
//  RECURSOS CON PREFIJO (se registran al montarla, desde SU proyecto.json):
//    materiales "Personajes/Piel"   (su propio espacio de nombres, solo lectura; los del
//                                    proyecto no se pisan ni se renumeran)
//    mallas     "Personajes/Cuerpo" (objects/MallaRecurso.h: W3dMallaRecursoNuevoLib)
//    animsets   "Personajes/Caminar" (animation/W3dAnimSet.h: W3dAnimSetsLibAgregar)
//    texturas   por su ruta "lib:Personajes/texturas/piel.png" (la clave del cache)
//  Dos librerias no chocan entre si ni con el proyecto, y N proxies del mismo personaje
//  comparten memoria (una malla, un animset, un material, una textura).
//  Nada de eso se guarda en el proyecto: el que lo usa guarda solo su nombre.
//
//  EL JUEGO COMPILADO: "Compilar juego" empaqueta las librerias usadas (sus entradas)
//  en "librerias/<nombre>/..." y el runtime las lee de ahi con las mismas rutas
//  "lib:<nombre>/..." (sin montar nada: ver W3dRutaLibreriaEmpaquetada). Lo que una
//  libreria deja AFUERA de su .w3d a proposito ("ext:", relativo a SU carpeta) viaja en
//  "librerias/<nombre>/_ext/..." (W3dLibsEntradaExterna), y lo que sus scripts nombran
//  por ruta al lado de su .w3d, en "librerias/<nombre>/<ruta>".
//
//  LIBRERIA ADENTRO DE LIBRERIA: una libreria puede vincular OTRAS (su propio registro
//  "librerias", con los "ext:" relativos a SU carpeta). Lo que su contenido nombra de
//  una de ellas (un proxy, un recurso con prefijo, una ruta "lib:") se resuelve con SU
//  registro (W3dLibsAnidada): si el nivel ya tiene vinculado ese mismo archivo, es esa
//  (comparten memoria); si no, se monta OCULTA (no esta en el registro del proyecto ni
//  se guarda) con su nombre, o con uno libre si ese ya lo usa otra. El juego compilado
//  lleva ese mapeo en "librerias/_anidadas.json" (lo escribe Compilar juego).
//
//  Compila en el editor y en el runtime 3D (C++03). Motor generico.
// ============================================================================
#include <string>
#include <vector>

struct JVal;
class Material;

// una libreria del registro del proyecto
struct W3dLibFila {
    std::string nombre;     // el prefijo ("Personajes")
    std::string rutaJson;   // como vino de proyecto.json ("ext:../personajes.w3d")
    std::string rutaDisco;  // el .w3d en DISCO, resuelto al abrir o al vincular (el guardado recalcula su "ext:"
                            // contra la carpeta NUEVA: guardar el nivel en otra carpeta no rompe la referencia)
};

// ---- EL REGISTRO ----
// (la base de las rutas "ext:" relativas es la carpeta del .w3d abierto: g_w3dDirProyecto)
int  W3dLibsCantidad();
const W3dLibFila& W3dLibsFila(int i);
int  W3dLibsBuscar(const std::string& nombre);          // -1 = no esta vinculada
// la ruta del .w3d en DISCO (la resuelta al abrir o al vincular)
std::string W3dLibsRutaDisco(int i);
// la ruta como va en proyecto.json para un archivo de disco ("ext:" + relativa a la carpeta del .w3d si esta
// adentro de ella; si no, absoluta)
std::string W3dLibsRutaParaJson(const std::string& rutaDisco);
// un nombre de libreria LIBRE para 'base' (unico entre las vinculadas y las montadas en la sesion; sin '/', ':' ni
// '~'). Con 'ruta' (el .w3d que se vincula): una que se desvinculo con ESE archivo recupera su nombre
std::string W3dLibsNombreLibre(const std::string& base, const std::string& ruta = std::string());
// agrega una al registro (sin undo: el editor lo pone) con su archivo de disco. -1 = nombre vacio o repetido
int  W3dLibsAgregar(const std::string& nombre, const std::string& rutaDisco);
// la saca del registro (lo que ya se cargo de ella sigue vivo hasta cerrar el proyecto, pero deja de servir para
// generar: sus proxies se regeneran vacios, ver W3dProxiesLibreriaCambio de io/PrefabsEditor.h)
bool W3dLibsQuitar(int i);
// la lista entera de una vez (el undo del editor)
std::vector<W3dLibFila> W3dLibsFilas();
void W3dLibsFijar(const std::vector<W3dLibFila>& filas);
// proyecto.json: el registro "librerias" (reemplaza el anterior). 'montarDeDisco' = false en el JUEGO
// COMPILADO (sus librerias vienen empaquetadas: no se abre ningun .w3d)
void W3dLibsLeerJson(JVal* raiz, bool montarDeDisco);
// "librerias": [...] con coma final (nada si no hay)
void W3dLibsGuardarJson(std::string& s);
// cierre del proyecto: desmonta todas y olvida lo registrado (los recursos se liberan con el proyecto)
void W3dLibsCerrarProyecto();

// ---- USARLA ----
// deja la libreria LISTA: montada (su .w3d, en el editor) y sus recursos registrados con su prefijo. Idempotente.
// false + motivo (en ingles) si no esta vinculada (ni montada OCULTA por otra libreria: W3dLibsAnidada) o no se puede
// leer. Una que se DESVINCULO ya no sirve para generar (lo que ya se cargo de ella sigue vivo hasta cerrar el proyecto)
bool W3dLibsAsegurar(const std::string& nombre, std::string* motivo);
// la libreria de un nombre CON PREFIJO ("Personajes/Cuerpo" -> "Personajes") si es de una vinculada (y la deja
// lista). "" = no es de ninguna
std::string W3dLibsDeNombre(const std::string& nombreConPrefijo);
// el nombre con el prefijo de la libreria
inline std::string W3dLibsPrefijar(const std::string& lib, const std::string& nombre) {
    return (lib.empty() || nombre.empty()) ? nombre : lib + "/" + nombre;
}
// el .w3d en DISCO de una libreria por su NOMBRE: una vinculada o una montada oculta (adentro de otra). "" = no se
// sabe (o el juego compilado: vienen empaquetadas)
std::string W3dLibsRutaDiscoDe(const std::string& nombre);
// true en el EDITOR (las librerias se montan de su .w3d); false en el JUEGO COMPILADO (vienen empaquetadas)
bool W3dLibsDesdeDisco();
// la ENTRADA empaquetada de un archivo que una libreria deja AFUERA de su .w3d ("ext:<rel>", relativo a su carpeta):
// "_ext/<rel>" normalizada (sin "." ni ".."; un ".." que sale de su carpeta queda como "__"). La usan Compilar juego
// (donde lo copia) y el lector del juego compilado (de donde lo lee: "lib:<libreria>/_ext/<rel>")
std::string W3dLibsEntradaExterna(const std::string& rel);
// un archivo SUELTO al lado del .w3d de la libreria (lo que sus scripts nombran por ruta y no esta adentro): la ruta
// de disco si existe (solo el editor); "" = no hay
std::string W3dLibsRutaSuelta(const std::string& lib, const std::string& rel);

// ---- LIBRERIA ADENTRO DE LIBRERIA ----
// el registro "librerias" de la libreria 'lib' (la deja lista): sus librerias de adentro, con su archivo de disco
// resuelto contra SU carpeta (vacio en el juego compilado)
void W3dLibsSubFilas(const std::string& lib, std::vector<W3dLibFila>& out);
// la libreria 'nombre' DE ADENTRO de 'de' (su registro) -> el nombre con que vive en memoria: la del nivel con ese
// mismo archivo, o una montada OCULTA (su nombre, o uno libre). Si 'de' no la vincula, 'nombre' tal cual (la del
// nivel con ese nombre, si hay)
std::string W3dLibsAnidada(const std::string& de, const std::string& nombre);
// un nombre CON PREFIJO ("Armas/Culata", o una ruta "Armas/texturas/x.png") adentro del contenido de 'de': si el
// prefijo es una libreria de SU registro, el mismo con el prefijo con que vive en memoria (y la deja lista); "" = no
std::string W3dLibsNombreAnidado(const std::string& de, const std::string& conPrefijo);
// el mapeo de las de adentro (lo escribe Compilar juego en el juego: "librerias/_anidadas.json")
extern const char* const kW3dLibsAnidadasEntrada;

// ---- LOS MATERIALES DE LAS LIBRERIAS (su espacio de nombres) ----
// el material de libreria con ese nombre CON prefijo (NULL = no hay)
Material* W3dLibMaterial(const std::string& nombreConPrefijo);
// anota uno (lo llama el lector al registrar la libreria)
void W3dLibMaterialRegistrar(const std::string& nombreConPrefijo, Material* m);

// ---- SUS ELEMENTOS (lo que un proxy genera) ----
enum { W3D_LIB_PREFAB = 0, W3D_LIB_ESCENA = 1 };
// los prefabs o escenas de la libreria (la deja lista). Vacio si no se puede leer
void W3dLibsElementos(const std::string& lib, int tipo, std::vector<std::string>& out);
bool W3dLibsTieneElemento(const std::string& lib, int tipo, const std::string& elem);
// la CLAVE de un elemento en la cache de definiciones de los prefabs (io/Prefabs.h):
// "lib:<lib>/prefab/<elem>" o "lib:<lib>/escena/<elem>". La usan el proxy (su 'prefab') y los prefabs anidados
// de adentro de una libreria.
std::string W3dLibsClave(const std::string& lib, int tipo, const std::string& elem);
bool W3dLibsDeClave(const std::string& clave, std::string* lib, int* tipo, std::string* elem);
// lee la DEFINICION de un elemento: devuelve el documento (el que llama es su dueno) y en *raizObj el objeto
// que se genera (un prefab: su "raiz"; una escena: un vacio con el nombre de la escena que tiene como hijos sus
// objetos de primer nivel). NULL = no se pudo (motivo en el log)
JVal* W3dLibsLeerElemento(const std::string& lib, int tipo, const std::string& elem, JVal** raizObj);

// ---- EL CONTEXTO DE CARGA (lo mira el lector, importers/import_w3d.cpp) ----
// Mientras se arma contenido de una libreria (lo que genera un proxy), las rutas del JSON son entradas de ESA
// libreria ("lib:<lib>/...") y los nombres de sus recursos llevan su prefijo. Es una pila (un prefab anidado).
void W3dLibContextoEntrar(const std::string& lib);
void W3dLibContextoSalir();
const std::string& W3dLibContexto();   // "" = el proyecto
// guarda de alcance (C++03)
struct W3dLibContextoGuarda {
    bool activo;
    explicit W3dLibContextoGuarda(const std::string& lib) : activo(!lib.empty()) { if (activo) W3dLibContextoEntrar(lib); }
    ~W3dLibContextoGuarda() { if (activo) W3dLibContextoSalir(); }
private:
    W3dLibContextoGuarda(const W3dLibContextoGuarda&);
    W3dLibContextoGuarda& operator=(const W3dLibContextoGuarda&);
};

// ---- REFERENCIAS DEL PROYECTO A UNA LIBRERIA QUE NO ESTA ----
// Lo que el PROYECTO nombra de una libreria que hoy no se puede usar (se desvinculo, o su .w3d falta en disco): la malla
// de un objeto o el animset de un esqueleto, con su prefijo ("Personajes/Tronco"). El objeto queda VACIO (sin geometria /
// sin clips) pero la REFERENCIA no se pierde ni frena el guardado: se escribe tal cual, y se vuelve a resolver al
// vincular otra vez la libreria (en la sesion: W3dLibRefsResolver de io/PrefabsEditor.h; o al reabrir el nivel).
// Por SERIAL del objeto (nunca un puntero colgado); se olvidan al cerrar el proyecto.
enum { W3D_LIBREF_MALLA = 0, W3D_LIBREF_ANIMSET = 1 };
struct W3dLibRef {
    std::string nombre;            // con el prefijo de la libreria
    std::vector<int> clips;        // (animset) los clips que usa el esqueleto, como venian en el archivo
    bool conClips;                 // (animset) el archivo traia la lista de clips
    W3dLibRef() : conClips(false) {}
};
class Object;
void W3dLibRefAnotar(const Object* o, int tipo, const W3dLibRef& ref);
const W3dLibRef* W3dLibRefDe(const Object* o, int tipo);   // NULL = no tiene
void W3dLibRefOlvidar(const Object* o, int tipo);
// los seriales de los objetos con una referencia pendiente de ese tipo a la libreria 'lib' ("" = a cualquiera)
void W3dLibRefsSeriales(const std::string& lib, int tipo, std::vector<unsigned>& out);
// la libreria que nombra un nombre de recurso con prefijo ("Personajes/Tronco" -> "Personajes"); "" = no tiene prefijo
std::string W3dLibPrefijoDe(const std::string& nombre);

// ---- implementado por el lector (importers/import_w3d.cpp) ----
// registra los recursos de la libreria desde su proyecto.json: materiales, mallas y animsets (con su prefijo)
bool W3dLibRegistrarRecursos(const std::string& lib, JVal* raiz);

#endif
