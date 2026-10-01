// ============================================================================
//  Miniaturas.cpp — las miniaturas de la biblioteca (ver el .h): un rasterizador
//  por software chico (triangulos planos con z-buffer y luz de Lambert), la esfera
//  de un material y la reduccion de una imagen. Cache por clave de biblioteca con
//  una FIRMA (si el recurso cambio, se rehace).
// ============================================================================
#include "io/Miniaturas.h"
#include "io/RecursosProyecto.h"
#include "io/CambiosProyecto.h"         // la version de lo editado (una escena se rehace si se toco)
#include "W3dRaices.h"
#include "objects/MallaRecurso.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Camera.h"
#include "io/W3dMalla.h"               // W3dMallaInfo
#include "io/W3dMallaBin.h"
#include "importers/import_w3d.h"       // la malla de trabajo fuera de la escena (un recurso sin cargar)
#include "importers/import_obj.h"       // TexturaPendienteDe: la textura de un material que espera en la cola
#include "math/Matrix4.h"
#include "gfx/w3dTexture.h"
#include "render/OpcionesRender.h"      // g_redraw
#include "w3dlog.h"
#include <map>
#include <cmath>
#include <cstring>
#include <algorithm>   // std::min / std::max

namespace {
struct Mini {
    unsigned int tex;     // la textura GL (0 = el tipo no tiene)
    bool propia;          // la subio este modulo (se borra al olvidar); una textura cargada no
    std::string firma;
    int w, h;
    Mini() : tex(0), propia(false), w(0), h(0) {}
};
}
static std::map<std::string, Mini> gCache;
static int gPresupuesto = 2;      // miniaturas que se pueden GENERAR en este cuadro
static int gPendientes = 0;

// LA TEXTURA de un material reducida (la que se pega en su esfera): se decodifica UNA vez por ruta, chica
// (unos pocos KB). Vacia = no se pudo (o el N95: no decodifica miniaturas, la esfera va lisa).
namespace {
struct TexChica { std::vector<unsigned char> px; int w, h; TexChica() : w(0), h(0) {} };
}
static std::map<std::string, TexChica> gTexChicas;

void W3dMiniaturasCuadro() { gPresupuesto = 2; gPendientes = 0; }
int  W3dMiniaturasEnCache() { return (int)gCache.size(); }
int  W3dMiniaturasPendientes() { return gPendientes; }
void W3dMiniaturasOlvidar() {
    for (std::map<std::string, Mini>::iterator it = gCache.begin(); it != gCache.end(); ++it)
        if (it->second.propia && it->second.tex) w3dEngine::DeleteTexture(it->second.tex);
    gCache.clear();
    gTexChicas.clear();
}

