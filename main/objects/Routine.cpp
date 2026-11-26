#include "Routine.h"
#include "objects/Mesh.h"
#include "objects/Textures.h"
#include "objects/MeshResource.h"
#include "objects/MeshFlows.h"
#include "script/W3dScript.h"      // W3dScriptEvent (paso "Llamar Lua")
#include "w3dGraphics.h"
#include "io/JsonW3d.h"            // JVal / JsonNumText: los campos del .w3d
#include "animation/Animation.h"   // AnimStep: los pasos se animan (g_animStepHook)
#include "animation/VertexAnimation.h"   // W3dUpToDatePose: la pose pendiente de "Object pointer"
#include "objects/Camera.h"              // "Skip if camera riel": el riel de la camara activa
#include "w3dTexture.h"                  // TexturesMemory: lo subido a GL, en el log de la precarga
#include "render/SceneRender.h"          // las capturas de pantalla ("Copy screen" y las texturas "pantalla:")
#include "w3dlog.h"
#include "objects/WrittenShader.h"     // el paso "Shader" (un shader escrito por su archivo)
#include "WhiskUI/theme/colors.h"   // el fondo del tema: un "Clear color" viejo con ese color pasa a Auto
#include <map>
#include <math.h>                  // cosf / sinf (el giro de "Matriz del objeto")
#include <stdlib.h>
#include <string.h>
#include <time.h>                  // el reloj del perfil sin el perfilador: clock()
// el FONDO 360 vuelve a dibujarse despues de que una rutina limpia el color (lo instala render/SceneRender.cpp)
void (*g_skyAfterClearHook)(Texture*, const float*) = 0;   // (NULL = el fondo del render; si no, esa imagen y su giro)
bool (*g_w3dBackgroundAutoHook)(float*) = 0;   // el color de "Clear color" en Auto (render/RenderOptions.cpp); false = ninguno

namespace gfx = w3dEngine;

bool (*W3dRoutineValidateHook)(Routine*) = NULL;
#ifdef W3D_NO_EDITOR
bool g_w3dOnlyGame = true;     // el juego compilado: siempre solo-juego
#else
bool g_w3dOnlyGame = false;    // el editor: lo prende el modo VERDE+0 (LayoutPureGameToggle)
#endif
unsigned g_w3dConstructorGen = 1;
bool g_w3dClearColorFixed = false;
bool g_w3dClipViewOn = false;
int g_w3dClipView[4] = { 0, 0, 0, 0 };
unsigned g_w3dClearsColor = 0;

