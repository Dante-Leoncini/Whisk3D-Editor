#ifndef SHORTCUTS_POPUP_H
#define SHORTCUTS_POPUP_H
// ============================================================================
//  ShortcutsPopup.h - la ventana "Keyboard Shortcuts" (Properties > Settings): la lista de funciones de un contexto
//  con sus combinaciones (app/W3dShortcuts.h). Se elige una fila y "Add shortcut" espera la proxima combinacion que se
//  apriete; "Clear" la deja sin atajo y "Reset" vuelve a la de fabrica. Cada cambio se guarda en shortcuts.start.
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
class PropButton; class PropButtonRow; class Card;

class ShortcutsPopup : public PopUpBase {
    public:
        ShortcutsPopup();
        ~ShortcutsPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Wheel(int delta) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        GroupPropertie* group;
        PropButton* pView; PropButton* pContext; PropButtonRow* pRow; PropButtonRow* pGeneral;
        Card* list;
        class PopupMenu* offset;   // el desplegable abierto (Viewport o Contexto): lo dibuja y lo maneja la ventana
        int offsetThat;            // 0 = viewport, 1 = contexto
        int view;                  // el viewport que se muestra (indice de la tabla de ShortcutsPopup.cpp)
        int ctx;                   // W3dShortcutCtx que se muestra (uno de los contextos de ese viewport)
        int sel;                   // fila elegida (indice en 'filas')
        int scroll;                // primera fila visible
        std::vector<int> rows;     // indices de W3dShortcutInfo del contexto

        void BuildRows();
        void ChangeContext(int step);   // el viewport anterior / siguiente (flechas izq/der)
        void PickView(int v);
        void PickContext(int c);
        void OpenDropdown(int that);
        void EnsureVisible();
        int  Selected() const;    // indice global de la funcion elegida (-1 = ninguna)
        int  Visible() const;
        void ListRect(int& lx, int& ly, int& lw, int& lh) const;
        void ResizeGroup();
        void Relocate();
};
void OpenShortcutsPopup();
bool ShortcutsPopupOpen();
ShortcutsPopup* ShortcutsCurrentPopup();
#endif // SHORTCUTS_POPUP_H
