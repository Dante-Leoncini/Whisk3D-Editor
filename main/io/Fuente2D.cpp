// ============================================================================
//  Fuente2D.cpp — ver Fuente2D.h. El horneado TTF usa stb_truetype (vendored).
// ============================================================================
#include "io/Fuente2D.h"
#include "WhiskUI/text/W3dTextAtlas.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include <map>
#include <vector>
#include <math.h>

namespace gfx = w3dEngine;

// cache: una fuente se hornea UNA vez por sesion ("" = la default de Whisk3D)
static std::map<std::string, w3dui::W3dTextAtlas*> gFuentes;

// mismos codepoints extra que bake_atlas.py (acentos del espanol)
static const unsigned kExtras[] = {
    0xE1,0xE9,0xED,0xF3,0xFA, 0xC1,0xC9,0xCD,0xD3,0xDA, 0xF1,0xD1, 0xFC,0xDC, 0xBF,0xA1
};
static const int kNumExtras = (int)(sizeof(kExtras)/sizeof(kExtras[0]));

// ---- default: la fuente de Whisk3D (Inter), ya horneada en res/ ----------------------------
static w3dui::W3dTextAtlas* CargarDefault() {
    std::vector<unsigned char> fnt, png;
    const std::string base = w3dFileSystem::GetResDir();
    if (!w3dFileSystem::ReadFileBytes(base + "/inter_atlas.w3dfnt", fnt) ||
        !w3dFileSystem::ReadFileBytes(base + "/inter_atlas.png", png)) {
        w3dLogE("Fuente2D: falta res/inter_atlas.* (la fuente de Whisk3D)");
        return NULL;
    }
    w3dui::W3dTextAtlas* at = new w3dui::W3dTextAtlas();
    if (!at->Load(&fnt[0], fnt.size(), &png[0], png.size())) { delete at; return NULL; }
    return at;
}

// ---- horneado de un TTF en runtime ---------------------------------------------------------
// Rellena el W3dTextAtlas igual que lo haria bake_atlas.py + Load(): glifos empaquetados en
// una hoja de 512 a 64px, con yoff medido desde la CIMA DEL ASCENDER (stb lo da desde el
// baseline: se corrige sumando el ascent).
static w3dui::W3dTextAtlas* HornearTTF(const std::string& ruta) {
    std::vector<unsigned char> ttf;
    if (!w3dFileSystem::ReadFileBytes(ruta, ttf) || ttf.empty()) {
        w3dLogfE("Fuente2D: no pude leer %s", ruta.c_str());
        return NULL;
    }
    stbtt_fontinfo fi;
    if (!stbtt_InitFont(&fi, &ttf[0], stbtt_GetFontOffsetForIndex(&ttf[0], 0))) {
        w3dLogfE("Fuente2D: %s no parece un TTF valido", ruta.c_str());
        return NULL;
    }

    const int FPX = 64, AT = 512;   // mismos valores que el atlas de Whisk3D (nitido hasta ~64px)
    std::vector<unsigned char> gris((size_t)AT * AT, 0);

    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, &gris[0], AT, AT, 0, 1, NULL)) return NULL;
    stbtt_PackSetOversampling(&pc, 1, 1);

    stbtt_packedchar ascii[95], extras[kNumExtras];
    int cps[kNumExtras];
    for (int i = 0; i < kNumExtras; i++) cps[i] = (int)kExtras[i];
    stbtt_pack_range rangos[2];
    rangos[0].font_size = (float)FPX;
    rangos[0].first_unicode_codepoint_in_range = 32;
    rangos[0].array_of_unicode_codepoints = NULL;
    rangos[0].num_chars = 95;
    rangos[0].chardata_for_range = ascii;
    rangos[1].font_size = (float)FPX;
    rangos[1].first_unicode_codepoint_in_range = 0;
    rangos[1].array_of_unicode_codepoints = cps;
    rangos[1].num_chars = kNumExtras;
    rangos[1].chardata_for_range = extras;
    stbtt_PackFontRanges(&pc, &ttf[0], 0, rangos, 2);
    stbtt_PackEnd(&pc);

    int asc, desc, gap;
    stbtt_GetFontVMetrics(&fi, &asc, &desc, &gap);
    const float sc = stbtt_ScaleForPixelHeight(&fi, (float)FPX);

    w3dui::W3dTextAtlas* at = new w3dui::W3dTextAtlas();
    at->fontPx = FPX; at->atlasW = AT; at->atlasH = AT;
    at->ascent = (int)(asc * sc + 0.5f);
    at->lineH  = (int)((asc - desc + gap) * sc + 0.5f);

    for (int r = 0; r < 2; r++) {
        stbtt_packedchar* pcs = (r == 0) ? ascii : extras;
        int n = (r == 0) ? 95 : kNumExtras;
        for (int i = 0; i < n; i++) {
            unsigned cp = (r == 0) ? (unsigned)(32 + i) : kExtras[i];
            const stbtt_packedchar& q = pcs[i];
            w3dui::W3dAtlasGlyph g;
            g.u0 = q.x0 / (float)AT; g.v0 = q.y0 / (float)AT;
            g.u1 = q.x1 / (float)AT; g.v1 = q.y1 / (float)AT;
            g.w = (short)(q.x1 - q.x0); g.h = (short)(q.y1 - q.y0);
            g.xoff = (short)floorf(q.xoff + 0.5f);
            // stb mide yoff desde el BASELINE (negativo hacia arriba); el atlas lo quiere
            // desde la cima del ascender, hacia abajo.
            g.yoff = (short)(at->ascent + (int)floorf(q.yoff + 0.5f));
            g.advance = (short)floorf(q.xadvance + 0.5f);
            at->glyphs[cp] = g;
        }
    }

    // gris -> RGBA premultiplicado (idem Load: alpha = cobertura) y a la GPU
    std::vector<unsigned char> rgba((size_t)AT * AT * 4);
    for (int i = 0; i < AT * AT; i++) {
        unsigned char v = gris[i];
        rgba[i*4+0] = v; rgba[i*4+1] = v; rgba[i*4+2] = v; rgba[i*4+3] = v;
    }
    at->tex = gfx::UploadRGBA(&rgba[0], AT, AT, true);
    gfx::BindTexture(at->tex); gfx::TexFilter(true); gfx::TexWrap(false);
    if (!at->tex) { delete at; return NULL; }
    w3dLogf("Fuente2D: horneada %s (%d glifos)", ruta.c_str(), (int)at->glyphs.size());
    return at;
}

