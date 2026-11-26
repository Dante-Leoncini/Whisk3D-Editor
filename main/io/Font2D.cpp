// ============================================================================
//  Font2D.cpp — ver Font2D.h. El horneado TTF usa stb_truetype (vendored).
// ============================================================================
#include "io/Font2D.h"
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
#include <set>
#include <vector>
#include <math.h>
#include <string.h>   // memcpy (padding POT del atlas), memset

namespace gfx = w3dEngine;

// cache: una fuente se hornea UNA vez por proyecto abierto ("" = la default de Whisk3D)
static std::map<std::string, w3dui::W3dTextAtlas*> gFonts;
// las TTF horneadas al tamano de PANTALLA (Font2DGetPx), por ruta: at[n] = la de n px (6..63); tried[n] = ya se horneo
// (si fallo queda NULL: no se reintenta por cuadro). Una busqueda por la ruta y el tamano es el indice: lo pide cada
// texto y cada boton en cada cuadro, y antes era armar una clave nueva ("@n|ruta", heap) para buscarla
namespace {
struct FontSizes {
    w3dui::W3dTextAtlas* at[64];
    bool tried[64];
    FontSizes() { memset(at, 0, sizeof(at)); memset(tried, 0, sizeof(tried)); }
};
}
static std::map<std::string, FontSizes> gFontsPx;
// las que usan una textura AJENA (atlas_unico: el id es del cache 2D/3D): al liberar no se borra
static std::set<w3dui::W3dTextAtlas*> gBorrowedFonts;

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
static int NextPot(int n) { int p = 1; while (p < n) p <<= 1; return p; }
static unsigned UploadAtlasPOT(const unsigned char* rgba, int w, int h,
                              bool filter, int* outW, int* outH) {
    int pw = NextPot(w), ph = NextPot(h);
    *outW = pw; *outH = ph;
    if (pw == w && ph == h) return gfx::UploadRGBA(rgba, w, h, filter);
    std::vector<unsigned char> pad((size_t)pw * ph * 4, 0);
    for (int y = 0; y < h; y++)
        memcpy(&pad[(size_t)y * pw * 4], rgba + (size_t)y * w * 4, (size_t)w * 4);
    return gfx::UploadRGBA(&pad[0], pw, ph, filter);
}

// mismos codepoints extra que bake_atlas.py (acentos del espanol)
static const unsigned kExtras[] = {
    0xE1,0xE9,0xED,0xF3,0xFA, 0xC1,0xC9,0xCD,0xD3,0xDA, 0xF1,0xD1, 0xFC,0xDC, 0xBF,0xA1
};
static const int kNumExtras = (int)(sizeof(kExtras)/sizeof(kExtras[0]));

