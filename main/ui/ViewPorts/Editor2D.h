#ifndef EDITOR2D_H
#define EDITOR2D_H

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/WithBorder.h" // borde (verde si activo), igual que UV/Outliner

// =====================================================================
//  Editor 2D — lienzo para INTERFACES de juegos/programas (HUD, menus),
//  juegos 2D y animacion 2D. Mismo manejo que el UV Editor:
//    rueda del mouse       = zoom (hacia el cursor)
//    boton del MEDIO + mover = paneo
//    flechas               = paneo con teclado
//
//  V1: el lienzo con grilla + el marco de "pantalla" de referencia
//  (1080x1920, un celular en vertical: el caso de los slots). Sobre esto
//  se monta la edicion del objeto UI (widgets, imagenes, export).
// =====================================================================
class Editor2D : public ViewportBase, public WithBorder {
    public:
        int ViewportKind() const { return 6; } // 1=3D 2=outliner 3=props 4=UV 5=timeline 6=2D

        float zoom;           // 1 = el marco de pantalla entra comodo (~80% del alto)
        float panX, panY;     // desplazamiento de la vista, en pixeles de viewport
        int   lastMx, lastMy; // para el delta del paneo con boton del medio

        Editor2D();
        virtual ~Editor2D();

        void Render() override;
        void Resize(int newW, int newH) override;
        void event_mouse_motion(int mx, int my) override;
        bool event_finger_scroll(int px, int py, int dx, int dy) override;             // touch: 1 dedo panea
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) override; // touch: 2 dedos zoom+paneo

        void Panear(float dx, float dy); // compartido PC/Symbian (flechas / touch)
        void ZoomCentro(int dir);        // zoom centrado (teclado / pinch)
#ifndef W3D_SYMBIAN
        void event_key_down(int tecla, bool repeticion) override; // flechas = paneo
        void event_mouse_wheel(float dy, int mx, int my) override;
        // IMPRESCINDIBLE: resetear ViewPortClickDown al soltar (sino viewPortActive se congela
        // y el borde verde/resize dejan de andar; mismo gotcha documentado en UVEditor).
        void mouse_button_up(int boton) override;
#endif
        // mapeo lienzo->pantalla (centro cx,cy del marco y escala s), igual que usa Render.
        void ParamsLienzo(float& cx, float& cy, float& s) const;
    private:
        // dibuja los TEXTOS de todos los objetos UI de la escena sobre el lienzo.
        // (fx,fy) = esquina superior izquierda del marco en pantalla; s = escala.
        void DibujarTextosUI(float fx, float fy, float s);
};

#endif // EDITOR2D_H
