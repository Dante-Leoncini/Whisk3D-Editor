#ifndef TEXTURA2D_H
#define TEXTURA2D_H
#include <string>
#include <vector>

// ============================================================================
//  Textura2D — cache de texturas para los elementos IMAGEN de las interfaces
//  2D (Imagen2D). Cada archivo se sube a la GPU UNA vez por sesion y se
//  recuerda su tamano en pixeles (para "ajustar"/"cover" y para darle a una
//  imagen nueva su tamano natural).
// ============================================================================

// GL id de la textura (0 si no se pudo cargar) + tamano del archivo en px.
// La ruta "" devuelve 0 sin intentar cargar.
unsigned Textura2DObtener(const std::string& ruta, int* w = 0, int* h = 0);

// enumera las rutas CACHEADAS (las imagenes 2D del PROYECTO: HUD, Imagen2D...).
// Lo usa el dropdown Texture del editor UV para listar la UI del juego.
void Textura2DListar(std::vector<std::string>& rutas);

// ruta que stb PUEDE decodificar: un .webp se convierte (ffmpeg, editor de PC) a un png
// cacheado en /tmp una vez por sesion; cualquier otra ruta vuelve tal cual. La usan el
// cache de texturas y las miniaturas del explorador.
std::string Textura2DRutaDecodificable(const std::string& ruta);

#endif // TEXTURA2D_H
