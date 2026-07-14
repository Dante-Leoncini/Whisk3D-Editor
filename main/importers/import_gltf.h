#ifndef IMPORT_GLTF_H
#define IMPORT_GLTF_H

#include <string>

// Importador glTF 2.0 (.gltf con buffer externo/base64, y .glb binario). A diferencia del FBX, el modelo de skinning
// de glTF es LIMPIO y estandar: huesos = nodos con TRS, inverseBindMatrices dadas, animaciones = keyframes de TRS por
// nodo. Sin Lcl degenerado, sin figure-scale, sin reconstruccion de biped: mapea al FK ESTANDAR del core (Y-up).
bool ImportGLTF(const std::string& filepath);

#endif // IMPORT_GLTF_H
