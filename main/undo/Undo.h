#ifndef W3D_UNDO_H
#define W3D_UNDO_H

#include <string>
#include <vector>   // UndoBone2DRenameCapture: la lista de nombres que van en el MISMO paso

class Mesh;     // capturas de geometria/material
class Material; // modificacion de material (checkbox/shininess)

// ============================================================================
//  UNDO (Ctrl+Z) por COMANDOS - compartido 4 OS. Cubre: transforms (pos/rot/escala
//  en object mode), nombres, cambio de modo edit/objeto, seleccion de objetos,
//  material de mesh part, geometria de malla (extrude/delete/loop/duplicate/assign),
//  mover sub-elementos en edit, y SELECCION de sub-elementos en edit (verts/edges/faces).
//  BORRAR objetos tambien es un comando (DeleteUndo: detacha y restaura, no limpia
//  el stack). CREAR objetos todavia no se captura (el Ctrl+Z no deshace un Add).
//  Las capturas se llaman ANTES del cambio: guardan el estado PREVIO.
//
//  *** LOS OBJETOS DETACHADOS SON PARTE DEL ESTADO DEL PROGRAMA ***
//  Mientras un DeleteUndo esta en el stack, los objetos que borro NO estan liberados: los tiene
//  el comando, fuera del arbol. O sea que "recorrer la escena" NO es lo mismo que "recorrer todo
//  lo que existe", y cualquier limpieza que solo camine SceneCollection deja esa mitad afuera.
//  Por eso ~Object avisa por la lista de ganchos de Objects.h (W3dUnlinkRegister) y este
//  archivo engancha ahi un recorrido de los subarboles detachados (UndoUnlinkDetached,
//  Undo.cpp): ningun puntero a Object puede sobrevivir al free, este donde este. En esa misma
//  lista se registra la UI del editor, que tambien guarda punteros fuera del arbol (el popup de
//  color y el panel "Add" apuntan adentro del objeto que editan; ver el test 'uipunteros'). Corolario practico: un comando SIEMPRE se
//  saca del stack ANTES de hacerle delete (DeleteCmds), sino ese recorrido lo pisa muerto.
// ============================================================================

void UndoUndo();  // Ctrl+Z: deshace el ultimo comando (pasa al stack de redo)
void UndoRedo();  // Ctrl+Y: rehace el ultimo deshecho (pasa de vuelta al stack de undo)
void UndoClear(); // limpia ambos stacks (al borrar/crear objetos: punteros invalidos)
// una escena abierta que se CIERRA y se libera (io/EditorRoots.h): sus pasos de undo y redo se descartan
class Object;
void UndoForgetRoot(Object* root);
bool UndoHasSomething();  // hay algo para deshacer?
bool UndoHasRedo();       // hay algo para rehacer?
const void* UndoLast();          // identidad del ultimo comando (NULL = vacio): el panel de la ultima operacion
                                   // solo deshace SU paso
extern void (*g_undoChangeHook)(); // se llama antes de cada deshacer / rehacer (el panel se cierra)

void UndoCaptureMode();                       // antes de cambiar Edit/Object
void UndoCaptureVisibility();                 // antes de togglear Object::visible con 'h' (guarda el estado previo)
void UndoCaptureVisibilityOf(Object* o);      // antes del ojo / la camarita del outliner (visible y renderable de ESE objeto)

#include "edit/W3dRenameDest.h"   // W3dRenameDest + W3dDest*: viven en el Core (las usa MeshEdit)

