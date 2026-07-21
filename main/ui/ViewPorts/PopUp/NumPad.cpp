#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render/OpcionesRender.h"   // RenderType / g_redraw: son del editor
#include "NumPad.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey + NumInput* (modo transform)
#include "ViewPorts/Timeline.h"             // DopeNumInputChar (transform de keyframes del dope sheet)
#include "WhiskUI/widgets/PopupMenu.h"              // MenuPantallaW / MenuPantallaH
#include "WhiskUI/Propieties/PropFloat.h"   // g_propFloatEditando + NumEditCommit/Cancel
#include "WhiskUI/widgets/TextField.h"              // TextFieldInputChar (mismo camino que el teclado fisico)
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/text/font.h"                   // WhiskFont->getMeshTri (barra de espacio: guion estirado)
#include <cstdio>   // snprintf (resultado -> texto)
#include <cstdlib>  // strtod (evaluador)
#include <cstring>  // strcmp (tokens de tecla: "OK" "DEL" "<" ">")

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
//  Teclado: 4 filas x 6 columnas. La ultima columna es navegacion/borrado
//  (DEL, ← , →) y "X"/"OK" abajo a la derecha, separados de los numeros por un
//  hueco para no pulsarlos por error. "<" = flecha izquierda, ">" = derecha.
// ---------------------------------------------------------------------------
#define NP_COLS 6
#define NP_FILAS 4
static const char* kTeclas[NP_FILAS][NP_COLS] = {
    { "7", "8", "9", "(", ")", "DEL" },
    { "4", "5", "6", "*", "/", "<"   },
    { "1", "2", "3", "+", "-", ">"   },
    { ".", "0", "",  "",  "X", "OK"  },
};

static NumPad* gNumPad = NULL;

NumPad::NumPad(bool transform) : PopUpBase("NumPad") {
    modoTransform = transform;
    prevPopup = NULL;
    keyCard = new Card(NULL, 10, 10);
    keyW = keyH = dispH = 0;
    Reubicar();
}

// cierra el numpad devolviendo el foco al popup que estaba antes (ej: el panel redo del loop cut),
// en vez de dejar PopUpActive en NULL. Sin esto, editar un campo del panel lo hacia desaparecer.
void NumPad::CerrarRestaurando(){
    if (PopUpActive == this) PopUpActive = prevPopup;
    if (prevPopup) g_redraw = true;
}

void NumPad::Reubicar(){
    // ancho COMPLETO de la ventana, pegado al borde de abajo. Teclas altas (dedos).
    // En modo transform NO hay fila de display (se edita la barra de estado de arriba).
    dispH = modoTransform ? 0 : (RenglonHeightGS + gapGS * 2);
    keyH  = RenglonHeightGS * 2;                 // alto de tecla
    int w = MenuPantallaW;
    int h = borderGS * 2 + dispH + (keyH + gapGS) * NP_FILAS;
    keyW = (w - borderGS * 2 - gapGS * (NP_COLS - 1)) / NP_COLS; // columnas con gap entre medio
    x = 0;
    y = MenuPantallaH - h;
    popUpWindow->Resize(w, h);
}

