/*
 * ==============================================================================
 *  w3dpantallas.cpp — pantallas externas en Symbian. Ver w3dpantallas.h.
 *
 *  Todo lo que se decide queda en e:\whisk3d.log con el prefijo [pantallas]: cuantas pantallas expone wserv, su
 *  tamano, si hay un cable de video (AccMonitor) y por que no se pudo crear una superficie.
 * ==============================================================================
 */
#include <e32std.h>
#include <w32std.h>
#include <coemain.h>
#include <coecntrl.h>
#include <GLES/egl.h>
#ifdef W3D_GLES2
#include <accmonitor.h>   // el cable de video (HDMI / video compuesto): solo Symbian^3, por eso solo en la variante ES 2.0
#endif

#include "w3dpantallas.h"
#include "ViewPorts/ExternalScreens.h"
#include "w3dlog.h"

// lo que dibuja la salida externa como maximo. El N95 muestra 240p por el TV-out aunque la senal sea de 480i:
// dibujar mas grande es gastar al pedo. El N8 (ES 2.0) dibuja a la resolucion de la pantalla (480i RCA, 720p HDMI).
#ifndef W3D_GLES2
static const TInt KMaxWidthExt  = 320;
static const TInt KMaxHeightExt = 240;
#endif

static const TInt KMaxScreens = 4;   // la del telefono + hasta 3 externas (el N8 tiene 2: RCA y HDMI)

// LA VENTANA DE LA PANTALLA EXTERNA: un control de CONE de verdad, aunque no dibuje nada (dibuja EGL). CONE recibe los
// eventos de TODAS las ventanas de la sesion de la app y trata el handle de cada ventana y de cada window group como un
// CCoeControl*: con un RWindow pelado (handle = el RWindow) el primer evento del window server para la ventana de la
// TV hacia que CONE llamara metodos de algo que no era un control -> KERN-EXEC 3 al elegir "nuevo viewport" en el N8.
// Por eso la ventana es un control y su puntero es tambien el handle del window group.
class CExternalWindow : public CCoeControl {
public:
    void BuildL(RWindowGroup& aGroup, const TRect& aRect) {
        CreateWindowL(aGroup);
        SetRect(aRect);
        Window().SetBackgroundColor(KRgbBlack);
        ActivateL();
    }
    RWindow& Window() { return Window(); }
private:
    void Draw(const TRect& /*aRect*/) const {}
};

struct TScreenSym {
    TBool connected;          // ya se le aviso al editor
    TInt w, h;                // a lo que se dibuja (recortado)
    TInt pantW, pantH;        // lo que mide la pantalla de verdad
    CWsScreenDevice* dev;
    RWindowGroup* group;
    CExternalWindow* window;   // (su puntero es el handle del grupo y de la ventana: ver arriba)
    EGLSurface sup;
    TBool keeps;           // el swap conserva el cuadro (EGL_BUFFER_PRESERVED): se puede mover solo el cursor
};
static TScreenSym gP[KMaxScreens];

static EGLDisplay gDpy = EGL_NO_DISPLAY;
static EGLConfig gCfg = 0;
static EGLConfig gCfgKeeps = 0;   // (0 = no hay)
void W3dScreensConfigKeeps(EGLConfig aConfig) { gCfgKeeps = aConfig; }
static EGLContext gCtx = EGL_NO_CONTEXT;
static EGLSurface gMain = EGL_NO_SURFACE;
static TUint gLastTick = 0;
static TInt gLastNumber = -1;      // NumberOfScreens de la ultima revision (para loguear solo los cambios)
static TInt gLastVideo = -1;       // idem el cable de video

