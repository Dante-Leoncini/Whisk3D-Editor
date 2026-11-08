// ============================================================================
//  Postproceso.cpp - la pila de efectos de postprocesado (ver Postproceso.h): por GPU con shaders, por CPU sin ellos.
// ============================================================================
#include "render/Postproceso.h"
#include "render/VectoresMovimiento.h"   // W3dDesenfoqueMovimiento (el de la CPU)
#include "objects/Camera.h"              // el foco de la camara activa
#include "graphics/w3dGraphics.h"
#include "graphics/w3dTexture.h"              // ReservarPantalla / CopiarPantalla
#include <math.h>
#include <string.h>

static W3dPostOpciones gPostVacio;
static const W3dPostOpciones* gPostCur = &gPostVacio;
void W3dPostUsar(const W3dPostOpciones* o) { gPostCur = o ? o : &gPostVacio; }
const W3dPostOpciones& W3dPostActual() { return *gPostCur; }
#define g_post (*gPostCur)

const char* W3dPostNombre(int tipo) {
    return tipo == W3dPostGrano ? "Film grain" : tipo == W3dPostFoco ? "Depth of field" :
           tipo == W3dPostMovimiento ? "Motion blur" : tipo == W3dPostBloom ? "Bloom" : "?";
}
int W3dPostNumeros(int tipo) { return tipo == W3dPostBloom ? 6 : 4; }
void W3dPostDefectos(W3dPostEfecto& e, int tipo) {
    e.tipo = tipo; e.activo = true; e.opc = (tipo == W3dPostFoco);
    for (int k = 0; k < 8; k++) e.v[k] = 0.0f;
    e.col[0] = e.col[1] = e.col[2] = e.col[3] = 1.0f;
    if (tipo == W3dPostGrano)           { e.v[0] = 0.12f; e.v[1] = 1.0f; }
    else if (tipo == W3dPostBloom)      { e.v[0] = 1.0f; e.v[1] = 0.8f; e.v[2] = 0.5f; e.v[3] = 1.0f; e.v[4] = 5.0f; e.v[5] = 0.0f; }
    else if (tipo == W3dPostFoco)       { e.v[0] = 5.0f; e.v[1] = 1.0f; e.v[2] = 1.5f; }
    else if (tipo == W3dPostMovimiento) { e.v[0] = 0.5f; }
}
// tildado y que HACE algo: el desenfoque de campo con el foco de la camara y la camara sin profundidad de campo no
// cuenta (ni copia la pantalla ni pide el pase de profundidad)
static bool Vivo(const W3dPostEfecto& e) {
    if (!e.activo) return false;
    if (e.tipo == W3dPostFoco && e.opc && !(CameraActive && CameraActive->dof)) return false;
    return true;
}
bool W3dPostHay() {
    if (!g_post.on) return false;
    for (size_t i = 0; i < g_post.efectos.size(); i++) if (Vivo(g_post.efectos[i])) return true;
    return false;
}
static bool HayDe(int tipo) {
    if (!g_post.on) return false;
    for (size_t i = 0; i < g_post.efectos.size(); i++) if (Vivo(g_post.efectos[i]) && g_post.efectos[i].tipo == tipo) return true;
    return false;
}
bool W3dPostNecesitaProfundidad() { return HayDe(W3dPostFoco); }
bool W3dPostNecesitaMovimiento() { return HayDe(W3dPostMovimiento); }
bool W3dPostPorCPU() { return !w3dEngine::PasesConShader(); }
float W3dPostDistanciaFoco(const W3dPostEfecto& e) {
    if (e.opc && CameraActive && CameraActive->dof) return CameraActive->DistanciaFoco();
    return e.v[0] > 0.01f ? e.v[0] : 0.01f;
}
bool W3dPostFocoNumeros(const W3dPostEfecto& e, int h, float& foco, float& rango, float& radioMax) {
    rango = e.v[1];
    radioMax = e.v[2] * 0.01f * (float)h;
    if (!e.opc) { foco = W3dPostDistanciaFoco(e); return radioMax > 0.5f; }
    if (!CameraActive || !CameraActive->dof) return false;   // la camara es un agujerito: todo nitido
    foco = CameraActive->DistanciaFoco();
    // el circulo de confusion de lo que esta en el infinito: la lente vista desde el foco, en la pantalla
    const float lente = CameraActive->RadioLente();
    const float t = tanf(CameraActive->fov * 0.5f * 3.14159265f / 180.0f);
    if (lente <= 0.0f || t < 1e-4f) return false;
    radioMax = lente / (foco * t) * 0.5f * (float)h;
    if (radioMax > 0.2f * (float)h) radioMax = 0.2f * (float)h;   // (un tope: el disco de muestras no da para mas)
    return radioMax > 0.5f;
}

