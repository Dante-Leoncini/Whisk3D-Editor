// ============================================================================
//  WEIGHT PAINT (Fase 2): pincel reutilizable + escritura de pesos por
//  control-point. Ver WeightPaint.h para el contrato. C++03 (compila en Symbian).
// ============================================================================
#include "edit/WeightPaint.h"
#include "edit/MeshEdit.h"      // CreateVertexGroup / CreateUVGroup (grupo automatico al primer trazo)
#include "objects/Mesh.h"       // Mesh, VertexGroup, UVGroup, vertCtrlPoint, posRep
#include "objects/EditMesh.h"   // faceSel/faceSrc (mascara "solo lo seleccionado")
#include "objects/Objects.h"    // g_editMesh (Assign/Select de vertex groups: seleccion de Edit Mode)
#include "Undo.h"               // UndoWeightsStart / UndoWeightsConfirm (un undo por TRAZO)
#include "w3dGraphics.h"        // dibujo del circulo (DrawLines)
#include "w3dlog.h"             // aviso al crear el grupo automatico
#include "W3dLang.h"            // T(): titulos de los menus en el idioma del sistema
#include "W3dPalettes.h"        // vertex color por INDICE: la paleta efectiva del objeto
#include "WhiskUI/widgets/PopupMenu.h" // menus deslizables (AddFloat) de la toolbar
#include <math.h>               // sqrtf / cosf / sinf
#include <cstdio>               // sprintf (labels de la toolbar)

namespace gfx = w3dEngine;
extern bool g_redraw;

// ---------------------------------------------------------------------------
//  PINCEL (estado global unico; separado de los pesos para reusar en texturas/escultura)
// ---------------------------------------------------------------------------
static BrushState g_brush;
BrushState& BrushGet() { return g_brush; }

// TOPE POR TRAZO (ver WeightPaint.h): cuanto recibio YA cada elemento en este trazo. El indice
// es el del pincel que este corriendo (control-point, render-vert o corner): un trazo es de UN
// pincel solo, asi que un vector alcanza. Se vacia al arrancar cada trazo.
static std::vector<float> g_strokeApply;
void BrushStrokeReset() { g_strokeApply.clear(); }

// cuanto FALTA aplicarle al elemento i para llegar a 'a' en este trazo (0 = ya recibio eso o
// mas). Deja anotado el nuevo maximo. 'n' es el tamano del dominio (para dimensionar el vector).
static float PendingStroke(size_t i, size_t n, float a) {
    if (g_strokeApply.size() != n) g_strokeApply.assign(n, 0.0f);
    if (i >= n) return a;
    const float already = g_strokeApply[i];
    if (a <= already) return 0.0f;      // ya se paso por aca con igual o mas fuerza: no suma
    g_strokeApply[i] = a;
    return a - already;
}

// El modo "=" (valor exacto en todo el circulo) no degrada con la distancia: eso es un falloff
// CONSTANTE, asi que en vez de meter un "if" adentro del pincel se cambia la curva y el pincel
// sigue siendo uno solo. Las MARCAS no cambian el falloff: son solo donde se dibuja/testea cada
// punto (antes las marcas forzaban constante, y con las marcas prendidas por defecto en Vertex
// Paint el falloff "no andaba": rozar un corner lo pintaba entero).
const W3dFalloff& EffectiveBrushFalloff() {
    static W3dFalloff constant;          // se arma una vez
    constant.type = FoConstant;
    return (g_brush.mode == WPEqualize) ? constant : g_brush.falloff;
}

// circulo por SEGMENTOS DE LINEA (no habia helper de circulo en el motor): 48 segmentos,
// dos pasadas -> halo NEGRO grueso abajo + linea BLANCA fina arriba (se lee sobre
// cualquier fondo, mismo criterio que el cursor del editor UV).
void BrushDrawCircle(float cx, float cy, float radiusPx) {
    if (radiusPx <= 0.0f) return;
    const int N = 48;
    static float buf[N * 4]; // N segmentos = N pares de puntos (x,y)
    const float step = 6.2831853f / (float)N;
    for (int i = 0; i < N; i++) {
        float a0 = step * (float)i, a1 = step * (float)(i + 1);
        buf[i*4+0] = cx + cosf(a0) * radiusPx; buf[i*4+1] = cy + sinf(a0) * radiusPx;
        buf[i*4+2] = cx + cosf(a1) * radiusPx; buf[i*4+3] = cy + sinf(a1) * radiusPx;
    }
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::VertexPointer2f(0, buf);
    gfx::LineWidth(3.0f); gfx::Color4f(0.0f, 0.0f, 0.0f, 1.0f); gfx::DrawLines(N * 2); // halo oscuro
    gfx::LineWidth(1.0f); gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f); gfx::DrawLines(N * 2); // circulo blanco
}

// ---------------------------------------------------------------------------
//  MARCAS: un cuadradito por punto pintable (relleno + borde negro).
//  Dos modos con el MISMO dibujo y distinta posicion (ver W3dMarksMode):
//    MarksVertex -> uno por control-point, EXACTO sobre el vertice   (pesos)
//    MarksCorner  -> uno por face corner, corrido hacia el centro de SU cara
//                     para que los corners que comparten esquina no se tapen (vertex color)
//  Todo en 2D de PANTALLA: las posiciones salen del proyector (el mismo que usa el
//  pincel para decidir a quien pinta), asi lo que se VE y lo que se PINTA no pueden
//  desalinearse -- y el mismo codigo sirve en el viewport 3D y en el editor UV.
// ---------------------------------------------------------------------------
// CUANTO se corre un face corner hacia el centro de su cara, como fraccion. Es lo que despega
// los 2-3 corners que comparten una esquina para que se vean -- y se puedan APUNTAR -- por
// separado. Vive en UN solo lugar porque lo usan el DIBUJO de los cuadraditos y el HIT-TEST del
// pincel: si divergieran, apuntarias a un cuadrado y pintarias otro (que es exactamente el bug
// que habia: se dibujaba corrido y se testeaba sobre el vertice crudo, donde los 3 corners caen
// en el MISMO punto -> el pincel agarraba los 3 juntos).
static const float kCornerInset = 0.25f;

// posicion EN PANTALLA de cada corner, en el orden de faces3d (el mismo que indexa las capas).
// ok[L] = 0 si ese corner no se ve (detras de camara / back-facing): no se dibuja ni se pinta.
// Una cara entra ENTERA o no entra: si un corner no proyecta, la cara se saltea (sino se
// dibujarian esquinas sueltas de una cara que no se ve).
// ---------------------------------------------------------------------------
//  Oclusion (ver WeightPaint.h)
// ---------------------------------------------------------------------------
static const unsigned char* g_oclRGBA = NULL;
static int g_oclX0 = 0, g_oclY0 = 0, g_oclW = 0, g_oclH = 0, g_oclVpX = 0, g_oclVpY = 0, g_oclHeight = 0;
static const Mesh* g_oclMesh = NULL;

