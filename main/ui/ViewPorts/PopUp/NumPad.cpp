#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render/RenderOptions.h"   // RenderType / g_redraw: son del editor
#include "NumPad.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey + NumInput* (modo transform)
#include "ViewPorts/Timeline.h"             // DopeNumInputChar (transform de keyframes del dope sheet)
#include "ViewPorts/IDE.h"                  // IDE + viewPortActive: destino del simbolo elegido
#include "WhiskUI/widgets/PopupMenu.h"              // MenuScreenW / MenuScreenH
#include "WhiskUI/Propieties/PropFloat.h"   // g_propFloatEditing + NumEditCommit/Cancel
#include "WhiskUI/widgets/TextField.h"              // TextFieldInputChar (mismo camino que el teclado fisico)
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/text/font.h"                   // WhiskFont->getMeshTri (barra de espacio: guion estirado)
#include "W3dLang.h"                             // T(): el titulo y los softkeys del popup de simbolos
#include "app/W3dKeyboard.h"                     // el teclado del sistema y el area que deja (el IDE arriba del teclado)
#include <cstdio>   // snprintf (resultado -> texto)
#include <cstdlib>  // strtod (evaluador)
#include <cstring>  // strcmp (tokens de tecla: "OK" "DEL" "<" ">")

extern bool g_redraw;
extern void NumEditExitFromPanel(); // Properties.cpp: limpia el 'editando' del panel al terminar

