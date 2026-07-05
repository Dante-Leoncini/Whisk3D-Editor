#ifndef VIEWPORTS_H
#define VIEWPORTS_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "objects/Objects.h"
#include "WhiskUI/UI.h" // globals de escala/layout (compartidos)
#include "WhiskUI/colores.h" // paleta compartida (ListaColores/ColorID)
#include "WhiskUI/icons.h"   // iconos compartidos (mismo atlas font.png)
#include "WhiskUI/Button.h"  // boton compartido (barra de viewports)
#include "WhiskUI/rectangle.h" // linea separadora de la barra
#include "WhiskUI/Tab.h"       // pestanias de la barra (properties)
#include "variables.h" // REAL, ahora portable

// El icono de un objeto se DERIVA de su tipo (el core ya no guarda iconos de UI).
// Compartido por Outliner + Properties; definido en Outliner.cpp.
size_t IconoDeObjeto(Object* o);

#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
    #include <SDL2/SDL.h>
    #include <iostream>
    #include "WhiskUI/UI.h"
    #include "WhiskUI/font.h"
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

// Variables UV/indices
extern GLubyte indicesBorder[];
extern GLfloat bourderUV[32];

// -----------------------------
// Clase base de Viewport
// -----------------------------
class ViewportBase {
    public:
        int x, y;             // (inicializados en el constructor: C++03)
        int width, height;
        ViewportBase* parent;

        ViewportBase();

        // Métodos virtuales para que cada vista defina su comportamiento
        virtual ~ViewportBase();

        virtual bool Contains(int mx, int my) const;
        virtual bool isLeaf() const;
        // 0 = hoja, 1 = fila, 2 = columna (sin RTTI: RVCT no tiene)
        virtual int ContainerKind() const { return 0; }
        // tipo de hoja: 1 = 3D, 2 = outliner, 3 = properties
        virtual int ViewportKind() const { return 0; }

        virtual void event_mouse_motion(int mx, int my);
        virtual void button_left();
        virtual void button_right();
        virtual void button_up();
        virtual void button_down();
#ifndef W3D_SYMBIAN
        virtual void event_key_down(SDL_Event &e);
        virtual void event_key_up(SDL_Event &e);

        virtual void event_mouse_wheel(SDL_Event &e);
        virtual void mouse_button_up(SDL_Event &e);
#endif
        virtual void event_finger_motion(float pinch);

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
#ifndef W3D_SYMBIAN
        void mouse_button_up(SDL_Event &e) override;
#endif
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
#ifndef W3D_SYMBIAN
        void mouse_button_up(SDL_Event &e) override;
#endif
        void event_mouse_motion(int mx, int my) override;
};

#endif