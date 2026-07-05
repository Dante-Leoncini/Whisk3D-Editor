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
#include "WhiskUI/rectangle.h"
#include "WhiskUI/bitmapText.h"

class Outliner : public ViewportBase, public WithBorder, public Scrollable {
    public:
        size_t CantidadRenglones;
        Rec2D* Renglon;

        Outliner();
        ~Outliner() override;

        void CalcularRenglon(Object* obj, int* MaxPosXtemp, int* MaxPosYtemp);
        void Resize(int newW, int newH) override;
        void Render() override;
        void DibujarRenglon(Object* obj, bool hidden);
        void DibujarLineaDesplegada(Object* obj);
        void DibujarOjos(Object* obj, bool hidden);

        void button_left() override;
        void FindMouseOver(int mx, int my);
        void event_mouse_motion(int mx, int my) override;
#ifndef W3D_SYMBIAN
        void mouse_button_up(SDL_Event &e) override;
        void event_mouse_wheel(SDL_Event &e) override;
        void event_key_down(SDL_Event &e) override;
        void event_key_up(SDL_Event &e) override;
#endif
        // click para seleccionar la fila (compartido; en el N95 lo llama el
        // router del mouse HID, en PC se puede cablear a mouse_button_up)
        void ClickSeleccionar(int mx, int my);

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