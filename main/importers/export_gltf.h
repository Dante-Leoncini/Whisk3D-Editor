#ifndef EXPORT_GLTF_H
#define EXPORT_GLTF_H

#include <string>
#include <vector>

class Mesh;

// ORDEN CANONICO de las caras de una malla al exportarla a glTF: los indices de m->faces3d en el MISMO
// orden en que ExportGLTF emite sus triangulos (por mesh part ascendente y, dentro de cada uno, en orden
// de faces3d; se saltean las caras de menos de 3 corners y las de 'mat' fuera de rango cuentan como 0).
// La usa el propio exportador. La compartia con el bloque "topologia" del .w3d, que ya no se escribe
// (la geometria va en .w3dm, con los poligonos nativos); el LECTOR de ese bloque la sigue necesitando
// para MIGRAR los proyectos viejos, asi que el criterio de emision no se puede cambiar solo.
void OrdenCarasGLB(const Mesh* m, std::vector<int>& orden);

// Exporta la escena (o solo lo seleccionado) a glTF 2.0. Es el INVERSO del importador (import_gltf.cpp):
// malla en BIND pose + skin (JOINTS/WEIGHTS + inverseBindMatrices) + huesos (nodos TRS) + animaciones
// (samplers TRS por hueso). NO hornea el skinning: exporta el rig y sus clips.
//   binary=false -> .gltf (JSON con el buffer embebido en base64: un solo archivo, sin .bin al lado)
//   binary=true  -> .glb (contenedor binario: header + chunk JSON + chunk BIN)
bool ExportGLTF(const std::string& filepath, bool selectedOnly, bool binary);

#endif // EXPORT_GLTF_H
