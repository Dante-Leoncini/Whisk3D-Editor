#ifndef VIEWPORTS_H
#define VIEWPORTS_H

#include "W3dInput.h"   // teclas/botones propios: los viewports no dependen de SDL (ver el header)

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "objects/Objects.h"
#include "WhiskUI/core/UI.h" // globals de escala/layout (compartidos)
#include "WhiskUI/theme/colores.h" // paleta compartida (ListaColores/ColorID)
#include "WhiskUI/draw/icons.h"   // iconos compartidos (mismo atlas font.png)
#include "WhiskUI/widgets/Button.h"  // boton compartido (barra de viewports)
#include "WhiskUI/draw/rectangle.h" // linea separadora de la barra
#include "WhiskUI/widgets/Tab.h"       // pestanias de la barra (properties)
#include "variables.h" // REAL, ahora portable

class Scrollable; // ScrollBar.h (que incluye a este: por eso va declarada, no incluida)

// El icono de un objeto se DERIVA de su tipo (el core ya no guarda iconos de UI).
// Compartido por Outliner + Properties; definido en Outliner.cpp.
size_t IconoDeObjeto(Object* o);

#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
    #include <SDL2/SDL.h>
    #include <iostream>
    #include "WhiskUI/text/font.h"
#endif

// Enum de vistas
// dialecto C++03 compartido (RVCT no tiene enum class)
struct View {
    enum Enum {
        ViewPort3D,
        Outliner,
        Properties,
        UVeditor,
        Timeline,
        GraphEditor,
        Editor2D,   // editor 2D (interfaces/juegos 2D); ViewportKind() = 6
        Row,
        Column
    };
    Enum v;
    View(Enum e) : v(e) {}
    operator Enum() const { return v; }
};

// Adelantos de clases
class ViewportBase;

// Variables globales
extern ViewportBase* viewPortActive;
extern ViewportBase* rootViewport;

// Funciones globales
void CalcBorderUV(int texW, int texH);
ViewportBase* FindViewportUnderMouse(ViewportBase* vp, int mx, int my);
void SetGlobalScale(int scale);
void CheckWarpMouseInViewport(int mx, int my, const ViewportBase* vp);

// Botones de barra por ROL (no por indice): reordenar una barra no rompe el dispatch.
// Sirven para la barra superior (BarButtons) Y la toolbar (ToolButtons). Def en ViewPort3D.cpp.
Button* BarRolBtn(std::vector<Button*>& B, int rol); // el boton con ese rol (NULL si no esta)
int     BarRolIdx(std::vector<Button*>& B, int rol); // su INDICE (para la nav izq/der), -1 si no esta

// MRU de acciones de una toolbar (compartido; def en ToolbarBase.cpp): 'id' pasa al frente,
// sin repetidos, hasta 8. Cada editor tiene SU historial (el 3D dos: objeto/edicion).
void ToolbarMRU(std::vector<int>& hist, int id);
// texto de una accion TB* del historial (TBMove/TBRotate/... de variables.h), ya traducido.
const char* ToolbarAccionLabel(int id);

// Variables UV/indices
extern GLubyte indicesBorder[];
extern GLfloat bourderUV[32];

/**
 * @brief Clase base de todo panel de la UI del editor.
 *
 * Cada vista de la pantalla hereda de ViewportBase: el viewport 3D (Viewport3D), la línea de tiempo (Timeline), el
 * panel de propiedades (Properties), el árbol de objetos (Outliner) y el editor de UVs (UVEditor). La base define el
 * contrato de ENTRADA (clicks, teclas, rueda) y DIBUJO; el ruteo de eventos (LayoutInput) le manda al viewport
 * ACTIVO por esta interfaz sin saber cuál es. Eso es lo que permite agregar una vista nueva sin tocar el ruteo.
 *
 * El layout es un ÁRBOL: los contenedores ViewportRow / ViewportColumn parten la pantalla y contienen viewports-hoja.
 * @c ContainerKind() distingue hoja/fila/columna sin RTTI (el compilador del N95 no lo tiene).
 *
 * Para agregar una vista: heredar de ViewportBase e implementar @c Render() + los @c event_*(). El polimorfismo hace
 * que el resto del sistema la trate por la interfaz — agregás una clase, no editás diez archivos.
 */
