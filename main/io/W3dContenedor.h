#ifndef W3DCONTENEDOR_H
#define W3DCONTENEDOR_H
#include <string>
#include <vector>
#include <map>
#include <set>

class W3dZipLector;

// ============================================================================
//  W3dContenedor — el .w3d v4 ES UN ZIP y el proyecto entero vive adentro.
//
//  ARBOL (nombres normativos, ver la spec del contenedor seccion 3):
//     mimetype           PRIMERA entrada y SIN COMPRIMIR (estilo OpenDocument)
//     LEEME.txt          texto para el humano que abra el zip. Se REGENERA en
//                        cada guardado y NUNCA se lee: es documentacion, no dato
//     proyecto.json      el JSON raiz de siempre (legible, editable a mano)
//     escenas/           los .w3dui
//     scripts/           los .lua internos
//     texturas/  fuentes/  sonidos/  videos/
//     mallas/            la geometria (.w3dm propio; .glb de los archivos viejos)
//     animaciones/       los blobs de vertex anim (.w3danim / .bin)
//     modelos/           .obj/.wobj/.fbx traidos por el usuario
//     proyecto/          el icono del juego y su arte fuente
//     extra/             lo que el editor no reconoce
//     EXTERNOS.txt       SOLO si hay alguna referencia "ext:"
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
//  su nombre de entrada ("texturas/pausa.png") y el filesystem la resuelve.
//
//  REFERENCIA INTERNA vs EXTERNA (spec seccion 5): interna = ruta pelada;
//  externa = prefijo "ext:" en el archivo. Los externos NO se copian: se guarda
//  la ruta y, si falta, se avisa (EXTERNOS.txt + notificacion). El prefijo no
//  puede colisionar con un nombre de entrada porque ':' esta prohibido en los
//  nombres de entrada.
// ============================================================================

// ---------------------------------------------------------------------------
//  MONTAJE del proyecto abierto
// ---------------------------------------------------------------------------
// abre el .w3d como contenedor y lo monta (desmonta el anterior). false si no es
// un zip legible o no trae proyecto.json.
bool W3dContenedorMontar(const std::string& rutaW3d);
void W3dContenedorDesmontar();
bool W3dContenedorHayMontado();
// el lector del contenedor montado (NULL si no hay). Lo usa el guardado
// incremental para reenvasar byte a byte lo que no cambio.
W3dZipLector* W3dContenedorLector();
// true si el .w3d de 'ruta' es un contenedor v4 (zip con proyecto.json adentro)
bool W3dContenedorEs(const std::string& ruta);

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
//      zip viejo (lo aplican Ingerir y PreservarPasajeras).
//  El overlay se va con el desmontaje, o sea que guardar el proyecto lo vacia solo.
// ---------------------------------------------------------------------------
// escribe/pisa una entrada EN MEMORIA. false = no hay contenedor montado o el
// nombre no es un nombre de entrada valido.
bool W3dContenedorEscribirEntrada(const std::string& entrada,
                                  const void* datos, size_t tam);
// true si esa entrada esta editada y todavia no se guardo al disco
bool W3dContenedorEntradaEditada(const std::string& entrada);
// los bytes editados (false si esa entrada no esta en el overlay)
bool W3dContenedorLeerEditada(const std::string& entrada, std::vector<unsigned char>& out);
// cuantas entradas hay editadas sin guardar (el aviso de "tenes cambios sin guardar")
size_t W3dContenedorEditadasCantidad();

// ---------------------------------------------------------------------------
//  CONSULTAS SOBRE EL CONTENEDOR MONTADO (las usa la UI: IDE, tarjetas)
// ---------------------------------------------------------------------------
// las entradas que cuelgan de 'carpeta' ("scripts/"), ordenadas y sin repetidos,
// incluyendo las editadas en caliente. Vacio si no hay contenedor montado.
void W3dContenedorListarCarpeta(const std::string& carpeta, std::vector<std::string>& out);
// EXTRAER una entrada a un archivo de DISCO (para editarla con otro programa).
// No cambia nada adentro del .w3d: es una copia hacia afuera.
bool W3dContenedorExtraer(const std::string& entrada, const std::string& rutaDisco);

