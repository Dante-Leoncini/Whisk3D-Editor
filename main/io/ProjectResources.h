#ifndef PROJECT_RESOURCES_H
#define PROJECT_RESOURCES_H
// ============================================================================
//  RecursosProyecto — los RECURSOS del proyecto vistos como la BIBLIOTECA: el
//  explorador del contenido del .w3d que muestra el OUTLINER (vista "Biblioteca")
//  y lo que Properties muestra cuando se elige uno ("recurso activo").
//
//  LOS TIPOS: mallas 3D, materiales, texturas, animaciones (animsets y sus clips de
//  jerarquia), prefabs, escenas/juegos, sonidos, scripts, fuentes y videos. Cada tipo
//  tiene UN proveedor (W3dResourceProvider) que sabe listar sus items (nombre, icono,
//  carpeta, cuantos lo usan) y hacer las acciones de la biblioteca (renombrar, mover a
//  una carpeta, borrar, los objetos que lo usan, dentro/fuera del .w3d). Mallas,
//  materiales, texturas, animaciones y los ARCHIVOS (sonidos, scripts, fuentes, videos:
//  las entradas de su carpeta del contenedor y lo que los objetos nombran) los trae este
//  modulo; ESCENAS y PREFABS los registra io/RaicesEditor (W3dRoots.h). Los usuarios se
//  cuentan en TODAS las raices cargadas (una malla que usa solo otra escena no es huerfana).
//  Las constantes de tipo se llaman W3D_VISTA_* por historia (la fase anterior tenia una
//  vista del outliner por tipo): hoy son el TIPO de un recurso y el FILTRO de la biblioteca.
//
//  LAS CARPETAS son COSMETICAS y son UN SOLO ARBOL para TODOS los tipos: una carpeta
//  "Personaje" puede tener su malla, su textura, su animset y su prefab. No cambian nada
//  del codigo ni del runtime. Una carpeta es una RUTA con '/' ("A/B" = B adentro de A),
//  normalizada como la de las mallas. Se guardan en project.json:
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
//  TexturePurge). DENTRO / FUERA: un archivo puede vivir adentro del .w3d (una entrada) o
//  AFUERA, al lado (una referencia "ext:" relativa al .w3d, ver W3dContainer.h): pasarlo
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
class W3dContainerWriter;
struct MeshResource;
struct JVal;

// los TIPOS de recurso (y los filtros de la biblioteca). En "carpetas" y en el harness van por
// su CLAVE (W3dViewKey), nunca por el numero. Los nuevos, siempre al FINAL.
enum W3dViewRec {
    W3D_VIEW_SCENE = 0,     // (no es un tipo: el arbol de la escena)
    W3D_VIEW_MESHES,
    W3D_VIEW_MATERIALS,
    W3D_VIEW_TEXTURES,
    W3D_VIEW_ANIMATIONS,
    W3D_VIEW_PREFABS,      // io/RaicesEditor
    W3D_VIEW_SCENES,       // escenas y juegos (io/RaicesEditor)
    W3D_VIEW_LIBS,         // (sin proveedor: las librerias externas son VISTAS del outliner)
    W3D_VIEW_SOUNDS,       // archivos de sounds/
    W3D_VIEW_SCRIPTS,      // archivos .lua de scripts/
    W3D_VIEW_FONTS,        // archivos de fonts/ (.ttf, .otf, .w3dfnt)
    W3D_VIEW_VIDEOS,       // archivos de videos/
    W3D_VIEW_SHADERS,      // archivos .glsl de shaders/ (los shaders escritos de los materiales)
    W3D_VIEW_MISSING,      // los ARCHIVOS QUE FALTAN: referencias a archivos de afuera que no estan en esta maquina
    W3D_VIEWS              // tope
};
const char* W3dViewKey(int view);                 // "escena", "mallas", "materiales"...
int         W3dViewByKey(const std::string& key); // -1 = no es una vista
const char* W3dTitleView(int view);                 // en INGLES (la UI lo pasa por T())
const char* W3dViewSingular(int view);              // "3D Mesh", "Material"... en INGLES
int         W3dIconView(int view);                  // IconType del tipo

