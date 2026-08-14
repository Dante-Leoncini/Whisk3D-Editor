#include "ViewPorts/Editor2D.h"
#include "objects/Textures.h"        // Textures[0] = atlas de iconos (para la barra)
#include "w3dGraphics.h"             // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"   // W3dPantallaAlto
#include "WhiskUI/theme/colores.h"   // ListaColores / ColorID
#include "render/OpcionesRender.h"   // g_redraw (render event-driven)
#include "PopUp/PopUpBase.h"         // PopUpActive (los popups modales tienen prioridad)
#include "objects/Objects.h"         // SceneCollection / ObjActivo (los UI viven en la escena)
#include "objects/Texto2D.h"         // el elemento de texto
#include "objects/Imagen2D.h"        // el elemento de imagen (resize por puntos de agarre)
#include "objects/Rect2D.h"          // el elemento rectangulo
#include "objects/Contenedor2D.h"    // el contenedor (rect invisible)
#include "objects/Slice9.h"          // imagen con bordes fijos (guias de sus cortes)
#include "io/Fuente2D.h"             // fuentes del editor 2D (atlas)
#include "Undo.h"                    // Ctrl+Z de los transforms/drags del editor 2D
#include "WhiskUI/text/W3dTextAtlas.h"
#include "W3dLang.h"                 // T()
#include "objects/ObjectMode.h"      // W3dDuplicarUno (duplicar la seleccion 2D)
#include "PopUp/ConfirmarPopup.h"    // AbrirConfirmarBorrado (X / menu Objeto)
#include "WhiskUI/draw/icons.h"      // IconType (boton del pivote)
#include "ViewPorts/TransformUI.h"   // UI compartida del transform (barra de info / numerico / tilde-cruz-ejes)
#include "ViewPorts/NumInput.h"      // entrada numerica compartida (NumInputActivo/Reset)
#include "script/SimJuego.h"         // SimActiva(): solo alimentar los binds cuando el juego corre
#include "script/BindsJuego.h"       // alimentar mapeo + posiciones resueltas -> areaSegura()/cajaUI()
#include <math.h>
#include <vector>

namespace gfx = w3dEngine;

#include "objects/UI.h"

// el tamano de la "ventana" que simula el lienzo: el del UI que se esta editando
// ("como el render" en vivo, o su tamano propio si esta en modo responsive).
static void TamanoLienzo(float* w, float* h) { UI2D_TamanoLienzo(w, h); }

// los 8 puntos de agarre de un rect: 0=arr-izq 1=arriba 2=arr-der 3=der 4=aba-der
// 5=abajo 6=aba-izq 7=izq (esquinas y centros de borde)
static void PuntosDeAgarre(float x0, float y0, float x1, float y1, float* px, float* py) {
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    px[0]=x0; py[0]=y0;  px[1]=cx; py[1]=y0;  px[2]=x1; py[2]=y0;  px[3]=x1; py[3]=cy;
    px[4]=x1; py[4]=y1;  px[5]=cx; py[5]=y1;  px[6]=x0; py[6]=y1;  px[7]=x0; py[7]=cy;
}

static void RotarPuntoFwd(float* px, float* py, float cx, float cy, float grados);

// dibuja los 8 puntos (cuadraditos rellenos) sobre el rect, rotados con el elemento
// (rot alrededor de cx,cy). Asume estado de lineas 2D.
static void DibujarPuntosDeAgarre(float x0, float y0, float x1, float y1, const float* col,
                                  float rot = 0.0f, float cx = 0.0f, float cy = 0.0f) {
    float hx[8], hy[8];
    PuntosDeAgarre(x0, y0, x1, y1, hx, hy);
    float r = 2.5f * (float)GlobalScale;   // "un punto mas grande": bien visible
    gfx::Color4f(col[0], col[1], col[2], 1.0f);
    for (int i = 0; i < 8; i++) {
        if (rot != 0.0f) RotarPuntoFwd(&hx[i], &hy[i], cx, cy, rot);
        float V[12] = { hx[i]-r,hy[i]-r, hx[i]+r,hy[i]-r, hx[i]+r,hy[i]+r,
                        hx[i]-r,hy[i]-r, hx[i]+r,hy[i]+r, hx[i]-r,hy[i]+r };
        gfx::VertexPointer2f(0, V);
        gfx::DrawTrianglesArray(6);
    }
}

// que punto de agarre del rect cae bajo el mouse (coords locales); -1 si ninguno.
// rot/cx/cy: la rotacion del elemento (los puntos giran con el)
static int HandleBajoMouse(float x0, float y0, float x1, float y1, float mx, float my,
                           float rot = 0.0f, float cx = 0.0f, float cy = 0.0f) {
    float hx[8], hy[8];
    PuntosDeAgarre(x0, y0, x1, y1, hx, hy);
    float r = 4.0f * (float)GlobalScale;   // radio de click, un toque mas grande que el punto
    for (int i = 0; i < 8; i++) {
        if (rot != 0.0f) RotarPuntoFwd(&hx[i], &hy[i], cx, cy, rot);
        if (mx >= hx[i]-r && mx <= hx[i]+r && my >= hy[i]-r && my <= hy[i]+r) return i;
    }
    return -1;
}

// rota (px,py) alrededor de (cx,cy) 'grados' (Y hacia abajo: positivo = horario)
static void RotarPunto(float* px, float* py, float cx, float cy, float grados) {
    RotarPuntoFwd(px, py, cx, cy, grados);
}
static void RotarPuntoFwd(float* px, float* py, float cx, float cy, float grados) {
    if (grados == 0.0f) return;
    float rad = grados * 0.01745329f;
    float c = cosf(rad), s = sinf(rad);
    float dx = *px - cx, dy = *py - cy;
    *px = cx + dx * c - dy * s;
    *py = cy + dx * s + dy * c;
}

// el layout del PADRE del elemento (si esta en filas/columnas su posicion no se edita)
static int LayoutDelPadre(Object* o) {
    Object* p = o ? o->Parent : NULL;
    if (!p) return 0;
    if (p->getType() == ObjectType::ui) return ((UI*)p)->layoutHijos;
    if (UI2D_EsElemento2D(p))           return ((Elemento2D*)p)->layoutHijos;
    return 0;
}

// linea punteada 2D (guiones a mano: GLES no tiene stipple), como la del 3D/UV
static void DibujarLineaPunteada(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 2.0f) return;
    float ux = dx / len, uy = dy / len;
    const float trazo = 6.0f, hueco = 5.0f, paso = trazo + hueco;
    float v[4 * 128];
    int n = 0;
    for (float t = 0.0f; t < len && n < 128; t += paso) {
        float t2 = t + trazo; if (t2 > len) t2 = len;
        v[n*4+0] = ax + ux*t;  v[n*4+1] = ay + uy*t;
        v[n*4+2] = ax + ux*t2; v[n*4+3] = ay + uy*t2;
        n++;
    }
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    SetColorID(ColorID::accent, 0.9f);
    gfx::LineWidth(1.0f);
    gfx::VertexPointer2f(0, v);
    gfx::DrawLines(n * 2);
}

// saca UN objeto de la seleccion (para el toggle con Shift+click)
static void Deseleccionar2D(Object* o) {
    if (!o || !o->select) return;
    o->Deseleccionar();
}

Editor2D::Editor2D() {
    zoom = 1.0f; zoomInicializado = false;
    panX = 0.0f; panY = 0.0f;
    lastMx = 0; lastMy = 0;
    cursor2DX = 320.0f; cursor2DY = 240.0f;   // (se recentra solo hasta que lo coloques)
    cursorColocado = false;
    pivotModo = 0;                             // girar desde el centro de la seleccion
    xform = 0; xformEje = 0; xformMx = xformMy = 0;
    xformPivX = xformPivY = 0.0f;
    drag = 0; dragHandle = -1; dragObj = NULL;
    dragOrigW = dragOrigH = 0.0f; dragMx = dragMy = 0;
    dragObjRefW = dragObjRefH = 1.0f;
    dragPaso = false;
    acumX = acumY = acumAng = 0.0f; acumFactor = 1.0f;
    accionPrimerMov = false;
    moverTrasDuplicar = false;
    vistaJuego = false;   // se entra con numpad 0 (o menu View); ver Editor2D.h
    lastFx = lastFy = 0.0f; lastEsc = 1.0f;
    BarCrear();   // el boton [0] de la barra (menu de tipo/split), como todos los viewports
    // botones de barra, cada uno con su ROL estable (BarRol2D): el dispatch (LayoutClickBarra2D)
    // los busca por rol, NO por indice -> agregar/reordenar botones no rompe nada.
    Button* b;
    b = new Button(T("Select")); b->rol = BR2D_Select;                 // todo / nada / invertir
        b->desplegable = true; BarButtons.push_back(b);
    b = new Button(T("Add")); b->rol = BR2D_Add;                       // crear elementos 2D
        b->desplegable = true; BarButtons.push_back(b);
    b = new Button(T("Object")); b->rol = BR2D_Object;                 // transformar/duplicar/borrar
        b->desplegable = true; BarButtons.push_back(b);
    b = new Button("", (int)IconType::monitor); b->rol = BR2D_View;    // zoom / vista de juego
        b->desplegable = true; BarButtons.push_back(b);
    b = new Button("", (int)IconType::pivotMedian); b->rol = BR2D_Pivot; // pivote (icono)
        b->desplegable = true; BarButtons.push_back(b);
    // TOOLBAR inferior (mecanismo compartido de ViewportBase): historial MRU con Mover/Rotar/
    // Escalar de ELEMENTOS 2D (los starters de IniciarXform2D, mismos que G/R/S y el menu Object).
    for (int i = 0; i < 3; i++){
        b = new Button(""); b->rol = TBR_2DHist + i; b->visible = false; ToolButtons.push_back(b);
    }
    // botones de TRANSFORM compartidos (tilde/cruz/X/Y, roles TBR_* del 3D): solo se ven
    // durante un G/R/S propio (ToolbarSincronizarTransform2D, como en el 3D y el UV)
    ToolbarCrearTransform2D(ToolButtons);
}

