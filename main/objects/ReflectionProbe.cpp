// ============================================================================
//  ReflectionProbe.cpp — ver ReflectionProbe.h. Compartido editor/juego (C++03: Symbian lo compila).
// ============================================================================
#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "objects/ReflectionProbe.h"
#include "objects/Textures.h"       // TextureTake / TextureRelease: la imagen 360
#include "objects/RenderColors.h"   // el color de seleccion
#include "graphics/w3dTexture.h"   // DeleteTexture: la copia borrosa del PBR
#include "io/JsonW3d.h"             // JVal / JsonNumText: los campos del .w3d
#ifndef W3D_NO_EDITOR
#include "render/RayTracer.h"      // RTBakeReflection: el horneado (solo el editor)
#include "io/EditedTexture.h"      // la imagen horneada es una textura del proyecto (en memoria hasta guardar)
#include "render/RenderOptions.h"  // g_redraw
#endif
#include <math.h>

namespace gfx = w3dEngine;
extern unsigned int w3dGetTicks();

// las sondas VIVAS
static std::vector<ReflectionProbe*> gProbes;

// el gancho de la malla (objects/Mesh.cpp): un material con el reflejo en "La sonda mas cercana" pide la de su lugar
extern Texture* (*g_w3dReflectionProbeHook)(Object* o, float* box, float* center, bool* withBox);
static Texture* ReflectionFor(Object* o, float* box, float* center, bool* withBox) {
    if (withBox) *withBox = false;
    if (!o) return NULL;
    Matrix4 W; o->GetWorldMatrix(W);
    const float p[3] = { W.m[12], W.m[13], W.m[14] };
    ReflectionProbe* s = ReflectionProbeFor(p);
    if (!s) return NULL;
    Texture* t = s->Image();
    if (!t) return NULL;
    if (s->box && box && center) { s->BoxWorld(box); s->WorldCenter(center); if (withBox) *withBox = true; }
    return t;
}
namespace { struct InstallReflection { InstallReflection() { g_w3dReflectionProbeHook = ReflectionFor; } }; }
static InstallReflection gInstallReflection;

ReflectionProbe::ReflectionProbe(Object* parent, Vector3 pos)
    : Object(parent, "ReflectionProbe", pos), type(W3D_REFLECTION_BAKED), importance(1), intensity(1.0f), box(false),
      resolution(256), background(0), near(0.3f), samples(32), bounces(3), eachSeg(0.0f), tex(NULL), lastMs(0), version(1),
      blurryTex(0), blurryVersion(0) {
    size[0] = size[1] = size[2] = 10.0f;
    origin[0] = origin[1] = origin[2] = 0.0f;
    backgroundColor[0] = 0.18f; backgroundColor[1] = 0.22f; backgroundColor[2] = 0.3f; backgroundColor[3] = 1.0f;
    gProbes.push_back(this);
}
ReflectionProbe::~ReflectionProbe() {
    for (size_t i = 0; i < gProbes.size(); i++) if (gProbes[i] == this) { gProbes.erase(gProbes.begin() + (long)i); break; }
    if (tex) TextureRelease(tex);
    tex = NULL;
    if (blurryTex) gfx::DeleteTexture(blurryTex);
    blurryTex = 0;
    if (live.iID) gfx::DeleteTexture(live.iID);
    live.iID = 0;
}

bool ReflectionProbe::HasImage() const { return type == W3D_REFLECTION_TIME_REAL ? live.iID != 0 : !texture.empty(); }
Texture* ReflectionProbe::Image() {
    if (type == W3D_REFLECTION_TIME_REAL) return live.iID ? &live : NULL;
    if (texture.empty()) { if (tex) { TextureRelease(tex); tex = NULL; } return NULL; }
    if (tex && tex->path == texture) return tex;
    if (tex) TextureRelease(tex);
    tex = TextureTake(texture);
    version++;
    return tex;
}
void ReflectionProbe::WorldCenter(float out[3]) const {
    Matrix4 W; const_cast<ReflectionProbe*>(this)->GetWorldMatrix(W);
    const Vector3 c = W * Vector3(origin[0], origin[1], origin[2]);
    out[0] = c.x; out[1] = c.y; out[2] = c.z;
}
void ReflectionProbe::BoxWorld(float out[6]) const {
    Matrix4 W; const_cast<ReflectionProbe*>(this)->GetWorldMatrix(W);
    out[0] = out[1] = out[2] = 1e30f; out[3] = out[4] = out[5] = -1e30f;
    for (int i = 0; i < 8; i++) {
        const Vector3 p = W * Vector3((i & 1) ? 0.5f * size[0] : -0.5f * size[0], (i & 2) ? 0.5f * size[1] : -0.5f * size[1],
                                      (i & 4) ? 0.5f * size[2] : -0.5f * size[2]);
        const float q[3] = { p.x, p.y, p.z };
        for (int k = 0; k < 3; k++) { if (q[k] < out[k]) out[k] = q[k]; if (q[k] > out[3 + k]) out[3 + k] = q[k]; }
    }
}