// un recurso tal como lo lista su tipo
struct W3dResourceItem {
    int  type;              // W3dViewRec (lo pone el listado)
    std::string id;         // identidad ESTABLE en su tipo (el nombre del recurso, la entrada de la textura)
    std::string name;       // lo que se muestra
    std::string folder;     // carpeta cosmetica NORMALIZADA ("" = la raiz)
    std::string entry;      // su entrada del contenedor / su ruta ("" = todavia no se guardo)
    std::string info;       // datos del tipo para Properties, YA en el idioma de la UI ("24 vertices, 2 parts", "256x256"...)
    int  icon;              // IconType
    int  users;             // cuantos lo usan (0 = HUERFANO)
    bool readOnly;          // no se renombra, no se mueve, no se borra (una libreria externa)
    bool renamable;         // el tipo sabe renombrarlo (una textura: si ya es una entrada del proyecto)
    // un item que es PARTE de otro (un clip de jerarquia de una biblioteca): 'padre' = el id del item que lo
    // contiene. La vista lo muestra debajo de su padre (una sangria mas, sin carpeta propia: va donde va su
    // padre), no se mueve a otra carpeta, "Purge orphans" no lo toca y se BORRA aunque su padre este en uso
    // (con su paso de undo: es editar el contenido del padre, no purgar un recurso).
    std::string parent;
    long bytes;              // cuanto ocupa: cargado, lo que ocupa en memoria (una textura: en la GPU); si no, su archivo
                             // (-1 = no se sabe / no aplica, un material)
    bool loaded;            // esta en memoria (false = guardado en el proyecto o en un archivo, sin cargar)
    int  check;             // la casilla de la fila: -1 = no tiene, 0 = sin tildar, 1 = tildada (las escenas ABIERTAS en el
                             // editor: tildada = cargada y a la vista). La cambia el proveedor (Tildar)
    W3dResourceItem() : type(0), icon(-1), users(0), readOnly(false), renamable(true), bytes(-1), loaded(true),
                       check(-1) {}
};

// un DATO de un recurso para Properties: etiqueta y valor, ya en el idioma de la UI
struct W3dResourceData {
    std::string label, value;
    W3dResourceData() {}
    W3dResourceData(const std::string& e, const std::string& v) : label(e), value(v) {}
};

