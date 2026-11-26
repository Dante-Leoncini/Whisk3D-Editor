#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "Curve.h"
#include "objects/Mesh.h"       // ~Curve: liberar la malla de edicion interna ('edicion')
#include "objects/Materials.h"  // MaterialDefault: el de la geometria generada sin material propio
#include "io/w3dFilesystem.h"   // leer el .cap por el Core (disco / pak / APK)
#include <sstream>
#include <algorithm>
#include <cstdlib>   // strtod/strtol: parsear el .cap sin stringstream (rompe el locale en Symbian)
#include <cstring>   // strncmp
#include "base/w3dlog.h"   // w3dLogf: el .cap ahora carga en Symbian; sus mensajes van al log
#include "WhiskUI/theme/colors.h"
#include "variables.h" // showOverlayGlobal / ViewFromCameraActiveGlobal (estado del editor)
#ifndef W3D_SYMBIAN
    #include <functional>
    #include <limits>
#endif

// ===================================================
// Constructor
// ===================================================
Curve::Curve(Object* parent, Vector3 pos)
    : Object(parent, T("Curve"), pos),
      vertexSize(0), vertex(NULL), indices(NULL), rotNode(NULL), fovNode(NULL),
      signZ(-1.0f), aspect(0.0f), nBranches(0), edit(NULL), kdRoot(NULL)
{
    loadsHandle = -1;   // sin lista de carga hasta LoadLoadList
    // SPLINES: ninguna (curva de nodos). La forma, apagada: una curva nueva es solo su linea
    resolution = 12.0f; bevelDepth = 0.0f; bevelResolution = 4.0f; extrude = 0.0f; fillCaps = false;
    material = NULL; geom = NULL; editing = false; tessVersion = 0;
    for (int i = 0; i < 6; i++) geomKey[i] = -1.0f;   // (nunca armada: la primera UpdateGeometry decide)
    geomVertexPtr = NULL; geomHad = false;

}

// RAMAS = componentes conexas del grafo (aristas explicitas, o la cadena implicita de la
// polilinea). BFS iterativo simple: ~1000 nodos, corre solo al cargar/terminar de editar.
void Curve::RecalculateBranches() {
    nodeBranch.assign(vertexSize > 0 ? vertexSize : 0, -1);
    nBranches = 0;
    if (vertexSize <= 0) return;
    std::vector< std::vector<int> > ady(vertexSize);
    if (!edges.empty()) {
        for (size_t k = 0; k + 1 < edges.size(); k += 2) {
            int a = edges[k], b = edges[k+1];
            if (a >= 0 && a < vertexSize && b >= 0 && b < vertexSize) { ady[a].push_back(b); ady[b].push_back(a); }
        }
    } else {
        for (int i = 0; i + 1 < vertexSize; i++) { ady[i].push_back(i+1); ady[i+1].push_back(i); }
    }
    std::vector<int> queue;
    for (int s = 0; s < vertexSize; s++) {
        if (nodeBranch[s] >= 0) continue;
        int r = nBranches++;
        queue.clear(); queue.push_back(s); nodeBranch[s] = r;
        while (!queue.empty()) {
            int c = queue.back(); queue.pop_back();
            for (size_t k = 0; k < ady[c].size(); k++)
                if (nodeBranch[ady[c][k]] < 0) { nodeBranch[ady[c][k]] = r; queue.push_back(ady[c][k]); }
        }
    }
}

