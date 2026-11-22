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
#include "ScrollablePanel.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
class Card;

void TimelineRedoOpen();          // Timeline::DopeMoveConfirm, si dejo un paso de undo
void TimelineRedoClosePanel();
bool TimelineRedoPanelOpen();
// (harness) tocar un valor como en la tarjeta (campo 0/1 = el primero / segundo valor) y aceptar o cancelar
bool TimelineRedoSet(int field, float value);
void TimelineRedoButton(bool accept);

class TimelineRedoPanel : public PopUpBase {
    public:
        TimelineRedoPanel();
        ~TimelineRedoPanel();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        bool Dragging() W3D_OVERRIDE;
        bool ClosesWithViewport() W3D_OVERRIDE { return true; } bool Collapsible() W3D_OVERRIDE { return group != 0; } bool Collapsed() W3D_OVERRIDE { return group && !group->open; } void Expand(bool open) W3D_OVERRIDE { if (group) group->open = open; }   // click afuera = aceptar (y el click sigue)
        void Wheel(int delta) W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        GroupPropertie* group;
        class PropFloat* dragField; bool dragMoved; int lastDragMx;
        Card* frame;
        ScrollablePanel dz;
        void ClickRow(int mx, int my, bool onlyBuild);
        void Build();
        void ResizeGroup();
        void Relocate();
        int  RowY(int idx) const;
};
#endif // TIMELINEREDOPANEL_H
