#ifndef CONFIRMARPOPUP_H
#define CONFIRMARPOPUP_H

#include "PopUpBase.h"
#include <string>

class Button;

// Popup de CONFIRMACION (estilo soft-keys de Symbian): una BARRA al fondo de la pantalla, ancho COMPLETO,
// con el mensaje arriba y dos botones abajo -> "No" a la IZQUIERDA, "Si" a la DERECHA (el de borrar en ROJO).
// Simula la UI de Symbian (los soft keys estan abajo). Compartido 4 OS. Hoy lo usa el borrado de objetos.
class ConfirmarPopup : public PopUpBase {
    public:
        std::string mensaje;
        void (*onSi)();   // callback al confirmar (Si)
        Button* btnSi;    // "Si" (rojo, derecha) = soft key derecho / Accept / flecha derecha
        Button* btnNo;    // "No" (izquierda)     = soft key izquierdo / Cancel / flecha izquierda
        int foco;         // 0 = No, 1 = Si (resaltado por teclado)

        ConfirmarPopup();
        ~ConfirmarPopup();

        // arma la barra inferior (ancho completo) con el mensaje + callback de Si
        void Abrir(const std::string& msg, void (*cb)());
        void Layout(); // recalcula posicion/tamaño (ancho completo, abajo) con las metricas actuales
        int Padding() const;   // padding UNIFORME de la ventana (4 lados)
        int AltoBarra() const; // alto total (pad + mensaje + gap + botones + pad)
        void Render();
        bool Click(int mx, int my);
        bool Motion(int mx, int my); // actualiza el hover de los botones (iluminacion al pasar el mouse)
        bool Tecla(int tecla); // LayoutKey: Accept/flecha-der = Si ; Cancel/flecha-izq = No
};

extern ConfirmarPopup* confirmarPopup;

// arma el mensaje "Borrar \"XXX\"?" (o "Borrar N objetos?") y abre el popup; Si -> borra (con undo). Object mode.
void AbrirConfirmarBorrado();

#endif // CONFIRMARPOPUP_H
