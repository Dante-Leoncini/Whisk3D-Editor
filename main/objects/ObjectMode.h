#ifndef OBJECTMODE_H
#define OBJECTMODE_H

#include "objects/Objects.h"
#include "variables.h" // REAL, portable
#include "animation/Animation.h" // AnimProperty/curvas: Symbian-safe (GLES propio, sin SDL)
#ifndef W3D_SYMBIAN
// Instance arrastra SDL/GL de escritorio
#include "Instance.h"
#endif

class Mesh; // MoverSeleccionEditLocal opera sobre una malla en Edit Mode

void ReestablecerEstado(bool ClearEstado = true);
void Cancelar();
void EliminarAnimaciones(Object& obj);
// animacion de OBJETOS (transform pos/rot/escala; menu Object > Animation)
// Insert Keyframe: guarda el transform de los seleccionados en el frame actual. 'canales' = mascara
// KfCanalLoc/Rot/Scl (Animation.h) del menu desplegable; 0/omitido = los tres (como siempre).
void InsertarKeyframeObjeto(int canales = 0);
// AUTO KEY: al confirmar un transform guarda SOLO los canales que cambiaron (si solo rotaste en X, solo X Euler
// Rotation). Se mide contra estadoObjetos (el snapshot del transform) -> hay que llamarla ANTES de limpiarlo.
// Devuelve cuantos canales guardo. El boton del timeline prende/apaga AutoKeyOn.
extern bool AutoKeyOn;
int AutoKeyObjetos();

// MOTION TRAIL: el camino (SOLO POSICION) que recorre el origen del objeto, muestreado frame a frame entre su
// primer y ultimo keyframe de posicion. 'keys' = los frames que tienen keyframe (para marcarlos).
// false = el objeto no tiene curvas de posicion.
bool MotionTrailDe(Object* o, std::vector<Vector3>& pts, std::vector<int>& keys, int& desde, int& hasta);
extern bool MotionTrailOn;   // el checkbox del menu Animation del viewport 3D
void BorrarKeyframeObjeto();   // Delete Keyframe: saca el keyframe del frame actual
void LimpiarKeyframeObjeto();  // Clear Keyframe: borra toda la animacion del objeto
void AplicarAnimacionObjetos();// PLAYBACK: aplica los keyframes al transform en el frame actual (al cambiar de frame)
// aplica las curvas de UN objeto en EL FRAME QUE SE LE PIDA (transform + visible + render + fov/clip de
// camara + color/atenuacion/spot de luz). Es lo mismo que hace el playback, pero sin depender de donde
// este el playhead: lo usa el GUARDADO para escribir el objeto en su CUADRO BASE y no en la pose del
// scrub (ver ReposoAnimObjetos en io/GuardarW3D.cpp).
void W3dAplicarCurvasEnFrame(Object* o, std::vector<AnimProperty>& props, int frame);
// estado del keyframe de un CANAL del objeto en 'frame': 0 sin animacion / 1 animado
// pero sin key en este frame / 2 hay key en este frame. Lo usa el rombo del panel.
int  AnimCanalEstado(class Object* o, int prop, int comp, int frame);
// pone o QUITA (toggle) un key del canal en 'frame', con el valor actual del objeto
void AnimCanalToggle(class Object* o, int prop, int comp, int frame);
void Eliminar(bool IncluirCollecciones = false);

// ===== NOMBRES UNICOS de OBJETO (ver el bloque grande al final de ObjectMode.cpp) =====
// LA puerta del rename de objetos: uniquifica en el scope del objeto (su escena UI o
// toda la escena) y ARRASTRA los vinculos por nombre -refs de scripts lua, targetName,
// RielName, escenaInicial- en UN SOLO paso de undo. Devuelve el nombre que quedo (puede
// diferir del pedido). 'avisar' = toast cuando cambia respecto de lo tipeado.
#include <string>
std::string W3dRenombrarObjeto(Object* o, const std::string& pedido, bool avisar = true);
// cuantas refs de script de la escena de 'o' nombran a 'viejo' (para tests/diagnostico)
int W3dNombresContarRefs(Object* o, const std::string& viejo);
// REPARA los duplicados de un proyecto ya guardado (se llama al terminar de abrir un
// .w3d o de mergear un .w3dui) en TODOS los espacios de nombres: objetos, materiales,
// vertex/uv groups, uv maps, color layers, mesh parts, modificadores, huesos 3D/2D,
// armatures 2D, clips 3D/2D, paletas, colores de paleta y animaciones de escena.
// El PRIMERO de cada espacio (en el orden en que lo busca el motor) CONSERVA el nombre
// pelado y NO se arrastra ningun vinculo: todo lo que ya resolvia por nombre sigue
// resolviendo al MISMO sitio. AVISA de forma visible. Devuelve cuantos renombro.
int W3dNombresRepararEscena(bool avisar = true);

