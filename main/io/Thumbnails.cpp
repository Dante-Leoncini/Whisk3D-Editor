// ============================================================================
//  Thumbnails.cpp — las miniaturas de la biblioteca (ver el .h): un rasterizador
//  por software chico (triangulos planos con z-buffer y luz de Lambert), la esfera
//  de un material y la reduccion de una imagen. Cache por clave de biblioteca con
//  una FIRMA (si el recurso cambio, se rehace).
// ============================================================================
#include "io/Thumbnails.h"
#include "W3dLang.h"   // T(): el aviso de que no hay memoria para las vistas previas
#include "io/ProjectResources.h"
#include "io/ProjectChanges.h"         // la version de lo editado (una escena se rehace si se toco)
#include "io/ProjectMeshes.h"          // W3dMeshesPublishPendingOf: la edicion diferida antes de mirar el recurso
#include "W3dRoots.h"
#include "objects/MeshResource.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Camera.h"
#include "io/W3dMesh.h"               // W3dMeshInfo
#include "io/W3dMeshBin.h"
#include "importers/import_w3d.h"       // la malla de trabajo fuera de la escena (un recurso sin cargar)
#include "importers/import_obj.h"       // PendingTextureOf: la textura de un material que espera en la cola
#include "math/Matrix4.h"
#include "graphics/w3dTexture.h"
#include "render/RenderOptions.h"      // g_redraw
#include "w3dlog.h"
#include <map>
#include <cmath>
#include <cstring>
#include <algorithm>   // std::min / std::max

namespace {
struct Mini {
    unsigned int tex;     // la textura GL (0 = el tipo no tiene)
    bool own;             // la subio este modulo (se borra al olvidar); una textura cargada no
    std::string signature;
    int w, h;
    Mini() : tex(0), own(false), w(0), h(0) {}
};
}
static std::map<std::string, Mini> gCache;
static int gBudget = 2;      // miniaturas que se pueden GENERAR en este cuadro
static int gPending = 0;

// LA TEXTURA de un material reducida (la que se pega en su esfera): se decodifica UNA vez por ruta, chica
// (unos pocos KB). Vacia = no se pudo (o el N95: no decodifica miniaturas, la esfera va lisa).
namespace {
struct SmallTex { std::vector<unsigned char> px; int w, h; SmallTex() : w(0), h(0) {} };
}
static std::map<std::string, SmallTex> gSmallTex;

// LA MEMORIA: con poca RAM libre las vistas previas son lo primero que se va. Antes de generar una se mira la libre; si
// no alcanza se sueltan las que hay, POR PRIORIDAD (lo que menos sirve primero: sonidos, texturas, mallas 3D, materiales,
// prefabs, escenas) y, si sigue sin alcanzar, no se genera: la biblioteca avisa "No hay memoria para cargar las vistas
// previas" y muestra los iconos
static long kFreeMiniMinKB = 12 * 1024;   // (lo que se deja libre para el resto del editor; el harness lo cambia)
void W3dThumbnailsMinKB(long kb) { kFreeMiniMinKB = kb; }
static bool gNoMemory = false;
extern long W3dFreeMemoryKB();                 // (OutlinerResources.cpp: -1 = la plataforma no lo dice)
extern void Notify(const std::string& msg, bool error);
static void ReleaseType(int type) {
    const std::string pre = W3dBibKey(type, std::string());
    for (std::map<std::string, Mini>::iterator it = gCache.begin(); it != gCache.end(); ) {
        if (it->first.compare(0, pre.size(), pre) == 0) {
            if (it->second.own && it->second.tex) w3dEngine::DeleteTexture(it->second.tex);
            gCache.erase(it++);
        } else ++it;
    }
    if (type == W3D_VIEW_TEXTURES) gSmallTex.clear();   // (las texturas chicas de las esferas de los materiales)
}
static bool HasMemoryForThumbnails() {
    long free = W3dFreeMemoryKB();
    if (free < 0 || free >= kFreeMiniMinKB) {
        if (gNoMemory) { gNoMemory = false; g_redraw = true; }
        return true;
    }
    static const int order[] = { W3D_VIEW_SOUNDS, W3D_VIEW_TEXTURES, W3D_VIEW_MESHES, W3D_VIEW_MATERIALS,
                                 W3D_VIEW_PREFABS, W3D_VIEW_SCENES };
    for (size_t k = 0; k < sizeof(order) / sizeof(order[0]) && free < kFreeMiniMinKB; k++) {
        ReleaseType(order[k]);
        free = W3dFreeMemoryKB();
    }
    if (free >= kFreeMiniMinKB) return true;
    if (!gNoMemory) Notify(T("There is no memory to load the previews"), true);
    gNoMemory = true;
    g_redraw = true;
    return false;
}
bool W3dThumbnailsNoMemory() { return gNoMemory; }
void W3dThumbnailsFrame() { gBudget = 2; gPending = 0; }
int  W3dThumbnailsInCache() { return (int)gCache.size(); }
int  W3dPendingThumbnails() { return gPending; }
void W3dThumbnailsForget() {
    for (std::map<std::string, Mini>::iterator it = gCache.begin(); it != gCache.end(); ++it)
        if (it->second.own && it->second.tex) w3dEngine::DeleteTexture(it->second.tex);
    gCache.clear();
    gSmallTex.clear();
}

