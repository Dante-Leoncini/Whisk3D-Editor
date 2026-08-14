#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <string>
#include <vector>
#include "PopUpBase.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/card.h"
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
        bool modoGuardar;          // true = MODO ELEGIR CARPETA/DESTINO: navegas el arbol y
                                   // el boton verde (siempre activo) dice "Usar carpeta actual"
                                   // y devuelve currentPath. Si ademas seleccionaste un archivo
                                   // el boton pasa a la accion del caller y devuelve ESE archivo
                                   // (sobrescribir). El caller arma la ruta con W3dRutaDeSalida.

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
        // texto del boton verde segun el estado (lo usa Render y lo verifica el test
        // 'fbcarpeta'): en modo carpeta sin archivo seleccionado es "Usar carpeta actual".
        std::string EtiquetaAccion() const;
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
        // true si hay un ARCHIVO (no carpeta) seleccionado en la lista
        bool archivoSeleccionado() const;
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

// texto EXACTO del boton verde cuando no hay archivo seleccionado en modo guardar
// (lo comparte el test del harness para no duplicar el literal)
extern const char* const kUsarCarpetaActual;

// ---------------------------------------------------------------------------
//  Arma la RUTA DE SALIDA a partir de lo que devolvio el explorador en modo
//  guardar (ver modoGuardar). Es el unico lugar donde se pega carpeta + nombre:
//    - 'elegido' es una CARPETA  -> carpeta + '/' + nombre (+ ext si le falta)
//    - 'elegido' es un ARCHIVO   -> se respeta (sobrescribir), forzando la ext
//    - 'elegido' vacio           -> carpeta de salida por defecto
//  Normaliza con w3dFileSystem::JoinPath: nunca deja barras duplicadas ni "./".
//  De 'nombre' se toma solo el ultimo tramo (si viniera con carpeta pegada) y si
//  queda vacio se usa 'sin_titulo'. 'ext' incluye el punto (".w3d"); "" = ninguna.
// ---------------------------------------------------------------------------
std::string W3dRutaDeSalida(const std::string& elegido, const std::string& nombre,
                            const std::string& ext);

// Igual pero cuando el caller YA SABE que 'carpeta' es una carpeta (p.ej. el campo
// "Carpeta" de la tarjeta Archivo), exista o no todavia en disco.
// ESTA es la UNICA funcion que pega carpeta + nombre en todo el editor: la usan
// guardar proyecto, guardar version, exportar (.obj/.gltf/.glb) y renderizar.
std::string W3dRutaEnCarpeta(const std::string& carpeta, const std::string& nombre,
                             const std::string& ext);

// Carpeta contra la que se resuelve una carpeta RELATIVA tipeada a mano en un campo
// de salida ("renders", "../entregas"): la del PROYECTO abierto si hay uno, sino la
// carpeta de salida por defecto de la plataforma. NUNCA el CWD del editor (una ruta
// relativa terminaba escribiendo dentro del arbol de Whisk3D).
std::string W3dCarpetaBaseSalida();

#endif
