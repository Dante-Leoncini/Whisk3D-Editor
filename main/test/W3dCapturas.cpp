// Comandos de harness para las capturas de los tutoriales (ver W3dCapturas.h y tools/capturas/README.md).
#include "test/W3dCapturas.h"
#include "app/controles.h"            // SDL_Event + InputUsuarioSDL3: la puerta real de la entrada
#include "app/variables.h"            // window
#include "ViewPorts/ViewPorts.h"      // ViewportBase / ViewportColumn (el arbol del layout)
#include "ViewPorts/ViewPort3D.h"     // Viewport3D (layoutpordefecto)
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/core/UI.h"          // RenglonHeightGS, gapGS, borderGS (las filas del menu)
#include "render/OpcionesRender.h"    // g_redraw
#include "objects/Objects.h"          // g_editMesh, EditSelectMode (caras)
#include "objects/Mesh.h"             // Mesh (caras)
#include "WhiskUI/text/font.h"         // WhiskFont->Soporta (cobertura)
#include "config/W3dLang.h"           // los idiomas (cobertura)
#include "w3dFilesystem.h"            // leer res/idiomas (cobertura)
#include <set>
#include "objects/EditMesh.h"         // EditMesh (caras)
#include <stdio.h>
#include <string.h>
#include <map>
#include <math.h>
#include "importers/stl.h"         // ExportSTL (stl)
#include "objects/ObjectMode.h"     // SetPosicion / SetRotacion / SetScale / CiclarEjeTransform (n95 transformar)
#include "ViewPorts/Gizmo.h"         // GizmoManijaEnPantalla (capinfo: las manijas)
#include "importers/import_obj.h"   // CargarTodasTexturasPendientes (texturas)
#include "undo/Undo.h"               // UndoConAgregar / UndoModAgregar (agregar)
#include "ViewPorts/Outliner.h"      // las filas (capinfo)
#include "ViewPorts/Properties.h"    // las tarjetas y filas (capinfo)
#include "ViewPorts/PopUp/NumPad.h"  // NumPadTeclaCentro (capinfo)
#include "WhiskUI/widgets/Tab.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
extern int OutlinerFilaDeObjeto(Object* o);   // Outliner.cpp: la fila visible de un objeto en la escena (-1)

extern ViewportBase* rootViewport;
extern int MenuPantallaW, MenuPantallaH;
// la tecla VERDE del N95 (platform/symbian/src/Whisk3DContainer.cpp): las MISMAS funciones que llama el telefono
void LayoutCiclarViewportActivo(int dir);                              // verde sola (al soltarla)
void LayoutRedimensionarViewportActivo(int dx, int dy, float paso);    // verde + flecha, una vez por cuadro
ViewportBase* LayoutPorDefecto(int w, int h, Viewport3D** vp3dOut);    // LayoutInput.cpp: el del arranque
extern Viewport3D* Viewport3DActive;
extern bool LShiftPressed, LAltPressed, LCtrlPressed;   // controles.cpp (los modificadores que mira el viewport 3D)
void LayoutRenderMenu(int w, int h);
void SetGlobalScale(int scale);        // ViewPorts.cpp: re-deriva las metricas *GS   // LayoutInput.cpp: los desplegables y popups (van encima de todo)

namespace {
    // la ultima posicion de cada puntero: SDL manda el desplazamiento (xrel/yrel) en cada movimiento, y el
    // arrastre del boton de la esquina redimensiona con ESO, no con la posicion
    int gRatonX = 0, gRatonY = 0;
    Uint32 gRatonBotones = 0;
    std::map<int, std::pair<int, int> > gDedos;   // la ultima posicion de cada dedo apoyado (por id)
    int gModoN95 = 0;   // el transform del keypad en curso (n95 transformar): 0 nada, 1 mover, 2 rotar, 3 escalar

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

    // el editor lee la posicion del mouse de SDL (SDL_GetMouseState) al apretar un boton (GuardarMousePos): el delta
    // del arrastre se mide desde ahi. Los eventos sintetizados no la tocan: se lleva el cursor de SDL a mano (en el
    // driver 'offscreen' solo cambia ese estado). Sin esto el primer movimiento de un arrastre venia desde (0, 0)
    // y la vista pegaba un salto enorme.
    void LlevarCursor(int x, int y) { if (window) SDL_WarpMouseInWindow(window, (float)x, (float)y); }

