#ifndef W3D_HISTORIAL_H
#define W3D_HISTORIAL_H

// ============================================================================
//  W3dHistorial -- la GRABADORA DE PASOS: anota en un archivo de texto todo lo que la persona hace en el editor, para
//  poder armar despues un tutorial con los pasos exactos (y sus videos para la PC, el tactil y el N95).
//
//  Se prende con la variable de entorno W3D_HISTORIAL=<ruta del .txt>. Sin ella no hace nada (una comparacion por
//  evento y otra por cuadro). Solo en la PC: vive en main/test, que Symbian, la Dreamcast, la 3DS, la PSP y la Vita no
//  compilan, y todo va adentro de W3D_MCP (lo define el CMakeLists de escritorio).
//
//  Anota dos cosas:
//   - el COMO: cada tecla, cada click (y que habia debajo: el boton de la barra, la fila de Propiedades, la manija
//     del gizmo...), cada opcion de menu elegida (con la ruta: Objeto > Definir Origen > Origen al Cursor 3D), cada
//     arrastre y la rueda;
//   - el QUE: despues de cada cosa, lo que cambio (el modo, lo elegido, la seleccion de la malla con su centro y su
//     tamanio, la transformacion y las medidas del objeto, el cursor 3D, los modificadores, los pasos del Ctrl+Z).
//  Las posiciones van como las muestra Propiedades: Z arriba, en metros.
//
//  El archivo se escribe y se vacia en cada linea: si el editor se cierra de golpe, lo anotado queda.
// ============================================================================
#ifdef W3D_MCP

#include "app/controles.h"   // SDL_Event

void W3dHistorialIniciar();                       // main.cpp, al arrancar: lee W3D_HISTORIAL
bool W3dHistorialActivo();
void W3dHistorialEvento(const SDL_Event& e);      // controles.cpp, ANTES de procesar el evento (lo que hay debajo)
void W3dHistorialCuadro();                        // main.cpp, cada cuadro: anota lo que cambio
void W3dHistorialUndo(const char* tipo);          // Undo.cpp: un paso nuevo en el Ctrl+Z (el tipo de comando)

#endif
#endif
