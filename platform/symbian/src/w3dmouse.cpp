// ============================================================================
//  w3dmouse.cpp — cursor VIRTUAL de Symbian (ver w3dmouse.h)
//
//  Todo lo del mouse junto: el telefono no tiene mouse, asi que esto es un
//  asunto de la plataforma, no del editor. El estado de graficos pasa por las
//  abstracciones del motor (w3dEngine); solo los primitivos de dibujo
//  inmediatos / point-sprite de GLES1 quedan como llamada GL directa (el motor
//  no los abstrae).
// ============================================================================
#include "w3dmouse.h"
#include <GLES/gl.h>
#include <string.h>            // memset / memcpy
#include "Whisk3D.h"           // CWhisk3D (DrawMouseCursor/SetMouse son metodos)
#include "w3dGraphics.h"       // motor: abstraccion del ESTADO de graficos
#include "app/W3dMouseEditor.h"  // el mouse Bluetooth por el camino de la PC (controls.cpp)
#include "ViewPorts/ExternalScreens.h"   // el MONITOR EXTENDIDO: el mouse pasa a la pantalla externa
#include "w3dlayout.h"         // W3dLayoutDragPopup / W3dLayoutBusy
#include "w3dnewscene.h"   // W3dNew* (transform/orbit/zoom) + DuplicatedObject
#include "WhiskUI/theme/colors.h"        // ColorList / ColorID (paleta del editor)
#include "w3dlog.h"
#include "w3dpantallas.h"      // W3dScreenKeepsFrame (mover solo el cursor)
#include "w3dTexture.h"        // ReserveScreen / CopyScreen (lo que esta debajo del cursor)
#include "base/W3dInteractionState.h"   // InteractionMode
#include "render/RenderOptions.h"  // g_redraw (render event-driven)
#include "ViewPorts/LayoutInput.h" // LayoutDeleteEdit (menu Delete de edit mode)
#include "ViewPorts/Pick3D.h"      // LoopCutActive / LoopSelOrienting (siguen al mouse: no es solo el cursor)
#include "edit/BoxSelect.h"        // BoxSelectActive
#include "ViewPorts/Timeline.h"     // DopeXformActive (el transform de keyframes lo maneja el mouse como en la PC)

// el color y el triangulo del cursor: GL crudo en ES 1.1 (el camino probado del N95); por el motor en ES 2.0 (N8),
// donde glColor4f/glTexEnvf/glPointSize no existen
#ifdef W3D_GLES2
#define COLOR_CURSOR(r, g, b, a) w3dEngine::Color4f((r), (g), (b), (a))
#define DRAW_ARROW() w3dEngine::DrawTrianglesArray(3)
#else
#define COLOR_CURSOR(r, g, b, a) W3D_GL_RAW(glColor4f((r), (g), (b), (a)))
#define DRAW_ARROW() W3D_GL_RAW(glDrawArrays(GL_TRIANGLES, 0, 3))
#endif

#ifndef GL_POINT_SPRITE_OES
#define GL_POINT_SPRITE_OES 0x8861
#endif
#ifndef GL_COORD_REPLACE_OES
#define GL_COORD_REPLACE_OES 0x8862
#endif

// posicion del cursor virtual. Son globales COMPARTIDOS (variables.h los
// declara extern). En PC los define variables.cpp; en Symbian (que no tiene
// mouse) los define ESTE archivo. Tipos EXACTOS de variables.h (bool, no TBool).
GLshort mouseX = 0;
GLshort mouseY = 0;
bool mouseVisible = false;

// --- sprite del cursor ---
static GLuint gMouseTex = 0;

// mouse.png es 16x32: va acolchada en una celda de 32x32 (mitad derecha
// transparente) para dibujarla como UN point sprite de 32px
void W3dMouseBuildTex(const unsigned char* aRGBA, int aW, int aH) {
    if (gMouseTex != 0 || !aRGBA || aW < 1 || aW > 32 || aH > 32) return;
    static unsigned char cell[32 * 32 * 4];
    memset(cell, 0, sizeof(cell));
    for (TInt y = 0; y < aH; y++) {
        memcpy(cell + (y * 32) * 4, aRGBA + (y * aW) * 4, aW * 4);
    }
    glGenTextures(1, &gMouseTex);
    glBindTexture(GL_TEXTURE_2D, gMouseTex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, cell);
#ifdef W3D_GLES2
    w3dEngine::GLES2Resync();   // (el glBindTexture de arriba no paso por el motor)
#endif
    w3dLog("mouse.png acolchada a 32x32 (point sprite)");
}

unsigned int W3dMouseTex() {
    return (unsigned int)gMouseTex;
}

// el "warp" en Symbian = mover el cursor virtual
void W3dMouseWarp(int aX, int aY) {
    mouseX = (GLshort)aX;
    mouseY = (GLshort)aY;
}

