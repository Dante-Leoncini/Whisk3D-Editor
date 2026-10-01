// ============================================================================
//  TexturaEditada.cpp - texturas editables en memoria (ver TexturaEditada.h).
// ============================================================================
#include "io/TexturaEditada.h"
#include "io/W3dContenedor.h"      // contenedor montado / entradas / externas
#include "objects/Textures.h"
#include "edit/W3dFalloff.h"
#include "w3dTexture.h"            // UploadRGBA / UpdateRGBA / DecodeImage / EncodePNG(RGBA)
#include "w3dGraphics.h"           // TexTieneMips
#include "w3dFilesystem.h"
#include "ui/ViewPorts/UVEditor.h" // el UV editor la sigue mostrando aunque cambie de ruta (override por ruta)
#include <stdio.h>
#include <string.h>
#include <math.h>

static std::vector<TexturaEditable*> g_tex;

static bool TexturaViva(const Texture* t) {
    for (size_t i = 0; i < Textures.size(); i++) if (Textures[i] == t) return true;
    return false;
}
TexturaEditable* TexEditBuscar(Texture* t) {
    if (!t) return NULL;
    for (size_t i = 0; i < g_tex.size(); i++) {
        if (g_tex[i]->tex != t) continue;
        if (!TexturaViva(t)) { delete g_tex[i]; g_tex.erase(g_tex.begin() + (long)i); return NULL; }   // la textura se libero
        return g_tex[i];
    }
    return NULL;
}
TexturaEditable* TexEditObtener(Texture* t) {
    TexturaEditable* te = TexEditBuscar(t);
    if (te) return te;
    if (!t || !t->iID || t->path.empty()) return NULL;
    unsigned char* px = NULL; int w = 0, h = 0;
    if (!w3dEngine::DecodeImage(t->path.c_str(), &px, &w, &h) || !px || w <= 0 || h <= 0) return NULL;
    te = new TexturaEditable();
    te->tex = t; te->w = w; te->h = h;
    // la que se PINTA no se descarga en la sesion (sus pixeles editados viven aca hasta guardar): una referencia mas,
    // aunque el streaming duerma el material que la usaba (Textures.h: las ranuras son las duenas). La suelta el
    // cierre del proyecto, que libera todas
    TexturaRetener(t);
    te->rgba.assign(px, px + (size_t)w * h * 4);
    w3dEngine::FreeImage(px);
    te->alpha = false;
    for (size_t i = 3; i < te->rgba.size(); i += 4) if (te->rgba[i] != 255) { te->alpha = true; break; }
    g_tex.push_back(te);
    return te;
}
static bool PathEnUso(const std::string& p) {
    for (size_t i = 0; i < Textures.size(); i++) if (Textures[i] && Textures[i]->path == p) return true;
    return TexturaBuscar(p) != NULL;
}
static std::string NombreInternoLibre(const std::string& nombre) {
    const std::string base = "texturas/" + W3dSlugEntrada(nombre);
    std::string ruta = base + ".png";
    for (int k = 2; PathEnUso(ruta) && k < 10000; k++) { char b[16]; sprintf(b, "_%d", k); ruta = base + b + ".png"; }
    return ruta;
}
Texture* TexEditCrear(const std::string& nombre, const std::vector<unsigned char>& rgba, int w, int h, bool alpha) {
    if (w <= 0 || h <= 0 || rgba.size() < (size_t)w * h * 4) return NULL;
    const unsigned int id = w3dEngine::UploadRGBA(&rgba[0], w, h, true, true);
    if (!id) return NULL;
    Texture* t = new Texture(NombreInternoLibre(nombre));
    t->iID = id; t->ancho = w; t->alto = h; t->conMipmaps = w3dEngine::TexTieneMips(id);
    TexturaRegistrar(t->path, t);
    TexturaEditable* te = new TexturaEditable();
    te->tex = t; te->w = w; te->h = h; te->alpha = alpha; te->rgba = rgba;
    te->modificada = true; te->enMemoria = true;
    g_tex.push_back(te);
    return t;
}
void TexEditSubir(TexturaEditable* te, int x0, int y0, int x1, int y1) {
    if (!te || !te->tex || !te->tex->iID) return;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > te->w) x1 = te->w; if (y1 > te->h) y1 = te->h;
    if (x1 <= x0 || y1 <= y0) return;
    const int rw = x1 - x0, rh = y1 - y0;
    if (rw == te->w && rh == te->h) { w3dEngine::UpdateRGBA(te->tex->iID, 0, 0, te->w, te->h, &te->rgba[0]); return; }
    std::vector<unsigned char> tmp((size_t)rw * rh * 4);
    for (int y = 0; y < rh; y++) memcpy(&tmp[(size_t)y * rw * 4], &te->rgba[((size_t)(y0 + y) * te->w + x0) * 4], (size_t)rw * 4);
    w3dEngine::UpdateRGBA(te->tex->iID, x0, y0, rw, rh, &tmp[0]);
}
bool TexEditEsExterna(const Texture* t) { return t && !t->path.empty() && !W3dEsNombreDeEntrada(t->path); }

