// ============================================================================
//  LightProbe.cpp — ver LightProbe.h. Compartido editor/juego (C++03: Symbian lo compila).
// ============================================================================
#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "objects/LightProbe.h"
#include "objects/Mesh.h"           // la malla que se dibuja: su centro (el gancho de la sonda)
#include "objects/RenderColors.h"   // el color de seleccion
#include "io/JsonW3d.h"             // JVal / JsonNumText: los campos del .w3d
#ifndef W3D_NO_EDITOR
#include "render/RayTracer.h"       // RTBakeProbes: el horneado (solo el editor: el juego trae la sonda horneada)
#endif
#include <math.h>

namespace gfx = w3dEngine;

// las sondas VIVAS (las que estan en la escena las marca su dibujo: LightProbe::pase)
static std::vector<LightProbe*> gProbes;
static unsigned gPass = 1;
void LightProbeNewPass() { gPass++; }

LightProbe::LightProbe(Object* parent, Vector3 pos) : Object(parent, "LightProbe", pos), rays(256), pass(0) {
    size[0] = size[1] = size[2] = 4.0f;
    points[0] = points[1] = points[2] = 3;
    gProbes.push_back(this);
}
LightProbe::~LightProbe() {
    for (size_t i = 0; i < gProbes.size(); i++) if (gProbes[i] == this) { gProbes.erase(gProbes.begin() + (long)i); break; }
}

void LightProbe::PointLocal(int i, int j, int k, float out[3]) const {
    const int ix[3] = { i, j, k };
    for (int e = 0; e < 3; e++) {
        const int n = points[e] < 2 ? 2 : points[e];
        out[e] = ((float)ix[e] / (float)(n - 1) - 0.5f) * size[e];
    }
}

bool LightProbeDraws() { return w3dRenderOverlays; }
// (SIEMPRE: aunque los overlays esten apagados se tiene que recorrer, asi se anota en el pase y sigue iluminando)
bool LightProbe::DrawsSomething() { return true; }

void LightProbeEdges(const LightProbe* s, float out[72]) {
    const float hx = 0.5f * fabsf(s->size[0]), hy = 0.5f * fabsf(s->size[1]), hz = 0.5f * fabsf(s->size[2]);
    float v[8][3];
    for (int i = 0; i < 8; i++) { v[i][0] = (i & 1) ? hx : -hx; v[i][1] = (i & 2) ? hy : -hy; v[i][2] = (i & 4) ? hz : -hz; }
    static const int kEdges[12][2] = { {0,1},{2,3},{4,5},{6,7}, {0,2},{1,3},{4,6},{5,7}, {0,4},{1,5},{2,6},{3,7} };
    int k = 0;
    for (int a = 0; a < 12; a++)
        for (int e = 0; e < 2; e++) { const float* p = v[kEdges[a][e]]; out[k++] = p[0]; out[k++] = p[1]; out[k++] = p[2]; }
}