void WPOcclusionSet(const unsigned char* rgba, int x0, int y0, int w, int h, int vpX, int vpY, int screenHeight, Mesh* m) {
    g_oclRGBA = rgba; g_oclX0 = x0; g_oclY0 = y0; g_oclW = w; g_oclH = h;
    g_oclVpX = vpX; g_oclVpY = vpY; g_oclHeight = screenHeight; g_oclMesh = m;
}
void WPOcclusionClear() { g_oclRGBA = NULL; g_oclMesh = NULL; }
bool WPOcclusionActive(const Mesh* m) { return g_oclRGBA != NULL && g_oclMesh == m && g_oclW > 0 && g_oclH > 0; }
int WPOcclusionFaceIn(float sx, float sy) {
    if (!g_oclRGBA) return -1;
    const int px = (int)floorf((float)g_oclVpX + sx + 0.5f) - g_oclX0;                 // columna de ventana
    const int py = (g_oclHeight - 1 - (int)floorf((float)g_oclVpY + sy + 0.5f)) - g_oclY0; // fila GL (desde abajo)
    if (px < 0 || py < 0 || px >= g_oclW || py >= g_oclH) return -1;
    const unsigned char* q = g_oclRGBA + ((size_t)py * (size_t)g_oclW + (size_t)px) * 4;
    // misma codificacion 5-6-5 que el picking por color (sobrevive a un framebuffer de 16 bits)
    const int id = (q[0] >> 3) | ((q[1] >> 2) << 5) | ((q[2] >> 3) << 11);
    return id - 1;
}

void WPRenderVisibleVerts(Mesh* m, WPProjector proj, void* ctx, std::vector<char>& vis) {
    const int nV = m ? m->vertexSize : 0;
    vis.assign((size_t)(nV > 0 ? nV : 0), 0);
    if (nV <= 0) return;
    for (int i = 0; i < nV; i++) { float sx, sy; vis[(size_t)i] = proj(ctx, i, sx, sy) ? 1 : 0; }
    if (!WPOcclusionActive(m)) return;
    // con mapa: ademas alguna esquina del vert tiene que verse de verdad
    std::vector<float> cx, cy; std::vector<char> ok;
    WPCornersInScreen(m, proj, ctx, cx, cy, ok);
    std::vector<char> any((size_t)nV, 0);
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++)
        for (size_t k = 0; k < m->faces3d[f].idx.size(); k++, L++) {
            const int rv = m->faces3d[f].idx[k];
            if (rv >= 0 && rv < nV && L < (int)ok.size() && ok[(size_t)L]) any[(size_t)rv] = 1;
        }
    for (int i = 0; i < nV; i++) if (!any[(size_t)i]) vis[(size_t)i] = 0;
}

void WPCornersInScreen(Mesh* m, WPProjector proj, void* ctx,
                                std::vector<float>& cx, std::vector<float>& cy,
                                std::vector<char>& ok) {
    const int nC = m->CountCorners();
    const size_t n0 = (size_t)(nC > 0 ? nC : 0);
    cx.assign(n0, 0.0f); cy.assign(n0, 0.0f); ok.assign(n0, 0);
    if (nC <= 0) return;
    const bool occlusion = WPOcclusionActive(m);
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& idx = m->faces3d[f].idx;
        const size_t n = idx.size();
        if (L + (int)n > nC) break;
        if (n < 3) { L += (int)n; continue; }
        static std::vector<float> px, py;
        px.clear(); py.clear();
        float sumX = 0.0f, sumY = 0.0f;
        bool all   = true;
        for (size_t k = 0; k < n; k++) {
            float sx = 0.0f, sy = 0.0f;
            const int rv = idx[k];
            if (rv < 0 || rv >= m->vertexSize || !proj(ctx, rv, sx, sy)) { all = false; break; }
            px.push_back(sx); py.push_back(sy);
            sumX += sx; sumY += sy;
        }
        if (all) {
            const float ccx = sumX / (float)n, ccy = sumY / (float)n;  // centro de la cara EN PANTALLA
            for (size_t k = 0; k < n; k++) {
                const size_t Lk = (size_t)(L + (int)k);
                cx[Lk] = px[k] + (ccx - px[k]) * kCornerInset;
                cy[Lk] = py[k] + (ccy - py[k]) * kCornerInset;
                // tapado por otra cara? el pixel del cuadradito tiene que mostrar ESTA cara
                ok[Lk] = (!occlusion || WPOcclusionFaceIn(cx[Lk], cy[Lk]) == (int)f) ? 1 : 0;
            }
        }
        L += (int)n;
    }
}

static void MarkPush(std::vector<float>& tri, float cx, float cy, float half) {
    const float x0 = cx - half, x1 = cx + half;
    const float y0 = cy - half, y1 = cy + half;
    // 2 triangulos (sin index buffer: son pocos y se dibujan con DrawTrianglesArray)
    tri.push_back(x0); tri.push_back(y0);  tri.push_back(x1); tri.push_back(y0);  tri.push_back(x1); tri.push_back(y1);
    tri.push_back(x0); tri.push_back(y0);  tri.push_back(x1); tri.push_back(y1);  tri.push_back(x0); tri.push_back(y1);
}
static void MarkColor(std::vector<unsigned char>& col, const unsigned char* c) {
    for (int k = 0; k < 6; k++) {          // los 6 vertices del cuadrado, mismo color
        if (c) { col.push_back(c[0]); col.push_back(c[1]); col.push_back(c[2]); col.push_back(255); }
        else   { col.push_back(170); col.push_back(170); col.push_back(170); col.push_back(255); }
    }
}

void BrushDrawMarks(Mesh* m, int mode, float sidePx, const unsigned char* colorRV,
                        WPProjector proj, void* ctx) {
    if (!m || !proj || mode == MarksOff || m->vertexSize <= 0 || sidePx <= 1.0f) return;
    const float half = sidePx * 0.5f;
    const float halfBorder = half + 1.0f * (float)GlobalScale; // el borde negro asoma alrededor

    static std::vector<float> triBackground, triFill;
    static std::vector<unsigned char> colFill;
    triBackground.clear(); triFill.clear(); colFill.clear();

    if (mode == MarksVertex) {
        // UNO POR CONTROL-POINT: el peso es del vertice, no del corner, asi que dos corners
        // del mismo vertice no pueden mostrar cosas distintas -> se dibuja uno solo.
        WeightPaintEnsureMap(m);
        int maxCP = -1;
        for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
            if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
        if (maxCP < 0) return;
        std::vector<char> vis; WPRenderVisibleVerts(m, proj, ctx, vis);   // back-facing + tapados
        std::vector<char> seen((size_t)maxCP + 1, 0);
        for (int i = 0; i < m->vertexSize && i < (int)m->vertCtrlPoint.size(); i++) {
            const int cp = m->vertCtrlPoint[i];
            if (cp < 0 || cp > maxCP || seen[(size_t)cp]) continue;
            if (i >= (int)vis.size() || !vis[(size_t)i]) continue;
            float sx = 0.0f, sy = 0.0f;
            if (!proj(ctx, i, sx, sy)) continue;   // detras de camara / back-facing: no se ve
            seen[(size_t)cp] = 1;
            MarkPush(triBackground, sx, sy, halfBorder);
            MarkPush(triFill, sx, sy, half);
            MarkColor(colFill, colorRV ? &colorRV[(size_t)i * 4] : NULL);
        }
    } else {
        // UNO POR FACE CORNER: la posicion sale del helper COMPARTIDO con el pincel, asi el
        // cuadradito que ves es exactamente el punto que el pincel testea.
        std::vector<float> cx, cy; std::vector<char> ok;
        WPCornersInScreen(m, proj, ctx, cx, cy, ok);
        int L = 0;
        for (size_t f = 0; f < m->faces3d.size(); f++)
            for (size_t k = 0; k < m->faces3d[f].idx.size(); k++, L++) {
                if (L >= (int)ok.size() || !ok[(size_t)L]) continue;
                MarkPush(triBackground,   cx[(size_t)L], cy[(size_t)L], halfBorder);
                MarkPush(triFill, cx[(size_t)L], cy[(size_t)L], half);
                MarkColor(colFill, colorRV ? &colorRV[(size_t)m->faces3d[f].idx[k] * 4] : NULL);
            }
    }
    if (triFill.empty()) return;

    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    // 1) el BORDE: los mismos cuadrados un poquito mas grandes, en negro, abajo de todo
    gfx::DisableArray(gfx::ColorArray);
    gfx::Color4f(0.0f, 0.0f, 0.0f, 1.0f);
    gfx::VertexPointer2f(0, &triBackground[0]);
    gfx::DrawTrianglesArray((int)(triBackground.size() / 2));
    // 2) el RELLENO: el color de cada punto (el peso pintado, o el vertex color)
    gfx::EnableArray(gfx::ColorArray);
    gfx::ColorPointer4ub(&colFill[0]);
    gfx::VertexPointer2f(0, &triFill[0]);
    gfx::DrawTrianglesArray((int)(triFill.size() / 2));
    gfx::DisableArray(gfx::ColorArray);
}

