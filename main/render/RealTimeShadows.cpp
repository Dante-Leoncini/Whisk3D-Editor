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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>
#include "app/W3dNoNearFar.h"   // (Windows: windows.h define near/far como macros vacias)

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
// LO QUE ESTA EN LA GPU de una mascara: su textura (w x h), au / av (un punto LOCAL de la malla -> la UV de la textura:
// u = au . p + au[3]), las UV de cada vertice (uvVbo, uvN vertices) y los triangulos de la malla que tocan la parte OSCURA
// (idxVbo: idxN indices de una malla de geoN; el gancho dibuja solo esos: en una esfera con un poco de sombra, unos cientos)
struct MaskGPU { unsigned tex; int w, h; float au[4], av[4]; unsigned uvVbo; int uvN; unsigned idxVbo; int idxN, geoN; };
void FreeMaskGPU(MaskGPU& s) {
    if (s.tex) gfx::DeleteTexture(s.tex);
    if (s.uvVbo) gfx::DeleteBuffer(s.uvVbo);
    if (s.idxVbo) gfx::DeleteBuffer(s.idxVbo);
    memset(&s, 0, sizeof(s));
}
// la textura de sombra de un objeto que recibe: oscura donde algo le tapa el sol. Tres juegos en la GPU que se van turnando
// (cur = el de ahora); key = de que salio el de ahora (si sale igual, no se vuelve a subir)
struct Mask { MaskGPU gpu[3]; int cur; unsigned key; bool active; };
std::map<Mesh*, Mask> gMasks;
bool gProjActive = false;
unsigned gSignature = 0;
std::vector<unsigned char> gMore, gBlur;
std::vector<unsigned> gRGBA;
std::vector<MeshIndex> gSub;
std::vector<int> gSat;
std::vector<float> gUV;

inline void Mix(unsigned& h, const void* p, size_t n) {
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
}
// el numero de "nadie" en el mapa de lo que ve el sol (los objetos van del 1 en adelante)
const int kIdBackground = 1023;

