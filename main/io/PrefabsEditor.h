#ifndef W3D_PREFABS_EDITOR_H
#define W3D_PREFABS_EDITOR_H
// ============================================================================
//  PrefabsEditor — el EDITOR sobre las instancias de prefab (io/Prefabs.h,
//  objects/InstancePrefab.h):
//    - lo GENERADO por una instancia se ve y se elige, pero su estructura no se toca:
//      borrar, duplicar, reparentar, unir (Join), agregarle o quitarle scripts, moverlo
//      (G/R/S) o ponerle keyframes, o meter algo adentro de una instancia esta bloqueado
//      (con aviso): nada de eso se guardaria. Lo que SI se guarda son los OVERRIDES
//      (valores de propiedades de scripts y el visible de cada hijo);
//    - Add > Prefab (submenu con los prefabs del proyecto) y soltar un prefab de la
//      Biblioteca en el 3D crean una instancia (con Ctrl+Z); soltarlo sobre el selector
//      de la tarjeta de una instancia le cambia el prefab (con Ctrl+Z);
//    - "Create Prefab" desde la seleccion: mueve el subarbol a un prefab nuevo y lo
//      reemplaza por una instancia; "Unpack" materializa los hijos como objetos comunes
//      (los dos con Ctrl+Z);
//    - volver de editar un prefab regenera TODAS sus instancias (W3dActivateRoot).
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include <string>
#include "math/Vector3.h"

class Object;
class InstancePrefab;
class PopupMenu;

// ---- lo generado no se toca ----
// 'o' es un objeto GENERADO por una instancia (su estructura no se edita). avisar = notificacion en pantalla
bool W3dPrefabIsGeneratedNotice(Object* o, bool notify);
// se puede colgar 'obj' de 'newParent'? (no se saca nada de lo generado ni se mete nada adentro de una
// instancia). false + aviso en pantalla. Lo preguntan todas las puertas de reparent (ObjectMode.cpp)
bool W3dPrefabCrossingValid(Object* obj, Object* newParent);
// true mientras "Unpack" mueve lo generado afuera (la unica puerta que lo puede sacar)
extern bool g_w3dPrefabUnpacking;

// ---- lo que es de una LIBRERIA externa no se toca (formato/librerias.md) ----
class Mesh;
// 'o' lo genero un PROXY de una libreria (a cualquier profundidad: una instancia anidada adentro tambien)
bool W3dGeneratedByProxy(const Object* o);
// la malla 'm' es de SOLO LECTURA: la genero un proxy o, con 'recurso', su RECURSO es de una libreria (la geometria,
// las partes, los grupos de vertices y de UV, los mapas UV y las capas de color son de la libreria; lo del OBJETO
// -sus modificadores, que malla usa- es del proyecto aunque su malla sea de una libreria)
bool W3dMeshReadOnly(const Mesh* m, bool resource);
// idem, con aviso en pantalla (la puerta de las acciones de Properties). true = bloqueado
bool W3dReadOnlyMeshNotice(const Mesh* m, bool resource);

// ---- crear / cambiar / desempaquetar ----
// una instancia NUEVA de 'nombre' en 'pos' (mundo, colgada de la raiz activa), elegida y con Ctrl+Z. NULL +
// motivo (en ingles, pasa por T()) si no se puede (no existe, o seria un prefab adentro de si mismo)
InstancePrefab* W3dPrefabAdd(const std::string& name, const Vector3& pos, std::string* reason);
// se podria agregar una instancia de 'nombre' ahora? (lo mismo que mira W3dPrefabAdd, sin crear nada)
bool W3dPrefabCanAdd(const std::string& name, std::string* reason);
// el prefab de una instancia pasa a ser 'nuevo' (regenera), con Ctrl+Z. No con el Play andando
bool W3dPrefabChange(InstancePrefab* ip, const std::string& newValue, std::string* reason);
// "Reset Overrides": la instancia vuelve a ser exactamente su prefab (regenera), con Ctrl+Z. No con el Play
// andando ni en una instancia anidada (sus overrides son de la de afuera)
bool W3dPrefabResetOverrides(InstancePrefab* ip, std::string* reason);
//  (las dos REGENERAN sin liberar lo generado: el paso de undo se queda con la generacion que sale y el Ctrl+Z
//   la vuelve a colgar, asi los pasos del historial que nombran objetos generados siguen valiendo)
// "Create Prefab": lo ELEGIDO (sus raices, con todo lo que cuelga) pasa a un prefab NUEVO y en su lugar queda
// una instancia. Un solo objeto elegido es el objeto raiz del prefab (la instancia toma su transform); varios
// quedan colgados de un vacio con el nombre del prefab. Con Ctrl+Z (un solo paso): la instancia se va, lo
// elegido vuelve tal cual a la escena y el prefab sale del registro. nameOut = el nombre del prefab creado
bool W3dPrefabCreateFromSelection(std::string* nameOut, std::string* reason);
// "Unpack": lo que genero la instancia pasa a ser objetos comunes de la escena (en el lugar de la instancia,
// en su scope: los nombres que choquen se renumeran) y la instancia se borra. Con Ctrl+Z (un solo paso)
bool W3dPrefabUnpack(InstancePrefab* ip, std::string* reason);
// el prefab de la fila 'idx' se DEJO de editar (el editor cambio de raiz): su definicion cambio y TODAS sus
// instancias (de todas las raices cargadas) se regeneran. Avisa si quedaron objetos SUELTOS al lado de su
// objeto raiz (sus instancias no los generan)
void W3dPrefabStoppedEditing(const std::string& name);
// Add editando un prefab: el objeto recien creado (suelto en el primer nivel de la raiz del prefab) pasa ADENTRO
// de su objeto raiz, en el mismo lugar del mundo. Fuera de un prefab no hace nada
void W3dPrefabAdoptNew(Object* newValue);

