#ifndef UVUNWRAP_H
#define UVUNWRAP_H
// ============================================================================
//  UVUnwrap.h - desplegado de UVs sobre las caras seleccionadas en Edit Mode (menu U).
//  Todas escriben el mapa UV activo de las CARAS SELECCIONADAS (con su Ctrl+Z) y, si
//  devuelven false, dejan en 'msg' el aviso (una clave de la tabla de idiomas).
//  Los algoritmos estan en UVUnwrap.cpp, escritos de cero para Whisk3D.
// ============================================================================
#include <string>
class Mesh;

enum { W3dUnwrapAngular = 0, W3dUnwrapConformal = 1, W3dUnwrapStretch = 2 };

// Unwrap: corta la seleccion en islas por las costuras (seamEdges), aplana cada isla y las
// empaqueta en [0,1] con 'margen' entre ellas. 'islas' (opcional) recibe cuantas salieron.
bool W3dUnwrap(Mesh* m, int method, float margin, std::string& msg, int* islands = 0);
// Smart UV Project: caras agrupadas por normal (limite en grados), cada grupo proyectado a su plano.
bool W3dSmartUVProject(Mesh* m, float angleLimitDeg, float margin, std::string& msg, int* islands = 0);
bool W3dSmartUVProjectAll(Mesh* m, float angleLimitDeg, float margin, std::string& msg);   // todas las caras (sin Edit Mode)
// Lightmap Pack: cada cara en su propia celda de una grilla cuadrada.
bool W3dLightmapPack(Mesh* m, float margin, std::string& msg);
// Follow Active Quads: extiende la grilla UV del quad activo a los quads seleccionados vecinos.
bool W3dFollowActiveQuads(Mesh* m, std::string& msg, int* quads = 0);
// Reset: cada cara seleccionada ocupa toda la textura.
bool W3dUVReset(Mesh* m, std::string& msg);
#endif // UVUNWRAP_H