// ---- TOOLBAR inferior (compartida): historial G/R/S por editor ----
// MRU PROPIO del Editor 2D (compartido entre todos los viewports 2D, como el del 3D por modo).
static std::vector<int> gToolHist2D;
static std::vector<int>& Toolbar2DHist(){
    if (gToolHist2D.empty()){ gToolHist2D.push_back(TBMove); gToolHist2D.push_back(TBRotate); gToolHist2D.push_back(TBScale); }
    return gToolHist2D;
}

// oculta en VISTA DE JUEGO y durante el PLAY del juego (preview limpio, como la del 3D)
bool Editor2D::ToolbarVisible() const {
    extern bool SimActiva();
    extern bool AnimEsJuego; // animation/Animation.cpp: la anim activa es un "juego"
    if (vistaJuego) return false;
    if (AnimEsJuego && SimActiva()) return false;
    return true;
}

void Editor2D::ToolbarSincronizar(){
    std::vector<int>& h = Toolbar2DHist();
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        int hi = btn->rol - TBR_2DHist;
        if (hi >= 0 && hi < 3){
            // durante un transform el historial se esconde (la barra es tilde/cruz/ejes, como el 3D)
            btn->visible = !xform && hi < (int)h.size();
            if (btn->visible) btn->text = ToolbarAccionLabel(h[hi]);
        }
    }
    // TRANSFORM en curso: tilde/cruz/X/Y compartidos (mismas reglas y colores que el 3D/UV)
    ToolbarSincronizarTransform2D(ToolButtons, xform != 0, xformEje);
}

void Editor2D::ToolbarAccionRol(int rol){
    // botones de TRANSFORM compartidos (tilde/cruz/X/Y): mismo camino que las teclas
    if (xform && ToolbarAccionTransform2D(this, rol)) return;
    if (xform) { XformConfirmar(); return; } // como cualquier click: un transform en curso se CONFIRMA (con gate)
    int hi = rol - TBR_2DHist;
    std::vector<int>& h = Toolbar2DHist();
    if (hi >= 0 && hi < (int)h.size()){
        int id = h[hi];
        IniciarXform2D((id == TBMove) ? 1 : (id == TBRotate) ? 2 : 3); // (registra el MRU adentro)
    }
}

Editor2D::~Editor2D() {
    UI2D_OverrideVentanaQuitar(this);   // no dejar un override colgado de un viewport muerto
}

// VISTA DE JUEGO: el LIENZO del juego es el rect util de ESTE viewport (viewport menos su
// barra), como la ventana real del runtime en la rama igualQueRender de w3drun ("el editor 2D
// ES el verdadero render para un juego"). Se materializa con el override de UI2D_TamanoVentana,
// asi el DIBUJO (TamanoLienzo/ParamsLienzo), el INPUT (SimJuego MapearAJuego), los BINDS
// (pantalla()/posPx/cajaUI leen UI2D_TamanoLienzo) y Properties ven TODOS el mismo lienzo.
// Se re-fija en cada Render (sigue los resize del viewport) y al alimentar input; se limpia al
// salir del modo. Con la UI de tamano FIJO (igualQueRender=false) NO hay override: ese lienzo
// fijo se letterboxea en el viewport, que ya era el comportamiento correcto.
void Editor2D::ActualizarOverrideLienzo() {
    UI* u = UI2D_UIDelEditor();
    bool dinamica = (!u || u->igualQueRender);   // sin UI en escena el lienzo tambien es "de ventana"
    if (vistaJuego && dinamica) {
        int uy, uh; RectUtil(&uy, &uh);
        UI2D_OverrideVentana(this, (float)width, (float)uh);
    } else {
        UI2D_OverrideVentanaQuitar(this);
    }
}

void Editor2D::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH); // el borde sigue el tamano (como Outliner/Properties)
}

// el rect UTIL del viewport: todo MENOS su barra de menu, en coords LOCALES (0 = borde de
// arriba del viewport). OJO: BarTopOffset() NO alcanza, devuelve 0 cuando la barra esta ABAJO
// y esa barra igual tapa el lienzo (se dibuja despues del contenido).
void Editor2D::RectUtil(int* uy, int* uh) const {
    int arri = 0, abaj = 0;
    if (barCard) { if (barAbajo) abaj = BarHeight(); else arri = BarHeight(); }
    int h = height - arri - abaj; if (h < 1) h = 1;
    *uy = arri; *uh = h;
}

// centro del marco en pantalla (cx,cy) y escala s (pixeles de viewport por pixel de lienzo).
// La escala es el ZOOM directo: cambiar el tamano del lienzo o de un elemento NO re-escala
// la vista (antes se auto-ajustaba y arrastrar un borde hacia un desastre).
// UNICO punto de verdad del mapeo: la VISTA DE JUEGO se resuelve entera aca (Render, el
// hit-test, los drags, el encuadre y el toque del Play salen todos de estos tres valores).
void Editor2D::ParamsLienzo(float& cx, float& cy, float& s) const {
    if (vistaJuego) {
        // VISTA DE JUEGO: el lienzo ENCAJADO (letterbox) y centrado en el rect util, sin
        // zoom ni paneo. Es la MISMA cuenta que hace el runtime en W3dGameRender
        // (Whisk3D-Examples/game/w3drun.cpp) con la ventana real: esc = min(ancho/lw, alto/lh).
        // Con la UI "igual que el render" el override de ActualizarOverrideLienzo hace que
        // TamanoLienzo devuelva justo el rect util -> esta cuenta degenera a esc=1 y el lienzo
        // LLENA el viewport (la rama igualQueRender de w3drun). Con la UI de tamano FIJO no hay
        // override y esta misma cuenta hace el letterbox de siempre (tambien igual que w3drun).
        // OJO: la escala puede no ser entera, o sea que se pierde el pixel-perfect de la fuente
        // pixel. Es CORRECTO: en el dispositivo tampoco es entera (no es un bug de la fuente).
        int uy, uh; RectUtil(&uy, &uh);
        float lw, lh; TamanoLienzo(&lw, &lh);
        if (lw < 1.0f) lw = 1.0f;
        if (lh < 1.0f) lh = 1.0f;
        float e = (float)width / lw;
        float ey = (float)uh / lh;
        if (ey < e) e = ey;
        if (e < 0.0001f) e = 0.0001f;
        cx = width * 0.5f;
        cy = (float)uy + uh * 0.5f;
        s  = e;
        return;
    }
    const int top = BarTopOffset();
    int ch = height - top; if (ch < 1) ch = 1;
    cx = width * 0.5f + panX;
    cy = top + ch * 0.5f + panY;
    s  = zoom;
}

