#ifndef TIME_POPUP_H
#define TIME_POPUP_H
// ============================================================================
//  TimePopup.h - el RELOJ de la barra del timeline: frame actual / Inicio / Fin / FPS en un desplegable, en vez
//  de tres botones que se comian la barra (en el N95 no entraban). Los campos son los MISMOS PropFloat del timeline
//  (los aplica su onChange), asi que editar aca o tipear en la tarjeta de la escena es lo mismo.
//  Se abre DEBAJO del boton del reloj; un click afuera lo cierra (lo tipeado ya quedo).
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"

class TimePopup : public PopUpBase {
    public:
        TimePopup(PropFloat* current, PropFloat* start, PropFloat* end, PropFloat* fps, int ax, int ay);
        ~TimePopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        bool Dragging() W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        GroupPropertie* group;      // NO es duenio de los PropFloat (son del timeline)
        PropFloat* dragField; bool dragMoved; int lastDragMx;
        int anchorX, anchorY;         // abajo-izquierda del boton del reloj (pantalla)
        void Relocate();
};
// abre el desplegable debajo de (ax, ay): Current Frame / Start / End / FPS. fin = NULL lo oculta (modo juego: la
// animacion no termina)
void OpenTimePopup(PropFloat* current, PropFloat* start, PropFloat* end, PropFloat* fps, int ax, int ay);
bool TimePopupOpen();
#endif // TIME_POPUP_H
