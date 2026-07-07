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
        NumPad();

        void Render() override;
        bool Click(int mx, int my) override;
        bool Tecla(int tecla) override; // Enter fisico = Aceptar, Esc = Cancelar
        void Cerrar() override;         // click AFUERA = commit (igual que la edicion inline)

    private:
        Card* keyCard;               // tarjeta reusada para dibujar cada tecla
        int keyW, keyH, dispH;       // medidas calculadas en Reubicar()
        void Reubicar();             // ancho de ventana, pegado al borde inferior
        void AccionTecla(int fila, int col);
        void Aceptar();              // evalua la expresion -> commit -> cierra
        void Cancelar();             // descarta -> cierra
};

// abre el teclado (reemplaza la instancia anterior) para el PropFloat en edicion
void NumPadAbrir();

#endif // NUMPAD_H
