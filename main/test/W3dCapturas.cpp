// Comandos de harness para las capturas de los tutoriales (ver W3dCapturas.h y tools/capturas/README.md).
#include "test/W3dCapturas.h"
#include "app/controles.h"            // SDL_Event + InputUsuarioSDL3: la puerta real de la entrada
#include "app/variables.h"            // window
#include "ViewPorts/ViewPorts.h"      // ViewportBase / ViewportColumn (el arbol del layout)
#include "ViewPorts/ViewPort3D.h"     // Viewport3D (layoutpordefecto)
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/core/UI.h"          // RenglonHeightGS, gapGS, borderGS (las filas del menu)
#include "render/OpcionesRender.h"    // g_redraw
#include <stdio.h>
#include <string.h>

extern ViewportBase* rootViewport;
extern int MenuPantallaW, MenuPantallaH;
// la tecla VERDE del N95 (platform/symbian/src/Whisk3DContainer.cpp): las MISMAS funciones que llama el telefono
void LayoutCiclarViewportActivo(int dir);                              // verde sola (al soltarla)
void LayoutRedimensionarViewportActivo(int dx, int dy, float paso);    // verde + flecha, una vez por cuadro
ViewportBase* LayoutPorDefecto(int w, int h, Viewport3D** vp3dOut);    // LayoutInput.cpp: el del arranque
extern Viewport3D* Viewport3DActive;
void LayoutRenderMenu(int w, int h);
void SetGlobalScale(int scale);        // ViewPorts.cpp: re-deriva las metricas *GS   // LayoutInput.cpp: los desplegables y popups (van encima de todo)

namespace {
    // la ultima posicion de cada puntero: SDL manda el desplazamiento (xrel/yrel) en cada movimiento, y el
    // arrastre del boton de la esquina redimensiona con ESO, no con la posicion
    int gRatonX = 0, gRatonY = 0;
    Uint32 gRatonBotones = 0;
    int gDedoX = 0, gDedoY = 0;

    // UN CUADRO, como el loop del editor entre evento y evento: el hit-test de los botones usa la posicion
    // que les dejo el ultimo dibujado (sin esto, el primer click no le acertaba a la esquina: sx/sy en 0)
    void Cuadro() {
        if (!rootViewport) return;
        rootViewport->Render();
        LayoutRenderMenu(MenuPantallaW, MenuPantallaH);
    }

    bool gPrimero = true;
    void Despachar(SDL_Event& e) {
        if (gPrimero) { gPrimero = false; Cuadro(); }
        g_redraw = true;
        InputUsuarioSDL3(e);
        Cuadro();
    }

    int BotonDe(const std::string& s) {
        if (s == "der" || s == "derecho") return SDL_BUTTON_RIGHT;
        if (s == "medio") return SDL_BUTTON_MIDDLE;
        return SDL_BUTTON_LEFT;
    }

    void Movio(Uint32 which, Uint32 estado, int x, int y, int px, int py) {
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_MOUSEMOTION;
        e.motion.timestamp = SDL_GetTicks();
        e.motion.which = which;
        e.motion.state = estado;
        e.motion.x = x; e.motion.y = y; e.motion.xrel = x - px; e.motion.yrel = y - py;
        Despachar(e);
    }

    void Boton(Uint32 which, int b, bool abajo, int x, int y) {
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = abajo ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.timestamp = SDL_GetTicks();
        e.button.which = which;
        e.button.button = (Uint8)b;
        e.button.state = abajo ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = 1;
        e.button.x = x; e.button.y = y;
        Despachar(e);
    }

    void Dedo(Uint32 tipo, int id, int x, int y, int px, int py) {
        int ww = 0, hh = 0; if (window) SDL_GetWindowSize(window, &ww, &hh);
        if (ww <= 0 || hh <= 0) { ww = MenuPantallaW; hh = MenuPantallaH; }
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = tipo;
        e.tfinger.timestamp = SDL_GetTicks();
        e.tfinger.fingerId = (SDL_FingerID)id;
        // controles.cpp lo pasa a pixeles con el tamanio de la VENTANA: se normaliza con el mismo
        // (+0.5: que el (int) de vuelta caiga en el mismo pixel)
        e.tfinger.x = ((float)x + 0.5f) / (float)ww;  e.tfinger.y = ((float)y + 0.5f) / (float)hh;
        e.tfinger.dx = (float)(x - px) / (float)ww;   e.tfinger.dy = (float)(y - py) / (float)hh;
        e.tfinger.pressure = (tipo == SDL_FINGERUP) ? 0.0f : 1.0f;
        Despachar(e);
    }

