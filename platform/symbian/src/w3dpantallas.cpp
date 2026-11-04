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
#include "ViewPorts/PantallasExternas.h"
#include "w3dlog.h"

// lo que dibuja la salida externa como maximo. El N95 muestra 240p por el TV-out aunque la senal sea de 480i:
// dibujar mas grande es gastar al pedo. El N8 (ES 2.0) dibuja a la resolucion de la pantalla (480i RCA, 720p HDMI).
#ifndef W3D_GLES2
static const TInt KMaxAnchoExt = 320;
static const TInt KMaxAltoExt  = 240;
#endif

static const TInt KMaxPantallas = 4;   // la del telefono + hasta 3 externas (el N8 tiene 2: RCA y HDMI)

// LA VENTANA DE LA PANTALLA EXTERNA: un control de CONE de verdad, aunque no dibuje nada (dibuja EGL). CONE recibe los
// eventos de TODAS las ventanas de la sesion de la app y trata el handle de cada ventana y de cada window group como un
// CCoeControl*: con un RWindow pelado (handle = el RWindow) el primer evento del window server para la ventana de la
// TV hacia que CONE llamara metodos de algo que no era un control -> KERN-EXEC 3 al elegir "nuevo viewport" en el N8.
// Por eso la ventana es un control y su puntero es tambien el handle del window group.
class CVentanaExterna : public CCoeControl {
public:
    void ConstruirL(RWindowGroup& aGrupo, const TRect& aRect) {
        CreateWindowL(aGrupo);
        SetRect(aRect);
        Window().SetBackgroundColor(KRgbBlack);
        ActivateL();
    }
    RWindow& Ventana() { return Window(); }
private:
    void Draw(const TRect& /*aRect*/) const {}
};

struct TPantallaSym {
    TBool conectada;          // ya se le aviso al editor
    TInt w, h;                // a lo que se dibuja (recortado)
    TInt pantW, pantH;        // lo que mide la pantalla de verdad
    CWsScreenDevice* dev;
    RWindowGroup* grupo;
    CVentanaExterna* ventana;   // (su puntero es el handle del grupo y de la ventana: ver arriba)
    EGLSurface sup;
    TBool conserva;           // el swap conserva el cuadro (EGL_BUFFER_PRESERVED): se puede mover solo el cursor
};
static TPantallaSym gP[KMaxPantallas];

static EGLDisplay gDpy = EGL_NO_DISPLAY;
static EGLConfig gCfg = 0;
static EGLConfig gCfgConserva = 0;   // (0 = no hay)
void W3dPantallasConfigConserva(EGLConfig aConfig) { gCfgConserva = aConfig; }
static EGLContext gCtx = EGL_NO_CONTEXT;
static EGLSurface gPrincipal = EGL_NO_SURFACE;
static TUint gUltimoTick = 0;
static TInt gUltimoNumero = -1;      // NumberOfScreens de la ultima revision (para loguear solo los cambios)
static TInt gUltimoVideo = -1;       // idem el cable de video

#ifdef W3D_GLES2
static CAccMonitor* gAcc = NULL;
// hay un accesorio de video conectado (cable HDMI o el de video compuesto)? -1 = no se puede saber
static TInt CableDeVideo() {
    if (!gAcc) return -1;
    RConnectedAccessories lista;
    TInt hay = 0;
    TRAPD(err, gAcc->GetConnectedAccessoriesL(lista));
    if (err != KErrNone) { lista.ResetAndDestroy(); return -1; }
    for (TInt i = 0; i < lista.Count(); i++)
        if (lista[i]->Exists(KAccMonHDMI) || lista[i]->Exists(KAccMonVideoOut)) hay = 1;
    lista.ResetAndDestroy();
    return hay;
}
#else
static TInt CableDeVideo() { return -1; }   // (el N95 no tiene AccMonitor: alcanza con que wserv exponga la pantalla)
#endif