// ---------------------------------------------------------------------------
//  POR CPU
// ---------------------------------------------------------------------------
static inline unsigned Hash(unsigned x, unsigned y, unsigned s) {
    unsigned h = x * 374761393u + y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static void GranoCPU(unsigned char* rgba, int w, int h, const W3dPostEfecto& e, unsigned semilla) {
    const int tam = (e.v[1] >= 1.0f) ? (int)(e.v[1] + 0.5f) : 1;
    const int k = (int)(e.v[0] * 255.0f);
    const bool color = e.opc;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        const unsigned gx = (unsigned)(x / tam), gy = (unsigned)(y / tam);
        unsigned char* p = &rgba[((size_t)y * w + x) * 4];
        for (int c = 0; c < 3; c++) {
            const unsigned n = Hash(gx, gy, semilla + (color ? (unsigned)c * 977u : 0u)) & 255u;
            int v = (int)p[c] + (((int)n - 128) * k) / 255;
            p[c] = (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    }
}
// el radio del circulo de confusion (px) a esa distancia
static inline float RadioCoC(float z, float foco, float rango, float rmax) {
    float r = (fabsf(z - foco) - rango * 0.5f) / (rango > 0.01f ? rango : 0.01f);
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    return r * rmax;
}
static void FocoCPU(unsigned char* rgba, int w, int h, const float* prof, const W3dPostEfecto& e) {
    if (!prof) return;
    static const float kDisco[16][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {0.707f,0.707f}, {-0.707f,0.707f}, {0.707f,-0.707f},
        {-0.707f,-0.707f}, {0.5f,0}, {-0.5f,0}, {0,0.5f}, {0,-0.5f}, {0.92f,0.38f}, {-0.38f,0.92f}, {-0.92f,-0.38f}, {0.38f,-0.92f} };
    float foco, rango, rmax;
    if (!W3dPostFocoNumeros(e, h, foco, rango, rmax)) return;
    std::vector<unsigned char> src(rgba, rgba + (size_t)w * h * 4);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        const float z = prof[(size_t)y * w + x];
        const float r = RadioCoC(z, foco, rango, rmax);
        if (r <= 0.5f) continue;
        const unsigned char* c0 = &src[((size_t)y * w + x) * 4];
        int acc[3] = { c0[0], c0[1], c0[2] }, n = 1;
        for (int s = 0; s < 16; s++) {
            int sx = x + (int)(kDisco[s][0] * r), sy = y + (int)(kDisco[s][1] * r);
            if (sx < 0) sx = 0; if (sx >= w) sx = w - 1; if (sy < 0) sy = 0; if (sy >= h) sy = h - 1;
            const float zs = prof[(size_t)sy * w + sx];
            // (lo que esta delante y nitido no se desparrama sobre lo desenfocado de atras)
            if (zs < z - 0.05f && RadioCoC(zs, foco, rango, rmax) < r * 0.5f) continue;
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
static void BloomPasos(const W3dPostEfecto& e, int h, float& px, float& py) {
    const float radio = (float)h * 0.015f * (e.v[4] < 0.1f ? 0.1f : e.v[4]);
    const float paso = radio / (e.opc ? 3.0f : 6.0f);
    float r = e.v[5]; if (r < -1.0f) r = -1.0f; if (r > 1.0f) r = 1.0f;
    px = paso * (1.0f + r); py = paso * (1.0f - r);
}
// lo que BRILLA de un color (0..1): recortado al tope, por encima del umbral con la rodilla suave (cuadratica)
static inline void BloomBrillo(float* c, float umbral, float rodilla, float tope) {
    for (int k = 0; k < 3; k++) if (c[k] > tope) c[k] = tope;
    const float br = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
    float rq = br - umbral + rodilla;
    if (rq < 0.0f) rq = 0.0f; if (rq > 2.0f * rodilla) rq = 2.0f * rodilla;
    rq = (rodilla > 0.0f) ? rq * rq / (4.0f * rodilla + 0.0001f) : 0.0f;
    float f = br - umbral; if (rq > f) f = rq;
    f = (br > 0.0001f) ? f / br : 0.0f; if (f < 0.0f) f = 0.0f;
    for (int k = 0; k < 3; k++) c[k] *= f;
}
static void BloomCPU(unsigned char* rgba, int w, int h, const W3dPostEfecto& e) {
    const int n = e.opc ? 3 : 6;
    const float sigma2 = e.opc ? 4.5f : 18.0f;
    float px, py; BloomPasos(e, h, px, py);
    float peso[13]; float total = 0.0f;
    for (int i = -n; i <= n; i++) { peso[i + n] = expf(-(float)(i * i) / sigma2); total += peso[i + n]; }
    for (int i = 0; i <= 2 * n; i++) peso[i] /= total;
    std::vector<float> a((size_t)w * h * 3), b((size_t)w * h * 3);
    const float rodilla = e.v[1] * e.v[2];
    for (size_t i = 0; i < (size_t)w * h; i++) {
        float c[3] = { rgba[i * 4] / 255.0f, rgba[i * 4 + 1] / 255.0f, rgba[i * 4 + 2] / 255.0f };
        BloomBrillo(c, e.v[1], rodilla, e.v[3]);
        a[i * 3] = c[0]; a[i * 3 + 1] = c[1]; a[i * 3 + 2] = c[2];
    }
    // horizontal (a -> b) y vertical (b -> a)
    for (int pasada = 0; pasada < 2; pasada++) {
        const std::vector<float>& src = pasada == 0 ? a : b;
        std::vector<float>& dst = pasada == 0 ? b : a;
        const float paso = pasada == 0 ? px : py;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            float acc[3] = { 0, 0, 0 };
            for (int i = -n; i <= n; i++) {
                int sx = x, sy = y;
                const int d = (int)floorf((float)i * paso + 0.5f);
                if (pasada == 0) sx += d; else sy += d;
                if (sx < 0) sx = 0; if (sx >= w) sx = w - 1; if (sy < 0) sy = 0; if (sy >= h) sy = h - 1;
                const float* s = &src[((size_t)sy * w + sx) * 3];
                const float q = peso[i + n];
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
void W3dPostCPU(unsigned char* rgba, int w, int h, const float* prof, const unsigned char* vec, unsigned semilla) {
    if (!rgba || w <= 0 || h <= 0 || !g_post.on) return;
    std::vector<unsigned char> tmp;
    for (size_t i = 0; i < g_post.efectos.size(); i++) {
        const W3dPostEfecto& e = g_post.efectos[i];
        if (!e.activo) continue;
        if (e.tipo == W3dPostGrano) GranoCPU(rgba, w, h, e, semilla + (unsigned)i * 131u);
        else if (e.tipo == W3dPostFoco) FocoCPU(rgba, w, h, prof, e);
        else if (e.tipo == W3dPostBloom) BloomCPU(rgba, w, h, e);
        else if (e.tipo == W3dPostMovimiento && vec && e.v[0] > 0.0f) {
            tmp.resize((size_t)w * h * 4);
            W3dDesenfoqueMovimiento(rgba, vec, w, h, e.v[0], 12, &tmp[0]);
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
static bool gDatosPrevios = false, gPrevProf = false, gPrevMov = false;
static bool ReservarTexturas(int w, int h) {
    namespace gfx = w3dEngine;
    if (w > gTexW || h > gTexH || !gTexImg) {
        if (w > gTexW) gTexW = Pot(w);
        if (h > gTexH) gTexH = Pot(h);
        gTexImg = gfx::ReservarPantalla(gTexImg, gTexW, gTexH);
        gTexProf = gfx::ReservarPantalla(gTexProf, gTexW, gTexH);
        gTexVec = gfx::ReservarPantalla(gTexVec, gTexW, gTexH);
        gTexBloom = gfx::ReservarPantalla(gTexBloom, gTexW, gTexH);
    }
    return gTexImg != 0;
}
bool W3dPostDatosPrevios(int x, int y, int w, int h, void (*dibujarDatos)(void*, bool), void* ctx, bool prof, bool mov) {
    namespace gfx = w3dEngine;
    gDatosPrevios = false;
    if (!gfx::PasesConShader() || w <= 0 || h <= 0 || !dibujarDatos || !ReservarTexturas(w, h)) return false;
    gPrevProf = prof && gTexProf; gPrevMov = mov && gTexVec;
    if (gPrevProf) { dibujarDatos(ctx, true); gfx::CopiarPantalla(gTexProf, x, y, w, h); }
    if (gPrevMov) { dibujarDatos(ctx, false); gfx::CopiarPantalla(gTexVec, x, y, w, h); }
    gDatosPrevios = true;
    return true;
}
void W3dPostDatosOlvidar() { gDatosPrevios = false; }
bool W3dPostGPU(int x, int y, int w, int h, void (*dibujarDatos)(void*, bool), void* ctx, float lejos, unsigned semilla) {
    namespace gfx = w3dEngine;
    if (!gfx::PasesConShader() || w <= 0 || h <= 0 || !W3dPostHay()) return false;
    if (!ReservarTexturas(w, h)) return false;
    gfx::CopiarPantalla(gTexImg, x, y, w, h);
    // los DATOS que piden los efectos: la profundidad y el movimiento, cada uno en su pase (en 8 bits por canal, sin
    // alfa: la pantalla puede no tenerlo)
    // (dibujados DE ANTEMANO, antes de la escena: W3dPostDatosPrevios; si no, aca)
    const bool prof = W3dPostNecesitaProfundidad() && gTexProf && (gDatosPrevios ? gPrevProf : dibujarDatos != 0);
    const bool mov = W3dPostNecesitaMovimiento() && gTexVec && (gDatosPrevios ? gPrevMov : dibujarDatos != 0);
    if (prof && !gDatosPrevios) { dibujarDatos(ctx, true); gfx::CopiarPantalla(gTexProf, x, y, w, h); }
    if (mov && !gDatosPrevios) { dibujarDatos(ctx, false); gfx::CopiarPantalla(gTexVec, x, y, w, h); }
    const bool datos = (prof || mov) && !gDatosPrevios;
    float uv[4] = { 0.0f, 0.0f, (float)w / (float)gTexW, (float)h / (float)gTexH };
    if (gfx::CopiaPantallaDadaVuelta()) { uv[1] = uv[3]; uv[3] = 0.0f; }   // (un driver que la copia al reves)
    // el ultimo efecto activo (despues de el no hace falta volver a copiar la pantalla)
    int ultimo = -1;
    for (size_t i = 0; i < g_post.efectos.size(); i++) if (Vivo(g_post.efectos[i])) ultimo = (int)i;
    // la imagen de nuevo en la pantalla (el pase de datos la piso)
    if (datos) gfx::PostEfecto(gfx::PostCopiar, gTexImg, 0, gTexW, gTexH, uv, NULL);
    for (int i = 0; i <= ultimo; i++) {
        const W3dPostEfecto& e = g_post.efectos[(size_t)i];
        if (!Vivo(e)) continue;
        float p[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        int ef = -1;
        if (e.tipo == W3dPostGrano) { ef = gfx::PostGrano; p[0] = e.v[0]; p[1] = e.v[1]; p[2] = (float)((semilla + (unsigned)i * 131u) % 997u); p[3] = e.opc ? 1.0f : 0.0f; }
        unsigned dat = 0;
        if (e.tipo == W3dPostFoco && prof) {
            if (!W3dPostFocoNumeros(e, h, p[0], p[1], p[2])) continue;
            ef = gfx::PostFoco; dat = gTexProf; p[3] = lejos;
        }
        else if (e.tipo == W3dPostMovimiento && mov) { ef = gfx::PostMovimiento; dat = gTexVec; p[0] = e.v[0]; }
        else if (e.tipo == W3dPostBloom && gTexBloom) {
            // el brillo filtrado y desenfocado HORIZONTAL (a la pantalla y a gTexBloom), despues VERTICAL (idem); la
            // mezcla con la imagen queda como el paso del efecto (abajo)
            float px, py; BloomPasos(e, h, px, py);
            float q[8] = { e.v[1], e.v[1] * e.v[2], e.v[3], e.opc ? 1.0f : 0.0f, px, 0.0f, 1.0f, 0.0f };
            if (!gfx::PostEfecto(gfx::PostBloomDifuminar, gTexImg, 0, gTexW, gTexH, uv, q)) return false;
            gfx::CopiarPantalla(gTexBloom, x, y, w, h);
            q[4] = 0.0f; q[5] = py; q[6] = 0.0f;
            if (!gfx::PostEfecto(gfx::PostBloomDifuminar, gTexBloom, 0, gTexW, gTexH, uv, q)) return false;
            gfx::CopiarPantalla(gTexBloom, x, y, w, h);
            ef = gfx::PostBloomMezcla; dat = gTexBloom;
            p[0] = e.v[0]; p[4] = e.col[0]; p[5] = e.col[1]; p[6] = e.col[2];
        }
        if (ef < 0) continue;
        if (!gfx::PostEfecto(ef, gTexImg, dat, gTexW, gTexH, uv, p)) return false;
        if (i < ultimo) gfx::CopiarPantalla(gTexImg, x, y, w, h);
    }
    return true;
}