// SELECCIONADA: LA CAJA (el alcance: lo que esta adentro toma su luz) y en cada PUNTO una ESFERA chica pintada con la luz
// horneada (cada vertice con la luz que le llega a una superficie que mira para ese lado): se ve como iluminaria a un
// objeto en ese lugar. Sin hornear, grises. Sin seleccionar solo el icono (render.cpp: RenderIcons3D). El juego no la
// dibuja, pero igual se anota en el pase (asi ilumina)
#ifdef W3D_SYMBIAN
// (el telefono: una esfera por PUNTO de la sonda, muchas a la vez -> 80 triangulos en vez de 252; el color se lee igual)
static const int kSphereSeg = 8, kSphereRings = 5;
#else
static const int kSphereSeg = 14, kSphereRings = 9;
#endif
static std::vector<float> gSphereN;          // la esfera unitaria: normales (= posiciones)
static std::vector<MeshIndex> gSphereIdx;    // sus triangulos
static void BuildSphereUnit() {
    if (!gSphereN.empty()) return;
    for (int i = 0; i <= kSphereRings; i++) {
        const float la = 3.14159265f * (float)i / (float)kSphereRings, sl = sinf(la), cl = cosf(la);
        for (int j = 0; j <= kSphereSeg; j++) {
            const float lo = 6.2831853f * (float)j / (float)kSphereSeg;
            gSphereN.push_back(sl * cosf(lo)); gSphereN.push_back(cl); gSphereN.push_back(sl * sinf(lo));
        }
    }
    const int c = kSphereSeg + 1;
    for (int i = 0; i < kSphereRings; i++)
        for (int j = 0; j < kSphereSeg; j++) {
            const MeshIndex a = (MeshIndex)(i * c + j), b = (MeshIndex)(i * c + j + 1);
            const MeshIndex d = (MeshIndex)((i + 1) * c + j), e = (MeshIndex)((i + 1) * c + j + 1);
            gSphereIdx.push_back(a); gSphereIdx.push_back(d); gSphereIdx.push_back(b);
            gSphereIdx.push_back(b); gSphereIdx.push_back(d); gSphereIdx.push_back(e);
        }
}
void LightProbe::RenderObject() {
    pass = gPass;
    if (!LightProbeDraws() || !select) return;
    static float lines[72];
    LightProbeEdges(this, lines);
    const bool lightWas = gfx::IsEnabled(gfx::Lighting), texWas = gfx::IsEnabled(gfx::Texture2D);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    const float* sc = gRenderColors[(this == ActiveObj) ? RC_selActive : RC_selInactive];
    gfx::Color4f(sc[0], sc[1], sc[2], 1.0f);
    gfx::LineWidth(2.0f);
    gfx::VertexPointer3f(0, lines);
    gfx::TexCoordPointer2f(12, lines);   // (dummy valido: el driver del N95 lo lee aunque no haya textura)
    gfx::DrawLines(24);
    gfx::LineWidth(1.0f);
    // LAS ESFERAS: del tamano que entra entre puntos
    BuildSphereUnit();
    float step = 1e30f;
    for (int e = 0; e < 3; e++) { const int n = points[e] < 2 ? 2 : points[e]; const float d = fabsf(size[e]) / (float)(n - 1); if (d < step) step = d; }
    const float r = 0.18f * step;
    Matrix4 W; GetWorldMatrix(W);
    float R[9];   // la rotacion local -> mundo (columnas normalizadas: la escala no cuenta)
    for (int c = 0; c < 3; c++) {
        const float x = W.m[c*4], y = W.m[c*4+1], z = W.m[c*4+2], l = sqrtf(x*x + y*y + z*z);
        R[c*3] = l > 1e-6f ? x / l : 0.0f; R[c*3+1] = l > 1e-6f ? y / l : 0.0f; R[c*3+2] = l > 1e-6f ? z / l : 0.0f;
    }
    const int nv = (int)(gSphereN.size() / 3);
    static std::vector<float> pos; static std::vector<unsigned char> col;
    pos.resize((size_t)nv * 3); col.resize((size_t)nv * 4);
    const bool baked = Baked();
    gfx::EnableArray(gfx::ColorArray);
    int idx = 0;
    for (int k = 0; k < points[2]; k++)
        for (int j = 0; j < points[1]; j++)
            for (int i = 0; i < points[0]; i++, idx++) {
                float p[3]; PointLocal(i, j, k, p);
                const float* S = baked ? &sh[(size_t)idx * 27] : NULL;
                for (int v = 0; v < nv; v++) {
                    const float nx = gSphereN[(size_t)v*3], ny = gSphereN[(size_t)v*3+1], nz = gSphereN[(size_t)v*3+2];
                    pos[(size_t)v*3] = p[0] + r * nx; pos[(size_t)v*3+1] = p[1] + r * ny; pos[(size_t)v*3+2] = p[2] + r * nz;
                    float c3[3] = { 0.5f, 0.5f, 0.5f };
                    if (S) {
                        const float wx = R[0]*nx + R[3]*ny + R[6]*nz, wy = R[1]*nx + R[4]*ny + R[7]*nz, wz = R[2]*nx + R[5]*ny + R[8]*nz;
                        const float Y[9] = { 0.282095f, 0.488603f*wy, 0.488603f*wz, 0.488603f*wx, 1.092548f*wx*wy, 1.092548f*wy*wz,
                                             0.315392f*(3.0f*wz*wz - 1.0f), 1.092548f*wx*wz, 0.546274f*(wx*wx - wy*wy) };
                        for (int c = 0; c < 3; c++) { float t = 0.0f; for (int q = 0; q < 9; q++) t += S[q*3+c] * Y[q]; c3[c] = t; }
                    }
                    for (int c = 0; c < 3; c++) col[(size_t)v*4+c] = (unsigned char)(c3[c] <= 0.0f ? 0 : c3[c] >= 1.0f ? 255 : (int)(c3[c] * 255.0f + 0.5f));
                    col[(size_t)v*4+3] = 255;
                }
                gfx::VertexPointer3f(0, &pos[0]);
                gfx::ColorPointer4ub(&col[0]);
                gfx::DrawTriangles((int)gSphereIdx.size(), &gSphereIdx[0]);
            }
    gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::NormalArray);
    if (texWas) gfx::Enable(gfx::Texture2D);
    if (lightWas) gfx::Enable(gfx::Lighting);
}

