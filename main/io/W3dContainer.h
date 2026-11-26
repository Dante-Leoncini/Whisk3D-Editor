#ifndef W3D_CONTAINER_H
#define W3D_CONTAINER_H
#include <string>
#include <vector>
#include <map>
#include <set>

class W3dZipReader;

// ============================================================================
//  W3dContenedor — el .w3d v4 ES UN ZIP y el proyecto entero vive adentro.
//
//  ARBOL (nombres normativos, ver la spec del contenedor seccion 3):
//     mimetype           PRIMERA entrada y SIN COMPRIMIR (estilo OpenDocument)
//     README.txt          texto para el humano que abra el zip. Se REGENERA en
//                        cada guardado y NUNCA se lee: es documentacion, no dato
//     project.json      el JSON raiz de siempre (legible, editable a mano)
//     scenes/           los .w3dui
//     scripts/           los .lua internos
//     textures/  fonts/  sounds/  videos/
//     meshes/            la geometria (.w3db binario / .w3dm texto; .glb de los archivos viejos)
//     animations/       los blobs de vertex anim (.w3danim / .bin)
//     models/           .obj/.wobj/.fbx traidos por el usuario
//     project/          el icono del juego y su arte fuente
//     extra/             lo que el editor no reconoce
//     EXTERNALS.txt       SOLO si hay alguna referencia "ext:"
//
//  ESTILO OPENDOCUMENT: la entrada "mimetype" va PRIMERA, STORE y sin campo
//  extra. El local header del writer mide 30 bytes FIJOS (W3dZip.cpp), asi que
//  el nombre cae en el offset 30 y el contenido en el 38, que es exactamente
//  donde file(1) y xdg-mime buscan la firma de un OpenDocument. No hace falta
//  tocar nada del zip para conseguirlo: alcanza con el ORDEN.
//
//  EL CONTENEDOR SE MONTA, NO SE EXTRAE: al abrir se indexa el zip y se registra
//  como origen virtual de w3dFileSystem::ReadFileBytes (igual que el pak de los
//  juegos compilados). Por eso texturas, fuentes, lua, .w3dui y GLB cargan de
//  ADENTRO sin tocar sus call sites: en memoria la "ruta" de un asset interno ES
//  su nombre de entrada ("textures/pausa.png") y el filesystem la resuelve.
//
//  REFERENCIA INTERNA vs EXTERNA (spec seccion 5): interna = ruta pelada;
//  externa = prefijo "ext:" en el archivo. Los externos NO se copian: se guarda
//  la ruta y, si falta, se avisa (EXTERNALS.txt + notificacion). El prefijo no
//  puede colisionar con un nombre de entrada porque ':' esta prohibido en los
//  nombres de entrada.
// ============================================================================

// ---------------------------------------------------------------------------
//  MONTAJE del proyecto abierto
// ---------------------------------------------------------------------------
// abre el .w3d como contenedor y lo monta (desmonta el anterior). false si no es
// un zip legible o no trae project.json.
bool W3dContainerMount(const std::string& pathW3d);
void W3dContainerUnmount();
bool W3dContainerHasMounted();
// UN PROYECTO QUE TODAVIA NO ES UN ARCHIVO (nuevo sin guardar, o en el navegador): un contenedor SOLO EN MEMORIA, sin
// .w3d detras. Lo que se guarda "adentro del proyecto" (un script del IDE) vive en sus entradas editadas, la biblioteca
// lo lista, los objetos lo cargan, y el primer guardado del proyecto lo hornea al .w3d. Si ya habia uno montado, no
// hace nada. true = hay contenedor
bool W3dContainerEnsure();
bool W3dContainerInMemory();   // el montado es de memoria (no hay .w3d todavia)
// la ruta del .w3d montado ("" = ninguno): el explorador del proyecto por dentro muestra su nombre
std::string W3dContainerMountedPath();
// el lector del contenedor montado (NULL si no hay). Lo usa el guardado
// incremental para reenvasar byte a byte lo que no cambio.
W3dZipReader* W3dContainerReader();
// true si el .w3d de 'ruta' es un contenedor v4 (zip con project.json adentro)
bool W3dContainerIs(const std::string& path);

