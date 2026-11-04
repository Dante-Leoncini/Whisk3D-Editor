#ifndef PICK3D_H
#define PICK3D_H

/**
 * @file Pick3D.h
 * @brief Interaccion/seleccion en el viewport 3D: pick por color, pick de malla, Select/Loop Select, Loop Cut. Extraido de LayoutInput.
 *
 * Interaccion/seleccion en el viewport 3D: pick por color (ScenePick3D), pick de malla en Edit Mode,
 * Select Linked / Loop Select (modos guiados) y Loop Cut. Extraido de LayoutInput (Fase 2).
 */

bool EditSelAvanzar(int paso, bool extender);
bool EditSelTodoToggle();
bool EditSelToggleActual();
void LayoutSelectLinked(int mx, int my);
bool LoopSelOrientando();
void LayoutLoopSelectGuiado(); // definida mas abajo (modo guiado: pide click cuando no hay elemento activo);
void LayoutLoopSelectActivo(int tipo);
void LoopSelTecla(int dir);
void LoopSelConfirm();
bool LayoutLoopSelectEnPos(int mx, int my, int vx, int vy, int vw, int vh, int screenH);
bool PickPathGuiado();
void LayoutPickPathIniciar(bool fill);
void LayoutPickPathCancelar();
bool GuiadoUnClickActivo();
void LayoutGuiadoCancelar();
void LayoutSelectLinkedGuiado();

// BOX SELECT del viewport 3D: aplica la caja (en coords de PANTALLA) sobre los objetos
// (Object Mode) o los sub-elementos (Edit Mode). 'tocar' = regla azul (alcanza con rozar);
// false = regla verde (tiene que entrar entero). 'sumar' = suma a la seleccion (Shift).
void BoxSelectAplicar3D(int x0, int y0, int x1, int y1, bool tocar, bool sumar);
bool LoopCutOrientando();
bool LoopCutActivo();
void LoopCutIniciar(int mx, int my);
void LayoutLoopCutDesdeActivo();
void LoopCutOrientConfirmarTeclado();
void LoopCutMotion(int mx, int my);
void LoopCutWheel(int dir);
void LoopCutTecla(int dir);
void LoopCutClickIzq(int mx, int my);
void LoopCutClickDer();
void LoopCutCancelar();
void LoopCutRedoAplicar(int cortes, float factor);
float LoopCutGetFactor();
void LoopCutRenderPreview();
bool ScenePick3D(int mx, int my, int vx, int vy, int vw, int vh, int screenH);

// INSET FACES (I en Edit Mode / menu Face): el modal + el panel redo (la op de malla: Mesh::InsetEdit)
struct InsetOpciones;
bool InsetActivo();
void InsetIniciar(int mx, int my);
void InsetMotion(int mx, int my);
void InsetConfirmar();
void InsetCancelar();
bool InsetTecla(int tecla);                 // W3dK_*: true = la uso (mientras dura el modal, todas)
void InsetRedoAplicar();                    // el panel cambio una opcion: re-inseta desde la malla de antes
InsetOpciones& InsetOpcionesActuales();     // las opciones (las tildes del panel apuntan aca)
void InsetCentroPantalla(float& x, float& y, float& pxPorU);   // (pruebas) el centro de la seleccion en pantalla
// BEVEL (Ctrl+B en Edit Mode / menus Edge y Vertex): el modal + el panel redo (la op de malla: Mesh::BevelEdit)
struct BevelOpciones;
bool BevelActivo();
void BevelIniciar(int mx, int my, int afecta = 1);   // afecta: 1 = aristas, 0 = vertices
void BevelMotion(int mx, int my);
void BevelRueda(int dir);
void BevelConfirmar();
void BevelCancelar();
bool BevelTecla(int tecla);
void BevelRedoAplicar();
BevelOpciones& BevelOpcionesActuales();
void BevelCentroPantalla(float& x, float& y, float& pxPorU);

#endif
