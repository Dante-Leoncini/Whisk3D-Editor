// ============================================================================
//  GIZMO DE TRANSFORMACION. Ver Gizmo.h para el contrato. C++03 (compila en Symbian).
// ============================================================================
#include "ui/ViewPorts/Gizmo.h"
#include "ui/ViewPorts/ViewPort3D.h"
#include "ui/ViewPorts/LayoutInput.h"   // EditXformStart / EditXformActivo
#include "ui/ViewPorts/TransformUI.h"   // ToolbarUsaTactil
#include "ui/ViewPorts/PopUp/PopUpBase.h"
#include "objects/ObjectMode.h"         // SetPosicion, SetTransformPivotPoint
#include "objects/EditMesh.h"
#include "objects/Mesh.h"
#include "app/variables.h"
#include "w3dGraphics.h"
#include "WhiskUI/draw/glesdraw.h"      // W3dDrawLinesF
#include "WhiskUI/core/UI.h"            // GlobalScale
#include <math.h>
#include <vector>

namespace gfx = w3dEngine;
extern bool g_redraw;

#ifdef W3D_SYMBIAN
bool g_gizmoOn = false;   // N95: apagado por defecto (pantalla chica, sin puntero)
#else
bool g_gizmoOn = true;
#endif

static bool g_gizmoArrastre = false;
static int  g_gizmoManija   = GizmoNinguna;
extern bool g_viewEditMode;   // toggle "vista" de la barra: el puntero navega -> el gizmo ni se dibuja ni se agarra

// ARRASTRE: la restriccion (eje / plano / plano de la vista) queda ANCLADA al pivote y a los ejes del momento del
// agarre, y el punto agarrado sobre ella sigue al puntero: lo que se mueve es (punto de hoy - punto del agarre).
// Asi el objeto queda pegado al dedo/mouse en vez de correr a una velocidad por pixel.
static Vector3 g_gizmoC0, g_gizmoEjes0[3], g_gizmoP0, g_gizmoAplicado;
static bool    g_gizmoTieneP0 = false;

// colores de los ejes (los mismos que las flechas de la grilla)
static const float kColX[3] = { 0.90f, 0.25f, 0.25f };
static const float kColY[3] = { 0.45f, 0.85f, 0.30f };
static const float kColZ[3] = { 0.30f, 0.50f, 0.95f };
static const float* ColEje(int k) { return (k == 0) ? kColX : (k == 1) ? kColY : kColZ; }

// ---------------------------------------------------------------------------
//  Base: centro (mundo) y ejes unitarios (mundo). false = no hay seleccion -> no hay gizmo
// ---------------------------------------------------------------------------
// OJO con la convencion del editor: el eje "Z" de la UI (azul, arriba) es el Y interno, y el "Y" de la UI
// (verde, horizontal) es el Z interno (asi lo muestran la grilla, el panel y los ejes X/Y/Z del transform).
// ejes[0] = X, ejes[1] = "Y" de la UI, ejes[2] = "Z" de la UI.
static const int kColInterno[3] = { 0, 2, 1 };   // columna de la matriz de cada eje de la UI
static bool GizmoBase(Vector3& c, Vector3 ejes[3]) {
    ejes[0] = Vector3(1, 0, 0); ejes[1] = Vector3(0, 0, 1); ejes[2] = Vector3(0, 1, 0);
    Matrix4 W; bool local = false;
    if (InteractionMode == ObjectMode) {
        if (!ObjActivo || !ObjActivo->select) return false;
        SetTransformPivotPoint(); c = TransformPivotPoint;
        if (transformOrientation == LocalOrient) { ObjActivo->GetWorldMatrix(W); local = true; }
    } else if (InteractionMode == EditMode && g_editMesh && g_editMesh->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return false;
        float cx = 0, cy = 0, cz = 0;
        if (!m->edit->CentroSeleccion(cx, cy, cz)) return false;
        m->GetWorldMatrix(W); c = W * Vector3(cx, cy, cz);
        if (transformOrientation == LocalOrient) local = true;
    } else return false;
    if (local) {
        for (int k = 0; k < 3; k++) {
            const int col = kColInterno[k];
            Vector3 e(W.m[col*4], W.m[col*4+1], W.m[col*4+2]);
            const float l = e.Length(); if (l > 1e-6f) ejes[k] = e * (1.0f / l);
        }
    }
    return true;
}