// ---------------------------------------------------------------------------
//  IMPORTAR UN ASSET (elegiste un archivo de tu disco desde el editor)
//
//  LA REGLA, en una linea: lo que importas pasa a ser DEL PROYECTO. Se COPIA
//  adentro del .w3d en el acto y a partir de ahi se referencia como INTERNO
//  ("texturas/piso.png"), asi que mandar el .w3d a otra maquina lleva todo.
//
//  Antes la copia recien pasaba al guardar: entre el "abrir" y el "guardar" el
//  proyecto dependia de un archivo del disco del usuario, y si lo movia o el
//  pendrive se desconectaba en el medio, el asset se perdia sin que nadie lo
//  hubiera decidido. Ahora la decision es explicita y en el momento.
//
//  SI QUERES QUE SIGA AFUERA A PROPOSITO (una textura que editas todo el tiempo
//  con otro programa, un asset compartido entre proyectos): W3dRefExternaMarcar()
//  con la ruta de disco. Esa ref se escribe "ext:..." y NO se copia; queda
//  listada en el EXTERNOS.txt y se avisa si falta.
// ---------------------------------------------------------------------------
// copia 'rutaDisco' adentro del contenedor montado y devuelve su nombre de
// entrada. Deduplica por CONTENIDO: importar dos veces la misma imagen da UNA
// entrada. Sin contenedor montado (proyecto v3) devuelve 'rutaDisco' tal cual, o
// sea que el editor sigue funcionando igual con los proyectos viejos.
// 'categoria' NULL = la que le toca por extension.
std::string W3dImportarAsset(const std::string& rutaDisco, const char* categoria = 0);

// ---------------------------------------------------------------------------
//  REFERENCIAS EXTERNAS ("ext:")
//
//  En memoria una ref externa queda como la ruta de disco RESUELTA (asi todos
//  los cargadores siguen andando sin cambios). Este registro es el que recuerda
//  que en el ARCHIVO llevaba el prefijo, para volver a escribirla como externa.
// ---------------------------------------------------------------------------
void W3dRefExternaMarcar(const std::string& rutaResuelta);
bool W3dRefEsExterna(const std::string& rutaResuelta);
void W3dRefExternasLimpiar();
// TODAS las refs externas del proyecto abierto (rutas de disco resueltas). La
// necesita "Compilar juego": un archivo que quedo AFUERA del contenedor a
// proposito (los .cap de los rieles del demo, por ejemplo) igual tiene que
// viajar con el juego, o el juego abre sin el.
void W3dRefExternasListar(std::vector<std::string>* out);

// ---------------------------------------------------------------------------
//  ENTRADAS DE SERVICIO (las que NO son datos del proyecto)
// ---------------------------------------------------------------------------
// el tipo MIME del .w3d. Son 36 bytes EXACTOS y la entrada "mimetype" los lleva
// SIN salto de linea final (si no, la firma del offset 38 no cierra).
extern const char* const kW3dMimetype;
// el texto de LEEME.txt: fijo y DETERMINISTA (ni fecha, ni usuario, ni rutas:
// cualquier campo volatil rompe el round-trip byte a byte).
std::string W3dContenedorLeeme();
// true si 'n' es una entrada de SERVICIO del contenedor (mimetype, LEEME.txt,
// proyecto.json, EXTERNOS.txt): las genera el guardado, no las referencia nadie
// y no se preservan del archivo viejo (se vuelven a escribir).
bool W3dEsEntradaDeServicio(const std::string& n);

// ---------------------------------------------------------------------------
//  NOMBRES DE ENTRADA
// ---------------------------------------------------------------------------
// slug ASCII minusculas de UN componente (spec seccion 3): translitera, baja a
// minusculas, deja [a-z0-9._-], colapsa '_', saca los bordes, evita los nombres
// reservados de Windows y trunca a 60 bytes. Vacio -> "sin_nombre".
std::string W3dSlugEntrada(const std::string& nombre);
// la carpeta que le toca a una ruta por su EXTENSION ("texturas", "scripts"...).
// Lo que no se reconoce cae en "extra".
const char* W3dCategoriaPorExtension(const std::string& ruta);
// true si 'r' arranca con una de las carpetas reservadas del layout: o sea, si
// parece un NOMBRE DE ENTRADA y no una ruta de disco.
bool W3dEsNombreDeEntrada(const std::string& r);

