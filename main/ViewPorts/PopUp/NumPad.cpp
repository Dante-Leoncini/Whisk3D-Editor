#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "NumPad.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "WhiskUI/PopupMenu.h"              // MenuPantallaW / MenuPantallaH
#include "WhiskUI/Propieties/PropFloat.h"   // g_propFloatEditando + NumEditCommit/Cancel
#include "WhiskUI/TextField.h"              // TextFieldInputChar (mismo camino que el teclado fisico)
#include "WhiskUI/bitmapText.h"
#include <cstdio>   // snprintf (resultado -> texto)
#include <cstdlib>  // strtod (evaluador)

extern bool g_redraw;
extern void NumEditSalirDelPanel(); // Properties.cpp: limpia el 'editando' del panel al terminar

// ---------------------------------------------------------------------------
//  Evaluador de EXPRESIONES ( + - * / y parentesis, precedencia normal ) para
//  el Aceptar: "2.5*(1+3)" -> 10. Si la expresion no parsea completa se deja
//  el texto como esta (el commit hace atof igual que siempre).
// ---------------------------------------------------------------------------
static const char* gEp = NULL;
static bool gEvalErr = false;
static double EvalExpr();
static double EvalAtomo(){
    while (*gEp == ' ') gEp++;
    if (*gEp == '('){
        gEp++;
        double v = EvalExpr();
        while (*gEp == ' ') gEp++;
        if (*gEp == ')') gEp++; else gEvalErr = true;
        return v;
    }
    if (*gEp == '-'){ gEp++; return -EvalAtomo(); } // signo
    if (*gEp == '+'){ gEp++; return  EvalAtomo(); }
    char* fin = NULL;
    double v = strtod(gEp, &fin);
    if (fin == gEp){ gEvalErr = true; return 0.0; } // no habia numero
    gEp = fin;
    return v;
}
static double EvalTermino(){
    double v = EvalAtomo();
    for (;;){
        while (*gEp == ' ') gEp++;
        if (*gEp == '*'){ gEp++; v *= EvalAtomo(); }
        else if (*gEp == '/'){ gEp++; double d = EvalAtomo(); v = (d != 0.0) ? v / d : 0.0; } // div/0 -> 0
        else return v;
    }
}
static double EvalExpr(){
    double v = EvalTermino();
    for (;;){
        while (*gEp == ' ') gEp++;
        if (*gEp == '+'){ gEp++; v += EvalTermino(); }
        else if (*gEp == '-'){ gEp++; v -= EvalTermino(); }
        else return v;
    }
}
static bool EvaluarExpresion(const std::string& s, double* out){
    gEp = s.c_str(); gEvalErr = false;
    double v = EvalExpr();
    while (*gEp == ' ') gEp++;
    if (gEvalErr || *gEp != '\0') return false; // sobro basura o fallo el parseo
    *out = v;
    return true;
}

// ---------------------------------------------------------------------------
//  Teclado: 4 filas x 5 columnas
// ---------------------------------------------------------------------------
static const char* kTeclas[4][5] = {
    { "7", "8", "9", "(", ")"  },
    { "4", "5", "6", "*", "/"  },
    { "1", "2", "3", "+", "-"  },
    { ".", "0", "<-", "X", "OK" },
};

static NumPad* gNumPad = NULL;

NumPad::NumPad() : PopUpBase("NumPad") {
    keyCard = new Card(NULL, 10, 10);
    keyW = keyH = dispH = 0;
    Reubicar();
}

void NumPad::Reubicar(){
    // ancho COMPLETO de la ventana, pegado al borde de abajo. Teclas altas (dedos).
    dispH = RenglonHeightGS + gapGS * 2;         // fila del display (label + valor)
    keyH  = RenglonHeightGS * 2;                 // alto de tecla
    int w = MenuPantallaW;
    int h = borderGS * 2 + dispH + (keyH + gapGS) * 4;
    keyW = (w - borderGS * 2 - gapGS * 4) / 5;   // 5 columnas con gap entre medio
    x = 0;
    y = MenuPantallaH - h;
    popUpWindow->Resize(w, h);
}

