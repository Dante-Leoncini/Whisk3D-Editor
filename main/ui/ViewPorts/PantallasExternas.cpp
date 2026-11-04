// PANTALLAS EXTERNAS (TV-out RCA, HDMI): el popup que pregunta que hacer con cada una y el dibujo de las que tienen
// viewport propio. Ver PantallasExternas.h. C++03.
#include "ViewPorts/PantallasExternas.h"
#include "ViewPorts/ViewPorts.h"         // ViewportBase, rootViewport, viewPortActive
#include "ViewPorts/ViewPort3D.h"        // Viewport3D (la vista que se copia del telefono)
#include "ViewPorts/Properties.h"        // Properties, PropsActivo
#include "ViewPorts/Notificaciones.h"    // Notificar (no se pudo crear la superficie)
#include "WhiskUI/widgets/PopupMenu.h"   // PopupMenu, MenuAbierto, MenuPantallaW/H
#include "WhiskUI/draw/glesdraw.h"       // W3dPantallaAlto (el flip de Y de todo el dibujo)
#include "WhiskUI/theme/colores.h"       // ListaColores / ColorID (el fondo)
#include "render/OpcionesRender.h"       // g_redraw
#include "ViewPorts/LayoutInput.h"      // LayoutRenderMenu (el menu abierto en una pantalla externa)
#include "ViewPorts/PopUp/PopUpBase.h"   // PopUpActive
#include "ViewPorts/LayoutArbol.h"       // LayoutRaizCompleta (el modo "solo la externa" redimensiona el arbol)
#include "W3dLang.h"                     // T()
#include "w3dGraphics.h"
#include "w3dlog.h"
#include <vector>
#include <stdio.h>

namespace gfx = w3dEngine;

extern Viewport3D* Viewport3DActive;

bool (*PantallaExternaPreparar)(int id, int modo) = 0;
// cuadros que la plataforma deja paso a paso en el log (elegir el modo de una pantalla lo vuelve a prender: si algo se
// cae despues, la ultima linea dice donde)
int g_pantallasLogCuadros = 0;
bool (*PantallaExternaActivar)(int id) = 0;
void (*PantallaExternaPresentar)(int id) = 0;
void (*PantallaExternaVolver)() = 0;
void (*PantallaExternaCursor)(int id, int w, int h) = 0;

struct PantallaExt {
    int id, w, h, modo;
    bool puedeViewport;
    ViewportBase* raiz;   // el arbol propio (modo 2/3); NULL clonando
    // sus "activos" (el foco, el 3D y el panel de Propiedades de ESA pantalla: independientes de los del telefono)
    ViewportBase* activoVp;
    Viewport3D*   activo3D;
    Properties*   activoProps;
};
static std::vector<PantallaExt> gPantallas;

// ---- el monitor extendido: la pantalla Entrada y lo que se guardo del telefono mientras tanto ----
static int gEnUso = 0;
static ViewportBase* gTelRaiz = 0;
static ViewportBase* gTelVp = 0;
static Viewport3D*   gTelActivo3D = 0;
static Properties*   gTelProps = 0;
static int gTelAlto = 0, gTelMenuW = 0, gTelMenuH = 0;
static int gMenuDe = 0;   // la pantalla donde se abrio el menu/popup abierto (0 = el telefono)
static PopupMenu* gMenuAlEntrar = 0;     // el menu/popup abierto al Entrar: si al Salir hay OTRO, se abrio en la pantalla
static PopUpBase* gPopupAlEntrar = 0;
// SOLO LA EXTERNA: la pantalla que tiene el editor entero (0 = ninguna) y el tamano del arbol en el telefono
static int gSolo = 0;
static int gSoloTelW = 0, gSoloTelH = 0;
static bool gEntroSolo = false;   // lo Entrado es la pantalla solo: el arbol es el del telefono, no se cambia

// el popup: uno solo (se pregunta por una pantalla a la vez; si se conectan dos juntas, la segunda espera)
static PopupMenu* gMenu = 0;
static int gMenuId = -1;           // la pantalla por la que pregunta el popup abierto
static bool gGrisViewport = true;  // MenuItem::gris: las opciones de viewport en gris si es false

// el popup sigue abierto preguntando? (se puede cerrar sin elegir: click afuera, otro menu. Entonces la pantalla
// queda clonando -lo que hace el sistema solo- y la proxima que se conecte pregunta igual)
static bool Preguntando() {
    if (gMenuId < 0) return false;
    if (gMenu && gMenu->abierto && MenuAbierto == gMenu) return true;
    gMenuId = -1;
    return false;
}

static PantallaExt* Buscar(int id) {
    for (size_t i = 0; i < gPantallas.size(); i++) if (gPantallas[i].id == id) return &gPantallas[i];
    return 0;
}