// ---- los PROXIES W3D (objects/ProxyW3d.h): un prefab o una escena de una LIBRERIA externa ----
class ProxyW3d;
// se podria agregar un proxy de ese elemento ahora? ('libreria' vacia = un proxy vacio, se elige despues)
bool W3dProxyCanAdd(const std::string& libName, int type, const std::string& element, std::string* reason);
// un PROXY nuevo en 'pos' (mundo; editando un prefab va adentro de su objeto raiz), elegido y con Ctrl+Z. NULL +
// motivo (en ingles, pasa por T()) si no se puede
ProxyW3d* W3dProxyAdd(const std::string& lib, int type, const std::string& element, const Vector3& pos,
                          std::string* reason);
// el proxy pasa a generar otro elemento (u otra libreria), con Ctrl+Z (regenera sin liberar, como cambiar el
// prefab de una instancia; los overrides del anterior no aplican). No con el Play andando
bool W3dProxyChange(ProxyW3d* px, const std::string& lib, int type, const std::string& element, std::string* reason);

// el REGISTRO DE LIBRERIAS cambio para 'lib' (se vinculo, se desvinculo, cambio su archivo, o un Ctrl+Z lo devolvio):
// lo que depende de ella (sus proxies, y lo que tiene adentro uno suyo) se regenera: al desvincularla queda vacio
// (igual que al reabrir el nivel sin ella) y al vincularla vuelve. En la raiz ACTIVA, con 'activeWithUndo', un paso de
// undo por cada uno (sin liberar: el que llama los agrupa con su paso del registro); en las demas raices cargadas,
// liberando. Devuelve cuantos regenero. No con el Play andando (el que llama no lo llama)
int W3dProxiesLibChange(const std::string& lib, bool activeWithUndo);
// las REFERENCIAS DEL PROYECTO que quedaron pendientes porque 'lib' no estaba (io/Libraries.h: la malla de un objeto, el
// animset de un esqueleto) se resuelven ahora que esta vinculada (sin undo: es el estado que el archivo ya nombraba). Lo
// llama W3dProxiesLibChange. Devuelve cuantas resolvio
int W3dLibRefsResolve(const std::string& lib);

// ---- COMO SE CARGA una instancia o un proxy (io/Streaming.h), con Ctrl+Z ----
// la instancia YA tiene lo nuevo (la tarjeta lo edito en el lugar): empuja el paso que vuelve a 'carga'/'distancia'/
// 'objetivo' (lo de antes). Nada si no cambio nada
void W3dInstanceLoadUndo(InstancePrefab* ip, int load, float distance, const std::string& target);
// lo cambia y empuja el paso (el harness, y cualquiera que no lo edite en el lugar)
void W3dInstanceLoadChange(InstancePrefab* ip, int load, float distance, const std::string& target);
// sube con cada paso anotado y con cada Ctrl+Z / Ctrl+Y de uno: la tarjeta que edita en el lugar vuelve a mirar
extern unsigned g_w3dLoadVersion;

// ---- Add > Prefab ----
// arma el submenu con los prefabs del proyecto (sin el que se esta editando ni los que lo contienen)
void W3dPrefabMenuAddBuild(PopupMenu* m);
void W3dPrefabMenuAddAction(int id);   // crea la instancia en el cursor 3D
// "Add > Scene": las otras escenas del proyecto (menos la que se edita y las que la contienen); la instancia va al
// cursor 3D (una InstancePrefab de "escena:<nombre>", io/Prefabs.h)
void W3dSceneMenuAddBuild(PopupMenu* m);
void W3dSceneMenuAddAction(int id);
// el prefab que el menu arma en la fila 'id' ("" = ninguno): el harness
std::string W3dPrefabMenuAddName(int id);

#endif