#ifdef W3D_GLES2
static CAccMonitor* gAcc = NULL;
// hay un accesorio de video conectado (cable HDMI o el de video compuesto)? -1 = no se puede saber
static TInt VideoCable() {
    if (!gAcc) return -1;
    RConnectedAccessories list;
    TInt has = 0;
    TRAPD(err, gAcc->GetConnectedAccessoriesL(list));
    if (err != KErrNone) { list.ResetAndDestroy(); return -1; }
    for (TInt i = 0; i < list.Count(); i++)
        if (list[i]->Exists(KAccMonHDMI) || list[i]->Exists(KAccMonVideoOut)) has = 1;
    list.ResetAndDestroy();
    return has;
}
#else
static TInt VideoCable() { return -1; }   // (el N95 no tiene AccMonitor: alcanza con que wserv exponga la pantalla)
#endif

// LA API DE VARIAS PANTALLAS de wserv (RWsSession::NumberOfScreens, CWsScreenDevice::Construct(pantalla),
// RWindowGroup::Construct en otra pantalla). En ES 2.0 (Symbian^3) existe seguro: se llama directo. En la variante
// ES 1.1 NO se importa: si el ws32.dll del N95 (Symbian 9.2) no tuviera esos ordinales, el cargador no dejaria ni
// abrir la app. Se buscan por ordinal en runtime (los ordinales de una DLL de Symbian son fijos: si existe, es esa
// funcion); si falta alguno, no hay pantallas externas y listo.
#ifdef W3D_GLES2
static TBool ApiScreens() { return ETrue; }
static TInt ScreenNumber(RWsSession& aWs) { return aWs.NumberOfScreens(); }
static TInt BuildDevice(CWsScreenDevice* aDev, TInt aScreen) { return aDev->Construct(aScreen); }
static TInt BuildGroup(RWindowGroup* aGroup, TUint32 aHandle, CWsScreenDevice* aDev) {
    return aGroup->Construct(aHandle, EFalse, aDev);
}
static void ReleaseApiScreens() {}
#else
// (ordinales de ws32 del SDK: el .dso los numera igual en todas las versiones)
static const TInt KOrdNumberOfScreens = 571;         // TInt RWsSession::NumberOfScreens() const
static const TInt KOrdScreenDeviceConstruct = 468;   // TInt CWsScreenDevice::Construct(TInt)
static const TInt KOrdWindowGroupConstruct = 569;    // TInt RWindowGroup::Construct(TUint32, TBool, CWsScreenDevice*)
typedef TInt (*TFnNumber)(const RWsSession*);
typedef TInt (*TFnDevice)(CWsScreenDevice*, TInt);
typedef TInt (*TFnGroup)(RWindowGroup*, TUint32, TBool, CWsScreenDevice*);
static RLibrary gWs32;
static TBool gWs32Found = EFalse, gWs32Open = EFalse;
static TFnNumber gFnNumber = NULL;
static TFnDevice gFnDevice = NULL;
static TFnGroup gFnGroup = NULL;
static TBool ApiScreens() {
    if (!gWs32Found) {
        gWs32Found = ETrue;
        _LIT(KWs32, "ws32.dll");
        if (gWs32.Load(KWs32) == KErrNone) {
            gWs32Open = ETrue;
            gFnNumber = (TFnNumber)gWs32.Lookup(KOrdNumberOfScreens);
            gFnDevice = (TFnDevice)gWs32.Lookup(KOrdScreenDeviceConstruct);
            gFnGroup = (TFnGroup)gWs32.Lookup(KOrdWindowGroupConstruct);
        }
        w3dLogf("[pantallas] API de varias pantallas en ws32: %s",
                (gFnNumber && gFnDevice && gFnGroup) ? "SI" : "no (este telefono solo clona)");
    }
    return gFnNumber && gFnDevice && gFnGroup;
}
static TInt ScreenNumber(RWsSession& aWs) { return ApiScreens() ? gFnNumber(&aWs) : 1; }
static TInt BuildDevice(CWsScreenDevice* aDev, TInt aScreen) {
    return ApiScreens() ? gFnDevice(aDev, aScreen) : KErrNotSupported;
}
static TInt BuildGroup(RWindowGroup* aGroup, TUint32 aHandle, CWsScreenDevice* aDev) {
    return ApiScreens() ? gFnGroup(aGroup, aHandle, EFalse, aDev) : KErrNotSupported;
}
static void ReleaseApiScreens() {
    if (gWs32Open) gWs32.Close();
    gWs32Open = EFalse; gWs32Found = EFalse;
    gFnNumber = NULL; gFnDevice = NULL; gFnGroup = NULL;
}
#endif

