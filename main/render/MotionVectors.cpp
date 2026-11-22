// ============================================================================
//  MotionVectors.cpp - el pase de VECTORES DE MOVIMIENTO (GL, color por vertice) y el desenfoque por CPU.
//  Ver MotionVectors.h (la codificacion, el uso).
// ============================================================================
#include "render/MotionVectors.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "animation/SkeletalAnimation.h"   // SkinMesh: la pose del esqueleto al cuadro actual
#include "animation/VertexAnimation.h"     // W3dUpToDatePose: la de la vertex anim
#include "graphics/w3dGraphics.h"
#include <map>
#include <math.h>
#include <string.h>

// lo RECORDADO del cuadro anterior: las posiciones en el MUNDO de cada malla (las que dibujo) y la matriz de la camara
// DOS RANURAS: 0 = la del render a PNG (Render Animation la arrastra de un cuadro al otro), 1 = la de la vista "Motion
// Vectors" del viewport (se rearma en cada dibujo; si fuera la misma, redibujar la UI en medio de una animacion se la
// cambiaba al render)
struct VecSlot { std::map<const Mesh*, std::vector<float> > ant; float vp[16]; bool has; VecSlot() : has(false) {} };
static VecSlot gSlots[2];
static int gSlot = 0;
#define gPrev    gSlots[gSlot].ant
#define gVPPrev  gSlots[gSlot].vp
#define gHasPrev gSlots[gSlot].has
void W3dVecUseSlot(int r) { gSlot = (r == 1) ? 1 : 0; }

// lo que DIBUJA una malla (la generada por los modificadores, la deformada por el esqueleto o la de siempre)
struct MeshDrawing { const float* pos; int nV; const MeshIndex* idx; int nIdx; };
static bool MeshOf(Mesh* m, MeshDrawing& d) {
    if (m->skinArmature) SkinMesh(m); else W3dUpToDatePose(m);   // (la pose al cuadro ACTUAL, aunque no se haya dibujado)
    const bool gen = m->genValid && m->genVertex && m->genFaces && m->genVertexSize > 0 && m->genFacesSize > 0;
    if (gen) { d.pos = m->genVertex; d.nV = m->genVertexSize; d.idx = m->genFaces; d.nIdx = m->genFacesSize; }
    else {
        d.pos = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex;
        d.nV = m->vertexSize; d.idx = m->faces; d.nIdx = m->facesSize;
    }
    return d.pos && d.nV > 0 && d.idx && d.nIdx >= 3;
}
static void ToWorld(Mesh* m, const MeshDrawing& d, std::vector<float>& out) {
    Matrix4 W; m->GetWorldMatrix(W);
    const float* M = W.m;
    out.resize((size_t)d.nV * 3);
    for (int i = 0; i < d.nV; i++) {
        const float x = d.pos[i*3], y = d.pos[i*3+1], z = d.pos[i*3+2];
        out[(size_t)i*3]   = M[0]*x + M[4]*y + M[8]*z  + M[12];
        out[(size_t)i*3+1] = M[1]*x + M[5]*y + M[9]*z  + M[13];
        out[(size_t)i*3+2] = M[2]*x + M[6]*y + M[10]*z + M[14];
    }
}
// las mallas que se ven (como el render: visible, renderizable, con todos sus padres visibles)
static void Meshes(Object* o, std::vector<Mesh*>& out, bool root) {
    if (!o) return;
    if (!root && (!o->visible || !o->renderable)) return;
    if (o->getType() == ObjectType::mesh) out.push_back((Mesh*)o);
    for (size_t i = 0; i < o->Childrens.size(); i++) Meshes(o->Childrens[i], out, false);
}

void W3dVecForget() { gPrev.clear(); gHasPrev = false; }
bool W3dVecHasPrev() { return gHasPrev; }

