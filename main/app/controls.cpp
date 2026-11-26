#include "ViewPorts/LayoutInput.h" // ruteo compartido (menus/barras/paneles)
#include <set>
#include "ViewPorts/LayoutTree.h"  // aviso de destruccion de viewports (los gestos en curso los sueltan)
#include "edit/BoxSelect.h"
#include "ui/ViewPorts/Gizmo.h"   // GizmoDown: click/tap sobre el gizmo de mover
extern bool g_viewEditMode;   // toggle "vista" tactil (ViewPort3D_Toolbar): en pintura, el dedo navega en vez de pintar
#include "ViewPorts/Pick3D.h"
#include "render/RenderOptions.h"   // RenderType / g_redraw: son del editor
#include "ViewPorts/Properties.h" // PropertiesTouchScrollEnd (fin del scroll tactil de listas)
#include "ViewPorts/ViewPort3D.h" // Viewport3DActive->Accept() en el transform
#include "ViewPorts/PopUp/PopUpBase.h" // PopUpActive (la X cancela)
#include "ViewPorts/Timeline.h"    // DopeNumInputChar (valor numerico del transform de keyframes)
#include "ViewPorts/Editor2D.h"    // Editor2DXformActivo (envolver el cursor en G/R/S 2D)
#include "ViewPorts/TransformUI.h" // TransformUIClickBar (tap en la barra de info -> teclado numerico)
#include "ViewPorts/IDE.h"         // IDEKeyboardCapture/IDETextText (el IDE captura el teclado)
#include "ViewPorts/MenuText.h"    // el menu del texto (click derecho / mantener apretado)
#include "ViewPorts/UVEditor.h"    // UVEditorTakeTab (Tab sobre el UV = entrar/salir de huesos 2D)
#include "ViewPorts/Outliner.h"    // el menu contextual de las vistas de recursos (click derecho)
#include "WhiskUI/draw/glesdraw.h"        // W3dScreenHeight
#include "WhiskUI/widgets/card.h"         // CardUV: el circulo de carga de la pulsacion larga
#include "WhiskUI/theme/colors.h"         // SetColorID
#include "objects/Textures.h"             // el atlas de la UI
#include <cmath>
#include "controls.h"
#include "Undo.h" // Ctrl+Z
#include "objects/Gamepad.h"   // axisState/buttonState: se sueltan al desenchufar el mando
#include "script/BindsGame.h"  // el REGISTRO compartido de controles (hotplug + avisos + lua)
#include "input/W3dVibrate.h"     // el agarre del outliner vibra
#include "script/W3dScript.h"   // W3dScriptGamepadsVibrateHook: vibrate() de lua en los mandos
#include "app/W3dShortcuts.h"   // atajos de teclado reasignables

// ---------------------------------------------------------------------------------------------------------------
//  SDL -> el evento propio del editor (ViewPorts/W3dInput.h). Los viewports no hablan SDL: mientras lo hicieron,
//  sus 4 metodos de input se compilaban afuera en Symbian y el telefono tenia que reinventar cada tecla por su
//  cuenta. Aca es donde PC hace SU traduccion; el telefono hace la suya con TKeyEvent y de ahi para adentro es
//  todo el mismo codigo.
//  Los imprimibles ya coinciden (SDLK_g es 'g'): solo hay que mapear los que no tienen ASCII.
// ---------------------------------------------------------------------------------------------------------------
// La identidad de los imprimibles no es una suposicion: si SDL cambiara de numeracion, esto no compila en vez de
// morirse en silencio dejando todos los atajos rotos.
// (Symbian compila con RVCT 2.2, C++03: sin static_assert. Usa W3dSdlMin.h, con los mismos numeros)
#ifndef W3D_SYMBIAN
static_assert(SDLK_g == W3dK_G && SDLK_x == W3dK_X && SDLK_0 == W3dK_0 && SDLK_SPACE == W3dK_SPACE,
              "SDL dejo de numerar los imprimibles por su ASCII: revisar W3dTeclaDesdeSDL");
static_assert(SDL_BUTTON_LEFT == W3dMB_IZQ && SDL_BUTTON_MIDDLE == W3dMB_MIDDLE && SDL_BUTTON_RIGHT == W3dMB_DER,
              "SDL cambio los numeros de los botones: revisar W3dBotonDesdeSDL");
#endif

static int W3dKeyFromSDL(SDL_Keycode k){
    switch (k){
        case SDLK_RETURN:    return W3dK_RETURN;
        case SDLK_ESCAPE:    return W3dK_ESCAPE;
        case SDLK_BACKSPACE: return W3dK_BACKSPACE;
        case SDLK_DELETE:    return W3dK_DELETE;
        case SDLK_TAB:       return W3dK_TAB;
        case SDLK_HOME:      return W3dK_HOME;
        case SDLK_END:       return W3dK_END;
        case SDLK_PAGEUP:    return W3dK_PGUP;
        case SDLK_PAGEDOWN:  return W3dK_PGDN;
        case SDLK_F1:        return W3dK_F1;
        case SDLK_F2:        return W3dK_F2;
        case SDLK_F3:        return W3dK_F3;
        case SDLK_F4:        return W3dK_F4;
        case SDLK_F5:        return W3dK_F5;
        case SDLK_F6:        return W3dK_F6;
        case SDLK_F7:        return W3dK_F7;
        case SDLK_F8:        return W3dK_F8;
        case SDLK_F9:        return W3dK_F9;
        case SDLK_F10:       return W3dK_F10;
        case SDLK_F11:       return W3dK_F11;
        case SDLK_F12:       return W3dK_F12;
        case SDLK_LEFT:      return W3dK_LEFT;
        case SDLK_RIGHT:     return W3dK_RIGHT;
        case SDLK_UP:        return W3dK_UP;
        case SDLK_DOWN:      return W3dK_DOWN;
        case SDLK_LSHIFT:    return W3dK_LSHIFT;
        case SDLK_LCTRL:     return W3dK_LCTRL;
        case SDLK_LALT:      return W3dK_LALT;
        case SDLK_KP_0:      return W3dK_KP_0;
        case SDLK_KP_1:      return W3dK_KP_1;
        case SDLK_KP_2:      return W3dK_KP_2;
        case SDLK_KP_3:      return W3dK_KP_3;
        case SDLK_KP_4:      return W3dK_KP_4;
        case SDLK_KP_5:      return W3dK_KP_5;
        case SDLK_KP_6:      return W3dK_KP_6;
        case SDLK_KP_7:      return W3dK_KP_7;
        case SDLK_KP_8:      return W3dK_KP_8;
        case SDLK_KP_9:      return W3dK_KP_9;
        case SDLK_KP_ENTER:  return W3dK_KP_ENTER;
        case SDLK_KP_PERIOD: return W3dK_KP_PERIOD;
#ifndef W3D_SYMBIAN
        case SDLK_KP_DIVIDE: return W3dK_SLASH;   // el "/" del teclado numerico = la Vista local (como el "/")
#endif
        default: break;
    }
    if (k >= 32 && k < 127) return (int)k;   // imprimible: vale su ASCII
    return W3dK_NOTHING;
}
// La vuelta: un atajo reasignado reescribe el evento SDL con la tecla por defecto de su funcion, asi los atajos
// globales de aca abajo (que miran el SDL_Event) y los viewports (que miran la W3dKey) ven lo mismo.
static SDL_Keycode W3dKeyASDL(int t){
    static const SDL_Keycode sdl[] = { SDLK_RETURN, SDLK_ESCAPE, SDLK_BACKSPACE, SDLK_DELETE, SDLK_TAB, SDLK_HOME,
        SDLK_END, SDLK_PAGEUP, SDLK_PAGEDOWN, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8,
        SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12, SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_KP_0, SDLK_KP_1,
        SDLK_KP_2, SDLK_KP_3, SDLK_KP_4, SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8, SDLK_KP_9, SDLK_KP_ENTER,
        SDLK_KP_PERIOD };
    for (size_t i = 0; i < sizeof(sdl) / sizeof(sdl[0]); i++) if (W3dKeyFromSDL(sdl[i]) == t) return sdl[i];
    if (t >= 32 && t < 127) return (SDL_Keycode)t;
    return SDLK_UNKNOWN;
}
SDL_Keycode W3dKeyPublicASDL(int t){ return W3dKeyASDL(t); }   // (el harness: teclareal)
static int W3dModsFromSDL(Uint16 m){
    return ((m & KMOD_SHIFT) ? W3dMOD_SHIFT : 0) | ((m & KMOD_CTRL) ? W3dMOD_CTRL : 0) | ((m & KMOD_ALT) ? W3dMOD_ALT : 0);
}
static Uint16 W3dModsASDL(int m){
    return (Uint16)(((m & W3dMOD_SHIFT) ? KMOD_LSHIFT : 0) | ((m & W3dMOD_CTRL) ? KMOD_LCTRL : 0) | ((m & W3dMOD_ALT) ? KMOD_LALT : 0));
}
// tecla APRETADA -> la que se mando en su lugar (para que el SOLTAR llegue como la misma tecla que se apreto)
static std::map<SDL_Keycode, SDL_Keycode> g_translatedKeys;

// un popup NO MODAL abierto (Buscar del IDE) con el puntero afuera: el teclado y la rueda siguen yendo al viewport
static bool PopUpLeavesPass() {
    if (!PopUpActive) return true;
    if (!PopUpActive->NoModal()) return false;
    int mx, my; SDL_GetMouseState(&mx, &my);
    return !PopUpActive->Contains(mx, my);
}

static int W3dButtonFromSDL(Uint8 b){
    if (b == SDL_BUTTON_LEFT)   return W3dMB_IZQ;
    if (b == SDL_BUTTON_MIDDLE) return W3dMB_MIDDLE;
    if (b == SDL_BUTTON_RIGHT)  return W3dMB_DER;
    return W3dMB_NONE;
}
void RebindMaterialMeshPart(); // (def en Properties.cpp) refresca el panel de material tras undo/redo
extern bool BoneGrabActive();  // (def en BoneEdit.cpp) hay drag/transform de huesos (no envolver el cursor)

// rename de mesh part / material (def en Properties.cpp): el textfield se acepta/cancela aca
extern bool RenameActive();
extern void RenameCommit();
extern void RenameCancel();
// edicion numerica por texto de un PropFloat (click/OK -> tipear + enter). Declarado en WhiskUI/PropFloat.
extern bool NumEditActive();
extern void NumEditCommit();
extern void NumEditCancel();
extern void NumEditExitFromPanel(); // limpia el 'editando' del panel al terminar (sino te clava en la propiedad)

std::map<SDL_FingerID, Finger> fingers;
static Finger FingerIn(float x, float y) { Finger f; f.x = x; f.y = y; return f; }   // (C++03: sin {x, y})
// DOBLE TAP DE 2 DEDOS (tactil): dos toques cortos con dos dedos, sin moverlos, sobre el viewport 3D = encuadrar la
// seleccion (lo mismo que View > Frame Selected). El gesto se mide desde que apoya el 2do dedo hasta que se levantan
// los dos; si alguno se corrio (pinch/paneo) o tardo mucho, no es un tap.
static Uint32 g_twoFingersTicks = 0;                      // momento en que apoyo el 2do dedo (0 = sin gesto de 2 dedos)
static bool   g_twoFingersMoved = false;                  // algun dedo se corrio: era pinch/paneo
static int    g_twoFingersPx = 0, g_twoFingersPy = 0;     // centro (pixeles) al apoyar el 2do dedo
static std::map<SDL_FingerID, Finger> g_twoFingersStart;  // donde estaba cada dedo al apoyar el 2do
static Uint32 g_lastTap2Ticks = 0;                        // ultimo tap de 2 dedos (para detectar el doble)
// PINTURA TACTIL (pesos / vertex color): el viewport 3D necesita saber si el down que le llega vino de un
// dedo (para ESPERAR antes de pintar: un 2do dedo en esa ventana es un gesto de camara, no un trazo) y
// cuantos dedos hay apoyados. g_simulatedFingers (>= 0) lo pisa para los tests del harness.
bool g_lastDownTouch = false;
static int g_simulatedFingers = -1;
int  W3dActiveFingers() { return (g_simulatedFingers >= 0) ? g_simulatedFingers : (int)fingers.size(); }
void W3dSimulateFingers(int n) { g_simulatedFingers = n; }
extern void WP3DPaintCancelPending();   // ViewPort3D.cpp: un trazo que esperaba se descarta (2do dedo)
float lastDistance = 0.0f;
float PINCHposY = 0.0f;
float lastCentroidX = 0.0f, lastCentroidY = 0.0f; // 2 dedos: punto medio anterior (para el paneo)
static float lastSepX = 0.0f, lastSepY = 0.0f;    // 2 dedos: separacion anterior por eje (el zoom por eje de las curvas)
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
static bool g_view3dTapTouch   = false;   // el toque pendiente del 3D es de un DEDO (la pulsacion larga solo es tactil)
static bool g_view3dMenuLength = false;   // la pulsacion larga abrio el menu contextual: el dedo que sigue apoyado no
                                         // orbita, y al levantarlo no se elige nada (la opcion se toca despues)
bool g_view3dTapPending = false;    // toque sobre el VIEWPORT 3D (tactil): si LEVANTA sin arrastrar = TAP (selecciona);
                                    // si arrastra = orbita/panea (NO cambia la seleccion). Antes se perdia al orbitar.
ViewportBase* g_barTapView = NULL;  // viewport cuya barra/contenido se toco (para lockear el scroll a ESE, no al hover)
bool g_popupFinger = false;         // un dedo esta manejando el popup activo (bypass de la sintesis de mouse de SDL)
bool g_popupOpenedByMouseDown = false; // el popup lo abrio el mouse-down de ESTE toque (menu->action): el FINGERDOWN
                                    // que sigue NO debe cerrarlo (caeria "afuera", en la posicion del item del menu)
// ESQUINA (boton de menu [0] de cada viewport) = redimension estilo esquina de Windows: ARRASTRAR
// cambia el tamaño (borde izq + sup del viewport), un TAP abre el menu desplegable. Vale mouse y touch.
ViewportBase* g_cornerVp = NULL;    // viewport cuyo boton-esquina se apreto; NULL = ninguno
bool g_cornerResizing = false;      // ya se decidio que ESTE gesto redimensiona (paso el umbral)
int g_tapStartX = 0, g_tapStartY = 0; // posicion del down (umbral: hasta que no se mueve N px sigue siendo TAP)
// ESPERA ANTI-GESTO (tactil): un toque pendiente NO se convierte en arrastre (orbita, box select, gizmo, scrub del
// timeline, scroll) hasta que pasen estos ms desde el down. Si en esa ventana cae un 2do dedo, era un pinch/paneo
// y el 1er dedo no dispara nada. Sin esto, poner un dedo y despues el otro agarraba el gizmo o scrubeaba.
static Uint32 g_tapStartTicks = 0;
static const Uint32 kWaitGestureMs = 180;
static bool g_tlTapPending = false;          // toque sobre el CONTENIDO del timeline (kind 5) esperando: tap o arrastre
static ViewportBase* g_tlTapView = NULL;
// NAVEGACION CON ALT (estilo de los programas de pintura 3D): Alt + arrastre izquierdo ORBITA y Alt + Shift +
// arrastre izquierdo DESPLAZA, ademas del boton del medio de siempre. Un click QUIETO con Alt sigue siendo el
// click de siempre (Shift+Alt+click = loop select en Edit Mode): se decide al pasar el umbral de arrastre.
bool g_altNavPend = false;        // down con Alt sobre el 3D, todavia sin decidir click / arrastre
bool g_altNavActive    = false;   // arrastre con Alt en curso: el viewport 3D orbita / desplaza (ViewPort3D.cpp)
bool g_uiTapInProgress = false;       // el LayoutClickUI que corre es un TAP tactil diferido (no un click de mouse)
Uint32 g_lastFingerTicks = 0;         // ultimo evento de DEDO: filtra los mouse FANTASMA que el browser sintetiza
                                    // tras el touch (llegan como mouse "real" y clickeaban/abrian al soltar)

// PULSACION LARGA (tactil) sobre el OUTLINER: el dedo quieto ~0,5 s AGARRA la fila (arrastrar = moverla; soltar sin
// mover = el menu contextual). Mientras tanto, arrastrar el dedo sigue siendo SCROLL (lo normal). El agarre queda en
// g_outGrab hasta levantar el dedo: el motion va derecho al outliner (no scrollea) y el up lo suelta ahi.
static const Uint32 kLongPressMs = 500;
// el dedo se ALEJO del punto donde se apoyo (mas que el umbral de un toque). Una pulsacion larga es un dedo QUIETO:
// un arrastre rapido que entra entero en la espera anti-gesto (kWaitGestureMs) no limpiaba lo pendiente y, con el
// dedo parado despues, abria el menu contextual
static bool g_tapMoved = false;
// el AGARRE de la biblioteca / el outliner: medio segundo con el dedo QUIETO (como el menu de la vista 3D y del
// timeline; con un segundo tardaba mucho) y el telefono vibra: recien ahi arrastrar mueve el elemento; antes, arrastrar
// es scroll. Que no se confunda con empezar a scrollear lo cuida g_tapMoved (el dedo se alejo: ya no es agarre)
static const Uint32 kGrabOutlinerMs = 500;
// DONDE va el circulo de carga de la pulsacion larga y donde se abre su menu: arriba y a la izquierda del dedo (el dedo
// tapa donde se apoya); pegado a un borde, del otro lado
static void LoadFingerPlace(int px, int py, int& cx, int& cy) {
    const int gs = GlobalScale > 0 ? GlobalScale : 1;
    const int r = 9 * gs;
    cx = px - 22 * gs; cy = py - 36 * gs;
    if (cx - r < 0) cx = px + 22 * gs;
    if (cy - r < 0) cy = py + 36 * gs;
}
// el aviso de que la pulsacion larga ya esta: 45 ms (el VibHit de 32 ms casi no se sentia). Solo el telefono: un mando
// conectado no tiene por que vibrar por tocar la pantalla
static void VibrateLongPress() {
    void (*gamepads)(int, float) = W3dScriptGamepadsVibrateHook;
    W3dScriptGamepadsVibrateHook = NULL;
    w3dEngine::VibrateMs(45, 1.0f);
    W3dScriptGamepadsVibrateHook = gamepads;
}
// IDE TACTIL: la lupa (pulsacion larga) o una agarradera de la seleccion en curso; mientras dura, el dedo es de el
static IDE* g_ideGestureVp = NULL;
static bool g_outGrab = false;
static bool g_contentTapTouch = false;   // el toque de contenido pendiente es de un DEDO (el mouse no agarra asi)
// DOBLE TAP (tactil): dos toques cortos seguidos en el mismo lugar = el doble click (renombrar en el outliner)
static Uint32 g_lastTapTicks = 0;
static int g_lastTapX = -1000, g_lastTapY = -1000;

