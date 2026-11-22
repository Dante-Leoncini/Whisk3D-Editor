// ============================================================================
//  BOX SELECT compartido. Ver BoxSelect.h para el contrato. C++03 (Symbian).
// ============================================================================
#include "edit/BoxSelect.h"
#include "w3dGraphics.h"
#include "WhiskUI/core/UI.h"      // GlobalScale
#include "WhiskUI/theme/colors.h"
#include <vector>
#include <math.h>   // sqrt (largo de la linea punteada)

namespace gfx = w3dEngine;
extern bool g_redraw;

// ---------------------------------------------------------------------------
//  Estado
// ---------------------------------------------------------------------------
static int  gPhase = 0;                // 0 = apagado, 1 = armado (cruz), 2 = arrastrando
static int  gX0 = 0, gY0 = 0;          // donde se apoyo (ancla del arrastre)
static int  gX1 = 0, gY1 = 0;          // donde esta el cursor
static int  gCurX = 0, gCurY = 0;      // cursor mientras esta ARMADO (para la cruz)

bool BoxSelectBuilt()      { return gPhase == 1; }
bool BoxSelectDragging() { return gPhase == 2; }
bool BoxSelectActive()      { return gPhase != 0; }

void BoxSelectBuild() {
    gPhase = 1;
    // la cruz arranca donde este el cursor; el primer motion la corrige
    gX0 = gY0 = gX1 = gY1 = 0;
    g_redraw = true;
}
void BoxSelectCancel() {
    if (!gPhase) return;
    gPhase = 0;
    g_redraw = true;
}

void BoxSelectDown(int mx, int my) {
    if (gPhase != 1) return;      // solo se entra al arrastre desde el paso 1 (armado)
    gPhase = 2;
    gX0 = gX1 = mx;
    gY0 = gY1 = my;
    g_redraw = true;
}

void BoxSelectMove(int mx, int my) {
    if (gPhase == 1) { gCurX = mx; gCurY = my; g_redraw = true; return; }  // mover la cruz
    if (gPhase != 2) return;
    gX1 = mx; gY1 = my;
    g_redraw = true;
}

// EL SENTIDO lo da el ANCLA, no el rect normalizado: arrastrar hacia la izquierda es
// azul aunque despues vuelvas. Es la regla que espera cualquiera que haya usado un CAD.
bool BoxSelectTouch() { return gX1 < gX0; }

void BoxSelectRect(int& x0, int& y0, int& x1, int& y1) {
    x0 = (gX0 < gX1) ? gX0 : gX1;
    x1 = (gX0 < gX1) ? gX1 : gX0;
    y0 = (gY0 < gY1) ? gY0 : gY1;
    y1 = (gY0 < gY1) ? gY1 : gY0;
}

bool BoxSelectRelease(int& x0, int& y0, int& x1, int& y1, bool& touch) {
    if (gPhase != 2) { gPhase = 0; return false; }
    touch = BoxSelectTouch();
    BoxSelectRect(x0, y0, x1, y1);
    gPhase = 0;
    g_redraw = true;
    // un click sin arrastre no es una caja: se descarta (sino un tap deseleccionaba todo
    // sin querer). El umbral es el mismo que usa el resto de la UI para separar tap de drag.
    const int min = 3 * GlobalScale;
    return (x1 - x0) >= min || (y1 - y0) >= min;
}

// ---------------------------------------------------------------------------
//  Geometria
// ---------------------------------------------------------------------------
bool BoxPoint(int x0, int y0, int x1, int y1, float px, float py) {
    return px >= (float)x0 && px <= (float)x1 && py >= (float)y0 && py <= (float)y1;
}

// cruce de dos segmentos por el signo de los productos cruzados
static bool Cross(float ax, float ay, float bx, float by,
                   float cx, float cy, float dx, float dy) {
    const float d1 = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
    const float d2 = (bx-ax)*(dy-ay) - (by-ay)*(dx-ax);
    const float d3 = (dx-cx)*(ay-cy) - (dy-cy)*(ax-cx);
    const float d4 = (dx-cx)*(by-cy) - (dy-cy)*(bx-cx);
    return ((d1 > 0.0f) != (d2 > 0.0f)) && ((d3 > 0.0f) != (d4 > 0.0f));
}

