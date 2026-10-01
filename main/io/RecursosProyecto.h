#ifndef W3D_RECURSOS_PROYECTO_H
#define W3D_RECURSOS_PROYECTO_H
// ============================================================================
//  RecursosProyecto — los RECURSOS del proyecto vistos como la BIBLIOTECA: el
//  explorador del contenido del .w3d que muestra el OUTLINER (vista "Biblioteca")
//  y lo que Properties muestra cuando se elige uno ("recurso activo").
//
//  LOS TIPOS: mallas 3D, materiales, texturas, animaciones (animsets y sus clips de
//  jerarquia), prefabs, escenas/juegos, sonidos, scripts, fuentes y videos. Cada tipo
//  tiene UN proveedor (W3dProveedorRecursos) que sabe listar sus items (nombre, icono,
//  carpeta, cuantos lo usan) y hacer las acciones de la biblioteca (renombrar, mover a
//  una carpeta, borrar, los objetos que lo usan, dentro/fuera del .w3d). Mallas,
//  materiales, texturas, animaciones y los ARCHIVOS (sonidos, scripts, fuentes, videos:
//  las entradas de su carpeta del contenedor y lo que los objetos nombran) los trae este
//  modulo; ESCENAS y PREFABS los registra io/RaicesEditor (W3dRaices.h). Los usuarios se
//  cuentan en TODAS las raices cargadas (una malla que usa solo otra escena no es huerfana).
//  Las constantes de tipo se llaman W3D_VISTA_* por historia (la fase anterior tenia una
//  vista del outliner por tipo): hoy son el TIPO de un recurso y el FILTRO de la biblioteca.
//
//  LAS CARPETAS son COSMETICAS y son UN SOLO ARBOL para TODOS los tipos: una carpeta
//  "Personaje" puede tener su malla, su textura, su animset y su prefab. No cambian nada
//  del codigo ni del runtime. Una carpeta es una RUTA con '/' ("A/B" = B adentro de A),
//  normalizada como la de las mallas. Se guardan en proyecto.json:
//      "carpetas": ["Personaje", "Vegetacion", "Vegetacion/Arboles"]
//  (las que el usuario creo, aunque esten vacias) y cada item dice la suya en su propio
//  registro ("carpeta" de "mallas" / "animsets" / "escenas3d" / "prefabs" / cada material);
//  las texturas, que no tienen registro, en "texturas": [{"entrada", "carpeta"}] y los demas
//  archivos (sonidos, scripts, fuentes, videos) en "archivos": [{"entrada", "carpeta"}].
//  MIGRACION: el formato de la fase anterior ("carpetas": {"mallas": [...], "materiales":
//  [...]}, un arbol por tipo) se lee juntando todo en el arbol unico; se guarda el nuevo.
//
//  EN USO / HUERFANO: un recurso sin usuarios es HUERFANO (se conserva igual; "Purge
//  orphans" los borra). Usuarios: mallas = objetos malla de la escena; materiales =
//  mallas con alguna parte que lo usa (y las mallas huerfanas del registro, que se
//  guardan con sus materiales); texturas = materiales, particulas, elementos 2D, la fuente
//  bitmap de un texto 2D, el atlas de los flipbooks y los scripts .lua que nombran el archivo
//  (entero); animsets = armatures y los objetos raiz que la usan como biblioteca de clips de
//  jerarquia; scripts = los objetos que lo corren; sonidos = los scripts que nombran el
//  archivo; fuentes = los textos 2D; videos = los videos 2D. Debajo de cada animset CARGADO
//  van sus CLIPS DE JERARQUIA (items con 'padre', id "Biblioteca/clip", el mismo nombre
//  calificado que animObjeto de lua): se renombran y se borran de a uno, con undo.
//
//  TEXTURAS Y ARCHIVOS: su nombre ES su entrada del contenedor, asi que renombrarlos muda la
//  entrada (y a todos los que la nombran); una textura CARGADA que nadie usa se purga en
//  caliente (sale de la GPU; el objeto queda en el cementerio del cache hasta cerrar, ver
//  TexturaPurgar). DENTRO / FUERA: un archivo puede vivir adentro del .w3d (una entrada) o
//  AFUERA, al lado (una referencia "ext:" relativa al .w3d, ver W3dContenedor.h): pasarlo
//  afuera escribe el archivo en el disco y lo referencia por su ruta; pasarlo adentro lo
//  ingiere al contenedor. Con undo.
//
//  Solo editor (main/). Motor generico: aca no hay nombres de ningun juego.
// ============================================================================
#include <string>
#include <vector>