void ReflectionProbeEdges(const ReflectionProbe* s, float out[72]) {
    const float hx = 0.5f * fabsf(s->size[0]), hy = 0.5f * fabsf(s->size[1]), hz = 0.5f * fabsf(s->size[2]);
    float v[8][3];
    for (int i = 0; i < 8; i++) { v[i][0] = (i & 1) ? hx : -hx; v[i][1] = (i & 2) ? hy : -hy; v[i][2] = (i & 4) ? hz : -hz; }
    static const int kEdges[12][2] = { {0,1},{2,3},{4,5},{6,7}, {0,2},{1,3},{4,6},{5,7}, {0,4},{1,5},{2,6},{3,7} };
    int k = 0;
    for (int a = 0; a < 12; a++)
        for (int e = 0; e < 2; e++) { const float* p = v[kEdges[a][e]]; out[k++] = p[0]; out[k++] = p[1]; out[k++] = p[2]; }
}

// LA ESFERA DE MUESTRA (la sonda seleccionada): un ESPEJO en el centro de la captura con la imagen de la sonda, como
// la de Unity: se ve lo que se horneo (o lo que esta viendo la de tiempo real) tal como lo refleja un metal liso. El
// reflejo se calcula por esquina en la CPU (la vista en cada vertice, reflejada en la normal y llevada al MUNDO), asi
// anda igual con shaders y en el pipeline fijo
extern float g_w3dEyeToWorld[9];   // (EscenaRender: la vista de este pase, ojo -> mundo)
static void SphereSample(ReflectionProbe* s) {
    Texture* t = s->Image();
    if (!t || !t->iID) return;
    const int NU = 32, NV = 16;
    static std::vector<float> nrm, pos, uv;
    if (nrm.empty()) {   // (la esfera unidad, triangulo por triangulo: cada esquina con su propia UV)
        const float PI = 3.14159265f;
        for (int j = 0; j < NV; j++)
            for (int i = 0; i < NU; i++) {
                const int esq[6][2] = { {i, j}, {i + 1, j}, {i + 1, j + 1}, {i, j}, {i + 1, j + 1}, {i, j + 1} };
                for (int k = 0; k < 6; k++) {
                    const float th = PI * (float)esq[k][1] / NV, ph = 2.0f * PI * (float)esq[k][0] / NU;
                    nrm.push_back(sinf(th) * cosf(ph)); nrm.push_back(cosf(th)); nrm.push_back(sinf(th) * sinf(ph));
                }
            }
        pos.resize(nrm.size()); uv.resize(nrm.size() / 3 * 2);
    }
    float MV[16]; gfx::GetMatrix(gfx::ModelView, MV);
    const float r = 0.5f;
    const size_t n = nrm.size() / 3;
    for (size_t k = 0; k < n; k++) {
        const float* q = &nrm[k * 3];
        float* p = &pos[k * 3];
        p[0] = s->origin[0] + r * q[0]; p[1] = s->origin[1] + r * q[1]; p[2] = s->origin[2] + r * q[2];
        // en OJO: el punto, la normal y la vista reflejada
        const float pe[3] = { MV[0] * p[0] + MV[4] * p[1] + MV[8] * p[2] + MV[12], MV[1] * p[0] + MV[5] * p[1] + MV[9] * p[2] + MV[13],
                              MV[2] * p[0] + MV[6] * p[1] + MV[10] * p[2] + MV[14] };
        float ne[3] = { MV[0] * q[0] + MV[4] * q[1] + MV[8] * q[2], MV[1] * q[0] + MV[5] * q[1] + MV[9] * q[2],
                        MV[2] * q[0] + MV[6] * q[1] + MV[10] * q[2] };
        float l = sqrtf(ne[0] * ne[0] + ne[1] * ne[1] + ne[2] * ne[2]); if (l < 1e-9f) l = 1.0f;
        ne[0] /= l; ne[1] /= l; ne[2] /= l;
        float v[3] = { pe[0], pe[1], pe[2] };
        l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l < 1e-9f) l = 1.0f;
        v[0] /= l; v[1] /= l; v[2] /= l;
        const float d = v[0] * ne[0] + v[1] * ne[1] + v[2] * ne[2];
        const float R[3] = { v[0] - 2.0f * d * ne[0], v[1] - 2.0f * d * ne[1], v[2] - 2.0f * d * ne[2] };
        // al MUNDO (ojo -> mundo, por columnas) y a la imagen 360: u = 0.5 + atan2(z, x) / 2pi, v = acos(y) / pi
        float w[3];
        for (int f = 0; f < 3; f++) w[f] = g_w3dEyeToWorld[0 * 3 + f] * R[0] + g_w3dEyeToWorld[1 * 3 + f] * R[1] + g_w3dEyeToWorld[2 * 3 + f] * R[2];
        const float wy = w[1] < -1.0f ? -1.0f : (w[1] > 1.0f ? 1.0f : w[1]);
        uv[k * 2] = 0.5f + atan2f(w[2], w[0]) * 0.15915494f;
        uv[k * 2 + 1] = acosf(wy) * 0.31830989f;
    }
    // (la COSTURA: un triangulo que cruza u = 0 / 1 iria por toda la imagen; sus u chicas pasan del otro lado)
    for (size_t k = 0; k + 2 < n; k += 3) {
        float mn = uv[k * 2], mx = mn;
        for (int e = 1; e < 3; e++) { const float u = uv[(k + e) * 2]; if (u < mn) mn = u; if (u > mx) mx = u; }
        if (mx - mn > 0.5f) for (int e = 0; e < 3; e++) if (uv[(k + e) * 2] < 0.5f) uv[(k + e) * 2] += 1.0f;
    }
    gfx::Disable(gfx::Lighting); gfx::Enable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::BindTexture(t->iID);
    gfx::TexFilter(true); gfx::TexWrap(true);
    gfx::TexEnvReplace(true);
    gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::VertexPointer3f(0, &pos[0]);
    gfx::TexCoordPointer2f(0, &uv[0]);
    gfx::DrawTrianglesArray((int)n);
    gfx::TexEnvReplace(false);
    gfx::Disable(gfx::Texture2D);
}

