#include "RenderOptions.h"
#include "objects/Textures.h"      // el fondo 360: Texture, su referencia (TextureRetain / TextureRelease)
#include "animation/Animation.h"   // AnimBackground / AnimBackgroundRot / AnimBackgroundTex
#include <math.h>
#include "w3dGraphics.h"            // w3dRenderLights: el pase con las luces (donde se ve el fondo 360)

// Definición de variables
RenderType view = RenderType::MaterialPreview;

// fondo del RENDER (pase Rendered): NEGRO por defecto, editable desde el menu Render (color picker).
float g_renderBg[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
int      g_renderBgMode = 0;
Texture* g_renderBgTex = NULL;
float    g_renderBgRot[3]        = { 0.0f, 0.0f, 0.0f };
float    g_renderLightBackground = 1.0f;
std::vector<Texture*> g_renderBgTexs;
bool W3dBackgroundWithTexture() { return g_renderBgMode == 1 && g_renderBgTex && g_renderBgTex->iID; }
static float gBackgroundAuto[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static bool gBackgroundAutoVisor = false;
void W3dBackgroundAutoSet(const float* c) {
    gBackgroundAutoVisor = (c != NULL);
    if (c) for (int k = 0; k < 4; k++) gBackgroundAuto[k] = c[k];
}
bool W3dBackgroundAutoColor(float c[4]) {
    if (gBackgroundAutoVisor) { for (int k = 0; k < 4; k++) c[k] = gBackgroundAuto[k]; return true; }
    if (W3dBackgroundWithTexture() && w3dRenderLights) return false;
    for (int k = 0; k < 3; k++) c[k] = g_renderBg[k];
    c[3] = 1.0f;
    return true;
}
// el gancho del "Clear color" en Auto (objects/Routine.cpp): lo instala el arranque, como el de la animacion de abajo
extern bool (*g_w3dBackgroundAutoHook)(float*);
static struct BackgroundAutoHook { BackgroundAutoHook() { g_w3dBackgroundAutoHook = W3dBackgroundAutoColor; } } gBackgroundAutoHook;
int W3dBackgroundTextureIndex(Texture* t) {
    if (!t) return 0;
    for (size_t i = 0; i < g_renderBgTexs.size(); i++) if (g_renderBgTexs[i] == t) return (int)i + 1;
    return 0;
}
void W3dBackgroundSetTexture(Texture* t) {
    if (t && !W3dBackgroundTextureIndex(t)) { TextureRetain(t); g_renderBgTexs.push_back(t); }
    g_renderBgTex = t;
}
void W3dBackgroundApplyChannel(int prop, int comp, float v) {
    if (comp < 0 || comp > 2) return;
    if (prop == AnimBackground) g_renderBg[comp] = v < 0.0f ? 0.0f : v;
    else if (prop == AnimBackgroundRot) g_renderBgRot[comp] = v;
    else if (prop == AnimBackgroundTex && comp == 0) {
        const int k = (int)(v + 0.5f);
        if (k <= 0) g_renderBgMode = 0;
        else if (k <= (int)g_renderBgTexs.size()) { g_renderBgMode = 1; g_renderBgTex = g_renderBgTexs[(size_t)(k - 1)]; }
    }
}
void W3dBackgroundMatrix(float m[9]) {
    // R = Rz * Ry * Rx (los grados del fondo) y se usa su TRANSPUESTA: girar la imagen +a es mirar la direccion -a.
    // LA IMAGEN 360 TIENE LA Z ARRIBA (v = acos(z)) y el motor, la Y: antes de girar, la direccion del mundo pasa a la
    // de la imagen (x, -z, y), un giro de 90 grados en X. Sin eso el fondo quedaba ACOSTADO: mirando al horizonte se
    // veia el suelo, y un piso espejo reflejaba el suelo en vez del cielo. (Lo usan todos: los reflejos, la luz del
    // entorno y el trazado de rayos; el cielo dibujado hace el mismo giro en DrawSky)
    const float k = 3.14159265f / 180.0f;
    const float cx = cosf(g_renderBgRot[0] * k), sx = sinf(g_renderBgRot[0] * k);
    const float cy = cosf(g_renderBgRot[1] * k), sy = sinf(g_renderBgRot[1] * k);
    const float cz = cosf(g_renderBgRot[2] * k), sz = sinf(g_renderBgRot[2] * k);
    const float r[9] = { cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
                         sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
                         -sy,     cy * sx,                cy * cx };
    float t[9];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[i * 3 + j] = r[j * 3 + i];
    // m = R^T * S, con S (mundo -> imagen) = (x, -z, y): sus columnas, la x, la z y la -y de R^T
    for (int i = 0; i < 3; i++) { m[i * 3 + 0] = t[i * 3 + 0]; m[i * 3 + 1] = t[i * 3 + 2]; m[i * 3 + 2] = -t[i * 3 + 1]; }
}
void W3dBackgroundDirectionUV(const float d[3], float* u, float* v) {
    float m[9]; W3dBackgroundMatrix(m);
    const float x = m[0] * d[0] + m[1] * d[1] + m[2] * d[2];
    const float y = m[3] * d[0] + m[4] * d[1] + m[5] * d[2];
    float z = m[6] * d[0] + m[7] * d[1] + m[8] * d[2];
    if (z > 1.0f) z = 1.0f; if (z < -1.0f) z = -1.0f;
    *u = 0.5f + atan2f(y, x) / (2.0f * 3.14159265f);
    *v = acosf(z) / 3.14159265f;
}
// el gancho de la animacion: lo instala el arranque (el editor y el juego compilan este archivo)
static struct BackgroundHook { BackgroundHook() { g_animBackgroundHook = W3dBackgroundApplyChannel; } } gBackgroundHook;
void W3dBackgroundClear() {
    g_renderBg[0] = g_renderBg[1] = g_renderBg[2] = 0.0f; g_renderBg[3] = 1.0f;
    g_renderBgMode = 0; g_renderBgTex = NULL;
    g_renderBgRot[0]        = g_renderBgRot[1] = g_renderBgRot[2] = 0.0f;
    g_renderLightBackground = 1.0f;
    for (size_t i = 0; i < g_renderBgTexs.size(); i++) TextureRelease(g_renderBgTexs[i]);
    g_renderBgTexs.clear();
}
// (los valores de fabrica de la tarjeta Render)
W3dRenderOpt::W3dRenderOpt()
    : name("render.png"),
#ifdef W3D_SYMBIAN
      w(320.0f), h(240.0f),   // el N95 rinde a 240p (su pantalla; el trazado de rayos a mas ya no es "esperable")
#else
      w(1280.0f), h(720.0f),  // 720p en PC/Android/Web
#endif
      zbuffer(false), normal(false), alpha(false), vectors(false), blur(false), shutter(0.5f),
      format(0), quality(90.0f), final(true), finalAlpha(false), post(true), sound(true), audioFormat(3), audioKbps(192.0f),
      videoCodec(0), antialias(1),
#ifdef W3D_SYMBIAN
      videoKbps(1000.0f)   // (240p: lo que el RealPlayer del N95 reproduce bien)
#else
      videoKbps(8000.0f)   // (720p)
#endif
      {}
W3dRenderOpt g_render;
unsigned g_renderGen = 0;
float g_renderAspect = 640.0f / 480.0f; // 4:3 por defecto (igual que el default del render 640x480)

GLfloat MaterialPreviewAmbient[4]  = { 0.3f, 0.3f, 0.3f, 1.0f };
GLfloat MaterialPreviewDiffuse[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat MaterialPreviewSpecular[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat MaterialPreviewPosition[4] = { -0.45f, 0.55f, 1.0f, 0.0f };

bool g_showOverlays = true; // master de overlays (false = limpieza de pantalla, oculta TODO)
int g_pureGame = 0;         // MODO JUEGO PURO (VERDE+0): 0 editor / 1 bandas / 2 cover
// overlay de normales (solo en meshes seleccionadas); apagados por defecto
bool OverlayVertexNormal = false;
bool OverlayCustomNormal = false;
bool OverlayFaceNormal   = false;
float OverlayNormalSize  = 0.10f;

// overlay de estadisticas / fps: los 6 flags (OverlayStat*/OverlayFps) son AHORA miembros PER-VIEWPORT de
// Viewport3D (mismo nombre, ver ViewPort3D.h) -> cada viewport prende sus stats por separado. Aca solo quedan
// los datos GLOBALES que alimentan el overlay (los actualiza cada plataforma / el Core).
float g_currentFps      = 0.0f;
// caras (triangulos) y draw calls del ultimo pase de escena del viewport ACTIVO (planos, para el [PERF] sin
// arrastrar ViewPort3D.h). Los setea ViewPort3D::Render; los lee el loop del juego (Whisk3DContainer / [PERF]).
int  g_renderFaces   = 0;
int  g_renderDraws   = 0;
long g_genMeshCount  = 0; // diagnostico de regeneracion de modificadores (ver RenderOptions.h)
bool g_objectsMoved  = false; // un objeto se movio -> regenerar los Mirror/Boolean con target (ver RenderOptions.h)
bool g_meshesEdited  = false; // alguna malla cambio su geometria -> regenerar los Boolean que la usan de target

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
    if(s == "MotionVectors")    return RenderType::Vectors;

    std::cerr << "[StringToRenderType] WARNING: valor desconocido '" << s
              << "' → usando RenderType::Solid" << std::endl;
    return RenderType::Solid; // fallback seguro
}

// (ver RenderOptions.h: la plataforma que necesita dibujar aparte lo pone; NULL = en la ventana)
bool (*W3dOutsideScreenBegin)(int w, int h) = 0;
void (*W3dOutsideScreenFinish)() = 0;
