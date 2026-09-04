// ============================================================================
//  BOOLEAN (CSG) sobre poligonos. Ver BooleanMod.h para el contrato.
//
//  COMO FUNCIONA (segunda version). La primera recortaba cada poligono contra
//  los PLANOS del otro objeto (BSP a la csg.js) y eso fragmentaba de mas: el
//  plano de una cara es infinito, asi que partia caras que ni tocaba, y dejaba
//  vertices sobre los bordes de las caras vecinas (T-junctions) que al mover un
//  vertice se abrian como grietas. El dueno lo vio enseguida en el cubo con el
//  agujero: las paredes tenian que quedar quads intactos y la tapa un solo ngon.
//
//  Ahora cada cara se corta SOLO por la CURVA DE INTERSECCION real con la otra
//  malla: se intersecan cara contra cara (segmento comun de dos poligonos), los
//  segmentos de cada cara se encadenan, y la cara se parte por esas cadenas:
//    - una cadena ABIERTA (cruza la cara de borde a borde) la parte en dos, y
//      el punto donde toca el borde queda como vertice de la cara vecina tambien
//      (esa cara recibe el mismo segmento) -> sin T-junctions;
//    - una cadena CERRADA (un agujero adentro de la cara) se resuelve con DOS
//      cortes a esquinas vecinas: un "cuadrado" y una "U" (ver CortarCerrada),
//      mas la "tapa" del agujero como poligono aparte para que la clasificacion
//      decida su suerte.
//  El BSP queda SOLO para clasificar un punto adentro/afuera de la otra malla.
//  Nada se triangula (salvo un poligono que no es plano, que no puede entrar).
//  C++03 (compila en Symbian): sin auto, sin lambdas, sin nullptr.
// ============================================================================
#include "edit/BooleanMod.h"
#include <map>
#include <string>
#include <algorithm>
#include <math.h>

