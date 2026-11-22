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
//  ProjectUVFaces). Los algoritmos trabajan con V hacia arriba (asi "bien orientado" es
//  determinante positivo) y al escribir se voltea V (v = 1 - v).
// ============================================================================
#include "edit/UVUnwrap.h"
#include "edit/MeshEdit.h"      // W3dTriangulateFace (ngons)
#include "objects/Mesh.h"
#include "objects/EditMesh.h"
#include "objects/Objects.h"    // EditSelectMode / SelFace
#include "Undo.h"               // UndoCaptureMeshGeo
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
struct UVMesh {
    Mesh* m;
    const GLfloat* P;                 // posiciones de reposo por render vert
    int nC;                           // corners totales
    std::vector<int> start;           // ini[f] = primer corner de faces3d[f]
    std::vector<unsigned char> sel;   // faces3d[f] seleccionada
    std::vector<int> rep;             // render vert -> vertice unico
    int active;                       // faces3d activa (modo cara) o -1
    int nSel;
};

// (W3dSmartUVProjectAll) desplegar TODAS las caras, sin mirar la seleccion de Edit Mode
static bool gUVAllFaces = false;
static bool Prepare(Mesh* m, UVMesh& M, std::string& msg, const char* op) {
    msg = std::string(op) + ": select faces first";
    if (!m) return false;
    m->EnsureEdit();
    if (!m->edit || !m->vertex || m->vertexSize <= 0) return false;
    M.m = m;
    M.P = m->RestPositions(); if (!M.P) M.P = m->vertex;
    // vertice unico por POSICION con tolerancia (posRep agrupa por bytes exactos: los polos de la esfera
    // difieren en un redondeo y quedaban como vertices distintos -> la isla se abria en el polo)
    const int nV = m->vertexSize;
    const bool hasRep = ((int)m->posRep.size() == nV);
    M.rep.resize((size_t)nV);
    std::map<std::pair<long, std::pair<long,long> >, int> byPos;
    for (int i = 0; i < nV; i++) {
        const int r = hasRep ? m->posRep[(size_t)i] : i;
        const float* p = &M.P[(size_t)r * 3];
        std::pair<long, std::pair<long,long> > key((long)floor(p[0] * 1e5 + 0.5), std::make_pair((long)floor(p[1] * 1e5 + 0.5), (long)floor(p[2] * 1e5 + 0.5)));
        std::map<std::pair<long, std::pair<long,long> >, int>::iterator it = byPos.find(key);
        if (it == byPos.end()) { byPos[key] = r; M.rep[(size_t)i] = r; } else M.rep[(size_t)i] = it->second;
    }
    EditMesh* e = m->edit;
    M.sel.assign(m->faces3d.size(), 0); M.nSel = 0;
    if (gUVAllFaces) { for (size_t f = 0; f < m->faces3d.size(); f++) M.sel[f] = 1; M.nSel = (int)m->faces3d.size(); }
    else
    for (size_t f = 0; f < e->faceSel.size() && f < e->faceSrc.size(); f++) {
        if (!e->faceSel[f]) continue;
        const int f3 = e->faceSrc[f];
        if (f3 >= 0 && f3 < (int)m->faces3d.size() && !M.sel[(size_t)f3]) { M.sel[(size_t)f3] = 1; M.nSel++; }
    }
    M.active = -1;
    if (EditSelectMode == SelFace && e->activeIdx >= 0 && e->activeIdx < (int)e->faceSrc.size()) M.active = e->faceSrc[(size_t)e->activeIdx];
    M.start.resize(m->faces3d.size()); int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++) { M.start[f] = L; L += (int)m->faces3d[f].idx.size(); }
    M.nC = L;
    if (M.nSel == 0) return false;
    msg.clear();
    return true;
}

static inline const float* Pos(const UVMesh& M, int rv) { return &M.P[(size_t)rv * 3]; }
static inline int Rep(const UVMesh& M, int rv) { return M.rep[(size_t)rv]; }
static inline double Dist3(const float* a, const float* b) { const double dx = a[0]-b[0], dy = a[1]-b[1], dz = a[2]-b[2]; return sqrt(dx*dx + dy*dy + dz*dz); }
static bool IsSeam(const UVMesh& M, int rva, int rvb) {
    if (M.m->seamEdges.empty()) return false;
    return M.m->seamEdges.count(Mesh::SharpEdgeKey(Pos(M, rva), Pos(M, rvb))) > 0;
}

// union-find chico (corners, caras)
struct UF {
    std::vector<int> p;
    void Init(int n) { p.resize((size_t)n); for (int i = 0; i < n; i++) p[(size_t)i] = i; }
    int Find(int a) { while (p[(size_t)a] != a) { p[(size_t)a] = p[(size_t)p[(size_t)a]]; a = p[(size_t)a]; } return a; }
    void Join(int a, int b) { a = Find(a); b = Find(b); if (a != b) p[(size_t)a] = b; }
};

