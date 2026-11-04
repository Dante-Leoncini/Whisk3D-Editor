// El mouse de las plataformas sin SDL -> SDL_Event -> InputUsuarioSDL3. Ver W3dRatonEditor.h. C++03 (Symbian).
#include "W3dRatonEditor.h"
#include "controles.h"                // SDL_Event (o W3dSdlMinimo.h) + InputUsuarioSDL3
#include "render/OpcionesRender.h"    // g_redraw
#include <string.h>

namespace {
    bool gEnEvento = false;
    unsigned gApretados = 0;          // los botones que el editor cree apretados (bit 1 << (boton-1))
    // el acorde izquierdo + derecho = boton del medio
    int  gEspera = 0;                 // boton que bajo y espera a ver si llega el otro (0 = ninguno)
    Uint32 gEsperaDesde = 0;
    int  gEsperaX = 0, gEsperaY = 0;
    bool gAcorde = false;             // izq + der apretados = el del medio esta abajo
    bool gFisico[4] = { false, false, false, false };   // como estan de verdad (1 izq, 2 medio, 3 der)

    void Despachar(SDL_Event& e) {
        const bool antes = gEnEvento;
        gEnEvento = true;
        g_redraw = true;
        InputUsuarioSDL3(e);
        gEnEvento = antes;
    }

    void Boton(int b, bool abajo, int x, int y) {
        const unsigned bit = 1u << (b - 1);
        if (abajo == ((gApretados & bit) != 0)) return;   // (ya estaba asi)
        if (abajo) gApretados |= bit; else gApretados &= ~bit;
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = abajo ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.timestamp = SDL_GetTicks();
        e.button.which = 0;   // un mouse de verdad (no el del tactil)
        e.button.button = (Uint8)b;
        e.button.state = abajo ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = 1;
        e.button.x = x; e.button.y = y;
        Despachar(e);
    }

    void SoltarEspera() {
        if (!gEspera) return;
        const int b = gEspera; gEspera = 0;
        Boton(b, true, gEsperaX, gEsperaY);
    }
}

void W3dRatonMovio(int x, int y, int dx, int dy) {
    SoltarEspera();   // (moverse ya decide: era un boton solo)
    SDL_Event e; memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEMOTION;
    e.motion.timestamp = SDL_GetTicks();
    e.motion.which = 0;
    e.motion.state = gApretados;   // (SDL_BUTTON(b) = 1 << (b-1): el mismo mapa)
    e.motion.x = x; e.motion.y = y; e.motion.xrel = dx; e.motion.yrel = dy;
    Despachar(e);
}

void W3dRatonBoton(int boton, bool abajo, int x, int y) {
    if (boton < 1 || boton > 3) return;
    gFisico[boton] = abajo;
    if (boton == 2) { Boton(2, abajo, x, y); return; }
    const int otro = (boton == 1) ? 3 : 1;
    if (abajo) {
        if (gEspera == otro && SDL_GetTicks() - gEsperaDesde < (Uint32)kRatonAcordeMs) {
            gEspera = 0; gAcorde = true;   // los dos juntos: el del medio
            Boton(2, true, x, y);
            return;
        }
        SoltarEspera();
        gEspera = boton; gEsperaDesde = SDL_GetTicks(); gEsperaX = x; gEsperaY = y;
        return;
    }
    if (gAcorde) {   // se solto uno de los dos: termina el del medio (el otro, al soltarse, no hace nada)
        if (!gFisico[1] && !gFisico[3]) gAcorde = false;
        Boton(2, false, x, y);
        return;
    }
    if (gEspera == boton) SoltarEspera();   // un click corto: el apretar sale ahora, antes del soltar
    Boton(boton, false, x, y);
}

void W3dRatonRueda(int pasos, int x, int y) {
    SoltarEspera();
    SDL_Event e; memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEWHEEL;
    e.wheel.timestamp = SDL_GetTicks();
    e.wheel.which = 0;
    e.wheel.y = pasos;
    (void)x; (void)y;   // (la rueda va al viewport bajo el puntero: controles.cpp lo lee de SDL_GetMouseState)
    Despachar(e);
}

void W3dRatonTick() {
    if (gEspera && SDL_GetTicks() - gEsperaDesde >= (Uint32)kRatonAcordeMs) SoltarEspera();
}