// ============================================================================
//  EL RASTERIZADOR
// ============================================================================
namespace {
struct Canvas {
    int n;
    std::vector<unsigned char> px;   // RGBA
    std::vector<float> z;            // profundidad (mas chica = mas cerca)
    explicit Canvas(int side) : n(side), px((size_t)side * side * 4, 0), z((size_t)side * side, 1e30f) {}
};
// un punto ya proyectado: x, y en pixeles del lienzo, z = profundidad
struct P2 { float x, y, z; };
}
static float Border(const P2& a, const P2& b, float x, float y) { return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x); }
static void Triangle(Canvas& L, const P2& a, const P2& b, const P2& c, const unsigned char* rgb) {
    const float area = Border(a, b, c.x, c.y);
    if (fabsf(area) < 1e-6f) return;
    int x0 = (int)floorf(std::min(a.x, std::min(b.x, c.x))), x1 = (int)ceilf(std::max(a.x, std::max(b.x, c.x)));
    int y0 = (int)floorf(std::min(a.y, std::min(b.y, c.y))), y1 = (int)ceilf(std::max(a.y, std::max(b.y, c.y)));
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, L.n - 1); y1 = std::min(y1, L.n - 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float w0 = Border(b, c, px, py), w1 = Border(c, a, px, py), w2 = Border(a, b, px, py);
            if (area < 0) { w0 = -w0; w1 = -w1; w2 = -w2; }
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            const float aa = fabsf(area);
            const float zz = (w0 * a.z + w1 * b.z + w2 * c.z) / aa;
            const size_t i = (size_t)y * (size_t)L.n + (size_t)x;
            if (zz >= L.z[i]) continue;
            L.z[i] = zz;
            L.px[i * 4] = rgb[0]; L.px[i * 4 + 1] = rgb[1]; L.px[i * 4 + 2] = rgb[2]; L.px[i * 4 + 3] = 255;
        }
}

