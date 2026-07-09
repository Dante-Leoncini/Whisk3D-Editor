#ifndef NUMPAD_H
#define NUMPAD_H

#include "PopUpBase.h"

// ============================================================================
//  TECLADO NUMERICO de Whisk3D (tactil): popup al ANCHO COMPLETO de la ventana,
//  pegado abajo. Edita el PropFloat activo (g_propFloatEditando): 0-9, ".", los
//  operadores ( ) + - * / (la expresion se evalua al Aceptar), retroceso,
//  Cancelar y Aceptar (verde). Solo campos NUMERICOS; texto usa otro camino.
//  Se abre desde Properties cuando el tap fue TACTIL (en PC con mouse y en
//  Symbian la edicion inline sigue igual).
// ============================================================================
class NumPad : public PopUpBase {
    public:
        NumPad(bool transform = false); // transform=true: edita el transform de la barra (sin display propio)

        void Render() override;
        bool Click(int mx, int my) override;
        bool Tecla(int tecla) override; // Enter fisico = Aceptar, Esc = Cancelar
        void Cerrar() override;         // click AFUERA = commit (float) / deja el transform (transform)

        PopUpBase* prevPopup;        // popup a restaurar al cerrar (ej: el panel redo del loop cut). NULL = ninguno

    private:
        Card* keyCard;               // tarjeta reusada para dibujar cada tecla
        int keyW, keyH, dispH;       // medidas calculadas en Reubicar()
        bool modoTransform;          // true = alimenta NumInput* (barra de estado); false = PropFloat
        void CerrarRestaurando();    // cierra el numpad restaurando prevPopup (en vez de dejar PopUpActive en NULL)
        void Reubicar();             // ancho de ventana, pegado al borde inferior
        void Feed(int c);            // rutea un caracter/backspace al destino segun el modo
        void AccionTecla(int fila, int col);
        void Aceptar();              // evalua la expresion -> commit -> cierra
        void Cancelar();             // descarta -> cierra
};

// abre el teclado (reemplaza la instancia anterior) para el PropFloat en edicion
void NumPadAbrir();
// abre el teclado en modo TRANSFORM: edita directamente el transform de la barra de estado
// (mover/rotar/escalar) como un teclado fisico en PC. Sin popup/textinput propio.
void NumPadAbrirTransform();

#endif // NUMPAD_H