// ---- default: la fuente PIXEL de Whisk3D (font.png del skin, la MISMA del editor) ----------
// Grilla de 5x11 en un atlas de 128: se arma un W3dTextAtlas con las UVs de W3dFontGetGlyph.
// Nitida a multiplos enteros (pixel-perfect, como en el N95); NEAREST para no emborronarla.
static w3dui::W3dTextAtlas* LoadDefault() {
#ifndef W3D_GAME_RUNTIME
    // el MISMO font.png que carga el editor (W3dInitUI): en Symbian esta en la carpeta privada (flat),
    // no en GetResDir()+"/Skins/..." (que ahi es "" -> la UI2D quedaba sin texto). g_skinDir trae el
    // separador final que puso cada plataforma.
    extern std::string g_skinDir;
    std::string path = g_skinDir + "font.png";
#else
    std::string path = W3dGameFontPng();
#endif
    unsigned char* rgba = NULL; int w = 0, h = 0;
    if (!gfx::DecodeImage(path.c_str(), &rgba, &w, &h) || !rgba) {
        w3dLogfE("Fuente2D: no pude leer %s", path.c_str());
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
    int tw = NextPot(w), th = NextPot(h);
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
    at->tex = UploadAtlasPOT(rgba, w, h, false, &tw, &th);   // NEAREST: pixel-perfect al escalar
    gfx::FreeImage(rgba);
    if (!at->tex) { delete at; return NULL; }
    gfx::BindTexture(at->tex); gfx::TexFilter(false); gfx::TexWrap(false);
    return at;
}

// ---- horneado de un TTF en runtime ---------------------------------------------------------
// Rellena el W3dTextAtlas igual que lo haria bake_atlas.py + Load(): glifos empaquetados en
// una hoja de 512 a 64px, con yoff medido desde la CIMA DEL ASCENDER (stb lo da desde el
// baseline: se corrige sumando el ascent).
// FPX = el alto al que se hornea. 64 es el atlas de siempre (se escala libre); uno MENOR es el de un tamano de PANTALLA
// (Font2DGetPx): la hoja mas chica en la que entran los glifos, y se dibuja 1:1 en pixel entero (pixelPerfect)
static w3dui::W3dTextAtlas* BakeTTF(const std::string& path, int FPX = 64) {
    std::vector<unsigned char> ttf;
    if (!w3dFileSystem::ReadFileBytes(path, ttf) || ttf.empty()) {
        w3dLogfE("Fuente2D: no pude leer %s", path.c_str());
        return NULL;
    }
    stbtt_fontinfo fi;
    if (!stbtt_InitFont(&fi, &ttf[0], stbtt_GetFontOffsetForIndex(&ttf[0], 0))) {
        w3dLogfE("Fuente2D: %s no parece un TTF valido", path.c_str());
        return NULL;
    }

    // (el de 64 px: 512, como el atlas de Whisk3D; uno chico: desde 64 x 64, doblando hasta que entran todos)
    int AT = (FPX >= 64) ? 512 : 64;
    std::vector<unsigned char> gray;
    stbtt_packedchar ascii[95], extras[kNumExtras];
    int cps[kNumExtras];
    for (int i = 0; i < kNumExtras; i++) cps[i] = (int)kExtras[i];
    for (;;) {
        gray.assign((size_t)AT * AT, 0);
        stbtt_pack_context pc;
        if (!stbtt_PackBegin(&pc, &gray[0], AT, AT, 0, 1, NULL)) return NULL;
        stbtt_PackSetOversampling(&pc, 1, 1);
        stbtt_pack_range ranges[2];
        ranges[0].font_size = (float)FPX;
        ranges[0].first_unicode_codepoint_in_range = 32;
        ranges[0].array_of_unicode_codepoints = NULL;
        ranges[0].num_chars = 95;
        ranges[0].chardata_for_range = ascii;
        ranges[1].font_size = (float)FPX;
        ranges[1].first_unicode_codepoint_in_range = 0;
        ranges[1].array_of_unicode_codepoints = cps;
        ranges[1].num_chars = kNumExtras;
        ranges[1].chardata_for_range = extras;
        const int entered = stbtt_PackFontRanges(&pc, &ttf[0], 0, ranges, 2);
        stbtt_PackEnd(&pc);
        if (entered || AT >= 1024) break;
        AT *= 2;
    }

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
        unsigned char v = gray[i];
        rgba[i*4+0] = v; rgba[i*4+1] = v; rgba[i*4+2] = v; rgba[i*4+3] = v;
    }
    at->tex = gfx::UploadRGBA(&rgba[0], AT, AT, true);
    if (!at->tex) { delete at; return NULL; }
    gfx::BindTexture(at->tex); gfx::TexFilter(true); gfx::TexWrap(false);
    // (la de un tamano de pantalla va 1:1: escala entera de su alto y pluma en pixel entero, ver PxSnap en UIOverlay)
    if (FPX < 64) at->pixelPerfect = true;
    w3dLogf("Fuente2D: horneada %s a %d px (%d glifos, hoja de %d)", path.c_str(), FPX, (int)at->glyphs.size(), AT);
    return at;
}