// ---------------------------------------------------------------------------
//  Evaluador de EXPRESIONES ( + - * / y parentesis, precedencia normal ) para
//  el Aceptar: "2.5*(1+3)" -> 10. Si la expresion no parsea completa se deja
//  el texto como esta (el commit hace atof igual que siempre).
// ---------------------------------------------------------------------------
static const char* gEp = NULL;
static bool gEvalErr = false;
static double EvalExpr();
static double EvalAtom(){
    while (*gEp == ' ') gEp++;
    if (*gEp == '('){
        gEp++;
        double v = EvalExpr();
        while (*gEp == ' ') gEp++;
        if (*gEp == ')') gEp++; else gEvalErr = true;
        return v;
    }
    if (*gEp == '-'){ gEp++; return -EvalAtom(); } // signo
    if (*gEp == '+'){ gEp++; return  EvalAtom(); }
    char* end = NULL;
    double v = strtod(gEp, &end);
    if (end == gEp){ gEvalErr = true; return 0.0; } // no habia numero
    gEp = end;
    return v;
}
static double EvalEnded(){
    double v = EvalAtom();
    for (;;){
        while (*gEp == ' ') gEp++;
        if (*gEp == '*'){ gEp++; v *= EvalAtom(); }
        else if (*gEp == '/'){ gEp++; double d = EvalAtom(); v = (d != 0.0) ? v / d : 0.0; } // div/0 -> 0
        else return v;
    }
}
static double EvalExpr(){
    double v = EvalEnded();
    for (;;){
        while (*gEp == ' ') gEp++;
        if (*gEp == '+'){ gEp++; v += EvalEnded(); }
        else if (*gEp == '-'){ gEp++; v -= EvalEnded(); }
        else return v;
    }
}
static bool EvaluateExpression(const std::string& s, double* out){
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
#define NP_ROWS 4
static const char* kKeys[NP_ROWS][NP_COLS] = {
    { "7", "8", "9", "(", ")", "DEL" },
    { "4", "5", "6", "*", "/", "<"   },
    { "1", "2", "3", "+", "-", ">"   },
    { ".", "0", "",  "",  "X", "OK"  },
};

static NumPad* gNumPad = NULL;

bool NumPadKeyCenter(const char* text, int& cx, int& cy){
    if (!gNumPad || PopUpActive != gNumPad) return false;
    return gNumPad->KeyCenter(text, cx, cy);
}

NumPad::NumPad(bool transform) : PopUpBase("NumPad") {
    transformMode = transform;
    prevPopup = NULL;
    field = NULL;
    keyCard = new Card(NULL, 10, 10);
    keyW = keyH = dispH = 0;
    Relocate();
}

// cierra el numpad devolviendo el foco al popup que estaba antes (ej: el panel redo del loop cut),
// en vez de dejar PopUpActive en NULL. Sin esto, editar un campo del panel lo hacia desaparecer.
void NumPad::CloseRestoring(){
    if (PopUpActive == this) PopUpActive = prevPopup;
    if (prevPopup) g_redraw = true;
}

bool NumPad::KeyCenter(const char* text, int& cx, int& cy){
    Relocate();
    for (int row = 0; row < NP_ROWS; row++)
        for (int col = 0; col < NP_COLS; col++)
            if (kKeys[row][col][0] && strcmp(kKeys[row][col], text) == 0) {
                cx = x + borderGS + col * (keyW + gapGS) + keyW / 2;
                cy = y + borderGS + dispH + row * (keyH + gapGS) + keyH / 2;
                return true;
            }
    return false;
}

void NumPad::Relocate(){
    // ancho COMPLETO de la ventana, pegado al borde de abajo. Teclas altas (dedos).
    // En modo transform NO hay fila de display (se edita la barra de estado de arriba).
    dispH = transformMode ? 0 : (RowHeightGS + gapGS * 2);
    keyH  = RowHeightGS * 2;                 // alto de tecla
    int w = MenuScreenW;
    int h = borderGS * 2 + dispH + (keyH + gapGS) * NP_ROWS;
    keyW = (w - borderGS * 2 - gapGS * (NP_COLS - 1)) / NP_COLS; // columnas con gap entre medio
    x = 0;
    y = MenuScreenH - h;
    popUpWindow->Resize(w, h);
}

TextField* NumPad::FieldEdited(){
    if (field) return field;
    return g_propFloatEditing ? &g_propFloatEditing->field : NULL;
}

void NumPad::Render(){
    // la edicion pudo terminar por OTRO camino: el teclado se va solo.
    if (transformMode){
        if (!NumInputTransformInProgress()){ CloseRestoring(); g_redraw = true; return; }
    } else if (field) {
        if (g_activeTextField != field){ CloseRestoring(); g_redraw = true; return; }   // (Enter fisico, otro foco)
    } else {
        if (!g_propFloatEditing){ Close(); return; } // Enter fisico / click que commiteo
    }
    // el popup sobre el que se abrio (el formulario que se esta editando) sigue a la vista, DEBAJO del teclado:
    // ese popup se corre hacia arriba solo (mira que PopUpActive es otro) y asi se ve todo junto
    if (prevPopup) prevPopup->Render();
    Relocate(); // por si roto la pantalla / cambio el tamano de la ventana

    const float* gray   = ColorList[static_cast<int>(ColorID::gray)];
    const float* header = ColorList[static_cast<int>(ColorID::headerColor)];
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    const float* white  = ColorList[static_cast<int>(ColorID::white)];
    const float* grayUI = ColorList[static_cast<int>(ColorID::grayUI)];
    const float* black  = ColorList[static_cast<int>(ColorID::black)];
    const float red[3]  = { 0.92f, 0.28f, 0.24f }; // mismo rojo que las notificaciones de error

    initView();

    // fondo + borde accent (popup activo)
    w3dEngine::Color4f(gray[0], gray[1], gray[2], 0.98f);
    popUpWindow->RenderObject(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // ---- display: label del campo a la IZQ + valor (con caret) a la DER ----
    // Solo en modo float. El valor se ancla al borde DERECHO (no al centro) para
    // que un valor largo se extienda hacia la izquierda SIN pisar el label.
    if (!transformMode && FieldEdited()){
        int colLabel = popUpWindow->width / 2 - borderGS - gapGS * 2;
        int ty = borderGS + gapGS;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)ty, 0);
        w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
        RenderBitmapText(field ? fieldLabel : g_propFloatEditing->name, textAlign::left, colLabel);
        w3dEngine::PopMatrix();

        // ---- CAJA de input (fondo oscuro + borde claro, como en Properties) con el valor a la DERECHA ----
        TextField& f = *FieldEdited();
        int boxX = popUpWindow->width / 2;
        int boxW = popUpWindow->width - borderGS - gapGS - boxX;
        int boxH = RowHeightGS + gapGS;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)boxX, (GLfloat)(borderGS + gapGS / 2), 0);
        w3dEngine::Color4f(black[0], black[1], black[2], 1.0f);
        keyCard->Resize(boxW, boxH); keyCard->RenderObject(false);
        w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
        keyCard->RenderBorder(false);
        w3dEngine::Translatef((GLfloat)(boxW - gapGS), (GLfloat)((boxH - RowHeightGS) / 2), 0);
        // TODO seleccionado (recien abierto): fondo atras, tipear lo reemplaza; si no, el cursor que parpadea
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        RenderTextField(f, true, textAlign::right, boxW - gapGS * 2);
        w3dEngine::PopMatrix();
    }

    // ---- teclas ----
    int textY = (keyH - RowHeightGS) / 2; // texto centrado vertical en la tecla
    for (int rowIdx = 0; rowIdx < NP_ROWS; rowIdx++){
        for (int col = 0; col < NP_COLS; col++){
            const char* t = kKeys[rowIdx][col];
            if (t[0] == '\0') continue; // hueco: sin tecla
            int kx = borderGS + col * (keyW + gapGS);
            int ky = borderGS + dispH + rowIdx * (keyH + gapGS);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)kx, (GLfloat)ky, 0);
            // fondo: OK verde / X rojo / resto gris de header
            if      (!strcmp(t, "OK")) w3dEngine::Color4f(accent[0] * 0.55f, accent[1] * 0.55f, accent[2] * 0.55f, 1.0f);
            else if (!strcmp(t, "X"))  w3dEngine::Color4f(red[0] * 0.55f, red[1] * 0.55f, red[2] * 0.55f, 1.0f);
            else                       w3dEngine::Color4f(header[0], header[1], header[2], 1.0f);
            keyCard->Resize(keyW, keyH);
            keyCard->RenderObject(false);
            // etiqueta
            if      (!strcmp(t, "OK")) w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
            else if (!strcmp(t, "X"))  w3dEngine::Color4f(red[0], red[1], red[2], 1.0f);
            else                       w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            w3dEngine::Translatef(0, (GLfloat)textY, 0);
            RenderBitmapText(t, textAlign::center, keyW);
            w3dEngine::PopMatrix();
        }
    }

    endView();
}

// alimenta un caracter (o 8=backspace) al destino segun el modo
void NumPad::Feed(int c){
    if (transformMode){ if (!NumInputChar(c)) DopeNumInputChar(c); } // transform del editor activo (3D/UV/2D); si no, keyframes
    else               TextFieldInputChar(c);
}

void NumPad::ActionKey(int rowIdx, int col){
    const char* t = kKeys[rowIdx][col];
    if (t[0] == '\0')       return;                              // hueco
    if (!strcmp(t, "OK"))   {
        // TRANSFORM: el OK solo CIERRA el teclado, NO confirma la transformacion (el valor
        // tipeado ya esta aplicado en vivo; confirmar sigue siendo click / Enter / tilde).
        // En PROPIEDADES (modo float) el OK si confirma el valor, como siempre (Aceptar).
        if (transformMode){ CloseRestoring(); g_redraw = true; return; }
        Accept();  return;
    }
    if (!strcmp(t, "X"))    { Cancel(); return; }              // Cancelar
    if (!strcmp(t, "DEL"))  { Feed(8); }                       // retroceso
    else if (!strcmp(t, "<")){                                 // flecha izquierda
        if (transformMode) NumInputLeft();
        else if (FieldEdited()) FieldEdited()->CaretLeft();
    }
    else if (!strcmp(t, ">")){                                   // flecha derecha
        if (transformMode) NumInputRight();
        else if (FieldEdited()) FieldEdited()->CaretRight();
    }
    else                    { Feed(t[0]); }                      // digito / . / operador
    g_redraw = true;
}

