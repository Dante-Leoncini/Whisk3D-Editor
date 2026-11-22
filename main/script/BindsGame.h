#ifndef BINDS_GAME_H
#define BINDS_GAME_H
#include <vector>
#include <string>

// ============================================================================
//  BindsJuego — el set COMPARTIDO de binds 2D de JUEGO que ven los scripts lua,
//  IGUAL en el editor (Play, via ScriptUI/SimJuego) que en el runtime compilado
//  (game/w3drun). Unifica lo que antes vivia duplicado/divergente en los dos
//  lados, para que un juego corra identico en Play y compilado.
//
//  Binds registrados (coordenadas en PIXELES del lienzo, (0,0) en el CENTRO):
//    screen()             -> ancho, alto del lienzo
//    getPosPx(o)               -> x, y            setPosPx(o, x, y)  (eje en nil = no se toca)
//    getSizePx(o)               -> ancho, alto     setSizePx(o, w, h)
//    setText(o, v)         setTexture(o, "ruta.png")
//    show(o, visible)    setOpacity(o, 0..1)   setSize(o, px)
//    quit()                cierra el juego (la plataforma corta el loop)
//    pressed(o) -> bool    (flanco de tap/click nuevo DENTRO del rect)
//    collide(a, b) -> bool    (AABB de las cajas EN PANTALLA; regla crudo-vs-
//                             resuelto: gameplay libre = pos cruda sin lag,
//                             puesto por el layout = rect resuelto dibujado)
//    limit(o, area)  /  limit(o, arriba, abajo)     getScale() -> min(w,h)/480
//    inside(area, x, y) -> bool   (el punto cae en la caja del area; eje nil = no se mira)
//    safeArea() -> x, y, ancho, alto   (rect sin notch/barras, esquina sup-izq)
//    fade(o, objetivo, paso)    (crossfade de opacidad)
//    uiBox(o) -> x, y, ancho, alto   (rect YA resuelto por el layout, centro)
//
//  LAYOUT: el .w3dui decide DONDE va cada cosa; el script no calcula posiciones,
//  bordes ni centros. Por eso los binds que necesitan una zona toman el OBJETO
//  que la representa (limit(pala, cancha), inside(cancha, x, y), y del lado del
//  Core bounceOn(pelota, cancha, "arriba abajo")) y leen su caja ya resuelta.
//
//  El estado por-frame (punteros del finger/mouse, mapeo ventana<->canvas, rect
//  seguro, posiciones resueltas por el layout) vive ACA y lo ALIMENTAN los dos
//  builds: el runtime desde W3dGameRender/Toque/Actualizar, y el editor desde
//  SimJuego (input del Play) + Editor2D (mapeo y posiciones resueltas).
//
//  C++03 puro (mismo codigo compila para Symbian/Android). Comentarios sin acentos.
// ============================================================================

#include "render/UIOverlay.h"   // UI2DPos, tipo COMPLETO: el STLport de RVCT no acepta vector<incompleto>
class Object;

// emit(obj, n): rafaga de n particulas del objeto Particulas (el polvo de las
// pisadas, un estallido puntual). HOOK como el de setSector: este .cpp lo
// compila tambien el RUNTIME del juego, que no linkea el objeto Particulas del
// editor; el editor lo instala (main/objects/Particles.cpp) y sin instalar el
// bind es un no-op que linkea igual.
extern void (*W3dParticlesEmitHook)(Object* o, int n);
// setEmitting(obj, bool): prende/apaga la emision continua (las vivas terminan su vida). NULL en el runtime sin Particulas.
extern void (*W3dActiveParticlesHook)(Object* o, bool active);
// getFog(obj) / setFog(obj, densidad [, r, g, b [, inicio, fin]]): la Niebla vive en main/objects (el hook lo
// instala Fog.cpp). v[0] densidad, v[1..3] color, v[4] inicio, v[5] fin. Leer: escribir=false (v de salida).
extern bool (*W3dFogHook)(Object* o, bool write, float* v, int n);