// UNA VISTA: posicion, ejes (derecha, arriba, adelante) y la lente. Perspectiva o paralela.
namespace {
struct ThumbView {
    Vector3 pos, right, arr, fwd;
    bool ortho;
    float scale;   // orto: mundo -> media pantalla; perspectiva: tan(fov/2)
};
}
static bool Project(const ThumbView& v, const Canvas& L, const Vector3& p, P2& out) {
    const Vector3 r = p - v.pos;
    const float ez = r.Dot(v.fwd);
    if (!v.ortho && ez < 0.01f) return false;
    const float d = v.ortho ? 1.0f : ez;
    const float nx = r.Dot(v.right) / (v.scale * d), ny = r.Dot(v.arr) / (v.scale * d);
    out.x = (nx * 0.5f + 0.5f) * (float)L.n;
    out.y = (1.0f - (ny * 0.5f + 0.5f)) * (float)L.n;
    out.z = ez;
    return true;
}
// un color con luz de Lambert (luz desde arriba-adelante-izquierda) + ambiente
static void Shade(const Vector3& n, const float* diffuse, unsigned char* rgb) {
    static const Vector3 kLight = Vector3(-0.4f, 0.7f, 0.6f).Normalized();
    float l = n.Dot(kLight);
    if (l < 0) l = -l * 0.25f;   // (la cara de atras un poco iluminada: se ve la forma)
    const float k = 0.28f + 0.72f * l;
    for (int c = 0; c < 3; c++) {
        float v = diffuse[c] * k;
        if (v > 1.0f) v = 1.0f;
        rgb[c] = (unsigned char)(v * 255.0f);
    }
}
static const float kGray[4] = { 0.75f, 0.75f, 0.78f, 1.0f };
static const float* ColorMat(const Material* m) { return m ? m->diffuse : kGray; }

// una malla (sus arrays) en el lienzo, con la matriz de mundo 'W' y el material de cada rango
static void DrawMesh(Canvas& L, const ThumbView& v, const Matrix4& W, const float* vert, int nVert, const MeshIndex* faces, int nIdx,
                  const std::vector<int>& start, const std::vector<int>& count, const std::vector<const Material*>& mats) {
    if (!vert || !faces || nIdx < 3) return;
    for (size_t g = 0; g < start.size(); g++) {
        const float* col = ColorMat(mats[g]);
        const int a0 = start[g], a1 = start[g] + count[g];
        for (int i = a0; i + 2 < a1 + 0 && i + 2 < nIdx; i += 3) {
            const int ia = (int)faces[i], ib = (int)faces[i + 1], ic = (int)faces[i + 2];
            if (ia >= nVert || ib >= nVert || ic >= nVert) continue;
            const Vector3 pa = W * Vector3(vert[ia * 3], vert[ia * 3 + 1], vert[ia * 3 + 2]);
            const Vector3 pb = W * Vector3(vert[ib * 3], vert[ib * 3 + 1], vert[ib * 3 + 2]);
            const Vector3 pc = W * Vector3(vert[ic * 3], vert[ic * 3 + 1], vert[ic * 3 + 2]);
            P2 a, b, c;
            if (!Project(v, L, pa, a) || !Project(v, L, pb, b) || !Project(v, L, pc, c)) continue;
            Vector3 n = Vector3::Cross(pb - pa, pc - pa).Normalized();
            unsigned char rgb[3];
            Shade(n, col, rgb);
            Triangle(L, a, b, c, rgb);
        }
    }
}

// la vista "de frente" (un poco de arriba y de costado, para que se lea el volumen) que encuadra la caja
static ThumbView ViewThatFrames(const Vector3& mn, const Vector3& mx) {
    ThumbView v;
    const Vector3 center = (mn + mx) * 0.5f;
    float radius = (mx - mn).Length() * 0.5f;
    if (radius < 1e-4f) radius = 1.0f;
    const float yaw = 0.45f, pitch = 0.35f;   // radianes
    v.fwd   = Vector3(-sinf(yaw) * cosf(pitch), -sinf(pitch), -cosf(yaw) * cosf(pitch)).Normalized();
    v.right = Vector3::Cross(v.fwd, Vector3(0, 1, 0)).Normalized();
    v.arr   = Vector3::Cross(v.right, v.fwd).Normalized();
    v.pos   = center - v.fwd * (radius * 3.0f);
    v.ortho = true;
    v.scale = radius * 1.1f;
    return v;
}
static void Box(const Matrix4& W, const float* vert, int n, Vector3& mn, Vector3& mx, bool& has) {
    for (int i = 0; i < n; i++) {
        const Vector3 p = W * Vector3(vert[i * 3], vert[i * 3 + 1], vert[i * 3 + 2]);
        if (!has) { mn = mx = p; has = true; continue; }
        mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
    }
}