// nearest con FILTRO de ramas (el modificador Oclusion elige que ramas participan:
// la del bonus no sirve al nivel principal). Lineal, comun a PC y N95.
int Curve::FindNearestFiltered(const Vector3& target, const std::vector<char>* branchesOn) const {
    bool filter = branchesOn && !branchesOn->empty() && (int)nodeBranch.size() == vertexSize;
    int best = -1; float bd = 3.4e38f;
    for (int i = 0; i < vertexSize; i++) {
        if (filter) {
            int r = nodeBranch[i];
            if (r >= 0 && r < (int)branchesOn->size() && !(*branchesOn)[r]) continue;
        }
        float dx = vertex[i*3] - target.x, dy = vertex[i*3+1] - target.y, dz = vertex[i*3+2] - target.z;
        float d = dx*dx + dy*dy + dz*dz;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// FOCO/ENCUADRE: bounding de los nodos en MUNDO (centro del AABB + radio que lo envuelve).
Vector3 Curve::PointFocus() const {
    if (!vertex || vertexSize < 1) return GetGlobalPosition();
    Vector3 mn(vertex[0], vertex[1], vertex[2]), mx = mn;
    for (int i = 1; i < vertexSize; i++) {
        if (vertex[i*3]   < mn.x) mn.x = vertex[i*3];   if (vertex[i*3]   > mx.x) mx.x = vertex[i*3];
        if (vertex[i*3+1] < mn.y) mn.y = vertex[i*3+1]; if (vertex[i*3+1] > mx.y) mx.y = vertex[i*3+1];
        if (vertex[i*3+2] < mn.z) mn.z = vertex[i*3+2]; if (vertex[i*3+2] > mx.z) mx.z = vertex[i*3+2];
    }
    return GetGlobalPosition() + (mn + mx) * 0.5f;   // nodos locales + posicion del objeto
}
float Curve::FocusRadius() const {
    if (!vertex || vertexSize < 1) return 0.0f;
    Vector3 c = PointFocus() - GetGlobalPosition();
    float r2 = 0.0f;
    for (int i = 0; i < vertexSize; i++) {
        float dx = vertex[i*3] - c.x, dy = vertex[i*3+1] - c.y, dz = vertex[i*3+2] - c.z;
        float d2 = dx*dx + dy*dy + dz*dz; if (d2 > r2) r2 = d2;
    }
    return sqrtf(r2) + (bevelDepth > 0.0f ? bevelDepth : 0.0f) + (extrude > 0.0f ? extrude : 0.0f);   // (+ el grosor del tubo)
}

// ===================================================
// Tipo de objeto
// ===================================================
ObjectType Curve::getType() {
    return ObjectType::curve;
}

// ===================================================
// Destructor
// ===================================================
static void CurveDeleteKD(KDNode* n){
    if (!n) return;
    CurveDeleteKD(n->left);
    CurveDeleteKD(n->right);
    delete n;
}
Curve::~Curve() {
    delete[] vertex;
    delete[] indices;          // (antes se fugaban indices y el arbol KD)
    delete[] rotNode;           // canales opcionales del riel (pueden ser NULL)
    delete[] fovNode;
    delete edit;             // la malla de edicion interna es de esta curva
    delete geom;             // la malla generada (tubo / cinta), idem
    CurveDeleteKD(kdRoot);
}

// interpolacion angular POR ARCO CORTO, en grados (el yaw del riel cruza el wrap
// 0/360 seguido: 442 de 9672 transiciones en un riel real medido).
static float CurveLerpAng(float a, float b, float t) {
    float d = b - a;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return a + d * t;
}

bool Curve::RotationInNode(float index, Vector3* pitchYawRoll, float* fovOut) const {
    if (!rotNode || vertexSize <= 0) return false;
    const int last = vertexSize - 1;
    if (index < 0.0f) index = 0.0f;
    if (index > (float)last) index = (float)last;
    int base = (int)index;                    // floor: indice ya es >= 0
    if (base > last) base = last;
    float t  = index - (float)base;
    int next = (base + 1 <= last) ? base + 1 : last;
    if (pitchYawRoll) {
        pitchYawRoll->x = CurveLerpAng(rotNode[base*3+0], rotNode[next*3+0], t);
        pitchYawRoll->y = CurveLerpAng(rotNode[base*3+1], rotNode[next*3+1], t);
        pitchYawRoll->z = CurveLerpAng(rotNode[base*3+2], rotNode[next*3+2], t);
    }
    // el FOV es un numero comun (no un angulo con wrap): lerp lineal
    if (fovOut && fovNode) *fovOut = fovNode[base] * (1.0f - t) + fovNode[next] * t;
    return true;
}

// MANIJAS en Edit Mode: una linea del punto a cada manija con el color de su TIPO, los de Blender
// (Free negro, Aligned violeta, Vector verde, Auto amarillo), mas claro si la manija esta elegida.
// Van ENCIMA de todo (sin z): un tubo grueso las taparia justo cuando hay que agarrarlas.
static void CurveDrawHandles(const Curve* c) {
    static const unsigned char kCol[4][2][3] = {
        { {   0,   0,   0 }, {  96,  96,  96 } },   // Free
        { { 128,  48, 128 }, { 240,  92, 171 } },   // Aligned
        { {  64, 143,  48 }, {  64, 255,  64 } },   // Vector
        { { 143, 143,   0 }, { 240, 240,  31 } },   // Auto
    };
    static std::vector<GLfloat> P;   // (estaticos: se reusan cuadro a cuadro sin alocar)
    static std::vector<GLubyte> C;
    P.clear(); C.clear();
    for (size_t s = 0; s < c->splines.size(); s++) {
        const CurveSpline& sp = c->splines[s];
        if (sp.type != W3dSplineBezier) continue;
        for (size_t i = 0; i < sp.points.size(); i++) {
            const CurvePoint& q = sp.points[i];
            for (int side = 0; side < 2; side++) {
                const Vector3& h = side ? q.right : q.left;
                int t = side ? q.typeR : q.typeL; if (t < 0 || t > 3) t = 0;
                const int on = (q.sel & (side ? W3dCurveSelR : W3dCurveSelL)) ? 1 : 0;
                P.push_back(q.co.x); P.push_back(q.co.y); P.push_back(q.co.z);
                P.push_back(h.x); P.push_back(h.y); P.push_back(h.z);
                for (int k = 0; k < 2; k++) { C.push_back(kCol[t][on][0]); C.push_back(kCol[t][on][1]); C.push_back(kCol[t][on][2]); C.push_back(255); }
            }
        }
    }
    if (P.empty()) return;
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::EnableArray(w3dEngine::ColorArray);
    w3dEngine::LineWidth(2);
    w3dEngine::VertexPointer3f(0, &P[0]);
    w3dEngine::ColorPointer4ub(&C[0]);
    w3dEngine::DrawLines((int)(P.size() / 3));
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::LineWidth(1);
    w3dEngine::Enable(w3dEngine::DepthTest);
    if (c->vertex) w3dEngine::VertexPointer3f(0, c->vertex);   // (no dejar el puntero en el buffer estatico)
}

void Curve::RenderObject() {
    // LA GEOMETRIA (tubo / cinta) es una superficie de verdad: sale siempre que la curva se vea (render
    // final, el juego, el telefono), con o sin el overlay "Curves", que solo esconde la LINEA del riel
    UpdateGeometry();
    if (geom) {
        geom->visible = visible; geom->renderable = renderable;
        geom->projectsShadow = projectsShadow; geom->receivesShadow = receivesShadow;
        geom->bouncesLight = bouncesLight; geom->catchesShadows = catchesShadows; geom->inProbes = inProbes;
        geom->select = select && !editing;   // (el contorno de seleccion lo dibuja la malla)
        // EDITANDO, el tubo no escribe z: los puntos y las manijas van por ADENTRO de un cable y si
        // lo escribiera no se podrian ver ni agarrar. (La puerta "sin z" es la del pase del espejo:
        // es la unica que el material de la malla respeta, ver w3dPassMirrorNoZ.)
        const bool noZ = editing && !w3dEngine::w3dPassMirrorNoZ;
        if (noZ) { w3dEngine::Enable(w3dEngine::DepthTest); w3dEngine::w3dPassMirrorNoZ = true; }
        geom->RenderObject();
        if (noZ) { w3dEngine::w3dPassMirrorNoZ = false; w3dEngine::DepthMask(true); }
    }
    { extern bool g_showCurves; if (!g_showCurves) return; } // toggle "Curves" del overlay: ocultar la linea del riel
    // con tubo/cinta la linea del medio sobra (queda adentro); editando si se dibuja: es la que se edita
    if (geom && !editing) return;
#ifdef W3D_SYMBIAN
    if (!vertex || vertexSize < 2) return;
    const float* c;
    if (ActiveObj == this && select) c = ColorList[static_cast<int>(ColorID::accent)];
    else if (select)                 c = ColorList[static_cast<int>(ColorID::accentDark)];
    else                             c = ColorList[static_cast<int>(ColorID::grayUI)];
    w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    GLboolean lightWas = w3dEngine::IsEnabled(w3dEngine::Lighting); // restaurar al salir!
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::LineWidth(2);
    w3dEngine::VertexPointer3f(0, vertex);
    w3dEngine::TexCoordPointer2f(12, vertex); // dummy valido
    if (!edges.empty())                      // GRAFO con ramas: un segmento por arista
        for (size_t k = 0; k + 1 < edges.size(); k += 2)
            w3dEngine::DrawLineStripIndexed(2, &edges[k]);
    else
        w3dEngine::DrawLineStrip(vertexSize);
    w3dEngine::LineWidth(1);
    if (editing) CurveDrawHandles(this);
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (lightWas) w3dEngine::Enable(w3dEngine::Lighting);
    return;
#else
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;
    if (!vertex || vertexSize < 2) return; // curva vacia (LoadFromFile fallido): nada que dibujar (como la rama Symbian)

    if (ActiveObj == this && select){
        w3dEngine::Color4fv(ColorList[static_cast<int>(ColorID::accent)]);
    }
    else if (select){
        w3dEngine::Color4fv(ColorList[static_cast<int>(ColorID::accentDark)]);
    }
    else {
        w3dEngine::Color4fv(ColorList[static_cast<int>(ColorID::grayUI)]);
    }

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::ColorMaterial);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::LineWidth(2);

    w3dEngine::VertexPointer3f(0, vertex);
    if (!edges.empty())                      // GRAFO con ramas: un segmento por arista
        for (size_t k = 0; k + 1 < edges.size(); k += 2)
            w3dEngine::DrawLineStripIndexed(2, &edges[k]);
    else
        w3dEngine::DrawLineStripIndexed(vertexSize, indices);
    if (editing) CurveDrawHandles(this);
#endif // !W3D_SYMBIAN
}

#ifdef W3D_SYMBIAN
// KD-tree deshabilitado en Symbian (la version real usa lambdas/std::function C++11). En su lugar
// FindNearest hace una busqueda LINEAL sobre los nodos del riel: son ~1000 y se corre unas pocas veces
// por frame -> trivial en el N95, y da el MISMO nodo que el arbol (nearest global por LengthSq).
// BuildKDTree queda no-op (kdRoot=NULL, no se usa). Antes FindNearest devolvia -1 (stub): con eso la
// camara con riel NUNCA se ubicaba en el telefono (Camera::UpdatePosition corta en 'if idx < 0 return').
KDNode* Curve::BuildKDTreeRecursive(std::vector<int>&, int) { return NULL; }
void Curve::BuildKDTree() { kdRoot = NULL; }
int Curve::FindNearest(const Vector3& target) const {
    int bestIndex = -1;
    float bestDist = 3.4e38f;   // ~FLT_MAX, sin <limits> (excluido en Symbian)
    for (int i = 0; i < vertexSize; i++) {
        Vector3 p(vertex[i*3+0], vertex[i*3+1], vertex[i*3+2]);
        float d = (p - target).LengthSq();
        if (d < bestDist) { bestDist = d; bestIndex = i; }
    }
    return bestIndex;
}
#else
KDNode* Curve::BuildKDTreeRecursive(std::vector<int>& idx, int depth){
    if (idx.empty()) return NULL;

    int axis = depth % 3;

    std::sort(idx.begin(), idx.end(), [&](int a, int b){
        return vertex[a*3 + axis] < vertex[b*3 + axis];
    });

    int mid = idx.size() / 2;

    KDNode* node = new KDNode();
    node->index = idx[mid];
    node->point = Vector3(
        vertex[node->index*3 + 0],
        vertex[node->index*3 + 1],
        vertex[node->index*3 + 2]
    );

    std::vector<int> left(idx.begin(), idx.begin()+mid);
    std::vector<int> right(idx.begin()+mid+1, idx.end());

    node->left  = BuildKDTreeRecursive(left, depth+1);
    node->right = BuildKDTreeRecursive(right, depth+1);

    return node;
}

void Curve::BuildKDTree(){
    std::cerr << "Creando BuildKDTree\n";
    std::vector<int> idx(vertexSize);
    for (int i=0; i<vertexSize; i++) idx[i] = i;

    kdRoot = BuildKDTreeRecursive(idx, 0);
}

int Curve::FindNearest(const Vector3& target) const{
    float bestDist = std::numeric_limits<float>::infinity();
    int bestIndex = -1;

    std::function<void(KDNode*, int)> search = [&](KDNode* node, int depth){
        if (!node) return;

        float d = (node->point - target).LengthSq();
        if (d < bestDist) {
            bestDist = d;
            bestIndex = node->index;
        }

        int axis = depth % 3;
        float delta = target[axis] - node->point[axis];

        KDNode* nearNode = delta < 0 ? node->left : node->right;
        KDNode* farNode  = delta < 0 ? node->right : node->left;

        search(nearNode, depth+1);

        if (delta * delta < bestDist)
            search(farNode, depth+1);
    };

    search(kdRoot, 0);
    return bestIndex;
}

#endif // !W3D_SYMBIAN

Vector3 Curve::GetPoint(int i) const {
    if (!vertex || i < 0 || i >= vertexSize) {
        return Vector3(0,0,0); // o lanzar error si querés
    }

    int idx = i * 3; // x,y,z
    return Vector3(
        vertex[idx + 0],
        vertex[idx + 1],
        vertex[idx + 2]
    );
}

// ===========================================================================
//  SPLINES BEZIER / POLY (ver el bloque de Curve.h): manijas, teselacion,
//  evaluacion y la geometria generada (tubo / cinta).
// ===========================================================================

// ---- MANIJAS: la cuenta de Blender (calchandleNurb_intern, curve.cc) ----------------------
// Es la MISMA a proposito: una curva que viene de Blender trae sus manijas Auto ya calculadas
// alla; si aca se calcularan distinto, la curva cambiaria de forma apenas se toca un punto.
// prev/next NULL = punta de una spline abierta (se refleja el vecino que hay).
static void CurveCalcHandlePoint(CurvePoint& b, const CurvePoint* prev, const CurvePoint* next) {
    if (b.typeL == W3dHandleFree && b.typeR == W3dHandleFree) return;
    if (!prev && !next) return;   // spline de un solo punto: no hay de donde sacar la direccion
    const Vector3 p2 = b.co;
    Vector3 p1, p3;
    if (prev) p1 = prev->co; else p1 = p2 * 2.0f - next->co;
    if (next) p3 = next->co; else p3 = p2 * 2.0f - p1;
    const Vector3 da = p2 - p1, db = p3 - p2;
    float la = da.Length(), lb = db.Length();
    if (la == 0.0f) la = 1.0f;
    if (lb == 0.0f) lb = 1.0f;
    if (b.typeL == W3dHandleAuto || b.typeR == W3dHandleAuto) {
        const Vector3 tv = db * (1.0f / lb) + da * (1.0f / la);
        const float len = tv.Length() * 2.5614f;   // (la constante de Blender: ~0.39 del tramo)
        if (len != 0.0f) {
            if (b.typeL == W3dHandleAuto) b.left  = p2 - tv * (la / len);
            if (b.typeR == W3dHandleAuto) b.right = p2 + tv * (lb / len);
        }
    }
    if (b.typeL == W3dHandleVector) b.left  = p2 - da * (1.0f / 3.0f);
    if (b.typeR == W3dHandleVector) b.right = p2 + db * (1.0f / 3.0f);
    // ALIGNED: opuesta a la otra, conservando su largo. Con una Free no se alinea nada (Blender #35952)
    if (b.typeL == W3dHandleFree || b.typeR == W3dHandleFree) return;
    if (b.typeL != W3dHandleAligned && b.typeR != W3dHandleAligned) return;
    float ha = (p2 - b.left).Length(), hb = (p2 - b.right).Length();
    if (ha == 0.0f) ha = 1.0f;
    if (hb == 0.0f) hb = 1.0f;
    const float ratio = ha / hb, eps = 1e-5f;
    // el ORDEN lo decide la seleccion: la manija izquierda elegida manda sobre la derecha (es la que
    // el usuario esta moviendo); si no, la derecha manda
    if (b.sel & W3dCurveSelL) {
        if (b.typeR == W3dHandleAligned && ha > eps) b.right = p2 + (p2 - b.left) * (1.0f / ratio);
        if (b.typeL == W3dHandleAligned && hb > eps) b.left  = p2 + (p2 - b.right) * ratio;
    } else {
        if (b.typeL == W3dHandleAligned && hb > eps) b.left  = p2 + (p2 - b.right) * ratio;
        if (b.typeR == W3dHandleAligned && ha > eps) b.right = p2 + (p2 - b.left) * (1.0f / ratio);
    }
}

void Curve::CalcHandlesSpline(int s) {
    if (s < 0 || s >= (int)splines.size()) return;
    CurveSpline& sp = splines[(size_t)s];
    if (sp.type != W3dSplineBezier) return;
    const int n = (int)sp.points.size();
    if (n < 2) return;
    for (int i = 0; i < n; i++) {
        const CurvePoint* prev = (i > 0) ? &sp.points[(size_t)i - 1] : (sp.cyclic ? &sp.points[(size_t)n - 1] : NULL);
        const CurvePoint* next = (i + 1 < n) ? &sp.points[(size_t)i + 1] : (sp.cyclic ? &sp.points[0] : NULL);
        CurveCalcHandlePoint(sp.points[(size_t)i], prev, next);
    }
}
void Curve::CalcHandles() {
    for (int s = 0; s < (int)splines.size(); s++) CalcHandlesSpline(s);
}

// ---- EVALUACION ---------------------------------------------------------------------------
Vector3 Curve::BezierPoint(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
    const float u = 1.0f - t;
    return p0 * (u * u * u) + p1 * (3.0f * u * u * t) + p2 * (3.0f * u * t * t) + p3 * (t * t * t);
}
Vector3 Curve::BezierDerivative(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
    const float u = 1.0f - t;
    return (p1 - p0) * (3.0f * u * u) + (p2 - p1) * (6.0f * u * t) + (p3 - p2) * (3.0f * t * t);
}

int Curve::SegmentCount(int s) const {
    if (!HasSplines()) return vertexSize > 1 ? vertexSize - 1 : 0;   // curva de nodos: la cadena 0-1-2-...
    if (s < 0 || s >= (int)splines.size()) return 0;
    const CurveSpline& sp = splines[(size_t)s];
    const int n = (int)sp.points.size();
    if (n < 2) return 0;
    return sp.cyclic ? n : n - 1;
}

bool Curve::EvalSegment(int s, int seg, float u, Vector3* point, Vector3* tangent) const {
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    Vector3 p, d;
    if (!HasSplines()) {
        if (!vertex || seg < 0 || seg + 1 >= vertexSize) return false;
        const Vector3 a = GetPoint(seg), b = GetPoint(seg + 1);
        p = a + (b - a) * u; d = b - a;
    } else {
        const int nseg = SegmentCount(s);
        if (seg < 0 || seg >= nseg) return false;
        const CurveSpline& sp = splines[(size_t)s];
        const CurvePoint& a = sp.points[(size_t)seg];
        const CurvePoint& b = sp.points[(size_t)((seg + 1) % (int)sp.points.size())];
        if (sp.type == W3dSplineBezier) {
            p = BezierPoint(a.co, a.right, b.left, b.co, u);
            d = BezierDerivative(a.co, a.right, b.left, b.co, u);
            if (d.LengthSq() < 1e-12f) d = b.co - a.co;   // manijas encima del punto: la cuerda
        } else {
            p = a.co + (b.co - a.co) * u; d = b.co - a.co;
        }
    }
    if (point) *point = p;
    if (tangent) *tangent = d.Normalized();
    return true;
}

bool Curve::Evaluate(float t, Vector3* point, Vector3* tangent, int s) const {
    const int n = SegmentCount(s);
    if (n <= 0) {   // un solo punto (o nada)
        Vector3 p; bool ok = false;
        if (HasSplines()) { if (s >= 0 && s < (int)splines.size() && !splines[(size_t)s].points.empty()) { p = splines[(size_t)s].points[0].co; ok = true; } }
        else if (vertex && vertexSize == 1) { p = GetPoint(0); ok = true; }
        if (ok) { if (point) *point = p; if (tangent) *tangent = Vector3(0, 0, 0); }
        return ok;
    }
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const float f = t * (float)n;
    int seg = (int)f;
    if (seg >= n) seg = n - 1;
    return EvalSegment(s, seg, f - (float)seg, point, tangent);
}

// el rango de la teselacion de la spline s: [first, first+count), y si cierra
static bool CurveTessRange(const Curve* c, int s, int& first, int& count, bool& cyclic) {
    if (!c->HasSplines()) { first = 0; count = c->vertexSize; cyclic = false; return c->vertex && count > 0; }
    if (s < 0 || s >= (int)c->splines.size() || s >= (int)c->splineFirst.size()) return false;
    first = c->splineFirst[(size_t)s]; count = c->splineCount[(size_t)s];
    cyclic = c->splines[(size_t)s].cyclic && count >= 3;
    return c->vertex && count > 0 && first + count <= c->vertexSize;
}

float Curve::Length(int s) const {
    if (!vertex || vertexSize < 2) return 0.0f;
    if (!HasSplines() && !edges.empty()) {   // grafo con ramas: la suma de las aristas
        float L = 0.0f;
        for (size_t k = 0; k + 1 < edges.size(); k += 2) L += (GetPoint(edges[k + 1]) - GetPoint(edges[k])).Length();
        return L;
    }
    if (HasSplines() && s < 0) {
        float L = 0.0f;
        for (int i = 0; i < (int)splines.size(); i++) L += Length(i);
        return L;
    }
    int first, count; bool cyc;
    if (!CurveTessRange(this, s, first, count, cyc)) return 0.0f;
    float L = 0.0f;
    for (int i = first; i + 1 < first + count; i++) L += (GetPoint(i + 1) - GetPoint(i)).Length();
    if (cyc) L += (GetPoint(first) - GetPoint(first + count - 1)).Length();
    return L;
}

bool Curve::EvaluateAtLength(float dist, Vector3* point, Vector3* tangent, int s) const {
    int first, count; bool cyc;
    if (!CurveTessRange(this, s, first, count, cyc)) return false;
    if (count == 1) { if (point) *point = GetPoint(first); if (tangent) *tangent = Vector3(0, 0, 0); return true; }
    const int nseg = cyc ? count : count - 1;
    float acc = 0.0f;
    if (dist < 0.0f) dist = 0.0f;
    for (int k = 0; k < nseg; k++) {
        const Vector3 a = GetPoint(first + k), b = GetPoint(first + (k + 1) % count);
        const float len = (b - a).Length();
        if (acc + len >= dist || k == nseg - 1) {
            float u = (len > 1e-12f) ? (dist - acc) / len : 0.0f;
            if (u > 1.0f) u = 1.0f;
            if (point) *point = a + (b - a) * u;
            if (tangent) *tangent = (b - a).Normalized();
            return true;
        }
        acc += len;
    }
    return false;
}

// ---- TESELACION -----------------------------------------------------------------------------
void Curve::Tessellate(bool full) {
    if (splines.empty()) return;   // curva de nodos: 'vertex' ES la verdad, no hay nada que derivar
    int defRes = (int)(resolution + 0.5f);
    if (defRes < 1) defRes = 1;
    if (defRes > 1024) defRes = 1024;
    // TOPE de 16 bits (indices GLushort, el N95): si la resolucion pedida no entra, se divide
    // pareja en todas las splines antes que cortar una a la mitad
    int div = 1;
    for (;;) {
        long total = 0;
        for (size_t s = 0; s < splines.size(); s++) {
            const CurveSpline& sp = splines[s];
            const long np = (long)sp.points.size();
            if (sp.type == W3dSplineBezier && np >= 2) {
                int r = (sp.resolution > 0 ? sp.resolution : defRes) / div; if (r < 1) r = 1;
                total += (sp.cyclic ? np : np - 1) * r + (sp.cyclic ? 0 : 1);
            } else total += np;
        }
        if (total <= 60000 || div > 1024) break;
        div *= 2;
    }
    std::vector<GLfloat> P; std::vector<float> R, Tl;
    splineFirst.assign(splines.size(), 0); splineCount.assign(splines.size(), 0);
    bool needEdges = splines.size() > 1;
    for (size_t s = 0; s < splines.size(); s++) {
        const CurveSpline& sp = splines[s];
        const int first = (int)(P.size() / 3);
        const int np = (int)sp.points.size();
        if (sp.type == W3dSplineBezier && np >= 2) {
            int res = (sp.resolution > 0 ? sp.resolution : defRes) / div; if (res < 1) res = 1;
            const int nseg = sp.cyclic ? np : np - 1;
            for (int g = 0; g < nseg; g++) {
                const CurvePoint& a = sp.points[(size_t)g];
                const CurvePoint& b = sp.points[(size_t)((g + 1) % np)];
                for (int j = 0; j < res; j++) {
                    const float t = (float)j / (float)res;
                    const Vector3 q = BezierPoint(a.co, a.right, b.left, b.co, t);
                    P.push_back(q.x); P.push_back(q.y); P.push_back(q.z);
                    R.push_back(a.radius + (b.radius - a.radius) * t);
                    Tl.push_back(a.tilt + (b.tilt - a.tilt) * t);
                }
            }
            if (!sp.cyclic) {
                const CurvePoint& e = sp.points[(size_t)np - 1];
                P.push_back(e.co.x); P.push_back(e.co.y); P.push_back(e.co.z);
                R.push_back(e.radius); Tl.push_back(e.tilt);
            }
        } else {   // POLY (o una Bezier de un solo punto): los puntos tal cual
            for (int i = 0; i < np; i++) {
                const CurvePoint& e = sp.points[(size_t)i];
                P.push_back(e.co.x); P.push_back(e.co.y); P.push_back(e.co.z);
                R.push_back(e.radius); Tl.push_back(e.tilt);
            }
        }
        splineFirst[s] = first;
        splineCount[s] = (int)(P.size() / 3) - first;
        if (sp.cyclic && splineCount[s] >= 3) needEdges = true;
    }
    const int n = (int)(P.size() / 3);
    delete[] vertex;  vertex = NULL;
    delete[] indices; indices = NULL;
    // los canales por nodo del .cap (rotacion/fov) son de NODOS, no de una teselacion: no mapean
    delete[] rotNode; rotNode = NULL;
    delete[] fovNode; fovNode = NULL;
    vertexSize = n;
    if (n > 0) {
        vertex = new GLfloat[(size_t)n * 3];
        for (size_t i = 0; i < P.size(); i++) vertex[i] = P[i];
        indices = new GLushort[(size_t)n];
        for (int i = 0; i < n; i++) indices[i] = (GLushort)i;
    }
    // ARISTAS: una sola spline abierta es la polilinea IMPLICITA de siempre (edges vacio: el riel y
    // todo lo que lee la cadena 0-1-2 anda igual). Varias splines, o una cerrada, van explicitas.
    edges.clear();
    if (needEdges)
        for (size_t s = 0; s < splines.size(); s++) {
            const int f = splineFirst[s], c = splineCount[s];
            for (int i = 0; i + 1 < c; i++) { edges.push_back((GLushort)(f + i)); edges.push_back((GLushort)(f + i + 1)); }
            if (splines[s].cyclic && c >= 3) { edges.push_back((GLushort)(f + c - 1)); edges.push_back((GLushort)f); }
        }
    evalRadius.swap(R); evalTilt.swap(Tl);
    tessVersion++;
    RecalculateBranches();
    if (full) {
        CurveDeleteKD(kdRoot); kdRoot = NULL;
        if (vertexSize > 0) BuildKDTree();
        UpdateGeometry();
    }
}

// ---- GEOMETRIA GENERADA (tubo / cinta) ---------------------------------------------------------
// giro de v alrededor del eje UNITARIO k (Rodrigues)
static Vector3 CurveRotate(const Vector3& v, const Vector3& k, float ang) {
    const float c = cosf(ang), s = sinf(ang);
    return v * c + Vector3::Cross(k, v) * s + k * (k.Dot(v) * (1.0f - c));
}
// lleva n (perpendicular a ta) a ser perpendicular a tb con el giro MINIMO (transporte paralelo):
// el perfil no se retuerce solo a lo largo del cable
static Vector3 CurveTransport(const Vector3& n, const Vector3& ta, const Vector3& tb) {
    const Vector3 ax = Vector3::Cross(ta, tb);
    const float s = ax.Length(), c = ta.Dot(tb);
    Vector3 r = (s < 1e-7f) ? n : CurveRotate(n, ax * (1.0f / s), atan2f(s, c));
    r = r - tb * r.Dot(tb);   // (re-ortogonalizar: el error se acumula en cables largos)
    const float l = r.Length();
    return (l > 1e-7f) ? r * (1.0f / l) : n;
}

namespace {
struct CurveChain { std::vector<int> idx; bool cyclic; };
}

static void CurveChains(const Curve* c, std::vector<CurveChain>& out) {
    out.clear();
    if (!c->vertex || c->vertexSize < 2) return;
    if (!c->HasSplines()) {   // curva de nodos: la cadena, o cada arista del grafo por separado
        if (c->edges.empty()) {
            CurveChain ch; ch.cyclic = false;
            for (int i = 0; i < c->vertexSize; i++) ch.idx.push_back(i);
            out.push_back(ch);
        } else
            for (size_t k = 0; k + 1 < c->edges.size(); k += 2) {
                CurveChain ch; ch.cyclic = false;
                ch.idx.push_back(c->edges[k]); ch.idx.push_back(c->edges[k + 1]);
                out.push_back(ch);
            }
        return;
    }
    for (size_t s = 0; s < c->splines.size() && s < c->splineFirst.size(); s++) {
        const int f = c->splineFirst[s], n = c->splineCount[s];
        if (n < 2) continue;
        CurveChain ch; ch.cyclic = c->splines[s].cyclic && n >= 3;
        for (int i = 0; i < n; i++) ch.idx.push_back(f + i);
        out.push_back(ch);
    }
}

bool Curve::BuildShapeMesh(Mesh* out) const {
    if (!out) return false;
    const float depth = bevelDepth > 0.0f ? bevelDepth : 0.0f;
    const float ext   = extrude > 0.0f ? extrude : 0.0f;
    if (depth <= 0.0f && ext <= 0.0f) return false;
    std::vector<CurveChain> chains;
    CurveChains(this, chains);
    if (chains.empty()) return false;
    int res = (int)(bevelResolution + 0.5f);
    if (res < 0) res = 0;
    if (res > 31) res = 31;
    const bool tube = depth > 0.0f;
    // lados del perfil: 4 + 2 * resolucion, la MISMA cuenta que el bevel redondo de Blender (curve_bevel: un cable
    // traido de alla tiene la misma cantidad de lados). La cinta: un solo "lado"
    const int S = tube ? 4 + 2 * res : 1;
    const int RV = S + 1;                     // vertices por anillo (el ultimo repite el primero: costura de UV)
    const bool radiusOk = ((int)evalRadius.size() == vertexSize), tiltOk = ((int)evalTilt.size() == vertexSize);
    std::vector<GLfloat> P, UV; std::vector<GLbyte> N; std::vector<MeshFace> F;
    const float perim = tube ? 6.2831853f * depth : 2.0f * ext;
    for (size_t c = 0; c < chains.size(); c++) {
        const std::vector<int>& id = chains[c].idx;
        const bool cyc = chains[c].cyclic;
        const int m = (int)id.size();
        std::vector<Vector3> Pc(m), T(m), Nn(m);
        for (int i = 0; i < m; i++) Pc[(size_t)i] = GetPoint(id[(size_t)i]);
        for (int i = 0; i < m; i++) {
            Vector3 d;
            if (cyc) d = Pc[(size_t)((i + 1) % m)] - Pc[(size_t)((i - 1 + m) % m)];
            else if (i == 0) d = Pc[1] - Pc[0];
            else if (i == m - 1) d = Pc[(size_t)m - 1] - Pc[(size_t)m - 2];
            else d = Pc[(size_t)i + 1] - Pc[(size_t)i - 1];
            if (d.LengthSq() < 1e-14f) d = (i > 0) ? T[(size_t)i - 1] : Vector3(1, 0, 0);
            T[(size_t)i] = d.Normalized();
        }
        // la NORMAL de arranque: el "arriba" del motor (+Y) proyectado. Una curva acostada (un circulo
        // en el piso) queda con la cinta parada, como el Extrude de una curva 2D de Blender
        Vector3 n0 = Vector3(0, 1, 0) - T[0] * T[0].y;
        if (n0.LengthSq() < 1e-8f) n0 = Vector3(0, 0, 1) - T[0] * T[0].z;
        Nn[0] = n0.Normalized();
        for (int i = 1; i < m; i++) Nn[(size_t)i] = CurveTransport(Nn[(size_t)i - 1], T[(size_t)i - 1], T[(size_t)i]);
        // CERRADA: la vuelta completa del transporte no cierra exacto -> el desfasaje se reparte
        // parejo a lo largo (si no, la costura del final queda retorcida)
        std::vector<float> tw(m, 0.0f);
        if (cyc) {
            const Vector3 ne = CurveTransport(Nn[(size_t)m - 1], T[(size_t)m - 1], T[0]);
            const float phi = atan2f(Vector3::Cross(ne, Nn[0]).Dot(T[0]), ne.Dot(Nn[0]));
            for (int i = 0; i < m; i++) tw[(size_t)i] = phi * (float)i / (float)m;
        }
        // largo acumulado (la V de las UV)
        std::vector<float> acc(m + 1, 0.0f);
        for (int i = 1; i < m; i++) acc[(size_t)i] = acc[(size_t)i - 1] + (Pc[(size_t)i] - Pc[(size_t)i - 1]).Length();
        acc[(size_t)m] = acc[(size_t)m - 1] + (cyc ? (Pc[0] - Pc[(size_t)m - 1]).Length() : 0.0f);
        const int rings = cyc ? m + 1 : m;   // cerrada: el anillo final repite el primero (V continua)
        const int base = (int)(P.size() / 3);
        for (int r = 0; r < rings; r++) {
            const int i = r % m;
            const int vi = id[(size_t)i];
            const float rad = radiusOk ? evalRadius[(size_t)vi] : 1.0f;
            // (el anillo final de una cerrada es el punto 0 otra vez: i = 0, tw[0] = 0 -> identico al primero)
            const float twist = tw[(size_t)i] + (tiltOk ? evalTilt[(size_t)vi] : 0.0f);
            const Vector3 nn = CurveRotate(Nn[(size_t)i], T[(size_t)i], twist);
            const Vector3 bb = Vector3::Cross(T[(size_t)i], nn);
            const float v = acc[(size_t)r] / (perim > 1e-6f ? perim : 1.0f);
            for (int j = 0; j < RV; j++) {
                Vector3 off, nor;
                if (tube) {
                    const float a = 6.2831853f * (float)(j % S) / (float)S;
                    const float ca = cosf(a), sa = sinf(a);
                    const float up = sa * depth + (sa > 1e-4f ? ext : (sa < -1e-4f ? -ext : 0.0f));
                    off = (bb * (ca * depth) + nn * up) * rad;
                    nor = bb * ca + nn * sa;
                } else {   // CINTA: arriba / abajo, de cara hacia el costado (B)
                    off = nn * ((j == 0 ? ext : -ext) * rad);
                    nor = bb;
                }
                const Vector3 q = Pc[(size_t)i] + off;
                P.push_back(q.x); P.push_back(q.y); P.push_back(q.z);
                N.push_back((GLbyte)(nor.x * 127.0f)); N.push_back((GLbyte)(nor.y * 127.0f)); N.push_back((GLbyte)(nor.z * 127.0f));
                UV.push_back((float)j / (float)S); UV.push_back(v);
            }
        }
        for (int r = 0; r + 1 < rings; r++)
            for (int j = 0; j + 1 < RV; j++) {
                const int a = base + r * RV + j, b = base + (r + 1) * RV + j;
                MeshFace f;
                if (tube) { f.idx.push_back(a); f.idx.push_back(b); f.idx.push_back(b + 1); f.idx.push_back(a + 1); }
                else      { f.idx.push_back(a + 1); f.idx.push_back(b + 1); f.idx.push_back(b); f.idx.push_back(a); }
                F.push_back(f);
            }
        // TAPAS (tubo abierto): un poligono por punta, con su propia normal plana
        if (tube && fillCaps && !cyc) {
            for (int e = 0; e < 2; e++) {
                const int r = (e == 0) ? 0 : m - 1;
                const Vector3 nc = (e == 0) ? -T[0] : T[(size_t)m - 1];
                const int cb = (int)(P.size() / 3);
                MeshFace f;
                for (int j = 0; j < S; j++) {
                    const int src = base + r * RV + j;
                    P.push_back(P[(size_t)src * 3]); P.push_back(P[(size_t)src * 3 + 1]); P.push_back(P[(size_t)src * 3 + 2]);
                    N.push_back((GLbyte)(nc.x * 127.0f)); N.push_back((GLbyte)(nc.y * 127.0f)); N.push_back((GLbyte)(nc.z * 127.0f));
                    const float a = 6.2831853f * (float)j / (float)S;
                    UV.push_back(cosf(a) * 0.5f + 0.5f); UV.push_back(sinf(a) * 0.5f + 0.5f);
                }
                // (el anillo en orden creciente mira hacia -T: la del principio va asi, la del final al reves)
                for (int j = 0; j < S; j++) f.idx.push_back(e == 0 ? cb + j : cb + (S - 1 - j));
                F.push_back(f);
            }
        }
    }
    const int nV = (int)(P.size() / 3);
    if (nV <= 0 || F.empty()) return false;
#ifdef W3D_SYMBIAN
    if (nV > 65535) { w3dLogfW("[Curve] '%s': la geometria tiene %d vertices (tope 65535 en el telefono)", name.c_str(), nV); return false; }
#endif
    // la malla: arrays propios (sin recurso compartido), caras logicas + su triangulacion en abanico
    delete[] out->vertex; delete[] out->normals; delete[] out->uv; delete[] out->vertexColor; delete[] out->faces;
    out->vertexSize = nV;
    out->vertex = new GLfloat[(size_t)nV * 3];
    out->normals = new GLbyte[(size_t)nV * 3];
    out->uv = new GLfloat[(size_t)nV * 2];
    out->vertexColor = new GLubyte[(size_t)nV * 4];
    for (size_t i = 0; i < P.size(); i++) out->vertex[i] = P[i];
    for (size_t i = 0; i < N.size(); i++) out->normals[i] = N[i];
    for (size_t i = 0; i < UV.size(); i++) out->uv[i] = UV[i];
    for (int i = 0; i < nV * 4; i++) out->vertexColor[i] = 255;
    std::vector<MeshIndex> tris;
    for (size_t f = 0; f < F.size(); f++)
        for (size_t k = 1; k + 1 < F[f].idx.size(); k++) {
            tris.push_back((MeshIndex)F[f].idx[0]); tris.push_back((MeshIndex)F[f].idx[k]); tris.push_back((MeshIndex)F[f].idx[k + 1]);
        }
    out->facesSize = (int)tris.size();
    out->faces = new MeshIndex[tris.empty() ? 1 : tris.size()];
    for (size_t i = 0; i < tris.size(); i++) out->faces[i] = tris[i];
    out->faces3d.swap(F);
    out->looseEdges.clear(); out->looseVerts.clear();
    out->materialsGroup.clear();
    MaterialGroup g; g.startDrawn = 0; g.material = material ? material : MaterialDefault; g.indicesDrawnCount = out->facesSize;
    out->materialsGroup.push_back(g);
    out->meshSmooth = true;
    out->posRep.clear();
    out->ComputeBorders();   // posRep/aristas/AABB (+ sube skinGeomVersion: el VBO se vuelve a subir)
    out->geoVersion++;
    return true;
}

// ¿cambio algo de lo que define la forma desde la ultima vez? (teselacion + parametros)
static bool CurveGeomKeySame(const Curve* c, const float* key) {
    return key[0] == c->bevelDepth && key[1] == c->bevelResolution && key[2] == c->extrude &&
           key[3] == (c->fillCaps ? 1.0f : 0.0f) && key[4] == (float)(c->tessVersion & 0xFFFFFF) &&
           key[5] == (float)c->vertexSize && c->geomVertexPtr == c->vertex;
}

void Curve::UpdateGeometry() {
    if (CurveGeomKeySame(this, geomKey) && (geom != NULL) == geomHad) {
        // (el material se cambia sin rearmar nada)
        if (geom && !geom->materialsGroup.empty()) {
            Material* want = material ? material : MaterialDefault;
            if (geom->materialsGroup[0].material != want) { geom->materialsGroup[0].material = want; geom->geoVersion++; }
        }
        return;
    }
    geomKey[0] = bevelDepth; geomKey[1] = bevelResolution; geomKey[2] = extrude;
    geomKey[3] = fillCaps ? 1.0f : 0.0f; geomKey[4] = (float)(tessVersion & 0xFFFFFF); geomKey[5] = (float)vertexSize;
    geomVertexPtr = vertex;   // (una curva de NODOS editada cambia su arreglo entero: eso tambien la rehace)
    if (bevelDepth <= 0.0f && extrude <= 0.0f) { delete geom; geom = NULL; geomHad = false; return; }
    if (!geom) {
        // nace SUELTA, como una raiz (W3dRootCreating): el ctor de Object no la cuelga de nadie, no le busca
        // nombre y NO TOCA LA SELECCION (si no, crear el tubo -que puede pasar dibujando- le robaba la
        // seleccion al usuario). Es un miembro de la curva, no un hijo
        W3dRootCreating = true;
        geom = new Mesh(NULL, Vector3(0, 0, 0));
        W3dRootCreating = false;
        geom->Parent = this;   // (sin estar en Childrens: ver Curve.h)
        geom->name = name;
    }
    if (!BuildShapeMesh(geom)) { delete geom; geom = NULL; }
    geomHad = (geom != NULL);
}

// DUPLICAR: la copia es independiente (sus propios arreglos); un riel de .cap sigue nombrando el mismo
// archivo y sus canales por nodo viajan con el. La lista de carga (streaming) no: es del riel original.
Curve* CurveDuplicate(Curve* src) {
    if (!src) return NULL;
    Curve* d = new Curve(src->Parent, src->pos);
    d->origin = src->origin; d->signZ = src->signZ; d->aspect = src->aspect;
    d->splines = src->splines;
    d->resolution = src->resolution; d->bevelDepth = src->bevelDepth; d->bevelResolution = src->bevelResolution;
    d->extrude = src->extrude; d->fillCaps = src->fillCaps; d->material = src->material;
    if (d->HasSplines()) { d->Tessellate(true); return d; }
    const int n = src->vertexSize;
    if (n > 0 && src->vertex) {
        d->vertexSize = n;
        d->vertex = new GLfloat[(size_t)n * 3];
        for (int i = 0; i < n * 3; i++) d->vertex[i] = src->vertex[i];
        d->indices = new GLushort[(size_t)n];
        for (int i = 0; i < n; i++) d->indices[i] = (GLushort)i;
        if (src->rotNode) { d->rotNode = new GLfloat[(size_t)n * 3]; for (int i = 0; i < n * 3; i++) d->rotNode[i] = src->rotNode[i]; }
        if (src->fovNode) { d->fovNode = new GLfloat[(size_t)n]; for (int i = 0; i < n; i++) d->fovNode[i] = src->fovNode[i]; }
        d->edges = src->edges;
        d->BuildKDTree();
    }
    d->RecalculateBranches();
    return d;
}

// Carga el .cap del riel. Corre en TODAS las plataformas (antes era un stub `return false` en
// Symbian por un ifstream que ya no existe: hoy lee por w3dFileSystem, que anda en el N95). Sin esto,
// en el telefono el .cap NUNCA se abria -> la Curve se borraba -> la camara con riel quedaba en el
// origen (y el culling, que usa esa camara, fallaba con ella). El parseo es MANUAL con strtod/strtol:
// operator>> de float reinicializa el locale por linea y en Symbian (STLport/RVCT) lee BASURA.
bool Curve::LoadFromFile(const std::string& filepath){
    // POR LA ABSTRACCION DEL CORE, no con ifstream: el riel (.cap) puede venir del
    // pak embebido o de ADENTRO DEL APK. Con ifstream, en Android la camara se
    // quedaba SIN RIEL -- que es exactamente el sintoma de "abre en negro con el
    // HUD encima" que ya se habia pagado una vez en el .deb.
    bool okCap = false;
    const std::string dataCap = w3dFileSystem::ReadTextFile(filepath, &okCap);
    if (!okCap) {
        w3dLogfW("[Curve] no se pudo abrir %s", filepath.c_str());
        return false;
    }
    std::istringstream file(dataCap);

    std::string line;

    // ============================
    // 1) Leer la línea "count X"
    // ============================
    if (!std::getline(file, line)) {
        w3dLogfW("[Curve] archivo vacio: %s", filepath.c_str());
        return false;
    }

    {
        const char* p = line.c_str();
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "count", 5) != 0) {
            w3dLogfW("[Curve] formato invalido: se esperaba 'count' en %s", filepath.c_str());
            return false;
        }
        vertexSize = (int)strtol(p + 5, NULL, 10);   // cantidad de vertices
    }

    if (vertexSize <= 0) {
        w3dLogfW("[Curve] cantidad de vertices invalida en %s", filepath.c_str());
        return false;
    }

    // Reservar memoria (3 floats por vertice). Tambien liberar indices/arbol KD
    // de una carga anterior (antes cada recarga los fugaba).
    if (vertex != NULL)
        delete[] vertex;
    delete[] indices; indices = NULL;
    delete[] rotNode; rotNode = NULL;
    delete[] fovNode; fovNode = NULL;
    CurveDeleteKD(kdRoot); kdRoot = NULL;
    signZ = -1.0f;   // default legacy; lo cambia la cabecera OPCIONAL `ejes` (ver Curve.h)
    aspect = 0.0f;   // encuadre autoral: lo declara la cabecera OPCIONAL `aspecto` (ver Curve.h)

    vertex = new GLfloat[vertexSize * 3];

    // ============================
    // 2) Leer línea por línea: "p x y z", y los canales OPCIONALES del riel
    //    ("r pitch yaw roll" / "f fov"), que van DESPUES de su "p" y valen para
    //    el nodo recien leido. Un .cap sin ellos se lee exactamente como antes.
    // ============================

    int loaded  = 0;
    int withRot = 0, withFov = 0;

    // OJO CON LA CONDICION DE CORTE: NO se puede parar apenas loaded == vertexSize. Las
    // lineas 'r'/'f' del ULTIMO nodo van DESPUES de su 'p', asi que cortar ahi dejaba al
    // ultimo sin rotacion ("el riel trae rotacion en 4 de 5 nodos"). Se recorre el archivo
    // entero y lo que se acota es cuantos PUNTOS se aceptan.
    while (std::getline(file, line)){
        if (line.size() < 2) continue; // evitar líneas vacías

        // CABECERA OPCIONAL DE EJES: "ejes x y -z" (o "ejes x y z"). Va suelta,
        // tipicamente justo despues del `count`. Sin ella queda el default legacy.
        // Se compara la palabra entera para no confundirla con un punto 'e...'.
        if (line.compare(0, 4, "axes") == 0 &&
            (line.size() == 4 || line[4] == ' ' || line[4] == '\t')) {
            signZ = (line.find("-z") != std::string::npos ||
                      line.find("-Z") != std::string::npos) ? -1.0f : 1.0f;
            continue;
        }

        // CABECERA OPCIONAL DE ENCUADRE: "aspecto <ancho/alto>" (ej: 1.481481 =
        // 40:27). Es el aspecto AUTORAL del riel (ver Curve.h). Palabra entera,
        // como `ejes`, para no confundirla con un punto 'a...'.
        if (line.compare(0, 6, "aspect") == 0 &&
            (line.size() == 6 || line[6] == ' ' || line[6] == '\t')) {
            float v = (float)strtod(line.c_str() + 6, NULL);
            aspect  = (v > 0.01f && v < 100.0f) ? v : 0.0f;
            continue;
        }

        // PARSEO MANUAL con strtod (NO stringstream/operator>>: reinicializa el locale por linea y
        // en Symbian lee basura -> el riel quedaba deformado). Mismo criterio que import_wobj.cpp.
        const char* p = line.c_str();
        while (*p == ' ' || *p == '\t') p++;
        char type = *p;
        char* e; const char* q = p + 1;

        if (type == 'r') {                       // rotacion AUTORAL del nodo anterior
            if (loaded <= 0) continue;           // una 'r' antes del primer punto no es de nadie
            double rx = strtod(q, &e); double ry = strtod(e, &e); double rz = strtod(e, &e);
            if (!rotNode) {                          // primera 'r': recien ahi se paga la memoria
                rotNode = new GLfloat[vertexSize * 3];
                for (int i = 0; i < vertexSize * 3; i++) rotNode[i] = 0.0f;
            }
            rotNode[(loaded - 1) * 3 + 0] = (GLfloat)rx;
            rotNode[(loaded - 1) * 3 + 1] = (GLfloat)ry;
            rotNode[(loaded - 1) * 3 + 2] = (GLfloat)rz;
            withRot++;
            continue;
        }
        if (type == 'f') {                       // fov del nodo anterior
            if (loaded <= 0) continue;
            double fv = strtod(q, &e);
            if (!fovNode) {
                fovNode = new GLfloat[vertexSize];
                for (int i = 0; i < vertexSize; i++) fovNode[i] = 0.0f;
            }
            fovNode[loaded - 1] = (GLfloat)fv;
            withFov++;
            continue;
        }

        if (type != 'p')
            continue; // ignoramos líneas que no empiezan con "p"
        if (loaded >= vertexSize)
            continue; // el 'count' de la cabecera manda: los puntos de mas se descartan

        double px = strtod(q, &e); double py = strtod(e, &e); double pz = strtod(e, &e);
        vertex[loaded * 3 + 0] = (GLfloat)px;
        vertex[loaded * 3 + 1] = (GLfloat)py;
        vertex[loaded * 3 + 2] = signZ * (GLfloat)pz;   // ver Curve::signZ

        loaded++;
    }

    // (el buffer se libera solo: ya no hay archivo abierto que cerrar)

    // canales incompletos: no se adivina nada. Si el .cap trae rotacion para
    // algunos nodos y para otros no, los que falten quedan en 0 (y se avisa).
    if (rotNode && withRot != loaded)
        w3dLogfW("[Curve] rotacion en %d de %d nodos (los que falten quedan en 0)", withRot, loaded);
    if (fovNode && withFov != loaded)
        w3dLogfW("[Curve] fov en %d de %d nodos", withFov, loaded);

    if (loaded != vertexSize) {
        w3dLogfW("[Curve] se esperaban %d nodos pero se leyeron %d", vertexSize, loaded);
        vertexSize = loaded;
    }

    indices = new GLushort[vertexSize];
    for (int i=0; i < vertexSize; i++)
        indices[i] = i;

    w3dLogf("[Curve] cargada: %d nodos (%s)", vertexSize, filepath.c_str());

    origin = filepath;   // el .w3d guarda esta ruta para recargar la curva al abrir

    BuildKDTree();

    return true;
}