namespace {

static const float EPS  = 1e-5f;   // "esta sobre el plano" (unidades de escena: metros)
static const float TOL  = 1e-4f;   // coincidencia de puntos / punto sobre un borde
static const float SEP  = 1e-3f;   // corrimiento del centroide para clasificar (1 mm)

struct CV {
    Vector3 p;
    float u, v;
    float c[4];
};
static CV Lerp(const CV& a, const CV& b, float t) {
    CV r;
    r.p = a.p + (b.p - a.p) * t;
    r.u = a.u + (b.u - a.u) * t;
    r.v = a.v + (b.v - a.v) * t;
    for (int k = 0; k < 4; k++) r.c[k] = a.c[k] + (b.c[k] - a.c[k]) * t;
    return r;
}

struct Plane {
    Vector3 n; float w; bool ok;   // n . x = w
    Plane() : w(0.0f), ok(false) {}
};

struct Poly {
    std::vector<CV> v;
    Plane pl;
    int mat;
    int smooth;   // shading por cara (-1 hereda / 0 flat / 1 smooth): viaja intacto por los cortes
    void Flip() {
        std::vector<CV> r(v.rbegin(), v.rend());
        v.swap(r);
        pl.n = -pl.n; pl.w = -pl.w;
    }
    Vector3 Centroide() const {
        Vector3 c(0, 0, 0);
        for (size_t i = 0; i < v.size(); i++) c += v[i].p;
        return v.empty() ? c : c * (1.0f / (float)v.size());
    }
};

// plano por NEWELL: robusto con ngons y con tres primeros vertices casi colineales
static Plane PlanoDe(const std::vector<CV>& v) {
    Plane pl;
    const int n = (int)v.size();
    if (n < 3) return pl;
    Vector3 nn(0, 0, 0), c(0, 0, 0);
    for (int i = 0; i < n; i++) {
        const Vector3& a = v[i].p;
        const Vector3& b = v[(i + 1) % n].p;
        nn.x += (a.y - b.y) * (a.z + b.z);
        nn.y += (a.z - b.z) * (a.x + b.x);
        nn.z += (a.x - b.x) * (a.y + b.y);
        c += a;
    }
    const float len = nn.Length();
    if (len < 1e-12f) return pl;
    pl.n = nn * (1.0f / len);
    pl.w = pl.n.Dot(c * (1.0f / (float)n));
    pl.ok = true;
    return pl;
}

// ---------------------------------------------------------------------------
//  BSP: solo para decir si un PUNTO esta adentro de la malla cerrada
// ---------------------------------------------------------------------------
struct Node {
    Plane pl;
    Node* front;
    Node* back;
    Node() : front(NULL), back(NULL) {}
    ~Node() { delete front; delete back; }
    void Build(const std::vector<Poly>& lista, int prof = 0) {
        if (lista.empty() || prof > 4000) return;
        if (!pl.ok) pl = lista[0].pl;
        std::vector<Poly> f, b;
        for (size_t i = 0; i < lista.size(); i++) {
            const Poly& p = lista[i];
            int tipo = 0;
            std::vector<int> tipos(p.v.size(), 0);
            for (size_t k = 0; k < p.v.size(); k++) {
                const float t = pl.n.Dot(p.v[k].p) - pl.w;
                tipos[k] = (t < -EPS) ? 2 : (t > EPS) ? 1 : 0;
                tipo |= tipos[k];
            }
            if (tipo == 0) continue;               // coplanar: el nodo ya lo representa
            if (tipo == 1) { f.push_back(p); continue; }
            if (tipo == 2) { b.push_back(p); continue; }
            Poly pf, pb; pf.mat = pb.mat = p.mat; pf.smooth = pb.smooth = p.smooth; pf.pl = pb.pl = p.pl;   // cruza: se parte
            const int n = (int)p.v.size();
            for (int i2 = 0; i2 < n; i2++) {
                const int j = (i2 + 1) % n;
                const CV& vi = p.v[(size_t)i2]; const CV& vj = p.v[(size_t)j];
                if (tipos[(size_t)i2] != 2) pf.v.push_back(vi);
                if (tipos[(size_t)i2] != 1) pb.v.push_back(vi);
                if ((tipos[(size_t)i2] | tipos[(size_t)j]) == 3) {
                    const float den = pl.n.Dot(vj.p - vi.p);
                    const float t = (fabsf(den) < 1e-12f) ? 0.5f : (pl.w - pl.n.Dot(vi.p)) / den;
                    const CV nv = Lerp(vi, vj, t);
                    pf.v.push_back(nv); pb.v.push_back(nv);
                }
            }
            if (pf.v.size() >= 3) f.push_back(pf);
            if (pb.v.size() >= 3) b.push_back(pb);
        }
        if (!f.empty()) { if (!front) front = new Node(); front->Build(f, prof + 1); }
        if (!b.empty()) { if (!back)  back  = new Node(); back->Build(b, prof + 1); }
    }
    // true = adentro del solido. Un punto justo sobre un plano se empuja al frente (afuera);
    // los llamadores clasifican centroides CORRIDOS 1 mm, asi que casi nunca pasa.
    bool Adentro(const Vector3& p) const {
        if (!pl.ok) return false;
        const float t = pl.n.Dot(p) - pl.w;
        if (t >= -EPS) return front ? front->Adentro(p) : false;
        return back ? back->Adentro(p) : true;
    }
};

// ---------------------------------------------------------------------------
//  Carga / descarga
// ---------------------------------------------------------------------------
static void Cargar(const PolyMesh& M, int matForzado, std::vector<Poly>& out) {
    for (size_t f = 0; f < M.F.size(); f++) {
        const std::vector<int>& id = M.F[f];
        if (id.size() < 3) continue;
        Poly p; p.mat = (matForzado >= 0) ? matForzado : M.Fmat[f]; p.smooth = PolySmoothDe(M, f);
        for (size_t k = 0; k < id.size(); k++) {
            const int i = id[k];
            if (i < 0 || i >= (int)M.P.size()) { p.v.clear(); break; }
            CV cv; cv.p = M.P[(size_t)i];
            cv.u = (f < M.Fuv.size() && k*2+1 < M.Fuv[f].size()) ? M.Fuv[f][k*2]   : 0.0f;
            cv.v = (f < M.Fuv.size() && k*2+1 < M.Fuv[f].size()) ? M.Fuv[f][k*2+1] : 0.0f;
            for (int q = 0; q < 4; q++)
                cv.c[q] = (f < M.Fcol.size() && k*4+3 < M.Fcol[f].size()) ? (float)M.Fcol[f][k*4+q] : 255.0f;
            p.v.push_back(cv);
        }
        if (p.v.size() < 3) continue;
        p.pl = PlanoDe(p.v);
        if (!p.pl.ok) continue;
        // un poligono que NO es plano (un quad torcido) no se puede cortar por su plano: se
        // abre en abanico ANTES de entrar. Solo esos; los planos (la regla) siguen enteros.
        float desvio = 0.0f;
        for (size_t k = 0; k < p.v.size(); k++) {
            const float d = fabsf(p.pl.n.Dot(p.v[k].p) - p.pl.w);
            if (d > desvio) desvio = d;
        }
        if (desvio <= EPS * 0.5f || p.v.size() == 3) { out.push_back(p); continue; }
        for (size_t k = 1; k + 1 < p.v.size(); k++) {
            Poly t; t.mat = p.mat; t.smooth = p.smooth;
            t.v.push_back(p.v[0]); t.v.push_back(p.v[k]); t.v.push_back(p.v[k+1]);
            t.pl = PlanoDe(t.v);
            if (t.pl.ok) out.push_back(t);
        }
    }
}

// lista de poligonos -> PolyMesh. Posiciones deduplicadas (cuantizadas a 1e-4, como
// SoldarPolyPorPos). Dos corners seguidos en el mismo lugar se funden en uno.
static void Descargar(const std::vector<Poly>& polys, PolyMesh& out) {
    out.P.clear(); out.F.clear(); out.Fmat.clear(); out.Fsmooth.clear(); out.Fuv.clear(); out.Fcol.clear(); out.E.clear();
    std::map<std::string, int> pool;
    for (size_t i = 0; i < polys.size(); i++) {
        const Poly& p = polys[i];
        std::vector<int> face; std::vector<float> uv; std::vector<unsigned char> col;
        for (size_t k = 0; k < p.v.size(); k++) {
            const CV& cv = p.v[k];
            int q[3] = { (int)floorf(cv.p.x*10000.0f+0.5f), (int)floorf(cv.p.y*10000.0f+0.5f), (int)floorf(cv.p.z*10000.0f+0.5f) };
            std::string key((const char*)q, sizeof(q));
            std::map<std::string,int>::iterator it = pool.find(key);
            int idx;
            if (it == pool.end()) { idx = (int)out.P.size(); pool[key] = idx; out.P.push_back(cv.p); }
            else idx = it->second;
            if (!face.empty() && face.back() == idx) continue;    // dos corners consecutivos en el mismo lugar
            face.push_back(idx);
            uv.push_back(cv.u); uv.push_back(cv.v);
            for (int z = 0; z < 4; z++) {
                float c = cv.c[z]; if (c < 0.0f) c = 0.0f; if (c > 255.0f) c = 255.0f;
                col.push_back((unsigned char)(c + 0.5f));
            }
        }
        if (face.size() >= 2 && face.front() == face.back()) {
            face.pop_back(); uv.pop_back(); uv.pop_back(); for (int z = 0; z < 4; z++) col.pop_back();
        }
        if (face.size() < 3) continue;
        out.F.push_back(face); out.Fmat.push_back(p.mat); out.Fsmooth.push_back(p.smooth); out.Fuv.push_back(uv); out.Fcol.push_back(col);
    }
    std::map<std::pair<int,int>, int> eset;
    for (size_t f = 0; f < out.F.size(); f++) {
        const int m = (int)out.F[f].size();
        for (int i = 0; i < m; i++) {
            int a = out.F[f][i], b = out.F[f][(i+1)%m];
            if (a == b) continue;
            std::pair<int,int> e = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
            if (eset.insert(std::make_pair(e, 1)).second) out.E.push_back(e);
        }
    }
}

static void BboxPoly(const Poly& p, Vector3& lo, Vector3& hi) {
    lo = Vector3(1e30f, 1e30f, 1e30f); hi = Vector3(-1e30f, -1e30f, -1e30f);
    for (size_t k = 0; k < p.v.size(); k++) {
        const Vector3& q = p.v[k].p;
        if (q.x < lo.x) lo.x = q.x; if (q.y < lo.y) lo.y = q.y; if (q.z < lo.z) lo.z = q.z;
        if (q.x > hi.x) hi.x = q.x; if (q.y > hi.y) hi.y = q.y; if (q.z > hi.z) hi.z = q.z;
    }
}
static bool CajasSeTocan(const Vector3& lo1, const Vector3& hi1, const Vector3& lo2, const Vector3& hi2) {
    const float m = TOL;
    return !(hi1.x < lo2.x - m || lo1.x > hi2.x + m || hi1.y < lo2.y - m || lo1.y > hi2.y + m || hi1.z < lo2.z - m || lo1.z > hi2.z + m);
}

// ---------------------------------------------------------------------------
//  Geometria plana (dentro del plano de una cara)
// ---------------------------------------------------------------------------
static void Ejes2D(const Vector3& n, Vector3& ex, Vector3& ey) {
    Vector3 a = (fabsf(n.x) < 0.9f) ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
    ex = Vector3::Cross(n, a).Normalized();
    ey = Vector3::Cross(n, ex);
}
static bool DentroDe(const Poly& P, const Vector3& q) {
    Vector3 ex, ey; Ejes2D(P.pl.n, ex, ey);
    const float qx = q.Dot(ex), qy = q.Dot(ey);
    bool dentro = false;
    const int n = (int)P.v.size();
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const float xi = P.v[(size_t)i].p.Dot(ex), yi = P.v[(size_t)i].p.Dot(ey);
        const float xj = P.v[(size_t)j].p.Dot(ex), yj = P.v[(size_t)j].p.Dot(ey);
        if (((yi > qy) != (yj > qy)) && (qx < (xj - xi) * (qy - yi) / (yj - yi + 1e-12f) + xi)) dentro = !dentro;
    }
    return dentro;
}
static bool Cruzan2D(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
    const float d1 = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
    const float d2 = (bx-ax)*(dy-ay) - (by-ay)*(dx-ax);
    const float d3 = (dx-cx)*(ay-cy) - (dy-cy)*(ax-cx);
    const float d4 = (dx-cx)*(by-cy) - (dy-cy)*(bx-cx);
    return ((d1 > 0.0f) != (d2 > 0.0f)) && ((d3 > 0.0f) != (d4 > 0.0f));
}
// uv/color de un punto interior a la cara: promedio de sus vertices pesado por 1/distancia.
// Es aproximado (una cara cortada por un boolean rara vez conserva un mapeo prolijo), pero
// nunca se sale del rango de la cara.
static CV Interior(const Poly& P, const Vector3& q) {
    CV r; r.p = q; r.u = r.v = 0.0f; r.c[0] = r.c[1] = r.c[2] = r.c[3] = 0.0f;
    float sw = 0.0f;
    for (size_t k = 0; k < P.v.size(); k++) {
        const float w = 1.0f / ((P.v[k].p - q).Length() + 1e-6f);
        r.u += P.v[k].u * w; r.v += P.v[k].v * w;
        for (int z = 0; z < 4; z++) r.c[z] += P.v[k].c[z] * w;
        sw += w;
    }
    if (sw > 0.0f) { r.u /= sw; r.v /= sw; for (int z = 0; z < 4; z++) r.c[z] /= sw; }
    return r;
}