void NumPad::Render(){
    // la edicion pudo terminar por OTRO camino: el teclado se va solo.
    if (modoTransform){
        if (!NumInputTransformEnCurso()){ CerrarRestaurando(); g_redraw = true; return; }
    } else {
        if (!g_propFloatEditando){ Cerrar(); return; } // Enter fisico / click que commiteo
    }
    Reubicar(); // por si roto la pantalla / cambio el tamano de la ventana

    const float* gris   = ListaColores[static_cast<int>(ColorID::gris)];
    const float* header = ListaColores[static_cast<int>(ColorID::headerColor)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
    const float* negro  = ListaColores[static_cast<int>(ColorID::negro)];
    const float rojo[3] = { 0.92f, 0.28f, 0.24f }; // mismo rojo que las notificaciones de error

    initView();

    // fondo + borde accent (popup activo)
    w3dEngine::Color4f(gris[0], gris[1], gris[2], 0.98f);
    popUpWindow->RenderObject(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // ---- display: label del campo a la IZQ + valor (con caret) a la DER ----
    // Solo en modo float. El valor se ancla al borde DERECHO (no al centro) para
    // que un valor largo se extienda hacia la izquierda SIN pisar el label.
    if (!modoTransform && g_propFloatEditando){
        int colLabel = popUpWindow->width / 2 - borderGS - gapGS * 2;
        int ty = borderGS + gapGS;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)ty, 0);
        w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
        RenderBitmapText(g_propFloatEditando->name, textAlign::left, colLabel);
        w3dEngine::PopMatrix();

        // ---- CAJA de input (fondo oscuro + borde claro, como en Properties) con el valor a la DERECHA ----
        TextField& f = g_propFloatEditando->field;
        int boxX = popUpWindow->width / 2;
        int boxW = popUpWindow->width - borderGS - gapGS - boxX;
        int boxH = RenglonHeightGS + gapGS;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)boxX, (GLfloat)(borderGS + gapGS / 2), 0);
        w3dEngine::Color4f(negro[0], negro[1], negro[2], 1.0f);
        keyCard->Resize(boxW, boxH); keyCard->RenderObject(false);
        w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
        keyCard->RenderBorder(false);
        w3dEngine::Translatef((GLfloat)(boxW - gapGS), (GLfloat)((boxH - RenglonHeightGS) / 2), 0);
        if (f.selectAll){
            // TODO seleccionado (recien abierto): valor en accent, tipear lo reemplaza
            w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
            RenderBitmapText(f.text, textAlign::right, boxW - gapGS * 2);
        } else {
            std::string shown = f.text.substr(0, f.caret) + "|" + f.text.substr(f.caret);
            w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            RenderBitmapText(shown, textAlign::right, boxW - gapGS * 2);
        }
        w3dEngine::PopMatrix();
    }

    // ---- teclas ----
    int textoY = (keyH - RenglonHeightGS) / 2; // texto centrado vertical en la tecla
    for (int fila = 0; fila < NP_FILAS; fila++){
        for (int col = 0; col < NP_COLS; col++){
            const char* t = kTeclas[fila][col];
            if (t[0] == '\0') continue; // hueco: sin tecla
            int kx = borderGS + col * (keyW + gapGS);
            int ky = borderGS + dispH + fila * (keyH + gapGS);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)kx, (GLfloat)ky, 0);
            // fondo: OK verde / X rojo / resto gris de header
            if      (!strcmp(t, "OK")) w3dEngine::Color4f(accent[0] * 0.55f, accent[1] * 0.55f, accent[2] * 0.55f, 1.0f);
            else if (!strcmp(t, "X"))  w3dEngine::Color4f(rojo[0] * 0.55f, rojo[1] * 0.55f, rojo[2] * 0.55f, 1.0f);
            else                       w3dEngine::Color4f(header[0], header[1], header[2], 1.0f);
            keyCard->Resize(keyW, keyH);
            keyCard->RenderObject(false);
            // etiqueta
            if      (!strcmp(t, "OK")) w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
            else if (!strcmp(t, "X"))  w3dEngine::Color4f(rojo[0], rojo[1], rojo[2], 1.0f);
            else                       w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            w3dEngine::Translatef(0, (GLfloat)textoY, 0);
            RenderBitmapText(t, textAlign::center, keyW);
            w3dEngine::PopMatrix();
        }
    }

    endView();
}

// alimenta un caracter (o 8=backspace) al destino segun el modo
void NumPad::Feed(int c){
    if (modoTransform){ if (!NumInputChar(c)) DopeNumInputChar(c); } // 3D; si no, el transform de keyframes
    else               TextFieldInputChar(c);
}

