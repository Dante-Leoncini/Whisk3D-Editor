// ============================================================================
//  RayTracer.cpp - trazado de rayos por CPU (ver RayTracer.h). Todo escrito de cero para Whisk3D.
//
//  Estructura (pensada para portar el nucleo a CUDA/RTX):
//    RTEscena  = arrays planos: posiciones/normales/uv de mundo, triangulos (3 indices + material),
//                materiales (color difuso, emision, textura), texturas (pixeles CPU), luces, y un BVH
//                (cajas alineadas, particion por la mediana del eje mas largo, recorrido iterativo).
//    RTCamara  = base ortonormal + fov/aspecto (perspectiva u ortografica), misma cuenta que ProyectarPunto.
//    RTRender  = acumulador float RGB + imagen de 8 bits + tiles; Avanzar(presupuesto) hace tiles hasta
//                agotar el tiempo (o todos los del pase); con hilos, cada uno toma tiles de un contador.
// ============================================================================
#include "render/RayTracer.h"
#include "ui/ViewPorts/ViewPort3D.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Light.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Collection.h"      // SceneCollection
#include "objects/Camera.h"          // CameraActive (mirar por la camara)
#include "WhiskUI/theme/colores.h"   // SetColorID
#include "io/TexturaEditada.h"      // los pixeles CPU de una textura (para muestrearla)
#include "render/OpcionesRender.h"  // g_renderBg
#include "app/variables.h"          // fovDeg, SceneCollection
#include "config/W3dProfile.h"      // W3dNowMs
#include "W3dLang.h"
#include "w3dGraphics.h"
#include "w3dTexture.h"             // UploadRGBA / UpdateRGBA / SavePNG
#include "WhiskUI/draw/glesdraw.h"  // W3dDrawStrip4
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/core/UI.h"        // GlobalScale, gapGS
#include "ui/ViewPorts/PopUp/ProgressPopup.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#if !defined(W3D_SYMBIAN) && !defined(__EMSCRIPTEN__)
    #define RT_HILOS 1
    #include <thread>
    #include <atomic>
#endif

extern bool g_redraw;
W3dRTOpciones g_rt;

// ---------------------------------------------------------------------------
//  vectores chicos (float[3]) sin clases: lo que mas facil se traduce a un kernel
// ---------------------------------------------------------------------------
struct RTV { float x, y, z; };
static inline RTV V3(float x, float y, float z) { RTV v; v.x = x; v.y = y; v.z = z; return v; }
static inline RTV operator+(const RTV& a, const RTV& b) { return V3(a.x+b.x, a.y+b.y, a.z+b.z); }
static inline RTV operator-(const RTV& a, const RTV& b) { return V3(a.x-b.x, a.y-b.y, a.z-b.z); }
static inline RTV operator*(const RTV& a, float s) { return V3(a.x*s, a.y*s, a.z*s); }
static inline float Dot(const RTV& a, const RTV& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline RTV Cruz(const RTV& a, const RTV& b) { return V3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x); }
static inline float Largo(const RTV& a) { return sqrtf(Dot(a, a)); }
static inline RTV Norm(const RTV& a) { const float l = Largo(a); return (l > 1e-12f) ? a * (1.0f / l) : V3(0, 0, 1); }

// xorshift32: rapido, sin estado global (uno por hilo/pixel)
struct RTRng { unsigned int s; RTRng(unsigned int seed) : s(seed ? seed : 0x9E3779B9u) {}
    inline unsigned int U() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float F() { return (float)(U() & 0xFFFFFF) / 16777216.0f; } };   // [0,1)

// ---------------------------------------------------------------------------
//  escena plana
// ---------------------------------------------------------------------------
struct RTTri { int v0, v1, v2; int mat; };
// transparente: alpha = alpha del color x alpha de la textura (mezcla 0). Con mezcla ADITIVA (2) el color se SUMA
// a lo de atras y no tapa nada. Lo transparente deja pasar la luz en proporcion: alpha 0 = no proyecta sombra.
// metalico 0..1: esa fraccion del color es REFLEJO de la escena (teñido por el color del material), el resto difuso;
// rugosidad 0..1 desparrama el reflejo (0 = espejo, 1 = mate) y ensancha el brillo de las lamparas.
// nmTex: el NORMAL MAP (RGB = normal en tangent-space: R = tangente, G = bitangente, B = normal; la misma
// convencion que el pase DOT3 del render GL), -1 = sin normal map.
struct RTMaterial { float difuso[3]; float emision[3]; int tex; int nmTex; bool sinLuz; bool transparente; bool aditivo; float alfa; float rugosidad; float metalico; };
struct RTTextura { const unsigned char* px; int w, h; };
struct RTLuz { RTV pos; RTV dir; bool direccional; float color[3]; float ambiente[3]; float radio; int rayos; float attC, attL, attQ; };
struct RTNodo { float bmin[3], bmax[3]; int izq, der; int primero, cantidad; };   // hoja: cantidad > 0

struct RTEscena {
    std::vector<float> pos, nrm, uv;      // por vertice (mundo)
    std::vector<float> tan;               // por vertice: tangente de mundo (xyz) + mano (w); solo con normal map (sino 0)
    std::vector<RTTri> tris;
    std::vector<RTMaterial> mats;
    std::vector<RTTextura> texs;
    std::vector<RTLuz> luces;
    std::vector<RTNodo> nodos;
    std::vector<int> orden;               // indices de triangulos, en el orden de las hojas
    float bg[3];
    bool hayTransparentes;                // algun material transparente: los rayos de sombra atraviesan (sin any-hit)
    void Limpiar() { pos.clear(); nrm.clear(); uv.clear(); tan.clear(); tris.clear(); mats.clear(); texs.clear(); luces.clear(); nodos.clear(); orden.clear(); hayTransparentes = false; }
};

