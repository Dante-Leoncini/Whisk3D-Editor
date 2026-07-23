#include "ViewPorts/Editor2D.h"
#include "objects/Textures.h"        // Textures[0] = atlas de iconos (para la barra)
#include "w3dGraphics.h"             // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"   // W3dPantallaAlto
#include "WhiskUI/theme/colores.h"   // ListaColores / ColorID
#include "render/OpcionesRender.h"   // g_redraw (render event-driven)
#include "PopUp/PopUpBase.h"         // PopUpActive (los popups modales tienen prioridad)
#include "objects/Objects.h"         // SceneCollection / ObjActivo (los UI viven en la escena)
#include "objects/Texto2D.h"         // el elemento de texto
#include "io/Fuente2D.h"             // fuentes del editor 2D (atlas)
#include "WhiskUI/text/W3dTextAtlas.h"
#include "W3dLang.h"                 // T()
#include <vector>

namespace gfx = w3dEngine;

// El marco de referencia del lienzo: una pantalla de CELULAR EN VERTICAL (el caso de los
// slots). Cuando el objeto UI tenga tamano propio, esto pasa a leerse del objeto.
static const float LIENZO_W = 1080.0f;
static const float LIENZO_H = 1920.0f;

Editor2D::Editor2D() {
    zoom = 1.0f;
    panX = 0.0f; panY = 0.0f;
    lastMx = 0; lastMy = 0;
    BarCrear();   // el boton [0] de la barra (menu de tipo/split), como todos los viewports
    // [1] Add: el menu para crear elementos 2D (texto, y lo que venga)
    BarButtons.push_back(new Button(T("Add")));
    BarButtons[1]->desplegable = true;
}

Editor2D::~Editor2D() {}

void Editor2D::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH); // el borde sigue el tamano (como Outliner/Properties)
}

// centro del marco en pantalla (cx,cy) y escala s (pixeles de viewport por pixel de lienzo).
// A zoom 1 el marco ocupa ~80% del alto util: se ve entero apenas se abre el editor.
void Editor2D::ParamsLienzo(float& cx, float& cy, float& s) const {
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    cx = width * 0.5f + panX;
    cy = top + ch * 0.5f + panY;
    s  = (ch * 0.8f / LIENZO_H) * zoom;
}