// ============================================================================================================
//  SCROLL CON INERCIA (tactil): el contenido que se scrolleaba con el dedo SIGUE DESLIZANDO al soltarlo, y frena solo
//  (como en el telefono). Mientras el dedo arrastra se guardan los ultimos movimientos; al levantarlo, si venia rapido,
//  ese envion se sigue aplicando cuadro a cuadro con rozamiento. Vale para lo que scrollea con un dedo: el contenido
//  de los paneles (Propiedades, outliner, biblioteca, consola, codigo), las barras de botones y el paneo del UV.
//  Un toque nuevo lo FRENA en seco, y ese toque no elige nada (es para parar, como en el telefono).
// ============================================================================================================
struct SampleScroll { Uint32 t; float dx, dy; };
static const int kSamplesScroll = 8;
static SampleScroll g_inSamples[kSamplesScroll];
static int g_inSamplesN = 0, g_inSamplesPos = 0;
static ViewportBase* g_inVp = NULL;      // el viewport que sigue deslizando (NULL = nada)
static bool  g_inBar = false;            // la barra de botones (horizontal) o el contenido
static float g_inVx = 0.0f, g_inVy = 0.0f;         // velocidad (pixeles por milisegundo)
static float g_inRestX = 0.0f, g_inRestY = 0.0f;   // fraccion de pixel que todavia no se aplico
static Uint32 g_inLast = 0;
static int g_inPx = 0, g_inPy = 0;       // donde se levanto el dedo (event_finger_scroll lo recibe)
static int g_inStill = 0;                // cuadros seguidos sin que el contenido se mueva (llego al tope)
static bool g_inBrakeTouch = false;      // el toque en curso freno una inercia: al soltarlo no es un tap
static void InertiaSamplesClear() { g_inSamplesN = 0; g_inSamplesPos = 0; }
static void InertiaSample(int dx, int dy) {
    SampleScroll& m = g_inSamples[g_inSamplesPos];
    m.t = SDL_GetTicks(); m.dx = (float)dx; m.dy = (float)dy;
    g_inSamplesPos = (g_inSamplesPos + 1) % kSamplesScroll;
    if (g_inSamplesN < kSamplesScroll) g_inSamplesN++;
}
static void InertiaStop() {
    if (!g_inVp) return;
    g_inVp = NULL;
    if (!g_inBar) PropertiesTouchScrollEnd();   // el latch de la lista que se scrolleaba (Propiedades) se suelta ahora
    g_inVx = g_inVy = 0.0f;
}
bool W3dInertiaActive() { return g_inVp != NULL; }
// al levantar el dedo: la velocidad de los ultimos ~100 ms. true = arranco a deslizar solo
static bool InertiaStart(ViewportBase* vp, bool bar, int px, int py) {
    InertiaStop();
    if (!vp || g_inSamplesN == 0) return false;
    const Uint32 now = SDL_GetTicks();
    const int last = (g_inSamplesPos + kSamplesScroll - 1) % kSamplesScroll;
    if (now - g_inSamples[last].t > 60) { InertiaSamplesClear(); return false; }   // el dedo ya estaba quieto
    float sx = 0.0f, sy = 0.0f; Uint32 t0 = now;
    for (int k = 0; k < g_inSamplesN; k++) {
        const SampleScroll& m = g_inSamples[(g_inSamplesPos + kSamplesScroll - 1 - k) % kSamplesScroll];
        if (now - m.t > 100) break;
        sx += m.dx; sy += m.dy; t0 = m.t;
    }
    InertiaSamplesClear();
    float dt = (float)(now - t0); if (dt < 16.0f) dt = 16.0f;
    float vx = sx / dt, vy = sy / dt;
    if (bar) vy = 0.0f;
    const float gs = (float)(GlobalScale > 0 ? GlobalScale : 1);
    const float vMax = 6.0f * gs;                            // tope: un manotazo no tira la lista a la luna
    const float v = sqrtf(vx * vx + vy * vy);
    if (v < 0.12f * gs) return false;                        // soltado despacio: se queda donde esta
    if (v > vMax) { vx *= vMax / v; vy *= vMax / v; }
    g_inVp = vp; g_inBar = bar; g_inVx = vx; g_inVy = vy;
    g_inRestX = g_inRestY = 0.0f; g_inLast = now; g_inPx = px; g_inPy = py; g_inStill = 0;
    g_redraw = true;
    return true;
}
// PESTANIAS DE PROPIEDADES con el dedo: si la barra ENTRA ENTERA (no hay nada que scrollear), deslizar el dedo por las
// pestanias las va mostrando (la que esta abajo del dedo se activa al pasar) y soltar deja la ultima. Si la barra no
// entra, deslizar la scrollea como siempre.
static Properties* g_tabSlide = NULL;
static bool BarEntersWhole(ViewportBase* v) {
    if (!v || v->barScrollX != 0) return false;
    int right = 0;
    for (size_t i = 0; i < v->BarButtons.size(); i++) { Button* b = v->BarButtons[i];
        if (b->visible && b->sx > -9000 && b->sx + b->width > right) right = b->sx + b->width; }
    for (size_t i = 0; i < v->BarTabs.size(); i++) { Tab* t = v->BarTabs[i];
        if (t->visible && t->sx + t->width > right) right = t->sx + t->width; }
    return right <= v->x + v->width;
}
// la pestania de abajo del dedo (por X: el dedo puede irse un poco de la barra para arriba o para abajo)
static void TabSlideIn(int mx) {
    if (!g_tabSlide) return;
    for (size_t i = 0; i < g_tabSlide->BarTabs.size(); i++) { Tab* t = g_tabSlide->BarTabs[i];
        if (!t->visible || mx < t->sx || mx >= t->sx + t->width) continue;
        if ((int)i != g_tabSlide->activeTab) { g_tabSlide->ClickTab(mx, t->sy + t->height / 2); g_redraw = true; }
        // el HOVER sigue al dedo (si no, la pestania donde se apoyo quedaba iluminada junto con la nueva)
        g_tabSlide->BarHover(mx, t->sy + t->height / 2);
        return; }
}
static bool TabUnder(ViewportBase* v, int mx, int my) {
    for (size_t i = 0; i < v->BarTabs.size(); i++) if (v->BarTabs[i]->visible && v->BarTabs[i]->Contains(mx, my)) return true;
    return false;
}
// el MENU DESPLEGABLE con el dedo: mantener, deslizar y soltar (LayoutInput.cpp)
extern bool LayoutMenuTouchStart(int mx, int my, bool fromLength);
extern bool LayoutMenuTouchActive();
extern void LayoutMenuTouchMove(int mx, int my);
extern void LayoutMenuTouchRelease(int mx, int my);
extern void LayoutMenuTouchCancel();
extern void LayoutMenuTouchTick(unsigned nowMs);
// un cuadro de la inercia (lo llama ControlsTick)
static void InertiaTick() {
    if (!g_inVp) return;
    if (!fingers.empty() || leftMouseDown) { InertiaStop(); return; }   // (un dedo apoyado la frena: ver el down)
    const Uint32 now = SDL_GetTicks();
    float dt = (float)(now - g_inLast); g_inLast = now;
    if (dt <= 0.0f) { g_redraw = true; return; }
    if (dt > 50.0f) dt = 50.0f;                              // un cuadro trabado no da un salto
    // ROZAMIENTO: la velocidad cae a la mitad cada ~350 ms (el ritmo del scroll de los telefonos)
    const float f = powf(0.998f, dt);
    g_inVx *= f; g_inVy *= f;
    g_inRestX += g_inVx * dt; g_inRestY += g_inVy * dt;
    const int ix = (int)g_inRestX, iy = (int)g_inRestY;
    g_inRestX -= (float)ix; g_inRestY -= (float)iy;
    if (ix || iy) {
        // (el tope se mide con el scroll del panel; la vista del timeline -frames y valor- no tiene tope que medir asi)
        Scrollable* sc = (g_inBar || g_inVp->ViewportKind() == 5) ? NULL : g_inVp->AsScrollable();
        const int px0 = sc ? sc->PosX : 0, py0 = sc ? sc->PosY : 0;
        if (g_inBar) g_inVp->BarScrollBy(ix);
        else           g_inVp->event_finger_scroll(g_inPx, g_inPy, ix, iy);
        // llego al tope: ya no se mueve -> se corta (no seguir redibujando un panel quieto)
        if (sc && sc->PosX == px0 && sc->PosY == py0) { if (++g_inStill >= 3) { InertiaStop(); g_redraw = true; return; } }
        else g_inStill = 0;
    }
    const float gs = (float)(GlobalScale > 0 ? GlobalScale : 1);
    if (sqrtf(g_inVx * g_inVx + g_inVy * g_inVy) < 0.02f * gs) InertiaStop();
    g_redraw = true;
}

void ControlsTick() {
    InertiaTick();
    LayoutMenuTouchTick(SDL_GetTicks());   // el dedo en el borde de un menu largo lo scrollea
    // PULSACION LARGA sobre la VISTA 3D (tactil): el dedo quieto medio segundo es el click DERECHO de la PC: abre el
    // menu contextual donde esta el dedo (Objeto en el Modo Objeto; el de la malla en Edicion; Pose). Vibra.
    if (g_view3dTapPending && g_view3dTapTouch && fingers.size() == 1 && !g_tapMoved && state == editNavigation &&
        g_barTapView && g_barTapView->isLeaf() && g_barTapView->ViewportKind() == 1) {
        if (SDL_GetTicks() - g_tapStartTicks < kLongPressMs) { g_redraw = true; return; }   // (sigue esperando)
        g_view3dTapPending = false;      // ya no es un toque: al levantar no selecciona
        g_view3dMenuLength = true;
        ViewPortClickDown  = false;
        VibrateLongPress();
        extern void LayoutMenuContext3D(int mx, int my);
        { int mx, my; LoadFingerPlace(g_tapStartX, g_tapStartY, mx, my); LayoutMenuContext3D(mx, my); }   // (donde estaba el circulo)
        // el MISMO dedo puede deslizar hasta una opcion y soltar ahi (soltarlo quieto deja el menu abierto)
        if (LayoutMenuTouchStart(g_tapStartX, g_tapStartY, true)) g_view3dMenuLength = false;
        g_redraw = true;
        return;
    }
    // PULSACION LARGA sobre el TIMELINE (el dedo quieto medio segundo en el strip): el menu de los keyframes, el mismo
    // del click derecho. El dedo puede deslizar hasta una opcion y soltar ahi
    // (un dedo APOYADO -si el soltar se perdio, no hay dedo y no es pulsacion larga- y QUIETO desde que se apoyo)
    if (g_tlTapPending && fingers.size() == 1 && !g_tapMoved && g_tlTapView && g_tlTapView->isLeaf() && g_tlTapView->ViewportKind() == 5) {
        static Uint32 attempt = 0;
        if (SDL_GetTicks() - g_tapStartTicks < kLongPressMs) { g_redraw = true; return; }
        if (attempt != g_tapStartTicks) {
            attempt = g_tapStartTicks;
            int mx, my; LoadFingerPlace(g_tapStartX, g_tapStartY, mx, my);   // (el menu, donde estaba el circulo)
            if (((Timeline*)g_tlTapView)->MenuContext(g_tapStartX, g_tapStartY, mx, my)) {
                g_tlTapPending = false; ViewPortClickDown = false;
                VibrateLongPress();
                LayoutMenuTouchStart(g_tapStartX, g_tapStartY, true);
                g_redraw = true;
            }
        }
        return;
    }
    if (!g_contentTapPending || !g_contentTapTouch || g_outGrab || fingers.size() > 1) return;
    // IDE: dedo quieto un rato sobre el codigo = la LUPA para poner el cursor justo
    if (g_barTapView && g_barTapView->isLeaf() && g_barTapView->ViewportKind() == 8) {
        if (SDL_GetTicks() - g_tapStartTicks < kLongPressMs) { g_redraw = true; return; }   // (sigue esperando)
        IDE* ide = (IDE*)g_barTapView;
        if (ide->PressLong(g_tapStartX, g_tapStartY)) {
            g_contentTapPending = false;   // ya no es ni tap ni scroll
            g_ideGestureVp = ide;
            viewPortActive = ide;
            ViewPortClickDown = true;
            g_redraw = true;
        }
        return;
    }
    // PROPERTIES: el dedo quieto medio segundo sobre una fila de valor = el click derecho: su menu (restablecer, copiar,
    // pegar, los keyframes). Mientras espera, arriba del dedo gira el circulo de carga (LoadFingerRender). El dedo
    // puede deslizar hasta una opcion y soltar ahi
    if (g_barTapView && g_barTapView->isLeaf() && g_barTapView->ViewportKind() == 3) {
        if (g_tapMoved) return;
        // UNA FILA DE UNA LISTA (partes de malla, modificadores, capas...): mantener apretado la AGARRA. Mientras espera
        // el telefono VIBRA; cuando deja de vibrar ya esta agarrada: arrastrar la reordena y soltar ahi abre sus opciones
        Properties* pp = (Properties*)g_barTapView;
        const bool inRow = pp->ListHasRow(g_tapStartX, g_tapStartY);
        if (inRow) {
            static Uint32 vibrating = 0;
            const Uint32 past = SDL_GetTicks() - g_tapStartTicks;
            if (past >= 150 && vibrating != g_tapStartTicks) {
                vibrating = g_tapStartTicks;
                void (*gamepads)(int, float) = W3dScriptGamepadsVibrateHook;
                W3dScriptGamepadsVibrateHook = NULL;
                w3dEngine::VibrateMs((int)(kLongPressMs - 150), 0.6f);
                W3dScriptGamepadsVibrateHook = gamepads;
            }
            if (past < kLongPressMs) { g_redraw = true; return; }
            static Uint32 grab = 0;
            if (grab != g_tapStartTicks) {
                grab = g_tapStartTicks;
                if (pp->ListGrab(g_tapStartX, g_tapStartY)) {
                    g_contentTapPending = false;   // ya no es ni tap ni scroll: es un agarre
                    g_outGrab = true;
                    viewPortActive = pp;
                    ViewPortClickDown = true;
                    g_redraw = true;
                }
            }
            return;
        }
        if (SDL_GetTicks() - g_tapStartTicks < kLongPressMs) { g_redraw = true; return; }   // (sigue esperando)
        static Uint32 attempt = 0;
        if (attempt != g_tapStartTicks) {
            attempt = g_tapStartTicks;
            int mx, my; LoadFingerPlace(g_tapStartX, g_tapStartY, mx, my);   // (el menu, donde estaba el circulo)
            if (((Properties*)g_barTapView)->MenuContext(g_tapStartX, g_tapStartY, mx, my)) {
                g_contentTapPending = false;   // ya no es ni tap ni scroll
                ViewPortClickDown = false;
                VibrateLongPress();
                LayoutMenuTouchStart(g_tapStartX, g_tapStartY, true);
                g_redraw = true;
            }
        }
        return;
    }
    if (!g_barTapView || !g_barTapView->isLeaf() || !VpIsOutliner(g_barTapView) || g_tapMoved) return;
    if (SDL_GetTicks() - g_tapStartTicks < kGrabOutlinerMs) { g_redraw = true; return; }   // (sigue esperando)
    if (LayoutLongPressUI(g_tapStartX, g_tapStartY)) {
        VibrateLongPress();   // el aviso: ya esta agarrado, ahora se arrastra
        g_contentTapPending = false;   // ya no es ni tap ni scroll: es un agarre
        g_outGrab = true;
        ViewPortClickDown = true;
        g_redraw = true;
    }
}

// ============================================================================
//  EL CIRCULO DE CARGA arriba del dedo (tactil): mientras una PULSACION LARGA espera (el dedo quieto sobre Properties,
//  la biblioteca / el outliner, el timeline o la vista 3D) gira arriba y a la izquierda del dedo un circulo de
//  cuadraditos -pixelado, como la letra de la UI-: puntos blancos que se van poniendo verdes hasta que sale el menu
//  (ahi mismo) o el agarre. Un toque corto no lo muestra
//  (aparece a los 120 ms) y se va si el dedo se mueve o se levanta. Lo dibuja main.cpp encima de todo.
// ============================================================================
static bool LongPressInWait(int& px, int& py, float& frac) {
    if (fingers.size() != 1 || g_tapMoved) return false;
    Uint32 threshold = 0;
    if (g_view3dTapPending && g_view3dTapTouch && state == editNavigation && g_barTapView && g_barTapView->isLeaf() &&
        g_barTapView->ViewportKind() == 1) threshold = kLongPressMs;
    else if (g_tlTapPending && g_tlTapView && g_tlTapView->isLeaf() && g_tlTapView->ViewportKind() == 5) threshold = kLongPressMs;
    else if (g_contentTapPending && g_contentTapTouch && !g_outGrab && g_barTapView && g_barTapView->isLeaf()) {
        if (g_barTapView->ViewportKind() == 3) threshold = kLongPressMs;
        else if (VpIsOutliner(g_barTapView)) threshold = kGrabOutlinerMs;
    }
    if (!threshold) return false;
    const Uint32 t = SDL_GetTicks() - g_tapStartTicks;
    if (t < 120 || t >= threshold) return false;
    px = g_tapStartX; py = g_tapStartY;
    frac = (float)(t - 120) / (float)(threshold - 120);
    return true;
}
void LoadFingerRender(int screenW, int screenH) {
    int px, py; float frac;
    if (!LongPressInWait(px, py, frac) || Textures.empty() || !Textures[0]) return;
    w3dEngine::Viewport(0, 0, screenW, screenH);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::MatrixMode(w3dEngine::Projection); w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, screenW, screenH, 0, -1, 1);
    w3dEngine::MatrixMode(w3dEngine::ModelView); w3dEngine::LoadIdentity();
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::BindTexture(Textures[0]->iID);
    const int gs = GlobalScale > 0 ? GlobalScale : 1;
    const int n = 12;                       // los cuadraditos de la ronda
    const float radius = 9.0f * gs;
    const int side = 3 * gs;                // (la grilla de la UI: se ve pixelado, como las letras)
    int cx, cy;
    LoadFingerPlace(px, py, cx, cy);         // ARRIBA y a la IZQUIERDA del dedo (ahi mismo se abre el menu)
    const float turn = (float)SDL_GetTicks() * 0.006f;
    const int full   = (int)(frac * n + 0.5f);
    const GLfloat u = CardUV[5 * 2], v = CardUV[5 * 2 + 1];   // un texel del relleno de la tarjeta: un color liso
    GLfloat uv[8] = { u, v, u, v, u, v, u, v };
    for (int i = 0; i < n; i++) {
        const float a = turn + 6.2831853f * (float)i / (float)n;
        int qx = cx + (int)(cosf(a) * radius) - side / 2;
        int qy = cy + (int)(sinf(a) * radius) - side / 2;
        qx -= qx % gs; qy -= qy % gs;
        // puntos BLANCOS que se van poniendo VERDES hasta completar la ronda (y ahi sale el menu)
        if (i < full) SetColorID(ColorID::accent); else SetColorID(ColorID::white, 0.9f);
        GLshort q[8] = { (GLshort)qx, (GLshort)qy, (GLshort)(qx + side), (GLshort)qy,
                         (GLshort)qx, (GLshort)(qy + side), (GLshort)(qx + side), (GLshort)(qy + side) };
        W3dDrawStrip4(q, uv);
    }
    g_redraw = true;   // sigue girando
}