// antes de escribir un nombre (objeto/material/uv/color/grupo/hueso/clip...). SIEMPRE con un
// W3dRenameDest: no hay version que tome std::string* (ver el bloque de arriba).
void UndoCaptureRename(const W3dRenameDest& destination);
// N nombres en UN SOLO paso de undo: para los renames que arrastran su contraparte POR NOMBRE
// (vertex group <-> hueso 3D, UV group <-> hueso 2D, refs de scripts lua, targetName/RailName).
// En pasos separados, el Ctrl+Z intermedio deja el binding roto (ver Bone2DRenameUndo en Undo.cpp).
void UndoCaptureRenames(const std::vector<W3dRenameDest>& destinations);
// FUNDE los ultimos 'n' pasos del stack en UNO: un Ctrl+Z los deshace juntos. Para las
// operaciones compuestas armadas con capturas que ya existen (ej: rename de hueso 3D =
// snapshot de huesos + nombres de los vertex groups homonimos, que tienen que volver juntos).
void UndoMergeLast(int n);
// ============================================================================
//  GRUPO: todo lo que se empuje entre Iniciar y Fin es UN SOLO Ctrl+Z.
//
//  Es la version "por rango" de UndoMergeLast, para cuando el que abre el grupo NO sabe
//  cuantos pasos van a caer adentro (el reparent empuja el paso del ARBOL y, si la mudanza
//  cruzo de escena, el embudo empuja ADEMAS el de los NOMBRES; y un Ctrl+P sobre N objetos
//  seleccionados son N mudanzas). Anida (guarda una pila de bases) y nunca funde de mas: si
//  el desalojo del historial (MAXU) se comio pasos del grupo, funde los que quedaron.
//  Fundir NO es una accion nueva: el redo ya lo vacio el primer Push de adentro.
// ============================================================================
void UndoGroupStart();
void UndoGroupEnd();
// idem, pero las partes son SECUENCIALES: cada una se hizo sobre el resultado de la anterior
// (hacer unica la malla compartida y DESPUES hornearle el transform / el Join / el origen).
// Ctrl+Z las deshace AL REVES y Ctrl+Y las rehace AL DERECHO. UndoGroupEnd aplica en el mismo
// orden los dos sentidos (lo que necesitan la mudanza del outliner y sus nombres).
void UndoGroupSequentialEnd();
// ============================================================================
//  REPARENT DESHACIBLE: Ctrl+Z deshace un Ctrl+P (ronda 14)
//
//  Mudar un objeto de padre no era un paso de undo, y eso no era "una comodidad que
//  faltaba": era el HABILITADOR de una familia de corrupciones. Un comando que espera en el
//  stack de REDO guarda POSICIONES del arbol (DeleteUndo: el padre y el indice de cada raiz
//  borrada, y el padre de cada hijo preservado) y las replaya al rehacer; si el usuario
//  podia mover esos objetos SIN vaciar el redo, el replay caia sobre un arbol que ya no era
//  el que el comando dejo -> el mismo Object* en dos listas de hijos, .w3d con el nombre
//  repetido y SEGV al desalojarse el comando (ver ReparentUndo en Undo.cpp y el test
//  'reparentundo'). Al empujar su paso, el reparent VACIA el stack de redo: la ventana se
//  cierra de raiz, no por un guard mas.
//  Se captura en las CUATRO puertas publicas (W3dReparent, ReparentSimple,
//  ReparentKeepTransform y MoveNextTo, ObjectMode.cpp) y NO en el embudo W3dAttachTo: el
//  embudo solo hace la cirugia de punteros y el TRANSFORM lo reescriben los llamadores
//  DESPUES (Keep Transform), asi que capturar ahi guardaba coordenadas ya pisadas.
//  Pendiente (Iniciar/Confirmar/Cancelar) como el transform: una mudanza que deja al objeto
//  donde estaba no ensucia el historial.
// ============================================================================
class Object;
void UndoReparentStart(Object* o); // ANTES de mover: padre + orden entre hermanos + transform local
void UndoReparentConfirm();        // DESPUES: pushea si algo de eso cambio
void UndoReparentCancel();         // descarta el pendiente
// ============================================================================
//  MOVER / BORRAR un elemento de una lista con nombre: los INDICES se corren.
//
//  Los destinos de rename van por (dueno, lista, INDICE), asi que CUALQUIER operacion que
//  corra los indices de la lista tiene que avisarle al undo. Si no, el Ctrl+Z de un rename
//  anterior escribe el nombre viejo ENCIMA del elemento de al lado: corrupcion SILENCIOSA
//  (ni crash ni aviso), y ademas el rename que se queria deshacer NO se deshace.
//
//  (A) REORDENAR (botones subir/bajar): UndoMoveLayerMesh / UndoMoveClipArm hacen el
//      intercambio, mueven el activo, dejan el PASO DE UNDO y remapean los destinos, todo
//      desde el MISMO lugar: nadie reordena "por su cuenta" y se saltea el remapeo.
//      'capa' = W3dRenameDest::Capa. Cubre las listas que TIENEN botones de reordenar:
//      VGroup, UVGroup, UVMap y ColorLayer. Arm2D y VertAnim no tienen (no hay boton), y
//      para ellas devuelve false sin tocar nada. true = se movio.
bool UndoMoveLayerMesh(Mesh* m, int layer, int i, int j);
// clips 3D del esqueleto (Armature::animations, botones subir/bajar de la tarjeta Animation).
// La lista vive en el CORE (SkeletalAnimation.cpp) pero el remapeo es del EDITOR (el runtime
// de los juegos no tiene undo), asi que el swap se hace ACA, igual que UndoMoveLayerMesh, y
// MoveActiveAnimation del Core queda SOLO para el runtime. Ver el .cpp.
bool UndoMoveClipArm(Armature* a, int i, int j);
//
//  (B) BORRAR (el boton "-"): borrar TAMBIEN corre los indices (los de ARRIBA del borrado
//      bajan uno) y esa era la otra mitad del fallo. Hay dos familias:
//       (1) el "-" empuja ANTES un paso de undo que restaura la LISTA ENTERA: los cuatro
//           Borrar{UVMap,ColorLayer,VertexGroup,UVGroup}Activo (UndoCaptureMeshGeo),
//           UndoArm2DDelete y ActionMeshPartUp/Down. Esas NO se avisan: por LIFO el Ctrl+Z
//           restaura la lista con sus indices ORIGINALES antes de que aplique el rename
//           viejo, asi que desplazarlos aca los dejaria corridos justo al reves.
//           Las ANIMACIONES DE ESCENA tambien son de esta familia: UndoDeleteActiveScene y
//           UndoNewScene empujan el paso que la saca/devuelve a su posicion.
//       (2) el "-" no empuja NADA (clips 3D, clips 2D, vertex anims y la lista de SCRIPTS lua de
//           un objeto: borrarlos todavia no es deshacible). Ahi el indice stale pega de lleno ->
//           hay que avisar.
//      'lista' = el destino tipado CON el indice que se borra: W3dDestClipArm(a, i),
//      W3dDestClip2D(m, arm, i), W3dDestLayerMesh(m, VertAnim, i),
//      W3dDestRefLua(obj, script, 0) -para la lista de SCRIPTS, donde el indice que corre es el
//      del script (.i), no el de la ref-... Se puede llamar antes o despues del erase (solo mira
//      los stacks del undo/redo). El destino del PROPIO elemento queda MUERTO (indice -1 = no-op
//      seguro) y los de arriba bajan uno.
void UndoDeletedList(const W3dRenameDest& list);
// REORDENAR una lista cuyo movimiento NO es deshacible (hoy: los scripts lua de un objeto,
// que tampoco tienen borrado deshacible). Es la mitad "aviso" de UndoMoveLayerMesh: el swap
// lo hace el llamador y esto SOLO remapea los destinos de rename i <-> j, que es lo que exige
// el criterio general de arriba para cualquier operacion que corra indices.
// 'lista' = el destino tipado con indice -1 (identifica la lista, no un elemento).
void UndoMovedList(const W3dRenameDest& list, int i, int j);
// atajos que hacen el aviso Y el borrado en UN solo lugar: esas listas viven en el Core
// y olvidarse el aviso en uno de los llamadores es justo el bug que esto arregla.
void UndoDeleteClipArm(Armature* a);   // "-" de la tarjeta Animation: clip 3D activo
void UndoDeleteClip2D(Mesh* m);        // "-" de la tarjeta Animation: clip del armature 2D activo
// "-" de la tarjeta Animation con una animacion de ESCENA activa: la SACA de la lista y el paso de undo se la
// queda (Ctrl+Z la devuelve a su lugar, activa). La unica se vacia (el paso guarda sus curvas). Con la VISTA
// de un clip de jerarquia activa, su CLIP sale de la biblioteca en el mismo paso (vuelven juntos).
void UndoDeleteActiveScene();
// "New" de una animacion de ESCENA: NewScene + el paso que la saca (Ctrl+Z) y la devuelve (Ctrl+Y)
int  UndoNewScene();
// ============================================================================
//  CLIPS DE JERARQUIA (animation/W3dAnimSet.h): la BIBLIOTECA de un objeto, sus clips y el RETARGET.
//  Los pasos que guardan una biblioteca le toman una referencia (W3dAnimSet::refsUndo: no es un usuario),
//  asi una biblioteca en memoria que un Ctrl+Z dejo sin usuarios sigue viva para el Ctrl+Y.
// ============================================================================
class W3dClipHier;
struct W3dResource;
// ANTES de cambiar la biblioteca de clips de 'o' (el desplegable "Clips", "New Library", un clip nuevo que
// le crea la suya): el paso guarda la que tenia
void UndoCaptureHierLibrary(Object* o);
// ANTES de cambiar el RETARGET PROPIO de 'o' (armature: el de sus clips de esqueleto; si no, el de sus clips
// de jerarquia, -1 = el de cada clip)
void UndoCaptureRetarget(Object* o);
// el clip 'c' YA se agrego a la biblioteca 'r' ("New Animation > Hierarchy Clip"): Ctrl+Z lo saca SIN
// liberarlo (su vista queda huerfana: el timeline vuelve a una animacion de escena) y Ctrl+Y lo devuelve y
// vuelve a elegir su vista sobre 'duenio' (la raiz que lo creo; NULL = solo lo devuelve)
void UndoClipAddedHier(W3dResource* r, W3dClipHier* c, Object* owner);
// SACA el clip de su biblioteca y deja el paso que lo devuelve (borrar desde el outliner). false = no estaba
bool UndoClipHierDelete(W3dResource* r, W3dClipHier* c);
// ANTES de cambiar el NOMBRE o el RETARGET POR DEFECTO de un clip de jerarquia
void UndoCaptureClipHier(W3dResource* r, W3dClipHier* c);
// ============================================================================
//  LOS PASOS DE UNDO QUE GUARDAN UN INDICE
//
//  Un paso puede guardar la POSICION de algo en una lista (la capa 3 de una malla, el
//  clip 2 de un armature). Si esa lista se CORRE entre que se guarda y que se aplica
//  -porque se borro o se reordeno un elemento-, el Ctrl+Z cae sobre el elemento de al
//  lado: no hay crash ni aviso, se corrompe en silencio.
//
//  Toda operacion que corra los indices de una lista con destinos de rename por indice
//  tiene que SNAPSHOTEAR la lista entera en el mismo paso, o AVISAR (UndoDeletedList /
//  UndoMovedList / UndoMoveLayerMesh / UndoMoveClipArm). Y el paso que guarde esos
//  indices tiene que implementar RemapList.
//
//  La regla completa, con la tabla de clasificacion de miembros (a/b/c/d) y las tres
//  reglas para no clasificar mal, esta en docs/decisiones.md. Por que es asi (ocho
//  rondas del mismo bug) esta en docs/decisiones-historia.md.
// ============================================================================
void UndoCaptureSelection();                   // antes de cambiar la seleccion de OBJETOS
void UndoCaptureSelectionEdit(Mesh* m);        // antes de cambiar la seleccion de SUB-ELEMENTOS (verts/edges/faces)
void UndoCaptureMaterial(Mesh* m, int idx);    // antes de cambiar el Material de un mesh part
void UndoCaptureMeshGeo(Mesh* m);              // ANTES de un op de geometria (extrude/delete/loop/duplicate/assign)
bool UndoCaptureDeleted(bool incCol);          // BORRA objetos: los DETACHA (sin liberar) + guarda para deshacer. true si borro algo
// los objetos SELECCIONADOS se acaban de CREAR (Shift+D / Alt+D): Ctrl+Z los saca de la escena
// (detachados, como un borrado) y Ctrl+Y los devuelve. true si habia algo que registrar. incCol: tambien las
// COLECCIONES seleccionadas (el Apply del Scatter crea una con las copias adentro)
bool UndoCaptureCreation(bool incCol = false);
// ============================================================================
//  PASO DE UNDO DE UN MODULO DE AFUERA: el que lo empuja conoce sus datos, Undo.cpp no.
//  aplicar     = el toggle de siempre (undo Y redo intercambian lo vivo con lo guardado)
//  desvincular = un objeto se LIBERA de verdad: soltar el puntero si es el suyo (NULL = nada)
//  liberar     = el paso se cae del historial: liberar 'dato' (NULL = nada)
//  Sus datos NO pueden ser posiciones en una lista (no los alcanza RemapList): punteros que
//  el propio modulo valida al aplicar, o valores.
// ============================================================================
struct UndoExternal {
    void (*apply)(void* data);
    void (*unlink)(void* data, Object* deleted);
    void (*free)(void* data);
    UndoExternal() : apply(0), unlink(0), free(0) {}
};
void UndoPushExternal(const UndoExternal& f, void* data);
// JOIN (Ctrl+J): undo ATOMICO en 1 paso = geometria del activo + borrado de los mergeados.
void UndoJoinStart(Mesh* activeMesh);         // ANTES de mergear: snapshot de la geo del objeto activo
void UndoJoinConfirm();                       // DESPUES: borra los seleccionados (los mergeados) + empaqueta todo
// APPLY (Alt+A): undo ATOMICO en 1 paso = geo + transform de TODAS las mallas seleccionadas.
void UndoApplyStart();                        // ANTES de hornear: snapshot de transforms + geo de los seleccionados
void UndoApplyConfirm();                      // DESPUES: empaqueta geo+transform en 1 comando
void UndoCaptureColor(float* target, const float* old); // COLOR de material/luz (lo llama el ColorPicker al cerrar)
void UndoMaterialModStart(Material* m);       // antes de tocar checkbox/shininess de un material (snapshot)
void UndoMaterialModCommit();                 // al soltar el mouse: pushea el cambio de material si difiere