// ---------------------------------------------------------------------------
//  LA CAMARA, POR HOOK (mismo patron que los dos de arriba)
//
//  screenOf() y la familia del riel (setRail / setRailNode / setRailLook /
//  getRail) estaban bajo `#ifdef W3D_GAME_RUNTIME` devolviendo 0,0,false / false:
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
//         En SetRail, `curveName` permite pedir el riel por nombre y `soltar`
//         distingue "no hay riel" (nil) de "no lo encontre" (error).
extern bool (*W3dCameraProjectHook)(Object* obj, float canvasW, float canvasH,
                                      float* sx, float* sy, bool* front);
extern bool (*W3dCameraRailSetHook)(Object* cam, Object* curve, const char* curveName,
                                    bool release, const int* offset);
extern bool (*W3dCameraRailNodeHook)(Object* cam, float node);
extern bool (*W3dCameraRailLookHook)(Object* cam, bool on);
// `indice` (opcional, puede ser NULL) devuelve el INDICE FRACCIONARIO del riel
// donde la camara quedo parada este tick (railIndex: seguimiento o nodo manual,
// ya con offsetRail restado). Es el ancla que una VisZone modo curva espera en
// setVisCurveT(zona, t); -1 = la camara todavia no se paro en ningun nodo.
extern bool (*W3dCameraRailStateHook)(Object* cam, const char** name, int* offset,
                                       float* node, bool* look, float* index);
// LENTE: fov (grados, vertical) + los dos planos de recorte. Los punteros nulos en
// SetHook = "no tocar ese". Sirve para ajustar el alcance del dibujado en juego --
// pasar de un area abierta a un pasillo cerrado sin que asome geometria lejana.
extern bool (*W3dCameraLensHook)(Object* cam, float* fov, float* near, float* far);
extern bool (*W3dCameraSetLensHook)(Object* cam, const float* fov, const float* near,
                                     const float* far);
// cameraXZ(): el ADELANTE y la DERECHA de la camara activa proyectados al piso
// (mover al personaje "relativo a como estas mirando"). Mismo patron de hook: lo
// instala Camera.cpp. false = no hay camara -> el bind devuelve el default
// (0,-1, 1,0), NO nil: un juego que lo llama sin guarda no se puede caer.
extern bool (*W3dCameraXZHook)(float* fx, float* fz, float* rx, float* rz);
// PUNTERIA (juegos de disparos, "pistola de luz"): el RAYO de la camara activa por un punto del lienzo
// (px, centro 0,0, y hacia ABAJO: lo de mouse()/tap()) -> origen (en el plano cercano) y direccion
// normalizada, en MUNDO; y el inverso para un PUNTO suelto (screenOf es solo para el origen de un
// objeto). Los instala Camera.cpp con la MISMA lente y aspecto que proyectar.
extern bool (*W3dCameraRayHook)(float sx, float sy, float canvasW, float canvasH, float* o, float* d);
extern bool (*W3dCameraProjectPointHook)(float x, float y, float z, float canvasW, float canvasH,
                                           float* sx, float* sy, bool* front);
// target(): el target del objeto DUENO del script (el personaje que maneja un
// objeto Script). Lo instala quien linkee ObjectScript (main/objects/Gamepad.cpp).
extern Object* (*W3dTargetHook)(Object* owner);

// registra los binds de juego en un lua_State. Firma compatible con
// W3dScriptSetBindExtra (void(*)(void*)); el editor la usa DENTRO de su propio
// registrador (que ademas agrega cameraXZ/target/param), el runtime la
// pasa directo.
void BindsGameRegister(void* L);

