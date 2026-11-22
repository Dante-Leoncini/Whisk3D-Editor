#ifndef GIZMO_H
#define GIZMO_H
// ============================================================================
//  GIZMO DE TRANSFORMACION (mover / rotar / escalar) del viewport 3D. Para PC y pantallas tactiles; en
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
//  Apretar una manija arranca la MISMA traslacion de siempre (BeginTranslate / EditXformStart
//  con el eje ya elegido); moverse la aplica; SOLTAR el mouse o el dedo la confirma.
//  Sigue la orientacion Global/Local del menu (View/Normal se tratan como Global).
// ============================================================================
class ViewportBase;
class Viewport3D;

extern bool g_gizmoOn;   // Overlays > Gizmo. PC/tactil: ON; Symbian: OFF

enum GizmoHandle { GizmoNone = -1, GizmoAxisX = 0, GizmoAxisY, GizmoAxisZ, GizmoPlaneX, GizmoPlaneY, GizmoPlaneZ, GizmoCenter,
                   GizmoRingView, GizmoFree };   // (rotar) el anillo blanco = girar sobre la vista; el interior = giro libre
// MODOS: un click / toque sobre el gizmo (sin arrastrar) pasa al siguiente.
//   mover   : flechas con punta de piramide + cuadrados de plano + circulo central (libre)
//   rotar   : un semicirculo de color por eje (girar en ese eje), anillo blanco (girar sobre la vista), interior (libre)
//   escalar : como mover pero con puntas CUADRADAS; el circulo central escala parejo
enum GizmoModeType { GizmoModeMove = 0, GizmoModeRotate, GizmoModeScale };
int  GizmoMode();
void GizmoSetMode(int m);
bool GizmoTouch(ViewportBase* vp, int mx, int my);   // toque tactil sin arrastre sobre una manija: cambia de modo. true = consumido

void GizmoRender(Viewport3D* vp);                  // overlay (sin z), con las matrices de la vista cargadas
bool GizmoDown(ViewportBase* vp, int mx, int my);  // click/tap (coords de PANTALLA) sobre una manija: arranca. true = consumido
bool GizmoMotion(Viewport3D* vp, int mx, int my);  // arrastre: el punto agarrado sigue al puntero (coords de PANTALLA). true = consumido
void GizmoRelease(Viewport3D* vp);                 // soltar: confirma la traslacion en curso
bool GizmoDragging();                              // hay un arrastre del gizmo en curso
// harness: donde cae cada manija en pantalla (coords LOCALES del viewport). false = no hay gizmo
bool GizmoHandleInScreen(Viewport3D* vp, int handle, float& sx, float& sy);

// ============================================================================
//  GIZMO 2D: el MISMO gizmo para los editores 2D (las CURVAS y el dope sheet del timeline, el editor UV y el editor 2D).
//  La misma logica que el del 3D: se dibuja sobre el PIVOTE de la seleccion (el que se elige en el menu Pivot de cada
//  editor), con tamanio constante en pantalla, y un toque / click sin arrastrar pasa al modo siguiente:
//    mover   : flecha roja (solo X), flecha verde (solo Y) y el circulo blanco del centro (libre)
//    rotar   : el anillo azul (y lo de adentro): girar alrededor del pivote
//    escalar : como mover pero con puntas CUADRADAS; el circulo del centro escala parejo
//  Arrastrar una manija arranca el MISMO transform de siempre de ese editor (G/R/S, con su undo y su barra de info);
//  el punto agarrado sigue al puntero; SOLTAR lo confirma. Se prende y se apaga con el mismo g_gizmoOn del 3D.
// ============================================================================
enum Gizmo2DHandle { Gizmo2DNone = -1, Gizmo2DAxisX = 0, Gizmo2DAxisY, Gizmo2DCenter, Gizmo2DRing };
enum { Gizmo2DCanMove = 1, Gizmo2DCanRotate = 2, Gizmo2DCanScale = 4 };

// lo que cada editor 2D le ofrece al gizmo (lo heredan Timeline, UVEditor y Editor2D)
class Gizmo2DClient {
    public:
        virtual ~Gizmo2DClient() {}
        // el pivote en px LOCALES del viewport (Y abajo). false = no hay nada que transformar: no hay gizmo
        virtual bool Gizmo2DPivot(float& lx, float& ly) = 0;
        virtual int  Gizmo2DModes() const { return Gizmo2DCanMove | Gizmo2DCanRotate | Gizmo2DCanScale; }
        virtual bool Gizmo2DWithAxisY() const { return true; }   // el dope sheet solo tiene el eje del tiempo
        // la zona donde vive el contenido (el gizmo no se agarra sobre las barras ni sobre el panel de canales)
        virtual bool Gizmo2DInZone(float lx, float ly) const { (void)lx; (void)ly; return true; }
        // arranca el transform de siempre. modo: 0 mover, 1 rotar, 2 escalar; eje: 0 libre, 1 solo X, 2 solo Y
        virtual bool Gizmo2DStart(int mode, int axis) = 0;
        // valores ABSOLUTOS desde el agarre: mover = px de pantalla; rotar = radianes en pantalla (Y abajo: positivo =
        // sentido horario); escalar = factor (el eje lo fijo Gizmo2DStart)
        virtual void Gizmo2DApply(float dx, float dy, float ang, float f) = 0;
        virtual void Gizmo2DConfirm() = 0;
        virtual void Gizmo2DCancel() = 0;
};
Gizmo2DClient* Gizmo2DClientOf(ViewportBase* vp);   // NULL = ese viewport no tiene gizmo 2D
int  Gizmo2DMode();                                 // 0 mover, 1 rotar, 2 escalar (compartido por los editores 2D)
void Gizmo2DSetMode(int m);
void Gizmo2DRender(ViewportBase* vp);                 // overlay, con el Ortho(0, ancho, alto, 0) del editor puesto
bool Gizmo2DDown(ViewportBase* vp, int mx, int my);   // click/toque (coords de PANTALLA) sobre una manija. true = es del gizmo
bool Gizmo2DMotion(ViewportBase* vp, int mx, int my); // arrastre. true = consumido
bool Gizmo2DRelease(ViewportBase* vp);                // soltar: confirma (o, si no se movio, pasa al modo siguiente). true = era del gizmo
void Gizmo2DAbort();                                  // un 2do dedo: cancela lo que hubiera
void Gizmo2DForgetVp(ViewportBase* vp);               // ese viewport muere: se suelta el agarre SIN tocarlo
bool Gizmo2DDragging();
bool Gizmo2DOver(ViewportBase* vp, int mx, int my);  // hay una manija del gizmo 2D en ese punto (sin agarrarla)
// harness: donde cae una manija (coords LOCALES del viewport). false = no hay gizmo ahi
bool Gizmo2DHandleInScreen(ViewportBase* vp, int handle, float& lx, float& ly);
#endif // GIZMO_H
