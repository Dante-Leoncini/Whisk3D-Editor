#ifndef GUARDARW3D_H
#define GUARDARW3D_H
#include <string>

// ============================================================================
//  GuardarW3D — guarda el PROYECTO como .w3d (el mismo formato de texto que
//  abre OpenW3D). Version 1, honesta:
//    - guarda: colecciones, camaras, luces, objetos Script/gamepad (con su
//      script + referencias) y la UI (que se escribe a un .w3dui HERMANO y el
//      .w3d la referencia con `UI { archivo: ... }`).
//    - las MALLAS importadas Wavefront conservan su referencia si la traen;
//      las modeladas a mano TODAVIA no se guardan (aviso, no error).
//  Ctrl+S guarda al archivo abierto (w3dPath); sin archivo, pide nombre.
// ============================================================================
bool GuardarW3D(const std::string& ruta);

// Ctrl+S: guarda a w3dPath, o abre el explorador en modo guardar si no hay
void GuardarProyecto();

#endif // GUARDARW3D_H
