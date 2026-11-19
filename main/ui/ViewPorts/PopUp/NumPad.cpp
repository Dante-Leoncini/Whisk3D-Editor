#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render/OpcionesRender.h"   // RenderType / g_redraw: son del editor
#include "NumPad.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey + NumInput* (modo transform)
#include "ViewPorts/Timeline.h"             // DopeNumInputChar (transform de keyframes del dope sheet)
#include "ViewPorts/IDE.h"                  // IDE + viewPortActive: destino del simbolo elegido
#include "WhiskUI/widgets/PopupMenu.h"              // MenuPantallaW / MenuPantallaH
#include "WhiskUI/Propieties/PropFloat.h"   // g_propFloatEditando + NumEditCommit/Cancel
#include "WhiskUI/widgets/TextField.h"              // TextFieldInputChar (mismo camino que el teclado fisico)
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/text/font.h"                   // WhiskFont->getMeshTri (barra de espacio: guion estirado)
#include "W3dLang.h"                             // T(): el titulo y los softkeys del popup de simbolos
#include "app/W3dTeclado.h"                      // el teclado del sistema y el area que deja (el IDE arriba del teclado)
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

bool NumPadTeclaCentro(const char* texto, int& cx, int& cy){
    if (!gNumPad || PopUpActive != gNumPad) return false;
    return gNumPad->TeclaCentro(texto, cx, cy);
}

