// ===================================================================================================
//  TOOLBAR COMPARTIDA de los viewports (barra de HERRAMIENTAS de abajo). Mecanismo UNICO subido
//  desde ViewPort3D_Toolbar.cpp (donde nacio) a ViewportBase, para que el UV editor y el Editor 2D
//  tengan la misma barra sin duplicar nada:
//    - ToolButtons por ROL (btn->rol; reordenar no rompe el dispatch)
//    - layout con scroll horizontal clampeado (ToolbarActualizar)
//    - hit TOLERANTE al click (tap desviado agarra el boton mas cercano en X)
//    - render (fondo translucido + botones, mismo patron que RenderBar)
//    - MRU (ToolbarMRU): historial de acciones por editor, sin repetir, max 8
//  La parte CONTEXTUAL (que botones se ven y que hacen) es de cada editor, en dos virtuales:
//  ToolbarSincronizar() (estado puro, sin GL -> testeable headless) y ToolbarAccionRol(rol).
//  El Viewport3D la usa IGUAL que antes (cero regresion): sus overrides viven en ViewPort3D_Toolbar.cpp.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"     // T(): los textos salen en el idioma del sistema
#include "ViewPorts/ViewPorts.h"
#include "variables.h"   // enum TBMove..TBDelete (ids del historial de acciones)
#include "edit/WeightPaint.h"            // BrushGet() + WeightPaintLabels (el texto de las barras)
#include "WhiskUI/Propieties/PropFloat.h"// edicion numerica por teclado del valor (misma que el panel)
#include "ViewPorts/PopUp/NumPad.h"      // teclado virtual (tactil/Android)
#include "ViewPorts/TransformUI.h"       // ToolbarUsaTactil(): decide teclado fisico vs virtual
#include "ViewPorts/LayoutInput.h"       // LayoutKey (nav con el keypad de Symbian)
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
const char* ToolbarAccionLabel(int id){
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
// (el 3D: cfg.nuevoUsuario en Symbian; el UV: solo en edicion; el 2D: no en vista de juego).
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
    ToolbarActualizar(); // re-clampea contra el ancho total actual
    g_redraw = true;
}

// visibilidad CONTEXTUAL (virtual de cada editor) + layout comun: anchos, clamp del scroll y
// los sx/sy absolutos con el scroll aplicado. Mismo patron que la barra de arriba, en dos
// pasadas (primero el ancho total para clampear, despues las posiciones).
void ViewportBase::ToolbarActualizar(){
    ToolbarSincronizar(); // que se ve / que dice cada boton: lo decide el editor

    int barH = ToolbarHeight();
    int yBar = y + height - barH;
    int btnGap = gapGS / 2 + 1;
    int total = gapGS;
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible) continue;
        btn->Resize(width - gapGS * 2); // (btn->cuadrado fuerza ancho = alto, ej: X/Y/Z, Undo/Redo)
        total += btn->width + btnGap;
    }
    int maxS = total - width; if (maxS < 0) maxS = 0;
    if (toolScroll > maxS) toolScroll = maxS;
    int bx = gapGS - toolScroll;
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible){ btn->sx = -10000; btn->sy = -10000; btn->hover = false; continue; }
        btn->sx = x + bx;
        btn->sy = yBar + (barH - btn->height) / 2;
        btn->hover = btn->Contains(lastMouseX, lastMouseY);
        bx += btn->width + btnGap;
    }
}