void NumPad::AccionTecla(int fila, int col){
    const char* t = kTeclas[fila][col];
    if (t[0] == '\0')       return;                              // hueco
    if (!strcmp(t, "OK"))   { Aceptar();  return; }              // OK
    if (!strcmp(t, "X"))    { Cancelar(); return; }              // Cancelar
    if (!strcmp(t, "DEL"))  { Feed(8); }                         // retroceso
    else if (!strcmp(t, "<")){                                   // flecha izquierda
        if (modoTransform) NumInputLeft();
        else if (g_propFloatEditando) g_propFloatEditando->field.CaretIzq();
    }
    else if (!strcmp(t, ">")){                                   // flecha derecha
        if (modoTransform) NumInputRight();
        else if (g_propFloatEditando) g_propFloatEditando->field.CaretDer();
    }
    else                    { Feed(t[0]); }                      // digito / . / operador
    g_redraw = true;
}

bool NumPad::Click(int mx, int my){
    if (!Contains(mx, my)) return false; // afuera: LayoutClickUI nos cierra (Cerrar commitea)
    int lx = mx - x;
    int ly = my - y - borderGS - dispH;  // relativo a la zona de teclas
    if (ly < 0) return true;             // el display: consumir sin accion
    int fila = ly / (keyH + gapGS); if (fila > NP_FILAS - 1) fila = NP_FILAS - 1;
    int col  = (lx - borderGS) / (keyW + gapGS);
    if (col < 0) col = 0; if (col > NP_COLS - 1) col = NP_COLS - 1;
    AccionTecla(fila, col);
    return true;
}

bool NumPad::Tecla(int tecla){
    if (tecla == LayoutKey::Enter || tecla == LayoutKey::Accept) { Aceptar(); return true; }
    if (tecla == LayoutKey::Cancel) { Cancelar(); return true; }
    return true; // el resto (flechas) no navega nada aca
}

void NumPad::Aceptar(){
    if (modoTransform){ NumInputConfirmar(); CerrarRestaurando(); g_redraw = true; return; }
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
    CerrarRestaurando();
    g_redraw = true;
}

void NumPad::Cancelar(){
    if (modoTransform){ NumInputCancelar(); CerrarRestaurando(); g_redraw = true; return; }
    NumEditCancel();
    NumEditSalirDelPanel();
    CerrarRestaurando();
    g_redraw = true;
}

void NumPad::Cerrar(){
    // cerrado desde AFUERA (tap fuera del popup).
    if (modoTransform){ CerrarRestaurando(); g_redraw = true; return; } // deja el transform como quedo (valores ya aplicados en vivo)
    // modo float: commitea lo tipeado, igual que la edicion inline
    if (g_propFloatEditando){ NumEditCommit(); NumEditSalirDelPanel(); }
    CerrarRestaurando();
    g_redraw = true;
}

void NumPadAbrir(){
    PopUpBase* prev = PopUpActive; // si se abre SOBRE un popup (ej: panel redo del loop cut), se restaura al cerrar
    if (prev == gNumPad) prev = NULL; // no restaurarse a si mismo (y evita puntero colgante al borrar gNumPad)
    if (gNumPad){ delete gNumPad; gNumPad = NULL; } // reemplaza la instancia anterior
    gNumPad = new NumPad(false);
    gNumPad->prevPopup = prev;
    PopUpActive = gNumPad;
    g_redraw = true;
}

void NumPadAbrirTransform(){
    if (gNumPad){ delete gNumPad; gNumPad = NULL; }
    gNumPad = new NumPad(true);
    PopUpActive = gNumPad;
    NumInputBegin(); // muestra "[|]" en la barra de estado apenas se abre
    g_redraw = true;
}

