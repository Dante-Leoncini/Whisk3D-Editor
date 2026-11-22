// ============================================================================
//  RealTimeShadows.cpp — ver RealTimeShadows.h
// ============================================================================
#include "render/RealTimeShadows.h"
#include "render/RenderOptions.h"   // W3dBackgroundAutoColor: el camino de la CPU deja la pantalla como estaba
#include "graphics/w3dGraphics.h"
#include "graphics/w3dTexture.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Light.h"
#include "objects/Materials.h"   // la luz horneada de los materiales (sobre ella solo hace sombra lo que se mueve)
#include "math/Matrix4.h"
#ifndef W3D_NO_EDITOR
#include "W3dRoots.h"               // las otras escenas abiertas en el editor tambien proyectan
#include "script/W3dScript.h"       // W3dScriptHasSession: jugando solo se ve la base
#endif
#include <math.h>
#include <string.h>
#include <vector>
#include <map>

namespace gfx = w3dEngine;

extern bool g_w3dShadowsPass;                 // (objects/Mesh.cpp: el pase tiene sombras)
extern void (*g_w3dShadowMeshHook)(Mesh* m);  // (objects/Mesh.cpp: al terminar de dibujar una malla que las recibe)

W3dShadowsOpt::W3dShadowsOpt()
    : on(false), distance(50.0f), cascades(2), border(5.0f), blur(0.0f), resolution(1024), smooth(true),
      intensity(1.0f), bias(1.0f) {
    cuts[0] = 10.0f; cuts[1] = 25.0f; cuts[2] = 40.0f;
    color[0] = color[1] = color[2] = 0.0f; color[3] = 1.0f;
}
W3dShadowsOpt g_shadows;
static int gState = 0;
int W3dShadowsState() { return gState; }
bool W3dShadowsWithShaders() { return gfx::ShadowsSupported(); }

