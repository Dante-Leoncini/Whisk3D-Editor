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
// MODO del gizmo: un click / toque sobre el (sin arrastrar) pasa al siguiente: mover -> rotar -> escalar
static int  g_gizmoModo = GizmoModoMover;
int  GizmoModo() { return g_gizmoModo; }
void GizmoSetModo(int m) { g_gizmoModo = (m < 0 || m > 2) ? 0 : m; g_redraw = true; }
static void GizmoSiguienteModo() { g_gizmoModo = (g_gizmoModo + 1) % 3; g_redraw = true; }
// el agarre: donde fue (para distinguir el click del arrastre)
static int    g_gizmoDownX = 0, g_gizmoDownY = 0;
static int    g_gizmoPrevX = 0, g_gizmoPrevY = 0;   // el giro LIBRE (centro del de rotar): su propio delta del puntero
static bool   g_gizmoSeMovio = false;
// ROTAR por eje: el angulo de pantalla alrededor del centro, acumulado (cruza +-180 sin saltar) y su signo
static float g_gizmoAngPrev = 0.0f, g_gizmoAngAcum = 0.0f, g_gizmoSigno = 1.0f, g_gizmoCx = 0.0f, g_gizmoCy = 0.0f;
// ...o, si el circulo NO esta de canto, el angulo EN SU PLANO (rayo del puntero contra el plano): asi el punto agarrado
// sigue al puntero exacto aunque la perspectiva lo vea como una elipse. a = el eje; u, w = base del plano (w = a x u)
static bool    g_gizmoRotPlano = false;
static Vector3 g_gizmoRotA, g_gizmoRotU, g_gizmoRotW;
// ESCALAR: la medida del agarre (proyeccion sobre el eje / distancia en el plano / distancia en pantalla)
static float g_gizmoEsc0 = 1.0f;
extern bool g_viewEditMode;   // toggle "vista" de la barra: el puntero navega -> el gizmo ni se dibuja ni se agarra
// el toggle "vista" SOLO cuenta donde su boton existe (Edit Mode y los modos de pintura). Antes el gizmo lo miraba en
// cualquier modo: prendido en Edit Mode y vuelto a Object Mode (donde el boton no esta), el gizmo desaparecia y no
// habia forma de recuperarlo.
static bool VistaNavega() {
    return g_viewEditMode && (InteractionMode == EditMode || InteractionMode == WeightPaint ||
                              InteractionMode == VertexPaint || InteractionMode == TexturePaint);
}

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
        if (ObjActivo->sinTransformacion) return false;   // (la Rutina: no tiene transformacion que mover)
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
    // la MISMA lente que ProyectarPunto (mirando por la camara: la de la camara, con marco/zoom/pan)
    const Viewport3D::Lente L = vp->LenteActual();
    const float z = (L.zoom > 1e-4f) ? L.zoom : 1.0f;
    const float u = ((lx / (float)vp->width * 2.0f - 1.0f) - L.panX) / z * L.sx;
    const float v = ((1.0f - ly / (float)vp->height * 2.0f) - L.panY) / z * L.sy;
    if (L.orto) {
        o = L.pos + L.der * u + L.arr * v;
        d = L.fwd;
    } else {
        o = L.pos;
        d = (L.fwd + L.der * u + L.arr * v).Normalized();
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
    Vector3 n = vp->RotVista() * Vector3(0, 0, -1);   // (mirando por la camara: la suya)
    if (manija >= GizmoPlanoX && manija <= GizmoPlanoZ) n = ejes[manija - GizmoPlanoX];
    const float dn = d.Dot(n); if (fabsf(dn) < 1e-5f) return false;
    P = o + d * ((c0 - o).Dot(n) / dn);
    return true;
}

