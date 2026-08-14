#ifndef W3DDOCK_H
#define W3DDOCK_H

// ============================================================================
//  W3dDock - barra de progreso EN EL ICONO del dock/lanzador (Linux de escritorio).
//
//  QUE ES: la API "LauncherEntry" que invento Unity y que hoy siguen soportando
//  el dock de Ubuntu (Dash-to-Dock), KDE Plasma y varios docks mas. Se emite una
//  senal D-Bus 'Update' en com.canonical.Unity.LauncherEntry con el id de la app
//  ("application://whisk3d.desktop") y un diccionario con 'progress' (0..1) y
//  'progress-visible'. El dock dibuja la barrita sobre el icono.
//
//  ES UN EXTRA, NO EL LUGAR PRINCIPAL: en GNOME pelado (sin Dash-to-Dock) NO se
//  ve nada, y desde el AppImage o desde el build de desarrollo tampoco (el id no
//  matchea ningun .desktop instalado). La barra que el editor dibuja adentro
//  sigue siendo la de verdad; esto solo la acompana cuando el editor esta
//  minimizado o tapado.
//
//  SIN DEPENDENCIAS DE COMPILACION: GIO/GLib se cargan con dlopen EN EJECUCION.
//  Si la libreria no esta, o no hay bus de sesion, todo queda en no-op silencioso
//  (W3dDockDisponible() devuelve false) y el editor anda igual.
//
//  EN EL RESTO DE LAS PLATAFORMAS (Windows/Android/Symbian/Web) esto NI EXISTE:
//  son inline vacias, no hay .cpp que compilar ni simbolo que linkear (importa
//  para Symbian, que lista los .cpp a mano en el .mmp).
// ============================================================================

#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)

// Prende la barra del icono y le setea el avance (frac 0..1, se clampea).
// Se llama con el MISMO avance que ya publica la operacion; no lleva cuenta propia.
void W3dDockProgreso(float frac);

// Apaga la barra del icono. Tiene que llamarse SI O SI al terminar, al fallar y al
// cancelar: si no, el icono queda con una barra a medias para siempre. Es no-op si
// ya estaba apagada, asi que se puede llamar de mas (red de seguridad).
void W3dDockProgresoFin();

// true si la barra esta LOGICAMENTE prendida (lo pedimos), este o no el D-Bus.
bool W3dDockProgresoEncendido();

// true si se pudo cargar GIO y conectar al bus de sesion (o sea: se ve de verdad).
// false = todo lo de arriba es no-op silencioso. Solo para diagnostico/tests.
bool W3dDockDisponible();

#else

inline void W3dDockProgreso(float) {}
inline void W3dDockProgresoFin() {}
inline bool W3dDockProgresoEncendido() { return false; }
inline bool W3dDockDisponible() { return false; }

#endif

#endif // W3DDOCK_H
