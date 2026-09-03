// ============================================================================
//  EDITOR DE FALLOFF: lista de presets + curva editable. Ver FalloffEditor.h para
//  el contrato (es REUTILIZABLE: edita el W3dFalloff* de quien sea, como el
//  ColorPicker edita el color de quien sea). C++03 (compila en Symbian).
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"                 // T(): los nombres de los presets en el idioma del sistema
#include "FalloffEditor.h"
#include "ViewPorts/LayoutInput.h"   // LayoutKey
#include "WhiskUI/core/UI.h"
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/draw/icons.h"      // IconoIndice / IconsUV / IconMesh
#include "objects/Textures.h"        // el atlas de la UI (iconos + texto)

namespace gfx = w3dEngine;
extern bool g_redraw;

FalloffEditor* falloffEditor = NULL;

// cuantas muestras se toman de la curva para dibujarla. 48 alcanza para que un
// preset agudo (sharper) no se vea poligonal en un lienzo de ~120px.
static const int kMuestras = 48;

FalloffEditor::FalloffEditor() : PopUpBase("Falloff") {
    target = NULL;
    selPunto = -1;
    foco = 0;
    editandoPunto = false;
    arrastre = 0;
    rect = new Rec2D();
    filaCard = new Card(NULL, 10, 10);
    btnBorrar = new Button("X");
    btnBorrar->centrado = true;
    listaY = filaH = 0;
    lienzoX = lienzoY = lienzoW = lienzoH = 0;
    btnY = 0;
}

FalloffEditor::~FalloffEditor() {
    delete rect;
    delete filaCard;
    delete btnBorrar;
}

bool FalloffEditor::EsCustom() const { return target && target->tipo == FoCustom; }

void FalloffEditor::Abrir(W3dFalloff* Target, int px, int py) {
    target = Target;
    if (target) original = *target;   // copia entera (tipo + puntos) para poder cancelar
    selPunto = -1;
    foco = target ? target->tipo : 0;
    editandoPunto = false;
    arrastre = 0;
    x = px; y = py;
    Reflow();
    PopUpActive = this;
    g_redraw = true;
}

void FalloffEditor::OlvidarDueno() {
    // el dueno del W3dFalloff se esta destruyendo: soltar el puntero SIN tocarlo y
    // cerrar. Mismo contrato que ColorPicker::OlvidarDueno (ver el bloque ahi: un
    // popup no puede sobrevivir al dato que edita).
    target = NULL;
    arrastre = 0; editandoPunto = false;
    if (PopUpActive == this) PopUpActive = NULL;
}

void FalloffEditor::Reflow() {
    const int pad = borderGS + GlobalScale * 3;
    filaH = RenglonHeightGS + GlobalScale * 2;

    // ancho: que entre el nombre mas largo con su icono, y que el lienzo no quede
    // ridiculo. El nombre mas largo es "Inverse Square" (o su traduccion).
    int anchoTexto = 0;
    for (int i = 0; i < FoTotal; i++) {
        const char* n = T(W3dFalloffNombre(i));
        int len = 0; while (n && n[len]) len++;
        const int w = len * CharacterWidthGS;
        if (w > anchoTexto) anchoTexto = w;
    }
    int w = pad * 2 + IconSizeGS + gapGS + anchoTexto + gapGS * 2;
    // ...y que entre TAMBIEN la linea de ayuda del pie: si no, se dibuja cortada (el ancho lo
    // mandaba solo la lista y en castellano el texto es mas largo que el nombre mas largo).
    {
        const char* pie[2] = { T("Drag the points, click to add"), T("Custom: edit the curve") };
        for (int k = 0; k < 2; k++) {
            int len = 0; while (pie[k][len]) len++;
            const int wPie = pad * 2 + RenglonHeightGS * 2 + gapGS + len * CharacterWidthGS + gapGS;
            if (wPie > w) w = wPie;
        }
    }
    const int minW = 130 * GlobalScale;
    if (w < minW) w = minW;

    listaY = borderGS + RenglonHeightGS + gapGS;      // bajo el titulo
    lienzoX = pad;
    lienzoW = w - pad * 2;
    lienzoH = 90 * GlobalScale;
    lienzoY = listaY + FoTotal * filaH + gapGS;
    btnY = lienzoY + lienzoH + gapGS;

    int hTotal = btnY + RenglonHeightGS + pad;
    popUpWindow->Resize(w, hTotal);

    btnBorrar->Resize(w - pad * 2);
    btnBorrar->width = RenglonHeightGS * 2;

    if (x + w > MenuPantallaW) x = MenuPantallaW - w;
    if (y + hTotal > MenuPantallaH) y = MenuPantallaH - hTotal;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
}