class Object;
class Material;
class Texture;
class W3dContenedorEscritor;
struct MallaRecurso;
struct JVal;

// los TIPOS de recurso (y los filtros de la biblioteca). En "carpetas" y en el harness van por
// su CLAVE (W3dVistaClave), nunca por el numero. Los nuevos, siempre al FINAL.
enum W3dVistaRec {
    W3D_VISTA_ESCENA = 0,     // (no es un tipo: el arbol de la escena)
    W3D_VISTA_MALLAS,
    W3D_VISTA_MATERIALES,
    W3D_VISTA_TEXTURAS,
    W3D_VISTA_ANIMACIONES,
    W3D_VISTA_PREFABS,        // io/RaicesEditor
    W3D_VISTA_ESCENAS,        // escenas y juegos (io/RaicesEditor)
    W3D_VISTA_LIBRERIAS,      // (sin proveedor: las librerias externas son VISTAS del outliner)
    W3D_VISTA_SONIDOS,        // archivos de sonidos/
    W3D_VISTA_SCRIPTS,        // archivos .lua de scripts/
    W3D_VISTA_FUENTES,        // archivos de fuentes/ (.ttf, .otf, .w3dfnt)
    W3D_VISTA_VIDEOS,         // archivos de videos/
    W3D_VISTAS                // tope
};
const char* W3dVistaClave(int vista);                 // "escena", "mallas", "materiales"...
int         W3dVistaDeClave(const std::string& clave); // -1 = no es una vista
const char* W3dVistaTitulo(int vista);                // en INGLES (la UI lo pasa por T())
const char* W3dVistaSingular(int vista);              // "3D Mesh", "Material"... en INGLES
int         W3dVistaIcono(int vista);                 // IconType del tipo

// un recurso tal como lo lista su tipo
struct W3dRecursoItem {
    int  tipo;               // W3dVistaRec (lo pone el listado)
    std::string id;          // identidad ESTABLE en su tipo (el nombre del recurso, la entrada de la textura)
    std::string nombre;      // lo que se muestra
    std::string carpeta;     // carpeta cosmetica NORMALIZADA ("" = la raiz)
    std::string entrada;     // su entrada del contenedor / su ruta ("" = todavia no se guardo)
    std::string info;        // datos del tipo para Properties, YA en el idioma de la UI ("24 vertices, 2 parts", "256x256"...)
    int  icono;              // IconType
    int  usuarios;           // cuantos lo usan (0 = HUERFANO)
    bool soloLectura;        // no se renombra, no se mueve, no se borra (una libreria externa)
    bool renombrable;        // el tipo sabe renombrarlo (una textura: si ya es una entrada del proyecto)
    // un item que es PARTE de otro (un clip de jerarquia de una biblioteca): 'padre' = el id del item que lo
    // contiene. La vista lo muestra debajo de su padre (una sangria mas, sin carpeta propia: va donde va su
    // padre), no se mueve a otra carpeta, "Purge orphans" no lo toca y se BORRA aunque su padre este en uso
    // (con su paso de undo: es editar el contenido del padre, no purgar un recurso).
    std::string padre;
    W3dRecursoItem() : tipo(0), icono(-1), usuarios(0), soloLectura(false), renombrable(true) {}
};

// un DATO de un recurso para Properties: etiqueta y valor, ya en el idioma de la UI
struct W3dRecursoDato {
    std::string etiqueta, valor;
    W3dRecursoDato() {}
    W3dRecursoDato(const std::string& e, const std::string& v) : etiqueta(e), valor(v) {}
};

