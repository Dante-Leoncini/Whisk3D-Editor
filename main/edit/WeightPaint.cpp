// ============================================================================
//  WEIGHT PAINT (Fase 2): pincel reutilizable + escritura de pesos por
//  control-point. Ver WeightPaint.h para el contrato. C++03 (compila en Symbian).
// ============================================================================
#include "edit/WeightPaint.h"
#include "edit/MeshEdit.h"      // CrearVertexGroup / CrearUVGroup (grupo automatico al primer trazo)
#include "objects/Mesh.h"       // Mesh, VertexGroup, UVGroup, vertCtrlPoint, posRep
#include "objects/EditMesh.h"   // faceSel/faceSrc (mascara "solo lo seleccionado")
#include "objects/Objects.h"    // g_editMesh (Assign/Select de vertex groups: seleccion de Edit Mode)
#include "Undo.h"               // UndoPesosIniciar / UndoPesosConfirmar (un undo por TRAZO)
#include "w3dGraphics.h"        // dibujo del circulo (DrawLines)
#include "w3dlog.h"             // aviso al crear el grupo automatico
#include "W3dLang.h"            // T(): titulos de los menus en el idioma del sistema
#include "W3dPaletas.h"         // vertex color por INDICE: la paleta efectiva del objeto
#include "WhiskUI/widgets/PopupMenu.h" // menus deslizables (AgregarFloat) de la toolbar
#include <math.h>               // sqrtf / cosf / sinf
#include <cstdio>               // sprintf (labels de la toolbar)

namespace gfx = w3dEngine;
extern bool g_redraw;

// ---------------------------------------------------------------------------
//  PINCEL (estado global unico; separado de los pesos para reusar en texturas/escultura)
// ---------------------------------------------------------------------------
static BrushEstado g_brush;
BrushEstado& BrushGet() { return g_brush; }

// TOPE POR TRAZO (ver WeightPaint.h): cuanto recibio YA cada elemento en este trazo. El indice
// es el del pincel que este corriendo (control-point, render-vert o corner): un trazo es de UN
// pincel solo, asi que un vector alcanza. Se vacia al arrancar cada trazo.
static std::vector<float> g_trazoAplic;
void BrushTrazoResetear() { g_trazoAplic.clear(); }

// cuanto FALTA aplicarle al elemento i para llegar a 'a' en este trazo (0 = ya recibio eso o
// mas). Deja anotado el nuevo maximo. 'n' es el tamano del dominio (para dimensionar el vector).
static float TrazoPendiente(size_t i, size_t n, float a) {
    if (g_trazoAplic.size() != n) g_trazoAplic.assign(n, 0.0f);
    if (i >= n) return a;
    const float ya = g_trazoAplic[i];
    if (a <= ya) return 0.0f;      // ya se paso por aca con igual o mas fuerza: no suma
    g_trazoAplic[i] = a;
    return a - ya;
}

// Con las MARCAS prendidas el pincel no degrada con la distancia: el cuadradito se toca o
// no se toca. Eso es exactamente un falloff CONSTANTE, asi que en vez de meter un "if
// marcas" adentro del pincel (y tener que acordarse de repetirlo en cada llamador) se
// cambia la curva y el pincel sigue siendo uno solo.
const W3dFalloff& BrushFalloffEfectivo() {
    static W3dFalloff constante;          // se arma una vez
    constante.tipo = FoConstant;
    return (g_brush.marcas != MarcasOff) ? constante : g_brush.falloff;
}