// ---------------------------------------------------------------------------
//  Interseccion cara contra cara: los tramos de la recta comun de los dos planos que caen
//  ADENTRO de las dos caras.
// ---------------------------------------------------------------------------
static int SobreBorde(const Poly& P, const Vector3& q, float& s);
static bool DentroOBorde(const Poly& P, const Vector3& q) {
    float s; return SobreBorde(P, q, s) >= 0 || DentroDe(P, q);
}
// tramos [t0,t1] de la recta o + d*t que caen adentro de la cara (o sobre su borde). Se juntan
// TODOS los t candidatos (cruces de arista + vertices apoyados en la recta) y se decide tramo
// por tramo con el punto medio: asi una recta que corre POR una arista (la de la cara vecina,
// caso comun entre cajas) cuenta igual, y sirve para ngons concavos. Devuelve pares.
static void CrucesEnRecta(const Poly& P, const Vector3& o, const Vector3& d, std::vector<float>& ts) {
    const Vector3 m = Vector3::Cross(P.pl.n, d);      // perpendicular a la recta, dentro del plano
    const int n = (int)P.v.size();
    std::vector<float> cand;
    for (int i = 0; i < n; i++) {
        const Vector3& a = P.v[(size_t)i].p;
        const Vector3& b = P.v[(size_t)(i + 1) % n].p;
        const float da = (a - o).Dot(m), db = (b - o).Dot(m);
        const bool ona = fabsf(da) <= TOL, onb = fabsf(db) <= TOL;
        if (ona) cand.push_back((a - o).Dot(d));
        if (onb) cand.push_back((b - o).Dot(d));
        if (!ona && !onb && ((da > 0.0f) != (db > 0.0f))) {
            const float s = da / (da - db);
            cand.push_back(((a + (b - a) * s) - o).Dot(d));
        }
    }
    std::sort(cand.begin(), cand.end());
    std::vector<float> u;
    for (size_t i = 0; i < cand.size(); i++) if (u.empty() || cand[i] - u.back() > TOL) u.push_back(cand[i]);
    ts.clear();
    for (size_t i = 0; i + 1 < u.size(); i++) {
        if (!DentroOBorde(P, o + d * (0.5f * (u[i] + u[i+1])))) continue;
        if (!ts.empty() && fabsf(ts.back() - u[i]) <= TOL) ts.back() = u[i+1];   // pegado al anterior: se alarga
        else { ts.push_back(u[i]); ts.push_back(u[i+1]); }
    }
}
static void SegmentosInterseccion(const Poly& F, const Poly& G, std::vector<std::pair<Vector3,Vector3> >& out) {
    const Vector3 dn = Vector3::Cross(F.pl.n, G.pl.n);
    const float l2 = dn.LengthSq();
    if (l2 < 1e-10f) return;                            // coplanares o paralelas: no hay recta
    // punto de la recta comun: n1.o = w1, n2.o = w2, o.d = 0
    const Vector3 o = (Vector3::Cross(G.pl.n, dn) * F.pl.w + Vector3::Cross(dn, F.pl.n) * G.pl.w) * (1.0f / l2);
    const Vector3 d = dn * (1.0f / sqrtf(l2));
    std::vector<float> tf, tg;
    CrucesEnRecta(F, o, d, tf);
    CrucesEnRecta(G, o, d, tg);
    for (size_t i = 0; i + 1 < tf.size(); i += 2)
        for (size_t j = 0; j + 1 < tg.size(); j += 2) {
            const float a = (tf[i] > tg[j]) ? tf[i] : tg[j];
            const float b = (tf[i+1] < tg[j+1]) ? tf[i+1] : tg[j+1];
            if (b - a > TOL) out.push_back(std::make_pair(o + d * a, o + d * b));
        }
}

