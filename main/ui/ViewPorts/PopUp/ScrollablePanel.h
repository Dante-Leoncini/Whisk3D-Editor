#ifndef SCROLLABLE_PANEL_H
#define SCROLLABLE_PANEL_H
// ============================================================================
//  ScrollablePanel.h - el ALTO y el DESPLAZAMIENTO de los paneles flotantes con filas (el de la ultima transformacion,
//  Buscar/Reemplazar). Si el contenido pasa de la MITAD del alto de la pantalla, el panel se ve recortado a esa mitad
//  y se desplaza:
//    - arrastrando el dedo (o el mouse) en cualquier parte del panel: un arrastre vertical desplaza y NO toca las
//      opciones; un toque sin arrastrar es un click normal (se resuelve al soltar)
//    - con la rueda
//    - con las flechas (teclado / telefono): la fila elegida siempre queda a la vista
//  Solo el estado y las cuentas: cada panel lo usa desde su Click / Motion / Soltar / Wheel / Render. C++03.
// ============================================================================
#include "WhiskUI/core/UI.h"               // RowHeightGS / gapGS / GlobalScale
#include "WhiskUI/widgets/PopupMenu.h"     // MenuScreenH

struct ScrollablePanel {
    int scroll;      // px del contenido escondidos arriba
    int visible;     // alto visible del contenido
    int total;       // alto del contenido entero
    bool pressed;    // boton / dedo abajo adentro del panel
    bool scrolling;  // el gesto ya es un arrastre (no un toque)
    int pressX, pressY, lastY;

    ScrollablePanel() : scroll(0), visible(0), total(0), pressed(false), scrolling(false), pressX(0), pressY(0), lastY(0) {}

    // el alto que se ve: todo, o la mitad de la pantalla si no entra
    void Measure(int contentHeight) {
        total = contentHeight;
        const int limit = MenuScreenH / 2;
        visible = (limit > 0 && total > limit) ? limit : total;
        Limit();
    }
    bool Scrollable() const { return total > visible; }
    void Limit() {
        const int m = total - visible;
        if (scroll > m) scroll = m;
        if (scroll < 0) scroll = 0;
    }
    void Wheel(int delta) { scroll -= delta * (RowHeightGS + gapGS) * 2; Limit(); }
    // que la franja [y0, y0+h) del contenido quede a la vista
    void ToView(int y0, int h) {
        if (y0 < scroll) scroll = y0;
        if (y0 + h > scroll + visible) scroll = y0 + h - visible;
        Limit();
    }

    // ---- el gesto: Abajo en el Click, Mover en el Motion, Arriba en el Soltar ----
    void Down(int mx, int my) { pressed = true; scrolling = false; pressX = mx; pressY = my; lastY = my; }
    // true = es un arrastre vertical (desplaza; el panel no tiene que hacer nada mas con este movimiento)
    bool Move(int mx, int my) {
        (void)mx;
        if (!pressed || !Scrollable()) return false;
        const int threshold = 6 * (GlobalScale > 0 ? GlobalScale : 1);
        if (!scrolling && (my - pressY > threshold || pressY - my > threshold)) scrolling = true;
        if (scrolling) { scroll -= (my - lastY); Limit(); }
        lastY = my;
        return scrolling;
    }
    // true = fue un TOQUE (sin arrastrar): el panel ejecuta el click en (pressX, pressY)
    bool Up() {
        const bool touch = pressed && !scrolling;
        pressed = false; scrolling = false;
        return touch;
    }
};
#endif // SCROLLABLE_PANEL_H
