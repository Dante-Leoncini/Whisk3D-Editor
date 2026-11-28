#ifndef PICK3D_H
#define PICK3D_H

/**
 * @file Pick3D.h
 * @brief Interaccion/seleccion en el viewport 3D: pick por color, pick de malla, Select/Loop Select, Loop Cut. Extraido de LayoutInput.
 *
 * Interaccion/seleccion en el viewport 3D: pick por color (ScenePick3D), pick de malla en Edit Mode,
 * Select Linked / Loop Select (modos guiados) y Loop Cut. Extraido de LayoutInput (Fase 2).
 */

bool EditSelAdvance(int step, bool extend);
bool EditSelAllToggle();
bool EditSelToggleCurrent();
void LayoutSelectLinked(int mx, int my);
bool LoopSelOrienting();
void LayoutLoopSelectGuided(); // definida mas abajo (modo guiado: pide click cuando no hay elemento activo);
void LayoutLoopSelectActive(int type);
void LoopSelKey(int dir);
void LoopSelConfirm();
bool LayoutLoopSelectInPos(int mx, int my, int vx, int vy, int vw, int vh, int screenH);
bool PickPathGuided();
void LayoutPickPathStart(bool fill);
void LayoutPickPathCancel();
bool GuidedUnClickActive();
void LayoutGuidedCancel();
void LayoutSelectLinkedGuided();

// BOX SELECT del viewport 3D: aplica la caja (en coords de PANTALLA) sobre los objetos
// (Object Mode) o los sub-elementos (Edit Mode). 'tocar' = regla azul (alcanza con rozar);
// false = regla verde (tiene que entrar entero). 'sumar' = suma a la seleccion (Shift).
void BoxSelectApply3D(int x0, int y0, int x1, int y1, bool touch, bool add);
bool LoopCutOrienting();
bool LoopCutActive();
void LoopCutStart(int mx, int my);
void LayoutLoopCutFromActive();
void LoopCutOrientConfirmKeyboard();
void LoopCutMotion(int mx, int my);
void LoopCutWheel(int dir);
void LoopCutKey(int dir);
void LoopCutClickLeft(int mx, int my);
void LoopCutClickRight();
void LoopCutCancel();
void LoopCutRedoApply(int cuts, float factor);
float LoopCutGetFactor();
void LoopCutRenderPreview();
// EL CUCHILLO (K en Edit Mode): ver Mesh::KnifeCutEdit. mx,my = coords de ventana
bool KnifeActive();
bool KnifeThrough();
int  KnifePoints();      // los puntos puestos
int  KnifeHoverKind();   // lo que engancha el mouse: KnifePoint::Vertex / Edge / Face, -1 = nada
void KnifeStart(int mx, int my);
void KnifeMotion(int mx, int my);
void KnifeClickLeft(int mx, int my);
void KnifeBack();          // Backspace: saca el ultimo punto
void KnifeToggleThrough(); // Z: corta tambien lo que no se ve
void KnifeConfirm();       // Enter / Espacio
void KnifeCancel();        // Esc / click derecho
void KnifeRenderPreview(); // (el viewport 3D, en 3D)
void KnifeRenderOverlay(class Viewport3D* vp);   // (el viewport 3D, en 2D: el enganche bajo el mouse)
bool ScenePick3D(int mx, int my, int vx, int vy, int vw, int vh, int screenH);

// INSET FACES (I en Edit Mode / menu Face): el modal + el panel redo (la op de malla: Mesh::InsetEdit)
struct InsetOptions;
bool InsetActive();
void InsetStart(int mx, int my);
void InsetMotion(int mx, int my);
void InsetConfirm();
void InsetCancel();
bool InsetKey(int key);                  // W3dK_*: true = la uso (mientras dura el modal, todas)
void InsetGrab(int mx, int my);          // TACTIL: el dedo toca ahi y arrastra desde el valor que ya tenia
void InsetNumber();                      // el valor tipeado (NumInput) pasa a ser el grosor (o la profundidad)
bool& InsetDepthDrag();                  // el dedo maneja la PROFUNDIDAD (el boton de la barra; en la PC, Ctrl)
void InsetRedoApply();                   // el panel cambio una opcion: re-inseta desde la malla de antes
InsetOptions& InsetCurrentOptions();     // las opciones (las tildes del panel apuntan aca)
void InsetScreenCenter(float& x, float& y, float& pxByU);   // (pruebas) el centro de la seleccion en pantalla
// BEVEL (Ctrl+B en Edit Mode / menus Edge y Vertex): el modal + el panel redo (la op de malla: Mesh::BevelEdit)
struct BevelOptions;
bool BevelActive();
void BevelStart(int mx, int my, int affects = 1);   // afecta: 1 = aristas, 0 = vertices
void BevelMotion(int mx, int my);
void BevelWheel(int dir);
void BevelConfirm();
void BevelCancel();
bool BevelKey(int key);
void BevelGrab(int mx, int my);          // TACTIL: el dedo toca ahi y arrastra desde el ancho que ya tenia
void BevelNumber();                      // el valor tipeado (NumInput) pasa a ser el ancho
void BevelRedoApply();
BevelOptions& BevelCurrentOptions();
void BevelScreenCenter(float& x, float& y, float& pxByU);

#endif