// mover el cursor con las FLECHAS (mouse virtual visible): como mover el mouse BT, pero
// el delta viene del teclado. Clampea a la pantalla y deja activo el viewport bajo el cursor.
void W3dMouseMoveArrows(int aDx, int aDy) {
    // el DELTA de este movimiento (las globales que leen los viewports, como el xrel del mouse en la PC). Sin esto
    // el transform de KEYFRAMES (acumula dx/dy) usaba el que habia quedado de antes: derecha los movia para atras
    extern int dx, dy;
    dx = aDx; dy = aDy;
    mouseX = (GLshort)(mouseX + aDx);
    mouseY = (GLshort)(mouseY + aDy);
    // moviendo keyframes: el cursor da la vuelta DENTRO del timeline (como el mouse en la PC) y el transform sigue
    if (DopeXformActive() && viewPortActive && viewPortActive->ViewportKind() == 5) {
        const int l = viewPortActive->x + 1, r = viewPortActive->x + viewPortActive->width - 2;
        const int t = viewPortActive->y + 1, b = viewPortActive->y + viewPortActive->height - 2;
        if (r > l && b > t) {
            if (mouseX < l) { mouseX = (GLshort)r; } else if (mouseX > r) { mouseX = (GLshort)l; }
            if (mouseY < t) { mouseY = (GLshort)b; } else if (mouseY > b) { mouseY = (GLshort)t; }
        }
        g_redraw = true;
        viewPortActive->event_mouse_motion(mouseX, mouseY);
        return;
    }
    if (!mouseVisible) { mouseX = (GLshort)(mouseX - aDx); mouseY = (GLshort)(mouseY - aDy); return; }
    if (mouseX < 0) { mouseX = 0; }
    if (mouseX > winW-1) { mouseX = (GLshort)(winW-1); }
    if (mouseY < 0) { mouseY = 0; }
    if (mouseY > winH-1) { mouseY = (GLshort)(winH-1); }
    g_redraw = true;
    W3dLayoutMouseMotion(mouseX, mouseY); // hover de paneles (scrollbars, etc.)
    W3dLayoutMouseMoved(mouseX, mouseY);  // el viewport bajo el cursor pasa a activo
}

// muestra/oculta el cursor y lo centra (lo togglea una tecla via AppUi)
void CWhisk3D::SetMouse(){
    mouseVisible = !mouseVisible;
    mouseX = winW/2;
    mouseY = (GLshort)(winH/2);
}

// dibuja el cursor encima de todo (lo llama el container tras el render del
// arbol de viewports)
static CWhisk3D* gCursorOwner = NULL;   // (el que dibuja el cursor: el hook de la pantalla externa lo reusa)
void CWhisk3D::DrawMouseCursor(){
    gCursorOwner = this;
    static TInt cl = 0;
    if ((++cl & 0xFF) == 1){
        w3dLogf("cursor: visible=%d tex=%d", (TInt)mouseVisible, (TInt)W3dMouseTex());
    }
    if (!mouseVisible){ return; }

    // el render del frame dejo el cache del motor con SU estado: resincronizar
    // antes de pedir cambios via abstraccion (ver w3dGraphics::Invalidate)
    w3dEngine::Invalidate();

    w3dEngine::Disable(w3dEngine::Fog); // el render viejo lo apagaba cada frame

    // autocontenido: pantalla completa + ortho en pixeles + matrices propias
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Viewport(0, 0, winW, winH);
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0.0f, (GLfloat)winW, (GLfloat)winH, 0.0f, -5.0f, 1000.0f);   // (por el motor: lleva la sombra
    w3dEngine::MatrixMode(w3dEngine::ModelView);                                   //  de las matrices en la CPU)
    w3dEngine::PushMatrix();
    w3dEngine::LoadIdentity();

    w3dEngine::Enable(w3dEngine::Blend);
#ifdef W3D_GLES2
    w3dEngine::BlendAlpha();
