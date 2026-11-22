#ifndef TEXTURE2_D_H
#define TEXTURE2_D_H
#include <string>
#include <vector>

// ============================================================================
//  Textura2D — cache de texturas para los elementos IMAGEN de las interfaces
//  2D (Image2D). Cada archivo se sube a la GPU UNA vez por sesion y se
//  recuerda su tamano en pixeles (para "ajustar"/"cover" y para darle a una
//  imagen nueva su tamano natural).
// ============================================================================

// GL id de la textura (0 si no se pudo cargar) + tamano del archivo en px.
// La ruta "" devuelve 0 sin intentar cargar.
unsigned Texture2DGet(const std::string& path, int* w = 0, int* h = 0);

// enumera las rutas CACHEADAS (las imagenes 2D del PROYECTO: HUD, Image2D...).
// Lo usa el dropdown Texture del editor UV para listar la UI del juego.
void Texture2DList(std::vector<std::string>& paths);

// CIERRE DEL PROYECTO: suelta TODO el cache (borra de la GPU las texturas propias; las
// PRESTADAS del cache 3D no, esas son del material que las subio). La clave es la RUTA tal
// como la escribe el proyecto ("textures/hud.png" en un contenedor): sin vaciarlo, abrir
// OTRO proyecto con una entrada del mismo nombre mostraba la imagen del anterior.
void Texture2DFreeAll();

// ruta que stb PUEDE decodificar: un .webp se convierte (ffmpeg, editor de PC) a un png
// cacheado en /tmp una vez por sesion; cualquier otra ruta vuelve tal cual. La usan el
// cache de texturas y las miniaturas del explorador.
std::string Texture2DDecodablePath(const std::string& path);

#endif // TEXTURE2_D_H
