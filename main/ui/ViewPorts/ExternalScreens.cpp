// PANTALLAS EXTERNAS (TV-out RCA, HDMI): el popup que pregunta que hacer con cada una y el dibujo de las que tienen
// viewport propio. Ver ExternalScreens.h. C++03.
#include "ViewPorts/ExternalScreens.h"
#include "ViewPorts/ViewPorts.h"         // ViewportBase, rootViewport, viewPortActive
#include "ViewPorts/ViewPort3D.h"        // Viewport3D (la vista que se copia del telefono)
#include "ViewPorts/Properties.h"        // Properties, PropsActive
#include "ViewPorts/Notifications.h"     // Notificar (no se pudo crear la superficie)
#include "WhiskUI/widgets/PopupMenu.h"   // PopupMenu, OpenMenu, MenuScreenW/H
#include "WhiskUI/draw/glesdraw.h"       // W3dScreenHeight (el flip de Y de todo el dibujo)
#include "WhiskUI/theme/colors.h"        // ColorList / ColorID (el fondo)
#include "render/RenderOptions.h"        // g_redraw
#include "ViewPorts/LayoutInput.h"      // LayoutRenderMenu (el menu abierto en una pantalla externa)
#include "ViewPorts/PopUp/PopUpBase.h"   // PopUpActive
#include "ViewPorts/LayoutTree.h"        // LayoutCompleteRoot (el modo "solo la externa" redimensiona el arbol)
#include "W3dLang.h"                     // T()
#include "w3dGraphics.h"
#include "w3dlog.h"
#include <vector>
#include <stdio.h>

namespace gfx = w3dEngine;

extern Viewport3D* Viewport3DActive;

bool (*ExternalScreenPrepare)(int id, int mode) = 0;
// cuadros que la plataforma deja paso a paso en el log (elegir el modo de una pantalla lo vuelve a prender: si algo se
// cae despues, la ultima linea dice donde)
int g_screensLogFrames = 0;
bool (*ExternalScreenActivate)(int id) = 0;
void (*ExternalScreenPresent)(int id) = 0;
void (*ExternalScreenReturn)() = 0;
void (*ExternalScreenCursor)(int id, int w, int h) = 0;

struct ScreenExt {
    int id, w, h, mode;
    bool canViewport;
    ViewportBase* root;   // el arbol propio (modo 2/3); NULL clonando
    // sus "activos" (el foco, el 3D y el panel de Propiedades de ESA pantalla: independientes de los del telefono)
    ViewportBase* activeVp;
    Viewport3D*   active3D;
    Properties*   activeProps;
};
static std::vector<ScreenExt> gScreens;

// ---- el monitor extendido: la pantalla Entrada y lo que se guardo del telefono mientras tanto ----
static int gInUse = 0;
static ViewportBase* gTelRoot = 0;
static ViewportBase* gTelVp = 0;
static Viewport3D*   gActiveTel3D = 0;
static Properties*   gTelProps = 0;
static int gTelHeight = 0, gTelMenuW = 0, gTelMenuH = 0;
static int gMenuOf = 0;   // la pantalla donde se abrio el menu/popup abierto (0 = el telefono)
static PopupMenu* gMenuOnEnter = 0;     // el menu/popup abierto al Entrar: si al Salir hay OTRO, se abrio en la pantalla
static PopUpBase* gPopupOnEnter = 0;
// SOLO LA EXTERNA: la pantalla que tiene el editor entero (0 = ninguna) y el tamano del arbol en el telefono
static int gOnly = 0;
static int gOnlyTelW = 0, gOnlyTelH = 0;
static bool gEnteredOnly = false;   // lo Entrado es la pantalla solo: el arbol es el del telefono, no se cambia

// el popup: uno solo (se pregunta por una pantalla a la vez; si se conectan dos juntas, la segunda espera)
static PopupMenu* gMenu = 0;
static int gMenuId = -1;           // la pantalla por la que pregunta el popup abierto
static bool gGrayViewport = true;  // MenuItem::gris: las opciones de viewport en gris si es false

// el popup sigue abierto preguntando? (se puede cerrar sin elegir: click afuera, otro menu. Entonces la pantalla
// queda clonando -lo que hace el sistema solo- y la proxima que se conecte pregunta igual)
static bool Asking() {
    if (gMenuId < 0) return false;
    if (gMenu && gMenu->open && OpenMenu == gMenu) return true;
    gMenuId = -1;
    return false;
}

static ScreenExt* Find(int id) {
    for (size_t i = 0; i < gScreens.size(); i++) if (gScreens[i].id == id) return &gScreens[i];
    return 0;
}

