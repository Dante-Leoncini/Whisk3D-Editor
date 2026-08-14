#ifndef BINDSJUEGO_H
#define BINDSJUEGO_H
#include <vector>

// ============================================================================
//  BindsJuego — el set COMPARTIDO de binds 2D de JUEGO que ven los scripts lua,
//  IGUAL en el editor (Play, via ScriptUI/SimJuego) que en el runtime compilado
//  (game/w3drun). Unifica lo que antes vivia duplicado/divergente en los dos
//  lados, para que un juego corra identico en Play y compilado.
//
//  Binds registrados (coordenadas en PIXELES del lienzo, (0,0) en el CENTRO):
//    pantalla()             -> ancho, alto del lienzo
//    posPx(o)               -> x, y            setPosPx(o, x, y)  (eje en nil = no se toca)
//    tamPx(o)               -> ancho, alto     setTamPx(o, w, h)
//    setTexto(o, v)         setTextura(o, "ruta.png")
//    mostrar(o, visible)    setOpacidad(o, 0..1)   setTam(o, px)
//    salir()                cierra el juego (la plataforma corta el loop)
//    apretado(o) -> bool    (flanco de tap/click nuevo DENTRO del rect)
//    chocan(a, b) -> bool    (AABB de las cajas EN PANTALLA; regla crudo-vs-
//                             resuelto: gameplay libre = pos cruda sin lag,
//                             puesto por el layout = rect resuelto dibujado)
//    limitar(o, area)  /  limitar(o, arriba, abajo)     escala() -> min(w,h)/480
//    dentro(area, x, y) -> bool   (el punto cae en la caja del area; eje nil = no se mira)
//    areaSegura() -> x, y, ancho, alto   (rect sin notch/barras, esquina sup-izq)
//    fundir(o, objetivo, paso)    (crossfade de opacidad)
//    cajaUI(o) -> x, y, ancho, alto   (rect YA resuelto por el layout, centro)
//
//  LAYOUT: el .w3dui decide DONDE va cada cosa; el script no calcula posiciones,
//  bordes ni centros. Por eso los binds que necesitan una zona toman el OBJETO
//  que la representa (limitar(pala, cancha), dentro(cancha, x, y), y del lado del
//  Core rebotarEn(pelota, cancha, "arriba abajo")) y leen su caja ya resuelta.
//
//  El estado por-frame (punteros del dedo/mouse, mapeo ventana<->lienzo, rect
//  seguro, posiciones resueltas por el layout) vive ACA y lo ALIMENTAN los dos
//  builds: el runtime desde W3dGameRender/Toque/Actualizar, y el editor desde
//  SimJuego (input del Play) + Editor2D (mapeo y posiciones resueltas).
//
//  C++03 puro (mismo codigo compila para Symbian/Android). Comentarios sin acentos.
// ============================================================================

#include "render/UIOverlay.h"   // UI2DPos, tipo COMPLETO: el STLport de RVCT no acepta vector<incompleto>
class Object;

// emitir(obj, n): rafaga de n particulas del objeto Particulas (el polvo de las
// pisadas, un estallido puntual). HOOK como el de setSector: este .cpp lo
// compila tambien el RUNTIME del juego, que no linkea el objeto Particulas del
// editor; el editor lo instala (main/objects/Particulas.cpp) y sin instalar el
// bind es un no-op que linkea igual.
extern void (*W3dParticulasEmitirHook)(Object* o, int n);

// ---------------------------------------------------------------------------
//  LA CAMARA, POR HOOK (mismo patron que los dos de arriba)
//
//  pantallaDe() y la familia del riel (setRiel / setRielNodo / setMiradaRiel /
//  rielDe) estaban bajo `#ifdef W3D_GAME_RUNTIME` devolviendo 0,0,false / false:
//  en un juego COMPILADO no hacian nada, en silencio. Eso es una decision de
//  ALCANCE tomada a partir de como se juega un proyecto concreto -- "los juegos
//  2D compilados no tienen camara 3D" -- y contradice el contrato que declara el
//  encabezado de este archivo: que un juego corra IDENTICO en Play y compilado.
//
//  Con hooks, el bind funciona en cualquier build que linkee el objeto Camera
//  (que es quien los instala, en main/objects/Camera.cpp) y sigue siendo un
//  no-op seguro donde no haya camara.
//
//  proyectar: mundo -> px del lienzo (centro 0,0), + si el objeto esta DELANTE.
//  riel*: 'cam' y 'curva' llegan como Object*; el que instala verifica el tipo.
//         En SetRiel, `curvaNombre` permite pedir el riel por nombre y `soltar`
//         distingue "no hay riel" (nil) de "no lo encontre" (error).
extern bool (*W3dCamaraProyectarHook)(Object* obj, float lienzoW, float lienzoH,
                                      float* sx, float* sy, bool* delante);
extern bool (*W3dCamaraRielSetHook)(Object* cam, Object* curva, const char* curvaNombre,
                                    bool soltar, const int* offset);
extern bool (*W3dCamaraRielNodoHook)(Object* cam, float nodo);
extern bool (*W3dCamaraRielMiradaHook)(Object* cam, bool on);
// `indice` (opcional, puede ser NULL) devuelve el INDICE FRACCIONARIO del riel
// donde la camara quedo parada este tick (rielIndice: seguimiento o nodo manual,
// ya con offsetRiel restado). Es el ancla que una VisZona modo curva espera en
// setVisCurvaT(zona, t); -1 = la camara todavia no se paro en ningun nodo.
extern bool (*W3dCamaraRielEstadoHook)(Object* cam, const char** nombre, int* offset,
                                       float* nodo, bool* mirada, float* indice);
