#ifndef NUEVATEXTURAPOPUP_H
#define NUEVATEXTURAPOPUP_H
// ============================================================================
//  NuevaTexturaPopup.h - "Nueva textura" del UV editor: nombre, tamano, color, alpha y tipo
//  (liso / grilla UV / grilla de colores). Crear genera el PNG, lo mete en el proyecto y se lo
//  asigna al material de la parte activa (io/TexturaGenerada.h).
//  El formulario es SIEMPRE el popup activo: el selector de color y el desplegable del tipo se
//  dibujan encima y reciben los eventos a traves de el (asi ningun click "de afuera" lo cierra).
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
class PropText; class PropColor; class PropBool; class PropButton; class PropButtonRow; class Card;

class NuevaTexturaPopup : public PopUpBase {
    public:
        NuevaTexturaPopup();
        ~NuevaTexturaPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Tecla(int tecla) W3D_OVERRIDE;
        bool TeclaRepeat(int tecla) W3D_OVERRIDE;
        void Wheel(int delta) W3D_OVERRIDE;
        void Soltar() W3D_OVERRIDE;
        bool Arrastrando() W3D_OVERRIDE;
        void Cerrar() W3D_OVERRIDE;

        GroupPropertie* grupo;
        PropText* pNombre; PropFloat* pAncho; PropFloat* pAlto; PropColor* pColor; PropBool* pAlpha;
        PropButton* pTipo; PropButtonRow* pBotones;
        PropFloat* dragField; bool dragMoved; int lastDragMx;
        bool pickerAbierto;     // el selector de color esta abierto encima (se le reenvian los eventos)
        bool listaAbierta;      // el desplegable del tipo esta abierto
        int  listaSel;          // opcion resaltada del desplegable (teclado)
        Card* lista;            // la tarjeta del desplegable
        void ResizeGrupo();
        void Reubicar();
        int  FilaY(int idx) const;      // y local del renglon idx de la tarjeta
        void ListaRect(int& lx, int& ly, int& lw, int& lh) const; // rect local del desplegable
        void AbrirLista();
};
void AbrirNuevaTexturaPopup();
#endif // NUEVATEXTURAPOPUP_H
