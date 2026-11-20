// ============================================================================
//  SondaReflejo.cpp — ver SondaReflejo.h. Compartido editor/juego (C++03: Symbian lo compila).
// ============================================================================
#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "objects/SondaReflejo.h"
#include "objects/Textures.h"       // TexturaTomar / TexturaSoltar: la imagen 360
#include "objects/RenderColors.h"   // el color de seleccion
#include "graphics/w3dTexture.h"   // DeleteTexture: la copia borrosa del PBR
#include "io/JsonW3d.h"             // JVal / JsonNumTexto: los campos del .w3d
#ifndef W3D_SIN_EDITOR
#include "render/RayTracer.h"       // RTHornearReflejo: el horneado (solo el editor)
#include "io/TexturaEditada.h"      // la imagen horneada es una textura del proyecto (en memoria hasta guardar)
#include "render/OpcionesRender.h"  // g_redraw
#endif
#include <math.h>

namespace gfx = w3dEngine;
extern unsigned int w3dGetTicks();

// las sondas VIVAS
static std::vector<SondaReflejo*> gSondas;

// el gancho de la malla (objects/Mesh.cpp): un material con el reflejo en "La sonda mas cercana" pide la de su lugar
extern Texture* (*g_w3dReflejoSondaHook)(Object* o, float* caja, float* centro, bool* conCaja);
static Texture* ReflejoPara(Object* o, float* caja, float* centro, bool* conCaja) {
    if (conCaja) *conCaja = false;
    if (!o) return NULL;
    Matrix4 W; o->GetWorldMatrix(W);
    const float p[3] = { W.m[12], W.m[13], W.m[14] };
    SondaReflejo* s = SondaReflejoPara(p);
    if (!s) return NULL;
    Texture* t = s->Imagen();
    if (!t) return NULL;
    if (s->caja && caja && centro) { s->CajaMundo(caja); s->CentroMundo(centro); if (conCaja) *conCaja = true; }
    return t;
}
namespace { struct InstalarReflejo { InstalarReflejo() { g_w3dReflejoSondaHook = ReflejoPara; } }; }
static InstalarReflejo gInstalarReflejo;

SondaReflejo::SondaReflejo(Object* parent, Vector3 pos)
    : Object(parent, "SondaReflejo", pos), tipo(W3D_REFLEJO_HORNEADA), importancia(1), intensidad(1.0f), caja(false),
      resolucion(256), fondo(0), cerca(0.3f), muestras(32), rebotes(3), cadaSeg(0.0f), tex(NULL), ultimoMs(0), version(1),
      texBorrosa(0), versionBorrosa(0) {
    tam[0] = tam[1] = tam[2] = 10.0f;
    origen[0] = origen[1] = origen[2] = 0.0f;
    colorFondo[0] = 0.18f; colorFondo[1] = 0.22f; colorFondo[2] = 0.3f; colorFondo[3] = 1.0f;
    gSondas.push_back(this);
}
SondaReflejo::~SondaReflejo() {
    for (size_t i = 0; i < gSondas.size(); i++) if (gSondas[i] == this) { gSondas.erase(gSondas.begin() + (long)i); break; }
    if (tex) TexturaSoltar(tex);
    tex = NULL;
    if (texBorrosa) gfx::DeleteTexture(texBorrosa);
    texBorrosa = 0;
    if (vivo.iID) gfx::DeleteTexture(vivo.iID);
    vivo.iID = 0;
}

bool SondaReflejo::TieneImagen() const { return tipo == W3D_REFLEJO_TIEMPO_REAL ? vivo.iID != 0 : !textura.empty(); }
Texture* SondaReflejo::Imagen() {
    if (tipo == W3D_REFLEJO_TIEMPO_REAL) return vivo.iID ? &vivo : NULL;
    if (textura.empty()) { if (tex) { TexturaSoltar(tex); tex = NULL; } return NULL; }
    if (tex && tex->path == textura) return tex;
    if (tex) TexturaSoltar(tex);
    tex = TexturaTomar(textura);
    version++;
    return tex;
}
void SondaReflejo::CentroMundo(float out[3]) const {
    Matrix4 W; const_cast<SondaReflejo*>(this)->GetWorldMatrix(W);
    const Vector3 c = W * Vector3(origen[0], origen[1], origen[2]);
    out[0] = c.x; out[1] = c.y; out[2] = c.z;
}
void SondaReflejo::CajaMundo(float out[6]) const {
    Matrix4 W; const_cast<SondaReflejo*>(this)->GetWorldMatrix(W);
    out[0] = out[1] = out[2] = 1e30f; out[3] = out[4] = out[5] = -1e30f;
    for (int i = 0; i < 8; i++) {
        const Vector3 p = W * Vector3((i & 1) ? 0.5f * tam[0] : -0.5f * tam[0], (i & 2) ? 0.5f * tam[1] : -0.5f * tam[1],
                                      (i & 4) ? 0.5f * tam[2] : -0.5f * tam[2]);
        const float q[3] = { p.x, p.y, p.z };
        for (int k = 0; k < 3; k++) { if (q[k] < out[k]) out[k] = q[k]; if (q[k] > out[3 + k]) out[3 + k] = q[k]; }
    }
}

