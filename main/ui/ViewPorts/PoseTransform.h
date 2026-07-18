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

void PoseXformStart(int modo);      // G/R/S: arranca el transform (1=mover 2=rotar 3=escalar)
void PoseXformMotion(int mx, int my);
void PoseXformNumValor(float v);    // valor numerico exacto tipeado
void PoseXformConfirm();            // click/Enter/tick
void PoseXformCancel();             // Esc/click-der/cruz
void PoseCiclarEje(int eje);        // X/Y/Z: constrinie a un eje
void PoseCiclarPlano(int eje);      // Shift+X/Y/Z: al plano
void PoseCiclarOrient();            // "R de nuevo": Global->Local->View
bool PoseEjesMundo(Vector3& ex, Vector3& ey, Vector3& ez);
int  PoseHeaderModo();              // 0=none 1=grab 2=rotate 3=scale (para la barra de estado)
void PoseInsertKeyframe();
void PoseClearTransform(int what);
void PoseClearTransformAll();

#endif