    // la rueda va al viewport bajo el cursor, y controles.cpp lo lee de SDL_GetMouseState: se lleva el
    // cursor ahi primero (en el driver 'offscreen' no hay cursor de verdad: solo cambia el estado de SDL)
    void Rueda(int pasos, int x, int y) {
        if (window) SDL_WarpMouseInWindow(window, x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_MOUSEWHEEL;
        e.wheel.timestamp = SDL_GetTicks();
        e.wheel.which = 0;
        e.wheel.y = pasos;
        Despachar(e);
    }

    void InfoHojas(ViewportBase* n) {
        if (!n) return;
        if (!n->isLeaf()) {
            InfoHojas(((ViewportColumn*)n)->childA);
            InfoHojas(((ViewportColumn*)n)->childB);
            return;
        }
        n->ActualizarBarra();
        int bx = -1, by = -1, bw = 0, bh = 0;
        if (!n->BarButtons.empty()) {
            Button* b = n->BarButtons[0];
            bx = b->sx; by = b->sy; bw = b->width; bh = b->height;
        }
        printf("@cap vp kind=%d x=%d y=%d w=%d h=%d esquina=%d,%d,%d,%d barra=%d botones=%d activo=%d\n",
               n->ViewportKind(), n->x, n->y, n->width, n->height, bx, by, bw, bh,
               n->BarHeight(), (int)n->BarButtons.size(), n == viewPortActive ? 1 : 0);
        for (size_t i = 0; i < n->BarButtons.size(); i++) {
            Button* b = n->BarButtons[i];
            printf("@cap boton kind=%d i=%d x=%d y=%d w=%d h=%d texto=%s\n",
                   n->ViewportKind(), (int)i, b->sx, b->sy, b->width, b->height, b->text.c_str());
        }
    }

    void InfoMenu(PopupMenu* m, int nivel) {
        if (!m || !m->abierto) return;
        printf("@cap menu nivel=%d x=%d y=%d w=%d h=%d filas=%d scroll=%d titulo=%s\n",
               nivel, m->x, m->y, m->width, m->height, (int)m->items.size(), m->scroll, m->titulo.c_str());
        const int fila = RenglonHeightGS + gapGS;
        const int oy = m->titulo.empty() ? 0 : fila;
        const int visibles = m->FilasVisibles();
        for (int i = 0; i < (int)m->items.size(); i++) {
            const int v = i - m->scroll;
            if (v < 0 || v >= visibles) continue;
            const int cy = m->y + borderGS + oy + v * fila + RenglonHeightGS / 2;
            printf("@cap item nivel=%d i=%d id=%d cx=%d cy=%d texto=%s\n",
                   nivel, i, m->items[i]->id, m->x + m->width / 2, cy, m->items[i]->text.c_str());
        }
        if (m->submenuAbierto) InfoMenu(m->submenuAbierto, nivel + 1);
    }
}

bool W3dCapturasCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "raton") {
        std::string que, cual; int x = 0, y = 0; ss >> que >> x >> y >> cual;
        const int b = BotonDe(cual);
        if (que == "move") {
            Movio(0, gRatonBotones, x, y, gRatonX, gRatonY);
        } else if (que == "down" || que == "up") {
            if (x != gRatonX || y != gRatonY) Movio(0, gRatonBotones, x, y, gRatonX, gRatonY);
            const bool abajo = (que == "down");
            if (abajo) gRatonBotones |= SDL_BUTTON(b); else gRatonBotones &= ~SDL_BUTTON(b);
            Boton(0, b, abajo, x, y);
        } else { err = "raton: uso: raton down|move|up <x> <y> [izq|medio|der]"; return false; }
        gRatonX = x; gRatonY = y;
        return true;
    }
    if (cmd == "dedo") {
        std::string que; int x = 0, y = 0, id = 1; ss >> que >> x >> y; if (!(ss >> id)) id = 1;
        // el orden de SDL: primero el mouse sintetizado (which = SDL_TOUCH_MOUSEID), despues el del dedo
        if (que == "down") {
            Movio(SDL_TOUCH_MOUSEID, 0, x, y, x, y);
            Boton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x, y);
            Dedo(SDL_FINGERDOWN, id, x, y, x, y);
        } else if (que == "move") {
            Movio(SDL_TOUCH_MOUSEID, SDL_BUTTON_LMASK, x, y, gDedoX, gDedoY);
            Dedo(SDL_FINGERMOTION, id, x, y, gDedoX, gDedoY);
        } else if (que == "up") {
            Boton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, x, y);
            Dedo(SDL_FINGERUP, id, x, y, gDedoX, gDedoY);
        } else { err = "dedo: uso: dedo down|move|up <x> <y> [id]"; return false; }
        gDedoX = x; gDedoY = y;
        return true;
    }
    if (cmd == "rueda") {
        int pasos = 0, x = gRatonX, y = gRatonY; ss >> pasos >> x >> y;
        if (pasos == 0) { err = "rueda: uso: rueda <pasos> <x> <y> (positivo = arriba)"; return false; }
        Rueda(pasos, x, y);
        return true;
    }
    if (cmd == "tecla") {
        std::string nombre, como; ss >> nombre >> como;
        const SDL_Keycode k = SDL_GetKeyFromName(nombre.c_str());
        if (k == SDLK_UNKNOWN) { err = "tecla: SDL no conoce '" + nombre + "'"; return false; }
        for (int paso = 0; paso < 2; paso++) {
            const bool abajo = (paso == 0);
            if ((como == "down" && !abajo) || (como == "up" && abajo)) continue;
            SDL_Event e; memset(&e, 0, sizeof(e));
            e.type = abajo ? SDL_KEYDOWN : SDL_KEYUP;
            e.key.timestamp = SDL_GetTicks();
            e.key.state = abajo ? SDL_PRESSED : SDL_RELEASED;
            e.key.keysym.sym = k;
            e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
            Despachar(e);
        }
        return true;
    }
    if (cmd == "cuadro") { Cuadro(); return true; }
    // la escala de la interfaz, como la de Ajustes (1 = como se ve en el N95)
    if (cmd == "escala") {
        int v = 0; ss >> v;
        if (v < 1 || v > 6) { err = "escala: uso: escala <1..6>"; return false; }
        SetGlobalScale(v);
        if (rootViewport) rootViewport->Resize(MenuPantallaW, MenuPantallaH);
        Cuadro();
        return true;
    }
    // el layout por defecto PARA EL TAMANIO ACTUAL, como si el editor hubiera arrancado asi (el harness abre con
    // otro tamanio y 'ventana' solo lo redimensiona): en 320x240 son 2 viewports, la Vista 3D y Propiedades, como
    // en el N95. El arbol viejo no se borra (algun puntero del input podria seguir mirandolo; el harness dura poco).
    if (cmd == "layoutpordefecto") {
        Viewport3D* vp3d = NULL;
        rootViewport = LayoutPorDefecto(MenuPantallaW, MenuPantallaH, &vp3d);
        viewPortActive = vp3d;
        Viewport3DActive = vp3d;
        rootViewport->Resize(MenuPantallaW, MenuPantallaH);
        g_redraw = true;
        Cuadro();
        return true;
    }
    // la division de la raiz del layout (0..1, lo que ocupa el de la izquierda / el de arriba). En el N95 las
    // demos van con 2 viewports: la Vista 3D al 55 % y Propiedades al 45 %.
    if (cmd == "division") {
        float f = 0; ss >> f;
        if (!rootViewport || rootViewport->isLeaf()) { err = "division: la raiz del layout no esta dividida"; return false; }
        if (f < 0.05f || f > 0.95f) { err = "division: uso: division <0.05..0.95>"; return false; }
        if (rootViewport->ContainerKind() == 1) ((ViewportRow*)rootViewport)->splitFrac = f;
        else                                    ((ViewportColumn*)rootViewport)->splitFrac = f;
        rootViewport->Resize(MenuPantallaW, MenuPantallaH);
        Cuadro();
        return true;
    }
    // la tecla VERDE del N95: 'verde' = un toque (pasa al viewport siguiente); 'verde <dx> <dy> [cuadros]' = verde
    // mantenida + una flecha durante esos cuadros (el telefono redimensiona un paso de 0.01 por cuadro)
    if (cmd == "verde") {
        int dx = 0, dy = 0, cuadros = 1;
        if (!(ss >> dx >> dy)) { LayoutCiclarViewportActivo(1); g_redraw = true; Cuadro(); return true; }
        if (!(ss >> cuadros)) cuadros = 1;
        for (int i = 0; i < cuadros; i++) { LayoutRedimensionarViewportActivo(dx, dy, 0.01f); g_redraw = true; Cuadro(); }
        return true;
    }
    if (cmd == "capinfo") {
        if (!rootViewport) { err = "capinfo: no hay layout de viewports"; return false; }
        printf("@cap pantalla w=%d h=%d\n", MenuPantallaW, MenuPantallaH);
        InfoHojas(rootViewport);
        InfoMenu(MenuAbierto, 0);
        return true;
    }
    manejado = false;
    return false;
}