void SondaReflejoAristas(const SondaReflejo* s, float out[72]) {
    const float hx = 0.5f * fabsf(s->tam[0]), hy = 0.5f * fabsf(s->tam[1]), hz = 0.5f * fabsf(s->tam[2]);
    float v[8][3];
    for (int i = 0; i < 8; i++) { v[i][0] = (i & 1) ? hx : -hx; v[i][1] = (i & 2) ? hy : -hy; v[i][2] = (i & 4) ? hz : -hz; }
    static const int kAristas[12][2] = { {0,1},{2,3},{4,5},{6,7}, {0,2},{1,3},{4,6},{5,7}, {0,4},{1,5},{2,6},{3,7} };
    int k = 0;
    for (int a = 0; a < 12; a++)
        for (int e = 0; e < 2; e++) { const float* p = v[kAristas[a][e]]; out[k++] = p[0]; out[k++] = p[1]; out[k++] = p[2]; }
}

// LA ESFERA DE MUESTRA (la sonda seleccionada): un ESPEJO en el centro de la captura con la imagen de la sonda, como
// la de Unity: se ve lo que se horneo (o lo que esta viendo la de tiempo real) tal como lo refleja un metal liso. El
// reflejo se calcula por esquina en la CPU (la vista en cada vertice, reflejada en la normal y llevada al MUNDO), asi
// anda igual con shaders y en el pipeline fijo
extern float g_w3dOjoAMundo[9];   // (EscenaRender: la vista de este pase, ojo -> mundo)
static void EsferaMuestra(SondaReflejo* s) {
    Texture* t = s->Imagen();
    if (!t || !t->iID) return;
    const int NU = 32, NV = 16;
    static std::vector<float> nrm, pos, uv;
    if (nrm.empty()) {   // (la esfera unidad, triangulo por triangulo: cada esquina con su propia UV)
        const float PI = 3.14159265f;
        for (int j = 0; j < NV; j++)
            for (int i = 0; i < NU; i++) {
                const int esq[6][2] = { {i, j}, {i + 1, j}, {i + 1, j + 1}, {i, j}, {i + 1, j + 1}, {i, j + 1} };
                for (int k = 0; k < 6; k++) {
                    const float th = PI * (float)esq[k][1] / NV, ph = 2.0f * PI * (float)esq[k][0] / NU;
                    nrm.push_back(sinf(th) * cosf(ph)); nrm.push_back(cosf(th)); nrm.push_back(sinf(th) * sinf(ph));
                }
            }
        pos.resize(nrm.size()); uv.resize(nrm.size() / 3 * 2);
    }
    float MV[16]; gfx::GetMatrix(gfx::ModelView, MV);
    const float r = 0.5f;
    const size_t n = nrm.size() / 3;
    for (size_t k = 0; k < n; k++) {
        const float* q = &nrm[k * 3];
        float* p = &pos[k * 3];
        p[0] = s->origen[0] + r * q[0]; p[1] = s->origen[1] + r * q[1]; p[2] = s->origen[2] + r * q[2];
        // en OJO: el punto, la normal y la vista reflejada
        const float pe[3] = { MV[0] * p[0] + MV[4] * p[1] + MV[8] * p[2] + MV[12], MV[1] * p[0] + MV[5] * p[1] + MV[9] * p[2] + MV[13],
                              MV[2] * p[0] + MV[6] * p[1] + MV[10] * p[2] + MV[14] };
        float ne[3] = { MV[0] * q[0] + MV[4] * q[1] + MV[8] * q[2], MV[1] * q[0] + MV[5] * q[1] + MV[9] * q[2],
                        MV[2] * q[0] + MV[6] * q[1] + MV[10] * q[2] };
        float l = sqrtf(ne[0] * ne[0] + ne[1] * ne[1] + ne[2] * ne[2]); if (l < 1e-9f) l = 1.0f;
        ne[0] /= l; ne[1] /= l; ne[2] /= l;
        float v[3] = { pe[0], pe[1], pe[2] };
        l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l < 1e-9f) l = 1.0f;
        v[0] /= l; v[1] /= l; v[2] /= l;
        const float d = v[0] * ne[0] + v[1] * ne[1] + v[2] * ne[2];
        const float R[3] = { v[0] - 2.0f * d * ne[0], v[1] - 2.0f * d * ne[1], v[2] - 2.0f * d * ne[2] };
        // al MUNDO (ojo -> mundo, por columnas) y a la imagen 360: u = 0.5 + atan2(z, x) / 2pi, v = acos(y) / pi
        float w[3];
        for (int f = 0; f < 3; f++) w[f] = g_w3dOjoAMundo[0 * 3 + f] * R[0] + g_w3dOjoAMundo[1 * 3 + f] * R[1] + g_w3dOjoAMundo[2 * 3 + f] * R[2];
        const float wy = w[1] < -1.0f ? -1.0f : (w[1] > 1.0f ? 1.0f : w[1]);
        uv[k * 2] = 0.5f + atan2f(w[2], w[0]) * 0.15915494f;
        uv[k * 2 + 1] = acosf(wy) * 0.31830989f;
    }
    // (la COSTURA: un triangulo que cruza u = 0 / 1 iria por toda la imagen; sus u chicas pasan del otro lado)
    for (size_t k = 0; k + 2 < n; k += 3) {
        float mn = uv[k * 2], mx = mn;
        for (int e = 1; e < 3; e++) { const float u = uv[(k + e) * 2]; if (u < mn) mn = u; if (u > mx) mx = u; }
        if (mx - mn > 0.5f) for (int e = 0; e < 3; e++) if (uv[(k + e) * 2] < 0.5f) uv[(k + e) * 2] += 1.0f;
    }
    gfx::Disable(gfx::Lighting); gfx::Enable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::BindTexture(t->iID);
    gfx::TexFilter(true); gfx::TexWrap(true);
    gfx::TexEnvReplace(true);
    gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::VertexPointer3f(0, &pos[0]);
    gfx::TexCoordPointer2f(0, &uv[0]);
    gfx::DrawTrianglesArray((int)n);
    gfx::TexEnvReplace(false);
    gfx::Disable(gfx::Texture2D);
}

