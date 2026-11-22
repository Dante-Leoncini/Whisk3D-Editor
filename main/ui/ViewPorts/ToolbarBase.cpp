// ===================================================================================================
//  TOOLBAR COMPARTIDA de los viewports (barra de HERRAMIENTAS de abajo). Mecanismo UNICO subido
//  desde ViewPort3D_Toolbar.cpp (donde nacio) a ViewportBase, para que el UV editor y el Editor 2D
//  tengan la misma barra sin duplicar nada:
//    - ToolButtons por ROL (btn->role; reordenar no rompe el dispatch)
//    - layout con scroll horizontal clampeado (ToolbarUpdate)
//    - hit TOLERANTE al click (tap desviado agarra el boton mas cercano en X)
//    - render (fondo translucido + botones, mismo patron que RenderBar)
//    - MRU (ToolbarMRU): historial de acciones por editor, sin repetir, max 8
//  La parte CONTEXTUAL (que botones se ven y que hacen) es de cada editor, en dos virtuales:
//  ToolbarSync() (estado puro, sin GL -> testeable headless) y ToolbarActionRole(rol).
//  El Viewport3D la usa IGUAL que antes (cero regresion): sus overrides viven en ViewPort3D_Toolbar.cpp.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"     // T(): los textos salen en el idioma del sistema
#include "ViewPorts/ViewPorts.h"
#include "variables.h"   // enum TBMove..TBDelete (ids del historial de acciones)
#include "edit/WeightPaint.h"            // BrushGet() + WeightPaintLabels (el texto de las barras)
#include "WhiskUI/Propieties/PropFloat.h"// edicion numerica por teclado del valor (misma que el panel)
#include "ViewPorts/PopUp/NumPad.h"      // teclado virtual (tactil/Android)
#include "ViewPorts/TransformUI.h"       // ToolbarUsesTouch(): decide teclado fisico vs virtual
#include "ViewPorts/LayoutInput.h"       // LayoutKey (nav con el keypad de Symbian)
#include "edit/Proportional.h"           // la fila tambien es la BARRA DE INFLUENCIA del proportional editing
#include <stdio.h>
#include <cstdlib>                       // abs (umbral tap/arrastre)

namespace gfx = w3dEngine;
extern bool g_redraw;

// ---- MRU compartido: 'id' pasa al frente, sin repetidos, hasta 8 entradas ----
void ToolbarMRU(std::vector<int>& hist, int id){
    for (size_t i = 0; i < hist.size(); i++)
        if (hist[i] == id){ hist.erase(hist.begin() + i); break; } // sin repetir
    hist.insert(hist.begin(), id);                                 // la ultima usada, primera
    if (hist.size() > 8) hist.pop_back();                          // hasta 8
    g_redraw = true;
}

// ---- texto de una accion del historial (los ids TB* de variables.h), traducido ----
const char* ToolbarActionLabel(int id){
    switch (id){
        case TBMove:    return T("Move");
        case TBRotate:  return T("Rotate");
        case TBScale:   return T("Scale");
        case TBExtrude: return T("Extrude");
        case TBLoopCut: return T("Loop Cut");
        case TBDelete:  return T("Delete");
    }
    return "?";
}

// default: la toolbar existe si el editor puso botones. Cada editor la restringe por contexto
// (el 3D: cfg.newUser en Symbian; el UV: solo en edicion; el 2D: no en vista de juego).
bool ViewportBase::ToolbarVisible() const { return !ToolButtons.empty(); }

int ViewportBase::ToolbarHeight() const { return BarHeight(); }

bool ViewportBase::OnToolbar(int px, int py) const {
    if (ToolButtons.empty() || !ToolbarVisible()) return false;
    int barH = ToolbarHeight();
    int yBar = y + height - barH; // pegada abajo (la barra de menu esta arriba)
    return px >= x && px < x + width && py >= yBar && py < yBar + barH;
}

void ViewportBase::ToolbarScrollBy(int delta){
    toolScroll -= delta;
    if (toolScroll < 0) toolScroll = 0;
    ToolbarUpdate(); // re-clampea contra el ancho total actual
    g_redraw = true;
}

