#ifndef W3D_UNDO_H
#define W3D_UNDO_H

#include <string>

class Mesh;     // capturas de geometria/material
class Material; // modificacion de material (checkbox/shininess)

// ============================================================================
//  UNDO (Ctrl+Z) por COMANDOS — compartido 4 OS. Cubre: transforms (pos/rot/escala
//  en object mode), nombres, cambio de modo edit/objeto, seleccion de objetos,
//  material de mesh part, geometria de malla (extrude/delete/loop/duplicate/assign),
//  mover sub-elementos en edit, y SELECCION de sub-elementos en edit (verts/edges/faces).
//  BORRAR/CREAR objetos todavia LIMPIA el stack (UndoLimpiar: punteros invalidos).
//  Las capturas se llaman ANTES del cambio: guardan el estado PREVIO.
// ============================================================================

void UndoDeshacer(); // Ctrl+Z: deshace el ultimo comando (pasa al stack de redo)
void UndoRehacer();  // Ctrl+Y: rehace el ultimo deshecho (pasa de vuelta al stack de undo)
void UndoLimpiar();  // limpia ambos stacks (al borrar/crear objetos: punteros invalidos)
bool UndoHayAlgo();  // hay algo para deshacer?
bool UndoHayRedo();  // hay algo para rehacer?

void UndoCapturarModo();                        // antes de cambiar Edit/Object
void UndoCapturarRename(std::string* destino);  // antes de escribir un nombre (objeto/material/uv/color)
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

// transform (object mode): pendiente hasta confirmar, asi un transform CANCELADO no deja un undo no-op.
void UndoTransformIniciar();   // captura pos/rot/escala de los seleccionados (al empezar)
void UndoTransformConfirmar(); // al aceptar el transform: pushea el pendiente al stack
void UndoTransformCancelar();  // al cancelar: descarta el pendiente

// mover verts/aristas/caras en EDIT MODE (G/R/S, move PURO sin cambio de topologia): captura las
// posiciones+normales de la malla; pendiente hasta confirmar (mismo patron que el transform de objeto).
void UndoEditMoveIniciar(Mesh* m);
void UndoEditMoveConfirmar();
void UndoEditMoveCancelar();

#endif // W3D_UNDO_H
