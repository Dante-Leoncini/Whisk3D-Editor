#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <string>
#include "PopUpBase.h"
#include "WhiskUI/rectangle.h"
#include "WhiskUI/Tab.h"
#include "WhiskUI/Button.h"

// Selector de color tipo Blender (4 OS): circulo cromatico (el arte de
// 32x32 en (96,70) del atlas de la UI), barra de Valor, vista previa
// (original | actual) y pestanias RGB / HSV / Hex con valores 0..255.
// Acepta con click/enter; cancela (restaura) con esc, la C del telefono
// o la X; se cierra solo si el mouse se aleja (como los desplegables).
class ColorPicker : public PopUpBase {
    public:
        GLfloat* target;     // RGBA 0..1 (el float[4] del material)
        GLfloat original[4]; // para cancelar
        float h, s, v;       // estado HSV (el circulo edita H/S)
        int pestania;        // 0 = RGB, 1 = HSV, 2 = Hex
        Tab* tabs[3];
        int fila;            // foco de teclado: -1 = PESTANIAS, 0..Filas()-1 = filas de valor, Filas() = OK/Cancel
        int okFoco;          // dentro de OK/Cancel: 0 = OK (DERECHA), 1 = Cancel (IZQUIERDA) (logica S60)
        bool editCirculo;    // foco en el circulo + OK -> editar: izq/der = Hue, arr/aba = Saturacion
        bool editValue;      // foco en la barra value + OK -> editar: arr/aba (y der/izq) = Value
        int holdDir;         // aceleracion del hold: direccion del ultimo izq/der (+1/-1/0)
        int holdCount;       // cuantos izq/der seguidos en esa direccion (rampa el paso)
        int arrastre;        // 0 nada, 1 circulo, 2 barra V, 3 fila
        bool movio;          // hubo movimiento desde que empezo el drag
        int arrastreX;       // ultimo X del drag de una fila
        Rec2D* rect;         // dibujado plano (marcadores/bandas/preview)
        Card* filaCard;      // la caja de cada fila (como las propiedades)
        Button* btnOk;       // aceptar (el color ya esta aplicado)
        Button* btnCancel;   // cancelar (restaura el original)
        Button* btnUnidad;   // switch 0-255 / 0-100% (no existe en HSV)

        // layout (se calculan en Abrir, en pixeles del popup)
        int circX, circY, circLado;
        int barraX, barraW;
        int prevX, prevW;
        int tabsY, tabAlto;
        int unidadX;         // donde arranca el toggle 0-255 / 0-100%
        int filasY;          // la tarjeta-grupo de los 3 valores
        int grupoH;          // alto de esa tarjeta
        int alphaY;          // la fila de Alpha (separada)
        int btnY;            // fila del boton de unidad (ancho completo)
        int btnOkY;          // fila de OK / Cancel (50%% y 50%%)

        ColorPicker();
        ~ColorPicker();

        void Abrir(GLfloat* Target, int px, int py);
        void Cerrar() override; // Ctrl+Z: al cerrar captura el cambio de color (push solo si cambio; cancelar restaura -> no)

        void Render();
        bool Click(int mx, int my);
        bool Motion(int mx, int my);
        bool Tecla(int tecla);
        bool TeclaRepeat(int tecla); // flecha mantenida (N95): SOLO ajusta valores (no navega)
        int  PasoHold(int dir);      // aceleracion del hold: arranca LENTO (1) y acelera de a poco (Dante: antes saltaba a 30)
        void Soltar();      // PC: el boton se solto (si hubo movimiento)
        bool Arrastrando(); // cursor violeta mientras se arrastra

        int Filas() const; // filas de la pestania activa
        void DeRGB();  // target -> h,s,v
        void AlRGB();  // h,s,v -> target (el alpha no se toca)
        void AjustarFila(int delta); // +-delta en escala 0..255
};

extern ColorPicker* colorPicker;

// unidad de los valores RGBA del picker: 0 = 0..255, 1 = 0..100%
// (queda guardada en memoria entre aperturas)
extern int ColorPickerUnidad;

#endif