void Editor2D::Render() {
    const int glY = W3dPantallaAlto - y - height;
    // VISTA DE JUEGO: (re)fijar el override del lienzo ANTES de leer TamanoLienzo/ParamsLienzo
    // (cada frame: sigue los resize del viewport y los cambios de igualQueRender en caliente)
    ActualizarOverrideLienzo();
    // VISTA DE JUEGO (numpad 0): preview limpio. 'chrome' apaga TODOS los adornos de edicion
    // (marco, guias, bordes de seleccion, origenes, puntos de agarre, cursor 2D).
    const bool chrome = !vistaJuego;
    int utilY, utilH; RectUtil(&utilY, &utilH);

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
    // en VISTA DE JUEGO el contenido se recorta al rect UTIL: nada se mete debajo de la barra
    // de menu (el Clear de arriba ya pinto el viewport ENTERO con el fondo)
    if (vistaJuego)
        gfx::Scissor(x, glY + (height - utilY - utilH), width, utilH);

    float LIENZO_W, LIENZO_H; TamanoLienzo(&LIENZO_W, &LIENZO_H);
    // primer render: zoom 1:1 (PIXEL-PERFECT: un px del lienzo = un px de pantalla; la
    // fuente pixel y los bordes se ven EXACTOS). Solo si el lienzo no entra, el fit.
    if (!zoomInicializado && LIENZO_H > 0.0f) {
        int ch = height - BarTopOffset(); if (ch < 1) ch = 1;
        zoom = 1.0f;
        if (LIENZO_H > ch * 0.95f || LIENZO_W > width * 0.95f)
            zoom = ch * 0.8f / LIENZO_H;
        zoomInicializado = true;
    }
    float cx, cy, s;
    ParamsLienzo(cx, cy, s);
    // esquina superior izquierda del marco en pantalla (el lienzo va de 0,0 a ancho,alto del UI)
    const float fx = cx - LIENZO_W * 0.5f * s;
    const float fy = cy - LIENZO_H * 0.5f * s;
    // el cursor 2D arranca en el centro de la ventana (y sigue su tamano hasta que lo coloques)
    if (!cursorColocado) { cursor2DX = LIENZO_W * 0.5f; cursor2DY = LIENZO_H * 0.5f; }

    // ---- la "pantalla": interior del color de las tarjetas, el resto queda del fondo ----
    const float fx1 = fx + LIENZO_W * s;
    const float fy1 = fy + LIENZO_H * s;
    // VISTA DE JUEGO: PASSEPARTOUT. Lo que sobra del rect util alrededor del lienzo se pinta
    // oscuro (las bandas del letterbox, como en el dispositivo) para que se vea DONDE termina
    // la pantalla del juego y que el modo esta activo.
    if (vistaJuego) {
        float uy0 = (float)utilY, uy1 = (float)(utilY + utilH);
        float Q[12] = { 0,uy0, (float)width,uy0, (float)width,uy1,
                        0,uy0, (float)width,uy1, 0,uy1 };
        gfx::Color4f(0.04f, 0.04f, 0.05f, 1.0f);
        gfx::VertexPointer2f(0, Q);
        gfx::DrawTrianglesArray(6);
    }
    { const float* g = ListaColores[(int)ColorID::gris];
      float Q[12] = { fx,fy, fx1,fy, fx1,fy1,  fx,fy, fx1,fy1, fx,fy1 };
      // en vista de juego el fondo del lienzo es NEGRO: el mismo clear que hace el runtime
      if (vistaJuego) gfx::Color4f(0.0f, 0.0f, 0.0f, 1.0f);
      else            gfx::Color4f(g[0], g[1], g[2], 1.0f);
      gfx::VertexPointer2f(0, Q);
      gfx::DrawTrianglesArray(6);
    }
    if (chrome) {
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
    }

    // ---- linea del PADDING: el area REAL donde se enganchan bordes/esquinas de los hijos
    //      (mas transparente; solo cuando el UI esta seleccionado) ----
    { UI* uped = chrome ? UI2D_UIDelEditor() : NULL;
      if (uped && (uped->padIzq > 0.0f || uped->padDer > 0.0f || uped->padArr > 0.0f ||
                   uped->padAba > 0.0f) && (uped->select || ObjActivo == (Object*)uped)) {
          float menor = ((LIENZO_W < LIENZO_H) ? LIENZO_W : LIENZO_H) * s;
          float f = uped->padGapPx ? s : menor;
          float px0 = fx + uped->padIzq * f, py0 = fy + uped->padArr * f;
          float px1 = fx1 - uped->padDer * f, py1 = fy1 - uped->padAba * f;
          if (px1 > px0 && py1 > py0) {
              float P[16] = { px0,py0, px1,py0,  px1,py0, px1,py1,
                              px1,py1, px0,py1,  px0,py1, px0,py0 };
              gfx::Enable(gfx::Blend); gfx::BlendAlpha();
              gfx::LineWidth(1.0f);
              gfx::Color4f(0.85f, 0.85f, 0.88f, 0.35f);
              gfx::VertexPointer2f(0, P);
              gfx::DrawLines(8);
          }
      } }

    // ---- la INTERFAZ (mismo dibujador que el viewport 3D), sobre el marco del lienzo ----
    lastFx = fx; lastFy = fy; lastEsc = s;          // para pasar pantalla<->lienzo en el input
    posiciones.clear();
    // el overflow recorta con scissor absoluto. En VISTA DE JUEGO el recorte de arranque es el
    // rect UTIL (sin la barra), asi un elemento que se sale del lienzo no invade la barra.
    if (vistaJuego) {
        float clip[4] = { 0.0f, (float)utilY, (float)width, (float)(utilY + utilH) };
        UI2D_BaseRecorte(x, y, width, height, clip);
    } else {
        UI2D_BaseRecorte(x, y, width, height);
    }
    UI2D_DibujarOverlay(fx, fy, LIENZO_W * s, LIENZO_H * s, s, &posiciones);

    // ---- rect SEGURO en px de VENTANA (pantalla), como lo espera BindsJuegoRender (safeWin*).
    // En el EDITOR el area segura es SIEMPRE el lienzo entero: en el DISPOSITIVO real los insets
    // los reporta el OS (W3dGameSafeArea) y el runtime los aplica solo en W3dGameRender.
    float safeX0 = fx, safeY0 = fy;
    float safeW  = LIENZO_W * s, safeH = LIENZO_H * s;
    // alimentar a BindsJuego el mapeo ventana<->lienzo + el rect SEGURO + las posiciones RESUELTAS,
    // para que areaSegura()/cajaUI() lean lo mismo que se dibujo, IGUAL que el runtime (w3drun) via
    // W3dGameRender. Se alimenta SIEMPRE (edicion Y Play). (El input lo alimenta SimJuego.)
    BindsJuegoRender(s, fx, fy, safeX0, safeY0, safeW, safeH, &posiciones);

    // estado de lineas de nuevo (el texto deja mezcla premultiplicada y textura prendida)
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();

    // ---- VISTA DE JUEGO: borde DISCRETO del lienzo (donde termina la pantalla del juego).
    //      Va DESPUES del contenido (sino la UI lo tapa) y con el color accent tenue: es la
    //      unica marca del modo dentro del lienzo, junto con el boton [4] en verde. ----
    if (vistaJuego) {
        const float* ac = ListaColores[(int)ColorID::accent];
        const float bordeVJ[] = { fx,fy, fx1,fy,   fx1,fy, fx1,fy1,
                                  fx1,fy1, fx,fy1, fx,fy1, fx,fy };
        gfx::LineWidth(1.0f);
        gfx::Color4f(ac[0], ac[1], ac[2], 0.5f);
        gfx::VertexPointer2f(0, bordeVJ);
        gfx::DrawLines(8);
    }

    // ---- BORDE de seleccion (el rect del texto) + ORIGEN con el sprite origen.png ----
    // (en VISTA DE JUEGO no va NADA de esto: el modo muestra el juego, no la edicion)
    for (size_t i = 0; chrome && i < posiciones.size(); i++) {
        Object* o = posiciones[i].obj;
        if (!o->select && o != ObjActivo) continue;
        const UI2DPos& P = posiciones[i];
        const float* col = ListaColores[(int)((o == ObjActivo) ? ColorID::accent : ColorID::accentDark)];
        // borde alrededor del elemento; ROTA con el (las 4 esquinas giradas alrededor del origen)
        float m = 4.0f;
        float rotB = *UI2D_Rot2dDe(o);
        float ex[4] = { P.bx0 - m, P.bx1 + m, P.bx1 + m, P.bx0 - m };
        float ey[4] = { P.by0 - m, P.by0 - m, P.by1 + m, P.by1 + m };
        for (int k = 0; k < 4; k++) RotarPunto(&ex[k], &ey[k], P.sx, P.sy, rotB);
        float B[16] = { ex[0],ey[0], ex[1],ey[1],  ex[1],ey[1], ex[2],ey[2],
                        ex[2],ey[2], ex[3],ey[3],  ex[3],ey[3], ex[0],ey[0] };
        gfx::LineWidth(1.0f);
        gfx::Color4f(col[0], col[1], col[2], 1.0f);
        gfx::VertexPointer2f(0, B);
        gfx::DrawLines(8);
    }
    // el ORIGEN: el mismo sprite (origen.png) que usa el viewport 3D, tenido con el color de seleccion
    if (chrome && Textures.size() > 1 && Textures[1]) {
        gfx::Enable(gfx::Texture2D);
        gfx::EnableArray(gfx::TexCoordArray);
        gfx::BindTexture(Textures[1]->iID);
        gfx::TexFilter(false);
        for (size_t i = 0; i < posiciones.size(); i++) {
            Object* o = posiciones[i].obj;
            if (!o->select && o != ObjActivo) continue;
            const float* col = ListaColores[(int)((o == ObjActivo) ? ColorID::accent : ColorID::accentDark)];
            float px = posiciones[i].sx, py = posiciones[i].sy;
            float r = 4.0f * (float)GlobalScale;
            float V[12] = { px-r,py-r, px+r,py-r, px+r,py+r,  px-r,py-r, px+r,py+r, px-r,py+r };
            float U[12] = { 0,0, 1,0, 1,1,  0,0, 1,1, 0,1 };
            gfx::Color4f(col[0], col[1], col[2], 1.0f);
            gfx::VertexPointer2f(0, V);
            gfx::TexCoordPointer2f(0, U);
            gfx::DrawTrianglesArray(6);
        }
        gfx::Disable(gfx::Texture2D);
        gfx::DisableArray(gfx::TexCoordArray);
    }

    // ---- PUNTOS DE AGARRE: el elemento activo con rect (imagen/rectangulo) se
    //      redimensiona por esquinas/bordes ----
    { float *ew, *eh;
      if (chrome && ObjActivo && UI2D_TamanoElem(ObjActivo, &ew, &eh)) {
        for (size_t i = 0; i < posiciones.size(); i++) {
            if (posiciones[i].obj != ObjActivo) continue;
            const UI2DPos& P = posiciones[i];
            DibujarPuntosDeAgarre(P.bx0, P.by0, P.bx1, P.by1,
                                  ListaColores[(int)ColorID::accent],
                                  *UI2D_Rot2dDe(ObjActivo), P.sx, P.sy);
            break;
        }
      } }
    // en modo RESPONSIVE el marco del lienzo tambien se agarra (probar otra pantalla al vuelo)
    { UI* ued = chrome ? UI2D_UIDelEditor() : NULL;
      if (ued && !ued->igualQueRender)
          DibujarPuntosDeAgarre(fx, fy, fx1, fy1, ListaColores[(int)ColorID::blanco]); }

    // ---- GUIAS del SLICE9 activo: donde caen sus cortes (los bordes dibujados) ----
    if (chrome && ObjActivo && ObjActivo->getType() == ObjectType::slice9) {
        for (size_t i = 0; i < posiciones.size(); i++) {
            if (posiciones[i].obj != ObjActivo) continue;
            const UI2DPos& P = posiciones[i];
            float dX, dY;
            UI2D_CortesSlice9((Slice9*)ObjActivo, P.bx0, P.by0, P.bx1, P.by1,
                              UI2D_EscalaGlobalDe(ObjActivo), &dX, &dY);
            DibujarLineaPunteada(P.bx0 + dX, P.by0, P.bx0 + dX, P.by1);   // corte izquierdo
            DibujarLineaPunteada(P.bx1 - dX, P.by0, P.bx1 - dX, P.by1);   // derecho
            DibujarLineaPunteada(P.bx0, P.by0 + dY, P.bx1, P.by0 + dY);   // superior
            DibujarLineaPunteada(P.bx0, P.by1 - dY, P.bx1, P.by1 - dY);   // inferior
            break;
        }
    }

    // ---- ROTAR/ESCALAR: linea punteada del pivote al mouse (como el 3D y el UV) ----
    if (xform == 2 || xform == 3)
        DibujarLineaPunteada(xformPivX, xformPivY, (float)(lastMx - x), (float)(lastMy - y));

    // ---- PIVOTE (centro de rotacion): SOLO mientras rotas con R (sino ensucia la vista) ----
    if (xform == 2) {
        float pvx = xformPivX, pvy = xformPivY;
        float r = 7.0f;
        float C[8] = { pvx-r,pvy, pvx+r,pvy, pvx,pvy-r, pvx,pvy+r };
        gfx::LineWidth(2.0f);
        gfx::Color4f(1.0f, 0.75f, 0.15f, 1.0f);   // naranja: centro de rotacion
        gfx::VertexPointer2f(0, C);
        gfx::DrawLines(4);
    }

    // ---- CURSOR 2D: solo si lo colocaste (Shift+click) o si es el pivote elegido ----
    if (chrome && (cursorColocado || pivotModo == 2)) {
        float cxp = fx + cursor2DX * s, cyp = fy + cursor2DY * s;
        float r = 6.0f;
        float C1[8] = { cxp-r-4,cyp, cxp-r+2,cyp, cxp+r-2,cyp, cxp+r+4,cyp };
        float C2[8] = { cxp,cyp-r-4, cxp,cyp-r+2, cxp,cyp+r-2, cxp,cyp+r+4 };
        gfx::LineWidth(2.0f);
        gfx::Color4f(0.95f, 0.25f, 0.25f, 1.0f);
        gfx::VertexPointer2f(0, C1); gfx::DrawLines(4);
        gfx::Color4f(0.95f, 0.95f, 0.95f, 1.0f);
        gfx::VertexPointer2f(0, C2); gfx::DrawLines(4);
    }

    // Shift+D: las copias recien ahora tienen posicion resuelta -> arrancar el mover
    if (moverTrasDuplicar) {
        moverTrasDuplicar = false;
        IniciarXform2D(1);
    }

    // el boton View (el del monitor) se pone VERDE mientras la VISTA DE JUEGO esta
    // activa: sin marco ni guias, sino es facil confundir el modo con "se rompio el zoom"
    { Button* bView = BarRolBtn(BarButtons, BR2D_View);
      if (bView) bView->colorTexto = vistaJuego ? ListaColores[(int)ColorID::accent] : NULL; }
    // el icono del boton de pivote refleja el modo actual
    { Button* bPiv = BarRolBtn(BarButtons, BR2D_Pivot);
      if (bPiv) bPiv->icon = (pivotModo == 2) ? (int)IconType::pivotCursor :
                             (pivotModo == 1) ? (int)IconType::pivotActive : (int)IconType::pivotMedian; }

    gfx::Disable(gfx::ScissorTest);
    // RenderBar/DibujarBordes dibujan quads texturizados (iconos del atlas): dejarles el estado
    // prendido y RE-BINDEAR el atlas (mismo cuidado que el UV editor).
    gfx::Enable(gfx::Texture2D);
    gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    gfx::EnableArray(gfx::VertexArray);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::ColorArray);
    gfx::DisableArray(gfx::NormalArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    // durante un G/R/S propio la barra de menu se vuelve barra de INFO (operacion + eje +
    // delta en px de lienzo), igual que el 3D. El dibujo es el compartido de TransformUI.
    if (xform) RenderBarraInfo(XformTextoBarra());
    else RenderBar();
    RenderToolbar();     // toolbar inferior compartida (Move/Rotate/Scale; tilde/cruz/X/Y en transform)
    DibujarBordes(this); // borde del viewport (verde si es el activo)
}