// lados de las caras seleccionadas, por par de vertices unicos (min,max)
struct Side { int f, c; };   // cara y corner de salida (el lado va del corner c al c+1)
typedef std::map<std::pair<int,int>, std::vector<Side> > SideTable;
static void BuildSides(const UVMesh& M, SideTable& T) {
    for (size_t f = 0; f < M.m->faces3d.size(); f++) {
        if (!M.sel[f]) continue;
        const std::vector<int>& idx = M.m->faces3d[f].idx; const int cnt = (int)idx.size();
        for (int c = 0; c < cnt; c++) {
            const int a = Rep(M, idx[(size_t)c]), b = Rep(M, idx[(size_t)((c + 1) % cnt)]);
            if (a == b) continue;
            Side l; l.f = (int)f; l.c = c;
            T[std::make_pair(a < b ? a : b, a < b ? b : a)].push_back(l);
        }
    }
}
// corner (global) de la cara del lado que tiene el vertice unico v
static int CornerOf(const UVMesh& M, const Side& l, int v) {
    const std::vector<int>& idx = M.m->faces3d[(size_t)l.f].idx; const int cnt = (int)idx.size();
    return (Rep(M, idx[(size_t)l.c]) == v) ? M.start[(size_t)l.f] + l.c : M.start[(size_t)l.f] + (l.c + 1) % cnt;
}