NumPad::NumPad(bool transform) : PopUpBase("NumPad") {
    modoTransform = transform;
    prevPopup = NULL;
    campo = NULL;
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

bool NumPad::TeclaCentro(const char* texto, int& cx, int& cy){
    Reubicar();
    for (int fila = 0; fila < NP_FILAS; fila++)
        for (int col = 0; col < NP_COLS; col++)
            if (kTeclas[fila][col][0] && strcmp(kTeclas[fila][col], texto) == 0) {
                cx = x + borderGS + col * (keyW + gapGS) + keyW / 2;
                cy = y + borderGS + dispH + fila * (keyH + gapGS) + keyH / 2;
                return true;
            }
    return false;
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

TextField* NumPad::CampoEditado(){
    if (campo) return campo;
    return g_propFloatEditando ? &g_propFloatEditando->field : NULL;
}

void NumPad::Render(){
    // la edicion pudo terminar por OTRO camino: el teclado se va solo.
    if (modoTransform){
        if (!NumInputTransformEnCurso()){ CerrarRestaurando(); g_redraw = true; return; }
    } else if (campo) {
        if (g_textFieldActivo != campo){ CerrarRestaurando(); g_redraw = true; return; }   // (Enter fisico, otro foco)
    } else {
        if (!g_propFloatEditando){ Cerrar(); return; } // Enter fisico / click que commiteo
    }
    // el popup sobre el que se abrio (el formulario que se esta editando) sigue a la vista, DEBAJO del teclado:
    // ese popup se corre hacia arriba solo (mira que PopUpActive es otro) y asi se ve todo junto
    if (prevPopup) prevPopup->Render();
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
    if (!modoTransform && CampoEditado()){
        int colLabel = popUpWindow->width / 2 - borderGS - gapGS * 2;
        int ty = borderGS + gapGS;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)ty, 0);
        w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
        RenderBitmapText(campo ? campoEtiqueta : g_propFloatEditando->name, textAlign::left, colLabel);
        w3dEngine::PopMatrix();

        // ---- CAJA de input (fondo oscuro + borde claro, como en Properties) con el valor a la DERECHA ----
        TextField& f = *CampoEditado();
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
        // TODO seleccionado (recien abierto): fondo atras, tipear lo reemplaza; si no, el cursor que parpadea
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        RenderTextField(f, true, textAlign::right, boxW - gapGS * 2);
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
    if (modoTransform){ if (!NumInputChar(c)) DopeNumInputChar(c); } // transform del editor activo (3D/UV/2D); si no, keyframes
    else               TextFieldInputChar(c);
}

void NumPad::AccionTecla(int fila, int col){
    const char* t = kTeclas[fila][col];
    if (t[0] == '\0')       return;                              // hueco
    if (!strcmp(t, "OK"))   {
        // TRANSFORM: el OK solo CIERRA el teclado, NO confirma la transformacion (el valor
        // tipeado ya esta aplicado en vivo; confirmar sigue siendo click / Enter / tilde).
        // En PROPIEDADES (modo float) el OK si confirma el valor, como siempre (Aceptar).
        if (modoTransform){ CerrarRestaurando(); g_redraw = true; return; }
        Aceptar();  return;
    }
    if (!strcmp(t, "X"))    { Cancelar(); return; }              // Cancelar
    if (!strcmp(t, "DEL"))  { Feed(8); }                         // retroceso
    else if (!strcmp(t, "<")){                                   // flecha izquierda
        if (modoTransform) NumInputLeft();
        else if (CampoEditado()) CampoEditado()->CaretIzq();
    }
    else if (!strcmp(t, ">")){                                   // flecha derecha
        if (modoTransform) NumInputRight();
        else if (CampoEditado()) CampoEditado()->CaretDer();
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
    if (campo){   // el numero suelto: el resultado de la expresion queda en el campo y el dueno lo aplica al soltar el foco
        double v = 0.0;
        if (EvaluarExpresion(campo->text, &v)){ char buf[32]; snprintf(buf, sizeof(buf), "%g", v); campo->SetText(buf); }
        if (g_textFieldActivo == campo) g_textFieldActivo = NULL;
        CerrarRestaurando(); g_redraw = true; return;
    }
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
    if (campo){   // vuelve el valor de antes (el dueno no cambia nada)
        campo->SetText(campoAntes);
        if (g_textFieldActivo == campo) g_textFieldActivo = NULL;
        CerrarRestaurando(); g_redraw = true; return;
    }
    NumEditCancel();
    NumEditSalirDelPanel();
    CerrarRestaurando();
    g_redraw = true;
}

void NumPad::Cerrar(){
    // cerrado desde AFUERA (tap fuera del popup).
    if (modoTransform){ CerrarRestaurando(); g_redraw = true; return; } // deja el transform como quedo (valores ya aplicados en vivo)
    if (campo){ Aceptar(); return; }   // el numero suelto: como el Enter
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

void NumPadAbrirCampo(const std::string& etiqueta){
    if (!g_textFieldActivo) return;
    NumPadAbrir();
    gNumPad->campo = g_textFieldActivo;
    gNumPad->campoAntes = g_textFieldActivo->text;
    gNumPad->campoEtiqueta = etiqueta;
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
    ide = NULL;
    keyCard = new Card(NULL, 10, 10);
    keyH = dispH = 0; caps = false;
    Reubicar();
}

void QwertyPad::Reubicar(){
    dispH = RenglonHeightGS + gapGS * 3;   // caja de input (texto que se edita; el IDE se ve arriba) + la CRUZ de cerrar
    keyH  = RenglonHeightGS * 2;                   // alto de tecla (dedos); 4 filas
    int w = MenuPantallaW;
    int h = borderGS * 2 + dispH + (keyH + gapGS) * QP_FILAS;
    x = 0;
    y = MenuPantallaH - h;
    popUpWindow->Resize(w, h);
}

// el IDE sigue en el layout (no se cerro ni se cambio el tipo de su viewport). No se mira el viewport ACTIVO: el dedo
// sobre el teclado puede moverlo al viewport de abajo y el teclado igual escribe en el IDE que lo abrio
static bool QpEnArbol(ViewportBase* v, ViewportBase* buscado){
    if (!v) return false;
    if (v == buscado) return true;
    if (v->ContainerKind() == 1) return QpEnArbol(((ViewportRow*)v)->childA, buscado) || QpEnArbol(((ViewportRow*)v)->childB, buscado);
    if (v->ContainerKind() == 2) return QpEnArbol(((ViewportColumn*)v)->childA, buscado) || QpEnArbol(((ViewportColumn*)v)->childB, buscado);
    return false;
}
static IDE* QpIDE(IDE* ide){
    return (ide && QpEnArbol(rootViewport, (ViewportBase*)ide) && ((ViewportBase*)ide)->ViewportKind() == 8) ? ide : NULL;
}
// el renglon del cursor del IDE queda ARRIBA del teclado (el teclado de Whisk3D no achica la ventana como el del
// sistema en Android: sin esto se escribia debajo de las teclas)
static void QpIDECursorVisible(IDE* ide, int yTeclado){
    if (!ide) return;   // (cada edicion del IDE ya acomoda el scroll a su alto; aca, ademas, sobre el teclado)
    int cx = 0, cy = 0;
    ide->TextoAPunto(ide->curLin, ide->curCol, &cx, &cy);
    const int sobra = cy + RenglonHeightGS * 2 - yTeclado;
    if (sobra > 0){
        ide->PosY -= sobra;
        if (ide->PosY < ide->MaxPosY) ide->PosY = ide->MaxPosY;
    }
}
// EL IDE CON EL TECLADO DE WHISK3D: se MAXIMIZA y ocupa todo lo de arriba del teclado (como con el del sistema, que
// achica la ventana). Al cerrarse vuelve el layout de antes (si lo maximizo el teclado; si ya estaba solo, queda asi)
static bool gIDEMaxPorTeclado = false;
static void QpIDEAcomodar(IDE* ide, int altoTeclado){
    if (ide && altoTeclado > 0){
        ViewportBase* v = (ViewportBase*)ide;
        if (rootViewport != v){
            if (LayoutEstaMaximizado()) LayoutMaximizar();   // (otro panel maximizado: primero vuelve el layout)
            viewPortActive = v;
            LayoutMaximizar();
            gIDEMaxPorTeclado = LayoutEstaMaximizado();
        }
        W3dTecladoIDEAlto(altoTeclado);
        W3dTecladoReacomodar();
        return;
    }
    if (!W3dTecladoIDEAltoActual() && !gIDEMaxPorTeclado) return;
    W3dTecladoIDEAlto(0);
    W3dTecladoReacomodar();                       // (el IDE a todo el alto: el layout se restaura con ese tamanio)
    if (gIDEMaxPorTeclado && LayoutEstaMaximizado()) LayoutMaximizar();
    gIDEMaxPorTeclado = false;
    W3dTecladoReacomodar();
}
void QwertyPad::FeedStr(const char* s){
    if (ide){ IDE* i = QpIDE(ide); if (i){ i->InsertarTipeado(s); QpIDECursorVisible(i, y); } g_redraw = true; return; }
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
    if (ide ? !QpIDE(ide) : !g_textFieldActivo){ Cerrar(); return; } // la edicion termino por otro camino -> el teclado se va solo
    // el formulario sobre el que se abrio (el nombre de un layout, de una textura...) sigue a la vista ARRIBA del
    // teclado: ese popup se corre hacia arriba solo (mira que PopUpActive es otro), como con el numerico
    if (prevPopup) prevPopup->Render();
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

    // ---- LA CRUZ ROJA de arriba a la derecha: CIERRA el teclado (en un navegador no hay boton Atras: sin ella quedaba
    //      abierto para siempre). Como tocar afuera: lo escrito queda ----
    {
        const int boxH = RenglonHeightGS + gapGS;
        const int cx = popUpWindow->width - borderGS - gapGS - boxH;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)cx, (GLfloat)(borderGS + gapGS), 0);
        w3dEngine::Color4f(0.80f, 0.18f, 0.18f, 1.0f);
        keyCard->Resize(boxH, boxH); keyCard->RenderObject(false);
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        w3dEngine::Translatef(0, (GLfloat)((boxH - LetterHeightGS) / 2), 0);
        RenderBitmapText("X", textAlign::center, boxH);
        w3dEngine::PopMatrix();
    }
    // ---- caja de INPUT (fondo oscuro + borde claro, como en Properties) con el texto + caret ----
    if (!ide) {
        int boxH = RenglonHeightGS + gapGS;
        int boxW = popUpWindow->width - 2 * (borderGS + gapGS) - boxH - gapGS;   // (a la derecha, la cruz)
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(borderGS + gapGS), 0);
        w3dEngine::Color4f(negro[0], negro[1], negro[2], 1.0f);
        keyCard->Resize(boxW, boxH); keyCard->RenderObject(false);
        w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
        keyCard->RenderBorder(false);
        TextField& f = *g_textFieldActivo;
        w3dEngine::Translatef((GLfloat)gapGS, (GLfloat)((boxH - RenglonHeightGS) / 2), 0);
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        RenderTextField(f, true, textAlign::left, boxW - gapGS * 2);
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
                const char* lbl = (ide && !strcmp(t, "OK")) ? "Enter" : t; char up[3] = {0,0,0};
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
    if (ide){   // EL IDE: OK es un salto de linea; borrar y las flechas van al cursor del codigo (cerrar: tocar afuera / Atras)
        IDE* i = QpIDE(ide);
        if (i){
            if      (!strcmp(t, "OK"))  { i->Enter();               QpIDECursorVisible(i, y); g_redraw = true; return; }
            else if (!strcmp(t, "DEL")) { i->Backspace();           QpIDECursorVisible(i, y); g_redraw = true; return; }
            else if (!strcmp(t, "<"))   { i->MoverCursor(0, -1, false); QpIDECursorVisible(i, y); g_redraw = true; return; }
            else if (!strcmp(t, ">"))   { i->MoverCursor(0, 1, false);  QpIDECursorVisible(i, y); g_redraw = true; return; }
        }
    }
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
    if (ly < 0) {                        // caja de input: consumir sin accion... salvo la CRUZ, que cierra
        const int boxH = RenglonHeightGS + gapGS;
        const int cx = x + popUpWindow->width - borderGS - gapGS - boxH;
        if (mx >= cx - gapGS && mx <= cx + boxH + gapGS) Cerrar();
        return true;
    }
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
    if (ide){ if (PopUpActive == this) PopUpActive = prevPopup; QpIDEAcomodar(NULL, 0); g_redraw = true; return; }
    // un FORMULARIO abajo (el nombre de un layout): OK es su Enter -> acepta el formulario de una
    PopUpBase* prev = prevPopup;
    if (prev && g_textFieldActivo){
        if (PopUpActive == this) PopUpActive = prev;
        if (prev->TeclaEnTexto(true)){ g_redraw = true; return; }
        if (PopUpActive == prev) PopUpActive = this;   // (no lo tomo: sigue el camino de siempre)
    }
    if (RenameActivo()) RenameCommit(); else g_textFieldActivo = NULL; // el texto ya esta en field (live)
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}
void QwertyPad::Cancelar(){
    if (ide){ if (PopUpActive == this) PopUpActive = prevPopup; QpIDEAcomodar(NULL, 0); g_redraw = true; return; }
    if (RenameActivo()) RenameCancel(); else g_textFieldActivo = NULL;
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}
void QwertyPad::Cerrar(){ // click AFUERA = commit (igual que el Enter fisico)
    if (ide){ if (PopUpActive == this) PopUpActive = prevPopup; QpIDEAcomodar(NULL, 0); g_redraw = true; return; }
    if (RenameActivo()) RenameCommit(); else g_textFieldActivo = NULL;
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}

void QwertyAbrir(){
    PopUpBase* prev = PopUpActive;
    if (prev == gQwerty) prev = NULL;
    if (gQwerty){ if (gQwerty->ide) QpIDEAcomodar(NULL, 0); delete gQwerty; gQwerty = NULL; }
    gQwerty = new QwertyPad();
    gQwerty->prevPopup = prev;
    PopUpActive = gQwerty;
    g_redraw = true;
}
void QwertyAbrirIDE(IDE* ide){
    if (!ide) return;
    if (gQwerty && PopUpActive == gQwerty && gQwerty->ide == ide){   // ya esta abierto para ESE IDE (otro toque en el codigo)
        QpIDECursorVisible(ide, MenuPantallaH - gQwerty->popUpWindow->height);
        return;
    }
    QwertyAbrir();
    gQwerty->ide = ide;
    gQwerty->prevPopup = (gQwerty->prevPopup && gQwerty->prevPopup->NoModal()) ? gQwerty->prevPopup : NULL;   // (Buscar del IDE)
    const int alto = gQwerty->Alto();
    QpIDEAcomodar(ide, alto);   // el IDE maximizado arriba del teclado
    QpIDECursorVisible(ide, MenuPantallaH - alto);
}

// ---- EL TECLADO EN PANTALLA, SIEMPRE EL MISMO (Ajustes > "Use system keyboard"; ver NumPad.h) ----
void TecladoTextoAbrir(){
    if (W3dTecladoDelSistema()) W3dTecladoAbrir();
    else if (g_textFieldActivo) QwertyAbrir();
}
void TecladoNumeroAbrir(){
    if (W3dTecladoDelSistema()) W3dTecladoAbrir();
    else NumPadAbrir();
}
void TecladoNumeroCampoAbrir(const std::string& etiqueta){
    if (W3dTecladoDelSistema()) W3dTecladoAbrir();
    else NumPadAbrirCampo(etiqueta);
}
void TecladoIDEAbrir(IDE* ide){
    if (W3dTecladoDelSistema()) W3dTecladoAbrir();
    else QwertyAbrirIDE(ide);
}
void TecladosVigilar(){
    // (la web en un telefono: el teclado del navegador sigue mientras escribe un campo o el IDE activo)
    W3dTecladoWebVigilar(g_textFieldActivo != NULL || (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 8));
    if (!W3dTecladoIDEAltoActual() && !gIDEMaxPorTeclado) return;
    if (!(gQwerty && gQwerty->ide && PopUpActive == gQwerty)) QpIDEAcomodar(NULL, 0);
}

// ---- EL CAMPO QUE SE EDITA, ARRIBA DEL TECLADO DEL SISTEMA ----
// El de Android tapa media pantalla: el campo puede quedar debajo, en un panel que no se ve, o se escribe sin saber
// donde. Mientras esta abierto, el texto (con su cursor) va en una franja justo arriba del teclado, con la misma caja
// que el QWERTY de Whisk3D. El IDE no la necesita: se ve entero arriba del teclado
class CampoSobreTeclado : public PopUpBase {
    public:
        Card* caja;
        CampoSobreTeclado() : PopUpBase("CampoSobreTeclado") { caja = new Card(NULL, 10, 10); }
        void Render() W3D_OVERRIDE {
            if (!g_textFieldActivo) return;
            const int boxH = RenglonHeightGS + gapGS;
            const int h = boxH + 2 * (borderGS + gapGS);
            x = 0; y = MenuPantallaH - h;   // (MenuPantallaH: lo que no tapa el teclado)
            popUpWindow->Resize(MenuPantallaW, h);
            const float* gris   = ListaColores[static_cast<int>(ColorID::gris)];
            const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
            const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
            const float* negro  = ListaColores[static_cast<int>(ColorID::negro)];
            const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
            initView();
            w3dEngine::Color4f(gris[0], gris[1], gris[2], 0.98f);
            popUpWindow->RenderObject(false);
            w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
            popUpWindow->RenderBorder(false);
            const int boxW = popUpWindow->width - 2 * (borderGS + gapGS);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(borderGS + gapGS), 0);
            w3dEngine::Color4f(negro[0], negro[1], negro[2], 1.0f);
            caja->Resize(boxW, boxH); caja->RenderObject(false);
            w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
            caja->RenderBorder(false);
            w3dEngine::Translatef((GLfloat)gapGS, (GLfloat)((boxH - RenglonHeightGS) / 2), 0);
            w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            RenderTextField(*g_textFieldActivo, true, textAlign::left, boxW - gapGS * 2);
            w3dEngine::PopMatrix();
            endView();
        }
};
static CampoSobreTeclado* gCampoSobreTeclado = NULL;
void TecladoSistemaCampoRender(){
    if (!W3dTecladoVisible() || !g_textFieldActivo) return;
    if (PopUpActive && PopUpActive->EsTeclado()) return;   // (uno de Whisk3D ya muestra su caja)
    if (!gCampoSobreTeclado) gCampoSobreTeclado = new CampoSobreTeclado();
    gCampoSobreTeclado->Render();
}