// ---------------------------------------------------------------------------
//  la tabla de los pasos: nombre del .w3d, etiqueta de la interfaz, grupo y que campos usa
// ---------------------------------------------------------------------------
struct StepInfo {
    const char* name; const char* label;
    unsigned char group, numbers, ref, integer, options;
    bool on; const char* labelOn;
    const char* fields[5];
};
static const StepInfo kSteps[StepN] = {
    { "push",          "Push matrix",       GroupTransform, 0, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "pop",       "Pop matrix",        GroupTransform, 0, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "translate",       "Translate",         GroupTransform, 3, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "X", "Y", "Z" } },
    { "rotate",           "Rotate",            GroupTransform, 4, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Angle", "Axis X", "Axis Y", "Axis Z" } },
    { "scale",         "Scale",             GroupTransform, 3, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "X", "Y", "Z" } },
    { "objectMatrix",    "Object matrix",     GroupTransform, 1, RefObject,  IntegerNothing,   OptMatrix,        false, 0, { "Spin Y" } },
    { "array",           "Array on/off",      GroupDraw,     0, RefNothing,    IntegerNothing,   OptArray,         true,  "On", { 0 } },
    { "vertexPointer", "Vertex pointer",    GroupDraw,     0, RefMesh,   IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "normalPointer", "Normal pointer",    GroupDraw,     0, RefMesh,   IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "uvPointer",       "UV pointer",        GroupDraw,     0, RefMesh,   IntegerLayerUV, OptNothing,          false, 0, { 0 } },
    { "colorPointer",  "Color pointer",     GroupDraw,     0, RefMesh,   IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "draw",         "Draw elements",     GroupDraw,     0, RefMesh,   IntegerPart,  OptPrimitive,     false, 0, { 0 } },
    { "pointSize",        "Point size",        GroupDraw,     1, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Pixels" } },
    { "lineWidth",      "Line width",        GroupDraw,     1, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Pixels" } },
    { "children",           "Draw children",     GroupDraw,     0, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "routine",          "Call routine",      GroupDraw,     0, RefRoutine,  IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "texture",         "Texture on/off",    GroupTexture,     0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "bindTexture",    "Bind texture",      GroupTexture,     0, RefTexture, IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "textureMode",     "Texture mode",      GroupTexture,     0, RefNothing,    IntegerNothing,   OptTextureMode,   false, 0, { 0 } },
    { "textureFilter",   "Texture filter",    GroupTexture,     0, RefNothing,    IntegerNothing,   OptFilter,        false, 0, { 0 } },
    { "textureRepeat",  "Texture wrap",      GroupTexture,     0, RefNothing,    IntegerNothing,   OptRepeat,       false, 0, { 0 } },
    { "uvMatrix",        "UV offset",         GroupTexture,     4, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "U", "V", "Scale U", "Scale V" } },
    { "matcap",          "Matcap",            GroupTexture,     0, RefMesh,   IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "activeFog",    "Fog on/off",        GroupFog,      0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "fogMode",      "Fog mode",          GroupFog,      0, RefNothing,    IntegerNothing,   OptFogMode,    false, 0, { 0 } },
    { "fogStart",    "Fog start",         GroupFog,      1, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Start" } },
    { "fogEnd",       "Fog end",           GroupFog,      1, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "End" } },
    { "fogDensity",  "Fog density",       GroupFog,      1, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Density" } },
    { "fogColor",     "Fog color",         GroupFog,      4, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "R", "G", "B", "A" } },
    { "light",             "Lighting on/off",   GroupLights,       0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "activeLight",       "Light on/off",      GroupLights,       0, RefNothing,    IntegerLight,    OptNothing,          true,  "On", { 0 } },
    { "lightColor",        "Light color",       GroupLights,       4, RefNothing,    IntegerLight,    OptLightComponent, false, 0, { "R", "G", "B", "A" } },
    { "lightPosition",     "Light position",    GroupLights,       3, RefNothing,    IntegerLight,    OptLightType,       false, 0, { "X", "Y", "Z" } },
    { "ambientLight",     "Ambient light",     GroupLights,       4, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "R", "G", "B", "A" } },
    { "alphaTest",        "Alpha test on/off", GroupAlpha,        0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "alphaRef",         "Alpha reference",   GroupAlpha,        1, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Reference" } },
    { "color",           "Color",             GroupColor,       4, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "R", "G", "B", "A" } },
    { "vertexColor",    "Vertex color",      GroupColor,       0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "materialColor",   "Material color",    GroupColor,       4, RefNothing,    IntegerNothing,   OptMaterialComp,  false, 0, { "R", "G", "B", "A" } },
    { "brightness",          "Shininess",         GroupColor,       1, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Shininess" } },
    { "shading",       "Smooth shading",    GroupColor,       0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "testZ",           "Depth test",        GroupDepth, 0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "writeZ",       "Depth write",       GroupDepth, 0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "depthFunc",        "Depth function",    GroupDepth, 0, RefNothing,    IntegerNothing,   OptFunctionZ,      false, 0, { 0 } },
    { "biasZ",          "Depth bias",        GroupDepth, 2, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { "Factor", "Units" } },
    { "biasMeters",     "Depth shift (m)",   GroupDepth, 1, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Meters" } },
    { "rangeZ",          "Depth range",       GroupDepth, 2, RefNothing,    IntegerNothing,   OptNothing,          false, 0, { "Near", "Far" } },
    { "clearColor",   "Clear color",       GroupDepth, 4, RefNothing,    IntegerNothing,   OptColorClear, false, 0, { "R", "G", "B", "A" } },
    { "clear",         "Clear",             GroupDepth, 0, RefTexture, IntegerBuffers, OptClearBackground, false, 0, { 0 } },
    { "activeMix",    "Blending on/off",   GroupMix,      0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "mixMode",      "Blend mode",        GroupMix,      0, RefNothing,    IntegerNothing,   OptMixMode,    false, 0, { 0 } },
    { "faces",           "Face culling",      GroupFaces,       0, RefNothing,    IntegerNothing,   OptNothing,          true,  "On", { 0 } },
    { "skipIf",        "Skip if zero",      GroupControl,     1, RefNothing,    IntegerCount, OptNothing,          false, 0, { "Value" } },
    { "skipHidden",    "Skip if hidden",    GroupControl,     0, RefObject,  IntegerCount, OptNothing,          false, 0, { 0 } },
    { "visible",         "Visibility test",   GroupControl,     3, RefMesh,   IntegerPart,  OptNothing,          false, 0, { "Write to", "Box (memory)", "Max distance" } },
    { "lua",             "Call Lua",          GroupControl,     0, RefFunction, IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "textureMix",  "Texture mix",       GroupTexture,     1, RefTexture, IntegerLayerUV, OptMixTex,     true,  "On", { "Mix" } },
    { "skipIfNone", "Skip if no flag",   GroupControl,     2, RefNothing,    IntegerCount, OptNothing,          false, 0, { "Ranges (memory)", "Flags (memory)" } },
    { "posePointer",     "Object pointer",    GroupDraw,     0, RefObject,  IntegerNothing,   OptArray,         false, 0, { 0 } },
    { "drawVisible", "Draw visible",      GroupDraw,     0, RefObject,  IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "skipOutside",     "Skip if outside",   GroupControl,     4, RefObject,  IntegerCount, OptNothing,          false, 0, { "Min X", "Max X", "Min Z", "Max Z" } },
    { "skipRail",      "Skip if camera riel", GroupControl,   0, RefObject,  IntegerCount, OptRail,          false, 0, { 0 } },
    { "skipUnlisted", "Skip if not listed", GroupControl,    0, RefObject,  IntegerCount, OptNothing,          false, 0, { 0 } },
    { "copyScreen",  "Copy screen",       GroupTexture,     0, RefTexture, IntegerNothing,   OptNothing,          false, 0, { 0 } },
    { "shader",          "Shader",            GroupDraw,     4, RefShader,  IntegerNothing,   OptNothing,          true,  "On", { "IOR", "Transmission", "Roughness", "Metallic" } },
};
static const char* const kGroups[GroupN] = { "Transform", "Draw", "Texture", "Fog", "Lights", "Alpha test", "Color",
                                             "Depth & clear", "Blending", "Face culling", "Control" };

// las OPCIONES de los desplegables (el indice es el valor de 'modo')
static const char* const kOptFog[]     = { "Linear", "Exponential", "Exponential squared" };
static const char* const kOptTexture[] = { "Modulate", "Replace", "Normal map (DOT3)", "Alpha only" };
static const char* const kOptFuncZ[]   = { "Less", "Less or equal", "Equal", "Always" };            // gfx::DepthCmp
static const char* const kOptMix[]     = { "Opaque (one, zero)", "Alpha", "Additive", "Additive (by alpha)", "Multiply",
                                           "Screen", "Premultiplied alpha", "Subtract" };           // gfx::Mezcla
static const char* const kOptLightComp[] = { "Ambient", "Diffuse", "Specular" };
static const char* const kOptLightType[] = { "Point", "Directional" };
static const char* const kOptPrim[]      = { "Points", "Lines", "Line strip", "Line loop", "Triangles", "Triangle strip",
                                           "Triangle fan", "Quads (simulated)", "Quad strip (simulated)",
                                           "Polygon (simulated)" };                                 // W3dPrimRoutine
static const char* const kOptFilter[]  = { "Nearest (pixelated)", "Linear" };
static const char* const kOptRepeat[]  = { "Repeat", "Clamp" };
static const char* const kOptMatComp[] = { "Ambient", "Diffuse", "Specular", "Emission" };          // gfx::MatParam
static const char* const kOptArray[]   = { "Vertices", "Normals", "UV", "Colors" };                 // W3dArrayRoutine
static const char* const kOptMatrix[]  = { "Multiply", "Load (camera x object)" };
static const char* const kOptMixTex[] = { "Interpolate (mix)", "Multiply (lightmap)", "Multiply 2x" };   // gfx::MixTextures modo
static const char* const kOptRail[] = { "The camera rides it", "The camera does not ride it" };   // StepSkipRail
static const char* const kOptColorLimp[]      = { "Auto", "Use custom color" };                   // W3dClearColorMode
static const char* const kOptLimpBackground[] = { "Auto", "Clear with color", "Draw custom background" }; // W3dClearBackgroundMode
struct OptInfo { const char* title; const char* const* options; int n; };
static const OptInfo kOpt[OptN] = {
    { "", NULL, 0 },
    { "Mode",      kOptFog,  3 },
    { "Mode",      kOptTexture, 4 },
    { "Function",  kOptFuncZ,   4 },
    { "Mode",      kOptMix,  (int)gfx::MixCount_ },
    { "Component", kOptLightComp, 3 },
    { "Type",      kOptLightType, 2 },
    { "Primitive", kOptPrim,    (int)RPrimN },
    { "Filter",    kOptFilter,  2 },
    { "Wrap",      kOptRepeat, 2 },
    { "Component", kOptMatComp, 4 },
    { "Array",     kOptArray,   (int)ArrayN },
    { "Mode",      kOptMatrix,  2 },
    { "Mode",      kOptMixTex, 3 },
    { "Skip when", kOptRail, 2 },
    { "Mode", kOptColorLimp, 2 },
    { "Background", kOptLimpBackground, 3 },
};
// la primitiva en el .w3d (por nombre: un numero no dice nada al leer el archivo)
static const char* const kPrimName[RPrimN] = { "points", "lines", "lineStrip", "lineLoop", "triangles", "strip",
                                                  "fan", "quads", "quadStrip", "polygon" };

const char* W3dStepName(int t)    { return (t >= 0 && t < StepN) ? kSteps[t].name : "?"; }
const char* W3dStepLabel(int t)  { return (t >= 0 && t < StepN) ? kSteps[t].label : "?"; }
int  W3dStepGroupOf(int t)          { return (t >= 0 && t < StepN) ? kSteps[t].group : GroupControl; }
const char* W3dGroupLabel(int g) { return (g >= 0 && g < GroupN) ? kGroups[g] : "?"; }
int  W3dStepRef(int t)              { return (t >= 0 && t < StepN) ? kSteps[t].ref : RefNothing; }
bool W3dStepUsesOn(int t)            { return t >= 0 && t < StepN && kSteps[t].on; }
const char* W3dStepLabelOn(int t){ return (t >= 0 && t < StepN && kSteps[t].labelOn) ? kSteps[t].labelOn : "On"; }
int  W3dStepInteger(int t)           { return (t >= 0 && t < StepN) ? kSteps[t].integer : IntegerNothing; }
int  W3dStepOptions(int t)         { return (t >= 0 && t < StepN) ? kSteps[t].options : OptNothing; }
bool W3dStepIsColor(int t) {
    return t == StepColor || t == StepFogColor || t == StepLightColor || t == StepLightAmbient ||
           t == StepMaterialColor || t == StepColorClear;
}
int  W3dOptionsN(int l)            { return (l > 0 && l < OptN) ? kOpt[l].n : 0; }
const char* W3dOptionsTitle(int l){ return (l > 0 && l < OptN) ? kOpt[l].title : ""; }
const char* W3dOptionLabel(int l, int i) {
    return (l > 0 && l < OptN && i >= 0 && i < kOpt[l].n) ? kOpt[l].options[i] : "?";
}
const char* W3dPrimitiveName(int p) { return (p >= 0 && p < RPrimN) ? kPrimName[p] : "triangles"; }
static int PrimitiveFromName(const std::string& n) {
    for (int p = 0; p < RPrimN; p++) if (n == kPrimName[p]) return p;
    return RPrimTriangles;
}
int W3dStepNumbers(const W3dStep& p) {
    // "Dibujar elementos": manual = [primero, ultimo]; array = [primero, ultimo, "@array"] (el manual se guarda igual)
    if (p.type == StepDraw) return p.sub == RangeManual ? 2 : (p.sub == RangeArray ? 4 : 0);
    if (p.type == StepClear) return p.mode == ClearBackgroundOwn ? 3 : 0;   // (el giro de su fondo 360)
    return p.type < StepN ? kSteps[p.type].numbers : 0;
}
const char* W3dStepNumberName(const W3dStep& p, int i) {
    if (p.type == StepDraw) return i == 0 ? "First triangle" : (i == 1 ? "Last triangle" : (i == 2 ? "Array" : "Flags"));
    if (p.type == StepClear) return i == 0 ? "Rotation X" : (i == 1 ? "Rotation Y" : (i == 2 ? "Rotation Z" : ""));
    return (p.type < StepN && i >= 0 && i < kSteps[p.type].numbers) ? kSteps[p.type].fields[i] : "";
}
int W3dStepFromName(const std::string& n) {
    for (int t = 0; t < StepN; t++) if (n == kSteps[t].name) return t;
    return -1;
}

W3dStep W3dNewStep(int type) {
    W3dStep p;
    p.type = (unsigned char)type;
    switch (type) {
    case StepScale:        p.n[0].v    = p.n[1].v = p.n[2].v = 1.0f; break;
    case StepRotate:          p.n[2].v = 1.0f; break;                                  // eje Y
    case StepColor:          p.n[0].v  = p.n[1].v = p.n[2].v = p.n[3].v = 1.0f; break;
    case StepColorClear:  p.n[3].v     = 1.0f; break;                                  // negro
    case StepClear:        p.integer   = ClearColor | ClearDepth; break;
    case StepDraw:        p.integer    = -1; p.mode = RPrimTriangles; p.n[1].v = -1.0f; break;   // todas, triangulos
    case StepFogStart:   p.n[0].v      = 50.0f; break;
    case StepFogEnd:      p.n[0].v     = 300.0f; break;
    case StepFogDensity: p.n[0].v      = 0.01f; break;
    case StepFogColor:    p.n[0].v     = p.n[1].v = p.n[2].v = 0.7f; p.n[3].v = 1.0f; break;
    case StepLightColor:       p.mode = 1; p.n[0].v = p.n[1].v = p.n[2].v = p.n[3].v = 1.0f; break;   // difuso blanco
    case StepLightPosition:    p.mode = 1; p.n[1].v = 1.0f; break;                      // direccional, desde arriba
    case StepLightAmbient:    p.n[0].v = p.n[1].v = p.n[2].v = 0.2f; p.n[3].v = 1.0f; break;
    case StepAlphaRef:        p.n[0].v = 0.5f; break;
    case StepMixMode:     p.mode    = gfx::MixAlpha; break;
    case StepTextureFilter:  p.mode = 1; break;                                    // lineal
    case StepMixTextures: p.n[0].v = 0.5f; break;                                  // mitad y mitad
    case StepMaterialColor:  p.mode = 1; p.n[0].v = p.n[1].v = p.n[2].v = p.n[3].v = 1.0f; break;   // difuso blanco
    case StepBrightness:         p.n[0].v = 12.0f; break;
    case StepMatrixUV:       p.n[2].v     = p.n[3].v = 1.0f; break;                      // identidad
    case StepRangeZ:         p.n[1].v     = 1.0f; break;                                  // 0..1 (el normal)
    case StepSizePoint:       p.n[0].v    = 4.0f; break;
    case StepWidthLine:     p.n[0].v      = 1.0f; break;
    case StepBiasZ:         p.n[0].v      = -1.0f; p.n[1].v = -4.0f; break;              // acerca (calcomanias)
    case StepSkipIf: case StepSkipHidden: case StepSkipIfNone: case StepSkipOutside: case StepSkipRail: case StepSkipNoListing: p.integer = 1; break;
    case StepVisible:        p.integer = -1; break;                                    // la malla entera
    case StepArray:          p.mode = ArrayNormals; break;
    default: break;
    }
    return p;
}

// ---------------------------------------------------------------------------
//  las MEMORIAS: bloques fijos de 256 floats (un puntero a uno no se invalida nunca)
// ---------------------------------------------------------------------------
static std::map<std::string, float*>& Memories() { static std::map<std::string, float*> m; return m; }
float* W3dMemory(const std::string& name) {
    std::map<std::string, float*>::iterator it = Memories().find(name);
    if (it != Memories().end()) return it->second;
    float* b = (float*)calloc(W3D_MEMORY_SIZE, sizeof(float));
    Memories()[name] = b;
    return b;
}
bool W3dMemoryExists(const std::string& name) { return Memories().count(name) > 0; }
// "@nombre[i]" (sin [i] = el 0): el nombre y el indice; false si esta mal escrito
static bool SplitRef(const std::string& ref, std::string& name, int& i) {
    if (ref.size() < 2 || ref[0] != '@') return false;
    size_t c = ref.find('[');
    name = ref.substr(1, c == std::string::npos ? std::string::npos : c - 1);
    i = 0;
    if (c != std::string::npos) {
        size_t d = ref.find(']', c);
        if (d == std::string::npos) return false;
        i = atoi(ref.substr(c + 1, d - c - 1).c_str());
    }
    return !name.empty() && i >= 0 && i < W3D_MEMORY_SIZE;
}
const float* W3dMemoryRef(const std::string& ref) {
    std::string name; int i;
    if (!SplitRef(ref, name, i)) return NULL;
    return W3dMemory(name) + i;
}
int W3dMemoryIndex(const std::string& ref) {
    std::string name; int i;
    return SplitRef(ref, name, i) ? i : -1;
}

// la malla 3D de un nombre: un recurso de la biblioteca o el recurso del OBJETO malla con ese nombre
MeshResource* W3dRoutineFindMesh(Object* from, const std::string& name) {
    if (name.empty()) return NULL;
    MeshResource* r = W3dMeshResourceByName(name);
    if (r && !r->deleted) return r;
    Object* o = from ? W3dFindNameFrom(from, name) : NULL;
    if (o && o->getType() == ObjectType::mesh) return ((Mesh*)o)->mesh;
    return NULL;
}

// ---------------------------------------------------------------------------
Routine::Routine(Object* parent, const std::string& name)
    : Object(parent, name, Vector3(0, 0, 0)), editedList(0), activeStep(-1), invalid(false), dirty(true),
      run(false), constructor(false), repeat(1.0f), doneGen(0), arraysSeen(false), preloaded(false), pointersOk(false),
      pointersAge(0), valOk(false), valKnown(false), valSeen(false), valGen(0), valAge(0) {
    noTransform = true;
    for (int m = 0; m < ModeN; m++) { use[m] = -1; withChildren[m] = false; }
    for (int k = 0; k < 4; k++) arraysStart[k] = -1;
}
Routine::~Routine() {
    for (size_t i = 0; i < retained.size(); i++) W3dMeshResourceRelease(retained[i]);
    retained.clear();
    for (size_t i = 0; i < retainedTex.size(); i++) TextureRelease(retainedTex[i]);
    retainedTex.clear();
}

int Routine::CurrentMode() {
    if (w3dRenderWireframe) return ModeWire;
    if (w3dRenderNoLight || w3dRenderNormalColor || w3dRenderAlpha) return ModeZBuffer;
    if (w3dRenderSolid) return ModeSolid;
    if (w3dRenderLights) return ModeRender;
    return ModeMaterial;
}

int Routine::CurrentListIndex() {
    int u = use[CurrentMode()];
    return (u < 0 || u >= ModeN) ? (int)ModeDefault : u;
}
std::vector<W3dStep>& Routine::CurrentList() { return lists[CurrentListIndex()]; }

// la textura por su RUTA (la del cache de texturas: termina en el nombre pedido)
static Texture* FindTexture(const std::string& ref) {
    if (ref.empty()) return NULL;
    for (size_t i = 0; i < Textures.size(); i++) {
        Texture* t = Textures[i];
        if (!t) continue;
        const std::string& p = t->path;
        if (p == ref || (p.size() > ref.size() && p.compare(p.size() - ref.size(), ref.size(), ref) == 0)) return t;
    }
    return NULL;
}

// resuelve UN paso (nombres -> punteros). 'desde' = donde se buscan los objetos. Las mallas se retienen en
// 'retener' (el que llama suelta las viejas DESPUES: si es la misma, no se descarga para volver a cargarla)
static void ResolveStep(W3dStep& p, Object* from, Object* noSoy, std::vector<MeshResource*>* retain,
                         std::vector<Texture*>* texRetain) {
    for (int k = 0; k < 5; k++) p.n[k].p = p.n[k].ref.empty() ? NULL : W3dMemoryRef(p.n[k].ref);
    p.ptr = NULL; p.ptr2 = NULL;
    if (!p.ref2.empty()) p.ptr2 = W3dFindNameFrom(from, p.ref2);
    const int rt = W3dStepRef(p.type);
    if (rt == RefNothing || rt == RefFunction || rt == RefShader || p.ref.empty()) return;   // (un shader: por su archivo, al dibujar)
    if (rt == RefTexture && (p.type == StepCopyScreen || W3dIsCapturePath(p.ref))) {
        // una CAPTURA DE PANTALLA (SceneRender.h): se crea si no existe y no se retiene (es del registro de capturas)
        p.ptr = W3dCaptureTexture(p.ref, true);
        return;
    }
    if (rt == RefTexture) {
        // LA RUTINA RETIENE SUS TEXTURAS (como sus mallas): una referencia propia sobre la que ya este cargada (la de
        // un material, encontrada por sufijo: la MISMA, sin otra copia) o, si nadie la cargo, la que trae el cache.
        // Sin la referencia, un material que el streaming duerme liberaba la textura que la rutina sigue nombrando;
        // y la que la rutina cargaba por su cuenta (TextureTake) no se soltaba nunca: quedaba en la GPU hasta cerrar
        Texture* t = FindTexture(p.ref);
        if (t) TextureRetain(t);
        else t = TextureTake(p.ref);
        if (t) texRetain->push_back(t);
        p.ptr = t;
        return;
    }
    if (rt == RefMesh) {
        MeshResource* r = W3dRoutineFindMesh(from, p.ref);
        if (r && retain) {
            if (!W3dMeshResourceRetain(r)) r = NULL;
            else retain->push_back(r);
        }
        // (un nombre de OBJETO malla, de los .w3d viejos: queda el del recurso, que es lo que se dibuja)
        if (r && r->name != p.ref) p.ref = r->name;
        p.ptr = r;
        return;
    }
    Object* o = W3dFindNameFrom(from, p.ref);
    if (rt == RefRoutine) p.ptr = (o && o->getType() == ObjectType::routine && o != noSoy) ? o : NULL;
    else                 p.ptr = o;
}

// (en RAM o, si la malla solto su RAM, en su VBO: MeshResource.h, onlyGPU)
static const unsigned kArrayMask[ArrayN] = { W3DMD_POS, W3DMD_NOR, W3DMD_UV, W3DMD_COL };
static bool MeshHasArray(const MeshResource* r, int k) {
    return W3dMeshResourceHasArray(r, kArrayMask[k]);
}
// la capa UV k (> 0) a la que puede apuntar el puntero de UV: no vacia en RAM o, onlyGPU, la que subio a su VBO
static bool HasLayerUV(const MeshResource* r, int k) {
    if (!r || k <= 0) return false;
    if (r->onlyGPU) return W3dMeshResourceHasLayerUV(r, k);
    return k < (int)r->uvExtra.size() && !r->uvExtra[(size_t)k].empty();
}
W3dStep* Routine::StepById(unsigned id) {
    if (!id) return NULL;
    for (int m = 0; m < ModeN; m++)
        for (size_t i = 0; i < lists[m].size(); i++) if (lists[m][i].id == id) return &lists[m][i];
    return NULL;
}
void Routine::Resolve() {
    preloaded  = false;
    pointersOk = false;
    g_w3dRoutinesGen++;
    // los pasos NUEVOS (id 0: recien agregados o de un archivo viejo) toman un id libre de la rutina
    {
        unsigned mx = 0;
        for (int m = 0; m < ModeN; m++)
            for (size_t i = 0; i < lists[m].size(); i++) if (lists[m][i].id > mx) mx = lists[m][i].id;
        for (int m = 0; m < ModeN; m++)
            for (size_t i = 0; i < lists[m].size(); i++) if (!lists[m][i].id) lists[m][i].id = (unsigned short)++mx;
    }
    std::vector<MeshResource*> newValue;
    std::vector<Texture*> newTex;
    for (int m = 0; m < ModeN; m++) {
        withChildren[m] = false;
        for (size_t i = 0; i < lists[m].size(); i++) {
            W3dStep& p = lists[m][i];
            if (p.type == StepChildren) withChildren[m] = true;
            ResolveStep(p, this, this, &newValue, &newTex);
        }
        // (version 1) el "Array on" de un puntero viejo: si la malla no tiene ese array, queda apagado (como antes)
        for (size_t i = 0; i + 1 < lists[m].size(); i++) {
            W3dStep& p = lists[m][i];
            const W3dStep& q = lists[m][i + 1];
            if (p.type != StepArray || p.sub != 1 || q.type != StepPointerVertices + p.mode || !q.ptr) continue;
            p.on = MeshHasArray((const MeshResource*)q.ptr, p.mode);
            p.sub = 0;
        }
    }
    for (size_t i = 0; i < retained.size(); i++) W3dMeshResourceRelease(retained[i]);
    retained.swap(newValue);
    // (las texturas igual: las nuevas ya se retuvieron, recien ahora se sueltan las viejas -la misma no se descarga-)
    for (size_t i = 0; i < retainedTex.size(); i++) TextureRelease(retainedTex[i]);
    retainedTex.swap(newTex);
    dirty = false;
}

// ---------------------------------------------------------------------------
//  EL EJECUTOR. Estado de UNA corrida (la rutina del arbol y lo que llame adentro: subrutinas, hijos, lua)
// ---------------------------------------------------------------------------
static int   gProf = 0;                 // profundidad de rutinas corriendo (0 = afuera de toda rutina)
static unsigned gCurrentIbo = 0;        // el IBO bindeado (0xFFFFFFFF = no se sabe)
static bool  gVboInUse = false;         // algun puntero quedo en un VBO (al terminar se desbindea)
static int   gPlanesState = 0;          // 0 sin armar, 1 listos, -1 no cortar (orto / sin vista)
static float gPlanes[24];
static float gCamPos[3];                // (con los planos) de donde mira: la distancia maxima de "visible"
static bool  gAlphaOn = false;          // el alpha test es UN estado con dos pasos (on/off y la referencia)
static float gAlphaRef = 0.5f;
static Routine* gInProgress = NULL;        // la rutina cuya lista se esta ejecutando (lua: step())
// "Matriz del objeto" en modo CARGAR: la modelview con la que arranco la corrida (la camara) se lee una vez; cada
// objeto carga camara x objeto (una llamada en vez de apilar + multiplicar + desapilar) y al terminar se repone
static Matrix4 gBaseMV;
static bool gBaseMVRead = false, gMVLoaded = false;
// la camara sola: una rutina ANIDADA en un objeto con transformacion (la de un prefab, adentro de su instancia)
// arranca con la matriz del padre ya aplicada; "cargar" es camara x objeto, asi que la del padre se saca una vez
static Matrix4 gCamMV;
static Routine* gRootRun = NULL;   // la rutina que abrio la corrida (la de un paso "rutina" corre en la misma)
static void RestoreMV() { if (gMVLoaded) { gfx::LoadMatrix(gBaseMV.m); gMVLoaded = false; } }
// CACHE DE PUNTEROS: la malla a la que apunta cada array (vertices, normales, uv, colores). Apuntar de nuevo al mismo
// array no llama al driver (63 instancias de la misma pieza: sus punteros se ponen una vez). Se olvida al empezar
// cada corrida y despues de lo que puede poner otros punteros (los hijos, lua). Prender o apagar un array NO es
// cosa del puntero: es el paso "Array on/off" (se prende UNA vez y queda; el cache de gfx no repite)
static const MeshResource* gPtrMesh[4] = { 0, 0, 0, 0 };
static int gPtrLayerUV = 0;   // (la capa de UV a la que apunta el de UV: 0 = la de render)
static void ForgetPointers() { for (int k = 0; k < 4; k++) gPtrMesh[k] = 0; }
// PUNTEROS DIFERIDOS: el paso "puntero" solo ANOTA a que malla quiere apuntar; el driver recibe el bind + el puntero
// recien antes de un draw (Mandar). Una rutina que apunta y despues no dibuja nada (una celda del mapa con todas sus
// partes fuera de cuadro) no le cuesta nada al driver: en el mundo abierto eran la mitad de las llamadas de GL del
// cuadro (en el N95 cada una es CPU del driver). Se anotan los datos (no el paso: los de step() de lua son temporales)
struct PointerPend { int type; MeshResource* mesh; int integer; bool has; };
static PointerPend gPend[4];
static bool gHasPend = false;
static void PendClear() { for (int k = 0; k < 4; k++) gPend[k].has = false; gHasPend = false; }

// UN DIBUJO PENDIENTE ENTRE PASOS (solo con g_routineHoleTris > 0): los rangos visibles de un "Dibujar elementos" con
// banderas no salen enseguida; si el proximo de la MISMA parte sigue cerca (las tandas de banderas de un material del
// mundo trozado: una memoria tiene 256) se suman al mismo draw. Lo que no toca GL entre medio (los saltos, los "Test de
// visibilidad", fijar la MISMA textura) no lo corta; cualquier otro paso, y el fin de la corrida, lo mandan antes
struct DrawingPend { bool has; W3dFlow f; const MeshResource* r; int part, mode, x0, y0; unsigned tex; };
static DrawingPend gDrawPend = { false, W3dFlow(), 0, 0, 0, 0, 0, 0u };
static unsigned gTexRoutine  = 0xFFFFFFFFu;   // la textura que puso el ultimo "Fijar textura" de la corrida (o no se sabe)
static void SendTo(const W3dFlow& f, int from, int count);
static void DrawPendSend() {
    if (!gDrawPend.has) return;
    gDrawPend.has = false;
    SendTo(gDrawPend.f, gDrawPend.x0 * 3, (gDrawPend.y0 - gDrawPend.x0 + 1) * 3);
}
// el TIEMPO de las rutinas (ms acumulados de las corridas de afuera: las anidadas van adentro). Lo leen el [PERF] del
// N95 y benchjuego: cuanto del render es armar la escena con rutinas
double g_routinesMs = 0.0;
static double gRunFrom = 0.0;
static void OpenRun() {
    if (gProf++ > 0) return;
    { extern double W3dNowMs(); gRunFrom = W3dNowMs(); }
    gCurrentIbo = 0xFFFFFFFFu; gVboInUse = false; gPlanesState = 0;
    gBaseMVRead   = gMVLoaded = false;
    gDrawPend.has = false; gTexRoutine = 0xFFFFFFFFu;
    ForgetPointers();
    PendClear();
}
static void CloseRun() {
    if (--gProf > 0) return;
    DrawPendSend();
    { extern double W3dNowMs(); g_routinesMs += W3dNowMs() - gRunFrom; }
    if (gVboInUse) gfx::UnbindVBOs();
    gVboInUse = false; gCurrentIbo = 0xFFFFFFFFu;
    ForgetPointers();
    PendClear();
    RestoreMV();
    // (opcion de proyecto "mallasSoloGPU") las mallas que esta corrida subio y solo dibujan rutinas sueltan su RAM:
    // recien aca, cuando la corrida ya armo sus flujos y sus cajas. Sin anotadas no cuesta nada
    W3dMeshesOnlyGPUReleasePending();
}

// un PUNTERO: el array de esa malla (VBO si lo tiene, si no RAM). Solo apunta (glXxxPointer): el array lo prende o lo
// apaga el paso "Array on/off". Una malla sin ese array no cambia nada (el editor no deja dibujar con ese array
// prendido apuntando ahi). El de UV puede apuntar a una CAPA de la malla (entero k > 0: uvExtra[k]): la misma malla
// dibujada con otras UV sin otra copia de posiciones (los 39 rivales: una carroceria, una capa por auto)
static void PointerOf(int type, MeshResource* r, int integer) {
    const int k = type - StepPointerVertices;
    const int layer = (type == StepPointerUV && integer > 0 && HasLayerUV(r, integer)) ? integer : 0;
    if (type == StepPointerUV && integer > 0 && !layer) return;   // (una capa que la malla no tiene)
    // (la malla tiene el array en RAM o, si solto su RAM -onlyGPU-, en su VBO)
    if (!r || !(layer || MeshHasArray(r, k))) return;
    if (gPtrMesh[k] == r && (type != StepPointerUV || gPtrLayerUV == layer)) return;   // (ya apunta ahi: ni bind ni puntero)
    gPtrMesh[k] = r;
    if (type == StepPointerUV) gPtrLayerUV = layer;
    unsigned h = 0;
    if (W3dMeshResourceEnsureVBO(r))
        h = type == StepPointerVertices ? r->vboPos : type == StepPointerNormals ? r->vboNor :
            type == StepPointerUV ? (layer ? (layer < (int)r->vboUVx.size() ? r->vboUVx[(size_t)layer] : 0u) : r->vboUV) : r->vboCol;
    // sin VBO para ese array (no deberia pasar con una malla onlyGPU): de RAM, que se recupera y ya no se suelta
    if (!h && r->onlyGPU) { r->ramFixed = true; W3dMeshResourceEnsureRAM(r); }
    const void* data = type == StepPointerVertices ? (const void*)r->vertex :
                        type == StepPointerNormals ? (const void*)r->normals :
                        type == StepPointerUV ? (layer ? (layer < (int)r->uvExtra.size() && !r->uvExtra[(size_t)layer].empty() ?
                                                         (const void*)&r->uvExtra[(size_t)layer][0] : NULL) : (const void*)r->uv) :
                        (const void*)r->vertexColor;
    if (!h && !data) return;
    if (h) {
        switch (type) {
        case StepPointerVertices: gfx::VertexVBO(h); break;
        case StepPointerNormals: gfx::NormalVBO(h); break;
        case StepPointerUV:       gfx::TexCoordVBO(h); break;
        default:                  gfx::ColorVBO(h); break;
        }
        gVboInUse = true;
    } else {
        if (gVboInUse) { gfx::UnbindVBOs(); gVboInUse = false; gCurrentIbo = 0; ForgetPointers(); gPtrMesh[k] = r; }
        switch (type) {
        case StepPointerVertices: gfx::VertexPointer3f(0, r->vertex); break;
        case StepPointerNormals: gfx::NormalPointer3b(r->normals); break;
        case StepPointerUV:       gfx::TexCoordPointer2f(0, (const GLfloat*)data); break;
        default:                  gfx::ColorPointer4ub(r->vertexColor); break;
        }
    }
}
static void Pointer(int type, const W3dStep& p) { PointerOf(type, (MeshResource*)p.ptr, p.integer); }
static void PointerAnnotate(const W3dStep& p) {
    PointerPend& e = gPend[p.type - StepPointerVertices];
    e.type = p.type; e.mesh = (MeshResource*)p.ptr; e.integer = p.integer; e.has = true;
    gHasPend = true;
}
static void ApplyPointers() {
    if (!gHasPend) return;
    for (int k = 0; k < 4; k++)
        if (gPend[k].has) { gPend[k].has = false; PointerOf(gPend[k].type, gPend[k].mesh, gPend[k].integer); }
    gHasPend = false;
}
static const gfx::VArray kArrayGfx[ArrayN] = { gfx::VertexArray, gfx::NormalArray, gfx::TexCoordArray, gfx::ColorArray };

// manda un flujo (del IBO o de RAM), recortado a los indices [desde, desde + cuantos) si cuantos >= 0
static void SendTo(const W3dFlow& f, int from, int count) {
    int n = f.count, first = f.first;
    const MeshIndex* idx = f.idx;
    if (!f.ibo && !idx) return;   // (ni IBO ni indices en RAM: una malla onlyGPU que no pudo volver a subir su IBO)
    if (count >= 0) { n = count; first += from; idx += from; }
    if (n <= 0) return;
    ApplyPointers();   // (los punteros anotados: recien ahora, que se dibuja)
    if (f.ibo) {
        if (gCurrentIbo != f.ibo) { gfx::BindIndexVBO(f.ibo); gCurrentIbo = f.ibo; }
        gfx::DrawElementsVBO((gfx::Primitive)f.primGL, n, first);
    } else {
        if (gCurrentIbo != 0) { gfx::BindIndexVBO(0); gCurrentIbo = 0; }
        gfx::DrawElements((gfx::Primitive)f.primGL, n, idx);
    }
}
// EL HUECO QUE SE DIBUJA IGUAL: dos rangos visibles de un "Dibujar elementos" con banderas que estan a menos de estos
// triangulos van en UNA llamada, con lo oculto del medio (la GPU lo descarta al recortar). Cuanto conviene depende del
// aparato: en la VideoCore IV del Galaxy Ace cada draw le cuesta al driver ~0.2 ms de CPU (busca cada buffer y textura en
// una tabla) y la GPU le sobra; en el N95 los triangulos de mas pesan. 0 = solo los seguidos (lo de siempre). Lo pone la
// plataforma (el main del juego compilado)
int g_routineHoleTris = 0;
static void Draw(const W3dStep& p) {
    MeshResource* r = (MeshResource*)p.ptr;
    W3dMeshResourceEnsureVBO(r);   // (el IBO de la malla: los indices de la parte)
    W3dFlow f;
    if (!W3dMeshFlow(r, p.integer, p.mode, f)) return;
    const int tris = f.count / 3;
    if (p.sub != RangeArray) DrawPendSend();   // (lo pendiente va antes)
    if (p.sub == RangeManual) {
        const int a = (int)p.n[0].Value();
        int b = (int)p.n[1].Value(); if (b < 0) b = tris - 1;
        SendTo(f, a * 3, (b - a + 1) * 3);
    } else if (p.sub == RangeArray) {
        // [cantidad, primero0, ultimo0, primero1, ultimo1...] (ultimo < 0 = el ultimo de la parte). Con BANDERAS (n[3],
        // otra memoria) cada rango lleva la suya: [cantidad, primero, ultimo, bandera, ...] y se dibujan los de bandera
        // prendida. Los seguidos (uno empieza donde termino el anterior) van en UNA llamada, y tambien los separados por
        // un hueco de hasta g_routineHoleTris triangulos
        const float* a = p.n[2].p;
        const float* on = p.n[3].p;
        const int k = (int)a[0], width = on ? 3 : 2;
        if (g_routineHoleTris > 0) {   // (al dibujo pendiente: puede seguir en el proximo paso)
            DrawingPend& d = gDrawPend;
            if (d.has && (d.r != r || d.part != p.integer || d.mode != p.mode)) DrawPendSend();
            for (int j = 0; j < k; j++) {
                const float* e = a + 1 + width * j;
                if (on && on[(int)e[2]] == 0.0f) continue;
                const int x = (int)e[0];
                int y = (int)e[1]; if (y < 0) y = tris - 1;
                if (d.has && x > d.y0 && x - (d.y0 + 1) <= g_routineHoleTris) { if (y > d.y0) d.y0 = y; continue; }
                DrawPendSend();
                d.has = true; d.f = f; d.r = r; d.part = p.integer; d.mode = p.mode; d.x0 = x; d.y0 = y; d.tex = gTexRoutine;
            }
            return;
        }
        int x0 = -1, y0 = -1;
        for (int j = 0; j < k; j++) {
            const float* e = a + 1 + width * j;
            if (on && on[(int)e[2]] == 0.0f) continue;
            const int x = (int)e[0];
            int y = (int)e[1]; if (y < 0) y = tris - 1;
            if (x0 >= 0 && x > y0 && x - (y0 + 1) <= g_routineHoleTris) { if (y > y0) y0 = y; continue; }
            if (x0 >= 0) SendTo(f, x0 * 3, (y0 - x0 + 1) * 3);
            x0 = x; y0 = y;
        }
        if (x0 >= 0) SendTo(f, x0 * 3, (y0 - x0 + 1) * 3);
    } else {
        SendTo(f, 0, -1);
    }
}

// los triangulos de un "Dibujar elementos" SIN recorte: el rango de Dibujar sin mirar las banderas (son el recorte).
// Cuenta de las partes de la malla (lo que no se suelta con mallasSoloGPU) y no arma ningun flujo
static long TrisNoClip(const W3dStep& p) {
    const MeshResource* r = (const MeshResource*)p.ptr;
    if (!r || r->facesSize <= 0) return 0;
    const int tris = ((p.integer >= 0 && p.integer < (int)r->parts.size()) ? r->parts[(size_t)p.integer].count
                                                                          : r->facesSize) / 3;
    if (p.sub == RangeManual) {
        const int a = (int)p.n[0].Value();
        int b = (int)p.n[1].Value(); if (b < 0) b = tris - 1;
        return (b >= a) ? b - a + 1 : 0;
    }
    if (p.sub == RangeArray) {
        const float* a = p.n[2].p;
        if (!a) return 0;
        const int k = (int)a[0], width = p.n[3].p ? 3 : 2;
        long n = 0;
        for (int j = 0; j < k; j++) {
            const float* e = a + 1 + width * j;
            int y = (int)e[1]; if (y < 0) y = tris - 1;
            if (y >= (int)e[0]) n += y - (int)e[0] + 1;
        }
        return n;
    }
    return tris;
}

void W3dRoutineCount(Routine* rt, int& verts, int& tris) {
    verts = tris = 0;
    if (!rt || rt->dirty) return;   // (sucia: los punteros pueden ser de mallas que ya no estan; los resuelve al dibujarse)
    const std::vector<W3dStep>& L = rt->CurrentList();
    std::vector<const MeshResource*> views;
    long t = 0;
    for (size_t i = 0; i < L.size(); i++) {
        if (L[i].type != StepDraw || !L[i].ptr) continue;
        t += TrisNoClip(L[i]);
        const MeshResource* r = (const MeshResource*)L[i].ptr;
        bool already = false;
        for (size_t j = 0; j < views.size() && !already; j++) already = (views[j] == r);
        if (!already) { views.push_back(r); verts += r->vertexSize; }
    }
    const int turns = (rt->repeat > 1.0f) ? (int)rt->repeat : 1;
    tris = (int)(t * turns);
}

// el objeto malla de un paso (NULL si el nombre no es de una malla)
static Mesh* MeshObject(const W3dStep& p) {
    Object* o = (Object*)p.ptr;
    return (o && o->getType() == ObjectType::mesh) ? (Mesh*)o : NULL;
}
// "Object pointer": un array del OBJETO. La capa que sigue compartida con su malla de la biblioteca va por el VBO de la
// malla (como "Vertex pointer" de esa malla: el mismo cache); la que el objeto copio (la pose de su vertex anim, los
// colores que le cambio lua, las UV de su animacion de UV) va de su RAM: un puntero de cliente no re-especifica ningun
// buffer. La pose pendiente de una vertex anim se calcula aca (W3dUpToDatePose): lo que no se dibuja no interpola
static void PointerPose(const W3dStep& p) {
    Mesh* m = MeshObject(p);
    if (!m) return;
    const int k = (p.mode >= 0 && p.mode < ArrayN) ? p.mode : ArrayVertices;
    gPend[k].has = false;   // (la pose GANA a un puntero anotado antes para el mismo array)
    if (k == ArrayVertices || k == ArrayNormals || k == ArrayUV) W3dUpToDatePose(m);
    if (k == ArrayVertices) m->drawnPass = g_w3dPass;   // (se dibuja este cuadro: un espejo la puede reflejar)
    const bool skin = (m->skinArmature != NULL);
    // (la pose al dia: con la piel rigida RenderObject no deforma vertices y skinVertex podia quedar de antes)
    if (skin && (k == ArrayVertices || k == ArrayNormals)) { extern void SkinMesh(Mesh*); SkinMesh(m); }
    { static int dbg = 0; if (k == ArrayUV && (m->name == "Wumpa_22" || m->name == "Banderines") && dbg++ < 4)
        printf("DBGUV %s comp=%u uv=%p rec=%p uv0=%f,%f rec0=%f,%f flip=%p\n", m->name.c_str(), (unsigned)m->dataComp, (void*)m->uv,
               (void*)(m->mesh ? m->mesh->uv : 0), m->uv ? m->uv[0] : -1.f, m->uv ? m->uv[1] : -1.f,
               (m->mesh && m->mesh->uv) ? m->mesh->uv[0] : -1.f, (m->mesh && m->mesh->uv) ? m->mesh->uv[1] : -1.f, (void*)m->flipbook); }
    if (m->mesh && (m->dataComp & kArrayMask[k]) && !skin) {
        W3dStep q = p;
        q.ptr = m->mesh;
        Pointer(StepPointerVertices + k, q);
        return;
    }
    gPtrMesh[k] = NULL;   // (el proximo puntero de malla tiene que volver a apuntar)
    switch (k) {
    case ArrayVertices: { const GLfloat* v = (skin && m->skinVertex) ? m->skinVertex : m->vertex; if (v) gfx::VertexPointer3f(0, v); break; }
    case ArrayNormals: { const GLbyte* n = (skin && m->skinNormals) ? m->skinNormals : m->normals; if (n) gfx::NormalPointer3b(n); break; }
    case ArrayUV:       if (m->uv) gfx::TexCoordPointer2f(0, m->uv); break;
    default:            if (m->vertexColor) gfx::ColorPointer4ub(m->vertexColor); break;
    }
}
// "Draw visible": la lista de la celda del modificador Oclusion (indices de cliente, como el dibujo clasico: cambiar
// de celda no sube nada) o, sin lista, la malla entera desde el IBO de su recurso
static void DrawVisible(const W3dStep& p) {
    Mesh* m = MeshObject(p);
    if (!m) return;
    if (m->pvsFaces) {
        if (m->pvsGroups.size() != m->materialsGroup.size()) return;
        // los rangos seguidos van en UNA llamada (una malla de una textura: una sola)
        int a = -1, n = 0;
        for (size_t g = 0; g < m->pvsGroups.size(); g++) {
            const MaterialGroup& grp = m->pvsGroups[g];
            if (grp.indicesDrawnCount <= 0) continue;
            if (a >= 0 && grp.startDrawn == a + n) { n += grp.indicesDrawnCount; continue; }
            if (a >= 0) gfx::DrawTrianglesClientIdx(n, &m->pvsFaces[a]);
            a = grp.startDrawn; n = grp.indicesDrawnCount;
        }
        for (size_t r = 0; r < m->pvsRuns.size(); r++) {
            const Mesh::PvsRun& run = m->pvsRuns[r];
            if (run.count <= 0) continue;
            if (a >= 0 && run.start == a + n) { n += run.count; continue; }
            if (a >= 0) gfx::DrawTrianglesClientIdx(n, &m->pvsFaces[a]);
            a = run.start; n = run.count;
        }
        if (a >= 0) gfx::DrawTrianglesClientIdx(n, &m->pvsFaces[a]);
        gCurrentIbo = 0;   // (los indices de cliente dejaron el ELEMENT_ARRAY en 0)
        return;
    }
    // el IBO del recurso SOLO si la malla comparte sus caras con el: una malla con caras propias (en Edit, o recien
    // salida con la publicacion diferida) tiene otra topologia, y los indices del recurso sobre SUS vertices dibujan
    // basura o leen fuera del array (un extrude/delete cambia la cantidad de vertices)
    MeshResource* r = (m->dataComp & W3DMD_FACES) ? m->mesh : NULL;
    W3dFlow f;
    if (r && W3dMeshResourceEnsureVBO(r) && W3dMeshFlow(r, -1, RPrimTriangles, f)) { SendTo(f, 0, -1); return; }
    if (m->faces && m->facesSize >= 3) {
        gfx::DrawTrianglesClientIdx(m->facesSize, m->faces);
        gCurrentIbo = 0;
    }
}

// los planos de la camara los da Culling.cpp por hook (lo instala su constructor estatico): un juego 2D compila la
// Rutina sin el pase 3D (BindsJuego usa sus memorias) y sin hook "Test de visibilidad" no corta nada
bool (*g_routinePlanesHook)(float planes[24], float* posCamera) = 0;
bool (*g_routineAabbPlanesHook)(const float planes[24], const Vector3& mn, const Vector3& mx) = 0;

// la caja de la parte (movida por la matriz de un objeto, si hay) contra los planos de la camara
static bool Visible(const W3dStep& p) {
    if (gPlanesState == 0) gPlanesState = (g_routinePlanesHook && g_routinePlanesHook(gPlanes, gCamPos)) ? 1 : -1;
    if (gPlanesState < 0) return true;
    // la caja: la de la parte de la malla o, sin malla, 6 numeros de una memoria (minimo y maximo: las celdas de un mundo
    // unido, que no son partes)
    float mn[3], mx[3];
    const float* c = p.n[1].p;
    if (!p.ptr && c) { for (int k = 0; k < 3; k++) { mn[k] = c[k]; mx[k] = c[3 + k]; } }
    else if (!W3dMeshPartBox((MeshResource*)p.ptr, p.integer, mn, mx)) return true;
    Vector3 a(mn[0], mn[1], mn[2]), b(mx[0], mx[1], mx[2]);
    if (p.ptr2) {
        Matrix4 W; ((Object*)p.ptr2)->GetWorldMatrix(W);
        Vector3 c0(1e30f, 1e30f, 1e30f), c1(-1e30f, -1e30f, -1e30f);
        for (int k = 0; k < 8; k++) {
            Vector3 e = W * Vector3((k & 1) ? mx[0] : mn[0], (k & 2) ? mx[1] : mn[1], (k & 4) ? mx[2] : mn[2]);
            if (e.x < c0.x) c0.x = e.x;
            if (e.y < c0.y) c0.y = e.y;
            if (e.z < c0.z) c0.z = e.z;
            if (e.x > c1.x) c1.x = e.x;
            if (e.y > c1.y) c1.y = e.y;
            if (e.z > c1.z) c1.z = e.z;
        }
        a = c0; b = c1;
    }
    // la distancia maxima (0 = sin limite): de la camara al punto mas cercano de la caja. Lo chico y lejano no se
    // manda (en una pantalla de 320 x 240 son pocos pixeles y cada triangulo cuesta lo mismo)
    const float lim = p.n[2].Value();
    if (lim > 0.0f) {
        const float q0[3] = { a.x, a.y, a.z }, q1[3] = { b.x, b.y, b.z };
        float d2 = 0.0f;
        for (int k = 0; k < 3; k++) {
            const float e = (gCamPos[k] < q0[k]) ? q0[k] - gCamPos[k] : ((gCamPos[k] > q1[k]) ? gCamPos[k] - q1[k] : 0.0f);
            d2 += e * e;
        }
        if (d2 > lim * lim) return false;
    }
    return g_routineAabbPlanesHook(gPlanes, a, b);
}

bool g_routineProfile = false;
long g_routineProfileTimes[StepN], g_routineProfileGL[StepN];
// el TIEMPO (ms acumulados) de los pasos Dibujar y por rutina (inclusivo: una rutina adentro de otra suma en las dos), con el
// reloj de relojMs (script/BindsGame.cpp: el del perfilador o clock()). En el N95 el reloj es de 1 ms: un paso suelto
// marca 0 o 1, pero la suma de muchos cuadros da el promedio
double g_routineProfileMs[StepN];
std::map<std::string, double> g_routineProfileMsOf;
std::map<std::string, long> g_routineProfileTrisOf;   // los triangulos que mando cada rutina (inclusivo)
extern double (*g_w3dClockMsHook)();
static double ClockProfile() {
    return g_w3dClockMsHook ? g_w3dClockMsHook() : (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}
static int CallsGL() {
    return gfx::g_statDrawTris + gfx::g_statTexBinds + gfx::g_statStateChanges + gfx::g_otherStat;
}

// UN paso. 'i' es el indice en la lista (Saltar si lo avanza). 'yo' = la rutina duenia de la lista
static bool gShaderRoutinePlaced = false;   // un paso "Shader" dejo un programa puesto (se saca al terminar la rutina)
static void RunRawStep(W3dStep& p, size_t& i, Routine* yo);
static void RunStep(W3dStep& p, size_t& i, Routine* yo) {
#ifndef W3D_SYMBIAN
    // (harness 'gltraza sec') de quien es cada draw de una rutina: el nombre de lo que dibuja
    if (gfx::g_traceGLSec && (p.type == StepDraw || p.type == StepDrawVisible || p.type == StepMatrixObject))
        printf("      [glsec] -- %s:%s\n", W3dStepName(p.type), p.ref.c_str());
#endif
    if (!g_routineProfile) { RunRawStep(p, i, yo); return; }
    const int prev = CallsGL();
    const bool nests = (p.type == StepRoutine || p.type == StepChildren || p.type == StepLua);
    // el tiempo, solo de los DIBUJAR (ahi trabaja el driver): leer el reloj en los ~2000 pasos chicos de un cuadro costaba
    // mas que ellos (en el N95 cada lectura es una llamada al kernel). Lo que no es dibujar sale de la rutina entera
    const bool measure = (p.type == StepDraw || p.type == StepDrawVisible);
    const double t0 = measure ? ClockProfile() : 0.0;
    RunRawStep(p, i, yo);
    g_routineProfileTimes[p.type]++;
    if (!nests) g_routineProfileGL[p.type] += CallsGL() - prev;   // (lo de adentro lo cuentan sus propios pasos)
    if (measure) g_routineProfileMs[p.type] += ClockProfile() - t0;
}
static void RunRawStep(W3dStep& p, size_t& i, Routine* yo) {
    if (gDrawPend.has) {
        const bool still = p.type == StepDraw || p.type == StepVisible || p.type == StepSkipIf ||
                           p.type == StepSkipIfNone || p.type == StepSkipHidden || p.type == StepSkipOutside ||
                           p.type == StepSkipRail || p.type == StepSkipNoListing ||
                           (p.type == StepSetTexture && p.ptr && ((Texture*)p.ptr)->iID == gDrawPend.tex);
        if (!still) DrawPendSend();
    }
    switch (p.type) {
    // ---- transformar ----
    case StepStack:    gfx::PushMatrix(); break;
    case StepPop: gfx::PopMatrix(); break;
    case StepTranslate: gfx::Translatef(p.n[0].Value(), p.n[1].Value(), p.n[2].Value()); break;
    case StepRotate:     gfx::Rotatef(p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value()); break;
    case StepScale:   gfx::Scalef(p.n[0].Value(), p.n[1].Value(), p.n[2].Value()); break;
    case StepMatrixObject: {
        Matrix4 w; ((Object*)p.ptr)->GetWorldMatrix(w);
        // el GIRO en Y (grados, fijo o de una memoria: los anillos que giran todos juntos) va en la misma matriz: una
        // llamada al driver en vez de cargar + rotar
        const float turn = p.n[0].Value();
        if (turn != 0.0f) {
            const float r = turn * 3.14159265f / 180.0f, c = cosf(r), s = sinf(r);
            Matrix4 ry;   // (column-major, como GL: la rotacion de glRotatef(giro, 0, 1, 0))
            for (int i = 0; i < 16; i++) ry.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
            ry.m[0] = c; ry.m[2] = -s; ry.m[8] = s; ry.m[10] = c;
            w = w * ry;
        }
        if (p.mode == 0) { gfx::MultMatrix(w.m); break; }
        if (!gBaseMVRead) {
            gfx::GetMatrix(gfx::ModelView, gBaseMV.m); gBaseMVRead = true;
            gCamMV = gBaseMV;
            Object* parent = gRootRun ? gRootRun->Parent : NULL;
            if (parent && parent != SceneCollection) {
                Matrix4 wp, inv; parent->GetWorldMatrix(wp);
                if (Matrix4::InvertAffine(wp, inv)) gCamMV = gBaseMV * inv;
            }
        }
        Matrix4 m = gCamMV * w;
        gfx::LoadMatrix(m.m); gMVLoaded = true;
        break;
    }
    // ---- dibujar ----
    case StepArray: if (p.on) gfx::EnableArray(kArrayGfx[p.mode]); else gfx::DisableArray(kArrayGfx[p.mode]); break;
    case StepPointerVertices: case StepPointerNormals: case StepPointerUV: case StepPointerColors:
        PointerAnnotate(p); break;
    case StepDraw: Draw(p); break;
    case StepPointerPose: PointerPose(p); break;
    case StepDrawVisible: DrawVisible(p); break;
    case StepSizePoint:  gfx::PointSize(p.n[0].Value()); break;
    case StepWidthLine: gfx::LineWidth(p.n[0].Value()); break;
    case StepChildren:
        if (yo) {
            RestoreMV();   // (los hijos se apilan sobre la camara)
            yo->RenderChildren();
            // los hijos pudieron bindear y desbindear buffers, poner SUS punteros y prender o apagar arrays: no se
            // sabe que quedo (hay que volver a apuntar; el editor pide fijar los arrays antes del proximo draw)
            gCurrentIbo = 0xFFFFFFFFu; gVboInUse = true;
            ForgetPointers();
        }
        break;
    case StepRoutine: {
        Routine* r = (Routine*)p.ptr;
        if (r->dirty) r->Resolve();
        Routine* prev = gInProgress; gInProgress = r;
        r->Run(r->CurrentList());
        gInProgress = prev;
        break;
    }
    // ---- textura ----
    case StepTexture:        if (p.on) gfx::Enable(gfx::Texture2D); else gfx::Disable(gfx::Texture2D); break;
    case StepSetTexture:   gTexRoutine = ((Texture*)p.ptr)->iID; gfx::BindTexture(gTexRoutine); break;
    case StepCopyScreen: W3dCaptureCopyNow(p.ref); break;
    case StepShader: {   // el SHADER ESCRITO del archivo: los triangulos de despues lo usan (prendido) o no (apagado)
        const int sh = p.on ? W3dShaderFile(p.ref) : 0;
        if (sh) {
            float q[gfx::ShaderWrittenParams];
            W3dShaderParameters(q, p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value());
            gfx::WrittenShaderUse(sh, q);
            gShaderRoutinePlaced = true;
        } else gfx::WrittenShaderUse(0, NULL);
        break;
    }
    case StepTextureMode:
        // (DOT3: el color de vertice es la direccion de la luz, horneada; solo alfa: RGB del color, alfa de la textura por el del color)
        if (p.mode == 2) gfx::TexEnvDot3(true); else if (p.mode == 3) gfx::TexEnvAlphaOnly(true); else gfx::TexEnvReplace(p.mode == 1);
        break;
    case StepTextureFilter:  gfx::TexFilter(p.mode == 1); break;
    case StepTextureRepeat: gfx::TexWrap(p.mode == 0); break;
    case StepMixTextures: {   // la unidad 1 con la textura B (ref): Interpolate (B * mix + unidad0 * (1 - mix)), Multiply o
        // Multiply 2x (un lightmap en la misma pasada). 'entero' = la capa UV de la malla del ultimo puntero de UV con la
        // que se muestrea B (0 = las mismas UV que la unidad 0)
        Texture* tx = (Texture*)p.ptr;
        if (!p.on || !tx || !tx->iID) { gfx::MixTexturesOff(); break; }
        gfx::MixTextures(tx->iID, p.n[0].Value(), true, true, p.mode);
        MeshResource* r = (MeshResource*)gPtrMesh[ArrayUV];
        if (p.integer > 0 && HasLayerUV(r, p.integer)) {
            const unsigned h = (gVboInUse && p.integer < (int)r->vboUVx.size()) ? r->vboUVx[(size_t)p.integer] : 0u;
            // (de RAM sin el VBO: una malla onlyGPU la recupera y ya no la suelta)
            if (!h && r->onlyGPU) { r->ramFixed = true; W3dMeshResourceEnsureRAM(r); }
            if (h) gfx::MixTexturesUVVBO(h);
            else if (p.integer < (int)r->uvExtra.size() && !r->uvExtra[(size_t)p.integer].empty())
                gfx::MixTexturesUV(&r->uvExtra[(size_t)p.integer][0]);
            else gfx::MixTexturesUV(NULL);
        } else gfx::MixTexturesUV(NULL);
        break;
    }
    case StepMatrixUV: gfx::TexMatrixUV(p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value()); break;
    case StepMatcap: {
        // las NORMALES de la malla como UV (de RAM: el GL de PC no acepta bytes en un VBO de UV) y la matriz que las
        // lleva al matcap con la modelview de AHORA: va despues de la matriz del objeto, antes de dibujar
        MeshResource* r = (MeshResource*)p.ptr;
        // (las normales se leen de RAM en cada cuadro: una malla onlyGPU las recupera y ya no las suelta)
        if (p.on && r && r->onlyGPU && W3dMeshResourceHasArray(r, W3DMD_NOR)) { r->ramFixed = true; W3dMeshResourceEnsureRAM(r); }
        if (!p.on || !r || !r->normals) { gfx::TexMatrixMatcap(false); break; }
        if (gVboInUse) { gfx::UnbindVBOs(); gVboInUse = false; gCurrentIbo = 0; ForgetPointers(); }
        gfx::TexCoordPointer3b(r->normals, r->vertexSize);
        gPtrMesh[ArrayUV] = NULL;
        gfx::TexMatrixMatcap(true);
        break;
    }
    // ---- niebla ----
    case StepFog:         if (p.on) gfx::Enable(gfx::Fog); else gfx::Disable(gfx::Fog); break;
    case StepFogMode:     gfx::FogMode(p.mode); break;
    case StepFogStart:   gfx::FogStart(p.n[0].Value()); break;
    case StepFogEnd:      gfx::FogEnd(p.n[0].Value()); break;
    case StepFogDensity: gfx::FogDensity(p.n[0].Value()); break;
    case StepFogColor: { float c[4] = { p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value() }; gfx::FogColor(c); break; }
    // ---- luces ----
    case StepLight:  if (p.on) gfx::Enable(gfx::Lighting); else gfx::Disable(gfx::Lighting); break;
    case StepLightN: gfx::LightN(p.integer, p.on); break;
    case StepLightColor: {
        static const gfx::LightFv kComp[3] = { gfx::LightAmbient, gfx::LightDiffuse, gfx::LightSpecular };
        float c[4] = { p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value() };
        gfx::LightNfv(p.integer, kComp[p.mode], c);
        break;
    }
    case StepLightPosition: {
        // (en el espacio de la modelview ACTIVA, como glLightfv: w 0 = direccional, la direccion HACIA la luz)
        float v[4] = { p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.mode == 1 ? 0.0f : 1.0f };
        gfx::LightNfv(p.integer, gfx::LightPosition, v);
        break;
    }
    case StepLightAmbient: { float c[4] = { p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value() }; gfx::LightModelAmbient(c); break; }
    // ---- alpha test ----
    case StepAlphaTest: gAlphaOn = p.on; gfx::AlphaTest(p.on ? gAlphaRef : 0.0f); break;
    case StepAlphaRef:  gAlphaRef = p.n[0].Value(); if (gAlphaOn) gfx::AlphaTest(gAlphaRef); break;
    // ---- color y material ----
    case StepColor: gfx::Color4f(p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value()); break;
    case StepColorVertex: if (p.on) gfx::Enable(gfx::ColorMaterial); else gfx::Disable(gfx::ColorMaterial); break;
    case StepMaterialColor: {
        float c[4] = { p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value() };
        gfx::Material((gfx::MatParam)p.mode, c);
        break;
    }
    case StepBrightness: gfx::MaterialShininess(p.n[0].Value()); break;
    case StepShading: gfx::SmoothShading(p.on); break;
    // ---- profundidad y buffers ----
    case StepTestZ:     if (p.on) gfx::Enable(gfx::DepthTest); else gfx::Disable(gfx::DepthTest); break;
    case StepWriteZ: gfx::DepthMask(p.on); break;
    case StepFunctionZ:  gfx::DepthFunc((gfx::DepthCmp)p.mode); break;
    case StepBiasZ:
        if (p.on) {
            float u = p.n[1].Value();
#ifdef W3D_SYMBIAN
            u *= 4.0f;   // z de 16 bits (N95): el mismo sesgo necesita ~4x mas unidades (como el material)
#endif
            gfx::Enable(gfx::PolygonOffsetFill); gfx::PolygonOffset(p.n[0].Value(), u);
        } else { gfx::PolygonOffset(0.0f, 0.0f); gfx::Disable(gfx::PolygonOffsetFill); }
        break;
    case StepBiasMeters: gfx::BiasMeters(p.n[0].Value()); break;   // (la proyeccion corrida: estado de gfx)
    case StepRangeZ: gfx::DepthRange(p.n[0].Value(), p.n[1].Value()); break;
    case StepColorClear:
        if (g_w3dClearColorFixed) break;
        if (p.mode == ColorClearAuto && g_w3dBackgroundAutoHook) {
            // AUTO: el fondo (el del render; en el editor, fuera de la vista Render, el del visor). Con un fondo 360 no
            // hace nada: lo tapa el cielo que dibuja el "Clear"
            float c[4];
            if (g_w3dBackgroundAutoHook(c)) gfx::ClearColor(c[0], c[1], c[2], c[3]);
        } else gfx::ClearColor(p.n[0].Value(), p.n[1].Value(), p.n[2].Value(), p.n[3].Value());
        break;
    case StepClear: {
        // glClear de lo elegido. Respeta el scissor (en el editor: el rectangulo del viewport). La profundidad se limpia
        // aunque la escritura de z este apagada (glClear la respetaria y no limpiaria nada): queda prendida
        int bits = 0;
        if ((p.integer & ClearColor) && !g_w3dClearColorFixed) { bits |= gfx::ColorBuffer; g_w3dClearsColor++; }
        if (p.integer & ClearDepth) { bits |= gfx::DepthBuffer; gfx::DepthMask(true); }
        if (p.integer & ClearStencil) bits |= gfx::StencilBuffer;
        if (bits && g_w3dClipViewOn) {
            gfx::Enable(gfx::ScissorTest);
            gfx::Scissor(g_w3dClipView[0], g_w3dClipView[1], g_w3dClipView[2], g_w3dClipView[3]);
        }
        if (bits) gfx::Clear(bits);
        // el FONDO 360 atras de lo que se dibuje despues (limpiar el color lo borraba): en AUTO el del render, si hay uno;
        // con un fondo PROPIO, su imagen con su giro. "Limpiar con el color": nada mas
        // (por un gancho: el juego 2D compila las rutinas pero no el pase 3D, que es quien lo instala)
        if ((bits & gfx::ColorBuffer) && p.mode != ClearBackgroundColor && g_skyAfterClearHook) {
            if (p.mode == ClearBackgroundOwn) {
                const float turn[3] = { p.n[0].Value(), p.n[1].Value(), p.n[2].Value() };
                if (p.ptr) g_skyAfterClearHook((Texture*)p.ptr, turn);
            } else g_skyAfterClearHook(NULL, NULL);
        }
        break;
    }
    // ---- mezcla ----
    case StepMix:     if (p.on) gfx::Enable(gfx::Blend); else gfx::Disable(gfx::Blend); break;
    case StepMixMode: gfx::SetMix(p.mode); break;
    // ---- sueltos y control ----
    case StepFaces: if (p.on) gfx::Enable(gfx::CullFace); else gfx::Disable(gfx::CullFace); break;
    case StepSkipIf: if (p.n[0].Value() == 0.0f) i += (size_t)(p.integer > 0 ? p.integer : 0); break;
    case StepSkipIfNone: {   // [cantidad, (primero, ultimo, bandera) x k] + las banderas: ninguna prendida -> saltear
        const float* a = p.n[0].p;
        const float* on = p.n[1].p;
        if (!a || !on) break;
        const int k = (int)a[0];
        bool any    = false;
        for (int j = 0; j < k && !any; j++) if (on[(int)a[1 + 3 * j + 2]] != 0.0f) any = true;
        if (!any) i += (size_t)(p.integer > 0 ? p.integer : 0);
        break;
    }
    case StepSkipHidden: {
        const Object* o = (const Object*)p.ptr;
        while (o && o->visible) o = o->Parent;
        if (o) i += (size_t)(p.integer > 0 ? p.integer : 0);
        break;
    }
    case StepVisible: *(float*)p.n[0].p = Visible(p) ? 1.0f : 0.0f; break;
    case StepSkipNoListing: {
        const Object* o = (const Object*)p.ptr;
        if (o && o->listingRail == 0) i += (size_t)(p.integer > 0 ? p.integer : 0);
        break;
    }
    case StepSkipRail: {
        const bool va = CameraActive && CameraActive->Rail && (Object*)CameraActive->Rail == (Object*)p.ptr;
        if (va == (p.mode == 0)) i += (size_t)(p.integer > 0 ? p.integer : 0);
        break;
    }
    case StepSkipOutside: {
        const Object* o = (const Object*)p.ptr;
        if (!o) break;
        const Vector3 w = o->GetGlobalPosition();
        if (w.x < p.n[0].Value() || w.x > p.n[1].Value() || w.z < p.n[2].Value() || w.z > p.n[3].Value())
            i += (size_t)(p.integer > 0 ? p.integer : 0);
        break;
    }
    case StepLua:
        // la funcion del script de la RUTINA, o del objeto que eligio el paso
        W3dScriptEvent(p.ptr2 ? (Object*)p.ptr2 : (Object*)yo, p.ref.c_str(), (Object* const*)NULL, 0, (const float*)NULL, 0);
        ForgetPointers();   // (step() puede haber apuntado a otras mallas: igual pasa por este mismo cache)
        break;
    default: break;
    }
}

// la precarga de las mallas de una lista (ver RenderObject)
static void PreloadMeshes(std::vector<W3dStep>& L, int prof) {
    for (size_t i = 0; i < L.size(); i++) {
        // (opcion "mallasSoloGPU") tambien las de sus SUBRUTINAS: una malla que suelta su RAM al cerrar esta corrida
        // ya no puede armar el flujo derivado (una tira) que una subrutina dibuja recien mas adelante
        if (L[i].type == StepRoutine && L[i].ptr && prof < 8 && W3dMeshesOnlyGPUActive()) {
            Routine* sub = (Routine*)L[i].ptr;
            if (sub->dirty) sub->Resolve();
            PreloadMeshes(sub->CurrentList(), prof + 1);
            continue;
        }
        if (W3dStepRef(L[i].type) != RefMesh || !L[i].ptr) continue;
        MeshResource* m = (MeshResource*)L[i].ptr;
        // (opcion "mallasSoloGPU") el matcap lee las normales de RAM en cada cuadro: esa malla no la suelta (y si ya la
        // habia soltado, la recupera). Un flujo derivado (tira, aristas...) nuevo se arma de los indices: se relee y
        // se vuelve a soltar al cerrar la corrida. (ramFixed no cambia nada con la opcion apagada)
        if (L[i].type == StepMatcap) { m->ramFixed = true; if (m->onlyGPU) W3dMeshResourceEnsureRAM(m); }
        if (m->onlyGPU && L[i].type == StepDraw && !W3dMeshFlowReady(m, L[i].integer, L[i].mode))
            W3dMeshResourceEnsureRAM(m);
        const long vboPrev = gfx::VBOMemory();
        W3dMeshResourceEnsureVBO(m);
        // lo que se subio y cuanto hay en GL (el N95 comparte esa memoria entre texturas y VBO, y un glBufferData que no
        // entra falla en silencio: si el telefono se cae, la ultima linea dice hasta donde llego)
        const long vbo = gfx::VBOMemory();
        if (vbo != vboPrev)
            w3dLogf("[mallas] '%s': VBO %ld KB (en GL: texturas %ld KB, VBO %ld KB)", m->name.c_str(),
                    (vbo - vboPrev) / 1024, gfx::TexturesMemory().bytes / 1024, vbo / 1024);
        W3dFlow f;
        if (L[i].type == StepDraw) W3dMeshFlow(m, L[i].integer, L[i].mode, f);
    }
}

void Routine::RenderObject() {
    run = false;
    // (las caras de una sonda de reflejo de tiempo real: una rutina pone su camara, limpia la pantalla o corre su codigo
    //  una vez por cuadro; la sonda dibuja la escena desde su centro, sin ellas)
    if (gfx::w3dPassProbe) return;
    if (dirty) Resolve();
    // el EDITOR no ejecuta una rutina mal armada (el nucleo, si: es responsabilidad de quien la armo). Valida con los
    // arrays como estan en este punto del cuadro
    if (W3dRoutineValidateHook) {
        for (int k = 0; k < ArrayN; k++) arraysStart[k] = (signed char)gfx::ArrayState(kArrayGfx[k]);
        arraysSeen = true;
        if (!W3dRoutineValidateHook(this)) return;
    }
    run = true;
    if (!preloaded) {
        // los VBO y los indices de TODAS sus mallas (tambien las de los bloques que hoy se saltean: el humo, los rivales
        // cercanos): subirlos cuando aparecen era un tiron a mitad de la carrera
        preloaded = true;
        PreloadMeshes(CurrentList(), 0);
    }
    const double t0 = g_routineProfile ? ClockProfile() : 0.0;
    const long tris0 = (long)gfx::g_opaqueStatTris + gfx::g_statTrisBlend;
    OpenRun();
    if (gProf == 1) gRootRun = this;
    Routine* prev = gInProgress; gInProgress = this;
    int times = (int)(repeat + 0.5f); if (times < 1) times = 1; if (times > 100000) times = 100000;
    if (times == 1) Run(CurrentList());
    else {
        // BUCLE: la lista entera N veces, con el estado que dejo la vuelta anterior (la matriz se acumula)
        float* mem = W3dMemory("loop");
        mem[1] = (float)times;
        for (int k = 0; k < times; k++) { mem[0] = (float)k; Run(CurrentList()); }
    }
    gInProgress = prev;
    CloseRun();
    if (g_routineProfile) {
        g_routineProfileMsOf[name] += ClockProfile() - t0;
        g_routineProfileTrisOf[name] += (long)gfx::g_opaqueStatTris + gfx::g_statTrisBlend - tris0;
    }
}

// con "Dibujar hijos" en la lista, los hijos los dibuja ella (ahi); si no corrio (invalida), el recorrido de siempre
bool Routine::DrawsItsChildren() { return run && withChildren[CurrentListIndex()]; }

// el CONSTRUCTOR en modo solo-juego: corre UNA vez por generacion; despues no se recorre y se repone el estado que
// dejo (la foto: solo llama al driver por lo que algo le cambio, el HUD 2D por ejemplo)
bool Routine::SkipThisFrame() {
    if (!constructor || !g_w3dOnlyGame || !gfx::SnapshotStateSupported()) return false;
    if (doneGen == g_w3dConstructorGen) { gfx::SnapshotStateRestore(); return true; }
    return false;
}
void Routine::AfterRender() {
    if (!constructor || !g_w3dOnlyGame || !gfx::SnapshotStateSupported()) return;
    doneGen = g_w3dConstructorGen;
    gfx::SnapshotStateTake();   // (la ultima rutina constructor saca la foto con TODO lo del constructor)
}

// ---------------------------------------------------------------------------
//  ANIMACION de los pasos (canal AnimStep, animation/Animation.h): componente = id * 8 + campo
// ---------------------------------------------------------------------------
static void AnimStepApply(Object* o, int comp, float v) {
    W3dStep* p = ((Routine*)o)->StepById((unsigned)(comp / AnimStepFields));
    if (!p) return;
    const int field = comp % AnimStepFields;
    if (field == AnimStepOn) p->on = (v >= 0.5f);
    else if (field < 5) p->n[field].v = v;   // (un numero que nombra una MEMORIA la sigue leyendo de ella)
    g_w3dRoutinesGen++;
}
float W3dRoutineStepValue(Routine* r, int comp) {
    W3dStep* p = r ? r->StepById((unsigned)(comp / AnimStepFields)) : NULL;
    if (!p) return 0.0f;
    const int field = comp % AnimStepFields;
    return field == AnimStepOn ? (p->on ? 1.0f : 0.0f) : (field < 5 ? p->n[field].v : 0.0f);
}
namespace { struct RegisterAnimStep { RegisterAnimStep() { g_animStepHook = AnimStepApply; } } gRegisterAnimStep; }

bool W3dHasConstructor() {
    if (!SceneCollection) return false;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::routine && ((Routine*)o)->constructor) return true;
    }
    return false;
}
void W3dDefaultRoutines(Object* root, const std::string& constructorName, const std::string& nameClear,
                          const float background[4]) {
    if (!root) return;
    Routine* c = new Routine(root, constructorName);
    c->constructor = true;
    W3dStep col = W3dNewStep(StepColorClear);   // (el color una vez; la limpieza, cada cuadro)
    for (int k = 0; k < 4; k++) col.n[k].v = background[k];
    c->lists[Routine::ModeDefault].push_back(col);
    // la que LIMPIA va AFUERA del constructor, justo DESPUES (antes de la coleccion): limpiar es de CADA cuadro, y lo
    // de adentro del constructor corre UNA vez al empezar el juego (en el editor corre en cada cuadro solo porque la
    // interfaz pisa su estado)
    Routine* l = new Routine(root, nameClear);
    l->lists[Routine::ModeDefault].push_back(W3dNewStep(StepClear));
    // el constructor ARRIBA de todo y la limpieza segunda: el orden del arbol es el de ejecucion
    std::vector<Object*>& h = root->Childrens;
    for (size_t i = 0; i < h.size(); i++) if (h[i] == c || h[i] == l) { h.erase(h.begin() + (long)i); i--; }
    h.insert(h.begin(), (Object*)l);
    h.insert(h.begin(), (Object*)c);
    g_w3dConstructorGen++;
}
// los proyectos de antes traian la limpieza ADENTRO del constructor (corria en cada cuadro porque Object::Render
// recorria los hijos de un constructor ya corrido): al abrirlos se SACA afuera, justo despues de el. Asi lo de adentro
// del constructor es todo de una sola vez
static bool RoutineClean(Routine* r) {
    for (int m = 0; m < Routine::ModeN; m++)
        for (size_t i = 0; i < r->lists[m].size(); i++)
            if (r->lists[m][i].type == StepClear) return true;
    return false;
}
void W3dConstructorRemoveClear(Object* c) {
    if (!c || !c->Parent || c->getType() != ObjectType::routine || !((Routine*)c)->constructor) return;
    std::vector<Object*>& outside = c->Parent->Childrens;
    size_t pos = 0;
    while (pos < outside.size() && outside[pos] != c) pos++;
    if (pos == outside.size()) return;
    std::vector<Object*>& h = c->Childrens;
    for (size_t i = 0; i < h.size(); i++) {
        Object* o = h[i];
        if (!o || o->getType() != ObjectType::routine || ((Routine*)o)->constructor || !RoutineClean((Routine*)o)) continue;
        h.erase(h.begin() + (long)i); i--;
        o->Parent = c->Parent;
        outside.insert(outside.begin() + (long)(++pos), o);   // (en orden: la primera queda pegada al constructor)
    }
}