void SkinMesh(Mesh* m);          // (animation/SkeletalAnimation.cpp: la pose del esqueleto en este cuadro, cacheada)
void W3dUpToDatePose(Mesh* m);   // (animation/VertexAnimation.cpp: la animacion por vertice / las formas, idem)
namespace {
struct V3 { float x, y, z; };
inline V3 Mk(float x, float y, float z) { V3 v; v.x = x; v.y = y; v.z = z; return v; }
inline float Dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 Cross(const V3& a, const V3& b) { return Mk(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline V3 Norm(const V3& a) { const float l = sqrtf(Dot(a, a)); return l > 1e-12f ? Mk(a.x / l, a.y / l, a.z / l) : Mk(0, 0, 1); }
inline V3 Point(const Matrix4& M, const V3& p) {
    return Mk(M.m[0] * p.x + M.m[4] * p.y + M.m[8] * p.z + M.m[12], M.m[1] * p.x + M.m[5] * p.y + M.m[9] * p.z + M.m[13],
              M.m[2] * p.x + M.m[6] * p.y + M.m[10] * p.z + M.m[14]);
}
// una matriz por FILAS (r0..r2 de 4: x, y, z, constante; la cuarta es 0 0 0 1) a la de GL (por columnas)
void FromRows(Matrix4& M, const float* r0, const float* r1, const float* r2) {
    for (int c = 0; c < 4; c++) { M.m[c * 4 + 0] = r0[c]; M.m[c * 4 + 1] = r1[c]; M.m[c * 4 + 2] = r2[c]; M.m[c * 4 + 3] = (c == 3) ? 1.0f : 0.0f; }
}

// la geometria que se dibuja: la generada por los modificadores, la pose del esqueleto o la de siempre (de RAM)
struct Geo { const float* pos; const MeshIndex* idx; int n, nv; };
bool GeoOf(Mesh* m, Geo& g) {
    if (m->genValid && m->genVertex && m->genFaces && m->genFacesSize >= 3 && m->genVertexSize > 0) {
        g.pos = m->genVertex; g.idx = m->genFaces; g.n = m->genFacesSize; g.nv = m->genVertexSize; return true;
    }
    const float* p = (m->skinArmature && m->skinVertex) ? m->skinVertex : m->vertex;
    if (!p || !m->faces || m->facesSize < 3 || m->vertexSize <= 0) return false;
    g.pos = p; g.idx = m->faces; g.n = m->facesSize; g.nv = m->vertexSize;
    return true;
}

// LO QUE ENTRA EN LAS SOMBRAS: una malla, su matriz de mundo y su esfera en el mundo
// horneada: su luz es la HORNEADA (un material con la capa al 100%): ahi solo hace sombra lo que se mueve
struct Piece { Mesh* m; Matrix4 W; V3 c; float r; bool projects, receives, baked; };

// EL SOL: la primera luz direccional de la escena (recorriendo el arbol: una borrada -viva para el Ctrl+Z- no esta)
Light* FindSun(Object* o) {
    if (!o) return NULL;
    if (o != SceneCollection && (!o->visible || W3dViewLocalHidden(o))) return NULL;
    if (o->getType() == ObjectType::light) { Light* l = (Light*)o; if (l->directional && l->intensity > 0.0f) return l; }
    for (size_t i = 0; i < o->Childrens.size(); i++) { Light* l = FindSun(o->Childrens[i]); if (l) return l; }
    return NULL;
}
void Traverse(Object* o, std::vector<Piece>& out) {
    if (!o) return;
    if (o != SceneCollection && (!o->visible || W3dViewLocalHidden(o))) return;
    if (o->getType() == ObjectType::mesh && (o->projectsShadow || o->receivesShadow) && !W3dViewLocalOnlyStep(o)) {
        Mesh* m = (Mesh*)o;
        // LA POSE DE ESTE CUADRO: el esqueleto (o la animacion por vertice) se ponia al dia recien al DIBUJAR la malla, que
        // es despues de este pase: la sombra se hacia con la pose del cuadro ANTERIOR y una malla animada se tapaba a si
        // misma con su pose vieja (partes negras que parpadeaban). Queda cacheada: el dibujo de la malla no la repite
        if (m->skinArmature) SkinMesh(m); else W3dUpToDatePose(m);
        Geo g;
        if (m->DrawsSomething() && GeoOf(m, g)) {
            Piece p; p.m = m; m->GetWorldMatrix(p.W);
            // la esfera: la de la malla (centerGeom / radiusGeom) o, si no esta calculada, la de sus vertices
            V3 cl = Mk(m->centerGeom.x, m->centerGeom.y, m->centerGeom.z);
            float rl = m->radiusGeom;
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
            if (m->skinArmature || m->genValid) rl *= 1.5f;
            p.c = Point(p.W, cl);
            float e = 0.0f;
            for (int c = 0; c < 3; c++) {
                const float s = sqrtf(p.W.m[c * 4] * p.W.m[c * 4] + p.W.m[c * 4 + 1] * p.W.m[c * 4 + 1] + p.W.m[c * 4 + 2] * p.W.m[c * 4 + 2]);
                if (s > e) e = s;
            }
            p.r = rl * e + 1e-4f;
            p.projects = o->projectsShadow && !o->catchesShadows; p.receives = o->receivesShadow;   // (un captador de sombras solo las recibe)
            // (sin ningun material con luz no las recibe, como con shaders)
            p.baked = false;
            bool withLight = m->materialsGroup.empty();
            for (size_t g2 = 0; g2 < m->materialsGroup.size(); g2++) {
                const Material* mt = m->materialsGroup[g2].material;
                if (!mt || mt->lighting) withLight = true;
                if (mt && mt->lighting && mt->FactorLightBaked() >= 0.999f) p.baked = true;
            }
            if (!withLight) p.receives = false;
            out.push_back(p);
        }
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) Traverse(o->Childrens[i], out);
}
void Pieces(std::vector<Piece>& out) {
    Traverse(SceneCollection, out);
#ifndef W3D_NO_EDITOR
    if (!W3dScriptHasSession()) {
        std::vector<Object*> other;
        W3dOtherVisibleRoots(other);
        for (size_t i = 0; i < other.size(); i++) Traverse(other[i], out);
    }
#endif
}

// LA BASE DE LA LUZ: z mira HACIA el sol (el +Y local de la lampara, como GL_POSITION con w = 0); x e y, de costado
void BaseLight(Light* sun, V3& x, V3& y, V3& z) {
    Matrix4 W; sun->GetWorldMatrix(W);
    z = Norm(Mk(W.m[4], W.m[5], W.m[6]));
    const V3 up = (fabsf(z.y) < 0.99f) ? Mk(0, 1, 0) : Mk(1, 0, 0);
    x = Norm(Cross(up, z));
    y = Cross(z, x);
}

// LA CAMARA: las 8 esquinas (en el mundo) del tramo de la vista entre las distancias d0 y d1 (en el eje de la vista),
// con la proyeccion del pase (el zoom y el paneo del editor incluidos)
void CornersSegment(const float* P, const Matrix4& Vinv, float d0, float d1, V3 out[8]) {
    const bool ortho = P[11] == 0.0f;
    int k = 0;
    for (int e = 0; e < 2; e++) {
        const float d = e ? d1 : d0;
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sx = -1; sx <= 1; sx += 2) {
                float x, y;
                if (ortho) { x = ((float)sx - P[12]) / P[0]; y = ((float)sy - P[13]) / P[5]; }
                else      { x = d * ((float)sx + P[8]) / P[0]; y = d * ((float)sy + P[9]) / P[5]; }
                out[k++] = Point(Vinv, Mk(x, y, -d));
            }
    }
}
// el cerca y el lejos de la proyeccion
void NearFar(const float* P, float& n, float& f) {
    if (P[11] == 0.0f) { n = (P[14] + 1.0f) / P[10]; f = (P[14] - 1.0f) / P[10]; }
    else               { n = P[14] / (P[10] - 1.0f); f = P[14] / (P[10] + 1.0f); }
    if (!(n > 0.0f)) n = 0.01f;
    if (!(f > n)) f = n + 1000.0f;
}
int PowerDown(int v) { int p = 1; while (p * 2 <= v) p *= 2; return p; }
int PowerUp(int v) { int p = 1; while (p < v) p *= 2; return p; }

// ============================================================================
//  CON SHADERS: LAS CASCADAS
// ============================================================================
float gData[gfx::ShadowDataN];

// lo que proyecta, dibujado con la matriz de mundo (la proyeccion es la de la luz): del VBO si lo tiene, si no de RAM
void DrawProjector(const Piece& p) {
    gfx::LoadMatrix(p.W.m);
    Mesh* m = p.m;
    if (m->genValid && m->genVertex && m->genFaces && m->genFacesSize >= 3) {
        gfx::UnbindVBOs();
        gfx::VertexPointer3f(0, m->genVertex);
        gfx::DrawTriangles(m->genFacesSize, m->genFaces);
        return;
    }
    m->RawPrepare();
    m->RawRange(0, m->facesSize / 3 - 1, false);
    m->RawFinish();
}

bool PassCascades(Light* sun, const std::vector<Piece>& pieces) {
    const W3dShadowsOpt& o = g_shadows;
    float Pm[16], Vm[16];
    gfx::GetMatrix(gfx::Projection, Pm); gfx::GetMatrix(gfx::ModelView, Vm);
    Matrix4 V, Vinv; memcpy(V.m, Vm, sizeof(Vm));
    if (!Matrix4::InvertAffine(V, Vinv)) return false;
    float near, far; NearFar(Pm, near, far);
    float dist = o.distance; if (dist > far) dist = far;
    if (!(dist > near)) return false;
    V3 lx, ly, lz; BaseLight(sun, lx, ly, lz);

    const int n = o.cascades < 1 ? 1 : (o.cascades > 4 ? 4 : o.cascades);
    // LOS CORTES: crecientes, adentro de la distancia; la ultima cascada llega a la distancia
    float cut[4];
    { float ant = near;
      for (int i = 0; i < 4; i++) {
          float c = (i < n - 1 && i < 3) ? o.cuts[i] : dist;
          if (c > dist) c = dist;
          if (c < ant + 0.01f) c = ant + 0.01f;
          if (i >= n - 1) c = dist;
          cut[i] = c; ant = c;
      } }
    // EL MAPA: las cascadas una al lado de la otra (2 x 1) o en 2 x 2
    const int cols = (n >= 2) ? 2 : 1, rows = (n >= 3) ? 2 : 1;
    int res = o.resolution < 128 ? 128 : o.resolution;
    while (res * cols > 8192 || res * rows > 8192) res /= 2;
    const int W = res * cols, H = res * rows;
    if (!gfx::ShadowMapBegin(W, H)) return false;

    // el estado del dibujo (se repone al final): solo la profundidad, las dos caras
    const bool antCull = gfx::IsEnabled(gfx::CullFace), antBlend = gfx::IsEnabled(gfx::Blend), antTex = gfx::IsEnabled(gfx::Texture2D);
    const bool antLight = gfx::IsEnabled(gfx::Lighting), antZ = gfx::IsEnabled(gfx::DepthTest);
    gfx::Disable(gfx::CullFace); gfx::Disable(gfx::Blend); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Lighting);
    gfx::Enable(gfx::DepthTest); gfx::DepthFunc(gfx::DepthLEqual); gfx::DepthMask(true);
    gfx::EnableArray(gfx::VertexArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray); gfx::DisableArray(gfx::TexCoordArray);
    gfx::MatrixMode(gfx::Projection); gfx::PushMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PushMatrix();

    memset(gData, 0, sizeof(gData));
    // LAS DOS PROFUNDIDADES (ver el shader, W3D_P_SOMBRA): primero TODO lo que proyecta en RG; despues, con el z-buffer
    // limpio, solo lo que SE MUEVE en BA (si hay luz horneada que lo reciba). Sobre lo horneado cuenta solo BA
    bool hasBaked = false;
    for (size_t k = 0; k < pieces.size() && !hasBaked; k++) hasBaked = pieces[k].receives && pieces[k].baked;
    Matrix4 mLight[4]; std::vector<int> selOf[4]; bool hasDynamic = false;
    gfx::ColorMask(true, true, false, false);
    float ant = near;
    for (int i = 0; i < n; i++) {
        V3 is[8]; CornersSegment(Pm, Vinv, ant, cut[i], is);
        ant = cut[i];
        // la ESFERA del tramo (no cambia al girar la camara: la sombra no tiembla) y su centro corrido de a un texel
        V3 center = Mk(0, 0, 0);
        for (int k = 0; k < 8; k++) { center.x += is[k].x; center.y += is[k].y; center.z += is[k].z; }
        center.x /= 8.0f; center.y /= 8.0f; center.z /= 8.0f;
        float R = 0.0f;
        for (int k = 0; k < 8; k++) { V3 d = Mk(is[k].x - center.x, is[k].y - center.y, is[k].z - center.z); const float l = sqrtf(Dot(d, d)); if (l > R) R = l; }
        R = ceilf(R * 16.0f) / 16.0f; if (R < 0.01f) R = 0.01f;
        const float texel = 2.0f * R / (float)res;
        float cx = Dot(center, lx), cy = Dot(center, ly);
        const float cz = Dot(center, lz);
        cx = floorf(cx / texel) * texel; cy = floorf(cy / texel) * texel;
        float zMin = cz - R, zMax = cz + R;
        // lo que proyecta en esta cascada: lo que la toca de costado (vista del sol) y no esta detras de todo
        std::vector<int> sel;
        for (size_t k = 0; k < pieces.size(); k++) {
            const Piece& p = pieces[k];
            if (!p.projects) continue;
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
          FromRows(mLight[i], r0, r1, r2);
          gfx::MatrixMode(gfx::Projection); gfx::LoadMatrix(mLight[i].m); gfx::MatrixMode(gfx::ModelView); }
        const int tx = i % 2, ty = i / 2;
        gfx::ShadowMapBox(tx * res, ty * res, res, res);
        for (size_t k = 0; k < sel.size(); k++) {
            const Piece& p = pieces[(size_t)sel[k]];
            if (!p.m->isStatic) hasDynamic = true;
            DrawProjector(p);
        }
        selOf[i].swap(sel);
        // LO QUE RECIBE: OJO -> (u, v, profundidad) del pedazo de esta cascada, con el sesgo de la profundidad adentro
        const float biasZ = texel * 1.0f * o.bias / zr;
        const float su = 0.5f / (R * (float)cols), sv = 0.5f / (R * (float)rows);
        const float r0[4] = { lx.x * su, lx.y * su, lx.z * su, (0.5f - 0.5f * cx / R + (float)tx) / (float)cols };
        const float r1[4] = { ly.x * sv, ly.y * sv, ly.z * sv, (0.5f - 0.5f * cy / R + (float)ty) / (float)rows };
        const float r2[4] = { -lz.x / zr, -lz.y / zr, -lz.z / zr, zMax / zr - biasZ };
        Matrix4 A; FromRows(A, r0, r1, r2);
        const Matrix4 S = A * Vinv;
        memcpy(&gData[i * 16], S.m, sizeof(S.m));
        gData[64 + i * 4 + 0] = (float)tx / (float)cols; gData[64 + i * 4 + 1] = (float)ty / (float)rows;
        gData[64 + i * 4 + 2] = (float)(tx + 1) / (float)cols; gData[64 + i * 4 + 3] = (float)(ty + 1) / (float)rows;
        gData[80 + i] = cut[i];
        // LA PENUMBRA DE CONTACTO: los texeles de penumbra por unidad de profundidad (0..1 de la cascada): la distancia
        // entre lo que tapa y lo que recibe (zr metros) por la tangente del tamanio del sol, en texeles
        { float d = o.blur < 0.0f ? 0.0f : (o.blur > 1.0f ? 1.0f : o.blur);
          gData[84 + i] = zr * (d * 0.1f) / texel; }
        gData[88 + i] = texel * 1.5f * o.bias;              // (el corrimiento por la normal)
    }
    // LO QUE SE MUEVE, aparte (BA): sin esta pasada BA queda "lejos" (el borrado) y lo horneado no recibe nada
    if (hasBaked && hasDynamic) {
        gfx::ShadowMapBox(0, 0, W, H);
        gfx::ColorMask(false, false, true, true);
        gfx::Clear(gfx::DepthBuffer);
        for (int i = 0; i < n; i++) {
            gfx::MatrixMode(gfx::Projection); gfx::LoadMatrix(mLight[i].m); gfx::MatrixMode(gfx::ModelView);
            gfx::ShadowMapBox((i % 2) * res, (i / 2) * res, res, res);
            for (size_t k = 0; k < selOf[i].size(); k++) {
                const Piece& p = pieces[(size_t)selOf[i][k]];
                if (!p.m->isStatic) DrawProjector(p);
            }
        }
    }
    // las que no hay: copias de la ultima (no se eligen: la ultima ya llega a la distancia)
    for (int i = n; i < 4; i++) {
        memcpy(&gData[i * 16], &gData[(n - 1) * 16], sizeof(float) * 16);
        for (int k = 0; k < 4; k++) gData[64 + i * 4 + k] = gData[64 + (n - 1) * 4 + k];
        gData[80 + i] = dist; gData[84 + i] = gData[84 + n - 1]; gData[88 + i] = gData[88 + n - 1];
    }
    gData[92] = (float)W; gData[93] = (float)H;
    float border = o.border; if (border < 0.0f) border = 0.0f; if (border > dist) border = dist;
    gData[94] = dist - border; gData[95] = dist;
    gData[96] = o.intensity < 0.0f ? 0.0f : (o.intensity > 1.0f ? 1.0f : o.intensity);
    // (Q.y: la penumbra MAXIMA en texeles, la busqueda de lo que tapa llega hasta ahi)
    gData[97] = (o.blur > 0.0f) ? 4.0f + 20.0f * (o.blur > 1.0f ? 1.0f : o.blur) : 0.0f;
    gData[98] = 0.0f; gData[99] = o.smooth ? 1.0f : 0.0f;
    gData[101] = o.color[0]; gData[102] = o.color[1]; gData[103] = o.color[2];
    gData[100] = (float)((int)sun->LightID - (int)GL_LIGHT0);

    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
    gfx::ShadowMapFinish();
    gfx::ColorMask(true, true, true, true);
    if (antCull) gfx::Enable(gfx::CullFace);
    if (antBlend) gfx::Enable(gfx::Blend);
    if (antTex) gfx::Enable(gfx::Texture2D);
    if (antLight) gfx::Enable(gfx::Lighting);
    if (!antZ) gfx::Disable(gfx::DepthTest);
    gfx::DepthFunc(gfx::DepthLess);
    gfx::ShadowReceptors(gData);
    return true;
}

// ============================================================================
//  SIN SHADERS (GL ES 1.1 del N95, el GL fijo): LA SOMBRA PROYECTADA POR OBJETO
// ============================================================================
// la textura de sombra de un objeto que recibe: oscura donde algo le tapa el sol. au / av pasan un punto LOCAL de la malla
// a la UV de la textura (u = au . p + au[3])
struct Mask { unsigned tex; int w, h; float au[4], av[4]; bool active; };
std::map<Mesh*, Mask> gMasks;
bool gProjActive = false;
unsigned gSignature = 0;
std::vector<unsigned char> gIds, gMore, gTmp;
std::vector<float> gUV;

inline void Mix(unsigned& h, const void* p, size_t n) {
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
}
// el numero de cada objeto en colores de 5 y 6 bits (entra igual en una pantalla de 16 bits: el N95): rojo y verde
inline void ColorById(int id, unsigned char& r, unsigned char& g) {
    const int r5 = (id >> 6) & 31, g6 = id & 63;
    r = (unsigned char)((r5 * 255 + 15) / 31); g = (unsigned char)((g6 * 255 + 31) / 63);
}
inline int IdOfColor(unsigned char r, unsigned char g) {
    const int r5 = ((int)r * 31 + 127) / 255, g6 = ((int)g * 63 + 127) / 255;
    return (r5 << 6) | g6;   // (2047: el fondo, nadie)
}

// lo que ve el sol en un texel (id) le tapa la luz al objeto 'yo': otro objeto que proyecta (sobre la luz horneada, solo
// si se mueve: la sombra de lo estatico ya esta en la textura)
inline bool Cap(const std::vector<Piece>& pieces, int id, int yo, bool baked) {
    if (id == yo || id < 1 || id > (int)pieces.size()) return false;
    const Piece& q = pieces[(size_t)id - 1];
    return q.projects && !(baked && q.m->isStatic);
}
void DrawRAM(const Piece& p) {
    Geo g; if (!GeoOf(p.m, g)) return;
    gfx::LoadMatrix(p.W.m);
    gfx::VertexPointer3f(0, g.pos);
    gfx::DrawTriangles(g.n, g.idx);
}

bool PassProjected(Light* sun, const std::vector<Piece>& pieces) {
    const W3dShadowsOpt& o = g_shadows;
    if (pieces.empty() || pieces.size() > 2046) return false;
    bool hasReceives = false, hasProjects = false;
    for (size_t i = 0; i < pieces.size(); i++) { if (pieces[i].receives) hasReceives = true; if (pieces[i].projects) hasProjects = true; }
    if (!hasReceives || !hasProjects) return false;
    float Pm[16], Vm[16];
    gfx::GetMatrix(gfx::Projection, Pm); gfx::GetMatrix(gfx::ModelView, Vm);
    Matrix4 V, Vinv; memcpy(V.m, Vm, sizeof(Vm));
    if (!Matrix4::InvertAffine(V, Vinv)) return false;
    float near, far; NearFar(Pm, near, far);
    float dist = o.distance; if (dist > far) dist = far;
    if (!(dist > near)) return false;
    V3 lx, ly, lz; BaseLight(sun, lx, ly, lz);
    // LA CAJA vista desde el sol: lo que entra en las sombras, recortado a lo que ve la camara hasta la distancia
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f, z0 = 1e30f, z1 = -1e30f;
    for (size_t i = 0; i < pieces.size(); i++) {
        const Piece& p = pieces[i];
        const float px = Dot(p.c, lx), py = Dot(p.c, ly), pz = Dot(p.c, lz);
        if (px - p.r < x0) x0 = px - p.r; if (px + p.r > x1) x1 = px + p.r;
        if (py - p.r < y0) y0 = py - p.r; if (py + p.r > y1) y1 = py + p.r;
        if (pz - p.r < z0) z0 = pz - p.r; if (pz + p.r > z1) z1 = pz + p.r;
    }
    { V3 is[8]; CornersSegment(Pm, Vinv, near, dist, is);
      float a0 = 1e30f, a1 = -1e30f, b0 = 1e30f, b1 = -1e30f;
      for (int k = 0; k < 8; k++) {
          const float px = Dot(is[k], lx), py = Dot(is[k], ly);
          if (px < a0) a0 = px; if (px > a1) a1 = px; if (py < b0) b0 = py; if (py > b1) b1 = py;
      }
      if (a0 > x0) x0 = a0; if (a1 < x1) x1 = a1; if (b0 > y0) y0 = b0; if (b1 < y1) y1 = b1; }
    if (!(x1 > x0) || !(y1 > y0)) return false;
    z0 -= 0.01f; z1 += 0.01f;
    // EL TAMANIO: potencias de dos que entren en el viewport (se dibuja ahi y se lee)
    int vp[4]; gfx::GetViewport(vp);
    const int lim = o.resolution < 512 ? (o.resolution < 64 ? 64 : o.resolution) : 512;
    const int tw = PowerDown(vp[2] < lim ? vp[2] : lim), th = PowerDown(vp[3] < lim ? vp[3] : lim);
    if (tw < 16 || th < 16) return false;

    // SI NADA CAMBIO, quedan las texturas de antes (el sol, la caja, cada objeto, su matriz y algunos de sus vertices)
    unsigned signature = 2166136261u;
    Mix(signature, &lx, sizeof(lx)); Mix(signature, &ly, sizeof(ly));
    { const float box[6] = { x0, x1, y0, y1, z0, z1 }; Mix(signature, box, sizeof(box)); }
    { const float op[6] = { o.intensity, o.smooth ? 1.0f : 0.0f, (float)(tw * 4096 + th), o.color[0], o.color[1], o.color[2] }; Mix(signature, op, sizeof(op)); }
    for (size_t i = 0; i < pieces.size(); i++) {
        const Piece& p = pieces[i];
        Mix(signature, &p.m, sizeof(p.m)); Mix(signature, p.W.m, sizeof(p.W.m));
        const unsigned char fl = (unsigned char)((p.projects ? 1 : 0) | (p.receives ? 2 : 0) | (p.baked ? 4 : 0) | (p.m->isStatic ? 8 : 0));
        Mix(signature, &fl, 1);
        Geo g; if (GeoOf(p.m, g)) {
            Mix(signature, &g.n, sizeof(g.n));
            const int step = g.nv > 16 ? g.nv / 16 : 1;
            for (int k = 0; k < g.nv; k += step) Mix(signature, &g.pos[k * 3], sizeof(float) * 3);
        }
    }
    if (signature == gSignature && gProjActive) return true;

    // ---- 1. LA GPU: cada objeto de su color, desde el sol, con z-buffer ----
    const bool antDither = gfx::IsEnabled(gfx::Dither), antMs = gfx::IsEnabled(gfx::Multisample);
    const bool antCull = gfx::IsEnabled(gfx::CullFace), antBlend = gfx::IsEnabled(gfx::Blend), antTex = gfx::IsEnabled(gfx::Texture2D);
    const bool antLight = gfx::IsEnabled(gfx::Lighting), antZ = gfx::IsEnabled(gfx::DepthTest), antFog = gfx::IsEnabled(gfx::Fog);
    const bool antSc = gfx::IsEnabled(gfx::ScissorTest);
    gfx::Disable(gfx::Dither); gfx::Disable(gfx::Multisample);   // (los colores exactos: son numeros)
    gfx::Disable(gfx::CullFace); gfx::Disable(gfx::Blend); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::ColorMaterial);
    gfx::Enable(gfx::DepthTest); gfx::DepthFunc(gfx::DepthLEqual); gfx::DepthMask(true); gfx::ColorMask(true, true, true, true);
    // (en el N95, en una superficie APARTE: dibujado en la ventana se veia -los colores planos- antes del swap)
    const bool outside = W3dOutsideScreenBegin && W3dOutsideScreenBegin(tw, th);
    const int ox = outside ? 0 : vp[0], oy = outside ? 0 : vp[1];
    gfx::Viewport(ox, oy, tw, th);
    gfx::Scissor(ox, oy, tw, th); gfx::Enable(gfx::ScissorTest);
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
      Matrix4 Ml; FromRows(Ml, r0, r1, r2); gfx::LoadMatrix(Ml.m); }
    gfx::MatrixMode(gfx::ModelView); gfx::PushMatrix();
    for (size_t i = 0; i < pieces.size(); i++) {
        unsigned char r, g; ColorById((int)i + 1, r, g);
        gfx::Color4ub(r, g, 0, 255);
        DrawRAM(pieces[i]);
    }
    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
    // ---- 2. LA CPU: los pixeles ----
    gIds.resize((size_t)tw * th * 4);
    gfx::ReadPixelsRGBA(ox, oy, tw, th, &gIds[0]);
    // la pantalla como estaba: el fondo del render y la profundidad al fondo (el cielo 360, si hay, se dibuja encima)
    { float bg[4] = { 0, 0, 0, 1 };
      if (!W3dBackgroundAutoColor(bg)) { bg[0] = g_renderBg[0]; bg[1] = g_renderBg[1]; bg[2] = g_renderBg[2]; bg[3] = 1.0f; }
      gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
      gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer); }
    if (outside) W3dOutsideScreenFinish();   // (de vuelta a la ventana)
    gfx::Viewport(vp[0], vp[1], vp[2], vp[3]);
    if (!antSc) gfx::Disable(gfx::ScissorTest);
    if (antDither) gfx::Enable(gfx::Dither);
    if (antMs) gfx::Enable(gfx::Multisample);
    if (antCull) gfx::Enable(gfx::CullFace);
    if (antBlend) gfx::Enable(gfx::Blend);
    if (antTex) gfx::Enable(gfx::Texture2D);
    if (antLight) gfx::Enable(gfx::Lighting);
    if (!antZ) gfx::Disable(gfx::DepthTest);
    if (antFog) gfx::Enable(gfx::Fog);
    gfx::DepthFunc(gfx::DepthLess);
    gfx::Color4f(1, 1, 1, 1);
    // de color a numero (en el lugar: 1 byte por texel)
    const int nt = tw * th;
    for (int t = 0; t < nt; t++) {
        const int id = IdOfColor(gIds[(size_t)t * 4], gIds[(size_t)t * 4 + 1]);
        gIds[(size_t)t * 2] = (unsigned char)(id & 255); gIds[(size_t)t * 2 + 1] = (unsigned char)(id >> 8);
    }

    // ---- 3. CADA OBJETO QUE RECIBE: su textura, solo del pedazo que le toca ----
    for (std::map<Mesh*, Mask>::iterator it = gMasks.begin(); it != gMasks.end(); ++it) it->second.active = false;
    const float sx = (float)tw / (x1 - x0), sy = (float)th / (y1 - y0);
    const float inten = o.intensity < 0.0f ? 0.0f : (o.intensity > 1.0f ? 1.0f : o.intensity);
    // (la mascara marca la sombra ENTERA -0-; el color y la intensidad se ponen al pasarla a la textura, como los shaders:
    //  sobre la luz horneada queda al menos un 25%, la luz indirecta tambien esta en la textura)
    const unsigned char darkRT = 0, darkBake = 0;
    for (size_t i = 0; i < pieces.size(); i++) {
        const Piece& p = pieces[i];
        if (!p.receives) continue;
        const int yo = (int)i + 1;
        const unsigned char dark = p.baked ? darkBake : darkRT;
        const float px = (Dot(p.c, lx) - x0) * sx, py = (Dot(p.c, ly) - y0) * sy;
        int rx0 = (int)floorf(px - p.r * sx) - 1, rx1 = (int)ceilf(px + p.r * sx) + 1;
        int ry0 = (int)floorf(py - p.r * sy) - 1, ry1 = (int)ceilf(py + p.r * sy) + 1;
        if (rx0 < 0) rx0 = 0; if (ry0 < 0) ry0 = 0; if (rx1 > tw) rx1 = tw; if (ry1 > th) ry1 = th;
        if (rx1 <= rx0 || ry1 <= ry0) continue;
        // la sombra: lo que ve el sol ahi es OTRO objeto que proyecta
        int has = 0;
        for (int y = ry0; y < ry1; y++)
            for (int x = rx0; x < rx1; x++) {
                const int t = y * tw + x;
                const int id = (int)gIds[(size_t)t * 2] | ((int)gIds[(size_t)t * 2 + 1] << 8);
                if (Cap(pieces, id, yo, p.baked)) has++;
            }
        if (!has) continue;
        // la textura: el pedazo con un texel de luz alrededor (afuera queda iluminado)
        const int mw = PowerUp(rx1 - rx0 + 2), mh = PowerUp(ry1 - ry0 + 2);
        gMore.assign((size_t)mw * mh, 255);
        for (int y = ry0; y < ry1; y++)
            for (int x = rx0; x < rx1; x++) {
                const int t = y * tw + x;
                const int id = (int)gIds[(size_t)t * 2] | ((int)gIds[(size_t)t * 2 + 1] << 8);
                if (Cap(pieces, id, yo, p.baked)) gMore[(size_t)(y - ry0 + 1) * mw + (x - rx0 + 1)] = dark;
            }
        if (o.smooth) {   // (los bordes suaves: un promedio de 3 x 3; el borde de afuera sigue con luz)
            gTmp = gMore;
            for (int y = 1; y < mh - 1; y++)
                for (int x = 1; x < mw - 1; x++) {
                    int s = 0;
                    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) s += gTmp[(size_t)(y + dy) * mw + (x + dx)];
                    gMore[(size_t)y * mw + x] = (unsigned char)(s / 9);
                }
            for (int x = 0; x < mw; x++) { gMore[(size_t)x] = 255; gMore[(size_t)(mh - 1) * mw + x] = 255; }
            for (int y = 0; y < mh; y++) { gMore[(size_t)y * mw] = 255; gMore[(size_t)y * mw + mw - 1] = 255; }
        }
        gTmp.resize((size_t)mw * mh * 4);
        {   // el COLOR de la sombra por la intensidad: de la luz entera (255) al color, segun cuanto tapa
            const float floor = p.baked ? 0.25f : 0.0f;
            float c[3];
            for (int k = 0; k < 3; k++) { const float cc = o.color[k] < 0.0f ? 0.0f : (o.color[k] > 1.0f ? 1.0f : o.color[k]); c[k] = floor + (1.0f - floor) * cc; }
            for (int t = 0; t < mw * mh; t++) {
                const float a = (1.0f - gMore[(size_t)t] / 255.0f) * inten;   // (cuanto tapa)
                for (int k = 0; k < 3; k++) gTmp[(size_t)t * 4 + k] = (unsigned char)((1.0f + (c[k] - 1.0f) * a) * 255.0f + 0.5f);
                gTmp[(size_t)t * 4 + 3] = 255;
            }
        }
        Mask& m = gMasks[p.m];
