#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render/OpcionesRender.h"   // RenderType / g_redraw: son del editor
#include "ViewPorts/ViewPorts.h"
#include <math.h>

// -----------------------------
// Variables globales
// -----------------------------
ViewportBase* viewPortActive = NULL;
// (leftMouseDown/ViewPortClickDown: variables.cpp, ahora compartido)

// redondeo portable (std::round es C++11)
static float w3dRound(float v) {
    return (float)floor(v + 0.5f);
}
ViewportBase* rootViewport = NULL;

// -----------------------------
// ViewportBase
// -----------------------------
bool ViewportBase::Contains(int mx, int my) const {
    return (mx >= x && mx < x + width && my >= y && my < y + height);
}

bool ViewportBase::isLeaf() const { return true; }

void ViewportBase::Resize(int newW, int newH) {
    width = newW;
    height = newH;
}

ViewportBase::ViewportBase()
    : x(0), y(0), width(0), height(0) {
    // barra de botones (C++03: en el ctor)
    barAbajo = false;
    barAlpha = 1.0f;
    barFocusIndex = -1;
    barScrollX = 0;
    barScrollManual = 0;
    barCard = NULL;
    barLinea = NULL;
    toolScroll = 0;    // toolbar compartida (ToolbarBase.cpp): sin scroll ni gesto al nacer
    toolGesto = false;
}
ViewportBase::~ViewportBase(){}
void ViewportBase::event_mouse_motion(int mx, int my) {}
void ViewportBase::button_left() {}
void ViewportBase::button_right(){};
void ViewportBase::button_up(){};
void ViewportBase::button_down(){};
// base: no-op. El viewport que quiera teclas/rueda las overridea (hoy: Timeline en las dos plataformas; el resto
// todavia solo en PC). Asi el ruteo puede mandarle a cualquiera sin preguntar quien es.
void ViewportBase::event_key_down(int tecla, bool repeticion) {}
void ViewportBase::event_key_up(int tecla) {}
void ViewportBase::event_mouse_wheel(float dy, int mx, int my) {}
void ViewportBase::mouse_button_up(int boton) {}
void ViewportBase::event_finger_gesture(float zoomDelta, float panDx, float panDy){} // base: no-op (los paneles no zoomean/panean)
bool ViewportBase::event_finger_scroll(int px, int py, int dx, int dy){ return false; } // base: no scrollea (el viewport 3D orbita)

// scroll HORIZONTAL de la barra superior (botones + pestañas), unificado rueda/touch en todos los viewports.
extern bool g_redraw;
bool ViewportBase::BarScrollHorizontal(int px, int py, int delta){
    if (!barCard || (BarButtons.empty() && BarTabs.empty())) return false; // no hay barra
    int barH = BarHeight();
    int yBar = barAbajo ? (y + height - barH) : y;
    if (px < x || px >= x + width || py < yBar || py >= yBar + barH) return false; // el evento no cae en la barra
    barScrollManual -= delta; // delta>0 (rueda arriba / dedo a la derecha) = mostrar lo de la izquierda
    if (barScrollManual < 0) barScrollManual = 0;
    ActualizarBarra(); // re-clampea contra el ancho total (deja el maximo si te pasas)
    g_redraw = true;
    return true;
}

// (px,py) cae en la barra superior O en la toolbar de abajo? El gesto de arrastre queda lockeado
// a la barra que se toco (toolGesto) para que BarScrollBy scrollee la correcta. Antes esto era un
// override del Viewport3D (el unico con toolbar); ahora la toolbar es de la base -> vale para todos.
bool ViewportBase::OnBar(int px, int py){
    if (barCard && !(BarButtons.empty() && BarTabs.empty())){
        int barH = BarHeight();
        int yBar = barAbajo ? (y + height - barH) : y;
        if (px >= x && px < x + width && py >= yBar && py < yBar + barH){
            toolGesto = false;
            return true;
        }
    }
    if (OnToolbar(px, py)){ toolGesto = true; return true; }
    return false;
}

// scroll horizontal de la barra SIN hit-test: para cuando el gesto de scroll YA arranco sobre la barra
// y hay que seguir aunque el dedo se salga de ella (gesto lockeado). toolGesto decide CUAL barra.
void ViewportBase::BarScrollBy(int delta){
    if (toolGesto){ ToolbarScrollBy(delta); return; }
    if (!barCard || (BarButtons.empty() && BarTabs.empty())) return;
    barScrollManual -= delta;
    if (barScrollManual < 0) barScrollManual = 0;
    ActualizarBarra();
    g_redraw = true;
}