// EL PROVEEDOR de un tipo de recurso. Vive toda la sesion (un static del modulo que lo
// registra); el registro no es dueno. Todo lo que modifica lo llama el editor SIN undo: el
// paso de undo lo arma este modulo (W3dVistaRecRenombrar / Mover / carpetas) o el proveedor.
class W3dProveedorRecursos {
public:
    virtual ~W3dProveedorRecursos() {}
    // TODOS los items del tipo (el orden no importa: la vista ordena por carpeta y nombre)
    virtual void Listar(std::vector<W3dRecursoItem>& out) = 0;
    // uno solo (por id). Por defecto busca en Listar; un proveedor con muchos items lo pisa.
    virtual bool Info(const std::string& id, W3dRecursoItem& out);
    // la CARPETA cosmetica del item (ya normalizada). false = no existe o no se puede.
    virtual bool FijarCarpeta(const std::string& id, const std::string& carpeta) = 0;
    // renombrar CON SU PASO DE UNDO (el proveedor sabe cual es el suyo: el de las mallas y los
    // materiales ya existia). El nombre se hace unico. 'final' = el que quedo. false = no se pudo.
    virtual bool Renombrar(const std::string& id, const std::string& nuevo, std::string* final)
        { (void)id; (void)nuevo; (void)final; return false; }
    // el id que pediria 'nuevo' ANTES de hacerlo unico (para avisar "ese nombre ya estaba").
    // Por defecto el nombre normalizado (id == nombre); las texturas lo arman como su entrada.
    virtual std::string IdPedido(const std::string& id, const std::string& nuevo);
    // borrar un HUERFANO. false + motivo (en ingles, la UI lo pasa por T()) = no se pudo.
    virtual bool Borrar(const std::string& id, std::string* motivo)
        { (void)id; if (motivo) *motivo = "This resource type can't be deleted"; return false; }
    // borrar un recurso EN USO, dejando a sus usuarios SIN EL (con un paso de undo). false +
    // motivo = no se pudo. SabeBorrarEnUso = el tipo lo sabe hacer (si no, la UI no lo ofrece).
    virtual bool BorrarEnUso(const std::string& id, std::string* motivo)
        { (void)id; if (motivo) *motivo = "It is in use"; return false; }
    virtual bool SabeBorrarEnUso() const { return false; }
    // los OBJETOS de la escena que lo usan ("Select Users")
    virtual void Usuarios(const std::string& id, std::vector<Object*>& out) { (void)id; out.clear(); }
    // "Purge orphans" borra los huerfanos de este tipo? (las escenas y los prefabs no: un prefab sin
    // instancias o una escena que nadie nombra se borran de a uno, a pedido, nunca en montonera)
    virtual bool Purgable() const { return true; }
    // "Duplicate": una copia sin usuarios con nombre libre. false + motivo = el tipo no sabe.
    virtual bool Duplicar(const std::string& id, std::string* idNuevo, std::string* motivo)
        { (void)id; (void)idNuevo; if (motivo) *motivo = "This resource type can't be duplicated"; return false; }
    // los DATOS del tipo para Properties (resolucion y alfa de una textura, frecuencia y duracion de
    // un sonido, vertices de una malla...). Por defecto la linea 'info' del item.
    virtual void Datos(const std::string& id, std::vector<W3dRecursoDato>& out);
    // DENTRO / FUERA del .w3d: 0 = adentro (una entrada), 1 = afuera ("ext:"), -1 = el tipo no
    // tiene archivo propio (materiales, escenas...: viven en proyecto.json o en su registro)
    virtual int  Ubicacion(const std::string& id) { (void)id; return -1; }
    // la ruta que tendria afuera (de disco, al lado del .w3d) y el cambio (con su paso de undo).
    // 'idNuevo' = el id con el que quedo (la ruta nueva: el id de un archivo ES su ruta)
    virtual std::string RutaExternaSugerida(const std::string& id) { (void)id; return std::string(); }
    virtual bool FijarUbicacion(const std::string& id, int ubicacion, const std::string& rutaExterna,
                                std::string* idNuevo, std::string* motivo)
        { (void)id; (void)ubicacion; (void)rutaExterna; (void)idNuevo; if (motivo) *motivo = "It has no file of its own"; return false; }
};

// registra el proveedor de un tipo (prefabs, escenas). Pisa al anterior. NULL = el tipo vuelve
// a no tener datos.
void W3dRecursosVistaRegistrar(int vista, W3dProveedorRecursos* p);
W3dProveedorRecursos* W3dRecursosVistaProveedor(int vista);   // NULL = sin datos todavia
// los items del tipo (vacio si no tiene proveedor)
void W3dVistaRecListar(int vista, std::vector<W3dRecursoItem>& out);
bool W3dVistaRecInfo(int vista, const std::string& id, W3dRecursoItem* out);

