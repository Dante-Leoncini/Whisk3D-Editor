/*
 * ==============================================================================
 *  w3dlayout.h — arma el arbol de viewports del editor para Symbian
 *
 *  Usa las clases REALES de PC (ViewportRow/Column, Viewport3D, Properties; ver main/ui/ViewPorts):
 *    - pantalla horizontal -> Row( Viewport3D | Properties )  70/30
 *    - pantalla vertical   -> Column( Viewport3D / Properties ) 70/30
 *  Reemplaza a la UI 2D vieja de Symbian (dibujarUI), que queda obsoleta.
 *  El cursor del mouse se dibuja aparte, encima de todo (CWhisk3D::DrawMouseCursor).
 * ==============================================================================
 */

#ifndef __W3DLAYOUT_H__
#define __W3DLAYOUT_H__

#include <e32std.h>

class CWhisk3D;

// crea (o re-arma si cambio la orientacion) el arbol y hace el Resize.
// equivalente al rootViewport->Resize(winW, winH) del main.cpp de PC.
void W3dLayoutBuild(CWhisk3D* aWhisk, TInt aWidth, TInt aHeight);

// renderiza el frame completo: fondo + arbol de viewports
void W3dLayoutRender();

// ETrue si esa posicion del cursor cae dentro de un viewport 3D (la hoja
// "activa" que recibe orbita/zoom/seleccion)
TBool W3dLayoutMouseOver3D(TInt aX, TInt aY);

// el cursor se movio: actualiza que viewport esta activo (cambia camaras)
void W3dLayoutMouseMoved(TInt aX, TInt aY);

// click izquierdo sobre el panel de propiedades: selecciona/deselecciona la
// fila editable bajo el cursor. Devuelve ETrue si consumio el click.
TBool W3dLayoutClickUI(TInt aX, TInt aY); // menu/barras/paneles (compartido)
TBool W3dLayoutClickProps(TInt aX, TInt aY);
TBool W3dLayoutOpenMenu();
TBool W3dLayoutOverContent3D(TInt aX, TInt aY);   // sobre el contenido de un 3D (no sus barras)
TBool W3dLayoutActivePopup(); // hay un popup modal (File browser) abierto
void  W3dLayoutCycleViewport(TInt aDir); // tecla verde: cambia viewport activo
void  W3dLayoutToggleBar();              // soft-izq: abre/cierra barra de menu
void  W3dLayoutResizeViewport(TInt aDx, TInt aDy); // verde+flechas: redimensiona en un eje
TBool W3dLayout3DActive();                // el viewport activo es un 3D?
TBool W3dLayoutFocusTransport();          // el 3D activo tiene el foco de barra en Stop/Play? (flechas al panel, no orbita)
TBool W3dLayoutFocusUVEdit();             // el UV activo tiene el foco de barra en "Edit"? (flechas al panel, no panean)
void  W3dLayoutKey8();                   // "8": loop cut en Edit Mode, o toggle vista-desde-camara-activa fuera (como numpad 0 PC)
TBool W3dLayoutGameActiveViewport();     // el viewport activo es donde se juega (3D o 2D)

// IDE (editor de scripts) por keypad: el container los usa en el bloque T9 de OfferKeyEventL. barFocusIndex < 0 =
// EDITAR TEXTO (T9 escribe, flechas = cursor, OK = nueva linea, soft-izq = salir a la barra); >= 0 = navegar la barra.
bool W3dLayoutIDEEditing();              // el IDE activo esta editando texto?
void W3dLayoutIDEInsert(int c);
void W3dLayoutIDEDelete();
void W3dLayoutIDEEnter();
void W3dLayoutIDEArrow(int aScan, bool shift); // EStdKey* de flecha -> mover el cursor del IDE (shift = seleccionar)
void W3dLayoutIDEExitEdit();           // pasar a modo barra (barFocusIndex = 0)
bool W3dLayoutIDEInBar();              // el IDE activo esta en modo barra (barFocusIndex >= 0)?
void W3dLayoutIDEReturnEdit();         // volver a editar texto (barFocusIndex = -1)
bool W3dLayoutIDEStartWord();          // cursor al inicio de palabra (modo Abc del T9)
void W3dLayoutIDECopy();               // shift + softkey-izq: copiar la seleccion al portapapeles del sistema
void W3dLayoutIDEPaste();              // shift + softkey-der: pegar del portapapeles del sistema

// Consola (kind 7): scroll con flechas mantenidas (el container las repite via ApplyArrows3D con aceleracion)
bool W3dLayoutActiveConsole();
void W3dLayoutConsoleScroll(int dx, int dy);  // dx/dy = -1/0/1 (una flecha); aplica el paso + signo internamente
TBool W3dLayoutBusy();   // scroll/divisor agarrado (cursor verde)
TBool W3dLayoutDragPopup(); // el picker arrastrando (cursor violeta)
void W3dLayoutMenuParent(TBool aClear, TInt aX, TInt aY); // ctrl+P/alt+P
void W3dLayoutMenuSnap(TInt aX, TInt aY); // shift+S (menu de snap)
void W3dLayoutRelease(TInt aX, TInt aY); // izquierdo soltado (drop)
TBool W3dLayoutWrapMouse(TInt& aX, TInt& aY); // wrap dentro del panel

// click izquierdo sobre el outliner: selecciona el objeto de esa fila.
TBool W3dLayoutClickOutliner(TInt aX, TInt aY);

// click derecho sobre un panel: entra/sale del modo scroll-arrastre XY
// (mover el mouse scrollea; cualquier click derecho sale). ETrue si consumio.
TBool W3dLayoutScrollGrab(TInt aMx, TInt aMy);