// medidas (en pixeles) de cada parte; en tactil las manijas son mas grandes
struct GizmoMedidas { float L, r0, base, cono, cuad, cuadMitad, ring, ringAncho, hit, rotR, rotVista; };
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
    m.rotR      = 0.80f * m.L;                      // ROTAR: radio de los semicirculos de cada eje
    m.rotVista  = 1.00f * m.L;                      // ROTAR: el anillo blanco de afuera (girar sobre la vista)
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

static float DistSegmento(float px, float py, float ax, float ay, float bx, float by);   // (mas abajo, con el hit test)
// direccion (unitaria) del centro del gizmo hacia la camara
static Vector3 HaciaCamara(Viewport3D* vp, const Vector3& c) {
    CameraBase cam = vp->VistaCam();
    Vector3 d = cam.pos - c;
    if (vp->orthographic || d.Length() < 1e-6f) d = cam.rot * Vector3(0, 0, 1);
    return d.Normalized();
}
// los dos ejes del plano del circulo del eje k (u, v) y el punto del circulo mas de frente a la camara
static void BaseCirculo(const Vector3 ejes[3], int k, Vector3& u, Vector3& v) { u = ejes[(k + 1) % 3]; v = ejes[(k + 2) % 3]; }
static Vector3 FrenteCirculo(const Vector3 ejes[3], int k, const Vector3& haciaCam) {
    Vector3 d = haciaCam - ejes[k] * haciaCam.Dot(ejes[k]);
    if (d.Length() < 1e-4f) { Vector3 u, v; BaseCirculo(ejes, k, u, v); return u; }   // eje de frente: cualquiera
    return d.Normalized();
}
// ROTAR: los semicirculos (la mitad de cada circulo que mira a la camara), despues el anillo blanco (girar sobre
// la vista) y por ultimo el interior (giro libre)
static int GizmoHitRot(Viewport3D* vp, const Vector3& c, const Vector3 ejes[3], float lx, float ly) {
    const float upp = UnidadesPorPixel(vp, c); if (upp <= 0.0f) return GizmoNinguna;
    const GizmoMedidas md = Medidas();
    float cx, cy; if (!vp->ProyectarPunto(c, cx, cy)) return GizmoNinguna;
    const Vector3 hc = HaciaCamara(vp, c);
    // el CENTRO primero (= R R, el giro libre): un anillo visto de CANTO se proyecta como una linea que pasa justo por
    // el centro, y si los anillos se probaban antes, arrastrar desde el medio agarraba un eje
    { const float dx = lx - cx, dy = ly - cy; if (sqrtf(dx * dx + dy * dy) < md.rotR * 0.45f) return GizmoLibre; }
    const int N = 48;
    float mejor = md.hit; int cual = GizmoNinguna;
    for (int k = 0; k < 3; k++) {
        Vector3 u, v; BaseCirculo(ejes, k, u, v);
        float px = 0, py = 0; bool hayAnt = false;
        for (int i = 0; i <= N; i++) {
            const float a = (float)i / N * 6.2831853f;
            const Vector3 dir = u * cosf(a) + v * sinf(a);
            const bool frente = dir.Dot(hc) >= -0.02f;
            float sx, sy;
            if (!frente || !vp->ProyectarPunto(c + dir * (md.rotR * upp), sx, sy)) { hayAnt = false; continue; }
            if (hayAnt) { const float d = DistSegmento(lx, ly, px, py, sx, sy); if (d < mejor) { mejor = d; cual = GizmoEjeX + k; } }
            px = sx; py = sy; hayAnt = true;
        }
    }
    if (cual != GizmoNinguna) return cual;
    const float dx = lx - cx, dy = ly - cy, d = sqrtf(dx * dx + dy * dy);
    if (fabsf(d - md.rotVista) <= md.hit) return GizmoAnilloVista;
    if (d < md.rotR) return GizmoLibre;
    return GizmoNinguna;
}
static int GizmoHitModo(Viewport3D* vp, const Vector3& c, const Vector3 ejes[3], float lx, float ly);

