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
#include "WhiskUI/widgets/Button.h"                  // controles de transporte + campos = Buttons de la barra
#include "ViewPorts/Gizmo.h"                // Gizmo2DClient: el gizmo 2D sobre los keyframes elegidos
#include <string>

class AnimProperty; // animation/Animation.h (una CURVA: propiedad + componente). Lo incluye el .cpp
class PopupMenu;    // WhiskUI/PopupMenu.h (idem)
class Object;       // objects/Objects.h (DopeIdOwner)
class Sound;        // objects/Sound.h (las tiras de sonido)

// ============================================================================
//  IDENTIDAD DEL DUENO adentro de las claves del dope: "#<serial>", NO el nombre.
//
//  Las claves del dope llevaban al dueno POR NOMBRE ("arm:RigA/k0/b0"). El nombre es
//  unico EN UN MOMENTO DADO pero se LIBERA al renombrar (y al borrar), y el sistema de
//  nombres unicos se lo da al siguiente que lo pida: renombrar RigA -> "RigViejo" y
//  despues RigB -> "RigA" hace que la clave vieja matchee al rig AJENO y la tarjeta
//  "Keyframe" edite su curva. Un NOMBRE RECICLADO es el equivalente exacto del PUNTERO
//  RECICLADO de DeleteUndo (delete + new = misma direccion; rename + rename = mismo
//  nombre), y por eso el nombre NO sirve como identidad de estado efimero.
//  Object::serial (Core, objects/Objects.h) es un contador monotono que se asigna al
//  nacer y no se reusa nunca: esa es la identidad que va adentro de la clave.
//
//  CRITERIO (cual va por serial y cual sigue por NOMBRE):
//   - ESTADO EFIMERO que apunta a UNA entidad concreta y NO se guarda en el archivo
//     (la seleccion del dope, el plegado, el ojo, el keyframe activo) -> POR SERIAL.
//     Nadie lo re-liga a mano: si el nombre cambia de dueno, el estado tiene que
//     quedar INERTE, no mudarse de objeto.
//   - LIGADURAS DEL MODELO que el usuario re-liga renombrando y que SE GUARDAN
//     (hueso <-> vertex group, modArmature/target, la fuente de los constraints,
//     RailName, refs de lua, paleta) -> SIGUEN POR NOMBRE, a proposito. Ahi el nombre
//     ES el vinculo: renombrar el vertex group para que lo tome otro hueso es una
//     operacion VALIDA del usuario, y ademas el serial no sobrevive a guardar/cargar.
//  Regla corta: si se serializa o el usuario lo re-liga a mano, va por NOMBRE; si es
//  estado de sesion que apunta a "ese objeto de ahi", va por SERIAL.
// ============================================================================
std::string DopeIdOwner(const Object* o);

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
class Timeline : public ViewportBase, public WithBorder, public Scrollable, public Gizmo2DClient {
    public:
        int ViewportKind() const { return 5; } // 1=3D 2=outliner 3=properties 4=UV 5=timeline
        // el dope sheet scrollea en vertical: con esto hereda hover, agarre, arrastre y touch del ruteo generico
        Scrollable* AsScrollable() { return this; }

        float pxPerFrame; // zoom HORIZONTAL: pixeles por frame (tiempo)
        float viewStartF; // frame en el borde IZQUIERDO (float; arranca en negativo para ver el frame 0 prolijo)

        // ===== MODO: dope sheet (rombos por fila) o CURVAS (editor grafico) =====
        // En curvas: izquierda/derecha = TIEMPO, arriba/abajo = VALOR, y el CENTRO es el CERO. El zoom tiene DOS
        // EJES independientes (pxPerFrame y pxPerUnit): sin eso una curva chata o una enorme no se ven bien.
        enum { TL_MODE_DOPE = 0, TL_MODE_CURVES = 1 };
        int mode;
        float pxPerUnit;   // zoom VERTICAL: pixeles por unidad de valor (INDEPENDIENTE de pxPerFrame)
        float viewCenterV; // valor en el centro vertical del strip (0 = el centro es el cero)

        // campos numericos (reusan PropFloat: click -> editar por texto, entero, Enter aplica via onChange)
        PropFloat* pfStart; PropFloat* pfEnd; PropFloat* pfCur; PropFloat* pfFps;
        float fStart, fEnd, fCur, fFpsTL;

        // controles = Buttons de la barra del viewport (scrollean con la barra)
        Button* btnT[8];   // transporte (glyph vectorial dibujado encima)
        Button* btnStart;  // campo Start
        Button* btnEnd;    // campo End
        Button* btnFps;    // campo FPS (mas comodo aca que en el panel)
        Button* btnAnim;   // dropdown: elegir la animacion del esqueleto (solo si hay armature con clips)
        Button* btnAutoKey;// AUTO KEY (toggle): rojo = graba los canales que toques al transformar
        Button* btnMode;   // switch Dope Sheet <-> Curves (ya no va en la barra: es una opcion del menu View)
        Button* btnTime; // el RELOJ: Inicio / Fin / FPS en un desplegable (TimePopup)
        Button* btnSelect; // dope sheet: menu Select (All / None / Invert). Al FINAL de la barra; solo con filas
        Button* btnView;   // dope sheet: menu View (Frame Selected)
        Button* btnKey;    // menu Key: Transform (Move/Rotate/Scale) + Duplicate + Delete
        Button* btnSelMode;   // el MOUSE: que elige el click / el dedo / el lapiz (ambos, el tramo de curva, el keyframe)
        Button* btnPivot;  // dope sheet: menu Pivot (Center / Current Frame) = desde donde escala la 's'