// TEST del harness (comando 'contenedor'): hace que el escritor SE OLVIDE de la
// primera entrada que ingiere, o sea que el JSON la nombra y el zip no la tiene.
// Es la unica forma de probar la VERIFICACION previa al cierre, que es la
// mitigacion del fallo mas caro del diseno (el proyecto abriria "sin la textura"
// y no fallaria nada). Siempre false en el editor real.
extern bool g_w3dContOlvidarUna;

// ---------------------------------------------------------------------------
//  ESCRITOR del contenedor (solo lo usa el guardado)
//
//  DOS FASES a proposito:
//    1) el guardado camina la escena, arma el JSON y va INGIRIENDO cada asset:
//       cada uno queda anotado con su ORIGEN (bytes en memoria, un archivo de
//       disco, o una entrada del contenedor VIEJO que se reenvasa tal cual).
//       No se levantan los bytes de todos: pico de RAM = la entrada mas grande.
//    2) Escribir() vuelca el zip en orden DETERMINISTA (mimetype primero,
//       proyecto.json segundo, el resto alfabetico) a un temporal, y el caller
//       hace el rename.
// ---------------------------------------------------------------------------
struct W3dEntradaPend {
    std::string nombre;    // "texturas/pausa.png"
    int         origen;    // 0 = bytes, 1 = archivo de disco, 2 = entrada del zip viejo
    std::vector<unsigned char> bytes;   // origen 0
    std::string fuente;    // origen 1: la ruta de disco. origen 2: el nombre en el zip viejo
    bool        temporal;  // origen 1: borrar 'fuente' al terminar el guardado
    unsigned    crc, tam;  // huella del contenido (dedup)
    W3dEntradaPend() : origen(0), temporal(false), crc(0), tam(0) {}
};

// una referencia que quedo AFUERA del .w3d (va a EXTERNOS.txt y a la notificacion)
struct W3dExterno {
    std::string ref;     // "ext:/ruta/Texturas/piso.png"
    std::string quien;   // "material \"Piso\"" / "escena \"Menu\""
    bool existe;
};

class W3dContenedorEscritor {
public:
    W3dContenedorEscritor();
    ~W3dContenedorEscritor();

    // 'rutaFinal' = el .w3d destino (de ahi salen los temporales y la base de las
    // rutas relativas de los externos). 'viejo' = el contenedor montado (o NULL).
    void Iniciar(const std::string& rutaFinal, W3dZipLector* viejo);

    // EL CORAZON: mete un asset adentro y devuelve lo que hay que ESCRIBIR en el
    // archivo (su nombre de entrada, o "ext:..." si queda afuera). 'ruta' se
    // reescribe con lo que queda EN MEMORIA (el nombre de entrada para los
    // internos; la ruta de disco tal cual para los externos).
    std::string Ingerir(std::string& ruta, const char* categoria, const std::string& quien);

    // CORRIGE el "existe" de una referencia externa ya registrada.
    // Ingerir decide si una referencia existe preguntandole al disco por ESE nombre,
    // y eso es correcto para un archivo... pero no para una referencia que es un
    // PREFIJO. El basePath de una vertex animation nombra una SECUENCIA
    // ("modelos/heroe/agachado/agachado" -> agachado001.obj, agachado002.obj, ...):
    // no existe ningun archivo con ese nombre, asi que TODAS las vertex anims se
    // anotaban como "FALTA" y cada guardado terminaba con el cartel ROJO "Guardado,
    // pero N archivo(s) externo(s) NO estan" sin que faltara nada (52 en el proyecto
    // medidos). El que sabe cual es el archivo de verdad (el primer frame) lo
    // comprueba y lo dice por aca.
    void ExternoCorregirExiste(const std::string& ref, bool existe);