static void DeleteTree(ScreenExt& p) {
    if (!p.root) return;
    if (gInUse == p.id) ExternalScreenExit();
    if (gMenuOf == p.id) { if (OpenMenu) OpenMenu->Close(); OpenMenu = 0; gMenuOf = 0; }
    // los destructores sueltan Viewport3DActive / PropsActive si eran ellos
    delete p.root;
    p.root = 0;
    p.activeVp = 0; p.active3D = 0; p.activeProps = 0;
}

// el arbol del telefono a otro tamano: el completo (con el maximizado adentro) y despues el maximizado, como el resize
// de la plataforma (W3dLayoutBuild de Symbian, main.cpp de la PC)
static void ResizeTree(int w, int h) {
    ViewportBase* complete = LayoutCompleteRoot();
    if (!complete) complete = rootViewport;
    if (!complete) return;
    complete->x = 0; complete->y = 0;
    complete->Resize(w, h);
    if (rootViewport && rootViewport != complete) { rootViewport->x = 0; rootViewport->y = 0; rootViewport->Resize(w, h); }
}

static void ExitFromOnly() {
    if (!gOnly) return;
    if (gInUse == gOnly) ExternalScreenExit();
    if (gMenuOf == gOnly) gMenuOf = 0;
    gOnly = 0;
    ResizeTree(gOnlyTelW, gOnlyTelH);
    w3dLogf("[pantallas] el editor vuelve al telefono (%dx%d)", gOnlyTelW, gOnlyTelH);
}

int ExternalScreenOnly() { return gOnly; }

static void ActionMenu(int option);
static void Ask(ScreenExt& p) {
    if (!gMenu) { gMenu = new PopupMenu(); gMenu->action = ActionMenu; gMenu->closeFar = false; }   // (centrado: no en el mouse)
    gMenu->Clear();
    char tit[96];
    snprintf(tit, sizeof(tit), "%s (%dx%d)", T("External display"), p.w, p.h);
    gMenu->title  = tit;
    gGrayViewport = p.canViewport;
    gMenu->Add(T("Clone screen"), ScreenClone);
    gMenu->Add(T("Use new viewport"), ScreenViewport3D)->gray = &gGrayViewport;
    gMenu->Add(T("Properties viewport"), ScreenProperties)->gray = &gGrayViewport;
    gMenu->Add(T("Only external display (faster)"), ScreenOnly)->gray = &gGrayViewport;
    gMenu->selectIndex = 0;
    if (OpenMenu && OpenMenu != gMenu) OpenMenu->Close();
    gMenu->Resize();
    // centrado en la pantalla del telefono (no hay boton del que colgarlo)
    int x = (MenuScreenW - gMenu->width) / 2, y = (MenuScreenH - gMenu->height) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    gMenu->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenu;
    gMenuId = p.id;
    g_redraw = true;
}

// la proxima pantalla sin decidir (se conectaron dos a la vez): se pregunta cuando se cierra el popup anterior
static void AskNext() {
    for (size_t i = 0; i < gScreens.size(); i++)
        if (gScreens[i].mode == ScreenUndecided) { Ask(gScreens[i]); return; }
}

static void ActionMenu(int option) {
    const int id = gMenuId;
    gMenuId = -1;
    ScreenExt* p = Find(id);
    // (las opciones en gris no se pueden elegir con el popup; esto cubre cualquier otro camino)
    if (p && !p->canViewport && option != ScreenClone) {
        Notify(T("This phone can only clone the screen on the external display"), true);
        option = ScreenClone;
    }
    ExternalScreenPick(id, option);
    AskNext();
    w3dLogf("[pantallas] la accion del popup termino");
}

void ScreenExternalConnected(int id, int w, int h, bool canViewport) {
    ScreenExt* p = Find(id);
    if (p) {
        // (la misma pantalla cambio de resolucion: se re-arma el arbol a la nueva)
        p->w = w; p->h = h; p->canViewport = canViewport;
        if (p->root) { p->root->x = 0; p->root->y = 0; p->root->Resize(w, h); }
        if (gOnly == id) ResizeTree(w, h);
        w3dLogf("[pantallas] la pantalla %d cambio a %dx%d", id, w, h);
        g_redraw = true;
        return;
    }
    ScreenExt n;
    n.id = id; n.w = w; n.h = h; n.mode = ScreenUndecided; n.canViewport = canViewport; n.root = 0;
    n.activeVp = 0; n.active3D = 0; n.activeProps = 0;
    gScreens.push_back(n);
    w3dLogf("[pantallas] conectada la pantalla %d: %dx%d (%s)", id, w, h,
            canViewport ? "puede tener viewport propio" : "solo clona");
    if (!Asking()) Ask(gScreens.back());
}