// SELECCIONADA: su CAJA (el alcance) y la esfera de muestra con su imagen. Sin seleccionar, solo el icono (render.cpp)
void ReflectionProbe::RenderObject() {
    if (!w3dRenderOverlays || !select) return;
    static float lines[72];
    ReflectionProbeEdges(this, lines);
    const bool lightWas = gfx::IsEnabled(gfx::Lighting), texWas = gfx::IsEnabled(gfx::Texture2D);
    SphereSample(this);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    const float* sc = gRenderColors[(this == ActiveObj) ? RC_selActive : RC_selInactive];
    gfx::Color4f(sc[0], sc[1], sc[2], 1.0f);
    gfx::LineWidth(2.0f);
    gfx::VertexPointer3f(0, lines);
    gfx::TexCoordPointer2f(12, lines);   // (dummy valido: el driver del N95 lo lee aunque no haya textura)
    gfx::DrawLines(24);
    gfx::LineWidth(1.0f);
    gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    if (lightWas) gfx::Enable(gfx::Lighting);
    if (texWas) gfx::Enable(gfx::Texture2D);
}

// sigue COLGADA de la escena? Borrar en el editor no libera el objeto (lo guarda el Ctrl+Z): lo saca de la lista de
// hijos de su padre, pero el objeto sigue apuntando al padre y moviendose con el. Para las sondas ya no existe: si no,
// una sonda borrada seguia reflejando (y la de tiempo real capturando) desde adentro del objeto del que colgaba
bool ReflectionProbeInScene(const ReflectionProbe* s) {
    for (const Object* n = s; n && n->Parent; n = n->Parent) {
        const std::vector<Object*>& h = n->Parent->Childrens;
        bool is = false;
        for (size_t i = 0; i < h.size() && !is; i++) is = (h[i] == n);
        if (!is) return false;
    }
    return s != NULL;
}
ReflectionProbe* ReflectionProbeFor(const float pos[3], bool onlyInside) {
    ReflectionProbe* best = NULL; int bestImp = -2147483647; float bestVol = 1e30f;
    ReflectionProbe* near = NULL; float nearD = 1e30f;
    for (size_t i = 0; i < gProbes.size(); i++) {
        ReflectionProbe* s = gProbes[i];
        if (!s || !s->visible || !s->HasImage() || !ReflectionProbeInScene(s)) continue;
        float b[6]; s->BoxWorld(b);
        const bool inside = pos[0] >= b[0] && pos[0] <= b[3] && pos[1] >= b[1] && pos[1] <= b[4] && pos[2] >= b[2] && pos[2] <= b[5];
        if (inside) {
            // (la de mas importancia; con la misma, la mas chica: la mas especifica del lugar)
            const float vol = (b[3] - b[0]) * (b[4] - b[1]) * (b[5] - b[2]);
            if (s->importance > bestImp || (s->importance == bestImp && vol < bestVol)) { best = s; bestImp = s->importance; bestVol = vol; }
            continue;
        }
        float c[3]; s->WorldCenter(c);
        const float d = (c[0] - pos[0]) * (c[0] - pos[0]) + (c[1] - pos[1]) * (c[1] - pos[1]) + (c[2] - pos[2]) * (c[2] - pos[2]);
        if (d < nearD) { nearD = d; near = s; }
    }
    return best ? best : (onlyInside ? NULL : near);
}
// las de la escena (las borradas que guarda el Ctrl+Z no: ver ReflectionProbeInScene)
void ReflectionProbeList(std::vector<ReflectionProbe*>& out) {
    out.clear();
    for (size_t i = 0; i < gProbes.size(); i++) if (gProbes[i] && ReflectionProbeInScene(gProbes[i])) out.push_back(gProbes[i]);
}
bool ReflectionProbeIsOwn(const Texture* t) {
    if (!t) return false;
    for (size_t i = 0; i < gProbes.size(); i++) {
        if (!gProbes[i]) continue;
        if (t == &gProbes[i]->live || (!gProbes[i]->texture.empty() && gProbes[i]->texture == t->path)) return true;
    }
    return false;
}

