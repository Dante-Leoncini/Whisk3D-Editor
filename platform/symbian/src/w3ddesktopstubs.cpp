// ============================================================================
//  w3ddesktopstubs.cpp — stubs de simbolos que hoy NO tienen su .cpp en el build
//  de Symbian. Existen para que el editor LINKEE en el telefono; se reemplazan por
//  la implementacion real al portar cada pieza.
//
//   - CompilarJuego: compila el juego a binario (cmake/emcc/ndk via popen/system):
//     no aplica en el telefono. Devuelve false (el build no arranco).
//   - AlimentarDedosJuego: multi-touch del editor de escritorio (SDL); en Symbian
//     el input entra por otra via.
//
//  (g_w3dUINoCargo y AbrirProyectoDesde ya NO van aca: los define import_w3d.cpp,
//   que ahora SI compila en Symbian.)
// ============================================================================
#include <string>

class UI;

bool CompilarJuego(UI*, int, int, int, bool, bool, bool, bool) { return false; }

void AlimentarDedosJuego() {}
