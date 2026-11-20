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
#include "PanelDesplazable.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
class Card;

void TimelineRedoAbrir();          // Timeline::DopeMoveConfirm, si dejo un paso de undo
void TimelineRedoCerrarPanel();
bool TimelineRedoPanelAbierto();
// (harness) tocar un valor como en la tarjeta (campo 0/1 = el primero / segundo valor) y aceptar o cancelar
bool TimelineRedoFijar(int campo, float valor);
void TimelineRedoBoton(bool aceptar);

class TimelineRedoPanel : public PopUpBase {
    public:
        TimelineRedoPanel();
        ~TimelineRedoPanel();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Tecla(int tecla) W3D_OVERRIDE;
        void Soltar() W3D_OVERRIDE;
        bool Arrastrando() W3D_OVERRIDE;
        bool CierraConViewport() W3D_OVERRIDE { return true; } bool Plegable() W3D_OVERRIDE { return grupo != 0; } bool Plegado() W3D_OVERRIDE { return grupo && !grupo->open; } void Desplegar(bool abrir) W3D_OVERRIDE { if (grupo) grupo->open = abrir; }   // click afuera = aceptar (y el click sigue)
        void Wheel(int delta) W3D_OVERRIDE;
        void Cerrar() W3D_OVERRIDE;

        GroupPropertie* grupo;
        class PropFloat* dragField; bool dragMoved; int lastDragMx;
        Card* marco;
        PanelDesplazable dz;
        void ClickFila(int mx, int my, bool soloArmar);
        void Armar();
        void ResizeGrupo();
        void Reubicar();
        int  FilaY(int idx) const;
};
#endif // TIMELINEREDOPANEL_H
