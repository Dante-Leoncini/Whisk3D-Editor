#ifndef W3D_RAICES_EDITOR_H
#define W3D_RAICES_EDITOR_H
// ============================================================================
//  RaicesEditor — la UI del EDITOR sobre las raices del proyecto (escenas 3D,
//  juegos y prefabs, ver W3dRaices.h):
//    - W3dActivarRaiz: cambiar la escena/juego/prefab que se edita, con sus
//      precondiciones (sin Play ni un transform en curso), saliendo antes de
//      Edit/Pose/local view y con el undo limpio (el historial es de la raiz que se deja);
//    - el SELECTOR (el mismo menu en la barra del viewport 3D y en la del outliner):
//      escenas (icono camara), juegos (control de juego), prefabs, "New Scene", "New Game",
//      "New Prefab", "Set as Start Scene" y "Convert to Game" (DUPLICA la escena como un juego
//      nuevo: la foto del cuadro actual, sin sus animaciones de escena) / "Convert to Scene" (el
//      juego pasa a ser escena en el lugar); y el "+" desplegable de la barra del outliner (solo
//      los tres "New ...");
//    - el ESTADO BASE de un JUEGO: sus animaciones de escena son CLIPS que se editan
//      eligiendolos en el timeline; al volver a "Juego" todo vuelve a su frame 1 (la foto
//      que se tomo al salir de "Juego", ver W3dJuegoBase*);
//    - los PROVEEDORES de las vistas "Scenes" (escenas y juegos) y "Prefabs" del outliner
//      por recursos (carpetas, renombrar con undo, borrar). Los usuarios de un prefab son sus
//      INSTANCIAS (io/Prefabs.h, en las raices cargadas); "Purge Orphans" igual no los toca.
//  Solo editor. Motor generico: aca no hay nombres de ningun juego.
// ============================================================================
#include <string>

class PopupMenu;
class Button;
class Object;

// cambia la raiz ACTIVA del editor. false + motivo (en ingles; la UI lo pasa por T()) si no se
// puede ahora. Carga la raiz si todavia no se habia abierto.
bool W3dActivarRaiz(int idx, std::string* motivo);
// LAS ESCENAS ABIERTAS A LA VEZ: la casilla de la lista. Tildar abre (la carga si hace falta: se ve en el viewport y en
// el outliner); destildar la cierra y la saca de la memoria (con cambios sin guardar, primero el cartel: guardar y
// cerrar / cerrar sin guardar / cancelar). La activa se cierra pasando la activa a otra; la ultima abierta no se cierra
bool W3dRaizTildar(int idx, bool abrir, std::string* motivo);
// la escena abierta de esa RAIZ (el objeto Scene de arriba) pasa a ser la activa: un click en el outliner o en el
// viewport sobre algo de otra escena abierta. false = no se pudo (ya quedo avisado)
bool W3dActivarRaizDe(Object* raiz);
// "New Scene" / "New Game" / "New Prefab": crea una raiz vacia (un prefab nace con su objeto raiz)
// con nombre libre y la ABRE. Devuelve su indice (-1 = no se pudo abrir: la fila queda creada igual).
int  W3dRaizCrearYAbrir(int tipo, const std::string& nombre);
// cambia el TIPO de la raiz ACTIVA en el lugar (con undo): es el "Convert to Scene" de un juego (y el comando
// 'raiztipo' del harness, en los dos sentidos). false + motivo si no se puede (con el juego andando, o un prefab).
bool W3dRaizConvertirActiva(int tipo, std::string* motivo);
// "Convert to Game" de una ESCENA: la DUPLICA en un JUEGO NUEVO con lo que se ve en el CUADRO ACTUAL como su estado
// inicial (cada objeto con la posicion, rotacion, escala, visibilidad, lente, colores, pesos de sus formas y clip de
// esqueleto que la animacion le puso en ese cuadro; la camara activa de ese cuadro) y SIN las animaciones de la
// escena (sus keyframes, cortes de camara, tiras de esqueleto y capas del Mix). Lo que es de los objetos y el juego
// usa se copia como en cualquier duplicado: scripts (con sus valores y su ojo), mallas y materiales (los MISMOS
// recursos), clips de esqueleto y de jerarquia y vertex anims (compartidos), fisica, hitbox, constraints, instancias
// de prefab... El juego se llama como la escena con el sufijo de una copia ("Intro" -> "Intro.001") y pasa a ser la
// raiz activa, como con "New Game"; la escena queda intacta. Con undo: Ctrl+Z vuelve a la escena y saca el juego,
// Ctrl+Y lo devuelve. Devuelve el indice del juego (-1 + motivo, en ingles, si no se puede: no es una escena, el
// juego anda, un transform en curso).
int  W3dRaizConvertirEnJuego(std::string* motivo);

// ---- EL SELECTOR (barra del viewport 3D y del outliner) ----
// arma el menu (escenas, juegos, prefabs y las acciones). Lo elegido se aplica con W3dRaicesMenuAccion.
void W3dRaicesMenuArmar(PopupMenu* m);
// el menu del "+" de la barra del outliner (un solo boton desplegable, como el Add del 3D): "New Scene",
// "New Game" y "New Prefab" con el icono de lo que crean. Tambien se aplica con W3dRaicesMenuAccion.
void W3dRaicesMenuArmarNuevas(PopupMenu* m);
void W3dRaicesMenuAccion(int id);
enum { W3D_RAIZ_MENU_NUEVA_ESCENA = 3000, W3D_RAIZ_MENU_NUEVO_PREFAB, W3D_RAIZ_MENU_INICIAL,
       W3D_RAIZ_MENU_NUEVO_JUEGO, W3D_RAIZ_MENU_A_JUEGO, W3D_RAIZ_MENU_A_ESCENA };
// el icono de un tipo de raiz (escena / juego / prefab)
int  W3dRaizIcono(int tipo);
// el boton del selector: icono (escena/juego/prefab) + nombre de la raiz activa. true si cambio algo.
bool W3dRaicesBotonSincronizar(Button* b);

// ---- EL ESTADO BASE DE UN JUEGO (su "frame 1") ----
// La foto del estado base de la raiz activa: la toma el timeline al pasar de "Juego" a uno de sus
// clips (una animacion de escena, un clip...). Reponer la devuelve (a los objetos que los clips de
// la raiz pueden posar, y la camara activa) y la descarta; es lo que hace volver a "Juego".
void W3dJuegoBaseGuardar();
bool W3dJuegoBaseReponer();
bool W3dJuegoBaseHay();          // hay una foto de la raiz activa (un juego editando un clip)
void W3dJuegoBaseOlvidar();
// el guardado escribe el ESTADO BASE (no la pose del clip): si 'o' esta en la foto, se le pone su
// base (lo llama el reposo del guardado, que despues le devuelve lo que tenia)
void W3dJuegoBaseAplicarA(Object* o);

#endif