bool NumPad::Click(int mx, int my){
    if (!Contains(mx, my)) return false; // afuera: LayoutClickUI nos cierra (Cerrar commitea)
    int lx = mx - x;
    int ly = my - y - borderGS - dispH;  // relativo a la zona de teclas
    if (ly < 0) return true;             // el display: consumir sin accion
    int row = ly / (keyH + gapGS); if (row > NP_ROWS - 1) row = NP_ROWS - 1;
    int col = (lx - borderGS) / (keyW + gapGS);
    if (col < 0) col = 0; if (col > NP_COLS - 1) col = NP_COLS - 1;
    ActionKey(row, col);
    return true;
}

bool NumPad::Key(int key){
    if (key == LayoutKey::Enter || key == LayoutKey::Accept) { Accept(); return true; }
    if (key == LayoutKey::Cancel) { Cancel(); return true; }
    return true; // el resto (flechas) no navega nada aca
}

void NumPad::Accept(){
    if (transformMode){ NumInputConfirm(); CloseRestoring(); g_redraw = true; return; }
    if (field){   // el numero suelto: el resultado de la expresion queda en el campo y el dueno lo aplica al soltar el foco
        double v = 0.0;
        if (EvaluateExpression(field->text, &v)){ char buf[32]; snprintf(buf, sizeof(buf), "%g", v); field->SetText(buf); }
        if (g_activeTextField == field) g_activeTextField = NULL;
        CloseRestoring(); g_redraw = true; return;
    }
    if (g_propFloatEditing){
        double v = 0.0;
        if (EvaluateExpression(g_propFloatEditing->field.text, &v)){
            char buf[32];
            snprintf(buf, sizeof(buf), "%g", v);
            g_propFloatEditing->field.SetText(buf); // el commit parsea este resultado
        }
        NumEditCommit();
        NumEditExitFromPanel();
    }
    CloseRestoring();
    g_redraw = true;
}

void NumPad::Cancel(){
    if (transformMode){ NumInputCancel(); CloseRestoring(); g_redraw = true; return; }
    if (field){   // vuelve el valor de antes (el dueno no cambia nada)
        field->SetText(fieldPrev);
        if (g_activeTextField == field) g_activeTextField = NULL;
        CloseRestoring(); g_redraw = true; return;
    }
    NumEditCancel();
    NumEditExitFromPanel();
    CloseRestoring();
    g_redraw = true;
}

void NumPad::Close(){
    // cerrado desde AFUERA (tap fuera del popup).
    if (transformMode){ CloseRestoring(); g_redraw = true; return; } // deja el transform como quedo (valores ya aplicados en vivo)
    if (field){ Accept(); return; }   // el numero suelto: como el Enter
    // modo float: commitea lo tipeado, igual que la edicion inline
    if (g_propFloatEditing){ NumEditCommit(); NumEditExitFromPanel(); }
    CloseRestoring();
    g_redraw = true;
}

void NumPadOpen(){
    PopUpBase* prev = PopUpActive; // si se abre SOBRE un popup (ej: panel redo del loop cut), se restaura al cerrar
    if (prev == gNumPad) prev = NULL; // no restaurarse a si mismo (y evita puntero colgante al borrar gNumPad)
    if (gNumPad){ delete gNumPad; gNumPad = NULL; } // reemplaza la instancia anterior
    gNumPad = new NumPad(false);
    gNumPad->prevPopup = prev;
    PopUpActive = gNumPad;
    g_redraw = true;
}

void NumPadOpenField(const std::string& label){
    if (!g_activeTextField) return;
    NumPadOpen();
    gNumPad->field = g_activeTextField;
    gNumPad->fieldPrev = g_activeTextField->text;
    gNumPad->fieldLabel = label;
}

