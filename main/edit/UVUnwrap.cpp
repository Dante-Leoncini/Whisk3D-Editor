// ============================================================================
//  UVUnwrap.cpp - desplegado de UVs (menu U de Edit Mode) sobre las caras seleccionadas.
//
//  Todo escrito de cero para Whisk3D (C++03, sin dependencias):
//   - Unwrap: la seleccion se corta en ISLAS por las costuras (seamEdges). Cada isla se aplana
//     con un mapeo CONFORME por minimos cuadrados: cada triangulo quiere ser una semejanza de
//     su forma 3D (dos vertices fijos para quitar la libertad de escala/rotacion; el sistema
//     disperso se resuelve por gradientes conjugados sobre las ecuaciones normales). Despues,
//     segun el metodo, se relaja vertice por vertice con busqueda lineal:
//        angular  -> minimiza la diferencia entre los angulos UV y los angulos 3D,
//        minimo estiramiento -> minimiza la distorsion isometrica (estirar y comprimir cuestan
//                               igual: energia de Dirichlet simetrica, minima en la isometria)
//     y al final las islas se giran a su caja minima y se EMPAQUETAN en el cuadrado [0,1].
//   - Smart UV Project: agrupa las caras por normal (limite de angulo), proyecta cada grupo
//     sobre su plano y empaqueta las islas resultantes.
//   - Lightmap Pack: cada cara en su propia celda de una grilla.
//   - Follow Active Quads: propaga la grilla UV del quad activo a los quads vecinos.
//   - Reset: cada cara ocupa toda la textura.
//
//  Convencion de V: el engine dibuja V=0 ARRIBA de la imagen (como las proyecciones de
//  ProyectarUVCaras). Los algoritmos trabajan con V hacia arriba (asi "bien orientado" es
//  determinante positivo) y al escribir se voltea V (v = 1 - v).
// ============================================================================
#include "edit/UVUnwrap.h"
#include "edit/MeshEdit.h"      // W3dTriangularCara (ngons)
#include "objects/Mesh.h"
#include "objects/EditMesh.h"
#include "objects/Objects.h"    // EditSelectMode / SelFace
#include "Undo.h"               // UndoCapturarMallaGeo
#include <math.h>
#include <map>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>

namespace {

// ---------------------------------------------------------------------------
//  La malla vista por el desplegado: caras de faces3d, vertices UNICOS por posicion (posRep),
//  corners numerados en el orden de las capas por-corner (ini[f] + c) y la seleccion de caras.
// ---------------------------------------------------------------------------
struct UVMalla {
    Mesh* m;
    const GLfloat* P;                 // posiciones de reposo por render vert
    int nC;                           // corners totales
    std::vector<int> ini;             // ini[f] = primer corner de faces3d[f]
    std::vector<unsigned char> sel;   // faces3d[f] seleccionada
    std::vector<int> rep;             // render vert -> vertice unico
    int activa;                       // faces3d activa (modo cara) o -1
    int nSel;
};

static bool Preparar(Mesh* m, UVMalla& M, std::string& msg, const char* op) {
    msg = std::string(op) + ": select faces first";
    if (!m) return false;
    m->EnsureEdit();
    if (!m->edit || !m->vertex || m->vertexSize <= 0) return false;
    M.m = m;
    M.P = m->PosicionesReposo(); if (!M.P) M.P = m->vertex;
    // vertice unico por POSICION con tolerancia (posRep agrupa por bytes exactos: los polos de la esfera
    // difieren en un redondeo y quedaban como vertices distintos -> la isla se abria en el polo)
    const int nV = m->vertexSize;
    const bool hayRep = ((int)m->posRep.size() == nV);
    M.rep.resize((size_t)nV);
    std::map<std::pair<long, std::pair<long,long> >, int> porPos;
    for (int i = 0; i < nV; i++) {
        const int r = hayRep ? m->posRep[(size_t)i] : i;
        const float* p = &M.P[(size_t)r * 3];
        std::pair<long, std::pair<long,long> > key((long)floor(p[0] * 1e5 + 0.5), std::make_pair((long)floor(p[1] * 1e5 + 0.5), (long)floor(p[2] * 1e5 + 0.5)));
        std::map<std::pair<long, std::pair<long,long> >, int>::iterator it = porPos.find(key);
        if (it == porPos.end()) { porPos[key] = r; M.rep[(size_t)i] = r; } else M.rep[(size_t)i] = it->second;
    }
    EditMesh* e = m->edit;
    M.sel.assign(m->faces3d.size(), 0); M.nSel = 0;
    for (size_t f = 0; f < e->faceSel.size() && f < e->faceSrc.size(); f++) {
        if (!e->faceSel[f]) continue;
        const int f3 = e->faceSrc[f];
        if (f3 >= 0 && f3 < (int)m->faces3d.size() && !M.sel[(size_t)f3]) { M.sel[(size_t)f3] = 1; M.nSel++; }
    }
    M.activa = -1;
    if (EditSelectMode == SelFace && e->activeIdx >= 0 && e->activeIdx < (int)e->faceSrc.size()) M.activa = e->faceSrc[(size_t)e->activeIdx];
    M.ini.resize(m->faces3d.size()); int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++) { M.ini[f] = L; L += (int)m->faces3d[f].idx.size(); }
    M.nC = L;
    if (M.nSel == 0) return false;
    msg.clear();
    return true;
}

static inline const float* Pos(const UVMalla& M, int rv) { return &M.P[(size_t)rv * 3]; }
static inline int Rep(const UVMalla& M, int rv) { return M.rep[(size_t)rv]; }
static inline double Dist3(const float* a, const float* b) { const double dx = a[0]-b[0], dy = a[1]-b[1], dz = a[2]-b[2]; return sqrt(dx*dx + dy*dy + dz*dz); }
static bool EsCostura(const UVMalla& M, int rva, int rvb) {
    if (M.m->seamEdges.empty()) return false;
    return M.m->seamEdges.count(Mesh::SharpEdgeKey(Pos(M, rva), Pos(M, rvb))) > 0;
}

// union-find chico (corners, caras)
struct UF {
    std::vector<int> p;
    void Init(int n) { p.resize((size_t)n); for (int i = 0; i < n; i++) p[(size_t)i] = i; }
    int Find(int a) { while (p[(size_t)a] != a) { p[(size_t)a] = p[(size_t)p[(size_t)a]]; a = p[(size_t)a]; } return a; }
    void Unir(int a, int b) { a = Find(a); b = Find(b); if (a != b) p[(size_t)a] = b; }
};

