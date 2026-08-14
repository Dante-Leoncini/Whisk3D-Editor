// ============================================================================
//  Fuente2D.cpp — ver Fuente2D.h. El horneado TTF usa stb_truetype (vendored).
// ============================================================================
#include "io/Fuente2D.h"
#include "WhiskUI/text/W3dTextAtlas.h"
#include "WhiskUI/text/W3dFont.h"   // la fuente PIXEL de Whisk3D (font.png del skin)
#include "w3dTexture.h"             // DecodeImage/UploadRGBA (premultiplicar el alpha)
#ifndef W3D_GAME_RUNTIME
#include "variables.h"              // cfg.SkinName (de que skin sale font.png)
#else
// el runtime del juego provee la RUTA de font.png (no hay cfg del editor)
extern const char* W3dGameFontPng();
#endif
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include "io/JsonW3d.h"             // parser del .json de glifos de una fuente BITMAP

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include <map>
#include <vector>
#include <math.h>
#include <string.h>   // memcpy (padding POT del atlas)

namespace gfx = w3dEngine;

// cache: una fuente se hornea UNA vez por sesion ("" = la default de Whisk3D)
static std::map<std::string, w3dui::W3dTextAtlas*> gFuentes;

// ---------------------------------------------------------------------------
//  ATLAS SIEMPRE POT. GLES 1.1 (N95/Symbian) NO soporta texturas non-power-of-two:
//  glTexImage2D con un PNG de glifos NPOT (429x165 en el caso medido) FALLA y la textura
//  queda INCOMPLETA -> se muestrea NEGRO OPACO y cada glifo se dibuja como una
//  CAJA NEGRA (con la mezcla premultiplicada: dst = rgb(0) + dst*(1-alpha=0)).
//  Era el bug de "el texto de la UI pierde la transparencia": en desktop GL no se
//  ve (NPOT es core desde GL 2.0), en el telefono si. La fuente default (font.png
//  128x128) y el TTF horneado (512x512) ya eran POT; la BITMAP nueva no.
//  El arreglo de raiz: acolchar a la POT siguiente con CEROS (transparente
//  premultiplicado: el sangrado del sampler cae en nada) y calcular las UV contra
//  el tamano ACOLCHADO. Devuelve el handle; escribe el tamano real subido.
// ---------------------------------------------------------------------------
static int PotSiguiente(int n) { int p = 1; while (p < n) p <<= 1; return p; }
static unsigned SubirAtlasPOT(const unsigned char* rgba, int w, int h,
                              bool filtrar, int* outW, int* outH) {
    int pw = PotSiguiente(w), ph = PotSiguiente(h);
    *outW = pw; *outH = ph;
    if (pw == w && ph == h) return gfx::UploadRGBA(rgba, w, h, filtrar);
    std::vector<unsigned char> pad((size_t)pw * ph * 4, 0);
    for (int y = 0; y < h; y++)
        memcpy(&pad[(size_t)y * pw * 4], rgba + (size_t)y * w * 4, (size_t)w * 4);
    return gfx::UploadRGBA(&pad[0], pw, ph, filtrar);
}

// mismos codepoints extra que bake_atlas.py (acentos del espanol)
static const unsigned kExtras[] = {
    0xE1,0xE9,0xED,0xF3,0xFA, 0xC1,0xC9,0xCD,0xD3,0xDA, 0xF1,0xD1, 0xFC,0xDC, 0xBF,0xA1
};
static const int kNumExtras = (int)(sizeof(kExtras)/sizeof(kExtras[0]));