// ---------------------------------------------------------------------------
//  EDICION EN CALIENTE DE UNA ENTRADA (el IDE guardando un .lua de adentro)
//
//  El .w3d del disco es de SOLO LECTURA mientras esta montado: reescribir un zip
//  por el medio no existe, y aunque existiera romperia la garantia de que el
//  archivo del disco SIEMPRE esta integro (nunca a medio escribir).
//
//  Entonces lo editado vive en un OVERLAY en memoria que el montaje sirve PRIMERO,
//  y el proximo "Guardar proyecto" lo hornea al zip nuevo. Consecuencias buscadas:
//    - el IDE guarda "scripts/menu.lua" y NO aparece una carpeta scripts/ suelta
//      al lado del binario del editor (que es lo que pasaba antes: fopen sobre un
//      nombre de entrada crea un archivo relativo al directorio actual);
//    - el resto del editor (recargar el script, correr el juego, compilar) lee lo
//      NUEVO sin enterarse de nada, porque sigue leyendo por ReadFileBytes;
//    - al guardar el proyecto, el zip nuevo lleva los bytes editados, no los del
//      zip viejo (lo aplican Ingerir y PreserveTransient).
//  El overlay se va con el desmontaje, o sea que guardar el proyecto lo vacia solo.
// ---------------------------------------------------------------------------
// escribe/pisa una entrada EN MEMORIA. false = no hay contenedor montado o el
// nombre no es un nombre de entrada valido.
bool W3dContainerWriteEntry(const std::string& entry,
                                  const void* data, size_t size);
// true si esa entrada esta editada y todavia no se guardo al disco
bool W3dContainerEntryEdited(const std::string& entry);
// los bytes editados (false si esa entrada no esta en el overlay)
bool W3dContainerReadEdited(const std::string& entry, std::vector<unsigned char>& out);
// cuantas entradas hay editadas sin guardar (el aviso de "tenes cambios sin guardar")
size_t W3dEditedContainerCount();
// MUDA una entrada del overlay a otro nombre SIN copiar sus bytes (renombrar una textura importada
// en la sesion: el N95 no tiene memoria para dos copias). Pisa lo que hubiera editado con el nombre
// nuevo. false = no hay contenedor, 'vieja' no esta editada o 'nueva' no es un nombre de entrada.
bool W3dContainerMoveEdited(const std::string& old, const std::string& newValue);
// true si la entrada existe en el contenedor montado (editada o en el zip del disco)
bool W3dContainerExists(const std::string& entry);

// ---------------------------------------------------------------------------
//  CONSULTAS SOBRE EL CONTENEDOR MONTADO (las usa la UI: IDE, tarjetas)
// ---------------------------------------------------------------------------
// las entradas que cuelgan de 'carpeta' ("scripts/"), ordenadas y sin repetidos,
// incluyendo las editadas en caliente. Vacio si no hay contenedor montado.
void W3dContainerListFolder(const std::string& folder, std::vector<std::string>& out);
// EXTRAER una entrada a un archivo de DISCO (para editarla con otro programa).
// No cambia nada adentro del .w3d: es una copia hacia afuera.
bool W3dContainerExtract(const std::string& entry, const std::string& diskPath);