// ============================================================================
//  SELECTOR DE SIMBOLOS
// ============================================================================
#define NS_COLS 7
#define NS_FILAS 6
static const char* kSimbolos[NS_FILAS][NS_COLS] = {
    { ".", ",", ";", ":", "!", "?", "'"  },
    { "\"","`", "~", "^", "|", "\\","/"  },
    { "(", ")", "[", "]", "{", "}", "="  },
    { "<", ">", "-", "_", "+", "*", "&"  },
    { "%", "$", "#", "@", "0", "1", "2"  },
    { "3", "4", "5", "6", "7", "esp", "ent" },   // esp = espacio, ent = ENTER (nueva linea en el IDE)
};
static std::string gRecientes[7] = { ".", ",", "(", ")", "=", "_", "\"" };
static SimbolosPopup* gSimbolos = NULL;

static void RecientePush(const char* s){
    // MRU real (SIN dedup): cada uso empuja al frente y corre el resto -> el mismo simbolo puede aparecer VARIAS
    // veces en la fila (ej "= ( = )"). Solo se evita duplicarlo si YA esta al frente (no repetir el primero al
    // apretarlo seguido). Antes se deduplicaba y perdia la gracia de "los ultimos usados".
    if (gRecientes[0] == s) return;
    for (int j = 6; j > 0; j--) gRecientes[j] = gRecientes[j - 1];
    gRecientes[0] = s;
}

