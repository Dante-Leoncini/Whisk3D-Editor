#ifndef W3D_PRUEBAS_RECURSOS_H
#define W3D_PRUEBAS_RECURSOS_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasRecursos — comandos de harness de RECURSOS, CARGA y FORMATOS.
//
//  Viven aparte de test/W3dScript.cpp a proposito: ese archivo tarda minutos en
//  compilar y cualquier comando nuevo lo recompilaba entero. W3dRunCommand llama
//  a esta funcion ANTES de su cadena de comandos:
//    - 'cmd' ya viene leido; 'ss' esta parado justo despues (los argumentos);
//    - si el comando es de aca, pone manejado=true y devuelve su resultado
//      (false + 'err' con el motivo si fallo);
//    - si no lo conoce, deja manejado=false y W3dRunCommand sigue como siempre.
//  Por eso un comando de aca PISA a uno homonimo de W3dScript.cpp: no repetir
//  nombres (buscar con grep antes de agregar uno).
// ============================================================================
bool W3dPruebasRecursosCmd(const std::string& cmd, std::istringstream& ss,
                           std::string& err, bool& manejado);

#endif
