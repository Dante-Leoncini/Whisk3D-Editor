#include "ViewPorts/Console.h"
#include "w3dGraphics.h"                 // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"       // W3dPantallaAlto
#include "WhiskUI/theme/colores.h"       // ListaColores / ColorID / SetColorID
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText
#include "WhiskUI/core/UI.h"             // marginGS / RenglonHeightGS / GlobalScale / borderGS / dx / dy
#include "objects/Textures.h"            // Textures[0] = atlas de la fuente
#include "render/OpcionesRender.h"       // g_redraw (render event-driven)
#include "w3dlog.h"                      // ring buffer del log del Core
#include "script/W3dScript.h"            // W3dScriptUltimoError()
#include <string>

namespace gfx = w3dEngine;

// Colores de nivel FIJOS (el tema no tiene rol de error/aviso, y accent hoy es
// verde: no sirve para resaltar errores). Elegidos legibles sobre fondo oscuro.
static const float kRojoError[4]    = { 0.95f, 0.35f, 0.30f, 1.0f }; // [ERROR] + banner de lua
static const float kAmarilloWarn[4] = { 0.95f, 0.85f, 0.30f, 1.0f }; // [WARN]

Console::Console() {
    lastCount = -1;   // fuerza la primera medicion del contenido
    lastMaxAncho = 0;
    lastBannerH = 0;
    BarCrear();
}

Console::~Console() {}

// recalcula el rango del scrollbar con las metricas guardadas. AUTOSCROLL:
// si estabamos pegados al final (PosY == MaxPosY), el final nuevo nos sigue;
// si el usuario scrolleo hacia arriba, se respeta su posicion.
void Console::RecalcularScroll() {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    bool alFinal = (PosY <= MaxPosY); // pegado al fondo? (los clamps garantizan PosY >= MaxPosY)
    int n = (lastCount > 0) ? lastCount : 0;
    ResizeScrollbar(width, height, lastMaxAncho, -(n * lh + marginGS * 2),
                    BarTopOffset() + lastBannerH);
    if (alFinal) PosY = MaxPosY; // re-enganchar al final del log
}

void Console::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH); // el borde sigue el tamano (como Outliner/Properties/UV)
    RecalcularScroll();
}

// dibuja una linea de texto en (px, py) del viewport (coord local, origen arriba-izq)
static void DibujarLinea(int px, int py, const std::string& txt, const float* rgba) {
    gfx::Color4fv(rgba);
    gfx::PushMatrix();
    gfx::Translatef((GLfloat)px, (GLfloat)py, 0);
    RenderBitmapText(txt, textAlign::left);
    gfx::PopMatrix();
}

// color de la linea segun su tag ([ERROR]/[WARN]/resto). El tag lo antepone el ring buffer.
static const float* ColorDeLinea(const char* linea) {
    if (linea[0] == '[') {
        if (linea[1] == 'E') return kRojoError;     // ERROR: rojo
        if (linea[1] == 'W') return kAmarilloWarn;  // WARN: amarillo
    }
    return ListaColores[static_cast<int>(ColorID::grisUI)]; // INFO: gris claro
}

void Console::Render() {
    const int glY = W3dPantallaAlto - y - height;

    // fondo (igual patron que los demas viewports)
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    const float* bg = ListaColores[static_cast<int>(ColorID::background)];
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
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;

    // ---- BANNER del ultimo error de lua (bien visible, arriba) ----------------
    const char* errLua = W3dScriptUltimoError();
    int bannerH = (errLua && errLua[0]) ? (lh + marginGS) : 0;
    if (bannerH > 0) {
        // fondo del banner ROJO (error) para que salte a la vista
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
        gfx::Color4fv(kRojoError);
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
        const char* l = w3dLogRingLinea(i);
        int len = 0; while (l[len]) len++;
        if (len > maxLen) maxLen = len;
    }
    // + un colchon a la derecha para que el final de la linea no quede tapado por la barra vertical
    int maxAncho = marginGS + borderGS + maxLen * LetterWidthGS + marginGS + GlobalScale * 9;
    if (count != lastCount || maxAncho != lastMaxAncho || bannerH != lastBannerH) {
        lastCount = count; lastMaxAncho = maxAncho; lastBannerH = bannerH;
        RecalcularScroll();
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
        std::string linea = "Lua: ";
        linea += errLua;
        DibujarLinea(marginGS + borderGS, top + (marginGS / 2), linea,
                     ListaColores[static_cast<int>(ColorID::negro)]);
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
        const char* l = w3dLogRingLinea(i);
        DibujarLinea(px, py, std::string(l), ColorDeLinea(l));
    }

    gfx::Disable(gfx::ScissorTest);

    RenderBar();
    DibujarBordes(this);
    DibujarScrollbar(this); // barras v/h (mismo dibujo/agarre que el Outliner)
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
// y el drag de la barra superior tampoco (gesto lockeado en controles.cpp).
void Console::event_mouse_motion(int mx, int my) {
    if (leftMouseDown || middleMouseDown) {
        ViewPortClickDown = true; // el drag congela el foco por hover hasta soltar
        // delta fresco contra el ultimo punto guardado (GuardarMousePos en el down;
        // CheckWarpMouseInViewport lo actualiza despues de cada motion)
        ScrollByTouch(mx - lastMouseX, my - lastMouseY);
        g_redraw = true;
    }
}

#ifndef W3D_SYMBIAN
// IMPRESCINDIBLE (como en todos los viewports): soltar libera ViewPortClickDown.
// Sin esto, cualquier click sobre la consola dejaba el foco por hover CLAVADO aca
// (las teclas seguian viniendo a la consola aunque muevas el mouse a otro viewport).
void Console::mouse_button_up(int boton) {
    ViewPortClickDown = false;
    if (boton == W3dMB_IZQ) {
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
}
#endif

#ifndef W3D_SYMBIAN
void Console::event_mouse_wheel(float dy, int mx, int my) {
    if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; // sobre la barra superior -> scroll de la barra
    // rueda = scroll vertical (arriba = mirar hacia atras en el historial);
    // shift+rueda = horizontal (lineas largas). ScrollByTouch clampea y, al
    // volver al fondo, el autoscroll se re-engancha solo (PosY == MaxPosY).
    int paso = (int)(dy * 6 * GlobalScale);
    if (LShiftPressed) ScrollByTouch(paso, 0);
    else               ScrollByTouch(0, paso);
    g_redraw = true;
}
#endif

// TOUCH: arrastrar 1 dedo sobre el contenido = scroll (v/h), como el Outliner
bool Console::event_finger_scroll(int px, int py, int dx, int dy) {
    (void)px; (void)py;
    ScrollByTouch(dx, dy);
    g_redraw = true;
    return true;
}
