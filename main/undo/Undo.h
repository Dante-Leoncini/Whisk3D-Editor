#ifndef W3D_UNDO_H
#define W3D_UNDO_H

#include <string>
#include <vector>   // UndoBone2DRenameCapturar: la lista de nombres que van en el MISMO paso

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
//  Por eso ~Object avisa por la lista de ganchos de Objects.h (W3dDesvincularRegistrar) y este
//  archivo engancha ahi un recorrido de los subarboles detachados (UndoDesvincularDetachados,
//  Undo.cpp): ningun puntero a Object puede sobrevivir al free, este donde este. En esa misma
//  lista se registra la UI del editor, que tambien guarda punteros fuera del arbol (el popup de
//  color y el panel "Add" apuntan adentro del objeto que editan; ver el test 'uipunteros'). Corolario practico: un comando SIEMPRE se
//  saca del stack ANTES de hacerle delete (BorrarCmds), sino ese recorrido lo pisa muerto.
// ============================================================================

void UndoDeshacer(); // Ctrl+Z: deshace el ultimo comando (pasa al stack de redo)
void UndoRehacer();  // Ctrl+Y: rehace el ultimo deshecho (pasa de vuelta al stack de undo)
void UndoLimpiar();  // limpia ambos stacks (al borrar/crear objetos: punteros invalidos)
bool UndoHayAlgo();  // hay algo para deshacer?
bool UndoHayRedo();  // hay algo para rehacer?

void UndoCapturarModo();                        // antes de cambiar Edit/Object

// ============================================================================
//  DESTINO de un rename (W3dRenameDest) - IDENTIDAD ESTABLE, no un puntero crudo.
//
//  NINGUN destino se guarda como puntero "y listo": TODOS se revalidan al aplicar.
//  Hay dos formas de morir, y las dos terminaban escribiendo en memoria liberada:
//
//  (1) el nombre vive DENTRO de un contenedor POR VALOR:
//      Armature::bones (vector<W3dBone>), Armature2D::huesos (vector<W3dBone2D>),
//      Mesh::materialsGroup (mesh parts), W3dScriptEntrada::refs (vector<pair>).
//      Cualquier alta/baja posterior REALOCA el vector y el puntero queda colgando.
//      -> se guardan como DUENO + INDICE (Hueso3D/Hueso2D/MeshPart/RefLua) y se
//         resuelven al aplicar, chequeando que el dueno siga en la escena y que el
//         indice siga existiendo.
//
//  (2) el dueno del heap SE BORRA Y SE RECREA. La premisa vieja ("VertexGroup* /
//      UVGroup* / UVMap* / ColorLayer* son duenos del heap que no se mueven") era
//      FALSA: Mesh::LiberarCapas los DESTRUYE y MeshGeoUndo::AplicarA los rehace con
//      new (punteros distintos), asi que CUALQUIER operacion de geometria apilada
//      encima de un rename de grupo/capa dejaba el destino colgado; lo mismo el
//      boton "-" de las tarjetas, o borrar un material / un clip / un armature 2D /
//      un objeto. -> el caso Directo se valida POR IDENTIDAD contra los nombres
//         vivos (W3dNombrePunteroVivo, ObjectMode.cpp).
//
//  Sintomas reales de no hacerlo: crash (heap-use-after-free / std::length_error) o
//  -peor- corrupcion SILENCIOSA: el undo escribia el nombre viejo ENCIMA de otra
//  cosa (un UV group ajeno), rompiendo el binding por nombre sin ningun aviso.
//  Si el destino ya no existe, el paso es un NO-OP seguro (nunca escribe "en
//  cualquier lado").
// ============================================================================
class Armature;    // huesos 3D (por valor en Armature::bones)
struct Armature2D; // huesos 2D (por valor en Armature2D::huesos)
class Object;      // refs de los scripts lua (por valor en scriptDatos->scripts[].refs)