// ---- los objetos de un arbol (una escena, un prefab) ----
static void CollectMeshes(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h || !h->visible) continue;
        if (h->getType() == ObjectType::mesh) out.push_back((Mesh*)h);
        CollectMeshes(h, out);
    }
}
static Camera* FirstCamera(Object* o) {
    if (!o) return NULL;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::camera) return (Camera*)h;
        Camera* c = FirstCamera(h);
        if (c) return c;
    }
    return NULL;
}
static void DrawMesh(Canvas& L, const ThumbView& v, Mesh* m) {
    Matrix4 W; m->GetWorldMatrix(W);
    std::vector<int> start, count; std::vector<const Material*> mats;
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        start.push_back(m->materialsGroup[g].startDrawn); count.push_back(m->materialsGroup[g].indicesDrawnCount);
        mats.push_back(m->materialsGroup[g].material);
    }
    if (start.empty()) { start.push_back(0); count.push_back(m->facesSize); mats.push_back(NULL); }
    DrawMesh(L, v, W, m->vertex, m->vertexSize, m->faces, m->facesSize, start, count, mats);
}
// la camara por la que mira la miniatura de una escena o un juego: SU camara ACTIVA (la de la raiz activa es
// CameraActive; la de otra, la de su contexto) o, si no eligio ninguna, la primera que tenga. Un prefab no
// tiene: se encuadra
static Camera* RootCamera(int type, int idx) {
    if (type != W3D_VIEW_SCENES || idx < 0) return NULL;
    Camera* c = (Camera*)W3dRootActiveCamera(idx);
    return c ? c : FirstCamera(W3dRoots()[(size_t)idx].root);
}
// un ARBOL entero: desde su camara (una escena, un juego) o encuadrado (un prefab, o una escena sin camara)
static bool Tree(Canvas& L, Object* root, Camera* cam) {
    std::vector<Mesh*> ms;
    CollectMeshes(root, ms);
    ThumbView v;
    if (cam) {
        Matrix4 C; cam->GetWorldMatrix(C);
        v.pos   = C * Vector3(0, 0, 0);
        v.fwd   = (C * Vector3(0, 0, -1) - v.pos).Normalized();
        v.arr   = (C * Vector3(0, 1, 0) - v.pos).Normalized();
        v.right = (C * Vector3(1, 0, 0) - v.pos).Normalized();
        v.ortho = cam->orthographic;
        v.scale = v.ortho ? 5.0f : tanf(cam->fov * 0.5f * 3.14159265f / 180.0f);
    } else {
        if (ms.empty()) return false;
        Vector3 mn, mx; bool has = false;
        for (size_t i = 0; i < ms.size(); i++) { Matrix4 W; ms[i]->GetWorldMatrix(W); Box(W, ms[i]->vertex, ms[i]->vertex ? ms[i]->vertexSize : 0, mn, mx, has); }
        if (!has) return false;
        v = ViewThatFrames(mn, mx);
    }
    for (size_t i = 0; i < ms.size(); i++) DrawMesh(L, v, ms[i]);
    return true;
}