// ===========================================================================
//  LISTA DE CARGA: parseo del sidecar `<riel>.cargas.json` y registro en el
//  almacen de recursos (ver formato/cargas-json.md y io/W3dResources.h).
//  Ids con prefijo de tipo: "textura:...", "malla:...", "anim:...", "vis:...",
//  "audio:...", "otro:..." (sin prefijo = otro). En Symbian este camino
//  todavia no corre (como LoadFromFile, que tampoco pasa por el VFS).
// ===========================================================================
#ifndef W3D_SYMBIAN
#include "io/JsonW3d.h"
#include "io/W3dResources.h"
#include "w3dFilesystem.h"

static int LoadsTypeOf(std::string& id) {
    const size_t p = id.find(':');
    if (p == std::string::npos) return W3DREC_OTHER;
    const std::string pref = id.substr(0, p);
    int type = -1;
    if      (pref == "texture") type = W3DREC_TEXTURE;
    else if (pref == "mesh")   type = W3DREC_MESH;
    else if (pref == "anim")    type = W3DREC_ANIM;
    else if (pref == "vis")     type = W3DREC_LIST_VIS;
    else if (pref == "audio")   type = W3DREC_AUDIO;
    else if (pref == "other")    type = W3DREC_OTHER;
    else if (pref == "animset") type = W3DREC_ANIMSET;   // clips de esqueleto (.w3da): id = su entrada
    if (type < 0) return W3DREC_OTHER;      // prefijo desconocido: id entero, tipo otro
    id = id.substr(p + 1);
    return type;
}