// CURVAS de animacion: snapshot de todos los keyframes (frames, valores, interpolacion y handles) del clip/escena
// activa. Lo usan el editor de curvas y la tarjeta "Keyframe" del panel de propiedades. Si no hubo cambio real,
// Confirmar descarta el snapshot y no ensucia el stack.
void UndoKeyframesStart();
void UndoKeyframesStartArm(Armature* a);   // las curvas del clip activo de ESE esqueleto (auto key de pose)
void UndoKeyframesConfirm();
// POSE de un esqueleto (mismo criterio: pendiente hasta confirmar)
class Armature;
void UndoPoseStart(Armature* a);
void UndoPoseConfirm();

// EDIT MODE de HUESOS: snapshot COMPLETO de bones[] (+ activeBone + los tracks de los clips, que
// referencian huesos por INDICE y se remapean al borrar). Patron MeshGeoUndo: swap total.
//  - Capturar: ANTES de una operacion discreta (extrude/duplicar/borrar/desconectar/rename) -> push directo.
//  - Iniciar/Confirmar/Cancelar: para un DRAG de head/tail (pendiente; no pushea si no cambio nada).
void UndoBonesCapture(Armature* a);
void UndoBonesStart(Armature* a);
void UndoBonesConfirm();
void UndoBonesCancel();