// ============================================================================
//  CADA TIPO
// ============================================================================
static bool PixelsMesh(const std::string& id, Canvas& L) {
    MeshResource* r = W3dMeshResourceByName(id);
    if (!r) return false;
    W3dMeshesPublishPendingOf(NULL, r);   // la que salio de Edit sin publicar (unica usuaria): mirar lo editado
    // una malla que solto su RAM (onlyGPU, la dibujan solo rutinas) se relee de sus bytes para mirarla; la
    // proxima corrida de rutinas la vuelve a soltar (prueba_mallas_solo_gpu.w3s)
    if (r->onlyGPU) W3dMeshResourceEnsureRAM(r);
    Mesh* t = NULL;
    const float* vert = r->vertex; int nVert = r->vertexSize; const MeshIndex* faces = r->faces; int nIdx = r->facesSize;
    std::vector<int> start, count; std::vector<const Material*> mats;
    if (r->Loaded() && vert && faces) {
        for (size_t k = 0; k < r->parts.size(); k++) {
            start.push_back(r->parts[k].start); count.push_back(r->parts[k].count);
            mats.push_back(k < r->materials.size() ? r->materials[k] : NULL);
        }
    } else {
        // un huerfano sin cargar: se lee de sus bytes a una malla de trabajo (fuera de la escena)
        std::string b;
        if (!W3dMeshResourceReadBytes(r, b) || b.empty()) return false;
        t = W3dTempNewMesh();
        W3dMeshInfo info;
        W3dMeshBinOptions op;
        if (!W3dMeshBinRead((const unsigned char*)b.data(), b.size(), t, &info, op)) { W3dTempMeshDelete(t); return false; }
        vert = t->vertex; nVert = t->vertexSize; faces = t->faces; nIdx = t->facesSize;
        for (size_t k = 0; k < t->materialsGroup.size(); k++) {
            start.push_back(t->materialsGroup[k].startDrawn); count.push_back(t->materialsGroup[k].indicesDrawnCount);
            mats.push_back(k < r->materials.size() ? r->materials[k] : NULL);
        }
    }
    if (start.empty()) { start.push_back(0); count.push_back(nIdx); mats.push_back(NULL); }
    Matrix4 I; I.Identity();   // (la malla en su espacio)
    Vector3 mn, mx; bool has = false;
    Box(I, vert, nVert, mn, mx, has);
    bool ok = false;
    if (has) { DrawMesh(L, ViewThatFrames(mn, mx), I, vert, nVert, faces, nIdx, start, count, mats); ok = true; }
    if (t) W3dTempMeshDelete(t);
    return ok;
}
// la ruta de la textura BASE de un material (la cargada o la que espera en la cola); "" = sin textura
static std::string TexturePathOf(const Material* m) {
    if (!m || !m->textureOn) return std::string();
    return m->texture ? m->texture->path : PendingTextureOf(m);
}
static const SmallTex* SmallTexture(const std::string& path) {
#ifdef W3D_SYMBIAN
    (void)path;
    return NULL;
#else
    if (path.empty()) return NULL;
    std::map<std::string, SmallTex>::iterator it = gSmallTex.find(path);
    if (it == gSmallTex.end()) {
        SmallTex& t = gSmallTex[path];
        unsigned char* p = NULL; int w = 0, h = 0;
        if (w3dEngine::DecodeThumbnail(path.c_str(), 32, &p, &w, &h) && p) {
            t.px.assign(p, p + (size_t)w * h * 4);
            t.w = w; t.h = h;
            w3dEngine::FreeImage(p);
        }
        it = gSmallTex.find(path);
    }
    return it->second.px.empty() ? NULL : &it->second;
#endif
}
// una ESFERA con el material (su color difuso -por su TEXTURA, si tiene-, un brillo especular y su emision)
static bool PixelsMaterial(const std::string& id, Canvas& L) {
    Material* m = W3dMaterialById(id);
    if (!m) return false;
    // la textura se pega en la esfera con un mapeo esferico (u = longitud, v = latitud): dos materiales blancos
    // con texturas distintas (el caso comun) ya no dan la misma esfera lisa
    const SmallTex* tex = SmallTexture(TexturePathOf(m));
    static const Vector3 kLight = Vector3(-0.4f, 0.6f, 0.7f).Normalized();
    const Vector3 eye(0, 0, 1);
    const Vector3 middle = (kLight + eye).Normalized();
    const float rad = (float)L.n * 0.46f, c = (float)L.n * 0.5f;
    for (int y = 0; y < L.n; y++)
        for (int x = 0; x < L.n; x++) {
            const float dx = ((float)x + 0.5f - c) / rad, dy = (c - (float)y - 0.5f) / rad;
            const float r2 = dx * dx + dy * dy;
            if (r2 > 1.0f) continue;
            const Vector3 n(dx, dy, sqrtf(1.0f - r2));
            float l = n.Dot(kLight); if (l < 0) l = 0;
            float s = n.Dot(middle); s = (s > 0) ? powf(s, m->shininess > 1.0f ? m->shininess : 1.0f) : 0.0f;
            float base[3] = { m->diffuse[0], m->diffuse[1], m->diffuse[2] };
            if (tex) {
                // (las de double: el N95 no tiene todas las de float de C99)
                const float u = 0.5f + (float)atan2((double)n.x, (double)n.z) / (2.0f * 3.14159265f);
                const float v = 0.5f - (float)asin((double)(n.y > 1.0f ? 1.0f : n.y)) / 3.14159265f;
                int tx = (int)(u * (float)tex->w), ty = (int)(v * (float)tex->h);
                tx = std::max(0, std::min(tex->w - 1, tx)); ty = std::max(0, std::min(tex->h - 1, ty));
                const unsigned char* t = &tex->px[((size_t)ty * (size_t)tex->w + (size_t)tx) * 4];
                for (int k = 0; k < 3; k++) base[k] *= (float)t[k] / 255.0f;
            }
            const size_t i = ((size_t)y * (size_t)L.n + (size_t)x) * 4;
            for (int k = 0; k < 3; k++) {
                float v = base[k] * (0.25f + 0.75f * l) + m->specular[k] * s * 0.6f + m->emission[k];
                if (v > 1.0f) v = 1.0f;
                L.px[i + k] = (unsigned char)(v * 255.0f);
            }
            L.px[i + 3] = 255;
        }
    return true;
}
static bool PixelsRoot(int type, const std::string& id, Canvas& L) {
    const int i = W3dRootFind(type == W3D_VIEW_PREFABS ? W3D_ROOT_PREFAB : W3D_ROOT_SCENE, id);
    if (i < 0) return false;
    Object* root = W3dRoots()[(size_t)i].root;
    if (!root) return false;   // (sin cargar: no se carga una escena entera por una miniatura)
    return Tree(L, root, RootCamera(type, i));
}

