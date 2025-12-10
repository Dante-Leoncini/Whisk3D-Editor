#ifndef OBJECTMODE_H
#define OBJECTMODE_H

#include "variables.h"
#include "Objects.h"
#include "Instance.h"
#include "animation/Animation.h"

void ReestablecerEstado(bool ClearEstado = true);
void Cancelar();
void EliminarAnimaciones(Object& obj);
void Eliminar(bool IncluirCollecciones = false);
void CalcObjectsTransformPivotPoint(Object* obj);
void SetTransformPivotPoint();
void GuardarMousePos();
void guardarEstadoRec(Object* obj);
bool guardarEstado();
void SetPosicion();
void DuplicatedObject();
void NewInstance();
void SetRotacion(int dx, int dy);
void SetRotacion();
void SetScale(int dx, int dy, float factor = 0.01f);
void SetEscala();
void SetTranslacionObjetos(int dx, int dy, float factor = 1.0f);

#endif