void NumPadOpenTransform(){
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
extern bool RenameActive();   // controls.cpp: hay un rename en curso?
extern void RenameCommit();   // aceptar el rename
extern void RenameCancel();   // descartar el rename

struct QKey { const char* t; float w; };      // etiqueta + peso de ancho (fila = suma de pesos)
#define QP_ROWS 4
// terminador de fila: {NULL, 0}
static const QKey kRowsLetter[QP_ROWS][12] = {
    {{"q",1},{"w",1},{"e",1},{"r",1},{"t",1},{"y",1},{"u",1},{"i",1},{"o",1},{"p",1},{NULL,0}},
    {{"a",1},{"s",1},{"d",1},{"f",1},{"g",1},{"h",1},{"j",1},{"k",1},{"l",1},{"\xC3\xB1",1},{NULL,0}}, // ñ
    {{"Aa",1.5f},{"z",1},{"x",1},{"c",1},{"v",1},{"b",1},{"n",1},{"m",1},{".",1},{"DEL",1.5f},{NULL,0}},
    {{"123",1.6f},{"___",5.4f},{"<",1},{">",1},{"OK",2.0f},{NULL,0}},
};
static const QKey kRowsSymbol[QP_ROWS][12] = {
    {{"1",1},{"2",1},{"3",1},{"4",1},{"5",1},{"6",1},{"7",1},{"8",1},{"9",1},{"0",1},{NULL,0}},
    {{"-",1},{"_",1},{"/",1},{":",1},{"(",1},{")",1},{"$",1},{"&",1},{"@",1},{"\"",1},{NULL,0}},
    {{".",1},{",",1},{"?",1},{"!",1},{"'",1},{"#",1},{"%",1},{"=",1},{"+",1},{"DEL",1.5f},{NULL,0}},
    {{"ABC",1.6f},{"___",5.4f},{"<",1},{">",1},{"OK",2.0f},{NULL,0}},
};

static QwertyPad* gQwerty = NULL;
static bool gQwertySymbols = false; // modo actual (persiste entre aperturas)

// geometria de una fila: cuenta teclas + suma de pesos, y el ancho util (con gaps)
static const QKey* QpRow(bool symbols, int row){ return symbols ? kRowsSymbol[row] : kRowsLetter[row]; }

QwertyPad::QwertyPad() : PopUpBase("Qwerty") {
    prevPopup = NULL;
    ide = NULL;
    keyCard = new Card(NULL, 10, 10);
    keyH = dispH = 0; caps = false;
    Relocate();
}

void QwertyPad::Relocate(){
    dispH = RowHeightGS + gapGS * 3;   // caja de input (texto que se edita; el IDE se ve arriba) + la CRUZ de cerrar
    keyH  = RowHeightGS * 2;                   // alto de tecla (dedos); 4 filas
    int w = MenuScreenW;
    int h = borderGS * 2 + dispH + (keyH + gapGS) * QP_ROWS;
    x = 0;
    y = MenuScreenH - h;
    popUpWindow->Resize(w, h);
}

// el IDE sigue en el layout (no se cerro ni se cambio el tipo de su viewport). No se mira el viewport ACTIVO: el dedo
// sobre el teclado puede moverlo al viewport de abajo y el teclado igual escribe en el IDE que lo abrio
static bool QpInTree(ViewportBase* v, ViewportBase* found){
    if (!v) return false;
    if (v == found) return true;
    if (v->ContainerKind() == 1) return QpInTree(((ViewportRow*)v)->childA, found) || QpInTree(((ViewportRow*)v)->childB, found);
    if (v->ContainerKind() == 2) return QpInTree(((ViewportColumn*)v)->childA, found) || QpInTree(((ViewportColumn*)v)->childB, found);
    return false;
}
static IDE* QpIDE(IDE* ide){
    return (ide && QpInTree(rootViewport, (ViewportBase*)ide) && ((ViewportBase*)ide)->ViewportKind() == 8) ? ide : NULL;
}
// el renglon del cursor del IDE queda ARRIBA del teclado (el teclado de Whisk3D no achica la ventana como el del
// sistema en Android: sin esto se escribia debajo de las teclas)
static void QpIDECursorVisible(IDE* ide, int yKeyboard){
    if (!ide) return;   // (cada edicion del IDE ya acomoda el scroll a su alto; aca, ademas, sobre el teclado)
    int cx = 0, cy = 0;
    ide->TextToPoint(ide->curLin, ide->curCol, &cx, &cy);
    const int extra = cy + RowHeightGS * 2 - yKeyboard;
    if (extra > 0){
        ide->PosY -= extra;
        if (ide->PosY < ide->MaxPosY) ide->PosY = ide->MaxPosY;
    }
}
// EL IDE CON EL TECLADO DE WHISK3D: se MAXIMIZA y ocupa todo lo de arriba del teclado (como con el del sistema, que
// achica la ventana). Al cerrarse vuelve el layout de antes (si lo maximizo el teclado; si ya estaba solo, queda asi)
static bool gIDEMaxByKeyboard = false;
static void QpIDEArrange(IDE* ide, int keyboardHeight){
    if (ide && keyboardHeight > 0){
        ViewportBase* v = (ViewportBase*)ide;
        if (rootViewport != v){
            if (LayoutIsMaximized()) LayoutMaximize();   // (otro panel maximizado: primero vuelve el layout)
            viewPortActive = v;
            LayoutMaximize();
            gIDEMaxByKeyboard = LayoutIsMaximized();
        }
        W3dKeyboardIDEHeight(keyboardHeight);
        W3dKeyboardRearrange();
        return;
    }
    if (!W3dKeyboardIDECurrentHeight() && !gIDEMaxByKeyboard) return;
    W3dKeyboardIDEHeight(0);
    W3dKeyboardRearrange();                       // (el IDE a todo el alto: el layout se restaura con ese tamanio)
    if (gIDEMaxByKeyboard && LayoutIsMaximized()) LayoutMaximize();
    gIDEMaxByKeyboard = false;
    W3dKeyboardRearrange();
}
void QwertyPad::FeedStr(const char* s){
    if (ide){ IDE* i = QpIDE(ide); if (i){ i->TextTyped(s); QpIDECursorVisible(i, y); } g_redraw = true; return; }   // (TextTyped: tambien el autocompletado)
    if (!g_activeTextField) return;
    for (const char* p = s; *p; p++) g_activeTextField->InsertChar((unsigned char)*p); // byte a byte (ñ = 2 bytes)
    g_redraw = true;
}

// geometria de una fila: llena kx[]/kw[] (ancho PONDERADO por peso) y devuelve la cantidad de teclas.
static int QpRowGeom(bool symbols, int rowIdx, int fullW, int kx[12], int kw[12]){
    const QKey* row = QpRow(symbols, rowIdx);
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
    if (ide ? !QpIDE(ide) : !g_activeTextField){ Close(); return; } // la edicion termino por otro camino -> el teclado se va solo
    // el formulario sobre el que se abrio (el nombre de un layout, de una textura...) sigue a la vista ARRIBA del
    // teclado: ese popup se corre hacia arriba solo (mira que PopUpActive es otro), como con el numerico
    if (prevPopup) prevPopup->Render();
    Relocate();

    const float* gray   = ColorList[static_cast<int>(ColorID::gray)];
    const float* header = ColorList[static_cast<int>(ColorID::headerColor)];
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    const float* white  = ColorList[static_cast<int>(ColorID::white)];
    const float* black  = ColorList[static_cast<int>(ColorID::black)];
    const float* grayUI = ColorList[static_cast<int>(ColorID::grayUI)];

    initView();
    w3dEngine::Color4f(gray[0], gray[1], gray[2], 0.98f);
    popUpWindow->RenderObject(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
    popUpWindow->RenderBorder(false);

    // ---- LA CRUZ ROJA de arriba a la derecha: CIERRA el teclado (en un navegador no hay boton Atras: sin ella quedaba
    //      abierto para siempre). Como tocar afuera: lo escrito queda ----
    {
        const int boxH = RowHeightGS + gapGS;
        const int cx = popUpWindow->width - borderGS - gapGS - boxH;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)cx, (GLfloat)(borderGS + gapGS), 0);
        w3dEngine::Color4f(0.80f, 0.18f, 0.18f, 1.0f);
        keyCard->Resize(boxH, boxH); keyCard->RenderObject(false);
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        w3dEngine::Translatef(0, (GLfloat)((boxH - LetterHeightGS) / 2), 0);
        RenderBitmapText("X", textAlign::center, boxH);
        w3dEngine::PopMatrix();
    }
    // ---- caja de INPUT (fondo oscuro + borde claro, como en Properties) con el texto + caret ----
    if (!ide) {
        int boxH = RowHeightGS + gapGS;
        int boxW = popUpWindow->width - 2 * (borderGS + gapGS) - boxH - gapGS;   // (a la derecha, la cruz)
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(borderGS + gapGS), 0);
        w3dEngine::Color4f(black[0], black[1], black[2], 1.0f);
        keyCard->Resize(boxW, boxH); keyCard->RenderObject(false);
        w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
        keyCard->RenderBorder(false);
        TextField& f = *g_activeTextField;
        w3dEngine::Translatef((GLfloat)gapGS, (GLfloat)((boxH - RowHeightGS) / 2), 0);
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        RenderTextField(f, true, textAlign::left, boxW - gapGS * 2);
        w3dEngine::PopMatrix();
    }

    int textY = (keyH - RowHeightGS) / 2;
    int kx[12], kw[12];
    for (int rowIdx = 0; rowIdx < QP_ROWS; rowIdx++){
        const QKey* row = QpRow(gQwertySymbols, rowIdx);
        int n = QpRowGeom(gQwertySymbols, rowIdx, popUpWindow->width, kx, kw);
        int ky = borderGS + dispH + rowIdx * (keyH + gapGS);
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
            else                  w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            if (!strcmp(t, "___")){
                // BARRA DE ESPACIO: un guion bajo ESTIRADO a lo ancho de la tecla (el glifo mide getMeshTri()[2])
                const GLfloat* mt = WhiskFont ? WhiskFont->getMeshTri() : NULL;
                float gw = (mt && mt[2] > 1.0f) ? mt[2] : 5.0f;
                float sc = (float)(kw[i] - gapGS * 2) / gw;
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)gapGS, (GLfloat)textY, 0);
                w3dEngine::Scalef(sc, 1.0f, 1.0f);
                RenderBitmapText("_", textAlign::left, 1000000); // sin recorte
                w3dEngine::PopMatrix();
            } else {
                const char* lbl = (ide && !strcmp(t, "OK")) ? "Enter" : t; char up[3] = {0,0,0};
                if (caps){
                    if (t[1]=='\0' && t[0]>='a' && t[0]<='z'){ up[0]=(char)(t[0]-32); lbl=up; }
                    else if (!strcmp(t, "\xC3\xB1")){ up[0]='\xC3'; up[1]='\x91'; lbl=up; } // Ñ
                }
                w3dEngine::Translatef(0, (GLfloat)textY, 0);
                RenderBitmapText(lbl, textAlign::center, kw[i]);
            }
            w3dEngine::PopMatrix();
        }
    }
    endView();
}

