#ifndef W3D_STL_H
#define W3D_STL_H

#include <string>

// ---------------------------------------------------------------------------------------------------------------
//  STL: el formato de la impresion 3D y del CAD. Solo triangulos sueltos (sin UV, sin color, sin materiales, sin
//  jerarquia ni unidades): cada triangulo trae sus 3 vertices y una normal.
//
//  EJES: el STL casi siempre viene con Z arriba (el de las impresoras y los programas de CAD) y Whisk3D usa Y
//  arriba. El import lo gira para que el modelo quede parado y el export lo devuelve: (x, y, z) del STL = (x, -z, y)
//  de Whisk3D. Las medidas pasan tal cual (1 unidad del archivo = 1 m): una pieza en milimetros entra grande.
// ---------------------------------------------------------------------------------------------------------------

// Importa un .stl (binario o ASCII) como malla nueva en la coleccion activa. Los vertices que coinciden se sueldan
// (la malla queda unida, para editarla) y las normales son las de cada cara (sombreado plano, como en el CAD).
// Un STL ASCII con varios "solid" da un objeto por cada uno. false = no se pudo leer o no trae triangulos.
bool ImportSTL(const std::string& path);

// Exporta las mallas (o solo las seleccionadas) a UN .stl binario. applyModifiers = la malla generada por los
// modificadores en vez de la editable. applyTransforms = en coordenadas de MUNDO (lo normal: el STL no guarda
// transformaciones, asi que sin eso cada malla sale en el origen, como si no tuviera posicion, giro ni escala).
bool ExportSTL(const std::string& path, bool selectedOnly, bool applyModifiers = true, bool applyTransforms = true);

#endif
