#ifndef W3D_CAMBIOS_PROYECTO_H
#define W3D_CAMBIOS_PROYECTO_H
// ============================================================================
//  CambiosProyecto — lo NO GUARDADO del proyecto, POR RECURSO y POR RAIZ.
//
//  Al abrir, al crear uno nuevo y al guardar se toma una FOTO de lo que hay (cada
//  recurso de la biblioteca con su identidad, su nombre y su FIRMA de contenido; la
//  firma de las carpetas, de las librerias vinculadas y de la configuracion del
//  proyecto). Lo que difiere de la foto esta SUCIO:
//    - un recurso NUEVO (no estaba), RENOMBRADO, BORRADO (estaba y ya no), o con otro
//      contenido: malla editada (su version), material cambiado (sus campos), textura
//      pintada / generada / importada (bytes en memoria o en el overlay del contenedor),
//      sonido o script cambiado (el overlay: el IDE guardo), animset/clip editado;
//    - una RAIZ (escena, juego, prefab) nueva, renombrada o MODIFICADA: cada paso de
//      undo que se apila (y cada Ctrl+Z / Ctrl+Y) mientras esta activa la marca (sus
//      objetos, transforms, propiedades: todo lo que se edita pasa por el undo);
//    - las carpetas de la biblioteca, las librerias vinculadas, la configuracion.
//  La UI pone un '*' al lado de todo lo sucio (filas de la biblioteca, el selector de
//  raiz, el selector de malla de Properties, el nombre del proyecto, el titulo de la
//  ventana) y, al CERRAR Whisk3D o abrir/crear otro proyecto con algo sucio, muestra la
//  lista (CambiosPopup: "Se perderan los cambios en:").
//
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include <string>
#include <vector>

// una cosa sin guardar, para la lista del cartel: el icono, el tipo y el nombre (ya en el idioma de la UI)
struct W3dCambio {
    int icono;
    std::string tipo, nombre;
    W3dCambio() : icono(-1) {}
};

// "todo esta guardado": la foto (despues de abrir, de crear un proyecto nuevo y de cada guardado)
void W3dCambiosFoto();
// TODO lo sucio, para el cartel (el orden: el proyecto, las raices, los recursos, las carpetas)
void W3dCambiosListar(std::vector<W3dCambio>& out);
bool W3dCambiosHay();
// lo mismo, recalculado a lo sumo cada medio segundo (el titulo de la ventana, el '*' del nombre del proyecto
// en la tarjeta Archivo: se preguntan en cada cuadro)
bool W3dCambiosHayCache();
// el '*' de una fila de la biblioteca
bool W3dCambiosRecursoSucio(int tipo, const std::string& id);
// el '*' de una raiz (su fila del registro W3dRaices)
bool W3dCambiosRaizSucia(int idx);
// algo se EDITO en la raiz activa (lo llama el undo: cada paso que se apila, cada Ctrl+Z / Ctrl+Y)
void W3dCambiosTocado();
// la version del estado de "sucio" (sube con cada Tocado y cada foto): quien dibuja el '*' la compara
unsigned W3dCambiosVersion();
// cuantas EDICIONES de escena hubo en la sesion (cada paso de undo, deshacer y rehacer): la firma de las
// miniaturas de escenas y prefabs (se rehacen cuando algo cambio)
unsigned W3dCambiosEdiciones();

// ---- EL CARTEL AL CERRAR / ABRIR OTRO PROYECTO ----
// que se queria hacer cuando aparecio el cartel
enum { W3D_CAMBIOS_SALIR = 0, W3D_CAMBIOS_ABRIR, W3D_CAMBIOS_NUEVO };
// si hay algo sin guardar abre el cartel y devuelve true (la accion queda PENDIENTE: el cartel la hace si
// el usuario elige "Guardar y ..." o "... sin guardar"); si no hay nada, false y el que llama sigue. En el
// harness (--script) nunca abre nada (devuelve false). 'ruta' = el proyecto a abrir (ABRIR).
bool W3dCambiosPreguntar(int accion, const std::string& ruta);
// lo elegido en el cartel: 0 = guardar y seguir, 1 = seguir sin guardar, 2 = cancelar. Lo llama el cartel.
void W3dCambiosResponder(int respuesta);
// hay un cartel pendiente (el harness lo consulta)
bool W3dCambiosCartelAbierto();
// los ganchos que pone la app (main.cpp): salir de verdad, abrir un proyecto, crear uno nuevo, guardar
extern void (*W3dCambiosSalirHook)();
extern void (*W3dCambiosAbrirHook)(const std::string& ruta);
extern void (*W3dCambiosNuevoHook)();
extern bool (*W3dCambiosGuardarHook)();   // false = no se guardo (no sigue)
// modo harness: el cartel no se muestra nunca (el comando 'cambios' lo consulta y lo prueba)
extern bool g_w3dCambiosSinCartel;

#endif
