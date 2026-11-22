// ============================================================================
//  Proportional.cpp - estado, pesos y circulo del PROPORTIONAL EDITING (ver Proportional.h).
//  La aplicacion a los vertices vive en LayoutInput.cpp (EVWrite) y a los objetos en
//  ObjectMode.cpp (SetTranslationObjects / BeginRotate / SetScale): cada uno multiplica su
//  transform por el peso precalculado.
// ============================================================================
#include "edit/Proportional.h"
#include "ui/ViewPorts/ViewPort3D.h"
#include "ui/ViewPorts/LayoutInput.h"   // EditXformActive / ProportionalEditXformUpdate
#include "ui/ViewPorts/ViewPorts.h"     // BrushBarDragActive (la barra de influencia se esta arrastrando)
#include "objects/ObjectMode.h"         // SetTransformPivotPoint / ProportionalObjectsUpdate
#include "objects/EditMesh.h"
#include "objects/Mesh.h"
#include "app/variables.h"
#include "w3dGraphics.h"
#include "WhiskUI/draw/glesdraw.h"      // W3dDrawLinesF
#include "WhiskUI/draw/icons.h"         // IconIndex
#include "WhiskUI/core/UI.h"            // GlobalScale
#include <math.h>
#include <vector>

extern bool g_redraw;

W3dProportional g_prop;
W3dProportional::W3dProportional() : on(false), connected(false), type(PropSmooth), radius(1.0f) { curve.type = FoSmooth; }

// la curva del W3dFalloff detras de cada tipo (Random cae lineal y se multiplica por el azar)
static const int   kCurveOf[PropTypes] = { FoSmooth, FoSphere, FoRoot, FoInvSquare, FoSharp, FoLinear, FoConstant, FoLinear };
static const char* kNames[PropTypes]   = { "Smooth", "Sphere", "Root", "Inverse Square", "Sharp", "Linear", "Constant", "Random" };

const char* ProportionalTypeName(int type) { return (type >= 0 && type < PropTypes) ? kNames[type] : ""; }

int ProportionalIconType(int type) {
    if (type < 0 || type >= PropTypes) return (int)IconType::curve;
    const char* n = W3dFalloffIcon(kCurveOf[type]);
    const int ic = (n && n[0]) ? IconIndex(n) : -1;
    return (ic >= 0) ? ic : (int)IconType::curve;
}

void ProportionalSetType(int type) {
    if (type < 0 || type >= PropTypes) return;
    g_prop.type = type; g_prop.curve.type = kCurveOf[type];
}

// azar fijo por semilla (el indice del vertice/objeto): el mismo vertice pesa lo mismo en todo el arrastre
static float Random01(int seed) {
    unsigned int h = (unsigned int)seed * 2654435761u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (float)(h & 0xFFFFu) / 65535.0f;
}

float ProportionalWeight(float dist, int seed) {
    if (!g_prop.on || g_prop.radius <= 1e-6f || dist < 0.0f) return 0.0f;
    const float t = dist / g_prop.radius;
    if (t >= 1.0f) return 0.0f;
    if (g_prop.type == PropRandom) return (1.0f - t) * Random01(seed);
    return g_prop.curve.Eval(t);
}

void ProportionalRadiusSet(float r) {
    if (r < 0.001f) r = 0.001f;
    if (r > 100000.0f) r = 100000.0f;
    g_prop.radius = r;
}
void ProportionalRadiusScale(float factor) { ProportionalRadiusSet(g_prop.radius * factor); }

bool ProportionalApplicable() { return g_prop.on && (InteractionMode == EditMode || InteractionMode == ObjectMode); }

void ProportionalReapply() {
    if (!g_prop.on) return;
    if (InteractionMode == EditMode && EditXformActive()) ProportionalEditXformUpdate();
    else if (InteractionMode == ObjectMode && state != editNavigation) ProportionalObjectsUpdate();
    g_redraw = true;
}

// centro del circulo: el pivote fijado al arrancar el transform; fuera de uno, el centro de la seleccion
static bool InfluenceCenter(Vector3& c) {
    if (state != editNavigation) { c = TransformPivotPoint; return true; }
    if (InteractionMode == ObjectMode) {
        if (!ActiveObj || !ActiveObj->select) return false;
        SetTransformPivotPoint(); c = TransformPivotPoint; return true;
    }
    if (InteractionMode == EditMode && g_editMesh && g_editMesh->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return false;
        float cx = 0, cy = 0, cz = 0;
        if (!m->edit->SelectionCenter(cx, cy, cz)) return false;
        Matrix4 W; m->GetWorldMatrix(W); c = W * Vector3(cx, cy, cz); return true;
    }
    return false;
}

void ProportionalRender(Viewport3D* vp) {
    if (!vp || !ProportionalApplicable()) return;
    const bool transforming = (state != editNavigation);
    if (!transforming && !BrushBarDragActive()) return;   // se ve mientras se transforma o se ajusta la barra
    Vector3 c; if (!InfluenceCenter(c)) return;
    const float r = g_prop.radius;
    CameraBase cam = vp->ViewCam();
    const Vector3 right = cam.rot * Vector3(1, 0, 0), arr = cam.rot * Vector3(0, 1, 0);
    static std::vector<GLfloat> lin; static std::vector<GLushort> idx;
    const int N = 64;
    lin.clear(); idx.clear();
    for (int i = 0; i < N; i++) {
        const float a = 6.2831853f * (float)i / (float)N;
        const Vector3 p = c + right * (cosf(a) * r) + arr * (sinf(a) * r);
        lin.push_back(p.x); lin.push_back(p.y); lin.push_back(p.z);
        idx.push_back((GLushort)i); idx.push_back((GLushort)((i + 1) % N));
    }
    w3dEngine::Disable(w3dEngine::DepthTest); w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::DisableArray(w3dEngine::NormalArray); w3dEngine::DisableArray(w3dEngine::TexCoordArray); w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::Enable(w3dEngine::Blend); w3dEngine::BlendAlpha();
    w3dEngine::Color4f(0.85f, 0.85f, 0.85f, 0.6f);
    w3dEngine::LineWidth(1.5f * GlobalScale);
    W3dDrawLinesF(&lin[0], &idx[0], (int)idx.size());
    w3dEngine::LineWidth(1.0f);
    w3dEngine::Enable(w3dEngine::DepthTest);
}
