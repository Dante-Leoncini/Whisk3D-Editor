#ifndef OPCIONESRENDER_H
#define OPCIONESRENDER_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <iostream>
#include <GL/gl.h> // o GLES según corresponda

// interpolación
enum { lineal, closest };

enum class RenderType { Solid, MaterialPreview, Rendered, ZBuffer };

// Declaraciones de variables globales
extern RenderType view;

extern GLfloat MaterialPreviewAmbient[4];
extern GLfloat MaterialPreviewDiffuse[4];
extern GLfloat MaterialPreviewSpecular[4];
extern GLfloat MaterialPreviewPosition[4];

// Declaración de función
RenderType StringToRenderType(const std::string& s);

#endif // OPCIONESRENDER_H