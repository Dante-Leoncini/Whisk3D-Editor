#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <vector>
#include <SDL2/SDL.h>

#include <GL/gl.h>

#include "variables.h"
#include "sdl_key_compat.h"
#include "ViewPorts.h"
#include "ScrollBar.h"
#include "WithBorder.h"

#include "objects/Objects.h"
#include "objects/ObjectMode.h"

#include "objects/Light.h" 
#include "objects/Camera.h" 
#include "objects/Mesh.h" 

#include "Propieties/PropertieBase.h" 
#include "Propieties/GroupPropertie.h" 
#include "Propieties/PropFloat.h" 
#include "Propieties/PropGap.h" 
#include "Propieties/PropList.h" 
#include "Propieties/PropBool.h"

void DibujarTitulo(Object* obj, int maxPixels);
void ConstructorProperties();

extern GroupPropertie* propTransform;
extern GroupPropertie* propMeshParts;

class Properties : public ViewportBase, public WithBorder, public Scrollable {
    public:    
        Properties();
        ~Properties() override;

        Object* target = nullptr;
        int maxPixelsTitle = 1920;

        int selectIndex = 0;
        void NextSelect();
        void PrevSelect();
        void SetOpenGroup(bool open);

        bool editando = false;
        void EnterPropertieSelect();

        void Resize(int newW, int newH) override;
        void Render() override;

        void RefreshTargetProperties();
        void RefreshPropMeshParts();

        void button_left() override;
        void button_right() override;
        void button_up() override;
        void button_down() override;
        void Cancel();
        void mouse_button_up(SDL_Event &e) override;
        void event_mouse_wheel(SDL_Event &e) override;
        void FindMouseOver(int mx, int my);
        void event_mouse_motion(int mx, int my) override;
        void event_key_down(SDL_Event &e) override;
        void event_key_up(SDL_Event &e) override;
        void key_down_return();
};

#endif