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
void UI2D_BaseRecorte(int vx, int vy, int vw, int vh);

// dibuja TODOS los arboles UI de la escena sobre la ventana (x0,y0,w,h) en px de pantalla.
// escala = px de lienzo -> px de pantalla (1 en el viewport 3D; el zoom en el Editor 2D).
// outPos (opcional): junta la posicion resuelta de cada elemento (para los overlays del editor).
void UI2D_DibujarOverlay(float x0, float y0, float w, float h, float escala,
                         std::vector<UI2DPos>* outPos = 0, bool saltarVerEn3D = false);

// el tamano de la VENTANA que simula la UI: el del RENDER (Properties > Render), en vivo.
void UI2D_TamanoVentana(float* w, float* h);

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
