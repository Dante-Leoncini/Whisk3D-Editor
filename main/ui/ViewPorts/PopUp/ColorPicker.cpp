#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "ColorPicker.h"
#include "W3dPalettes.h"      // pestania "Pal": paleta efectiva + paletas del proyecto
#include "objects/Objects.h"  // Object::paleta (la seleccion del objeto)
#include "ViewPorts/LayoutInput.h" // LayoutKey
#include "WhiskUI/core/UI.h"
#include "WhiskUI/text/bitmapText.h"
#include "objects/Textures.h"
#include "Undo.h" // Ctrl+Z: captura el cambio de color al cerrar
#include "input/W3dClipboard.h"   // pestania Hex: Copiar / Pegar
#include "W3dNotice.h"            // Notificar (el portapapeles no tiene un color)
#include <cstdio>
#include <cmath>

ColorPicker* colorPicker = NULL;

// ============================================================================
//  EL PICKER NO PUEDE SOBREVIVIR A SU DUENO
//
//  El popup se queda con tres punteros ADENTRO del elemento que edita: 'target' (el float[4]
//  del color, que ESCRIBE en vivo), 'palRef' (el campo indice pal* del elemento) y 'palObj'
//  (el elemento). Ninguno es una copia: son memoria del objeto. Si ese objeto se borra, son
//  punteros muertos, y con la direccion reciclada por el objeto siguiente el picker pasa a
//  escribir el color / la paleta de OTRO elemento, en silencio (la familia de siempre).
//  Dos agujeros y dos tapas:
//   - CERRADO: Close() no los limpiaba, asi que quedaban colgados hasta la proxima apertura
//     (Abrir los resetea). Ahora se sueltan al cerrar: el estado no sobrevive a la sesion de
//     edicion que lo creo.
//   - ABIERTO: si borran el objeto con el picker abierto no alcanza con limpiar al cerrar
//     (justamente, Close() ESCRIBE en 'target' para capturar el paso de undo). Por eso se
//     engancha en el aviso de destruccion de Object (W3dUnlinkRegister, Objects.h):
//     el picker suelta todo y se cierra sin tocar la memoria del muerto.
// ============================================================================
static void ColorPickerUnlink(Object* deleted){
    if (!colorPicker || !deleted) return;
    if (colorPicker->palObj != deleted) return;   // no es el elemento que estamos editando
    colorPicker->ForgetOwner();
}
struct ColorPickerHook { ColorPickerHook(){ W3dUnlinkRegister(ColorPickerUnlink); } };
static ColorPickerHook g_colorPickerHook;

// suelta TODO lo que apunta adentro del elemento y cierra el popup si estaba abierto. No
// desreferencia nada (el dueno se esta destruyendo): ni captura el paso de undo del color,
// que necesitaria leer 'target'.
void ColorPicker::ForgetOwner(){
    target = NULL; palRef = NULL; palObj = NULL; onPaletteChange = NULL;
    nTabs = 3; if (tab == 3) tab = 1;   // sin paleta: vuelve a HSV
    if (PopUpActive == this) PopUpActive = NULL;
}

// 0 = 0..255, 1 = 0..100% (persistente entre aperturas del picker)
int ColorPickerUnit = 0;

// "0.000".."1.000" sin %f (la estlib de Symbian no lo tiene)
static void FormatFloat3(char* buf, float v) {
    int thousand = (int)(v * 1000.0f + 0.5f);
    if (thousand >= 1000) {
        buf[0] = '1'; buf[1] = '.'; buf[2] = buf[3] = buf[4] = '0'; buf[5] = 0;
    } else {
        sprintf(buf, "0.%03d", thousand);
    }
}

extern bool leftMouseDown;

// ---------------- conversiones HSV <-> RGB (0..1) ----------------

