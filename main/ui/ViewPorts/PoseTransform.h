#ifndef POSETRANSFORM_H
#define POSETRANSFORM_H

/**
 * @file PoseTransform.h
 * @brief Pose Mode: transform interactivo de huesos (G/R/S). Extraido de LayoutInput.
 *
 * Pose Mode: transform interactivo de huesos (G/R/S), extraido de LayoutInput. Ver PoseTransform.cpp.
 * Estas son las entradas PUBLICAS: las llaman el menu Pose + atajos (LayoutInput), controles, ObjectMode, ViewPort3D.
 */
#include "math/Vector3.h"

void PoseXformStart(int mode);      // G/R/S: arranca el transform (1=mover 2=rotar 3=escalar)
void PoseXformMotion(int mx, int my);
void PoseXformNumValue(float v);    // valor numerico exacto tipeado
void PoseXformConfirm();            // click/Enter/tick
void PoseXformCancel();             // Esc/click-der/cruz
void PoseCycleAxis(int axis);       // X/Y/Z: constrinie a un eje
void PoseCyclePlane(int axis);      // Shift+X/Y/Z: al plano
void PoseCycleOrient();             // "R de nuevo": Global->Local->View
bool PoseAxesWorld(Vector3& ex, Vector3& ey, Vector3& ez);
int  PoseHeaderMode();              // 0=none 1=grab 2=rotate 3=scale (para la barra de estado)
void PoseInsertKeyframe(int channels = 0); // canales = mascara KfCanal* (0 = Loc+Rot+Scl)
void PoseDeleteKeyframe(); // Animation > Delete Keyframe en el Modo Pose: los del frame actual de los huesos elegidos
void PoseClearKeyframes(); // Animation > Clear Keyframe en el Modo Pose: todos los de los huesos elegidos
void PoseClearTransform(int what);
void PoseClearTransformAll();

// ===== POSE 2D (armature en el UV editor, Fase 3) =====
// Mismos snapshots/undo/jerarquia que el 3D, pero el delta viene EN EL PLANO X-Y del armature (Z fijo):
// pensado para rigs AUTORADOS con Z=0 (armature "2D"), cuyos huesos viven en el espacio UV de la malla.
//  - Start(1=grab 2=rotate 3=scale): snapshot de los huesos seleccionados + pivote (activo/median).
//  - Delta: ABSOLUTO desde el arranque -> grab usa (du,dv); rotate el angulo (grados, alrededor de Z);
//    scale el factor (X e Y; Z queda 1). Se puede llamar por cada motion (no acumula drift).
//  - Confirm/Cancel: mismos efectos que el 3D (undo por drag + Auto Key si esta prendido).
void Pose2DStart(int mode);
bool Pose2DActive();
int  Pose2DMode();                          // 0=nada 1=grab 2=rotate 3=scale
void Pose2DDelta(float du, float dv, float angDeg, float factor);
void Pose2DConfirm();
void Pose2DCancel();
bool Pose2DPivotXY(float& px, float& py);   // pivote (espacio local del armature = UV en rigs 2D)

#endif
