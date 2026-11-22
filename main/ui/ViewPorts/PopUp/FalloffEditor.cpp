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
#include "WhiskUI/draw/icons.h"      // IconIndex / IconsUV / IconMesh
#include "objects/Textures.h"        // el atlas de la UI (iconos + texto)

namespace gfx = w3dEngine;
extern bool g_redraw;

FalloffEditor* falloffEditor = NULL;

// cuantas muestras se toman de la curva para dibujarla. 48 alcanza para que un
// preset agudo (sharper) no se vea poligonal en un lienzo de ~120px.
static const int kSamples = 48;

FalloffEditor::FalloffEditor() : PopUpBase("Falloff") {
    target = NULL;
    selPoint = -1;
    focus = 0;
    editingPoint = false;
    drag = 0;
    rect = new Rec2D();
    rowCard = new Card(NULL, 10, 10);
    btnDelete = new Button("X");
    btnDelete->centered = true;
    listY = rowH = 0;
    canvasX = canvasY = canvasW = canvasH = 0;
    btnY = 0;
}

FalloffEditor::~FalloffEditor() {
    delete rect;
    delete rowCard;
    delete btnDelete;
}

bool FalloffEditor::IsCustom() const { return target && target->type == FoCustom; }

void FalloffEditor::Open(W3dFalloff* Target, int px, int py) {
    target = Target;
    if (target) original = *target;   // copia entera (tipo + puntos) para poder cancelar
    selPoint = -1;
    focus = target ? target->type : 0;
    editingPoint = false;
    drag = 0;
    x = px; y = py;
    Reflow();
    PopUpActive = this;
    g_redraw = true;
}

void FalloffEditor::ForgetOwner() {
    // el dueno del W3dFalloff se esta destruyendo: soltar el puntero SIN tocarlo y
    // cerrar. Mismo contrato que ColorPicker::ForgetOwner (ver el bloque ahi: un
    // popup no puede sobrevivir al dato que edita).
    target = NULL;
    drag = 0; editingPoint = false;
    if (PopUpActive == this) PopUpActive = NULL;
}

void FalloffEditor::Reflow() {
    const int pad = borderGS + GlobalScale * 3;
    rowH = RowHeightGS + GlobalScale * 2;

    // ancho: que entre el nombre mas largo con su icono, y que el lienzo no quede
    // ridiculo. El nombre mas largo es "Inverse Square" (o su traduccion).
    int textWidth = 0;
    for (int i = 0; i < FoTotal; i++) {
        const char* n = T(W3dFalloffName(i));
        int len = 0; while (n && n[len]) len++;
        const int w = len * CharacterWidthGS;
        if (w > textWidth) textWidth = w;
    }
    int w = pad * 2 + IconSizeGS + gapGS + textWidth + gapGS * 2;
    // ...y que entre TAMBIEN la linea de ayuda del pie: si no, se dibuja cortada (el ancho lo
    // mandaba solo la lista y en castellano el texto es mas largo que el nombre mas largo).
    {
        const char* foot[2] = { T("Drag the points, click to add"), T("Custom: edit the curve") };
        for (int k = 0; k < 2; k++) {
            int len = 0; while (foot[k][len]) len++;
            const int wFoot = pad * 2 + RowHeightGS * 2 + gapGS + len * CharacterWidthGS + gapGS;
            if (wFoot > w) w = wFoot;
        }
    }
    const int minW = 130 * GlobalScale;
    if (w < minW) w = minW;

    listY = borderGS + RowHeightGS + gapGS;      // bajo el titulo
    canvasX = pad;
    canvasW = w - pad * 2;
    canvasH = 90 * GlobalScale;
    canvasY = listY + FoTotal * rowH + gapGS;
    btnY = canvasY + canvasH + gapGS;

    int hTotal = btnY + RowHeightGS + pad;
    popUpWindow->Resize(w, hTotal);

    btnDelete->Resize(w - pad * 2);
    btnDelete->width = RowHeightGS * 2;

    if (x + w > MenuScreenW) x = MenuScreenW - w;
    if (y + hTotal > MenuScreenH) y = MenuScreenH - hTotal;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
}

