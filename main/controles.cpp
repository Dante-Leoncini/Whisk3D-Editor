#include "ViewPorts/LayoutInput.h" // ruteo compartido (menus/barras/paneles)
#include "ViewPorts/Properties.h" // PropertiesTouchScrollFin (fin del scroll tactil de listas)
#include "ViewPorts/ViewPort3D.h" // Viewport3DActive->Aceptar() en el transform
#include "ViewPorts/PopUp/PopUpBase.h" // PopUpActive (la X cancela)
#include "ViewPorts/Timeline.h"    // DopeNumInputChar (valor numerico del transform de keyframes)
#include "WhiskUI/glesdraw.h"        // W3dPantallaAlto
#include "controles.h"
#include "Undo.h" // Ctrl+Z
void RebindMaterialMeshPart(); // (def en Properties.cpp) refresca el panel de material tras undo/redo

// rename de mesh part / material (def en Properties.cpp): el textfield se acepta/cancela aca
extern bool RenameActivo();
extern void RenameCommit();
extern void RenameCancel();
// edicion numerica por texto de un PropFloat (click/OK -> tipear + enter). Declarado en WhiskUI/PropFloat.
extern bool NumEditActivo();
extern void NumEditCommit();
extern void NumEditCancel();
extern void NumEditSalirDelPanel(); // limpia el 'editando' del panel al terminar (sino te clava en la propiedad)

std::map<SDL_FingerID, Finger> fingers;
float lastDistance = 0.0f;
float PINCHposY = 0.0f;
float lastCentroidX = 0.0f, lastCentroidY = 0.0f; // 2 dedos: punto medio anterior (para el paneo)
bool g_fingerScrolling = false; // 1 dedo esta scrolleando un panel/toolbar -> el UP no debe seleccionar
// GESTO de scroll LOCKEADO: apenas el dedo se mueve, el scroll queda "pegado" a ese viewport (barra o
// contenido) para todo el arrastre. Sin esto, al salir de la barra/viewport empezaba a orbitar y al
// pasar por las pestañas las abria. Se resetea al levantar el dedo.
ViewportBase* g_scrollView = NULL;  // viewport lockeado para scrollear (NULL = gesto sin decidir)
bool g_scrollBar = false;           // el scroll lockeado es de la BARRA (true) o del CONTENIDO (false)
bool g_barTapPending = false;       // toque sobre una barra: si LEVANTA sin arrastrar = tap (abre); si arrastra = scroll
bool g_contentTapPending = false;   // toque sobre el CONTENIDO de un panel (properties/outliner): si LEVANTA sin
                                    // arrastrar = tap (togglea checkbox / selecciona); si arrastra = scroll (NO togglea)
bool g_slideNum = false;            // arrastre HORIZONTAL sobre un campo numerico -> editar el valor (slider tactil)
bool g_view3dTapPending = false;    // toque sobre el VIEWPORT 3D (tactil): si LEVANTA sin arrastrar = TAP (selecciona);
                                    // si arrastra = orbita/panea (NO cambia la seleccion). Antes se perdia al orbitar.
ViewportBase* g_barTapView = NULL;  // viewport cuya barra/contenido se toco (para lockear el scroll a ESE, no al hover)
bool g_popupFinger = false;         // un dedo esta manejando el popup activo (bypass de la sintesis de mouse de SDL)
bool g_popupOpenedByMouseDown = false; // el popup lo abrio el mouse-down de ESTE toque (menu->accion): el FINGERDOWN
                                    // que sigue NO debe cerrarlo (caeria "afuera", en la posicion del item del menu)
// ESQUINA (boton de menu [0] de cada viewport) = redimension estilo esquina de Windows: ARRASTRAR
// cambia el tamaño (borde izq + sup del viewport), un TAP abre el menu desplegable. Vale mouse y touch.
ViewportBase* g_cornerVp = NULL;    // viewport cuyo boton-esquina se apreto; NULL = ninguno
bool g_cornerResizing = false;      // ya se decidio que ESTE gesto redimensiona (paso el umbral)
int g_tapStartX = 0, g_tapStartY = 0; // posicion del down (umbral: hasta que no se mueve N px sigue siendo TAP)
bool g_uiTapEnCurso = false;        // el LayoutClickUI que corre es un TAP tactil diferido (no un click de mouse)
Uint32 g_lastFingerTicks = 0;       // ultimo evento de DEDO: filtra los mouse FANTASMA que el browser sintetiza
                                    // tras el touch (llegan como mouse "real" y clickeaban/abrian al soltar)

void Contadores(){
    if (LShiftPressed){
        ShiftCount++;
    }
}

void SetFullScreen(bool value){
    cfg.fullscreen = value;
    if (value) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } 
    else {
        SDL_SetWindowFullscreen(window, 0);
    }
}

// coords en PIXELES de un evento de dedo (normalizado 0..1 -> pixeles de ventana, igual que la sintesis
// de SDL: mx = fx * ancho_ventana). Se usa para manejar el popup activo directo desde el touch.
static void FingerPix(const SDL_TouchFingerEvent& tf, int& mx, int& my){
    int ww = 0, hh = 0; if (window) SDL_GetWindowSize(window, &ww, &hh);
    mx = (int)(tf.x * ww); my = (int)(tf.y * hh);
}