        int stripY;        // Y (local) del CUERPO (bandas/lineas)
        int numY, barH2;   // barra negra de numeros (Y y alto = alto del menu)
        int curBtnX, curBtnW; // rect del boton VERDE del frame actual (para click -> editar)

        bool scrubbing;    // tocando/arrastrando en la BARRA DE NUMEROS -> mueve el frame
        int  lastMx, lastMy;

        Timeline();
        virtual ~Timeline();

        void Render() W3D_OVERRIDE;
        void Resize(int newW, int newH) W3D_OVERRIDE;
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE;
        void button_left() W3D_OVERRIDE;
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE;             // 1 dedo: panear
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) W3D_OVERRIDE; // 2 dedos: zoom + paneo
        bool event_finger_pinch(float cx, float cy, float panDx, float panDy,              // ...con los dedos: zoom anclado
                                float sepX0, float sepY0, float sepX1, float sepY1) W3D_OVERRIDE; // y por eje (curvas)
        void event_key_down(int keyCode, bool repeat) W3D_OVERRIDE;
        void event_key_up(int key) W3D_OVERRIDE;   // soltar el CERO sin flechas = Frame Selected (Symbian)
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
        void mouse_button_up(int button) W3D_OVERRIDE;
        // TOOLBAR de abajo (la compartida, ToolbarBase.cpp): con keyframes elegidos, un click cambia la
        // interpolacion | el tipo de handle | el ease del tramo (grupos separados por una linea vertical)
        bool ToolbarVisible() const W3D_OVERRIDE;
        void ToolbarSync() W3D_OVERRIDE;
        void ToolbarActionRole(int role) W3D_OVERRIDE;

        // click en la BARRA (transporte / campos): lo llama LayoutClickBarraTimeline. Devuelve true si consumio.
        bool ClickBarButton(int mx, int my);

        // ----- acciones de transporte (COMPARTIDAS PC/Symbian) -----
        void TransportAction(int i);    // i = 0..7 (mismo orden que btnT)
        void TogglePlay(int dir);       // dir=+1 adelante, -1 reversa; si ya juega en esa direccion -> pausa
        void GotoStart(); void GotoEnd();
        void StepFrame(int d);         // +1 / -1 frame
        void ScrubArrow(int dir);      // flecha MANTENIDA: corre el frame ~3px de pantalla por tick, con cualquier zoom
                                        // (el paso segun el zoom lo calcula adentro; ver el cuerpo en Timeline.cpp)
        void StepKeyframe(int d);       // proximo (+1) / anterior (-1) keyframe del objeto activo
        void ZoomBy(float factor, int centerXlocal); // zoom HORIZONTAL manteniendo fijo el frame bajo centerX
        void ZoomVBy(float factor);     // zoom VERTICAL (solo curvas) desde el centro del strip
        void ZoomVIn(float factor, float lyLocal); // zoom VERTICAL manteniendo fijo el valor bajo lyLocal (los dedos)
        // ===== GIZMO 2D (Gizmo.h) sobre los keyframes elegidos: en CURVAS mueve/rota/escala en tiempo y valor; en el
        // DOPE SHEET mueve/escala en el tiempo. Arranca el MISMO transform de G/R/S (DopeMoveStart) =====
        bool Gizmo2DPivot(float& lx, float& ly) W3D_OVERRIDE;
        int  Gizmo2DModes() const W3D_OVERRIDE;
        bool Gizmo2DWithAxisY() const W3D_OVERRIDE { return mode == TL_MODE_CURVES; }
        bool Gizmo2DInZone(float lx, float ly) const W3D_OVERRIDE;
        bool Gizmo2DStart(int modeG, int axis) W3D_OVERRIDE;
        // ===== el DEDO en el timeline (controls.cpp decide, pasada la espera anti-gesto) =====
        bool PanelIn(int mx, int my) const;            // (mx,my) cae en el PANEL de canales (la lista de la izquierda)
        void PanelScrollBy(int dx, int dy);            // scrollea ese panel: filas (vertical) y texto (horizontal)
        // modo VISTA (el monitor de la toolbar): arrastrar desde ahi MUEVE LA VISTA. No: la banda de los numeros y la
        // linea del frame actual (scrub), el gizmo, la toolbar y el panel
        bool DragMovesView(int mx, int my);
        void Gizmo2DApply(float dx, float dy, float ang, float f) W3D_OVERRIDE;
        void Gizmo2DConfirm() W3D_OVERRIDE;
        void Gizmo2DCancel() W3D_OVERRIDE;
        // ===== LA VISTA DEL TIMELINE VIAJA CON EL PROYECTO =====
        // Reporte del dueno: "falta guardar el zoom y posicion del visor en el timeline en dope sheet y curvas".
        // Los CUATRO campos de arriba (pxPerFrame/viewStartF + pxPerUnit/viewCenterV) se escriben como props de
        // la hoja "Timeline" del bloque "layout" del .w3d, al lado del 'modo' que ya viajaba ahi y por la misma
        // razon: la vista es de ESE timeline (puede haber mas de uno abierto) y colgarla de la hoja no obliga a
        // inventarle un identificador al viewport. Ver SaveW3D.cpp (escritura) y ApplyTimelineProps
        // (import_w3d.cpp, lectura).
        // ViewApply es el UNICO camino de entrada desde un archivo: valida (nan/inf/zoom<=0/absurdos) y acota
        // con los mismos topes que la rueda. Un .w3d sin las props abre con el encuadre de siempre.
        void ViewApply(float zoomF, float frameLeft, float zoomV, float centerV);
        int  CenterTimeline() const;    // centro exacto del timeline (centro del STRIP: excluye el panel del dope sheet)
        int  CenterVertical() const;    // centro vertical del strip = donde vive el CERO cuando viewCenterV=0
        float ValueToY(float v) const;  // curvas: valor -> Y local
        float YToValue(float ly) const; // curvas: Y local -> valor
        void PanFrames(float dFrames);  // corre la vista N frames
        void PanValue(float dValue);    // curvas: corre la vista N unidades de valor
        float MinView() const;          // tope de paneo hacia el lado negativo (comodo)
        void SetFrameFromX(int localX); // scrub

        float FrameToX(float f) const;
        float XToFrame(float localX) const;

        void EditField(int i);        // 0=Start 1=End 2=Frame 3=FPS: arranca edicion por texto
        void ApplyFps();
        void ApplyStart(); void ApplyEnd(); void ApplyCur(); // aplican los espejos float a los globales

        // ===== DOPE SHEET =====
        // Panel IZQUIERDO con las cosas que tienen animacion en la animacion ACTIVA, y SOLO de lo SELECCIONADO
        // (sin seleccion -> panel vacio y el timeline queda igual que sin dope sheet: panelW=0).
        // Filas tipo Outliner (desplegables, indentadas): Summary / objeto > "Object Transforms" > canal
        // (X/Y/Z Location, X/Y/Z Euler Rotation, X/Y/Z Scale). Armature: SOLO en Pose Mode y SOLO los huesos
        // seleccionados (armature > hueso > canales). El Summary marca la UNION de los keyframes de abajo.
        struct DopeRow {
            int type;            // 0=summary 1=objeto/armature 2=group(Object Transforms/hueso) 3=canal
            int level;           // indentacion (0,1,2)
            std::string name;
            std::string keyExpansion; // clave estable para recordar plegado (vacia = no desplegable)
            std::string keyRow;       // identidad estable de la fila (seleccion / borrar)
            std::string ownerKey;     // identidad del objeto/hueso dueño (arma la clave del keyframe)
            int propId;          // canal: AnimPosition/AnimRotation/AnimScale; -1 si no es canal
            int compId;          // canal: AnimX/AnimY/AnimZ (cada componente es una CURVA propia); -1 si no es canal
            std::vector<int> keys;       // frames con keyframe de esta fila
            int icon;            // IconType o -1 (sin icono -> el texto se corre a la izquierda)
            bool hidden;         // OJO apagado: la curva no se dibuja, no se puede clickear ni la agarran las
                                 // acciones de seleccion. Lo calcula BuildDopeRows y BAJA a los hijos (apagar
                                 // el ojo de un hueso apaga sus canales).
            Object* sound;      // la fila de un objeto SONIDO: en el strip va su TIRA (se mueve y se recorta)
            int stripRows;      // la tira ocupa esta fila y 'stripRows' mas (con la forma de onda o el volumen a la
                                 // vista: mas alta); -1 = es una de esas filas de CONTINUACION (sin nombre)
            bool ghost;       // un CANAL que NO SE LISTA: no tiene renglon (ni se dibuja ni se toca), pero esta para
                                 // que lo alcance lo de siempre (elegir por la fila padre, la caja, A, mover, borrar,
                                 // escalar, interpolar, Ctrl+Z). Los huesos de un armature en Object Mode: se ve UNA
                                 // fila con sus keyframes y se mueven todos juntos. Van AL FINAL de dopeRows
                                 // (desde dopeVisible): asi la fila r sigue estando en la altura r
            // los campos que NO son de canal arrancan en "no soy un canal": armar una fila padre y olvidarse de
            // propId/compId dejaba basura adentro
            DopeRow() : type(0), level(0), propId(-1), compId(-1), icon(-1), hidden(false), sound(NULL), stripRows(0),
                        ghost(false) {}
        };
        std::vector<DopeRow> dopeRows;
        size_t dopeVisible; // las filas que SE VEN (las primeras de dopeRows); de ahi en adelante, las fantasma
        int panelW;      // ancho del panel (0 = sin dope sheet -> timeline clasico)
        int panelWUser;  // ancho FIJADO por el usuario arrastrando el borde (0 = automatico segun el texto)
        bool panelResize;// arrastrando el borde del panel
        bool panelBorderHover; // el mouse sobre el borde (se pinta blanco; arrastrando, verde)
        int  panelPosX;  // scroll HORIZONTAL del texto del panel (<= 0)
        int  panelContW; // ancho del texto mas largo (flecha + icono + nombre), sin la columna de ojos
        bool panelBarH;// arrastrando la barra horizontal del panel
        int  panelPan; // boton del medio: 0 = sin decidir, 1 = scrollea el PANEL, 2 = panea el strip
        int  PanelMaxX() const;            // tope del scroll horizontal del panel (<= 0)
        bool PanelBarH(int& bx, int& by, int& bw, int& bh, int& tx, int& tw) const; // barra horizontal del panel
        bool PanelInBorder(int lx, int ly) const; // (lx, ly) sobre el borde que cambia el ancho del panel
        // EL PANEL CON EL TECLADO (el N95): la fila enfocada (-1 = el foco no esta en el panel). Desde la barra de arriba
        // (softkey izquierdo), arriba/abajo entran; las flechas recorren las filas (y la eligen: sus keyframes quedan
        // elegidos, asi el lapiz recorre los de ESA curva), izq/der pliegan/despliegan, el 2 la oculta / muestra, la C
        // borra lo elegido y el 0 lo encuadra
        int panelFocus;
        bool PanelFocusEnter(int dir = +1);  // desde la barra: abajo = la primera fila (Summary), arriba = la ultima. false = no hay filas
        bool PanelFocusMove(int dir);      // false = se paso de la primera o de la ultima (vuelve a la barra)
        void PanelFocusCollapse(bool collapse);
        void PanelFocusEye();
        int rowH;        // alto de fila
        int hoverRow;    // fila bajo el mouse (-1 = ninguna) para el mouse-over

        // rect del "Summary" FLOTANTE (panel oculto). Lo comparten el render y el click -> no se desincronizan.
        bool DopeRectFloating(int& rx, int& ry, int& rw, int& rh) const;
        void BuildDopeRows();            // arma dopeRows desde la animacion activa + la seleccion
        void RenderDopePanel();          // dibuja el panel (nombres/flechas) y los rombos por fila
        bool DopeClickPanel(int mx, int my); // click en el panel: ojo / plegar/desplegar / seleccionar fila
        int  DopeEyeX() const;               // X (local) de la columna de OJOS, igual que en el Outliner
        bool DopeClickStrip(int mx, int my); // click en el strip: selecciona keyframes (shift = agrega). true si consumio
        void DopeHover(int mx, int my);      // actualiza hoverRow
        void DopeSelectAll(); void DopeSelectNone(); void DopeSelectInvert(); // a / Alt+A / Ctrl+I
        // ----- navegacion de keyframes por TECLADO (Symbian: lapiz + flechas; sin mouse) -----
        // Recorre los frames del SUMMARY (uno por frame con algo): recorrer los de cada canal seria infinito.
        // Si hay FILAS de curva seleccionadas, recorre los de ESAS (para eso esta el modo curva).
        void DopeRowsNav(std::vector<int>& out) const;    // que filas se recorren
        void DopeFramesNav(std::vector<int>& out) const;  // sus frames, ordenados
        void DopeSelFrame(int f, bool on);                // elige/saca todos los keyframes de ese frame
        bool DopeSelAdvance(int step, bool extend);       // lapiz+der/izq: avanza el activo
        bool DopeSelAllNav();                             // lapiz+arriba: todos
        bool DopeSelNothingNav();                         // lapiz+abajo: ninguno
        void DopeDeleteSelection();           // 'x': borra los keyframes seleccionados, o la animacion de las filas seleccionadas
        void DopeDuplicateSelection();        // Shift+D: duplica los keyframes seleccionados y los agarra para moverlos
        bool DopeCopySelection();             // Ctrl+C: los keyframes elegidos al portapapeles (false = no habia)
        void DopeCutSelection();              // Ctrl+X: copiar + borrar
        void DopePaste();                     // Ctrl+V: los del portapapeles desde el frame ACTUAL (quedan elegidos)
        bool MenuContext(int mx, int my, int menuX = -1, int menuY = -1);
        bool MenuContextKeyboard();   // el mismo, en el playhead (el OK mantenido del N95: los keyframes elegidos)   // click derecho en el strip: Cortar / Copiar / Pegar / Duplicar / Borrar (menuX/Y: donde abrirlo)
        bool PlayheadIn(int mx, int my) const;  // (pantalla) sobre la LINEA del frame actual (se arrastra)
        bool CurveSegmentIn(int mx, int my, bool add); // click sobre la linea de una curva: elige su tramo A-B
        void BoxApply(int x0, int y0, int x1, int y1, bool touch, bool add); // box select (pantalla)
        void SetEasingSel(int ease);        // Ctrl+E: el easing (KfEaseAuto/In/Out/InOut) de las curvas de easing elegidas
        void ChangeMode(bool curves);       // Dope Sheet <-> Curves (menu View)
        void OpenMenuEasing(int mx, int my);
        // Transform de keyframes. En DOPE SHEET solo existe el eje TIEMPO (rotar no significa nada y no hay ejes
        // que elegir); en CURVAS hay tiempo y VALOR, y ahi si se rota y se puede limitar a un eje.
        enum { DOPE_MOV = 1, DOPE_ESC = 2, DOPE_ROT = 3 };
        enum { DOPE_AXIS_FREE = 0, DOPE_AXIS_X = 1, DOPE_AXIS_Y = 2 };
        void DopeMoveStart(int modeT);       // DOPE_MOV ('g') / DOPE_ESC ('s') / DOPE_ROT ('r', solo curvas)
        void DopeMoveApply(int mx, int my);  // arrastre -> corre/escala/rota los keyframes
        void DopeCycleAxis(int axis);        // DOPE_AXIS_X / DOPE_AXIS_Y: limita a un eje (de nuevo = libera)
        void DopeXformApply();               // re-aplica desde el snapshot (mouse o valor numerico)
        void HandleXformApply();             // con UN solo keyframe elegido, 'r'/'s' rotan/alargan sus HANDLES
        void DopeNumApply();                 // aplica el valor numerico tipeado (acepta matematica)
        bool DopeNumChar(int c);             // tecla durante el transform: arma el valor numerico. true si la consumio
        void DopeMoveConfirm(); void DopeMoveCancel();
        void DestroyTimelineState();       // ~Timeline: apaga el transform/handle a medio hacer
        void DopeSetPivot(int p);          // menu Pivot: 0 = Center, 1 = Current Frame, 2 = Median Point, 3 = Active Keyframe
        bool DopeSelectionRange(int& mn, int& mx) const; // 1er/ultimo frame seleccionado. false = no hay ninguno
        void DopeFrameSelected();            // menu View: encuadra los keyframes seleccionados (1ro a la izq, ultimo a la der;
                                             // uno solo -> al medio, sin tocar el zoom). En curvas encuadra los DOS ejes.

        // ===== EDITOR DE CURVAS =====
        // Cada canal (X/Y/Z de cada propiedad) se dibuja como una curva con el color de SU EJE: X rojo, Y verde,
        // Z azul. La forma sale de AnimProperty::EvalF (el MISMO evaluador que anima) -> lo que ves es lo que corre.
        void RenderCurves();                 // curvas + keyframes + handles del seleccionado
        // la CURVA viva de una fila de canal (NULL si no es canal). 'editar' = el que llama la va a ESCRIBIR:
        // un clip de esqueleto de un animset compartido pasa antes a clips propios (ver DopeActiveKeyframe)
        AnimProperty* RowCurve(const DopeRow& d, bool edit = true) const;
        // Arma el trazo de UNA curva (por TRAMOS, no por pixel) para dibujarlo de UNA sola llamada. Devuelve
        // cuantos vertices genero. Culling adentro: descarta lo que no se ve.
        int  CurveStroke(const AnimProperty* ap, float w);
        long long CurveStrokeCost();         // vertices de TODAS las curvas sin dibujar (mide el costo por frame)
        void DopeSelRowSelKeys();        // los keyframes de las filas seleccionadas = "seleccionar esa curva"
        bool CurveClickStrip(int mx, int my);// click en curvas: primero handles, despues keyframes. true si consumio
        bool HandleUnder(int mx, int my, bool grab); // hay un handle a la vista en el punto (agarrar: lo toma)
        void CurveFrameSelected();           // View > Frame Selected en modo curva (encuadra tiempo Y valor)
        // RANGO que ocupa lo que se ve en el editor de curvas, en DATOS (frames y valores). Es el nucleo del
        // encuadre, separado del zoom para poder testearlo sin ventana. onlySel=true mide SOLO los keyframes
        // SELECCIONADOS (numpad "." con seleccion); false mide las curvas VISIBLES enteras (auto-encuadre al
        // entrar al modo curvas, y numpad "." sin nada elegido). Incluye las filas de VERTEX ANIM
        // (Vertices/UV/Normales), que no son AnimProperty y por eso quedaban afuera del encuadre. El rango de
        // VALOR se mide sobre la curva MUESTREADA (una bezier se pasa de sus keyframes), y ademas entran los
        // HANDLES que se DIBUJAN (keyframe seleccionado + tramo bezier de ese lado), ubicados con
        // AnimProperty::EffectiveHandle -- el mismo del dibujo y la evaluacion. Un handle enorme no puede
        // aplastar la curva: su aporte esta topeado (ver el cuerpo). false = nada que encuadrar.
        bool CurveViewRange(bool onlySel, float& fmn, float& fmx, float& vmn, float& vmx) const;
        // aplica un rango al zoom/paneo de los dos ejes (rango degenerado -> margen minimo, ver el .cpp)
        void CurveFrame(float fmn, float fmx, float vmn, float vmx);
        void SetInterpolationSel(int interp);// Interpolation Mode -> a los keyframes seleccionados
        void SetHandleTypeSel(int type);     // Handle Type ('v') -> a los keyframes seleccionados
        // Smart Euler: saca las vueltas de mas de los keyframes de ROTACION seleccionados (405 -> 45). La pose no
        // cambia (un angulo se repite cada 360); lo que cambia es que la curva deja de dar la vuelta al pedo.
        // Devuelve cuantos keyframes corrigio.
        int  SmartEulerSel();
        // aperturas de menu COMPARTIDAS por el boton de la barra y el atajo de teclado (no pueden divergir)
        // el ruteo compartido llama a esta al PASAR el mouse (sin click) -> deslizarse de un menu de la barra a
        // otro, igual que en el viewport 3D. Por eso los menus van SEPARADOS de las acciones de la barra.
        bool OpenBarMenu(int mx, int my) W3D_OVERRIDE;
        void OpenMenuSelect(int mx, int my);   // menu Select (All / None / Invert)
        void OpenMenuPivot(int mx, int my);    // menu Pivot (Center / Current Frame)
        void OpenMenuSelMode(int mx, int my);  // menu del MOUSE: Ambos / Curva (el tramo) / Keyframe
        // las TIRAS DE SONIDO con el teclado (el lapiz elige punta izquierda / entera / derecha) y la TIJERA
        int  StripRowOf(const Sound* s) const;
        bool StripHas() const;
        bool DopeNoKeysNav() const;
        bool StripNavPrefers() const;
        bool StripSelAdvance(int step);
        Sound* PickedStrip() const;     // el sonido elegido (con el lapiz, o el activo)
        Sound* StripToCut() const;
        bool StripCut();
        void DopeSegmentPick(int row, int i, bool add);   // el TRAMO i de la fila (sus dos keyframes)
        bool DopeSegmentAdvance(int step, bool extend);   // el lapiz en modo Curva: tramo por tramo
        void OpenMenuAnim(int mx, int my);     // dropdown de animacion
        void OpenMenuView(int mx, int my);     // menu View (Frame Selected + Auto frame)
        void OpenMenuKey(int mx, int my);      // menu Key
        void OpenMenuInterp(int mx, int my);   // Interpolation Mode ('t')
        void OpenMenuHandle(int mx, int my);   // Handle Type ('v')
        void BuildMenuInterp(PopupMenu* m);
        void BuildMenuHandle(PopupMenu* m);
        // HANDLE agarrado (aparece al SELECCIONAR el keyframe; arrastrarlo curva el tramo)
        bool HandleDragging() const;
        bool HandleIsOutput() const;         // el handle agarrado es el de SALIDA (outTan) o el de ENTRADA (inTan)?
        // posicion (local) del handle del keyframe i. La comparten dibujo y hit-test: no se pueden desincronizar.
        void HandlePos(const AnimProperty* ap, size_t i, bool output, float& hx, float& hy) const;
        void HandleApply(int mx, int my);
        void HandleRelease();
        std::string DopeTextTransform() const; // readout: "Move: 2 frames" / "Scale: [2*2] = 4"
        bool DopeMoving() const;           // hay un 'g' de keyframes en curso?
        // ===== LAS TIRAS DE SONIDO (objects/Sound.h) =====
        // Un objeto Sonido seleccionado tiene su fila con una TIRA: donde arranca y cuanto dura, en SEGUNDOS de la
        // escena (no se engancha a los frames). Arrastrarla la corre, agarrarla de una punta la recorta; 'g' corre
        // todas las seleccionadas. Con View > Waveform se ve su forma de onda y con View > Volume line, la curva de
        // su volumen (animado o no). Sin eso es solo la tira.
        void DrawStrip(const DopeRow& d, int ry0);
        bool StripClick(int mx, int my);      // true = agarro una tira (o la punta de una)
        void StripHover(int mx, int my);      // la punta bajo el mouse (se pinta blanca; arrastrandola, verde)
        // que hay en (mx,my): la fila de un sonido y si es su punta izquierda (2), la derecha (3) o el cuerpo (1).
        // 0 = nada. La comparten el click y el hover: no se pueden desincronizar
        int  StripIn(int mx, int my, Sound** s);
        bool StripGrabStart();                // 'g' sin keyframes elegidos: corre los sonidos seleccionados
        void StripApply(int mx);
        void StripConfirm(); void StripCancel();
        bool StripActive() const;
        void StripRelease();                   // soltar el boton: el arrastre termina
        int  DopeContentHeight() const { return (int)dopeVisible*rowH; }   // (las fantasma no ocupan lugar)

        // espejos float <- globales + texto/visibilidad de los botones de la barra. Va SIEMPRE despues de
        // BuildDopeRows (la barra depende de las filas); Render llama a los dos, en ese orden.
        void SyncFields();

    private:
        int  TickStep() const; // paso "redondo" de la regla segun el zoom
};