// unidades de MUNDO por pixel en el centro del gizmo (para el tamanio constante en pantalla)
static float UnidadesPorPixel(Viewport3D* vp, const Vector3& c) {
    CameraBase cam = vp->VistaCam();
    const Vector3 der = cam.rot * Vector3(1, 0, 0);
    float sx0, sy0, sx1, sy1;
    if (!vp->ProyectarPunto(c, sx0, sy0) || !vp->ProyectarPunto(c + der, sx1, sy1)) return 0.0f;
    const float d = sqrtf((sx1 - sx0) * (sx1 - sx0) + (sy1 - sy0) * (sy1 - sy0));
    return (d > 1e-4f) ? 1.0f / d : 0.0f;
}

// rayo de MUNDO que sale del pixel (lx,ly) (coords locales del viewport): la inversa exacta de ProyectarPunto
static bool RayoDesdePixel(Viewport3D* vp, float lx, float ly, Vector3& o, Vector3& d) {
    if (vp->width <= 0 || vp->height <= 0) return false;
    const Vector3 cr = vp->viewRot * Vector3(1, 0, 0);
    const Vector3 cu = vp->viewRot * Vector3(0, 1, 0);
    const Vector3 cf = vp->viewRot * Vector3(0, 0, -1);
    const float aspectR = (float)vp->width / (float)vp->height;
    const float ndcX = lx / (float)vp->width * 2.0f - 1.0f;
    const float ndcY = 1.0f - ly / (float)vp->height * 2.0f;
    const float fRad = fovDeg * 3.14159265f / 180.0f;
    if (vp->orthographic) {
        float size = vp->orbitDistance * tanf(fRad * 0.5f); if (size < 0.001f) size = 0.001f;
        o = vp->viewPos + cr * (ndcX * size * aspectR) + cu * (ndcY * size);
        d = cf;
    } else {
        const float f = 1.0f / tanf(fRad * 0.5f);
        o = vp->viewPos;
        d = (cf + cr * (ndcX * aspectR / f) + cu * (ndcY / f)).Normalized();
    }
    return true;
}

// punto de la restriccion de la manija (anclada en c0/ejes) que "toca" el rayo del pixel (lx,ly).
// eje: el punto del eje mas cercano al rayo. plano / centro: donde el rayo cruza el plano (el del centro es el
// plano de la vista que pasa por c0). false = no se puede seguir (eje mirando a la camara, plano de canto).
static bool PuntoRestriccion(Viewport3D* vp, int manija, const Vector3& c0, const Vector3 ejes[3], float lx, float ly, Vector3& P) {
    Vector3 o, d; if (!RayoDesdePixel(vp, lx, ly, o, d)) return false;
    if (manija >= GizmoEjeX && manija <= GizmoEjeZ) {
        const Vector3 a = ejes[manija - GizmoEjeX];
        const Vector3 w = c0 - o;
        const float b = a.Dot(d), den = 1.0f - b * b;
        if (den < 1e-4f) return false;
        P = c0 + a * ((b * w.Dot(d) - w.Dot(a)) / den);
        return true;
    }
    Vector3 n = vp->viewRot * Vector3(0, 0, -1);
    if (manija >= GizmoPlanoX && manija <= GizmoPlanoZ) n = ejes[manija - GizmoPlanoX];
    const float dn = d.Dot(n); if (fabsf(dn) < 1e-5f) return false;
    P = o + d * ((c0 - o).Dot(n) / dn);
    return true;
}