// uvOf[corner] = uv vert (numerado denso) o -1 si la cara no esta seleccionada. Dos corners son el
// MISMO uv vert cuando comparten vertice a traves de un lado compartido que no es costura (y, si
// hay 'grupo', entre caras del mismo grupo). Una costura que muere en el medio de una isla no
// parte nada: los corners se juntan dando la vuelta por el otro lado.
static int AssignUVVerts(const UVMesh& M, const SideTable& T, bool seams, const std::vector<int>* group, std::vector<int>& uvOf) {
    UF uf; uf.Init(M.nC);
    for (SideTable::const_iterator it = T.begin(); it != T.end(); ++it) {
        const std::vector<Side>& ls = it->second;
        if (ls.size() < 2) continue;
        const int va = it->first.first, vb = it->first.second;
        {   const std::vector<int>& idx = M.m->faces3d[(size_t)ls[0].f].idx; const int cnt = (int)idx.size();
            if (seams && IsSeam(M, idx[(size_t)ls[0].c], idx[(size_t)((ls[0].c + 1) % cnt)])) continue; }
        for (size_t i = 0; i < ls.size(); i++) for (size_t j = i + 1; j < ls.size(); j++) {
            if (group && (*group)[(size_t)ls[i].f] != (*group)[(size_t)ls[j].f]) continue;
            uf.Join(CornerOf(M, ls[i], va), CornerOf(M, ls[j], va));
            uf.Join(CornerOf(M, ls[i], vb), CornerOf(M, ls[j], vb));
        }
    }
    // dos corners de la MISMA cara sobre el mismo vertice (el polo de la esfera: un quad con dos esquinas
    // en el mismo punto) son el mismo uv vert
    for (size_t f = 0; f < M.m->faces3d.size(); f++) {
        if (!M.sel[f]) continue;
        const std::vector<int>& idx = M.m->faces3d[f].idx; const int cnt = (int)idx.size();
        for (int c1 = 0; c1 < cnt; c1++) for (int c2 = c1 + 1; c2 < cnt; c2++)
            if (Rep(M, idx[(size_t)c1]) == Rep(M, idx[(size_t)c2])) uf.Join(M.start[f] + c1, M.start[f] + c2);
    }
    uvOf.assign((size_t)M.nC, -1);
    std::vector<int> id((size_t)M.nC, -1); int n = 0;
    for (size_t f = 0; f < M.m->faces3d.size(); f++) {
        if (!M.sel[f]) continue;
        const int cnt = (int)M.m->faces3d[f].idx.size();
        for (int c = 0; c < cnt; c++) {
            const int k = M.start[f] + c, r = uf.Find(k);
            if (id[(size_t)r] < 0) id[(size_t)r] = n++;
            uvOf[(size_t)k] = id[(size_t)r];
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
//  Cartas (islas): caras que comparten algun uv vert. Cada carta tiene sus corners, el uv vert
//  local de cada corner, la posicion 3D de cada uv vert local y sus coordenadas uv.
// ---------------------------------------------------------------------------
struct UVChart {
    std::vector<int> faces;       // faces3d
    std::vector<int> corners;     // corners globales
    std::vector<int> local;       // local[i] = uv vert local del corner corners[i]
    std::vector<double> p3;       // 3 por uv vert local (posicion 3D)
    std::vector<double> uv;       // 2 por uv vert local
    int nLoc;
    UVChart() : nLoc(0) {}
};

static void BuildCards(const UVMesh& M, const std::vector<int>& uvOf, int nUV, std::vector<UVChart>& cards) {
    const int nF = (int)M.m->faces3d.size();
    UF uf; uf.Init(nF);
    std::vector<int> first((size_t)nUV, -1);
    for (int f = 0; f < nF; f++) {
        if (!M.sel[(size_t)f]) continue;
        const int cnt = (int)M.m->faces3d[(size_t)f].idx.size();
        for (int c = 0; c < cnt; c++) {
            const int u = uvOf[(size_t)(M.start[(size_t)f] + c)];
            if (first[(size_t)u] < 0) first[(size_t)u] = f; else uf.Join(f, first[(size_t)u]);
        }
    }
    std::map<int,int> rootToCard;
    for (int f = 0; f < nF; f++) {
        if (!M.sel[(size_t)f]) continue;
        const int r = uf.Find(f);
        std::map<int,int>::iterator it = rootToCard.find(r);
        if (it == rootToCard.end()) { rootToCard[r] = (int)cards.size(); cards.push_back(UVChart()); it = rootToCard.find(r); }
        cards[(size_t)it->second].faces.push_back(f);
    }
    std::vector<int> loc((size_t)nUV, -1);
    for (size_t ci = 0; ci < cards.size(); ci++) {
        UVChart& C = cards[ci];
        std::vector<int> used;
        for (size_t i = 0; i < C.faces.size(); i++) {
            const int f = C.faces[i];
            const std::vector<int>& idx = M.m->faces3d[(size_t)f].idx; const int cnt = (int)idx.size();
            for (int c = 0; c < cnt; c++) {
                const int k = M.start[(size_t)f] + c, u = uvOf[(size_t)k];
                if (loc[(size_t)u] < 0) {
                    loc[(size_t)u] = C.nLoc++; used.push_back(u);
                    const float* p = Pos(M, idx[(size_t)c]);
                    C.p3.push_back(p[0]); C.p3.push_back(p[1]); C.p3.push_back(p[2]);
                }
                C.corners.push_back(k); C.local.push_back(loc[(size_t)u]);
            }
        }
        for (size_t i = 0; i < used.size(); i++) loc[(size_t)used[i]] = -1;
        C.uv.assign((size_t)C.nLoc * 2, 0.0);
    }
}

// triangulos de la cara f como TERNAS DE CORNERS globales (abanico; ear clipping si es concava)
static void TrianglesFace(const UVMesh& M, int f, std::vector<int>& out) {
    const std::vector<int>& idx = M.m->faces3d[(size_t)f].idx; const int cnt = (int)idx.size(); const int L = M.start[(size_t)f];
    out.clear();
    if (cnt < 3) return;
    if (cnt == 3) { out.push_back(L); out.push_back(L + 1); out.push_back(L + 2); return; }
    if (cnt == 4) {   // quad: por la diagonal mas corta (los no planos se aplanan mejor)
        const double d02 = Dist3(Pos(M, idx[0]), Pos(M, idx[2])), d13 = Dist3(Pos(M, idx[1]), Pos(M, idx[3]));
        if (d02 <= d13) { out.push_back(L); out.push_back(L+1); out.push_back(L+2); out.push_back(L); out.push_back(L+2); out.push_back(L+3); }
        else            { out.push_back(L); out.push_back(L+1); out.push_back(L+3); out.push_back(L+1); out.push_back(L+2); out.push_back(L+3); }
        return;
    }
    std::vector<MeshIndex> tris; W3dTriangulateFace(M.P, idx, tris);
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
struct TriPlane { int v[3]; double x[3], y[3], A, ang[3], gx[3], gy[3]; };

static bool BuildTriPlane(const double* p0, const double* p1, const double* p2, TriPlane& t) {
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
static void TrianglesCard(const UVMesh& M, const UVChart& C, const std::vector<int>& lc, std::vector<TriPlane>& tris) {
    std::vector<int> tc;
    for (size_t i = 0; i < C.faces.size(); i++) {
        TrianglesFace(M, C.faces[i], tc);
        for (size_t t = 0; t + 2 < tc.size(); t += 3) {
            TriPlane tp;
            tp.v[0] = lc[(size_t)tc[t]]; tp.v[1] = lc[(size_t)tc[t+1]]; tp.v[2] = lc[(size_t)tc[t+2]];
            if (tp.v[0] < 0 || tp.v[1] < 0 || tp.v[2] < 0) continue;
            if (tp.v[0] == tp.v[1] || tp.v[1] == tp.v[2] || tp.v[0] == tp.v[2]) continue;
            if (!BuildTriPlane(&C.p3[(size_t)tp.v[0]*3], &C.p3[(size_t)tp.v[1]*3], &C.p3[(size_t)tp.v[2]*3], tp)) continue;
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
struct Sparse {
    std::vector<int> start;        // ini[fila]..ini[fila+1]
    std::vector<int> col;
    std::vector<double> val;
    std::vector<double> rhs;
    int nCol;
    Sparse() : nCol(0) {}
    void MulA(const std::vector<double>& x, std::vector<double>& y) const {
        const size_t nR = rhs.size();
        for (size_t r = 0; r < nR; r++) {
            double s = 0.0;
            for (int k = start[r]; k < start[r+1]; k++) s += val[(size_t)k] * x[(size_t)col[(size_t)k]];
            y[r] = s;
        }
    }
    void MulAT(const std::vector<double>& y, std::vector<double>& x) const {
        x.assign((size_t)nCol, 0.0);
        const size_t nR = rhs.size();
        for (size_t r = 0; r < nR; r++)
            for (int k = start[r]; k < start[r+1]; k++) x[(size_t)col[(size_t)k]] += val[(size_t)k] * y[r];
    }
};

static double Point(const std::vector<double>& a, const std::vector<double>& b) { double s = 0.0; for (size_t i = 0; i < a.size(); i++) s += a[i] * b[i]; return s; }

static void ResolveMinCuad(const Sparse& S, std::vector<double>& x) {
    const size_t nR = S.rhs.size();
    x.assign((size_t)S.nCol, 0.0);
    if (nR == 0 || S.nCol == 0) return;
    std::vector<double> r(S.rhs), s, p, q(nR, 0.0);
    S.MulAT(r, s);
    p = s;
    double gamma = Point(s, s);
    const double tol = gamma * 1e-22;
    int maxIt = 4 * S.nCol + 200; if (maxIt > 20000) maxIt = 20000;
    for (int it = 0; it < maxIt; it++) {
        if (gamma <= tol) break;
        S.MulA(p, q);
        const double qq = Point(q, q);
        if (qq <= 0.0) break;
        const double alpha = gamma / qq;
        for (size_t i = 0; i < x.size(); i++) x[i] += alpha * p[i];
        for (size_t i = 0; i < nR; i++) r[i] -= alpha * q[i];
        S.MulAT(r, s);
        const double gn = Point(s, s);
        if (gn <= tol) break;
        const double beta = gn / gamma;
        for (size_t i = 0; i < p.size(); i++) p[i] = s[i] + beta * p[i];
        gamma = gn;
    }
}

static void FlattenConformal(const std::vector<TriPlane>& tris, int nLoc, const std::vector<double>& p3, std::vector<double>& uv) {
    uv.assign((size_t)nLoc * 2, 0.0);
    if (nLoc < 2 || tris.empty()) return;
    // los dos vertices fijos: extremos del eje mas largo de la caja de la isla
    double mn[3] = { 1e300, 1e300, 1e300 }, mx[3] = { -1e300, -1e300, -1e300 };
    for (int k = 0; k < nLoc; k++) for (int a = 0; a < 3; a++) { const double c = p3[(size_t)k*3 + a]; if (c < mn[a]) mn[a] = c; if (c > mx[a]) mx[a] = c; }
    int axis = 0; for (int a = 1; a < 3; a++) if (mx[a] - mn[a] > mx[axis] - mn[axis]) axis = a;
    int pA = 0, pB = 0;
    for (int k = 1; k < nLoc; k++) { if (p3[(size_t)k*3 + axis] < p3[(size_t)pA*3 + axis]) pA = k; if (p3[(size_t)k*3 + axis] > p3[(size_t)pB*3 + axis]) pB = k; }
    if (pA == pB) pB = (pA + 1) % nLoc;
    double d = 0.0; { const double dx = p3[(size_t)pB*3]-p3[(size_t)pA*3], dy = p3[(size_t)pB*3+1]-p3[(size_t)pA*3+1], dz = p3[(size_t)pB*3+2]-p3[(size_t)pA*3+2]; d = sqrt(dx*dx + dy*dy + dz*dz); }
    if (d < 1e-9) d = 1.0;
    uv[(size_t)pA*2] = 0.0; uv[(size_t)pA*2+1] = 0.0; uv[(size_t)pB*2] = d; uv[(size_t)pB*2+1] = 0.0;
    std::vector<int> colOf((size_t)nLoc, -1); int nCol = 0;
    for (int k = 0; k < nLoc; k++) if (k != pA && k != pB) { colOf[(size_t)k] = nCol; nCol += 2; }
    if (nCol == 0) return;
    Sparse S; S.nCol = nCol;
    for (size_t t = 0; t < tris.size(); t++) {
        const TriPlane& T = tris[t];
        const double sA = sqrt(T.A);
        for (int row = 0; row < 2; row++) {
            S.start.push_back((int)S.col.size()); double b = 0.0;
            for (int i = 0; i < 3; i++) {
                const int k = T.v[i];
                const double cu = (row == 0) ? sA * T.gx[i] : sA * T.gy[i];
                const double cv = (row == 0) ? -sA * T.gy[i] : sA * T.gx[i];
                if (colOf[(size_t)k] >= 0) { S.col.push_back(colOf[(size_t)k]); S.val.push_back(cu); S.col.push_back(colOf[(size_t)k] + 1); S.val.push_back(cv); }
                else b -= cu * uv[(size_t)k*2] + cv * uv[(size_t)k*2+1];
            }
            S.rhs.push_back(b);
        }
    }
    S.start.push_back((int)S.col.size());
    std::vector<double> x; ResolveMinCuad(S, x);
    for (int k = 0; k < nLoc; k++) if (colOf[(size_t)k] >= 0) { uv[(size_t)k*2] = x[(size_t)colOf[(size_t)k]]; uv[(size_t)k*2+1] = x[(size_t)colOf[(size_t)k] + 1]; }
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
static const double kInfinite = 1e30;

static double EnergyTri(const TriPlane& t, const std::vector<double>& uv, int method) {
    double a = 0.0, b = 0.0, c = 0.0, d = 0.0;
    for (int i = 0; i < 3; i++) {
        const double u = uv[(size_t)t.v[i]*2], v = uv[(size_t)t.v[i]*2+1];
        a += t.gx[i] * u; b += t.gy[i] * u; c += t.gx[i] * v; d += t.gy[i] * v;
    }
    const double det = a*d - b*c;
    if (det <= 1e-14) return kInfinite;
    if (method == W3dUnwrapStretch) return t.A * (a*a + b*b + c*c + d*d) * (1.0 + 1.0 / (det * det));
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

static double EnergyVert(const std::vector<int>& I, const std::vector<TriPlane>& tris, const std::vector<double>& uv, int method) {
    double e = 0.0;
    for (size_t i = 0; i < I.size(); i++) { e += EnergyTri(tris[(size_t)I[i]], uv, method); if (e >= kInfinite) return kInfinite; }
    return e;
}

static void Relax(const std::vector<TriPlane>& tris, int nLoc, std::vector<double>& uv, int method) {
    if (tris.empty() || nLoc == 0) return;
    std::vector<std::vector<int> > inc((size_t)nLoc);
    for (size_t t = 0; t < tris.size(); t++) for (int i = 0; i < 3; i++) inc[(size_t)tris[t].v[i]].push_back((int)t);
    const int passes = (tris.size() > 20000) ? 8 : (tris.size() > 5000) ? 15 : 30;
    for (int pass = 0; pass < passes; pass++) {
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
            const double E0 = EnergyVert(I, tris, uv, method);
            if (E0 >= kInfinite) continue;           // ya dado vuelta: no se toca
            const double u = uv[(size_t)k*2], v = uv[(size_t)k*2+1], h = esc * 1e-3;
            uv[(size_t)k*2] = u + h; const double Eu1 = EnergyVert(I, tris, uv, method);
            uv[(size_t)k*2] = u - h; const double Eu0 = EnergyVert(I, tris, uv, method); uv[(size_t)k*2] = u;
            uv[(size_t)k*2+1] = v + h; const double Ev1 = EnergyVert(I, tris, uv, method);
            uv[(size_t)k*2+1] = v - h; const double Ev0 = EnergyVert(I, tris, uv, method); uv[(size_t)k*2+1] = v;
            if (Eu1 >= kInfinite || Eu0 >= kInfinite || Ev1 >= kInfinite || Ev0 >= kInfinite) continue;
            const double gu = (Eu1 - Eu0) / (2.0 * h), gv = (Ev1 - Ev0) / (2.0 * h);
            const double g = sqrt(gu*gu + gv*gv);
            if (!(g > 1e-15)) continue;
            const double du = -gu / g, dv = -gv / g;
            double step = esc * 0.2; bool improved = false;
            for (int j = 0; j < 8; j++) {
                uv[(size_t)k*2] = u + du * step; uv[(size_t)k*2+1] = v + dv * step;
                if (EnergyVert(I, tris, uv, method) < E0) { improved = true; break; }
                step *= 0.5;
            }
            if (!improved) { uv[(size_t)k*2] = u; uv[(size_t)k*2+1] = v; }
        }
    }
}

// ---------------------------------------------------------------------------
//  Empaquetado: cada isla se gira al angulo (de a 15 grados) que achica mas su caja, se ordena
//  por alto y se acomoda en filas (estantes) dentro de [0,1]; la escala comun se busca por
//  biseccion (la mayor con la que todo entra). 'margen' = separacion entre islas (y la mitad
//  contra el borde), en unidades UV.
// ---------------------------------------------------------------------------
struct OrderHeight { const std::vector<double>* H; bool operator()(int a, int b) const { return (*H)[(size_t)a] > (*H)[(size_t)b]; } };

static bool Place(const std::vector<double>& W, const std::vector<double>& H, const std::vector<int>& order, double s, double mg, std::vector<double>& X, std::vector<double>& Y) {
    const double x0 = mg * 0.5, width = 1.0 - mg;
    double x = 0.0, y = 0.0, rowH = 0.0;
    for (size_t o = 0; o < order.size(); o++) {
        const int i = order[o];
        const double bw = W[(size_t)i] * s, bh = H[(size_t)i] * s;
        if (bw > width + 1e-12) return false;
        if (x > 0.0 && x + bw > width + 1e-12) { y += rowH + mg; x = 0.0; rowH = 0.0; }
        X[(size_t)i] = x0 + x; Y[(size_t)i] = x0 + y;
        x += bw + mg; if (bh > rowH) rowH = bh;
    }
    return y + rowH <= width + 1e-12;
}

static void Pack(std::vector<UVChart>& cards, double margin) {
    const int n = (int)cards.size(); if (n == 0) return;
    std::vector<double> W((size_t)n, 0.0), H((size_t)n, 0.0);
    for (int i = 0; i < n; i++) {
        UVChart& C = cards[(size_t)i]; if (C.nLoc == 0) continue;
        double bestArea = 1e300, bestAng = 0.0;
        for (int a = 0; a < 12; a++) {
            const double ang = a * (3.14159265358979 / 12.0), cs = cos(ang), sn = sin(ang);
            double mnx = 1e300, mny = 1e300, mxx = -1e300, mxy = -1e300;
            for (int k = 0; k < C.nLoc; k++) {
                const double u = C.uv[(size_t)k*2], v = C.uv[(size_t)k*2+1];
                const double x = u*cs - v*sn, y = u*sn + v*cs;
                if (x < mnx) mnx = x; if (x > mxx) mxx = x; if (y < mny) mny = y; if (y > mxy) mxy = y;
            }
            const double area = (mxx - mnx) * (mxy - mny);
            if (area < bestArea - 1e-15) { bestArea = area; bestAng = ang; }
        }
        const double cs = cos(bestAng), sn = sin(bestAng);
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
    double mg = margin; if (mg < 0.0) mg = 0.0; if (mg > 0.4) mg = 0.4;
    double maxW = 0.0, maxH = 0.0;
    for (int i = 0; i < n; i++) { if (W[(size_t)i] > maxW) maxW = W[(size_t)i]; if (H[(size_t)i] > maxH) maxH = H[(size_t)i]; }
    const double greater = (maxW > maxH) ? maxW : maxH;
    std::vector<int> order((size_t)n); for (int i = 0; i < n; i++) order[(size_t)i] = i;
    OrderHeight cmp; cmp.H = &H; std::sort(order.begin(), order.end(), cmp);
    std::vector<double> X((size_t)n, 0.0), Y((size_t)n, 0.0);
    double sOk = 0.0;
    if (greater > 1e-12) {
        double lo = 0.0, hi = (1.0 - mg) / greater;
        if (Place(W, H, order, hi, mg, X, Y)) sOk = hi;
        else for (int it = 0; it < 40; it++) { const double mid = 0.5 * (lo + hi); if (Place(W, H, order, mid, mg, X, Y)) { lo = mid; sOk = mid; } else hi = mid; }
    }
    Place(W, H, order, sOk, mg, X, Y);
    for (int i = 0; i < n; i++) {
        UVChart& C = cards[(size_t)i];
        for (int k = 0; k < C.nLoc; k++) { C.uv[(size_t)k*2] = X[(size_t)i] + C.uv[(size_t)k*2] * sOk; C.uv[(size_t)k*2+1] = Y[(size_t)i] + C.uv[(size_t)k*2+1] * sOk; }
    }
}

// escribe las cartas en el mapa UV activo (solo los corners de las caras seleccionadas), con Ctrl+Z
static bool WriteCards(const UVMesh& M, const std::vector<UVChart>& cards, bool flipV) {
    Mesh* m = M.m;
    if (m->uvMaps.empty()) m->PopulateLayers();
    UVMap* um = (m->uvActiveMap >= 0 && m->uvActiveMap < (int)m->uvMaps.size()) ? m->uvMaps[(size_t)m->uvActiveMap] : NULL;
    std::vector<float> uvL((size_t)M.nC * 2, 0.0f);
    if (um && (int)um->uv.size() == M.nC * 2) for (size_t i = 0; i < uvL.size(); i++) uvL[i] = um->uv[i];
    for (size_t ci = 0; ci < cards.size(); ci++) {
        const UVChart& C = cards[ci];
        for (size_t i = 0; i < C.corners.size(); i++) {
            const int k = C.corners[i], l = C.local[i];
            const double u = C.uv[(size_t)l*2], v = C.uv[(size_t)l*2+1];
            uvL[(size_t)k*2] = (float)u; uvL[(size_t)k*2+1] = (float)(flipV ? 1.0 - v : v);
        }
    }
    UndoCaptureMeshGeo(m);
    m->WriteUVProjection(uvL);
    return true;
}

// base ortonormal (t, b) del plano de normal n, con t x b = n (proyeccion bien orientada)
static void BasePlane(const double* n, double* t, double* b) {
    double h[3] = { 0.0, 0.0, 0.0 };
    const double ax = fabs(n[0]), ay = fabs(n[1]), az = fabs(n[2]);
    if (ax <= ay && ax <= az) h[0] = 1.0; else if (ay <= az) h[1] = 1.0; else h[2] = 1.0;
    t[0] = h[1]*n[2] - h[2]*n[1]; t[1] = h[2]*n[0] - h[0]*n[2]; t[2] = h[0]*n[1] - h[1]*n[0];
    const double lt = sqrt(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);
    if (lt > 1e-12) { t[0] /= lt; t[1] /= lt; t[2] /= lt; } else { t[0] = 1.0; t[1] = 0.0; t[2] = 0.0; }
    b[0] = n[1]*t[2] - n[2]*t[1]; b[1] = n[2]*t[0] - n[0]*t[2]; b[2] = n[0]*t[1] - n[1]*t[0];
}

// normal unitaria (Newell) y area de la cara f
static void NormalFace(const UVMesh& M, int f, double* n, double& area) {
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

struct OrderArea { const std::vector<double>* A; bool operator()(int a, int b) const { return (*A)[(size_t)a] > (*A)[(size_t)b]; } };

} // namespace

// ============================================================================
//  Unwrap
// ============================================================================
bool W3dUnwrap(Mesh* m, int method, float margin, std::string& msg, int* islands) {
    UVMesh M; if (!Prepare(m, M, msg, "Unwrap")) return false;
    SideTable T; BuildSides(M, T);
    std::vector<int> uvOf; const int nUV = AssignUVVerts(M, T, true, NULL, uvOf);
    std::vector<UVChart> cards; BuildCards(M, uvOf, nUV, cards);
    // una isla CERRADA (sin borde: ningun lado libre ni costura) no se puede aplanar: colapsa. Si toda la
    // seleccion es asi (un cubo o una esfera sin costuras), se avisa en vez de escribir basura.
    {   std::vector<unsigned char> open(cards.size(), 0);
        std::vector<int> cardOf(m->faces3d.size(), -1);
        for (size_t ci = 0; ci < cards.size(); ci++) for (size_t i = 0; i < cards[ci].faces.size(); i++) cardOf[(size_t)cards[ci].faces[i]] = (int)ci;
        for (SideTable::const_iterator it = T.begin(); it != T.end(); ++it) {
            const std::vector<Side>& ls = it->second;
            const int ci = cardOf[(size_t)ls[0].f]; if (ci < 0) continue;
            bool border = (ls.size() != 2);
            if (!border) { const std::vector<int>& idx = m->faces3d[(size_t)ls[0].f].idx; const int cnt = (int)idx.size();
                          border = IsSeam(M, idx[(size_t)ls[0].c], idx[(size_t)((ls[0].c + 1) % cnt)]); }
            if (border) open[(size_t)ci] = 1;
        }
        bool any = false; for (size_t ci = 0; ci < cards.size(); ci++) if (open[ci]) any = true;
        if (!any) { msg = "Unwrap: closed surface, mark seams first"; return false; }
    }
    std::vector<int> lc((size_t)M.nC, -1);
    for (size_t ci = 0; ci < cards.size(); ci++) {
        UVChart& C = cards[ci];
        for (size_t i = 0; i < C.corners.size(); i++) lc[(size_t)C.corners[i]] = C.local[i];
        std::vector<TriPlane> tris; TrianglesCard(M, C, lc, tris);
        FlattenConformal(tris, C.nLoc, C.p3, C.uv);
        if (method != W3dUnwrapConformal) Relax(tris, C.nLoc, C.uv, method);
        for (size_t i = 0; i < C.corners.size(); i++) lc[(size_t)C.corners[i]] = -1;
    }
    Pack(cards, margin);
    if (islands) *islands = (int)cards.size();
    return WriteCards(M, cards, true);
}

// ============================================================================
//  Smart UV Project
// ============================================================================
bool W3dSmartUVProject(Mesh* m, float angleLimitDeg, float margin, std::string& msg, int* islands) {
    UVMesh M; if (!Prepare(m, M, msg, "Smart UV Project")) return false;
    const int nF = (int)m->faces3d.size();
    std::vector<double> N((size_t)nF * 3, 0.0), Ar((size_t)nF, 0.0);
    std::vector<int> order;
    for (int f = 0; f < nF; f++) if (M.sel[(size_t)f]) { NormalFace(M, f, &N[(size_t)f*3], Ar[(size_t)f]); order.push_back(f); }
    OrderArea cmp; cmp.A = &Ar; std::sort(order.begin(), order.end(), cmp);
    double lim = angleLimitDeg; if (lim < 1.0) lim = 1.0; if (lim > 89.0) lim = 89.0;
    const double cosLim = cos(lim * 3.14159265358979 / 180.0);
    // grupos de proyeccion: la cara mas grande sin grupo abre uno y se lleva a las que miran parecido
    std::vector<int> group((size_t)nF, -1); std::vector<double> gN; int ng = 0;
    for (size_t i = 0; i < order.size(); i++) {
        const int f = order[i]; if (group[(size_t)f] >= 0) continue;
        const int g = ng++; group[(size_t)f] = g;
        const double* nf = &N[(size_t)f*3]; gN.push_back(nf[0]); gN.push_back(nf[1]); gN.push_back(nf[2]);
        for (size_t j = i + 1; j < order.size(); j++) {
            const int h = order[j]; if (group[(size_t)h] >= 0) continue;
            const double* nh = &N[(size_t)h*3];
            if (nf[0]*nh[0] + nf[1]*nh[1] + nf[2]*nh[2] >= cosLim) group[(size_t)h] = g;
        }
    }
    SideTable T; BuildSides(M, T);
    std::vector<int> uvOf; const int nUV = AssignUVVerts(M, T, true, &group, uvOf);
    std::vector<UVChart> cards; BuildCards(M, uvOf, nUV, cards);
    for (size_t ci = 0; ci < cards.size(); ci++) {
        UVChart& C = cards[ci]; if (C.faces.empty()) continue;
        const int g = group[(size_t)C.faces[0]];
        double t[3], b[3]; BasePlane(&gN[(size_t)g*3], t, b);
        for (int k = 0; k < C.nLoc; k++) {
            const double* p = &C.p3[(size_t)k*3];
            C.uv[(size_t)k*2] = p[0]*t[0] + p[1]*t[1] + p[2]*t[2];
            C.uv[(size_t)k*2+1] = p[0]*b[0] + p[1]*b[1] + p[2]*b[2];
        }
    }
    Pack(cards, margin);
    if (islands) *islands = (int)cards.size();
    return WriteCards(M, cards, true);
}

// ============================================================================
//  Lightmap Pack: grilla de celdas iguales, una cara por celda (proyectada sobre su plano)
// ============================================================================
bool W3dLightmapPack(Mesh* m, float margin, std::string& msg) {
    UVMesh M; if (!Prepare(m, M, msg, "Lightmap Pack")) return false;
    const int nF = (int)m->faces3d.size();
    std::vector<UVChart> cards;
    for (int f = 0; f < nF; f++) {
        if (!M.sel[(size_t)f]) continue;
        const std::vector<int>& idx = m->faces3d[(size_t)f].idx; const int cnt = (int)idx.size();
        UVChart C; C.faces.push_back(f); C.nLoc = cnt;
        double n[3], area; NormalFace(M, f, n, area);
        double t[3], b[3]; BasePlane(n, t, b);
        for (int c = 0; c < cnt; c++) {
            const float* p = Pos(M, idx[(size_t)c]);
            C.corners.push_back(M.start[(size_t)f] + c); C.local.push_back(c);
            C.p3.push_back(p[0]); C.p3.push_back(p[1]); C.p3.push_back(p[2]);
            C.uv.push_back(p[0]*t[0] + p[1]*t[1] + p[2]*t[2]);
            C.uv.push_back(p[0]*b[0] + p[1]*b[1] + p[2]*b[2]);
        }
        cards.push_back(C);
    }
    const int n = (int)cards.size();
    int cols = (int)ceil(sqrt((double)n)); if (cols < 1) cols = 1;
    const double cell = 1.0 / cols;
    double mg = margin; if (mg < 0.0) mg = 0.0; if (mg > cell * 0.5) mg = cell * 0.5;
    const double util = cell - mg;
    for (int i = 0; i < n; i++) {
        UVChart& C = cards[(size_t)i];
        double mnx = 1e300, mny = 1e300, mxx = -1e300, mxy = -1e300;
        for (int k = 0; k < C.nLoc; k++) { const double u = C.uv[(size_t)k*2], v = C.uv[(size_t)k*2+1]; if (u < mnx) mnx = u; if (u > mxx) mxx = u; if (v < mny) mny = v; if (v > mxy) mxy = v; }
        double greater = (mxx - mnx > mxy - mny) ? mxx - mnx : mxy - mny; if (greater < 1e-12) greater = 1.0;
        const double s = util / greater;
        const double cx = (i % cols) * cell + cell * 0.5, cy = (i / cols) * cell + cell * 0.5;
        const double bx = 0.5 * (mnx + mxx), by = 0.5 * (mny + mxy);
        for (int k = 0; k < C.nLoc; k++) { C.uv[(size_t)k*2] = cx + (C.uv[(size_t)k*2] - bx) * s; C.uv[(size_t)k*2+1] = cy + (C.uv[(size_t)k*2+1] - by) * s; }
    }
    return WriteCards(M, cards, true);
}

// ============================================================================
//  Follow Active Quads: desde el quad activo (con sus UV como estan), cada quad vecino por un
//  lado compartido recibe ese lado tal cual y sus otros dos corners extrapolados en la misma
//  direccion, a la razon de largos 3D (la grilla sigue derecha y a escala).
// ============================================================================
bool W3dFollowActiveQuads(Mesh* m, std::string& msg, int* quads) {
    UVMesh M; if (!Prepare(m, M, msg, "Follow Active Quads")) return false;
    if (M.active < 0 || !M.sel[(size_t)M.active] || m->faces3d[(size_t)M.active].idx.size() != 4) {
        msg = "Follow Active Quads: the active face must be a selected quad"; return false;
    }
    if (m->uvMaps.empty()) m->PopulateLayers();
    UVMap* um = (m->uvActiveMap >= 0 && m->uvActiveMap < (int)m->uvMaps.size()) ? m->uvMaps[(size_t)m->uvActiveMap] : NULL;
    if (!um || (int)um->uv.size() != M.nC * 2) { msg = "Follow Active Quads: the active face must be a selected quad"; return false; }
    std::vector<float> uvL(um->uv.begin(), um->uv.end());
    SideTable T; BuildSides(M, T);
    std::vector<unsigned char> seen(m->faces3d.size(), 0);
    std::vector<int> queue; queue.push_back(M.active); seen[(size_t)M.active] = 1;
    int done = 0;
    for (size_t qi = 0; qi < queue.size(); qi++) {
        const int f = queue[qi];
        const std::vector<int>& idx = m->faces3d[(size_t)f].idx; const int L = M.start[(size_t)f];
        for (int c = 0; c < 4; c++) {
            const int c1 = (c + 1) % 4;
            const int a = Rep(M, idx[(size_t)c]), b = Rep(M, idx[(size_t)c1]);
            if (a == b) continue;
            SideTable::const_iterator it = T.find(std::make_pair(a < b ? a : b, a < b ? b : a));
            if (it == T.end()) continue;
            for (size_t li = 0; li < it->second.size(); li++) {
                const int g = it->second[li].f;
                if (g == f || seen[(size_t)g] || !M.sel[(size_t)g] || m->faces3d[(size_t)g].idx.size() != 4) continue;
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
                const int Lg = M.start[(size_t)g];
                uvL[(size_t)(Lg+ia)*2] = ua0; uvL[(size_t)(Lg+ia)*2+1] = ua1;
                uvL[(size_t)(Lg+ib)*2] = ub0; uvL[(size_t)(Lg+ib)*2+1] = ub1;
                uvL[(size_t)(Lg+x)*2] = (float)(ua0 + (ua0 - ud0) * r); uvL[(size_t)(Lg+x)*2+1] = (float)(ua1 + (ua1 - ud1) * r);
                uvL[(size_t)(Lg+y)*2] = (float)(ub0 + (ub0 - ue0) * r); uvL[(size_t)(Lg+y)*2+1] = (float)(ub1 + (ub1 - ue1) * r);
                seen[(size_t)g] = 1; queue.push_back(g); done++;
            }
        }
    }
    if (done == 0) { msg = "Follow Active Quads: no neighbouring quads"; return false; }
    UndoCaptureMeshGeo(m);
    m->WriteUVProjection(uvL);
    if (quads) *quads = done;
    return true;
}

// ============================================================================
//  Reset: cada cara seleccionada ocupa toda la textura (quads y triangulos en las esquinas del
//  cuadrado, ngons sobre el circulo inscripto). V=0 arriba, como el resto del engine.
// ============================================================================
bool W3dUVReset(Mesh* m, std::string& msg) {
    UVMesh M; if (!Prepare(m, M, msg, "Reset")) return false;
    if (m->uvMaps.empty()) m->PopulateLayers();
    UVMap* um = (m->uvActiveMap >= 0 && m->uvActiveMap < (int)m->uvMaps.size()) ? m->uvMaps[(size_t)m->uvActiveMap] : NULL;
    std::vector<float> uvL((size_t)M.nC * 2, 0.0f);
    if (um && (int)um->uv.size() == M.nC * 2) for (size_t i = 0; i < uvL.size(); i++) uvL[i] = um->uv[i];
    static const float cuadU[4] = { 0.0f, 1.0f, 1.0f, 0.0f }, cuadV[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        if (!M.sel[f]) continue;
        const int cnt = (int)m->faces3d[f].idx.size(), L = M.start[f];
        for (int c = 0; c < cnt; c++) {
            float u, v;
            if (cnt <= 4) { u = cuadU[c]; v = cuadV[c]; }
            else { const double a = 2.0 * 3.14159265358979 * c / cnt; u = (float)(0.5 + 0.5 * cos(a)); v = (float)(0.5 - 0.5 * sin(a)); }
            uvL[(size_t)(L + c)*2] = u; uvL[(size_t)(L + c)*2+1] = v;
        }
    }
    UndoCaptureMeshGeo(m);
    m->WriteUVProjection(uvL);
    return true;
}

// Smart UV Project de TODAS las caras de la malla (sin Edit Mode ni seleccion): lo usa la pintura de textura cuando la
// malla no tiene UV o las tiene todas pisadas (el cubo por defecto: las 6 caras en el mismo cuadrado)
bool W3dSmartUVProjectAll(Mesh* m, float angleLimitDeg, float margin, std::string& msg) {
    if (!m) return false;
    m->EnsureEdit();
    gUVAllFaces = true;
    const bool ok = W3dSmartUVProject(m, angleLimitDeg, margin, msg);
    gUVAllFaces = false;
    return ok;
}
