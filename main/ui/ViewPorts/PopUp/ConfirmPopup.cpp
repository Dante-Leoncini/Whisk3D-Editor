#include "w3dGraphics.h"            // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "ConfirmPopup.h"
#include "ViewPorts/LayoutInput.h"  // LayoutKey
#include "WhiskUI/core/UI.h"             // metricas (RowHeightGS/borderGS/marginGS/gapGS/...) + RenderBitmapText
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/widgets/Button.h"
#include "objects/Textures.h"
#include "objects/ObjectMode.h"     // Eliminar
#include "variables.h"              // InteractionMode, ActiveObj, ObjSelects
#include <cstdio>

ConfirmPopup* confirmPopup = NULL;

static const float RED_DELETE[4] = { 0.92f, 0.26f, 0.26f, 1.0f }; // rojo del boton de borrar (fijo: la paleta no tiene un rojo claro)

ConfirmPopup::ConfirmPopup() : PopUpBase("Confirm") {
    onYes = NULL;
    onNo  = NULL;
    focus = 1; // por defecto resaltado en "Si" (la accion principal)
    btnYes = new Button(T("Yes"));
    btnNo  = new Button(T("No"));
    btnYes->adapt = false; btnYes->centered = true;
    btnNo->adapt  = false; btnNo->centered = true;
    btnYes->textColor = RED_DELETE; // borrar = ROJO
}

ConfirmPopup::~ConfirmPopup() {
    delete btnYes; delete btnNo;
}

void ConfirmPopup::Open(const std::string& msg, void (*cb)()) {
    message = msg;
    onYes = cb;
    onNo  = NULL;
    btnYes->text = T("Yes");
    btnNo->text  = T("No");
    btnYes->textColor = RED_DELETE;
    focus = 1;
    PopUpActive = this; // el layout (ancho/posicion) se calcula en Render con las metricas ACTUALES
}

void ConfirmPopup::Ask(const std::string& msg, const std::string& yes, const std::string& no, void (*cbYes)(),
                               void (*cbNo)()) {
    Open(msg, cbYes);
    onNo = cbNo;
    btnYes->text = yes;
    btnNo->text  = no;
    btnYes->textColor = NULL;
}

void ConfirmPopup::RespondNo() {
    void (*no)() = onNo;
    onNo = NULL;
    Close();
    if (no) no();
}

// parte 's' en renglones de <= maxChars, cortando en un espacio si puede (la fuente es monoespaciada).
// SIN limite de renglones: el mensaje entra completo (por eso el popup crece en alto). Un '\n' corta el
// renglon ahi (el mensaje + el nombre del archivo abajo)
static void WrapParagraph(const std::string& s, int maxChars, std::vector<std::string>& out);
static void WrapMessage(const std::string& s, int maxChars, std::vector<std::string>& out) {
    out.clear();
    size_t from = 0;
    for (;;) {
        const size_t jump = s.find('\n', from);
        WrapParagraph(s.substr(from, jump == std::string::npos ? std::string::npos : jump - from), maxChars, out);
        if (jump == std::string::npos) break;
        from = jump + 1;
    }
    if (out.empty()) out.push_back(s);
}
static void WrapParagraph(const std::string& s, int maxChars, std::vector<std::string>& out) {
    if (maxChars < 1) maxChars = 1;
    size_t pos = 0;
    while (pos < s.size()) {
        while (pos < s.size() && s[pos] == ' ') pos++;      // saltar espacios
        if (pos >= s.size()) break;
        if ((int)(s.size() - pos) <= maxChars) { out.push_back(s.substr(pos)); break; }
        size_t brk = s.rfind(' ', pos + maxChars);           // espacio mas a la derecha que entre
        if (brk != std::string::npos && brk > pos) { out.push_back(s.substr(pos, brk - pos)); pos = brk + 1; }
        else { out.push_back(s.substr(pos, maxChars)); pos += maxChars; } // palabra sin espacios: partir
    }
}

// BARRA al fondo, ANCHO COMPLETO de la pantalla (simula los soft keys de Symbian). Se recalcula cada frame
// para adaptarse al tamaño/orientacion actual (y porque al abrir las metricas pueden no estar listas).
// padding UNIFORME en los 4 lados (el contenido = N renglones del mensaje + gap + fila de botones)
int ConfirmPopup::Padding() const { return borderGS + GlobalScale * 3; } // borde + ~3px de aire (antes marginGS+borderGS = demasiado)
int ConfirmPopup::LineHeight() const { return LetterHeightGS + GlobalScale * 3; } // renglon de mensaje (con aire)
int ConfirmPopup::BarHeight()  const {
    int nL = lines.empty() ? 1 : (int)lines.size();       // el mensaje ocupa N renglones (word-wrap)
    return Padding() * 2 + nL * LineHeight() + gapGS + (RowHeightGS + bordersGS);
}

// LA BARRA AL FONDO (ancho completo, sobre los soft keys) es para el telefono SIN pantalla tactil manejado con su
// keypad (el N95): ahi se contesta con los soft keys. Con mouse y teclado (Bluetooth o PC) o con pantalla tactil (el
// N8: la variante ES 2.0 de Symbian es la de Symbian^3) va centrado, igual en todas las plataformas
#if defined(W3D_SYMBIAN) && !defined(W3D_GLES2)
extern int W3dHasMouseBT();          // (w3dmouse.cpp; TBool es int)
extern bool W3dSymbianKeyboardBT();
static bool ConfirmDown() { return !W3dHasMouseBT() && !W3dSymbianKeyboardBT(); }
#else
static bool ConfirmDown() { return false; }
#endif