bool ReflectionProbeBakeBegin(ReflectionProbe* s, std::string* reason) {
#ifdef W3D_NO_EDITOR
    (void)s; if (reason) *reason = "Nothing to bake"; return false;   // (el juego no hornea: usa la imagen del .w3d)
#else
    if (!s) return false;
    if (s->type == W3D_REFLECTION_CUSTOMIZED) { if (reason) *reason = "A custom probe uses the image you choose"; return false; }
    // (la de tiempo real no se hornea: la dibuja la GPU, render/SceneRender.cpp)
    if (s->type == W3D_REFLECTION_TIME_REAL) { if (reason) *reason = "A realtime probe is drawn by the GPU"; return false; }
    const int w = s->resolution < 32 ? 32 : (s->resolution > 2048 ? 2048 : s->resolution);
    const int h = w / 2;
    float c[3]; s->WorldCenter(c);
    // (el FONDO es el de la escena, tal cual: el color o el 360)
    return RTProbeBeginReflection(c, w, h, s->near, s->samples, s->bounces, s->intensity, reason);
#endif
}

bool ReflectionProbeBakeFinish(ReflectionProbe* s, std::string* reason) {
#ifdef W3D_NO_EDITOR
    (void)s; (void)reason; return false;
#else
    if (!s) return false;
    const int w = s->resolution < 32 ? 32 : (s->resolution > 2048 ? 2048 : s->resolution);
    const int h = w / 2;
    std::vector<unsigned char> rgba;
    if (!RTProbeTakeReflection(rgba) || (int)rgba.size() != w * h * 4) { if (reason) *reason = "Nothing to bake"; return false; }
    // LA TEXTURA: la misma si ya tenia una del mismo tamanio; si no, una nueva del proyecto (en memoria hasta guardar)
    Texture* t = s->texture.empty() ? NULL : TextureFind(s->texture);
    TextureEditable* te = t ? TexEditGet(t) : NULL;
    if (te && te->w == w && te->h == h && (int)te->rgba.size() == w * h * 4) {
        te->rgba = rgba;
        te->modified = true;
        TexEditUpload(te, 0, 0, w, h);
    } else {
        t = TexEditCreate("Reflejo " + s->name, rgba, w, h, false);
        if (!t) { if (reason) *reason = "Could not create the texture"; return false; }
        s->texture = t->path;
    }
    s->Image();
    s->version++;   // (la copia borrosa del PBR se rehace)
    s->lastMs = w3dGetTicks();
    g_redraw = true;
    return true;
#endif
}

// DE UNA SOLA VEZ (el harness)
bool ReflectionProbeBake(ReflectionProbe* s, std::string* reason) {
    if (!ReflectionProbeBakeBegin(s, reason)) return false;
#ifndef W3D_NO_EDITOR
    while (RTProbeActive()) RTProbeStep(100000);
#endif
    return ReflectionProbeBakeFinish(s, reason);
}