// visibilidad CONTEXTUAL (virtual de cada editor) + layout comun: anchos, clamp del scroll y
// los sx/sy absolutos con el scroll aplicado. Mismo patron que la barra de arriba, en dos
// pasadas (primero el ancho total para clampear, despues las posiciones).
void ViewportBase::ToolbarUpdate(){
    ToolbarSync(); // que se ve / que dice cada boton: lo decide el editor

    int barH = ToolbarHeight();
    int yBar = y + height - barH;
    int btnGap = gapGS / 2 + 1;
    int total = gapGS;
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible) continue;
        btn->Resize(width - gapGS * 2); // (btn->square fuerza ancho = alto, ej: X/Y/Z, Undo/Redo)
        if (btn->separator) total += gapGS * 2;   // el hueco de la linea que separa los grupos
        total += btn->width + btnGap;
    }
    int maxS = total - width; if (maxS < 0) maxS = 0;
    if (toolScroll > maxS) toolScroll = maxS;
    int bx = gapGS - toolScroll;
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible){ btn->sx = -10000; btn->sy = -10000; btn->hover = false; continue; }
        if (btn->separator) bx += gapGS * 2;
        btn->sx = x + bx;
        btn->sy = yBar + (barH - btn->height) / 2;
        // (el hover NO se calcula aca: lo pone BarHover con el mouse de cada movimiento, como la barra de arriba)
        bx += btn->width + btnGap;
    }
}

// EL FOCO DE TECLADO de la barra (el softkey derecho del N95, como el izquierdo con la barra de arriba): izq/der
// recorren los botones visibles, OK hace lo del click y la barra se corre para que el enfocado se vea
bool ViewportBase::ToolbarFocusMove(int dir){
    if (ToolButtons.empty() || !ToolbarVisible()) { toolFocus = -1; return false; }
    ToolbarUpdate();
    const int n = (int)ToolButtons.size();
    int i = toolFocus;
    if (i < 0 || i >= n || !ToolButtons[(size_t)i]->visible) {   // sin foco: el primero (o el ultimo) visible
        i = dir >= 0 ? -1 : n;
    }
    for (int k = 0; k < n; k++) {
        i += dir >= 0 ? 1 : -1;
        if (i < 0) i = n - 1; if (i >= n) i = 0;
        if (ToolButtons[(size_t)i]->visible) break;
    }
    if (i < 0 || i >= n || !ToolButtons[(size_t)i]->visible) { toolFocus = -1; return false; }
    toolFocus = i;
    // que se vea: correr el scroll hasta que entre entero
    Button* b = ToolButtons[(size_t)i];
    if (b->sx < x + gapGS) toolScroll -= (x + gapGS) - b->sx;
    else if (b->sx + b->width > x + width - gapGS) toolScroll += (b->sx + b->width) - (x + width - gapGS);
    if (toolScroll < 0) toolScroll = 0;
    ToolbarUpdate();
    g_redraw = true;
    return true;
}
bool ViewportBase::ToolbarFocusActivate(){
    if (toolFocus < 0 || toolFocus >= (int)ToolButtons.size() || !ToolbarVisible()) { toolFocus = -1; return false; }
    ToolbarUpdate();
    Button* b = ToolButtons[(size_t)toolFocus];
    if (!b->visible) { toolFocus = -1; return false; }
    ToolbarActionRole(b->role);
    // (la accion pudo cambiar los botones: el foco queda en uno que se vea, o se va)
    ToolbarUpdate();
    if (toolFocus >= (int)ToolButtons.size() || !ToolButtons[(size_t)toolFocus]->visible) ToolbarFocusMove(+1);
    g_redraw = true;
    return true;
}