    void Movio(Uint32 which, Uint32 estado, int x, int y, int px, int py) {
        LlevarCursor(x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_MOUSEMOTION;
        e.motion.timestamp = SDL_GetTicks();
        e.motion.which = which;
        e.motion.state = estado;
        e.motion.x = x; e.motion.y = y; e.motion.xrel = x - px; e.motion.yrel = y - py;
        Despachar(e);
    }

    int gClicks = 1;   // 'raton doble': el 2do click llega con clicks = 2 (SDL), que es lo que controles.cpp mira
    void Boton(Uint32 which, int b, bool abajo, int x, int y) {
        LlevarCursor(x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = abajo ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.timestamp = SDL_GetTicks();
        e.button.which = which;
        e.button.button = (Uint8)b;
        e.button.state = abajo ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = (Uint8)gClicks;
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

    // la misma cuenta que el harness del Outliner (W3dPruebasOutliner.cpp, PuntoDeNombre): el centro del NOMBRE de la fila
    void InfoFilas(Outliner* o, Object* raiz) {
        if (!raiz) return;
        for (size_t i = 0; i < raiz->Childrens.size(); i++) {
            Object* ob = raiz->Childrens[i];
            const int fila = OutlinerFilaDeObjeto(ob);
            if (fila >= 0) {
                int prof = 0;
                for (Object* p = ob->Parent; p && p != SceneCollection; p = p->Parent) prof++;
                const int x = o->x + marginGS + o->PosX + (prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
                const int y = o->y + borderGS + o->PosY + o->BarTopOffset() + fila * (int)RenglonHeightGS + (int)RenglonHeightGS / 2;
                const int reserva = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;   // (Outliner.cpp: la barra de desplazamiento)
                const int ojo = o->x + o->width - 2 * IconSizeGS - gapGS - marginGS - borderGS - reserva + IconSizeGS / 2;
                const int cam = o->x + o->width - IconSizeGS - marginGS - borderGS - reserva + IconSizeGS / 2;
                const int flecha = o->x + marginGS + o->PosX + prof * (IconSizeGS + gapGS) + IconSizeGS / 2;
                if (y >= o->y && y < o->y + o->height)
                    printf("@cap fila x=%d y=%d prof=%d ojo=%d camara=%d flecha=%d elegido=%d nombre=%s\n", x, y, prof, ojo, cam, flecha,
                           ob->select ? 1 : 0, ob->name.c_str());
            }
            InfoFilas(o, ob);
        }
    }

    void InfoObjetos(Viewport3D* vp, Object* raiz) {
        if (!raiz) return;
        for (size_t i = 0; i < raiz->Childrens.size(); i++) {
            Object* ob = raiz->Childrens[i];
            float sx = 0, sy = 0;
            if (ob->getType() != ObjectType::collection && vp->ProyectarPunto(ob->GetGlobalPositionBase(), sx, sy))
                printf("@cap objeto x=%d y=%d elegido=%d nombre=%s\n", vp->x + (int)(sx + 0.5f), vp->y + (int)(sy + 0.5f),
                       ob->select ? 1 : 0, ob->name.c_str());
            InfoObjetos(vp, ob);
        }
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
        // el boton del menu View de la Vista 3D (en la PC es solo un icono: no se lo encuentra por el texto)
        if (n->ViewportKind() == 1) {
            Button* bv = BarRolBtn(n->BarButtons, BR_View);
            if (bv && bv->visible) printf("@cap botonvista x=%d y=%d w=%d h=%d\n", bv->sx, bv->sy, bv->width, bv->height);
        }
        // el candado de la vista de camara (si se ve): para apuntarle con el mouse o el dedo
        if (n->ViewportKind() == 1 && ((Viewport3D*)n)->CandadoCamaraVisible()) {
            int rx = 0, ry = 0, lado = 0; ((Viewport3D*)n)->CandadoCamaraRect(rx, ry, lado);
            printf("@cap candado x=%d y=%d lado=%d\n", n->x + rx, n->y + ry, lado);
        }
        for (size_t i = 0; i < n->BarButtons.size(); i++) {
            Button* b = n->BarButtons[i];
            printf("@cap boton kind=%d i=%d rol=%d visible=%d x=%d y=%d w=%d h=%d texto=%s\n",
                   n->ViewportKind(), (int)i, b->rol, b->visible ? 1 : 0, b->sx, b->sy, b->width, b->height, b->text.c_str());
        }
        // la BARRA DE ABAJO (toolbar): Mover / Rotar / Escalar... (las ultimas acciones), la tilde y la cruz
        if (n->ToolbarVisible()) {
            n->ToolbarActualizar();
            for (size_t i = 0; i < n->ToolButtons.size(); i++) {
                Button* b = n->ToolButtons[i];
                if (!b->visible) continue;
                printf("@cap herramienta kind=%d i=%d rol=%d x=%d y=%d w=%d h=%d texto=%s\n",
                       n->ViewportKind(), (int)i, b->rol, b->sx, b->sy, b->width, b->height, b->text.c_str());
            }
        }
        // las MANIJAS del gizmo (coords de PANTALLA): 0..2 ejes X/Y/Z, 3..5 planos, 6 centro, 7 anillo de la vista, 8 libre
        if (n->ViewportKind() == 1) {
            for (int k = GizmoEjeX; k <= GizmoLibre; k++) {
                float sx = 0, sy = 0;
                if (GizmoManijaEnPantalla((Viewport3D*)n, k, sx, sy))
                    printf("@cap gizmo manija=%d x=%d y=%d modo=%d\n", k, n->x + (int)(sx + 0.5f), n->y + (int)(sy + 0.5f), GizmoModo());
            }
        }
        // los OBJETOS en la Vista 3D: donde cae el origen de cada uno en pantalla (para apuntarles: cajas, clicks)
        if (n->ViewportKind() == 1) InfoObjetos((Viewport3D*)n, SceneCollection);
        // las FILAS del Outliner (la vista Escena): donde cae el nombre de cada objeto que se ve
        if (n->ViewportKind() == 2 && ((Outliner*)n)->vista == OUT_VISTA_ESCENA) InfoFilas((Outliner*)n, SceneCollection);
        // PROPIEDADES: las pestanias, las tarjetas (su titulo) y las filas, con el centro del valor y del rombo.
        // El MISMO recorrido que Properties::ClickEn (si no, un click del harness cae en otra fila).
        if (n->ViewportKind() == 3) {
            Properties* P = (Properties*)n;
            for (size_t i = 0; i < P->BarTabs.size(); i++) {
                Tab* t = P->BarTabs[i];
                if (t->visible) printf("@cap pestania i=%d x=%d y=%d w=%d h=%d activa=%d\n", (int)i, t->sx, t->sy, t->width, t->height, t->activa ? 1 : 0);
            }
            int yCursor = P->y + P->BarTopOffset() + P->PosY + borderGS + RenglonHeightGS + gapGS;
            for (size_t i = 0; i < P->GroupProperties.size(); i++) {
                GroupPropertie* g = P->GroupProperties[i];
                if (!g->visible) continue;
                const int hCab = borderGS + RenglonHeightGS + gapGS;
                const int x0 = P->x + P->PosX + borderGS;
                printf("@cap tarjeta i=%d x=%d y=%d w=%d h=%d abierta=%d titulo=%s\n", (int)i, x0, yCursor, g->width, hCab, g->open ? 1 : 0, g->name.c_str());
                if (g->open) {
                    int yFila = yCursor + hCab;
                    const int xDer = P->x + P->PosX + borderGS * 2 + g->width - bordersGS;
                    for (size_t j = 0; j < g->properties.size(); j++) {
                        PropertieBase* prop = g->properties[j];
                        const int hFila = prop->Resize(g->width);
                        if (hFila > 0) {
                            const int finValor = xDer - (g->keyBtnCol > 0 ? g->keyBtnCol : 0);
                            const int iniValor = x0 + g->colEtiqueta;
                            const int rx = (prop->AnimProp() >= 0 && g->keyBtnCol > 0) ? xDer - g->keyBtnCol / 2 : -1;
                            printf("@cap prop g=%d j=%d y=%d h=%d tipo=%d valor=%d rombo=%d nombre=%s\n", (int)i, (int)j, yFila + hFila / 2, hFila,
                                   (int)prop->GetType(), (iniValor + finValor) / 2, rx, prop->name.c_str());
                        }
                        yFila += hFila;
                    }
                }
                yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
            }
        }
    }

    void InfoMenu(PopupMenu* m, int nivel) {
        if (!m || !m->abierto) return;
        printf("@cap menu nivel=%d x=%d y=%d w=%d h=%d filas=%d scroll=%d sel=%d titulo=%s\n",
               nivel, m->x, m->y, m->width, m->height, (int)m->items.size(), m->scroll, m->selectIndex, m->titulo.c_str());
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
        } else if (que == "doble") {   // doble click: dos clicks seguidos, el segundo con clicks = 2
            if (x != gRatonX || y != gRatonY) Movio(0, gRatonBotones, x, y, gRatonX, gRatonY);
            for (int k = 1; k <= 2; k++) {
                gClicks = k;
                gRatonBotones |= SDL_BUTTON(b); Boton(0, b, true, x, y);
                gRatonBotones &= ~SDL_BUTTON(b); Boton(0, b, false, x, y);
            }
            gClicks = 1;
        } else { err = "raton: uso: raton down|move|up|doble <x> <y> [izq|medio|der]"; return false; }
        gRatonX = x; gRatonY = y;
        return true;
    }
    if (cmd == "dedo") {
        std::string que; int x = 0, y = 0, id = 1; ss >> que >> x >> y; if (!(ss >> id)) id = 1;
        // el orden de SDL: primero el mouse sintetizado (which = SDL_TOUCH_MOUSEID), despues el del dedo. SDL
        // sintetiza el mouse solo con el PRIMER dedo (id 1): los demas (dos dedos = zoom y paneo) son solo dedo
        const bool primero = (id == 1);
        std::map<int, std::pair<int, int> >::iterator ant = gDedos.find(id);
        const int px = (ant != gDedos.end()) ? ant->second.first : x;
        const int py = (ant != gDedos.end()) ? ant->second.second : y;
        if (que == "down") {
            if (primero) { Movio(SDL_TOUCH_MOUSEID, 0, x, y, x, y); Boton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x, y); }
            Dedo(SDL_FINGERDOWN, id, x, y, x, y);
        } else if (que == "move") {
            if (primero) Movio(SDL_TOUCH_MOUSEID, SDL_BUTTON_LMASK, x, y, px, py);
            Dedo(SDL_FINGERMOTION, id, x, y, px, py);
        } else if (que == "up") {
            if (primero) Boton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, x, y);
            Dedo(SDL_FINGERUP, id, x, y, px, py);
        } else { err = "dedo: uso: dedo down|move|up <x> <y> [id]"; return false; }
        if (que == "up") gDedos.erase(id); else gDedos[id] = std::make_pair(x, y);
        return true;
    }
    // las teclas modificadoras MANTENIDAS mientras se usa el mouse: 'mod shift', 'mod alt shift', 'mod' (ninguna).
    // El viewport 3D las lee del estado de SDL (Shift + boton del medio = desplazar; Alt + izquierdo = orbitar), que
    // los eventos de tecla sintetizados no tocan
    if (cmd == "mod") {
        int m = KMOD_NONE; bool shift = false, alt = false, ctrl = false; std::string t;
        while (ss >> t) {
            if (t == "shift") { m |= KMOD_LSHIFT; shift = true; }
            else if (t == "alt") { m |= KMOD_LALT; alt = true; }
            else if (t == "ctrl") { m |= KMOD_LCTRL; ctrl = true; }
            else { err = "mod: uso: mod [shift] [alt] [ctrl]  (sin nada: ninguna)"; return false; }
        }
        SDL_SetModState((SDL_Keymod)m);
        LShiftPressed = shift; LAltPressed = alt; LCtrlPressed = ctrl;   // (Ctrl + A mira LCtrlPressed)
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
        for (size_t i = 0; i < nombre.size(); i++) if (nombre[i] == '_') nombre[i] = ' ';   // "Keypad_7" = "Keypad 7"
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
            e.key.keysym.mod = (Uint16)SDL_GetModState();   // los que dejo 'mod' (Ctrl + A = 'mod ctrl' y 'tecla a')
            Despachar(e);
        }
        return true;
    }
    if (cmd == "cuadro") { Cuadro(); return true; }
    // 'outliner n95 mover|visible|render': las teclas 1 / 2 / 3 del N95 con el Outliner activo (las MISMAS llamadas que
    // platform/symbian/src/w3dlayout.cpp: W3dOutlinerMoverToggle / ToggleVisible / ToggleRender).
    // 'outliner tecla <Up|Down|Left|Right|Enter|Cancel>': una tecla del N95 al panel activo (LayoutTeclaPanelActivo).
    if (cmd == "outliner") {
        std::string que, cual; ss >> que >> cual;
        if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 2) { err = "outliner: el viewport activo no es un Outliner"; return false; }
        Outliner* out = (Outliner*)viewPortActive;
        if (que == "n95") {
            if (cual == "mover") {
                if (out->ModoMover()) out->MoverConfirmar();
                else if (out->moviendoRec) out->MoverRecConfirmar();
                else out->MoverIniciar();
            } else if (cual == "visible") { extern void ChangeVisibilityObj(); ChangeVisibilityObj(); }
            else if (cual == "render") { extern void ChangeRenderizableObj(); ChangeRenderizableObj(); }
            else { err = "outliner n95: mover|visible|render"; return false; }
        } else if (que == "tecla") {   // (LayoutKey, LayoutInput.h: los codigos propios del panel, no los de SDL)
            const int k = (cual == "Up") ? 0 : (cual == "Down") ? 1 : (cual == "Left") ? 2 : (cual == "Right") ? 3 :
                          (cual == "Enter") ? 4 : (cual == "Cancel") ? 5 : -1;
            if (k < 0) { err = "outliner tecla: Up|Down|Left|Right|Enter|Cancel"; return false; }
            { extern bool LayoutTeclaPanelActivo(int tecla); LayoutTeclaPanelActivo(k); }
        } else { err = "outliner: uso: outliner n95 <mover|visible|render> | outliner tecla <tecla>"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'tick': el reloj de los controles (ControlesTick: las pulsaciones largas tactiles), como en cada cuadro del editor
    if (cmd == "tick") { extern void ControlesTick(); ControlesTick(); g_redraw = true; Cuadro(); return true; }
    // 'menuabierto si|no': que haya (o no) un menu desplegado ahora (la pulsacion larga abre el contextual)
    if (cmd == "menuabierto") {
        std::string q; ss >> q;
        extern PopupMenu* MenuAbierto;
        const bool hay = MenuAbierto && MenuAbierto->abierto;
        printf("      [menuabierto] %s%s%s\n", hay ? "si" : "no", hay ? ": " : "", hay ? MenuAbierto->titulo.c_str() : "");
        if ((q == "si") != hay) { err = std::string("menuabierto: esperaba ") + q; return false; }
        return true;
    }
    // 'texto <caracteres>': lo tipeado (SDL_TEXTINPUT, como el teclado de verdad despues de la tecla): los numeros de
    // un transform ("R Z 90"), un nombre... Las teclas sueltas ('tecla') no escriben: solo mandan la tecla.
    if (cmd == "texto") {
        std::string t; ss >> t;
        if (t.empty()) { err = "texto: uso: texto <caracteres>"; return false; }
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_TEXTINPUT;
        e.text.timestamp = SDL_GetTicks();
        strncpy(e.text.text, t.c_str(), sizeof(e.text.text) - 1);
        Despachar(e);
        return true;
    }
    // 'agregar sonido': Anadir > Sonido (en el cursor 3D, elegido). 'agregar restriccion <0..3>': la restriccion de ese
    // tipo (W3dConstraintTipo) en el objeto activo, por la puerta del boton Anadir de Propiedades. 'agregar
    // modificador <n>': el modificador de ese ModifierType en la malla activa (tambien Esqueleto, que 'modadd' no tiene).
    if (cmd == "agregar") {
        std::string que; ss >> que;
        if (que == "sonido") { extern void AddSonido(); AddSonido(); }
        else if (que == "restriccion" || que == "modificador") {
            int t = -1; ss >> t;
            if (!ObjActivo || t < 0) { err = "agregar: sin objeto activo o sin tipo"; return false; }
            if (que == "restriccion") UndoConAgregar(ObjActivo, t);
            else {
                if (ObjActivo->getType() != ObjectType::mesh) { err = "agregar modificador: el activo no es una malla"; return false; }
                UndoModAgregar((Mesh*)ObjActivo, t);
                ((Mesh*)ObjActivo)->GenerarMallaModificada();
            }
        } else { err = "agregar: uso: agregar sonido | agregar restriccion <tipo> | agregar modificador <tipo>"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'vista3d sombreado <n>': el modo de la Vista 3D activa (RenderType: 0 Solido, 1 Vista previa de material,
    // 2 Renderizado...). 'vista3d huesos <0|1>': dibujar o no los esqueletos (Superposiciones > Objetos).
    // 'vista3d gizmo <0|1>': Superposiciones > Gizmo (el N95 arranca sin gizmo). 'vista3d barra <0|1>': la barra de
    // abajo (en el N95 viene apagada: cfg.nuevoUsuario).
    if (cmd == "vista3d") {
        std::string que; int v = -1; ss >> que >> v;
        if (!Viewport3DActive && rootViewport) rootViewport->Render();
        Viewport3D* vp = Viewport3DActive;
        if (!vp) { err = "vista3d: no hay viewport 3D"; return false; }
        if (que == "sombreado" && v >= 0 && v <= (int)RenderType::Vectores) vp->view.v = (RenderType::Enum)v;
        else if (que == "huesos" && (v == 0 || v == 1)) vp->showArmature = (v == 1);
        else if (que == "gizmo" && (v == 0 || v == 1)) g_gizmoOn = (v == 1);   // (en el N95 arranca apagado)
        else if (que == "barra" && (v == 0 || v == 1)) { extern bool g_capturaSinBarra3D; g_capturaSinBarra3D = (v == 0); }
        else { err = "vista3d: uso: vista3d sombreado <n> | vista3d huesos <0|1> | vista3d gizmo <0|1> | vista3d barra <0|1>"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'texturas': carga YA las texturas que el import dejo en la cola (el editor carga una por cuadro: en una foto
    // sacada enseguida, un modelo recien importado se veria sin textura)
    if (cmd == "texturas") { CargarTodasTexturasPendientes(); g_redraw = true; Cuadro(); return true; }
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
    // el KEYPAD del N95 (platform/symbian/src/Whisk3DContainer.cpp + w3dnewscene.cpp): las MISMAS llamadas al
    // viewport 3D activo que hace el telefono. Lo que se mantiene apretado se aplica una vez por cuadro, como alla:
    //   n95 orbitar <dx> <dy> [cuadros]   las flechas mantenidas (8 por cuadro)
    //   n95 zoom <dy> [cuadros]           0 + arriba / abajo (dy -1 = arriba = acercar)
    //   n95 panear <dx> <dy> [cuadros]    * + las flechas
    //   n95 vista <dx> <dy>               # + una flecha (una vista por pulsacion)
    //   n95 perspectiva                   # solo, sin flecha
    //   n95 encuadrar                     0 solo, sin flecha
    //   n95 camara                        8 (fuera del Modo Edicion): mirar por la camara activa / volver
    //   n95 bloquearorbita                9: bloquear la orbita (lo que orbitaba, desplaza) / desbloquearla
    if (cmd == "n95") {
        std::string que; ss >> que;
        if (!Viewport3DActive && rootViewport) rootViewport->Render();   // (lo puebla el dibujado)
        Viewport3D* vp = Viewport3DActive;
        if (!vp) { err = "n95: no hay viewport 3D"; return false; }
        int a = 0, b = 0, cuadros = 1;
        if (que == "orbitar" || que == "panear") {
            if (!(ss >> a >> b)) { err = "n95: uso: n95 " + que + " <dx> <dy> [cuadros]"; return false; }
            if (!(ss >> cuadros)) cuadros = 1;
            for (int i = 0; i < cuadros; i++) {
                if (que == "orbitar") vp->OrbitarFlecha(a * 8, b * 8);
                else                  vp->PanFlecha(-a * 8, -b * 8);
                g_redraw = true; Cuadro();
            }
        } else if (que == "zoom") {
            if (!(ss >> b)) { err = "n95: uso: n95 zoom <dy> [cuadros]"; return false; }
            if (!(ss >> cuadros)) cuadros = 1;
            for (int i = 0; i < cuadros; i++) { vp->Zoom((float)(-b)); g_redraw = true; Cuadro(); }
        } else if (que == "vista") {
            if (!(ss >> a >> b)) { err = "n95: uso: n95 vista <dx> <dy>"; return false; }
            vp->VistaCuadranteNav(a, b);
        } else if (que == "perspectiva") vp->ChangePerspective();
        else if (que == "encuadrar") vp->EnfocarObject();
        else if (que == "camara") vp->SetViewFromCameraActive(!vp->ViewFromCameraActive);
        else if (que == "bloquearorbita") { extern bool LayoutLockOrbitToggle(); LayoutLockOrbitToggle(); }
        // MOVER / ROTAR / ESCALAR con el keypad (platform/symbian/src/w3dnewscene.cpp, W3dNewTransform*), en Modo Objeto:
        //   n95 transformar <1|2|3>              1 / 2 / 3 sin transform en curso: mover / rotar / escalar
        //   n95 eje <0|1|2>                      1 / 2 / 3 durante el transform: eje X / Y / Z (otra vez: cicla la orientacion)
        //   n95 transflechas <dx> <dy> [cuadros] las flechas mantenidas (mover: 4 por cuadro; rotar y escalar: 8)
        //   n95 aceptar | n95 cancelar           OK (el centro del pad) / C
        else if (que == "transformar") {
            int m = 0; ss >> m;
            if (m < 1 || m > 3) { err = "n95: uso: n95 transformar <1|2|3>"; return false; }
            if (!ObjActivo || !ObjActivo->select) { err = "n95 transformar: no hay un objeto activo seleccionado"; return false; }
            if (m == 1) SetPosicion();
            else {
                if (!guardarEstado()) { err = "n95 transformar: no se pudo guardar el estado"; return false; }
                estado = (m == 2) ? rotacion : EditScale;
                axisSelect = ViewAxis;
            }
            gModoN95 = m;
        } else if (que == "eje") {
            int e = 0; ss >> e;
            if (!gModoN95) { err = "n95 eje: no hay un transform en curso"; return false; }
            CiclarEjeTransform(e);
        } else if (que == "transflechas") {
            if (!gModoN95) { err = "n95 transflechas: no hay un transform en curso"; return false; }
            if (!(ss >> a >> b)) { err = "n95: uso: n95 transflechas <dx> <dy> [cuadros]"; return false; }
            if (!(ss >> cuadros)) cuadros = 1;
            const int d = (gModoN95 == 1) ? 4 : 8;
            for (int i = 0; i < cuadros; i++) {
                if (gModoN95 == 1) SetTranslacionObjetos(a * d, b * d, vp->VelocidadArrastreMundo());
                else if (gModoN95 == 2) SetRotacion(a * d, b * d);
                else SetScale(a * d, b * d, 0.001f);
                g_redraw = true; Cuadro();
            }
        } else if (que == "aceptar" || que == "cancelar") {
            if (!gModoN95) { err = "n95 " + que + ": no hay un transform en curso"; return false; }
            if (que == "aceptar") vp->Aceptar(); else { Cancelar(); estado = editNavegacion; }
            gModoN95 = 0;
        }
        else { err = "n95: accion desconocida '" + que + "' (orbitar|zoom|panear|vista|perspectiva|encuadrar|camara|bloquearorbita|transformar|eje|transflechas|aceptar|cancelar)"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'caras <n> <n> ...': en Modo Edicion, ELEGIR esas caras (solo esas, todas juntas). El 'select face' de los
    // tests deja una sola: cada una reemplaza a la anterior
    if (cmd == "caras") {
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "caras: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit();
        EditMesh* e = m->edit;
        if (!e) { err = "caras: la malla no tiene edit mesh"; return false; }
        EditSelectMode = SelFace;
        e->SeleccionarTodo(false);
        int f = 0, n = 0;
        while (ss >> f) {
            if (f < 0 || f >= e->NumFaces()) { err = "caras: cara fuera de rango"; return false; }
            e->TogglearFace(f, false);
            n++;
        }
        if (!n) { err = "caras: uso: caras <n> <n> ..."; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'stl exportar <ruta> [sel]': ExportSTL (todas las mallas o las seleccionadas), igual que Propiedades > Exportar.
    // 'stl revisar <malla> <puntos> <triangulos> <ymin> <ymax>': la malla (por nombre) tiene esos puntos UNICOS (los
    // vertices soldados), esos triangulos y esa altura en Y (en local): asi se ve que el STL entro unido y parado.
    // 'stl archivo <ruta> <triangulos> <zmax>': el .stl tiene esos triangulos y su Z mas alta es esa (Z arriba).
    if (cmd == "stl") {
        std::string que; ss >> que;
        if (que == "exportar") {
            std::string ruta, sel; ss >> ruta >> sel;
            if (!ExportSTL(ruta, sel == "sel", true, true)) { err = "stl exportar: fallo"; return false; }
            return true;
        }
        if (que == "revisar") {
            std::string nom; int puntos = -1, tris = -1; float ymin = 0, ymax = 0; ss >> nom >> puntos >> tris >> ymin >> ymax;
            Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
            if (!o || o->getType() != ObjectType::mesh) { err = "stl revisar: no hay una malla '" + nom + "'"; return false; }
            Mesh* m = (Mesh*)o;
            std::set<std::string> unicos;
            float y0 = 1e30f, y1 = -1e30f;
            for (int i = 0; i < m->vertexSize; i++) {
                unicos.insert(std::string((const char*)&m->vertex[i * 3], 12));
                if (m->vertex[i * 3 + 1] < y0) y0 = m->vertex[i * 3 + 1];
                if (m->vertex[i * 3 + 1] > y1) y1 = m->vertex[i * 3 + 1];
            }
            char b[200];
            snprintf(b, sizeof b, "stl revisar: '%s' tiene %d puntos, %d triangulos, Y de %.3f a %.3f (esperaba %d, %d, %.3f a %.3f)",
                     nom.c_str(), (int)unicos.size(), m->facesSize / 3, y0, y1, puntos, tris, ymin, ymax);
            printf("      %s\n", b);
            if ((int)unicos.size() != puntos || m->facesSize / 3 != tris || fabsf(y0 - ymin) > 1e-3f || fabsf(y1 - ymax) > 1e-3f) {
                err = b; return false;
            }
            return true;
        }
        if (que == "archivo") {
            std::string ruta; int tris = -1; float zmax = 0; ss >> ruta >> tris >> zmax;
            std::vector<unsigned char> d;
            if (!w3dFileSystem::ReadFileBytes(ruta, d) || d.size() < 84) { err = "stl archivo: no pude leer " + ruta; return false; }
            const unsigned int n = (unsigned int)d[80] | ((unsigned int)d[81] << 8) | ((unsigned int)d[82] << 16) | ((unsigned int)d[83] << 24);
            if (d.size() != 84 + (size_t)n * 50) { err = "stl archivo: el largo no cuadra con la cantidad de triangulos"; return false; }
            float z1 = -1e30f;
            for (unsigned int t = 0; t < n; t++)
                for (int v = 0; v < 3; v++) {
                    float z; memcpy(&z, &d[84 + (size_t)t * 50 + 12 + 12 * v + 8], 4);
                    if (z > z1) z1 = z;
                }
            char b[160];
            snprintf(b, sizeof b, "stl archivo: %u triangulos, Z max %.3f (esperaba %d, %.3f)", n, z1, tris, zmax);
            printf("      %s\n", b);
            if ((int)n != tris || fabsf(z1 - zmax) > 1e-3f) { err = b; return false; }
            return true;
        }
        err = "stl: uso: stl exportar|revisar|archivo ...";
        return false;
    }
    // 'cobertura': que la FUENTE de la interfaz pueda dibujar cada caracter de cada traduccion (res/idiomas/*.txt).
    // Usa la fuente de verdad (Font::Soporta: font.png + font-ext.png + las compuestas), no una lista aparte.
    if (cmd == "cobertura") {
        extern Font* WhiskFont;
        if (!WhiskFont) { err = "cobertura: no hay fuente"; return false; }
        int faltan = 0;
        for (int idi = 1; idi < (int)W3dLangCantidad; idi++) {
            std::vector<unsigned char> b;
            const std::string ruta = w3dFileSystem::GetResDir() + "/idiomas/" + W3dIdiomaCodigo((W3dIdioma)idi) + ".txt";
            if (!w3dFileSystem::ReadFileBytes(ruta, b) || b.empty()) { printf("@cap cobertura %s: sin archivo\n", W3dIdiomaCodigo((W3dIdioma)idi)); continue; }
            b.push_back(0);
            std::set<unsigned> malos;
            size_t i = 0;
            while (b[i]) {
                const uint16_t cp = UTF8_Char((const char*)&b[0], i);
                if (cp == '\n' || cp == '\t' || cp == '\r') continue;
                if (!WhiskFont->Soporta(cp)) malos.insert(cp);
            }
            printf("@cap cobertura %s: %d caracter(es) sin dibujo", W3dIdiomaCodigo((W3dIdioma)idi), (int)malos.size());
            for (std::set<unsigned>::iterator it = malos.begin(); it != malos.end(); ++it) printf(" U+%04X", *it);
            printf("\n");
            faltan += (int)malos.size();
        }
        if (faltan) { err = "cobertura: la fuente no tiene todos los caracteres de las traducciones"; return false; }
        return true;
    }
    if (cmd == "capinfo") {
        if (!rootViewport) { err = "capinfo: no hay layout de viewports"; return false; }
        printf("@cap pantalla w=%d h=%d\n", MenuPantallaW, MenuPantallaH);
        InfoHojas(rootViewport);
        InfoMenu(MenuAbierto, 0);
        // el TECLADO NUMERICO tactil, si esta abierto: el centro de cada tecla
        {
            static const char* kTec[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "*", "/", "+", "-", "(", ")", "DEL", "<", ">", "X", "OK" };
            for (size_t k = 0; k < sizeof(kTec) / sizeof(kTec[0]); k++) {
                int tx = 0, ty = 0;
                if (NumPadTeclaCentro(kTec[k], tx, ty)) printf("@cap numpad x=%d y=%d texto=%s\n", tx, ty, kTec[k]);
            }
        }
        return true;
    }
    manejado = false;
    return false;
}
