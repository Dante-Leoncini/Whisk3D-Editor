#ifndef BONE_EDIT_H
#define BONE_EDIT_H

#include <string>
#include <vector>
#include "math/Vector3.h"

class Armature;
class Viewport3D;

// ============================================================================
//  EDIT MODE de HUESOS (Fase 3): operaciones de edicion del esqueleto + los
//  transforms modales (G/R/S) de puntas y huesos. Compartido por el viewport 3D,
//  el UV editor (armature 2D: mismos huesos con Z=0) y el harness de tests.
//
//  MODELO DE SELECCION (estilo Blender): se selecciona el hueso ENTERO
//  (b.select, con las dos puntas) o UNA PUNTA (b.selHead / b.selTail). Una punta
//  COMPARTIDA (head del hijo CONECTADO == tail del padre) es UNA sola entidad:
//  seleccionarla o moverla afecta las dos a la vez (siguen soldadas).
//
//  Cada operacion discreta empuja SU paso de undo (UndoBonesCapturar); el
//  transform modal usa el pendiente Iniciar/Confirmar/Cancelar (un paso por
//  drag, nada si no cambio). Tras cada edicion se re-prepara el skin autorado
//  (BoneEditAplicado) para que el rig deforme la malla con el rest nuevo.
// ============================================================================

// NOMBRE LIBRE de hueso dentro del armature ("Bone", "Bone.001", ...). Delega en LA regla
// comun (base/W3dNombres.h). 'excepto' = indice que puede conservar su nombre (-1 = ninguno).
class Object;
std::string BoneNombreLibre(Armature* a, const std::string& base, int excepto);
// los vertex groups homonimos de las mallas ligadas a 'a' (para capturarlos junto al hueso en
// UN SOLO paso de undo). Salen como W3dRenameDest (malla + lista + indice), NO como
// std::string*: el VertexGroup* se libera y se recrea (ver W3dRenameDest en Undo.h).
struct W3dRenameDest;
void JuntarVGroupsHomonimos(Object* nodo, Armature* a, const std::string& nombre, std::vector<W3dRenameDest>& out);

// hay Edit Mode de armature? (InteractionMode==EditMode y el objeto activo es un armature)
bool BoneEditActivo();
Armature* BoneEditArm(); // el armature en edicion (NULL si no aplica)

// --- SELECCION por puntas/huesos (mantienen la punta compartida sincronizada) ---
void BoneSelLimpiar(Armature* a);                       // deselecciona todo (huesos y puntas)
void BoneSelHueso(Armature* a, int b, bool sel);        // hueso ENTERO (select + ambas puntas)
void BoneSelPunta(Armature* a, int b, int punta, bool sel); // una punta (1=head 2=tail) + su soldada
bool BoneEsConectado(Armature* a, int b);               // head del hueso == tail de su padre?
// INVARIANTE: 'conectado' solo puede estar en true si el hueso TIENE padre y su head cae en el tail de ese
// padre (misma tolerancia que BoneEsConectado). Las ops que re-parentan SIN mover el hueso (borrar el del
// medio, Clear Parent, duplicar, el desplegable Parent, Ctrl+P) dejarian el flag mintiendo: esto lo apaga
// donde ya no corresponde. NO mueve nada. Lo llaman esas ops; devuelve true si tuvo que corregir alguno.
bool BoneEditSanearConectado(Armature* a);

// --- operaciones discretas (cada una empuja su undo y devuelve si hizo algo) ---
// E: extruye un hueso nuevo desde CADA punta seleccionada (hueso entero = desde su tail, criterio
// Blender; punta compartida = UNA sola extrusion). Sin seleccion usa el tail del activo. Los nuevos
// quedan con el tail seleccionado (listos para arrastrar). outNuevos (opcional): sus indices.
// Devuelve el indice del nuevo hueso activo o -1.
int  BoneEditExtruir(Armature* a, std::vector<int>* outNuevos = 0);
int  BoneEditDuplicar(Armature* a);      // Shift+D: duplica los seleccionados (sin conectar al original). idx activo nuevo o -1
bool BoneEditBorrar(Armature* a);        // X/Supr: borra seleccionados; los hijos se re-parentan al abuelo (o raiz)
bool BoneEditDesconectar(Armature* a);   // Alt+P > Clear Parent: los seleccionados quedan sin padre (raiz)
bool BoneEditDesconectarConectados(Armature* a); // Alt+P > Disconnect Bone: siguen emparentados pero SIN soldar (conectado=false)
// Ctrl+P (menu de 2 opciones): seleccionados -> hijos del ACTIVO, sin ciclos, 1 solo undo.
// conectar=false -> "Keep Offset" (no se mueve nada, linea punteada al padre);
// conectar=true  -> "Connected" (el hijo se MUEVE para pegar su head al tail del padre + soldado).
bool BoneParentarSeleccionAlActivo(Armature* a, bool conectar = false);
// UNICO camino de conectar/desconectar un hueso de su padre (el flag 'conectado'): lo comparten
// el checkbox "Connected" de la tarjeta Bones, el Ctrl+P > Connected y el Alt+P > Disconnect Bone.
// Conectar SUELDA (mueve el hueso para pegar su head al tail del padre); desconectar solo apaga
// el flag (no mueve nada). Undo adentro; false = no cambio nada (sin padre o ya estaba asi).
bool BoneSetConectado(Armature* a, int bone, bool con);
// A / Alt+A en Edit Mode de armature: seleccionar TODOS los huesos (con sus dos puntas) o ninguno.
bool BoneEditSeleccionarTodos(Armature* a, bool sel);
// re-parenta el hueso (validando que no arme un ciclo). nuevoPadre = -1 -> raiz. Con undo.
bool BoneReparentar(Armature* a, int bone, int nuevoPadre);
// 'candidato' NO puede ser padre de 'bone' (es el mismo o un descendiente) -> true = excluir
bool BoneEsDescendiente(Armature* a, int ancestro, int b);
// renombra el hueso (nombre unico) y el VERTEX GROUP asociado en las mallas del rig (binding por nombre)
bool BoneRenombrar(Armature* a, int idx, const std::string& nuevo);
// la UI (tarjeta Bones) YA escribio el nombre nuevo: sincronizar los vertex groups viejo->nuevo + avisar
void BoneRenombradoDesdeUI(Armature* a, int idx, const std::string& viejo);
// mueve head (mask&1) y/o tail (mask&2) del hueso, en espacio LOCAL del armature. SIN undo (lo envuelve el caller)
void BoneEditMoverExtremo(Armature* a, int bone, int mask, const Vector3& delta);
// post-edicion comun: re-prepara el skin autorado + invalida pose/skin + redraw
void BoneEditAplicado(Armature* a);

