// ============================================================================
//  SondaLuz.cpp — ver SondaLuz.h. Compartido editor/juego (C++03: Symbian lo compila).
// ============================================================================
#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "objects/SondaLuz.h"
#include "objects/Mesh.h"           // la malla que se dibuja: su centro (el gancho de la sonda)
#include "objects/RenderColors.h"   // el color de seleccion
#include "io/JsonW3d.h"             // JVal / JsonNumTexto: los campos del .w3d
#ifndef W3D_SIN_EDITOR
#include "render/RayTracer.h"       // RTHornearSondas: el horneado (solo el editor: el juego trae la sonda horneada)
#endif
#include <math.h>

namespace gfx = w3dEngine;

// las sondas VIVAS (las que estan en la escena las marca su dibujo: SondaLuz::pase)
static std::vector<SondaLuz*> gSondas;
static unsigned gPase = 1;
void SondaLuzNuevoPase() { gPase++; }

SondaLuz::SondaLuz(Object* parent, Vector3 pos) : Object(parent, "SondaLuz", pos), rayos(256), pase(0) {
    tam[0] = tam[1] = tam[2] = 4.0f;
    puntos[0] = puntos[1] = puntos[2] = 3;
    gSondas.push_back(this);
}
SondaLuz::~SondaLuz() {
    for (size_t i = 0; i < gSondas.size(); i++) if (gSondas[i] == this) { gSondas.erase(gSondas.begin() + (long)i); break; }
}

void SondaLuz::PuntoLocal(int i, int j, int k, float out[3]) const {
    const int ix[3] = { i, j, k };
    for (int e = 0; e < 3; e++) {
        const int n = puntos[e] < 2 ? 2 : puntos[e];
        out[e] = ((float)ix[e] / (float)(n - 1) - 0.5f) * tam[e];
    }
}

bool SondaLuzSeDibuja() { return w3dRenderOverlays; }
// (SIEMPRE: aunque los overlays esten apagados se tiene que recorrer, asi se anota en el pase y sigue iluminando)
bool SondaLuz::DibujaAlgo() { return true; }

void SondaLuzAristas(const SondaLuz* s, float out[72]) {
    const float hx = 0.5f * fabsf(s->tam[0]), hy = 0.5f * fabsf(s->tam[1]), hz = 0.5f * fabsf(s->tam[2]);
    float v[8][3];
    for (int i = 0; i < 8; i++) { v[i][0] = (i & 1) ? hx : -hx; v[i][1] = (i & 2) ? hy : -hy; v[i][2] = (i & 4) ? hz : -hz; }
    static const int kAristas[12][2] = { {0,1},{2,3},{4,5},{6,7}, {0,2},{1,3},{4,6},{5,7}, {0,4},{1,5},{2,6},{3,7} };
    int k = 0;
    for (int a = 0; a < 12; a++)
        for (int e = 0; e < 2; e++) { const float* p = v[kAristas[a][e]]; out[k++] = p[0]; out[k++] = p[1]; out[k++] = p[2]; }
}