// lados de las caras seleccionadas, por par de vertices unicos (min,max)
struct Lado { int f, c; };   // cara y corner de salida (el lado va del corner c al c+1)
typedef std::map<std::pair<int,int>, std::vector<Lado> > TablaLados;
static void ArmarLados(const UVMalla& M, TablaLados& T) {
    for (size_t f = 0; f < M.m->faces3d.size(); f++) {
        if (!M.sel[f]) continue;
        const std::vector<int>& idx = M.m->faces3d[f].idx; const int cnt = (int)idx.size();
        for (int c = 0; c < cnt; c++) {
            const int a = Rep(M, idx[(size_t)c]), b = Rep(M, idx[(size_t)((c + 1) % cnt)]);
            if (a == b) continue;
            Lado l; l.f = (int)f; l.c = c;
            T[std::make_pair(a < b ? a : b, a < b ? b : a)].push_back(l);
        }
    }
}
// corner (global) de la cara del lado que tiene el vertice unico v
static int CornerDe(const UVMalla& M, const Lado& l, int v) {
    const std::vector<int>& idx = M.m->faces3d[(size_t)l.f].idx; const int cnt = (int)idx.size();
    return (Rep(M, idx[(size_t)l.c]) == v) ? M.ini[(size_t)l.f] + l.c : M.ini[(size_t)l.f] + (l.c + 1) % cnt;
}