// ---------------------------------------------------------------------------
//  LA BIBLIOTECA: todos los items de todos los tipos (o los de un FILTRO)
// ---------------------------------------------------------------------------
// filtro = un tipo, o -1 = todos. Cada item trae su 'tipo'.
void W3dBibliotecaListar(int filtro, std::vector<W3dRecursoItem>& out);
// los tipos que la biblioteca ofrece en su filtro (los que tienen proveedor), en el orden del menu
void W3dBibliotecaTipos(std::vector<int>& out);
// la CLAVE unica de un item en la biblioteca: "<clave del tipo>:<id>" ("mallas:Arbol")
std::string W3dBibClave(int tipo, const std::string& id);
// al reves; false = no es una clave de item
bool        W3dBibDeClave(const std::string& clave, int* tipo, std::string* id);

// ---------------------------------------------------------------------------
//  CARPETAS (cosmeticas, UN arbol para todos los tipos)
// ---------------------------------------------------------------------------
std::string W3dCarpetaNormalizar(const std::string& ruta);   // "a / b//" -> "a/b"
std::string W3dCarpetaPadre(const std::string& ruta);        // "a/b" -> "a"; "a" -> ""
std::string W3dCarpetaHoja(const std::string& ruta);         // "a/b" -> "b"
bool        W3dCarpetaAdentro(const std::string& ruta, const std::string& carpeta); // ruta == carpeta o cuelga de ella
// TODAS las carpetas: las creadas (aunque esten vacias) + las de los items (con sus
// ancestros), ordenadas y sin repetir. 'items' = los de TODOS los tipos (W3dBibliotecaListar(-1)).
void W3dCarpetasBiblioteca(const std::vector<W3dRecursoItem>& items, std::vector<std::string>& out);
void W3dCarpetasTodas(std::vector<std::string>& out);        // idem, listando todo
const std::vector<std::string>& W3dCarpetasCreadas();
// NUEVA carpeta adentro de 'padre' ("" = la raiz), con 'nombre' hecho unico entre sus hermanas
// ("" = "New Folder"). Con undo. 'ruta' = la que quedo.
bool W3dCarpetaNueva(const std::string& padre, const std::string& nombre, std::string* ruta);
// RENOMBRAR la carpeta (su ultimo tramo): arrastra sus subcarpetas y sus items. Con undo.
bool W3dCarpetaRenombrar(const std::string& ruta, const std::string& nombre, std::string* rutaNueva);
// true si ningun item (de ningun tipo) cuelga de la carpeta (ni de sus subcarpetas)
bool W3dCarpetaVacia(const std::string& ruta);
// BORRAR una carpeta VACIA (y sus subcarpetas vacias). Con undo. false + motivo si tiene items.
bool W3dCarpetaBorrar(const std::string& ruta, std::string* motivo);
// MUDAR una carpeta (con sus subcarpetas y sus recursos) ADENTRO de 'destino' ("" = la raiz), como
// en un explorador de archivos: conserva su nombre (hecho unico entre sus nuevas hermanas) y la
// carpeta de la que sale sigue existiendo. Con undo. false = no existe, o 'destino' es ella misma o
// cuelga de ella. 'rutaNueva' = donde quedo.
bool W3dCarpetaMover(const std::string& ruta, const std::string& destino, std::string* rutaNueva);
// GRUPO de operaciones de carpetas (la SELECCION MULTIPLE del outliner, el modo MOVER con la G):
// todo lo que pase entre Iniciar y Fin es UN SOLO paso de undo. Anida. Cancelar (con el grupo de
// afuera abierto) DESHACE lo que se hizo en el grupo y lo cierra sin dejar paso (el Esc del modo mover).
void W3dVistaRecGrupoIniciar();
void W3dVistaRecGrupoFin();
void W3dVistaRecGrupoCancelar();
// Ctrl+Z / Ctrl+Y con un grupo ABIERTO (el modo mover de la biblioteca a medias): el paso que el grupo
// confirmaria es la diferencia contra su foto y se comia lo deshecho. Se CANCELA el modo (lo elegido vuelve
// a donde estaba, como con Esc) y ese Ctrl+Z no hace nada mas. true = habia un grupo (el undo no sigue). Lo
// llaman UndoDeshacer / UndoRehacer. El gancho lo pone el outliner (sale de su modo mover).
bool W3dVistaRecAntesDeUndo();
extern void (*W3dVistaRecGrupoCancelarHook)();
// la firma de las carpetas (las creadas + la de cada item): cambia con cualquier cambio de carpeta
// (el seguimiento de lo no guardado, CambiosProyecto). 'items' = la biblioteca ya listada (sin eso la lista)
std::string W3dCarpetasFirma();
std::string W3dCarpetasFirma(const std::vector<W3dRecursoItem>& items);