// PESOS (weight paint): snapshot de LOS DOS grupos de pesos (vertexGroups por control-point +
// uvGroups por corner, ver Mesh.h) al EMPEZAR un trazo del pincel, commit al SOLTAR -> un paso
// de undo POR TRAZO, venga el trazo del viewport 3D o del editor UV. Descarta si no cambio nada.
void UndoWeightsStart(Mesh* m);
void UndoWeightsConfirm();

// VERTEX COLOR (pincel de color): mismo trato que los pesos -- un paso de undo POR TRAZO.
// Snapshot de la capa 'capa' (colores + indices de paleta) al apoyar, commit al soltar.
struct TextureEditable;
void UndoTextureStart(TextureEditable* te, bool whole = false);   // PINTURA DE TEXTURA: graba el antes de lo que toque el trazo (whole: todo ya, el horneado)
void UndoTextureConfirm(bool change);         // fin del trazo: pushea si pinto algo
void UndoColorStart(Mesh* m, int layer);
void UndoColorConfirm(bool change);
void UndoColorCancel();

// TRANSFORM DE UVs (G/R/S del editor UV + tarjeta "Transform UV" del panel): pendiente hasta
// confirmar, un paso por operacion. DOS variantes (decision documentada en Undo.cpp):
//  - LIVIANO (UVMapUndo): un G/R/S de verts/bordes (o de caras SIN split) solo cambia VALORES
//    de mesh->uv -> snapshot de la capa uv completa + uv2dRest (patron EditMoveUndo: swap).
//  - COMPLETO: un transform de CARA que hace SPLIT de esquinas (UVSeparateFacesSel cambia el
//    conteo de verts/faces3d/arrays paralelos) NO se puede deshacer con el liviano -> se reusa
//    MeshGeoUndo con el snapshot total tomado ANTES del split.
void UndoUVStart(Mesh* m);          // liviano (solo cambian valores de uv)
void UndoUVStartComplete(Mesh* m);  // pesado: transform de cara CON split (pre-split)
void UndoUVConfirm(bool change);    // push si el caller detecto cambio real; sino descarta
void UndoUVCancel();                // descarta el pendiente (transform cancelado)

