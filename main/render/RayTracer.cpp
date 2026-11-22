// ============================================================================
//  RayTracer.cpp - trazado de rayos por CPU (ver RayTracer.h). Todo escrito de cero para Whisk3D.
//
//  Estructura (pensada para portar el nucleo a CUDA/RTX):
//    RTScene  = arrays planos: posiciones/normales/uv de mundo, triangulos (3 indices + material),
//                materiales (color difuso, emision, textura), texturas (pixeles CPU), luces, y un BVH
//                (cajas alineadas, particion por la mediana del eje mas largo, recorrido iterativo).
//    RTCamera  = la LENTE del viewport (Viewport3D::CurrentLens: la misma cuenta que ProjectPoint y que la proyeccion
//                de GL, con el marco de la camara y el zoom / pan de inspeccion), al reves: de un pixel a un rayo.
//    RTRender  = acumulador float RGB + imagen de 8 bits + tiles; Advance(presupuesto) hace tiles hasta
//                agotar el tiempo (o todos los del pase); con hilos, cada uno toma tiles de un contador.
// ============================================================================
#include "render/RayTracer.h"
#include "render/Denoiser.h"       // limpiar el ruido al terminar cada pase (opcional: OIDN / OptiX)
#include "render/RTGpu.h"          // el mismo trazado en la GPU (Vulkan, opcional)
#include "render/UpscalingFSR.h"   // el reescalado del viewport con FSR 1 (g_rt.upscaler)
#include "ui/ViewPorts/ViewPort3D.h"
#include "ui/ViewPorts/LayoutTree.h"   // aviso de destruccion de viewports (el del preview)
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Light.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Collection.h"      // SceneCollection
#include "objects/Camera.h"          // CameraActive (mirar por la camara)
#include "animation/SkeletalAnimation.h"   // SkinMesh: la pose de cada instante del obturador
#include "animation/VertexAnimation.h"     // W3dUpToDatePose
#include "objects/Armature.h"              // lastPoseFrame: la pose de otro instante no se cachea
#include "WhiskUI/theme/colors.h"   // SetColorID
#include "io/EditedTexture.h"      // los pixeles CPU de una textura (para muestrearla)
#include "render/RenderOptions.h"  // g_renderBg
#include "render/RealTimeShadows.h"   // g_shadows: el color de la sombra del sol (tambien se hornea)
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
#include <stdlib.h>   // getenv (W3D_RT_PROFILE)
#include <vector>
#include <set>      // la vista local (g_visibleLocalView)
#include <algorithm>
#if !defined(W3D_SYMBIAN) && !defined(__EMSCRIPTEN__)
    #define RT_THREADS 1
    #include <thread>
    #include <atomic>
#endif

extern bool g_redraw;
W3dRTOptions g_rt;

// ---------------------------------------------------------------------------
//  vectores chicos (float[3]) sin clases: lo que mas facil se traduce a un kernel
// ---------------------------------------------------------------------------
struct RTV { float x, y, z; };
static inline RTV V3(float x, float y, float z) { RTV v; v.x = x; v.y = y; v.z = z; return v; }
static inline RTV operator+(const RTV& a, const RTV& b) { return V3(a.x+b.x, a.y+b.y, a.z+b.z); }
static inline RTV operator-(const RTV& a, const RTV& b) { return V3(a.x-b.x, a.y-b.y, a.z-b.z); }
static inline RTV operator*(const RTV& a, float s) { return V3(a.x*s, a.y*s, a.z*s); }
static inline float Dot(const RTV& a, const RTV& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline RTV Cross(const RTV& a, const RTV& b) { return V3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x); }
static inline float Length(const RTV& a) { return sqrtf(Dot(a, a)); }
static inline RTV Norm(const RTV& a) { const float l = Length(a); return (l > 1e-12f) ? a * (1.0f / l) : V3(0, 0, 1); }

// xorshift32: rapido, sin estado global (uno por hilo/pixel)
struct RTRng { unsigned int s; RTRng(unsigned int seed) : s(seed ? seed : 0x9E3779B9u) {}
    inline unsigned int U() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float F() { return (float)(U() & 0xFFFFFF) / 16777216.0f; } };   // [0,1)

// ---------------------------------------------------------------------------
//  escena plana
// ---------------------------------------------------------------------------
// fl: LO QUE HACE EL OBJETO CON LA LUZ (Object::projectsShadow / bouncesLight / receivesShadow / catchesShadows). Un rayo de
// SOMBRA solo ve lo que PROYECTA; un rayo de REBOTE (la luz indirecta, el reflejo) solo lo que REBOTA: lo demas le pasa
// de largo, como si no estuviera. RECIBE: en su superficie las lamparas hacen sombra. Los rayos de la camara ven todo
enum { RT_PROJECTS = 1, RT_BOUNCES = 2, RT_RECEIVES = 4, RT_ALL = 7 };
struct RTTri { int v0, v1, v2; int mat; unsigned char fl; RTTri() : v0(0), v1(0), v2(0), mat(0), fl(RT_ALL) {} };
// transparente: alpha = alpha del color x alpha de la textura (mezcla 0). Con mezcla ADITIVA (2) el color se SUMA
// a lo de atras y no tapa nada. Lo transparente deja pasar la luz en proporcion: alpha 0 = no proyecta sombra.
// metalico 0..1: esa fraccion del color es REFLEJO de la escena (teñido por el color del material), el resto difuso;
// rugosidad 0..1 desparrama el reflejo (0 = espejo, 1 = mate) y ensancha el brillo de las lamparas.
// nmTex: el NORMAL MAP (RGB = normal en tangent-space: R = tangente, G = bitangente, B = normal; la misma
// convencion que el pase DOT3 del render GL), -1 = sin normal map.
struct RTMaterial { float diffuse[3]; float emission[3]; int tex; int nmTex; bool noLight; bool transparent; bool additive; float alpha; float roughness; float metallic;
                   int emTex, rugTex, metTex, alphaTex;   // los MAPAS del material (texturas; -1 = el numero)
                   float f0;                              // reflejo de frente del dielectrico (del IOR: ((n-1)/(n+1))^2)
                   float clip;                            // > 0: RECORTADO por alfa (como GL: el texel con alfa menor no esta)
                   float ior;                             // > 1: lo transparente REFRACTA (1.33 agua, 1.5 vidrio)
                   float trans; };                        // TRANSMISION 0..1 (Material::transmision): lo que es vidrio
