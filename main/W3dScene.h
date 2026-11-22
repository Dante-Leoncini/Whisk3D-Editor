#ifndef W3D_SCENE_H
#define W3D_SCENE_H
#include <string>
#include <vector>

// ============================================================================
//  W3dEscena — el motor MULTI-ESCENA, COMPARTIDO por el editor (Play, via
//  SimJuego) y el runtime compilado (game/w3drun), para que un juego con varias
//  pantallas corra IGUAL en los dos lados.
//
//  Una ESCENA es un objeto UI raiz (un .w3dui) colgado de SceneCollection, con
//  su propio array de scripts. El .w3d v2 admite VARIOS nodos "ui" -> conviven N
//  escenas en SceneCollection. El nombre de la escena = el campo "nombre" del UI.
//
//  El cambio de escena se pide con changeScene(nombre) desde el lua; se APLICA
//  al FINAL del frame (no en el callback, para no mutar la iteracion de actualizar).
//  Al aplicarse: oculta la activa anterior, muestra la nueva, y hace init PEREZOSO
//  (si la nueva nunca corrio start() se le cargan+resuelven+start() sus scripts;
//  si ya corrio, se reanuda sin re-inicio: su estado lua se preserva).
//
//  Retrocompat: una sola .w3dui = una unica escena (arranca activa). Sin ninguna
//  UI (juego 3D puro) el sistema queda en PASS-THROUGH (no cambia el comportamiento
//  viejo: los scripts corren segun visible, como siempre).
//
//  C++03 puro (mismo codigo para Android/Symbian). Comentarios sin acentos.
// ============================================================================

class Object;
class UI;

// callback de INIT de UNA escena: cada build lo registra (el editor via SimJuego,
// el runtime via w3drun). Recibe la raiz UI y si hay que REINICIAR (descargar y
// recrear los lua_State). Debe cargar+resolver refs+start() de los scripts del
// SUBARBOL de la escena, resolviendo las refs contra la raiz de la escena.
typedef void (*W3dSceneInitFn)(UI* scene, bool restart);
void W3dSceneSetInit(W3dSceneInitFn fn);

// (RE)construye el mapa name->UI recorriendo SceneCollection->Childrens (tipo ui).
// No corre ningun start(); resetea activa/inited/pendiente (NO la escena inicial).
void W3dSceneRegisterAll();
// hay al menos una escena UI registrada?
bool W3dSceneHasScenes();
// la escena activa (NULL si no hay multi-escena corriendo)
UI* W3dActiveScene();
// busca una escena por nombre (NULL si no existe)
UI* W3dSceneFind(const std::string& name);

// nombre de la escena INICIAL (la que arranca). Lo fija el importador del .w3d o el
// runtime; PERSISTE (NO lo borran RegistrarTodas / Limpiar: es un ajuste del proyecto).
void W3dSceneSetInitial(const std::string& name);
const std::string& W3dInitialScene();
// modo multi-escena declarado por el proyecto (informativo por ahora)
void W3dSceneSetMode(bool multi);
bool W3dSceneMode();

// ACTIVA la escena inicial (o la primera registrada si no hay una fijada): la muestra,
// oculta el resto y corre su init (via el callback). Las demas quedan sin inicir (perezoso).
void W3dSceneStart();

// pide un cambio de escena; se aplica al FINAL del frame (W3dSceneApplyPending),
// NO en el acto (para no mutar la iteracion de actualizar). reiniciar=true fuerza el
// re-inicio de la escena destino. Un nombre inexistente se ignora (no rompe el juego).
void W3dSceneRequestChange(const std::string& name, bool restart = false);
// aplica el cambio pendiente (si hay): oculta la activa, muestra la nueva, init perezoso
// de la nueva si nunca corrio. Llamar UNA vez al final de cada frame.
void W3dSceneApplyPending();

// true si el objeto pertenece a la escena ACTIVA y esta visible (para decidir si sus
// scripts corren). Sin multi-escena (o para objetos fuera de toda escena UI, ej 3D):
// cae a la regla vieja (solo o->visible).
bool W3dSceneIsFromActive(Object* o);
// la raiz UI de la que cuelga 'o' (subiendo por Parent); si no cuelga de ninguna UI,
// devuelve SceneCollection (asi la busqueda de refs sigue igual que antes para el 3D).
Object* W3dSceneRootOf(Object* o);
// la REFERENCIA 'nombre' de un script de 'o' (las refs de su tarjeta, object("x")): adentro de su escena UI si
// es de una; si no, POR SCOPE: un objeto generado por una instancia de prefab ve primero lo de SU instancia
// (W3dFindNameFrom, objects/Objects.h) y despues toda la escena. La usan el Play y el runtime compilado.
Object* W3dSceneFindRef(Object* o, const std::string& name);

// limpia TODO (mapa/activa/inited/pendiente); NO borra la escena inicial. Lo llama el STOP.
void W3dSceneClear();

// DIBUJAR EL 3D: lua draw3D(false) apaga el pase 3D entero (W3dScene3DPass no recorre el arbol)
// mientras la UI sigue dibujandose y los scripts siguen corriendo. Para las pantallas que tapan todo
// (un menu con fondo opaco): no se gasta nada en una escena que no se ve. El STOP lo vuelve a prender.
bool W3dScene3DActive();
void W3dScene3DSetActive(bool on);