#else
    W3D_GL_RAW(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
#endif
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Lighting);

    // Hotspot = esquina superior izquierda, como Windows. 6 vertices sueltos:
    // el driver del N95 no dibuja glDrawElements en la fase 2D.
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::DisableArray(w3dEngine::NormalArray); // puntero viciado de la escena
    // (sin push de matriz de textura: los point sprites no la usan y un push
    // sin pop aca desbordaba el stack -> glErr 503 cada frame)
    COLOR_CURSOR(1.0f, 1.0f, 1.0f, 1.0f);
    w3dEngine::Translatef((float)mouseX, (float)mouseY, 0.0f);

    if (W3dMouseTex() != 0){
        // mouse.png como POINT SPRITE de 32px (16x32 acolchada): el unico
        // camino texturizado confiable en este driver
        static const GLfloat MousePt[2] = { 16.0f, 16.0f }; // centro de la celda
        if (W3dLayoutDragPopup()){
            // VIOLETA: arrastrando un valor del selector de color
            COLOR_CURSOR(0.65f, 0.4f, 1.0f, 1.0f);
        } else if (W3dLayoutBusy()){
            // VERDE: estas en medio de algo (scroll/divisor agarrado)
            COLOR_CURSOR(ColorList[static_cast<int>(ColorID::accent)][0], ColorList[static_cast<int>(ColorID::accent)][1],
                         ColorList[static_cast<int>(ColorID::accent)][2], 1.0f);
        } else {
            COLOR_CURSOR(1.0f, 1.0f, 1.0f, 1.0f);
        }
        w3dEngine::BindTexture(W3dMouseTex());
#ifdef W3D_GLES2
        // ES 2.0: el sprite es del shader del motor (gl_PointCoord); sin GL crudo
        w3dEngine::PointSpriteCoordReplace(true);
        w3dEngine::PointSize(32.0f);
        w3dEngine::VertexPointer2f(0, MousePt);
        w3dEngine::DrawPoints(1);
        w3dEngine::PointSpriteCoordReplace(false);
#else
        W3D_GL_RAW(glEnable(GL_POINT_SPRITE_OES));
        W3D_GL_RAW(glTexEnvf(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE));
        W3D_GL_RAW(glPointSize(32.0f));
        // los punteros por el motor (desbindea el VBO que haya dejado la escena y lleva su cache de punteros) y la
        // modelview perezosa en GL antes del draw crudo
        w3dEngine::VertexPointer2f(0, MousePt);
        w3dEngine::TexCoordPointer2f(0, MousePt); // dummy (COORD_REPLACE)
        w3dEngine::MatricesSync();
        W3D_GL_RAW(glDrawArrays(GL_POINTS, 0, 1));
        W3D_GL_RAW(glTexEnvf(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE));
        W3D_GL_RAW(glDisable(GL_POINT_SPRITE_OES));
#endif
    }
    else {
        // fallback: flecha plana mientras mouse.png no este subida
        static const GLfloat ArrowShadow[] = { 1,1,0,  1,15,0,  11,11,0 };
        static const GLfloat ArrowVerts[]  = { 0,0,0,  0,14,0,  10,10,0 };
        w3dEngine::Disable(w3dEngine::Texture2D);
        w3dEngine::DisableArray(w3dEngine::TexCoordArray);
        COLOR_CURSOR(0.0f, 0.0f, 0.0f, 1.0f);
        w3dEngine::VertexPointer3f(0, ArrowShadow);   // (por el motor, como el sprite de arriba)
        w3dEngine::MatricesSync();
        DRAW_ARROW();
        COLOR_CURSOR(1.0f, 1.0f, 1.0f, 1.0f);
        w3dEngine::VertexPointer3f(0, ArrowVerts);
        DRAW_ARROW();
        w3dEngine::EnableArray(w3dEngine::TexCoordArray);
        w3dEngine::Enable(w3dEngine::Texture2D);
    }

    w3dEngine::EnableArray(w3dEngine::NormalArray);
    w3dEngine::PopMatrix();
}

// ============================================================================
//  Input HID (mouse/teclado bluetooth) — tambien es cosa de Symbian, no del
//  editor: el N95 no tiene mouse/teclado propios. Llega desde CHidMonitor
//  (MHidObserver). El driver acumula movimientos mientras hay un boton apretado.
// ============================================================================

// Estado de los botones. OJO: el driver HID NO manda movimientos mientras un
// boton esta apretado (los acumula y los larga de golpe al soltar), asi que
// "mantener y arrastrar" para orbitar es IMPOSIBLE con este driver. En su
// lugar el click DERECHO es un TOGGLE de modo orbita: un click entra (el
// cursor se oculta y mover el mouse rota la camara), otro click sale y
// vuelve el cursor. El boton del medio queda como "mantener" para mouses
// cuyo driver si lo permita. Los botones ademas REBOTAN (varios down/up por
// click fisico, a ~8ms): por eso el debounce de 300ms.
static TBool hidMiddleDown = EFalse;
static TBool hidOrbitMode = EFalse;
static TUint hidLastRightTick = 0;

// globals reales de variables.cpp (los usan los Scrollable de PC)
extern int dx;
extern int dy;
extern bool leftMouseDown;
extern bool LShiftPressed;
extern bool LCtrlPressed;
extern bool LAltPressed;

static TBool gHasMouseBT = EFalse;
TBool W3dHasMouseBT() { return gHasMouseBT; }
// teclado BT: se "detecta" con la primera tecla que llega por hidsrv. Los menus lo usan para mostrar los atajos de
// teclado de PC en vez de las teclas del keypad (app/W3dShortcuts.cpp, W3dShortcutMenuLabel).
static TBool gHasKeyboardBT = EFalse;
bool W3dSymbianKeyboardBT() { return gHasKeyboardBT ? true : false; }
bool W3dSymbianMouseBT()   { return gHasMouseBT ? true : false; }

