#pragma once

#include "../../libs/Whisk3DCore/base/w3dBase.h"

#include <vector>
#include <cmath>

#ifndef W3D_SYMBIAN
#include "variables.h"
#endif
#include "GeometryUI/GeometryUI.h"
#include "WhiskUI/theme/colors.h"
#include "GeometryUI/Floor.h"
#include "objects/Objects.h"
#include "objects/Textures.h"

// Funciones de render
void RenderLinkLines(Object* obj);
void DrawOrigin(Object* obj);
void RenderOrigins();
void DrawIcon3D(Object* obj);
void RenderIcons3D();
// linea vertical de cada luz al piso, en ESPACIO MUNDO via
// GetGlobalPosition(): inmune a escala/rotacion/jerarquia (el viejo
// metodo bajo la matriz del objeto se deformaba)
void RenderLightLines();