// SELECCIONADA: su CAJA (el alcance) y la esfera de muestra con su imagen. Sin seleccionar, solo el icono (render.cpp)
void SondaReflejo::RenderObject() {
    if (!w3dRenderOverlays || !select) return;
    static float lineas[72];
    SondaReflejoAristas(this, lineas);
    const bool luzEstaba = gfx::IsEnabled(gfx::Lighting), texEstaba = gfx::IsEnabled(gfx::Texture2D);
    EsferaMuestra(this);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    const float* sc = gRenderColors[(this == ObjActivo) ? RC_selActive : RC_selInactive];
    gfx::Color4f(sc[0], sc[1], sc[2], 1.0f);
    gfx::LineWidth(2.0f);
    gfx::VertexPointer3f(0, lineas);
    gfx::TexCoordPointer2f(12, lineas);   // (dummy valido: el driver del N95 lo lee aunque no haya textura)
    gfx::DrawLines(24);
    gfx::LineWidth(1.0f);
    gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    if (luzEstaba) gfx::Enable(gfx::Lighting);
    if (texEstaba) gfx::Enable(gfx::Texture2D);
}

SondaReflejo* SondaReflejoPara(const float pos[3], bool soloAdentro) {
    SondaReflejo* mejor = NULL; int mejorImp = -2147483647; float mejorVol = 1e30f;
    SondaReflejo* cerca = NULL; float cercaD = 1e30f;
    for (size_t i = 0; i < gSondas.size(); i++) {
        SondaReflejo* s = gSondas[i];
        if (!s || !s->visible || !s->TieneImagen()) continue;
        float b[6]; s->CajaMundo(b);
        const bool adentro = pos[0] >= b[0] && pos[0] <= b[3] && pos[1] >= b[1] && pos[1] <= b[4] && pos[2] >= b[2] && pos[2] <= b[5];
        if (adentro) {
            // (la de mas importancia; con la misma, la mas chica: la mas especifica del lugar)
            const float vol = (b[3] - b[0]) * (b[4] - b[1]) * (b[5] - b[2]);
            if (s->importancia > mejorImp || (s->importancia == mejorImp && vol < mejorVol)) { mejor = s; mejorImp = s->importancia; mejorVol = vol; }
            continue;
        }
        float c[3]; s->CentroMundo(c);
        const float d = (c[0] - pos[0]) * (c[0] - pos[0]) + (c[1] - pos[1]) * (c[1] - pos[1]) + (c[2] - pos[2]) * (c[2] - pos[2]);
        if (d < cercaD) { cercaD = d; cerca = s; }
    }
    return mejor ? mejor : (soloAdentro ? NULL : cerca);
}
void SondaReflejoListar(std::vector<SondaReflejo*>& out) { out = gSondas; }
bool SondaReflejoEsSuya(const Texture* t) {
    if (!t) return false;
    for (size_t i = 0; i < gSondas.size(); i++) {
        if (!gSondas[i]) continue;
        if (t == &gSondas[i]->vivo || (!gSondas[i]->textura.empty() && gSondas[i]->textura == t->path)) return true;
    }
    return false;
}

