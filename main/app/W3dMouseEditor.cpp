// El mouse de las plataformas sin SDL -> SDL_Event -> InputUserSDL3. Ver W3dMouseEditor.h. C++03 (Symbian).
#include "W3dMouseEditor.h"
#include "controls.h"                // SDL_Event (o W3dSdlMin.h) + InputUserSDL3
#include "render/RenderOptions.h"    // g_redraw
#include <string.h>

namespace {
    bool gInEvent = false;
    unsigned gPressed = 0;          // los botones que el editor cree apretados (bit 1 << (boton-1))
    // el acorde izquierdo + derecho = boton del medio
    int  gWait = 0;                 // boton que bajo y espera a ver si llega el otro (0 = ninguno)
    Uint32 gWaitFrom = 0;
    int  gWaitX = 0, gWaitY = 0;
    bool gChord = false;             // izq + der apretados = el del medio esta abajo
    bool gPhysical[4] = { false, false, false, false };   // como estan de verdad (1 izq, 2 medio, 3 der)

    void Dispatch(SDL_Event& e) {
        const bool prev = gInEvent;
        gInEvent = true;
        g_redraw = true;
        InputUserSDL3(e);
        gInEvent = prev;
    }

    void SendButton(int b, bool down, int x, int y) {
        const unsigned bit = 1u << (b - 1);
        if (down == ((gPressed & bit) != 0)) return;   // (ya estaba asi)
        if (down) gPressed |= bit; else gPressed &= ~bit;
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.timestamp = SDL_GetTicks();
        e.button.which = 0;   // un mouse de verdad (no el del tactil)
        e.button.button = (Uint8)b;
        e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = 1;
        e.button.x = x; e.button.y = y;
        Dispatch(e);
    }

    void ReleaseWait() {
        if (!gWait) return;
        const int b = gWait; gWait = 0;
        SendButton(b, true, gWaitX, gWaitY);
    }
}

void W3dMouseMoved(int x, int y, int dx, int dy) {
    ReleaseWait();   // (moverse ya decide: era un boton solo)
    SDL_Event e; memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEMOTION;
    e.motion.timestamp = SDL_GetTicks();
    e.motion.which = 0;
    e.motion.state = gPressed;   // (SDL_BUTTON(b) = 1 << (b-1): el mismo mapa)
    e.motion.x = x; e.motion.y = y; e.motion.xrel = dx; e.motion.yrel = dy;
    Dispatch(e);
}

void W3dMouseButton(int button, bool down, int x, int y) {
    if (button < 1 || button > 3) return;
    gPhysical[button] = down;
    if (button == 2) { SendButton(2, down, x, y); return; }
    const int other = (button == 1) ? 3 : 1;
    if (down) {
        if (gWait == other && SDL_GetTicks() - gWaitFrom < (Uint32)kMouseChordMs) {
            gWait = 0; gChord = true;   // los dos juntos: el del medio
            SendButton(2, true, x, y);
            return;
        }
        ReleaseWait();
        gWait = button; gWaitFrom = SDL_GetTicks(); gWaitX = x; gWaitY = y;
        return;
    }
    if (gChord) {   // se solto uno de los dos: termina el del medio (el otro, al soltarse, no hace nada)
        if (!gPhysical[1] && !gPhysical[3]) gChord = false;
        SendButton(2, false, x, y);
        return;
    }
    if (gWait == button) ReleaseWait();   // un click corto: el apretar sale ahora, antes del soltar
    SendButton(button, false, x, y);
}

void W3dMouseWheel(int steps, int x, int y) {
    ReleaseWait();
    SDL_Event e; memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEWHEEL;
    e.wheel.timestamp = SDL_GetTicks();
    e.wheel.which = 0;
    e.wheel.y = steps;
    (void)x; (void)y;   // (la rueda va al viewport bajo el puntero: controls.cpp lo lee de SDL_GetMouseState)
    Dispatch(e);
}

void W3dMouseTick() {
    if (gWait && SDL_GetTicks() - gWaitFrom >= (Uint32)kMouseChordMs) ReleaseWait();
}

