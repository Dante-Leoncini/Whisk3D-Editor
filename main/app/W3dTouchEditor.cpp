// Los dedos del Core (input/W3dTouch.h) -> SDL_Event -> InputUserSDL3. Ver W3dTouchEditor.h. C++03 (Symbian).
#include "W3dTouchEditor.h"
#include "input/W3dTouch.h"
#include "controls.h"                // SDL_Event (o W3dSdlMin.h) + InputUserSDL3
#include "render/RenderOptions.h"    // g_redraw
#include <string.h>

static void ReceiveFinger(const w3dEngine::TouchEvent& t) {
    SDL_Event e; memset(&e, 0, sizeof(e));
    switch (t.type) {
        case w3dEngine::TouchMouseMoved:
#ifdef W3D_SYMBIAN
            // el cursor del telefono ES el mouse (SaveMousePos y el hover lo leen); con el dedo no se dibuja
            mouseX = (GLshort)t.x; mouseY = (GLshort)t.y;
            mouseVisible = false;
#endif
            e.type = SDL_MOUSEMOTION;
            e.motion.which = SDL_TOUCH_MOUSEID;
            e.motion.state = t.pressed ? SDL_BUTTON_LMASK : 0;
            e.motion.x = t.x; e.motion.y = t.y; e.motion.xrel = t.xrel; e.motion.yrel = t.yrel;
            break;
        case w3dEngine::TouchMouseButton:
            e.type = t.pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            e.button.which = SDL_TOUCH_MOUSEID;
            e.button.button = SDL_BUTTON_LEFT;
            e.button.state = t.pressed ? SDL_PRESSED : SDL_RELEASED;
            e.button.clicks = 1;
            e.button.x = t.x; e.button.y = t.y;
            break;
        default:
            e.type = (t.type == w3dEngine::TouchFingerUnder) ? SDL_FINGERDOWN
                   : (t.type == w3dEngine::TouchFingerUp) ? SDL_FINGERUP : SDL_FINGERMOTION;
            e.tfinger.touchId = 1;
            e.tfinger.fingerId = (SDL_FingerID)t.finger;
            e.tfinger.x = t.nx; e.tfinger.y = t.ny; e.tfinger.dx = t.ndx; e.tfinger.dy = t.ndy;
            e.tfinger.pressure = 1.0f;
            break;
    }
    e.motion.timestamp = SDL_GetTicks();   // (el timestamp esta en el mismo lugar en todos)
    g_redraw = true;                       // (como ProcessEvent de main.cpp: cualquier evento redibuja)
    InputUserSDL3(e);
}

// el editor recibe los dedos del Core desde que arranca (el juego compilado puede enganchar el suyo)
struct TouchEditorHook { TouchEditorHook() { w3dEngine::TouchSetReceptor(ReceiveFinger); } };
static TouchEditorHook g_touchEditorHook;

bool W3dEventTouch() { return w3dEngine::TouchInEvent(); }
#include "W3dMouseEditor.h"
bool W3dEventByControls() { return w3dEngine::TouchInEvent() || W3dEventMouse(); }
