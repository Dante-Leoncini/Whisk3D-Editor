/*
 * ==============================================================================
 *  Name        : Whisk3DContainer.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDE FILES
#include <eiklabel.h>
#include "ViewPorts/Timeline.h"  // DopeNavKey: el lapiz sobre el timeline navega keyframes
#include <e32math.h>

#include "Whisk3DContainer.h"
#include "Whisk3DAppUi.h"
#include "whisk3D.hrh"
#include "w3dlog.h" // log de diagnostico E:\whisk3d.log (modo dev)
#include "w3dlayout.h" // layout compartido (Fase 1 unificacion)
#include "w3dt9.h"     // entrada de texto multi-tap (T9) del keypad
#include "w3dnewscene.h"
#include <GLES/gl.h> // modelo compartido de PC (Fase 3c): init+render
#include "render/RenderOptions.h"     // g_redraw (render event-driven)
#include "objects/Materials.h"        // HasAnimationActive / UpdateAnimatedMaterials
#include "animation/Animation.h"      // PlayAnimation / AnimFPS / AnimTick (avance del play)
#include "w3dpantallas.h"             // pantallas externas (TV-out / HDMI): deteccion + superficie propia
#include "ViewPorts/ExternalScreens.h" // ...y su dibujo (el popup y los viewports son del editor)
#include "w3dGraphics.h"               // g_statFrame: las llamadas GL del ultimo cuadro en el [PERF]
#include <string.h>                    // strlen/memcpy: el volcado de capacidades GL
#include <hal.h>                       // HAL EPen: el telefono es tactil
#include <math.h>                      // atan2 / asin: el giro del telefono (inclinar)
#include <stdio.h>                     // sprintf (la captura de diagnostico)
#include "w3dTexture.h"                // SavePNG (idem)
#include "input/W3dTouch.h"            // la pantalla tactil (Core): los mismos eventos que SDL en Android
#include "app/variables.h"             // cfg (la barra de herramientas en los tactiles)
#include "objects/Gamepad.h"           // GamepadsSymbianTick: los joysticks Bluetooth
#include "app/W3dMouseEditor.h"        // W3dMouseTick: el mouse Bluetooth (boton del medio emulado)
#include "input/W3dGamepads.h"            // GamepadsClose
void ControlsTick();                  // (controls.cpp) la pulsacion larga tactil, por reloj
bool LayoutToggleEditMode();            // (LayoutInput.cpp) Tab: entrar/salir del modo edicion
void LayoutCycleActiveViewport(int dir);

// BARRA DE PROGRESO (export/import OBJ): el hook de swap que faltaba en Symbian. ProgressStart/Actualizar lo
// llaman para mostrar la barra DURANTE la operacion bloqueante (sin esto, ProgressStart hacia return y la
// barra/notificacion no aparecian en el N95). Guarda los handles EGL del surface activo (seteados en ConstructL).
extern void (*LayoutSwapBuffers)();
static EGLDisplay gSwapDisplay = NULL;
static EGLSurface gSwapSurface = NULL;
// (antes del swap, lo que el motor tiene JUNTADO sin dibujar: el swap es EGL directo y no lo manda. Como PCSwapBuffers)
// MIENTRAS UN RENDER LARGO BLOQUEA (la barra de progreso): una espera anidada CORTA deja llegar las teclas pendientes a
// OfferKeyEventL (la derecha, la C o Escape cancelan; el resto se descarta) y vuelve sola con un CIdle (prioridad
// minima: corre despues de todo lo pendiente). El DrawCallBack no dibuja mientras haya barra
static TInt ProgressStop(TAny* p) { CActiveSchedulerWait* w = (CActiveSchedulerWait*)p; if (w->IsStarted()) w->AsyncStop(); return 0; }
static void ProgressPollSymbian() {
    static TBool inside = EFalse;
    if (inside) return;
    inside = ETrue;
    CActiveSchedulerWait* w = new CActiveSchedulerWait;
    CIdle* idle = w ? CIdle::New(CActive::EPriorityIdle) : NULL;
    if (w && idle) { idle->Start(TCallBack(ProgressStop, w)); w->Start(); }
    delete idle; delete w;
    inside = EFalse;
}
static void W3dProgressSwap() { w3dEngine::DrawPending(); if (gSwapDisplay) eglSwapBuffers(gSwapDisplay, gSwapSurface); }

// ---- DIBUJAR FUERA DE LA PANTALLA (render/RenderOptions.h): un PBUFFER del tamanio mas grande que se haya pedido. En el
//      N95 lo dibujado en la ventana se veia antes del swap: los colores planos del pase de las sombras y las franjas del
//      render a archivo parpadeaban en pantalla. Anidable: solo el primero cambia de superficie y vuelve a la que habia ----
static EGLConfig  gOutsideCfg = 0;
static EGLContext gOutsideCtx = EGL_NO_CONTEXT;
static EGLSurface gOutsidePb = EGL_NO_SURFACE, gOutsideReturn = EGL_NO_SURFACE;
static TInt gOutsideW = 0, gOutsideH = 0, gOutsideLevel = 0;
static TBool gOutsideServes = EFalse;
static bool OutsideBegin(int w, int h) {
    if (gOutsideLevel > 0) { gOutsideLevel++; return true; }
    if (!gOutsideServes || !gSwapDisplay || w <= 0 || h <= 0) return false;
    if (gOutsidePb == EGL_NO_SURFACE || w > gOutsideW || h > gOutsideH) {
        if (gOutsidePb != EGL_NO_SURFACE) { eglDestroySurface(gSwapDisplay, gOutsidePb); gOutsidePb = EGL_NO_SURFACE; }
        const TInt nw = w > gOutsideW ? w : gOutsideW, nh = h > gOutsideH ? h : gOutsideH;
        const EGLint at[] = { EGL_WIDTH, nw, EGL_HEIGHT, nh, EGL_NONE };
        gOutsidePb = eglCreatePbufferSurface(gSwapDisplay, gOutsideCfg, at);
        if (gOutsidePb == EGL_NO_SURFACE) {
            w3dLogf("[fuera] no se pudo crear el pbuffer de %dx%d (0x%x): se dibuja en la ventana", nw, nh, (TInt)eglGetError());
            gOutsideServes = EFalse; return false;
        }
        gOutsideW = nw; gOutsideH = nh;
        w3dLogf("[fuera] pbuffer de %dx%d", nw, nh);
    }
    w3dEngine::DrawPending();
    gOutsideReturn = eglGetCurrentSurface(EGL_DRAW);
    if (eglMakeCurrent(gSwapDisplay, gOutsidePb, gOutsidePb, gOutsideCtx) != EGL_TRUE) {
        w3dLogf("[fuera] eglMakeCurrent al pbuffer fallo (0x%x): se dibuja en la ventana", (TInt)eglGetError());
        gOutsideServes = EFalse; return false;
    }
    gOutsideLevel = 1;
    return true;
}
static void OutsideFinish() {
    if (gOutsideLevel <= 0) return;
    if (--gOutsideLevel > 0) return;
    w3dEngine::DrawPending();
    eglMakeCurrent(gSwapDisplay, gOutsideReturn, gOutsideReturn, gOutsideCtx);
}

// ---- LAS TECLAS DE VOLUMEN (w3dvolumen.dll, RemCon): un paso de 10% por toque, con el aviso del porcentaje ----------
namespace w3dEngine { void VolumeSet(float v); float VolumeGet(); }
extern void Notify(const std::string& msg, bool error);
static void VolumeStep(int dir) {
    float v = w3dEngine::VolumeGet() + (dir > 0 ? 0.1f : -0.1f);
    if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
    w3dEngine::VolumeSet(v);
    char b[48]; sprintf(b, "Volume %d%%", (int)(v * 100.0f + 0.5f));
    Notify(b, false);
    g_redraw = true;
}
static void W3dVolumeKeysOpen() {
    static RLibrary lib; static TBool done = EFalse;
    if (done) return;
    done = ETrue;
    const TInt r = lib.Load(_L("w3dvolumen.dll"));
    if (r != KErrNone) { w3dLogf("[volumen] w3dvolumen.dll no cargo (err %d): sin las teclas de volumen", r); return; }
    struct Table { int version; void* (*Open)(void (*)(int)); void (*Close)(void*); };
    typedef void (*Fn)(Table*);
    Fn f = (Fn)lib.Lookup(1);
    Table t; Mem::FillZ(&t, sizeof(t));
    if (f) f(&t);
    if (t.version != 1 || !t.Open || !t.Open(VolumeStep)) { w3dLog("[volumen] no se pudo escuchar las teclas de volumen"); return; }
    w3dLog("[volumen] las teclas de volumen del costado manejan el volumen de Whisk3D");
}

// ---- LO QUE EXPONE EL DRIVER GL/EGL DEL TELEFONO (diagnostico, una vez al arrancar) ----------------------------
// Para decidir que caminos de render sirven en cada equipo: las extensiones (programas de vertices, paleta de
// matrices, multi-draw...), los limites, y si eglGetProcAddress encuentra funciones que el driver no anuncia.
// El log corta cada linea a ~200 caracteres: los textos largos van en tramos.
static void LogGlLength(const char* title, const char* txt){
    if (!txt) { w3dLogf("[gl] %s: (nada)", title); return; }
    const int n = (int)strlen(txt);
    int i = 0, part = 1;
    while (i < n) {
        int j = i + 150;
        if (j >= n) j = n;
        else { int k = j; while (k > i && txt[k] != ' ') k--; if (k > i) j = k; }
        char buf[192];
        int len = j - i; if (len > 190) len = 190;
        memcpy(buf, txt + i, len); buf[len] = 0;
        w3dLogf("[gl] %s %d: %s", title, part++, buf);
        i = j;
        while (i < n && txt[i] == ' ') i++;
    }
}
static void DumpCapacitiesGL(EGLDisplay d){
    LogGlLength("GL_VENDOR", (const char*)glGetString(GL_VENDOR));
    LogGlLength("GL_RENDERER", (const char*)glGetString(GL_RENDERER));
    LogGlLength("GL_VERSION", (const char*)glGetString(GL_VERSION));
    LogGlLength("GL_EXTENSIONS", (const char*)glGetString(GL_EXTENSIONS));
    LogGlLength("EGL_VENDOR", eglQueryString(d, EGL_VENDOR));
    LogGlLength("EGL_VERSION", eglQueryString(d, EGL_VERSION));
    LogGlLength("EGL_EXTENSIONS", eglQueryString(d, EGL_EXTENSIONS));
    // limites (los de las extensiones por su valor: el SDK puede no traer glext.h)
    struct Lim { GLenum e; const char* n; };
    const Lim lim[] = {
        { GL_MAX_LIGHTS, "GL_MAX_LIGHTS" }, { GL_MAX_TEXTURE_UNITS, "GL_MAX_TEXTURE_UNITS" },
        { GL_MAX_TEXTURE_SIZE, "GL_MAX_TEXTURE_SIZE" }, { GL_MAX_MODELVIEW_STACK_DEPTH, "GL_MAX_MODELVIEW_STACK_DEPTH" },
        { 0x8842, "GL_MAX_PALETTE_MATRICES_OES" }, { 0x86A4, "GL_MAX_VERTEX_UNITS_OES" }
    };
    for (unsigned k = 0; k < sizeof(lim) / sizeof(lim[0]); k++) {
        while (glGetError() != GL_NO_ERROR) {}
        GLint v[4] = { -1, -1, -1, -1 };
        glGetIntegerv(lim[k].e, v);
        const GLenum err = glGetError();
        w3dLogf("[gl] %s = %d%s", lim[k].n, (int)v[0], err != GL_NO_ERROR ? " (el driver no lo conoce)" : "");
    }
    // funciones que podrian estar aunque no se anuncien: programas de vertices (IMG_vertex_program, basado en
    // ARB_vertex_program), la paleta de matrices, multi-draw, draw_texture, clip planes de IMG, map buffer
    const char* fns[] = {
        "glProgramStringARB", "glBindProgramARB", "glGenProgramsARB", "glProgramLocalParameter4fARB",
        "glProgramEnvParameter4fARB", "glVertexAttribPointerARB", "glEnableVertexAttribArrayARB",
        "glCurrentPaletteMatrixOES", "glLoadPaletteFromModelViewMatrixOES", "glMatrixIndexPointerOES",
        "glWeightPointerOES", "glMultiDrawArraysEXT", "glMultiDrawElementsEXT", "glDrawTexiOES",
        "glClipPlanefIMG", "glMapBufferOES", "glGenFramebuffersOES", "glQueryMatrixxOES"
    };
    for (unsigned k = 0; k < sizeof(fns) / sizeof(fns[0]); k++)
        w3dLogf("[gl] eglGetProcAddress(%s): %s", fns[k], eglGetProcAddress(fns[k]) != NULL ? "SI" : "no");
    while (glGetError() != GL_NO_ERROR) {}
}


// ================= MEMBER FUNCTIONS =======================
// ---------------------------------------------------------
// CWhisk3DContainer::ConstructL
// Symbian 2nd phase constructor
// ---------------------------------------------------------
//
void CWhisk3DContainer::ConstructL(const TRect& /*aRect*/){
    // el log arranca ACA (antes de EGL y del compilado de shaders): si el arranque de GL falla, queda escrito
    w3dLogReset();
    w3dLog("ConstructL: inicio");
    CreateWindowL();
    // el FONDO de la ventana en NEGRO (el de fabrica es BLANCO): cuando el sistema la invalida -mientras se renderiza,
    // esperando al codificador de video- el servidor de ventanas la limpia con este color antes de que GL vuelva a
    // dibujar. En blanco eran los parpadeos de la barra de progreso (que limpia en negro)
    Window().SetBackgroundColor(KRgbBlack);
    iOpenGlInitialized = EFalse;
    iReleaseFingers = EFalse;
    // Create the input handler
    iInputHandler = CWhisk3DInput::NewL();

    SetExtentToWholeScreen();                // Take the whole screen into use
    // PANTALLA TACTIL: arrastrar el dedo (sin esto solo llegan el down y el up). En Symbian^3, ademas, un evento
    // por cada dedo (multitactil); tiene que pedirse ANTES de activar la ventana. (Sin tactil, el N95, no llega nada.)
    EnableDragEvents();
#ifdef W3D_GLES2
    Window().EnableAdvancedPointers();
#endif
    ActivateL();
    
    iFrame = 0;                              // Frame counter

    EGLConfig Config;                        // Describes the format, type and
                                             // size of the color buffers and
                                             // ancillary buffers for EGLSurface

    // Get the display for drawing graphics
    iEglDisplay = eglGetDisplay( EGL_DEFAULT_DISPLAY );
    if ( iEglDisplay == NULL ){
        _LIT(KGetDisplayFailed, "eglGetDisplay failed");
        User::Panic( KGetDisplayFailed, 0 );
    }

    // Initialize display
    if ( eglInitialize( iEglDisplay, NULL, NULL ) == EGL_FALSE ){
        _LIT(KInitializeFailed, "eglInitialize failed");
        User::Panic( KInitializeFailed, 0 );
    }
#ifdef W3D_GLES2
    // variante ES 2.0 (N8): la API de EGL tiene que ser OpenGL ES (lo es por defecto, pero el driver de Symbian^3
    // tambien trae OpenVG y no conviene dejarlo librado al azar)
    eglBindAPI( EGL_OPENGL_ES_API );
#endif

    EGLConfig *configList = NULL;            // Pointer for EGLConfigs
    EGLint numOfConfigs = 0;
    EGLint configSize   = 0;

    // Get the number of possible EGLConfigs
    if ( eglGetConfigs( iEglDisplay, configList, configSize, &numOfConfigs )
        == EGL_FALSE )
        {
        _LIT(KGetConfigsFailed, "eglGetConfigs failed");
        User::Panic( KGetConfigsFailed, 0 );
        }

    configSize = numOfConfigs;

    // Allocate memory for the configList
    configList = (EGLConfig*) User::Alloc( sizeof(EGLConfig)*configSize );
    if ( configList == NULL )
        {
        _LIT(KConfigAllocFailed, "config alloc failed");
        User::Panic( KConfigAllocFailed, 0 );
        }

    /* Define properties for the wanted EGLSurface.
       To get the best possible performance, choose
       an EGLConfig with a buffersize matching
       the current window's display mode*/
    TDisplayMode DMode = Window().DisplayMode();
    TInt BufferSize = 0;

    switch ( DMode ){
         case(EColor4K):
             BufferSize = 12;
             break;
         case(EColor64K):
             BufferSize = 16;
             break;
         case(EColor16M):
             BufferSize = 24;
             break;
         case(EColor16MU):
         case(EColor16MA):
             BufferSize = 32;
             break;
         default:
             _LIT(KDModeError,       "unsupported displaymode");
             User::Panic( KDModeError, 0 );
             break;
    }

    // Define properties for requesting a full-screen antialiased window surface
    const EGLint attrib_list_fsaa[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BUFFER_SIZE,  BufferSize,
        EGL_DEPTH_SIZE,   16,
        //EGL_RENDER_BUFFER, EGL_BACK_BUFFER, // Habilita doble buffer
				EGL_SAMPLE_BUFFERS, 1,
				EGL_SAMPLES,        4,//antialiasing
        EGL_NONE
    };

    const EGLint attrib_list_sinfsaa[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BUFFER_SIZE, BufferSize,
        EGL_DEPTH_SIZE, 16,
        //EGL_RENDER_BUFFER, EGL_BACK_BUFFER, // Habilita doble buffer
        EGL_SAMPLE_BUFFERS, 0,  // Desactiva el antialiasing
        EGL_SAMPLES, 0,         // Desactiva el antialiasing
        EGL_NONE
    };

    // Define properties for requesting a non-antialiased window surface
#ifdef W3D_GLES2
    // primero una config cuyo swap pueda CONSERVAR el cuadro (EGL_SWAP_BEHAVIOR_PRESERVED_BIT): la pantalla externa en
    // "solo la externa" mueve el cursor sin redibujar 1280x720 de editor. Si el driver no tiene, la de siempre
    const EGLint attrib_list_keeps[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | 0x0400 /*EGL_SWAP_BEHAVIOR_PRESERVED_BIT*/,
        EGL_BUFFER_SIZE,  BufferSize,
        EGL_DEPTH_SIZE,   16,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
#endif
    const EGLint attrib_list[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BUFFER_SIZE,  BufferSize,
        // (16 tambien en el N8: con 24 la superficie de la TV de 1280x720 no entraba en la memoria del VideoCore III,
        // EGL_BAD_ALLOC al elegir "nuevo viewport")
        EGL_DEPTH_SIZE,   16,
#ifdef W3D_GLES2
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,   // solo configs que sirven para un contexto ES 2.0
#endif
        EGL_NONE
    };

    // Choose an EGLConfig that best matches to the properties in attrib_list_fsaa
    //if ( eglChooseConfig( iEglDisplay, attrib_list_fsaa, configList, configSize, &numOfConfigs ) == EGL_FALSE ){
    numOfConfigs = 0;
#ifdef W3D_GLES2
    // (solo para la superficie de la TV: el telefono y el contexto van con la de siempre. Con esta config TODAS las
    // superficies pedian mas memoria grafica y la de 1280x720 de la TV ya no entraba: quedaba a 960x540 con bordes)
    if ( eglChooseConfig( iEglDisplay, attrib_list_keeps, configList, configSize, &numOfConfigs ) == EGL_FALSE ){
        numOfConfigs = 0;
    }
    w3dLogf( "EGL: config que conserva el cuadro en el swap: %s", numOfConfigs > 0 ? "SI" : "no" );
    if ( numOfConfigs > 0 ) W3dScreensConfigKeeps( configList[0] );
    numOfConfigs = 0;
#endif
    // primero una que sirva para la ventana Y para un PBUFFER (lo que se dibuja para leerlo y no se muestra: ver
    // OutsideBegin); si el driver no tiene, la de siempre
    {
        EGLint withPb[sizeof(attrib_list) / sizeof(attrib_list[0])];
        for (TUint k = 0; k < sizeof(attrib_list) / sizeof(attrib_list[0]); k++) withPb[k] = attrib_list[k];
        withPb[1] = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
        if ( eglChooseConfig( iEglDisplay, withPb, configList, configSize, &numOfConfigs ) == EGL_FALSE ) numOfConfigs = 0;
        gOutsideServes = (numOfConfigs > 0) ? ETrue : EFalse;
        w3dLogf( "EGL: config para ventana y pbuffer: %s", gOutsideServes ? "SI" : "no (lo de las sombras se dibuja en la ventana)" );
    }
    if ( numOfConfigs == 0 && eglChooseConfig( iEglDisplay, attrib_list, configList, configSize, &numOfConfigs ) == EGL_FALSE ){
        _LIT( KChooseConfigFailed, "eglChooseConfig failed" );
        User::Panic( KChooseConfigFailed, 0 );
    }

    // Check if no configurations were found
    if ( numOfConfigs == 0 ){
        // No configs with antialising were found. Try to get the non-antialiased config
		if ( eglChooseConfig( iEglDisplay, attrib_list, configList, configSize, &numOfConfigs ) == EGL_FALSE ){
		  _LIT( KChooseConfigFailed, "eglChooseConfig failed" );
		  User::Panic( KChooseConfigFailed, 0 );
			  }

		if ( numOfConfigs == 0 ){
	      // No configs found without antialiasing
		  _LIT( KNoConfig, "Can't find the requested config." );
		  User::Panic( KNoConfig, 0 );
		}
	}

    Config = configList[0];   // Choose the best EGLConfig. EGLConfigs
                              // returned by eglChooseConfig are sorted so
                              // that the best matching EGLConfig is first in
                              // the list.
    User::Free( configList ); // Free configList, not used anymore.

    // Create a window where the graphics are blitted
    iEglSurface = eglCreateWindowSurface( iEglDisplay, Config, &Window(), NULL );
    // SIN MEMORIA GRAFICA (EGL_BAD_ALLOC): en Symbian^3 la comparten todas las apps y una que quedo abierta en segundo
    // plano (un juego) puede tenerla tomada; el sistema la libera cerrando apps de fondo. Se reintenta unos segundos
    // antes de rendirse, y queda en el log el porque
    for ( TInt attempt = 0; iEglSurface == NULL && attempt < 10; attempt++ )
        {
        const EGLint err = eglGetError();
        w3dLogf( "EGL: eglCreateWindowSurface fallo (0x%x%s), reintento %d", (TInt)err,
                 err == EGL_BAD_ALLOC ? ": sin memoria grafica, hay otra app abierta?" : "", (TInt)( attempt + 1 ) );
        User::After( 500000 );
        iEglSurface = eglCreateWindowSurface( iEglDisplay, Config, &Window(), NULL );
        }
    if ( iEglSurface == NULL )
        {
        _LIT(KCreateWindowSurfaceFailed, "eglCreateWindowSurface failed");
        User::Panic( KCreateWindowSurfaceFailed, 0 );
        }

    // Create a rendering context
#ifdef W3D_GLES2
    const EGLint attributesContext[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    iEglContext = eglCreateContext( iEglDisplay, Config, EGL_NO_CONTEXT, attributesContext );
#else
    iEglContext = eglCreateContext( iEglDisplay, Config, EGL_NO_CONTEXT, NULL );
#endif
    iEglConfig = Config;   // (la segunda pantalla crea su superficie con la misma config: comparte el contexto)
    {   // la config que dio el driver (color, profundidad, multisample)
        EGLint buf = 0, prof = 0, mues = 0;
        eglGetConfigAttrib(iEglDisplay, Config, EGL_BUFFER_SIZE, &buf);
        eglGetConfigAttrib(iEglDisplay, Config, EGL_DEPTH_SIZE, &prof);
        eglGetConfigAttrib(iEglDisplay, Config, EGL_SAMPLES, &mues);
        w3dLogf("EGL: config de %d bits de color, %d de profundidad, %d muestras", (TInt)buf, (TInt)prof, (TInt)mues);
    }
    if ( iEglContext == NULL )
        {
        _LIT(KCreateContextFailed, "eglCreateContext failed");
        User::Panic( KCreateContextFailed, 0 );
        }

    /* Make the context current. Binds context to the current rendering thread
       and surface.*/
    if ( eglMakeCurrent( iEglDisplay, iEglSurface, iEglSurface, iEglContext )
        == EGL_FALSE )
        {
        _LIT(KMakeCurrentFailed, "eglMakeCurrent failed");
        User::Panic( KMakeCurrentFailed, 0 );
        }

    // BARRA DE PROGRESO: cablear el swap de Symbian (faltaba) -> la barra de export/import ya puede dibujarse.
    gSwapDisplay = iEglDisplay;
    gSwapSurface = iEglSurface;
    // lo que se dibuja para leerlo (las sombras, las franjas del render): en un pbuffer, si la config sirve para eso
    gOutsideCfg = Config; gOutsideCtx = iEglContext;
    if (gOutsideServes) { W3dOutsideScreenBegin = OutsideBegin; W3dOutsideScreenFinish = OutsideFinish; }
    LayoutSwapBuffers = W3dProgressSwap;
    { extern void (*W3dProgressPoll)(); W3dProgressPoll = ProgressPollSymbian; }   // (Cancelar en la barra)

#ifdef W3D_GLES2
    // backend de shaders: compila el uber-shader y arma los VBOs. TIENE que ir antes de AppInit (que ya dibuja
    // por la abstraccion), igual que en PC/Android. En ES 2.0 nativo no hay funciones que cargar (NULL).
    w3dLog("GLES2Init: compilando los shaders");
    w3dEngine::GLES2Init( NULL );
    w3dLogf("GLES2Init: listo (glErr=%x)", (TInt)glGetError());
#endif


    TSize size;
    size = this->Size();

    // telefono TACTIL (5800, N97, N8...): la barra de herramientas del 3D, como en Android. Con el dedo es la unica
    // forma de aceptar/cancelar un transform (tocar la pantalla lo MUEVE, no lo confirma). El N95 sigue sin ella.
    {
        TInt pencil = 0;
        if (HAL::Get(HALData::EPen, pencil) == KErrNone && pencil) {
            cfg.newUser = true;
            extern bool g_gizmoOn; g_gizmoOn = true;   // (y el gizmo, como Android: con el dedo se agarra)
        }
    }

    iWhisk3D = CWhisk3D::NewL( size.iWidth, size.iHeight, iInputHandler ); // Create an instance of Whisk3D
    iWhisk3D->AppInit();                                       // Initialize OpenGL ES
    DumpCapacitiesGL( iEglDisplay );   // (despues de AppInit: ahi arranca el log) que expone este driver

    // Fase 1 unificacion: arbol de viewports compartido (core/ui). Es el
    // equivalente al rootViewport->Resize(winW, winH) del main.cpp de PC:
    // horizontal = 2 columnas (3D | propiedades), vertical = 2 filas.
    W3dLayoutBuild( iWhisk3D, size.iWidth, size.iHeight );

    // Fase 3c: raiz del modelo compartido de PC + cubo default (como
    // ConstructUniversal). Su render sale en cada frame del viewport 3D.
    W3dNewSceneInit();
    { extern void W3dVideoEncPoll(); W3dVideoEncPoll(); }   // (al log: los codificadores de video del telefono)
    // LAS TECLAS DE VOLUMEN del costado: el volumen de Whisk3D (llegan por RemCon, desde w3dvolumen.dll)
    W3dVolumeKeysOpen();

    // PANTALLAS EXTERNAS (TV-out RCA, HDMI): al conectarse una, el popup "clonar / nuevo viewport / propiedades".
    // Despues del layout: el popup se centra con el tamano de pantalla que arma W3dLayoutBuild.
    W3dScreensInit( iEglDisplay, iEglConfig, iEglContext, iEglSurface );

    // Mouse/teclado bluetooth (HID). hidsrv.dll se carga en runtime: si el
    // telefono no tiene el driver, NewL deja y seguimos sin mouse (por eso
    // el TRAPD: un Leave aca abortaria la construccion de la app entera).
    iHidMonitor = NULL;
    TRAPD(hidErr, iHidMonitor = CHidMonitor::NewL(*iWhisk3D));
    if (hidErr != KErrNone)
        {
        w3dLogf("HidMonitor: no disponible (err=%d), seguimos sin mouse BT", hidErr);
        }

    iOpenGlInitialized = ETrue;

    // Prioridad MUY por debajo de EPriorityIdle (-100): este timer dispara
    // cada 100us y esta siempre listo, y el active scheduler ante igual
    // prioridad elige el AO mas viejo de la lista -> con EPriorityIdle el
    // render le ganaba SIEMPRE a los AOs del decoder de imagenes (ICL) y la
    // carga de texturas de un OBJ quedaba colgada para siempre (la app
    // clavada en ELoadingTextures). Con -200 cualquier AO del sistema o del
    // decoder que este listo pasa primero por construccion, y el render usa
    // todo el tiempo restante (no se nota diferencia de fps).
    iPeriodic = CPeriodic::NewL( -200 );                          // Create an active object for
                                                                  // animating the scene
    iPeriodic->Start( 100, 100,
                      TCallBack( CWhisk3DContainer::DrawCallBack, this ) );
}

// ------------------------------------------------------------------------------
// CBillboardContainer::OfferKeyEventL()
// Handles joystick movements.
// ------------------------------------------------------------------------------
//
// flechas MANTENIDAS: el loop de frame (DrawCallBack) las aplica CADA frame
// para que orbitar/mover/rotar/escalar con el teclado sea FLUIDO. El auto-
// repeat del telefono es lentisimo (se sentia a 1fps), por eso no se usa.
static TBool gHeldUp = EFalse, gHeldDown = EFalse, gHeldLeft = EFalse, gHeldRight = EFalse;
// tecla VERDE mantenida: + flechas = agrandar/achicar el viewport activo.
// gUsedGreen = se uso como modificador (entonces al soltar NO cicla viewport).
static TBool gGreenHeld = EFalse, gUsedGreen = EFalse;
// EL OK MANTENIDO = el menu CONTEXTUAL (como mantener el dedo en un tactil, o el click derecho de la PC). Sobre algo que
// tiene menu -una fila de Propiedades, del Outliner o de la Biblioteca, los keyframes del Timeline, el objeto activo en
// la Vista 3D- el OK no hace nada al bajar: soltarlo enseguida es el OK de siempre (al soltar); mantenerlo, el telefono
// VIBRA y cuando termina de vibrar sale el menu del elemento con foco. Con el MOUSE VIRTUAL el OK hace su click al bajar
// (como siempre) y, si se mantiene sin mover el cursor, sale el menu ahi (el click derecho).
static TBool gOkWaiting = EFalse;     // el OK esta apretado y todavia no se decidio (corto o largo)
static TBool gOkWithMouse = EFalse;   // ...con el mouse virtual (el click ya se hizo)
static TBool gOkVibrating = EFalse;
static TBool gOkLongDone = EFalse;  // ya salio el menu: soltar el OK no hace nada mas
static TUint gOkTick = 0;
static TInt  gOkMx = 0, gOkMy = 0;    // (con el mouse virtual: donde estaba el cursor)
static const TUint kOkVibrateMs = 150, kOkLongMs = 500;   // (los de la pulsacion larga del tactil)
static TUint OkTicksToMs(TUint t) {
#ifdef __WINS__
    return t * 5;   // (el emulador cuenta a 200 Hz)
#else
    return t;       // (el N95 a 1000 Hz: ms)
#endif
}
extern bool LayoutMenuContextKeyboardPossible();
extern bool LayoutMenuContextKeyboard();
extern bool LayoutMenuContextIn(int mx, int my);
namespace w3dEngine { void VibrateMs(int ms, float force); }
// (cada cuadro, DrawCallBack): vibra mientras espera y, al cumplirse el tiempo, abre el menu
static void OkLongTick() {
    if (!gOkWaiting) return;
    extern bool mouseVisible;
    extern GLshort mouseX, mouseY;
    // con el mouse virtual, mover el cursor (un arrastre) lo cancela: queda el click que ya se hizo
    if (gOkWithMouse && (!mouseVisible || mouseX != gOkMx || mouseY != gOkMy)) { gOkWaiting = EFalse; return; }
    const TUint ms = OkTicksToMs(User::NTickCount() - gOkTick);
    if (!gOkVibrating && ms >= kOkVibrateMs) {
        gOkVibrating = ETrue;
        w3dEngine::VibrateMs((int)(kOkLongMs - kOkVibrateMs), 0.6f);
    }
    if (ms < kOkLongMs) { g_redraw = true; return; }
    gOkWaiting = EFalse;
    gOkLongDone = ETrue;
    if (gOkWithMouse) {
        W3dLayoutReleaseViewport(gOkMx, gOkMy);   // (suelta el click del OK antes de abrir el menu)
        LayoutMenuContextIn(gOkMx, gOkMy);
    } else {
        LayoutMenuContextKeyboard();
    }
    g_redraw = true;
}
// mouse virtual (w3dmouse.cpp): si esta visible, las flechas/OK del telefono lo manejan
extern bool mouseVisible;
extern GLshort mouseX, mouseY; // pos del cursor virtual a nivel de archivo (el OK del loop cut la usa)
void W3dMouseMoveArrows(int aDx, int aDy);
// el IMAN con las flechas (main/ui/ViewPorts/LayoutInput.cpp): moviendo con el iman prendido, cada flecha salta al
// siguiente nivel de la geometria (en vez de deslizar)
bool SnapByArrows();
bool SnapStepArrow(int fx, int fy);
// loop cut modal (LayoutInput.cpp, compartido): el teclado del telefono lo maneja
bool LoopCutActive(); void LoopCutKey(int aDir); void LoopCutClickLeft(int, int); void LoopCutClickRight(); bool LoopCutOrienting(); void LoopCutOrientApplyPicked();
// loop select: modal de direccion (desde una cara, via menu Select); arrancado por LayoutLoopSelectActive
bool LoopSelOrienting(); void LoopSelKey(int aDir); void LoopSelConfirm();
// insertar caras / bevel (Pick3D.cpp, compartido): las flechas agrandan / achican, el lapiz escribe el valor exacto,
// OK confirma y C cancela
bool InsetActive(); bool InsetKey(int key); void InsetConfirm(); void InsetCancel();
bool BevelActive(); bool BevelKey(int key); void BevelConfirm(); void BevelCancel();
static TBool MeshModalActive() { return (InsetActive() || BevelActive()) ? ETrue : EFalse; }
// pick shortest path guiado (menu Select): el click lo maneja el pick; C cancela el modo
bool PickPathGuided(); void LayoutPickPathCancel();
// Select Linked / Loop Select guiados de 1 click (menu Select): el click lo maneja el pick; C cancela
bool GuidedUnClickActive(); void LayoutGuidedCancel();
// navegacion de seleccion en Edit Mode (LayoutInput.cpp): lapiz + flechas del telefono.
// Devuelven false si no estamos en Edit Mode (-> el caller hace el ciclado de OBJETOS).
bool EditSelAdvance(int, bool); bool EditSelAllToggle(); bool EditSelToggleCurrent();
// LAPIZ del N95 (= "shift" del usuario): pulsacion CORTA sin flecha = ciclar al soltar;
// pulsacion larga = nada; mantener + flecha = navegar la seleccion.
static TBool gPencilHeld = EFalse, gPencilArrowUsed = EFalse;
// accesor del lapiz (shift) para el codigo compartido: la barra del viewport muestra "copiar"/"pegar" al mantenerlo.
bool W3dPencilHeld(){ return gPencilHeld ? true : false; }
static TUint gPencilDownTick = 0;
// el lapiz como INTERRUPTOR durante un transform: prendido, los numeros del keypad escriben el valor exacto
static TBool gNumKeyboard = EFalse;
// MANTENER el lapiz durante un transform (sin tipear): INCLINAR EL TELEFONO. Rotando, el angulo girado (como un volante)
// es el del objeto y la vista lo acompania; moviendo o escalando, cuanto mas inclinado mas rapido. Ver TiltTick
static TBool gTilt = EFalse;           // inclinando ahora (el lapiz sigue apretado)
static TBool gTiltBase = EFalse;       // ya llego la primera muestra: el "cero"
static TBool gTiltRollView = EFalse;   // la vista esta girada (vuelve al terminar el transform)
static TBool gTiltSensorTurnedOn = EFalse;
static float gTiltAntRoll = 0.0f;       // el giro de la muestra anterior (para sumar de a pasos, sin el salto de +-180)
static float gTiltAngle = 0.0f;         // los grados girados desde que se apreto el lapiz
static float gTiltAccum = 0.0f;         // lo girado en las veces anteriores (soltar y volver a mantener sigue de ahi)
static float gTiltPitch0 = 0.0f, gTiltRoll0 = 0.0f;
static float gTiltRestX  = 0.0f, gTiltRestY = 0.0f;   // fracciones de pixel (mover/escalar)
static const TUint kTiltMs = 350;       // mantener el lapiz esto sin tipear = inclinar
// ---- INCLINAR EL TELEFONO durante un transform (el lapiz mantenido sin tipear) ----
namespace w3dEngine { void TiltTurnOn(bool); bool OnTilt(); bool TiltReadPhone(float&, float&, float&); }
extern void NumInputSet(float v);
extern void W3dViewRollSet(float aDegrees);
extern void W3dViewRollRestore();
extern TBool W3dXformRotatesInView();
static float TiltDegrees(float r) { return r * 57.29578f; }
static void TiltFinish() {   // se solto el lapiz: queda lo hecho (OK confirma, como siempre)
	if (!gTilt) return;
	gTilt = EFalse;
	gTiltAccum = gTiltAngle;
	if (gTiltSensorTurnedOn) { w3dEngine::TiltTurnOn(EFalse); gTiltSensorTurnedOn = EFalse; }
}
static void TiltTick() {
	extern bool NumInputActive();
	// termino el transform (OK / cancelar): la vista vuelve a donde estaba y lo girado se olvida
	if (!W3dNewTransformActive()) {
		TiltFinish();
		gTiltAccum = 0.0f; gTiltAngle = 0.0f;
		if (gTiltRollView) { W3dViewRollRestore(); gTiltRollView = EFalse; }
		return;
	}
	// arranca: el lapiz mantenido un rato SIN tipear numeros
	if (!gTilt) {
		if (!gPencilHeld || gPencilArrowUsed || gNumKeyboard) return;
		if (OkTicksToMs(User::NTickCount() - gPencilDownTick) < kTiltMs) return;
		if (!w3dEngine::OnTilt()) {
			w3dEngine::TiltTurnOn(ETrue);
			if (!w3dEngine::OnTilt()) { gPencilArrowUsed = ETrue; w3dLog("[inclinar] sin acelerometro"); return; }
			gTiltSensorTurnedOn = ETrue;
		}
		gTilt = ETrue; gTiltBase = EFalse; gTiltRestX = gTiltRestY = 0.0f;
		gPencilArrowUsed = ETrue;   // (soltar el lapiz NO prende el modo de tipear)
		w3dLog("[inclinar] empieza");
	}
	g_redraw = ETrue;
	float x, y, z;
	if (!w3dEngine::TiltReadPhone(x, y, z)) return;   // (el sensor tarda un poco en dar la primera)
	// el giro como un VOLANTE: el angulo de la gravedad en el plano de la pantalla (acostado no se puede medir)
	const TBool hasTurn = (x * x + y * y) > 0.12f;
	const float roll = TiltDegrees((float)atan2(-x, y));
	const float pitch = TiltDegrees((float)asin(z < -1.0f ? -1.0f : (z > 1.0f ? 1.0f : z)));
	if (!gTiltBase) {   // el CERO: como estaba el telefono al empezar
		gTiltBase = ETrue; gTiltAntRoll = roll; gTiltRoll0 = roll; gTiltPitch0 = pitch;
		gTiltAngle = gTiltAccum;
		return;
	}
	extern int DopeXformMode(); extern void DopeXformSetDegrees(float); extern void DopeXformKey(float, float);
	const int modeDope = DopeXformMode();
	if (modeDope == 2 || (modeDope == 0 && W3dNewTransformMode() == 2)) {
		// ROTAR: el angulo girado ES el del objeto (inclinar 83 grados = rotar 83), sobre el eje de la vista
		if (hasTurn) {
			float d = roll - gTiltAntRoll;
			if (d > 180.0f) d -= 360.0f; else if (d < -180.0f) d += 360.0f;
			gTiltAntRoll = roll;
			gTiltAngle += d;
		}
		if (modeDope == 2) { DopeXformSetDegrees(gTiltAngle); return; }   // (keyframes: sin girar la vista)
		NumInputSet(gTiltAngle);
		// la VISTA acompania (el telefono girado + la vista girada al reves: la escena se ve derecha)
		if (W3dXformRotatesInView()) { W3dViewRollSet(gTiltAngle); gTiltRollView = ETrue; }
		return;
	}
	// MOVER / ESCALAR: la inclinacion es la VELOCIDAD (como un joystick): de costado = izq/der, hacia adelante o atras
	// = arriba/abajo. Una zona muerta de 3 grados; a 30 grados, unos 7 pixeles por cuadro
	float dr = roll - gTiltRoll0, dp = pitch - gTiltPitch0;
	if (dr > 180.0f) dr -= 360.0f; else if (dr < -180.0f) dr += 360.0f;
	if (!hasTurn) dr = 0.0f;
	const float zone = 3.0f, k = 0.25f;
	float vx = (dr > zone) ? dr - zone : (dr < -zone ? dr + zone : 0.0f);
	float vy = (dp > zone) ? dp - zone : (dp < -zone ? dp + zone : 0.0f);
	gTiltRestX += vx * k; gTiltRestY -= vy * k;   // (la parte de arriba hacia atras = sube)
	const TInt px = (TInt)gTiltRestX, py = (TInt)gTiltRestY;
	gTiltRestX -= (float)px; gTiltRestY -= (float)py;
	if (px || py) { if (modeDope) DopeXformKey((float)px, (float)py); else W3dNewTransformMove(px, py); }
}


extern bool NumInputActive();
extern void NumInputBegin();
extern void NumInputReset();
// MODIFICADORES de camara del keypad (mismo patron que el lapiz): mantener + flecha = accion de camara.
// 0 = ZOOM (arriba/abajo); tap 0 (sin flecha) = enfocar (como antes). * = PANEO. # = PRIMERA PERSONA (girar).
static TBool g0Held = EFalse, g0ArrowUsed = EFalse;
// el SOFTKEY DERECHO: un toque = la barra de herramientas; mantenido = el mouse virtual
static TBool gSoftRightHeld = EFalse, gSoftRightLong = EFalse;
static TBool gCollapsedFreePanel = EFalse;   // esta tecla: el panel de la ultima operacion esta plegado (no es suya)
static TUint gSoftRightTick = 0;
static const TUint kSoftRightLongMs = 500;
static TBool gStarHeld = EFalse;
static TBool gHashHeld = EFalse, gHashArrowUsed = EFalse; // # + flecha = vista por cuadrante; tap sin flecha = orto/perspectiva
// C/backspace como MODIFICADOR (mismo patron que el LAPIZ): tap = popup de confirmar borrado al soltar;
// mantener + flecha IZQ = undo (Ctrl+Z); mantener + flecha DER = redo (Ctrl+Y).
static TBool gCHeld = EFalse, gCArrowUsed = EFalse;
// del editor/undo compartido (extern inline para no arrastrar headers pesados en Symbian)
extern void UndoUndo(); extern void UndoRedo();
extern void OpenConfirmDelete(bool includeCollections = false); // firma con bool (sino: undefined symbol en ARM)
extern bool LayoutDeleteEdit(int mx, int my);
extern void LayoutMaximize(); // verde + OK = maximizar/restaurar el viewport activo
extern void LayoutExtrudeFaces();      // 7 = Extrude (se protege sola: no-op fuera de Edit Mode)
extern void LoopCutStart(int aMx, int aMy); // 8 = Loop Cut (idem; arranca el modal en el cursor virtual)
extern void LayoutLoopCutFromActive();      // 8 = Loop Cut sobre el elemento ACTIVO (mismo path que el menu del viewport3d)
static void ApplyArrows3D(){
	static TInt gMouseAccel = 0; // rampa del mouse virtual (arranca lento, acelera al mantener)
	static TInt gIDEAccel = 0;   // rampa del cursor del IDE (idem: delay inicial + aceleracion)
	TInt dx = 0, dy = 0;
	if (gHeldLeft) dx -= 1;
	if (gHeldRight) dx += 1;
	if (gHeldUp) dy -= 1;
	if (gHeldDown) dy += 1;
	if (dx == 0 && dy == 0) { gMouseAccel = 0; gIDEAccel = 0; return; } // soltaron: la proxima vez arranca lento de nuevo
	g_redraw = true; // hay flecha mantenida moviendo camara/transform/valor -> redibujar
	// POPUP activo (color picker): la flecha MANTENIDA ajusta el valor CADA frame (R/G/B/A o circulo/value).
	// El auto-repeat del N95 es lentisimo (1fps) -> antes habia que tap-tap-tap ~255 veces. KeyRepeat NO
	// navega (eso es 1-por-tap), asi que mantener arr/aba/izq/der solo mueve el valor enfocado.
	{ extern TBool W3dLayoutCollapsedPopup(); if (W3dLayoutActivePopup() && !W3dLayoutCollapsedPopup()){   // (plegado: las flechas no son suyas)
		if (gHeldLeft)  W3dLayoutKeyRepeat(EStdKeyLeftArrow);
		if (gHeldRight) W3dLayoutKeyRepeat(EStdKeyRightArrow);
		if (gHeldUp)    W3dLayoutKeyRepeat(EStdKeyUpArrow);
		if (gHeldDown)  W3dLayoutKeyRepeat(EStdKeyDownArrow);
		return;
	} }
	// (el foco en la barra de herramientas: las flechas son de ella, una por pulsacion)
	{ extern bool LayoutToolbarFocusActive(); if (LayoutToolbarFocusActive()) return; }
	// INSERTAR CARAS / BEVEL: izq/der (y en el inset tambien arr/ab) agrandan / achican el valor, en PIXELES de pantalla
	// por frame: arranca en 1 px (un ajuste fino) y en ~3 s llega a 12 px por frame
	if (MeshModalActive()){
		extern void MeshModalArrowPx(float);
		float t = (float)gMouseAccel / 45.0f; if (t > 1.0f) t = 1.0f;
		const float f = 1.0f + 11.0f * t * t;
		gMouseAccel++;
		TInt d = dx; if (InsetActive()) d -= dy;   // (en el inset arriba = mas)
		if (d > 1) d = 1; if (d < -1) d = -1;
		if (d) MeshModalArrowPx((float)d * f);
		return;
	}
	// MOVIENDO KEYFRAMES con * o 0 mantenidos: la VISTA del timeline (paneo / zoom), para ver donde quedan antes del OK
	if (DopeXformActive() && (g0Held || gStarHeld)){
		if (W3dLayoutTimelineNav(dx, dy, g0Held, gStarHeld)){ if (g0Held) g0ArrowUsed = ETrue; return; }
	}
	// VERDE + flechas: agranda/achica el viewport activo (cualquiera, no solo 3D)
	// ...con el foco en el PANEL DE CURVAS del timeline: el ANCHO del panel (izq/der), no el del viewport
	{ extern bool TimelinePanelFocusActive(); extern void TimelinePanelWidth(int);
	  if (gGreenHeld && TimelinePanelFocusActive() && dx != 0){ gUsedGreen = ETrue; TimelinePanelWidth(dx); return; } }
	if (gGreenHeld){
		W3dLayoutResizeViewport(dx, dy); // un eje (der/abajo agranda childA)
		return;
	}
	// IDE editando: las flechas MANTENIDAS mueven el cursor con aceleracion (delay inicial, despues rapido).
	// shift(lapiz)+flecha extiende la seleccion. Sin esto el cursor era 1-por-pulsacion (inusable para el codigo).
	{ extern void W3dLayoutIDEArrow(int, bool);
	  if (W3dLayoutIDEEditing()){
		gIDEAccel++;
		bool move;
		if      (gIDEAccel == 1)   move = true;                    // primer paso inmediato
		else if (gIDEAccel < 10)   move = false;                   // delay inicial (como un teclado)
		else if (gIDEAccel < 25)   move = (gIDEAccel % 3 == 0);    // repeticion lenta
		else                       move = true;                    // rapido (cada frame)
		if (move){
			TInt steps = (gIDEAccel > 45) ? 2 : 1;                 // muy rapido: 2 pasos por frame
			bool shift = gPencilHeld ? true : false;
			for (TInt p = 0; p < steps; p++){
				if (gHeldLeft)  W3dLayoutIDEArrow(EStdKeyLeftArrow, shift);
				if (gHeldRight) W3dLayoutIDEArrow(EStdKeyRightArrow, shift);
				if (gHeldUp)    W3dLayoutIDEArrow(EStdKeyUpArrow, shift);
				if (gHeldDown)  W3dLayoutIDEArrow(EStdKeyDownArrow, shift);
			}
		}
		return;
	  }
	}
	// CONSOLA: las flechas MANTENIDAS scrollean con aceleracion (comparte la rampa gIDEAccel: solo una activa a la vez)
	{ extern bool W3dLayoutActiveConsole(); extern void W3dLayoutConsoleScroll(int, int);
	  if (W3dLayoutActiveConsole()){
		gIDEAccel++;
		bool move;
		if      (gIDEAccel == 1)   move = true;
		else if (gIDEAccel < 10)   move = false;
		else if (gIDEAccel < 25)   move = (gIDEAccel % 3 == 0);
		else                       move = true;
		if (move){
			TInt steps = (gIDEAccel > 45) ? 2 : 1;
			for (TInt p = 0; p < steps; p++) W3dLayoutConsoleScroll(dx, dy);
		}
		return;
	  }
	}
	// MOUSE VIRTUAL visible: las flechas mueven el CURSOR como un mouse (no la camara).
	// TAMBIEN durante un transform 3D/de malla (pedido Dante): se mantiene el softkey derecho para que salga el cursor
	// y las flechas lo llevan hasta que aparece el cuadrado del IMAN debajo (el transform sigue al cursor, como el mouse
	// en la PC, y el iman busca bajo el); OK confirma. Sin el cursor, las flechas mueven la seleccion (mas abajo). El de
	// KEYFRAMES lo maneja el CURSOR igual (event_mouse_motion -> DopeMoveApply, el mismo camino que arrastrar con el
	// mouse en PC), asi que ahi las flechas tienen que seguir moviendolo o el transform no se puede manejar con nada.
	// UNA TIRA DE SONIDO agarrada (el 1): izq/der la corren o recortan la punta elegida con el lapiz (lento y acelera)
	{ extern bool TimelineStripActive(); extern void TimelineStripKey(float);
	  if (TimelineStripActive() && !mouseVisible){
		float t = (float)gMouseAccel / 45.0f; if (t > 1.0f) t = 1.0f;
		const float f = 3.0f * (0.25f + 2.25f * t * t);
		gMouseAccel++;
		if (dx) TimelineStripKey((float)dx * f);
		return;
	  } }
	// KEYFRAMES sin el cursor: las flechas los mueven / escalan / rotan directo (arranca lento y acelera)
	if (DopeXformActive() && !mouseVisible){
		extern void DopeXformKey(float, float);
		float t = (float)gMouseAccel / 45.0f; if (t > 1.0f) t = 1.0f;
		const float f = 4.0f * (0.25f + 2.25f * t * t);
		gMouseAccel++;
		DopeXformKey((float)dx * f, (float)dy * f);
		return;
	}
	if (mouseVisible || DopeXformActive()){   // (keyframes: aunque el cursor este oculto)
		// arranca LENTO (2px/frame) y acelera al mantener (hasta 9px) -> control fino al inicio, rapido al cruzar
		int spd = (gMouseAccel < 6) ? 2 : (gMouseAccel < 14) ? 4 : (gMouseAccel < 24) ? 6 : 9;
		gMouseAccel++;
		W3dMouseMoveArrows(dx * spd, dy * spd);
		return;
	}
	// EDITOR UV activo (sin mouse): flechas MANTENIDAS = paneo CONSTANTE suave; 0 + arriba/abajo = ZOOM.
	if (W3dLayoutUVNav(dx, dy, g0Held)){ if (g0Held) g0ArrowUsed = ETrue; return; }
	// TIMELINE activo (sin mouse): izq/der = mover el frame (scrub); 0 + arriba/abajo = zoom; * + flechas = paneo.
	if (W3dLayoutTimelineNav(dx, dy, g0Held, gStarHeld)){ if (g0Held) g0ArrowUsed = ETrue; return; }
	if (!W3dLayout3DActive()) return;
	// MODIFICADORES de camara del keypad (mantener tecla + flechas), prioridad sobre orbit/transform:
	if (g0Held){ if (dy != 0) W3dNewZoom(-dy); g0ArrowUsed = ETrue; return; }   // 0 + arriba/abajo = ZOOM
	if (gStarHeld){ W3dNewPan(dx * 8, dy * 8); return; }                        // * + flechas = PANEO
	// # + flechas = VISTAS POR CUADRANTE: lo aplica el KEY-DOWN (una por pulsacion). Aca no va nada: esto corre
	// cada frame y te haria girar la vista sin control.
	if (W3dNewTransformActive()){
		// con el IMAN prendido, moviendo: las flechas SALTAN de a un nivel, una por pulsacion (en el key-down:
		// SnapStepArrow). Mantenida no desliza
		if (SnapByArrows()) return;
		// mover: paso fino; rotar/escalar: el DOBLE (se sentian lentos). MANTENIDA arranca LENTO (un cuarto: preciso) y
		// va ACELERANDO hasta 2.5 veces en ~1.5 s (cruzar la pantalla sin esperar). Fracciones de pixel acumuladas
		static float sRestX = 0.0f, sRestY = 0.0f;
		if (gMouseAccel == 0) { sRestX = 0.0f; sRestY = 0.0f; }
		const float d = (W3dNewTransformMode() == 1) ? 4.0f : 8.0f;
		float t = (float)gMouseAccel / 45.0f; if (t > 1.0f) t = 1.0f;
		const float f = d * (0.25f + 2.25f * t * t);
		gMouseAccel++;
		sRestX += (float)dx * f; sRestY += (float)dy * f;
		const TInt px = (TInt)sRestX, py = (TInt)sRestY;
		sRestX -= (float)px; sRestY -= (float)py;
		if (px || py) W3dNewTransformMove(px, py);
	} else {
		const TInt o = 8;          // orbita suave por frame
		W3dNewOrbit(dx * o, dy * o);
	}
}

// MODO JUEGO: mapea el scancode del keypad del N95 al nombre de tecla que leen los scripts lua
// (tecla/teclaApretada/stick). El D-pad navega el menu y mueve la pala (1 jugador usa arriba/abajo);
// el centro (select) se trata aparte como enter+espacio. 0 = no es tecla de juego (sigue al editor).
static const char* W3dGameKeyName(TInt sc){
	switch (sc){
		case EStdKeyUpArrow:    return "up";
		case EStdKeyDownArrow:  return "down";
		case EStdKeyLeftArrow:  return "left";
		case EStdKeyRightArrow: return "right";
		case EStdKeyDevice3:    // centro del D-pad (select)
		case EStdKeyEnter:      return "enter";
		case 164:               return "softizq"; // softkey IZQUIERDO: reservado para juegos (solo dentro de un viewport de juego)
		case 165:               return "softder"; // softkey DERECHO: idem (en pausa sigue alternando el cursor virtual)
		case '1':               return "1";       // digitos del keypad: van al juego durante el play (key("1").."key("6"))
		case '2':               return "2";
		case '3':               return "3";
		case '4':               return "4";       // 4/5/6 tambien son del juego (armas del demo GTA:
		case '5':               return "5";       // 4 = arma anterior, 5 = arma siguiente); en pausa
		case '6':               return "6";       // siguen siendo los atajos del editor
		default:                break;
	}
	// las LETRAS de un teclado QWERTY (E71, E72, N97...): el jugador 2 de la pantalla dividida juega con ellas.
	// (El scancode de una letra es su mayuscula ASCII; el script las lee en minuscula, como en la PC)
	static const char* const kLetters[26] = { "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q",
	                                         "r","s","t","u","v","w","x","y","z" };
	if (sc >= 'A' && sc <= 'Z') return kLetters[sc - 'A'];
	return 0;
}

TKeyResponse CWhisk3DContainer::OfferKeyEventL( const TKeyEvent& aKeyEvent,TEventCode aType ){
	// EEventKey (auto-repeat) se ignora: las flechas mantenidas las aplica el
	// loop de frame (gHeld* + ApplyArrows3D), no el repeat lento del telefono.
	if (aType == EEventKey) return EKeyWasConsumed;
	// UN RENDER EN CURSO (la barra de progreso): la tecla derecha, la C o Escape lo cancelan; nada mas se atiende
	{ extern bool ProgressActive(); extern void ProgressCancel();
	  if (ProgressActive()) {
		const TInt sc0 = aKeyEvent.iScanCode;
		if (aType == EEventKeyDown && (sc0 == EStdKeyDevice1 || sc0 == EStdKeyBackspace || sc0 == EStdKeyEscape)) ProgressCancel();
		return EKeyWasConsumed;
	  } }
	g_redraw = true; // cualquier tecla del telefono (down/up) -> redibujar
	switch( aType )
		{
		case EEventKeyDown:
			{
			// teclas del TELEFONO (el teclado BT entra por hidsrv, asi que
			// NO pasa por aca: 1/2/3 del BT quedan libres, como pidio Dante)
			// 1/2/3 = mover/rotar/escalar (G/R/S de PC) - 0 = modo de vista
			TInt sc = aKeyEvent.iScanCode;
			{ static TInt sLog = 0; if (sLog < 30) { sLog++; w3dLogf("[teclas] telefono: scancode %d", sc); } }
			// TAB (teclado Bluetooth que el sistema entrega como tecla normal): como en la PC, con una malla activa
			// entra/sale del modo edicion; si no, cicla el viewport activo
			if (sc == EStdKeyTab){
				if (!LayoutToggleEditMode()) LayoutCycleActiveViewport(+1);
				return EKeyWasConsumed;
			}
			// VERDE + '0' = MODO JUEGO PURO (pedido del dueno): el 3D del juego a
			// pantalla completa y CERO editor (menu/headers/overlays/estadisticas),
			// para medir el rendimiento REAL. Re-apretar restaura todo. gUsedGreen
			// evita que al soltar la VERDE se cicle el viewport (mismo patron que
			// VERDE+OK y VERDE+flecha).
			if (gGreenHeld && sc == '0'){
				gUsedGreen = ETrue;
				extern void LayoutPureGameToggle();
				LayoutPureGameToggle();
				return EKeyWasConsumed;
			}
			// MODO JUEGO: el D-pad + select van DIRECTO a los scripts (tecla/teclaApretada), con prioridad
			// sobre el ruteo del editor (que se come las flechas para orbita/cursor). Sin esto el juego cargaba
			// pero no respondia a nada en el N95. Cede a popup/menu abiertos (escotilla: softkeys siguen libres).
			if (AnimIsGame && PlayAnimation && !gGreenHeld && W3dLayoutGameActiveViewport() && !W3dLayoutActivePopup() && !W3dLayoutOpenMenu()){
				const char* jn = W3dGameKeyName(sc);
				if (jn){
					extern void W3dScriptKey(const char*, bool);
					W3dScriptKey(jn, true);
					if (sc == EStdKeyDevice3 || sc == EStdKeyEnter) W3dScriptKey("space", true); // select cuenta como enter Y espacio
					return EKeyWasConsumed;
				}
				// 7/8/9/0/*/# SON DEL JUEGO mientras corre (pedido del dueno: el 8
				// cambiaba camara<->viewport en pleno gameplay). Van a los scripts
				// por nombre y el editor NO los ve; en PAUSA vuelven a ser los
				// atajos de siempre (extrude/tecla8/orbital/zoom/paneo/vistas).
				if (sc == '7' || sc == '8' || sc == '9' || sc == '0' ||
				    sc == '*' || sc == EStdKeyHash){
					char jnom[2]; jnom[0] = (char)(sc == EStdKeyHash ? '#' : sc); jnom[1] = 0;
					extern void W3dScriptKey(const char*, bool);
					W3dScriptKey(jnom, true);
					return EKeyWasConsumed;
				}
			}
			// popup modal (File browser) o barra de menu abierta: se quedan con
			// TODO el keypad (flechas mueven el foco/menu, OK selecciona, soft
			// IZQ=164=Cancelar/cerrar). Asi se navegan SIN mouse BT y los
			// digitos no tocan la escena 3D.
			// ...SALVO un campo del popup en edicion (OK sobre un valor del panel de la ultima transformacion, el texto
			// de Buscar del IDE): ahi el keypad ESCRIBE (bloque T9 de abajo) y el popup sigue abierto. Termina con OK.
			// ...y el SELECTOR DE SIMBOLOS: se abre ESCRIBIENDO (el '*' de un texto), o sea con el T9 activo, y es el que
			// tiene que recibir las teclas. Antes caian al T9: las flechas movian el caret, los softkeys no lo cerraban y
			// el '*' lo volvia a abrir -> quedaba arriba de todo y la app no se podia usar mas.
			// EL PANEL DE LA ULTIMA OPERACION (abajo a la izquierda, despues de mover / extruir / mover keyframes): nace
			// PLEGADO (solo su titulo) y queda A LA VISTA. SOLO el SOFTKEY DERECHO lo despliega (ahi las flechas / OK / C
			// son de el) y otro softkey derecho lo vuelve a plegar. Plegado, CUALQUIER OTRA tecla (OK, flechas para orbitar,
			// el 0 del zoom, el softkey izquierdo del menu...) lo ACEPTA y lo CIERRA, y la tecla sigue su camino de siempre:
			// antes quedaba a la vista orbitando / haciendo zoom, el OK lo abria y el softkey izq abria el menu CON la
			// tarjeta. (Pedido Dante.) Con el dedo o el mouse, un toque en el panel, como siempre
			{ extern TBool W3dLayoutPopupSemimodal(); extern TBool W3dLayoutCollapsedPopup(); extern TBool W3dLayoutPopupCollapseToggle();
			  extern void W3dLayoutPopupContinue();
			  if (W3dLayoutPopupSemimodal() && !W3dT9Active() && !W3dLayoutOpenMenu()){
				if (sc == 165 && W3dLayoutPopupCollapseToggle()) return EKeyWasConsumed;   // abrir / plegar
				if (W3dLayoutCollapsedPopup()) W3dLayoutPopupContinue();   // aceptar y cerrar: la tecla sigue abajo
			  }
			}
			extern bool SymbolsActive();
			const TBool freePanel = gCollapsedFreePanel; gCollapsedFreePanel = EFalse;
			if ((W3dLayoutActivePopup() && !freePanel && (!W3dT9Active() || SymbolsActive())) || W3dLayoutOpenMenu()){
				extern GLshort mouseX; extern GLshort mouseY;
				// soft DER (165) sobre un MENU desplegable (no un popup modal) -> lo CIERRA, igual que el soft
				// IZQ. (En un popup -ej color picker- 165 sigue siendo Aceptar/OK.) Pedido Dante.
				if (sc == 165 && W3dLayoutOpenMenu() && !W3dLayoutActivePopup()){
					W3dLayoutKey(164, mouseX, mouseY); // 164 = Cancel = cierra el menu
					return EKeyWasConsumed;
				}
				// POPUP (no menu): marca la flecha MANTENIDA -> ApplyArrows3D la REPITE cada frame para
				// ajustar el valor (color picker R/G/B/A o circulo/value). El 1er press de abajo navega/ajusta.
				if (W3dLayoutActivePopup()){
					if (sc == EStdKeyLeftArrow)       gHeldLeft = ETrue;
					else if (sc == EStdKeyRightArrow) gHeldRight = ETrue;
					else if (sc == EStdKeyUpArrow)    gHeldUp = ETrue;
					else if (sc == EStdKeyDownArrow)  gHeldDown = ETrue;
				}
				// '*' cierra el selector de simbolos (otra forma de salir sin elegir nada)
				if (sc == '*'){ extern bool SymbolsActive(); extern void SymbolsClose(); if (SymbolsActive()){ SymbolsClose(); return EKeyWasConsumed; } }
				W3dLayoutKey(sc, mouseX, mouseY);
				return EKeyWasConsumed; // modal: traga el resto tambien
			}
			// EDICION NUMERICA por texto en curso (un PropFloat en Propiedades): el keypad ESCRIBE el valor.
			// 0-9 = digitos, '*' = punto decimal, C(backspace) borra un digito, OK aplica, soft-IZQ(164) cancela,
			// izq/der mueven el caret. Todo lo demas se consume (no toca la escena). Va ANTES de softkeys/1-2-3/etc.
			// EDICION DE TEXTO (CUALQUIER campo enfocado: nombre, path, formula numerica, script del IDE): el
			// keypad escribe con T9/multi-tap. Antes solo los campos NUMERICOS recibian teclas -> los PropText
			// (nombres/paths) no se podian editar en el N95. '#' cambia el modo (Abc/abc/ABC, mantenido=123);
			// '*' = punto decimal en numerico (los simbolos en texto vienen en la etapa 2). izq/der mueven el
			// caret, C borra, OK confirma y sale, soft-IZQ(164) cancela.
			// VERDE (resize/ciclar viewport) y LAPIZ (shift para seleccionar/copiar/pegar) NO los toma el T9: se dejan
			// pasar a sus handlers de abajo (gGreenHeld/gPencilHeld) asi andan mientras se edita texto. El bug era que
			// el T9 se los comia -> ni resize, ni copiar/pegar, ni seleccion (gPencilHeld nunca se seteaba).
			if (W3dT9Active() && sc != EStdKeyYes && sc != EStdKeyLeftShift && sc != EStdKeyRightShift){
				// IDE editando codigo: el T9 escribe en el editor de scripts (no en un TextField). Flechas = cursor,
				// OK = nueva linea, C = backspace, soft-izq = salir a la barra (script/save/refresh).
				if (W3dLayoutIDEEditing()){
					if ((sc >= '0' && sc <= '9') || sc == EStdKeyHash){ W3dT9Key(sc, ETrue); return EKeyWasConsumed; }
					if (sc == EStdKeyBackspace){ W3dT9Reset(); W3dLayoutIDEDelete(); return EKeyWasConsumed; }
					// las flechas se MANTIENEN (gHeld*) y ApplyArrows3D mueve el cursor cada frame con aceleracion
					// (arranca lento, acelera). shift(lapiz)+flecha selecciona. Single-tap = 1 movimiento.
					if (sc == EStdKeyLeftArrow){  W3dT9Reset(); gHeldLeft = ETrue;  return EKeyWasConsumed; }
					if (sc == EStdKeyRightArrow){ W3dT9Reset(); gHeldRight = ETrue; return EKeyWasConsumed; }
					if (sc == EStdKeyUpArrow){    W3dT9Reset(); gHeldUp = ETrue;    return EKeyWasConsumed; }
					if (sc == EStdKeyDownArrow){  W3dT9Reset(); gHeldDown = ETrue;  return EKeyWasConsumed; }
					if (sc == EStdKeyDevice3 || sc == EStdKeyEnter){ W3dT9Reset(); W3dLayoutIDEEnter(); return EKeyWasConsumed; }
					// shift(lapiz) + softkey: izquierda = COPIAR la seleccion, derecha = PEGAR (portapapeles del sistema)
					if (gPencilHeld && sc == 164){ W3dT9Reset(); W3dLayoutIDECopy(); return EKeyWasConsumed; }
					if (gPencilHeld && sc == 165){ W3dT9Reset(); W3dLayoutIDEPaste();  return EKeyWasConsumed; }
					if (sc == 164){ W3dT9Reset(); W3dLayoutIDEExitEdit(); return EKeyWasConsumed; }
					if (sc == '*'){ W3dT9Reset(); { extern void SymbolsOpen(); SymbolsOpen(); } return EKeyWasConsumed; } // selector de simbolos
					return EKeyWasConsumed; // el resto se consume: no toca la escena
				}
				if ((sc >= '0' && sc <= '9') || sc == EStdKeyHash){ W3dT9Key(sc, ETrue); return EKeyWasConsumed; }
				if (sc == EStdKeyBackspace){ W3dT9Reset(); W3dTextFieldChar(8); return EKeyWasConsumed; }
				if (sc == EStdKeyLeftArrow || sc == EStdKeyRightArrow){ W3dT9Reset(); W3dLayoutKeyPanel(sc); return EKeyWasConsumed; }
				if (sc == EStdKeyDevice3 || sc == EStdKeyEnter){ W3dT9Reset(); W3dLayoutKeyPanel(EStdKeyDevice3); return EKeyWasConsumed; }
				if (sc == 164){ W3dT9Reset(); W3dLayoutKeyPanel(EStdKeyEscape); return EKeyWasConsumed; } // soft-IZQ = cancelar
				if (sc == '*'){ if (W3dActiveNumEdit()){ W3dT9Reset(); W3dTextFieldChar('.'); } else { W3dT9Reset(); { extern void SymbolsOpen(); SymbolsOpen(); } } return EKeyWasConsumed; } // '.' en numerico; selector de simbolos en texto
				return EKeyWasConsumed; // mientras se edita, ninguna otra tecla toca la escena
			}
			// FOCO EN EL PANEL DE CURVAS del timeline: flechas y OK lo navegan (W3dLayoutKeyPanel), el 2 oculta / muestra
			// la curva; la C sigue de largo (borra lo elegido) y el 0 tambien (encuadra)
			{ extern bool TimelinePanelFocusActive();
			  if (TimelinePanelFocusActive() && !g0Held && !gStarHeld && !gGreenHeld && !gPencilHeld){
				if (sc == EStdKeyUpArrow || sc == EStdKeyDownArrow || sc == EStdKeyLeftArrow || sc == EStdKeyRightArrow ||
				    sc == EStdKeyDevice3 || sc == EStdKeyEnter){
					W3dLayoutKeyPanel(sc); return EKeyWasConsumed;
				}
				if (sc == '2'){ extern void TimelinePanelEye(); TimelinePanelEye(); return EKeyWasConsumed; }
			  } }
			// FOCO EN LA BARRA DE HERRAMIENTAS (softkey derecho): flechas / OK / C son de ella
			{ extern TBool W3dLayoutToolbarKey(TInt aScan);
			  if (sc != 164 && sc != 165 && W3dLayoutToolbarKey(sc)) return EKeyWasConsumed; }
			// soft IZQ: VERDE + izquierda = PAUSA el juego (combo DELIBERADO). El softkey SOLO no pausa: seria un
			// desastre por inercia (salis de un menu en el timeline y pausas sin querer). En un viewport de juego la
			// izquierda va al juego (W3dGameKeyName, se captura arriba); en el resto abre/cierra la barra de menu.
			if (sc == 164){
				if (gGreenHeld){
					gUsedGreen = ETrue;   // que soltar el verde NO cicle el viewport
					// verde + izquierda = PLAY/PAUSA toggle (pausar Y reanudar con el mismo combo). En JUEGO y tambien
					// en ANIMACION normal (pedido del usuario): togglea PlayAnimation en los dos; solo suelta las
					// teclas del juego si es juego (en animacion no hay input de juego que soltar).
					if (PlayAnimation){
						if (AnimIsGame){ extern void W3dScriptReleaseKeys(); W3dScriptReleaseKeys(); }
						PlayAnimation = false;
					} else PlayAnimation = true;   // reanudar
					return EKeyWasConsumed;
				}
				// IDE en modo barra: el soft-izq VUELVE a editar texto (toggle con el soft-izq que en edicion abre la
				// barra). Sin esto quedabas atascado en la barra (izquierda navegaba/abria el menu de tipo).
				if (W3dLayoutIDEInBar()){ W3dLayoutIDEReturnEdit(); return EKeyWasConsumed; }
				W3dLayoutToggleBar();
				return EKeyWasConsumed;
			}
			// soft DER fuera de un popup: alterna el cursor virtual (lo que
			// antes hacia el softkey "Back", perdido al vaciar el CBA).
			// soft DER: un TOQUE = el foco en la barra de HERRAMIENTAS de abajo (la Vista 3D, el Timeline...), como el
			// izquierdo con la barra de arriba; MANTENIDO medio segundo = el mouse virtual (SoftDerTick, al cumplirse)
			if (sc == 165){
				gSoftRightHeld = ETrue; gSoftRightLong = EFalse; gSoftRightTick = User::NTickCount();
				return EKeyWasConsumed;
			}
			// tecla VERDE de llamada: cicla el viewport activo (borde verde).
			// Sin mouse es la unica forma de elegir viewport. (Si el sistema
			// se la come, hay que CaptureKey EStdKeyYes en el window group.)
			if (sc == EStdKeyYes){
				// mantener: + flechas agranda/achica el viewport. Tap (sin flechas)
				// al soltar: cicla el viewport activo (ver EEventKeyUp).
				gGreenHeld = ETrue;
				gUsedGreen = EFalse;
				return EKeyWasConsumed;
			}
			// tecla ROJA "cortar llamada" (end key): la CONSUMIMOS y no hacemos nada -> que NO cierre la app.
			// Por defecto S60 la lleva a HandleCommandL(EEikCmdExit) -> Exit(). El app SI la recibe (igual que
			// la verde), asi que consumirla aca alcanza para que no cierre.
			if (sc == EStdKeyNo){
				return EKeyWasConsumed;
			}
			// FLECHAS: con el viewport 3D activo se MANTIENEN (gHeld*) y el loop de
			// frame las aplica continuo (fluido). Si el activo NO es 3D, caen al
			// ruteo de paneles de abajo (mouse BT).
			if (sc == EStdKeyUpArrow || sc == EStdKeyDownArrow ||
			    sc == EStdKeyLeftArrow || sc == EStdKeyRightArrow){
				// MENU/desplegable abierto: las flechas NAVEGAN el menu (no orbitan el 3D ni scrollean). Sin esto, con
				// el 3D activo las flechas se "mantenian" para la orbita y la seleccion del menu no se movia -> el menu
				// (ej: el de render) quedaba pegado y no llegabas al item (Stop/Play).
				if (W3dLayoutOpenMenu()){
					if (W3dLayoutKey(sc, 0, 0)) return EKeyWasConsumed;
				}
				// LOOP CUT activo: las flechas eligen orientacion / cortes / slide (single-press)
				if (LoopCutActive()){
					int d = (sc == EStdKeyLeftArrow) ? 0 : (sc == EStdKeyRightArrow) ? 1 : (sc == EStdKeyUpArrow) ? 2 : 3;
					LoopCutKey(d);
					return EKeyWasConsumed;
				}
				// INSERTAR CARAS / BEVEL: las flechas agrandan / achican el valor (en el bevel arriba / abajo = segmentos)
				// El valor se MANTIENE: la flecha queda apretada (gHeld*) y ApplyArrows3D lo cambia cada frame, arrancando
				// lento y acelerando (antes era de a saltos de 10 px por pulsacion). Arriba / abajo del bevel = segmentos,
				// uno por pulsacion.
				if (MeshModalActive()){
					if (BevelActive() && (sc == EStdKeyUpArrow || sc == EStdKeyDownArrow)){
						BevelKey(sc == EStdKeyUpArrow ? W3dK_UP : W3dK_DOWN);
						return EKeyWasConsumed;
					}
					if (sc == EStdKeyLeftArrow)       gHeldLeft = ETrue;
					else if (sc == EStdKeyRightArrow) gHeldRight = ETrue;
					else if (sc == EStdKeyUpArrow)    gHeldUp = ETrue;
					else                              gHeldDown = ETrue;
					return EKeyWasConsumed;
				}
				// LOOP SELECT (modal de direccion desde una cara): las flechas alternan el sentido
				if (LoopSelOrienting()){
					int d = (sc == EStdKeyLeftArrow || sc == EStdKeyUpArrow) ? 0 : 1;
					LoopSelKey(d);
					return EKeyWasConsumed;
				}
				// C mantenido + flecha = UNDO (izq) / REDO (der): el "Ctrl+Z / Ctrl+Y" del telefono
				if (gCHeld){
					gCArrowUsed = ETrue; // que soltar la C NO abra el popup de borrar
					if (sc == EStdKeyLeftArrow)       UndoUndo();
					else if (sc == EStdKeyRightArrow) UndoRedo();
					return EKeyWasConsumed;
				}
				// LAPIZ mantenido + flecha = navegar la seleccion (sub-elemento de malla; objeto = ciclado). SOLO
				// sin mouse virtual: con el mouse visible el lapiz es SHIFT (para shift+click = seleccion multiple) y
				// las flechas deben MOVER EL CURSOR (uno instintivamente manteniene shift y mueve con flechas). Sin
				// mouse virtual, ahi si las flechas navegan la seleccion (todo/nada/siguiente/anterior).
				if (gPencilHeld && !mouseVisible){
					gPencilArrowUsed = ETrue;
					// TIMELINE activo: el lapiz navega KEYFRAMES, no la seleccion de objetos (cambiar de objeto
					// mientras trabajas la animacion es lo ultimo que queres). DopeNavKey devuelve false si el
					// timeline no esta activo -> sigue el camino de siempre.
					int nav = (sc == EStdKeyRightArrow) ? 1 : (sc == EStdKeyLeftArrow) ? 2 :
					          (sc == EStdKeyUpArrow) ? 3 : 4;
					if (DopeNavKey(nav)) return EKeyWasConsumed;
					bool inEdit;
					if (sc == EStdKeyRightArrow)      inEdit = EditSelAdvance(1, true);   // mantiene + siguiente
					else if (sc == EStdKeyLeftArrow)  inEdit = EditSelAdvance(-1, true);  // mantiene + anterior
					else if (sc == EStdKeyUpArrow)    inEdit = EditSelAllToggle();          // todo / nada
					else                              inEdit = EditSelToggleCurrent();      // togglea el activo
					if (!inEdit && (sc == EStdKeyRightArrow || sc == EStdKeyDownArrow) && !W3dNewTransformActive()) W3dNewCycleSelect();
					return EKeyWasConsumed;
				}
				// # mantenido + flecha = VISTA POR CUADRANTE (el 1/3/7 del numpad que el telefono no tiene). Va en el
				// key-down: es UNA por pulsacion.
				if (gHashHeld){
					gHashArrowUsed = ETrue;   // hubo flecha: soltar el # ya no togglea la perspectiva
					const TInt vdx = (sc == EStdKeyRightArrow) ? 1 : (sc == EStdKeyLeftArrow) ? -1 : 0;
					const TInt vdy = (sc == EStdKeyDownArrow)  ? 1 : (sc == EStdKeyUpArrow)   ? -1 : 0;
					W3dQuadrantViewNav(vdx, vdy);
					return EKeyWasConsumed;
				}
				// EL IMAN prendido, moviendo SIN el cursor: cada flecha SALTA al siguiente nivel de la geometria en esa
				// direccion (uno por pulsacion, en vez de deslizar mientras se mantiene). Igual que las flechas de la PC
				if (!g0Held && !gStarHeld && !gGreenHeld && !mouseVisible && W3dNewTransformActive() && SnapByArrows()){
					const TInt fx = (sc == EStdKeyRightArrow) ? 1 : (sc == EStdKeyLeftArrow) ? -1 : 0;
					const TInt fy = (sc == EStdKeyDownArrow)  ? 1 : (sc == EStdKeyUpArrow)   ? -1 : 0;
					SnapStepArrow(fx, fy);
					return EKeyWasConsumed;
				}
				// TIMELINE + arriba/abajo (sin modificadores) = saltar al keyframe siguiente/anterior, igual que en
				// PC. Aca y no en la repeticion de flecha mantenida: es uno por pulsacion. Con 0 (zoom), * (paneo) o
				// VERDE (redimensionar) mantenidos, las flechas son de ellos.
				// Se pregunta por el TIMELINE y no "no es el 3D": el resto de los viewports (Properties, outliner)
				// todavia no overridea event_key_down en el telefono, asi que mandarles la tecla la perderia en el
				// no-op de la base en vez de dejarla seguir a la navegacion del panel.
				if (!g0Held && !gStarHeld && !gGreenHeld && !mouseVisible && W3dLayoutActiveTimeline() && !DopeXformActive() &&
				    (sc == EStdKeyUpArrow || sc == EStdKeyDownArrow)){
					if (W3dLayoutKeyViewport(sc)) return EKeyWasConsumed;
				}
				// TIMELINE + izquierda/derecha: el TOQUE mueve UN cuadro ya (como en PC); mantenida sigue el scrub,
				// que arranca despues de un ratito (W3dTimelinePressedArrow). Sigue abajo para marcarla mantenida.
				if (!g0Held && !gStarHeld && !gGreenHeld && !mouseVisible && W3dLayoutActiveTimeline() && !DopeXformActive() &&
				    (sc == EStdKeyLeftArrow || sc == EStdKeyRightArrow)){
					extern void W3dTimelinePressedArrow();
					W3dLayoutKeyViewport(sc);
					W3dTimelinePressedArrow();
				}
				// 3D con FOCO DE TRANSPORTE (Stop/Play): la flecha NO orbita, cae al dispatch del panel (linea 618)
				// para navegar Stop<->Play, igual que el Timeline con foco suelta las flechas. De ahi el && !W3dLayoutFocusTransport().
				// UV con foco en "Edit" (W3dLayoutFocusUVEdit): la flecha NO panea, cae al dispatch del panel
				// para seguir navegando la barra (mismo esquema que el foco de transporte del 3D).
				if (gGreenHeld || (W3dLayout3DActive() && !W3dLayoutFocusTransport()) || mouseVisible || (W3dLayoutActiveUV() && !W3dLayoutFocusUVEdit()) || W3dLayoutActiveTimeline() || W3dLayoutActiveConsole()){ // 3D=orbita, UV/timeline=paneo/scrub, mouse=cursor, consola=scroll
					if (gGreenHeld) gUsedGreen = ETrue; // verde+flecha = resize (no ciclar)
					if (sc == EStdKeyLeftArrow)       gHeldLeft = ETrue;
					else if (sc == EStdKeyRightArrow) gHeldRight = ETrue;
					else if (sc == EStdKeyUpArrow)    gHeldUp = ETrue;
					else                              gHeldDown = ETrue;
					return EKeyWasConsumed;
				}
				// sin verde ni 3D activo: la flecha va al PANEL activo (propiedades/
				// outliner) por teclado, sin mouse. Single-press (no se mantiene).
				if (W3dLayoutKeyPanel(sc)) return EKeyWasConsumed;
			}
			// LAPIZ + TECLADO durante un transform: el valor EXACTO, como tipear en la PC ("mover 1.5 metros", "rotar 90").
			// Con el lapiz mantenido los numeros se ESCRIBEN; sin el, el 1/2/3 siguen eligiendo el eje y el 0 lo de siempre.
			// * = punto decimal, # = cambia el signo, C = borra. OK confirma, como siempre. (Pedido Dante.)
			// Tambien como INTERRUPTOR: un toque corto del lapiz durante el transform prende el modo "escribir" (sale el
			// "[|]" en la barra) y los numeros escriben sin mantener nada; otro toque lo apaga y vuelve lo de siempre
			// (1/2/3 = ejes). Mantener el lapiz y apretar numeros sigue andando igual.
			const TBool xformDope = DopeXformActive();
			const TBool xformNum = W3dNewTransformActive() || MeshModalActive();   // (insertar caras / bevel: su valor)
			if (gNumKeyboard && (!xformNum || (!xformDope && !NumInputActive()))) gNumKeyboard = EFalse;   // (termino)
			if ((gPencilHeld || gNumKeyboard) && xformNum){
				TInt c = 0;
				if (sc >= '0' && sc <= '9') c = sc;
				else if (sc == '*') c = '.';
				else if (sc == EStdKeyHash) c = '-';
				else if (sc == EStdKeyBackspace) c = 8;
				if (c){
					extern bool NumInputChar(int);
					if (gPencilHeld) gPencilArrowUsed = ETrue;   // (soltar el lapiz despues NO cicla ni cambia el modo)
					// (los KEYFRAMES tienen su propia entrada numerica: la del timeline)
					if (xformDope){ extern bool DopeNumInputChar(int); DopeNumInputChar((int)c); return EKeyWasConsumed; }
					NumInputChar((int)c);
					// en el modo interruptor, borrar todo NO lo apaga (seguiria con "[|]" esperando numeros)
					if (gNumKeyboard && !NumInputActive()) NumInputBegin();
					return EKeyWasConsumed;
				}
			}
			if (sc == '0' && W3dNewTransformActive() && W3dNewTransformMode() == 2){
				W3dNewToggleOrbital(); // 0 durante rotacion: trackball <-> orbital
				return EKeyWasConsumed;
			}
			// "1" sobre el outliner (sin transform 3D en curso) = entrar/confirmar el modo MOVER:
			// reordenar/reparentar el objeto activo sin mouse. Clave en el N95 para poner las
			// lamparas ANTES de los objetos que iluminan. Va antes del 1/2/3 del transform 3D.
			if (sc == '1' && W3dActiveOutliner() && !W3dNewTransformActive()){
				W3dOutlinerMoveToggle();
				return EKeyWasConsumed;
			}
			// "2"/"3" sobre el outliner (sin transform 3D en curso): 2 = ocultar/mostrar, 3 = visibilidad de
			// RENDER del objeto activo. Van ANTES del 1/2/3 del transform 3D (que en el outliner no aplica).
			if (sc == '2' && W3dActiveOutliner() && !W3dNewTransformActive()){
				W3dOutlinerToggleVisible();
				return EKeyWasConsumed;
			}
			if (sc == '3' && W3dActiveOutliner() && !W3dNewTransformActive()){
				W3dOutlinerToggleRender();
				return EKeyWasConsumed;
			}
			if ((sc == '1' || sc == '2' || sc == '3') && DopeXformActive()){
				extern void DopeXformAxisKey(int); DopeXformAxisKey(sc - '0');   // 1 = X (tiempo), 2 = Y (valor)
				return EKeyWasConsumed;
			}
			if ((sc == '1' || sc == '2' || sc == '3') && MeshModalActive()) return EKeyWasConsumed;   // (insertar caras / bevel)
			if (sc == '1' || sc == '2' || sc == '3'){
				// El keypad del telefono no tiene letras: el 1/2/3 ES como se tipea g/r/s. Si el viewport activo no
				// es el 3D, la tecla va a EL, como en PC (el timeline transforma sus keyframes con las MISMAS g/r/s,
				// no con un camino propio). Solo el 3D sigue con W3dNewTransform*: su event_key_down todavia no
				// compila en el telefono.
				if (!W3dLayout3DActive()){
					const TInt t = (sc == '1') ? W3dK_G : (sc == '2') ? W3dK_R : W3dK_S;
					if (W3dLayoutKeyDirectViewport(t)) return EKeyWasConsumed;
				}
				// solo con el mouse sobre el 3D (el hover decide, como PC)
				extern TBool W3dHasMouseBT();
				extern GLshort mouseX; extern GLshort mouseY;
				if (W3dHasMouseBT() && !W3dLayoutMouseOver3D(mouseX, mouseY)){
					return EKeyWasConsumed;
				}
				if (W3dNewTransformActive()){
					W3dNewTransformAxis(sc - '1'); // 1/2/3 = eje X/Y/Z (cicla orient, NO reinicia)
					return EKeyWasConsumed;
				}
				W3dNewTransformStart(sc - '0');
				return EKeyWasConsumed;
			}
			if (sc == '0'){
				// 0 = MODIFICADOR: registrar (NO enfoca en el down). tap (sin flecha) al soltar = enfocar;
				// mantener + flecha arriba/abajo = ZOOM (la unica forma de zoom en el N95). Pedido Dante.
				g0Held = ETrue; g0ArrowUsed = EFalse;
				return EKeyWasConsumed;
			}
			if (sc == '*'){ gStarHeld = ETrue; return EKeyWasConsumed; } // * = MODIFICADOR de PANEO (mantener + flechas)
			if (sc == '6'){
				W3dNewSceneToggleRenderMode(); // 6 = cicla Material Preview/Render/Wireframe/Solid
				return EKeyWasConsumed;
			}
			// EDIT MODE: 7 = Extrude, 8 = Loop Cut, 4 = nada. Las funciones se protegen
			// solas fuera de Edit Mode (chequean InteractionMode), asi que en object mode no hacen nada.
			if (sc == '7'){ LayoutExtrudeFaces(); return EKeyWasConsumed; }
			// 8 = Loop Cut SOLO en Edit Mode de malla; FUERA de edicion = entra/sale de la vista desde la CAMARA
			// ACTIVA (toggle, como el numpad 0 en PC). Antes "8" disparaba Loop Cut siempre -> daba error de loop
			// cut aun sin estar editando una malla. El gate (Edit Mode vs camara) vive en W3dLayoutKey8.
			if (sc == '8'){ W3dLayoutKey8(); return EKeyWasConsumed; }
			if (sc == '9'){ if (!W3dActiveOutliner()) W3dLayoutLockOrbit(); return EKeyWasConsumed; } // 9 = bloquear orbital (NO en el outliner)
			if (sc == '4'){ return EKeyWasConsumed; } // 4 = nada
			// 5 sobre el outliner en una vista de RECURSOS = marcar/desmarcar (seleccion multiple sin mouse)
			if (sc == '5' && W3dActiveOutliner() && !W3dNewTransformActive()){ W3dOutlinerMarkToggle(); return EKeyWasConsumed; }
			if (sc == '5'){ return EKeyWasConsumed; } // 5 = nada
			// con mouse BT activo el teclado navega como en PC: las
			// flechas/OK van al panel bajo el cursor (no mueven el mouse)
			{
				extern TBool W3dHasMouseBT();
				extern GLshort mouseX;
				extern GLshort mouseY;
				if (W3dHasMouseBT() && W3dLayoutKey(sc, mouseX, mouseY)){
					return EKeyWasConsumed;
				}
			}
			// lapiz/shift del telefono: ciclar la seleccion entre objetos
			if (sc == EStdKeyHash){ gHashHeld = ETrue; gHashArrowUsed = EFalse; return EKeyWasConsumed; } // # = vistas (mantener + flechas); tap = orto/perspectiva
			if (sc == EStdKeyLeftShift || sc == EStdKeyRightShift){ // LAPIZ (solo shift ahora; el # se separo para la camara)
				// mantenido = SHIFT (shift + boton del medio = paneo): el shift de un teclado Bluetooth llega por aca, no
				// por el HID (que solo ve los codigos de las letras)
				{ extern bool LShiftPressed; LShiftPressed = true; }
				{ static TInt sLog = 0; if (sLog < 5) { sLog++; w3dLogf("[teclas] shift del sistema (scan %d)", (TInt)sc); } }
				// LAPIZ: solo REGISTRAR. La accion (ciclar) va al SOLTAR si fue una pulsacion
				// corta SIN flecha. Larga = nada. Mantener + flecha = navegar (handler de flechas).
				gPencilHeld = ETrue; gPencilArrowUsed = EFalse; gPencilDownTick = User::NTickCount();
				// editando un script: abajo aparece la barra de las softkeys "Copiar | Pegar" mientras se mantiene
				{ extern void IDEHeldPencil(bool); if (W3dLayoutIDEEditing()) IDEHeldPencil(true); }
				return EKeyWasConsumed;
			}
			// OK del telefono: acepta el transform o la edicion de propiedad
			if (sc == EStdKeyDevice3){
				if (gGreenHeld){ gUsedGreen = ETrue; LayoutMaximize(); return EKeyWasConsumed; } // verde + OK = maximizar/restaurar el viewport
					if (LoopCutActive()){ // OK = aceptar/avanzar fase. Eligiendo la direccion SIN mouse: la de las flechas, aplicada + el panel (antes tomaba el punto amarillo mas cercano al cursor escondido)
					if (LoopCutOrienting() && !mouseVisible) LoopCutOrientApplyPicked(); else LoopCutClickLeft(mouseX, mouseY);
					return EKeyWasConsumed; }
				if (LoopSelOrienting()){ LoopSelConfirm(); return EKeyWasConsumed; } // OK = acepta el loop (modal de direccion)
				if (InsetActive()){ InsetConfirm(); return EKeyWasConsumed; }   // OK = acepta insertar caras / el bevel
				if (BevelActive()){ BevelConfirm(); return EKeyWasConsumed; }
				if (W3dNewTransformActive()){
					W3dNewTransformEnd(EFalse);
					return EKeyWasConsumed;
				}
				// MOUSE VIRTUAL visible: OK = click izquierdo -> selecciona el vert/edge/face/objeto bajo el cursor
				// (mismo pick que el mouse BT). LAPIZ/shift + OK = click ADITIVO: suma a la seleccion, el ultimo
				// clickeado queda de ACTIVO (igual que Shift+click de PC). Asi se juntan varias cosas + un activo
				// para Set Parent, etc. (antes el lapiz+OK hacia loop select y el tap del lapiz ciclaba -> desastre).
				if (mouseVisible){
					// PRIMERO la UI bajo el cursor (menus/barras/paneles/popups/'x' de notificaciones), igual que el
					// BT mouse y PC. Antes el OK virtual hacia SIEMPRE pick 3D -> la cruz de cerrar no respondia (Dante).
					if (W3dLayoutClickUI(mouseX, mouseY)){ return EKeyWasConsumed; }
					extern bool LShiftPressed;
					LShiftPressed = (gPencilHeld || iWhisk3D->iShiftPressed) ? true : false; // lapiz mantenido = shift
					if (gPencilHeld) gPencilArrowUsed = ETrue; // que soltar el lapiz NO cicle la seleccion
					// El OK es un CLICK DE VERDAD al viewport bajo el cursor: el mismo down que manda el mouse en PC.
					// Asi el timeline se entera (curvas, keyframes, handles) sin que nadie reimplemente el click
					// afuera, y mantener el OK queda como arrastre solo (el cursor ya emite event_mouse_motion).
					// El 3D es la excepcion: su button_left() todavia es stub en el telefono -> su pick va aparte.
					// (mantenerlo quieto despues: el menu contextual ahi, ver OkLongTick)
					gOkWaiting = ETrue; gOkWithMouse = ETrue; gOkVibrating = EFalse; gOkLongDone = EFalse;
					gOkTick = User::NTickCount(); gOkMx = mouseX; gOkMy = mouseY;
					if (!W3dLayoutMouseOver3D(mouseX, mouseY) && W3dLayoutClickViewport(mouseX, mouseY)){
						return EKeyWasConsumed;
					}
					iWhisk3D->ClickSelect();
					return EKeyWasConsumed;
				}
				// SIN MOUSE: sobre algo con menu contextual el OK ESPERA (corto = el OK de siempre al soltar; mantenido =
				// vibra y sale el menu: OkLongTick)
				if (LayoutMenuContextKeyboardPossible()){
					gOkWaiting = ETrue; gOkWithMouse = EFalse; gOkVibrating = EFalse; gOkLongDone = EFalse;
					gOkTick = User::NTickCount();
					return EKeyWasConsumed;
				}
				// OK al panel ACTIVO (propiedades/outliner) por teclado, sin mouse
				if (W3dLayoutKeyPanel(EStdKeyDevice3)){
					return EKeyWasConsumed;
				}
				if (W3dLayoutKey(EStdKeyDevice3, 0, 0)){
					return EKeyWasConsumed;
				}
			}
			// tecla C: primero CANCELA (transform activo o edicion de
			// propiedad); si no hay nada que cancelar, borra el activo
			if (sc == EStdKeyBackspace){
				if (LoopCutActive()){ LoopCutClickRight(); return EKeyWasConsumed; } // C = centra el corte (0.5) + confirma
				if (InsetActive()){ InsetCancel(); return EKeyWasConsumed; }   // C = cancela insertar caras / el bevel
				if (BevelActive()){ BevelCancel(); return EKeyWasConsumed; }
				if (PickPathGuided()){ LayoutPickPathCancel(); return EKeyWasConsumed; } // C = cancela el modo guiado
					if (GuidedUnClickActive()){ LayoutGuidedCancel(); return EKeyWasConsumed; } // C = cancela Select Linked / Loop Select guiados
				if (W3dNewTransformActive()){
					W3dNewTransformEnd(ETrue);
					return EKeyWasConsumed;
				}
				// outliner en modo mover: C/backspace CANCELA (restaura la posicion original en el arbol).
				// Solo consume si realmente estaba moviendo (sino LayoutKeyPanelActive devuelve false).
				if (W3dLayoutKeyPanel(EStdKeyBackspace)){
					return EKeyWasConsumed;
				}
				if (W3dLayoutKey(EStdKeyBackspace, 0, 0)){
					return EKeyWasConsumed;
				}
				// el TIMELINE activo con keyframes / curvas elegidos: la C los BORRA (no al objeto activo)
				{ extern bool TimelineHasSelection();
				  if (W3dLayoutActiveTimeline() && TimelineHasSelection() && W3dLayoutKeyViewport(EStdKeyBackspace)) return EKeyWasConsumed; }
				// nada que cancelar: C = MODIFICADOR. Registrar (NO borra en el down). La accion va al
				// SOLTAR: tap (sin flecha) = popup de confirmar borrado; mantener + flecha = undo/redo.
				// gCArrowUsed se resetea SOLO en el PRIMER down: los repeats del backspace mantenido NO deben
				// borrar el "ya use una flecha" (sino, tras un undo/redo, al soltar abria el popup de borrar).
				if (!gCHeld) gCArrowUsed = EFalse;
				gCHeld = ETrue;
				return EKeyWasConsumed;
			}
			return iInputHandler->KeyDown( aKeyEvent.iScanCode );
			}
		case EEventKeyUp:
			{
			// soltar una flecha: deja de aplicarse en el loop de frame
			TInt usc = aKeyEvent.iScanCode;
			// T9: soltar '#' mientras se edita texto resuelve el cambio de modo (tap=Abc/abc/ABC, mantenido=123).
			// TAMBIEN soltar las teclas NUMERICAS: el T9 usa el key-UP para distinguir MANTENER (auto-repeat, sin
			// release) de un re-tap deliberado -> sin esto el "mantener->digito" no anda y el multi-tap se rompe.
			if (W3dT9Active() && (usc == EStdKeyHash || (usc >= '0' && usc <= '9'))){ W3dT9Key(usc, EFalse); return EKeyWasConsumed; }
			// MODO JUEGO: soltar el D-pad + select -> los scripts (fin del "mantenido" que lee key()).
			if (AnimIsGame && PlayAnimation && W3dLayoutGameActiveViewport() && !W3dLayoutActivePopup() && !W3dLayoutOpenMenu()){
				const char* jn = W3dGameKeyName(usc);
				if (jn){
					extern void W3dScriptKey(const char*, bool);
					W3dScriptKey(jn, false);
					if (usc == EStdKeyDevice3 || usc == EStdKeyEnter) W3dScriptKey("space", false);
					return EKeyWasConsumed;
				}
				// el par del key-down: 7/8/9/0/*/# reservadas al juego mientras corre
				if (usc == '7' || usc == '8' || usc == '9' || usc == '0' ||
				    usc == '*' || usc == EStdKeyHash){
					char jnom[2]; jnom[0] = (char)(usc == EStdKeyHash ? '#' : usc); jnom[1] = 0;
					extern void W3dScriptKey(const char*, bool);
					W3dScriptKey(jnom, false);
					return EKeyWasConsumed;
				}
			}
			if (usc == EStdKeyYes){
				if (!gUsedGreen){
					W3dLayoutCycleViewport(1); // tap = ciclar viewport
					// al cambiar de viewport durante el juego, soltar las teclas del juego: si se ciclo con
					// una flecha mantenida, que no quede la pala "trabada" moviendose sola.
					if (AnimIsGame && PlayAnimation){ extern void W3dScriptReleaseKeys(); W3dScriptReleaseKeys(); }
				}
				gGreenHeld = EFalse;
				return EKeyWasConsumed;
			}
			if (usc == EStdKeyNo){ return EKeyWasConsumed; } // tecla ROJA (end): consumida, no cierra la app
			// soltar el OK = soltar el boton del mouse: cierra el arrastre (y su undo) del viewport que lo recibio
			if (usc == EStdKeyDevice3 || usc == EStdKeyEnter){
				if (usc == EStdKeyDevice3 && gOkLongDone){ gOkLongDone = EFalse; return EKeyWasConsumed; }   // (ya salio el menu)
				if (usc == EStdKeyDevice3 && gOkWaiting && !gOkWithMouse){
					// CORTO, sin mouse: el OK de siempre (el que no se hizo al bajar)
					gOkWaiting = EFalse;
					if (!W3dLayoutKeyPanel(EStdKeyDevice3)) W3dLayoutKey(EStdKeyDevice3, 0, 0);
					return EKeyWasConsumed;
				}
				gOkWaiting = EFalse;
				if (mouseVisible) W3dLayoutReleaseViewport(mouseX, mouseY);
				return EKeyWasConsumed;
			}
			if (usc == '0'){ // 0 soltada: tap SIN flecha = enfocar (como antes); con flecha ya hizo zoom
				if (g0Held && !g0ArrowUsed) W3dNewFocus();
				g0Held = EFalse;
				return EKeyWasConsumed;
			}
			if (usc == '*'){ gStarHeld = EFalse; return EKeyWasConsumed; } // * soltada: fin del paneo
			// soft DER soltado: si fue un TOQUE (no llego al medio segundo del mouse virtual), la barra de herramientas
			if (usc == 165 && gSoftRightHeld){
				gSoftRightHeld = EFalse;
				if (!gSoftRightLong){ extern TBool W3dLayoutToolbarToggle(); W3dLayoutToolbarToggle(); }
				gSoftRightLong = EFalse;
				return EKeyWasConsumed;
			}
			if (usc == EStdKeyHash){ // soltada SIN haber tocado una flecha = TAP -> ortografica <-> perspectiva
				if (gHashHeld && !gHashArrowUsed) W3dPerspectiveViewToggle();
				gHashHeld = EFalse;
				return EKeyWasConsumed;
			}
			if (usc == EStdKeyLeftShift || usc == EStdKeyRightShift){ // LAPIZ (solo shift ahora)
				{ extern bool LShiftPressed; LShiftPressed = false; }
				TUint dt = User::NTickCount() - gPencilDownTick; // lapiz soltado: corto + sin flecha = ciclar
				{ extern void IDEHeldPencil(bool); IDEHeldPencil(false); }   // se va la barra Copiar | Pegar
				TiltFinish();   // (si estaba inclinando: queda lo hecho)
				// (editando un script el lapiz es solo shift: no cicla la seleccion de la escena)
				if (gPencilHeld && !gPencilArrowUsed && dt < 500 && !W3dNewTransformActive() && !MeshModalActive() && !mouseVisible && !W3dLayoutIDEEditing()){
					// en el TIMELINE: saca el keyframe actual y pasa al siguiente. En el resto: cicla el activo.
					if (!DopeNavKey(0)) W3dNewCycleSelect();
				} // con mouse virtual el lapiz es shift (no cicla)
				// toque corto DURANTE un transform: prende/apaga el modo de escribir el valor exacto
				else if (gPencilHeld && !gPencilArrowUsed && dt < 500 && (W3dNewTransformActive() || MeshModalActive())){
					// (keyframes: su entrada numerica arranca con el primer numero; aca solo el interruptor)
					if (DopeXformActive()){ gNumKeyboard = !gNumKeyboard; }
					else if (gNumKeyboard){ gNumKeyboard = EFalse; NumInputReset(); }   // apagado: vuelven los ejes (1/2/3)
					else            { gNumKeyboard = ETrue;  NumInputBegin(); }   // prendido: "[|]" en la barra
					g_redraw = true;
				}
				gPencilHeld = EFalse;
				return EKeyWasConsumed;
			}
			// C soltada: tap SIN flecha = borrar (edit mode -> menu Delete en el cursor; object mode ->
			// popup de confirmar). Si se uso flecha (undo/redo) NO hace nada (el undo/redo ya paso).
			if (usc == EStdKeyBackspace){
				if (gCHeld && !gCArrowUsed){
					extern GLshort mouseX; extern GLshort mouseY;
					// (en una vista de RECURSOS del outliner borra los recursos elegidos, no la escena)
					if (!LayoutDeleteEdit(mouseX, mouseY) && !W3dOutlinerResourcesDelete()) OpenConfirmDelete();
				}
				gCHeld = EFalse;
				return EKeyWasConsumed;
			}
			if (usc == EStdKeyLeftArrow)       gHeldLeft = EFalse;
			else if (usc == EStdKeyRightArrow) gHeldRight = EFalse;
			else if (usc == EStdKeyUpArrow)    gHeldUp = EFalse;
			else if (usc == EStdKeyDownArrow)  gHeldDown = EFalse;
			return iInputHandler->KeyUp( aKeyEvent.iScanCode );
			}
		}
	return EKeyWasNotConsumed;
}

// Destructor
CWhisk3DContainer::~CWhisk3DContainer(){
    // desde aca el editor se esta desarmando: el tactil ya no le manda nada (perder el foco al cerrar levantaba los
    // dedos y mandaba eventos a viewports a medio destruir: KERN-EXEC 3 al salir)
    iOpenGlInitialized = EFalse;
    w3dEngine::GamepadsClose();   // (apaga la vibracion y suelta el driver de los joysticks)
    delete iPeriodic;

    if ( iWhisk3D ){
        delete iWhisk3D;
    }
    delete iInputHandler;
    // el monitor HID es un CActive: si no se borra queda escuchando despues
    // de destruir el container
    delete iHidMonitor;
    W3dLayoutDestroy();

    W3dScreensDestroy();   // (sus superficies comparten el contexto: antes de destruirlo)
    eglMakeCurrent( iEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroySurface( iEglDisplay, iEglSurface );
    eglDestroyContext( iEglDisplay, iEglContext );
    eglTerminate( iEglDisplay );                   // Release resources associated
                                                   // with EGL and OpenGL ES
}

// ---------------------------------------------------------
// CWhisk3DContainer::SizeChanged()
// Called by framework when the view size is changed
// ---------------------------------------------------------
//
void CWhisk3DContainer::SizeChanged(){
    if( iOpenGlInitialized && iWhisk3D )
        {
        TSize size;
        size = this->Size();

        iWhisk3D->SetScreenSize( size.iWidth, size.iHeight );
        // re-arma el layout (cambia entre filas y columnas si cambio la
        // orientacion) y recalcula los rectangulos de cada viewport
        W3dLayoutBuild( iWhisk3D, size.iWidth, size.iHeight );
        }
    }

// ---------------------------------------------------------
// CWhisk3DContainer::HandleResourceChange(
//     TInt aType)
// Dynamic screen resize changes by calling the
// SetExtentToWholeScreen() method again.
// ---------------------------------------------------------
//
 void CWhisk3DContainer::HandleResourceChange(TInt aType)
    {
	switch( aType )
    	{
	    case KEikDynamicLayoutVariantSwitch:
		    SetExtentToWholeScreen();
		    break;
	    }
    }

// ---------------------------------------------------------
// CWhisk3DContainer::CountComponentControls() const
// ---------------------------------------------------------
//
TInt CWhisk3DContainer::CountComponentControls() const
    {
    return 0; // return nbr of controls inside this container
    }

// ---------------------------------------------------------
// CWhisk3DContainer::ComponentControl(TInt aIndex) const
// ---------------------------------------------------------
//
CCoeControl* CWhisk3DContainer::ComponentControl(TInt /*aIndex*/ ) const
    {
    return NULL;
    }

// ---------------------------------------------------------
// CWhisk3DContainer::Draw(const TRect& aRect) const
// ---------------------------------------------------------
//
void CWhisk3DContainer::Draw(const TRect& /*aRect*/ ) const
    {
    // No need to implement anything here!
    }

// ---------------------------------------------------------
// CWhisk3DContainer::DrawCallBack( TAny* aInstance )
// Called by the CPeriodic in order to draw the graphics
// ---------------------------------------------------------
//
// PERF del RENDER (lo escribe el [PERF] del modo juego): ms en armar el cuadro (las llamadas al driver: CPU) y en
// eglSwapBuffers (lo que espera al GPU del tiler), y los cuadros dibujados entre dos lineas del log (los fps reales)
static TUint gPfRender = 0, gPfSwap = 0, gPfFrames = 0, gPfFrom = 0;

int CWhisk3DContainer::DrawCallBack( TAny* aInstance )
    {
    CWhisk3DContainer* instance = (CWhisk3DContainer*) aInstance;
    // ESPERANDO AL CODIFICADOR DE VIDEO (render a MP4): su espera anidada deja correr este CPeriodic; no se dibuja ni se
    // toca nada hasta que vuelva (w3dvideoenc.cpp)
    { extern TBool W3dBusyVideoSymbian(); if (W3dBusyVideoSymbian()) return 0; }
    // (un render con su barra de progreso: la espera corta de la barra deja correr este CPeriodic; no se dibuja)
    { extern bool ProgressActive(); if (ProgressActive()) return 0; }
    instance->iFrame++;
    OkLongTick();  // el OK mantenido: vibrar y, al terminar, el menu contextual
    TiltTick();    // el lapiz mantenido en un transform: inclinar el telefono
    // LOS SONIDOS DEL EDITOR (el timeline, el audio scrubbing): en la PC los mueve main.cpp en cada cuadro; el telefono
    // no lo llamaba y la escena no sonaba en el editor
    { extern void W3dSoundsTickEditor(); W3dSoundsTickEditor(); }
    // el softkey derecho MANTENIDO: al cumplirse el medio segundo, el mouse virtual (sin esperar a soltarlo)
    if (gSoftRightHeld && !gSoftRightLong) {
        if (OkTicksToMs(User::NTickCount() - gSoftRightTick) >= kSoftRightLongMs) {
            gSoftRightLong = ETrue;
            { extern void LayoutToolbarReleaseFocus(); LayoutToolbarReleaseFocus(); }
            instance->iWhisk3D->SetMouse();
            w3dEngine::VibrateMs(60, 0.5f);
        }
        g_redraw = true;
    }

    // APERTURA DE PROYECTO DIFERIDA (tarjeta Archivo / abrir un .w3d en el explorador ->
    // OpenProjectFrom solo encola g_projOpenPending). Se procesa ACA, con el stack
    // limpio, igual que MainLoopFrame en PC. Sin esto el .w3d elegido quedaba en la cola y
    // NO se abria en el N95 (el boton que la pidio vive en el layout que OpenProjectNow
    // destruye, por eso el open es diferido y no directo).
    {
        extern std::string g_projOpenPending;
        extern void OpenProjectNow(const std::string&);
        // LA PANTALLA DE INICIO (HomePopup, como en PC): en el PRIMER cuadro, si la app arranco sin un .w3d (el file
        // manager lo encola en ProcessCommandParametersL, antes de este cuadro) ni en modo juego
        static TBool sDecidedStart = EFalse;
        if (!sDecidedStart) {
            sDecidedStart = ETrue;
            extern bool g_gameMode;
            extern void HomePopupOpen();
            if (g_projOpenPending.empty() && !g_gameMode) {
                // la escena de arranque esta "guardada": la foto de lo no guardado, ACA y no al armarla (lo que se inicializa
                // despues -pantallas, el layout, los recursos- la ensuciaba y abrir un reciente preguntaba si guardar)
                { extern void W3dChangesSnapshot(); W3dChangesSnapshot(); }
                HomePopupOpen();   // ("Nuevo archivo" resaltado: OK y arrancas)
                TInt pencil = 0;   // con el DEDO, sin resaltar (parecia apretado)
                if (HAL::Get(HALData::EPen, pencil) == KErrNone && pencil) { extern void HomePopupNoHighlight(); HomePopupNoHighlight(); }
            }
        }
        // LO PENDIENTE, por la MISMA puerta que la PC (main.cpp): abrir un proyecto, empezar uno NUEVO (Nuevo proyecto,
        // "Nuevo archivo" de la pantalla de inicio) y el LAYOUT elegido en el menu. Antes aca solo se abria: elegir el
        // layout de Animacion o un proyecto nuevo quedaban en espera para siempre
        { extern void ProcessPendingProject(); ProcessPendingProject(); }
    }

    // NOTA: aca NO va ningun User::After ni salteo de frames durante la
    // carga de texturas. User::After dentro de este callback hace
    // WaitForRequest y se COME las señales de completado del decoder de
    // imagenes (la carga quedaba colgada para siempre). La convivencia con
    // el decoder se resuelve con la prioridad del CPeriodic (ver ConstructL).

    // Compute the elapsed time in seconds since the startup of the example
#ifdef __WINS__

    // In the emulator the tickcount runs at 200Hz
    GLfloat timeSecs = ( (GLfloat) ( User::NTickCount() - instance->iStartTimeTicks ) ) / 200.f;

#else

    // In the device the tickcount runs at 1000hz (as intended)
    GLfloat timeSecs = ( (GLfloat) ( User::NTickCount() - instance->iStartTimeTicks ) ) / 1000.f;

#endif

    // toasts de export/import (y demas notificaciones): expira las de exito + corre su timer. Va FUERA del
    // bloque de render (como PC) para que, mientras hay un toast, mantenga g_redraw y se siga dibujando.
    // el TIEMPO REAL de este cuadro (las tiras UV de las frutas y los flipbooks lo usan: con el 1/60 fijo de antes, a
    // 48 fps giraban al 80% de su velocidad y mas lento en los cuadros pesados). Acotado como el de la simulacion
    float dtFrame = timeSecs - instance->iLastFrameTimeSecs;
    if (dtFrame > 0.1f)   dtFrame = 0.1f;
    if (dtFrame < 0.001f) dtFrame = 0.001f;
    { extern void NotificationsTick(float); NotificationsTick(timeSecs - instance->iLastFrameTimeSecs); }

    // Set the current time to be the last frame time for the upcoming frame
    instance->iLastFrameTimeSecs = timeSecs;

    // input de usuario (desde donde se cicla, no dentro de Whisk3D): el
    // Escape del telefono cancela la transformacion activa del modelo nuevo
    if (instance->iInputHandler->IsInputPressed( EEscape )){
        W3dNewTransformEnd(ETrue);
    }

    // pantallas externas: conectar/desconectar (revisa 1 vez por segundo)
    W3dScreensTick();

    // T9: confirmar el caracter multi-tap a medio elegir cuando vence el timeout (1 seg sin apretar)
    W3dT9Tick();
    // la PULSACION LARGA tactil (agarrar una fila del outliner, la lupa del IDE): se mide con el reloj, como en PC
    ControlsTick();
    // los JOYSTICKS Bluetooth (driver bthid-s60): conectar/desconectar + su estado, como los mandos de SDL en PC
    GamepadsSymbianTick();
    // el mouse Bluetooth: el apretar que esperaba por si era el boton del medio (izquierdo + derecho juntos), en la
    // pantalla donde esta el cursor (el telefono o la externa)
    extern void W3dMouseTickSymbian();   // (w3dmouse.cpp: la pantalla del mouse y el tick del editor)
    W3dMouseTickSymbian();
    // se perdio el foco con dedos apoyados (FocusChanged): se levantan aca, fuera de cualquier otro evento
    if (instance->iReleaseFingers) { instance->iReleaseFingers = EFalse; w3dEngine::TouchReleaseAll(); }
    // flechas MANTENIDAS: aplicar CADA frame (orbitar/mover/rotar/escalar
    // fluido con el teclado, sin depender del auto-repeat lento del telefono)
    ApplyArrows3D();

    // CARGA DIFERIDA de texturas del import: 1 por frame (decode+upload). Prende g_redraw al cargar una -> el
    // modelo aparece enseguida y las texturas entran solas. No-op si no hay pendientes. (Va ANTES del check de
    // g_redraw para que corra aunque la escena este quieta.)
    { extern void LoadPendingTextures(); LoadPendingTextures(); }

    // ANIMACION: materiales/UV animados cada frame + avance del frame en PLAY al ritmo de AnimFPS (independiente de
    // los fps de la UI). En PC vive en MainLoopFrame; en el N95 FALTABA -> el play quedaba "clavado" en el frame 1 y
    // los materiales animados no corrian. (El avance de las VERTEX-anims -VertexAnimation.cpp, SI en el .mmp- se hace
    // abajo, en la rama de juego, con UpdateAnimations(dtSim); el skinning/pose de esqueleto siguen stub.)
    // PAUSA: las FRUTAS y los BANDERINES son billboards de tira UV (AnimacionUV). En PAUSA del juego
    // (AnimIsGame && !PlayAnimation) NO se avanzan los materiales animados ni las tiras UV -> quedan quietos,
    // igual que en PC (main.cpp:609). En PLAY (PlayAnimation) y en el editor normal (AnimIsGame==false) el
    // gate da true y todo anima como antes. ActiveAnimKind!=3 = no pisar la edicion de vertex-anim del timeline.
    // (Las particulas ya estan gateadas por PlayAnimation dentro del bloque 'juego' de abajo: no van aca.)
    { extern bool TextFieldBlinkTick(); if (TextFieldBlinkTick()) g_redraw = true; }   // el cursor que parpadea
    { extern int ActiveAnimKind;
      if (!(AnimIsGame && !PlayAnimation) && ActiveAnimKind != 3) {
          UpdateAnimatedMaterials();
          extern bool UpdateUVAnims(float); if (UpdateUVAnims(dtFrame)) g_redraw = true;
          extern bool UpdateFlipbooks(float); if (UpdateFlipbooks(dtFrame)) g_redraw = true;
          extern bool W3dOcclusionTick(); if (W3dOcclusionTick()) g_redraw = true;   // oclusion por nodos (.w3dnodos)
      }
    }
    {
    #ifdef __WINS__
        const TUint tickHz = 200;  // el emulador corre NTickCount a 200Hz
    #else
        const TUint tickHz = 1000; // el device (N95) a 1000Hz = ms reales
    #endif
        static TUint gLastAnimTick = 0;
        TUint nowA = User::NTickCount();
        // MODO JUEGO (.w3d con scripts): la simulacion la maneja SimTickPlay -> corre los .lua
        // (inicio/actualizar) y dispara sonido/UI, con dt en TIEMPO REAL a 60 nominal. En PC vive en
        // main.cpp; en el N95 FALTABA -> el juego quedaba clavado en el frame 1 (nada de logica ni sonido).
        // Si NO es juego, el play de animacion normal sigue como antes (AnimTick al ritmo de AnimFPS).
        extern bool SimHasScripts(); extern bool SimActive(); extern void SimTickPlay(float);
        bool game = AnimIsGame && PlayAnimation && (SimActive() || SimHasScripts());
        if (game) {
            TUint gameTicks = tickHz / 60;   // objetivo 60 fps nominal
            if (nowA - gLastAnimTick >= gameTicks) {
                float dtSim = (float)(nowA - gLastAnimTick) / (float)tickHz;  // segundos reales entre ticks
                if (dtSim > 0.1f)   dtSim = 0.1f;    // clamp (igual que PC): no dar pasos gigantes
                if (dtSim < 0.001f) dtSim = 0.001f;
                gLastAnimTick = nowA;
                // PERF (DIAGNOSTICO): cronometrar cada fase de la LOGICA del juego. El profiler del render NO las ve
                // (corren fuera de su ventana). El bonus con 437 draws a 8fps ya prueba que el cuello es un COSTO FIJO
                // por frame (CPU), no los draws. Esto dice cual: lua (SimTickPlay), vertex-anim, PVS o particulas.
                // Loguea el acumulado + promedio cada 30 ticks a e:\whisk3d.log. NTickCount = ms en el N95 (1000 Hz).
                // reloj de ms para el PERF del SKINNING (lo consumen Mesh::RenderObject +
                // SkeletalAnimation.cpp): instalado aca, una vez, solo en modo juego.
                { extern unsigned int (*g_skinClockMs)();
                  struct ClockN95 { static unsigned int Ms() { return (unsigned int)User::NTickCount(); } };
                  if (!g_skinClockMs) g_skinClockMs = ClockN95::Ms; }
                static TUint gPfSim=0, gPfAnim=0, gPfPvs=0, gPfPart=0, gPfN=0; TUint _pf;
                _pf=User::NTickCount(); SimTickPlay(dtSim);                                              gPfSim  += User::NTickCount()-_pf;
                // AVANCE de las vertex-anims (Crash y demas mallas animadas): en PC vive en main.cpp; en el N95 FALTABA.
                _pf=User::NTickCount(); { extern void UpdateAnimations(float); UpdateAnimations(dtSim); }  gPfAnim += User::NTickCount()-_pf;
                // PVS DEL ESCENARIO (oclusion del MAPA como el Crash original): el stepper que lee el nodo del riel y
                // APLICA el sector (override del index buffer por triangulo). En PC vive en main.cpp:617; en el N95 FALTABA.
                _pf=User::NTickCount(); { extern void W3dVisZonesTick(); W3dVisZonesTick(); }              gPfPvs  += User::NTickCount()-_pf;
                // PARTICULAS: simula + MATA las vivas (polvo, humo). En PC vive en main.cpp; en el N95 FALTABA.
                _pf=User::NTickCount(); { extern void W3dParticlesTick(float); W3dParticlesTick(dtSim); } gPfPart += User::NTickCount()-_pf;
                if (++gPfN >= 30) {
                    extern void w3dLogf(const char*, ...);
                    // CULLING: g_cullHijos* se acumulan por Culling::RenderChildren cada frame. El RATIO visibles/total dice
                    // si el frustum descarta mallas en el N95 (visibles<<total = anda; visibles~=total = dibuja TODO ->
                    // el frustum cull no corta y hay que ver por que). El profiler del render mide aparte (overlay stats).
                    extern int g_cullChildrenTotal, g_cullVisibleChildren;
                    extern int g_luaActiveScripts, g_luaScriptsTotal, g_renderFaces, g_renderDraws;
                    // SKINNING (acumulado por Mesh::RenderObject en los 30 frames): skin= ms promedio
                    // por frame; recalc= cuantos SkinMesh recalcularon de verdad y cuantos cayeron
                    // al camino COMPLETO (el incremental deberia dejar casi todo en salpicado/nada).
                    extern int g_skinPerfMs, g_skinPerfSkins, g_completeSkinPerf;
                    // TOP de scripts por ms (promedio del bloque de 30): dice CUAL lua se come el sim=
                    extern void SimLuaPerfTop(char*, int, int);
                    char topLua[96]; SimLuaPerfTop(topLua, sizeof(topLua), 30);
                    // GL: TODAS las llamadas al driver del ultimo cuadro (escena + HUD del juego + editor + GL crudo)
                    const w3dEngine::FrameStats& gc = w3dEngine::g_statFrame;
                    // el render: promedio por cuadro dibujado y los fps de verdad (cuadros / tiempo entre lineas)
                    const TUint nowPf = User::NTickCount();
                    const TUint framesPf = gPfFrames ? gPfFrames : 1;
                    const TUint fpsPf = (gPfFrom && nowPf > gPfFrom) ? (gPfFrames * 1000u) / (nowPf - gPfFrom) : 0;
                    // (de ese render: cuanto es la escena armada por RUTINAS y cuanto el HUD; el resto, el editor y el GL suelto)
                    extern double g_routinesMs, g_uiMs;
                    w3dLogf("[PERF] %u fps | render=%u (routines=%d hud=%d) swap=%u ms por cuadro", fpsPf, gPfRender / framesPf,
                            (int)(g_routinesMs / framesPf + 0.5), (int)(g_uiMs / framesPf + 0.5), gPfSwap / framesPf);
                    g_routinesMs = g_uiMs = 0.0;
                    gPfRender = gPfSwap = gPfFrames = 0; gPfFrom = nowPf;
                    w3dLogf("[PERF] /30f logica: sim=%u anim=%u pvs=%u part=%u ms | skin=%d ms (recalc %d, completos %d) | lua %d/%d scripts [%s] | RENDER: %d caras %d draws | GL %d (esc %d ui %d ed %d) | CULLING: %d de %d hijos",
                            gPfSim/30, gPfAnim/30, gPfPvs/30, gPfPart/30,
                            g_skinPerfMs/30, g_skinPerfSkins/30, g_completeSkinPerf/30,
                            g_luaActiveScripts, g_luaScriptsTotal, topLua, g_renderFaces, g_renderDraws,
                            gc.total, gc.calls[w3dEngine::StatCatScene] + gc.calls[w3dEngine::StatCatParticles],
                            gc.calls[w3dEngine::StatCatUI], gc.calls[w3dEngine::StatCatEditor],
                            g_cullVisibleChildren/30, g_cullChildrenTotal/30);
                    // el DESGLOSE de sim= por fase (SimJuego): fisica / hitbox / lua / animaciones / streaming
                    { extern int SimPerfBreakdown(double*, bool); double d[5]; const int nt = SimPerfBreakdown(d, true);
                      const double k = nt > 0 ? 1.0 / nt : 0.0;
                      w3dLogf("[PERF] sim por tick: physics=%d hitbox=%d lua=%d anim=%d streaming=%d ms",
                              (int)(d[0] * k + 0.5), (int)(d[1] * k + 0.5), (int)(d[2] * k + 0.5), (int)(d[3] * k + 0.5),
                              (int)(d[4] * k + 0.5)); }
                    gPfSim=gPfAnim=gPfPvs=gPfPart=gPfN=0;
                    g_cullChildrenTotal=0; g_cullVisibleChildren=0;
                    g_skinPerfMs=0; g_skinPerfSkins=0; g_completeSkinPerf=0;
                }
                g_redraw = true;
            }
        } else {
            TUint animTicks = (AnimFPS > 0) ? (TUint)(tickHz / AnimFPS) : (tickHz / 30);
            if (nowA - gLastAnimTick >= animTicks) {
                gLastAnimTick = nowA;
                if (PlayAnimation) { AnimTick(); g_redraw = true; } // avanza CurrentFrame (loop Start..End del clip)
            }
        }
    }
    // ANIMACION DE OBJETOS (pos/rot/escala): aplica los keyframes al frame actual. FALTABA en el N95 -> ni el play
    // ni el scrub del timeline movian nada (el frame avanzaba y la escena quedaba quieta). Va FUERA del tick de
    // AnimFPS, como en PC: al scrubear con las flechas se tiene que ver YA, no al ritmo del play. Guard interno:
    // solo trabaja al CAMBIAR de frame.
    { extern void ApplyAnimationObjects(); ApplyAnimationObjects(); }
    // VERTEX ANIM ACTIVA EN EL TIMELINE (kind 3): posa la malla al frame del playhead. En PC vive en main.cpp; en
    // el N95 FALTABA -> al seleccionar un clip (ej "correr") y dar Play, la timeline avanzaba pero la vertex-anim
    // NO cambiaba. UpdateAnimations la saltea a proposito (kind 3): manda el playhead, no el auto-avance.
    { extern void ApplyVertexAnimTimeline(); ApplyVertexAnimTimeline(); }

    // Render EVENT-DRIVEN: solo dibujar+cursor+swap si algo CAMBIO (g_redraw, lo
    // prende cualquier tecla/cursor/flecha) o hay una animacion en play. Si la escena
    // esta quieta, el frame se saltea ENTERO (sin GL ni swap/vsync) -> CPU y bateria
    // casi 0. El timer sigue tickeando, pero sin trabajo de GPU.
    // SOLO LA EXTERNA: el editor entero se dibuja en la pantalla externa; el telefono no gasta GPU (ni swap)
    extern TBool W3dMouseFrameCursor();   // (w3dmouse.cpp)
    if (ExternalScreenOnly() && (g_redraw || HasAnimationActive())) {
        g_redraw = false;
        TUint _pfS = User::NTickCount();
        ExternalScreensRender();
        gPfSwap += User::NTickCount() - _pfS;
        gPfFrames++;
        // (el log paso a paso se apaga tambien aca: el telefono no dibuja, y escribirlo cada cuadro en la tarjeta frenaba)
        extern int g_screensLogFrames;
        if (g_screensLogFrames > 0) g_screensLogFrames--;
    }
    else if (ExternalScreenOnly() && W3dMouseFrameCursor()) {
        // (solo se movio el cursor: repuesto lo de abajo y dibujado en el lugar nuevo, sin redibujar el editor)
    }
    else if (g_redraw || HasAnimationActive()) {
        // CAPTURAR y apagar g_redraw ANTES de dibujar (mismo orden que PC main.cpp): lo que se prenda
        // DURANTE el render vale para el frame SIGUIENTE. Apagarlo despues PISABA esos pedidos: el ray
        // tracer progresivo (RTViewportStep prende g_redraw para seguir avanzando) quedaba congelado
        // hasta la proxima tecla -> "solo renderiza cuando aprieto flechas".
        g_redraw = false;
        // los PRIMEROS cuadros dejan cada paso en el log: si el telefono se cuelga dibujando, la ultima linea dice donde
        static TInt sFramesLog = 0;
        extern int g_screensLogFrames;   // (ExternalScreens.cpp: elegir el modo de una pantalla lo vuelve a prender)
        const TBool logFrame = (sFramesLog < 3) || g_screensLogFrames > 0;
        if (logFrame) w3dLogf("cuadro %d: dibujando el arbol", sFramesLog);
        // arbol de viewports (3D confinado a su rectangulo + props) y el cursor encima
        TUint _pfR = User::NTickCount();
        W3dLayoutRender();
        if (logFrame) w3dLogf("cuadro %d: arbol listo (glErr=%x), el cursor", sFramesLog, (TInt)glGetError());
        instance->iWhisk3D->DrawMouseCursor();
        TUint _pfS = User::NTickCount();
        gPfRender += _pfS - _pfR;

        // Call eglSwapBuffers, which blit the graphics to the window
        if (logFrame) w3dLogf("cuadro %d: swap (glErr=%x)", sFramesLog, (TInt)glGetError());
#ifdef W3D_GLES2
        // CAPTURA de diagnostico del N8 (E:\whisk3d_cap_N.png, se baja por CODA): lo que se va a presentar, leido con TODO
        // dibujado (despues de mandar lo juntado, antes del swap: no cambia lo que se ve). Una del cuadro 10 (sin menu:
        // los iconos) y la del 2do cuadro de cada menu o popup que se abre, hasta 4 en total
        {
            static TInt sCaptures = 0, sFrames = 0, sWithMenu = 0;
            sFrames++;
            const TBool hasMenu = W3dLayoutOpenMenu() || W3dLayoutActivePopup();
            sWithMenu = hasMenu ? sWithMenu + 1 : 0;
            if (sCaptures < 4 && (sFrames == 10 || sWithMenu == 2)) {
                w3dEngine::DrawPending();
                const TSize t = instance->Size();
                unsigned char* px = (unsigned char*)User::Alloc(t.iWidth * t.iHeight * 4);
                if (px) {
                    w3dEngine::ReadPixelsRGBA(0, 0, t.iWidth, t.iHeight, px);
                    char path[40];
                    sprintf(path, "E:\\whisk3d_cap_%d.png", (int)sCaptures);
                    w3dEngine::SavePNG(path, px, t.iWidth, t.iHeight, true);
                    User::Free(px);
                    w3dLogf("captura %d (%s)", (TInt)sCaptures, hasMenu ? "con un menu abierto" : "sin menu");
                    sCaptures++;
                }
            }
        }
#endif
        // lo JUNTADO sin dibujar (el lote de la UI 2D del backend ES 2.0: lo ultimo del cuadro, el menu y su texto) va
        // ANTES del swap, que es EGL directo y no lo manda: sin esto quedaba para el cuadro siguiente, dibujado encima
        // de otra cosa (el N8: los popups rompian todo). Como PCSwapBuffers en PC
        w3dEngine::DrawPending();
        const EGLBoolean swapOk = eglSwapBuffers( instance->iEglDisplay, instance->iEglSurface );
        if (logFrame) w3dLogf("cuadro %d: swap=%d eglErr=%x, pantallas externas", sFramesLog, (TInt)swapOk, (TInt)eglGetError());
        // las pantallas externas con viewport propio (TV/HDMI): su superficie, su arbol, su swap
        ExternalScreensRender();
        if (logFrame) { w3dLogf("cuadro %d: listo", sFramesLog); sFramesLog++; if (g_screensLogFrames > 0) g_screensLogFrames--; }
        gPfSwap += User::NTickCount() - _pfS;
        gPfFrames++;
    }

    // To keep the background light on
    if ( !(instance->iFrame%100) ){
        User::ResetInactivityTime();
    }

    /* Suspend the current thread for a short while. Give some time
       to other threads and AOs, avoids the ViewSrv error in ARMI and
       THUMB release builds. One may try to decrease the callback
       function instead of this. */
    if ( !(instance->iFrame%50) ){
        User::After(0);
    }

    return 0;
}

// ---------------------------------------------------------
// CWhisk3DContainer::HandleControlEventL(
//     CCoeControl* aControl,TCoeEvent aEventType)
// ---------------------------------------------------------
//
void CWhisk3DContainer::HandleControlEventL(
    CCoeControl* /*aControl*/,TCoeEvent /*aEventType*/)
    {
    }

// ---------------------------------------------------------
// PANTALLA TACTIL: cada dedo -> el Core (input/W3dTactil), que arma los mismos eventos que SDL en Android; el
// editor los recibe por controls.cpp, el mismo camino que Android y la web
// ---------------------------------------------------------
//
void CWhisk3DContainer::HandlePointerEventL( const TPointerEvent& aPointerEvent )
    {
    if ( !iOpenGlInitialized )
        {
        return;
        }
    const TSize size = Size();
    w3dEngine::TouchSymbian( aPointerEvent, size.iWidth, size.iHeight );
    }

// SEGUNDO PLANO (minimizar, otra app al frente): el loop de dibujo se detiene. Symbian^3 cierra las apps de fondo que
// siguen usando la GPU cuando otra necesita memoria grafica (y dibujar en una ventana que no se ve no sirve). Al volver
// arranca de nuevo y se redibuja todo
void CWhisk3DContainer::SetFirstPlane( TBool aFirstPlane )
    {
    if ( !iPeriodic )
        {
        return;
        }
    w3dLogf( "[app] %s", aFirstPlane ? "vuelve al frente" : "pasa a segundo plano: el dibujo se detiene" );
    if ( aFirstPlane )
        {
        if ( !iPeriodic->IsActive() )
            {
            iPeriodic->Start( 100, 100, TCallBack( CWhisk3DContainer::DrawCallBack, this ) );
            }
        g_redraw = true;
        }
    else
        {
        iPeriodic->Cancel();
        if ( iOpenGlInitialized && w3dEngine::TouchGetFingers() > 0 ) w3dEngine::TouchReleaseAll();
        }
    }

void CWhisk3DContainer::FocusChanged( TDrawNow /*aDrawNow*/ )
    {
    // (solo con el editor vivo y algun dedo apoyado: al cerrar la app tambien se pierde el foco)
    // (no aca adentro: este aviso puede llegar en medio de otro evento -crear la ventana de una pantalla externa desde
    // el click de un popup- y levantar los dedos manda eventos al editor; va en el cuadro siguiente)
    if ( !IsFocused() && iOpenGlInitialized && w3dEngine::TouchGetFingers() > 0 )
        {
        iReleaseFingers = ETrue;
        }
    }

// End of File
