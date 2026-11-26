#ifndef TIMELINEREDOPANEL_H
#define TIMELINEREDOPANEL_H
// ============================================================================
//  TimelineRedoPanel.h - "Ajustar la ultima operacion" del TIMELINE (dope sheet y curvas). Es la tarjeta del viewport
//  3D (TransformRedoPanel.h) para los keyframes: despues de mover / rotar / escalar aparece abajo a la izquierda del
//  timeline con lo que se hizo, editable:
//    - Mover: Frames (y Valor en curvas)
//    - Rotar: Angulo (solo curvas)
//    - Escalar: X = tiempo (e Y = valor en curvas): escalar Y a 0 aplana la curva, handles incluidos
//    - el pivote con el que se hizo (para saber desde donde)
//    - abajo: Cancelar (deshace el cambio) | Aceptar. Un click afuera tambien acepta; Esc tambien. Solo Cancelar deshace.
//  Cambiar un valor vuelve a aplicar el transform desde la foto de antes sobre el MISMO paso de undo (Timeline.cpp).
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
class Card;

void TimelineRedoOpen();          // Timeline::DopeMoveConfirm, si dejo un paso de undo
void TimelineRedoClosePanel();
bool TimelineRedoPanelOpen();
// (harness) tocar un valor como en la tarjeta (campo 0/1 = el primero / segundo valor) y aceptar o cancelar
bool TimelineRedoSet(int field, float value);
void TimelineRedoButton(bool accept);

class TimelineRedoPanel : public CardPanel {
    public:
        TimelineRedoPanel();
        ~TimelineRedoPanel();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;
        Card* frame;
        void Build();
        void ResizeGroup();
        void Relocate();
};
#endif // TIMELINEREDOPANEL_H
