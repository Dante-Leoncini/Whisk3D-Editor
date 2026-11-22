// ============================================================================
//  PostProcess.cpp - la pila de efectos de postprocesado (ver PostProcess.h): por GPU con shaders, por CPU sin ellos.
// ============================================================================
#include "render/PostProcess.h"
#include "render/MotionVectors.h"   // W3dMotionBlur (el de la CPU)
#include "objects/Camera.h"         // el foco de la camara activa
#include "graphics/w3dGraphics.h"
#include "graphics/w3dTexture.h"              // ReserveScreen / CopyScreen
#include <math.h>
#include <string.h>
#include "app/W3dNoNearFar.h"   // (Windows: windows.h define near/far como macros vacias)

static W3dPostOptions gEmptyPost;
static const W3dPostOptions* gPostCur = &gEmptyPost;
void W3dPostUse(const W3dPostOptions* o) { gPostCur = o ? o : &gEmptyPost; }
const W3dPostOptions& W3dCurrentPost() { return *gPostCur; }
#define g_post (*gPostCur)

const char* W3dPostName(int type) {
    return type == W3dPostGrain ? "Film grain" : type == W3dPostFocus ? "Depth of field" :
           type == W3dPostMotion ? "Motion blur" : type == W3dPostBloom ? "Bloom" : "?";
}
int W3dPostNumbers(int type) { return type == W3dPostBloom ? 6 : 4; }
void W3dPostDefaults(W3dPostEffect& e, int type) {
    e.type = type; e.active = true; e.opt = (type == W3dPostFocus);
    for (int k = 0; k < 8; k++) e.v[k] = 0.0f;
    e.col[0] = e.col[1] = e.col[2] = e.col[3] = 1.0f;
    if (type == W3dPostGrain)           { e.v[0] = 0.12f; e.v[1] = 1.0f; }
    else if (type == W3dPostBloom)      { e.v[0] = 1.0f; e.v[1] = 0.8f; e.v[2] = 0.5f; e.v[3] = 1.0f; e.v[4] = 5.0f; e.v[5] = 0.0f; }
    else if (type == W3dPostFocus)       { e.v[0] = 5.0f; e.v[1] = 1.0f; e.v[2] = 1.5f; }
    else if (type == W3dPostMotion) { e.v[0] = 0.5f; }
}
// tildado y que HACE algo: el desenfoque de campo con el foco de la camara y la camara sin profundidad de campo no
// cuenta (ni copia la pantalla ni pide el pase de profundidad)
static bool Live(const W3dPostEffect& e) {
    if (!e.active) return false;
    if (e.type == W3dPostFocus && e.opt && !(CameraActive && CameraActive->dof)) return false;
    return true;
}
bool W3dPostHas() {
    if (!g_post.on) return false;
    for (size_t i = 0; i < g_post.effects.size(); i++) if (Live(g_post.effects[i])) return true;
    return false;
}
static bool HasFrom(int type) {
    if (!g_post.on) return false;
    for (size_t i = 0; i < g_post.effects.size(); i++) if (Live(g_post.effects[i]) && g_post.effects[i].type == type) return true;
    return false;
}
bool W3dPostNeedsDepth() { return HasFrom(W3dPostFocus); }
bool W3dPostNeedsMotion() { return HasFrom(W3dPostMotion); }
bool W3dPostByCPU() { return !w3dEngine::PassesWithShader(); }
float W3dPostDistanceFocus(const W3dPostEffect& e) {
    if (e.opt && CameraActive && CameraActive->dof) return CameraActive->DistanceFocus();
    return e.v[0] > 0.01f ? e.v[0] : 0.01f;
}
bool W3dPostFocusNumbers(const W3dPostEffect& e, int h, float& focus, float& range, float& radiusMax) {
    range = e.v[1];
    radiusMax = e.v[2] * 0.01f * (float)h;
    if (!e.opt) { focus = W3dPostDistanceFocus(e); return radiusMax > 0.5f; }
    if (!CameraActive || !CameraActive->dof) return false;   // la camara es un agujerito: todo nitido
    focus = CameraActive->DistanceFocus();
    // el circulo de confusion de lo que esta en el infinito: la lente vista desde el foco, en la pantalla
    const float lens = CameraActive->LensRadius();
    const float t = tanf(CameraActive->fov * 0.5f * 3.14159265f / 180.0f);
    if (lens <= 0.0f || t < 1e-4f) return false;
    radiusMax = lens / (focus * t) * 0.5f * (float)h;
    if (radiusMax > 0.2f * (float)h) radiusMax = 0.2f * (float)h;   // (un tope: el disco de muestras no da para mas)
    return radiusMax > 0.5f;
}

