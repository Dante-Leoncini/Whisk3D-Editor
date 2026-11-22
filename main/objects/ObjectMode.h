#ifndef OBJECTMODE_H
#define OBJECTMODE_H

#include "objects/Objects.h"
#include "variables.h" // REAL, portable
#include "animation/Animation.h" // AnimProperty/curvas: Symbian-safe (GLES propio, sin SDL)
#ifndef W3D_SYMBIAN
// Instance arrastra SDL/GL de escritorio
#include "Instance.h"
#endif

class Mesh; // MoveSelectionEditLocal opera sobre una malla en Edit Mode

void RestoreState(bool ClearState = true);
void Cancel();
void RemoveAnimations(Object& obj);
// animacion de OBJETOS (transform pos/rot/escala; menu Object > Animation)
// Insert Keyframe: guarda el transform de los seleccionados en el frame actual. 'canales' = mascara
// KfChannelLoc/Rot/Scl (Animation.h) del menu desplegable; 0/omitido = los tres (como siempre).
void InsertKeyframeObject(int channels = 0);
// AUTO KEY: al confirmar un transform guarda SOLO los canales que cambiaron (si solo rotaste en X, solo X Euler
// Rotation). Se mide contra objectState (el snapshot del transform) -> hay que llamarla ANTES de limpiarlo.
// Devuelve cuantos canales guardo. El boton del timeline prende/apaga AutoKeyOn.
extern bool AutoKeyOn;
int AutoKeyObjects();

// MOTION TRAIL: el camino (SOLO POSICION) que recorre el origen del objeto, muestreado frame a frame entre su
// primer y ultimo keyframe de posicion. 'keys' = los frames que tienen keyframe (para marcarlos).
// false = el objeto no tiene curvas de posicion.
bool MotionTrailOf(Object* o, std::vector<Vector3>& pts, std::vector<int>& keys, int& from, int& until);
extern bool MotionTrailOn;   // el checkbox del menu Animation del viewport 3D
void DeleteKeyframeObject();   // Delete Keyframe: saca el keyframe del frame actual
void ClearKeyframeObject();  // Clear Keyframe: borra toda la animacion del objeto
void ApplyAnimationObjects();// PLAYBACK: aplica los keyframes al transform en el frame actual (al cambiar de frame)
// aplica las curvas de UN objeto en EL FRAME QUE SE LE PIDA (transform + visible + render + fov/clip de
// camara + color/atenuacion/spot de luz). Es lo mismo que hace el playback, pero sin depender de donde
// este el playhead: lo usa el GUARDADO para escribir el objeto en su CUADRO BASE y no en la pose del
// scrub (ver RestAnimObjects en io/SaveW3D.cpp).
void W3dApplyCurvesInFrame(Object* o, std::vector<AnimProperty>& props, int frame);
// estado del keyframe de un CANAL del objeto en 'frame': 0 sin animacion / 1 animado
// pero sin key en este frame / 2 hay key en este frame. Lo usa el rombo del panel.
int  AnimChannelState(class Object* o, int prop, int comp, int frame);
// pone o QUITA (toggle) un key del canal en 'frame', con el valor actual del objeto
void AnimChannelToggle(class Object* o, int prop, int comp, int frame);
// el MENU CONTEXTUAL de una propiedad: insertar el key (sin sacarlo si ya estaba), borrar TODOS los del canal, y copiar /
// pegar el key del frame (un color son sus 3 curvas). false = no se pudo (y se aviso por que, si hace falta)
bool AnimChannelInsertKey(class Object* o, int prop, int comp, int frame);
bool AnimChannelDeleteAll(class Object* o, int prop, int comp);
bool AnimChannelCopyKey(class Object* o, int prop, int comp, int frame, std::vector<class keyFrame>& out);
bool AnimChannelPasteKey(class Object* o, int prop, int comp, int frame, const std::vector<class keyFrame>& in);
void Remove(bool IncludeCollections = false);

// ===== NOMBRES UNICOS de OBJETO (ver el bloque grande al final de ObjectMode.cpp) =====
// LA puerta del rename de objetos: uniquifica en el scope del objeto (su escena UI o
// toda la escena) y ARRASTRA los vinculos por nombre -refs de scripts lua, targetName,
// RailName, escenaInicial- en UN SOLO paso de undo. Devuelve el nombre que quedo (puede
// diferir del pedido). 'avisar' = toast cuando cambia respecto de lo tipeado.
#include <string>
std::string W3dRenameObject(Object* o, const std::string& request, bool notify = true);
// cuantas refs de script de la escena de 'o' nombran a 'viejo' (para tests/diagnostico)
int W3dNamesCountRefs(Object* o, const std::string& old);
// REPARA los duplicados de un proyecto ya guardado (se llama al terminar de abrir un
// .w3d o de mergear un .w3dui) en TODOS los espacios de nombres: objetos, materiales,
// vertex/uv groups, uv maps, color layers, mesh parts, modificadores, huesos 3D/2D,
// armatures 2D, clips 3D/2D, paletas, colores de paleta y animaciones de escena.
// El PRIMERO de cada espacio (en el orden en que lo busca el motor) CONSERVA el nombre
// pelado y NO se arrastra ningun vinculo: todo lo que ya resolvia por nombre sigue
// resolviendo al MISMO sitio. AVISA de forma visible. Devuelve cuantos renombro.
int W3dNamesRepairScene(bool notify = true);