// ------------------ Constructor / Destructor ------------------
ViewportRow::ViewportRow(ViewportBase* a, ViewportBase* b, float frac)
    : childA(a), childB(b), splitFrac(frac) {}

ViewportRow::~ViewportRow() {
    //delete childA;
    delete childB;
}

// ------------------ Leaf check ------------------
bool ViewportRow::isLeaf() const {
    return !childA && !childB;
}

// ------------------ Resize children ------------------
void ViewportRow::SetSizeChildrens(int move) {
    if (!childA || !childB) return;  // validar antes

    // CLAMP en vez de RECHAZAR: antes, si un lado YA estaba por debajo del minimo
    // (split degenerado restaurado de un archivo), cualquier move se rechazaba y el
    // divisor quedaba muerto (no se podia ni agrandar el lado chico). Ahora el move
    // se recorta al rango legal: el primer drag lleva el lado chico al minimo y de
    // ahi en mas se agranda/achica normal.
    int lo = MinViewportWidthGS - childA->width;   // move minimo: childA queda en el minimo
    int hi = childB->width - MinViewportWidthGS;   // move maximo: childB queda en el minimo
    if (lo > hi) return;   // el padre no da para los dos minimos: no mover
    if (move < lo) move = lo;
    if (move > hi) move = hi;
    if (move == 0) return;

    childA->width += move;
    childA->Resize(childA->width, childA->height);

    childB->x += move;
    childB->width -= move;
    childB->Resize(childB->width, childB->height);

    splitFrac = static_cast<float>(childA->width) / static_cast<float>(width);
}

// ------------------ Resize ------------------
void ViewportRow::Resize(int newW, int newH) {
    width = newW;
    height = newH;

    if (isLeaf()) return;

    if (splitFrac < 0.0f) splitFrac = 0.0f;
    if (splitFrac > 1.0f) splitFrac = 1.0f;

    // CLAMP del split al MINIMO de viewport: un split degenerado (layout guardado
    // con otro tamano de ventana, o un valor roto en el archivo) dejaba un hijo mas
    // chico que el minimo, con el divisor inutilizable. Resize es el embudo comun
    // (BuildLayout, resize de ventana, expand): aca se sanea SIEMPRE. Si el padre
    // no da para los dos minimos, se reparte a la mitad.
    if (width > 0) {
        float fmin = (float)MinViewportWidthGS / (float)width;
        if (fmin <= 0.5f) {
            if (splitFrac < fmin)        splitFrac = fmin;
            if (splitFrac > 1.0f - fmin) splitFrac = 1.0f - fmin;
        } else {
            splitFrac = 0.5f;
        }
    }

    int wA = static_cast<int>(w3dRound(width * splitFrac));
    int wB = width - wA;

    if (childA) {
        childA->x = x;
        childA->y = y;
        childA->width = wA;
        childA->height = height;
        childA->Resize(wA, height);
    }
    if (childB) {
        childB->x = x + wA;
        childB->y = y;
        childB->width = wB;
        childB->height = height;
        childB->Resize(wB, height);
    }
}

// ------------------ Render ------------------
void ViewportRow::Render() {
    if (isLeaf()) return;

    if (childA) childA->Render();
    if (childB) childB->Render();
}

// ------------------ Input ------------------
void ViewportRow::button_left() {
    leftMouseDown = true;
    ViewPortClickDown = true;
}

void ViewportRow::mouse_button_up(int boton){
    ViewPortClickDown = false;
}

void ViewportRow::event_mouse_motion(int mx, int my) {
    if (leftMouseDown) {
        ViewPortClickDown = true;
        SetSizeChildrens(dx); // dx debe definirse en el contexto de movimiento
    }
}

// ------------------ Leaf check ------------------
ViewportColumn::ViewportColumn(ViewportBase* a, ViewportBase* b, float frac)
    : childA(a), childB(b), splitFrac(frac) {
}

bool ViewportColumn::isLeaf() const {
    return !childA && !childB;
}