// ---------------------------------------------------------------------------
//  Mapeo lienzo <-> curva. El lienzo es un GRAFICO: x crece a la derecha
//  (distancia al centro del pincel) e y crece hacia ARRIBA (intensidad).
// ---------------------------------------------------------------------------
void FalloffEditor::CurvaAPantalla(float cx, float cy, int& sx, int& sy) const {
    sx = lienzoX + (int)(cx * (float)lienzoW);
    sy = lienzoY + lienzoH - (int)(cy * (float)lienzoH);
}
void FalloffEditor::PantallaACurva(int sx, int sy, float& cx, float& cy) const {
    cx = (lienzoW > 0) ? (float)(sx - lienzoX) / (float)lienzoW : 0.0f;
    cy = (lienzoH > 0) ? (float)(lienzoY + lienzoH - sy) / (float)lienzoH : 0.0f;
    if (cx < 0.0f) cx = 0.0f; if (cx > 1.0f) cx = 1.0f;
    if (cy < 0.0f) cy = 0.0f; if (cy > 1.0f) cy = 1.0f;
}
bool FalloffEditor::EnLienzo(int mx, int my) const {
    const int lx = mx - x, ly = my - y;
    return lx >= lienzoX && lx < lienzoX + lienzoW && ly >= lienzoY && ly < lienzoY + lienzoH;
}

