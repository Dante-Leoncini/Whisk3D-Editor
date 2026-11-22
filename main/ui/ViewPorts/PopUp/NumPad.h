#ifndef NUMPAD_H
#define NUMPAD_H

#include "PopUpBase.h"
#include <string>
class TextField;

// ============================================================================
//  TECLADO NUMERICO de Whisk3D (tactil): popup al ANCHO COMPLETO de la ventana,
//  pegado abajo. Edita el PropFloat activo (g_propFloatEditing): 0-9, ".", los
//  operadores ( ) + - * / (la expresion se evalua al Aceptar), retroceso,
//  Cancelar y Aceptar (verde). Solo campos NUMERICOS; texto usa otro camino.
//  Se abre desde Properties cuando el tap fue TACTIL (en PC con mouse y en
//  Symbian la edicion inline sigue igual).
// ============================================================================
class NumPad : public PopUpBase {
    public:
        NumPad(bool transform = false); // transform=true: edita el transform de la barra (sin display propio)

        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE; // Enter fisico = Aceptar, Esc = Cancelar
        void Close() W3D_OVERRIDE;      // click AFUERA = commit (float) / deja el transform (transform)
        bool IsKeyboard() const W3D_OVERRIDE { return true; }

        PopUpBase* prevPopup;        // popup a restaurar al cerrar (ej: el panel redo del loop cut). NULL = ninguno
        bool KeyCenter(const char* text, int& cx, int& cy);   // (NumPadKeyCenter)

        // un NUMERO SUELTO (un TextField que no es un PropFloat: el valor de un mapa del material): edita
        // g_activeTextField; Aceptar evalua la expresion y lo DESENFOCA (su dueno aplica al perder el foco)
        TextField* field;
        std::string fieldPrev, fieldLabel;

    private:
        Card* keyCard;              // tarjeta reusada para dibujar cada tecla
        int keyW, keyH, dispH;      // medidas calculadas en Relocate()
        bool transformMode;         // true = alimenta NumInput* (barra de estado); false = PropFloat
        TextField* FieldEdited();   // el TextField que se esta editando (el del PropFloat o el campo suelto)
        void CloseRestoring();      // cierra el numpad restaurando prevPopup (en vez de dejar PopUpActive en NULL)
        void Relocate();            // ancho de ventana, pegado al borde inferior
        void Feed(int c);           // rutea un caracter/backspace al destino segun el modo
        void ActionKey(int rowIdx, int col);
        void Accept();             // evalua la expresion -> commit -> cierra
        void Cancel();             // descarta -> cierra
};

// abre el teclado (reemplaza la instancia anterior) para el PropFloat en edicion
void NumPadOpen();
// harness (las capturas de los tutoriales): el centro EN PANTALLA de la tecla con ese texto ("7", "*", "OK"...) del
// teclado abierto. false = no hay teclado abierto o no tiene esa tecla
bool NumPadKeyCenter(const char* text, int& cx, int& cy);
// abre el teclado en modo TRANSFORM: edita directamente el transform de la barra de estado
// (mover/rotar/escalar) como un teclado fisico en PC. Sin popup/textinput propio.
void NumPadOpenTransform();
// abre el teclado numerico para el TextField ENFOCADO (g_activeTextField) que es un numero suelto; 'etiqueta' va a la
// izquierda del valor
void NumPadOpenField(const std::string& label);

// ============================================================================
//  TECLADO QWERTY (tactil, español -> con ñ): al ANCHO COMPLETO, pegado abajo.
//  Edita el campo de texto enfocado (g_activeTextField) igual que el teclado
//  fisico. Se abre desde Properties al tocar un campo de TEXTO en tactil (Android).
// ============================================================================
class IDE;
class QwertyPad : public PopUpBase {
    public:
        QwertyPad();
        IDE* ide;                       // != NULL: escribe en el EDITOR DE SCRIPTS (no hay TextField; Enter = salto)
        // con el IDE es NO MODAL (como Buscar): tocar el codigo mueve el cursor, elige palabras, la lupa... con el teclado
        // abierto (si no, el primer toque afuera solo lo cerraba)
        bool NoModal() W3D_OVERRIDE { return ide != NULL; }
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE; // Enter = Aceptar, Esc = Cancelar
        void Close() W3D_OVERRIDE;      // click AFUERA = commitea el texto
        bool IsKeyboard() const W3D_OVERRIDE { return true; }
        PopUpBase* prevPopup;           // popup a restaurar al cerrar (NULL = ninguno)
        int Height() { Relocate(); return popUpWindow->height; }   // (el alto del teclado: el IDE va arriba)
    private:
        Card* keyCard;
        int   keyH, dispH;
        bool  caps;                     // mayusculas (toggle): letras -> A-Z, ñ -> Ñ
        void Relocate();
        void FeedStr(const char* s);    // inserta una cadena UTF-8 en el campo activo (soporta ñ = 2 bytes)
        void ActionKey(int rowIdx, int col);
        void Accept();
        void Cancel();
};
// abre el teclado QWERTY para el campo de texto enfocado (g_activeTextField)
void QwertyOpen();
// ...o para escribir codigo en el IDE
void QwertyOpenIDE(IDE* ide);

// ============================================================================
//  EL TECLADO EN PANTALLA, SIEMPRE EL MISMO. Todos los campos piden su teclado por aca y Ajustes > "Use system
//  keyboard" decide: destildado (default) = los de Whisk3D (el QWERTY para texto, el numerico para numeros);
//  tildado = el del sistema. Antes cada campo elegia por su cuenta.
// ============================================================================
void KeyboardTextOpen();                                 // el TextField enfocado (g_activeTextField)
void KeyboardNumberOpen();                               // el PropFloat en edicion (g_propFloatEditing)
void KeyboardNumberFieldOpen(const std::string& label);   // un numero suelto enfocado (NumPadOpenField)
void KeyboardIDEOpen(IDE* ide);                          // el editor de scripts
// en cada cuadro, despues de dibujar los popups: EL CAMPO QUE SE EDITA ARRIBA DEL TECLADO DEL SISTEMA (el de Android
// tapa media pantalla y el campo puede quedar debajo o en un panel que no se ve)
void KeyboardSystemFieldRender();
// en cada cuadro, antes de dibujar: si el teclado de Whisk3D del IDE se fue por otro camino, el IDE vuelve a su lugar
void KeyboardsWatch();

// ============================================================================
//  SELECTOR DE SIMBOLOS (keypad N95, sin teclado): popup con titulo, una fila con los
//  ULTIMOS 7 simbolos usados y una cuadricula 7x6 de simbolos + espacio + ENTER (nueva
//  linea, para el IDE). Se abre con '*' mientras se edita texto (T9). Flechas navegan,
//  OK inserta (queda abierto para elegir mas), C/soft-izq cierra.
// ============================================================================
class SymbolsPopup : public PopUpBase {
    public:
        SymbolsPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        int  focusRow, focusCol;   // -1 = fila de recientes; 0..5 = grid. Publico: SymbolsOpen lo resetea.
    private:
        Card* cell;
        int celW, celH, recentY, gridY, barY;   // barY = fila de "aceptar"/"cancelar" abajo
        void Relocate();
        void Pick(const char* s);
};
// abre el selector de simbolos (lo llama el container con '*' durante la edicion de texto)
void SymbolsOpen();
bool SymbolsActive();   // el selector de simbolos esta abierto? ('*' lo cierra desde el container)
void SymbolsClose();

#endif // NUMPAD_H