// uvDe[corner] = uv vert (numerado denso) o -1 si la cara no esta seleccionada. Dos corners son el
// MISMO uv vert cuando comparten vertice a traves de un lado compartido que no es costura (y, si
// hay 'grupo', entre caras del mismo grupo). Una costura que muere en el medio de una isla no
// parte nada: los corners se juntan dando la vuelta por el otro lado.
static int AsignarUVVerts(const UVMalla& M, const TablaLados& T, bool costuras, const std::vector<int>* grupo, std::vector<int>& uvDe) {
    UF uf; uf.Init(M.nC);
    for (TablaLados::const_iterator it = T.begin(); it != T.end(); ++it) {
        const std::vector<Lado>& ls = it->second;
        if (ls.size() < 2) continue;
        const int va = it->first.first, vb = it->first.second;
        {   const std::vector<int>& idx = M.m->faces3d[(size_t)ls[0].f].idx; const int cnt = (int)idx.size();
            if (costuras && EsCostura(M, idx[(size_t)ls[0].c], idx[(size_t)((ls[0].c + 1) % cnt)])) continue; }
        for (size_t i = 0; i < ls.size(); i++) for (size_t j = i + 1; j < ls.size(); j++) {
            if (grupo && (*grupo)[(size_t)ls[i].f] != (*grupo)[(size_t)ls[j].f]) continue;
            uf.Unir(CornerDe(M, ls[i], va), CornerDe(M, ls[j], va));
            uf.Unir(CornerDe(M, ls[i], vb), CornerDe(M, ls[j], vb));
        }
    }
    // dos corners de la MISMA cara sobre el mismo vertice (el polo de la esfera: un quad con dos esquinas
    // en el mismo punto) son el mismo uv vert
    for (size_t f = 0; f < M.m->faces3d.size(); f++) {
        if (!M.sel[f]) continue;
        const std::vector<int>& idx = M.m->faces3d[f].idx; const int cnt = (int)idx.size();
        for (int c1 = 0; c1 < cnt; c1++) for (int c2 = c1 + 1; c2 < cnt; c2++)
            if (Rep(M, idx[(size_t)c1]) == Rep(M, idx[(size_t)c2])) uf.Unir(M.ini[f] + c1, M.ini[f] + c2);
    }
    uvDe.assign((size_t)M.nC, -1);
    std::vector<int> id((size_t)M.nC, -1); int n = 0;
    for (size_t f = 0; f < M.m->faces3d.size(); f++) {
        if (!M.sel[f]) continue;
        const int cnt = (int)M.m->faces3d[f].idx.size();
        for (int c = 0; c < cnt; c++) {
            const int k = M.ini[f] + c, r = uf.Find(k);
            if (id[(size_t)r] < 0) id[(size_t)r] = n++;
            uvDe[(size_t)k] = id[(size_t)r];
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
//  Cartas (islas): caras que comparten algun uv vert. Cada carta tiene sus corners, el uv vert
//  local de cada corner, la posicion 3D de cada uv vert local y sus coordenadas uv.
// ---------------------------------------------------------------------------
struct Carta {
    std::vector<int> caras;       // faces3d
    std::vector<int> corners;     // corners globales
    std::vector<int> local;       // local[i] = uv vert local del corner corners[i]
    std::vector<double> p3;       // 3 por uv vert local (posicion 3D)
    std::vector<double> uv;       // 2 por uv vert local
    int nLoc;
    Carta() : nLoc(0) {}
};

static void ArmarCartas(const UVMalla& M, const std::vector<int>& uvDe, int nUV, std::vector<Carta>& cartas) {
    const int nF = (int)M.m->faces3d.size();
    UF uf; uf.Init(nF);
    std::vector<int> primera((size_t)nUV, -1);
    for (int f = 0; f < nF; f++) {
        if (!M.sel[(size_t)f]) continue;
        const int cnt = (int)M.m->faces3d[(size_t)f].idx.size();
        for (int c = 0; c < cnt; c++) {
            const int u = uvDe[(size_t)(M.ini[(size_t)f] + c)];
            if (primera[(size_t)u] < 0) primera[(size_t)u] = f; else uf.Unir(f, primera[(size_t)u]);
        }
    }
    std::map<int,int> raizACarta;
    for (int f = 0; f < nF; f++) {
        if (!M.sel[(size_t)f]) continue;
        const int r = uf.Find(f);
        std::map<int,int>::iterator it = raizACarta.find(r);
        if (it == raizACarta.end()) { raizACarta[r] = (int)cartas.size(); cartas.push_back(Carta()); it = raizACarta.find(r); }
        cartas[(size_t)it->second].caras.push_back(f);
    }
    std::vector<int> loc((size_t)nUV, -1);
    for (size_t ci = 0; ci < cartas.size(); ci++) {
        Carta& C = cartas[ci];
        std::vector<int> usados;
        for (size_t i = 0; i < C.caras.size(); i++) {
            const int f = C.caras[i];
            const std::vector<int>& idx = M.m->faces3d[(size_t)f].idx; const int cnt = (int)idx.size();
            for (int c = 0; c < cnt; c++) {
                const int k = M.ini[(size_t)f] + c, u = uvDe[(size_t)k];
                if (loc[(size_t)u] < 0) {
                    loc[(size_t)u] = C.nLoc++; usados.push_back(u);
                    const float* p = Pos(M, idx[(size_t)c]);
                    C.p3.push_back(p[0]); C.p3.push_back(p[1]); C.p3.push_back(p[2]);
                }
                C.corners.push_back(k); C.local.push_back(loc[(size_t)u]);
            }
        }
        for (size_t i = 0; i < usados.size(); i++) loc[(size_t)usados[i]] = -1;
        C.uv.assign((size_t)C.nLoc * 2, 0.0);
    }
}

// triangulos de la cara f como TERNAS DE CORNERS globales (abanico; ear clipping si es concava)
static void TriangulosCara(const UVMalla& M, int f, std::vector<int>& out) {
    const std::vector<int>& idx = M.m->faces3d[(size_t)f].idx; const int cnt = (int)idx.size(); const int L = M.ini[(size_t)f];
    out.clear();
    if (cnt < 3) return;
    if (cnt == 3) { out.push_back(L); out.push_back(L + 1); out.push_back(L + 2); return; }
    if (cnt == 4) {   // quad: por la diagonal mas corta (los no planos se aplanan mejor)
        const double d02 = Dist3(Pos(M, idx[0]), Pos(M, idx[2])), d13 = Dist3(Pos(M, idx[1]), Pos(M, idx[3]));
        if (d02 <= d13) { out.push_back(L); out.push_back(L+1); out.push_back(L+2); out.push_back(L); out.push_back(L+2); out.push_back(L+3); }
        else            { out.push_back(L); out.push_back(L+1); out.push_back(L+3); out.push_back(L+1); out.push_back(L+2); out.push_back(L+3); }
        return;
    }
    std::vector<MeshIndex> tris; W3dTriangularCara(M.P, idx, tris);
    for (size_t t = 0; t + 2 < tris.size(); t += 3) {
        int cs[3];
        for (int i = 0; i < 3; i++) {
            cs[i] = -1;
            for (int c = 0; c < cnt; c++) if (idx[(size_t)c] == (int)tris[t + (size_t)i]) { cs[i] = L + c; break; }
        }
        if (cs[0] < 0 || cs[1] < 0 || cs[2] < 0) continue;
        out.push_back(cs[0]); out.push_back(cs[1]); out.push_back(cs[2]);
    }
}

// ---------------------------------------------------------------------------
//  Triangulo aplanado: sus 3 uv verts locales, sus coordenadas 2D isometricas (p0 en el origen,
//  p1 sobre +x, p2 con y > 0: marco derecho respecto de la normal de la cara), area, angulos 3D
//  y los gradientes de la funcion "sombrero" de cada vertice (para el jacobiano).
// ---------------------------------------------------------------------------
struct TriPlano { int v[3]; double x[3], y[3], A, ang[3], gx[3], gy[3]; };

static bool ArmarTriPlano(const double* p0, const double* p1, const double* p2, TriPlano& t) {
    const double e1[3] = { p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2] }, e2[3] = { p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2] };
    const double l1 = sqrt(e1[0]*e1[0] + e1[1]*e1[1] + e1[2]*e1[2]);
    const double l2 = sqrt(e2[0]*e2[0] + e2[1]*e2[1] + e2[2]*e2[2]);
    if (l1 < 1e-9 || l2 < 1e-9) return false;
    const double x2 = (e1[0]*e2[0] + e1[1]*e2[1] + e1[2]*e2[2]) / l1;
    double y2s = l2*l2 - x2*x2; if (y2s < 0.0) y2s = 0.0;
    const double y2 = sqrt(y2s);
    {   // sin altura (colineal o con dos esquinas casi en el mismo punto): la altura minima contra el lado mas largo
        const double l3 = sqrt((p2[0]-p1[0])*(p2[0]-p1[0]) + (p2[1]-p1[1])*(p2[1]-p1[1]) + (p2[2]-p1[2])*(p2[2]-p1[2]));
        double lmax = l1; if (l2 > lmax) lmax = l2; if (l3 > lmax) lmax = l3;
        if (l1 * y2 < 1e-4 * lmax * lmax) return false;
    }
    t.x[0] = 0.0; t.y[0] = 0.0; t.x[1] = l1; t.y[1] = 0.0; t.x[2] = x2; t.y[2] = y2;
    t.A = 0.5 * l1 * y2;
    for (int i = 0; i < 3; i++) {
        const int j = (i + 1) % 3, k = (i + 2) % 3;
        const double ax = t.x[j]-t.x[i], ay = t.y[j]-t.y[i], bx = t.x[k]-t.x[i], by = t.y[k]-t.y[i];
        t.ang[i] = atan2(fabs(ax*by - ay*bx), ax*bx + ay*by);
        t.gx[i] = (t.y[j] - t.y[k]) / (2.0 * t.A);
        t.gy[i] = (t.x[k] - t.x[j]) / (2.0 * t.A);
    }
    return true;
}

// lc[corner] = uv vert local (solo los corners de esta carta estan cargados)
static void TriangulosCarta(const UVMalla& M, const Carta& C, const std::vector<int>& lc, std::vector<TriPlano>& tris) {
    std::vector<int> tc;
    for (size_t i = 0; i < C.caras.size(); i++) {
        TriangulosCara(M, C.caras[i], tc);
        for (size_t t = 0; t + 2 < tc.size(); t += 3) {
            TriPlano tp;
            tp.v[0] = lc[(size_t)tc[t]]; tp.v[1] = lc[(size_t)tc[t+1]]; tp.v[2] = lc[(size_t)tc[t+2]];
            if (tp.v[0] < 0 || tp.v[1] < 0 || tp.v[2] < 0) continue;
            if (tp.v[0] == tp.v[1] || tp.v[1] == tp.v[2] || tp.v[0] == tp.v[2]) continue;
            if (!ArmarTriPlano(&C.p3[(size_t)tp.v[0]*3], &C.p3[(size_t)tp.v[1]*3], &C.p3[(size_t)tp.v[2]*3], tp)) continue;
            tris.push_back(tp);
        }
    }
}

// ---------------------------------------------------------------------------
//  Mapeo conforme por minimos cuadrados. Para cada triangulo, con u y v lineales sobre sus
//  coordenadas 2D (x,y), la condicion de semejanza es du/dx = dv/dy y du/dy = -dv/dx; cada
//  triangulo aporta esas dos ecuaciones pesadas por sqrt(area). Dos vertices quedan fijos
//  (los extremos del eje mas largo de la isla, a su distancia 3D: la isla sale a escala 1).
//  El sistema rectangular se resuelve por gradientes conjugados sobre las ecuaciones normales
//  sin armar A^T A (cada iteracion hace un producto por A y otro por A^T).
// ---------------------------------------------------------------------------
struct Disperso {
    std::vector<int> ini;        // ini[fila]..ini[fila+1]
    std::vector<int> col;
    std::vector<double> val;
    std::vector<double> rhs;
    int nCol;
    Disperso() : nCol(0) {}
    void MulA(const std::vector<double>& x, std::vector<double>& y) const {
        const size_t nR = rhs.size();
        for (size_t r = 0; r < nR; r++) {
            double s = 0.0;
            for (int k = ini[r]; k < ini[r+1]; k++) s += val[(size_t)k] * x[(size_t)col[(size_t)k]];
            y[r] = s;
        }
    }
    void MulAT(const std::vector<double>& y, std::vector<double>& x) const {
        x.assign((size_t)nCol, 0.0);
        const size_t nR = rhs.size();
        for (size_t r = 0; r < nR; r++)
            for (int k = ini[r]; k < ini[r+1]; k++) x[(size_t)col[(size_t)k]] += val[(size_t)k] * y[r];
    }
};

static double Punto(const std::vector<double>& a, const std::vector<double>& b) { double s = 0.0; for (size_t i = 0; i < a.size(); i++) s += a[i] * b[i]; return s; }

static void ResolverMinCuad(const Disperso& S, std::vector<double>& x) {
    const size_t nR = S.rhs.size();
    x.assign((size_t)S.nCol, 0.0);
    if (nR == 0 || S.nCol == 0) return;
    std::vector<double> r(S.rhs), s, p, q(nR, 0.0);
    S.MulAT(r, s);
    p = s;
    double gamma = Punto(s, s);
    const double tol = gamma * 1e-22;
    int maxIt = 4 * S.nCol + 200; if (maxIt > 20000) maxIt = 20000;
    for (int it = 0; it < maxIt; it++) {
        if (gamma <= tol) break;
        S.MulA(p, q);
        const double qq = Punto(q, q);
        if (qq <= 0.0) break;
        const double alfa = gamma / qq;
        for (size_t i = 0; i < x.size(); i++) x[i] += alfa * p[i];
        for (size_t i = 0; i < nR; i++) r[i] -= alfa * q[i];
        S.MulAT(r, s);
        const double gn = Punto(s, s);
        if (gn <= tol) break;
        const double beta = gn / gamma;
        for (size_t i = 0; i < p.size(); i++) p[i] = s[i] + beta * p[i];
        gamma = gn;
    }
}

static void AplanarConforme(const std::vector<TriPlano>& tris, int nLoc, const std::vector<double>& p3, std::vector<double>& uv) {
    uv.assign((size_t)nLoc * 2, 0.0);
    if (nLoc < 2 || tris.empty()) return;
    // los dos vertices fijos: extremos del eje mas largo de la caja de la isla
    double mn[3] = { 1e300, 1e300, 1e300 }, mx[3] = { -1e300, -1e300, -1e300 };
    for (int k = 0; k < nLoc; k++) for (int a = 0; a < 3; a++) { const double c = p3[(size_t)k*3 + a]; if (c < mn[a]) mn[a] = c; if (c > mx[a]) mx[a] = c; }
    int eje = 0; for (int a = 1; a < 3; a++) if (mx[a] - mn[a] > mx[eje] - mn[eje]) eje = a;
    int pA = 0, pB = 0;
    for (int k = 1; k < nLoc; k++) { if (p3[(size_t)k*3 + eje] < p3[(size_t)pA*3 + eje]) pA = k; if (p3[(size_t)k*3 + eje] > p3[(size_t)pB*3 + eje]) pB = k; }
    if (pA == pB) pB = (pA + 1) % nLoc;
    double d = 0.0; { const double dx = p3[(size_t)pB*3]-p3[(size_t)pA*3], dy = p3[(size_t)pB*3+1]-p3[(size_t)pA*3+1], dz = p3[(size_t)pB*3+2]-p3[(size_t)pA*3+2]; d = sqrt(dx*dx + dy*dy + dz*dz); }
    if (d < 1e-9) d = 1.0;
    uv[(size_t)pA*2] = 0.0; uv[(size_t)pA*2+1] = 0.0; uv[(size_t)pB*2] = d; uv[(size_t)pB*2+1] = 0.0;
    std::vector<int> colDe((size_t)nLoc, -1); int nCol = 0;
    for (int k = 0; k < nLoc; k++) if (k != pA && k != pB) { colDe[(size_t)k] = nCol; nCol += 2; }
    if (nCol == 0) return;
    Disperso S; S.nCol = nCol;
    for (size_t t = 0; t < tris.size(); t++) {
        const TriPlano& T = tris[t];
        const double sA = sqrt(T.A);
        for (int fila = 0; fila < 2; fila++) {
            S.ini.push_back((int)S.col.size()); double b = 0.0;
            for (int i = 0; i < 3; i++) {
                const int k = T.v[i];
                const double cu = (fila == 0) ? sA * T.gx[i] : sA * T.gy[i];
                const double cv = (fila == 0) ? -sA * T.gy[i] : sA * T.gx[i];
                if (colDe[(size_t)k] >= 0) { S.col.push_back(colDe[(size_t)k]); S.val.push_back(cu); S.col.push_back(colDe[(size_t)k] + 1); S.val.push_back(cv); }
                else b -= cu * uv[(size_t)k*2] + cv * uv[(size_t)k*2+1];
            }
            S.rhs.push_back(b);
        }
    }
    S.ini.push_back((int)S.col.size());
    std::vector<double> x; ResolverMinCuad(S, x);
    for (int k = 0; k < nLoc; k++) if (colDe[(size_t)k] >= 0) { uv[(size_t)k*2] = x[(size_t)colDe[(size_t)k]]; uv[(size_t)k*2+1] = x[(size_t)colDe[(size_t)k] + 1]; }
}

// ---------------------------------------------------------------------------
//  Relajacion vertice por vertice (metodos angular y minimo estiramiento). La energia de un
//  triangulo sale de su jacobiano (x,y)->(u,v):
//    angular: suma de (angulo uv - angulo 3d)^2 en sus 3 esquinas (invariante a la escala);
//    estiramiento: area3d * (s1^2 + s2^2 + 1/s1^2 + 1/s2^2) con s1, s2 los valores singulares del
//      jacobiano = area3d * |J|^2 * (1 + 1/det^2). Estirar y comprimir cuestan lo mismo, asi que
//      ni agrandar ni achicar la isla "gana" (la version solo-estiramiento premiaba inflar el
//      borde); el minimo (4 por unidad de area) es la isometria.
//  Un triangulo dado vuelta (det <= 0) vale infinito: la busqueda lineal nunca lo acepta.
// ---------------------------------------------------------------------------
static const double kInfinito = 1e30;

static double EnergiaTri(const TriPlano& t, const std::vector<double>& uv, int metodo) {
    double a = 0.0, b = 0.0, c = 0.0, d = 0.0;
    for (int i = 0; i < 3; i++) {
        const double u = uv[(size_t)t.v[i]*2], v = uv[(size_t)t.v[i]*2+1];
        a += t.gx[i] * u; b += t.gy[i] * u; c += t.gx[i] * v; d += t.gy[i] * v;
    }
    const double det = a*d - b*c;
    if (det <= 1e-14) return kInfinito;
    if (metodo == W3dUnwrapEstiramiento) return t.A * (a*a + b*b + c*c + d*d) * (1.0 + 1.0 / (det * det));
    double e = 0.0;
    for (int i = 0; i < 3; i++) {
        const int j = (i + 1) % 3, k = (i + 2) % 3;
        const double ax = uv[(size_t)t.v[j]*2] - uv[(size_t)t.v[i]*2], ay = uv[(size_t)t.v[j]*2+1] - uv[(size_t)t.v[i]*2+1];
        const double bx = uv[(size_t)t.v[k]*2] - uv[(size_t)t.v[i]*2], by = uv[(size_t)t.v[k]*2+1] - uv[(size_t)t.v[i]*2+1];
        const double ang = atan2(fabs(ax*by - ay*bx), ax*bx + ay*by) - t.ang[i];
        e += ang * ang;
    }
    return e;
}

static double EnergiaVert(const std::vector<int>& I, const std::vector<TriPlano>& tris, const std::vector<double>& uv, int metodo) {
    double e = 0.0;
    for (size_t i = 0; i < I.size(); i++) { e += EnergiaTri(tris[(size_t)I[i]], uv, metodo); if (e >= kInfinito) return kInfinito; }
    return e;
}

static void Relajar(const std::vector<TriPlano>& tris, int nLoc, std::vector<double>& uv, int metodo) {
    if (tris.empty() || nLoc == 0) return;
    std::vector<std::vector<int> > inc((size_t)nLoc);
    for (size_t t = 0; t < tris.size(); t++) for (int i = 0; i < 3; i++) inc[(size_t)tris[t].v[i]].push_back((int)t);
    const int pasadas = (tris.size() > 20000) ? 8 : (tris.size() > 5000) ? 15 : 30;
    for (int pasada = 0; pasada < pasadas; pasada++) {
        for (int k = 0; k < nLoc; k++) {
            const std::vector<int>& I = inc[(size_t)k];
            if (I.empty()) continue;
            // escala local: largo medio de los lados uv que salen del vertice
            double esc = 0.0; int ne = 0;
            for (size_t i = 0; i < I.size(); i++) for (int j = 0; j < 3; j++) {
                const int o = tris[(size_t)I[i]].v[j]; if (o == k) continue;
                const double dx = uv[(size_t)o*2] - uv[(size_t)k*2], dy = uv[(size_t)o*2+1] - uv[(size_t)k*2+1];
                esc += sqrt(dx*dx + dy*dy); ne++;
            }
            if (ne == 0) continue; esc /= ne;
            if (esc < 1e-12) continue;
            const double E0 = EnergiaVert(I, tris, uv, metodo);
            if (E0 >= kInfinito) continue;           // ya dado vuelta: no se toca
            const double u = uv[(size_t)k*2], v = uv[(size_t)k*2+1], h = esc * 1e-3;
            uv[(size_t)k*2] = u + h; const double Eu1 = EnergiaVert(I, tris, uv, metodo);
            uv[(size_t)k*2] = u - h; const double Eu0 = EnergiaVert(I, tris, uv, metodo); uv[(size_t)k*2] = u;
            uv[(size_t)k*2+1] = v + h; const double Ev1 = EnergiaVert(I, tris, uv, metodo);
            uv[(size_t)k*2+1] = v - h; const double Ev0 = EnergiaVert(I, tris, uv, metodo); uv[(size_t)k*2+1] = v;
            if (Eu1 >= kInfinito || Eu0 >= kInfinito || Ev1 >= kInfinito || Ev0 >= kInfinito) continue;
            const double gu = (Eu1 - Eu0) / (2.0 * h), gv = (Ev1 - Ev0) / (2.0 * h);
            const double g = sqrt(gu*gu + gv*gv);
            if (!(g > 1e-15)) continue;
            const double du = -gu / g, dv = -gv / g;
            double paso = esc * 0.2; bool mejoro = false;
            for (int j = 0; j < 8; j++) {
                uv[(size_t)k*2] = u + du * paso; uv[(size_t)k*2+1] = v + dv * paso;
                if (EnergiaVert(I, tris, uv, metodo) < E0) { mejoro = true; break; }
                paso *= 0.5;
            }
            if (!mejoro) { uv[(size_t)k*2] = u; uv[(size_t)k*2+1] = v; }
        }
    }
}

// ---------------------------------------------------------------------------
//  Empaquetado: cada isla se gira al angulo (de a 15 grados) que achica mas su caja, se ordena
//  por alto y se acomoda en filas (estantes) dentro de [0,1]; la escala comun se busca por
//  biseccion (la mayor con la que todo entra). 'margen' = separacion entre islas (y la mitad
//  contra el borde), en unidades UV.
// ---------------------------------------------------------------------------
struct OrdenAlto { const std::vector<double>* H; bool operator()(int a, int b) const { return (*H)[(size_t)a] > (*H)[(size_t)b]; } };

static bool Colocar(const std::vector<double>& W, const std::vector<double>& H, const std::vector<int>& orden, double s, double mg, std::vector<double>& X, std::vector<double>& Y) {
    const double x0 = mg * 0.5, ancho = 1.0 - mg;
    double x = 0.0, y = 0.0, filaH = 0.0;
    for (size_t o = 0; o < orden.size(); o++) {
        const int i = orden[o];
        const double bw = W[(size_t)i] * s, bh = H[(size_t)i] * s;
        if (bw > ancho + 1e-12) return false;
        if (x > 0.0 && x + bw > ancho + 1e-12) { y += filaH + mg; x = 0.0; filaH = 0.0; }
        X[(size_t)i] = x0 + x; Y[(size_t)i] = x0 + y;
        x += bw + mg; if (bh > filaH) filaH = bh;
    }
    return y + filaH <= ancho + 1e-12;
}

static void Empaquetar(std::vector<Carta>& cartas, double margen) {
    const int n = (int)cartas.size(); if (n == 0) return;
    std::vector<double> W((size_t)n, 0.0), H((size_t)n, 0.0);
    for (int i = 0; i < n; i++) {
        Carta& C = cartas[(size_t)i]; if (C.nLoc == 0) continue;
        double mejorArea = 1e300, mejorAng = 0.0;
        for (int a = 0; a < 12; a++) {
            const double ang = a * (3.14159265358979 / 12.0), cs = cos(ang), sn = sin(ang);
            double mnx = 1e300, mny = 1e300, mxx = -1e300, mxy = -1e300;
            for (int k = 0; k < C.nLoc; k++) {
                const double u = C.uv[(size_t)k*2], v = C.uv[(size_t)k*2+1];
                const double x = u*cs - v*sn, y = u*sn + v*cs;
                if (x < mnx) mnx = x; if (x > mxx) mxx = x; if (y < mny) mny = y; if (y > mxy) mxy = y;
            }
            const double area = (mxx - mnx) * (mxy - mny);
            if (area < mejorArea - 1e-15) { mejorArea = area; mejorAng = ang; }
        }
        const double cs = cos(mejorAng), sn = sin(mejorAng);
        double mnx = 1e300, mny = 1e300, mxx = -1e300, mxy = -1e300;
        for (int k = 0; k < C.nLoc; k++) {
            const double u = C.uv[(size_t)k*2], v = C.uv[(size_t)k*2+1];
            const double x = u*cs - v*sn, y = u*sn + v*cs;
            C.uv[(size_t)k*2] = x; C.uv[(size_t)k*2+1] = y;
            if (x < mnx) mnx = x; if (x > mxx) mxx = x; if (y < mny) mny = y; if (y > mxy) mxy = y;
        }
        for (int k = 0; k < C.nLoc; k++) { C.uv[(size_t)k*2] -= mnx; C.uv[(size_t)k*2+1] -= mny; }
        W[(size_t)i] = mxx - mnx; H[(size_t)i] = mxy - mny;
    }
    double mg = margen; if (mg < 0.0) mg = 0.0; if (mg > 0.4) mg = 0.4;
    double maxW = 0.0, maxH = 0.0;
    for (int i = 0; i < n; i++) { if (W[(size_t)i] > maxW) maxW = W[(size_t)i]; if (H[(size_t)i] > maxH) maxH = H[(size_t)i]; }
    const double mayor = (maxW > maxH) ? maxW : maxH;
    std::vector<int> orden((size_t)n); for (int i = 0; i < n; i++) orden[(size_t)i] = i;
    OrdenAlto cmp; cmp.H = &H; std::sort(orden.begin(), orden.end(), cmp);
    std::vector<double> X((size_t)n, 0.0), Y((size_t)n, 0.0);
    double sOk = 0.0;
    if (mayor > 1e-12) {
        double lo = 0.0, hi = (1.0 - mg) / mayor;
        if (Colocar(W, H, orden, hi, mg, X, Y)) sOk = hi;
        else for (int it = 0; it < 40; it++) { const double mid = 0.5 * (lo + hi); if (Colocar(W, H, orden, mid, mg, X, Y)) { lo = mid; sOk = mid; } else hi = mid; }
    }
    Colocar(W, H, orden, sOk, mg, X, Y);
    for (int i = 0; i < n; i++) {
        Carta& C = cartas[(size_t)i];
        for (int k = 0; k < C.nLoc; k++) { C.uv[(size_t)k*2] = X[(size_t)i] + C.uv[(size_t)k*2] * sOk; C.uv[(size_t)k*2+1] = Y[(size_t)i] + C.uv[(size_t)k*2+1] * sOk; }
    }
}

// escribe las cartas en el mapa UV activo (solo los corners de las caras seleccionadas), con Ctrl+Z
static bool EscribirCartas(const UVMalla& M, const std::vector<Carta>& cartas, bool voltearV) {
    Mesh* m = M.m;
    if (m->uvMaps.empty()) m->PoblarCapas();
    UVMap* um = (m->uvMapActivo >= 0 && m->uvMapActivo < (int)m->uvMaps.size()) ? m->uvMaps[(size_t)m->uvMapActivo] : NULL;
    std::vector<float> uvL((size_t)M.nC * 2, 0.0f);
    if (um && (int)um->uv.size() == M.nC * 2) for (size_t i = 0; i < uvL.size(); i++) uvL[i] = um->uv[i];
    for (size_t ci = 0; ci < cartas.size(); ci++) {
        const Carta& C = cartas[ci];
        for (size_t i = 0; i < C.corners.size(); i++) {
            const int k = C.corners[i], l = C.local[i];
            const double u = C.uv[(size_t)l*2], v = C.uv[(size_t)l*2+1];
            uvL[(size_t)k*2] = (float)u; uvL[(size_t)k*2+1] = (float)(voltearV ? 1.0 - v : v);
        }
    }
    UndoCapturarMallaGeo(m);
    m->EscribirUVProyeccion(uvL);
    return true;
}

// base ortonormal (t, b) del plano de normal n, con t x b = n (proyeccion bien orientada)
static void BasePlano(const double* n, double* t, double* b) {
    double h[3] = { 0.0, 0.0, 0.0 };
    const double ax = fabs(n[0]), ay = fabs(n[1]), az = fabs(n[2]);
    if (ax <= ay && ax <= az) h[0] = 1.0; else if (ay <= az) h[1] = 1.0; else h[2] = 1.0;
    t[0] = h[1]*n[2] - h[2]*n[1]; t[1] = h[2]*n[0] - h[0]*n[2]; t[2] = h[0]*n[1] - h[1]*n[0];
    const double lt = sqrt(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);
    if (lt > 1e-12) { t[0] /= lt; t[1] /= lt; t[2] /= lt; } else { t[0] = 1.0; t[1] = 0.0; t[2] = 0.0; }
    b[0] = n[1]*t[2] - n[2]*t[1]; b[1] = n[2]*t[0] - n[0]*t[2]; b[2] = n[0]*t[1] - n[1]*t[0];
}

// normal unitaria (Newell) y area de la cara f
static void NormalCara(const UVMalla& M, int f, double* n, double& area) {
    const std::vector<int>& idx = M.m->faces3d[(size_t)f].idx; const int cnt = (int)idx.size();
    n[0] = n[1] = n[2] = 0.0;
    for (int c = 0; c < cnt; c++) {
        const float* a = Pos(M, idx[(size_t)c]); const float* b = Pos(M, idx[(size_t)((c + 1) % cnt)]);
        n[0] += (a[1]-b[1]) * (a[2]+b[2]); n[1] += (a[2]-b[2]) * (a[0]+b[0]); n[2] += (a[0]-b[0]) * (a[1]+b[1]);
    }
    const double l = sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    area = 0.5 * l;
    if (l > 1e-12) { n[0] /= l; n[1] /= l; n[2] /= l; } else { n[0] = 0.0; n[1] = 0.0; n[2] = 1.0; }
}

struct OrdenArea { const std::vector<double>* A; bool operator()(int a, int b) const { return (*A)[(size_t)a] > (*A)[(size_t)b]; } };

} // namespace