void QwertyPad::ActionKey(int rowIdx, int col){
    const QKey* row = QpRow(gQwertySymbols, rowIdx);
    int n = 0; for (; row[n].t; n++) {}
    if (col < 0 || col >= n) return;
    const char* t = row[col].t;
    if (ide){   // EL IDE: OK es un salto de linea; borrar y las flechas van al cursor del codigo (cerrar: tocar afuera / Atras)
        IDE* i = QpIDE(ide);
        if (i){
            // (con la lista del autocompletado abierta, OK escribe el elegido; DEL la sigue filtrando)
            if      (!strcmp(t, "OK"))  { if (i->acOpen) i->CompletionAccept(); else i->Enter(); QpIDECursorVisible(i, y); g_redraw = true; return; }
            else if (!strcmp(t, "DEL")) { i->Backspace(); if (i->acOpen) i->CompletionUpdate(false); QpIDECursorVisible(i, y); g_redraw = true; return; }
            else if (!strcmp(t, "<"))   { i->MoveCursor(0, -1, false); QpIDECursorVisible(i, y); g_redraw = true; return; }
            else if (!strcmp(t, ">"))   { i->MoveCursor(0, 1, false);  QpIDECursorVisible(i, y); g_redraw = true; return; }
        }
    }
    if (!strcmp(t, "OK")) { Accept(); return; }
    if (!strcmp(t, "123") || !strcmp(t, "ABC")) { gQwertySymbols = !gQwertySymbols; g_redraw = true; return; }
    if (!strcmp(t, "Aa")) { caps = !caps; g_redraw = true; return; }
    if (!strcmp(t, "DEL")){ if (g_activeTextField){ g_activeTextField->Backspace(); g_redraw = true; } return; }
    if (!strcmp(t, "<"))  { if (g_activeTextField){ g_activeTextField->CaretLeft(); g_redraw = true; } return; }
    if (!strcmp(t, ">"))  { if (g_activeTextField){ g_activeTextField->CaretRight(); g_redraw = true; } return; }
    if (!strcmp(t, "___")){ FeedStr(" "); return; }
    if (!strcmp(t, "\xC3\xB1")){ FeedStr(caps ? "\xC3\x91" : "\xC3\xB1"); return; } // Ñ / ñ
    if (t[1]=='\0' && t[0]>='a' && t[0]<='z'){ char u[2] = { (char)(caps ? t[0]-32 : t[0]), 0 }; FeedStr(u); return; }
    FeedStr(t); // digito / simbolo
}