struct W3dRenameDest {
    enum Tipo { Directo = 0, Hueso3D, Hueso2D, MeshPart, RefLua, CapaMalla, ClipArm, Clip2D, Global };
    // CapaMalla: CUAL de las listas con nombre de la malla (todas son vector<T*>, ver Mesh.h)
    enum Capa  { VGroup = 0, UVGroup, UVMap, ColorLayer, Arm2D, VertAnim };
    // Global: cual de las listas con nombre del PROYECTO
    enum Lista { MaterialG = 0, SceneAnimG };
    int   tipo;
    void* dueno;      // Armature* (Hueso3D, ClipArm) / Mesh* (Hueso2D, MeshPart, CapaMalla, Clip2D) / Object* (RefLua)
    void* dueno2;     // Hueso2D / Clip2D: el Armature2D* CONCRETO (la lista de la malla puede cambiar)
    int   cual;       // CapaMalla: Capa / Global: Lista
    int   i, j;       // indice del hueso / de la parte / del elemento de la lista / (script, ref)
    std::string* ptr; // solo Directo
    W3dRenameDest() : tipo(Directo), dueno(0), dueno2(0), cual(0), i(-1), j(-1), ptr(0) {}
};
// Directo: SOLO para nombres que son MIEMBRO de un Object (Object::name, y las puntas de
// vinculo que enumera Object::RefObjetoNombre: targetName / RielName / la fuente de cada
// constraint). Esos no se recrean: o el objeto esta en la escena
// o no esta, y eso se chequea (W3dNombrePunteroVivo). NO usarlo para elementos de una lista:
// esos se borran y se re-crean EN LA MISMA DIRECCION (el allocator recicla el bloque), asi que
// validar "el puntero sigue siendo un nombre vivo" NO alcanza -> van por (dueno + indice).
// Pasarle CUALQUIER otro puntero da un destino que W3dNombrePunteroVivo NO reconoce -> el
// resolver devuelve NULL SIEMPRE y el paso de undo queda MUERTO (no restaura nada, sin aviso).
// Justo eso pasaba con los UV groups del rename de hueso 2D. Por eso ya NO existe ninguna API
// de captura que tome std::string* pelado: todas piden un W3dRenameDest y el compilador rompe
// si alguien intenta capturar un elemento de lista por puntero. Aca se loguea si igual llega
// un puntero que no es un nombre de Object.
W3dRenameDest W3dDestNombre(std::string* p);
W3dRenameDest W3dDestHueso3D(Armature* a, int idx);
W3dRenameDest W3dDestHueso2D(Mesh* m, Armature2D* arm, int idx);
W3dRenameDest W3dDestMeshPart(Mesh* m, int idx);
W3dRenameDest W3dDestRefLua(Object* o, int script, int ref);
// listas con nombre que viven en un vector<T*>: el elemento SE LIBERA (boton "-", MeshGeoUndo,
// borrar un clip/material) y el proximo new puede caer en la misma direccion -> (dueno, indice)
W3dRenameDest W3dDestCapaMalla(Mesh* m, int capa, int idx);        // capa = W3dRenameDest::Capa
W3dRenameDest W3dDestClipArm(Armature* a, int idx);                // clip 3D del esqueleto
W3dRenameDest W3dDestClip2D(Mesh* m, Armature2D* arm, int idx);    // clip del armature 2D
W3dRenameDest W3dDestGlobal(int lista, int idx);                   // lista = W3dRenameDest::Lista
std::string*  W3dDestResolver(const W3dRenameDest& d);             // NULL = ya no existe

