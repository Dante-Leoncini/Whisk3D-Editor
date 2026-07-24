#ifndef TEXTURA2D_H
#define TEXTURA2D_H
#include <string>

// ============================================================================
//  Textura2D — cache de texturas para los elementos IMAGEN de las interfaces
//  2D (Imagen2D). Cada archivo se sube a la GPU UNA vez por sesion y se
//  recuerda su tamano en pixeles (para "ajustar"/"cover" y para darle a una
//  imagen nueva su tamano natural).
// ============================================================================

// GL id de la textura (0 si no se pudo cargar) + tamano del archivo en px.
// La ruta "" devuelve 0 sin intentar cargar.
unsigned Textura2DObtener(const std::string& ruta, int* w = 0, int* h = 0);

#endif // TEXTURA2D_H
