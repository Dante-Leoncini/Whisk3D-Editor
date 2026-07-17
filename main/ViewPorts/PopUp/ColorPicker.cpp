#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "ColorPicker.h"
#include "ViewPorts/LayoutInput.h" // LayoutKey
#include "WhiskUI/UI.h"
#include "WhiskUI/bitmapText.h"
#include "objects/Textures.h"
#include "Undo.h" // Ctrl+Z: captura el cambio de color al cerrar
#include <cstdio>
#include <cmath>

ColorPicker* colorPicker = NULL;

// 0 = 0..255, 1 = 0..100% (persistente entre aperturas del picker)
int ColorPickerUnidad = 0;

// "0.000".."1.000" sin %f (la estlib de Symbian no lo tiene)
static void FormatoFloat3(char* buf, float v) {
    int mil = (int)(v * 1000.0f + 0.5f);
    if (mil >= 1000) {
        buf[0] = '1'; buf[1] = '.'; buf[2] = buf[3] = buf[4] = '0'; buf[5] = 0;
    } else {
        sprintf(buf, "0.%03d", mil);
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

ColorPicker::ColorPicker() : PopUpBase("Color") {
    // (eran inicializadores de clase: C++03)
    target = NULL;
    original[0] = original[1] = original[2] = original[3] = 1.0f;
    h = 0.0f;
    s = 0.0f;
    v = 1.0f;
    pestania = 0;
    tabs[0] = new Tab("RGB");
    tabs[1] = new Tab("HSV");
    tabs[2] = new Tab("Hex");
    fila = 0;
    okFoco = 0; holdDir = 0; holdCount = 0; editCirculo = false; editValue = false;
    arrastre = 0;
    movio = false;
    arrastreX = 0;
    rect = new Rec2D();
    filaCard = new Card(NULL, 10, 10);
    btnOk = new Button("OK");
    btnCancel = new Button(T("Cancel"));
    btnUnidad = new Button(T("Switch to 0-100%"));
    btnOk->adaptar = false;
    btnCancel->adaptar = false;
    btnUnidad->adaptar = false;
    btnOk->centrado = true;
    btnCancel->centrado = true;
    btnUnidad->centrado = true;
    btnY = 0;
    btnOkY = 0;
    unidadX = 0;
    circX = circY = 0;
    circLado = 10;
    barraX = 0;
    barraW = 10;
    prevX = 0;
    prevW = 10;
    tabsY = 0;
    tabAlto = 10;
    filasY = 0;
}

ColorPicker::~ColorPicker() {
    delete rect;
    delete filaCard;
    delete btnOk;
    delete btnCancel;
    delete btnUnidad;
    delete tabs[0];
    delete tabs[1];
    delete tabs[2];
}

int ColorPicker::Filas() const {
    return (pestania == 2) ? 1 : 4; // Hex es una sola fila
}

void ColorPicker::DeRGB() {
    if (!target) return;
    RGBaHSV(target[0], target[1], target[2], &h, &s, &v);
}

void ColorPicker::AlRGB() {
    if (!target) return;
    HSVaRGB(h, s, v, &target[0], &target[1], &target[2]);
}

void ColorPicker::Abrir(GLfloat* Target, int px, int py) {
    target = Target;
    if (target) {
        original[0] = target[0];
        original[1] = target[1];
        original[2] = target[2];
        original[3] = target[3];
    }
    DeRGB();
    fila = 0;              // foco inicial: primera fila de valor
    okFoco = 0;            // OK por defecto
    editCirculo = false; editValue = false;
    holdDir = holdCount = 0;
    arrastre = 0;

    // layout (pixeles del popup, escalados)
    int pad = borderGS + GlobalScale * 3;       // padding exterior CHICO (~3px de aire; antes marginGS = demasiado)
    circLado = 48 * GlobalScale;
    circX = pad;
    circY = borderGS + RenglonHeightGS + gapGS; // bajo el titulo
    barraX = circX + circLado + gapGS * 2;
    barraW = 10 * GlobalScale;
    prevX = barraX + barraW + gapGS * 2;
    prevW = 22 * GlobalScale;

    // pestanias RGB / HSV / Hex (se funden con las filas de abajo)
    tabs[0]->Resize(100000);
    tabs[1]->Resize(100000);
    tabs[2]->Resize(100000);
    tabAlto = tabs[0]->height;
    tabsY = circY + circLado + gapGS * 2;
    filasY = tabsY + tabAlto; // sin gap: la pestania se funde

    int w = prevX + prevW + pad;
    int wTabs = pad * 2 +
                tabs[0]->width + tabs[1]->width + tabs[2]->width + gapGS * 2;
    if (w < wTabs) w = wTabs;
    // los 3 valores juntos en una tarjeta (margen de 1px entre rellenos)
    grupoH = 3 * RenglonHeightGS + 4 * GlobalScale;
    alphaY = filasY + gapGS + grupoH + gapGS;
    // boton de unidad (ancho completo) y abajo OK / Cancel al 50% cada uno
    btnY = alphaY + RenglonHeightGS + GlobalScale * 2 + gapGS;
    btnOkY = btnY + (RenglonHeightGS + bordersGS) + gapGS;
    int hTotal = btnOkY + (RenglonHeightGS + bordersGS) + pad;
    popUpWindow->Resize(w, hTotal);

    // posicion clampeada a la pantalla
    x = px;
    y = py;
    if (x + w > MenuPantallaW) x = MenuPantallaW - w;
    if (y + hTotal > MenuPantallaH) y = MenuPantallaH - hTotal;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    PopUpActive = this;
}

// Ctrl+Z: al cerrar (acepta O cancela), captura el cambio de color. Cancelar restaura 'original' ANTES de
// llamar Cerrar -> target == original -> UndoCapturarColor no pushea. Aceptar deja target cambiado -> pushea.
void ColorPicker::Cerrar() {
    if (target) UndoCapturarColor(target, original);
    editCirculo = false; editValue = false; // por si quedo editando
    PopUpBase::Cerrar();
}

// marco de foco (4 rect finos accent) para resaltar el circulo / barra de valor enfocados con el teclado.
// Texture2D debe estar apagado (se llama en la zona plana del render). grosor en GlobalScale.
static void MarcoFoco(Rec2D* r, int x, int y, int w, int h, int grosor) {
    const float* a = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(a[0], a[1], a[2], 1.0f);
    r->SetSize((GLshort)x, (GLshort)y, (GLshort)w, (GLshort)grosor); r->RenderObject(false);             // arriba
    r->SetSize((GLshort)x, (GLshort)(y + h - grosor), (GLshort)w, (GLshort)grosor); r->RenderObject(false); // abajo
    r->SetSize((GLshort)x, (GLshort)y, (GLshort)grosor, (GLshort)h); r->RenderObject(false);             // izquierda
    r->SetSize((GLshort)(x + w - grosor), (GLshort)y, (GLshort)grosor, (GLshort)h); r->RenderObject(false); // derecha
}

void ColorPicker::Render() {
    if (!target) return;
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);

    const float* gris = ListaColores[static_cast<int>(ColorID::gris)];
    const float* fondo = ListaColores[static_cast<int>(ColorID::background)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];

    // ventana con BORDE VERDE (el popup activo es el foco)
    w3dEngine::Color4f(gris[0], gris[1], gris[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // titulo
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)borderGS, 0);
    w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
    RenderBitmapText(name, textAlign::left, popUpWindow->width - bordersGS);
    w3dEngine::PopMatrix();

    // circulo cromatico: arte de 32x32 en (96,70) del atlas de 128x128
    {
        GLshort cv[8];
        GLfloat cuv[8];
        cv[0] = (GLshort)circX;              cv[1] = (GLshort)circY;
        cv[2] = (GLshort)(circX + circLado); cv[3] = (GLshort)circY;
        cv[4] = (GLshort)circX;              cv[5] = (GLshort)(circY + circLado);
        cv[6] = (GLshort)(circX + circLado); cv[7] = (GLshort)(circY + circLado);
        float u0 = 96.0f / 128.0f, v0 = 70.0f / 128.0f;
        float u1 = 1.0f, v1 = 102.0f / 128.0f;
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
        float rad = s * (circLado * 0.5f);
        int mxp = circX + circLado / 2 + (int)(cosf(ang) * rad);
        int myp = circY + circLado / 2 - (int)(sinf(ang) * rad);
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
    if (fila == -3) MarcoFoco(rect, circX, circY, circLado, circLado, editCirculo ? 2 * GlobalScale : GlobalScale);

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
        float x0 = (float)barraX;
        float x1 = (float)(barraX + barraW);
        float y0 = (float)circY;
        float y1 = (float)(circY + circLado);
        // dos triangulos: TL TR BL / TR BR BL
        bv[0] = x0; bv[1] = y0;   bv[2] = x1; bv[3] = y0;
        bv[4] = x0; bv[5] = y1;   bv[6] = x1; bv[7] = y0;
        bv[8] = x1; bv[9] = y1;   bv[10] = x0; bv[11] = y1;
        static const int arriba[6] = { 1, 1, 0, 1, 0, 0 };
        for (int vtx = 0; vtx < 6; vtx++) {
            bc[vtx * 4 + 0] = arriba[vtx] ? cr : 0;
            bc[vtx * 4 + 1] = arriba[vtx] ? cg : 0;
            bc[vtx * 4 + 2] = arriba[vtx] ? cb : 0;
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
        int myp = circY + (int)((1.0f - v) * (circLado - 2 * GlobalScale));
        w3dEngine::Color4f(0, 0, 0, 1.0f);
        rect->SetSize((GLshort)barraX, (GLshort)(myp - GlobalScale),
                      (GLshort)barraW, (GLshort)(4 * GlobalScale));
        rect->RenderObject(false);
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        rect->SetSize((GLshort)barraX, (GLshort)myp,
                      (GLshort)barraW, (GLshort)(2 * GlobalScale));
        rect->RenderObject(false);
    }

    // foco de teclado en la BARRA DE VALOR (verde)
    if (fila == -2) MarcoFoco(rect, barraX, circY, barraW, circLado, editValue ? 2 * GlobalScale : GlobalScale);

    // vista previa: original arriba | color elegido abajo, con BORDE
    {
        w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
        rect->SetSize((GLshort)(prevX - GlobalScale), (GLshort)(circY - GlobalScale),
                      (GLshort)(prevW + 2 * GlobalScale), (GLshort)(circLado + 2 * GlobalScale));
        rect->RenderObject(false);
        w3dEngine::Color4f(original[0], original[1], original[2], 1.0f);
        rect->SetSize((GLshort)prevX, (GLshort)circY,
                      (GLshort)prevW, (GLshort)(circLado / 2));
        rect->RenderObject(false);
        w3dEngine::Color4f(target[0], target[1], target[2], 1.0f);
        rect->SetSize((GLshort)prevX, (GLshort)(circY + circLado / 2),
                      (GLshort)prevW, (GLshort)(circLado / 2));
        rect->RenderObject(false);
    }

    w3dEngine::Enable(w3dEngine::Texture2D);

    // pestanias RGB / HSV / Hex + el toggle de unidad (0-255 / 0-100%)
    {
        int tx = borderGS + marginGS;
        for (int i = 0; i < 3; i++) {
            tabs[i]->activa = (pestania == i);
            tabs[i]->foco = (fila == -1 && pestania == i); // enfocada con flechas (verde)
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)tx, (GLfloat)tabsY, 0);
            tabs[i]->Render();
            w3dEngine::PopMatrix();
            tabs[i]->sx = x + tx;
            tabs[i]->sy = y + tabsY;
            tx += tabs[i]->width + gapGS;
        }
        unidadX = tx; // (el switch de unidad ahora es un boton abajo)
    }

    // valores de la pestania activa. RGB/HSV: los 3 juntos en UNA
    // tarjeta con borde grande; Alpha separado. Etiqueta a la izquierda
    // y el valor alineado al borde DERECHO.
    {
        int izq = borderGS + marginGS;
        int cajaW = popUpWindow->width - izq * 2;
        char buf[24];

        float vals[4];
        if (pestania == 1) {
            vals[0] = h; vals[1] = s; vals[2] = v; vals[3] = target[3];
        } else {
            vals[0] = target[0]; vals[1] = target[1];
            vals[2] = target[2]; vals[3] = target[3];
        }
        static const char* EtiquetasRGB[4] = { "Red", "Green", "Blue", "Alpha" };
        static const char* EtiquetasHSV[4] = { "Hue", "Saturation", "Value", "Alpha" };

        if (pestania == 2) {
            // Hex: una sola fila con su tarjeta
            filaCard->Resize(cajaW, RenglonHeightGS + GlobalScale * 2);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)izq, (GLfloat)(filasY + gapGS), 0);
            w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
            filaCard->Render(false);
            w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            if (fila == 0) filaCard->RenderBorder(false);
            sprintf(buf, "%02X%02X%02X%02X",
                    (int)(target[0] * 255.0f + 0.5f),
                    (int)(target[1] * 255.0f + 0.5f),
                    (int)(target[2] * 255.0f + 0.5f),
                    (int)(target[3] * 255.0f + 0.5f));
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)GlobalScale, 0);
            w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
            RenderBitmapText("Hex", textAlign::left, cajaW / 2);
            w3dEngine::PopMatrix();
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(cajaW - gapGS * 2), (GLfloat)GlobalScale, 0);
            w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            RenderBitmapText(buf, textAlign::right, cajaW - gapGS * 4);
            w3dEngine::PopMatrix();
            w3dEngine::PopMatrix();
        } else {
            // tarjeta-GRUPO con los 3 valores adentro
            filaCard->Resize(cajaW, grupoH);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)izq, (GLfloat)(filasY + gapGS), 0);
            w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
            filaCard->Render(false);
            // el borde grande: accent si la fila activa esta adentro
            if (fila < 3) {
                if (arrastre == 3) w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
                else w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
                filaCard->RenderBorder(false);
            }
            for (int i = 0; i < 3; i++) {
                int slotY = GlobalScale + i * (RenglonHeightGS + GlobalScale);
                // relleno (barra de progreso) con margen de 1 pixel
                float frac = vals[i];
                int rellenoW = (int)((cajaW - GlobalScale * 2) * frac);
                if (rellenoW > 0) {
                    w3dEngine::Disable(w3dEngine::Texture2D);
                    w3dEngine::Color4f(accent[0], accent[1], accent[2], 0.55f);
                    rect->SetSize((GLshort)GlobalScale, (GLshort)slotY,
                                  (GLshort)rellenoW, (GLshort)RenglonHeightGS);
                    rect->RenderObject(false);
                    w3dEngine::Enable(w3dEngine::Texture2D);
                }
                // etiqueta izquierda + valor a la DERECHA
                const char* etiqueta = (pestania == 0) ? EtiquetasRGB[i]
                                                       : EtiquetasHSV[i];
                if (pestania == 1) FormatoFloat3(buf, vals[i]);
                else if (ColorPickerUnidad) sprintf(buf, "%d%%", (int)(vals[i] * 100.0f + 0.5f));
                else sprintf(buf, "%d", (int)(vals[i] * 255.0f + 0.5f));
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)slotY, 0);
                if (i == fila) w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
                else w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
                RenderBitmapText(etiqueta, textAlign::left, cajaW / 2);
                w3dEngine::PopMatrix();
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(cajaW - gapGS * 2), (GLfloat)slotY, 0);
                if (i == fila) w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
                else w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
                RenderBitmapText(buf, textAlign::right, cajaW - gapGS * 4);
                w3dEngine::PopMatrix();
            }
            w3dEngine::PopMatrix();

            // ALPHA separado, con su propia tarjeta (como estaba)
            filaCard->Resize(cajaW, RenglonHeightGS + GlobalScale * 2);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)izq, (GLfloat)alphaY, 0);
            w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
            filaCard->Render(false);
            {
                int rellenoW = (int)((cajaW - GlobalScale * 2) * vals[3]);
                if (rellenoW > 0) {
                    w3dEngine::Disable(w3dEngine::Texture2D);
                    w3dEngine::Color4f(accent[0], accent[1], accent[2], 0.55f);
                    rect->SetSize((GLshort)GlobalScale, (GLshort)GlobalScale,
                                  (GLshort)rellenoW, (GLshort)RenglonHeightGS);
                    rect->RenderObject(false);
                    w3dEngine::Enable(w3dEngine::Texture2D);
                }
            }
            if (fila == 3) {
                if (arrastre == 3) w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
                else w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
                filaCard->RenderBorder(false);
            }
            // el alpha SIEMPRE es flotante 0..1, en RGB y en HSV por
            // igual (es el mismo valor: 0.540 aca y alla)
            FormatoFloat3(buf, vals[3]);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(gapGS * 2), (GLfloat)GlobalScale, 0);
            if (fila == 3) w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            else w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
            RenderBitmapText("Alpha", textAlign::left, cajaW / 2);
            w3dEngine::PopMatrix();
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(cajaW - gapGS * 2), (GLfloat)GlobalScale, 0);
            if (fila == 3) w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            else w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
            RenderBitmapText(buf, textAlign::right, cajaW - gapGS * 4);
            w3dEngine::PopMatrix();
            w3dEngine::PopMatrix();
        }
    }

    // boton de UNIDAD (ancho completo; el label dice a que cambia) y
    // abajo OK / Cancel repartidos 50% y 50%
    {
        int izq = borderGS + marginGS;
        int contW = popUpWindow->width - izq - marginGS - borderGS;
        if (pestania == 0) {
            // solo en RGB (HSV es flotante 0..1 y Hex no usa unidad)
            btnUnidad->text = ColorPickerUnidad ? "Switch to 0-255"
                                                : "Switch to 0-100%";
            btnUnidad->focoMenu = (fila == Filas()); // enfocado con flechas (la fila del switch en RGB)
            btnUnidad->Resize(contW);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)izq, (GLfloat)btnY, 0);
            btnUnidad->Render();
            w3dEngine::PopMatrix();
            btnUnidad->sx = x + izq;
            btnUnidad->sy = y + btnY;
        } else {
            btnUnidad->sx = btnUnidad->sy = -10000; // sin click en HSV
        }

        // OK a la DERECHA, Cancel a la IZQUIERDA (logica de popup S60, igual que el popup de borrar)
        int mitad = (contW - gapGS) / 2;
        int filaOk = Filas() + (pestania == 0 ? 1 : 0); // la fila virtual de OK/Cancel (despues del switch en RGB)
        btnOk->focoMenu     = (fila == filaOk && okFoco == 0); // enfocado con flechas (verde)
        btnCancel->focoMenu = (fila == filaOk && okFoco == 1);
        btnOk->Resize(mitad);
        btnCancel->Resize(mitad);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)izq, (GLfloat)btnOkY, 0); // Cancel IZQUIERDA
        btnCancel->Render();
        w3dEngine::PopMatrix();
        btnCancel->sx = x + izq;
        btnCancel->sy = y + btnOkY;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(izq + mitad + gapGS), (GLfloat)btnOkY, 0); // OK DERECHA
        btnOk->Render();
        w3dEngine::PopMatrix();
        btnOk->sx = x + izq + mitad + gapGS;
        btnOk->sy = y + btnOkY;
    }

    endView();
}