// ============================================================================
//  Unwrap
// ============================================================================
bool W3dUnwrap(Mesh* m, int metodo, float margen, std::string& msg, int* islas) {
    UVMalla M; if (!Preparar(m, M, msg, "Unwrap")) return false;
    TablaLados T; ArmarLados(M, T);
    std::vector<int> uvDe; const int nUV = AsignarUVVerts(M, T, true, NULL, uvDe);
    std::vector<Carta> cartas; ArmarCartas(M, uvDe, nUV, cartas);
    // una isla CERRADA (sin borde: ningun lado libre ni costura) no se puede aplanar: colapsa. Si toda la
    // seleccion es asi (un cubo o una esfera sin costuras), se avisa en vez de escribir basura.
    {   std::vector<unsigned char> abierta(cartas.size(), 0);
        std::vector<int> cartaDe(m->faces3d.size(), -1);
        for (size_t ci = 0; ci < cartas.size(); ci++) for (size_t i = 0; i < cartas[ci].caras.size(); i++) cartaDe[(size_t)cartas[ci].caras[i]] = (int)ci;
        for (TablaLados::const_iterator it = T.begin(); it != T.end(); ++it) {
            const std::vector<Lado>& ls = it->second;
            const int ci = cartaDe[(size_t)ls[0].f]; if (ci < 0) continue;
            bool borde = (ls.size() != 2);
            if (!borde) { const std::vector<int>& idx = m->faces3d[(size_t)ls[0].f].idx; const int cnt = (int)idx.size();
                          borde = EsCostura(M, idx[(size_t)ls[0].c], idx[(size_t)((ls[0].c + 1) % cnt)]); }
            if (borde) abierta[(size_t)ci] = 1;
        }
        bool alguna = false; for (size_t ci = 0; ci < cartas.size(); ci++) if (abierta[ci]) alguna = true;
        if (!alguna) { msg = "Unwrap: closed surface, mark seams first"; return false; }
    }
    std::vector<int> lc((size_t)M.nC, -1);
    for (size_t ci = 0; ci < cartas.size(); ci++) {
        Carta& C = cartas[ci];
        for (size_t i = 0; i < C.corners.size(); i++) lc[(size_t)C.corners[i]] = C.local[i];
        std::vector<TriPlano> tris; TriangulosCarta(M, C, lc, tris);
        AplanarConforme(tris, C.nLoc, C.p3, C.uv);
        if (metodo != W3dUnwrapConforme) Relajar(tris, C.nLoc, C.uv, metodo);
        for (size_t i = 0; i < C.corners.size(); i++) lc[(size_t)C.corners[i]] = -1;
    }
    Empaquetar(cartas, margen);
    if (islas) *islas = (int)cartas.size();
    return EscribirCartas(M, cartas, true);
}

