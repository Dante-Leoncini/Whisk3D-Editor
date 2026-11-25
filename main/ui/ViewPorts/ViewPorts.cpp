#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render/RenderOptions.h"   // RenderType / g_redraw: son del editor
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/LayoutTree.h" // aviso de destruccion + borrar un arbol entero
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
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

// ---- AVISO DE DESTRUCCION (ver LayoutTree.h) ----
// La lista arranca en NULL por inicializacion ESTATICA: los ganchos se registran desde
// constructores globales de otros .cpp, en cualquier orden, y no dependen de que este
// archivo haya "arrancado".
static ViewportForgetFn s_forgetHooks[ViewportForgetMaxHooks] = {0,0,0,0,0,0,0,0,0,0,0,0};
bool ViewportForgetRegister(ViewportForgetFn f){
    if (!f) return false;
    for (int i = 0; i < (int)ViewportForgetMaxHooks; i++){
        if (s_forgetHooks[i] == f) return true;   // ya estaba
        if (!s_forgetHooks[i]) { s_forgetHooks[i] = f; return true; }
    }
    return false;   // sin lugar: agrandar ViewportForgetMaxHooks
}

// viewports vivos en el proceso (hojas + contenedores): el harness lo compara con el
// arbol actual para probar que abrir un proyecto no deja el layout anterior huerfano
static int s_liveViewports = 0;
int LiveViewports(){ return s_liveViewports; }

void ViewportDeleteTree(ViewportBase* root){
    if (!root) return;
    if (!root->isLeaf()){
        if (root->ContainerKind() == 1){
            ViewportRow* r = (ViewportRow*)root;
            ViewportBase* a = r->childA; ViewportBase* b = r->childB;
            r->childA = NULL; r->childB = NULL;   // antes del delete: el dtor borra childB
            ViewportDeleteTree(a); ViewportDeleteTree(b);
        } else if (root->ContainerKind() == 2){
            ViewportColumn* c = (ViewportColumn*)root;
            ViewportBase* a = c->childA; ViewportBase* b = c->childB;
            c->childA = NULL; c->childB = NULL;
            ViewportDeleteTree(a); ViewportDeleteTree(b);
        }
    }
    delete root;
}

ViewportBase::ViewportBase()
    : x(0), y(0), width(0), height(0) {
    // barra de botones (C++03: en el ctor)
    barDown  = false;
    barAlpha = 1.0f;
    barFocusIndex = -1;
    barScrollX = 0;
    barScrollManual = 0;
    barCard = NULL;
    barLine = NULL;
    toolScroll = 0;
    toolFocus = -1;    // (sin foco de teclado en la barra de abajo)    // toolbar compartida (ToolbarBase.cpp): sin scroll ni gesto al nacer
    toolGesture = false;
    s_liveViewports++;
}
ViewportBase::~ViewportBase(){
    s_liveViewports--;
    BrushBarForgetViewport(this); // no dejar el gesto de la fila del pincel apuntando a un viewport muerto
    // los demas modulos que se guardan punteros a viewports (activo, gesto en curso, menu
    // abierto desde...) sueltan ESTE (LayoutTree.h)
    for (int g = 0; g < (int)ViewportForgetMaxHooks; g++)
        if (s_forgetHooks[g]) s_forgetHooks[g](this);
    if (viewPortActive == this) viewPortActive = NULL;
    if (rootViewport == this) rootViewport = NULL;
    // la BARRA es del viewport: sus botones, pestanias y toolbar se crean en el ctor de cada
    // tipo (BarCreate + los push_back) y nadie mas los libera. Mientras el layout vivia para
    // siempre daba igual; ahora un viewport borrado (abrir un proyecto, cambiar de tipo) los
    // fugaba. Cada Button/Tab libera su Card.
    for (size_t i = 0; i < BarButtons.size(); i++) delete BarButtons[i];
    for (size_t i = 0; i < BarTabs.size(); i++)    delete BarTabs[i];
    for (size_t i = 0; i < ToolButtons.size(); i++) delete ToolButtons[i];
    BarButtons.clear(); BarTabs.clear(); ToolButtons.clear();
    delete barCard;  barCard = NULL;
    delete barLine; barLine = NULL;
}
void ViewportBase::event_mouse_motion(int mx, int my) {}
void ViewportBase::button_left() {}
void ViewportBase::button_right(){};
void ViewportBase::button_up(){};
void ViewportBase::button_down(){};
// base: no-op. El viewport que quiera teclas/rueda las overridea (hoy: Timeline en las dos plataformas; el resto
// todavia solo en PC). Asi el ruteo puede mandarle a cualquiera sin preguntar quien es.
void ViewportBase::event_key_down(int keyCode, bool repeat) {}
void ViewportBase::event_key_up(int key) {}
void ViewportBase::event_mouse_wheel(float dy, int mx, int my) {}
void ViewportBase::mouse_button_up(int button) {}
void ViewportBase::event_finger_gesture(float zoomDelta, float panDx, float panDy){} // base: no-op (los paneles no zoomean/panean)
bool ViewportBase::event_finger_scroll(int px, int py, int dx, int dy){ return false; } // base: no scrollea (el viewport 3D orbita)