// SELECCIONADA: LA CAJA (el alcance: lo que esta adentro toma su luz) y en cada PUNTO una ESFERA chica pintada con la luz
// horneada (cada vertice con la luz que le llega a una superficie que mira para ese lado): se ve como iluminaria a un
// objeto en ese lugar. Sin hornear, grises. Sin seleccionar solo el icono (render.cpp: RenderIcons3D). El juego no la
// dibuja, pero igual se anota en el pase (asi ilumina)
#ifdef W3D_SYMBIAN
// (el telefono: una esfera por PUNTO de la sonda, muchas a la vez -> 80 triangulos en vez de 252; el color se lee igual)
static const int kEsfSeg = 8, kEsfAnillos = 5;
#else
static const int kEsfSeg = 14, kEsfAnillos = 9;
#endif
static std::vector<float> gEsfN;          // la esfera unitaria: normales (= posiciones)
static std::vector<MeshIndex> gEsfIdx;    // sus triangulos
static void ArmarEsferaUnidad() {
    if (!gEsfN.empty()) return;
    for (int i = 0; i <= kEsfAnillos; i++) {
        const float la = 3.14159265f * (float)i / (float)kEsfAnillos, sl = sinf(la), cl = cosf(la);
        for (int j = 0; j <= kEsfSeg; j++) {
            const float lo = 6.2831853f * (float)j / (float)kEsfSeg;
            gEsfN.push_back(sl * cosf(lo)); gEsfN.push_back(cl); gEsfN.push_back(sl * sinf(lo));
        }
    }
    const int c = kEsfSeg + 1;
    for (int i = 0; i < kEsfAnillos; i++)
        for (int j = 0; j < kEsfSeg; j++) {
            const MeshIndex a = (MeshIndex)(i * c + j), b = (MeshIndex)(i * c + j + 1);
            const MeshIndex d = (MeshIndex)((i + 1) * c + j), e = (MeshIndex)((i + 1) * c + j + 1);
            gEsfIdx.push_back(a); gEsfIdx.push_back(d); gEsfIdx.push_back(b);
            gEsfIdx.push_back(b); gEsfIdx.push_back(d); gEsfIdx.push_back(e);
        }
}
void SondaLuz::RenderObject() {
    pase = gPase;
    if (!SondaLuzSeDibuja() || !select) return;
    static float lineas[72];
    SondaLuzAristas(this, lineas);
    const bool luzEstaba = gfx::IsEnabled(gfx::Lighting), texEstaba = gfx::IsEnabled(gfx::Texture2D);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    const float* sc = gRenderColors[(this == ObjActivo) ? RC_selActive : RC_selInactive];
    gfx::Color4f(sc[0], sc[1], sc[2], 1.0f);
    gfx::LineWidth(2.0f);
    gfx::VertexPointer3f(0, lineas);
    gfx::TexCoordPointer2f(12, lineas);   // (dummy valido: el driver del N95 lo lee aunque no haya textura)
    gfx::DrawLines(24);
    gfx::LineWidth(1.0f);
    // LAS ESFERAS: del tamano que entra entre puntos
    ArmarEsferaUnidad();
    float paso = 1e30f;
    for (int e = 0; e < 3; e++) { const int n = puntos[e] < 2 ? 2 : puntos[e]; const float d = fabsf(tam[e]) / (float)(n - 1); if (d < paso) paso = d; }
    const float r = 0.18f * paso;
    Matrix4 W; GetWorldMatrix(W);
    float R[9];   // la rotacion local -> mundo (columnas normalizadas: la escala no cuenta)
    for (int c = 0; c < 3; c++) {
        const float x = W.m[c*4], y = W.m[c*4+1], z = W.m[c*4+2], l = sqrtf(x*x + y*y + z*z);
        R[c*3] = l > 1e-6f ? x / l : 0.0f; R[c*3+1] = l > 1e-6f ? y / l : 0.0f; R[c*3+2] = l > 1e-6f ? z / l : 0.0f;
    }
    const int nv = (int)(gEsfN.size() / 3);
    static std::vector<float> pos; static std::vector<unsigned char> col;
    pos.resize((size_t)nv * 3); col.resize((size_t)nv * 4);
    const bool horneada = Horneada();
    gfx::EnableArray(gfx::ColorArray);
    int idx = 0;
    for (int k = 0; k < puntos[2]; k++)
        for (int j = 0; j < puntos[1]; j++)
            for (int i = 0; i < puntos[0]; i++, idx++) {
                float p[3]; PuntoLocal(i, j, k, p);
                const float* S = horneada ? &sh[(size_t)idx * 27] : NULL;
                for (int v = 0; v < nv; v++) {
                    const float nx = gEsfN[(size_t)v*3], ny = gEsfN[(size_t)v*3+1], nz = gEsfN[(size_t)v*3+2];
                    pos[(size_t)v*3] = p[0] + r * nx; pos[(size_t)v*3+1] = p[1] + r * ny; pos[(size_t)v*3+2] = p[2] + r * nz;
                    float c3[3] = { 0.5f, 0.5f, 0.5f };
                    if (S) {
                        const float wx = R[0]*nx + R[3]*ny + R[6]*nz, wy = R[1]*nx + R[4]*ny + R[7]*nz, wz = R[2]*nx + R[5]*ny + R[8]*nz;
                        const float Y[9] = { 0.282095f, 0.488603f*wy, 0.488603f*wz, 0.488603f*wx, 1.092548f*wx*wy, 1.092548f*wy*wz,
                                             0.315392f*(3.0f*wz*wz - 1.0f), 1.092548f*wx*wz, 0.546274f*(wx*wx - wy*wy) };
                        for (int c = 0; c < 3; c++) { float t = 0.0f; for (int q = 0; q < 9; q++) t += S[q*3+c] * Y[q]; c3[c] = t; }
                    }
                    for (int c = 0; c < 3; c++) col[(size_t)v*4+c] = (unsigned char)(c3[c] <= 0.0f ? 0 : c3[c] >= 1.0f ? 255 : (int)(c3[c] * 255.0f + 0.5f));
                    col[(size_t)v*4+3] = 255;
                }
                gfx::VertexPointer3f(0, &pos[0]);
                gfx::ColorPointer4ub(&col[0]);
                gfx::DrawTriangles((int)gEsfIdx.size(), &gEsfIdx[0]);
            }
    gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::NormalArray);
    if (texEstaba) gfx::Enable(gfx::Texture2D);
    if (luzEstaba) gfx::Enable(gfx::Lighting);
}