// ---------------------------------------------------------------------------
//  IMPORTAR UN ASSET (elegiste un archivo de tu disco desde el editor)
//
//  LA REGLA, en una linea: lo que importas pasa a ser DEL PROYECTO. Se COPIA
//  adentro del .w3d en el acto y a partir de ahi se referencia como INTERNO
//  ("textures/piso.png"), asi que mandar el .w3d a otra maquina lleva todo.
//
//  Antes la copia recien pasaba al guardar: entre el "abrir" y el "guardar" el
//  proyecto dependia de un archivo del disco del usuario, y si lo movia o el
//  pendrive se desconectaba en el medio, el asset se perdia sin que nadie lo
//  hubiera decidido. Ahora la decision es explicita y en el momento.
//
//  SI QUERES QUE SIGA AFUERA A PROPOSITO (una textura que editas todo el tiempo
//  con otro programa, un asset compartido entre proyectos): W3dExternalRefMark()
//  con la ruta de disco. Esa ref se escribe "ext:..." y NO se copia; queda
//  listada en el EXTERNALS.txt y se avisa si falta.
// ---------------------------------------------------------------------------
// copia 'diskPath' adentro del contenedor montado y devuelve su nombre de
// entrada. Deduplica por CONTENIDO: importar dos veces la misma imagen da UNA
// entrada. Sin contenedor montado (proyecto v3) devuelve 'diskPath' tal cual, o
// sea que el editor sigue funcionando igual con los proyectos viejos.
// 'categoria' NULL = la que le toca por extension.
std::string W3dImportAsset(const std::string& diskPath, const char* category = 0);
// CARGAR UN ARCHIVO DE AFUERA (el explorador de archivos, soltar un archivo en el editor): QUEDA AFUERA, referenciado por
// su ruta ("ext:" en el .w3d). Lo que se carga de afuera es externo; adentro del .w3d vive lo que se CREA en el programa
// (una textura nueva, un script nuevo) o lo que el usuario mete con la Biblioteca (Guardado: adentro). Devuelve la ruta
// a usar. En el NAVEGADOR no hay un "afuera" que dure (el archivo subido vive en la memoria de la pagina): ahi se copia
// adentro como antes
std::string W3dLoadExternal(const std::string& diskPath, const char* category = 0);
// EL ORIGEN de un asset importado: el archivo de disco del que se copio adentro (lo anota W3dImportAsset y se guarda
// en project.json, "origenes"). "Actualizar textura" lo vuelve a leer de ahi cuando se edito afuera. Sobrevive al
// desmontaje (guardar desmonta y vuelve a montar): lo vacia abrir o crear otro proyecto
void        W3dAssetOriginAnnotate(const std::string& entry, const std::string& diskPath);
std::string W3dAssetOrigin(const std::string& entry);                 // "" = no se sabe
void        W3dAssetOriginMove(const std::string& old, const std::string& newValue);   // (la entrada se renombro)
void        W3dAssetOriginsClear();
void        W3dAssetOriginsList(std::vector<std::pair<std::string, std::string> >& out);   // (entrada, ruta)

// ---------------------------------------------------------------------------
//  REFERENCIAS EXTERNAS ("ext:")
//
//  En memoria una ref externa queda como la ruta de disco RESUELTA (asi todos
//  los cargadores siguen andando sin cambios). Este registro es el que recuerda
//  que en el ARCHIVO llevaba el prefijo, para volver a escribirla como externa.
// ---------------------------------------------------------------------------
void W3dExternalRefMark(const std::string& resolvedPath);
// la ref vuelve a ser comun (la biblioteca la paso ADENTRO del .w3d)
void W3dExternalRefUnmark(const std::string& resolvedPath);
bool W3dRefIsExternal(const std::string& resolvedPath);
void W3dExternalRefClear();
// TODAS las refs externas del proyecto abierto (rutas de disco resueltas). La
// necesita "Compilar juego": un archivo que quedo AFUERA del contenedor a
// proposito (los .cap de los rieles del demo, por ejemplo) igual tiene que
// viajar con el juego, o el juego abre sin el.
void W3dExternalRefList(std::vector<std::string>* out);