// movimiento del mouse al panel bajo el cursor (drag de scrollbar + hover)
void W3dLayoutMouseMotion(TInt aMx, TInt aMy);

// teclas de flecha/OK al panel bajo el mouse (cuando hay mouse BT, el
// teclado navega los paneles como en PC). Devuelve ETrue si consumio.
TBool W3dLayoutKey(TInt aScan, TInt aMx, TInt aMy);
// flecha MANTENIDA (frame-based) al popup activo: solo ajusta valores (color picker), no navega
TBool W3dLayoutKeyRepeat(TInt aScan);
// editor UV activo: paneo constante (o zoom si aZoom) por flecha mantenida. ETrue si lo manejo.
TBool W3dLayoutUVNav(TInt aDx, TInt aDy, TBool aZoom);
TBool W3dLayoutActiveUV(); // query: el viewport activo es el editor UV
// Timeline activo: flecha MANTENIDA = scrub (izq/der); 0 + arriba/abajo = zoom; * + flechas = paneo. ETrue si lo manejo.
TBool W3dLayoutTimelineNav(TInt aDx, TInt aDy, TBool aZoom, TBool aPan);

// ---- ruteo por VIEWPORT ACTIVO (el mismo de PC; ver main/ViewPorts/W3dInput.h) ----
TInt  W3dKeyFromSymbian(TInt aScanCode);       // scan del telefono -> tecla propia del editor
TBool W3dLayoutKeyViewport(TInt aScanCode);    // tecla -> viewport ACTIVO
TBool W3dLayoutKeyDirectViewport(TInt aKey); // idem, con la tecla ya traducida (el keypad tipea g/r/s)
TBool W3dLayoutKeyViewportUp(TInt aScanCode);
TBool W3dQuadrantViewNav(TInt aDx, TInt aDy);  // # + flechas: encuadra/gira por cuadrante (el numpad que no hay)
TBool W3dPerspectiveViewToggle();              // # solo: ortografica <-> perspectiva
TBool W3dLayoutLockOrbit();                    // 9: bloquear/desbloquear el orbital (arrastrar panea)

TBool W3dLayoutClickViewport(TInt aX, TInt aY);   // "ok" del cursor virtual = click de verdad (down)
TBool W3dLayoutReleaseViewport(TInt aX, TInt aY); // ...y su up (cierra arrastres)
TBool W3dLayoutActiveTimeline(); // query: el viewport activo es el Timeline

// keypad SIN mouse: rutea flecha/OK al viewport ACTIVO (propiedades/outliner).
// ETrue si lo consumio (el 3D devuelve EFalse: lo maneja orbit/transform).
TBool W3dLayoutKeyPanel(TInt aScan);

// EDICION NUMERICA por texto de un PropFloat: hay una en curso? + meter un caracter (0-9, '.', 8=borrar) al campo.
// El contenedor rutea aca los digitos del keypad cuando W3dActiveNumEdit() para que escriban el valor en vez de
// disparar transform/zoom.
TBool W3dActiveNumEdit();
void  W3dTextFieldChar(TInt c);

// el viewport activo es el outliner? (para que "1" entre al modo mover ahi)
TBool W3dActiveOutliner();
// "1" sobre el outliner: entra al modo MOVER (o confirma si ya esta moviendo)
void W3dOutlinerMoveToggle();
void W3dOutlinerToggleVisible();  // outliner N95: 2 = ocultar/mostrar (visible) el activo
void W3dOutlinerToggleRender();   // outliner N95: 3 = visibilidad de RENDER del activo
TBool W3dOutlinerMarkToggle();    // outliner N95 en una vista de RECURSOS: 5 = marca/desmarca el cursor (seleccion multiple)
TBool W3dOutlinerResourcesDelete(); // outliner N95 en una vista de RECURSOS: el toque de C borra la seleccion (no la escena)

// rueda sobre el outliner: scroll (consume el evento)
TBool W3dLayoutWheelOutliner(TInt aX, TInt aY, TInt aDelta);

// rueda sobre el panel de propiedades: ajusta el valor de la fila activa.
// Devuelve ETrue si consumio el evento (la rueda ahi no hace zoom).
TBool W3dLayoutWheelProps(TInt aX, TInt aY, TInt aDelta);

// resize de viewports: click sobre el divisor lo agarra, otro click lo
// suelta (el driver HID no manda movimientos con un boton apretado, asi que
// no hay arrastre clasico). Devuelve ETrue si consumio el click.
TBool W3dLayoutToggleSplitter(TInt aX, TInt aY);
TBool W3dLayoutDragging();

// rect actual del viewport 3D (para el picking del mundo nuevo)
void W3dLayoutGet3DRect(TInt& aX, TInt& aY, TInt& aW, TInt& aH, TInt& aScreenH);
void W3dLayoutDragMove(TInt aDx, TInt aDy);

// id GL de la textura aIdx del manager viejo (0 si no esta cargada).
// Para los iconos de origen del mundo nuevo (lamp.png = indice 2).

// fabrica las 96 texturas por glifo desde los pixeles RGBA de font.png
// (el texto se dibuja con point sprites: los triangulos texturizados no
// dibujan en el driver del N95). La llama el TexMgr al subir font.png.

// mouse.png (16x32) acolchada a 32x32 para dibujarla como point sprite
// (unico camino texturizado confiable del driver). La llama el TexMgr.

void W3dLayoutDestroy();

#endif // __W3DLAYOUT_H__