bool SondaReflejoHornearEmpezar(SondaReflejo* s, std::string* motivo) {
#ifdef W3D_SIN_EDITOR
    (void)s; if (motivo) *motivo = "Nothing to bake"; return false;   // (el juego no hornea: usa la imagen del .w3d)
#else
    if (!s) return false;
    if (s->tipo == W3D_REFLEJO_PERSONALIZADA) { if (motivo) *motivo = "A custom probe uses the image you choose"; return false; }
    // (la de tiempo real no se hornea: la dibuja la GPU, render/EscenaRender.cpp)
    if (s->tipo == W3D_REFLEJO_TIEMPO_REAL) { if (motivo) *motivo = "A realtime probe is drawn by the GPU"; return false; }
    const int w = s->resolucion < 32 ? 32 : (s->resolucion > 2048 ? 2048 : s->resolucion);
    const int h = w / 2;
    float c[3]; s->CentroMundo(c);
    // (el FONDO es el de la escena, tal cual: el color o el 360)
    return RTSondaEmpezarReflejo(c, w, h, s->cerca, s->muestras, s->rebotes, s->intensidad, motivo);
#endif
}

bool SondaReflejoHornearTerminar(SondaReflejo* s, std::string* motivo) {
#ifdef W3D_SIN_EDITOR
    (void)s; (void)motivo; return false;
#else
    if (!s) return false;
    const int w = s->resolucion < 32 ? 32 : (s->resolucion > 2048 ? 2048 : s->resolucion);
    const int h = w / 2;
    std::vector<unsigned char> rgba;
    if (!RTSondaTomarReflejo(rgba) || (int)rgba.size() != w * h * 4) { if (motivo) *motivo = "Nothing to bake"; return false; }
    // LA TEXTURA: la misma si ya tenia una del mismo tamanio; si no, una nueva del proyecto (en memoria hasta guardar)
    Texture* t = s->textura.empty() ? NULL : TexturaBuscar(s->textura);
    TexturaEditable* te = t ? TexEditObtener(t) : NULL;
    if (te && te->w == w && te->h == h && (int)te->rgba.size() == w * h * 4) {
        te->rgba = rgba;
        te->modificada = true;
        TexEditSubir(te, 0, 0, w, h);
    } else {
        t = TexEditCrear("Reflejo " + s->name, rgba, w, h, false);
        if (!t) { if (motivo) *motivo = "Could not create the texture"; return false; }
        s->textura = t->path;
    }
    s->Imagen();
    s->version++;   // (la copia borrosa del PBR se rehace)
    s->ultimoMs = w3dGetTicks();
    g_redraw = true;
    return true;
#endif
}

// DE UNA SOLA VEZ (el harness)
bool SondaReflejoHornear(SondaReflejo* s, std::string* motivo) {
    if (!SondaReflejoHornearEmpezar(s, motivo)) return false;
#ifndef W3D_SIN_EDITOR
    while (RTSondaActiva()) RTSondaPaso(100000);
#endif
    return SondaReflejoHornearTerminar(s, motivo);
}