// ---- LA LUZ EN UN PUNTO DEL MUNDO ----
// la grilla de la sonda 's' en el punto LOCAL q: interpolacion TRILINEAL de los 8 puntos que lo rodean (afuera de la
// caja, el borde mas cercano)
static void InterpolarEn(const SondaLuz* s, const float q[3], float out[27]) {
    int i0[3]; float t[3];
    for (int e = 0; e < 3; e++) {
        const int n = s->puntos[e] < 2 ? 2 : s->puntos[e];
        float g = (s->tam[e] > 1e-6f ? q[e] / s->tam[e] : 0.0f) + 0.5f;
        g *= (float)(n - 1);
        if (g < 0.0f) g = 0.0f; if (g > (float)(n - 1)) g = (float)(n - 1);
        i0[e] = (int)floorf(g); if (i0[e] > n - 2) i0[e] = n - 2;
        t[e] = g - (float)i0[e];
    }
    for (int c = 0; c < 27; c++) out[c] = 0.0f;
    for (int b = 0; b < 8; b++) {
        const int ix = i0[0] + (b & 1), iy = i0[1] + ((b >> 1) & 1), iz = i0[2] + ((b >> 2) & 1);
        const float w = ((b & 1) ? t[0] : 1.0f - t[0]) * (((b >> 1) & 1) ? t[1] : 1.0f - t[1]) * (((b >> 2) & 1) ? t[2] : 1.0f - t[2]);
        if (w <= 0.0f) continue;
        const size_t base = ((size_t)iz * s->puntos[1] * s->puntos[0] + (size_t)iy * s->puntos[0] + (size_t)ix) * 27;
        for (int c = 0; c < 27; c++) out[c] += w * s->sh[base + c];
    }
}
bool SondaLuzEn(const float pos[3], float sh[27]) {
    const SondaLuz* mejor = NULL; float mejorD = 1e30f, mejorVol = 1e30f; float qMejor[3] = { 0, 0, 0 };
    for (size_t i = 0; i < gSondas.size(); i++) {
        const SondaLuz* s = gSondas[i];
        if (!s || !s->Horneada() || s->pase + 1 < gPase) continue;   // (sin hornear, o no esta en la escena)
        Matrix4 W, inv; s->GetWorldMatrix(W);
        if (!Matrix4::InvertirAfin(W, inv)) continue;
        const Vector3 ql = inv * Vector3(pos[0], pos[1], pos[2]);
        const float q[3] = { ql.x, ql.y, ql.z };
        // cuanto se sale de la caja (0 = adentro), en proporcion a su tamano
        float d = 0.0f, mayor = 0.0f;
        for (int e = 0; e < 3; e++) {
            const float h = 0.5f * fabsf(s->tam[e]);
            if (h > mayor) mayor = h;
            const float f = fabsf(q[e]) - h;
            if (f > d) d = f;
        }
        const float rel = mayor > 1e-6f ? d / (2.0f * mayor) : 1e30f;
        if (rel > 0.25f) continue;   // (lejos de la caja: no la alcanza)
        const float vol = fabsf(s->tam[0] * s->tam[1] * s->tam[2]);
        // adentro gana la mas CHICA (mas detalle); afuera, la mas cercana
        if (rel < mejorD - 1e-6f || (fabsf(rel - mejorD) <= 1e-6f && vol < mejorVol)) {
            mejor = s; mejorD = rel; mejorVol = vol; qMejor[0] = q[0]; qMejor[1] = q[1]; qMejor[2] = q[2];
        }
    }
    if (!mejor) return false;
    InterpolarEn(mejor, qMejor, sh);
    return true;
}