// ---------------------------------------------------------------------------
//  PESOS por control-point (sparse en VertexGroup::verts/pesos)
// ---------------------------------------------------------------------------
float WeightOf(Mesh* m, int group, int cp) {
    if (!m || group < 0 || group >= (int)m->vertexGroups.size() || cp < 0) return 0.0f;
    VertexGroup* vg = m->vertexGroups[group];
    for (size_t j = 0; j < vg->verts.size() && j < vg->weights.size(); j++)
        if (vg->verts[j] == cp) return vg->weights[j];
    return 0.0f;
}

void WeightAssign(Mesh* m, int group, int cp, float w) {
    if (!m || group < 0 || group >= (int)m->vertexGroups.size() || cp < 0) return;
    if (w > 1.0f) w = 1.0f;
    VertexGroup* vg = m->vertexGroups[group];
    for (size_t j = 0; j < vg->verts.size(); j++) {
        if (vg->verts[j] != cp) continue;
        if (w <= 0.0f) { // peso 0 = BORRAR la entrada sparse (no acumular basura)
            vg->verts.erase(vg->verts.begin() + j);
            if (j < vg->weights.size()) vg->weights.erase(vg->weights.begin() + j);
        } else if (j < vg->weights.size()) vg->weights[j] = w;
        return;
    }
    if (w > 0.0f) { vg->verts.push_back(cp); vg->weights.push_back(w); } // entrada nueva
}

// ---------------------------------------------------------------------------
//  PESOS del UV GROUP (sparse por RENDER-VERT / CORNER, Mesh::uvGroups).
//  Los escribe el pincel del editor UV; los leen Armature2DApply (skinning 2D) y el relleno
//  de color del UV. El camino 3D (SkinMesh / GLB / weight paint del viewport 3D) usa la OTRA
//  entidad (vertexGroups) y no mira esta: son dos grupos distintos, sin bake entre ellos.
// ---------------------------------------------------------------------------
float WeightUVOf(Mesh* m, int uvGroup, int rv) {
    if (!m || uvGroup < 0 || uvGroup >= (int)m->uvGroups.size() || rv < 0) return 0.0f;
    UVGroup* ug = m->uvGroups[uvGroup];
    if (!ug) return 0.0f;
    for (size_t j = 0; j < ug->verts.size() && j < ug->weights.size(); j++)
        if (ug->verts[j] == rv) return ug->weights[j];
    return 0.0f;
}

void WeightUVAssign(Mesh* m, int uvGroup, int rv, float w) {
    if (!m || uvGroup < 0 || uvGroup >= (int)m->uvGroups.size() || rv < 0) return;
    if (w > 1.0f) w = 1.0f;
    UVGroup* ug = m->uvGroups[uvGroup];
    if (!ug) return;
    for (size_t j = 0; j < ug->verts.size(); j++) {
        if (ug->verts[j] != rv) continue;
        if (w <= 0.0f) { // peso 0 = BORRAR la entrada sparse (no acumular basura)
            ug->verts.erase(ug->verts.begin() + j);
            if (j < ug->weights.size()) ug->weights.erase(ug->weights.begin() + j);
        } else if (j < ug->weights.size()) ug->weights[j] = w;
        return;
    }
    if (w > 0.0f) { ug->verts.push_back(rv); ug->weights.push_back(w); } // entrada nueva
}

void UVGroupClearWeights(Mesh* m, int uvGroup) {
    if (!m || uvGroup < 0 || uvGroup >= (int)m->uvGroups.size()) return;
    UVGroup* ug = m->uvGroups[uvGroup];
    if (!ug) return;
    ug->verts.clear(); ug->weights.clear();
}

// (WeightPaintEnsureMap se MUDO a main/edit/MeshEdit.cpp. Motivo: la llama la
//  CARGA de una malla con esqueleto, o sea tambien el runtime de un juego
//  compilado, y este .cpp es todo herramienta INTERACTIVA del editor -- pincel,
//  popup, undo -- que un juego no linkea. Es dato de la malla, no del pincel.)

// ---------------------------------------------------------------------------
//  "EDITAR SOLO LO SELECCIONADO": toggle global compartido por la toolbar del 3D
//  (Weight Paint) y la del UV editor (modo pintura), rol TBR_OnlySel. Default OFF.
// ---------------------------------------------------------------------------
static bool g_wpOnlySel = false;
bool& WeightPaintOnlySel() { return g_wpOnlySel; }

// control-points PERMITIDOS por la mascara: los de las caras logicas (faces3d) marcadas en
// 'fsel'. Si fsel es NULL se deriva de la EDIT MESH (faceSel via faceSrc = la seleccion de
// caras de edit mode, que persiste al cambiar a Weight Paint).
// caras seleccionadas en EDIT MODE (por indice de faces3d). Es la fuente unica de la que
// salen las dos mascaras del pincel Y el velo de caras bloqueadas: si divergieran, se veria
// bloqueada una cara que si se puede pintar (o al reves).
static void WPFacesSel(Mesh* m, const std::vector<char>* fsel, std::vector<char>& out) {
    if (fsel) { out = *fsel; return; }
    out.assign(m->faces3d.size(), 0);
    m->EnsureEdit();
    if (!m->edit) return;
    for (size_t f = 0; f < m->edit->faceSel.size(); f++)
        if (m->edit->faceSel[f] && f < m->edit->faceSrc.size()) {
            const int f3 = m->edit->faceSrc[f];
            if (f3 >= 0 && f3 < (int)m->faces3d.size()) out[(size_t)f3] = 1;
        }
}

// velo negro sobre lo que NO se puede pintar (ver WeightPaint.h). En PANTALLA, via proyector.
void BrushDrawFacesLocked(Mesh* m, WPProjector proj, void* ctx,
                                 const std::vector<char>* onlyFaces) {
    if (!m || !proj || !g_wpOnlySel || m->vertexSize <= 0) return;
    std::vector<char> sel;
    WPFacesSel(m, onlyFaces, sel);

    static std::vector<float> tri;
    tri.clear();
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        if (f < sel.size() && sel[f]) continue;          // esta cara SI se puede pintar
        const std::vector<int>& id = m->faces3d[f].idx;
        const size_t n = id.size();
        if (n < 3) continue;
        static std::vector<float> px, py;
        px.clear(); py.clear();
        bool all = true;
        for (size_t k = 0; k < n; k++) {
            float sx = 0.0f, sy = 0.0f;
            // el proyector descarta lo que mira para el otro lado: una cara de atras no se
            // oscurece (tampoco se pinta, asi que no hay nada que avisar ahi)
            if (id[k] < 0 || id[k] >= m->vertexSize || !proj(ctx, id[k], sx, sy)) { all = false; break; }
            px.push_back(sx); py.push_back(sy);
        }
        if (!all) continue;
        for (size_t k = 2; k < n; k++) {                  // abanico: ngon -> triangulos
            tri.push_back(px[0]);   tri.push_back(py[0]);
            tri.push_back(px[k-1]); tri.push_back(py[k-1]);
            tri.push_back(px[k]);   tri.push_back(py[k]);
        }
    }
    if (tri.empty()) return;

    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::Enable(gfx::Blend);
    gfx::BlendAlpha();
    gfx::Color4f(0.0f, 0.0f, 0.0f, 0.40f);   // 40% mas oscuro, como se pidio
    gfx::VertexPointer2f(0, &tri[0]);
    gfx::DrawTrianglesArray((int)(tri.size() / 2));
    gfx::Disable(gfx::Blend);
}