// la FIRMA de lo que se dibujo (si cambia, la miniatura se rehace)
static std::string Signature(int type, const std::string& id) {
    char b[64];
    switch (type) {
        case W3D_VIEW_MESHES: {
            MeshResource* r = W3dMeshResourceByName(id);
            if (!r) return std::string();
            snprintf(b, sizeof(b), "%u", r->version);
            std::string f = b;
            for (size_t k = 0; k < r->materials.size(); k++)
                if (r->materials[k]) { snprintf(b, sizeof(b), ";%.3f,%.3f,%.3f", r->materials[k]->diffuse[0], r->materials[k]->diffuse[1], r->materials[k]->diffuse[2]); f += b; }
            return f;
        }
        case W3D_VIEW_MATERIALS: {
            Material* m = W3dMaterialById(id);
            if (!m) return std::string();
            snprintf(b, sizeof(b), "%.3f,%.3f,%.3f,%.2f,%.3f;", m->diffuse[0], m->diffuse[1], m->diffuse[2], m->shininess, m->emission[0] + m->emission[1] + m->emission[2]);
            return std::string(b) + TexturePathOf(m);   // (otra textura = otra esfera)
        }
        case W3D_VIEW_SCENES: case W3D_VIEW_PREFABS: {
            // cualquier edicion (pasa por el undo), la camara por la que mira (elegir otra camara activa no
            // apila un paso de undo) y su arbol: una raiz perezosa que se cargo despues ya tiene que dibujar
            const int i = W3dRootFind(type == W3D_VIEW_PREFABS ? W3D_ROOT_PREFAB : W3D_ROOT_SCENE, id);
            const void* root = (i >= 0) ? (const void*)W3dRoots()[(size_t)i].root : NULL;
            snprintf(b, sizeof(b), "%u/%u/%p/%p", W3dChangesVersion(), W3dChangesEdits(), (void*)RootCamera(type, i), root);
            return b;
        }
        case W3D_VIEW_TEXTURES: {
            Texture* t = W3dLoadedTextureOf(id);
            snprintf(b, sizeof(b), "%u", t ? t->iID : 0u);
            return b;
        }
    }
    return std::string("-");
}