// ---------------------------------------------------------------------------
//  POR CPU
// ---------------------------------------------------------------------------
static inline unsigned Hash(unsigned x, unsigned y, unsigned s) {
    unsigned h = x * 374761393u + y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static void GrainCPU(unsigned char* rgba, int w, int h, const W3dPostEffect& e, unsigned seed) {
    const int size = (e.v[1] >= 1.0f) ? (int)(e.v[1] + 0.5f) : 1;
    const int k = (int)(e.v[0] * 255.0f);
    const bool color = e.opt;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        const unsigned gx = (unsigned)(x / size), gy = (unsigned)(y / size);
        unsigned char* p = &rgba[((size_t)y * w + x) * 4];
        for (int c = 0; c < 3; c++) {
            const unsigned n = Hash(gx, gy, seed + (color ? (unsigned)c * 977u : 0u)) & 255u;
            int v = (int)p[c] + (((int)n - 128) * k) / 255;
            p[c] = (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    }
}
// el radio del circulo de confusion (px) a esa distancia
static inline float RadiusCoC(float z, float focus, float range, float rmax) {
    float r = (fabsf(z - focus) - range * 0.5f) / (range > 0.01f ? range : 0.01f);
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    return r * rmax;
}
static void FocusCPU(unsigned char* rgba, int w, int h, const float* prof, const W3dPostEffect& e) {
    if (!prof) return;
    static const float kDisk[16][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {0.707f,0.707f}, {-0.707f,0.707f}, {0.707f,-0.707f},
        {-0.707f,-0.707f}, {0.5f,0}, {-0.5f,0}, {0,0.5f}, {0,-0.5f}, {0.92f,0.38f}, {-0.38f,0.92f}, {-0.92f,-0.38f}, {0.38f,-0.92f} };
    float focus, range, rmax;
    if (!W3dPostFocusNumbers(e, h, focus, range, rmax)) return;
    std::vector<unsigned char> src(rgba, rgba + (size_t)w * h * 4);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        const float z = prof[(size_t)y * w + x];
        const float r = RadiusCoC(z, focus, range, rmax);
        if (r <= 0.5f) continue;
        const unsigned char* c0 = &src[((size_t)y * w + x) * 4];
        int acc[3] = { c0[0], c0[1], c0[2] }, n = 1;
        for (int s = 0; s < 16; s++) {
            int sx = x + (int)(kDisk[s][0] * r), sy = y + (int)(kDisk[s][1] * r);
            if (sx < 0) sx = 0; if (sx >= w) sx = w - 1; if (sy < 0) sy = 0; if (sy >= h) sy = h - 1;
            const float zs = prof[(size_t)sy * w + sx];
            // (lo que esta delante y nitido no se desparrama sobre lo desenfocado de atras)
            if (zs < z - 0.05f && RadiusCoC(zs, focus, range, rmax) < r * 0.5f) continue;
            const unsigned char* c = &src[((size_t)sy * w + sx) * 4];
            acc[0] += c[0]; acc[1] += c[1]; acc[2] += c[2]; n++;
        }
        unsigned char* p = &rgba[((size_t)y * w + x) * 4];
        p[0] = (unsigned char)(acc[0] / n); p[1] = (unsigned char)(acc[1] / n); p[2] = (unsigned char)(acc[2] / n);
    }
}
// ---- BLOOM ----
// el paso entre muestras del desenfoque (px) en cada eje: la difusion (relativa al alto) repartida en las muestras, y
// la relacion anamorfica estirando un eje y achicando el otro
static void BloomSteps(const W3dPostEffect& e, int h, float& px, float& py) {
    const float radius = (float)h * 0.015f * (e.v[4] < 0.1f ? 0.1f : e.v[4]);
    const float step = radius / (e.opt ? 3.0f : 6.0f);
    float r = e.v[5]; if (r < -1.0f) r = -1.0f; if (r > 1.0f) r = 1.0f;
    px = step * (1.0f + r); py = step * (1.0f - r);
}
// lo que BRILLA de un color (0..1): recortado al tope, por encima del umbral con la rodilla suave (cuadratica)
static inline void BloomBrightness(float* c, float threshold, float knee, float limit) {
    for (int k = 0; k < 3; k++) if (c[k] > limit) c[k] = limit;
    const float br = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
    float rq = br - threshold + knee;
    if (rq < 0.0f) rq = 0.0f; if (rq > 2.0f * knee) rq = 2.0f * knee;
    rq = (knee > 0.0f) ? rq * rq / (4.0f * knee + 0.0001f) : 0.0f;
    float f = br - threshold; if (rq > f) f = rq;
    f = (br > 0.0001f) ? f / br : 0.0f; if (f < 0.0f) f = 0.0f;
    for (int k = 0; k < 3; k++) c[k] *= f;
}
static void BloomCPU(unsigned char* rgba, int w, int h, const W3dPostEffect& e) {
    const int n = e.opt ? 3 : 6;
    const float sigma2 = e.opt ? 4.5f : 18.0f;
    float px, py; BloomSteps(e, h, px, py);
    float weight[13]; float total = 0.0f;
    for (int i = -n; i <= n; i++) { weight[i + n] = expf(-(float)(i * i) / sigma2); total += weight[i + n]; }
    for (int i = 0; i <= 2 * n; i++) weight[i] /= total;
    std::vector<float> a((size_t)w * h * 3), b((size_t)w * h * 3);
    const float knee = e.v[1] * e.v[2];
    for (size_t i = 0; i < (size_t)w * h; i++) {
        float c[3] = { rgba[i * 4] / 255.0f, rgba[i * 4 + 1] / 255.0f, rgba[i * 4 + 2] / 255.0f };
        BloomBrightness(c, e.v[1], knee, e.v[3]);
        a[i * 3] = c[0]; a[i * 3 + 1] = c[1]; a[i * 3 + 2] = c[2];
    }
    // horizontal (a -> b) y vertical (b -> a)
    for (int pass = 0; pass < 2; pass++) {
        const std::vector<float>& src = pass == 0 ? a : b;
        std::vector<float>& dst = pass == 0 ? b : a;
        const float step = pass == 0 ? px : py;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            float acc[3] = { 0, 0, 0 };
            for (int i = -n; i <= n; i++) {
                int sx = x, sy = y;
                const int d = (int)floorf((float)i * step + 0.5f);
                if (pass == 0) sx += d; else sy += d;
                if (sx < 0) sx = 0; if (sx >= w) sx = w - 1; if (sy < 0) sy = 0; if (sy >= h) sy = h - 1;
                const float* s = &src[((size_t)sy * w + sx) * 3];
                const float q = weight[i + n];
                acc[0] += s[0] * q; acc[1] += s[1] * q; acc[2] += s[2] * q;
            }
            float* o = &dst[((size_t)y * w + x) * 3];
            o[0] = acc[0]; o[1] = acc[1]; o[2] = acc[2];
        }
    }
    for (size_t i = 0; i < (size_t)w * h; i++) for (int k = 0; k < 3; k++) {
        int v = (int)rgba[i * 4 + k] + (int)(a[i * 3 + k] * e.v[0] * e.col[k] * 255.0f + 0.5f);
        rgba[i * 4 + k] = (unsigned char)(v > 255 ? 255 : v);
    }
}
void W3dPostCPU(unsigned char* rgba, int w, int h, const float* prof, const unsigned char* vec, unsigned seed) {
    if (!rgba || w <= 0 || h <= 0 || !g_post.on) return;
    std::vector<unsigned char> tmp;
    for (size_t i = 0; i < g_post.effects.size(); i++) {
        const W3dPostEffect& e = g_post.effects[i];
        if (!e.active) continue;
        if (e.type == W3dPostGrain) GrainCPU(rgba, w, h, e, seed + (unsigned)i * 131u);
        else if (e.type == W3dPostFocus) FocusCPU(rgba, w, h, prof, e);
        else if (e.type == W3dPostBloom) BloomCPU(rgba, w, h, e);
        else if (e.type == W3dPostMotion && vec && e.v[0] > 0.0f) {
            tmp.resize((size_t)w * h * 4);
            W3dMotionBlur(rgba, vec, w, h, e.v[0], 12, &tmp[0]);
            memcpy(rgba, &tmp[0], tmp.size());
        }
    }
}