// LA API DE VARIAS PANTALLAS de wserv (RWsSession::NumberOfScreens, CWsScreenDevice::Construct(pantalla),
// RWindowGroup::Construct en otra pantalla). En ES 2.0 (Symbian^3) existe seguro: se llama directo. En la variante
// ES 1.1 NO se importa: si el ws32.dll del N95 (Symbian 9.2) no tuviera esos ordinales, el cargador no dejaria ni
// abrir la app. Se buscan por ordinal en runtime (los ordinales de una DLL de Symbian son fijos: si existe, es esa
// funcion); si falta alguno, no hay pantallas externas y listo.
#ifdef W3D_GLES2
static TBool ApiPantallas() { return ETrue; }
static TInt NumeroDePantallas(RWsSession& aWs) { return aWs.NumberOfScreens(); }
static TInt ConstruirDispositivo(CWsScreenDevice* aDev, TInt aPantalla) { return aDev->Construct(aPantalla); }
static TInt ConstruirGrupo(RWindowGroup* aGrupo, TUint32 aHandle, CWsScreenDevice* aDev) {
    return aGrupo->Construct(aHandle, EFalse, aDev);
}
static void SoltarApiPantallas() {}
#else
// (ordinales de ws32 del SDK: el .dso los numera igual en todas las versiones)
static const TInt KOrdNumberOfScreens = 571;         // TInt RWsSession::NumberOfScreens() const
static const TInt KOrdScreenDeviceConstruct = 468;   // TInt CWsScreenDevice::Construct(TInt)
static const TInt KOrdWindowGroupConstruct = 569;    // TInt RWindowGroup::Construct(TUint32, TBool, CWsScreenDevice*)
typedef TInt (*TFnNumero)(const RWsSession*);
typedef TInt (*TFnDispositivo)(CWsScreenDevice*, TInt);
typedef TInt (*TFnGrupo)(RWindowGroup*, TUint32, TBool, CWsScreenDevice*);
static RLibrary gWs32;
static TBool gWs32Buscado = EFalse, gWs32Abierta = EFalse;
static TFnNumero gFnNumero = NULL;
static TFnDispositivo gFnDispositivo = NULL;
static TFnGrupo gFnGrupo = NULL;
static TBool ApiPantallas() {
    if (!gWs32Buscado) {
        gWs32Buscado = ETrue;
        _LIT(KWs32, "ws32.dll");
        if (gWs32.Load(KWs32) == KErrNone) {
            gWs32Abierta = ETrue;
            gFnNumero = (TFnNumero)gWs32.Lookup(KOrdNumberOfScreens);
            gFnDispositivo = (TFnDispositivo)gWs32.Lookup(KOrdScreenDeviceConstruct);
            gFnGrupo = (TFnGrupo)gWs32.Lookup(KOrdWindowGroupConstruct);
        }
        w3dLogf("[pantallas] API de varias pantallas en ws32: %s",
                (gFnNumero && gFnDispositivo && gFnGrupo) ? "SI" : "no (este telefono solo clona)");
    }
    return gFnNumero && gFnDispositivo && gFnGrupo;
}
static TInt NumeroDePantallas(RWsSession& aWs) { return ApiPantallas() ? gFnNumero(&aWs) : 1; }
static TInt ConstruirDispositivo(CWsScreenDevice* aDev, TInt aPantalla) {
    return ApiPantallas() ? gFnDispositivo(aDev, aPantalla) : KErrNotSupported;
}
static TInt ConstruirGrupo(RWindowGroup* aGrupo, TUint32 aHandle, CWsScreenDevice* aDev) {
    return ApiPantallas() ? gFnGrupo(aGrupo, aHandle, EFalse, aDev) : KErrNotSupported;
}
static void SoltarApiPantallas() {
    if (gWs32Abierta) gWs32.Close();
    gWs32Abierta = EFalse; gWs32Buscado = EFalse;
    gFnNumero = NULL; gFnDispositivo = NULL; gFnGrupo = NULL;
}
#endif

