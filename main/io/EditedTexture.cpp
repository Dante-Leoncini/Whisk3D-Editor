// ============================================================================
//  EditedTexture.cpp - texturas editables en memoria (ver EditedTexture.h).
// ============================================================================
#include "io/W3dWeb.h"   // navegador: guardar = descargar
#include "io/EditedTexture.h"
#include "io/W3dContainer.h"      // contenedor montado / entradas / externas
#include "objects/Textures.h"
#include "edit/W3dFalloff.h"
#include "w3dTexture.h"            // UploadRGBA / UpdateRGBA / DecodeImage / EncodePNG(RGBA)
#include "w3dGraphics.h"           // TexHasMips
#include "w3dFilesystem.h"
#include "ui/ViewPorts/UVEditor.h" // el UV editor la sigue mostrando aunque cambie de ruta (override por ruta)
#include "edit/ModeMemory.h"     // sus pixeles se sueltan al dejar de pintar
#include "base/W3dInteractionState.h" // TexturePaint
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>   // swap_ranges (el deshacer por baldosas)

static std::vector<TextureEditable*> g_tex;

static bool TextureLive(const Texture* t) {
    for (size_t i = 0; i < Textures.size(); i++) if (Textures[i] == t) return true;
    return false;
}
TextureEditable* TexEditFind(Texture* t) {
    if (!t) return NULL;
    for (size_t i = 0; i < g_tex.size(); i++) {
        if (g_tex[i]->tex != t) continue;
        if (!TextureLive(t)) { delete g_tex[i]; g_tex.erase(g_tex.begin() + (long)i); return NULL; }   // la textura se libero
        return g_tex[i];
    }
    return NULL;
}
TextureEditable* TexEditGet(Texture* t) {
    TextureEditable* te = TexEditFind(t);
    if (te) return te;
    // (una textura que ya no esta: la pide un paso del Ctrl+Z de una que se pinto, solto sus pixeles y despues se libero)
    if (!t || !TextureLive(t) || !t->iID || t->path.empty()) return NULL;
    unsigned char* px = NULL; int w = 0, h = 0;
    if (!w3dEngine::DecodeImage(t->path.c_str(), &px, &w, &h) || !px || w <= 0 || h <= 0) return NULL;
    te = new TextureEditable();
    te->tex = t; te->w = w; te->h = h;
    // la que se PINTA no se descarga en la sesion (sus pixeles editados viven aca hasta guardar): una referencia mas,
    // aunque el streaming duerma el material que la usaba (Textures.h: las ranuras son las duenas). La suelta el
    // cierre del proyecto, que libera todas
    TextureRetain(t);
    te->rgba.assign(px, px + (size_t)w * h * 4);
    w3dEngine::FreeImage(px);
    te->alpha = false;
    for (size_t i = 3; i < te->rgba.size(); i += 4) if (te->rgba[i] != 255) { te->alpha = true; break; }
    g_tex.push_back(te);
    return te;
}
TextureEditable* TexEditGetPinned(Texture* t) {
    TextureEditable* te = TexEditGet(t);
    if (te) te->pinnedRT = true;
    return te;
}
static bool PathInUse(const std::string& p) {
    for (size_t i = 0; i < Textures.size(); i++) if (Textures[i] && Textures[i]->path == p) return true;
    return TextureFind(p) != NULL;
}
static std::string InternalFreeName(const std::string& name) {
    const std::string base = "textures/" + W3dSlugEntry(name);
    std::string path = base + ".png";
    for (int k = 2; PathInUse(path) && k < 10000; k++) { char b[16]; sprintf(b, "_%d", k); path = base + b + ".png"; }
    return path;
}
Texture* TexEditCreate(const std::string& name, const std::vector<unsigned char>& rgba, int w, int h, bool alpha) {
    if (w <= 0 || h <= 0 || rgba.size() < (size_t)w * h * 4) return NULL;
    const unsigned int id = w3dEngine::UploadRGBA(&rgba[0], w, h, true, true);
    if (!id) return NULL;
    Texture* t = new Texture(InternalFreeName(name));
    t->iID = id; t->width = w; t->height = h; t->withMipmaps = w3dEngine::TexHasMips(id);
    TextureRegister(t->path, t);
    TextureEditable* te = new TextureEditable();
    te->tex = t; te->w = w; te->h = h; te->alpha = alpha; te->rgba = rgba;
    te->modified = true; te->inMemory = true;
    g_tex.push_back(te);
    return t;
}
void TexEditUpload(TextureEditable* te, int x0, int y0, int x1, int y1, bool stroke) {
    if (!te || !te->tex || !te->tex->iID) return;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > te->w) x1 = te->w; if (y1 > te->h) y1 = te->h;
    if (x1 <= x0 || y1 <= y0) return;
    const int rw = x1 - x0, rh = y1 - y0;
    if (stroke) te->mipsPending = true;
    if (rw == te->w && rh == te->h) { w3dEngine::UpdateRGBA(te->tex->iID, 0, 0, te->w, te->h, &te->rgba[0], !stroke); return; }
    std::vector<unsigned char> tmp((size_t)rw * rh * 4);
    for (int y = 0; y < rh; y++) memcpy(&tmp[(size_t)y * rw * 4], &te->rgba[((size_t)(y0 + y) * te->w + x0) * 4], (size_t)rw * 4);
    w3dEngine::UpdateRGBA(te->tex->iID, x0, y0, rw, rh, &tmp[0], !stroke);
}
void TexEditEndStroke(TextureEditable* te) {
    if (!te || !te->mipsPending || !te->tex || !te->tex->iID || te->rgba.empty()) return;
    te->mipsPending = false;
    w3dEngine::RegenerateMips(te->tex->iID, 0, 0, &te->rgba[0]);
}
bool TexEditIsExternal(const Texture* t) { return t && !t->path.empty() && !W3dIsEntryName(t->path); }