// ============================================================================
//  Smart UV Project
// ============================================================================
bool W3dSmartUVProject(Mesh* m, float anguloLimiteDeg, float margen, std::string& msg, int* islas) {
    UVMalla M; if (!Preparar(m, M, msg, "Smart UV Project")) return false;
    const int nF = (int)m->faces3d.size();
    std::vector<double> N((size_t)nF * 3, 0.0), Ar((size_t)nF, 0.0);
    std::vector<int> orden;
    for (int f = 0; f < nF; f++) if (M.sel[(size_t)f]) { NormalCara(M, f, &N[(size_t)f*3], Ar[(size_t)f]); orden.push_back(f); }
    OrdenArea cmp; cmp.A = &Ar; std::sort(orden.begin(), orden.end(), cmp);
    double lim = anguloLimiteDeg; if (lim < 1.0) lim = 1.0; if (lim > 89.0) lim = 89.0;
    const double cosLim = cos(lim * 3.14159265358979 / 180.0);
    // grupos de proyeccion: la cara mas grande sin grupo abre uno y se lleva a las que miran parecido
    std::vector<int> grupo((size_t)nF, -1); std::vector<double> gN; int ng = 0;
    for (size_t i = 0; i < orden.size(); i++) {
        const int f = orden[i]; if (grupo[(size_t)f] >= 0) continue;
        const int g = ng++; grupo[(size_t)f] = g;
        const double* nf = &N[(size_t)f*3]; gN.push_back(nf[0]); gN.push_back(nf[1]); gN.push_back(nf[2]);
        for (size_t j = i + 1; j < orden.size(); j++) {
            const int h = orden[j]; if (grupo[(size_t)h] >= 0) continue;
            const double* nh = &N[(size_t)h*3];
            if (nf[0]*nh[0] + nf[1]*nh[1] + nf[2]*nh[2] >= cosLim) grupo[(size_t)h] = g;
        }
    }
    TablaLados T; ArmarLados(M, T);
    std::vector<int> uvDe; const int nUV = AsignarUVVerts(M, T, true, &grupo, uvDe);
    std::vector<Carta> cartas; ArmarCartas(M, uvDe, nUV, cartas);
    for (size_t ci = 0; ci < cartas.size(); ci++) {
        Carta& C = cartas[ci]; if (C.caras.empty()) continue;
        const int g = grupo[(size_t)C.caras[0]];
        double t[3], b[3]; BasePlano(&gN[(size_t)g*3], t, b);
        for (int k = 0; k < C.nLoc; k++) {
            const double* p = &C.p3[(size_t)k*3];
            C.uv[(size_t)k*2] = p[0]*t[0] + p[1]*t[1] + p[2]*t[2];
            C.uv[(size_t)k*2+1] = p[0]*b[0] + p[1]*b[1] + p[2]*b[2];
        }
    }
    Empaquetar(cartas, margen);
    if (islas) *islas = (int)cartas.size();
    return EscribirCartas(M, cartas, true);
}

