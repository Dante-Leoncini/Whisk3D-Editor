// ===================================================================================================
//  UI COMPARTIDA del TRANSFORM en curso (G/R/S): barra de info, entrada numerica, botones de la
//  toolbar (tilde/cruz/ejes) y teclado numerico tactil. Extraido del viewport 3D (la referencia)
//  para que el UV editor y el Editor 2D se porten IGUAL sin copiar bloques. Ver TransformUI.h.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"     // T(): los textos salen en el idioma del sistema
#include "ViewPorts/TransformUI.h"
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"   // roles TBR_Accept/TBR_Cancel/TBR_AxisX/Y (los MISMOS del 3D)
#include "ViewPorts/NumInput.h"     // buffer/valor de la entrada numerica (compartida)
#include "ViewPorts/PopUp/NumPad.h" // NumPadOpenTransform (tap en la barra de info)
#include "WhiskUI/text/bitmapText.h"   // RenderBitmapText (texto de la barra de info)
#include "WhiskUI/widgets/card.h"      // Card (fondo de la barra)
#include "render/RenderOptions.h"      // g_redraw
#include <cstdio>   // sprintf (W3dFmtFloat)

namespace gfx = w3dEngine;
extern bool g_redraw;

// ---------------------------------------------------------------------------------------------
//  float -> texto portable (sin %f, que no anda en Symbian): parte entera con %d y los
//  decimales rellenados a mano. Era el W3dFmtF file-local de ViewPort3D.cpp; ahora es de los 3.
// ---------------------------------------------------------------------------------------------
std::string W3dFmtFloat(float v, int dec){
    char buf[48];
    int neg = (v < 0.0f) ? 1 : 0;
    float av = neg ? -v : v;
    int ent = (int)av;
    long mul = 1; for (int i = 0; i < dec; i++) mul *= 10;
    int frac = (int)((av - (float)ent) * (float)mul + 0.5f);
    if (frac >= (int)mul) { ent++; frac -= (int)mul; }
    sprintf(buf, "%s%d", neg ? "-" : "", ent);
    std::string r = buf;
    if (dec > 0){
        sprintf(buf, "%d", frac);
        std::string fs = buf;
        while ((int)fs.size() < dec) fs = "0" + fs;
        r += "." + fs;
    }
    return r;
}

// ---------------------------------------------------------------------------------------------
//  "[expr|] = valor" de la ENTRADA NUMERICA ( "Move: [(2*3)+3] = 9"). El caret
//  se dibuja en el medio (editable con las flechas del teclado tactil). Era parte del
//  W3dTextTransform del 3D; ahora los 3 editores arman su texto con esto.
// ---------------------------------------------------------------------------------------------
std::string TransformUITextNum(const char* unit){
    float v = 0.0f; bool valid = NumInputValue(v);
    std::string buf = NumInputBuffer();
    int ch = NumInputCaret(); if (ch < 0) ch = 0; if (ch > (int)buf.size()) ch = (int)buf.size();
    buf.insert(buf.begin() + ch, '|'); // caret visible
    std::string expr = (NumInputNegated() ? "-[" : "[") + buf + "]";
    std::string val = valid ? (W3dFmtFloat(v, 4) + unit) : "?";
    return expr + " = " + val;
}

