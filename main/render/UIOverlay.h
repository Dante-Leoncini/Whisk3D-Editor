#ifndef UI_OVERLAY_H
#define UI_OVERLAY_H
// ============================================================================
//  UIOverlay — dibuja las interfaces 2D (objetos UI y sus elementos) sobre una
//  "ventana": el marco del Editor 2D o el viewport 3D entero (que SIMULA la
//  ventana del juego/programa). Un solo dibujador para los dos lados: lo que
//  ves en el editor es exactamente lo que ves corriendo.
//
//  ANCLAS: cada elemento se agarra a un punto de su padre (centro por defecto,
//  o un borde/esquina) y pos.x/pos.y son el corrimiento desde ese punto. Un
//  elemento con pos en cero anclado a una esquina muestra 1/4 de si mismo y
//  anclado a un borde la mitad: queda EN el borde, no metido adentro. Al
//  cambiar el tamano de la ventana todo se recalcula solo.
// ============================================================================
#include <vector>

class Object;
class UI;
class Slice9;

// posicion RESUELTA en pantalla de un elemento 2D (para el origen, el pivote y el grab)
// + el rectangulo que ocupa (para seleccion y puntos de agarre; sin la rotacion aplicada)
// + el tamano del rect de REFERENCIA de su ancla (la posicion se guarda RELATIVA a el)
// cx0..cy1 = el RECORTE vigente cuando se dibujo (overflow de los ancestros): el editor
// solo hit-testea dentro (un simbolo recortado por su rodillo no se clickea donde no se ve)
struct UI2DPos { Object* obj; float sx, sy; float bx0, by0, bx1, by1; float refW, refH;
                 float cx0, cy0, cx1, cy1; };

// punto de anclaje 'ancla' (0..8) dentro del rect (x0,y0,w,h)
void UI2D_PuntoAncla(int ancla, float x0, float y0, float w, float h, float* ax, float* ay);

// origen y tamano ABSOLUTOS del viewport que va a dibujar el overlay (coords de arbol,
// top-left). Lo necesita el RECORTE (overflow): el scissor de GL es absoluto. Llamalo
// antes de UI2D_DibujarOverlay; al terminar el scissor queda en el rect completo.
// clipLocal (opcional): {x0,y0,x1,y1} en coords LOCALES del viewport = el recorte de
// arranque, por si el que dibuja no puede usar TODO su rect (ej: la VISTA DE JUEGO del
// Editor 2D descuenta la barra de menu). NULL = el rect entero, que es lo de siempre.
void UI2D_BaseRecorte(int vx, int vy, int vw, int vh, const float* clipLocal = 0);

// dibuja UNA escena UI (la de UI2D_UIDelEditor: la activa en Play/runtime, la que se edita
// en edicion) sobre la ventana (x0,y0,w,h) en px de pantalla. MULTI-ESCENA: no apila varias.
// escala = px de lienzo -> px de pantalla (1 en el viewport 3D; el zoom en el Editor 2D).
// outPos (opcional): junta la posicion resuelta de cada elemento (para los overlays del editor).
void UI2D_DibujarOverlay(float x0, float y0, float w, float h, float escala,
                         std::vector<UI2DPos>* outPos = 0, bool saltarVerEn3D = false);

// el tamano de la VENTANA que simula la UI: el del RENDER (Properties > Render), en vivo.
void UI2D_TamanoVentana(float* w, float* h);

// OVERRIDE de la ventana (SOLO editor): mientras un viewport 2D esta en VISTA DE JUEGO y la UI es
// "igual que el render", la ventana simulada ES el rect util de ESE viewport ("el editor 2D es el
// verdadero render del juego", como la ventana real en la rama igualQueRender de w3drun). Lo fija
// el propio viewport al dibujar/alimentar input y lo limpia al salir del modo (Editor2D). 'dueno'
// identifica quien lo fijo: otro viewport puede pisarlo (el ultimo gana) y Quitar solo limpia si
// sigue siendo el dueno, asi dos viewports 2D no se borran el override entre si. Con la UI de
// tamano FIJO (igualQueRender=false) NO hay override: el lienzo fijo se letterboxea como siempre.
void UI2D_OverrideVentana(const void* dueno, float w, float h);
void UI2D_OverrideVentanaQuitar(const void* dueno);