// antes de escribir un nombre (objeto/material/uv/color/grupo/hueso/clip...). SIEMPRE con un
// W3dRenameDest: no hay version que tome std::string* (ver el bloque de arriba).
void UndoCapturarRename(const W3dRenameDest& destino);
// N nombres en UN SOLO paso de undo: para los renames que arrastran su contraparte POR NOMBRE
// (vertex group <-> hueso 3D, UV group <-> hueso 2D, refs de scripts lua, targetName/RielName).
// En pasos separados, el Ctrl+Z intermedio deja el binding roto (ver Bone2DRenameUndo en Undo.cpp).
void UndoCapturarRenames(const std::vector<W3dRenameDest>& destinos);
// FUNDE los ultimos 'n' pasos del stack en UNO: un Ctrl+Z los deshace juntos. Para las
// operaciones compuestas armadas con capturas que ya existen (ej: rename de hueso 3D =
// snapshot de huesos + nombres de los vertex groups homonimos, que tienen que volver juntos).
void UndoFundirUltimos(int n);
// ============================================================================
//  GRUPO: todo lo que se empuje entre Iniciar y Fin es UN SOLO Ctrl+Z.
//
//  Es la version "por rango" de UndoFundirUltimos, para cuando el que abre el grupo NO sabe
//  cuantos pasos van a caer adentro (el reparent empuja el paso del ARBOL y, si la mudanza
//  cruzo de escena, el embudo empuja ADEMAS el de los NOMBRES; y un Ctrl+P sobre N objetos
//  seleccionados son N mudanzas). Anida (guarda una pila de bases) y nunca funde de mas: si
//  el desalojo del historial (MAXU) se comio pasos del grupo, funde los que quedaron.
//  Fundir NO es una accion nueva: el redo ya lo vacio el primer Push de adentro.
// ============================================================================
void UndoGrupoIniciar();
void UndoGrupoFin();
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
//  ReparentKeepTransform y MoverJuntoA, ObjectMode.cpp) y NO en el embudo W3dAdjuntarA: el
//  embudo solo hace la cirugia de punteros y el TRANSFORM lo reescriben los llamadores
//  DESPUES (Keep Transform), asi que capturar ahi guardaba coordenadas ya pisadas.
//  Pendiente (Iniciar/Confirmar/Cancelar) como el transform: una mudanza que deja al objeto
//  donde estaba no ensucia el historial.
// ============================================================================
class Object;
void UndoReparentIniciar(Object* o); // ANTES de mover: padre + orden entre hermanos + transform local
void UndoReparentConfirmar();        // DESPUES: pushea si algo de eso cambio
void UndoReparentCancelar();         // descarta el pendiente
// ============================================================================
//  MOVER / BORRAR un elemento de una lista con nombre: los INDICES se corren.
//
//  Los destinos de rename van por (dueno, lista, INDICE), asi que CUALQUIER operacion que
//  corra los indices de la lista tiene que avisarle al undo. Si no, el Ctrl+Z de un rename
//  anterior escribe el nombre viejo ENCIMA del elemento de al lado: corrupcion SILENCIOSA
//  (ni crash ni aviso), y ademas el rename que se queria deshacer NO se deshace.
//
//  (A) REORDENAR (botones subir/bajar): UndoMoverCapaMalla / UndoMoverClipArm hacen el
//      intercambio, mueven el activo, dejan el PASO DE UNDO y remapean los destinos, todo
//      desde el MISMO lugar: nadie reordena "por su cuenta" y se saltea el remapeo.
//      'capa' = W3dRenameDest::Capa. Cubre las listas que TIENEN botones de reordenar:
//      VGroup, UVGroup, UVMap y ColorLayer. Arm2D y VertAnim no tienen (no hay boton), y
//      para ellas devuelve false sin tocar nada. true = se movio.
bool UndoMoverCapaMalla(Mesh* m, int capa, int i, int j);
// clips 3D del esqueleto (Armature::animations, botones subir/bajar de la tarjeta Animation).
// La lista vive en el CORE (SkeletalAnimation.cpp) pero el remapeo es del EDITOR (el runtime
// de los juegos no tiene undo), asi que el swap se hace ACA, igual que UndoMoverCapaMalla, y
// MoverAnimacionActiva del Core queda SOLO para el runtime. Ver el .cpp.
bool UndoMoverClipArm(Armature* a, int i, int j);
//
//  (B) BORRAR (el boton "-"): borrar TAMBIEN corre los indices (los de ARRIBA del borrado
//      bajan uno) y esa era la otra mitad del fallo. Hay dos familias:
//       (1) el "-" empuja ANTES un paso de undo que restaura la LISTA ENTERA: los cuatro
//           Borrar{UVMap,ColorLayer,VertexGroup,UVGroup}Activo (UndoCapturarMallaGeo),
//           UndoArm2DBorrar y AccionMeshPartUp/Down. Esas NO se avisan: por LIFO el Ctrl+Z
//           restaura la lista con sus indices ORIGINALES antes de que aplique el rename
//           viejo, asi que desplazarlos aca los dejaria corridos justo al reves.
//       (2) el "-" no empuja NADA (clips 3D, clips 2D, vertex anims, ANIMACIONES DE ESCENA y
//           la lista de SCRIPTS lua de un objeto: borrarlos todavia no es deshacible). Ahi el
//           indice stale pega de lleno -> hay que avisar.
//      'lista' = el destino tipado CON el indice que se borra: W3dDestClipArm(a, i),
//      W3dDestClip2D(m, arm, i), W3dDestCapaMalla(m, VertAnim, i), W3dDestGlobal(SceneAnimG, i),
//      W3dDestRefLua(obj, script, 0) -para la lista de SCRIPTS, donde el indice que corre es el
//      del script (.i), no el de la ref-... Se puede llamar antes o despues del erase (solo mira
//      los stacks del undo/redo). El destino del PROPIO elemento queda MUERTO (indice -1 = no-op
//      seguro) y los de arriba bajan uno.
void UndoListaBorrada(const W3dRenameDest& lista);
// REORDENAR una lista cuyo movimiento NO es deshacible (hoy: los scripts lua de un objeto,
// que tampoco tienen borrado deshacible). Es la mitad "aviso" de UndoMoverCapaMalla: el swap
// lo hace el llamador y esto SOLO remapea los destinos de rename i <-> j, que es lo que exige
// el criterio general de arriba para cualquier operacion que corra indices.
// 'lista' = el destino tipado con indice -1 (identifica la lista, no un elemento).
void UndoListaMovida(const W3dRenameDest& lista, int i, int j);
// atajos que hacen el aviso Y el borrado en UN solo lugar: esas listas viven en el Core
// y olvidarse el aviso en uno de los llamadores es justo el bug que esto arregla.
void UndoBorrarClipArm(Armature* a);   // "-" de la tarjeta Animation: clip 3D activo
void UndoBorrarClip2D(Mesh* m);        // "-" de la tarjeta Animation: clip del armature 2D activo
void UndoBorrarEscenaActiva();         // "-" de la tarjeta Animation: animacion de ESCENA activa
// ============================================================================
//  LOS PASOS DE UNDO QUE GUARDAN UN INDICE
//
//  Un paso puede guardar la POSICION de algo en una lista (la capa 3 de una malla, el
//  clip 2 de un armature). Si esa lista se CORRE entre que se guarda y que se aplica
//  -porque se borro o se reordeno un elemento-, el Ctrl+Z cae sobre el elemento de al
//  lado: no hay crash ni aviso, se corrompe en silencio.
//
//  Toda operacion que corra los indices de una lista con destinos de rename por indice
//  tiene que SNAPSHOTEAR la lista entera en el mismo paso, o AVISAR (UndoListaBorrada /
//  UndoListaMovida / UndoMoverCapaMalla / UndoMoverClipArm). Y el paso que guarde esos
//  indices tiene que implementar RemapLista.
//
//  La regla completa, con la tabla de clasificacion de miembros (a/b/c/d) y las tres
//  reglas para no clasificar mal, esta en docs/decisiones.md. Por que es asi (ocho
//  rondas del mismo bug) esta en docs/decisiones-historia.md.
// ============================================================================
void UndoCapturarSeleccion();                   // antes de cambiar la seleccion de OBJETOS
void UndoCapturarSeleccionEdit(Mesh* m);        // antes de cambiar la seleccion de SUB-ELEMENTOS (verts/edges/faces)
void UndoCapturarMaterial(Mesh* m, int idx);    // antes de cambiar el Material de un mesh part
void UndoCapturarMallaGeo(Mesh* m);             // ANTES de un op de geometria (extrude/delete/loop/duplicate/assign)
bool UndoCapturarBorrado(bool incCol);          // BORRA objetos: los DETACHA (sin liberar) + guarda para deshacer. true si borro algo
// JOIN (Ctrl+J): undo ATOMICO en 1 paso = geometria del activo + borrado de los mergeados.
void UndoJoinIniciar(Mesh* activeMesh);         // ANTES de mergear: snapshot de la geo del objeto activo
void UndoJoinConfirmar();                       // DESPUES: borra los seleccionados (los mergeados) + empaqueta todo
// APPLY (Alt+A): undo ATOMICO en 1 paso = geo + transform de TODAS las mallas seleccionadas.
void UndoApplyIniciar();                        // ANTES de hornear: snapshot de transforms + geo de los seleccionados
void UndoApplyConfirmar();                      // DESPUES: empaqueta geo+transform en 1 comando
void UndoCapturarColor(float* target, const float* viejo); // COLOR de material/luz (lo llama el ColorPicker al cerrar)
void UndoMaterialModIniciar(Material* m);       // antes de tocar checkbox/shininess de un material (snapshot)
void UndoMaterialModCommit();                   // al soltar el mouse: pushea el cambio de material si difiere

