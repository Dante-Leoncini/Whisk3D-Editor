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
//      "New Prefab", "Set as Start Scene" y "Convert to Game" / "Convert to Scene"; y el
//      "+" desplegable de la barra del outliner (solo los tres "New ...");
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
// "New Scene" / "New Game" / "New Prefab": crea una raiz vacia (un prefab nace con su objeto raiz)
// con nombre libre y la ABRE. Devuelve su indice (-1 = no se pudo abrir: la fila queda creada igual).
int  W3dRaizCrearYAbrir(int tipo, const std::string& nombre);
// "Convert to Game" / "Convert to Scene" de la raiz ACTIVA (con undo). false + motivo si no se puede
// (con el juego andando, o un prefab).
bool W3dRaizConvertirActiva(int tipo, std::string* motivo);

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