// EL MOUSE BLUETOOTH COMO EL DE LA PC: con el driver bthid-s60 llegan el apretar y el soltar de verdad (el de Hinkka
// mandaba un click por reporte y nada se podia mantener: de ahi el modo orbita con el derecho y el debounce). Ahora va
// por W3dRatonEditor -> controls.cpp, el mismo camino que la PC: click derecho = menu contextual, arrastrar = caja,
// pintar, rueda = zoom, el del medio (o izquierdo + derecho juntos) = orbitar.
// La EXCEPCION: un mover/rotar/escalar arrancado con el teclado Bluetooth (g/r/s, el modelo del N95): el mouse lo
// maneja como siempre (W3dNewTransform*) hasta que se acepta o se cancela.
// ---- EL MONITOR EXTENDIDO: el cursor sale por la derecha del telefono y entra por la izquierda de la pantalla externa
//      (la que tiene arbol propio, ExternalScreenNeighbor), y vuelve por su borde izquierdo. Mientras esta ahi, cada
//      evento del mouse se atiende ADENTRO de esa pantalla (ExternalScreenEnter: su arbol, sus activos, su tamano) y
//      con las coordenadas de ella; el cursor se dibuja ahi (ExternalScreenCursor) y no en el telefono.
static TInt gMouseIn = 0;            // la pantalla donde esta el cursor (0 = el telefono)

// un mover/rotar/escalar que arranco EL MOUSE (el gizmo, la barra de herramientas, un menu): lo sigue manejando el camino
// de la PC hasta que termina. W3dNewTransformActive() tambien es true con esos (un transform de malla en Edit Mode lo
// es siempre) y el camino del N95 se quedaba con el soltar: el gizmo no se soltaba nunca.
static TBool gMouseXform = EFalse;
static TBool KeyboardXform() {
	if (!W3dNewTransformActive()) { gMouseXform = EFalse; return EFalse; }
	// el de KEYFRAMES (1/2/3 = g/r/s en el timeline) lo maneja el mouse por el camino de la PC aunque lo haya
	// arrancado el teclado: el cursor da la vuelta dentro del TIMELINE y mueve los keyframes. Por este camino se
	// encerraba en el viewport 3D (saltaba arriba de todo) y el movimiento iba a un transform 3D que no existe
	if (DopeXformActive()) return EFalse;
	return !gMouseXform;
}
// todo lo que el mouse le entrega al editor pasa con uno de estos: si un transform arranca adentro, es del mouse. El
// apretar sale DEMORADO (W3dRatonEditor espera kMouseChordMs por el acorde izquierdo+derecho = boton del medio): el
// gizmo arranca en el tick o en el movimiento siguiente, no en el click. Mirando solo el click no se marcaba, el camino
// del N95 se quedaba con el soltar y el gizmo no se soltaba (y despues el mouse seguia escalando sin boton)
class TLooksXform {
public:
	TLooksXform() : iHad(W3dNewTransformActive()) {}
	~TLooksXform() { if (!iHad && W3dNewTransformActive()) gMouseXform = ETrue; }
private:
	TBool iHad;
};
static TInt gExtX = 0, gExtY = 0, gExtW = 1, gExtH = 1;

// atender un evento del mouse en la pantalla externa: sus globales y el cursor (mouseX/mouseY, lo que leen
// SaveMousePos y SDL_GetMouseState) en SUS coordenadas, y despues todo como estaba
// aSurface: ademas, SU superficie de GL pasa a ser la de dibujo mientras dura el evento. El pick por color (click,
// el hover del loop cut, el snap) dibuja y lee pixeles: sobre la del telefono (640x360) solo andaba en la parte de la
// TV que cae adentro de esos 640x360 -un borde si, otro no; vertices imposibles de clickear-. Lo que se dibuje ahi
// pisa el cuadro conservado: el cuadro siguiente se redibuja entero
class TInExternalScreen {
public:
	TInExternalScreen(TBool aActivate = ETrue, TBool aSurface = ETrue) : iX(mouseX), iY(mouseY), iInside(EFalse), iSup(EFalse) {
		if (aActivate && gMouseIn && ExternalScreenEnter(gMouseIn)) {
			iInside = ETrue;
			mouseX = (GLshort)gExtX; mouseY = (GLshort)gExtY;
			if (aSurface && ExternalScreenActivate && ExternalScreenActivate(gMouseIn)) iSup = ETrue;
		}
	}
	~TInExternalScreen() {
		if (!iInside) return;
		if (iSup) { if (ExternalScreenReturn) ExternalScreenReturn(); g_redraw = true; }
		mouseX = iX; mouseY = iY;
		ExternalScreenExit();
	}
private:
	GLshort iX, iY;
	TBool iInside, iSup;
};
// un movimiento sin boton solo pickea con una herramienta que sigue al mouse (o un transform: el snap). Fuera de eso no
// se cambia de superficie por cada movimiento (eglMakeCurrent dos veces por evento)
static TBool MotionPicks() {
	return state != editNavigation || LoopCutActive() || LoopSelOrienting() || PickPathGuided() || W3dAnyMousePressed();
}

