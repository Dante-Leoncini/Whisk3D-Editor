#ifndef ATAJOSPOPUP_H
#define ATAJOSPOPUP_H
// ============================================================================
//  AtajosPopup.h - la ventana "Keyboard Shortcuts" (Properties > Settings): la lista de funciones de un contexto
//  con sus combinaciones (app/W3dAtajos.h). Se elige una fila y "Add shortcut" espera la proxima combinacion que se
//  apriete; "Clear" la deja sin atajo y "Reset" vuelve a la de fabrica. Cada cambio se guarda en atajos.ini.
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
class PropButton; class PropButtonRow; class Card;

class AtajosPopup : public PopUpBase {
    public:
        AtajosPopup();
        ~AtajosPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Tecla(int tecla) W3D_OVERRIDE;
        void Wheel(int delta) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        void Cerrar() W3D_OVERRIDE;

        GroupPropertie* grupo;
        PropButton* pVista; PropButton* pContexto; PropButtonRow* pFila; PropButtonRow* pGeneral;
        Card* lista;
        class PopupMenu* desp;    // el desplegable abierto (Viewport o Contexto): lo dibuja y lo maneja la ventana
        int despQue;              // 0 = viewport, 1 = contexto
        int vista;                // el viewport que se muestra (indice de la tabla de AtajosPopup.cpp)
        int ctx;                  // W3dAtajoCtx que se muestra (uno de los contextos de ese viewport)
        int sel;                  // fila elegida (indice en 'filas')
        int scroll;               // primera fila visible
        std::vector<int> filas;   // indices de W3dAtajoInfo del contexto

        void ArmarFilas();
        void CambiarContexto(int paso);   // el viewport anterior / siguiente (flechas izq/der)
        void ElegirVista(int v);
        void ElegirContexto(int c);
        void AbrirDesplegable(int que);
        void AsegurarVisible();
        int  Seleccionada() const;    // indice global de la funcion elegida (-1 = ninguna)
        int  Visibles() const;
        void ListaRect(int& lx, int& ly, int& lw, int& lh) const;
        void ResizeGrupo();
        void Reubicar();
};
void AbrirAtajosPopup();
bool AtajosPopupAbierto();
AtajosPopup* AtajosPopupActual();
#endif // ATAJOSPOPUP_H