bool ViewportBase::ToolbarClick(int mx, int my){
    if (!OnToolbar(mx, my)) return false;
    ToolbarUpdate(); // sx/sy frescos
    // Buscar el boton: match EXACTO, o si el toque cayo al lado (dedo gordo sobre botones chicos) el MAS
    // CERCANO en X dentro de una tolerancia. Asi un tap un poco desviado igual pulsa el boton que se queria
    // en vez de caer en el gap y no hacer nada. La barra es una franja horizontal -> decide el X.
    Button* target = NULL;
    for (size_t i = 0; i < ToolButtons.size(); i++){       // 1) match exacto
        Button* btn = ToolButtons[i];
        if (btn->visible && btn->Contains(mx, my)) { target = btn; break; }
    }
    if (!target){                                          // 2) sin exacto: el mas cercano en X
        int bestD = 1 << 30;
        for (size_t i = 0; i < ToolButtons.size(); i++){
            Button* b = ToolButtons[i];
            if (!b->visible) continue;
            int cx = b->sx + b->width / 2;
            int d = (mx > cx) ? (mx - cx) : (cx - mx);
            if (d < bestD) { bestD = d; target = b; }
        }
        if (target && bestD > target->width) target = NULL; // gap real (lejos): sin accion
    }
    if (target){
        ToolbarActionRole(target->role); // la accion es del editor (virtual)
        g_redraw = true;
    }
    return true; // dentro de la barra: consumir igual (no pasa al contenido)
}


// ===================================================================================================
//  FILA DE BARRAS DEL PINCEL (justo ARRIBA de la toolbar): [radio: 40px] [valor: 100%]
//  Dos barras al 50% del ancho del viewport que se arrastran para cambiar el valor EN VIVO, sin
//  abrir ningun desplegable (antes eran dos botones que abrian un menu con un slider adentro).
//
//  LAS DOS NO SON IGUALES, y es a proposito:
//    - VALOR (0..100%): tiene tope, asi que SE LLENA y el arrastre es ABSOLUTO (donde tocas, ese
//      valor), el mismo mapeo que el item-slider de los menus.
//    - RADIO (px): NO tiene tope (el minimo es 0px), asi que no hay fraccion que dibujar: no se
//      llena, y el arrastre es RELATIVO (1 px de dedo = 1 px de radio) desde el valor que tenia
//      al empezar. Mapearlo absoluto obligaria a inventar un maximo.
//
//  Un TAP/click SIN arrastrar (menos que el umbral) abre la EDICION NUMERICA exacta: en PC el
//  campo se edita con el teclado fisico (g_activeTextField, igual que un campo del panel de
//  propiedades) y en tactil/Android se abre ademas el NumPad. Reusa PropFloat para no reimplementar
//  el parseo/commit/cancel (Enter y Esc ya estan ruteados para cualquier PropFloat en LayoutInput).
//
//  TECLADO (Symbian/keypad, sin mouse): arriba/abajo turnan entre las dos barras, OK entra a
//  ajustar y ahi izquierda/derecha mueven el valor, C lo suelta. Mismo contrato que el slider de
//  los menus (PopupMenu::FitSlider) y que el foco de barra del Timeline.
//
//  El estado del gesto es GLOBAL (no por viewport): el pincel es uno solo y solo se puede arrastrar
//  una barra a la vez -- misma decision que el drag del item-slider (MenuSliderDrag*).
// ===================================================================================================
static ViewportBase* gBBvp     = NULL;  // viewport de la barra que se esta arrastrando (NULL = ninguna)
static int   gBBidx     = -1;           // 0 = radio, 1 = valor
static int   gBBdownX   = 0, gBBdownY = 0;
static float gBBRadius0 = 0.0f;          // radio al apoyar (el radio se arrastra RELATIVO a el)
static bool  gBBMoved   = false;         // ya paso el umbral -> es arrastre, no tap
static int   gBBFocus   = -1;            // teclado: barra enfocada (-1 = ninguna)
static bool  gBBedit    = false;         // teclado: OK apretado -> izq/der mueven el valor

// El PropFloat no edita el pincel DIRECTO sino estos espejos, por las unidades: la fuerza vive
// 0..1 y la barra se muestra en %, asi que tipear "32" tiene que dar 0.32 y no 32. El espejo se
// sincroniza antes de abrir la edicion y el onChange lo baja al pincel.
static float gBBRadiusPx = 40.0f;
static float gBBValuePct = 100.0f;
static PropFloat* gBBpfRadius = NULL;
static PropFloat* gBBpfValue  = NULL;

// ---- BARRA DE INFLUENCIA del proportional editing: la misma fila, cuando no hay pincel (PropBarVisible).
// Una sola barra a todo el ancho con el radio, que se arrastra RELATIVO (no tiene tope). Solo esta
// mientras se mueve/rota/escala; para prender/apagar estan la tecla O y el boton de la toolbar. ----
static bool BBModeProp(const ViewportBase* vp){ return vp && !vp->BrushBarVisible() && vp->PropBarVisible(); }
static void BBCellProp(const ViewportBase* vp, int& sx, int& sw){
    sx = vp->x + gapGS; sw = vp->width - 2 * gapGS; if (sw < 1) sw = 1;
}

