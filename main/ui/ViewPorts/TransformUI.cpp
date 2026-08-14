// ===================================================================================================
//  UI COMPARTIDA del TRANSFORM en curso (G/R/S): barra de info, entrada numerica, botones de la
//  toolbar (tilde/cruz/ejes) y teclado numerico tactil. Extraido del viewport 3D (la referencia)
//  para que el UV editor y el Editor 2D se porten IGUAL sin copiar bloques. Ver TransformUI.h.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"     // T(): los textos salen en el idioma del sistema
#include "ViewPorts/TransformUI.h"
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"   // roles TBR_Aceptar/TBR_Cancelar/TBR_EjeX/Y (los MISMOS del 3D)
#include "ViewPorts/NumInput.h"     // buffer/valor de la entrada numerica (compartida)
#include "ViewPorts/PopUp/NumPad.h" // NumPadAbrirTransform (tap en la barra de info)
#include "WhiskUI/text/bitmapText.h"   // RenderBitmapText (texto de la barra de info)
#include "WhiskUI/widgets/card.h"      // Card (fondo de la barra)
#include "render/OpcionesRender.h"     // g_redraw
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
//  "[expr|] = valor" de la ENTRADA NUMERICA (estilo Blender "Move: [(2*3)+3] = 9"). El caret
//  se dibuja en el medio (editable con las flechas del teclado tactil). Era parte del
//  W3dTextoTransform del 3D; ahora los 3 editores arman su texto con esto.
// ---------------------------------------------------------------------------------------------
std::string TransformUITextoNum(const char* unidad){
    float v = 0.0f; bool valido = NumInputValor(v);
    std::string buf = NumInputBuffer();
    int car = NumInputCaret(); if (car < 0) car = 0; if (car > (int)buf.size()) car = (int)buf.size();
    buf.insert(buf.begin() + car, '|'); // caret visible
    std::string expr = (NumInputNegado() ? "-[" : "[") + buf + "]";
    std::string val = valido ? (W3dFmtFloat(v, 4) + unidad) : "?";
    return expr + " = " + val;
}

// texto completo de la barra de info de un transform 2D (UV / Editor 2D). Mismo formato que
// el 3D: "Translate X: .. along X" / "Rotate  ..°" / "Scale: ..", y con entrada numerica
// "Move: [1/4|] = 0.2500  along X".
std::string TransformUITexto2D(int modo, float d1, float d2, float ang, float fac,
                               int ejeLock, const char* ejeA, const char* ejeB,
                               const char* unidad){
    if (NumInputActivo()){
        const char* op = (modo == 2) ? T("Rotate") : (modo == 3) ? T("Scale") : T("Move");
        const char* unit = (modo == 2) ? "\xC2\xB0" : unidad;
        std::string suf;
        if (modo != 2 && ejeLock == 1) suf = std::string("  along ") + ejeA;
        if (modo != 2 && ejeLock == 2) suf = std::string("  along ") + ejeB;
        return std::string(op) + ": " + TransformUITextoNum(unit) + suf;
    }
    if (modo == 2) return std::string("Rotate  ") + W3dFmtFloat(ang, 1) + "\xC2\xB0";
    if (modo == 3){
        std::string s = std::string(T("Scale")) + ": " + W3dFmtFloat(fac, 4);
        if (ejeLock == 1) s += std::string("  along ") + ejeA;
        if (ejeLock == 2) s += std::string("  along ") + ejeB;
        return s;
    }
    // mover: un eje bloqueado muestra SOLO ese eje (como el 3D); libre muestra los dos
    if (ejeLock == 1) return std::string("Translate ") + ejeA + ": " + W3dFmtFloat(d1, 4) + unidad +
                             "  along " + ejeA;
    if (ejeLock == 2) return std::string("Translate ") + ejeB + ": " + W3dFmtFloat(d2, 4) + unidad +
                             "  along " + ejeB;
    return std::string("Translate  ") + ejeA + ": " + W3dFmtFloat(d1, 4) + unidad +
           "  " + ejeB + ": " + W3dFmtFloat(d2, 4) + unidad;
}

// ---------------------------------------------------------------------------------------------
//  BARRA DE INFO durante un transform: la barra de menu se reemplaza por el fondo + el texto
//  en color de acento. Extraido del RenderBarraTransform del 3D (que ahora la llama; el UV y
//  el 2D tambien). Se dibuja en el ortho LOCAL del viewport, como RenderBar.
// ---------------------------------------------------------------------------------------------
void ViewportBase::RenderBarraInfo(const std::string& texto){
    if (!barCard) return;
    int barH = BarHeight();
    int yBar = barAbajo ? height - barH : 0;

    // fondo de la barra (mas opaco que la normal para que el texto se lea)
    gfx::PushMatrix();
    gfx::Translatef(0, (GLfloat)yBar, 0);
    SetColorID(ColorID::gris, 0.85f);
    barCard->Resize(width, barH);
    barCard->RenderObject(false);
    gfx::PopMatrix();

    // texto de estado en color de acento (modo activo), centrado vertical
    SetColorID(ColorID::accent);
    gfx::PushMatrix();
    int ty = yBar + (barH - LetterHeightGS) / 2;
    gfx::Translatef((GLfloat)(gapGS * 2), (GLfloat)ty, 0);
    RenderBitmapText(texto, textAlign::left, width - gapGS * 2);
    gfx::PopMatrix();
}