bool ViewportBase::ToolbarClick(int mx, int my){
    if (!OnToolbar(mx, my)) return false;
    ToolbarActualizar(); // sx/sy frescos
    // Buscar el boton: match EXACTO, o si el toque cayo al lado (dedo gordo sobre botones chicos) el MAS
    // CERCANO en X dentro de una tolerancia. Asi un tap un poco desviado igual pulsa el boton que se queria
    // en vez de caer en el gap y no hacer nada. La barra es una franja horizontal -> decide el X.
    Button* target = NULL;
    for (size_t i = 0; i < ToolButtons.size(); i++){       // 1) match exacto
        Button* btn = ToolButtons[i];
        if (btn->visible && btn->Contains(mx, my)) { target = btn; break; }
    }
    if (!target){                                          // 2) sin exacto: el mas cercano en X
        int mejorD = 1 << 30;
        for (size_t i = 0; i < ToolButtons.size(); i++){
            Button* b = ToolButtons[i];
            if (!b->visible) continue;
            int cx = b->sx + b->width / 2;
            int d = (mx > cx) ? (mx - cx) : (cx - mx);
            if (d < mejorD) { mejorD = d; target = b; }
        }
        if (target && mejorD > target->width) target = NULL; // gap real (lejos): sin accion
    }
    if (target){
        ToolbarAccionRol(target->rol); // la accion es del editor (virtual)
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
//  campo se edita con el teclado fisico (g_textFieldActivo, igual que un campo del panel de
//  propiedades) y en tactil/Android se abre ademas el NumPad. Reusa PropFloat para no reimplementar
//  el parseo/commit/cancel (Enter y Esc ya estan ruteados para cualquier PropFloat en LayoutInput).
//
//  TECLADO (Symbian/keypad, sin mouse): arriba/abajo turnan entre las dos barras, OK entra a
//  ajustar y ahi izquierda/derecha mueven el valor, C lo suelta. Mismo contrato que el slider de
//  los menus (PopupMenu::AjustarSlider) y que el foco de barra del Timeline.
//
//  El estado del gesto es GLOBAL (no por viewport): el pincel es uno solo y solo se puede arrastrar
//  una barra a la vez -- misma decision que el drag del item-slider (MenuSliderDrag*).
// ===================================================================================================
static ViewportBase* gBBvp     = NULL;  // viewport de la barra que se esta arrastrando (NULL = ninguna)
static int   gBBidx    = -1;            // 0 = radio, 1 = valor
static int   gBBdownX  = 0, gBBdownY = 0;
static float gBBradio0 = 0.0f;          // radio al apoyar (el radio se arrastra RELATIVO a el)
static bool  gBBmovio  = false;         // ya paso el umbral -> es arrastre, no tap
static int   gBBfoco   = -1;            // teclado: barra enfocada (-1 = ninguna)
static bool  gBBedit   = false;         // teclado: OK apretado -> izq/der mueven el valor

// El PropFloat no edita el pincel DIRECTO sino estos espejos, por las unidades: la fuerza vive
// 0..1 y la barra se muestra en %, asi que tipear "32" tiene que dar 0.32 y no 32. El espejo se
// sincroniza antes de abrir la edicion y el onChange lo baja al pincel.
static float gBBradioPx  = 40.0f;
static float gBBvalorPct = 100.0f;
static PropFloat* gBBpfRadio = NULL;
static PropFloat* gBBpfValor = NULL;

static const float kBBradioMax = 4000.0f; // "sin tope" en la practica (mas grande que cualquier pantalla)

static void BBaplicarRadio(){ BrushGet().radioPx = gBBradioPx;         g_redraw = true; }
static void BBaplicarValor(){ BrushGet().fuerza  = gBBvalorPct*0.01f;  g_redraw = true; }

// el PropFloat de la barra 'idx', creado al vuelo y con el espejo YA sincronizado con el pincel
static PropFloat* BBprop(int idx){
    if (idx == 0){
        if (!gBBpfRadio){
            gBBpfRadio = new PropFloat(T("Radius"), "px");
            gBBpfRadio->value = &gBBradioPx;
            gBBpfRadio->entero = true;              // el radio es px enteros
            gBBpfRadio->SetRango(0.0f, kBBradioMax); // minimo 0px; el maximo es solo un tope de cordura
            gBBpfRadio->onChange = BBaplicarRadio;
        }
        gBBradioPx = BrushGet().radioPx;
        return gBBpfRadio;
    }
    if (!gBBpfValor){
        gBBpfValor = new PropFloat(T("Value"), "%");
        gBBpfValor->value = &gBBvalorPct;
        gBBpfValor->entero = true;                  // 0..100 en enteros: "32%" y no "32.4171%"
        gBBpfValor->SetRango(0.0f, 100.0f);
        gBBpfValor->onChange = BBaplicarValor;
    }
    gBBvalorPct = BrushGet().fuerza * 100.0f;
    return gBBpfValor;
}

// alto REAL que ocupa la toolbar de este viewport (0 si no se dibuja): la fila del pincel se
// apoya arriba de ella, y si la toolbar esta oculta (Symbian sin cfg.nuevoUsuario) baja al piso.
static int BBaltoToolbar(const ViewportBase* vp){
    if (!vp || vp->ToolButtons.empty() || !vp->ToolbarVisible()) return 0;
    return vp->ToolbarHeight();
}

int ViewportBase::BrushBarHeight() const {
    return BrushBarVisible() ? BarHeight() : 0;
}

// las dos celdas: mitad y mitad, con el mismo gap de la toolbar como margen y separacion
static void BBceldas(const ViewportBase* vp, int& cx0, int& cx1, int& cw){
    cw  = (vp->width - gapGS * 3) / 2;
    if (cw < 1) cw = 1;
    cx0 = vp->x + gapGS;
    cx1 = cx0 + cw + gapGS;
}

bool ViewportBase::OnBrushBar(int px, int py) const {
    int h = BrushBarHeight();
    if (h <= 0) return false;
    int yTop = y + height - BBaltoToolbar(this) - h;
    return px >= x && px < x + width && py >= yTop && py < yTop + h;
}

// mapeo ABSOLUTO de la barra del VALOR: la X dentro de la celda es el porcentaje (0..100)
static void BBvalorDesdeX(const ViewportBase* vp, int mx){
    int cx0, cx1, cw; BBceldas(vp, cx0, cx1, cw);
    float frac = (float)(mx - cx1) / (float)cw;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    BrushGet().fuerza = frac;
}

bool ViewportBase::BrushBarClick(int mx, int my){
    if (!OnBrushBar(mx, my)) return false;
    // habia una edicion numerica abierta (esta barra u otro campo): tocar afuera la CONFIRMA,
    // igual que en el panel de propiedades. Sino quedaba un campo a medio tipear invisible.
    if (NumEditActivo()) NumEditCommit();
    int cx0, cx1, cw; BBceldas(this, cx0, cx1, cw);
    gBBvp    = this;
    gBBidx   = (mx < cx1) ? 0 : 1;   // el gap del medio cae del lado del radio: da igual, no hay valor ahi
    gBBdownX = mx; gBBdownY = my;
    gBBradio0 = BrushGet().radioPx;
    gBBmovio = false;
    // el foco de TECLADO no se toca aca a proposito: es del keypad (Symbian). Si el click lo
    // moviera, en PC quedaria una barra con el borde accent encendido para siempre despues de
    // tocarla una vez, que se lee como "seleccionada" y no lo esta.
    // OJO: el DOWN no cambia el valor. Si lo cambiara, un tap (que es "editar exacto por teclado")
    // primero pegaria un salto al valor de donde cayo el dedo. El valor se mueve recien al ARRASTRAR.
    g_redraw = true;
    return true;
}

bool BrushBarDragActivo(){ return gBBvp != NULL; }

// el viewport que se estaba arrastrando se DESTRUYO (se cerro el panel / cambio el layout en medio
// del gesto): olvidarlo sin pasar por el camino de "tap" (no hay barra a la que abrirle el teclado).
void BrushBarOlvidarViewport(ViewportBase* vp){
    if (gBBvp == vp){ gBBvp = NULL; gBBidx = -1; gBBmovio = false; }
}

void BrushBarDragMover(int mx, int my){
    if (!gBBvp) return;
    if (!gBBmovio){
        int umbral = 6 * GlobalScale; // el mismo del drag-scroll de los menus
        if (abs(mx - gBBdownX) < umbral && abs(my - gBBdownY) < umbral) return; // todavia puede ser un tap
        gBBmovio = true;
    }
    if (gBBidx == 0){
        float r = gBBradio0 + (float)(mx - gBBdownX); // RELATIVO: no hay tope al que mapear
        if (r < 0.0f) r = 0.0f;
        if (r > kBBradioMax) r = kBBradioMax;
        BrushGet().radioPx = r;
    } else {
        BBvalorDesdeX(gBBvp, mx);                     // ABSOLUTO: donde tocas, ese valor
    }
    g_redraw = true;
}

void BrushBarDragSoltar(){
    if (!gBBvp) return;
    if (!gBBmovio){
        // TAP / click simple: edicion numerica EXACTA. En PC alcanza el campo inline (el teclado
        // fisico entra por g_textFieldActivo); en tactil hace falta ademas el teclado en pantalla.
        PropFloat* pf = BBprop(gBBidx);
        if (pf){
            pf->IniciarEdicionTexto();
            if (ToolbarUsaTactil()) NumPadAbrir();
        }
    }
    gBBvp = NULL; gBBidx = -1; gBBmovio = false;
    g_redraw = true;
}

int  BrushBarFoco(){ return gBBfoco; }
bool BrushBarEditando(){ return gBBedit; }
void BrushBarSoltarFoco(){ gBBfoco = -1; gBBedit = false; }

bool BrushBarTecla(ViewportBase* vp, int tecla){
    if (!vp || !vp->BrushBarVisible()) return false;
    switch (tecla){
        case LayoutKey::Up:
        case LayoutKey::Down:
            // turnar entre las dos barras (la primera vez entra en la de arriba a la izquierda)
            gBBfoco = (gBBfoco < 0) ? 0 : (gBBfoco == 0 ? 1 : 0);
            gBBedit = false;
            g_redraw = true;
            return true;
        case LayoutKey::Enter:
            if (gBBfoco < 0) gBBfoco = 0;   // OK sin foco: entra a la primera
            else gBBedit = !gBBedit;        // OK con foco: entra/sale del ajuste con izq/der
            g_redraw = true;
            return true;
        case LayoutKey::Left:
        case LayoutKey::Right: {
            if (gBBfoco < 0 || !gBBedit) return false; // sin OK previo las flechas son del viewport
            int dir = (tecla == LayoutKey::Right) ? +1 : -1;
            if (gBBfoco == 0){
                // el radio no tiene rango del que sacar el paso: px fijos, escalados como la UI
                float r = BrushGet().radioPx + (float)(dir * 2 * GlobalScale);
                if (r < 0.0f) r = 0.0f;
                if (r > kBBradioMax) r = kBBradioMax;
                BrushGet().radioPx = r;
            } else {
                // mismo paso que el slider de los menus: un veinteavo del rango = 5%
                float v = BrushGet().fuerza + (float)dir * 0.05f;
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                BrushGet().fuerza = v;
            }
            g_redraw = true;
            return true;
        }
        case LayoutKey::Cancel:
            if (gBBfoco < 0) return false;  // sin foco, el Cancel es del viewport (salir del modo, etc.)
            gBBfoco = -1; gBBedit = false;
            g_redraw = true;
            return true;
    }
    return false;
}

// dibuja UNA celda: track (+ relleno si tiene fraccion) + texto centrado. 'frac' < 0 = sin relleno
// (la barra del radio, que no tiene tope). Asume la matriz ya trasladada al origen de la fila.
static void BBrenderCelda(Card* card, int cx, int cw, int h, float frac,
                          const std::string& txt, bool enfocada, bool editando,
                          const std::string& textoEdit, float alpha){
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
    if (enfocada){                                      // foco de teclado: borde accent (como la barra de arriba)
        SetColorID(ColorID::accent, 1.0f);
        card->Resize(cw, barH); card->RenderBorder(false);
    }
    w3dEngine::PopMatrix();

    // texto: el valor normal, o lo que se esta tipeando con el caret si esta en edicion numerica
    SetColorID(editando ? ColorID::accent : ColorID::blanco, 1.0f);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)cx, (GLfloat)((h - LetterHeightGS) / 2), 0);
    RenderBitmapText(editando ? textoEdit : txt, textAlign::center, cw);
    w3dEngine::PopMatrix();
}

