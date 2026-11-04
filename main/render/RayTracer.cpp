// ============================================================================
//  RayTracer.cpp - trazado de rayos por CPU (ver RayTracer.h). Todo escrito de cero para Whisk3D.
//
//  Estructura (pensada para portar el nucleo a CUDA/RTX):
//    RTEscena  = arrays planos: posiciones/normales/uv de mundo, triangulos (3 indices + material),
//                materiales (color difuso, emision, textura), texturas (pixeles CPU), luces, y un BVH
//                (cajas alineadas, particion por la mediana del eje mas largo, recorrido iterativo).
//    RTCamara  = la LENTE del viewport (Viewport3D::LenteActual: la misma cuenta que ProyectarPunto y que la proyeccion
//                de GL, con el marco de la camara y el zoom / pan de inspeccion), al reves: de un pixel a un rayo.
//    RTRender  = acumulador float RGB + imagen de 8 bits + tiles; Avanzar(presupuesto) hace tiles hasta
//                agotar el tiempo (o todos los del pase); con hilos, cada uno toma tiles de un contador.
// ============================================================================
#include "render/RayTracer.h"
#include "render/Denoiser.h"       // limpiar el ruido al terminar cada pase (opcional: OIDN / OptiX)
#include "render/RTGpu.h"          // el mismo trazado en la GPU (Vulkan, opcional)
#include "render/ReescaladoFSR.h"   // el reescalado del viewport con FSR 1 (g_rt.reescalador)
#include "ui/ViewPorts/ViewPort3D.h"
#include "ui/ViewPorts/LayoutArbol.h"   // aviso de destruccion de viewports (el del preview)
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Light.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Collection.h"      // SceneCollection
#include "objects/Camera.h"          // CameraActive (mirar por la camara)
#include "animation/SkeletalAnimation.h"   // SkinearMesh: la pose de cada instante del obturador
#include "animation/VertexAnimation.h"     // W3dPoseAlDia
#include "objects/Armature.h"              // lastPoseFrame: la pose de otro instante no se cachea
#include "WhiskUI/theme/colores.h"   // SetColorID
#include "io/TexturaEditada.h"      // los pixeles CPU de una textura (para muestrearla)
#include "render/OpcionesRender.h"  // g_renderBg
#include "app/variables.h"          // fovDeg, SceneCollection
#include "config/W3dProfile.h"      // W3dNowMs
#include "W3dLang.h"
#include "w3dGraphics.h"
#include "w3dTexture.h"             // UploadRGBA / UpdateRGBA / SavePNG
#include "w3dlog.h"
#include "WhiskUI/draw/glesdraw.h"  // W3dDrawStrip4
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/core/UI.h"        // GlobalScale, gapGS
#include "ui/ViewPorts/PopUp/ProgressPopup.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>   // getenv (W3D_RT_PERFIL)
#include <vector>
#include <set>      // la vista local (g_localViewVisibles)
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
struct RTMaterial { float difuso[3]; float emision[3]; int tex; int nmTex; bool sinLuz; bool transparente; bool aditivo; float alfa; float rugosidad; float metalico;
                   int emTex, rugTex, metTex, alfaTex;   // los MAPAS del material (texturas; -1 = el numero)
                   float f0;                             // reflejo de frente del dielectrico (del IOR: ((n-1)/(n+1))^2)
                   float recorte;                        // > 0: RECORTADO por alfa (como GL: el texel con alfa menor no esta)
                   float ior;                            // > 1: lo transparente REFRACTA (1.33 agua, 1.5 vidrio)
                   float trans; };                       // TRANSMISION 0..1 (Material::transmision): lo que es vidrio