// EL PROVEEDOR de un tipo de recurso. Vive toda la sesion (un static del modulo que lo
// registra); el registro no es dueno. Todo lo que modifica lo llama el editor SIN undo: el
// paso de undo lo arma este modulo (W3dViewRecRename / Mover / carpetas) o el proveedor.
class W3dResourceProvider {
public:
    virtual ~W3dResourceProvider() {}
    // TODOS los items del tipo (el orden no importa: la vista ordena por carpeta y nombre)
    virtual void List(std::vector<W3dResourceItem>& out) = 0;
    // uno solo (por id). Por defecto busca en Listar; un proveedor con muchos items lo pisa.
    virtual bool Info(const std::string& id, W3dResourceItem& out);
    // la CARPETA cosmetica del item (ya normalizada). false = no existe o no se puede.
    virtual bool SetFolder(const std::string& id, const std::string& folder) = 0;
    // renombrar CON SU PASO DE UNDO (el proveedor sabe cual es el suyo: el de las mallas y los
    // materiales ya existia). El nombre se hace unico. 'final' = el que quedo. false = no se pudo.
    virtual bool Rename(const std::string& id, const std::string& newValue, std::string* final)
        { (void)id; (void)newValue; (void)final; return false; }
    // el id que pediria 'nuevo' ANTES de hacerlo unico (para avisar "ese nombre ya estaba").
    // Por defecto el nombre normalizado (id == nombre); las texturas lo arman como su entrada.
    virtual std::string IdRequest(const std::string& id, const std::string& newValue);
    // borrar un HUERFANO. false + motivo (en ingles, la UI lo pasa por T()) = no se pudo.
    virtual bool Delete(const std::string& id, std::string* reason)
        { (void)id; if (reason) *reason = "This resource type can't be deleted"; return false; }
    // borrar un recurso EN USO, dejando a sus usuarios SIN EL (con un paso de undo). false +
    // motivo = no se pudo. KnowsDeleteInUse = el tipo lo sabe hacer (si no, la UI no lo ofrece).
    virtual bool DeleteInUse(const std::string& id, std::string* reason)
        { (void)id; if (reason) *reason = "It is in use"; return false; }
    virtual bool KnowsDeleteInUse() const { return false; }
    // los OBJETOS de la escena que lo usan ("Select Users")
    virtual void Users(const std::string& id, std::vector<Object*>& out) { (void)id; out.clear(); }
    // "Purge orphans" borra los huerfanos de este tipo? (las escenas y los prefabs no: un prefab sin
    // instancias o una escena que nadie nombra se borran de a uno, a pedido, nunca en montonera)
    virtual bool Purgeable() const { return true; }
    // LA CASILLA de la fila (W3dResourceItem::tilde): tildar / destildar. false + motivo (en ingles) = no se pudo
    virtual bool Check(const std::string& id, bool on, std::string* reason)
        { (void)id; (void)on; if (reason) *reason = "It has no checkbox"; return false; }
    // "Duplicate": una copia sin usuarios con nombre libre. false + motivo = el tipo no sabe.
    virtual bool Duplicate(const std::string& id, std::string* idNew, std::string* reason)
        { (void)id; (void)idNew; if (reason) *reason = "This resource type can't be duplicated"; return false; }
    // los DATOS del tipo para Properties (resolucion y alfa de una textura, frecuencia y duracion de
    // un sonido, vertices de una malla...). Por defecto la linea 'info' del item.
    virtual void Data(const std::string& id, std::vector<W3dResourceData>& out);
    // DENTRO / FUERA del .w3d: 0 = adentro (una entrada), 1 = afuera ("ext:"), -1 = el tipo no
    // tiene archivo propio (materiales, escenas...: viven en project.json o en su registro)
    virtual int  Location(const std::string& id) { (void)id; return -1; }
    // la ruta que tendria afuera (de disco, al lado del .w3d) y el cambio (con su paso de undo).
    // 'idNew' = el id con el que quedo (la ruta nueva: el id de un archivo ES su ruta)
    virtual std::string PathExternalSuggested(const std::string& id) { (void)id; return std::string(); }
    virtual bool SetLocation(const std::string& id, int location, const std::string& externalPath,
                                std::string* idNew, std::string* reason)
        { (void)id; (void)location; (void)externalPath; (void)idNew; if (reason) *reason = "It has no file of its own"; return false; }
};

// registra el proveedor de un tipo (prefabs, escenas). Pisa al anterior. NULL = el tipo vuelve
// a no tener datos.
void W3dResourcesViewRegister(int view, W3dResourceProvider* p);
W3dResourceProvider* W3dResourcesProviderView(int view);   // NULL = sin datos todavia
// los items del tipo (vacio si no tiene proveedor)
void W3dViewRecList(int view, std::vector<W3dResourceItem>& out);
bool W3dViewRecInfo(int view, const std::string& id, W3dResourceItem* out);

// ---------------------------------------------------------------------------
//  LA BIBLIOTECA: todos los items de todos los tipos (o los de un FILTRO)
// ---------------------------------------------------------------------------
// filtro = un tipo, o -1 = todos. Cada item trae su 'tipo'.
void W3dLibraryList(int filter, std::vector<W3dResourceItem>& out);
// los tipos que la biblioteca ofrece en su filtro (los que tienen proveedor), en el orden del menu
void W3dLibraryTypes(std::vector<int>& out);
// la CLAVE unica de un item en la biblioteca: "<clave del tipo>:<id>" ("mallas:Arbol")
std::string W3dBibKey(int type, const std::string& id);
// al reves; false = no es una clave de item
bool        W3dBibByKey(const std::string& key, int* type, std::string* id);
// ACTUALIZAR TEXTURA: vuelve a leer la imagen 'clave' (su id en la biblioteca) del archivo de disco del que salio y la
// sube de nuevo (se edito afuera). false si no se pudo: 'motivo' en INGLES (la UI lo pasa por T()) y 'detalle' la ruta
bool W3dTextureUpdate(const std::string& key, std::string* reason, std::string* detail = NULL);
// lo mismo desde la UI: actualiza cada una y avisa como salio (una notificacion)
void W3dTexturesUpdateNotifying(const std::vector<std::string>& keys);
// la clave de biblioteca de una textura cargada ("" = ninguna)
class Texture;
std::string W3dTextureKey(const Texture* t);