// ---------------------------------------------------------------------------
//  Render
// ---------------------------------------------------------------------------
void FalloffEditor::Render() {
    if (!target) return;
    initView();
    gfx::BindTexture(Textures[0]->iID);

    const float* gris   = ListaColores[static_cast<int>(ColorID::gris)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];

    gfx::Color4f(gris[0], gris[1], gris[2], 1.0f);
    popUpWindow->Render(false);
    gfx::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // titulo
    gfx::PushMatrix();
    gfx::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)borderGS, 0);
    gfx::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
    RenderBitmapText(T("Falloff"), textAlign::left, popUpWindow->width - bordersGS);
    gfx::PopMatrix();

    // ---- la LISTA de tipos ----
    const int pad = borderGS + GlobalScale * 3;
    for (int i = 0; i < FoTotal; i++) {
        const int fy = listaY + i * filaH;
        const bool activo = (target->tipo == i);
        const bool enfocado = (foco == i);
        if (activo || enfocado) {   // fondo de la fila activa / enfocada
            gfx::Color4f(blanco[0], blanco[1], blanco[2], enfocado ? 0.20f : 0.10f);
            filaCard->Resize(popUpWindow->width - pad * 2, filaH);
            gfx::PushMatrix();
            gfx::Translatef((GLfloat)pad, (GLfloat)fy, 0);
            filaCard->RenderObject(false);
            gfx::PopMatrix();
        }
        // icono del preset (si ya tiene arte; si no, solo el texto)
        const int ic = IconoIndice(W3dFalloffIcono(i));
        const float* col = activo ? accent : (enfocado ? blanco : grisUI);
        gfx::Color4f(col[0], col[1], col[2], 1.0f);
        if (ic >= 0 && ic < (int)IconsUV.size()) {
            gfx::PushMatrix();
            gfx::Translatef((GLfloat)(pad + gapGS), (GLfloat)(fy + (filaH - IconSizeGS) / 2), 0);
            W3dDrawStrip4(IconMesh, IconsUV[ic]->uvs);
            gfx::PopMatrix();
        }
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)(pad + gapGS + IconSizeGS + gapGS),
                        (GLfloat)(fy + (filaH - LetterHeightGS) / 2), 0);
        gfx::Color4f(col[0], col[1], col[2], 1.0f);
        RenderBitmapText(T(W3dFalloffNombre(i)), textAlign::left, popUpWindow->width - pad * 2);
        gfx::PopMatrix();
    }

    // ---- el LIENZO de la curva (las partes planas van sin textura) ----
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    // fondo
    gfx::Color4f(0.10f, 0.10f, 0.10f, 1.0f);
    rect->SetSize((GLshort)lienzoX, (GLshort)lienzoY, (GLshort)lienzoW, (GLshort)lienzoH);
    rect->RenderObject(false);
    // grilla: los cuartos, para poder leer alturas sin numeros
    gfx::Color4f(1.0f, 1.0f, 1.0f, 0.10f);
    for (int g = 1; g < 4; g++) {
        rect->SetSize((GLshort)lienzoX, (GLshort)(lienzoY + lienzoH * g / 4),
                      (GLshort)lienzoW, (GLshort)GlobalScale);
        rect->RenderObject(false);
        rect->SetSize((GLshort)(lienzoX + lienzoW * g / 4), (GLshort)lienzoY,
                      (GLshort)GlobalScale, (GLshort)lienzoH);
        rect->RenderObject(false);
    }
    // la CURVA: se muestrea el falloff REAL (el mismo Eval que usa el pincel), asi que
    // lo que se ve es literalmente lo que se va a pintar -- presets incluidos.
    {
        static GLfloat pts[kMuestras * 2];
        for (int i = 0; i < kMuestras; i++) {
            const float t = (float)i / (float)(kMuestras - 1);
            int sx, sy;
            CurvaAPantalla(t, target->Eval(t), sx, sy);
            pts[i*2+0] = (GLfloat)sx;
            pts[i*2+1] = (GLfloat)sy;
        }
        gfx::EnableArray(gfx::VertexArray);
        gfx::VertexPointer2f(0, pts);
        gfx::LineWidth(2.0f * (float)GlobalScale);
        gfx::Color4f(accent[0], accent[1], accent[2], 1.0f);
        gfx::DrawLineStrip(kMuestras);
        gfx::LineWidth(1.0f);
    }
    // los PUNTOS: solo en Custom (con un preset la curva es de solo lectura)
    if (EsCustom()) {
        const int lado = 5 * GlobalScale;
        for (size_t i = 0; i < target->puntos.size(); i++) {
            int sx, sy;
            CurvaAPantalla(target->puntos[i].x, target->puntos[i].y, sx, sy);
            gfx::Color4f(0.0f, 0.0f, 0.0f, 1.0f);   // borde negro
            rect->SetSize((GLshort)(sx - lado/2 - GlobalScale), (GLshort)(sy - lado/2 - GlobalScale),
                          (GLshort)(lado + GlobalScale*2), (GLshort)(lado + GlobalScale*2));
            rect->RenderObject(false);
            const bool sel = ((int)i == selPunto);
            if (sel) gfx::Color4f(accent[0], accent[1], accent[2], 1.0f);
            else     gfx::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            rect->SetSize((GLshort)(sx - lado/2), (GLshort)(sy - lado/2), (GLshort)lado, (GLshort)lado);
            rect->RenderObject(false);
        }
    }
    // marco del lienzo (accent si el teclado esta parado ahi)
    const bool focoLienzo = (foco == FoTotal);
    if (focoLienzo) gfx::Color4f(accent[0], accent[1], accent[2], 1.0f);
    else            gfx::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
    rect->SetSize((GLshort)lienzoX, (GLshort)lienzoY, (GLshort)lienzoW, (GLshort)GlobalScale);
    rect->RenderObject(false);
    rect->SetSize((GLshort)lienzoX, (GLshort)(lienzoY + lienzoH - GlobalScale), (GLshort)lienzoW, (GLshort)GlobalScale);
    rect->RenderObject(false);
    rect->SetSize((GLshort)lienzoX, (GLshort)lienzoY, (GLshort)GlobalScale, (GLshort)lienzoH);
    rect->RenderObject(false);
    rect->SetSize((GLshort)(lienzoX + lienzoW - GlobalScale), (GLshort)lienzoY, (GLshort)GlobalScale, (GLshort)lienzoH);
    rect->RenderObject(false);

    // ---- pie: con Custom, el boton de BORRAR el punto elegido; si no, la ayuda ----
    gfx::Enable(gfx::Texture2D);
    gfx::EnableArray(gfx::TexCoordArray);
    if (EsCustom()) {
        btnBorrar->sx = x + lienzoX; btnBorrar->sy = y + btnY;
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)lienzoX, (GLfloat)btnY, 0);
        btnBorrar->Render();
        gfx::PopMatrix();
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)(lienzoX + btnBorrar->width + gapGS),
                        (GLfloat)(btnY + (RenglonHeightGS - LetterHeightGS) / 2), 0);
        gfx::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
        RenderBitmapText(T("Drag the points, click to add"), textAlign::left,
                         popUpWindow->width - lienzoX - btnBorrar->width - gapGS * 2);
        gfx::PopMatrix();
    } else {
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)lienzoX, (GLfloat)(btnY + (RenglonHeightGS - LetterHeightGS) / 2), 0);
        gfx::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
        RenderBitmapText(T("Custom: edit the curve"), textAlign::left, lienzoW);
        gfx::PopMatrix();
    }
    endView();
}

