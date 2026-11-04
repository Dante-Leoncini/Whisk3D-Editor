#ifndef PANTALLASEXTERNAS_H
#define PANTALLASEXTERNAS_H

/**
 * @file PantallasExternas.h
 * @brief Pantallas que se conectan al equipo (TV-out RCA, HDMI): que hacer con cada una.
 *
 * Al conectarse una pantalla, la plataforma avisa (PantallaExternaConectada) y el editor pregunta con un popup:
 *   - "Clonar pantalla": no se hace nada; el sistema copia la pantalla del telefono (lo de siempre).
 *   - "Usar nuevo viewport": la pantalla externa es un MONITOR EXTENDIDO con su propio arbol de viewports (arranca
 *     con un 3D, independiente del del telefono; se divide en filas/columnas y se cambia el tipo como cualquier otro).
 *   - "Viewport de propiedades": lo mismo, arrancando con un panel de Propiedades.
 *   El mouse (el Bluetooth del telefono) pasa de una pantalla a la otra por el borde: sale por la derecha del telefono
 *   y entra por la izquierda de la externa. Mientras esta ahi, sus eventos van al arbol de esa pantalla.
 *
 * La logica (el popup, el arbol de cada pantalla, el dibujo con los globales de pantalla cambiados) es compartida;
 * la plataforma solo crea/borra la superficie de cada pantalla y la activa/presenta con los ganchos de abajo.
 * Sin ganchos (el harness de la PC) se dibuja en la superficie actual: sirve para probar la logica.
 * C++03 (se compila en el N95).
 */

// modos de una pantalla externa
enum {
    PantallaSinDecidir = 0,   // el popup esta abierto (o se cerro sin elegir: queda clonando)
    PantallaClonar     = 1,
    PantallaViewport3D = 2,
    PantallaPropiedades = 3,
    // SOLO LA EXTERNA: el editor entero (el arbol del telefono) pasa a la pantalla externa, a su resolucion, y el
    // telefono deja de dibujar (la GPU dibuja una pantalla sola: mas rapido). El mouse queda en la externa. Al
    // desconectarla, o al elegir otro modo, el arbol vuelve al tamano del telefono
    PantallaSolo = 4
};

// ---- plataforma -> editor ----
// id = el numero de pantalla de la plataforma (Symbian: el de wserv, 1 = la primera externa). w/h = la resolucion
// a la que se va a dibujar (ya recortada por la plataforma: el N95 dibuja a 240p). puedeViewport = false si la
// plataforma solo puede clonar (las opciones de viewport salen en gris).
void PantallaExternaConectada(int id, int w, int h, bool puedeViewport);
void PantallaExternaDesconectada(int id);
void PantallaExternaElegir(int id, int modo);   // lo que hace el popup (y el harness)

// ---- container / loop principal ----
// dibuja las pantallas que tienen viewport propio (despues del swap de la principal). Devuelve cuantas dibujo.
int  PantallasExternasRender();
int  PantallasExternasCantidad();
// el estado de la pantalla 'idx' (0..Cantidad-1): para el harness y el log. false si no existe.
bool PantallaExternaInfo(int idx, int* id, int* w, int* h, int* modo);

// ---- MONITOR EXTENDIDO ----
// atender algo EN una pantalla externa (los eventos del mouse cuando el cursor esta ahi, o su dibujo): los globales del
// editor (la raiz del arbol, el viewport/3D/Properties activos, el alto de pantalla, el tamano para los menus) pasan a
// ser los de esa pantalla hasta Salir, que guarda lo que cambio (un split cambia la raiz) y devuelve los del telefono.
// false = esa pantalla no tiene arbol propio (clona)
bool PantallaExternaEntrar(int id);
void PantallaExternaSalir();
int  PantallaExternaEnUso();          // la que esta Entrada (0 = el telefono)
// la primera pantalla con arbol propio, a la derecha del telefono (el mouse pasa a ella): false si no hay
bool PantallaExternaVecina(int* id, int* w, int* h);
// el menu/popup abierto es de una pantalla externa (se abrio con el mouse ahi): se dibuja ahi y no en el telefono.
// 0 = del telefono
int  PantallaExternaDelMenu();
// la pantalla en modo "solo la externa" (0 = ninguna): el container no dibuja el telefono y el mouse vive ahi
int  PantallaExternaSolo();

// ---- ganchos de la plataforma (NULL = no hay superficie aparte) ----
// el modo cambio: crear (modo 2/3) o borrar (modo 0/1) la superficie de esa pantalla. false = no se pudo crear
// (la pantalla vuelve a clonar y se avisa con un toast)
extern bool (*PantallaExternaPreparar)(int id, int modo);
extern bool (*PantallaExternaActivar)(int id);    // la superficie de 'id' pasa a ser la de dibujo
extern void (*PantallaExternaPresentar)(int id);  // swap de esa superficie
extern void (*PantallaExternaVolver)();           // la superficie principal vuelve a ser la de dibujo
extern void (*PantallaExternaCursor)(int id, int w, int h);   // dibujar el cursor del mouse (si esta en esa pantalla)

#endif
