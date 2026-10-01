#ifndef W3D_PRUEBAS_INTEGRACION_H
#define W3D_PRUEBAS_INTEGRACION_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasIntegracion — comandos de harness de la prueba CRUZADA: mallas como
//  recurso + animsets compartidos + hitbox, todo en la misma escena (un personaje
//  skinneado duplicado con Alt+D, guardado, reabierto, en el Play y en el juego
//  compilado). Mismo contrato que W3dPruebasRecursosCmd (que lo llama): si el
//  comando es de aca pone manejado=true y devuelve su resultado; si no,
//  manejado=false.
// ============================================================================
bool W3dPruebasIntegracionCmd(const std::string& cmd, std::istringstream& ss,
                              std::string& err, bool& manejado);

#endif