void ScreenExternalDisconnected(int id) {
    for (size_t i = 0; i < gScreens.size(); i++) {
        if (gScreens[i].id != id) continue;
        if (gOnly == id) ExitFromOnly();
        DeleteTree(gScreens[i]);
        if (ExternalScreenPrepare) ExternalScreenPrepare(id, ScreenClone);   // (suelta la superficie)
        gScreens.erase(gScreens.begin() + i);
        w3dLogf("[pantallas] desconectada la pantalla %d", id);
        if (gMenuId == id) {   // el popup preguntaba por esta: se cierra
            gMenuId = -1;
            if (OpenMenu == gMenu) { gMenu->Close(); OpenMenu = 0; }
            AskNext();
        }
        g_redraw = true;
        return;
    }
}

void ExternalScreenPick(int id, int mode) {
    ScreenExt* p = Find(id);
    if (!p) return;
    if (mode == p->mode) return;
    if (gOnly == id) ExitFromOnly();
    DeleteTree(*p);
    p->mode = mode;
    if (mode == ScreenOnly) {
        if (gOnly || (ExternalScreenPrepare && !ExternalScreenPrepare(id, mode))) {
            Notify(T("Could not create a viewport on the external display: cloning the screen"), true);
            p->mode = ScreenClone;
        } else {
            ViewportBase* complete = LayoutCompleteRoot();
            if (!complete) complete = rootViewport;
            gOnlyTelW = complete ? complete->width : 0; gOnlyTelH = complete ? complete->height : 0;
            gOnly = id;
            ResizeTree(p->w, p->h);
        }
    } else if (mode == ScreenViewport3D || mode == ScreenProperties) {
        if (ExternalScreenPrepare && !ExternalScreenPrepare(id, mode)) {
            Notify(T("Could not create a viewport on the external display: cloning the screen"), true);
            p->mode = ScreenClone;
        } else {
            // un arbol PROPIO, independiente del telefono (su propia vista): se divide y se cambia como cualquiera
            if (mode == ScreenViewport3D) { Viewport3D* v = new Viewport3D(); p->root = v; p->active3D = v; }
            else { Properties* pr = new Properties(); p->root = pr; p->activeProps = pr; }
            p->activeVp = p->root;
            p->root->x = 0; p->root->y = 0;
            p->root->Resize(p->w, p->h);
        }
    } else if (ExternalScreenPrepare) {
        ExternalScreenPrepare(id, mode);   // clonar: se suelta la superficie y el sistema vuelve a copiar
    }
    w3dLogf("[pantallas] la pantalla %d queda en modo %d", id, p->mode);
    g_screensLogFrames = 3;
    g_redraw = true;
}

// ---- EL MONITOR EXTENDIDO ----
bool ExternalScreenEnter(int id) {
    if (gInUse) ExternalScreenExit();
    ScreenExt* p = Find(id);
    if (!p || (!p->root && gOnly != id)) return false;
    gEnteredOnly = (gOnly == id);
    gTelHeight = W3dScreenHeight; gTelMenuW = MenuScreenW; gTelMenuH = MenuScreenH;
    W3dScreenHeight = p->h; MenuScreenW = p->w; MenuScreenH = p->h;
    gInUse = id;
    if (gEnteredOnly) return true;   // (solo la externa: el arbol y los activos son los del telefono, ya a su tamano)
    gTelRoot = rootViewport; gTelVp = viewPortActive; gActiveTel3D = Viewport3DActive; gTelProps = PropsActive;
    rootViewport = p->root;
    viewPortActive = p->activeVp ? p->activeVp : p->root;
    Viewport3DActive = p->active3D;
    PropsActive = p->activeProps;
    gMenuOnEnter = (OpenMenu && OpenMenu->open) ? OpenMenu : 0;
    gPopupOnEnter = PopUpActive;
    return true;
}