void ColorPicker::AjustarFila(int delta) {
    if (!target || pestania == 2) return; // hex: solo lectura por ahora
    float d = delta / 255.0f;
    if (pestania == 0) {
        target[fila] += d;
        if (target[fila] < 0.0f) target[fila] = 0.0f;
        if (target[fila] > 1.0f) target[fila] = 1.0f;
        if (fila < 3) DeRGB(); // el alpha no toca el HSV
    } else {
        if (fila == 3) {
            target[3] += d;
            if (target[3] < 0.0f) target[3] = 0.0f;
            if (target[3] > 1.0f) target[3] = 1.0f;
            return;
        }
        float* p = (fila == 0) ? &h : (fila == 1) ? &s : &v;
        *p += d;
        if (fila == 0) {
            // el matiz da la vuelta
            if (*p < 0.0f) *p += 1.0f;
            if (*p > 1.0f) *p -= 1.0f;
        } else {
            if (*p < 0.0f) *p = 0.0f;
            if (*p > 1.0f) *p = 1.0f;
        }
        AlRGB();
    }
}

bool ColorPicker::Click(int mx, int my) {
    if (arrastre != 0) {
        // otro click CONFIRMA el arrastre (Symbian: click para empezar,
        // click para terminar; el cursor vuelve de violeta a normal)
        arrastre = 0;
        return true;
    }
    if (!Contains(mx, my)) return false; // afuera: el caller cierra
    int lx = mx - x;
    int ly = my - y;

    // pestanias (y el toggle de unidad a su derecha)
    if (ly >= tabsY && ly < tabsY + tabAlto) {
        int tx = borderGS + marginGS;
        for (int i = 0; i < 3; i++) {
            if (lx >= tx && lx < tx + tabs[i]->width) {
                pestania = i;
                fila = 0;
                return true;
            }
            tx += tabs[i]->width + gapGS;
        }
        return true;
    }
    // circulo cromatico: setea H/S
    if (lx >= circX && lx < circX + circLado &&
        ly >= circY && ly < circY + circLado) {
        arrastre = 1;
        movio = false;
        Motion(mx, my);
        return true;
    }
    // barra de valor
    if (lx >= barraX - gapGS && lx < barraX + barraW + gapGS &&
        ly >= circY && ly < circY + circLado) {
        arrastre = 2;
        movio = false;
        Motion(mx, my);
        return true;
    }
    // boton de unidad: 0-255 <-> 0-100% (queda guardado en memoria)
    if (pestania == 0 && btnUnidad->Contains(mx, my)) {
        ColorPickerUnidad = ColorPickerUnidad ? 0 : 1;
        return true;
    }

    // botones OK / Cancel
    if (btnOk->Contains(mx, my)) {
        Cerrar(); // aceptar: el color ya esta aplicado en vivo
        return true;
    }
    if (btnCancel->Contains(mx, my)) {
        if (target) {
            target[0] = original[0];
            target[1] = original[1];
            target[2] = original[2];
            target[3] = original[3];
        }
        Cerrar();
        return true;
    }

    // valores: el grupo de 3 y la fila de Alpha (arrastran en X)
    if (ly >= filasY && ly < btnY - gapGS) {
        if (pestania == 2) {
            fila = 0;
            return true;
        }
        int grupoY = filasY + gapGS;
        if (ly >= grupoY && ly < grupoY + grupoH) {
            int f = (ly - grupoY - GlobalScale) / (RenglonHeightGS + GlobalScale);
            if (f < 0) f = 0;
            if (f > 2) f = 2;
            fila = f;
            arrastre = 3;
            movio = false;
            arrastreX = mx;
        } else if (ly >= alphaY && ly < alphaY + RenglonHeightGS + GlobalScale * 2) {
            fila = 3;
            arrastre = 3;
            movio = false;
            arrastreX = mx;
        }
        return true;
    }
    return true;
}