// ---- TRANSFORM 2D (G mover / R rotar, con X/Y para restringir el eje) -----------------------

// la posicion RESUELTA en pantalla de un objeto (del ultimo Render); true si estaba
static bool PosResuelta(const std::vector<UI2DPos>& v, Object* o, float* sx, float* sy) {
    for (size_t i = 0; i < v.size(); i++)
        if (v[i].obj == o) { *sx = v[i].sx; *sy = v[i].sy; return true; }
    return false;
}

void Editor2D::PivoteEnPantalla(float* px, float* py) {
    if (pivotModo == 2) {                          // cursor 2D
        *px = lastFx + cursor2DX * lastEsc;
        *py = lastFy + cursor2DY * lastEsc;
        return;
    }
    if (pivotModo == 1 && ObjActivo) {             // elemento activo
        if (PosResuelta(posiciones, ObjActivo, px, py)) return;
    }
    // centro (mediana) de la seleccion
    float sx = 0, sy = 0; int n = 0;
    for (size_t i = 0; i < posiciones.size(); i++)
        if (posiciones[i].obj->select) { sx += posiciones[i].sx; sy += posiciones[i].sy; n++; }
    if (n) { *px = sx / n; *py = sy / n; }
    else   { *px = lastFx; *py = lastFy; }
}

void Editor2D::IniciarXform2D(int modo) {
    xformObjs.clear(); xformOrig.clear(); xformRotOrig.clear(); xformResueltas.clear();
    xformTamOrig.clear(); xformTamOrig2.clear();
    for (size_t i = 0; i < posiciones.size(); i++) {
        Object* o = posiciones[i].obj;
        if (!o->select || !UI2D_EsElemento2D(o)) continue;
        if (LayoutDelPadre(o) != 0) continue;   // en filas/columnas la posicion no se toca
        xformObjs.push_back(o);
        xformOrig.push_back(o->pos);
        xformRotOrig.push_back(*UI2D_Rot2dDe(o));
        float *ew, *eh;
        if (UI2D_TamanoElem(o, &ew, &eh)) {
            xformTamOrig.push_back(*ew);
            xformTamOrig2.push_back(*eh);
        } else {
            xformTamOrig.push_back(((Texto2D*)o)->tam);
            xformTamOrig2.push_back(0.0f);
        }
        xformResueltas.push_back(posiciones[i]);
    }
    if (xformObjs.empty()) return;                 // nada 2D seleccionado
    UndoTransformIniciar();   // Ctrl+Z: snapshot previo (se pushea al confirmar)
    // historial MRU de la toolbar del 2D: teclado (G/R/S), menu Object y toolbar, mismo historial
    ToolbarMRU(Toolbar2DHist(), (modo == 1) ? TBMove : (modo == 2) ? TBRotate : TBScale);
    xform = modo; xformEje = 0;
    xformMx = lastMx; xformMy = lastMy;
    acumX = acumY = acumAng = 0.0f; acumFactor = 1.0f;
    accionPrimerMov = true;   // el 1er motion no suma (lastMx puede venir viejo de un menu)
    PivoteEnPantalla(&xformPivX, &xformPivY);
    NumInputReset();          // sin restos de una entrada numerica anterior
    g_redraw = true;
}