// EL BORDE DEL RECORTADO: en vez de todo o nada en el umbral, una rampa corta alrededor (con el filtro bilineal el
// borde queda suave y los pases lo promedian: sin escalones de texel ni el serruchado del pelo y las hojas)
static inline float Crop(float a, float threshold) { a = (a - threshold) * 8.0f + 0.5f; return a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
struct RTTexture { const unsigned char* px; int w, h; bool filtered; };   // filtrado: bilineal (como la ve GL), si no el texel
// spotCos: el coseno del corte del cono (-1 = sin cono, luz puntual); spotDir: hacia donde alumbra; spotExpo: lo
// concentrado del haz (como GL: (cos)^expo adentro del cono)
struct RTLight { RTV pos; RTV dir; bool directional; float color[3]; float ambient[3]; float radius; int rays; float attC, attL, attQ;
               float spotCos, spotExpo; RTV spotDir; float distance;   // distancia: 0 = sin limite (Light::distancia)
               const void* source; };   // (la lampara de la escena: el horneado busca cual es el SOL)
struct RTNode { float bmin[3], bmax[3]; int left, right; int first, count; };   // hoja: cantidad > 0

struct RTScene {
    std::vector<float> pos, nrm, uv;      // por vertice (mundo)
    std::vector<float> tan;               // por vertice: tangente de mundo (xyz) + mano (w); solo con normal map (sino 0)
    std::vector<RTTri> tris;
    std::vector<RTMaterial> mats;
    std::vector<RTTexture> texs;
    std::vector<RTLight> lights;
    std::vector<RTNode> nodes;
    std::vector<int> order;               // indices de triangulos, en el orden de las hojas
    float bg[3];
    int   bgTex;             // el FONDO como textura 360 (equirectangular): su lugar en 'texs' (-1 = el color 'bg')
    float lightBackground;   // LA LUZ DEL FONDO (g_renderLightBackground): cuanto ilumina (lo que se ve del fondo no cambia)
    float bgRot[9];          // la rotacion del fondo (mundo -> imagen, ver W3dBackgroundMatrix)
    bool hasTransparent;                // algun material transparente: los rayos de sombra atraviesan (sin any-hit)
    // LAS MALLAS (sin modificadores): donde quedaron sus vertices y con que matriz / pose. Cuando solo se MUEVEN (la
    // pose de un esqueleto, o la posicion: el publico que salta) se renuevan sus vertices y se reajustan las cajas del
    // BVH (UpdatePosesYLights) en vez de armar todo de nuevo
    struct Live { Mesh* m; int base, nV; float W[16]; unsigned pose; };
    std::vector<Live> live;
    void Clear() { pos.clear(); nrm.clear(); uv.clear(); tan.clear(); tris.clear(); mats.clear(); texs.clear(); lights.clear(); nodes.clear(); order.clear(); live.clear(); hasTransparent = false; }
};

// ---- BVH: mediana del eje mas largo, hojas de hasta 4 triangulos ----
static void BVHBox(const RTScene& e, int tri, float* bmin, float* bmax) {
    const RTTri& t = e.tris[(size_t)tri];
    const int vs[3] = { t.v0, t.v1, t.v2 };
    for (int a = 0; a < 3; a++) { bmin[a] = 1e30f; bmax[a] = -1e30f; }
    for (int k = 0; k < 3; k++) for (int a = 0; a < 3; a++) {
        const float c = e.pos[(size_t)vs[k]*3 + a];
        if (c < bmin[a]) bmin[a] = c; if (c > bmax[a]) bmax[a] = c;
    }
}
struct BVHOrderAxis { const std::vector<float>* center; int axis; bool operator()(int a, int b) const { return (*center)[(size_t)a*3 + axis] < (*center)[(size_t)b*3 + axis]; } };
static int BVHBuildNode(RTScene& e, std::vector<float>& center, int first, int count) {
    RTNode n; n.left = n.right = -1; n.first = first; n.count = 0;
    for (int a = 0; a < 3; a++) { n.bmin[a] = 1e30f; n.bmax[a] = -1e30f; }
    for (int i = 0; i < count; i++) {
        float bmin[3], bmax[3]; BVHBox(e, e.order[(size_t)(first + i)], bmin, bmax);
        for (int a = 0; a < 3; a++) { if (bmin[a] < n.bmin[a]) n.bmin[a] = bmin[a]; if (bmax[a] > n.bmax[a]) n.bmax[a] = bmax[a]; }
    }
    const int idx = (int)e.nodes.size();
    e.nodes.push_back(n);
    if (count <= 4) { e.nodes[(size_t)idx].count = count; return idx; }
    int axis = 0; float ext = n.bmax[0] - n.bmin[0];
    for (int a = 1; a < 3; a++) if (n.bmax[a] - n.bmin[a] > ext) { ext = n.bmax[a] - n.bmin[a]; axis = a; }
    BVHOrderAxis cmp; cmp.center = &center; cmp.axis = axis;
    std::sort(e.order.begin() + first, e.order.begin() + first + count, cmp);
    const int half = count / 2;
    const int left  = BVHBuildNode(e, center, first, half);
    const int right = BVHBuildNode(e, center, first + half, count - half);
    e.nodes[(size_t)idx].left = left; e.nodes[(size_t)idx].right = right;
    return idx;
}
// REAJUSTAR el BVH (los vertices se movieron, los triangulos son los mismos): cada caja de nuevo, de las hojas para
// arriba (los hijos siempre estan despues que su padre: BVHBuildNode agrega el padre antes de recursar). Mucho mas
// rapido que construir; con la banda moviendose en su lugar las cajas casi no crecen
static void BVHFit(RTScene& e) {
    for (int i = (int)e.nodes.size() - 1; i >= 0; i--) {
        RTNode& n = e.nodes[(size_t)i];
        for (int a = 0; a < 3; a++) { n.bmin[a] = 1e30f; n.bmax[a] = -1e30f; }
        if (n.count > 0) {
            for (int k = 0; k < n.count; k++) {
                float bmin[3], bmax[3]; BVHBox(e, e.order[(size_t)(n.first + k)], bmin, bmax);
                for (int a = 0; a < 3; a++) { if (bmin[a] < n.bmin[a]) n.bmin[a] = bmin[a]; if (bmax[a] > n.bmax[a]) n.bmax[a] = bmax[a]; }
            }
        } else {
            const int hs[2] = { n.left, n.right };
            for (int k = 0; k < 2; k++) {
                if (hs[k] < 0) continue;
                const RTNode& h = e.nodes[(size_t)hs[k]];
                for (int a = 0; a < 3; a++) { if (h.bmin[a] < n.bmin[a]) n.bmin[a] = h.bmin[a]; if (h.bmax[a] > n.bmax[a]) n.bmax[a] = h.bmax[a]; }
            }
        }
    }
}
// las POSES de ahora de las mallas con esqueleto: sus vertices (y normales) de mundo, en el mismo lugar del arreglo
static void UpdateLive(RTScene& e) {
    for (size_t k = 0; k < e.live.size(); k++) {
        RTScene::Live& v = e.live[k];
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
static void BVHBuild(RTScene& e) {
    e.nodes.clear(); e.order.clear();
    const int n = (int)e.tris.size();
    if (n == 0) return;
    std::vector<float> center((size_t)n * 3);
    e.order.resize((size_t)n);
    for (int i = 0; i < n; i++) {
        e.order[(size_t)i] = i;
        const RTTri& t = e.tris[(size_t)i];
        for (int a = 0; a < 3; a++) center[(size_t)i*3 + a] = (e.pos[(size_t)t.v0*3 + a] + e.pos[(size_t)t.v1*3 + a] + e.pos[(size_t)t.v2*3 + a]) / 3.0f;
    }
    e.nodes.reserve((size_t)n * 2);
    BVHBuildNode(e, center, 0, n);
}

// ---- interseccion ----
struct RTHit { float t; float b1, b2; int tri; };
static inline bool RayTri(const RTScene& e, int tri, const RTV& o, const RTV& d, float tMin, float tMax, RTHit& h) {
    const RTTri& T = e.tris[(size_t)tri];
    const RTV p0 = V3(e.pos[(size_t)T.v0*3], e.pos[(size_t)T.v0*3+1], e.pos[(size_t)T.v0*3+2]);
    const RTV p1 = V3(e.pos[(size_t)T.v1*3], e.pos[(size_t)T.v1*3+1], e.pos[(size_t)T.v1*3+2]);
    const RTV p2 = V3(e.pos[(size_t)T.v2*3], e.pos[(size_t)T.v2*3+1], e.pos[(size_t)T.v2*3+2]);
    const RTV e1 = p1 - p0, e2 = p2 - p0;
    const RTV pv = Cross(d, e2);
    const float det = Dot(e1, pv);
    if (det > -1e-9f && det < 1e-9f) return false;
    const float inv = 1.0f / det;
    const RTV tv = o - p0;
    const float b1 = Dot(tv, pv) * inv; if (b1 < 0.0f || b1 > 1.0f) return false;
    const RTV qv = Cross(tv, e1);
    const float b2 = Dot(d, qv) * inv; if (b2 < 0.0f || b1 + b2 > 1.0f) return false;
    const float t = Dot(e2, qv) * inv;
    if (t <= tMin || t >= tMax) return false;
    h.t = t; h.b1 = b1; h.b2 = b2; h.tri = tri;
    return true;
}
static inline bool RayBox(const float* bmin, const float* bmax, const RTV& o, const RTV& invD, float tMax) {
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
static bool Trace(const RTScene& e, const RTV& o, const RTV& d, float tMin, float tMax, bool shadow, RTHit& best, int excluded = -1,
                   unsigned char mask = 0) {
    if (e.nodes.empty()) return false;
    const RTV invD = V3(1.0f / (fabsf(d.x) > 1e-12f ? d.x : 1e-12f), 1.0f / (fabsf(d.y) > 1e-12f ? d.y : 1e-12f), 1.0f / (fabsf(d.z) > 1e-12f ? d.z : 1e-12f));
    int stack[64]; int sp = 0; stack[sp++] = 0;
    bool has = false; best.t = tMax;
    while (sp > 0) {
        const RTNode& n = e.nodes[(size_t)stack[--sp]];
        if (!RayBox(n.bmin, n.bmax, o, invD, best.t)) continue;
        if (n.count > 0) {
            for (int i = 0; i < n.count; i++) {
                const int tri = e.order[(size_t)(n.first + i)];
                if (tri == excluded) continue;
                if ((e.tris[(size_t)tri].fl & mask) != mask) continue;   // (le pasa de largo: ver RTTri::fl)
                RTHit h;
                if (RayTri(e, tri, o, d, tMin, best.t, h)) { best = h; has = true; if (shadow) return true; }
            }
        } else {
            if (n.left >= 0 && sp < 63) stack[sp++] = n.left;
            if (n.right >= 0 && sp < 63) stack[sp++] = n.right;
        }
    }
    return has;
}

// ---------------------------------------------------------------------------
//  armado de la escena desde los objetos del editor
// ---------------------------------------------------------------------------
static int MaterialIdx(RTScene& e, Material* mt, std::vector<Material*>& seen) {
    for (size_t i = 0; i < seen.size(); i++) if (seen[i] == mt) return (int)i;
    RTMaterial rm;
    rm.diffuse[0] = mt ? mt->diffuse[0] : 0.8f; rm.diffuse[1] = mt ? mt->diffuse[1] : 0.8f; rm.diffuse[2] = mt ? mt->diffuse[2] : 0.8f;
    rm.emission[0] = mt ? mt->emission[0] : 0.0f; rm.emission[1] = mt ? mt->emission[1] : 0.0f; rm.emission[2] = mt ? mt->emission[2] : 0.0f;
    rm.tex = -1; rm.nmTex = -1;
    rm.emTex = rm.rugTex = rm.metTex = rm.alphaTex = -1;
    // el reflejo de frente (f0) sale del IOR; con IOR 1 (sin refraccion) el de un dielectrico comun (0.04)
    { const float n = mt ? (mt->ior < 1.0f ? 1.0f : mt->ior) : 1.0f; const float q = (n - 1.0f) / (n + 1.0f); rm.f0 = (n > 1.0001f) ? q * q : ((mt && mt->transmission > 0.0f) ? 0.0f : 0.04f); rm.ior = n; }   // (con transmision, IOR 1 = sin reflejo: transparente perfecto)
    // los MAPAS en modo textura (emision, rugosidad, metalico, alfa): sus pixeles CPU, como la base
    if (mt) {
        int* dst[W3D_MAPS] = { 0, 0, &rm.emTex, &rm.rugTex, &rm.metTex, &rm.alphaTex };
        for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) {
            if (!mt->mapWithTexture[k] || !mt->mapTex[k] || !mt->mapTex[k]->iID) continue;
            TextureEditable* tm = TexEditGet(mt->mapTex[k]);
            if (tm && tm->w > 0 && tm->h > 0) { RTTexture tx; tx.px = &tm->rgba[0]; tx.w = tm->w; tx.h = tm->h; tx.filtered = mt->filtered; e.texs.push_back(tx); *dst[k] = (int)e.texs.size() - 1; }
        }
        if (rm.alphaTex >= 0) e.hasTransparent = true;
    }
    rm.noLight = mt ? !mt->lighting : false;
    if (mt && mt->normalMap && mt->normalTexture) {
        TextureEditable* tn = TexEditGet(mt->normalTexture);
        if (tn && tn->w > 0 && tn->h > 0) { RTTexture tx; tx.px = &tn->rgba[0]; tx.w = tn->w; tx.h = tn->h; tx.filtered = mt->filtered; e.texs.push_back(tx); rm.nmTex = (int)e.texs.size() - 1; }
    }
    rm.roughness = mt ? mt->rtRoughness : 0.5f; if (rm.roughness < 0.0f) rm.roughness = 0.0f; if (rm.roughness > 1.0f) rm.roughness = 1.0f;
    rm.metallic = mt ? mt->rtMetallic : 0.0f; if (rm.metallic < 0.0f) rm.metallic = 0.0f; if (rm.metallic > 1.0f) rm.metallic = 1.0f;
    rm.transparent = mt ? mt->transparent : false;
    rm.additive = rm.transparent && mt && mt->mix == 2;
    rm.alpha = mt ? mt->diffuse[3] : 1.0f; if (rm.alpha < 0.0f) rm.alpha = 0.0f; if (rm.alpha > 1.0f) rm.alpha = 1.0f;
    // RECORTADO por alfa (las hojas, las rejas, el publico): lo transparente de su textura deja pasar el rayo
    rm.clip = 0.0f;
    // (tambien con mezcla: el pelo va mezclado Y recortado; sin el recorte lo casi transparente de cada mechon contaba)
    if (mt && mt->alphaTest > 0.0f && !rm.additive) { if (!rm.transparent) { rm.transparent = true; rm.alpha = 1.0f; } rm.clip = mt->alphaTest; }
    // TRANSMISION: la parte que es vidrio deja pasar la luz doblada por el IOR (las sombras tambien pasan)
    rm.trans = (mt && !rm.additive) ? mt->transmission : 0.0f; if (rm.trans < 0.0f) rm.trans = 0.0f; if (rm.trans > 1.0f) rm.trans = 1.0f;
    if (rm.transparent || rm.trans > 0.0f) e.hasTransparent = true;
    if (mt && mt->textureOn && mt->texture && mt->texture->iID) {
        TextureEditable* te = TexEditGet(mt->texture);   // los pixeles CPU (se decodifican una vez)
        if (te && te->w > 0 && te->h > 0) { RTTexture tx; tx.px = &te->rgba[0]; tx.w = te->w; tx.h = te->h; tx.filtered = mt->filtered; e.texs.push_back(tx); rm.tex = (int)e.texs.size() - 1; }
    }
    e.mats.push_back(rm); seen.push_back(mt);
    return (int)e.mats.size() - 1;
}
// TANGENTES por vertice (mundo) para los vertices [v0, v1) desde los triangulos [t0, t1) ya cargados: la
// direccion de U sobre la superficie, perpendicular a la normal (Gram-Schmidt) + la mano (w) para la bitangente.
// Misma cuenta que Mesh::ComputeTangents, pero sobre los arrays de MUNDO del trazador (sirve para la malla
// generada por modificadores y la deformada por esqueleto, que no tienen tangentes propias).
static void TangentsOf(RTScene& e, int v0, int v1, size_t t0, size_t t1) {
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
        const float hand = (Dot(Cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;
        e.tan[(size_t)v*4] = T.x; e.tan[(size_t)v*4+1] = T.y; e.tan[(size_t)v*4+2] = T.z; e.tan[(size_t)v*4+3] = hand;
    }
}
static void AddMeshEnd(RTScene& e, int base, int nV, size_t trisPrev);
static void AddMesh(RTScene& e, Mesh* m, std::vector<Material*>& seen) {
    // la POSE del cuadro actual (cacheada: si GL ya la dibujo no cuesta nada). Sin esto, el render a archivo despues de
    // cambiar de cuadro sin que GL dibujara tomaba los vertices skineados de OTRO cuadro
    if (m->skinArmature) SkinMesh(m); else W3dUpToDatePose(m);
    Matrix4 W; m->GetWorldMatrix(W);
    const size_t trisPrev = e.tris.size();
    const bool gen = m->genValid && m->genVertex && m->genFaces && m->genVertexSize > 0 && m->genFacesSize > 0;
    const GLfloat* P = gen ? m->genVertex : ((m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex);
    const bool skin = !gen && m->skinArmature && m->skinVertex;
    const GLbyte*  N = gen ? m->genNormals : ((skin && m->skinNormals) ? m->skinNormals : m->normals);   // (deformadas si las hay)
    const GLfloat* UV = gen ? m->genUV : m->uv;
    const int nV = gen ? m->genVertexSize : m->vertexSize;
    if (!P || nV <= 0) return;
    const int base = (int)(e.pos.size() / 3);
    if (!gen) { RTScene::Live v; v.m = m; v.base = base; v.nV = nV; memcpy(v.W, W.m, sizeof(v.W)); v.pose = skin ? m->skinPoseSerial : 0u; e.live.push_back(v); }
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
            const int mi = MaterialIdx(e, mtG, seen);
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
            const int mi = MaterialIdx(e, G[g].material, seen);
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
            const int mi = MaterialIdx(e, mt, seen);
            for (int t = 1; t + 1 < n; t++) {
                const int a = F.idx[0], b = F.idx[(size_t)t], c = F.idx[(size_t)t + 1];
                if (a < 0 || b < 0 || c < 0 || a >= nV || b >= nV || c >= nV) continue;
                RTTri tr; tr.v0 = base + a; tr.v1 = base + b; tr.v2 = base + c; tr.mat = mi;
                e.tris.push_back(tr);
            }
        }
    }
    // lo que hace con la luz (ver RTTri::fl): un CAPTADOR DE SOMBRAS solo las recibe (no tapa ni rebota)
    {
        unsigned char fl = 0;
        if (m->projectsShadow && !m->catchesShadows) fl |= RT_PROJECTS;
        if (m->bouncesLight && !m->catchesShadows) fl |= RT_BOUNCES;
        if (m->receivesShadow) fl |= RT_RECEIVES;
        for (size_t t = trisPrev; t < e.tris.size(); t++) e.tris[t].fl = fl;
    }
    AddMeshEnd(e, base, nV, trisPrev);
}
// (cierre de AddMesh: tangentes si hace falta) -> ver AddMeshEnd
static void AddMeshEnd(RTScene& e, int base, int nV, size_t trisPrev) {
    bool withNM = false;
    for (size_t t = trisPrev; t < e.tris.size() && !withNM; t++) withNM = (e.mats[(size_t)e.tris[t].mat].nmTex >= 0);
    if (withNM) TangentsOf(e, base, base + nV, trisPrev, e.tris.size());
    else if (e.tan.size() < e.pos.size() / 3 * 4) e.tan.resize(e.pos.size() / 3 * 4, 0.0f);
}
extern bool g_localViewActive;                          // VISTA LOCAL ("/"): solo se traza lo aislado (Objects.cpp)
extern const std::set<Object*>* g_visibleLocalView;
// EL HORNEADO DE LAS SONDAS DE LUZ (RTBakeProbes) arma la escena solo con lo ESTATICO (Object::estatico): lo que se
// mueve -un personaje, una pelota- no tapa ni rebota luz en la sonda, solo la recibe (como el "static" de Unity)
static bool g_rtOnlyStatic = false;
// EL HORNEADO DE UNA SONDA (de luz o de reflejo): lo que no sale en las sondas (Object::inProbes) no entra en la escena
static bool g_rtForProbe = false;
// SIN SOMBRAS: la luz directa del punto que se esta sombreando no prueba si algo tapa las lamparas (un objeto que no
// recibe sombras; el horneado de uno que no las recibe)
static bool g_rtNoShadows = false;
// EL SOL APARTE (el horneado de la luz, RTBakeStep): la luz difusa directa de la lampara g_rtLightSun (un indice de
// e.lights; -1 = nada) se suma ademas en g_rtSunLight
static int g_rtLightSun = -1;
static float g_rtSunLight[3] = { 0, 0, 0 };
// LA SOMBRA DEL SOL CON SU COLOR: donde algo tapa al sol llega igual esta parte de su luz (el color de las sombras en
// tiempo real por su intensidad). Asi la sombra horneada tiene el mismo tono que la de lo que se mueve sobre ella
static float g_rtShadowColorSun[3] = { 0, 0, 0 };
static void TraverseScene(RTScene& e, Object* o, std::vector<Material*>& seen) {
    if (!o) return;
    if (o != (Object*)SceneCollection && (!o->visible || !o->renderable)) return;
    if (o != (Object*)SceneCollection && W3dViewLocalHidden(o)) return;
    if (o->getType() == ObjectType::mesh && (o == (Object*)SceneCollection || !W3dViewLocalOnlyStep(o)) &&
        !(g_rtOnlyStatic && !o->isStatic) && !(g_rtForProbe && !o->inProbes)) AddMesh(e, (Mesh*)o, seen);
    for (size_t i = 0; i < o->Childrens.size(); i++) TraverseScene(e, o->Childrens[i], seen);
}
static void AddLights(RTScene& e) {
    for (size_t i = 0; i < Lights.size(); i++) {
        Light* l = Lights[i]; if (!l || !l->visible) continue;
        RTLight L; Matrix4 W; l->GetWorldMatrix(W);
        L.pos = V3(W.m[12], W.m[13], W.m[14]);
        L.dir = Norm(V3(W.m[4], W.m[5], W.m[6]));   // el +Y local de la lampara apunta HACIA la luz (como GL_POSITION w=0)
        L.directional = l->directional;
        for (int c = 0; c < 3; c++) { L.color[c] = l->diffuse[c] * (l->intensity > 0.0f ? l->intensity : 0.0f); L.ambient[c] = l->ambient[c]; }
        L.radius = l->rtRadius;
        L.rays   = (int)(l->rtRays + 0.5f); if (L.rays < 1) L.rays = g_rt.rays;   // 0 = el global de la pestania Render
        if (L.rays < 1) L.rays = 1; if (L.rays > 256) L.rays = 256;
        L.attC = l->attConstant; L.attL = l->attLinear; L.attQ = l->attQuadratic;
        L.distance = (l->directional || l->distance <= 0.0f) ? 0.0f : l->distance;
        // FOCO (spotCutoff < 180): alumbra hacia su -Z local, como el de GL
        L.spotCos = (!l->directional && l->spotCutoff < 179.9f) ? cosf(l->spotCutoff * 3.14159265f / 180.0f) : -1.0f;
        L.spotExpo = l->spotExponent;
        L.spotDir = Norm(V3(-W.m[8], -W.m[9], -W.m[10]));
        L.source = l;
        e.lights.push_back(L);
    }
}
static void BuildRTScene(RTScene& e) {
    e.Clear();
    std::vector<Material*> seen;
    TraverseScene(e, SceneCollection, seen);
    AddLights(e);
    e.bg[0] = g_renderBg[0]; e.bg[1] = g_renderBg[1]; e.bg[2] = g_renderBg[2];
    e.lightBackground = g_renderLightBackground < 0.0f ? 0.0f : g_renderLightBackground;
    // el FONDO 360: sus pixeles CPU (los de la textura) y su rotacion
    e.bgTex = -1; W3dBackgroundMatrix(e.bgRot);
    if (W3dBackgroundWithTexture()) {
        TextureEditable* tf = TexEditGet(g_renderBgTex);
        if (tf && tf->w > 0 && tf->h > 0) { RTTexture tx; tx.px = &tf->rgba[0]; tx.w = tf->w; tx.h = tf->h; tx.filtered = true; e.texs.push_back(tx); e.bgTex = (int)e.texs.size() - 1; }
    }
    BVHBuild(e);
}

// ---------------------------------------------------------------------------
//  LAS SONDAS DE LUZ (ver RayTracer.h): rayos repartidos parejo en la esfera (la espiral de Fibonacci) desde cada punto;
//  lo que trae cada uno es la luz que llega desde esa direccion. La profundidad 1 cuenta como un rebote: el fondo va
//  por su factor de luz (g_renderLightBackground), como la luz que rebota en el render
// ---------------------------------------------------------------------------
static void Radiance(const RTScene& e, const RTV& o0, const RTV& d0, RTRng& rng, int depth, float* out);
// HORNEAR UNA SONDA DE A PEDAZOS (ver RayTracer.h): la escena se arma UNA vez al empezar y RTProbeStep avanza unos
// puntos (la de luz) o unas filas de la imagen 360 (la de reflejo) por cuadro, asi el editor muestra la barra
struct RTJobProbe {
    int type;                              // 0 = nada, 1 = sonda de luz, 2 = sonda de reflejo
    bool ready;                            // termino: espera que lo tomen (RTSondaTomar*)
    RTScene e;
    RTRng rng;
    // la de LUZ
    std::vector<float> pos, sh;
    std::vector<unsigned char> ok;
    int rays, next, n;
    // la de REFLEJO
    float o[3], near, intensity;
    int w, h, ns, bounces, row;
    std::vector<float> img;
    RTJobProbe() : type(0), ready(false), rng(12345u), rays(0), next(0), n(0), near(0), intensity(1), w(0), h(0), ns(1),
                       bounces(0), row(0) { o[0] = o[1] = o[2] = 0.0f; }
};
static RTJobProbe gProbe;
static void ProbeBuildScene() {
    gProbe.e.Clear();
    g_rtOnlyStatic = true;    // (solo lo estatico ilumina la sonda / sale en el reflejo, como Unity)
    g_rtForProbe = true;
    BuildRTScene(gProbe.e);
    g_rtOnlyStatic = false; g_rtForProbe = false;
}
bool RTProbeActive() { return gProbe.type != 0 && !gProbe.ready; }
int  RTProbeType() { return gProbe.type; }
void RTProbeCancel() {
    gProbe.type = 0; gProbe.ready = false;
    gProbe.e.Clear();
    std::vector<float>().swap(gProbe.pos); std::vector<float>().swap(gProbe.sh); std::vector<float>().swap(gProbe.img);
    std::vector<unsigned char>().swap(gProbe.ok);
}

bool RTProbeBeginLight(const std::vector<float>& pos, int rays) {
    const int n = (int)(pos.size() / 3);
    if (n <= 0) return false;
    RTProbeCancel();
    gProbe.type = 1; gProbe.ready = false;
    gProbe.pos = pos; gProbe.n = n; gProbe.next = 0;
    gProbe.rays = rays < 16 ? 16 : rays;
    gProbe.rng = RTRng(12345u);
    ProbeBuildScene();
    w3dLogf("[sonda] hornear: %d puntos x %d rayos (la escena: %d triangulos, %d luces)", n, gProbe.rays, (int)gProbe.e.tris.size(), (int)gProbe.e.lights.size());
    gProbe.sh.assign((size_t)n * 27, 0.0f);
    gProbe.ok.assign((size_t)n, 1);
    return true;
}
static void LightProbePoint(int pt) {
    const RTScene& e = gProbe.e;
    const int rays = gProbe.rays;
    static const float kA[9] = { 1.0f, 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
    const float w = 4.0f * 3.14159265f / (float)rays;   // (el angulo solido de cada rayo)
    const std::vector<float>& pos = gProbe.pos;
    const RTV o = V3(pos[(size_t)pt * 3], pos[(size_t)pt * 3 + 1], pos[(size_t)pt * 3 + 2]);
    double c[9][3]; for (int i = 0; i < 9; i++) c[i][0] = c[i][1] = c[i][2] = 0.0;
    int backs = 0;
    for (int r = 0; r < rays; r++) {
        const float z = 1.0f - 2.0f * ((float)r + 0.5f) / (float)rays;
        const float rr = sqrtf(z < 1.0f ? 1.0f - z * z : 0.0f), ph = (float)r * 2.39996323f;
        const float dx = rr * cosf(ph), dy = rr * sinf(ph), dz = z;
        const RTV d = V3(dx, dy, dz);
        // ADENTRO DE ALGO: el rayo pega en una cara DE ESPALDAS (sale de la superficie desde adentro). Un punto con
        // muchos asi esta adentro de una malla (o debajo del piso): su luz no vale (la rellenan los vecinos)
        RTHit h;
        if (Trace(e, o, d, 1e-4f, 1e30f, false, h, -1)) {
            const RTTri& T = e.tris[(size_t)h.tri];
            const float b0 = 1.0f - h.b1 - h.b2;
            const float nx = e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2;
            const float ny = e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2;
            const float nz = e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2;
            if (nx * dx + ny * dy + nz * dz > 0.0f) backs++;
        }
        float L[3]; Radiance(e, o, d, gProbe.rng, 1, L);
        const float Y[9] = { 0.282095f, 0.488603f * dy, 0.488603f * dz, 0.488603f * dx, 1.092548f * dx * dy,
                             1.092548f * dy * dz, 0.315392f * (3.0f * dz * dz - 1.0f), 1.092548f * dx * dz,
                             0.546274f * (dx * dx - dy * dy) };
        for (int i = 0; i < 9; i++) for (int k = 0; k < 3; k++) c[i][k] += (double)(L[k] * Y[i] * w);
    }
    for (int i = 0; i < 9; i++) for (int k = 0; k < 3; k++) gProbe.sh[(size_t)pt * 27 + i * 3 + k] = (float)c[i][k] * kA[i];
    if (backs > rays / 5) gProbe.ok[(size_t)pt] = 0;
}

bool RTProbeBeginReflection(const float pos[3], int w, int h, float near, int samples, int bounces, float intensity,
                           std::string* reason) {
    if (w < 2 || h < 1) { if (reason) *reason = "Invalid size"; return false; }
    RTProbeCancel();
    gProbe.type = 2; gProbe.ready = false;
    for (int k = 0; k < 3; k++) gProbe.o[k] = pos[k];
    gProbe.w = w; gProbe.h = h; gProbe.near = near; gProbe.intensity = intensity; gProbe.row = 0;
    // CON LA LUZ QUE REBOTA, como la luz precalculada: con los rebotes del render (0 en muchas escenas) la cara de algo
    // que no ve el sol salia NEGRA en el reflejo (en la escena se ve clara: su textura horneada tiene los rebotes). Los
    // REBOTES y las MUESTRAS por pixel son de la sonda (de fabrica 3 y 32: con 4 la penumbra de un sol con radio salia
    // granulada)
    gProbe.bounces = bounces < 0 ? 0 : (bounces > 8 ? 8 : bounces);
    gProbe.ns = samples < 1 ? 1 : (samples > 1024 ? 1024 : samples);
    gProbe.rng = RTRng(7919u);
    gProbe.img.assign((size_t)w * h * 3, 0.0f);
    ProbeBuildScene();
    return true;
}
static void ReflectionProbeRow(int y) {
    const int w = gProbe.w, h = gProbe.h, ns = gProbe.ns;
    const RTV o = V3(gProbe.o[0], gProbe.o[1], gProbe.o[2]);
    const float near = gProbe.near;
    RTRng& rng = gProbe.rng;
    for (int x = 0; x < w; x++) {
        float acc[3] = { 0, 0, 0 };
        for (int s = 0; s < ns; s++) {
            const float jx = (ns == 1) ? 0.5f : (ns == 4) ? ((s & 1) ? 0.75f : 0.25f) : rng.F();
            const float jy = (ns == 1) ? 0.5f : (ns == 4) ? ((s & 2) ? 0.75f : 0.25f) : rng.F();
            const float u = ((float)x + jx) / (float)w, v = ((float)y + jy) / (float)h;
            const float fi = (u - 0.5f) * 6.2831853f, te = v * 3.14159265f;
            const RTV d = V3(sinf(te) * cosf(fi), cosf(te), sinf(te) * sinf(fi));
            float L[3]; Radiance(gProbe.e, o + d * (near > 0.0f ? near : 0.0f), d, rng, 0, L);
            acc[0] += L[0]; acc[1] += L[1]; acc[2] += L[2];
        }
        for (int c = 0; c < 3; c++) gProbe.img[((size_t)y * w + x) * 3 + c] = acc[c] / (float)ns * gProbe.intensity;
    }
}

float RTProbeStep(int msBudget) {
    if (gProbe.type == 0) return 0.0f;
    if (gProbe.ready) return 1.0f;
    extern unsigned int w3dGetTicks();
    const unsigned int t0 = w3dGetTicks();
    if (gProbe.type == 1) {
        while (gProbe.next < gProbe.n) {
            LightProbePoint(gProbe.next++);
            if ((int)(w3dGetTicks() - t0) >= msBudget) break;
        }
        if (gProbe.next >= gProbe.n) { gProbe.ready = true; gProbe.e.Clear(); }
        return gProbe.n > 0 ? (float)gProbe.next / (float)gProbe.n : 1.0f;
    }
    const int rebPrev = g_rt.bounces;
    g_rt.bounces = gProbe.bounces;
    while (gProbe.row < gProbe.h) {
        ReflectionProbeRow(gProbe.row++);
        if ((int)(w3dGetTicks() - t0) >= msBudget) break;
    }
    g_rt.bounces = rebPrev;
    if (gProbe.row >= gProbe.h) { gProbe.ready = true; gProbe.e.Clear(); }
    return gProbe.h > 0 ? (float)gProbe.row / (float)gProbe.h : 1.0f;
}

bool RTProbeTakeLight(std::vector<float>& sh, std::vector<unsigned char>* valid) {
    if (gProbe.type != 1 || !gProbe.ready) return false;
    sh.swap(gProbe.sh);
    if (valid) valid->swap(gProbe.ok);
    RTProbeCancel();
    return true;
}

bool RTProbeTakeReflection(std::vector<unsigned char>& rgba) {
    if (gProbe.type != 2 || !gProbe.ready) return false;
    const int w = gProbe.w, h = gProbe.h, ns = gProbe.ns;
    const std::vector<float>& img = gProbe.img;
    rgba.assign((size_t)w * h * 4, 255);
    // EL GRANULADO de los rebotes: un promedio con los vecinos de COLOR PARECIDO (5 x 5, da la vuelta a lo ancho; los
    // bordes de un objeto contra la pared quedan netos)
    std::vector<float> out(img);
    if (ns > 1)
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const float* c0 = &img[((size_t)y * w + x) * 3];
                float sm[3] = { 0, 0, 0 }, weight = 0.0f;
                for (int dy = -2; dy <= 2; dy++) {
                    const int yy = y + dy; if (yy < 0 || yy >= h) continue;
                    for (int dx = -2; dx <= 2; dx++) {
                        const int xx = (x + dx + w) % w;
                        const float* c1 = &img[((size_t)yy * w + xx) * 3];
                        const float d0 = c1[0] - c0[0], d1 = c1[1] - c0[1], d2 = c1[2] - c0[2];
                        const float wgt = expf(-(d0 * d0 + d1 * d1 + d2 * d2) * 30.0f);
                        sm[0] += c1[0] * wgt; sm[1] += c1[1] * wgt; sm[2] += c1[2] * wgt; weight += wgt;
                    }
                }
                for (int c = 0; c < 3; c++) out[((size_t)y * w + x) * 3 + c] = sm[c] / weight;
            }
    for (size_t i = 0; i < (size_t)w * h; i++)
        for (int c = 0; c < 3; c++) {
            const float val = out[i * 3 + c];
            rgba[i * 4 + c] = (unsigned char)(val <= 0.0f ? 0 : val >= 1.0f ? 255 : (int)(val * 255.0f + 0.5f));
        }
    RTProbeCancel();
    return true;
}

// DE UNA SOLA VEZ (el harness, lo que no muestra barra): el mismo trabajo hasta terminar
bool RTBakeProbes(const std::vector<float>& pos, int rays, std::vector<float>& sh, std::vector<unsigned char>* valid) {
    if (!RTProbeBeginLight(pos, rays)) return false;
    while (RTProbeActive()) RTProbeStep(100000);
    return RTProbeTakeLight(sh, valid);
}

// ---------------------------------------------------------------------------
//  PRECALCULAR LA ILUMINACION (el lightmap de una malla estatica, ver RayTracer.h): cada TEXEL que cubre una cara en la
//  capa UV elegida es un punto de la superficie (su posicion y su normal en el mundo). Su luz = la DIRECTA de las
//  lamparas (con sus sombras: LightIn con un material blanco y mate) + la INDIRECTA (rayos al hemisferio pesados por el
//  coseno: el fondo y lo que rebota). Solo con lo ESTATICO, como las sondas. Se hace de a pedazos (RTBakeStep) para
//  que el editor muestre el progreso
// ---------------------------------------------------------------------------
static void LightIn(const RTScene& e, const RTV& p, const RTV& n, const RTV& towardsCam, const float* alb, const RTMaterial& m, RTRng& rng, float* out);
static inline RTV DirRandomSphere(RTRng& rng);
struct RTBakeTexel { int idx; RTV p, n; };
struct RTBake {
    bool active; RTScene e; std::vector<RTBakeTexel> tx; size_t next; int w, h, samples;
    std::vector<float> light; std::vector<unsigned char> covered; RTRng rng;
    std::vector<float> ind, nrm;   // la luz INDIRECTA aparte (se suaviza al terminar) y la normal de cada texel
    bool indirect, denoiser; int bounces, bouncesPrev;
    std::vector<int> directRays, raysBounce;   // los rayos de sombra de cada lampara: en la luz del texel y en los rebotes
    // LO QUE HACE LA MALLA HORNEADA CON LA LUZ (sus banderas al empezar): sin sombras = no recibe sombras (la directa le
    // llega entera); captador = la textura sale BLANCA con solo las sombras (directa con sombras / sin sombras) por la
    // oclusion ambiental (lo que la tapa alrededor, a menos de un metro)
    bool noShadows, picker;
    // LA MASCARA DEL SOL (TexLayer::maskSun): de cada texel, la luz directa del SOL (la lampara que da las sombras en
    // tiempo real: la primera direccional) -su brillo; en un captador, que parte de la directa es del sol- y al terminar,
    // que fraccion de toda su luz es esa (0..1)
    int sunIdx;
    std::vector<float> sunL, frac;
    RTBake() : active(false), next(0), w(0), h(0), samples(16), rng(4242u), indirect(true), denoiser(true), bounces(3), bouncesPrev(0),
                   noShadows(false), picker(false), sunIdx(-1) {}
};
static RTBake gBake;
bool RTBakeActive() { return gBake.active; }
void RTBakeCancel() {
    if (gBake.active) g_rt.bounces = gBake.bouncesPrev;   // (los rebotes del render de vuelta)
    gBake.active = false; gBake.e.Clear(); gBake.tx.clear();
}
const std::vector<float>& RTBakeLight() { return gBake.light; }
const std::vector<float>& RTBakeSun() { return gBake.frac; }
// EL SOL de la escena: la primera lampara direccional prendida, recorriendo el arbol (la misma que elige
// RealTimeShadows.cpp para las sombras en tiempo real)
static Light* SunOfScene(Object* o) {
    if (!o) return NULL;
    if (o != (Object*)SceneCollection && (!o->visible || W3dViewLocalHidden(o))) return NULL;
    if (o->getType() == ObjectType::light) { Light* l = (Light*)o; if (l->directional && l->intensity > 0.0f) return l; }
    for (size_t i = 0; i < o->Childrens.size(); i++) { Light* l = SunOfScene(o->Childrens[i]); if (l) return l; }
    return NULL;
}
const std::vector<unsigned char>& RTCoveredBake() { return gBake.covered; }
bool RTBakePicker() { return gBake.picker; }
bool RTBakeBegin(Mesh* m, int uvLayer, int w, int h, int samples, std::string* reason, bool indirect, int bounces, bool denoiser) {
    RTBakeCancel();
    if (!m || !m->vertex || !m->normals || !m->faces || m->facesSize < 3 || w < 1 || h < 1) { if (reason) *reason = "The mesh has no faces"; return false; }
    const GLfloat* uvs = m->UVOfLayer(uvLayer);
    if (!uvs) uvs = m->uv;
    if (!uvs) { if (reason) *reason = "The mesh has no UV"; return false; }
    g_rtOnlyStatic = true;
    BuildRTScene(gBake.e);
    g_rtOnlyStatic = false;
    gBake.w = w; gBake.h = h; gBake.samples = samples < 1 ? 1 : samples; gBake.next = 0;
    // LAS SOMBRAS BLANDAS (una lampara con radio: el sol de area, una lampara grande): cada rayo de sombra cae en un punto
    // al azar de la lampara, y con los pocos del render la penumbra salia PUNTEADA (un pixel si, un pixel no). Al hornear
    // cada lampara con radio tira el doble de las muestras (de 16 a 256): la penumbra sale suave
    // (SOLO en la luz DIRECTA del texel: en los rebotes van los de siempre, la indirecta ya sale suavizada; con muchos en
    //  cada rebote el horneado tardaba 15 veces mas)
    {
        int rs = gBake.samples * 2; if (rs < 16) rs = 16; if (rs > 256) rs = 256;
        gBake.directRays.clear(); gBake.raysBounce.clear();
        for (size_t i = 0; i < gBake.e.lights.size(); i++) {
            const int r = gBake.e.lights[i].rays;
            gBake.raysBounce.push_back(r);
            gBake.directRays.push_back((gBake.e.lights[i].radius > 0.0f && r < rs) ? rs : r);
        }
    }
    gBake.indirect = indirect; gBake.denoiser = denoiser;
    gBake.noShadows = !m->receivesShadow;
    gBake.picker = m->catchesShadows;
    gBake.bounces = bounces < 1 ? 1 : (bounces > 8 ? 8 : bounces);
    gBake.light.assign((size_t)w * h * 3, 0.0f);
    gBake.ind.assign((size_t)w * h * 3, 0.0f);
    gBake.sunL.assign((size_t)w * h, 0.0f);
    gBake.frac.assign((size_t)w * h, 0.0f);
    gBake.sunIdx = -1;
    { const Light* sun = SunOfScene(SceneCollection);
      for (size_t i = 0; i < gBake.e.lights.size() && sun; i++) if (gBake.e.lights[i].source == sun) { gBake.sunIdx = (int)i; break; } }
    // (con las sombras en tiempo real prendidas: la que llega igual es 1 - intensidad * (1 - color), como en el shader)
    { const float in = g_shadows.intensity < 0.0f ? 0.0f : (g_shadows.intensity > 1.0f ? 1.0f : g_shadows.intensity);
      for (int c = 0; c < 3; c++) g_rtShadowColorSun[c] = g_shadows.on ? 1.0f - in * (1.0f - g_shadows.color[c]) : 0.0f; }
    gBake.nrm.assign((size_t)w * h * 3, 0.0f);
    gBake.covered.assign((size_t)w * h, 0);
    Matrix4 W; m->GetWorldMatrix(W);
    // LOS TEXELS DE CADA TRIANGULO (la fila es v * alto, como la lee GL: la fila 0 es la de arriba de la imagen)
    for (int t = 0; t + 2 < m->facesSize; t += 3) {
        const int vi[3] = { (int)m->faces[t], (int)m->faces[t + 1], (int)m->faces[t + 2] };
        float px[3], py[3]; RTV P[3], N[3]; bool ok = true;
        for (int k = 0; k < 3; k++) {
            if (vi[k] < 0 || vi[k] >= m->vertexSize) { ok = false; break; }
            px[k] = uvs[vi[k] * 2] * (float)w; py[k] = uvs[vi[k] * 2 + 1] * (float)h;
            const Vector3 wp = W * Vector3(m->vertex[vi[k] * 3], m->vertex[vi[k] * 3 + 1], m->vertex[vi[k] * 3 + 2]);
            P[k] = V3(wp.x, wp.y, wp.z);
            const float nx = m->normals[vi[k] * 3] / 127.0f, ny = m->normals[vi[k] * 3 + 1] / 127.0f, nz = m->normals[vi[k] * 3 + 2] / 127.0f;
            N[k] = Norm(V3(W.m[0]*nx + W.m[4]*ny + W.m[8]*nz, W.m[1]*nx + W.m[5]*ny + W.m[9]*nz, W.m[2]*nx + W.m[6]*ny + W.m[10]*nz));
        }
        if (!ok) continue;
        const float den = (py[1] - py[2]) * (px[0] - px[2]) + (px[2] - px[1]) * (py[0] - py[2]);
        if (fabsf(den) < 1e-12f) continue;
        int x0 = (int)floorf((std::min)(px[0], (std::min)(px[1], px[2]))) - 1, x1 = (int)ceilf((std::max)(px[0], (std::max)(px[1], px[2]))) + 1;
        int y0 = (int)floorf((std::min)(py[0], (std::min)(py[1], py[2]))) - 1, y1 = (int)ceilf((std::max)(py[0], (std::max)(py[1], py[2]))) + 1;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > w - 1) x1 = w - 1; if (y1 > h - 1) y1 = h - 1;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                const float cx = (float)x + 0.5f, cy = (float)y + 0.5f;
                float b0 = ((py[1] - py[2]) * (cx - px[2]) + (px[2] - px[1]) * (cy - py[2])) / den;
                float b1 = ((py[2] - py[0]) * (cx - px[2]) + (px[0] - px[2]) * (cy - py[2])) / den;
                float b2 = 1.0f - b0 - b1;
                // (adentro, o a medio texel del borde: el texel que el filtro bilineal lee en la orilla de la cara)
                const float margin = -0.5f / (float)(w > h ? w : h) * 4.0f;
                if (b0 < margin || b1 < margin || b2 < margin) continue;
                const int id = y * w + x;
                if (gBake.covered[(size_t)id]) continue;   // (el primer triangulo que lo toca)
                if (b0 < 0.0f) b0 = 0.0f; if (b1 < 0.0f) b1 = 0.0f; if (b2 < 0.0f) b2 = 0.0f;
                const float sb = b0 + b1 + b2; b0 /= sb; b1 /= sb; b2 /= sb;
                RTBakeTexel tt; tt.idx = id;
                tt.p = P[0] * b0 + P[1] * b1 + P[2] * b2;
                tt.n = Norm(N[0] * b0 + N[1] * b1 + N[2] * b2);
                gBake.tx.push_back(tt);
                gBake.covered[(size_t)id] = 1;
                gBake.nrm[(size_t)id * 3] = tt.n.x; gBake.nrm[(size_t)id * 3 + 1] = tt.n.y; gBake.nrm[(size_t)id * 3 + 2] = tt.n.z;
            }
    }
    if (gBake.tx.empty()) { if (reason) *reason = "The UV does not cover the texture"; gBake.e.Clear(); return false; }
    w3dLogf("[horneado] %d texels (%dx%d), %d muestras, la escena estatica: %d triangulos, %d luces",
            (int)gBake.tx.size(), w, h, gBake.samples, (int)gBake.e.tris.size(), (int)gBake.e.lights.size());
    // LOS REBOTES: mientras hornea, los del horneado (el primer rayo desde el texel ya es el primero). Al terminar vuelven
    gBake.bouncesPrev = g_rt.bounces;
    g_rt.bounces = gBake.bounces;
    gBake.active = true;
    return true;
}
float RTBakeStep(int msBudget) {
    if (!gBake.active) return 1.0f;
    extern unsigned int w3dGetTicks();
    const unsigned int t0 = w3dGetTicks();
    RTMaterial white; memset(&white, 0, sizeof(white));   // (mate y blanco: solo la luz que llega)
    white.diffuse[0] = white.diffuse[1] = white.diffuse[2] = 1.0f; white.alpha = 1.0f; white.roughness = 0.0f;
    white.tex = white.nmTex = white.emTex = white.rugTex = white.metTex = white.alphaTex = -1; white.ior = 1.0f;
    static const float one[3] = { 1.0f, 1.0f, 1.0f };
    int done = 0;
    while (gBake.next < gBake.tx.size()) {
        const RTBakeTexel& T = gBake.tx[gBake.next++];
        for (size_t i = 0; i < gBake.e.lights.size(); i++) gBake.e.lights[i].rays = gBake.directRays[i];
        float dir[3];
        g_rtNoShadows = gBake.noShadows;
        g_rtLightSun = gBake.sunIdx; g_rtSunLight[0] = g_rtSunLight[1] = g_rtSunLight[2] = 0.0f;   // (el sol, aparte)
        LightIn(gBake.e, T.p, T.n, T.n, one, white, gBake.rng, dir);
        g_rtLightSun = -1;
        g_rtNoShadows = false;
        const float sunLum = g_rtSunLight[0] * 0.299f + g_rtSunLight[1] * 0.587f + g_rtSunLight[2] * 0.114f;
        const RTV o = T.p + T.n * 1e-3f;
        if (gBake.picker) {
            // CAPTADOR DE SOMBRAS: cuanto de la luz directa le tapan (con sombras / sin sombras) y la oclusion ambiental.
            // Gris: la textura es blanca donde no le cae nada
            float sin[3];
            g_rtNoShadows = true;
            LightIn(gBake.e, T.p, T.n, T.n, one, white, gBake.rng, sin);
            g_rtNoShadows  = false;
            const float lc = dir[0] * 0.299f + dir[1] * 0.587f + dir[2] * 0.114f, ls = sin[0] * 0.299f + sin[1] * 0.587f + sin[2] * 0.114f;
            gBake.sunL[(size_t)T.idx] = (lc > 1e-5f) ? sunLum / lc : 0.0f;   // (el captador: que parte de su directa es del sol)
            float r = (ls > 1e-5f) ? lc / ls : 1.0f;
            if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
            const int nAO = gBake.samples < 16 ? 16 : gBake.samples;
            int covered = 0;
            for (int s = 0; s < nAO; s++) {
                RTV d = Norm(T.n + DirRandomSphere(gBake.rng));   // (pesado por el coseno)
                if (Dot(d, T.n) <= 0.0f) d = T.n;
                RTHit hh;
                if (Trace(gBake.e, o, d, 1e-4f, 1.0f, true, hh, -1, RT_PROJECTS)) covered++;
            }
            const float ao = 1.0f - (float)covered / (float)nAO;
            float* out = &gBake.light[(size_t)T.idx * 3];
            float* oi = &gBake.ind[(size_t)T.idx * 3];
            out[0] = out[1] = out[2] = r;
            oi[0] = oi[1] = oi[2] = ao;
            if ((++done & 15) == 0 && (int)(w3dGetTicks() - t0) >= msBudget) break;
            continue;
        }
        for (size_t i = 0; i < gBake.e.lights.size(); i++) gBake.e.lights[i].rays = gBake.raysBounce[i];
        float ind[3] = { 0, 0, 0 };
        for (int s = 0; gBake.indirect && s < gBake.samples; s++) {
            RTV d = Norm(T.n + DirRandomSphere(gBake.rng));   // (pesado por el coseno)
            if (Dot(d, T.n) <= 0.0f) d = T.n;
            float L[3]; Radiance(gBake.e, o, d, gBake.rng, 1, L);
            ind[0] += L[0]; ind[1] += L[1]; ind[2] += L[2];
        }
        const float k = 1.0f / (float)gBake.samples;
        float* out = &gBake.light[(size_t)T.idx * 3];
        float* oi = &gBake.ind[(size_t)T.idx * 3];
        out[0] = dir[0]; out[1] = dir[1]; out[2] = dir[2];
        oi[0] = ind[0] * k; oi[1] = ind[1] * k; oi[2] = ind[2] * k;
        gBake.sunL[(size_t)T.idx] = sunLum;
        if ((++done & 15) == 0 && (int)(w3dGetTicks() - t0) >= msBudget) break;
    }
    if (gBake.next >= gBake.tx.size()) {
        // LA INDIRECTA SUAVIZADA: con pocos rayos por texel sale granulada. Se promedia con los vecinos que miran para el
        // mismo lado (asi no cruza de una cara a otra); la DIRECTA (las sombras) queda nitida
        const int w = gBake.w, h = gBake.h;
        for (int pass = 0; gBake.denoiser && (gBake.indirect || gBake.picker) && pass < 3; pass++) {
            std::vector<float> I2(gBake.ind);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    const int id = y * w + x;
                    if (!gBake.covered[(size_t)id]) continue;
                    const float* n0 = &gBake.nrm[(size_t)id * 3];
                    float acc[3] = { 0, 0, 0 }; float weight = 0.0f;
                    for (int dy = -2; dy <= 2; dy++)
                        for (int dx = -2; dx <= 2; dx++) {
                            const int xx = x + dx, yy = y + dy;
                            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                            const int j = yy * w + xx;
                            if (!gBake.covered[(size_t)j]) continue;
                            const float* n1 = &gBake.nrm[(size_t)j * 3];
                            if (n0[0] * n1[0] + n0[1] * n1[1] + n0[2] * n1[2] < 0.9f) continue;
                            const float* q = &gBake.ind[(size_t)j * 3];
                            acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]; weight += 1.0f;
                        }
                    if (weight > 0.0f) for (int c = 0; c < 3; c++) I2[(size_t)id * 3 + c] = acc[c] / weight;
                }
            gBake.ind.swap(I2);
        }
        // (el captador: las sombras POR la oclusion ambiental; si no, la directa MAS la indirecta)
        if (gBake.picker) for (size_t i = 0; i < gBake.light.size(); i++) gBake.light[i] *= gBake.ind[i];
        else                for (size_t i = 0; i < gBake.light.size(); i++) gBake.light[i] += gBake.ind[i];
        // LA MASCARA DEL SOL: que fraccion de la luz de cada texel es la directa del sol (el captador ya la tiene)
        for (size_t i = 0; i < gBake.frac.size(); i++) {
            float f;
            if (gBake.picker) f = gBake.sunL[i];
            else {
                const float* l = &gBake.light[i * 3];
                const float tot = l[0] * 0.299f + l[1] * 0.587f + l[2] * 0.114f;
                f = (tot > 1e-5f) ? gBake.sunL[i] / tot : 0.0f;
            }
            gBake.frac[i] = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
        }
        g_rt.bounces = gBake.bouncesPrev;
        gBake.active = false; gBake.e.Clear();
        return 1.0f;
    }
    return (float)gBake.next / (float)gBake.tx.size();
}