// ---------------------------------------------------------------------------
//  Mapeo lienzo <-> curva. El lienzo es un GRAFICO: x crece a la derecha
//  (distancia al centro del pincel) e y crece hacia ARRIBA (intensidad).
// ---------------------------------------------------------------------------
void FalloffEditor::CurveToScreen(float cx, float cy, int& sx, int& sy) const {
    sx = canvasX + (int)(cx * (float)canvasW);
    sy = canvasY + canvasH - (int)(cy * (float)canvasH);
}
void FalloffEditor::ScreenToCurve(int sx, int sy, float& cx, float& cy) const {
    cx = (canvasW > 0) ? (float)(sx - canvasX) / (float)canvasW : 0.0f;
    cy = (canvasH > 0) ? (float)(canvasY + canvasH - sy) / (float)canvasH : 0.0f;
    if (cx < 0.0f) cx = 0.0f; if (cx > 1.0f) cx = 1.0f;
    if (cy < 0.0f) cy = 0.0f; if (cy > 1.0f) cy = 1.0f;
}
bool FalloffEditor::InCanvas(int mx, int my) const {
    const int lx = mx - x, ly = my - y;
    return lx >= canvasX && lx < canvasX + canvasW && ly >= canvasY && ly < canvasY + canvasH;
}

// ---------------------------------------------------------------------------
//  Render
// ---------------------------------------------------------------------------
void FalloffEditor::Render() {
    if (!target) return;
    initView();
    gfx::BindTexture(Textures[0]->iID);

    const float* gray   = ColorList[static_cast<int>(ColorID::gray)];
    const float* white  = ColorList[static_cast<int>(ColorID::white)];
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    const float* grayUI = ColorList[static_cast<int>(ColorID::grayUI)];

    gfx::Color4f(gray[0], gray[1], gray[2], 1.0f);
    popUpWindow->Render(false);
    gfx::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // titulo
    gfx::PushMatrix();
    gfx::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)borderGS, 0);
    gfx::Color4f(white[0], white[1], white[2], 1.0f);
    RenderBitmapText(T("Falloff"), textAlign::left, popUpWindow->width - bordersGS);
    gfx::PopMatrix();

    // ---- la LISTA de tipos ----
    const int pad = borderGS + GlobalScale * 3;
    for (int i = 0; i < FoTotal; i++) {
        const int fy = listY + i * rowH;
        const bool active = (target->type == i);
        const bool focused = (focus == i);
        if (active || focused) {   // fondo de la fila activa / enfocada
            gfx::Color4f(white[0], white[1], white[2], focused ? 0.20f : 0.10f);
            rowCard->Resize(popUpWindow->width - pad * 2, rowH);
            gfx::PushMatrix();
            gfx::Translatef((GLfloat)pad, (GLfloat)fy, 0);
            rowCard->RenderObject(false);
            gfx::PopMatrix();
        }
        // icono del preset (si ya tiene arte; si no, solo el texto)
        const int ic = IconIndex(W3dFalloffIcon(i));
        const float* col = active ? accent : (focused ? white : grayUI);
        gfx::Color4f(col[0], col[1], col[2], 1.0f);
        if (ic >= 0 && ic < (int)IconsUV.size()) {
            gfx::PushMatrix();
            gfx::Translatef((GLfloat)(pad + gapGS), (GLfloat)(fy + (rowH - IconSizeGS) / 2), 0);
            W3dDrawStrip4(IconMesh, IconsUV[ic]->uvs);
            gfx::PopMatrix();
        }
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)(pad + gapGS + IconSizeGS + gapGS),
                        (GLfloat)(fy + (rowH - LetterHeightGS) / 2), 0);
        gfx::Color4f(col[0], col[1], col[2], 1.0f);
        RenderBitmapText(T(W3dFalloffName(i)), textAlign::left, popUpWindow->width - pad * 2);
        gfx::PopMatrix();
    }

    // ---- el LIENZO de la curva (las partes planas van sin textura) ----
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::TexCoordArray);
    // fondo
    gfx::Color4f(0.10f, 0.10f, 0.10f, 1.0f);
    rect->SetSize((GLshort)canvasX, (GLshort)canvasY, (GLshort)canvasW, (GLshort)canvasH);
    rect->RenderObject(false);
    // grilla: los cuartos, para poder leer alturas sin numeros
    gfx::Color4f(1.0f, 1.0f, 1.0f, 0.10f);
    for (int g = 1; g < 4; g++) {
        rect->SetSize((GLshort)canvasX, (GLshort)(canvasY + canvasH * g / 4),
                      (GLshort)canvasW, (GLshort)GlobalScale);
        rect->RenderObject(false);
        rect->SetSize((GLshort)(canvasX + canvasW * g / 4), (GLshort)canvasY,
                      (GLshort)GlobalScale, (GLshort)canvasH);
        rect->RenderObject(false);
    }
    // la CURVA: se muestrea el falloff REAL (el mismo Eval que usa el pincel), asi que
    // lo que se ve es literalmente lo que se va a pintar -- presets incluidos.
    {
        static GLfloat pts[kSamples * 2];
        for (int i = 0; i < kSamples; i++) {
            const float t = (float)i / (float)(kSamples - 1);
            int sx, sy;
            CurveToScreen(t, target->Eval(t), sx, sy);
            pts[i*2+0] = (GLfloat)sx;
            pts[i*2+1] = (GLfloat)sy;
        }
        gfx::EnableArray(gfx::VertexArray);
        gfx::VertexPointer2f(0, pts);
        gfx::LineWidth(2.0f * (float)GlobalScale);
        gfx::Color4f(accent[0], accent[1], accent[2], 1.0f);
        gfx::DrawLineStrip(kSamples);
        gfx::LineWidth(1.0f);
    }
    // los PUNTOS: solo en Custom (con un preset la curva es de solo lectura)
    if (IsCustom()) {
        const int side = 5 * GlobalScale;
        for (size_t i = 0; i < target->points.size(); i++) {
            int sx, sy;
            CurveToScreen(target->points[i].x, target->points[i].y, sx, sy);
            gfx::Color4f(0.0f, 0.0f, 0.0f, 1.0f);   // borde negro
            rect->SetSize((GLshort)(sx - side/2 - GlobalScale), (GLshort)(sy - side/2 - GlobalScale),
                          (GLshort)(side + GlobalScale*2), (GLshort)(side + GlobalScale*2));
            rect->RenderObject(false);
            const bool sel = ((int)i == selPoint);
            if (sel) gfx::Color4f(accent[0], accent[1], accent[2], 1.0f);
            else     gfx::Color4f(white[0], white[1], white[2], 1.0f);
            rect->SetSize((GLshort)(sx - side/2), (GLshort)(sy - side/2), (GLshort)side, (GLshort)side);
            rect->RenderObject(false);
        }
    }
    // marco del lienzo (accent si el teclado esta parado ahi)
    const bool focusCanvas = (focus == FoTotal);
    if (focusCanvas) gfx::Color4f(accent[0], accent[1], accent[2], 1.0f);
    else            gfx::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
    rect->SetSize((GLshort)canvasX, (GLshort)canvasY, (GLshort)canvasW, (GLshort)GlobalScale);
    rect->RenderObject(false);
    rect->SetSize((GLshort)canvasX, (GLshort)(canvasY + canvasH - GlobalScale), (GLshort)canvasW, (GLshort)GlobalScale);
    rect->RenderObject(false);
    rect->SetSize((GLshort)canvasX, (GLshort)canvasY, (GLshort)GlobalScale, (GLshort)canvasH);
    rect->RenderObject(false);
    rect->SetSize((GLshort)(canvasX + canvasW - GlobalScale), (GLshort)canvasY, (GLshort)GlobalScale, (GLshort)canvasH);
    rect->RenderObject(false);

    // ---- pie: con Custom, el boton de BORRAR el punto elegido; si no, la ayuda ----
    gfx::Enable(gfx::Texture2D);
    gfx::EnableArray(gfx::TexCoordArray);
    if (IsCustom()) {
        btnDelete->sx = x + canvasX; btnDelete->sy = y + btnY;
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)canvasX, (GLfloat)btnY, 0);
        btnDelete->Render();
        gfx::PopMatrix();
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)(canvasX + btnDelete->width + gapGS),
                        (GLfloat)(btnY + (RowHeightGS - LetterHeightGS) / 2), 0);
        gfx::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
        RenderBitmapText(T("Drag the points, click to add"), textAlign::left,
                         popUpWindow->width - canvasX - btnDelete->width - gapGS * 2);
        gfx::PopMatrix();
    } else {
        gfx::PushMatrix();
        gfx::Translatef((GLfloat)canvasX, (GLfloat)(btnY + (RowHeightGS - LetterHeightGS) / 2), 0);
        gfx::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
        RenderBitmapText(T("Custom: edit the curve"), textAlign::left, canvasW);
        gfx::PopMatrix();
    }
    endView();
}