// ===========================================================================
//  TECLADO QWERTY (español, con ñ). Teclas de ANCHO PONDERADO (peso relativo);
//  2 modos: LETRAS y SIMBOLOS ("123"/"ABC"). "Aa" = mayusculas, "___" = barra de
//  espacio (un guion bajo ESTIRADO), "DEL" retroceso, "<"/">" caret, "OK" acepta.
// ===========================================================================
extern bool RenameActivo();   // controles.cpp: hay un rename en curso?
extern void RenameCommit();   // aceptar el rename
extern void RenameCancel();   // descartar el rename

struct QKey { const char* t; float w; };      // etiqueta + peso de ancho (fila = suma de pesos)
#define QP_FILAS 4
// terminador de fila: {NULL, 0}
static const QKey kFilasLetra[QP_FILAS][12] = {
    {{"q",1},{"w",1},{"e",1},{"r",1},{"t",1},{"y",1},{"u",1},{"i",1},{"o",1},{"p",1},{NULL,0}},
    {{"a",1},{"s",1},{"d",1},{"f",1},{"g",1},{"h",1},{"j",1},{"k",1},{"l",1},{"\xC3\xB1",1},{NULL,0}}, // ñ
    {{"Aa",1.5f},{"z",1},{"x",1},{"c",1},{"v",1},{"b",1},{"n",1},{"m",1},{".",1},{"DEL",1.5f},{NULL,0}},
    {{"123",1.6f},{"___",5.4f},{"<",1},{">",1},{"OK",2.0f},{NULL,0}},
};
static const QKey kFilasSimbolo[QP_FILAS][12] = {
    {{"1",1},{"2",1},{"3",1},{"4",1},{"5",1},{"6",1},{"7",1},{"8",1},{"9",1},{"0",1},{NULL,0}},
    {{"-",1},{"_",1},{"/",1},{":",1},{"(",1},{")",1},{"$",1},{"&",1},{"@",1},{"\"",1},{NULL,0}},
    {{".",1},{",",1},{"?",1},{"!",1},{"'",1},{"#",1},{"%",1},{"=",1},{"+",1},{"DEL",1.5f},{NULL,0}},
    {{"ABC",1.6f},{"___",5.4f},{"<",1},{">",1},{"OK",2.0f},{NULL,0}},
};

static QwertyPad* gQwerty = NULL;
static bool gQwertySimbolos = false; // modo actual (persiste entre aperturas)

// geometria de una fila: cuenta teclas + suma de pesos, y el ancho util (con gaps)
static const QKey* QpFila(bool simbolos, int fila){ return simbolos ? kFilasSimbolo[fila] : kFilasLetra[fila]; }

QwertyPad::QwertyPad() : PopUpBase("Qwerty") {
    prevPopup = NULL;
    keyCard = new Card(NULL, 10, 10);
    keyH = dispH = 0; caps = false;
    Reubicar();
}

void QwertyPad::Reubicar(){
    dispH = RenglonHeightGS + gapGS * 3;          // caja de input (texto que se edita)
    keyH  = RenglonHeightGS * 2;                   // alto de tecla (dedos); 4 filas
    int w = MenuPantallaW;
    int h = borderGS * 2 + dispH + (keyH + gapGS) * QP_FILAS;
    x = 0;
    y = MenuPantallaH - h;
    popUpWindow->Resize(w, h);
}

void QwertyPad::FeedStr(const char* s){
    if (!g_textFieldActivo) return;
    for (const char* p = s; *p; p++) g_textFieldActivo->InsertChar((unsigned char)*p); // byte a byte (ñ = 2 bytes)
    g_redraw = true;
}

// geometria de una fila: llena kx[]/kw[] (ancho PONDERADO por peso) y devuelve la cantidad de teclas.
static int QpRowGeom(bool simbolos, int fila, int fullW, int kx[12], int kw[12]){
    const QKey* row = QpFila(simbolos, fila);
    int n = 0; float totalW = 0;
    for (; row[n].t; n++) totalW += row[n].w;
    if (n == 0 || totalW <= 0) return 0;
    int avail = fullW - borderGS * 2 - gapGS * (n - 1);
    int cx = borderGS;
    for (int i = 0; i < n; i++){
        int w = (int)(avail * row[i].w / totalW + 0.5f);
        kx[i] = cx; kw[i] = w; cx += w + gapGS;
    }
    return n;
}

