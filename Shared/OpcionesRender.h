//interpolacion
enum {lineal, closest};

enum class RenderType { Solid, MaterialPreview, Rendered, ZBuffer };

RenderType view = RenderType::MaterialPreview;

RenderType StringToRenderType(const std::string& s){
    if(s == "Solid")            return RenderType::Solid;
    if(s == "MaterialPreview")  return RenderType::MaterialPreview;
    if(s == "Rendered")         return RenderType::Rendered;
    if(s == "ZBuffer")          return RenderType::ZBuffer;

    std::cerr << "[StringToRenderType] WARNING: valor desconocido '" << s
              << "' → usando RenderType::Solid" << std::endl;
    return RenderType::Solid; // fallback seguro
}

//luz material preview
GLfloat MaterialPreviewAmbient[4]  = { 0.3f, 0.3f, 0.3f, 1.0f };
GLfloat MaterialPreviewDiffuse[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat MaterialPreviewSpecular[4]   = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat MaterialPreviewPosition[4]  = { -0.45f, 0.55f, 1.0f, 0.0f };

#include "./Objects/Textures.h"
#include "./UI/UI.h"
#include "./Objects/Materials.h"
#include "./Objects/Objects.h"