// ---------------------------------------------------------------------------
//  ACCIONES SOBRE UN ITEM (con su paso de undo cuando corresponde)
// ---------------------------------------------------------------------------
bool W3dVistaRecMover(int vista, const std::string& id, const std::string& carpeta);
bool W3dVistaRecRenombrar(int vista, const std::string& id, const std::string& nuevo, std::string* final);
// borra UN huerfano (sin undo: el "Purge" de Blender). false + motivo (ingles).
bool W3dVistaRecBorrar(int vista, const std::string& id, std::string* motivo);
// borra un recurso EN USO dejando a sus usuarios sin el (con undo si el tipo lo tiene). Para un
// huerfano es lo mismo que W3dVistaRecBorrar.
bool W3dVistaRecBorrarForzado(int vista, const std::string& id, std::string* motivo);
bool W3dVistaRecSabeBorrarEnUso(int vista);
// "Purge orphans": borra TODOS los huerfanos que se puedan de ese tipo (-1 = de todos los tipos).
// Devuelve cuantos; 'nombres' = cuales.
int  W3dVistaRecPurgar(int vista, std::vector<std::string>* nombres);
// "Select Users": selecciona en la escena los objetos que lo usan (con undo de seleccion) y
// despliega sus padres para que se vean en el arbol. Devuelve cuantos.
int  W3dVistaRecSeleccionarUsuarios(int vista, const std::string& id);
// idem con VARIOS recursos (claves de biblioteca, W3dBibClave): la union de sus usuarios, un solo
// paso de undo. Sin objetos que seleccionar devuelve 0 y la seleccion queda como estaba.
int  W3dBibSeleccionarUsuarios(const std::vector<std::string>& claves);
int  W3dVistaRecSeleccionarUsuarios(int vista, const std::vector<std::string>& ids);
// cuantos OBJETOS de la escena usan esos recursos (la union). Puede ser 0 aunque tengan usuarios: una
// malla huerfana del registro, un script o un flipbook cuentan como usuarios pero no se seleccionan.
// "Select Users" solo se ofrece si hay alguno.
int  W3dVistaRecObjetosUsuarios(int vista, const std::vector<std::string>& ids);
int  W3dBibObjetosUsuarios(const std::vector<std::string>& claves);
// "Duplicate": una copia del recurso (malla, material) con nombre libre, sin usuarios. false + motivo
// si el tipo no lo sabe hacer. 'idNuevo' = la copia.
bool W3dVistaRecDuplicar(int vista, const std::string& id, std::string* idNuevo, std::string* motivo);
// los datos por tipo (Properties)
void W3dVistaRecDatos(int vista, const std::string& id, std::vector<W3dRecursoDato>& out);
// DENTRO / FUERA del .w3d (-1 = el tipo no tiene archivo propio)
int  W3dVistaRecUbicacion(int vista, const std::string& id);
bool W3dVistaRecFijarUbicacion(int vista, const std::string& id, int ubicacion, const std::string& rutaExterna,
                               std::string* idNuevo, std::string* motivo);
std::string W3dVistaRecRutaExternaSugerida(int vista, const std::string& id);

// ---------------------------------------------------------------------------
//  LO PURGADO QUE VUELVE: un material purgado queda en su cementerio y una textura purgada EN
//  CALIENTE (cargada) tambien (TexturaPurgar: sin GPU, pero el objeto vivo). Si un undo vuelve a
//  ponerlos en uso (el material de un mesh part), el material vuelve a su lista y su textura se
//  recarga (o se repunta a la que ya este cargada con esa ruta). Lo llaman el undo y los listados.
// ---------------------------------------------------------------------------
void W3dRecursosRevisarPurgados();

