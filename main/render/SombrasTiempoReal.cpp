// ============================================================================
//  SombrasTiempoReal.cpp — ver SombrasTiempoReal.h
// ============================================================================
#include "render/SombrasTiempoReal.h"
#include "render/OpcionesRender.h"   // W3dFondoAutoColor: el camino de la CPU deja la pantalla como estaba
#include "graphics/w3dGraphics.h"
#include "graphics/w3dTexture.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Light.h"
#include "objects/Materials.h"   // la luz horneada de los materiales (sobre ella solo hace sombra lo que se mueve)
#include "math/Matrix4.h"
#ifndef W3D_SIN_EDITOR
#include "W3dRaices.h"              // las otras escenas abiertas en el editor tambien proyectan
#include "script/W3dScript.h"       // W3dScriptHayPartida: jugando solo se ve la base
#endif
#include <math.h>
#include <string.h>
#include <vector>
#include <map>

namespace gfx = w3dEngine;

extern bool g_w3dSombrasPase;                  // (objects/Mesh.cpp: el pase tiene sombras)
extern void (*g_w3dSombraMallaHook)(Mesh* m);  // (objects/Mesh.cpp: al terminar de dibujar una malla que las recibe)

W3dSombrasOpc::W3dSombrasOpc()
    : on(false), distancia(50.0f), cascadas(2), borde(5.0f), desenfoque(0.0f), resolucion(1024), suaves(true),
      intensidad(1.0f), sesgo(1.0f) {
    cortes[0] = 10.0f; cortes[1] = 25.0f; cortes[2] = 40.0f;
}
W3dSombrasOpc g_sombras;
static int gEstado = 0;
int W3dSombrasEstado() { return gEstado; }
bool W3dSombrasConShaders() { return gfx::SombrasSoportadas(); }

