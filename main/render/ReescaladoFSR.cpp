// ============================================================================
//  ReescaladoFSR.cpp - FSR 1 (EASU + RCAS) por CPU. Ver ReescaladoFSR.h.
//  Port directo de ffx_fsr1.h (AMD FidelityFX Super Resolution 1.0, MIT): FsrEasuF / FsrEasuSetF / FsrEasuTapF y
//  FsrRcasF, en float y sin las aproximaciones de bits de la GPU (aca 1/x y 1/sqrt(x) de verdad).
//  Copyright (c) 2021 Advanced Micro Devices, Inc. (el algoritmo); MIT.
// ============================================================================
#include "render/ReescaladoFSR.h"
#include <math.h>
#include <vector>
#if !defined(W3D_SYMBIAN) && !defined(__EMSCRIPTEN__)
    #define FSR_HILOS 1
    #include <thread>
#endif

namespace {
struct C3 { float r, g, b; };
static inline float Mn(float a, float b) { return a < b ? a : b; }
static inline float Mx(float a, float b) { return a > b ? a : b; }
static inline float Sat(float a) { return a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a); }
static inline float Rcp(float a) { return 1.0f / (a != 0.0f ? a : 1e-20f); }

struct Img {
    const unsigned char* p; int w, h;
    inline C3 At(int x, int y) const {
        if (x < 0) x = 0; else if (x >= w) x = w - 1;
        if (y < 0) y = 0; else if (y >= h) y = h - 1;
        const unsigned char* q = p + ((size_t)y * w + x) * 4;
        const C3 c = { q[0] * (1.0f / 255.0f), q[1] * (1.0f / 255.0f), q[2] * (1.0f / 255.0f) };
        return c;
    }
};
static inline float Luma(const C3& c) { return c.b * 0.5f + (c.r * 0.5f + c.g); }

