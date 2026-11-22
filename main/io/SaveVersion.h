#ifndef SAVE_VERSION_H
#define SAVE_VERSION_H
#include <set>
#include <string>

// ============================================================================
//  GuardarVersion — GUARDADO POR VERSIONES del proyecto .w3d.
//
//  UNA VERSION ES UNA COPIA DEL ARCHIVO, AGRUPADA EN versions/:
//
//      miProyecto.w3d
//      versions/miProyecto_v01.w3d
//      versions/miProyecto_v02.w3d
//
//  Con el formato v4 el .w3d es un CONTENEDOR (un zip con el proyecto entero
//  adentro: escenas, scripts, texturas, icono), asi que copiar el archivo ES
//  copiar la version completa.
//
//  CAMBIO DELIBERADO de conducta (pedido del dueno, textual: "y si quiero que
//  hagas la carpeta versiones y adentro metas el test1_v01.w3d"): antes los
//  _vNN.w3d quedaban SUELTOS al lado del proyecto. NO es la vuelta a la carpeta
//  versions/vN/ con assets.zip adentro, que el dueno rechazo: cada version
//  sigue siendo UN archivo autocontenido con su sufijo _vNN, lo unico que cambio
//  es la carpeta donde vive (y de donde sale la numeracion).
//
//  Modelo SIMPLE y SECUENCIAL: primero la v1, despues la v2 y asi. El boton
//  "Guardar version vN" de la tarjeta Archivo hace:
//    1) Guardado NORMAL de siempre (la ubicacion actual queda con lo ultimo)
//    2) Copia ese .w3d a versions/<proyecto>_vNN.w3d
//
//  N = el numero del UNICO archivo que el boton VA A CREAR (el label nunca
//  miente): 1 si no hay ninguna version, sino max(vNN) + 1. Se calcula
//  escaneando versions/, asi que borrar una version vieja no rompe la cuenta.
//  Un click = UN archivo nuevo. Si la copia falla se avisa por notificacion SIN
//  romper el guardado normal.
//
//  RESTAURAR una version = ABRIR SU ARCHIVO. No hay que descomprimir nada.
//  Lo unico que puede faltar son las referencias "ext:" (las que el usuario
//  dejo afuera a proposito): estan listadas en el EXTERNALS.txt de adentro.
// ============================================================================

// el numero de version que crearia el boton AHORA (ver la regla de arriba)
int SaveNextVersionN();

// el texto del boton de la tarjeta Archivo: "Guardar version vN" con el N real.
// Se recalcula al abrir un proyecto y tras cada guardado de version.
std::string SaveVersionLabel();

// el flujo completo del boton (guardar normal + snapshot a versiones/vN).
// false si no hay proyecto con ubicacion o si fallo el guardado normal.
bool SaveVersionRun();

// COLECTOR de referencias, reusable (lo usa tambien CompileGame): parsea el
// JSON en 'filePath' (un .w3d o un .w3dui ya escrito en disco) y junta en
// 'refs' las rutas RELATIVAS AL PROYECTO de todo lo que referencia; cada
// .w3dui nuevo que aparece se parsea en cadena (sus refs son relativas a SU
// carpeta, con fallback a la raiz del proyecto, como al abrir). 'baseRel' es
// la carpeta (relativa al proyecto, "" = raiz) contra la que resuelven las
// referencias de ESTE archivo. Solo entra lo que existe bajo 'dirProject'.
void SaveVersionCollectFrom(const std::string& filePath,
                              const std::string& baseRel,
                              const std::string& dirProject,
                              std::set<std::string>* refs);

#endif // SAVE_VERSION_H