// CURVAS de animacion: snapshot de todos los keyframes (frames, valores, interpolacion y handles) del clip/escena
// activa. Lo usan el editor de curvas y la tarjeta "Keyframe" del panel de propiedades. Si no hubo cambio real,
// Confirmar descarta el snapshot y no ensucia el stack.
void UndoKeyframesIniciar();
void UndoKeyframesConfirmar();
// POSE de un esqueleto (mismo criterio: pendiente hasta confirmar)
class Armature;
void UndoPoseIniciar(Armature* a);
void UndoPoseConfirmar();

// EDIT MODE de HUESOS: snapshot COMPLETO de bones[] (+ boneActivo + los tracks de los clips, que
// referencian huesos por INDICE y se remapean al borrar). Patron MeshGeoUndo: swap total.
//  - Capturar: ANTES de una operacion discreta (extrude/duplicar/borrar/desconectar/rename) -> push directo.
//  - Iniciar/Confirmar/Cancelar: para un DRAG de head/tail (pendiente; no pushea si no cambio nada).
void UndoBonesCapturar(Armature* a);
void UndoBonesIniciar(Armature* a);
void UndoBonesConfirmar();
void UndoBonesCancelar();

// PESOS (weight paint): snapshot de LOS DOS grupos de pesos (vertexGroups por control-point +
// uvGroups por corner, ver Mesh.h) al EMPEZAR un trazo del pincel, commit al SOLTAR -> un paso
// de undo POR TRAZO, venga el trazo del viewport 3D o del editor UV. Descarta si no cambio nada.
void UndoPesosIniciar(Mesh* m);
void UndoPesosConfirmar();

