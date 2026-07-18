#ifndef LECTURAESCRITURA_H
#define LECTURAESCRITURA_H

#include <string>

// Declaraciones de funciones
int abrir();
int BuscarVertexAnimation();

#ifndef W3D_SYMBIAN
// dispatch por EXTENSION al importador: .fbx -> ImportFBX; .gltf/.glb -> ImportGLTF; resto (.obj) -> ImportOBJ.
// Compartido por el File browser (abrir), los callbacks del menu Add > Import y el flag --open. Solo no-Symbian.
void ImportModeloPorExtension(const std::string& path);
#endif

#endif