// ============================================================================
//  Lightmap Pack: grilla de celdas iguales, una cara por celda (proyectada sobre su plano)
// ============================================================================
bool W3dLightmapPack(Mesh* m, float margen, std::string& msg) {
    UVMalla M; if (!Preparar(m, M, msg, "Lightmap Pack")) return false;
    const int nF = (int)m->faces3d.size();
    std::vector<Carta> cartas;
    for (int f = 0; f < nF; f++) {
        if (!M.sel[(size_t)f]) continue;
        const std::vector<int>& idx = m->faces3d[(size_t)f].idx; const int cnt = (int)idx.size();
        Carta C; C.caras.push_back(f); C.nLoc = cnt;
        double n[3], area; NormalCara(M, f, n, area);
        double t[3], b[3]; BasePlano(n, t, b);
        for (int c = 0; c < cnt; c++) {
            const float* p = Pos(M, idx[(size_t)c]);
            C.corners.push_back(M.ini[(size_t)f] + c); C.local.push_back(c);
            C.p3.push_back(p[0]); C.p3.push_back(p[1]); C.p3.push_back(p[2]);
            C.uv.push_back(p[0]*t[0] + p[1]*t[1] + p[2]*t[2]);
            C.uv.push_back(p[0]*b[0] + p[1]*b[1] + p[2]*b[2]);
        }
        cartas.push_back(C);
    }
    const int n = (int)cartas.size();
    int cols = (int)ceil(sqrt((double)n)); if (cols < 1) cols = 1;
    const double celda = 1.0 / cols;
    double mg = margen; if (mg < 0.0) mg = 0.0; if (mg > celda * 0.5) mg = celda * 0.5;
    const double util = celda - mg;
    for (int i = 0; i < n; i++) {
        Carta& C = cartas[(size_t)i];
        double mnx = 1e300, mny = 1e300, mxx = -1e300, mxy = -1e300;
        for (int k = 0; k < C.nLoc; k++) { const double u = C.uv[(size_t)k*2], v = C.uv[(size_t)k*2+1]; if (u < mnx) mnx = u; if (u > mxx) mxx = u; if (v < mny) mny = v; if (v > mxy) mxy = v; }
        double mayor = (mxx - mnx > mxy - mny) ? mxx - mnx : mxy - mny; if (mayor < 1e-12) mayor = 1.0;
        const double s = util / mayor;
        const double cx = (i % cols) * celda + celda * 0.5, cy = (i / cols) * celda + celda * 0.5;
        const double bx = 0.5 * (mnx + mxx), by = 0.5 * (mny + mxy);
        for (int k = 0; k < C.nLoc; k++) { C.uv[(size_t)k*2] = cx + (C.uv[(size_t)k*2] - bx) * s; C.uv[(size_t)k*2+1] = cy + (C.uv[(size_t)k*2+1] - by) * s; }
    }
    return EscribirCartas(M, cartas, true);
}