// ---------------------------------------------------------------------------
//  Cadenas: los segmentos sueltos de una cara se unen por sus puntas
// ---------------------------------------------------------------------------
struct Cadena { std::vector<Vector3> pts; bool cerrada; Cadena() : cerrada(false) {} };
static bool Mismo(const Vector3& a, const Vector3& b) { return (a - b).LengthSq() < TOL * TOL; }
static void Encadenar(std::vector<std::pair<Vector3,Vector3> > segs, std::vector<Cadena>& out) {
    while (!segs.empty()) {
        Cadena c; c.pts.push_back(segs.back().first); c.pts.push_back(segs.back().second); segs.pop_back();
        bool crecio = true;
        while (crecio) {
            crecio = false;
            for (size_t i = 0; i < segs.size(); i++) {
                const Vector3& a = segs[i].first; const Vector3& b = segs[i].second;
                if      (Mismo(c.pts.back(), a))  { c.pts.push_back(b); }
                else if (Mismo(c.pts.back(), b))  { c.pts.push_back(a); }
                else if (Mismo(c.pts.front(), a)) { c.pts.insert(c.pts.begin(), b); }
                else if (Mismo(c.pts.front(), b)) { c.pts.insert(c.pts.begin(), a); }
                else continue;
                segs.erase(segs.begin() + i); crecio = true; break;
            }
        }
        if (c.pts.size() >= 4 && Mismo(c.pts.front(), c.pts.back())) { c.pts.pop_back(); c.cerrada = true; }
        if (c.pts.size() >= 2) out.push_back(c);
    }
}