// aplica el transform en curso desde los ACUMULADORES (wrap-safe); mx/my ya no se usan
void Editor2D::AplicarXform2D(int mx, int my) {
    (void)mx; (void)my;
    if (!xform) return;
    if (xform == 1) {                              // MOVER: delta acumulado, en RELATIVO
        float dx = acumX, dy = acumY;              // px de pantalla
        if (xformEje == 1) dy = 0.0f;              // X = solo horizontal
        if (xformEje == 2) dx = 0.0f;              // Y = solo vertical
        for (size_t i = 0; i < xformObjs.size(); i++) {
            float rW = xformResueltas[i].refW > 0.0f ? xformResueltas[i].refW : 1.0f;
            float rH = xformResueltas[i].refH > 0.0f ? xformResueltas[i].refH : 1.0f;
            xformObjs[i]->pos.x = xformOrig[i].x + dx / rW;
            xformObjs[i]->pos.y = xformOrig[i].y + dy / rH;
        }
    } else if (xform == 2) {                       // ROTAR alrededor del pivote
        float d   = acumAng * 57.29578f;           // grados (Y-abajo: positivo = horario)
        float rad = acumAng;
        for (size_t i = 0; i < xformObjs.size(); i++) {
            Object* o = xformObjs[i];
            *UI2D_Rot2dDe(o) = xformRotOrig[i] + d;
            // la posicion ORBITA el pivote (si el pivote no es su propio origen)
            float rx = xformResueltas[i].sx - xformPivX;
            float ry = xformResueltas[i].sy - xformPivY;
            float nx = xformPivX + rx * cosf(rad) - ry * sinf(rad);
            float ny = xformPivY + rx * sinf(rad) + ry * cosf(rad);
            float rW = xformResueltas[i].refW > 0.0f ? xformResueltas[i].refW : 1.0f;
            float rH = xformResueltas[i].refH > 0.0f ? xformResueltas[i].refH : 1.0f;
            o->pos.x = xformOrig[i].x + (nx - xformResueltas[i].sx) / rW;
            o->pos.y = xformOrig[i].y + (ny - xformResueltas[i].sy) / rH;
        }
    } else {                                       // ESCALAR desde el pivote
        float f = acumFactor; if (f < 0.01f) f = 0.01f;
        float fx2 = (xformEje == 2) ? 1.0f : f;    // X/Y restringen el eje
        float fy2 = (xformEje == 1) ? 1.0f : f;
        for (size_t i = 0; i < xformObjs.size(); i++) {
            Object* o = xformObjs[i];
            float *ew, *eh;
            if (UI2D_TamanoElem(o, &ew, &eh)) {
                *ew = xformTamOrig[i] * fx2;
                *eh = xformTamOrig2[i] * fy2;
            } else {
                ((Texto2D*)o)->tam = xformTamOrig[i] * f;   // el texto escala parejo
            }
            // la posicion se ALEJA/ACERCA del pivote con el factor
            float nx = xformPivX + (xformResueltas[i].sx - xformPivX) * fx2;
            float ny = xformPivY + (xformResueltas[i].sy - xformPivY) * fy2;
            float rW = xformResueltas[i].refW > 0.0f ? xformResueltas[i].refW : 1.0f;
            float rH = xformResueltas[i].refH > 0.0f ? xformResueltas[i].refH : 1.0f;
            o->pos.x = xformOrig[i].x + (nx - xformResueltas[i].sx) / rW;
            o->pos.y = xformOrig[i].y + (ny - xformResueltas[i].sy) / rH;
        }
    }
    g_redraw = true;
}

void Editor2D::ConfirmarXform2D() {
    UndoTransformConfirmar();   // pushea el paso de undo (si hubo cambio real)
    xform = 0; xformEje = 0; g_redraw = true;
    NumInputReset(); // la entrada numerica muere con el transform (como el Aceptar del 3D)
}

void Editor2D::CancelarXform2D() {
    UndoTransformCancelar();    // cancelado: el snapshot pendiente se descarta
    for (size_t i = 0; i < xformObjs.size(); i++) {
        Object* o = xformObjs[i];
        o->pos = xformOrig[i];
        *UI2D_Rot2dDe(o) = xformRotOrig[i];
        float *ew, *eh;
        if (UI2D_TamanoElem(o, &ew, &eh)) {
            *ew = xformTamOrig[i];
            *eh = xformTamOrig2[i];
        } else {
            ((Texto2D*)o)->tam = xformTamOrig[i];
        }
    }
    xform = 0; xformEje = 0; g_redraw = true;
    NumInputReset(); // idem ConfirmarXform2D
}

// ===== hooks del TRANSFORM UI COMPARTIDO (ver TransformUI.h): mismos mecanismos que el 3D
// y el UV editor (barra de info, entrada numerica, warp, tilde/cruz/X/Y), sin duplicar. =====

// valor EXACTO tipeado (NumInput): mover = v en px de LIENZO (los acumuladores van en px de
// PANTALLA -> se convierte con la escala del encuadre); el lock X/Y filtra en AplicarXform2D.
// Sin lock aplica en los dos ejes (como el translate libre del 3D). Rotar = grados; escalar = factor.
void Editor2D::XformNumValor(float v) {
    if (!xform) return;
    float cx, cy, s; ParamsLienzo(cx, cy, s); if (s <= 0.0f) s = 1.0f;
    if (xform == 1)      { acumX = v * s; acumY = v * s; }
    else if (xform == 2) { acumAng = v * (3.14159265358979f / 180.0f); }
    else                 { acumFactor = v; }
    AplicarXform2D(0, 0); // aplica desde los acumuladores (el lock filtra adentro)
}

// confirmar (Enter / click / tilde): una expresion tipeada INVALIDA no confirma (la barra
// ya muestra "= ?"). Funnel comun de TODOS los caminos de confirmar del Editor 2D.
void Editor2D::XformConfirmar() {
    if (!xform) return;
    if (!TransformUIPuedeConfirmar()) { g_redraw = true; return; }
    ConfirmarXform2D();
}

void Editor2D::XformCancelar() {
    if (xform) CancelarXform2D();
}

// toggle del BLOQUEO de eje (1=X 2=Y): teclas X/Y y botones de la toolbar, UN solo camino.
void Editor2D::XformToggleEje(int eje) {
    if (!xform) return;
    xformEje = (eje == 1) ? ((xformEje == 1) ? 0 : 1) : ((xformEje == 2) ? 0 : 2);
    float v;
    if (NumInputActivo() && NumInputValor(v)) XformNumValor(v); // el valor tipeado, al eje nuevo
    else AplicarXform2D(lastMx, lastMy);
    g_redraw = true;
}

// texto de la barra de INFO (formato compartido con el 3D y el UV; valores en px de lienzo)
std::string Editor2D::XformTextoBarra() {
    float cx, cy, s; ParamsLienzo(cx, cy, s); if (s <= 0.0f) s = 1.0f;
    float dx = acumX / s, dy = acumY / s;          // px de pantalla -> px de LIENZO
    if (xformEje == 1) dy = 0.0f;                  // el lock filtra (mismos valores que se aplican)
    if (xformEje == 2) dx = 0.0f;
    return TransformUITexto2D(xform, dx, dy, acumAng * 57.29578f, acumFactor,
                              xformEje, "X", "Y", " px");
}