// ============================================================================
//  Follow Active Quads: desde el quad activo (con sus UV como estan), cada quad vecino por un
//  lado compartido recibe ese lado tal cual y sus otros dos corners extrapolados en la misma
//  direccion, a la razon de largos 3D (la grilla sigue derecha y a escala).
// ============================================================================
bool W3dFollowActiveQuads(Mesh* m, std::string& msg, int* quads) {
    UVMalla M; if (!Preparar(m, M, msg, "Follow Active Quads")) return false;
    if (M.activa < 0 || !M.sel[(size_t)M.activa] || m->faces3d[(size_t)M.activa].idx.size() != 4) {
        msg = "Follow Active Quads: the active face must be a selected quad"; return false;
    }
    if (m->uvMaps.empty()) m->PoblarCapas();
    UVMap* um = (m->uvMapActivo >= 0 && m->uvMapActivo < (int)m->uvMaps.size()) ? m->uvMaps[(size_t)m->uvMapActivo] : NULL;
    if (!um || (int)um->uv.size() != M.nC * 2) { msg = "Follow Active Quads: the active face must be a selected quad"; return false; }
    std::vector<float> uvL(um->uv.begin(), um->uv.end());
    TablaLados T; ArmarLados(M, T);
    std::vector<unsigned char> visto(m->faces3d.size(), 0);
    std::vector<int> cola; cola.push_back(M.activa); visto[(size_t)M.activa] = 1;
    int hechos = 0;
    for (size_t qi = 0; qi < cola.size(); qi++) {
        const int f = cola[qi];
        const std::vector<int>& idx = m->faces3d[(size_t)f].idx; const int L = M.ini[(size_t)f];
        for (int c = 0; c < 4; c++) {
            const int c1 = (c + 1) % 4;
            const int a = Rep(M, idx[(size_t)c]), b = Rep(M, idx[(size_t)c1]);
            if (a == b) continue;
            TablaLados::const_iterator it = T.find(std::make_pair(a < b ? a : b, a < b ? b : a));
            if (it == T.end()) continue;
            for (size_t li = 0; li < it->second.size(); li++) {
                const int g = it->second[li].f;
                if (g == f || visto[(size_t)g] || !M.sel[(size_t)g] || m->faces3d[(size_t)g].idx.size() != 4) continue;
                const std::vector<int>& jg = m->faces3d[(size_t)g].idx;
                int ia = -1, ib = -1;
                for (int cc = 0; cc < 4; cc++) { const int r = Rep(M, jg[(size_t)cc]); if (r == a && ia < 0) ia = cc; else if (r == b && ib < 0) ib = cc; }
                if (ia < 0 || ib < 0) continue;
                const int x = ((ia + 1) % 4 == ib) ? (ia + 3) % 4 : (ia + 1) % 4;   // en g: junto a 'a', del otro lado
                const int y = ((ib + 1) % 4 == ia) ? (ib + 3) % 4 : (ib + 1) % 4;   // en g: junto a 'b'
                const int d = (c + 3) % 4, e2 = (c + 2) % 4;                      // en f: junto a 'a' y junto a 'b'
                const double la = Dist3(Pos(M, jg[(size_t)x]), Pos(M, idx[(size_t)c])),  ld = Dist3(Pos(M, idx[(size_t)c]),  Pos(M, idx[(size_t)d]));
                const double lb = Dist3(Pos(M, jg[(size_t)y]), Pos(M, idx[(size_t)c1])), le = Dist3(Pos(M, idx[(size_t)c1]), Pos(M, idx[(size_t)e2]));
                double r = 0.0; int nr = 0;
                if (ld > 1e-9) { r += la / ld; nr++; }
                if (le > 1e-9) { r += lb / le; nr++; }
                r = nr ? r / nr : 1.0;
                const float ua0 = uvL[(size_t)(L+c)*2],  ua1 = uvL[(size_t)(L+c)*2+1];
                const float ub0 = uvL[(size_t)(L+c1)*2], ub1 = uvL[(size_t)(L+c1)*2+1];
                const float ud0 = uvL[(size_t)(L+d)*2],  ud1 = uvL[(size_t)(L+d)*2+1];
                const float ue0 = uvL[(size_t)(L+e2)*2], ue1 = uvL[(size_t)(L+e2)*2+1];
                const int Lg = M.ini[(size_t)g];
                uvL[(size_t)(Lg+ia)*2] = ua0; uvL[(size_t)(Lg+ia)*2+1] = ua1;
                uvL[(size_t)(Lg+ib)*2] = ub0; uvL[(size_t)(Lg+ib)*2+1] = ub1;
                uvL[(size_t)(Lg+x)*2] = (float)(ua0 + (ua0 - ud0) * r); uvL[(size_t)(Lg+x)*2+1] = (float)(ua1 + (ua1 - ud1) * r);
                uvL[(size_t)(Lg+y)*2] = (float)(ub0 + (ub0 - ue0) * r); uvL[(size_t)(Lg+y)*2+1] = (float)(ub1 + (ub1 - ue1) * r);
                visto[(size_t)g] = 1; cola.push_back(g); hechos++;
            }
        }
    }
    if (hechos == 0) { msg = "Follow Active Quads: no neighbouring quads"; return false; }
    UndoCapturarMallaGeo(m);
    m->EscribirUVProyeccion(uvL);
    if (quads) *quads = hechos;
    return true;
}