// ---- EL GANCHO DEL CORE (Mesh::RenderObject, ver objects/Mesh.h) ----
// al empezar una malla: la luz de su CENTRO (el del AABB de la malla, en el mundo). Con shaders va al backend
// (AmbienteSH: la luz por pixel); para el resto, el PROMEDIO es la luz ambiente de GL de ese objeto. Re-entrante (una
// malla dibujada adentro de otra: una instancia)
extern void (*g_w3dSondaObjetoHook)(Object* o, bool empieza);
extern bool  g_w3dSondaHay;
extern float g_w3dSondaSH[27];
extern float g_w3dOjoAMundo[9];                 // (EscenaRender: la vista de este pase, ojo -> mundo)
extern void  W3dAmbientePase(float c[4]);       // (EscenaRender: el ambiente del fondo de este pase)
namespace {
struct NivelSonda { bool aplicada; float sh[27]; };
NivelSonda gPila[8];
int gNivel = 0;
void AplicarSH(const float* sh) {
    for (int c = 0; c < 27; c++) g_w3dSondaSH[c] = sh[c];
    g_w3dSondaHay = true;
    gfx::AmbienteSH(g_w3dSondaSH, g_w3dOjoAMundo);
    float a[4] = { sh[0] * 0.282095f, sh[1] * 0.282095f, sh[2] * 0.282095f, 1.0f };
    for (int k = 0; k < 3; k++) if (a[k] < 0.0f) a[k] = 0.0f;
    gfx::LightModelAmbient(a);
}
void SacarSH() {
    g_w3dSondaHay = false;
    gfx::AmbienteSH(NULL, NULL);
    float a[4]; W3dAmbientePase(a);
    gfx::LightModelAmbient(a);
}
void HookSonda(Object* o, bool empieza) {
    if (empieza) {
        NivelSonda& n = gPila[gNivel < 8 ? gNivel : 7];
        if (gNivel < 8) gNivel++;
        n.aplicada = false;
        if (gSondas.empty() || !o || o->getType() != ObjectType::mesh) return;
        // LO ESTATICO no: su luz se PRECALCULA (la tarjeta "Bake Lighting"); la sonda es para lo que se mueve
        if (o->estatico) return;
        Mesh* me = (Mesh*)o;
        Vector3 cl(0, 0, 0);
        // (con ESQUELETO, el centro de la ULTIMA POSE: el de reposo se queda donde se modelo y la luz no seguia al
        //  personaje que el esqueleto lleva por la escena -el proxy de un personaje animado-)
        if (me->skinArmature && me->skinAabbOk)
            cl = Vector3(0.5f * (me->skinAabbMin.x + me->skinAabbMax.x), 0.5f * (me->skinAabbMin.y + me->skinAabbMax.y),
                         0.5f * (me->skinAabbMin.z + me->skinAabbMax.z));
        else if (me->aabbOk) cl = Vector3(0.5f * (me->aabbMin.x + me->aabbMax.x), 0.5f * (me->aabbMin.y + me->aabbMax.y),
                                          0.5f * (me->aabbMin.z + me->aabbMax.z));
        Matrix4 W; o->GetWorldMatrix(W);
        const Vector3 cw = W * cl;
        const float p[3] = { cw.x, cw.y, cw.z };
        if (!SondaLuzEn(p, n.sh)) return;
        n.aplicada = true;
        AplicarSH(n.sh);
    } else {
        if (gNivel <= 0) return;
        gNivel--;
        const bool estaba = gPila[gNivel < 8 ? gNivel : 7].aplicada;
        if (!estaba) return;
        // (vuelve la de la malla de afuera, si tenia)
        if (gNivel > 0 && gPila[gNivel - 1].aplicada) AplicarSH(gPila[gNivel - 1].sh);
        else SacarSH();
    }
}
struct RegistrarSonda { RegistrarSonda() { g_w3dSondaObjetoHook = HookSonda; } } gRegistrarSonda;
}

