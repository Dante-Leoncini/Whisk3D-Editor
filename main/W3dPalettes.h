#ifndef W3D_PALETTES_H
#define W3D_PALETTES_H
#include <string>
#include <vector>
#include "objects/UI.h"   // Paleta / PaletteColor (el mismo esquema de siempre)

class Object;

// ============================================================================
//  W3dPalettes — las PALETAS DE COLORES a nivel PROYECTO, COMPARTIDAS por el
//  editor y el runtime compilado (la implementacion vive en W3dScene.cpp,
//  que ya compila en todos los builds: no hay que tocar ningun CMake).
//
//  MODELO: la fuente de verdad es el PROYECTO (el .w3d las guarda en su raiz
//  "paletas"); cada objeto puede elegir una POR NOMBRE (Object::paleta) y los
//  hijos la HEREDAN ("" = la del padre). Los elementos 2D referencian COLORES
//  por INDICE (campos pal*) contra su paleta EFECTIVA. El palette-swap es
//  cambiar la paleta de un padre: todos sus herederos se re-pintan.
//
//  RUNTIME standalone: los juegos cargan .w3dui sueltos sin el .w3d, asi que
//  al GUARDAR el proyecto las paletas se BAKEAN en cada .w3dui (el campo
//  "paletas" que ese formato siempre tuvo). Al cargarlas se ADOPTAN aca
//  (merge por nombre, la primera gana) y la resolucion es identica en los
//  dos lados.
//
//  INVARIANTES (reglas del dueno, aplican a nivel proyecto):
//   1) TODAS las paletas tienen LA MISMA cantidad de colores; agregar un
//      color lo agrega (mismo nombre y valor inicial) a TODAS.
//   2) Borrar un color borra el MISMO indice en TODAS y corrige TODAS las
//      referencias del proyecto: las mayores se corren -1; las que apuntaban
//      al borrado pasan a color PROPIO con el valor efectivo horneado.
//   3) Si se borran todos los colores, se deselecciona (todo cae a color
//      propio/default).
//
//  C++03 puro. Comentarios sin acentos.
// ============================================================================

// las paletas del proyecto (max 8 paletas de 32 colores; los vectores de
// colores llevan reserve -> los punteros a sus rgba son estables)
std::vector<Palette>& W3dPalettes();
void W3dPalettesClear();                          // al cerrar/abrir proyecto
Palette* W3dPaletteByName(const std::string& n);  // NULL si no existe

// merge POR NOMBRE (la primera gana): paletas que llegan de un .w3dui cargado
// o de la raiz del .w3d. Despues normaliza el invariante 1 (todas del mismo
// largo: a las cortas se les completan los colores que faltan).
void W3dPalettesAdopt(const std::vector<Palette>& ps);

// paleta EFECTIVA de un objeto: sube por Parent hasta el primero con paleta
// PROPIA (Object::paleta, por nombre); RETROCOMPAT: una raiz UI sin seleccion
// cae a su activePalette de siempre. NULL = ninguna (colores propios).
Palette* W3dEffectivePalette(Object* o);
std::vector<PaletteColor>* W3dEffectiveColors(Object* o);   // NULL si no hay

// ---- INVARIANTES ----
// agrega el color a TODAS las paletas; devuelve el indice nuevo (-1 si no se pudo)
int  W3dPaletteAddColor(const std::string& name, const float* rgba);
// nombres UNICOS de paleta y de color de paleta (LA regla comun, base/W3dNames.h).
// 'excepto' = indice que puede conservar el suyo (-1 = ninguno).
std::string W3dPaletteFreeName(const std::string& base, int except);
std::string W3dPaletteFreeNameColor(const std::string& base, int except);
// borra el indice en TODAS + corrige las referencias del PROYECTO ENTERO
void W3dPaletteDeleteColor(int idx);
// crea una paleta (copia de 'base' si existe: mismos largos); indice o -1
int  W3dNewPalette(const std::string& name, int baseIdx);
// borra una paleta; los objetos que la elegian vuelven a HEREDAR
void W3dPaletteDeletePalette(int idx);
// renombra y actualiza las selecciones por nombre de todo el proyecto.
// No pisa un nombre ya tomado (los nombres son la clave de la herencia).
void W3dPaletteRename(int idx, const std::string& newValue);

#endif // W3D_PALETTES_H
