#ifndef EDITOR2D_H
#define EDITOR2D_H

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/WithBorder.h" // borde (verde si activo), igual que UV/Outliner
#include "render/UIOverlay.h"     // dibujo compartido de la UI 2D (mismo que el viewport 3D)
#include <vector>

// ROLES estables de los botones de la barra del Editor 2D (dispatch por rol via BarRolBtn,
// NO por indice: agregar/reordenar botones no rompe nada; mismo esquema que el 3D y el UV).
enum BarRol2D {
    BR2D_Select = 1, // todo / nada / invertir
    BR2D_Add,        // crear elementos 2D
    BR2D_Object,     // transformar / duplicar / emparentar / borrar
    BR2D_View,       // zoom / vista de juego
    BR2D_Pivot       // pivote de transformacion (icono)
};

// ROLES de la TOOLBAR inferior del Editor 2D (mecanismo compartido de ViewportBase).
enum ToolbarRol2D {
    TBR_2DHist = 1   // .. TBR_2DHist+2: historial MRU (Move/Rotate/Scale de elementos)
};

// =====================================================================
//  Editor 2D — lienzo para INTERFACES de juegos/programas (HUD, menus),
//  juegos 2D y animacion 2D. Mismo manejo que el UV Editor:
//    rueda del mouse       = zoom (hacia el cursor)
//    boton del MEDIO + mover = paneo
//    flechas               = paneo con teclado
//
//  V1: el lienzo con grilla + el marco de "pantalla" de referencia
//  (1080x1920, un celular en vertical: el caso de los slots). Sobre esto
//  se monta la edicion del objeto UI (widgets, imagenes, export).
// =====================================================================
class Editor2D : public ViewportBase, public WithBorder {
    public:
        int ViewportKind() const { return 6; } // 1=3D 2=outliner 3=props 4=UV 5=timeline 6=2D

        float zoom;           // px de pantalla por px de lienzo (arranca ajustado al viewport)
        bool  zoomInicializado; // el primer Render calcula el zoom "que entre comodo" UNA vez
        float panX, panY;     // desplazamiento de la vista, en pixeles de viewport
        int   lastMx, lastMy; // para el delta del paneo con boton del medio

        // VISTA DE JUEGO (numpad 0 / menu View > "Game View"): el analogo de la vista de
        // camara de Blender. El lienzo se ENCAJA en el rect UTIL del viewport (el viewport
        // MENOS su barra de menu) con la misma cuenta que hace el runtime en W3dGameRender,
        // y se apagan TODOS los adornos de edicion: se ve lo mismo que en el dispositivo.
        // Mientras dura, el zoom y el paneo se CONGELAN (no se pisan: al salir vuelve el
        // encuadre exacto que habia). Es POR VIEWPORT (como ViewFromCameraActive del 3D),
        // no global: se puede tener un viewport 2D editando y otro mostrando el juego.
        bool  vistaJuego;

        // CURSOR 2D (en px de lienzo): pivote opcional + referencia. Shift+click lo coloca.
        float cursor2DX, cursor2DY;
        bool  cursorColocado;   // false = sigue el centro de la ventana
        // PIVOTE de transformacion: desde donde se gira. 0=centro de la seleccion (mediana),
        // 1=elemento activo, 2=cursor 2D. Se elige en el boton de la barra.
        int   pivotModo;

        // TRANSFORM 2D en curso (G = mover, R = rotar, S = escalar; X/Y restringen el eje)
        int   xform;          // 0=nada 1=mover 2=rotar 3=escalar
        int   xformEje;       // 0=libre 1=solo X 2=solo Y
        int   xformMx, xformMy;                 // mouse al iniciar (pantalla)
        std::vector<Object*> xformObjs;         // los elementos que se estan transformando
        std::vector<Vector3> xformOrig;         // su pos original (para aplicar/cancelar)
        std::vector<float>   xformRotOrig;      // su rot2d original
        std::vector<float>   xformTamOrig;      // escalar: tam (texto) / ancho (imagen) original
        std::vector<float>   xformTamOrig2;     // escalar: alto original (imagen)
        std::vector<UI2DPos> xformResueltas;    // su posicion en PANTALLA al iniciar
        float xformPivX, xformPivY;             // pivote en px de PANTALLA (al iniciar)

        // ARRASTRE directo con el mouse: mover un elemento agarrandolo, redimensionar una
        // imagen por sus puntos de agarre (esquinas/bordes) o el marco del lienzo (responsive)
        int   drag;            // 0=nada 1=mover elemento 2=resize imagen 3=resize lienzo
        int   dragHandle;      // 0=arr-izq 1=arriba 2=arr-der 3=der 4=aba-der 5=abajo 6=aba-izq 7=izq
        Object* dragObj;       // el elemento agarrado (drag 1 y 2)
        Vector3 dragOrigPos;   // su pos al iniciar
        float dragOrigW, dragOrigH;   // ancho/alto originales (imagen o lienzo)
        int   dragMx, dragMy;         // mouse al iniciar (pantalla)
        bool  dragPaso;               // el mover ya supero el umbral anti-accidente
        std::vector<Object*> dragObjs;   // mover: TODA la seleccion 2D
        std::vector<Vector3> dragOrigs;
        std::vector<float>   dragRefW;   // rect de referencia de cada uno (pos = RELATIVA a el)
        std::vector<float>   dragRefH;
        float dragObjRefW, dragObjRefH;  // ref del elemento del resize (drag 2)