// ---------------------------------------------------------------------------
//  Mouse / dedo
// ---------------------------------------------------------------------------
bool FalloffEditor::Click(int mx, int my) {
    if (!target) return false;
    if (arrastre != 0) {  // Symbian: click empieza, click termina
        arrastre = 0;
        return true;
    }
    if (!Contains(mx, my)) return false;   // afuera: el caller cierra
    const int lx = mx - x, ly = my - y;

    // lista de tipos
    if (ly >= listaY && ly < listaY + FoTotal * filaH) {
        const int i = (ly - listaY) / filaH;
        if (i >= 0 && i < FoTotal) {
            target->tipo = i;
            foco = i;
            selPunto = -1;
            g_redraw = true;
        }
        return true;
    }
    // boton borrar punto (solo Custom)
    if (EsCustom() && ly >= btnY && ly < btnY + RenglonHeightGS &&
        lx >= lienzoX && lx < lienzoX + btnBorrar->width) {
        if (selPunto > 0 && selPunto < (int)target->puntos.size() - 1) {
            target->CurvaBorrar(selPunto);
            selPunto = -1;
            g_redraw = true;
        }
        return true;
    }
    // lienzo: agarrar un punto, o agregar uno nuevo donde se toco
    if (EnLienzo(mx, my)) {
        foco = FoTotal;
        if (!EsCustom()) return true;      // preset: la curva es de solo lectura
        float cx, cy;
        PantallaACurva(lx, ly, cx, cy);
        // radio de agarre generoso: con el dedo un punto de 5px no se toca nunca
        const float radio = (float)(10 * GlobalScale) / (float)(lienzoW > 0 ? lienzoW : 1);
        int i = target->CurvaCercano(cx, cy, radio);
        if (i < 0) i = target->CurvaAgregar(cx, cy);   // en el vacio: punto nuevo ahi
        selPunto = i;
        arrastre = 1;
        g_redraw = true;
        return true;
    }
    return true;   // adentro del popup pero en un hueco: se consume igual
}