bool ColorPicker::Motion(int mx, int my) {
    if (arrastre != 0) {
        movio = true; // el up de PC ya puede soltar
        int margen = borderGS;
        if (arrastre == 3) {
            // arrastrando un VALOR: el mouse da la vuelta (wrap)
            int nx = mx;
            int ny = my;
            if (mx <= x + margen) nx = x + popUpWindow->width - margen - 1;
            else if (mx >= x + popUpWindow->width - margen) nx = x + margen + 1;
            if (my <= y + margen) ny = y + popUpWindow->height - margen - 1;
            else if (my >= y + popUpWindow->height - margen) ny = y + margen + 1;
            if (nx != mx || ny != my) {
                if (LayoutWarpMouse) LayoutWarpMouse(nx, ny);
                arrastreX = nx; // que el salto no cuente como delta
                return true;
            }
        } else if (arrastre == 1) {
            // el cursor NO se sale del circulo cromatico: se frena en su
            // borde (radio), nada de aparecer en otro lado del cuadrado
            float cx = (float)(x + circX) + circLado * 0.5f;
            float cy = (float)(y + circY) + circLado * 0.5f;
            float dxp = (float)mx - cx;
            float dyp = (float)my - cy;
            float dist = sqrtf(dxp * dxp + dyp * dyp);
            float radio = circLado * 0.5f - 1.0f;
            if (dist > radio && dist > 0.0f) {
                int nx = (int)(cx + dxp / dist * radio);
                int ny = (int)(cy + dyp / dist * radio);
                if (LayoutWarpMouse) LayoutWarpMouse(nx, ny);
                mx = nx;
                my = ny;
            }
        } else {
            // barra V: el cursor queda sobre la barra
            int nx = mx;
            int ny = my;
            if (nx < x + barraX) nx = x + barraX;
            if (nx > x + barraX + barraW) nx = x + barraX + barraW;
            if (ny < y + circY) ny = y + circY;
            if (ny > y + circY + circLado) ny = y + circY + circLado;
            if (nx != mx || ny != my) {
                if (LayoutWarpMouse) LayoutWarpMouse(nx, ny);
                mx = nx;
                my = ny;
            }
        }
    } else {
        // sin arrastre: si el mouse se aleja mucho se cierra solo
        // (ACEPTA, igual que los menus desplegables)
        int margen = 60 * GlobalScale;
        if (mx < x - margen || mx > x + popUpWindow->width + margen ||
            my < y - margen || my > y + popUpWindow->height + margen) {
            Cerrar();
            return true;
        }
    }
    btnOk->hover = btnOk->Contains(mx, my);
    btnCancel->hover = btnCancel->Contains(mx, my);
    btnUnidad->hover = (pestania == 0) && btnUnidad->Contains(mx, my);
    if (arrastre == 0) {
        return true; // sin arrastre: solo hover
    }
    int lx = mx - x;
    int ly = my - y;
    if (arrastre == 1) {
        // H por angulo (rojo ABAJO, horario), S por distancia al centro
        float cx = circX + circLado * 0.5f;
        float cy = circY + circLado * 0.5f;
        float dx = (lx - cx) / (circLado * 0.5f);
        float dy = (cy - ly) / (circLado * 0.5f);
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > 1.0f) dist = 1.0f;
        float ang = atan2f(dy, dx);
        h = (-1.5707963f - ang) / 6.2831853f;
        while (h < 0.0f) h += 1.0f;
        while (h > 1.0f) h -= 1.0f;
        s = dist;
        AlRGB();
    } else if (arrastre == 2) {
        float t = 1.0f - (float)(ly - circY) / (float)circLado;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        v = t;
        AlRGB();
    } else if (arrastre == 3) {
        int delta = mx - arrastreX;
        if (delta != 0) {
            arrastreX = mx;
            AjustarFila(delta); // 1 pixel = 1/255
        }
    }
    return true;
}

