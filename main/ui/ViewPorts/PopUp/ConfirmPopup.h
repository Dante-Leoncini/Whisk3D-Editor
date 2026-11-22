#ifndef CONFIRM_POPUP_H
#define CONFIRM_POPUP_H

#include "PopUpBase.h"
#include <string>
#include <vector>

class Button;

// Popup de CONFIRMACION (estilo soft-keys de Symbian): una BARRA al fondo de la pantalla, ancho COMPLETO,
// con el mensaje arriba y dos botones abajo -> "No" a la IZQUIERDA, "Si" a la DERECHA (el de borrar en ROJO).
// Simula la UI de Symbian (los soft keys estan abajo). Compartido 4 OS. Hoy lo usa el borrado de objetos.
class ConfirmPopup : public PopUpBase {
    public:
        std::string message;
        std::vector<std::string> lines; // mensaje partido en renglones (word-wrap al ancho disponible)
        void (*onYes)();   // callback al confirmar (Si)
        void (*onNo)();    // callback al decir que no (el boton No o Esc; cerrarlo de otra forma no responde)
        Button* btnYes;    // "Si" (rojo, derecha) = soft key derecho / Accept / flecha derecha
        Button* btnNo;     // "No" (izquierda)     = soft key izquierdo / Cancel / flecha izquierda
        int focus;         // 0 = No, 1 = Si (resaltado por teclado)

        ConfirmPopup();
        ~ConfirmPopup();

        // arma la barra inferior (ancho completo) con el mensaje + callback de Si
        void Open(const std::string& msg, void (*cb)());
        // una PREGUNTA (no un borrado): los dos botones con su texto, el Si sin el rojo y una respuesta para el No
        // (el boton o Esc; un click afuera la cierra sin responder)
        void Ask(const std::string& msg, const std::string& yes, const std::string& no, void (*cbYes)(),
                       void (*cbNo)());
        void RespondNo();   // el No: cierra y avisa
        void Layout(); // recalcula posicion/tamaño (ancho completo, abajo) con las metricas actuales
        int Padding() const;   // padding UNIFORME de la ventana (4 lados)
        int LineHeight() const; // alto de un renglon de mensaje
        int BarHeight() const; // alto total (pad + N renglones de mensaje + gap + botones + pad)
        void Render();
        bool Click(int mx, int my);
        bool Motion(int mx, int my); // actualiza el hover de los botones (iluminacion al pasar el mouse)
        bool Key(int key); // LayoutKey: Accept/flecha-der = Si ; Cancel/flecha-izq = No
};

extern ConfirmPopup* confirmPopup;

// arma el mensaje "Borrar \"XXX\"?" (o "Borrar N objetos?") y abre el popup; Si -> borra (con undo). Object mode.
void OpenConfirmDelete(bool includeCollections = false);

#endif // CONFIRM_POPUP_H