// destino de texto activo: el IDE editando (kind 8, barFocusIndex<0) o el TextField enfocado
static IDE* SimbIDEDestino(){
    if (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 8 &&
        ((IDE*)viewPortActive)->barFocusIndex < 0) return (IDE*)viewPortActive;
    return NULL;
}
static void SimbInsertar(const char* s){
    IDE* ide = SimbIDEDestino();
    if (!strcmp(s, "ent")){ if (ide) ide->Enter(); return; }   // nueva linea (solo IDE; el TextField es de una linea)
    char c = !strcmp(s, "esp") ? ' ' : s[0];
    if (ide) ide->InsertarChar(c);
    else if (g_textFieldActivo) TextFieldInputChar((int)c);
}

SimbolosPopup::SimbolosPopup() : PopUpBase(T("Select symbol")) {
    celda = new Card(NULL, 10, 10);
    celW = celH = recentY = gridY = 0;
    focoFila = 0; focoCol = 0;
    Reubicar();
}

void SimbolosPopup::Reubicar(){
    int w = MenuPantallaW;
    celH = RenglonHeightGS + gapGS;
    celW = (w - borderGS * 2 - gapGS * (NS_COLS - 1)) / NS_COLS;
    int tituloH = RenglonHeightGS + gapGS;
    recentY = borderGS + tituloH;
    gridY   = recentY + celH + gapGS * 2;
    barraY  = gridY + (celH + gapGS) * NS_FILAS + gapGS;   // barra "aceptar"/"cancelar" abajo de la cuadricula
    int h = barraY + celH + borderGS;
    x = 0;
    y = MenuPantallaH - h;
    popUpWindow->Resize(w, h);
}