// ---------------------------------------------------------------------------
//  Cortar una cara por sus cadenas
// ---------------------------------------------------------------------------
static int SobreBorde(const Poly& P, const Vector3& q, float& s) {
    const int n = (int)P.v.size();
    for (int i = 0; i < n; i++) {
        const Vector3& a = P.v[(size_t)i].p; const Vector3& b = P.v[(size_t)(i + 1) % n].p;
        const Vector3 ab = b - a; const float L2 = ab.LengthSq(); if (L2 < 1e-12f) continue;
        float t = (q - a).Dot(ab) / L2; if (t < -1e-3f || t > 1.0f + 1e-3f) continue;
        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
        if (((a + ab * t) - q).LengthSq() < TOL * TOL) { s = t; return i; }
    }
    return -1;
}
static int Insertar(Poly& P, int e, float s, const Vector3& q, bool& inserto) {
    const int n = (int)P.v.size();
    inserto = false;
    if (s < 1e-4f) return e;                       // ya es un vertice de la cara
    if (s > 1.0f - 1e-4f) return (e + 1) % n;
    CV nv = Lerp(P.v[(size_t)e], P.v[(size_t)(e + 1) % n], s); nv.p = q;
    P.v.insert(P.v.begin() + e + 1, nv);
    inserto = true;
    return e + 1;
}
// el segmento a-b esta apoyado sobre una arista de P? Un tramo de la curva de interseccion
// que corre por el borde de la cara no la corta (es el borde de la cara vecina): se ignora.
static bool SobreArista(const Poly& P, const Vector3& a, const Vector3& b) {
    const int n = (int)P.v.size();
    for (int i = 0; i < n; i++) {
        const Vector3& p = P.v[(size_t)i].p; const Vector3& q = P.v[(size_t)(i + 1) % n].p;
        const Vector3 pq = q - p; const float L2 = pq.LengthSq(); if (L2 < 1e-12f) continue;
        float ta = (a - p).Dot(pq) / L2, tb = (b - p).Dot(pq) / L2;
        if (ta < -1e-3f || ta > 1.001f || tb < -1e-3f || tb > 1.001f) continue;
        if (((p + pq * ta) - a).LengthSq() > TOL * TOL) continue;
        if (((p + pq * tb) - b).LengthSq() > TOL * TOL) continue;
        return true;
    }
    return false;
}
// un punto SEGURO adentro del poligono (no el centroide: en un ojo de cerradura el centroide
// cae en el agujero). Es el centro de la "oreja" mas grande: un vertice convexo cuyo
// triangulo con sus vecinos no contiene otro vertice.
static Vector3 PuntoInterior(const Poly& P) {
    const int n = (int)P.v.size();
    if (n <= 3) return P.Centroide();
    Vector3 ex, ey; Ejes2D(P.pl.n, ex, ey);
    std::vector<float> x((size_t)n), y((size_t)n);
    float area = 0.0f;
    for (int i = 0; i < n; i++) { x[(size_t)i] = P.v[(size_t)i].p.Dot(ex); y[(size_t)i] = P.v[(size_t)i].p.Dot(ey); }
    for (int i = 0; i < n; i++) { const int j = (i + 1) % n; area += x[(size_t)i] * y[(size_t)j] - x[(size_t)j] * y[(size_t)i]; }
    const float signo = (area < 0.0f) ? -1.0f : 1.0f;
    int mejor = -1; float mejorArea = 0.0f;
    for (int i = 0; i < n; i++) {
        const int a = (i + n - 1) % n, c = (i + 1) % n;
        const float ax = x[(size_t)a], ay = y[(size_t)a], bx = x[(size_t)i], by = y[(size_t)i], cx = x[(size_t)c], cy = y[(size_t)c];
        const float cr = ((bx - ax) * (cy - ay) - (by - ay) * (cx - ax)) * signo;
        if (cr <= 1e-10f || cr <= mejorArea) continue;    // concavo, degenerado, o mas chico que el mejor
        bool libre = true;
        for (int k = 0; k < n && libre; k++) {
            if (k == a || k == i || k == c) continue;
            const float px = x[(size_t)k], py = y[(size_t)k];
            if ((fabsf(px - ax) < TOL && fabsf(py - ay) < TOL) || (fabsf(px - bx) < TOL && fabsf(py - by) < TOL) ||
                (fabsf(px - cx) < TOL && fabsf(py - cy) < TOL)) continue;   // el mismo lugar que una esquina
            const float d1 = ((bx - ax) * (py - ay) - (by - ay) * (px - ax)) * signo;
            const float d2 = ((cx - bx) * (py - by) - (cy - by) * (px - bx)) * signo;
            const float d3 = ((ax - cx) * (py - cy) - (ay - cy) * (px - cx)) * signo;
            if (d1 > 0.0f && d2 > 0.0f && d3 > 0.0f) libre = false;
        }
        if (libre) { mejor = i; mejorArea = cr; }
    }
    if (mejor < 0) return P.Centroide();
    const int a = (mejor + n - 1) % n, c = (mejor + 1) % n;
    return (P.v[(size_t)a].p + P.v[(size_t)mejor].p + P.v[(size_t)c].p) * (1.0f / 3.0f);
}
static bool CortarAbierta(const Poly& P, const Cadena& c, Poly& p1, Poly& p2) {
    Poly W = P;
    float s0, s1;
    int e0 = SobreBorde(W, c.pts.front(), s0); if (e0 < 0) return false;
    bool ins0, ins1;
    int i0 = Insertar(W, e0, s0, c.pts.front(), ins0);
    int e1 = SobreBorde(W, c.pts.back(), s1);  if (e1 < 0) return false;
    int i1 = Insertar(W, e1, s1, c.pts.back(), ins1);
    if (ins1 && i1 <= i0) i0++;                   // insertar ANTES de i0 lo corrio un lugar
    const int n = (int)W.v.size();
    std::vector<CV> in;
    for (size_t k = 1; k + 1 < c.pts.size(); k++) in.push_back(Interior(P, c.pts[k]));
    p1.mat = p2.mat = P.mat; p1.smooth = p2.smooth = P.smooth; p1.pl = p2.pl = P.pl; p1.v.clear(); p2.v.clear();
    if (i0 == i1) {
        // entra y sale por el MISMO punto: un agujero pegado al borde. Ojo de cerradura con
        // puente de largo cero (el vertice se repite) + la tapa del agujero.
        if (in.size() < 2) return false;
        p1.v.push_back(W.v[(size_t)i0]);
        for (size_t k = 0; k < in.size(); k++) p1.v.push_back(in[k]);
        for (int k = 0; k <= n; k++) p2.v.push_back(W.v[(size_t)((i0 + k) % n)]);
        for (int k = (int)in.size() - 1; k >= 0; k--) p2.v.push_back(in[(size_t)k]);
        return true;
    }
    for (int k = i0; ; k = (k + 1) % n) { p1.v.push_back(W.v[(size_t)k]); if (k == i1) break; }
    for (int k = (int)in.size() - 1; k >= 0; k--) p1.v.push_back(in[(size_t)k]);
    for (int k = i1; ; k = (k + 1) % n) { p2.v.push_back(W.v[(size_t)k]); if (k == i0) break; }
    for (size_t k = 0; k < in.size(); k++) p2.v.push_back(in[k]);
    return p1.v.size() >= 3 && p2.v.size() >= 3;
}
// un puente p->q (en 2D) que no cruce ninguna arista de las listas (salvo las que tocan sus puntas)
static bool PuenteLibre(float px, float py, float qx, float qy,
                        const std::vector<float>& ox, const std::vector<float>& oy, int i,
                        const std::vector<float>& hx, const std::vector<float>& hy, int j) {
    const int n = (int)ox.size(), m = (int)hx.size();
    for (int k = 0; k < n; k++) {
        if (k == i || (k + 1) % n == i) continue;
        if (Cruzan2D(px, py, qx, qy, ox[(size_t)k], oy[(size_t)k], ox[(size_t)((k + 1) % n)], oy[(size_t)((k + 1) % n)])) return false;
    }
    for (int k = 0; k < m; k++) {
        if (k == j || (k + 1) % m == j) continue;
        if (Cruzan2D(px, py, qx, qy, hx[(size_t)k], hy[(size_t)k], hx[(size_t)((k + 1) % m)], hy[(size_t)((k + 1) % m)])) return false;
    }
    return true;
}
// Agujero adentro de una cara: DOS cortes, de dos esquinas VECINAS de la cara a dos vertices
// vecinos del agujero. Salen dos poligonos simples (sin vertices repetidos): un "cuadrado"
// (esquina, esquina, vertice, vertice) y una "U" con TODO el resto (todas las esquinas + todos
// los vertices del agujero: 4 + 8 = 12 en el cubo con el cilindro). Mas la "tapa" del agujero
// aparte, para que la clasificacion decida si se queda. Lo propuso el dueno en vez del ojo de
// cerradura (una "O" con una linea): un vertice repetido en la misma cara rompia la
// triangulacion y las aristas al editar.
static bool CortarCerrada(const Poly& P, const Cadena& c, Poly& U, Poly& quad, Poly& tapa) {
    if (c.pts.size() < 3) return false;
    Vector3 ex, ey; Ejes2D(P.pl.n, ex, ey);
    float area = 0.0f;
    for (size_t k = 0; k < c.pts.size(); k++) {
        const Vector3& a = c.pts[k]; const Vector3& b = c.pts[(k + 1) % c.pts.size()];
        area += a.Dot(ex) * b.Dot(ey) - b.Dot(ex) * a.Dot(ey);
    }
    std::vector<Vector3> ccw = c.pts;
    if (area < 0.0f) { std::vector<Vector3> r(ccw.rbegin(), ccw.rend()); ccw.swap(r); }
    const int n = (int)P.v.size(), m = (int)ccw.size();
    std::vector<float> ox((size_t)n), oy((size_t)n), hx((size_t)m), hy((size_t)m);
    for (int i = 0; i < n; i++) { ox[(size_t)i] = P.v[(size_t)i].p.Dot(ex); oy[(size_t)i] = P.v[(size_t)i].p.Dot(ey); }
    for (int j = 0; j < m; j++) { hx[(size_t)j] = ccw[(size_t)j].Dot(ex); hy[(size_t)j] = ccw[(size_t)j].Dot(ey); }
    // el par de puentes mas corto que no cruce nada: (i -> j) y (i+1 -> j+1), los dos avanzando
    // en el mismo sentido (las dos vueltas son CCW) asi el cuadrado no se cruza
    int mi = -1, mj = -1; float md = 1e30f;
    for (int i = 0; i < n; i++) for (int j = 0; j < m; j++) {
        const int i2 = (i + 1) % n, j2 = (j + 1) % m;
        const float d1 = (P.v[(size_t)i].p - ccw[(size_t)j]).LengthSq();
        const float d2 = (P.v[(size_t)i2].p - ccw[(size_t)j2]).LengthSq();
        if (d1 + d2 >= md) continue;
        if (!PuenteLibre(ox[(size_t)i], oy[(size_t)i], hx[(size_t)j], hy[(size_t)j], ox, oy, i, hx, hy, j)) continue;
        if (!PuenteLibre(ox[(size_t)i2], oy[(size_t)i2], hx[(size_t)j2], hy[(size_t)j2], ox, oy, i2, hx, hy, j2)) continue;
        if (Cruzan2D(ox[(size_t)i], oy[(size_t)i], hx[(size_t)j], hy[(size_t)j], ox[(size_t)i2], oy[(size_t)i2], hx[(size_t)j2], hy[(size_t)j2])) continue;
        md = d1 + d2; mi = i; mj = j;
    }
    if (mi < 0) return false;
    const int mi2 = (mi + 1) % n, mj2 = (mj + 1) % m;
    quad.mat = U.mat = tapa.mat = P.mat; quad.smooth = U.smooth = tapa.smooth = P.smooth; quad.pl = U.pl = tapa.pl = P.pl;
    quad.v.clear(); U.v.clear(); tapa.v.clear();
    quad.v.push_back(P.v[(size_t)mi]); quad.v.push_back(P.v[(size_t)mi2]);
    quad.v.push_back(Interior(P, ccw[(size_t)mj2])); quad.v.push_back(Interior(P, ccw[(size_t)mj]));
    for (int k = 0; k < n; k++) U.v.push_back(P.v[(size_t)((mi2 + k) % n)]);              // borde: de mi2 hasta mi
    for (int k = 0; k < m; k++) U.v.push_back(Interior(P, ccw[(size_t)(((mj - k) % m + m) % m)]));   // agujero al reves: de mj hasta mj2
    for (int k = 0; k < m; k++) tapa.v.push_back(Interior(P, ccw[(size_t)k]));
    return true;
}
static void Cortar(const Poly& P, const std::vector<std::pair<Vector3,Vector3> >& segsTodos, std::vector<Poly>& out) {
    std::vector<std::pair<Vector3,Vector3> > segs;
    for (size_t k = 0; k < segsTodos.size(); k++)
        if (!SobreArista(P, segsTodos[k].first, segsTodos[k].second)) segs.push_back(segsTodos[k]);
    if (segs.empty()) { out.push_back(P); return; }
    std::vector<Cadena> cadenas; Encadenar(segs, cadenas);
    // un lazo cerrado que TOCA el borde no es un agujero: se abre en ese punto y se corta
    // como cadena abierta (que entra y sale por el mismo lugar)
    for (size_t c = 0; c < cadenas.size(); c++) {
        if (!cadenas[c].cerrada) continue;
        int toca = -1; float st;
        for (size_t k = 0; k < cadenas[c].pts.size() && toca < 0; k++)
            if (SobreBorde(P, cadenas[c].pts[k], st) >= 0) toca = (int)k;
        if (toca < 0) continue;
        std::vector<Vector3> ab;
        const size_t m = cadenas[c].pts.size();
        for (size_t k = 0; k <= m; k++) ab.push_back(cadenas[c].pts[((size_t)toca + k) % m]);
        cadenas[c].pts.swap(ab); cadenas[c].cerrada = false;
    }
    std::vector<Poly> piezas(1, P);
    for (size_t c = 0; c < cadenas.size(); c++) {
        if (cadenas[c].cerrada) continue;
        for (size_t i = 0; i < piezas.size(); i++) {
            Poly p1, p2;
            if (CortarAbierta(piezas[i], cadenas[c], p1, p2)) { piezas[i] = p1; piezas.push_back(p2); break; }
        }
    }
    for (size_t c = 0; c < cadenas.size(); c++) {
        if (!cadenas[c].cerrada) continue;
        int mejor = -1, mejorVotos = 0;                  // la pieza que contiene MAS puntos del lazo
        for (size_t i = 0; i < piezas.size(); i++) {
            int votos = 0;
            for (size_t k = 0; k < cadenas[c].pts.size(); k++) if (DentroDe(piezas[i], cadenas[c].pts[k])) votos++;
            if (votos > mejorVotos) { mejorVotos = votos; mejor = (int)i; }
        }
        if (mejor < 0) continue;
        Poly U, quad, tapa;
        if (CortarCerrada(piezas[(size_t)mejor], cadenas[c], U, quad, tapa)) { piezas[(size_t)mejor] = U; piezas.push_back(quad); piezas.push_back(tapa); }
    }
    out.insert(out.end(), piezas.begin(), piezas.end());
}