// ---------------------------------------------------------------------------
//  POR GPU: la imagen (y los datos) copiados de la pantalla a texturas; cada efecto, un quad con su shader sobre la
//  pantalla; si sigue otro, la pantalla se vuelve a copiar
// ---------------------------------------------------------------------------
static unsigned gTexImg = 0, gTexProf = 0, gTexVec = 0, gTexBloom = 0;
static int gTexW = 0, gTexH = 0;
static int Pot(int v) { int p = 64; while (p < v) p <<= 1; return p; }
static bool gDataPrevious = false, gPrevProf = false, gPrevMov = false;
static bool ReserveTextures(int w, int h) {
    namespace gfx = w3dEngine;
    if (w > gTexW || h > gTexH || !gTexImg) {
        if (w > gTexW) gTexW = Pot(w);
        if (h > gTexH) gTexH = Pot(h);
        gTexImg = gfx::ReserveScreen(gTexImg, gTexW, gTexH);
        gTexProf = gfx::ReserveScreen(gTexProf, gTexW, gTexH);
        gTexVec = gfx::ReserveScreen(gTexVec, gTexW, gTexH);
        gTexBloom = gfx::ReserveScreen(gTexBloom, gTexW, gTexH);
    }
    return gTexImg != 0;
}
bool W3dPostDataPrevious(int x, int y, int w, int h, void (*drawData)(void*, bool), void* ctx, bool prof, bool mov) {
    namespace gfx = w3dEngine;
    gDataPrevious = false;
    if (!gfx::PassesWithShader() || w <= 0 || h <= 0 || !drawData || !ReserveTextures(w, h)) return false;
    gPrevProf = prof && gTexProf; gPrevMov = mov && gTexVec;
    if (gPrevProf) { drawData(ctx, true); gfx::CopyScreen(gTexProf, x, y, w, h); }
    if (gPrevMov) { drawData(ctx, false); gfx::CopyScreen(gTexVec, x, y, w, h); }
    gDataPrevious = true;
    return true;
}
void W3dPostDataForget() { gDataPrevious = false; }
bool W3dPostGPU(int x, int y, int w, int h, void (*drawData)(void*, bool), void* ctx, float far, unsigned seed) {
    namespace gfx = w3dEngine;
    if (!gfx::PassesWithShader() || w <= 0 || h <= 0 || !W3dPostHas()) return false;
    if (!ReserveTextures(w, h)) return false;
    gfx::CopyScreen(gTexImg, x, y, w, h);
    // los DATOS que piden los efectos: la profundidad y el movimiento, cada uno en su pase (en 8 bits por canal, sin
    // alfa: la pantalla puede no tenerlo)
    // (dibujados DE ANTEMANO, antes de la escena: W3dPostDataPrevious; si no, aca)
    const bool prof = W3dPostNeedsDepth() && gTexProf && (gDataPrevious ? gPrevProf : drawData != 0);
    const bool mov = W3dPostNeedsMotion() && gTexVec && (gDataPrevious ? gPrevMov : drawData != 0);
    if (prof && !gDataPrevious) { drawData(ctx, true); gfx::CopyScreen(gTexProf, x, y, w, h); }
    if (mov && !gDataPrevious) { drawData(ctx, false); gfx::CopyScreen(gTexVec, x, y, w, h); }
    const bool data = (prof || mov) && !gDataPrevious;
    float uv[4] = { 0.0f, 0.0f, (float)w / (float)gTexW, (float)h / (float)gTexH };
    if (gfx::CopyScreenFlipped()) { uv[1] = uv[3]; uv[3] = 0.0f; }   // (un driver que la copia al reves)
    // el ultimo efecto activo (despues de el no hace falta volver a copiar la pantalla)
    int last = -1;
    for (size_t i = 0; i < g_post.effects.size(); i++) if (Live(g_post.effects[i])) last = (int)i;
    // la imagen de nuevo en la pantalla (el pase de datos la piso)
    if (data) gfx::PostEffect(gfx::PostCopy, gTexImg, 0, gTexW, gTexH, uv, NULL);
    for (int i = 0; i <= last; i++) {
        const W3dPostEffect& e = g_post.effects[(size_t)i];
        if (!Live(e)) continue;
        float p[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        int ef = -1;
        if (e.type == W3dPostGrain) { ef = gfx::PostGrain; p[0] = e.v[0]; p[1] = e.v[1]; p[2] = (float)((seed + (unsigned)i * 131u) % 997u); p[3] = e.opt ? 1.0f : 0.0f; }
        unsigned dat = 0;
        if (e.type == W3dPostFocus && prof) {
            if (!W3dPostFocusNumbers(e, h, p[0], p[1], p[2])) continue;
            ef = gfx::PostFocus; dat = gTexProf; p[3] = far;
        }
        else if (e.type == W3dPostMotion && mov) { ef = gfx::PostMotion; dat = gTexVec; p[0] = e.v[0]; }
        else if (e.type == W3dPostBloom && gTexBloom) {
            // el brillo filtrado y desenfocado HORIZONTAL (a la pantalla y a gTexBloom), despues VERTICAL (idem); la
            // mezcla con la imagen queda como el paso del efecto (abajo)
            float px, py; BloomSteps(e, h, px, py);
            float q[8] = { e.v[1], e.v[1] * e.v[2], e.v[3], e.opt ? 1.0f : 0.0f, px, 0.0f, 1.0f, 0.0f };
            if (!gfx::PostEffect(gfx::PostBloomBlur, gTexImg, 0, gTexW, gTexH, uv, q)) return false;
            gfx::CopyScreen(gTexBloom, x, y, w, h);
            q[4] = 0.0f; q[5] = py; q[6] = 0.0f;
            if (!gfx::PostEffect(gfx::PostBloomBlur, gTexBloom, 0, gTexW, gTexH, uv, q)) return false;
            gfx::CopyScreen(gTexBloom, x, y, w, h);
            ef = gfx::PostBloomMix; dat = gTexBloom;
            p[0] = e.v[0]; p[4] = e.col[0]; p[5] = e.col[1]; p[6] = e.col[2];
        }
        if (ef < 0) continue;
        if (!gfx::PostEffect(ef, gTexImg, dat, gTexW, gTexH, uv, p)) return false;
        if (i < last) gfx::CopyScreen(gTexImg, x, y, w, h);
    }
    return true;
}