// ---- default: la fuente PIXEL de Whisk3D (font.png del skin, la MISMA del editor) ----------
// Grilla de 5x11 en un atlas de 128: se arma un W3dTextAtlas con las UVs de W3dFontGetGlyph.
// Nitida a multiplos enteros (pixel-perfect, como en el N95); NEAREST para no emborronarla.
static w3dui::W3dTextAtlas* CargarDefault() {
#ifndef W3D_GAME_RUNTIME
    std::string ruta = w3dFileSystem::GetResDir() + "/Skins/" + cfg.SkinName + "/font.png";
#else
    std::string ruta = W3dGameFontPng();
#endif
    unsigned char* rgba = NULL; int w = 0, h = 0;
    if (!gfx::DecodeImage(ruta.c_str(), &rgba, &w, &h) || !rgba) {
        w3dLogfE("Fuente2D: no pude leer %s", ruta.c_str());
        return NULL;
    }
    for (int i = 0; i < w * h; i++) {   // premultiplicar (el atlas mezcla premultiplicado)
        unsigned char a = rgba[i*4+3];
        rgba[i*4+0] = (unsigned char)(rgba[i*4+0] * a / 255);
        rgba[i*4+1] = (unsigned char)(rgba[i*4+1] * a / 255);
        rgba[i*4+2] = (unsigned char)(rgba[i*4+2] * a / 255);
    }
    // POT tambien aca: el font.png default es 128x128, pero un skin del usuario
    // puede traer otro tamano y en GLES1.1 un NPOT es una caja negra (ver arriba)
    int tw = PotSiguiente(w), th = PotSiguiente(h);
    w3dui::W3dTextAtlas* at = new w3dui::W3dTextAtlas();
    at->fontPx = W3dFont_GlyphH; at->atlasW = tw; at->atlasH = th;
    at->pixelPerfect = true;   // grilla pixel: solo multiplos enteros (nitida como el editor)
    at->ascent = W3dFont_GlyphH; at->lineH = W3dFont_LineH;
    for (unsigned cp = 32; cp < 256; cp++) {
        int gx = 0, gy = 0;   // posicion en PIXELES dentro del archivo (las UVs salen de
        // aca contra el tamano SUBIDO: la textura del EDITOR puede tener otro tamano)
        if (!W3dFontGetGlyphPx((unsigned short)cp, &gx, &gy) && cp != '?') continue;
        w3dui::W3dAtlasGlyph g;
        g.u0 = (float)gx / tw;                      g.v0 = (float)gy / th;
        g.u1 = (float)(gx + W3dFont_GlyphW) / tw;   g.v1 = (float)(gy + W3dFont_GlyphH) / th;
        g.w = W3dFont_GlyphW; g.h = W3dFont_GlyphH;
        g.xoff = 0; g.yoff = 0;
        g.advance = W3dFont_Advance;
        at->glyphs[cp] = g;
    }
    // el ESPACIO: el atlas de la grilla no lo trae; sin este glifo las palabras quedan pegadas
    if (at->glyphs.find(32) == at->glyphs.end()) {
        w3dui::W3dAtlasGlyph esp;
        esp.u0 = esp.v0 = esp.u1 = esp.v1 = 0.0f;
        esp.w = 0; esp.h = 0; esp.xoff = 0; esp.yoff = 0;
        esp.advance = W3dFont_Advance;
        at->glyphs[32] = esp;
    }
    at->tex = SubirAtlasPOT(rgba, w, h, false, &tw, &th);   // NEAREST: pixel-perfect al escalar
    gfx::FreeImage(rgba);
    if (!at->tex) { delete at; return NULL; }
    gfx::BindTexture(at->tex); gfx::TexFilter(false); gfx::TexWrap(false);
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
    if (!at->tex) { delete at; return NULL; }
    gfx::BindTexture(at->tex); gfx::TexFilter(true); gfx::TexWrap(false);
    w3dLogf("Fuente2D: horneada %s (%d glifos)", ruta.c_str(), (int)at->glyphs.size());
    return at;
}

