#ifndef W3D_SCRIPT_H
#define W3D_SCRIPT_H
#include <string>

// ============================================================================
//  W3dScript — interprete de scripts de comandos del editor para TESTS.
//  Ejecuta cada comando llamando a las funciones COMPARTIDAS (NewMesh,
//  LayoutExtrudeFaces, EditXform*, ExportOBJ...) -> NO necesita GUI/SDL ni
//  simular mouse/teclado. Cada comando es una operacion discreta y replayable:
//  la MISMA base sirve para tests, historial y un futuro undo (Ctrl+Z) que
//  tome un snapshot antes de cada comando.
//
//  En PC se invoca con:  whisk3d --script <ruta>   (sale 0 si todo OK, 1 si fallo)
// ============================================================================

// Corre un script: cada linea es un comando. Loguea OK/FALLO por linea y CORTA
// al primer fallo. Devuelve true solo si TODOS los comandos dieron OK.
bool W3dRunScript(const std::string& path);

// Ejecuta UN comando (una linea). err = motivo si falla. Reusable por el runner,
// y a futuro por el historial / la consola.
bool W3dRunCommand(const std::string& linea, std::string& err);

#endif