// ---- BVH: mediana del eje mas largo, hojas de hasta 4 triangulos ----
static void BVHCaja(const RTEscena& e, int tri, float* bmin, float* bmax) {
    const RTTri& t = e.tris[(size_t)tri];
    const int vs[3] = { t.v0, t.v1, t.v2 };
    for (int a = 0; a < 3; a++) { bmin[a] = 1e30f; bmax[a] = -1e30f; }
    for (int k = 0; k < 3; k++) for (int a = 0; a < 3; a++) {
        const float c = e.pos[(size_t)vs[k]*3 + a];
        if (c < bmin[a]) bmin[a] = c; if (c > bmax[a]) bmax[a] = c;
    }
}
struct BVHOrdenEje { const std::vector<float>* cen; int eje; bool operator()(int a, int b) const { return (*cen)[(size_t)a*3 + eje] < (*cen)[(size_t)b*3 + eje]; } };
static int BVHConstruir(RTEscena& e, std::vector<float>& cen, int primero, int cantidad) {
    RTNodo n; n.izq = n.der = -1; n.primero = primero; n.cantidad = 0;
    for (int a = 0; a < 3; a++) { n.bmin[a] = 1e30f; n.bmax[a] = -1e30f; }
    for (int i = 0; i < cantidad; i++) {
        float bmin[3], bmax[3]; BVHCaja(e, e.orden[(size_t)(primero + i)], bmin, bmax);
        for (int a = 0; a < 3; a++) { if (bmin[a] < n.bmin[a]) n.bmin[a] = bmin[a]; if (bmax[a] > n.bmax[a]) n.bmax[a] = bmax[a]; }
    }
    const int idx = (int)e.nodos.size();
    e.nodos.push_back(n);
    if (cantidad <= 4) { e.nodos[(size_t)idx].cantidad = cantidad; return idx; }
    int eje = 0; float ext = n.bmax[0] - n.bmin[0];
    for (int a = 1; a < 3; a++) if (n.bmax[a] - n.bmin[a] > ext) { ext = n.bmax[a] - n.bmin[a]; eje = a; }
    BVHOrdenEje cmp; cmp.cen = &cen; cmp.eje = eje;
    std::sort(e.orden.begin() + primero, e.orden.begin() + primero + cantidad, cmp);
    const int mitad = cantidad / 2;
    const int izq = BVHConstruir(e, cen, primero, mitad);
    const int der = BVHConstruir(e, cen, primero + mitad, cantidad - mitad);
    e.nodos[(size_t)idx].izq = izq; e.nodos[(size_t)idx].der = der;
    return idx;
}
static void BVHArmar(RTEscena& e) {
    e.nodos.clear(); e.orden.clear();
    const int n = (int)e.tris.size();
    if (n == 0) return;
    std::vector<float> cen((size_t)n * 3);
    e.orden.resize((size_t)n);
    for (int i = 0; i < n; i++) {
        e.orden[(size_t)i] = i;
        const RTTri& t = e.tris[(size_t)i];
        for (int a = 0; a < 3; a++) cen[(size_t)i*3 + a] = (e.pos[(size_t)t.v0*3 + a] + e.pos[(size_t)t.v1*3 + a] + e.pos[(size_t)t.v2*3 + a]) / 3.0f;
    }
    e.nodos.reserve((size_t)n * 2);
    BVHConstruir(e, cen, 0, n);
}

// ---- interseccion ----
struct RTHit { float t; float b1, b2; int tri; };
static inline bool RayoTri(const RTEscena& e, int tri, const RTV& o, const RTV& d, float tMin, float tMax, RTHit& h) {
    const RTTri& T = e.tris[(size_t)tri];
    const RTV p0 = V3(e.pos[(size_t)T.v0*3], e.pos[(size_t)T.v0*3+1], e.pos[(size_t)T.v0*3+2]);
    const RTV p1 = V3(e.pos[(size_t)T.v1*3], e.pos[(size_t)T.v1*3+1], e.pos[(size_t)T.v1*3+2]);
    const RTV p2 = V3(e.pos[(size_t)T.v2*3], e.pos[(size_t)T.v2*3+1], e.pos[(size_t)T.v2*3+2]);
    const RTV e1 = p1 - p0, e2 = p2 - p0;
    const RTV pv = Cruz(d, e2);
    const float det = Dot(e1, pv);
    if (det > -1e-9f && det < 1e-9f) return false;
    const float inv = 1.0f / det;
    const RTV tv = o - p0;
    const float b1 = Dot(tv, pv) * inv; if (b1 < 0.0f || b1 > 1.0f) return false;
    const RTV qv = Cruz(tv, e1);
    const float b2 = Dot(d, qv) * inv; if (b2 < 0.0f || b1 + b2 > 1.0f) return false;
    const float t = Dot(e2, qv) * inv;
    if (t <= tMin || t >= tMax) return false;
    h.t = t; h.b1 = b1; h.b2 = b2; h.tri = tri;
    return true;
}
static inline bool RayoCaja(const float* bmin, const float* bmax, const RTV& o, const RTV& invD, float tMax) {
    float t0 = 0.0f, t1 = tMax;
    const float ox[3] = { o.x, o.y, o.z }, id[3] = { invD.x, invD.y, invD.z };
    for (int a = 0; a < 3; a++) {
        float ta = (bmin[a] - ox[a]) * id[a], tb = (bmax[a] - ox[a]) * id[a];
        if (ta > tb) { const float c = ta; ta = tb; tb = c; }
        if (ta > t0) t0 = ta; if (tb < t1) t1 = tb;
        if (t0 > t1) return false;
    }
    return true;
}
// el mas cercano (sombra=false) o CUALQUIERA antes de tMax (sombra=true). Con tMin/excluido se sigue el MISMO rayo
// detras de una superficie transparente: no se avanza el origen (eso saltaba una cara opaca coplanar, p.ej. una
// calcomania pegada a una pared), se descarta solo el triangulo ya atravesado.
static bool Trazar(const RTEscena& e, const RTV& o, const RTV& d, float tMin, float tMax, bool sombra, RTHit& mejor, int excluido = -1) {
    if (e.nodos.empty()) return false;
    const RTV invD = V3(1.0f / (fabsf(d.x) > 1e-12f ? d.x : 1e-12f), 1.0f / (fabsf(d.y) > 1e-12f ? d.y : 1e-12f), 1.0f / (fabsf(d.z) > 1e-12f ? d.z : 1e-12f));
    int pila[64]; int sp = 0; pila[sp++] = 0;
    bool hay = false; mejor.t = tMax;
    while (sp > 0) {
        const RTNodo& n = e.nodos[(size_t)pila[--sp]];
        if (!RayoCaja(n.bmin, n.bmax, o, invD, mejor.t)) continue;
        if (n.cantidad > 0) {
            for (int i = 0; i < n.cantidad; i++) {
                const int tri = e.orden[(size_t)(n.primero + i)];
                if (tri == excluido) continue;
                RTHit h;
                if (RayoTri(e, tri, o, d, tMin, mejor.t, h)) { mejor = h; hay = true; if (sombra) return true; }
            }
        } else {
            if (n.izq >= 0 && sp < 63) pila[sp++] = n.izq;
            if (n.der >= 0 && sp < 63) pila[sp++] = n.der;
        }
    }
    return hay;
}

