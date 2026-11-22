#ifndef W3D_TESTS_STREAMING_H
#define W3D_TESTS_STREAMING_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dPruebasStreaming — comandos de harness del STREAMING (io/Streaming.h): las
//  instancias de prefab y los proxies DIFERIDOS que cargan y descargan en tiempo
//  real. Mismo contrato que W3dTestsResourcesCmd (que lo llama primero): si el
//  comando es de aca pone manejado=true y devuelve su resultado; si no,
//  manejado=false.
// ============================================================================
bool W3dTestsStreamingCmd(const std::string& cmd, std::istringstream& ss,
                            std::string& err, bool& handled);

#endif
