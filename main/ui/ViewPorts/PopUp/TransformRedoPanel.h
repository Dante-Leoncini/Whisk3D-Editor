#ifndef TRANSFORMREDOPANEL_H
#define TRANSFORMREDOPANEL_H
// ============================================================================
//  TransformRedoPanel.h - "Ajustar la ultima operacion" del viewport 3D. Despues de mover / rotar / escalar (con el
//  teclado, el gizmo o el tilde) aparece abajo a la izquierda del viewport, EN LUGAR de la barra de herramientas:
//    - lo que se hizo: Mover X/Y/Z, Rotar angulo (+ eje), Escalar X/Y/Z; todo editable
//    - la orientacion (Global / Local) y la edicion proporcional (prendida, curva, tamano, solo conectados)
//    - abajo: Cancelar (deshace el cambio) | Aceptar. Un click afuera tambien acepta; Esc o Supr tambien (OK sobre un
//      valor lo edita SIN cerrar). Solo Cancelar deshace.
//    - si no entra en la mitad del alto de la pantalla se recorta y se desplaza (dedo, rueda o flechas)
//  Cambiar un valor DESHACE el paso y lo vuelve a hacer con los valores nuevos (queda UN paso de undo).
// ============================================================================
#include "PopUpBase.h"
#include "ScrollablePanel.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
class Viewport3D; class Card;

// Viewport3D::Aceptar: ANTES de confirmar lee lo que se hizo (hace falta el snapshot); DESPUES abre el panel
void TransformRedoCapture(Viewport3D* vp);
void TransformRedoAfterConfirm();
bool TransformRedoOpen(const Viewport3D* vp);
bool TransformRedoReapplying();           // re-haciendo la operacion (Aceptar no vuelve a capturar)
void TransformRedoClose();
extern bool g_transformRedoAuto;          // false = no se abre solo (el harness: las pruebas viejas no lo esperan)
// (harness) tocar un valor como si se editara en el panel: campo 0..2 = X/Y/Z (o 0 = angulo), y aceptar/cancelar
bool TransformRedoSet(int field, float value);
void TransformRedoButton(bool accept);
int  TransformRedoType();                 // translacion / rotacion / EditScale; -1 = cerrado

class TransformRedoPanel : public PopUpBase {
    public:
        TransformRedoPanel();
        ~TransformRedoPanel();
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
        int listOpen;   // 0 = ninguna, 1 = orientacion, 2 = curva proporcional
        int listSel;
        Card* list;
        Card* frame;                 // el borde verde de lo que se ve (el contenido puede estar desplazado)
        ScrollablePanel dz;          // mas alto que media pantalla: recortado y con desplazamiento
        void ClickRow(int mx, int my, bool onlyBuild);
        void Build();
        void ResizeGroup();
        void Relocate();
        int  RowY(int idx) const;
        void ListRect(int& lx, int& ly, int& lw, int& lh) const;
        int  ListCount() const;
};
#endif // TRANSFORMREDOPANEL_H
