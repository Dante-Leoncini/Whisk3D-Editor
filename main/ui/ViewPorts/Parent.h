#ifndef PARENT_H
#define PARENT_H

/**
 * @file Parent.h
 * @brief Emparentar / desemparentar objetos (Ctrl+P / Alt+P), estilo Blender. Extraido de LayoutInput.
 *
 * Emparentar / desemparentar objetos (Ctrl+P / Alt+P), estilo Blender. Extraido de LayoutInput.
 */
class PopupMenu; class Object;

void AccionSetParent(int aId);          // ids 0-3: Object / Keep Transform / Without Inverse / ...
void AccionClearParent(int aId);        // ids 0-2: Clear / Clear+Keep T. / Clear Inverse
PopupMenu* LayoutSubmenuSetParent();    // "Set Parent To" -> submenu del menu Object
PopupMenu* LayoutSubmenuClearParent();  // "Clear Parent"  -> submenu del menu Object
void LayoutMenuParent(bool clear, int mx, int my);  // atajo Ctrl+P / Alt+P: abre el menu en el cursor

#endif
