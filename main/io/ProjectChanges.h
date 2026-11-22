#ifndef PROJECT_CHANGES_H
#define PROJECT_CHANGES_H
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
//  lista (ChangesPopup: "Se perderan los cambios en:").
//
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include <string>
#include <vector>

// una cosa sin guardar, para la lista del cartel: el icono, el tipo y el nombre (ya en el idioma de la UI)
struct W3dChange {
    int icon;
    std::string type, name;
    W3dChange() : icon(-1) {}
};

// "todo esta guardado": la foto (despues de abrir, de crear un proyecto nuevo y de cada guardado)
void W3dChangesSnapshot();
// TODO lo sucio, para el cartel (el orden: el proyecto, las raices, los recursos, las carpetas)
void W3dChangesList(std::vector<W3dChange>& out);
bool W3dChangesHas();
// lo mismo, recalculado a lo sumo cada medio segundo (el titulo de la ventana, el '*' del nombre del proyecto
// en la tarjeta Archivo: se preguntan en cada cuadro)
bool W3dChangesHasCache();
// el '*' de una fila de la biblioteca
bool W3dChangesResourceDirty(int type, const std::string& id);
// el '*' de una raiz (su fila del registro W3dRoots)
bool W3dChangesRootDirty(int idx);
// algo se EDITO en la raiz activa (lo llama el undo: cada paso que se apila, cada Ctrl+Z / Ctrl+Y)
void W3dTouchedChanges();
// la version del estado de "sucio" (sube con cada Tocado y cada foto): quien dibuja el '*' la compara
unsigned W3dChangesVersion();
// cuantas EDICIONES de escena hubo en la sesion (cada paso de undo, deshacer y rehacer): la firma de las
// miniaturas de escenas y prefabs (se rehacen cuando algo cambio)
unsigned W3dChangesEdits();

// ---- EL CARTEL AL CERRAR / ABRIR OTRO PROYECTO ----
// que se queria hacer cuando aparecio el cartel
enum { W3D_CHANGES_EXIT = 0, W3D_CHANGES_OPEN, W3D_CHANGES_NEW, W3D_CHANGES_CLOSE_SCENE };
// si hay algo sin guardar abre el cartel y devuelve true (la accion queda PENDIENTE: el cartel la hace si
// el usuario elige "Guardar y ..." o "... sin guardar"); si no hay nada, false y el que llama sigue. En el
// harness (--script) nunca abre nada (devuelve false). 'ruta' = el proyecto a abrir (ABRIR).
bool W3dChangesAsk(int action, const std::string& path);
// CERRAR UNA ESCENA ABIERTA en el editor (destildarla, io/EditorRoots.h) que tiene cambios sin guardar: abre el cartel
// (guardar y cerrar / cerrar sin guardar / cancelar) y devuelve true; la cierra W3dChangesCloseSceneHook si el
// usuario sigue. false = no hay nada sin guardar en ESA escena (o el harness): el que llama la cierra
bool W3dChangesAskCloseScene(const std::string& name);
extern void (*W3dChangesCloseSceneHook)(const std::string& name);
class Object;
// una raiz que se cierra y se libera (sus cambios se guardaron o se descartaron): deja de contar como editada
void W3dChangesForgetRoot(const Object* root);
// lo elegido en el cartel: 0 = guardar y seguir, 1 = seguir sin guardar, 2 = cancelar. Lo llama el cartel.
void W3dChangesRespond(int response);
// hay un cartel pendiente (el harness lo consulta)
bool W3dChangesDialogOpen();
// los ganchos que pone la app (main.cpp): salir de verdad, abrir un proyecto, crear uno nuevo, guardar
extern void (*W3dChangesExitHook)();
extern void (*W3dChangesOpenHook)(const std::string& path);
extern void (*W3dChangesNewHook)();
extern bool (*W3dChangesSaveHook)();   // false = no se guardo (no sigue)
// modo harness: el cartel no se muestra nunca (el comando 'cambios' lo consulta y lo prueba)
extern bool g_w3dChangesNoDialog;
// el cartel "Se perderan los cambios en:" esta APAGADO en el producto (pedido del dueno); el harness lo prende para probarlo
extern bool g_w3dChangesDialogActive;

#endif