void Editor2D::Render() {
    const int glY = W3dPantallaAlto - y - height;

    // fondo (mismo setup 2D que el UV editor)
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
    gfx::Disable(gfx::Blend);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::EnableArray(gfx::VertexArray);

    float cx, cy, s;
    ParamsLienzo(cx, cy, s);
    // esquina superior izquierda del marco en pantalla (el lienzo va de 0,0 a LIENZO_W,LIENZO_H)
    const float fx = cx - LIENZO_W * 0.5f * s;
    const float fy = cy - LIENZO_H * 0.5f * s;

    // ---- GRILLA: lineas cada 100 px de lienzo, solo dentro del viewport visible ----
    // rango del lienzo que se ve (invirtiendo el mapeo pantalla->lienzo)
    const float paso = 100.0f;
    float l0 = (0.0f  - fx) / s, l1 = ((float)width  - fx) / s;   // rango X visible, en lienzo
    float t0 = (0.0f  - fy) / s, t1 = ((float)height - fy) / s;   // rango Y visible, en lienzo
    std::vector<float> v;
    int i0 = (int)(l0 / paso) - 1, i1 = (int)(l1 / paso) + 1;
    for (int i = i0; i <= i1; i++) {
        float sx = fx + i * paso * s;
        v.push_back(sx); v.push_back(0.0f);
        v.push_back(sx); v.push_back((float)height);
    }
    int j0 = (int)(t0 / paso) - 1, j1 = (int)(t1 / paso) + 1;
    for (int j = j0; j <= j1; j++) {
        float sy = fy + j * paso * s;
        v.push_back(0.0f);         v.push_back(sy);
        v.push_back((float)width); v.push_back(sy);
    }
    if (!v.empty()) {
        gfx::LineWidth(1.0f);
        gfx::Color4f(0.28f, 0.28f, 0.30f, 1.0f);     // grilla: apenas mas clara que el fondo
        gfx::VertexPointer2f(0, &v[0]);
        gfx::DrawLines((int)v.size() / 2);
    }

    // ---- MARCO de la "pantalla" (el area donde vive la interfaz) ----
    const float fx1 = fx + LIENZO_W * s;
    const float fy1 = fy + LIENZO_H * s;
    const float marco[] = {
        fx,  fy,   fx1, fy,      // arriba
        fx1, fy,   fx1, fy1,     // derecha
        fx1, fy1,  fx,  fy1,     // abajo
        fx,  fy1,  fx,  fy       // izquierda
    };
    gfx::LineWidth(2.0f);
    gfx::Color4f(0.85f, 0.85f, 0.88f, 1.0f);
    gfx::VertexPointer2f(0, marco);
    gfx::DrawLines(8);
    // linea de "mitad de pantalla" (referencia para centrar HUDs), bien tenue
    const float mitad[] = { fx, (fy + fy1) * 0.5f, fx1, (fy + fy1) * 0.5f,
                            (fx + fx1) * 0.5f, fy, (fx + fx1) * 0.5f, fy1 };
    gfx::LineWidth(1.0f);
    gfx::Color4f(0.38f, 0.38f, 0.42f, 1.0f);
    gfx::VertexPointer2f(0, mitad);
    gfx::DrawLines(4);

    // ---- TEXTOS de las interfaces (objetos UI y sus hijos), sobre el lienzo ----
    DibujarTextosUI(fx, fy, s);

    gfx::Disable(gfx::ScissorTest);
    // RenderBar/DibujarBordes dibujan quads texturizados (iconos del atlas): dejarles el estado
    // prendido y RE-BINDEAR el atlas (mismo cuidado que el UV editor, bug reportado por Dante).
    gfx::Enable(gfx::Texture2D);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::EnableArray(gfx::VertexArray);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    RenderBar();
    DibujarBordes(this); // borde del viewport (verde si es el activo)
}

// recorre un arbol de la UI dibujando cada Texto2D. La posicion de un hijo se ACUMULA sobre la
// del padre (heredan la transformacion: asi se arman jerarquias tipo la rotacion de portadas).
static void DibujarTextosRec(Object* o, float baseX, float baseY,
                             float fx, float fy, float esc, bool* alguno) {
    if (!o) return;
    float ox = baseX, oy = baseY;
    if (o->getType() == ObjectType::texto2d) {
        Texto2D* t = (Texto2D*)o;
        ox += t->pos.x; oy += t->pos.y;
        w3dui::W3dTextAtlas* at = Fuente2DObtener(t->fuente);
        if (at) {
            float px = t->tam * esc;
            float w  = at->TextWidth(t->texto.c_str(), px);
            float sx = fx + ox * esc, sy = fy + oy * esc;
            if (t->alignH == 1) sx -= w * 0.5f; else if (t->alignH == 2) sx -= w;
            if (t->alignV == 1) sy -= px * 0.5f; else if (t->alignV == 2) sy -= px;
            at->Begin();
            at->DrawText(t->texto.c_str(), sx, sy, px,
                         t->color[0], t->color[1], t->color[2], t->color[3]);
            at->End();
            if (t->select) {   // marco de seleccion alrededor del texto
                gfx::Disable(gfx::Texture2D);
                gfx::DisableArray(gfx::TexCoordArray);
                gfx::EnableArray(gfx::VertexArray);
                float m = 4.0f;
                float x0 = sx - m, y0 = sy - m, x1 = sx + w + m, y1 = sy + px + m;
                float V[16] = { x0,y0, x1,y0,  x1,y0, x1,y1,  x1,y1, x0,y1,  x0,y1, x0,y0 };
                gfx::LineWidth(1.0f);
                gfx::Color4f(0.30f, 0.95f, 0.45f, 1.0f);   // verde de seleccion
                gfx::VertexPointer2f(0, V);
                gfx::DrawLines(8);
            }
            *alguno = true;
        }
    } else if (o->getType() != ObjectType::ui) {
        return;   // dentro de un UI solo se dibujan elementos 2D
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        DibujarTextosRec(o->Childrens[i], ox, oy, fx, fy, esc, alguno);
}

void Editor2D::DibujarTextosUI(float fx, float fy, float s) {
    if (!SceneCollection) return;
    bool alguno = false;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::ui)
            DibujarTextosRec(o, 0.0f, 0.0f, fx, fy, s, &alguno);
    }
}

