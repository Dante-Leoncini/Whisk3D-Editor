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
        void SoltarDrag(int mx, int my);
        int ViewportKind() const { return 2; } // (menu de tipo)
        void ClearHover() { hoverFila = -1; } // el mouse se fue

        void key_down_return();
};

#endif