static void HSVaRGB(float h, float s, float v, float* r, float* g, float* b) {
    if (s <= 0.0f) { *r = *g = *b = v; return; }
    h = h - (float)floor(h); // wrap 0..1
    float hh = h * 6.0f;
    int i = (int)hh;
    if (i > 5) i = 5;
    float f = hh - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    switch (i) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

static void RGBaHSV(float r, float g, float b, float* h, float* s, float* v) {
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float d = mx - mn;
    *v = mx;
    *s = (mx <= 0.0f) ? 0.0f : d / mx;
    if (d <= 0.0f) { *h = 0.0f; return; }
    float hh;
    if (mx == r) hh = (g - b) / d;
    else if (mx == g) hh = 2.0f + (b - r) / d;
    else hh = 4.0f + (r - g) / d;
    hh /= 6.0f;
    if (hh < 0.0f) hh += 1.0f;
    *h = hh;
}

// ----------------------------------------------------------------

ColorPicker::ColorPicker() : PopUpBase(T("Color")) {
    // (eran inicializadores de clase: C++03)
    target = NULL;
    original[0] = original[1] = original[2] = original[3] = 1.0f;
    h = 0.0f;
    s = 0.0f;
    v = 1.0f;
    tab = 1;                    // arranca en HSV (pedido del dueno): es como se piensa un color
    tabs[0] = new Tab("RGB");
    tabs[1] = new Tab("HSV");
    tabs[2] = new Tab("Hex");
    tabs[3] = new Tab("Pal");   // la pestania PALETA (solo con contexto: SetPalette)
    nTabs = 3;
    palRef = NULL; palObj = NULL; onPaletteChange = NULL;
    palScroll = 0; palVis = 0; palRowH = 10;
    row = 0;
    okFocus = 0; holdDir = 0; holdCount = 0; editCircle = false; editValue = false;
    drag = 0;
    moved = false;
    dragX = 0;
    rect = new Rec2D();
    rowCard = new Card(NULL, 10, 10);
    btnOk = new Button(T("OK"));
    btnCancel = new Button(T("Cancel"));
    btnUnit   = new Button(T("Switch to 0-100%"));
    btnCopy   = new Button(T("Copy"));
    btnPaste = new Button(T("Paste"));
    btnCopy->adapt = false; btnPaste->adapt = false;
    btnCopy->centered = true; btnPaste->centered = true;
    hexBtnY = 0;
    btnOk->adapt = false;
    btnCancel->adapt = false;
    btnUnit->adapt   = false;
    btnOk->centered = true;
    btnCancel->centered = true;
    btnUnit->centered   = true;
    btnY = 0;
    btnOkY = 0;
    unitX = 0;
    circX = circY = 0;
    circSide = 10;
    barX = 0;
    barW = 10;
    prevX = 0;
    prevW = 10;
    tabsY = 0;
    tabHeight = 10;
    rowsY = 0;
}

ColorPicker::~ColorPicker() {
    delete rect;
    delete rowCard;
    delete btnOk;
    delete btnCancel;
    delete btnUnit;
    delete btnCopy;
    delete btnPaste;
    delete tabs[0];
    delete tabs[1];
    delete tabs[2];
    delete tabs[3];
}

int ColorPicker::Rows() const {
    if (tab == 3)   // Paleta: [Propio + colores] + [Igual que el padre + paletas]
        return PalColorCount() + PalPaletteCount() + 2;
    return (tab == 2) ? 1 : 4; // Hex es una sola fila
}

// ---------------- pestania PALETA ----------------

int ColorPicker::PalColorCount() const {
    std::vector<PaletteColor>* cs = palObj ? W3dEffectiveColors(palObj) : NULL;
    return cs ? (int)cs->size() : 0;
}

int ColorPicker::PalPaletteCount() const {
    return (int)W3dPalettes().size();
}

// fila SELECCIONABLE k -> linea de display (las 2 cabeceras no se seleccionan):
// linea 0 = cabecera "Colores"; 1..1+nC = Propio + colores;
// linea 2+nC = cabecera "Paletas"; 3+nC.. = Igual que el padre + paletas
int ColorPicker::PalRowLine(int k) const {
    int nC = PalColorCount();
    return (k <= nC) ? 1 + k : 2 + k;
}

void ColorPicker::PalEnsureVisible() {
    if (tab != 3 || row < 0) return;
    int d = PalRowLine(row);
    int nC = PalColorCount();
    // al pararse en la PRIMERA fila de una seccion conviene ver su cabecera
    if (row == 0) d = 0;
    else if (row == nC + 1) d = d - 1;
    if (d < palScroll) palScroll = d;
    int dRow = PalRowLine(row);
    if (dRow >= palScroll + palVis) palScroll = dRow - palVis + 1;
}

// ejecuta la fila: 0 = color PROPIO; 1..nC = referenciar ese INDICE de la
// paleta efectiva; nC+1 = heredar del padre; nC+2.. = elegir esa PALETA para
// el objeto (el swap: sus herederos se re-pintan al instante)
void ColorPicker::PalApplyRow(int k) {
    if (!palRef || !palObj) return;
    int nC = PalColorCount();
    std::vector<Palette>& ps = W3dPalettes();
    if (k == 0) {
        *palRef = -1;
    } else if (k <= nC) {
        *palRef = k - 1;
    } else if (k == nC + 1) {
        palObj->palette.clear();
    } else {
        int j = k - nC - 2;
        if (j >= 0 && j < (int)ps.size()) palObj->palette = ps[j].name;
    }
    if (onPaletteChange) onPaletteChange();
    // cambiar la paleta puede cambiar la cantidad de colores efectivos
    if (row >= Rows()) row = Rows() > 0 ? Rows() - 1 : 0;
    Reflow();
}

void ColorPicker::FromRGB() {
    if (!target) return;
    RGBaHSV(target[0], target[1], target[2], &h, &s, &v);
}

void ColorPicker::ToRGB() {
    if (!target) return;
    HSVaRGB(h, s, v, &target[0], &target[1], &target[2]);
}

void ColorPicker::Open(GLfloat* Target, int px, int py) {
    target = Target;
    if (target) {
        original[0] = target[0];
        original[1] = target[1];
        original[2] = target[2];
        original[3] = target[3];
    }
    FromRGB();
    row = 0;                // foco inicial: primera fila de valor
    okFocus = 0;            // OK por defecto
    editCircle = false; editValue = false;
    holdDir = holdCount = 0;
    drag = 0;
    // SIN contexto de paleta hasta que el panel lo setee (SetPalette despues de Abrir)
    palRef = NULL; palObj = NULL; onPaletteChange = NULL;
    nTabs = 3; palScroll = 0;
    if (tab == 3) tab = 1;   // la pestania Pal quedo de otra apertura: vuelve a HSV

    x = px;
    y = py;
    Reflow();
    PopUpActive = this;
}

// habilita la pestania "Pal": ref = el campo indice pal* del elemento (-1 =
// color propio), obj = el elemento (su paleta efectiva y su seleccion),
// onChange = aviso al panel (re-bind + redraw). Llamar DESPUES de Abrir.
void ColorPicker::SetPalette(int* ref, Object* obj, void (*onChange)()) {
    palRef = ref; palObj = obj; onPaletteChange = onChange;
    nTabs = (ref && obj) ? 4 : 3;
    // si el campo YA referencia un indice, abrir directo mostrando la paleta
    if (nTabs == 4 && *ref >= 0) { tab = 3; row = 0; palScroll = 0; }
    Reflow();
}

// layout (pixeles del popup, escalados). El ALTO depende de la pestania: la
// lista de la pestania Paleta crece con los colores/paletas (con tope de
// pantalla + scroll); las otras conservan su geometria de siempre.
void ColorPicker::Reflow() {
    int pad = borderGS + GlobalScale * 3;       // padding exterior CHICO (~3px de aire; antes marginGS = demasiado)
    circSide = 48 * GlobalScale;
    circX = pad;
    circY = borderGS + RowHeightGS + gapGS; // bajo el titulo
    barX = circX + circSide + gapGS * 2;
    barW = 10 * GlobalScale;
    prevX = barX + barW + gapGS * 2;
    prevW = 22 * GlobalScale;

    // pestanias RGB / HSV / Hex (+ Pal) (se funden con las filas de abajo)
    for (int i = 0; i < nTabs; i++) tabs[i]->Resize(100000);
    tabHeight = tabs[0]->height;
    tabsY = circY + circSide + gapGS * 2;
    rowsY = tabsY + tabHeight; // sin gap: la pestania se funde

    int w = prevX + prevW + pad;
    int wTabs = pad * 2 + gapGS * (nTabs - 1);
    for (int i = 0; i < nTabs; i++) wTabs += tabs[i]->width;
    if (w < wTabs) w = wTabs;
    // los 3 valores juntos en una tarjeta (margen de 1px entre rellenos)
    groupH = 3 * RowHeightGS + 4 * GlobalScale;
    if (tab == 3) {
        // la LISTA: 2 cabeceras + filas seleccionables; si no entra en la
        // pantalla se recorta y scrollea (el foco de teclado la sigue)
        palRowH = RowHeightGS + GlobalScale * 2;
        int lines = Rows() + 2;
        int fixed = rowsY + gapGS + gapGS + (RowHeightGS + bordersGS) + pad + gapGS;
        int maxLines = (MenuScreenH - fixed) / palRowH;
        if (maxLines < 3) maxLines = 3;
        palVis = (lines < maxLines) ? lines : maxLines;
        int maxScroll = lines - palVis; if (maxScroll < 0) maxScroll = 0;
        if (palScroll > maxScroll) palScroll = maxScroll;
        if (palScroll < 0) palScroll = 0;
        alphaY = 0;   // sin fila de alpha ni boton de unidad en esta pestania
        btnY = rowsY + gapGS + palVis * palRowH + gapGS;
        btnOkY = btnY;
    } else {
        alphaY = rowsY + gapGS + groupH + gapGS;
        hexBtnY = rowsY + gapGS + RowHeightGS + GlobalScale * 2 + gapGS;   // (Hex: debajo de su unica fila)
        // boton de unidad (ancho completo) y abajo OK / Cancel al 50% cada uno
        btnY = alphaY + RowHeightGS + GlobalScale * 2 + gapGS;
        btnOkY = btnY + (RowHeightGS + bordersGS) + gapGS;
    }
    int hTotal = btnOkY + (RowHeightGS + bordersGS) + pad;
    popUpWindow->Resize(w, hTotal);

    // posicion clampeada a la pantalla
    if (x + w > MenuScreenW) x = MenuScreenW - w;
    if (y + hTotal > MenuScreenH) y = MenuScreenH - hTotal;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
}

// Ctrl+Z: al cerrar (acepta O cancela), captura el cambio de color. Cancelar restaura 'original' ANTES de
// llamar Cerrar -> target == original -> UndoCaptureColor no pushea. Aceptar deja target cambiado -> pushea.
void ColorPicker::Close() {
    if (target) UndoCaptureColor(target, original);
    if (g_activeTextField == &hexField) g_activeTextField = NULL;   // (la caja del hex no sobrevive al popup)
    editCircle = false; editValue = false; // por si quedo editando
    // SOLTAR lo que apunta adentro del elemento: 'target' (el float[4] del color), 'palRef' (su
    // campo indice) y 'palObj' (el elemento). Nada de eso puede sobrevivir al cierre: el objeto
    // se puede borrar despues y la direccion se recicla (ver el bloque de arriba). Open() los
    // vuelve a poner; la pestania Pal se apaga porque ya no hay contexto de paleta.
    target = NULL; palRef = NULL; palObj = NULL; onPaletteChange = NULL;
    nTabs = 3; if (tab == 3) tab = 1;   // sin paleta: vuelve a HSV
    PopUpBase::Close();
}

// ---------------- UV del circulo cromatico (ver ColorPicker.h) ----------------
// arte de 32x32 en el (96,70) de font.png; font.png esta en el (0,0) del atlas
static const float kWheelPx = 32.0f, kWheelX = 96.0f, kWheelY = 70.0f;
static float gWheelU0 = kWheelX / 128.0f,                 gWheelV0 = kWheelY / 128.0f;
static float gWheelU1 = (kWheelX + kWheelPx) / 128.0f,    gWheelV1 = (kWheelY + kWheelPx) / 128.0f;

void CalcColorWheelUV(int texW, int texH) {
    if (texW <= 0 || texH <= 0) return;
    gWheelU0 = kWheelX / (float)texW;  gWheelU1 = (kWheelX + kWheelPx) / (float)texW;
    gWheelV0 = kWheelY / (float)texH;  gWheelV1 = (kWheelY + kWheelPx) / (float)texH;
}

void ColorWheelUV(float* u0, float* v0, float* u1, float* v1) {
    if (u0) *u0 = gWheelU0; if (v0) *v0 = gWheelV0;
    if (u1) *u1 = gWheelU1; if (v1) *v1 = gWheelV1;
}

// marco de foco (4 rect finos accent) para resaltar el circulo / barra de valor enfocados con el teclado.
// Texture2D debe estar apagado (se llama en la zona plana del render). grosor en GlobalScale.
static void FrameFocus(Rec2D* r, int x, int y, int w, int h, int thickness) {
    const float* a = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(a[0], a[1], a[2], 1.0f);
    r->SetSize((GLshort)x, (GLshort)y, (GLshort)w, (GLshort)thickness); r->RenderObject(false);             // arriba
    r->SetSize((GLshort)x, (GLshort)(y + h - thickness), (GLshort)w, (GLshort)thickness); r->RenderObject(false); // abajo
    r->SetSize((GLshort)x, (GLshort)y, (GLshort)thickness, (GLshort)h); r->RenderObject(false);             // izquierda
    r->SetSize((GLshort)(x + w - thickness), (GLshort)y, (GLshort)thickness, (GLshort)h); r->RenderObject(false); // derecha
}

void ColorPicker::Render() {
    if (!target) return;
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);

    const float* gray = ColorList[static_cast<int>(ColorID::gray)];
    const float* background = ColorList[static_cast<int>(ColorID::background)];
    const float* white  = ColorList[static_cast<int>(ColorID::white)];
    const float* grayUI = ColorList[static_cast<int>(ColorID::grayUI)];
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];

    // ventana con BORDE VERDE (el popup activo es el foco)
    w3dEngine::Color4f(gray[0], gray[1], gray[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // titulo
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)borderGS, 0);
    w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
    RenderBitmapText(name, textAlign::left, popUpWindow->width - bordersGS);
    w3dEngine::PopMatrix();

    // circulo cromatico: arte de 32x32 en (96,70) del atlas de 128x128
    {
        GLshort cv[8];
        GLfloat cuv[8];
        cv[0] = (GLshort)circX;              cv[1] = (GLshort)circY;
        cv[2] = (GLshort)(circX + circSide); cv[3] = (GLshort)circY;
        cv[4] = (GLshort)circX;              cv[5] = (GLshort)(circY + circSide);
        cv[6] = (GLshort)(circX + circSide); cv[7] = (GLshort)(circY + circSide);
        float u0, v0, u1, v1;
        ColorWheelUV(&u0, &v0, &u1, &v1);   // contra el tamano REAL del atlas (ver ColorPicker.h)
        cuv[0] = u0; cuv[1] = v0;
        cuv[2] = u1; cuv[3] = v0;
        cuv[4] = u0; cuv[5] = v1;
        cuv[6] = u1; cuv[7] = v1;
        w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        W3dDrawStrip4(cv, cuv);
    }

    // las partes planas van sin textura
    w3dEngine::Disable(w3dEngine::Texture2D);

    // marcador H/S sobre el circulo. La rueda del arte tiene el ROJO
    // ABAJO y el matiz crece en sentido HORARIO (verde arriba-izquierda,
    // azul arriba-derecha)
    {
        float ang = -1.5707963f - h * 6.2831853f;
        float rad = s * (circSide * 0.5f);
        int mxp = circX + circSide / 2 + (int)(cosf(ang) * rad);
        int myp = circY + circSide / 2 - (int)(sinf(ang) * rad);
        w3dEngine::Color4f(0, 0, 0, 1.0f);
        rect->SetSize((GLshort)(mxp - 2 * GlobalScale), (GLshort)(myp - 2 * GlobalScale),
                      (GLshort)(4 * GlobalScale), (GLshort)(4 * GlobalScale));
        rect->RenderObject(false);
        w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
        rect->SetSize((GLshort)(mxp - GlobalScale), (GLshort)(myp - GlobalScale),
                      (GLshort)(2 * GlobalScale), (GLshort)(2 * GlobalScale));
        rect->RenderObject(false);
    }

    // foco de teclado en el CIRCULO (verde); mas grueso si esta en modo edicion (izq/der=Hue, arr/aba=Sat)
    if (row == -3) FrameFocus(rect, circX, circY, circSide, circSide, editCircle ? 2 * GlobalScale : GlobalScale);

    // barra de VALOR: un solo quad con VERTEX COLOR (los dos vertices
    // de arriba con V=1 y los de abajo con V=0): el degradado lo mezcla
    // el hardware automaticamente
    {
        static GLfloat bv[12];
        static GLubyte bc[24];
        float r1, g1, b1;
        HSVaRGB(h, s, 1.0f, &r1, &g1, &b1);
        GLubyte cr = (GLubyte)(r1 * 255.0f);
        GLubyte cg = (GLubyte)(g1 * 255.0f);
        GLubyte cb = (GLubyte)(b1 * 255.0f);
        float x0 = (float)barX;
        float x1 = (float)(barX + barW);
        float y0 = (float)circY;
        float y1 = (float)(circY + circSide);
        // dos triangulos: TL TR BL / TR BR BL
        bv[0] = x0; bv[1] = y0;   bv[2] = x1; bv[3] = y0;
        bv[4] = x0; bv[5] = y1;   bv[6] = x1; bv[7] = y0;
        bv[8] = x1; bv[9] = y1;   bv[10] = x0; bv[11] = y1;
        static const int up[6] = { 1, 1, 0, 1, 0, 0 };
        for (int vtx = 0; vtx < 6; vtx++) {
            bc[vtx * 4 + 0] = up[vtx] ? cr : 0;
            bc[vtx * 4 + 1] = up[vtx] ? cg : 0;
            bc[vtx * 4 + 2] = up[vtx] ? cb : 0;
            bc[vtx * 4 + 3] = 255;
        }
        w3dEngine::EnableArray(w3dEngine::ColorArray);
        w3dEngine::ColorPointer4ub(bc);
        w3dEngine::VertexPointer2f(0, bv);
        w3dEngine::TexCoordPointer2f(8, bv); // dummy (textura apagada)
        w3dEngine::DrawTrianglesArray(6);
        w3dEngine::DisableArray(w3dEngine::ColorArray);

        // marcador de V: linea con contorno (antes parecia un
        // rectangulo blanco suelto)
        int myp = circY + (int)((1.0f - v) * (circSide - 2 * GlobalScale));
        w3dEngine::Color4f(0, 0, 0, 1.0f);
        rect->SetSize((GLshort)barX, (GLshort)(myp - GlobalScale),
                      (GLshort)barW, (GLshort)(4 * GlobalScale));
        rect->RenderObject(false);
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        rect->SetSize((GLshort)barX, (GLshort)myp,
                      (GLshort)barW, (GLshort)(2 * GlobalScale));
        rect->RenderObject(false);
    }

    // foco de teclado en la BARRA DE VALOR (verde)
    if (row == -2) FrameFocus(rect, barX, circY, barW, circSide, editValue ? 2 * GlobalScale : GlobalScale);

    // vista previa: original arriba | color elegido abajo, con BORDE
    {
        w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
        rect->SetSize((GLshort)(prevX - GlobalScale), (GLshort)(circY - GlobalScale),
                      (GLshort)(prevW + 2 * GlobalScale), (GLshort)(circSide + 2 * GlobalScale));
        rect->RenderObject(false);
        w3dEngine::Color4f(original[0], original[1], original[2], 1.0f);
        rect->SetSize((GLshort)prevX, (GLshort)circY,
                      (GLshort)prevW, (GLshort)(circSide / 2));
        rect->RenderObject(false);
        w3dEngine::Color4f(target[0], target[1], target[2], 1.0f);
        rect->SetSize((GLshort)prevX, (GLshort)(circY + circSide / 2),
                      (GLshort)prevW, (GLshort)(circSide / 2));
        rect->RenderObject(false);
    }

    w3dEngine::Enable(w3dEngine::Texture2D);

    // pestanias RGB / HSV / Hex (+ Pal) + el toggle de unidad (0-255 / 0-100%)
    {
        int tx = borderGS + marginGS;
        for (int i = 0; i < nTabs; i++) {
            tabs[i]->active = (tab == i);
            tabs[i]->focus = (row == -1 && tab == i); // enfocada con flechas (verde)
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)tx, (GLfloat)tabsY, 0);
            tabs[i]->Render();
            w3dEngine::PopMatrix();
            tabs[i]->sx = x + tx;
            tabs[i]->sy = y + tabsY;
            tx += tabs[i]->width + gapGS;
        }
        unitX = tx; // (el switch de unidad ahora es un boton abajo)
    }

    // ---- pestania PALETA: colores de la paleta EFECTIVA del objeto (click =
    // el campo referencia ese INDICE; "Propio" = color propio) y las PALETAS
    // del proyecto (click = la paleta del objeto; "Igual que el padre" =
    // heredar). La fila actual se marca con un punto accent.
    if (tab == 3) {
        int left = borderGS + marginGS;
        int boxW = popUpWindow->width - left * 2;
        std::vector<Palette>& ps = W3dPalettes();
        std::vector<PaletteColor>* cs = palObj ? W3dEffectiveColors(palObj) : NULL;
        Palette* effective = palObj ? W3dEffectivePalette(palObj) : NULL;
        int nC = cs ? (int)cs->size() : 0;
        int nP = (int)ps.size();
        int lines = nC + nP + 4;   // 2 cabeceras + (1+nC) + (1+nP)
        int yRow = rowsY + gapGS;
        int cb = RowHeightGS;   // lado del swatch
        for (int d = palScroll; d < lines && d - palScroll < palVis; d++, yRow += palRowH) {
            // que es la linea d (ver PalRowLine)
            bool header = (d == 0 || d == 2 + nC);
            if (header) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(left + gapGS), (GLfloat)(yRow + GlobalScale), 0);
                w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
                RenderBitmapText(T(d == 0 ? "Colors" : "Palettes"), textAlign::left, boxW);
                w3dEngine::PopMatrix();
                continue;
            }
            int k = (d <= 1 + nC) ? d - 1 : d - 2;   // fila seleccionable
            // la tarjeta de la fila (borde blanco = foco de teclado)
            rowCard->Resize(boxW, palRowH - GlobalScale);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)left, (GLfloat)yRow, 0);
            w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
            rowCard->Render(false);
            if (row == k) {
                w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
                rowCard->RenderBorder(false);
            }
            // marcador de SELECCION actual (punto accent a la izquierda)
            bool marked = false;
            std::string label;
            const float* sw = NULL;   // swatch (solo filas de color)
            if (k == 0) { label = T("Own"); marked = palRef && *palRef < 0; }
            else if (k <= nC) {
                label = (*cs)[k - 1].name;
                sw = (*cs)[k - 1].rgba;
                marked = palRef && *palRef == k - 1;
            } else if (k == nC + 1) {
                label = T("Same as parent");
                marked = palObj && palObj->palette.empty();
            } else {
                int j = k - nC - 2;
                label = ps[j].name;
                marked = palObj && palObj->palette == ps[j].name;
                // sin seleccion propia: senalar la HEREDADA (la efectiva)
                if (!marked && palObj && palObj->palette.empty() && effective == &ps[j])
                    label += std::string(" ") + T("(inherited)");
            }
            int tx = gapGS * 2;
            if (marked) {
                w3dEngine::Disable(w3dEngine::Texture2D);
                w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
                rect->SetSize((GLshort)tx, (GLshort)((palRowH - GlobalScale * 3) / 2),
                              (GLshort)(3 * GlobalScale), (GLshort)(3 * GlobalScale));
                rect->RenderObject(false);
                w3dEngine::Enable(w3dEngine::Texture2D);
            }
            tx += 5 * GlobalScale;
            if (sw) {
                // el SWATCH del color (sobre un fondo negro por el alpha)
                w3dEngine::Disable(w3dEngine::Texture2D);
                w3dEngine::Color4f(0, 0, 0, 1.0f);
                rect->SetSize((GLshort)tx, (GLshort)GlobalScale, (GLshort)cb,
                              (GLshort)(palRowH - GlobalScale * 3));
                rect->RenderObject(false);
                w3dEngine::Color4f(sw[0], sw[1], sw[2], sw[3]);
                rect->SetSize((GLshort)(tx + GlobalScale), (GLshort)(GlobalScale * 2),
                              (GLshort)(cb - GlobalScale * 2),
                              (GLshort)(palRowH - GlobalScale * 5));
                rect->RenderObject(false);
                w3dEngine::Enable(w3dEngine::Texture2D);
                tx += cb + gapGS * 2;
            }
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)tx, (GLfloat)GlobalScale, 0);
            if (row == k) w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            else w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
            RenderBitmapText(label, textAlign::left, boxW - tx - gapGS);
            w3dEngine::PopMatrix();
            w3dEngine::PopMatrix();
        }
        // hay mas filas arriba/abajo: indicadores chicos al borde derecho
        if (palScroll > 0 || palScroll + palVis < lines) {
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(left + boxW - gapGS * 3), (GLfloat)(rowsY + gapGS), 0);
            w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
            if (palScroll > 0) RenderBitmapText("^", textAlign::left, gapGS * 4);
            w3dEngine::PopMatrix();
            if (palScroll + palVis < lines) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(left + boxW - gapGS * 3),
                                      (GLfloat)(rowsY + gapGS + (palVis - 1) * palRowH), 0);
                w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
                RenderBitmapText("v", textAlign::left, gapGS * 4);
                w3dEngine::PopMatrix();
            }
        }
    }
    // valores de la pestania activa. RGB/HSV: los 3 juntos en UNA
    // tarjeta con borde grande; Alpha separado. Etiqueta a la izquierda
    // y el valor alineado al borde DERECHO.
    if (tab != 3) {
        int left = borderGS + marginGS;
        int boxW = popUpWindow->width - left * 2;
        char buf[24];

        float vals[4];
        if (tab == 1) {
            vals[0] = h; vals[1] = s; vals[2] = v; vals[3] = target[3];
        } else {
            vals[0] = target[0]; vals[1] = target[1];
            vals[2] = target[2]; vals[3] = target[3];
        }
        static const char* LabelsRGB[4] = { "Red", "Green", "Blue", "Alpha" };
        static const char* LabelsHSV[4] = { "Hue", "Saturation", "Value", "Alpha" };

        if (tab == 2) {
            // Hex: una sola fila con su tarjeta
            rowCard->Resize(boxW, RowHeightGS + GlobalScale * 2);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)left, (GLfloat)(rowsY + gapGS), 0);
            w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
            rowCard->Render(false);
            w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            if (row == 0) rowCard->RenderBorder(false);
            sprintf(buf, "%02X%02X%02X%02X",
                    (int)(target[0] * 255.0f + 0.5f),
                    (int)(target[1] * 255.0f + 0.5f),
                    (int)(target[2] * 255.0f + 0.5f),
                    (int)(target[3] * 255.0f + 0.5f));
            // EDITANDO con el teclado: lo escrito (con el cursor); en vivo, si es un hex valido ya es el color
            bool hexBad = false;
            std::string hexTxt = buf;
            if (TextFieldLive(&hexField)) {
                hexBad = !HexApply(hexField.text);
                hexTxt = hexField.text;
            }
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)GlobalScale, 0);
            w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
            RenderBitmapText("Hex", textAlign::left, boxW / 2);
            w3dEngine::PopMatrix();
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(boxW - gapGS * 2), (GLfloat)GlobalScale, 0);
            if (hexBad) w3dEngine::Color4f(1.0f, 0.35f, 0.35f, 1.0f);   // (todavia no es un hex: rojo)
            else w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            if (g_activeTextField == &hexField) RenderTextField(hexField, true, textAlign::right, boxW - gapGS * 4);
            else RenderBitmapText(hexTxt.c_str(), textAlign::right, boxW - gapGS * 4);
            w3dEngine::PopMatrix();
            w3dEngine::PopMatrix();
        } else {
            // tarjeta-GRUPO con los 3 valores adentro
            rowCard->Resize(boxW, groupH);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)left, (GLfloat)(rowsY + gapGS), 0);
            w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
            rowCard->Render(false);
            // el borde grande: accent si la fila activa esta adentro
            if (row < 3) {
                if (drag == 3) w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
                else w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
                rowCard->RenderBorder(false);
            }
            for (int i = 0; i < 3; i++) {
                int slotY = GlobalScale + i * (RowHeightGS + GlobalScale);
                // relleno (barra de progreso) con margen de 1 pixel
                float frac = vals[i];
                int fillW = (int)((boxW - GlobalScale * 2) * frac);
                if (fillW > 0) {
                    w3dEngine::Disable(w3dEngine::Texture2D);
                    w3dEngine::Color4f(accent[0], accent[1], accent[2], 0.55f);
                    rect->SetSize((GLshort)GlobalScale, (GLshort)slotY,
                                  (GLshort)fillW, (GLshort)RowHeightGS);
                    rect->RenderObject(false);
                    w3dEngine::Enable(w3dEngine::Texture2D);
                }
                // etiqueta izquierda + valor a la DERECHA
                const char* label = (tab == 0) ? LabelsRGB[i]
                                                       : LabelsHSV[i];
                if (tab == 1) FormatFloat3(buf, vals[i]);
                else if (ColorPickerUnit) sprintf(buf, "%d%%", (int)(vals[i] * 100.0f + 0.5f));
                else sprintf(buf, "%d", (int)(vals[i] * 255.0f + 0.5f));
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)slotY, 0);
                if (i == row) w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
                else w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
                RenderBitmapText(T(label), textAlign::left, boxW / 2);
                w3dEngine::PopMatrix();
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(boxW - gapGS * 2), (GLfloat)slotY, 0);
                if (i == row) w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
                else w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
                RenderBitmapText(buf, textAlign::right, boxW - gapGS * 4);
                w3dEngine::PopMatrix();
            }
            w3dEngine::PopMatrix();

            // ALPHA separado, con su propia tarjeta (como estaba)
            rowCard->Resize(boxW, RowHeightGS + GlobalScale * 2);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)left, (GLfloat)alphaY, 0);
            w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
            rowCard->Render(false);
            {
                int fillW = (int)((boxW - GlobalScale * 2) * vals[3]);
                if (fillW > 0) {
                    w3dEngine::Disable(w3dEngine::Texture2D);
                    w3dEngine::Color4f(accent[0], accent[1], accent[2], 0.55f);
                    rect->SetSize((GLshort)GlobalScale, (GLshort)GlobalScale,
                                  (GLshort)fillW, (GLshort)RowHeightGS);
                    rect->RenderObject(false);
                    w3dEngine::Enable(w3dEngine::Texture2D);
                }
            }
            if (row == 3) {
                if (drag == 3) w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
                else w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
                rowCard->RenderBorder(false);
            }
            // el alpha SIEMPRE es flotante 0..1, en RGB y en HSV por
            // igual (es el mismo valor: 0.540 aca y alla)
            FormatFloat3(buf, vals[3]);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)GlobalScale, 0);
            if (row == 3) w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            else w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
            RenderBitmapText(T("Alpha"), textAlign::left, boxW / 2);
            w3dEngine::PopMatrix();
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(boxW - gapGS * 2), (GLfloat)GlobalScale, 0);
            if (row == 3) w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            else w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
            RenderBitmapText(buf, textAlign::right, boxW - gapGS * 4);
            w3dEngine::PopMatrix();
            w3dEngine::PopMatrix();
        }
    }

    // boton de UNIDAD (ancho completo; el label dice a que cambia) y
    // abajo OK / Cancel repartidos 50% y 50%
    {
        int left = borderGS + marginGS;
        int contW = popUpWindow->width - left - marginGS - borderGS;
        if (tab == 0) {
            // solo en RGB (HSV es flotante 0..1 y Hex no usa unidad)
            btnUnit->text = T(ColorPickerUnit ? "Switch to 0-255"
                                                  : "Switch to 0-100%");
            btnUnit->focusMenu = (row == Rows()); // enfocado con flechas (la fila del switch en RGB)
            btnUnit->Resize(contW);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)left, (GLfloat)btnY, 0);
            btnUnit->Render();
            w3dEngine::PopMatrix();
            btnUnit->sx = x + left;
            btnUnit->sy = y + btnY;
        } else {
            btnUnit->sx = btnUnit->sy = -10000; // sin click en HSV
        }

        // OK a la DERECHA, Cancel a la IZQUIERDA (logica de popup S60, igual que el popup de borrar)
        int half = (contW - gapGS) / 2;
        // Hex: Copiar / Pegar en el lugar libre de abajo del valor (50% y 50%, como OK / Cancel)
        if (tab == 2) {
            btnCopy->Resize(half); btnPaste->Resize(half);
            w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)left, (GLfloat)hexBtnY, 0); btnCopy->Render(); w3dEngine::PopMatrix();
            w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(left + half + gapGS), (GLfloat)hexBtnY, 0); btnPaste->Render(); w3dEngine::PopMatrix();
            btnCopy->sx  = x + left;                 btnCopy->sy = y + hexBtnY;
            btnPaste->sx = x + left + half + gapGS; btnPaste->sy  = y + hexBtnY;
        } else {
            btnCopy->sx = btnCopy->sy = btnPaste->sx = btnPaste->sy = -10000;
        }
        int rowOk = Rows() + (tab == 0 ? 1 : 0); // la fila virtual de OK/Cancel (despues del switch en RGB)
        btnOk->focusMenu     = (row == rowOk && okFocus == 0); // enfocado con flechas (verde)
        btnCancel->focusMenu = (row == rowOk && okFocus == 1);
        btnOk->Resize(half);
        btnCancel->Resize(half);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)left, (GLfloat)btnOkY, 0); // Cancel IZQUIERDA
        btnCancel->Render();
        w3dEngine::PopMatrix();
        btnCancel->sx = x + left;
        btnCancel->sy = y + btnOkY;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(left + half + gapGS), (GLfloat)btnOkY, 0); // OK DERECHA
        btnOk->Render();
        w3dEngine::PopMatrix();
        btnOk->sx = x + left + half + gapGS;
        btnOk->sy = y + btnOkY;
    }

    endView();
}

