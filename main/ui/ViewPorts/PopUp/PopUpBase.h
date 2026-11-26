#ifndef POPUPBASE_H
#define POPUPBASE_H

#include <string>
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/theme/colors.h"
#include "WhiskUI/draw/glesdraw.h" // W3dScreenHeight (flip de Y, 4 OS)

// LAS TARJETAS DE LA ULTIMA OPERACION (mover/rotar/escalar, la del timeline, la de la malla) mientras se ARRASTRA uno de
// sus valores con el dedo o el mouse: la tarjeta entera se dibuja con este alfa (w3dEngine::g_alphaColor) para ver detras
// como va quedando el cambio, y la fila que se edita (su nombre y su campo) va opaca encima (GroupPropertie::RenderRow)
const float kAlphaCardDrag = 0.4f;

// base de los popups modales (selector de color, etc). El ruteo de
// entrada compartido (LayoutInput) les da prioridad sobre todo lo demas.
/**
 * @brief Clase base de los diálogos flotantes (popups), que van encima de todo.
 *
 * Heredan de PopUpBase: ColorPicker (selector de color), FileBrowser (abrir/guardar), ConfirmPopup ("¿Borrar X?"),
 * RedoMeshPanel (la ventanita "Add ..." con los parámetros de la primitiva), ProgressPopup (import largo), y los
 * teclados en pantalla de Symbian (NumPad, QwertyPad). Se dibujan sobre los viewports y capturan el input mientras
 * están abiertos.
 */
class PopUpBase {
    public:
        std::string name;
        int x, y; // posicion ABSOLUTA en pantalla (arbol arriba-izq)
        Card* popUpWindow;

        PopUpBase(const std::string& Name);
        virtual ~PopUpBase();

        void initView(); // viewport+ortho propios del popup
        void endView();

        bool Contains(int mx, int my) const;

        virtual void Render();
        // Click devuelve false si fue AFUERA (el caller cierra el popup)
        virtual bool Click(int mx, int my);
        virtual bool Motion(int mx, int my);
        virtual bool Key(int key); // LayoutKey::Enum
        // flecha MANTENIDA (frame-based, keypad N95 que no auto-repite): por defecto NO hace nada
        // (la navegacion es 1-por-tap). El color picker la override para AJUSTAR valores manteniendo.
        virtual bool KeyRepeat(int key) { return true; }
        virtual void Wheel(int delta) {} // rueda del mouse (+1 arriba, -1 abajo)
        virtual void Release();      // PC: se solto el boton izquierdo
        virtual bool Dragging();     // para el cursor violeta (Symbian)
        virtual void Close();

        // SEMI-MODAL: si es true, una accion del viewport (orbitar, r/g/s,
        // click) con el mouse AFUERA del popup lo cierra y deja pasar la accion
        // Por defecto los popups son modales.
        // Enter (enter = true) o Esc con SU caja de texto enfocada: true = lo tomo (aceptar / cancelar de una, como la
        // ventanita del nombre). false = lo de siempre: la caja se desenfoca y el popup sigue abierto
        virtual bool KeyInText(bool enter) { (void)enter; return false; }
        virtual bool ClosesWithViewport() { return false; }
        // PLEGABLE (los paneles de ajustar la ultima operacion): plegado se ve solo el titulo y NO se queda con el teclado
        // (el N95 sigue trabajando con el panel a la vista); el lapiz lo despliega para editarlo y lo vuelve a plegar
        virtual bool Collapsible() { return false; }
        virtual bool Collapsed() { return false; }
        virtual void Expand(bool open) { (void)open; }
        // SEGUIR TRABAJANDO: el usuario usa el viewport AFUERA del popup (la rueda, orbitar) sin contestarle. Solo los
        // paneles de ajustar la ultima operacion (abajo a la izquierda: mover, escalar, inset...) se resuelven solos:
        // la operacion ya esta hecha, seguir trabajando la deja asi -> se cierran y la accion pasa al viewport. Los que
        // piden una respuesta (borrar, pisar un archivo, descartar cambios) devuelven false: hay que contestarles
        virtual bool ContinueWorking() { if (!ClosesWithViewport()) return false; Close(); return true; }
        // NO MODAL (Buscar del IDE): un click o una tecla con el puntero AFUERA del popup van al viewport de abajo
        // como si no estuviera, y el popup sigue abierto (se cierra con su boton o con Esc).
        virtual bool NoModal() { return false; }
        // TECLADO en pantalla (numerico / qwerty): sus teclas EDITAN el campo activo, asi que un click adentro NO
        // commitea la edicion. En cualquier otro popup, un click en otro control APLICA lo tipeado.
        virtual bool IsKeyboard() const { return false; }
};

extern PopUpBase* PopUpActive;

// LOS PANELES DE AJUSTAR LA ULTIMA OPERACION (el del timeline y el de mover / rotar / escalar): una tarjeta plegable con
// sus filas. Lo comun a los dos: tocar una fila, arrastrar un numero (un toque sin arrastrar lo escribe), desplazarse
// con el dedo o la rueda y las flechas / OK sobre las filas. Cada panel arma su tarjeta, se dibuja y decide que hace
// Cancelar (y si tiene algo mas, como la lista de Transformar)
#include "ScrollablePanel.h"
class GroupPropertie; class PropFloat;
class CardPanel : public PopUpBase {
    public:
        CardPanel();
        bool Motion(int mx, int my);
        void Release();
        void Wheel(int delta);
        bool Dragging();
        bool ClosesWithViewport() { return true; }   // click afuera = aceptar (y el click sigue)
        bool Collapsible() { return group != 0; }
        bool Collapsed();
        void Expand(bool open);

        GroupPropertie* group;
        PropFloat* dragField; bool dragMoved; int lastDragMx;
        ScrollablePanel dz;          // mas alto que media pantalla: recortado y con desplazamiento
        // la fila bajo el puntero: un numero arma el arrastre; lo demas se toca (onlyBuild: solo arma el arrastre, el
        // toque se resuelve al soltar si el panel se desplaza)
        void ClickRow(int mx, int my, bool onlyBuild);
        int  RowY(int idx) const;
        bool KeyRows(int key);       // flechas y OK sobre las filas (la tarjeta abierta); false = la tecla no es de ellas
};

#endif