// ---- fuente BITMAP: un .png + su .json de glifos ------------------------------------------
// Formato completo en formato/fuente-bitmap.md; el generador de referencia es
// tools/build_fuente_bitmap.py (cualquier extractor de un proyecto escribe lo mismo).
// JSON esperado: { "textura": "x.png", "alto_linea": N, "mayusculas_si_falta": false, "pixel_perfect": true,
//                  "glifos": { "A": {"x":..,"y":..,"w":..,"h":..,
//                                    "xoff":..,"yoff":..,"avance":..}, ... } }
// xoff / yoff / avance son OPCIONALES: el default apoya el glifo en la BASE del renglon
// (yoff = alto_linea - h) sin bearing, que es el caso de una fuente sin descendentes.
// Una tipografia con 'g'/'p'/'y'/coma declara su yoff y su alto real.
// Pixel-perfect: NEAREST + solo multiplos enteros.
static double JNum(JVal* o, const char* k, double def) {
    if (!o || o->type != 4) return def;
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    return (it != o->obj.end() && it->second->type == 1) ? it->second->num : def;
}
// booleano tolerante: acepta true/false (tipo 3) y tambien 0/1 numerico
static bool JBool(JVal* o, const char* k, bool def) {
    if (!o || o->type != 4) return def;
    std::map<std::string, JVal*>::iterator it = o->obj.find(k);
    if (it == o->obj.end() || !it->second) return def;
    if (it->second->type == 3) return it->second->b;
    if (it->second->type == 1) return it->second->num != 0.0;
    return def;
}
static w3dui::W3dTextAtlas* LoadBitmapJson(const std::string& path) {
    // acepta la ruta del .png O la del .json: el hermano es mismo nombre, otra extension
    size_t pt = path.find_last_of('.');
    std::string noExt = (pt == std::string::npos) ? path : path.substr(0, pt);
    std::string ext = (pt == std::string::npos) ? std::string() : path.substr(pt);
    for (size_t i = 0; i < ext.size(); i++)
        if (ext[i] >= 'A' && ext[i] <= 'Z') ext[i] = (char)(ext[i] + 32);
    const bool gavePng   = (ext == ".png");
    std::string jsonPath = noExt + ".json";
    std::vector<unsigned char> js;
    if (!w3dFileSystem::ReadFileBytes(jsonPath, js) || js.empty()) {
        // v4: el contenedor clasifica los .json bajo "extra/" (el png queda en
        // "textures/") -> el hermano por nombre no resuelve; se busca ahi.
        size_t b = jsonPath.find_last_of("/\\");
        std::string alt = "extra/" + (b == std::string::npos ? jsonPath : jsonPath.substr(b + 1));
        if (!w3dFileSystem::ReadFileBytes(alt, js) || js.empty()) {
            w3dLogfE("Fuente2D: falta el json de glifos %s", jsonPath.c_str());
            return NULL;
        }
        jsonPath = alt;
    }
    JParser pair((const char*)&js[0], js.size());
    JVal* root = pair.Value();
    if (!root || root->type != 4) { delete root; return NULL; }
    int lineHeight = (int)JNum(root, "line_height", 0.0);
    if (lineHeight < 1) lineHeight = 1;
    // el PNG: si el caller dio la ruta del png, ESA manda (contrato de fontBitmap);
    // si dio el .json, se usa su campo "textura" (relativo a la carpeta del json).
    std::string pngPath = noExt + ".png";
    std::map<std::string, JVal*>::iterator itT = root->obj.find("texture");
    if (!gavePng && itT != root->obj.end() && itT->second->type == 2 && !itT->second->str.empty()) {
        size_t b = jsonPath.find_last_of("/\\");
        std::string dir = (b == std::string::npos) ? "" : jsonPath.substr(0, b + 1);
        const std::string& t = itT->second->str;
        pngPath = (t[0] == '/' || (t.size() > 1 && t[1] == ':')) ? t : dir + t;
    }
    // ATLAS UNICO: el json remapeado por build_atlas declara "atlas_unico": true
    // y su "textura" es el atlas del juego (ya subido por el 3D). En este caso
    // el campo "textura" del json GANA siempre (los glifos estan en coordenadas
    // ABSOLUTAS del atlas: contra el png chico de fontBitmap saldrian fuera
    // de rango y el texto desaparecia). Se usa el id compartido (Texture2DGet
    // puentea al cache 3D): ni decode, ni premultiplicar, ni copia en VRAM --
    // y la mezcla pasa a alpha recto (el atlas no va premultiplicado).
    bool atlasUnique = JBool(root, "single_atlas", false);
    unsigned sharedTex = 0; int w = 0, h = 0;
    if (atlasUnique) {
        if (itT != root->obj.end() && itT->second->type == 2 && !itT->second->str.empty()) {
            size_t b2 = jsonPath.find_last_of("/\\");
            std::string dir2 = (b2 == std::string::npos) ? "" : jsonPath.substr(0, b2 + 1);
            const std::string& t2 = itT->second->str;
            pngPath = (t2[0] == '/' || (t2.size() > 1 && t2[1] == ':')) ? t2 : dir2 + t2;
            // colapsar "dir/../" ("textures/ui/../atlas.png" -> "textures/atlas.png"):
            // el VFS del contenedor v4 no resuelve ".." y el puente compara por sufijo
            for (size_t i = 0; i < pngPath.size(); i++)
                if (pngPath[i] == '\\') pngPath[i] = '/';
            size_t pp;
            while ((pp = pngPath.find("/../")) != std::string::npos) {
                size_t s2 = pngPath.rfind('/', pp ? pp - 1 : 0);
                if (s2 == std::string::npos) break;
                pngPath.erase(s2, pp + 3 - s2);
            }
        }
        extern unsigned Texture2DGet(const std::string&, int*, int*);
        sharedTex = Texture2DGet(pngPath, &w, &h);
        if (!sharedTex || w < 1 || h < 1) {
            w3dLogfW("Fuente2D: %s declara atlas_unico pero el atlas no se pudo "
                     "obtener: el texto va a salir sin glifos", pngPath.c_str());
            delete root; return NULL;
        }
    }
    unsigned char* rgba = NULL;
    if (!atlasUnique &&
        (!gfx::DecodeImage(pngPath.c_str(), &rgba, &w, &h) || !rgba || w < 1 || h < 1)) {
        w3dLogfE("Fuente2D: no pude leer %s", pngPath.c_str());
        delete root; return NULL;
    }
    // GUARDIA "RGB sin alpha": DecodeImage fuerza 4 canales, asi que un PNG que no
    // trae canal alpha llega con a=255 en TODOS los pixels -> el fondo del glifo
    // saldria OPACO (caja del color del fondo) y nadie diria por que. Se avisa.
    if (rgba) {
        bool allOpaque = true;
        for (int i = 0; i < w * h && allOpaque; i++) allOpaque = (rgba[i*4+3] == 255);
        if (allOpaque)
            w3dLogfW("Fuente2D: %s no trae canal alpha (o esta todo opaco): el fondo de los glifos va a salir OPACO", pngPath.c_str());
    }
    if (rgba) for (int i = 0; i < w * h; i++) {   // premultiplicar (el atlas mezcla premultiplicado)
        unsigned char a = rgba[i*4+3];
        rgba[i*4+0] = (unsigned char)(rgba[i*4+0] * a / 255);
        rgba[i*4+1] = (unsigned char)(rgba[i*4+1] * a / 255);
        rgba[i*4+2] = (unsigned char)(rgba[i*4+2] * a / 255);
    }
    // el atlas SIEMPRE se sube POT (ver UploadAtlasPOT): las UV se calculan contra
    // el tamano ACOLCHADO (tw/th), no contra el del PNG. Con el atlas unico el
    // PNG ya ES POT (1024): tw/th quedan iguales y las UV absolutas del json valen.
    int tw = NextPot(w), th = NextPot(h);
    w3dui::W3dTextAtlas* at = new w3dui::W3dTextAtlas();
    at->atlasW = tw; at->atlasH = th;
    at->fontPx = lineHeight; at->ascent = lineHeight; at->lineH = lineHeight;
    // pixel-art: multiplos enteros + pen entero (como la default). "pixel_perfect": false en el json la deja
    // escalar FRACCIONARIA como el resto de la UI (una pantalla de arcade 496x384 encajada en cualquier ventana:
    // con el snap entero el texto sale mas chico que las imagenes que lo rodean)
    at->pixelPerfect = JBool(root, "pixel_perfect", true);
    std::map<std::string, JVal*>::iterator itG = root->obj.find("glyphs");
    if (itG != root->obj.end() && itG->second->type == 4) {
        for (std::map<std::string, JVal*>::iterator g = itG->second->obj.begin();
             g != itG->second->obj.end(); ++g) {
            if (g->first.empty() || !g->second || g->second->type != 4) continue;
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
            //     `lineHeight - gh` apoya en la base, que solo sirve si NINGUN glifo
            //     tiene descendente; con 'g', 'p', 'y' o una coma da negativo y los
            //     levanta por encima del renglon.
            //   * xoff  = bearing izquierdo (el hueco antes del trazo).
            //   * avance = cuanto corre el lapiz; default gw + 1 px de separacion.
            gl.xoff    = (short)JNum(g->second, "xoff",   0.0);
            gl.yoff    = (short)JNum(g->second, "yoff",   (double)(lineHeight - gh));
            gl.advance = (short)JNum(g->second, "advance", (double)(gw + 1));
            at->glyphs[cp] = gl;
        }
    }
    // "mayusculas_si_falta": OPT-IN del propio archivo. Una fuente que a proposito
    // no trae minusculas (muchas tipografias de juego lo son) puede pedir que cada
    // minuscula ausente caiga en su mayuscula. Sin esta clave, un codepoint sin
    // glifo cae en el fallback normal del atlas ('?') y se avisa una vez: escribir
    // texto en MAYUSCULAS sin que nadie lo haya pedido es una sorpresa cara.
    if (JBool(root, "uppercase_if_missing", false)) {
        for (unsigned cp = 'a'; cp <= 'z'; cp++)
            if (at->glyphs.find(cp) == at->glyphs.end() &&
                at->glyphs.find(cp - 32) != at->glyphs.end())
                at->glyphs[cp] = at->glyphs[cp - 32];
    } else {
        int missing = 0; unsigned first = 0;
        for (unsigned cp = 'a'; cp <= 'z'; cp++)
            if (at->glyphs.find(cp) == at->glyphs.end()) { if (!missing) first = cp; missing++; }
        if (missing)
            w3dLogfW("Fuente2D: %s no trae %d minuscula(s) (la primera es '%c') y no declara "
                     "\"uppercase_if_missing\": esos codepoints van a caer en el glifo de reemplazo",
                     jsonPath.c_str(), missing, (char)first);
    }
    delete root;
    if (at->glyphs.empty()) { if (rgba) gfx::FreeImage(rgba); delete at; return NULL; }
    if (atlasUnique) {
        at->tex = sharedTex;               // el MISMO id del atlas del juego
        gBorrowedFonts.insert(at);         // el id no es de la fuente: no se borra al liberar
        at->mix = gfx::MixAlpha;           // RGBA recto (no premultiplicado)
    } else {
        at->tex = UploadAtlasPOT(rgba, w, h, false, &tw, &th);   // NEAREST: pixel-perfect al escalar
    }
    if (rgba) gfx::FreeImage(rgba);
    if (!at->tex) { delete at; return NULL; }
    gfx::BindTexture(at->tex); gfx::TexFilter(false); gfx::TexWrap(false);
    w3dLogf("Fuente2D: bitmap %s (%d glifos, linea %d px)",
            pngPath.c_str(), (int)at->glyphs.size(), lineHeight);
    return at;
}

