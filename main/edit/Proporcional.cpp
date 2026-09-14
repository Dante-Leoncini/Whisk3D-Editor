// ============================================================================
//  Proporcional.cpp - estado, pesos y circulo del PROPORTIONAL EDITING (ver Proporcional.h).
//  La aplicacion a los vertices vive en LayoutInput.cpp (EVEscribir) y a los objetos en
//  ObjectMode.cpp (SetTranslacionObjetos / SetRotacion / SetScale): cada uno multiplica su
//  transform por el peso precalculado.
// ============================================================================
#include "edit/Proporcional.h"
#include "ui/ViewPorts/ViewPort3D.h"
#include "ui/ViewPorts/LayoutInput.h"   // EditXformActivo / EditXformProporcionalActualizar
#include "ui/ViewPorts/ViewPorts.h"     // BrushBarDragActivo (la barra de influencia se esta arrastrando)
#include "objects/ObjectMode.h"         // SetTransformPivotPoint / ProporcionalObjetosActualizar
#include "objects/EditMesh.h"
#include "objects/Mesh.h"
#include "app/variables.h"
#include "w3dGraphics.h"
#include "WhiskUI/draw/glesdraw.h"      // W3dDrawLinesF
#include "WhiskUI/draw/icons.h"         // IconoIndice
#include "WhiskUI/core/UI.h"            // GlobalScale
#include <math.h>
#include <vector>

extern bool g_redraw;

W3dProporcional g_prop;
W3dProporcional::W3dProporcional() : on(false), conectado(false), tipo(PropSmooth), radio(1.0f) { curva.tipo = FoSmooth; }

// la curva del W3dFalloff detras de cada tipo (Random cae lineal y se multiplica por el azar)
static const int   kCurvaDe[PropTipos] = { FoSmooth, FoSphere, FoRoot, FoInvSquare, FoSharp, FoLinear, FoConstant, FoLinear };
static const char* kNombres[PropTipos] = { "Smooth", "Sphere", "Root", "Inverse Square", "Sharp", "Linear", "Constant", "Random" };

const char* ProporcionalTipoNombre(int tipo) { return (tipo >= 0 && tipo < PropTipos) ? kNombres[tipo] : ""; }

int ProporcionalTipoIcono(int tipo) {
    if (tipo < 0 || tipo >= PropTipos) return (int)IconType::curve;
    const char* n = W3dFalloffIcono(kCurvaDe[tipo]);
    const int ic = (n && n[0]) ? IconoIndice(n) : -1;
    return (ic >= 0) ? ic : (int)IconType::curve;
}

void ProporcionalSetTipo(int tipo) {
    if (tipo < 0 || tipo >= PropTipos) return;
    g_prop.tipo = tipo; g_prop.curva.tipo = kCurvaDe[tipo];
}

// azar fijo por semilla (el indice del vertice/objeto): el mismo vertice pesa lo mismo en todo el arrastre
static float Azar01(int semilla) {
    unsigned int h = (unsigned int)semilla * 2654435761u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (float)(h & 0xFFFFu) / 65535.0f;
}

float ProporcionalPeso(float dist, int semilla) {
    if (!g_prop.on || g_prop.radio <= 1e-6f || dist < 0.0f) return 0.0f;
    const float t = dist / g_prop.radio;
    if (t >= 1.0f) return 0.0f;
    if (g_prop.tipo == PropRandom) return (1.0f - t) * Azar01(semilla);
    return g_prop.curva.Eval(t);
}

void ProporcionalRadioSet(float r) {
    if (r < 0.001f) r = 0.001f;
    if (r > 100000.0f) r = 100000.0f;
    g_prop.radio = r;
}
void ProporcionalRadioEscalar(float factor) { ProporcionalRadioSet(g_prop.radio * factor); }

bool ProporcionalAplicable() { return g_prop.on && (InteractionMode == EditMode || InteractionMode == ObjectMode); }

void ProporcionalReaplicar() {
    if (!g_prop.on) return;
    if (InteractionMode == EditMode && EditXformActivo()) EditXformProporcionalActualizar();
    else if (InteractionMode == ObjectMode && estado != editNavegacion) ProporcionalObjetosActualizar();
    g_redraw = true;
}

// centro del circulo: el pivote fijado al arrancar el transform; fuera de uno, el centro de la seleccion
static bool CentroInfluencia(Vector3& c) {
    if (estado != editNavegacion) { c = TransformPivotPoint; return true; }
    if (InteractionMode == ObjectMode) {
        if (!ObjActivo || !ObjActivo->select) return false;
        SetTransformPivotPoint(); c = TransformPivotPoint; return true;
    }
    if (InteractionMode == EditMode && g_editMesh && g_editMesh->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return false;
        float cx = 0, cy = 0, cz = 0;
        if (!m->edit->CentroSeleccion(cx, cy, cz)) return false;
        Matrix4 W; m->GetWorldMatrix(W); c = W * Vector3(cx, cy, cz); return true;
    }
    return false;
}

void ProporcionalRender(Viewport3D* vp) {
    if (!vp || !ProporcionalAplicable()) return;
    const bool transformando = (estado != editNavegacion);
    if (!transformando && !BrushBarDragActivo()) return;   // se ve mientras se transforma o se ajusta la barra
    Vector3 c; if (!CentroInfluencia(c)) return;
    const float r = g_prop.radio;
    CameraBase cam = vp->VistaCam();
    const Vector3 der = cam.rot * Vector3(1, 0, 0), arr = cam.rot * Vector3(0, 1, 0);
    static std::vector<GLfloat> lin; static std::vector<GLushort> idx;
    const int N = 64;
    lin.clear(); idx.clear();
    for (int i = 0; i < N; i++) {
        const float a = 6.2831853f * (float)i / (float)N;
        const Vector3 p = c + der * (cosf(a) * r) + arr * (sinf(a) * r);
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
