#ifndef FONT2_D_H
#define FONT2_D_H
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
w3dui::W3dTextAtlas* Font2DGet(const std::string& path);
// la misma para dibujarla a 'px' pixeles de PANTALLA: una .ttf se hornea a ese tamano (cacheada por tamano) y se
// dibuja 1:1 -> el texto queda nitido en cualquier definicion (un telefono, un monitor 4K). Las demas: Font2DGet
w3dui::W3dTextAtlas* Font2DGetPx(const std::string& path, float px);

// CIERRE DEL PROYECTO: suelta todas las fuentes cacheadas (sus texturas propias; el atlas
// COMPARTIDO de una fuente "atlas_unico" es del cache 2D/3D y no se borra aca). Va junto con
// Texture2DFreeAll: una fuente atlas_unico guarda un id de ESE cache.
void Font2DFreeAll();

// nombre corto para mostrar en la UI ("Whisk3D" o "Inter-VariableFont")
std::string Font2DName(const std::string& path);

// VISTA PREVIA de un .ttf para el file browser: rasteriza "AaBb" en RGBA.
// true si ok; el caller libera *outRGBA con delete[].
bool Font2DThumb(const std::string& ttfPath, int maxPx,
                   unsigned char** outRGBA, int* outW, int* outH);

#endif // FONT2D_H
