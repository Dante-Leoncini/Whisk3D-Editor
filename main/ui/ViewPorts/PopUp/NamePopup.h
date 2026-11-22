#ifndef NAME_POPUP_H
#define NAME_POPUP_H
// ============================================================================
//  NamePopup.h - una ventanita que PIDE UN NOMBRE: un titulo, el campo "Nombre" (ya enfocado: se escribe directo; en
//  tactil sale el teclado) y los botones Cancelar / <accion>. La accion recibe lo escrito (sin espacios en las puntas;
//  vacio no se acepta: el campo se pone en rojo). Enter acepta, Esc o un toque afuera cancelan.
//  Lo usa "Guardar layout" (la esquina de un viewport -> Layout).
// ============================================================================
#include "PopUpBase.h"
#include <string>
class GroupPropertie; class PropText; class PropButtonRow;

class NamePopup : public PopUpBase {
    public:
        NamePopup(const std::string& title, const std::string& button, const std::string& initial,
                    void (*accept)(const std::string& name));
        ~NamePopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;
        bool KeyInText(bool enter) W3D_OVERRIDE;   // Enter acepta y Esc cancela, aunque se este escribiendo
        void Accept();              // lo que hace el boton de la accion (y Enter)
        GroupPropertie* group;
        PropText* pName;
        PropButtonRow* pButtons;
        void (*onAccept)(const std::string& name);
        void Relocate();
};
void OpenNamePopup(const std::string& title, const std::string& button, const std::string& initial,
                      void (*accept)(const std::string& name));
extern NamePopup* namePopup;   // (harness) el abierto, o NULL

#endif // NAME_POPUP_H