// medidas (en pixeles) de cada parte; en tactil las manijas son mas grandes
struct GizmoMedidas { float L, r0, base, cono, cuad, cuadMitad, ring, ringAncho, hit; };
static GizmoMedidas Medidas() {
    const bool tactil = ToolbarUsaTactil();
    const float s = (float)GlobalScale;
    GizmoMedidas m;
    m.L         = (tactil ? 55.0f : 45.0f) * s;    // largo del eje (hasta la punta): chico, para no tapar la escena
    m.r0        = 0.20f * m.L;                      // donde arranca la linea (deja libre el circulo)
    m.base      = 0.78f * m.L;                      // base de la piramide
    m.cono      = (tactil ? 0.10f : 0.07f) * m.L;   // radio de la piramide
    m.cuad      = 0.45f * m.L;                      // centro del cuadrado, sobre cada uno de sus 2 ejes
    m.cuadMitad = (tactil ? 0.13f : 0.08f) * m.L;   // medio lado del cuadrado
    m.ring      = (tactil ? 0.19f : 0.15f) * m.L;   // radio del circulo
    m.ringAncho = 2.0f * s;                         // grosor del circulo
    m.hit       = (tactil ? 14.0f : 8.0f) * s;      // margen para acertarle
    return m;
}

// centro de cada manija en MUNDO
static Vector3 ManijaCentro(int manija, const Vector3& c, const Vector3 ejes[3], float upp, const GizmoMedidas& md) {
    if (manija >= GizmoEjeX && manija <= GizmoEjeZ) return c + ejes[manija] * (md.L * upp);
    if (manija >= GizmoPlanoX && manija <= GizmoPlanoZ) {
        const int k = manija - GizmoPlanoX, a = (k + 1) % 3, b = (k + 2) % 3;
        return c + (ejes[a] + ejes[b]) * (md.cuad * upp);
    }
    return c;
}

bool GizmoManijaEnPantalla(Viewport3D* vp, int manija, float& sx, float& sy) {
    Vector3 c, ejes[3];
    if (!vp || !g_gizmoOn || !GizmoBase(c, ejes)) return false;
    const float upp = UnidadesPorPixel(vp, c); if (upp <= 0.0f) return false;
    const GizmoMedidas md = Medidas();
    if (manija >= GizmoEjeX && manija <= GizmoEjeZ) {          // el medio de la linea (donde se agarra)
        const Vector3 p = c + ejes[manija] * ((md.r0 + md.base) * 0.5f * upp);
        return vp->ProyectarPunto(p, sx, sy);
    }
    return vp->ProyectarPunto(ManijaCentro(manija, c, ejes, upp, md), sx, sy);
}

// ---------------------------------------------------------------------------
//  Hit test (coords LOCALES del viewport)
// ---------------------------------------------------------------------------
static float DistSegmento(float px, float py, float ax, float ay, float bx, float by) {
    const float vx = bx - ax, vy = by - ay; const float l2 = vx * vx + vy * vy;
    float t = (l2 > 1e-6f) ? ((px - ax) * vx + (py - ay) * vy) / l2 : 0.0f;
    if (t < 0) t = 0; if (t > 1) t = 1;
    const float qx = ax + vx * t - px, qy = ay + vy * t - py;
    return sqrtf(qx * qx + qy * qy);
}
static int GizmoHit(Viewport3D* vp, const Vector3& c, const Vector3 ejes[3], float lx, float ly) {
    const float upp = UnidadesPorPixel(vp, c); if (upp <= 0.0f) return GizmoNinguna;
    const GizmoMedidas md = Medidas();
    float cx, cy; if (!vp->ProyectarPunto(c, cx, cy)) return GizmoNinguna;
    // 0) el circulo del centro PRIMERO: las flechas nacen pegadas a el y si se probaban antes, apretar el
    //    centro agarraba un eje (inusable como "G")
    { const float dx = lx - cx, dy = ly - cy; if (sqrtf(dx * dx + dy * dy) <= md.ring + md.hit * 0.5f) return GizmoCentro; }
    // 1) cuadrados (planos): estan mas cerca del centro que las puntas, van antes que las flechas
    for (int k = 0; k < 3; k++) {
        float sx, sy; if (!vp->ProyectarPunto(ManijaCentro(GizmoPlanoX + k, c, ejes, upp, md), sx, sy)) continue;
        const float dx = fabsf(sx - lx), dy = fabsf(sy - ly);
        if (dx <= md.cuadMitad + md.hit * 0.5f && dy <= md.cuadMitad + md.hit * 0.5f) return GizmoPlanoX + k;
    }
    // 2) flechas: la linea desde mas alla del circulo hasta la punta
    for (int k = 0; k < 3; k++) {
        float ax, ay, bx, by;
        if (!vp->ProyectarPunto(c + ejes[k] * ((md.ring + md.hit) * upp), ax, ay) || !vp->ProyectarPunto(c + ejes[k] * (md.L * upp), bx, by)) continue;
        if (DistSegmento(lx, ly, ax, ay, bx, by) <= md.hit) return GizmoEjeX + k;
    }
    return GizmoNinguna;
}