// ---------------------------------------------------------------------------
//  LA SONDA DE REFLEJO (objects/ReflectionProbe.h): lo que se ve desde 'pos' en todas las direcciones, en una imagen 360
//  equirectangular de w x h (w = 2h), con la MISMA convencion que el reflejo "Equirectangular 360" del material (mundo, Y
//  arriba: u = 0.5 + atan2(z, x) / 2pi, v = acos(y) / pi; la fila 0 es la de arriba). Cada pixel: 4 rayos de camara (con
//  las lamparas, sus sombras, los rebotes y el fondo, como el render). soloEstaticos: lo que se mueve no se ve (horneada)
// ---------------------------------------------------------------------------
bool RTBakeReflection(const float pos[3], int w, int h, float near, int samples, int bounces, float intensity,
                      std::vector<unsigned char>& rgba, std::string* reason) {
    if (!RTProbeBeginReflection(pos, w, h, near, samples, bounces, intensity, reason)) return false;
    while (RTProbeActive()) RTProbeStep(100000);
    return RTProbeTakeReflection(rgba);
}

// ---------------------------------------------------------------------------
//  camara (misma cuenta que Viewport3D::ProjectPoint, al reves)
// ---------------------------------------------------------------------------
// ndc = (e.xy / (sx, sy)) [/ ez en perspectiva] * zoom + pan  (Viewport3D::Lente)
// lente / foco: la PROFUNDIDAD DE CAMPO de la camara (Camera::dof): el radio de la lente (m, 0 = agujerito: todo nitido) y
// la distancia enfocada (m)
struct RTCamera { RTV pos, right, arr, forward; float sx, sy, zoom, panX, panY; bool ortho; float lens, focus; };
// EL RENDER POR FRANJAS (RTRenderStripes): la imagen que se traza es una FRANJA de alto h que empieza en la fila
// g_rtStripeY0 de una imagen de g_rtStripeHeight filas (0 = la imagen entera). La camara es la misma, mirando ese pedazo
static int g_rtStripeY0 = 0, g_rtStripeHeight = 0;
// 'vista' = lo que se ve en el viewport (su lente tal cual: el marco de la camara, el zoom y el pan); si no, el render
// a archivo: la imagen entera de w x h con la camara (o la vista), sin inspeccion
static void CameraOf(Viewport3D* vp, int w, int h, RTCamera& c, bool view) {
    const Viewport3D::Lens L = vp->CurrentLens();
    c.pos = V3(L.pos.x, L.pos.y, L.pos.z); c.right = V3(L.right.x, L.right.y, L.right.z);
    c.arr = V3(L.arr.x, L.arr.y, L.arr.z); c.forward = V3(L.fwd.x, L.fwd.y, L.fwd.z);
    c.ortho = L.ortho;
    const bool camFrame = (vp->ViewFromCameraActive && CameraActive);
    // la PROFUNDIDAD DE CAMPO: solo mirando por la camara y con la suya prendida (la vista libre es un agujerito)
    c.lens = (camFrame && CameraActive->dof && !c.ortho) ? CameraActive->LensRadius() : 0.0f;
    c.focus = (c.lens > 0.0f) ? CameraActive->DistanceFocus() : 0.0f;
    if (view) { c.sx = L.sx; c.sy = L.sy; c.zoom = L.zoom; c.panX = L.panX; c.panY = L.panY; return; }
    float t = tanf((camFrame ? CameraActive->fov : fovDeg) * 0.5f * 3.14159265f / 180.0f);
    if (c.ortho) { t *= vp->orbitDistance; if (t < 0.001f) t = 0.001f; }
    const int hTot = (g_rtStripeHeight > 0) ? g_rtStripeHeight : h;
    c.sy = t; c.sx = t * ((hTot > 0) ? (float)w / (float)hTot : 1.0f);
    c.zoom = 1.0f; c.panX = c.panY = 0.0f;
    // una FRANJA (filas y0..y0+h de hTot): su alto es la parte que le toca del alto de la imagen y se corre hacia arriba
    // o hacia abajo (fila py de la franja = fila y0 + py de la imagen)
    if (g_rtStripeHeight > 0 && h > 0) { c.sy = t * (float)h / (float)hTot; c.panY = -(float)(hTot - 2 * g_rtStripeY0 - h) / (float)h; }
}
// LA LENTE: el rayo sale de un punto al azar del disco de la lente hacia el MISMO punto del plano enfocado (lo que esta a
// esa distancia queda nitido; lo de mas cerca y mas lejos, desparramado en un disco que crece con la distancia al foco)
static inline void RayLens(const RTCamera& c, RTRng& rng, RTV& o, RTV& d) {
    if (c.lens <= 0.0f || c.ortho) return;
    const float dd = Dot(d, c.forward);
    if (dd <= 1e-4f) return;
    const RTV P = o + d * (c.focus / dd);
    const float r = sqrtf(rng.F()) * c.lens, a = rng.F() * 6.2831853f;
    o = o + c.right * (r * cosf(a)) + c.arr * (r * sinf(a));
    d = Norm(P - o);
}
static inline void RayPixel(const RTCamera& c, float px, float py, int w, int h, RTV& o, RTV& d) {
    const float ndcX = px / (float)w * 2.0f - 1.0f, ndcY = 1.0f - py / (float)h * 2.0f;
    const float ex = (ndcX - c.panX) / c.zoom * c.sx, ey = (ndcY - c.panY) / c.zoom * c.sy;
    if (c.ortho) { o = c.pos + c.right * ex + c.arr * ey; d = c.forward; }
    else { o = c.pos; d = Norm(c.forward + c.right * ex + c.arr * ey); }
}