// circulo por SEGMENTOS DE LINEA (no habia helper de circulo en el motor): 48 segmentos,
// dos pasadas -> halo NEGRO grueso abajo + linea BLANCA fina arriba (se lee sobre
// cualquier fondo, mismo criterio que el cursor del editor UV).
void BrushDibujarCirculo(float cx, float cy, float radioPx) {
    if (radioPx <= 0.0f) return;
    const int N = 48;
    static float buf[N * 4]; // N segmentos = N pares de puntos (x,y)
    const float paso = 6.2831853f / (float)N;
    for (int i = 0; i < N; i++) {
        float a0 = paso * (float)i, a1 = paso * (float)(i + 1);
        buf[i*4+0] = cx + cosf(a0) * radioPx; buf[i*4+1] = cy + sinf(a0) * radioPx;
        buf[i*4+2] = cx + cosf(a1) * radioPx; buf[i*4+3] = cy + sinf(a1) * radioPx;
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
//  Dos modos con el MISMO dibujo y distinta posicion (ver W3dMarcasModo):
//    MarcasVertice -> uno por control-point, EXACTO sobre el vertice   (pesos)
//    MarcasCorner  -> uno por face corner, corrido hacia el centro de SU cara
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
static void WPCornersEnPantalla(Mesh* m, WPProyector proy, void* ctx,
                                std::vector<float>& cx, std::vector<float>& cy,
                                std::vector<char>& ok) {
    const int nC = m->ContarCorners();
    const size_t n0 = (size_t)(nC > 0 ? nC : 0);
    cx.assign(n0, 0.0f); cy.assign(n0, 0.0f); ok.assign(n0, 0);
    if (nC <= 0) return;
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& idx = m->faces3d[f].idx;
        const size_t n = idx.size();
        if (L + (int)n > nC) break;
        if (n < 3) { L += (int)n; continue; }
        static std::vector<float> px, py;
        px.clear(); py.clear();
        float sumX = 0.0f, sumY = 0.0f;
        bool todos = true;
        for (size_t k = 0; k < n; k++) {
            float sx = 0.0f, sy = 0.0f;
            const int rv = idx[k];
            if (rv < 0 || rv >= m->vertexSize || !proy(ctx, rv, sx, sy)) { todos = false; break; }
            px.push_back(sx); py.push_back(sy);
            sumX += sx; sumY += sy;
        }
        if (todos) {
            const float ccx = sumX / (float)n, ccy = sumY / (float)n;  // centro de la cara EN PANTALLA
            for (size_t k = 0; k < n; k++) {
                cx[(size_t)(L + (int)k)] = px[k] + (ccx - px[k]) * kCornerInset;
                cy[(size_t)(L + (int)k)] = py[k] + (ccy - py[k]) * kCornerInset;
                ok[(size_t)(L + (int)k)] = 1;
            }
        }
        L += (int)n;
    }
}

static void MarcaEmpujar(std::vector<float>& tri, float cx, float cy, float mitad) {
    const float x0 = cx - mitad, x1 = cx + mitad;
    const float y0 = cy - mitad, y1 = cy + mitad;
    // 2 triangulos (sin index buffer: son pocos y se dibujan con DrawTrianglesArray)
    tri.push_back(x0); tri.push_back(y0);  tri.push_back(x1); tri.push_back(y0);  tri.push_back(x1); tri.push_back(y1);
    tri.push_back(x0); tri.push_back(y0);  tri.push_back(x1); tri.push_back(y1);  tri.push_back(x0); tri.push_back(y1);
}
static void MarcaColor(std::vector<unsigned char>& col, const unsigned char* c) {
    for (int k = 0; k < 6; k++) {          // los 6 vertices del cuadrado, mismo color
        if (c) { col.push_back(c[0]); col.push_back(c[1]); col.push_back(c[2]); col.push_back(255); }
        else   { col.push_back(170); col.push_back(170); col.push_back(170); col.push_back(255); }
    }
}

void BrushDibujarMarcas(Mesh* m, int modo, float ladoPx, const unsigned char* colorRV,
                        WPProyector proy, void* ctx) {
    if (!m || !proy || modo == MarcasOff || m->vertexSize <= 0 || ladoPx <= 1.0f) return;
    const float mitad = ladoPx * 0.5f;
    const float mitadBorde = mitad + 1.0f * (float)GlobalScale; // el borde negro asoma alrededor

    static std::vector<float> triFondo, triRelleno;
    static std::vector<unsigned char> colRelleno;
    triFondo.clear(); triRelleno.clear(); colRelleno.clear();

    if (modo == MarcasVertice) {
        // UNO POR CONTROL-POINT: el peso es del vertice, no del corner, asi que dos corners
        // del mismo vertice no pueden mostrar cosas distintas -> se dibuja uno solo.
        WeightPaintAsegurarMapa(m);
        int maxCP = -1;
        for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
            if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
        if (maxCP < 0) return;
        std::vector<char> visto((size_t)maxCP + 1, 0);
        for (int i = 0; i < m->vertexSize && i < (int)m->vertCtrlPoint.size(); i++) {
            const int cp = m->vertCtrlPoint[i];
            if (cp < 0 || cp > maxCP || visto[(size_t)cp]) continue;
            float sx = 0.0f, sy = 0.0f;
            if (!proy(ctx, i, sx, sy)) continue;   // detras de camara / back-facing: no se ve
            visto[(size_t)cp] = 1;
            MarcaEmpujar(triFondo, sx, sy, mitadBorde);
            MarcaEmpujar(triRelleno, sx, sy, mitad);
            MarcaColor(colRelleno, colorRV ? &colorRV[(size_t)i * 4] : NULL);
        }
    } else {
        // UNO POR FACE CORNER: la posicion sale del helper COMPARTIDO con el pincel, asi el
        // cuadradito que ves es exactamente el punto que el pincel testea.
        std::vector<float> cx, cy; std::vector<char> ok;
        WPCornersEnPantalla(m, proy, ctx, cx, cy, ok);
        int L = 0;
        for (size_t f = 0; f < m->faces3d.size(); f++)
            for (size_t k = 0; k < m->faces3d[f].idx.size(); k++, L++) {
                if (L >= (int)ok.size() || !ok[(size_t)L]) continue;
                MarcaEmpujar(triFondo,   cx[(size_t)L], cy[(size_t)L], mitadBorde);
                MarcaEmpujar(triRelleno, cx[(size_t)L], cy[(size_t)L], mitad);
                MarcaColor(colRelleno, colorRV ? &colorRV[(size_t)m->faces3d[f].idx[k] * 4] : NULL);
            }
    }
    if (triRelleno.empty()) return;

    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    // 1) el BORDE: los mismos cuadrados un poquito mas grandes, en negro, abajo de todo
    gfx::DisableArray(gfx::ColorArray);
    gfx::Color4f(0.0f, 0.0f, 0.0f, 1.0f);
    gfx::VertexPointer2f(0, &triFondo[0]);
    gfx::DrawTrianglesArray((int)(triFondo.size() / 2));
    // 2) el RELLENO: el color de cada punto (el peso pintado, o el vertex color)
    gfx::EnableArray(gfx::ColorArray);
    gfx::ColorPointer4ub(&colRelleno[0]);
    gfx::VertexPointer2f(0, &triRelleno[0]);
    gfx::DrawTrianglesArray((int)(triRelleno.size() / 2));
    gfx::DisableArray(gfx::ColorArray);
}

// ---------------------------------------------------------------------------
//  PESOS por control-point (sparse en VertexGroup::verts/pesos)
// ---------------------------------------------------------------------------
float PesoDe(Mesh* m, int grupo, int cp) {
    if (!m || grupo < 0 || grupo >= (int)m->vertexGroups.size() || cp < 0) return 0.0f;
    VertexGroup* vg = m->vertexGroups[grupo];
    for (size_t j = 0; j < vg->verts.size() && j < vg->pesos.size(); j++)
        if (vg->verts[j] == cp) return vg->pesos[j];
    return 0.0f;
}

void PesoAsignar(Mesh* m, int grupo, int cp, float w) {
    if (!m || grupo < 0 || grupo >= (int)m->vertexGroups.size() || cp < 0) return;
    if (w > 1.0f) w = 1.0f;
    VertexGroup* vg = m->vertexGroups[grupo];
    for (size_t j = 0; j < vg->verts.size(); j++) {
        if (vg->verts[j] != cp) continue;
        if (w <= 0.0f) { // peso 0 = BORRAR la entrada sparse (no acumular basura)
            vg->verts.erase(vg->verts.begin() + j);
            if (j < vg->pesos.size()) vg->pesos.erase(vg->pesos.begin() + j);
        } else if (j < vg->pesos.size()) vg->pesos[j] = w;
        return;
    }
    if (w > 0.0f) { vg->verts.push_back(cp); vg->pesos.push_back(w); } // entrada nueva
}

// ---------------------------------------------------------------------------
//  PESOS del UV GROUP (sparse por RENDER-VERT / CORNER, Mesh::uvGroups).
//  Los escribe el pincel del editor UV; los leen Armature2DAplicar (skinning 2D) y el relleno
//  de color del UV. El camino 3D (SkinearMesh / GLB / weight paint del viewport 3D) usa la OTRA
//  entidad (vertexGroups) y no mira esta: son dos grupos distintos, sin bake entre ellos.
// ---------------------------------------------------------------------------
float PesoUVDe(Mesh* m, int uvGrupo, int rv) {
    if (!m || uvGrupo < 0 || uvGrupo >= (int)m->uvGroups.size() || rv < 0) return 0.0f;
    UVGroup* ug = m->uvGroups[uvGrupo];
    if (!ug) return 0.0f;
    for (size_t j = 0; j < ug->verts.size() && j < ug->pesos.size(); j++)
        if (ug->verts[j] == rv) return ug->pesos[j];
    return 0.0f;
}

void PesoUVAsignar(Mesh* m, int uvGrupo, int rv, float w) {
    if (!m || uvGrupo < 0 || uvGrupo >= (int)m->uvGroups.size() || rv < 0) return;
    if (w > 1.0f) w = 1.0f;
    UVGroup* ug = m->uvGroups[uvGrupo];
    if (!ug) return;
    for (size_t j = 0; j < ug->verts.size(); j++) {
        if (ug->verts[j] != rv) continue;
        if (w <= 0.0f) { // peso 0 = BORRAR la entrada sparse (no acumular basura)
            ug->verts.erase(ug->verts.begin() + j);
            if (j < ug->pesos.size()) ug->pesos.erase(ug->pesos.begin() + j);
        } else if (j < ug->pesos.size()) ug->pesos[j] = w;
        return;
    }
    if (w > 0.0f) { ug->verts.push_back(rv); ug->pesos.push_back(w); } // entrada nueva
}

void UVGroupLimpiarPesos(Mesh* m, int uvGrupo) {
    if (!m || uvGrupo < 0 || uvGrupo >= (int)m->uvGroups.size()) return;
    UVGroup* ug = m->uvGroups[uvGrupo];
    if (!ug) return;
    ug->verts.clear(); ug->pesos.clear();
}

// (WeightPaintAsegurarMapa se MUDO a main/edit/MeshEdit.cpp. Motivo: la llama la
//  CARGA de una malla con esqueleto, o sea tambien el runtime de un juego
//  compilado, y este .cpp es todo herramienta INTERACTIVA del editor -- pincel,
//  popup, undo -- que un juego no linkea. Es dato de la malla, no del pincel.)

// ---------------------------------------------------------------------------
//  "EDITAR SOLO LO SELECCIONADO": toggle global compartido por la toolbar del 3D
//  (Weight Paint) y la del UV editor (modo pintura), rol TBR_SoloSel. Default OFF.
// ---------------------------------------------------------------------------
static bool g_wpSoloSel = false;
bool& WeightPaintSoloSel() { return g_wpSoloSel; }

// control-points PERMITIDOS por la mascara: los de las caras logicas (faces3d) marcadas en
// 'fsel'. Si fsel es NULL se deriva de la EDIT MESH (faceSel via faceSrc = la seleccion de
// caras de edit mode, que persiste al cambiar a Weight Paint).
// caras seleccionadas en EDIT MODE (por indice de faces3d). Es la fuente unica de la que
// salen las dos mascaras del pincel Y el velo de caras bloqueadas: si divergieran, se veria
// bloqueada una cara que si se puede pintar (o al reves).
static void WPCarasSel(Mesh* m, const std::vector<char>* fsel, std::vector<char>& out) {
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
void BrushDibujarCarasBloqueadas(Mesh* m, WPProyector proy, void* ctx,
                                 const std::vector<char>* soloCaras) {
    if (!m || !proy || !g_wpSoloSel || m->vertexSize <= 0) return;
    std::vector<char> sel;
    WPCarasSel(m, soloCaras, sel);

    static std::vector<float> tri;
    tri.clear();
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        if (f < sel.size() && sel[f]) continue;          // esta cara SI se puede pintar
        const std::vector<int>& id = m->faces3d[f].idx;
        const size_t n = id.size();
        if (n < 3) continue;
        static std::vector<float> px, py;
        px.clear(); py.clear();
        bool todos = true;
        for (size_t k = 0; k < n; k++) {
            float sx = 0.0f, sy = 0.0f;
            // el proyector descarta lo que mira para el otro lado: una cara de atras no se
            // oscurece (tampoco se pinta, asi que no hay nada que avisar ahi)
            if (id[k] < 0 || id[k] >= m->vertexSize || !proy(ctx, id[k], sx, sy)) { todos = false; break; }
            px.push_back(sx); py.push_back(sy);
        }
        if (!todos) continue;
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
    std::vector<char> propia;
    if (!fsel) {
        propia.assign(m->faces3d.size(), 0);
        m->EnsureEdit();
        if (m->edit)
            for (size_t f = 0; f < m->edit->faceSel.size(); f++)
                if (m->edit->faceSel[f] && f < m->edit->faceSrc.size()) {
                    int f3 = m->edit->faceSrc[f];
                    if (f3 >= 0 && f3 < (int)m->faces3d.size()) propia[f3] = 1;
                }
        fsel = &propia;
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
    std::vector<char> propia;
    if (!fsel) {
        propia.assign(m->faces3d.size(), 0);
        m->EnsureEdit();
        if (m->edit)
            for (size_t f = 0; f < m->edit->faceSel.size(); f++)
                if (m->edit->faceSel[f] && f < m->edit->faceSrc.size()) {
                    int f3 = m->edit->faceSrc[f];
                    if (f3 >= 0 && f3 < (int)m->faces3d.size()) propia[f3] = 1;
                }
        fsel = &propia;
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

bool PincelAplicar(Mesh* m, int grupo, float centroX, float centroY, float radioPx,
                   float fuerza01, WPModo modo, WPProyector proy, void* ctx,
                   const std::vector<char>* soloCaras, const W3dFalloff* falloff) {
    if (!m || !proy || m->vertexSize <= 0 || radioPx <= 0.0f) return false;
    WeightPaintAsegurarMapa(m);
    if (grupo < 0 || grupo >= (int)m->vertexGroups.size()) return false;
    if (fuerza01 < 0.0f) fuerza01 = 0.0f;
    if (fuerza01 > 1.0f) fuerza01 = 1.0f;
    // valor 0: en +/- el delta seria 0 y no hay nada que hacer; en "=" SI hay (deja los
    // pesos en cero exacto, o sea BORRA lo pintado), asi que ese modo no se corta aca.
    if (modo != WPIgualar && fuerza01 <= 0.0f) return false;

    int maxCP = -1;
    for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
        if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
    if (maxCP < 0) return false;

    // MASCARA "solo lo seleccionado" (toggle ON): control-points de caras seleccionadas.
    // Con la mascara vacia (ninguna cara seleccionada) el pincel no pinta nada.
    std::vector<char> cpOk;
    if (g_wpSoloSel) WPMaskCPs(m, soloCaras, maxCP, cpOk);

    // 1) falloff MAXIMO por control-point: los splits de un mismo CP no acumulan doble
    const W3dFalloff& fo = FalloffODefault(falloff);
    std::vector<float> fall((size_t)maxCP + 1, 0.0f);
    const float r2 = radioPx * radioPx;
    bool alguno = false;
    for (int i = 0; i < m->vertexSize && i < (int)m->vertCtrlPoint.size(); i++) {
        int cp = m->vertCtrlPoint[i];
        if (cp < 0 || cp > maxCP) continue;
        if (!cpOk.empty() && !cpOk[(size_t)cp]) continue; // mascara: cara no seleccionada
        float sx = 0.0f, sy = 0.0f;
        if (!proy(ctx, i, sx, sy)) continue;   // detras de camara / back-facing: no se pinta
        float dx = sx - centroX, dy = sy - centroY;
        float d2 = dx * dx + dy * dy;
        if (d2 > r2) continue;
        // la CURVA elegida decide cuanto entra este vert: t = 0 en el centro, 1 en el borde
        float f = fo.Eval(sqrtf(d2) / radioPx);
        if (f > fall[(size_t)cp]) { fall[(size_t)cp] = f; alguno = true; }
    }
    if (!alguno) return false;

    // 2) aplicar UNA vez por control-point (clamp 0..1; entrada sparse creada/borrada)
    bool cambio = false;
    const size_t dom = (size_t)maxCP + 1;
    for (int cp = 0; cp <= maxCP; cp++) {
        float f = fall[(size_t)cp];
        if (f <= 0.0f) continue;
        float w0 = PesoDe(m, grupo, cp);
        // TOPE POR TRAZO: de este control-point ya puede haberse ocupado una pasada anterior
        // del mismo trazo (el mouse pasa decenas de veces por el mismo lugar). Solo se aplica
        // lo que FALTA para llegar a valor*falloff. "=" no necesita tope: deja el valor exacto,
        // aplicarlo dos veces da lo mismo.
        float paso = fuerza01 * f;
        if (modo != WPIgualar) {
            paso = TrazoPendiente((size_t)cp, dom, paso);
            if (paso <= 0.0f) continue;
        }
        float w = (modo == WPIgualar) ? fuerza01
                : (modo == WPSumar)   ? (w0 + paso)
                                      : (w0 - paso);
        if (w < 0.0f) w = 0.0f;
        if (w > 1.0f) w = 1.0f;
        if (w != w0) { PesoAsignar(m, grupo, cp, w); cambio = true; }
    }
    if (cambio) {
        // los pesos cambiaron: invalidar el CSR de skinning (su firma no hashea los valores)
        // y forzar re-skin -> si la malla esta posada, la deformacion refleja la pintura al toque
        m->skinGeomVersion++;
        m->lastSkinFrame = -999999;
        g_redraw = true;
    }
    return cambio;
}

// ---------------------------------------------------------------------------
//  PINCEL DE VERTEX COLOR (por CORNER, sobre una ColorLayer). Ver WeightPaint.h.
// ---------------------------------------------------------------------------
bool PincelAplicarColor(Mesh* m, int capa, float centroX, float centroY, float radioPx,
                        float valor01, const unsigned char* rgba, int palIdx,
                        WPProyector proy, void* ctx,
                        const std::vector<char>* soloCaras, const W3dFalloff* falloff) {
    if (!m || !proy || !rgba || m->vertexSize <= 0 || radioPx <= 0.0f) return false;
    if (capa < 0 || capa >= (int)m->colorLayers.size()) return false;
    ColorLayer* cl = m->colorLayers[capa];
    if (!cl) return false;
    const int nC = m->ContarCorners();
    if (nC <= 0 || (int)cl->color.size() != nC * 4) return false;   // capa stale: no tocar
    if (valor01 < 0.0f) valor01 = 0.0f;
    if (valor01 > 1.0f) valor01 = 1.0f;
    if (valor01 <= 0.0f) return false;
    if (cl->porIndice && (int)cl->indice.size() != nC) cl->indice.assign((size_t)nC, -1);

    const W3dFalloff& fo = FalloffODefault(falloff);
    std::vector<char> rvOk;
    if (g_wpSoloSel) WPMaskRVs(m, soloCaras, rvOk);   // "solo lo seleccionado": por render-vert

    // 1) cuanto le toca a cada CORNER (en el orden de faces3d, que es como indexa la capa).
    //    LA POSICION DEL CORNER ES LA DEL CUADRADITO, no la del vertice: los 2-3 corners que
    //    comparten una esquina caen en el MISMO punto del vertice, asi que testear ahi hacia
    //    imposible pintar uno solo (agarraba los 3 juntos aunque apuntaras a un cuadrado).
    //    Con capa Per-Vertex es al reves: ahi el punto ES el vertice (un vertice = un color) y
    //    el bloque 2) de abajo reparte lo pintado a todos sus corners.
    std::vector<float> peso((size_t)nC, 0.0f);
    const float r2 = radioPx * radioPx;
    bool alguno = false;
    std::vector<float> cx, cy; std::vector<char> vis;
    if (!cl->porVertice) WPCornersEnPantalla(m, proy, ctx, cx, cy, vis);
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& id = m->faces3d[f].idx;
        for (size_t c = 0; c < id.size(); c++, L++) {
            if (L >= nC) break;
            const int rv = id[c];
            if (rv < 0 || rv >= m->vertexSize) continue;
            if (!rvOk.empty() && !rvOk[(size_t)rv]) continue;   // mascara: cara no seleccionada
            float sx = 0.0f, sy = 0.0f;
            if (!cl->porVertice) {
                if (L >= (int)vis.size() || !vis[(size_t)L]) continue;  // corner no visible
                sx = cx[(size_t)L]; sy = cy[(size_t)L];                 // el punto del cuadradito
            } else if (!proy(ctx, rv, sx, sy)) continue;                // Per-Vertex: el vertice
            const float dx = sx - centroX, dy = sy - centroY;
            const float d2 = dx * dx + dy * dy;
            if (d2 > r2) continue;
            const float a = valor01 * fo.Eval(sqrtf(d2) / radioPx);
            if (a > peso[(size_t)L]) { peso[(size_t)L] = a; alguno = true; }
        }
    }
    if (!alguno) return false;

    // 2) POR VERTICE: la capa igual guarda por corner, pero al hornear se colapsa por posicion
    //    (todos los corners de una posicion toman el color del primero). Si se pintara solo el
    //    corner tocado, el resultado dependeria de cual corner es el primero -> se propaga el
    //    maximo a TODOS los corners que comparten posicion. Es "un vertice = un color".
    if (cl->porVertice) {
        WeightPaintAsegurarMapa(m);
        int maxCP = -1;
        for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
            if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
        if (maxCP >= 0) {
            std::vector<float> porCP((size_t)maxCP + 1, 0.0f);
            L = 0;
            for (size_t f = 0; f < m->faces3d.size(); f++)
                for (size_t c = 0; c < m->faces3d[f].idx.size(); c++, L++) {
                    if (L >= nC) break;
                    const int rv = m->faces3d[f].idx[c];
                    if (rv < 0 || rv >= (int)m->vertCtrlPoint.size()) continue;
                    const int cp = m->vertCtrlPoint[rv];
                    if (cp >= 0 && cp <= maxCP && peso[(size_t)L] > porCP[(size_t)cp])
                        porCP[(size_t)cp] = peso[(size_t)L];
                }
            L = 0;
            for (size_t f = 0; f < m->faces3d.size(); f++)
                for (size_t c = 0; c < m->faces3d[f].idx.size(); c++, L++) {
                    if (L >= nC) break;
                    const int rv = m->faces3d[f].idx[c];
                    if (rv < 0 || rv >= (int)m->vertCtrlPoint.size()) continue;
                    const int cp = m->vertCtrlPoint[rv];
                    if (cp >= 0 && cp <= maxCP) peso[(size_t)L] = porCP[(size_t)cp];
                }
        }
    }

    // 3) escribir: mezcla del color (o el indice entero, que no se puede mezclar)
    bool cambio = false;
    for (int i = 0; i < nC; i++) {
        const float a = peso[(size_t)i];
        if (a <= 0.0f) continue;
        if (cl->porIndice) {
            if (a < 0.5f) continue;                 // el falloff decide QUIEN, no cuanto
            if (cl->indice[(size_t)i] != palIdx) { cl->indice[(size_t)i] = palIdx; cambio = true; }
            for (int q = 0; q < 4; q++) {
                if (cl->color[(size_t)i * 4 + q] == rgba[q]) continue;
                cl->color[(size_t)i * 4 + q] = rgba[q]; cambio = true;
            }
            continue;
        }
        // TOPE POR TRAZO en una MEZCLA: no se puede sumar el pendiente y listo (mezclar dos
        // veces al 50% no da 100%). Se aplica t = (a - ya) / (1 - ya), que es exactamente el
        // factor que hace que la mezcla ACUMULADA del trazo termine valiendo 'a'.
        const float ya = (i < (int)g_trazoAplic.size() && g_trazoAplic.size() == (size_t)nC)
                         ? g_trazoAplic[(size_t)i] : 0.0f;
        const float pend = TrazoPendiente((size_t)i, (size_t)nC, a);
        if (pend <= 0.0f) continue;
        const float t = (ya >= 0.999f) ? 1.0f : (pend / (1.0f - ya));
        for (int q = 0; q < 4; q++) {
            const float viejo = (float)cl->color[(size_t)i * 4 + q];
            const float nuevo = viejo + ((float)rgba[q] - viejo) * t;
            const unsigned char b = (unsigned char)(nuevo + 0.5f);
            if (b != cl->color[(size_t)i * 4 + q]) { cl->color[(size_t)i * 4 + q] = b; cambio = true; }
        }
    }
    if (cambio) {
        m->AplicarCapasAlRender();   // la capa -> vertexColor[] (lo unico que el core dibuja)
        g_redraw = true;
    }
    return cambio;
}

// FUSION Per-Corner -> Per-Vertex: promedia los colores de los corners que comparten posicion
// y se los escribe a TODOS. Ver WeightPaint.h: esto es lo que hace destructivo al cambio.
bool VertexColorFusionarPorVertice(Mesh* m, int capa) {
    if (!m || capa < 0 || capa >= (int)m->colorLayers.size()) return false;
    ColorLayer* cl = m->colorLayers[capa];
    if (!cl) return false;
    const int nC = m->ContarCorners();
    if (nC <= 0 || (int)cl->color.size() != nC * 4) return false;
    WeightPaintAsegurarMapa(m);
    int maxCP = -1;
    for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
        if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
    if (maxCP < 0) return false;

    // suma por control-point (4 canales) + cuantos corners aporto cada uno
    std::vector<float> suma((size_t)(maxCP + 1) * 4, 0.0f);
    std::vector<int>   n((size_t)maxCP + 1, 0);
    int L = 0;
    for (size_t f = 0; f < m->faces3d.size(); f++)
        for (size_t c = 0; c < m->faces3d[f].idx.size(); c++, L++) {
            if (L >= nC) break;
            const int rv = m->faces3d[f].idx[c];
            if (rv < 0 || rv >= (int)m->vertCtrlPoint.size()) continue;
            const int cp = m->vertCtrlPoint[rv];
            if (cp < 0 || cp > maxCP) continue;
            for (int q = 0; q < 4; q++) suma[(size_t)cp * 4 + q] += (float)cl->color[(size_t)L * 4 + q];
            n[(size_t)cp]++;
        }
    bool cambio = false;
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
                    (unsigned char)(suma[(size_t)cp * 4 + q] / (float)n[(size_t)cp] + 0.5f);
                if (prom != cl->color[(size_t)L * 4 + q]) { cl->color[(size_t)L * 4 + q] = prom; cambio = true; }
            }
        }
    // el INDICE de paleta no se puede promediar (no existe "el promedio de dos indices"): al
    // fusionar, esos corners pasan a color libre con el promedio ya horneado.
    if (cl->porIndice && (int)cl->indice.size() == nC)
        for (int i = 0; i < nC; i++) cl->indice[(size_t)i] = -1;
    if (cambio) { m->AplicarCapasAlRender(); g_redraw = true; }
    return cambio;
}

// re-hornea los colores desde los INDICES contra la paleta efectiva del objeto: ESTO es el
// palette-swap (cambiar la paleta de un padre re-pinta a todos sus herederos).
bool VertexColorResolverIndices(Mesh* m, int capa, Object* dueno) {
    if (!m || capa < 0 || capa >= (int)m->colorLayers.size()) return false;
    ColorLayer* cl = m->colorLayers[capa];
    if (!cl || !cl->porIndice) return false;
    const int nC = m->ContarCorners();
    if (nC <= 0 || (int)cl->color.size() != nC * 4) return false;
    if ((int)cl->indice.size() != nC) return false;
    std::vector<PaletaColor>* cols = W3dColoresEfectivos(dueno ? dueno : (Object*)m);
    bool cambio = false;
    for (int i = 0; i < nC; i++) {
        const int idx = cl->indice[(size_t)i];
        if (idx < 0 || !cols || idx >= (int)cols->size()) continue;  // sin indice: color libre, no se toca
        const float* c = (*cols)[(size_t)idx].rgba;
        for (int q = 0; q < 4; q++) {
            float v = c[q]; if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            const unsigned char b = (unsigned char)(v * 255.0f + 0.5f);
            if (b != cl->color[(size_t)i * 4 + q]) { cl->color[(size_t)i * 4 + q] = b; cambio = true; }
        }
    }
    if (cambio) { m->AplicarCapasAlRender(); g_redraw = true; }
    return cambio;
}

// ---------------------------------------------------------------------------
//  PINCEL DEL EDITOR UV (por CORNER, sobre un UV GROUP). Misma pasada/falloff que el de arriba,
//  pero la unidad es el RENDER-VERT: los 4 corners de UNA cara se pesan sin tocar las caras
//  vecinas que comparten esos vertices 3D. No mira ni toca los vertex groups.
// ---------------------------------------------------------------------------
bool PincelAplicarUV(Mesh* m, int uvGrupo, float centroX, float centroY, float radioPx,
                     float fuerza01, WPModo modo, WPProyector proy, void* ctx,
                     const std::vector<char>* soloCaras, const W3dFalloff* falloff) {
    if (!m || !proy || m->vertexSize <= 0 || radioPx <= 0.0f) return false;
    if (uvGrupo < 0 || uvGrupo >= (int)m->uvGroups.size()) return false;
    if (fuerza01 < 0.0f) fuerza01 = 0.0f;
    if (fuerza01 > 1.0f) fuerza01 = 1.0f;
    if (modo != WPIgualar && fuerza01 <= 0.0f) return false; // "=" con 0 SI hace algo (borra)

    std::vector<char> rvOk;
    const W3dFalloff& fo = FalloffODefault(falloff);
    if (g_wpSoloSel) WPMaskRVs(m, soloCaras, rvOk);
    // falloff por render-vert (cada uno se proyecta a SU posicion en pantalla; no hay
    // "maximo entre splits" que valga: los splits son justamente lo que se separa)
    std::vector<float> fall((size_t)m->vertexSize, 0.0f);
    const float r2 = radioPx * radioPx;
    bool alguno = false;
    for (int i = 0; i < m->vertexSize; i++) {
        if (!rvOk.empty() && !rvOk[(size_t)i]) continue; // mascara: cara no seleccionada
        float sx = 0.0f, sy = 0.0f;
        if (!proy(ctx, i, sx, sy)) continue;
        float dx = sx - centroX, dy = sy - centroY;
        float d2 = dx * dx + dy * dy;
        if (d2 > r2) continue;
        fall[(size_t)i] = fo.Eval(sqrtf(d2) / radioPx); // la CURVA elegida (0 centro .. 1 borde)
        alguno = true;
    }
    if (!alguno) return false;
    // peso DENSO del grupo para no hacer la busqueda lineal de PesoUVDe dentro del loop
    UVGroup* ug = m->uvGroups[uvGrupo];
    std::vector<float> wIni((size_t)m->vertexSize, 0.0f);
    if (ug) for (size_t k = 0; k < ug->verts.size() && k < ug->pesos.size(); k++) {
        int rv = ug->verts[k];
        if (rv >= 0 && rv < m->vertexSize) wIni[(size_t)rv] = ug->pesos[k];
    }
    bool cambio = false;
    for (int i = 0; i < m->vertexSize; i++) {
        float f = fall[(size_t)i];
        if (f <= 0.0f) continue;
        float w0 = wIni[(size_t)i];
        float paso = fuerza01 * f;                                // idem 3D: tope por trazo
        if (modo != WPIgualar) {
            paso = TrazoPendiente((size_t)i, (size_t)m->vertexSize, paso);
            if (paso <= 0.0f) continue;
        }
        float w = (modo == WPIgualar) ? fuerza01                 // "=": exacto, sin falloff
                : (modo == WPSumar)   ? (w0 + paso)
                                      : (w0 - paso);
        if (w < 0.0f) w = 0.0f;
        if (w > 1.0f) w = 1.0f;
        if (w != w0) { PesoUVAsignar(m, uvGrupo, i, w); cambio = true; }
    }
    // NO se tocan skinGeomVersion/lastSkinFrame ni se escribe mesh->uv: pintar pesos NO mueve
    // UVs. El caller (editor UV) re-aplica el skinning 2D SOLO si hay una pose real que dependa
    // de estos pesos (ver el invariante uv = f(uv2dRest, pose) en Mesh.h).
    if (cambio) g_redraw = true;
    return cambio;
}

// ---------------------------------------------------------------------------
//  TRAZO (un undo por trazo: snapshot al mouse-down, commit al soltar)
// ---------------------------------------------------------------------------
static Mesh* g_wpTrazoMesh = NULL;

int WeightPaintTrazoIniciar(Mesh* m) {
    BrushTrazoResetear();
    if (!m || m->vertexSize <= 0) return -1;
    WeightPaintAsegurarMapa(m);
    UndoPesosIniciar(m); // snapshot ANTES de crear el grupo -> el undo del trazo tambien lo saca
    if (m->vertexGroups.empty()) {
        CrearVertexGroup(m); // "Group" (nombre unico) + queda activo
        w3dLogf("[weightpaint] '%s' sin vertex groups: se creo '%s' automaticamente",
                m->name.c_str(), m->vertexGroups[0]->nombre.c_str());
    }
    if (m->grupoActivo < 0 || m->grupoActivo >= (int)m->vertexGroups.size()) m->grupoActivo = 0;
    g_wpTrazoMesh = m;
    return m->grupoActivo;
}

// TRAZO del EDITOR UV: la entidad es el UV GROUP. Sin ninguno, el primer trazo crea uno con el
// NOMBRE DEL HUESO 2D ACTIVO si la malla tiene armature 2D (asi el binding por nombre queda
// hecho y pintar deforma al toque); sin armature 2D se llama "UV Group".
int WeightPaintTrazoIniciarUV(Mesh* m) {
    BrushTrazoResetear();
    if (!m || m->vertexSize <= 0) return -1;
    UndoPesosIniciar(m); // snapshot ANTES de crear el grupo -> el undo del trazo tambien lo saca
    if (m->uvGroups.empty()) {
        std::string base = "UV Group";
        if (m->Arm2DBoneActivo() >= 0 && m->Arm2DBoneActivo() < (int)m->Arm2DHuesos().size())
            base = m->Arm2DHuesos()[m->Arm2DBoneActivo()].nombre;
        else if (!m->Arm2DHuesos().empty()) base = m->Arm2DHuesos()[0].nombre;
        CrearUVGroup(m, base); // nombre unico + queda activo
        w3dLogf("[weightpaint] '%s' sin UV groups: se creo '%s' automaticamente",
                m->name.c_str(), m->uvGroups[0]->nombre.c_str());
    }
    if (m->uvGrupoActivo < 0 || m->uvGrupoActivo >= (int)m->uvGroups.size()) m->uvGrupoActivo = 0;
    g_wpTrazoMesh = m;
    return m->uvGrupoActivo;
}

void WeightPaintTrazoFin() {
    if (!g_wpTrazoMesh) return;
    UndoPesosConfirmar(); // pushea el trazo (descarta si no cambio nada)
    g_wpTrazoMesh = NULL;
}

bool WeightPaintTrazoActivo() { return g_wpTrazoMesh != NULL; }

// ---------------------------------------------------------------------------
//  ASSIGN / REMOVE / SELECT / DESELECT de los DOS grupos de pesos
//  (tarjetas "Vertex Groups" y "UV Groups" del panel Properties; ver WeightPaint.h)
// ---------------------------------------------------------------------------
bool UVVertsSelEfectivos(Mesh* m, std::vector<char>& sv);  // (decl. en ViewPorts/UVEditor.h)

// ---- VERTEX GROUPS (control-points; seleccion de EDIT MODE del 3D) ----
int VertexGroupAsignarSel(Mesh* m, bool asignar) {
    if (!m || m->vertexGroups.empty()) return 0;
    if (m->grupoActivo < 0 || m->grupoActivo >= (int)m->vertexGroups.size()) return 0;
    if ((Object*)m != g_editMesh || !m->edit) return 0;   // sin Edit Mode no hay seleccion que asignar
    WeightPaintAsegurarMapa(m);                            // render-vert -> control-point (lazy)
    const int nV = m->vertexSize;
    UndoPesosIniciar(m);
    int n = 0;
    std::vector<char> visto;                               // un control-point se toca UNA vez
    for (size_t k = 0; k < m->edit->editVerts.size(); k++) {
        if (k >= m->edit->vertSel.size() || !m->edit->vertSel[k]) continue;
        int rv = m->edit->editVerts[k];
        if (rv < 0 || rv >= nV) continue;
        int cp = ((int)m->vertCtrlPoint.size() == nV) ? m->vertCtrlPoint[rv] : rv;
        if (cp < 0) continue;
        if ((int)visto.size() <= cp) visto.resize(cp + 1, 0);
        if (visto[cp]) continue;
        visto[cp] = 1;
        PesoAsignar(m, m->grupoActivo, cp, asignar ? 1.0f : 0.0f); // 0 BORRA la entrada sparse
        n++;
    }
    UndoPesosConfirmar();
    // los pesos cambiaron: MISMA invalidacion que el pincel (ver PincelAplicar) -> si la malla
    // esta posada, la deformacion refleja el assign al toque
    if (n) { m->skinGeomVersion++; m->lastSkinFrame = -999999; g_redraw = true; }
    return n;
}

int VertexGroupSeleccionar(Mesh* m, bool sel) {
    if (!m || m->vertexGroups.empty()) return 0;
    if (m->grupoActivo < 0 || m->grupoActivo >= (int)m->vertexGroups.size()) return 0;
    if ((Object*)m != g_editMesh || !m->edit) return 0;
    WeightPaintAsegurarMapa(m);
    const int nV = m->vertexSize;
    const VertexGroup* g = m->vertexGroups[m->grupoActivo];
    if (!g) return 0;
    // set de control-points con peso > 0 (denso por indice: son pocos y el lookup es O(1))
    std::vector<char> pesado;
    for (size_t i = 0; i < g->verts.size() && i < g->pesos.size(); i++) {
        int cp = g->verts[i];
        if (cp < 0 || g->pesos[i] <= 0.0f) continue;
        if ((int)pesado.size() <= cp) pesado.resize(cp + 1, 0);
        pesado[cp] = 1;
    }
    int n = 0;
    for (size_t k = 0; k < m->edit->editVerts.size(); k++) {
        int rv = m->edit->editVerts[k];
        if (rv < 0 || rv >= nV) continue;
        int cp = ((int)m->vertCtrlPoint.size() == nV) ? m->vertCtrlPoint[rv] : rv;
        if (cp < 0 || cp >= (int)pesado.size() || !pesado[cp]) continue;
        if (k < m->edit->vertSel.size()) { m->edit->vertSel[k] = sel ? 1 : 0; n++; }
    }
    if (n) { m->edit->Recolorear(); g_redraw = true; }
    return n;
}

// ---- UV GROUPS (render-verts / corners; seleccion del editor UV) ----
int UVGroupAsignarSel(Mesh* m, bool asignar) {
    if (!m || m->uvGroups.empty()) return 0;
    if (m->uvGrupoActivo < 0 || m->uvGrupoActivo >= (int)m->uvGroups.size()) return 0;
    std::vector<char> sv;
    if (!UVVertsSelEfectivos(m, sv)) return 0;             // nada seleccionado por ningun camino
    UndoPesosIniciar(m);
    int n = 0;
    for (int i = 0; i < m->vertexSize && i < (int)sv.size(); i++) {
        if (!sv[i]) continue;
        PesoUVAsignar(m, m->uvGrupoActivo, i, asignar ? 1.0f : 0.0f); // 0 BORRA la entrada
        n++;
    }
    UndoPesosConfirmar();
    if (n) g_redraw = true;
    return n;
}

int UVGroupSeleccionar(Mesh* m, bool sel) {
    if (!m || m->uvGroups.empty()) return 0;
    if (m->uvGrupoActivo < 0 || m->uvGrupoActivo >= (int)m->uvGroups.size()) return 0;
    const int nV = m->vertexSize;
    if (nV <= 0) return 0;
    const UVGroup* g = m->uvGroups[m->uvGrupoActivo];
    if (!g) return 0;
    if ((int)m->uvSelVert.size() != nV) m->uvSelVert.assign(nV, 0);
    int n = 0;
    for (size_t i = 0; i < g->verts.size() && i < g->pesos.size(); i++) {
        int rv = g->verts[i];
        if (rv < 0 || rv >= nV || g->pesos[i] <= 0.0f) continue;
        m->uvSelVert[rv] = sel ? 1 : 0;
        n++;
    }
    if (n) g_redraw = true;
    return n;
}

// ---------------------------------------------------------------------------
//  MENUS del pincel (compartidos por la toolbar del 3D y la del UV editor)
// ---------------------------------------------------------------------------
static PopupMenu* gMenuBrushGrupo  = NULL; // 3D: vertex groups
static PopupMenu* gMenuBrushUVGrp  = NULL; // UV: uv groups
static Mesh*      gMenuGrupoMesh   = NULL; // la malla cuyo dropdown de grupos esta abierto

// abre 'menu' desde la toolbar: crece hacia ARRIBA del boton (mismo criterio que el
// menu Orient de la toolbar del 3D) para no taparse con la barra ni salirse de pantalla.
static void AbrirMenuToolbar(PopupMenu* menu, int sx, int syTop) {
    if (!menu) return;
    if (MenuAbierto && MenuAbierto != menu) MenuAbierto->Cerrar();
    menu->Resize();
    int my = syTop - menu->height;
    if (my < 0) my = 0;
    menu->Abrir(sx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = menu;
    g_redraw = true;
}

static void AccionMenuGrupo(int id) {
    Mesh* m = gMenuGrupoMesh;
    if (!m) return;
    if (id == 1000) {           // New Group: crea uno y lo deja activo (como la tarjeta de Properties)
        CrearVertexGroup(m);
    } else if (id >= 0 && id < (int)m->vertexGroups.size()) {
        m->grupoActivo = id;    // mismo efecto que elegirlo en la tarjeta Vertex Groups
    }
    g_redraw = true;
}

// desplegable del VIEWPORT 3D (Weight Paint): SOLO vertex groups. Sin items que crucen con los
// UV groups: son dos entidades distintas y no se bakea una desde la otra.
void WeightPaintMenuGrupo(Mesh* m, int sx, int syTop) {
    if (!m) return;
    gMenuGrupoMesh = m;
    if (!gMenuBrushGrupo) gMenuBrushGrupo = new PopupMenu();
    gMenuBrushGrupo->Limpiar(); // se rearma cada vez (los grupos cambian; marca el activo en verde)
    gMenuBrushGrupo->titulo = T("Vertex Groups");
    for (size_t g = 0; g < m->vertexGroups.size(); g++)
        gMenuBrushGrupo->Agregar(m->vertexGroups[g]->nombre, (int)g)->verde = ((int)g == m->grupoActivo);
    gMenuBrushGrupo->Agregar(T("Add Vertex Group"), 1000, (int)IconType::mas);
    gMenuBrushGrupo->action = AccionMenuGrupo;
    AbrirMenuToolbar(gMenuBrushGrupo, sx, syTop);
}

// ---- CAPAS DE COLOR (vertex paint): elegir a cual se pinta, o crear una ----
static PopupMenu* gMenuCapaColor = NULL;
static void AccionMenuCapaColor(int id) {
    Mesh* m = gMenuGrupoMesh;
    if (!m) return;
    if (id == 1000) {                       // capa nueva: copia de la activa (misma regla que UV/color del panel)
        DuplicarColorLayerActivo(m);        // declarada en edit/MeshEdit.h (ya incluido)
    } else if (id >= 0 && id < (int)m->colorLayers.size()) {
        m->colorActivo = id;
        m->AplicarCapasAlRender();          // la capa elegida es la que se ve (y la que se pinta)
    }
    g_redraw = true;
}

void WeightPaintMenuCapaColor(Mesh* m, int sx, int syTop) {
    if (!m) return;
    gMenuGrupoMesh = m;
    if (!gMenuCapaColor) gMenuCapaColor = new PopupMenu();
    gMenuCapaColor->Limpiar();
    gMenuCapaColor->titulo = T("Color Layers");
    for (size_t i = 0; i < m->colorLayers.size(); i++)
        gMenuCapaColor->Agregar(m->colorLayers[i]->nombre, (int)i)->verde = ((int)i == m->colorActivo);
    if (!m->colorLayers.empty())
        gMenuCapaColor->Agregar(T("New Color Layer"), 1000, (int)IconType::mas);
    gMenuCapaColor->action = AccionMenuCapaColor;
    AbrirMenuToolbar(gMenuCapaColor, sx, syTop);
}

static void AccionMenuUVGrupo(int id) {
    Mesh* m = gMenuGrupoMesh;
    if (!m) return;
    if (id == 1000) {           // Add UV Group: crea uno y lo deja activo
        CrearUVGroup(m, "UV Group");
    } else if (id == 1001) {    // Clear: deja el UV group activo SIN pesos (un paso de undo)
        UndoPesosIniciar(m);
        UVGroupLimpiarPesos(m, m->uvGrupoActivo);
        UndoPesosConfirmar();
    } else if (id >= 0 && id < (int)m->uvGroups.size()) {
        m->uvGrupoActivo = id;  // mismo efecto que elegirlo en la tarjeta UV Groups
    }
    g_redraw = true;
}

// desplegable del EDITOR UV (modo pintura): SOLO uv groups (pesos por corner). "Clear" vacia el
// activo; NO hay ningun item que hornee pesos desde los vertex groups.
void WeightPaintMenuUVGroup(Mesh* m, int sx, int syTop) {
    if (!m) return;
    gMenuGrupoMesh = m;
    if (!gMenuBrushUVGrp) gMenuBrushUVGrp = new PopupMenu();
    gMenuBrushUVGrp->Limpiar();
    gMenuBrushUVGrp->titulo = T("UV Groups");
    for (size_t g = 0; g < m->uvGroups.size(); g++)
        gMenuBrushUVGrp->Agregar(m->uvGroups[g]->nombre, (int)g)->verde = ((int)g == m->uvGrupoActivo);
    gMenuBrushUVGrp->Agregar(T("Add UV Group"), 1000, (int)IconType::mas);
    if (m->uvGrupoActivo >= 0 && m->uvGrupoActivo < (int)m->uvGroups.size() &&
        !m->uvGroups[m->uvGrupoActivo]->verts.empty())
        gMenuBrushUVGrp->Agregar(T("Clear UV Group Weights"), 1001);
    gMenuBrushUVGrp->action = AccionMenuUVGrupo;
    AbrirMenuToolbar(gMenuBrushUVGrp, sx, syTop);
}

// los 3 labels del PINCEL (compartidos: el pincel es uno solo)
static void BrushLabels(std::string& tam, std::string& fuerza, std::string& modo) {
    char b[32];
    sprintf(b, "%dpx", (int)(g_brush.radioPx + 0.5f));          tam = b;
    sprintf(b, "%d%%", (int)(g_brush.fuerza * 100.0f + 0.5f));  fuerza = b;
    modo = (g_brush.modo == WPIgualar) ? "=" : (g_brush.modo == WPRestar) ? "-" : "+";
}

void WeightPaintLabels(Mesh* m, std::string& tam, std::string& fuerza,
                       std::string& modo, std::string& grupo) {
    BrushLabels(tam, fuerza, modo);
    grupo = "Group";
    if (m && m->grupoActivo >= 0 && m->grupoActivo < (int)m->vertexGroups.size())
        grupo = m->vertexGroups[m->grupoActivo]->nombre;
}

void WeightPaintLabelsUV(Mesh* m, std::string& tam, std::string& fuerza,
                         std::string& modo, std::string& grupo) {
    BrushLabels(tam, fuerza, modo);
    grupo = "UV Group";
    if (m && m->uvGrupoActivo >= 0 && m->uvGrupoActivo < (int)m->uvGroups.size())
        grupo = m->uvGroups[m->uvGrupoActivo]->nombre;
}