void ViewportBase::RenderBrushBar(){
    int h = BrushBarHeight();
    if (h <= 0 || !barCard) {
        // la fila dejo de estar (se salio del modo pintura, o el editor no la muestra): si quedaba
        // una edicion numerica DE ESTAS BARRAS abierta, cerrarla. Sino el campo -- ya invisible --
        // se queda con el teclado y el usuario no tiene donde apretar Enter para salir.
        if (g_propFloatEditando && (g_propFloatEditando == gBBpfRadio || g_propFloatEditando == gBBpfValor))
            NumEditCancel();
        return;
    }
    int yLocal = height - BBaltoToolbar(this) - h;   // local al viewport (la toolbar queda abajo)

    // textos: el MISMO formateo que usaban los botones ("40px" / "100%"), sin duplicar el sprintf
    std::string lTam, lFuerza, lModo, lGrupo;
    WeightPaintLabels(NULL, lTam, lFuerza, lModo, lGrupo);
    std::string txtRadio = std::string(T("Radius")) + ": " + lTam;
    std::string txtValor = std::string(T("Value"))  + ": " + lFuerza;

    // que celda esta en edicion numerica (para mostrar lo tipeado + caret en vez del valor)
    bool edR = (gBBpfRadio && g_propFloatEditando == gBBpfRadio);
    bool edV = (gBBpfValor && g_propFloatEditando == gBBpfValor);
    std::string txtEdR, txtEdV;
    if (edR) txtEdR = gBBpfRadio->field.selectAll ? gBBpfRadio->field.text
                    : gBBpfRadio->field.text.substr(0, gBBpfRadio->field.caret) + "|" +
                      gBBpfRadio->field.text.substr(gBBpfRadio->field.caret);
    if (edV) txtEdV = gBBpfValor->field.selectAll ? gBBpfValor->field.text
                    : gBBpfValor->field.text.substr(0, gBBpfValor->field.caret) + "|" +
                      gBBpfValor->field.text.substr(gBBpfValor->field.caret);

    int cx0, cx1, cw; BBceldas(this, cx0, cx1, cw);
    bool foco = (this == viewPortActive) && gBBfoco >= 0; // el foco de teclado es de UN viewport

    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)yLocal, 0);
    // fondo de la fila: el mismo gris translucido de la toolbar (se sigue viendo la escena atras)
    const float* gris = ListaColores[static_cast<int>(ColorID::gris)];
    w3dEngine::Color4f(gris[0], gris[1], gris[2], barAlpha);
    barCard->Resize(width, h);
    barCard->RenderObject(false);
    // RADIO: sin relleno (no tiene tope) | VALOR: relleno 0..100%
    BBrenderCelda(barCard, cx0 - x, cw, h, -1.0f, txtRadio, foco && gBBfoco == 0, edR, txtEdR, barAlpha);
    BBrenderCelda(barCard, cx1 - x, cw, h, BrushGet().fuerza, txtValor, foco && gBBfoco == 1, edV, txtEdV, barAlpha);
    w3dEngine::PopMatrix();
}