// ---- HORNEAR ----
bool SondaLuzHornearEmpezar(SondaLuz* s, std::string* motivo) {
    if (!s) return false;
    for (int e = 0; e < 3; e++) { if (s->puntos[e] < 2) s->puntos[e] = 2; if (s->puntos[e] > 16) s->puntos[e] = 16; }
    Matrix4 W; s->GetWorldMatrix(W);
    std::vector<float> pos;
    pos.reserve((size_t)s->Cantidad() * 3);
    for (int k = 0; k < s->puntos[2]; k++)
        for (int j = 0; j < s->puntos[1]; j++)
            for (int i = 0; i < s->puntos[0]; i++) {
                float p[3]; s->PuntoLocal(i, j, k, p);
                const Vector3 w = W * Vector3(p[0], p[1], p[2]);
                pos.push_back(w.x); pos.push_back(w.y); pos.push_back(w.z);
            }
#ifdef W3D_SIN_EDITOR
    if (motivo) *motivo = "Nothing to bake"; return false;   // (el juego no hornea: usa la sonda horneada del .w3d)
#else
    if (!RTSondaEmpezarLuz(pos, s->rayos)) { if (motivo) *motivo = "Nothing to bake"; return false; }
    return true;
#endif
}

bool SondaLuzHornearTerminar(SondaLuz* s) {
    if (!s) return false;
    std::vector<float> sh;
    std::vector<unsigned char> ok;
#ifdef W3D_SIN_EDITOR
    return false;
#else
    if (!RTSondaTomarLuz(sh, &ok)) return false;
    if (sh.size() != (size_t)s->Cantidad() * 27 || ok.size() != (size_t)s->Cantidad()) return false;   // (cambio la grilla)
#endif
    // LOS PUNTOS ADENTRO DE ALGO (una pared, el piso, la malla misma) no ven la luz: toman el promedio de sus vecinos
    // validos de la grilla, pasada tras pasada, hasta que no queda ninguno sin llenar (o no hay de donde)
    const int nx = s->puntos[0], ny = s->puntos[1], nz = s->puntos[2];
    for (int pasada = 0; pasada < nx + ny + nz; pasada++) {
        std::vector<unsigned char> ok2 = ok;
        bool falta = false, lleno = false;
        for (int k = 0; k < nz; k++) for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) {
            const size_t id = (size_t)k * ny * nx + (size_t)j * nx + (size_t)i;
            if (ok[id]) continue;
            falta = true;
            float acc[27]; for (int c = 0; c < 27; c++) acc[c] = 0.0f;
            int cuantos = 0;
            static const int dv[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
            for (int v = 0; v < 6; v++) {
                const int a = i + dv[v][0], b = j + dv[v][1], c3 = k + dv[v][2];
                if (a < 0 || a >= nx || b < 0 || b >= ny || c3 < 0 || c3 >= nz) continue;
                const size_t vid = (size_t)c3 * ny * nx + (size_t)b * nx + (size_t)a;
                if (!ok[vid]) continue;
                for (int c = 0; c < 27; c++) acc[c] += sh[vid * 27 + c];
                cuantos++;
            }
            if (cuantos) { for (int c = 0; c < 27; c++) sh[id * 27 + c] = acc[c] / (float)cuantos; ok2[id] = 1; lleno = true; }
        }
        ok.swap(ok2);
        if (!falta || !lleno) break;
    }
    s->sh.swap(sh);
    return true;
}