bool QwertyPad::Click(int mx, int my){
    if (!Contains(mx, my)) return false; // afuera: LayoutClickUI cierra (Cerrar commitea)
    int ly = my - y - borderGS - dispH;
    if (ly < 0) {                        // caja de input: consumir sin accion... salvo la CRUZ, que cierra
        const int boxH = RowHeightGS + gapGS;
        const int cx = x + popUpWindow->width - borderGS - gapGS - boxH;
        if (mx >= cx - gapGS && mx <= cx + boxH + gapGS) Close();
        return true;
    }
    int row = ly / (keyH + gapGS); if (row < 0) row = 0; if (row > QP_ROWS - 1) row = QP_ROWS - 1;
    int kx[12], kw[12];
    int n = QpRowGeom(gQwertySymbols, row, popUpWindow->width, kx, kw);
    int lx = mx - x; int col = n - 1;
    for (int i = 0; i < n; i++){ if (lx < kx[i] + kw[i] + gapGS / 2){ col = i; break; } }
    ActionKey(row, col);
    return true;
}

bool QwertyPad::Key(int key){
    if (key == LayoutKey::Enter || key == LayoutKey::Accept) { Accept(); return true; }
    if (key == LayoutKey::Cancel) { Cancel(); return true; }
    return true;
}

void QwertyPad::Accept(){
    if (ide){ if (PopUpActive == this) PopUpActive = prevPopup; QpIDEArrange(NULL, 0); g_redraw = true; return; }
    // un FORMULARIO abajo (el nombre de un layout): OK es su Enter -> acepta el formulario de una
    PopUpBase* prev = prevPopup;
    if (prev && g_activeTextField){
        if (PopUpActive == this) PopUpActive = prev;
        if (prev->KeyInText(true)){ g_redraw = true; return; }
        if (PopUpActive == prev) PopUpActive = this;   // (no lo tomo: sigue el camino de siempre)
    }
    if (RenameActive()) RenameCommit(); else g_activeTextField = NULL; // el texto ya esta en field (live)
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}
void QwertyPad::Cancel(){
    if (ide){ if (PopUpActive == this) PopUpActive = prevPopup; QpIDEArrange(NULL, 0); g_redraw = true; return; }
    if (RenameActive()) RenameCancel(); else g_activeTextField = NULL;
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}
void QwertyPad::Close(){ // click AFUERA = commit (igual que el Enter fisico)
    if (ide){ if (PopUpActive == this) PopUpActive = prevPopup; QpIDEArrange(NULL, 0); g_redraw = true; return; }
    if (RenameActive()) RenameCommit(); else g_activeTextField = NULL;
    if (PopUpActive == this) PopUpActive = prevPopup;
    g_redraw = true;
}

void QwertyOpen(){
    PopUpBase* prev = PopUpActive;
    if (prev == gQwerty) prev = NULL;
    if (gQwerty){ if (gQwerty->ide) QpIDEArrange(NULL, 0); delete gQwerty; gQwerty = NULL; }
    gQwerty = new QwertyPad();
    gQwerty->prevPopup = prev;
    PopUpActive = gQwerty;
    g_redraw = true;
}
void QwertyOpenIDE(IDE* ide){
    if (!ide) return;
    if (gQwerty && PopUpActive == gQwerty && gQwerty->ide == ide){   // ya esta abierto para ESE IDE (otro toque en el codigo)
        QpIDECursorVisible(ide, MenuScreenH - gQwerty->popUpWindow->height);
        return;
    }
    QwertyOpen();
    gQwerty->ide = ide;
    gQwerty->prevPopup = (gQwerty->prevPopup && gQwerty->prevPopup->NoModal()) ? gQwerty->prevPopup : NULL;   // (Buscar del IDE)
    const int height = gQwerty->Height();
    QpIDEArrange(ide, height);   // el IDE maximizado arriba del teclado
    QpIDECursorVisible(ide, MenuScreenH - height);
}

// ---- EL TECLADO EN PANTALLA, SIEMPRE EL MISMO (Ajustes > "Use system keyboard"; ver NumPad.h) ----
void KeyboardTextOpen(){
    if (W3dSystemKeyboard()) W3dKeyboardOpen();
    else if (g_activeTextField) QwertyOpen();
}
void KeyboardNumberOpen(){
    if (W3dSystemKeyboard()) W3dKeyboardOpen();
    else NumPadOpen();
}
void KeyboardNumberFieldOpen(const std::string& label){
    if (W3dSystemKeyboard()) W3dKeyboardOpen();
    else NumPadOpenField(label);
}
void KeyboardIDEOpen(IDE* ide){
    if (W3dSystemKeyboard()) W3dKeyboardOpen();
    else QwertyOpenIDE(ide);
}
void KeyboardsWatch(){
    // (la web en un telefono: el teclado del navegador sigue mientras escribe un campo o el IDE activo)
    W3dKeyboardWebWatch(g_activeTextField != NULL || (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 8));
    if (!W3dKeyboardIDECurrentHeight() && !gIDEMaxByKeyboard) return;
    if (!(gQwerty && gQwerty->ide && PopUpActive == gQwerty)) QpIDEArrange(NULL, 0);
}

