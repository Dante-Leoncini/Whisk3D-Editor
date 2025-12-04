#include "controles.h"
#include "variables.h"
#include "ViewPorts/ViewPorts.h"

void Contadores(){
    if (LShiftPressed){
        ShiftCount++;
    }
}

void InputUsuarioSDL3(SDL_Event &e){
    RefreshInputControllerSDL(e);

    if (e.type == SDL_EVENT_MOUSE_MOTION){
        int mx = e.motion.x;
        int my = e.motion.y;

        int oglY = winH - my;
        if (!ViewPortClickDown){
            viewPortActive = FindViewportUnderMouse(rootViewport, mx, oglY);
        }

        if (viewPortActive){
            viewPortActive->event_mouse_motion(mx, my);
        }

        if ((leftMouseDown || middleMouseDown) && viewPortActive) {
            CheckWarpMouseInViewport(mx, my, viewPortActive);
        }
        else if (estado == translacion || estado == rotacion || estado == EditScale){
            ViewPortClickDown = true;
            CheckWarpMouseInViewport(mx, my, viewPortActive);
        }
    }

    if (!viewPortActive) return;

    // rueda del mouse
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        viewPortActive->event_mouse_wheel(e);
    }

    // Botones del mouse
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        ViewPortClickDown = true;
        if (e.button.button == SDL_BUTTON_LEFT) { 
            leftMouseDown = true;
            viewPortActive->button_left();
            GuardarMousePos();
        }
        else if (e.button.button == SDL_BUTTON_MIDDLE) {
            middleMouseDown = true;
            GuardarMousePos();
        }
        else if (e.button.button == SDL_BUTTON_RIGHT) {  
            if (estado == translacion || estado == EditScale || estado == rotacion){
                Cancelar();
            }
        }
    }
    else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        ViewPortClickDown = false;
        if (e.button.button == SDL_BUTTON_LEFT) {  
            leftMouseDown = false;
        }
        else if (e.button.button == SDL_BUTTON_MIDDLE) {
            middleMouseDown = false;
        }
        GuardarMousePos();
        viewPortActive->mouse_button_up(e);

        int oglY = winH - lastMouseY;
        viewPortActive = FindViewportUnderMouse(rootViewport, lastMouseX, oglY);
    }

    // eventos del teclado
    if (e.type == SDL_EVENT_KEY_DOWN) {
        viewPortActive->event_key_down(e);
    }   
    else if (e.type == SDL_EVENT_KEY_UP) {
        viewPortActive->event_key_up(e);
    }
}