// ---- PANEO / ZOOM (mismo manejo que el UV editor) -------------------------------------------

void Editor2D::Panear(float dx, float dy) {
    panX += dx; panY += dy;
    g_redraw = true;
}

// zoom CENTRADO en el viewport (teclado/pinch): el centro de pantalla no se mueve.
void Editor2D::ZoomCentro(int dir) {
    float f = (dir > 0) ? 1.05f : (1.0f / 1.05f);
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    f = nz / zoom;
    zoom = nz; panX *= f; panY *= f;
    g_redraw = true;
}

void Editor2D::event_mouse_motion(int mx, int my) {
    if (middleMouseDown) {                 // boton del medio: paneo
        panX += (float)(mx - lastMx);
        panY += (float)(my - lastMy);
        g_redraw = true;
    }
    lastMx = mx; lastMy = my;
}

// TOUCH: 1 dedo sobre el contenido = panear la vista.
bool Editor2D::event_finger_scroll(int px, int py, int dx, int dy) {
    (void)px; (void)py;
    Panear((float)dx, (float)dy);
    return true;
}
// TOUCH: 2 dedos = zoom (pinch) + paneo del centroide.
void Editor2D::event_finger_gesture(float zoomDelta, float panDx, float panDy) {
    if (zoomDelta > 1.0f)       ZoomCentro(1);
    else if (zoomDelta < -1.0f) ZoomCentro(-1);
    if (panDx != 0.0f || panDy != 0.0f) Panear(panDx, panDy);
}

#ifndef W3D_SYMBIAN
// flechas = paneo (la flecha revela ese lado, igual que el UV editor)
void Editor2D::event_key_down(int tecla, bool repeticion) {
    (void)repeticion;
    if (PopUpActive) return;               // un popup modal abierto tiene prioridad
    const float pp = (float)GlobalScale * 16.0f;
    if (tecla == W3dK_LEFT)  { Panear(+pp, 0); return; }
    if (tecla == W3dK_RIGHT) { Panear(-pp, 0); return; }
    if (tecla == W3dK_UP)    { Panear(0, +pp); return; }
    if (tecla == W3dK_DOWN)  { Panear(0, -pp); return; }
}

void Editor2D::event_mouse_wheel(float dy, int mx, int my) {
    (void)mx; (void)my;
    if (PopUpActive) return;
    float f = (dy > 0) ? 1.1f : (1.0f / 1.1f);
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    f = nz / zoom;                         // factor real tras el clamp
    // zoom HACIA EL CURSOR: el punto del lienzo bajo el mouse queda fijo
    float cx, cy, s;
    ParamsLienzo(cx, cy, s);
    float curX = (float)(lastMx - x);      // cursor en coords LOCALES del viewport
    float curY = (float)(lastMy - y);
    panX += (curX - cx) * (1.0f - f);
    panY += (curY - cy) * (1.0f - f);
    zoom = nz;
    g_redraw = true;
}

void Editor2D::mouse_button_up(int boton) {
    (void)boton;
    ViewPortClickDown = false;
    g_redraw = true;
}
#endif
