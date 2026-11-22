#ifndef CHANGES_POPUP_H
#define CHANGES_POPUP_H
// ============================================================================
//  ChangesPopup — el cartel "Se perderan los cambios en:" (io/ProjectChanges.h):
//  aparece al CERRAR Whisk3D, o al abrir / crear otro proyecto, con algo sin
//  guardar. Una LISTA SCROLLEABLE (icono + tipo + nombre de cada cosa editada) y
//  tres botones: "Guardar y salir" (o "... y abrir"), "Salir sin guardar" y
//  "Cancelar". Teclado (flechas / Enter / Esc, el keypad del N95: izquierda,
//  derecha, OK, C), mouse, rueda y tactil (arrastrar la lista = scroll).
//  Compartido 4 OS. Motor generico.
// ============================================================================
#include "PopUpBase.h"
#include "io/ProjectChanges.h"
#include <string>
#include <vector>

class Button;

class ChangesPopup : public PopUpBase {
    public:
        int action;                        // W3D_CHANGES_EXIT / ABRIR / NUEVO
        std::vector<W3dChange> changes;    // la lista (se arma al abrir)
        Button* btn[3];                    // 0 guardar y seguir, 1 seguir sin guardar, 2 cancelar
        int focus;                         // el boton resaltado por teclado
        int scroll;                        // primera fila visible de la lista
        int visibleRows;
        int listX, listY, listW, listH; // el area de la lista (absoluta)
        bool dragging; int dragY0, scroll0;

        ChangesPopup();
        ~ChangesPopup();
        void Open(int action);
        void Layout();
        void Render();
        bool Click(int mx, int my);
        bool Motion(int mx, int my);
        bool Key(int key);
        void Wheel(int delta);
        void Release();
        bool Dragging() { return dragging; }
        void Respond(int r);
};

extern ChangesPopup* changesPopup;
void ChangesPopupOpen(int action);

#endif