static void SoltarSuperficie(TPantallaSym& p) {
    if (p.sup != EGL_NO_SURFACE) {
        eglMakeCurrent(gDpy, gPrincipal, gPrincipal, gCtx);
        eglDestroySurface(gDpy, p.sup);
        p.sup = EGL_NO_SURFACE;
    }
    p.conserva = EFalse;
    delete p.ventana; p.ventana = NULL;   // (el control cierra su ventana)
    if (p.grupo) { p.grupo->Close(); delete p.grupo; p.grupo = NULL; }
}

static void SoltarPantalla(TPantallaSym& p) {
    SoltarSuperficie(p);
    delete p.dev;
    p.dev = NULL;
    p.conectada = EFalse;
}

// el gancho del editor: modo 2/3/4 = crear la superficie en esa pantalla; 0/1 = soltarla (vuelve a clonar)
static bool Preparar(int id, int modo) {
    if (id <= 0 || id >= KMaxPantallas) return false;
    TPantallaSym& p = gP[id];
    if (modo != PantallaViewport3D && modo != PantallaPropiedades && modo != PantallaSolo) { SoltarSuperficie(p); return true; }
    if (p.sup != EGL_NO_SURFACE) return true;
    if (!p.dev) return false;
    RWsSession& ws = CCoeEnv::Static()->WsSession();
    p.ventana = new CVentanaExterna();   // (primero el control: su puntero es el handle del grupo)
    p.grupo = new RWindowGroup(ws);
    if (!p.ventana || !p.grupo || ConstruirGrupo(p.grupo, (TUint32)p.ventana, p.dev) != KErrNone) {
        w3dLogf("[pantallas] no se pudo crear el window group en la pantalla %d", id);
        delete p.grupo; p.grupo = NULL;
        delete p.ventana; p.ventana = NULL;
        return false;
    }
    p.grupo->SetOrdinalPosition(0);
    // centrada: si se dibuja mas chico que la pantalla (el N95 a 240p) queda en el medio
    TRAPD(errVentana, p.ventana->ConstruirL(*p.grupo,
          TRect(TPoint((p.pantW - p.w) / 2, (p.pantH - p.h) / 2), TSize(p.w, p.h))));
    if (errVentana != KErrNone) {
        w3dLogf("[pantallas] no se pudo crear la ventana en la pantalla %d (error %d)", id, errVentana);
        SoltarSuperficie(p);
        return false;
    }
    // la superficie: a la resolucion de la pantalla; si la memoria grafica no alcanza (EGL_BAD_ALLOC: el N8 la comparte
    // con las demas apps, y un juego abierto en segundo plano se queda con buena parte) se prueba a 3/4 y a la mitad,
    // centrada, antes de rendirse y clonar
    // Primero, a la resolucion entera, con la config que conserva el cuadro (el cursor sin redibujar el editor); si esa
    // no entra, la de siempre a la resolucion entera: toda la pantalla vale mas que el truco del cursor
    static const TInt kNum[3] = { 1, 3, 1 }, kDen[3] = { 1, 4, 2 };
    const TInt w0 = p.w, h0 = p.h;
    TBool conConserva = gCfgConserva != 0;
    for (TInt k = 0; k < 3; k++) {
        if (k > 0) {
            p.w = w0 * kNum[k] / kDen[k]; p.h = h0 * kNum[k] / kDen[k];
            p.ventana->SetRect(TRect(TPoint((p.pantW - p.w) / 2, (p.pantH - p.h) / 2), TSize(p.w, p.h)));
        }
        ws.Flush();
        p.sup = eglCreateWindowSurface(gDpy, conConserva ? gCfgConserva : gCfg, (EGLNativeWindowType)&p.ventana->Ventana(), NULL);
        if (p.sup != EGL_NO_SURFACE && conConserva) {   // (el contexto se creo con la otra config: que la acepte)
            const TBool ok = eglMakeCurrent(gDpy, p.sup, p.sup, gCtx) == EGL_TRUE;
            if (!ok) w3dLogf("[pantallas] el contexto no acepta la superficie que conserva el cuadro (0x%x)", eglGetError());
            eglMakeCurrent(gDpy, gPrincipal, gPrincipal, gCtx);
            if (!ok) { eglDestroySurface(gDpy, p.sup); p.sup = EGL_NO_SURFACE; conConserva = EFalse; k--; continue; }
        }
        if (p.sup != EGL_NO_SURFACE) break;
        if (conConserva) {
            w3dLogf("[pantallas] la superficie de %dx%d que conserva el cuadro no entro (0x%x): la de siempre", p.w, p.h, eglGetError());
            conConserva = EFalse; k--; continue;
        }
        const EGLint err = eglGetError();
        w3dLogf("[pantallas] eglCreateWindowSurface de %dx%d fallo en la pantalla %d (error 0x%x, modo de color %d)%s",
                p.w, p.h, id, err, (TInt)p.ventana->Ventana().DisplayMode(),
                err == EGL_BAD_ALLOC ? ": sin memoria grafica (otra app abierta?)" : "");
        if (err != EGL_BAD_ALLOC) break;
    }
    if (p.sup == EGL_NO_SURFACE) {
        p.w = w0; p.h = h0;
        SoltarSuperficie(p);
        return false;
    }
    w3dLogf("[pantallas] superficie de %dx%d creada en la pantalla %d (%dx%d)", p.w, p.h, id, p.pantW, p.pantH);
#ifdef W3D_GLES2
    // que el swap CONSERVE el cuadro: pasar el mouse sin cambiar nada redibuja solo el cursor (W3dMouseCuadroCursor),
    // no 1280x720 de editor. La config se eligio con EGL_SWAP_BEHAVIOR_PRESERVED_BIT si el driver la tenia
    p.conserva = conConserva &&
                 eglSurfaceAttrib(gDpy, p.sup, 0x3093 /*EGL_SWAP_BEHAVIOR*/, 0x3094 /*EGL_BUFFER_PRESERVED*/) == EGL_TRUE;
    w3dLogf("[pantallas] el swap de la pantalla %d %s el cuadro", id, p.conserva ? "CONSERVA" : "no conserva");
    if (!p.conserva) eglGetError();
#endif
    if (p.w != w0) PantallaExternaConectada(id, p.w, p.h, true);   // (el arbol de la pantalla, a lo que entro)
    return true;
}

