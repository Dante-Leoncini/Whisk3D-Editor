#ifndef OUTLINER_H
#define OUTLINER_H

#include <vector>
#include <SDL2/SDL.h>

#include "variables.h"
#include "sdl_key_compat.h"
#include "ViewPorts.h"
#include "objects/Objects.h"
#include "objects/ObjectMode.h"
#include "UI/rectangle.h"

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
    void mouse_button_up(SDL_Event &e) override;
    void event_mouse_wheel(SDL_Event &e) override;
    void FindMouseOver(int mx, int my);
    void event_mouse_motion(int mx, int my) override;
    void event_key_down(SDL_Event &e) override;
    void event_key_up(SDL_Event &e) override;
    void key_down_return();
};

#endif