bool W3dEventoRaton() { return gEnEvento; }

// ---- teclado ----
extern bool LShiftPressed, LCtrlPressed, LAltPressed;
namespace {
    bool gEnTecla = false;
    // las imprimibles de la distribucion de EEUU por codigo de uso HID: sin shift / con shift (0 = no es imprimible)
    struct TeclaHID { unsigned char uso; char normal, conShift; };
    const TeclaHID kImprimibles[] = {
        {30,'1','!'},{31,'2','@'},{32,'3','#'},{33,'4','$'},{34,'5','%'},{35,'6','^'},{36,'7','&'},{37,'8','*'},
        {38,'9','('},{39,'0',')'},{44,' ',' '},{45,'-','_'},{46,'=','+'},{47,'[','{'},{48,']','}'},{49,'\\','|'},
        {51,';',':'},{52,'\'','"'},{54,',','<'},{55,'.','>'},{56,'/','?'},
        {84,'/','/'},{85,'*','*'},{86,'-','-'},{87,'+','+'}
    };
    // el keycode de SDL: las imprimibles son su ASCII (sin shift: SDLK_a = 'a'); las demas, el scancode (que en SDL ES el
    // codigo de uso HID) con el bit 30
    SDL_Keycode SymDesdeHID(int uso, char* texto) {
        texto[0] = 0;
        if (uso >= 4 && uso <= 29) {
            const char c = (char)('a' + uso - 4);
            texto[0] = LShiftPressed ? (char)(c - 'a' + 'A') : c; texto[1] = 0;
            return (SDL_Keycode)c;
        }
        for (unsigned i = 0; i < sizeof(kImprimibles) / sizeof(kImprimibles[0]); i++) {
            if (kImprimibles[i].uso != uso) continue;
            texto[0] = LShiftPressed ? kImprimibles[i].conShift : kImprimibles[i].normal; texto[1] = 0;
            return uso >= 84 ? (SDL_Keycode)(uso | (1 << 30)) : (SDL_Keycode)kImprimibles[i].normal;
        }
        switch (uso) {
            case 40: case 88: return SDLK_RETURN;
            case 41: case 53: return SDLK_ESCAPE;   // (53 = la ` ; los teclados compactos ponen ahi el Esc)
            case 42: return SDLK_BACKSPACE;
            case 43: return SDLK_TAB;
            case 76: return SDLK_DELETE;
            case 230: return SDLK_LALT;             // (el editor solo mira el Alt izquierdo)
            case 231: return SDLK_LCTRL;            // (la tecla de Windows: nada mejor que hacer con ella)
        }
        if (uso >= 89 && uso <= 99) { texto[0] = (uso == 99) ? '.' : (uso == 98) ? '0' : (char)('1' + uso - 89); texto[1] = 0; }
        return (SDL_Keycode)(uso | (1 << 30));
    }
}

void W3dTecladoTeclaHID(int aUso, bool abajo) {
    char texto[2];
    const SDL_Keycode sym = SymDesdeHID(aUso, texto);
    const bool antes = gEnTecla;
    gEnTecla = true;
    SDL_Event e; memset(&e, 0, sizeof(e));
    e.type = abajo ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.timestamp = SDL_GetTicks();
    e.key.state = abajo ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.sym = sym;
    e.key.keysym.mod = (Uint16)((LShiftPressed ? KMOD_LSHIFT : 0) | (LCtrlPressed ? KMOD_LCTRL : 0) | (LAltPressed ? KMOD_LALT : 0));
    Despachar(e);
    // el texto, como SDL: despues del apretar, y no con Ctrl/Alt (son atajos)
    if (abajo && texto[0] && !LCtrlPressed && !LAltPressed) {
        SDL_Event t; memset(&t, 0, sizeof(t));
        t.type = SDL_TEXTINPUT;
        t.text.timestamp = SDL_GetTicks();
        t.text.text[0] = texto[0];
        Despachar(t);
    }
    gEnTecla = antes;
}

bool W3dEventoTeclado() { return gEnTecla; }
bool W3dRatonAlgunoApretado() { return gApretados != 0 || gEspera != 0; }
void (*W3dRatonTeletransportar)(int x, int y) = 0;
