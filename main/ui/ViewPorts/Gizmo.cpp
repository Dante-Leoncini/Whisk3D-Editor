// ============================================================================
//  GIZMO DE TRANSFORMACION. Ver Gizmo.h para el contrato. C++03 (compila en Symbian).
// ============================================================================
#include "ui/ViewPorts/Gizmo.h"
#include "ui/ViewPorts/ViewPort3D.h"
#include "ui/ViewPorts/LayoutInput.h"   // EditXformStart / EditXformActive
#include "ui/ViewPorts/TransformUI.h"   // ToolbarUsesTouch
#include "ui/ViewPorts/PopUp/PopUpBase.h"
#include "objects/ObjectMode.h"         // BeginTranslate, SetTransformPivotPoint
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

static bool g_gizmoDrag   = false;
static int  g_gizmoHandle = GizmoNone;
// MODO del gizmo: un click / toque sobre el (sin arrastrar) pasa al siguiente: mover -> rotar -> escalar
static int  g_gizmoMode = GizmoModeMove;
int  GizmoMode() { return g_gizmoMode; }
void GizmoSetMode(int m) { g_gizmoMode = (m < 0 || m > 2) ? 0 : m; g_redraw = true; }
static void NextGizmoMode() { g_gizmoMode = (g_gizmoMode + 1) % 3; g_redraw = true; }
// el agarre: donde fue (para distinguir el click del arrastre)
static int    g_gizmoDownX = 0, g_gizmoDownY = 0;
static int    g_gizmoPrevX = 0, g_gizmoPrevY = 0;   // el giro LIBRE (centro del de rotar): su propio delta del puntero
static bool   g_gizmoMoved = false;
// ROTAR por eje: el angulo de pantalla alrededor del centro, acumulado (cruza +-180 sin saltar) y su signo
static float g_gizmoAngPrev = 0.0f, g_gizmoAngAccum = 0.0f, g_gizmoSign = 1.0f, g_gizmoCx = 0.0f, g_gizmoCy = 0.0f;
// ...o, si el circulo NO esta de canto, el angulo EN SU PLANO (rayo del puntero contra el plano): asi el punto agarrado
// sigue al puntero exacto aunque la perspectiva lo vea como una elipse. a = el eje; u, w = base del plano (w = a x u)
static bool    g_gizmoRotPlane = false;
static Vector3 g_gizmoRotA, g_gizmoRotU, g_gizmoRotW;
// ESCALAR: la medida del agarre (proyeccion sobre el eje / distancia en el plano / distancia en pantalla)
static float g_gizmoEsc0 = 1.0f;
extern bool g_viewEditMode;   // toggle "vista" de la barra: el puntero navega -> el gizmo ni se dibuja ni se agarra
// el toggle "vista" SOLO cuenta donde su boton existe (Edit Mode y los modos de pintura). Antes el gizmo lo miraba en
// cualquier modo: prendido en Edit Mode y vuelto a Object Mode (donde el boton no esta), el gizmo desaparecia y no
// habia forma de recuperarlo.
static bool ViewNavigates() {
    return g_viewEditMode && (InteractionMode == EditMode || InteractionMode == WeightPaint ||
                              InteractionMode == VertexPaint || InteractionMode == TexturePaint);
}

// ARRASTRE: la restriccion (eje / plano / plano de la vista) queda ANCLADA al pivote y a los ejes del momento del
// agarre, y el punto agarrado sobre ella sigue al puntero: lo que se mueve es (punto de hoy - punto del agarre).
// Asi el objeto queda pegado al dedo/mouse en vez de correr a una velocidad por pixel.
static Vector3 g_gizmoC0, g_gizmoAxes0[3], g_gizmoP0, g_appliedGizmo;
static bool    g_gizmoHasP0 = false;

// colores de los ejes (los mismos que las flechas de la grilla)
static const float kColX[3] = { 0.90f, 0.25f, 0.25f };
static const float kColY[3] = { 0.45f, 0.85f, 0.30f };
static const float kColZ[3] = { 0.30f, 0.50f, 0.95f };
static const float* ColAxis(int k) { return (k == 0) ? kColX : (k == 1) ? kColY : kColZ; }

// ---------------------------------------------------------------------------
//  Base: centro (mundo) y ejes unitarios (mundo). false = no hay seleccion -> no hay gizmo
// ---------------------------------------------------------------------------
// OJO con la convencion del editor: el eje "Z" de la UI (azul, arriba) es el Y interno, y el "Y" de la UI
// (verde, horizontal) es el Z interno (asi lo muestran la grilla, el panel y los ejes X/Y/Z del transform).
// ejes[0] = X, ejes[1] = "Y" de la UI, ejes[2] = "Z" de la UI.
static const int kInternalCol[3] = { 0, 2, 1 };   // columna de la matriz de cada eje de la UI
// la orientacion VIEW: el gizmo mira a la camara (sus ejes son los de la vista: X a la derecha de la pantalla, "Z"
// para arriba y "Y" la profundidad) y se trabaja en 2D: el eje de la profundidad y los dos planos que lo contienen no
// estan, solo el plano de la pantalla; rotar es solo girar sobre la vista. La ESCALA de un objeto es siempre local (no
// existe escalar desde la vista): ahi el gizmo sigue con los ejes del objeto
static bool GizmoInView() {
    if (transformOrientation != ViewOrient) return false;
    return !(InteractionMode == ObjectMode && g_gizmoMode == GizmoModeScale);
}
// la manija existe con la orientacion de ahora (en View: ver GizmoInView)
static bool HandleVisible(int handle) {
    if (!GizmoInView()) return true;
    if (g_gizmoMode == GizmoModeRotate) return handle == GizmoRingView;
    return handle != GizmoAxisY && handle != GizmoPlaneX && handle != GizmoPlaneZ;
}
static bool GizmoBase(Vector3& c, Vector3 axes[3], Viewport3D* vp) {
    axes[0] = Vector3(1, 0, 0); axes[1] = Vector3(0, 0, 1); axes[2] = Vector3(0, 1, 0);
    Matrix4 W; bool local = false;
    if (InteractionMode == ObjectMode) {
        if (!ActiveObj || !ActiveObj->select) return false;
        if (ActiveObj->noTransform) return false;   // (la Rutina: no tiene transformacion que mover)
        SetTransformPivotPoint(); c = TransformPivotPoint;
        // la ESCALA de un objeto es SIEMPRE local (AxisOriented, ObjectMode.cpp: no existe escalar en global): el
        // gizmo de escalar va con los ejes del objeto aunque el menu diga Global, si no la flecha roja de un objeto
        // girado apuntaba para un lado y el objeto se estiraba para otro
        if (transformOrientation == LocalOrient || g_gizmoMode == GizmoModeScale) { ActiveObj->GetWorldMatrix(W); local = true; }
    } else if (InteractionMode == EditMode && g_editMesh && g_editMesh->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)g_editMesh; m->EnsureEdit(); if (!m->edit) return false;
        float cx = 0, cy = 0, cz = 0;
        if (!m->edit->SelectionCenter(cx, cy, cz)) return false;
        m->GetWorldMatrix(W); c = W * Vector3(cx, cy, cz);
        if (transformOrientation == LocalOrient) local = true;
    } else return false;
    if (local) {
        for (int k = 0; k < 3; k++) {
            const int col = kInternalCol[k];
            Vector3 e(W.m[col*4], W.m[col*4+1], W.m[col*4+2]);
            const float l = e.Length(); if (l > 1e-6f) axes[k] = e * (1.0f / l);
        }
    } else if (vp && GizmoInView()) {
        // (los mismos ejes que el transform con View: AxisOriented, ObjectMode.cpp)
        const Quaternion r = vp->RotView();
        axes[0] = r * Vector3(1, 0, 0); axes[1] = r * Vector3(0, 0, -1); axes[2] = r * Vector3(0, 1, 0);
    }
    return true;
}