static bool LoadsReadDirection(JVal* root, const char* key,
                                std::vector<W3dLoadsRow>& out) {
    out.clear();
    if (!root || root->type != 4) return false;
    std::map<std::string, JVal*>::iterator it = root->obj.find(key);
    if (it == root->obj.end() || !it->second || it->second->type != 5) return true; // direccion ausente = vacia
    JVal* list = it->second;
    // las filas vienen ORDENADAS por nodo; se materializa una fila por nodo
    // 0..max (nodo sin fila = fila vacia), asi el indice de fila ES el nodo.
    int maxNode = -1;
    for (size_t i = 0; i < list->list.size(); i++) {
        JVal* f = list->list[i];
        if (!f || f->type != 4) continue;
        std::map<std::string, JVal*>::iterator jn = f->obj.find("node");
        if (jn == f->obj.end() || !jn->second || jn->second->type != 1) continue;
        const int n = (int)jn->second->num;
        if (n > maxNode) maxNode = n;
    }
    if (maxNode < 0) return true;
    out.resize((size_t)maxNode + 1);
    for (size_t i = 0; i < list->list.size(); i++) {
        JVal* f = list->list[i];
        if (!f || f->type != 4) continue;
        std::map<std::string, JVal*>::iterator jn = f->obj.find("node");
        if (jn == f->obj.end() || !jn->second || jn->second->type != 1) continue;
        const int n = (int)jn->second->num;
        if (n < 0 || n > maxNode) continue;
        std::map<std::string, JVal*>::iterator ji = f->obj.find("ids");
        if (ji == f->obj.end() || !ji->second || ji->second->type != 5) continue;
        for (size_t k = 0; k < ji->second->list.size(); k++) {
            JVal* e = ji->second->list[k];
            if (!e || e->type != 2 || e->str.empty()) continue;   // 2 = string (JsonW3d)
            W3dLoadsItem item;
            std::string id = e->str;
            item.type = LoadsTypeOf(id);
            item.id = id;
            out[(size_t)n].push_back(item);
        }
    }
    return true;
}

bool Curve::LoadLoadList(const std::string& path) {
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(path.c_str(), data) || data.empty()) {
        std::cerr << "[ListaCarga] no pude leer " << path << "\n";
        return false;
    }
    JParser pair((const char*)&data[0], data.size());
    JVal* root = pair.Value();
    std::vector<W3dLoadsRow> forward, back;
    bool ok = root && root->type == 4;
    if (ok) {
        LoadsReadDirection(root, "forward", forward);
        LoadsReadDirection(root, "back", back);
    } else {
        std::cerr << "[ListaCarga] JSON invalido: " << path << "\n";
    }
    delete root;
    if (!ok) return false;
    loadsHandle = W3dLoadsRegister(name, forward, back);
    loadsFile = path;
    return true;
}
#else
bool Curve::LoadLoadList(const std::string&) { return false; }
#endif