// ---------------------------------------------------------------------------
//  Mouse / dedo
// ---------------------------------------------------------------------------
bool FalloffEditor::Click(int mx, int my) {
    if (!target) return false;
    if (drag != 0) {  // Symbian: click empieza, click termina
        drag = 0;
        return true;
    }
    if (!Contains(mx, my)) return false;   // afuera: el caller cierra
    const int lx = mx - x, ly = my - y;

    // lista de tipos
    if (ly >= listY && ly < listY + FoTotal * rowH) {
        const int i = (ly - listY) / rowH;
        if (i >= 0 && i < FoTotal) {
            target->type = i;
            focus = i;
            selPoint = -1;
            g_redraw = true;
        }
        return true;
    }
    // boton borrar punto (solo Custom)
    if (IsCustom() && ly >= btnY && ly < btnY + RowHeightGS &&
        lx >= canvasX && lx < canvasX + btnDelete->width) {
        if (selPoint > 0 && selPoint < (int)target->points.size() - 1) {
            target->CurveDelete(selPoint);
            selPoint = -1;
            g_redraw = true;
        }
        return true;
    }
    // lienzo: agarrar un punto, o agregar uno nuevo donde se toco
    if (InCanvas(mx, my)) {
        focus = FoTotal;
        if (!IsCustom()) return true;      // preset: la curva es de solo lectura
        float cx, cy;
        ScreenToCurve(lx, ly, cx, cy);
        // radio de agarre generoso: con el dedo un punto de 5px no se toca nunca
        const float radius = (float)(10 * GlobalScale) / (float)(canvasW > 0 ? canvasW : 1);
        int i = target->NearCurve(cx, cy, radius);
        if (i < 0) i = target->CurveAdd(cx, cy);   // en el vacio: punto nuevo ahi
        selPoint = i;
        drag     = 1;
        g_redraw = true;
        return true;
    }
    return true;   // adentro del popup pero en un hueco: se consume igual
}