// dibuja una celda (fondo header o accent si enfocada) + su etiqueta centrada
static void SimbCelda(Card* celda, int kx, int ky, int celW, int celH, int textoY, const char* txt, bool foco){
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float* header = ListaColores[static_cast<int>(ColorID::headerColor)];
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)kx, (GLfloat)ky, 0);
    if (foco) w3dEngine::Color4f(accent[0] * 0.55f, accent[1] * 0.55f, accent[2] * 0.55f, 1.0f);
    else      w3dEngine::Color4f(header[0], header[1], header[2], 1.0f);
    celda->Resize(celW, celH); celda->RenderObject(false);
    w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
    w3dEngine::Translatef(0, (GLfloat)textoY, 0);
    RenderBitmapText(txt, textAlign::center, celW);
    w3dEngine::PopMatrix();
}

void SimbolosPopup::Render(){
    // si ya no hay destino de texto (Enter fisico, cambio de viewport), el popup se cierra solo
    if (!g_textFieldActivo && !SimbIDEDestino()){ Cerrar(); return; }
    Reubicar();
    const float* gris   = ListaColores[static_cast<int>(ColorID::gris)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];

    initView();
    w3dEngine::Color4f(gris[0], gris[1], gris[2], 0.98f); popUpWindow->RenderObject(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f); popUpWindow->RenderBorder(false);

    // titulo
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)borderGS, 0);
    w3dEngine::Color4f(grisUI[0], grisUI[1], grisUI[2], 1.0f);
    RenderBitmapText(name, textAlign::left, popUpWindow->width - borderGS * 2);
    w3dEngine::PopMatrix();

    int textoY = (celH - RenglonHeightGS) / 2;
    // fila de RECIENTES (focoFila == -1)
    for (int col = 0; col < NS_COLS; col++)
        SimbCelda(celda, borderGS + col * (celW + gapGS), recentY, celW, celH, textoY,
                  gRecientes[col].c_str(), focoFila == -1 && focoCol == col);
    // cuadricula 7x6
    for (int fila = 0; fila < NS_FILAS; fila++)
        for (int col = 0; col < NS_COLS; col++)
            SimbCelda(celda, borderGS + col * (celW + gapGS), gridY + fila * (celH + gapGS), celW, celH, textoY,
                      kSimbolos[fila][col], focoFila == fila && focoCol == col);
    // barra de abajo: "aceptar" (softkey IZQUIERDO) a la izq, "cancelar" (softkey DERECHO) a la der. '*' tambien cierra.
    { const float* acc = ListaColores[static_cast<int>(ColorID::accent)];
      int barTY = barraY + (celH - RenglonHeightGS) / 2;
      w3dEngine::Color4f(acc[0], acc[1], acc[2], 1.0f);
      w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)barTY, 0);
      RenderBitmapText(std::string(T("Accept")), textAlign::left, popUpWindow->width / 2); w3dEngine::PopMatrix();
      w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(popUpWindow->width - borderGS - gapGS), (GLfloat)barTY, 0);
      RenderBitmapText(std::string(T("Cancel")), textAlign::right, popUpWindow->width / 2); w3dEngine::PopMatrix(); }
    endView();
}