bool BoxSegment(int x0, int y0, int x1, int y1,
                 float ax, float ay, float bx, float by) {
    // una punta adentro ya alcanza (cubre tambien el segmento ENTERO adentro)
    if (BoxPoint(x0, y0, x1, y1, ax, ay)) return true;
    if (BoxPoint(x0, y0, x1, y1, bx, by)) return true;
    // sino, que cruce alguno de los 4 lados
    const float rx0 = (float)x0, ry0 = (float)y0, rx1 = (float)x1, ry1 = (float)y1;
    if (Cross(ax, ay, bx, by, rx0, ry0, rx1, ry0)) return true;
    if (Cross(ax, ay, bx, by, rx1, ry0, rx1, ry1)) return true;
    if (Cross(ax, ay, bx, by, rx1, ry1, rx0, ry1)) return true;
    if (Cross(ax, ay, bx, by, rx0, ry1, rx0, ry0)) return true;
    return false;
}

bool BoxPointInPolygon(const float* px, const float* py, int n, float qx, float qy) {
    if (!px || !py || n < 3) return false;
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((py[i] > qy) != (py[j] > qy)) &&
            (qx < (px[j] - px[i]) * (qy - py[i]) / (py[j] - py[i] + 1e-9f) + px[i]))
            inside = !inside;
    }
    return inside;
}

// ---------------------------------------------------------------------------
//  Dibujo (2D local del viewport)
// ---------------------------------------------------------------------------
// GL ES 1.1 no tiene line stipple: el punteado se arma a mano, como segmentos cortos.
static void DashLine(std::vector<GLfloat>& v, float ax, float ay, float bx, float by, float step) {
    const float dx = bx - ax, dy = by - ay;
    const float length = (float)sqrt((double)(dx*dx + dy*dy));
    if (length < 0.001f || step < 0.5f) return;
    const int n = (int)(length / step);
    const float ux = dx / length, uy = dy / length;
    for (int i = 0; i < n; i += 2) {            // un trazo si, uno no
        const float t0 = (float)i * step;
        float t1 = t0 + step;
        if (t1 > length) t1 = length;
        v.push_back(ax + ux * t0); v.push_back(ay + uy * t0);
        v.push_back(ax + ux * t1); v.push_back(ay + uy * t1);
    }
}

void BoxSelectDraw(int vpX, int vpY, int vpWidth, int vpHeight) {
    if (!gPhase) return;
    static std::vector<GLfloat> lin;
    lin.clear();
    const float step = 5.0f * (float)GlobalScale;   // largo del trazo del punteado

    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::Enable(gfx::Blend);
    gfx::BlendAlpha();

    if (gPhase == 1) {
        // CRUZ GUIA: de lado a lado del viewport, cruzando en el cursor. Los 4 cuadrantes
        // dejan ver de que lado va a caer cada cosa ANTES de arrastrar.
        const float cx = (float)(gCurX - vpX), cy = (float)(gCurY - vpY);
        DashLine(lin, 0.0f, cy, (float)vpWidth, cy, step);
        DashLine(lin, cx, 0.0f, cx, (float)vpHeight, step);
        if (!lin.empty()) {
            gfx::LineWidth(1.0f);
            gfx::Color4f(1.0f, 1.0f, 1.0f, 0.75f);
            gfx::VertexPointer2f(0, &lin[0]);
            gfx::DrawLines((int)(lin.size() / 2));
        }
        gfx::Disable(gfx::Blend);
        gfx::Invalidate();
        return;
    }

    // CAJA: relleno translucido + borde punteado. Verde = entra lo que este ENTERO adentro;
    // azul = alcanza con rozar (ver BoxSelect.h).
    int rx0, ry0, rx1, ry1;
    BoxSelectRect(rx0, ry0, rx1, ry1);
    const float x0 = (float)(rx0 - vpX), y0 = (float)(ry0 - vpY);
    const float x1 = (float)(rx1 - vpX), y1 = (float)(ry1 - vpY);
    const bool touch = BoxSelectTouch();
    const float r = touch ? 0.35f : 0.30f;
    const float g = touch ? 0.55f : 0.90f;
    const float b = touch ? 1.00f : 0.35f;

    GLfloat quad[12] = { x0,y0, x1,y0, x1,y1,  x0,y0, x1,y1, x0,y1 };
    gfx::Color4f(r, g, b, 0.15f);
    gfx::VertexPointer2f(0, quad);
    gfx::DrawTrianglesArray(6);

    DashLine(lin, x0, y0, x1, y0, step);
    DashLine(lin, x1, y0, x1, y1, step);
    DashLine(lin, x1, y1, x0, y1, step);
    DashLine(lin, x0, y1, x0, y0, step);
    if (!lin.empty()) {
        gfx::LineWidth(1.0f);
        gfx::Color4f(r, g, b, 1.0f);
        gfx::VertexPointer2f(0, &lin[0]);
        gfx::DrawLines((int)(lin.size() / 2));
    }
    gfx::Disable(gfx::Blend);
    gfx::Invalidate();
}