// FsrEasuSetF: la direccion y el largo del borde en una de las 4 esquinas del bilineal ('+': a arriba, b izq,
// c centro, d der, e abajo), pesada por su peso bilineal
static inline void EasuSet(float& dx, float& dy, float& len, float w, float lA, float lB, float lC, float lD, float lE) {
    const float dc = lD - lC, cb = lC - lB;
    float lenX = Rcp(Mx(fabsf(dc), fabsf(cb)));
    const float dirX = lD - lB;
    dx += dirX * w;
    lenX = Sat(fabsf(dirX) * lenX); lenX *= lenX;
    len += lenX * w;
    const float ec = lE - lC, ca = lC - lA;
    float lenY = Rcp(Mx(fabsf(ec), fabsf(ca)));
    const float dirY = lE - lA;
    dy += dirY * w;
    lenY = Sat(fabsf(dirY) * lenY); lenY *= lenY;
    len += lenY * w;
}
// FsrEasuTapF: un tap del lanczos-2 aproximado, rotado al borde y estirado (anisotropico)
static inline void EasuTap(C3& aC, float& aW, float ox, float oy, float dx, float dy, float l2x, float l2y,
                           float lob, float clp, const C3& c) {
    float vx = ox * dx + oy * dy;
    float vy = ox * (-dy) + oy * dx;
    vx *= l2x; vy *= l2y;
    float d2 = vx * vx + vy * vy;
    d2 = Mn(d2, clp);
    float wB = (2.0f / 5.0f) * d2 - 1.0f;
    float wA = lob * d2 - 1.0f;
    wB *= wB; wA *= wA;
    wB = (25.0f / 16.0f) * wB - (25.0f / 16.0f - 1.0f);
    const float w = wB * wA;
    aC.r += c.r * w; aC.g += c.g * w; aC.b += c.b * w; aW += w;
}
// FsrEasuF: un pixel de salida
static C3 Easu(const Img& im, float ppx, float ppy) {
    const float fx = floorf(ppx), fy = floorf(ppy);
    const int ix = (int)fx, iy = (int)fy;
    const float px = ppx - fx, py = ppy - fy;
    //    b c
    //  e f g h
    //  i j k l
    //    n o
    const C3 b = im.At(ix, iy - 1), c = im.At(ix + 1, iy - 1);
    const C3 e = im.At(ix - 1, iy), f = im.At(ix, iy), g = im.At(ix + 1, iy), h = im.At(ix + 2, iy);
    const C3 i = im.At(ix - 1, iy + 1), j = im.At(ix, iy + 1), k = im.At(ix + 1, iy + 1), l = im.At(ix + 2, iy + 1);
    const C3 n = im.At(ix, iy + 2), o = im.At(ix + 1, iy + 2);
    const float bL = Luma(b), cL = Luma(c), eL = Luma(e), fL = Luma(f), gL = Luma(g), hL = Luma(h);
    const float iL = Luma(i), jL = Luma(j), kL = Luma(k), lL = Luma(l), nL = Luma(n), oL = Luma(o);
    float dx = 0.0f, dy = 0.0f, len = 0.0f;
    EasuSet(dx, dy, len, (1.0f - px) * (1.0f - py), bL, eL, fL, gL, jL);
    EasuSet(dx, dy, len, px * (1.0f - py),          cL, fL, gL, hL, kL);
    EasuSet(dx, dy, len, (1.0f - px) * py,          fL, iL, jL, kL, nL);
    EasuSet(dx, dy, len, px * py,                   gL, jL, kL, lL, oL);
    // la direccion normalizada (sin direccion: horizontal)
    float dirR = dx * dx + dy * dy;
    const bool zro = dirR < (1.0f / 32768.0f);
    dirR = zro ? 1.0f : 1.0f / sqrtf(dirR);
    if (zro) dx = 1.0f;
    dx *= dirR; dy *= dirR;
    // {0..2} -> {0..1}, al cuadrado
    len = len * 0.5f; len *= len;
    // estirar el nucleo: 1 en vertical/horizontal, sqrt(2) en diagonal
    const float stretch = (dx * dx + dy * dy) * Rcp(Mx(fabsf(dx), fabsf(dy)));
    const float l2x = 1.0f + (stretch - 1.0f) * len, l2y = 1.0f - 0.5f * len;
    // la ventana: de +-sqrt(2) a un poco mas de 2 segun cuanto borde hay
    const float lob = 0.5f + ((1.0f / 4.0f - 0.04f) - 0.5f) * len;
    const float clp = Rcp(lob);
    // el resultado, acotado al min/max de los 4 mas cercanos (sin halos)
    C3 mn4 = { Mn(Mn(Mn(f.r, g.r), j.r), k.r), Mn(Mn(Mn(f.g, g.g), j.g), k.g), Mn(Mn(Mn(f.b, g.b), j.b), k.b) };
    C3 mx4 = { Mx(Mx(Mx(f.r, g.r), j.r), k.r), Mx(Mx(Mx(f.g, g.g), j.g), k.g), Mx(Mx(Mx(f.b, g.b), j.b), k.b) };
    C3 aC = { 0, 0, 0 }; float aW = 0.0f;
    EasuTap(aC, aW,  0.0f - px, -1.0f - py, dx, dy, l2x, l2y, lob, clp, b);
    EasuTap(aC, aW,  1.0f - px, -1.0f - py, dx, dy, l2x, l2y, lob, clp, c);
    EasuTap(aC, aW, -1.0f - px,  1.0f - py, dx, dy, l2x, l2y, lob, clp, i);
    EasuTap(aC, aW,  0.0f - px,  1.0f - py, dx, dy, l2x, l2y, lob, clp, j);
    EasuTap(aC, aW,  0.0f - px,  0.0f - py, dx, dy, l2x, l2y, lob, clp, f);
    EasuTap(aC, aW, -1.0f - px,  0.0f - py, dx, dy, l2x, l2y, lob, clp, e);
    EasuTap(aC, aW,  1.0f - px,  1.0f - py, dx, dy, l2x, l2y, lob, clp, k);
    EasuTap(aC, aW,  2.0f - px,  1.0f - py, dx, dy, l2x, l2y, lob, clp, l);
    EasuTap(aC, aW,  2.0f - px,  0.0f - py, dx, dy, l2x, l2y, lob, clp, h);
    EasuTap(aC, aW,  1.0f - px,  0.0f - py, dx, dy, l2x, l2y, lob, clp, g);
    EasuTap(aC, aW,  1.0f - px,  2.0f - py, dx, dy, l2x, l2y, lob, clp, o);
    EasuTap(aC, aW,  0.0f - px,  2.0f - py, dx, dy, l2x, l2y, lob, clp, n);
    const float r = Rcp(aW);
    C3 out = { Mn(mx4.r, Mx(mn4.r, aC.r * r)), Mn(mx4.g, Mx(mn4.g, aC.g * r)), Mn(mx4.b, Mx(mn4.b, aC.b * r)) };
    return out;
}

static inline unsigned char A8(float v) { v = v * 255.0f + 0.5f; return (unsigned char)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v)); }