ReflectionProbe* ReflectionProbeDuplicate(ReflectionProbe* src) {
    if (!src) return NULL;
    ReflectionProbe* d = new ReflectionProbe(src->Parent, src->pos);
    d->type = src->type; d->importance = src->importance; d->intensity = src->intensity; d->box = src->box;
    for (int k = 0; k < 3; k++) { d->size[k] = src->size[k]; d->origin[k] = src->origin[k]; }
    for (int k = 0; k < 4; k++) d->backgroundColor[k] = src->backgroundColor[k];
    d->resolution = src->resolution; d->background = src->background; d->near = src->near; d->eachSeg = src->eachSeg;
    d->samples = src->samples; d->bounces = src->bounces;
    d->texture = src->texture;   // (la misma imagen hasta que se hornee otra: el horneado de la copia crea la suya)
    return d;
}

// ---- .w3d ----
static void SrSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
static void SrEsc(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) { const char c = v[i]; if (c == '"' || c == '\\') { s += '\\'; s += c; } else s += c; }
    s += '"';
}
static void SrVec(std::string& s, int ind, const char* k, const float* v, int n) {
    s += ",\n"; SrSang(s, ind); s += "\""; s += k; s += "\": [";
    for (int i = 0; i < n; i++) { if (i) s += ", "; s += JsonNumText(v[i]); }
    s += "]";
}
void ReflectionProbeWriteFields(std::string& s, int ind, const ReflectionProbe* o) {
    static const char* kTypes[3] = { "baked", "custom", "realTime" };
    s += ",\n"; SrSang(s, ind); s += "\"reflectionType\": \""; s += kTypes[o->type < 0 || o->type > 2 ? 0 : o->type]; s += "\"";
    s += ",\n"; SrSang(s, ind); s += "\"importance\": "; s += JsonNumText((float)o->importance);
    s += ",\n"; SrSang(s, ind); s += "\"intensity\": "; s += JsonNumText(o->intensity);
    s += ",\n"; SrSang(s, ind); s += "\"box\": "; s += o->box ? "true" : "false";
    SrVec(s, ind, "size", o->size, 3);
    SrVec(s, ind, "origin", o->origin, 3);
    s += ",\n"; SrSang(s, ind); s += "\"resolution\": "; s += JsonNumText((float)o->resolution);
    s += ",\n"; SrSang(s, ind); s += "\"near\": "; s += JsonNumText(o->near);
    s += ",\n"; SrSang(s, ind); s += "\"samples\": "; s += JsonNumText((float)o->samples);
    s += ",\n"; SrSang(s, ind); s += "\"bounces\": "; s += JsonNumText((float)o->bounces);
    s += ",\n"; SrSang(s, ind); s += "\"everySec\": "; s += JsonNumText(o->eachSeg);
    if (!o->texture.empty()) { s += ",\n"; SrSang(s, ind); s += "\"texture\": "; SrEsc(s, o->texture); }
}
void ReflectionProbeReadFields(JVal* j, ReflectionProbe* o) {
    if (!j || !o) return;
    const std::string t = JS(j, "reflectionType", "baked");
    o->type = t == "custom" ? W3D_REFLECTION_CUSTOMIZED : t == "realTime" ? W3D_REFLECTION_TIME_REAL : W3D_REFLECTION_BAKED;
    o->importance = JI(j, "importance", 1);
    o->intensity = JF(j, "intensity", 1.0f);
    o->box  = JB(j, "box", false);
    JVal* v = JChild(j, "size", 5);
    if (v) for (size_t i = 0; i < 3 && i < JRowLen(v); i++) o->size[i] = fabsf(JRowNum(v, i, 10.0f));
    v = JChild(j, "origin", 5);
    if (v) for (size_t i = 0; i < 3 && i < JRowLen(v); i++) o->origin[i] = JRowNum(v, i, 0.0f);
    o->resolution = JI(j, "resolution", 256);
    o->background = 0;   // (el "fondo de color" de antes ya no existe: se hornea el fondo de la escena)
    o->near       = JF(j, "near", 0.3f);
    o->samples = JI(j, "samples", 32); if (o->samples < 1) o->samples = 1;
    o->bounces = JI(j, "bounces", 3); if (o->bounces < 0) o->bounces = 0;
    o->eachSeg = JF(j, "everySec", 2.0f);
    o->texture = JS(j, "texture", "");
    // (una ruta del contenedor: la del proyecto que se esta leyendo; el contexto de una libreria la prefija)
}