// unidades de MUNDO por pixel en el centro del gizmo (para el tamanio constante en pantalla)
static float UnitsPerPixel(Viewport3D* vp, const Vector3& c) {
    CameraBase cam = vp->ViewCam();
    const Vector3 right = cam.rot * Vector3(1, 0, 0);
    float sx0, sy0, sx1, sy1;
    if (!vp->ProjectPoint(c, sx0, sy0) || !vp->ProjectPoint(c + right, sx1, sy1)) return 0.0f;
    const float d = sqrtf((sx1 - sx0) * (sx1 - sx0) + (sy1 - sy0) * (sy1 - sy0));
    return (d > 1e-4f) ? 1.0f / d : 0.0f;
}

// rayo de MUNDO que sale del pixel (lx,ly) (coords locales del viewport): la inversa exacta de ProjectPoint
static bool RayFromPixel(Viewport3D* vp, float lx, float ly, Vector3& o, Vector3& d) {
    if (vp->width <= 0 || vp->height <= 0) return false;
    // la MISMA lente que ProjectPoint (mirando por la camara: la de la camara, con marco/zoom/pan)
    const Viewport3D::Lens L = vp->CurrentLens();
    const float z = (L.zoom > 1e-4f) ? L.zoom : 1.0f;
    const float u = ((lx / (float)vp->width * 2.0f - 1.0f) - L.panX) / z * L.sx;
    const float v = ((1.0f - ly / (float)vp->height * 2.0f) - L.panY) / z * L.sy;
    if (L.ortho) {
        o = L.pos + L.right * u + L.arr * v;
        d = L.fwd;
    } else {
        o = L.pos;
        d = (L.fwd + L.right * u + L.arr * v).Normalized();
    }
    return true;
}

// punto de la restriccion de la manija (anclada en c0/ejes) que "toca" el rayo del pixel (lx,ly).
// eje: el punto del eje mas cercano al rayo. plano / centro: donde el rayo cruza el plano (el del centro es el
// plano de la vista que pasa por c0). false = no se puede seguir (eje mirando a la camara, plano de canto).
static bool PointConstraint(Viewport3D* vp, int handle, const Vector3& c0, const Vector3 axes[3], float lx, float ly, Vector3& P) {
    Vector3 o, d; if (!RayFromPixel(vp, lx, ly, o, d)) return false;
    if (handle >= GizmoAxisX && handle <= GizmoAxisZ) {
        const Vector3 a = axes[handle - GizmoAxisX];
        const Vector3 w = c0 - o;
        const float b = a.Dot(d), den = 1.0f - b * b;
        if (den < 1e-4f) return false;
        P = c0 + a * ((b * w.Dot(d) - w.Dot(a)) / den);
        return true;
    }
    Vector3 n = vp->RotView() * Vector3(0, 0, -1);   // (mirando por la camara: la suya)
    if (handle >= GizmoPlaneX && handle <= GizmoPlaneZ) n = axes[handle - GizmoPlaneX];
    const float dn = d.Dot(n); if (fabsf(dn) < 1e-5f) return false;
    P = o + d * ((c0 - o).Dot(n) / dn);
    return true;
}

// medidas (en pixeles) de cada parte; en tactil las manijas son mas grandes
struct GizmoMeasures { float L, r0, base, cone, cuad, cuadHalf, ring, ringWidth, hit, rotR, rotView; };
static GizmoMeasures Measures() {
    const bool touch = ToolbarUsesTouch();
    const float s = (float)GlobalScale;
    GizmoMeasures m;
    m.L         = (touch ? 55.0f : 45.0f) * s;    // largo del eje (hasta la punta): chico, para no tapar la escena
    m.r0        = 0.20f * m.L;                     // donde arranca la linea (deja libre el circulo)
    m.base      = 0.78f * m.L;                     // base de la piramide
    m.cone      = (touch ? 0.10f : 0.07f) * m.L;   // radio de la piramide
    m.cuad      = 0.45f * m.L;                     // centro del cuadrado, sobre cada uno de sus 2 ejes
    m.cuadHalf  = (touch ? 0.13f : 0.08f) * m.L;   // medio lado del cuadrado
    m.ring      = (touch ? 0.19f : 0.15f) * m.L;   // radio del circulo
    m.ringWidth = 2.0f * s;                        // grosor del circulo
    m.hit       = (touch ? 14.0f : 8.0f) * s;      // margen para acertarle
    m.rotR      = 0.80f * m.L;                     // ROTAR: radio de los semicirculos de cada eje
    m.rotView   = 1.00f * m.L;                     // ROTAR: el anillo blanco de afuera (girar sobre la vista)
    return m;
}

// centro de cada manija en MUNDO
static Vector3 HandleCenter(int handle, const Vector3& c, const Vector3 axes[3], float upp, const GizmoMeasures& md) {
    if (handle >= GizmoAxisX && handle <= GizmoAxisZ) return c + axes[handle] * (md.L * upp);
    if (handle >= GizmoPlaneX && handle <= GizmoPlaneZ) {
        const int k = handle - GizmoPlaneX, a = (k + 1) % 3, b = (k + 2) % 3;
        return c + (axes[a] + axes[b]) * (md.cuad * upp);
    }
    return c;
}

static float DistSegment(float px, float py, float ax, float ay, float bx, float by);   // (mas abajo, con el hit test)
// direccion (unitaria) del centro del gizmo hacia la camara
static Vector3 TowardsCamera(Viewport3D* vp, const Vector3& c) {
    CameraBase cam = vp->ViewCam();
    Vector3 d = cam.pos - c;
    if (vp->orthographic || d.Length() < 1e-6f) d = cam.rot * Vector3(0, 0, 1);
    return d.Normalized();
}
// los dos ejes del plano del circulo del eje k (u, v) y el punto del circulo mas de frente a la camara
static void BaseCircle(const Vector3 axes[3], int k, Vector3& u, Vector3& v) { u = axes[(k + 1) % 3]; v = axes[(k + 2) % 3]; }
static Vector3 FrontCircle(const Vector3 axes[3], int k, const Vector3& towardsCam) {
    Vector3 d = towardsCam - axes[k] * towardsCam.Dot(axes[k]);
    if (d.Length() < 1e-4f) { Vector3 u, v; BaseCircle(axes, k, u, v); return u; }   // eje de frente: cualquiera
    return d.Normalized();
}
// ROTAR: los semicirculos (la mitad de cada circulo que mira a la camara), despues el anillo blanco (girar sobre
// la vista) y por ultimo el interior (giro libre)
static int GizmoHitRot(Viewport3D* vp, const Vector3& c, const Vector3 axes[3], float lx, float ly) {
    const float upp = UnitsPerPixel(vp, c); if (upp <= 0.0f) return GizmoNone;
    const GizmoMeasures md = Measures();
    float cx, cy; if (!vp->ProjectPoint(c, cx, cy)) return GizmoNone;
    const Vector3 hc = TowardsCamera(vp, c);
    // View (2D): solo el anillo de girar sobre la vista
    if (GizmoInView()) {
        const float dx = lx - cx, dy = ly - cy, d = sqrtf(dx * dx + dy * dy);
        return (fabsf(d - md.rotView) <= md.hit) ? GizmoRingView : GizmoNone;
    }
    // el CENTRO primero (= R R, el giro libre): un anillo visto de CANTO se proyecta como una linea que pasa justo por
    // el centro, y si los anillos se probaban antes, arrastrar desde el medio agarraba un eje
    { const float dx = lx - cx, dy = ly - cy; if (sqrtf(dx * dx + dy * dy) < md.rotR * 0.45f) return GizmoFree; }
    const int N = 48;
    float best  = md.hit; int which = GizmoNone;
    for (int k = 0; k < 3; k++) {
        Vector3 u, v; BaseCircle(axes, k, u, v);
        float px = 0, py = 0; bool hasAnt = false;
        for (int i = 0; i <= N; i++) {
            const float a = (float)i / N * 6.2831853f;
            const Vector3 dir = u * cosf(a) + v * sinf(a);
            const bool front  = dir.Dot(hc) >= -0.02f;
            float sx, sy;
            if (!front || !vp->ProjectPoint(c + dir * (md.rotR * upp), sx, sy)) { hasAnt = false; continue; }
            if (hasAnt) { const float d = DistSegment(lx, ly, px, py, sx, sy); if (d < best) { best = d; which = GizmoAxisX + k; } }
            px = sx; py = sy; hasAnt = true;
        }
    }
    if (which != GizmoNone) return which;
    const float dx = lx - cx, dy = ly - cy, d = sqrtf(dx * dx + dy * dy);
    if (fabsf(d - md.rotView) <= md.hit) return GizmoRingView;
    if (d < md.rotR) return GizmoFree;
    return GizmoNone;
}
static int GizmoHitMode(Viewport3D* vp, const Vector3& c, const Vector3 axes[3], float lx, float ly);

