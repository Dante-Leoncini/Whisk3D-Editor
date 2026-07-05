#ifndef OBJECTMODE_H
#define OBJECTMODE_H

#include "objects/Objects.h"
#include "variables.h" // REAL, portable
#ifndef W3D_SYMBIAN
// (Instance/Animation arrastran SDL/GL de PC)
#include "Instance.h"
#include "animation/Animation.h"
#endif

class Mesh; // MoverSeleccionEditLocal opera sobre una malla en Edit Mode

void ReestablecerEstado(bool ClearEstado = true);
void Cancelar();
void EliminarAnimaciones(Object& obj);
void Eliminar(bool IncluirCollecciones = false);
void CalcObjectsTransformPivotPoint(Object* obj);
void SetTransformPivotPoint();
// centro geometrico de la seleccion (para el FOCO '.', distinto del pivote)
Vector3 CentroFocoSeleccion();
// transform global (TRS) por la cadena de padres: world<->local de PUNTOS. Lo usa
// el transform de sub-elementos de malla (editor) sin re-implementar la cadena.
Quaternion RotGlobalDe(Object* o);
Vector3 ScaleGlobalDe(Object* o);
// reposiciona los seleccionados alrededor del pivote tras rotar/escalar (objetos)
void AplicarPivotATransform();
void GuardarMousePos();
void guardarEstadoRec(Object* obj);
bool guardarEstado();
void SetPosicion();
void DuplicatedObject();
void NewInstance();
// Join (Ctrl+J, menu Object): une las mallas seleccionadas dentro del objeto ACTIVO (conserva su transform).
void JoinObjetos();
// Apply (Alt+A, menu Object > Apply): hornea el transform en la malla. what: 0=Location 1=Rotation 2=Scale 3=All.
void AplicarTransform(int what);
// Set Active Object as Camera (Ctrl+Numpad 0): el activo pasa a ser la camara activa. false si NO es una camara.
bool SetActiveObjectAsCamera();
// ctrl+p: emparenta los seleccionados al objeto ACTIVO (conservando la
// posicion global). alt+p: los devuelve a la raiz.
void SetParentSeleccion();
// reparenta conservando lo local (puede saltar) / manteniendo lo global
void ReparentSimple(Object* obj, Object* nuevoPadre);
void ReparentKeepTransform(Object* obj, Object* nuevoPadre);
// reordena junto a otro objeto (drag del outliner)
void MoverJuntoA(Object* obj, Object* ref, bool despues);
void ClearParentSeleccion();
void SetRotacion(int dx, int dy);
void SetRotacion();
// reconstruye el quaternion REAL del objeto activo desde los valores de display
// editados, segun rotMode (euler XYZ / quaternion / axis-angle). onChange.
void SincronizarRotacionActiva();
// rotacion ORBITAL/gimbal (libre): izq/der=camUp, arr/ab=camRight (quaterniones)
void RotarOrbital(int dx, int dy);
// alterna rotacion libre entre trackball (eje de vista) y orbital (V / 0)
void ToggleRotacionOrbital();
// cicla eje/orientacion (X/Y/Z=0/1/2) durante un transform: Global->Local->View->libre
void CiclarEjeTransform(int eje);
// Shift+eje: constriñe a un PLANO (excluye 'eje', mueve en los otros dos)
void CiclarPlanoTransform(int eje);
// eje (X/Y/Z) EN WORLD segun la orientacion actual (global/local/view).
// Lo usa el render para dibujar las guias con la orientacion correcta.
Vector3 EjeOrientado(Object& obj, int a);
void SetScale(int dx, int dy, float factor = 0.01f);
void SetEscala();
void SetTranslacionObjetos(int dx, int dy, float factor = 1.0f);
// entrada numerica: aplica un valor EXACTO al transform de objetos en curso
void SetTransformNumerico(float v);

// EDIT MODE: traslada RIGIDO los verts seleccionados por 'deltaLocal' (coords LOCALES del mesh) + persiste
// (empuja al render, rebordes, normales, preview de modificadores, undo). Lo usan el snap y los campos X/Y/Z
// de posicion del panel de Vertices.
void MoverSeleccionEditLocal(Mesh* m, const Vector3& deltaLocal);

// snap (menu shift+s): mueve la seleccion o el cursor 3D estilo Blender
void SnapSeleccionAlCursor(bool mantenerOffset);
void SnapSeleccionAlActivo();
void SnapSeleccionAlGrid();
void SnapCursorAlGrid();
void SnapCursorAlOrigen();
void SnapCursorALoSeleccionado();
void SnapCursorAlActivo();
// Set Origin (submenu Object): mueve el origen y/o la geometria de las mallas sel
void SetOriginGeometryToOrigin(); // baricentro -> origen (no mueve el objeto)
void SetOriginOriginToGeometry(); // origen -> baricentro (la geometria queda igual)
void SetOriginToCursor();         // origen -> cursor 3D (la geometria queda igual)

#endif