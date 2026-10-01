#ifndef CAMBIOSPOPUP_H
#define CAMBIOSPOPUP_H
// ============================================================================
//  CambiosPopup — el cartel "Se perderan los cambios en:" (io/CambiosProyecto.h):
//  aparece al CERRAR Whisk3D, o al abrir / crear otro proyecto, con algo sin
//  guardar. Una LISTA SCROLLEABLE (icono + tipo + nombre de cada cosa editada) y
//  tres botones: "Guardar y salir" (o "... y abrir"), "Salir sin guardar" y
//  "Cancelar". Teclado (flechas / Enter / Esc, el keypad del N95: izquierda,
//  derecha, OK, C), mouse, rueda y tactil (arrastrar la lista = scroll).
//  Compartido 4 OS. Motor generico.
// ============================================================================
#include "PopUpBase.h"
#include "io/CambiosProyecto.h"
#include <string>
#include <vector>

class Button;

class CambiosPopup : public PopUpBase {
    public:
        int accion;                        // W3D_CAMBIOS_SALIR / ABRIR / NUEVO
        std::vector<W3dCambio> cambios;    // la lista (se arma al abrir)
        Button* btn[3];                    // 0 guardar y seguir, 1 seguir sin guardar, 2 cancelar
        int foco;                          // el boton resaltado por teclado
        int scroll;                        // primera fila visible de la lista
        int filasVisibles;
        int listaX, listaY, listaW, listaH; // el area de la lista (absoluta)
        bool arrastrando; int arrastreY0, scroll0;

        CambiosPopup();
        ~CambiosPopup();
        void Abrir(int accion);
        void Layout();
        void Render();
        bool Click(int mx, int my);
        bool Motion(int mx, int my);
        bool Tecla(int tecla);
        void Wheel(int delta);
        void Soltar();
        bool Arrastrando() { return arrastrando; }
        void Responder(int r);
};

extern CambiosPopup* cambiosPopup;
void CambiosPopupAbrir(int accion);

#endif