void ViewportBase::RenderToolbar(){
    // la fila del pincel se dibuja SIEMPRE que corresponda, aunque la toolbar este oculta: sin ella
    // no hay como cambiar radio ni valor (misma excepcion que ya tenian los controles del pincel
    // frente al modo "usuario experimentado", ver ViewPort3D_Toolbar.cpp).
    RenderBrushBar();
    if (ToolButtons.empty() || !ToolbarVisible() || !barCard) return;
    ToolbarActualizar();
    int barH = ToolbarHeight();

    gfx::PushMatrix();
    gfx::Translatef(0, (GLfloat)(height - barH), 0);
    // fondo translucido como la barra de arriba (barAlpha del viewport)
    const float* gris = ListaColores[static_cast<int>(ColorID::gris)];
    gfx::Color4f(gris[0], gris[1], gris[2], barAlpha);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);
    // botones en sus sx/sy (ya con scroll). Local = sx-x, sy-y-(height-barH). Mismo patron que RenderBar.
    for (size_t i = 0; i < ToolButtons.size(); i++){
        Button* btn = ToolButtons[i];
        if (!btn->visible) continue;
        btn->alpha = barAlpha;   // misma translucidez que el fondo de la barra
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)(btn->sx - x), (GLfloat)(btn->sy - y - (height - barH)), 0);
        btn->Render();
        gfx::PopMatrix();
    }
    gfx::PopMatrix();
}
