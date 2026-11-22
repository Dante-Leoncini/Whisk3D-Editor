#ifndef NEW_TEXTURE_POPUP_H
#define NEW_TEXTURE_POPUP_H
// ============================================================================
//  NewTexturePopup.h - "Nueva textura" del UV editor: nombre, tamano, color, alpha y tipo
//  (liso / grilla UV / grilla de colores). Crear genera el PNG, lo mete en el proyecto y se lo
//  asigna al material de la parte activa (io/GeneratedTexture.h).
//  El formulario es SIEMPRE el popup activo: el selector de color y el desplegable del tipo se
//  dibujan encima y reciben los eventos a traves de el (asi ningun click "de afuera" lo cierra).
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
class PropText; class PropColor; class PropBool; class PropButton; class PropButtonRow; class Card;

class NewTexturePopup : public PopUpBase {
    public:
        NewTexturePopup();
        ~NewTexturePopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        bool KeyRepeat(int key) W3D_OVERRIDE;
        void Wheel(int delta) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        bool Dragging() W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        GroupPropertie* group;
        PropText* pName; PropFloat* pWidth; PropFloat* pHeight; PropColor* pColor; PropBool* pAlpha;
        PropButton* pType; PropButtonRow* pButtons;
        PropFloat* dragField; bool dragMoved; int lastDragMx;
        bool pickerOpen;     // el selector de color esta abierto encima (se le reenvian los eventos)
        bool listOpen;       // el desplegable del tipo esta abierto
        int  listSel;        // opcion resaltada del desplegable (teclado)
        Card* list;          // la tarjeta del desplegable
        void ResizeGroup();
        void Relocate();
        int  RowY(int idx) const;      // y local del renglon idx de la tarjeta
        void ListRect(int& lx, int& ly, int& lw, int& lh) const; // rect local del desplegable
        void OpenList();
};
void OpenNewTexturePopup();
#endif // NEW_TEXTURE_POPUP_H