// ------------------ Resize children vertical ------------------
void ViewportColumn::SetSizeChildrens(int move) {
    if (!childA || !childB) return;  // validar antes

    // arriba-izquierda: move > 0 = el divisor BAJA = crece childA (arriba)
    // CLAMP en vez de RECHAZAR (mismo fix que ViewportRow): con un lado por debajo
    // del minimo el divisor quedaba muerto; ahora el move se recorta al rango legal
    // y el primer drag saca al lado chico del estado degenerado.
    int lo = MinViewportHeightGS - childA->height;   // move minimo: childA queda en el minimo
    int hi = childB->height - MinViewportHeightGS;   // move maximo: childB queda en el minimo
    if (lo > hi) return;   // el padre no da para los dos minimos: no mover
    if (move < lo) move = lo;
    if (move > hi) move = hi;
    if (move == 0) return;

    childA->height += move;
    childA->Resize(childA->width, childA->height);

    childB->y += move;
    childB->height -= move;
    childB->Resize(childB->width, childB->height);

    splitFrac = static_cast<float>(childA->height) / static_cast<float>(height);
}

// ------------------ Resize ------------------
void ViewportColumn::Resize(int newW, int newH) {
    width = newW;
    height = newH;

    if (isLeaf()) return;

    if (splitFrac < 0.0f) splitFrac = 0.0f;
    if (splitFrac > 1.0f) splitFrac = 1.0f;

    // CLAMP del split al MINIMO de viewport (mismo saneo que ViewportRow::Resize):
    // ningun hijo puede quedar mas chico que el alto minimo al reconstruir un
    // layout guardado ni al redimensionar la ventana.
    if (height > 0) {
        float fmin = (float)MinViewportHeightGS / (float)height;
        if (fmin <= 0.5f) {
            if (splitFrac < fmin)        splitFrac = fmin;
            if (splitFrac > 1.0f - fmin) splitFrac = 1.0f - fmin;
        } else {
            splitFrac = 0.5f;
        }
    }

    int hA = static_cast<int>(w3dRound(height * splitFrac));
    int hB = height - hA;

    if (childA) {
        childA->x = x;
        childA->y = y;
        childA->width  = width;
        childA->height = hA;
        childA->Resize(width, hA);
    }
    if (childB) {
        childB->x = x;
        childB->y = y + hA;
        childB->width  = width;
        childB->height = hB;
        childB->Resize(width, hB);
    }
}

// ------------------ Render ------------------
void ViewportColumn::Render() {
    if (isLeaf()) return;

    if (childA) childA->Render();
    if (childB) childB->Render();
}

// ------------------ Input ------------------
void ViewportColumn::button_left() {
    leftMouseDown = true;
    ViewPortClickDown = true;
}

void ViewportColumn::mouse_button_up(int boton){
    ViewPortClickDown = false;
}

void ViewportColumn::event_mouse_motion(int mx, int my) {
    if (leftMouseDown) {
        ViewPortClickDown = true;
        SetSizeChildrens(dy); // dy debe definirse en el contexto de movimiento
    }
}

ViewportColumn::~ViewportColumn() {
    //delete childA;
    delete childB;
}

// helpers C++03 (eran lambdas)
static bool VpIsInside(ViewportBase* v, int mx, int my) {
    return v != NULL && mx >= v->x && mx < v->x + v->width &&
           my >= v->y && my < v->y + v->height;
}

static bool VpIsInPadding(ViewportBase* a, ViewportBase* b, bool isRow,
                          int mx, int my, int PADDING) {
    if (!a || !b) return false;
    if (isRow) {
        int splitX = a->x + a->width;
        if (mx >= splitX - PADDING && mx < splitX + PADDING &&
            my >= a->y && my < a->y + a->height)
        {
#ifndef W3D_SYMBIAN
            SDL_SetCursor(cursorScaleHorizontal);
#endif
            return true;
        }
    } else {
        int splitY = a->y + a->height;
        if (my >= splitY - PADDING && my < splitY + PADDING &&
            mx >= a->x && mx < a->x + a->width)
        {
#ifndef W3D_SYMBIAN
            SDL_SetCursor(cursorScaleVertical);
#endif
            return true;
        }
    }
#ifndef W3D_SYMBIAN
    SDL_SetCursor(cursorDefault);
#endif
    return false;
}

