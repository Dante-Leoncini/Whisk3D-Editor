#ifndef REDOMESHPANEL_H
#define REDOMESHPANEL_H

#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"

class Mesh;
class Viewport3D;

// Ventanita flotante "Add ..." (estilo redo-panel de Blender) que aparece al
// crear una primitiva (cube/plane/circle). REUSA las tarjetas de Propiedades:
// un GroupPropertie con campos PropFloat (Size / Radius+Vertices). Editar un
// campo regenera la malla en vivo (Mesh::Regenerar). Se cierra al hacer otra
// cosa (click afuera).
class RedoMeshPanel : public PopUpBase {
    public:
        RedoMeshPanel(Mesh* m, int modo = 0); // 0 = primitiva (Add); 1 = Recalculate Normals
        ~RedoMeshPanel();

        void Render() override;
        bool Click(int mx, int my) override;
        bool Motion(int mx, int my) override;
        bool Tecla(int tecla) override;
        void Soltar() override;
        bool CierraConViewport() override { return true; } // se cierra al tocar el viewport

    private:
        GroupPropertie* grupo;
        PropFloat* dragField; // campo bajo el toque: si se ARRASTRA cambia el valor; si es un TAP abre el teclado
        bool dragMoved;       // el dedo/mouse se movio (arrastre) -> no abrir el teclado al soltar
        int lastDragMx;       // X del ultimo evento (el popup no warpea el mouse)
        Viewport3D* vpCreador;  // el viewport3D que creo la forma (se lee su rect ACTUAL)
        void ResizeGrupo();
        void Reubicar(); // x/y en la esquina inf-izq del viewport
};

// abre/reemplaza el panel para la primitiva recien creada
void AbrirRedoMeshPanel(Mesh* m);

// abre/reemplaza el panel "Recalculate Normals" (tilde Inside) sobre la malla en edicion
void AbrirRedoNormalesPanel(Mesh* m);

// abre/reemplaza el panel "Loop Cut and Slide" (Number of Cuts + Factor)
void AbrirRedoLoopCutPanel(Mesh* m);

#endif
