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
#include "w3dnewscene.h"
#include <GLES/gl.h> // modelo compartido de PC (Fase 3c): init+render
#include "render/OpcionesRender.h"     // g_redraw (render event-driven)
#include "objects/Materials.h"         // HayAnimacionActiva / UpdateAnimatedMaterials
#include "animation/Animation.h"       // PlayAnimation / AnimFPS / AnimTick / ReloadAnimation (avance del play)

// BARRA DE PROGRESO (export/import OBJ): el hook de swap que faltaba en Symbian. ProgresoIniciar/Actualizar lo
// llaman para mostrar la barra DURANTE la operacion bloqueante (sin esto, ProgresoIniciar hacia return y la
// barra/notificacion no aparecian en el N95). Guarda los handles EGL del surface activo (seteados en ConstructL).
extern void (*LayoutSwapBuffers)();
static EGLDisplay gSwapDisplay = NULL;
static EGLSurface gSwapSurface = NULL;
static void W3dProgresoSwap() { if (gSwapDisplay) eglSwapBuffers(gSwapDisplay, gSwapSurface); }


// ================= MEMBER FUNCTIONS =======================
// ---------------------------------------------------------
// CWhisk3DContainer::ConstructL
// Symbian 2nd phase constructor
// ---------------------------------------------------------
//
void CWhisk3DContainer::ConstructL(const TRect& /*aRect*/){
    CreateWindowL();
    iOpenGlInitialized = EFalse;
    // Create the input handler
    iInputHandler = CWhisk3DInput::NewL();

    SetExtentToWholeScreen();                // Take the whole screen into use
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
    const EGLint attrib_list[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BUFFER_SIZE,  BufferSize,
        EGL_DEPTH_SIZE,   16,
        EGL_NONE
    };

    // Choose an EGLConfig that best matches to the properties in attrib_list_fsaa
    //if ( eglChooseConfig( iEglDisplay, attrib_list_fsaa, configList, configSize, &numOfConfigs ) == EGL_FALSE ){
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
    if ( iEglSurface == NULL )
        {
        _LIT(KCreateWindowSurfaceFailed, "eglCreateWindowSurface failed");
        User::Panic( KCreateWindowSurfaceFailed, 0 );
        }

    // Create a rendering context
    iEglContext = eglCreateContext( iEglDisplay, Config, EGL_NO_CONTEXT, NULL );
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


    TSize size;
    size = this->Size();

    iWhisk3D = CWhisk3D::NewL( size.iWidth, size.iHeight, iInputHandler ); // Create an instance of Whisk3D
    iWhisk3D->AppInit();                                       // Initialize OpenGL ES

    // Fase 1 unificacion: arbol de viewports compartido (core/ui). Es el
    // equivalente al rootViewport->Resize(winW, winH) del main.cpp de PC:
    // horizontal = 2 columnas (3D | propiedades), vertical = 2 filas.
    W3dLayoutBuild( iWhisk3D, size.iWidth, size.iHeight );

    // Fase 3c: raiz del modelo compartido de PC + cubo default (como
    // ConstructUniversal). Su render sale en cada frame del viewport 3D.
    W3dNewSceneInit();

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
	TInt dx = 0, dy = 0;
	if (gHeldLeft) dx -= 1;
	if (gHeldRight) dx += 1;
	if (gHeldUp) dy -= 1;
	if (gHeldDown) dy += 1;
	if (dx == 0 && dy == 0) { gMouseAccel = 0; return; } // soltaron: la proxima vez arranca lento de nuevo
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
	// MOUSE VIRTUAL visible: las flechas mueven el CURSOR como un mouse (no la camara).
	// Durante un transform 3D/de malla NO: ahi las flechas mueven la seleccion. El de KEYFRAMES es al reves: lo
	// maneja el CURSOR (event_mouse_motion -> DopeMoveApply, el mismo camino que arrastrar con el mouse en PC), asi
	// que ahi las flechas tienen que seguir moviendolo o el transform no se puede manejar con nada.
	if (mouseVisible && (!W3dNewTransformActive() || DopeXformActivo())){
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
			// popup modal (File browser) o barra de menu abierta: se quedan con
			// TODO el keypad (flechas mueven el foco/menu, OK selecciona, soft
			// IZQ=164=Cancelar/cerrar). Asi se navegan SIN mouse BT y los
			// digitos no tocan la escena 3D.
			if (W3dLayoutPopupActivo() || W3dLayoutMenuAbierto()){
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
				W3dLayoutTecla(sc, mouseX, mouseY);
				return EKeyWasConsumed; // modal: traga el resto tambien
			}
			// EDICION NUMERICA por texto en curso (un PropFloat en Propiedades): el keypad ESCRIBE el valor.
			// 0-9 = digitos, '*' = punto decimal, C(backspace) borra un digito, OK aplica, soft-IZQ(164) cancela,
			// izq/der mueven el caret. Todo lo demas se consume (no toca la escena). Va ANTES de softkeys/1-2-3/etc.
			if (W3dNumEditActivo()){
				if (sc >= '0' && sc <= '9'){ W3dTextFieldChar(sc); return EKeyWasConsumed; }
				if (sc == '*'){ W3dTextFieldChar('.'); return EKeyWasConsumed; }
				if (sc == EStdKeyBackspace){ W3dTextFieldChar(8); return EKeyWasConsumed; }
				if (sc == EStdKeyLeftArrow || sc == EStdKeyRightArrow){ W3dLayoutTeclaPanel(sc); return EKeyWasConsumed; }
				if (sc == EStdKeyDevice3 || sc == EStdKeyEnter){ W3dLayoutTeclaPanel(EStdKeyDevice3); return EKeyWasConsumed; }
				if (sc == 164){ W3dLayoutTeclaPanel(EStdKeyEscape); return EKeyWasConsumed; } // soft-IZQ = cancelar
				return EKeyWasConsumed; // mientras se tipea, ninguna otra tecla toca la escena
			}
			// soft IZQ sin nada abierto: abre la barra de menu del viewport
			// activo (el primer desplegable, sin preseleccionar item).
			if (sc == 164){
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
				if (gGreenHeld || W3dLayout3DActivo() || mouseVisible || W3dLayoutUVActivo() || W3dLayoutTimelineActivo()){ // 3D=orbita, UV/timeline=paneo/scrub, mouse=cursor
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
			// EDIT MODE (pedido Dante): 7 = Extrude, 8 = Loop Cut, 4 = nada. Las funciones se protegen
			// solas fuera de Edit Mode (chequean InteractionMode), asi que en object mode no hacen nada.
			if (sc == '7'){ LayoutExtrudeFaces(); return EKeyWasConsumed; }
			// 8 = Loop Cut desde el ELEMENTO ACTIVO (cara -> pregunta orientacion; arista -> directo), IGUAL que el
			// menu del viewport (que Dante confirmo que anda). Antes era LoopCutIniciar(mouseX,mouseY) = el path por
			// HOVER de PC: en el N95 el cursor virtual no cae sobre una arista -> sin preview, sin orientacion, OK no hacia nada.
			if (sc == '8'){ LayoutLoopCutDesdeActivo(); return EKeyWasConsumed; }
			if (sc == '9'){ W3dLayoutLockOrbit(); return EKeyWasConsumed; } // 9 = bloquear/desbloquear el orbital
			if (sc == '4'){ return EKeyWasConsumed; } // 4 = nada
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
			if (sc == EStdKeyLeftShift){ // LAPIZ (solo shift ahora; el # se separo para la camara)
				// LAPIZ: solo REGISTRAR. La accion (ciclar) va al SOLTAR si fue una pulsacion
				// corta SIN flecha. Larga = nada. Mantener + flecha = navegar (handler de flechas).
				gLapizHeld = ETrue; gLapizArrowUsed = EFalse; gLapizDownTick = User::NTickCount();
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
			if (usc == EStdKeyYes){
				if (!gGreenUsado) W3dLayoutCiclarViewport(1); // tap = ciclar viewport
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
			if (usc == EStdKeyLeftShift){ // LAPIZ (solo shift ahora)
				TUint dt = User::NTickCount() - gLapizDownTick; // lapiz soltado: corto + sin flecha = ciclar
				if (gLapizHeld && !gLapizArrowUsed && dt < 500 && !W3dNewTransformActive() && !mouseVisible){
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
					if (!LayoutDeleteEdit(mouseX, mouseY)) AbrirConfirmarBorrado();
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
    delete iPeriodic;

    if ( iWhisk3D ){
        delete iWhisk3D;
    }
    delete iInputHandler;
    // el monitor HID es un CActive: si no se borra queda escuchando despues
    // de destruir el container
    delete iHidMonitor;
    W3dLayoutDestroy();

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
int CWhisk3DContainer::DrawCallBack( TAny* aInstance )
    {
    CWhisk3DContainer* instance = (CWhisk3DContainer*) aInstance;
    instance->iFrame++;

    // Heartbeat de diagnostico: una linea cada 300 frames. Si despues de un
    // cuelgue el log sigue creciendo, el loop de render esta vivo y el
    // problema es el ESTADO (clavado en ELoadingTextures); si no crece mas,
    // el thread principal esta bloqueado de verdad.
    if ( (instance->iFrame % 300) == 0 )
        {
        w3dLogf("heartbeat: frame=%d", instance->iFrame);
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
    { extern void NotificacionesTick(float); NotificacionesTick(timeSecs - instance->iLastFrameTimeSecs); }

    // Set the current time to be the last frame time for the upcoming frame
    instance->iLastFrameTimeSecs = timeSecs;

    // input de usuario (desde donde se cicla, no dentro de Whisk3D): el
    // Escape del telefono cancela la transformacion activa del modelo nuevo
    if (instance->iInputHandler->IsInputPressed( EEscape )){
        W3dNewTransformEnd(ETrue);
    }

    // flechas MANTENIDAS: aplicar CADA frame (orbitar/mover/rotar/escalar
    // fluido con el teclado, sin depender del auto-repeat lento del telefono)
    AplicarFlechas3D();

    // CARGA DIFERIDA de texturas del import: 1 por frame (decode+upload). Prende g_redraw al cargar una -> el
    // modelo aparece enseguida y las texturas entran solas. No-op si no hay pendientes. (Va ANTES del check de
    // g_redraw para que corra aunque la escena este quieta.)
    { extern void CargarTexturasPendientes(); CargarTexturasPendientes(); }

    // ANIMACION: materiales/UV animados cada frame + avance del frame en PLAY al ritmo de AnimFPS (independiente de
    // los fps de la UI). En PC vive en MainLoopFrame; en el N95 FALTABA -> el play quedaba "clavado" en el frame 1 y
    // los materiales animados no corrian. (UpdateAnimations -vertex anim, VertexAnimation.cpp- NO se compila en el
    // .mmp -> se saltea; el skinning/pose de esqueleto siguen stub -> el frame avanza pero la malla no deforma.)
    UpdateAnimatedMaterials();
    {
    #ifdef __WINS__
        const TUint tickHz = 200;  // el emulador corre NTickCount a 200Hz
    #else
        const TUint tickHz = 1000; // el device (N95) a 1000Hz = ms reales
    #endif
        static TUint gLastAnimTick = 0;
        TUint nowA = User::NTickCount();
        TUint animTicks = (AnimFPS > 0) ? (TUint)(tickHz / AnimFPS) : (tickHz / 30);
        if (nowA - gLastAnimTick >= animTicks) {
            gLastAnimTick = nowA;
            if (PlayAnimation) { AnimTick(); g_redraw = true; } // avanza CurrentFrame (loop Start..End del clip)
            ReloadAnimation();
        }
    }
    // ANIMACION DE OBJETOS (pos/rot/escala): aplica los keyframes al frame actual. FALTABA en el N95 -> ni el play
    // ni el scrub del timeline movian nada (el frame avanzaba y la escena quedaba quieta). Va FUERA del tick de
    // AnimFPS, como en PC: al scrubear con las flechas se tiene que ver YA, no al ritmo del play. Guard interno:
    // solo trabaja al CAMBIAR de frame.
    { extern void AplicarAnimacionObjetos(); AplicarAnimacionObjetos(); }

    // Render EVENT-DRIVEN: solo dibujar+cursor+swap si algo CAMBIO (g_redraw, lo
    // prende cualquier tecla/cursor/flecha) o hay una animacion en play. Si la escena
    // esta quieta, el frame se saltea ENTERO (sin GL ni swap/vsync) -> CPU y bateria
    // casi 0, como Blender. El timer sigue tickeando, pero sin trabajo de GPU.
    if (g_redraw || HayAnimacionActiva()) {
        // arbol de viewports (3D confinado a su rectangulo + props) y el cursor encima
        W3dLayoutRender();
        instance->iWhisk3D->DrawMouseCursor();

        // Call eglSwapBuffers, which blit the graphics to the window
        eglSwapBuffers( instance->iEglDisplay, instance->iEglSurface );
        g_redraw = false; // ya dibujamos; esperar al proximo cambio
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

// End of File