// ---------------------------------------------------------------------------
//  armado de la escena desde los objetos del editor
// ---------------------------------------------------------------------------
static int MaterialIdx(RTEscena& e, Material* mt, std::vector<Material*>& vistos) {
    for (size_t i = 0; i < vistos.size(); i++) if (vistos[i] == mt) return (int)i;
    RTMaterial rm;
    rm.difuso[0] = mt ? mt->diffuse[0] : 0.8f; rm.difuso[1] = mt ? mt->diffuse[1] : 0.8f; rm.difuso[2] = mt ? mt->diffuse[2] : 0.8f;
    rm.emision[0] = mt ? mt->emission[0] : 0.0f; rm.emision[1] = mt ? mt->emission[1] : 0.0f; rm.emision[2] = mt ? mt->emission[2] : 0.0f;
    rm.tex = -1; rm.nmTex = -1;
    rm.sinLuz = mt ? !mt->lighting : false;
    if (mt && mt->normalMap && mt->normalTexture) {
        TexturaEditable* tn = TexEditObtener(mt->normalTexture);
        if (tn && tn->w > 0 && tn->h > 0) { RTTextura tx; tx.px = &tn->rgba[0]; tx.w = tn->w; tx.h = tn->h; e.texs.push_back(tx); rm.nmTex = (int)e.texs.size() - 1; }
    }
    rm.rugosidad = mt ? mt->rtRugosidad : 0.5f; if (rm.rugosidad < 0.0f) rm.rugosidad = 0.0f; if (rm.rugosidad > 1.0f) rm.rugosidad = 1.0f;
    rm.metalico = mt ? mt->rtMetalico : 0.0f; if (rm.metalico < 0.0f) rm.metalico = 0.0f; if (rm.metalico > 1.0f) rm.metalico = 1.0f;
    rm.transparente = mt ? mt->transparent : false;
    rm.aditivo = rm.transparente && mt && mt->mezcla == 2;
    rm.alfa = mt ? mt->diffuse[3] : 1.0f; if (rm.alfa < 0.0f) rm.alfa = 0.0f; if (rm.alfa > 1.0f) rm.alfa = 1.0f;
    if (rm.transparente) e.hayTransparentes = true;
    if (mt && mt->textureOn && mt->texture && mt->texture->iID) {
        TexturaEditable* te = TexEditObtener(mt->texture);   // los pixeles CPU (se decodifican una vez)
        if (te && te->w > 0 && te->h > 0) { RTTextura tx; tx.px = &te->rgba[0]; tx.w = te->w; tx.h = te->h; e.texs.push_back(tx); rm.tex = (int)e.texs.size() - 1; }
    }
    e.mats.push_back(rm); vistos.push_back(mt);
    return (int)e.mats.size() - 1;
}
// TANGENTES por vertice (mundo) para los vertices [v0, v1) desde los triangulos [t0, t1) ya cargados: la
// direccion de U sobre la superficie, perpendicular a la normal (Gram-Schmidt) + la mano (w) para la bitangente.
// Misma cuenta que Mesh::CalcularTangentes, pero sobre los arrays de MUNDO del trazador (sirve para la malla
// generada por modificadores y la deformada por esqueleto, que no tienen tangentes propias).
static void TangentesDe(RTEscena& e, int v0, int v1, size_t t0, size_t t1) {
    if (e.tan.size() < e.pos.size() / 3 * 4) e.tan.resize(e.pos.size() / 3 * 4, 0.0f);
    const int n = v1 - v0; if (n <= 0) return;
    std::vector<float> tA((size_t)n * 3, 0.0f), bA((size_t)n * 3, 0.0f);
    for (size_t t = t0; t < t1; t++) {
        const RTTri& T = e.tris[t];
        const int a = T.v0, b = T.v1, c = T.v2;
        if (a < v0 || a >= v1 || b < v0 || b >= v1 || c < v0 || c >= v1) continue;
        const RTV p0 = V3(e.pos[(size_t)a*3], e.pos[(size_t)a*3+1], e.pos[(size_t)a*3+2]);
        const RTV p1 = V3(e.pos[(size_t)b*3], e.pos[(size_t)b*3+1], e.pos[(size_t)b*3+2]);
        const RTV p2 = V3(e.pos[(size_t)c*3], e.pos[(size_t)c*3+1], e.pos[(size_t)c*3+2]);
        const float du1 = e.uv[(size_t)b*2] - e.uv[(size_t)a*2], dv1 = e.uv[(size_t)b*2+1] - e.uv[(size_t)a*2+1];
        const float du2 = e.uv[(size_t)c*2] - e.uv[(size_t)a*2], dv2 = e.uv[(size_t)c*2+1] - e.uv[(size_t)a*2+1];
        const float d = du1 * dv2 - du2 * dv1;
        if (d > -1e-9f && d < 1e-9f) continue;   // triangulo degenerado en UV
        const float r = 1.0f / d;
        const RTV e1 = p1 - p0, e2 = p2 - p0;
        const RTV Tg = (e1 * dv2 - e2 * dv1) * r, Bt = (e2 * du1 - e1 * du2) * r;
        const int vs[3] = { a - v0, b - v0, c - v0 };
        for (int k = 0; k < 3; k++) { float* ta = &tA[(size_t)vs[k]*3]; float* ba = &bA[(size_t)vs[k]*3];
            ta[0] += Tg.x; ta[1] += Tg.y; ta[2] += Tg.z; ba[0] += Bt.x; ba[1] += Bt.y; ba[2] += Bt.z; }
    }
    for (int i = 0; i < n; i++) {
        const int v = v0 + i;
        const RTV N = V3(e.nrm[(size_t)v*3], e.nrm[(size_t)v*3+1], e.nrm[(size_t)v*3+2]);
        RTV T = V3(tA[(size_t)i*3], tA[(size_t)i*3+1], tA[(size_t)i*3+2]);
        T = T - N * Dot(N, T);
        if (Dot(T, T) < 1e-12f) { T = V3(1, 0, 0); if (fabsf(Dot(N, T)) > 0.9f) T = V3(0, 1, 0); T = T - N * Dot(N, T); }
        T = Norm(T);
        const RTV B = V3(bA[(size_t)i*3], bA[(size_t)i*3+1], bA[(size_t)i*3+2]);
        const float mano = (Dot(Cruz(N, T), B) < 0.0f) ? -1.0f : 1.0f;
        e.tan[(size_t)v*4] = T.x; e.tan[(size_t)v*4+1] = T.y; e.tan[(size_t)v*4+2] = T.z; e.tan[(size_t)v*4+3] = mano;
    }
}
static void AgregarMallaFin(RTEscena& e, int base, int nV, size_t trisAntes);
static void AgregarMalla(RTEscena& e, Mesh* m, std::vector<Material*>& vistos) {
    Matrix4 W; m->GetWorldMatrix(W);
    const size_t trisAntes = e.tris.size();
    const bool gen = m->genValido && m->genVertex && m->genFaces && m->genVertexSize > 0 && m->genFacesSize > 0;
    const GLfloat* P = gen ? m->genVertex : ((m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex);
    const GLbyte*  N = gen ? m->genNormals : m->normals;
    const GLfloat* UV = gen ? m->genUV : m->uv;
    const int nV = gen ? m->genVertexSize : m->vertexSize;
    if (!P || nV <= 0) return;
    const int base = (int)(e.pos.size() / 3);
    // normal: la matriz sin traslacion (escala uniforme: se normaliza al final)
    for (int i = 0; i < nV; i++) {
        const Vector3 wp = W * Vector3(P[i*3], P[i*3+1], P[i*3+2]);
        e.pos.push_back(wp.x); e.pos.push_back(wp.y); e.pos.push_back(wp.z);
        float nx = 0, ny = 1, nz = 0;
        if (N) { const float lx = N[i*3] / 127.0f, ly = N[i*3+1] / 127.0f, lz = N[i*3+2] / 127.0f;
                 nx = W.m[0]*lx + W.m[4]*ly + W.m[8]*lz; ny = W.m[1]*lx + W.m[5]*ly + W.m[9]*lz; nz = W.m[2]*lx + W.m[6]*ly + W.m[10]*lz; }
        const float l = sqrtf(nx*nx + ny*ny + nz*nz); if (l > 1e-9f) { nx /= l; ny /= l; nz /= l; }
        e.nrm.push_back(nx); e.nrm.push_back(ny); e.nrm.push_back(nz);
        e.uv.push_back(UV ? UV[i*2] : 0.0f); e.uv.push_back(UV ? UV[i*2+1] : 0.0f);
    }
    if (gen) {
        const std::vector<MaterialGroup>& G = m->genMaterialsGroup;
        for (size_t g = 0; g < G.size(); g++) {
            const int mi = MaterialIdx(e, G[g].material, vistos);
            for (int t = G[g].start; t < G[g].start + G[g].count && (t + 1) * 3 <= m->genFacesSize; t++) {
                RTTri tr; tr.v0 = base + m->genFaces[t*3]; tr.v1 = base + m->genFaces[t*3+1]; tr.v2 = base + m->genFaces[t*3+2]; tr.mat = mi;
                e.tris.push_back(tr);
            }
        }
    } else if (m->faces3d.empty() && m->faces && m->facesSize >= 3) {
        // sin caras logicas (malla cargada solo con su index buffer): triangulos por rango de mesh part
        const std::vector<MaterialGroup>& G = m->materialsGroup;
        for (size_t g = 0; g < G.size(); g++) {
            const int mi = MaterialIdx(e, G[g].material, vistos);
            for (int t = G[g].start; t < G[g].start + G[g].count && (t + 1) * 3 <= m->facesSize; t++) {
                const int a = m->faces[t*3], b = m->faces[t*3+1], c = m->faces[t*3+2];
                if (a >= nV || b >= nV || c >= nV) continue;
                RTTri tr; tr.v0 = base + a; tr.v1 = base + b; tr.v2 = base + c; tr.mat = mi;
                e.tris.push_back(tr);
            }
        }
    } else {
        for (size_t f = 0; f < m->faces3d.size(); f++) {
            const MeshFace& F = m->faces3d[f]; const int n = (int)F.idx.size(); if (n < 3) continue;
            Material* mt = (F.mat >= 0 && F.mat < (int)m->materialsGroup.size()) ? m->materialsGroup[(size_t)F.mat].material : NULL;
            const int mi = MaterialIdx(e, mt, vistos);
            for (int t = 1; t + 1 < n; t++) {
                const int a = F.idx[0], b = F.idx[(size_t)t], c = F.idx[(size_t)t + 1];
                if (a < 0 || b < 0 || c < 0 || a >= nV || b >= nV || c >= nV) continue;
                RTTri tr; tr.v0 = base + a; tr.v1 = base + b; tr.v2 = base + c; tr.mat = mi;
                e.tris.push_back(tr);
            }
        }
    }
    AgregarMallaFin(e, base, nV, trisAntes);
}
// (cierre de AgregarMalla: tangentes si hace falta) -> ver AgregarMallaFin
static void AgregarMallaFin(RTEscena& e, int base, int nV, size_t trisAntes) {
    bool conNM = false;
    for (size_t t = trisAntes; t < e.tris.size() && !conNM; t++) conNM = (e.mats[(size_t)e.tris[t].mat].nmTex >= 0);
    if (conNM) TangentesDe(e, base, base + nV, trisAntes, e.tris.size());
    else if (e.tan.size() < e.pos.size() / 3 * 4) e.tan.resize(e.pos.size() / 3 * 4, 0.0f);
}
static void RecorrerEscena(RTEscena& e, Object* o, std::vector<Material*>& vistos) {
    if (!o) return;
    if (o != (Object*)SceneCollection && (!o->visible || !o->renderizable)) return;
    if (o->getType() == ObjectType::mesh) AgregarMalla(e, (Mesh*)o, vistos);
    for (size_t i = 0; i < o->Childrens.size(); i++) RecorrerEscena(e, o->Childrens[i], vistos);
}
static void AgregarLuces(RTEscena& e) {
    for (size_t i = 0; i < Lights.size(); i++) {
        Light* l = Lights[i]; if (!l || !l->visible) continue;
        RTLuz L; Matrix4 W; l->GetWorldMatrix(W);
        L.pos = V3(W.m[12], W.m[13], W.m[14]);
        L.dir = Norm(V3(W.m[4], W.m[5], W.m[6]));   // el +Y local de la lampara apunta HACIA la luz (como GL_POSITION w=0)
        L.direccional = l->direccional;
        for (int c = 0; c < 3; c++) { L.color[c] = l->diffuse[c]; L.ambiente[c] = l->ambient[c]; }
        L.radio = l->rtRadio;
        L.rayos = (int)(l->rtRayos + 0.5f); if (L.rayos < 1) L.rayos = g_rt.rayos;   // 0 = el global de la pestania Render
        if (L.rayos < 1) L.rayos = 1; if (L.rayos > 256) L.rayos = 256;
        L.attC = l->attConstant; L.attL = l->attLinear; L.attQ = l->attQuadratic;
        e.luces.push_back(L);
    }
}
static void ArmarEscena(RTEscena& e) {
    e.Limpiar();
    std::vector<Material*> vistos;
    RecorrerEscena(e, SceneCollection, vistos);
    AgregarLuces(e);
    e.bg[0] = g_renderBg[0]; e.bg[1] = g_renderBg[1]; e.bg[2] = g_renderBg[2];
    BVHArmar(e);
}

// ---------------------------------------------------------------------------
//  camara (misma cuenta que Viewport3D::ProyectarPunto, al reves)
// ---------------------------------------------------------------------------
struct RTCamara { RTV pos, der, arr, adel; float tanMedio, aspecto, ortoSize; bool orto; };
static void CamaraDe(Viewport3D* vp, int w, int h, RTCamara& c) {
    CameraBase cam = vp->VistaCam();
    const bool camFrame = (vp->ViewFromCameraActive && CameraActive);
    const float fov = camFrame ? CameraActive->fov : fovDeg;
    c.orto = camFrame ? CameraActive->orthographic : vp->orthographic;
    Vector3 d = cam.rot * Vector3(1, 0, 0), u = cam.rot * Vector3(0, 1, 0), f = cam.rot * Vector3(0, 0, -1);
    c.pos = V3(cam.pos.x, cam.pos.y, cam.pos.z); c.der = V3(d.x, d.y, d.z); c.arr = V3(u.x, u.y, u.z); c.adel = V3(f.x, f.y, f.z);
    c.tanMedio = tanf(fov * 0.5f * 3.14159265f / 180.0f);
    c.aspecto = (h > 0) ? (float)w / (float)h : 1.0f;
    c.ortoSize = vp->orbitDistance * c.tanMedio; if (c.ortoSize < 0.001f) c.ortoSize = 0.001f;
}
static inline void RayoPixel(const RTCamara& c, float px, float py, int w, int h, RTV& o, RTV& d) {
    const float ndcX = px / (float)w * 2.0f - 1.0f, ndcY = 1.0f - py / (float)h * 2.0f;
    if (c.orto) { o = c.pos + c.der * (ndcX * c.aspecto * c.ortoSize) + c.arr * (ndcY * c.ortoSize); d = c.adel; }
    else { o = c.pos; d = Norm(c.adel + c.der * (ndcX * c.aspecto * c.tanMedio) + c.arr * (ndcY * c.tanMedio)); }
}

// ---------------------------------------------------------------------------
//  sombreado difuso: color x (ambiente + suma de luces con sombra) + emision
// ---------------------------------------------------------------------------
static inline void Muestrear(const RTTextura& t, float u, float v, float* rgba) {
    u -= floorf(u); v -= floorf(v);
    int x = (int)(u * t.w), y = (int)(v * t.h); if (x >= t.w) x = t.w - 1; if (y >= t.h) y = t.h - 1; if (x < 0) x = 0; if (y < 0) y = 0;
    const unsigned char* p = t.px + ((size_t)y * t.w + x) * 4;
    rgba[0] = p[0] / 255.0f; rgba[1] = p[1] / 255.0f; rgba[2] = p[2] / 255.0f; rgba[3] = p[3] / 255.0f;
}
// el UV interpolado del impacto
static inline void UVEn(const RTEscena& e, const RTHit& h, float& u, float& v) {
    const RTTri& T = e.tris[(size_t)h.tri]; const float b0 = 1.0f - h.b1 - h.b2;
    u = e.uv[(size_t)T.v0*2]*b0 + e.uv[(size_t)T.v1*2]*h.b1 + e.uv[(size_t)T.v2*2]*h.b2;
    v = e.uv[(size_t)T.v0*2+1]*b0 + e.uv[(size_t)T.v1*2+1]*h.b1 + e.uv[(size_t)T.v2*2+1]*h.b2;
}
// cuanto TAPA la superficie en ese impacto: 1 = opaca; transparente = alpha del color x alpha de la textura;
// aditiva = 0 (suma luz, no tapa nada)
static inline float OpacidadEn(const RTEscena& e, const RTHit& h) {
    const RTMaterial& m = e.mats[(size_t)e.tris[(size_t)h.tri].mat];
    if (!m.transparente) return 1.0f;
    if (m.aditivo) return 0.0f;
    float a = m.alfa;
    if (m.tex >= 0 && a > 0.0f) { float u, v, tx[4]; UVEn(e, h, u, v); Muestrear(e.texs[(size_t)m.tex], u, v, tx); a *= tx[3]; }
    return a;
}
// VISIBILIDAD de una luz desde 'o' en direccion 'd' (0 = tapada, 1 = libre): los opacos cortan; los transparentes
// dejan pasar (1 - opacidad) y se sigue atravesando (hasta 8 capas)
static float Visibilidad(const RTEscena& e, const RTV& o, const RTV& d, float tMax) {
    RTHit h;
    if (!e.hayTransparentes) return Trazar(e, o, d, 1e-4f, tMax, true, h) ? 0.0f : 1.0f;
    float vis = 1.0f; float tMin = 1e-4f; int excl = -1;
    for (int capa = 0; capa < 8; capa++) {
        if (!Trazar(e, o, d, tMin, tMax, false, h, excl)) return vis;
        const float op = OpacidadEn(e, h);
        vis *= (1.0f - op);
        if (vis < 0.004f) return 0.0f;
        tMin = h.t * (1.0f - 1e-5f); excl = h.tri;   // seguir el mismo rayo: solo se descarta lo ya atravesado
    }
    return vis;
}
static inline RTV DirAzarEsfera(RTRng& rng) {   // punto uniforme sobre la esfera unitaria
    const float z = 1.0f - 2.0f * rng.F();
    float rr = 1.0f - z * z; if (rr < 0.0f) rr = 0.0f;
    const float r = sqrtf(rr), a = 6.2831853f * rng.F();
    return V3(r * cosf(a), r * sinf(a), z);
}
// luz directa en el impacto (difuso: color x (ambiente + suma de luces visibles) + emision). 'alb' ya trae la textura.
static void LuzEn(const RTEscena& e, const RTV& p, const RTV& n, const RTV& haciaCam, const float* alb, const RTMaterial& m, RTRng& rng, float* out) {
    if (m.sinLuz) { out[0] = alb[0]; out[1] = alb[1]; out[2] = alb[2]; return; }   // "unlit": el color tal cual
    float luz[3] = { 0, 0, 0 };
    float brillo[3] = { 0, 0, 0 };   // el reflejo de las LAMPARAS (no son geometria: el rayo de reflejo no las ve)
    // exponente del brillo desde la rugosidad: mate (1) = ancho y debil; pulido (0.05) = chico e intenso. Un espejo
    // perfecto (rugosidad 0) no tiene brillo de lampara puntual (seria un punto infinitesimal).
    const float rug = (m.rugosidad < 0.05f) ? 0.05f : m.rugosidad;
    const float expo = 2.0f / (rug * rug) - 2.0f + 1.0f;
    const float pesoBrillo = (m.rugosidad <= 0.0f) ? 0.0f : (m.metalico + (1.0f - m.metalico) * 0.04f);
    float colBrillo[3] = { m.metalico * alb[0] + (1.0f - m.metalico), m.metalico * alb[1] + (1.0f - m.metalico), m.metalico * alb[2] + (1.0f - m.metalico) };
    const RTV po = p + n * 1e-3f;   // origen de los rayos de sombra, apenas afuera de la superficie
    for (size_t i = 0; i < e.luces.size(); i++) {
        const RTLuz& L = e.luces[i];
        luz[0] += L.ambiente[0]; luz[1] += L.ambiente[1]; luz[2] += L.ambiente[2];
        float lambert = 0.0f, spec = 0.0f, att = 1.0f;
        for (int r = 0; r < L.rayos; r++) {
            RTV ldir; float dist;
            if (L.direccional) {
                ldir = L.dir;
                if (L.radio > 0.0f) ldir = Norm(L.dir + DirAzarEsfera(rng) * tanf(L.radio * 3.14159265f / 180.0f));   // radio = grados
                dist = 1e30f;
            } else {
                RTV lp = L.pos;
                if (L.radio > 0.0f) lp = lp + DirAzarEsfera(rng) * L.radio;   // un punto de la esfera de la lampara
                const RTV a = lp - po; dist = Largo(a); ldir = (dist > 1e-9f) ? a * (1.0f / dist) : n;
            }
            const float nl = Dot(n, ldir);
            if (nl <= 0.0f) continue;
            const float vis = Visibilidad(e, po, ldir, dist);   // 0..1: lo transparente deja pasar parte de la luz
            if (vis <= 0.0f) continue;
            lambert += nl * vis;
            if (pesoBrillo > 0.0f) {   // Blinn-Phong: (n . h)^expo, normalizado para que el pulido concentre la misma energia
                const RTV hv = Norm(ldir + haciaCam);
                const float nh = Dot(n, hv);
                if (nh > 0.0f) spec += powf(nh, expo) * (expo + 2.0f) * 0.125f * vis;
            }
        }
        if (lambert <= 0.0f && spec <= 0.0f) continue;
        lambert /= (float)L.rayos; spec /= (float)L.rayos;
        if (!L.direccional) { const float dd = Largo(L.pos - po); att = 1.0f / (L.attC + L.attL * dd + L.attQ * dd * dd); if (att > 1.0f) att = 1.0f; }
        for (int c = 0; c < 3; c++) { luz[c] += L.color[c] * lambert * att; brillo[c] += L.color[c] * spec * att; }
    }
    const float difusoPeso = 1.0f - m.metalico;   // el metal no tiene difuso: todo es reflejo
    for (int c = 0; c < 3; c++) out[c] = alb[c] * luz[c] * difusoPeso + colBrillo[c] * brillo[c] * pesoBrillo + m.emision[c];
}
// un rayo de camara: la primera superficie; si es transparente, su color pesa 'alpha' y el resto lo pone lo que hay
// detras (se sigue el mismo rayo, hasta 8 capas); lo aditivo se suma y no tapa
static void Radiancia(const RTEscena& e, const RTV& o0, const RTV& d, RTRng& rng, int profundidad, float* out);
static void Sombrear(const RTEscena& e, const RTV& o0, const RTV& d, RTRng& rng, float* out) { Radiancia(e, o0, d, rng, 0, out); }
static void Radiancia(const RTEscena& e, const RTV& o0, const RTV& d, RTRng& rng, int profundidad, float* out) {
    out[0] = out[1] = out[2] = 0.0f;
    float peso = 1.0f; const RTV& o = o0; float tMin = 1e-4f; int excl = -1;
    for (int capa = 0; capa < 8; capa++) {
        RTHit h;
        if (!Trazar(e, o, d, tMin, 1e30f, false, h, excl)) { out[0] += peso * e.bg[0]; out[1] += peso * e.bg[1]; out[2] += peso * e.bg[2]; return; }
        const RTTri& T = e.tris[(size_t)h.tri];
        const RTMaterial& m = e.mats[(size_t)T.mat];
        const float b0 = 1.0f - h.b1 - h.b2;
        const RTV p = o + d * h.t;
        float alb[3] = { m.difuso[0], m.difuso[1], m.difuso[2] };
        float alfa = m.transparente ? m.alfa : 1.0f;
        if (m.tex >= 0) {
            float u, v, tx[4]; UVEn(e, h, u, v); Muestrear(e.texs[(size_t)m.tex], u, v, tx);
            alb[0] *= tx[0]; alb[1] *= tx[1]; alb[2] *= tx[2];
            if (m.transparente) alfa *= tx[3];
        }
        if (alfa > 0.0f) {
            RTV n = V3(e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2,
                       e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2,
                       e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2);
            n = Norm(n);
            if (Dot(n, d) > 0.0f) n = n * -1.0f;   // la cara de atras se ilumina como si mirara a la camara (dos caras)
            if (m.nmTex >= 0 && e.tan.size() >= e.pos.size() / 3 * 4) {
                // NORMAL MAP: la normal de la textura (tangent-space) llevada al mundo con la base T/B/N del impacto
                float u, v, tx[4]; UVEn(e, h, u, v); Muestrear(e.texs[(size_t)m.nmTex], u, v, tx);
                RTV Tg = V3(e.tan[(size_t)T.v0*4]*b0 + e.tan[(size_t)T.v1*4]*h.b1 + e.tan[(size_t)T.v2*4]*h.b2,
                            e.tan[(size_t)T.v0*4+1]*b0 + e.tan[(size_t)T.v1*4+1]*h.b1 + e.tan[(size_t)T.v2*4+1]*h.b2,
                            e.tan[(size_t)T.v0*4+2]*b0 + e.tan[(size_t)T.v1*4+2]*h.b1 + e.tan[(size_t)T.v2*4+2]*h.b2);
                const float mano = (e.tan[(size_t)T.v0*4+3] < 0.0f) ? -1.0f : 1.0f;
                Tg = Tg - n * Dot(n, Tg);
                if (Dot(Tg, Tg) > 1e-12f) {
                    Tg = Norm(Tg);
                    const RTV B = Cruz(n, Tg) * mano;
                    const float nx = tx[0] * 2.0f - 1.0f, ny = tx[1] * 2.0f - 1.0f, nz = tx[2] * 2.0f - 1.0f;
                    const RTV np = Tg * nx + B * ny + n * nz;
                    if (Dot(np, np) > 1e-12f) n = Norm(np);
                }
            }
            float col[3]; LuzEn(e, p, n, d * -1.0f, alb, m, rng, col);
            // METALICO: reflejo de la escena (hasta 4 rebotes), teñido por el color; la rugosidad desparrama la
            // direccion reflejada (un rayo por muestra: los pases lo promedian)
            if (m.metalico > 0.0f && profundidad < 4) {
                RTV r = d - n * (2.0f * Dot(d, n));
                if (m.rugosidad > 0.0f) {
                    const float k = m.rugosidad * m.rugosidad;
                    r = Norm(r + DirAzarEsfera(rng) * k);
                    if (Dot(r, n) <= 0.0f) r = Norm(r - n * (2.0f * Dot(r, n)));   // no meterse en la superficie
                }
                float refl[3]; Radiancia(e, p + n * 1e-3f, r, rng, profundidad + 1, refl);
                for (int c = 0; c < 3; c++) col[c] += m.metalico * alb[c] * refl[c];
            }
            if (m.aditivo) { for (int c = 0; c < 3; c++) out[c] += peso * col[c]; }   // suma y no tapa
            else           { for (int c = 0; c < 3; c++) out[c] += peso * alfa * col[c]; peso *= (1.0f - alfa); }
        }
        if (!m.transparente || peso < 0.004f) return;
        tMin = h.t * (1.0f - 1e-5f); excl = h.tri;   // seguir el mismo rayo detras de esta superficie
    }
}

// ---------------------------------------------------------------------------
//  el render progresivo: acumulador + tiles
// ---------------------------------------------------------------------------
static const int kTile = 32;
struct RTRender {
    RTEscena esc; RTCamara cam;
    int w, h, tilesX, tilesY;
    std::vector<float> acum;             // 3 por pixel
    std::vector<unsigned char> img;      // RGBA 8 bits (lo que se muestra / guarda)
    int paso;                            // pases completos
    int tileSig;                         // proximo tile del pase en curso
    std::vector<unsigned char> tileSucio; // tiles con pixeles nuevos desde la ultima subida
    RTRender() : w(0), h(0), tilesX(0), tilesY(0), paso(0), tileSig(0) {}
    void Iniciar(Viewport3D* vp, int W, int H) {
        w = W; h = H; tilesX = (w + kTile - 1) / kTile; tilesY = (h + kTile - 1) / kTile;
        acum.assign((size_t)w * h * 3, 0.0f); img.assign((size_t)w * h * 4, 255);
        tileSucio.assign((size_t)tilesX * tilesY, 0);
        paso = 0; tileSig = 0;
        ArmarEscena(esc); CamaraDe(vp, w, h, cam);
    }
    bool Terminado() const { return paso >= g_rt.pases; }
    void Tile(int ti) {   // un tile del pase actual: 'samples' muestras por pixel
        const int tx = (ti % tilesX) * kTile, ty = (ti / tilesX) * kTile;
        const int x1 = (tx + kTile < w) ? tx + kTile : w, y1 = (ty + kTile < h) ? ty + kTile : h;
        const int ns = (g_rt.samples < 1) ? 1 : g_rt.samples;
        const int pasesHechos = paso + 1;
        for (int y = ty; y < y1; y++) for (int x = tx; x < x1; x++) {
            RTRng rng((unsigned int)(x * 1973 + y * 9277 + paso * 26699 + 1));
            float c[3] = { 0, 0, 0 };
            for (int s = 0; s < ns; s++) {
                RTV o, d; RayoPixel(cam, x + rng.F(), y + rng.F(), w, h, o, d);
                float m[3]; Sombrear(esc, o, d, rng, m);
                c[0] += m[0]; c[1] += m[1]; c[2] += m[2];
            }
            float* a = &acum[((size_t)y * w + x) * 3];
            a[0] += c[0] / ns; a[1] += c[1] / ns; a[2] += c[2] / ns;
            unsigned char* p = &img[((size_t)y * w + x) * 4];
            for (int k = 0; k < 3; k++) { float v = a[k] / (float)pasesHechos; if (v < 0) v = 0; if (v > 1) v = 1; p[k] = (unsigned char)(v * 255.0f + 0.5f); }
            p[3] = 255;
        }
        tileSucio[(size_t)ti] = 1;
    }
    // avanza hasta agotar el presupuesto (ms; <= 0 = sin limite) o terminar el pase; true si hizo algo
    bool Avanzar(double presupuestoMs) {
        if (Terminado()) return false;
        const int total = tilesX * tilesY;
        const double t0 = W3dNowMs();
        int hilos = (g_rt.hilos > 0) ? g_rt.hilos : 1;
#ifdef RT_HILOS
        if (g_rt.hilos <= 0) { hilos = (int)std::thread::hardware_concurrency(); if (hilos < 1) hilos = 1; if (hilos > 16) hilos = 16; }
        if (hilos > 1 && total - tileSig > 1) {
            // cada hilo toma el proximo tile del contador (en orden) hasta que se acaben o se pase el tiempo; un
            // tile tomado SIEMPRE se termina -> lo hecho es el prefijo contiguo [tileSig, sig) y no hay huecos
            std::atomic<int> sig(tileSig);
            const int tope = total;
            std::vector<std::thread> hs;
            for (int i = 0; i < hilos; i++) hs.push_back(std::thread([this, &sig, tope, t0, presupuestoMs]() {
                for (;;) {
                    if (presupuestoMs > 0 && W3dNowMs() - t0 > presupuestoMs) break;
                    const int ti = sig.fetch_add(1);
                    if (ti >= tope) break;
                    Tile(ti);
                }
            }));
            for (size_t i = 0; i < hs.size(); i++) hs[i].join();
            int hecho = sig.load(); if (hecho > total) hecho = total;
            tileSig = hecho;
        } else
#endif
        {
            // sin reloj (Symbian: W3dNowMs devuelve 0) el presupuesto no corta -> tope de tiles por llamada, asi la UI
            // del N95 sigue respondiendo (4 tiles de 32x32 por frame)
            const bool sinReloj = (t0 <= 0.0);
            int hechos = 0;
            while (tileSig < total) {
                Tile(tileSig); tileSig++; hechos++;
                if (presupuestoMs > 0 && W3dNowMs() - t0 > presupuestoMs) break;
                if (presupuestoMs > 0 && sinReloj && hechos >= 4) break;
            }
        }
        if (tileSig >= total) { paso++; tileSig = 0; }
        return true;
    }
};

// ---------------------------------------------------------------------------
//  viewport: textura de pantalla + firma para reiniciar cuando cambia la vista o la escena
// ---------------------------------------------------------------------------
static RTRender* g_rtVp = NULL;
static const Viewport3D* g_rtVpDe = NULL;
static unsigned int g_rtTexId = 0; static int g_rtTexW = 0, g_rtTexH = 0;
static double g_rtFirma = 0.0;
static bool g_rtForzar = false;

static int Pot(int v) { int p = 1; while (p < v) p <<= 1; return p; }
static double FirmaVista(Viewport3D* vp) {
    CameraBase cam = vp->VistaCam();
    double f = cam.pos.x * 1.1 + cam.pos.y * 2.3 + cam.pos.z * 3.7 + cam.rot.x * 5.1 + cam.rot.y * 7.3 + cam.rot.z * 11.7 + cam.rot.w * 13.1;
    f += vp->orbitDistance * 0.37 + (vp->orthographic ? 1000.0 : 0.0) + fovDeg * 0.01 + vp->width * 0.001 + vp->height * 0.0013;
    // escena: cuantos objetos y donde estan (posiciones de mundo) + luces + version de las mallas
    struct S { static void Rec(Object* o, double& f) { if (!o) return; if (o->getType() == ObjectType::mesh) { Mesh* m = (Mesh*)o; f += m->geoVersion * 0.019 + (m->genValido ? 0.5 : 0.0); }
                 Matrix4 W; o->GetWorldMatrix(W); f += W.m[12] * 0.31 + W.m[13] * 0.53 + W.m[14] * 0.71 + W.m[0] * 0.13 + W.m[5] * 0.17 + (o->visible ? 0.7 : 0.0);
                 for (size_t i = 0; i < o->Childrens.size(); i++) Rec(o->Childrens[i], f); } };
    S::Rec(SceneCollection, f);
    for (size_t i = 0; i < Lights.size(); i++) if (Lights[i]) { Light* l = Lights[i]; f += l->rtRadio * 3.3 + l->rtRayos * 0.7 + l->diffuse[0] + l->diffuse[1] * 2 + l->diffuse[2] * 3 + (l->direccional ? 9.0 : 0.0); }
    f += g_rt.samples * 0.101 + g_rt.rayos * 0.047 + g_renderBg[0] + g_renderBg[1] * 2 + g_renderBg[2] * 3;
    for (size_t i = 0; i < Materials.size(); i++) if (Materials[i]) { Material* mt = Materials[i];
        f += mt->diffuse[0] * 0.21 + mt->diffuse[1] * 0.23 + mt->diffuse[2] * 0.29 + mt->diffuse[3] * 0.37 + (mt->transparent ? 1.7 : 0.0) + mt->mezcla * 0.13 + mt->rtRugosidad * 0.41 + mt->rtMetalico * 0.43 + mt->emission[0] + mt->emission[1] + mt->emission[2] + (mt->lighting ? 0.3 : 0.0) + (mt->textureOn ? 0.9 : 0.0) + (mt->texture ? mt->texture->iID * 0.011 : 0.0) + (mt->normalMap ? 2.3 : 0.0) + (mt->normalTexture ? mt->normalTexture->iID * 0.017 : 0.0); }
    return f;
}
void RTInvalidar() { g_rtForzar = true; }
bool RTActivoEn(const Viewport3D* vp) { return g_rt.on && g_rtVp && g_rtVpDe == vp && !g_rtVp->Terminado(); }

static void SubirTiles(RTRender* r) {
    if (!g_rtTexId) return;
    for (int ti = 0; ti < r->tilesX * r->tilesY; ti++) {
        if (!r->tileSucio[(size_t)ti]) continue;
        r->tileSucio[(size_t)ti] = 0;
        const int tx = (ti % r->tilesX) * kTile, ty = (ti / r->tilesX) * kTile;
        const int tw = (tx + kTile < r->w) ? kTile : r->w - tx, th = (ty + kTile < r->h) ? kTile : r->h - ty;
        std::vector<unsigned char> tmp((size_t)tw * th * 4);
        for (int y = 0; y < th; y++) memcpy(&tmp[(size_t)y * tw * 4], &r->img[((size_t)(ty + y) * r->w + tx) * 4], (size_t)tw * 4);
        w3dEngine::UpdateRGBA(g_rtTexId, tx, ty, tw, th, &tmp[0]);
    }
}
void RTViewportPaso(Viewport3D* vp) {
    if (!g_rt.on || !vp || vp->width <= 0 || vp->height <= 0) return;
    const double firma = FirmaVista(vp);
    if (!g_rtVp || g_rtVpDe != vp || g_rtVp->w != vp->width || g_rtVp->h != vp->height || firma != g_rtFirma || g_rtForzar) {
        if (!g_rtVp) g_rtVp = new RTRender();
        g_rtVp->Iniciar(vp, vp->width, vp->height);
        g_rtVpDe = vp; g_rtFirma = firma; g_rtForzar = false;
        // textura de pantalla, potencia de 2 (GLES 1.1 del N95 no tiene NPOT), sin mips
        const int tw = Pot(vp->width), th = Pot(vp->height);
        if (!g_rtTexId || tw != g_rtTexW || th != g_rtTexH) {
            if (g_rtTexId) w3dEngine::DeleteTexture(g_rtTexId);
            std::vector<unsigned char> vacio((size_t)tw * th * 4, 0);
            g_rtTexId = w3dEngine::UploadRGBA(&vacio[0], tw, th, false, false);
            g_rtTexW = tw; g_rtTexH = th;
        }
    }
    // avanza con un presupuesto por frame: la UI sigue viva y la imagen se completa de a tiles
    if (!g_rtVp->Terminado()) { g_rtVp->Avanzar(40.0); g_redraw = true; }
    SubirTiles(g_rtVp);
    // el blit: un quad del tamano del viewport, encima de la escena GL (sin z: los overlays 3D siguen usando el z de la escena)
    namespace gfx = w3dEngine;
    gfx::MatrixMode(gfx::Projection); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Ortho(0, vp->width, vp->height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Fog); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Blend); gfx::Enable(gfx::Texture2D);
    gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::Color4f(1, 1, 1, 1);
    gfx::BindTexture(g_rtTexId);
    const float su = (float)vp->width / (float)g_rtTexW, sv = (float)vp->height / (float)g_rtTexH;
    const GLshort v[8] = { 0, 0, (GLshort)vp->width, 0, 0, (GLshort)vp->height, (GLshort)vp->width, (GLshort)vp->height };
    const GLfloat uv[8] = { 0, 0, su, 0, 0, sv, su, sv };
    W3dDrawStrip4(v, uv);
    gfx::BindTexture(0);
    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
    gfx::Enable(gfx::DepthTest);
}
void RTRenderEstado(Viewport3D* vp) {
    if (!g_rt.on || !g_rtVp || g_rtVpDe != vp) return;
    char b[96];
    if (g_rtVp->Terminado()) sprintf(b, "%s: %s (%d)", T("Ray Tracing"), T("done"), g_rtVp->paso);
    else sprintf(b, "%s: %d/%d", T("Ray Tracing"), g_rtVp->paso, g_rt.pases);
    w3dEngine::PushMatrix();
    // una linea debajo del borde superior (la fila 0 del viewport queda tapada por el borde/menu)
    w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)(vp->BarTopOffset() + gapGS * 2 + LetterHeightGS), 0);
    SetColorID(ColorID::blanco, 1.0f);
    RenderBitmapText(b, textAlign::left, vp->width - gapGS * 4);
    w3dEngine::PopMatrix();
}