// Entrada numerica del transform de keyframes ('g'/'s' del dope sheet). Gemela de NumInputChar: cada plataforma
// le pasa los caracteres tipeados. true = los consumio (hay un transform de keyframes en curso).
bool DopeNumInputChar(int c);

// LAS INTERPOLACIONES en un menu: Constant / Linear / Bezier | las de easing por fuerza | las de efecto, separadas
// con una linea, cada una con su icono. id de cada fila = idBase + KfConstant...KfElastic. Lo comparten el timeline
// (E / T y el menu Key) y la tarjeta Keyframe de Properties. 'actual' (-1 = ninguna) va en verde.
class PopupMenu;
void W3dMenuInterpolations(PopupMenu* m, int idBase, int current);
// el EASING (Ctrl+E): Automatic / Ease In / Ease Out / Ease In and Out. id = idBase + KfEaseAuto...KfEaseInOut
void W3dMenuEasings(PopupMenu* m, int idBase, int current);
const char* W3dInterpolationName(int interp);   // "Bounce", "Bezier"... (traducido)
const char* W3dNameEasing(int ease);

// Invalida la pose + el skin cacheado tras tocar una curva (sin esto la escena sigue mostrando lo viejo).
void InvalidateAnimAndRedraw();

// Hay un transform de keyframes (g/s/r) en curso? Lo pregunta controls.cpp para ENVOLVER el cursor dentro del
// timeline, como al panear: asi se puede seguir escalando/rotando aunque el mouse llegue al borde.
bool DopeXformActive();
// el DEDO arrastra el PANEL de canales de ESE timeline (event_finger_scroll lo scrollea en vez de mover la vista).
// NULL = ninguno. Lo fija controls.cpp al decidir el gesto; vale hasta el proximo toque (sigue con la inercia)
void TimelineFingerPanel(Timeline* t);
// la TOOLBAR del timeline con el DEDO: el monitor (true, de fabrica) = arrastrar mueve la vista; la flecha de
// seleccion (false) = arrastrar arma la caja de seleccion y agarra keyframes, como con el mouse
extern bool g_tlViewMode;
bool TL_TogglePlay();   // barra de espacio: play/pausa sin tener el timeline enfocado. false = no hay timeline.
// Lapiz (+flechas) de Symbian sobre el TIMELINE: navega KEYFRAMES en vez de cambiar la seleccion de objetos.
// dir: 0=lapiz solo (saca el actual y avanza) 1=derecha 2=izquierda 3=up(todos) 4=down(ninguno).
// false = el timeline no esta activo -> que el llamador siga con lo suyo.
bool DopeNavKey(int dir);