// ---- LA lista de espacios de nombres del proyecto, SALVO los objetos ----
// (los objetos van por scope de escena y los maneja Object::NombreLibre / RepararRec).
// 'nombres' viene en el ORDEN DE BUSQUEDA del espacio: el primero es el que gana.
// La usan la reparacion de carga y el undo; si aparece un espacio nuevo se agrega ahi.
#include <vector>
struct W3dEspacioNombres {
    const char* etiqueta;                // "MATERIAL", "VERTEX GROUP", ... (para el log)
    const char* porDefecto;              // nombre a usar si el del archivo esta vacio
    void*       dueno;                   // Mesh* / Armature* / NULL (global de proyecto)
    std::vector<std::string*> nombres;   // TODAS las puntas del espacio
    W3dEspacioNombres() : etiqueta(""), porDefecto("Nombre"), dueno(0) {}
};
void W3dNombresJuntarEspacios(std::vector<W3dEspacioNombres>& out);
// nombre LIBRE para el std::string* 'destino' en SU espacio de nombres (lo ubica por
// IDENTIDAD DE PUNTERO). Si 'destino' no pertenece a ningun espacio (una ref de lua, un
// targetName, un RielName) devuelve 'pedido' tal cual. Lo usa el UNDO al RESTAURAR un
// nombre: mientras el paso estaba en el stack, ese nombre se lo pudo tomar otro.
// 'etiquetaOut' (opcional) sale con el nombre del espacio, o NULL si no es un nombre.
std::string W3dNombreLibrePara(std::string* destino, const std::string& pedido,
                               const char** etiquetaOut = 0);
// el std::string* sigue siendo un nombre MIEMBRO de un objeto vivo? Lo busca POR IDENTIDAD en
// el arbol de la escena: Object::name o una punta de vinculo cacheada (targetName / RielName).
// false = el objeto ya no esta (borrado/liberado) -> el paso de undo que apunte ahi tiene que
// ser un NO-OP, nunca un write a memoria liberada. Es LA validacion del destino 'Directo' del
// undo de renames; los elementos de una lista NO van por puntero (ver W3dRenameDest en Undo.h).
bool W3dNombrePunteroVivo(std::string* p);
void CalcObjectsTransformPivotPoint(Object* obj);
void SetTransformPivotPoint();
// centro geometrico de la seleccion (para el FOCO '.', distinto del pivote)
Vector3 CentroFocoSeleccion();
// transform global (TRS) por la cadena de padres: world<->local de PUNTOS. Lo usa
// el transform de sub-elementos de malla (editor) sin re-implementar la cadena.
// (RotGlobalDe se declara en el Core, objects/Objects.h)
Vector3 ScaleGlobalDe(Object* o);
// reposiciona los seleccionados alrededor del pivote tras rotar/escalar (objetos)
void AplicarPivotATransform();
void GuardarMousePos();
void guardarEstadoRec(Object* obj);
bool guardarEstado();
void SetPosicion();
void DuplicatedObject();
// copia REAL de UN objeto (mesh deep-copy; luz/camara/empty/texto2d/imagen2d sus propiedades).
// La usan DuplicatedObject y el duplicado del Editor 2D.
Object* W3dDuplicarUno(Object* src);
// Separate (Edit Mode: P / menu Mesh > Separate): mueve las caras SELECCIONADAS a un mesh NUEVO (misma
// transform + materiales + vertex groups + modificadores) y las borra del actual. true si separo algo.
bool SepararSeleccionEdit(Mesh* m);
void NewInstance();
// Join (Ctrl+J, menu Object): une las mallas seleccionadas dentro del objeto ACTIVO (conserva su transform).
void JoinObjetos();
// Apply (Alt+A, menu Object > Apply): hornea el transform en la malla. what: 0=Location 1=Rotation 2=Scale 3=All.
void AplicarTransform(int what);
// Set Active Object as Camera (Ctrl+Numpad 0): el activo pasa a ser la camara activa. false si NO es una camara.
bool SetActiveObjectAsCamera();
// ctrl+p: emparenta los seleccionados al objeto ACTIVO (conservando la
// posicion global). alt+p: los devuelve a la raiz.
void SetParentSeleccion();
// reparenta conservando lo local (puede saltar) / manteniendo lo global
void ReparentSimple(Object* obj, Object* nuevoPadre);
void ReparentKeepTransform(Object* obj, Object* nuevoPadre);
// reordena junto a otro objeto (drag del outliner)
void MoverJuntoA(Object* obj, Object* ref, bool despues);
void ClearParentSeleccion();
// REPARENTAR ES DESHACIBLE (ronda 14): cada una de esas cuatro puertas deja SU paso de undo
// (ver el bloque del embudo en ObjectMode.cpp). El que mueve VARIOS objetos en una sola
// accion del usuario -los menus Set/Clear Parent, el emparentado del Editor 2D- envuelve su
// bucle con estas dos, y los N pasos se funden en UN Ctrl+Z. Anidan.
void ReparentGrupoIniciar();
void ReparentGrupoFin();
void SetRotacion(int dx, int dy);
void SetRotacion();
// reconstruye el quaternion REAL del objeto activo desde los valores de display
// editados, segun rotMode (euler XYZ / quaternion / axis-angle). onChange.
void SincronizarRotacionActiva();
// rotacion ORBITAL/gimbal (libre): izq/der=camUp, arr/ab=camRight (quaterniones)
void RotarOrbital(int dx, int dy);
// alterna rotacion libre entre trackball (eje de vista) y orbital (V / 0)
void ToggleRotacionOrbital();
// cicla eje/orientacion (X/Y/Z=0/1/2) durante un transform: Global->Local->View->libre
void CiclarEjeTransform(int eje);
// Shift+eje: constriñe a un PLANO (excluye 'eje', mueve en los otros dos)
void CiclarPlanoTransform(int eje);
// eje (X/Y/Z) EN WORLD segun la orientacion actual (global/local/view).
// Lo usa el render para dibujar las guias con la orientacion correcta.
Vector3 EjeOrientado(Object& obj, int a);
void SetScale(int dx, int dy, float factor = 0.01f);
void SetEscala();
void SetTranslacionObjetos(int dx, int dy, float factor = 1.0f);
// entrada numerica: aplica un valor EXACTO al transform de objetos en curso
void SetTransformNumerico(float v);

