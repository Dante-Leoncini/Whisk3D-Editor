#ifndef UVMAPAPOPUP_H
#define UVMAPAPOPUP_H
// ============================================================================
//  UVMapaPopup.h - la CAPA UV de un mapa del material (el boton de la malla de cada mapa en Properties).
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

class UVMapaPopup : public PopUpBase {
    public:
        UVMapaPopup(const std::string& titulo);
        ~UVMapaPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Tecla(int tecla) W3D_OVERRIDE;
        void Wheel(int delta) W3D_OVERRIDE;
        void Soltar() W3D_OVERRIDE;
        bool Arrastrando() W3D_OVERRIDE;
        void Cerrar() W3D_OVERRIDE;

        GroupPropertie* grupo;
        PropFloat* pIdxMat; PropBool* pUsarMat; PropButton* pUV; PropFloat* pIdxObj; PropButtonRow* pBotones;
        PropFloat* dragField; bool dragMoved; int lastDragMx;
        bool listaAbierta; int listaScroll;   // el desplegable de las capas UV (con scroll: puede haber cientos)
        Card* lista;
        void Sincronizar();                    // que filas se ven (segun el tilde y lo elegido)
        void ResizeGrupo();
        void Reubicar();
        int  FilaY(int idx) const;
        void ListaRect(int& lx, int& ly, int& lw, int& lh) const;
        int  ListaFilas() const;               // cuantas se ven a la vez
        int  ListaTotal() const;               // "Use index" + las capas UV de la malla
};
// abre el popup para el mapa 'mapa' del material 'mat' en la malla 'm' (el objeto activo)
void AbrirUVMapaPopup(Material* mat, Mesh* m, int mapa, const std::string& titulo);
// (harness) lo que haria el popup con estos valores al aceptar: indiceMat 1.., usarMat, uvNombre ("" = "Use index"),
// indiceObj 1..
void UVMapaPopupAplicar(Material* mat, Mesh* m, int mapa, int indiceMat, bool usarMat, const std::string& uvNombre, int indiceObj);
#endif // UVMAPAPOPUP_H