// ARMATURE 2D del mesh (huesos del editor UV): patron BonesUndo adaptado a W3dBone2D.
// Snapshot COMPLETO de los huesos del armature 2D ACTIVO (+ su hueso activo) y la capa uv y uv2dRest (la POSE 2D
// deforma mesh->uv, y borrar huesos re-aplica el skinning 2D). Apply() = SWAP total.
// Los UV groups NO se snapshotean: Bone2DDelete los conserva a proposito, y el grupo creado
// por un Add/Extrude deshecho queda vacio (inofensivo, como el grupo automatico del weight
// paint). Los vertex groups no los toca el rig 2D.
//  - Capturar: ANTES de una op discreta (crear/agregar/extruir/duplicar/borrar/desconectar) -> push directo.
//  - Iniciar/Confirmar/Cancelar: G/R/S modal y drag de puntas (pendiente; sin cambio no pushea).
void UndoBones2DCapture(Mesh* m);
void UndoBones2DStart(Mesh* m);
void UndoBones2DConfirm();
void UndoBones2DCancel();
// RENAME de un hueso 2D: ATOMICO (mismo patron que JoinUndo). El hueso y los UV GROUPS homonimos
// se llaman IGUAL a proposito (el binding del rig 2D es POR NOMBRE), asi que renombrarlos en dos
// pasos de undo separados dejaba un estado intermedio con el binding ROTO: el primer Ctrl+Z
// devolvia el nombre del hueso y los UV groups se quedaban con el nuevo -> el rig no deformaba
// hasta apretar Ctrl+Z otra vez. Se captura el snapshot de huesos + los nombres de los grupos que
// van a cambiar, y se pushea UN solo comando. Llamar ANTES de escribir los nombres.
// 'grupos' = los DESTINOS (malla, lista UVGroup, indice) de los UV groups homonimos; pueden ser 0
// (el hueso puede no tener ninguno). OJO: NO alcanza con un std::string* al nombre del grupo. Los
// UV groups viven en un vector<UVGroup*> que se libera y se recrea (FreeLayers / boton "-"), y
// ademas un puntero que no es &Object::name NUNCA lo reconoce W3dPointerNameLive -> el
// RenameUndo quedaba MUERTO y el undo devolvia el nombre del HUESO dejando el del GRUPO con el
// nuevo: binding roto PARA SIEMPRE (ni un segundo Ctrl+Z lo arreglaba). Ver W3dRenameDest arriba.
void UndoBone2DRenameCapture(Mesh* m, const std::vector<W3dRenameDest>& groups);