void W3dVecRemember(Object* root, const float* VP16) {
    gPrev.clear();
    std::vector<Mesh*> ms; Meshes(root, ms, true);
    for (size_t i = 0; i < ms.size(); i++) {
        MeshDrawing d; if (!MeshOf(ms[i], d)) continue;
        ToWorld(ms[i], d, gPrev[ms[i]]);
    }
    memcpy(gVPPrev, VP16, sizeof(gVPPrev));
    gHasPrev = true;
}

void W3dVecBackgroundColor(float* c) { c[0] = 128.0f / 255.0f; c[1] = 128.0f / 255.0f; c[2] = 0.0f; c[3] = 1.0f; }

// el punto del mundo -> pixel de la imagen (Y hacia abajo); false = detras de la camara
static inline bool APixel(const float* VP, float x, float y, float z, int w, int h, float& px, float& py) {
    const float cx = VP[0]*x + VP[4]*y + VP[8]*z  + VP[12];
    const float cy = VP[1]*x + VP[5]*y + VP[9]*z  + VP[13];
    const float cw = VP[3]*x + VP[7]*y + VP[11]*z + VP[15];
    if (cw <= 1e-6f) return false;
    px = (cx / cw * 0.5f + 0.5f) * (float)w;
    py = (0.5f - cy / cw * 0.5f) * (float)h;
    return true;
}
static inline unsigned char Encode(float d) {
    float v = 128.0f + d * W3D_VEC_STEPS_BY_PIXEL;
    if (v < 1.0f) v = 1.0f; if (v > 255.0f) v = 255.0f;   // (1..255: simetrico alrededor de 128)
    return (unsigned char)(v + 0.5f);
}

// el pase: con shaders (ES 2.0 / Vulkan) la GPU calcula el movimiento en cada pixel con las posiciones de ahora y las del
// anterior (PassVectors / PassData: ademas la profundidad); sin shaders, el color de cada vertice desde la CPU
static void DrawMotion(Object* root, const float* VP16, int outW, int outH, int pass, float far) {
    namespace gfx = w3dEngine;
    const bool gpu = gfx::PassesWithShader();
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Fog); gfx::Disable(gfx::Blend);
    gfx::Disable(gfx::CullFace);
    gfx::UnbindVBOs();
    gfx::EnableArray(gfx::VertexArray);
    if (gpu) gfx::DisableArray(gfx::ColorArray); else gfx::EnableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::TexCoordArray);
    if (gpu) {
        float p[35];
        memcpy(p, gHasPrev ? gVPPrev : VP16, sizeof(float) * 16);   // (sin anterior: quieto)
        p[16] = (float)outW; p[17] = (float)outH; p[18] = far;
        memcpy(p + 19, VP16, sizeof(float) * 16);
        gfx::SpecialPass(pass, p);
    }
    static std::vector<float> world;
    static std::vector<unsigned char> col;
    std::vector<Mesh*> ms; Meshes(root, ms, true);
    for (size_t i = 0; i < ms.size(); i++) {
        Mesh* m = ms[i];
        MeshDrawing d; if (!MeshOf(m, d)) continue;
        ToWorld(m, d, world);
        // el anterior de ESTA malla (si cambio la cantidad de vertices, o no estaba: solo se mueve la camara)
        const float* ant = &world[0];
        if (gHasPrev) {
            std::map<const Mesh*, std::vector<float> >::const_iterator it = gPrev.find(m);
            if (it != gPrev.end() && it->second.size() == world.size()) ant = &it->second[0];
        }
        gfx::VertexPointer3f(0, &world[0]);
        if (gpu) gfx::PrevPointer3f(ant);
        else {
            col.resize((size_t)d.nV * 4);
            for (int v = 0; v < d.nV; v++) {
                const float* p = &world[(size_t)v*3];
                const float* q = ant + (size_t)v*3;
                float x1, y1, x0, y0, dx = 0.0f, dy = 0.0f;
                if (gHasPrev && APixel(VP16, p[0], p[1], p[2], outW, outH, x1, y1) &&
                    APixel(gVPPrev, q[0], q[1], q[2], outW, outH, x0, y0)) { dx = x1 - x0; dy = y1 - y0; }
                unsigned char* c = &col[(size_t)v*4];
                c[0] = Encode(dx); c[1] = Encode(dy); c[2] = 255; c[3] = 255;
            }
            gfx::ColorPointer4ub(&col[0]);
        }
        // (por partes, como el render: los indices fuera de rango de una parte no se dibujan)
        gfx::DrawTriangles(d.nIdx - d.nIdx % 3, d.idx);
    }
    if (gpu) { gfx::PrevPointer3f(NULL); gfx::SpecialPass(gfx::PassNone, NULL); }
    gfx::DisableArray(gfx::ColorArray);
}
void W3dVecDraw(Object* root, const float* VP16, int outW, int outH) {
    DrawMotion(root, VP16, outW, outH, w3dEngine::PassVectors, 1.0f);
}
bool W3dVecDrawData(Object* root, const float* VP16, int outW, int outH, float far) {
    if (!w3dEngine::PassesWithShader()) return false;
    DrawMotion(root, VP16, outW, outH, w3dEngine::PassData, far);
    return true;
}
void W3dVecBackgroundColorData(float* c) { c[0] = c[1] = c[2] = c[3] = 1.0f; }   // (lo mas lejos)

