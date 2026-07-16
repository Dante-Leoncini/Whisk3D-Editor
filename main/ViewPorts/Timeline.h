#ifndef TIMELINE_H
#define TIMELINE_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/WithBorder.h"           // borde (verde si activo) como Outliner/Properties
#include "ViewPorts/ScrollBar.h"            // Scrollable: LA barra de scroll del editor (la misma del Outliner)
#include "WhiskUI/Propieties/PropFloat.h"   // edicion numerica de Start/End/Frame (reusa NumEdit)
#include "WhiskUI/Button.h"                  // controles de transporte + campos = Buttons de la barra

class AnimProperty; // animation/Animation.h (una CURVA: propiedad + componente). Lo incluye el .cpp
class PopupMenu;    // WhiskUI/PopupMenu.h (idem)

// =====================================================================
//  TIMELINE (linea de tiempo) — controla el frame ACTUAL de la animacion.
// =====================================================================
//  BARRA (menu, scrollea si no entra): [tipo] + 8 botones de transporte
//  (inicio | kf ant | frame ant | play rev | play/pausa | frame sig |
//  kf sig | final) + campos Start / End editables (minimo espacio).
//  BARRA NEGRA (debajo, mismo alto que el menu): los NUMEROS de frame,
//  centrados sobre su linea; el frame ACTUAL va en un boton VERDE sobre
//  la linea verde.
//  CUERPO: bandas verticales alternando gris oscuro / mas oscuro, las
//  lineas van del piso hasta tocar la barra negra; el playhead (frame
//  actual) es una linea VERDE. Todo opera sobre los globales de
//  animacion (StartFrame/EndFrame/CurrentFrame/PlayAnimation).
class Timeline : public ViewportBase, public WithBorder, public Scrollable {
    public:
        int ViewportKind() const { return 5; } // 1=3D 2=outliner 3=properties 4=UV 5=timeline
        // el dope sheet scrollea en vertical: con esto hereda hover, agarre, arrastre y touch del ruteo generico
        Scrollable* ComoScrollable() { return this; }

        float pxPerFrame; // zoom HORIZONTAL: pixeles por frame (tiempo)
        float viewStartF; // frame en el borde IZQUIERDO (float; arranca en negativo para ver el frame 0 prolijo)

        // ===== MODO: dope sheet (rombos por fila) o CURVAS (editor grafico) =====
        // En curvas: izquierda/derecha = TIEMPO, arriba/abajo = VALOR, y el CENTRO es el CERO. El zoom tiene DOS
        // EJES independientes (pxPerFrame y pxPerUnit): sin eso una curva chata o una enorme no se ven bien.
        enum { TL_MODO_DOPE = 0, TL_MODO_CURVAS = 1 };
        int modo;
        float pxPerUnit;   // zoom VERTICAL: pixeles por unidad de valor (INDEPENDIENTE de pxPerFrame)
        float viewCenterV; // valor en el centro vertical del strip (0 = el centro es el cero)

        // campos numericos (reusan PropFloat: click -> editar por texto, entero, Enter aplica via onChange)
        PropFloat* pfStart; PropFloat* pfEnd; PropFloat* pfCur;
        float fStart, fEnd, fCur;

        // controles = Buttons de la barra del viewport (scrollean con la barra)
        Button* btnT[8];   // transporte (glyph vectorial dibujado encima)
        Button* btnStart;  // campo Start
        Button* btnEnd;    // campo End
        Button* btnAnim;   // dropdown: elegir la animacion del esqueleto (solo si hay armature con clips)
        Button* btnModo;   // switch Dope Sheet <-> Curves
        Button* btnSelect; // dope sheet: menu Select (All / None / Invert). Al FINAL de la barra; solo con filas
        Button* btnView;   // dope sheet: menu View (Frame Selected)
        Button* btnKey;    // menu Key: Transform (Move/Rotate/Scale) + Duplicate + Delete
        Button* btnPivot;  // dope sheet: menu Pivot (Center / Current Frame) = desde donde escala la 's'

        int stripY;        // Y (local) del CUERPO (bandas/lineas)
        int numY, barH2;   // barra negra de numeros (Y y alto = alto del menu)
        int curBtnX, curBtnW; // rect del boton VERDE del frame actual (para click -> editar)