static void WPMaskCPs(Mesh* m, const std::vector<char>* fsel, int maxCP, std::vector<char>& cpOk) {
    cpOk.assign((size_t)maxCP + 1, 0);
    std::vector<char> own;
    if (!fsel) {
        own.assign(m->faces3d.size(), 0);
        m->EnsureEdit();
        if (m->edit)
            for (size_t f = 0; f < m->edit->faceSel.size(); f++)
                if (m->edit->faceSel[f] && f < m->edit->faceSrc.size()) {
                    int f3 = m->edit->faceSrc[f];
                    if (f3 >= 0 && f3 < (int)m->faces3d.size()) own[f3] = 1;
                }
        fsel = &own;
    }
    for (size_t f = 0; f < m->faces3d.size() && f < fsel->size(); f++) {
        if (!(*fsel)[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c];
            if (k >= 0 && k < (int)m->vertCtrlPoint.size()) {
                int cp = m->vertCtrlPoint[k];
                if (cp >= 0 && cp <= maxCP) cpOk[(size_t)cp] = 1;
            }
        }
    }
}

// caras logicas (faces3d) marcadas en 'fsel', devueltas como MASCARA POR RENDER-VERT (version
// por corner de WPMaskCPs: no traduce a control-point -> la cara vecina que comparte el punto
// 3D NO entra). Si fsel es NULL se deriva de la EDIT MESH, igual que la variante por CP.
static void WPMaskRVs(Mesh* m, const std::vector<char>* fsel, std::vector<char>& rvOk) {
    rvOk.assign((size_t)m->vertexSize, 0);
    std::vector<char> own;
    if (!fsel) {
        own.assign(m->faces3d.size(), 0);
        m->EnsureEdit();
        if (m->edit)
            for (size_t f = 0; f < m->edit->faceSel.size(); f++)
                if (m->edit->faceSel[f] && f < m->edit->faceSrc.size()) {
                    int f3 = m->edit->faceSrc[f];
                    if (f3 >= 0 && f3 < (int)m->faces3d.size()) own[f3] = 1;
                }
        fsel = &own;
    }
    for (size_t f = 0; f < m->faces3d.size() && f < fsel->size(); f++) {
        if (!(*fsel)[f]) continue;
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++) {
            int k = id[c];
            if (k >= 0 && k < m->vertexSize) rvOk[(size_t)k] = 1;
        }
    }
}

// ---------------------------------------------------------------------------
//  PINCEL sobre la malla (agnostico del viewport via el proyector)
// ---------------------------------------------------------------------------
// el falloff a usar: el que pidio el llamador, o SMOOTH (la formula que estaba clavada
// antes de que el falloff fuera elegible) para que un llamador viejo no cambie de conducta.
static const W3dFalloff& FalloffODefault(const W3dFalloff* f) {
    static const W3dFalloff smooth;   // el constructor deja tipo = FoSmooth
    return f ? *f : smooth;
}

bool BrushApply(Mesh* m, int group, float centerX, float centerY, float radiusPx,
                   float force01, WPMode mode, WPProjector proj, void* ctx,
                   const std::vector<char>* onlyFaces, const W3dFalloff* falloff) {
    if (!m || !proj || m->vertexSize <= 0 || radiusPx <= 0.0f) return false;
    WeightPaintEnsureMap(m);
    if (group < 0 || group >= (int)m->vertexGroups.size()) return false;
    if (force01 < 0.0f) force01 = 0.0f;
    if (force01 > 1.0f) force01 = 1.0f;
    // valor 0: en +/- el delta seria 0 y no hay nada que hacer; en "=" SI hay (deja los
    // pesos en cero exacto, o sea BORRA lo pintado), asi que ese modo no se corta aca.
    if (mode != WPEqualize && force01 <= 0.0f) return false;

    int maxCP = -1;
    for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
        if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
    if (maxCP < 0) return false;

    // MASCARA "solo lo seleccionado" (toggle ON): control-points de caras seleccionadas.
    // Con la mascara vacia (ninguna cara seleccionada) el pincel no pinta nada.
    std::vector<char> cpOk;
    if (g_wpOnlySel) WPMaskCPs(m, onlyFaces, maxCP, cpOk);

    // 1) falloff MAXIMO por control-point: los splits de un mismo CP no acumulan doble
    const W3dFalloff& fo = FalloffODefault(falloff);
    std::vector<float> fall((size_t)maxCP + 1, 0.0f);
    const float r2 = radiusPx * radiusPx;
    bool any = false;
    std::vector<char> visRV; WPRenderVisibleVerts(m, proj, ctx, visRV);   // back-facing + tapados
    for (int i = 0; i < m->vertexSize && i < (int)m->vertCtrlPoint.size(); i++) {
        int cp = m->vertCtrlPoint[i];
        if (cp < 0 || cp > maxCP) continue;
        if (!cpOk.empty() && !cpOk[(size_t)cp]) continue; // mascara: cara no seleccionada
        float sx = 0.0f, sy = 0.0f;
        if (i >= (int)visRV.size() || !visRV[(size_t)i]) continue;   // tapado por otra cara (mapa de oclusion)
        if (!proj(ctx, i, sx, sy)) continue;   // detras de camara / back-facing: no se pinta
        float dx = sx - centerX, dy = sy - centerY;
        float d2 = dx * dx + dy * dy;
        if (d2 > r2) continue;
        // la CURVA elegida decide cuanto entra este vert: t = 0 en el centro, 1 en el borde
        float f = fo.Eval(sqrtf(d2) / radiusPx);
        if (f > fall[(size_t)cp]) { fall[(size_t)cp] = f; any = true; }
    }
    if (!any) return false;

    // 2) aplicar UNA vez por control-point (clamp 0..1; entrada sparse creada/borrada)
    bool change = false;
    const size_t dom = (size_t)maxCP + 1;
    for (int cp = 0; cp <= maxCP; cp++) {
        float f = fall[(size_t)cp];
        if (f <= 0.0f) continue;
        float w0 = WeightOf(m, group, cp);
        // TOPE POR TRAZO: de este control-point ya puede haberse ocupado una pasada anterior
        // del mismo trazo (el mouse pasa decenas de veces por el mismo lugar). Solo se aplica
        // lo que FALTA para llegar a valor*falloff. "=" no necesita tope: deja el valor exacto,
        // aplicarlo dos veces da lo mismo.
        float step = force01 * f;
        if (mode != WPEqualize) {
            step = PendingStroke((size_t)cp, dom, step);
            if (step <= 0.0f) continue;
        }
        float w = (mode == WPEqualize) ? force01
                : (mode == WPAdd)   ? (w0 + step)
                                      : (w0 - step);
        if (w < 0.0f) w = 0.0f;
        if (w > 1.0f) w = 1.0f;
        if (w != w0) { WeightAssign(m, group, cp, w); change = true; }
    }
    if (change) {
        // los pesos cambiaron: invalidar el CSR de skinning (su firma no hashea los valores)
        // y forzar re-skin -> si la malla esta posada, la deformacion refleja la pintura al toque
        m->skinGeomVersion++;
        m->lastSkinFrame = -999999;
        g_redraw = true;
    }
    return change;
}