void Routine::Run(std::vector<W3dStep>& L) {
    static int level = 0;   // (las subrutinas tambien pasan por aca: el shader se saca al terminar la de AFUERA)
    level++;
    const size_t n = L.size();
    for (size_t i = 0; i < n; i++) RunStep(L[i], i, this);
    level--;
    if (level == 0 && gShaderRoutinePlaced) { gfx::WrittenShaderUse(0, NULL); gShaderRoutinePlaced = false; }
}

// ---------------------------------------------------------------------------
//  LUA: step(...) adentro de una funcion del paso "Llamar Lua"
// ---------------------------------------------------------------------------
bool W3dRoutineStepFromLua(W3dStep& p, std::string& error) {
    if (gProf <= 0 || !gInProgress) {
        error = "paso() solo se puede usar mientras se dibuja: adentro de una funcion que llama el paso 'Llamar Lua'";
        return false;
    }
    // los nombres, con un cache (una funcion de lua se llama cada cuadro: buscar por nombre cada vez es caro)
    static std::map<std::string, MeshResource*> meshes;
    const int rt = W3dStepRef(p.type);
    if (rt == RefMesh) {
        std::map<std::string, MeshResource*>::iterator it = meshes.find(p.ref);
        MeshResource* r = NULL;
        if (it != meshes.end() && W3dMeshResourceLive(it->second)) r = it->second;
        else {
            r = W3dRoutineFindMesh(gInProgress, p.ref);
            if (r && !W3dMeshResourceRetain(r)) r = NULL;
            if (r) meshes[p.ref] = r;
        }
        if (!r) { error = "no hay una malla '" + p.ref + "'"; return false; }
        for (int k = 0; k < 5; k++) p.n[k].p = p.n[k].ref.empty() ? NULL : W3dMemoryRef(p.n[k].ref);
        p.ptr = r;
        p.ptr2 = p.ref2.empty() ? NULL : W3dFindNameFrom(gInProgress, p.ref2);
    } else {
        // (una textura que step() pide y nadie tenia cargada queda retenida por la rutina que lo llama: se suelta con
        //  ella. Si ya la tenia, la referencia de mas se devuelve)
        std::vector<Texture*> tex;
        ResolveStep(p, gInProgress, gInProgress, NULL, &tex);
        for (size_t k = 0; k < tex.size(); k++) {
            bool already = false;
            for (size_t j = 0; j < gInProgress->retainedTex.size() && !already; j++) already = (gInProgress->retainedTex[j] == tex[k]);
            if (already) TextureRelease(tex[k]); else gInProgress->retainedTex.push_back(tex[k]);
        }
        if (rt != RefNothing && rt != RefFunction && rt != RefShader && !p.ptr) { error = "no se encontro '" + p.ref + "'"; return false; }
    }
    if (p.type == StepVisible && !p.n[0].p) { error = "visible: el destino tiene que ser una memoria (@nombre[i])"; return false; }
    if (p.type == StepLua && !p.ref2.empty() && !p.ptr2) { error = "lua: no existe el objeto '" + p.ref2 + "'"; return false; }
    if (p.type == StepDraw && p.sub == RangeArray && !p.n[2].p) { error = "dibujar: el array tiene que ser una memoria"; return false; }
    size_t i = 0;
    RunStep(p, i, gInProgress);
    return true;
}