// ---------------------------------------------------------------------------
//  Arranque / confirmacion
// ---------------------------------------------------------------------------
bool GizmoArrastrando() { return g_gizmoArrastre; }

bool GizmoDown(ViewportBase* vb, int mx, int my) {
    if (!g_gizmoOn || !vb || vb->ViewportKind() != 1 || PopUpActive) return false;
    if (estado != editNavegacion) return false;
    if (g_viewEditMode) return false;      // "vista" prendida: el dedo/mouse navega, el gizmo no esta
    Viewport3D* vp = (Viewport3D*)vb;
    if (!vp->showOverlays) return false;   // sin overlays no se dibuja -> tampoco se agarra
    Vector3 c, ejes[3];
    if (!GizmoBase(c, ejes)) return false;
    const int manija = GizmoHit(vp, c, ejes, (float)(mx - vp->x), (float)(my - vp->y));
    if (manija == GizmoNinguna) return false;
    int eje = ViewAxis;
    if (manija >= GizmoEjeX && manija <= GizmoEjeZ) eje = (manija == GizmoEjeX) ? X : (manija == GizmoEjeY) ? Y : Z;
    else if (manija >= GizmoPlanoX && manija <= GizmoPlanoZ) eje = (manija == GizmoPlanoX) ? PlaneX : (manija == GizmoPlanoY) ? PlaneY : PlaneZ;
    viewPortActive = vp; Viewport3DActive = vp;
    lastMouseX = mx; lastMouseY = my;           // el primer motion mide desde ACA (y ademas arranca en cero)
    if (InteractionMode == ObjectMode) {
        SetPosicion();                          // la misma traslacion de G (con su undo)
        if (estado != translacion) return false;
    } else {
        if (!EditXformStart(translacion, eje) || !EditXformActivo()) return false;
    }
    axisSelect = eje;
    // las manijas se dibujaron en Global o Local: View/Normal no tienen gizmo propio -> Global
    if (eje != ViewAxis && transformOrientation != LocalOrient) transformOrientation = GlobalOrient;
    // ancla del arrastre 1:1: pivote, ejes y el punto de la restriccion bajo el puntero AHORA
    g_gizmoC0 = c; for (int k = 0; k < 3; k++) g_gizmoEjes0[k] = ejes[k];
    g_gizmoAplicado = Vector3(0, 0, 0);
    g_gizmoTieneP0 = PuntoRestriccion(vp, manija, c, ejes, (float)(mx - vp->x), (float)(my - vp->y), g_gizmoP0);
    g_gizmoArrastre = true; g_gizmoManija = manija;
    g_redraw = true;
    return true;
}

