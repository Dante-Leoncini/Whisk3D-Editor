#ifndef SIMJUEGO_H
#define SIMJUEGO_H

// ============================================================================
//  SimJuego — la SIMULACION del editor: al dar PLAY en el timeline, los
//  objetos con script (lua) cobran vida; el frame 1 es el estado inicial.
//
//  - PLAY:  snapshot del estado, carga los scripts (en el orden del arbol,
//           como se dibuja/outliner), resuelve las referencias y llama
//           inicio(); despues corre actualizar(dt) una vez por frame.
//  - PAUSA: (barra espaciadora / boton) congela; los botones < > del
//           timeline van FRAME A FRAME: adelante re-simula, atras VUELVE
//           EN EL TIEMPO (el editor graba un snapshot por frame).
//  - PLAY tras retroceder: descarta el futuro grabado y se sigue jugando
//           desde ese momento (las variables internas del lua no viajan
//           en el tiempo: solo el estado de los objetos).
//  - STOP:  (boton "inicio" del timeline) restaura TODO al estado inicial
//           y descarga los scripts.
// ============================================================================

bool SimActiva();                 // hay una partida cargada (jugando o en pausa)
// MODO JUEGO de verdad: el timeline esta en "Juego" (AnimEsJuego) Y hay una
// partida cargada (jugando o en pausa; con Stop vuelve a false). Es el gate
// compartido de "el juego se ve limpio": los overlays de EDICION (curvas,
// empties, huesos, gizmos de camara, iconos 3D...) no se dibujan mientras esto
// de true — mismo criterio que el borde blanco del passepartout.
bool W3dJuegoCorriendo();
bool SimHayScripts();             // hay algo con script en la escena?
void SimTickPlay(float dt);       // lo llama el main loop con el timeline en PLAY
void SimStop();                   // restaurar el estado inicial + descargar
bool SimStep(int dir);            // en pausa: +1 re-simula / -1 vuelve un frame
void SimIrA(int tick);            // saltar a un frame GRABADO (scrub / ir al final)
void SimTeclaSDL(int sdlk, bool down);   // teclado del editor -> scripts
int  SimFramesGrabados();
int  SimFrameActual();
int  SimPrimerFrame();            // el tick mas viejo aun grabado (el cache RUEDA)
// re-resuelve las referencias/opciones de los scripts de 'o' (editar una propiedad
// del script durante el juego se ve al instante)
void SimReresolver(class Object* o);
// los scripts de 'o' CAMBIARON (agregar/quitar/cambiar archivo) con el juego andando:
// se recargan al instante (sus variables locales arrancan de cero)
void SimScriptsCambiados(class Object* o);
// TOQUE en un viewport durante el juego (jugar con el dedo/mouse): convierte las
// coordenadas de pantalla al LIENZO del juego y alimenta toque() de los scripts.
// En el Editor2D usa el mapeo real (zoom/pan); en el 3D, el rect del viewport.
void SimToquePantalla(class ViewportBase* v, int mx, int my, bool activo);
// techo del cache de estados (frames): configurable desde la tarjeta Juego
extern int gSimCacheMax;
extern bool gSimCacheOn;   // cache de juego (rewind) ON/OFF: OFF = sin snapshot por tick -> juego fluido
bool SimHayCache();        // hay frames grabados para rebobinar? (OFF/juego compilado -> false)
void SimCacheReset();      // limpia el cache y lo re-ancla al tick actual (al des/tildar "Cache de juego")

#endif // SIMJUEGO_H
