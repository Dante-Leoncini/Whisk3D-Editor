#include "ViewPorts/LayoutInput.h" // ruteo compartido (menus/barras/paneles)
#include "ViewPorts/ViewPort3D.h" // Viewport3DActive->Aceptar() en el transform
#include "ViewPorts/PopUp/PopUpBase.h" // PopUpActive (la X cancela)
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

std::map<SDL_FingerID, Finger> fingers;
float lastDistance = 0.0f;
float PINCHposY = 0.0f;

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

void InputUsuarioSDL3(SDL_Event &e){
    RefreshInputControllerSDL(e);

    switch(e.type) {
        case SDL_FINGERDOWN:
            fingers[e.tfinger.fingerId] = {e.tfinger.x, e.tfinger.y};
            break;

        case SDL_FINGERUP:
            fingers.erase(e.tfinger.fingerId);
            lastDistance = 0.0f; // reset
            break;

        case SDL_FINGERMOTION:
            fingers[e.tfinger.fingerId] = {e.tfinger.x, e.tfinger.y};
            if(fingers.size() == 2) {
                auto it = fingers.begin();
                Finger f1 = it->second;
                ++it;
                Finger f2 = it->second;

                float dx = f2.x - f1.x;
                float dy = f2.y - f1.y;
                float dist = sqrt(dx*dx + dy*dy);

                if(lastDistance != 0.0f) {
                    float delta = dist - lastDistance;
                    PINCHposY += delta * 1; // escalado a gusto
                }

                lastDistance = dist;
            }
		
			if (viewPortActive){
				viewPortActive->event_finger_motion(PINCHposY);
			}
            break;
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION){
        int mx = e.motion.x;
        int my = e.motion.y;

        // hover de la 'x' de las notificaciones (estan encima de todo)
        NotificacionesMotion(mx, my);

        // loop cut en curso: el motion maneja el preview (sigue la arista) o el slide
        if (LoopCutActivo()) { LoopCutMotion(mx, my); return; }

        // menu desplegable abierto / hover de barras (ruteo compartido)
        if (LayoutMotionUI(mx, my)) {
            return;
        }

        // arbol en coordenadas ARRIBA-izquierda (como Symbian): el mouse
        // de SDL ya viene asi, sin flip
        if (!ViewPortClickDown){
            viewPortActive = FindViewportUnderMouse(rootViewport, mx, my);
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

    if (!viewPortActive) return;

    // rueda del mouse (bloqueada con un desplegable abierto)
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        if (LoopCutActivo()) { LoopCutWheel(e.wheel.y > 0 ? 1 : -1); return; } // mas/menos cortes
        if (PopUpActive) {
            PopUpActive->Wheel(e.wheel.y); // popup modal (file browser, etc)
        } else if (!LayoutMenuAbierto()) {
            viewPortActive->event_mouse_wheel(e);
        }
    }

    // Botones del mouse
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        // loop cut en curso: izq aplica/confirma, der centra+confirma (o cancela en preview)
        if (LoopCutActivo()) {
            if (e.button.button == SDL_BUTTON_LEFT)       LoopCutClickIzq((int)e.button.x, (int)e.button.y);
            else if (e.button.button == SDL_BUTTON_RIGHT) LoopCutClickDer();
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
        // edicion numerica por texto: un click en cualquier lado APLICA lo tipeado (no consume el click -> si fue
        // sobre otro campo, ese arranca su propia edicion en el mouse-up).
        if (NumEditActivo()) NumEditCommit();
        ViewPortClickDown = true;
        if (e.button.button == SDL_BUTTON_LEFT) {
            leftMouseDown = true;
            g_textFieldActivo = NULL; // click en cualquier lado desenfoca el texto
                                      // (ClickEn de Properties re-enfoca en el up si es una caja)
            // si habia un transform activo, este click lo ACEPTA y NO
            // debe ademas cambiar la seleccion (como en Symbian)
            bool habiaTransform = (estado == translacion ||
                                   estado == rotacion ||
                                   estado == EditScale);
            // la 'x' de una notificacion de error la cierra (esta encima de todo)
            if (NotificacionesClick((int)e.button.x, (int)e.button.y)) {
                // consumido: no propagar el click
            }
            else if (habiaTransform) {
                // durante un transform (mover/rotar/escalar, o ubicar un
                // duplicado) el click CONFIRMA, este donde este el mouse
                // (incluso sobre la barra/menu): la UI no lo consume
                if (Viewport3DActive) Viewport3DActive->Aceptar();
            }
            // la UI compartida (menu/barras/paneles) consume primero
            else if (!LayoutClickUI((int)e.button.x, (int)e.button.y)) {
                viewPortActive->button_left();
                // click sobre un viewport 3D: seleccion compartida por
                // color picking
                ViewportBase* hoja3d = FindViewportUnderMouse(
                    rootViewport, (int)e.button.x, (int)e.button.y);
                if (hoja3d && hoja3d->isLeaf() &&
                    hoja3d->ViewportKind() == 1 && estado == editNavegacion) {
                    ScenePick3D((int)e.button.x, (int)e.button.y,
                                hoja3d->x, hoja3d->y,
                                hoja3d->width, hoja3d->height,
                                W3dPantallaAlto);
                }
            }
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
            // suelta scroll agarrado y dropea el drag del outliner
            LayoutSoltar((int)e.button.x, (int)e.button.y);
        }
        else if (e.button.button == SDL_BUTTON_MIDDLE) {
            middleMouseDown = false;
        }
        GuardarMousePos();

        if (viewPortActive){
            viewPortActive->mouse_button_up(e);
        }

        viewPortActive = FindViewportUnderMouse(rootViewport, lastMouseX, lastMouseY);
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
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { if (RenameActivo()) RenameCommit(); else if (NumEditActivo()) NumEditCommit(); else g_textFieldActivo = NULL; }
            else if (k == SDLK_ESCAPE) { if (RenameActivo()) RenameCancel(); else if (NumEditActivo()) NumEditCancel(); else g_textFieldActivo = NULL; }
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
            if (k == SDLK_RETURN  || k == SDLK_KP_ENTER)      { LoopCutClickIzq(lastMouseX, lastMouseY); return; }
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
        if (e.key.keysym.sym == SDLK_TAB) {
            if (!LayoutToggleEditMode())
                LayoutCiclarViewportActivo(LShiftPressed ? -1 : +1);
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
        if (e.key.keysym.sym == SDLK_BACKSPACE && (TextFieldInputChar(8) || NumInputChar(8))) {
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
        // prioridad: una caja de texto enfocada; si no, el valor numerico del transform
        for (const char* p = e.text.text; *p; ++p)
            if (!TextFieldInputChar((unsigned char)*p)) NumInputChar((unsigned char)*p);
    }
    #endif
}