    // entradas GENERADAS por el guardado (el JSON, EXTERNOS.txt): bytes en memoria.
    // 'dedup' = false para las entradas de SERVICIO: si "mimetype" entrara al
    // indice por contenido, un asset del usuario con esos mismos 36 bytes se
    // aliasaria a el y el JSON terminaria apuntando a "mimetype".
    bool AgregarBytes(const std::string& nombre, const std::string& texto, bool dedup = true);
    // idem, pero el contenido quedo en un TEMPORAL de disco (.w3dui, .glb, .bin):
    // se anota la ruta y se borra al final del guardado
    bool AgregarTemporal(const std::string& nombre, const std::string& rutaTmp);

    // un temporal unico en la carpeta del destino (para el .w3dui / el .glb)
    std::string RutaTemporal(const char* etiqueta);

    // ¿ya hay una entrada con ese nombre?
    bool Tiene(const std::string& nombre) const;

    // VERIFICACION (spec seccion 9.1, punto 2): toda referencia interna del JSON y
    // de cada .w3dui tiene su entrada escrita. Sin esto el proyecto abre "sin la
    // textura" y no falla nada: es el fallo mas caro del diseno. false + el nombre
    // de la primera que falta.
    bool Verificar(std::string& falta);

    // lo que el contenedor VIEJO traia y el editor no referencia: se preserva
    // VERBATIM (spec seccion 10), salvo lo que cuelgue de mallas/ o animaciones/
    // (carpetas exclusivas del editor: ahi lo no referenciado se descarta).
    void PreservarPasajeras();

    // las dos entradas de servicio del estilo OpenDocument: "mimetype" (que
    // Escribir() manda PRIMERA) y "LEEME.txt". Se llama en cada guardado: las dos
    // se REGENERAN siempre, nunca se preservan del archivo viejo.
    void EscribirCabeceraOdf();

    // EXTERNOS.txt (solo si hay alguna ref "ext:")
    void EscribirExternos();
    size_t CantidadExternos() const { return externos.size(); }
    size_t CantidadExternosQueFaltan() const;

    // vuelca el zip a 'rutaTmpZip' en orden determinista. false = no se escribio
    // (el archivo anterior queda INTACTO porque el caller todavia no renombro).
    bool Escribir(const std::string& rutaTmpZip);

    // borra los temporales del .w3dui/.glb (se llama pase lo que pase)
    void LimpiarTemporales();

private:
    W3dContenedorEscritor(const W3dContenedorEscritor&);
    W3dContenedorEscritor& operator=(const W3dContenedorEscritor&);

    std::string NombreLibre(const char* categoria, const std::string& base);
    void RegistrarExterno(const std::string& ref, const std::string& quien, bool existe);
    // los bytes REALES de una entrada ya anotada (sea cual sea su origen). false =
    // no se pudieron releer.
    bool BytesDeEntrada(size_t idx, std::vector<unsigned char>& out) const;
    // indice de la entrada que tiene EXACTAMENTE estos bytes, o (size_t)-1.
    // Compara BYTE A BYTE: la huella crc:tam solo sirve para no comparar contra
    // todo el mundo (ver el comentario de 'porHuella').
    size_t BuscarPorContenido(const std::string& huella, const std::vector<unsigned char>& datos) const;

    std::string    dirDestino;
    std::string    rutaDestino;
    W3dZipLector*  viejo;
    int            contadorTmp;
    std::vector<W3dEntradaPend>   pend;
    std::map<std::string, size_t> porNombre;    // nombre -> indice en 'pend'
    // "crc:tam" -> TODAS las entradas con esa huella. Es un INDICE, no una prueba:
    // CRC32 tiene 32 bits, o sea que dos archivos DISTINTOS del mismo tamano pueden
    // compartirlo (y construir el par es trivial: el CRC es lineal). Confiar en el
    // hacia que una textura se convirtiera en OTRA al guardar. Por eso el dedup real
    // lo hace BuscarPorContenido comparando byte a byte, igual que W3dImportarAsset.
    std::map<std::string, std::vector<size_t> > porHuella;
    std::map<std::string, std::string> yaResueltas;  // ruta original -> lo que se escribio
    std::vector<W3dExterno>       externos;
    std::set<std::string>         olvidadas;   // solo el hook de test las llena
};

#endif // W3DCONTENEDOR_H