// LENTE: fov (grados, vertical) + los dos planos de recorte. Los punteros nulos en
// SetHook = "no tocar ese". Sirve para ajustar el alcance del dibujado en juego --
// pasar de un area abierta a un pasillo cerrado sin que asome geometria lejana.
extern bool (*W3dCamaraLenteHook)(Object* cam, float* fov, float* cerca, float* lejos);
extern bool (*W3dCamaraSetLenteHook)(Object* cam, const float* fov, const float* cerca,
                                     const float* lejos);
// camaraXZ(): el ADELANTE y la DERECHA de la camara activa proyectados al piso
// (mover al personaje "relativo a como estas mirando"). Mismo patron de hook: lo
// instala Camera.cpp. false = no hay camara -> el bind devuelve el default
// (0,-1, 1,0), NO nil: un juego que lo llama sin guarda no se puede caer.
extern bool (*W3dCamaraXZHook)(float* fx, float* fz, float* rx, float* rz);
// objetivo(): el target del objeto DUENO del script (el personaje que maneja un
// objeto Script). Lo instala quien linkee ObjetoScript (main/objects/Gamepad.cpp).
extern Object* (*W3dObjetivoHook)(Object* duenio);

// registra los binds de juego en un lua_State. Firma compatible con
// W3dScriptSetBindExtra (void(*)(void*)); el editor la usa DENTRO de su propio
// registrador (que ademas agrega camaraXZ/objetivo/parametro), el runtime la
// pasa directo.
void BindsJuegoRegistrar(void* L);

// --- ALIMENTACION del estado por-frame (la llaman los dos builds) -----------
// mapeo del ULTIMO render (ventana -> lienzo): escala + offset (x0,y0), el rect
// SEGURO en px de VENTANA (sin notch/barras) y las posiciones RESUELTAS por el
// layout (anclas/flex). Solo hace falta si el juego usa areaSegura()/cajaUI().
void BindsJuegoRender(float esc, float x0, float y0,
                      float safeWinX, float safeWinY, float safeWinW, float safeWinH,
                      const std::vector<UI2DPos>* posiciones);
// un puntero/dedo (slot 0..3) en coords de LIENZO (centro 0,0), como posPx. El
// clic/mouse entra por el slot 0. Lo alimenta el input de cada build.
void BindsJuegoPunto(int slot, float cx, float cy, bool activo);
// MOUSE-OVER: posicion del mouse (SIN clic) en coords de LIENZO (centro 0,0).
// 'presente' true = hay un mouse real encima del juego (solo lo alimentan los
// eventos de MOUSE; el tactil no pasa por aca -> sin mouse no hay hover).
void BindsJuegoRaton(float cx, float cy, bool presente);
// getter para el overlay (borde de hover de los botones): true si hay mouse
// presente; deja su posicion de lienzo en (cx, cy).
bool BindsJuegoRatonEstado(float* cx, float* cy);
// snapshot del estado apretado para el flanco de apretado() del PROXIMO frame:
// llamar UNA vez por frame DESPUES de correr los scripts.
void BindsJuegoSnapshotPunteros(void);
// soltar todos los punteros (al parar/reiniciar la simulacion).
void BindsJuegoResetPunteros(void);

// ============================================================================
//  LOS CONTROLES CONECTADOS — EL REGISTRO COMPARTIDO (HOTPLUG).
//
//  Pedido del dueno, textual: "me gustaria que cuando conecte por usb el control
//  de ps4 o de xbox diga 'se detecto un nuevo control' con un mensaje/popup que
//  ya tiene el sistema y desde consola. y mensaje de error de 'se desconecto un
//  control'". Antes habia que CERRAR el editor, enchufar y volver a abrir: el
//  mando se buscaba UNA sola vez, al arrancar (SDL_GameControllerOpen(0)).
//
//  Este registro es la parte que NO sabe de SDL: la lista de lo que hay
//  enchufado AHORA, con su nombre y su tipo. Quien maneja los eventos SDL
//  (el editor en main.cpp, el juego compilado en su main.cpp generado) llama a
//  W3dControlAlta/W3dControlBaja y ESTAS avisan por el sistema de
//  notificaciones + la consola, iguales en los dos lados.
//
//  Preparado para lua: los binds `controles()` y `control(n)` ya salen de aca
//  (cuantos hay, como se llama cada uno y de que tipo es), asi que un script
//  puede pausar al desconectarse un mando sin tocar C++.
// ============================================================================
// tipo del mando, en texto y estable: "xbox" | "playstation" | "nintendo" | "generico"
int         W3dControlesCuantos(void);
const char* W3dControlNombre(int i);      // "" fuera de rango
const char* W3dControlTipo(int i);        // "" fuera de rango
void*       W3dControlHandle(int i);      // SDL_GameController* (void*: aca no entra SDL)
int         W3dControlIdEn(int i);        // instance id (-1 fuera de rango)
// alta: 'handle' es el SDL_GameController* ya abierto, 'id' su instance id.
// Duplicado (mismo id) = no-op. true si entro (y aviso).
bool        W3dControlAlta(void* handle, int id, const char* nombre, const char* tipo);
// baja por instance id: deja en *handle el que hay que cerrar (puede ser NULL).
// true si estaba registrado (y aviso).
bool        W3dControlBaja(int id, void** handle);

// salir(): true si algun script pidio cerrar el juego (lo consulta el runtime).
bool BindsJuegoQuierePararse(void);
// pedir el cierre desde el RUNTIME (no desde lua): lo usa la captura de
// verificacion del juego compilado (W3dGameCaptura), que guarda un PNG y cierra.
void BindsJuegoPedirSalir(void);

#endif // BINDSJUEGO_H