// ---------------------------------------------------------------------------
//  sombreado difuso: color x (ambiente + suma de luces con sombra) + emision
// ---------------------------------------------------------------------------
static inline void Sample(const RTTexture& t, float u, float v, float* rgba) {
    u -= floorf(u); v -= floorf(v);
    if (t.filtered) {   // BILINEAL entre los 4 texeles de alrededor (repetido en los bordes)
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
static inline void UVIn(const RTScene& e, const RTHit& h, float& u, float& v) {
    const RTTri& T = e.tris[(size_t)h.tri]; const float b0 = 1.0f - h.b1 - h.b2;
    u = e.uv[(size_t)T.v0*2]*b0 + e.uv[(size_t)T.v1*2]*h.b1 + e.uv[(size_t)T.v2*2]*h.b2;
    v = e.uv[(size_t)T.v0*2+1]*b0 + e.uv[(size_t)T.v1*2+1]*h.b1 + e.uv[(size_t)T.v2*2+1]*h.b2;
}
// el material EN ESE IMPACTO: los mapas en modo textura reemplazan a sus numeros (la emision, la rugosidad, el metalico
// y el alfa leidos de su textura en el UV del impacto)
static inline RTMaterial MaterialIn(const RTScene& e, const RTHit& h, const RTMaterial& m0) {
    RTMaterial m = m0;
    if (m.emTex < 0 && m.rugTex < 0 && m.metTex < 0 && m.alphaTex < 0) return m;
    float u, v, tx[4]; UVIn(e, h, u, v);
    if (m.emTex >= 0) { Sample(e.texs[(size_t)m.emTex], u, v, tx); m.emission[0] = tx[0]; m.emission[1] = tx[1]; m.emission[2] = tx[2]; }
    if (m.rugTex >= 0) { Sample(e.texs[(size_t)m.rugTex], u, v, tx); m.roughness = tx[0]; }
    if (m.metTex >= 0) { Sample(e.texs[(size_t)m.metTex], u, v, tx); m.metallic = tx[0]; }
    if (m.alphaTex >= 0) { Sample(e.texs[(size_t)m.alphaTex], u, v, tx); m.alpha = (tx[3] < 0.999f) ? tx[3] : tx[0]; m.transparent = true; }
    return m;
}
// cuanto TAPA la superficie en ese impacto: 1 = opaca; transparente = alpha del color x alpha de la textura;
// aditiva = 0 (suma luz, no tapa nada)
static inline float OpacityIn(const RTScene& e, const RTHit& h) {
    const RTMaterial m = MaterialIn(e, h, e.mats[(size_t)e.tris[(size_t)h.tri].mat]);
    if (!m.transparent && m.trans <= 0.0f) return 1.0f;
    if (m.additive) return 0.0f;
    float a = m.transparent ? m.alpha : 1.0f;
    if (m.transparent && m.tex >= 0 && a > 0.0f) { float u, v, tx[4]; UVIn(e, h, u, v); Sample(e.texs[(size_t)m.tex], u, v, tx); a *= tx[3]; }
    if (m.clip > 0.0f) a = Crop(a, m.clip);
    return a * (1.0f - m.trans);   // (el vidrio deja pasar la luz)
}
// VISIBILIDAD de una luz desde 'o' en direccion 'd' (0 = tapada, 1 = libre): los opacos cortan; los transparentes
// dejan pasar (1 - opacidad) y se sigue atravesando (hasta 8 capas)
// la DISTANCIA de una luz: se apaga suave hasta 0 justo ahi, (1 - (d/D)^4)^2 (sin escalon en el borde). D <= 0 = sin limite
static inline float WindowDistance(float d, float D) {
    if (D <= 0.0f) return 1.0f;
    const float x = d / D; if (x >= 1.0f) return 0.0f;
    const float x2 = x * x, w = 1.0f - x2 * x2; return w * w;
}
// EL FONDO en una direccion: el color, o el punto de la textura 360 (la misma cuenta que W3dBackgroundDirectionUV)
static void BackgroundIn(const RTScene& e, const RTV& d, float* out) {
    if (e.bgTex < 0) { out[0] = e.bg[0]; out[1] = e.bg[1]; out[2] = e.bg[2]; return; }
    const float* m = e.bgRot;
    const float x = m[0] * d.x + m[1] * d.y + m[2] * d.z, y = m[3] * d.x + m[4] * d.y + m[5] * d.z;
    float z = m[6] * d.x + m[7] * d.y + m[8] * d.z;
    if (z > 1.0f) z = 1.0f; if (z < -1.0f) z = -1.0f;
    float u = 0.5f + atan2f(y, x) * 0.15915494f, v = acosf(z) * 0.31830989f;
    if (u >= 1.0f) u -= 1.0f; if (u < 0.0f) u += 1.0f;
    float rgba[4]; Sample(e.texs[(size_t)e.bgTex], u, v, rgba);
    out[0] = rgba[0]; out[1] = rgba[1]; out[2] = rgba[2];
}
static float Visibility(const RTScene& e, const RTV& o, const RTV& d, float tMax) {
    RTHit h;
    if (!e.hasTransparent) return Trace(e, o, d, 1e-4f, tMax, true, h, -1, RT_PROJECTS) ? 0.0f : 1.0f;
    float vis = 1.0f; float tMin = 1e-4f; int excl = -1;
    // (lo que en ese punto es TOTALMENTE transparente no gasta capa: el pelo son muchas tarjetas superpuestas y el rayo
    // se quedaba sin capas en los huecos, lo de atras salia negro. 'vuelta' es el tope por si nada tapa nunca)
    for (int layer = 0, turn = 0; layer < g_rt.layers && turn < 256; turn++) {
        if (!Trace(e, o, d, tMin, tMax, false, h, excl, RT_PROJECTS)) return vis;
        const float op = OpacityIn(e, h);
        if (op > 0.0f) layer++;
        vis *= (1.0f - op);
        if (vis < 0.004f) return 0.0f;
        tMin = h.t * (1.0f - 1e-5f); excl = h.tri;   // seguir el mismo rayo: solo se descarta lo ya atravesado
    }
    return vis;
}
static inline RTV DirRandomSphere(RTRng& rng) {   // punto uniforme sobre la esfera unitaria
    const float z = 1.0f - 2.0f * rng.F();
    float rr = 1.0f - z * z; if (rr < 0.0f) rr = 0.0f;
    const float r = sqrtf(rr), a = 6.2831853f * rng.F();
    return V3(r * cosf(a), r * sinf(a), z);
}
// en la VISTA PREVIA (escalera de resolucion) cada luz tira UN solo rayo de sombra: las penumbras
// (rtRays > 1) recien se muestrean en los pases a resolucion completa. En el N95 esto mantiene el
// primer pantallazo en ~1 rayo por bloque aunque haya lamparas de area.
static bool g_rtPreviewFast = false;
// luz directa en el impacto (difuso: color x (ambiente + suma de luces visibles) + emision). 'alb' ya trae la textura.
static void LightIn(const RTScene& e, const RTV& p, const RTV& n, const RTV& towardsCam, const float* alb, const RTMaterial& m, RTRng& rng, float* out) {
    if (m.noLight) { out[0] = alb[0]; out[1] = alb[1]; out[2] = alb[2]; return; }   // "unlit": el color tal cual
    float light[3] = { 0, 0, 0 };
    float brightness[3] = { 0, 0, 0 };   // el reflejo de las LAMPARAS (no son geometria: el rayo de reflejo no las ve)
    // exponente del brillo desde la rugosidad: mate (1) = ancho y debil; pulido (0.05) = chico e intenso. Un espejo
    // perfecto (rugosidad 0) no tiene brillo de lampara puntual (seria un punto infinitesimal).
    const float rug = (m.roughness < 0.05f) ? 0.05f : m.roughness;
    const float expo = 2.0f / (rug * rug) - 2.0f + 1.0f;
    const float brightnessWeight = (m.roughness <= 0.0f) ? 0.0f : (m.metallic + (1.0f - m.metallic) * m.f0);   // (f0: del IOR)
    float colBrightness[3] = { m.metallic * alb[0] + (1.0f - m.metallic), m.metallic * alb[1] + (1.0f - m.metallic), m.metallic * alb[2] + (1.0f - m.metallic) };
    const RTV po = p + n * 1e-3f;   // origen de los rayos de sombra, apenas afuera de la superficie
    for (size_t i = 0; i < e.lights.size(); i++) {
        const RTLight& L = e.lights[i];
        light[0] += L.ambient[0]; light[1] += L.ambient[1]; light[2] += L.ambient[2];
        float lambert = 0.0f, spec = 0.0f, att = 1.0f, covered = 0.0f;
        const bool sunBake = (int)i == g_rtLightSun && L.directional;
        const int nRays = g_rtPreviewFast ? 1 : L.rays;
        for (int r = 0; r < nRays; r++) {
            RTV ldir; float dist;
            if (L.directional) {
                ldir = L.dir;
                if (L.radius > 0.0f) ldir = Norm(L.dir + DirRandomSphere(rng) * tanf(L.radius * 3.14159265f / 180.0f));   // radio = grados
                dist = 1e30f;
            } else {
                RTV lp = L.pos;
                if (L.radius > 0.0f) lp = lp + DirRandomSphere(rng) * L.radius;   // un punto de la esfera de la lampara
                const RTV a = lp - po; dist = Length(a); ldir = (dist > 1e-9f) ? a * (1.0f / dist) : n;
            }
            const float nl = Dot(n, ldir);
            if (nl <= 0.0f) continue;
            if (L.distance > 0.0f && dist >= L.distance) continue;   // (mas alla de su distancia no alumbra)
            float cone = 1.0f;   // (afuera del cono de un foco no llega luz; adentro, mas concentrada hacia el centro)
            if (L.spotCos > -1.0f) {
                const float sd = -Dot(ldir, L.spotDir);
                if (sd < L.spotCos) continue;
                if (L.spotExpo > 0.0f) cone = powf(sd, L.spotExpo);
            }
            const float vis = (g_rtNoShadows ? 1.0f : Visibility(e, po, ldir, dist)) * cone;   // 0..1: lo transparente deja pasar parte de la luz
            if (sunBake) covered += nl * (cone - vis);
            if (vis <= 0.0f) continue;
            lambert += nl * vis;
            if (brightnessWeight > 0.0f) {   // Blinn-Phong: (n . h)^expo, normalizado para que el pulido concentre la misma energia
                const RTV hv = Norm(ldir + towardsCam);
                const float nh = Dot(n, hv);
                if (nh > 0.0f) spec += powf(nh, expo) * (expo + 2.0f) * 0.125f * vis;
            }
        }
        // (la parte tapada del sol entra con el color de la sombra; la mascara del sol -g_rtSunLight- es solo la que llega)
        if (sunBake && covered > 0.0f)
            for (int c = 0; c < 3; c++) light[c] += L.color[c] * covered / (float)nRays * g_rtShadowColorSun[c];
        if (lambert <= 0.0f && spec <= 0.0f) continue;
        lambert /= (float)nRays; spec /= (float)nRays;
        if (!L.directional) { const float dd = Length(L.pos - po); att = 1.0f / (L.attC + L.attL * dd + L.attQ * dd * dd); if (att > 1.0f) att = 1.0f;
            att *= WindowDistance(dd, L.distance); }
        for (int c = 0; c < 3; c++) { light[c] += L.color[c] * lambert * att; brightness[c] += L.color[c] * spec * att; }
        if ((int)i == g_rtLightSun) for (int c = 0; c < 3; c++) g_rtSunLight[c] += L.color[c] * lambert * att;
    }
    const float diffuseWeight = 1.0f - m.metallic;   // el metal no tiene difuso: todo es reflejo
    for (int c = 0; c < 3; c++) out[c] = alb[c] * light[c] * diffuseWeight + colBrightness[c] * brightness[c] * brightnessWeight + m.emission[c];
}
// un rayo de camara: la primera superficie; si es transparente, su color pesa 'alpha' y el resto lo pone lo que hay
// detras (se sigue el mismo rayo, hasta 8 capas); lo aditivo se suma y no tapa
static void Radiance(const RTScene& e, const RTV& o0, const RTV& d, RTRng& rng, int depth, float* out);
static void Shade(const RTScene& e, const RTV& o0, const RTV& d, RTRng& rng, float* out) { Radiance(e, o0, d, rng, 0, out); }
static void Radiance(const RTScene& e, const RTV& o0, const RTV& d0, RTRng& rng, int depth, float* out) {
    out[0] = out[1] = out[2] = 0.0f;
    float weight = 1.0f; RTV o = o0, d = d0; float tMin = 1e-4f; int excl = -1;
    // (pasadas las capas transparentes que se permiten, lo de atras queda negro: no se suma nada mas)
    // (lo TOTALMENTE transparente en ese punto no gasta capa: ver Visibilidad)
    for (int layer = 0, turn = 0; layer < g_rt.layers && turn < 256; turn++) {
        RTHit h;
        if (!Trace(e, o, d, tMin, 1e30f, false, h, excl, depth > 0 ? (unsigned char)RT_BOUNCES : (unsigned char)0)) {   // (un REBOTE que llega al fondo trae su luz: por el factor)
            float f[3]; BackgroundIn(e, d, f); const float s = depth > 0 ? e.lightBackground : 1.0f;
            out[0] += weight * f[0] * s; out[1] += weight * f[1] * s; out[2] += weight * f[2] * s; return; }
        const RTTri& T = e.tris[(size_t)h.tri];
        const RTMaterial m = MaterialIn(e, h, e.mats[(size_t)T.mat]);   // (con los mapas leidos en el impacto)
        const float b0 = 1.0f - h.b1 - h.b2;
        const RTV p = o + d * h.t;
        float alb[3] = { m.diffuse[0], m.diffuse[1], m.diffuse[2] };
        float alpha = m.transparent ? m.alpha : 1.0f;
        if (m.tex >= 0) {
            float u, v, tx[4]; UVIn(e, h, u, v); Sample(e.texs[(size_t)m.tex], u, v, tx);
            alb[0] *= tx[0]; alb[1] *= tx[1]; alb[2] *= tx[2];
            if (m.transparent) alpha *= tx[3];
        }
        if (m.clip > 0.0f) alpha = Crop(alpha, m.clip);
        if (alpha > 0.0f) layer++;
        if (alpha > 0.0f) {
            RTV n = V3(e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2,
                       e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2,
                       e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2);
            n = Norm(n);
            if (Dot(n, d) > 0.0f) n = n * -1.0f;   // la cara de atras se ilumina como si mirara a la camara (dos caras)
            if (m.nmTex >= 0 && e.tan.size() >= e.pos.size() / 3 * 4) {
                // NORMAL MAP: la normal de la textura (tangent-space) llevada al mundo con la base T/B/N del impacto
                float u, v, tx[4]; UVIn(e, h, u, v); Sample(e.texs[(size_t)m.nmTex], u, v, tx);
                RTV Tg = V3(e.tan[(size_t)T.v0*4]*b0 + e.tan[(size_t)T.v1*4]*h.b1 + e.tan[(size_t)T.v2*4]*h.b2,
                            e.tan[(size_t)T.v0*4+1]*b0 + e.tan[(size_t)T.v1*4+1]*h.b1 + e.tan[(size_t)T.v2*4+1]*h.b2,
                            e.tan[(size_t)T.v0*4+2]*b0 + e.tan[(size_t)T.v1*4+2]*h.b1 + e.tan[(size_t)T.v2*4+2]*h.b2);
                const float hand = (e.tan[(size_t)T.v0*4+3] < 0.0f) ? -1.0f : 1.0f;
                Tg = Tg - n * Dot(n, Tg);
                if (Dot(Tg, Tg) > 1e-12f) {
                    Tg = Norm(Tg);
                    const RTV B = Cross(n, Tg) * hand;
                    const float nx = tx[0] * 2.0f - 1.0f, ny = tx[1] * 2.0f - 1.0f, nz = tx[2] * 2.0f - 1.0f;
                    const RTV np = Tg * nx + B * ny + n * nz;
                    if (Dot(np, np) > 1e-12f) n = Norm(np);
                }
            }
            float col[3];
            g_rtNoShadows = !(T.fl & RT_RECEIVES);   // (un objeto que no recibe sombras: las lamparas le llegan enteras)
            LightIn(e, p, n, d * -1.0f, alb, m, rng, col);
            g_rtNoShadows = false;
            // METALICO: reflejo de la escena (hasta 4 rebotes), teñido por el color; la rugosidad desparrama la
            // direccion reflejada (un rayo por muestra: los pases lo promedian)
            if (m.metallic > 0.0f && depth < 4) {
                RTV r = d - n * (2.0f * Dot(d, n));
                if (m.roughness > 0.0f) {
                    const float k = m.roughness * m.roughness;
                    r = Norm(r + DirRandomSphere(rng) * k);
                    if (Dot(r, n) <= 0.0f) r = Norm(r - n * (2.0f * Dot(r, n)));   // no meterse en la superficie
                }
                float refl[3]; Radiance(e, p + n * 1e-3f, r, rng, depth + 1, refl);
                for (int c = 0; c < 3; c++) col[c] += m.metallic * alb[c] * refl[c];
            }
            // LA LUZ DEL FONDO 360 sin rebotes: un rayo al cielo pesado por el coseno; si no lo tapa nada, trae lo que se ve
            // de la imagen en esa direccion (el cielo alumbra de arriba, el piso de la imagen de abajo). Con rebotes, el
            // rebote que no pega en nada ya trae el fondo
            // (tambien un fondo de COLOR ilumina: es la luz ambiente, como en la vista Render)
            const bool backgroundLights = (e.bgTex >= 0 || e.bg[0] > 0.0f || e.bg[1] > 0.0f || e.bg[2] > 0.0f) && e.lightBackground > 0.0f;
            if (backgroundLights && g_rt.bounces == 0 && !m.noLight && m.metallic < 1.0f && !g_rtPreviewFast) {
                RTV dir = Norm(n + DirRandomSphere(rng));
                if (Dot(dir, n) <= 0.0f) dir = n;
                const float vis = Visibility(e, p + n * 1e-3f, dir, 1e30f);
                if (vis > 0.0f) { float f[3]; BackgroundIn(e, dir, f); for (int c = 0; c < 3; c++) col[c] += vis * (1.0f - m.metallic) * alb[c] * f[c] * e.lightBackground; }
            }
            // REBOTES DE LA LUZ (g_rt.bounces): la parte difusa sigue en UNA direccion al azar pesada por el coseno (n +
            // un punto de la esfera) y trae lo que ve alla (su luz directa, su emision, el fondo y sus propios rebotes).
            // Con ese reparto el aporte es albedo * lo que llega (los pases lo promedian). No en la vista previa rapida
            if (g_rt.bounces > 0 && depth < g_rt.bounces && !m.noLight && !g_rtPreviewFast && m.metallic < 1.0f) {
                RTV dir = Norm(n + DirRandomSphere(rng));
                if (Dot(dir, n) <= 0.0f) dir = n;
                float ind[3]; Radiance(e, p + n * 1e-3f, dir, rng, depth + 1, ind);
                for (int c = 0; c < 3; c++) col[c] += (1.0f - m.metallic) * alb[c] * ind[c];
            }
            if (m.additive) { for (int c = 0; c < 3; c++) out[c] += weight * col[c]; }   // suma y no tapa
            else           { for (int c = 0; c < 3; c++) out[c] += weight * alpha * (1.0f - m.trans) * col[c]; if (m.trans <= 0.0f) weight *= (1.0f - alpha); }
        }
        // TRANSMISION: lo que el alfa saca (1 - alfa) pasa DERECHO; la parte que es vidrio (alfa * transmision) pasa
        // REFRACTADA. Cada muestra sigue uno de los dos caminos al azar, con su probabilidad (los pases lo promedian)
        if (m.trans > 0.0f && !m.additive) {
            const float right = 1.0f - alpha, glass = alpha * m.trans, passes = right + glass;
            weight *= passes;
            if (weight < 0.004f) return;
            if (m.ior > 1.0001f && rng.F() * passes < glass) {
                RTV nr = Norm(V3(e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2,
                                 e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2,
                                 e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2));
                const bool enters = Dot(nr, d) < 0.0f;
                if (!enters) nr = nr * -1.0f;
                const float eta = enters ? 1.0f / m.ior : m.ior;
                const float c = -Dot(nr, d), k = 1.0f - eta * eta * (1.0f - c * c);
                d = (k < 0.0f) ? Norm(d + nr * (2.0f * c)) : Norm(d * eta + nr * (eta * c - sqrtf(k)));
                // VIDRIO ESMERILADO: la rugosidad desparrama la direccion refractada (como el reflejo del metal)
                if (m.roughness > 0.0f) {
                    const RTV dr = Norm(d + DirRandomSphere(rng) * (m.roughness * m.roughness));
                    if (Dot(dr, nr) * Dot(d, nr) > 0.0f) d = dr;   // (sin volver para el lado de donde vino)
                }
                o = p; tMin = 1e-4f; excl = h.tri;
                continue;
            }
            tMin = h.t * (1.0f - 1e-5f); excl = h.tri;
            continue;
        }
        if (!m.transparent || weight < 0.004f) return;
        if (m.ior > 1.0001f && !m.additive && m.clip <= 0.0f) {
            // REFRACCION (agua, vidrio): el rayo se dobla al entrar (o salir) y sigue desde aca
            RTV nr = Norm(V3(e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2,
                             e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2,
                             e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2));
            const bool enters = Dot(nr, d) < 0.0f;
            if (!enters) nr = nr * -1.0f;
            const float eta = enters ? 1.0f / m.ior : m.ior;
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
static void FirstImpact(const RTScene& e, const RTV& o, const RTV& d, float* alb, float* nrm) {
    alb[0] = alb[1] = alb[2] = 0.0f; nrm[0] = nrm[1] = nrm[2] = 0.0f;
    RTHit h;
    float tMin = 1e-4f; int excl = -1;
    for (int layer = 0; ; layer++) {
        if (!Trace(e, o, d, tMin, 1e30f, false, h, excl)) { BackgroundIn(e, d, alb); return; }
        // (la que tapa al menos la mitad; lo transparente y lo recortado se atraviesa: si no, el denoiser borroneaba
        // lo que se ve a traves de un vidrio o de las hojas)
        if (!e.hasTransparent || layer + 1 >= g_rt.layers || OpacityIn(e, h) >= 0.5f) break;
        tMin = h.t * (1.0f - 1e-5f); excl = h.tri;
    }
    const RTTri& T = e.tris[(size_t)h.tri];
    const RTMaterial m = MaterialIn(e, h, e.mats[(size_t)T.mat]);
    alb[0] = m.diffuse[0]; alb[1] = m.diffuse[1]; alb[2] = m.diffuse[2];
    if (m.tex >= 0) { float u, v, tx[4]; UVIn(e, h, u, v); Sample(e.texs[(size_t)m.tex], u, v, tx); alb[0] *= tx[0]; alb[1] *= tx[1]; alb[2] *= tx[2]; }
    const float b0 = 1.0f - h.b1 - h.b2;
    RTV n = V3(e.nrm[(size_t)T.v0*3]*b0 + e.nrm[(size_t)T.v1*3]*h.b1 + e.nrm[(size_t)T.v2*3]*h.b2,
               e.nrm[(size_t)T.v0*3+1]*b0 + e.nrm[(size_t)T.v1*3+1]*h.b1 + e.nrm[(size_t)T.v2*3+1]*h.b2,
               e.nrm[(size_t)T.v0*3+2]*b0 + e.nrm[(size_t)T.v1*3+2]*h.b1 + e.nrm[(size_t)T.v2*3+2]*h.b2);
    n = Norm(n);
    if (Dot(n, d) > 0.0f) n = n * -1.0f;
    nrm[0] = n.x; nrm[1] = n.y; nrm[2] = n.z;
}
// LA ESCENA A LA GPU (render/RTGpu.h): los mismos arrays, empaquetados como los lee el compute shader
static void VertsAGpu(const RTScene& e, std::vector<float>& out) {
    const size_t nv = e.pos.size() / 3;
    const bool withTan = e.tan.size() >= nv * 4;
    out.assign(nv * 12, 0.0f);
    for (size_t v = 0; v < nv; v++) {
        float* d = &out[v * 12];
        d[0] = e.pos[v*3]; d[1] = e.pos[v*3+1]; d[2] = e.pos[v*3+2]; d[3] = e.uv[v*2];
        d[4] = e.nrm[v*3]; d[5] = e.nrm[v*3+1]; d[6] = e.nrm[v*3+2]; d[7] = e.uv[v*2+1];
        if (withTan) for (int k = 0; k < 4; k++) d[8 + k] = e.tan[v*4 + k];
    }
}
static void NodesToGpu(const RTScene& e, std::vector<float>& out) {
    out.assign(e.nodes.size() * 12, 0.0f);
    for (size_t i = 0; i < e.nodes.size(); i++) {
        const RTNode& n = e.nodes[i]; float* d = &out[i * 12];
        d[0] = n.bmin[0]; d[1] = n.bmin[1]; d[2] = n.bmin[2]; d[3] = (float)n.left;
        d[4] = n.bmax[0]; d[5] = n.bmax[1]; d[6] = n.bmax[2]; d[7] = (float)n.right;
        d[8] = (float)n.first; d[9] = (float)n.count;
    }
}
static void LightsToGpu(const RTScene& e, std::vector<float>& out);
static bool SceneToGpuEnd(const RTScene& e, RTGpuScene& g);
static bool SceneToGpu(const RTScene& e) {
    RTGpuScene g;
    VertsAGpu(e, g.verts);
    g.tris.resize(e.tris.size() * 4);
    for (size_t t = 0; t < e.tris.size(); t++) { g.tris[t*4] = e.tris[t].v0; g.tris[t*4+1] = e.tris[t].v1; g.tris[t*4+2] = e.tris[t].v2; g.tris[t*4+3] = e.tris[t].mat; }
    g.mats.assign(e.mats.size() * 20, 0.0f);
    for (size_t i = 0; i < e.mats.size(); i++) {
        const RTMaterial& m = e.mats[i]; float* d = &g.mats[i * 20];
        d[0] = m.diffuse[0]; d[1] = m.diffuse[1]; d[2] = m.diffuse[2]; d[3] = m.alpha;
        d[4] = m.emission[0]; d[5] = m.emission[1]; d[6] = m.emission[2]; d[7] = m.roughness;
        d[8] = m.metallic; d[9] = m.f0; d[10] = (float)((m.noLight ? 1 : 0) | (m.transparent ? 2 : 0) | (m.additive ? 4 : 0));
        d[12] = (float)m.tex; d[13] = (float)m.nmTex; d[14] = (float)m.emTex; d[15] = (float)m.rugTex;
        d[11] = m.ior;
        d[16] = (float)m.metTex; d[17] = (float)m.alphaTex; d[18] = m.clip; d[19] = m.trans;
    }
    // el FONDO 360: un material extra al final (la rotacion en d[0..8], la textura en d[12]); ningun triangulo lo usa
    g.envMat = -1;
    if (e.bgTex >= 0) {
        g.envMat = (int)e.mats.size();
        g.mats.resize(g.mats.size() + 20, 0.0f);
        float* d = &g.mats[(size_t)g.envMat * 20];
        for (int k = 0; k < 9; k++) d[k] = e.bgRot[k];
        d[12] = (float)e.bgTex;
    }
    LightsToGpu(e, g.lights);
    NodesToGpu(e, g.nodes);
    g.order = e.order;
    size_t total = 0; for (size_t i = 0; i < e.texs.size(); i++) total += (size_t)e.texs[i].w * e.texs[i].h;
    g.texels.resize(total); g.texInfo.resize(e.texs.size() * 4);
    return SceneToGpuEnd(e, g);
}
static void LightsToGpu(const RTScene& e, std::vector<float>& out) {
    out.assign(e.lights.size() * 20, 0.0f);
    for (size_t i = 0; i < e.lights.size(); i++) {
        const RTLight& L = e.lights[i]; float* d = &out[i * 20];
        d[0] = L.pos.x; d[1] = L.pos.y; d[2] = L.pos.z; d[3] = L.directional ? 1.0f : -L.distance;   // (<= 0: -distancia)
        if (L.directional) { d[4] = L.dir.x; d[5] = L.dir.y; d[6] = L.dir.z; }
        else { d[4] = L.spotDir.x; d[5] = L.spotDir.y; d[6] = L.spotDir.z; }
        d[7] = L.radius;
        d[8] = L.color[0]; d[9] = L.color[1]; d[10] = L.color[2]; d[11] = (float)L.rays;
        d[12] = L.ambient[0]; d[13] = L.ambient[1]; d[14] = L.ambient[2]; d[15] = L.spotCos;
        d[16] = L.attC; d[17] = L.attL; d[18] = L.attQ; d[19] = L.spotExpo;
    }
}
// (las texturas, el fondo y a la GPU)
static bool SceneToGpuEnd(const RTScene& e, RTGpuScene& g) {
    size_t from = 0;
    for (size_t i = 0; i < e.texs.size(); i++) {
        const RTTexture& t = e.texs[i]; const size_t n = (size_t)t.w * t.h;
        if (n) memcpy(&g.texels[from], t.px, n * 4);
        g.texInfo[i*4] = (int)from; g.texInfo[i*4+1] = t.w; g.texInfo[i*4+2] = t.h; g.texInfo[i*4+3] = t.filtered ? 1 : 0;
        from += n;
    }
    g.bg[0] = e.bg[0]; g.bg[1] = e.bg[1]; g.bg[2] = e.bg[2];
    g.hasTransparent = e.hasTransparent;
    return RTGpuLoadScene(g);
}
static bool g_rtLastInGpu = false;
// LA GPU ES UNA SOLA (una escena y una imagen de un tamano) y la comparten dos renders: el del viewport y el de a
// archivo (Render Image / F12, a otra resolucion). El que la uso ultimo es su duenio; el otro, antes de volver a usarla,
// sube de nuevo su escena y su imagen (si no, leia la imagen del otro tamano en su buffer: se colgaba el editor)
static const void* g_rtGpuOwner = NULL;
// LOS RAYOS POR SEGUNDO de la GPU (los que traza mientras trabaja: los pases de este cuadro)
static double g_rtRaysBySeg = 0.0;
static void RTCountRays(unsigned long long rays, double ms) {
    if (ms <= 0.05 || rays == 0) return;
    const double r = (double)rays / (ms / 1000.0);
    g_rtRaysBySeg = (g_rtRaysBySeg <= 0.0) ? r : g_rtRaysBySeg * 0.8 + r * 0.2;
    static const bool profile = getenv("W3D_RT_PROFILE") != NULL;
    if (profile) w3dLogf("[rt perfil] %llu rayos en %.2f ms: %.0f Mrayos/s", rays, ms, r / 1e6);
}
double RTRaysPerSecond() { return g_rtRaysBySeg; }
// W3D_RT_PROFILE=1: cuanto tarda armar la escena, subirla y cada pase (al log)
static const bool g_rtProfile = getenv("W3D_RT_PROFILE") != NULL;

static const int kTile = 32;
// VISTA PREVIA progresiva por resolucion: antes del primer pase a resolucion completa se hacen
// pasadas rapidas con 1 rayo por BLOQUE de s x s pixeles (s = 6, 4, 2). En un viewport de 240p
// equivalen a ~40p, ~60p y ~120p: la imagen entera aparece enseguida (clave en el N95: ~1/36,
// 1/16 y 1/4 del costo) y se va afinando; recien despues arranca la acumulacion real de samples.
static const int kPrevSteps[3] = { 6, 4, 2 };
static const int kPrevLevels = 3;   // nivel 0..2 = preview; kPrevLevels = resolucion completa
struct RTRender {
    RTScene esc; RTCamera cam;
    int w, h, tilesX, tilesY;
    std::vector<float> accum;            // 3 por pixel
    std::vector<unsigned char> img;      // RGBA 8 bits (lo que se muestra / guarda)
    // el DENOISER (g_rt.denoiser): el albedo y la normal del primer impacto (del primer pase) y el resultado limpio
    std::vector<float> aovAlb, aovNrm, clean, middleColor;
    int denoiser;                         // el de este render (cambiarlo reinicia el render: RTInvalidate)
    bool denoiserFailed;                  // (fallo una vez: no se insiste, se ve sin limpiar)
    int step;                             // pases completos (solo los de resolucion completa)
    int level;                            // escalera de preview: 0..2 = bloques de 6/4/2 px; 3 = resolucion completa
    int nextTile;                         // proximo tile del pase en curso
    std::vector<unsigned char> dirtyTile; // tiles con pixeles nuevos desde la ultima subida
    bool gpu;                          // este render va en la GPU (render/RTGpu.h); si falla sigue por CPU
    bool aovGpu;                       // (los AOV del denoiser ya bajaron de la GPU)
    double lastDenoise;                // (los denoisers de la CPU, de vez en cuando mientras se forma la imagen)
    // ANTES DE CADA PASE (el desenfoque de movimiento: mueve la escena y la camara al instante de ese pase); NULL = nada
    void (*prevOfPass)(RTRender& r, void* ctx);
    void* prevCtx;
    RTRender() : w(0), h(0), tilesX(0), tilesY(0), step(0), level(kPrevLevels), nextTile(0), denoiser(0), denoiserFailed(false), gpu(false), aovGpu(false), lastDenoise(0.0), prevOfPass(NULL), prevCtx(NULL), seed(0) {}
    // la SEMILLA del ruido de una imagen nueva: con "otro ruido en cada imagen" cambia (si no, 0: la misma siempre)
    unsigned int seed;
    void NewSeed() { static unsigned int cont = 0; seed = g_rt.seedVariable ? ++cont : 0u; }
    // reuseScene: la escena ya armada (las franjas de un mismo render: el BVH una sola vez)
    void Start(Viewport3D* vp, int W, int H, bool withPreview, bool reuseScene = false) {
        NewSeed();
        w = W; h = H; tilesX = (w + kTile - 1) / kTile; tilesY = (h + kTile - 1) / kTile;
        accum.assign((size_t)w * h * 3, 0.0f); img.assign((size_t)w * h * 4, 255);
        dirtyTile.assign((size_t)tilesX * tilesY, 0);
        step = 0; nextTile = 0;
        level = withPreview ? 0 : kPrevLevels;   // el render a archivo va directo a resolucion completa
        denoiser = W3dDenoiserAvailable(g_rt.denoiser) ? g_rt.denoiser : 0;
        denoiserFailed = false;
        if (denoiser) { aovAlb.assign((size_t)w * h * 3, 0.0f); aovNrm.assign((size_t)w * h * 3, 0.0f); }
        else { aovAlb.clear(); aovNrm.clear(); }
        const double tp0 = W3dNowMs();
        if (!reuseScene) BuildRTScene(esc);
        CameraOf(vp, w, h, cam, withPreview);
        const double tp1 = W3dNowMs();
        // LA GPU: la escena sube una vez; los pases son tan rapidos que no hace falta la escalera de preview
        gpu = g_rt.gpu && RTGpuAvailable() && ((reuseScene && g_rtGpuOwner == this) || SceneToGpu(esc)) && RTGpuImage(w, h);
        if (g_rtProfile) w3dLogf("[rt perfil] armar %.1f ms, a la gpu %.1f ms (%d tris, %d texturas)", tp1 - tp0, W3dNowMs() - tp1,
                                             (int)esc.tris.size(), (int)esc.texs.size());
        aovGpu = false;
        if (gpu) { level = kPrevLevels; g_rtGpuOwner = this; }
        g_rtLastInGpu = gpu;
    }
    // la GPU la uso el otro render: la escena y la imagen de este otra vez, y la acumulacion de cero (la de la GPU se
    // perdio). false = no se pudo: sigue por CPU
    bool ResumeGpu() {
        if (g_rtGpuOwner == this) return true;
        step = 0; nextTile = 0; aovGpu = false;
        if (!(RTGpuAvailable() && SceneToGpu(esc) && RTGpuImage(w, h))) {
            gpu = false; g_rtLastInGpu = false; accum.assign(accum.size(), 0.0f);
            return false;
        }
        g_rtGpuOwner = this;
        return true;
    }
    // EN LA GPU: pases enteros hasta agotar el presupuesto; despues la imagen (y el color y los AOV si hay denoiser)
    // baja una vez. Si la GPU falla, el render sigue por CPU desde cero
    bool AdvanceGpu(double budgetMs) {
        if (!ResumeGpu()) return true;
        const double t0 = W3dNowMs();
        RTGpuCamera c;
        const int ns = (g_rt.samples < 1) ? 1 : g_rt.samples;
        double msPasses = 0.0;
        while (!Finished()) {
            if (prevOfPass) { prevOfPass(*this, prevCtx); if (!gpu) return true; }   // (si la GPU fallo, sigue por CPU)
            c.pos[0] = cam.pos.x; c.pos[1] = cam.pos.y; c.pos[2] = cam.pos.z; c.right[0] = cam.right.x; c.right[1] = cam.right.y; c.right[2] = cam.right.z;
            c.arr[0] = cam.arr.x; c.arr[1] = cam.arr.y; c.arr[2] = cam.arr.z; c.forward[0] = cam.forward.x; c.forward[1] = cam.forward.y; c.forward[2] = cam.forward.z;
            c.sx = cam.sx; c.sy = cam.sy; c.zoom = cam.zoom; c.panX = cam.panX; c.panY = cam.panY; c.ortho = cam.ortho;
            c.lens = cam.lens; c.focus = cam.focus;
            c.seed = seed;
            const double tq = W3dNowMs();
            const bool ok = RTGpuPass(c, step, ns, denoiser && step == 0, g_rt.layers, g_rt.bounces);
            msPasses += W3dNowMs() - tq;
            if (g_rtProfile) w3dLogf("[rt perfil] pase %d: %.1f ms", step, W3dNowMs() - tq);
            if (!ok) { gpu = false; g_rtLastInGpu = false; step = 0; accum.assign(accum.size(), 0.0f); return true; }
            step++;
            if (budgetMs > 0 && W3dNowMs() - t0 > budgetMs) break;
        }
        RTCountRays(RTGpuRays(), msPasses);
        // EL DENOISER: el de la GPU trabaja sobre lo acumulado sin bajarlo (va en tiempo real); los otros (OIDN, OptiX)
        // necesitan el color en la CPU: mientras se forma la imagen, como mucho cada 250 ms, y siempre al terminar
        bool den = denoiser && !denoiserFailed;
        if (den && denoiser == W3D_DENOISER_GPU) {
            if (!RTGpuDenoiseAccumulated(step)) { denoiserFailed = true; w3dLogfW("[rt] el denoiser de la GPU fallo"); }
            den = false;
        } else if (den && !Finished() && W3dNowMs() - lastDenoise < 250.0) den = false;
        if (den) { middleColor.resize((size_t)w * h * 3); clean.resize((size_t)w * h * 3); }
        const bool lowerAov = den && !aovGpu;
        if (!RTGpuRead(&img[0], den ? &middleColor[0] : 0, lowerAov ? &aovAlb[0] : 0, lowerAov ? &aovNrm[0] : 0, step)) {
            gpu = false; g_rtLastInGpu = false; step = 0; accum.assign(accum.size(), 0.0f); return true;
        }
        if (den) { aovGpu = true; ApplyDenoiser(); lastDenoise = W3dNowMs(); }
        for (size_t t = 0; t < dirtyTile.size(); t++) dirtyTile[t] = 1;
        return true;
    }
    bool Finished() const { return step >= g_rt.passes; }
    // SOLO CAMBIARON LAS POSES (la banda tocando): los vertices nuevos, el BVH reajustado y, en la GPU, solo eso de
    // nuevo (no los materiales, las texturas ni las luces); la acumulacion de cero
    void UpdatePosesYLights(Viewport3D* vp, bool poses, bool lights) {
        const double t0 = W3dNowMs();
        const bool foreign = gpu && g_rtGpuOwner != this;   // (en la GPU esta la escena del otro render: va entera)
        if (poses) {
            UpdateLive(esc);
            BVHFit(esc);
            if (gpu && !foreign) {
                std::vector<float> vs, ns; VertsAGpu(esc, vs); NodesToGpu(esc, ns);
                if (!RTGpuUpdateGeometry(vs, ns)) { gpu = false; g_rtLastInGpu = false; }
            }
        }
        if (lights) {
            esc.lights.clear(); AddLights(esc);
            if (gpu && !foreign) {
                std::vector<float> ls; LightsToGpu(esc, ls);
                if (!RTGpuUpdateLights(ls)) { gpu = false; g_rtLastInGpu = false; }
            }
        }
        if (foreign) ResumeGpu();
        if (g_rtProfile) w3dLogf("[rt perfil] %s%s %.1f ms (%d mallas, %d luces)", poses ? "mallas " : "", lights ? "lights" : "",
                                W3dNowMs() - t0, (int)esc.live.size(), (int)esc.lights.size());
        NewCamera(vp);
    }
    // la camara se movio (la escena es la misma): la acumulacion de cero con la camara nueva
    void NewCamera(Viewport3D* vp) {
        CameraOf(vp, w, h, cam, true);
        step = 0; nextTile = 0; aovGpu = false;
        NewSeed();
        if (!gpu) { accum.assign(accum.size(), 0.0f); level = 0; }   // (la CPU vuelve a la escalera de preview)
        for (size_t t = 0; t < dirtyTile.size(); t++) dirtyTile[t] = 0;
    }
    // un tile de PREVIEW: 1 rayo por bloque de s x s (al centro del bloque), pinta el bloque entero.
    // No toca el acumulador: es solo para que se vea algo enseguida; los pases reales lo pisan.
    void TilePreview(int ti) {
        const int tx = (ti % tilesX) * kTile, ty = (ti / tilesX) * kTile;
        const int x1 = (tx + kTile < w) ? tx + kTile : w, y1 = (ty + kTile < h) ? ty + kTile : h;
        const int s = kPrevSteps[level];
        for (int y = ty; y < y1; y += s) for (int x = tx; x < x1; x += s) {
            RTRng rng((unsigned int)(x * 1973 + y * 9277 + 1));
            RTV o, d; RayPixel(cam, x + s * 0.5f, y + s * 0.5f, w, h, o, d);
            float m[3]; Shade(esc, o, d, rng, m);
            unsigned char rgb[3];
            for (int k = 0; k < 3; k++) { float v = m[k]; if (v < 0) v = 0; if (v > 1) v = 1; rgb[k] = (unsigned char)(v * 255.0f + 0.5f); }
            const int bx1 = (x + s < x1) ? x + s : x1, by1 = (y + s < y1) ? y + s : y1;
            for (int yy = y; yy < by1; yy++) {
                unsigned char* p = &img[((size_t)yy * w + x) * 4];
                for (int xx = x; xx < bx1; xx++, p += 4) { p[0] = rgb[0]; p[1] = rgb[1]; p[2] = rgb[2]; p[3] = 255; }
            }
        }
        dirtyTile[(size_t)ti] = 1;
    }
    void Tile(int ti) {   // un tile del pase actual: 'samples' muestras por pixel
        if (level < kPrevLevels) { TilePreview(ti); return; }
        const int tx = (ti % tilesX) * kTile, ty = (ti / tilesX) * kTile;
        const int x1 = (tx + kTile < w) ? tx + kTile : w, y1 = (ty + kTile < h) ? ty + kTile : h;
        const int ns = (g_rt.samples < 1) ? 1 : g_rt.samples;
        const int donePasses = step + 1;
        for (int y = ty; y < y1; y++) for (int x = tx; x < x1; x++) {
            RTRng rng((unsigned int)(x * 1973 + y * 9277 + step * 26699 + 1) ^ (seed * 2654435761u));
            float c[3] = { 0, 0, 0 };
            for (int s = 0; s < ns; s++) {
                RTV o, d; RayPixel(cam, x + rng.F(), y + rng.F(), w, h, o, d); RayLens(cam, rng, o, d);
                float m[3]; Shade(esc, o, d, rng, m);
                c[0] += m[0]; c[1] += m[1]; c[2] += m[2];
            }
            float* a = &accum[((size_t)y * w + x) * 3];
            a[0] += c[0] / ns; a[1] += c[1] / ns; a[2] += c[2] / ns;
            // el DENOISER: en el primer pase, el albedo y la normal del centro del pixel; despues, la imagen que se
            // ve es la LIMPIA de cada pase terminado (los tiles con ruido no se muestran)
            if (denoiser && step == 0) {
                RTV o, d; RayPixel(cam, x + 0.5f, y + 0.5f, w, h, o, d);
                FirstImpact(esc, o, d, &aovAlb[((size_t)y * w + x) * 3], &aovNrm[((size_t)y * w + x) * 3]);
            }
            if (denoiser && !denoiserFailed && step > 0) continue;
            unsigned char* p = &img[((size_t)y * w + x) * 4];
            for (int k = 0; k < 3; k++) { float v = a[k] / (float)donePasses; if (v < 0) v = 0; if (v > 1) v = 1; p[k] = (unsigned char)(v * 255.0f + 0.5f); }
            p[3] = 255;
        }
        dirtyTile[(size_t)ti] = 1;
    }
    // avanza hasta agotar el presupuesto (ms; <= 0 = sin limite) o terminar el pase; true si hizo algo
    bool Advance(double budgetMs) {
        if (Finished()) return false;
        if (gpu) return AdvanceGpu(budgetMs);
        if (prevOfPass && nextTile == 0 && level >= kPrevLevels) { prevOfPass(*this, prevCtx); if (gpu) return AdvanceGpu(budgetMs); }
        const int total = tilesX * tilesY;
        const double t0 = W3dNowMs();
        g_rtPreviewFast = (level < kPrevLevels);   // preview: 1 rayo de sombra por luz (ver LightIn)
        int threads = (g_rt.threads > 0) ? g_rt.threads : 1;
#ifdef RT_THREADS
        if (g_rt.threads <= 0) { threads = (int)std::thread::hardware_concurrency(); if (threads < 1) threads = 1; if (threads > 16) threads = 16; }
        if (threads > 1 && total - nextTile > 1) {
            // cada hilo toma el proximo tile del contador (en orden) hasta que se acaben o se pase el tiempo; un
            // tile tomado SIEMPRE se termina -> lo hecho es el prefijo contiguo [nextTile, sig) y no hay huecos
            std::atomic<int> next(nextTile);
            const int limit = total;
            std::vector<std::thread> hs;
            for (int i = 0; i < threads; i++) hs.push_back(std::thread([this, &next, limit, t0, budgetMs]() {
                for (;;) {
                    if (budgetMs > 0 && W3dNowMs() - t0 > budgetMs) break;
                    const int ti = next.fetch_add(1);
                    if (ti >= limit) break;
                    Tile(ti);
                }
            }));
            for (size_t i = 0; i < hs.size(); i++) hs[i].join();
            int done = next.load(); if (done > total) done = total;
            nextTile = done;
        } else
#endif
        {
            // en el N95 W3dNowMs es User::NTickCount (~ms): el presupuesto corta bien. El tope de tiles
            // por llamada queda como red de seguridad por si el reloj no avanza (t0 en 0).
            const bool noClock = (t0 <= 0.0);
            int doneCount = 0;
            while (nextTile < total) {
                Tile(nextTile); nextTile++; doneCount++;
                if (budgetMs > 0 && W3dNowMs() - t0 > budgetMs) break;
                if (budgetMs > 0 && noClock && doneCount >= 4) break;
            }
        }
        if (nextTile >= total) { nextTile = 0; if (level < kPrevLevels) level++; else { step++; Denoise(); } }
        return true;
    }
    // un pase TERMINADO: el promedio de lo acumulado pasa por el denoiser y eso es lo que se ve (o se guarda)
    void Denoise() {
        if (!denoiser || denoiserFailed || step <= 0) return;
        const size_t n = (size_t)w * h * 3;
        middleColor.resize(n); clean.resize(n);
        const float inv = 1.0f / (float)step;
        for (size_t i = 0; i < n; i++) { float v = accum[i] * inv; middleColor[i] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
        ApplyDenoiser();
    }
    // middleColor (el promedio de los pases) -> el denoiser -> la imagen que se ve
    void ApplyDenoiser() {
        std::string err;
        if (!W3dDenoiserApply(denoiser, &middleColor[0], aovAlb.empty() ? 0 : &aovAlb[0], aovNrm.empty() ? 0 : &aovNrm[0], w, h, &clean[0], &err)) {
            denoiserFailed = true;   // (se sigue viendo el render sin limpiar)
            w3dLogfW("[rt] el denoiser fallo: %s", err.c_str());
            for (size_t i = 0; i < (size_t)w * h; i++) {
                unsigned char* p = &img[i * 4];
                for (int k = 0; k < 3; k++) p[k] = (unsigned char)(middleColor[i * 3 + k] * 255.0f + 0.5f);
            }
        } else {
            for (size_t i = 0; i < (size_t)w * h; i++) {
                unsigned char* p = &img[i * 4];
                for (int k = 0; k < 3; k++) { float v = clean[i * 3 + k]; if (v < 0) v = 0; if (v > 1) v = 1; p[k] = (unsigned char)(v * 255.0f + 0.5f); }
                p[3] = 255;
            }
        }
        for (size_t t = 0; t < dirtyTile.size(); t++) dirtyTile[t] = 1;
    }
};

// ---------------------------------------------------------------------------
//  viewport: textura de pantalla + firma para reiniciar cuando cambia la vista o la escena
// ---------------------------------------------------------------------------
static RTRender* g_rtVp = NULL;
static const Viewport3D* g_rtVpOf = NULL;
// el viewport del preview MURIO (abrir un proyecto libera el layout anterior): olvidarlo. El
// puntero solo se compara, pero con la direccion reciclada un 3D NUEVO en la misma direccion
// seguiria el preview del muerto en vez de reiniciarlo (LayoutTree.h).
static void RTForgetViewport(ViewportBase* vp) {
    if (g_rtVpOf && (const ViewportBase*)g_rtVpOf == vp) g_rtVpOf = NULL;
}
struct RTHookForget { RTHookForget() { ViewportForgetRegister(RTForgetViewport); } };
static RTHookForget g_rtHookForget;
static unsigned int g_rtTexId = 0; static int g_rtTexW = 0, g_rtTexH = 0;
static bool g_rtTexLinear = false;
static double g_rtSignature = 0.0, g_rtSignatureCam = 0.0, g_rtSignaturePoses = 0.0, g_rtSignatureLights = 0.0;
static bool g_rtForce = false;

static int Pot(int v) { int p = 1; while (p < v) p <<= 1; return p; }
// LA CAMARA y LA ESCENA por separado: si solo se movio la camara (orbitar, acercarse) la escena armada se queda (el
// BVH, las texturas y, en la GPU, todo lo subido) y solo se vuelve a empezar la acumulacion
static double SignatureCamera(Viewport3D* vp) {
    CameraBase cam = vp->ViewCam();
    double f = cam.pos.x * 1.1 + cam.pos.y * 2.3 + cam.pos.z * 3.7 + cam.rot.x * 5.1 + cam.rot.y * 7.3 + cam.rot.z * 11.7 + cam.rot.w * 13.1;
    f += vp->orbitDistance * 0.37 + (vp->orthographic ? 1000.0 : 0.0) + fovDeg * 0.01 + (vp->ViewFromCameraActive ? 7.7 : 0.0);
    const Viewport3D::Lens L = vp->CurrentLens();
    f += L.sx * 3.3 + L.sy * 5.9 + L.zoom * 11.3 + L.panX * 13.7 + L.panY * 17.9;
    // (la profundidad de campo: un foco animado tiene que volver a empezar la imagen)
    if (CameraActive && CameraActive->dof) f += CameraActive->LensRadius() * 101.3 + CameraActive->DistanceFocus() * 0.913 + 3.1;
    return f;
}
// las POSES de las mallas con esqueleto (cambian en cada cuadro de la banda: no hace falta armar la escena de nuevo)
// LAS LUCES: donde estan, hacia donde y como alumbran (un lightshow las cambia en cada cuadro: solo se vuelven a
// mandar ellas)
static double SignatureLights() {
    double f = 0.0;
    for (size_t i = 0; i < Lights.size(); i++) if (Lights[i]) {
        Light* l = Lights[i]; Matrix4 W; l->GetWorldMatrix(W);
        f += l->rtRadius * 3.3 + l->rtRays * 0.7 + l->diffuse[0] * 1.1 + l->diffuse[1] * 2.3 + l->diffuse[2] * 3.7 + (l->directional ? 9.0 : 0.0)
           + l->ambient[0] * 0.41 + l->ambient[1] * 0.43 + l->ambient[2] * 0.47 + l->spotCutoff * 0.013 + l->spotExponent * 0.17
           + l->intensity * 1.7 + l->distance * 2.3
           + l->attConstant * 0.19 + l->attLinear * 0.23 + l->attQuadratic * 0.29 + (l->visible ? 0.77 : 0.0)
           + W.m[12] * 0.31 + W.m[13] * 0.53 + W.m[14] * 0.71 + W.m[4] * 0.13 + W.m[5] * 0.17 + W.m[8] * 0.37 + W.m[10] * 0.59;
        f *= 1.0000013;   // (que importe cual es cual)
    }
    return f;
}
static double SignaturePoses() {
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
static double SignatureView(Viewport3D* vp) {
    double f = vp->width * 0.001 + vp->height * 0.0013;
    // escena: cuantos objetos y donde estan (posiciones de mundo) + luces + version de las mallas
    struct S { static void Rec(Object* o, double& f) { if (!o) return; if (o->getType() == ObjectType::mesh) { Mesh* m = (Mesh*)o; f += m->geoVersion * 0.019 + (m->genValid ? 0.5 : 0.0); }
                 const ObjectType t = o->getType();
                 // (las luces van en SignatureLights, la camara en SignatureCamera y donde esta cada malla en SignaturePoses: aca
                 // solo lo que esta y si se ve)
                 if (t != ObjectType::light && t != ObjectType::camera) f += (o->visible ? 0.7 : 0.0) + 0.013;
                 f *= 1.0000003;
                 for (size_t i = 0; i < o->Childrens.size(); i++) Rec(o->Childrens[i], f); } };
    S::Rec(SceneCollection, f);
    f += (double)Lights.size() * 9.1;   // (cuantas: agregar o borrar una arma de nuevo)
    if (g_localViewActive && g_visibleLocalView) f += 77.7 + (double)g_visibleLocalView->size() * 1.31;   // (la vista local)
    f += g_rt.bounces * 5.3;   // (los rebotes de la luz)
    f += g_renderBgMode * 3.7 + (g_renderBgTex ? g_renderBgTex->iID * 0.023 : 0.0) + g_renderBgRot[0] * 0.011 + g_renderBgRot[1] * 0.013 + g_renderBgRot[2] * 0.017;   // (el fondo 360)
    f += (g_rt.seedVariable ? 0.33 : 0.0) + g_rt.samples * 0.101 + g_rt.rays * 0.047 + g_rt.passes * 0.0071 + g_renderBg[0] + g_renderBg[1] * 2 + g_renderBg[2] * 3 + g_rt.denoiser * 17.3 + (g_rt.gpu ? 23.9 : 0.0);
    for (size_t i = 0; i < Materials.size(); i++) if (Materials[i]) { Material* mt = Materials[i];
        f += mt->diffuse[0] * 0.21 + mt->diffuse[1] * 0.23 + mt->diffuse[2] * 0.29 + mt->diffuse[3] * 0.37 + (mt->transparent ? 1.7 : 0.0) + mt->mix * 0.13 + mt->rtRoughness * 0.41 + mt->rtMetallic * 0.43 + mt->emission[0] + mt->emission[1] + mt->emission[2] + (mt->lighting ? 0.3 : 0.0) + (mt->textureOn ? 0.9 : 0.0) + (mt->texture ? mt->texture->iID * 0.011 : 0.0) + (mt->normalMap ? 2.3 : 0.0) + (mt->normalTexture ? mt->normalTexture->iID * 0.017 : 0.0);
        // (los MAPAS: numero / textura, su textura y el IOR tambien cambian lo que se ve)
        for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++)
            f += (mt->mapWithTexture[k] ? 3.1 * (k + 1) : 0.0) + (mt->mapTex[k] ? mt->mapTex[k]->iID * 0.019 * (k + 1) : 0.0);
        f += mt->ior * 0.47 + mt->transmission * 0.53; }
    return f;
}
void RTInvalidate() { g_rtForce = true; }
static double g_rtLastImage = 0.0;
static float g_rtImgBySeg = 0.0f;
static void RTCountImage() {
    const double t = W3dNowMs();
    if (g_rtLastImage > 0.0) {
        const double dt = t - g_rtLastImage;
        if (dt > 0.0 && dt < 5000.0) {
            const float fps = (float)(1000.0 / dt);
            g_rtImgBySeg = (g_rtImgBySeg <= 0.0f) ? fps : g_rtImgBySeg * 0.8f + fps * 0.2f;
        }
    }
    g_rtLastImage = t;
}
float RTImagesPerSecond() {
    // (sin imagenes nuevas hace rato: se va a cero, no se queda con lo ultimo)
    if (g_rtLastImage > 0.0 && W3dNowMs() - g_rtLastImage > 2000.0) return 0.0f;
    return g_rtImgBySeg;
}
bool RTActiveIn(const Viewport3D* vp) { return g_rt.on && g_rtVp && g_rtVpOf == vp && !g_rtVp->Finished(); }

const char* RTUpscalerName(int r) { return r == 1 ? "FSR 1 (AMD)" : "Linear"; }
// el REESCALADO con FSR (g_rt.upscaler = 1): la imagen trazada chica -> la del viewport, por CPU, y esa se sube entera.
// Mientras se va formando, a lo sumo cada ~60 ms (cuesta como un pase): lo que no se subio queda sucio para el siguiente
static std::vector<unsigned char> g_rtFsr;
static int g_rtFsrW = 0, g_rtFsrH = 0;
static double g_rtLastFsr = 0.0;
static void UploadFSR(RTRender* r, bool finished) {
    bool dirty = false;
    for (size_t t = 0; t < r->dirtyTile.size() && !dirty; t++) dirty = r->dirtyTile[t] != 0;
    if (!dirty) return;
    const double now = W3dNowMs();
    if (!finished && now - g_rtLastFsr < 60.0) { g_redraw = true; return; }
    g_rtLastFsr = now;
    g_rtFsr.resize((size_t)g_rtFsrW * g_rtFsrH * 4);
    W3dFSR1(&r->img[0], r->w, r->h, &g_rtFsr[0], g_rtFsrW, g_rtFsrH, 0.2f, g_rt.threads);
    w3dEngine::UpdateRGBA(g_rtTexId, 0, 0, g_rtFsrW, g_rtFsrH, &g_rtFsr[0], false);
    for (size_t t = 0; t < r->dirtyTile.size(); t++) r->dirtyTile[t] = 0;
}
static void UploadTiles(RTRender* r) {
    if (!g_rtTexId) return;
    // (todo sucio, lo de la GPU: la imagen entera de una vez y no ~2000 tiles de 32x32)
    bool all = !r->dirtyTile.empty();
    for (size_t t = 0; t < r->dirtyTile.size() && all; t++) all = r->dirtyTile[t] != 0;
    if (all) {
        w3dEngine::UpdateRGBA(g_rtTexId, 0, 0, r->w, r->h, &r->img[0], false);
        for (size_t t = 0; t < r->dirtyTile.size(); t++) r->dirtyTile[t] = 0;
        return;
    }
    for (int ti = 0; ti < r->tilesX * r->tilesY; ti++) {
        if (!r->dirtyTile[(size_t)ti]) continue;
        r->dirtyTile[(size_t)ti] = 0;
        const int tx = (ti % r->tilesX) * kTile, ty = (ti / r->tilesX) * kTile;
        const int tw = (tx + kTile < r->w) ? kTile : r->w - tx, th = (ty + kTile < r->h) ? kTile : r->h - ty;
        std::vector<unsigned char> tmp((size_t)tw * th * 4);
        for (int y = 0; y < th; y++) memcpy(&tmp[(size_t)y * tw * 4], &r->img[((size_t)(ty + y) * r->w + tx) * 4], (size_t)tw * 4);
        w3dEngine::UpdateRGBA(g_rtTexId, tx, ty, tw, th, &tmp[0]);
    }
}
void RTViewportStep(Viewport3D* vp) {
    if (!g_rt.on || !vp || vp->width <= 0 || vp->height <= 0) return;
    const double signature = SignatureView(vp), signatureCam = SignatureCamera(vp), signaturePoses = SignaturePoses(), signatureLights = SignatureLights();
    // la RESOLUCION del trazado: la del viewport; en el telefono, un viewport grande (la TV a 720p) se traza a la mitad
    // y se estira: a 1280x720 la textura de potencia de 2 era de 2048x1024 (8 MB) en una VideoCore que ya andaba justa
    // de memoria con la superficie de la TV (y el preview salia NEGRO), y el CPU tardaba 4 veces mas por pase
    int rw = vp->width, rh = vp->height;
#ifdef W3D_SYMBIAN
    while (rw > 640) { rw /= 2; rh /= 2; }
    if (rh < 1) rh = 1;
#endif
    // el REESCALADO (g_rt.scale): menos pixeles que trazar, la textura se estira con filtro lineal
    const int esc = g_rt.scale < 1 ? 1 : (g_rt.scale > 8 ? 8 : g_rt.scale);
    const int vw = rw, vh = rh;   // (lo que se ve: el destino del reescalado)
    if (esc > 1) { rw /= esc; rh /= esc; if (rw < 1) rw = 1; if (rh < 1) rh = 1; }
#ifdef W3D_SYMBIAN
    const bool fsr = false;       // (a 333 MHz cuesta mas que el pase: lineal)
#else
    const bool fsr = esc > 1 && g_rt.upscaler == 1;
#endif
    // la textura: la imagen trazada tal cual (estirada al dibujarla) o, con FSR, ya agrandada al tamano de la vista
    const int texW = fsr ? vw : rw, texH = fsr ? vh : rh;
    if (fsr && (g_rtFsrW != vw || g_rtFsrH != vh)) { g_rtFsrW = vw; g_rtFsrH = vh; g_rtForce = true; }
    if (!fsr && g_rtFsrW) { g_rtFsrW = g_rtFsrH = 0; g_rtFsr.clear(); g_rtForce = true; }
    if (!g_rtVp || g_rtVpOf != vp || g_rtVp->w != rw || g_rtVp->h != rh || signature != g_rtSignature || g_rtForce) {
        if (!g_rtVp) g_rtVp = new RTRender();
        g_rtVp->Start(vp, rw, rh, true);   // con la escalera de preview (40p/60p/120p en un viewport de 240p)
        g_rtVpOf = vp; g_rtSignature = signature; g_rtSignatureCam = signatureCam; g_rtSignaturePoses = signaturePoses; g_rtSignatureLights = signatureLights; g_rtForce = false;
        // textura de pantalla, potencia de 2 (GLES 1.1 del N95 no tiene NPOT), sin mips
        const int tw = Pot(texW), th = Pot(texH);
        const bool linear = esc > 1 && !fsr;   // (a resolucion completa o ya agrandada con FSR, pixel a pixel)
        if (!g_rtTexId || tw != g_rtTexW || th != g_rtTexH || linear != g_rtTexLinear) {
            if (g_rtTexId) w3dEngine::DeleteTexture(g_rtTexId);
            g_rtTexLinear = linear;
            // negro OPACO: la imagen trazada no tiene transparencia (en la Dreamcast una textura opaca va en RGB565 y no
            // en ARGB4444, que dejaba bandas marcadas en la luz)
            std::vector<unsigned char> empty((size_t)tw * th * 4, 0);
            for (size_t i = 3; i < empty.size(); i += 4) empty[i] = 255;
            g_rtTexId = w3dEngine::UploadRGBA(&empty[0], tw, th, linear, false);
            g_rtTexW = tw; g_rtTexH = th;
            if (!g_rtTexId) w3dLogf("[rt] no se pudo crear la textura del preview de %dx%d (memoria grafica?)", tw, th);
        }
    } else if (g_rt.onlyFinal && !g_rtVp->Finished()) {
        // "SOLO LA IMAGEN FINAL": la que se esta trazando se termina antes de pasar a lo que cambio (si cada cambio la
        // empezara de nuevo, con Play nunca terminaria ninguna y la vista quedaba congelada en la ultima)
    } else if (signaturePoses != g_rtSignaturePoses || signatureLights != g_rtSignatureLights) {
        // las poses y/o las luces (y quiza la camara): solo eso de nuevo
        g_rtVp->UpdatePosesYLights(vp, signaturePoses != g_rtSignaturePoses, signatureLights != g_rtSignatureLights);
        g_rtSignaturePoses = signaturePoses; g_rtSignatureLights = signatureLights; g_rtSignatureCam = signatureCam;
    } else if (signatureCam != g_rtSignatureCam) {   // solo la camara: la misma escena, la acumulacion de cero
        g_rtVp->NewCamera(vp);
        g_rtSignatureCam = signatureCam;
    }
    // avanza con un presupuesto por frame: la UI sigue viva y la imagen se completa de a tiles.
    // En el N95 el presupuesto es mayor: cada frame ademas paga el render GL de la escena, asi que
    // con 40ms la mitad del tiempo se iba en dibujar lo que la imagen trazada tapa; con 80ms el
    // render termina antes y la UI sigue usable (~8 fps mientras traza).
#ifdef W3D_SYMBIAN
    const double kBudgetMs = 80.0;
#else
    // (en la GPU cada pase es de pocos ms: con un cuadro de 60 Hz la vista sigue a la camara en tiempo real)
    const double kBudgetMs = g_rtVp->gpu ? 14.0 : 40.0;
#endif
    if (!g_rtVp->Finished()) { g_rtVp->Advance(kBudgetMs); g_redraw = true; if (g_rtVp->Finished()) RTCountImage(); }
    // "solo la imagen final": la textura se queda con la ultima terminada hasta que esta tambien termine
    if (!g_rt.onlyFinal || g_rtVp->Finished()) { if (fsr) UploadFSR(g_rtVp, g_rtVp->Finished()); else UploadTiles(g_rtVp); }
    {   // (diagnostico del preview NEGRO en la TV del N8: unas lineas al log, espaciadas)
        static int sLog = 0, sFrame = 0;
        if (sLog < 12 && (sFrame++ % 20) == 0) {
            sLog++;
            const size_t c = ((size_t)(g_rtVp->h / 2) * g_rtVp->w + g_rtVp->w / 2) * 4;
            const int e = (int)w3dEngine::ErrorGL();
            w3dLogf("[rt] %dx%d (viewport %dx%d) tex=%u %dx%d step=%d/%d lights=%d centro=(%d,%d,%d) glErr=0x%x",
                    g_rtVp->w, g_rtVp->h, vp->width, vp->height, g_rtTexId, g_rtTexW, g_rtTexH, g_rtVp->step, g_rt.passes,
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
bool RTRenderState(Viewport3D* vp) {
    if (!g_rt.on || !g_rtVp || g_rtVpOf != vp) return false;
    char b[160];
    // (en la vista local, adelante: "Local View - Ray Tracing: 3/16")
    char pre[64] = ""; if (vp->localViewActive) snprintf(pre, sizeof(pre), "%s - ", T("Local View"));
    if (g_rtVp->Finished()) snprintf(b, sizeof(b), "%s%s: %s (%d)", pre, T("Ray Tracing"), T("done"), g_rtVp->step);
    else if (g_rtVp->level < kPrevLevels) snprintf(b, sizeof(b), "%s%s: %s %d/%d", pre, T("Ray Tracing"), T("preview"), g_rtVp->level + 1, kPrevLevels);
    else snprintf(b, sizeof(b), "%s%s: %d/%d", pre, T("Ray Tracing"), g_rtVp->step, g_rt.passes);
    W3dDialogView(vp, b);
    return true;
}
void W3dDialogView(Viewport3D* vp, const char* b) {
    w3dEngine::PushMatrix();
    // una linea debajo del borde superior (la fila 0 del viewport queda tapada por el borde/menu)
    w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)(vp->BarTopOffset() + gapGS * 2 + LetterHeightGS), 0);
    SetColorID(ColorID::white, 1.0f);
    RenderBitmapText(b, textAlign::left, vp->width - gapGS * 4);
    w3dEngine::PopMatrix();
}

// ---------------------------------------------------------------------------
//  render a archivo y harness (buffer propio, sincronico)
// ---------------------------------------------------------------------------
static RTRender* g_rtOff = NULL;

// ---------------------------------------------------------------------------
//  DESENFOQUE DE MOVIMIENTO TRAZADO (g_rt.blur): la escena en los cuadros f-1, f y f+1 (los vertices de las mallas
//  que se mueven y la camara) y, antes de cada pase, el instante de ese pase dentro del obturador interpolado entre
//  ellos. Las mallas son las mismas: solo se reajustan las cajas del BVH. Las luces quedan como en el cuadro f
// ---------------------------------------------------------------------------
const char* RTShutterPosName(int pos) {
    return pos == 1 ? "Start of frame" : pos == 2 ? "End of frame" : "Center of frame";
}
struct RTMotion {
    std::vector<float> W[3];   // la matriz de mundo de cada malla viva (16 por malla) en los cuadros f-1, f, f+1
    RTCamera cam[3];
    float t0, t1;              // el obturador, en cuadros desde f
    int f;                     // el cuadro que se renderiza
};
static RTMotion g_rtMov;
// las poses de ESTE cuadro en las mallas vivas (el esqueleto / la vertex anim) y sus vertices de mundo en la escena
static void PosesToFrame(RTScene& e) {
    for (size_t k = 0; k < e.live.size(); k++) {
        Mesh* m = e.live[k].m;
        if (m->skinArmature) SkinMesh(m); else W3dUpToDatePose(m);
    }
    UpdateLive(e);
}
// (la pose cacheada de cada esqueleto ya no sirve: se evaluo en otro instante)
static void ForgetPoses(RTScene& e) {
    for (size_t k = 0; k < e.live.size(); k++) if (e.live[k].m->skinArmature) e.live[k].m->skinArmature->lastPoseFrame = -999999;
}
static void CameraToFrame() { if (CameraActive) { CameraActive->UpdatePosition(); CameraActive->UpdateLookAt(); } }
static void MotionBuild(RTRender& r, Viewport3D* vp, int w, int h, RTMotion& mv) {
    extern int CurrentFrame; extern void ApplyAnimationObjects();
    float s = g_rt.shutter; if (s < 0.01f) s = 0.01f; if (s > 1.0f) s = 1.0f;
    if (g_rt.shutterPos == 1)      { mv.t0 = 0.0f; mv.t1 = s; }
    else if (g_rt.shutterPos == 2) { mv.t0 = -s;   mv.t1 = 0.0f; }
    else                             { mv.t0 = -s * 0.5f; mv.t1 = s * 0.5f; }
    const int f = CurrentFrame; mv.f = f;
    const size_t nv = r.esc.live.size();
    for (int k = 0; k < 3; k++) {
        CurrentFrame = f + k - 1; ApplyAnimationObjects(); CameraToFrame();
        mv.W[k].resize(nv * 16);
        for (size_t i = 0; i < nv; i++) { Matrix4 M; r.esc.live[i].m->GetWorldMatrix(M); memcpy(&mv.W[k][i * 16], M.m, sizeof(M.m)); }
        CameraOf(vp, w, h, mv.cam[k], false);
    }
    CurrentFrame = f; ApplyAnimationObjects(); CameraToFrame();
    ForgetPoses(r.esc); PosesToFrame(r.esc);
    w3dLogf("[rt] desenfoque: obturador %.2f..%.2f cuadros, %d mallas vivas", mv.t0, mv.t1, (int)nv);
}
static inline RTV Mix(const RTV& a, const RTV& b, float u) { return a + (b - a) * u; }
// la escena y la camara en el instante x (en cuadros desde f, -1..1): el ESQUELETO evaluado en ese instante (con las
// rotaciones interpoladas) y la matriz de cada malla y la camara entre los cuadros enteros de al lado
static void MotionIn(RTRender& r, const RTMotion& mv, float x) {
    extern int CurrentFrame;
    int fi = (int)floorf(x); if (fi < -1) fi = -1; if (fi > 0) fi = 0;   // (x = 1: el final del tramo 0..1)
    const float u = x - (float)fi;                                         // 0..1 entre f+fi y f+fi+1
    const int k0 = fi + 1, k1 = k0 + 1;
    const int cfPrev = CurrentFrame;
    CurrentFrame = mv.f + fi;
    g_poseFraction = (u > 1e-4f && u < 1.0f) ? u : 0.0f;
    const int cfPose = (u >= 1.0f) ? mv.f + fi + 1 : mv.f + fi;
    CurrentFrame = cfPose;
    ForgetPoses(r.esc);
    for (size_t i = 0; i < r.esc.live.size(); i++) {
        RTScene::Live& v = r.esc.live[i];
        Mesh* m = v.m;
        if (m->skinArmature) SkinMesh(m); else W3dUpToDatePose(m);
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
        v.pose = 0xFFFFFFFFu;   // (lo de UpdateLive ya no vale: la proxima vez rehace los vertices)
    }
    g_poseFraction = 0.0f;
    CurrentFrame = cfPrev;
    BVHFit(r.esc);
    const float uc = (u > 1.0f) ? 1.0f : u;
    const RTCamera& c0 = mv.cam[k0], & c1 = mv.cam[k1];
    r.cam = c0;
    r.cam.pos   = Mix(c0.pos, c1.pos, uc);
    r.cam.right = Norm(Mix(c0.right, c1.right, uc)); r.cam.arr = Norm(Mix(c0.arr, c1.arr, uc)); r.cam.forward = Norm(Mix(c0.forward, c1.forward, uc));
    if (r.gpu) {
        std::vector<float> vs, ns; VertsAGpu(r.esc, vs); NodesToGpu(r.esc, ns);
        if (!RTGpuUpdateGeometry(vs, ns)) { r.gpu = false; g_rtLastInGpu = false; r.step = 0; r.accum.assign(r.accum.size(), 0.0f); }
    }
}
// el gancho: el instante de ESTE pase (estratificado: cada pase en su tramo del obturador, con un corrimiento al azar
// dentro del tramo para que no queden copias marcadas)
static void PassMotion(RTRender& r, void* ctx) {
    const RTMotion& mv = *(const RTMotion*)ctx;
    const int n = (g_rt.passes > 0) ? g_rt.passes : 1;
    unsigned int hsh = (unsigned int)r.step * 2654435761u + r.seed * 40503u + 12345u; hsh ^= hsh >> 15; hsh *= 2246822519u; hsh ^= hsh >> 13;
    const float jit = (float)(hsh & 0xFFFF) / 65536.0f;
    const float t = ((float)r.step + jit) / (float)n;
    MotionIn(r, mv, mv.t0 + (mv.t1 - mv.t0) * t);
}

// (prog0..prog1: la parte de la barra de progreso que le toca, para las franjas)
static bool RenderComplete(Viewport3D* vp, int w, int h, int passes, bool progress, bool reuseScene = false,
                           float prog0 = 0.0f, float prog1 = 1.0f) {
    if (!vp || w <= 0 || h <= 0) return false;
    if (!g_rtOff) g_rtOff = new RTRender();
    const int passesPrev = g_rt.passes; g_rt.passes = (passes > 0) ? passes : g_rt.passes;
    g_rtOff->Start(vp, w, h, false, reuseScene);   // a archivo: sin preview, directo a resolucion completa
    if (g_rt.blur && g_rt.shutter > 0.0f) {
        MotionBuild(*g_rtOff, vp, w, h, g_rtMov);
        g_rtOff->prevOfPass = PassMotion; g_rtOff->prevCtx = &g_rtMov;
    }
    while (!g_rtOff->Finished()) {
        g_rtOff->Advance(0.0);
        if (progress) ProgressUpdate(prog0 + (prog1 - prog0) * (float)g_rtOff->step / (float)g_rt.passes);
        if (progress && ProgressCanceled()) break;   // (el boton Cancelar de la barra)
    }
    if (g_rtOff->prevOfPass) { ForgetPoses(g_rtOff->esc); PosesToFrame(g_rtOff->esc); }   // (la pose del cuadro, otra vez)
    g_rtOff->prevOfPass = NULL; g_rtOff->prevCtx = NULL;
    g_rt.passes = passesPrev;
    return true;
}
bool RTRenderToPNG(Viewport3D* vp, int w, int h, const std::string& path, std::string& msg) {
    if (!RenderComplete(vp, w, h, 0, true)) { msg = "Ray Tracing: nothing to render"; return false; }
    if (!w3dEngine::SavePNG(path.c_str(), &g_rtOff->img[0], w, h, false)) { msg = "Ray Tracing: could not save"; return false; }
    msg.clear();
    return true;
}
// EL TRAZADO POR FRANJAS DIRECTO AL ARCHIVO: la imagen de w x h de a franjas de 'stripeHeight' filas, cada una a 'fn' en
// cuanto termina (todos sus pases). En memoria hay solo una franja: el N95 y el N8 trazan en 4K escribiendo en la
// tarjeta. La escena se arma una vez
bool RTRenderStripes(Viewport3D* vp, int w, int h, int stripeHeight, W3dStripeFn fn, void* ctx, std::string& msg) {
    if (!vp || w <= 0 || h <= 0 || !fn) { msg = "Ray Tracing: nothing to render"; return false; }
    if (stripeHeight < 1) stripeHeight = 32;
    bool ok = true;
    for (int y0 = 0; y0 < h && ok && !ProgressCanceled(); y0 += stripeHeight) {
        const int bh = (y0 + stripeHeight <= h) ? stripeHeight : (h - y0);
        g_rtStripeY0 = y0; g_rtStripeHeight = h;
        const bool done = RenderComplete(vp, w, bh, 0, true, y0 > 0, (float)y0 / (float)h, (float)(y0 + bh) / (float)h);
        g_rtStripeY0 = 0; g_rtStripeHeight = 0;
        if (!done) { msg = "Ray Tracing: nothing to render"; return false; }
        if (!fn(ctx, &g_rtOff->img[0], y0, bh)) { msg = "Ray Tracing: could not save"; ok = false; }
    }
    if (ok) msg.clear();
    return ok;
}
bool RTRenderToBuffer(Viewport3D* vp, int w, int h, std::vector<unsigned char>& rgba, std::string& msg) {
    if (!RenderComplete(vp, w, h, 0, true)) { msg = "Ray Tracing: nothing to render"; return false; }
    rgba = g_rtOff->img;
    msg.clear();
    return true;
}
bool RTHarnessRender(Viewport3D* vp, int w, int h, int passes) { return RenderComplete(vp, w, h, passes, false); }
bool RTHarnessPixel(int x, int y, unsigned char* rgb) {
    if (!g_rtOff || x < 0 || y < 0 || x >= g_rtOff->w || y >= g_rtOff->h) return false;
    const unsigned char* p = &g_rtOff->img[((size_t)y * g_rtOff->w + x) * 4];
    rgb[0] = p[0]; rgb[1] = p[1]; rgb[2] = p[2];
    return true;
}
bool RTHarnessSave(const std::string& path) {
    if (!g_rtOff) return false;
    return w3dEngine::SavePNG(path.c_str(), &g_rtOff->img[0], g_rtOff->w, g_rtOff->h, false);
}
int RTHarnessTriangles() { return g_rtOff ? (int)g_rtOff->esc.tris.size() : 0; }
bool RTHarnessFsrView(int& w, int& h, unsigned char* rgb) {
    if (g_rtFsr.empty() || g_rtFsrW <= 0 || g_rtFsrH <= 0) return false;
    w = g_rtFsrW; h = g_rtFsrH;
    const unsigned char* p = &g_rtFsr[((size_t)(h / 2) * w + w / 2) * 4];
    rgb[0] = p[0]; rgb[1] = p[1]; rgb[2] = p[2];
    return true;
}
bool RTHarnessInGpu() { return g_rtOff && g_rtOff->gpu; }
bool RTHarnessImage(std::vector<unsigned char>& rgba, int& w, int& h) {
    if (!g_rtOff || g_rtOff->img.empty()) return false;
    rgba = g_rtOff->img; w = g_rtOff->w; h = g_rtOff->h;
    return true;
}