// la pantalla externa se desconecto o volvio a clonar: el cursor vuelve al telefono
static void MouseVerifyScreen() {
	TInt id = 0, w = 0, h = 0;
	// SOLO LA EXTERNA: el cursor vive ahi (arranca en el medio) y no vuelve al telefono
	if (ExternalScreenOnly()) {
		// (el tamaño se relee siempre: si el cursor ya estaba en la externa de antes, o la superficie cambio de tamaño,
		// el gExtW viejo no lo dejaba llegar al borde derecho -la barra de desplazamiento-)
		for (TInt i = 0; ExternalScreenInfo(i, &id, &w, &h, NULL); i++) {
			if (id != ExternalScreenOnly()) continue;
			if (gMouseIn != id) { gMouseIn = id; gExtX = w / 2; gExtY = h / 2; mouseVisible = false; }
			if (gExtW != w || gExtH != h) w3dLogf("[raton] solo la externa: el cursor va en %dx%d", w, h);
			gExtW = w; gExtH = h;
			if (gExtX > w - 1) gExtX = w - 1;
			if (gExtY > h - 1) gExtY = h - 1;
			break;
		}
		return;
	}
	if (gMouseIn && (!ExternalScreenNeighbor(&id, &w, &h) || id != gMouseIn)) {
		gMouseIn = 0;
		mouseVisible = ETrue;
	}
}

// ---- MOVER SOLO EL CURSOR ("solo la externa", 1280x720): pasar el mouse sin botones por el contenido del 3D no cambia
//      nada de lo dibujado (como mucho el overlay). Redibujar el editor entero por eso era lo que lo hacia inusable. Si el
//      swap conserva el cuadro, se guarda lo que queda debajo del cursor (32x32, el sprite) antes de dibujarlo; al moverse
//      se repone eso, se guarda lo del lugar nuevo y se dibuja ahi. El resto del cuadro queda como estaba
static TUint gBackgroundTex = 0;
static TInt gBackgroundX = -1, gBackgroundY = -1, gBackgroundHeight = 0;
static TBool gPendingCursor   = EFalse;   // se movio solo el cursor: falta presentarlo (W3dMouseFrameCursor)
static const TInt KCursorSide = 32;

static void SaveBackgroundCursor(int aW, int aH) {
	if (!gBackgroundTex) gBackgroundTex = w3dEngine::ReserveScreen(0, KCursorSide, KCursorSide);
	TInt x = gExtX, y = gExtY;   // (el sprite va de la punta hacia abajo a la derecha: cerca del borde, el ultimo cuadrado)
	if (x > aW - KCursorSide) x = aW - KCursorSide;
	if (y > aH - KCursorSide) y = aH - KCursorSide;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	w3dEngine::CopyScreen(gBackgroundTex, x, aH - y - KCursorSide, KCursorSide, KCursorSide);
	gBackgroundX = x; gBackgroundY = y; gBackgroundHeight = aH;
}

static void RestoreBackgroundCursor(int aW, int aH) {
	if (!gBackgroundTex || gBackgroundX < 0 || gBackgroundHeight != aH) return;
	const GLfloat x0 = (GLfloat)gBackgroundX, y0 = (GLfloat)gBackgroundY;
	const GLfloat x1 = x0 + KCursorSide, y1 = y0 + KCursorSide;
	// (la copia tiene la fila de ABAJO en t = 0)
	static GLfloat v[12], t[12];
	const GLfloat vv[12] = { x0,y0, x0,y1, x1,y1,  x0,y0, x1,y1, x1,y0 };
	const GLfloat tt[12] = { 0,1,   0,0,   1,0,    0,1,   1,0,   1,1 };
	memcpy(v, vv, sizeof(v)); memcpy(t, tt, sizeof(t));   // (el backend puede juntar el draw: punteros que sigan vivos)
	w3dEngine::Invalidate();
	w3dEngine::Disable(w3dEngine::ScissorTest);
	w3dEngine::Disable(w3dEngine::Blend);
	w3dEngine::Disable(w3dEngine::DepthTest);
	w3dEngine::Disable(w3dEngine::Lighting);
	w3dEngine::Disable(w3dEngine::Fog);
	w3dEngine::Viewport(0, 0, aW, aH);
	w3dEngine::MatrixMode(w3dEngine::Projection);
	w3dEngine::LoadIdentity();
	w3dEngine::Ortho(0.0f, (GLfloat)aW, (GLfloat)aH, 0.0f, -5.0f, 1000.0f);
	w3dEngine::MatrixMode(w3dEngine::ModelView);
	w3dEngine::LoadIdentity();
	w3dEngine::Enable(w3dEngine::Texture2D);
	w3dEngine::BindTexture(gBackgroundTex);
	w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
	w3dEngine::DisableArray(w3dEngine::NormalArray);
	w3dEngine::DisableArray(w3dEngine::ColorArray);
	w3dEngine::EnableArray(w3dEngine::TexCoordArray);
	w3dEngine::VertexPointer2f(0, v);
	w3dEngine::TexCoordPointer2f(0, t);
	w3dEngine::DrawTrianglesArray(6);
}