// TRANSFORM DE UVs (G/R/S del editor UV + tarjeta "Transform UV" del panel): pendiente hasta
// confirmar, un paso por operacion. DOS variantes (decision documentada en Undo.cpp):
//  - LIVIANO (UVMapUndo): un G/R/S de verts/bordes (o de caras SIN split) solo cambia VALORES
//    de mesh->uv -> snapshot de la capa uv completa + uv2dRest (patron EditMoveUndo: swap).
//  - COMPLETO: un transform de CARA que hace SPLIT de esquinas (UVSepararCarasSel cambia el
//    conteo de verts/faces3d/arrays paralelos) NO se puede deshacer con el liviano -> se reusa
//    MeshGeoUndo con el snapshot total tomado ANTES del split.
void UndoUVIniciar(Mesh* m);          // liviano (solo cambian valores de uv)
void UndoUVIniciarCompleto(Mesh* m);  // pesado: transform de cara CON split (pre-split)
void UndoUVConfirmar(bool cambio);    // push si el caller detecto cambio real; sino descarta
void UndoUVCancelar();                // descarta el pendiente (transform cancelado)

// ARMATURE 2D del mesh (huesos del editor UV): patron BonesUndo adaptado a W3dBone2D.
// Snapshot COMPLETO de los huesos del armature 2D ACTIVO (+ su hueso activo) y la capa uv y uv2dRest (la POSE 2D
// deforma mesh->uv, y borrar huesos re-aplica el skinning 2D). Aplicar() = SWAP total.
// Los UV groups NO se snapshotean: Bone2DBorrar los conserva a proposito, y el grupo creado
// por un Add/Extrude deshecho queda vacio (inofensivo, como el grupo automatico del weight
// paint). Los vertex groups no los toca el rig 2D.
//  - Capturar: ANTES de una op discreta (crear/agregar/extruir/duplicar/borrar/desconectar) -> push directo.
//  - Iniciar/Confirmar/Cancelar: G/R/S modal y drag de puntas (pendiente; sin cambio no pushea).
void UndoBones2DCapturar(Mesh* m);
void UndoBones2DIniciar(Mesh* m);
void UndoBones2DConfirmar();
void UndoBones2DCancelar();
// RENAME de un hueso 2D: ATOMICO (mismo patron que JoinUndo). El hueso y los UV GROUPS homonimos
// se llaman IGUAL a proposito (el binding del rig 2D es POR NOMBRE), asi que renombrarlos en dos
// pasos de undo separados dejaba un estado intermedio con el binding ROTO: el primer Ctrl+Z
// devolvia el nombre del hueso y los UV groups se quedaban con el nuevo -> el rig no deformaba
// hasta apretar Ctrl+Z otra vez. Se captura el snapshot de huesos + los nombres de los grupos que
// van a cambiar, y se pushea UN solo comando. Llamar ANTES de escribir los nombres.
// 'grupos' = los DESTINOS (malla, lista UVGroup, indice) de los UV groups homonimos; pueden ser 0
// (el hueso puede no tener ninguno). OJO: NO alcanza con un std::string* al nombre del grupo. Los
// UV groups viven en un vector<UVGroup*> que se libera y se recrea (LiberarCapas / boton "-"), y
// ademas un puntero que no es &Object::name NUNCA lo reconoce W3dNombrePunteroVivo -> el
// RenameUndo quedaba MUERTO y el undo devolvia el nombre del HUESO dejando el del GRUPO con el
// nuevo: binding roto PARA SIEMPRE (ni un segundo Ctrl+Z lo arreglaba). Ver W3dRenameDest arriba.
void UndoBone2DRenameCapturar(Mesh* m, const std::vector<W3dRenameDest>& grupos);