// ---------------------------------------------------------------------------
//  PINCEL DE VERTEX COLOR (por CORNER, sobre una ColorLayer). Ver WeightPaint.h.
// ---------------------------------------------------------------------------
bool BrushApplyColor(Mesh* m, int layer, float centerX, float centerY, float radiusPx,
                        float value01, const unsigned char* rgba, int palIdx,
                        WPProjector proj, void* ctx,
                        const std::vector<char>* onlyFaces, const W3dFalloff* falloff) {
    if (!m || !proj || !rgba || m->vertexSize <= 0 || radiusPx <= 0.0f) return false;
    if (layer < 0 || layer >= (int)m->colorLayers.size()) return false;
    ColorLayer* cl = m->colorLayers[layer];
    if (!cl) return false;
    const int nC = m->CountCorners();
    if (nC <= 0 || (int)cl->color.size() != nC * 4) return false;   // capa stale: no tocar
    if (value01 < 0.0f) value01 = 0.0f;
    if (value01 > 1.0f) value01 = 1.0f;
    if (value01 <= 0.0f) return false;
    if (cl->byIndex && (int)cl->index.size() != nC) cl->index.assign((size_t)nC, -1);
    // por VERTICE si la capa es Per-Vertex o si el pincel esta en "por vertice" (el boton de la barra)
    const bool byVert = cl->perVertex || BrushGet().perVertex;

    const W3dFalloff& fo = FalloffODefault(falloff);
    std::vector<char> rvOk;
    if (g_wpOnlySel) WPMaskRVs(m, onlyFaces, rvOk);   // "solo lo seleccionado": por render-vert

    // 1) cuanto le toca a cada CORNER (en el orden de faces3d, que es como indexa la capa).
    //    LA POSICION DEL CORNER ES LA DEL CUADRADITO, no la del vertice: los 2-3 corners que
    //    comparten una esquina caen en el MISMO punto del vertice, asi que testear ahi hacia
    //    imposible pintar uno solo (agarraba los 3 juntos aunque apuntaras a un cuadrado).
    //    Con capa Per-Vertex es al reves: ahi el punto ES el vertice (un vertice = un color) y
    //    el bloque 2) de abajo reparte lo pintado a todos sus corners.
    std::vector<float> weight((size_t)nC, 0.0f);
    const float r2 = radiusPx * radiusPx;
    bool any = false;
    std::vector<float> cx, cy; std::vector<char> vis;
    if (!byVert) WPCornersInScreen(m, proj, ctx, cx, cy, vis);
    std::vector<char> visRV; if (byVert) WPRenderVisibleVerts(m, proj, ctx, visRV);   // back-facing + tapados
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++, L++) {
            if (L >= nC) break;
            const int rv = id[c];
            if (rv < 0 || rv >= m->vertexSize) continue;
            if (!rvOk.empty() && !rvOk[(size_t)rv]) continue;   // mascara: cara no seleccionada
            float sx = 0.0f, sy = 0.0f;
            if (!byVert) {
                if (L >= (int)vis.size() || !vis[(size_t)L]) continue;  // corner no visible
                sx = cx[(size_t)L]; sy = cy[(size_t)L];                 // el punto del cuadradito
            } else if (rv >= (int)visRV.size() || !visRV[(size_t)rv] || !proj(ctx, rv, sx, sy)) continue;   // Per-Vertex: el vertice (visible)
            const float dx = sx - centerX, dy = sy - centerY;
            const float d2 = dx * dx + dy * dy;
            if (d2 > r2) continue;
            const float a = value01 * fo.Eval(sqrtf(d2) / radiusPx);
            if (a > weight[(size_t)L]) { weight[(size_t)L] = a; any = true; }
        }
    }
    if (!any) return false;

    // 2) POR VERTICE: la capa igual guarda por corner, pero al hornear se colapsa por posicion
    //    (todos los corners de una posicion toman el color del primero). Si se pintara solo el
    //    corner tocado, el resultado dependeria de cual corner es el primero -> se propaga el
    //    maximo a TODOS los corners que comparten posicion. Es "un vertice = un color".
    if (byVert) {
        WeightPaintEnsureMap(m);
        int maxCP = -1;
        for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
            if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
        if (maxCP >= 0) {
            std::vector<float> byCP((size_t)maxCP + 1, 0.0f);
            L = 0;
            for (size_t f = 0; f < m->faces3d.size(); f++)
                for (size_t c = 0; c < m->faces3d[f].idx.size(); c++, L++) {
                    if (L >= nC) break;
                    const int rv = m->faces3d[f].idx[c];
                    if (rv < 0 || rv >= (int)m->vertCtrlPoint.size()) continue;
                    const int cp = m->vertCtrlPoint[rv];
                    if (cp >= 0 && cp <= maxCP && weight[(size_t)L] > byCP[(size_t)cp])
                        byCP[(size_t)cp] = weight[(size_t)L];
                }
            L = 0;
            for (size_t f = 0; f < m->faces3d.size(); f++)
                for (size_t c = 0; c < m->faces3d[f].idx.size(); c++, L++) {
                    if (L >= nC) break;
                    const int rv = m->faces3d[f].idx[c];
                    if (rv < 0 || rv >= (int)m->vertCtrlPoint.size()) continue;
                    const int cp = m->vertCtrlPoint[rv];
                    if (cp >= 0 && cp <= maxCP) weight[(size_t)L] = byCP[(size_t)cp];
                }
        }
    }

    // 3) escribir: mezcla del color (o el indice entero, que no se puede mezclar)
    bool change = false;
    for (int i = 0; i < nC; i++) {
        const float a = weight[(size_t)i];
        if (a <= 0.0f) continue;
        if (cl->byIndex) {
            if (a < 0.5f) continue;                 // el falloff decide QUIEN, no cuanto
            if (cl->index[(size_t)i] != palIdx) { cl->index[(size_t)i] = palIdx; change = true; }
            for (int q = 0; q < 4; q++) {
                if (cl->color[(size_t)i * 4 + q] == rgba[q]) continue;
                cl->color[(size_t)i * 4 + q] = rgba[q]; change = true;
            }
            continue;
        }
        // TOPE POR TRAZO en una MEZCLA: no se puede sumar el pendiente y listo (mezclar dos
        // veces al 50% no da 100%). Se aplica t = (a - ya) / (1 - ya), que es exactamente el
        // factor que hace que la mezcla ACUMULADA del trazo termine valiendo 'a'.
        const float already = (i < (int)g_strokeApply.size() && g_strokeApply.size() == (size_t)nC)
                         ? g_strokeApply[(size_t)i] : 0.0f;
        const float pend = PendingStroke((size_t)i, (size_t)nC, a);
        if (pend <= 0.0f) continue;
        const float t = (already >= 0.999f) ? 1.0f : (pend / (1.0f - already));
        for (int q = 0; q < 4; q++) {
            const float old      = (float)cl->color[(size_t)i * 4 + q];
            const float newValue = old + ((float)rgba[q] - old) * t;
            const unsigned char b = (unsigned char)(newValue + 0.5f);
            if (b != cl->color[(size_t)i * 4 + q]) { cl->color[(size_t)i * 4 + q] = b; change = true; }
        }
    }
    if (change) {
        m->ApplyLayersToRender();   // la capa -> vertexColor[] (lo unico que el core dibuja)
        g_redraw = true;
    }
    return change;
}