// ---- EL CAMPO QUE SE EDITA, ARRIBA DEL TECLADO DEL SISTEMA ----
// El de Android tapa media pantalla: el campo puede quedar debajo, en un panel que no se ve, o se escribe sin saber
// donde. Mientras esta abierto, el texto (con su cursor) va en una franja justo arriba del teclado, con la misma caja
// que el QWERTY de Whisk3D. El IDE no la necesita: se ve entero arriba del teclado
class FieldOverKeyboard : public PopUpBase {
    public:
        Card* box;
        FieldOverKeyboard() : PopUpBase("FieldOverKeyboard") { box = new Card(NULL, 10, 10); }
        void Render() W3D_OVERRIDE {
            if (!g_activeTextField) return;
            const int boxH = RowHeightGS + gapGS;
            const int h = boxH + 2 * (borderGS + gapGS);
            x = 0; y = MenuScreenH - h;   // (MenuScreenH: lo que no tapa el teclado)
            popUpWindow->Resize(MenuScreenW, h);
            const float* gray   = ColorList[static_cast<int>(ColorID::gray)];
            const float* accent = ColorList[static_cast<int>(ColorID::accent)];
            const float* white  = ColorList[static_cast<int>(ColorID::white)];
            const float* black  = ColorList[static_cast<int>(ColorID::black)];
            const float* grayUI = ColorList[static_cast<int>(ColorID::grayUI)];
            initView();
            w3dEngine::Color4f(gray[0], gray[1], gray[2], 0.98f);
            popUpWindow->RenderObject(false);
            w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f);
            popUpWindow->RenderBorder(false);
            const int boxW = popUpWindow->width - 2 * (borderGS + gapGS);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)(borderGS + gapGS), 0);
            w3dEngine::Color4f(black[0], black[1], black[2], 1.0f);
            box->Resize(boxW, boxH); box->RenderObject(false);
            w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
            box->RenderBorder(false);
            w3dEngine::Translatef((GLfloat)gapGS, (GLfloat)((boxH - RowHeightGS) / 2), 0);
            w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            RenderTextField(*g_activeTextField, true, textAlign::left, boxW - gapGS * 2);
            w3dEngine::PopMatrix();
            endView();
        }
};
static FieldOverKeyboard* gFieldOverKeyboard = NULL;
void KeyboardSystemFieldRender(){
    if (!W3dKeyboardVisible() || !g_activeTextField) return;
    if (PopUpActive && PopUpActive->IsKeyboard()) return;   // (uno de Whisk3D ya muestra su caja)
    if (!gFieldOverKeyboard) gFieldOverKeyboard = new FieldOverKeyboard();
    gFieldOverKeyboard->Render();
}

// ============================================================================
//  SELECTOR DE SIMBOLOS
// ============================================================================
#define NS_COLS 7
#define NS_ROWS 6
static const char* kSymbols[NS_ROWS][NS_COLS] = {
    { ".", ",", ";", ":", "!", "?", "'"  },
    { "\"","`", "~", "^", "|", "\\","/"  },
    { "(", ")", "[", "]", "{", "}", "="  },
    { "<", ">", "-", "_", "+", "*", "&"  },
    { "%", "$", "#", "@", "0", "1", "2"  },
    { "3", "4", "5", "6", "7", "esp", "ent" },   // esp = espacio, ent = ENTER (nueva linea en el IDE)
};
static std::string gRecent[7] = { ".", ",", "(", ")", "=", "_", "\"" };
static SymbolsPopup* gSymbols = NULL;

static void RecentPush(const char* s){
    // MRU real (SIN dedup): cada uso empuja al frente y corre el resto -> el mismo simbolo puede aparecer VARIAS
    // veces en la fila (ej "= ( = )"). Solo se evita duplicarlo si YA esta al frente (no repetir el primero al
    // apretarlo seguido). Antes se deduplicaba y perdia la gracia de "los ultimos usados".
    if (gRecent[0] == s) return;
    for (int j = 6; j > 0; j--) gRecent[j] = gRecent[j - 1];
    gRecent[0] = s;
}

// destino de texto activo: el IDE editando (kind 8, barFocusIndex<0) o el TextField enfocado
static IDE* SymbolIDEDestination(){
    if (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 8 &&
        ((IDE*)viewPortActive)->barFocusIndex < 0) return (IDE*)viewPortActive;
    return NULL;
}
static void SymbolInsert(const char* s){
    IDE* ide = SymbolIDEDestination();
    if (!strcmp(s, "ent")){ if (ide) ide->Enter(); return; }   // nueva linea (solo IDE; el TextField es de una linea)
    char c = !strcmp(s, "esp") ? ' ' : s[0];
    if (ide) { const char t[2] = { c, 0 }; ide->TextTyped(t); }   // (un punto abre la lista de math.; otro signo la cierra)
    else if (g_activeTextField) TextFieldInputChar((int)c);
}

SymbolsPopup::SymbolsPopup() : PopUpBase(T("Select symbol")) {
    cell = new Card(NULL, 10, 10);
    celW = celH = recentY = gridY = 0;
    focusRow = 0; focusCol = 0;
    Relocate();
}

void SymbolsPopup::Relocate(){
    int w = MenuScreenW;
    celH = RowHeightGS + gapGS;
    celW = (w - borderGS * 2 - gapGS * (NS_COLS - 1)) / NS_COLS;
    int titleH = RowHeightGS + gapGS;
    recentY = borderGS + titleH;
    gridY   = recentY + celH + gapGS * 2;
    barY    = gridY + (celH + gapGS) * NS_ROWS + gapGS;   // barra "aceptar"/"cancelar" abajo de la cuadricula
    int h = barY + celH + borderGS;
    x = 0;
    y = MenuScreenH - h;
    popUpWindow->Resize(w, h);
}

// dibuja una celda (fondo header o accent si enfocada) + su etiqueta centrada
static void SymbolCell(Card* cell, int kx, int ky, int celW, int celH, int textY, const char* txt, bool focus){
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    const float* white  = ColorList[static_cast<int>(ColorID::white)];
    const float* header = ColorList[static_cast<int>(ColorID::headerColor)];
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)kx, (GLfloat)ky, 0);
    if (focus) w3dEngine::Color4f(accent[0] * 0.55f, accent[1] * 0.55f, accent[2] * 0.55f, 1.0f);
    else      w3dEngine::Color4f(header[0], header[1], header[2], 1.0f);
    cell->Resize(celW, celH); cell->RenderObject(false);
    w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
    w3dEngine::Translatef(0, (GLfloat)textY, 0);
    RenderBitmapText(txt, textAlign::center, celW);
    w3dEngine::PopMatrix();
}