// CIERRE DEL PROYECTO (editor): suelta los WAV precargados/cacheados por sound(). La clave es
// la ruta tal como la escribe el proyecto ("sounds/x.wav" en un contenedor): sin vaciarlo el
// siguiente proyecto con una entrada del mismo nombre sonaba con el WAV del anterior, y el
// audio decodificado del proyecto cerrado quedaba en el heap para siempre.
void W3dSoundsFree();
// cuantos WAV hay en ese cache (incluye los que fallaron: se cachean NULL). Lo usa el harness.
int  W3dSoundsCached();
// true si sound() ya cargo (o intento cargar: un faltante tambien se cachea) la ruta RESUELTA 'ruta' (la prueba:
// un script de una libreria resuelve "sounds/x.wav" en SU libreria)
bool W3dSoundCached(const std::string& path);

// --- ALIMENTACION del estado por-frame (la llaman los dos builds) -----------
// mapeo del ULTIMO render (ventana -> lienzo): escala + offset (x0,y0), el rect
// SEGURO en px de VENTANA (sin notch/barras) y las posiciones RESUELTAS por el
// layout (anclas/flex). Solo hace falta si el juego usa safeArea()/uiBox().
void BindsGameRender(float esc, float x0, float y0,
                      float safeWinX, float safeWinY, float safeWinW, float safeWinH,
                      const std::vector<UI2DPos>* positions);
// un puntero/finger (slot 0..3) en coords de LIENZO (centro 0,0), como getPosPx. El
// clic/mouse entra por el slot 0. Lo alimenta el input de cada build.
void BindsGamePoint(int slot, float cx, float cy, bool active);
// MOUSE-OVER: posicion del mouse (SIN clic) en coords de LIENZO (centro 0,0).
// 'presente' true = hay un mouse real encima del juego (solo lo alimentan los
// eventos de MOUSE; el tactil no pasa por aca -> sin mouse no hay hover).
void BindsGameMouse(float cx, float cy, bool present);
// getter para el overlay (borde de hover de los botones): true si hay mouse
// presente; deja su posicion de lienzo en (cx, cy).
bool BindsGameMouseState(float* cx, float* cy);
// snapshot del estado apretado para el flanco de pressed() del PROXIMO frame:
// llamar UNA vez por frame DESPUES de correr los scripts.
void BindsGameSnapshotPointers(void);
// soltar todos los punteros (al parar/reiniciar la simulacion).
void BindsGameResetPointers(void);

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
//  W3dControlAdd/W3dControlRemove y ESTAS avisan por el sistema de
//  notificaciones + la consola, iguales en los dos lados.
//
//  Preparado para lua: los binds `controls()` y `control(n)` ya salen de aca
//  (cuantos hay, como se llama cada uno y de que tipo es), asi que un script
//  puede pausar al desconectarse un mando sin tocar C++.
// ============================================================================
// tipo del mando, en texto y estable: "xbox" | "playstation" | "nintendo" | "generico"
int         W3dControlsCount(void);
const char* W3dControlName(int i);        // "" fuera de rango
const char* W3dControlType(int i);        // "" fuera de rango
void*       W3dControlHandle(int i);      // SDL_GameController* (void*: aca no entra SDL)
int         W3dControlIdIn(int i);        // instance id (-1 fuera de rango)
// alta: 'handle' es el SDL_GameController* ya abierto, 'id' su instance id.
// Duplicado (mismo id) = no-op. true si entro (y aviso).
bool        W3dControlAdd(void* handle, int id, const char* name, const char* type);
// baja por instance id: deja en *handle el que hay que cerrar (puede ser NULL).
// true si estaba registrado (y aviso).
bool        W3dControlRemove(int id, void** handle);

// quit(): true si algun script pidio cerrar el juego (lo consulta el runtime).
bool BindsGameWantsStop(void);
// pedir el cierre desde el RUNTIME (no desde lua): lo usa la captura de
// verificacion del juego compilado (W3dGameCapture), que guarda un PNG y cierra.
void BindsGameRequestExit(void);

#endif // BINDS_GAME_H
