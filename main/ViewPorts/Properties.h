#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <vector>
#include <SDL2/SDL.h>

#include "variables.h"
#include "sdl_key_compat.h"
#include "ViewPorts.h"
#include "ScrollBar.h"
#include "WithBorder.h"
#include "objects/Objects.h"
#include "objects/ObjectMode.h"

//para la UI 2d
#include "UI/card.h"
#include "UI/bitmapText.h"

#include <GL/gl.h>

class Properties : public ViewportBase, public WithBorder, public Scrollable {
    public:
        Card* card;
        Properties();
        ~Properties() override;

        void Resize(int newW, int newH) override;
        void Render() override;

        void DibujarTitulo(Object* obj);
        void CardTitulo(GLfloat* icon, const std::string& texto);
        void DibujarPropiedadFloat(const std::string& text, float value);

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