void SimbolosPopup::Elegir(const char* s){
    SimbInsertar(s);
    RecientePush(s);
    g_redraw = true;   // queda ABIERTO para elegir mas simbolos; C/soft-izq cierra
}

bool SimbolosPopup::Tecla(int tecla){
    // Convencion Symbian: soft-IZQ (Cancel, 164) = ACEPTAR (inserta el enfocado y CIERRA); soft-DER (Accept, 165) =
    // CANCELAR (cierra sin insertar). OK del centro (Enter) = inserta y QUEDA abierto (para elegir varios).
    if (tecla == LayoutKey::Cancel){
        if (focoFila < 0) Elegir(gRecientes[focoCol].c_str()); else Elegir(kSimbolos[focoFila][focoCol]);
        Cerrar(); return true;
    }
    if (tecla == LayoutKey::Accept){ Cerrar(); return true; }
    if (tecla == LayoutKey::Enter){
        if (focoFila < 0) Elegir(gRecientes[focoCol].c_str());
        else              Elegir(kSimbolos[focoFila][focoCol]);
        return true;
    }
    if (tecla == LayoutKey::Up)   { focoFila--; if (focoFila < -1) focoFila = NS_FILAS - 1; g_redraw = true; return true; }
    if (tecla == LayoutKey::Down) { focoFila++; if (focoFila > NS_FILAS - 1) focoFila = -1; g_redraw = true; return true; }
    if (tecla == LayoutKey::Left) { focoCol--;  if (focoCol < 0) focoCol = NS_COLS - 1;     g_redraw = true; return true; }
    if (tecla == LayoutKey::Right){ focoCol++;  if (focoCol > NS_COLS - 1) focoCol = 0;     g_redraw = true; return true; }
    return true;
}

