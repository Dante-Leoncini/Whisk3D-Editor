#ifndef PARENT_H
#define PARENT_H

/**
 * @file Parent.h
 * @brief Emparentar / desemparentar objetos (Ctrl+P / Alt+P), estilo Blender. Extraido de LayoutInput.
 *
 * Emparentar / desemparentar objetos (Ctrl+P / Alt+P), estilo Blender. Extraido de LayoutInput.
 */
class PopupMenu; class Object; class Mesh;

void AccionSetParent(int aId);          // ids 0-3: Object / Keep Transform / Without Inverse / ...
void AccionClearParent(int aId);        // ids 0-2: Clear / Clear+Keep T. / Clear Inverse
PopupMenu* LayoutSubmenuSetParent();    // "Set Parent To" -> submenu del menu Object
PopupMenu* LayoutSubmenuClearParent();  // "Clear Parent"  -> submenu del menu Object
void LayoutMenuParent(bool clear, int mx, int my);  // atajo Ctrl+P / Alt+P: abre el menu en el cursor

// ===== EMPARENTAR HUESOS (Ctrl+P / Alt+P en EDICION de huesos: 3D y armature 2D del UV) =====
// El MISMO popup de 2 opciones en ambos editores: "Disconnect Bone" (sigue emparentado pero sin
// soldar: queda la linea punteada) y "Clear Parent" (hueso libre). El contexto (huesos 3D vs 2D)
// se fija al ABRIRLO (cada opener sabe sobre que opera).
PopupMenu* LayoutSubmenuBoneAltP();            // el menu (se crea una vez); tambien va como submenu de los menus Armature
// Ctrl+P: el OTRO popup de 2 opciones, "Keep Offset" (id 0, el hueso no se mueve) y "Connected"
// (id 1, el hueso se muda para pegar su head al tail del padre y queda soldado). Mismo contexto
// (BoneAltPContexto3D / BoneAltPContexto2D) que el menu de Alt+P.
PopupMenu* LayoutSubmenuBoneCtrlP();
void BoneAltPContexto3D();                     // proximo Ejecutar() del menu -> huesos del armature en edicion (3D)
void BoneAltPContexto2D(Mesh* m);              // proximo Ejecutar() -> huesos 2D de ESTE mesh (editor UV)
void LayoutMenuBoneAltP2D(Mesh* m, int mx, int my); // abre el popup Alt+P para huesos 2D (boton Disconnect de la toolbar UV)
void LayoutMenuBoneCtrlP2D(Mesh* m, int mx, int my);// abre el popup Ctrl+P para huesos 2D (menu Armature de la barra UV)
// atajos Ctrl+P (alt=false: popup Keep Offset / Connected) / Alt+P (alt=true: popup Disconnect
// Bone / Clear Parent) EN CONTEXTO DE HUESOS. true = habia contexto de huesos (3D o UV) y se
// consumio el atajo; false = no hay huesos en edicion -> el caller cae a los menus de OBJETOS.
bool LayoutParentHuesos(bool alt, int mx, int my);

#endif
