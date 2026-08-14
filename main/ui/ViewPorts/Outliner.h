#ifndef OUTLINER_H
#define OUTLINER_H

#include <vector>
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>
    #include "variables.h"
    #include "sdl_key_compat.h"
#endif
#include "ViewPorts.h"
#include "ScrollBar.h"
#include "WithBorder.h"
#include "objects/Objects.h"
#include "objects/ObjectMode.h"
#include "objects/Textures.h"
#include "WhiskUI/draw/rectangle.h"
#include "WhiskUI/text/bitmapText.h"

class Mesh; // fila VIRTUAL "Armature 2D" (los huesos 2D viven dentro del mesh, no son objetos)

class Outliner : public ViewportBase, public WithBorder, public Scrollable {
    public:
        Scrollable* ComoScrollable() { return this; }
        size_t CantidadRenglones;
        int lastContentRows;   // filas visibles del ultimo render: si cambian (import/add/delete/desplegar) se recalcula
                               // el scrollbar (antes solo se recalculaba al redimensionar -> tras importar no scrolleaba)
        Rec2D* Renglon;

        // CULLING vertical: DibujarRenglon/DibujarOjos recorren TODA la jerarquia (avanzan la matriz igual), pero SALTEAN
        // el DRAW (iconos + texto) de las filas fuera del area visible. Sin esto, una escena con muchos objetos dibuja
        // miles de filas/nombres que el Scissor descarta despues (pagando igual los draw-calls). filaDFS = indice de fila
        // en orden DFS (mapea 1:1 con la Y de la matriz); cullBaseY = Y en pantalla de la 1er fila del recorrido actual.
        int cullBaseY;
        unsigned filaDFS;

        Outliner();
        ~Outliner() override;

        void CalcularRenglon(Object* obj, int* MaxPosXtemp, int* MaxPosYtemp);
        void Resize(int newW, int newH) override;
        void Render() override;
        void DibujarRenglon(Object* obj, bool hidden);
        // fila VIRTUAL "Armature 2D" de una malla: UNA sola fila informativa, en AZUL y colgando de
        // la malla, que avisa que el rig 2D es PARTE del mesh y no un objeto de escena. Inerte: no
        // se arrastra, no acepta drops, no se desempareja y NO se despliega (los huesos se listan
        // en la pestania "Armature 2D" del panel Properties).
        void DibujarArm2D(Mesh* m, int idx, bool hidden);
        void DibujarLineaDesplegada(Object* obj);
        void DibujarOjos(Object* obj, bool hidden, bool noRender);

        void button_left() override;
        void FindMouseOver(int mx, int my);
        void event_mouse_motion(int mx, int my) override;
        bool event_finger_scroll(int px, int py, int dx, int dy) override; // touch: arrastrar = scroll v/h
#ifndef W3D_SYMBIAN
        void mouse_button_up(int boton) override;
        void event_mouse_wheel(float dy, int mx, int my) override;
        void event_key_down(int tecla, bool repeticion) override;
        void event_key_up(int tecla) override;
#endif
        // click para seleccionar la fila (compartido; en el N95 lo llama el
        // router del mouse HID, en PC se puede cablear a mouse_button_up)
        void ClickSeleccionar(int mx, int my);

        // numpad "." (como el Frame Selected de Blender): scrollea el outliner para CENTRAR
        // la seleccion en la vista (promedio de las filas visibles de lo seleccionado)
        void CentrarSeleccion();
        // scroll MINIMO para que la fila del objeto ACTIVO se vea entera. A diferencia de
        // CentrarSeleccion (numpad "."), no mueve nada si la fila ya esta a la vista: con
        // 315 objetos, centrar en cada tecla marea. La llama la navegacion por flechas.
        void AsegurarVisible();

        // fila bajo el mouse (hover) para feedback visual antes del click
        int hoverFila;
        // arrastre de filas (reordenar / emparentar soltando encima)
        Object* dragObjeto;
        bool dragging;
        int dragY0;
        // vista previa del drop: -2 nada, -1 al vacio (raiz),
        // 0 antes de la fila, 1 hijo de la fila, 2 despues
        int dropFila;
        int dropZona;
        int dropProf; // profundidad (nivel de sangria) del destino: la linea de insercion se indenta a ese nivel
        void SoltarDrag(int mx, int my);

        // MODO MOVER con teclado (sin mouse, clave en N95 para ordenar lamparas antes de los objetos):
        // g (PC) / 1 (Symbian) entra; flechas reordenan (arriba/abajo) o reparentan (izq=sacar / der=meter);
        // OK/Enter confirma; C/backspace cancela (restaura la posicion original en el arbol).
        bool moviendo;
        Object* moverObj;          // el objeto que se esta moviendo (= el activo al entrar)
        Object* moverPadreOrig;    // padre original (NULL = raiz) para cancelar
        Object* moverAnteriorOrig; // hermano que estaba ANTES de moverObj (NULL = era el primero) para cancelar
        void MoverIniciar();       // entra en modo mover con el objeto activo
        void MoverPaso(int dir);   // 0=arriba 1=abajo 2=afuera(unparent) 3=adentro(parent)
        void MoverConfirmar();     // sale del modo (deja la posicion nueva)
        void MoverCancelar();      // restaura la posicion original y sale
        bool ModoMover() const { return moviendo; }
        int ViewportKind() const { return 2; } // (menu de tipo)
        void ClearHover() { hoverFila = -1; } // el mouse se fue

        void key_down_return();
};

#endif