class ViewportBase {
    public:
        int x, y;             // (inicializados en el constructor: C++03)
        int width, height;

        ViewportBase();

        // Métodos virtuales para que cada vista defina su comportamiento
        virtual ~ViewportBase();

        virtual bool Contains(int mx, int my) const;
        virtual bool isLeaf() const;
        // 0 = hoja, 1 = fila, 2 = columna (sin RTTI: RVCT no tiene)
        virtual int ContainerKind() const { return 0; }
        // tipo de hoja: 1 = 3D, 2 = outliner, 3 = properties, 4 = UV, 5 = timeline,
        // 6 = editor 2D, 7 = consola, 8 = IDE (lua)
        virtual int ViewportKind() const { return 0; }

        // Barra de scroll: un viewport que scrollea hereda Scrollable y devuelve 'this' aca. El ruteo de input
        // (hover / agarre / arrastre / touch) es UNO SOLO y generico para todos: ver LayoutInput.
        // Antes esto era una tabla de downcasts a mano indexada por ViewportKind(), en LayoutInput: para que un
        // viewport nuevo tuviera scroll habia que acordarse de agregarlo ALLA. Si te olvidabas, la barra se
        // dibujaba y no respondia a nada. Asi el enganche vive en el propio viewport y es una linea.
        virtual Scrollable* ComoScrollable() { return NULL; }

        virtual void event_mouse_motion(int mx, int my);
        virtual void button_left();
        virtual void button_right();
        virtual void button_up();
        // El viewport abre el menu de barra que este bajo (mx,my), si hay alguno. Lo llama el ruteo compartido al
        // pasar el mouse SIN click, para deslizarse de un menu a otro. Solo menus: deslizarse por encima de un
        // boton de accion no puede dispararla. false = ahi no hay ningun menu.
        virtual bool AbrirMenuDeBarra(int mx, int my) { (void)mx; (void)my; return false; }
        virtual void button_down();
        // Input: teclas/rueda/soltar. Ya no dependen de SDL (ver W3dInput.h), asi que existen en TODAS las
        // plataformas: es lo que permite que el ruteo por viewport activo sea UNO SOLO. Mientras pedian un
        // SDL_Event, en el telefono se compilaban afuera y cada tecla habia que reinventarla desde el otro lado.
        virtual void event_key_down(int tecla, bool repeticion);
        virtual void event_key_up(int tecla);
        // La rueda ocurre EN una posicion, y esa posicion VIENE CON EL EVENTO. No se lee de lastMouseX/Y: esos
        // son el ULTIMO CLICK, no donde esta el cursor -- solo los refrescan el click y el warp. Con el global, la
        // rueda sobre Properties no scrolleaba hasta que clickeabas algo (la barra se la comia por una posicion
        // vieja).
        virtual void event_mouse_wheel(float dy, int mx, int my);
        virtual void mouse_button_up(int boton);
        // gesto de 2 dedos (web/movil): zoomDelta = pinch (abrir dedos = acercar); panDx/panDy = arrastre
        // del punto medio en pixeles. Default vacio; lo implementa el Viewport3D (zoom + paneo).
        virtual void event_finger_gesture(float zoomDelta, float panDx, float panDy);
        // arrastre de 1 dedo sobre un PANEL: scrollear (px/py = pos del dedo; dx/dy = delta). Devuelve
        // true si lo consumio (paneles + toolbar del viewport) -> ahi el mouse NO orbita/selecciona.
        virtual bool event_finger_scroll(int px, int py, int dx, int dy);
        // scroll HORIZONTAL de la barra superior (botones + pestañas), UNIFICADO para rueda y touch en
        // TODOS los viewports. delta>0 = mostrar contenido de la izquierda. Devuelve true si (px,py) cae
        // en la barra (ahi consume el evento). La usan los event_mouse_wheel / event_finger_scroll.
        bool BarScrollHorizontal(int px, int py, int delta);
        // virtuales: la barra superior O la de HERRAMIENTAS (abajo) entran al hit-test/scroll.
        // OnBar setea toolGesto para que BarScrollBy scrollee la barra donde ARRANCO el gesto.
        virtual bool OnBar(int px, int py);  // (px,py) cae en una barra? (hit-test, sin scrollear)
        virtual void BarScrollBy(int delta); // scroll horizontal de la barra SIN hit-test (gesto ya lockeado)
        // touch: (mx,my) cae sobre un campo NUMERICO editable (value box)? Default false; lo redefine
        // Properties. Si true, el arrastre horizontal edita (slider) en vez de scrollear.
        virtual bool PuntoEnCampoNumerico(int mx, int my) { return false; }
        // SLIDER TACTIL: arrastre horizontal dentro de un campo numerico edita su valor (independiente del
        // gFloatDrag de mouse). Armar guarda el campo bajo (mx,my); Mover suma dx*dragStep; Soltar limpia.
        virtual bool TouchSliderArmar(int mx, int my) { return false; }
        virtual void TouchSliderMover(int dx) {}
        virtual void TouchSliderSoltar() {}