// ---- fuente BITMAP: un .png + su .json de glifos ------------------------------------------
// Formato completo en formato/fuente-bitmap.md; el generador de referencia es
// tools/build_fuente_bitmap.py (cualquier extractor de un proyecto escribe lo mismo).
// JSON esperado: { "textura": "x.png", "alto_linea": N, "mayusculas_si_falta": false,
//                  "glifos": { "A": {"x":..,"y":..,"w":..,"h":..,
//                                    "xoff":..,"yoff":..,"avance":..}, ... } }
// xoff / yoff / avance son OPCIONALES: el default apoya el glifo en la BASE del renglon
// (yoff = alto_linea - h) sin bearing, que es el caso de una fuente sin descendentes.
// Una tipografia con 'g'/'p'/'y'/coma declara su yoff y su alto real.
// Pixel-perfect: NEAREST + solo multiplos enteros.
static double JNum(JVal* o, const char* k, double def) {
    if (!o || o->tipo != 4) return def;
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->tipo == 1) ? it->second->num : def;
}
// booleano tolerante: acepta true/false (tipo 3) y tambien 0/1 numerico
static bool JBool(JVal* o, const char* k, bool def) {
    if (!o || o->tipo != 4) return def;
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    if (it == o->obj.end() || !it->second) return def;
    if (it->second->tipo == 3) return it->second->b;
    if (it->second->tipo == 1) return it->second->num != 0.0;
    return def;
}
static w3dui::W3dTextAtlas* CargarBitmapJson(const std::string& ruta) {
    // acepta la ruta del .png O la del .json: el hermano es mismo nombre, otra extension
    size_t pt = ruta.find_last_of('.');
    std::string sinExt = (pt == std::string::npos) ? ruta : ruta.substr(0, pt);
    std::string ext = (pt == std::string::npos) ? std::string() : ruta.substr(pt);
    for (size_t i = 0; i < ext.size(); i++)
        if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = (char)(ext[i] + 32);
    const bool dieronPng = (ext == ".png");
    std::string rutaJson = sinExt + ".json";
    std::vector<unsigned char> js;
    if (!w3dFileSystem::ReadFileBytes(rutaJson, js) || js.empty()) {
        // v4: el contenedor clasifica los .json bajo "extra/" (el png queda en
        // "texturas/") -> el hermano por nombre no resuelve; se busca ahi.
        size_t b = rutaJson.find_last_of("/\\");
        std::string alt = "extra/" + (b == std::string::npos ? rutaJson : rutaJson.substr(b + 1));
        if (!w3dFileSystem::ReadFileBytes(alt, js) || js.empty()) {
            w3dLogfE("Fuente2D: falta el json de glifos %s", rutaJson.c_str());
            return NULL;
        }
        rutaJson = alt;
    }
    JParser par((const char*)&js[0], js.size());
    JVal* root = par.Valor();
    if (!root || root->tipo != 4) { delete root; return NULL; }
    int altoLinea = (int)JNum(root, "alto_linea", 0.0);
    if (altoLinea < 1) altoLinea = 1;
    // el PNG: si el caller dio la ruta del png, ESA manda (contrato de fuenteBitmap);
    // si dio el .json, se usa su campo "textura" (relativo a la carpeta del json).
    std::string rutaPng = sinExt + ".png";
    std::map<std::string, JVal*>::iterator itT = root->obj.find("textura");
    if (!dieronPng && itT != root->obj.end() && itT->second->tipo == 2 && !itT->second->str.empty()) {
        size_t b = rutaJson.find_last_of("/\\");
        std::string dir = (b == std::string::npos) ? "" : rutaJson.substr(0, b + 1);
        const std::string& t = itT->second->str;
        rutaPng = (t[0] == '/' || (t.size() > 1 && t[1] == ':')) ? t : dir + t;
    }
    unsigned char* rgba = NULL; int w = 0, h = 0;
    if (!gfx::DecodeImage(rutaPng.c_str(), &rgba, &w, &h) || !rgba || w < 1 || h < 1) {
        w3dLogfE("Fuente2D: no pude leer %s", rutaPng.c_str());
        delete root; return NULL;
    }
    // GUARDIA "RGB sin alpha": DecodeImage fuerza 4 canales, asi que un PNG que no
    // trae canal alpha llega con a=255 en TODOS los pixels -> el fondo del glifo
    // saldria OPACO (caja del color del fondo) y nadie diria por que. Se avisa.
    {
        bool todoOpaco = true;
        for (int i = 0; i < w * h && todoOpaco; i++) todoOpaco = (rgba[i*4+3] == 255);
        if (todoOpaco)
            w3dLogfW("Fuente2D: %s no trae canal alpha (o esta todo opaco): el fondo de los glifos va a salir OPACO", rutaPng.c_str());
    }
    for (int i = 0; i < w * h; i++) {   // premultiplicar (el atlas mezcla premultiplicado)
        unsigned char a = rgba[i*4+3];
        rgba[i*4+0] = (unsigned char)(rgba[i*4+0] * a / 255);
        rgba[i*4+1] = (unsigned char)(rgba[i*4+1] * a / 255);
        rgba[i*4+2] = (unsigned char)(rgba[i*4+2] * a / 255);
    }
    // el atlas SIEMPRE se sube POT (ver SubirAtlasPOT): las UV se calculan contra
    // el tamano ACOLCHADO (tw/th), no contra el del PNG.
    int tw = PotSiguiente(w), th = PotSiguiente(h);
    w3dui::W3dTextAtlas* at = new w3dui::W3dTextAtlas();
    at->atlasW = tw; at->atlasH = th;
    at->fontPx = altoLinea; at->ascent = altoLinea; at->lineH = altoLinea;
    at->pixelPerfect = true;   // pixel-art: multiplos enteros + pen entero (como la default)
    std::map<std::string, JVal*>::iterator itG = root->obj.find("glifos");
    if (itG != root->obj.end() && itG->second->tipo == 4) {
        for (std::map<std::string, JVal*>::iterator g = itG->second->obj.begin();
             g != itG->second->obj.end(); ++g) {
            if (g->first.empty() || !g->second || g->second->tipo != 4) continue;
            const char* k = g->first.c_str();
            unsigned cp = w3dui::W3dTextAtlas::NextCp(k);   // la clave es UN caracter (utf-8)
            int gx = (int)JNum(g->second, "x", 0.0), gy = (int)JNum(g->second, "y", 0.0);
            int gw = (int)JNum(g->second, "w", 0.0), gh = (int)JNum(g->second, "h", 0.0);
            w3dui::W3dAtlasGlyph gl;
            gl.u0 = (float)gx / tw;        gl.v0 = (float)gy / th;
            gl.u1 = (float)(gx + gw) / tw; gl.v1 = (float)(gy + gh) / th;
            gl.w = (short)gw; gl.h = (short)gh;
            // xoff / yoff / avance son OPCIONALES y su default es el caso simple
            // (todos los glifos apoyados en la base del renglon, sin bearing).
            // Declararlos es lo que permite representar una tipografia completa:
            //   * yoff  = cuanto BAJA el glifo desde el tope del renglon. El default
            //     `altoLinea - gh` apoya en la base, que solo sirve si NINGUN glifo
            //     tiene descendente; con 'g', 'p', 'y' o una coma da negativo y los
            //     levanta por encima del renglon.
            //   * xoff  = bearing izquierdo (el hueco antes del trazo).
            //   * avance = cuanto corre el lapiz; default gw + 1 px de separacion.
            gl.xoff    = (short)JNum(g->second, "xoff",   0.0);
            gl.yoff    = (short)JNum(g->second, "yoff",   (double)(altoLinea - gh));
            gl.advance = (short)JNum(g->second, "avance", (double)(gw + 1));
            at->glyphs[cp] = gl;
        }
    }
    // "mayusculas_si_falta": OPT-IN del propio archivo. Una fuente que a proposito
    // no trae minusculas (muchas tipografias de juego lo son) puede pedir que cada
    // minuscula ausente caiga en su mayuscula. Sin esta clave, un codepoint sin
    // glifo cae en el fallback normal del atlas ('?') y se avisa una vez: escribir
    // texto en MAYUSCULAS sin que nadie lo haya pedido es una sorpresa cara.
    if (JBool(root, "mayusculas_si_falta", false)) {
        for (unsigned cp = 'a'; cp <= 'z'; cp++)
            if (at->glyphs.find(cp) == at->glyphs.end() &&
                at->glyphs.find(cp - 32) != at->glyphs.end())
                at->glyphs[cp] = at->glyphs[cp - 32];
    } else {
        int faltan = 0; unsigned primera = 0;
        for (unsigned cp = 'a'; cp <= 'z'; cp++)
            if (at->glyphs.find(cp) == at->glyphs.end()) { if (!faltan) primera = cp; faltan++; }
        if (faltan)
            w3dLogfW("Fuente2D: %s no trae %d minuscula(s) (la primera es '%c') y no declara "
                     "\"mayusculas_si_falta\": esos codepoints van a caer en el glifo de reemplazo",
                     rutaJson.c_str(), faltan, (char)primera);
    }
    delete root;
    if (at->glyphs.empty()) { gfx::FreeImage(rgba); delete at; return NULL; }
    at->tex = SubirAtlasPOT(rgba, w, h, false, &tw, &th);   // NEAREST: pixel-perfect al escalar
    gfx::FreeImage(rgba);
    if (!at->tex) { delete at; return NULL; }
    gfx::BindTexture(at->tex); gfx::TexFilter(false); gfx::TexWrap(false);
    w3dLogf("Fuente2D: bitmap %s (%d glifos, linea %d px)",
            rutaPng.c_str(), (int)at->glyphs.size(), altoLinea);
    return at;
}

// true si la ruta pinta a fuente BITMAP (png/json) y no a ttf
static bool EsRutaBitmap(const std::string& r) {
    size_t p = r.find_last_of('.');
    if (p == std::string::npos) return false;
    std::string ext = r.substr(p);
    for (size_t i = 0; i < ext.size(); i++)
        if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = (char)(ext[i] + 32);
    return ext == ".png" || ext == ".json";
}

w3dui::W3dTextAtlas* Fuente2DObtener(const std::string& ruta) {
    std::map<std::string, w3dui::W3dTextAtlas*>::iterator it = gFuentes.find(ruta);
    if (it != gFuentes.end()) return it->second;
    w3dui::W3dTextAtlas* at = ruta.empty() ? CargarDefault()
                            : (EsRutaBitmap(ruta) ? CargarBitmapJson(ruta) : HornearTTF(ruta));
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
