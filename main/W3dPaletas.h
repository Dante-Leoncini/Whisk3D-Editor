#ifndef W3DPALETAS_H
#define W3DPALETAS_H
#include <string>
#include <vector>
#include "objects/UI.h"   // Paleta / PaletaColor (el mismo esquema de siempre)

class Object;

// ============================================================================
//  W3dPaletas — las PALETAS DE COLORES a nivel PROYECTO, COMPARTIDAS por el
//  editor y el runtime compilado (la implementacion vive en W3dEscena.cpp,
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
std::vector<Paleta>& W3dPaletas();
void W3dPaletasLimpiar();                          // al cerrar/abrir proyecto
Paleta* W3dPaletaPorNombre(const std::string& n);  // NULL si no existe

// merge POR NOMBRE (la primera gana): paletas que llegan de un .w3dui cargado
// o de la raiz del .w3d. Despues normaliza el invariante 1 (todas del mismo
// largo: a las cortas se les completan los colores que faltan).
void W3dPaletasAdoptar(const std::vector<Paleta>& ps);

// paleta EFECTIVA de un objeto: sube por Parent hasta el primero con paleta
// PROPIA (Object::paleta, por nombre); RETROCOMPAT: una raiz UI sin seleccion
// cae a su paletaActiva de siempre. NULL = ninguna (colores propios).
Paleta* W3dPaletaEfectiva(Object* o);
std::vector<PaletaColor>* W3dColoresEfectivos(Object* o);   // NULL si no hay

// ---- INVARIANTES ----
// agrega el color a TODAS las paletas; devuelve el indice nuevo (-1 si no se pudo)
int  W3dPaletaAgregarColor(const std::string& nombre, const float* rgba);
// nombres UNICOS de paleta y de color de paleta (LA regla comun, base/W3dNombres.h).
// 'excepto' = indice que puede conservar el suyo (-1 = ninguno).
std::string W3dPaletaNombreLibre(const std::string& base, int excepto);
std::string W3dPaletaColorNombreLibre(const std::string& base, int excepto);
// borra el indice en TODAS + corrige las referencias del PROYECTO ENTERO
void W3dPaletaBorrarColor(int idx);
// crea una paleta (copia de 'base' si existe: mismos largos); indice o -1
int  W3dPaletaNueva(const std::string& nombre, int baseIdx);
// borra una paleta; los objetos que la elegian vuelven a HEREDAR
void W3dPaletaBorrarPaleta(int idx);
// renombra y actualiza las selecciones por nombre de todo el proyecto.
// No pisa un nombre ya tomado (los nombres son la clave de la herencia).
void W3dPaletaRenombrar(int idx, const std::string& nuevo);

#endif // W3DPALETAS_H
