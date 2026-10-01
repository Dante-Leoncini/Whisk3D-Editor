#ifndef W3DESCENA_H
#define W3DESCENA_H
#include <string>

// ============================================================================
//  W3dEscena — el motor MULTI-ESCENA, COMPARTIDO por el editor (Play, via
//  SimJuego) y el runtime compilado (game/w3drun), para que un juego con varias
//  pantallas corra IGUAL en los dos lados.
//
//  Una ESCENA es un objeto UI raiz (un .w3dui) colgado de SceneCollection, con
//  su propio array de scripts. El .w3d v2 admite VARIOS nodos "ui" -> conviven N
//  escenas en SceneCollection. El nombre de la escena = el campo "nombre" del UI.
//
//  El cambio de escena se pide con cambiarEscena(nombre) desde el lua; se APLICA
//  al FINAL del frame (no en el callback, para no mutar la iteracion de actualizar).
//  Al aplicarse: oculta la activa anterior, muestra la nueva, y hace init PEREZOSO
//  (si la nueva nunca corrio inicio() se le cargan+resuelven+inicio() sus scripts;
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
// recrear los lua_State). Debe cargar+resolver refs+inicio() de los scripts del
// SUBARBOL de la escena, resolviendo las refs contra la raiz de la escena.
typedef void (*W3dEscenaInitFn)(UI* escena, bool reiniciar);
void W3dEscenaSetInit(W3dEscenaInitFn fn);

// (RE)construye el mapa nombre->UI recorriendo SceneCollection->Childrens (tipo ui).
// No corre ningun inicio(); resetea activa/inited/pendiente (NO la escena inicial).
void W3dEscenaRegistrarTodas();
// hay al menos una escena UI registrada?
bool W3dEscenaHayEscenas();
// la escena activa (NULL si no hay multi-escena corriendo)
UI* W3dEscenaActiva();
// busca una escena por nombre (NULL si no existe)
UI* W3dEscenaBuscar(const std::string& nombre);

// nombre de la escena INICIAL (la que arranca). Lo fija el importador del .w3d o el
// runtime; PERSISTE (NO lo borran RegistrarTodas / Limpiar: es un ajuste del proyecto).
void W3dEscenaSetInicial(const std::string& nombre);
const std::string& W3dEscenaInicial();
// modo multi-escena declarado por el proyecto (informativo por ahora)
void W3dEscenaSetModo(bool multi);
bool W3dEscenaModo();

// ACTIVA la escena inicial (o la primera registrada si no hay una fijada): la muestra,
// oculta el resto y corre su init (via el callback). Las demas quedan sin inicir (perezoso).
void W3dEscenaArrancar();

// pide un cambio de escena; se aplica al FINAL del frame (W3dEscenaAplicarPendiente),
// NO en el acto (para no mutar la iteracion de actualizar). reiniciar=true fuerza el
// re-inicio de la escena destino. Un nombre inexistente se ignora (no rompe el juego).
void W3dEscenaPedirCambio(const std::string& nombre, bool reiniciar = false);
// aplica el cambio pendiente (si hay): oculta la activa, muestra la nueva, init perezoso
// de la nueva si nunca corrio. Llamar UNA vez al final de cada frame.
void W3dEscenaAplicarPendiente();

// true si el objeto pertenece a la escena ACTIVA y esta visible (para decidir si sus
// scripts corren). Sin multi-escena (o para objetos fuera de toda escena UI, ej 3D):
// cae a la regla vieja (solo o->visible).
bool W3dEscenaEsDeActiva(Object* o);
// la raiz UI de la que cuelga 'o' (subiendo por Parent); si no cuelga de ninguna UI,
// devuelve SceneCollection (asi la busqueda de refs sigue igual que antes para el 3D).
Object* W3dEscenaRaizDe(Object* o);
// la REFERENCIA 'nombre' de un script de 'o' (las refs de su tarjeta, objeto("x")): adentro de su escena UI si
// es de una; si no, POR SCOPE: un objeto generado por una instancia de prefab ve primero lo de SU instancia
// (W3dBuscarNombreDesde, objects/Objects.h) y despues toda la escena. La usan el Play y el runtime compilado.
Object* W3dEscenaBuscarRef(Object* o, const std::string& nombre);

// limpia TODO (mapa/activa/inited/pendiente); NO borra la escena inicial. Lo llama el STOP.
void W3dEscenaLimpiar();

// ESCENAS 3D (main/W3dRaices.h): cambiarEscena(nombre) que no nombra una escena UI se le pasa
// al modulo de raices. 'pedir' anota el cambio (false = no es una escena 3D) y 'aplicar' lo
// hace al final del frame (true = cambio el arbol entero: el cambio de UI pendiente se descarta).
// Punteros a funcion para que este archivo siga compilando solo en los juegos 2D.
void W3dEscenaSet3D(bool (*pedir)(const std::string& nombre, bool reiniciar), bool (*aplicar)());
// CINEMATICAS (main/W3dRaices.h): reproducirEscena(nombre [, "alTerminar"]) / pararEscena3D() /
// escenaReproduciendo() de lua se le pasan al modulo de raices ('reproducir' recibe al objeto del script
// que la pidio y la funcion a llamar al volver). Sin 3D (juego 2D) los binds existen y no hacen nada.
void W3dEscenaSetCine(bool (*reproducir)(const std::string& nombre, Object* llamador, const std::string& fn),
                      void (*parar)(), const char* (*actual)());

// registra el bind lua cambiarEscena(nombre[,reiniciar]). Lo llama el punto de binds
// COMPARTIDO (BindsJuegoRegistrar), asi el editor y el runtime lo exponen identico.
void W3dEscenaRegistrarBind(void* L);

#endif // W3DESCENA_H