static void Codificar(TexturaEditable* te, std::vector<unsigned char>& out) {
    int len = 0;
    unsigned char* png = te->alpha ? w3dEngine::EncodePNGRGBA(&te->rgba[0], te->w, te->h, false, &len)
                                   : w3dEngine::EncodePNG(&te->rgba[0], te->w, te->h, false, &len);
    out.clear();
    if (png && len > 0) out.assign(png, png + len);
    delete[] png;
}
static bool EscribirDisco(const std::string& ruta, const std::vector<unsigned char>& bytes) {
    FILE* f = fopen(ruta.c_str(), "wb"); if (!f) return false;
    const size_t wr = bytes.empty() ? 0 : fwrite(&bytes[0], 1, bytes.size(), f);
    fclose(f);
    return wr == bytes.size();
}
bool TexEditGuardar(TexturaEditable* te, std::string& msg) {
    if (!te || !te->tex) { msg = "Save Texture: no texture"; return false; }
    std::vector<unsigned char> png; Codificar(te, png);
    if (png.empty()) { msg = "Save Texture: could not encode"; return false; }
    if (TexEditEsExterna(te->tex)) {
        if (!EscribirDisco(te->tex->path, png)) { msg = "Save Texture: could not write"; return false; }
        te->enMemoria = false;
    } else if (W3dContenedorHayMontado()) {
        if (!W3dContenedorEscribirEntrada(te->tex->path, &png[0], png.size())) { msg = "Save Texture: could not write"; return false; }
        te->enMemoria = false; te->pngGuardado.clear();
    } else {
        te->pngGuardado = png; te->enMemoria = true;   // sin proyecto guardado: viaja con el proximo guardado del .w3d
    }
    te->modificada = false;
    msg.clear();
    return true;
}
bool TexEditHacerExterna(TexturaEditable* te, const std::string& rutaDisco, std::string& msg) {
    if (!te || !te->tex || rutaDisco.empty()) { msg = "Save Texture: no texture"; return false; }
    std::vector<unsigned char> png; Codificar(te, png);
    if (png.empty() || !EscribirDisco(rutaDisco, png)) { msg = "Save Texture: could not write"; return false; }
    const std::string viejo = te->tex->path;
    TexturaRenombrar(te->tex, rutaDisco);
    if (UVTexProyectoRuta() == viejo) UVSetTexProyecto(te->tex->path);   // el UV editor la sigue mostrando
    W3dRefExternaMarcar(rutaDisco);
    te->enMemoria = false; te->modificada = false; te->pngGuardado.clear();
    msg.clear();
    return true;
}
bool TexEditHacerInterna(TexturaEditable* te, std::string& msg) {
    if (!te || !te->tex) { msg = "Save Texture: no texture"; return false; }
    std::string nombre = te->tex->path;
    const size_t sl = nombre.find_last_of("/\\"); if (sl != std::string::npos) nombre = nombre.substr(sl + 1);
    const size_t pt = nombre.rfind('.'); if (pt != std::string::npos) nombre = nombre.substr(0, pt);
    const std::string viejo = te->tex->path;
    TexturaRenombrar(te->tex, NombreInternoLibre(nombre));
    if (UVTexProyectoRuta() == viejo) UVSetTexProyecto(te->tex->path);
    te->enMemoria = true;
    return TexEditGuardar(te, msg);
}
bool TexEditSinGuardar(const Texture* t) {
    for (size_t i = 0; i < g_tex.size(); i++)
        if (g_tex[i]->tex == t) return g_tex[i]->modificada || (g_tex[i]->enMemoria && g_tex[i]->pngGuardado.empty());
    return false;
}
std::string TexEditEtiqueta(const Texture* t) {
    if (!t) return std::string();
    std::string lbl = t->path;
    const size_t sl = lbl.find_last_of("/\\"); if (sl != std::string::npos) lbl = lbl.substr(sl + 1);
    if (TexEditSinGuardar(t)) lbl += "*";
    return lbl;
}
bool TexEditBytesParaGuardar(const std::string& path, std::string& png) {
    for (size_t i = 0; i < g_tex.size(); i++) {
        TexturaEditable* te = g_tex[i];
        if (!te->tex || te->tex->path != path || !te->enMemoria || TexEditEsExterna(te->tex)) continue;
        if (!te->pngGuardado.empty()) { png.assign((const char*)&te->pngGuardado[0], te->pngGuardado.size()); return true; }
        std::vector<unsigned char> b; Codificar(te, b);
        if (b.empty()) return false;
        png.assign((const char*)&b[0], b.size());
        te->modificada = false;   // lo que se escribio ES lo que hay: queda guardado
        return true;
    }
    return false;
}
void TexEditProyectoGuardado() {
    for (size_t i = 0; i < g_tex.size(); i++) { g_tex[i]->enMemoria = false; g_tex[i]->pngGuardado.clear(); }
}
void TexEditLimpiarTodo() {
    for (size_t i = 0; i < g_tex.size(); i++) delete g_tex[i];
    g_tex.clear();
}
void TexEditRectUnir(int* rect, int x, int y) {
    if (rect[0] < 0) { rect[0] = x; rect[1] = y; rect[2] = x + 1; rect[3] = y + 1; return; }
    if (x < rect[0]) rect[0] = x; if (y < rect[1]) rect[1] = y;
    if (x + 1 > rect[2]) rect[2] = x + 1; if (y + 1 > rect[3]) rect[3] = y + 1;
}
void TexEditMezclar(TexturaEditable* te, int x, int y, const unsigned char* rgba, float a) {
    if (!te || x < 0 || y < 0 || x >= te->w || y >= te->h) return;
    if (a <= 0.0f) return; if (a > 1.0f) a = 1.0f;
    unsigned char* p = &te->rgba[((size_t)y * te->w + x) * 4];
    for (int c = 0; c < 3; c++) p[c] = (unsigned char)(p[c] + (rgba[c] - p[c]) * a + 0.5f);
    p[3] = (unsigned char)(p[3] + (255 - p[3]) * a * (rgba[3] / 255.0f) + 0.5f);   // pintar tambien opaca
    te->modificada = true;
}
bool TexEditDab(TexturaEditable* te, float cx, float cy, float r, const unsigned char* rgba, float fuerza, const W3dFalloff& fo, int* rect) {
    if (!te || r <= 0.0f || fuerza <= 0.0f) return false;
    int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r), y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > te->w - 1) x1 = te->w - 1; if (y1 > te->h - 1) y1 = te->h - 1;
    bool alguno = false;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        const float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx*dx + dy*dy);
        if (d > r) continue;
        const float a = fuerza * fo.Eval(d / r);
        if (a <= 0.0f) continue;
        TexEditMezclar(te, x, y, rgba, a);
        if (rect) TexEditRectUnir(rect, x, y);
        alguno = true;
    }
    return alguno;
}