static void ReleaseSurface(TScreenSym& p) {
    if (p.sup != EGL_NO_SURFACE) {
        eglMakeCurrent(gDpy, gMain, gMain, gCtx);
        eglDestroySurface(gDpy, p.sup);
        p.sup = EGL_NO_SURFACE;
    }
    p.keeps = EFalse;
    delete p.window; p.window = NULL;   // (el control cierra su ventana)
    if (p.group) { p.group->Close(); delete p.group; p.group = NULL; }
}

static void ReleaseScreen(TScreenSym& p) {
    ReleaseSurface(p);
    delete p.dev;
    p.dev = NULL;
    p.connected = EFalse;
}

// el gancho del editor: modo 2/3/4 = crear la superficie en esa pantalla; 0/1 = soltarla (vuelve a clonar)
static bool Prepare(int id, int mode) {
    if (id <= 0 || id >= KMaxScreens) return false;
    TScreenSym& p = gP[id];
    if (mode != ScreenViewport3D && mode != ScreenProperties && mode != ScreenOnly) { ReleaseSurface(p); return true; }
    if (p.sup != EGL_NO_SURFACE) return true;
    if (!p.dev) return false;
    RWsSession& ws = CCoeEnv::Static()->WsSession();
    p.window = new CExternalWindow();   // (primero el control: su puntero es el handle del grupo)
    p.group = new RWindowGroup(ws);
    if (!p.window || !p.group || BuildGroup(p.group, (TUint32)p.window, p.dev) != KErrNone) {
        w3dLogf("[pantallas] no se pudo crear el window group en la pantalla %d", id);
        delete p.group; p.group = NULL;
        delete p.window; p.window = NULL;
        return false;
    }
    p.group->SetOrdinalPosition(0);
    // centrada: si se dibuja mas chico que la pantalla (el N95 a 240p) queda en el medio
    TRAPD(errWindow, p.window->BuildL(*p.group,
          TRect(TPoint((p.pantW - p.w) / 2, (p.pantH - p.h) / 2), TSize(p.w, p.h))));
    if (errWindow != KErrNone) {
        w3dLogf("[pantallas] no se pudo crear la ventana en la pantalla %d (error %d)", id, errWindow);
        ReleaseSurface(p);
        return false;
    }
    // la superficie: a la resolucion de la pantalla; si la memoria grafica no alcanza (EGL_BAD_ALLOC: el N8 la comparte
    // con las demas apps, y un juego abierto en segundo plano se queda con buena parte) se prueba a 3/4 y a la mitad,
    // centrada, antes de rendirse y clonar
    // Primero, a la resolucion entera, con la config que conserva el cuadro (el cursor sin redibujar el editor); si esa
    // no entra, la de siempre a la resolucion entera: toda la pantalla vale mas que el truco del cursor
    static const TInt kNum[3] = { 1, 3, 1 }, kDen[3] = { 1, 4, 2 };
    const TInt w0 = p.w, h0 = p.h;
    TBool withKeeps = gCfgKeeps != 0;
    for (TInt k = 0; k < 3; k++) {
        if (k > 0) {
            p.w = w0 * kNum[k] / kDen[k]; p.h = h0 * kNum[k] / kDen[k];
            p.window->SetRect(TRect(TPoint((p.pantW - p.w) / 2, (p.pantH - p.h) / 2), TSize(p.w, p.h)));
        }
        ws.Flush();
        p.sup = eglCreateWindowSurface(gDpy, withKeeps ? gCfgKeeps : gCfg, (EGLNativeWindowType)&p.window->Window(), NULL);
        if (p.sup != EGL_NO_SURFACE && withKeeps) {   // (el contexto se creo con la otra config: que la acepte)
            const TBool ok = eglMakeCurrent(gDpy, p.sup, p.sup, gCtx) == EGL_TRUE;
            if (!ok) w3dLogf("[pantallas] el contexto no acepta la superficie que conserva el cuadro (0x%x)", eglGetError());
            eglMakeCurrent(gDpy, gMain, gMain, gCtx);
            if (!ok) { eglDestroySurface(gDpy, p.sup); p.sup = EGL_NO_SURFACE; withKeeps = EFalse; k--; continue; }
        }
        if (p.sup != EGL_NO_SURFACE) break;
        if (withKeeps) {
            w3dLogf("[pantallas] la superficie de %dx%d que conserva el cuadro no entro (0x%x): la de siempre", p.w, p.h, eglGetError());
            withKeeps = EFalse; k--; continue;
        }
        const EGLint err = eglGetError();
        w3dLogf("[pantallas] eglCreateWindowSurface de %dx%d fallo en la pantalla %d (error 0x%x, modo de color %d)%s",
                p.w, p.h, id, err, (TInt)p.window->Window().DisplayMode(),
                err == EGL_BAD_ALLOC ? ": sin memoria grafica (otra app abierta?)" : "");
        if (err != EGL_BAD_ALLOC) break;
    }
    if (p.sup == EGL_NO_SURFACE) {
        p.w = w0; p.h = h0;
        ReleaseSurface(p);
        return false;
    }
    w3dLogf("[pantallas] superficie de %dx%d creada en la pantalla %d (%dx%d)", p.w, p.h, id, p.pantW, p.pantH);
#ifdef W3D_GLES2
    // que el swap CONSERVE el cuadro: pasar el mouse sin cambiar nada redibuja solo el cursor (W3dMouseFrameCursor),
    // no 1280x720 de editor. La config se eligio con EGL_SWAP_BEHAVIOR_PRESERVED_BIT si el driver la tenia
    p.keeps = withKeeps &&
                 eglSurfaceAttrib(gDpy, p.sup, 0x3093 /*EGL_SWAP_BEHAVIOR*/, 0x3094 /*EGL_BUFFER_PRESERVED*/) == EGL_TRUE;
    w3dLogf("[pantallas] el swap de la pantalla %d %s el cuadro", id, p.keeps ? "CONSERVA" : "no conserva");
    if (!p.keeps) eglGetError();
#endif
    if (p.w != w0) ScreenExternalConnected(id, p.w, p.h, true);   // (el arbol de la pantalla, a lo que entro)
    return true;
}