bool W3dThumbnailPixels(int type, const std::string& id, std::vector<unsigned char>& rgba, int& w, int& h) {
    Canvas L(W3D_THUMBNAIL_SIDE);
    bool ok = false;
    switch (type) {
        case W3D_VIEW_MESHES:     ok = PixelsMesh(id, L); break;
        case W3D_VIEW_MATERIALS: ok  = PixelsMaterial(id, L); break;
        case W3D_VIEW_SCENES:
        case W3D_VIEW_PREFABS:    ok = PixelsRoot(type, id, L); break;
        case W3D_VIEW_TEXTURES: {
#ifndef W3D_SYMBIAN
            unsigned char* p = NULL; int tw = 0, th = 0;
            if (w3dEngine::DecodeThumbnail(id.c_str(), W3D_THUMBNAIL_SIDE, &p, &tw, &th) && p) {
                rgba.assign(p, p + (size_t)tw * th * 4);
                w3dEngine::FreeImage(p);
                w = tw; h = th;
                return true;
            }
#endif
            return false;
        }
        default: return false;
    }
    if (!ok) return false;
    rgba.swap(L.px);
    w = h = W3D_THUMBNAIL_SIDE;
    return true;
}

unsigned int W3dThumbnail(int type, const std::string& id, int* w, int* h) {
    if (type != W3D_VIEW_MESHES && type != W3D_VIEW_MATERIALS && type != W3D_VIEW_TEXTURES &&
        type != W3D_VIEW_SCENES && type != W3D_VIEW_PREFABS) return 0;   // (el icono del tipo)
    const std::string key       = W3dBibKey(type, id);
    const std::string signature = Signature(type, id);
    std::map<std::string, Mini>::iterator it = gCache.find(key);
    if (it != gCache.end() && it->second.signature == signature) {
        if (w) *w = it->second.w;
        if (h) *h = it->second.h;
        return it->second.tex;
    }
    // una TEXTURA cargada es su propia miniatura (la GPU ya la tiene: se dibuja reducida)
    if (type == W3D_VIEW_TEXTURES) {
        Texture* t = W3dLoadedTextureOf(id);
        if (t && t->iID) {
            Mini& m = gCache[key];
            if (m.own && m.tex) w3dEngine::DeleteTexture(m.tex);
            m.tex = t->iID; m.own = false; m.signature = signature; m.w = t->width; m.h = t->height;
            if (w) *w = m.w;
            if (h) *h = m.h;
            return m.tex;
        }
    }
    if (gBudget <= 0) { gPending++; g_redraw = true; return it != gCache.end() ? it->second.tex : 0; }
    if (!HasMemoryForThumbnails()) return 0;   // (sin memoria: el icono del tipo)
    gBudget--;
    std::vector<unsigned char> px; int pw = 0, ph = 0;
    Mini& m = gCache[key];
    if (m.own && m.tex) w3dEngine::DeleteTexture(m.tex);
    m.tex = 0; m.own = false; m.signature = signature; m.w = m.h = 0;
    if (W3dThumbnailPixels(type, id, px, pw, ph) && !px.empty()) {
        m.tex = w3dEngine::UploadRGBA(&px[0], pw, ph, true, false);
        m.own = (m.tex != 0);
        m.w = pw; m.h = ph;
    }
    if (w) *w = m.w;
    if (h) *h = m.h;
    return m.tex;
}