// ============================================================================
//  LA SELECCION DEL DOPE GUARDA INDICES (misma clase de bug que el undo, ver Undo.h)
//  Los ownerKey son strings en un set: sobreviven a TODO. Llevan DOS cosas adentro, y las dos son
//  indices que se corren:
//    (1) QUE CURVA es: el indice del hueso / el nombre del objeto;
//    (2) DE QUE LISTA es: el clip 3D, la escena o el (armature 2D + clip 2D)  <-- ronda 7.
//  FORMA COMPLETA de las claves (la arma DopePrefScene/DopePrefArm/DopePrefArm2D, Timeline.cpp):
//    escena    "obj:e<ESCENA>/#<SERIAL objeto>"          (+ "/xf")
//    clip 3D   "arm:#<SERIAL rig>/k<CLIP>/b<HUESO>"
//    clip 2D   "arm2d:#<SERIAL malla>/a<ARM2D>/k<CLIP>/b<HUESO>"
//  El DUENO va por SERIAL y no por nombre (ver DopeIdOwner arriba: el nombre se recicla).
//  EL DUENO VA PRIMERO Y EL NUMERO DESPUES, siempre: el prefijo de un aviso de remapeo termina
//  JUSTO ANTES del numero (DopeKeySplit exige el digito pegado), asi que un numero adelante
//  obliga a un prefijo sin dueno y el remapeo pisa las claves de TODOS los duenos. Le pasaba al
//  clip 3D ("arm:k<CLIP>/<rig>/...", ronda 7): borrar un clip de UN rig corria los indices de
//  clip de las claves de los OTROS, y cuando el usuario volvia a su rig la clave ya corrida
//  resolvia al CLIP EQUIVOCADO de su propio rig (test 'dopeclip3d').
//  Sin (2) el resolver ataba la clave a lo que estuviera ACTIVO al usarla, asi que cambiar de clip
//  o de escena hacia que la tarjeta "Keyframe" y la 'X' del dope editaran las curvas de una lista
//  que el usuario nunca eligio (y encima se dibujaban como seleccionadas). Sin (1), la clave vieja
//  matchea la fila NUEVA de otro hueso. Los dos casos son silenciosos.
//  Por eso TODA operacion que corra esos indices tiene que avisar por aca: BoneEditDelete y
//  UVEditor::Bone2DDelete (huesos, POR CADA CLIP), UndoArm2DDelete (armatures 2D), UndoDeleteClipArm
//  / UndoMoveClipArm / ClipOrderUndo (clips 3D), UndoDeleteClip2D (clips 2D) y UndoDeleteActiveScene
//  (escenas).
//   'prefijo' = lo que va ANTES del numero: "arm:#<s>/k<c>/b", "arm2d:#<s>/a<n>/k<c>/b",
//               "arm2d:#<s>/a" (indice del armature 2D), "arm2d:#<s>/a<n>/k" (clip 2D),
//               "arm:#<s>/k" (clip 3D) u "obj:e" (escena). El "#<s>" sale de DopeIdOwner.
//   TODOS llevan a su DUENO adentro menos "obj:e", y ese no lo necesita: las animaciones de
//   ESCENA son UNA lista global del proyecto (SceneAnimations), no hay otro dueno que acotar.
//   convencion IGUAL a la del undo: b >= 0 = intercambio a<->b; b < 0 = se BORRO el indice 'a'.
// ============================================================================
void DopeRemapIndexKey(const std::string& prefix, int a, int b);
// y la vuelta: se INSERTO un elemento en la posicion 'a' (el undo de un borrado que devuelve el
// armature 2D a su lugar) -> los indices guardados que son >= a suben uno.
void DopeInsertIndexKey(const std::string& prefix, int a);
// La otra mitad: las filas "objvtx:/objuv:/objnrm:/objanim:<malla>" NO dicen de QUE vertex anim
// son (se resuelven por la ACTIVA al usarlas) -> al cambiar de vertex anim, esa seleccion se
// SUELTA. Van LAS CUATRO: "objanim:" (las curvas de TRANSFORM propias de la anim, que es ademas
// la fila PADRE de las otras tres) quedaba afuera y su tarjeta "Keyframe" seguia editando la
// curva homonima de la anim nueva. Llamarla desde TODO camino que cambie la anim activa.
// La anim activa es el PAR (ActiveAnimKind + ActiveAnimMesh) mas el currentAnim de esa malla,
// asi que son CUATRO caminos, no dos: AnimSelVertex y DeleteVertexAnimOf (Properties.cpp)
// cambian la anim DENTRO de la malla, y DeleteUndo cambia la MALLA por los dos lados -Detach()
// la pone en NULL al borrar lo que la contiene y Apply() la RESTAURA al deshacer- (Undo.cpp,
// ReleaseDopeIfChangeAnimVertex). Como red de abajo, el resolver ademas valida que la clave sea
// de la malla ACTIVA (DopeOwnerIsAnimMesh, Timeline.cpp): entre anims de mallas DISTINTAS la
// clave ajena queda inerte aunque alguien se olvide de soltar. Entre dos anims de LA MISMA
// malla la clave es identica y el unico que las separa es este soltado.
void DopeReleaseVertexAnim();

