#ifndef W3D_PRUEBAS_LIBRERIAS_H
#define W3D_PRUEBAS_LIBRERIAS_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasLibrerias — comandos de harness de las LIBRERIAS EXTERNAS y los
//  PROXIES W3D (io/Librerias.h, objects/ProxyW3d.h). Mismo contrato que
//  W3dPruebasRecursosCmd (que lo llama primero): si el comando es de aca pone
//  manejado=true y devuelve su resultado; si no, manejado=false.
// ============================================================================
bool W3dPruebasLibreriasCmd(const std::string& cmd, std::istringstream& ss,
                            std::string& err, bool& manejado);

#endif