// FUSION Per-Corner -> Per-Vertex: promedia los colores de los corners que comparten posicion
// y se los escribe a TODOS. Ver WeightPaint.h: esto es lo que hace destructivo al cambio.
bool VertexColorMergePerVertex(Mesh* m, int layer) {
    if (!m || layer < 0 || layer >= (int)m->colorLayers.size()) return false;
    ColorLayer* cl = m->colorLayers[layer];
    if (!cl) return false;
    const int nC = m->CountCorners();
    if (nC <= 0 || (int)cl->color.size() != nC * 4) return false;
    WeightPaintEnsureMap(m);
    int maxCP = -1;
    for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
        if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
    if (maxCP < 0) return false;

    // suma por control-point (4 canales) + cuantos corners aporto cada uno
    std::vector<float> sum((size_t)(maxCP + 1) * 4, 0.0f);
    std::vector<int>   n((size_t)maxCP + 1, 0);
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++)
        for (size_t c = 0; c < m->faces3d[f].idx.size(); c++, L++) {
            if (L >= nC) break;
            const int rv = m->faces3d[f].idx[c];
            if (rv < 0 || rv >= (int)m->vertCtrlPoint.size()) continue;
            const int cp = m->vertCtrlPoint[rv];
            if (cp < 0 || cp > maxCP) continue;
            for (int q = 0; q < 4; q++) sum[(size_t)cp * 4 + q] += (float)cl->color[(size_t)L * 4 + q];
            n[(size_t)cp]++;
        }
    bool change = false;
    L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++)
        for (size_t c = 0; c < m->faces3d[f].idx.size(); c++, L++) {
            if (L >= nC) break;
            const int rv = m->faces3d[f].idx[c];
            if (rv < 0 || rv >= (int)m->vertCtrlPoint.size()) continue;
            const int cp = m->vertCtrlPoint[rv];
            if (cp < 0 || cp > maxCP || n[(size_t)cp] <= 0) continue;
            for (int q = 0; q < 4; q++) {
                const unsigned char prom =
                    (unsigned char)(sum[(size_t)cp * 4 + q] / (float)n[(size_t)cp] + 0.5f);
                if (prom != cl->color[(size_t)L * 4 + q]) { cl->color[(size_t)L * 4 + q] = prom; change = true; }
            }
        }
    // el INDICE de paleta no se puede promediar (no existe "el promedio de dos indices"): al
    // fusionar, esos corners pasan a color libre con el promedio ya horneado.
    if (cl->byIndex && (int)cl->index.size() == nC)
        for (int i = 0; i < nC; i++) cl->index[(size_t)i] = -1;
    if (change) { m->ApplyLayersToRender(); g_redraw = true; }
    return change;
}

// re-hornea los colores desde los INDICES contra la paleta efectiva del objeto: ESTO es el
// palette-swap (cambiar la paleta de un padre re-pinta a todos sus herederos).
bool VertexColorResolveIndices(Mesh* m, int layer, Object* owner) {
    if (!m || layer < 0 || layer >= (int)m->colorLayers.size()) return false;
    ColorLayer* cl = m->colorLayers[layer];
    if (!cl || !cl->byIndex) return false;
    const int nC = m->CountCorners();
    if (nC <= 0 || (int)cl->color.size() != nC * 4) return false;
    if ((int)cl->index.size() != nC) return false;
    std::vector<PaletteColor>* cols = W3dEffectiveColors(owner ? owner : (Object*)m);
    bool change = false;
    for (int i = 0; i < nC; i++) {
        const int idx = cl->index[(size_t)i];
        if (idx < 0 || !cols || idx >= (int)cols->size()) continue;  // sin indice: color libre, no se toca
        const float* c = (*cols)[(size_t)idx].rgba;
        for (int q = 0; q < 4; q++) {
            float v = c[q]; if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            const unsigned char b = (unsigned char)(v * 255.0f + 0.5f);
            if (b != cl->color[(size_t)i * 4 + q]) { cl->color[(size_t)i * 4 + q] = b; change = true; }
        }
    }
    if (change) { m->ApplyLayersToRender(); g_redraw = true; }
    return change;
}

// ---------------------------------------------------------------------------
//  PINCEL DEL EDITOR UV (por CORNER, sobre un UV GROUP). Misma pasada/falloff que el de arriba,
//  pero la unidad es el RENDER-VERT: los 4 corners de UNA cara se pesan sin tocar las caras
//  vecinas que comparten esos vertices 3D. No mira ni toca los vertex groups.
// ---------------------------------------------------------------------------
bool BrushApplyUV(Mesh* m, int uvGroup, float centerX, float centerY, float radiusPx,
                     float force01, WPMode mode, WPProjector proj, void* ctx,
                     const std::vector<char>* onlyFaces, const W3dFalloff* falloff) {
    if (!m || !proj || m->vertexSize <= 0 || radiusPx <= 0.0f) return false;
    if (uvGroup < 0 || uvGroup >= (int)m->uvGroups.size()) return false;
    if (force01 < 0.0f) force01 = 0.0f;
    if (force01 > 1.0f) force01 = 1.0f;
    if (mode != WPEqualize && force01 <= 0.0f) return false; // "=" con 0 SI hace algo (borra)

    std::vector<char> rvOk;
    const W3dFalloff& fo = FalloffODefault(falloff);
    if (g_wpOnlySel) WPMaskRVs(m, onlyFaces, rvOk);
    // falloff por render-vert (cada uno se proyecta a SU posicion en pantalla; no hay
    // "maximo entre splits" que valga: los splits son justamente lo que se separa)
    std::vector<float> fall((size_t)m->vertexSize, 0.0f);
    const float r2 = radiusPx * radiusPx;
    bool any = false;
    for (int i = 0; i < m->vertexSize; i++) {
        if (!rvOk.empty() && !rvOk[(size_t)i]) continue; // mascara: cara no seleccionada
        float sx = 0.0f, sy = 0.0f;
        if (!proj(ctx, i, sx, sy)) continue;
        float dx = sx - centerX, dy = sy - centerY;
        float d2 = dx * dx + dy * dy;
        if (d2 > r2) continue;
        fall[(size_t)i] = fo.Eval(sqrtf(d2) / radiusPx); // la CURVA elegida (0 centro .. 1 borde)
        any = true;
    }
    if (!any) return false;
    // peso DENSO del grupo para no hacer la busqueda lineal de WeightUVOf dentro del loop
    UVGroup* ug = m->uvGroups[uvGroup];
    std::vector<float> wStart((size_t)m->vertexSize, 0.0f);
    if (ug) for (size_t k = 0; k < ug->verts.size() && k < ug->weights.size(); k++) {
        int rv = ug->verts[k];
        if (rv >= 0 && rv < m->vertexSize) wStart[(size_t)rv] = ug->weights[k];
    }
    bool change = false;
    for (int i = 0; i < m->vertexSize; i++) {
        float f = fall[(size_t)i];
        if (f <= 0.0f) continue;
        float w0 = wStart[(size_t)i];
        float step = force01 * f;                                // idem 3D: tope por trazo
        if (mode != WPEqualize) {
            step = PendingStroke((size_t)i, (size_t)m->vertexSize, step);
            if (step <= 0.0f) continue;
        }
        float w = (mode == WPEqualize) ? force01                 // "=": exacto, sin falloff
                : (mode == WPAdd)   ? (w0 + step)
                                      : (w0 - step);
        if (w < 0.0f) w = 0.0f;
        if (w > 1.0f) w = 1.0f;
        if (w != w0) { WeightUVAssign(m, uvGroup, i, w); change = true; }
    }
    // NO se tocan skinGeomVersion/lastSkinFrame ni se escribe mesh->uv: pintar pesos NO mueve
    // UVs. El caller (editor UV) re-aplica el skinning 2D SOLO si hay una pose real que dependa
    // de estos pesos (ver el invariante uv = f(uv2dRest, pose) en Mesh.h).
    if (change) g_redraw = true;
    return change;
}