// ============================================================================
//  EL RASTERIZADOR
// ============================================================================
namespace {
struct Lienzo {
    int n;
    std::vector<unsigned char> px;   // RGBA
    std::vector<float> z;            // profundidad (mas chica = mas cerca)
    explicit Lienzo(int lado) : n(lado), px((size_t)lado * lado * 4, 0), z((size_t)lado * lado, 1e30f) {}
};
// un punto ya proyectado: x, y en pixeles del lienzo, z = profundidad
struct P2 { float x, y, z; };
}
static float Borde(const P2& a, const P2& b, float x, float y) { return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x); }
static void Triangulo(Lienzo& L, const P2& a, const P2& b, const P2& c, const unsigned char* rgb) {
    const float area = Borde(a, b, c.x, c.y);
    if (fabsf(area) < 1e-6f) return;
    int x0 = (int)floorf(std::min(a.x, std::min(b.x, c.x))), x1 = (int)ceilf(std::max(a.x, std::max(b.x, c.x)));
    int y0 = (int)floorf(std::min(a.y, std::min(b.y, c.y))), y1 = (int)ceilf(std::max(a.y, std::max(b.y, c.y)));
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(x1, L.n - 1); y1 = std::min(y1, L.n - 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float w0 = Borde(b, c, px, py), w1 = Borde(c, a, px, py), w2 = Borde(a, b, px, py);
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
struct Vista {
    Vector3 pos, der, arr, fwd;
    bool orto;
    float escala;   // orto: mundo -> media pantalla; perspectiva: tan(fov/2)
};
}
static bool Proyectar(const Vista& v, const Lienzo& L, const Vector3& p, P2& out) {
    const Vector3 r = p - v.pos;
    const float ez = r.Dot(v.fwd);
    if (!v.orto && ez < 0.01f) return false;
    const float d = v.orto ? 1.0f : ez;
    const float nx = r.Dot(v.der) / (v.escala * d), ny = r.Dot(v.arr) / (v.escala * d);
    out.x = (nx * 0.5f + 0.5f) * (float)L.n;
    out.y = (1.0f - (ny * 0.5f + 0.5f)) * (float)L.n;
    out.z = ez;
    return true;
}
// un color con luz de Lambert (luz desde arriba-adelante-izquierda) + ambiente
static void Sombrear(const Vector3& n, const float* difuso, unsigned char* rgb) {
    static const Vector3 kLuz = Vector3(-0.4f, 0.7f, 0.6f).Normalized();
    float l = n.Dot(kLuz);
    if (l < 0) l = -l * 0.25f;   // (la cara de atras un poco iluminada: se ve la forma)
    const float k = 0.28f + 0.72f * l;
    for (int c = 0; c < 3; c++) {
        float v = difuso[c] * k;
        if (v > 1.0f) v = 1.0f;
        rgb[c] = (unsigned char)(v * 255.0f);
    }
}
static const float kGris[4] = { 0.75f, 0.75f, 0.78f, 1.0f };
static const float* ColorMat(const Material* m) { return m ? m->diffuse : kGris; }

// una malla (sus arrays) en el lienzo, con la matriz de mundo 'W' y el material de cada rango
static void Malla(Lienzo& L, const Vista& v, const Matrix4& W, const float* vert, int nVert, const MeshIndex* caras, int nIdx,
                  const std::vector<int>& ini, const std::vector<int>& cant, const std::vector<const Material*>& mats) {
    if (!vert || !caras || nIdx < 3) return;
    for (size_t g = 0; g < ini.size(); g++) {
        const float* col = ColorMat(mats[g]);
        const int a0 = ini[g], a1 = ini[g] + cant[g];
        for (int i = a0; i + 2 < a1 + 0 && i + 2 < nIdx; i += 3) {
            const int ia = (int)caras[i], ib = (int)caras[i + 1], ic = (int)caras[i + 2];
            if (ia >= nVert || ib >= nVert || ic >= nVert) continue;
            const Vector3 pa = W * Vector3(vert[ia * 3], vert[ia * 3 + 1], vert[ia * 3 + 2]);
            const Vector3 pb = W * Vector3(vert[ib * 3], vert[ib * 3 + 1], vert[ib * 3 + 2]);
            const Vector3 pc = W * Vector3(vert[ic * 3], vert[ic * 3 + 1], vert[ic * 3 + 2]);
            P2 a, b, c;
            if (!Proyectar(v, L, pa, a) || !Proyectar(v, L, pb, b) || !Proyectar(v, L, pc, c)) continue;
            Vector3 n = Vector3::Cross(pb - pa, pc - pa).Normalized();
            unsigned char rgb[3];
            Sombrear(n, col, rgb);
            Triangulo(L, a, b, c, rgb);
        }
    }
}

// la vista "de frente" (un poco de arriba y de costado, para que se lea el volumen) que encuadra la caja
static Vista VistaQueEncuadra(const Vector3& mn, const Vector3& mx) {
    Vista v;
    const Vector3 centro = (mn + mx) * 0.5f;
    float radio = (mx - mn).Length() * 0.5f;
    if (radio < 1e-4f) radio = 1.0f;
    const float yaw = 0.45f, pitch = 0.35f;   // radianes
    v.fwd = Vector3(-sinf(yaw) * cosf(pitch), -sinf(pitch), -cosf(yaw) * cosf(pitch)).Normalized();
    v.der = Vector3::Cross(v.fwd, Vector3(0, 1, 0)).Normalized();
    v.arr = Vector3::Cross(v.der, v.fwd).Normalized();
    v.pos = centro - v.fwd * (radio * 3.0f);
    v.orto = true;
    v.escala = radio * 1.1f;
    return v;
}
static void Caja(const Matrix4& W, const float* vert, int n, Vector3& mn, Vector3& mx, bool& hay) {
    for (int i = 0; i < n; i++) {
        const Vector3 p = W * Vector3(vert[i * 3], vert[i * 3 + 1], vert[i * 3 + 2]);
        if (!hay) { mn = mx = p; hay = true; continue; }
        mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
    }
}

// ---- los objetos de un arbol (una escena, un prefab) ----
static void JuntarMallas(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h || !h->visible) continue;
        if (h->getType() == ObjectType::mesh) out.push_back((Mesh*)h);
        JuntarMallas(h, out);
    }
}
static Camera* PrimeraCamara(Object* o) {
    if (!o) return NULL;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::camera) return (Camera*)h;
        Camera* c = PrimeraCamara(h);
        if (c) return c;
    }
    return NULL;
}
static void DibujarMalla(Lienzo& L, const Vista& v, Mesh* m) {
    Matrix4 W; m->GetWorldMatrix(W);
    std::vector<int> ini, cant; std::vector<const Material*> mats;
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        ini.push_back(m->materialsGroup[g].startDrawn); cant.push_back(m->materialsGroup[g].indicesDrawnCount);
        mats.push_back(m->materialsGroup[g].material);
    }
    if (ini.empty()) { ini.push_back(0); cant.push_back(m->facesSize); mats.push_back(NULL); }
    Malla(L, v, W, m->vertex, m->vertexSize, m->faces, m->facesSize, ini, cant, mats);
}
// la camara por la que mira la miniatura de una escena o un juego: SU camara ACTIVA (la de la raiz activa es
// CameraActive; la de otra, la de su contexto) o, si no eligio ninguna, la primera que tenga. Un prefab no
// tiene: se encuadra
static Camera* CamaraDeRaiz(int tipo, int idx) {
    if (tipo != W3D_VISTA_ESCENAS || idx < 0) return NULL;
    Camera* c = (Camera*)W3dRaizCamaraActiva(idx);
    return c ? c : PrimeraCamara(W3dRaices()[(size_t)idx].raiz);
}
// un ARBOL entero: desde su camara (una escena, un juego) o encuadrado (un prefab, o una escena sin camara)
static bool Arbol(Lienzo& L, Object* raiz, Camera* cam) {
    std::vector<Mesh*> ms;
    JuntarMallas(raiz, ms);
    Vista v;
    if (cam) {
        Matrix4 C; cam->GetWorldMatrix(C);
        v.pos = C * Vector3(0, 0, 0);
        v.fwd = (C * Vector3(0, 0, -1) - v.pos).Normalized();
        v.arr = (C * Vector3(0, 1, 0) - v.pos).Normalized();
        v.der = (C * Vector3(1, 0, 0) - v.pos).Normalized();
        v.orto = cam->orthographic;
        v.escala = v.orto ? 5.0f : tanf(cam->fov * 0.5f * 3.14159265f / 180.0f);
    } else {
        if (ms.empty()) return false;
        Vector3 mn, mx; bool hay = false;
        for (size_t i = 0; i < ms.size(); i++) { Matrix4 W; ms[i]->GetWorldMatrix(W); Caja(W, ms[i]->vertex, ms[i]->vertex ? ms[i]->vertexSize : 0, mn, mx, hay); }
        if (!hay) return false;
        v = VistaQueEncuadra(mn, mx);
    }
    for (size_t i = 0; i < ms.size(); i++) DibujarMalla(L, v, ms[i]);
    return true;
}