bool GizmoManijaEnPantalla(Viewport3D* vp, int manija, float& sx, float& sy) {
    Vector3 c, ejes[3];
    if (!vp || !g_gizmoOn || !GizmoBase(c, ejes)) return false;
    const float upp = UnidadesPorPixel(vp, c); if (upp <= 0.0f) return false;
    const GizmoMedidas md = Medidas();
    if (g_gizmoModo == GizmoModoRotar) {
        if (manija >= GizmoEjeX && manija <= GizmoEjeZ)   // el medio del semicirculo (lo mas de frente)
            return vp->ProyectarPunto(c + FrenteCirculo(ejes, manija - GizmoEjeX, HaciaCamara(vp, c)) * (md.rotR * upp), sx, sy);
        CameraBase cam = vp->VistaCam();
        if (manija == GizmoAnilloVista) return vp->ProyectarPunto(c + (cam.rot * Vector3(1, 0, 0)) * (md.rotVista * upp), sx, sy);
        if (manija == GizmoLibre) return vp->ProyectarPunto(c + (cam.rot * Vector3(1, 1, 0)) * (md.rotR * 0.3f * upp), sx, sy);
        return false;
    }
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

static int GizmoHitModo(Viewport3D* vp, const Vector3& c, const Vector3 ejes[3], float lx, float ly) {
    return (g_gizmoModo == GizmoModoRotar) ? GizmoHitRot(vp, c, ejes, lx, ly) : GizmoHit(vp, c, ejes, lx, ly);
}
// angulo de pantalla (antihorario, Y arriba) del punto respecto del centro proyectado
static float AnguloPantalla(float lx, float ly, float cx, float cy) { return atan2f(-(ly - cy), lx - cx); }
// angulo (regla de la mano derecha alrededor de g_gizmoRotA) del punto donde el rayo del pixel cruza el plano del circulo
static bool AnguloEnPlano(Viewport3D* vp, float lx, float ly, float& ang) {
    Vector3 o, d; if (!RayoDesdePixel(vp, lx, ly, o, d)) return false;
    const float dn = d.Dot(g_gizmoRotA); if (fabsf(dn) < 1e-5f) return false;
    const Vector3 P = o + d * ((g_gizmoC0 - o).Dot(g_gizmoRotA) / dn);
    const Vector3 r = P - g_gizmoC0;
    if (r.Length() < 1e-7f) return false;
    ang = atan2f(r.Dot(g_gizmoRotW), r.Dot(g_gizmoRotU));
    return true;
}

// ---------------------------------------------------------------------------
//  Arranque / confirmacion
// ---------------------------------------------------------------------------
bool GizmoArrastrando() { return g_gizmoArrastre; }

// toque (tactil) SIN arrastre sobre una manija: el gizmo pasa al modo siguiente (en vez de seleccionar lo de abajo)
bool GizmoTocar(ViewportBase* vb, int mx, int my) {
    if (!g_gizmoOn || !vb || vb->ViewportKind() != 1 || PopUpActive) return false;
    if (estado != editNavegacion || VistaNavega()) return false;
    Viewport3D* vp = (Viewport3D*)vb;
    if (!vp->showOverlays) return false;
    Vector3 c, ejes[3];
    if (!GizmoBase(c, ejes)) return false;
    if (GizmoHitModo(vp, c, ejes, (float)(mx - vp->x), (float)(my - vp->y)) == GizmoNinguna) return false;
    GizmoSiguienteModo();
    return true;
}

bool GizmoDown(ViewportBase* vb, int mx, int my) {
    if (!g_gizmoOn || !vb || vb->ViewportKind() != 1 || PopUpActive) return false;
    if (estado != editNavegacion) return false;
    if (VistaNavega()) return false;      // "vista" prendida: el dedo/mouse navega, el gizmo no esta
    Viewport3D* vp = (Viewport3D*)vb;
    if (!vp->showOverlays) return false;   // sin overlays no se dibuja -> tampoco se agarra
    Vector3 c, ejes[3];
    if (!GizmoBase(c, ejes)) return false;
    const float lx = (float)(mx - vp->x), ly = (float)(my - vp->y);
    const int manija = GizmoHitModo(vp, c, ejes, lx, ly);
    if (manija == GizmoNinguna) return false;
    // la manija -> la restriccion de siempre (la misma que con el teclado)
    int eje = (g_gizmoModo == GizmoModoEscalar) ? XYZ : ViewAxis;
    if (manija >= GizmoEjeX && manija <= GizmoEjeZ) eje = (manija == GizmoEjeX) ? X : (manija == GizmoEjeY) ? Y : Z;
    else if (manija >= GizmoPlanoX && manija <= GizmoPlanoZ) eje = (manija == GizmoPlanoX) ? PlaneX : (manija == GizmoPlanoY) ? PlaneY : PlaneZ;
    else if (manija == GizmoLibre) eje = OrbitalAxis;   // el interior del de rotar: giro libre
    const int est = (g_gizmoModo == GizmoModoRotar) ? rotacion : (g_gizmoModo == GizmoModoEscalar) ? EditScale : translacion;
    viewPortActive = vp; Viewport3DActive = vp;
    lastMouseX = mx; lastMouseY = my;           // el primer motion mide desde ACA (y ademas arranca en cero)
    if (InteractionMode == ObjectMode) {
        // las mismas operaciones de G / R / S (con su undo)
        if (est == translacion) SetPosicion(); else if (est == rotacion) SetRotacion(); else SetEscala();
        if (estado != est) return false;
    } else {
        if (!EditXformStart(est, eje) || !EditXformActivo()) return false;
    }
    axisSelect = eje;
    if (est == rotacion) { extern bool gTrackballCap; gTrackballCap = false; }   // la vista / el giro libre miden desde aca
    g_gizmoDownX = mx; g_gizmoDownY = my; g_gizmoSeMovio = false;
    // ROTAR por eje: angulo de pantalla alrededor del centro; el signo depende de si el eje mira a la camara
    if (est == rotacion && manija >= GizmoEjeX && manija <= GizmoEjeZ) {
        float cx = lx, cy = ly; vp->ProyectarPunto(c, cx, cy);
        g_gizmoCx = cx; g_gizmoCy = cy;
        g_gizmoAngAcum = 0.0f;
        const Vector3 hc = HaciaCamara(vp, c);
        g_gizmoSigno = (ejes[manija - GizmoEjeX].Dot(hc) >= 0.0f) ? 1.0f : -1.0f;
        // circulo bien visible (no de canto): el angulo se mide en su plano; de canto, en pantalla
        g_gizmoC0 = c;
        g_gizmoRotA = ejes[manija - GizmoEjeX];
        g_gizmoRotU = ejes[(manija - GizmoEjeX + 1) % 3];
        g_gizmoRotW = Vector3::Cross(g_gizmoRotA, g_gizmoRotU);
        float ang = 0.0f;
        g_gizmoRotPlano = fabsf(g_gizmoRotA.Dot(hc)) > 0.25f && AnguloEnPlano(vp, lx, ly, ang);
        g_gizmoAngPrev = g_gizmoRotPlano ? ang : AnguloPantalla(lx, ly, cx, cy);
    }
    // las manijas se dibujaron en Global o Local: View/Normal no tienen gizmo propio -> Global
    if (eje != ViewAxis && transformOrientation != LocalOrient) transformOrientation = GlobalOrient;
    // ancla del arrastre 1:1: pivote, ejes y el punto de la restriccion bajo el puntero AHORA
    g_gizmoC0 = c; for (int k = 0; k < 3; k++) g_gizmoEjes0[k] = ejes[k];
    g_gizmoAplicado = Vector3(0, 0, 0);
    g_gizmoTieneP0 = PuntoRestriccion(vp, manija, c, ejes, lx, ly, g_gizmoP0);
    // ESCALAR: la medida del agarre
    if (est == EditScale) {
        g_gizmoEsc0 = 1.0f;
        const GizmoMedidas md = Medidas();
        if (manija >= GizmoEjeX && manija <= GizmoEjeZ && g_gizmoTieneP0) g_gizmoEsc0 = (g_gizmoP0 - c).Dot(ejes[manija - GizmoEjeX]);
        else if (manija >= GizmoPlanoX && manija <= GizmoPlanoZ && g_gizmoTieneP0) g_gizmoEsc0 = (g_gizmoP0 - c).Length();
        else { float cx = lx, cy = ly; vp->ProyectarPunto(c, cx, cy); g_gizmoCx = cx; g_gizmoCy = cy;
               const float d = sqrtf((lx - cx) * (lx - cx) + (ly - cy) * (ly - cy)); g_gizmoEsc0 = (d > md.L * 0.5f) ? d : md.L * 0.5f; }
        if (fabsf(g_gizmoEsc0) < 1e-6f) g_gizmoEsc0 = 1e-6f;
    }
    g_gizmoArrastre = true; g_gizmoManija = manija;
    g_redraw = true;
    return true;
}

bool GizmoMotion(Viewport3D* vp, int mx, int my) {
    if (!g_gizmoArrastre || !vp) return false;
    // hasta pasar el umbral es un CLICK (quieto): si se suelta asi, el gizmo cambia de modo. El MISMO umbral que el
    // toque con el dedo (controles.cpp): al apretar el touchpad o un mouse sensible el puntero se corre unos pixeles
    if (!g_gizmoSeMovio) {
        const int ddx = mx - g_gizmoDownX, ddy = my - g_gizmoDownY;
        if ((ddx < 0 ? -ddx : ddx) + (ddy < 0 ? -ddy : ddy) <= 8 * GlobalScale) return true;
        g_gizmoSeMovio = true;
        g_gizmoPrevX = mx; g_gizmoPrevY = my;   // (el giro libre arranca desde aca: sin el salto del umbral)
    }
    const float lx = (float)(mx - vp->x), ly = (float)(my - vp->y);
    const bool edit = (InteractionMode != ObjectMode);
    if (estado == rotacion) {
        // anillo de la vista / interior: la rotacion de siempre (R: alrededor de la vista / R R: libre)
        // el CENTRO = R R (giro libre / orbital). Arrastrando el gizmo el cursor NO se envuelve, y el delta del mouse
        // (dx/dy) lo calcula ese envolver: quedaba en cero y el objeto no giraba. Aca el delta es el del gizmo
        if (g_gizmoManija == GizmoLibre) {
            const int mdx = mx - g_gizmoPrevX, mdy = my - g_gizmoPrevY;
            g_gizmoPrevX = mx; g_gizmoPrevY = my;
            if (mdx || mdy) { if (edit) EditXformRotOrbital(mdx, mdy); else RotarOrbital(mdx, mdy); g_redraw = true; }
            return true;
        }
        if (g_gizmoManija < GizmoEjeX || g_gizmoManija > GizmoEjeZ) return false;
        float a = 0.0f;
        if (g_gizmoRotPlano) { if (!AnguloEnPlano(vp, lx, ly, a)) return true; }
        else a = AnguloPantalla(lx, ly, g_gizmoCx, g_gizmoCy);
        float d = a - g_gizmoAngPrev;
        while (d > 3.14159265f) d -= 6.2831853f; while (d < -3.14159265f) d += 6.2831853f;
        g_gizmoAngAcum += d; g_gizmoAngPrev = a;
        // en el plano el angulo ya es el de la mano derecha alrededor del eje; en pantalla depende de hacia donde mira
        const float grados = g_gizmoAngAcum * (g_gizmoRotPlano ? 1.0f : g_gizmoSigno) * 57.2957795f;
        if (edit) EditXformNumValor(grados); else SetTransformNumerico(grados);
        g_redraw = true;
        return true;
    }
    if (estado == EditScale) {
        float f = 1.0f;
        if (g_gizmoManija >= GizmoEjeX && g_gizmoManija <= GizmoPlanoZ) {
            Vector3 P;
            if (!g_gizmoTieneP0 || !PuntoRestriccion(vp, g_gizmoManija, g_gizmoC0, g_gizmoEjes0, lx, ly, P)) return true;
            if (g_gizmoManija <= GizmoEjeZ) f = (P - g_gizmoC0).Dot(g_gizmoEjes0[g_gizmoManija - GizmoEjeX]) / g_gizmoEsc0;
            else f = (P - g_gizmoC0).Length() / g_gizmoEsc0;
        } else {
            f = sqrtf((lx - g_gizmoCx) * (lx - g_gizmoCx) + (ly - g_gizmoCy) * (ly - g_gizmoCy)) / g_gizmoEsc0;
        }
        if (fabsf(f) < 1e-4f) f = (f < 0.0f) ? -1e-4f : 1e-4f;
        if (edit) EditXformNumValor(f); else SetTransformNumerico(f);
        g_redraw = true;
        return true;
    }
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
    // CLICK sin arrastrar: no era una operacion -> se descarta (sin paso de undo) y el gizmo pasa al modo siguiente
    // (mover -> rotar -> escalar). Como el toque con el dedo: cuenta solo la quietud, no cuanto se tuvo apretado.
    const bool click = !g_gizmoSeMovio;
    g_gizmoArrastre = false; g_gizmoManija = GizmoNinguna; g_gizmoTieneP0 = false;
    if (click) {
        if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar(); else Cancelar();
        GizmoSiguienteModo();
    }
    else if (vp) vp->Aceptar();                 // confirma (objeto o malla), como el click / Enter de siempre
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  Dibujo
// ---------------------------------------------------------------------------
static void Empujar(std::vector<GLfloat>& v, const Vector3& p) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); }

