// ===================================================================================================
//  EMPARENTAR / DESEMPARENTAR (Ctrl+P / Alt+P). Extraido de LayoutInput.cpp (Fase 2). Ver Parent.h.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
#include "ViewPorts/Notificaciones.h" // toasts (extraido a su propio archivo)
#include "ViewPorts/NumInput.h" // entrada numerica/formulas (extraido a su propio archivo)
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/UVEditor.h"
#include "ViewPorts/Timeline.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h" // el velo del modo foco
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Materials.h" // Material (mat->texture) para el dropdown "Texture" del UV editor
#include "objects/Textures.h"  // Texture (path) para las etiquetas del dropdown
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertarKeyframeEsqueleto (Pose Mode: Insert Keyframe)
#include "objects/Instance.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "objects/Primitivas.h"
#include "objects/Textures.h"
#include "variables.h"
#include "render/OpcionesRender.h" // g_fpsActual
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/theme/colores.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
#include "ViewPorts/Parent.h"

// ====================================================================
// menus de emparentar (ctrl+P / alt+P), como Blender
// ====================================================================

static PopupMenu* gMenuSetParent = NULL;
static PopupMenu* gMenuClearParent = NULL;

// los seleccionados (sin contar al activo / a los ya-raiz)
static void RecolectarSeleccionados(Object* aNodo, std::vector<Object*>& aSel,
                                    Object* aExcluir) {
    if (!aNodo) return;
    for (size_t i = 0; i < aNodo->Childrens.size(); i++) {
        Object* o = aNodo->Childrens[i];
        if (o->select && o != aExcluir) aSel.push_back(o);
        RecolectarSeleccionados(o, aSel, aExcluir);
    }
}

// 0 Object, 1 Keep Transform, 2 Without Inverse, 3 Keep T. Without Inv.
// (sin matrices de inversa propias: 0==2 y 1==3 en nuestro modelo)
void AccionSetParent(int aId) {
    if (!ObjActivo || !SceneCollection) return;
    std::vector<Object*> sel;
    RecolectarSeleccionados(SceneCollection, sel, ObjActivo);
    for (size_t i = 0; i < sel.size(); i++) {
        if (aId == 1 || aId == 3) ReparentKeepTransform(sel[i], ObjActivo);
        else ReparentSimple(sel[i], ObjActivo);
    }
}

// 0 Clear Parent, 1 Clear and Keep Transformation, 2 Clear Parent Inverse
void AccionClearParent(int aId) {
    if (!SceneCollection) return;
    std::vector<Object*> sel;
    RecolectarSeleccionados(SceneCollection, sel, NULL);
    for (size_t i = 0; i < sel.size(); i++) {
        if (!sel[i]->Parent || sel[i]->Parent == SceneCollection) continue;
        if (aId == 1) ReparentKeepTransform(sel[i], SceneCollection);
        else ReparentSimple(sel[i], SceneCollection);
    }
}

// Los menus Set/Clear Parent se usan de DOS formas: standalone (Ctrl+P / Ctrl+Alt+P) y como SUBMENUS del menu
// "Object" de la barra. Para que anden igual en ambos casos usan ids UNICOS (230-233 / 240-242) + la accion
// LayoutAccionObject (que rutea esos ids a AccionSetParent/AccionClearParent), como el submenu Apply/Delete.
PopupMenu* LayoutSubmenuSetParent() {
    if (!gMenuSetParent) {
        gMenuSetParent = new PopupMenu();
        gMenuSetParent->titulo = T("Set Parent To");
        gMenuSetParent->action = LayoutAccionObject;
        gMenuSetParent->Agregar(T("Object"), 230);
        gMenuSetParent->Agregar(T("Object (Keep Transform)"), 231);
        gMenuSetParent->Agregar(T("Object (Without Inverse)"), 232);
        gMenuSetParent->Agregar(T("Object (Keep Transform Without Inverse)"), 233);
    }
    return gMenuSetParent;
}
PopupMenu* LayoutSubmenuClearParent() {
    if (!gMenuClearParent) {
        gMenuClearParent = new PopupMenu();
        gMenuClearParent->titulo = T("Clear Parent");
        gMenuClearParent->action = LayoutAccionObject;
        gMenuClearParent->Agregar(T("Clear Parent"), 240);
        gMenuClearParent->Agregar(T("Clear and Keep Transformation"), 241);
        gMenuClearParent->Agregar(T("Clear Parent Inverse"), 242);
    }
    return gMenuClearParent;
}

void LayoutMenuParent(bool aClear, int mx, int my) {
    if (aClear) {
        PopupMenu* m = LayoutSubmenuClearParent();
        m->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
        MenuAbierto = m;
    } else {
        if (!ObjActivo) return; // no hay a quien emparentar
        PopupMenu* m = LayoutSubmenuSetParent();
        m->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
        MenuAbierto = m;
    }
}