        bool scrubbing;    // tocando/arrastrando en la BARRA DE NUMEROS -> mueve el frame
        bool panning;      // arrastrando en el CUERPO -> scroll (paneo) con un dedo
        int  lastMx, lastMy;
        int  pressMx, pressMy; // pos del down en el CUERPO: distingue CLICK puro (mouse: saltar el
                               // frame ahi) de un arrastre (panear). El touch solo panea/tapea.

        Timeline();
        virtual ~Timeline();

        void Render() override;
        void Resize(int newW, int newH) override;
        void event_mouse_motion(int mx, int my) override;
        void button_left() override;
        bool event_finger_scroll(int px, int py, int dx, int dy) override;             // 1 dedo: panear
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) override; // 2 dedos: zoom + paneo
#ifndef W3D_SYMBIAN
        void event_key_down(SDL_Event &e) override;
        void event_key_up(SDL_Event &e) override;   // soltar el CERO sin flechas = Frame Selected (Symbian)
        void event_mouse_wheel(SDL_Event &e) override;
        void mouse_button_up(SDL_Event &e) override;
#endif

        // click en la BARRA (transporte / campos): lo llama LayoutClickBarraTimeline. Devuelve true si consumio.
        bool ClickBarButton(int mx, int my);

        // ----- acciones de transporte (COMPARTIDAS PC/Symbian) -----
        void TransportAction(int i);    // i = 0..7 (mismo orden que btnT)
        void TogglePlay(int dir);       // dir=+1 adelante, -1 reversa; si ya juega en esa direccion -> pausa
        void GotoStart(); void GotoEnd();
        void StepFrame(int d);          // +1 / -1 frame
        void StepKeyframe(int d);       // proximo (+1) / anterior (-1) keyframe del objeto activo
        void ZoomBy(float factor, int centerXlocal); // zoom HORIZONTAL manteniendo fijo el frame bajo centerX
        void ZoomVBy(float factor);     // zoom VERTICAL (solo curvas) desde el centro del strip
        int  CentroTimeline() const;    // centro exacto del timeline (centro del STRIP: excluye el panel del dope sheet)
        int  CentroVertical() const;    // centro vertical del strip = donde vive el CERO cuando viewCenterV=0
        float ValueToY(float v) const;  // curvas: valor -> Y local
        float YToValue(float ly) const; // curvas: Y local -> valor
        void PanFrames(float dFrames);  // corre la vista N frames
        void PanValor(float dValor);    // curvas: corre la vista N unidades de valor
        float MinView() const;          // tope de paneo hacia el lado negativo (comodo)
        void SetFrameFromX(int localX); // scrub

        float FrameToX(float f) const;
        float XToFrame(float localX) const;

        void EditarCampo(int i);        // 0=Start 1=End 2=Frame actual: arranca edicion por texto
        void ApplyStart(); void ApplyEnd(); void ApplyCur(); // aplican los espejos float a los globales

        // ===== DOPE SHEET =====
        // Panel IZQUIERDO con las cosas que tienen animacion en la animacion ACTIVA, y SOLO de lo SELECCIONADO
        // (sin seleccion -> panel vacio y el timeline queda igual que sin dope sheet: panelW=0).
        // Filas tipo Outliner (desplegables, indentadas): Summary / objeto > "Object Transforms" > canal
        // (X/Y/Z Location, X/Y/Z Euler Rotation, X/Y/Z Scale). Armature: SOLO en Pose Mode y SOLO los huesos
        // seleccionados (armature > hueso > canales). El Summary marca la UNION de los keyframes de abajo.
        struct DopeRow {
            int tipo;            // 0=summary 1=objeto/armature 2=grupo(Object Transforms/hueso) 3=canal
            int nivel;           // indentacion (0,1,2)
            std::string nombre;
            std::string claveDespliegue; // clave estable para recordar plegado (vacia = no desplegable)
            std::string claveFila;       // identidad estable de la fila (seleccion / borrar)
            std::string ownerKey;        // identidad del objeto/hueso dueño (arma la clave del keyframe)
            int propId;          // canal: AnimPosition/AnimRotation/AnimScale; -1 si no es canal
            int compId;          // canal: AnimX/AnimY/AnimZ (cada componente es una CURVA propia); -1 si no es canal
            std::vector<int> keys;       // frames con keyframe de esta fila
            int icono;           // IconType o -1 (sin icono -> el texto se corre a la izquierda)
            bool oculto;         // OJO apagado: la curva no se dibuja, no se puede clickear ni la agarran las
                                 // acciones de seleccion. Lo calcula ConstruirDopeRows y BAJA a los hijos (apagar
                                 // el ojo de un hueso apaga sus canales).
            // los campos que NO son de canal arrancan en "no soy un canal": armar una fila padre y olvidarse de
            // propId/compId dejaba basura adentro
            DopeRow() : tipo(0), nivel(0), propId(-1), compId(-1), icono(-1), oculto(false) {}
        };
        std::vector<DopeRow> dopeRows;
        int panelW;      // ancho del panel (0 = sin dope sheet -> timeline clasico)
        int panelWUser;  // ancho FIJADO por el usuario arrastrando el borde (0 = automatico segun el texto)
        bool panelResize;// arrastrando el borde del panel
        int rowH;        // alto de fila
        int hoverRow;    // fila bajo el mouse (-1 = ninguna) para el mouse-over

        // rect del "Summary" FLOTANTE (panel oculto). Lo comparten el render y el click -> no se desincronizan.
        bool DopeRectFlotante(int& rx, int& ry, int& rw, int& rh) const;
        void ConstruirDopeRows();          // arma dopeRows desde la animacion activa + la seleccion
        void RenderDopePanel();            // dibuja el panel (nombres/flechas) y los rombos por fila
        bool DopeClickPanel(int mx, int my); // click en el panel: ojo / plegar/desplegar / seleccionar fila
        int  DopeOjoX() const;               // X (local) de la columna de OJOS, igual que en el Outliner
        bool DopeClickStrip(int mx, int my); // click en el strip: selecciona keyframes (shift = agrega). true si consumio
        void DopeHover(int mx, int my);      // actualiza hoverRow
        void DopeSelectAll(); void DopeSelectNone(); void DopeSelectInvert(); // a / Alt+A / Ctrl+I
        void DopeBorrarSeleccion();          // 'x': borra los keyframes seleccionados, o la animacion de las filas seleccionadas
        void DopeDuplicarSeleccion();        // Shift+D: duplica los keyframes seleccionados y los agarra para moverlos
        // Transform de keyframes. En DOPE SHEET solo existe el eje TIEMPO (rotar no significa nada y no hay ejes
        // que elegir); en CURVAS hay tiempo y VALOR, y ahi si se rota y se puede limitar a un eje.
        enum { DOPE_MOV = 1, DOPE_ESC = 2, DOPE_ROT = 3 };
        enum { DOPE_EJE_LIBRE = 0, DOPE_EJE_X = 1, DOPE_EJE_Y = 2 };
        void DopeMoveStart(int modoT);       // DOPE_MOV ('g') / DOPE_ESC ('s') / DOPE_ROT ('r', solo curvas)
        void DopeMoveApply(int mx, int my);  // arrastre -> corre/escala/rota los keyframes
        void DopeCiclarEje(int eje);         // DOPE_EJE_X / DOPE_EJE_Y: limita a un eje (de nuevo = libera)
        void DopeXformAplicar();             // re-aplica desde el snapshot (mouse o valor numerico)
        void HandleXformAplicar();           // con UN solo keyframe elegido, 'r'/'s' rotan/alargan sus HANDLES
        void DopeNumAplicar();               // aplica el valor numerico tipeado (acepta matematica)
        bool DopeNumChar(int c);             // tecla durante el transform: arma el valor numerico. true si la consumio
        void DopeMoveConfirm(); void DopeMoveCancel();
        void DestruirEstadoTimeline();       // ~Timeline: apaga el transform/handle a medio hacer
        void DopeSetPivot(int p);            // menu Pivot: 0 = Center, 1 = Current Frame
        bool DopeRangoSeleccion(int& mn, int& mx) const; // 1er/ultimo frame seleccionado. false = no hay ninguno
        void DopeFrameSelected();            // menu View: encuadra los keyframes seleccionados (1ro a la izq, ultimo a la der;
                                             // uno solo -> al medio, sin tocar el zoom). En curvas encuadra los DOS ejes.

        // ===== EDITOR DE CURVAS =====
        // Cada canal (X/Y/Z de cada propiedad) se dibuja como una curva con el color de SU EJE: X rojo, Y verde,
        // Z azul. La forma sale de AnimProperty::EvalF (el MISMO evaluador que anima) -> lo que ves es lo que corre.
        void RenderCurvas();                 // curvas + keyframes + handles del seleccionado
        AnimProperty* CurvaDeFila(const DopeRow& d) const; // la CURVA viva de una fila de canal (NULL si no es canal)
        // Arma el trazo de UNA curva (por TRAMOS, no por pixel) para dibujarlo de UNA sola llamada. Devuelve
        // cuantos vertices genero. Culling adentro: descarta lo que no se ve.
        int  CurvaTrazo(const AnimProperty* ap, float w);
        long long CurvaTrazoCosto();         // vertices de TODAS las curvas sin dibujar (mide el costo por frame)
        void DopeSelKeysDeFilasSel();        // los keyframes de las filas seleccionadas = "seleccionar esa curva"
        bool CurvaClickStrip(int mx, int my);// click en curvas: primero handles, despues keyframes. true si consumio
        void CurvaFrameSelected();           // View > Frame Selected en modo curva (encuadra tiempo Y valor)
        void SetInterpolacionSel(int interp);// Interpolation Mode -> a los keyframes seleccionados
        void SetHandleTypeSel(int tipo);     // Handle Type ('v') -> a los keyframes seleccionados
        // aperturas de menu COMPARTIDAS por el boton de la barra y el atajo de teclado (no pueden divergir)
        void AbrirMenuKey(int mx, int my);      // menu Key
        void AbrirMenuInterp(int mx, int my);   // Interpolation Mode ('t')
        void AbrirMenuHandle(int mx, int my);   // Handle Type ('v')
        void ConstruirMenuInterp(PopupMenu* m);
        void ConstruirMenuHandle(PopupMenu* m);
        // HANDLE agarrado (aparece al SELECCIONAR el keyframe; arrastrarlo curva el tramo)
        bool HandleArrastrando() const;
        bool HandleEsSalida() const;         // el handle agarrado es el de SALIDA (outTan) o el de ENTRADA (inTan)?
        // posicion (local) del handle del keyframe i. La comparten dibujo y hit-test: no se pueden desincronizar.
        void HandlePos(const AnimProperty* ap, size_t i, bool salida, float& hx, float& hy) const;
        void HandleApply(int mx, int my);
        void HandleSoltar();
        std::string DopeTextoTransform() const; // readout: "Move: 2 frames" / "Scale: [2*2] = 4"
        bool DopeMoviendo() const;           // hay un 'g' de keyframes en curso?
        int  DopeAltoContenido() const { return (int)dopeRows.size()*rowH; }

        // espejos float <- globales + texto/visibilidad de los botones de la barra. Va SIEMPRE despues de
        // ConstruirDopeRows (la barra depende de las filas); Render llama a los dos, en ese orden.
        void SyncFields();

    private:
        int  TickStep() const; // paso "redondo" de la regla segun el zoom
};