// el puntero (ya Entrado a su pantalla) esta donde moverlo no cambia nada de lo dibujado
static TBool OnlyCursorHere() {
	return ExternalScreenOnly() && gMouseIn == ExternalScreenOnly() && W3dScreenKeepsFrame(gMouseIn) &&
	       !W3dAnyMousePressed() && !W3dNewTransformActive() && !W3dLayoutOpenMenu() && !W3dLayoutActivePopup() &&
	       (InteractionMode == ObjectMode || InteractionMode == EditMode) && W3dLayoutOverContent3D(mouseX, mouseY) &&
	       // las herramientas que siguen al mouse sin boton (los cortes posibles del loop cut, la direccion del loop
	       // select, el camino guiado, un transform que arranco el teclado) SI cambian el cuadro
	       state == editNavigation && !LoopCutActive() && !InsetActive() && !BevelActive() && !LoopSelOrienting() && !PickPathGuided() && !BoxSelectActive();
}

// el cursor en la pantalla externa: el mismo dibujo, con su tamano y su posicion
static void CursorInExternalScreen(int aId, int aW, int aH) {
	if (aId != gMouseIn || !gCursorOwner) return;
	gPendingCursor = EFalse;   // (este cuadro ya lo dibuja donde esta)
	if (W3dScreenKeepsFrame(aId)) SaveBackgroundCursor(aW, aH);
	const GLshort x0 = mouseX, y0 = mouseY; const int w0 = winW, h0 = winH; const bool vis0 = mouseVisible;
	mouseX = (GLshort)gExtX; mouseY = (GLshort)gExtY; winW = aW; winH = aH; mouseVisible = true;
	gCursorOwner->DrawMouseCursor();
	mouseX = x0; mouseY = y0; winW = w0; winH = h0; mouseVisible = vis0;
}
struct THookExternalCursor { THookExternalCursor() { ExternalScreenCursor = CursorInExternalScreen; } };
static THookExternalCursor gHookExternalCursor;

// el envolver del cursor dentro del viewport (CheckWarpMouseInViewport, como la PC): en la pantalla donde esta
static void TeleportMouse(int aX, int aY) {
	if (gMouseIn) { gExtX = aX; gExtY = aY; }   // (mouseX/mouseY tienen las de la externa solo durante el evento)
	mouseX = (GLshort)aX; mouseY = (GLshort)aY;
}
struct THookTeleport { THookTeleport() { W3dMouseTeleport = TeleportMouse; } };
static THookTeleport gHookTeleport;

// el container, cuando no hay nada que redibujar: si solo se movio el cursor, se presenta eso. false = no habia nada
TBool W3dMouseFrameCursor() {
	if (!gPendingCursor) return EFalse;
	gPendingCursor = EFalse;
	const TInt id = gMouseIn;
	if (!id || !W3dScreenKeepsFrame(id) || gBackgroundX < 0 || !ExternalScreenActivate) { g_redraw = true; return ETrue; }
	w3dEngine::DrawPending();
	if (!ExternalScreenActivate(id)) { g_redraw = true; return ETrue; }
	RestoreBackgroundCursor(gExtW, gExtH);
	CursorInExternalScreen(id, gExtW, gExtH);   // (guarda lo de abajo del lugar nuevo y lo dibuja)
	w3dEngine::DrawPending();
	if (ExternalScreenPresent) ExternalScreenPresent(id);
	if (ExternalScreenReturn) ExternalScreenReturn();
	return ETrue;
}

// una vez por cuadro (container): el apretar que esperaba por si era el boton del medio, en la pantalla donde esta
void W3dMouseTick() {
	MouseVerifyScreen();
	TInExternalScreen in(ETrue, W3dAnyMousePressed());   // (el apretar demorado puede pickear)
	TLooksXform looks;
	W3dMouseTick();
}

