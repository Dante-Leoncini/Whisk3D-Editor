#ifndef LECTURAESCRITURA_H
#define LECTURAESCRITURA_H

#include <string>

// Declaraciones de funciones
int abrir();
int BuscarVertexAnimation();

#ifndef W3D_SYMBIAN
// dispatch por EXTENSION al importador: .fbx -> ImportFBX; .gltf/.glb -> ImportGLTF; resto (.obj) -> ImportOBJ.
// LA PUERTA UNICA de importar un modelo: ademas de elegir el importador, marca Mesh::origen en todo
// lo que nacio (de ahi salen "reimportar del original" y el renglon de EXTERNOS.txt). Cualquier camino
// nuevo tiene que entrar por aca y NO llamar a los importadores directo. Solo no-Symbian.
bool ImportModeloPorExtension(const std::string& path);

// el MISMO import, con la firma que pide el callback del File browser (void(const std::string&)).
// Existe solo para no tentar a nadie a llamar a los importadores directo desde un callback.
void ImportModeloCallback(const std::string& path);
#endif

#endif