// tap/click sobre la franja de la barra de info durante un transform del viewport -> teclado
// numerico en modo transform (edita el valor exacto, como tipear en PC). Mismo criterio que
// el ClickBarraTransform del 3D, pero via los hooks compartidos (UV / Editor 2D).
bool TransformUIClickBarra(ViewportBase* vp, int mx, int my){
    if (!vp || !vp->XformEnCurso()) return false;
    int barH = vp->BarHeight();
    int yBar = vp->barAbajo ? (vp->y + vp->height - barH) : vp->y;
    if (mx < vp->x || mx >= vp->x + vp->width || my < yBar || my >= yBar + barH) return false;
    NumPadAbrirTransform();
    return true;
}

// una expresion tipeada INVALIDA (no vacia y que no parsea, ej "1/") no deja confirmar:
// la barra ya muestra "= ?" como feedback. Sin entrada activa siempre se puede.
bool TransformUIPuedeConfirmar(){
    if (!NumInputActivo()) return true;
    float v; return NumInputValor(v);
}

// ---------------------------------------------------------------------------------------------
//  TOOLBAR durante el transform: tilde/cruz/X/Y con los MISMOS roles y colores del 3D.
// ---------------------------------------------------------------------------------------------
#ifndef W3D_SYMBIAN
extern Uint32 g_lastFingerTicks; // controles.cpp: hubo input tactil en la sesion
bool ToolbarUsaTactil(){ return g_lastFingerTicks != 0; }
#else
bool ToolbarUsaTactil(){ return false; } // (cuando el N8 tenga touch, cablear aca)
#endif

// colores de los ejes (X rojo / Y verde / Z azul) + rojos del cancelar; el fondo "iluminado"
// es el mismo color atenuado. Eran statics de ViewPort3D_Toolbar.cpp; ahora los usan los 3.
static const float kTbEje[3][3] = { {0.90f,0.25f,0.25f}, {0.30f,0.85f,0.30f}, {0.35f,0.55f,1.00f} };
static float sTbEjeBg[3][3];
static const float kTbRojo[3]   = { 0.92f, 0.28f, 0.24f };
static const float kTbRojoBg[3] = { 0.41f, 0.13f, 0.11f };
static float sTbVerdeBg[3];

const float* TbEjeColor(int e){ if (e < 0 || e > 2) e = 0; return kTbEje[e]; }
const float* TbEjeBg(int e){
    if (e < 0 || e > 2) e = 0;
    for (int i = 0; i < 3; i++) sTbEjeBg[e][i] = kTbEje[e][i] * 0.45f;
    return sTbEjeBg[e];
}
const float* TbRojo(){ return kTbRojo; }
const float* TbRojoBg(){ return kTbRojoBg; }
const float* TbVerdeBg(){
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    for (int i = 0; i < 3; i++) sTbVerdeBg[i] = accent[i] * 0.40f;
    return sTbVerdeBg;
}

// crea tilde / cruz / X / Y (ocultos) al final de B. Mismos iconos y roles que el 3D.
void ToolbarCrearTransform2D(std::vector<Button*>& B){
    Button* b;
    b = new Button("", (int)IconType::notifOk);    b->rol = TBR_Aceptar;  b->centrado = true; b->visible = false; B.push_back(b);
    b = new Button("", (int)IconType::notifError); b->rol = TBR_Cancelar; b->centrado = true; b->visible = false; B.push_back(b);
    b = new Button("X"); b->rol = TBR_EjeX; b->centrado = true; b->cuadrado = true; b->visible = false; B.push_back(b);
    b = new Button("Y"); b->rol = TBR_EjeY; b->centrado = true; b->cuadrado = true; b->visible = false; B.push_back(b);
}

// visibilidad + colores durante un G/R/S propio de un editor 2D. Mismas reglas que el 3D:
// los EJES se ven durante el transform; tilde/cruz se muestran SIEMPRE durante el transform
// (mouse incluido; antes solo en sesion tactil y el dueño no los veia en PC -> ahora son
// descubribles ademas de Enter/Esc/click). ejeLock = -1 oculta los ejes (transform sin
// lock, ej: huesos 2D).
void ToolbarSincronizarTransform2D(std::vector<Button*>& B, bool transformando, int ejeLock){
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    Button* b;
    if ((b = BarRolBtn(B, TBR_Aceptar))){
        b->visible = transformando;
        b->tinte = TbVerdeBg(); b->colorTexto = accent;
    }
    if ((b = BarRolBtn(B, TBR_Cancelar))){
        b->visible = transformando;
        b->tinte = TbRojoBg(); b->colorTexto = TbRojo();
    }
    for (int e = 0; e < 2; e++){ // 0 = X, 1 = Y (en 2D no hay Z)
        if (!(b = BarRolBtn(B, TBR_EjeX + e))) continue;
        b->visible = transformando && ejeLock >= 0;
        bool on = (ejeLock == e + 1);
        b->tinte = on ? TbEjeBg(e) : NULL;              // encendido: fondo de SU color
        b->colorTexto = on ? blanco : TbEjeColor(e);    // apagado: la letra en su color
    }
}

// despacho de los roles de transform via los hooks del viewport (teclado y toolbar
// comparten el MISMO camino: XformToggleEje es el que usan las teclas X/Y).
bool ToolbarAccionTransform2D(ViewportBase* vp, int rol){
    if (!vp) return false;
    if (rol == TBR_Aceptar) { vp->XformConfirmar(); return true; }
    if (rol == TBR_Cancelar){ vp->XformCancelar();  return true; }
    if (rol == TBR_EjeX)    { vp->XformToggleEje(1); return true; }
    if (rol == TBR_EjeY)    { vp->XformToggleEje(2); return true; }
    return false;
}
