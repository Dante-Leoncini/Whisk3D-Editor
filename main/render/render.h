#pragma once

#include "../../engine/w3dBase.h"

#include <vector>
#include <cmath>

#include "variables.h"
#include "GeometriaUI/GeometriaUI.h"
#include "UI/colores.h"
#include "GeometriaUI/Floor.h"
#include "objects/Objects.h"
#include "objects/Textures.h"
/*#ifdef __ANDROID__
#include "render/GLES_Android_helpers.h"
#endif*/

// Funciones de render
void DrawnLines(int LineWidth, int cantidad, GLshort* vertexlines, GLushort* lineasIndices);
void DrawnLines(int LineWidth, int cantidad, const GLshort* vertexlines, const GLushort* lineasIndices);
void RenderLinkLines(Object* obj);
void DrawTransformAxis();
bool RenderAxisTransform(Object* obj);
void DibujarOrigen(Object* obj);
void RenderOrigins();
void DibujarIcono3D(Object* obj);
void RenderIcons3D();
void RenderVK();