// ALTA / BAJA de un ARMATURE 2D entero (la LISTA Mesh::armatures2d, no sus huesos). El paso de
// undo se queda con el PUNTERO al Armature2D (no una copia: es no copiable y ademas los
// Bones2DUndo pendientes apuntan a ese puntero concreto y lo validan con Vive()) -> deshacer un
// borrado devuelve el MISMO armature y el historial de huesos anterior sigue aplicando.
void UndoArm2DAgregado(Mesh* m, int idx); // el armature YA se agrego: deshacer lo saca
bool UndoArm2DBorrar(Mesh* m, int idx);   // BORRA el armature idx dejandolo vivo en el undo

// ============================================================================
//  STACK DE MODIFICADORES (Mesh::modificadores) - LA PUERTA UNICA DEL USUARIO
//
//  Estas tres son las que tiene que llamar TODO camino de usuario (los botones Add / Remove /
//  subir / bajar de la tarjeta Modifier, los comandos del script, el Apply): capturan el stack,
//  hacen la operacion y dejan el paso de undo, todo junto. Los metodos crudos del Mesh
//  (AgregarModificador / QuitarModificadorActivo / MoverModificador, main/edit/MeshEdit.cpp)
//  NO dejan paso y quedan solo para la carga del .w3d y el destructor.
//  Antes el "Remove" hacia delete a secas: se perdia el modificador con sus parametros sin
//  Ctrl+Z posible, y ademas era la unica de estas entidades que se LIBERABA por un boton, sin
//  ningun invariante de orden que la protegiera -> el DeleteUndo que se habia guardado ese
//  Modifier* para restaurarle el target escribia sobre la direccion RECICLADA por el
//  modificador de otra malla, y eso se guardaba en el .w3d (test 'modtarget'). Ahora el
//  modificador quitado no se libera: lo adopta el paso de undo (ver ModStackUndo en Undo.cpp).
void UndoModAgregar(Mesh* m, int tipo);  // tipo = ModifierType::Enum
void UndoModQuitar(Mesh* m);             // quita el ACTIVO (sin liberarlo: lo adopta el undo)
void UndoModMover(Mesh* m, int dir);     // -1 = sube, +1 = baja (el orden del stack importa)
void UndoModVaciar(Mesh* m);             // saca TODOS (el "Apply" que hornea el stack entero)

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
void UndoConAgregar(Object* o, int tipo); // tipo = W3dConstraintTipo::Enum
void UndoConQuitar(Object* o);            // quita el ACTIVO (sin liberarlo: lo adopta el undo)
void UndoConMover(Object* o, int dir);    // -1 = sube, +1 = baja (el orden del stack importa)

// transform (object mode): pendiente hasta confirmar, asi un transform CANCELADO no deja un undo no-op.
// Los elementos 2D capturan ademas rot2d/ancho/alto (o el tam del texto y el lienzo del UI).
class Object;
void UndoTransformIniciar();   // captura pos/rot/escala de los seleccionados (al empezar)
void UndoTransformIniciarObj(Object* o); // variante: captura SOLO este objeto (ej: resize del lienzo)
void UndoTransformConfirmar(); // al aceptar el transform: pushea el pendiente al stack
void UndoTransformCancelar();  // al cancelar: descarta el pendiente

// mover verts/aristas/caras en EDIT MODE (G/R/S, move PURO sin cambio de topologia): captura las
// posiciones+normales de la malla; pendiente hasta confirmar (mismo patron que el transform de objeto).
void UndoEditMoveIniciar(Mesh* m);
void UndoEditMoveConfirmar();
void UndoEditMoveCancelar();

#endif // W3D_UNDO_H