static bool Activar(int id) {
    if (id <= 0 || id >= KMaxPantallas || gP[id].sup == EGL_NO_SURFACE) return false;
    if (eglMakeCurrent(gDpy, gP[id].sup, gP[id].sup, gCtx) == EGL_FALSE) {
        static TBool avisado = EFalse;   // (una vez: esto corre cada cuadro)
        if (!avisado) { avisado = ETrue; w3dLogf("[pantallas] eglMakeCurrent fallo en la pantalla %d (0x%x)", id, eglGetError()); }
        eglMakeCurrent(gDpy, gPrincipal, gPrincipal, gCtx);
        return false;
    }
    return true;
}

static void Presentar(int id) {
    if (id > 0 && id < KMaxPantallas && gP[id].sup != EGL_NO_SURFACE) eglSwapBuffers(gDpy, gP[id].sup);
}

TBool W3dPantallaConservaCuadro(TInt aId) {
    return aId > 0 && aId < KMaxPantallas && gP[aId].sup != EGL_NO_SURFACE && gP[aId].conserva;
}

static void Volver() { eglMakeCurrent(gDpy, gPrincipal, gPrincipal, gCtx); }

void W3dPantallasInit(EGLDisplay aDisplay, EGLConfig aConfig, EGLContext aContext, EGLSurface aPrincipal) {
    gDpy = aDisplay; gCfg = aConfig; gCtx = aContext; gPrincipal = aPrincipal;
    for (TInt i = 0; i < KMaxPantallas; i++) {
        gP[i].conectada = EFalse; gP[i].w = gP[i].h = gP[i].pantW = gP[i].pantH = 0;
        gP[i].dev = NULL; gP[i].grupo = NULL; gP[i].ventana = NULL; gP[i].sup = EGL_NO_SURFACE; gP[i].conserva = EFalse;
    }
    PantallaExternaPreparar = Preparar;
    PantallaExternaActivar = Activar;
    PantallaExternaPresentar = Presentar;
    PantallaExternaVolver = Volver;
#ifdef W3D_GLES2
    TRAPD(err, gAcc = CAccMonitor::NewL());
    if (err != KErrNone) { gAcc = NULL; w3dLogf("[pantallas] AccMonitor no disponible (err=%d)", err); }
#endif
    gUltimoTick = 0;
    W3dPantallasTick();   // (una pantalla ya enchufada al abrir la app tambien pregunta)
}