void InputUsuarioSDL3(SDL_Event &e){
    RefreshInputControllerSDL(e);

    switch(e.type) {
        case SDL_FINGERDOWN:
            // OJO ORDEN: SDL sintetiza el MOUSEBUTTONDOWN ANTES de pushear este FINGERDOWN -> el reset del
            // gesto va en el down del mouse (aca borraria los flags recien seteados por ese down).
            fingers[e.tfinger.fingerId] = {e.tfinger.x, e.tfinger.y};
            g_lastFingerTicks = SDL_GetTicks();
            // POPUP activo (explorador, etc): lo maneja el DEDO directo (la sintesis de mouse de SDL se
            // desincroniza). 1 dedo = click del popup; si Click devuelve false (afuera) se cierra.
            if (PopUpActive && fingers.size() == 1) {
                // ...salvo que el popup lo ACABE de abrir el mouse-down de ESTE toque (menu Delete, etc):
                // ese finger-down caeria afuera (en la posicion del item del menu) y lo cerraria. Se ignora.
                if (g_popupOpenedByMouseDown) { g_popupOpenedByMouseDown = false; break; }
                int mx, my; FingerPix(e.tfinger, mx, my);
                leftMouseDown = true; g_popupFinger = true;
                if (!PopUpActive->Click(mx, my)) {   // afuera del popup -> cerrarlo (como LayoutClickUI)
                    PopUpActive->Cerrar();
                    g_popupFinger = false; leftMouseDown = false;
                }
                break; // el popup ya lo manejo: no seguir al flujo de gestos de viewport
            }
            // LOOP CUT en fase de ORIENTACION: el dedo elige la direccion tocando un punto. Igual que el
            // popup, lo maneja el dedo DIRECTO (la sintesis touch->mouse se desincroniza). Si el mouse
            // sintetizado ya lo proceso (llega ANTES del FINGERDOWN), gLoopCutOrientando ya es false y esto
            // no hace nada; si se desincronizo, este es el unico camino que registra el toque.
            if (LoopCutActivo() && LoopCutOrientando() && fingers.size() == 1) {
                int mx, my; FingerPix(e.tfinger, mx, my);
                LoopCutClickIzq(mx, my);
                break;
            }
            if (fingers.size() >= 2) {
                // 2do dedo -> arranca el gesto de 2 dedos: cortamos el orbit/drag del mouse (1 dedo
                // sintetizado) y cancelamos el gesto de 1 dedo a medio decidir (tap/scroll/slider).
                if (g_popupFinger) { if (PopUpActive) PopUpActive->Soltar(); g_popupFinger = false; }
                leftMouseDown = middleMouseDown = ViewPortClickDown = false;
                g_barTapPending = g_contentTapPending = g_slideNum = false;
                g_view3dTapPending = false; // 2do dedo (pan/zoom): NO seleccionar al soltar
                g_scrollView = NULL;
                g_cornerVp = NULL; g_cornerResizing = false; // 2do dedo: cancela el gesto de esquina
                lastDistance = 0.0f; // re-arma pinch/paneo desde este toque
            }
            break;

        case SDL_FINGERUP:
            fingers.erase(e.tfinger.fingerId);
            g_lastFingerTicks = SDL_GetTicks();
            lastDistance = 0.0f; // reset
            // POPUP manejado por el dedo: soltar (dispara tap/abre entrada/termina scroll) al levantar
            if (g_popupFinger) {
                if (PopUpActive) PopUpActive->Soltar();
                g_popupFinger = false; leftMouseDown = false;
                break;
            }
            if (fingers.size() < 2) {
                // volvimos a <2 dedos -> cortar el drag (el dedo que queda no debe "saltar" a orbitar)
                leftMouseDown = middleMouseDown = ViewPortClickDown = false;
            }
            break;

        case SDL_FINGERMOTION:
            fingers[e.tfinger.fingerId] = {e.tfinger.x, e.tfinger.y};
            // POPUP manejado por el dedo: arrastre = Motion del popup (scroll de la lista, etc)
            if (g_popupFinger && PopUpActive && fingers.size() == 1) {
                g_lastFingerTicks = SDL_GetTicks();
                int mx, my; FingerPix(e.tfinger, mx, my);
                PopUpActive->Motion(mx, my);
                g_redraw = true;
                break;
            }
            g_lastFingerTicks = SDL_GetTicks();
            if (fingers.size() == 2 && viewPortActive) {
                std::map<SDL_FingerID, Finger>::iterator it = fingers.begin();
                Finger f1 = it->second; ++it; Finger f2 = it->second;
                float ex = f2.x - f1.x, ey = f2.y - f1.y;
                float dist = sqrtf(ex*ex + ey*ey);                          // distancia entre dedos (pinch)
                float cx = (f1.x + f2.x) * 0.5f, cy = (f1.y + f2.y) * 0.5f; // punto medio (paneo)
                if (lastDistance != 0.0f) {
                    // dedos vienen NORMALIZADOS (0-1). Zoom: abrir dedos = acercar. Paneo: mover el
                    // punto medio (a pixeles del viewport). Solo el Viewport3D responde (los paneles no).
                    float zoomDelta = (dist - lastDistance) * 60.0f;
                    float panDx = (cx - lastCentroidX) * (float)viewPortActive->width;
                    float panDy = (cy - lastCentroidY) * (float)viewPortActive->height;
                    viewPortActive->event_finger_gesture(zoomDelta, panDx, panDy);
                    g_redraw = true;
                }
                lastDistance = dist; lastCentroidX = cx; lastCentroidY = cy;
            }
		
            // (el gesto de 2 dedos ya se aplico arriba con event_finger_gesture)
            break;
    }

    // MOUSE FANTASMA: tras un gesto tactil el browser sintetiza mousedown/move/up de "compatibilidad" que
    // llegan como mouse REAL (which != SDL_TOUCH_MOUSEID) cuando ya no queda ningun dedo -> caian al flujo
    // de mouse y clickeaban/abrian el editor al soltar el scroll. Se descartan si hubo un dedo hace <500ms.
    // (Con dedos APOYADOS no se filtra: el mouse sintetizado del touch es el gesto en curso. En desktop
    // g_lastFingerTicks queda en 0 y no filtra nunca.)
    if (g_lastFingerTicks != 0 && fingers.empty() &&
        (SDL_GetTicks() - g_lastFingerTicks) < 500) {
        if (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.which != SDL_TOUCH_MOUSEID) return;
        if ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
            e.button.which != SDL_TOUCH_MOUSEID) return;
    }

    // POPUP en TACTIL: lo maneja el path de FINGER directo (abajo), porque la sintesis touch->mouse de SDL
    // se DESINCRONIZA con toques rapidos (deja de mandar mouse-down y el explorador quedaba "atrapado").
    // Aca se DESCARTAN los mouse SINTETIZADOS del touch mientras hay un popup: el dedo ya lo maneja, y asi
    // no hay doble proceso. (El mouse REAL de escritorio -which != TOUCH- sigue manejando el popup normal.)
    if (PopUpActive) {
        if (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.which == SDL_TOUCH_MOUSEID) return;
        if ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
            e.button.which == SDL_TOUCH_MOUSEID) return;
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION){
        // 2+ dedos: el mouse lo sintetiza el touch desde 1 dedo -> NO orbitar mientras se hace
        // pinch/paneo (el gesto de 2 dedos ya se maneja en SDL_FINGERMOTION).
        if (fingers.size() >= 2) return;

        // TACTIL durante un TRANSFORM: el touchStart teletransporta el mouse virtual (motion sintetizado
        // que llega ANTES del button-down) -> ese salto NO debe mover el transform (tocar el tilde de la
        // barra corria el objeto; y re-apoyar el dedo para seguir moviendo saltaba). El drag real llega
        // con leftMouseDown y pasa normal.
        if (e.motion.which == SDL_TOUCH_MOUSEID && !leftMouseDown && estado != editNavegacion) {
            g_xformPrimerMov = true; // el proximo motion ignora su delta (absorbe el salto)
            GuardarMousePos();
            return;
        }

        int mx = e.motion.x;
        int my = e.motion.y;

        // hover de la 'x' de las notificaciones (estan encima de todo)
        NotificacionesMotion(mx, my);

        // loop cut en curso: el motion maneja el preview (sigue la arista) o el slide
        if (LoopCutActivo()) { LoopCutMotion(mx, my); return; }

        // GESTO TACTIL DE SCROLL (1 dedo arrastrado). VA ANTES de LayoutMotionUI: sino el hover de la
        // barra consume el motion sobre un boton y el scroll SOLO andaba tocando el gap entre botones.
        // Queda LOCKEADO al viewport del down (g_barTapView) hasta levantar el dedo: NO orbita ni abre
        // menus aunque el dedo se salga. El mouse lo sintetiza el touch (fingers.size()==1).
        // gesto de scroll con 1 dedo O con el mouse (fingers<=1). El scroll de BARRA (g_barTapPending) vale para
        // ambos; el de CONTENIDO de panel y el slider numerico son solo touch (en PC el mouse usa scrollbar/rueda
        // + el drag numerico clasico). Lockeado al viewport del down hasta soltar.
        if (leftMouseDown && fingers.size() <= 1) {
            // VIEWPORT 3D en tactil: el down NO selecciono (g_view3dTapPending). Si el dedo paso el umbral,
            // es un orbit/paneo -> se cancela el tap (la seleccion NO cambia) y se sigue de largo al
            // event_mouse_motion (que orbita). Un jitter chico sigue siendo TAP (selecciona al soltar).
            if (g_view3dTapPending) {
                int vdx = mx - g_tapStartX; if (vdx < 0) vdx = -vdx;
                int vdy = my - g_tapStartY; if (vdy < 0) vdy = -vdy;
                if (vdx + vdy > 8 * GlobalScale) g_view3dTapPending = false;
            }
            // ESQUINA (boton de menu): si se ARRASTRA, redimensiona el viewport (borde izq + sup). Un TAP
            // (sin pasar el umbral) NO entra aca -> al soltar abre el menu como siempre.
            if (g_cornerVp) {
                if (!g_cornerResizing) {
                    int cdx = mx - g_tapStartX; if (cdx < 0) cdx = -cdx;
                    int cdy = my - g_tapStartY; if (cdy < 0) cdy = -cdy;
                    if (cdx + cdy > 8 * GlobalScale) {
                        g_cornerResizing = true;
                        g_barTapPending = false; g_contentTapPending = false; // cancela el tap->menu / scroll de barra
                    }
                }
                if (g_cornerResizing) {
                    LayoutResizeEsquina(g_cornerVp, e.motion.xrel, e.motion.yrel); // dx=borde izq, dy=borde sup
                    g_fingerScrolling = true; g_redraw = true; return;
                }
            }
            if (g_slideNum) {                                        // slider numerico lockeado -> el arrastre EDITA
                if (g_barTapView) g_barTapView->TouchSliderMover(e.motion.xrel);
                g_fingerScrolling = true; g_redraw = true; return;
            }
            if (g_scrollView) {                                      // scroll lockeado (barra o contenido)
                if (g_scrollBar) g_scrollView->BarScrollBy(e.motion.xrel);
                else             g_scrollView->event_finger_scroll(mx, my, e.motion.xrel, e.motion.yrel);
                g_fingerScrolling = true; g_redraw = true; return;
            }
            // gesto SIN decidir (down sobre barra o contenido de panel): sigue siendo TAP hasta pasar el
            // umbral. Recien ahi decide barra/scroll/slider. Asi un tap con jitter chico igual togglea/abre.
            if (g_barTapPending || g_contentTapPending) {
                int ddx = mx - g_tapStartX; if (ddx < 0) ddx = -ddx;
                int ddy = my - g_tapStartY; if (ddy < 0) ddy = -ddy;
                if (ddx + ddy < 8 * GlobalScale) { g_redraw = true; return; } // aun es tap
                if (g_barTapPending) {                               // BARRA -> scroll horizontal (mouse o touch)
                    g_barTapPending = false;
                    g_scrollView = g_barTapView; g_scrollBar = true;
                    if (g_barTapView) g_barTapView->BarScrollBy(e.motion.xrel);
                } else {                                             // CONTENIDO de panel (touch): decidir por DIRECCION
                    g_contentTapPending = false;
                    // arrastre HORIZONTAL que arranco DENTRO del value box de un campo numerico -> editar (slider).
                    // Cualquier otro (vertical, o sobre el label) -> SCROLL vertical. Asi el scroll NUNCA se rompe
                    // por tocar un campo, y solo se edita arrastrando horizontal adentro del campo.
                    if (ddx > ddy && g_barTapView &&
                        g_barTapView->PuntoEnCampoNumerico(g_tapStartX, g_tapStartY) &&
                        g_barTapView->TouchSliderArmar(g_tapStartX, g_tapStartY)) {
                        g_slideNum = true;
                        g_barTapView->TouchSliderMover(e.motion.xrel);
                    } else {
                        g_scrollView = g_barTapView; g_scrollBar = false;
                        if (g_barTapView) g_barTapView->event_finger_scroll(mx, my, e.motion.xrel, e.motion.yrel);
                    }
                }
                g_fingerScrolling = true; g_redraw = true; return;
            }
            // fallback SOLO touch (editor UV que panea, etc.): el viewport del down decide si scrollea.
            // Touch por el which del evento (el mapa de dedos puede no estar al dia por el orden de eventos).
            if (e.motion.which == SDL_TOUCH_MOUSEID && viewPortActive &&
                viewPortActive->ViewportKind() == 4 &&
                viewPortActive->event_finger_scroll(mx, my, e.motion.xrel, e.motion.yrel)) {
                g_scrollView = viewPortActive; g_scrollBar = false;
                g_fingerScrolling = true; g_redraw = true; return;
            }
            // ni barra ni contenido diferido (viewport 3D) -> sigue de largo al event_mouse_motion -> orbita
        }

        // menu desplegable abierto / hover de barras (ruteo compartido)
        if (LayoutMotionUI(mx, my)) {
            return;
        }

        // arbol en coordenadas ARRIBA-izquierda (como Symbian): el mouse
        // de SDL ya viene asi, sin flip
        if (!ViewPortClickDown){
            // NO perder el foco al cruzar un GAP/SEPARADOR: si el mouse virtual (touch) cae entre viewports
            // FindViewportUnderMouse devuelve NULL. Antes eso dejaba viewPortActive=NULL y, con ViewPortClickDown
            // trabado, el 'if (!viewPortActive) return' de mas abajo bloqueaba TODOS los clicks (no se podia
            // confirmar/cancelar ni tocar nada; solo el hover andaba). Mantenemos el ultimo viewport valido.
            ViewportBase* vpBajoCursor = FindViewportUnderMouse(rootViewport, mx, my);
            if (vpBajoCursor) viewPortActive = vpBajoCursor;
        }

        if (viewPortActive){
            viewPortActive->event_mouse_motion(mx, my);
        }

        if ((leftMouseDown || middleMouseDown) && viewPortActive) {
            CheckWarpMouseInViewport(mx, my, viewPortActive);
        }
        // POSE Mode NO envuelve el cursor: el transform de huesos usa el delta REAL mx/my (PoseXformMotion). Si se
        // envolviera, el salto del cursor al borde daria un delta enorme y la pose pegaria un tiron.
        else if ((estado == translacion || estado == rotacion || estado == EditScale) && InteractionMode != PoseMode){
            ViewPortClickDown = true;
            CheckWarpMouseInViewport(mx, my, viewPortActive);
        }
        // TRANSFORM DE KEYFRAMES del timeline (g/s/r): mismo trato que el del 3D. Envolver el cursor deja seguir
        // escalando/rotando sin quedarse sin pantalla. Puede: usa un cursor VIRTUAL que acumula dx/dy (que el warp
        // pone en 0 justo en el salto), no la posicion real. ViewPortClickDown congela el foco en el timeline: sin
        // eso, al envolverse el cursor podria caer en otro viewport y el warp pasaria a usar EL RECT EQUIVOCADO.
        else if (DopeXformActivo() && viewPortActive && viewPortActive->ViewportKind() == 5){
            ViewPortClickDown = true;
            CheckWarpMouseInViewport(mx, my, viewPortActive);
        }
    }

    // modificadores GLOBALES (shift/ctrl/alt): se trackean siempre,
    // los usa la seleccion del outliner y el pick 3D
    if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {
        bool down = (e.type == SDL_EVENT_KEY_DOWN);
        switch (e.key.keysym.sym) {
            case SDLK_LSHIFT:
            case SDLK_RSHIFT: LShiftPressed = down; break;
            case SDLK_LCTRL:
            case SDLK_RCTRL:  LCtrlPressed = down; break;
            case SDLK_LALT:   LAltPressed = down; break;
        }
    }

    // ANTI-TRABA (red de seguridad): si por cualquier camino viewPortActive quedo NULL, un BOTON del mouse/
    // touch recupera el foco desde la posicion del click y destraba ViewPortClickDown. Sin esto, el
    // 'if (!viewPortActive) return' de abajo mataba toda la interaccion (bug nefasto del pinch de 2 dedos:
    // un dedo cae en un gap -> viewPortActive NULL -> no se podia aceptar/cancelar un extrude ni tocar nada).
    if (!viewPortActive &&
        (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP)) {
        viewPortActive = FindViewportUnderMouse(rootViewport, (int)e.button.x, (int)e.button.y);
        if (viewPortActive) ViewPortClickDown = false;
    }
    if (!viewPortActive) return;

    // rueda del mouse (bloqueada con un desplegable abierto)
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        if (LoopCutActivo()) { LoopCutWheel(e.wheel.y > 0 ? 1 : -1); return; } // mas/menos cortes
        if (PopUpActive) {
            PopUpActive->Wheel(e.wheel.y); // popup modal (file browser, etc)
        } else if (LayoutMenuAbierto() && MenuAbierto) {
            MenuAbierto->Wheel(e.wheel.y > 0 ? 1 : -1); g_redraw = true; // menu desplegable largo (ej: 129 clips) -> scroll
        } else if (!LayoutMenuAbierto()) {
            // la rueda va al viewport BAJO EL CURSOR, no al activo: si le di play (o toque el timeline) y muevo el
            // mouse a Properties, scrollear tiene que scrollear Properties, no zoomear el timeline. Fallback al
            // activo si el cursor cae en un gap/separador (FindViewportUnderMouse -> NULL).
            int wmx, wmy; SDL_GetMouseState(&wmx, &wmy);
            ViewportBase* vpRueda = FindViewportUnderMouse(rootViewport, wmx, wmy);
            if (vpRueda && !ViewPortClickDown) viewPortActive = vpRueda; // el foco (borde verde) tambien sigue al cursor
            (vpRueda ? vpRueda : viewPortActive)->event_mouse_wheel(e);
        }
    }

    // Botones del mouse
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        // loop cut en curso: izq aplica/confirma, der centra+confirma (o cancela en preview)
        if (LoopCutActivo()) {
            bool popupAntes = (PopUpActive != NULL);
            if (e.button.button == SDL_BUTTON_LEFT)       LoopCutClickIzq((int)e.button.x, (int)e.button.y);
            else if (e.button.button == SDL_BUTTON_RIGHT) LoopCutClickDer();
            // si este toque (tactil) abrio el panel redo del loop cut, avisar al FINGERDOWN del mismo
            // toque para que NO lo cierre (caeria afuera del panel recien abierto). Igual que el menu Delete.
            if (e.button.which == SDL_TOUCH_MOUSEID && !popupAntes && PopUpActive) g_popupOpenedByMouseDown = true;
            GuardarMousePos();
            return;
        }
        // loop select (modal de direccion desde una cara): cualquier click acepta
        if (LoopSelOrientando()) { LoopSelConfirm(); GuardarMousePos(); return; }
        // pick shortest path guiado: click DERECHO cancela (el izquierdo lo maneja el pick)
        if (PickPathGuiado() && e.button.button == SDL_BUTTON_RIGHT) { LayoutPickPathCancelar(); GuardarMousePos(); return; }
        // Select Linked / Loop Select guiados (1 click): click DERECHO cancela (el izquierdo lo maneja el pick)
        if (GuiadoUnClickActivo() && e.button.button == SDL_BUTTON_RIGHT) { LayoutGuiadoCancelar(); GuardarMousePos(); return; }
        // rename de mesh part / material en curso: click IZQ ACEPTA, click DER CANCELA. Consume el click.
        if (RenameActivo()) {
            if (e.button.button == SDL_BUTTON_LEFT)       RenameCommit();
            else if (e.button.button == SDL_BUTTON_RIGHT) RenameCancel();
            GuardarMousePos();
            return;
        }
        // click DENTRO de un popup modal (teclado numerico, color picker): NO commitea la edicion ni
        // desenfoca el campo (las teclas del popup EDITAN ese campo; sino el 1er tap al teclado lo cerraba)
        bool clickEnPopup = PopUpActive && PopUpActive->Contains((int)e.button.x, (int)e.button.y);
        // edicion numerica por texto: un click en cualquier lado APLICA lo tipeado (no consume el click -> si fue
        // sobre otro campo, ese arranca su propia edicion en el mouse-up).
        if (NumEditActivo() && !clickEnPopup) { NumEditCommit(); NumEditSalirDelPanel(); }
        ViewPortClickDown = true;
        if (e.button.button == SDL_BUTTON_LEFT) {
            leftMouseDown = true;
            bool popupAntes = (PopUpActive != NULL); // para detectar si ESTE down abre un popup (menu->accion)
            if (!clickEnPopup)
                g_textFieldActivo = NULL; // click en cualquier lado desenfoca el texto
                                          // (ClickEn de Properties re-enfoca en el up si es una caja)
            // TOUCH se detecta por el which del evento (SDL_TOUCH_MOUSEID), NO por el mapa de dedos: SDL
            // sintetiza este mouse-down ANTES de pushear el SDL_FINGERDOWN -> en este punto 'fingers'
            // todavia esta VACIO y chequearlo hacia caer el touch al flujo de mouse (clickeaba al apoyar).
            bool esTouch = (e.button.which == SDL_TOUCH_MOUSEID);
            // gesto NUEVO: limpiar lo que quedo del anterior (scroll lockeado, tap a medio decidir, slider)
            g_fingerScrolling = false; g_scrollView = NULL; g_slideNum = false;
            g_barTapPending = false; g_contentTapPending = false;
            // si habia un transform activo, este click lo ACEPTA y NO
            // debe ademas cambiar la seleccion (como en Symbian)
            extern int g_poseModo; // Pose Mode: transform de huesos (G/R/S) en curso -> el click lo CONFIRMA igual
            bool habiaTransform = (estado == translacion ||
                                   estado == rotacion ||
                                   estado == EditScale ||
                                   g_poseModo != 0);
            // viewport bajo el click (lo usa el diferido de barra/contenido, abajo)
            ViewportBase* vpDown = FindViewportUnderMouse(rootViewport, (int)e.button.x, (int)e.button.y);
            // ESQUINA: down sobre el boton de menu [0] del viewport -> candidato a redimension por arrastre
            // (el tap sigue abriendo el menu via el diferido de barra). Vale mouse y touch. NO si hay un
            // popup/menu modal abierto (p.ej. el explorador de archivos): esos son duenos del input y el
            // corner-drag secuestraria/bloquearia sus botones y su scroll.
            g_cornerVp = NULL; g_cornerResizing = false;
            if (!clickEnPopup && !PopUpActive && !LayoutMenuAbierto() &&
                vpDown && vpDown->isLeaf() && !vpDown->BarButtons.empty() &&
                vpDown->BarButtons[0]->Contains((int)e.button.x, (int)e.button.y)) {
                g_cornerVp = vpDown;
                g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y;
            }
            // touch NO genera hover antes del down -> viewPortActive podia quedar stale y el motion/up
            // (slide del campo numerico, mouse_button_up del panel) iban al viewport equivocado.
            if (esTouch && vpDown) viewPortActive = vpDown;
            // la 'x' de una notificacion de error la cierra (esta encima de todo)
            if (NotificacionesClick((int)e.button.x, (int)e.button.y)) {
                // consumido: no propagar el click
            }
            else if (habiaTransform) {
                if (esTouch) {
                    // TACTIL: tocar NO confirma el transform (la pantalla se usa para MOVER; sin ella no
                    // podes ni arrastrar). Confirmar/cancelar = tilde/cruz de la barra de HERRAMIENTAS.
                    if (MenuAbierto) {
                        // menu abierto desde la barra (orientacion): el tap elige la opcion / lo cierra
                        LayoutClickUI((int)e.button.x, (int)e.button.y);
                    }
                    // tap en la barra de ESTADO del transform ("Move: ... = ..."): abre el teclado
                    // numerico para editar el valor exacto (como tipear en PC). Si no cayo ahi, la
                    // barra de HERRAMIENTAS (tilde/cruz/ejes) maneja el tap.
                    else if (vpDown && vpDown->ViewportKind() == 1) {
                        Viewport3D* v3 = (Viewport3D*)vpDown;
                        if (!v3->ClickBarraTransform((int)e.button.x, (int)e.button.y))
                            v3->ToolbarClick((int)e.button.x, (int)e.button.y);
                    }
                    // fuera de la barra: nada en el down; el arrastre del dedo mueve el transform
                }
                // durante un transform (mover/rotar/escalar, o ubicar un duplicado) el CLICK de mouse
                // CONFIRMA, este donde este (incluso sobre la barra/menu): la UI no lo consume
                else if (g_poseModo){ extern void PoseXformConfirm(); PoseXformConfirm(); }
                else if (Viewport3DActive) Viewport3DActive->Aceptar();
            }
            // POPUP MODAL (teclado numerico, color picker) bajo el click: va directo, sin diferir -> las
            // teclas responden al DOWN (y el drag sobre el popup no scrollea el panel de atras).
            else if (clickEnPopup) {
                LayoutClickUI((int)e.button.x, (int)e.button.y);
            }
            // BARRA superior (MOUSE o touch): NO abrir la pestaña/menu en el DOWN. Si se ARRASTRA = scroll
            // horizontal; si se SUELTA sin mover = abre en el mouse-up. Vale para mouse PORQUE en la barra del
            // viewport 3D la rueda hace ZOOM -> en PC la unica forma de scrollear esa barra es arrastrandola.
            else if (!LayoutMenuAbierto() && vpDown && vpDown->OnBar((int)e.button.x, (int)e.button.y)) {
                g_barTapPending = true;
                g_barTapView = vpDown; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y;
            }
            // CONTENIDO de un panel (properties=3 / outliner=2), SOLO en TOUCH: no ejecutar el click en el down
            // (sino togglea/selecciona apenas apoyas). En PC el mouse usa scrollbar/rueda + el drag numerico
            // clasico (gFloatDrag), asi que ahi NO se difiere. Se decide en el 1er motion (dir) o al soltar (tap).
            // (con un desplegable abierto NO se difiere: el click es del MENU, va directo a LayoutClickUI de abajo.)
            else if (esTouch && !LayoutMenuAbierto() && vpDown &&
                     (vpDown->ViewportKind() == 2 || vpDown->ViewportKind() == 3)) {
                g_contentTapPending = true;
                g_barTapView = vpDown; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y;
            }
            // la UI compartida (menu/barras/paneles) consume primero
            else if (!LayoutClickUI((int)e.button.x, (int)e.button.y)) {
                // click sobre un viewport 3D: seleccion compartida por color picking
                ViewportBase* hoja3d = FindViewportUnderMouse(
                    rootViewport, (int)e.button.x, (int)e.button.y);
                bool es3Dnav = hoja3d && hoja3d->isLeaf() &&
                               hoja3d->ViewportKind() == 1 && estado == editNavegacion;
                // TACTIL sobre el viewport 3D: NO seleccionar en el DOWN. Si el dedo se arrastra = orbita/
                // panea (la seleccion NO cambia); si es un TAP simple, se pickea al SOLTAR. Antes cualquier
                // orbit/paneo pisaba la seleccion (se deseleccionaba al tocar el fondo para orbitar).
                if (esTouch && es3Dnav) {
                    g_view3dTapPending = true;
                    g_barTapView = hoja3d; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y;
                } else {
                    viewPortActive->button_left();
                    if (es3Dnav) {
                        ScenePick3D((int)e.button.x, (int)e.button.y,
                                    hoja3d->x, hoja3d->y,
                                    hoja3d->width, hoja3d->height,
                                    W3dPantallaAlto);
                    }
                }
            }
            // si ESTE mouse-down tactil abrio un popup (p.ej. menu Delete -> ConfirmarPopup), avisar al
            // FINGERDOWN del mismo toque para que NO lo cierre (caeria afuera del popup recien abierto).
            if (esTouch && !popupAntes && PopUpActive) g_popupOpenedByMouseDown = true;
            GuardarMousePos();
        }
        else if (e.button.button == SDL_BUTTON_MIDDLE) {
            middleMouseDown = true;
            // orbitar con el mouse AFUERA cierra el redo-panel (semi-modal);
            // al quedar PopUpActive=NULL, las motions de la orbita ya pasan
            if (PopUpActive && PopUpActive->CierraConViewport() &&
                !PopUpActive->Contains((int)e.button.x, (int)e.button.y))
                PopUpActive->Cerrar();
            GuardarMousePos();
        }
        else if (e.button.button == SDL_BUTTON_RIGHT) {
            // transform de KEYFRAMES (timeline): mismo trato que el del 3D -> derecho CANCELA. No usa 'estado'
            // (ese es el del viewport 3D), asi que necesita su propio chequeo.
            if (DopeXformActivo()){ DopeXformCancelar(); GuardarMousePos(); return; }
            if (estado == translacion || estado == EditScale || estado == rotacion){
                // Edit Mode con transform de malla en curso: descarta el snapshot
                if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
                else Cancelar();
                NumInputReset();
            }
        }
    }
    else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            leftMouseDown = false;
            g_cornerVp = NULL; g_cornerResizing = false; // fin del gesto de esquina
            // DRAG-SCROLL de menu largo: el soltar resuelve (drag=solo scrolleo; tap=selecciona el item). Consume el up.
            { extern bool LayoutMenuDragSoltar(int, int); if (LayoutMenuDragSoltar((int)e.button.x, (int)e.button.y)) { GuardarMousePos(); return; } }
            // suelta scroll agarrado y dropea el drag del outliner
            LayoutSoltar((int)e.button.x, (int)e.button.y);
        }
        else if (e.button.button == SDL_BUTTON_MIDDLE) {
            middleMouseDown = false;
        }
        GuardarMousePos();

        // fin del gesto tactil: soltar el lock del scroll. Si venia SCROLLEANDO, el soltar NO es un click de
        // seleccion. Si fue un TAP sobre una barra (toco sin arrastrar), RECIEN AHORA abrimos la pestaña/menu
        // (lo diferimos del down para no abrir menus al scrollear).
        bool wasBarTap = g_barTapPending;      // tap sobre una BARRA (mouse o touch) -> abre menu/pestaña
        bool wasContentTap = g_contentTapPending; // tap sobre CONTENIDO de panel (touch) -> togglea/selecciona/edita
        g_barTapPending = false;
        g_contentTapPending = false;
        g_scrollView = NULL;
        PropertiesTouchScrollFin(); // fin del gesto: libera el latch del scroll tactil de listas
        // slider numerico tactil: el arrastre YA edito el valor; soltar NO es un click (no abre el editor)
        if (g_slideNum) { if (g_barTapView) g_barTapView->TouchSliderSoltar(); g_slideNum = false; g_fingerScrolling = false; GuardarMousePos(); return; }
        if (g_fingerScrolling) { g_fingerScrolling = false; return; }
        // TAP simple sobre el viewport 3D (tactil): RECIEN AHORA se pickea la seleccion. Si hubo orbit/paneo,
        // g_view3dTapPending ya se limpio en el motion -> la seleccion NO cambia (era lo molesto que pedia Dante).
        if (g_view3dTapPending) {
            g_view3dTapPending = false;
            ViewportBase* h = g_barTapView;
            if (h && h->isLeaf() && h->ViewportKind() == 1 && estado == editNavegacion) {
                if (viewPortActive) viewPortActive->button_left(); // GuardarMousePos + cursor 3D en el punto tocado
                ScenePick3D((int)e.button.x, (int)e.button.y, h->x, h->y, h->width, h->height, W3dPantallaAlto);
            }
            GuardarMousePos();
            return;
        }
        // tap sin arrastrar sobre la UI: RECIEN AHORA ejecutamos el click (abre menu/pestaña, togglea checkbox,
        // selecciona, o abre el editor de un campo numerico/texto). g_uiTapEnCurso solo para el CONTENIDO tactil
        // (ahi el campo numerico abre el editor inline); en la barra es un click normal.
        if (wasBarTap || wasContentTap) {
            g_uiTapEnCurso = wasContentTap;
            LayoutClickUI((int)e.button.x, (int)e.button.y);
            g_uiTapEnCurso = false;
            return;
        }

        if (viewPortActive){
            viewPortActive->mouse_button_up(e);
        }

        // idem: no dejar viewPortActive en NULL si el up cayo en un gap/separador (se trababa todo)
        ViewportBase* vpUp = FindViewportUnderMouse(rootViewport, lastMouseX, lastMouseY);
        if (vpUp) viewPortActive = vpUp;
    }

    // eventos del teclado
    if (e.type == SDL_EVENT_KEY_DOWN) {

        // Ctrl+Z = DESHACER (undo). Global, antes del campo de texto y de los atajos del viewport.
        // Ctrl+Shift+Z tambien REHACE (convencion comun ademas de Ctrl+Y).
        if ((e.key.keysym.mod & KMOD_CTRL) && e.key.keysym.sym == SDLK_z && !RenameActivo()) {
            if (e.key.keysym.mod & KMOD_SHIFT) UndoRehacer(); else UndoDeshacer();
            RebindMaterialMeshPart(); // refresca el panel (ej: asignacion de material deshecha)
            g_redraw = true; return;
        }
        // Ctrl+Y = REHACER (redo).
        if ((e.key.keysym.mod & KMOD_CTRL) && e.key.keysym.sym == SDLK_y && !RenameActivo()) {
            UndoRehacer(); RebindMaterialMeshPart(); g_redraw = true; return;
        }

        // CAJA DE TEXTO enfocada: captura TODO el teclado (los caracteres entran por
        // SDL_TEXTINPUT). Enter/Esc desenfocan, flechas mueven el caret, backspace/
        // supr borran; el resto se consume para no disparar atajos del viewport.
        if (g_textFieldActivo) {
            SDL_Keycode k = e.key.keysym.sym;
            // si es un RENAME (mesh part / material): Enter ACEPTA, ESC CANCELA (escribe / descarta el
            // nombre). En un campo normal (export/output) ambos solo desenfocan (edicion en vivo).
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { if (RenameActivo()) RenameCommit(); else if (NumEditActivo()) { NumEditCommit(); NumEditSalirDelPanel(); } else g_textFieldActivo = NULL; }
            else if (k == SDLK_ESCAPE) { if (RenameActivo()) RenameCancel(); else if (NumEditActivo()) { NumEditCancel(); NumEditSalirDelPanel(); } else g_textFieldActivo = NULL; }
            else if (k == SDLK_LEFT)  { g_textFieldActivo->CaretIzq();  g_redraw = true; }
            else if (k == SDLK_RIGHT) { g_textFieldActivo->CaretDer();  g_redraw = true; }
            else if (k == SDLK_BACKSPACE) { g_textFieldActivo->Backspace();  g_redraw = true; }
            else if (k == SDLK_DELETE)    { g_textFieldActivo->DelForward(); g_redraw = true; }
            return; // consume el keydown (no cae al viewport)
        }

        // TECLADO durante un LOOP CUT (mismo flujo que Symbian):
        //  flechas = orientacion (quad) / +-cortes (preview) / slide; enter = acepta/avanza;
        //  ESC/backspace/click-der = centra el corte (factor 0.5) y CONFIRMA, no cancela.
        if (LoopCutActivo()) {
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE)      { LoopCutClickDer(); return; }
            // Enter en orientacion (teclado) = confirma la direccion de las FLECHAS -> preview clasico
            // (el picker de puntos es solo click/tactil). En las demas fases, Enter aplica/confirma.
            if (k == SDLK_RETURN  || k == SDLK_KP_ENTER)      { if (LoopCutOrientando()) LoopCutOrientConfirmarTeclado(); else LoopCutClickIzq(lastMouseX, lastMouseY); return; }
            if (k == SDLK_LEFT)  { LoopCutTecla(0); return; }
            if (k == SDLK_RIGHT) { LoopCutTecla(1); return; }
            if (k == SDLK_UP)    { LoopCutTecla(2); return; }
            if (k == SDLK_DOWN)  { LoopCutTecla(3); return; }
        }
        // loop select desde una cara: flechas eligen el sentido; enter/esc acepta la seleccion
        if (LoopSelOrientando()) {
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_ESCAPE) { LoopSelConfirm(); return; }
            if (k == SDLK_LEFT  || k == SDLK_UP)   { LoopSelTecla(0); return; }
            if (k == SDLK_RIGHT || k == SDLK_DOWN) { LoopSelTecla(1); return; }
        }
        // pick shortest path guiado: ESC cancela el modo (los clicks los maneja el pick)
        if (PickPathGuiado() && e.key.keysym.sym == SDLK_ESCAPE) { LayoutPickPathCancelar(); return; }
        // Select Linked / Loop Select guiados: ESC cancela
        if (GuiadoUnClickActivo() && e.key.keysym.sym == SDLK_ESCAPE) { LayoutGuiadoCancelar(); return; }

        // entrar o salir de la pantalla completa F11
        if (e.key.keysym.sym == SDLK_F11) {
            SetFullScreen(!cfg.fullscreen);
        }
        // atajos de menu contextual: si abren un menu NO deben caer al
        // viewport (si no, shift+S abriria el menu Y empezaria a escalar)
        bool atajoMenu = false;
        // ctrl+P / alt+P: menus de emparentar (compartidos)
        if (e.key.keysym.sym == SDLK_p && (LCtrlPressed || LAltPressed)) {
            LayoutMenuParent(LAltPressed, lastMouseX, lastMouseY);
            atajoMenu = true;
        }
        // shift+S: menu de snap (cursor/seleccion), estilo Blender
        if (e.key.keysym.sym == SDLK_s && LShiftPressed) {
            LayoutMenuSnap(lastMouseX, lastMouseY);
            atajoMenu = true;
        }
        // W (Edit Mode): menu de contexto del sub-modo (Vertex/Edge/Face) en el cursor
        if (e.key.keysym.sym == SDLK_w && InteractionMode == EditMode && !PopUpActive) {
            LayoutMenuEditContexto(lastMouseX, lastMouseY);
            atajoMenu = true;
        }
        // Ctrl+T (Edit Mode): triangula las caras seleccionadas (>3 lados)
        if (e.key.keysym.sym == SDLK_t && LCtrlPressed && InteractionMode == EditMode && !PopUpActive) {
            LayoutTriangulate();
            atajoMenu = true;
        }
        // Tab: si el activo es una malla, alterna Object<->Edit Mode (como Blender);
        // si no, cicla el viewport activo (borde verde). La logica del toggle vive
        // en LayoutToggleEditMode (la comparte Symbian).
        // Tab cambia de modo o cicla de viewport POR ENCIMA del ruteo a los viewports. Si el timeline tiene un
        // transform de keyframes a medio hacer hay que cancelarlo: se maneja con el foco congelado
        // (ViewPortClickDown) y, al irse el foco, ya nadie podria confirmarlo ni cancelarlo.
        if (e.key.keysym.sym == SDLK_TAB && DopeXformActivo()) DopeXformCancelar();
        if (e.key.keysym.sym == SDLK_TAB) {
            if (LShiftPressed) SnapToggle();  // Shift+Tab = toggle SNAP (imantado), como Blender
            else if (!LayoutToggleEditMode())
                LayoutCiclarViewportActivo(+1);
            atajoMenu = true;
        }
        // 'm' en Edit Mode = menu MERGE (soldar verts, estilo Blender); en el resto abre/cierra la barra de
        // menu del viewport activo (= soft-izq en Symbian). Teclado-solo: el primer menu se abre sin preseleccionar.
        if (e.key.keysym.sym == SDLK_m && !PopUpActive) {
            if (InteractionMode == EditMode) LayoutMenuMerge(lastMouseX, lastMouseY);
            else                             LayoutToggleBarraViewportActivo();
            atajoMenu = true;
        }
        // backspace durante un transform: borra del valor numerico tipeado (los
        // numeros/operadores entran por SDL_TEXTINPUT; el backspace no genera texto)
        if (e.key.keysym.sym == SDLK_BACKSPACE && (TextFieldInputChar(8) || NumInputChar(8) || DopeNumInputChar(8))) {
            atajoMenu = true;
        }
        // primero el ruteo compartido (menu abierto / edicion / hover)
        int lk = -1;
        switch (e.key.keysym.sym) {
            case SDLK_UP:     lk = LayoutKey::Up; break;
            case SDLK_DOWN:   lk = LayoutKey::Down; break;
            case SDLK_LEFT:   lk = LayoutKey::Left; break;
            case SDLK_RIGHT:  lk = LayoutKey::Right; break;
            case SDLK_RETURN: lk = LayoutKey::Enter; break;
            case SDLK_ESCAPE: lk = LayoutKey::Cancel; break;
            case SDLK_x:
                // la X cancela el popup (color picker), como esc
                if (PopUpActive) lk = LayoutKey::Cancel;
                break;
        }
        if (atajoMenu) {
            // ya manejado por el atajo de menu contextual
        }
        else if (lk >= 0 && LayoutTeclaUI(lk, lastMouseX, lastMouseY)) {
            // consumida por la UI compartida
        }
        else {
            // redo-panel semi-modal: una tecla de operador (r/g/s/x...) con el
            // mouse AFUERA lo cierra y deja pasar la tecla; con el mouse adentro
            // la tecla no va al viewport (estas editando el panel)
            if (PopUpActive && PopUpActive->CierraConViewport()) {
                if (!PopUpActive->Contains(lastMouseX, lastMouseY)) {
                    PopUpActive->Cerrar();
                    viewPortActive->event_key_down(e);
                }
            } else {
                viewPortActive->event_key_down(e);
            }
        }
    }
    else if (e.type == SDL_EVENT_KEY_UP) {
        viewPortActive->event_key_up(e);
    }
    // ENTRADA NUMERICA: el texto tipeado (respeta el layout del teclado: parentesis,
    // *, /, etc.) alimenta el valor exacto del transform. NumInputChar ignora todo si
    // no hay un transform activo. (PC = SDL_TEXTINPUT; Symbian alimenta NumInputChar
    // desde su propio teclado/keypad.)
    #if SDL_MAJOR_VERSION == 2
    else if (e.type == SDL_TEXTINPUT) {
        // prioridad: una caja de texto enfocada; si no, el transform del 3D; si no, el de los keyframes del dope sheet
        for (const char* p = e.text.text; *p; ++p)
            if (!TextFieldInputChar((unsigned char)*p) && !NumInputChar((unsigned char)*p))
                DopeNumInputChar((unsigned char)*p);
    }
    #endif
}