/*
 * ==============================================================================
 *  w3dpantallas.h — pantallas externas en Symbian (TV-out RCA del N95/N8, HDMI del N8)
 *
 *  Detecta las pantallas que wserv expone ademas de la del telefono (RWsSession::NumberOfScreens) y, en la
 *  variante ES 2.0, confirma con el AccMonitor que haya un cable de video conectado. Al conectarse una avisa al
 *  editor (PantallasExternas.h: el popup "clonar / nuevo viewport / propiedades"); si se elige un viewport, crea
 *  en esa pantalla un window group + ventana + superficie EGL que comparte el contexto GL de la principal.
 *  La variante ES 1.1 (N95) dibuja la externa a 240p como maximo (320x240): es lo que la salida muestra.
 * ==============================================================================
 */
#ifndef __W3DPANTALLAS_H__
#define __W3DPANTALLAS_H__

#include <GLES/egl.h>

// despues del eglMakeCurrent de la principal: guarda los handles y engancha los ganchos del editor
void W3dPantallasInit(EGLDisplay aDisplay, EGLConfig aConfig, EGLContext aContext, EGLSurface aPrincipal);
// (ES 2.0) una config compatible cuyo swap puede conservar el cuadro: la superficie de la TV la prueba primero
void W3dPantallasConfigConserva(EGLConfig aConfig);
// cada cuadro del timer (revisa conexiones una vez por segundo: es barato)
void W3dPantallasTick();
// al cerrar la app, antes de eglTerminate
void W3dPantallasDestroy();
// el swap de esa pantalla conserva el cuadro (se puede redibujar solo el cursor encima)
TBool W3dPantallaConservaCuadro(TInt aId);

#endif