// --- GRAB/transform modal (drag de puntas o huesos enteros) ---
// masks[i]: 1=head 2=tail 3=ambos. porClick=true -> el drag termina al SOLTAR el mouse;
// false (toolbar/G) -> queda modal y confirma con click/Enter (Esc cancela).
void BoneGrabStart(Armature* a, const std::vector<int>& bones, const std::vector<int>& masks, bool porClick);
// arranca el drag de LA SELECCION (puntas seleccionadas, o huesos enteros; la punta compartida
// arrastra a su soldada; sin seleccion usa el activo entero)
void BoneGrabStartSeleccion(Armature* a, bool porClick);
bool BoneGrabActivo();
bool BoneGrabPorClick();
void BoneGrabMotion3D(Viewport3D* vp, int mx, int my);   // arrastre en el plano de la vista (3D)
void BoneGrabMotionUV(float du, float dv);               // delta ABSOLUTO (desde el arranque) en UV: X/Y local, Z fijo
void BoneGrabConfirmar();
void BoneGrabCancelar();

// --- TRANSFORM MODAL 3D con ejes y pivote (G/R/S del viewport, Fase 3 pulida) ---
// modo: 1=grab 2=rotate 3=scale. Opera sobre la seleccion alrededor del pivote del viewport
// (g_transformPivot: cursor 3D / activo / mediana / individual). Usa el estado compartido
// (estado/axisSelect/transformOrientation) -> barra de estado, linea de eje y entrada numerica
// funcionan como en Object/Pose. X/Y/Z constrinie (re-apretar cicla Global->Local->View->libre).
void BoneXformStart(Armature* a, int modo);
int  BoneXformModo();                      // 0=no hay 1=grab 2=rotate 3=scale (solo el modal con ejes)
float BoneXformHeaderValor();              // valor para la barra de estado (grab: mag, scale: factor)
void BoneXformNumValor(float v);           // valor numerico exacto tipeado (NumInput)
void BoneXformReaplicar();                 // re-aplica con el eje/orientacion nuevos (toolbar/menu Orient)
void BoneCiclarEje(int eje);               // X/Y/Z (0/1/2): constrinie; re-apretar cicla la orientacion
void BoneCiclarPlano(int eje);             // Shift+X/Y/Z: al plano (grab/scale)
void BoneCiclarOrient();                   // R de nuevo: cicla Global->Local->View
bool BoneEjesMundo(Vector3& ex, Vector3& ey, Vector3& ez); // ejes del transform en MUNDO (para la linea-guia)

// pick de hueso para EDIT (head/tail crudos, no la pose): las PUNTAS tienen prioridad (radio en px);
// si no, el cuerpo. outMask: 1=head 2=tail 3=cuerpo. lmx/lmy en coords LOCALES del viewport.
int BonePickEdit(Viewport3D* vp, Armature* a, float lmx, float lmy, int* outMask);
// click completo del 3D en Edit-de-armature: seleccion explicita de punta/hueso (shift agrega) +
// arranca el drag de LO SELECCIONADO. true = consumio el click (lo llama ScenePick3D).
bool BoneEditClick3D(Viewport3D* vp, int mx, int my, int vx, int vy, bool shift);
// el click que CONFIRMA un grab modal marca esto: el ScenePick3D del mismo evento no re-pickea
void BoneGrabMarcarClickConsumido();
// variantes INTERACTIVAS (teclado/toolbar/menu): extruir/duplicar + dejar lo nuevo agarrado al mouse.
// ejes3d=true (viewport 3D): el grab resultante acepta X/Y/Z y muestra la barra de estado.
void BoneEditExtruirInteractivo(Armature* a, bool ejes3d = false);
void BoneEditDuplicarInteractivo(Armature* a, bool ejes3d = false);

#endif // BONE_EDIT_H
