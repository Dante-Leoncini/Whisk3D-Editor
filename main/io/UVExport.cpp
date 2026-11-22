// ============================================================================
//  UVExport.cpp - el rasterizador del mapa UV (ver UVExport.h). Escrito de cero para Whisk3D, C++03 (N95).
//    caras : relleno por barrido (scanline) con regla par-impar, asi un n-gono concavo se pinta UNA vez por
//            pixel (un abanico de triangulos se pisaria a si mismo y quedaria mas opaco adentro de una sola cara)
//    mezcla: "over" con alpha no premultiplicado -> dos caras superpuestas dan 1-(1-a)^2, tres 1-(1-a)^3...
//    bordes: despues de todas las caras, opacos, con un grosor que acompana la resolucion (1px hasta 512)
// ============================================================================
#include "io/W3dWeb.h"   // navegador: guardar = descargar
#include "io/UVExport.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "w3dTexture.h"     // EncodePNGRGBA / TextureSize
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const float kFill[3] = { 1.0f, 1.0f, 1.0f };       // color de las caras
static const float kFillAlpha = 0.25f;                    // opacidad de UNA cara
static const unsigned char kBorder[4] = { 0, 0, 0, 255 }; // color de los bordes (opacos)

void UVExportDefaultSize(Texture* tex, int& w, int& h) {
#ifdef W3D_SYMBIAN
    w = h = 256;
#else
    w = h = 512;
#endif
    if (!tex) return;
    int tw = tex->width, th = tex->height;
    if ((tw <= 0 || th <= 0) && tex->iID) w3dEngine::TextureSize(tex->iID, tw, th);
    if (tw > 0 && th > 0) { w = tw; h = th; }
}

static std::string NoFolderNiExt(const std::string& s) {
    std::string n = s;
    const size_t sl = n.find_last_of("/\\"); if (sl != std::string::npos) n = n.substr(sl + 1);
    const size_t pt = n.rfind('.'); if (pt != std::string::npos && pt > 0) n = n.substr(0, pt);
    return n;
}
std::string UVExportDefaultName(Mesh* m, Texture* tex) {
    std::string base;
    if (tex && !tex->path.empty()) base = NoFolderNiExt(tex->path);
    if (base.empty() && m) base = m->name;
    if (base.empty()) base = "uv";
    return base + "_uv.png";
}

// "src over dst" de un color con alfa sobre el pixel (alfa no premultiplicado)
static inline void Mix(unsigned char* p, const float* rgb, float a) {
    const float da = p[3] / 255.0f;
    const float oa = a + da * (1.0f - a);
    if (oa <= 0.0f) return;
    for (int c = 0; c < 3; c++) {
        const float v = (rgb[c] * 255.0f * a + p[c] * da * (1.0f - a)) / oa;
        p[c] = (unsigned char)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v + 0.5f));
    }
    p[3] = (unsigned char)(oa * 255.0f + 0.5f);
}

// relleno par-impar de UN poligono (pts = x0,y0,x1,y1,... en pixeles): cada pixel cuyo centro cae adentro se mezcla una vez
static void FillPolygon(std::vector<unsigned char>& img, int w, int h, const std::vector<float>& pts) {
    const int n = (int)pts.size() / 2;
    if (n < 3) return;
    float ymin = pts[1], ymax = pts[1];
    for (int i = 1; i < n; i++) { if (pts[i*2+1] < ymin) ymin = pts[i*2+1]; if (pts[i*2+1] > ymax) ymax = pts[i*2+1]; }
    int y0 = (int)floorf(ymin), y1 = (int)ceilf(ymax);
    if (y0 < 0) y0 = 0; if (y1 > h) y1 = h;
    std::vector<float> xs;
    for (int y = y0; y < y1; y++) {
        const float yc = y + 0.5f;
        xs.clear();
        for (int i = 0; i < n; i++) {
            const float ax = pts[i*2], ay = pts[i*2+1];
            const int j = (i + 1) % n;
            const float bx = pts[j*2], by = pts[j*2+1];
            if ((ay <= yc && yc < by) || (by <= yc && yc < ay)) xs.push_back(ax + (yc - ay) * (bx - ax) / (by - ay));
        }
        if (xs.size() < 2) continue;
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) {
            // pixeles con centro en [xa, xb)
            int xa = (int)ceilf(xs[k] - 0.5f), xb = (int)ceilf(xs[k + 1] - 0.5f);
            if (xa < 0) xa = 0; if (xb > w) xb = w;
            for (int x = xa; x < xb; x++) Mix(&img[((size_t)y * w + x) * 4], kFill, kFillAlpha);
        }
    }
}

// linea opaca de grosor g (cuadrado de g x g por paso)
static void Line(std::vector<unsigned char>& img, int w, int h, float ax, float ay, float bx, float by, int g) {
    const float dx = bx - ax, dy = by - ay;
    const float length = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
    const int steps = (int)ceilf(length) + 1;
    const int r0 = -(g - 1) / 2;
    for (int s = 0; s <= steps; s++) {
        const float t = (steps > 0) ? (float)s / (float)steps : 0.0f;
        const int cx = (int)floorf(ax + dx * t), cy = (int)floorf(ay + dy * t);
        for (int oy = 0; oy < g; oy++) for (int ox = 0; ox < g; ox++) {
            const int x = cx + r0 + ox, y = cy + r0 + oy;
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            memcpy(&img[((size_t)y * w + x) * 4], kBorder, 4);
        }
    }
}