bool SondaLuzViva(const SondaLuz* s) {
    for (size_t i = 0; i < gSondas.size(); i++) if (gSondas[i] == s) return true;
    return false;
}

// DE UNA SOLA VEZ (el harness)
bool SondaLuzHornear(SondaLuz* s, std::string* motivo) {
    if (!SondaLuzHornearEmpezar(s, motivo)) return false;
#ifndef W3D_SIN_EDITOR
    while (RTSondaActiva()) RTSondaPaso(100000);
#endif
    if (!SondaLuzHornearTerminar(s)) { if (motivo) *motivo = "Nothing to bake"; return false; }
    return true;
}

SondaLuz* SondaLuzDuplicar(SondaLuz* src) {
    if (!src) return NULL;
    SondaLuz* d = new SondaLuz(src->Parent, src->pos);
    for (int k = 0; k < 3; k++) { d->tam[k] = src->tam[k]; d->puntos[k] = src->puntos[k]; }
    d->rayos = src->rayos;
    d->sh = src->sh;
    return d;
}

// ---- .w3d ----
static void SlSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
void SondaLuzEscribirCampos(std::string& s, int ind, const SondaLuz* o) {
    s += ",\n"; SlSang(s, ind); s += "\"tam\": [";
    for (int k = 0; k < 3; k++) { if (k) s += ", "; s += JsonNumTexto(o->tam[k]); }
    s += "]";
    s += ",\n"; SlSang(s, ind); s += "\"puntos\": [";
    for (int k = 0; k < 3; k++) { if (k) s += ", "; s += JsonNumTexto((float)o->puntos[k]); }
    s += "]";
    s += ",\n"; SlSang(s, ind); s += "\"rayos\": "; s += JsonNumTexto((float)o->rayos);
    if (o->Horneada()) {   // (la luz horneada: 27 numeros por punto)
        s += ",\n"; SlSang(s, ind); s += "\"sh\": [";
        for (size_t i = 0; i < o->sh.size(); i++) { if (i) s += (i % 27 == 0) ? ",\n" : ", "; s += JsonNumTexto(o->sh[i]); }
        s += "]";
    }
}
void SondaLuzLeerCampos(JVal* j, SondaLuz* o) {
    if (!j || !o) return;
    JVal* t = JHijo(j, "tam", 5);
    if (t) for (size_t i = 0; i < 3 && i < JFilaLen(t); i++) o->tam[i] = fabsf(JFilaNum(t, i, 4.0f));
    JVal* p = JHijo(j, "puntos", 5);
    if (p) for (size_t i = 0; i < 3 && i < JFilaLen(p); i++) {
        int n = (int)(JFilaNum(p, i, 3.0f) + 0.5f); if (n < 2) n = 2; if (n > 16) n = 16; o->puntos[i] = n;
    }
    o->rayos = JI(j, "rayos", 256);
    o->sh.clear();
    JVal* sh = JHijo(j, "sh", 5);
    if (sh) {
        const size_t n = JFilaLen(sh);
        if (n == (size_t)o->Cantidad() * 27) { o->sh.resize(n); for (size_t i = 0; i < n; i++) o->sh[i] = JFilaNum(sh, i, 0.0f); }
    }
}