// ROTAR: un semicirculo de color por eje (la mitad que mira a la camara: se agarra para girar en ese eje), el anillo
// blanco de afuera (girar sobre la vista) y un disco tenue adentro (arrastrar el interior = giro libre)
static void RenderRotar(Viewport3D* vp, const Vector3& c, const Vector3 ejes[3], float upp, const GizmoMedidas& md,
                        const Vector3& camDer, const Vector3& camArr) {
    static std::vector<GLfloat> lin, tri; static std::vector<GLushort> idx;
    const bool activo = g_gizmoArrastre;
    const Vector3 hc = HaciaCamara(vp, c);
    const int N = 64;
    // disco del giro libre
    {
        const bool esta = activo && g_gizmoManija == GizmoLibre;
        tri.clear();
        for (int i = 0; i < N; i++) {
            const float a0 = (float)i / N * 6.2831853f, a1 = (float)(i + 1) / N * 6.2831853f;
            Empujar(tri, c);
            Empujar(tri, c + (camDer * cosf(a0) + camArr * sinf(a0)) * (md.rotR * upp));
            Empujar(tri, c + (camDer * cosf(a1) + camArr * sinf(a1)) * (md.rotR * upp));
        }
        gfx::Color4f(1, 1, 1, esta ? 0.16f : 0.06f);
        gfx::VertexPointer3f(0, &tri[0]);
        gfx::DrawTrianglesArray((int)(tri.size() / 3));
    }
    // semicirculos de los ejes
    for (int k = 0; k < 3; k++) {
        const float* cl = ColEje(k);
        const bool esta = activo && (g_gizmoManija == GizmoEjeX + k);
        Vector3 u, v; BaseCirculo(ejes, k, u, v);
        lin.clear(); idx.clear();
        int ant = -1;
        for (int i = 0; i <= N; i++) {
            const float a = (float)i / N * 6.2831853f;
            const Vector3 dir = u * cosf(a) + v * sinf(a);
            if (dir.Dot(hc) < -0.02f) { ant = -1; continue; }   // la mitad de atras no se dibuja
            Empujar(lin, c + dir * (md.rotR * upp));
            const int cur = (int)(lin.size() / 3) - 1;
            if (ant >= 0) { idx.push_back((GLushort)ant); idx.push_back((GLushort)cur); }
            ant = cur;
        }
        if (idx.empty()) continue;
        gfx::Color4f(cl[0], cl[1], cl[2], (activo && !esta) ? 0.3f : 1.0f);
        gfx::LineWidth((esta ? 4.0f : 3.0f) * GlobalScale);
        W3dDrawLinesF(&lin[0], &idx[0], (int)idx.size());
    }
    // anillo blanco de la vista
    {
        const bool esta = activo && g_gizmoManija == GizmoAnilloVista;
        lin.clear(); idx.clear();
        for (int i = 0; i < N; i++) {
            const float a = (float)i / N * 6.2831853f;
            Empujar(lin, c + (camDer * cosf(a) + camArr * sinf(a)) * (md.rotVista * upp));
            idx.push_back((GLushort)i); idx.push_back((GLushort)((i + 1) % N));
        }
        gfx::Color4f(1, 1, 1, (activo && !esta) ? 0.3f : 0.8f);
        gfx::LineWidth((esta ? 3.0f : 2.0f) * GlobalScale);
        W3dDrawLinesF(&lin[0], &idx[0], (int)idx.size());
    }
}

