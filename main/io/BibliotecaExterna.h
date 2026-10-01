#ifndef W3D_BIBLIOTECA_EXTERNA_H
#define W3D_BIBLIOTECA_EXTERNA_H
// ============================================================================
//  BibliotecaExterna — las LIBRERIAS EXTERNAS vinculadas al proyecto, vistas desde el
//  EDITOR: la biblioteca del outliner las EXPLORA como la propia pero de SOLO LECTURA
//  (sin renombrar, mover ni borrar). Para explorarlas se abre el .w3d con W3dZipLector
//  SIN montarlo, se lee su proyecto.json y sus registros (mallas, materiales, texturas,
//  animsets, escenas, prefabs y los archivos de sus carpetas) y se arma la lista de
//  items con su arbol de carpetas. Vincular / desvincular (con undo) y el ARRASTRE de
//  su contenido: un prefab o una escena = un PROXY; una malla, un material, una
//  textura o un animset = una referencia al recurso de la libreria (W3dLibreriaIdGlobal).
//
//  EL REGISTRO (quien esta vinculada, su nombre, su archivo, el proyecto.json) y el
//  MONTAJE real viven en io/Librerias.h (los usa tambien el juego compilado):
//      "librerias": [{"nombre": "Personajes", "archivo": "ext:../personajes.w3d"}]
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include <string>
#include <vector>
#include "io/RecursosProyecto.h"

struct JVal;

int         W3dLibreriasCantidad();
std::string W3dLibreriaRuta(int i);          // la ruta de disco (resuelta)
std::string W3dLibreriaNombre(int i);        // su nombre (el PREFIJO de sus recursos; al vincularla, el del archivo)
// vincula otro .w3d (false + motivo en ingles si no es un contenedor o ya estaba). Con undo.
bool W3dLibreriaVincular(const std::string& ruta, std::string* motivo);
// la desvincula (con undo). false = no existe. Lo que el PROYECTO usaba de ella no queda colgando: cada malla de la raiz
// activa que usa una malla suya pasa a una COPIA propia del proyecto (New Copy, en el mismo Ctrl+Z; '*copias' = cuantas).
// Lo demas que el proyecto nombre de ella (en otras raices, un animset, un material) se conserva como REFERENCIA: se
// guarda igual y se resuelve al vincularla otra vez (io/Librerias.h)
bool W3dLibreriaDesvincular(int i, int* copias = NULL);
// cuantas mallas del PROYECTO (la raiz activa, sin lo que genera una instancia o un proxy) usan una malla de la libreria
// 'i': las que desvincularla copia al proyecto (el editor lo avisa antes)
int  W3dLibreriaUsosProyecto(int i);
// el CONTENIDO de la libreria (se lee una vez y se cachea; W3dLibreriasInvalidar lo olvida): sus items
// (todos soloLectura, con su tipo) y sus carpetas creadas. false + error si no se pudo leer.
bool W3dLibreriaListar(int i, std::vector<W3dRecursoItem>& items, std::vector<std::string>& carpetas,
                       std::string* error);
void W3dLibreriasInvalidar();
// la firma del registro (lo no guardado: CambiosProyecto)
std::string W3dLibreriasFirma();

// proyecto.json (lo lee el lector del proyecto: W3dLibsLeerJson de io/Librerias.h)
void W3dLibreriasGuardarJson(std::string& s);   // "librerias": [...] con coma final (nada si no hay)
void W3dLibreriasCerrarProyecto();
// el ID con el que un item de la libreria 'i' (de W3dLibreriaListar: su tipo de vista y su id) se SUELTA en el 3D o
// en Properties: la malla / el material / el animset con el prefijo de la libreria ("Personajes/Cuerpo"), la
// textura por su ruta ("lib:Personajes/texturas/piel.png"), un prefab o una escena por su clave de proxy
// (W3dLibsClave). "" = eso no se puede arrastrar (un sonido, un script, un clip suelto)
std::string W3dLibreriaIdGlobal(int i, int tipo, const std::string& id);

#endif
