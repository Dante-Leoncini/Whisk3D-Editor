#ifndef EXTERNAL_LIBRARY_H
#define EXTERNAL_LIBRARY_H
// ============================================================================
//  BibliotecaExterna — las LIBRERIAS EXTERNAS vinculadas al proyecto, vistas desde el
//  EDITOR: la biblioteca del outliner las EXPLORA como la propia pero de SOLO LECTURA
//  (sin renombrar, mover ni borrar). Para explorarlas se abre el .w3d con W3dZipReader
//  SIN montarlo, se lee su project.json y sus registros (mallas, materiales, texturas,
//  animsets, escenas, prefabs y los archivos de sus carpetas) y se arma la lista de
//  items con su arbol de carpetas. Vincular / desvincular (con undo) y el ARRASTRE de
//  su contenido: un prefab o una escena = un PROXY; una malla, un material, una
//  textura o un animset = una referencia al recurso de la libreria (W3dExtLibGlobalId).
//
//  EL REGISTRO (quien esta vinculada, su nombre, su archivo, el project.json) y el
//  MONTAJE real viven en io/Libraries.h (los usa tambien el juego compilado):
//      "librerias": [{"nombre": "Personajes", "archivo": "ext:../personajes.w3d"}]
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include <string>
#include <vector>
#include "io/ProjectResources.h"
#include "io/Libraries.h"

struct JVal;

// vincula otro .w3d (false + motivo en ingles si no es un contenedor o ya estaba). Con undo.
bool W3dExtLibLink(const std::string& path, std::string* reason);
// la desvincula (con undo). false = no existe. Lo que el PROYECTO usaba de ella no queda colgando: cada malla de la raiz
// activa que usa una malla suya pasa a una COPIA propia del proyecto (New Copy, en el mismo Ctrl+Z; '*copias' = cuantas).
// Lo demas que el proyecto nombre de ella (en otras raices, un animset, un material) se conserva como REFERENCIA: se
// guarda igual y se resuelve al vincularla otra vez (io/Libraries.h)
bool W3dExtLibUnlink(int i, int* copies = NULL);
// cuantas mallas del PROYECTO (la raiz activa, sin lo que genera una instancia o un proxy) usan una malla de la libreria
// 'i': las que desvincularla copia al proyecto (el editor lo avisa antes)
int  W3dExtLibProjectUses(int i);
// el CONTENIDO de la libreria (se lee una vez y se cachea; W3dExtLibsInvalidate lo olvida): sus items
// (todos readOnly, con su tipo) y sus carpetas creadas. false + error si no se pudo leer.
bool W3dExtLibList(int i, std::vector<W3dResourceItem>& items, std::vector<std::string>& folders,
                       std::string* error);
void W3dExtLibsInvalidate();
// la firma del registro (lo no guardado: CambiosProyecto)
std::string W3dExtLibsSignature();

// project.json (lo lee el lector del proyecto: W3dLibsReadJson de io/Libraries.h)
void W3dExtLibsCloseProject();
// el ID con el que un item de la libreria 'i' (de W3dExtLibList: su tipo de vista y su id) se SUELTA en el 3D o
// en Properties: la malla / el material / el animset con el prefijo de la libreria ("Personajes/Cuerpo"), la
// textura por su ruta ("lib:Personajes/textures/piel.png"), un prefab o una escena por su clave de proxy
// (W3dLibsKey). "" = eso no se puede arrastrar (un sonido, un script, un clip suelto)
std::string W3dExtLibGlobalId(int i, int type, const std::string& id);

#endif
