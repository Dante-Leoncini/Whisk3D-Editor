#ifndef W3D_TESTS_PREFABS_H
#define W3D_TESTS_PREFABS_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasPrefabs — comandos de harness de las INSTANCIAS DE PREFAB
//  (objects/InstancePrefab.h, io/Prefabs.h, io/PrefabsEditor.h). Mismo contrato
//  que W3dTestsResourcesCmd (que lo llama primero): si el comando es de aca pone
//  manejado=true y devuelve su resultado; si no, manejado=false.
// ============================================================================
bool W3dTestsPrefabsCmd(const std::string& cmd, std::istringstream& ss,
                          std::string& err, bool& handled);

#endif
