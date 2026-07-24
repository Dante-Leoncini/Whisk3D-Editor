#ifndef SKIN_ATLAS_H
#define SKIN_ATLAS_H
#include <string>
#include "WhiskUI/draw/icons.h"   // IconRect / ICON_TOTAL

// ============================================================================
//  SkinAtlas — arma EL atlas del skin al ARRANCAR: font.png (fuente + borde +
//  cards + scrollbar, con sus coordenadas de siempre) va entero en el (0,0) y
//  cada icono INDIVIDUAL de atlas/iconos/<nombre>.png se empaqueta en el
//  espacio libre. Una sola textura = una sola llamada de bind para toda la UI.
//
//  Agregar un icono nuevo = tirar su png en atlas/iconos/ (el nombre es el del
//  enum IconType) + una fila en el enum. Un icono SIN png cae al arte que ya
//  esta dentro de font.png (migracion suave). Si el canvas se llena, crece
//  solo: 128x128 -> 256x128 -> 256x256 -> 512x256...
// ============================================================================

// true si armo el atlas dinamico (existe skinDir + "atlas/iconos/"). Deja la
// textura subida, su tamano, y el rect ABSOLUTO de cada icono en 'rects'.
bool SkinAtlasArmar(const std::string& skinDir, unsigned* outTex,
                    int* outW, int* outH, IconRect rects[ICON_TOTAL]);

#endif // SKIN_ATLAS_H