ViewportBase* FindViewportUnderMouse(ViewportBase* vp, int mx, int my) {
    if (!vp) return NULL;

    const int PADDING = paddingViewportGS;

    // -----------------------------
    // ViewportRow (divide en columnas)
    // -----------------------------
    if (vp->ContainerKind() == 1) {
        ViewportRow* row = (ViewportRow*)vp;
        if (VpIsInPadding(row->childA, row->childB, true, mx, my, PADDING))
            return vp;

        if (VpIsInside(row->childA, mx, my))
            return FindViewportUnderMouse(row->childA, mx, my);

        if (VpIsInside(row->childB, mx, my))
            return FindViewportUnderMouse(row->childB, mx, my);
    }
    // -----------------------------
    // ViewportColumn (divide en filas)
    // -----------------------------
    else if (vp->ContainerKind() == 2) {
        ViewportColumn* col = (ViewportColumn*)vp;
        if (VpIsInPadding(col->childA, col->childB, false, mx, my, PADDING))
            return vp;

        if (VpIsInside(col->childA, mx, my))
            return FindViewportUnderMouse(col->childA, mx, my);

        if (VpIsInside(col->childB, mx, my))
            return FindViewportUnderMouse(col->childB, mx, my);
    }
    // -----------------------------
    // Viewport final (sin hijos)
    // -----------------------------
    else if (vp->Contains(mx, my)) {
        if (mx <= vp->x + PADDING || mx >= vp->x + vp->width - PADDING ||
            my <= vp->y + PADDING || my >= vp->y + vp->height - PADDING)
        {
            return NULL;
        }
        return vp;
    }

    return NULL;
}

void SetGlobalScale(int scale){
    GlobalScale = scale;
    marginGS = margin * scale;
    paddingGS = padding * scale;
    gapGS = gap * scale;
    RenglonHeightGS = RenglonHeight * scale;
    borderGS = border * scale;
    bordersGS = borderGS * 2;
    LetterWidthGS = LetterWidth * scale;
    LetterHeightGS = LetterHeight * scale;
    paddingViewportGS = paddingViewport * scale;
    // ALTO MINIMO de un viewport = EXACTAMENTE el alto de la BARRA de menu (mismo calculo que BarHeight(), con el
    // separador de 1px de las barras que no son del 3D). Asi se puede achicar un viewport hasta que queden SOLO sus
    // controles: p.ej. dejar el timeline como una tira con el transporte y nada mas. Antes MinViewportHeight era un
    // valor fijo mas grande y siempre sobraba un hueco inutil debajo de la barra.
    MinViewportHeightGS = UIBotonAltura() + UIBarPadding() * 2 + scale * 2;
    MinViewportWidthGS = MinViewportWidth * scale;
    CharacterWidthGS = CharacterWidth * scale;

#ifndef W3D_SYMBIAN
    // (iconos y fuente de PC: pendientes de portar)
    SetIconScale(scale);
    WhiskFont->SetScale(scale);
#endif

    /*for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        SceneCollection->Childrens[i]->name->scaleX = scale;
        SceneCollection->Childrens[i]->name->scaleY = scale;
        SceneCollection->Childrens[i]->name->UpdateCache();
    }*/
}

#ifndef W3D_SYMBIAN
void CheckWarpMouseInViewport(int mx, int my, const ViewportBase* vp) {
    if (!vp) return;

    int vx = vp->x;
    int vy = vp->y;
    int vw = vp->width;
    int vh = vp->height;

    int left   = vx + borderGS;
    int right  = vx + vw - borderGS;
    int top    = vy + borderGS;            // arbol arriba-izq: sin flip
    int bottom = vy + vh - borderGS;

    //std::cout << "winW: " << winW << " winH: " << winH << std::endl;
    //std::cout << "top: " << top << " bottom: " << bottom << std::endl;
    //std::cout << "my: " << my << std::endl;

    bool warped = false;

    // Wrap horizontal
    if (mx <= left) {
        mx = right - 1;
        warped = true;
    } else if (mx >= right) {
        mx = left + 1;
        warped = true;
    }

    // Wrap vertical
    if (my <= top) {
        my = bottom - 1;
        warped = true;
    }
    else if (my >= bottom) {
        my = top + 1;
        warped = true;
    }

    if (warped) {
        SDL_WarpMouseInWindow(window, mx, my);
        dx = 0;
        dy = 0;
    } else {
        dx = mx - lastMouseX;
        dy = my - lastMouseY;
    }

    lastMouseX = mx;
    lastMouseY = my;
}
#endif
// ============== barra de botones de los viewports ==============

