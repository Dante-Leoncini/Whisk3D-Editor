#include "OpcionesRender.h"
#include "objects/Textures.h"      // el fondo 360: Texture, su referencia (TexturaRetener / TexturaSoltar)
#include "animation/Animation.h"   // AnimFondo / AnimFondoRot / AnimFondoTex
#include <math.h>

// Definición de variables
RenderType view = RenderType::MaterialPreview;

// fondo del RENDER (pase Rendered): NEGRO por defecto, editable desde el menu Render (color picker).
float g_renderBg[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
int      g_renderBgModo = 0;
Texture* g_renderBgTex = NULL;
float    g_renderBgRot[3] = { 0.0f, 0.0f, 0.0f };
std::vector<Texture*> g_renderBgTexs;
bool W3dFondoConTextura() { return g_renderBgModo == 1 && g_renderBgTex && g_renderBgTex->iID; }
int W3dFondoIndiceTextura(Texture* t) {
    if (!t) return 0;
    for (size_t i = 0; i < g_renderBgTexs.size(); i++) if (g_renderBgTexs[i] == t) return (int)i + 1;
    return 0;
}
void W3dFondoPonerTextura(Texture* t) {
    if (t && !W3dFondoIndiceTextura(t)) { TexturaRetener(t); g_renderBgTexs.push_back(t); }
    g_renderBgTex = t;
}
void W3dFondoAplicarCanal(int prop, int comp, float v) {
    if (comp < 0 || comp > 2) return;
    if (prop == AnimFondo) g_renderBg[comp] = v < 0.0f ? 0.0f : v;
    else if (prop == AnimFondoRot) g_renderBgRot[comp] = v;
    else if (prop == AnimFondoTex && comp == 0) {
        const int k = (int)(v + 0.5f);
        if (k <= 0) g_renderBgModo = 0;
        else if (k <= (int)g_renderBgTexs.size()) { g_renderBgModo = 1; g_renderBgTex = g_renderBgTexs[(size_t)(k - 1)]; }
    }
}
void W3dFondoMatriz(float m[9]) {
    // R = Rz * Ry * Rx (los grados del fondo) y se usa su TRANSPUESTA: girar la imagen +a es mirar la direccion -a
    const float k = 3.14159265f / 180.0f;
    const float cx = cosf(g_renderBgRot[0] * k), sx = sinf(g_renderBgRot[0] * k);
    const float cy = cosf(g_renderBgRot[1] * k), sy = sinf(g_renderBgRot[1] * k);
    const float cz = cosf(g_renderBgRot[2] * k), sz = sinf(g_renderBgRot[2] * k);
    const float r[9] = { cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
                         sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
                         -sy,     cy * sx,                cy * cx };
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[i * 3 + j] = r[j * 3 + i];
}
void W3dFondoDireccionUV(const float d[3], float* u, float* v) {
    float m[9]; W3dFondoMatriz(m);
    const float x = m[0] * d[0] + m[1] * d[1] + m[2] * d[2];
    const float y = m[3] * d[0] + m[4] * d[1] + m[5] * d[2];
    float z = m[6] * d[0] + m[7] * d[1] + m[8] * d[2];
    if (z > 1.0f) z = 1.0f; if (z < -1.0f) z = -1.0f;
    *u = 0.5f + atan2f(y, x) / (2.0f * 3.14159265f);
    *v = acosf(z) / 3.14159265f;
}
// el gancho de la animacion: lo instala el arranque (el editor y el juego compilan este archivo)
static struct FondoGancho { FondoGancho() { g_animFondoHook = W3dFondoAplicarCanal; } } gFondoGancho;
void W3dFondoLimpiar() {
    g_renderBg[0] = g_renderBg[1] = g_renderBg[2] = 0.0f; g_renderBg[3] = 1.0f;
    g_renderBgModo = 0; g_renderBgTex = NULL;
    g_renderBgRot[0] = g_renderBgRot[1] = g_renderBgRot[2] = 0.0f;
    for (size_t i = 0; i < g_renderBgTexs.size(); i++) TexturaSoltar(g_renderBgTexs[i]);
    g_renderBgTexs.clear();
}
// (los valores de fabrica de la tarjeta Render)
W3dRenderOpc::W3dRenderOpc()
    : nombre("render.png"),
#ifdef W3D_SYMBIAN
      w(320.0f), h(240.0f),   // el N95 rinde a 240p (su pantalla; el trazado de rayos a mas ya no es "esperable")
#else
      w(1280.0f), h(720.0f),  // 720p en PC/Android/Web
#endif
      zbuffer(false), normal(false), alfa(false), vectores(false), desenfoque(false), obturador(0.5f),
      formato(0), calidad(90.0f), final(true), finalAlfa(false), post(true), sonido(true), audioFormato(3), audioKbps(192.0f) {}
W3dRenderOpc g_render;
unsigned g_renderGen = 0;
float g_renderAspect = 640.0f / 480.0f; // 4:3 por defecto (igual que el default del render 640x480)

GLfloat MaterialPreviewAmbient[4]  = { 0.3f, 0.3f, 0.3f, 1.0f };
GLfloat MaterialPreviewDiffuse[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat MaterialPreviewSpecular[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat MaterialPreviewPosition[4] = { -0.45f, 0.55f, 1.0f, 0.0f };

bool g_mostrarOverlays = true; // master de overlays (false = limpieza de pantalla, oculta TODO)
int g_juegoPuro = 0;           // MODO JUEGO PURO (VERDE+0): 0 editor / 1 bandas / 2 cover
// overlay de normales (solo en meshes seleccionadas); apagados por defecto
bool OverlayVertexNormal = false;
bool OverlayCustomNormal = false;
bool OverlayFaceNormal   = false;
float OverlayNormalSize  = 0.10f;

// overlay de estadisticas / fps: los 6 flags (OverlayStat*/OverlayFps) son AHORA miembros PER-VIEWPORT de
// Viewport3D (mismo nombre, ver ViewPort3D.h) -> cada viewport prende sus stats por separado. Aca solo quedan
// los datos GLOBALES que alimentan el overlay (los actualiza cada plataforma / el Core).
float g_fpsActual      = 0.0f;
// caras (triangulos) y draw calls del ultimo pase de escena del viewport ACTIVO (planos, para el [PERF] sin
// arrastrar ViewPort3D.h). Los setea ViewPort3D::Render; los lee el loop del juego (Whisk3DContainer / [PERF]).
int  g_renderCaras     = 0;
int  g_renderDraws     = 0;
long g_genMallaCount   = 0; // diagnostico de regeneracion de modificadores (ver OpcionesRender.h)
bool g_objetosMovidos  = false; // un objeto se movio -> regenerar los Mirror/Boolean con target (ver OpcionesRender.h)
bool g_mallasEditadas  = false; // alguna malla cambio su geometria -> regenerar los Boolean que la usan de target

// arranca en true para dibujar el primer frame
bool g_redraw = true;

// ajustes de transformacion del EDITOR (pivot + normales). Default Blender.
int g_transformPivot = PivotMedian;   // pivote por defecto = punto medio
float g_mergeDist = 0.0002f;          // Merge > By Distance: suelda verts a <= 0.0002 m. El editor cuantiza los
                                      // verts a una grilla de 1e-4, asi que 0.0002 es el minimo util (justo por
                                      // encima) y alcanza para soldar la tapa de un extrude "dejado en su lugar"
bool g_modRenderMode = false;         // true SOLO durante el render final (Subdivision usa subRenderLevel)
bool g_autoMerge = false;             // Auto Merge (menu Mesh): OFF por defecto. Al confirmar un move, suelda los verts
float g_autoMergeThreshold = 0.001f;  // movidos con cualquier vert a <= threshold (opt-in; sin esto el snap NO funde)

// Implementación de función
RenderType StringToRenderType(const std::string& s){
    if(s == "Solid")            return RenderType::Solid;
    if(s == "MaterialPreview")  return RenderType::MaterialPreview;
    if(s == "Rendered")         return RenderType::Rendered;
    if(s == "ZBuffer")          return RenderType::ZBuffer;
    if(s == "Alpha")            return RenderType::Alpha;
    if(s == "Wireframe")        return RenderType::Wireframe;
    if(s == "NormalView")       return RenderType::NormalView;
    if(s == "MotionVectors")    return RenderType::Vectores;

    std::cerr << "[StringToRenderType] WARNING: valor desconocido '" << s
              << "' → usando RenderType::Solid" << std::endl;
    return RenderType::Solid; // fallback seguro
}