// ============================================================================
//  Reset: cada cara seleccionada ocupa toda la textura (quads y triangulos en las esquinas del
//  cuadrado, ngons sobre el circulo inscripto). V=0 arriba, como el resto del engine.
// ============================================================================
bool W3dUVReset(Mesh* m, std::string& msg) {
    UVMalla M; if (!Preparar(m, M, msg, "Reset")) return false;
    if (m->uvMaps.empty()) m->PoblarCapas();
    UVMap* um = (m->uvMapActivo >= 0 && m->uvMapActivo < (int)m->uvMaps.size()) ? m->uvMaps[(size_t)m->uvMapActivo] : NULL;
    std::vector<float> uvL((size_t)M.nC * 2, 0.0f);
    if (um && (int)um->uv.size() == M.nC * 2) for (size_t i = 0; i < uvL.size(); i++) uvL[i] = um->uv[i];
    static const float cuadU[4] = { 0.0f, 1.0f, 1.0f, 0.0f }, cuadV[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        if (!M.sel[f]) continue;
        const int cnt = (int)m->faces3d[f].idx.size(), L = M.ini[f];
        for (int c = 0; c < cnt; c++) {
            float u, v;
            if (cnt <= 4) { u = cuadU[c]; v = cuadV[c]; }
            else { const double a = 2.0 * 3.14159265358979 * c / cnt; u = (float)(0.5 + 0.5 * cos(a)); v = (float)(0.5 - 0.5 * sin(a)); }
            uvL[(size_t)(L + c)*2] = u; uvL[(size_t)(L + c)*2+1] = v;
        }
    }
    UndoCapturarMallaGeo(m);
    m->EscribirUVProyeccion(uvL);
    return true;
}