// texto completo de la barra de info de un transform 2D (UV / Editor 2D). Mismo formato que
// el 3D: "Translate X: .. along X" / "Rotate  ..°" / "Scale: ..", y con entrada numerica
// "Move: [1/4|] = 0.2500  along X".
std::string TransformUIText2D(int mode, float d1, float d2, float ang, float fac,
                               int axisLock, const char* axisA, const char* axisB,
                               const char* unitName){
    if (NumInputActive()){
        const char* op = (mode == 2) ? T("Rotate") : (mode == 3) ? T("Scale") : T("Move");
        const char* unit = (mode == 2) ? "\xC2\xB0" : unitName;
        std::string suf;
        if (mode != 2 && axisLock == 1) suf = std::string("  ") + T("along") + " " + axisA;
        if (mode != 2 && axisLock == 2) suf = std::string("  ") + T("along") + " " + axisB;
        return std::string(op) + ": " + TransformUITextNum(unit) + suf;
    }
    if (mode == 2) return std::string(T("Rotate")) + "  " + W3dFmtFloat(ang, 1) + "\xC2\xB0";
    if (mode == 3){
        std::string s = std::string(T("Scale")) + ": " + W3dFmtFloat(fac, 4);
        if (axisLock == 1) s += std::string("  ") + T("along") + " " + axisA;
        if (axisLock == 2) s += std::string("  ") + T("along") + " " + axisB;
        return s;
    }
    // mover: un eje bloqueado muestra SOLO ese eje (como el 3D); libre muestra los dos
    if (axisLock == 1) return std::string(T("Translate")) + " " + axisA + ": " + W3dFmtFloat(d1, 4) + unitName +
                             "  " + T("along") + " " + axisA;
    if (axisLock == 2) return std::string(T("Translate")) + " " + axisB + ": " + W3dFmtFloat(d2, 4) + unitName +
                             "  " + T("along") + " " + axisB;
    return std::string(T("Translate")) + "  " + axisA + ": " + W3dFmtFloat(d1, 4) + unitName +
           "  " + axisB + ": " + W3dFmtFloat(d2, 4) + unitName;
}

// ---------------------------------------------------------------------------------------------
//  BARRA DE INFO durante un transform: la barra de menu se reemplaza por el fondo + el texto
//  en color de acento. Extraido del RenderBarTransform del 3D (que ahora la llama; el UV y
//  el 2D tambien). Se dibuja en el ortho LOCAL del viewport, como RenderBar.
// ---------------------------------------------------------------------------------------------
void ViewportBase::RenderBarInfo(const std::string& text){
    if (!barCard) return;
    int barH = BarHeight();
    int yBar = barDown ? height - barH : 0;

    // fondo de la barra (mas opaco que la normal para que el texto se lea)
    gfx::PushMatrix();
    gfx::Translatef(0, (GLfloat)yBar, 0);
    SetColorID(ColorID::gray, 0.85f);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);
    gfx::PopMatrix();

    // texto de estado en color de acento (modo activo), centrado vertical
    SetColorID(ColorID::accent);
    gfx::PushMatrix();
    int ty = yBar + (barH - LetterHeightGS) / 2;
    gfx::Translatef((GLfloat)(gapGS * 2), (GLfloat)ty, 0);
    RenderBitmapText(text, textAlign::left, width - gapGS * 2);
    gfx::PopMatrix();
}

// tap/click sobre la franja de la barra de info durante un transform del viewport -> teclado
// numerico en modo transform (edita el valor exacto, como tipear en PC). Mismo criterio que
// el ClickBarTransform del 3D, pero via los hooks compartidos (UV / Editor 2D).
bool TransformUIClickBar(ViewportBase* vp, int mx, int my){
    if (!vp || !vp->XformInProgress()) return false;
    int barH = vp->BarHeight();
    int yBar = vp->barDown ? (vp->y + vp->height - barH) : vp->y;
    if (mx < vp->x || mx >= vp->x + vp->width || my < yBar || my >= yBar + barH) return false;
    NumPadOpenTransform();
    return true;
}

// una expresion tipeada INVALIDA (no vacia y que no parsea, ej "1/") no deja confirmar:
// la barra ya muestra "= ?" como feedback. Sin entrada activa siempre se puede.
bool TransformUICanConfirm(){
    if (!NumInputActive()) return true;
    float v; return NumInputValue(v);
}

// ---------------------------------------------------------------------------------------------
//  TOOLBAR durante el transform: tilde/cruz/X/Y con los MISMOS roles y colores del 3D.
// ---------------------------------------------------------------------------------------------
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controls.cpp: hubo input tactil en la sesion
bool ToolbarUsesTouch(){ return g_lastFingerTicks != 0; }
#else
bool ToolbarUsesTouch(){ return false; } // (cuando el N8 tenga touch, cablear aca)
#endif