// ---------------------------------------------------------------------------
//  CARPETAS (cosmeticas, UN arbol para todos los tipos)
// ---------------------------------------------------------------------------
std::string W3dFolderNormalize(const std::string& path);   // "a / b//" -> "a/b"
std::string W3dFolderParent(const std::string& path);      // "a/b" -> "a"; "a" -> ""
std::string W3dFolderLeaf(const std::string& path);        // "a/b" -> "b"
bool        W3dFolderInside(const std::string& path, const std::string& folder); // ruta == carpeta o cuelga de ella
// TODAS las carpetas: las creadas (aunque esten vacias) + las de los items (con sus
// ancestros), ordenadas y sin repetir. 'items' = los de TODOS los tipos (W3dLibraryList(-1)).
void W3dFoldersLibrary(const std::vector<W3dResourceItem>& items, std::vector<std::string>& out);
void W3dFoldersAll(std::vector<std::string>& out);        // idem, listando todo
const std::vector<std::string>& W3dFoldersCreated();
// NUEVA carpeta adentro de 'padre' ("" = la raiz), con 'nombre' hecho unico entre sus hermanas
// ("" = "New Folder"). Con undo. 'ruta' = la que quedo.
bool W3dNewFolder(const std::string& parent, const std::string& name, std::string* path);
// RENOMBRAR la carpeta (su ultimo tramo): arrastra sus subcarpetas y sus items. Con undo.
bool W3dFolderRename(const std::string& path, const std::string& name, std::string* newPath);
// true si ningun item (de ningun tipo) cuelga de la carpeta (ni de sus subcarpetas)
bool W3dFolderEmpty(const std::string& path);
// BORRAR una carpeta VACIA (y sus subcarpetas vacias). Con undo. false + motivo si tiene items.
bool W3dFolderDelete(const std::string& path, std::string* reason);
// MUDAR una carpeta (con sus subcarpetas y sus recursos) ADENTRO de 'destino' ("" = la raiz), como
// en un explorador de archivos: conserva su nombre (hecho unico entre sus nuevas hermanas) y la
// carpeta de la que sale sigue existiendo. Con undo. false = no existe, o 'destino' es ella misma o
// cuelga de ella. 'newPath' = donde quedo.
bool W3dFolderMove(const std::string& path, const std::string& destination, std::string* newPath);
// GRUPO de operaciones de carpetas (la SELECCION MULTIPLE del outliner, el modo MOVER con la G):
// todo lo que pase entre Iniciar y Fin es UN SOLO paso de undo. Anida. Cancelar (con el grupo de
// afuera abierto) DESHACE lo que se hizo en el grupo y lo cierra sin dejar paso (el Esc del modo mover).
void W3dViewRecGroupStart();
void W3dViewRecGroupEnd();
void W3dViewRecGroupCancel();
// Ctrl+Z / Ctrl+Y con un grupo ABIERTO (el modo mover de la biblioteca a medias): el paso que el grupo
// confirmaria es la diferencia contra su foto y se comia lo deshecho. Se CANCELA el modo (lo elegido vuelve
// a donde estaba, como con Esc) y ese Ctrl+Z no hace nada mas. true = habia un grupo (el undo no sigue). Lo
// llaman UndoUndo / UndoRedo. El gancho lo pone el outliner (sale de su modo mover).
bool W3dViewRecBeforeUndo();
extern void (*W3dViewRecGroupCancelHook)();
// la firma de las carpetas (las creadas + la de cada item): cambia con cualquier cambio de carpeta
// (el seguimiento de lo no guardado, CambiosProyecto). 'items' = la biblioteca ya listada (sin eso la lista)
std::string W3dFoldersSignature();
std::string W3dFoldersSignature(const std::vector<W3dResourceItem>& items);
// el ORDEN MANUAL de la biblioteca (arrastrar un recurso entre otros dos): una lista de claves de biblioteca
// ("mallas:Arbol"). Lo que esta en la lista va primero y en ese orden; el resto despues, por nombre. Se guarda en el
// .w3d ("orden") y tiene undo (es parte del paso de las carpetas).
int  W3dBibOrder(const std::string& key);                     // su lugar (-1 = sin orden manual)
void W3dBibOrderSet(const std::vector<std::string>& siblings); // estas claves quedan seguidas en este orden

