#ifndef REDOMESHPANEL_H
#define REDOMESHPANEL_H

#include "PopUpBase.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"

class Mesh;
class Viewport3D;

// Ventanita flotante "Add ..." que aparece al
// crear una primitiva (cube/plane/circle). REUSA las tarjetas de Propiedades:
// un GroupPropertie con campos PropFloat (Size / Radius+Vertices). Editar un
// campo regenera la malla en vivo (Mesh::Regenerar). Se cierra al hacer otra
// cosa (click afuera).
class RedoMeshPanel : public PopUpBase {
    public:
        RedoMeshPanel(Mesh* m, int mode = 0); // 0 = primitiva (Add); 1 = Recalculate Normals
        ~RedoMeshPanel();

        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        bool ClosesWithViewport() W3D_OVERRIDE { return true; } bool Collapsible() W3D_OVERRIDE { return group != 0; } bool Collapsed() W3D_OVERRIDE { return group && !group->open; } void Expand(bool open) W3D_OVERRIDE { if (group) group->open = open; } // se cierra al tocar el viewport
        void Collapse();   // nace PLEGADO: solo la flecha y el titulo (ver RedoMeshPanel.cpp)

    private:
        GroupPropertie* group;
        PropFloat* dragField; // campo bajo el toque: si se ARRASTRA cambia el valor; si es un TAP abre el teclado
        bool dragMoved;       // el dedo/mouse se movio (arrastre) -> no abrir el teclado al soltar
        int lastDragMx;       // X del ultimo evento (el popup no warpea el mouse)
        Viewport3D* vpCreator;  // el viewport3D que creo la forma (se lee su rect ACTUAL)
        void ResizeGroup();
        void Relocate(); // x/y en la esquina inf-izq del viewport
};

// abre/reemplaza el panel para la primitiva recien creada
void OpenRedoMeshPanel(Mesh* m);

// abre/reemplaza el panel "Recalculate Normals" (tilde Inside) sobre la malla en edicion
void OpenRedoNormalsPanel(Mesh* m);

// abre/reemplaza el panel "Loop Cut and Slide" (Number of Cuts + Factor)
void OpenRedoLoopCutPanel(Mesh* m);

// abre/reemplaza el panel "Inset Faces" (Thickness, Depth y las tildes de Blender)
void OpenRedoInsetPanel(Mesh* m);
// abre/reemplaza el panel "Bevel" (Affect, Width Type, Width, Segments, Profile... como el de Blender)
void OpenRedoBevelPanel(Mesh* m);

// el panel (o su malla) sigue vivo? Lo usa el test 'uipunteros': si borran la malla que edita,
// el panel tiene que cerrarse, porque sus campos apuntan a MIEMBROS de ella y los ESCRIBEN.
bool RedoMeshPanelActive();
void RedoMeshPanelClose();   // undo/redo lo cierran (su snapshot quedo viejo)

#endif