void CWhisk3D::HidMouseMove(TInt aDx, TInt aDy){
	if (!gHasMouseBT){ extern bool g_gizmoOn; g_gizmoOn = true; }   // con mouse, el gizmo (como la PC)
	gHasMouseBT = ETrue;
	const bool redrawPrevious = g_redraw;
	g_redraw = true; // se movio el mouse BT -> redibujar
	// el delta gigante que algunos drivers acumulan mientras un boton esta apretado se descarta
	if (aDx > 100 || aDx < -100 || aDy > 100 || aDy < -100){
		return;
	}
	MouseVerifyScreen();
	TLooksXform looks;
	// "solo la externa" con un transform del teclado: el cursor da la vuelta dentro del 3D (que esta en la externa)
	if (gMouseIn && ExternalScreenOnly() && KeyboardXform()){
		gExtX += aDx; gExtY += aDy;
		TInt vx, vy, vw, vh, sh;
		{ TInExternalScreen in(ETrue, EFalse); W3dLayoutGet3DRect(vx, vy, vw, vh, sh); }
		if (vw > 2 && vh > 2){
			if (gExtX <= vx)                { gExtX = vx + vw - 2; }
			else if (gExtX >= vx + vw - 1) { gExtX = vx + 1; }
			if (gExtY <= vy)                { gExtY = vy + vh - 2; }
			else if (gExtY >= vy + vh - 1) { gExtY = vy + 1; }
		}
		TInExternalScreen in;
		if (W3dNewIsRotateFromView()){ W3dNewRotateFromView(mouseX, mouseY); }
		else { W3dNewTransformMove(aDx * 2, aDy * 2); }
		W3dNewTransformLine(mouseX, mouseY);
		return;
	}
	// EN LA PANTALLA EXTERNA: se mueve ahi; por su borde izquierdo vuelve al telefono (salvo en "solo la externa")
	if (gMouseIn && !KeyboardXform()){
		TInt nx = gExtX + aDx;
		if (nx < 0 && ExternalScreenOnly()) nx = 0;
		if (nx < 0){
			gMouseIn = 0;
			mouseX = (GLshort)(winW - 1 + nx);
			mouseY = (GLshort)(gExtY * winH / (gExtH > 0 ? gExtH : 1));
			if (mouseX < 0){ mouseX = 0; }
			if (mouseY > winH-1){ mouseY = (GLshort)(winH-1); }
			mouseVisible = true;
			W3dMouseMoved(mouseX, mouseY, aDx, aDy);
			return;
		}
		TBool onlyCursor;
		{ TInExternalScreen en0(ETrue, EFalse); onlyCursor = OnlyCursorHere(); }
		gExtX = nx < gExtW - 1 ? nx : gExtW - 1;
		gExtY = gExtY + aDy;
		if (gExtY < 0){ gExtY = 0; }
		if (gExtY > gExtH - 1){ gExtY = gExtH - 1; }
		TInExternalScreen in(ETrue, MotionPicks());
		W3dMouseMoved(mouseX, mouseY, aDx, aDy);
		// de un lugar del 3D a otro, sin botones: nada cambio salvo el cursor (el cuadro no se redibuja)
		if (onlyCursor && OnlyCursorHere()){ g_redraw = redrawPrevious; gPendingCursor = ETrue; }
		return;
	}
	if (!mouseVisible){ mouseVisible = true; }
	// por el borde DERECHO del telefono se pasa a la pantalla externa (si tiene arbol propio)
	{
		TInt id = 0, w = 0, h = 0;
		if (!KeyboardXform() && mouseX + aDx > winW - 1 && ExternalScreenNeighbor(&id, &w, &h)){
			gMouseIn = id; gExtW = w; gExtH = h;
			gExtX = mouseX + aDx - winW;
			if (gExtX > w - 1){ gExtX = w - 1; }
			gExtY = mouseY * h / (winH > 0 ? winH : 1);
			mouseVisible = false;   // (el cursor se dibuja en la otra pantalla)
			TInExternalScreen in;
			W3dMouseMoved(mouseX, mouseY, aDx, aDy);
			return;
		}
	}
	mouseX = (GLshort)(mouseX + aDx);
	mouseY = (GLshort)(mouseY + aDy);
	// como la flecha de Windows: el hotspot puede llegar hasta el ultimo pixel de la pantalla
	if (mouseX < 0){ mouseX = 0; }
	if (mouseX > winW-1){ mouseX = (GLshort)(winW-1); }
	if (mouseY < 0){ mouseY = 0; }
	if (mouseY > winH-1){ mouseY = (GLshort)(winH-1); }
	// transformacion del modelo del N95 (teclas g/r/s): el mouse la maneja. Como en la PC, el cursor no se sale del
	// viewport 3D: aparece por el borde opuesto y el transform sigue
	if (KeyboardXform()){
		TInt vx, vy, vw, vh, sh;
		W3dLayoutGet3DRect(vx, vy, vw, vh, sh);
		if (vw > 2 && vh > 2){
			if (mouseX <= vx)            { mouseX = (GLshort)(vx + vw - 2); }
			else if (mouseX >= vx + vw - 1) { mouseX = (GLshort)(vx + 1); }
			if (mouseY <= vy)            { mouseY = (GLshort)(vy + vh - 2); }
			else if (mouseY >= vy + vh - 1) { mouseY = (GLshort)(vy + 1); }
		}
		if (W3dNewIsRotateFromView()){
			W3dNewRotateFromView(mouseX, mouseY);   // rotar LIBRE = trackball alrededor del pivot
		} else {
			W3dNewTransformMove(aDx * 2, aDy * 2);   // mover/rotar-eje/escalar
		}
		W3dNewTransformLine(mouseX, mouseY);
		return;
	}
	W3dMouseMoved(mouseX, mouseY, aDx, aDy);
}