void ColorPicker::Soltar() {
    // PC: al soltar el boton, si hubo movimiento, termina el arrastre
    // (el tap de Symbian no mueve: queda en modo toggle hasta otro click)
    if (arrastre != 0 && movio) {
        arrastre = 0;
    }
}

bool ColorPicker::Arrastrando() {
    return arrastre != 0;
}

// aceleracion del hold (MISMA en todas las barras de color): arranca LENTO (1 por frame ~0.3s -> permite
// soltar en el valor exacto) y acelera DE A POCO (tope 8). Antes saltaba a 30 -> "muy rapido" (Dante).
int ColorPicker::PasoHold(int dir) {
    if (dir == holdDir) holdCount++; else { holdDir = dir; holdCount = 0; }
    int c = holdCount;
    return (c < 8) ? 1 : (c < 18) ? 2 : (c < 32) ? 4 : 8;
}

bool ColorPicker::Tecla(int tecla) {
    int filas = Filas();
    bool haySwitch = (pestania == 0);                 // el boton "Switch 0-255/0-100%" SOLO existe en RGB
    int filaSwitch = haySwitch ? filas : -100;        // (inalcanzable si no hay switch)
    int filaOk = filas + (haySwitch ? 1 : 0);         // ultima fila virtual: OK/Cancel
    // FOCO (fila): -3 CIRCULO, -2 barra VALUE, -1 PESTANIAS, 0..filas-1 FILAS, filaSwitch SWITCH, filaOk OK/Cancel

    // ---- MODO EDITAR CIRCULO: las flechas mueven Hue (izq/der) y Saturacion (arr/aba); OK/Enter/Cancel salen ----
    if (editCirculo) {
        float sat = 4.0f / 255.0f;              // SATURACION: paso fijo (Dante: la velocidad esta perfecta)
        switch (tecla) {
            // HUE: acelera como las barras pero topeado en 4/255 (el hue es mas sensible -> el circulo entero;
            // arranca lento para landing preciso y sube hasta la velocidad "perfecta" de la saturacion).
            case LayoutKey::Left:  { int p = PasoHold(-1); if (p > 4) p = 4; h -= (float)p / 255.0f; if (h < 0.0f) h += 1.0f; AlRGB(); return true; }
            case LayoutKey::Right: { int p = PasoHold(1);  if (p > 4) p = 4; h += (float)p / 255.0f; if (h > 1.0f) h -= 1.0f; AlRGB(); return true; }
            case LayoutKey::Up:    holdDir = holdCount = 0; s += sat; if (s > 1.0f) s = 1.0f;  AlRGB(); return true;
            case LayoutKey::Down:  holdDir = holdCount = 0; s -= sat; if (s < 0.0f) s = 0.0f;  AlRGB(); return true;
            case LayoutKey::Accept: case LayoutKey::Enter: case LayoutKey::Cancel:
                editCirculo = false; return true; // sale del modo edicion (el color queda aplicado)
        }
        return true;
    }

    // ---- MODO EDITAR BARRA VALUE (vertical): arriba/derecha suben v, abajo/izquierda bajan; OK/Cancel salen ----
    if (editValue) {
        int dir = 0;
        if (tecla == LayoutKey::Up || tecla == LayoutKey::Right) dir = 1;
        else if (tecla == LayoutKey::Down || tecla == LayoutKey::Left) dir = -1;
        if (dir != 0) {                         // ajuste con aceleracion (sirve para el hold del N95)
            int paso = PasoHold(dir);
            v += dir * paso / 255.0f; if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f; AlRGB();
            return true;
        }
        editValue = false; holdDir = holdCount = 0; // Accept/Enter/Cancel: salir del modo edicion
        return true;
    }

    switch (tecla) {
        case LayoutKey::Up:
            holdDir = holdCount = 0;
            fila--; if (fila < -3) fila = filaOk;   // wrap: arriba del circulo -> OK/Cancel
            return true;
        case LayoutKey::Down:
            holdDir = holdCount = 0;
            fila++; if (fila > filaOk) fila = -3;   // wrap: abajo de OK/Cancel -> circulo
            return true;
        case LayoutKey::Left:
        case LayoutKey::Right: {
            int dir = (tecla == LayoutKey::Right) ? 1 : -1;
            if (fila == -3) {                       // CIRCULO: der -> barra VALUE (esta a su derecha); el circulo se edita con OK
                holdDir = holdCount = 0;
                if (dir > 0) fila = -2;
            } else if (fila == -2) {                // barra VALUE: izq -> CIRCULO (a su izquierda); el value se edita con OK (arr/aba)
                holdDir = holdCount = 0;
                if (dir < 0) fila = -3;
            } else if (fila == -1) {                // PESTANIAS: cambia RGB / HSV / Hex
                holdDir = holdCount = 0;
                pestania += dir; if (pestania < 0) pestania = 2; if (pestania > 2) pestania = 0;
            } else if (fila == filaSwitch) {        // SWITCH 0-255 <-> 0-100%
                holdDir = holdCount = 0;
                ColorPickerUnidad = ColorPickerUnidad ? 0 : 1;
            } else if (fila == filaOk) {            // OK (derecha) / Cancel (izquierda)
                holdDir = holdCount = 0;
                okFoco = (dir > 0) ? 0 : 1;         // der = OK (0), izq = Cancel (1)
            } else {                                // FILA de valor R/G/B/A: ACELERACION (tap preciso, hold rampa)
                AjustarFila(dir * PasoHold(dir));
            }
            return true;
        }
        case LayoutKey::Accept:
        case LayoutKey::Enter:
            if (fila == -3) {                       // CIRCULO: OK = entrar a editar (flechas mueven H/S)
                editCirculo = true;
            } else if (fila == -2) {                // BARRA VALUE: OK = entrar a editar (arr/aba ajustan v)
                editValue = true;
            } else if (fila == filaSwitch) {        // SWITCH: OK lo togglea
                ColorPickerUnidad = ColorPickerUnidad ? 0 : 1;
            } else if (fila == filaOk && okFoco == 1) { // Cancel enfocado: restaurar
                if (target) { target[0]=original[0]; target[1]=original[1]; target[2]=original[2]; target[3]=original[3]; }
                Cerrar();
            } else {                                // OK enfocado / cualquier otra fila: aceptar (ya esta vivo)
                Cerrar();
            }
            return true;
        case LayoutKey::Cancel:                     // esc / C del telefono / X: restaurar el original
            if (target) { target[0]=original[0]; target[1]=original[1]; target[2]=original[2]; target[3]=original[3]; }
            Cerrar();
            return true;
    }
    return true; // modal: no le roban teclas
}

// flecha MANTENIDA (frame-based, keypad N95 que no auto-repite rapido): SOLO ajusta valores, NUNCA
// navega -> mantener una flecha para subir/bajar R/G/B/A o editar circulo/value es FLUIDO (con la misma
// aceleracion del hold). La navegacion entre elementos sigue 1-por-tap (via Tecla en el key-down).
bool ColorPicker::TeclaRepeat(int tecla) {
    if (editCirculo || editValue) return Tecla(tecla);   // en edicion las flechas AJUSTAN -> repetir todas
    if (fila >= 0 && fila < Filas() &&                    // fila de valor R/G/B/A: izq/der ajusta -> repetir
        (tecla == LayoutKey::Left || tecla == LayoutKey::Right))
        return Tecla(tecla);
    return true; // navegacion (circulo/value/pestanias/switch/OK-Cancel): el repeat NO hace nada
}