// ---- LA LUZ EN UN PUNTO DEL MUNDO ----
// la grilla de la sonda 's' en el punto LOCAL q: interpolacion TRILINEAL de los 8 puntos que lo rodean (afuera de la
// caja, el borde mas cercano)
static void InterpolateIn(const LightProbe* s, const float q[3], float out[27]) {
    int i0[3]; float t[3];
    for (int e = 0; e < 3; e++) {
        const int n = s->points[e] < 2 ? 2 : s->points[e];
        float g = (s->size[e] > 1e-6f ? q[e] / s->size[e] : 0.0f) + 0.5f;
        g *= (float)(n - 1);
        if (g < 0.0f) g = 0.0f; if (g > (float)(n - 1)) g = (float)(n - 1);
        i0[e] = (int)floorf(g); if (i0[e] > n - 2) i0[e] = n - 2;
        t[e] = g - (float)i0[e];
    }
    for (int c = 0; c < 27; c++) out[c] = 0.0f;
    for (int b = 0; b < 8; b++) {
        const int ix = i0[0] + (b & 1), iy = i0[1] + ((b >> 1) & 1), iz = i0[2] + ((b >> 2) & 1);
        const float w = ((b & 1) ? t[0] : 1.0f - t[0]) * (((b >> 1) & 1) ? t[1] : 1.0f - t[1]) * (((b >> 2) & 1) ? t[2] : 1.0f - t[2]);
        if (w <= 0.0f) continue;
        const size_t base = ((size_t)iz * s->points[1] * s->points[0] + (size_t)iy * s->points[0] + (size_t)ix) * 27;
        for (int c = 0; c < 27; c++) out[c] += w * s->sh[base + c];
    }
}
bool LightProbeIn(const float pos[3], float sh[27]) {
    const LightProbe* best = NULL; float bestD = 1e30f, bestVol = 1e30f; float qBest[3] = { 0, 0, 0 };
    for (size_t i = 0; i < gProbes.size(); i++) {
        const LightProbe* s = gProbes[i];
        if (!s || !s->Baked() || s->pass + 1 < gPass) continue;   // (sin hornear, o no esta en la escena)
        Matrix4 W, inv; s->GetWorldMatrix(W);
        if (!Matrix4::InvertAffine(W, inv)) continue;
        const Vector3 ql = inv * Vector3(pos[0], pos[1], pos[2]);
        const float q[3] = { ql.x, ql.y, ql.z };
        // cuanto se sale de la caja (0 = adentro), en proporcion a su tamano
        float d = 0.0f, greater = 0.0f;
        for (int e = 0; e < 3; e++) {
            const float h = 0.5f * fabsf(s->size[e]);
            if (h > greater) greater = h;
            const float f = fabsf(q[e]) - h;
            if (f > d) d = f;
        }
        const float rel = greater > 1e-6f ? d / (2.0f * greater) : 1e30f;
        if (rel > 0.25f) continue;   // (lejos de la caja: no la alcanza)
        const float vol = fabsf(s->size[0] * s->size[1] * s->size[2]);
        // adentro gana la mas CHICA (mas detalle); afuera, la mas cercana
        if (rel < bestD - 1e-6f || (fabsf(rel - bestD) <= 1e-6f && vol < bestVol)) {
            best = s; bestD = rel; bestVol = vol; qBest[0] = q[0]; qBest[1] = q[1]; qBest[2] = q[2];
        }
    }
    if (!best) return false;
    InterpolateIn(best, qBest, sh);
    return true;
}