// ---------------------------------------------------------------------------
//  Clasificacion de un pedazo contra la OTRA malla
// ---------------------------------------------------------------------------
enum { AFUERA = 0, ADENTRO = 1, COPL_MISMO = 2, COPL_OPUESTO = 3 };
static int Lado(const Poly& p, const Node& otro) {
    const Vector3 c = PuntoInterior(p);
    const bool masAdentro = otro.Adentro(c - p.pl.n * SEP);
    const bool masAfuera  = otro.Adentro(c + p.pl.n * SEP);
    if (masAdentro && masAfuera)   return ADENTRO;
    if (!masAdentro && !masAfuera) return AFUERA;
    return masAdentro ? COPL_MISMO : COPL_OPUESTO;
}

} // namespace

void BooleanPoly(PolyMesh& A, const PolyMesh& B, int op, int matB) {
    std::vector<Poly> pa, pb;
    Cargar(A, -1, pa);
    Cargar(B, matB, pb);
    if (pa.empty() || pb.empty()) return;

    Node ta, tb;
    ta.Build(pa);
    tb.Build(pb);

    std::vector<std::vector<std::pair<Vector3,Vector3> > > segA(pa.size()), segB(pb.size());
    std::vector<Vector3> loA(pa.size()), hiA(pa.size()), loB(pb.size()), hiB(pb.size());
    for (size_t i = 0; i < pa.size(); i++) BboxPoly(pa[i], loA[i], hiA[i]);
    for (size_t j = 0; j < pb.size(); j++) BboxPoly(pb[j], loB[j], hiB[j]);
    for (size_t i = 0; i < pa.size(); i++)
        for (size_t j = 0; j < pb.size(); j++) {
            if (!CajasSeTocan(loA[i], hiA[i], loB[j], hiB[j])) continue;
            std::vector<std::pair<Vector3,Vector3> > s;
            SegmentosInterseccion(pa[i], pb[j], s);
            for (size_t k = 0; k < s.size(); k++) { segA[i].push_back(s[k]); segB[j].push_back(s[k]); }
        }

    std::vector<Poly> piezasA, piezasB;
    for (size_t i = 0; i < pa.size(); i++) Cortar(pa[i], segA[i], piezasA);
    for (size_t j = 0; j < pb.size(); j++) Cortar(pb[j], segB[j], piezasB);

    // que pedazo sobrevive, segun la operacion (la misma tabla que da csg.js con sus invert):
    //   union:      A afuera de B + B afuera de A;   coplanar mismo sentido -> queda el de A
    //   difference: A afuera de B + B ADENTRO de A dado vuelta;   A coplanar opuesto queda
    //   intersect:  A adentro de B + B adentro de A;  coplanar mismo sentido -> queda el de A
    std::vector<Poly> salida;
    for (size_t i = 0; i < piezasA.size(); i++) {
        const int l = Lado(piezasA[i], tb);
        bool queda;
        if (op == BoolOp::Union)           queda = (l == AFUERA  || l == COPL_MISMO);
        else if (op == BoolOp::Difference) queda = (l == AFUERA  || l == COPL_OPUESTO);
        else                               queda = (l == ADENTRO || l == COPL_MISMO);
        if (queda) salida.push_back(piezasA[i]);
    }
    for (size_t j = 0; j < piezasB.size(); j++) {
        const int l = Lado(piezasB[j], ta);
        const bool queda = (op == BoolOp::Union) ? (l == AFUERA) : (l == ADENTRO);
        if (!queda) continue;
        Poly p = piezasB[j];
        if (op == BoolOp::Difference) p.Flip();   // las caras de B miran hacia el hueco
        salida.push_back(p);
    }
    Descargar(salida, A);
}