void CWhisk3D::HidMouseButton(TInt aButton, TBool aDown){
	gHasMouseBT = ETrue;
	g_redraw = true; // click del mouse BT -> redibujar
	// transformacion del modelo del N95 (la arranco el teclado): izquierdo acepta, derecho cancela
	if (KeyboardXform()){
		if (aDown && aButton == EMouseButtonLeft){ W3dNewTransformEnd(EFalse); }
		else if (aDown && aButton != EMouseButtonNull){ W3dNewTransformEnd(ETrue); }
		return;
	}
	const TInt button = (aButton == EMouseButtonLeft) ? 1 : (aButton == EMouseButtonMiddle) ? 2 :
	                   (aButton == EMouseButtonRight) ? 3 : 0;
	if (button == 0){ return; }
	MouseVerifyScreen();
	TInExternalScreen in;   // (en la pantalla externa: sus coordenadas y su arbol)
	TLooksXform looks;
	W3dMouseButton(button, aDown ? true : false, mouseX, mouseY);
}

void CWhisk3D::HidMouseWheel(TInt aDelta){
	gHasMouseBT = ETrue;
	g_redraw = true; // rueda del mouse BT -> redibujar
	{ static TInt sLog = 0; if (sLog < 10) { sLog++; w3dLogf("[raton] rueda %d", aDelta); } }
	MouseVerifyScreen();
	TInExternalScreen in;
	TLooksXform looks;
	W3dMouseWheel(aDelta, mouseX, mouseY);
}

void CWhisk3D::HidKey(TInt aScanCode, TBool aDown){
	g_redraw = true; // tecla del teclado BT -> redibujar
	gHasKeyboardBT = ETrue;
	{ static TInt sLog = 0; if (sLog < 400) { sLog++; w3dLogf("[teclas] hid: codigo %d %s", aScanCode, aDown ? "down" : "up"); } }
	// el teclado Bluetooth es el de la PC: cada tecla se vuelve el evento de teclado de SDL y entra por el mismo camino
	// (controls.cpp -> el viewport bajo el cursor), con los mismos atajos. Antes habia aca una lista propia de una docena
	// de teclas que se iba quedando atras (faltaban E, Shift+R, Ctrl+Z, el Esc de los teclados compactos...)
	MouseVerifyScreen();
	TInExternalScreen in;   // (con el cursor en la pantalla externa: su arbol, como el mouse)
	TLooksXform looks;      // (un transform que arranca con una tecla lo maneja el mouse, como en la PC)
	W3dKeyboardKeyHID(aScanCode, aDown ? true : false);
}

// seleccion con click izquierdo: el pick real + el ciclado de solapados los
// hace W3dNewScenePick (compartido); aca solo el guard de estar sobre el 3D
// y el debounce del rebote del boton izquierdo del mouse BT.
void CWhisk3D::ClickSelect(){
	if (!W3dLayoutMouseOver3D(mouseX, mouseY)){ return; }

	static TUint lastClickTick = 0;
	TUint now = User::NTickCount();
	if (now - lastClickTick < 250){ return; }
	lastClickTick = now;

	TInt vx, vy, vw, vh, sh;
	W3dLayoutGet3DRect(vx, vy, vw, vh, sh);
	W3dNewScenePick(mouseX, mouseY, vx, vy, vw, vh, sh);
}

// (compartida, LayoutInput.cpp) loop select en una posicion -> declarada aca para no arrastrar el .h
bool LayoutLoopSelectInPos(int mx, int my, int vx, int vy, int vw, int vh, int screenH);

// lapiz + OK con el mouse virtual: LOOP SELECT en la posicion del cursor (mismo rect que el pick).
void CWhisk3D::ClickLoopSelect(){
	if (!W3dLayoutMouseOver3D(mouseX, mouseY)){ return; }
	TInt vx, vy, vw, vh, sh;
	W3dLayoutGet3DRect(vx, vy, vw, vh, sh);
	LayoutLoopSelectInPos(mouseX, mouseY, vx, vy, vw, vh, sh);
}