// ALTA / BAJA de un ARMATURE 2D entero (la LISTA Mesh::armatures2d, no sus huesos). El paso de
// undo se queda con el PUNTERO al Armature2D (no una copia: es no copiable y ademas los
// Bones2DUndo pendientes apuntan a ese puntero concreto y lo validan con Vive()) -> deshacer un
// borrado devuelve el MISMO armature y el historial de huesos anterior sigue aplicando.
void UndoArm2DAdded(Mesh* m, int idx);  // el armature YA se agrego: deshacer lo saca
bool UndoArm2DDelete(Mesh* m, int idx); // BORRA el armature idx dejandolo vivo en el undo

// ============================================================================
//  STACK DE MODIFICADORES (Mesh::modificadores) - LA PUERTA UNICA DEL USUARIO
//
//  Estas tres son las que tiene que llamar TODO camino de usuario (los botones Add / Remove /
//  subir / bajar de la tarjeta Modifier, los comandos del script, el Apply): capturan el stack,
//  hacen la operacion y dejan el paso de undo, todo junto. Los metodos crudos del Mesh
//  (AddModifier / DeleteActiveModifier / MoveModifier, libs/Whisk3DCore/edit/MeshEdit.cpp)
//  NO dejan paso y quedan solo para la carga del .w3d y el destructor.
//  Antes el "Remove" hacia delete a secas: se perdia el modificador con sus parametros sin
//  Ctrl+Z posible, y ademas era la unica de estas entidades que se LIBERABA por un boton, sin
//  ningun invariante de orden que la protegiera -> el DeleteUndo que se habia guardado ese
//  Modifier* para restaurarle el target escribia sobre la direccion RECICLADA por el
//  modificador de otra malla, y eso se guardaba en el .w3d (test 'modtarget'). Ahora el
//  modificador quitado no se libera: lo adopta el paso de undo (ver ModStackUndo en Undo.cpp).
void UndoModAdd(Mesh* m, int type);  // tipo = ModifierType::Enum
void UndoModRemove(Mesh* m);         // quita el ACTIVO (sin liberarlo: lo adopta el undo)
void UndoModMove(Mesh* m, int dir);  // -1 = sube, +1 = baja (el orden del stack importa)
void UndoModEmpty(Mesh* m);          // saca TODOS (el "Apply" que hornea el stack entero)

