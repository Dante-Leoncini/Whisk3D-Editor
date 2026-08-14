#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/WithBorder.h" // borde (verde si activo) como Outliner/Properties/UV
#include "ViewPorts/ScrollBar.h"  // Scrollable: barras v/h + rueda + drag (mismo ruteo que Outliner)

// =====================================================================
//  Console — viewport que muestra el LOG del motor y los ERRORES de lua.
// =====================================================================
//  Lee el ring buffer en memoria del log del Core (w3dLogRingLinea /
//  w3dLogRingCount): las ultimas N lineas de w3dLog/w3dLogf, con su nivel.
//  Ademas muestra BIEN VISIBLE el ultimo error de lua (W3dScriptUltimoError),
//  que hasta ahora se llenaba pero no se mostraba en ningun lado de la UI.
//
//  Colores por nivel: [ERROR] rojo, [WARN] amarillo, [INFO] gris claro.
//  Scroll: hereda Scrollable (como el Outliner) -> barra vertical y horizontal
//  arrastrables, rueda = vertical (shift+rueda = horizontal), y arrastrar el
//  contenido (click o dedo) tambien scrollea. AUTOSCROLL pegado al final: si
//  estas mirando el final, el log nuevo te sigue; si scrolleas hacia arriba se
//  despega, y al volver al fondo se re-engancha.
class Console : public ViewportBase, public WithBorder, public Scrollable {
    public:
        // 1=3D 2=outliner 3=props 4=UV 5=timeline 6=2D 7=console
        int ViewportKind() const { return 7; }
        // engancha el ruteo compartido de scrollbars (hover/agarre/arrastre/touch)
        Scrollable* ComoScrollable() { return this; }

        // metricas del contenido la ultima vez que se midio (para recalcular el
        // rango de scroll SOLO cuando cambia algo, como lastContentRows del Outliner)
        int lastCount;    // lineas del ring buffer
        int lastMaxAncho; // ancho (px) de la linea mas larga
        int lastBannerH;  // alto del banner del ultimo error de lua (0 si no hay)

        Console();
        virtual ~Console();

        void Render() W3D_OVERRIDE;
        void Resize(int newW, int newH) W3D_OVERRIDE;
        void button_left() W3D_OVERRIDE;
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE; // drag con boton = scroll del contenido
#ifndef W3D_SYMBIAN
        void mouse_button_up(int boton) W3D_OVERRIDE;         // libera ViewPortClickDown (foco por hover)
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
#endif
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // touch: arrastrar = scroll v/h

    private:
        // recalcula el scrollbar con las metricas guardadas, manteniendo el
        // autoscroll: si estabas pegado al final, el final nuevo te sigue
        void RecalcularScroll();
};

#endif // CONSOLE_H