bool GizmoHandleInScreen(Viewport3D* vp, int handle, float& sx, float& sy) {
    Vector3 c, axes[3];
    if (!vp || !g_gizmoOn || !GizmoBase(c, axes, vp) || !HandleVisible(handle)) return false;
    const float upp = UnitsPerPixel(vp, c); if (upp <= 0.0f) return false;
    const GizmoMeasures md = Measures();
    if (g_gizmoMode == GizmoModeRotate) {
        if (handle >= GizmoAxisX && handle <= GizmoAxisZ)   // el medio del semicirculo (lo mas de frente)
            return vp->ProjectPoint(c + FrontCircle(axes, handle - GizmoAxisX, TowardsCamera(vp, c)) * (md.rotR * upp), sx, sy);
        CameraBase cam = vp->ViewCam();
        if (handle == GizmoRingView) return vp->ProjectPoint(c + (cam.rot * Vector3(1, 0, 0)) * (md.rotView * upp), sx, sy);
        if (handle == GizmoFree) return vp->ProjectPoint(c + (cam.rot * Vector3(1, 1, 0)) * (md.rotR * 0.3f * upp), sx, sy);
        return false;
    }
    if (handle >= GizmoAxisX && handle <= GizmoAxisZ) {          // el medio de la linea (donde se agarra)
        const Vector3 p = c + axes[handle] * ((md.r0 + md.base) * 0.5f * upp);
        return vp->ProjectPoint(p, sx, sy);
    }
    return vp->ProjectPoint(HandleCenter(handle, c, axes, upp, md), sx, sy);
}

// ---------------------------------------------------------------------------
//  Hit test (coords LOCALES del viewport)
// ---------------------------------------------------------------------------
static float DistSegment(float px, float py, float ax, float ay, float bx, float by) {
    const float vx = bx - ax, vy = by - ay; const float l2 = vx * vx + vy * vy;
    float t = (l2 > 1e-6f) ? ((px - ax) * vx + (py - ay) * vy) / l2 : 0.0f;
    if (t < 0) t = 0; if (t > 1) t = 1;
    const float qx = ax + vx * t - px, qy = ay + vy * t - py;
    return sqrtf(qx * qx + qy * qy);
}
static int GizmoHit(Viewport3D* vp, const Vector3& c, const Vector3 axes[3], float lx, float ly) {
    const float upp = UnitsPerPixel(vp, c); if (upp <= 0.0f) return GizmoNone;
    const GizmoMeasures md = Measures();
    float cx, cy; if (!vp->ProjectPoint(c, cx, cy)) return GizmoNone;
    // 0) el circulo del centro PRIMERO: las flechas nacen pegadas a el y si se probaban antes, apretar el
    //    centro agarraba un eje (inusable como "G")
    { const float dx = lx - cx, dy = ly - cy; if (sqrtf(dx * dx + dy * dy) <= md.ring + md.hit * 0.5f) return GizmoCenter; }
    // 1a) justo SOBRE la linea de una flecha: es la flecha, aunque un cuadrado se proyecte encima (con el objeto girado
    //     y los ejes locales, el cuadrado de un plano puede caer sobre la flecha de otro eje y la tapaba)
    for (int k = 0; k < 3; k++) {
        if (!HandleVisible(GizmoAxisX + k)) continue;
        float ax, ay, bx, by;
        if (!vp->ProjectPoint(c + axes[k] * ((md.ring + md.hit) * upp), ax, ay) || !vp->ProjectPoint(c + axes[k] * (md.L * upp), bx, by)) continue;
        if (DistSegment(lx, ly, ax, ay, bx, by) <= 2.0f * GlobalScale) return GizmoAxisX + k;
    }
    // 1b) cuadrados (planos): estan mas cerca del centro que las puntas, van antes que las flechas
    for (int k = 0; k < 3; k++) {
        if (!HandleVisible(GizmoPlaneX + k)) continue;
        float sx, sy; if (!vp->ProjectPoint(HandleCenter(GizmoPlaneX + k, c, axes, upp, md), sx, sy)) continue;
        const float dx = fabsf(sx - lx), dy = fabsf(sy - ly);
        if (dx <= md.cuadHalf + md.hit * 0.5f && dy <= md.cuadHalf + md.hit * 0.5f) return GizmoPlaneX + k;
    }
    // 2) flechas: la linea desde mas alla del circulo hasta la punta
    for (int k = 0; k < 3; k++) {
        if (!HandleVisible(GizmoAxisX + k)) continue;
        float ax, ay, bx, by;
        if (!vp->ProjectPoint(c + axes[k] * ((md.ring + md.hit) * upp), ax, ay) || !vp->ProjectPoint(c + axes[k] * (md.L * upp), bx, by)) continue;
        if (DistSegment(lx, ly, ax, ay, bx, by) <= md.hit) return GizmoAxisX + k;
    }
    return GizmoNone;
}