// ESCENAS 3D (main/W3dRoots.h): changeScene(nombre) que no nombra una escena UI se le pasa
// al modulo de raices. 'pedir' anota el cambio (false = no es una escena 3D) y 'aplicar' lo
// hace al final del frame (true = cambio el arbol entero: el cambio de UI pendiente se descarta).
// Punteros a funcion para que este archivo siga compilando solo en los juegos 2D.
void W3dSceneSet3D(bool (*request)(const std::string& name, bool restart), bool (*apply)());
// CINEMATICAS (main/W3dRoots.h): playScene(nombre [, "alTerminar"]) / stopScene3D() /
// scenePlaying() de lua se le pasan al modulo de raices ('reproducir' recibe al objeto del script
// que la pidio y la funcion a llamar al volver). Sin 3D (juego 2D) los binds existen y no hacen nada.
void W3dSceneSetCinematic(bool (*play)(const std::string& name, Object* caller, const std::string& fn),
                      void (*stop)(), const char* (*current)());

// ============================================================================
//  LAS CAPAS: escenas cargadas ENCIMA de la que corre (la "base"), a la vez. Por ejemplo un menu que queda
//  cargado todo el juego (salir, guardar, cambiar de mapa) encima de cada nivel.
//    loadScene("Menu")       la carga encima (no reemplaza nada). Una escena 2D (una UI) se dibuja arriba de
//                               las demas, en el orden en que se cargaron; una escena o un juego 3D del proyecto
//                               se suma al mundo (una instancia de "escena:<nombre>", como instantiate()).
//    unloadScene("Menu")    la saca (sus scripts se descargan; una 3D se destruye)
//    changeScene("Nivel 2")   reemplaza la BASE, como siempre; las capas siguen cargadas
//    showScene / hideScene(nombre)   la capa se ve o no (oculta: tampoco corre, como un objeto oculto)
//    pauseScene / resumeScene(nombre)   sus scripts no corren (se sigue viendo). Con el nombre de la BASE
//                               se pausa el mundo entero: sus scripts, la fisica, los hitbox y las animaciones
//                               (un menu de pausa); las capas siguen
//    sceneLoaded(nombre)      true si es la base o una capa
//  Todo se aplica al FINAL del frame (W3dSceneApplyPending), como changeScene.
//
//  LOS MENSAJES entre escenas: send("Nivel 1", "guardar", datos) llama, al final del frame, a
//  onReceive(mensaje, datos, de) en cada script de esa escena ("de" = la escena del que lo mando). Sin escena
//  (nil o "*") le llega a todas. 'datos' se COPIA (nil, booleanos, numeros, textos, objetos y tablas de eso).
// ============================================================================
// cuantas capas hay, la raiz de cada una (una UI, o la instancia de una escena 3D) y si corre ahora
int     W3dSceneLayers();
Object* W3dSceneLayerRoot(int i);
bool    W3dSceneLayerEs3D(int i);
bool    W3dSceneIsLayerRoot(Object* o);
bool    W3dSceneInLayer(Object* o);           // 'o' esta adentro de una capa (o es su raiz)
// la base esta en pausa (pauseScene con su nombre): el mundo no avanza (scripts, fisica, hitbox, animaciones)
bool    W3dSceneBasePaused();
// la escena de 'o': la de su capa, la de su UI o la base
std::string W3dSceneOf(Object* o);
// changeScene 3D: las capas sobreviven al cambio de base. 'Desprender' las descuelga de su padre antes de que la
// base se vaya ('withUI': tambien las escenas 2D; el editor deja las suyas donde estan) y 'Colgar' las cuelga de la
// raiz nueva, arriba de todo
void    W3dSceneLayersDetach(bool withUI, std::vector<Object*>& loose);
void    W3dSceneLayersHang(Object* root, const std::vector<Object*>& loose);
// alguna capa vive adentro de este subarbol (el editor no descarga esa raiz al cambiar de escena)
bool    W3dSceneLayerInsideOf(Object* root);
// el loadScene(nombre) de lua, desde C++ (el Play del editor carga asi las otras escenas abiertas encima de la base)
void    W3dSceneRequestLoad(const std::string& name);
// fin de la partida (el Stop del editor): sin capas, sin pausa, sin mensajes; las escenas 2D cargadas encima
// vuelven a verse como las dejo el usuario
void    W3dSceneLayersClear();
// lo 3D que no conoce este modulo (compila tambien en los juegos 2D): crear una capa 3D (NULL = no es una escena 3D
// del proyecto) y el nombre de la base 3D ("" = no hay: la base es la escena 2D activa)
void    W3dSceneSetLayers3D(Object* (*create)(const std::string& name), std::string (*baseName)());

// registra el bind lua changeScene(nombre[,reiniciar]). Lo llama el punto de binds
// COMPARTIDO (BindsGameRegister), asi el editor y el runtime lo exponen identico.
void W3dSceneRegisterBind(void* L);

#endif // W3D_SCENE_H
