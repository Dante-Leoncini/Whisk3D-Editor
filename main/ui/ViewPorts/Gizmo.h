#ifndef GIZMO_H
#define GIZMO_H
// ============================================================================
//  GIZMO DE TRANSFORMACION (mover) del viewport 3D. Para PC y pantallas tactiles; en
//  Symbian arranca apagado (Overlays > Gizmo).
//
//  Se dibuja sobre el pivote de la seleccion (objetos en Object Mode, verts en Edit Mode)
//  con tamanio constante en pantalla:
//    - 3 lineas con una PIRAMIDE en la punta (rojo X, verde Y, azul Z): arrastrar una
//      mueve SOLO en ese eje;
//    - 3 cuadrados entre cada par de ejes: arrastrar el azul mueve en rojo+verde (el
//      plano perpendicular al azul), etc. En tactil son mas grandes para darles con el dedo;
//    - un circulo blanco de borde blando en el centro: arrastrarlo es como apretar G y
//      mover en la vista (libre).
//  Apretar una manija arranca la MISMA traslacion de siempre (SetPosicion / EditXformStart
//  con el eje ya elegido); moverse la aplica; SOLTAR el mouse o el dedo la confirma.
//  Sigue la orientacion Global/Local del menu (View/Normal se tratan como Global).
// ============================================================================
class ViewportBase;
class Viewport3D;

extern bool g_gizmoOn;   // Overlays > Gizmo. PC/tactil: ON; Symbian: OFF

enum GizmoManija { GizmoNinguna = -1, GizmoEjeX = 0, GizmoEjeY, GizmoEjeZ, GizmoPlanoX, GizmoPlanoY, GizmoPlanoZ, GizmoCentro };

void GizmoRender(Viewport3D* vp);                  // overlay (sin z), con las matrices de la vista cargadas
bool GizmoDown(ViewportBase* vp, int mx, int my);  // click/tap (coords de PANTALLA) sobre una manija: arranca. true = consumido
bool GizmoMotion(Viewport3D* vp, int mx, int my);  // arrastre: el punto agarrado sigue al puntero (coords de PANTALLA). true = consumido
void GizmoSoltar(Viewport3D* vp);                  // soltar: confirma la traslacion en curso
bool GizmoArrastrando();                           // hay un arrastre del gizmo en curso
// harness: donde cae cada manija en pantalla (coords LOCALES del viewport). false = no hay gizmo
bool GizmoManijaEnPantalla(Viewport3D* vp, int manija, float& sx, float& sy);
#endif // GIZMO_H