// ---- KEYFRAME ACTIVO (el ultimo clickeado): lo edita la tarjeta "Keyframe" del panel de propiedades ----
// Devuelve la curva viva + el indice, o NULL si no hay ninguno elegido. El indice se resuelve cada vez: el vector
// de keyframes se reordena al moverlos, guardarse un indice seria colgarse.
// 'editar' = la tarjeta va a ESCRIBIR el keyframe (sus acciones): un clip de esqueleto de un animset
// COMPARTIDO pasa antes a clips propios. El refresco de la tarjeta (solo muestra) pasa false.
AnimProperty* DopeActiveKeyframe(int* idx, bool edit = true);
std::string   DopeActiveKeyframeChannel();          // nombre del canal ("X Location", ...) para el titulo
// SELECCION MULTIPLE (tarjetas Curva / Keyframes de propiedades): las curvas con keyframes elegidos (con su nombre de
// canal) y cada keyframe elegido como (curva, indice). La curva ACTIVA = la del keyframe activo, o la unica elegida.
int           DopeCurvesPicked(std::vector<AnimProperty*>& curves, std::vector<std::string>* names, bool edit = true);
int           DopeKeysPicked(std::vector<AnimProperty*>& curves, std::vector<int>& idx, bool edit = true);
AnimProperty* DopeActiveCurve(std::string* name, bool edit = true);
// editar la seleccion desde las tarjetas (un paso de undo cada uno): los keyframes elegidos...
void DopeKeysSetValue(float v);
void DopeKeysSetFrame(int frame);
void DopeKeysSetHandle(int side, int axis, float v);   // lado 0 izq / 1 der; eje 0 X / 1 Y
void DopeKeysSetInterp(int id);                        // id >= 100: el easing (id - 100)
void DopeKeysSetHandleType(int type);
// ...y todos los keyframes de las curvas elegidas
void DopeCurvesSetInterp(int id);
void DopeCurvesSetHandleType(int type);
void          DopeActiveKeyframeReFrame(int newFrame); // el frame cambio: seguirlo (y mover la seleccion)
// Aceptar / cancelar ese transform desde AFUERA: el click izq/der de PC, el OK/backspace de Symbian y el Tab
// (que cambia de viewport por encima del ruteo del timeline). Un solo par de funciones para todas las plataformas.
void DopeXformCancel();
void DopeXformAccept();

// Regla de pertenencia fila padre -> canal (ownerKey). La expone el test 'dopecubre'.
bool W3dScriptDopeCovers(const std::string& parent, const std::string& child);


// AJUSTAR LA ULTIMA OPERACION del timeline (PopUp/TimelineRedoPanel.cpp): lo que se hizo al mover / rotar / escalar
// keyframes, para editarlo despues de soltar. modo: 0 mover, 1 rotar, 2 escalar.
struct TimelineRedoOp { int mode; bool curves; int pivot; float frames, value, angle, escX, escY; };
bool      TimelineRedoRead(TimelineRedoOp& op);           // false = no hay operacion para ajustar
bool      TimelineRedoReapply(const TimelineRedoOp& op); // la vuelve a hacer con estos valores (mismo paso de undo)
void      TimelineRedoCancel();                          // la deshace
void      TimelineRedoForget();
bool      TimelineRedoReapplying();
Timeline* TimelineRedoView();
int       TimelineToolbarHeight(const Timeline* t);
#endif // TIMELINE_H