void SymbolsPopup::Render(){
    // si ya no hay destino de texto (Enter fisico, cambio de viewport), el popup se cierra solo
    if (!g_activeTextField && !SymbolIDEDestination()){ Close(); return; }
    Relocate();
    const float* gray   = ColorList[static_cast<int>(ColorID::gray)];
    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    const float* grayUI = ColorList[static_cast<int>(ColorID::grayUI)];

    initView();
    w3dEngine::Color4f(gray[0], gray[1], gray[2], 0.98f); popUpWindow->RenderObject(false);
    w3dEngine::Color4f(accent[0], accent[1], accent[2], 1.0f); popUpWindow->RenderBorder(false);

    // titulo
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)borderGS, 0);
    w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);
    RenderBitmapText(name, textAlign::left, popUpWindow->width - borderGS * 2);
    w3dEngine::PopMatrix();

    int textY = (celH - RowHeightGS) / 2;
    // fila de RECIENTES (focusRow == -1)
    for (int col = 0; col < NS_COLS; col++)
        SymbolCell(cell, borderGS + col * (celW + gapGS), recentY, celW, celH, textY,
                  gRecent[col].c_str(), focusRow == -1 && focusCol == col);
    // cuadricula 7x6
    for (int row = 0; row < NS_ROWS; row++)
        for (int col = 0; col < NS_COLS; col++)
            SymbolCell(cell, borderGS + col * (celW + gapGS), gridY + row * (celH + gapGS), celW, celH, textY,
                      kSymbols[row][col], focusRow == row && focusCol == col);
    // barra de abajo: "aceptar" (softkey IZQUIERDO) a la izq, "cancelar" (softkey DERECHO) a la der. '*' tambien cierra.
    { const float* bla = ColorList[static_cast<int>(ColorID::white)];   // (en blanco, como los textos de los botones)
      int barTY = barY + (celH - RowHeightGS) / 2;
      w3dEngine::Color4f(bla[0], bla[1], bla[2], 1.0f);
      w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(borderGS + gapGS), (GLfloat)barTY, 0);
      RenderBitmapText(std::string(T("Accept")), textAlign::left, popUpWindow->width / 2); w3dEngine::PopMatrix();
      w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(popUpWindow->width - borderGS - gapGS), (GLfloat)barTY, 0);
      RenderBitmapText(std::string(T("Cancel")), textAlign::right, popUpWindow->width / 2); w3dEngine::PopMatrix(); }
    endView();
}

void SymbolsPopup::Pick(const char* s){
    SymbolInsert(s);
    RecentPush(s);
    g_redraw = true;   // queda ABIERTO para elegir mas simbolos; C/soft-izq cierra
}

bool SymbolsPopup::Key(int key){
    // Convencion Symbian: soft-IZQ (Cancel, 164) = ACEPTAR (inserta el enfocado y CIERRA); soft-DER (Accept, 165) =
    // CANCELAR (cierra sin insertar). OK del centro (Enter) = inserta y QUEDA abierto (para elegir varios).
    if (key == LayoutKey::Cancel){
        if (focusRow < 0) Pick(gRecent[focusCol].c_str()); else Pick(kSymbols[focusRow][focusCol]);
        Close(); return true;
    }
    if (key == LayoutKey::Accept){ Close(); return true; }
    if (key == LayoutKey::Enter){
        if (focusRow < 0) Pick(gRecent[focusCol].c_str());
        else              Pick(kSymbols[focusRow][focusCol]);
        return true;
    }
    if (key == LayoutKey::Up)   { focusRow--; if (focusRow < -1) focusRow = NS_ROWS - 1; g_redraw = true; return true; }
    if (key == LayoutKey::Down) { focusRow++; if (focusRow > NS_ROWS - 1) focusRow = -1; g_redraw = true; return true; }
    if (key == LayoutKey::Left) { focusCol--;  if (focusCol < 0) focusCol = NS_COLS - 1;     g_redraw = true; return true; }
    if (key == LayoutKey::Right){ focusCol++;  if (focusCol > NS_COLS - 1) focusCol = 0;     g_redraw = true; return true; }
    return true;
}

bool SymbolsPopup::Click(int mx, int my){
    if (!Contains(mx, my)) return false;   // afuera: LayoutClickUI nos cierra
    int lx = mx - x, ly = my - y;
    if (ly >= barY){   // barra de abajo: mitad izq = aceptar (insertar enfocado + cerrar), mitad der = cancelar (cerrar)
        if (lx < popUpWindow->width / 2){
            if (focusRow < 0) Pick(gRecent[focusCol].c_str()); else Pick(kSymbols[focusRow][focusCol]);
        }
        Close(); return true;
    }
    if (ly >= recentY && ly < recentY + celH){
        int col = (lx - borderGS) / (celW + gapGS);
        if (col >= 0 && col < NS_COLS){ focusRow = -1; focusCol = col; Pick(gRecent[col].c_str()); }
        return true;
    }
    if (ly >= gridY){
        int row = (ly - gridY) / (celH + gapGS);
        int col = (lx - borderGS) / (celW + gapGS);
        if (row >= 0 && row < NS_ROWS && col >= 0 && col < NS_COLS){ focusRow = row; focusCol = col; Pick(kSymbols[row][col]); }
        return true;
    }
    return true;
}

void SymbolsOpen(){
    if (!gSymbols) gSymbols = new SymbolsPopup();
    gSymbols->focusRow = 0; gSymbols->focusCol = 0;
    PopUpActive = gSymbols;
    g_redraw = true;
}
bool SymbolsActive(){ return (gSymbols && PopUpActive == gSymbols); }
void SymbolsClose(){ if (SymbolsActive()) gSymbols->Close(); }
