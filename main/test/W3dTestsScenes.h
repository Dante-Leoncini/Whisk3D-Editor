#ifndef W3D_TESTS_SCENES_H
#define W3D_TESTS_SCENES_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasEscenas — comandos de harness de las RAICES del proyecto: ESCENAS 3D
//  y PREFABS (W3dRoots.h, io/EditorRoots.h). Mismo contrato que
//  W3dTestsResourcesCmd (que lo llama primero): si el comando es de aca pone
//  manejado=true y devuelve su resultado; si no, manejado=false.
// ============================================================================
bool W3dTestsScenesCmd(const std::string& cmd, std::istringstream& ss,
                          std::string& err, bool& handled);

#endif