// ---- EL GANCHO DEL CORE (Mesh::RenderObject, ver objects/Mesh.h) ----
// al empezar una malla: la luz de su CENTRO (el del AABB de la malla, en el mundo). Con shaders va al backend
// (AmbientSH: la luz por pixel); para el resto, el PROMEDIO es la luz ambiente de GL de ese objeto. Re-entrante (una
// malla dibujada adentro de otra: una instancia)
extern void (*g_w3dObjectProbeHook)(Object* o, bool begins);
extern bool  g_w3dProbeHas;
extern float g_w3dProbeSH[27];
extern float g_w3dEyeToWorld[9];               // (EscenaRender: la vista de este pase, ojo -> mundo)
extern void  W3dAmbientPass(float c[4]);       // (EscenaRender: el ambiente del fondo de este pase)
namespace {
struct LevelProbe { bool applied; float sh[27]; };
LevelProbe gStack[8];
int gLevel = 0;
void ApplySH(const float* sh) {
    for (int c = 0; c < 27; c++) g_w3dProbeSH[c] = sh[c];
    g_w3dProbeHas = true;
    gfx::AmbientSH(g_w3dProbeSH, g_w3dEyeToWorld);
    float a[4] = { sh[0] * 0.282095f, sh[1] * 0.282095f, sh[2] * 0.282095f, 1.0f };
    for (int k = 0; k < 3; k++) if (a[k] < 0.0f) a[k] = 0.0f;
    gfx::LightModelAmbient(a);
}
void RemoveSH() {
    g_w3dProbeHas = false;
    gfx::AmbientSH(NULL, NULL);
    float a[4]; W3dAmbientPass(a);
    gfx::LightModelAmbient(a);
}
void HookProbe(Object* o, bool begins) {
    if (begins) {
        LevelProbe& n = gStack[gLevel < 8 ? gLevel : 7];
        if (gLevel < 8) gLevel++;
        n.applied = false;
        if (gProbes.empty() || !o || o->getType() != ObjectType::mesh) return;
        // LO ESTATICO no: su luz se PRECALCULA (la tarjeta "Bake Lighting"); la sonda es para lo que se mueve
        if (o->isStatic) return;
        Mesh* me = (Mesh*)o;
        Vector3 cl(0, 0, 0);
        // (con ESQUELETO, el centro de la ULTIMA POSE: el de reposo se queda donde se modelo y la luz no seguia al
        //  personaje que el esqueleto lleva por la escena -el proxy de un personaje animado-)
        if (me->skinArmature && me->skinAabbOk)
            cl = Vector3(0.5f * (me->skinAabbMin.x + me->skinAabbMax.x), 0.5f * (me->skinAabbMin.y + me->skinAabbMax.y),
                         0.5f * (me->skinAabbMin.z + me->skinAabbMax.z));
        else if (me->aabbOk) cl = Vector3(0.5f * (me->aabbMin.x + me->aabbMax.x), 0.5f * (me->aabbMin.y + me->aabbMax.y),
                                          0.5f * (me->aabbMin.z + me->aabbMax.z));
        Matrix4 W; o->GetWorldMatrix(W);
        const Vector3 cw = W * cl;
        const float p[3] = { cw.x, cw.y, cw.z };
        if (!LightProbeIn(p, n.sh)) return;
        n.applied = true;
        ApplySH(n.sh);
    } else {
        if (gLevel <= 0) return;
        gLevel--;
        const bool was = gStack[gLevel < 8 ? gLevel : 7].applied;
        if (!was) return;
        // (vuelve la de la malla de afuera, si tenia)
        if (gLevel > 0 && gStack[gLevel - 1].applied) ApplySH(gStack[gLevel - 1].sh);
        else RemoveSH();
    }
}
struct RegisterProbe { RegisterProbe() { g_w3dObjectProbeHook = HookProbe; } } gRegisterProbe;
}

// ---- HORNEAR ----
bool LightProbeBakeBegin(LightProbe* s, std::string* reason) {
    if (!s) return false;
    for (int e = 0; e < 3; e++) { if (s->points[e] < 2) s->points[e] = 2; if (s->points[e] > 16) s->points[e] = 16; }
    Matrix4 W; s->GetWorldMatrix(W);
    std::vector<float> pos;
    pos.reserve((size_t)s->Count() * 3);
    for (int k = 0; k < s->points[2]; k++)
        for (int j = 0; j < s->points[1]; j++)
            for (int i = 0; i < s->points[0]; i++) {
                float p[3]; s->PointLocal(i, j, k, p);
                const Vector3 w = W * Vector3(p[0], p[1], p[2]);
                pos.push_back(w.x); pos.push_back(w.y); pos.push_back(w.z);
            }
#ifdef W3D_NO_EDITOR
    if (reason) *reason = "Nothing to bake"; return false;   // (el juego no hornea: usa la sonda horneada del .w3d)
#else
    if (!RTProbeBeginLight(pos, s->rays)) { if (reason) *reason = "Nothing to bake"; return false; }
    return true;
#endif
}

