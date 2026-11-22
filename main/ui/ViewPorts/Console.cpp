#include "ViewPorts/Console.h"
#include "app/W3dTouchEditor.h"   // W3D_ONLY_BY_CONTROLS (Symbian: el soltar de PC solo con el dedo o el mouse)
#include "w3dGraphics.h"                 // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"       // W3dScreenHeight
#include "WhiskUI/theme/colors.h"        // ColorList / ColorID / SetColorID
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText
#include "WhiskUI/core/UI.h"             // marginGS / RowHeightGS / GlobalScale / borderGS / dx / dy
#include "objects/Textures.h"            // Textures[0] = atlas de la fuente
#include "render/RenderOptions.h"        // g_redraw (render event-driven)
#include "w3dlog.h"                      // ring buffer del log del Core
#include "script/W3dScript.h"            // W3dScriptLastError()
#include <string>

namespace gfx = w3dEngine;

// Colores de nivel FIJOS (el tema no tiene rol de error/aviso, y accent hoy es
// verde: no sirve para resaltar errores). Elegidos legibles sobre fondo oscuro.
static const float kRedError[4]   = { 0.95f, 0.35f, 0.30f, 1.0f }; // [ERROR] + banner de lua
static const float kYellowWarn[4] = { 0.95f, 0.85f, 0.30f, 1.0f }; // [WARN]

Console::Console() {
    lastCount = -1;   // fuerza la primera medicion del contenido
    lastMaxWidth = 0;
    lastBannerH = 0;
    BarCreate();
}

Console::~Console() {}

// recalcula el rango del scrollbar con las metricas guardadas. AUTOSCROLL:
// si estabamos pegados al final (PosY == MaxPosY), el final nuevo nos sigue;
// si el usuario scrolleo hacia arriba, se respeta su posicion.
void Console::RecalculateScroll() {
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    bool end     = (PosY <= MaxPosY); // pegado al fondo? (los clamps garantizan PosY >= MaxPosY)
    int n = (lastCount > 0) ? lastCount : 0;
    ResizeScrollbar(width, height, lastMaxWidth, -(n * lh + marginGS * 2),
                    BarTopOffset() + lastBannerH);
    if (end) PosY = MaxPosY; // re-enganchar al final del log
}

void Console::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH); // el borde sigue el tamano (como Outliner/Properties/UV)
    RecalculateScroll();
}

// dibuja una linea de texto en (px, py) del viewport (coord local, origen arriba-izq)
static void DrawLine(int px, int py, const std::string& txt, const float* rgba) {
    gfx::Color4fv(rgba);
    gfx::PushMatrix();
    gfx::Translatef((GLfloat)px, (GLfloat)py, 0);
    RenderBitmapText(txt, textAlign::left);
    gfx::PopMatrix();
}

// color de la linea segun su tag ([ERROR]/[WARN]/resto). El tag lo antepone el ring buffer.
static const float* LineColor(const char* line) {
    if (line[0] == '[') {
        if (line[1] == 'E') return kRedError;    // ERROR: rojo
        if (line[1] == 'W') return kYellowWarn;  // WARN: amarillo
    }
    return ColorList[static_cast<int>(ColorID::grayUI)]; // INFO: gris claro
}

void Console::Render() {
    const int glY = W3dScreenHeight - y - height;

    // fondo (igual patron que los demas viewports)
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    const float* bg = ColorList[static_cast<int>(ColorID::background)];
    gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);

    gfx::Viewport(x, glY, width, height);
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Blend);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);

    const int top = BarTopOffset();
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;

    // ---- BANNER del ultimo error de lua (bien visible, arriba) ----------------
    const char* errLua = W3dScriptLastError();
    int bannerH = (errLua && errLua[0]) ? (lh + marginGS) : 0;
    if (bannerH > 0) {
        // fondo del banner ROJO (error) para que salte a la vista
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
        gfx::Color4fv(kRedError);
        float bx0 = (float)borderGS, by0 = (float)(top);
        float bx1 = (float)(width - borderGS), by1 = (float)(top + bannerH);
        float quad[12] = { bx0,by0, bx1,by0, bx1,by1,  bx0,by0, bx1,by1, bx0,by1 };
        gfx::VertexPointer2f(0, quad);
        gfx::DrawTrianglesArray(6);
    }

    // ---- medir el contenido y recalcular el scroll SOLO si cambio -------------
    // (mismo espiritu que lastContentRows del Outliner: sin esto el scrollbar
    // quedaria viejo cuando el log crece sin que se redimensione el viewport)
    const int count = w3dLogRingCount();
    int maxLen = 0;
    for (int i = 0; i < count; i++) {
        const char* l = w3dLogRingLine(i);
        int len = 0; while (l[len]) len++;
        if (len > maxLen) maxLen = len;
    }
    // + un colchon a la derecha para que el final de la linea no quede tapado por la barra vertical
    int maxWidth = marginGS + borderGS + maxLen * LetterWidthGS + marginGS + GlobalScale * 9;
    if (count != lastCount || maxWidth != lastMaxWidth || bannerH != lastBannerH) {
        lastCount = count; lastMaxWidth = maxWidth; lastBannerH = bannerH;
        RecalculateScroll();
    }

    // ---- TEXTO (fuente): bind del atlas + blend, como el Outliner --------------
    gfx::BindTexture(Textures[0]->iID);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::Enable(gfx::Texture2D);
    gfx::Enable(gfx::Blend);
    gfx::BlendAlpha();