void W3dPantallasTick() {
    const TUint ahora = User::NTickCount();
    if (gUltimoTick != 0 && ahora - gUltimoTick < 1000) return;
    gUltimoTick = ahora;
    RWsSession& ws = CCoeEnv::Static()->WsSession();
    TInt n = NumeroDePantallas(ws);
    if (n > KMaxPantallas) n = KMaxPantallas;
    const TInt video = CableDeVideo();
    if (n != gUltimoNumero || video != gUltimoVideo) {
        w3dLogf("[pantallas] wserv expone %d pantalla(s); cable de video: %s", n,
                video < 0 ? "no se sabe" : (video ? "SI" : "no"));
        gUltimoNumero = n; gUltimoVideo = video;
    }
    for (TInt i = 1; i < KMaxPantallas; i++) {
        TPantallaSym& p = gP[i];
        TSize tam(0, 0);
        if (i < n) {
            if (!p.dev) {
                p.dev = new CWsScreenDevice(ws);
                if (p.dev && ConstruirDispositivo(p.dev, i) != KErrNone) { delete p.dev; p.dev = NULL; }
            }
            if (p.dev) tam = p.dev->SizeInPixels();
        }
        // conectada = wserv la expone CON tamano y (si se puede saber) hay un cable de video: el N8 puede tener la
        // pantalla 1 configurada siempre, aunque no haya nada enchufado
        const TBool hay = tam.iWidth > 0 && tam.iHeight > 0 && video != 0;
        if (hay && (!p.conectada || tam.iWidth != p.pantW || tam.iHeight != p.pantH)) {
            const TBool nueva = !p.conectada;
            p.pantW = tam.iWidth; p.pantH = tam.iHeight;
            p.w = p.pantW; p.h = p.pantH;
#ifndef W3D_GLES2
            if (p.w > KMaxAnchoExt) p.w = KMaxAnchoExt;
            if (p.h > KMaxAltoExt) p.h = KMaxAltoExt;
#endif
            p.conectada = ETrue;
            if (!nueva && p.sup != EGL_NO_SURFACE) {   // cambio la resolucion: la ventana se re-acomoda
                p.ventana->SetRect(TRect(TPoint((p.pantW - p.w) / 2, (p.pantH - p.h) / 2), TSize(p.w, p.h)));
            }
            PantallaExternaConectada(i, p.w, p.h, true);
        } else if (!hay && p.conectada) {
            PantallaExternaDesconectada(i);   // (el editor llama a Preparar(clonar): suelta la superficie)
            SoltarPantalla(p);
        } else if (!hay && p.dev && i >= n) {
            SoltarPantalla(p);
        }
    }
}

void W3dPantallasDestroy() {
    for (TInt i = 1; i < KMaxPantallas; i++) {
        if (gP[i].conectada) PantallaExternaDesconectada(i);
        SoltarPantalla(gP[i]);
    }
    PantallaExternaPreparar = 0; PantallaExternaActivar = 0; PantallaExternaPresentar = 0; PantallaExternaVolver = 0;
    SoltarApiPantallas();
#ifdef W3D_GLES2
    delete gAcc;
    gAcc = NULL;
#endif
}