void Counters(){
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

// viewport del juego bajo el dedo/mouse en PLAY (antes era static LOCAL de InputUserSDL3; se subio
// a file-scope para poder alimentar TODOS los dedos al juego por frame, ver FeedFingersGame).
static ViewportBase* g_touchGameVp = NULL;

// los gestos en curso de este archivo apuntan a viewports del layout: si uno muere (abrir un
// proyecto libera el layout anterior, cambiar el tipo de un viewport lo reemplaza) se sueltan
// (LayoutTree.h). El gesto se pierde; el puntero colgado seria un crash al levantar el dedo.
static void ControlsForgetViewport(ViewportBase* vp) {
    if (g_scrollView == vp)    g_scrollView = NULL;
    if (g_inVp == vp)          InertiaStop();
    Gizmo2DForgetVp(vp);      // (el gizmo 2D agarrado en ese viewport)
    if ((ViewportBase*)g_tabSlide == vp) g_tabSlide = NULL;
    if (g_barTapView == vp)    g_barTapView = NULL;
    if (g_cornerVp == vp)    { g_cornerVp = NULL; g_cornerResizing = false; }
    if (g_tlTapView == vp)   { g_tlTapView = NULL; g_tlTapPending = false; }
    if (g_touchGameVp == vp)  g_touchGameVp = NULL;
    if ((ViewportBase*)g_ideGestureVp == vp) g_ideGestureVp = NULL;
}
struct ControlsHookForget { ControlsHookForget() { ViewportForgetRegister(ControlsForgetViewport); } };
static ControlsHookForget g_controlsHookForget;

// LOS MANDOS (vibrar + enchufar en caliente) son de SDL: Symbian no tiene SDL y sus controles BT entran por otra
// via. Hasta el fin de la seccion de mandos queda solo para las plataformas con SDL.
#ifndef W3D_SYMBIAN
// vibrate() de lua en los MANDOS (W3dScriptGamepadsVibrateHook de script/W3dScript.h): todos los conectados, la fuerza en
// los dos motores. (SDL 2.0.7, la de Android 2.3, no tiene rumble: ahi vibra el telefono)
static void VibrateGamepads(int ms, float force) {
#if SDL_VERSION_ATLEAST(2, 0, 9)
    const Uint16 v = (Uint16)(force * 65535.0f);
    for (int i = 0; i < W3dControlsCount(); i++) {
        SDL_GameController* gc = (SDL_GameController*)W3dControlHandle(i);
        if (gc) SDL_GameControllerRumble(gc, v, v, (Uint32)ms);
    }
#else
    (void)ms; (void)force;
#endif
}
struct ControlsHookVibrate { ControlsHookVibrate() { W3dScriptGamepadsVibrateHook = VibrateGamepads; } };
static ControlsHookVibrate g_controlsHookVibrate;
#endif

// MULTI-TOUCH del juego: alimenta cada dedo tactil activo al script (finger(1), finger(2), ...) durante el
// play. Aditivo: si NO hay dedos (mouse/desktop) no hace nada -> el dedo 0 lo sigue manejando
// SimTouchScreen (mouse). Lo llama SimTickPlay cada frame. Asi el 2 jugadores tactil tiene un dedo
// por pala (el whiskpaddle.lua reparte por la X: mitad izquierda = jugador 1, derecha = jugador 2).
void FeedFingersGame() {
    if (fingers.empty() || !g_touchGameVp) return;
    extern bool SimActive(); extern bool PlayAnimation;
    if (!SimActive() || !PlayAnimation) return;
    extern void SimTouchScreenFinger(ViewportBase*, int, int, int, bool);
    int ww = 0, hh = 0; if (window) SDL_GetWindowSize(window, &ww, &hh);
    int i = 0;
    for (std::map<SDL_FingerID, Finger>::iterator it = fingers.begin(); it != fingers.end() && i < 4; ++it, ++i)
        SimTouchScreenFinger(g_touchGameVp, i, (int)(it->second.x * ww), (int)(it->second.y * hh), true);
    for (; i < 4; i++) SimTouchScreenFinger(g_touchGameVp, i, 0, 0, false); // los que no hay: inactivos
}

#ifndef W3D_SYMBIAN
// ============================================================================
//  HOTPLUG DE CONTROLES (pedido del dueno).
//
//  Antes el mando se buscaba UNA sola vez, al arrancar el editor (main.cpp:
//  SDL_GameControllerOpen(0)): si lo enchufabas despues no existia hasta cerrar
//  y volver a abrir, y al desenchufarlo quedaba el device abierto colgado.
//  Ahora se atienden los eventos de SDL, se abre/cierra el mando EN CALIENTE, y
//  el aviso ("se detecto un nuevo control" / "se desconecto un control") lo emite
//  el registro COMPARTIDO de BindsJuego -> mismo texto, toast y linea de consola
//  en el editor y en el juego compilado.
//
//  El global `controller` sigue apuntando al PRIMERO abierto (es lo que usa el
//  resto del editor y lo que cierra la salida); sin mandos queda NULL.
// ============================================================================
static const char* ControlType(SDL_GameController* gc) {
    if (!gc) return "generic";
    switch (SDL_GameControllerGetType(gc)) {
        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE:             return "xbox";
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
        case SDL_CONTROLLER_TYPE_PS5:                 return "playstation";
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return "nintendo";
        default:                                      return "generic";
    }
}
// abre el device 'indice' (indice de DISPOSITIVO: el que trae el evento ADDED)
static void ControlOpen(int index) {
    if (!SDL_IsGameController(index)) return;      // un joystick que no es gamepad
    SDL_GameController* gc = SDL_GameControllerOpen(index);
    if (!gc) return;
    SDL_Joystick* js = SDL_GameControllerGetJoystick(gc);
    int id = js ? (int)SDL_JoystickInstanceID(js) : -1;
    if (!W3dControlAdd(gc, id, SDL_GameControllerName(gc), ControlType(gc))) {
        SDL_GameControllerClose(gc);                // ya registrado: no duplicar
        return;
    }
    if (!controller) controller = gc;
}
// cierra el device del instance id 'id' (el que trae el evento REMOVED)
static void ControlClose(int id) {
    void* h = 0;
    if (!W3dControlRemove(id, &h)) return;
    if (controller == (SDL_GameController*)h)
        controller = (SDL_GameController*)W3dControlHandle(0);   // el que quede (NULL si ninguno)
    if (h) SDL_GameControllerClose((SDL_GameController*)h);
    // SOLTAR EL ESTADO: sin esto queda pegado el ultimo valor del stick/boton y
    // el personaje sigue caminando solo con el mando ya desenchufado.
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++)   axisState[a] = 0.0f;
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) buttonState[b] = false;
}
// al arrancar: los que YA estaban enchufados (SDL ademas encola un ADDED por
// cada uno; W3dControlAdd descarta el repetido por instance id).
void W3dControlsOpenThatHas() {
    for (int i = 0; i < SDL_NumJoysticks(); i++) ControlOpen(i);
}
#endif // (fin de los mandos de SDL)

// las teclas que el JUEGO tiene apretadas (en play): su soltar va al juego siempre, y si la ventana pierde el
// foco (alt-tab) se las suelta todas (SDL no manda el KEY_UP de lo que se suelta afuera)
static std::set<int> gKeysGame;
#ifdef W3D_MCP
#include "test/W3dHistory.h"   // la grabadora de pasos (W3D_HISTORY=<ruta>): anota cada tecla y cada click
#endif
#ifndef W3D_SYMBIAN
// EL SOLTAR QUE NO LLEGO (ver "UP PERDIDO" en el movimiento): se arma el evento y entra por InputUserSDL3 como cualquier
// otro, asi corre lo MISMO que con un soltar real (la barra de scroll, el foco congelado, la caja, el mouse_button_up
// del viewport...) sin una segunda copia de ese camino
static void MouseUpLost(const SDL_Event& motion, Uint8 button) {
    SDL_Event up; memset(&up, 0, sizeof(up));
    up.type = SDL_EVENT_MOUSE_BUTTON_UP;
    up.button.timestamp = motion.motion.timestamp;
    up.button.windowID = motion.motion.windowID;
    up.button.which = motion.motion.which;
    up.button.button = button;
    up.button.state = SDL_RELEASED;
    up.button.clicks = 1;
    up.button.x = motion.motion.x;
    up.button.y = motion.motion.y;
    InputUserSDL3(up);
}
#endif
void InputUserSDL3(SDL_Event &e){
#ifdef W3D_MCP
    if (W3dHistoryActive()) W3dHistoryEvent(e);   // ANTES de procesarlo: lo que hay debajo del click
#endif
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && !gKeysGame.empty()) {
        extern void SimKeySDL(int, bool);
        for (std::set<int>::iterator it = gKeysGame.begin(); it != gKeysGame.end(); ++it) SimKeySDL(*it, false);
        gKeysGame.clear();
    }
#ifndef W3D_SYMBIAN
    RefreshInputControllerSDL(e);

    // HOTPLUG: enchufar/desenchufar con el editor abierto
    if (e.type == SDL_CONTROLLERDEVICEADDED)   { ControlOpen(e.cdevice.which);  g_redraw = true; return; }
    if (e.type == SDL_CONTROLLERDEVICEREMOVED) { ControlClose(e.cdevice.which); g_redraw = true; return; }