static bool Activate(int id) {
    if (id <= 0 || id >= KMaxScreens || gP[id].sup == EGL_NO_SURFACE) return false;
    if (eglMakeCurrent(gDpy, gP[id].sup, gP[id].sup, gCtx) == EGL_FALSE) {
        static TBool notified = EFalse;   // (una vez: esto corre cada cuadro)
        if (!notified) { notified = ETrue; w3dLogf("[pantallas] eglMakeCurrent fallo en la pantalla %d (0x%x)", id, eglGetError()); }
        eglMakeCurrent(gDpy, gMain, gMain, gCtx);
        return false;
    }
    return true;
}

static void Present(int id) {
    if (id > 0 && id < KMaxScreens && gP[id].sup != EGL_NO_SURFACE) eglSwapBuffers(gDpy, gP[id].sup);
}

TBool W3dScreenKeepsFrame(TInt aId) {
    return aId > 0 && aId < KMaxScreens && gP[aId].sup != EGL_NO_SURFACE && gP[aId].keeps;
}

static void Return() { eglMakeCurrent(gDpy, gMain, gMain, gCtx); }

void W3dScreensInit(EGLDisplay aDisplay, EGLConfig aConfig, EGLContext aContext, EGLSurface aMain) {
    gDpy = aDisplay; gCfg = aConfig; gCtx = aContext; gMain = aMain;
    for (TInt i = 0; i < KMaxScreens; i++) {
        gP[i].connected = EFalse; gP[i].w = gP[i].h = gP[i].pantW = gP[i].pantH = 0;
        gP[i].dev = NULL; gP[i].group = NULL; gP[i].window = NULL; gP[i].sup = EGL_NO_SURFACE; gP[i].keeps = EFalse;
    }
    ExternalScreenPrepare = Prepare;
    ExternalScreenActivate = Activate;
    ExternalScreenPresent = Present;
    ExternalScreenReturn = Return;
#ifdef W3D_GLES2
    TRAPD(err, gAcc = CAccMonitor::NewL());
    if (err != KErrNone) { gAcc = NULL; w3dLogf("[pantallas] AccMonitor no disponible (err=%d)", err); }
#endif
    gLastTick = 0;
    W3dScreensTick();   // (una pantalla ya enchufada al abrir la app tambien pregunta)
}

