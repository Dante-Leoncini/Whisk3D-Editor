#ifndef W3D_TESTS_OUTLINER_H
#define W3D_TESTS_OUTLINER_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasOutliner — comandos de harness del OUTLINER POR RECURSOS (vistas,
//  carpetas, en uso / huerfano, recurso activo en Properties). Mismo contrato que
//  W3dTestsResourcesCmd (que lo llama primero): si el comando es de aca pone
//  manejado=true y devuelve su resultado; si no, manejado=false.
// ============================================================================
bool W3dTestsOutlinerCmd(const std::string& cmd, std::istringstream& ss,
                           std::string& err, bool& handled);

#endif
