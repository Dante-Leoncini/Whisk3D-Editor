#ifndef UV_MAP_POPUP_H
#define UV_MAP_POPUP_H
// ============================================================================
//  UVMapPopup.h - la CAPA UV de un mapa del material (el boton de la malla de cada mapa en Properties).
//   - "Material index": el indice UV del MATERIAL (lo comparten todos los objetos que lo usan: el de siempre).
//     Se escribe o se arrastra (puede haber cientos de capas UV: un menu no alcanza).
//   - "Use the material's UV index" (tildado por defecto): ESTE objeto usa el indice del material. Destildado, esta
//     malla con este material elige lo suyo: su propio indice ("Use index" + "Object index") o una capa UV por nombre.
//   - Cancel / Accept (todo junto en UN paso de undo).
//  Como el de "Nueva textura": una tarjeta de propiedades flotante, con su desplegable propio.
// ============================================================================
#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
class PropBool; class PropButton; class PropButtonRow; class Card; class Material; class Mesh;

class UVMapPopup : public PopUpBase {
    public:
        UVMapPopup(const std::string& title);
        ~UVMapPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Wheel(int delta) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        bool Dragging() W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        GroupPropertie* group;
        PropFloat* pIdxMat; PropBool* pUseMat; PropButton* pUV; PropFloat* pIdxObj; PropButtonRow* pButtons;
        PropFloat* dragField; bool dragMoved; int lastDragMx;
        bool listOpen; int listScroll;   // el desplegable de las capas UV (con scroll: puede haber cientos)
        Card* list;
        void Sync();                    // que filas se ven (segun el tilde y lo elegido)
        void ResizeGroup();
        void Relocate();
        int  RowY(int idx) const;
        void ListRect(int& lx, int& ly, int& lw, int& lh) const;
        int  RowList() const;                 // cuantas se ven a la vez
        int  ListTotal() const;               // "Use index" + las capas UV de la malla
};
// abre el popup para el mapa 'mapa' del material 'mat' en la malla 'm' (el objeto activo)
void OpenUVMapPopup(Material* mat, Mesh* m, int map, const std::string& title);
// (harness) lo que haria el popup con estos valores al aceptar: matIndex 1.., useMat, uvName ("" = "Use index"),
// objIndex 1..
void UVMapPopupApply(Material* mat, Mesh* m, int map, int matIndex, bool useMat, const std::string& uvName, int objIndex);
#endif // UV_MAP_POPUP_H
