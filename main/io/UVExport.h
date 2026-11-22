#ifndef UV_EXPORT_H
#define UV_EXPORT_H
// ============================================================================
//  UVExport.h - "Export UV Layout" del editor UV: el mapa de las UV en un PNG con transparencia, para pintar
//  la textura en un programa 2D (se pone como capa encima). Fondo transparente; cada cara es un relleno
//  semitransparente (donde se superponen caras se acumula y queda mas opaco: se ven las superposiciones) y
//  los bordes van encima, opacos.
//  Coordenadas: x = u * ancho, y = v * alto (v = 0 arriba), la misma convencion que la textura en memoria y el
//  editor UV, asi el PNG calza pixel a pixel con la textura.
// ============================================================================
#include <string>
#include <vector>
class Mesh; class Texture;

// tamano que propone el popup: el de la textura mostrada si hay; sino 256x256 en el N95 y 512x512 en el resto
void UVExportDefaultSize(Texture* tex, int& w, int& h);
// nombre que propone el popup ("<textura>_uv.png" o "<malla>_uv.png")
std::string UVExportDefaultName(Mesh* m, Texture* tex);
// rasteriza el mapa a RGBA (w*h*4, fila 0 = arriba). 'filtro': solo las caras cuyo material usa esa textura
// (si ninguna parte la usa, o es NULL, van todas). Devuelve false si la malla no tiene UV / caras.
bool UVExportRaster(Mesh* m, Texture* filter, int w, int h, std::vector<unsigned char>& rgba, int* facesOut = 0);
// rasteriza y escribe el PNG (RGBA) en 'ruta'. msg = el error (en ingles, se traduce al mostrarlo)
bool UVExportPNG(Mesh* m, Texture* filter, int w, int h, const std::string& path, std::string& msg);
// fraccion del area UV cubierta que cubre MAS DE UNA cara (sin UV = 1): la pintura de textura despliega si se pisan
float UVOverlappedFraction(Mesh* m);
#endif // UV_EXPORT_H
