#ifndef W3D_PRUEBAS_ESCENAS_H
#define W3D_PRUEBAS_ESCENAS_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasEscenas — comandos de harness de las RAICES del proyecto: ESCENAS 3D
//  y PREFABS (W3dRaices.h, io/RaicesEditor.h). Mismo contrato que
//  W3dPruebasRecursosCmd (que lo llama primero): si el comando es de aca pone
//  manejado=true y devuelve su resultado; si no, manejado=false.
// ============================================================================
bool W3dPruebasEscenasCmd(const std::string& cmd, std::istringstream& ss,
                          std::string& err, bool& manejado);

#endif