static int GizmoHitMode(Viewport3D* vp, const Vector3& c, const Vector3 axes[3], float lx, float ly) {
    return (g_gizmoMode == GizmoModeRotate) ? GizmoHitRot(vp, c, axes, lx, ly) : GizmoHit(vp, c, axes, lx, ly);
}
// angulo de pantalla (antihorario, Y arriba) del punto respecto del centro proyectado
static float AngleScreen(float lx, float ly, float cx, float cy) { return atan2f(-(ly - cy), lx - cx); }
// angulo (regla de la mano derecha alrededor de g_gizmoRotA) del punto donde el rayo del pixel cruza el plano del circulo
static bool AngleInPlane(Viewport3D* vp, float lx, float ly, float& ang) {
    Vector3 o, d; if (!RayFromPixel(vp, lx, ly, o, d)) return false;
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
bool GizmoDragging() { return g_gizmoDrag; }

// toque (tactil) SIN arrastre sobre una manija: el gizmo pasa al modo siguiente (en vez de seleccionar lo de abajo)
bool GizmoTouch(ViewportBase* vb, int mx, int my) {
    if (!g_gizmoOn || !vb || vb->ViewportKind() != 1 || PopUpActive) return false;
    if (state != editNavigation || ViewNavigates()) return false;
    Viewport3D* vp = (Viewport3D*)vb;
    if (!vp->showOverlays) return false;
    Vector3 c, axes[3];
    if (!GizmoBase(c, axes, vp)) return false;
    if (GizmoHitMode(vp, c, axes, (float)(mx - vp->x), (float)(my - vp->y)) == GizmoNone) return false;
    NextGizmoMode();
    return true;
}

bool GizmoDown(ViewportBase* vb, int mx, int my) {
    if (!g_gizmoOn || !vb || vb->ViewportKind() != 1 || PopUpActive) return false;
    if (state != editNavigation) return false;
    if (ViewNavigates()) return false;      // "vista" prendida: el dedo/mouse navega, el gizmo no esta
    Viewport3D* vp = (Viewport3D*)vb;
    if (!vp->showOverlays) return false;   // sin overlays no se dibuja -> tampoco se agarra
    Vector3 c, axes[3];
    if (!GizmoBase(c, axes, vp)) return false;
    const float lx = (float)(mx - vp->x), ly = (float)(my - vp->y);
    const int handle = GizmoHitMode(vp, c, axes, lx, ly);
    if (handle == GizmoNone) return false;
    // la manija -> la restriccion de siempre (la misma que con el teclado)
    int axis = (g_gizmoMode == GizmoModeScale) ? XYZ : ViewAxis;
    if (handle >= GizmoAxisX && handle <= GizmoAxisZ) axis = (handle == GizmoAxisX) ? X : (handle == GizmoAxisY) ? Y : Z;
    else if (handle >= GizmoPlaneX && handle <= GizmoPlaneZ) axis = (handle == GizmoPlaneX) ? PlaneX : (handle == GizmoPlaneY) ? PlaneY : PlaneZ;
    else if (handle == GizmoFree) axis = OrbitalAxis;   // el interior del de rotar: giro libre
    const int est = (g_gizmoMode == GizmoModeRotate) ? rotation : (g_gizmoMode == GizmoModeScale) ? EditScale : translation;
    viewPortActive = vp; Viewport3DActive = vp;
    lastMouseX = mx; lastMouseY = my;           // el primer motion mide desde ACA (y ademas arranca en cero)
    if (InteractionMode == ObjectMode) {
        // las mismas operaciones de G / R / S (con su undo)
        if (est == translation) BeginTranslate(); else if (est == rotation) BeginRotate(); else BeginScale();
        if (state != est) return false;
    } else {
        if (!EditXformStart(est, axis) || !EditXformActive()) return false;
    }
    axisSelect = axis;
    if (est == rotation) { extern bool gTrackballCap; gTrackballCap = false; }   // la vista / el giro libre miden desde aca
    g_gizmoDownX = mx; g_gizmoDownY = my; g_gizmoMoved = false;
    // ROTAR por eje: angulo de pantalla alrededor del centro; el signo depende de si el eje mira a la camara
    if (est == rotation && handle >= GizmoAxisX && handle <= GizmoAxisZ) {
        float cx = lx, cy = ly; vp->ProjectPoint(c, cx, cy);
        g_gizmoCx = cx; g_gizmoCy = cy;
        g_gizmoAngAccum = 0.0f;
        const Vector3 hc = TowardsCamera(vp, c);
        g_gizmoSign = (axes[handle - GizmoAxisX].Dot(hc) >= 0.0f) ? 1.0f : -1.0f;
        // circulo bien visible (no de canto): el angulo se mide en su plano; de canto, en pantalla
        g_gizmoC0 = c;
        g_gizmoRotA = axes[handle - GizmoAxisX];
        g_gizmoRotU = axes[(handle - GizmoAxisX + 1) % 3];
        g_gizmoRotW = Vector3::Cross(g_gizmoRotA, g_gizmoRotU);
        float ang = 0.0f;
        g_gizmoRotPlane = fabsf(g_gizmoRotA.Dot(hc)) > 0.25f && AngleInPlane(vp, lx, ly, ang);
        g_gizmoAngPrev = g_gizmoRotPlane ? ang : AngleScreen(lx, ly, cx, cy);
    }
    // las manijas se dibujaron en Global, Local o View: Normal no tiene gizmo propio -> Global
    // (la escala de un objeto es siempre local: ahi no se toca el menu)
    if (axis != ViewAxis && transformOrientation != LocalOrient && transformOrientation != ViewOrient &&
        !(est == EditScale && InteractionMode == ObjectMode))
        transformOrientation = GlobalOrient;
    // ancla del arrastre 1:1: pivote, ejes y el punto de la restriccion bajo el puntero AHORA
    g_gizmoC0 = c; for (int k = 0; k < 3; k++) g_gizmoAxes0[k] = axes[k];
    g_appliedGizmo = Vector3(0, 0, 0);
    g_gizmoHasP0 = PointConstraint(vp, handle, c, axes, lx, ly, g_gizmoP0);
    // ESCALAR: la medida del agarre
    if (est == EditScale) {
        g_gizmoEsc0 = 1.0f;
        const GizmoMeasures md = Measures();
        if (handle >= GizmoAxisX && handle <= GizmoAxisZ && g_gizmoHasP0) g_gizmoEsc0 = (g_gizmoP0 - c).Dot(axes[handle - GizmoAxisX]);
        else if (handle >= GizmoPlaneX && handle <= GizmoPlaneZ && g_gizmoHasP0) g_gizmoEsc0 = (g_gizmoP0 - c).Length();
        else { float cx = lx, cy = ly; vp->ProjectPoint(c, cx, cy); g_gizmoCx = cx; g_gizmoCy = cy;
               const float d = sqrtf((lx - cx) * (lx - cx) + (ly - cy) * (ly - cy)); g_gizmoEsc0 = (d > md.L * 0.5f) ? d : md.L * 0.5f; }
        if (fabsf(g_gizmoEsc0) < 1e-6f) g_gizmoEsc0 = 1e-6f;
    }
    g_gizmoDrag = true; g_gizmoHandle = handle;
    g_redraw = true;
    return true;
}

bool GizmoMotion(Viewport3D* vp, int mx, int my) {
    if (!g_gizmoDrag || !vp) return false;
    // hasta pasar el umbral es un CLICK (quieto): si se suelta asi, el gizmo cambia de modo. El MISMO umbral que el
    // toque con el dedo (controls.cpp): al apretar el touchpad o un mouse sensible el puntero se corre unos pixeles
    if (!g_gizmoMoved) {
        const int ddx = mx - g_gizmoDownX, ddy = my - g_gizmoDownY;
        if ((ddx < 0 ? -ddx : ddx) + (ddy < 0 ? -ddy : ddy) <= 8 * GlobalScale) return true;
        g_gizmoMoved = true;
        g_gizmoPrevX = mx; g_gizmoPrevY = my;   // (el giro libre arranca desde aca: sin el salto del umbral)
    }
    const float lx = (float)(mx - vp->x), ly = (float)(my - vp->y);
    const bool edit = (InteractionMode != ObjectMode);
    if (state == rotation) {
        // anillo de la vista / interior: la rotacion de siempre (R: alrededor de la vista / R R: libre)
        // el CENTRO = R R (giro libre / orbital). Arrastrando el gizmo el cursor NO se envuelve, y el delta del mouse
        // (dx/dy) lo calcula ese envolver: quedaba en cero y el objeto no giraba. Aca el delta es el del gizmo
        if (g_gizmoHandle == GizmoFree) {
            const int mdx = mx - g_gizmoPrevX, mdy = my - g_gizmoPrevY;
            g_gizmoPrevX = mx; g_gizmoPrevY = my;
            if (mdx || mdy) { if (edit) EditXformRotOrbital(mdx, mdy); else RotateOrbital(mdx, mdy); g_redraw = true; }
            return true;
        }
        if (g_gizmoHandle < GizmoAxisX || g_gizmoHandle > GizmoAxisZ) return false;
        float a = 0.0f;
        if (g_gizmoRotPlane) { if (!AngleInPlane(vp, lx, ly, a)) return true; }
        else a = AngleScreen(lx, ly, g_gizmoCx, g_gizmoCy);
        float d = a - g_gizmoAngPrev;
        while (d > 3.14159265f) d -= 6.2831853f; while (d < -3.14159265f) d += 6.2831853f;
        g_gizmoAngAccum += d; g_gizmoAngPrev = a;
        // en el plano el angulo ya es el de la mano derecha alrededor del eje; en pantalla depende de hacia donde mira
        const float degrees = g_gizmoAngAccum * (g_gizmoRotPlane ? 1.0f : g_gizmoSign) * 57.2957795f;
        if (edit) EditXformNumValue(degrees); else SetNumericTransform(degrees);
        g_redraw = true;
        return true;
    }
    if (state == EditScale) {
        float f = 1.0f;
        if (g_gizmoHandle >= GizmoAxisX && g_gizmoHandle <= GizmoPlaneZ) {
            Vector3 P;
            if (!g_gizmoHasP0 || !PointConstraint(vp, g_gizmoHandle, g_gizmoC0, g_gizmoAxes0, lx, ly, P)) return true;
            if (g_gizmoHandle <= GizmoAxisZ) f = (P - g_gizmoC0).Dot(g_gizmoAxes0[g_gizmoHandle - GizmoAxisX]) / g_gizmoEsc0;
            else f = (P - g_gizmoC0).Length() / g_gizmoEsc0;
        } else {
            f = sqrtf((lx - g_gizmoCx) * (lx - g_gizmoCx) + (ly - g_gizmoCy) * (ly - g_gizmoCy)) / g_gizmoEsc0;
        }
        if (fabsf(f) < 1e-4f) f = (f < 0.0f) ? -1e-4f : 1e-4f;
        if (edit) EditXformNumValue(f); else SetNumericTransform(f);
        g_redraw = true;
        return true;
    }
    if (!g_gizmoHasP0) return true;               // no se pudo anclar (eje de frente): se queda quieto
    Vector3 P;
    if (!PointConstraint(vp, g_gizmoHandle, g_gizmoC0, g_gizmoAxes0, (float)(mx - vp->x), (float)(my - vp->y), P)) return true;
    Vector3 D = P - g_gizmoP0;
    // sin residuo numerico fuera de la restriccion (el cuadrado azul NUNCA mueve en altura, etc)
    if (g_gizmoHandle >= GizmoAxisX && g_gizmoHandle <= GizmoAxisZ) { const Vector3& a = g_gizmoAxes0[g_gizmoHandle - GizmoAxisX]; D = a * D.Dot(a); }
    else if (g_gizmoHandle >= GizmoPlaneX && g_gizmoHandle <= GizmoPlaneZ) { const Vector3& n = g_gizmoAxes0[g_gizmoHandle - GizmoPlaneX]; D = D - n * D.Dot(n); }
    if (InteractionMode == ObjectMode) SetTranslationObjectsWorld(D);
    else EditXformTranslationWorld(D);
    g_appliedGizmo = D;
    g_redraw = true;
    return true;
}

void GizmoRelease(Viewport3D* vp) {
    if (!g_gizmoDrag) return;
    // CLICK sin arrastrar: no era una operacion -> se descarta (sin paso de undo) y el gizmo pasa al modo siguiente
    // (mover -> rotar -> escalar). Como el toque con el dedo: cuenta solo la quietud, no cuanto se tuvo apretado.
    const bool click = !g_gizmoMoved;
    g_gizmoDrag = false; g_gizmoHandle = GizmoNone; g_gizmoHasP0 = false;
    if (click) {
        if (InteractionMode == EditMode && EditXformActive()) EditXformCancel(); else Cancel();
        NextGizmoMode();
    }
    else if (vp) vp->Accept();                 // confirma (objeto o malla), como el click / Enter de siempre
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  Dibujo
// ---------------------------------------------------------------------------
static void Push(std::vector<GLfloat>& v, const Vector3& p) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); }
// una linea GRUESA de verdad: dos triangulos de cara a la camara, de 'widthPx' pixeles (upp = unidades de mundo por
// pixel en el gizmo). glLineWidth no sirve: WebGL y casi todos los drivers de ES lo clavan en 1 pixel, y en un iPhone
// con la escala x4 las lineas del gizmo quedaban finitas
static void ThickLine(std::vector<GLfloat>& tri, const Vector3& a, const Vector3& b, const Vector3& towardsCam,
                        float widthPx, float upp) {
    widthPx *= (float)LineScaleUI();   // (en las pantallas x4, el doble: ver UI.h)
    Vector3 n = Vector3::Cross(b - a, towardsCam);
    const float l = n.Length(); if (l < 1e-9f) return;
    n = n * (widthPx * upp * 0.5f / l);
    Push(tri, a + n); Push(tri, b + n); Push(tri, b - n);
    Push(tri, a + n); Push(tri, b - n); Push(tri, a - n);
}
static void PaintTri3(std::vector<GLfloat>& tri) {
    if (tri.empty()) return;
    gfx::VertexPointer3f(0, &tri[0]);
    gfx::DrawTrianglesArray((int)(tri.size() / 3));
    tri.clear();
}