void Editor2D::button_left() {
    if (xform) { XformConfirmar(); return; }       // click = confirmar el transform (con gate numerico)
    float mx = (float)(lastMx - x), my = (float)(lastMy - y);   // coords locales
    if (my < (float)BarTopOffset()) return;                     // la barra no es lienzo

    if (LShiftPressed) {
        // Shift+click SOBRE un elemento: sumar/sacar de la seleccion (multi-seleccion).
        // Shift+click en el vacio: colocar el CURSOR 2D (como siempre).
        const float m = 4.0f;
        for (int i = (int)posiciones.size() - 1; i >= 0; i--) {
            const UI2DPos& P = posiciones[i];
            if (mx < P.bx0 - m || mx > P.bx1 + m || my < P.by0 - m || my > P.by1 + m) continue;
            // recortado por el overflow de un ancestro: donde no se VE no se clickea
            if (mx < P.cx0 || mx > P.cx1 || my < P.cy0 || my > P.cy1) continue;
            if (P.obj->select) Deseleccionar2D(P.obj);
            else               P.obj->Seleccionar();
            g_redraw = true;
            return;
        }
        cursor2DX = (mx - lastFx) / lastEsc;
        cursor2DY = (my - lastFy) / lastEsc;
        cursorColocado = true;
        g_redraw = true;
        return;
    }

    // 1) puntos de agarre del elemento ACTIVO con rect (imagen/rectangulo): resize
    { float *ew, *eh;
      if (ObjActivo && UI2D_TamanoElem(ObjActivo, &ew, &eh)) {
        for (size_t i = 0; i < posiciones.size(); i++) {
            if (posiciones[i].obj != ObjActivo) continue;
            const UI2DPos& P = posiciones[i];
            int h = HandleBajoMouse(P.bx0, P.by0, P.bx1, P.by1, mx, my,
                                    *UI2D_Rot2dDe(ObjActivo), P.sx, P.sy);
            if (h >= 0) {
                UndoTransformIniciar();   // Ctrl+Z del resize (confirma al soltar)
                drag = 2; dragHandle = h; dragObj = ObjActivo;
                dragOrigPos = ObjActivo->pos;
                dragOrigW = *ew; dragOrigH = *eh;
                dragObjRefW = (P.refW > 0.0f) ? P.refW : 1.0f;
                dragObjRefH = (P.refH > 0.0f) ? P.refH : 1.0f;
                dragMx = lastMx; dragMy = lastMy;
                acumX = acumY = 0.0f; accionPrimerMov = true;
                return;
            }
            break;
        }
      } }

    // 2) el marco del lienzo en modo RESPONSIVE: arrancar el resize de la "pantalla".
    //    En VISTA DE JUEGO no: sus puntos de agarre no se dibujan (el marco es el viewport) y
    //    agarrarlos a ciegas dejaria el lienzo en un tamano raro.
    { UI* ued = vistaJuego ? NULL : UI2D_UIDelEditor();
      if (ued && !ued->igualQueRender) {
          float lw, lh; TamanoLienzo(&lw, &lh);
          int h = HandleBajoMouse(lastFx, lastFy, lastFx + lw * lastEsc,
                                  lastFy + lh * lastEsc, mx, my);
          if (h >= 0) {
              UndoTransformIniciarObj(ued);   // Ctrl+Z del lienzo (el UI puede no estar seleccionado)
              drag = 3; dragHandle = h; dragObj = NULL;
              dragOrigW = ued->ancho; dragOrigH = ued->alto;
              dragMx = lastMx; dragMy = lastMy;
              acumX = acumY = 0.0f; accionPrimerMov = true;
              return;
          }
      } }

    // 3) click sobre un elemento: seleccionarlo y arrastrarlo (del mas arriba al mas abajo:
    //    los ultimos de posiciones se dibujaron al final, o sea ENCIMA). Si ya estaba
    //    seleccionado NO deselecciona al resto: el arrastre mueve TODA la seleccion.
    const float m = 4.0f;   // mismo margen que el borde de seleccion
    for (int i = (int)posiciones.size() - 1; i >= 0; i--) {
        const UI2DPos& P = posiciones[i];
        if (mx < P.bx0 - m || mx > P.bx1 + m || my < P.by0 - m || my > P.by1 + m) continue;
        // recortado por el overflow de un ancestro: donde no se VE no se clickea (asi
        // tocar fuera de la pantalla deselecciona en vez de agarrar algo invisible)
        if (mx < P.cx0 || mx > P.cx1 || my < P.cy0 || my > P.cy1) continue;
        if (!P.obj->select) DeseleccionarTodo();
        P.obj->Seleccionar();
        drag = 1; dragObj = P.obj;
        dragObjs.clear(); dragOrigs.clear(); dragRefW.clear(); dragRefH.clear();
        for (size_t j = 0; j < posiciones.size(); j++) {
            Object* o = posiciones[j].obj;
            if (o->select && UI2D_EsElemento2D(o) && LayoutDelPadre(o) == 0) {
                dragObjs.push_back(o);
                dragOrigs.push_back(o->pos);
                dragRefW.push_back(posiciones[j].refW > 0.0f ? posiciones[j].refW : 1.0f);
                dragRefH.push_back(posiciones[j].refH > 0.0f ? posiciones[j].refH : 1.0f);
            }
        }
        dragMx = lastMx; dragMy = lastMy;
        dragPaso = false;                       // umbral anti movimiento accidental
        acumX = acumY = 0.0f; accionPrimerMov = true;
        UndoTransformIniciar();   // Ctrl+Z del mover (un click sin arrastre no pushea)
        g_redraw = true;
        return;
    }

    // 4) click en el vacio: deseleccionar (como en el viewport 3D)
    DeseleccionarTodo();
    g_redraw = true;
}

// aplica el arrastre en curso desde los ACUMULADORES (wrap-safe). La posicion se guarda
// RELATIVA al rect de referencia de cada elemento: el delta de pantalla se divide por el.
void Editor2D::AplicarDrag(int mx, int my) {
    (void)mx; (void)my;
    float dx = acumX / lastEsc;                  // en px de lienzo
    float dy = acumY / lastEsc;
    if (drag == 1) {                             // MOVER agarrado desde adentro
        // umbral anti-accidente: hasta ~10 px de pantalla el click NO mueve nada
        if (!dragPaso) {
            float umbral = 10.0f;
            if (acumX * acumX + acumY * acumY < umbral * umbral) return;
            dragPaso = true;
        }
        for (size_t i = 0; i < dragObjs.size(); i++) {
            dragObjs[i]->pos.x = dragOrigs[i].x + acumX / dragRefW[i];
            dragObjs[i]->pos.y = dragOrigs[i].y + acumY / dragRefH[i];
        }
    } else if (drag == 2 && dragObj) {           // RESIZE del elemento por su punto de agarre
        float *ew, *eh;
        if (!UI2D_TamanoElem(dragObj, &ew, &eh)) { UndoTransformCancelar(); drag = 0; return; } // descarta el snapshot pendiente
        bool izq = (dragHandle == 0 || dragHandle == 7 || dragHandle == 6);
        bool der = (dragHandle == 2 || dragHandle == 3 || dragHandle == 4);
        bool arr = (dragHandle == 0 || dragHandle == 1 || dragHandle == 2);
        bool aba = (dragHandle == 4 || dragHandle == 5 || dragHandle == 6);
        // MODIFICADORES: Ctrl mantiene las PROPORCIONES (un cuadrado
        // sigue cuadrado); Shift escala DESDE EL CENTRO (el objeto no se mueve);
        // Ctrl+Shift = las dos cosas. Se leen EN VIVO: cambiar de idea a mitad del
        // arrastre recalcula todo desde el estado original.
        bool ctrl = LCtrlPressed, shift = LShiftPressed;
        // el delta se guarda EN LA UNIDAD del elemento: fraccion del rect del padre,
        // px de lienzo, o px escalados (dividido por el factor de pantalla)
        int tamModo = ((Elemento2D*)dragObj)->tamModo;
        float fesc = UI2D_FactorEscala();
        if (fesc < 0.01f) fesc = 1.0f;
        float dW = (tamModo == TAM2D_FRACCION) ? acumX / dragObjRefW
                 : (tamModo == TAM2D_ESCALADO) ? dx / fesc : dx;
        float dH = (tamModo == TAM2D_FRACCION) ? acumY / dragObjRefH
                 : (tamModo == TAM2D_ESCALADO) ? dy / fesc : dy;
        float minimo = (tamModo == TAM2D_FRACCION) ? 0.005f : 1.0f;
        float dm = shift ? 2.0f : 1.0f;          // desde el centro: el borde sigue al mouse
        float nw = dragOrigW + (der ? dm * dW : 0.0f) - (izq ? dm * dW : 0.0f);
        float nh = dragOrigH + (aba ? dm * dH : 0.0f) - (arr ? dm * dH : 0.0f);
        if (ctrl && dragOrigW > 0.0001f && dragOrigH > 0.0001f) {
            bool enX = (izq || der), enY = (arr || aba);
            if (enX && !enY)      nh = dragOrigH * (nw / dragOrigW);   // borde lateral
            else if (enY && !enX) nw = dragOrigW * (nh / dragOrigH);   // borde arriba/abajo
            else {
                // esquina: manda el eje con MAYOR cambio relativo (escala pareja)
                float fw = nw / dragOrigW, fh = nh / dragOrigH;
                float fac = (fabsf(fw - 1.0f) > fabsf(fh - 1.0f)) ? fw : fh;
                if (fac < 0.01f) fac = 0.01f;
                nw = dragOrigW * fac; nh = dragOrigH * fac;
            }
        }
        if (nw < minimo) nw = minimo;
        if (nh < minimo) nh = minimo;
        *ew = nw; *eh = nh;
        if (shift) {
            // desde el CENTRO: la posicion no se toca
            dragObj->pos.x = dragOrigPos.x;
            dragObj->pos.y = dragOrigPos.y;
        } else {
            // el lado OPUESTO queda quieto: el centro se corre la mitad del cambio
            // EFECTIVO (con Ctrl el tamano real difiere del delta crudo del mouse).
            // k pasa el delta (en la unidad del elemento) a fraccion del rect padre.
            float uniEsc = (tamModo == TAM2D_ESCALADO) ? fesc : 1.0f;
            float kx = (tamModo == TAM2D_FRACCION) ? 1.0f : uniEsc * lastEsc / dragObjRefW;
            float ky = (tamModo == TAM2D_FRACCION) ? 1.0f : uniEsc * lastEsc / dragObjRefH;
            float dwEf = nw - dragOrigW, dhEf = nh - dragOrigH;
            dragObj->pos.x = dragOrigPos.x;
            dragObj->pos.y = dragOrigPos.y;
            if (der)      dragObj->pos.x += dwEf * 0.5f * kx;
            else if (izq) dragObj->pos.x -= dwEf * 0.5f * kx;
            if (aba)      dragObj->pos.y += dhEf * 0.5f * ky;
            else if (arr) dragObj->pos.y -= dhEf * 0.5f * ky;
        }
    } else if (drag == 3) {                      // RESIZE del lienzo (responsive)
        UI* ued = UI2D_UIDelEditor();
        if (!ued || ued->igualQueRender) { UndoTransformCancelar(); drag = 0; return; } // descarta el snapshot pendiente
        bool izq = (dragHandle == 0 || dragHandle == 7 || dragHandle == 6);
        bool der = (dragHandle == 2 || dragHandle == 3 || dragHandle == 4);
        bool arr = (dragHandle == 0 || dragHandle == 1 || dragHandle == 2);
        bool aba = (dragHandle == 4 || dragHandle == 5 || dragHandle == 6);
        // el marco se dibuja CENTRADO: para que el borde siga al mouse el delta vale doble
        float nw = dragOrigW + (der ? 2*dx : 0.0f) - (izq ? 2*dx : 0.0f);
        float nh = dragOrigH + (aba ? 2*dy : 0.0f) - (arr ? 2*dy : 0.0f);
        if (nw < 16.0f) nw = 16.0f;
        if (nh < 16.0f) nh = 16.0f;
        ued->ancho = nw; ued->alto = nh;   // los elementos se re-anclan EN VIVO
    }
    g_redraw = true;
}

