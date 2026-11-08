/*
 * ==============================================================================
 *  Name        : Whisk3DContainer.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDE FILES
#include <eiklabel.h>
#include "ViewPorts/Timeline.h"  // DopeNavTecla: el lapiz sobre el timeline navega keyframes
#include <e32math.h>

#include "Whisk3DContainer.h"
#include "Whisk3DAppUi.h"
#include "whisk3D.hrh"
#include "w3dlog.h" // log de diagnostico E:\whisk3d.log (modo dev)
#include "w3dlayout.h" // layout compartido (Fase 1 unificacion)
#include "w3dt9.h"     // entrada de texto multi-tap (T9) del keypad
#include "w3dnewscene.h"
#include <GLES/gl.h> // modelo compartido de PC (Fase 3c): init+render
#include "render/OpcionesRender.h"     // g_redraw (render event-driven)
#include "objects/Materials.h"         // HayAnimacionActiva / UpdateAnimatedMaterials
#include "animation/Animation.h"       // PlayAnimation / AnimFPS / AnimTick (avance del play)
#include "w3dpantallas.h"              // pantallas externas (TV-out / HDMI): deteccion + superficie propia
#include "ViewPorts/PantallasExternas.h" // ...y su dibujo (el popup y los viewports son del editor)
#include "w3dGraphics.h"               // g_statCuadro: las llamadas GL del ultimo cuadro en el [PERF]
#include <string.h>                    // strlen/memcpy: el volcado de capacidades GL
#include <hal.h>                       // HAL EPen: el telefono es tactil
#include <stdio.h>                     // sprintf (la captura de diagnostico)
#include "w3dTexture.h"                // SavePNG (idem)
#include "input/W3dTactil.h"            // la pantalla tactil (Core): los mismos eventos que SDL en Android
#include "app/variables.h"             // cfg (la barra de herramientas en los tactiles)
#include "objects/Gamepad.h"           // MandosSymbianTick: los joysticks Bluetooth
#include "app/W3dRatonEditor.h"        // W3dRatonTick: el mouse Bluetooth (boton del medio emulado)
#include "input/W3dMandos.h"            // MandosCerrar
void ControlesTick();                  // (controles.cpp) la pulsacion larga tactil, por reloj
bool LayoutToggleEditMode();            // (LayoutInput.cpp) Tab: entrar/salir del modo edicion
void LayoutCiclarViewportActivo(int dir);

// BARRA DE PROGRESO (export/import OBJ): el hook de swap que faltaba en Symbian. ProgresoIniciar/Actualizar lo
// llaman para mostrar la barra DURANTE la operacion bloqueante (sin esto, ProgresoIniciar hacia return y la
// barra/notificacion no aparecian en el N95). Guarda los handles EGL del surface activo (seteados en ConstructL).
extern void (*LayoutSwapBuffers)();
static EGLDisplay gSwapDisplay = NULL;
static EGLSurface gSwapSurface = NULL;
// (antes del swap, lo que el motor tiene JUNTADO sin dibujar: el swap es EGL directo y no lo manda. Como PCSwapBuffers)
static void W3dProgresoSwap() { w3dEngine::DibujarPendiente(); if (gSwapDisplay) eglSwapBuffers(gSwapDisplay, gSwapSurface); }

// ---- LO QUE EXPONE EL DRIVER GL/EGL DEL TELEFONO (diagnostico, una vez al arrancar) ----------------------------
// Para decidir que caminos de render sirven en cada equipo: las extensiones (programas de vertices, paleta de
// matrices, multi-draw...), los limites, y si eglGetProcAddress encuentra funciones que el driver no anuncia.
// El log corta cada linea a ~200 caracteres: los textos largos van en tramos.
static void LogGlLargo(const char* titulo, const char* txt){
    if (!txt) { w3dLogf("[gl] %s: (nada)", titulo); return; }
    const int n = (int)strlen(txt);
    int i = 0, parte = 1;
    while (i < n) {
        int j = i + 150;
        if (j >= n) j = n;
        else { int k = j; while (k > i && txt[k] != ' ') k--; if (k > i) j = k; }
        char buf[192];
        int len = j - i; if (len > 190) len = 190;
        memcpy(buf, txt + i, len); buf[len] = 0;
        w3dLogf("[gl] %s %d: %s", titulo, parte++, buf);
        i = j;
        while (i < n && txt[i] == ' ') i++;
    }
}
static void VolcarCapacidadesGL(EGLDisplay d){
    LogGlLargo("GL_VENDOR", (const char*)glGetString(GL_VENDOR));
    LogGlLargo("GL_RENDERER", (const char*)glGetString(GL_RENDERER));
    LogGlLargo("GL_VERSION", (const char*)glGetString(GL_VERSION));
    LogGlLargo("GL_EXTENSIONS", (const char*)glGetString(GL_EXTENSIONS));
    LogGlLargo("EGL_VENDOR", eglQueryString(d, EGL_VENDOR));
    LogGlLargo("EGL_VERSION", eglQueryString(d, EGL_VERSION));
    LogGlLargo("EGL_EXTENSIONS", eglQueryString(d, EGL_EXTENSIONS));
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
    iOpenGlInitialized = EFalse;
    iSoltarDedos = EFalse;
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
    const EGLint attrib_list_conserva[] = {
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
    if ( eglChooseConfig( iEglDisplay, attrib_list_conserva, configList, configSize, &numOfConfigs ) == EGL_FALSE ){
        numOfConfigs = 0;
    }
    w3dLogf( "EGL: config que conserva el cuadro en el swap: %s", numOfConfigs > 0 ? "SI" : "no" );
    if ( numOfConfigs > 0 ) W3dPantallasConfigConserva( configList[0] );
    numOfConfigs = 0;
#endif
    if ( eglChooseConfig( iEglDisplay, attrib_list, configList, configSize, &numOfConfigs ) == EGL_FALSE ){
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
    for ( TInt intento = 0; iEglSurface == NULL && intento < 10; intento++ )
        {
        const EGLint err = eglGetError();
        w3dLogf( "EGL: eglCreateWindowSurface fallo (0x%x%s), reintento %d", (TInt)err,
                 err == EGL_BAD_ALLOC ? ": sin memoria grafica, hay otra app abierta?" : "", (TInt)( intento + 1 ) );
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
    const EGLint atributosContexto[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    iEglContext = eglCreateContext( iEglDisplay, Config, EGL_NO_CONTEXT, atributosContexto );
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
    LayoutSwapBuffers = W3dProgresoSwap;

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
        TInt lapiz = 0;
        if (HAL::Get(HALData::EPen, lapiz) == KErrNone && lapiz) {
            cfg.nuevoUsuario = true;
            extern bool g_gizmoOn; g_gizmoOn = true;   // (y el gizmo, como Android: con el dedo se agarra)
        }
    }

    iWhisk3D = CWhisk3D::NewL( size.iWidth, size.iHeight, iInputHandler ); // Create an instance of Whisk3D
    iWhisk3D->AppInit();                                       // Initialize OpenGL ES
    VolcarCapacidadesGL( iEglDisplay );   // (despues de AppInit: ahi arranca el log) que expone este driver

    // Fase 1 unificacion: arbol de viewports compartido (core/ui). Es el
    // equivalente al rootViewport->Resize(winW, winH) del main.cpp de PC:
    // horizontal = 2 columnas (3D | propiedades), vertical = 2 filas.
    W3dLayoutBuild( iWhisk3D, size.iWidth, size.iHeight );

    // Fase 3c: raiz del modelo compartido de PC + cubo default (como
    // ConstructUniversal). Su render sale en cada frame del viewport 3D.
    W3dNewSceneInit();

    // PANTALLAS EXTERNAS (TV-out RCA, HDMI): al conectarse una, el popup "clonar / nuevo viewport / propiedades".
    // Despues del layout: el popup se centra con el tamano de pantalla que arma W3dLayoutBuild.
    W3dPantallasInit( iEglDisplay, iEglConfig, iEglContext, iEglSurface );

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
// gGreenUsado = se uso como modificador (entonces al soltar NO cicla viewport).
static TBool gGreenHeld = EFalse, gGreenUsado = EFalse;
// mouse virtual (w3dmouse.cpp): si esta visible, las flechas/OK del telefono lo manejan
extern bool mouseVisible;
extern GLshort mouseX, mouseY; // pos del cursor virtual a nivel de archivo (el OK del loop cut la usa)
void W3dMouseMoverFlechas(int aDx, int aDy);
// loop cut modal (LayoutInput.cpp, compartido): el teclado del telefono lo maneja
bool LoopCutActivo(); void LoopCutTecla(int aDir); void LoopCutClickIzq(int, int); void LoopCutClickDer();
// loop select: modal de direccion (desde una cara, via menu Select); arrancado por LayoutLoopSelectActivo
bool LoopSelOrientando(); void LoopSelTecla(int aDir); void LoopSelConfirm();
// pick shortest path guiado (menu Select): el click lo maneja el pick; C cancela el modo
bool PickPathGuiado(); void LayoutPickPathCancelar();
// Select Linked / Loop Select guiados de 1 click (menu Select): el click lo maneja el pick; C cancela
bool GuiadoUnClickActivo(); void LayoutGuiadoCancelar();
// navegacion de seleccion en Edit Mode (LayoutInput.cpp): lapiz + flechas del telefono.
// Devuelven false si no estamos en Edit Mode (-> el caller hace el ciclado de OBJETOS).
bool EditSelAvanzar(int, bool); bool EditSelTodoToggle(); bool EditSelToggleActual();
// LAPIZ del N95 (= "shift" del usuario): pulsacion CORTA sin flecha = ciclar al soltar;
// pulsacion larga = nada; mantener + flecha = navegar la seleccion.
static TBool gLapizHeld = EFalse, gLapizArrowUsed = EFalse;
// accesor del lapiz (shift) para el codigo compartido: la barra del viewport muestra "copiar"/"pegar" al mantenerlo.
bool W3dLapizHeld(){ return gLapizHeld ? true : false; }
static TUint gLapizDownTick = 0;
// MODIFICADORES de camara del keypad (mismo patron que el lapiz): mantener + flecha = accion de camara.
// 0 = ZOOM (arriba/abajo); tap 0 (sin flecha) = enfocar (como antes). * = PANEO. # = PRIMERA PERSONA (girar).
static TBool g0Held = EFalse, g0ArrowUsed = EFalse;
static TBool gStarHeld = EFalse;
static TBool gHashHeld = EFalse, gHashArrowUsed = EFalse; // # + flecha = vista por cuadrante; tap sin flecha = orto/perspectiva
// C/backspace como MODIFICADOR (mismo patron que el LAPIZ): tap = popup de confirmar borrado al soltar;
// mantener + flecha IZQ = undo (Ctrl+Z); mantener + flecha DER = redo (Ctrl+Y).
static TBool gCHeld = EFalse, gCArrowUsed = EFalse;
// del editor/undo compartido (extern inline para no arrastrar headers pesados en Symbian)
extern void UndoDeshacer(); extern void UndoRehacer();
extern void AbrirConfirmarBorrado(bool incluirCollecciones = false); // firma con bool (sino: undefined symbol en ARM)
extern bool LayoutDeleteEdit(int mx, int my);
extern void LayoutMaximizar(); // verde + OK = maximizar/restaurar el viewport activo
extern void LayoutExtrudeFaces();      // 7 = Extrude (se protege sola: no-op fuera de Edit Mode)
extern void LoopCutIniciar(int aMx, int aMy); // 8 = Loop Cut (idem; arranca el modal en el cursor virtual)
extern void LayoutLoopCutDesdeActivo();       // 8 = Loop Cut sobre el elemento ACTIVO (mismo path que el menu del viewport3d)
static void AplicarFlechas3D(){
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
	// El auto-repeat del N95 es lentisimo (1fps) -> antes habia que tap-tap-tap ~255 veces. TeclaRepeat NO
	// navega (eso es 1-por-tap), asi que mantener arr/aba/izq/der solo mueve el valor enfocado.
	if (W3dLayoutPopupActivo()){
		if (gHeldLeft)  W3dLayoutTeclaRepeat(EStdKeyLeftArrow);
		if (gHeldRight) W3dLayoutTeclaRepeat(EStdKeyRightArrow);
		if (gHeldUp)    W3dLayoutTeclaRepeat(EStdKeyUpArrow);
		if (gHeldDown)  W3dLayoutTeclaRepeat(EStdKeyDownArrow);
		return;
	}
	// VERDE + flechas: agranda/achica el viewport activo (cualquiera, no solo 3D)
	if (gGreenHeld){
		W3dLayoutRedimensionarViewport(dx, dy); // un eje (der/abajo agranda childA)
		return;
	}
	// IDE editando: las flechas MANTENIDAS mueven el cursor con aceleracion (delay inicial, despues rapido).
	// shift(lapiz)+flecha extiende la seleccion. Sin esto el cursor era 1-por-pulsacion (inusable para el codigo).
	{ extern void W3dLayoutIDEFlecha(int, bool);
	  if (W3dLayoutIDEEditando()){
		gIDEAccel++;
		bool mover;
		if      (gIDEAccel == 1)   mover = true;                    // primer paso inmediato
		else if (gIDEAccel < 10)   mover = false;                   // delay inicial (como un teclado)
		else if (gIDEAccel < 25)   mover = (gIDEAccel % 3 == 0);    // repeticion lenta
		else                       mover = true;                    // rapido (cada frame)
		if (mover){
			TInt pasos = (gIDEAccel > 45) ? 2 : 1;                 // muy rapido: 2 pasos por frame
			bool shift = gLapizHeld ? true : false;
			for (TInt p = 0; p < pasos; p++){
				if (gHeldLeft)  W3dLayoutIDEFlecha(EStdKeyLeftArrow, shift);
				if (gHeldRight) W3dLayoutIDEFlecha(EStdKeyRightArrow, shift);
				if (gHeldUp)    W3dLayoutIDEFlecha(EStdKeyUpArrow, shift);
				if (gHeldDown)  W3dLayoutIDEFlecha(EStdKeyDownArrow, shift);
			}
		}
		return;
	  }
	}
	// CONSOLA: las flechas MANTENIDAS scrollean con aceleracion (comparte la rampa gIDEAccel: solo una activa a la vez)
	{ extern bool W3dLayoutConsolaActivo(); extern void W3dLayoutConsolaScroll(int, int);
	  if (W3dLayoutConsolaActivo()){
		gIDEAccel++;
		bool mover;
		if      (gIDEAccel == 1)   mover = true;
		else if (gIDEAccel < 10)   mover = false;
		else if (gIDEAccel < 25)   mover = (gIDEAccel % 3 == 0);
		else                       mover = true;
		if (mover){
			TInt pasos = (gIDEAccel > 45) ? 2 : 1;
			for (TInt p = 0; p < pasos; p++) W3dLayoutConsolaScroll(dx, dy);
		}
		return;
	  }
	}
	// MOUSE VIRTUAL visible: las flechas mueven el CURSOR como un mouse (no la camara).
	// Durante un transform 3D/de malla NO: ahi las flechas mueven la seleccion. El de KEYFRAMES es al reves: lo
	// maneja el CURSOR (event_mouse_motion -> DopeMoveApply, el mismo camino que arrastrar con el mouse en PC), asi
	// que ahi las flechas tienen que seguir moviendolo o el transform no se puede manejar con nada.
	if ((mouseVisible && !W3dNewTransformActive()) || DopeXformActivo()){   // (keyframes: aunque el cursor este oculto)
		// arranca LENTO (2px/frame) y acelera al mantener (hasta 9px) -> control fino al inicio, rapido al cruzar
		int spd = (gMouseAccel < 6) ? 2 : (gMouseAccel < 14) ? 4 : (gMouseAccel < 24) ? 6 : 9;
		gMouseAccel++;
		W3dMouseMoverFlechas(dx * spd, dy * spd);
		return;
	}
	// EDITOR UV activo (sin mouse): flechas MANTENIDAS = paneo CONSTANTE suave; 0 + arriba/abajo = ZOOM.
	if (W3dLayoutUVNav(dx, dy, g0Held)){ if (g0Held) g0ArrowUsed = ETrue; return; }
	// TIMELINE activo (sin mouse): izq/der = mover el frame (scrub); 0 + arriba/abajo = zoom; * + flechas = paneo.
	if (W3dLayoutTimelineNav(dx, dy, g0Held, gStarHeld)){ if (g0Held) g0ArrowUsed = ETrue; return; }
	if (!W3dLayout3DActivo()) return;
	// MODIFICADORES de camara del keypad (mantener tecla + flechas), prioridad sobre orbit/transform:
	if (g0Held){ if (dy != 0) W3dNewZoom(-dy); g0ArrowUsed = ETrue; return; }   // 0 + arriba/abajo = ZOOM
	if (gStarHeld){ W3dNewPan(dx * 8, dy * 8); return; }                        // * + flechas = PANEO
	// # + flechas = VISTAS POR CUADRANTE: lo aplica el KEY-DOWN (una por pulsacion). Aca no va nada: esto corre
	// cada frame y te haria girar la vista sin control.
	if (W3dNewTransformActive()){
		// mover: paso fino; rotar/escalar: el DOBLE (se sentian lentos)
		const TInt d = (W3dNewTransformModo() == 1) ? 4 : 8;
		W3dNewTransformMove(dx * d, dy * d);
	} else {
		const TInt o = 8;          // orbita suave por frame
		W3dNewOrbit(dx * o, dy * o);
	}
}

// MODO JUEGO: mapea el scancode del keypad del N95 al nombre de tecla que leen los scripts lua
// (tecla/teclaApretada/stick). El D-pad navega el menu y mueve la pala (1 jugador usa arriba/abajo);
// el centro (select) se trata aparte como enter+espacio. 0 = no es tecla de juego (sigue al editor).
static const char* W3dJuegoNombreTecla(TInt sc){
	switch (sc){
		case EStdKeyUpArrow:    return "arriba";
		case EStdKeyDownArrow:  return "abajo";
		case EStdKeyLeftArrow:  return "izquierda";
		case EStdKeyRightArrow: return "derecha";
		case EStdKeyDevice3:    // centro del D-pad (select)
		case EStdKeyEnter:      return "enter";
		case 164:               return "softizq"; // softkey IZQUIERDO: reservado para juegos (solo dentro de un viewport de juego)
		case 165:               return "softder"; // softkey DERECHO: idem (en pausa sigue alternando el cursor virtual)
		case '1':               return "1";       // digitos del keypad: van al juego durante el play (tecla("1").."tecla("6"))
		case '2':               return "2";
		case '3':               return "3";
		case '4':               return "4";       // 4/5/6 tambien son del juego (armas del demo GTA:
		case '5':               return "5";       // 4 = arma anterior, 5 = arma siguiente); en pausa
		case '6':               return "6";       // siguen siendo los atajos del editor
		default:                break;
	}
	// las LETRAS de un teclado QWERTY (E71, E72, N97...): el jugador 2 de la pantalla dividida juega con ellas.
	// (El scancode de una letra es su mayuscula ASCII; el script las lee en minuscula, como en la PC)
	static const char* const kLetras[26] = { "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q",
	                                         "r","s","t","u","v","w","x","y","z" };
	if (sc >= 'A' && sc <= 'Z') return kLetras[sc - 'A'];
	return 0;
}

TKeyResponse CWhisk3DContainer::OfferKeyEventL( const TKeyEvent& aKeyEvent,TEventCode aType ){
	// EEventKey (auto-repeat) se ignora: las flechas mantenidas las aplica el
	// loop de frame (gHeld* + AplicarFlechas3D), no el repeat lento del telefono.
	if (aType == EEventKey) return EKeyWasConsumed;
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
				if (!LayoutToggleEditMode()) LayoutCiclarViewportActivo(+1);
				return EKeyWasConsumed;
			}
			// VERDE + '0' = MODO JUEGO PURO (pedido del dueno): el 3D del juego a
			// pantalla completa y CERO editor (menu/headers/overlays/estadisticas),
			// para medir el rendimiento REAL. Re-apretar restaura todo. gGreenUsado
			// evita que al soltar la VERDE se cicle el viewport (mismo patron que
			// VERDE+OK y VERDE+flecha).
			if (gGreenHeld && sc == '0'){
				gGreenUsado = ETrue;
				extern void LayoutJuegoPuroToggle();
				LayoutJuegoPuroToggle();
				return EKeyWasConsumed;
			}
			// MODO JUEGO: el D-pad + select van DIRECTO a los scripts (tecla/teclaApretada), con prioridad
			// sobre el ruteo del editor (que se come las flechas para orbita/cursor). Sin esto el juego cargaba
			// pero no respondia a nada en el N95. Cede a popup/menu abiertos (escotilla: softkeys siguen libres).
			if (AnimEsJuego && PlayAnimation && !gGreenHeld && W3dLayoutJuegoViewportActivo() && !W3dLayoutPopupActivo() && !W3dLayoutMenuAbierto()){
				const char* jn = W3dJuegoNombreTecla(sc);
				if (jn){
					extern void W3dScriptTecla(const char*, bool);
					W3dScriptTecla(jn, true);
					if (sc == EStdKeyDevice3 || sc == EStdKeyEnter) W3dScriptTecla("espacio", true); // select cuenta como enter Y espacio
					return EKeyWasConsumed;
				}
				// 7/8/9/0/*/# SON DEL JUEGO mientras corre (pedido del dueno: el 8
				// cambiaba camara<->viewport en pleno gameplay). Van a los scripts
				// por nombre y el editor NO los ve; en PAUSA vuelven a ser los
				// atajos de siempre (extrude/tecla8/orbital/zoom/paneo/vistas).
				if (sc == '7' || sc == '8' || sc == '9' || sc == '0' ||
				    sc == '*' || sc == EStdKeyHash){
					char jnom[2]; jnom[0] = (char)(sc == EStdKeyHash ? '#' : sc); jnom[1] = 0;
					extern void W3dScriptTecla(const char*, bool);
					W3dScriptTecla(jnom, true);
					return EKeyWasConsumed;
				}
			}
			// popup modal (File browser) o barra de menu abierta: se quedan con
			// TODO el keypad (flechas mueven el foco/menu, OK selecciona, soft
			// IZQ=164=Cancelar/cerrar). Asi se navegan SIN mouse BT y los
			// digitos no tocan la escena 3D.
			// ...SALVO un campo del popup en edicion (OK sobre un valor del panel de la ultima transformacion, el texto
			// de Buscar del IDE): ahi el keypad ESCRIBE (bloque T9 de abajo) y el popup sigue abierto. Termina con OK.
			if ((W3dLayoutPopupActivo() && !W3dT9Activo()) || W3dLayoutMenuAbierto()){
				extern GLshort mouseX; extern GLshort mouseY;
				// soft DER (165) sobre un MENU desplegable (no un popup modal) -> lo CIERRA, igual que el soft
				// IZQ. (En un popup -ej color picker- 165 sigue siendo Aceptar/OK.) Pedido Dante.
				if (sc == 165 && W3dLayoutMenuAbierto() && !W3dLayoutPopupActivo()){
					W3dLayoutTecla(164, mouseX, mouseY); // 164 = Cancel = cierra el menu
					return EKeyWasConsumed;
				}
				// POPUP (no menu): marca la flecha MANTENIDA -> AplicarFlechas3D la REPITE cada frame para
				// ajustar el valor (color picker R/G/B/A o circulo/value). El 1er press de abajo navega/ajusta.
				if (W3dLayoutPopupActivo()){
					if (sc == EStdKeyLeftArrow)       gHeldLeft = ETrue;
					else if (sc == EStdKeyRightArrow) gHeldRight = ETrue;
					else if (sc == EStdKeyUpArrow)    gHeldUp = ETrue;
					else if (sc == EStdKeyDownArrow)  gHeldDown = ETrue;
				}
				// '*' cierra el selector de simbolos (otra forma de salir sin elegir nada)
				if (sc == '*'){ extern bool SimbolosActivo(); extern void SimbolosCerrar(); if (SimbolosActivo()){ SimbolosCerrar(); return EKeyWasConsumed; } }
				W3dLayoutTecla(sc, mouseX, mouseY);
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
			// pasar a sus handlers de abajo (gGreenHeld/gLapizHeld) asi andan mientras se edita texto. El bug era que
			// el T9 se los comia -> ni resize, ni copiar/pegar, ni seleccion (gLapizHeld nunca se seteaba).
			if (W3dT9Activo() && sc != EStdKeyYes && sc != EStdKeyLeftShift && sc != EStdKeyRightShift){
				// IDE editando codigo: el T9 escribe en el editor de scripts (no en un TextField). Flechas = cursor,
				// OK = nueva linea, C = backspace, soft-izq = salir a la barra (script/save/refresh).
				if (W3dLayoutIDEEditando()){
					if ((sc >= '0' && sc <= '9') || sc == EStdKeyHash){ W3dT9Tecla(sc, ETrue); return EKeyWasConsumed; }
					if (sc == EStdKeyBackspace){ W3dT9Reset(); W3dLayoutIDEBorrar(); return EKeyWasConsumed; }
					// las flechas se MANTIENEN (gHeld*) y AplicarFlechas3D mueve el cursor cada frame con aceleracion
					// (arranca lento, acelera). shift(lapiz)+flecha selecciona. Single-tap = 1 movimiento.
					if (sc == EStdKeyLeftArrow){  W3dT9Reset(); gHeldLeft = ETrue;  return EKeyWasConsumed; }
					if (sc == EStdKeyRightArrow){ W3dT9Reset(); gHeldRight = ETrue; return EKeyWasConsumed; }
					if (sc == EStdKeyUpArrow){    W3dT9Reset(); gHeldUp = ETrue;    return EKeyWasConsumed; }
					if (sc == EStdKeyDownArrow){  W3dT9Reset(); gHeldDown = ETrue;  return EKeyWasConsumed; }
					if (sc == EStdKeyDevice3 || sc == EStdKeyEnter){ W3dT9Reset(); W3dLayoutIDEEnter(); return EKeyWasConsumed; }
					// shift(lapiz) + softkey: izquierda = COPIAR la seleccion, derecha = PEGAR (portapapeles del sistema)
					if (gLapizHeld && sc == 164){ W3dT9Reset(); W3dLayoutIDECopiar(); return EKeyWasConsumed; }
					if (gLapizHeld && sc == 165){ W3dT9Reset(); W3dLayoutIDEPegar();  return EKeyWasConsumed; }
					if (sc == 164){ W3dT9Reset(); W3dLayoutIDESalirEdicion(); return EKeyWasConsumed; }
					if (sc == '*'){ W3dT9Reset(); { extern void SimbolosAbrir(); SimbolosAbrir(); } return EKeyWasConsumed; } // selector de simbolos
					return EKeyWasConsumed; // el resto se consume: no toca la escena
				}
				if ((sc >= '0' && sc <= '9') || sc == EStdKeyHash){ W3dT9Tecla(sc, ETrue); return EKeyWasConsumed; }
				if (sc == EStdKeyBackspace){ W3dT9Reset(); W3dTextFieldChar(8); return EKeyWasConsumed; }
				if (sc == EStdKeyLeftArrow || sc == EStdKeyRightArrow){ W3dT9Reset(); W3dLayoutTeclaPanel(sc); return EKeyWasConsumed; }
				if (sc == EStdKeyDevice3 || sc == EStdKeyEnter){ W3dT9Reset(); W3dLayoutTeclaPanel(EStdKeyDevice3); return EKeyWasConsumed; }
				if (sc == 164){ W3dT9Reset(); W3dLayoutTeclaPanel(EStdKeyEscape); return EKeyWasConsumed; } // soft-IZQ = cancelar
				if (sc == '*'){ if (W3dNumEditActivo()){ W3dT9Reset(); W3dTextFieldChar('.'); } else { W3dT9Reset(); { extern void SimbolosAbrir(); SimbolosAbrir(); } } return EKeyWasConsumed; } // '.' en numerico; selector de simbolos en texto
				return EKeyWasConsumed; // mientras se edita, ninguna otra tecla toca la escena
			}
			// soft IZQ: VERDE + izquierda = PAUSA el juego (combo DELIBERADO). El softkey SOLO no pausa: seria un
			// desastre por inercia (salis de un menu en el timeline y pausas sin querer). En un viewport de juego la
			// izquierda va al juego (W3dJuegoNombreTecla, se captura arriba); en el resto abre/cierra la barra de menu.
			if (sc == 164){
				if (gGreenHeld){
					gGreenUsado = ETrue;   // que soltar el verde NO cicle el viewport
					// verde + izquierda = PLAY/PAUSA toggle (pausar Y reanudar con el mismo combo). En JUEGO y tambien
					// en ANIMACION normal (pedido del usuario): togglea PlayAnimation en los dos; solo suelta las
					// teclas del juego si es juego (en animacion no hay input de juego que soltar).
					if (PlayAnimation){
						if (AnimEsJuego){ extern void W3dScriptSoltarTeclas(); W3dScriptSoltarTeclas(); }
						PlayAnimation = false;
					} else PlayAnimation = true;   // reanudar
					return EKeyWasConsumed;
				}
				// IDE en modo barra: el soft-izq VUELVE a editar texto (toggle con el soft-izq que en edicion abre la
				// barra). Sin esto quedabas atascado en la barra (izquierda navegaba/abria el menu de tipo).
				if (W3dLayoutIDEEnBarra()){ W3dLayoutIDEVolverEdicion(); return EKeyWasConsumed; }
				W3dLayoutToggleBarra();
				return EKeyWasConsumed;
			}
			// soft DER fuera de un popup: alterna el cursor virtual (lo que
			// antes hacia el softkey "Back", perdido al vaciar el CBA).
			if (sc == 165){
				iWhisk3D->SetMouse();
				return EKeyWasConsumed;
			}
			// tecla VERDE de llamada: cicla el viewport activo (borde verde).
			// Sin mouse es la unica forma de elegir viewport. (Si el sistema
			// se la come, hay que CaptureKey EStdKeyYes en el window group.)
			if (sc == EStdKeyYes){
				// mantener: + flechas agranda/achica el viewport. Tap (sin flechas)
				// al soltar: cicla el viewport activo (ver EEventKeyUp).
				gGreenHeld = ETrue;
				gGreenUsado = EFalse;
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
				if (W3dLayoutMenuAbierto()){
					if (W3dLayoutTecla(sc, 0, 0)) return EKeyWasConsumed;
				}
				// LOOP CUT activo: las flechas eligen orientacion / cortes / slide (single-press)
				if (LoopCutActivo()){
					int d = (sc == EStdKeyLeftArrow) ? 0 : (sc == EStdKeyRightArrow) ? 1 : (sc == EStdKeyUpArrow) ? 2 : 3;
					LoopCutTecla(d);
					return EKeyWasConsumed;
				}
				// LOOP SELECT (modal de direccion desde una cara): las flechas alternan el sentido
				if (LoopSelOrientando()){
					int d = (sc == EStdKeyLeftArrow || sc == EStdKeyUpArrow) ? 0 : 1;
					LoopSelTecla(d);
					return EKeyWasConsumed;
				}
				// C mantenido + flecha = UNDO (izq) / REDO (der): el "Ctrl+Z / Ctrl+Y" del telefono
				if (gCHeld){
					gCArrowUsed = ETrue; // que soltar la C NO abra el popup de borrar
					if (sc == EStdKeyLeftArrow)       UndoDeshacer();
					else if (sc == EStdKeyRightArrow) UndoRehacer();
					return EKeyWasConsumed;
				}
				// LAPIZ mantenido + flecha = navegar la seleccion (sub-elemento de malla; objeto = ciclado). SOLO
				// sin mouse virtual: con el mouse visible el lapiz es SHIFT (para shift+click = seleccion multiple) y
				// las flechas deben MOVER EL CURSOR (uno instintivamente manteniene shift y mueve con flechas). Sin
				// mouse virtual, ahi si las flechas navegan la seleccion (todo/nada/siguiente/anterior).
				if (gLapizHeld && !mouseVisible){
					gLapizArrowUsed = ETrue;
					// TIMELINE activo: el lapiz navega KEYFRAMES, no la seleccion de objetos (cambiar de objeto
					// mientras trabajas la animacion es lo ultimo que queres). DopeNavTecla devuelve false si el
					// timeline no esta activo -> sigue el camino de siempre.
					int nav = (sc == EStdKeyRightArrow) ? 1 : (sc == EStdKeyLeftArrow) ? 2 :
					          (sc == EStdKeyUpArrow) ? 3 : 4;
					if (DopeNavTecla(nav)) return EKeyWasConsumed;
					bool enEdit;
					if (sc == EStdKeyRightArrow)      enEdit = EditSelAvanzar(1, true);   // mantiene + siguiente
					else if (sc == EStdKeyLeftArrow)  enEdit = EditSelAvanzar(-1, true);  // mantiene + anterior
					else if (sc == EStdKeyUpArrow)    enEdit = EditSelTodoToggle();        // todo / nada
					else                              enEdit = EditSelToggleActual();      // togglea el activo
					if (!enEdit && (sc == EStdKeyRightArrow || sc == EStdKeyDownArrow) && !W3dNewTransformActive()) W3dNewCycleSelect();
					return EKeyWasConsumed;
				}
				// # mantenido + flecha = VISTA POR CUADRANTE (el 1/3/7 del numpad que el telefono no tiene). Va en el
				// key-down: es UNA por pulsacion.
				if (gHashHeld){
					gHashArrowUsed = ETrue;   // hubo flecha: soltar el # ya no togglea la perspectiva
					const TInt vdx = (sc == EStdKeyRightArrow) ? 1 : (sc == EStdKeyLeftArrow) ? -1 : 0;
					const TInt vdy = (sc == EStdKeyDownArrow)  ? 1 : (sc == EStdKeyUpArrow)   ? -1 : 0;
					W3dVistaCuadranteNav(vdx, vdy);
					return EKeyWasConsumed;
				}
				// TIMELINE + arriba/abajo (sin modificadores) = saltar al keyframe siguiente/anterior, igual que en
				// PC. Aca y no en la repeticion de flecha mantenida: es uno por pulsacion. Con 0 (zoom), * (paneo) o
				// VERDE (redimensionar) mantenidos, las flechas son de ellos.
				// Se pregunta por el TIMELINE y no "no es el 3D": el resto de los viewports (Properties, outliner)
				// todavia no overridea event_key_down en el telefono, asi que mandarles la tecla la perderia en el
				// no-op de la base en vez de dejarla seguir a la navegacion del panel.
				if (!g0Held && !gStarHeld && !gGreenHeld && !mouseVisible && W3dLayoutTimelineActivo() &&
				    (sc == EStdKeyUpArrow || sc == EStdKeyDownArrow)){
					if (W3dLayoutTeclaViewport(sc)) return EKeyWasConsumed;
				}
				// TIMELINE + izquierda/derecha: el TOQUE mueve UN cuadro ya (como en PC); mantenida sigue el scrub,
				// que arranca despues de un ratito (W3dTimelineFlechaPulsada). Sigue abajo para marcarla mantenida.
				if (!g0Held && !gStarHeld && !gGreenHeld && !mouseVisible && W3dLayoutTimelineActivo() &&
				    (sc == EStdKeyLeftArrow || sc == EStdKeyRightArrow)){
					extern void W3dTimelineFlechaPulsada();
					W3dLayoutTeclaViewport(sc);
					W3dTimelineFlechaPulsada();
				}
				// 3D con FOCO DE TRANSPORTE (Stop/Play): la flecha NO orbita, cae al dispatch del panel (linea 618)
				// para navegar Stop<->Play, igual que el Timeline con foco suelta las flechas. De ahi el && !W3dLayoutFocoTransporte().
				// UV con foco en "Edit" (W3dLayoutFocoUVEditar): la flecha NO panea, cae al dispatch del panel
				// para seguir navegando la barra (mismo esquema que el foco de transporte del 3D).
				if (gGreenHeld || (W3dLayout3DActivo() && !W3dLayoutFocoTransporte()) || mouseVisible || (W3dLayoutUVActivo() && !W3dLayoutFocoUVEditar()) || W3dLayoutTimelineActivo() || W3dLayoutConsolaActivo()){ // 3D=orbita, UV/timeline=paneo/scrub, mouse=cursor, consola=scroll
					if (gGreenHeld) gGreenUsado = ETrue; // verde+flecha = resize (no ciclar)
					if (sc == EStdKeyLeftArrow)       gHeldLeft = ETrue;
					else if (sc == EStdKeyRightArrow) gHeldRight = ETrue;
					else if (sc == EStdKeyUpArrow)    gHeldUp = ETrue;
					else                              gHeldDown = ETrue;
					return EKeyWasConsumed;
				}
				// sin verde ni 3D activo: la flecha va al PANEL activo (propiedades/
				// outliner) por teclado, sin mouse. Single-press (no se mantiene).
				if (W3dLayoutTeclaPanel(sc)) return EKeyWasConsumed;
			}
			if (sc == '0' && W3dNewTransformActive() && W3dNewTransformModo() == 2){
				W3dNewToggleOrbital(); // 0 durante rotacion: trackball <-> orbital
				return EKeyWasConsumed;
			}
			// "1" sobre el outliner (sin transform 3D en curso) = entrar/confirmar el modo MOVER:
			// reordenar/reparentar el objeto activo sin mouse. Clave en el N95 para poner las
			// lamparas ANTES de los objetos que iluminan. Va antes del 1/2/3 del transform 3D.
			if (sc == '1' && W3dOutlinerActivo() && !W3dNewTransformActive()){
				W3dOutlinerMoverToggle();
				return EKeyWasConsumed;
			}
			// "2"/"3" sobre el outliner (sin transform 3D en curso): 2 = ocultar/mostrar, 3 = visibilidad de
			// RENDER del objeto activo. Van ANTES del 1/2/3 del transform 3D (que en el outliner no aplica).
			if (sc == '2' && W3dOutlinerActivo() && !W3dNewTransformActive()){
				W3dOutlinerToggleVisible();
				return EKeyWasConsumed;
			}
			if (sc == '3' && W3dOutlinerActivo() && !W3dNewTransformActive()){
				W3dOutlinerToggleRender();
				return EKeyWasConsumed;
			}
			if (sc == '1' || sc == '2' || sc == '3'){
				// El keypad del telefono no tiene letras: el 1/2/3 ES como se tipea g/r/s. Si el viewport activo no
				// es el 3D, la tecla va a EL, como en PC (el timeline transforma sus keyframes con las MISMAS g/r/s,
				// no con un camino propio). Solo el 3D sigue con W3dNewTransform*: su event_key_down todavia no
				// compila en el telefono.
				if (!W3dLayout3DActivo()){
					const TInt t = (sc == '1') ? W3dK_G : (sc == '2') ? W3dK_R : W3dK_S;
					if (W3dLayoutTeclaViewportDirecta(t)) return EKeyWasConsumed;
				}
				// solo con el mouse sobre el 3D (el hover decide, como PC)
				extern TBool W3dHayMouseBT();
				extern GLshort mouseX; extern GLshort mouseY;
				if (W3dHayMouseBT() && !W3dLayoutMouseSobre3D(mouseX, mouseY)){
					return EKeyWasConsumed;
				}
				if (W3dNewTransformActive()){
					W3dNewTransformEje(sc - '1'); // 1/2/3 = eje X/Y/Z (cicla orient, NO reinicia)
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
			// cut aun sin estar editando una malla. El gate (Edit Mode vs camara) vive en W3dLayoutTecla8.
			if (sc == '8'){ W3dLayoutTecla8(); return EKeyWasConsumed; }
			if (sc == '9'){ if (!W3dOutlinerActivo()) W3dLayoutLockOrbit(); return EKeyWasConsumed; } // 9 = bloquear orbital (NO en el outliner)
			if (sc == '4'){ return EKeyWasConsumed; } // 4 = nada
			// 5 sobre el outliner en una vista de RECURSOS = marcar/desmarcar (seleccion multiple sin mouse)
			if (sc == '5' && W3dOutlinerActivo() && !W3dNewTransformActive()){ W3dOutlinerMarcarToggle(); return EKeyWasConsumed; }
			if (sc == '5'){ return EKeyWasConsumed; } // 5 = nada
			// con mouse BT activo el teclado navega como en PC: las
			// flechas/OK van al panel bajo el cursor (no mueven el mouse)
			{
				extern TBool W3dHayMouseBT();
				extern GLshort mouseX;
				extern GLshort mouseY;
				if (W3dHayMouseBT() && W3dLayoutTecla(sc, mouseX, mouseY)){
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
				gLapizHeld = ETrue; gLapizArrowUsed = EFalse; gLapizDownTick = User::NTickCount();
				// editando un script: abajo aparece la barra de las softkeys "Copiar | Pegar" mientras se mantiene
				{ extern void IDELapizSostenido(bool); if (W3dLayoutIDEEditando()) IDELapizSostenido(true); }
				return EKeyWasConsumed;
			}
			// OK del telefono: acepta el transform o la edicion de propiedad
			if (sc == EStdKeyDevice3){
				if (gGreenHeld){ gGreenUsado = ETrue; LayoutMaximizar(); return EKeyWasConsumed; } // verde + OK = maximizar/restaurar el viewport
					if (LoopCutActivo()){ LoopCutClickIzq(mouseX, mouseY); return EKeyWasConsumed; } // OK = aceptar/avanzar fase
				if (LoopSelOrientando()){ LoopSelConfirm(); return EKeyWasConsumed; } // OK = acepta el loop (modal de direccion)
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
					LShiftPressed = (gLapizHeld || iWhisk3D->iShiftPressed) ? true : false; // lapiz mantenido = shift
					if (gLapizHeld) gLapizArrowUsed = ETrue; // que soltar el lapiz NO cicle la seleccion
					// El OK es un CLICK DE VERDAD al viewport bajo el cursor: el mismo down que manda el mouse en PC.
					// Asi el timeline se entera (curvas, keyframes, handles) sin que nadie reimplemente el click
					// afuera, y mantener el OK queda como arrastre solo (el cursor ya emite event_mouse_motion).
					// El 3D es la excepcion: su button_left() todavia es stub en el telefono -> su pick va aparte.
					if (!W3dLayoutMouseSobre3D(mouseX, mouseY) && W3dLayoutClickViewport(mouseX, mouseY)){
						return EKeyWasConsumed;
					}
					iWhisk3D->ClickSelect();
					return EKeyWasConsumed;
				}
				// OK al panel ACTIVO (propiedades/outliner) por teclado, sin mouse
				if (W3dLayoutTeclaPanel(EStdKeyDevice3)){
					return EKeyWasConsumed;
				}
				if (W3dLayoutTecla(EStdKeyDevice3, 0, 0)){
					return EKeyWasConsumed;
				}
			}
			// tecla C: primero CANCELA (transform activo o edicion de
			// propiedad); si no hay nada que cancelar, borra el activo
			if (sc == EStdKeyBackspace){
				if (LoopCutActivo()){ LoopCutClickDer(); return EKeyWasConsumed; } // C = centra el corte (0.5) + confirma
				if (PickPathGuiado()){ LayoutPickPathCancelar(); return EKeyWasConsumed; } // C = cancela el modo guiado
					if (GuiadoUnClickActivo()){ LayoutGuiadoCancelar(); return EKeyWasConsumed; } // C = cancela Select Linked / Loop Select guiados
				if (W3dNewTransformActive()){
					W3dNewTransformEnd(ETrue);
					return EKeyWasConsumed;
				}
				// outliner en modo mover: C/backspace CANCELA (restaura la posicion original en el arbol).
				// Solo consume si realmente estaba moviendo (sino LayoutTeclaPanelActivo devuelve false).
				if (W3dLayoutTeclaPanel(EStdKeyBackspace)){
					return EKeyWasConsumed;
				}
				if (W3dLayoutTecla(EStdKeyBackspace, 0, 0)){
					return EKeyWasConsumed;
				}
				// el TIMELINE activo con keyframes / curvas elegidos: la C los BORRA (no al objeto activo)
				{ extern bool TimelineHaySeleccion();
				  if (W3dLayoutTimelineActivo() && TimelineHaySeleccion() && W3dLayoutTeclaViewport(EStdKeyBackspace)) return EKeyWasConsumed; }
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
			if (W3dT9Activo() && (usc == EStdKeyHash || (usc >= '0' && usc <= '9'))){ W3dT9Tecla(usc, EFalse); return EKeyWasConsumed; }
			// MODO JUEGO: soltar el D-pad + select -> los scripts (fin del "mantenido" que lee tecla()).
			if (AnimEsJuego && PlayAnimation && W3dLayoutJuegoViewportActivo() && !W3dLayoutPopupActivo() && !W3dLayoutMenuAbierto()){
				const char* jn = W3dJuegoNombreTecla(usc);
				if (jn){
					extern void W3dScriptTecla(const char*, bool);
					W3dScriptTecla(jn, false);
					if (usc == EStdKeyDevice3 || usc == EStdKeyEnter) W3dScriptTecla("espacio", false);
					return EKeyWasConsumed;
				}
				// el par del key-down: 7/8/9/0/*/# reservadas al juego mientras corre
				if (usc == '7' || usc == '8' || usc == '9' || usc == '0' ||
				    usc == '*' || usc == EStdKeyHash){
					char jnom[2]; jnom[0] = (char)(usc == EStdKeyHash ? '#' : usc); jnom[1] = 0;
					extern void W3dScriptTecla(const char*, bool);
					W3dScriptTecla(jnom, false);
					return EKeyWasConsumed;
				}
			}
			if (usc == EStdKeyYes){
				if (!gGreenUsado){
					W3dLayoutCiclarViewport(1); // tap = ciclar viewport
					// al cambiar de viewport durante el juego, soltar las teclas del juego: si se ciclo con
					// una flecha mantenida, que no quede la pala "trabada" moviendose sola.
					if (AnimEsJuego && PlayAnimation){ extern void W3dScriptSoltarTeclas(); W3dScriptSoltarTeclas(); }
				}
				gGreenHeld = EFalse;
				return EKeyWasConsumed;
			}
			if (usc == EStdKeyNo){ return EKeyWasConsumed; } // tecla ROJA (end): consumida, no cierra la app
			// soltar el OK = soltar el boton del mouse: cierra el arrastre (y su undo) del viewport que lo recibio
			if (usc == EStdKeyDevice3 || usc == EStdKeyEnter){
				if (mouseVisible) W3dLayoutSoltarViewport(mouseX, mouseY);
				return EKeyWasConsumed;
			}
			if (usc == '0'){ // 0 soltada: tap SIN flecha = enfocar (como antes); con flecha ya hizo zoom
				if (g0Held && !g0ArrowUsed) W3dNewEnfocar();
				g0Held = EFalse;
				return EKeyWasConsumed;
			}
			if (usc == '*'){ gStarHeld = EFalse; return EKeyWasConsumed; } // * soltada: fin del paneo
			if (usc == EStdKeyHash){ // soltada SIN haber tocado una flecha = TAP -> ortografica <-> perspectiva
				if (gHashHeld && !gHashArrowUsed) W3dVistaPerspectivaToggle();
				gHashHeld = EFalse;
				return EKeyWasConsumed;
			}
			if (usc == EStdKeyLeftShift || usc == EStdKeyRightShift){ // LAPIZ (solo shift ahora)
				{ extern bool LShiftPressed; LShiftPressed = false; }
				TUint dt = User::NTickCount() - gLapizDownTick; // lapiz soltado: corto + sin flecha = ciclar
				{ extern void IDELapizSostenido(bool); IDELapizSostenido(false); }   // se va la barra Copiar | Pegar
				// (editando un script el lapiz es solo shift: no cicla la seleccion de la escena)
				if (gLapizHeld && !gLapizArrowUsed && dt < 500 && !W3dNewTransformActive() && !mouseVisible && !W3dLayoutIDEEditando()){
					// en el TIMELINE: saca el keyframe actual y pasa al siguiente. En el resto: cicla el activo.
					if (!DopeNavTecla(0)) W3dNewCycleSelect();
				} // con mouse virtual el lapiz es shift (no cicla)
				gLapizHeld = EFalse;
				return EKeyWasConsumed;
			}
			// C soltada: tap SIN flecha = borrar (edit mode -> menu Delete en el cursor; object mode ->
			// popup de confirmar). Si se uso flecha (undo/redo) NO hace nada (el undo/redo ya paso).
			if (usc == EStdKeyBackspace){
				if (gCHeld && !gCArrowUsed){
					extern GLshort mouseX; extern GLshort mouseY;
					// (en una vista de RECURSOS del outliner borra los recursos elegidos, no la escena)
					if (!LayoutDeleteEdit(mouseX, mouseY) && !W3dOutlinerRecursosBorrar()) AbrirConfirmarBorrado();
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
    w3dEngine::MandosCerrar();   // (apaga la vibracion y suelta el driver de los joysticks)
    delete iPeriodic;

    if ( iWhisk3D ){
        delete iWhisk3D;
    }
    delete iInputHandler;
    // el monitor HID es un CActive: si no se borra queda escuchando despues
    // de destruir el container
    delete iHidMonitor;
    W3dLayoutDestroy();

    W3dPantallasDestroy();   // (sus superficies comparten el contexto: antes de destruirlo)
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
static TUint gPfRender = 0, gPfSwap = 0, gPfCuadros = 0, gPfDesde = 0;

int CWhisk3DContainer::DrawCallBack( TAny* aInstance )
    {
    CWhisk3DContainer* instance = (CWhisk3DContainer*) aInstance;
    instance->iFrame++;

    // APERTURA DE PROYECTO DIFERIDA (tarjeta Archivo / abrir un .w3d en el explorador ->
    // AbrirProyectoDesde solo encola g_proyAbrirPendiente). Se procesa ACA, con el stack
    // limpio, igual que MainLoopFrame en PC. Sin esto el .w3d elegido quedaba en la cola y
    // NO se abria en el N95 (el boton que la pidio vive en el layout que AbrirProyectoAhora
    // destruye, por eso el open es diferido y no directo).
    {
        extern std::string g_proyAbrirPendiente;
        extern void AbrirProyectoAhora(const std::string&);
        if (!g_proyAbrirPendiente.empty()) {
            std::string r = g_proyAbrirPendiente;
            g_proyAbrirPendiente.clear();
            AbrirProyectoAhora(r);
        }
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
    { extern void NotificacionesTick(float); NotificacionesTick(timeSecs - instance->iLastFrameTimeSecs); }

    // Set the current time to be the last frame time for the upcoming frame
    instance->iLastFrameTimeSecs = timeSecs;

    // input de usuario (desde donde se cicla, no dentro de Whisk3D): el
    // Escape del telefono cancela la transformacion activa del modelo nuevo
    if (instance->iInputHandler->IsInputPressed( EEscape )){
        W3dNewTransformEnd(ETrue);
    }

    // pantallas externas: conectar/desconectar (revisa 1 vez por segundo)
    W3dPantallasTick();

    // T9: confirmar el caracter multi-tap a medio elegir cuando vence el timeout (1 seg sin apretar)
    W3dT9Tick();
    // la PULSACION LARGA tactil (agarrar una fila del outliner, la lupa del IDE): se mide con el reloj, como en PC
    ControlesTick();
    // los JOYSTICKS Bluetooth (driver bthid-s60): conectar/desconectar + su estado, como los mandos de SDL en PC
    MandosSymbianTick();
    // el mouse Bluetooth: el apretar que esperaba por si era el boton del medio (izquierdo + derecho juntos), en la
    // pantalla donde esta el cursor (el telefono o la externa)
    extern void W3dMouseTick();
    W3dMouseTick();
    // se perdio el foco con dedos apoyados (FocusChanged): se levantan aca, fuera de cualquier otro evento
    if (instance->iSoltarDedos) { instance->iSoltarDedos = EFalse; w3dEngine::TactilSoltarTodo(); }
    // flechas MANTENIDAS: aplicar CADA frame (orbitar/mover/rotar/escalar
    // fluido con el teclado, sin depender del auto-repeat lento del telefono)
    AplicarFlechas3D();

    // CARGA DIFERIDA de texturas del import: 1 por frame (decode+upload). Prende g_redraw al cargar una -> el
    // modelo aparece enseguida y las texturas entran solas. No-op si no hay pendientes. (Va ANTES del check de
    // g_redraw para que corra aunque la escena este quieta.)
    { extern void CargarTexturasPendientes(); CargarTexturasPendientes(); }

    // ANIMACION: materiales/UV animados cada frame + avance del frame en PLAY al ritmo de AnimFPS (independiente de
    // los fps de la UI). En PC vive en MainLoopFrame; en el N95 FALTABA -> el play quedaba "clavado" en el frame 1 y
    // los materiales animados no corrian. (El avance de las VERTEX-anims -VertexAnimation.cpp, SI en el .mmp- se hace
    // abajo, en la rama de juego, con UpdateAnimations(dtSim); el skinning/pose de esqueleto siguen stub.)
    // PAUSA: las FRUTAS y los BANDERINES son billboards de tira UV (AnimacionUV). En PAUSA del juego
    // (AnimEsJuego && !PlayAnimation) NO se avanzan los materiales animados ni las tiras UV -> quedan quietos,
    // igual que en PC (main.cpp:609). En PLAY (PlayAnimation) y en el editor normal (AnimEsJuego==false) el
    // gate da true y todo anima como antes. ActiveAnimKind!=3 = no pisar la edicion de vertex-anim del timeline.
    // (Las particulas ya estan gateadas por PlayAnimation dentro del bloque 'juego' de abajo: no van aca.)
    { extern bool TextFieldParpadeoTick(); if (TextFieldParpadeoTick()) g_redraw = true; }   // el cursor que parpadea
    { extern int ActiveAnimKind;
      if (!(AnimEsJuego && !PlayAnimation) && ActiveAnimKind != 3) {
          UpdateAnimatedMaterials();
          extern bool UpdateUVAnims(float); if (UpdateUVAnims(dtFrame)) g_redraw = true;
          extern bool UpdateFlipbooks(float); if (UpdateFlipbooks(dtFrame)) g_redraw = true;
          extern bool W3dOclusionTick(); if (W3dOclusionTick()) g_redraw = true;   // oclusion por nodos (.w3dnodos)
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
        extern bool SimHayScripts(); extern bool SimActiva(); extern void SimTickPlay(float);
        bool juego = AnimEsJuego && PlayAnimation && (SimActiva() || SimHayScripts());
        if (juego) {
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
                { extern unsigned int (*g_skinRelojMs)();
                  struct RelojN95 { static unsigned int Ms() { return (unsigned int)User::NTickCount(); } };
                  if (!g_skinRelojMs) g_skinRelojMs = RelojN95::Ms; }
                static TUint gPfSim=0, gPfAnim=0, gPfPvs=0, gPfPart=0, gPfN=0; TUint _pf;
                _pf=User::NTickCount(); SimTickPlay(dtSim);                                              gPfSim  += User::NTickCount()-_pf;
                // AVANCE de las vertex-anims (Crash y demas mallas animadas): en PC vive en main.cpp; en el N95 FALTABA.
                _pf=User::NTickCount(); { extern void UpdateAnimations(float); UpdateAnimations(dtSim); }  gPfAnim += User::NTickCount()-_pf;
                // PVS DEL ESCENARIO (oclusion del MAPA como el Crash original): el stepper que lee el nodo del riel y
                // APLICA el sector (override del index buffer por triangulo). En PC vive en main.cpp:617; en el N95 FALTABA.
                _pf=User::NTickCount(); { extern void W3dVisZonasTick(); W3dVisZonasTick(); }              gPfPvs  += User::NTickCount()-_pf;
                // PARTICULAS: simula + MATA las vivas (polvo, humo). En PC vive en main.cpp; en el N95 FALTABA.
                _pf=User::NTickCount(); { extern void W3dParticulasTick(float); W3dParticulasTick(dtSim); } gPfPart += User::NTickCount()-_pf;
                if (++gPfN >= 30) {
                    extern void w3dLogf(const char*, ...);
                    // CULLING: g_cullHijos* se acumulan por Culling::RenderHijos cada frame. El RATIO visibles/total dice
                    // si el frustum descarta mallas en el N95 (visibles<<total = anda; visibles~=total = dibuja TODO ->
                    // el frustum cull no corta y hay que ver por que). El profiler del render mide aparte (overlay stats).
                    extern int g_cullHijosTotal, g_cullHijosVisibles;
                    extern int g_luaScriptsActivos, g_luaScriptsTotal, g_renderCaras, g_renderDraws;
                    // SKINNING (acumulado por Mesh::RenderObject en los 30 frames): skin= ms promedio
                    // por frame; recalc= cuantos SkinearMesh recalcularon de verdad y cuantos cayeron
                    // al camino COMPLETO (el incremental deberia dejar casi todo en salpicado/nada).
                    extern int g_skinPerfMs, g_skinPerfSkins, g_skinPerfCompletos;
                    // TOP de scripts por ms (promedio del bloque de 30): dice CUAL lua se come el sim=
                    extern void SimLuaPerfTop(char*, int, int);
                    char topLua[96]; SimLuaPerfTop(topLua, sizeof(topLua), 30);
                    // GL: TODAS las llamadas al driver del ultimo cuadro (escena + HUD del juego + editor + GL crudo)
                    const w3dEngine::StatsDeCuadro& gc = w3dEngine::g_statCuadro;
                    // el render: promedio por cuadro dibujado y los fps de verdad (cuadros / tiempo entre lineas)
                    const TUint ahoraPf = User::NTickCount();
                    const TUint cuadrosPf = gPfCuadros ? gPfCuadros : 1;
                    const TUint fpsPf = (gPfDesde && ahoraPf > gPfDesde) ? (gPfCuadros * 1000u) / (ahoraPf - gPfDesde) : 0;
                    // (de ese render: cuanto es la escena armada por RUTINAS y cuanto el HUD; el resto, el editor y el GL suelto)
                    extern double g_rutinasMs, g_uiMs;
                    w3dLogf("[PERF] %u fps | render=%u (rutinas=%d hud=%d) swap=%u ms por cuadro", fpsPf, gPfRender / cuadrosPf,
                            (int)(g_rutinasMs / cuadrosPf + 0.5), (int)(g_uiMs / cuadrosPf + 0.5), gPfSwap / cuadrosPf);
                    g_rutinasMs = g_uiMs = 0.0;
                    gPfRender = gPfSwap = gPfCuadros = 0; gPfDesde = ahoraPf;
                    w3dLogf("[PERF] /30f logica: sim=%u anim=%u pvs=%u part=%u ms | skin=%d ms (recalc %d, completos %d) | lua %d/%d scripts [%s] | RENDER: %d caras %d draws | GL %d (esc %d ui %d ed %d) | CULLING: %d de %d hijos",
                            gPfSim/30, gPfAnim/30, gPfPvs/30, gPfPart/30,
                            g_skinPerfMs/30, g_skinPerfSkins/30, g_skinPerfCompletos/30,
                            g_luaScriptsActivos, g_luaScriptsTotal, topLua, g_renderCaras, g_renderDraws,
                            gc.total, gc.llamadas[w3dEngine::StatCatEscena] + gc.llamadas[w3dEngine::StatCatParticulas],
                            gc.llamadas[w3dEngine::StatCatUI], gc.llamadas[w3dEngine::StatCatEditor],
                            g_cullHijosVisibles/30, g_cullHijosTotal/30);
                    // el DESGLOSE de sim= por fase (SimJuego): fisica / hitbox / lua / animaciones / streaming
                    { extern int SimPerfDesglose(double*, bool); double d[5]; const int nt = SimPerfDesglose(d, true);
                      const double k = nt > 0 ? 1.0 / nt : 0.0;
                      w3dLogf("[PERF] sim por tick: fisica=%d hitbox=%d lua=%d anim=%d streaming=%d ms",
                              (int)(d[0] * k + 0.5), (int)(d[1] * k + 0.5), (int)(d[2] * k + 0.5), (int)(d[3] * k + 0.5),
                              (int)(d[4] * k + 0.5)); }
                    gPfSim=gPfAnim=gPfPvs=gPfPart=gPfN=0;
                    g_cullHijosTotal=0; g_cullHijosVisibles=0;
                    g_skinPerfMs=0; g_skinPerfSkins=0; g_skinPerfCompletos=0;
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
    { extern void AplicarAnimacionObjetos(); AplicarAnimacionObjetos(); }
    // VERTEX ANIM ACTIVA EN EL TIMELINE (kind 3): posa la malla al frame del playhead. En PC vive en main.cpp; en
    // el N95 FALTABA -> al seleccionar un clip (ej "correr") y dar Play, la timeline avanzaba pero la vertex-anim
    // NO cambiaba. UpdateAnimations la saltea a proposito (kind 3): manda el playhead, no el auto-avance.
    { extern void AplicarVertexAnimTimeline(); AplicarVertexAnimTimeline(); }

    // Render EVENT-DRIVEN: solo dibujar+cursor+swap si algo CAMBIO (g_redraw, lo
    // prende cualquier tecla/cursor/flecha) o hay una animacion en play. Si la escena
    // esta quieta, el frame se saltea ENTERO (sin GL ni swap/vsync) -> CPU y bateria
    // casi 0. El timer sigue tickeando, pero sin trabajo de GPU.
    // SOLO LA EXTERNA: el editor entero se dibuja en la pantalla externa; el telefono no gasta GPU (ni swap)
    extern TBool W3dMouseCuadroCursor();   // (w3dmouse.cpp)
    if (PantallaExternaSolo() && (g_redraw || HayAnimacionActiva())) {
        g_redraw = false;
        TUint _pfS = User::NTickCount();
        PantallasExternasRender();
        gPfSwap += User::NTickCount() - _pfS;
        gPfCuadros++;
        // (el log paso a paso se apaga tambien aca: el telefono no dibuja, y escribirlo cada cuadro en la tarjeta frenaba)
        extern int g_pantallasLogCuadros;
        if (g_pantallasLogCuadros > 0) g_pantallasLogCuadros--;
    }
    else if (PantallaExternaSolo() && W3dMouseCuadroCursor()) {
        // (solo se movio el cursor: repuesto lo de abajo y dibujado en el lugar nuevo, sin redibujar el editor)
    }
    else if (g_redraw || HayAnimacionActiva()) {
        // CAPTURAR y apagar g_redraw ANTES de dibujar (mismo orden que PC main.cpp): lo que se prenda
        // DURANTE el render vale para el frame SIGUIENTE. Apagarlo despues PISABA esos pedidos: el ray
        // tracer progresivo (RTViewportPaso prende g_redraw para seguir avanzando) quedaba congelado
        // hasta la proxima tecla -> "solo renderiza cuando aprieto flechas".
        g_redraw = false;
        // los PRIMEROS cuadros dejan cada paso en el log: si el telefono se cuelga dibujando, la ultima linea dice donde
        static TInt sCuadrosLog = 0;
        extern int g_pantallasLogCuadros;   // (PantallasExternas.cpp: elegir el modo de una pantalla lo vuelve a prender)
        const TBool logCuadro = (sCuadrosLog < 3) || g_pantallasLogCuadros > 0;
        if (logCuadro) w3dLogf("cuadro %d: dibujando el arbol", sCuadrosLog);
        // arbol de viewports (3D confinado a su rectangulo + props) y el cursor encima
        TUint _pfR = User::NTickCount();
        W3dLayoutRender();
        if (logCuadro) w3dLogf("cuadro %d: arbol listo (glErr=%x), el cursor", sCuadrosLog, (TInt)glGetError());
        instance->iWhisk3D->DrawMouseCursor();
        TUint _pfS = User::NTickCount();
        gPfRender += _pfS - _pfR;

        // Call eglSwapBuffers, which blit the graphics to the window
        if (logCuadro) w3dLogf("cuadro %d: swap (glErr=%x)", sCuadrosLog, (TInt)glGetError());
#ifdef W3D_GLES2
        // CAPTURA de diagnostico del N8 (E:\whisk3d_cap_N.png, se baja por CODA): lo que se va a presentar, leido con TODO
        // dibujado (despues de mandar lo juntado, antes del swap: no cambia lo que se ve). Una del cuadro 10 (sin menu:
        // los iconos) y la del 2do cuadro de cada menu o popup que se abre, hasta 4 en total
        {
            static TInt sCapturas = 0, sCuadros = 0, sConMenu = 0;
            sCuadros++;
            const TBool hayMenu = W3dLayoutMenuAbierto() || W3dLayoutPopupActivo();
            sConMenu = hayMenu ? sConMenu + 1 : 0;
            if (sCapturas < 4 && (sCuadros == 10 || sConMenu == 2)) {
                w3dEngine::DibujarPendiente();
                const TSize t = instance->Size();
                unsigned char* px = (unsigned char*)User::Alloc(t.iWidth * t.iHeight * 4);
                if (px) {
                    w3dEngine::ReadPixelsRGBA(0, 0, t.iWidth, t.iHeight, px);
                    char ruta[40];
                    sprintf(ruta, "E:\\whisk3d_cap_%d.png", (int)sCapturas);
                    w3dEngine::SavePNG(ruta, px, t.iWidth, t.iHeight, true);
                    User::Free(px);
                    w3dLogf("captura %d (%s)", (TInt)sCapturas, hayMenu ? "con un menu abierto" : "sin menu");
                    sCapturas++;
                }
            }
        }
#endif
        // lo JUNTADO sin dibujar (el lote de la UI 2D del backend ES 2.0: lo ultimo del cuadro, el menu y su texto) va
        // ANTES del swap, que es EGL directo y no lo manda: sin esto quedaba para el cuadro siguiente, dibujado encima
        // de otra cosa (el N8: los popups rompian todo). Como PCSwapBuffers en PC
        w3dEngine::DibujarPendiente();
        const EGLBoolean swapOk = eglSwapBuffers( instance->iEglDisplay, instance->iEglSurface );
        if (logCuadro) w3dLogf("cuadro %d: swap=%d eglErr=%x, pantallas externas", sCuadrosLog, (TInt)swapOk, (TInt)eglGetError());
        // las pantallas externas con viewport propio (TV/HDMI): su superficie, su arbol, su swap
        PantallasExternasRender();
        if (logCuadro) { w3dLogf("cuadro %d: listo", sCuadrosLog); sCuadrosLog++; if (g_pantallasLogCuadros > 0) g_pantallasLogCuadros--; }
        gPfSwap += User::NTickCount() - _pfS;
        gPfCuadros++;
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
// editor los recibe por controles.cpp, el mismo camino que Android y la web
// ---------------------------------------------------------
//
void CWhisk3DContainer::HandlePointerEventL( const TPointerEvent& aPointerEvent )
    {
    if ( !iOpenGlInitialized )
        {
        return;
        }
    const TSize tam = Size();
    w3dEngine::TactilSymbian( aPointerEvent, tam.iWidth, tam.iHeight );
    }

// SEGUNDO PLANO (minimizar, otra app al frente): el loop de dibujo se detiene. Symbian^3 cierra las apps de fondo que
// siguen usando la GPU cuando otra necesita memoria grafica (y dibujar en una ventana que no se ve no sirve). Al volver
// arranca de nuevo y se redibuja todo
void CWhisk3DContainer::SetPrimerPlano( TBool aPrimerPlano )
    {
    if ( !iPeriodic )
        {
        return;
        }
    w3dLogf( "[app] %s", aPrimerPlano ? "vuelve al frente" : "pasa a segundo plano: el dibujo se detiene" );
    if ( aPrimerPlano )
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
        if ( iOpenGlInitialized && w3dEngine::TactilDedos() > 0 ) w3dEngine::TactilSoltarTodo();
        }
    }

void CWhisk3DContainer::FocusChanged( TDrawNow /*aDrawNow*/ )
    {
    // (solo con el editor vivo y algun dedo apoyado: al cerrar la app tambien se pierde el foco)
    // (no aca adentro: este aviso puede llegar en medio de otro evento -crear la ventana de una pantalla externa desde
    // el click de un popup- y levantar los dedos manda eventos al editor; va en el cuadro siguiente)
    if ( !IsFocused() && iOpenGlInitialized && w3dEngine::TactilDedos() > 0 )
        {
        iSoltarDedos = ETrue;
        }
    }

// End of File