SondaReflejo* SondaReflejoDuplicar(SondaReflejo* src) {
    if (!src) return NULL;
    SondaReflejo* d = new SondaReflejo(src->Parent, src->pos);
    d->tipo = src->tipo; d->importancia = src->importancia; d->intensidad = src->intensidad; d->caja = src->caja;
    for (int k = 0; k < 3; k++) { d->tam[k] = src->tam[k]; d->origen[k] = src->origen[k]; }
    for (int k = 0; k < 4; k++) d->colorFondo[k] = src->colorFondo[k];
    d->resolucion = src->resolucion; d->fondo = src->fondo; d->cerca = src->cerca; d->cadaSeg = src->cadaSeg;
    d->muestras = src->muestras; d->rebotes = src->rebotes;
    d->textura = src->textura;   // (la misma imagen hasta que se hornee otra: el horneado de la copia crea la suya)
    return d;
}

// ---- .w3d ----
static void SrSang(std::string& s, int n) { for (int i = 0; i < n; i++) s += "  "; }
static void SrEsc(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) { const char c = v[i]; if (c == '"' || c == '\\') { s += '\\'; s += c; } else s += c; }
    s += '"';
}
static void SrVec(std::string& s, int ind, const char* k, const float* v, int n) {
    s += ",\n"; SrSang(s, ind); s += "\""; s += k; s += "\": [";
    for (int i = 0; i < n; i++) { if (i) s += ", "; s += JsonNumTexto(v[i]); }
    s += "]";
}
void SondaReflejoEscribirCampos(std::string& s, int ind, const SondaReflejo* o) {
    static const char* kTipos[3] = { "horneada", "personalizada", "tiempoReal" };
    s += ",\n"; SrSang(s, ind); s += "\"reflejoTipo\": \""; s += kTipos[o->tipo < 0 || o->tipo > 2 ? 0 : o->tipo]; s += "\"";
    s += ",\n"; SrSang(s, ind); s += "\"importancia\": "; s += JsonNumTexto((float)o->importancia);
    s += ",\n"; SrSang(s, ind); s += "\"intensidad\": "; s += JsonNumTexto(o->intensidad);
    s += ",\n"; SrSang(s, ind); s += "\"caja\": "; s += o->caja ? "true" : "false";
    SrVec(s, ind, "tam", o->tam, 3);
    SrVec(s, ind, "origen", o->origen, 3);
    s += ",\n"; SrSang(s, ind); s += "\"resolucion\": "; s += JsonNumTexto((float)o->resolucion);
    s += ",\n"; SrSang(s, ind); s += "\"cerca\": "; s += JsonNumTexto(o->cerca);
    s += ",\n"; SrSang(s, ind); s += "\"muestras\": "; s += JsonNumTexto((float)o->muestras);
    s += ",\n"; SrSang(s, ind); s += "\"rebotes\": "; s += JsonNumTexto((float)o->rebotes);
    s += ",\n"; SrSang(s, ind); s += "\"cadaSeg\": "; s += JsonNumTexto(o->cadaSeg);
    if (!o->textura.empty()) { s += ",\n"; SrSang(s, ind); s += "\"textura\": "; SrEsc(s, o->textura); }
}
void SondaReflejoLeerCampos(JVal* j, SondaReflejo* o) {
    if (!j || !o) return;
    const std::string t = JS(j, "reflejoTipo", "horneada");
    o->tipo = t == "personalizada" ? W3D_REFLEJO_PERSONALIZADA : t == "tiempoReal" ? W3D_REFLEJO_TIEMPO_REAL : W3D_REFLEJO_HORNEADA;
    o->importancia = JI(j, "importancia", 1);
    o->intensidad = JF(j, "intensidad", 1.0f);
    o->caja = JB(j, "caja", false);
    JVal* v = JHijo(j, "tam", 5);
    if (v) for (size_t i = 0; i < 3 && i < JFilaLen(v); i++) o->tam[i] = fabsf(JFilaNum(v, i, 10.0f));
    v = JHijo(j, "origen", 5);
    if (v) for (size_t i = 0; i < 3 && i < JFilaLen(v); i++) o->origen[i] = JFilaNum(v, i, 0.0f);
    o->resolucion = JI(j, "resolucion", 256);
    o->fondo = 0;   // (el "fondo de color" de antes ya no existe: se hornea el fondo de la escena)
    o->cerca = JF(j, "cerca", 0.3f);
    o->muestras = JI(j, "muestras", 32); if (o->muestras < 1) o->muestras = 1;
    o->rebotes = JI(j, "rebotes", 3); if (o->rebotes < 0) o->rebotes = 0;
    o->cadaSeg = JF(j, "cadaSeg", 2.0f);
    o->textura = JS(j, "textura", "");
    // (una ruta del contenedor: la del proyecto que se esta leyendo; el contexto de una libreria la prefija)
}