static const float kBBRadiusMax = 4000.0f; // "sin tope" en la practica (mas grande que cualquier pantalla)

static void BBApplyRadius(){ BrushGet().radiusPx = gBBRadiusPx;         g_redraw = true; }
static void BBApplyValue(){ BrushGet().force  = gBBValuePct*0.01f;  g_redraw = true; }

// el PropFloat de la barra 'idx', creado al vuelo y con el espejo YA sincronizado con el pincel
static PropFloat* BBprop(int idx){
    if (idx == 0){
        if (!gBBpfRadius){
            gBBpfRadius = new PropFloat(T("Radius"), "px");
            gBBpfRadius->value = &gBBRadiusPx;
            gBBpfRadius->integer = true;              // el radio es px enteros
            gBBpfRadius->SetRange(0.0f, kBBRadiusMax); // minimo 0px; el maximo es solo un tope de cordura
            gBBpfRadius->onChange = BBApplyRadius;
        }
        gBBRadiusPx = BrushGet().radiusPx;
        return gBBpfRadius;
    }
    if (!gBBpfValue){
        gBBpfValue = new PropFloat(T("Value"), "%");
        gBBpfValue->value = &gBBValuePct;
        gBBpfValue->integer = true;                  // 0..100 en enteros: "32%" y no "32.4171%"
        gBBpfValue->SetRange(0.0f, 100.0f);
        gBBpfValue->onChange = BBApplyValue;
    }
    gBBValuePct = BrushGet().force * 100.0f;
    return gBBpfValue;
}

// alto REAL que ocupa la toolbar de este viewport (0 si no se dibuja): la fila del pincel se
// apoya arriba de ella, y si la toolbar esta oculta (Symbian sin cfg.newUser) baja al piso.
static int BBHeightToolbar(const ViewportBase* vp){
    if (!vp || vp->ToolButtons.empty() || !vp->ToolbarVisible()) return 0;
    return vp->ToolbarHeight();
}

int ViewportBase::BrushBarHeight() const {
    return (BrushBarVisible() || PropBarVisible()) ? BarHeight() : 0;
}

// las dos celdas: mitad y mitad, con el mismo gap de la toolbar como margen y separacion
static void BBCells(const ViewportBase* vp, int& cx0, int& cx1, int& cw){
    cw  = (vp->width - gapGS * 3) / 2;
    if (cw < 1) cw = 1;
    cx0 = vp->x + gapGS;
    cx1 = cx0 + cw + gapGS;
}

bool ViewportBase::OnBrushBar(int px, int py) const {
    int h = BrushBarHeight();
    if (h <= 0) return false;
    int yTop = y + height - BBHeightToolbar(this) - h;
    return px >= x && px < x + width && py >= yTop && py < yTop + h;
}

// mapeo ABSOLUTO de la barra del VALOR: la X dentro de la celda es el porcentaje (0..100)
static void BBValueFromX(const ViewportBase* vp, int mx){
    int cx0, cx1, cw; BBCells(vp, cx0, cx1, cw);
    float frac = (float)(mx - cx1) / (float)cw;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    BrushGet().force = frac;
}

bool ViewportBase::BrushBarClick(int mx, int my){
    if (!OnBrushBar(mx, my)) return false;
    if (BBModeProp(this)){   // barra de influencia: el down arma el arrastre del radio (en cualquier punto de la fila)
        if (NumEditActive()) NumEditCommit();
        gBBvp = this; gBBidx = 2; gBBdownX = mx; gBBdownY = my; gBBRadius0 = g_prop.radius; gBBMoved = false;
        g_redraw = true;
        return true;
    }
    // habia una edicion numerica abierta (esta barra u otro campo): tocar afuera la CONFIRMA,
    // igual que en el panel de propiedades. Sino quedaba un campo a medio tipear invisible.
    if (NumEditActive()) NumEditCommit();
    int cx0, cx1, cw; BBCells(this, cx0, cx1, cw);
    gBBvp    = this;
    gBBidx   = (mx < cx1) ? 0 : 1;   // el gap del medio cae del lado del radio: da igual, no hay valor ahi
    gBBdownX = mx; gBBdownY = my;
    gBBRadius0 = BrushGet().radiusPx;
    gBBMoved = false;
    // el foco de TECLADO no se toca aca a proposito: es del keypad (Symbian). Si el click lo
    // moviera, en PC quedaria una barra con el borde accent encendido para siempre despues de
    // tocarla una vez, que se lee como "seleccionada" y no lo esta.
    // OJO: el DOWN no cambia el valor. Si lo cambiara, un tap (que es "editar exacto por teclado")
    // primero pegaria un salto al valor de donde cayo el dedo. El valor se mueve recien al ARRASTRAR.
    g_redraw = true;
    return true;
}