// ---------------------------------------------------------------------------
//  render a archivo y harness (buffer propio, sincronico)
// ---------------------------------------------------------------------------
static RTRender* g_rtOff = NULL;
static bool RenderCompleto(Viewport3D* vp, int w, int h, int pases, bool progreso) {
    if (!vp || w <= 0 || h <= 0) return false;
    if (!g_rtOff) g_rtOff = new RTRender();
    const int pasesPrev = g_rt.pases; g_rt.pases = (pases > 0) ? pases : g_rt.pases;
    g_rtOff->Iniciar(vp, w, h);
    while (!g_rtOff->Terminado()) {
        g_rtOff->Avanzar(0.0);
        if (progreso) ProgresoActualizar((float)g_rtOff->paso / (float)g_rt.pases);
    }
    g_rt.pases = pasesPrev;
    return true;
}
bool RTRenderizarAPNG(Viewport3D* vp, int w, int h, const std::string& ruta, std::string& msg) {
    if (!RenderCompleto(vp, w, h, 0, true)) { msg = "Ray Tracing: nothing to render"; return false; }
    if (!w3dEngine::SavePNG(ruta.c_str(), &g_rtOff->img[0], w, h, false)) { msg = "Ray Tracing: could not save"; return false; }
    msg.clear();
    return true;
}
bool RTHarnessRender(Viewport3D* vp, int w, int h, int pases) { return RenderCompleto(vp, w, h, pases, false); }
bool RTHarnessPixel(int x, int y, unsigned char* rgb) {
    if (!g_rtOff || x < 0 || y < 0 || x >= g_rtOff->w || y >= g_rtOff->h) return false;
    const unsigned char* p = &g_rtOff->img[((size_t)y * g_rtOff->w + x) * 4];
    rgb[0] = p[0]; rgb[1] = p[1]; rgb[2] = p[2];
    return true;
}
bool RTHarnessGuardar(const std::string& ruta) {
    if (!g_rtOff) return false;
    return w3dEngine::SavePNG(ruta.c_str(), &g_rtOff->img[0], g_rtOff->w, g_rtOff->h, false);
}
int RTHarnessTriangulos() { return g_rtOff ? (int)g_rtOff->esc.tris.size() : 0; }
