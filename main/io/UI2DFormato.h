#ifndef UI2D_FORMATO_H
#define UI2D_FORMATO_H
#include <string>

class UI;
class Object;

// ============================================================================
//  UI2DFormato — GUARDA y CARGA una interfaz 2D (.w3dui): un archivo de TEXTO
//  en JSON con el arbol entero (ventana + elementos + hijos). La idea: disenar
//  los menus/HUD/layout en el Editor 2D en minutos, guardar, y que ese MISMO
//  archivo sea el que compila el juego/programa (RQ_Games, etc).
//
//  RUTAS: las texturas y fuentes que viven al lado del archivo (o mas adentro)
//  se guardan RELATIVAS a el (el proyecto se puede mover de carpeta o de
//  maquina); las de afuera quedan absolutas. Al cargar se resuelven contra la
//  carpeta del archivo.
//
//  El formato es versionado ("version": 1). Mas adelante: animaciones y demas.
// ============================================================================

// escribe el arbol del UI en 'ruta' (JSON legible). false si no pudo escribir.
bool UI2DGuardar(UI* u, const std::string& ruta);

// lee un .w3dui y arma el arbol (el UI queda colgado de la escena, sin
// seleccionar). NULL si el archivo no se pudo leer o no parsea.
UI* UI2DCargar(const std::string& ruta);

#endif // UI2D_FORMATO_H