void ViewportBase::BarCrear(){
    barCard = new Card(NULL, 10, 10);
    barLinea = new Rec2D();
    // todos los viewports llevan SIEMPRE primero el boton de icono:
    // dice que tipo de viewport es (a futuro va a servir para cambiarlo)
    BarButtons.push_back(new Button("", IconType::arrow, true));
}

int ViewportBase::BarTopOffset() const {
    if (!barCard || barAbajo) return 0;
    return BarHeight();
}

int ViewportBase::BarHeight() const {
    // boton + padding abajo + 1px MAS de margen arriba (los botones quedan un
    // toque mas abajo). Las barras que NO son del 3D llevan ademas una linea
    // separadora de 1px en el borde inferior.
    int h = UIBotonAltura() + UIBarPadding() * 2 + GlobalScale;
    if (ViewportKind() != 1 && barLinea) h += GlobalScale; // separador (1px)
    return h;
}

// recalcula anchos, auto-scroll (centra el menu enfocado) y los sx/sy
// absolutos de botones/pestañas. SIN GL: la usa RenderBar y tambien el ruteo
// de teclado antes de abrir un menu (para que el hit-test acierte la posicion
// YA scrolleada). El foco solo vale con un menu abierto.
void ViewportBase::ActualizarBarra(){
    if (!barCard || BarButtons.empty()) return;
    int barH = BarHeight();
    int yBar = barAbajo ? height - barH : 0;
    int btnGap = 2 * GlobalScale; // gap CHICO entre botones (antes gapGS) p/ que entren mas
    // NO se resetea barFocusIndex aca: el ruteo de teclado la llama ANTES de
    // abrir el menu (ahi todavia no hay menu abierto). El reset lo hace RenderBar.

    // medir anchos + posicion del enfocado
    int total = gapGS, focoX = -1, focoW = 0;
    for (size_t b = 0; b < BarButtons.size(); b++){
        Button* btn = BarButtons[b];
        if (!btn->visible) continue;
        btn->Resize(width - gapGS * 2);
        if ((int)b == barFocusIndex){ focoX = total; focoW = btn->width; }
        total += btn->width + btnGap;
    }
    // las PESTAÑAS tambien cuentan para el ancho total (sino el scroll maximo salia mal: en propiedades hay
    // 1 boton + varias pestañas -> maxS quedaba ~0 y las pestañas NO scrolleaban aunque se salieran de pantalla).
    for (size_t t = 0; t < BarTabs.size(); t++){
        Tab* tab = BarTabs[t];
        if (!tab->visible) continue;
        tab->Resize(width - gapGS * 2);
        total += tab->width + GlobalScale;
    }
    int maxS = total - width; if (maxS < 0) maxS = 0; // sin hueco a la derecha
    if (focoX >= 0){
        barScrollX = focoX + focoW / 2 - width / 2; // teclado: centrar el enfocado
    } else {
        barScrollX = barScrollManual; // PC sin foco: scroll por rueda del mouse
    }
    if (barScrollX > maxS) barScrollX = maxS;
    if (barScrollX < 0) barScrollX = 0;
    barScrollManual = barScrollX; // dejar el manual ya clampeado

    // sx/sy absolutos (post-scroll). Lo que se sale lo recorta el ortho local.
    int bx = gapGS - barScrollX;
    for (size_t b = 0; b < BarButtons.size(); b++){
        Button* btn = BarButtons[b];
        if (!btn->visible){ btn->sx = -10000; btn->sy = -10000; btn->hover = false; continue; } // hover apagado (como la toolbar)
        int by = barAbajo ? (barH - UIBarPadding() - btn->height)
                          : (UIBarPadding() + GlobalScale);
        if (by < 0) by = 0;
        // con un menu abierto, el ENFOCADO se resalta verde y los demas no
        btn->focoMenu = (barFocusIndex >= 0 && (int)b == barFocusIndex);
        btn->sx = x + bx;
        btn->sy = y + yBar + by;
        bx += btn->width + btnGap;
    }
    for (size_t t = 0; t < BarTabs.size(); t++){
        Tab* tab = BarTabs[t];
        if (!tab->visible){ tab->sx = -10000; tab->sy = -10000; continue; }
        tab->Resize(width - gapGS * 2);
        int ty = barAbajo ? 0 : barH - tab->height;
        tab->sx = x + bx;
        tab->sy = y + yBar + ty;
        bx += tab->width + GlobalScale;
    }
}

