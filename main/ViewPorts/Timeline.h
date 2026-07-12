#ifndef TIMELINE_H
#define TIMELINE_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/WithBorder.h"           // borde (verde si activo) como Outliner/Properties
#include "WhiskUI/Propieties/PropFloat.h"   // edicion numerica de Start/End/Frame (reusa NumEdit)
#include "WhiskUI/Button.h"                  // controles de transporte + campos = Buttons de la barra

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
class Timeline : public ViewportBase, public WithBorder {
    public:
        int ViewportKind() const { return 5; } // 1=3D 2=outliner 3=properties 4=UV 5=timeline

        float pxPerFrame; // zoom: pixeles por frame
        float viewStartF; // frame en el borde IZQUIERDO (float; arranca en negativo para ver el frame 0 prolijo)

        // campos numericos (reusan PropFloat: click -> editar por texto, entero, Enter aplica via onChange)
        PropFloat* pfStart; PropFloat* pfEnd; PropFloat* pfCur;
        float fStart, fEnd, fCur;

        // controles = Buttons de la barra del viewport (scrollean con la barra)
        Button* btnT[8];   // transporte (glyph vectorial dibujado encima)
        Button* btnStart;  // campo Start
        Button* btnEnd;    // campo End
        Button* btnAnim;   // dropdown: elegir la animacion del esqueleto (solo si hay armature con clips)

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
        void ZoomBy(float factor, int centerXlocal); // zoom manteniendo fijo el frame bajo centerX
        void PanFrames(float dFrames);  // corre la vista N frames
        float MinView() const;          // tope de paneo hacia el lado negativo (comodo)
        void SetFrameFromX(int localX); // scrub

        float FrameToX(float f) const;
        float XToFrame(float localX) const;

        void EditarCampo(int i);        // 0=Start 1=End 2=Frame actual: arranca edicion por texto
        void ApplyStart(); void ApplyEnd(); void ApplyCur(); // aplican los espejos float a los globales

    private:
        void SyncFields();  // espejos float <- globales + texto/editField de los botones
        int  TickStep() const; // paso "redondo" de la regla segun el zoom
};

#endif // TIMELINE_H