bool FalloffEditor::Motion(int mx, int my) {
    if (!target || arrastre != 1 || selPunto < 0) return false;
    float cx, cy;
    PantallaACurva(mx - x, my - y, cx, cy);
    target->CurvaMover(selPunto, cx, cy);
    g_redraw = true;
    return true;
}

void FalloffEditor::Soltar() { arrastre = 0; }
bool FalloffEditor::Arrastrando() { return arrastre != 0; }

// ---------------------------------------------------------------------------
//  Teclado (keypad de Symbian incluido)
// ---------------------------------------------------------------------------
bool FalloffEditor::Tecla(int tecla) {
    if (!target) return true;
    const int nFilas = FoTotal + 1;    // las filas + el lienzo
    switch (tecla) {
        case LayoutKey::Up:
            if (editandoPunto) {       // moviendo un punto: la flecha lo SUBE
                if (selPunto >= 0) {
                    const W3dFalloffPunto& p = target->puntos[selPunto];
                    target->CurvaMover(selPunto, p.x, p.y + 0.05f);
                }
            } else { foco--; if (foco < 0) foco = nFilas - 1; }
            g_redraw = true; return true;
        case LayoutKey::Down:
            if (editandoPunto) {
                if (selPunto >= 0) {
                    const W3dFalloffPunto& p = target->puntos[selPunto];
                    target->CurvaMover(selPunto, p.x, p.y - 0.05f);
                }
            } else { foco++; if (foco >= nFilas) foco = 0; }
            g_redraw = true; return true;
        case LayoutKey::Left:
        case LayoutKey::Right: {
            const int dir = (tecla == LayoutKey::Right) ? +1 : -1;
            if (foco != FoTotal || !EsCustom()) return true;  // en la lista las flechas no hacen nada
            if (editandoPunto) {
                if (selPunto >= 0) {
                    const W3dFalloffPunto& p = target->puntos[selPunto];
                    target->CurvaMover(selPunto, p.x + 0.05f * dir, p.y);
                }
            } else {   // sin OK: cambiar DE punto
                const int n = (int)target->puntos.size();
                if (n > 0) {
                    if (selPunto < 0) selPunto = (dir > 0) ? 0 : n - 1;
                    else { selPunto += dir; if (selPunto < 0) selPunto = n - 1; if (selPunto >= n) selPunto = 0; }
                }
            }
            g_redraw = true; return true;
        }
        case LayoutKey::Enter:
            if (foco < FoTotal) { target->tipo = foco; selPunto = -1; }
            else if (EsCustom()) {
                // entrar/salir de mover el punto. Sin punto elegido, agarra el primero.
                if (selPunto < 0 && !target->puntos.empty()) selPunto = 0;
                editandoPunto = !editandoPunto;
            }
            g_redraw = true; return true;
        case LayoutKey::Cancel:
            if (editandoPunto) { editandoPunto = false; g_redraw = true; return true; }
            if (target) *target = original;    // C = cancelar: vuelve todo como estaba
            Cerrar();
            return true;
    }
    return true;   // modal: mientras esta abierto no le roban teclas
}

void FalloffEditor::Cerrar() {
    target = NULL;          // no sobrevivir al dato que se estaba editando
    arrastre = 0; editandoPunto = false; selPunto = -1;
    if (PopUpActive == this) PopUpActive = NULL;
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  LA PUERTA reutilizable
// ---------------------------------------------------------------------------
void FalloffEditorAbrir(W3dFalloff* target, int px, int py) {
    if (!target) return;
    if (!falloffEditor) falloffEditor = new FalloffEditor();
    falloffEditor->Abrir(target, px, py);
}

bool FalloffEditorActivo(const W3dFalloff* target) {
    if (!falloffEditor || PopUpActive != falloffEditor) return false;
    return target ? (falloffEditor->target == target) : true;
}