bool FalloffEditor::Motion(int mx, int my) {
    if (!target || drag != 1 || selPoint < 0) return false;
    float cx, cy;
    ScreenToCurve(mx - x, my - y, cx, cy);
    target->CurveMove(selPoint, cx, cy);
    g_redraw = true;
    return true;
}

void FalloffEditor::Release() { drag = 0; }
bool FalloffEditor::Dragging() { return drag != 0; }

// ---------------------------------------------------------------------------
//  Teclado (keypad de Symbian incluido)
// ---------------------------------------------------------------------------
bool FalloffEditor::Key(int key) {
    if (!target) return true;
    const int nRows = FoTotal + 1;    // las filas + el lienzo
    switch (key) {
        case LayoutKey::Up:
            if (editingPoint) {       // moviendo un punto: la flecha lo SUBE
                if (selPoint >= 0) {
                    const W3dFalloffPoint& p = target->points[selPoint];
                    target->CurveMove(selPoint, p.x, p.y + 0.05f);
                }
            } else { focus--; if (focus < 0) focus = nRows - 1; }
            g_redraw = true; return true;
        case LayoutKey::Down:
            if (editingPoint) {
                if (selPoint >= 0) {
                    const W3dFalloffPoint& p = target->points[selPoint];
                    target->CurveMove(selPoint, p.x, p.y - 0.05f);
                }
            } else { focus++; if (focus >= nRows) focus = 0; }
            g_redraw = true; return true;
        case LayoutKey::Left:
        case LayoutKey::Right: {
            const int dir = (key == LayoutKey::Right) ? +1 : -1;
            if (focus != FoTotal || !IsCustom()) return true;  // en la lista las flechas no hacen nada
            if (editingPoint) {
                if (selPoint >= 0) {
                    const W3dFalloffPoint& p = target->points[selPoint];
                    target->CurveMove(selPoint, p.x + 0.05f * dir, p.y);
                }
            } else {   // sin OK: cambiar DE punto
                const int n = (int)target->points.size();
                if (n > 0) {
                    if (selPoint < 0) selPoint = (dir > 0) ? 0 : n - 1;
                    else { selPoint += dir; if (selPoint < 0) selPoint = n - 1; if (selPoint >= n) selPoint = 0; }
                }
            }
            g_redraw = true; return true;
        }
        case LayoutKey::Enter:
            if (focus < FoTotal) { target->type = focus; selPoint = -1; }
            else if (IsCustom()) {
                // entrar/salir de mover el punto. Sin punto elegido, agarra el primero.
                if (selPoint < 0 && !target->points.empty()) selPoint = 0;
                editingPoint = !editingPoint;
            }
            g_redraw = true; return true;
        case LayoutKey::Cancel:
            if (editingPoint) { editingPoint = false; g_redraw = true; return true; }
            if (target) *target = original;    // C = cancelar: vuelve todo como estaba
            Close();
            return true;
    }
    return true;   // modal: mientras esta abierto no le roban teclas
}

void FalloffEditor::Close() {
    target = NULL;          // no sobrevivir al dato que se estaba editando
    drag = 0; editingPoint = false; selPoint = -1;
    if (PopUpActive == this) PopUpActive = NULL;
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  LA PUERTA reutilizable
// ---------------------------------------------------------------------------
void FalloffEditorOpen(W3dFalloff* target, int px, int py) {
    if (!target) return;
    if (!falloffEditor) falloffEditor = new FalloffEditor();
    falloffEditor->Open(target, px, py);
}

bool FalloffEditorActive(const W3dFalloff* target) {
    if (!falloffEditor || PopUpActive != falloffEditor) return false;
    return target ? (falloffEditor->target == target) : true;
}
