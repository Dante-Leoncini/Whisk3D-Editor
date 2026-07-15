#ifndef EXPORT_GLTF_H
#define EXPORT_GLTF_H

#include <string>

// Exporta la escena (o solo lo seleccionado) a glTF 2.0. Es el INVERSO del importador (import_gltf.cpp):
// malla en BIND pose + skin (JOINTS/WEIGHTS + inverseBindMatrices) + huesos (nodos TRS) + animaciones
// (samplers TRS por hueso). NO hornea el skinning: exporta el rig y sus clips, tal como pidio Dante.
//   binary=false -> .gltf (JSON con el buffer embebido en base64: un solo archivo, sin .bin al lado)
//   binary=true  -> .glb (contenedor binario: header + chunk JSON + chunk BIN)
bool ExportGLTF(const std::string& filepath, bool selectedOnly, bool binary);

#endif // EXPORT_GLTF_H
