/*
 * ==============================================================================
 *  w3dlayout.h — Fase 1 del plan de unificacion (ver PLAN-UNIFICACION.md)
 *
 *  Arma el arbol de viewports compartido (core/ui/W3dViewports) para Symbian:
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
TBool W3dLayoutMouseSobre3D(TInt aX, TInt aY);

// el cursor se movio: actualiza que viewport esta activo (cambia camaras)
void W3dLayoutMouseMoved(TInt aX, TInt aY);

// click izquierdo sobre el panel de propiedades: selecciona/deselecciona la
// fila editable bajo el cursor. Devuelve ETrue si consumio el click.
TBool W3dLayoutClickUI(TInt aX, TInt aY); // menu/barras/paneles (compartido)
TBool W3dLayoutClickProps(TInt aX, TInt aY);
TBool W3dLayoutMenuAbierto();
TBool W3dLayoutPopupActivo(); // hay un popup modal (File browser) abierto
void  W3dLayoutCiclarViewport(TInt aDir); // tecla verde: cambia viewport activo
void  W3dLayoutToggleBarra();             // soft-izq: abre/cierra barra de menu
void  W3dLayoutRedimensionarViewport(TInt aDx, TInt aDy); // verde+flechas: redimensiona en un eje
TBool W3dLayout3DActivo();                // el viewport activo es un 3D?
TBool W3dLayoutOcupado();   // scroll/divisor agarrado (cursor verde)
TBool W3dLayoutArrastrePopup(); // el picker arrastrando (cursor violeta)
void W3dLayoutMenuParent(TBool aClear, TInt aX, TInt aY); // ctrl+P/alt+P
void W3dLayoutMenuSnap(TInt aX, TInt aY); // shift+S (menu de snap)
void W3dLayoutSoltar(TInt aX, TInt aY); // izquierdo soltado (drop)
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
TBool W3dLayoutTecla(TInt aScan, TInt aMx, TInt aMy);
// flecha MANTENIDA (frame-based) al popup activo: solo ajusta valores (color picker), no navega
TBool W3dLayoutTeclaRepeat(TInt aScan);
// editor UV activo: paneo constante (o zoom si aZoom) por flecha mantenida. ETrue si lo manejo.
TBool W3dLayoutUVNav(TInt aDx, TInt aDy, TBool aZoom);
TBool W3dLayoutUVActivo(); // query: el viewport activo es el editor UV

// keypad SIN mouse: rutea flecha/OK al viewport ACTIVO (propiedades/outliner).
// ETrue si lo consumio (el 3D devuelve EFalse: lo maneja orbit/transform).
TBool W3dLayoutTeclaPanel(TInt aScan);

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