void ExternalScreenExit() {
    if (!gInUse) return;
    ScreenExt* p = Find(gInUse);
    if (gEnteredOnly) {
        W3dScreenHeight = gTelHeight; MenuScreenW = gTelMenuW; MenuScreenH = gTelMenuH;
        gEnteredOnly = false;
        gInUse = 0;
        return;
    }
    if (p) {
        // lo que cambio mientras tanto: un split o un cambio de tipo reemplazan la raiz; el foco y los activos
        p->root = rootViewport;
        p->activeVp = viewPortActive; p->active3D = Viewport3DActive; p->activeProps = PropsActive;
        // un menu o popup que se ABRIO aca (no estaba al Entrar) es de esta pantalla: se dibuja aca, no en el telefono
        PopupMenu* menu = (OpenMenu && OpenMenu->open) ? OpenMenu : 0;
        if ((menu && menu != gMenuOnEnter) || (PopUpActive && PopUpActive != gPopupOnEnter)) gMenuOf = gInUse;
    }
    rootViewport = gTelRoot; viewPortActive = gTelVp; Viewport3DActive = gActiveTel3D; PropsActive = gTelProps;
    W3dScreenHeight = gTelHeight; MenuScreenW = gTelMenuW; MenuScreenH = gTelMenuH;
    gInUse = 0;
}

int ExternalScreenInUse() { return gInUse; }

bool ExternalScreenNeighbor(int* id, int* w, int* h) {
    for (size_t i = 0; i < gScreens.size(); i++) {
        if (!gScreens[i].root) continue;
        if (id) *id = gScreens[i].id;
        if (w) *w = gScreens[i].w;
        if (h) *h = gScreens[i].h;
        return true;
    }
    return false;
}

int MenuExternalScreen() {
    // (se cerro: el proximo que se abra decide de nuevo)
    if (gMenuOf && !(OpenMenu && OpenMenu->open) && !PopUpActive) gMenuOf = 0;
    // solo la externa: todo menu es de ella (el telefono no dibuja)
    if (gOnly && ((OpenMenu && OpenMenu->open) || PopUpActive)) return gOnly;
    return gMenuOf;
}

int ExternalScreensRender() {
    int drawn = 0;
    // los 2 primeros cuadros de cada pantalla dejan cada paso en el log: si el driver se cae, la ultima linea dice donde
    static int sFramesLog = 0;
    for (size_t i = 0; i < gScreens.size(); i++) {
        ScreenExt& p = gScreens[i];
        if (!p.root && gOnly != p.id) continue;
        const bool log = sFramesLog < 2 || g_screensLogFrames > 0;
        if (log) w3dLogf("[pantallas] cuadro %d de la pantalla %d: activar su superficie", sFramesLog, p.id);
        // cambiar de superficie, presentar y volver son de la plataforma (EGL directo): lo que el motor tiene juntado
        // sin dibujar tiene que ir ANTES, a la superficie que corresponde
        gfx::DrawPending();
        if (ExternalScreenActivate && !ExternalScreenActivate(p.id)) continue;
        if (log) w3dLogf("[pantallas] cuadro %d: dibujar (modo %d, %dx%d)", sFramesLog, p.mode, p.w, p.h);
        // el dibujo del editor mide la pantalla con los globales (el flip de Y del scissor/viewport, el clamp de los
        // menus) y engancha los "activos": se Entra a la pantalla y se Sale (vuelven los del telefono)
        ExternalScreenEnter(p.id);
        gfx::Disable(gfx::ScissorTest);
        gfx::Viewport(0, 0, p.w, p.h);
        const float* background = ColorList[static_cast<int>(ColorID::background)];
        gfx::ClearColor(background[0], background[1], background[2], 1.0f);
        gfx::DepthMask(true);   // (el clear respeta la mascara)
        gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
        rootViewport->Render();
        if (MenuExternalScreen() == p.id) LayoutRenderMenu(p.w, p.h);   // el menu que se abrio aca
        if (gOnly == p.id) NotificationsRender(p.w, p.h);                 // (y los avisos: el telefono no dibuja)
        if (ExternalScreenCursor) ExternalScreenCursor(p.id, p.w, p.h);   // el cursor, si esta aca
        ExternalScreenExit();
        if (log) w3dLogf("[pantallas] cuadro %d: presentar", sFramesLog);
        gfx::DrawPending();
        if (ExternalScreenPresent) ExternalScreenPresent(p.id);
        if (ExternalScreenReturn) ExternalScreenReturn();
        if (log) w3dLogf("[pantallas] cuadro %d: listo", sFramesLog++);
        drawn++;
    }
    return drawn;
}

int ExternalScreensCount() { return (int)gScreens.size(); }

bool ExternalScreenInfo(int idx, int* id, int* w, int* h, int* mode) {
    if (idx < 0 || idx >= (int)gScreens.size()) return false;
    const ScreenExt& p = gScreens[idx];
    if (id) *id = p.id;
    if (w) *w = p.w;
    if (h) *h = p.h;
    if (mode) *mode = p.mode;
    return true;
}