// ============================================================================
//  STACK DE CONSTRAINTS (Object::constraints) - LA PUERTA UNICA DEL USUARIO
//
//  Las tres gemelas de las de arriba, para los botones Add / Remove / Move Up / Move Down de la
//  tarjeta "Constraints". Mismo trato: capturan el stack, operan y dejan el paso, todo junto; y
//  el constraint quitado NO se libera (lo adopta el paso de undo), porque su serial lo puede
//  estar guardando un RefEntry pendiente y una direccion reciclada le daria OTRO constraint.
//  A diferencia del Mesh, aca NO hay metodos crudos publicos: las operaciones son static en
//  Undo.cpp, o sea que ningun camino de usuario puede saltearse el paso de undo.
//  'o' es CUALQUIER objeto: los constraints no son de la malla (ver Objects.h).
void UndoConstraintAdd(Object* o, int type); // tipo = W3dConstraintType::Enum
void UndoConstraintRemove(Object* o);        // quita el ACTIVO (sin liberarlo: lo adopta el undo)
void UndoConstraintMove(Object* o, int dir); // -1 = sube, +1 = baja (el orden del stack importa)
// ---- el STACK DE CONSTRAINTS DE UN HUESO (W3dBone::constraints, Pose Mode) ----
// Snapshot POR VALOR de los stacks de TODOS los huesos del armature (W3dBoneConstraints copia en profundidad
// conservando el serial: lo que vuelve es el MISMO constraint, y un RefEntry pendiente lo sigue encontrando).
// 'bone' = el hueso del stack; Remove/Move actuan sobre su constraint ACTIVO.
void UndoBoneConstraintAdd(Armature* a, int bone, int type);
void UndoBoneConstraintRemove(Armature* a, int bone);
void UndoBoneConstraintMove(Armature* a, int bone, int dir);
void UndoBoneConstraintsCapture(Armature* a);   // un paso con el estado actual (antes de editar a mano)

// transform (object mode): pendiente hasta confirmar, asi un transform CANCELADO no deja un undo no-op.
// Los elementos 2D capturan ademas rot2d/ancho/alto (o el tam del texto y el lienzo del UI).
class Object;
void UndoTransformStart();   // captura pos/rot/escala de los seleccionados (al empezar)
void UndoTransformStartObj(Object* o); // variante: captura SOLO este objeto (ej: resize del lienzo)
void UndoTransformConfirm(); // al aceptar el transform: pushea el pendiente al stack
void UndoTransformCancel();  // al cancelar: descarta el pendiente

// mover verts/aristas/caras en EDIT MODE (G/R/S, move PURO sin cambio de topologia): captura las
// posiciones+normales de la malla; pendiente hasta confirmar (mismo patron que el transform de objeto).
void UndoEditMoveStart(Mesh* m);
void UndoEditMoveConfirm();
void UndoEditMoveCancel();

#endif // W3D_UNDO_H