namespace {
struct V3 { float x, y, z; };
inline V3 Mk(float x, float y, float z) { V3 v; v.x = x; v.y = y; v.z = z; return v; }
inline float Dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 Cross(const V3& a, const V3& b) { return Mk(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline V3 Norm(const V3& a) { const float l = sqrtf(Dot(a, a)); return l > 1e-12f ? Mk(a.x / l, a.y / l, a.z / l) : Mk(0, 0, 1); }
inline V3 Punto(const Matrix4& M, const V3& p) {
    return Mk(M.m[0] * p.x + M.m[4] * p.y + M.m[8] * p.z + M.m[12], M.m[1] * p.x + M.m[5] * p.y + M.m[9] * p.z + M.m[13],
              M.m[2] * p.x + M.m[6] * p.y + M.m[10] * p.z + M.m[14]);
}
// una matriz por FILAS (r0..r2 de 4: x, y, z, constante; la cuarta es 0 0 0 1) a la de GL (por columnas)
void DeFilas(Matrix4& M, const float* r0, const float* r1, const float* r2) {
    for (int c = 0; c < 4; c++) { M.m[c * 4 + 0] = r0[c]; M.m[c * 4 + 1] = r1[c]; M.m[c * 4 + 2] = r2[c]; M.m[c * 4 + 3] = (c == 3) ? 1.0f : 0.0f; }
}

// la geometria que se dibuja: la generada por los modificadores, la pose del esqueleto o la de siempre (de RAM)
struct Geo { const float* pos; const MeshIndex* idx; int n, nv; };
bool GeoDe(Mesh* m, Geo& g) {
    if (m->genValido && m->genVertex && m->genFaces && m->genFacesSize >= 3 && m->genVertexSize > 0) {
        g.pos = m->genVertex; g.idx = m->genFaces; g.n = m->genFacesSize; g.nv = m->genVertexSize; return true;
    }
    const float* p = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex;
    if (!p || !m->faces || m->facesSize < 3 || m->vertexSize <= 0) return false;
    g.pos = p; g.idx = m->faces; g.n = m->facesSize; g.nv = m->vertexSize;
    return true;
}

// LO QUE ENTRA EN LAS SOMBRAS: una malla, su matriz de mundo y su esfera en el mundo
// horneada: su luz es la HORNEADA (un material con la capa al 100%): ahi solo hace sombra lo que se mueve
struct Pieza { Mesh* m; Matrix4 W; V3 c; float r; bool proyecta, recibe, horneada; };

// EL SOL: la primera luz direccional de la escena (recorriendo el arbol: una borrada -viva para el Ctrl+Z- no esta)
Light* BuscarSol(Object* o) {
    if (!o) return NULL;
    if (o != SceneCollection && (!o->visible || W3dVistaLocalOculta(o))) return NULL;
    if (o->getType() == ObjectType::light) { Light* l = (Light*)o; if (l->direccional && l->intensidad > 0.0f) return l; }
    for (size_t i = 0; i < o->Childrens.size(); i++) { Light* l = BuscarSol(o->Childrens[i]); if (l) return l; }
    return NULL;
}
void Recorrer(Object* o, std::vector<Pieza>& out) {
    if (!o) return;
    if (o != SceneCollection && (!o->visible || W3dVistaLocalOculta(o))) return;
    if (o->getType() == ObjectType::mesh && (o->proyectaSombra || o->recibeSombra) && !W3dVistaLocalSoloPaso(o)) {
        Mesh* m = (Mesh*)o;
        Geo g;
        if (m->DibujaAlgo() && GeoDe(m, g)) {
            Pieza p; p.m = m; m->GetWorldMatrix(p.W);
            // la esfera: la de la malla (centroGeom / radioGeom) o, si no esta calculada, la de sus vertices
            V3 cl = Mk(m->centroGeom.x, m->centroGeom.y, m->centroGeom.z);
            float rl = m->radioGeom;
            if (rl <= 0.0f) {
                V3 mn = Mk(g.pos[0], g.pos[1], g.pos[2]), mx = mn;
                for (int i = 1; i < g.nv; i++) {
                    const float* q = &g.pos[i * 3];
                    if (q[0] < mn.x) mn.x = q[0]; if (q[1] < mn.y) mn.y = q[1]; if (q[2] < mn.z) mn.z = q[2];
                    if (q[0] > mx.x) mx.x = q[0]; if (q[1] > mx.y) mx.y = q[1]; if (q[2] > mx.z) mx.z = q[2];
                }
                cl = Mk((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
                rl = sqrtf((mx.x - mn.x) * (mx.x - mn.x) + (mx.y - mn.y) * (mx.y - mn.y) + (mx.z - mn.z) * (mx.z - mn.z)) * 0.5f;
            }
            // (con esqueleto o modificadores la forma cambia: un margen)
            if (m->skinArmature || m->genValido) rl *= 1.5f;
            p.c = Punto(p.W, cl);
            float e = 0.0f;
            for (int c = 0; c < 3; c++) {
                const float s = sqrtf(p.W.m[c * 4] * p.W.m[c * 4] + p.W.m[c * 4 + 1] * p.W.m[c * 4 + 1] + p.W.m[c * 4 + 2] * p.W.m[c * 4 + 2]);
                if (s > e) e = s;
            }
            p.r = rl * e + 1e-4f;
            p.proyecta = o->proyectaSombra && !o->captaSombras; p.recibe = o->recibeSombra;   // (un captador de sombras solo las recibe)
            // (sin ningun material con luz no las recibe, como con shaders)
            p.horneada = false;
            bool conLuz = m->materialsGroup.empty();
            for (size_t g2 = 0; g2 < m->materialsGroup.size(); g2++) {
                const Material* mt = m->materialsGroup[g2].material;
                if (!mt || mt->lighting) conLuz = true;
                if (mt && mt->lighting && mt->FactorLuzHorneada() >= 0.999f) p.horneada = true;
            }
            if (!conLuz) p.recibe = false;
            out.push_back(p);
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) Recorrer(o->Childrens[i], out);
}
void Piezas(std::vector<Pieza>& out) {
    Recorrer(SceneCollection, out);
#ifndef W3D_SIN_EDITOR
    if (!W3dScriptHayPartida()) {
        std::vector<Object*> otras;
        W3dRaicesOtrasVisibles(otras);
        for (size_t i = 0; i < otras.size(); i++) Recorrer(otras[i], out);
    }
#endif
}

// LA BASE DE LA LUZ: z mira HACIA el sol (el +Y local de la lampara, como GL_POSITION con w = 0); x e y, de costado
void BaseLuz(Light* sol, V3& x, V3& y, V3& z) {
    Matrix4 W; sol->GetWorldMatrix(W);
    z = Norm(Mk(W.m[4], W.m[5], W.m[6]));
    const V3 arriba = (fabsf(z.y) < 0.99f) ? Mk(0, 1, 0) : Mk(1, 0, 0);
    x = Norm(Cross(arriba, z));
    y = Cross(z, x);
}

// LA CAMARA: las 8 esquinas (en el mundo) del tramo de la vista entre las distancias d0 y d1 (en el eje de la vista),
// con la proyeccion del pase (el zoom y el paneo del editor incluidos)
void EsquinasTramo(const float* P, const Matrix4& Vinv, float d0, float d1, V3 out[8]) {
    const bool orto = P[11] == 0.0f;
    int k = 0;
    for (int e = 0; e < 2; e++) {
        const float d = e ? d1 : d0;
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sx = -1; sx <= 1; sx += 2) {
                float x, y;
                if (orto) { x = ((float)sx - P[12]) / P[0]; y = ((float)sy - P[13]) / P[5]; }
                else      { x = d * ((float)sx + P[8]) / P[0]; y = d * ((float)sy + P[9]) / P[5]; }
                out[k++] = Punto(Vinv, Mk(x, y, -d));
            }
    }
}
// el cerca y el lejos de la proyeccion
void CercaLejos(const float* P, float& n, float& f) {
    if (P[11] == 0.0f) { n = (P[14] + 1.0f) / P[10]; f = (P[14] - 1.0f) / P[10]; }
    else               { n = P[14] / (P[10] - 1.0f); f = P[14] / (P[10] + 1.0f); }
    if (!(n > 0.0f)) n = 0.01f;
    if (!(f > n)) f = n + 1000.0f;
}
int PotenciaAbajo(int v) { int p = 1; while (p * 2 <= v) p *= 2; return p; }
int PotenciaArriba(int v) { int p = 1; while (p < v) p *= 2; return p; }

// ============================================================================
//  CON SHADERS: LAS CASCADAS
// ============================================================================
float gDatos[gfx::SombraDatosN];

// lo que proyecta, dibujado con la matriz de mundo (la proyeccion es la de la luz): del VBO si lo tiene, si no de RAM
void DibujarProyector(const Pieza& p) {
    gfx::LoadMatrix(p.W.m);
    Mesh* m = p.m;
    if (m->genValido && m->genVertex && m->genFaces && m->genFacesSize >= 3) {
        gfx::UnbindVBOs();
        gfx::VertexPointer3f(0, m->genVertex);
        gfx::DrawTriangles(m->genFacesSize, m->genFaces);
        return;
    }
    m->CrudoPreparar();
    m->CrudoRango(0, m->facesSize / 3 - 1, false);
    m->CrudoTerminar();
}

bool PaseCascadas(Light* sol, const std::vector<Pieza>& piezas) {
    const W3dSombrasOpc& o = g_sombras;
    float Pm[16], Vm[16];
    gfx::GetMatrix(gfx::Projection, Pm); gfx::GetMatrix(gfx::ModelView, Vm);
    Matrix4 V, Vinv; memcpy(V.m, Vm, sizeof(Vm));
    if (!Matrix4::InvertirAfin(V, Vinv)) return false;
    float cerca, lejos; CercaLejos(Pm, cerca, lejos);
    float dist = o.distancia; if (dist > lejos) dist = lejos;
    if (!(dist > cerca)) return false;
    V3 lx, ly, lz; BaseLuz(sol, lx, ly, lz);

    const int n = o.cascadas < 1 ? 1 : (o.cascadas > 4 ? 4 : o.cascadas);
    // LOS CORTES: crecientes, adentro de la distancia; la ultima cascada llega a la distancia
    float corte[4];
    { float ant = cerca;
      for (int i = 0; i < 4; i++) {
          float c = (i < n - 1 && i < 3) ? o.cortes[i] : dist;
          if (c > dist) c = dist;
          if (c < ant + 0.01f) c = ant + 0.01f;
          if (i >= n - 1) c = dist;
          corte[i] = c; ant = c;
      } }
    // EL MAPA: las cascadas una al lado de la otra (2 x 1) o en 2 x 2
    const int cols = (n >= 2) ? 2 : 1, filas = (n >= 3) ? 2 : 1;
    int res = o.resolucion < 128 ? 128 : o.resolucion;
    while (res * cols > 8192 || res * filas > 8192) res /= 2;
    const int W = res * cols, H = res * filas;
    if (!gfx::SombraMapaEmpezar(W, H)) return false;

    // el estado del dibujo (se repone al final): solo la profundidad, las dos caras
    const bool antCull = gfx::IsEnabled(gfx::CullFace), antBlend = gfx::IsEnabled(gfx::Blend), antTex = gfx::IsEnabled(gfx::Texture2D);
    const bool antLuz = gfx::IsEnabled(gfx::Lighting), antZ = gfx::IsEnabled(gfx::DepthTest);
    gfx::Disable(gfx::CullFace); gfx::Disable(gfx::Blend); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Lighting);
    gfx::Enable(gfx::DepthTest); gfx::DepthFunc(gfx::DepthLEqual); gfx::DepthMask(true);
    gfx::EnableArray(gfx::VertexArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray); gfx::DisableArray(gfx::TexCoordArray);
    gfx::MatrixMode(gfx::Projection); gfx::PushMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PushMatrix();

    memset(gDatos, 0, sizeof(gDatos));
    float ant = cerca;
    for (int i = 0; i < n; i++) {
        V3 es[8]; EsquinasTramo(Pm, Vinv, ant, corte[i], es);
        ant = corte[i];
        // la ESFERA del tramo (no cambia al girar la camara: la sombra no tiembla) y su centro corrido de a un texel
        V3 cen = Mk(0, 0, 0);
        for (int k = 0; k < 8; k++) { cen.x += es[k].x; cen.y += es[k].y; cen.z += es[k].z; }
        cen.x /= 8.0f; cen.y /= 8.0f; cen.z /= 8.0f;
        float R = 0.0f;
        for (int k = 0; k < 8; k++) { V3 d = Mk(es[k].x - cen.x, es[k].y - cen.y, es[k].z - cen.z); const float l = sqrtf(Dot(d, d)); if (l > R) R = l; }
        R = ceilf(R * 16.0f) / 16.0f; if (R < 0.01f) R = 0.01f;
        const float texel = 2.0f * R / (float)res;
        float cx = Dot(cen, lx), cy = Dot(cen, ly);
        const float cz = Dot(cen, lz);
        cx = floorf(cx / texel) * texel; cy = floorf(cy / texel) * texel;
        float zMin = cz - R, zMax = cz + R;
        // lo que proyecta en esta cascada: lo que la toca de costado (vista del sol) y no esta detras de todo
        std::vector<int> sel;
        for (size_t k = 0; k < piezas.size(); k++) {
            const Pieza& p = piezas[k];
            if (!p.proyecta) continue;
            const float px = Dot(p.c, lx), py = Dot(p.c, ly), pz = Dot(p.c, lz);
            if (fabsf(px - cx) > R + p.r || fabsf(py - cy) > R + p.r || pz + p.r < zMin) continue;
            if (pz + p.r > zMax) zMax = pz + p.r;
            sel.push_back((int)k);
        }
        zMax += texel; zMin -= texel;
        const float zr = zMax - zMin;
        // la proyeccion de la luz (mundo -> NDC): x, y en la esfera; z de zMax (-1, lo mas cerca del sol) a zMin (+1)
        { const float r0[4] = { lx.x / R, lx.y / R, lx.z / R, -cx / R };
          const float r1[4] = { ly.x / R, ly.y / R, ly.z / R, -cy / R };
          const float r2[4] = { -2.0f * lz.x / zr, -2.0f * lz.y / zr, -2.0f * lz.z / zr, 2.0f * zMax / zr - 1.0f };
          Matrix4 Ml; DeFilas(Ml, r0, r1, r2);
          gfx::MatrixMode(gfx::Projection); gfx::LoadMatrix(Ml.m); gfx::MatrixMode(gfx::ModelView); }
        const int tx = i % 2, ty = i / 2;
        gfx::SombraMapaCaja(tx * res, ty * res, res, res);
        // (el alfa del color: lo que se mueve -1- o es estatico -0-; sobre la luz horneada solo tapa lo que se mueve)
        for (size_t k = 0; k < sel.size(); k++) {
            const Pieza& p = piezas[(size_t)sel[k]];
            gfx::Color4f(1.0f, 1.0f, 1.0f, p.m->estatico ? 0.0f : 1.0f);
            DibujarProyector(p);
        }
        // LO QUE RECIBE: OJO -> (u, v, profundidad) del pedazo de esta cascada, con el sesgo de la profundidad adentro
        const float sesgoZ = texel * 1.0f * o.sesgo / zr;
        const float su = 0.5f / (R * (float)cols), sv = 0.5f / (R * (float)filas);
        const float r0[4] = { lx.x * su, lx.y * su, lx.z * su, (0.5f - 0.5f * cx / R + (float)tx) / (float)cols };
        const float r1[4] = { ly.x * sv, ly.y * sv, ly.z * sv, (0.5f - 0.5f * cy / R + (float)ty) / (float)filas };
        const float r2[4] = { -lz.x / zr, -lz.y / zr, -lz.z / zr, zMax / zr - sesgoZ };
        Matrix4 A; DeFilas(A, r0, r1, r2);
        const Matrix4 S = A * Vinv;
        memcpy(&gDatos[i * 16], S.m, sizeof(S.m));
        gDatos[64 + i * 4 + 0] = (float)tx / (float)cols; gDatos[64 + i * 4 + 1] = (float)ty / (float)filas;
        gDatos[64 + i * 4 + 2] = (float)(tx + 1) / (float)cols; gDatos[64 + i * 4 + 3] = (float)(ty + 1) / (float)filas;
        gDatos[80 + i] = corte[i];
        gDatos[84 + i] = o.desenfoque * 6.0f;                 // (en texeles a la distancia maxima: crece con la distancia)
        gDatos[88 + i] = texel * 1.5f * o.sesgo;              // (el corrimiento por la normal)
    }
    // las que no hay: copias de la ultima (no se eligen: la ultima ya llega a la distancia)
    for (int i = n; i < 4; i++) {
        memcpy(&gDatos[i * 16], &gDatos[(n - 1) * 16], sizeof(float) * 16);
        for (int k = 0; k < 4; k++) gDatos[64 + i * 4 + k] = gDatos[64 + (n - 1) * 4 + k];
        gDatos[80 + i] = dist; gDatos[84 + i] = gDatos[84 + n - 1]; gDatos[88 + i] = gDatos[88 + n - 1];
    }
    gDatos[92] = (float)W; gDatos[93] = (float)H;
    float borde = o.borde; if (borde < 0.0f) borde = 0.0f; if (borde > dist) borde = dist;
    gDatos[94] = dist - borde; gDatos[95] = dist;
    gDatos[96] = o.intensidad < 0.0f ? 0.0f : (o.intensidad > 1.0f ? 1.0f : o.intensidad);
    gDatos[97] = (float)n; gDatos[98] = 0.0f; gDatos[99] = o.suaves ? 1.0f : 0.0f;
    gDatos[100] = (float)((int)sol->LightID - (int)GL_LIGHT0);

    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
    gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    gfx::SombraMapaTerminar();
    if (antCull) gfx::Enable(gfx::CullFace);
    if (antBlend) gfx::Enable(gfx::Blend);
    if (antTex) gfx::Enable(gfx::Texture2D);
    if (antLuz) gfx::Enable(gfx::Lighting);
    if (!antZ) gfx::Disable(gfx::DepthTest);
    gfx::DepthFunc(gfx::DepthLess);
    gfx::SombraReceptores(gDatos);
    return true;
}

// ============================================================================
//  SIN SHADERS (GL ES 1.1 del N95, el GL fijo): LA SOMBRA PROYECTADA POR OBJETO
// ============================================================================
// la textura de sombra de un objeto que recibe: oscura donde algo le tapa el sol. au / av pasan un punto LOCAL de la malla
// a la UV de la textura (u = au . p + au[3])
struct Mascara { unsigned tex; int w, h; float au[4], av[4]; bool activa; };
std::map<Mesh*, Mascara> gMascaras;
bool gProyActiva = false;
unsigned gFirma = 0;
std::vector<unsigned char> gIds, gMas, gTmp;
std::vector<float> gUV;

inline void Mezclar(unsigned& h, const void* p, size_t n) {
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
}
// el numero de cada objeto en colores de 5 y 6 bits (entra igual en una pantalla de 16 bits: el N95): rojo y verde
inline void ColorDeId(int id, unsigned char& r, unsigned char& g) {
    const int r5 = (id >> 6) & 31, g6 = id & 63;
    r = (unsigned char)((r5 * 255 + 15) / 31); g = (unsigned char)((g6 * 255 + 31) / 63);
}
inline int IdDeColor(unsigned char r, unsigned char g) {
    const int r5 = ((int)r * 31 + 127) / 255, g6 = ((int)g * 63 + 127) / 255;
    return (r5 << 6) | g6;   // (2047: el fondo, nadie)
}

// lo que ve el sol en un texel (id) le tapa la luz al objeto 'yo': otro objeto que proyecta (sobre la luz horneada, solo
// si se mueve: la sombra de lo estatico ya esta en la textura)
inline bool Tapa(const std::vector<Pieza>& piezas, int id, int yo, bool horneada) {
    if (id == yo || id < 1 || id > (int)piezas.size()) return false;
    const Pieza& q = piezas[(size_t)id - 1];
    return q.proyecta && !(horneada && q.m->estatico);
}
void DibujarRAM(const Pieza& p) {
    Geo g; if (!GeoDe(p.m, g)) return;
    gfx::LoadMatrix(p.W.m);
    gfx::VertexPointer3f(0, g.pos);
    gfx::DrawTriangles(g.n, g.idx);
}

bool PaseProyectado(Light* sol, const std::vector<Pieza>& piezas) {
    const W3dSombrasOpc& o = g_sombras;
    if (piezas.empty() || piezas.size() > 2046) return false;
    bool hayRecibe = false, hayProyecta = false;
    for (size_t i = 0; i < piezas.size(); i++) { if (piezas[i].recibe) hayRecibe = true; if (piezas[i].proyecta) hayProyecta = true; }
    if (!hayRecibe || !hayProyecta) return false;
    float Pm[16], Vm[16];
    gfx::GetMatrix(gfx::Projection, Pm); gfx::GetMatrix(gfx::ModelView, Vm);
    Matrix4 V, Vinv; memcpy(V.m, Vm, sizeof(Vm));
    if (!Matrix4::InvertirAfin(V, Vinv)) return false;
    float cerca, lejos; CercaLejos(Pm, cerca, lejos);
    float dist = o.distancia; if (dist > lejos) dist = lejos;
    if (!(dist > cerca)) return false;
    V3 lx, ly, lz; BaseLuz(sol, lx, ly, lz);
    // LA CAJA vista desde el sol: lo que entra en las sombras, recortado a lo que ve la camara hasta la distancia
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f, z0 = 1e30f, z1 = -1e30f;
    for (size_t i = 0; i < piezas.size(); i++) {
        const Pieza& p = piezas[i];
        const float px = Dot(p.c, lx), py = Dot(p.c, ly), pz = Dot(p.c, lz);
        if (px - p.r < x0) x0 = px - p.r; if (px + p.r > x1) x1 = px + p.r;
        if (py - p.r < y0) y0 = py - p.r; if (py + p.r > y1) y1 = py + p.r;
        if (pz - p.r < z0) z0 = pz - p.r; if (pz + p.r > z1) z1 = pz + p.r;
    }
    { V3 es[8]; EsquinasTramo(Pm, Vinv, cerca, dist, es);
      float a0 = 1e30f, a1 = -1e30f, b0 = 1e30f, b1 = -1e30f;
      for (int k = 0; k < 8; k++) {
          const float px = Dot(es[k], lx), py = Dot(es[k], ly);
          if (px < a0) a0 = px; if (px > a1) a1 = px; if (py < b0) b0 = py; if (py > b1) b1 = py;
      }
      if (a0 > x0) x0 = a0; if (a1 < x1) x1 = a1; if (b0 > y0) y0 = b0; if (b1 < y1) y1 = b1; }
    if (!(x1 > x0) || !(y1 > y0)) return false;
    z0 -= 0.01f; z1 += 0.01f;
    // EL TAMANIO: potencias de dos que entren en el viewport (se dibuja ahi y se lee)
    int vp[4]; gfx::GetViewport(vp);
    const int lim = o.resolucion < 512 ? (o.resolucion < 64 ? 64 : o.resolucion) : 512;
    const int tw = PotenciaAbajo(vp[2] < lim ? vp[2] : lim), th = PotenciaAbajo(vp[3] < lim ? vp[3] : lim);
    if (tw < 16 || th < 16) return false;

    // SI NADA CAMBIO, quedan las texturas de antes (el sol, la caja, cada objeto, su matriz y algunos de sus vertices)
    unsigned firma = 2166136261u;
    Mezclar(firma, &lx, sizeof(lx)); Mezclar(firma, &ly, sizeof(ly));
    { const float caja[6] = { x0, x1, y0, y1, z0, z1 }; Mezclar(firma, caja, sizeof(caja)); }
    { const float op[3] = { o.intensidad, o.suaves ? 1.0f : 0.0f, (float)(tw * 4096 + th) }; Mezclar(firma, op, sizeof(op)); }
    for (size_t i = 0; i < piezas.size(); i++) {
        const Pieza& p = piezas[i];
        Mezclar(firma, &p.m, sizeof(p.m)); Mezclar(firma, p.W.m, sizeof(p.W.m));
        const unsigned char fl = (unsigned char)((p.proyecta ? 1 : 0) | (p.recibe ? 2 : 0) | (p.horneada ? 4 : 0) | (p.m->estatico ? 8 : 0));
        Mezclar(firma, &fl, 1);
        Geo g; if (GeoDe(p.m, g)) {
            Mezclar(firma, &g.n, sizeof(g.n));
            const int paso = g.nv > 16 ? g.nv / 16 : 1;
            for (int k = 0; k < g.nv; k += paso) Mezclar(firma, &g.pos[k * 3], sizeof(float) * 3);
        }
    }
    if (firma == gFirma && gProyActiva) return true;

    // ---- 1. LA GPU: cada objeto de su color, desde el sol, con z-buffer ----
    const bool antDither = gfx::IsEnabled(gfx::Dither), antMs = gfx::IsEnabled(gfx::Multisample);
    const bool antCull = gfx::IsEnabled(gfx::CullFace), antBlend = gfx::IsEnabled(gfx::Blend), antTex = gfx::IsEnabled(gfx::Texture2D);
    const bool antLuz = gfx::IsEnabled(gfx::Lighting), antZ = gfx::IsEnabled(gfx::DepthTest), antFog = gfx::IsEnabled(gfx::Fog);
    const bool antSc = gfx::IsEnabled(gfx::ScissorTest);
    gfx::Disable(gfx::Dither); gfx::Disable(gfx::Multisample);   // (los colores exactos: son numeros)
    gfx::Disable(gfx::CullFace); gfx::Disable(gfx::Blend); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::ColorMaterial);
    gfx::Enable(gfx::DepthTest); gfx::DepthFunc(gfx::DepthLEqual); gfx::DepthMask(true); gfx::ColorMask(true, true, true, true);
    gfx::Viewport(vp[0], vp[1], tw, th);
    gfx::Scissor(vp[0], vp[1], tw, th); gfx::Enable(gfx::ScissorTest);
    gfx::ClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
    gfx::UnbindVBOs();
    gfx::EnableArray(gfx::VertexArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray); gfx::DisableArray(gfx::TexCoordArray);
    gfx::MatrixMode(gfx::Projection); gfx::PushMatrix();
    { const float hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f, zr = z1 - z0;
      const float r0[4] = { lx.x / hx, lx.y / hx, lx.z / hx, -(x0 + hx) / hx };
      const float r1[4] = { ly.x / hy, ly.y / hy, ly.z / hy, -(y0 + hy) / hy };
      const float r2[4] = { -2.0f * lz.x / zr, -2.0f * lz.y / zr, -2.0f * lz.z / zr, 2.0f * z1 / zr - 1.0f };
      Matrix4 Ml; DeFilas(Ml, r0, r1, r2); gfx::LoadMatrix(Ml.m); }
    gfx::MatrixMode(gfx::ModelView); gfx::PushMatrix();
    for (size_t i = 0; i < piezas.size(); i++) {
        unsigned char r, g; ColorDeId((int)i + 1, r, g);
        gfx::Color4ub(r, g, 0, 255);
        DibujarRAM(piezas[i]);
    }
    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
    // ---- 2. LA CPU: los pixeles ----
    gIds.resize((size_t)tw * th * 4);
    gfx::ReadPixelsRGBA(vp[0], vp[1], tw, th, &gIds[0]);
    // la pantalla como estaba: el fondo del render y la profundidad al fondo (el cielo 360, si hay, se dibuja encima)
    { float bg[4] = { 0, 0, 0, 1 };
      if (!W3dFondoAutoColor(bg)) { bg[0] = g_renderBg[0]; bg[1] = g_renderBg[1]; bg[2] = g_renderBg[2]; bg[3] = 1.0f; }
      gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
      gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer); }
    gfx::Viewport(vp[0], vp[1], vp[2], vp[3]);
    if (!antSc) gfx::Disable(gfx::ScissorTest);
    if (antDither) gfx::Enable(gfx::Dither);
    if (antMs) gfx::Enable(gfx::Multisample);
    if (antCull) gfx::Enable(gfx::CullFace);
    if (antBlend) gfx::Enable(gfx::Blend);
    if (antTex) gfx::Enable(gfx::Texture2D);
    if (antLuz) gfx::Enable(gfx::Lighting);
    if (!antZ) gfx::Disable(gfx::DepthTest);
    if (antFog) gfx::Enable(gfx::Fog);
    gfx::DepthFunc(gfx::DepthLess);
    gfx::Color4f(1, 1, 1, 1);
    // de color a numero (en el lugar: 1 byte por texel)
    const int nt = tw * th;
    for (int t = 0; t < nt; t++) {
        const int id = IdDeColor(gIds[(size_t)t * 4], gIds[(size_t)t * 4 + 1]);
        gIds[(size_t)t * 2] = (unsigned char)(id & 255); gIds[(size_t)t * 2 + 1] = (unsigned char)(id >> 8);
    }

    // ---- 3. CADA OBJETO QUE RECIBE: su textura, solo del pedazo que le toca ----
    for (std::map<Mesh*, Mascara>::iterator it = gMascaras.begin(); it != gMascaras.end(); ++it) it->second.activa = false;
    const float sx = (float)tw / (x1 - x0), sy = (float)th / (y1 - y0);
    const float inten = o.intensidad < 0.0f ? 0.0f : (o.intensidad > 1.0f ? 1.0f : o.intensidad);
    // (sobre la luz horneada hasta un 75%, como los shaders: la luz indirecta tambien esta en la textura)
    const unsigned char oscuroRT = (unsigned char)((1.0f - inten) * 255.0f + 0.5f);
    const unsigned char oscuroHorn = (unsigned char)((1.0f - 0.75f * inten) * 255.0f + 0.5f);
    for (size_t i = 0; i < piezas.size(); i++) {
        const Pieza& p = piezas[i];
        if (!p.recibe) continue;
        const int yo = (int)i + 1;
        const unsigned char oscuro = p.horneada ? oscuroHorn : oscuroRT;
        const float px = (Dot(p.c, lx) - x0) * sx, py = (Dot(p.c, ly) - y0) * sy;
        int rx0 = (int)floorf(px - p.r * sx) - 1, rx1 = (int)ceilf(px + p.r * sx) + 1;
        int ry0 = (int)floorf(py - p.r * sy) - 1, ry1 = (int)ceilf(py + p.r * sy) + 1;
        if (rx0 < 0) rx0 = 0; if (ry0 < 0) ry0 = 0; if (rx1 > tw) rx1 = tw; if (ry1 > th) ry1 = th;
        if (rx1 <= rx0 || ry1 <= ry0) continue;
        // la sombra: lo que ve el sol ahi es OTRO objeto que proyecta
        int hay = 0;
        for (int y = ry0; y < ry1; y++)
            for (int x = rx0; x < rx1; x++) {
                const int t = y * tw + x;
                const int id = (int)gIds[(size_t)t * 2] | ((int)gIds[(size_t)t * 2 + 1] << 8);
                if (Tapa(piezas, id, yo, p.horneada)) hay++;
            }
        if (!hay) continue;
        // la textura: el pedazo con un texel de luz alrededor (afuera queda iluminado)
        const int mw = PotenciaArriba(rx1 - rx0 + 2), mh = PotenciaArriba(ry1 - ry0 + 2);
        gMas.assign((size_t)mw * mh, 255);
        for (int y = ry0; y < ry1; y++)
            for (int x = rx0; x < rx1; x++) {
                const int t = y * tw + x;
                const int id = (int)gIds[(size_t)t * 2] | ((int)gIds[(size_t)t * 2 + 1] << 8);
                if (Tapa(piezas, id, yo, p.horneada)) gMas[(size_t)(y - ry0 + 1) * mw + (x - rx0 + 1)] = oscuro;
            }
        if (o.suaves) {   // (los bordes suaves: un promedio de 3 x 3; el borde de afuera sigue con luz)
            gTmp = gMas;
            for (int y = 1; y < mh - 1; y++)
                for (int x = 1; x < mw - 1; x++) {
                    int s = 0;
                    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) s += gTmp[(size_t)(y + dy) * mw + (x + dx)];
                    gMas[(size_t)y * mw + x] = (unsigned char)(s / 9);
                }
            for (int x = 0; x < mw; x++) { gMas[(size_t)x] = 255; gMas[(size_t)(mh - 1) * mw + x] = 255; }
            for (int y = 0; y < mh; y++) { gMas[(size_t)y * mw] = 255; gMas[(size_t)y * mw + mw - 1] = 255; }
        }
        gTmp.resize((size_t)mw * mh * 4);
        for (int t = 0; t < mw * mh; t++) { gTmp[(size_t)t * 4] = gTmp[(size_t)t * 4 + 1] = gTmp[(size_t)t * 4 + 2] = gMas[(size_t)t]; gTmp[(size_t)t * 4 + 3] = 255; }
        Mascara& m = gMascaras[p.m];
        if (m.tex && m.w == mw && m.h == mh) gfx::UpdateRGBA(m.tex, 0, 0, mw, mh, &gTmp[0], false);
        else {
            if (m.tex) gfx::DeleteTexture(m.tex);
            m.tex = gfx::UploadRGBA(&gTmp[0], mw, mh, true, false, false, false);
            m.w = mw; m.h = mh;
            if (m.tex) { gfx::BindTexture(m.tex); gfx::TexWrap(false); gfx::TexFilter(true); gfx::BindTexture(0); }
        }
        if (!m.tex) continue;
        // un punto LOCAL de la malla -> la UV de su textura: u = ((x_sol(W p) - x0) * sx - (rx0 - 1)) / mw
        const float ku = sx / (float)mw, kv = sy / (float)mh;
        const float bu = (-x0 * sx - (float)(rx0 - 1)) / (float)mw, bv = (-y0 * sy - (float)(ry0 - 1)) / (float)mh;
        for (int c = 0; c < 3; c++) {
            m.au[c] = (p.W.m[c * 4] * lx.x + p.W.m[c * 4 + 1] * lx.y + p.W.m[c * 4 + 2] * lx.z) * ku;
            m.av[c] = (p.W.m[c * 4] * ly.x + p.W.m[c * 4 + 1] * ly.y + p.W.m[c * 4 + 2] * ly.z) * kv;
        }
        m.au[3] = (p.W.m[12] * lx.x + p.W.m[13] * lx.y + p.W.m[14] * lx.z) * ku + bu;
        m.av[3] = (p.W.m[12] * ly.x + p.W.m[13] * ly.y + p.W.m[14] * ly.z) * kv + bv;
        m.activa = true;
    }
    // (las texturas de lo que ya no esta: se sueltan cuando se juntan muchas)
    if (gMascaras.size() > 64) {
        for (std::map<Mesh*, Mascara>::iterator it = gMascaras.begin(); it != gMascaras.end();) {
            if (!it->second.activa) { if (it->second.tex) gfx::DeleteTexture(it->second.tex); gMascaras.erase(it++); }
            else ++it;
        }
    }
    gFirma = firma;
    return true;
}

// EL GANCHO DE LA MALLA (sin shaders): recien dibujada, se la vuelve a dibujar encima con su textura de sombra, que
// MULTIPLICA (blanco = queda igual). Sin escribir la profundidad y con la misma geometria (pasa el z-test LEQUAL)
void HookMalla(Mesh* m) {
    if (!gProyActiva || gfx::SombrasSoportadas()) return;
    std::map<Mesh*, Mascara>::iterator it = gMascaras.find(m);
    if (it == gMascaras.end() || !it->second.activa || !it->second.tex) return;
    const Mascara& ms = it->second;
    Geo g; if (!GeoDe(m, g)) return;
    gUV.resize((size_t)g.nv * 2);
    for (int i = 0; i < g.nv; i++) {
        const float* q = &g.pos[i * 3];
        gUV[(size_t)i * 2]     = ms.au[0] * q[0] + ms.au[1] * q[1] + ms.au[2] * q[2] + ms.au[3];
        gUV[(size_t)i * 2 + 1] = ms.av[0] * q[0] + ms.av[1] * q[1] + ms.av[2] * q[2] + ms.av[3];
    }
    const bool antFog = gfx::IsEnabled(gfx::Fog);
    gfx::UnbindVBOs();
    gfx::MezclaTexturasOff();
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::ColorMaterial); gfx::Disable(gfx::Fog);
    gfx::Enable(gfx::Texture2D); gfx::BindTexture(ms.tex);
    gfx::TexEnvReplace(false);
    gfx::MatrixMode(gfx::TextureMatrix); gfx::PushMatrix(); gfx::LoadIdentity(); gfx::MatrixMode(gfx::ModelView);
    gfx::Color4f(1, 1, 1, 1);
    gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::VertexPointer3f(0, g.pos); gfx::TexCoordPointer2f(0, &gUV[0]);
    gfx::Enable(gfx::Blend); gfx::SetMezcla(gfx::MezclaMultiply);
    gfx::DepthMask(false); gfx::DepthFunc(gfx::DepthLEqual);
    gfx::DrawTriangles(g.n, g.idx);
    gfx::DepthMask(true); gfx::DepthFunc(gfx::DepthLess);
    gfx::SetMezcla(gfx::MezclaAlpha); gfx::Disable(gfx::Blend);
    gfx::MatrixMode(gfx::TextureMatrix); gfx::PopMatrix(); gfx::MatrixMode(gfx::ModelView);
    gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Texture2D);
    if (antFog) gfx::Enable(gfx::Fog);
}
struct Gancho { Gancho() { g_w3dSombraMallaHook = HookMalla; } } gGancho;
}   // namespace

void W3dSombrasPase() {
    g_w3dSombrasPase = false;
    gfx::SombraReceptores(NULL);
    gEstado = 0;
    if (!g_sombras.on || !w3dRenderLuces || !SceneCollection) { gProyActiva = false; return; }
    Light* sol = BuscarSol(SceneCollection);
    if (!sol) { gEstado = 1; gProyActiva = false; return; }
    gEstado = 2;
    std::vector<Pieza> piezas;
    Piezas(piezas);
    if (gfx::SombrasSoportadas()) {
        gProyActiva = false;
        if (PaseCascadas(sol, piezas)) g_w3dSombrasPase = true;
        return;
    }
    gProyActiva = PaseProyectado(sol, piezas);
    g_w3dSombrasPase = gProyActiva;
}

void W3dSombrasFin() {
    g_w3dSombrasPase = false;
    gfx::SombraReceptores(NULL);
}
