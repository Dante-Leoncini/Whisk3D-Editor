#ifndef RENDER_H
#define RENDER_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glext.h> 
#include <vector>
#include <cmath>

#include "variables.h"
#include "UI/GeometriaUI.h"
#include "UI/colores.h"
#include "Floor.h"
#include "Objects/Objects.h"
#include "GLES_Android_helpers.h"

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

#endif