#ifndef W3D_SYMBIAN
    gfx::TexFilter(false);
#endif

    // el texto del banner (sobre el fondo rojo): negro para contraste
    if (bannerH > 0) {
        gfx::Scissor(x, glY, width, height);
        std::string line = "Lua: ";
        line += errLua;
        DrawLine(marginGS + borderGS, top + (marginGS / 2), line,
                     ColorList[static_cast<int>(ColorID::black)]);
    }

    // ---- LOG (ring buffer): lineas desplazadas por PosX/PosY -------------------
    // scissor SOLO al area de contenido: al scrollear, las lineas no se pisan
    // con la barra de botones ni con el banner
    const int contentTop = top + bannerH;
    int hVis = height - contentTop; if (hVis < 0) hVis = 0; // viewport minusculo: scissor negativo es error GL
    gfx::Scissor(x, glY, width, hVis);
    int px = marginGS + borderGS + PosX;
    int py0 = contentTop + marginGS + PosY;
    for (int i = 0; i < count; i++) {
        int py = py0 + i * lh;
        if (py + lh <= contentTop) continue; // arriba del area visible
        if (py >= height) break;             // abajo del area visible
        const char* l = w3dLogRingLine(i);
        DrawLine(px, py, std::string(l), LineColor(l));
    }

    gfx::Disable(gfx::ScissorTest);

    RenderBar();
    DrawBorders(this);
    DrawScrollbar(this); // barras v/h (mismo dibujo/agarre que el Outliner)
#ifdef W3D_SYMBIAN
    gfx::EnableArray(gfx::NormalArray); // baseline que asume la escena
#endif
}

// agarre de la barra en el down (patron del Outliner; el ruteo compartido de
// LayoutInput ya calcula mouseOverScrollY/X con la zona del borde del panel)
void Console::button_left() {
    if (mouseOverScrollY) mouseOverScrollYpress = true;
    if (mouseOverScrollX) mouseOverScrollXpress = true;
}

// arrastrar con un boton (izq o medio) sobre el CONTENIDO = scroll 1:1, como el
// touch. La scrollbar agarrada NO pasa por aca (LayoutMotionUI consume ese motion),
// y el drag de la barra superior tampoco (gesto lockeado en controls.cpp).
void Console::event_mouse_motion(int mx, int my) {
    if (leftMouseDown || middleMouseDown) {
        ViewPortClickDown = true; // el drag congela el foco por hover hasta soltar
        // delta fresco contra el ultimo punto guardado (SaveMousePos en el down;
        // CheckWarpMouseInViewport lo actualiza despues de cada motion)
        ScrollByTouch(mx - lastMouseX, my - lastMouseY);
        g_redraw = true;
    }
}

// IMPRESCINDIBLE (como en todos los viewports): soltar libera ViewPortClickDown.
// Sin esto, cualquier click sobre la consola dejaba el foco por hover CLAVADO aca
// (las teclas seguian viniendo a la consola aunque muevas el mouse a otro viewport).
void Console::mouse_button_up(int button) {
    W3D_ONLY_BY_CONTROLS();
    ViewPortClickDown = false;
    if (button == W3dMB_IZQ) {
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
}

void Console::event_mouse_wheel(float dy, int mx, int my) {
    if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; // sobre la barra superior -> scroll de la barra
    // rueda = scroll vertical (arriba = mirar hacia atras en el historial);
    // shift+rueda = horizontal (lineas largas). ScrollByTouch clampea y, al
    // volver al fondo, el autoscroll se re-engancha solo (PosY == MaxPosY).
    int step = (int)(dy * 6 * GlobalScale);
    if (LShiftPressed) ScrollByTouch(step, 0);
    else               ScrollByTouch(0, step);
    g_redraw = true;
}

// TOUCH: arrastrar 1 dedo sobre el contenido = scroll (v/h), como el Outliner
bool Console::event_finger_scroll(int px, int py, int dx, int dy) {
    (void)px; (void)py;
    ScrollByTouch(dx, dy);
    g_redraw = true;
    return true;
}