void W3dScreensTick() {
    const TUint now = User::NTickCount();
    if (gLastTick != 0 && now - gLastTick < 1000) return;
    gLastTick = now;
    RWsSession& ws = CCoeEnv::Static()->WsSession();
    TInt n = ScreenNumber(ws);
    if (n > KMaxScreens) n = KMaxScreens;
    const TInt video = VideoCable();
    if (n != gLastNumber || video != gLastVideo) {
        w3dLogf("[pantallas] wserv expone %d pantalla(s); cable de video: %s", n,
                video < 0 ? "no se sabe" : (video ? "SI" : "no"));
        gLastNumber = n; gLastVideo = video;
    }
    for (TInt i = 1; i < KMaxScreens; i++) {
        TScreenSym& p = gP[i];
        TSize size(0, 0);
        if (i < n) {
            if (!p.dev) {
                p.dev = new CWsScreenDevice(ws);
                if (p.dev && BuildDevice(p.dev, i) != KErrNone) { delete p.dev; p.dev = NULL; }
            }
            if (p.dev) size = p.dev->SizeInPixels();
        }
        // conectada = wserv la expone CON tamano y (si se puede saber) hay un cable de video: el N8 puede tener la
        // pantalla 1 configurada siempre, aunque no haya nada enchufado
        const TBool has = size.iWidth > 0 && size.iHeight > 0 && video != 0;
        if (has && (!p.connected || size.iWidth != p.pantW || size.iHeight != p.pantH)) {
            const TBool newValue = !p.connected;
            p.pantW = size.iWidth; p.pantH = size.iHeight;
            p.w = p.pantW; p.h = p.pantH;
#ifndef W3D_GLES2
            if (p.w > KMaxWidthExt) p.w = KMaxWidthExt;
            if (p.h > KMaxHeightExt) p.h = KMaxHeightExt;
#endif
            p.connected = ETrue;
            if (!newValue && p.sup != EGL_NO_SURFACE) {   // cambio la resolucion: la ventana se re-acomoda
                p.window->SetRect(TRect(TPoint((p.pantW - p.w) / 2, (p.pantH - p.h) / 2), TSize(p.w, p.h)));
            }
            ScreenExternalConnected(i, p.w, p.h, true);
        } else if (!has && p.connected) {
            ScreenExternalDisconnected(i);   // (el editor llama a Prepare(clonar): suelta la superficie)
            ReleaseScreen(p);
        } else if (!has && p.dev && i >= n) {
            ReleaseScreen(p);
        }
    }
}

void W3dScreensDestroy() {
    for (TInt i = 1; i < KMaxScreens; i++) {
        if (gP[i].connected) ScreenExternalDisconnected(i);
        ReleaseScreen(gP[i]);
    }
    ExternalScreenPrepare = 0; ExternalScreenActivate = 0; ExternalScreenPresent = 0; ExternalScreenReturn = 0;
    ReleaseApiScreens();
#ifdef W3D_GLES2
    delete gAcc;
    gAcc = NULL;
#endif
}