// true si la ruta pinta a fuente BITMAP (png/json) y no a ttf
// (sin copiar la extension: se pregunta por cada texto en cada cuadro)
static bool EndsWithNoCase(const std::string& r, size_t p, const char* ext) {
    for (const char* c = r.c_str() + p; ; c++, ext++) {
        const char a = (*c >= 'A' && *c <= 'Z') ? (char)(*c + 32) : *c;
        if (a != *ext) return false;
        if (!a) return true;
    }
}
static bool IsBitmapPath(const std::string& r) {
    size_t p = r.find_last_of('.');
    if (p == std::string::npos) return false;
    return EndsWithNoCase(r, p, ".png") || EndsWithNoCase(r, p, ".json");
}

w3dui::W3dTextAtlas* Font2DGet(const std::string& path) {
    std::map<std::string, w3dui::W3dTextAtlas*>::iterator it = gFonts.find(path);
    if (it != gFonts.end()) return it->second;
    w3dui::W3dTextAtlas* at = path.empty() ? LoadDefault()
                            : (IsBitmapPath(path) ? LoadBitmapJson(path) : BakeTTF(path));
    gFonts[path] = at;   // se cachea aunque sea NULL (no reintentar por frame)
    return at;
}

// una TTF horneada al tamano de PANTALLA con que se va a dibujar (redondeado; de 6 a 63 px: mas grande, la de 64 escalada)
// -> nitida en cualquier definicion. Las demas (la de Whisk3D, las bitmap) no cambian con el tamano
w3dui::W3dTextAtlas* Font2DGetPx(const std::string& path, float px) {
    if (path.empty() || IsBitmapPath(path)) return Font2DGet(path);
    int n = (int)(px + 0.5f);
    if (n >= 64) return Font2DGet(path);
    if (n < 6) n = 6;
    FontSizes& s = gFontsPx[path];
    if (!s.tried[n]) { s.tried[n] = true; s.at[n] = BakeTTF(path, n); }   // (NULL tambien: no reintentar por cuadro)
    return s.at[n] ? s.at[n] : Font2DGet(path);
}

