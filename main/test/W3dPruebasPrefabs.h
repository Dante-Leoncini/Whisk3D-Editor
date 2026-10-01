#ifndef W3D_PRUEBAS_PREFABS_H
#define W3D_PRUEBAS_PREFABS_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasPrefabs — comandos de harness de las INSTANCIAS DE PREFAB
//  (objects/InstanciaPrefab.h, io/Prefabs.h, io/PrefabsEditor.h). Mismo contrato
//  que W3dPruebasRecursosCmd (que lo llama primero): si el comando es de aca pone
//  manejado=true y devuelve su resultado; si no, manejado=false.
// ============================================================================
bool W3dPruebasPrefabsCmd(const std::string& cmd, std::istringstream& ss,
                          std::string& err, bool& manejado);

#endif