#endif

    switch(e.type) {
        case SDL_FINGERDOWN:
            // OJO ORDEN: SDL sintetiza el MOUSEBUTTONDOWN ANTES de pushear este FINGERDOWN -> el reset del
            // gesto va en el down del mouse (aca borraria los flags recien seteados por ese down).
            fingers[e.tfinger.fingerId] = FingerIn(e.tfinger.x, e.tfinger.y);
            g_lastFingerTicks = SDL_GetTicks();
            // POPUP activo (explorador, etc): lo maneja el DEDO directo (la sintesis de mouse de SDL se
            // desincroniza). 1 dedo = click del popup; si Click devuelve false (afuera) se cierra.
            if (PopUpActive && fingers.size() == 1) {
                // ...salvo que el popup lo ACABE de abrir el mouse-down de ESTE toque (menu Delete, etc):
                // ese finger-down caeria afuera (en la posicion del item del menu) y lo cerraria. Se ignora.
                if (g_popupOpenedByMouseDown) { g_popupOpenedByMouseDown = false; break; }
                int mx, my; FingerPix(e.tfinger, mx, my);
                if (PopUpActive->NoModal() && !PopUpActive->Contains(mx, my)) {
                    // popup NO modal (Buscar del IDE): el toque de afuera es del viewport y el popup sigue abierto
                } else {
                    leftMouseDown = true; g_popupFinger = true;
                    if (!PopUpActive->Click(mx, my)) {   // afuera del popup -> cerrarlo (como LayoutClickUI)
                        PopUpActive->Close();
                        g_popupFinger = false; leftMouseDown = false;
                    }
                    break; // el popup ya lo manejo: no seguir al flujo de gestos de viewport
                }
            }
            // LOOP CUT en fase de ORIENTACION: el dedo elige la direccion tocando un punto. Igual que el
            // popup, lo maneja el dedo DIRECTO (la sintesis touch->mouse se desincroniza). Si el mouse
            // sintetizado ya lo proceso (llega ANTES del FINGERDOWN), gLoopCutOrienting ya es false y esto
            // no hace nada; si se desincronizo, este es el unico camino que registra el toque.
            if (LoopCutActive() && LoopCutOrienting() && fingers.size() == 1) {
                int mx, my; FingerPix(e.tfinger, mx, my);
                LoopCutClickLeft(mx, my);
                break;
            }
            if (fingers.size() >= 2) {
                // 2do dedo -> arranca el gesto de 2 dedos: cortamos el orbit/drag del mouse (1 dedo
                // sintetizado) y cancelamos el gesto de 1 dedo a medio decidir (tap/scroll/slider).
                if (g_popupFinger) { if (PopUpActive) PopUpActive->Release(); g_popupFinger = false; }
                if (g_ideGestureVp) { g_ideGestureVp->TouchGestureEnd(); g_ideGestureVp = NULL; }   // 2do dedo: era zoom/scroll
                Gizmo2DAbort();   // el gizmo 2D agarrado con el 1er dedo (UV / 2D): era un pinch, no un transform
                LayoutMenuTouchCancel();   // (el dedo sobre un menu: no elige nada)
                g_tabSlide = NULL;
                InertiaStop();
                leftMouseDown = middleMouseDown = ViewPortClickDown = false;
                g_barTapPending = g_contentTapPending = g_slideNum = false;
                g_view3dTapPending = false; // 2do dedo (pan/zoom): NO seleccionar al soltar
                WP3DPaintCancelPending(); // ...ni pintar: el trazo que esperaba era un gesto de camara
                g_tlTapPending = false;   // ...ni scrubear el timeline
                g_scrollView = NULL;
                g_cornerVp = NULL; g_cornerResizing = false; // 2do dedo: cancela el gesto de esquina
                lastDistance = 0.0f; // re-arma pinch/paneo desde este toque
                if (fingers.size() == 2) {   // posible TAP de 2 dedos: arranca el reloj y se recuerda donde estan
                    g_twoFingersTicks = SDL_GetTicks(); g_twoFingersMoved = false; g_twoFingersStart = fingers;
                    std::map<SDL_FingerID, Finger>::iterator it = fingers.begin();
                    Finger f1 = it->second; ++it; Finger f2 = it->second;
                    SDL_TouchFingerEvent tc = e.tfinger; tc.x = (f1.x + f2.x) * 0.5f; tc.y = (f1.y + f2.y) * 0.5f;
                    FingerPix(tc, g_twoFingersPx, g_twoFingersPy);
                } else g_twoFingersTicks = 0;  // 3 dedos: no es el gesto
            }
            break;

        case SDL_FINGERUP:
            fingers.erase(e.tfinger.fingerId);
            if (fingers.empty()) { extern int g_mouseHoverX, g_mouseHoverY; g_mouseHoverX = g_mouseHoverY = -10000; }   // (sin dedo: sin cartelito ni resaltado)
            g_lastFingerTicks = SDL_GetTicks();
            lastDistance = 0.0f; // reset
            if (g_twoFingersTicks != 0 && fingers.empty()) {   // se levantaron los dos: ¿fue un TAP de 2 dedos?
                const Uint32 now = SDL_GetTicks();
                if (!g_twoFingersMoved && now - g_twoFingersTicks < 400) {
                    if (g_lastTap2Ticks != 0 && now - g_lastTap2Ticks < 500) {   // DOBLE tap: encuadrar la seleccion
                        ViewportBase* v = FindViewportUnderMouse(rootViewport, g_twoFingersPx, g_twoFingersPy);
                        if (v && v->isLeaf() && v->ViewportKind() == 1) { ((Viewport3D*)v)->FocusObject(); g_redraw = true; }
                        g_lastTap2Ticks = 0;
                    } else g_lastTap2Ticks = now;
                } else g_lastTap2Ticks = 0;
                g_twoFingersTicks = 0;
            }
            // POPUP manejado por el dedo: soltar (dispara tap/abre entrada/termina scroll) al levantar
            if (g_popupFinger) {
                if (PopUpActive) PopUpActive->Release();
                g_popupFinger = false; leftMouseDown = false;
                break;
            }
            if (fingers.size() < 2) {
                // volvimos a <2 dedos -> cortar el drag (el dedo que queda no debe "saltar" a orbitar)
                leftMouseDown = middleMouseDown = ViewPortClickDown = false;
            }
            break;

        case SDL_FINGERMOTION:
            fingers[e.tfinger.fingerId] = FingerIn(e.tfinger.x, e.tfinger.y);
            if (g_twoFingersTicks != 0 && !g_twoFingersMoved) {   // tap de 2 dedos: si un dedo se corre, era pinch/paneo
                std::map<SDL_FingerID, Finger>::iterator i0 = g_twoFingersStart.find(e.tfinger.fingerId);
                if (i0 != g_twoFingersStart.end()) {
                    int ax, ay, bx, by; FingerPix(e.tfinger, bx, by);
                    SDL_TouchFingerEvent t0 = e.tfinger; t0.x = i0->second.x; t0.y = i0->second.y; FingerPix(t0, ax, ay);
                    if (abs(bx - ax) + abs(by - ay) > 12 * GlobalScale) g_twoFingersMoved = true;
                }
            }
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
                    { extern bool SimActive();   // JUGANDO: pinch bloqueado (en pausa la inspeccion vuelve)
                      if (SimActive() && PlayAnimation &&
                          (viewPortActive->ViewportKind() == 1 || viewPortActive->ViewportKind() == 6)) {
                          lastDistance = dist; lastCentroidX = cx; lastCentroidY = cy;
                          break;
                      } }
                    // los editores 2D (timeline/curvas, UV, 2D) usan la geometria completa: zoom anclado a los dedos.
                    // (Los dedos llegan normalizados a la VENTANA: a pixeles se pasa con su tamano, no el del viewport)
                    int ww = 0, hh = 0; if (window) SDL_GetWindowSize(window, &ww, &hh);
                    if (!viewPortActive->event_finger_pinch(cx * ww, cy * hh, (cx - lastCentroidX) * ww, (cy - lastCentroidY) * hh,
                                                            lastSepX * ww, lastSepY * hh, ex * ww, ey * hh))
                        viewPortActive->event_finger_gesture(zoomDelta, panDx, panDy);
                    g_redraw = true;
                }
                lastDistance = dist; lastCentroidX = cx; lastCentroidY = cy; lastSepX = ex; lastSepY = ey;
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
    // SALVO el SEMI-MODAL (la tarjeta de la ultima operacion: mover/rotar/escalar, la del timeline, la de la malla):
    // un toque AFUERA de el lo ACEPTA aca mismo, en el down sintetizado (llega ANTES del FINGERDOWN), y el toque
    // sigue su camino como si no hubiera popup: orbitar, elegir un objeto o un vertice, tocar otro viewport...
    // Antes se descartaba y el FINGERDOWN solo cerraba la tarjeta: el primer toque se perdia (como el click del mouse
    // de escritorio, que ya aceptaba y seguia: LayoutClickUI).
    if (PopUpActive && e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.which == SDL_TOUCH_MOUSEID &&
        PopUpActive->ClosesWithViewport() && !PopUpActive->Dragging() &&
        !PopUpActive->Contains((int)e.button.x, (int)e.button.y))
        PopUpActive->Close();
    // (un popup NO MODAL -Buscar del IDE, el teclado de Whisk3D escribiendo en el IDE- deja pasar el toque de AFUERA:
    //  es del viewport de abajo, como si el popup no estuviera. Sin esto, con el teclado abierto no se podia tocar el
    //  codigo para mover el cursor ni elegir una palabra)
    if (PopUpActive) {
        const bool isButton = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP);
        const int px = isButton ? (int)e.button.x : (int)e.motion.x, py = isButton ? (int)e.button.y : (int)e.motion.y;
        const bool outsideNoModal = PopUpActive->NoModal() && !PopUpActive->Contains(px, py);
        if (!outsideNoModal) {
            if (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.which == SDL_TOUCH_MOUSEID) return;
            if (isButton && e.button.which == SDL_TOUCH_MOUSEID) return;
        }
    }

    // ---- MODO JUEGO con el DEDO/MOUSE: mientras el juego esta en PLAY, tocar el
    // visor 2D o el 3D JUEGA (el arrastre llega a los scripts via touch()) y NO
    // navega el editor (sin zoom, sin rotacion, sin seleccionar). Fuera de esos
    // viewports (timeline, paneles) el editor sigue normal; SPACE pausa. (g_touchGameVp: file-scope arriba)

    if (e.type == SDL_EVENT_MOUSE_MOTION){
#ifndef W3D_SYMBIAN
        // UP PERDIDO: se mueve el MOUSE (no un dedo: ese lo maneja su gesto) y SDL dice que el boton YA NO esta apretado,
        // pero el editor nunca vio el soltar (se solto afuera de la ventana, un evento que se comio algun filtro...). Sin
        // esto seguia todo como con el boton abajo: la barra de scroll agarrada scrolleando detras del mouse, el foco
        // congelado en el viewport del click y el cursor ENVOLVIENDOSE en sus bordes (CheckWarpMouseInViewport): atrapado
        // aunque ya se hubiera soltado. Primero ese soltar (por la misma puerta que uno real) y despues el movimiento.
        // (el izquierdo solo si lo apreto el mouse: el de un dedo se suelta con el dedo)
        if (e.motion.which != SDL_TOUCH_MOUSEID && fingers.empty()) {
            if (leftMouseDown && !g_lastDownTouch && !(e.motion.state & SDL_BUTTON_LMASK)) MouseUpLost(e, SDL_BUTTON_LEFT);
            if (middleMouseDown && !(e.motion.state & SDL_BUTTON_MMASK)) MouseUpLost(e, SDL_BUTTON_MIDDLE);
        }
#endif
        { extern int g_mouseHoverX, g_mouseHoverY; g_mouseHoverX = (int)e.motion.x; g_mouseHoverY = (int)e.motion.y; }   // (el cartelito de ayuda)
        // arrastrar sobre la caja de texto enfocada: selecciona (con el boton apretado desde que se apreto ahi)
        if ((e.motion.state & SDL_BUTTON_LMASK) && TextFieldMouseDrag((int)e.motion.x)) { g_redraw = true; return; }
        if (e.motion.which == SDL_TOUCH_MOUSEID && leftMouseDown && !g_tapMoved) {
            int ddx = (int)e.motion.x - g_tapStartX; if (ddx < 0) ddx = -ddx;
            int ddy = (int)e.motion.y - g_tapStartY; if (ddy < 0) ddy = -ddy;
            if (ddx + ddy >= 8 * GlobalScale) g_tapMoved = true;
        }
        // MOUSE-OVER del juego: en PLAY, alimentar la posicion del mouse (SIN clic) al script para
        // resaltar la opcion bajo el cursor (mouse()). El clic sigue por el flujo de toque de abajo.
        { extern bool SimActive();
          if (SimActive() && PlayAnimation) {
              extern void SimMouseScreen(ViewportBase*, int, int, bool);
              ViewportBase* vg = FindViewportUnderMouse(rootViewport, (int)e.motion.x, (int)e.motion.y);
              bool isGame = vg && (vg->ViewportKind() == 6 || vg->ViewportKind() == 1);
              SimMouseScreen(vg, (int)e.motion.x, (int)e.motion.y, isGame);
          } }
        if (g_touchGameVp) {   // arrastre del juego en curso: seguir alimentando
            extern void SimTouchScreen(ViewportBase*, int, int, bool);
            SimTouchScreen(g_touchGameVp, (int)e.motion.x, (int)e.motion.y, true);
            SaveMousePos();
            return;
        }
        if (g_ideGestureVp) {     // IDE: la lupa / la agarradera siguen al dedo (nada de scroll)
            g_ideGestureVp->TouchGestureMove((int)e.motion.x, (int)e.motion.y);
            g_redraw = true;
            return;
        }
        // 2+ dedos: el mouse lo sintetiza el touch desde 1 dedo -> NO orbitar mientras se hace
        // pinch/paneo (el gesto de 2 dedos ya se maneja en SDL_FINGERMOTION).
        if (fingers.size() >= 2) return;

        // TACTIL durante un TRANSFORM: el touchStart teletransporta el mouse virtual (motion sintetizado
        // que llega ANTES del button-down) -> ese salto NO debe mover el transform (tocar el tilde de la
        // barra corria el objeto; y re-apoyar el dedo para seguir moviendo saltaba). El drag real llega
        // con leftMouseDown y pasa normal.
        if (e.motion.which == SDL_TOUCH_MOUSEID && !leftMouseDown && state != editNavigation) {
            g_xformFirstMov = true; // el proximo motion ignora su delta (absorbe el salto)
            SaveMousePos();
            return;
        }

        int mx = e.motion.x;
        int my = e.motion.y;
        { extern bool g_cursorIsFinger; g_cursorIsFinger = (e.motion.which == SDL_TOUCH_MOUSEID); }   // (ver CheckWarpMouseInViewport)

        // el dedo sobre un MENU DESPLEGABLE: resalta la opcion de abajo (soltar la elige)
        if (LayoutMenuTouchActive()) { LayoutMenuTouchMove(mx, my); SaveMousePos(); return; }
        // UN DEDO NO TIENE HOVER: SDL manda el movimiento del mouse sintetizado hasta el punto del toque ANTES del down
        // (sin boton apretado). Con un menu abierto ese movimiento lo tomaba como el hover del mouse: tocar "Malla" del
        // menu Anadir ya le abria el submenu debajo del dedo antes de apoyarlo, el toque empezaba y terminaba ADENTRO
        // del submenu y soltar elegia su primera opcion (agregaba un Plano). El resaltado con el dedo lo hace el down
        // (LayoutMenuTouchStart) y el arrastre (LayoutMenuTouchMove)
        if (e.motion.which == SDL_TOUCH_MOUSEID && e.motion.state == 0 && LayoutMenuOpen()) { SaveMousePos(); return; }

        // hover de la 'x' de las notificaciones (estan encima de todo)
        NotificationsMotion(mx, my);

        // loop cut en curso: el motion maneja el preview (sigue la arista) o el slide
        if (LoopCutActive()) { LoopCutMotion(mx, my); return; }
        // inset / bevel en curso: el mouse maneja el grosor (o la profundidad con Ctrl) / el ancho. Con el DEDO, el mouse
        // sintetizado llega al lugar del toque ANTES del toque y sin boton: ese salto no cuenta (el toque agarra desde ahi)
        if (InsetActive() || BevelActive()) {
            const bool fingerHover = e.motion.which == SDL_TOUCH_MOUSEID && e.motion.state == 0;
            if (!fingerHover) { if (InsetActive()) InsetMotion(mx, my); else BevelMotion(mx, my); }
            SaveMousePos();
            return;
        }

        // BOX SELECT: mientras esta armado la cruz sigue al cursor, y mientras se arrastra la
        // caja se estira. En los dos casos se queda con el motion: si siguiera de largo, el
        // viewport orbitaria abajo de la caja.
        if (BoxSelectActive()) { BoxSelectMove(mx, my); SaveMousePos(); return; }

        // GESTO TACTIL DE SCROLL (1 dedo arrastrado). VA ANTES de LayoutMotionUI: sino el hover de la
        // barra consume el motion sobre un boton y el scroll SOLO andaba tocando el gap entre botones.
        // Queda LOCKEADO al viewport del down (g_barTapView) hasta levantar el dedo: NO orbita ni abre
        // menus aunque el dedo se salga. El mouse lo sintetiza el touch (fingers.size()==1).
        // gesto de scroll con 1 dedo O con el mouse (fingers<=1). El scroll de BARRA (g_barTapPending) vale para
        // ambos; el de CONTENIDO de panel y el slider numerico son solo touch (en PC el mouse usa scrollbar/rueda
        // + el drag numerico clasico). Lockeado al viewport del down hasta soltar.
        // una fila del OUTLINER AGARRADA con la pulsacion larga: el dedo la arrastra (no scrollea)
        if (g_outGrab && g_barTapView) {
            g_barTapView->event_mouse_motion(mx, my);
            SaveMousePos();
            g_redraw = true;
            return;
        }
        if (g_view3dMenuLength) { SaveMousePos(); g_redraw = true; return; }   // (el dedo del menu contextual: no orbita)
        if (leftMouseDown && fingers.size() <= 1) {
            // NAVEGACION CON ALT: pasado el umbral el arrastre ORBITA (o desplaza con Shift) y el click no pickea
            if (g_altNavPend) {
                int adx = mx - g_tapStartX; if (adx < 0) adx = -adx;
                int ady = my - g_tapStartY; if (ady < 0) ady = -ady;
                if (adx + ady > 4 * GlobalScale) {
                    g_altNavPend = false; g_altNavActive = true; g_view3dTapPending = false;
                    dx = 0; dy = 0;   // el delta lo calcula el warp DESPUES: el primero arranca en cero (sin salto)
                } else { g_redraw = true; return; }   // todavia es un click
            }
            // VIEWPORT 3D en tactil: el down NO selecciono (g_view3dTapPending). Si el dedo paso el umbral,
            // es un orbit/paneo -> se cancela el tap (la seleccion NO cambia) y se sigue de largo al
            // event_mouse_motion (que orbita). Un jitter chico sigue siendo TAP (selecciona al soltar).
            if (g_view3dTapPending) {
                // TACTIL: dentro de la ventana anti-gesto el dedo no arrastra nada todavia (ver g_tapStartTicks)
                if (e.motion.which == SDL_TOUCH_MOUSEID && SDL_GetTicks() - g_tapStartTicks < kWaitGestureMs) { g_redraw = true; return; }
                int vdx = mx - g_tapStartX; if (vdx < 0) vdx = -vdx;
                int vdy = my - g_tapStartY; if (vdy < 0) vdy = -vdy;
                // todavia es un TOQUE / CLICK (no paso el umbral): no se mueve NADA. Antes este movimiento seguia de
                // largo al viewport y la camara orbitaba con el dedo puesto sobre el gizmo; recien al pasar el umbral
                // se agarraba el gizmo y giraba el objeto. Hasta no saber que es un arrastre no se sabe si es la
                // orbita, el gizmo o la caja; y si se suelta antes, es el toque (cambia el modo del gizmo o elige).
                if (vdx + vdy <= 8 * GlobalScale) { g_redraw = true; return; }
                {
                    g_view3dTapPending = false;
                    // DEDO sobre una manija del gizmo: recien ahora se agarra, desde el punto del down, y en este
                    // mismo movimiento ya sigue al dedo: el objeto gira (o se mueve, o escala) justo al pasar el umbral
                    if (e.motion.which == SDL_TOUCH_MOUSEID && GizmoDown(g_barTapView, g_tapStartX, g_tapStartY)) {
                        ViewPortClickDown = true; GizmoMotion((Viewport3D*)g_barTapView, mx, my); SaveMousePos(); return;
                    }
                    // PC: arrastrar sobre el contenido del 3D es un BOX SELECT (con el mouse ese
                    // arrastre no hacia nada: la orbita es el boton del medio). En TACTIL no, que
                    // ahi el arrastre de 1 dedo ES la orbita.
                    // ...pero NO en los modos de pintura: ahi el arrastre ES el trazo del pincel
                    // (y la seleccion no se puede cambiar). Este era el bug de "pinto y me arma la caja".
                    const bool painting = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
                    if (e.motion.which != SDL_TOUCH_MOUSEID && !BoxSelectActive() && !painting) {
                        { extern void TimelineBoxForget(); TimelineBoxForget(); }   // (esta es la del 3D)
                        BoxSelectBuild();
                        BoxSelectDown(g_tapStartX, g_tapStartY);
                        BoxSelectMove(mx, my);
                        SaveMousePos();
                        return;
                    }
                    // sigue de largo: es la ORBITA (o el paneo). Este primer paso gira TODO lo que el dedo ya se movio
                    // desde que se apoyo (la espera y el umbral no se pierden: antes arrancaba en cero y el dedo se
                    // adelantaba a la vista). Se mide desde el punto del toque, no desde el mouse de antes: el salto
                    // del touchStart (el mouse virtual se teletransporta al dedo) no cuenta
                    dx = mx - g_tapStartX; dy = my - g_tapStartY; lastMouseX = mx; lastMouseY = my;
                }
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
                    LayoutResizeCorner(g_cornerVp, e.motion.xrel, e.motion.yrel); // dx=borde izq, dy=borde sup
                    g_fingerScrolling = true; g_redraw = true; return;
                }
            }
            if (g_tabSlide) { TabSlideIn(mx); g_fingerScrolling = true; SaveMousePos(); return; }   // (pestanias)
            if (g_slideNum) {                                        // slider numerico lockeado -> el arrastre EDITA
                if (g_barTapView) g_barTapView->TouchSliderMove(e.motion.xrel);
                g_fingerScrolling = true; g_redraw = true; return;
            }
            if (g_scrollView) {                                      // scroll lockeado (barra o contenido)
                if (g_scrollBar) g_scrollView->BarScrollBy(e.motion.xrel);
                else             g_scrollView->event_finger_scroll(mx, my, e.motion.xrel, e.motion.yrel);
                if (e.motion.which == SDL_TOUCH_MOUSEID) InertiaSample(e.motion.xrel, e.motion.yrel);   // (el envion al soltar)
                g_fingerScrolling = true; g_redraw = true; return;
            }
            // gesto SIN decidir (down sobre barra o contenido de panel): sigue siendo TAP hasta pasar el
            // umbral. Recien ahi decide barra/scroll/slider. Asi un tap con jitter chico igual togglea/abre.
            if (g_tlTapPending) {                                    // TIMELINE (tactil): tap o scrub, pasada la espera
                if (SDL_GetTicks() - g_tapStartTicks < kWaitGestureMs) { g_redraw = true; return; }
                int tdx = mx - g_tapStartX; if (tdx < 0) tdx = -tdx;
                int tdy = my - g_tapStartY; if (tdy < 0) tdy = -tdy;
                if (tdx + tdy < 8 * GlobalScale) { g_redraw = true; return; } // aun es tap
                g_tlTapPending = false;
                // el DEDO sobre el PANEL de canales lo scrollea (vertical y horizontal); en el strip, con el MONITOR de la
                // toolbar prendido (el de fabrica), arrastrar MUEVE LA VISTA. El primer paso trae todo lo que el dedo ya
                // se movio desde que se apoyo (la espera y el umbral no se pierden), y al soltar sigue con inercia
                if (g_tlTapView && e.motion.which == SDL_TOUCH_MOUSEID) {
                    Timeline* tlv = (Timeline*)g_tlTapView;
                    const bool panel = tlv->PanelIn(g_tapStartX, g_tapStartY);
                    if (panel || (g_tlViewMode && tlv->DragMovesView(g_tapStartX, g_tapStartY))) {
                        TimelineFingerPanel(panel ? tlv : NULL);
                        g_scrollView = tlv; g_scrollBar = false;
                        const int pdx = mx - g_tapStartX, pdy = my - g_tapStartY;
                        tlv->event_finger_scroll(mx, my, pdx, pdy);
                        InertiaSamplesClear(); InertiaSample(e.motion.xrel, e.motion.yrel);
                        g_fingerScrolling = true; g_redraw = true; SaveMousePos(); return;
                    }
                }
                // el timeline mira SU ultima posicion de mouse: primero se le pasa la del toque (donde se apreto), despues el click
                if (g_tlTapView) { viewPortActive = g_tlTapView; g_tlTapView->event_mouse_motion(g_tapStartX, g_tapStartY); g_tlTapView->button_left(); g_tlTapView->event_mouse_motion(mx, my); }
                g_redraw = true; SaveMousePos(); return;
            }
            if (g_barTapPending || g_contentTapPending) {
                // (sin la espera anti-gesto del 3D y el timeline: los paneles y las barras no tienen gesto de 2 dedos, y
                //  esperar 180 ms antes de mover el contenido hacia que el scroll respondiera tarde y se sintiera trabado)
                int ddx = mx - g_tapStartX; if (ddx < 0) ddx = -ddx;
                int ddy = my - g_tapStartY; if (ddy < 0) ddy = -ddy;
                if (ddx + ddy < 8 * GlobalScale) { g_redraw = true; return; } // aun es tap
                InertiaSamplesClear();
                if (e.motion.which == SDL_TOUCH_MOUSEID) InertiaSample(e.motion.xrel, e.motion.yrel);
                if (g_barTapPending && e.motion.which == SDL_TOUCH_MOUSEID && g_barTapView && g_barTapView->isLeaf() &&
                    g_barTapView->ViewportKind() == 3 && BarEntersWhole(g_barTapView) &&
                    TabUnder(g_barTapView, g_tapStartX, g_tapStartY)) {   // PESTANIAS (la barra entra entera): ir mostrandolas
                    g_barTapPending = false;
                    g_tabSlide = (Properties*)g_barTapView;
                    TabSlideIn(mx);
                    g_fingerScrolling = true; g_redraw = true; SaveMousePos(); return;
                }
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
                        g_barTapView->PointInFieldNumeric(g_tapStartX, g_tapStartY) &&
                        g_barTapView->TouchSliderBuild(g_tapStartX, g_tapStartY)) {
                        g_slideNum = true;
                        g_barTapView->TouchSliderMove(e.motion.xrel);
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
                InertiaSamplesClear(); InertiaSample(e.motion.xrel, e.motion.yrel);
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
        // UN DEDO APOYADO NO CAMBIA DE VIEWPORT: orbitar, panear, scrollear, el pinch de 2 dedos o un transform siguen en
        // el viewport donde empezo el gesto aunque el dedo (o los dedos) salgan de el. Con el mouse el cursor se envuelve
        // en el borde; el dedo no se puede teletransportar. (Un toque nuevo elige su viewport al apoyarse)
        const bool fingerRested = (e.motion.which == SDL_TOUCH_MOUSEID) && (leftMouseDown || middleMouseDown || !fingers.empty());
        if (!ViewPortClickDown && !fingerRested){
            // NO perder el foco al cruzar un GAP/SEPARADOR: si el mouse virtual (touch) cae entre viewports
            // FindViewportUnderMouse devuelve NULL. Antes eso dejaba viewPortActive=NULL y, con ViewPortClickDown
            // trabado, el 'if (!viewPortActive) return' de mas abajo bloqueaba TODOS los clicks (no se podia
            // confirmar/cancelar ni tocar nada; solo el hover andaba). Mantenemos el ultimo viewport valido.
            ViewportBase* vpUnderCursor = FindViewportUnderMouse(rootViewport, mx, my);
            if (vpUnderCursor) viewPortActive = vpUnderCursor;
        }

        if (viewPortActive){
            viewPortActive->event_mouse_motion(mx, my);
        }

        // GIZMO: el punto agarrado sigue al puntero en ABSOLUTO (GizmoMotion): envolver el cursor lo haria saltar
        if (GizmoDragging() || Gizmo2DDragging()) { ViewPortClickDown = true; }
        else if ((leftMouseDown || middleMouseDown) && viewPortActive && !viewPortActive->NoWrapCursor()) {
            CheckWarpMouseInViewport(mx, my, viewPortActive);
        }
        // POSE Mode NO envuelve el cursor: el transform de huesos usa el delta REAL mx/my (PoseXformMotion). Si se
        // envolviera, el salto del cursor al borde daria un delta enorme y la pose pegaria un tiron.
        // Idem el transform de huesos del EDIT de armature (BoneGrabMotion3D usa mx/my reales).
        else if ((state == translation || state == rotation || state == EditScale) &&
                 InteractionMode != PoseMode && !BoneGrabActive()){
            ViewPortClickDown = true;
            CheckWarpMouseInViewport(mx, my, viewPortActive);
        }
        // TRANSFORM DE KEYFRAMES del timeline (g/s/r): mismo trato que el del 3D. Envolver el cursor deja seguir
        // escalando/rotando sin quedarse sin pantalla. Puede: usa un cursor VIRTUAL que acumula dx/dy (que el warp
        // pone en 0 justo en el salto), no la posicion real. ViewPortClickDown congela el foco en el timeline: sin
        // eso, al envolverse el cursor podria caer en otro viewport y el warp pasaria a usar EL RECT EQUIVOCADO.
        else if (DopeXformActive() && viewPortActive && viewPortActive->ViewportKind() == 5){
            ViewPortClickDown = true;
            CheckWarpMouseInViewport(mx, my, viewPortActive);
        }
        // TRANSFORM MODAL de un editor 2D (Editor 2D o UV editor; G/R/S sin boton apretado):
        // mismo envolver del cursor que el 3D, via el hook COMPARTIDO XformInProgress (TransformUI).
        // Cada editor acumula deltas y SALTEA el salto del warp -> la accion sigue fluida.
        else if (viewPortActive && viewPortActive->XformInProgress()){
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
        // JUEGO EN PLAY: con el mouse SOBRE el viewport (3D o 2D), el teclado es del
        // juego: el script lo captura y NO dispara atajos del editor (que no rote ni
        // haga nada raro). Fuera del viewport todo se comporta como siempre. La barra
        // espaciadora es DEL JUEGO (disparar, saltar...): no pausa. La pausa es ESC, que
        // sigue de largo: es la PAUSA global del juego (ver el bloque "ESC = PAUSA" en KEY_DOWN).
        // Y con el mouse FUERA de la ventana (o sobre ningun panel) el teclado SIGUE siendo del juego: reporte del
        // dueno, textual: "la camara se sale cuando saco el mouse de la ventana". Manejando con las flechas y el
        // mouse afuera, las teclas caian en el editor (las flechas navegaban la vista) y el juego no recibia el
        // SOLTAR de la flecha que tenia apretada (volante trabado). Ademas: la tecla que el juego recibio APRETADA
        // le llega SOLTADA siempre, este donde este el mouse (si no, quedaba trabada al pasar a otro panel).
        { extern bool SimActive(); extern void SimKeySDL(int, bool);
          if (SimActive() && PlayAnimation) {
            int smx, smy; SDL_GetMouseState(&smx, &smy);
            ViewportBase* vj = FindViewportUnderMouse(rootViewport, smx, smy);
            const int sym = (int)e.key.keysym.sym;
            const bool outside = (SDL_GetMouseFocus() == NULL) || vj == NULL;
            bool toGame = outside || (vj && (vj->ViewportKind() == 1 || vj->ViewportKind() == 6));
            if (!down && gKeysGame.count(sym)) toGame = true;
            if (toGame) {
                if (down) gKeysGame.insert(sym); else gKeysGame.erase(sym);
                SimKeySDL(sym, down);
                if (sym != SDLK_ESCAPE) return;
            }
          } }
    }

    // FOCO DE TECLADO POR HOVER (, convencion del editor): el destino de las
    // teclas es el viewport BAJO EL CURSOR, no el ultimo clickeado. Antes el foco solo se
    // actualizaba en el mouse-motion, y varios gestos (ej: el tap al boton Play de la barra
    // del Timeline) retornaban SIN pasar por mouse_button_up del viewport -> ViewPortClickDown
    // quedaba trabado en true y viewPortActive se CLAVABA en el Timeline: el teclado seguia
    // yendo ahi aunque movieras el mouse a otro viewport. Ahora el destino se resuelve AL
    // MOMENTO de la tecla. Solo con el foco LIBRE: un drag/transform/edicion en curso
    // (ViewPortClickDown) lo congela a proposito, y la caja de texto enfocada
    // (g_activeTextField) ya captura todo el teclado en su propio bloque de abajo.
    // (El gamepad no se rutea por viewport: en Play lo consume SIEMPRE el juego, SimJuego.)
    if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && !ViewPortClickDown) {
        int kmx, kmy; SDL_GetMouseState(&kmx, &kmy);
        ViewportBase* vpKey = FindViewportUnderMouse(rootViewport, kmx, kmy);
        if (vpKey) viewPortActive = vpKey; // el foco (borde verde) sigue al cursor, como la rueda
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
        if (LoopCutActive()) { LoopCutWheel(e.wheel.y > 0 ? 1 : -1); return; } // mas/menos cortes
        if (BevelActive()) { BevelWheel(e.wheel.y > 0 ? 1 : -1); return; }      // mas/menos segmentos
        // la rueda AFUERA de un popup que se resuelve solo (el panel de la ultima operacion, un borrado recien pedido):
        // seguir trabajando lo cierra y la rueda va al viewport (zoom) en vez de quedar trabada
        if (PopUpActive && !PopUpLeavesPass()) {
            int pmx, pmy; SDL_GetMouseState(&pmx, &pmy);
            if (!PopUpActive->Contains(pmx, pmy)) PopUpActive->ContinueWorking();
        }
        if (PopUpActive && !PopUpLeavesPass()) {
            PopUpActive->Wheel(e.wheel.y); // popup modal (file browser, etc)
        } else if (LayoutMenuOpen() && OpenMenu) {
            OpenMenu->Wheel(e.wheel.y > 0 ? 1 : -1); g_redraw = true; // menu desplegable largo (ej: 129 clips) -> scroll
        } else if (!LayoutMenuOpen()) {
            // la rueda va al viewport BAJO EL CURSOR, no al activo: si le di play (o toque el timeline) y muevo el
            // mouse a Properties, scrollear tiene que scrollear Properties, no zoomear el timeline. Fallback al
            // activo si el cursor cae en un gap/separador (FindViewportUnderMouse -> NULL).
            int wmx, wmy; SDL_GetMouseState(&wmx, &wmy);
            ViewportBase* vpWheel = FindViewportUnderMouse(rootViewport, wmx, wmy);
            if (vpWheel && !ViewPortClickDown) viewPortActive = vpWheel; // el foco (borde verde) tambien sigue al cursor
            { extern bool SimActive();   // JUGANDO: sin zoom en los visores (en pausa la inspeccion vuelve)
              ViewportBase* vw = vpWheel ? vpWheel : viewPortActive;
              if (SimActive() && PlayAnimation && vw &&
                  (vw->ViewportKind() == 1 || vw->ViewportKind() == 6)) return; }
            (vpWheel ? vpWheel : viewPortActive)->event_mouse_wheel((float)e.wheel.y, wmx, wmy);
        }
    }

    // Botones del mouse
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        // INSERTAR CARAS / BEVEL en curso. Con el MOUSE el click izq confirma (y abre el panel) y el der cancela, salvo
        // sobre la barra de herramientas (el tilde, la cruz y las opciones de la operacion). Con el DEDO tocar NO confirma:
        // la barra de estado abre el teclado numerico (el valor exacto), la de herramientas hace lo suyo y en el resto el
        // dedo AGARRA desde donde toca: arrastrarlo ajusta el tamano desde el valor que ya tenia. Un toque sobre el
        // teclado numerico abierto es del teclado.
        if ((InsetActive() || BevelActive()) && !(PopUpActive && PopUpActive->Contains((int)e.button.x, (int)e.button.y))) {
            const int bx = (int)e.button.x, by = (int)e.button.y;
            const bool inset = InsetActive();
            ViewportBase* vb = FindViewportUnderMouse(rootViewport, bx, by);
            Viewport3D* v3 = (vb && vb->ViewportKind() == 1) ? (Viewport3D*)vb : NULL;
            if (e.button.which == SDL_TOUCH_MOUSEID) {
                if (OpenMenu) LayoutClickUI(bx, by);
                else if (v3 && (v3->ClickBarTransform(bx, by) || v3->ToolbarClick(bx, by))) {}
                else if (inset) InsetGrab(bx, by);
                else            BevelGrab(bx, by);
            }
            else if (e.button.button == SDL_BUTTON_LEFT && v3 && v3->ToolbarClick(bx, by)) {}
            else if (e.button.button == SDL_BUTTON_LEFT)  { if (inset) InsetConfirm(); else BevelConfirm(); }
            else if (e.button.button == SDL_BUTTON_RIGHT) { if (inset) InsetCancel();  else BevelCancel(); }
            if (e.button.which == SDL_TOUCH_MOUSEID && PopUpActive) g_popupOpenedByMouseDown = true;
            SaveMousePos();
            return;
        }
        // loop cut en curso: izq aplica/confirma, der centra+confirma (o cancela en preview)
        if (LoopCutActive()) {
            bool popupPrev = (PopUpActive != NULL);
            if (e.button.button == SDL_BUTTON_LEFT)       LoopCutClickLeft((int)e.button.x, (int)e.button.y);
            else if (e.button.button == SDL_BUTTON_RIGHT) LoopCutClickRight();
            // si este toque (tactil) abrio el panel redo del loop cut, avisar al FINGERDOWN del mismo
            // toque para que NO lo cierre (caeria afuera del panel recien abierto). Igual que el menu Delete.
            if (e.button.which == SDL_TOUCH_MOUSEID && !popupPrev && PopUpActive) g_popupOpenedByMouseDown = true;
            SaveMousePos();
            return;
        }
        // loop select (modal de direccion desde una cara): cualquier click acepta
        if (LoopSelOrienting()) { LoopSelConfirm(); SaveMousePos(); return; }
        // pick shortest path guiado: click DERECHO cancela (el izquierdo lo maneja el pick)
        if (PickPathGuided() && e.button.button == SDL_BUTTON_RIGHT) { LayoutPickPathCancel(); SaveMousePos(); return; }
        // Select Linked / Loop Select guiados (1 click): click DERECHO cancela (el izquierdo lo maneja el pick)
        if (GuidedUnClickActive() && e.button.button == SDL_BUTTON_RIGHT) { LayoutGuidedCancel(); SaveMousePos(); return; }
        // LA CAJA DE TEXTO ENFOCADA: el mouse (o el dedo) pone el cursor ahi, con Shift extiende la seleccion, arrastrar
        // selecciona y el doble click selecciona todo. El click DERECHO abre el menu del texto (Cortar, Copiar, Pegar,
        // Seleccionar todo), tambien sobre el codigo del IDE. Con un menu abierto el click es del menu
        if (!LayoutMenuOpen()) {
            if (e.button.button == SDL_BUTTON_LEFT && g_activeTextField &&
                TextFieldMouseDown((int)e.button.x, (int)e.button.y, (SDL_GetModState() & KMOD_SHIFT) != 0, e.button.clicks >= 2)) {
                g_redraw = true; SaveMousePos(); return;
            }
            if (e.button.button == SDL_BUTTON_RIGHT && MenuTextClickRight((int)e.button.x, (int)e.button.y)) {
                g_redraw = true; SaveMousePos(); return;
            }
        }
        // rename de mesh part / material en curso: click IZQ ACEPTA, click DER CANCELA. Consume el click.
        // (no si el click es del menu del texto: Pegar pega en el nombre)
        if (RenameActive() && !MenuTextOpen()) {
            if (e.button.button == SDL_BUTTON_LEFT)       RenameCommit();
            else if (e.button.button == SDL_BUTTON_RIGHT) RenameCancel();
            SaveMousePos();
            return;
        }
        // click DENTRO de un popup modal (teclado numerico, color picker): NO commitea la edicion ni
        // desenfoca el campo (las teclas del popup EDITAN ese campo; sino el 1er tap al teclado lo cerraba)
        bool clickInPopup = PopUpActive && PopUpActive->Contains((int)e.button.x, (int)e.button.y);
        // edicion numerica por texto: un click en cualquier lado APLICA lo tipeado (no consume el click -> si fue
        // sobre otro campo, ese arranca su propia edicion en el mouse-up).
        // (adentro de un popup que NO es el teclado -otro campo, un boton- tambien se aplica: lo tipeado no se pierde)
        if (NumEditActive() && !MenuTextOpen() && !(clickInPopup && PopUpActive->IsKeyboard())) { NumEditCommit(); NumEditExitFromPanel(); }
        // MODO JUEGO: el click IZQUIERDO sobre el visor (2D o 3D) es del JUEGO
        { extern bool SimActive();
          // SOLO durante PLAY: el click sobre el visor es del JUEGO. En PAUSA el
          // editor vuelve (inspeccion: zoom/pan/seleccion) y los MENUS abiertos
          // siempre tienen prioridad (el click es del menu, no del juego).
          if (SimActive() && PlayAnimation && e.button.button == SDL_BUTTON_LEFT &&
              !LayoutMenuOpen() && !PopUpActive) {
              ViewportBase* vj = FindViewportUnderMouse(rootViewport, (int)e.button.x, (int)e.button.y);
              if (vj && (vj->ViewportKind() == 1 || vj->ViewportKind() == 6) &&
                  !vj->OnBar((int)e.button.x, (int)e.button.y)) {   // la BARRA sigue siendo del editor
                  extern void SimTouchScreen(ViewportBase*, int, int, bool);
                  g_touchGameVp = vj;
                  SimTouchScreen(vj, (int)e.button.x, (int)e.button.y, true);
                  SaveMousePos();
                  return;
              }
          } }
        ViewPortClickDown = true;
        if (e.button.button == SDL_BUTTON_LEFT) {
            leftMouseDown = true;
            bool popupPrev = (PopUpActive != NULL); // para detectar si ESTE down abre un popup (menu->action)
            if (!clickInPopup)
                g_activeTextField = NULL; // click en cualquier lado desenfoca el texto
                                          // (ClickIn de Properties re-enfoca en el up si es una caja)
            // TOUCH se detecta por el which del evento (SDL_TOUCH_MOUSEID), NO por el mapa de dedos: SDL
            // sintetiza este mouse-down ANTES de pushear el SDL_FINGERDOWN -> en este punto 'fingers'
            // todavia esta VACIO y chequearlo hacia caer el touch al flujo de mouse (clickeaba al apoyar).
            bool isTouch = (e.button.which == SDL_TOUCH_MOUSEID);
            g_lastDownTouch = isTouch;   // el arrastre que sigue es de un dedo (el 3D orbita con 1 dedo)
            // gesto NUEVO: limpiar lo que quedo del anterior (scroll lockeado, tap a medio decidir, slider)
            g_fingerScrolling = false; g_scrollView = NULL; g_slideNum = false; g_tabSlide = NULL;
            // (y los toques pendientes del timeline / la 3D: si el mouse-up que SDL sintetiza del dedo se perdio -pasa
            //  con toques rapidos-, quedaban prendidos con la hora del toque VIEJO y el proximo evento abria el menu de
            //  la pulsacion larga en el acto. Los vuelve a armar su rama de abajo, con la hora de ESTE toque)
            g_tlTapPending = false; g_view3dTapPending = false; g_tapMoved = false;
            TimelineFingerPanel(NULL);
            g_inBrakeTouch = false;
            g_barTapPending = false; g_contentTapPending = false;
            // un toque sobre un panel que sigue DESLIZANDO solo (inercia) lo frena en seco; si se suelta sin arrastrar,
            // no elige nada de lo que pasaba por abajo (como en el telefono)
            if (g_inVp) {
                ViewportBase* vpBrake = FindViewportUnderMouse(rootViewport, (int)e.button.x, (int)e.button.y);
                if (isTouch && vpBrake == g_inVp) g_inBrakeTouch = true;
                InertiaStop();
            }
            InertiaSamplesClear();
            // MENU DESPLEGABLE abierto + DEDO: apoyar no elige; la opcion de abajo se resalta y soltar la elige
            if (isTouch && !clickInPopup && LayoutMenuTouchStart((int)e.button.x, (int)e.button.y, false)) {
                SaveMousePos();
                return;
            }
            // si habia un transform activo, este click lo ACEPTA y NO
            // debe ademas cambiar la seleccion (como en Symbian)
            extern int g_poseMode; // Pose Mode: transform de huesos (G/R/S) en curso -> el click lo CONFIRMA igual
            bool hadTransform = (state == translation ||
                                   state == rotation ||
                                   state == EditScale ||
                                   g_poseMode != 0);
            // viewport bajo el click (lo usa el diferido de barra/contenido, abajo)
            ViewportBase* vpDown = FindViewportUnderMouse(rootViewport, (int)e.button.x, (int)e.button.y);
            // ESQUINA: down sobre el boton de menu [0] del viewport -> candidato a redimension por arrastre
            // (el tap sigue abriendo el menu via el diferido de barra). Vale mouse y touch. NO si hay un
            // popup/menu modal abierto (p.ej. el explorador de archivos): esos son duenos del input y el
            // corner-drag secuestraria/bloquearia sus botones y su scroll.
            g_cornerVp = NULL; g_cornerResizing = false;
            if (!clickInPopup && !PopUpActive && !LayoutMenuOpen() &&
                vpDown && vpDown->isLeaf() && !vpDown->BarButtons.empty() &&
                vpDown->BarButtons[0]->Contains((int)e.button.x, (int)e.button.y)) {
                g_cornerVp = vpDown;
                g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y;
            }
            // touch NO genera hover antes del down -> viewPortActive podia quedar stale y el motion/up
            // (slide del campo numerico, mouse_button_up del panel) iban al viewport equivocado.
            if (isTouch && vpDown) viewPortActive = vpDown;
            // click AFUERA del panel de la ultima operacion (semi-modal): lo confirma y el click es del viewport
            // que esta abajo del cursor, aunque no haya llegado un motion antes
            if (vpDown && PopUpActive && PopUpActive->ClosesWithViewport() &&
                !PopUpActive->Contains((int)e.button.x, (int)e.button.y)) viewPortActive = vpDown;
            // la 'x' de una notificacion de error la cierra (esta encima de todo)
            if (NotificationsClick((int)e.button.x, (int)e.button.y)) {
                // consumido: no propagar el click
            }
            else if (hadTransform) {
                if (isTouch) {
                    // TACTIL: tocar NO confirma el transform (la pantalla se usa para MOVER; sin ella no
                    // podes ni arrastrar). Confirmar/cancelar = tilde/cruz de la barra de HERRAMIENTAS.
                    if (OpenMenu) {
                        // menu abierto desde la barra (orientacion): el tap elige la opcion / lo cierra
                        LayoutClickUI((int)e.button.x, (int)e.button.y);
                    }
                    // tap en la barra de ESTADO del transform ("Move: ... = ..."): abre el teclado
                    // numerico para editar el valor exacto (como tipear en PC). Si no cayo ahi, la
                    // barra de HERRAMIENTAS (tilde/cruz/ejes) maneja el tap.
                    else if (vpDown && vpDown->ViewportKind() == 1) {
                        Viewport3D* v3 = (Viewport3D*)vpDown;
                        if (!v3->ClickBarTransform((int)e.button.x, (int)e.button.y) &&
                            !v3->BrushBarClick((int)e.button.x, (int)e.button.y))   // barra de influencia (proportional)
                            v3->ToolbarClick((int)e.button.x, (int)e.button.y);
                    }
                    // fuera de la barra: nada en el down; el arrastre del dedo mueve el transform
                }
                // durante un transform (mover/rotar/escalar, o ubicar un duplicado) el CLICK de mouse
                // CONFIRMA, este donde este (incluso sobre la barra/menu): la UI no lo consume
                else if (g_poseMode){ extern void PoseXformConfirm(); PoseXformConfirm(); }
                // ...salvo sobre la BARRA DE INFLUENCIA del proportional editing: ahi el click arma el arrastre del radio
                else if (vpDown && vpDown->ViewportKind() == 1 && vpDown->BrushBarClick((int)e.button.x, (int)e.button.y)) {}
                else if (Viewport3DActive) Viewport3DActive->Accept();
            }
            // TRANSFORM MODAL del UV editor / Editor 2D en curso (no usa el 'estado' global):
            // mismo trato que el 3D. MOUSE: el click CONFIRMA este donde este (con el gate de
            // la expresion invalida). TACTIL: tocar NO confirma (la pantalla mueve el transform);
            // el tap en la barra de INFO abre el teclado numerico (TransformUIClickBar) y el
            // tap en la toolbar va al tilde/cruz/ejes. Con un popup abajo (el teclado numerico
            // mismo) el click es del popup (clickInPopup, mas abajo).
            else if (!clickInPopup &&
                     ((viewPortActive && viewPortActive->XformInProgress()) ||
                      (vpDown && vpDown->XformInProgress()))) {
                ViewportBase* vpX = (viewPortActive && viewPortActive->XformInProgress())
                                    ? viewPortActive : vpDown;
                if (isTouch) {
                    if (OpenMenu) LayoutClickUI((int)e.button.x, (int)e.button.y);
                    else if (!TransformUIClickBar(vpX, (int)e.button.x, (int)e.button.y))
                        vpX->ToolbarClick((int)e.button.x, (int)e.button.y);
                    // fuera de las barras: nada en el down; el arrastre del dedo mueve el transform
                } else {
                    vpX->XformConfirm();
                }
            }
            // POPUP MODAL (teclado numerico, color picker) bajo el click: va directo, sin diferir -> las
            // teclas responden al DOWN (y el drag sobre el popup no scrollea el panel de atras).
            else if (clickInPopup) {
                LayoutClickUI((int)e.button.x, (int)e.button.y);
            }
            // BARRA superior (MOUSE o touch): NO abrir la pestaña/menu en el DOWN. Si se ARRASTRA = scroll
            // horizontal; si se SUELTA sin mover = abre en el mouse-up. Vale para mouse PORQUE en la barra del
            // viewport 3D la rueda hace ZOOM -> en PC la unica forma de scrollear esa barra es arrastrandola.
            else if (!LayoutMenuOpen() && vpDown && vpDown->OnBar((int)e.button.x, (int)e.button.y)) {
                g_barTapPending = true;
                g_barTapView = vpDown; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y; g_tapStartTicks = SDL_GetTicks();
            }
            // CONTENIDO de un panel (properties=3 / outliner=2 / console=7), SOLO en TOUCH: no ejecutar el click
            // en el down (sino togglea/selecciona apenas apoyas). En PC el mouse usa scrollbar/rueda + el drag
            // numerico clasico (gFloatDrag), asi que ahi NO se difiere. Se decide en el 1er motion (dir) o al
            // soltar (tap). (con un desplegable abierto NO se difiere: el click es del MENU, va a LayoutClickUI.)
            // IDE TACTIL: una AGARRADERA de la seleccion se agarra al toque (ajusta esa punta de la seleccion)
            else if (isTouch && !LayoutMenuOpen() && vpDown && vpDown->isLeaf() && vpDown->ViewportKind() == 8 &&
                     ((IDE*)vpDown)->HandleGrab((int)e.button.x, (int)e.button.y)) {
                g_ideGestureVp = (IDE*)vpDown;
                viewPortActive = vpDown;
                ViewPortClickDown = true;
                g_redraw = true;
            }
            else if (isTouch && !LayoutMenuOpen() && vpDown &&
                     (VpIsOutliner(vpDown) || vpDown->ViewportKind() == 3 ||
                      vpDown->ViewportKind() == 7 || vpDown->ViewportKind() == 8)) {
                g_contentTapPending = true;
                g_contentTapTouch = true;
                g_barTapView = vpDown; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y; g_tapStartTicks = SDL_GetTicks();
            }
            // BOX SELECT armado (tecla B / menu Select): el click NO pickea, empieza la caja.
            // TIMELINE en tactil: el toque tambien espera (tap = poner el frame ahi; arrastre = scrub; 2 dedos = zoom)
            else if (isTouch && !LayoutMenuOpen() && vpDown && vpDown->ViewportKind() == 5) {
                g_tlTapPending = true;
                g_tlTapView = vpDown; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y; g_tapStartTicks = SDL_GetTicks();
            }
            else if (BoxSelectBuilt()) {
                BoxSelectDown((int)e.button.x, (int)e.button.y);
            }
            // la UI compartida (menu/barras/paneles) consume primero. Un DOBLE CLICK del mouse (el clicks == 2 de
            // SDL) ademas renombra en linea la fila del outliner que esta bajo el puntero
            else if (LayoutClickUI((int)e.button.x, (int)e.button.y)) {
                if (!isTouch && e.button.clicks >= 2) LayoutDoubleClickUI((int)e.button.x, (int)e.button.y);
            }
            else {
                // click sobre un viewport 3D: seleccion compartida por color picking
                ViewportBase* leaf3d = FindViewportUnderMouse(
                    rootViewport, (int)e.button.x, (int)e.button.y);
                bool es3Dnav = leaf3d && leaf3d->isLeaf() &&
                               leaf3d->ViewportKind() == 1 && state == editNavigation;
                // en los modos de PINTURA el down es el arranque del trazo y la seleccion no cambia:
                // nada de diferir el pick ni de armar box select por arrastre (era el bug de "pinto y
                // me arma la caja"). El trazo arranca en el down, en PC y en tactil por igual.
                // ...salvo en TACTIL con el toggle "vista" (el monitor de la barra) prendido: ahi el dedo NO pinta,
                // navega como en Object Mode (arrastrar = orbitar, dos dedos = zoom/paneo).
                { const bool paintMode = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
                  if (paintMode && !(isTouch && g_viewEditMode)) es3Dnav = false; }
                // TACTIL sobre el viewport 3D: NO seleccionar en el DOWN. Si el dedo se arrastra = orbita/
                // panea (la seleccion NO cambia); si es un TAP simple, se pickea al SOLTAR. Antes cualquier
                // orbit/paneo pisaba la seleccion (se deseleccionaba al tocar el fondo para orbitar).
                // GIZMO: un click/tap sobre una manija arranca la traslacion ahi mismo (mouse o dedo por igual);
                // el up del viewport la confirma. Nada de diferir el pick ni de armar box select.
                // GIZMO con MOUSE: agarra al instante (el up del viewport confirma). Con DEDO va por el toque
                // pendiente de abajo: se decide recien pasada la espera anti-gesto (un 2do dedo = pinch, no gizmo).
                // NAVEGACION CON ALT (mouse): ni gizmo, ni trazo de pintura, ni caja de seleccion. Si se suelta sin
                // arrastrar es el click de siempre (pick al soltar; en pintura no hace nada).
                const bool altNav = !isTouch && (SDL_GetModState() & KMOD_ALT) != 0 && leaf3d && leaf3d->isLeaf() &&
                                    leaf3d->ViewportKind() == 1 && state == editNavigation;
                g_altNavPend = false; g_altNavActive = false;
                if (altNav) {
                    const bool paintAlt = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
                    g_altNavPend = true;
                    viewPortActive = leaf3d;
                    g_barTapView = leaf3d; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y; g_tapStartTicks = SDL_GetTicks();
                    if (!paintAlt) { viewPortActive->button_left(); g_view3dTapPending = true; }
                }
                // el CANDADO de la vista de camara (abajo al centro): abre / cierra, mouse o dedo
                else if (es3Dnav && ((Viewport3D*)leaf3d)->LockCameraClick((int)e.button.x, (int)e.button.y)) {}
                else if (es3Dnav && !isTouch && GizmoDown(leaf3d, (int)e.button.x, (int)e.button.y)) {
                    ViewPortClickDown = true;
                }
                else if (isTouch && es3Dnav) {
                    g_view3dTapPending = true; g_view3dTapTouch = true;
                    g_barTapView = leaf3d; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y; g_tapStartTicks = SDL_GetTicks();
                } else {
                    g_lastDownTouch = isTouch;   // el viewport decide si ESPERA antes de pintar
                    viewPortActive->button_left();
                    // PC sobre el viewport 3D: el pick tambien se DIFIERE al soltar, con el mismo
                    // mecanismo que el tactil. Es lo que permite distinguir CLICK (pickea) de
                    // ARRASTRE (box select): si se pickeara en el down, arrastrar deseleccionaba
                    // todo antes de empezar la caja -- que es justo lo que pasaba.
                    if (es3Dnav) {
                        g_view3dTapPending = true; g_view3dTapTouch = false;
                        g_barTapView = leaf3d; g_tapStartX = (int)e.button.x; g_tapStartY = (int)e.button.y; g_tapStartTicks = SDL_GetTicks();
                    }
                }
            }
            // si ESTE mouse-down tactil abrio un popup (p.ej. menu Delete -> ConfirmPopup), avisar al
            // FINGERDOWN del mismo toque para que NO lo cierre (caeria afuera del popup recien abierto).
            if (isTouch && !popupPrev && PopUpActive) g_popupOpenedByMouseDown = true;
            SaveMousePos();
        }
        else if (e.button.button == SDL_BUTTON_MIDDLE) {
            middleMouseDown = true;
            // orbitar con el mouse AFUERA cierra el redo-panel (semi-modal);
            // al quedar PopUpActive=NULL, las motions de la orbita ya pasan
            if (PopUpActive && !PopUpActive->Contains((int)e.button.x, (int)e.button.y))
                PopUpActive->ContinueWorking();   // (un panel de la ultima operacion se cierra; un borrado se confirma)
            SaveMousePos();
        }
        else if (e.button.button == SDL_BUTTON_RIGHT) {
            // el derecho CANCELA la caja de seleccion, como cancela cualquier operacion modal
            if (BoxSelectActive()) { extern void TimelineBoxForget(); TimelineBoxForget(); BoxSelectCancel(); SaveMousePos(); return; }
            // transform de KEYFRAMES (timeline): mismo trato que el del 3D -> derecho CANCELA. No usa 'estado'
            // (ese es el del viewport 3D), asi que necesita su propio chequeo.
            if (DopeXformActive()){ DopeXformCancel(); SaveMousePos(); return; }
            // GRAB de HUESOS (Edit de armature, Fase 3): derecho CANCELA (restaura head/tail)
            { extern bool BoneGrabActive(); extern void BoneGrabCancel();
              if (BoneGrabActive()){ BoneGrabCancel(); SaveMousePos(); return; } }
            if (state == translation || state == EditScale || state == rotation){
                // Edit Mode con transform de malla en curso: descarta el snapshot
                if (InteractionMode == EditMode && EditXformActive()) EditXformCancel();
                else Cancel();
                NumInputReset();
            }
            // TRANSFORM MODAL del UV editor / Editor 2D (G/R/S de UVs y de huesos 2D): el
            // derecho CANCELA (paridad con el 3D). SOLO aplica con transform en curso: sin
            // transform el derecho no se rutea al viewport (hoy en PC no tiene otros usos).
            // Con un popup abajo (teclado numerico) el click es del popup, como el izquierdo.
            // Mismo par activo/bajo-el-mouse que el confirmar del click izquierdo (arriba).
            else if (!clickInPopup) {
                ViewportBase* vpRight = FindViewportUnderMouse(rootViewport, (int)e.button.x, (int)e.button.y);
                ViewportBase* vpX = (viewPortActive && viewPortActive->XformInProgress()) ? viewPortActive
                                  : (vpRight && vpRight->XformInProgress()) ? vpRight : NULL;
                if (vpX) { vpX->XformCancel(); g_redraw = true; SaveMousePos(); return; }
                // SIN transform en curso: el derecho VA AL VIEWPORT (button_right). Hasta ahora no
                // se ruteaba nunca y UVEditor::button_right ("el derecho coloca el CURSOR 2D en el
                // mouse") era CODIGO MUERTO: el pivote "3D Cursor" del UV y medio menu Cursor eran
                // decorativos porque el cursor solo se podia mover desde el propio menu.
                // Se rutea SOLO al editor UV (ViewportKind 4) a proposito: en el 3D el derecho
                // todavia no tiene semantica de "colocar cursor" y los otros viewports usan
                // button_right para otra cosa (navegacion del panel Properties / del redo).
                if (vpRight && vpRight->isLeaf() && vpRight->ViewportKind() == 4 &&
                    !vpRight->OnBar((int)e.button.x, (int)e.button.y) &&
                    !vpRight->OnToolbar((int)e.button.x, (int)e.button.y)) {
                    // el editor pone el cursor 2D en SU lastMx/lastMy (que normalmente actualiza
                    // el motion): se sincroniza con este click para que no dependa de que haya
                    // habido un movimiento antes.
                    UVEditor* uvRight = (UVEditor*)vpRight;
                    uvRight->lastMx = (int)e.button.x; uvRight->lastMy = (int)e.button.y;
                    uvRight->button_right(); g_redraw = true; SaveMousePos(); return;
                }
                // VIEWPORT 3D: el derecho abre el MENU CONTEXTUAL en el mouse (Object / el de la
                // malla en Edit / Pose). Solo sobre el contenido y sin transform en curso (con
                // transform el derecho ya cancelo, mas arriba).
                if (vpRight && vpRight->isLeaf() && vpRight->ViewportKind() == 1 &&
                    !vpRight->OnBar((int)e.button.x, (int)e.button.y) &&
                    !vpRight->OnToolbar((int)e.button.x, (int)e.button.y) &&
                    !vpRight->OnBrushBar((int)e.button.x, (int)e.button.y)) {
                    LayoutMenuContext3D((int)e.button.x, (int)e.button.y);
                    SaveMousePos(); return;
                }
                // TIMELINE: el derecho sobre el strip abre el menu de los keyframes (cortar/copiar/pegar/duplicar)
                if (vpRight && vpRight->isLeaf() && vpRight->ViewportKind() == 5 &&
                    !vpRight->OnBar((int)e.button.x, (int)e.button.y) &&
                    ((Timeline*)vpRight)->MenuContext((int)e.button.x, (int)e.button.y)) {
                    SaveMousePos(); return;
                }
                // PROPERTIES: el derecho sobre una fila de valor abre su menu (restablecer, copiar, pegar y, si es
                // animable, los keyframes)
                if (vpRight && vpRight->isLeaf() && vpRight->ViewportKind() == 3 &&
                    ((Properties*)vpRight)->MenuContext((int)e.button.x, (int)e.button.y)) {
                    g_redraw = true; SaveMousePos(); return;
                }
                // OUTLINER en una vista de RECURSOS: el derecho elige la fila y abre el menu de
                // acciones (carpetas, renombrar, mover, borrar huerfanos) en el mouse
                if (vpRight && vpRight->isLeaf() && VpIsOutliner(vpRight) &&
                    !vpRight->OnBar((int)e.button.x, (int)e.button.y) &&
                    ((Outliner*)vpRight)->MenuContext((int)e.button.x, (int)e.button.y)) {
                    g_redraw = true; SaveMousePos(); return;
                }
            }
        }
    }
    else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (e.button.button == SDL_BUTTON_LEFT) TextFieldMouseUp();   // (termina el arrastre que selecciona texto)
        // el dedo sobre un MENU DESPLEGABLE: soltar elige la opcion de abajo (afuera: cierra sin elegir)
        if (LayoutMenuTouchActive()) {
            LayoutMenuTouchRelease((int)e.button.x, (int)e.button.y);
            leftMouseDown = false; ViewPortClickDown = false;
            g_barTapPending = g_contentTapPending = false;
            SaveMousePos();
            return;
        }
        // el dedo que abrio el menu contextual con la pulsacion larga: se levanta sin elegir nada (la opcion se toca)
        if (g_view3dMenuLength) { g_view3dMenuLength = false; ViewPortClickDown = false; SaveMousePos(); return; }
        if (g_ideGestureVp) {     // IDE: se solto la lupa / la agarradera (el cursor y la seleccion quedan ahi)
            // (la LUPA soltada sin mover el dedo: el menu del texto, como en los telefonos)
            IDE* ide = g_ideGestureVp;
            const bool menu = ide->touchGesture == 1 && !g_tapMoved;
            ide->TouchGestureEnd();
            g_ideGestureVp = NULL;
            leftMouseDown = false;
            ViewPortClickDown = false;
            g_contentTapPending = false;
            if (menu) MenuTextIDE(ide, (int)e.button.x, (int)e.button.y);
            SaveMousePos();
            return;
        }
        if (g_touchGameVp) {   // fin del arrastre del juego
            extern void SimTouchScreen(ViewportBase*, int, int, bool);
            SimTouchScreen(g_touchGameVp, (int)e.button.x, (int)e.button.y, false);
            g_touchGameVp = NULL;
            leftMouseDown = false;
            ViewPortClickDown = false; // este up no pasa por mouse_button_up: liberar el foco aca
            SaveMousePos();
            return;
        }
        if (e.button.button == SDL_BUTTON_LEFT && g_outGrab) {
            // se levanto el dedo con una fila AGARRADA: se suelta ahi (a una carpeta, al 3D, a Properties; sin
            // haberla movido = el menu contextual)
            g_outGrab = false;
            g_contentTapPending = false; g_contentTapTouch = false;
            leftMouseDown = false;
            ViewPortClickDown = false;
            if (g_barTapView && g_barTapView->isLeaf() && g_barTapView->ViewportKind() == 3) {
                // (una fila de una lista de Propiedades: cae donde marca la linea; sin moverla, sus opciones)
                int cx, cy; LoadFingerPlace((int)e.button.x, (int)e.button.y, cx, cy);
                ((Properties*)g_barTapView)->ListGrabRelease((int)e.button.x, (int)e.button.y, cx, cy);
            } else
            LayoutRelease((int)e.button.x, (int)e.button.y);
            SaveMousePos();
            g_redraw = true;
            return;
        }
        if (e.button.button == SDL_BUTTON_LEFT) {
            leftMouseDown = false;
            g_altNavPend = false; g_altNavActive = false;   // fin de la navegacion con Alt (si fue click, pickea abajo)
            // BOX SELECT: soltar cierra la caja y selecciona. Shift SUMA a lo que ya estaba
            // (misma regla que el pick de siempre). Consume el up: no hay pick ni orbita.
            if (BoxSelectDragging()) {
                int bx0, by0, bx1, by1; bool touch = false;
                extern bool TimelineBoxOwn(); extern void TimelineBoxRelease(bool, int, int, int, int, bool, bool);
                const bool ofTimeline = TimelineBoxOwn();
                const bool had = BoxSelectRelease(bx0, by0, bx1, by1, touch);
                if (ofTimeline) TimelineBoxRelease(had, bx0, by0, bx1, by1, touch, LShiftPressed);   // (la armo el timeline)
                else if (had)
                    BoxSelectApply3D(bx0, by0, bx1, by1, touch, LShiftPressed);
                ViewPortClickDown = false;
                SaveMousePos();
                return;
            }
            g_cornerVp = NULL; g_cornerResizing = false; // fin del gesto de esquina
            // DRAG-SCROLL de menu largo: el soltar resuelve (drag=solo scrolleo; tap=selecciona el item). Consume el up.
            // OJO: consume el up SIN pasar por mouse_button_up del viewport (quien normalmente apaga
            // ViewPortClickDown) -> hay que liberarlo aca, sino viewPortActive queda CLAVADO (punto 1).
            { extern bool LayoutMenuDragRelease(int, int); if (LayoutMenuDragRelease((int)e.button.x, (int)e.button.y)) { ViewPortClickDown = false; SaveMousePos(); return; } }
            // suelta scroll agarrado y dropea el drag del outliner
            LayoutRelease((int)e.button.x, (int)e.button.y);
        }
        else if (e.button.button == SDL_BUTTON_MIDDLE) {
            middleMouseDown = false;
        }
        SaveMousePos();

        // fin del gesto tactil: soltar el lock del scroll. Si venia SCROLLEANDO, el soltar NO es un click de
        // seleccion. Si fue un TAP sobre una barra (toco sin arrastrar), RECIEN AHORA abrimos la pestaña/menu
        // (lo diferimos del down para no abrir menus al scrollear).
        bool wasBarTap = g_barTapPending;      // tap sobre una BARRA (mouse o touch) -> abre menu/pestaña
        bool wasContentTap = g_contentTapPending; // tap sobre CONTENIDO de panel (touch) -> togglea/selecciona/edita
        g_barTapPending = false;
        g_contentTapPending = false;
        // (la pestania que quedo abajo del dedo ya esta activa). El dedo NO tiene hover: al soltar no queda
        // ninguna pestania/boton de la barra iluminado (antes quedaba la del toque inicial y la elegida)
        if (g_tabSlide) g_tabSlide->BarHover(-99999, -99999);
        g_tabSlide = NULL;
        // INERCIA: si venia scrolleando con el dedo y lo solto con envion, el contenido sigue deslizando solo
        bool inertia = false;
        if (g_fingerScrolling && g_scrollView && e.button.which == SDL_TOUCH_MOUSEID && !g_slideNum && !g_cornerResizing)
            inertia = InertiaStart(g_scrollView, g_scrollBar, (int)e.button.x, (int)e.button.y);
        g_scrollView = NULL;
        if (!inertia) PropertiesTouchScrollEnd(); // fin del gesto: libera el latch del scroll tactil de listas
                                                  // (con inercia sigue scrolleando la misma lista: lo suelta InertiaStop)
        // el toque que FRENO una inercia: si se solto sin arrastrar, no es un tap (no elige lo que pasaba por abajo)
        if (g_inBrakeTouch) {
            g_inBrakeTouch = false;
            if (wasBarTap || wasContentTap) { ViewPortClickDown = false; g_contentTapTouch = false; SaveMousePos(); return; }
        }
        // slider numerico tactil: el arrastre YA edito el valor; soltar NO es un click (no abre el editor)
        // (estos caminos consumen el up SIN pasar por mouse_button_up del viewport, que es quien
        //  normalmente apaga ViewPortClickDown: se libera aca, sino el foco de teclado por hover
        //  quedaba CLAVADO en el viewport del down -- el bug del Play del Timeline, punto 1)
        if (g_slideNum) { if (g_barTapView) g_barTapView->TouchSliderRelease(); g_slideNum = false; g_fingerScrolling = false; ViewPortClickDown = false; SaveMousePos(); return; }
        if (g_fingerScrolling) { g_fingerScrolling = false; ViewPortClickDown = false; return; }
        // TAP simple sobre el viewport 3D (tactil): RECIEN AHORA se pickea la seleccion. Si hubo orbit/paneo,
        // g_view3dTapPending ya se limpio en el motion -> la seleccion NO cambia.
        if (g_tlTapPending) {   // TAP en el timeline (sin arrastre): recien ahora se manda el click (poner el frame ahi)
            g_tlTapPending = false; ViewPortClickDown = false;
            if (g_tlTapView) { viewPortActive = g_tlTapView; g_tlTapView->event_mouse_motion(g_tapStartX, g_tapStartY); g_tlTapView->button_left(); g_tlTapView->mouse_button_up(W3dButtonFromSDL(e.button.button)); }
            SaveMousePos(); return;
        }
        if (g_view3dTapPending) {
            g_view3dTapPending = false;
            ViewPortClickDown = false;
            ViewportBase* h = g_barTapView;
            const bool paintTap = (InteractionMode == WeightPaint || InteractionMode == VertexPaint || InteractionMode == TexturePaint);
            // TAP (dedo) sobre el GIZMO: pasa al modo siguiente (mover -> rotar -> escalar) en vez de seleccionar lo de abajo
            if (e.button.which == SDL_TOUCH_MOUSEID && h && !paintTap && GizmoTouch(h, (int)e.button.x, (int)e.button.y)) {
                SaveMousePos();
                return;
            }
            if (h && h->isLeaf() && h->ViewportKind() == 1 && state == editNavigation && !paintTap) {   // pintando (vista ON) un tap no pickea ni pinta
                if (viewPortActive) viewPortActive->button_left(); // SaveMousePos + cursor 3D en el punto tocado
                ScenePick3D((int)e.button.x, (int)e.button.y, h->x, h->y, h->width, h->height, W3dScreenHeight);
            }
            SaveMousePos();
            return;
        }
        // tap sin arrastrar sobre la UI: RECIEN AHORA ejecutamos el click (abre menu/pestaña, togglea checkbox,
        // selecciona, o abre el editor de un campo numerico/texto). g_uiTapInProgress solo para el CONTENIDO tactil
        // (ahi el campo numerico abre el editor inline); en la barra es un click normal.
        if (wasBarTap || wasContentTap) {
            // liberar el foco ANTES del click diferido: este camino tampoco pasa por mouse_button_up.
            // Si el click abre una edicion (ClickIn de Properties), ESE camino lo vuelve a prender.
            ViewPortClickDown = false;
            g_uiTapInProgress = wasContentTap;
            LayoutClickUI((int)e.button.x, (int)e.button.y);
            // DOBLE TAP: un segundo toque de contenido, rapido y en el mismo lugar, es el doble click
            if (wasContentTap && g_contentTapTouch) {
                const Uint32 now = SDL_GetTicks();
                int ddx = (int)e.button.x - g_lastTapX; if (ddx < 0) ddx = -ddx;
                int ddy = (int)e.button.y - g_lastTapY; if (ddy < 0) ddy = -ddy;
                if (now - g_lastTapTicks < 400 && ddx + ddy < 12 * GlobalScale) {
                    LayoutDoubleClickUI((int)e.button.x, (int)e.button.y);
                    g_lastTapTicks = 0;
                } else { g_lastTapTicks = now; g_lastTapX = (int)e.button.x; g_lastTapY = (int)e.button.y; }
            }
            g_uiTapInProgress = false;
            g_contentTapTouch = false;
            return;
        }

        if (viewPortActive){
            viewPortActive->mouse_button_up(W3dButtonFromSDL(e.button.button));
        }

        // idem: no dejar viewPortActive en NULL si el up cayo en un gap/separador (se trababa todo)
        ViewportBase* vpUp = FindViewportUnderMouse(rootViewport, lastMouseX, lastMouseY);
        if (vpUp) viewPortActive = vpUp;
    }

    // eventos del teclado
    W3dTempMods modsShortcut;   // (vuelve los modificadores a lo apretado al salir, por cualquier return)
    if (e.type == SDL_EVENT_KEY_DOWN) {
        // los menus que abre una tecla (W, Shift+S, Ctrl+P...) salen EN EL CURSOR: lastMouseX/Y es donde esta ahora, no
        // el ultimo click (en el telefono solo se actualizaba al clickear: el menu salia lejos y se cerraba al mover)
        if (state == editNavigation) SaveMousePos();

        // CAPTURA de la ventana de atajos: la combinacion que se aprieta es para la funcion que la espera
        if (g_shortcutCapturing >= 0 &&
            W3dShortcutCapture(W3dKeyFromSDL(e.key.keysym.sym), W3dModsFromSDL(e.key.keysym.mod))) {
            g_redraw = true;
            return;
        }
        // ATAJOS REASIGNADOS: la combinacion apretada se cambia por la por defecto de la funcion que la tiene, y de
        // aca para abajo todo sigue igual que siempre. No con texto enfocado, un menu o un popup modal abiertos:
        // ahi las teclas son de ellos (tipear, navegar). El panel semi-modal del ultimo transform si deja pasar.
        if (!g_activeTextField && !RenameActive() && !LayoutMenuOpen() &&
            (!PopUpActive || PopUpActive->ClosesWithViewport())) {
            const SDL_Keycode physics = e.key.keysym.sym;
            int t = W3dKeyFromSDL(physics), m = W3dModsFromSDL(e.key.keysym.mod);
            const int t0 = t, m0 = m;
            int ctx[6]; const int nctx = W3dShortcutContexts(ctx);
            if (!W3dShortcutTranslate(ctx, nctx, t, m, e.key.repeat != 0)) return;   // era el atajo de fabrica de algo reasignado: libre (o corrio su accion)
            if (t != t0 || m != m0) {
                const SDL_Keycode newValue = W3dKeyASDL(t);
                if (newValue != SDLK_UNKNOWN) {
                    e.key.keysym.sym = newValue;
                    e.key.keysym.mod = W3dModsASDL(m);
                    modsShortcut.Set(m);
                    g_translatedKeys[physics] = newValue;
                }
            }
        }

        // IDE (editor de texto lua): con el foco por hover en un viewport IDE, el teclado
        // ES DEL TEXTO: ni los atajos globales (Ctrl+S/Ctrl+Z de proyecto) ni los del
        // viewport corren (los caracteres entran por SDL_TEXTINPUT; aca van navegacion,
        // edicion y clipboard). Popups, menus y las cajas de texto clasicas tienen
        // prioridad y siguen su camino de siempre. Va ANTES del Ctrl+S global: sobre el
        // IDE, Ctrl+S guarda EL SCRIPT (no el proyecto).
        if (PopUpLeavesPass() && !LayoutMenuOpen() && !g_activeTextField && !RenameActive() &&
            IDEKeyboardCapture(W3dKeyFromSDL(e.key.keysym.sym), e.key.repeat != 0)) {
            g_redraw = true;
            return;
        }

        // Ctrl+Z = DESHACER (undo). Global, antes del campo de texto y de los atajos del viewport.
        // Ctrl+Shift+Z tambien REHACE (convencion comun ademas de Ctrl+Y).
        // Ctrl+S = GUARDAR el proyecto (.w3d): al archivo abierto, o pide nombre
        if ((e.key.keysym.mod & KMOD_CTRL) && e.key.keysym.sym == SDLK_s && !RenameActive() &&
            e.type == SDL_EVENT_KEY_DOWN) {
            extern void SaveProject();
            SaveProject();
            return;
        }
        if ((e.key.keysym.mod & KMOD_CTRL) && e.key.keysym.sym == SDLK_z && !RenameActive()) {
            if (e.key.keysym.mod & KMOD_SHIFT) UndoRedo(); else UndoUndo();
            RebindMaterialMeshPart(); // refresca el panel (ej: asignacion de material deshecha)
            g_redraw = true; return;
        }
        // Ctrl+Y = REHACER (redo).
        if ((e.key.keysym.mod & KMOD_CTRL) && e.key.keysym.sym == SDLK_y && !RenameActive()) {
            UndoRedo(); RebindMaterialMeshPart(); g_redraw = true; return;
        }

        // CAJA DE TEXTO enfocada: captura TODO el teclado (los caracteres entran por
        // SDL_TEXTINPUT). Enter/Esc desenfocan, flechas mueven el caret, backspace/
        // supr borran; el resto se consume para no disparar atajos del viewport.
        if (g_activeTextField) {
            SDL_Keycode k = e.key.keysym.sym;
            // si es un RENAME (mesh part / material): Enter ACEPTA, ESC CANCELA (escribe / descarta el
            // nombre). En un campo normal (Name, rename en linea del outliner, export/output) Enter
            // desenfoca (el dueno confirma al perder el foco) y ESC lo devuelve al texto de ANTES y
            // desenfoca: el dueno ve su valor de siempre y no confirma nada (TextFieldCancel).
            TextFieldContinueFocus();   // el texto de antes, si el foco se puso a mano y no hubo teclas
            // (un popup que toma el Enter / Esc de su caja: la ventanita del nombre acepta o cancela de una)
            if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_ESCAPE) && PopUpActive &&
                PopUpActive->KeyInText(k != SDLK_ESCAPE)) { g_redraw = true; return; }
            const bool ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0, shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { if (RenameActive()) RenameCommit(); else if (NumEditActive()) { NumEditCommit(); NumEditExitFromPanel(); } else g_activeTextField = NULL; }
            else if (k == SDLK_ESCAPE) { if (RenameActive()) RenameCancel(); else if (NumEditActive()) { NumEditCancel(); NumEditExitFromPanel(); } else TextFieldCancel(); }
            // EL PORTAPAPELES y TODO (como el IDE): Ctrl+A / Ctrl+C / Ctrl+X / Ctrl+V. Shift + flechas selecciona una
            // parte; Ctrl + flechas salta palabras; Inicio / Fin
            else if (ctrl && k == SDLK_a) TextFieldSelectAll();
            else if (ctrl && k == SDLK_c) TextFieldCopy();
            else if (ctrl && k == SDLK_x) TextFieldCut();
            else if (ctrl && k == SDLK_v) TextFieldPaste();
            else if (k == SDLK_LEFT)  { if (ctrl) g_activeTextField->WordLeft(shift); else g_activeTextField->CaretLeft(shift);  g_redraw = true; }
            else if (k == SDLK_RIGHT) { if (ctrl) g_activeTextField->WordRight(shift); else g_activeTextField->CaretRight(shift);  g_redraw = true; }
            else if (k == SDLK_HOME)  { g_activeTextField->Start(shift); g_redraw = true; }
            else if (k == SDLK_END)   { g_activeTextField->End(shift); g_redraw = true; }
            else if (k == SDLK_BACKSPACE) { g_activeTextField->Backspace();  g_redraw = true; }
            else if (k == SDLK_DELETE)    { g_activeTextField->DelForward(); g_redraw = true; }
            return; // consume el keydown (no cae al viewport)
        }

        // TECLADO durante un LOOP CUT (mismo flujo que Symbian):
        //  flechas = orientacion (quad) / +-cortes (preview) / slide; enter = acepta/avanza;
        //  ESC/backspace/click-der = centra el corte (factor 0.5) y CONFIRMA, no cancela.
        // TECLADO durante un INSET: Enter confirma, Esc cancela, I/B/O cambian Individual/Boundary/Outset
        if (InsetActive() && InsetKey(W3dKeyFromSDL(e.key.keysym.sym))) return;
        if (BevelActive() && BevelKey(W3dKeyFromSDL(e.key.keysym.sym))) return;
        if (LoopCutActive()) {
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE)      { LoopCutClickRight(); return; }
            // Enter en orientacion (teclado) = confirma la direccion de las FLECHAS -> preview clasico
            // (el picker de puntos es solo click/tactil). En las demas fases, Enter aplica/confirma.
            if (k == SDLK_RETURN  || k == SDLK_KP_ENTER)      { if (LoopCutOrienting()) LoopCutOrientConfirmKeyboard(); else LoopCutClickLeft(lastMouseX, lastMouseY); return; }
            if (k == SDLK_LEFT)  { LoopCutKey(0); return; }
            if (k == SDLK_RIGHT) { LoopCutKey(1); return; }
            if (k == SDLK_UP)    { LoopCutKey(2); return; }
            if (k == SDLK_DOWN)  { LoopCutKey(3); return; }
        }
        // TECLADO durante un VERTEX SLIDE (Symbian / sin mouse, mismo flujo que el loop cut):
        //  arriba/abajo = cambia de direccion entre las posibles; izquierda/derecha = del 0% al 100%.
        //  Enter y Esc los maneja el modal del move (confirma / cancela), como cualquier transform.
        if (EditSlideActive()) {
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_LEFT)  { EditSlideKey(0); return; }
            if (k == SDLK_RIGHT) { EditSlideKey(1); return; }
            if (k == SDLK_UP)    { EditSlideKey(2); return; }
            if (k == SDLK_DOWN)  { EditSlideKey(3); return; }
        }
        // loop select desde una cara: flechas eligen el sentido; enter/esc acepta la seleccion
        if (LoopSelOrienting()) {
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_ESCAPE) { LoopSelConfirm(); return; }
            if (k == SDLK_LEFT  || k == SDLK_UP)   { LoopSelKey(0); return; }
            if (k == SDLK_RIGHT || k == SDLK_DOWN) { LoopSelKey(1); return; }
        }
        // pick shortest path guiado: ESC cancela el modo (los clicks los maneja el pick)
        if (PickPathGuided() && e.key.keysym.sym == SDLK_ESCAPE) { LayoutPickPathCancel(); return; }
        // Select Linked / Loop Select guiados: ESC cancela
        if (GuidedUnClickActive() && e.key.keysym.sym == SDLK_ESCAPE) { LayoutGuidedCancel(); return; }

        // ====== ESC = PAUSA del juego en Play (punto 2) ======
        // PRECEDENCIA elegida (de arriba hacia abajo):
        //   1) caja de texto / loop cut / picks guiados -> Esc es de ellos (bloques de ARRIBA, ya retornaron)
        //   2) popup o menu abierto                     -> Esc lo CIERRA (LayoutKeyUI, mas abajo: por eso
        //                                                  aca se exige !PopUpActive && !LayoutMenuOpen())
        //   3) transform en curso (3D / keyframes / 2D) -> Esc lo cancela (lo maneja el viewport)
        //   4) juego CORRIENDO (SimActive + Play)       -> Esc PAUSA la simulacion: NO la detiene ni resetea
        //      (el estado lua/escena queda congelado, SimStop NO se llama) y el editor vuelve a responder
        //      normal. Reanudar = Play del Timeline o barra espaciadora, como siempre.
        //   5) juego pausado o parado                   -> Esc sigue al viewport por hover (el Editor 2D lo
        //      usa para salir de la Vista de juego; asi: corriendo -> 1er Esc pausa, 2do Esc sale de la vista)
        { extern bool SimActive(); extern int g_poseMode;
          if (e.key.keysym.sym == SDLK_ESCAPE && SimActive() && PlayAnimation &&
              !PopUpActive && !LayoutMenuOpen() && g_poseMode == 0 &&
              state == editNavigation && !DopeXformActive() &&
              !(viewPortActive && viewPortActive->XformInProgress())) {
              PlayAnimation = false;   // pausa (lo mismo que el boton de pausa del transporte)
              g_redraw = true;
              return;
          } }

        // entrar o salir de la pantalla completa F11
        if (e.key.keysym.sym == SDLK_F11) {
            SetFullScreen(!cfg.fullscreen);
        }
        // F12 = Render Image, Ctrl+F12 = Render Animation (los botones de la tarjeta Render). Sin la repeticion de
        // la tecla mantenida: cada render es largo y no tiene que encolarse otro
        if (e.key.keysym.sym == SDLK_F12) {
            if (!e.key.repeat) {
                extern void W3dShortcutRender(bool animation);
                W3dShortcutRender((e.key.keysym.mod & KMOD_CTRL) != 0);
            }
            g_redraw = true;
            return;
        }
        // ===== SHIFT USADO COMO MODIFICADOR: ANULA EL GESTO DE "SIGUIENTE OBJETO" =====
        // Soltar Shift a secas cambia de objeto (el gesto rapido: apretar y soltar Shift
        // sin nada mas). Ese gesto se mide con ShiftCount, que Counters() sube UNA VEZ
        // POR FRAME; como el editor es por eventos, sin mover el mouse casi no acumula
        // frames y el gesto se disparaba CASI SIEMPRE. Resultado: al hacer Shift+S para ir
        // al menu Snap, soltar Shift te robaba la seleccion y "Cursor to Selected" se
        // quedaba sin nada que usar. Con esto, en cuanto Shift acompania a OTRA tecla deja
        // de ser un gesto y pasa a ser un modificador. Mismo truco que Viewport3D::Pan()
        // (ViewPort3D.cpp: ShiftCount = 100). Arregla Shift+S, Shift+A, Shift+D, Shift+Tab...
        if (LShiftPressed && e.key.keysym.sym != SDLK_LSHIFT && e.key.keysym.sym != SDLK_RSHIFT)
            ShiftCount = 100;

        // atajos de menu contextual: si abren un menu NO deben caer al
        // viewport (si no, shift+S abriria el menu Y empezaria a escalar)
        bool shortcutMenu = false;
        // ctrl+P / alt+P: emparentar. En EDICION DE HUESOS (3D o armature 2D del UV) operan
        // sobre HUESOS y cada uno abre SU popup de 2 opciones: Ctrl+P = Keep Offset / Connected,
        // Alt+P = Disconnect Bone / Clear Parent. Fuera de ahi, los menus de emparentar OBJETOS
        // de siempre (antes Ctrl+P en edicion de huesos abria el popup de OBJETOS: bug del dueno).
        if (e.key.keysym.sym == SDLK_p && (LCtrlPressed || LAltPressed)) {
            if (!LayoutParentBones(LAltPressed, lastMouseX, lastMouseY))
                LayoutMenuParent(LAltPressed, lastMouseX, lastMouseY);
            shortcutMenu = true;
        }
        // ===== ATAJOS GLOBALES QUE OPERAN LA GEOMETRIA 3D: NO con el foco en el EDITOR UV =====
        // Shift+S, W, Ctrl+T y M se disparaban aunque el mouse estuviera sobre el editor UV, le
        // robaban la tecla (shortcutMenu=true corta el ruteo al viewport) y operaban la MALLA: parado
        // en el mapa UV, la M soldaba vertices 3D y Shift+S movia la seleccion del 3D. Se gatean
        // por el viewport bajo el foco (el UV es ViewportKind 4); ahi la tecla sigue su camino
        // normal hasta UVEditor::event_key_down, que decide si hace algo o no.
        const bool focusUV = (viewPortActive && viewPortActive->isLeaf() &&
                             viewPortActive->ViewportKind() == 4);
        // shift+S: menu de snap (cursor/seleccion), 
        if (e.key.keysym.sym == SDLK_s && LShiftPressed && !focusUV) {
            LayoutMenuSnap(lastMouseX, lastMouseY);
            shortcutMenu = true;
        }
        // W (Edit Mode): menu de contexto del sub-modo (Vertex/Edge/Face) en el cursor
        if (e.key.keysym.sym == SDLK_w && InteractionMode == EditMode && !PopUpActive && !focusUV) {
            LayoutMenuEditContext(lastMouseX, lastMouseY);
            shortcutMenu = true;
        }
        // W en el MODO OBJETO y en el POSE (3d.menuContexto): lo mismo que el click derecho sobre la Vista 3D, el menu
        // Objeto (o Pose) en el mouse. Estaba en la tabla de atajos pero nadie lo atendia: solo andaba en Edicion
        else if (e.key.keysym.sym == SDLK_w && (InteractionMode == ObjectMode || InteractionMode == PoseMode) &&
                 !PopUpActive && !focusUV && !LCtrlPressed && !LShiftPressed && !LAltPressed && state == editNavigation &&
                 viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 1) {
            extern void LayoutMenuContext3D(int mx, int my);
            LayoutMenuContext3D((int)lastMouseX, (int)lastMouseY);
            shortcutMenu = true;
        }
        // Ctrl+T (Edit Mode): triangula las caras seleccionadas (>3 lados)
        if (e.key.keysym.sym == SDLK_t && LCtrlPressed && InteractionMode == EditMode && !PopUpActive && !focusUV) {
            LayoutTriangulate();
            shortcutMenu = true;
        }
        // Tab: si el activo es una malla, alterna Object<->Edit Mode (como Blender);
        // si no, cicla el viewport activo (borde verde). La logica del toggle vive
        // en LayoutToggleEditMode (la comparte Symbian).
        // Tab cambia de modo o cicla de viewport POR ENCIMA del ruteo a los viewports. Si el timeline tiene un
        // transform de keyframes a medio hacer hay que cancelarlo: se maneja con el foco congelado
        // (ViewPortClickDown) y, al irse el foco, ya nadie podria confirmarlo ni cancelarlo.
        if (e.key.keysym.sym == SDLK_TAB && DopeXformActive()) DopeXformCancel();
        if (e.key.keysym.sym == SDLK_TAB) {
            // PRECEDENCIA del Tab (de mayor a menor):
            //   1) Shift+Tab            -> toggle del SNAP (imantado), como Blender.
            //   2) UV EDITOR bajo el mouse CON armature 2D -> se lo queda EL: entra/sale de la
            //      edicion de huesos 2D. Aca NO se hace nada (shortcutMenu queda false) y la tecla
            //      sigue su ruteo normal hasta UVEditor::event_key_down, que es el unico lugar que
            //      togglea. El foco por hover ya dejo viewPortActive en el UV, mas arriba. Es un
            //      modo PROPIO del viewport: NO toca el InteractionMode global del 3D.
            //   3) malla/esqueleto activo -> Object <-> Edit/Pose Mode (LayoutToggleEditMode).
            //   4) nada de eso           -> cicla el viewport activo (borde verde).
            if (LShiftPressed) { SnapToggle(); shortcutMenu = true; }
            else if (!UVEditorTakeTab(viewPortActive)) {
                if (!LayoutToggleEditMode()) LayoutCycleActiveViewport(+1);
                shortcutMenu = true;   // consumida aca: no se re-rutea al viewport
            }
        }
        // 'm' en Edit Mode = menu MERGE (soldar verts, ); en el resto abre/cierra la barra de
        // menu del viewport activo (= soft-izq en Symbian). Teclado-solo: el primer menu se abre sin preseleccionar.
        if (e.key.keysym.sym == SDLK_m && !PopUpActive) {
            // Con el foco en el EDITOR UV el Merge NO corre (soldaria vertices de la MALLA estando
            // parado en el mapa UV: el peor de los cuatro atajos robados). Ahi la M hace lo que
            // hace en cualquier otro viewport que no sea el 3D en edicion: abrir/cerrar la barra.
            if (InteractionMode == EditMode && !focusUV) LayoutMenuMerge(lastMouseX, lastMouseY);
            else                                        LayoutToggleBarActiveViewport();
            shortcutMenu = true;
        }
        // barra de espacio = play/pausa de la animacion, este donde este el foco (el timeline no tiene por que
        // estar activo para querer ver la animacion correr en el 3D). Si estas tipeando en un campo, el espacio
        // es un espacio.
        // JUGANDO (modo Juego en play) el espacio es del juego en TODO el editor, no solo sobre el
        // viewport: pausar con la tecla de disparar cortaba el juego a cada tiro. La pausa es ESC.
        bool gameSpace = false;
        { extern bool SimActive(); extern bool AnimIsGame;
          gameSpace = AnimIsGame && PlayAnimation && SimActive(); }
        if (e.key.keysym.sym == SDLK_SPACE && !gameSpace && !g_activeTextField && !PopUpActive && TL_TogglePlay()) {
            shortcutMenu = true;
        }
        // backspace durante un transform: borra del valor numerico tipeado (los
        // numeros/operadores entran por SDL_TEXTINPUT; el backspace no genera texto)
        if (e.key.keysym.sym == SDLK_BACKSPACE && (TextFieldInputChar(8) || NumInputChar(8) || DopeNumInputChar(8))) {
            shortcutMenu = true;
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
            case SDLK_DELETE:
                // Supr cierra el popup como Esc (en los paneles: acepta; solo su boton Cancelar deshace)
                if (PopUpActive && !g_activeTextField) lk = LayoutKey::Cancel;
                break;
        }
        // coords ACTUALES del cursor para el ruteo por hover de la UI compartida: lastMouseX/Y son
        // el ultimo CLICK y quedaban viejos (las flechas iban al panel clickeado, no al hovereado;
        // parte del mismo "se queda clavado" del punto 1). El fallback es el ultimo click.
        int hovMx = lastMouseX, hovMy = lastMouseY;
        SDL_GetMouseState(&hovMx, &hovMy);
        if (shortcutMenu) {
            // ya manejado por el atajo de menu contextual
        }
        else if (lk >= 0 && LayoutKeyUI(lk, hovMx, hovMy)) {
            // consumida por la UI compartida
        }
        else {
            // redo-panel semi-modal: una tecla de operador (r/g/s/x...) con el
            // mouse AFUERA lo cierra y deja pasar la tecla; con el mouse adentro
            // la tecla no va al viewport (estas editando el panel)
            if (PopUpActive && PopUpActive->ClosesWithViewport()) {
                if (!PopUpActive->Contains(hovMx, hovMy)) {
                    PopUpActive->Close();
                    viewPortActive->event_key_down(W3dKeyFromSDL(e.key.keysym.sym), e.key.repeat != 0);
                }
            } else {
                viewPortActive->event_key_down(W3dKeyFromSDL(e.key.keysym.sym), e.key.repeat != 0);
            }
        }
    }
    else if (e.type == SDL_EVENT_KEY_UP) {
        SDL_Keycode k = e.key.keysym.sym;
        std::map<SDL_Keycode, SDL_Keycode>::iterator tr = g_translatedKeys.find(k);
        if (tr != g_translatedKeys.end()) { k = tr->second; g_translatedKeys.erase(tr); }
        viewPortActive->event_key_up(W3dKeyFromSDL(k));
    }
    // ENTRADA NUMERICA: el texto tipeado (respeta el layout del teclado: parentesis,
    // *, /, etc.) alimenta el valor exacto del transform. NumInputChar ignora todo si
    // no hay un transform activo. (PC = SDL_TEXTINPUT; Symbian alimenta NumInputChar
    // desde su propio teclado/keypad.)
    #if SDL_MAJOR_VERSION == 2
    else if (e.type == SDL_TEXTINPUT) {
        // el IDE con foco por hover se queda con el texto tipeado (editor de texto lua);
        // una caja de texto enfocada / popup / menu abierto tienen prioridad, como siempre
        if (!g_activeTextField && PopUpLeavesPass() && !LayoutMenuOpen() &&
            IDETextText(e.text.text)) {
            g_redraw = true;
        }
        // prioridad: una caja de texto enfocada; si no, el transform del 3D; si no, el de los keyframes del dope sheet
        else
        for (const char* p = e.text.text; *p; ++p)
            if (!TextFieldInputChar((unsigned char)*p) && !NumInputChar((unsigned char)*p) &&
                !DopeNumInputChar((unsigned char)*p) && *p == '/') {
                // VISTA LOCAL con un teclado que hace el "/" con Shift+7 (espanol, aleman...): SDL manda la tecla
                // "7", no la barra, y el atajo nunca llegaba. El TEXTO si dice "/". Si la barra (o la del teclado
                // numerico) esta apretada de verdad, ya la tomo el keydown: no se repite.
#ifndef W3D_SYMBIAN
                const Uint8* est = SDL_GetKeyboardState(NULL);
                const bool barPhysics = est && (est[SDL_GetScancodeFromKey(SDLK_SLASH)] || est[SDL_SCANCODE_KP_DIVIDE]);
#else
                const bool barPhysics = false;   // (el teclado del telefono no tiene SDL: la barra llega como texto)
#endif
                if (!barPhysics && viewPortActive && viewPortActive->ViewportKind() == 1 && !g_activeTextField) {
                    ((Viewport3D*)viewPortActive)->LocalViewToggle();
                    g_redraw = true;
                }
            }
    }
    #endif
}