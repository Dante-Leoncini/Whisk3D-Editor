#ifndef W3D_PRUEBAS_HITBOX_H
#define W3D_PRUEBAS_HITBOX_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasHitbox — comandos de harness del HITBOX (objects/Hitbox.h) y de los
//  eventos lua (alEntrar/alQuedarse/alSalir/alTocar). Mismo contrato que
//  W3dPruebasRecursosCmd (que lo llama primero): si el comando es de aca pone
//  manejado=true y devuelve su resultado; si no, manejado=false.
// ============================================================================
bool W3dPruebasHitboxCmd(const std::string& cmd, std::istringstream& ss,
                         std::string& err, bool& manejado);

#endif