// ---------------------------------------------------------------------------
//  EL RECURSO ACTIVO (lo elige la biblioteca del outliner; Properties lo muestra en su pestania
//  "Malla 3D"). Se suelta cuando la escena toma la seleccion (se ELIGE un objeto, aunque sea el mismo
//  que seguia activo en Edit Mode: W3dSeleccionSerial) o cambia el objeto activo, cuando el recurso
//  deja de existir o al cerrar el proyecto.
// ---------------------------------------------------------------------------
void W3dRecursoActivar(int vista, const std::string& id);
void W3dRecursoDesactivar();
// false = no hay (o dejo de valer). 'vista'/'id' opcionales.
bool W3dRecursoActivo(int* vista, std::string* id);
// el renombre de un recurso arrastra al activo (lo llama W3dVistaRecRenombrar)
void W3dRecursoActivoRenombrado(int vista, const std::string& viejo, const std::string& nuevo);
Material*     W3dRecursoActivoMaterial();    // NULL si el activo no es un material
MallaRecurso* W3dRecursoActivoMalla();       // idem una malla
// la textura activa para la VISTA PREVIA: la cargada, o se carga (una referencia que se suelta
// al cambiar de activo). NULL si no es una textura o no se pudo cargar.
Texture*      W3dRecursoActivoTextura();
// cuantas veces cambio el activo (Properties lo mira para re-bindear sin comparar strings)
unsigned      W3dRecursoActivoVersion();
// la textura de 'id' YA CARGADA (NULL = no esta en memoria: no carga nada)
Texture*      W3dTexturaCargadaDe(const std::string& id);
// la CLAVE de una ruta de textura (la del cache: separador '/', sin "./" ni "dir/../"): dos rutas que
// nombran el mismo archivo dan la misma
std::string   W3dTexturaClave(const std::string& ruta);
// el MATERIAL de un id de la biblioteca (NULL = no hay)
Material*     W3dMaterialDeId(const std::string& id);

// ---------------------------------------------------------------------------
//  GUARDADO / CARGA / CIERRE (GuardarW3D, import_w3d, ReiniciarEscena)
// ---------------------------------------------------------------------------
// ANTES de escribir los materiales: anota la ruta de cada textura cargada con carpeta (la
// ingesta la cambia al nombre de entrada y la carpeta tiene que seguirla)
void W3dRecursosVistaGuardarAntes();
// el bloque "carpetas", el registro "texturas" y el de "archivos" (con coma final si hay algo).
// 'esc' = el escritor del guardado (una textura se nombra solo si su entrada va a estar).
void W3dRecursosVistaGuardarJson(std::string& s, W3dContenedorEscritor* esc);
// las texturas del proyecto que viven SOLO EN MEMORIA y nadie usa (una "New Texture" o una pintada
// que todavia no se asigno): la vista las lista como huerfanas, y un huerfano se conserva hasta
// purgarlo. Sin usuarios, ningun Asset() las lleva al archivo: van por aca (antes del JSON, asi su
// carpeta cosmetica tambien queda). Lo mismo los ARCHIVOS externos ("ext:") que nadie nombra
// (una textura o un sonido que el usuario paso afuera). 'esc' = el escritor del guardado.
void W3dRecursosVistaGuardarEnMemoria(W3dContenedorEscritor* esc);
// las entradas de texturas y archivos PURGADOS (o pasados afuera): el escritor no las conserva
void W3dRecursosVistaGuardarDescartes(W3dContenedorEscritor* esc);
// el guardado salio: lo purgado ya no esta en el archivo
void W3dRecursosVistaGuardado();
// lee "carpetas", "texturas" y "archivos" del proyecto que se abre (y migra el formato viejo)
void W3dRecursosVistaLeerJson(JVal* raiz);
// cierre del proyecto: carpetas, activo, vista previa, purgados y caches
void W3dRecursosVistaCerrarProyecto();
// el contenido cambio por afuera (un script guardado, una textura importada): se re-escanea
void W3dRecursosVistaInvalidar();

#endif
