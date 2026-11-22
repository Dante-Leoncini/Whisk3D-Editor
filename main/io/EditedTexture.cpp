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
#include <stdio.h>
#include <string.h>
#include <math.h>

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
    if (!t || !t->iID || t->path.empty()) return NULL;
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
void TexEditMix(TextureEditable* te, int x, int y, const unsigned char* rgba, float a) {
    if (!te || x < 0 || y < 0 || x >= te->w || y >= te->h) return;
    if (a <= 0.0f) return; if (a > 1.0f) a = 1.0f;
    unsigned char* p = &te->rgba[((size_t)y * te->w + x) * 4];
    for (int c = 0; c < 3; c++) p[c] = (unsigned char)(p[c] + (rgba[c] - p[c]) * a + 0.5f);
    p[3] = (unsigned char)(p[3] + (255 - p[3]) * a * (rgba[3] / 255.0f) + 0.5f);   // pintar tambien opaca
    te->modified = true;
}
bool TexEditDab(TextureEditable* te, float cx, float cy, float r, const unsigned char* rgba, float force, const W3dFalloff& fo, int* rect) {
    if (!te || r <= 0.0f || force <= 0.0f) return false;
    int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r), y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > te->w - 1) x1 = te->w - 1; if (y1 > te->h - 1) y1 = te->h - 1;
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
