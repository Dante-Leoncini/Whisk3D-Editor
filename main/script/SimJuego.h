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
// techo del cache de estados (frames): configurable desde la tarjeta Juego
extern int gSimCacheMax;

#endif // SIMJUEGO_H