bool LightProbeBakeFinish(LightProbe* s) {
    if (!s) return false;
    std::vector<float> sh;
    std::vector<unsigned char> ok;
#ifdef W3D_NO_EDITOR
    return false;
#else
    if (!RTProbeTakeLight(sh, &ok)) return false;
    if (sh.size() != (size_t)s->Count() * 27 || ok.size() != (size_t)s->Count()) return false;   // (cambio la grilla)
#endif
    // LOS PUNTOS ADENTRO DE ALGO (una pared, el piso, la malla misma) no ven la luz: toman el promedio de sus vecinos
    // validos de la grilla, pasada tras pasada, hasta que no queda ninguno sin llenar (o no hay de donde)
    const int nx = s->points[0], ny = s->points[1], nz = s->points[2];
    for (int pass = 0; pass < nx + ny + nz; pass++) {
        std::vector<unsigned char> ok2 = ok;
        bool missing = false, full = false;
        for (int k = 0; k < nz; k++) for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) {
            const size_t id = (size_t)k * ny * nx + (size_t)j * nx + (size_t)i;
            if (ok[id]) continue;
            missing = true;
            float acc[27]; for (int c = 0; c < 27; c++) acc[c] = 0.0f;
            int count = 0;
            static const int dv[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
            for (int v = 0; v < 6; v++) {
                const int a = i + dv[v][0], b = j + dv[v][1], c3 = k + dv[v][2];
                if (a < 0 || a >= nx || b < 0 || b >= ny || c3 < 0 || c3 >= nz) continue;
                const size_t vid = (size_t)c3 * ny * nx + (size_t)b * nx + (size_t)a;
                if (!ok[vid]) continue;
                for (int c = 0; c < 27; c++) acc[c] += sh[vid * 27 + c];
                count++;
            }
            if (count) { for (int c = 0; c < 27; c++) sh[id * 27 + c] = acc[c] / (float)count; ok2[id] = 1; full = true; }
        }
        ok.swap(ok2);
        if (!missing || !full) break;
    }
    s->sh.swap(sh);
    return true;
}

bool LightProbeLive(const LightProbe* s) {
    for (size_t i = 0; i < gProbes.size(); i++) if (gProbes[i] == s) return true;
    return false;
}

// DE UNA SOLA VEZ (el harness)
bool LightProbeBake(LightProbe* s, std::string* reason) {
    if (!LightProbeBakeBegin(s, reason)) return false;
#ifndef W3D_NO_EDITOR
    while (RTProbeActive()) RTProbeStep(100000);
#endif
    if (!LightProbeBakeFinish(s)) { if (reason) *reason = "Nothing to bake"; return false; }
    return true;
}

LightProbe* LightProbeDuplicate(LightProbe* src) {
    if (!src) return NULL;
    LightProbe* d = new LightProbe(src->Parent, src->pos);
    for (int k = 0; k < 3; k++) { d->size[k] = src->size[k]; d->points[k] = src->points[k]; }
    d->rays = src->rays;
    d->sh = src->sh;
    return d;
}

// ---- .w3d ----
static void SlSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
void LightProbeWriteFields(std::string& s, int ind, const LightProbe* o) {
    s += ",\n"; SlSang(s, ind); s += "\"size\": [";
    for (int k = 0; k < 3; k++) { if (k) s += ", "; s += JsonNumText(o->size[k]); }
    s += "]";
    s += ",\n"; SlSang(s, ind); s += "\"points\": [";
    for (int k = 0; k < 3; k++) { if (k) s += ", "; s += JsonNumText((float)o->points[k]); }
    s += "]";
    s += ",\n"; SlSang(s, ind); s += "\"rays\": "; s += JsonNumText((float)o->rays);
    if (o->Baked()) {   // (la luz horneada: 27 numeros por punto)
        s += ",\n"; SlSang(s, ind); s += "\"sh\": [";
        for (size_t i = 0; i < o->sh.size(); i++) { if (i) s += (i % 27 == 0) ? ",\n" : ", "; s += JsonNumText(o->sh[i]); }
        s += "]";
    }
}
void LightProbeReadFields(JVal* j, LightProbe* o) {
    if (!j || !o) return;
    JVal* t = JChild(j, "size", 5);
    if (t) for (size_t i = 0; i < 3 && i < JRowLen(t); i++) o->size[i] = fabsf(JRowNum(t, i, 4.0f));
    JVal* p = JChild(j, "points", 5);
    if (p) for (size_t i = 0; i < 3 && i < JRowLen(p); i++) {
        int n = (int)(JRowNum(p, i, 3.0f) + 0.5f); if (n < 2) n = 2; if (n > 16) n = 16; o->points[i] = n;
    }
    o->rays = JI(j, "rays", 256);
    o->sh.clear();
    JVal* sh = JChild(j, "sh", 5);
    if (sh) {
        const size_t n = JRowLen(sh);
        if (n == (size_t)o->Count() * 27) { o->sh.resize(n); for (size_t i = 0; i < n; i++) o->sh[i] = JRowNum(sh, i, 0.0f); }
    }
}