        // ACUMULADORES wrap-safe: el cursor se envuelve en los bordes (como el 3D) y ese
        // salto no debe sumar. Las acciones usan esto, nunca (mouse - inicio).
        float acumX, acumY;   // delta acumulado, en px de pantalla
        float acumAng;        // angulo acumulado alrededor del pivote (radianes)
        float acumFactor;     // factor de escala acumulado (multiplicativo)
        bool  accionPrimerMov; // el 1er motion tras iniciar no suma (lastMx puede estar viejo)
        bool  moverTrasDuplicar; // Shift+D: el proximo Render arranca el mover de las copias

        // estado del ultimo Render (para pasar de pantalla a lienzo fuera del dibujo)
        float lastFx, lastFy, lastEsc;
        std::vector<UI2DPos> posiciones;        // posicion resuelta de cada elemento (pantalla)

        Editor2D();
        virtual ~Editor2D();

        void Render() override;
        void Resize(int newW, int newH) override;
        void event_mouse_motion(int mx, int my) override;
        bool event_finger_scroll(int px, int py, int dx, int dy) override;             // touch: 1 dedo panea
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) override; // touch: 2 dedos zoom+paneo

        void Panear(float dx, float dy); // compartido PC/Symbian (flechas / touch)
        void ZoomCentro(int dir);        // zoom centrado (teclado / pinch)
        void button_left() override;     // confirmar transform / Shift+click = colocar el cursor 2D
        void button_right() override;    // cancelar transform
        // transform 2D
        void IniciarXform2D(int modo);   // 1=mover 2=rotar (captura seleccion + pivote)
        void AplicarXform2D(int mx, int my);
        void ConfirmarXform2D();
        void CancelarXform2D();
        // ---- hooks del TRANSFORM UI COMPARTIDO (TransformUI.cpp): barra de info, entrada
        // numerica exacta (en px de LIENZO), warp del cursor y tilde/cruz/X/Y de la toolbar.
        // Mismo mecanismo que el 3D y el UV editor, sin duplicar codigo. ----
        bool XformEnCurso() const override { return xform != 0; }
        void XformNumValor(float v) override;   // v en px de lienzo (mover) / grados / factor
        void XformConfirmar() override;         // gate: expresion invalida NO confirma
        void XformCancelar() override;
        void XformToggleEje(int eje) override;  // teclas X/Y y botones de la toolbar
        std::string XformTextoBarra() override; // texto de la barra de info (px)
#ifndef W3D_SYMBIAN
        void event_key_down(int tecla, bool repeticion) override; // flechas = paneo
        void event_mouse_wheel(float dy, int mx, int my) override;
        // IMPRESCINDIBLE: resetear ViewPortClickDown al soltar (sino viewPortActive se congela
        // y el borde verde/resize dejan de andar; mismo gotcha documentado en UVEditor).
        void mouse_button_up(int boton) override;
#endif
        // toolbar inferior compartida (ToolbarBase): Mover/Rotar/Escalar de elementos (MRU).
        // Oculta en vista de juego y durante el Play (el juego se mira limpio, como el 3D).
        bool ToolbarVisible() const override;
        void ToolbarSincronizar() override;
        void ToolbarAccionRol(int rol) override;
        // mapeo lienzo->pantalla (centro cx,cy del marco y escala s), igual que usa Render.
        void ParamsLienzo(float& cx, float& cy, float& s) const;
        // rect UTIL del viewport (todo MENOS la barra de menu), en coords LOCALES del viewport
        void RectUtil(int* uy, int* uh) const;
        // VISTA DE JUEGO: fija (o limpia) el override del tamano de ventana (UI2D_OverrideVentana)
        // para que el LIENZO sea el rect util de ESTE viewport. Lo llaman Render (cada frame:
        // sigue los resize) y SimJuego antes de mapear el input (Editor2DAlimentarOverride).
        void ActualizarOverrideLienzo();
    private:
        void PivoteEnPantalla(float* px, float* py);   // el pivote actual, en px de pantalla
        void AplicarDrag(int mx, int my);              // mueve/redimensiona segun el drag activo
};

// (el viejo Editor2DXformActivo se reemplazo por el hook COMPARTIDO ViewportBase::XformEnCurso:
//  controles.cpp envuelve el cursor con el mismo chequeo para el Editor 2D y el UV editor)
// acciones compartidas con los menus de la barra (LayoutInput) y los atajos
void Editor2DDuplicarSeleccion(Editor2D* ed);   // duplica la seleccion 2D (copias +20,+20)
void Editor2DSeleccionar(int modo);             // 0 = todo, 1 = nada, 2 = invertir (solo 2D)
void Editor2DZoom1a1(Editor2D* ed);             // zoom 1:1 (pixel-perfect) + vista centrada
void Editor2DZoomExacto(Editor2D* ed, int n);   // zoom exacto xN, centrado
void Editor2DEncuadrarSeleccion(Editor2D* ed);  // encuadrar lo seleccionado (numpad .)
// VISTA DE JUEGO (numpad 0 / menu View): prende o apaga el modo en ESE viewport 2D
void Editor2DVistaJuego(Editor2D* ed, bool on);
// SimJuego (input del Play): si 'vp' es un Editor2D, re-fija su override de lienzo ANTES de
// mapear pantalla->lienzo, asi el input usa el MISMO lienzo que el ultimo dibujo aunque haya
// varios viewports 2D (el que alimenta gana). No hace nada si vp no es un viewport 2D.
void Editor2DAlimentarOverride(ViewportBase* vp);
void Editor2DBorrarSeleccion();                 // borra la seleccion (popup de confirmacion)

#endif // EDITOR2D_H