void GizmoRender(Viewport3D* vp) {
    if (!g_gizmoOn || !vp) return;
    if (VistaNavega()) return;                                   // "vista": el puntero navega, el gizmo se esconde
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

    if (g_gizmoModo == GizmoModoRotar) {
        RenderRotar(vp, c, ejes, upp, md, camDer, camArr);
        gfx::Color4f(1, 1, 1, 1); gfx::LineWidth(1.0f);
        gfx::Disable(gfx::Blend); gfx::Enable(gfx::CullFace); gfx::Enable(gfx::DepthTest);
        gfx::Invalidate();
        return;
    }
    const bool escalar = (g_gizmoModo == GizmoModoEscalar);
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
        tri.clear();
        if (escalar) {
            // ESCALAR: punta CUADRADA (un cubo) en vez de la piramide
            const float h = (md.L - md.base) * 0.45f;
            const Vector3 cc = c + ejes[k] * ((md.L - h) * upp);
            const Vector3 e = ejes[k] * (h * upp), u = ejes[(k + 1) % 3] * (h * upp), w = ejes[(k + 2) % 3] * (h * upp);
            const Vector3 p[8] = { cc - e - u - w, cc - e + u - w, cc - e + u + w, cc - e - u + w,
                                   cc + e - u - w, cc + e + u - w, cc + e + u + w, cc + e - u + w };
            const int caras[6][4] = { {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {1,2,6,5}, {2,3,7,6}, {3,0,4,7} };
            for (int f = 0; f < 6; f++) {
                Empujar(tri, p[caras[f][0]]); Empujar(tri, p[caras[f][1]]); Empujar(tri, p[caras[f][2]]);
                Empujar(tri, p[caras[f][0]]); Empujar(tri, p[caras[f][2]]); Empujar(tri, p[caras[f][3]]);
            }
        } else {
            // MOVER: piramide de 4 lados en la punta
            const Vector3 base = c + ejes[k] * (md.base * upp), punta = c + ejes[k] * (md.L * upp);
            const Vector3 u = ejes[(k + 1) % 3] * (md.cono * upp), w = ejes[(k + 2) % 3] * (md.cono * upp);
            const Vector3 q[4] = { base + u, base + w, base - u, base - w };
            for (int i = 0; i < 4; i++) { Empujar(tri, punta); Empujar(tri, q[i]); Empujar(tri, q[(i + 1) % 4]); }
            Empujar(tri, q[0]); Empujar(tri, q[1]); Empujar(tri, q[2]); Empujar(tri, q[0]); Empujar(tri, q[2]); Empujar(tri, q[3]);   // la base, cerrada
        }
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