// ---- LA lista de espacios de nombres del proyecto, SALVO los objetos ----
// (los objetos van por scope de escena y los maneja Object::FreeName / RepairRec).
// 'nombres' viene en el ORDEN DE BUSQUEDA del espacio: el primero es el que gana.
// La usan la reparacion de carga y el undo; si aparece un espacio nuevo se agrega ahi.
#include <vector>
struct W3dNamespace {
    const char* label;                 // "MATERIAL", "VERTEX GROUP", ... (para el log)
    const char* defaultValue;          // nombre a usar si el del archivo esta vacio
    void*       owner;                 // Mesh* / Armature* / NULL (global de proyecto)
    std::vector<std::string*> names;   // TODAS las puntas del espacio
    W3dNamespace() : label(""), defaultValue("Name"), owner(0) {}
};
void W3dNamesCollectSpaces(std::vector<W3dNamespace>& out);
// nombre LIBRE para el std::string* 'destino' en SU espacio de nombres (lo ubica por
// IDENTIDAD DE PUNTERO). Si 'destino' no pertenece a ningun espacio (una ref de lua, un
// targetName, un RailName) devuelve 'pedido' tal cual. Lo usa el UNDO al RESTAURAR un
// nombre: mientras el paso estaba en el stack, ese nombre se lo pudo tomar otro.
// 'labelOut' (opcional) sale con el nombre del espacio, o NULL si no es un nombre.
std::string W3dFreeNameFor(std::string* destination, const std::string& request,
                               const char** labelOut = 0);