bool BrushBarDragActive(){ return gBBvp != NULL; }

// el viewport que se estaba arrastrando se DESTRUYO (se cerro el panel / cambio el layout en medio
// del gesto): olvidarlo sin pasar por el camino de "tap" (no hay barra a la que abrirle el teclado).
void BrushBarForgetViewport(ViewportBase* vp){
    if (gBBvp == vp){ gBBvp = NULL; gBBidx = -1; gBBMoved = false; }
}

void BrushBarDragMove(int mx, int my){
    if (!gBBvp) return;
    if (!gBBMoved){
        int threshold = 6 * GlobalScale; // el mismo del drag-scroll de los menus
        if (abs(mx - gBBdownX) < threshold && abs(my - gBBdownY) < threshold) return; // todavia puede ser un tap
        gBBMoved = true;
    }
    if (gBBidx == 2){   // radio del proportional editing: RELATIVO (1% del radio por pixel), sin tope
        ProportionalRadiusSet(gBBRadius0 * (1.0f + (float)(mx - gBBdownX) / (100.0f * GlobalScale)));
        ProportionalReapply();
    } else if (gBBidx == 0){
        float r = gBBRadius0 + (float)(mx - gBBdownX); // RELATIVO: no hay tope al que mapear
        if (r < 0.0f) r = 0.0f;
        if (r > kBBRadiusMax) r = kBBRadiusMax;
        BrushGet().radiusPx = r;
    } else {
        BBValueFromX(gBBvp, mx);                     // ABSOLUTO: donde tocas, ese valor
    }
    g_redraw = true;
}

void BrushBarDragRelease(){
    if (!gBBvp) return;
    if (!gBBMoved && gBBidx != 2){   // (la barra de influencia solo se arrastra: el transform ya usa el teclado)
        // TAP / click simple: edicion numerica EXACTA. En PC alcanza el campo inline (el teclado
        // fisico entra por g_activeTextField); en tactil hace falta ademas el teclado en pantalla.
        PropFloat* pf = BBprop(gBBidx);
        if (pf){
            pf->StartEditText();
            if (ToolbarUsesTouch()) KeyboardNumberOpen();
        }
    }
    gBBvp = NULL; gBBidx = -1; gBBMoved = false;
    g_redraw = true;
}

int  BrushBarFocus(){ return gBBFocus; }
bool BrushBarDraggingRadius(){ return gBBvp && gBBidx == 0 && gBBMoved; }
bool BrushBarEditing(){ return gBBedit; }
void BrushBarReleaseFocus(){ gBBFocus = -1; gBBedit = false; }