// ROTAR: un semicirculo de color por eje (la mitad que mira a la camara: se agarra para girar en ese eje), el anillo
// blanco de afuera (girar sobre la vista) y un disco tenue adentro (arrastrar el interior = giro libre)
static void RenderRotate(Viewport3D* vp, const Vector3& c, const Vector3 axes[3], float upp, const GizmoMeasures& md,
                        const Vector3& camRight, const Vector3& camArr) {
    static std::vector<GLfloat> tri;
    const bool active = g_gizmoDrag;
    const Vector3 hc = TowardsCamera(vp, c);
    const int N = 64;
    const bool view = GizmoInView();   // (View: solo el anillo de girar sobre la vista)
    // disco del giro libre
    if (!view) {
        const bool is = active && g_gizmoHandle == GizmoFree;
        tri.clear();
        for (int i = 0; i < N; i++) {
            const float a0 = (float)i / N * 6.2831853f, a1 = (float)(i + 1) / N * 6.2831853f;
            Push(tri, c);
            Push(tri, c + (camRight * cosf(a0) + camArr * sinf(a0)) * (md.rotR * upp));
            Push(tri, c + (camRight * cosf(a1) + camArr * sinf(a1)) * (md.rotR * upp));
        }
        gfx::Color4f(1, 1, 1, is ? 0.16f : 0.06f);
        gfx::VertexPointer3f(0, &tri[0]);
        gfx::DrawTrianglesArray((int)(tri.size() / 3));
    }
    // semicirculos de los ejes
    for (int k = 0; k < 3 && !view; k++) {
        const float* cl = ColAxis(k);
        const bool is   = active && (g_gizmoHandle == GizmoAxisX + k);
        Vector3 u, v; BaseCircle(axes, k, u, v);
        tri.clear();
        bool hasAnt = false; Vector3 pAnt;
        for (int i = 0; i <= N; i++) {
            const float a = (float)i / N * 6.2831853f;
            const Vector3 dir = u * cosf(a) + v * sinf(a);
            if (dir.Dot(hc) < -0.02f) { hasAnt = false; continue; }   // la mitad de atras no se dibuja
            const Vector3 p = c + dir * (md.rotR * upp);
            if (hasAnt) ThickLine(tri, pAnt, p, hc, (is ? 4.0f : 3.0f) * GlobalScale, upp);
            pAnt = p; hasAnt = true;
        }
        if (tri.empty()) continue;
        gfx::Color4f(cl[0], cl[1], cl[2], (active && !is) ? 0.3f : 1.0f);
        PaintTri3(tri);
    }
    // anillo blanco de la vista
    {
        const bool is = active && g_gizmoHandle == GizmoRingView;
        tri.clear();
        for (int i = 0; i < N; i++) {
            const float a0 = (float)i / N * 6.2831853f, a1 = (float)(i + 1) / N * 6.2831853f;
            ThickLine(tri, c + (camRight * cosf(a0) + camArr * sinf(a0)) * (md.rotView * upp),
                             c + (camRight * cosf(a1) + camArr * sinf(a1)) * (md.rotView * upp), hc, (is ? 3.0f : 2.0f) * GlobalScale, upp);
        }
        gfx::Color4f(1, 1, 1, (active && !is) ? 0.3f : 0.8f);
        PaintTri3(tri);
    }
}

