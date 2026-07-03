#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <string>
#include <vector>
#include "PopUpBase.h"
#include "WhiskUI/Button.h"
#include "WhiskUI/card.h"
#include "ViewPorts/ScrollBar.h" // Scrollable: el MISMO scroll que el outliner
#include "w3dFilesystem.h"

// ============================================================================
//  File browser COMPARTIDO (4 OS) — modal a pantalla completa, estilo Blender.
//  Reusa los elementos que ya existen: Card (9-patch con borde), Button (con
//  icono/tinte) y Scrollable (la barra de scroll real, con su textura).
// ============================================================================
class FileBrowser : public PopUpBase, public Scrollable {
    public:
        void (*onAccept)(const std::string& path);
        std::string actionLabel;
        std::string filterExt;     // extension valida en minuscula ("" = todas)
        bool modoGuardar;          // true = elegir CARPETA destino (devuelve currentPath
                                   // o el archivo sel); el boton de accion siempre activo

        std::string currentPath;
        std::vector<w3dFileSystem::DirEntry> entries;
        std::vector<w3dFileSystem::Bookmark> bookmarks;

        std::vector<std::string> history;
        int histPos;

        int selected;   // entrada seleccionada (-1)
        int hover;      // entrada bajo el mouse (-1)
        int hoverBm;    // bookmark bajo el mouse (-1)
        int selBm;      // bookmark seleccionado (para el boton -)
        bool gridView;

        // --- foco de teclado/keypad (4 OS) -----------------------------------
        // zona con foco + indice dentro de la zona. FZ_NONE = manda el mouse.
        // Las flechas mueven el foco (saltan entre zonas segun la orientacion),
        // Enter/OK lo activa. Mover el mouse vuelve a FZ_NONE.
        enum FocoZona { FZ_NONE = 0, FZ_TOP, FZ_FILES, FZ_BOOKMARKS, FZ_BMBTN, FZ_BOTTOM };
        int focoZona;
        int focoIdx;    // boton dentro de TOP(0-3) / BMBTN(0-1) / BOTTOM(0-1)

        Card* card;       // tarjeta reusable (entries / bookmarks / url)
        ViewportBase* pane; // adaptador para el Scrollable (area de archivos)

        Button* btnBack;
        Button* btnFwd;
        Button* btnUp;
        Button* btnView;
        Button* btnCancel;
        Button* btnAction;  // verde (tinte accent + texto negro), apagado si no hay archivo
        Button* btnBmAdd;   // + : guarda el directorio actual
        Button* btnBmDel;   // - : quita el bookmark seleccionado

        // layout calculado por frame
        bool horizontal;
        int topH, botH;
        int panelX, panelY, panelW, panelH;
        int fileX, fileY, fileW, fileH;
        int cellW, cellH, cols;
        int bmCols;   // columnas de accesos: 1 en horizontal (lista), varias en
                      // vertical (el panel es ancho y bajo -> grilla de chips)

        FileBrowser(const std::string& title, const std::string& accionLabel,
                    const std::string& filtro, void (*accept)(const std::string&));
        ~FileBrowser();

        void Abrir(const std::string& startDir);
        void Navegar(const std::string& dir, bool pushHistory = true);
        void Recargar();
        void Layout();

        void Render();
        bool Click(int mx, int my);
        bool Motion(int mx, int my);
        bool Tecla(int tecla);
        void Wheel(int delta);
        void Soltar();
        void Cerrar();

    private:
        bool seleccionValida() const;
        void Aceptar();
        // navegacion con flechas/OK (foco de teclado, los 4 OS)
        void MoverFoco(int dir);
        void ActivarFoco();
        void LimpiarHoverMouse();
        void AgregarBookmark();
        void QuitarBookmark();
        void Atras();
        void Adelante();
        void Arriba();
        void AbrirEntrada(int idx);
        int  EntryAt(int mx, int my);
        int  BookmarkAt(int mx, int my);
        int  ContentH() const;
        void EnsureVisible(int idx);
        bool PasaFiltro(const w3dFileSystem::DirEntry& e) const;
        // dibuja una tarjeta (Card) en (x,y,w,h) con fondo 'bg' y borde 'bd'
        void TarjetaEn(int x, int y, int w, int h, const float* bg, const float* bd);
};

void AbrirFileBrowser(const std::string& title, const std::string& accionLabel,
                      const std::string& filtro, void (*accept)(const std::string&),
                      bool guardar = false); // guardar=true: elegir carpeta destino

#endif