// colores de los ejes (X rojo / Y verde / Z azul) + rojos del cancelar; el fondo "iluminado"
// es el mismo color atenuado. Eran statics de ViewPort3D_Toolbar.cpp; ahora los usan los 3.
static const float kTbAxis[3][3] = { {0.90f,0.25f,0.25f}, {0.30f,0.85f,0.30f}, {0.35f,0.55f,1.00f} };
static float sTbAxisBg[3][3];
static const float kRedTb[3]   = { 0.92f, 0.28f, 0.24f };
static const float kRedTbBg[3] = { 0.41f, 0.13f, 0.11f };
static float sGreenTbBg[3];

const float* TbAxisColor(int e){ if (e < 0 || e > 2) e = 0; return kTbAxis[e]; }
const float* TbAxisBg(int e){
    if (e < 0 || e > 2) e = 0;
    for (int i = 0; i < 3; i++) sTbAxisBg[e][i] = kTbAxis[e][i] * 0.45f;
    return sTbAxisBg[e];
}
const float* TbRed(){ return kRedTb; }
const float* TbRedBg(){ return kRedTbBg; }
const float* TbGreenBg(){
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    for (int i = 0; i < 3; i++) sGreenTbBg[i] = accent[i] * 0.40f;
    return sGreenTbBg;
}

// crea tilde / cruz / X / Y (ocultos) al final de B. Mismos iconos y roles que el 3D.
void ToolbarCreateTransform2D(std::vector<Button*>& B){
    Button* b;
    b = new Button("", (int)IconType::notifOk);    b->role = TBR_Accept;  b->centered = true; b->visible = false; B.push_back(b);
    b = new Button("", (int)IconType::notifError); b->role = TBR_Cancel; b->centered = true; b->visible = false; B.push_back(b);
    b = new Button("X"); b->role = TBR_AxisX; b->centered = true; b->square = true; b->visible = false; B.push_back(b);
    b = new Button("Y"); b->role = TBR_AxisY; b->centered = true; b->square = true; b->visible = false; B.push_back(b);
}

// visibilidad + colores durante un G/R/S propio de un editor 2D. Mismas reglas que el 3D:
// los EJES se ven durante el transform; tilde/cruz se muestran SIEMPRE durante el transform
// (mouse incluido; antes solo en sesion tactil y el dueño no los veia en PC -> ahora son
// descubribles ademas de Enter/Esc/click). axisLock = -1 oculta los ejes (transform sin
// lock, ej: huesos 2D).
void ToolbarSyncTransform2D(std::vector<Button*>& B, bool transforming, int axisLock){
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    const float* white  = ColorList[static_cast<int>(ColorID::white)];
    Button* b;
    if ((b = BarRoleBtn(B, TBR_Accept))){
        b->visible = transforming;
        b->tint = TbGreenBg(); b->textColor = accent;
    }
    if ((b = BarRoleBtn(B, TBR_Cancel))){
        b->visible = transforming;
        b->tint = TbRedBg(); b->textColor = TbRed();
    }
    for (int e = 0; e < 2; e++){ // 0 = X, 1 = Y (en 2D no hay Z)
        if (!(b = BarRoleBtn(B, TBR_AxisX + e))) continue;
        b->visible = transforming && axisLock >= 0;
        bool on = (axisLock == e + 1);
        b->tint = on ? TbAxisBg(e) : NULL;             // encendido: fondo de SU color
        b->textColor = on ? white : TbAxisColor(e);    // apagado: la letra en su color
    }
}

// despacho de los roles de transform via los hooks del viewport (teclado y toolbar
// comparten el MISMO camino: XformToggleAxis es el que usan las teclas X/Y).
bool ToolbarActionTransform2D(ViewportBase* vp, int role){
    if (!vp) return false;
    if (role == TBR_Accept) { vp->XformConfirm(); return true; }
    if (role == TBR_Cancel){ vp->XformCancel();  return true; }
    if (role == TBR_AxisX)    { vp->XformToggleAxis(1); return true; }
    if (role == TBR_AxisY)    { vp->XformToggleAxis(2); return true; }
    return false;
}