void Editor2D::button_right() {
    if (xform) { XformCancelar(); return; }        // click derecho = cancelar G/R/S
    if (drag) UndoTransformCancelar();             // drag cancelado: sin paso de undo
    if (drag == 1) {                               // mover arrastrando: todo vuelve a su lugar
        for (size_t i = 0; i < dragObjs.size(); i++) dragObjs[i]->pos = dragOrigs[i];
        drag = 0; dragObj = NULL; g_redraw = true;
        return;
    }
    if (drag == 2 && dragObj) {                    // resize: rect y posicion originales
        float *ew, *eh;
        if (UI2D_TamanoElem(dragObj, &ew, &eh)) { *ew = dragOrigW; *eh = dragOrigH; }
        dragObj->pos = dragOrigPos;
        drag = 0; dragObj = NULL; g_redraw = true;
        return;
    }
    if (drag == 3) {                               // resize del lienzo: tamano original
        UI* ued = UI2D_UIDelEditor();
        if (ued) { ued->ancho = dragOrigW; ued->alto = dragOrigH; }
        drag = 0; g_redraw = true;
    }
}

// ---- PANEO / ZOOM (mismo manejo que el UV editor) -------------------------------------------

// NAVEGACION CONGELADA en VISTA DE JUEGO: el encuadre lo manda el rect del viewport, asi que
// panear/zoomear se IGNORA (no se pisan zoom/panX/panY: al salir vuelve el encuadre exacto que
// habia). Las acciones EXPLICITAS de vista (zoom 1:1, encuadrar) SI salen del modo, como Blender
// abandona la vista de camara al orbitar.
void Editor2D::Panear(float dx, float dy) {
    if (vistaJuego) return;
    panX += dx; panY += dy;
    g_redraw = true;
}

// zoom CENTRADO en el viewport (teclado/pinch): el centro de pantalla no se mueve.
void Editor2D::ZoomCentro(int dir) {
    if (vistaJuego) return;
    float f = (dir > 0) ? 1.05f : (1.0f / 1.05f);
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    f = nz / zoom;
    zoom = nz; panX *= f; panY *= f;
    g_redraw = true;
}