// cierre del proyecto (ver Font2D.h): el arbol ya se destruyo, nadie guarda un atlas
// (UIOverlay lo pide por ruta en cada dibujo) y la proxima vez se vuelve a cargar solo
static void FreeAtlas(w3dui::W3dTextAtlas* at) {
    if (!at) return;
    if (at->tex && gBorrowedFonts.find(at) == gBorrowedFonts.end()) gfx::DeleteTexture(at->tex);
    delete at;
}
void Font2DFreeAll() {
    for (std::map<std::string, w3dui::W3dTextAtlas*>::iterator it = gFonts.begin(); it != gFonts.end(); ++it)
        FreeAtlas(it->second);
    for (std::map<std::string, FontSizes>::iterator it = gFontsPx.begin(); it != gFontsPx.end(); ++it)
        for (int n = 0; n < 64; n++) FreeAtlas(it->second.at[n]);
    gFonts.clear();
    gFontsPx.clear();
    gBorrowedFonts.clear();
}

std::string Font2DName(const std::string& path) {
    if (path.empty()) return "Whisk3D";
    size_t b = path.find_last_of("/\\");
    std::string n = (b == std::string::npos) ? path : path.substr(b + 1);
    size_t p = n.find_last_of('.');
    if (p != std::string::npos) n = n.substr(0, p);
    return n;
}