// Entrada numerica del transform de keyframes ('g'/'s' del dope sheet). Gemela de NumInputChar: cada plataforma
// le pasa los caracteres tipeados. true = los consumio (hay un transform de keyframes en curso).
bool DopeNumInputChar(int c);

// Hay un transform de keyframes (g/s/r) en curso? Lo pregunta controles.cpp para ENVOLVER el cursor dentro del
// timeline, como al panear: asi se puede seguir escalando/rotando aunque el mouse llegue al borde.
// Invalida la pose + el skin cacheado tras tocar una curva (sin esto la escena sigue mostrando lo viejo).
void InvalidarAnimYRedraw();

bool DopeXformActivo();

// ---- KEYFRAME ACTIVO (el ultimo clickeado): lo edita la tarjeta "Keyframe" del panel de propiedades ----
// Devuelve la curva viva + el indice, o NULL si no hay ninguno elegido. El indice se resuelve cada vez: el vector
// de keyframes se reordena al moverlos, guardarse un indice seria colgarse.
AnimProperty* DopeKeyframeActivo(int* idx);
std::string   DopeKeyframeActivoCanal();          // nombre del canal ("X Location", ...) para el titulo
void          DopeKeyframeActivoReFrame(int nuevoFrame); // el frame cambio: seguirlo (y mover la seleccion)
// Cancela ese transform desde afuera (lo usa el Tab, que cambia de viewport por encima del ruteo del timeline).
void DopeXformCancelar();

// Regla de pertenencia fila padre -> canal (ownerKey). La expone el test 'dopecubre'.
bool W3dScriptDopeCubre(const std::string& padre, const std::string& hijo);

#endif // TIMELINE_H