void Editor2D::event_mouse_motion(int mx, int my) {
    if (drag || xform) {
        // tipeando un valor exacto (NumInput): el mouse no interfiere (como el 3D)
        if (xform && !drag && NumInputActivo()) { lastMx = mx; lastMy = my; return; }
        // acumular el delta, SALTEANDO los saltos del warp (el cursor se envuelve en el
        // borde, como en el 3D: la accion sigue fluida en vez de reiniciarse)
        int jx = mx - lastMx, jy = my - lastMy;
        bool salto = (jx > width / 2 || jx < -width / 2 || jy > height / 2 || jy < -height / 2);
        if (accionPrimerMov) { accionPrimerMov = false; salto = true; }   // arranque en cero
        if (!salto) {
            acumX += (float)jx; acumY += (float)jy;
            if (xform == 2) {              // angulo INCREMENTAL alrededor del pivote
                float a0 = atan2f((float)(lastMy - y) - xformPivY, (float)(lastMx - x) - xformPivX);
                float a1 = atan2f((float)(my - y) - xformPivY,     (float)(mx - x) - xformPivX);
                float da = a1 - a0;
                while (da >  3.14159265f) da -= 6.2831853f;
                while (da < -3.14159265f) da += 6.2831853f;
                acumAng += da;
            } else if (xform == 3) {       // factor INCREMENTAL (distancia al pivote)
                float dx0 = (float)(lastMx - x) - xformPivX, dy0 = (float)(lastMy - y) - xformPivY;
                float dx1 = (float)(mx - x) - xformPivX,     dy1 = (float)(my - y) - xformPivY;
                float d0 = sqrtf(dx0 * dx0 + dy0 * dy0);
                float d1 = sqrtf(dx1 * dx1 + dy1 * dy1);
                if (d0 > 1.0f) acumFactor *= d1 / d0;
            }
        }
        if (drag) AplicarDrag(mx, my);
        else      AplicarXform2D(mx, my);
    } else if (middleMouseDown && !vistaJuego) {   // boton del medio: paneo (congelado en vista de juego)
        // SALTEAR el salto del warp (igual que drag/xform arriba): al llegar al borde el
        // cursor se envuelve al lado opuesto (CheckWarpMouseInViewport) y mx/my pegan un
        // salto del ancho/alto del viewport. Sin este filtro ese delta gigante entraba al
        // paneo y lo devolvia todo para atras; con el, el paneo sigue fluido en la misma
        // direccion despues del teleport.
        int jx = mx - lastMx, jy = my - lastMy;
        bool salto = (jx > width / 2 || jx < -width / 2 || jy > height / 2 || jy < -height / 2);
        if (!salto) {
            panX += (float)jx;
            panY += (float)jy;
        }
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
    if (vistaJuego) return;                  // navegacion congelada (ver Panear)
    // zoom CONTINUO, proporcional a cuanto se abren/cierran los dedos (igual que
    // el Viewport3D). El umbral discreto de antes (ZoomCentro +-1 recien al
    // superar 1.0) descartaba casi todo el gesto: el pinch se sentia trabado.
    if (zoomDelta != 0.0f) {
        float f = expf(zoomDelta * 0.06f);       // la MISMA sensibilidad que el 3D
        if (f < 0.5f) f = 0.5f;                  // un frame nunca pega un salto brutal
        if (f > 2.0f) f = 2.0f;
        float nz = zoom * f;
        if (nz < 0.05f) nz = 0.05f;
        if (nz > 50.0f) nz = 50.0f;
        f = nz / zoom;
        zoom = nz; panX *= f; panY *= f;         // centrado en el viewport (como ZoomCentro)
        g_redraw = true;
    }
    if (panDx != 0.0f || panDy != 0.0f) Panear(panDx, panDy);
}

// VISTA DE JUEGO on/off (numpad 0 y menu View > "Game View"). Es POR VIEWPORT. No toca
// zoom/panX/panY: el encuadre de edicion queda intacto y al salir vuelve tal cual estaba.
// Fuera del #ifndef: en Symbian no hay teclado de PC, pero la entrada del menu SI existe.
void Editor2DVistaJuego(Editor2D* ed, bool on) {
    if (!ed || ed->vistaJuego == on) return;
    ed->vistaJuego = on;
    // el override del lienzo entra/sale JUNTO con el modo (no esperar al proximo Render:
    // el input y los binds tienen que ver el lienzo nuevo ya mismo)
    ed->ActualizarOverrideLienzo();
    g_redraw = true;
}

// SimJuego alimenta input por este viewport: re-fijar SU override antes del mapeo (ver Editor2D.h)
void Editor2DAlimentarOverride(ViewportBase* vp) {
    if (vp && vp->ViewportKind() == 6) ((Editor2D*)vp)->ActualizarOverrideLienzo();
}

#ifndef W3D_SYMBIAN
// G = mover, R = rotar; X/Y restringen el eje; ESC cancela, ENTER confirma; flechas = paneo
void Editor2D::event_key_down(int tecla, bool repeticion) {
    (void)repeticion;
    if (PopUpActive) return;               // un popup modal abierto tiene prioridad
    if (xform) {                           // dentro de un transform (via los hooks compartidos)
        // (el BACKSPACE ya no cancela: durante un transform edita la entrada numerica, como
        // en el 3D - lo consume NumInputChar en controles.cpp. Cancelar = Esc / click derecho.)
        if (tecla == W3dK_ESCAPE) { XformCancelar(); return; }
        if (tecla == W3dK_RETURN || tecla == W3dK_KP_ENTER) { XformConfirmar(); return; }
        if (tecla == W3dK_X) { XformToggleEje(1); return; }
        if (tecla == W3dK_Y) { XformToggleEje(2); return; }
        return;
    }
    // VISTA DE JUEGO: numpad 0, igual que la vista de camara del 3D (y de Blender). Va DESPUES
    // del bloque de xform para no romper un G/R/S en curso. Esc tambien sale (sin xform activo:
    // ese caso ya se atendio arriba), asi el modo nunca se siente "atrapado".
    if (tecla == W3dK_KP_0) { Editor2DVistaJuego(this, !vistaJuego); return; }
    if (tecla == W3dK_ESCAPE && vistaJuego && !drag) { Editor2DVistaJuego(this, false); return; }
    if (tecla == W3dK_G) { IniciarXform2D(1); return; }   // mover (como en el 3D)
    if (tecla == W3dK_R) { IniciarXform2D(2); return; }   // rotar alrededor del pivote
    if (tecla == W3dK_S) { IniciarXform2D(3); return; }   // escalar desde el pivote
    if (tecla == W3dK_D && LShiftPressed) { Editor2DDuplicarSeleccion(this); return; }  // Shift+D
    if (tecla == W3dK_X || tecla == W3dK_DELETE) { Editor2DBorrarSeleccion(); return; }  // X / Supr = borrar
    if (tecla == W3dK_KP_PERIOD) { Editor2DEncuadrarSeleccion(this); return; }  // encuadrar
    if (tecla == W3dK_A && LAltPressed)  { Editor2DSeleccionar(1); return; }  // Alt+A = nada
    if (tecla == W3dK_A)                 { Editor2DSeleccionar(0); return; }  // A = todo (2D)
    if (tecla == W3dK_I && LCtrlPressed) { Editor2DSeleccionar(2); return; }  // Ctrl+I = invertir
    const float pp = (float)GlobalScale * 16.0f;
    if (tecla == W3dK_LEFT)  { Panear(+pp, 0); return; }
    if (tecla == W3dK_RIGHT) { Panear(-pp, 0); return; }
    if (tecla == W3dK_UP)    { Panear(0, +pp); return; }
    if (tecla == W3dK_DOWN)  { Panear(0, -pp); return; }
}

void Editor2D::event_mouse_wheel(float dy, int mx, int my) {
    if (PopUpActive) return;
    if (OnToolbar(mx, my)) { ToolbarScrollBy((int)(dy * 40)); return; } // toolbar de abajo: scroll horizontal
    if (vistaJuego) return;                // navegacion congelada (ver Panear)
    float f = (dy > 0) ? 1.1f : (1.0f / 1.1f);
    float nz = zoom * f;
    if (nz < 0.05f) nz = 0.05f;
    if (nz > 50.0f) nz = 50.0f;
    // IMAN a los enteros: pasando cerca de x1/x2/x3 el zoom se clava ahi y la fuente
    // pixel y los bordes quedan exactos (a escalas no enteras se ven masticados)
    { float ent = (float)(int)(nz + 0.5f);
      if (ent >= 1.0f && nz > ent * 0.94f && nz < ent * 1.06f) nz = ent; }
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

// (el chequeo "hay un transform de teclado activo" es ahora el hook compartido XformEnCurso
//  de ViewportBase: controles.cpp lo usa para envolver el cursor en el Editor 2D y el UV editor)

// junta TODOS los elementos 2D de la escena (recursivo, arboles de los UI)
static void Juntar2D(Object* o, std::vector<Object*>& v) {
    if (!o) return;
    if (UI2D_EsElemento2D(o)) v.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) Juntar2D(o->Childrens[i], v);
}
// ZOOM exacto xN: un px del lienzo = N px de pantalla (pixel-perfect) y la vista centrada
void Editor2DZoomExacto(Editor2D* ed, int n) {
    if (!ed || n < 1) return;
    Editor2DVistaJuego(ed, false);   // pedir un zoom explicito SALE de la vista de juego
    ed->zoom = (float)n;
    ed->panX = 0.0f;
    ed->panY = 0.0f;
    ed->zoomInicializado = true;
    g_redraw = true;
}
void Editor2DZoom1a1(Editor2D* ed) { Editor2DZoomExacto(ed, 1); }

// ENCUADRAR la seleccion (numpad . / View > Frame Selected): zoom y paneo para que los
// elementos seleccionados llenen comodos la vista (con iman a los zooms enteros)
void Editor2DEncuadrarSeleccion(Editor2D* ed) {
    if (!ed) return;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    bool hay = false;
    for (size_t i = 0; i < ed->posiciones.size(); i++) {
        const UI2DPos& P = ed->posiciones[i];
        if (!P.obj->select) continue;
        if (P.bx0 < x0) x0 = P.bx0;
        if (P.by0 < y0) y0 = P.by0;
        if (P.bx1 > x1) x1 = P.bx1;
        if (P.by1 > y1) y1 = P.by1;
        hay = true;
    }
    if (!hay || ed->lastEsc <= 0.0001f) return;
    // encuadrar es una accion EXPLICITA de vista: sale de la vista de juego (como Blender
    // abandona la vista de camara). Los lastFx/lastEsc del ultimo render siguen siendo los
    // buenos para leer las posiciones de ESTE frame, asi que se convierte antes de cambiar.
    Editor2DVistaJuego(ed, false);
    // pantalla -> lienzo (con el estado del ultimo render)
    float cx0 = (x0 - ed->lastFx) / ed->lastEsc, cy0 = (y0 - ed->lastFy) / ed->lastEsc;
    float cx1 = (x1 - ed->lastFx) / ed->lastEsc, cy1 = (y1 - ed->lastFy) / ed->lastEsc;
    float selW = cx1 - cx0; if (selW < 1.0f) selW = 1.0f;
    float selH = cy1 - cy0; if (selH < 1.0f) selH = 1.0f;
    int top = ed->BarTopOffset();
    float ch = (float)(ed->height - top); if (ch < 1.0f) ch = 1.0f;
    float z = (ed->width * 0.8f) / selW;
    float zy = (ch * 0.8f) / selH;
    if (zy < z) z = zy;
    if (z < 0.05f) z = 0.05f;
    if (z > 50.0f) z = 50.0f;
    { float ent = (float)(int)(z + 0.5f);   // iman a los enteros: pixel-perfect si se puede
      if (ent >= 1.0f && z > ent * 0.80f && z < ent * 1.25f) z = ent; }
    float lw, lh; UI2D_TamanoLienzo(&lw, &lh);
    ed->zoom = z;
    ed->panX = z * (lw * 0.5f - (cx0 + cx1) * 0.5f);
    ed->panY = z * (lh * 0.5f - (cy0 + cy1) * 0.5f);
    ed->zoomInicializado = true;
    g_redraw = true;
}

// Seleccionar del Editor 2D (solo elementos 2D): 0 = todo, 1 = nada, 2 = invertir
void Editor2DSeleccionar(int modo) {
    if (modo == 1) { DeseleccionarTodo(); g_redraw = true; return; }
    std::vector<Object*> v;
    if (SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
            Object* o = SceneCollection->Childrens[i];
            if (o && o->getType() == ObjectType::ui)
                for (size_t c = 0; c < o->Childrens.size(); c++) Juntar2D(o->Childrens[c], v);
        }
    for (size_t i = 0; i < v.size(); i++) {
        if (modo == 0)            v[i]->Seleccionar();
        else if (v[i]->select)    v[i]->Deseleccionar();
        else                      v[i]->Seleccionar();
    }
    g_redraw = true;
}

// duplica la seleccion 2D: copias reales corridas +20,+20 y seleccionadas (Shift+D / menu Objeto)
void Editor2DDuplicarSeleccion(Editor2D* ed) {
    (void)ed;
    std::vector<Object*> sel;
    for (size_t i = 0; i < ObjSelects.size(); i++)
        if (UI2D_EsElemento2D(ObjSelects[i])) sel.push_back(ObjSelects[i]);
    if (sel.empty()) return;
    std::vector<Object*> copias;
    for (size_t i = 0; i < sel.size(); i++) {
        Object* d = W3dDuplicarUno(sel[i]);
        if (d) copias.push_back(d);
    }
    if (copias.empty()) return;
    DeseleccionarTodo();
    for (size_t i = 0; i < copias.size(); i++) copias[i]->Seleccionar();
    // como Blender: la copia queda AGARRADA para moverla (click/Enter aceptan;
    // click derecho/Esc/Backspace cancelan y queda donde estaba el original).
    // El mover arranca en el PROXIMO render: recien ahi las copias tienen posicion resuelta.
    if (ed) ed->moverTrasDuplicar = true;
    g_redraw = true;
}

// borra la seleccion con el popup de confirmacion de siempre (si hay algo 2D elegido)
void Editor2DBorrarSeleccion() {
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (UI2D_EsElemento2D(o) || o->getType() == ObjectType::ui) {
            AbrirConfirmarBorrado();
            return;
        }
    }
}

void Editor2D::mouse_button_up(int boton) {
    (void)boton;
    if (drag) UndoTransformConfirmar();   // Ctrl+Z: cierra el paso (no-op si no se movio)
    drag = 0; dragHandle = -1; dragObj = NULL;   // fin del arrastre
    ViewPortClickDown = false;
    g_redraw = true;
}
#endif