// el std::string* sigue siendo un nombre MIEMBRO de un objeto vivo? Lo busca POR IDENTIDAD en
// el arbol de la escena: Object::name o una punta de vinculo cacheada (targetName / RailName).
// false = el objeto ya no esta (borrado/liberado) -> el paso de undo que apunte ahi tiene que
// ser un NO-OP, nunca un write a memoria liberada. Es LA validacion del destino 'Directo' del
// undo de renames; los elementos de una lista NO van por puntero (ver W3dRenameDest en Undo.h).
bool W3dPointerNameLive(std::string* p);
void CalcObjectsTransformPivotPoint(Object* obj);
void SetTransformPivotPoint();
// centro geometrico de la seleccion (para el FOCO '.', distinto del pivote)
Vector3 FocusCenterSelection();
// transform global (TRS) por la cadena de padres: world<->local de PUNTOS. Lo usa
// el transform de sub-elementos de malla (editor) sin re-implementar la cadena.
// (RotGlobalOf se declara en el Core, objects/Objects.h)
Vector3 ScaleGlobalOf(Object* o);
// aplica una rotacion DE MUNDO sobre una rotacion LOCAL base de 'o', conjugando
// por el padre (local' = P^-1 * R * P * base). Con padre sin rotar es el
// pre-multiply de siempre. La usan TODOS los caminos que rotan objetos desde un
// eje de mundo (BeginRotate, el trackball de la vista, el snap de rotacion...):
// sin la conjugacion, rotar un objeto emparentado bajo un padre rotado giraba
// alrededor del eje equivocado.
Quaternion W3dRotateWorldOver(Object& o, const Quaternion& Rmundo, const Quaternion& localBase);
// reposiciona los seleccionados alrededor del pivote tras rotar/escalar (objetos)
void ApplyPivotToTransform();
void SaveMousePos();
void saveRecState(Object* obj);
bool saveState();
void BeginTranslate();
void DuplicatedObject();
// copia REAL de UN objeto (mesh deep-copy; luz/camara/empty/texto2d/imagen2d sus propiedades).
// La usan DuplicatedObject y el duplicado del Editor 2D. 'vinculado' (Alt+D) solo cambia lo
// que se puede compartir sin dejar de ser una copia: un armature usa los MISMOS clips que el
// original (W3dArmatureAnimsLink) en vez de una copia propia.
Object* W3dDuplicateOne(Object* src, bool linked = false);
// Separate (Edit Mode: P / menu Mesh > Separate): mueve las caras SELECCIONADAS a un mesh NUEVO (misma
// transform + materiales + vertex groups + modificadores) y las borra del actual. true si separo algo.
bool SeparateSelectionEdit(Mesh* m);
void NewInstance();
// Join (Ctrl+J, menu Object): une las mallas seleccionadas dentro del objeto ACTIVO (conserva su transform).
void JoinObjects();
// Apply (Alt+A, menu Object > Apply): hornea el transform en la malla. what: 0=Location 1=Rotation 2=Scale 3=All.
void ApplyTransform(int what);
// Set Active Object as Camera (Ctrl+Numpad 0): el activo pasa a ser la camara activa. false si NO es una camara.
bool SetActiveObjectAsCamera();
// ctrl+p: emparenta los seleccionados al objeto ACTIVO (conservando la
// posicion global). alt+p: los devuelve a la raiz.
void SetParentSelection();
// reparenta conservando lo local (puede saltar) / manteniendo lo global
void ReparentSimple(Object* obj, Object* newParent);
void ReparentKeepTransform(Object* obj, Object* newParent);
// reordena junto a otro objeto (drag del outliner)
void MoveNextTo(Object* obj, Object* ref, bool after);
void ClearParentSelection();
// REPARENTAR ES DESHACIBLE (ronda 14): cada una de esas cuatro puertas deja SU paso de undo
// (ver el bloque del embudo en ObjectMode.cpp). El que mueve VARIOS objetos en una sola
// accion del usuario -los menus Set/Clear Parent, el emparentado del Editor 2D- envuelve su
// bucle con estas dos, y los N pasos se funden en UN Ctrl+Z. Anidan.
void ReparentGroupStart();
void ReparentGroupEnd();
void SetRotation(int dx, int dy);
void BeginRotate();
// reconstruye el quaternion REAL del objeto activo desde los valores de display
// editados, segun rotMode (euler XYZ / quaternion / axis-angle). onChange.
void SyncActiveRotation();
// rotacion ORBITAL/gimbal (libre): izq/der=camUp, arr/ab=camRight (quaterniones)
void RotateOrbital(int dx, int dy);
// alterna rotacion libre entre trackball (eje de vista) y orbital (V / 0)
void ToggleRotationOrbital();
// cicla eje/orientacion (X/Y/Z=0/1/2) durante un transform: Global->Local->View->free
void CycleAxisTransform(int axis);
// Shift+eje: constriñe a un PLANO (excluye 'eje', mueve en los otros dos)
void CyclePlaneTransform(int axis);
// eje (X/Y/Z) EN WORLD segun la orientacion actual (global/local/view).
// Lo usa el render para dibujar las guias con la orientacion correcta.
Vector3 AxisOriented(Object& obj, int a);
void SetScale(int dx, int dy, float factor = 0.01f);
// la escala de los objetos en curso = la de ANTES * f (en los ejes elegidos; proportional editing con su peso). La usa
// el mouse: f = distancia del puntero al pivote / la de cuando empezo (como Blender: proporcional a lo que se ve)
void SetScaleFactor(float f);
void BeginScale();
void SetTranslationObjects(int dx, int dy, float factor = 1.0f);
void SetTranslationObjectsWorld(const Vector3& d); // absoluta (mundo) desde el snapshot: la usa el gizmo
// entrada numerica: aplica un valor EXACTO al transform de objetos en curso
void SetNumericTransform(float v);

// EDIT MODE: traslada RIGIDO los verts seleccionados por 'deltaLocal' (coords LOCALES del mesh) + persiste
// (empuja al render, rebordes, normales, preview de modificadores, undo). Lo usan el snap y los campos X/Y/Z
// de posicion del panel de Vertices.
void MoveSelectionEditLocal(Mesh* m, const Vector3& deltaLocal);

// snap (menu shift+s): mueve la seleccion o el cursor 3D 
void SnapSelectionToCursor(bool keepOffset);
void SnapSelectionToActive();
void SnapSelectionToGrid();
void SnapCursorToGrid();
void SnapCursorToOrigin();
void SnapCursorToSelected();
void SnapCursorToActive();
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
//  carpeta es EL flujo tipico- cada guardado tiraba "Guardado, pero 1 file(s)
//  external(s) NO estan" y cada apertura su warning, PARA SIEMPRE y sin nada en la
//  UI que lo mostrara ni lo borrara. Un aviso correcto que no se puede apagar es
//  ruido, y el ruido irreversible entrena a ignorar los avisos que si importan.
//  Esta es la salida: olvidar la procedencia (la geometria no se toca).
//  Devuelve cuantas mallas quedaron sin origen.
int ForgetOriginSelected();

// PROPORTIONAL EDITING en Object Mode: cambio el radio con un transform en curso -> re-evaluar el peso
// de los objetos no seleccionados del snapshot (edit/Proportional.h)
void ProportionalObjectsUpdate();

#endif