// EDIT MODE: traslada RIGIDO los verts seleccionados por 'deltaLocal' (coords LOCALES del mesh) + persiste
// (empuja al render, rebordes, normales, preview de modificadores, undo). Lo usan el snap y los campos X/Y/Z
// de posicion del panel de Vertices.
void MoverSeleccionEditLocal(Mesh* m, const Vector3& deltaLocal);

// snap (menu shift+s): mueve la seleccion o el cursor 3D estilo Blender
void SnapSeleccionAlCursor(bool mantenerOffset);
void SnapSeleccionAlActivo();
void SnapSeleccionAlGrid();
void SnapCursorAlGrid();
void SnapCursorAlOrigen();
void SnapCursorALoSeleccionado();
void SnapCursorAlActivo();
// Set Origin (submenu Object): mueve el origen y/o la geometria de las mallas sel
void SetOriginGeometryToOrigin(); // baricentro -> origen (no mueve el objeto)
void SetOriginOriginToGeometry(); // origen -> baricentro (la geometria queda igual)
void SetOriginToCursor();         // origen -> cursor 3D (la geometria queda igual)

// ============================================================================
//  OLVIDAR EL ARCHIVO DE ORIGEN (menu Object > "Clear Original File")
//
//  Mesh::origen es el .obj/.fbx del que se importo la malla. Es SOLO PROCEDENCIA
//  (para "reimportar del original"): la geometria viaja HORNEADA adentro del .w3d,
//  en su .w3dm. Pero el guardado lo marca como REFERENCIA EXTERNA, asi que si el
//  usuario mueve o borra ese .obj -bajarlo a Descargas, importarlo y limpiar la
//  carpeta es EL flujo tipico- cada guardado tiraba "Guardado, pero 1 archivo(s)
//  externo(s) NO estan" y cada apertura su warning, PARA SIEMPRE y sin nada en la
//  UI que lo mostrara ni lo borrara. Un aviso correcto que no se puede apagar es
//  ruido, y el ruido irreversible entrena a ignorar los avisos que si importan.
//  Esta es la salida: olvidar la procedencia (la geometria no se toca).
//  Devuelve cuantas mallas quedaron sin origen.
int OlvidarOrigenSeleccionadas();

#endif