// ---------------------------------------------------------------------------
//  ENTRADAS DE SERVICIO (las que NO son datos del proyecto)
// ---------------------------------------------------------------------------
// el tipo MIME del .w3d. Son 36 bytes EXACTOS y la entrada "mimetype" los lleva
// SIN salto de linea final (si no, la firma del offset 38 no cierra).
extern const char* const kW3dMimetype;
// el texto de README.txt: fijo y DETERMINISTA (ni fecha, ni usuario, ni rutas:
// cualquier campo volatil rompe el round-trip byte a byte).
std::string W3dContainerReadme();
// true si 'n' es una entrada de SERVICIO del contenedor (mimetype, README.txt,
// project.json, EXTERNALS.txt): las genera el guardado, no las referencia nadie
// y no se preservan del archivo viejo (se vuelven a escribir).
bool W3dIsServiceEntry(const std::string& n);

// ---------------------------------------------------------------------------
//  NOMBRES DE ENTRADA
// ---------------------------------------------------------------------------
// slug ASCII minusculas de UN componente (spec seccion 3): translitera, baja a
// minusculas, deja [a-z0-9._-], colapsa '_', saca los bordes, evita los nombres
// reservados de Windows y trunca a 60 bytes. Vacio -> "sin_nombre".
std::string W3dSlugEntry(const std::string& name);
// la carpeta que le toca a una ruta por su EXTENSION ("texturas", "scripts"...).
// Lo que no se reconoce cae en "extra".
const char* W3dCategoryByExtension(const std::string& path);
// true si 'r' arranca con una de las carpetas reservadas del layout: o sea, si
// parece un NOMBRE DE ENTRADA y no una ruta de disco.
bool W3dIsEntryName(const std::string& r);
// una RUTA COSMETICA (la carpeta de un recurso del outliner: "texturas/UI") tal como se escribe
// en el JSON: si parece un nombre de entrada se le antepone '/' para que la verificacion del
// guardado no la tome por una referencia a una entrada que no existe (y aborte). Al leerla, la
// normalizacion de carpetas (W3dMeshFolderNormalize) descarta la barra inicial.
inline std::string W3dCosmeticPathJson(const std::string& r) {
    return W3dIsEntryName(r) ? std::string("/") + r : r;
}

// TEST del harness (comando 'contenedor'): hace que el escritor SE OLVIDE de la
// primera entrada que ingiere, o sea que el JSON la nombra y el zip no la tiene.
// Es la unica forma de probar la VERIFICACION previa al cierre, que es la
// mitigacion del fallo mas caro del diseno (el proyecto abriria "sin la textura"
// y no fallaria nada). Siempre false en el editor real.
extern bool g_w3dContForgetOne;

// ---------------------------------------------------------------------------
//  ESCRITOR del contenedor (solo lo usa el guardado)
//
//  DOS FASES a proposito:
//    1) el guardado camina la escena, arma el JSON y va INGIRIENDO cada asset:
//       cada uno queda anotado con su ORIGEN (bytes en memoria, un archivo de
//       disco, o una entrada del contenedor VIEJO que se reenvasa tal cual).
//       No se levantan los bytes de todos: pico de RAM = la entrada mas grande.
//    2) Write() vuelca el zip en orden DETERMINISTA (mimetype primero,
//       project.json segundo, el resto alfabetico) a un temporal, y el caller
//       hace el rename.
// ---------------------------------------------------------------------------
struct W3dEntryPend {
    std::string name;      // "textures/pausa.png"
    int         origin;    // 0 = bytes, 1 = archivo de disco, 2 = entrada del zip viejo
    std::vector<unsigned char> bytes;   // origen 0
    std::string source;     // origen 1: la ruta de disco. origen 2: el nombre en el zip viejo
    bool        temp;       // origen 1: borrar 'fuente' al terminar el guardado
    unsigned    crc, size;  // huella del contenido (dedup)
    W3dEntryPend() : origin(0), temp(false), crc(0), size(0) {}
};

// una referencia que quedo AFUERA del .w3d (va a EXTERNALS.txt y a la notificacion)
struct W3dExternal {
    std::string ref;   // "ext:/ruta/Texturas/piso.png"
    std::string who;   // "material \"Piso\"" / "escena \"Menu\""
    bool exists;
    std::string original;   // la ruta EN MEMORIA (antes de reescribirla): la biblioteca la cruza con sus duenos
};
// lo que FALTABA en el ultimo guardado (las referencias externas que no estan en esta maquina): la biblioteca lo muestra
// en "Archivos que faltan" (io/ProjectResources.cpp). Se vacia al abrir otro proyecto
extern std::vector<W3dExternal> g_w3dExternalMissing;