w3dui::W3dTextAtlas* Fuente2DObtener(const std::string& ruta) {
    std::map<std::string, w3dui::W3dTextAtlas*>::iterator it = gFuentes.find(ruta);
    if (it != gFuentes.end()) return it->second;
    w3dui::W3dTextAtlas* at = ruta.empty() ? CargarDefault() : HornearTTF(ruta);
    gFuentes[ruta] = at;   // se cachea aunque sea NULL (no reintentar por frame)
    return at;
}

std::string Fuente2DNombre(const std::string& ruta) {
    if (ruta.empty()) return "Whisk3D";
    size_t b = ruta.find_last_of("/\\");
    std::string n = (b == std::string::npos) ? ruta : ruta.substr(b + 1);
    size_t p = n.find_last_of('.');
    if (p != std::string::npos) n = n.substr(0, p);
    return n;
}

// ---- vista previa para el file browser -----------------------------------------------------
bool Fuente2DThumb(const std::string& rutaTtf, int maxPx,
                   unsigned char** outRGBA, int* outW, int* outH) {
    std::vector<unsigned char> ttf;
    if (!w3dFileSystem::ReadFileBytes(rutaTtf, ttf) || ttf.empty()) return false;
    stbtt_fontinfo fi;
    if (!stbtt_InitFont(&fi, &ttf[0], stbtt_GetFontOffsetForIndex(&ttf[0], 0))) return false;

    const char* MUESTRA = "AaBb";
    const int W = maxPx * 2, H = maxPx;             // miniatura apaisada 2:1
    const float px = H * 0.62f;
    const float sc = stbtt_ScaleForPixelHeight(&fi, px);
    int asc, desc, gap; stbtt_GetFontVMetrics(&fi, &asc, &desc, &gap);

    std::vector<unsigned char> lienzo((size_t)W * H, 0);
    float x = 2.0f;
    const float base = (H + asc * sc) * 0.5f;       // baseline centrado vertical
    for (const char* s = MUESTRA; *s; s++) {
        int gw, gh, gx, gy;
        unsigned char* bm = stbtt_GetCodepointBitmap(&fi, sc, sc, *s, &gw, &gh, &gx, &gy);
        if (bm) {
            int ox = (int)x + gx, oy = (int)base + gy;
            for (int j = 0; j < gh; j++) for (int i = 0; i < gw; i++) {
                int dx = ox + i, dy = oy + j;
                if (dx < 0 || dy < 0 || dx >= W || dy >= H) continue;
                unsigned char v = bm[j * gw + i];
                if (v > lienzo[dy * W + dx]) lienzo[dy * W + dx] = v;
            }
            stbtt_FreeBitmap(bm, NULL);
        }
        int adv, lsb; stbtt_GetCodepointHMetrics(&fi, *s, &adv, &lsb);
        x += adv * sc;
        if (x > W - 2) break;
    }

    unsigned char* rgba = new unsigned char[(size_t)W * H * 4];
    for (int i = 0; i < W * H; i++) {
        unsigned char v = lienzo[i];
        rgba[i*4+0] = 235; rgba[i*4+1] = 235; rgba[i*4+2] = 235; rgba[i*4+3] = v;   // letra clara
    }
    *outRGBA = rgba; *outW = W; *outH = H;
    return true;
}