// ---- vista previa para el file browser -----------------------------------------------------
bool Font2DThumb(const std::string& ttfPath, int maxPx,
                   unsigned char** outRGBA, int* outW, int* outH) {
    std::vector<unsigned char> ttf;
    if (!w3dFileSystem::ReadFileBytes(ttfPath, ttf) || ttf.empty()) return false;
    stbtt_fontinfo fi;
    if (!stbtt_InitFont(&fi, &ttf[0], stbtt_GetFontOffsetForIndex(&ttf[0], 0))) return false;

    const char* SAMPLE = "AaBb";
    const int W = maxPx * 2, H = maxPx;             // miniatura apaisada 2:1
    const float px = H * 0.62f;
    const float sc = stbtt_ScaleForPixelHeight(&fi, px);
    int asc, desc, gap; stbtt_GetFontVMetrics(&fi, &asc, &desc, &gap);

    std::vector<unsigned char> canvas((size_t)W * H, 0);
    float x = 2.0f;
    const float base = (H + asc * sc) * 0.5f;       // baseline centrado vertical
    for (const char* s = SAMPLE; *s; s++) {
        int gw, gh, gx, gy;
        unsigned char* bm = stbtt_GetCodepointBitmap(&fi, sc, sc, *s, &gw, &gh, &gx, &gy);
        if (bm) {
            int ox = (int)x + gx, oy = (int)base + gy;
            for (int j = 0; j < gh; j++) for (int i = 0; i < gw; i++) {
                int dx = ox + i, dy = oy + j;
                if (dx < 0 || dy < 0 || dx >= W || dy >= H) continue;
                unsigned char v = bm[j * gw + i];
                if (v > canvas[dy * W + dx]) canvas[dy * W + dx] = v;
            }
            stbtt_FreeBitmap(bm, NULL);
        }
        int adv, lsb; stbtt_GetCodepointHMetrics(&fi, *s, &adv, &lsb);
        x += adv * sc;
        if (x > W - 2) break;
    }

    unsigned char* rgba = new unsigned char[(size_t)W * H * 4];
    for (int i = 0; i < W * H; i++) {
        unsigned char v = canvas[i];
        rgba[i*4+0] = 235; rgba[i*4+1] = 235; rgba[i*4+2] = 235; rgba[i*4+3] = v;   // letra clara
    }
    *outRGBA = rgba; *outW = W; *outH = H;
    return true;
}