static void Encode(TextureEditable* te, std::vector<unsigned char>& out) {
    int len = 0;
    unsigned char* png = te->alpha ? w3dEngine::EncodePNGRGBA(&te->rgba[0], te->w, te->h, false, &len)
                                   : w3dEngine::EncodePNG(&te->rgba[0], te->w, te->h, false, &len);
    out.clear();
    if (png && len > 0) out.assign(png, png + len);
    delete[] png;
}
static bool WriteDisk(const std::string& path, const std::vector<unsigned char>& bytes) {
    FILE* f = fopen(path.c_str(), "wb"); if (!f) return false;
    const size_t wr = bytes.empty() ? 0 : fwrite(&bytes[0], 1, bytes.size(), f);
    fclose(f);
    if (wr == bytes.size()) W3dDownloadIfWeb(path);   // navegador: la textura guardada afuera baja a la PC
    return wr == bytes.size();
}
bool TexEditSave(TextureEditable* te, std::string& msg) {
    if (!te || !te->tex) { msg = "Save Texture: no texture"; return false; }
    std::vector<unsigned char> png; Encode(te, png);
    if (png.empty()) { msg = "Save Texture: could not encode"; return false; }
    if (TexEditIsExternal(te->tex)) {
        if (!WriteDisk(te->tex->path, png)) { msg = "Save Texture: could not write"; return false; }
        te->inMemory = false;
    } else if (W3dContainerHasMounted()) {
        if (!W3dContainerWriteEntry(te->tex->path, &png[0], png.size())) { msg = "Save Texture: could not write"; return false; }
        te->inMemory = false; te->savedPng.clear();
    } else {
        te->savedPng = png; te->inMemory = true;   // sin proyecto guardado: viaja con el proximo guardado del .w3d
    }
    te->modified = false;
    msg.clear();
    return true;
}
bool TexEditMakeExternal(TextureEditable* te, const std::string& diskPath, std::string& msg) {
    if (!te || !te->tex || diskPath.empty()) { msg = "Save Texture: no texture"; return false; }
    std::vector<unsigned char> png; Encode(te, png);
    if (png.empty() || !WriteDisk(diskPath, png)) { msg = "Save Texture: could not write"; return false; }
    const std::string old = te->tex->path;
    TextureRename(te->tex, diskPath);
    if (UVTexProjectPath() == old) UVSetTexProject(te->tex->path);   // el UV editor la sigue mostrando
    W3dExternalRefMark(diskPath);
    te->inMemory = false; te->modified = false; te->savedPng.clear();
    msg.clear();
    return true;
}
bool TexEditMakeInternal(TextureEditable* te, std::string& msg) {
    if (!te || !te->tex) { msg = "Save Texture: no texture"; return false; }
    std::string name = te->tex->path;
    const size_t sl = name.find_last_of("/\\"); if (sl != std::string::npos) name = name.substr(sl + 1);
    const size_t pt = name.rfind('.'); if (pt != std::string::npos) name = name.substr(0, pt);
    const std::string old = te->tex->path;
    TextureRename(te->tex, InternalFreeName(name));
    if (UVTexProjectPath() == old) UVSetTexProject(te->tex->path);
    te->inMemory = true;
    return TexEditSave(te, msg);
}
// "Guardar como..." ADENTRO del .w3d con otro nombre: la textura pasa a ser textures/<nombre>.png (los materiales que
// la usan la siguen usando: es la misma textura renombrada). Si otra textura del proyecto ya usa ese nombre, no se
// pisa (msg); 'existe'/'forzar' quedan por la firma comun de los "Guardar como".
bool TexEditSaveAsInternal(TextureEditable* te, const std::string& name, bool force, bool* exists, std::string& msg) {
    if (exists) *exists = false;
    if (!te || !te->tex) { msg = "Save Texture: no texture"; return false; }
    std::string base = name;
    const size_t sl = base.find_last_of("/\\"); if (sl != std::string::npos) base = base.substr(sl + 1);
    const size_t pt = base.rfind('.'); if (pt != std::string::npos) base = base.substr(0, pt);
    if (base.empty()) { msg = "Name"; return false; }
    const std::string destination = "textures/" + W3dSlugEntry(base) + ".png";
    (void)force;
    if (destination != te->tex->path && PathInUse(destination)) {   // otra textura VIVA con ese nombre: no se pisa
        msg = "A texture with that name already exists";
        return false;
    }
    const std::string old = te->tex->path;
    if (destination != old) TextureRename(te->tex, destination);
    if (UVTexProjectPath() == old) UVSetTexProject(te->tex->path);
    te->inMemory = true;
    return TexEditSave(te, msg);
}
bool TexEditUnsaved(const Texture* t) {
    for (size_t i = 0; i < g_tex.size(); i++)
        if (g_tex[i]->tex == t) return g_tex[i]->modified || (g_tex[i]->inMemory && g_tex[i]->savedPng.empty());
    return false;
}
bool TexEditPNGCurrent(const Texture* t, std::vector<unsigned char>& png) {
    png.clear();
    for (size_t i = 0; i < g_tex.size(); i++) {
        if (g_tex[i]->tex != t) continue;
        Encode(g_tex[i], png);
        return !png.empty();
    }
    return false;
}
std::string TexEditLabel(const Texture* t) {
    if (!t) return std::string();
    std::string lbl = t->path;
    const size_t sl = lbl.find_last_of("/\\"); if (sl != std::string::npos) lbl = lbl.substr(sl + 1);
    if (TexEditUnsaved(t)) lbl += "*";
    return lbl;
}
bool TexEditBytesToSave(const std::string& path, std::string& png) {
    for (size_t i = 0; i < g_tex.size(); i++) {
        TextureEditable* te = g_tex[i];
        if (!te->tex || te->tex->path != path || !te->inMemory || TexEditIsExternal(te->tex)) continue;
        if (!te->savedPng.empty()) { png.assign((const char*)&te->savedPng[0], te->savedPng.size()); return true; }
        std::vector<unsigned char> b; Encode(te, b);
        if (b.empty()) return false;
        png.assign((const char*)&b[0], b.size());
        te->modified = false;   // lo que se escribio ES lo que hay: queda guardado
        return true;
    }
    return false;
}
void TexEditSavedProject() {
    for (size_t i = 0; i < g_tex.size(); i++) { g_tex[i]->inMemory = false; g_tex[i]->savedPng.clear(); }
}
void TexEditClearAll() {
    for (size_t i = 0; i < g_tex.size(); i++) delete g_tex[i];
    g_tex.clear();
}
void TexEditRectJoin(int* rect, int x, int y) {
    if (rect[0] < 0) { rect[0] = x; rect[1] = y; rect[2] = x + 1; rect[3] = y + 1; return; }
    if (x < rect[0]) rect[0] = x; if (y < rect[1]) rect[1] = y;
    if (x + 1 > rect[2]) rect[2] = x + 1; if (y + 1 > rect[3]) rect[3] = y + 1;
}
// ---- el ANTES de un trazo, por baldosas (ver TexEditTiles) ----
static int TilesAcross(int n) { return (n + kTexEditTile - 1) / kTexEditTile; }
// la baldosa i de la textura: su rectangulo [x0,x1) x [y0,y1)
static void TileRect(const TextureEditable* te, int i, int& x0, int& y0, int& x1, int& y1) {
    const int cols = TilesAcross(te->w);
    x0 = (i % cols) * kTexEditTile; y0 = (i / cols) * kTexEditTile;
    x1 = x0 + kTexEditTile; if (x1 > te->w) x1 = te->w;
    y1 = y0 + kTexEditTile; if (y1 > te->h) y1 = te->h;
}
static void RecordTile(TextureEditable* te, int i) {
    if (te->recDone[(size_t)i]) return;
    te->recDone[(size_t)i] = 1;
    int x0, y0, x1, y1; TileRect(te, i, x0, y0, x1, y1);
    const size_t row = (size_t)(x1 - x0) * 4;
    TexEditTiles& t = *te->rec;
    t.index.push_back(i);
    for (int y = y0; y < y1; y++) {
        const unsigned char* s = &te->rgba[((size_t)y * te->w + x0) * 4];
        t.px.insert(t.px.end(), s, s + row);
    }
}
// el rectangulo [x0,x1] x [y0,y1] (inclusive, ya dentro de la textura) va a cambiar
static void RecordRect(TextureEditable* te, int x0, int y0, int x1, int y1) {
    if (!te->rec) return;
    const int cols = TilesAcross(te->w);
    for (int ty = y0 / kTexEditTile; ty <= y1 / kTexEditTile; ty++)
        for (int tx = x0 / kTexEditTile; tx <= x1 / kTexEditTile; tx++) RecordTile(te, ty * cols + tx);
}
void TexEditRecordStart(TextureEditable* te, TexEditTiles* into, bool whole) {
    if (!te || !into || te->w <= 0 || te->h <= 0 || te->rgba.size() < (size_t)te->w * te->h * 4) return;
    into->w = te->w; into->h = te->h; into->index.clear(); into->px.clear();
    te->rec = into;
    te->recDone.assign((size_t)TilesAcross(te->w) * TilesAcross(te->h), 0);
    if (whole) RecordRect(te, 0, 0, te->w - 1, te->h - 1);
}
void TexEditRecordStop(TextureEditable* te) {
    if (!te) return;
    te->rec = NULL;
    std::vector<unsigned char>().swap(te->recDone);
}
bool TexEditSwapTiles(TextureEditable* te, TexEditTiles& t) {
    if (!te || t.index.empty() || t.w != te->w || t.h != te->h || te->rgba.size() < (size_t)te->w * te->h * 4) return false;
    size_t total = 0;   // (lo guardado tiene que ser justo esas baldosas: nada se intercambia a medias)
    for (size_t k = 0; k < t.index.size(); k++) {
        int x0, y0, x1, y1; TileRect(te, t.index[k], x0, y0, x1, y1);
        total += (size_t)(x1 - x0) * (size_t)(y1 - y0) * 4;
    }
    if (total != t.px.size()) return false;
    size_t off = 0;
    int bx0 = te->w, by0 = te->h, bx1 = 0, by1 = 0;   // lo que cambio, para subirlo de una
    for (size_t k = 0; k < t.index.size(); k++) {
        int x0, y0, x1, y1; TileRect(te, t.index[k], x0, y0, x1, y1);
        const size_t row = (size_t)(x1 - x0) * 4;
        for (int y = y0; y < y1; y++, off += row) {
            unsigned char* d = &te->rgba[((size_t)y * te->w + x0) * 4];
            std::swap_ranges(d, d + row, &t.px[off]);
        }
        if (x0 < bx0) bx0 = x0; if (y0 < by0) by0 = y0; if (x1 > bx1) bx1 = x1; if (y1 > by1) by1 = y1;
    }
    TexEditUpload(te, bx0, by0, bx1, by1);
    te->modified = true;
    return true;
}
TexPixels::~TexPixels() { if (own) w3dEngine::FreeImage(own); }
bool TexEditPixels(Texture* t, TexPixels& out) {
    if (out.own) { w3dEngine::FreeImage(out.own); out.own = NULL; }
    out.px = NULL; out.w = out.h = 0;
    if (!t || !t->iID) return false;
    if (TextureEditable* te = TexEditFind(t)) {
        if (te->w <= 0 || te->h <= 0 || te->rgba.size() < (size_t)te->w * te->h * 4) return false;
        out.px = &te->rgba[0]; out.w = te->w; out.h = te->h;
        return true;
    }
    if (t->path.empty()) return false;
    unsigned char* px = NULL; int w = 0, h = 0;
    if (!w3dEngine::DecodeImage(t->path.c_str(), &px, &w, &h) || !px || w <= 0 || h <= 0) { if (px) w3dEngine::FreeImage(px); return false; }
    out.own = px; out.px = px; out.w = w; out.h = h;
    return true;
}
// (al dejar de pintar texturas: edit/ModeMemory.h)
W3D_MODE_MEMORY(W3D_MODE_BIT(TexturePaint), TexEditReleaseIdle);
void TexEditReleaseIdle() {
    for (size_t i = g_tex.size(); i-- > 0; ) {
        TextureEditable* te = g_tex[i];
        if (te->modified || te->inMemory || !te->savedPng.empty() || te->pins > 0 || te->pinnedRT || te->rec || te->mipsPending) continue;
        Texture* t = te->tex;
        delete te;
        g_tex.erase(g_tex.begin() + (long)i);
        if (TextureLive(t)) TextureRelease(t);   // (la referencia que tomo TexEditGet)
    }
}
void TexEditMix(TextureEditable* te, int x, int y, const unsigned char* rgba, float a) {
    if (!te || x < 0 || y < 0 || x >= te->w || y >= te->h) return;
    if (a <= 0.0f) return; if (a > 1.0f) a = 1.0f;
    if (te->rec) RecordTile(te, (y / kTexEditTile) * TilesAcross(te->w) + x / kTexEditTile);
    unsigned char* p = &te->rgba[((size_t)y * te->w + x) * 4];
    for (int c = 0; c < 3; c++) p[c] = (unsigned char)(p[c] + (rgba[c] - p[c]) * a + 0.5f);
    p[3] = (unsigned char)(p[3] + (255 - p[3]) * a * (rgba[3] / 255.0f) + 0.5f);   // pintar tambien opaca
    te->modified = true;
}
bool TexEditDab(TextureEditable* te, float cx, float cy, float r, const unsigned char* rgba, float force, const W3dFalloff& fo, int* rect) {
    if (!te || r <= 0.0f || force <= 0.0f) return false;
    int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r), y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > te->w - 1) x1 = te->w - 1; if (y1 > te->h - 1) y1 = te->h - 1;
    if (x1 < x0 || y1 < y0) return false;
    RecordRect(te, x0, y0, x1, y1);   // (de una, antes del recorrido: TexEditMix ya no tiene que guardar nada)
    bool any = false;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        const float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx*dx + dy*dy);
        if (d > r) continue;
        const float a = force * fo.Eval(d / r);
        if (a <= 0.0f) continue;
        TexEditMix(te, x, y, rgba, a);
        if (rect) TexEditRectJoin(rect, x, y);
        any = true;
    }
    return any;
}
