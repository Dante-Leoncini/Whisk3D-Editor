#ifndef SIMJUEGO_H
#define SIMJUEGO_H

// ============================================================================
//  SimJuego — la SIMULACION del editor: al dar PLAY en el timeline, los
//  objetos con script (lua) cobran vida; el frame 1 es el estado inicial.
//
//  - PLAY:  snapshot del estado, carga los scripts (en el orden del arbol,
//           como se dibuja/outliner), resuelve las referencias y llama
//           inicio(); despues corre actualizar(dt) una vez por frame.
//  - PAUSA: (barra espaciadora / boton) congela; el boton > del timeline
//           avanza FRAME A FRAME (simula uno). El juego no graba su pasado:
//           no se rebobina.
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
bool SimHayScripts();             // hay algo que simular? (algo con script, o la raiz activa es un JUEGO)
void SimTickPlay(float dt);       // lo llama el main loop con el timeline en PLAY
// instrumentacion de scripts (para [PERF]/overlay): activos este tick / total cargados / ms del loop
extern int    g_luaScriptsActivos;
extern int    g_luaScriptsTotal;
extern double g_luaTickMs;
// el DESGLOSE de la logica del juego por fase (ver SimJuego.cpp): ms acumulados en 'out', devuelve cuantos ticks
// abarcan; 'reiniciar' los pone en cero
enum { SIM_PERF_FISICA = 0, SIM_PERF_HITBOX, SIM_PERF_LUA, SIM_PERF_ANIM, SIM_PERF_STREAM, SIM_PERF_N };
int SimPerfDesglose(double out[SIM_PERF_N], bool reiniciar);
void SimLuaPerfTop(char* buf, int bufLen, int frames);   // top-3 scripts por ms ("Juego:41 agua:12"), resetea
void SimStop();                   // restaurar el estado inicial + descargar
bool SimStep(int dir);            // en pausa: +1 simula un frame (-1 no hace nada: no se rebobina)
void SimTeclaSDL(int sdlk, bool down);   // teclado del editor -> scripts
int  SimFrameActual();
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

#endif // SIMJUEGO_H
