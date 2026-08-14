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

#endif