// ---------------------------------------------------------------------------
//  TRAZO (un undo por trazo: snapshot al mouse-down, commit al soltar)
// ---------------------------------------------------------------------------
static Mesh* g_wpStrokeMesh = NULL;

int WeightPaintStrokeStart(Mesh* m) {
    BrushStrokeReset();
    if (!m || m->vertexSize <= 0) return -1;
    WeightPaintEnsureMap(m);
    UndoWeightsStart(m); // snapshot ANTES de crear el grupo -> el undo del trazo tambien lo saca
    if (m->vertexGroups.empty()) {
        CreateVertexGroup(m); // "Group" (nombre unico) + queda activo
        w3dLogf("[weightpaint] '%s' sin vertex groups: se creo '%s' automaticamente",
                m->name.c_str(), m->vertexGroups[0]->name.c_str());
    }
    if (m->activeGroup < 0 || m->activeGroup >= (int)m->vertexGroups.size()) m->activeGroup = 0;
    g_wpStrokeMesh = m;
    return m->activeGroup;
}

// TRAZO del EDITOR UV: la entidad es el UV GROUP. Sin ninguno, el primer trazo crea uno con el
// NOMBRE DEL HUESO 2D ACTIVO si la malla tiene armature 2D (asi el binding por nombre queda
// hecho y pintar deforma al toque); sin armature 2D se llama "UV Group".
int WeightPaintStrokeStartUV(Mesh* m) {
    BrushStrokeReset();
    if (!m || m->vertexSize <= 0) return -1;
    UndoWeightsStart(m); // snapshot ANTES de crear el grupo -> el undo del trazo tambien lo saca
    if (m->uvGroups.empty()) {
        std::string base = "UV Group";
        if (m->Arm2DActiveBone() >= 0 && m->Arm2DActiveBone() < (int)m->Arm2DBones().size())
            base = m->Arm2DBones()[m->Arm2DActiveBone()].name;
        else if (!m->Arm2DBones().empty()) base = m->Arm2DBones()[0].name;
        CreateUVGroup(m, base); // nombre unico + queda activo
        w3dLogf("[weightpaint] '%s' sin UV groups: se creo '%s' automaticamente",
                m->name.c_str(), m->uvGroups[0]->name.c_str());
    }
    if (m->uvActiveGroup < 0 || m->uvActiveGroup >= (int)m->uvGroups.size()) m->uvActiveGroup = 0;
    g_wpStrokeMesh = m;
    return m->uvActiveGroup;
}

void WeightPaintStrokeEnd() {
    if (!g_wpStrokeMesh) return;
    UndoWeightsConfirm(); // pushea el trazo (descarta si no cambio nada)
    g_wpStrokeMesh = NULL;
}

bool WeightPaintStrokeActive() { return g_wpStrokeMesh != NULL; }

// ---------------------------------------------------------------------------
//  ASSIGN / REMOVE / SELECT / DESELECT de los DOS grupos de pesos
//  (tarjetas "Vertex Groups" y "UV Groups" del panel Properties; ver WeightPaint.h)
// ---------------------------------------------------------------------------
bool UVVertsSelEffective(Mesh* m, std::vector<char>& sv);  // (decl. en ViewPorts/UVEditor.h)

// ---- VERTEX GROUPS (control-points; seleccion de EDIT MODE del 3D) ----
int VertexGroupAssignSel(Mesh* m, bool assign) {
    if (!m || m->vertexGroups.empty()) return 0;
    if (m->activeGroup < 0 || m->activeGroup >= (int)m->vertexGroups.size()) return 0;
    if ((Object*)m != g_editMesh || !m->edit) return 0;   // sin Edit Mode no hay seleccion que asignar
    WeightPaintEnsureMap(m);                            // render-vert -> control-point (lazy)
    const int nV = m->vertexSize;
    UndoWeightsStart(m);
    int n = 0;
    std::vector<char> seen;                               // un control-point se toca UNA vez
    for (size_t k = 0; k < m->edit->editVerts.size(); k++) {
        if (k >= m->edit->vertSel.size() || !m->edit->vertSel[k]) continue;
        int rv = m->edit->editVerts[k];
        if (rv < 0 || rv >= nV) continue;
        int cp = ((int)m->vertCtrlPoint.size() == nV) ? m->vertCtrlPoint[rv] : rv;
        if (cp < 0) continue;
        if ((int)seen.size() <= cp) seen.resize(cp + 1, 0);
        if (seen[cp]) continue;
        seen[cp] = 1;
        WeightAssign(m, m->activeGroup, cp, assign ? 1.0f : 0.0f); // 0 BORRA la entrada sparse
        n++;
    }
    UndoWeightsConfirm();
    // los pesos cambiaron: MISMA invalidacion que el pincel (ver BrushApply) -> si la malla
    // esta posada, la deformacion refleja el assign al toque
    if (n) { m->skinGeomVersion++; m->lastSkinFrame = -999999; g_redraw = true; }
    return n;
}

int VertexGroupSelect(Mesh* m, bool sel) {
    if (!m || m->vertexGroups.empty()) return 0;
    if (m->activeGroup < 0 || m->activeGroup >= (int)m->vertexGroups.size()) return 0;
    if ((Object*)m != g_editMesh || !m->edit) return 0;
    WeightPaintEnsureMap(m);
    const int nV = m->vertexSize;
    const VertexGroup* g = m->vertexGroups[m->activeGroup];
    if (!g) return 0;
    // set de control-points con peso > 0 (denso por indice: son pocos y el lookup es O(1))
    std::vector<char> heavy;
    for (size_t i = 0; i < g->verts.size() && i < g->weights.size(); i++) {
        int cp = g->verts[i];
        if (cp < 0 || g->weights[i] <= 0.0f) continue;
        if ((int)heavy.size() <= cp) heavy.resize(cp + 1, 0);
        heavy[cp] = 1;
    }
    int n = 0;
    for (size_t k = 0; k < m->edit->editVerts.size(); k++) {
        int rv = m->edit->editVerts[k];
        if (rv < 0 || rv >= nV) continue;
        int cp = ((int)m->vertCtrlPoint.size() == nV) ? m->vertCtrlPoint[rv] : rv;
        if (cp < 0 || cp >= (int)heavy.size() || !heavy[cp]) continue;
        if (k < m->edit->vertSel.size()) { m->edit->vertSel[k] = sel ? 1 : 0; n++; }
    }
    if (n) { m->edit->Recolor(); g_redraw = true; }
    return n;
}

// ---- UV GROUPS (render-verts / corners; seleccion del editor UV) ----
int UVGroupAssignSel(Mesh* m, bool assign) {
    if (!m || m->uvGroups.empty()) return 0;
    if (m->uvActiveGroup < 0 || m->uvActiveGroup >= (int)m->uvGroups.size()) return 0;
    std::vector<char> sv;
    if (!UVVertsSelEffective(m, sv)) return 0;             // nada seleccionado por ningun camino
    UndoWeightsStart(m);
    int n = 0;
    for (int i = 0; i < m->vertexSize && i < (int)sv.size(); i++) {
        if (!sv[i]) continue;
        WeightUVAssign(m, m->uvActiveGroup, i, assign ? 1.0f : 0.0f); // 0 BORRA la entrada
        n++;
    }
    UndoWeightsConfirm();
    if (n) g_redraw = true;
    return n;
}

int UVGroupSelect(Mesh* m, bool sel) {
    if (!m || m->uvGroups.empty()) return 0;
    if (m->uvActiveGroup < 0 || m->uvActiveGroup >= (int)m->uvGroups.size()) return 0;
    const int nV = m->vertexSize;
    if (nV <= 0) return 0;
    const UVGroup* g = m->uvGroups[m->uvActiveGroup];
    if (!g) return 0;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign(nV, 0);
    int n = 0;
    for (size_t i = 0; i < g->verts.size() && i < g->weights.size(); i++) {
        int rv = g->verts[i];
        if (rv < 0 || rv >= nV || g->weights[i] <= 0.0f) continue;
        m->uvSelVert[rv] = sel ? 1 : 0;
        n++;
    }
    if (n) g_redraw = true;
    return n;
}

