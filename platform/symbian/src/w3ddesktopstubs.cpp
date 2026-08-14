// ============================================================================
//  w3ddesktopstubs.cpp — stubs de simbolos que hoy NO tienen su .cpp en el build
//  de Symbian. Existen para que el editor LINKEE en el telefono; se reemplazan por
//  la implementacion real al portar cada pieza.
//
//   - g_w3dUINoCargo: el global lo DEFINE import_w3d.cpp (cargador del proyecto
//     .w3d), diferido en Symbian; GuardarW3D.cpp lo lee -> aca queda como global.
//   - AbrirProyectoDesde: abrir un proyecto .w3d (lo hace import_w3d.cpp, diferido).
//   - CompilarJuego: compila el juego a binario (cmake/emcc/ndk via popen/system):
//     no aplica en el telefono. Devuelve false (el build no arranco).
//   - AlimentarDedosJuego: multi-touch del editor de escritorio (SDL); en Symbian
//     el input entra por otra via.
// ============================================================================
#include <string>

class UI;

std::string g_w3dUINoCargo;

void AbrirProyectoDesde(const std::string&) {}

bool CompilarJuego(UI*, int, int, int, bool, bool, bool, bool) { return false; }

void AlimentarDedosJuego() {}