// scroll HORIZONTAL de la barra superior (botones + pestañas), unificado rueda/touch en todos los viewports.
extern bool g_redraw;
bool ViewportBase::BarScrollHorizontal(int px, int py, int delta){
    if (!barCard || (BarButtons.empty() && BarTabs.empty())) return false; // no hay barra
    int barH = BarHeight();
    int yBar = barDown ? (y + height - barH) : y;
    if (px < x || px >= x + width || py < yBar || py >= yBar + barH) return false; // el evento no cae en la barra
    barScrollManual -= delta; // delta>0 (rueda arriba / dedo a la derecha) = mostrar lo de la izquierda
    if (barScrollManual < 0) barScrollManual = 0;
    UpdateBar(); // re-clampea contra el ancho total (deja el maximo si te pasas)
    g_redraw = true;
    return true;
}

// (px,py) cae en la barra superior O en la toolbar de abajo? El gesto de arrastre queda lockeado
// a la barra que se toco (toolGesture) para que BarScrollBy scrollee la correcta. Antes esto era un
// override del Viewport3D (el unico con toolbar); ahora la toolbar es de la base -> vale para todos.
bool ViewportBase::OnBar(int px, int py){
    if (barCard && !(BarButtons.empty() && BarTabs.empty())){
        int barH = BarHeight();
        int yBar = barDown ? (y + height - barH) : y;
        if (px >= x && px < x + width && py >= yBar && py < yBar + barH){
            toolGesture = false;
            return true;
        }
    }
    if (OnToolbar(px, py)){ toolGesture = true; return true; }
    return false;
}

