#ifndef UI2D_FORMATO_H
#define UI2D_FORMATO_H
#include <string>

class UI;
class Object;

// ============================================================================
//  UI2DFormato — GUARDA y CARGA una interfaz 2D (.w3dui): un archivo de TEXTO
//  en JSON con el arbol entero (ventana + elementos + hijos). La idea: disenar
//  los menus/HUD/layout en el Editor 2D en minutos, guardar, y que ese MISMO
//  archivo sea el que compila el juego/programa.
//
//  RUTAS: las texturas y fuentes que viven al lado del archivo (o mas adentro)
//  se guardan RELATIVAS a el (el proyecto se puede mover de carpeta o de
//  maquina); las de afuera quedan absolutas. Al cargar se resuelven contra la
//  carpeta del archivo.
//
//  El formato es versionado ("version": 1). Mas adelante: animaciones y demas.
// ============================================================================

// escribe el arbol del UI en 'ruta' (JSON legible). false si no pudo escribir.
bool UI2DGuardar(UI* u, const std::string& ruta);

// variante para Compilar juego: escribe el .w3dui en 'ruta' PERO calcula las rutas
// relativas (texturas/scripts/fuentes) contra 'baseRel' (la carpeta del PROYECTO), no
// contra la del archivo. Asi el .w3dui puede ir a platform-build/<plat> conservando refs
// relativas planas ("parlante.png", "menu.lua") mientras esos assets se copian planos a su lado.
bool UI2DGuardar(UI* u, const std::string& ruta, const std::string& baseRel);

// lee un .w3dui y arma el arbol (el UI queda colgado de la escena, sin
// seleccionar). NULL si el archivo no se pudo leer o no parsea.
UI* UI2DCargar(const std::string& ruta);

// ---------------------------------------------------------------------------
//  LA regla de "esta ruta cuelga de esta carpeta?" (una sola, para todo el arbol).
//  true si 'ruta' esta adentro de 'dir'; deja en 'rel' el resto SIN barra inicial.
//  - el corte tiene que caer EN un separador (sino "/proy-x/a.png" se hacia relativo
//    contra la base "/proy" y quedaba "-x/a.png": asset perdido);
//  - absorbe separadores REPETIDOS ("/p" + "/p//a.png" -> "a.png"; devolver "/a.png"
//    lo hacia releerse como ruta ABSOLUTA);
//  - cubre la base que ya termina en separador (una raiz: "/" o "C:/").
//  Vive en UI2DFormato.cpp (que lo compilan editor Y runtime) justamente para que no
//  haya dos copias: GuardarW3D.cpp tenia la suya, incompleta, y de ahi salio un bug de
//  perdida de assets.
// ---------------------------------------------------------------------------
bool W3dRutaBajoCarpeta(const std::string& ruta, const std::string& dir, std::string& rel);

// ---------------------------------------------------------------------------
//  FORMATO v4: el .w3d es un CONTENEDOR (zip) y las escenas van ADENTRO.
//
//  Este .cpp lo compila TAMBIEN el runtime de los juegos (whiskpaddle, calculadora,
//  consola, Android, Web, Symbian): es el que lee y escribe los .w3dui. El editor,
//  en cambio, es el unico que sabe que existe un contenedor, y su codigo no puede
//  entrar al runtime (arrastraria medio editor). Por eso la comunicacion va por
//  estos cuatro simbolos, que el editor instala y el runtime deja en su valor
//  neutro para siempre (ahi las rutas se resuelven exactamente como antes).
//
//  SINTAXIS DE LAS REFERENCIAS (v4):
//     "escenas/menu.w3dui"        INTERNA: es un nombre de ENTRADA del zip.
//                                 w3dFileSystem la resuelve por el montaje.
//     "ext:contenido/menu.lua"    EXTERNA relativa a la carpeta del .w3d
//     "ext:/ruta/x.png"     EXTERNA absoluta
//  ':' esta prohibido en los nombres de entrada, asi que el prefijo nunca
//  puede colisionar con una entrada legitima.
// ---------------------------------------------------------------------------

// AL CARGAR: true = las rutas del archivo son ENTRADAS del contenedor y se
// devuelven TAL CUAL (no se cuelgan de la carpeta del .w3dui).
extern bool g_w3dRefsEntradas;

// la carpeta del .w3d abierto: la base de las rutas "ext:" RELATIVAS (que son
// relativas al PROYECTO, no al .w3dui). Vacia = se usa la base del archivo.
extern std::string g_w3dDirProyecto;

// AL GUARDAR en v4: mete el asset adentro del contenedor y devuelve lo que hay
// que ESCRIBIR ("texturas/pausa.png" o "ext:..."). Reescribe 'ruta' con lo que
// queda EN MEMORIA. NULL = guardado de siempre (rutas relativas al archivo).
typedef std::string (*W3dRefEmitFn)(std::string& ruta);
extern W3dRefEmitFn g_w3dRefEmit;

// AL CARGAR: avisa que esta ref venia con "ext:" en el archivo, para que el
// proximo guardado la vuelva a escribir como externa (y no se la trague el
// contenedor sin que el usuario lo haya pedido). NULL = no hacer nada.
typedef void (*W3dRefExtFn)(const std::string& rutaResuelta);
extern W3dRefExtFn g_w3dRefExtMarcar;

#endif // UI2D_FORMATO_H