bool GizmoMotion(Viewport3D* vp, int mx, int my) {
    if (!g_gizmoArrastre || !vp) return false;
    if (!g_gizmoTieneP0) return true;               // no se pudo anclar (eje de frente): se queda quieto
    Vector3 P;
    if (!PuntoRestriccion(vp, g_gizmoManija, g_gizmoC0, g_gizmoEjes0, (float)(mx - vp->x), (float)(my - vp->y), P)) return true;
    Vector3 D = P - g_gizmoP0;
    // sin residuo numerico fuera de la restriccion (el cuadrado azul NUNCA mueve en altura, etc)
    if (g_gizmoManija >= GizmoEjeX && g_gizmoManija <= GizmoEjeZ) { const Vector3& a = g_gizmoEjes0[g_gizmoManija - GizmoEjeX]; D = a * D.Dot(a); }
    else if (g_gizmoManija >= GizmoPlanoX && g_gizmoManija <= GizmoPlanoZ) { const Vector3& n = g_gizmoEjes0[g_gizmoManija - GizmoPlanoX]; D = D - n * D.Dot(n); }
    if (InteractionMode == ObjectMode) SetTranslacionObjetosMundo(D);
    else EditXformTraslacionMundo(D);
    g_gizmoAplicado = D;
    g_redraw = true;
    return true;
}

void GizmoSoltar(Viewport3D* vp) {
    if (!g_gizmoArrastre) return;
    g_gizmoArrastre = false; g_gizmoManija = GizmoNinguna; g_gizmoTieneP0 = false;
    if (vp) vp->Aceptar();                      // confirma (objeto o malla), como el click / Enter de siempre
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  Dibujo
// ---------------------------------------------------------------------------
static void Empujar(std::vector<GLfloat>& v, const Vector3& p) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); }