class W3dContainerWriter {
public:
    W3dContainerWriter();
    ~W3dContainerWriter();

    // 'finalPath' = el .w3d destino (de ahi salen los temporales y la base de las
    // rutas relativas de los externos). 'viejo' = el contenedor montado (o NULL).
    void Start(const std::string& finalPath, W3dZipReader* old);

    // EL CORAZON: mete un asset adentro y devuelve lo que hay que ESCRIBIR en el
    // archivo (su nombre de entrada, o "ext:..." si queda afuera). 'ruta' se
    // reescribe con lo que queda EN MEMORIA (el nombre de entrada para los
    // internos; la ruta de disco tal cual para los externos).
    std::string Ingest(std::string& path, const char* category, const std::string& who);

    // CORRIGE el "existe" de una referencia externa ya registrada.
    // Ingerir decide si una referencia existe preguntandole al disco por ESE nombre,
    // y eso es correcto para un archivo... pero no para una referencia que es un
    // PREFIJO. El basePath de una vertex animation nombra una SECUENCIA
    // ("modelos/heroe/agachado/agachado" -> agachado001.obj, agachado002.obj, ...):
    // no existe ningun archivo con ese nombre, asi que TODAS las vertex anims se
    // anotaban como "FALTA" y cada guardado terminaba con el cartel ROJO "Guardado,
    // pero N file(s) external(s) NO estan" sin que faltara nada (52 en el proyecto
    // medidos). El que sabe cual es el archivo de verdad (el primer frame) lo
    // comprueba y lo dice por aca.
    void ExternalCorrectExists(const std::string& ref, bool exists);

    // entradas GENERADAS por el guardado (el JSON, EXTERNALS.txt): bytes en memoria.
    // 'dedup' = false para las entradas de SERVICIO: si "mimetype" entrara al
    // indice por contenido, un asset del usuario con esos mismos 36 bytes se
    // aliasaria a el y el JSON terminaria apuntando a "mimetype".
    bool AddBytes(const std::string& name, const std::string& text, bool dedup = true);
    // idem, pero el contenido quedo en un TEMPORAL de disco (.w3dui, .glb, .bin):
    // se anota la ruta y se borra al final del guardado
    bool AddTemp(const std::string& name, const std::string& tmpPath);

    // un temporal unico en la carpeta del destino (para el .w3dui / el .glb)
    std::string TempPath(const char* label);

    // ¿ya hay una entrada con ese nombre?
    bool Has(const std::string& name) const;

    // lo que la INGESTA hizo hasta ahora con cada ruta ORIGINAL que le llego (la de disco de una
    // particula, un elemento 2D, un material...): pares (original, entrada con la que quedo ADENTRO).
    // Las que quedaron afuera ("ext:") no van. La usa la carpeta cosmetica de una textura de disco,
    // que tiene que seguir a su entrada aunque ningun material la cargue.
    void Ingested(std::vector<std::pair<std::string, std::string> >& out) const;

    // la entrada 'nombre' del contenedor VIEJO pasa TAL CUAL al nuevo, con el mismo nombre
    // (sin leerla a memoria ni recomprimirla): una malla del proyecto que no cambio.
    // false = no hay contenedor viejo, no la trae, o ya hay una entrada con ese nombre.
    bool CopyFromOld(const std::string& name);

    // VERIFICACION (spec seccion 9.1, punto 2): toda referencia interna del JSON y
    // de cada .w3dui tiene su entrada escrita. Sin esto el proyecto abre "sin la
    // textura" y no falla nada: es el fallo mas caro del diseno. false + el nombre
    // de la primera que falta.
    bool Verify(std::string& missing);