static void BorrarArbol(PantallaExt& p) {
    if (!p.raiz) return;
    if (gEnUso == p.id) PantallaExternaSalir();
    if (gMenuDe == p.id) { if (MenuAbierto) MenuAbierto->Cerrar(); MenuAbierto = 0; gMenuDe = 0; }
    // los destructores sueltan Viewport3DActive / PropsActivo si eran ellos
    delete p.raiz;
    p.raiz = 0;
    p.activoVp = 0; p.activo3D = 0; p.activoProps = 0;
}

// el arbol del telefono a otro tamano: el completo (con el maximizado adentro) y despues el maximizado, como el resize
// de la plataforma (W3dLayoutBuild de Symbian, main.cpp de la PC)
static void RedimensionarArbol(int w, int h) {
    ViewportBase* completa = LayoutRaizCompleta();
    if (!completa) completa = rootViewport;
    if (!completa) return;
    completa->x = 0; completa->y = 0;
    completa->Resize(w, h);
    if (rootViewport && rootViewport != completa) { rootViewport->x = 0; rootViewport->y = 0; rootViewport->Resize(w, h); }
}

static void SalirDeSolo() {
    if (!gSolo) return;
    if (gEnUso == gSolo) PantallaExternaSalir();
    if (gMenuDe == gSolo) gMenuDe = 0;
    gSolo = 0;
    RedimensionarArbol(gSoloTelW, gSoloTelH);
    w3dLogf("[pantallas] el editor vuelve al telefono (%dx%d)", gSoloTelW, gSoloTelH);
}

int PantallaExternaSolo() { return gSolo; }

static void AccionMenu(int opcion);
static void Preguntar(PantallaExt& p) {
    if (!gMenu) { gMenu = new PopupMenu(); gMenu->action = AccionMenu; gMenu->cerrarLejos = false; }   // (centrado: no en el mouse)
    gMenu->Limpiar();
    char tit[96];
    snprintf(tit, sizeof(tit), "%s (%dx%d)", T("External display"), p.w, p.h);
    gMenu->titulo = tit;
    gGrisViewport = p.puedeViewport;
    gMenu->Agregar(T("Clone screen"), PantallaClonar);
    gMenu->Agregar(T("Use new viewport"), PantallaViewport3D)->gris = &gGrisViewport;
    gMenu->Agregar(T("Properties viewport"), PantallaPropiedades)->gris = &gGrisViewport;
    gMenu->Agregar(T("Only external display (faster)"), PantallaSolo)->gris = &gGrisViewport;
    gMenu->selectIndex = 0;
    if (MenuAbierto && MenuAbierto != gMenu) MenuAbierto->Cerrar();
    gMenu->Resize();
    // centrado en la pantalla del telefono (no hay boton del que colgarlo)
    int x = (MenuPantallaW - gMenu->width) / 2, y = (MenuPantallaH - gMenu->height) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    gMenu->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenu;
    gMenuId = p.id;
    g_redraw = true;
}

// la proxima pantalla sin decidir (se conectaron dos a la vez): se pregunta cuando se cierra el popup anterior
static void PreguntarSiguiente() {
    for (size_t i = 0; i < gPantallas.size(); i++)
        if (gPantallas[i].modo == PantallaSinDecidir) { Preguntar(gPantallas[i]); return; }
}

static void AccionMenu(int opcion) {
    const int id = gMenuId;
    gMenuId = -1;
    PantallaExt* p = Buscar(id);
    // (las opciones en gris no se pueden elegir con el popup; esto cubre cualquier otro camino)
    if (p && !p->puedeViewport && opcion != PantallaClonar) {
        Notificar(T("This phone can only clone the screen on the external display"), true);
        opcion = PantallaClonar;
    }
    PantallaExternaElegir(id, opcion);
    PreguntarSiguiente();
    w3dLogf("[pantallas] la accion del popup termino");
}

void PantallaExternaConectada(int id, int w, int h, bool puedeViewport) {
    PantallaExt* p = Buscar(id);
    if (p) {
        // (la misma pantalla cambio de resolucion: se re-arma el arbol a la nueva)
        p->w = w; p->h = h; p->puedeViewport = puedeViewport;
        if (p->raiz) { p->raiz->x = 0; p->raiz->y = 0; p->raiz->Resize(w, h); }
        if (gSolo == id) RedimensionarArbol(w, h);
        w3dLogf("[pantallas] la pantalla %d cambio a %dx%d", id, w, h);
        g_redraw = true;
        return;
    }
    PantallaExt n;
    n.id = id; n.w = w; n.h = h; n.modo = PantallaSinDecidir; n.puedeViewport = puedeViewport; n.raiz = 0;
    n.activoVp = 0; n.activo3D = 0; n.activoProps = 0;
    gPantallas.push_back(n);
    w3dLogf("[pantallas] conectada la pantalla %d: %dx%d (%s)", id, w, h,
            puedeViewport ? "puede tener viewport propio" : "solo clona");
    if (!Preguntando()) Preguntar(gPantallas.back());
}