bool UVExportRaster(Mesh* m, Texture* filter, int w, int h, std::vector<unsigned char>& rgba, int* facesOut) {
    if (facesOut) *facesOut = 0;
    if (!m || !m->uv || m->faces3d.empty() || w <= 0 || h <= 0) return false;
    rgba.assign((size_t)w * h * 4, 0);   // transparente
    // filtro por textura: solo si alguna parte de la malla la usa (una textura del proyecto sin asignar -> todas)
    bool doFilter = false;
    if (filter)
        for (size_t g = 0; g < m->materialsGroup.size() && !doFilter; g++)
            doFilter = (m->materialsGroup[g].material && m->materialsGroup[g].material->texture == filter);
    const int nV = m->vertexSize;
    std::vector<size_t> faces;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const MeshFace& F = m->faces3d[f];
        if (F.idx.size() < 2) continue;
        if (doFilter) {
            Material* mt = (F.mat >= 0 && F.mat < (int)m->materialsGroup.size()) ? m->materialsGroup[(size_t)F.mat].material : NULL;
            if (!mt || mt->texture != filter) continue;
        }
        bool ok = true;
        for (size_t c = 0; c < F.idx.size(); c++) if (F.idx[c] < 0 || F.idx[c] >= nV) { ok = false; break; }
        if (ok) faces.push_back(f);
    }
    if (faces.empty()) return false;
    std::vector<float> pts;
    // 1) las caras (semitransparentes, se acumulan)
    for (size_t k = 0; k < faces.size(); k++) {
        const MeshFace& F = m->faces3d[faces[k]];
        pts.clear();
        for (size_t c = 0; c < F.idx.size(); c++) { const int v = F.idx[c]; pts.push_back(m->uv[v*2] * w); pts.push_back(m->uv[v*2+1] * h); }
        FillPolygon(rgba, w, h, pts);
    }
    // 2) los bordes encima (1px hasta 512, 2px a 1024, 4px a 2048...)
    const int side = (w < h) ? w : h;
    int g = (int)(side / 512.0f + 0.5f); if (g < 1) g = 1;
    for (size_t k = 0; k < faces.size(); k++) {
        const MeshFace& F = m->faces3d[faces[k]];
        const int nc = (int)F.idx.size();
        for (int c = 0; c < nc; c++) {
            const int a = F.idx[(size_t)c], b = F.idx[(size_t)((c + 1) % nc)];
            Line(rgba, w, h, m->uv[a*2] * w, m->uv[a*2+1] * h, m->uv[b*2] * w, m->uv[b*2+1] * h, g);
        }
    }
    if (facesOut) *facesOut = (int)faces.size();
    return true;
}

bool UVExportPNG(Mesh* m, Texture* filter, int w, int h, const std::string& path, std::string& msg) {
    std::vector<unsigned char> rgba;
    if (!UVExportRaster(m, filter, w, h, rgba)) { msg = "No UVs to export"; return false; }
    int len = 0;
    unsigned char* png = w3dEngine::EncodePNGRGBA(&rgba[0], w, h, false, &len);
    if (!png || len <= 0) { delete[] png; msg = "Could not encode the PNG"; return false; }
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { delete[] png; msg = "Could not write the file"; return false; }
    const size_t wr = fwrite(png, 1, (size_t)len, f);
    fclose(f);
    delete[] png;
    if (wr != (size_t)len) { msg = "Could not write the file"; return false; }
    W3dDownloadIfWeb(path);   // navegador: el mapa UV baja a la PC
    msg.clear();
    return true;
}

// que fraccion del area UV cubierta la cubre MAS DE UNA cara (0 = nada se pisa; el cubo por defecto da 1)
float UVOverlappedFraction(Mesh* m) {
    if (!m || !m->uv || m->faces3d.empty()) return 1.0f;
    const int W = 96, H = 96;
    std::vector<unsigned char> count((size_t)W * H, 0);
    std::vector<float> xs;
    const int nV = m->vertexSize;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx; const int n = (int)id.size();
        if (n < 3) continue;
        bool ok = true; for (int c = 0; c < n; c++) if (id[(size_t)c] < 0 || id[(size_t)c] >= nV) ok = false;
        if (!ok) continue;
        float ymin = 1e9f, ymax = -1e9f;
        for (int c = 0; c < n; c++) { const float v = m->uv[id[(size_t)c]*2+1] * H; if (v < ymin) ymin = v; if (v > ymax) ymax = v; }
        int y0 = (int)floorf(ymin), y1 = (int)ceilf(ymax); if (y0 < 0) y0 = 0; if (y1 > H) y1 = H;
        for (int y = y0; y < y1; y++) {
            const float yc = y + 0.5f; xs.clear();
            for (int c = 0; c < n; c++) {
                const int a = id[(size_t)c], b = id[(size_t)((c + 1) % n)];
                const float ax = m->uv[a*2] * W, ay = m->uv[a*2+1] * H, bx = m->uv[b*2] * W, by = m->uv[b*2+1] * H;
                if ((ay <= yc && yc < by) || (by <= yc && yc < ay)) xs.push_back(ax + (yc - ay) * (bx - ax) / (by - ay));
            }
            std::sort(xs.begin(), xs.end());
            for (size_t k = 0; k + 1 < xs.size(); k += 2) {
                int xa = (int)ceilf(xs[k] - 0.5f), xb = (int)ceilf(xs[k + 1] - 0.5f); if (xa < 0) xa = 0; if (xb > W) xb = W;
                for (int x = xa; x < xb; x++) { unsigned char& q = count[(size_t)y * W + x]; if (q < 255) q++; }
            }
        }
    }
    int covered = 0, overwritten = 0;
    for (size_t i = 0; i < count.size(); i++) { if (count[i]) covered++; if (count[i] > 1) overwritten++; }
    return covered ? (float)overwritten / (float)covered : 1.0f;
}