// scroll horizontal de la barra SIN hit-test: para cuando el gesto de scroll YA arranco sobre la barra
// y hay que seguir aunque el dedo se salga de ella (gesto lockeado). toolGesture decide CUAL barra.
void ViewportBase::BarScrollBy(int delta){
    if (toolGesture){ ToolbarScrollBy(delta); return; }
    if (!barCard || (BarButtons.empty() && BarTabs.empty())) return;
    barScrollManual -= delta;
    if (barScrollManual < 0) barScrollManual = 0;
    UpdateBar();
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

void ViewportRow::mouse_button_up(int button){
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

void ViewportColumn::mouse_button_up(int button){
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

// EL BORDE ENTRE DOS VIEWPORTS: cuanto se puede errar al agarrarlo, a cada lado de la division. Era el hueco que se ve
// (3 de la UI): con el mouse habia que pegarle justo al centro y con el dedo casi no se podia. Ahora cubre la banda del
// borde de los dos viewports (5 de la UI con el mouse, 9 con el dedo -el ultimo toque fue hace menos de 2 segundos-) y
// en las pantallas x4 el doble, como la linea del borde (LineScaleUI)
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks;   // controls.cpp: el ultimo evento de dedo
#endif
int VpToleranceBorder() {
    int t = 5;
#ifndef W3D_SYMBIAN
    if (g_lastFingerTicks != 0 && SDL_GetTicks() - g_lastFingerTicks < 2000) t = 9;
#endif
    return t * GlobalScale * LineScaleUI();
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
    const int GRAB = VpToleranceBorder();   // (la division gana antes que el contenido de los dos lados)

    // -----------------------------
    // ViewportRow (divide en columnas)
    // -----------------------------
    if (vp->ContainerKind() == 1) {
        ViewportRow* row = (ViewportRow*)vp;
        if (VpIsInPadding(row->childA, row->childB, true, mx, my, GRAB))
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
        if (VpIsInPadding(col->childA, col->childB, false, mx, my, GRAB))
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
    marginGS = uiMargin * scale;
    paddingGS = padding * scale;
    gapGS = gap * scale;
    RowHeightGS = RowHeight * scale;
    borderGS = border * scale;
    bordersGS = borderGS * 2;
    LetterWidthGS = LetterWidth * scale;
    LetterHeightGS = LetterHeight * scale;
    paddingViewportGS = paddingViewport * scale;
    // ALTO MINIMO de un viewport = EXACTAMENTE el alto de la BARRA de menu (mismo calculo que BarHeight(), con el
    // separador de 1px de las barras que no son del 3D). Asi se puede achicar un viewport hasta que queden SOLO sus
    // controles: p.ej. dejar el timeline como una tira con el transporte y nada mas. Antes MinViewportHeight era un
    // valor fijo mas grande y siempre sobraba un hueco inutil debajo de la barra.
    MinViewportHeightGS = UIButtonHeight() + UIBarPadding() * 2 + scale * 2;
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
// el movimiento que se esta procesando es de un DEDO (controls.cpp lo marca en cada motion): un dedo no se puede
// teletransportar como el cursor del mouse, asi que no se envuelve en el borde. Queda el delta real y el gesto (orbitar,
// panear, transformar) sigue aunque el dedo salga del viewport
bool g_cursorIsFinger = false;
void CheckWarpMouseInViewport(int mx, int my, const ViewportBase* vp) {
    if (!vp) return;
    if (g_cursorIsFinger) {
        dx = mx - lastMouseX; dy = my - lastMouseY;
        lastMouseX = mx; lastMouseY = my;
        return;
    }

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
#else
// Symbian: lo llama controls.cpp con el DEDO o con el MOUSE Bluetooth. El mouse se envuelve como en la PC (el cursor
// sale por un borde del viewport y aparece por el otro: se puede seguir moviendo/rotando sin quedarse sin pantalla); el
// cursor lo mueve la plataforma (W3dMouseTeleport). Un dedo no se puede teletransportar: queda solo el delta.
#include "app/W3dMouseEditor.h"
bool g_cursorIsFinger = false;   // (lo marca controls.cpp; aca decide W3dEventMouse: el dedo nunca se envuelve)
void CheckWarpMouseInViewport(int mx, int my, const ViewportBase* vp) {
    if (vp && W3dEventMouse() && W3dMouseTeleport) {
        const int left = vp->x + borderGS, right = vp->x + vp->width - borderGS;
        const int top = vp->y + borderGS, bottom = vp->y + vp->height - borderGS;
        bool warped = false;
        if (mx <= left)        { mx = right - 1; warped = true; }
        else if (mx >= right)  { mx = left + 1;  warped = true; }
        if (my <= top)         { my = bottom - 1; warped = true; }
        else if (my >= bottom) { my = top + 1;    warped = true; }
        if (warped) { W3dMouseTeleport(mx, my); dx = 0; dy = 0; }
        else        { dx = mx - lastMouseX; dy = my - lastMouseY; }
        lastMouseX = mx; lastMouseY = my;
        return;
    }
    (void)vp;
    dx = mx - lastMouseX;
    dy = my - lastMouseY;
    lastMouseX = mx;
    lastMouseY = my;
}
#endif
// ============== barra de botones de los viewports ==============

void ViewportBase::BarCreate(){
    barCard = new Card(NULL, 10, 10);
    barLine = new Rec2D();
    // todos los viewports llevan SIEMPRE primero el boton de icono:
    // dice que tipo de viewport es (a futuro va a servir para cambiarlo)
    BarButtons.push_back(new Button("", IconType::arrow, true));
}

int ViewportBase::BarTopOffset() const {
    if (!barCard || barDown) return 0;
    return BarHeight();
}

int ViewportBase::BarHeight() const {
    // boton + padding abajo + 1px MAS de margen arriba (los botones quedan un
    // toque mas abajo). Las barras que NO son del 3D llevan ademas una linea
    // separadora de 1px en el borde inferior.
    int h = UIButtonHeight() + UIBarPadding() * 2 + GlobalScale;
    if (ViewportKind() != 1 && barLine) h += GlobalScale; // separador (1px)
    return h;
}

// recalcula anchos, auto-scroll (centra el menu enfocado) y los sx/sy
// absolutos de botones/pestañas. SIN GL: la usa RenderBar y tambien el ruteo
// de teclado antes de abrir un menu (para que el hit-test acierte la posicion
// YA scrolleada). El foco solo vale con un menu abierto.
// EL ICONO DE CADA TIPO de viewport en el MENU de tipo (el boton de la esquina sigue siendo la flecha para abajo): el cubo
// el 3D, el ojo el outliner, la llave inglesa las propiedades, la malla el editor UV, el reloj la linea de tiempo, la
// textura el editor 2D, el codigo el IDE, la carpeta la biblioteca. Los que no tienen uno propio, ninguno (la flecha)
int W3dViewportIcon(int kind) {
    switch (kind) {
        case 1: return (int)IconType::object;
        case 2: return (int)IconType::visible;
        case 3: return (int)IconType::modifier;
        case 4: return (int)IconType::mesh;
        case 5: return (int)IconType::clock;
        case 6: return (int)IconType::texture;
        case 7: return (int)IconType::console;
        case 8: return (int)IconType::script;
        case 9: return (int)IconType::folder;
    }
    return (int)IconType::arrow;
}
void ViewportBase::UpdateBar(){
    if (!barCard || BarButtons.empty()) return;
    int barH = BarHeight();
    int yBar = barDown ? height - barH : 0;
    int btnGap = 2 * GlobalScale; // gap CHICO entre botones (antes gapGS) p/ que entren mas
    // NO se resetea barFocusIndex aca: el ruteo de teclado la llama ANTES de
    // abrir el menu (ahi todavia no hay menu abierto). El reset lo hace RenderBar.

    // medir anchos + posicion del enfocado
    int total = gapGS, focusX = -1, focusW = 0;
    for (size_t b = 0; b < BarButtons.size(); b++){
        Button* btn = BarButtons[b];
        if (!btn->visible) continue;
        btn->Resize(width - gapGS * 2);
        if (this == viewPortActive && (int)b == barFocusIndex){ focusX = total; focusW = btn->width; }
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
    if (focusX >= 0){
        barScrollX = focusX + focusW / 2 - width / 2; // teclado: centrar el enfocado
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
        int by = barDown ? (barH - UIBarPadding() - btn->height)
                          : (UIBarPadding() + GlobalScale);
        if (by < 0) by = 0;
        // con un menu abierto, el ENFOCADO se resalta verde y los demas no
        btn->focusMenu = (this == viewPortActive && barFocusIndex >= 0 && (int)b == barFocusIndex); // solo el viewport ACTIVO resalta su barra
        btn->sx = x + bx;
        btn->sy = y + yBar + by;
        bx += btn->width + btnGap;
    }
    for (size_t t = 0; t < BarTabs.size(); t++){
        Tab* tab = BarTabs[t];
        if (!tab->visible){ tab->sx = -10000; tab->sy = -10000; continue; }
        tab->Resize(width - gapGS * 2);
        int ty = barDown ? 0 : barH - tab->height;
        tab->sx = x + bx;
        tab->sy = y + yBar + ty;
        bx += tab->width + GlobalScale;
    }
}

// hook opcional (lo setea la plataforma): texto del modo de entrada de texto a mostrar en la barra del viewport
// ACTIVO mientras se edita un campo (Symbian lo apunta a W3dT9TextMode: "Abc"/"abc"/"ABC"/"123"). NULL en PC.
const char* (*g_ViewportBarModeHook)() = 0;
// shift (lapiz) mantenido durante la edicion de texto: la barra muestra "copiar" (izq) y "pegar" (der). NULL en PC.
bool (*g_ViewportBarShiftHook)() = 0;

void ViewportBase::RenderBar(){
    // se llama al final del Render del viewport (con su ortho 2D activo)
    if (!barCard || BarButtons.empty()) return;
    // sin ningun menu abierto el foco se apaga (la barra vuelve a la normalidad). EXCEPCION: el Timeline (kind 5)
    // navega sus botones de transporte SIN abrir menu -> su foco lo maneja el ruteo de teclado (LayoutTimelineBar*),
    // no se apaga aca (sino se perderia cada frame).
    extern bool LayoutMenuOpen();
    extern bool LayoutFocusInTransport(ViewportBase*);
    // el 3D (kind 1) retiene su foco de barra SOLO si esta en el transporte Stop/Play Y es el viewport ACTIVO
    // (como el Timeline kind 5, que no se apaga cada frame). El 'this==viewPortActive' hace que al ciclar de
    // viewport (verde) el 3D SUELTE el foco -> al volver las flechas orbitan de nuevo (no quedan pegadas en
    // Stop/Play) y un 3D no-activo (multi-vista PC) no retiene un indice stale.
    bool retainsTransport = (this == viewPortActive) && LayoutFocusInTransport(this);
    // el UV retiene el foco sobre "Edit" (accion directa sin menu, como el transporte): sin esto el
    // foco se borraba cada frame y el OK no encontraba a quien activar (keypad del N95)
    extern bool LayoutFocusInUVEdit(ViewportBase*);
    bool retainsUVEdit = (this == viewPortActive) && LayoutFocusInUVEdit(this);
    if (!LayoutMenuOpen() && ViewportKind() != 5 && ViewportKind() != 6 && ViewportKind() != 8
        && !retainsTransport && !retainsUVEdit) barFocusIndex = -1;
    UpdateBar(); // layout + auto-scroll + sx/sy frescos
    int barH = BarHeight();
    int yBar = barDown ? height - barH : 0;

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)yBar, 0);

    // fondo de la barra: el color de las tarjetas (en el 3D barAlpha
    // es 0.5 y se ve la escena detras)
    SetColorID(ColorID::gray, barAlpha);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);

    // linea oscura que separa la barra del resto de la interfaz (en el
    // 3D no hace falta: la barra es translucida)
    if (ViewportKind() != 1 && barLine){
        w3dEngine::Disable(w3dEngine::Texture2D);
        SetColorID(ColorID::background);
        barLine->SetSize(0, (GLshort)(barDown ? 0 : barH - GlobalScale),
                          (GLshort)width, (GLshort)GlobalScale);
        barLine->RenderObject(false);
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
    if (this == viewPortActive && g_ViewportBarModeHook){
        const char* mode = g_ViewportBarModeHook();
        if (mode && mode[0]){
            int ty = (barH - LetterHeightGS) / 2;
            bool shift = (g_ViewportBarShiftHook && g_ViewportBarShiftHook());
            if (shift){
                // con SHIFT: "copiar" a la IZQUIERDA + "pegar" a la DERECHA (asi se sabe que shift+softkey copia/pega);
                // el modo (Abc/abc/ABC/123) queda CENTRADO para no pisar "pegar".
                SetColorID(ColorID::white);
                w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)gapGS, (GLfloat)ty, 0);
                RenderBitmapText(std::string(T("Copy")), textAlign::left, width / 2); w3dEngine::PopMatrix();
                w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(width - gapGS), (GLfloat)ty, 0);
                RenderBitmapText(std::string(T("Paste")), textAlign::right, width / 2); w3dEngine::PopMatrix();
                SetColorID(ColorID::accent);
                w3dEngine::PushMatrix(); w3dEngine::Translatef(0, (GLfloat)ty, 0);
                RenderBitmapText(std::string(mode), textAlign::center, width); w3dEngine::PopMatrix();
            } else {
                // sin shift: el modo a la DERECHA (borde derecho anclado en width - gapGS; antes quedaba en x=0 = fuera)
                SetColorID(ColorID::accent);
                w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(width - gapGS), (GLfloat)ty, 0);
                RenderBitmapText(std::string(mode), textAlign::right, width - gapGS * 2); w3dEngine::PopMatrix();
            }
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
    // la TOOLBAR de abajo: igual que la de arriba (se ilumina y dice que es al pasarle el mouse)
    const bool tb = !ToolButtons.empty() && ToolbarVisible();
    for (size_t b = 0; b < ToolButtons.size(); b++){
        Button* btn = ToolButtons[b];
        btn->hover = tb && btn->visible && btn->Contains(mx, my);
    }
}

bool ViewportBase::BarClick(int mx, int my){
    if (!barCard) return false;
    if (!Contains(mx, my)) return false;
    int yBar = barDown ? y + height - BarHeight() : y;
    if (my < yBar || my >= yBar + BarHeight()) return false;
    // por ahora los botones no hacen nada: la barra solo consume el click
    return true;
}

// ============================================================================
//  LOS PANELES GUARDADOS MIENTRAS SE NAVEGA (ver ViewPorts.h)
// ============================================================================
#include "graphics/w3dTexture.h"     // UploadRGBA / UpdateRGBA: la imagen guardada
#include "WhiskUI/draw/glesdraw.h"   // W3dDrawStrip4
#include "base/w3dlog.h"            // w3dLogfW
#include "ViewPorts/LayoutInput.h"   // LayoutMenuOpen
#include "ViewPorts/Notifications.h" // NotificationsVisible
#include "WhiskUI/widgets/TextField.h"   // g_activeTextField (el cursor que parpadea)
#include "animation/Animation.h"     // PlayAnimation
#include "animation/VertexAnimation.h"
#include "objects/Materials.h"       // HasAnimationActive
#include "importers/import_obj.h"    // PendingTextures
#include <vector>
extern int g_pureGame;   // (render/RenderOptions.h: el modo juego puro no tiene paneles)
namespace {
bool gCacheNav = false, gCacheChanged = false, gCacheValid = false;
bool gCacheOff = false;   // (la lectura de la pantalla volvio negra: en esta GPU no se guarda, se dibuja todo)
int gCacheW = 0, gCacheH = 0, gCacheTexW = 0, gCacheTexH = 0;
unsigned int gCacheTex = 0;
std::vector<unsigned char> gCachePx;
int PowerOfTwo(int v) { int p = 1; while (p < v) p <<= 1; return p; }
// lo que tambien cambia la pantalla mientras se navega: ahi se dibuja todo
bool CacheQuiet() {
    return !g_pureGame && !PlayAnimation && !HasAnimationActive() && VertexAnimationActives.empty() && !LayoutMenuOpen() &&
           !NotificationsVisible() && PendingTextures() == 0 && !g_activeTextField;
}
}
int g_viewCacheFrames = 0;   // (los cuadros que salieron de la imagen guardada: el harness lo mira)
void W3dViewCacheNavigated() { gCacheNav = true; }
void W3dViewCacheChanged() { gCacheChanged = true; }

// las vistas 3D del arbol (las que se vuelven a dibujar encima de la imagen guardada)
static void Views3D(ViewportBase* node, std::vector<ViewportBase*>& out) {
    if (!node) return;
    if (!node->isLeaf()) {
        const int k = node->ContainerKind();
        if (k == 1) { Views3D(((ViewportRow*)node)->childA, out); Views3D(((ViewportRow*)node)->childB, out); }
        else if (k == 2) { Views3D(((ViewportColumn*)node)->childA, out); Views3D(((ViewportColumn*)node)->childB, out); }
        return;
    }
    if (node->ViewportKind() == 1) out.push_back(node);
}
// un rectangulo de la imagen guardada (x, y, w, h de la pantalla, arriba a la izquierda) en el mismo lugar
static void CacheRect(int x, int y, int rw, int rh, int h) {
    if (rw <= 0 || rh <= 0) return;
    // (las filas de la lectura van de abajo para arriba: la de arriba de la pantalla es la ultima de la imagen)
    const float u0 = (float)x / (float)gCacheTexW, u1 = (float)(x + rw) / (float)gCacheTexW;
    const float v0 = (float)(h - y) / (float)gCacheTexH, v1 = (float)(h - y - rh) / (float)gCacheTexH;
    const GLshort v[8] = { (GLshort)x, (GLshort)y, (GLshort)(x + rw), (GLshort)y,
                           (GLshort)x, (GLshort)(y + rh), (GLshort)(x + rw), (GLshort)(y + rh) };
    const GLfloat uv[8] = { u0, v0, u1, v0, u0, v1, u1, v1 };
    W3dDrawStrip4(v, uv);
}

bool W3dViewCacheBegin(ViewportBase* root, int w, int h) {
    if (gCacheOff || !gCacheValid || !gCacheNav || gCacheChanged || !gCacheTex || w != gCacheW || h != gCacheH ||
        !CacheQuiet())
        return false;
    namespace gfx = w3dEngine;
    gfx::Viewport(0, 0, w, h);
    gfx::Disable(gfx::ScissorTest);
    gfx::MatrixMode(gfx::Projection); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Ortho(0, w, h, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::PushMatrix(); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Fog); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Blend); gfx::Enable(gfx::Texture2D);
    gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::Color4f(1, 1, 1, 1);
    gfx::BindTexture(gCacheTex);
    // con UNA vista 3D, la imagen solo alrededor (la vista se dibuja entera encima: pintarla dos veces era la mitad de
    // la GPU del cuadro en el Ace); con varias, toda
    std::vector<ViewportBase*> v3;
    Views3D(root, v3);
    if (v3.size() == 1) {
        const ViewportBase* vp = v3[0];
        const int x0 = vp->x, y0 = vp->y, x1 = vp->x + vp->width, y1 = vp->y + vp->height;
        CacheRect(0, 0, w, y0, h);              // arriba
        CacheRect(0, y1, w, h - y1, h);         // abajo
        CacheRect(0, y0, x0, y1 - y0, h);       // izquierda
        CacheRect(x1, y0, w - x1, y1 - y0, h);  // derecha
    } else CacheRect(0, 0, w, h, h);
    gfx::BindTexture(0);
    gfx::MatrixMode(gfx::Projection); gfx::PopMatrix();
    gfx::MatrixMode(gfx::ModelView); gfx::PopMatrix();
    gfx::Enable(gfx::DepthTest);
    g_viewCacheFrames++;
    return true;
}

void W3dViewCacheRender3D(ViewportBase* node) {
    if (!node) return;
    if (!node->isLeaf()) {
        const int k = node->ContainerKind();
        if (k == 1) { W3dViewCacheRender3D(((ViewportRow*)node)->childA); W3dViewCacheRender3D(((ViewportRow*)node)->childB); }
        else if (k == 2) { W3dViewCacheRender3D(((ViewportColumn*)node)->childA); W3dViewCacheRender3D(((ViewportColumn*)node)->childB); }
        return;
    }
    if (node->ViewportKind() == 1) node->Render();   // (las vistas 3D)
}

// guarda la pantalla de ahora (los paneles) en la textura
static void CacheSave(int w, int h) {
    namespace gfx = w3dEngine;
    gCachePx.resize((size_t)w * h * 4);
    gfx::ReadPixelsRGBA(0, 0, w, h, &gCachePx[0]);
    // (una lectura toda negra: esta GPU no deja leer la pantalla asi. Se apaga para la sesion: los paneles negros
    //  mientras se navega serian peores que dibujarlos)
    bool any = false;
    for (size_t i = 0; i < gCachePx.size() && !any; i += 4) any = (gCachePx[i] | gCachePx[i + 1] | gCachePx[i + 2]) != 0;
    if (!any) { gCacheOff = true; w3dLogfW("[paneles] la pantalla se lee negra: se dibujan enteros al navegar"); return; }
    // (potencias de dos: GLES 1.1 -el N95- no tiene otras; la imagen ocupa la esquina)
    const int tw = PowerOfTwo(w), th = PowerOfTwo(h);
    if (!gCacheTex || tw != gCacheTexW || th != gCacheTexH) {
        if (gCacheTex) gfx::DeleteTexture(gCacheTex);
        std::vector<unsigned char> empty((size_t)tw * th * 4, 0);
        gCacheTex = gfx::UploadRGBA(&empty[0], tw, th, false, false, false, false);
        gCacheTexW = tw; gCacheTexH = th;
    }
    if (gCacheTex && gfx::UpdateRGBA(gCacheTex, 0, 0, w, h, &gCachePx[0], false)) { gCacheValid = true; gCacheW = w; gCacheH = h; }
}

void W3dViewCacheEnd(int w, int h, bool only3D) {
    if (!only3D) {
        // un cuadro entero: si arranco una navegacion se guarda (la imagen de los paneles para los cuadros que siguen);
        // si no, la guardada ya no vale
        gCacheValid = false;
        if (!gCacheOff && gCacheNav && !gCacheChanged && CacheQuiet() && w > 0 && h > 0) CacheSave(w, h);
    }
    gCacheNav = false; gCacheChanged = false;
}