void PantallaExternaDesconectada(int id) {
    for (size_t i = 0; i < gPantallas.size(); i++) {
        if (gPantallas[i].id != id) continue;
        if (gSolo == id) SalirDeSolo();
        BorrarArbol(gPantallas[i]);
        if (PantallaExternaPreparar) PantallaExternaPreparar(id, PantallaClonar);   // (suelta la superficie)
        gPantallas.erase(gPantallas.begin() + i);
        w3dLogf("[pantallas] desconectada la pantalla %d", id);
        if (gMenuId == id) {   // el popup preguntaba por esta: se cierra
            gMenuId = -1;
            if (MenuAbierto == gMenu) { gMenu->Cerrar(); MenuAbierto = 0; }
            PreguntarSiguiente();
        }
        g_redraw = true;
        return;
    }
}

void PantallaExternaElegir(int id, int modo) {
    PantallaExt* p = Buscar(id);
    if (!p) return;
    if (modo == p->modo) return;
    if (gSolo == id) SalirDeSolo();
    BorrarArbol(*p);
    p->modo = modo;
    if (modo == PantallaSolo) {
        if (gSolo || (PantallaExternaPreparar && !PantallaExternaPreparar(id, modo))) {
            Notificar(T("Could not create a viewport on the external display: cloning the screen"), true);
            p->modo = PantallaClonar;
        } else {
            ViewportBase* completa = LayoutRaizCompleta();
            if (!completa) completa = rootViewport;
            gSoloTelW = completa ? completa->width : 0; gSoloTelH = completa ? completa->height : 0;
            gSolo = id;
            RedimensionarArbol(p->w, p->h);
        }
    } else if (modo == PantallaViewport3D || modo == PantallaPropiedades) {
        if (PantallaExternaPreparar && !PantallaExternaPreparar(id, modo)) {
            Notificar(T("Could not create a viewport on the external display: cloning the screen"), true);
            p->modo = PantallaClonar;
        } else {
            // un arbol PROPIO, independiente del telefono (su propia vista): se divide y se cambia como cualquiera
            if (modo == PantallaViewport3D) { Viewport3D* v = new Viewport3D(); p->raiz = v; p->activo3D = v; }
            else { Properties* pr = new Properties(); p->raiz = pr; p->activoProps = pr; }
            p->activoVp = p->raiz;
            p->raiz->x = 0; p->raiz->y = 0;
            p->raiz->Resize(p->w, p->h);
        }
    } else if (PantallaExternaPreparar) {
        PantallaExternaPreparar(id, modo);   // clonar: se suelta la superficie y el sistema vuelve a copiar
    }
    w3dLogf("[pantallas] la pantalla %d queda en modo %d", id, p->modo);
    g_pantallasLogCuadros = 3;
    g_redraw = true;
}

// ---- EL MONITOR EXTENDIDO ----
bool PantallaExternaEntrar(int id) {
    if (gEnUso) PantallaExternaSalir();
    PantallaExt* p = Buscar(id);
    if (!p || (!p->raiz && gSolo != id)) return false;
    gEntroSolo = (gSolo == id);
    gTelAlto = W3dPantallaAlto; gTelMenuW = MenuPantallaW; gTelMenuH = MenuPantallaH;
    W3dPantallaAlto = p->h; MenuPantallaW = p->w; MenuPantallaH = p->h;
    gEnUso = id;
    if (gEntroSolo) return true;   // (solo la externa: el arbol y los activos son los del telefono, ya a su tamano)
    gTelRaiz = rootViewport; gTelVp = viewPortActive; gTelActivo3D = Viewport3DActive; gTelProps = PropsActivo;
    rootViewport = p->raiz;
    viewPortActive = p->activoVp ? p->activoVp : p->raiz;
    Viewport3DActive = p->activo3D;
    PropsActivo = p->activoProps;
    gMenuAlEntrar = (MenuAbierto && MenuAbierto->abierto) ? MenuAbierto : 0;
    gPopupAlEntrar = PopUpActive;
    return true;
}

