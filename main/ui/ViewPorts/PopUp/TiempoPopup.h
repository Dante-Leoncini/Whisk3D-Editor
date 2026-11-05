#ifndef TIEMPOPOPUP_H
#define TIEMPOPOPUP_H
// ============================================================================
//  TiempoPopup.h - el RELOJ de la barra del timeline: frame actual / Inicio / Fin / FPS en un desplegable, en vez
//  de tres botones que se comian la barra (en el N95 no entraban). Los campos son los MISMOS PropFloat del timeline
//  (los aplica su onChange), asi que editar aca o tipear en la tarjeta de la escena es lo mismo.
//  Se abre DEBAJO del boton del reloj; un click afuera lo cierra (lo tipeado ya quedo).
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"

class TiempoPopup : public PopUpBase {
    public:
        TiempoPopup(PropFloat* actual, PropFloat* inicio, PropFloat* fin, PropFloat* fps, int ax, int ay);
        ~TiempoPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Tecla(int tecla) W3D_OVERRIDE;
        void Soltar() W3D_OVERRIDE;
        bool Arrastrando() W3D_OVERRIDE;
        void Cerrar() W3D_OVERRIDE;

        GroupPropertie* grupo;      // NO es duenio de los PropFloat (son del timeline)
        PropFloat* dragField; bool dragMoved; int lastDragMx;
        int anclaX, anclaY;         // abajo-izquierda del boton del reloj (pantalla)
        void Reubicar();
};
// abre el desplegable debajo de (ax, ay): Current Frame / Start / End / FPS. fin = NULL lo oculta (modo juego: la
// animacion no termina)
void AbrirTiempoPopup(PropFloat* actual, PropFloat* inicio, PropFloat* fin, PropFloat* fps, int ax, int ay);
bool TiempoPopupAbierto();
#endif // TIEMPOPOPUP_H