// "#RRGGBB", "RRGGBB", "RRGGBBAA" (con o sin #, espacios afuera, mayusculas o minusculas)
bool ColorPicker::HexApply(const std::string& t) {
    if (!target) return false;
    std::string d;
    for (size_t i = 0; i < t.size(); i++) {
        const char c = t[i];
        if (c == '#' || c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
        d += c;
    }
    if (d.size() != 6 && d.size() != 8) return false;
    for (size_t k = 0; k < d.size() / 2; k++) {
        unsigned v = 0;
        for (int q = 0; q < 2; q++) { const char c = d[k*2+q];
            v = v * 16 + (unsigned)((c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - 'A' + 10); }
        target[k] = (float)v / 255.0f;
    }
    FromRGB();
    return true;
}
void ColorPicker::HexEdit() {
    if (!target) return;
    char buf[16];
    sprintf(buf, "%02X%02X%02X%02X", (int)(target[0] * 255.0f + 0.5f), (int)(target[1] * 255.0f + 0.5f),
            (int)(target[2] * 255.0f + 0.5f), (int)(target[3] * 255.0f + 0.5f));
    hexField.SetText(buf);
    hexField.SelectAll();          // tipear reemplaza todo (como renombrar)
    TextFieldFocus(&hexField);
}

void ColorPicker::FitRow(int delta) {
    if (!target || tab >= 2) return; // hex: solo lectura; paleta: filas de accion
    float d = delta / 255.0f;
    if (tab == 0) {
        target[row] += d;
        if (target[row] < 0.0f) target[row] = 0.0f;
        if (target[row] > 1.0f) target[row] = 1.0f;
        if (row < 3) FromRGB(); // el alpha no toca el HSV
    } else {
        if (row == 3) {
            target[3] += d;
            if (target[3] < 0.0f) target[3] = 0.0f;
            if (target[3] > 1.0f) target[3] = 1.0f;
            return;
        }
        float* p = (row == 0) ? &h : (row == 1) ? &s : &v;
        *p += d;
        if (row == 0) {
            // el matiz da la vuelta
            if (*p < 0.0f) *p += 1.0f;
            if (*p > 1.0f) *p -= 1.0f;
        } else {
            if (*p < 0.0f) *p = 0.0f;
            if (*p > 1.0f) *p = 1.0f;
        }
        ToRGB();
    }
}

bool ColorPicker::Click(int mx, int my) {
    if (drag != 0) {
        // otro click CONFIRMA el arrastre (Symbian: click para empezar,
        // click para terminar; el cursor vuelve de violeta a normal)
        drag = 0;
        return true;
    }
    if (!Contains(mx, my)) return false; // afuera: el caller cierra
    int lx = mx - x;
    int ly = my - y;

    // pestanias (y el toggle de unidad a su derecha)
    if (ly >= tabsY && ly < tabsY + tabHeight) {
        int tx = borderGS + marginGS;
        for (int i = 0; i < nTabs; i++) {
            if (lx >= tx && lx < tx + tabs[i]->width) {
                tab = i;
                row = 0;
                palScroll = 0;
                Reflow();   // el alto del popup depende de la pestania
                return true;
            }
            tx += tabs[i]->width + gapGS;
        }
        return true;
    }
    // circulo cromatico: setea H/S
    if (lx >= circX && lx < circX + circSide &&
        ly >= circY && ly < circY + circSide) {
        drag = 1;
        moved = false;
        Motion(mx, my);
        return true;
    }
    // barra de valor
    if (lx >= barX - gapGS && lx < barX + barW + gapGS &&
        ly >= circY && ly < circY + circSide) {
        drag = 2;
        moved = false;
        Motion(mx, my);
        return true;
    }
    // boton de unidad: 0-255 <-> 0-100% (queda guardado en memoria)
    if (tab == 0 && btnUnit->Contains(mx, my)) {
        ColorPickerUnit = ColorPickerUnit ? 0 : 1;
        return true;
    }

    // Hex: Copiar (#RRGGBB, +AA si no es opaco) / Pegar (cualquier hex del portapapeles)
    if (tab == 2 && btnCopy->Contains(mx, my)) {
        if (target) {
            char b[16]; const int a = (int)(target[3] * 255.0f + 0.5f);
            if (a >= 255) sprintf(b, "#%02X%02X%02X", (int)(target[0]*255.0f+0.5f), (int)(target[1]*255.0f+0.5f), (int)(target[2]*255.0f+0.5f));
            else sprintf(b, "#%02X%02X%02X%02X", (int)(target[0]*255.0f+0.5f), (int)(target[1]*255.0f+0.5f), (int)(target[2]*255.0f+0.5f), a);
            w3dEngine::W3dClipboardSet(b);
            Notify(std::string(T("Copied")) + " " + b, false);
        }
        return true;
    }
    if (tab == 2 && btnPaste->Contains(mx, my)) {
        const std::string t = w3dEngine::W3dClipboardGet();
        if (!HexApply(t)) Notify(T("The clipboard has no hex color"), true);
        else if (g_activeTextField == &hexField) HexEdit();   // (la caja muestra lo pegado)
        return true;
    }
    // botones OK / Cancel
    if (btnOk->Contains(mx, my)) {
        Close(); // aceptar: el color ya esta aplicado en vivo
        return true;
    }
    if (btnCancel->Contains(mx, my)) {
        if (target) {
            target[0] = original[0];
            target[1] = original[1];
            target[2] = original[2];
            target[3] = original[3];
        }
        Close();
        return true;
    }

    // pestania PALETA: click en una fila de la lista = ejecutarla (referenciar
    // el indice / volver a propio / elegir la paleta del objeto / heredar)
    if (tab == 3) {
        int listY = rowsY + gapGS;
        if (ly >= listY && ly < listY + palVis * palRowH) {
            int d = palScroll + (ly - listY) / palRowH;
            int nC = PalColorCount();
            int k = -1;   // las cabeceras (0 y 2+nC) no son seleccionables
            if (d >= 1 && d <= 1 + nC) k = d - 1;
            else if (d >= 3 + nC) k = d - 2;
            if (k >= 0 && k < Rows()) {
                row = k;
                PalApplyRow(k);
            }
        }
        return true;
    }

    // valores: el grupo de 3 y la fila de Alpha (arrastran en X)
    if (ly >= rowsY && ly < btnY - gapGS) {
        if (tab == 2) {
            row = 0;
            if (ly < hexBtnY) HexEdit();   // click en el valor: escribirlo con el teclado
            return true;
        }
        int groupY = rowsY + gapGS;
        if (ly >= groupY && ly < groupY + groupH) {
            int f = (ly - groupY - GlobalScale) / (RowHeightGS + GlobalScale);
            if (f < 0) f = 0;
            if (f > 2) f = 2;
            row = f;
            drag = 3;
            moved = false;
            dragX = mx;
        } else if (ly >= alphaY && ly < alphaY + RowHeightGS + GlobalScale * 2) {
            row = 3;
            drag = 3;
            moved = false;
            dragX = mx;
        }
        return true;
    }
    return true;
}

bool ColorPicker::Motion(int mx, int my) {
    if (drag != 0) {
        moved = true; // el up de PC ya puede soltar
        int margin = borderGS;
        if (drag == 3) {
            // arrastrando un VALOR: el mouse da la vuelta (wrap)
            int nx = mx;
            int ny = my;
            if (mx <= x + margin) nx = x + popUpWindow->width - margin - 1;
            else if (mx >= x + popUpWindow->width - margin) nx = x + margin + 1;
            if (my <= y + margin) ny = y + popUpWindow->height - margin - 1;
            else if (my >= y + popUpWindow->height - margin) ny = y + margin + 1;
            if (nx != mx || ny != my) {
                if (LayoutWarpMouse) LayoutWarpMouse(nx, ny);
                dragX = nx; // que el salto no cuente como delta
                return true;
            }
        } else if (drag == 1) {
            // el cursor NO se sale del circulo cromatico: se frena en su
            // borde (radio), nada de aparecer en otro lado del cuadrado
            float cx = (float)(x + circX) + circSide * 0.5f;
            float cy = (float)(y + circY) + circSide * 0.5f;
            float dxp = (float)mx - cx;
            float dyp = (float)my - cy;
            float dist = sqrtf(dxp * dxp + dyp * dyp);
            float radius = circSide * 0.5f - 1.0f;
            if (dist > radius && dist > 0.0f) {
                int nx = (int)(cx + dxp / dist * radius);
                int ny = (int)(cy + dyp / dist * radius);
                if (LayoutWarpMouse) LayoutWarpMouse(nx, ny);
                mx = nx;
                my = ny;
            }
        } else {
            // barra V: el cursor queda sobre la barra
            int nx = mx;
            int ny = my;
            if (nx < x + barX) nx = x + barX;
            if (nx > x + barX + barW) nx = x + barX + barW;
            if (ny < y + circY) ny = y + circY;
            if (ny > y + circY + circSide) ny = y + circY + circSide;
            if (nx != mx || ny != my) {
                if (LayoutWarpMouse) LayoutWarpMouse(nx, ny);
                mx = nx;
                my = ny;
            }
        }
    } else {
        // sin arrastre: si el mouse se aleja mucho se cierra solo
        // (ACEPTA, igual que los menus desplegables)
        int margin = 60 * GlobalScale;
        if (mx < x - margin || mx > x + popUpWindow->width + margin ||
            my < y - margin || my > y + popUpWindow->height + margin) {
            Close();
            return true;
        }
    }
    btnOk->hover = btnOk->Contains(mx, my);
    btnCancel->hover = btnCancel->Contains(mx, my);
    btnUnit->hover   = (tab == 0) && btnUnit->Contains(mx, my);
    if (drag == 0) {
        return true; // sin arrastre: solo hover
    }
    int lx = mx - x;
    int ly = my - y;
    if (drag == 1) {
        // H por angulo (rojo ABAJO, horario), S por distancia al centro
        float cx = circX + circSide * 0.5f;
        float cy = circY + circSide * 0.5f;
        float dx = (lx - cx) / (circSide * 0.5f);
        float dy = (cy - ly) / (circSide * 0.5f);
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 1.0f) dist = 1.0f;
        float ang = atan2f(dy, dx);
        h = (-1.5707963f - ang) / 6.2831853f;
        while (h < 0.0f) h += 1.0f;
        while (h > 1.0f) h -= 1.0f;
        s = dist;
        ToRGB();
    } else if (drag == 2) {
        float t = 1.0f - (float)(ly - circY) / (float)circSide;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        v = t;
        ToRGB();
    } else if (drag == 3) {
        int delta = mx - dragX;
        if (delta != 0) {
            dragX = mx;
            FitRow(delta); // 1 pixel = 1/255
        }
    }
    return true;
}

void ColorPicker::Release() {
    // PC: al soltar el boton, si hubo movimiento, termina el arrastre
    // (el tap de Symbian no mueve: queda en modo toggle hasta otro click)
    if (drag != 0 && moved) {
        drag = 0;
    }
}

bool ColorPicker::Dragging() {
    return drag != 0;
}

// aceleracion del hold (MISMA en todas las barras de color): arranca LENTO (1 por frame ~0.3s -> permite
// soltar en el valor exacto) y acelera DE A POCO (tope 8). Antes saltaba a 30 -> era demasiado rapido.
int ColorPicker::StepHold(int dir) {
    if (dir == holdDir) holdCount++; else { holdDir = dir; holdCount = 0; }
    int c = holdCount;
    return (c < 8) ? 1 : (c < 18) ? 2 : (c < 32) ? 4 : 8;
}

bool ColorPicker::Key(int key) {
    int rows = Rows();
    bool hasSwitch = (tab == 0);                     // el boton "Switch 0-255/0-100%" SOLO existe en RGB
    int rowSwitch  = hasSwitch ? rows : -100;        // (inalcanzable si no hay switch)
    int rowOk = rows + (hasSwitch ? 1 : 0);          // ultima fila virtual: OK/Cancel
    // FOCO (fila): -3 CIRCULO, -2 barra VALUE, -1 PESTANIAS, 0..filas-1 FILAS, rowSwitch SWITCH, rowOk OK/Cancel

    // ---- MODO EDITAR CIRCULO: las flechas mueven Hue (izq/der) y Saturacion (arr/aba); OK/Enter/Cancel salen ----
    if (editCircle) {
        float sat = 4.0f / 255.0f;              // SATURACION: paso fijo (la velocidad asi esta bien)
        switch (key) {
            // HUE: acelera como las barras pero topeado en 4/255 (el hue es mas sensible -> el circulo entero;
            // arranca lento para landing preciso y sube hasta la velocidad "perfecta" de la saturacion).
            case LayoutKey::Left:  { int p = StepHold(-1); if (p > 4) p = 4; h -= (float)p / 255.0f; if (h < 0.0f) h += 1.0f; ToRGB(); return true; }
            case LayoutKey::Right: { int p = StepHold(1);  if (p > 4) p = 4; h += (float)p / 255.0f; if (h > 1.0f) h -= 1.0f; ToRGB(); return true; }
            case LayoutKey::Up:    holdDir = holdCount = 0; s += sat; if (s > 1.0f) s = 1.0f;  ToRGB(); return true;
            case LayoutKey::Down:  holdDir = holdCount = 0; s -= sat; if (s < 0.0f) s = 0.0f;  ToRGB(); return true;
            case LayoutKey::Accept: case LayoutKey::Enter: case LayoutKey::Cancel:
                editCircle = false; return true; // sale del modo edicion (el color queda aplicado)
        }
        return true;
    }

    // ---- MODO EDITAR BARRA VALUE (vertical): arriba/derecha suben v, abajo/izquierda bajan; OK/Cancel salen ----
    if (editValue) {
        int dir = 0;
        if (key == LayoutKey::Up || key == LayoutKey::Right) dir = 1;
        else if (key == LayoutKey::Down || key == LayoutKey::Left) dir = -1;
        if (dir != 0) {                         // ajuste con aceleracion (sirve para el hold del N95)
            int step = StepHold(dir);
            v += dir * step / 255.0f; if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f; ToRGB();
            return true;
        }
        editValue = false; holdDir = holdCount = 0; // Accept/Enter/Cancel: salir del modo edicion
        return true;
    }

    switch (key) {
        case LayoutKey::Up:
            holdDir = holdCount = 0;
            row--; if (row < -3) row = rowOk;   // wrap: arriba del circulo -> OK/Cancel
            PalEnsureVisible();                 // pestania Paleta: la lista sigue al foco
            return true;
        case LayoutKey::Down:
            holdDir = holdCount = 0;
            row++; if (row > rowOk) row = -3;   // wrap: abajo de OK/Cancel -> circulo
            PalEnsureVisible();
            return true;
        case LayoutKey::Left:
        case LayoutKey::Right: {
            int dir = (key == LayoutKey::Right) ? 1 : -1;
            if (row == -3) {                       // CIRCULO: der -> barra VALUE (esta a su derecha); el circulo se edita con OK
                holdDir = holdCount = 0;
                if (dir > 0) row = -2;
            } else if (row == -2) {                // barra VALUE: izq -> CIRCULO (a su izquierda); el value se edita con OK (arr/aba)
                holdDir = holdCount = 0;
                if (dir < 0) row = -3;
            } else if (row == -1) {                // PESTANIAS: cambia RGB / HSV / Hex (/ Pal)
                holdDir = holdCount = 0;
                tab += dir; if (tab < 0) tab = nTabs - 1; if (tab >= nTabs) tab = 0;
                palScroll = 0;
                Reflow();                         // el alto del popup depende de la pestania
            } else if (row == rowSwitch) {        // SWITCH 0-255 <-> 0-100%
                holdDir = holdCount = 0;
                ColorPickerUnit = ColorPickerUnit ? 0 : 1;
            } else if (row == rowOk) {            // OK (derecha) / Cancel (izquierda)
                holdDir = holdCount = 0;
                okFocus = (dir > 0) ? 0 : 1;         // der = OK (0), izq = Cancel (1)
            } else {                                 // FILA de valor R/G/B/A: ACELERACION (tap preciso, hold rampa)
                FitRow(dir * StepHold(dir));
            }
            return true;
        }
        case LayoutKey::Accept:
        case LayoutKey::Enter:
            if (row == -3) {                       // CIRCULO: OK = entrar a editar (flechas mueven H/S)
                editCircle = true;
            } else if (row == -2) {                // BARRA VALUE: OK = entrar a editar (arr/aba ajustan v)
                editValue = true;
            } else if (row == rowSwitch) {        // SWITCH: OK lo togglea
                ColorPickerUnit = ColorPickerUnit ? 0 : 1;
            } else if (row == rowOk && okFocus == 1) { // Cancel enfocado: restaurar
                if (target) { target[0]=original[0]; target[1]=original[1]; target[2]=original[2]; target[3]=original[3]; }
                Close();
            } else if (tab == 2 && row == 0) {
                HexEdit();                        // Hex: Enter = escribir el valor (Enter de nuevo lo deja)
            } else if (tab == 3 && row >= 0 && row < rows) {
                PalApplyRow(row);               // pestania Paleta: ejecutar la fila (no cierra)
            } else {                            // OK enfocado / cualquier otra fila: aceptar (ya esta vivo)
                Close();
            }
            return true;
        case LayoutKey::Cancel:                     // esc / C del telefono / X: restaurar el original
            if (target) { target[0]=original[0]; target[1]=original[1]; target[2]=original[2]; target[3]=original[3]; }
            Close();
            return true;
    }
    return true; // modal: no le roban teclas
}

// flecha MANTENIDA (frame-based, keypad N95 que no auto-repite rapido): SOLO ajusta valores, NUNCA
// navega -> mantener una flecha para subir/bajar R/G/B/A o editar circulo/value es FLUIDO (con la misma
// aceleracion del hold). La navegacion entre elementos sigue 1-por-tap (via Tecla en el key-down).
bool ColorPicker::KeyRepeat(int key) {
    if (editCircle || editValue) return Key(key);   // en edicion las flechas AJUSTAN -> repetir todas
    if (row >= 0 && row < Rows() &&                    // fila de valor R/G/B/A: izq/der ajusta -> repetir
        (key == LayoutKey::Left || key == LayoutKey::Right))
        return Key(key);
    return true; // navegacion (circulo/value/pestanias/switch/OK-Cancel): el repeat NO hace nada
}