bool BrushBarKey(ViewportBase* vp, int key){
    if (!vp || !(vp->BrushBarVisible() || vp->PropBarVisible())) return false;
    switch (key){
        case LayoutKey::Up:
        case LayoutKey::Down:
            // turnar entre las dos barras (la primera vez entra en la de arriba a la izquierda)
            gBBFocus = (gBBFocus < 0) ? 0 : (gBBFocus == 0 ? 1 : 0);
            gBBedit  = false;
            g_redraw = true;
            return true;
        case LayoutKey::Enter:
            if (gBBFocus < 0) gBBFocus = 0;   // OK sin foco: entra a la primera
            else gBBedit = !gBBedit;          // OK con foco: entra/sale del ajuste con izq/der
            g_redraw = true;
            return true;
        case LayoutKey::Left:
        case LayoutKey::Right: {
            if (gBBFocus < 0 || !gBBedit) return false; // sin OK previo las flechas son del viewport
            int dir = (key == LayoutKey::Right) ? +1 : -1;
            if (BBModeProp(vp)){ ProportionalRadiusScale(dir > 0 ? 1.1f : 1.0f / 1.1f); ProportionalReapply(); g_redraw = true; return true; }
            if (gBBFocus == 0){
                // el radio no tiene rango del que sacar el paso: px fijos, escalados como la UI
                float r = BrushGet().radiusPx + (float)(dir * 2 * GlobalScale);
                if (r < 0.0f) r = 0.0f;
                if (r > kBBRadiusMax) r = kBBRadiusMax;
                BrushGet().radiusPx = r;
            } else {
                // mismo paso que el slider de los menus: un veinteavo del rango = 5%
                float v = BrushGet().force + (float)dir * 0.05f;
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                BrushGet().force = v;
            }
            g_redraw = true;
            return true;
        }
        case LayoutKey::Cancel:
            if (gBBFocus < 0) return false;  // sin foco, el Cancel es del viewport (salir del modo, etc.)
            gBBFocus = -1; gBBedit = false;
            g_redraw = true;
            return true;
    }
    return false;
}

// dibuja UNA celda: track (+ relleno si tiene fraccion) + texto centrado. 'frac' < 0 = sin relleno
// (la barra del radio, que no tiene tope). Asume la matriz ya trasladada al origen de la fila.
static void BBrenderCell(Card* card, int cx, int cw, int h, float frac,
                          const std::string& txt, bool focused, const TextField* edit, float alpha){
    int barH = h - 2 * GlobalScale;
    if (barH < 1) barH = 1;
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)cx, (GLfloat)GlobalScale, 0);
    SetColorID(ColorID::headerColor, alpha);            // track (mismo color que el slider de los menus)
    card->Resize(cw, barH); card->RenderObject(false);
    if (frac >= 0.0f){
        int fillW = (int)(frac * (float)cw);
        if (fillW > 2){
            SetColorID(ColorID::accent, alpha);
            card->Resize(fillW, barH); card->RenderObject(false);
        }
    }
    if (focused){                                      // foco de teclado: borde accent (como la barra de arriba)
        SetColorID(ColorID::accent, 1.0f);
        card->Resize(cw, barH); card->RenderBorder(false);
    }
    w3dEngine::PopMatrix();

    // texto: el valor normal, o lo que se esta tipeando con el caret si esta en edicion numerica
    SetColorID(ColorID::white, 1.0f);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)cx, (GLfloat)((h - LetterHeightGS) / 2), 0);
    if (edit) RenderTextField(*edit, true, textAlign::center, cw);
    else RenderBitmapText(txt, textAlign::center, cw);
    w3dEngine::PopMatrix();
}