bool W3dEventMouse() { return gInEvent; }

// ---- teclado ----
extern bool LShiftPressed, LCtrlPressed, LAltPressed;
namespace {
    bool gInKey = false;
    // las imprimibles de la distribucion de EEUU por codigo de uso HID: sin shift / con shift (0 = no es imprimible)
    struct KeyHID { unsigned char use; char normal, withShift; };
    const KeyHID kPrintable[] = {
        {30,'1','!'},{31,'2','@'},{32,'3','#'},{33,'4','$'},{34,'5','%'},{35,'6','^'},{36,'7','&'},{37,'8','*'},
        {38,'9','('},{39,'0',')'},{44,' ',' '},{45,'-','_'},{46,'=','+'},{47,'[','{'},{48,']','}'},{49,'\\','|'},
        {51,';',':'},{52,'\'','"'},{54,',','<'},{55,'.','>'},{56,'/','?'},
        {84,'/','/'},{85,'*','*'},{86,'-','-'},{87,'+','+'}
    };
    // el keycode de SDL: las imprimibles son su ASCII (sin shift: SDLK_a = 'a'); las demas, el scancode (que en SDL ES el
    // codigo de uso HID) con el bit 30
    SDL_Keycode SymFromHID(int use, char* text) {
        text[0] = 0;
        if (use >= 4 && use <= 29) {
            const char c = (char)('a' + use - 4);
            text[0] = LShiftPressed ? (char)(c - 'a' + 'A') : c; text[1] = 0;
            return (SDL_Keycode)c;
        }
        for (unsigned i = 0; i < sizeof(kPrintable) / sizeof(kPrintable[0]); i++) {
            if (kPrintable[i].use != use) continue;
            text[0] = LShiftPressed ? kPrintable[i].withShift : kPrintable[i].normal; text[1] = 0;
            return use >= 84 ? (SDL_Keycode)(use | (1 << 30)) : (SDL_Keycode)kPrintable[i].normal;
        }
        switch (use) {
            case 40: case 88: return SDLK_RETURN;
            case 41: case 53: return SDLK_ESCAPE;   // (53 = la ` ; los teclados compactos ponen ahi el Esc)
            case 42: return SDLK_BACKSPACE;
            case 43: return SDLK_TAB;
            case 76: return SDLK_DELETE;
            case 230: return SDLK_LALT;             // (el editor solo mira el Alt izquierdo)
            case 231: return SDLK_LCTRL;            // (la tecla de Windows: nada mejor que hacer con ella)
        }
        if (use >= 89 && use <= 99) { text[0] = (use == 99) ? '.' : (use == 98) ? '0' : (char)('1' + use - 89); text[1] = 0; }
        return (SDL_Keycode)(use | (1 << 30));
    }
}

void W3dKeyboardKeyHID(int aUse, bool down) {
    char text[2];
    const SDL_Keycode sym = SymFromHID(aUse, text);
    const bool prev = gInKey;
    gInKey = true;
    SDL_Event e; memset(&e, 0, sizeof(e));
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.timestamp = SDL_GetTicks();
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.sym = sym;
    e.key.keysym.mod = (Uint16)((LShiftPressed ? KMOD_LSHIFT : 0) | (LCtrlPressed ? KMOD_LCTRL : 0) | (LAltPressed ? KMOD_LALT : 0));
    Dispatch(e);
    // el texto, como SDL: despues del apretar, y no con Ctrl/Alt (son atajos)
    if (down && text[0] && !LCtrlPressed && !LAltPressed) {
        SDL_Event t; memset(&t, 0, sizeof(t));
        t.type = SDL_TEXTINPUT;
        t.text.timestamp = SDL_GetTicks();
        t.text.text[0] = text[0];
        Dispatch(t);
    }
    gInKey = prev;
}

bool W3dEventKeyboard() { return gInKey; }
bool W3dAnyMousePressed() { return gPressed != 0 || gWait != 0; }
void (*W3dMouseTeleport)(int x, int y) = 0;
