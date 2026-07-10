#ifndef IMPORT_FBX_H
#define IMPORT_FBX_H

#include <string>

// Importa un modelo FBX (BINARIO) a la escena: mallas 3D, normales, UV y (primera) textura. Para arrancar:
// versiones 7100..7400 (offsets de 32 bits) y 7500+ (64 bits). ASCII no (solo binario "Kaydara FBX Binary").
// Los arrays comprimidos se descomprimen con w3dEngine::Inflate (Core). Devuelve true si importo al menos una malla.
bool ImportFBX(const std::string& filepath);

#endif // IMPORT_FBX_H
