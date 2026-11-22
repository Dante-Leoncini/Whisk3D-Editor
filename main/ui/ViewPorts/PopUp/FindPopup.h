#ifndef FIND_POPUP_H
#define FIND_POPUP_H
// ============================================================================
//  FindPopup.h - Buscar / Reemplazar del editor de scripts (IDE). Un panel arriba a la derecha del IDE, del mismo
//  estilo que el de la ultima transformacion del 3D, con borde verde (es la ventana con el foco):
//    Buscar [texto]  ·  "3 de 7"  ·  Mayusculas/minusculas  ·  Palabra completa  ·  [Anterior] [Siguiente]
//    (Reemplazar) Reemplazar [texto]  ·  [Reemplazar] [Reemplazar todo]
//    [Cerrar]
//  No es modal: con el panel abierto se sigue escribiendo en el script (con el puntero sobre el IDE); las coincidencias
//  se resaltan en el texto. Se cierra con Cerrar o Esc. Ctrl+F lo abre para buscar y Ctrl+H para reemplazar.
// ============================================================================
#include "PopUpBase.h"
#include "ScrollablePanel.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include <string>
class IDE; class PropText; class PropLabel; class PropButtonRow; class Card;

class FindPopup : public PopUpBase {
    public:
        FindPopup(IDE* ide, bool replace);
        ~FindPopup();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        bool Dragging() W3D_OVERRIDE;
        void Wheel(int delta) W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;
        bool NoModal() W3D_OVERRIDE { return true; }

        IDE* ide;
        bool replace;
        GroupPropertie* group;
        PropText* pFind; PropText* pReplacement; PropLabel* pCount; PropButtonRow* pNav;
        Card* bar;
        ScrollablePanel dz;
        bool findFocused;   // el campo Buscar tenia el foco el cuadro anterior (para saber cuando se suelta)
        void Build();
        void Relocate();
        int  RowY(int idx) const;
        void ClickInRow(int mx, int my);
        void Sync();    // lee los campos, busca de nuevo si cambio el texto, actualiza "3 de 7"
};

void IDEOpenFind(IDE* ide, bool replace);
bool IDEFindOpen();
// lo que el IDE resalta (NULL = el panel esta cerrado o es de otro IDE)
struct IdeSearch { const IDE* ide; std::string text; bool caps; bool word; };
const IdeSearch* IDEActiveSearch();
// (harness) los botones del panel: 0 anterior, 1 siguiente, 2 reemplazar, 3 reemplazar todo, 4 cerrar
void IDEFindSet(const std::string& text, const std::string& replacement, int caps, int word);
void IDEFindButton(int which);
std::string IDEFindCount();
#endif // FIND_POPUP_H