void QwertyPad::Render(){
    if (!g_textFieldActivo){ Cerrar(); return; } // la edicion termino por otro camino -> el teclado se va solo
    Reubicar();

    const float* gris   = ListaColores[static_cast<int>(ColorID::gris)];
    const float* header = ListaColores[static_cast<int>(ColorID::headerColor)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* negro  = ListaColores[static_cast<int>(ColorID::negro)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];

    initView();
    w3dEngine::Color4f(gris[0], gris[1], gris[2], 0.98f);
    popUpWindow->RenderObject(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // ---- caja de INPUT (fondo oscuro + borde claro, como en Properties) con el texto + caret ----
    {
        int boxH = RenglonHeightGS + gapGS;
        int boxW = popUpWindow->width - 2 * (borderGS + gapGS);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(borderGS + gapGS), 0);
        w3dEngine::Color4f(negro[0], negro[1], negro[2], 1.0f);
        keyCard->Resize(boxW, boxH); keyCard->RenderObject(false);
        w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
        keyCard->RenderBorder(false);
        TextField& f = *g_textFieldActivo;
        std::string shown = f.selectAll ? f.text : (f.text.substr(0, f.caret) + "|" + f.text.substr(f.caret));
        w3dEngine::Translatef((GLfloat)gapGS, (GLfloat)((boxH - RenglonHeightGS) / 2), 0);
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        RenderBitmapText(shown, textAlign::left, boxW - gapGS * 2);
        w3dEngine::PopMatrix();
    }

    int textoY = (keyH - RenglonHeightGS) / 2;
    int kx[12], kw[12];
    for (int fila = 0; fila < QP_FILAS; fila++){
        const QKey* row = QpFila(gQwertySimbolos, fila);
        int n = QpRowGeom(gQwertySimbolos, fila, popUpWindow->width, kx, kw);
        int ky = borderGS + dispH + fila * (keyH + gapGS);
        for (int i = 0; i < n; i++){
            const char* t = row[i].t;
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)kx[i], (GLfloat)ky, 0);
            // fondo: OK / Aa(caps) en accent; resto gris de header
            if      (!strcmp(t, "OK")) w3dEngine::Color4f(accent[0]*0.55f, accent[1]*0.55f, accent[2]*0.55f, 1.0f);
            else if (!strcmp(t, "Aa") && caps) w3dEngine::Color4f(accent[0]*0.55f, accent[1]*0.55f, accent[2]*0.55f, 1.0f);
            else                       w3dEngine::Color4f(header[0], header[1], header[2], 1.0f);
            keyCard->Resize(kw[i], keyH); keyCard->RenderObject(false);
            // etiqueta
            if (!strcmp(t, "OK")) w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
            else                  w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            if (!strcmp(t, "___")){
                // BARRA DE ESPACIO: un guion bajo ESTIRADO a lo ancho de la tecla (el glifo mide getMeshTri()[2])
                const GLfloat* mt = WhiskFont ? WhiskFont->getMeshTri() : NULL;
                float gw = (mt && mt[2] > 1.0f) ? mt[2] : 5.0f;
                float sc = (float)(kw[i] - gapGS * 2) / gw;
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)gapGS, (GLfloat)textoY, 0);
                w3dEngine::Scalef(sc, 1.0f, 1.0f);
                RenderBitmapText("_", textAlign::left, 1000000); // sin recorte
                w3dEngine::PopMatrix();
            } else {
                const char* lbl = t; char up[3] = {0,0,0};
                if (caps){
                    if (t[1]=='\0' && t[0]>='a' && t[0]<='z'){ up[0]=(char)(t[0]-32); lbl=up; }
                    else if (!strcmp(t, "\xC3\xB1")){ up[0]='\xC3'; up[1]='\x91'; lbl=up; } // Ñ
                }
                w3dEngine::Translatef(0, (GLfloat)textoY, 0);
                RenderBitmapText(lbl, textAlign::center, kw[i]);
            }
            w3dEngine::PopMatrix();
        }
    }
    endView();
}