void ConfirmPopup::Layout() {
    // ancho del dialogo primero (independiente del alto), para saber cuantos caracteres entran por renglon
    const bool down = ConfirmDown();
    int w = down ? MenuScreenW : MenuScreenW / 2;   // abajo: ancho COMPLETO (ahi estan los soft keys)
    if (w < 200) w = MenuScreenW; // pantallas muy chicas: ancho completo
    // word-wrap del mensaje al ancho INTERIOR (w - padding a ambos lados). Asi nunca se corta.
    int cw = (CharacterWidthGS > 0) ? CharacterWidthGS : 1;
    int maxChars = (w - Padding() * 2) / cw;
    WrapMessage(message, maxChars, lines);
    int h = BarHeight(); // usa lineas.size() (ya calculado)
    popUpWindow->Resize(w, h);
    if (down) { x = 0; y = MenuScreenH - h; }
    else { x = (MenuScreenW - w) / 2; y = (MenuScreenH - h) / 2; }
}

void ConfirmPopup::Render() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);

    const float* background = ColorList[static_cast<int>(ColorID::background)];
    const float* white = ColorList[static_cast<int>(ColorID::white)];
    const float* green = ColorList[static_cast<int>(ColorID::accent)];

    // fondo + BORDE VERDE
    w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
    popUpWindow->Render(false);
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    popUpWindow->RenderBorder(false);

    int w = popUpWindow->width;
    int pad = Padding(); // mismo padding arriba/abajo/lados

    // mensaje, centrado, en N renglones (word-wrap) apilados desde arriba
    int lineH = LineHeight();
    int ty = pad;
    for (size_t i = 0; i < lines.size(); i++) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)ty, 0);
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        RenderBitmapText(lines[i].c_str(), textAlign::center, w - pad * 2);
        w3dEngine::PopMatrix();
        ty += lineH;
    }

    // botones: No a la IZQUIERDA, Si a la DERECHA (ancho 50% cada uno), debajo del mensaje
    int left = pad;
    int contW = w - pad * 2;
    int half  = (contW - gapGS) / 2;
    int nL = lines.empty() ? 1 : (int)lines.size();
    int btnY = pad + nL * lineH + gapGS;

    btnNo->Resize(half);
    btnYes->Resize(half); // Si queda ROJO siempre (textColor), sin resaltado verde de foco (es el de borrar)

    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)left, (GLfloat)btnY, 0);
    btnNo->Render();
    w3dEngine::PopMatrix();
    btnNo->sx = x + left; btnNo->sy = y + btnY;

    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(left + half + gapGS), (GLfloat)btnY, 0);
    btnYes->Render();
    w3dEngine::PopMatrix();
    btnYes->sx = x + left + half + gapGS; btnYes->sy = y + btnY;

    endView();
}

bool ConfirmPopup::Motion(int mx, int my) {
    Layout();
    btnNo->hover  = btnNo->Contains(mx, my); // ilumina el boton al pasar el mouse (igual que cualquier boton)
    btnYes->hover = btnYes->Contains(mx, my);
    return true;
}

bool ConfirmPopup::Click(int mx, int my) {
    Layout(); // asegura posicion/tamaño actuales para el hit-test
    if (btnNo->Contains(mx, my)) { RespondNo(); return true; }
    if (btnYes->Contains(mx, my)) { void (*cb)() = onYes; Close(); if (cb) cb(); return true; }
    // click adentro de la barra (sin botón): lo consume (modal); afuera -> el caller cierra
    return Contains(mx, my);
}

bool ConfirmPopup::Key(int key) {
    switch (key) {
        case LayoutKey::Right:   // flecha derecha / soft key derecho = Si
        case LayoutKey::Enter:   // ENTER (teclado PC) = Si: el boton de borrar es el default resaltado
        case LayoutKey::Accept:  // OK / soft key derecho (Symbian) = Si (borrar)
            { void (*cb)() = onYes; Close(); if (cb) cb(); } return true;
        case LayoutKey::Left:    // flecha izquierda / soft key izquierdo = No
        case LayoutKey::Cancel:  // Esc / C = No (cancela)
            RespondNo(); return true;
    }
    return true; // modal: no le roban teclas
}

// ----------------------------------------------------------------

static void ConfirmDeletedYes()    { Remove(false); } // el borrado real (con su undo) al confirmar
static void ConfirmDeletedIfCol() { Remove(true); }   // desde el outliner: incluye colecciones

void OpenConfirmDelete(bool includeCollections) {
    if (InteractionMode != ObjectMode) return;          // Edit Mode tiene su propio menu Delete
    if (!HasObjectsSelected(includeCollections)) return;

    int n = 0; Object* one = NULL;
    for (size_t i = 0; i < ObjSelects.size(); i++)
        if (ObjSelects[i] && ObjSelects[i]->select) { n++; one = ObjSelects[i]; }

    char buf[128];   // snprintf: el nombre lo tipea el usuario (sprintf desbordaba con nombres largos)
    if (n == 1 && one) snprintf(buf, sizeof(buf), T("Delete \"%s\"?"), one->name.c_str());
    else if (n > 1)    snprintf(buf, sizeof(buf), T("Delete %d objects?"), n);
    else               snprintf(buf, sizeof(buf), "%s", T("Delete selected?")); // ej. una coleccion (no esta en ObjSelects)

    if (!confirmPopup) confirmPopup = new ConfirmPopup();
    confirmPopup->Open(buf, includeCollections ? ConfirmDeletedIfCol : ConfirmDeletedYes);
}
