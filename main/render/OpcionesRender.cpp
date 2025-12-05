#include "OpcionesRender.h"

// Definición de variables
RenderType view = RenderType::MaterialPreview;

GLfloat MaterialPreviewAmbient[4]  = { 0.3f, 0.3f, 0.3f, 1.0f };
GLfloat MaterialPreviewDiffuse[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat MaterialPreviewSpecular[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat MaterialPreviewPosition[4] = { -0.45f, 0.55f, 1.0f, 0.0f };

// Implementación de función
RenderType StringToRenderType(const std::string& s){
    if(s == "Solid")            return RenderType::Solid;
    if(s == "MaterialPreview")  return RenderType::MaterialPreview;
    if(s == "Rendered")         return RenderType::Rendered;
    if(s == "ZBuffer")          return RenderType::ZBuffer;

    std::cerr << "[StringToRenderType] WARNING: valor desconocido '" << s
              << "' → usando RenderType::Solid" << std::endl;
    return RenderType::Solid; // fallback seguro
}