// EL BORDE DEL RECORTADO: en vez de todo o nada en el umbral, una rampa corta alrededor (con el filtro bilineal el
// borde queda suave y los pases lo promedian: sin escalones de texel ni el serruchado del pelo y las hojas)
static inline float Recortar(float a, float umbral) { a = (a - umbral) * 8.0f + 0.5f; return a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
struct RTTextura { const unsigned char* px; int w, h; bool filtrado; };   // filtrado: bilineal (como la ve GL), si no el texel
// spotCos: el coseno del corte del cono (-1 = sin cono, luz puntual); spotDir: hacia donde alumbra; spotExpo: lo
// concentrado del haz (como GL: (cos)^expo adentro del cono)
struct RTLuz { RTV pos; RTV dir; bool direccional; float color[3]; float ambiente[3]; float radio; int rayos; float attC, attL, attQ;
               float spotCos, spotExpo; RTV spotDir; float distancia; };   // distancia: 0 = sin limite (Light::distancia)
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
    // LAS MALLAS (sin modificadores): donde quedaron sus vertices y con que matriz / pose. Cuando solo se MUEVEN (la
    // pose de un esqueleto, o la posicion: el publico que salta) se renuevan sus vertices y se reajustan las cajas del
    // BVH (ActualizarPosesYLuces) en vez de armar todo de nuevo
    struct Viva { Mesh* m; int base, nV; float W[16]; unsigned pose; };
    std::vector<Viva> vivas;
    void Limpiar() { pos.clear(); nrm.clear(); uv.clear(); tan.clear(); tris.clear(); mats.clear(); texs.clear(); luces.clear(); nodos.clear(); orden.clear(); vivas.clear(); hayTransparentes = false; }
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
// REAJUSTAR el BVH (los vertices se movieron, los triangulos son los mismos): cada caja de nuevo, de las hojas para
// arriba (los hijos siempre estan despues que su padre: BVHConstruir agrega el padre antes de recursar). Mucho mas
// rapido que construir; con la banda moviendose en su lugar las cajas casi no crecen
static void BVHAjustar(RTEscena& e) {
    for (int i = (int)e.nodos.size() - 1; i >= 0; i--) {
        RTNodo& n = e.nodos[(size_t)i];
        for (int a = 0; a < 3; a++) { n.bmin[a] = 1e30f; n.bmax[a] = -1e30f; }
        if (n.cantidad > 0) {
            for (int k = 0; k < n.cantidad; k++) {
                float bmin[3], bmax[3]; BVHCaja(e, e.orden[(size_t)(n.primero + k)], bmin, bmax);
                for (int a = 0; a < 3; a++) { if (bmin[a] < n.bmin[a]) n.bmin[a] = bmin[a]; if (bmax[a] > n.bmax[a]) n.bmax[a] = bmax[a]; }
            }
        } else {
            const int hs[2] = { n.izq, n.der };
            for (int k = 0; k < 2; k++) {
                if (hs[k] < 0) continue;
                const RTNodo& h = e.nodos[(size_t)hs[k]];
                for (int a = 0; a < 3; a++) { if (h.bmin[a] < n.bmin[a]) n.bmin[a] = h.bmin[a]; if (h.bmax[a] > n.bmax[a]) n.bmax[a] = h.bmax[a]; }
            }
        }
    }
}
// las POSES de ahora de las mallas con esqueleto: sus vertices (y normales) de mundo, en el mismo lugar del arreglo
static void ActualizarVivas(RTEscena& e) {
    for (size_t k = 0; k < e.vivas.size(); k++) {
        RTEscena::Viva& v = e.vivas[k];
        Mesh* m = v.m;
        const bool skin = m->skinArmature && m->skinVertex;
        const GLfloat* P = skin ? m->skinVertex : m->vertex;
        if (!P || m->vertexSize != v.nV) continue;
        Matrix4 W; m->GetWorldMatrix(W);
        const unsigned pose = skin ? m->skinPoseSerial : 0u;
        if (pose == v.pose && memcmp(W.m, v.W, sizeof(v.W)) == 0) continue;   // (esta quieta: nada que hacer)
        memcpy(v.W, W.m, sizeof(v.W)); v.pose = pose;
        const GLbyte* N = (skin && m->skinNormals) ? m->skinNormals : m->normals;
        for (int i = 0; i < v.nV; i++) {
            const Vector3 wp = W * Vector3(P[i*3], P[i*3+1], P[i*3+2]);
            float* d = &e.pos[(size_t)(v.base + i) * 3]; d[0] = wp.x; d[1] = wp.y; d[2] = wp.z;
            if (!N) continue;
            const float lx = N[i*3] / 127.0f, ly = N[i*3+1] / 127.0f, lz = N[i*3+2] / 127.0f;
            float nx = W.m[0]*lx + W.m[4]*ly + W.m[8]*lz, ny = W.m[1]*lx + W.m[5]*ly + W.m[9]*lz, nz = W.m[2]*lx + W.m[6]*ly + W.m[10]*lz;
            const float l = sqrtf(nx*nx + ny*ny + nz*nz); if (l > 1e-9f) { nx /= l; ny /= l; nz /= l; }
            float* q = &e.nrm[(size_t)(v.base + i) * 3]; q[0] = nx; q[1] = ny; q[2] = nz;
        }
    }
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
    rm.emTex = rm.rugTex = rm.metTex = rm.alfaTex = -1;
    // el reflejo de frente (f0) sale del IOR; con IOR 1 (sin refraccion) el de un dielectrico comun (0.04)
    { const float n = mt ? (mt->ior < 1.0f ? 1.0f : mt->ior) : 1.0f; const float q = (n - 1.0f) / (n + 1.0f); rm.f0 = (n > 1.0001f) ? q * q : 0.04f; rm.ior = n; }
    // los MAPAS en modo textura (emision, rugosidad, metalico, alfa): sus pixeles CPU, como la base
    if (mt) {
        int* dst[W3D_MAPAS] = { 0, 0, &rm.emTex, &rm.rugTex, &rm.metTex, &rm.alfaTex };
        for (int k = W3D_MAPA_EMISION; k < W3D_MAPAS; k++) {
            if (!mt->mapaConTextura[k] || !mt->mapaTex[k] || !mt->mapaTex[k]->iID) continue;
            TexturaEditable* tm = TexEditObtener(mt->mapaTex[k]);
            if (tm && tm->w > 0 && tm->h > 0) { RTTextura tx; tx.px = &tm->rgba[0]; tx.w = tm->w; tx.h = tm->h; tx.filtrado = mt->filtrado; e.texs.push_back(tx); *dst[k] = (int)e.texs.size() - 1; }
        }
        if (rm.alfaTex >= 0) e.hayTransparentes = true;
    }
    rm.sinLuz = mt ? !mt->lighting : false;
    if (mt && mt->normalMap && mt->normalTexture) {
        TexturaEditable* tn = TexEditObtener(mt->normalTexture);
        if (tn && tn->w > 0 && tn->h > 0) { RTTextura tx; tx.px = &tn->rgba[0]; tx.w = tn->w; tx.h = tn->h; tx.filtrado = mt->filtrado; e.texs.push_back(tx); rm.nmTex = (int)e.texs.size() - 1; }
    }
    rm.rugosidad = mt ? mt->rtRugosidad : 0.5f; if (rm.rugosidad < 0.0f) rm.rugosidad = 0.0f; if (rm.rugosidad > 1.0f) rm.rugosidad = 1.0f;
    rm.metalico = mt ? mt->rtMetalico : 0.0f; if (rm.metalico < 0.0f) rm.metalico = 0.0f; if (rm.metalico > 1.0f) rm.metalico = 1.0f;
    rm.transparente = mt ? mt->transparent : false;
    rm.aditivo = rm.transparente && mt && mt->mezcla == 2;
    rm.alfa = mt ? mt->diffuse[3] : 1.0f; if (rm.alfa < 0.0f) rm.alfa = 0.0f; if (rm.alfa > 1.0f) rm.alfa = 1.0f;
    // RECORTADO por alfa (las hojas, las rejas, el publico): lo transparente de su textura deja pasar el rayo
    rm.recorte = 0.0f;
    // (tambien con mezcla: el pelo va mezclado Y recortado; sin el recorte lo casi transparente de cada mechon contaba)
    if (mt && mt->alphaTest > 0.0f && !rm.aditivo) { if (!rm.transparente) { rm.transparente = true; rm.alfa = 1.0f; } rm.recorte = mt->alphaTest; }
    // TRANSMISION: la parte que es vidrio deja pasar la luz doblada por el IOR (las sombras tambien pasan)
    rm.trans = (mt && !rm.aditivo) ? mt->transmision : 0.0f; if (rm.trans < 0.0f) rm.trans = 0.0f; if (rm.trans > 1.0f) rm.trans = 1.0f;
    if (rm.transparente || rm.trans > 0.0f) e.hayTransparentes = true;
    if (mt && mt->textureOn && mt->texture && mt->texture->iID) {
        TexturaEditable* te = TexEditObtener(mt->texture);   // los pixeles CPU (se decodifican una vez)
        if (te && te->w > 0 && te->h > 0) { RTTextura tx; tx.px = &te->rgba[0]; tx.w = te->w; tx.h = te->h; tx.filtrado = mt->filtrado; e.texs.push_back(tx); rm.tex = (int)e.texs.size() - 1; }
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
    // la POSE del cuadro actual (cacheada: si GL ya la dibujo no cuesta nada). Sin esto, el render a archivo despues de
    // cambiar de cuadro sin que GL dibujara tomaba los vertices skineados de OTRO cuadro
    if (m->skinArmature) SkinearMesh(m); else W3dPoseAlDia(m);
    Matrix4 W; m->GetWorldMatrix(W);
    const size_t trisAntes = e.tris.size();
    const bool gen = m->genValido && m->genVertex && m->genFaces && m->genVertexSize > 0 && m->genFacesSize > 0;
    const GLfloat* P = gen ? m->genVertex : ((m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex);
    const bool skin = !gen && m->skinArmature && m->skinVertex;
    const GLbyte*  N = gen ? m->genNormals : ((skin && m->skinNormals) ? m->skinNormals : m->normals);   // (deformadas si las hay)
    const GLfloat* UV = gen ? m->genUV : m->uv;
    const int nV = gen ? m->genVertexSize : m->vertexSize;
    if (!P || nV <= 0) return;
    const int base = (int)(e.pos.size() / 3);
    if (!gen) { RTEscena::Viva v; v.m = m; v.base = base; v.nV = nV; memcpy(v.W, W.m, sizeof(v.W)); v.pose = skin ? m->skinPoseSerial : 0u; e.vivas.push_back(v); }
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
        // el rango de cada mesh part es el de DIBUJO (startDrawn/indicesDrawnCount, en indices), igual que abajo:
        // start/count nadie los llena y con un modificador (Subdivision) la malla no aportaba ni un triangulo.
        // El material, VIVO de materialsGroup (genMaterialsGroup es una copia del momento de generar, como en Mesh.cpp)
        const std::vector<MaterialGroup>& G = m->genMaterialsGroup;
        for (size_t g = 0; g < G.size(); g++) {
            Material* mtG = (g < m->materialsGroup.size()) ? m->materialsGroup[g].material : G[g].material;
            const int mi = MaterialIdx(e, mtG, vistos);
            const int t0 = G[g].startDrawn / 3, tn = G[g].indicesDrawnCount / 3;
            for (int t = t0; t < t0 + tn && (t + 1) * 3 <= m->genFacesSize; t++) {
                RTTri tr; tr.v0 = base + m->genFaces[t*3]; tr.v1 = base + m->genFaces[t*3+1]; tr.v2 = base + m->genFaces[t*3+2]; tr.mat = mi;
                e.tris.push_back(tr);
            }
        }
    } else if (m->faces3d.empty() && m->faces && m->facesSize >= 3) {
        // sin caras logicas (malla cargada solo con su index buffer, o la de un RECURSO compartido
        // con la edicion pendiente): triangulos por rango de mesh part. El rango que llena la carga
        // es el de DIBUJO (startDrawn/indicesDrawnCount, en indices); start/count nadie los llena.
        const std::vector<MaterialGroup>& G = m->materialsGroup;
        for (size_t g = 0; g < G.size(); g++) {
            const int mi = MaterialIdx(e, G[g].material, vistos);
            const int t0 = G[g].startDrawn / 3, tn = G[g].indicesDrawnCount / 3;
            for (int t = t0; t < t0 + tn && (t + 1) * 3 <= m->facesSize; t++) {
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
extern bool g_localViewActivo;                          // VISTA LOCAL ("/"): solo se traza lo aislado (Objects.cpp)
extern const std::set<Object*>* g_localViewVisibles;
static void RecorrerEscena(RTEscena& e, Object* o, std::vector<Material*>& vistos) {
    if (!o) return;
    if (o != (Object*)SceneCollection && (!o->visible || !o->renderizable)) return;
    if (o != (Object*)SceneCollection && W3dVistaLocalOculta(o)) return;
    if (o->getType() == ObjectType::mesh && (o == (Object*)SceneCollection || !W3dVistaLocalSoloPaso(o))) AgregarMalla(e, (Mesh*)o, vistos);
    for (size_t i = 0; i < o->Childrens.size(); i++) RecorrerEscena(e, o->Childrens[i], vistos);
}
static void AgregarLuces(RTEscena& e) {
    for (size_t i = 0; i < Lights.size(); i++) {
        Light* l = Lights[i]; if (!l || !l->visible) continue;
        RTLuz L; Matrix4 W; l->GetWorldMatrix(W);
        L.pos = V3(W.m[12], W.m[13], W.m[14]);
        L.dir = Norm(V3(W.m[4], W.m[5], W.m[6]));   // el +Y local de la lampara apunta HACIA la luz (como GL_POSITION w=0)
        L.direccional = l->direccional;
        for (int c = 0; c < 3; c++) { L.color[c] = l->diffuse[c] * (l->intensidad > 0.0f ? l->intensidad : 0.0f); L.ambiente[c] = l->ambient[c]; }
        L.radio = l->rtRadio;
        L.rayos = (int)(l->rtRayos + 0.5f); if (L.rayos < 1) L.rayos = g_rt.rayos;   // 0 = el global de la pestania Render
        if (L.rayos < 1) L.rayos = 1; if (L.rayos > 256) L.rayos = 256;
        L.attC = l->attConstant; L.attL = l->attLinear; L.attQ = l->attQuadratic;
        L.distancia = (l->direccional || l->distancia <= 0.0f) ? 0.0f : l->distancia;
        // FOCO (spotCutoff < 180): alumbra hacia su -Z local, como el de GL
        L.spotCos = (!l->direccional && l->spotCutoff < 179.9f) ? cosf(l->spotCutoff * 3.14159265f / 180.0f) : -1.0f;
        L.spotExpo = l->spotExponent;
        L.spotDir = Norm(V3(-W.m[8], -W.m[9], -W.m[10]));
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
// ndc = (e.xy / (sx, sy)) [/ ez en perspectiva] * zoom + pan  (Viewport3D::Lente)
// lente / foco: la PROFUNDIDAD DE CAMPO de la camara (Camera::dof): el radio de la lente (m, 0 = agujerito: todo nitido) y
// la distancia enfocada (m)
struct RTCamara { RTV pos, der, arr, adel; float sx, sy, zoom, panX, panY; bool orto; float lente, foco; };
// 'vista' = lo que se ve en el viewport (su lente tal cual: el marco de la camara, el zoom y el pan); si no, el render
// a archivo: la imagen entera de w x h con la camara (o la vista), sin inspeccion
static void CamaraDe(Viewport3D* vp, int w, int h, RTCamara& c, bool vista) {
    const Viewport3D::Lente L = vp->LenteActual();
    c.pos = V3(L.pos.x, L.pos.y, L.pos.z); c.der = V3(L.der.x, L.der.y, L.der.z);
    c.arr = V3(L.arr.x, L.arr.y, L.arr.z); c.adel = V3(L.fwd.x, L.fwd.y, L.fwd.z);
    c.orto = L.orto;
    const bool camFrame = (vp->ViewFromCameraActive && CameraActive);
    // la PROFUNDIDAD DE CAMPO: solo mirando por la camara y con la suya prendida (la vista libre es un agujerito)
    c.lente = (camFrame && CameraActive->dof && !c.orto) ? CameraActive->RadioLente() : 0.0f;
    c.foco = (c.lente > 0.0f) ? CameraActive->DistanciaFoco() : 0.0f;
    if (vista) { c.sx = L.sx; c.sy = L.sy; c.zoom = L.zoom; c.panX = L.panX; c.panY = L.panY; return; }
    float t = tanf((camFrame ? CameraActive->fov : fovDeg) * 0.5f * 3.14159265f / 180.0f);
    if (c.orto) { t *= vp->orbitDistance; if (t < 0.001f) t = 0.001f; }
    c.sy = t; c.sx = t * ((h > 0) ? (float)w / (float)h : 1.0f);
    c.zoom = 1.0f; c.panX = c.panY = 0.0f;
}
// LA LENTE: el rayo sale de un punto al azar del disco de la lente hacia el MISMO punto del plano enfocado (lo que esta a
// esa distancia queda nitido; lo de mas cerca y mas lejos, desparramado en un disco que crece con la distancia al foco)
static inline void RayoLente(const RTCamara& c, RTRng& rng, RTV& o, RTV& d) {
    if (c.lente <= 0.0f || c.orto) return;
    const float dd = Dot(d, c.adel);
    if (dd <= 1e-4f) return;
    const RTV P = o + d * (c.foco / dd);
    const float r = sqrtf(rng.F()) * c.lente, a = rng.F() * 6.2831853f;
    o = o + c.der * (r * cosf(a)) + c.arr * (r * sinf(a));
    d = Norm(P - o);
}
static inline void RayoPixel(const RTCamara& c, float px, float py, int w, int h, RTV& o, RTV& d) {
    const float ndcX = px / (float)w * 2.0f - 1.0f, ndcY = 1.0f - py / (float)h * 2.0f;
    const float ex = (ndcX - c.panX) / c.zoom * c.sx, ey = (ndcY - c.panY) / c.zoom * c.sy;
    if (c.orto) { o = c.pos + c.der * ex + c.arr * ey; d = c.adel; }
    else { o = c.pos; d = Norm(c.adel + c.der * ex + c.arr * ey); }
}

// ---------------------------------------------------------------------------
//  sombreado difuso: color x (ambiente + suma de luces con sombra) + emision
// ---------------------------------------------------------------------------
static inline void Muestrear(const RTTextura& t, float u, float v, float* rgba) {
    u -= floorf(u); v -= floorf(v);
    if (t.filtrado) {   // BILINEAL entre los 4 texeles de alrededor (repetido en los bordes)
        const float fx = u * t.w - 0.5f, fy = v * t.h - 0.5f;
        const float x0f = floorf(fx), y0f = floorf(fy), ax = fx - x0f, ay = fy - y0f;
        int x0 = (int)x0f % t.w, y0 = (int)y0f % t.h; if (x0 < 0) x0 += t.w; if (y0 < 0) y0 += t.h;
        const int x1 = (x0 + 1) % t.w, y1 = (y0 + 1) % t.h;
        const unsigned char* a = t.px + ((size_t)y0 * t.w + x0) * 4, * b = t.px + ((size_t)y0 * t.w + x1) * 4;
        const unsigned char* c = t.px + ((size_t)y1 * t.w + x0) * 4, * d = t.px + ((size_t)y1 * t.w + x1) * 4;
        for (int k = 0; k < 4; k++)
            rgba[k] = ((a[k] * (1.0f - ax) + b[k] * ax) * (1.0f - ay) + (c[k] * (1.0f - ax) + d[k] * ax) * ay) / 255.0f;
        return;
    }
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
// el material EN ESE IMPACTO: los mapas en modo textura reemplazan a sus numeros (la emision, la rugosidad, el metalico
// y el alfa leidos de su textura en el UV del impacto)
static inline RTMaterial MaterialEn(const RTEscena& e, const RTHit& h, const RTMaterial& m0) {
    RTMaterial m = m0;
    if (m.emTex < 0 && m.rugTex < 0 && m.metTex < 0 && m.alfaTex < 0) return m;
    float u, v, tx[4]; UVEn(e, h, u, v);
    if (m.emTex >= 0) { Muestrear(e.texs[(size_t)m.emTex], u, v, tx); m.emision[0] = tx[0]; m.emision[1] = tx[1]; m.emision[2] = tx[2]; }
    if (m.rugTex >= 0) { Muestrear(e.texs[(size_t)m.rugTex], u, v, tx); m.rugosidad = tx[0]; }
    if (m.metTex >= 0) { Muestrear(e.texs[(size_t)m.metTex], u, v, tx); m.metalico = tx[0]; }
    if (m.alfaTex >= 0) { Muestrear(e.texs[(size_t)m.alfaTex], u, v, tx); m.alfa = (tx[3] < 0.999f) ? tx[3] : tx[0]; m.transparente = true; }
    return m;
}
// cuanto TAPA la superficie en ese impacto: 1 = opaca; transparente = alpha del color x alpha de la textura;
// aditiva = 0 (suma luz, no tapa nada)
static inline float OpacidadEn(const RTEscena& e, const RTHit& h) {
    const RTMaterial m = MaterialEn(e, h, e.mats[(size_t)e.tris[(size_t)h.tri].mat]);
    if (!m.transparente && m.trans <= 0.0f) return 1.0f;
    if (m.aditivo) return 0.0f;
    float a = m.transparente ? m.alfa : 1.0f;
    if (m.transparente && m.tex >= 0 && a > 0.0f) { float u, v, tx[4]; UVEn(e, h, u, v); Muestrear(e.texs[(size_t)m.tex], u, v, tx); a *= tx[3]; }
    if (m.recorte > 0.0f) a = Recortar(a, m.recorte);
    return a * (1.0f - m.trans);   // (el vidrio deja pasar la luz)
}
// VISIBILIDAD de una luz desde 'o' en direccion 'd' (0 = tapada, 1 = libre): los opacos cortan; los transparentes
// dejan pasar (1 - opacidad) y se sigue atravesando (hasta 8 capas)
// la DISTANCIA de una luz: se apaga suave hasta 0 justo ahi, (1 - (d/D)^4)^2 (sin escalon en el borde). D <= 0 = sin limite
static inline float VentanaDistancia(float d, float D) {
    if (D <= 0.0f) return 1.0f;
    const float x = d / D; if (x >= 1.0f) return 0.0f;
    const float x2 = x * x, w = 1.0f - x2 * x2; return w * w;
}
static float Visibilidad(const RTEscena& e, const RTV& o, const RTV& d, float tMax) {
    RTHit h;
    if (!e.hayTransparentes) return Trazar(e, o, d, 1e-4f, tMax, true, h) ? 0.0f : 1.0f;
    float vis = 1.0f; float tMin = 1e-4f; int excl = -1;
    // (lo que en ese punto es TOTALMENTE transparente no gasta capa: el pelo son muchas tarjetas superpuestas y el rayo
    // se quedaba sin capas en los huecos, lo de atras salia negro. 'vuelta' es el tope por si nada tapa nunca)
    for (int capa = 0, vuelta = 0; capa < g_rt.capas && vuelta < 256; vuelta++) {
        if (!Trazar(e, o, d, tMin, tMax, false, h, excl)) return vis;
        const float op = OpacidadEn(e, h);
        if (op > 0.0f) capa++;
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
// en la VISTA PREVIA (escalera de resolucion) cada luz tira UN solo rayo de sombra: las penumbras
// (rtRayos > 1) recien se muestrean en los pases a resolucion completa. En el N95 esto mantiene el
// primer pantallazo en ~1 rayo por bloque aunque haya lamparas de area.
static bool g_rtPreviewRapida = false;
// luz directa en el impacto (difuso: color x (ambiente + suma de luces visibles) + emision). 'alb' ya trae la textura.
static void LuzEn(const RTEscena& e, const RTV& p, const RTV& n, const RTV& haciaCam, const float* alb, const RTMaterial& m, RTRng& rng, float* out) {
    if (m.sinLuz) { out[0] = alb[0]; out[1] = alb[1]; out[2] = alb[2]; return; }   // "unlit": el color tal cual
    float luz[3] = { 0, 0, 0 };
    float brillo[3] = { 0, 0, 0 };   // el reflejo de las LAMPARAS (no son geometria: el rayo de reflejo no las ve)
    // exponente del brillo desde la rugosidad: mate (1) = ancho y debil; pulido (0.05) = chico e intenso. Un espejo
    // perfecto (rugosidad 0) no tiene brillo de lampara puntual (seria un punto infinitesimal).
    const float rug = (m.rugosidad < 0.05f) ? 0.05f : m.rugosidad;
    const float expo = 2.0f / (rug * rug) - 2.0f + 1.0f;
    const float pesoBrillo = (m.rugosidad <= 0.0f) ? 0.0f : (m.metalico + (1.0f - m.metalico) * m.f0);   // (f0: del IOR)
    float colBrillo[3] = { m.metalico * alb[0] + (1.0f - m.metalico), m.metalico * alb[1] + (1.0f - m.metalico), m.metalico * alb[2] + (1.0f - m.metalico) };
    const RTV po = p + n * 1e-3f;   // origen de los rayos de sombra, apenas afuera de la superficie
    for (size_t i = 0; i < e.luces.size(); i++) {
        const RTLuz& L = e.luces[i];
        luz[0] += L.ambiente[0]; luz[1] += L.ambiente[1]; luz[2] += L.ambiente[2];
        float lambert = 0.0f, spec = 0.0f, att = 1.0f;
        const int nRayos = g_rtPreviewRapida ? 1 : L.rayos;
        for (int r = 0; r < nRayos; r++) {
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
            if (L.distancia > 0.0f && dist >= L.distancia) continue;   // (mas alla de su distancia no alumbra)
            float cono = 1.0f;   // (afuera del cono de un foco no llega luz; adentro, mas concentrada hacia el centro)
            if (L.spotCos > -1.0f) {
                const float sd = -Dot(ldir, L.spotDir);
                if (sd < L.spotCos) continue;
                if (L.spotExpo > 0.0f) cono = powf(sd, L.spotExpo);
            }
            const float vis = Visibilidad(e, po, ldir, dist) * cono;   // 0..1: lo transparente deja pasar parte de la luz
            if (vis <= 0.0f) continue;
            lambert += nl * vis;
            if (pesoBrillo > 0.0f) {   // Blinn-Phong: (n . h)^expo, normalizado para que el pulido concentre la misma energia
                const RTV hv = Norm(ldir + haciaCam);
                const float nh = Dot(n, hv);
                if (nh > 0.0f) spec += powf(nh, expo) * (expo + 2.0f) * 0.125f * vis;
            }
        }
        if (lambert <= 0.0f && spec <= 0.0f) continue;
        lambert /= (float)nRayos; spec /= (float)nRayos;
        if (!L.direccional) { const float dd = Largo(L.pos - po); att = 1.0f / (L.attC + L.attL * dd + L.attQ * dd * dd); if (att > 1.0f) att = 1.0f;
            att *= VentanaDistancia(dd, L.distancia); }
        for (int c = 0; c < 3; c++) { luz[c] += L.color[c] * lambert * att; brillo[c] += L.color[c] * spec * att; }
    }
    const float difusoPeso = 1.0f - m.metalico;   // el metal no tiene difuso: todo es reflejo
    for (int c = 0; c < 3; c++) out[c] = alb[c] * luz[c] * difusoPeso + colBrillo[c] * brillo[c] * pesoBrillo + m.emision[c];
}
// un rayo de camara: la primera superficie; si es transparente, su color pesa 'alpha' y el resto lo pone lo que hay
// detras (se sigue el mismo rayo, hasta 8 capas); lo aditivo se suma y no tapa
static void Radiancia(const RTEscena& e, const RTV& o0, const RTV& d, RTRng& rng, int profundidad, float* out);
static void Sombrear(const RTEscena& e, const RTV& o0, const RTV& d, RTRng& rng, float* out) { Radiancia(e, o0, d, rng, 0, out); }
static void Radiancia(const RTEscena& e, const RTV& o0, const RTV& d0, RTRng& rng, int profundidad, float* out) {
    out[0] = out[1] = out[2] = 0.0f;
    float peso = 1.0f; RTV o = o0, d = d0; float tMin = 1e-4f; int excl = -1;
    // (pasadas las capas transparentes que se permiten, lo de atras queda negro: no se suma nada mas)
    // (lo TOTALMENTE transparente en ese punto no gasta capa: ver Visibilidad)
    for (int capa = 0, vuelta = 0; capa < g_rt.capas && vuelta < 256; vuelta++) {
        RTHit h;
        if (!Trazar(e, o, d, tMin, 1e30f, false, h, excl)) { out[0] += peso * e.bg[0]; out[1] += peso * e.bg[1]; out[2] += peso * e.bg[2]; return; }
        const RTTri& T = e.tris[(size_t)h.tri];
        const RTMaterial m = MaterialEn(e, h, e.mats[(size_t)T.mat]);   // (con los mapas leidos en el impacto)
        const float b0 = 1.0f - h.b1 - h.b2;
        const RTV p = o + d * h.t;
        float alb[3] = { m.difuso[0], m.difuso[1], m.difuso[2] };
        float alfa = m.transparente ? m.alfa : 1.0f;
        if (m.tex >= 0) {
            float u, v, tx[4]; UVEn(e, h, u, v); Muestrear(e.texs[(size_t)m.tex], u, v, tx);
            alb[0] *= tx[0]; alb[1] *= tx[1]; alb[2] *= tx[2];
            if (m.transparente) alfa *= tx[3];
        }
        if (m.recorte > 0.0f) alfa = Recortar(alfa, m.recorte);
        if (alfa > 0.0f) capa++;
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
            else           { for (int c = 0; c < 3; c++) out[c] += peso * alfa * (1.0f - m.trans) * col[c]; if (m.trans <= 0.0f) peso *= (1.0f - alfa); }
        }
        // TRANSMISION: lo que el alfa saca (1 - alfa) pasa DERECHO; la parte que es vidrio (alfa * transmision) pasa
        // REFRACTADA. Cada muestra sigue uno de los dos caminos al azar, con su probabilidad (los pases lo promedian)
        if (m.trans > 0.0f && !m.aditivo) {
            const float derecho = 1.0f - alfa, vidrio = alfa * m.trans, pasa = derecho + vidrio;
            peso *= pasa;
            if (peso < 0.004f) return;
            if (m.ior > 1.0001f && rng.F() * pasa < vidrio) {
                RTV nr = Norm(V3(e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2,
                                 e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2,
                                 e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2));
                const bool entra = Dot(nr, d) < 0.0f;
                if (!entra) nr = nr * -1.0f;
                const float eta = entra ? 1.0f / m.ior : m.ior;
                const float c = -Dot(nr, d), k = 1.0f - eta * eta * (1.0f - c * c);
                d = (k < 0.0f) ? Norm(d + nr * (2.0f * c)) : Norm(d * eta + nr * (eta * c - sqrtf(k)));
                // VIDRIO ESMERILADO: la rugosidad desparrama la direccion refractada (como el reflejo del metal)
                if (m.rugosidad > 0.0f) {
                    const RTV dr = Norm(d + DirAzarEsfera(rng) * (m.rugosidad * m.rugosidad));
                    if (Dot(dr, nr) * Dot(d, nr) > 0.0f) d = dr;   // (sin volver para el lado de donde vino)
                }
                o = p; tMin = 1e-4f; excl = h.tri;
                continue;
            }
            tMin = h.t * (1.0f - 1e-5f); excl = h.tri;
            continue;
        }
        if (!m.transparente || peso < 0.004f) return;
        if (m.ior > 1.0001f && !m.aditivo && m.recorte <= 0.0f) {
            // REFRACCION (agua, vidrio): el rayo se dobla al entrar (o salir) y sigue desde aca
            RTV nr = Norm(V3(e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2,
                             e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2,
                             e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2));
            const bool entra = Dot(nr, d) < 0.0f;
            if (!entra) nr = nr * -1.0f;
            const float eta = entra ? 1.0f / m.ior : m.ior;
            const float c = -Dot(nr, d), k = 1.0f - eta * eta * (1.0f - c * c);
            d = (k < 0.0f) ? Norm(d + nr * (2.0f * c))                       // (reflexion total interna)
                           : Norm(d * eta + nr * (eta * c - sqrtf(k)));
            o = p; tMin = 1e-4f; excl = h.tri;
            continue;
        }
        tMin = h.t * (1.0f - 1e-5f); excl = h.tri;   // seguir el mismo rayo detras de esta superficie
    }
}

// ---------------------------------------------------------------------------
//  el render progresivo: acumulador + tiles
// ---------------------------------------------------------------------------
// el PRIMER IMPACTO de un rayo: el color de la superficie (albedo, con su textura) y su normal. Es lo que el
// denoiser usa para no lavar los bordes ni las texturas (se calcula una vez por pixel, en el primer pase)
static void PrimerImpacto(const RTEscena& e, const RTV& o, const RTV& d, float* alb, float* nrm) {
    alb[0] = alb[1] = alb[2] = 0.0f; nrm[0] = nrm[1] = nrm[2] = 0.0f;
    RTHit h;
    float tMin = 1e-4f; int excl = -1;
    for (int capa = 0; ; capa++) {
        if (!Trazar(e, o, d, tMin, 1e30f, false, h, excl)) { alb[0] = e.bg[0]; alb[1] = e.bg[1]; alb[2] = e.bg[2]; return; }
        // (la que tapa al menos la mitad; lo transparente y lo recortado se atraviesa: si no, el denoiser borroneaba
        // lo que se ve a traves de un vidrio o de las hojas)
        if (!e.hayTransparentes || capa + 1 >= g_rt.capas || OpacidadEn(e, h) >= 0.5f) break;
        tMin = h.t * (1.0f - 1e-5f); excl = h.tri;
    }
    const RTTri& T = e.tris[(size_t)h.tri];
    const RTMaterial m = MaterialEn(e, h, e.mats[(size_t)T.mat]);
    alb[0] = m.difuso[0]; alb[1] = m.difuso[1]; alb[2] = m.difuso[2];
    if (m.tex >= 0) { float u, v, tx[4]; UVEn(e, h, u, v); Muestrear(e.texs[(size_t)m.tex], u, v, tx); alb[0] *= tx[0]; alb[1] *= tx[1]; alb[2] *= tx[2]; }
    const float b0 = 1.0f - h.b1 - h.b2;
    RTV n = V3(e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2,
               e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2,
               e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2);
    n = Norm(n);
    if (Dot(n, d) > 0.0f) n = n * -1.0f;
    nrm[0] = n.x; nrm[1] = n.y; nrm[2] = n.z;
}
// LA ESCENA A LA GPU (render/RTGpu.h): los mismos arrays, empaquetados como los lee el compute shader
static void VertsAGpu(const RTEscena& e, std::vector<float>& out) {
    const size_t nv = e.pos.size() / 3;
    const bool conTan = e.tan.size() >= nv * 4;
    out.assign(nv * 12, 0.0f);
    for (size_t v = 0; v < nv; v++) {
        float* d = &out[v * 12];
        d[0] = e.pos[v*3]; d[1] = e.pos[v*3+1]; d[2] = e.pos[v*3+2]; d[3] = e.uv[v*2];
        d[4] = e.nrm[v*3]; d[5] = e.nrm[v*3+1]; d[6] = e.nrm[v*3+2]; d[7] = e.uv[v*2+1];
        if (conTan) for (int k = 0; k < 4; k++) d[8 + k] = e.tan[v*4 + k];
    }
}
static void NodosAGpu(const RTEscena& e, std::vector<float>& out) {
    out.assign(e.nodos.size() * 12, 0.0f);
    for (size_t i = 0; i < e.nodos.size(); i++) {
        const RTNodo& n = e.nodos[i]; float* d = &out[i * 12];
        d[0] = n.bmin[0]; d[1] = n.bmin[1]; d[2] = n.bmin[2]; d[3] = (float)n.izq;
        d[4] = n.bmax[0]; d[5] = n.bmax[1]; d[6] = n.bmax[2]; d[7] = (float)n.der;
        d[8] = (float)n.primero; d[9] = (float)n.cantidad;
    }
}
static void LucesAGpu(const RTEscena& e, std::vector<float>& out);
static bool EscenaAGpuFin(const RTEscena& e, RTGpuEscena& g);
static bool EscenaAGpu(const RTEscena& e) {
    RTGpuEscena g;
    VertsAGpu(e, g.verts);
    g.tris.resize(e.tris.size() * 4);
    for (size_t t = 0; t < e.tris.size(); t++) { g.tris[t*4] = e.tris[t].v0; g.tris[t*4+1] = e.tris[t].v1; g.tris[t*4+2] = e.tris[t].v2; g.tris[t*4+3] = e.tris[t].mat; }
    g.mats.assign(e.mats.size() * 20, 0.0f);
    for (size_t i = 0; i < e.mats.size(); i++) {
        const RTMaterial& m = e.mats[i]; float* d = &g.mats[i * 20];
        d[0] = m.difuso[0]; d[1] = m.difuso[1]; d[2] = m.difuso[2]; d[3] = m.alfa;
        d[4] = m.emision[0]; d[5] = m.emision[1]; d[6] = m.emision[2]; d[7] = m.rugosidad;
        d[8] = m.metalico; d[9] = m.f0; d[10] = (float)((m.sinLuz ? 1 : 0) | (m.transparente ? 2 : 0) | (m.aditivo ? 4 : 0));
        d[12] = (float)m.tex; d[13] = (float)m.nmTex; d[14] = (float)m.emTex; d[15] = (float)m.rugTex;
        d[11] = m.ior;
        d[16] = (float)m.metTex; d[17] = (float)m.alfaTex; d[18] = m.recorte; d[19] = m.trans;
    }
    LucesAGpu(e, g.luces);
    NodosAGpu(e, g.nodos);
    g.orden = e.orden;
    size_t total = 0; for (size_t i = 0; i < e.texs.size(); i++) total += (size_t)e.texs[i].w * e.texs[i].h;
    g.texels.resize(total); g.texInfo.resize(e.texs.size() * 4);
    return EscenaAGpuFin(e, g);
}
static void LucesAGpu(const RTEscena& e, std::vector<float>& out) {
    out.assign(e.luces.size() * 20, 0.0f);
    for (size_t i = 0; i < e.luces.size(); i++) {
        const RTLuz& L = e.luces[i]; float* d = &out[i * 20];
        d[0] = L.pos.x; d[1] = L.pos.y; d[2] = L.pos.z; d[3] = L.direccional ? 1.0f : -L.distancia;   // (<= 0: -distancia)
        if (L.direccional) { d[4] = L.dir.x; d[5] = L.dir.y; d[6] = L.dir.z; }
        else { d[4] = L.spotDir.x; d[5] = L.spotDir.y; d[6] = L.spotDir.z; }
        d[7] = L.radio;
        d[8] = L.color[0]; d[9] = L.color[1]; d[10] = L.color[2]; d[11] = (float)L.rayos;
        d[12] = L.ambiente[0]; d[13] = L.ambiente[1]; d[14] = L.ambiente[2]; d[15] = L.spotCos;
        d[16] = L.attC; d[17] = L.attL; d[18] = L.attQ; d[19] = L.spotExpo;
    }
}
// (las texturas, el fondo y a la GPU)
static bool EscenaAGpuFin(const RTEscena& e, RTGpuEscena& g) {
    size_t desde = 0;
    for (size_t i = 0; i < e.texs.size(); i++) {
        const RTTextura& t = e.texs[i]; const size_t n = (size_t)t.w * t.h;
        if (n) memcpy(&g.texels[desde], t.px, n * 4);
        g.texInfo[i*4] = (int)desde; g.texInfo[i*4+1] = t.w; g.texInfo[i*4+2] = t.h; g.texInfo[i*4+3] = t.filtrado ? 1 : 0;
        desde += n;
    }
    g.bg[0] = e.bg[0]; g.bg[1] = e.bg[1]; g.bg[2] = e.bg[2];
    g.hayTransparentes = e.hayTransparentes;
    return RTGpuCargarEscena(g);
}
static bool g_rtUltimoEnGpu = false;
// LA GPU ES UNA SOLA (una escena y una imagen de un tamano) y la comparten dos renders: el del viewport y el de a
// archivo (Render Image / F12, a otra resolucion). El que la uso ultimo es su duenio; el otro, antes de volver a usarla,
// sube de nuevo su escena y su imagen (si no, leia la imagen del otro tamano en su buffer: se colgaba el editor)
static const void* g_rtGpuDuenio = NULL;
// LOS RAYOS POR SEGUNDO de la GPU (los que traza mientras trabaja: los pases de este cuadro)
static double g_rtRayosPorSeg = 0.0;
static void RTContarRayos(unsigned long long rayos, double ms) {
    if (ms <= 0.05 || rayos == 0) return;
    const double r = (double)rayos / (ms / 1000.0);
    g_rtRayosPorSeg = (g_rtRayosPorSeg <= 0.0) ? r : g_rtRayosPorSeg * 0.8 + r * 0.2;
    static const bool perfil = getenv("W3D_RT_PERFIL") != NULL;
    if (perfil) w3dLogf("[rt perfil] %llu rayos en %.2f ms: %.0f Mrayos/s", rayos, ms, r / 1e6);
}
double RTRayosPorSegundo() { return g_rtRayosPorSeg; }
// W3D_RT_PERFIL=1: cuanto tarda armar la escena, subirla y cada pase (al log)
static const bool g_rtPerfil = getenv("W3D_RT_PERFIL") != NULL;

static const int kTile = 32;
// VISTA PREVIA progresiva por resolucion: antes del primer pase a resolucion completa se hacen
// pasadas rapidas con 1 rayo por BLOQUE de s x s pixeles (s = 6, 4, 2). En un viewport de 240p
// equivalen a ~40p, ~60p y ~120p: la imagen entera aparece enseguida (clave en el N95: ~1/36,
// 1/16 y 1/4 del costo) y se va afinando; recien despues arranca la acumulacion real de samples.
static const int kPrevPasos[3] = { 6, 4, 2 };
static const int kPrevNiveles = 3;   // nivel 0..2 = preview; kPrevNiveles = resolucion completa
struct RTRender {
    RTEscena esc; RTCamara cam;
    int w, h, tilesX, tilesY;
    std::vector<float> acum;             // 3 por pixel
    std::vector<unsigned char> img;      // RGBA 8 bits (lo que se muestra / guarda)
    // el DENOISER (g_rt.denoiser): el albedo y la normal del primer impacto (del primer pase) y el resultado limpio
    std::vector<float> aovAlb, aovNrm, limpio, colorMedio;
    int denoiser;                        // el de este render (cambiarlo reinicia el render: RTInvalidar)
    bool denoiserFallo;                  // (fallo una vez: no se insiste, se ve sin limpiar)
    int paso;                            // pases completos (solo los de resolucion completa)
    int nivel;                           // escalera de preview: 0..2 = bloques de 6/4/2 px; 3 = resolucion completa
    int tileSig;                         // proximo tile del pase en curso
    std::vector<unsigned char> tileSucio; // tiles con pixeles nuevos desde la ultima subida
    bool gpu;                            // este render va en la GPU (render/RTGpu.h); si falla sigue por CPU
    bool aovGpu;                         // (los AOV del denoiser ya bajaron de la GPU)
    double ultimoDenoise;                // (los denoisers de la CPU, de vez en cuando mientras se forma la imagen)
    // ANTES DE CADA PASE (el desenfoque de movimiento: mueve la escena y la camara al instante de ese pase); NULL = nada
    void (*antesDelPase)(RTRender& r, void* ctx);
    void* antesCtx;
    RTRender() : w(0), h(0), tilesX(0), tilesY(0), paso(0), nivel(kPrevNiveles), tileSig(0), denoiser(0), denoiserFallo(false), gpu(false), aovGpu(false), ultimoDenoise(0.0), antesDelPase(NULL), antesCtx(NULL), semilla(0) {}
    // la SEMILLA del ruido de una imagen nueva: con "otro ruido en cada imagen" cambia (si no, 0: la misma siempre)
    unsigned int semilla;
    void NuevaSemilla() { static unsigned int cont = 0; semilla = g_rt.semillaVariable ? ++cont : 0u; }
    void Iniciar(Viewport3D* vp, int W, int H, bool conPreview) {
        NuevaSemilla();
        w = W; h = H; tilesX = (w + kTile - 1) / kTile; tilesY = (h + kTile - 1) / kTile;
        acum.assign((size_t)w * h * 3, 0.0f); img.assign((size_t)w * h * 4, 255);
        tileSucio.assign((size_t)tilesX * tilesY, 0);
        paso = 0; tileSig = 0;
        nivel = conPreview ? 0 : kPrevNiveles;   // el render a archivo va directo a resolucion completa
        denoiser = W3dDenoiserDisponible(g_rt.denoiser) ? g_rt.denoiser : 0;
        denoiserFallo = false;
        if (denoiser) { aovAlb.assign((size_t)w * h * 3, 0.0f); aovNrm.assign((size_t)w * h * 3, 0.0f); }
        else { aovAlb.clear(); aovNrm.clear(); }
        const double tp0 = W3dNowMs();
        ArmarEscena(esc); CamaraDe(vp, w, h, cam, conPreview);
        const double tp1 = W3dNowMs();
        // LA GPU: la escena sube una vez; los pases son tan rapidos que no hace falta la escalera de preview
        gpu = g_rt.gpu && RTGpuDisponible() && EscenaAGpu(esc) && RTGpuImagen(w, h);
        if (g_rtPerfil) w3dLogf("[rt perfil] armar %.1f ms, a la gpu %.1f ms (%d tris, %d texturas)", tp1 - tp0, W3dNowMs() - tp1,
                                             (int)esc.tris.size(), (int)esc.texs.size());
        aovGpu = false;
        if (gpu) { nivel = kPrevNiveles; g_rtGpuDuenio = this; }
        g_rtUltimoEnGpu = gpu;
    }
    // la GPU la uso el otro render: la escena y la imagen de este otra vez, y la acumulacion de cero (la de la GPU se
    // perdio). false = no se pudo: sigue por CPU
    bool RetomarGpu() {
        if (g_rtGpuDuenio == this) return true;
        paso = 0; tileSig = 0; aovGpu = false;
        if (!(RTGpuDisponible() && EscenaAGpu(esc) && RTGpuImagen(w, h))) {
            gpu = false; g_rtUltimoEnGpu = false; acum.assign(acum.size(), 0.0f);
            return false;
        }
        g_rtGpuDuenio = this;
        return true;
    }
    // EN LA GPU: pases enteros hasta agotar el presupuesto; despues la imagen (y el color y los AOV si hay denoiser)
    // baja una vez. Si la GPU falla, el render sigue por CPU desde cero
    bool AvanzarGpu(double presupuestoMs) {
        if (!RetomarGpu()) return true;
        const double t0 = W3dNowMs();
        RTGpuCamara c;
        const int ns = (g_rt.samples < 1) ? 1 : g_rt.samples;
        double msPases = 0.0;
        while (!Terminado()) {
            if (antesDelPase) { antesDelPase(*this, antesCtx); if (!gpu) return true; }   // (si la GPU fallo, sigue por CPU)
            c.pos[0] = cam.pos.x; c.pos[1] = cam.pos.y; c.pos[2] = cam.pos.z; c.der[0] = cam.der.x; c.der[1] = cam.der.y; c.der[2] = cam.der.z;
            c.arr[0] = cam.arr.x; c.arr[1] = cam.arr.y; c.arr[2] = cam.arr.z; c.adel[0] = cam.adel.x; c.adel[1] = cam.adel.y; c.adel[2] = cam.adel.z;
            c.sx = cam.sx; c.sy = cam.sy; c.zoom = cam.zoom; c.panX = cam.panX; c.panY = cam.panY; c.orto = cam.orto;
            c.lente = cam.lente; c.foco = cam.foco;
            c.semilla = semilla;
            const double tq = W3dNowMs();
            const bool ok = RTGpuPase(c, paso, ns, denoiser && paso == 0, g_rt.capas);
            msPases += W3dNowMs() - tq;
            if (g_rtPerfil) w3dLogf("[rt perfil] pase %d: %.1f ms", paso, W3dNowMs() - tq);
            if (!ok) { gpu = false; g_rtUltimoEnGpu = false; paso = 0; acum.assign(acum.size(), 0.0f); return true; }
            paso++;
            if (presupuestoMs > 0 && W3dNowMs() - t0 > presupuestoMs) break;
        }
        RTContarRayos(RTGpuRayos(), msPases);
        // EL DENOISER: el de la GPU trabaja sobre lo acumulado sin bajarlo (va en tiempo real); los otros (OIDN, OptiX)
        // necesitan el color en la CPU: mientras se forma la imagen, como mucho cada 250 ms, y siempre al terminar
        bool den = denoiser && !denoiserFallo;
        if (den && denoiser == W3D_DENOISER_GPU) {
            if (!RTGpuDenoisearLoAcumulado(paso)) { denoiserFallo = true; w3dLogfW("[rt] el denoiser de la GPU fallo"); }
            den = false;
        } else if (den && !Terminado() && W3dNowMs() - ultimoDenoise < 250.0) den = false;
        if (den) { colorMedio.resize((size_t)w * h * 3); limpio.resize((size_t)w * h * 3); }
        const bool bajarAov = den && !aovGpu;
        if (!RTGpuLeer(&img[0], den ? &colorMedio[0] : 0, bajarAov ? &aovAlb[0] : 0, bajarAov ? &aovNrm[0] : 0, paso)) {
            gpu = false; g_rtUltimoEnGpu = false; paso = 0; acum.assign(acum.size(), 0.0f); return true;
        }
        if (den) { aovGpu = true; AplicarDenoiser(); ultimoDenoise = W3dNowMs(); }
        for (size_t t = 0; t < tileSucio.size(); t++) tileSucio[t] = 1;
        return true;
    }
    bool Terminado() const { return paso >= g_rt.pases; }
    // SOLO CAMBIARON LAS POSES (la banda tocando): los vertices nuevos, el BVH reajustado y, en la GPU, solo eso de
    // nuevo (no los materiales, las texturas ni las luces); la acumulacion de cero
    void ActualizarPosesYLuces(Viewport3D* vp, bool poses, bool luces) {
        const double t0 = W3dNowMs();
        const bool ajena = gpu && g_rtGpuDuenio != this;   // (en la GPU esta la escena del otro render: va entera)
        if (poses) {
            ActualizarVivas(esc);
            BVHAjustar(esc);
            if (gpu && !ajena) {
                std::vector<float> vs, ns; VertsAGpu(esc, vs); NodosAGpu(esc, ns);
                if (!RTGpuActualizarGeometria(vs, ns)) { gpu = false; g_rtUltimoEnGpu = false; }
            }
        }
        if (luces) {
            esc.luces.clear(); AgregarLuces(esc);
            if (gpu && !ajena) {
                std::vector<float> ls; LucesAGpu(esc, ls);
                if (!RTGpuActualizarLuces(ls)) { gpu = false; g_rtUltimoEnGpu = false; }
            }
        }
        if (ajena) RetomarGpu();
        if (g_rtPerfil) w3dLogf("[rt perfil] %s%s %.1f ms (%d mallas, %d luces)", poses ? "mallas " : "", luces ? "luces" : "",
                                W3dNowMs() - t0, (int)esc.vivas.size(), (int)esc.luces.size());
        NuevaCamara(vp);
    }
    // la camara se movio (la escena es la misma): la acumulacion de cero con la camara nueva
    void NuevaCamara(Viewport3D* vp) {
        CamaraDe(vp, w, h, cam, true);
        paso = 0; tileSig = 0; aovGpu = false;
        NuevaSemilla();
        if (!gpu) { acum.assign(acum.size(), 0.0f); nivel = 0; }   // (la CPU vuelve a la escalera de preview)
        for (size_t t = 0; t < tileSucio.size(); t++) tileSucio[t] = 0;
    }
    // un tile de PREVIEW: 1 rayo por bloque de s x s (al centro del bloque), pinta el bloque entero.
    // No toca el acumulador: es solo para que se vea algo enseguida; los pases reales lo pisan.
    void TilePreview(int ti) {
        const int tx = (ti % tilesX) * kTile, ty = (ti / tilesX) * kTile;
        const int x1 = (tx + kTile < w) ? tx + kTile : w, y1 = (ty + kTile < h) ? ty + kTile : h;
        const int s = kPrevPasos[nivel];
        for (int y = ty; y < y1; y += s) for (int x = tx; x < x1; x += s) {
            RTRng rng((unsigned int)(x * 1973 + y * 9277 + 1));
            RTV o, d; RayoPixel(cam, x + s * 0.5f, y + s * 0.5f, w, h, o, d);
            float m[3]; Sombrear(esc, o, d, rng, m);
            unsigned char rgb[3];
            for (int k = 0; k < 3; k++) { float v = m[k]; if (v < 0) v = 0; if (v > 1) v = 1; rgb[k] = (unsigned char)(v * 255.0f + 0.5f); }
            const int bx1 = (x + s < x1) ? x + s : x1, by1 = (y + s < y1) ? y + s : y1;
            for (int yy = y; yy < by1; yy++) {
                unsigned char* p = &img[((size_t)yy * w + x) * 4];
                for (int xx = x; xx < bx1; xx++, p += 4) { p[0] = rgb[0]; p[1] = rgb[1]; p[2] = rgb[2]; p[3] = 255; }
            }
        }
        tileSucio[(size_t)ti] = 1;
    }
    void Tile(int ti) {   // un tile del pase actual: 'samples' muestras por pixel
        if (nivel < kPrevNiveles) { TilePreview(ti); return; }
        const int tx = (ti % tilesX) * kTile, ty = (ti / tilesX) * kTile;
        const int x1 = (tx + kTile < w) ? tx + kTile : w, y1 = (ty + kTile < h) ? ty + kTile : h;
        const int ns = (g_rt.samples < 1) ? 1 : g_rt.samples;
        const int pasesHechos = paso + 1;
        for (int y = ty; y < y1; y++) for (int x = tx; x < x1; x++) {
            RTRng rng((unsigned int)(x * 1973 + y * 9277 + paso * 26699 + 1) ^ (semilla * 2654435761u));
            float c[3] = { 0, 0, 0 };
            for (int s = 0; s < ns; s++) {
                RTV o, d; RayoPixel(cam, x + rng.F(), y + rng.F(), w, h, o, d); RayoLente(cam, rng, o, d);
                float m[3]; Sombrear(esc, o, d, rng, m);
                c[0] += m[0]; c[1] += m[1]; c[2] += m[2];
            }
            float* a = &acum[((size_t)y * w + x) * 3];
            a[0] += c[0] / ns; a[1] += c[1] / ns; a[2] += c[2] / ns;
            // el DENOISER: en el primer pase, el albedo y la normal del centro del pixel; despues, la imagen que se
            // ve es la LIMPIA de cada pase terminado (los tiles con ruido no se muestran)
            if (denoiser && paso == 0) {
                RTV o, d; RayoPixel(cam, x + 0.5f, y + 0.5f, w, h, o, d);
                PrimerImpacto(esc, o, d, &aovAlb[((size_t)y * w + x) * 3], &aovNrm[((size_t)y * w + x) * 3]);
            }
            if (denoiser && !denoiserFallo && paso > 0) continue;
            unsigned char* p = &img[((size_t)y * w + x) * 4];
            for (int k = 0; k < 3; k++) { float v = a[k] / (float)pasesHechos; if (v < 0) v = 0; if (v > 1) v = 1; p[k] = (unsigned char)(v * 255.0f + 0.5f); }
            p[3] = 255;
        }
        tileSucio[(size_t)ti] = 1;
    }
    // avanza hasta agotar el presupuesto (ms; <= 0 = sin limite) o terminar el pase; true si hizo algo
    bool Avanzar(double presupuestoMs) {
        if (Terminado()) return false;
        if (gpu) return AvanzarGpu(presupuestoMs);
        if (antesDelPase && tileSig == 0 && nivel >= kPrevNiveles) { antesDelPase(*this, antesCtx); if (gpu) return AvanzarGpu(presupuestoMs); }
        const int total = tilesX * tilesY;
        const double t0 = W3dNowMs();
        g_rtPreviewRapida = (nivel < kPrevNiveles);   // preview: 1 rayo de sombra por luz (ver LuzEn)
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
            // en el N95 W3dNowMs es User::NTickCount (~ms): el presupuesto corta bien. El tope de tiles
            // por llamada queda como red de seguridad por si el reloj no avanza (t0 en 0).
            const bool sinReloj = (t0 <= 0.0);
            int hechos = 0;
            while (tileSig < total) {
                Tile(tileSig); tileSig++; hechos++;
                if (presupuestoMs > 0 && W3dNowMs() - t0 > presupuestoMs) break;
                if (presupuestoMs > 0 && sinReloj && hechos >= 4) break;
            }
        }
        if (tileSig >= total) { tileSig = 0; if (nivel < kPrevNiveles) nivel++; else { paso++; Denoisear(); } }
        return true;
    }
    // un pase TERMINADO: el promedio de lo acumulado pasa por el denoiser y eso es lo que se ve (o se guarda)
    void Denoisear() {
        if (!denoiser || denoiserFallo || paso <= 0) return;
        const size_t n = (size_t)w * h * 3;
        colorMedio.resize(n); limpio.resize(n);
        const float inv = 1.0f / (float)paso;
        for (size_t i = 0; i < n; i++) { float v = acum[i] * inv; colorMedio[i] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
        AplicarDenoiser();
    }
    // colorMedio (el promedio de los pases) -> el denoiser -> la imagen que se ve
    void AplicarDenoiser() {
        std::string err;
        if (!W3dDenoiserAplicar(denoiser, &colorMedio[0], aovAlb.empty() ? 0 : &aovAlb[0], aovNrm.empty() ? 0 : &aovNrm[0], w, h, &limpio[0], &err)) {
            denoiserFallo = true;   // (se sigue viendo el render sin limpiar)
            w3dLogfW("[rt] el denoiser fallo: %s", err.c_str());
            for (size_t i = 0; i < (size_t)w * h; i++) {
                unsigned char* p = &img[i * 4];
                for (int k = 0; k < 3; k++) p[k] = (unsigned char)(colorMedio[i * 3 + k] * 255.0f + 0.5f);
            }
        } else {
            for (size_t i = 0; i < (size_t)w * h; i++) {
                unsigned char* p = &img[i * 4];
                for (int k = 0; k < 3; k++) { float v = limpio[i * 3 + k]; if (v < 0) v = 0; if (v > 1) v = 1; p[k] = (unsigned char)(v * 255.0f + 0.5f); }
                p[3] = 255;
            }
        }
        for (size_t t = 0; t < tileSucio.size(); t++) tileSucio[t] = 1;
    }
};

// ---------------------------------------------------------------------------
//  viewport: textura de pantalla + firma para reiniciar cuando cambia la vista o la escena
// ---------------------------------------------------------------------------
static RTRender* g_rtVp = NULL;
static const Viewport3D* g_rtVpDe = NULL;
// el viewport del preview MURIO (abrir un proyecto libera el layout anterior): olvidarlo. El
// puntero solo se compara, pero con la direccion reciclada un 3D NUEVO en la misma direccion
// seguiria el preview del muerto en vez de reiniciarlo (LayoutArbol.h).
static void RTOlvidarViewport(ViewportBase* vp) {
    if (g_rtVpDe && (const ViewportBase*)g_rtVpDe == vp) g_rtVpDe = NULL;
}
struct RTEngancharOlvido { RTEngancharOlvido() { ViewportOlvidarRegistrar(RTOlvidarViewport); } };
static RTEngancharOlvido g_rtEngancheOlvido;
static unsigned int g_rtTexId = 0; static int g_rtTexW = 0, g_rtTexH = 0;
static bool g_rtTexLineal = false;
static double g_rtFirma = 0.0, g_rtFirmaCam = 0.0, g_rtFirmaPoses = 0.0, g_rtFirmaLuces = 0.0;
static bool g_rtForzar = false;

static int Pot(int v) { int p = 1; while (p < v) p <<= 1; return p; }
// LA CAMARA y LA ESCENA por separado: si solo se movio la camara (orbitar, acercarse) la escena armada se queda (el
// BVH, las texturas y, en la GPU, todo lo subido) y solo se vuelve a empezar la acumulacion
static double FirmaCamara(Viewport3D* vp) {
    CameraBase cam = vp->VistaCam();
    double f = cam.pos.x * 1.1 + cam.pos.y * 2.3 + cam.pos.z * 3.7 + cam.rot.x * 5.1 + cam.rot.y * 7.3 + cam.rot.z * 11.7 + cam.rot.w * 13.1;
    f += vp->orbitDistance * 0.37 + (vp->orthographic ? 1000.0 : 0.0) + fovDeg * 0.01 + (vp->ViewFromCameraActive ? 7.7 : 0.0);
    const Viewport3D::Lente L = vp->LenteActual();
    f += L.sx * 3.3 + L.sy * 5.9 + L.zoom * 11.3 + L.panX * 13.7 + L.panY * 17.9;
    // (la profundidad de campo: un foco animado tiene que volver a empezar la imagen)
    if (CameraActive && CameraActive->dof) f += CameraActive->RadioLente() * 101.3 + CameraActive->DistanciaFoco() * 0.913 + 3.1;
    return f;
}
// las POSES de las mallas con esqueleto (cambian en cada cuadro de la banda: no hace falta armar la escena de nuevo)
// LAS LUCES: donde estan, hacia donde y como alumbran (un lightshow las cambia en cada cuadro: solo se vuelven a
// mandar ellas)
static double FirmaLuces() {
    double f = 0.0;
    for (size_t i = 0; i < Lights.size(); i++) if (Lights[i]) {
        Light* l = Lights[i]; Matrix4 W; l->GetWorldMatrix(W);
        f += l->rtRadio * 3.3 + l->rtRayos * 0.7 + l->diffuse[0] * 1.1 + l->diffuse[1] * 2.3 + l->diffuse[2] * 3.7 + (l->direccional ? 9.0 : 0.0)
           + l->ambient[0] * 0.41 + l->ambient[1] * 0.43 + l->ambient[2] * 0.47 + l->spotCutoff * 0.013 + l->spotExponent * 0.17
           + l->intensidad * 1.7 + l->distancia * 2.3
           + l->attConstant * 0.19 + l->attLinear * 0.23 + l->attQuadratic * 0.29 + (l->visible ? 0.77 : 0.0)
           + W.m[12] * 0.31 + W.m[13] * 0.53 + W.m[14] * 0.71 + W.m[4] * 0.13 + W.m[5] * 0.17 + W.m[8] * 0.37 + W.m[10] * 0.59;
        f *= 1.0000013;   // (que importe cual es cual)
    }
    return f;
}
static double FirmaPoses() {
    struct S { static void Rec(Object* o, double& f) { if (!o) return;
                 if (o->getType() == ObjectType::mesh) {
                     Mesh* m = (Mesh*)o;
                     if (m->skinArmature) f += m->skinPoseSerial * 0.0071 + 0.31;
                     Matrix4 W; m->GetWorldMatrix(W);   // (donde esta: moverla es solo reajustar)
                     f += W.m[12] * 0.31 + W.m[13] * 0.53 + W.m[14] * 0.71 + W.m[0] * 0.13 + W.m[5] * 0.17 + W.m[2] * 0.19 + W.m[8] * 0.23;
                     f *= 1.0000007;
                 }
                 for (size_t i = 0; i < o->Childrens.size(); i++) Rec(o->Childrens[i], f); } };
    double f = 0.0; S::Rec(SceneCollection, f);
    return f;
}
static double FirmaVista(Viewport3D* vp) {
    double f = vp->width * 0.001 + vp->height * 0.0013;
    // escena: cuantos objetos y donde estan (posiciones de mundo) + luces + version de las mallas
    struct S { static void Rec(Object* o, double& f) { if (!o) return; if (o->getType() == ObjectType::mesh) { Mesh* m = (Mesh*)o; f += m->geoVersion * 0.019 + (m->genValido ? 0.5 : 0.0); }
                 const ObjectType t = o->getType();
                 // (las luces van en FirmaLuces, la camara en FirmaCamara y donde esta cada malla en FirmaPoses: aca
                 // solo lo que esta y si se ve)
                 if (t != ObjectType::light && t != ObjectType::camera) f += (o->visible ? 0.7 : 0.0) + 0.013;
                 f *= 1.0000003;
                 for (size_t i = 0; i < o->Childrens.size(); i++) Rec(o->Childrens[i], f); } };
    S::Rec(SceneCollection, f);
    f += (double)Lights.size() * 9.1;   // (cuantas: agregar o borrar una arma de nuevo)
    if (g_localViewActivo && g_localViewVisibles) f += 77.7 + (double)g_localViewVisibles->size() * 1.31;   // (la vista local)
    f += (g_rt.semillaVariable ? 0.33 : 0.0) + g_rt.samples * 0.101 + g_rt.rayos * 0.047 + g_rt.pases * 0.0071 + g_renderBg[0] + g_renderBg[1] * 2 + g_renderBg[2] * 3 + g_rt.denoiser * 17.3 + (g_rt.gpu ? 23.9 : 0.0);
    for (size_t i = 0; i < Materials.size(); i++) if (Materials[i]) { Material* mt = Materials[i];
        f += mt->diffuse[0] * 0.21 + mt->diffuse[1] * 0.23 + mt->diffuse[2] * 0.29 + mt->diffuse[3] * 0.37 + (mt->transparent ? 1.7 : 0.0) + mt->mezcla * 0.13 + mt->rtRugosidad * 0.41 + mt->rtMetalico * 0.43 + mt->emission[0] + mt->emission[1] + mt->emission[2] + (mt->lighting ? 0.3 : 0.0) + (mt->textureOn ? 0.9 : 0.0) + (mt->texture ? mt->texture->iID * 0.011 : 0.0) + (mt->normalMap ? 2.3 : 0.0) + (mt->normalTexture ? mt->normalTexture->iID * 0.017 : 0.0);
        // (los MAPAS: numero / textura, su textura y el IOR tambien cambian lo que se ve)
        for (int k = W3D_MAPA_EMISION; k < W3D_MAPAS; k++)
            f += (mt->mapaConTextura[k] ? 3.1 * (k + 1) : 0.0) + (mt->mapaTex[k] ? mt->mapaTex[k]->iID * 0.019 * (k + 1) : 0.0);
        f += mt->ior * 0.47 + mt->transmision * 0.53; }
    return f;
}
void RTInvalidar() { g_rtForzar = true; }
static double g_rtUltimaImagen = 0.0;
static float g_rtImgPorSeg = 0.0f;
static void RTContarImagen() {
    const double t = W3dNowMs();
    if (g_rtUltimaImagen > 0.0) {
        const double dt = t - g_rtUltimaImagen;
        if (dt > 0.0 && dt < 5000.0) {
            const float fps = (float)(1000.0 / dt);
            g_rtImgPorSeg = (g_rtImgPorSeg <= 0.0f) ? fps : g_rtImgPorSeg * 0.8f + fps * 0.2f;
        }
    }
    g_rtUltimaImagen = t;
}
float RTImagenesPorSegundo() {
    // (sin imagenes nuevas hace rato: se va a cero, no se queda con lo ultimo)
    if (g_rtUltimaImagen > 0.0 && W3dNowMs() - g_rtUltimaImagen > 2000.0) return 0.0f;
    return g_rtImgPorSeg;
}
bool RTActivoEn(const Viewport3D* vp) { return g_rt.on && g_rtVp && g_rtVpDe == vp && !g_rtVp->Terminado(); }

const char* RTReescaladorNombre(int r) { return r == 1 ? "FSR 1 (AMD)" : "Linear"; }
// el REESCALADO con FSR (g_rt.reescalador = 1): la imagen trazada chica -> la del viewport, por CPU, y esa se sube entera.
// Mientras se va formando, a lo sumo cada ~60 ms (cuesta como un pase): lo que no se subio queda sucio para el siguiente
static std::vector<unsigned char> g_rtFsr;
static int g_rtFsrW = 0, g_rtFsrH = 0;
static double g_rtFsrUlt = 0.0;
static void SubirFSR(RTRender* r, bool terminado) {
    bool sucio = false;
    for (size_t t = 0; t < r->tileSucio.size() && !sucio; t++) sucio = r->tileSucio[t] != 0;
    if (!sucio) return;
    const double ahora = W3dNowMs();
    if (!terminado && ahora - g_rtFsrUlt < 60.0) { g_redraw = true; return; }
    g_rtFsrUlt = ahora;
    g_rtFsr.resize((size_t)g_rtFsrW * g_rtFsrH * 4);
    W3dFSR1(&r->img[0], r->w, r->h, &g_rtFsr[0], g_rtFsrW, g_rtFsrH, 0.2f, g_rt.hilos);
    w3dEngine::UpdateRGBA(g_rtTexId, 0, 0, g_rtFsrW, g_rtFsrH, &g_rtFsr[0], false);
    for (size_t t = 0; t < r->tileSucio.size(); t++) r->tileSucio[t] = 0;
}
static void SubirTiles(RTRender* r) {
    if (!g_rtTexId) return;
    // (todo sucio, lo de la GPU: la imagen entera de una vez y no ~2000 tiles de 32x32)
    bool todos = !r->tileSucio.empty();
    for (size_t t = 0; t < r->tileSucio.size() && todos; t++) todos = r->tileSucio[t] != 0;
    if (todos) {
        w3dEngine::UpdateRGBA(g_rtTexId, 0, 0, r->w, r->h, &r->img[0], false);
        for (size_t t = 0; t < r->tileSucio.size(); t++) r->tileSucio[t] = 0;
        return;
    }
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
    const double firma = FirmaVista(vp), firmaCam = FirmaCamara(vp), firmaPoses = FirmaPoses(), firmaLuces = FirmaLuces();
    // la RESOLUCION del trazado: la del viewport; en el telefono, un viewport grande (la TV a 720p) se traza a la mitad
    // y se estira: a 1280x720 la textura de potencia de 2 era de 2048x1024 (8 MB) en una VideoCore que ya andaba justa
    // de memoria con la superficie de la TV (y el preview salia NEGRO), y el CPU tardaba 4 veces mas por pase
    int rw = vp->width, rh = vp->height;
#ifdef W3D_SYMBIAN
    while (rw > 640) { rw /= 2; rh /= 2; }
    if (rh < 1) rh = 1;
#endif
    // el REESCALADO (g_rt.escala): menos pixeles que trazar, la textura se estira con filtro lineal
    const int esc = g_rt.escala < 1 ? 1 : (g_rt.escala > 8 ? 8 : g_rt.escala);
    const int vw = rw, vh = rh;   // (lo que se ve: el destino del reescalado)
    if (esc > 1) { rw /= esc; rh /= esc; if (rw < 1) rw = 1; if (rh < 1) rh = 1; }
#ifdef W3D_SYMBIAN
    const bool fsr = false;       // (a 333 MHz cuesta mas que el pase: lineal)
#else
    const bool fsr = esc > 1 && g_rt.reescalador == 1;
#endif
    // la textura: la imagen trazada tal cual (estirada al dibujarla) o, con FSR, ya agrandada al tamano de la vista
    const int texW = fsr ? vw : rw, texH = fsr ? vh : rh;
    if (fsr && (g_rtFsrW != vw || g_rtFsrH != vh)) { g_rtFsrW = vw; g_rtFsrH = vh; g_rtForzar = true; }
    if (!fsr && g_rtFsrW) { g_rtFsrW = g_rtFsrH = 0; g_rtFsr.clear(); g_rtForzar = true; }
    if (!g_rtVp || g_rtVpDe != vp || g_rtVp->w != rw || g_rtVp->h != rh || firma != g_rtFirma || g_rtForzar) {
        if (!g_rtVp) g_rtVp = new RTRender();
        g_rtVp->Iniciar(vp, rw, rh, true);   // con la escalera de preview (40p/60p/120p en un viewport de 240p)
        g_rtVpDe = vp; g_rtFirma = firma; g_rtFirmaCam = firmaCam; g_rtFirmaPoses = firmaPoses; g_rtFirmaLuces = firmaLuces; g_rtForzar = false;
        // textura de pantalla, potencia de 2 (GLES 1.1 del N95 no tiene NPOT), sin mips
        const int tw = Pot(texW), th = Pot(texH);
        const bool lineal = esc > 1 && !fsr;   // (a resolucion completa o ya agrandada con FSR, pixel a pixel)
        if (!g_rtTexId || tw != g_rtTexW || th != g_rtTexH || lineal != g_rtTexLineal) {
            if (g_rtTexId) w3dEngine::DeleteTexture(g_rtTexId);
            g_rtTexLineal = lineal;
            // negro OPACO: la imagen trazada no tiene transparencia (en la Dreamcast una textura opaca va en RGB565 y no
            // en ARGB4444, que dejaba bandas marcadas en la luz)
            std::vector<unsigned char> vacio((size_t)tw * th * 4, 0);
            for (size_t i = 3; i < vacio.size(); i += 4) vacio[i] = 255;
            g_rtTexId = w3dEngine::UploadRGBA(&vacio[0], tw, th, lineal, false);
            g_rtTexW = tw; g_rtTexH = th;
            if (!g_rtTexId) w3dLogf("[rt] no se pudo crear la textura del preview de %dx%d (memoria grafica?)", tw, th);
        }
    } else if (g_rt.soloFinal && !g_rtVp->Terminado()) {
        // "SOLO LA IMAGEN FINAL": la que se esta trazando se termina antes de pasar a lo que cambio (si cada cambio la
        // empezara de nuevo, con Play nunca terminaria ninguna y la vista quedaba congelada en la ultima)
    } else if (firmaPoses != g_rtFirmaPoses || firmaLuces != g_rtFirmaLuces) {
        // las poses y/o las luces (y quiza la camara): solo eso de nuevo
        g_rtVp->ActualizarPosesYLuces(vp, firmaPoses != g_rtFirmaPoses, firmaLuces != g_rtFirmaLuces);
        g_rtFirmaPoses = firmaPoses; g_rtFirmaLuces = firmaLuces; g_rtFirmaCam = firmaCam;
    } else if (firmaCam != g_rtFirmaCam) {   // solo la camara: la misma escena, la acumulacion de cero
        g_rtVp->NuevaCamara(vp);
        g_rtFirmaCam = firmaCam;
    }
    // avanza con un presupuesto por frame: la UI sigue viva y la imagen se completa de a tiles.
    // En el N95 el presupuesto es mayor: cada frame ademas paga el render GL de la escena, asi que
    // con 40ms la mitad del tiempo se iba en dibujar lo que la imagen trazada tapa; con 80ms el
    // render termina antes y la UI sigue usable (~8 fps mientras traza).
#ifdef W3D_SYMBIAN
    const double kPresupuestoMs = 80.0;
#else
    // (en la GPU cada pase es de pocos ms: con un cuadro de 60 Hz la vista sigue a la camara en tiempo real)
    const double kPresupuestoMs = g_rtVp->gpu ? 14.0 : 40.0;
#endif
    if (!g_rtVp->Terminado()) { g_rtVp->Avanzar(kPresupuestoMs); g_redraw = true; if (g_rtVp->Terminado()) RTContarImagen(); }
    // "solo la imagen final": la textura se queda con la ultima terminada hasta que esta tambien termine
    if (!g_rt.soloFinal || g_rtVp->Terminado()) { if (fsr) SubirFSR(g_rtVp, g_rtVp->Terminado()); else SubirTiles(g_rtVp); }
    {   // (diagnostico del preview NEGRO en la TV del N8: unas lineas al log, espaciadas)
        static int sLog = 0, sCuadro = 0;
        if (sLog < 12 && (sCuadro++ % 20) == 0) {
            sLog++;
            const size_t c = ((size_t)(g_rtVp->h / 2) * g_rtVp->w + g_rtVp->w / 2) * 4;
            const int e = (int)w3dEngine::ErrorGL();
            w3dLogf("[rt] %dx%d (viewport %dx%d) tex=%u %dx%d paso=%d/%d luces=%d centro=(%d,%d,%d) glErr=0x%x",
                    g_rtVp->w, g_rtVp->h, vp->width, vp->height, g_rtTexId, g_rtTexW, g_rtTexH, g_rtVp->paso, g_rt.pases,
                    (int)Lights.size(), c + 2 < g_rtVp->img.size() ? (int)g_rtVp->img[c] : -1,
                    c + 2 < g_rtVp->img.size() ? (int)g_rtVp->img[c+1] : -1, c + 2 < g_rtVp->img.size() ? (int)g_rtVp->img[c+2] : -1, e);
        }
    }
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
    const float su = (float)texW / (float)g_rtTexW, sv = (float)texH / (float)g_rtTexH;   // (la imagen trazada, estirada al viewport)
    const GLshort v[8] = { 0, 0, (GLshort)vp->width, 0, 0, (GLshort)vp->height, (GLshort)vp->width, (GLshort)vp->height };
    const GLfloat uv[8] = { 0, 0, su, 0, 0, sv, su, sv };
    W3dDrawStrip4(v, uv);
    gfx::BindTexture(0);
    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
    gfx::Enable(gfx::DepthTest);
}
bool RTRenderEstado(Viewport3D* vp) {
    if (!g_rt.on || !g_rtVp || g_rtVpDe != vp) return false;
    char b[160];
    // (en la vista local, adelante: "Local View - Ray Tracing: 3/16")
    char pre[64] = ""; if (vp->localViewActivo) snprintf(pre, sizeof(pre), "%s - ", T("Local View"));
    if (g_rtVp->Terminado()) snprintf(b, sizeof(b), "%s%s: %s (%d)", pre, T("Ray Tracing"), T("done"), g_rtVp->paso);
    else if (g_rtVp->nivel < kPrevNiveles) snprintf(b, sizeof(b), "%s%s: %s %d/%d", pre, T("Ray Tracing"), T("preview"), g_rtVp->nivel + 1, kPrevNiveles);
    else snprintf(b, sizeof(b), "%s%s: %d/%d", pre, T("Ray Tracing"), g_rtVp->paso, g_rt.pases);
    W3dVistaCartel(vp, b);
    return true;
}
void W3dVistaCartel(Viewport3D* vp, const char* b) {
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

// ---------------------------------------------------------------------------
//  DESENFOQUE DE MOVIMIENTO TRAZADO (g_rt.desenfoque): la escena en los cuadros f-1, f y f+1 (los vertices de las mallas
//  que se mueven y la camara) y, antes de cada pase, el instante de ese pase dentro del obturador interpolado entre
//  ellos. Las mallas son las mismas: solo se reajustan las cajas del BVH. Las luces quedan como en el cuadro f
// ---------------------------------------------------------------------------
const char* RTObturadorPosNombre(int pos) {
    return pos == 1 ? "Start of frame" : pos == 2 ? "End of frame" : "Center of frame";
}
struct RTMovimiento {
    std::vector<float> W[3];   // la matriz de mundo de cada malla viva (16 por malla) en los cuadros f-1, f, f+1
    RTCamara cam[3];
    float t0, t1;              // el obturador, en cuadros desde f
    int f;                     // el cuadro que se renderiza
};
static RTMovimiento g_rtMov;
// las poses de ESTE cuadro en las mallas vivas (el esqueleto / la vertex anim) y sus vertices de mundo en la escena
static void PosesAlCuadro(RTEscena& e) {
    for (size_t k = 0; k < e.vivas.size(); k++) {
        Mesh* m = e.vivas[k].m;
        if (m->skinArmature) SkinearMesh(m); else W3dPoseAlDia(m);
    }
    ActualizarVivas(e);
}
// (la pose cacheada de cada esqueleto ya no sirve: se evaluo en otro instante)
static void OlvidarPoses(RTEscena& e) {
    for (size_t k = 0; k < e.vivas.size(); k++) if (e.vivas[k].m->skinArmature) e.vivas[k].m->skinArmature->lastPoseFrame = -999999;
}
static void CamaraAlCuadro() { if (CameraActive) { CameraActive->UpdatePosition(); CameraActive->UpdateLookAt(); } }
static void MovimientoArmar(RTRender& r, Viewport3D* vp, int w, int h, RTMovimiento& mv) {
    extern int CurrentFrame; extern void AplicarAnimacionObjetos();
    float s = g_rt.obturador; if (s < 0.01f) s = 0.01f; if (s > 1.0f) s = 1.0f;
    if (g_rt.obturadorPos == 1)      { mv.t0 = 0.0f; mv.t1 = s; }
    else if (g_rt.obturadorPos == 2) { mv.t0 = -s;   mv.t1 = 0.0f; }
    else                             { mv.t0 = -s * 0.5f; mv.t1 = s * 0.5f; }
    const int f = CurrentFrame; mv.f = f;
    const size_t nv = r.esc.vivas.size();
    for (int k = 0; k < 3; k++) {
        CurrentFrame = f + k - 1; AplicarAnimacionObjetos(); CamaraAlCuadro();
        mv.W[k].resize(nv * 16);
        for (size_t i = 0; i < nv; i++) { Matrix4 M; r.esc.vivas[i].m->GetWorldMatrix(M); memcpy(&mv.W[k][i * 16], M.m, sizeof(M.m)); }
        CamaraDe(vp, w, h, mv.cam[k], false);
    }
    CurrentFrame = f; AplicarAnimacionObjetos(); CamaraAlCuadro();
    OlvidarPoses(r.esc); PosesAlCuadro(r.esc);
    w3dLogf("[rt] desenfoque: obturador %.2f..%.2f cuadros, %d mallas vivas", mv.t0, mv.t1, (int)nv);
}
static inline RTV Mezclar(const RTV& a, const RTV& b, float u) { return a + (b - a) * u; }
// la escena y la camara en el instante x (en cuadros desde f, -1..1): el ESQUELETO evaluado en ese instante (con las
// rotaciones interpoladas) y la matriz de cada malla y la camara entre los cuadros enteros de al lado
static void MovimientoEn(RTRender& r, const RTMovimiento& mv, float x) {
    extern int CurrentFrame;
    int fi = (int)floorf(x); if (fi < -1) fi = -1; if (fi > 0) fi = 0;   // (x = 1: el final del tramo 0..1)
    const float u = x - (float)fi;                                         // 0..1 entre f+fi y f+fi+1
    const int k0 = fi + 1, k1 = k0 + 1;
    const int cfAntes = CurrentFrame;
    CurrentFrame = mv.f + fi;
    g_poseFraccion = (u > 1e-4f && u < 1.0f) ? u : 0.0f;
    const int cfPose = (u >= 1.0f) ? mv.f + fi + 1 : mv.f + fi;
    CurrentFrame = cfPose;
    OlvidarPoses(r.esc);
    for (size_t i = 0; i < r.esc.vivas.size(); i++) {
        RTEscena::Viva& v = r.esc.vivas[i];
        Mesh* m = v.m;
        if (m->skinArmature) SkinearMesh(m); else W3dPoseAlDia(m);
        const bool skin = m->skinArmature && m->skinVertex;
        const GLfloat* P = skin ? m->skinVertex : m->vertex;
        if (!P || m->vertexSize != v.nV || mv.W[k0].size() < (i + 1) * 16) continue;
        const float* A = &mv.W[k0][i * 16]; const float* B = &mv.W[k1][i * 16];
        float M[16]; for (int c = 0; c < 16; c++) M[c] = A[c] + (B[c] - A[c]) * u;
        const GLbyte* N = (skin && m->skinNormals) ? m->skinNormals : m->normals;
        for (int j = 0; j < v.nV; j++) {
            const float px = P[j*3], py = P[j*3+1], pz = P[j*3+2];
            float* d = &r.esc.pos[(size_t)(v.base + j) * 3];
            d[0] = M[0]*px + M[4]*py + M[8]*pz + M[12]; d[1] = M[1]*px + M[5]*py + M[9]*pz + M[13]; d[2] = M[2]*px + M[6]*py + M[10]*pz + M[14];
            if (!N) continue;
            const float lx = N[j*3] / 127.0f, ly = N[j*3+1] / 127.0f, lz = N[j*3+2] / 127.0f;
            float nx = M[0]*lx + M[4]*ly + M[8]*lz, ny = M[1]*lx + M[5]*ly + M[9]*lz, nz = M[2]*lx + M[6]*ly + M[10]*lz;
            const float l = sqrtf(nx*nx + ny*ny + nz*nz); if (l > 1e-9f) { nx /= l; ny /= l; nz /= l; }
            float* q = &r.esc.nrm[(size_t)(v.base + j) * 3]; q[0] = nx; q[1] = ny; q[2] = nz;
        }
        v.pose = 0xFFFFFFFFu;   // (lo de ActualizarVivas ya no vale: la proxima vez rehace los vertices)
    }
    g_poseFraccion = 0.0f;
    CurrentFrame = cfAntes;
    BVHAjustar(r.esc);
    const float uc = (u > 1.0f) ? 1.0f : u;
    const RTCamara& c0 = mv.cam[k0], & c1 = mv.cam[k1];
    r.cam = c0;
    r.cam.pos = Mezclar(c0.pos, c1.pos, uc);
    r.cam.der = Norm(Mezclar(c0.der, c1.der, uc)); r.cam.arr = Norm(Mezclar(c0.arr, c1.arr, uc)); r.cam.adel = Norm(Mezclar(c0.adel, c1.adel, uc));
    if (r.gpu) {
        std::vector<float> vs, ns; VertsAGpu(r.esc, vs); NodosAGpu(r.esc, ns);
        if (!RTGpuActualizarGeometria(vs, ns)) { r.gpu = false; g_rtUltimoEnGpu = false; r.paso = 0; r.acum.assign(r.acum.size(), 0.0f); }
    }
}
// el gancho: el instante de ESTE pase (estratificado: cada pase en su tramo del obturador, con un corrimiento al azar
// dentro del tramo para que no queden copias marcadas)
static void PaseMovimiento(RTRender& r, void* ctx) {
    const RTMovimiento& mv = *(const RTMovimiento*)ctx;
    const int n = (g_rt.pases > 0) ? g_rt.pases : 1;
    unsigned int hsh = (unsigned int)r.paso * 2654435761u + r.semilla * 40503u + 12345u; hsh ^= hsh >> 15; hsh *= 2246822519u; hsh ^= hsh >> 13;
    const float jit = (float)(hsh & 0xFFFF) / 65536.0f;
    const float t = ((float)r.paso + jit) / (float)n;
    MovimientoEn(r, mv, mv.t0 + (mv.t1 - mv.t0) * t);
}

static bool RenderCompleto(Viewport3D* vp, int w, int h, int pases, bool progreso) {
    if (!vp || w <= 0 || h <= 0) return false;
    if (!g_rtOff) g_rtOff = new RTRender();
    const int pasesPrev = g_rt.pases; g_rt.pases = (pases > 0) ? pases : g_rt.pases;
    g_rtOff->Iniciar(vp, w, h, false);   // a archivo: sin preview, directo a resolucion completa
    if (g_rt.desenfoque && g_rt.obturador > 0.0f) {
        MovimientoArmar(*g_rtOff, vp, w, h, g_rtMov);
        g_rtOff->antesDelPase = PaseMovimiento; g_rtOff->antesCtx = &g_rtMov;
    }
    while (!g_rtOff->Terminado()) {
        g_rtOff->Avanzar(0.0);
        if (progreso) ProgresoActualizar((float)g_rtOff->paso / (float)g_rt.pases);
    }
    if (g_rtOff->antesDelPase) { OlvidarPoses(g_rtOff->esc); PosesAlCuadro(g_rtOff->esc); }   // (la pose del cuadro, otra vez)
    g_rtOff->antesDelPase = NULL; g_rtOff->antesCtx = NULL;
    g_rt.pases = pasesPrev;
    return true;
}
bool RTRenderizarAPNG(Viewport3D* vp, int w, int h, const std::string& ruta, std::string& msg) {
    if (!RenderCompleto(vp, w, h, 0, true)) { msg = "Ray Tracing: nothing to render"; return false; }
    if (!w3dEngine::SavePNG(ruta.c_str(), &g_rtOff->img[0], w, h, false)) { msg = "Ray Tracing: could not save"; return false; }
    msg.clear();
    return true;
}
bool RTRenderizarABuffer(Viewport3D* vp, int w, int h, std::vector<unsigned char>& rgba, std::string& msg) {
    if (!RenderCompleto(vp, w, h, 0, true)) { msg = "Ray Tracing: nothing to render"; return false; }
    rgba = g_rtOff->img;
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
bool RTHarnessFsrVista(int& w, int& h, unsigned char* rgb) {
    if (g_rtFsr.empty() || g_rtFsrW <= 0 || g_rtFsrH <= 0) return false;
    w = g_rtFsrW; h = g_rtFsrH;
    const unsigned char* p = &g_rtFsr[((size_t)(h / 2) * w + w / 2) * 4];
    rgb[0] = p[0]; rgb[1] = p[1]; rgb[2] = p[2];
    return true;
}
bool RTHarnessEnGpu() { return g_rtOff && g_rtOff->gpu; }
bool RTHarnessImagen(std::vector<unsigned char>& rgba, int& w, int& h) {
    if (!g_rtOff || g_rtOff->img.empty()) return false;
    rgba = g_rtOff->img; w = g_rtOff->w; h = g_rtOff->h;
    return true;
}
