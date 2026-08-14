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
bool PincelAplicar(Mesh* m, int grupo, float centroX, float centroY, float radioPx,
                   float fuerza01, bool sumar, WPProyector proy, void* ctx,
                   const std::vector<char>* soloCaras) {
    if (!m || !proy || m->vertexSize <= 0 || radioPx <= 0.0f) return false;
    WeightPaintAsegurarMapa(m);
    if (grupo < 0 || grupo >= (int)m->vertexGroups.size()) return false;
    if (fuerza01 < 0.0f) fuerza01 = 0.0f;
    if (fuerza01 > 1.0f) fuerza01 = 1.0f;
    if (fuerza01 <= 0.0f) return false;

    int maxCP = -1;
    for (size_t i = 0; i < m->vertCtrlPoint.size(); i++)
        if (m->vertCtrlPoint[i] > maxCP) maxCP = m->vertCtrlPoint[i];
    if (maxCP < 0) return false;

    // MASCARA "solo lo seleccionado" (toggle ON): control-points de caras seleccionadas.
    // Con la mascara vacia (ninguna cara seleccionada) el pincel no pinta nada.
    std::vector<char> cpOk;
    if (g_wpSoloSel) WPMaskCPs(m, soloCaras, maxCP, cpOk);

    // 1) falloff MAXIMO por control-point: los splits de un mismo CP no acumulan doble
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
        // falloff SMOOTHSTEP del centro (1) al borde (0): suave, sin escalones
        float s = 1.0f - sqrtf(d2) / radioPx;
        float f = s * s * (3.0f - 2.0f * s);
        if (f > fall[(size_t)cp]) { fall[(size_t)cp] = f; alguno = true; }
    }
    if (!alguno) return false;

    // 2) aplicar UNA vez por control-point (clamp 0..1; entrada sparse creada/borrada)
    bool cambio = false;
    for (int cp = 0; cp <= maxCP; cp++) {
        float f = fall[(size_t)cp];
        if (f <= 0.0f) continue;
        float w0 = PesoDe(m, grupo, cp);
        float w = sumar ? (w0 + fuerza01 * f) : (w0 - fuerza01 * f);
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
//  PINCEL DEL EDITOR UV (por CORNER, sobre un UV GROUP). Misma pasada/falloff que el de arriba,
//  pero la unidad es el RENDER-VERT: los 4 corners de UNA cara se pesan sin tocar las caras
//  vecinas que comparten esos vertices 3D. No mira ni toca los vertex groups.
// ---------------------------------------------------------------------------
bool PincelAplicarUV(Mesh* m, int uvGrupo, float centroX, float centroY, float radioPx,
                     float fuerza01, bool sumar, WPProyector proy, void* ctx,
                     const std::vector<char>* soloCaras) {
    if (!m || !proy || m->vertexSize <= 0 || radioPx <= 0.0f) return false;
    if (uvGrupo < 0 || uvGrupo >= (int)m->uvGroups.size()) return false;
    if (fuerza01 < 0.0f) fuerza01 = 0.0f;
    if (fuerza01 > 1.0f) fuerza01 = 1.0f;
    if (fuerza01 <= 0.0f) return false;

    std::vector<char> rvOk;
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
        float s = 1.0f - sqrtf(d2) / radioPx;   // falloff SMOOTHSTEP del centro (1) al borde (0)
        fall[(size_t)i] = s * s * (3.0f - 2.0f * s);
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
        float w = sumar ? (w0 + fuerza01 * f) : (w0 - fuerza01 * f);
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
static PopupMenu* gMenuBrushTam    = NULL;
static PopupMenu* gMenuBrushFuerza = NULL;
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

void WeightPaintMenuTam(int sx, int syTop) {
    if (!gMenuBrushTam) {
        gMenuBrushTam = new PopupMenu();
        gMenuBrushTam->titulo = T("Radius");
        // "menu deslizable": el mismo item-slider de los menus (AgregarFloat)
        gMenuBrushTam->AgregarFloat(T("Radius"), 0, &BrushGet().radioPx, 4.0f, 200.0f);
    }
    AbrirMenuToolbar(gMenuBrushTam, sx, syTop);
}

void WeightPaintMenuFuerza(int sx, int syTop) {
    if (!gMenuBrushFuerza) {
        gMenuBrushFuerza = new PopupMenu();
        gMenuBrushFuerza->titulo = T("Strength");
        gMenuBrushFuerza->AgregarFloat(T("Strength"), 0, &BrushGet().fuerza, 0.0f, 1.0f);
    }
    AbrirMenuToolbar(gMenuBrushFuerza, sx, syTop);
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
    modo = g_brush.modo ? "-" : "+";
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