// ---------------------------------------------------------------------------
//  MENUS del pincel (compartidos por la toolbar del 3D y la del UV editor)
// ---------------------------------------------------------------------------
static PopupMenu* gMenuBrushGroup  = NULL; // 3D: vertex groups
static PopupMenu* gMenuBrushUVGrp  = NULL; // UV: uv groups
static Mesh*      gMenuGroupMesh   = NULL; // la malla cuyo dropdown de grupos esta abierto

// abre 'menu' desde la toolbar: crece hacia ARRIBA del boton (mismo criterio que el
// menu Orient de la toolbar del 3D) para no taparse con la barra ni salirse de pantalla.
static void OpenMenuToolbar(PopupMenu* menu, int sx, int syTop) {
    if (!menu) return;
    if (OpenMenu && OpenMenu != menu) OpenMenu->Close();
    menu->Resize();
    int my = syTop - menu->height;
    if (my < 0) my = 0;
    menu->Open(sx, my, MenuScreenW, MenuScreenH);
    OpenMenu = menu;
    g_redraw = true;
}

static void ActionMenuGroup(int id) {
    Mesh* m = gMenuGroupMesh;
    if (!m) return;
    if (id == 1000) {           // New Group: crea uno y lo deja activo (como la tarjeta de Properties)
        CreateVertexGroup(m);
    } else if (id >= 0 && id < (int)m->vertexGroups.size()) {
        m->activeGroup = id;    // mismo efecto que elegirlo en la tarjeta Vertex Groups
    }
    g_redraw = true;
}

// desplegable del VIEWPORT 3D (Weight Paint): SOLO vertex groups. Sin items que crucen con los
// UV groups: son dos entidades distintas y no se bakea una desde la otra.
void WeightPaintMenuGroup(Mesh* m, int sx, int syTop) {
    if (!m) return;
    gMenuGroupMesh = m;
    if (!gMenuBrushGroup) gMenuBrushGroup = new PopupMenu();
    gMenuBrushGroup->Clear(); // se rearma cada vez (los grupos cambian; marca el activo en verde)
    gMenuBrushGroup->title = T("Vertex Groups");
    for (size_t g = 0; g < m->vertexGroups.size(); g++)
        gMenuBrushGroup->Add(m->vertexGroups[g]->name, (int)g)->green = ((int)g == m->activeGroup);
    gMenuBrushGroup->Add(T("Add Vertex Group"), 1000, (int)IconType::plus);
    gMenuBrushGroup->action = ActionMenuGroup;
    OpenMenuToolbar(gMenuBrushGroup, sx, syTop);
}

// ---- CAPAS DE COLOR (vertex paint): elegir a cual se pinta, o crear una ----
static PopupMenu* gMenuLayerColor = NULL;
static void ActionMenuLayerColor(int id) {
    Mesh* m = gMenuGroupMesh;
    if (!m) return;
    if (id == 1000) {                        // capa nueva: copia de la activa (misma regla que UV/color del panel)
        DuplicateActiveColorLayer(m);        // declarada en edit/MeshEdit.h (ya incluido)
    } else if (id >= 0 && id < (int)m->colorLayers.size()) {
        m->activeColor = id;
        m->ApplyLayersToRender();          // la capa elegida es la que se ve (y la que se pinta)
    }
    g_redraw = true;
}

void WeightPaintMenuLayerColor(Mesh* m, int sx, int syTop) {
    if (!m) return;
    gMenuGroupMesh = m;
    if (!gMenuLayerColor) gMenuLayerColor = new PopupMenu();
    gMenuLayerColor->Clear();
    gMenuLayerColor->title = T("Color Layers");
    for (size_t i = 0; i < m->colorLayers.size(); i++)
        gMenuLayerColor->Add(m->colorLayers[i]->name, (int)i)->green = ((int)i == m->activeColor);
    if (!m->colorLayers.empty())
        gMenuLayerColor->Add(T("New Color Layer"), 1000, (int)IconType::plus);
    gMenuLayerColor->action = ActionMenuLayerColor;
    OpenMenuToolbar(gMenuLayerColor, sx, syTop);
}

static void ActionMenuUVGroup(int id) {
    Mesh* m = gMenuGroupMesh;
    if (!m) return;
    if (id == 1000) {           // Add UV Group: crea uno y lo deja activo
        CreateUVGroup(m, "UV Group");
    } else if (id == 1001) {    // Clear: deja el UV group activo SIN pesos (un paso de undo)
        UndoWeightsStart(m);
        UVGroupClearWeights(m, m->uvActiveGroup);
        UndoWeightsConfirm();
    } else if (id >= 0 && id < (int)m->uvGroups.size()) {
        m->uvActiveGroup = id;  // mismo efecto que elegirlo en la tarjeta UV Groups
    }
    g_redraw = true;
}

// desplegable del EDITOR UV (modo pintura): SOLO uv groups (pesos por corner). "Clear" vacia el
// activo; NO hay ningun item que hornee pesos desde los vertex groups.
void WeightPaintMenuUVGroup(Mesh* m, int sx, int syTop) {
    if (!m) return;
    gMenuGroupMesh = m;
    if (!gMenuBrushUVGrp) gMenuBrushUVGrp = new PopupMenu();
    gMenuBrushUVGrp->Clear();
    gMenuBrushUVGrp->title = T("UV Groups");
    for (size_t g = 0; g < m->uvGroups.size(); g++)
        gMenuBrushUVGrp->Add(m->uvGroups[g]->name, (int)g)->green = ((int)g == m->uvActiveGroup);
    gMenuBrushUVGrp->Add(T("Add UV Group"), 1000, (int)IconType::plus);
    if (m->uvActiveGroup >= 0 && m->uvActiveGroup < (int)m->uvGroups.size() &&
        !m->uvGroups[m->uvActiveGroup]->verts.empty())
        gMenuBrushUVGrp->Add(T("Clear UV Group Weights"), 1001);
    gMenuBrushUVGrp->action = ActionMenuUVGroup;
    OpenMenuToolbar(gMenuBrushUVGrp, sx, syTop);
}

// los 3 labels del PINCEL (compartidos: el pincel es uno solo)
static void BrushLabels(std::string& size, std::string& force, std::string& mode) {
    char b[32];
    sprintf(b, "%dpx", (int)(g_brush.radiusPx + 0.5f));          size = b;
    sprintf(b, "%d%%", (int)(g_brush.force * 100.0f + 0.5f));  force = b;
    mode = (g_brush.mode == WPEqualize) ? "=" : (g_brush.mode == WPSubtract) ? "-" : "+";
}

void WeightPaintLabels(Mesh* m, std::string& size, std::string& force,
                       std::string& mode, std::string& group) {
    BrushLabels(size, force, mode);
    group = T("Group");
    if (m && m->activeGroup >= 0 && m->activeGroup < (int)m->vertexGroups.size())
        group = m->vertexGroups[m->activeGroup]->name;
}

void WeightPaintLabelsUV(Mesh* m, std::string& size, std::string& force,
                         std::string& mode, std::string& group) {
    BrushLabels(size, force, mode);
    group = T("UV Group");
    if (m && m->uvActiveGroup >= 0 && m->uvActiveGroup < (int)m->uvGroups.size())
        group = m->uvGroups[m->uvActiveGroup]->name;
}
