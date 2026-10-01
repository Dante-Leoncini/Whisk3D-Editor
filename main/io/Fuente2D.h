#ifndef FUENTE2D_H
#define FUENTE2D_H
// ============================================================================
//  Fuente2D — las FUENTES del Editor 2D.
//
//  Cada fuente es un W3dTextAtlas (glifos en una textura, tamano/color libres).
//  La default ("") es la fuente de Whisk3D (Inter, res/inter_atlas.*, ya
//  horneada). Una ruta .ttf se HORNEA en runtime con stb_truetype y queda
//  cacheada: cargar una fuente nueva es elegir el archivo, nada mas.
// ============================================================================
#include <string>

namespace w3dui { struct W3dTextAtlas; }

// la fuente lista para dibujar (cacheada). "" = la fuente de Whisk3D. NULL si fallo.
w3dui::W3dTextAtlas* Fuente2DObtener(const std::string& ruta);

// CIERRE DEL PROYECTO: suelta todas las fuentes cacheadas (sus texturas propias; el atlas
// COMPARTIDO de una fuente "atlas_unico" es del cache 2D/3D y no se borra aca). Va junto con
// Textura2DLiberarTodas: una fuente atlas_unico guarda un id de ESE cache.
void Fuente2DLiberarTodas();

// nombre corto para mostrar en la UI ("Whisk3D" o "Inter-VariableFont")
std::string Fuente2DNombre(const std::string& ruta);

// VISTA PREVIA de un .ttf para el file browser: rasteriza "AaBb" en RGBA.
// true si ok; el caller libera *outRGBA con delete[].
bool Fuente2DThumb(const std::string& rutaTtf, int maxPx,
                   unsigned char** outRGBA, int* outW, int* outH);

#endif // FUENTE2D_H
