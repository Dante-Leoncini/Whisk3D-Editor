#ifndef HOME_POPUP_H
#define HOME_POPUP_H
// ============================================================================
//  HomePopup.h - LA PANTALLA DE INICIO: al abrir el editor sin un archivo aparece una tarjeta al medio (el borde redondeado
//  y verde de las tarjetas de Whisk3D), con el editor atenuado detras.
//    - arriba, en NEGRO: el logo con el ENGRANAJE GIRANDO (el emblema del medio queda quieto), a su derecha el texto
//      "Whisk3D" (negro con borde blanco y la geometria del 3, res/home/text.png) y abajo la pagina (whisk3d.com.ar, a la izquierda) y el
//      numero de version (a la derecha)
//    - "Nuevo archivo" (la escena de arranque, como el "Nuevo proyecto" de la tarjeta Archivo). Con layouts guardados
//      (config/W3dLayouts.h) la tarjeta pasa a mostrar LA LISTA DE LAYOUTS para arrancar con el que se quiera: el rotulo
//      "Nuevo archivo", "Por defecto", los guardados e "Ir atras" (Esc tambien vuelve). Sin ninguno guardado, directo
//    - "Abrir..." (el explorador, como "Abrir proyecto" de la tarjeta Archivo)
//    - "Archivos recientes": los ultimos 5 abiertos o guardados (config/W3dRecent.h); un toque lo abre. Si no hay
//      ninguno no aparece ni el rotulo: quedan "Nuevo archivo" y "Abrir..."
//  Cada opcion va separada con una linea. Un toque afuera o Esc la cierran (queda la escena de arranque, lo mismo que
//  "Nuevo archivo"). Elegir se hace AL SOLTAR (el dedo o el
//  boton del mouse): el toque no sigue de largo a lo que abre. Las texturas del logo (res/home/: reflejo, mascara,
//  borde, centro y texto, de tools/logo_inicio.py) se cargan al abrirla y se LIBERAN al cerrarla.
// ============================================================================
#include "PopUpBase.h"
#include <string>
#include <vector>

class HomePopup : public PopUpBase {
    public:
        HomePopup();
        ~HomePopup();
        void Open();
        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        // las FILAS que se eligen: 0 = Nuevo archivo, 1..N = los recientes, N+1 = Abrir...
        // En la lista de layouts: 0 = Por defecto, 1..L = los guardados, L+1 = Ir atras
        int Rows() const { return 2 + (int)(showLayouts ? layouts.size() : recent.size()); }
        int RowIn(int mx, int my) const;      // la fila elegible bajo el punto (-1 = ninguna)
        void Pick(int row, bool keyboard = false);   // teclado: la lista de layouts abre con la primera resaltada
        void ShowLayouts(bool show, bool keyboard = false);

        bool showLayouts;                      // la tarjeta muestra la lista de layouts (despues de "Nuevo archivo")
        std::vector<std::string> layouts;      // los guardados (sin "Por defecto"), en el orden de la lista

        std::vector<std::string> recent;
        // el engranaje, como en la intro de las demos: el reflejo QUIETO, la mascara GIRANDO que lo recorta
        // (multiplicando), el borde girando y el emblema del medio quieto. 0 = sin cargar (el logo no se dibuja)
        unsigned int texReflection, texMask, texBorder, texCenter;
        bool LogoLoaded() const { return texReflection && texMask && texBorder && texCenter && texText; }
        bool LogoFreed() const { return !texReflection && !texMask && !texBorder && !texCenter && !texText; }
        unsigned int texText;                // "Whisk3D" (0 = sin cargar: va con la letra de la UI)
        int texTextW, texTextH;              // su tamano en pixeles (la proporcion al dibujarlo)
        int sel;                             // la fila resaltada (mouse encima / flechas)
        int pressed;                         // la fila donde se apoyo el dedo / se apreto el boton (-1 = ninguna)
        double t0;                           // cuando se abrio: el angulo del engranaje
        Card* rect;                          // para los rectangulos (el negro, las lineas, el resaltado)
        // el armado (coordenadas locales): cabecera negra, logo, y donde empieza cada fila
        int width, height, headerHeight, sideLogo, rowHeight, yRows, yRecentTitle;
        int yLogo;                             // donde empieza el logo
        int colX, colW;                        // la columna de las opciones (y de la pagina y la version): la tarjeta
        void Layout();
        int RowY(int row) const;             // la y local de una fila elegible
        int LastRow() const;                 // la que queda mas abajo (la ultima reciente, o Abrir... sin recientes)
};

void HomePopupOpen();
void HomePopupClose();       // si esta abierta (abrir un proyecto o empezar uno nuevo la cierran)
bool HomePopupIsOpen();
extern HomePopup* homePopup;

#endif // HOME_POPUP_H
