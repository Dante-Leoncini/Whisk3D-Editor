#ifndef W3D_PRUEBAS_ANIMS_H
#define W3D_PRUEBAS_ANIMS_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasAnims — comandos de harness de los ANIMSETS (clips de esqueleto
//  binarios y compartidos, ver animation/W3dAnimSet.h).
//
//  W3dPruebasRecursosCmd (test/W3dPruebasRecursos.cpp) llama aca PRIMERO, con
//  cada comando del script:
//    - antes de mirar el comando verifica los SELLOS de los clips compartidos:
//      ningun clip de un animset cargado puede haber cambiado desde el comando
//      anterior (si cambio, algo lo escribio sin copy-on-write y el comando FALLA);
//    - si el comando es de aca pone manejado=true y devuelve su resultado;
//    - si no, manejado=false y el dispatcher sigue.
//  No repetir nombres con W3dScript.cpp ni con W3dPruebasRecursos.cpp.
// ============================================================================
bool W3dPruebasAnimsCmd(const std::string& cmd, std::istringstream& ss,
                        std::string& err, bool& manejado);
// la linea "animsets" del informe de 'meminfo' (W3dScript.cpp)
void W3dPruebasAnimsMeminfo();

#endif
