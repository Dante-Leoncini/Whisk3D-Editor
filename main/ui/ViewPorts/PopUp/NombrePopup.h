#ifndef NOMBREPOPUP_H
#define NOMBREPOPUP_H
// ============================================================================
//  NombrePopup.h - una ventanita que PIDE UN NOMBRE: un titulo, el campo "Nombre" (ya enfocado: se escribe directo; en
//  tactil sale el teclado) y los botones Cancelar / <accion>. La accion recibe lo escrito (sin espacios en las puntas;
//  vacio no se acepta: el campo se pone en rojo). Enter acepta, Esc o un toque afuera cancelan.
//  Lo usa "Guardar layout" (la esquina de un viewport -> Layout).
// ============================================================================
#include "PopUpBase.h"
#include <string>
class GroupPropertie; class PropText; class PropButtonRow;

class NombrePopup : public PopUpBase {
    public:
        NombrePopup(const std::string& titulo, const std::string& boton, const std::string& inicial,
                    void (*aceptar)(const std::string& nombre));
        ~NombrePopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Tecla(int tecla) W3D_OVERRIDE;
        void Cerrar() W3D_OVERRIDE;
        bool TeclaEnTexto(bool enter) W3D_OVERRIDE;   // Enter acepta y Esc cancela, aunque se este escribiendo
        void Aceptar();              // lo que hace el boton de la accion (y Enter)
        GroupPropertie* grupo;
        PropText* pNombre;
        PropButtonRow* pBotones;
        void (*alAceptar)(const std::string& nombre);
        void Reubicar();
};
void AbrirNombrePopup(const std::string& titulo, const std::string& boton, const std::string& inicial,
                      void (*aceptar)(const std::string& nombre));
extern NombrePopup* nombrePopup;   // (harness) el abierto, o NULL

#endif // NOMBREPOPUP_H