// lo que ve el sol en un texel (id) le tapa la luz al objeto 'yo': otro objeto que proyecta (sobre la luz horneada, solo
// si se mueve: la sombra de lo estatico ya esta en la textura)
inline bool Cap(const std::vector<Piece>& pieces, int id, int yo, bool baked) {
    if (id == yo || id < 1 || id > (int)pieces.size()) return false;
    const Piece& q = pieces[(size_t)id - 1];
    return q.projects && !(baked && q.m->isStatic);
}
// LO QUE VE EL SOL, armado en la CPU: en cada texel el numero del objeto mas cercano al sol (y su profundidad). Antes se
// dibujaba con la GPU en la pantalla y se leia: la lectura frenaba el cuadro hasta que la GPU terminara todo lo anterior
// (en el Ace, ~40 ms de 50 al mover un objeto), el driver convertia cada pixel y el tramado de 16 bits corria los numeros
std::vector<unsigned short> gIdMap;
std::vector<int> gIdZ, gTI, gBox;
std::vector<float> gTV;
std::vector<unsigned char> gFar;
const float kZOne = 1048576.0f;   // (la profundidad en enteros: 0..2 de la caja -> 0..2^21)
// redondeo de una coordenada del mapa (mas de -65536 dieciseisavos): sin floorf -una llamada a libm en el Ace- ni comparar
// floats -cada comparacion frena la FPU del ARM11-
inline int Round(float f) { return (int)(f + 65536.5f) - 65536; }
// UN TRIANGULO, ya en el mapa: x, y en dieciseisavos de texel (a lo sumo un margen afuera, ver RasterFar) y z (mas chica = mas
// cerca del sol). Los texeles con el centro adentro, de las dos caras. En enteros: el ARM11 del Ace compara los float muy lento
// (cada comparacion frena la FPU) y eran cuatro por texel
void RasterTri(const int* a, const int* b, const int* c, unsigned short id, int tw, int th) {
    int area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (area < 0) { const int* t = b; b = c; c = t; area = -area; }
    if (area == 0) return;
    const int mnx = a[0] < b[0] ? (a[0] < c[0] ? a[0] : c[0]) : (b[0] < c[0] ? b[0] : c[0]);
    const int mxx = a[0] > b[0] ? (a[0] > c[0] ? a[0] : c[0]) : (b[0] > c[0] ? b[0] : c[0]);
    const int mny = a[1] < b[1] ? (a[1] < c[1] ? a[1] : c[1]) : (b[1] < c[1] ? b[1] : c[1]);
    const int mxy = a[1] > b[1] ? (a[1] > c[1] ? a[1] : c[1]) : (b[1] > c[1] ? b[1] : c[1]);
    // (los texeles cuyo centro -x * 16 + 8- cae en la caja del triangulo)
    int x0 = (mnx + 7) >> 4, x1 = (mxx - 8) >> 4, y0 = (mny + 7) >> 4, y1 = (mxy - 8) >> 4;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > tw - 1) x1 = tw - 1; if (y1 > th - 1) y1 = th - 1;
    if (x1 < x0 || y1 < y0) return;
    // los pesos de cada vertice (lo que da su lado opuesto en el punto: los tres >= 0 adentro) y cuanto cambian por texel
    const int d0x = (b[1] - c[1]) * 16, d0y = (c[0] - b[0]) * 16;
    const int d1x = (c[1] - a[1]) * 16, d1y = (a[0] - c[0]) * 16;
    const int d2x = (a[1] - b[1]) * 16, d2y = (b[0] - a[0]) * 16;
    const int px = x0 * 16 + 8, py = y0 * 16 + 8;
    int r0 = (c[0] - b[0]) * (py - b[1]) - (c[1] - b[1]) * (px - b[0]);
    int r1 = (a[0] - c[0]) * (py - c[1]) - (a[1] - c[1]) * (px - c[0]);
    int r2 = (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
    // la profundidad en el plano del triangulo: la del primer vertice mas lo que agregan los otros dos por su peso
    const float inv = 1.0f / (float)area, zb = (float)(b[2] - a[2]), zc = (float)(c[2] - a[2]);
    const int dzx = (int)(((float)d1x * inv) * zb + ((float)d2x * inv) * zc);
    const int dzy = (int)(((float)d1y * inv) * zb + ((float)d2y * inv) * zc);
    int rz = a[2] + (int)(((float)r1 * inv) * zb + ((float)r2 * inv) * zc);
    for (int y = y0; y <= y1; y++) {
        int w0 = r0, w1 = r1, w2 = r2, z = rz;
        unsigned short* ids = &gIdMap[(size_t)y * tw];
        int* zs = &gIdZ[(size_t)y * tw];
        for (int x = x0; x <= x1; x++) {
            if ((w0 | w1 | w2) >= 0 && z <= zs[x]) { zs[x] = z; ids[x] = id; }
            w0 += d0x; w1 += d1x; w2 += d2x; z += dzx;
        }
        r0 += d0y; r1 += d1y; r2 += d2y; rz += dzy;
    }
}
// el margen alrededor del mapa (en dieciseisavos): adentro de el las cuentas de RasterTri entran en 32 bits
const float kFarMargin = 4096.0f;
// un triangulo con algun vertice lejos del mapa (en float): se recorta contra el margen (los lados nuevos caen sobre los del
// triangulo: adentro cubre lo mismo) y se dibuja en abanico
void RasterFar(const float* a, const float* b, const float* c, unsigned short id, int tw, int th) {
    const float lo = -kFarMargin, hx = tw * 16.0f + kFarMargin, hy = th * 16.0f + kFarMargin;
    float P[2][9][3]; int n = 3;
    for (int k = 0; k < 3; k++) { P[0][0][k] = a[k]; P[0][1][k] = b[k]; P[0][2][k] = c[k]; }
    int cur = 0;
    for (int plane = 0; plane < 4; plane++) {
        const int ax = plane & 1;                 // (0: x, 1: y)
        const bool low = plane < 2;               // (el borde de abajo o el de arriba)
        const float lim = low ? lo : (ax ? hy : hx);
        int m = 0;
        for (int i = 0; i < n; i++) {
            const float* u = P[cur][i]; const float* v = P[cur][(i + 1) % n];
            const bool iu = low ? u[ax] >= lim : u[ax] <= lim, iv = low ? v[ax] >= lim : v[ax] <= lim;
            if (iu) { for (int k = 0; k < 3; k++) P[1 - cur][m][k] = u[k]; m++; }
            if (iu != iv) {
                const float t = (lim - u[ax]) / (v[ax] - u[ax]);
                for (int k = 0; k < 3; k++) P[1 - cur][m][k] = u[k] + (v[k] - u[k]) * t;
                m++;
            }
        }
        cur = 1 - cur; n = m;
        if (n < 3) return;
    }
    int I[9][3];
    for (int i = 0; i < n; i++) {
        I[i][0] = Round(P[cur][i][0]); I[i][1] = Round(P[cur][i][1]);
        const float z = P[cur][i][2] < 0.0f ? 0.0f : (P[cur][i][2] > 2.0f ? 2.0f : P[cur][i][2]);
        I[i][2] = (int)(z * kZOne);
    }
    for (int i = 1; i + 1 < n; i++) RasterTri(I[0], I[i], I[i + 1], id, tw, th);
}

