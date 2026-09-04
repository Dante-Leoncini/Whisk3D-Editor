// ============================================================================
//  BOX SELECT compartido. Ver BoxSelect.h para el contrato. C++03 (Symbian).
// ============================================================================
#include "edit/BoxSelect.h"
#include "w3dGraphics.h"
#include "WhiskUI/core/UI.h"      // GlobalScale
#include "WhiskUI/theme/colores.h"
#include <vector>
#include <math.h>   // sqrt (largo de la linea punteada)

namespace gfx = w3dEngine;
extern bool g_redraw;

// ---------------------------------------------------------------------------
//  Estado
// ---------------------------------------------------------------------------
static int  gFase = 0;                 // 0 = apagado, 1 = armado (cruz), 2 = arrastrando
static int  gX0 = 0, gY0 = 0;          // donde se apoyo (ancla del arrastre)
static int  gX1 = 0, gY1 = 0;          // donde esta el cursor
static int  gCurX = 0, gCurY = 0;      // cursor mientras esta ARMADO (para la cruz)

bool BoxSelectArmado()      { return gFase == 1; }
bool BoxSelectArrastrando() { return gFase == 2; }
bool BoxSelectActivo()      { return gFase != 0; }

void BoxSelectArmar() {
    gFase = 1;
    // la cruz arranca donde este el cursor; el primer motion la corrige
    gX0 = gY0 = gX1 = gY1 = 0;
    g_redraw = true;
}
void BoxSelectCancelar() {
    if (!gFase) return;
    gFase = 0;
    g_redraw = true;
}

void BoxSelectDown(int mx, int my) {
    if (gFase != 1) return;      // solo se entra al arrastre desde el paso 1 (armado)
    gFase = 2;
    gX0 = gX1 = mx;
    gY0 = gY1 = my;
    g_redraw = true;
}

void BoxSelectMover(int mx, int my) {
    if (gFase == 1) { gCurX = mx; gCurY = my; g_redraw = true; return; }  // mover la cruz
    if (gFase != 2) return;
    gX1 = mx; gY1 = my;
    g_redraw = true;
}

// EL SENTIDO lo da el ANCLA, no el rect normalizado: arrastrar hacia la izquierda es
// azul aunque despues vuelvas. Es la misma regla que espera cualquiera que venga de
// un CAD o de Blender.
bool BoxSelectTocar() { return gX1 < gX0; }

void BoxSelectRect(int& x0, int& y0, int& x1, int& y1) {
    x0 = (gX0 < gX1) ? gX0 : gX1;
    x1 = (gX0 < gX1) ? gX1 : gX0;
    y0 = (gY0 < gY1) ? gY0 : gY1;
    y1 = (gY0 < gY1) ? gY1 : gY0;
}

bool BoxSelectSoltar(int& x0, int& y0, int& x1, int& y1, bool& tocar) {
    if (gFase != 2) { gFase = 0; return false; }
    tocar = BoxSelectTocar();
    BoxSelectRect(x0, y0, x1, y1);
    gFase = 0;
    g_redraw = true;
    // un click sin arrastre no es una caja: se descarta (sino un tap deseleccionaba todo
    // sin querer). El umbral es el mismo que usa el resto de la UI para separar tap de drag.
    const int minimo = 3 * GlobalScale;
    return (x1 - x0) >= minimo || (y1 - y0) >= minimo;
}

// ---------------------------------------------------------------------------
//  Geometria
// ---------------------------------------------------------------------------
bool BoxPunto(int x0, int y0, int x1, int y1, float px, float py) {
    return px >= (float)x0 && px <= (float)x1 && py >= (float)y0 && py <= (float)y1;
}

// cruce de dos segmentos por el signo de los productos cruzados
static bool Cruzan(float ax, float ay, float bx, float by,
                   float cx, float cy, float dx, float dy) {
    const float d1 = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
    const float d2 = (bx-ax)*(dy-ay) - (by-ay)*(dx-ax);
    const float d3 = (dx-cx)*(ay-cy) - (dy-cy)*(ax-cx);
    const float d4 = (dx-cx)*(by-cy) - (dy-cy)*(bx-cx);
    return ((d1 > 0.0f) != (d2 > 0.0f)) && ((d3 > 0.0f) != (d4 > 0.0f));
}

