#pragma once

#include "../../libs/Whisk3DCore/base/w3dBase.h"

#include <vector>
#include <cmath>

#ifndef W3D_SYMBIAN
#include "variables.h"
#endif
#include "GeometriaUI/GeometriaUI.h"
#include "WhiskUI/theme/colores.h"
#include "GeometriaUI/Floor.h"
#include "objects/Objects.h"
#include "objects/Textures.h"

// Funciones de render
void RenderLinkLines(Object* obj);
void DrawTransformAxis();
bool RenderAxisTransform(Object* obj);
void DibujarOrigen(Object* obj);
void RenderOrigins();
void DibujarIcono3D(Object* obj);
void RenderIcons3D();
// linea vertical de cada luz al piso, en ESPACIO MUNDO via
// GetGlobalPosition(): inmune a escala/rotacion/jerarquia (el viejo
// metodo bajo la matriz del objeto se deformaba)
void RenderLightLines();