// ============================================================================
//  CADA TIPO
// ============================================================================
static bool PixelesMalla(const std::string& id, Lienzo& L) {
    MallaRecurso* r = W3dMallaRecursoPorNombre(id);
    if (!r) return false;
    Mesh* t = NULL;
    const float* vert = r->vertex; int nVert = r->vertexSize; const MeshIndex* caras = r->faces; int nIdx = r->facesSize;
    std::vector<int> ini, cant; std::vector<const Material*> mats;
    if (r->Cargada() && vert && caras) {
        for (size_t k = 0; k < r->partes.size(); k++) {
            ini.push_back(r->partes[k].inicio); cant.push_back(r->partes[k].cantidad);
            mats.push_back(k < r->materiales.size() ? r->materiales[k] : NULL);
        }
    } else {
        // un huerfano sin cargar: se lee de sus bytes a una malla de trabajo (fuera de la escena)
        std::string b;
        if (!W3dMallaRecursoLeerBytes(r, b) || b.empty()) return false;
        t = W3dMallaTemporalNueva();
        W3dMallaInfo info;
        W3dMallaBinOpciones op;
        if (!W3dMallaBinLeer((const unsigned char*)b.data(), b.size(), t, &info, op)) { W3dMallaTemporalBorrar(t); return false; }
        vert = t->vertex; nVert = t->vertexSize; caras = t->faces; nIdx = t->facesSize;
        for (size_t k = 0; k < t->materialsGroup.size(); k++) {
            ini.push_back(t->materialsGroup[k].startDrawn); cant.push_back(t->materialsGroup[k].indicesDrawnCount);
            mats.push_back(k < r->materiales.size() ? r->materiales[k] : NULL);
        }
    }
    if (ini.empty()) { ini.push_back(0); cant.push_back(nIdx); mats.push_back(NULL); }
    Matrix4 I; I.Identity();   // (la malla en su espacio)
    Vector3 mn, mx; bool hay = false;
    Caja(I, vert, nVert, mn, mx, hay);
    bool ok = false;
    if (hay) { Malla(L, VistaQueEncuadra(mn, mx), I, vert, nVert, caras, nIdx, ini, cant, mats); ok = true; }
    if (t) W3dMallaTemporalBorrar(t);
    return ok;
}
// la ruta de la textura BASE de un material (la cargada o la que espera en la cola); "" = sin textura
static std::string RutaTexturaDe(const Material* m) {
    if (!m || !m->textureOn) return std::string();
    return m->texture ? m->texture->path : TexturaPendienteDe(m);
}
static const TexChica* TexturaChica(const std::string& ruta) {
#ifdef W3D_SYMBIAN
    (void)ruta;
    return NULL;
#else
    if (ruta.empty()) return NULL;
    std::map<std::string, TexChica>::iterator it = gTexChicas.find(ruta);
    if (it == gTexChicas.end()) {
        TexChica& t = gTexChicas[ruta];
        unsigned char* p = NULL; int w = 0, h = 0;
        if (w3dEngine::DecodeThumbnail(ruta.c_str(), 32, &p, &w, &h) && p) {
            t.px.assign(p, p + (size_t)w * h * 4);
            t.w = w; t.h = h;
            w3dEngine::FreeImage(p);
        }
        it = gTexChicas.find(ruta);
    }
    return it->second.px.empty() ? NULL : &it->second;
#endif
}
// una ESFERA con el material (su color difuso -por su TEXTURA, si tiene-, un brillo especular y su emision)
static bool PixelesMaterial(const std::string& id, Lienzo& L) {
    Material* m = W3dMaterialDeId(id);
    if (!m) return false;
    // la textura se pega en la esfera con un mapeo esferico (u = longitud, v = latitud): dos materiales blancos
    // con texturas distintas (el caso comun) ya no dan la misma esfera lisa
    const TexChica* tex = TexturaChica(RutaTexturaDe(m));
    static const Vector3 kLuz = Vector3(-0.4f, 0.6f, 0.7f).Normalized();
    const Vector3 ojo(0, 0, 1);
    const Vector3 medio = (kLuz + ojo).Normalized();
    const float rad = (float)L.n * 0.46f, c = (float)L.n * 0.5f;
    for (int y = 0; y < L.n; y++)
        for (int x = 0; x < L.n; x++) {
            const float dx = ((float)x + 0.5f - c) / rad, dy = (c - (float)y - 0.5f) / rad;
            const float r2 = dx * dx + dy * dy;
            if (r2 > 1.0f) continue;
            const Vector3 n(dx, dy, sqrtf(1.0f - r2));
            float l = n.Dot(kLuz); if (l < 0) l = 0;
            float s = n.Dot(medio); s = (s > 0) ? powf(s, m->shininess > 1.0f ? m->shininess : 1.0f) : 0.0f;
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
static bool PixelesRaiz(int tipo, const std::string& id, Lienzo& L) {
    const int i = W3dRaizBuscar(tipo == W3D_VISTA_PREFABS ? W3D_RAIZ_PREFAB : W3D_RAIZ_ESCENA, id);
    if (i < 0) return false;
    Object* raiz = W3dRaices()[(size_t)i].raiz;
    if (!raiz) return false;   // (sin cargar: no se carga una escena entera por una miniatura)
    return Arbol(L, raiz, CamaraDeRaiz(tipo, i));
}

// la FIRMA de lo que se dibujo (si cambia, la miniatura se rehace)
static std::string Firma(int tipo, const std::string& id) {
    char b[64];
    switch (tipo) {
        case W3D_VISTA_MALLAS: {
            MallaRecurso* r = W3dMallaRecursoPorNombre(id);
            if (!r) return std::string();
            snprintf(b, sizeof(b), "%u", r->version);
            std::string f = b;
            for (size_t k = 0; k < r->materiales.size(); k++)
                if (r->materiales[k]) { snprintf(b, sizeof(b), ";%.3f,%.3f,%.3f", r->materiales[k]->diffuse[0], r->materiales[k]->diffuse[1], r->materiales[k]->diffuse[2]); f += b; }
            return f;
        }
        case W3D_VISTA_MATERIALES: {
            Material* m = W3dMaterialDeId(id);
            if (!m) return std::string();
            snprintf(b, sizeof(b), "%.3f,%.3f,%.3f,%.2f,%.3f;", m->diffuse[0], m->diffuse[1], m->diffuse[2], m->shininess, m->emission[0] + m->emission[1] + m->emission[2]);
            return std::string(b) + RutaTexturaDe(m);   // (otra textura = otra esfera)
        }
        case W3D_VISTA_ESCENAS: case W3D_VISTA_PREFABS: {
            // cualquier edicion (pasa por el undo), la camara por la que mira (elegir otra camara activa no
            // apila un paso de undo) y su arbol: una raiz perezosa que se cargo despues ya tiene que dibujar
            const int i = W3dRaizBuscar(tipo == W3D_VISTA_PREFABS ? W3D_RAIZ_PREFAB : W3D_RAIZ_ESCENA, id);
            const void* raiz = (i >= 0) ? (const void*)W3dRaices()[(size_t)i].raiz : NULL;
            snprintf(b, sizeof(b), "%u/%u/%p/%p", W3dCambiosVersion(), W3dCambiosEdiciones(), (void*)CamaraDeRaiz(tipo, i), raiz);
            return b;
        }
        case W3D_VISTA_TEXTURAS: {
            Texture* t = W3dTexturaCargadaDe(id);
            snprintf(b, sizeof(b), "%u", t ? t->iID : 0u);
            return b;
        }
    }
    return std::string("-");
}

bool W3dMiniaturaPixeles(int tipo, const std::string& id, std::vector<unsigned char>& rgba, int& w, int& h) {
    Lienzo L(W3D_MINIATURA_LADO);
    bool ok = false;
    switch (tipo) {
        case W3D_VISTA_MALLAS:     ok = PixelesMalla(id, L); break;
        case W3D_VISTA_MATERIALES: ok = PixelesMaterial(id, L); break;
        case W3D_VISTA_ESCENAS:
        case W3D_VISTA_PREFABS:    ok = PixelesRaiz(tipo, id, L); break;
        case W3D_VISTA_TEXTURAS: {
#ifndef W3D_SYMBIAN
            unsigned char* p = NULL; int tw = 0, th = 0;
            if (w3dEngine::DecodeThumbnail(id.c_str(), W3D_MINIATURA_LADO, &p, &tw, &th) && p) {
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
    w = h = W3D_MINIATURA_LADO;
    return true;
}

unsigned int W3dMiniatura(int tipo, const std::string& id, int* w, int* h) {
    if (tipo != W3D_VISTA_MALLAS && tipo != W3D_VISTA_MATERIALES && tipo != W3D_VISTA_TEXTURAS &&
        tipo != W3D_VISTA_ESCENAS && tipo != W3D_VISTA_PREFABS) return 0;   // (el icono del tipo)
    const std::string clave = W3dBibClave(tipo, id);
    const std::string firma = Firma(tipo, id);
    std::map<std::string, Mini>::iterator it = gCache.find(clave);
    if (it != gCache.end() && it->second.firma == firma) {
        if (w) *w = it->second.w;
        if (h) *h = it->second.h;
        return it->second.tex;
    }
    // una TEXTURA cargada es su propia miniatura (la GPU ya la tiene: se dibuja reducida)
    if (tipo == W3D_VISTA_TEXTURAS) {
        Texture* t = W3dTexturaCargadaDe(id);
        if (t && t->iID) {
            Mini& m = gCache[clave];
            if (m.propia && m.tex) w3dEngine::DeleteTexture(m.tex);
            m.tex = t->iID; m.propia = false; m.firma = firma; m.w = t->ancho; m.h = t->alto;
            if (w) *w = m.w;
            if (h) *h = m.h;
            return m.tex;
        }
    }
    if (gPresupuesto <= 0) { gPendientes++; g_redraw = true; return it != gCache.end() ? it->second.tex : 0; }
    gPresupuesto--;
    std::vector<unsigned char> px; int pw = 0, ph = 0;
    Mini& m = gCache[clave];
    if (m.propia && m.tex) w3dEngine::DeleteTexture(m.tex);
    m.tex = 0; m.propia = false; m.firma = firma; m.w = m.h = 0;
    if (W3dMiniaturaPixeles(tipo, id, px, pw, ph) && !px.empty()) {
        m.tex = w3dEngine::UploadRGBA(&px[0], pw, ph, true, false);
        m.propia = (m.tex != 0);
        m.w = pw; m.h = ph;
    }
    if (w) *w = m.w;
    if (h) *h = m.h;
    return m.tex;
}
