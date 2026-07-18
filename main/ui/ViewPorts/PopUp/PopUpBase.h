#ifndef POPUPBASE_H
#define POPUPBASE_H

#include <string>
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/glesdraw.h" // W3dPantallaAlto (flip de Y, 4 OS)

// base de los popups modales (selector de color, etc). El ruteo de
// entrada compartido (LayoutInput) les da prioridad sobre todo lo demas.
/**
 * @brief Clase base de los diálogos flotantes (popups), que van encima de todo.
 *
 * Heredan de PopUpBase: ColorPicker (selector de color), FileBrowser (abrir/guardar), ConfirmarPopup ("¿Borrar X?"),
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
        virtual bool Tecla(int tecla); // LayoutKey::Enum
        // flecha MANTENIDA (frame-based, keypad N95 que no auto-repite): por defecto NO hace nada
        // (la navegacion es 1-por-tap). El color picker la override para AJUSTAR valores manteniendo.
        virtual bool TeclaRepeat(int tecla) { return true; }
        virtual void Wheel(int delta) {} // rueda del mouse (+1 arriba, -1 abajo)
        virtual void Soltar();          // PC: se solto el boton izquierdo
        virtual bool Arrastrando();     // para el cursor violeta (Symbian)
        virtual void Cerrar();

        // SEMI-MODAL: si es true, una accion del viewport (orbitar, r/g/s,
        // click) con el mouse AFUERA del popup lo cierra y deja pasar la accion
        // (estilo redo-panel de Blender). Por defecto los popups son modales.
        virtual bool CierraConViewport() { return false; }
};

extern PopUpBase* PopUpActive;

#endif