// leer/reponer el estado CRUDO del override (los usa el scope de abajo)
void UI2D_OverrideVentanaLeer(const void** dueno, float* w, float* h);
void UI2D_OverrideVentanaReponer(const void* dueno, float w, float h);

// OVERRIDE CON ALCANCE: fija la ventana simulada SOLO mientras el objeto vive
// (dibujar el overlay de UN viewport, mapear UN toque) y al salir REPONE lo que
// habia. Es la mitad "por viewport" del estado del overlay: con varios
// viewports 3D, cada uno dibuja su HUD con SU pantalla sin pisar el override
// PERSISTENTE -- el que leen los binds del juego (pantalla()/pantallaDe) entre
// frames --, que publica solo el viewport ACTIVO. Sin esto, el ultimo viewport
// en dibujar (o el ultimo click) dejaba SU pantalla puesta y la UI del juego se
// re-armaba a cada rato con un lienzo distinto ("se agranda y achica como loca").
class UI2D_OverrideVentanaScope {
public:
    // guarda el estado actual y (opcional) pone ya mismo el override pedido
    UI2D_OverrideVentanaScope() { UI2D_OverrideVentanaLeer(&dueno, &w, &h); }
    UI2D_OverrideVentanaScope(const void* d, float ww, float hh) {
        UI2D_OverrideVentanaLeer(&dueno, &w, &h);
        UI2D_OverrideVentana(d, ww, hh);
    }
    ~UI2D_OverrideVentanaScope() { UI2D_OverrideVentanaReponer(dueno, w, h); }
private:
    const void* dueno; float w, h;
};

// el UI que esta editando el Editor 2D: el de la cadena del objeto activo, o el
// primero de la escena (NULL si no hay ninguno).
UI* UI2D_UIDelEditor();
// el tamano del LIENZO del Editor 2D: el de su UI ("como el render", o el tamano
// propio si esta en modo responsive). Sin UI en escena: el del render.
void UI2D_TamanoLienzo(float* w, float* h);

// true si el objeto es un ELEMENTO 2D (texto/imagen/rectangulo; NO la raiz UI)
bool UI2D_EsElemento2D(Object* o);
// la rot2d del elemento (NULL si no es un elemento 2D)
float* UI2D_Rot2dDe(Object* o);
// punteros al ancho/alto del elemento, si tiene rect propio (imagen/rectangulo):
// los usan los puntos de agarre y la escala del editor. false si no (texto).
bool UI2D_TamanoElem(Object* o, float** w, float** h);

// la escalaGlobal del UI raiz del elemento (1 si no cuelga de ninguno)
float UI2D_EscalaGlobalDe(Object* o);
// el factor de escala DE PANTALLA tipo dpi: min(ancho, alto) del lienzo / 480 (el
// MISMO numero que el bind escala() de los juegos). Lo usan los tamanos en unidad
// ESCALADA (tamModo 2): valor * este factor = px de lienzo, igual de grande fisico
// en vertical y en horizontal.
float UI2D_FactorEscala();
// donde CAEN los cortes del slice9 (sus bordes dibujados, ya clampeados) dentro del rect
// dado: dX/dY en px de pantalla. escalaDest = escala SIN el zoom de la vista.
void UI2D_CortesSlice9(Slice9* s9, float x0, float y0, float x1, float y1,
                       float escalaDest, float* dX, float* dY);

// hay algun UI con "ver en 3D" prendido?
bool UI2D_HayVerEn3D();
// dibuja los elementos EN EL MUNDO 3D (con su profundidad Z), usando las matrices de la
// escena ya activas. Para inspeccionar la profundidad desde el viewport 3D.
void UI2D_DibujarEnMundo();

#endif // UI_OVERLAY_H