void PantallaExternaSalir() {
    if (!gEnUso) return;
    PantallaExt* p = Buscar(gEnUso);
    if (gEntroSolo) {
        W3dPantallaAlto = gTelAlto; MenuPantallaW = gTelMenuW; MenuPantallaH = gTelMenuH;
        gEntroSolo = false;
        gEnUso = 0;
        return;
    }
    if (p) {
        // lo que cambio mientras tanto: un split o un cambio de tipo reemplazan la raiz; el foco y los activos
        p->raiz = rootViewport;
        p->activoVp = viewPortActive; p->activo3D = Viewport3DActive; p->activoProps = PropsActivo;
        // un menu o popup que se ABRIO aca (no estaba al Entrar) es de esta pantalla: se dibuja aca, no en el telefono
        PopupMenu* menu = (MenuAbierto && MenuAbierto->abierto) ? MenuAbierto : 0;
        if ((menu && menu != gMenuAlEntrar) || (PopUpActive && PopUpActive != gPopupAlEntrar)) gMenuDe = gEnUso;
    }
    rootViewport = gTelRaiz; viewPortActive = gTelVp; Viewport3DActive = gTelActivo3D; PropsActivo = gTelProps;
    W3dPantallaAlto = gTelAlto; MenuPantallaW = gTelMenuW; MenuPantallaH = gTelMenuH;
    gEnUso = 0;
}

int PantallaExternaEnUso() { return gEnUso; }

bool PantallaExternaVecina(int* id, int* w, int* h) {
    for (size_t i = 0; i < gPantallas.size(); i++) {
        if (!gPantallas[i].raiz) continue;
        if (id) *id = gPantallas[i].id;
        if (w) *w = gPantallas[i].w;
        if (h) *h = gPantallas[i].h;
        return true;
    }
    return false;
}

int PantallaExternaDelMenu() {
    // (se cerro: el proximo que se abra decide de nuevo)
    if (gMenuDe && !(MenuAbierto && MenuAbierto->abierto) && !PopUpActive) gMenuDe = 0;
    // solo la externa: todo menu es de ella (el telefono no dibuja)
    if (gSolo && ((MenuAbierto && MenuAbierto->abierto) || PopUpActive)) return gSolo;
    return gMenuDe;
}

int PantallasExternasRender() {
    int dibujadas = 0;
    // los 2 primeros cuadros de cada pantalla dejan cada paso en el log: si el driver se cae, la ultima linea dice donde
    static int sCuadrosLog = 0;
    for (size_t i = 0; i < gPantallas.size(); i++) {
        PantallaExt& p = gPantallas[i];
        if (!p.raiz && gSolo != p.id) continue;
        const bool log = sCuadrosLog < 2 || g_pantallasLogCuadros > 0;
        if (log) w3dLogf("[pantallas] cuadro %d de la pantalla %d: activar su superficie", sCuadrosLog, p.id);
        // cambiar de superficie, presentar y volver son de la plataforma (EGL directo): lo que el motor tiene juntado
        // sin dibujar tiene que ir ANTES, a la superficie que corresponde
        gfx::DibujarPendiente();
        if (PantallaExternaActivar && !PantallaExternaActivar(p.id)) continue;
        if (log) w3dLogf("[pantallas] cuadro %d: dibujar (modo %d, %dx%d)", sCuadrosLog, p.modo, p.w, p.h);
        // el dibujo del editor mide la pantalla con los globales (el flip de Y del scissor/viewport, el clamp de los
        // menus) y engancha los "activos": se Entra a la pantalla y se Sale (vuelven los del telefono)
        PantallaExternaEntrar(p.id);
        gfx::Disable(gfx::ScissorTest);
        gfx::Viewport(0, 0, p.w, p.h);
        const float* fondo = ListaColores[static_cast<int>(ColorID::background)];
        gfx::ClearColor(fondo[0], fondo[1], fondo[2], 1.0f);
        gfx::DepthMask(true);   // (el clear respeta la mascara)
        gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
        rootViewport->Render();
        if (PantallaExternaDelMenu() == p.id) LayoutRenderMenu(p.w, p.h);   // el menu que se abrio aca
        if (gSolo == p.id) NotificacionesRender(p.w, p.h);                 // (y los avisos: el telefono no dibuja)
        if (PantallaExternaCursor) PantallaExternaCursor(p.id, p.w, p.h);   // el cursor, si esta aca
        PantallaExternaSalir();
        if (log) w3dLogf("[pantallas] cuadro %d: presentar", sCuadrosLog);
        gfx::DibujarPendiente();
        if (PantallaExternaPresentar) PantallaExternaPresentar(p.id);
        if (PantallaExternaVolver) PantallaExternaVolver();
        if (log) w3dLogf("[pantallas] cuadro %d: listo", sCuadrosLog++);
        dibujadas++;
    }
    return dibujadas;
}

int PantallasExternasCantidad() { return (int)gPantallas.size(); }

bool PantallaExternaInfo(int idx, int* id, int* w, int* h, int* modo) {
    if (idx < 0 || idx >= (int)gPantallas.size()) return false;
    const PantallaExt& p = gPantallas[idx];
    if (id) *id = p.id;
    if (w) *w = p.w;
    if (h) *h = p.h;
    if (modo) *modo = p.modo;
    return true;
}
