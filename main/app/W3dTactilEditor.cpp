// Los dedos del Core (input/W3dTactil.h) -> SDL_Event -> InputUsuarioSDL3. Ver W3dTactilEditor.h. C++03 (Symbian).
#include "W3dTactilEditor.h"
#include "input/W3dTactil.h"
#include "controles.h"                // SDL_Event (o W3dSdlMinimo.h) + InputUsuarioSDL3
#include "render/OpcionesRender.h"    // g_redraw
#include <string.h>

static void RecibirDedo(const w3dEngine::TactilEvento& t) {
    SDL_Event e; memset(&e, 0, sizeof(e));
    switch (t.tipo) {
        case w3dEngine::TactilMouseMovio:
#ifdef W3D_SYMBIAN
            // el cursor del telefono ES el mouse (GuardarMousePos y el hover lo leen); con el dedo no se dibuja
            mouseX = (GLshort)t.x; mouseY = (GLshort)t.y;
            mouseVisible = false;
#endif
            e.type = SDL_MOUSEMOTION;
            e.motion.which = SDL_TOUCH_MOUSEID;
            e.motion.state = t.apretado ? SDL_BUTTON_LMASK : 0;
            e.motion.x = t.x; e.motion.y = t.y; e.motion.xrel = t.xrel; e.motion.yrel = t.yrel;
            break;
        case w3dEngine::TactilMouseBoton:
            e.type = t.apretado ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            e.button.which = SDL_TOUCH_MOUSEID;
            e.button.button = SDL_BUTTON_LEFT;
            e.button.state = t.apretado ? SDL_PRESSED : SDL_RELEASED;
            e.button.clicks = 1;
            e.button.x = t.x; e.button.y = t.y;
            break;
        default:
            e.type = (t.tipo == w3dEngine::TactilDedoBajo) ? SDL_FINGERDOWN
                   : (t.tipo == w3dEngine::TactilDedoSubio) ? SDL_FINGERUP : SDL_FINGERMOTION;
            e.tfinger.touchId = 1;
            e.tfinger.fingerId = (SDL_FingerID)t.dedo;
            e.tfinger.x = t.nx; e.tfinger.y = t.ny; e.tfinger.dx = t.ndx; e.tfinger.dy = t.ndy;
            e.tfinger.pressure = 1.0f;
            break;
    }
    e.motion.timestamp = SDL_GetTicks();   // (el timestamp esta en el mismo lugar en todos)
    g_redraw = true;                       // (como ProcesarEvento de main.cpp: cualquier evento redibuja)
    InputUsuarioSDL3(e);
}

// el editor recibe los dedos del Core desde que arranca (el juego compilado puede enganchar el suyo)
struct TactilEditorEnganche { TactilEditorEnganche() { w3dEngine::TactilSetReceptor(RecibirDedo); } };
static TactilEditorEnganche g_tactilEditorEnganche;

bool W3dEventoTactil() { return w3dEngine::TactilEnEvento(); }
#include "W3dRatonEditor.h"
bool W3dEventoPorControles() { return w3dEngine::TactilEnEvento() || W3dEventoRaton(); }