    // lo que el contenedor VIEJO traia y el editor no referencia: se preserva
    // VERBATIM (spec seccion 10), salvo lo que cuelgue de meshes/ o animations/
    // (carpetas exclusivas del editor: ahi lo no referenciado se descarta).
    void PreserveTransient();
    // una entrada del contenedor VIEJO que el usuario PURGO (un huerfano del outliner por
    // recursos: una textura que nadie usa): PreserveTransient ya no la conserva. Si algo del
    // guardado la vuelve a referenciar, la ingesta la escribe igual (esto solo afecta a lo que
    // se conservaria sin referencia).
    void Discard(const std::string& name) { forgotten.insert(name); }

    // las dos entradas de servicio del estilo OpenDocument: "mimetype" (que
    // Write() manda PRIMERA) y "README.txt". Se llama en cada guardado: las dos
    // se REGENERAN siempre, nunca se preservan del archivo viejo.
    void WriteHeaderOdf();

    // EXTERNALS.txt (solo si hay alguna ref "ext:")
    void WriteExternal();
    size_t ExternalCount() const { return external.size(); }
    size_t MissingExternalCount() const;
    void MissingExternals(std::vector<W3dExternal>& out) const;
    const std::vector<W3dExternal>& Externals() const { return external; }

    // vuelca el zip a 'tmpPathZip' en orden determinista. false = no se escribio
    // (el archivo anterior queda INTACTO porque el caller todavia no renombro).
    bool Write(const std::string& tmpPathZip);

    // borra los temporales del .w3dui/.glb (se llama pase lo que pase)
    void ClearTemp();

private:
    W3dContainerWriter(const W3dContainerWriter&);
    W3dContainerWriter& operator=(const W3dContainerWriter&);

    std::string FreeName(const char* category, const std::string& base);
    void RegisterExternal(const std::string& ref, const std::string& who, bool exists, const std::string& original = std::string());
    // los bytes REALES de una entrada ya anotada (sea cual sea su origen). false =
    // no se pudieron releer.
    bool EntryBytes(size_t idx, std::vector<unsigned char>& out) const;
    // indice de la entrada que tiene EXACTAMENTE estos bytes, o (size_t)-1.
    // Compara BYTE A BYTE: la huella crc:tam solo sirve para no comparar contra
    // todo el mundo (ver el comentario de 'byFootprint').
    size_t FindByContent(const std::string& footprint, const std::vector<unsigned char>& data) const;
    // anota la entrada 'e' (con su crc y su tamano) y, con 'dedup', tambien en el indice por contenido
    void AddPend(const W3dEntryPend& e, bool dedup);
    // anota una entrada EDITADA en memoria (W3dContainerWriteEntry: van sus bytes nuevos). false = no esta
    bool PendEdited(const std::string& name, bool dedup);
    // anota una entrada del zip VIEJO (se reenvasa tal cual; ver 'named' en el .cpp). false = no se anoto
    bool PendOld(const std::string& name, bool named);

    std::string    dirDestination;
    std::string    destinationPath;
    W3dZipReader*  old;
    int            tmpCounter;
    std::vector<W3dEntryPend>   pend;
    std::map<std::string, size_t> byName;    // nombre -> indice en 'pend'
    // "crc:tam" -> TODAS las entradas con esa huella. Es un INDICE, no una prueba:
    // CRC32 tiene 32 bits, o sea que dos archivos DISTINTOS del mismo tamano pueden
    // compartirlo (y construir el par es trivial: el CRC es lineal). Confiar en el
    // hacia que una textura se convirtiera en OTRA al guardar. Por eso el dedup real
    // lo hace FindByContent comparando byte a byte, igual que W3dImportAsset.
    std::map<std::string, std::vector<size_t> > byFootprint;
    std::map<std::string, std::string> alreadyResolved;  // ruta original -> lo que se escribio
    std::vector<W3dExternal>       external;
    std::set<std::string>         forgotten;   // el hook de test y Descartar (huerfanos purgados)
};

#endif // W3D_CONTAINER_H