void GizmoRender(Viewport3D* vp) {
    if (!g_gizmoOn || !vp) return;
    if (g_viewEditMode) return;                                  // "vista": el puntero navega, el gizmo se esconde
    if (estado != editNavegacion && !g_gizmoArrastre) return;   // durante G/R/S del teclado no molesta
    Vector3 c, ejes[3];
    if (!GizmoBase(c, ejes)) return;
    const float upp = UnidadesPorPixel(vp, c); if (upp <= 0.0f) return;
    const GizmoMedidas md = Medidas();
    CameraBase cam = vp->VistaCam();
    const Vector3 camDer = cam.rot * Vector3(1, 0, 0), camArr = cam.rot * Vector3(0, 1, 0);

    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::Disable(gfx::CullFace);

    static std::vector<GLfloat> tri, lin; static std::vector<GLushort> idx; static std::vector<GLubyte> col;
    const bool activo = g_gizmoArrastre;
    for (int k = 0; k < 3; k++) {
        const float* cl = ColEje(k);
        const bool esta = activo && (g_gizmoManija == GizmoEjeX + k);
        const float alfa = (activo && !esta) ? 0.35f : 1.0f;
        // linea
        lin.clear(); idx.clear();
        Empujar(lin, c + ejes[k] * (md.r0 * upp)); Empujar(lin, c + ejes[k] * (md.base * upp));
        idx.push_back(0); idx.push_back(1);
        gfx::Color4f(cl[0], cl[1], cl[2], alfa);
        gfx::LineWidth(esta ? 3.0f * GlobalScale : 2.0f * GlobalScale);
        W3dDrawLinesF(&lin[0], &idx[0], 2);
        // piramide de 4 lados en la punta
        const Vector3 base = c + ejes[k] * (md.base * upp), punta = c + ejes[k] * (md.L * upp);
        const Vector3 u = ejes[(k + 1) % 3] * (md.cono * upp), w = ejes[(k + 2) % 3] * (md.cono * upp);
        const Vector3 q[4] = { base + u, base + w, base - u, base - w };
        tri.clear();
        for (int i = 0; i < 4; i++) { Empujar(tri, punta); Empujar(tri, q[i]); Empujar(tri, q[(i + 1) % 4]); }
        Empujar(tri, q[0]); Empujar(tri, q[1]); Empujar(tri, q[2]); Empujar(tri, q[0]); Empujar(tri, q[2]); Empujar(tri, q[3]);   // la base, cerrada
        gfx::VertexPointer3f(0, &tri[0]);
        gfx::DrawTrianglesArray((int)(tri.size() / 3));
    }
    // cuadrados de plano: el color del eje PERPENDICULAR (el que NO mueve)
    for (int k = 0; k < 3; k++) {
        const float* cl = ColEje(k);
        const bool esta = activo && (g_gizmoManija == GizmoPlanoX + k);
        const float alfa = (activo && !esta) ? 0.25f : 0.75f;
        const int a = (k + 1) % 3, b = (k + 2) % 3;
        const Vector3 cc = ManijaCentro(GizmoPlanoX + k, c, ejes, upp, md);
        const Vector3 ea = ejes[a] * (md.cuadMitad * upp), eb = ejes[b] * (md.cuadMitad * upp);
        tri.clear();
        Empujar(tri, cc - ea - eb); Empujar(tri, cc + ea - eb); Empujar(tri, cc + ea + eb);
        Empujar(tri, cc - ea - eb); Empujar(tri, cc + ea + eb); Empujar(tri, cc - ea + eb);
        gfx::Color4f(cl[0], cl[1], cl[2], alfa);
        gfx::VertexPointer3f(0, &tri[0]);
        gfx::DrawTrianglesArray(6);
        // borde
        lin.clear(); idx.clear();
        Empujar(lin, cc - ea - eb); Empujar(lin, cc + ea - eb); Empujar(lin, cc + ea + eb); Empujar(lin, cc - ea + eb);
        idx.push_back(0); idx.push_back(1); idx.push_back(1); idx.push_back(2); idx.push_back(2); idx.push_back(3); idx.push_back(3); idx.push_back(0);
        gfx::Color4f(cl[0], cl[1], cl[2], (activo && !esta) ? 0.4f : 1.0f);
        gfx::LineWidth(1.5f * GlobalScale);
        W3dDrawLinesF(&lin[0], &idx[0], 8);
    }
    // circulo del centro: anillo blanco de borde blando (3 anillos: transparente -> blanco -> transparente), mirando a camara
    {
        const bool esta = activo && (g_gizmoManija == GizmoCentro);
        const float alfa = (activo && !esta) ? 0.3f : 0.9f;
        const int N = 40;
        const float rIn = (md.ring - md.ringAncho * 1.5f) * upp, rA = (md.ring - md.ringAncho * 0.5f) * upp;
        const float rB  = (md.ring + md.ringAncho * 0.5f) * upp, rOut = (md.ring + md.ringAncho * 1.5f) * upp;
        const float radios[4] = { rIn, rA, rB, rOut }; const float alfas[4] = { 0.0f, alfa, alfa, 0.0f };
        tri.clear(); col.clear();
        for (int band = 0; band < 3; band++) {
            for (int i = 0; i < N; i++) {
                const float a0 = (float)i / N * 6.2831853f, a1 = (float)(i + 1) / N * 6.2831853f;
                const Vector3 d0 = camDer * cosf(a0) + camArr * sinf(a0), d1 = camDer * cosf(a1) + camArr * sinf(a1);
                const Vector3 p00 = c + d0 * radios[band], p01 = c + d1 * radios[band];
                const Vector3 p10 = c + d0 * radios[band + 1], p11 = c + d1 * radios[band + 1];
                const GLubyte al0 = (GLubyte)(alfas[band] * 255.0f), al1 = (GLubyte)(alfas[band + 1] * 255.0f);
                Empujar(tri, p00); Empujar(tri, p01); Empujar(tri, p11);
                Empujar(tri, p00); Empujar(tri, p11); Empujar(tri, p10);
                const GLubyte als[6] = { al0, al0, al1, al0, al1, al1 };
                for (int v = 0; v < 6; v++) { col.push_back(255); col.push_back(255); col.push_back(255); col.push_back(als[v]); }
            }
        }
        gfx::EnableArray(gfx::ColorArray);
        gfx::ColorPointer4ub(&col[0]);
        gfx::VertexPointer3f(0, &tri[0]);
        gfx::DrawTrianglesArray((int)(tri.size() / 3));
        gfx::DisableArray(gfx::ColorArray);
    }
    gfx::Color4f(1, 1, 1, 1);
    gfx::LineWidth(1.0f);
    gfx::Disable(gfx::Blend);
    gfx::Enable(gfx::CullFace);
    gfx::Enable(gfx::DepthTest);
    gfx::Invalidate();
}