#ifdef W3D_SYMBIAN
        // (el MBX del N95 dibuja en diferido: pisar la mascara que el cuadro anterior todavia lee daba parpadeos por
        // baldosas. Se espera a que termine: mas lento, sin parpadeos)
        if (m.tex && m.w == mw && m.h == mh) gfx::Finish();
#endif
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
        m.active = true;
    }
    // (las texturas de lo que ya no esta: se sueltan cuando se juntan muchas)
    if (gMasks.size() > 64) {
        for (std::map<Mesh*, Mask>::iterator it = gMasks.begin(); it != gMasks.end();) {
            if (!it->second.active) { if (it->second.tex) gfx::DeleteTexture(it->second.tex); gMasks.erase(it++); }
            else ++it;
        }
    }
    gSignature = signature;
    return true;
}

// EL GANCHO DE LA MALLA (sin shaders): recien dibujada, se la vuelve a dibujar encima con su textura de sombra, que
// MULTIPLICA (blanco = queda igual). Sin escribir la profundidad y con la misma geometria (pasa el z-test LEQUAL)
void HookMesh(Mesh* m) {
    if (!gProjActive || gfx::ShadowsSupported()) return;
    std::map<Mesh*, Mask>::iterator it = gMasks.find(m);
    if (it == gMasks.end() || !it->second.active || !it->second.tex) return;
    const Mask& ms = it->second;
    Geo g; if (!GeoOf(m, g)) return;
    gUV.resize((size_t)g.nv * 2);
    for (int i = 0; i < g.nv; i++) {
        const float* q = &g.pos[i * 3];
        gUV[(size_t)i * 2]     = ms.au[0] * q[0] + ms.au[1] * q[1] + ms.au[2] * q[2] + ms.au[3];
        gUV[(size_t)i * 2 + 1] = ms.av[0] * q[0] + ms.av[1] * q[1] + ms.av[2] * q[2] + ms.av[3];
    }
    const bool antFog = gfx::IsEnabled(gfx::Fog);
    gfx::UnbindVBOs();
    gfx::MixTexturesOff();
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::ColorMaterial); gfx::Disable(gfx::Fog);
    gfx::Enable(gfx::Texture2D); gfx::BindTexture(ms.tex);
    gfx::TexEnvReplace(false);
    gfx::MatrixMode(gfx::TextureMatrix); gfx::PushMatrix(); gfx::LoadIdentity(); gfx::MatrixMode(gfx::ModelView);
    gfx::Color4f(1, 1, 1, 1);
    gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::VertexPointer3f(0, g.pos); gfx::TexCoordPointer2f(0, &gUV[0]);
    gfx::Enable(gfx::Blend); gfx::SetMix(gfx::MixMultiply);
    gfx::DepthMask(false); gfx::DepthFunc(gfx::DepthLEqual);
    gfx::DrawTriangles(g.n, g.idx);
    gfx::DepthMask(true); gfx::DepthFunc(gfx::DepthLess);
    gfx::SetMix(gfx::MixAlpha); gfx::Disable(gfx::Blend);
    gfx::MatrixMode(gfx::TextureMatrix); gfx::PopMatrix(); gfx::MatrixMode(gfx::ModelView);
    gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Texture2D);
    if (antFog) gfx::Enable(gfx::Fog);
}
struct Hook { Hook() { g_w3dShadowMeshHook = HookMesh; } } gHook;
}   // namespace

void W3dShadowsPass() {
    g_w3dShadowsPass = false;
    gfx::ShadowReceptors(NULL);
    gState = 0;
    if (!g_shadows.on || !w3dRenderLights || !SceneCollection) { gProjActive = false; return; }
    Light* sun = FindSun(SceneCollection);
    if (!sun) { gState = 1; gProjActive = false; return; }
    gState = 2;
    std::vector<Piece> pieces;
    Pieces(pieces);
    if (gfx::ShadowsSupported()) {
        gProjActive = false;
        if (PassCascades(sun, pieces)) g_w3dShadowsPass = true;
        return;
    }
    gProjActive = PassProjected(sun, pieces);
    g_w3dShadowsPass = gProjActive;
}

void W3dShadowsEnd() {
    g_w3dShadowsPass = false;
    gfx::ShadowReceptors(NULL);
}