// ---------------------------------------------------------------------------
//  ACCIONES SOBRE UN ITEM (con su paso de undo cuando corresponde)
// ---------------------------------------------------------------------------
bool W3dViewRecMove(int view, const std::string& id, const std::string& folder);
bool W3dViewRecRename(int view, const std::string& id, const std::string& newValue, std::string* final);
// borra UN huerfano (sin undo: el "Purge" de Blender). false + motivo (ingles).
bool W3dViewRecDelete(int view, const std::string& id, std::string* reason);
// borra un recurso EN USO dejando a sus usuarios sin el (con undo si el tipo lo tiene). Para un
// huerfano es lo mismo que W3dViewRecDelete.
bool W3dViewRecDeleteForced(int view, const std::string& id, std::string* reason);
bool W3dViewRecKnowsDeleteInUse(int view);
// "Purge orphans": borra TODOS los huerfanos que se puedan de ese tipo (-1 = de todos los tipos).
// Devuelve cuantos; 'nombres' = cuales.
int  W3dViewRecPurge(int view, std::vector<std::string>* names);
// no se puede borrar NI a la fuerza (la imagen de una sonda de reflejo: lo dice el motivo, ya traducido, con su nombre)
bool W3dViewRecUntouchable(int view, const std::string& id, std::string* reason);
// "Select Users": selecciona en la escena los objetos que lo usan (con undo de seleccion) y
// despliega sus padres para que se vean en el arbol. Devuelve cuantos.
int  W3dViewRecSelectUsers(int view, const std::string& id);
// idem con VARIOS recursos (claves de biblioteca, W3dBibKey): la union de sus usuarios, un solo
// paso de undo. Sin objetos que seleccionar devuelve 0 y la seleccion queda como estaba.
int  W3dBibSelectUsers(const std::vector<std::string>& keys);
int  W3dViewRecSelectUsers(int view, const std::vector<std::string>& ids);
// cuantos OBJETOS de la escena usan esos recursos (la union). Puede ser 0 aunque tengan usuarios: una
// malla huerfana del registro, un script o un flipbook cuentan como usuarios pero no se seleccionan.
// "Select Users" solo se ofrece si hay alguno.
int  W3dViewRecObjectsUsers(int view, const std::vector<std::string>& ids);
int  W3dBibObjectsUsers(const std::vector<std::string>& keys);
// "Duplicate": una copia del recurso (malla, material) con nombre libre, sin usuarios. false + motivo
// si el tipo no lo sabe hacer. 'idNew' = la copia.
bool W3dViewRecDuplicate(int view, const std::string& id, std::string* idNew, std::string* reason);
// los datos por tipo (Properties)
void W3dViewRecData(int view, const std::string& id, std::vector<W3dResourceData>& out);
// DENTRO / FUERA del .w3d (-1 = el tipo no tiene archivo propio)
int  W3dViewRecLocation(int view, const std::string& id);
bool W3dViewRecSetLocation(int view, const std::string& id, int location, const std::string& externalPath,
                               std::string* idNew, std::string* reason);
std::string W3dViewRecPathExternalSuggested(int view, const std::string& id);
// true si 'ruta' es el ARCHIVO ORIGINAL de alguna malla importada (Mesh::origen): solo procedencia (la geometria
// viaja adentro del .w3d). Que falte no pierde nada: el guardado no lo trata como un archivo que falta
bool W3dPathIsMeshOrigin(const std::string& path);

// ---------------------------------------------------------------------------
//  LO PURGADO QUE VUELVE: un material purgado queda en su cementerio y una textura purgada EN
//  CALIENTE (cargada) tambien (TexturePurge: sin GPU, pero el objeto vivo). Si un undo vuelve a
//  ponerlos en uso (el material de un mesh part), el material vuelve a su lista y su textura se
//  recarga (o se repunta a la que ya este cargada con esa ruta). Lo llaman el undo y los listados.
// ---------------------------------------------------------------------------
void W3dResourcesCheckPurged();