// ---------------------------------------------------------------------------
//  DESENFOQUE: 'muestras' lecturas a lo largo del vector de cada pixel, centradas en el (de -v/2 a +v/2), con el
//  obturador escalando el vector. Lo quieto (128,128) se copia tal cual. Enteros donde se puede (el N95)
// ---------------------------------------------------------------------------
void W3dMotionBlur(const unsigned char* rgba, const unsigned char* vec, int w, int h, float shutter, int samples,
                             unsigned char* out) {
    if (!rgba || !vec || !out || w <= 0 || h <= 0) return;
    if (samples < 2) samples = 2; if (samples > 64) samples = 64;
    if (shutter < 0.0f) shutter = 0.0f;
    // el paso del vector en 16.16: (codigo - 128) / PASOS_POR_PIXEL * obturador pixeles
    const int scale = (int)(shutter / W3D_VEC_STEPS_BY_PIXEL * 65536.0f + 0.5f);
    std::vector<unsigned char> copy;
    const unsigned char* src = rgba;
    if (out == rgba) { copy.assign(rgba, rgba + (size_t)w * h * 4); src = &copy[0]; }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const size_t i = ((size_t)y * w + x) * 4;
            const int vx = ((int)vec[i] - 128) * scale, vy = ((int)vec[i + 1] - 128) * scale;   // 16.16, en pixeles
            if (vx > -32768 && vx < 32768 && vy > -32768 && vy < 32768) {   // menos de medio pixel: igual
                out[i] = src[i]; out[i+1] = src[i+1]; out[i+2] = src[i+2]; out[i+3] = src[i+3];
                continue;
            }
            unsigned int s0 = 0, s1 = 0, s2 = 0, s3 = 0;
            for (int k = 0; k < samples; k++) {
                // t = k / (muestras-1) - 0.5, en 16.16
                const int t = (int)(((long long)k * 65536) / (samples - 1)) - 32768;
                int sx = x + (int)(((long long)vx * t) >> 32), sy = y + (int)(((long long)vy * t) >> 32);
                if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
                if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
                const unsigned char* p = src + ((size_t)sy * w + sx) * 4;
                s0 += p[0]; s1 += p[1]; s2 += p[2]; s3 += p[3];
            }
            out[i] = (unsigned char)(s0 / samples); out[i+1] = (unsigned char)(s1 / samples);
            out[i+2] = (unsigned char)(s2 / samples); out[i+3] = (unsigned char)(s3 / samples);
        }
    }
}
