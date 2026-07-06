#include "OpcionesRender.h"

// Definición de variables
RenderType view = RenderType::MaterialPreview;

// fondo del RENDER (pase Rendered): gris oscuro por defecto, editable desde el menu Render (color picker).
float g_renderBg[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

GLfloat MaterialPreviewAmbient[4]  = { 0.3f, 0.3f, 0.3f, 1.0f };
GLfloat MaterialPreviewDiffuse[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat MaterialPreviewSpecular[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat MaterialPreviewPosition[4] = { -0.45f, 0.55f, 1.0f, 0.0f };

bool g_mostrarOverlays = true; // master de overlays (false = limpieza de pantalla, oculta TODO)
// overlay de normales (solo en meshes seleccionadas); apagados por defecto
bool OverlayVertexNormal = false;
bool OverlayCustomNormal = false;
bool OverlayFaceNormal   = false;
float OverlayNormalSize  = 0.10f;

// overlay de estadisticas / fps (apagados por defecto)
bool OverlayStatistics = false;
bool OverlayFps        = false;
float g_fpsActual      = 0.0f;
long g_genMallaCount   = 0; // diagnostico de regeneracion de modificadores (ver OpcionesRender.h)
bool g_objetosMovidos  = false; // un objeto se movio -> regenerar los Mirror con target (ver OpcionesRender.h)

// arranca en true para dibujar el primer frame
bool g_redraw = true;

// ajustes de transformacion del EDITOR (pivot + normales). Default Blender.
int g_transformPivot = PivotMedian;   // pivote por defecto = punto medio
bool g_editLockNormales = false;      // por defecto SI recalcula normales (al confirmar)
float g_mergeDist = 0.0002f;          // Merge > By Distance: suelda verts a <= 0.0002 m. El editor cuantiza los
                                      // verts a una grilla de 1e-4, asi que 0.0002 es el minimo util (justo por
                                      // encima) y alcanza para soldar la tapa de un extrude "dejado en su lugar"
bool g_modRenderMode = false;         // true SOLO durante el render final (Subdivision usa subRenderLevel)

// Implementación de función
RenderType StringToRenderType(const std::string& s){
    if(s == "Solid")            return RenderType::Solid;
    if(s == "MaterialPreview")  return RenderType::MaterialPreview;
    if(s == "Rendered")         return RenderType::Rendered;
    if(s == "ZBuffer")          return RenderType::ZBuffer;
    if(s == "Alpha")            return RenderType::Alpha;

    std::cerr << "[StringToRenderType] WARNING: valor desconocido '" << s
              << "' → usando RenderType::Solid" << std::endl;
    return RenderType::Solid; // fallback seguro
}