struct Trabajo {
    const unsigned char* src; int sw, sh;
    unsigned char* dst; int dw, dh;
    std::vector<unsigned char>* medio;   // la salida de EASU (la entrada de RCAS)
    float afilado;                       // exp2(-nitidez)
};
static void EasuFilas(const Trabajo& t, int y0, int y1) {
    Img im; im.p = t.src; im.w = t.sw; im.h = t.sh;
    const float sx = (float)t.sw / (float)t.dw, sy = (float)t.sh / (float)t.dh;
    unsigned char* m = &(*t.medio)[0];
    for (int y = y0; y < y1; y++) {
        const float ppy = ((float)y + 0.5f) * sy - 0.5f;
        int ay = (int)(((float)y + 0.5f) * sy); if (ay >= t.sh) ay = t.sh - 1;
        for (int x = 0; x < t.dw; x++) {
            const float ppx = ((float)x + 0.5f) * sx - 0.5f;
            const C3 c = Easu(im, ppx, ppy);
            unsigned char* q = m + ((size_t)y * t.dw + x) * 4;
            int ax = (int)(((float)x + 0.5f) * sx); if (ax >= t.sw) ax = t.sw - 1;
            q[0] = A8(c.r); q[1] = A8(c.g); q[2] = A8(c.b); q[3] = t.src[((size_t)ay * t.sw + ax) * 4 + 3];
        }
    }
}
// FsrRcasF: el afilado (la cruz b d e f h), con el limite de AMD (FSR_RCAS_LIMIT = 0.25 - 1/16)
static void RcasFilas(const Trabajo& t, int y0, int y1) {
    Img im; im.p = &(*t.medio)[0]; im.w = t.dw; im.h = t.dh;
    const float kLimite = 0.25f - (1.0f / 16.0f);
    for (int y = y0; y < y1; y++) {
        for (int x = 0; x < t.dw; x++) {
            const C3 b = im.At(x, y - 1), d = im.At(x - 1, y), e = im.At(x, y), f = im.At(x + 1, y), h = im.At(x, y + 1);
            const float mn4R = Mn(Mn(b.r, d.r), Mn(f.r, h.r)), mn4G = Mn(Mn(b.g, d.g), Mn(f.g, h.g)), mn4B = Mn(Mn(b.b, d.b), Mn(f.b, h.b));
            const float mx4R = Mx(Mx(b.r, d.r), Mx(f.r, h.r)), mx4G = Mx(Mx(b.g, d.g), Mx(f.g, h.g)), mx4B = Mx(Mx(b.b, d.b), Mx(f.b, h.b));
            const float hitMinR = Mn(mn4R, e.r) * Rcp(4.0f * mx4R), hitMinG = Mn(mn4G, e.g) * Rcp(4.0f * mx4G), hitMinB = Mn(mn4B, e.b) * Rcp(4.0f * mx4B);
            const float hitMaxR = (1.0f - Mx(mx4R, e.r)) * Rcp(4.0f * mn4R - 4.0f);
            const float hitMaxG = (1.0f - Mx(mx4G, e.g)) * Rcp(4.0f * mn4G - 4.0f);
            const float hitMaxB = (1.0f - Mx(mx4B, e.b)) * Rcp(4.0f * mn4B - 4.0f);
            const float lobeR = Mx(-hitMinR, hitMaxR), lobeG = Mx(-hitMinG, hitMaxG), lobeB = Mx(-hitMinB, hitMaxB);
            const float lobe = Mx(-kLimite, Mn(Mx(Mx(lobeR, lobeG), lobeB), 0.0f)) * t.afilado;
            const float rcpL = Rcp(4.0f * lobe + 1.0f);
            unsigned char* q = t.dst + ((size_t)y * t.dw + x) * 4;
            q[0] = A8((lobe * (b.r + d.r + f.r + h.r) + e.r) * rcpL);
            q[1] = A8((lobe * (b.g + d.g + f.g + h.g) + e.g) * rcpL);
            q[2] = A8((lobe * (b.b + d.b + f.b + h.b) + e.b) * rcpL);
            q[3] = (*t.medio)[((size_t)y * t.dw + x) * 4 + 3];
        }
    }
}
static void EnHilos(void (*fn)(const Trabajo&, int, int), const Trabajo& t, int filas, int hilos) {
#ifdef FSR_HILOS
    if (hilos <= 0) { hilos = (int)std::thread::hardware_concurrency(); if (hilos < 1) hilos = 1; if (hilos > 16) hilos = 16; }
    if (hilos > 1 && filas >= hilos * 8) {
        std::vector<std::thread> hs;
        for (int i = 0; i < hilos; i++) {
            const int a = filas * i / hilos, b = filas * (i + 1) / hilos;
            hs.push_back(std::thread(fn, std::cref(t), a, b));
        }
        for (size_t i = 0; i < hs.size(); i++) hs[i].join();
        return;
    }
#else
    (void)hilos;
#endif
    fn(t, 0, filas);
}
} // namespace

void W3dFSR1(const unsigned char* src, int sw, int sh, unsigned char* dst, int dw, int dh, float nitidez, int hilos) {
    if (!src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    std::vector<unsigned char> medio((size_t)dw * dh * 4);
    Trabajo t; t.src = src; t.sw = sw; t.sh = sh; t.dst = dst; t.dw = dw; t.dh = dh; t.medio = &medio;
    if (nitidez < 0.0f) nitidez = 0.0f;
    t.afilado = powf(2.0f, -nitidez);
    EnHilos(EasuFilas, t, dh, hilos);
    EnHilos(RcasFilas, t, dh, hilos);
}