void QwertyPad::AccionTecla(int fila, int col){
    const QKey* row = QpFila(gQwertySimbolos, fila);
    int n = 0; for (; row[n].t; n++) {}
    if (col < 0 || col >= n) return;
    const char* t = row[col].t;
    if (!strcmp(t, "OK")) { Aceptar(); return; }
    if (!strcmp(t, "123") || !strcmp(t, "ABC")) { gQwertySimbolos = !gQwertySimbolos; g_redraw = true; return; }
    if (!strcmp(t, "Aa")) { caps = !caps; g_redraw = true; return; }
    if (!strcmp(t, "DEL")){ if (g_textFieldActivo){ g_textFieldActivo->Backspace(); g_redraw = true; } return; }
    if (!strcmp(t, "<"))  { if (g_textFieldActivo){ g_textFieldActivo->CaretIzq(); g_redraw = true; } return; }
    if (!strcmp(t, ">"))  { if (g_textFieldActivo){ g_textFieldActivo->CaretDer(); g_redraw = true; } return; }
    if (!strcmp(t, "___")){ FeedStr(" "); return; }
    if (!strcmp(t, "\xC3\xB1")){ FeedStr(caps ? "\xC3\x91" : "\xC3\xB1"); return; } // Ñ / ñ
    if (t[1]=='\0' && t[0]>='a' && t[0]<='z'){ char u[2] = { (char)(caps ? t[0]-32 : t[0]), 0 }; FeedStr(u); return; }
    FeedStr(t); // digito / simbolo
}

bool QwertyPad::Click(int mx, int my){
    if (!Contains(mx, my)) return false; // afuera: LayoutClickUI cierra (Cerrar commitea)
    int ly = my - y - borderGS - dispH;
    if (ly < 0) return true;             // caja de input: consumir sin accion
    int fila = ly / (keyH + gapGS); if (fila < 0) fila = 0; if (fila > QP_FILAS - 1) fila = QP_FILAS - 1;
    int kx[12], kw[12];
    int n = QpRowGeom(gQwertySimbolos, fila, popUpWindow->width, kx, kw);
    int lx = mx - x; int col = n - 1;
    for (int i = 0; i < n; i++){ if (lx < kx[i] + kw[i] + gapGS / 2){ col = i; break; } }
    AccionTecla(fila, col);
    return true;
}

bool QwertyPad::Tecla(int tecla){
    if (tecla == LayoutKey::Enter || tecla == LayoutKey::Accept) { Aceptar(); return true; }
    if (tecla == LayoutKey::Cancel) { Cancelar(); return true; }
    return true;
}

void QwertyPad::Aceptar(){
    if (RenameActivo()) RenameCommit(); else g_textFieldActivo = NULL; // el texto ya esta en field (live)
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}
void QwertyPad::Cancelar(){
    if (RenameActivo()) RenameCancel(); else g_textFieldActivo = NULL;
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}
void QwertyPad::Cerrar(){ // click AFUERA = commit (igual que el Enter fisico)
    if (RenameActivo()) RenameCommit(); else g_textFieldActivo = NULL;
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}

void QwertyAbrir(){
    PopUpBase* prev = PopUpActive;
    if (prev == gQwerty) prev = NULL;
    if (gQwerty){ delete gQwerty; gQwerty = NULL; }
    gQwerty = new QwertyPad();
    gQwerty->prevPopup = prev;
    PopUpActive = gQwerty;
    g_redraw = true;
}
