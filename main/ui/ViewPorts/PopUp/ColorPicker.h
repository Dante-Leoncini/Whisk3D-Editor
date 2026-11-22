#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <string>
#include "PopUpBase.h"
#include "WhiskUI/draw/rectangle.h"
#include "WhiskUI/widgets/Tab.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/TextField.h"

class Object;   // el objeto dueno del color (contexto de la pestania Paleta)

// Selector de color (4 OS): circulo cromatico (el arte de
// 32x32 en (96,70) del atlas de la UI), barra de Valor, vista previa
// (original | actual) y pestanias RGB / HSV / Hex con valores 0..255.
// CUARTA pestania "Pal" (solo con contexto de paleta, ver SetPalette): los
// COLORES de la paleta efectiva del objeto (click = el campo referencia ese
// INDICE; "Propio" vuelve al color propio) y las PALETAS del proyecto
// (click = la paleta del OBJETO; "Igual que el padre" vuelve a heredar).
// Acepta con click/enter; cancela (restaura) con esc, la C del telefono
// o la X; se cierra solo si el mouse se aleja (como los desplegables).
class ColorPicker : public PopUpBase {
    public:
        GLfloat* target;     // RGBA 0..1 (el float[4] del material)
        GLfloat original[4]; // para cancelar
        float h, s, v;       // estado HSV (el circulo edita H/S)
        int tab;             // 0 = RGB, 1 = HSV, 2 = Hex, 3 = Paleta
        Tab* tabs[4];
        int nTabs;           // 3 sin contexto de paleta (materiales); 4 con el
        // contexto de PALETA (lo setea Properties al abrir; NULL = sin pestania)
        int* palRef;         // el campo indice pal* del elemento (-1 = propio)
        Object* palObj;      // el elemento dueno (su paleta efectiva y su seleccion)
        void (*onPaletteChange)();   // aviso al panel (re-bind + redraw)
        int palScroll;       // primera LINEA visible de la lista (si no entra toda)
        int palVis;          // cuantas lineas entran
        int palRowH;         // alto de una linea de la lista
        int row;             // foco de teclado: -1 = PESTANIAS, 0..Rows()-1 = filas de valor, Rows() = OK/Cancel
        int okFocus;         // dentro de OK/Cancel: 0 = OK (DERECHA), 1 = Cancel (IZQUIERDA) (logica S60)
        bool editCircle;     // foco en el circulo + OK -> editar: izq/der = Hue, arr/aba = Saturacion
        bool editValue;      // foco en la barra value + OK -> editar: arr/aba (y der/izq) = Value
        int holdDir;         // aceleracion del hold: direccion del ultimo izq/der (+1/-1/0)
        int holdCount;       // cuantos izq/der seguidos en esa direccion (rampa el paso)
        int drag;            // 0 nada, 1 circulo, 2 barra V, 3 fila
        bool moved;          // hubo movimiento desde que empezo el drag
        int dragX;           // ultimo X del drag de una fila
        Rec2D* rect;         // dibujado plano (marcadores/bandas/preview)
        Card* rowCard;       // la caja de cada fila (como las propiedades)
        Button* btnOk;       // aceptar (el color ya esta aplicado)
        Button* btnCancel;   // cancelar (restaura el original)
        Button* btnUnit;     // switch 0-255 / 0-100% (no existe en HSV)
        // pestania Hex: el valor se ESCRIBE con el teclado (la caja de texto de siempre, en vivo: cada hex valido ya
        // pinta) y se copia / pega con el portapapeles del sistema (los dos botones de abajo)
        TextField hexField;
        Button* btnCopy;
        Button* btnPaste;
        int hexBtnY;       // fila de Copiar / Pegar (debajo del valor)
        void HexEdit();    // enfoca la caja con el valor actual (todo seleccionado)
        bool HexApply(const std::string& t);   // un hex valido -> el color (false = no es un hex)

        // layout (se calculan en Abrir, en pixeles del popup)
        int circX, circY, circSide;
        int barX, barW;
        int prevX, prevW;
        int tabsY, tabHeight;
        int unitX;          // donde arranca el toggle 0-255 / 0-100%
        int rowsY;          // la tarjeta-grupo de los 3 valores
        int groupH;         // alto de esa tarjeta
        int alphaY;         // la fila de Alpha (separada)
        int btnY;           // fila del boton de unidad (ancho completo)
        int btnOkY;         // fila de OK / Cancel (50%% y 50%%)

        ColorPicker();
        ~ColorPicker();

        void Open(GLfloat* Target, int px, int py);
        // habilita la pestania "Pal" (llamar DESPUES de Abrir): ref = el campo
        // indice del elemento; obj = el elemento (paleta efectiva/seleccion)
        void SetPalette(int* ref, Object* obj, void (*onChange)());
        void Reflow();          // recalcula el layout (el alto depende de la pestania)
        void Close() W3D_OVERRIDE; // Ctrl+Z: al cerrar captura el cambio de color (push solo si cambio; cancelar restaura -> no) Y SUELTA target/palRef/palObj
        // el elemento dueno se esta DESTRUYENDO: soltar target/palRef/palObj (todos apuntan
        // adentro de el) y cerrar, sin desreferenciar nada. Ver el bloque en ColorPicker.cpp.
        void ForgetOwner();

        void Render();
        bool Click(int mx, int my);
        bool Motion(int mx, int my);
        bool Key(int key);
        bool KeyRepeat(int key); // flecha mantenida (N95): SOLO ajusta valores (no navega)
        int  StepHold(int dir);  // aceleracion del hold: arranca LENTO (1) y acelera de a poco (antes saltaba a 30)
        void Release();  // PC: el boton se solto (si hubo movimiento)
        bool Dragging(); // cursor violeta mientras se arrastra

        int Rows() const; // filas de la pestania activa
        void FromRGB();  // target -> h,s,v
        void ToRGB();    // h,s,v -> target (el alpha no se toca)
        void FitRow(int delta); // +-delta en escala 0..255
        // ---- pestania Paleta ----
        int  PalColorCount() const;     // colores de la paleta efectiva del objeto
        int  PalPaletteCount() const;   // paletas del proyecto
        int  PalRowLine(int k) const;  // fila seleccionable -> linea de display
        void PalApplyRow(int k);       // ejecuta la fila (indice / propio / paleta)
        void PalEnsureVisible();       // scrollea para que la fila enfocada se vea
};

extern ColorPicker* colorPicker;

// UV del CIRCULO CROMATICO contra el tamano REAL del atlas de la UI (misma familia que CalcCardUV /
// CalcBorderUV / CalcScrollUV: la llama W3dInitUI cuando ya sabe cuanto mide la textura). El arte son
// 32x32 pixeles en el (96,70) de font.png, y font.png va SIEMPRE pegado en el (0,0) del atlas -> lo unico
// que cambia entre el atlas clasico y el DINAMICO (font.png + los iconos sueltos empaquetados, que sale
// mas grande que 128) es por cuanto se divide. Con los UV a 128 fijo la rueda apuntaba a otra parte de la
// textura y desaparecia. Sin llamarla quedan los valores del atlas clasico de 128x128.
void CalcColorWheelUV(int texW, int texH);
// solo para el harness: los 4 UV que quedaron (u0,v0,u1,v1)
void ColorWheelUV(float* u0, float* v0, float* u1, float* v1);

// unidad de los valores RGBA del picker: 0 = 0..255, 1 = 0..100%
// (queda guardada en memoria entre aperturas)
extern int ColorPickerUnit;

#endif