bool BoxSegmento(int x0, int y0, int x1, int y1,
                 float ax, float ay, float bx, float by) {
    // una punta adentro ya alcanza (cubre tambien el segmento ENTERO adentro)
    if (BoxPunto(x0, y0, x1, y1, ax, ay)) return true;
    if (BoxPunto(x0, y0, x1, y1, bx, by)) return true;
    // sino, que cruce alguno de los 4 lados
    const float rx0 = (float)x0, ry0 = (float)y0, rx1 = (float)x1, ry1 = (float)y1;
    if (Cruzan(ax, ay, bx, by, rx0, ry0, rx1, ry0)) return true;
    if (Cruzan(ax, ay, bx, by, rx1, ry0, rx1, ry1)) return true;
    if (Cruzan(ax, ay, bx, by, rx1, ry1, rx0, ry1)) return true;
    if (Cruzan(ax, ay, bx, by, rx0, ry1, rx0, ry0)) return true;
    return false;
}

bool BoxPuntoEnPoligono(const float* px, const float* py, int n, float qx, float qy) {
    if (!px || !py || n < 3) return false;
    bool dentro = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((py[i] > qy) != (py[j] > qy)) &&
            (qx < (px[j] - px[i]) * (qy - py[i]) / (py[j] - py[i] + 1e-9f) + px[i]))
            dentro = !dentro;
    }
    return dentro;
}

// ---------------------------------------------------------------------------
//  Dibujo (2D local del viewport)
// ---------------------------------------------------------------------------
// GL ES 1.1 no tiene line stipple: el punteado se arma a mano, como segmentos cortos.
static void PuntearLinea(std::vector<GLfloat>& v, float ax, float ay, float bx, float by, float paso) {
    const float dx = bx - ax, dy = by - ay;
    const float largo = (float)sqrt((double)(dx*dx + dy*dy));
    if (largo < 0.001f || paso < 0.5f) return;
    const int n = (int)(largo / paso);
    const float ux = dx / largo, uy = dy / largo;
    for (int i = 0; i < n; i += 2) {            // un trazo si, uno no
        const float t0 = (float)i * paso;
        float t1 = t0 + paso;
        if (t1 > largo) t1 = largo;
        v.push_back(ax + ux * t0); v.push_back(ay + uy * t0);
        v.push_back(ax + ux * t1); v.push_back(ay + uy * t1);
    }
}

void BoxSelectDibujar(int vpX, int vpY, int vpAncho, int vpAlto) {
    if (!gFase) return;
    static std::vector<GLfloat> lin;
    lin.clear();
    const float paso = 5.0f * (float)GlobalScale;   // largo del trazo del punteado

    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::Enable(gfx::Blend);
    gfx::BlendAlpha();

    if (gFase == 1) {
        // CRUZ GUIA: de lado a lado del viewport, cruzando en el cursor. Los 4 cuadrantes
        // dejan ver de que lado va a caer cada cosa ANTES de arrastrar.
        const float cx = (float)(gCurX - vpX), cy = (float)(gCurY - vpY);
        PuntearLinea(lin, 0.0f, cy, (float)vpAncho, cy, paso);
        PuntearLinea(lin, cx, 0.0f, cx, (float)vpAlto, paso);
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
    const bool tocar = BoxSelectTocar();
    const float r = tocar ? 0.35f : 0.30f;
    const float g = tocar ? 0.55f : 0.90f;
    const float b = tocar ? 1.00f : 0.35f;

    GLfloat quad[12] = { x0,y0, x1,y0, x1,y1,  x0,y0, x1,y1, x0,y1 };
    gfx::Color4f(r, g, b, 0.15f);
    gfx::VertexPointer2f(0, quad);
    gfx::DrawTrianglesArray(6);

    PuntearLinea(lin, x0, y0, x1, y0, paso);
    PuntearLinea(lin, x1, y0, x1, y1, paso);
    PuntearLinea(lin, x1, y1, x0, y1, paso);
    PuntearLinea(lin, x0, y1, x0, y0, paso);
    if (!lin.empty()) {
        gfx::LineWidth(1.0f);
        gfx::Color4f(r, g, b, 1.0f);
        gfx::VertexPointer2f(0, &lin[0]);
        gfx::DrawLines((int)(lin.size() / 2));
    }
    gfx::Disable(gfx::Blend);
    gfx::Invalidate();
}