bool PassProjected(Light* sun, const std::vector<Piece>& pieces) {
    const W3dShadowsOpt& o = g_shadows;
    if (pieces.empty() || pieces.size() >= (size_t)kIdBackground) return false;
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
    // (recortada a lo que ve la camara solo si la escena es mas grande que la distancia de las sombras: una que entra
    //  entera se toma entera, y girar la vista no cambia la caja -ni hay que volver a armar las mascaras en cada cuadro-)
    if ((x1 - x0) > dist || (y1 - y0) > dist)
    { V3 is[8]; CornersSegment(Pm, Vinv, near, dist, is);
      float a0 = 1e30f, a1 = -1e30f, b0 = 1e30f, b1 = -1e30f;
      for (int k = 0; k < 8; k++) {
          const float px = Dot(is[k], lx), py = Dot(is[k], ly);
          if (px < a0) a0 = px; if (px > a1) a1 = px; if (py < b0) b0 = py; if (py > b1) b1 = py;
      }
      if (a0 > x0) x0 = a0; if (a1 < x1) x1 = a1; if (b0 > y0) y0 = b0; if (b1 < y1) y1 = b1; }
    if (!(x1 > x0) || !(y1 > y0)) return false;
    z0 -= 0.01f; z1 += 0.01f;
    // EL TAMANIO: potencias de dos hasta el tamanio de la vista (mas detalle no se veria)
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

    // ---- 1. LO QUE VE EL SOL: cada objeto con su numero, con profundidad (la caja de la luz -x0..x1, y0..y1- a los texeles,
    //         z0..z1 a -1..1 como un z-buffer: mas chico, mas cerca del sol) ----
    gIdMap.assign((size_t)tw * th, (unsigned short)kIdBackground);
    gIdZ.assign((size_t)tw * th, 0x7fffffff);
    Matrix4 Ml;
    { const float hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f, zr = z1 - z0;
      const float r0[4] = { lx.x / hx, lx.y / hx, lx.z / hx, -(x0 + hx) / hx };
      const float r1[4] = { ly.x / hy, ly.y / hy, ly.z / hy, -(y0 + hy) / hy };
      const float r2[4] = { -2.0f * lz.x / zr, -2.0f * lz.y / zr, -2.0f * lz.z / zr, 2.0f * z1 / zr - 1.0f };
      FromRows(Ml, r0, r1, r2); }
    const float htw = tw * 0.5f, hth = th * 0.5f;
    for (size_t i = 0; i < pieces.size(); i++) {
        Geo g; if (!GeoOf(pieces[i].m, g)) continue;
        const Matrix4 M = Ml * pieces[i].W;
        const float* k = M.m;
        gTV.resize((size_t)g.nv * 3); gTI.resize((size_t)g.nv * 3); gFar.resize((size_t)g.nv);
        const float hx = tw * 16.0f + kFarMargin, hy = th * 16.0f + kFarMargin;
        for (int v = 0; v < g.nv; v++) {
            const float* q = &g.pos[v * 3];
            float* t = &gTV[(size_t)v * 3];   // (x, y del texel en dieciseisavos; z de 0 a 2 adentro de la caja)
            t[0] = (k[0] * q[0] + k[4] * q[1] + k[8] * q[2] + k[12] + 1.0f) * htw * 16.0f;
            t[1] = (k[1] * q[0] + k[5] * q[1] + k[9] * q[2] + k[13] + 1.0f) * hth * 16.0f;
            t[2] = k[2] * q[0] + k[6] * q[1] + k[10] * q[2] + k[14] + 1.0f;
            const bool far = t[0] < -kFarMargin || t[0] > hx || t[1] < -kFarMargin || t[1] > hy;
            gFar[(size_t)v] = far ? 1 : 0;
            if (far) continue;
            int* e = &gTI[(size_t)v * 3];
            e[0] = Round(t[0]); e[1] = Round(t[1]);
            e[2] = (int)((t[2] < 0.0f ? 0.0f : (t[2] > 2.0f ? 2.0f : t[2])) * kZOne);
        }
        const unsigned short id = (unsigned short)(i + 1);
        for (int f = 0; f + 2 < g.n; f += 3) {
            const size_t ia = g.idx[f], ib = g.idx[f + 1], ic = g.idx[f + 2];
            if (gFar[ia] | gFar[ib] | gFar[ic]) RasterFar(&gTV[ia * 3], &gTV[ib * 3], &gTV[ic * 3], id, tw, th);
            else RasterTri(&gTI[ia * 3], &gTI[ib * 3], &gTI[ic * 3], id, tw, th);
        }
    }
    {   // (W3D_CAPTURE_IDS=<archivo.ppm>: lo que vio el sol, una vez, un color por objeto; para ver la sombra de la CPU donde
        //  no hay depurador)
        static bool dumped = false;
        const char* f = dumped ? NULL : getenv("W3D_CAPTURE_IDS");
        FILE* out = (f && *f) ? fopen(f, "wb") : NULL;
        if (out) {
            fprintf(out, "P6\n%d %d\n255\n", tw, th);
            for (int y = th - 1; y >= 0; y--)
                for (int x = 0; x < tw; x++) {
                    const int id = gIdMap[(size_t)y * tw + x];
                    const unsigned char px[3] = { (unsigned char)(id == kIdBackground ? 255 : id * 97), (unsigned char)(id == kIdBackground ? 255 : id * 57),
                                                  (unsigned char)(id == kIdBackground ? 255 : id * 31) };
                    fwrite(px, 1, 3, out);
                }
            fclose(out);
            dumped = true;
        }
    }

    // ---- 3. CADA OBJETO QUE RECIBE: su textura, solo del pedazo OSCURO ----
    for (std::map<Mesh*, Mask>::iterator it = gMasks.begin(); it != gMasks.end(); ++it) it->second.active = false;
    const float sx = (float)tw / (x1 - x0), sy = (float)th / (y1 - y0);
    const float inten = o.intensity < 0.0f ? 0.0f : (o.intensity > 1.0f ? 1.0f : o.intensity);
    // EL COLOR de cada nivel de la mascara (0 = la sombra entera, 255 = luz): una tabla con la cuenta de los shaders, de la
    // luz entera al color de la sombra segun cuanto tapa. Sobre la luz horneada queda al menos un 25% (la luz indirecta
    // tambien esta en la textura). Antes eran cuentas de punto flotante por texel
    unsigned lutLight[2][256];
    for (int b = 0; b < 2; b++) {
        const float floor = b ? 0.25f : 0.0f;
        float c[3];
        for (int k = 0; k < 3; k++) { const float cc = o.color[k] < 0.0f ? 0.0f : (o.color[k] > 1.0f ? 1.0f : o.color[k]); c[k] = floor + (1.0f - floor) * cc; }
        for (int v = 0; v < 256; v++) {
            const float a = (1.0f - v / 255.0f) * inten;   // (cuanto tapa)
            unsigned char px[4];
            for (int k = 0; k < 3; k++) px[k] = (unsigned char)((1.0f + (c[k] - 1.0f) * a) * 255.0f + 0.5f);
            px[3] = 255;
            memcpy(&lutLight[b][v], px, 4);
        }
    }
    // LA CAJA DE CADA OBJETO en lo que ve el sol (una pasada por el mapa): lo oscuro de cada uno que recibe sale de juntar las
    // de los que le tapan el sol, sin recorrer su pedazo entero (el piso y las paredes cubren casi todo el mapa)
    const int nIds = (int)pieces.size() + 1;
    gBox.assign((size_t)nIds * 4, 0);
    for (int id = 0; id < nIds; id++) { gBox[(size_t)id * 4] = tw; gBox[(size_t)id * 4 + 2] = th; }
    for (int y = 0; y < th; y++) {
        const unsigned short* row = &gIdMap[(size_t)y * tw];
        for (int x = 0; x < tw; x++) {
            const int id = row[x];
            if (id >= nIds) continue;   // (el fondo)
            int* bx = &gBox[(size_t)id * 4];
            if (x < bx[0]) bx[0] = x; if (x >= bx[1]) bx[1] = x + 1;
            if (y < bx[2]) bx[2] = y; bx[3] = y + 1;
        }
    }
    unsigned char capT[kIdBackground + 1];
    for (size_t i = 0; i < pieces.size(); i++) {
        const Piece& p = pieces[i];
        if (!p.receives) continue;
        const int yo = (int)i + 1;
        // quien le tapa el sol, por numero (una tabla: no se pregunta en cada texel)
        memset(capT, 0, sizeof(capT));
        for (size_t q = 0; q < pieces.size(); q++) capT[q + 1] = Cap(pieces, (int)q + 1, yo, p.baked) ? 1 : 0;
        const float px = (Dot(p.c, lx) - x0) * sx, py = (Dot(p.c, ly) - y0) * sy;
        int rx0 = (int)floorf(px - p.r * sx) - 1, rx1 = (int)ceilf(px + p.r * sx) + 1;
        int ry0 = (int)floorf(py - p.r * sy) - 1, ry1 = (int)ceilf(py + p.r * sy) + 1;
        if (rx0 < 0) rx0 = 0; if (ry0 < 0) ry0 = 0; if (rx1 > tw) rx1 = tw; if (ry1 > th) ry1 = th;
        if (rx1 <= rx0 || ry1 <= ry0) continue;
        // LO OSCURO (lo que ve el sol ahi es OTRO objeto que proyecta): su caja. La textura cubre solo esa caja con un texel
        // de luz alrededor; afuera la UV se sale y se estira el borde, que es luz. El piso con la sombra de una esfera es una
        // textura chica, no el piso entero (era el doble del pedazo por redondear, y se subia entera en cada cuadro)
        int dx0 = rx1, dx1 = rx0, dy0 = ry1, dy1 = ry0;
        for (int q = 1; q < nIds; q++) {
            if (!capT[q]) continue;
            const int* bx = &gBox[(size_t)q * 4];
            const int ax0 = bx[0] > rx0 ? bx[0] : rx0, ax1 = bx[1] < rx1 ? bx[1] : rx1;
            const int ay0 = bx[2] > ry0 ? bx[2] : ry0, ay1 = bx[3] < ry1 ? bx[3] : ry1;
            if (ax1 <= ax0 || ay1 <= ay0) continue;
            if (ax0 < dx0) dx0 = ax0; if (ax1 > dx1) dx1 = ax1; if (ay0 < dy0) dy0 = ay0; if (ay1 > dy1) dy1 = ay1;
        }
        if (dx1 <= dx0) continue;
        Mask& m = gMasks[p.m];
        // la textura: la caja + el borde de luz (+ uno mas con los bordes suaves, que el promedio corre la sombra un texel)
        const int pad = o.smooth ? 2 : 1;
        const int mw = PowerUp(dx1 - dx0 + 2 * pad), mh = PowerUp(dy1 - dy0 + 2 * pad);
        const int ox = dx0 - pad, oy = dy0 - pad;   // (el texel de la ID que cae en la esquina de la textura)
        const unsigned char dark = 0;   // (la mascara marca la sombra ENTERA: el color y la intensidad salen de la tabla)
        gMore.assign((size_t)mw * mh, 255);
        for (int y = dy0; y < dy1; y++) {
            const unsigned short* row = &gIdMap[(size_t)y * tw];
            const int base = (y - oy) * mw - ox;   // (x >= dx0 > ox: base + x no es negativo)
            for (int x = dx0; x < dx1; x++)
                if (capT[row[x]]) gMore[(size_t)(base + x)] = dark;
        }
        if (o.smooth) {   // (los bordes suaves: un promedio de 3 x 3; el borde de afuera sigue con luz)
            gBlur = gMore;
            for (int y = 1; y < mh - 1; y++)
                for (int x = 1; x < mw - 1; x++) {
                    int s = 0;
                    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) s += gBlur[(size_t)(y + dy) * mw + (x + dx)];
                    gMore[(size_t)y * mw + x] = (unsigned char)(s / 9);
                }
            for (int x = 0; x < mw; x++) { gMore[(size_t)x] = 255; gMore[(size_t)(mh - 1) * mw + x] = 255; }
            for (int y = 0; y < mh; y++) { gMore[(size_t)y * mw] = 255; gMore[(size_t)y * mw + mw - 1] = 255; }
        }
        // un punto LOCAL de la malla -> la UV de su textura: u = ((x_sol(W p) - x0) * sx - ox) / mw
        float au[4], av[4];
        const float ku = sx / (float)mw, kv = sy / (float)mh;
        const float bu = (-x0 * sx - (float)ox) / (float)mw, bv = (-y0 * sy - (float)oy) / (float)mh;
        for (int c = 0; c < 3; c++) {
            au[c] = (p.W.m[c * 4] * lx.x + p.W.m[c * 4 + 1] * lx.y + p.W.m[c * 4 + 2] * lx.z) * ku;
            av[c] = (p.W.m[c * 4] * ly.x + p.W.m[c * 4 + 1] * ly.y + p.W.m[c * 4 + 2] * ly.z) * kv;
        }
        au[3] = (p.W.m[12] * lx.x + p.W.m[13] * lx.y + p.W.m[14] * lx.z) * ku + bu;
        av[3] = (p.W.m[12] * ly.x + p.W.m[13] * ly.y + p.W.m[14] * ly.z) * kv + bv;
        // SI NADA CAMBIO (la sombra que le cae, como cae sobre la malla y la malla misma: se movio otro objeto, o este sin
        // cambiar lo que le tapa), queda lo que ya esta en la GPU
        Geo g;
        const bool vbo = gfx::VBOSupported() && GeoOf(p.m, g);
        unsigned key = 2166136261u;
        Mix(key, &gMore[0], gMore.size());
        { const float mk[7] = { (float)mw, (float)mh, inten, o.color[0], o.color[1], o.color[2], p.baked ? 1.0f : 0.0f }; Mix(key, mk, sizeof(mk)); }
        Mix(key, au, sizeof(au)); Mix(key, av, sizeof(av));
        if (vbo) {
            Mix(key, &g.pos, sizeof(g.pos)); Mix(key, &g.idx, sizeof(g.idx)); Mix(key, &g.n, sizeof(g.n)); Mix(key, &g.nv, sizeof(g.nv));
            Mix(key, &p.m->genVersion, sizeof(p.m->genVersion));
            const int step = g.nv > 16 ? g.nv / 16 : 1;
            for (int k = 0; k < g.nv; k += step) Mix(key, &g.pos[k * 3], sizeof(float) * 3);
        }
        m.active = true;
        if (m.gpu[m.cur].tex && m.key == key) continue;
        // LO NUEVO, EN RECURSOS NUEVOS: la textura y los buffers de esta mascara se crean de cero, y los de hace tres
        // mascaras se sueltan. Pisar uno que la GPU uso hace que el driver espere a que termine todo lo anterior (en el Ace,
        // 10 a 15 ms por cuadro al mover un objeto, aunque fuera de hace dos cuadros); uno nuevo no espera. En el MBX del N95
        // pisar el que el cuadro anterior todavia leia daba parpadeos por baldosas
        m.cur = (m.cur + 1) % 3;
        MaskGPU& s = m.gpu[m.cur];
        FreeMaskGPU(s);
        const unsigned* lut = lutLight[p.baked ? 1 : 0];
        gRGBA.resize((size_t)mw * mh);
        for (size_t t = 0; t < gRGBA.size(); t++) gRGBA[t] = lut[gMore[t]];
        s.tex = gfx::UploadRGBA((const unsigned char*)&gRGBA[0], mw, mh, true, false, false, false);
        if (!s.tex) continue;
        gfx::BindTexture(s.tex); gfx::TexWrap(false); gfx::TexFilter(true); gfx::BindTexture(0);
        s.w = mw; s.h = mh;
        memcpy(s.au, au, sizeof(au)); memcpy(s.av, av, sizeof(av));
        m.key = key;
        if (!vbo) continue;
        gUV.resize((size_t)g.nv * 2);
        for (int k = 0; k < g.nv; k++) {
            const float* q = &g.pos[k * 3];
            gUV[(size_t)k * 2]     = au[0] * q[0] + au[1] * q[1] + au[2] * q[2] + au[3];
            gUV[(size_t)k * 2 + 1] = av[0] * q[0] + av[1] * q[1] + av[2] * q[2] + av[3];
        }
        s.uvVbo = gfx::GenBuffer();
        gfx::ArrayBufferData(s.uvVbo, &gUV[0], g.nv * 2 * (int)sizeof(float));
        s.uvN = g.nv;
        // LOS TRIANGULOS QUE TOCAN LO OSCURO: los texeles de la mascara debajo de cada uno (su caja, con uno de margen por el
        // filtro de la textura), contados con una tabla de sumas. Lo oscuro de la mascara que cae afuera de la malla (lo que
        // el sol ve alrededor: el piso, una pared) no la toca: esos triangulos no se dibujan
        gSat.assign((size_t)(mw + 1) * (mh + 1), 0);
        for (int y = 0; y < mh; y++) {
            int row = 0;
            for (int x = 0; x < mw; x++) {
                row += gMore[(size_t)y * mw + x] < 255 ? 1 : 0;
                gSat[(size_t)(y + 1) * (mw + 1) + x + 1] = gSat[(size_t)y * (mw + 1) + x + 1] + row;
            }
        }
        gSub.clear();
        for (int k = 0; k + 2 < g.n; k += 3) {
            const float* a = &gUV[(size_t)g.idx[k] * 2];
            const float* b = &gUV[(size_t)g.idx[k + 1] * 2];
            const float* c = &gUV[(size_t)g.idx[k + 2] * 2];
            const float tu0 = a[0] < b[0] ? (a[0] < c[0] ? a[0] : c[0]) : (b[0] < c[0] ? b[0] : c[0]);
            const float tu1 = a[0] > b[0] ? (a[0] > c[0] ? a[0] : c[0]) : (b[0] > c[0] ? b[0] : c[0]);
            const float tv0 = a[1] < b[1] ? (a[1] < c[1] ? a[1] : c[1]) : (b[1] < c[1] ? b[1] : c[1]);
            const float tv1 = a[1] > b[1] ? (a[1] > c[1] ? a[1] : c[1]) : (b[1] > c[1] ? b[1] : c[1]);
            int x0 = (int)floorf(tu0 * mw) - 1, x1 = (int)floorf(tu1 * mw) + 1;
            int y0 = (int)floorf(tv0 * mh) - 1, y1 = (int)floorf(tv1 * mh) + 1;
            if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > mw - 1) x1 = mw - 1; if (y1 > mh - 1) y1 = mh - 1;
            if (x1 < x0 || y1 < y0) continue;
            const int W1 = mw + 1;
            const int n = gSat[(size_t)(y1 + 1) * W1 + x1 + 1] - gSat[(size_t)y0 * W1 + x1 + 1]
                        - gSat[(size_t)(y1 + 1) * W1 + x0] + gSat[(size_t)y0 * W1 + x0];
            if (n <= 0) continue;
            gSub.push_back(g.idx[k]); gSub.push_back(g.idx[k + 1]); gSub.push_back(g.idx[k + 2]);
        }
        if (!gSub.empty()) {
            s.idxVbo = gfx::GenBuffer();
            gfx::IndexBufferData(s.idxVbo, &gSub[0], (int)gSub.size() * (int)sizeof(MeshIndex));
        }
        s.idxN = (int)gSub.size(); s.geoN = g.n;
    }
    // (las texturas de lo que ya no esta: se sueltan cuando se juntan muchas)
    if (gMasks.size() > 64) {
        for (std::map<Mesh*, Mask>::iterator it = gMasks.begin(); it != gMasks.end();) {
            if (!it->second.active) {
                for (int k = 0; k < 3; k++) FreeMaskGPU(it->second.gpu[k]);
                gMasks.erase(it++);
            }
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
    if (it == gMasks.end() || !it->second.active) return;
    const MaskGPU& ms = it->second.gpu[it->second.cur];
    if (!ms.tex) return;
    Geo g; if (!GeoOf(m, g)) return;
    // LA MALLA GENERADA (subdiv...) Y LAS UV DE SIEMPRE DESDE LA GPU: la geometria de sus VBOs (Mesh::UploadGenVBO) y las UV
    // de la mascara (armadas con ella). Sin esto cada cuadro recalculaba las UV y mandaba la malla de RAM otra vez
    const bool genGPU = g.pos == m->genVertex && g.idx == m->genFaces && m->vboGenPos && m->vboGenIdx &&
                        m->vboGenVer == m->genVersion;
    const bool uvGPU = ms.uvVbo && ms.uvN == g.nv;
    const bool subGPU = uvGPU && ms.idxVbo && ms.geoN == g.n;   // (solo los triangulos que tocan lo oscuro)
    if (subGPU && ms.idxN <= 0) return;
    if (!uvGPU) {
        gUV.resize((size_t)g.nv * 2);
        for (int i = 0; i < g.nv; i++) {
            const float* q = &g.pos[i * 3];
            gUV[(size_t)i * 2]     = ms.au[0] * q[0] + ms.au[1] * q[1] + ms.au[2] * q[2] + ms.au[3];
            gUV[(size_t)i * 2 + 1] = ms.av[0] * q[0] + ms.av[1] * q[1] + ms.av[2] * q[2] + ms.av[3];
        }
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
    if (genGPU) gfx::VertexVBO(m->vboGenPos); else gfx::VertexPointer3f(0, g.pos);
    if (uvGPU) gfx::TexCoordVBO(ms.uvVbo); else gfx::TexCoordPointer2f(0, &gUV[0]);
    gfx::Enable(gfx::Blend); gfx::SetMix(gfx::MixMultiply);
    gfx::DepthMask(false); gfx::DepthFunc(gfx::DepthLEqual);
    if (subGPU) { gfx::BindIndexVBO(ms.idxVbo); gfx::DrawTrianglesVBO(ms.idxN, 0); }
    else if (genGPU) { gfx::BindIndexVBO(m->vboGenIdx); gfx::DrawTrianglesVBO(g.n, 0); }
    else gfx::DrawTriangles(g.n, g.idx);
    if (genGPU || uvGPU) gfx::UnbindVBOs();
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