bool SimbolosPopup::Click(int mx, int my){
    if (!Contains(mx, my)) return false;   // afuera: LayoutClickUI nos cierra
    int lx = mx - x, ly = my - y;
    if (ly >= barraY){   // barra de abajo: mitad izq = aceptar (insertar enfocado + cerrar), mitad der = cancelar (cerrar)
        if (lx < popUpWindow->width / 2){
            if (focoFila < 0) Elegir(gRecientes[focoCol].c_str()); else Elegir(kSimbolos[focoFila][focoCol]);
        }
        Cerrar(); return true;
    }
    if (ly >= recentY && ly < recentY + celH){
        int col = (lx - borderGS) / (celW + gapGS);
        if (col >= 0 && col < NS_COLS){ focoFila = -1; focoCol = col; Elegir(gRecientes[col].c_str()); }
        return true;
    }
    if (ly >= gridY){
        int fila = (ly - gridY) / (celH + gapGS);
        int col  = (lx - borderGS) / (celW + gapGS);
        if (fila >= 0 && fila < NS_FILAS && col >= 0 && col < NS_COLS){ focoFila = fila; focoCol = col; Elegir(kSimbolos[fila][col]); }
        return true;
    }
    return true;
}

void SimbolosAbrir(){
    if (!gSimbolos) gSimbolos = new SimbolosPopup();
    gSimbolos->focoFila = 0; gSimbolos->focoCol = 0;
    PopUpActive = gSimbolos;
    g_redraw = true;
}
bool SimbolosActivo(){ return (gSimbolos && PopUpActive == gSimbolos); }
void SimbolosCerrar(){ if (SimbolosActivo()) gSimbolos->Cerrar(); }