void ViewportBase::RenderBrushBar(){
    int h = BrushBarHeight();
    if (h <= 0 || !barCard) {
        // la fila dejo de estar (se salio del modo pintura, o el editor no la muestra): si quedaba
        // una edicion numerica DE ESTAS BARRAS abierta, cerrarla. Sino el campo -- ya invisible --
        // se queda con el teclado y el usuario no tiene donde apretar Enter para salir.
        if (g_propFloatEditing && (g_propFloatEditing == gBBpfRadius || g_propFloatEditing == gBBpfValue))
            NumEditCancel();
        return;
    }
    int yLocal = height - BBHeightToolbar(this) - h;   // local al viewport (la toolbar queda abajo)
    if (BBModeProp(this)){   // BARRA DE INFLUENCIA del proportional editing: una sola barra a todo el ancho
        int sx, sw; BBCellProp(this, sx, sw);
        char num[48]; sprintf(num, "%.2f", g_prop.radius);
        const std::string txt = std::string(T("Proportional Size")) + ": " + num + " m";
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(0, (GLfloat)yLocal, 0);
        const float* grayP = ColorList[static_cast<int>(ColorID::gray)];
        w3dEngine::Color4f(grayP[0], grayP[1], grayP[2], barAlpha);
        barCard->Resize(width, h);
        barCard->RenderObject(false);
        BBrenderCell(barCard, sx - x, sw, h, -1.0f, txt, false, NULL, barAlpha);
        w3dEngine::PopMatrix();
        return;
    }

    // textos: el MISMO formateo que usaban los botones ("40px" / "100%"), sin duplicar el sprintf
    std::string lSize, lForce, lMode, lGroup;
    WeightPaintLabels(NULL, lSize, lForce, lMode, lGroup);
    std::string txtRadius = std::string(T("Radius")) + ": " + lSize;
    std::string txtValue  = std::string(T("Value"))  + ": " + lForce;

    // que celda esta en edicion numerica (para mostrar lo tipeado + caret en vez del valor)
    bool edR = (gBBpfRadius && g_propFloatEditing == gBBpfRadius);
    bool edV = (gBBpfValue && g_propFloatEditing == gBBpfValue);

    int cx0, cx1, cw; BBCells(this, cx0, cx1, cw);
    bool focus = (this == viewPortActive) && gBBFocus >= 0; // el foco de teclado es de UN viewport

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)yLocal, 0);
    // fondo de la fila: el mismo gris translucido de la toolbar (se sigue viendo la escena atras)
    const float* gray = ColorList[static_cast<int>(ColorID::gray)];
    w3dEngine::Color4f(gray[0], gray[1], gray[2], barAlpha);
    barCard->Resize(width, h);
    barCard->RenderObject(false);
    // RADIO: sin relleno (no tiene tope) | VALOR: relleno 0..100%
    BBrenderCell(barCard, cx0 - x, cw, h, -1.0f, txtRadius, focus && gBBFocus == 0, edR ? &gBBpfRadius->field : NULL, barAlpha);
    BBrenderCell(barCard, cx1 - x, cw, h, BrushGet().force, txtValue, focus && gBBFocus == 1, edV ? &gBBpfValue->field : NULL, barAlpha);
    w3dEngine::PopMatrix();
}

void ViewportBase::RenderToolbar(){
    // la fila del pincel se dibuja SIEMPRE que corresponda, aunque la toolbar este oculta: sin ella
    // no hay como cambiar radio ni valor (misma excepcion que ya tenian los controles del pincel
    // frente al modo "usuario experimentado", ver ViewPort3D_Toolbar.cpp).
    RenderBrushBar();
    if (ToolButtons.empty() || !ToolbarVisible() || !barCard) return;
    ToolbarUpdate();
    int barH = ToolbarHeight();

    gfx::PushMatrix();
    gfx::Translatef(0, (GLfloat)(height - barH), 0);
    // fondo translucido como la barra de arriba (barAlpha del viewport)
    const float* gray = ColorList[static_cast<int>(ColorID::gray)];
    gfx::Color4f(gray[0], gray[1], gray[2], barAlpha);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);
    // las LINEAS que separan los grupos (Button::separador): verticales, en el medio del hueco de antes del boton
    { bool first = true;
      for (size_t i = 0; i < ToolButtons.size(); i++){
          Button* btn = ToolButtons[i];
          if (!btn->visible) continue;
          if (btn->separator && !first){
              const int lx = btn->sx - x - gapGS - (gapGS / 2 + 1) / 2;
              gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
              SetColorID(ColorID::grayUI, 0.6f);
              const int m = barH / 5;
              const GLfloat x0 = (GLfloat)lx, x1 = (GLfloat)(lx + GlobalScale), y0 = (GLfloat)m, y1 = (GLfloat)(barH - m);
              const GLfloat q[12] = { x0,y0, x1,y0, x1,y1,  x0,y0, x1,y1, x0,y1 };
              gfx::VertexPointer2f(0, q); gfx::DrawTrianglesArray(6);
              gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
          }
          first = false;
      } }
    // botones en sus sx/sy (ya con scroll). Local = sx-x, sy-y-(height-barH). Mismo patron que RenderBar.
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible) continue;
        btn->alpha = barAlpha;   // misma translucidez que el fondo de la barra
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)(btn->sx - x), (GLfloat)(btn->sy - y - (height - barH)), 0);
        // el enfocado con el teclado (softkey derecho): resaltado IGUAL que el de la barra de arriba (focusMenu)
        btn->focusMenu = (this == viewPortActive && (int)i == toolFocus);
        btn->Render();
        gfx::PopMatrix();
    }
    gfx::PopMatrix();
}