void GizmoRender(Viewport3D* vp) {
    if (!g_gizmoOn || !vp) return;
    if (ViewNavigates()) return;                                   // "vista": el puntero navega, el gizmo se esconde
    if (state != editNavigation && !g_gizmoDrag) return;   // durante G/R/S del teclado no molesta
    Vector3 c, axes[3];
    if (!GizmoBase(c, axes, vp)) return;
    const float upp = UnitsPerPixel(vp, c); if (upp <= 0.0f) return;
    const GizmoMeasures md = Measures();
    CameraBase cam = vp->ViewCam();
    const Vector3 camRight = cam.rot * Vector3(1, 0, 0), camArr = cam.rot * Vector3(0, 1, 0);

    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::Disable(gfx::CullFace);

    if (g_gizmoMode == GizmoModeRotate) {
        RenderRotate(vp, c, axes, upp, md, camRight, camArr);
        gfx::Color4f(1, 1, 1, 1); gfx::LineWidth(1.0f);
        gfx::Disable(gfx::Blend); gfx::Enable(gfx::CullFace); gfx::Enable(gfx::DepthTest);
        gfx::Invalidate();
        return;
    }
    const bool scale = (g_gizmoMode == GizmoModeScale);
    static std::vector<GLfloat> tri; static std::vector<GLubyte> col;
    const Vector3 hcAxis = TowardsCamera(vp, c);
    const bool active = g_gizmoDrag;
    for (int k = 0; k < 3; k++) {
        if (!HandleVisible(GizmoAxisX + k)) continue;
        const float* cl = ColAxis(k);
        const bool is   = active && (g_gizmoHandle == GizmoAxisX + k);
        const float alpha = (active && !is) ? 0.35f : 1.0f;
        // linea (gruesa de verdad: ver ThickLine)
        tri.clear();
        ThickLine(tri, c + axes[k] * (md.r0 * upp), c + axes[k] * (md.base * upp), hcAxis, (is ? 3.0f : 2.0f) * GlobalScale, upp);
        gfx::Color4f(cl[0], cl[1], cl[2], alpha);
        PaintTri3(tri);
        if (scale) {
            // ESCALAR: punta CUADRADA (un cubo) en vez de la piramide
            const float h = (md.L - md.base) * 0.45f;
            const Vector3 cc = c + axes[k] * ((md.L - h) * upp);
            const Vector3 e = axes[k] * (h * upp), u = axes[(k + 1) % 3] * (h * upp), w = axes[(k + 2) % 3] * (h * upp);
            const Vector3 p[8] = { cc - e - u - w, cc - e + u - w, cc - e + u + w, cc - e - u + w,
                                   cc + e - u - w, cc + e + u - w, cc + e + u + w, cc + e - u + w };
            const int faces[6][4] = { {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {1,2,6,5}, {2,3,7,6}, {3,0,4,7} };
            for (int f = 0; f < 6; f++) {
                Push(tri, p[faces[f][0]]); Push(tri, p[faces[f][1]]); Push(tri, p[faces[f][2]]);
                Push(tri, p[faces[f][0]]); Push(tri, p[faces[f][2]]); Push(tri, p[faces[f][3]]);
            }
        } else {
            // MOVER: piramide de 4 lados en la punta
            const Vector3 base = c + axes[k] * (md.base * upp), tip = c + axes[k] * (md.L * upp);
            const Vector3 u = axes[(k + 1) % 3] * (md.cone * upp), w = axes[(k + 2) % 3] * (md.cone * upp);
            const Vector3 q[4] = { base + u, base + w, base - u, base - w };
            for (int i = 0; i < 4; i++) { Push(tri, tip); Push(tri, q[i]); Push(tri, q[(i + 1) % 4]); }
            Push(tri, q[0]); Push(tri, q[1]); Push(tri, q[2]); Push(tri, q[0]); Push(tri, q[2]); Push(tri, q[3]);   // la base, cerrada
        }
        gfx::VertexPointer3f(0, &tri[0]);
        gfx::DrawTrianglesArray((int)(tri.size() / 3));
    }
    // cuadrados de plano: el color del eje PERPENDICULAR (el que NO mueve)
    for (int k = 0; k < 3; k++) {
        if (!HandleVisible(GizmoPlaneX + k)) continue;
        const float* cl = ColAxis(k);
        const bool is   = active && (g_gizmoHandle == GizmoPlaneX + k);
        const float alpha = (active && !is) ? 0.25f : 0.75f;
        const int a = (k + 1) % 3, b = (k + 2) % 3;
        const Vector3 cc = HandleCenter(GizmoPlaneX + k, c, axes, upp, md);
        const Vector3 ea = axes[a] * (md.cuadHalf * upp), eb = axes[b] * (md.cuadHalf * upp);
        tri.clear();
        Push(tri, cc - ea - eb); Push(tri, cc + ea - eb); Push(tri, cc + ea + eb);
        Push(tri, cc - ea - eb); Push(tri, cc + ea + eb); Push(tri, cc - ea + eb);
        gfx::Color4f(cl[0], cl[1], cl[2], alpha);
        gfx::VertexPointer3f(0, &tri[0]);
        gfx::DrawTrianglesArray(6);
        // borde
        { const Vector3 q[4] = { cc - ea - eb, cc + ea - eb, cc + ea + eb, cc - ea + eb };
          for (int e = 0; e < 4; e++) ThickLine(tri, q[e], q[(e + 1) % 4], hcAxis, 1.5f * GlobalScale, upp); }
        gfx::Color4f(cl[0], cl[1], cl[2], (active && !is) ? 0.4f : 1.0f);
        PaintTri3(tri);
    }
    // circulo del centro: anillo blanco de borde blando (3 anillos: transparente -> blanco -> transparente), mirando a camara
    {
        const bool is = active && (g_gizmoHandle == GizmoCenter);
        const float alpha = (active && !is) ? 0.3f : 0.9f;
        const int N = 40;
        const float rIn = (md.ring - md.ringWidth * 1.5f) * upp, rA = (md.ring - md.ringWidth * 0.5f) * upp;
        const float rB  = (md.ring + md.ringWidth * 0.5f) * upp, rOut = (md.ring + md.ringWidth * 1.5f) * upp;
        const float radii[4] = { rIn, rA, rB, rOut }; const float alphas[4] = { 0.0f, alpha, alpha, 0.0f };
        tri.clear(); col.clear();
        for (int band = 0; band < 3; band++) {
            for (int i = 0; i < N; i++) {
                const float a0 = (float)i / N * 6.2831853f, a1 = (float)(i + 1) / N * 6.2831853f;
                const Vector3 d0 = camRight * cosf(a0) + camArr * sinf(a0), d1 = camRight * cosf(a1) + camArr * sinf(a1);
                const Vector3 p00 = c + d0 * radii[band], p01 = c + d1 * radii[band];
                const Vector3 p10 = c + d0 * radii[band + 1], p11 = c + d1 * radii[band + 1];
                const GLubyte to0 = (GLubyte)(alphas[band] * 255.0f), to1 = (GLubyte)(alphas[band + 1] * 255.0f);
                Push(tri, p00); Push(tri, p01); Push(tri, p11);
                Push(tri, p00); Push(tri, p11); Push(tri, p10);
                const GLubyte als[6] = { to0, to0, to1, to0, to1, to1 };
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

// ============================================================================
//  GIZMO 2D (curvas / dope sheet del timeline, editor UV, editor 2D). Ver Gizmo.h.
//  Todo en px LOCALES del viewport (Y abajo), que es el Ortho que ponen los tres editores.
// ============================================================================
#include "ui/ViewPorts/Timeline.h"
#include "ui/ViewPorts/UVEditor.h"
#include "ui/ViewPorts/Editor2D.h"

static bool  g2Drag = false;
static int   g2Handle = Gizmo2DNone;
static int   g2Mode = GizmoModeMove;          // el modo de los editores 2D (aparte del 3D)
static ViewportBase*   g2Vp = NULL;
static Gizmo2DClient* g2Cli = NULL;
static int   g2DownX = 0, g2DownY = 0;          // donde se agarro (PANTALLA)
static bool  g2Moved = false;                   // paso el umbral: el transform del editor ya arranco
static float g2Px = 0.0f, g2Py = 0.0f;          // el pivote al agarrar (LOCAL): rotar y escalar giran alrededor de ESE
static float g2AngPrev = 0.0f, g2AngAccum = 0.0f;
static float g2Esc0 = 1.0f;

Gizmo2DClient* Gizmo2DClientOf(ViewportBase* vp) {
    if (!vp || !vp->isLeaf()) return NULL;
    switch (vp->ViewportKind()) {
        case 4: return static_cast<Gizmo2DClient*>((UVEditor*)vp);
        case 5: return static_cast<Gizmo2DClient*>((Timeline*)vp);
        case 6: return static_cast<Gizmo2DClient*>((Editor2D*)vp);
    }
    return NULL;
}
// un G/R/S del TECLADO en curso en ese editor (ahi el gizmo no se dibuja ni se agarra, como en el 3D)
static bool Xform2DKeyboard(ViewportBase* vp) {
    if (g2Drag && g2Vp == vp) return false;
    if (vp->ViewportKind() == 5) return DopeXformActive();
    return vp->XformInProgress();
}
// el modo que se ve en ESE editor: el compartido, o el siguiente que el editor acepte (el dope sheet no rota)
static int Mode2DIn(Gizmo2DClient* cli) {
    const int can = cli->Gizmo2DModes();
    for (int k = 0; k < 3; k++) { const int m = (g2Mode + k) % 3; if (can & (1 << m)) return m; }
    return GizmoModeMove;
}
int  Gizmo2DMode() { return g2Mode; }
void Gizmo2DSetMode(int m) { g2Mode = (m < 0 || m > 2) ? 0 : m; g_redraw = true; }
static void Gizmo2DNextMode(Gizmo2DClient* cli) {
    const int can = cli->Gizmo2DModes();
    int m = Mode2DIn(cli);
    for (int k = 1; k <= 3; k++) { const int n = (m + k) % 3; if (can & (1 << n)) { g2Mode = n; break; } }
    g_redraw = true;
}
bool Gizmo2DDragging() { return g2Drag; }

// medidas en px (las del 3D, para que se sientan iguales); con el dedo, mas grandes
struct Gizmo2DMeasures { float L, r0, base, cone, ring, ringWidth, hit, rotR; };
static Gizmo2DMeasures Measures2D() {
    const bool touch = ToolbarUsesTouch();
    const float s = (float)GlobalScale;
    Gizmo2DMeasures m;
    m.L         = (touch ? 55.0f : 45.0f) * s;
    m.r0        = 0.20f * m.L;
    m.base      = 0.78f * m.L;
    m.cone      = (touch ? 0.11f : 0.08f) * m.L;
    m.ring      = (touch ? 0.19f : 0.15f) * m.L;
    m.ringWidth = 2.0f * s;
    m.hit       = (touch ? 14.0f : 8.0f) * s;
    m.rotR      = 0.80f * m.L;
    return m;
}
// el gizmo de ese viewport: cliente + pivote (LOCAL). false = no hay gizmo
static bool Base2D(ViewportBase* vp, Gizmo2DClient*& cli, float& px, float& py) {
    if (!g_gizmoOn || !vp || PopUpActive) return false;
    cli = Gizmo2DClientOf(vp);
    if (!cli || Xform2DKeyboard(vp)) return false;
    if (g2Drag && g2Vp == vp && g2Moved && Mode2DIn(cli) != GizmoModeMove) { px = g2Px; py = g2Py; return true; }
    return cli->Gizmo2DPivot(px, py);
}
static int Hit2D(Gizmo2DClient* cli, float px, float py, float lx, float ly) {
    const Gizmo2DMeasures md = Measures2D();
    const float dx = lx - px, dy = ly - py, d = sqrtf(dx * dx + dy * dy);
    if (Mode2DIn(cli) == GizmoModeRotate) return (d <= md.rotR + md.hit) ? Gizmo2DRing : Gizmo2DNone;
    if (d <= md.ring + md.hit * 0.5f) return Gizmo2DCenter;
    if (DistSegment(lx, ly, px + md.ring + md.hit, py, px + md.L, py) <= md.hit) return Gizmo2DAxisX;
    if (cli->Gizmo2DWithAxisY() && DistSegment(lx, ly, px, py - md.ring - md.hit, px, py - md.L) <= md.hit) return Gizmo2DAxisY;
    return Gizmo2DNone;
}

bool Gizmo2DHandleInScreen(ViewportBase* vp, int handle, float& lx, float& ly) {
    Gizmo2DClient* cli; float px, py;
    if (!Base2D(vp, cli, px, py)) return false;
    const Gizmo2DMeasures md = Measures2D();
    lx = px; ly = py;
    if (handle == Gizmo2DAxisX)   { lx = px + (md.r0 + md.base) * 0.5f; return true; }
    if (handle == Gizmo2DAxisY)   { if (!cli->Gizmo2DWithAxisY()) return false; ly = py - (md.r0 + md.base) * 0.5f; return true; }
    if (handle == Gizmo2DRing) { lx = px + md.rotR * 0.7071f; ly = py - md.rotR * 0.7071f; return true; }
    return handle == Gizmo2DCenter;
}

bool Gizmo2DOver(ViewportBase* vp, int mx, int my) {
    Gizmo2DClient* cli; float px, py;
    if (!Base2D(vp, cli, px, py)) return false;
    const float lx = (float)(mx - vp->x), ly = (float)(my - vp->y);
    return cli->Gizmo2DInZone(lx, ly) && Hit2D(cli, px, py, lx, ly) != Gizmo2DNone;
}
bool Gizmo2DDown(ViewportBase* vp, int mx, int my) {
    if (g2Drag) return false;
    Gizmo2DClient* cli; float px, py;
    if (!Base2D(vp, cli, px, py)) return false;
    const float lx = (float)(mx - vp->x), ly = (float)(my - vp->y);
    if (!cli->Gizmo2DInZone(lx, ly)) return false;
    const int handle = Hit2D(cli, px, py, lx, ly);
    if (handle == Gizmo2DNone) return false;
    // el transform del editor recien arranca al pasar el umbral (Gizmo2DMotion): un toque quieto no toca nada
    // (en el editor UV, arrancar el transform de caras ya separa esquinas)
    g2Drag = true; g2Handle = handle; g2Vp = vp; g2Cli = cli;
    g2DownX = mx; g2DownY = my; g2Moved = false;
    g2Px = px; g2Py = py;
    g_redraw = true;
    return true;
}

bool Gizmo2DMotion(ViewportBase* vp, int mx, int my) {
    if (!g2Drag || vp != g2Vp || !g2Cli) return false;
    const int mode = Mode2DIn(g2Cli);
    if (!g2Moved) {
        const int ddx = mx - g2DownX, ddy = my - g2DownY;
        if ((ddx < 0 ? -ddx : ddx) + (ddy < 0 ? -ddy : ddy) <= 8 * GlobalScale) return true;   // todavia es un toque
        const int axis = (g2Handle == Gizmo2DAxisX) ? 1 : (g2Handle == Gizmo2DAxisY) ? 2 : 0;
        if (!g2Cli->Gizmo2DStart(mode, axis)) { g2Drag = false; g2Handle = Gizmo2DNone; g2Vp = NULL; g2Cli = NULL; return true; }
        g2Moved = true;
        const float ax = (float)(g2DownX - vp->x) - g2Px, ay = (float)(g2DownY - vp->y) - g2Py;
        g2AngPrev = atan2f(ay, ax); g2AngAccum = 0.0f;
        // ESCALAR: la medida del agarre (sobre el eje, o la distancia al pivote), con un minimo para no dividir por casi 0
        const Gizmo2DMeasures md = Measures2D();
        const float min = md.L * 0.25f;
        float e0 = (g2Handle == Gizmo2DAxisX) ? ax : (g2Handle == Gizmo2DAxisY) ? -ay : sqrtf(ax * ax + ay * ay);
        if (fabsf(e0) < min) e0 = (e0 < 0.0f) ? -min : min;
        g2Esc0 = e0;
    }
    const float lx = (float)(mx - vp->x), ly = (float)(my - vp->y);
    float dx = 0.0f, dy = 0.0f, ang = 0.0f, f = 1.0f;
    if (mode == GizmoModeRotate) {
        const float a = atan2f(ly - g2Py, lx - g2Px);
        float d = a - g2AngPrev;
        while (d > 3.14159265f) d -= 6.2831853f; while (d < -3.14159265f) d += 6.2831853f;
        g2AngAccum += d; g2AngPrev = a;
        ang = g2AngAccum;
    } else if (mode == GizmoModeScale) {
        const float ax = lx - g2Px, ay = ly - g2Py;
        f = (g2Handle == Gizmo2DAxisX) ? ax / g2Esc0 : (g2Handle == Gizmo2DAxisY) ? -ay / g2Esc0 : sqrtf(ax * ax + ay * ay) / g2Esc0;
        if (fabsf(f) < 1e-3f) f = (f < 0.0f) ? -1e-3f : 1e-3f;
    } else {
        dx = (float)(mx - g2DownX); dy = (float)(my - g2DownY);
        if (g2Handle == Gizmo2DAxisX) dy = 0.0f;
        if (g2Handle == Gizmo2DAxisY) dx = 0.0f;
    }
    g2Cli->Gizmo2DApply(dx, dy, ang, f);
    g_redraw = true;
    return true;
}

bool Gizmo2DRelease(ViewportBase* vp) {
    if (!g2Drag || vp != g2Vp) return false;
    Gizmo2DClient* cli = g2Cli;
    const bool moved = g2Moved;
    g2Drag = false; g2Handle = Gizmo2DNone; g2Vp = NULL; g2Cli = NULL; g2Moved = false;
    if (cli) { if (moved) cli->Gizmo2DConfirm(); else Gizmo2DNextMode(cli); }   // toque quieto: el modo siguiente
    g_redraw = true;
    return true;
}

void Gizmo2DForgetVp(ViewportBase* vp) {
    if (g2Vp != vp) return;
    g2Drag = false; g2Handle = Gizmo2DNone; g2Vp = NULL; g2Cli = NULL; g2Moved = false;
}
void Gizmo2DAbort() {
    if (!g2Drag) return;
    Gizmo2DClient* cli = g2Cli;
    const bool moved = g2Moved;
    g2Drag = false; g2Handle = Gizmo2DNone; g2Vp = NULL; g2Cli = NULL; g2Moved = false;
    if (cli && moved) cli->Gizmo2DCancel();
    g_redraw = true;
}

// ---- dibujo (triangulos: el grosor de GL_LINES no es confiable en ES) ----
static void Line2D(std::vector<GLfloat>& b, float ax, float ay, float bx, float by, float w) {
    const float dx = bx - ax, dy = by - ay, L = sqrtf(dx * dx + dy * dy);
    if (L < 1e-4f) return;
    const float nx = -dy / L * w * 0.5f, ny = dx / L * w * 0.5f;
    const GLfloat q[12] = { ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny,
                            ax + nx, ay + ny, bx - nx, by - ny, ax - nx, ay - ny };
    b.insert(b.end(), q, q + 12);
}
static void Ring2D(std::vector<GLfloat>& b, float cx, float cy, float r, float w, int N) {
    for (int i = 0; i < N; i++) {
        const float a0 = (float)i / N * 6.2831853f, a1 = (float)(i + 1) / N * 6.2831853f;
        const float ri = r - w * 0.5f, ro = r + w * 0.5f;
        const GLfloat q[12] = { cx + cosf(a0) * ri, cy + sinf(a0) * ri, cx + cosf(a1) * ri, cy + sinf(a1) * ri,
                                cx + cosf(a1) * ro, cy + sinf(a1) * ro, cx + cosf(a0) * ri, cy + sinf(a0) * ri,
                                cx + cosf(a1) * ro, cy + sinf(a1) * ro, cx + cosf(a0) * ro, cy + sinf(a0) * ro };
        b.insert(b.end(), q, q + 12);
    }
}
static void Paint2D(std::vector<GLfloat>& b, float r, float g, float bl, float a) {
    if (b.empty()) return;
    gfx::Color4f(r, g, bl, a);
    gfx::VertexPointer2f(0, &b[0]);
    gfx::DrawTrianglesArray((int)(b.size() / 2));
    b.clear();
}

void Gizmo2DRender(ViewportBase* vp) {
    Gizmo2DClient* cli; float px, py;
    if (!Base2D(vp, cli, px, py)) return;
    const Gizmo2DMeasures md = Measures2D();
    const int mode = Mode2DIn(cli);
    const bool active = g2Drag && g2Vp == vp && g2Moved;
    gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting);
    gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::ColorArray); gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    static std::vector<GLfloat> b;
    b.clear();
    const float s = (float)GlobalScale;
    if (mode == GizmoModeRotate) {
        const bool is = active && g2Handle == Gizmo2DRing;
        // disco tenue (todo lo de adentro tambien gira) + el anillo azul (el eje Z, el que sale de la pantalla)
        for (int i = 0; i < 48; i++) {
            const float a0 = (float)i / 48 * 6.2831853f, a1 = (float)(i + 1) / 48 * 6.2831853f;
            const GLfloat t[6] = { px, py, px + cosf(a0) * md.rotR, py + sinf(a0) * md.rotR, px + cosf(a1) * md.rotR, py + sinf(a1) * md.rotR };
            b.insert(b.end(), t, t + 6);
        }
        Paint2D(b, 1, 1, 1, is ? 0.14f : 0.06f);
        Ring2D(b, px, py, md.rotR, (is ? 4.0f : 3.0f) * s, 64);
        Paint2D(b, kColZ[0], kColZ[1], kColZ[2], 1.0f);
        // el pivote: un punto blanco
        Ring2D(b, px, py, md.ring * 0.35f, md.ringWidth, 24);
        Paint2D(b, 1, 1, 1, 0.8f);
    } else {
        const bool scale = (mode == GizmoModeScale);
        const int nAxes = cli->Gizmo2DWithAxisY() ? 2 : 1;
        for (int k = 0; k < nAxes; k++) {
            const float* cl = ColAxis(k);
            const bool is   = active && g2Handle == (k == 0 ? Gizmo2DAxisX : Gizmo2DAxisY);
            const float alpha = (active && !is) ? 0.35f : 1.0f;
            const float ux = (k == 0) ? 1.0f : 0.0f, uy = (k == 0) ? 0.0f : -1.0f;   // X a la derecha, Y para ARRIBA
            const float vx = -uy, vy = ux;                                          // perpendicular
            Line2D(b, px + ux * md.r0, py + uy * md.r0, px + ux * md.base, py + uy * md.base, (is ? 3.0f : 2.0f) * s);
            if (scale) {   // punta CUADRADA
                const float h = (md.L - md.base) * 0.45f, cx = px + ux * (md.L - h), cy = py + uy * (md.L - h);
                const GLfloat q[12] = { cx - ux * h - vx * h, cy - uy * h - vy * h, cx + ux * h - vx * h, cy + uy * h - vy * h,
                                        cx + ux * h + vx * h, cy + uy * h + vy * h, cx - ux * h - vx * h, cy - uy * h - vy * h,
                                        cx + ux * h + vx * h, cy + uy * h + vy * h, cx - ux * h + vx * h, cy - uy * h + vy * h };
                b.insert(b.end(), q, q + 12);
            } else {         // punta de FLECHA
                const float bx = px + ux * md.base, by = py + uy * md.base;
                const GLfloat t[6] = { px + ux * md.L, py + uy * md.L, bx + vx * md.cone, by + vy * md.cone, bx - vx * md.cone, by - vy * md.cone };
                b.insert(b.end(), t, t + 6);
            }
            Paint2D(b, cl[0], cl[1], cl[2], alpha);
        }
        // el circulo del centro: libre (mover) / parejo (escalar)
        const bool is = active && g2Handle == Gizmo2DCenter;
        Ring2D(b, px, py, md.ring, md.ringWidth * (is ? 1.6f : 1.0f), 40);
        Paint2D(b, 1, 1, 1, (active && !is) ? 0.3f : 0.9f);
    }
    gfx::Color4f(1, 1, 1, 1);
    gfx::Invalidate();
}