// ---------------------------------------------------------------------------
//  EL RECURSO ACTIVO (lo elige la biblioteca del outliner; Properties lo muestra en su pestania
//  "Malla 3D"). Se suelta cuando la escena toma la seleccion (se ELIGE un objeto, aunque sea el mismo
//  que seguia activo en Edit Mode: W3dSelectionSerial) o cambia el objeto activo, cuando el recurso
//  deja de existir o al cerrar el proyecto.
// ---------------------------------------------------------------------------
void W3dResourceActivate(int view, const std::string& id);
void W3dResourceDeactivate();
// false = no hay (o dejo de valer). 'vista'/'id' opcionales.
bool W3dResourceActive(int* view, std::string* id);
// UN RECURSO DE UNA LIBRERIA VINCULADA (la Biblioteca mirando otro .w3d): Propiedades lo MUESTRA, de solo lectura (su
// tipo, si este proyecto lo usa, si esta cargado...). 'lib' = el indice de la libreria; 'idGlobal' = con el que lo nombra
// este proyecto (W3dExtLibGlobalId: "Lib/Cuerpo", "lib:Lib/textures/piel.png", la clave de un proxy). Se suelta igual
// que el activo (al elegir un objeto, otro recurso o al cerrar)
void W3dResourceExtActivate(int lib, const W3dResourceItem& it, const std::string& idGlobal);
bool W3dResourceExtActive(int* lib, W3dResourceItem* it, std::string* idGlobal);
// el renombre de un recurso arrastra al activo (lo llama W3dViewRecRename)
void W3dActiveRenamedResource(int view, const std::string& old, const std::string& newValue);
Material*     W3dActiveResourceMaterial();    // NULL si el activo no es un material
MeshResource* W3dActiveResourceMesh();        // idem una malla
// la textura activa para la VISTA PREVIA: la cargada, o se carga (una referencia que se suelta
// al cambiar de activo). NULL si no es una textura o no se pudo cargar.
Texture*      W3dActiveResourceTexture();
// cuantas veces cambio el activo (Properties lo mira para re-bindear sin comparar strings)
unsigned      W3dActiveResourceVersion();
// la textura de 'id' YA CARGADA (NULL = no esta en memoria: no carga nada)
Texture*      W3dLoadedTextureOf(const std::string& id);
// la CLAVE de una ruta de textura (la del cache: separador '/', sin "./" ni "dir/../"): dos rutas que
// nombran el mismo archivo dan la misma
std::string   W3dTextureKey(const std::string& path);
// el MATERIAL de un id de la biblioteca (NULL = no hay)
Material*     W3dMaterialById(const std::string& id);

// ---------------------------------------------------------------------------
//  GUARDADO / CARGA / CIERRE (SaveW3D, import_w3d, RestartScene)
// ---------------------------------------------------------------------------
// ANTES de escribir los materiales: anota la ruta de cada textura cargada con carpeta (la
// ingesta la cambia al nombre de entrada y la carpeta tiene que seguirla)
void W3dResourcesViewSaveBefore();
// el bloque "carpetas", el registro "texturas" y el de "archivos" (con coma final si hay algo).
// 'esc' = el escritor del guardado (una textura se nombra solo si su entrada va a estar).
void W3dResourcesViewSaveJson(std::string& s, W3dContainerWriter* esc);
// las texturas del proyecto que viven SOLO EN MEMORIA y nadie usa (una "New Texture" o una pintada
// que todavia no se asigno): la vista las lista como huerfanas, y un huerfano se conserva hasta
// purgarlo. Sin usuarios, ningun Asset() las lleva al archivo: van por aca (antes del JSON, asi su
// carpeta cosmetica tambien queda). Lo mismo los ARCHIVOS externos ("ext:") que nadie nombra
// (una textura o un sonido que el usuario paso afuera). 'esc' = el escritor del guardado.
void W3dResourcesViewSaveInMemory(W3dContainerWriter* esc);
// las entradas de texturas y archivos PURGADOS (o pasados afuera): el escritor no las conserva
void W3dResourcesViewSaveDiscards(W3dContainerWriter* esc);
// el guardado salio: lo purgado ya no esta en el archivo
void W3dResourcesSavedView();
// lee "carpetas", "texturas" y "archivos" del proyecto que se abre (y migra el formato viejo)
void W3dResourcesViewReadJson(JVal* root);
// cierre del proyecto: carpetas, activo, vista previa, purgados y caches
void W3dResourcesViewCloseProject();
// el contenido cambio por afuera (un script guardado, una textura importada): se re-escanea
void W3dResourcesViewInvalidate();

#endif