void NumPad::Render(){
    // la edicion pudo terminar por OTRO camino (Enter fisico, click que commiteo): el teclado se va solo
    if (!g_propFloatEditando){ Cerrar(); return; }
    Reubicar(); // por si roto la pantalla / cambio el tamano de la ventana

    const float* gris   = ListaColores[static_cast<int>(ColorID::gris)];
    const float* header = ListaColores[static_cast<int>(ColorID::headerColor)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
    const float rojo[3] = { 0.92f, 0.28f, 0.24f }; // mismo rojo que las notificaciones de error

    initView();

    // fondo + borde accent (popup activo)
    w3dEngine::Color4f(gris[0], gris[1], gris[2], 0.98f);
    popUpWindow->RenderObject(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // ---- display: label del campo a la IZQ + valor (con caret) a la DER ----
    {
        int colLabel = popUpWindow->width / 2 - borderGS - gapGS * 2;
        int ty = borderGS + gapGS;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)ty, 0);
        w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
        RenderBitmapText(g_propFloatEditando->name, textAlign::left, colLabel);
        w3dEngine::PopMatrix();

        TextField& f = g_propFloatEditando->field;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(popUpWindow->width / 2), (GLfloat)ty, 0);
        if (f.selectAll){
            // TODO seleccionado (recien abierto): valor en accent, tipear lo reemplaza
            w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
            RenderBitmapText(f.text, textAlign::right, colLabel);
        } else {
            std::string shown = f.text.substr(0, f.caret) + "|" + f.text.substr(f.caret);
            w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            RenderBitmapText(shown, textAlign::right, colLabel);
        }
        w3dEngine::PopMatrix();
    }

    // ---- teclas ----
    int textoY = (keyH - RenglonHeightGS) / 2; // texto centrado vertical en la tecla
    for (int fila = 0; fila < 4; fila++){
        for (int col = 0; col < 5; col++){
            const char* t = kTeclas[fila][col];
            int kx = borderGS + col * (keyW + gapGS);
            int ky = borderGS + dispH + fila * (keyH + gapGS);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)kx, (GLfloat)ky, 0);
            // fondo: OK verde / X rojo / resto gris de header
            if      (t[0] == 'O') w3dEngine::Color4f(accent[0] * 0.55f, accent[1] * 0.55f, accent[2] * 0.55f, 1.0f);
            else if (t[0] == 'X') w3dEngine::Color4f(rojo[0] * 0.55f, rojo[1] * 0.55f, rojo[2] * 0.55f, 1.0f);
            else                  w3dEngine::Color4f(header[0], header[1], header[2], 1.0f);
            keyCard->Resize(keyW, keyH);
            keyCard->RenderObject(false);
            // etiqueta
            if      (t[0] == 'O') w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
            else if (t[0] == 'X') w3dEngine::Color4f(rojo[0], rojo[1], rojo[2], 1.0f);
            else                  w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            w3dEngine::Translatef(0, (GLfloat)textoY, 0);
            RenderBitmapText(t, textAlign::center, keyW);
            w3dEngine::PopMatrix();
        }
    }

    endView();
}

void NumPad::AccionTecla(int fila, int col){
    const char* t = kTeclas[fila][col];
    if (t[0] == 'O')      { Aceptar(); return; }                 // OK
    if (t[0] == 'X')      { Cancelar(); return; }                // Cancelar
    if (t[0] == '<')      { TextFieldInputChar(8); }             // retroceso
    else                  { TextFieldInputChar(t[0]); }          // digito / . / operador
    g_redraw = true;
}

bool NumPad::Click(int mx, int my){
    if (!Contains(mx, my)) return false; // afuera: LayoutClickUI nos cierra (Cerrar commitea)
    int lx = mx - x;
    int ly = my - y - borderGS - dispH;  // relativo a la zona de teclas
    if (ly < 0) return true;             // el display: consumir sin accion
    int fila = ly / (keyH + gapGS); if (fila > 3) fila = 3;
    int col  = (lx - borderGS) / (keyW + gapGS);
    if (col < 0) col = 0; if (col > 4) col = 4;
    AccionTecla(fila, col);
    return true;
}

bool NumPad::Tecla(int tecla){
    if (tecla == LayoutKey::Enter || tecla == LayoutKey::Accept) { Aceptar(); return true; }
    if (tecla == LayoutKey::Cancel) { Cancelar(); return true; }
    return true; // el resto (flechas) no navega nada aca
}

void NumPad::Aceptar(){
    if (g_propFloatEditando){
        double v = 0.0;
        if (EvaluarExpresion(g_propFloatEditando->field.text, &v)){
            char buf[32];
            snprintf(buf, sizeof(buf), "%g", v);
            g_propFloatEditando->field.SetText(buf); // el commit parsea este resultado
        }
        NumEditCommit();
        NumEditSalirDelPanel();
    }
    PopUpBase::Cerrar();
    g_redraw = true;
}

void NumPad::Cancelar(){
    NumEditCancel();
    NumEditSalirDelPanel();
    PopUpBase::Cerrar();
    g_redraw = true;
}

void NumPad::Cerrar(){
    // cerrado desde AFUERA (tap fuera del popup): commitea lo tipeado, igual que la edicion inline
    if (g_propFloatEditando){ NumEditCommit(); NumEditSalirDelPanel(); }
    PopUpBase::Cerrar();
    g_redraw = true;
}

void NumPadAbrir(){
    if (gNumPad){ delete gNumPad; gNumPad = NULL; } // reemplaza la instancia anterior
    gNumPad = new NumPad();
    PopUpActive = gNumPad;
    g_redraw = true;
}