// hook opcional (lo setea la plataforma): texto del modo de entrada de texto a mostrar en la barra del viewport
// ACTIVO mientras se edita un campo (Symbian lo apunta a W3dT9ModoTexto: "Abc"/"abc"/"ABC"/"123"). NULL en PC.
const char* (*g_ViewportBarModoHook)() = 0;

void ViewportBase::RenderBar(){
    // se llama al final del Render del viewport (con su ortho 2D activo)
    if (!barCard || BarButtons.empty()) return;
    // sin ningun menu abierto el foco se apaga (la barra vuelve a la normalidad). EXCEPCION: el Timeline (kind 5)
    // navega sus botones de transporte SIN abrir menu -> su foco lo maneja el ruteo de teclado (LayoutTimelineBar*),
    // no se apaga aca (sino se perderia cada frame).
    extern bool LayoutMenuAbierto();
    if (!LayoutMenuAbierto() && ViewportKind() != 5 && ViewportKind() != 6 && ViewportKind() != 8) barFocusIndex = -1;
    ActualizarBarra(); // layout + auto-scroll + sx/sy frescos
    int barH = BarHeight();
    int yBar = barAbajo ? height - barH : 0;

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)yBar, 0);

    // fondo de la barra: el color de las tarjetas (en el 3D barAlpha
    // es 0.5 y se ve la escena detras)
    SetColorID(ColorID::gris, barAlpha);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);

    // linea oscura que separa la barra del resto de la interfaz (en el
    // 3D no hace falta: la barra es translucida)
    if (ViewportKind() != 1 && barLinea){
        w3dEngine::Disable(w3dEngine::Texture2D);
        SetColorID(ColorID::background);
        barLinea->SetSize(0, (GLshort)(barAbajo ? 0 : barH - GlobalScale),
                          (GLshort)width, (GLshort)GlobalScale);
        barLinea->RenderObject(false);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }

    // botones + pestañas en sus sx/sy (ya con scroll). Local = sx-x, sy-y-yBar.
    for (size_t b = 0; b < BarButtons.size(); b++){
        Button* btn = BarButtons[b];
        if (!btn->visible) continue;
        btn->alpha = barAlpha;   // en el 3D la barra es translucida: los botones tambien
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(btn->sx - x), (GLfloat)(btn->sy - y - yBar), 0);
        btn->Render();
        w3dEngine::PopMatrix();
    }
    for (size_t t = 0; t < BarTabs.size(); t++){
        Tab* tab = BarTabs[t];
        if (!tab->visible) continue;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(tab->sx - x), (GLfloat)(tab->sy - y - yBar), 0);
        tab->Render();
        w3dEngine::PopMatrix();
    }

    // indicador del modo de entrada de texto (T9 del keypad Symbian) a la DERECHA de la barra del viewport ACTIVO,
    // mientras se edita un campo: "Abc"/"abc"/"ABC"/"123". En PC el hook es NULL y no dibuja nada.
    if (this == viewPortActive && g_ViewportBarModoHook){
        const char* modo = g_ViewportBarModoHook();
        if (modo && modo[0]){
            SetColorID(ColorID::accent);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef(0, (GLfloat)((barH - LetterHeightGS) / 2), 0);
            RenderBitmapText(std::string(modo), textAlign::right, width - gapGS * 2);
            w3dEngine::PopMatrix();
        }
    }
    w3dEngine::PopMatrix();
}

void ViewportBase::BarHover(int mx, int my){
    if (!barCard) return;
    // (el redraw ya lo dispara el movimiento del mouse)
    for (size_t b = 0; b < BarButtons.size(); b++){
        BarButtons[b]->hover = BarButtons[b]->Contains(mx, my);
    }
    for (size_t t = 0; t < BarTabs.size(); t++){
        BarTabs[t]->hover = BarTabs[t]->visible && BarTabs[t]->Contains(mx, my);
    }
}

bool ViewportBase::BarClick(int mx, int my){
    if (!barCard) return false;
    if (!Contains(mx, my)) return false;
    int yBar = barAbajo ? y + height - BarHeight() : y;
    if (my < yBar || my >= yBar + BarHeight()) return false;
    // por ahora los botones no hacen nada: la barra solo consume el click
    return true;
}