        virtual void Render() = 0;
        virtual void Resize(int newW, int newH);

        // barra de botones del viewport, siempre visible (arriba o,
        // a eleccion del usuario, abajo)
        bool barAbajo;
        GLfloat barAlpha; // 1 = opaca; el 3D usa 0.5 (se ve la escena)
        std::vector<Button*> BarButtons; // [0] = boton de icono (derecha)
        std::vector<Tab*> BarTabs; // pestanias (por ahora sin accion)
        // indice del menu ENFOCADO con el teclado (-1 = ninguno). La barra se
        // auto-scrollea para centrarlo (menus mas anchos que la pantalla, Symbian)
        int barFocusIndex;
        int barScrollX; // desplazamiento horizontal actual de la barra (px)
        int barScrollManual; // scroll por rueda del mouse (PC); se usa cuando NO
                             // hay foco de teclado (sino el foco centra el boton)
        Card* barCard;
        Rec2D* barLinea; // separador oscuro bajo la barra (no en el 3D)

        void BarCrear();   // crea la barra con el boton de icono comun
        int BarHeight() const;
        int BarTopOffset() const; // alto que la barra le come al contenido
        void RenderBar();  // dibujar al final del Render (en ortho 2D)
        // recalcula el layout de la barra (anchos, auto-scroll que centra el
        // menu enfocado, y los sx/sy absolutos de botones/pestañas). La llama
        // RenderBar antes de dibujar y el ruteo de teclado antes de abrir un
        // menu (asi el hit-test usa la posicion YA scrolleada).
        void ActualizarBarra();
        void BarHover(int mx, int my);
        bool BarClick(int mx, int my); // true: el click es de la barra