// ---------------------------------------------------------------------------
//  el .w3d
//    "pasos": [ {"paso": "trasladar", "n": [0, "@autos[3]", 1.5]}, {"paso": "dibujar", "ref": "Carroceria"}, ... ]
//    "listas": { "solido": [ ...pasos... ] }    las propias de cada modo (solo si tienen algo o se usan)
//    "usar":   { "solido": "solido", "alambre": "material" }   que lista usa cada modo (falta = la de todos)
//    "constructor": true                         la seccion de arriba del outliner
//    "version": 2                                la de los pasos (sin version = 1: se convierte al leer)
//  Cada paso: "paso" (nombre), y si no valen lo de siempre: "on" (false), "entero", "modo", "ref", "ref2", "n".
//  "Dibujar elementos" guarda la primitiva por NOMBRE ("primitiva": "tira") y el rango ("rango": 1 manual, 2 array).
// ---------------------------------------------------------------------------
// la version de los pasos: 1 = los punteros prendian su array y "limpiar" llevaba el color; 2 = "Array on/off" y
// "Clear color" son pasos aparte (lo de la 1 se convierte al leer: ReadList); 3 = "Clear color" tiene modo (Auto o un
// color propio): el de antes era siempre propio
static const int kVersionSteps = 3;
static const char* const kModes[Routine::ModeN] = { "default", "solid", "material", "render", "wire", "zbuffer" };
const char* W3dRoutineModeName(int m) { return (m >= 0 && m < Routine::ModeN) ? kModes[m] : "default"; }
static int ModeFromName(const std::string& n) {
    for (int m = 0; m < Routine::ModeN; m++) if (n == kModes[m]) return m;
    return -1;
}
static void RuSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
static void RuEsc(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const char c = v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += c; }
        else if (c == '\n') s += "\\n";
        else s += c;
    }
    s += '"';
}
static void WriteList(std::string& s, int ind, std::vector<W3dStep>& L,
                          std::string (*emitTexture)(std::string&, void*), void* ctx) {
    s += "[";
    for (size_t i = 0; i < L.size(); i++) {
        W3dStep& p = L[i];
        char b[16];
        s += (i ? ",\n" : "\n"); RuSang(s, ind + 1);
        s += "{\"step\": "; RuEsc(s, W3dStepName(p.type));
        if (p.id) { snprintf(b, sizeof b, "%u", (unsigned)p.id); s += ", \"id\": "; s += b; }
        if (!p.on) s += ", \"on\": false";
        if (p.integer != 0) { s += ", \"integer\": "; snprintf(b, sizeof b, "%d", p.integer); s += b; }
        if (p.type == StepDraw) {
            if (p.mode != RPrimTriangles) { s += ", \"primitive\": "; RuEsc(s, W3dPrimitiveName(p.mode)); }
            if (p.sub != 0) { s += ", \"range\": "; snprintf(b, sizeof b, "%d", p.sub); s += b; }
        } else if (p.mode != 0) { s += ", \"mode\": "; snprintf(b, sizeof b, "%d", p.mode); s += b; }
        if (!p.ref.empty()) {
            s += ", \"ref\": ";
            if (emitTexture && W3dStepRef(p.type) == RefTexture && p.ptr) {
                // la textura entra al .w3d (como la de un material); en memoria queda su ruta nueva
                Texture* tx = (Texture*)p.ptr;
                RuEsc(s, emitTexture(tx->path, ctx));
                p.ref = tx->path;
            } else if (emitTexture && W3dStepRef(p.type) == RefShader) {
                // el .glsl entra al .w3d (shaders/) como un archivo mas; en memoria queda su entrada
                RuEsc(s, emitTexture(p.ref, ctx));
            } else RuEsc(s, p.ref);
        }
        if (!p.ref2.empty()) { s += ", \"ref2\": "; RuEsc(s, p.ref2); }
        const int nn = W3dStepNumbers(p);
        if (nn > 0) {
            s += ", \"n\": [";
            for (int k = 0; k < nn; k++) {
                if (k) s += ", ";
                if (!p.n[k].ref.empty()) RuEsc(s, p.n[k].ref); else s += JsonNumText(p.n[k].v);
            }
            s += "]";
        }
        s += "}";
    }
    if (!L.empty()) { s += "\n"; RuSang(s, ind); }
    s += "]";
}
void RoutineWriteFields(std::string& s, int ind, Routine* r, std::string (*emitTexture)(std::string&, void*), void* ctx) {
    if (r->dirty) r->Resolve();   // (los punteros de las texturas: su ruta en memoria es la que se emite)
    if (r->constructor) { s += ",\n"; RuSang(s, ind); s += "\"constructor\": true"; }
    if ((int)(r->repeat + 0.5f) > 1) { char b[48]; snprintf(b, sizeof b, "\"repeat\": %d", (int)(r->repeat + 0.5f)); s += ",\n"; RuSang(s, ind); s += b; }
    { char b[32]; snprintf(b, sizeof b, "\"version\": %d", kVersionSteps); s += ",\n"; RuSang(s, ind); s += b; }
    s += ",\n"; RuSang(s, ind); s += "\"steps\": "; WriteList(s, ind, r->lists[Routine::ModeDefault], emitTexture, ctx);
    bool has = false;
    for (int m = 1; m < Routine::ModeN; m++) {
        bool used = false;
        for (int k = 0; k < Routine::ModeN; k++) if (r->use[k] == m) used = true;
        if (r->lists[m].empty() && !used) continue;
        if (!has) { s += ",\n"; RuSang(s, ind); s += "\"lists\": {\n"; } else s += ",\n";
        RuSang(s, ind + 1);
        has = true;
        RuEsc(s, kModes[m]); s += ": "; WriteList(s, ind + 1, r->lists[m], emitTexture, ctx);
    }
    if (has) { s += "\n"; RuSang(s, ind); s += "}"; }
    bool hasU = false;
    for (int m = 1; m < Routine::ModeN; m++) {
        if (r->use[m] < 0) continue;
        if (!hasU) { s += ",\n"; RuSang(s, ind); s += "\"use\": {"; } else s += ", ";
        hasU = true;
        RuEsc(s, kModes[m]); s += ": "; RuEsc(s, kModes[(int)r->use[m]]);
    }
    if (hasU) s += "}";
    if (!r->initialMemories.empty()) {
        s += ",\n"; RuSang(s, ind); s += "\"memories\": {";
        for (size_t i = 0; i < r->initialMemories.size(); i++) {
            s += i ? ",\n" : "\n"; RuSang(s, ind + 1); RuEsc(s, r->initialMemories[i].first); s += ": [";
            const std::vector<float>& v = r->initialMemories[i].second;
            for (size_t k = 0; k < v.size(); k++) { char b[32]; snprintf(b, sizeof b, "%s%g", k ? ", " : "", v[k]); s += b; }
            s += "]";
        }
        s += "\n"; RuSang(s, ind); s += "}";
    }
}
// un paso de un tipo con UN numero (copiado de otro): los pasos viejos se parten en estos
static W3dStep StepWithNumber(int type, const W3dNum& n) {
    W3dStep p = W3dNewStep(type);
    p.n[0] = n; p.n[0].p = NULL;
    return p;
}
static W3dStep StepPointer(int type, const std::string& mesh) {
    W3dStep p = W3dNewStep(type);
    p.ref = mesh;
    return p;
}
// (version 1) el "Array on" de un puntero viejo: el puntero prendia su array, salvo que la malla no lo tuviera (lo
// dejaba apagado). Eso se sabe al resolver la malla: 'sub' = 1 lo marca (Routine::Resolver lo corrige y lo borra)
static W3dStep StepArrayV1(int k, bool on, bool byMesh) {
    W3dStep p = W3dNewStep(StepArray);
    p.mode = k; p.on = on; p.sub = byMesh ? 1 : 0;
    return p;
}
// (antes de la version 3) un "Clear color" era SIEMPRE su color. El que traia el fondo del tema (el que ponia un proyecto
// nuevo, sin que nadie lo eligiera) pasa a Auto; uno que alguien cambio sigue siendo suyo
static int OldClearColor(const W3dNum num[5]) {
    const float* theme = ColorList[static_cast<int>(ColorID::background)];
    for (int k = 0; k < 3; k++) {
        if (!num[k].ref.empty()) return ColorClearOwn;   // (de una memoria: lo maneja el juego)
        const float d = num[k].v - theme[k];
        if (d > 0.003f || d < -0.003f) return ColorClearOwn;
    }
    return ColorClearAuto;
}
static void ReadList(JVal* l, std::vector<W3dStep>& L, std::string (*refTexture)(const std::string&, void*), void* ctx,
                      int version) {
    L.clear();
    if (!l) return;
    // cada paso del archivo da UNO o MAS pasos (los viejos se parten; uno desconocido, ninguno): un ITEM por paso del
    // archivo. Los "Saltar si" cuentan pasos del archivo: al final se recuentan en pasos nuevos
    std::vector<std::vector<W3dStep> > items;
    bool hasPointers = false, hasDraw = false;
    for (size_t i = 0; i < l->list.size(); i++) {
        JVal* o = l->list[i];
        if (!o || o->type != 4) continue;
        items.push_back(std::vector<W3dStep>());
        std::vector<W3dStep>& it = items.back();
        const std::string name   = JS(o, "step", "");
        W3dNum num[5];
        JVal* n = JChild(o, "n", 5);
        if (n) for (size_t k = 0; k < 5 && k < JRowLen(n); k++) {
            if (JRowIsNum(n, k)) num[k].v = JRowNum(n, k, 0.0f);
            else if (k < n->list.size() && n->list[k] && n->list[k]->type == 2) num[k].ref = n->list[k]->str;
        }
        const bool on = JB(o, "on", true);
        const int integer = JI(o, "integer", 0);
        // ---- los pasos de la primera version (un paso con varios estados) se parten en los estados sueltos ----
        if (name == "fog") {           // on, modo (entero), inicio, fin, densidad, gris, alfa
            W3dStep a = W3dNewStep(StepFog); a.on = on; it.push_back(a);
            if (!on) continue;
            W3dStep b = W3dNewStep(StepFogMode); b.mode = integer; it.push_back(b);
            it.push_back(StepWithNumber(StepFogStart, num[0]));
            it.push_back(StepWithNumber(StepFogEnd, num[1]));
            it.push_back(StepWithNumber(StepFogDensity, num[2]));
            W3dStep c = W3dNewStep(StepFogColor);
            c.n[0] = c.n[1] = c.n[2] = num[3]; c.n[3] = num[4];
            it.push_back(c);
            continue;
        }
        if (name == "depth") {      // test on/off + escribe z (entero)
            W3dStep a = W3dNewStep(StepTestZ); a.on = on; it.push_back(a);
            W3dStep b = W3dNewStep(StepWriteZ); b.on = (integer != 0); it.push_back(b);
            continue;
        }
        if (name == "mix") {           // el modo (entero): 0 = apagada
            W3dStep a = W3dNewStep(StepMix); a.on = (integer != 0); it.push_back(a);
            if (integer != 0) { W3dStep b = W3dNewStep(StepMixMode); b.mode = integer; it.push_back(b); }
            continue;
        }
        const int t = W3dStepFromName(name);
        if (t < 0) continue;                         // un paso que esta version no conoce: se saltea
        W3dStep p = W3dNewStep(t);
        p.id = (unsigned short)JI(o, "id", 0);
        p.on = on;
        p.integer = integer;
        p.ref = JS(o, "ref", "");
        p.ref2 = JS(o, "ref2", "");
        if (t == StepDraw) {
            p.mode = PrimitiveFromName(JS(o, "primitive", "triangles"));
            // (la version anterior guardaba el rango en "modo") y "con sus materiales" (on) ya no existe
            p.sub = JI(o, "range", JI(o, "mode", 0));
            p.on = true;
            hasDraw = true;
        } else {
            p.mode = JI(o, "mode", 0);
        }
        if (refTexture && (W3dStepRef(t) == RefTexture || W3dStepRef(t) == RefShader) && !p.ref.empty()) p.ref = refTexture(p.ref, ctx);
        if (n) for (size_t k = 0; k < 5 && k < JRowLen(n); k++) p.n[k] = num[k];
        if (version < 3 && t == StepColorClear) p.mode = OldClearColor(num);
        if (version < 2 && t == StepClear) {
            // (version 1) un solo "Clear": on = el color (con sus 4 numeros), entero = la profundidad
            if (on) { W3dStep c = W3dNewStep(StepColorClear); for (int k = 0; k < 4; k++) c.n[k] = num[k];
                      c.mode = OldClearColor(num); it.push_back(c); }
            W3dStep q = W3dNewStep(StepClear);
            q.integer = (on ? (int)ClearColor : 0) | (integer ? (int)ClearDepth : 0);
            it.push_back(q);
            continue;
        }
        if (version < 2 && t >= StepPointerVertices && t <= StepPointerColors) {
            // (version 1) el puntero prendia o apagaba su array; sin malla SOLO prendia o apagaba
            hasPointers = true;
            const int k = t - StepPointerVertices;
            p.on = true;
            if (p.ref.empty())  it.push_back(StepArrayV1(k, on, false));
            else if (k == 0)    it.push_back(p);
            else if (!on)       it.push_back(StepArrayV1(k, false, false));
            else { it.push_back(StepArrayV1(k, true, true)); it.push_back(p); }
            continue;
        }
        it.push_back(p);
    }
    // una lista VIEJA (de antes de los punteros: "Dibujar malla" ponia todo solo): antes de cada draw van sus
    // cuatro punteros, con su array prendido si la malla lo tiene. Los materiales ya no los pone el draw: se ven con
    // el estado que haya. (antes de CADA draw: un "Saltar si" puede saltear el bloque que los ponia para el anterior)
    if (version < 2 && hasDraw && !hasPointers) {
        for (size_t i = 0; i < items.size(); i++) {
            if (items[i].size() != 1 || items[i][0].type != StepDraw) continue;
            const W3dStep d = items[i][0];
            std::vector<W3dStep> v;
            v.push_back(StepPointer(StepPointerVertices, d.ref));
            for (int k = 1; k < 4; k++) { v.push_back(StepArrayV1(k, true, true)); v.push_back(StepPointer(StepPointerVertices + k, d.ref)); }
            v.push_back(d);
            items[i].swap(v);
        }
    }
    // aplanar y recontar los "Saltar si": saltear c pasos del archivo = saltear los pasos nuevos de esos c items
    std::vector<size_t> start(items.size() + 1, 0);
    for (size_t i = 0; i < items.size(); i++) start[i + 1] = start[i] + items[i].size();
    for (size_t i = 0; i < items.size(); i++)
        for (size_t k = 0; k < items[i].size(); k++) {
            W3dStep q = items[i][k];
            if ((q.type == StepSkipIf || q.type == StepSkipHidden || q.type == StepSkipIfNone) && q.integer > 0) {
                size_t end = i + (size_t)q.integer;
                if (end >= items.size()) end = items.size() - 1;
                q.integer = (int)(start[end + 1] - start[i + 1]);
            }
            L.push_back(q);
        }
}
void RoutineReadFields(JVal* j, Routine* r, std::string (*refTexture)(const std::string&, void*), void* ctx) {
    if (!j || !r) return;
    r->constructor = JB(j, "constructor", false);
    r->repeat = (float)JI(j, "repeat", 1);   // bucle de rutina (sin el campo: una rutina comun)
    if (r->constructor) g_w3dConstructorGen++;   // (un proyecto que se abre: su constructor corre de nuevo)
    const int version = JI(j, "version", 1);     // (sin version: la 1, la de los punteros que prendian su array)
    ReadList(JChild(j, "steps", 5), r->lists[Routine::ModeDefault], refTexture, ctx, version);
    JVal* ls = JChild(j, "lists", 4);
    if (ls) for (std::map<std::string, JVal*>::iterator it = ls->obj.begin(); it != ls->obj.end(); ++it) {
        const int m = ModeFromName(it->first);
        if (m > 0 && it->second->type == 5) ReadList(it->second, r->lists[m], refTexture, ctx, version);
    }
    JVal* us = JChild(j, "use", 4);
    if (us) for (std::map<std::string, JVal*>::iterator it = us->obj.begin(); it != us->obj.end(); ++it) {
        const int m = ModeFromName(it->first);
        const int u = (it->second->type == 2) ? ModeFromName(it->second->str) : -1;
        if (m > 0) r->use[m] = (signed char)(u > 0 ? u : -1);
    }
    // las memorias iniciales (hasta 256 floats cada una): se escriben ya, antes del primer cuadro
    r->initialMemories.clear();
    JVal* ms = JChild(j, "memories", 4);
    if (ms) for (std::map<std::string, JVal*>::iterator it = ms->obj.begin(); it != ms->obj.end(); ++it) {
        if (!it->second || it->second->type != 5) continue;
        std::vector<float> v;
        for (size_t k = 0; k < JRowLen(it->second) && k < (size_t)W3D_MEMORY_SIZE; k++) v.push_back(JRowNum(it->second, k, 0.0f));
        float* m = W3dMemory(it->first);
        for (size_t k = 0; k < v.size(); k++) m[k] = v[k];
        r->initialMemories.push_back(std::make_pair(it->first, v));
    }
    // (sin transformacion: lo que traiga el archivo no cuenta)
    r->pos = Vector3(0, 0, 0); r->scale = Vector3(1, 1, 1); r->SetRot(Quaternion());
    r->dirty = true;
}
