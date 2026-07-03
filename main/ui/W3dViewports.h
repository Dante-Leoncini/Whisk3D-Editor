/*
 * ==============================================================================
 *  W3dViewports.h — arbol de viewports COMPARTIDO (whisk3dcore)
 *
 *  Port portable de main/ViewPorts/ViewPorts.{h,cpp} (PC). Reglas (ver
 *  PLAN-UNIFICACION.md):
 *   - Dialecto C++03: compila con RVCT 2.2 (Symbian) y con cualquier
 *     compilador moderno de PC/Android. Nada de enum class / nullptr /
 *     lambdas / auto / dynamic_cast.
 *   - CERO dependencias de plataforma: ni SDL, ni GL, ni Avkon. El arbol solo
 *     hace layout y ruteo de input; las HOJAS (que dibujan) las define cada
 *     app heredando de W3dViewportBase.
 *   - Sin RTTI: cada nodo declara su tipo con Kind().
 *
 *  Fase 2 del plan: main/ViewPorts de PC pasa a heredar de estas clases y
 *  borra su copia de Row/Column. Hasta entonces conviven.
 * ==============================================================================
 */

#ifndef W3D_VIEWPORTS_H
#define W3D_VIEWPORTS_H

// Tipos de nodo (enum plano, C++03)
enum W3dViewKind {
    W3dView_Leaf = 0,   // hoja generica (Viewport3D, Properties, etc.)
    W3dView_Row,        // divide en 2 columnas (A izquierda, B derecha)
    W3dView_Column      // divide en 2 filas (A arriba, B abajo)
};

// Evento de input portable. Cada plataforma traduce lo suyo a esto
// (SDL_Event en PC, eventos HID/Avkon en Symbian).
struct W3dEvent {
    enum Type {
        MouseMove = 0,
        MouseDownLeft,
        MouseDownRight,
        MouseDownMiddle,
        MouseUp,
        MouseWheel,
        KeyDown,
        KeyUp
    };
    int type;       // W3dEvent::Type
    int x, y;       // posicion absoluta del cursor (pixeles de ventana)
    int dx, dy;     // delta de movimiento
    int wheel;      // +arriba / -abajo
    int key;        // keycode (significado por plataforma, por ahora)

    W3dEvent() : type(0), x(0), y(0), dx(0), dy(0), wheel(0), key(0) {}
};

class W3dViewportBase {
    public:
        int x, y;
        int width, height;
        W3dViewportBase* parent;

        W3dViewportBase();
        virtual ~W3dViewportBase();

        virtual int  Kind() const;          // W3dViewKind (sin RTTI)
        virtual bool Contains(int mx, int my) const;
        virtual bool isLeaf() const;

        // layout
        virtual void Resize(int newW, int newH);

        // dibujo: lo implementa cada hoja con el backend de su plataforma
        virtual void Render() = 0;

        // input (default: ignorar)
        virtual void OnEvent(const W3dEvent& e);
};

// ----------------------------------------------------------------------------
// Splitters (mismo algoritmo que ViewportRow/Column de PC)
// ----------------------------------------------------------------------------

class W3dViewportRow : public W3dViewportBase {
    public:
        W3dViewportBase* childA;   // izquierda
        W3dViewportBase* childB;   // derecha
        float splitFrac;

        W3dViewportRow(W3dViewportBase* a, W3dViewportBase* b, float frac);
        virtual ~W3dViewportRow();

        virtual int  Kind() const;
        virtual bool isLeaf() const;
        virtual void Resize(int newW, int newH);
        virtual void Render();

        void SetSizeChildrens(int move);
};

class W3dViewportColumn : public W3dViewportBase {
    public:
        W3dViewportBase* childA;   // arriba
        W3dViewportBase* childB;   // abajo
        float splitFrac;

        W3dViewportColumn(W3dViewportBase* a, W3dViewportBase* b, float frac);
        virtual ~W3dViewportColumn();

        virtual int  Kind() const;
        virtual bool isLeaf() const;
        virtual void Resize(int newW, int newH);
        virtual void Render();

        void SetSizeChildrens(int move);
};

// ----------------------------------------------------------------------------
// Ruteo: devuelve la hoja bajo el mouse (o el splitter si el mouse esta sobre
// la division). Sin SDL: el cambio de cursor visual queda a cargo del caller.
// ----------------------------------------------------------------------------
W3dViewportBase* W3dFindViewportUnderMouse(W3dViewportBase* vp, int mx, int my);

// limites de layout configurables por plataforma (defaults razonables)
extern int W3dMinViewportWidth;
extern int W3dMinViewportHeight;
extern int W3dPaddingViewport;

#endif // W3D_VIEWPORTS_H