        // ---- BARRA DE HERRAMIENTAS (abajo), COMPARTIDA. Mecanismo unico (ToolbarBase.cpp):
        // botones persistentes por ROL, scroll horizontal, hit tolerante (tap desviado agarra el
        // boton mas cercano en X), render y MRU (ToolbarMRU). Cada editor pone SUS botones en el
        // ctor y define la parte CONTEXTUAL en dos virtuales chicos:
        //   ToolbarSincronizar() = visibilidad/colores/texto segun el estado (puro, sin GL -> testeable)
        //   ToolbarAccionRol(r)  = que hace el boton con rol r al clickearlo
        // La usan el 3D (historial+transform), el UV editor y el Editor 2D (G/R/S). Un viewport sin
        // ToolButtons no tiene toolbar (OnToolbar/Render devuelven que no hay). ----
        std::vector<Button*> ToolButtons; // botones persistentes (con ->rol)
        int  toolScroll;                  // scroll horizontal manual de la toolbar
        bool toolGesto;                   // el gesto de arrastre arranco sobre la toolbar (no la barra de arriba)
        virtual bool ToolbarVisible() const;   // default: hay botones. Cada editor lo restringe (contexto)
        int  ToolbarHeight() const;            // = BarHeight()
        bool OnToolbar(int px, int py) const;  // (px,py) cae en la toolbar?
        void ToolbarScrollBy(int delta);
        virtual void ToolbarSincronizar() {}   // visibilidad CONTEXTUAL (estado puro; cada editor la define)
        void ToolbarActualizar();              // Sincronizar + layout (anchos, clamp del scroll, sx/sy absolutos)
        bool ToolbarClick(int mx, int my);     // hit tolerante + despacho por rol (true = consumido)
        virtual void ToolbarAccionRol(int rol) { (void)rol; } // accion del boton con ese rol
        virtual void RenderToolbar();          // fondo + botones (al final del Render, en ortho 2D)

        // ---- TRANSFORM UI COMPARTIDO (TransformUI.cpp): barra de info + entrada numerica +
        // envolver el cursor + tilde/cruz/ejes de la toolbar. Un editor con G/R/S MODAL propio
        // (UV editor / Editor 2D) implementa estos hooks chicos y lo comun corre por la base,
        // sin duplicar. El 3D NO los implementa: su transform va por el estado global (es la
        // referencia de la que se extrajo esta mecanica) y NumInput cae a sus caminos de siempre. ----
        virtual bool XformEnCurso() const { return false; }   // hay un G/R/S propio en curso
        virtual void XformNumValor(float v) { (void)v; }      // aplicar el valor exacto tipeado (NumInput)
        virtual void XformConfirmar() {}                      // tilde / Enter / click
        virtual void XformCancelar() {}                       // cruz / Esc / click derecho
        virtual void XformToggleEje(int eje) { (void)eje; }   // 1=X 2=Y (teclas y toolbar, MISMO camino)
        virtual std::string XformTextoBarra() { return std::string(); } // texto de la barra de info
        // durante el transform la barra de menu se vuelve barra de INFO (fondo + texto accent);
        // extraida del RenderBarraTransform del 3D. Def. en TransformUI.cpp.
        void RenderBarraInfo(const std::string& texto);

        // el mouse se fue a OTRO viewport: apagar el hover propio
        virtual void ClearHover() {}
};

// -----------------------------
// ViewportRow
// -----------------------------
class ViewportRow : public ViewportBase {
    public:
        ViewportBase* childA;
        ViewportBase* childB;
        float splitFrac;

        // ------------------ Constructor / Destructor ------------------
        ViewportRow(ViewportBase* a = NULL, ViewportBase* b = NULL, float frac = 0.5f);
        ~ViewportRow();

        // ------------------ Funciones override ------------------
        bool isLeaf() const override;
        int ContainerKind() const override { return 1; }

        void Resize(int newW, int newH) override;
        void Render() override;
        void button_left() override;
        void mouse_button_up(int boton) override;
        void event_mouse_motion(int mx, int my) override;

        // ------------------ Funciones propias ------------------
        void SetSizeChildrens(int move);
};

// -----------------------------
// ViewportColumn
// -----------------------------
class ViewportColumn : public ViewportBase {
    public:
        ViewportBase* childA;
        ViewportBase* childB;
        float splitFrac;

        ViewportColumn(ViewportBase* a = NULL, ViewportBase* b = NULL, float frac = 0.5f);
        ~ViewportColumn();

        bool isLeaf() const override;
        int ContainerKind() const override { return 2; }

        void Resize(int newW, int newH) override;
        void SetSizeChildrens(int move);
        void Render() override;

        void button_left() override;
        void mouse_button_up(int boton) override;
        void event_mouse_motion(int mx, int my) override;
};

#endif