#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <string>
#include <vector>
#include "PopUpBase.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/ScrollBar.h" // Scrollable: el MISMO scroll que el outliner
#include "w3dFilesystem.h"
#include "WhiskUI/widgets/TextField.h"   // la barra del NOMBRE del archivo (guardar como)

// ============================================================================
//  File browser COMPARTIDO (4 OS) — modal a pantalla completa, .
//  Reusa los elementos que ya existen: Card (9-patch con borde), Button (con
//  icono/tinte) y Scrollable (la barra de scroll real, con su textura).
// ============================================================================
class FileBrowser : public PopUpBase, public Scrollable {
    public:
        void (*onAccept)(const std::string& path);
        std::string actionLabel;
        std::string filterExt;     // extension valida en minuscula ("" = todas)
        bool modeSave;             // true = MODO ELEGIR CARPETA/DESTINO: navegas el arbol y
                                   // el boton verde (siempre activo) dice "Usar carpeta actual"
                                   // y devuelve currentPath. Si ademas seleccionaste un archivo
                                   // el boton pasa a la accion del caller y devuelve ESE archivo
                                   // (sobrescribir). El caller arma la ruta con W3dOutputPath.
        // GUARDAR CON NOMBRE (OpenFileBrowserSaveAs): arriba de los botones, la barra "Nombre de archivo".
        // Aceptar devuelve carpeta + nombre (+ extName si no la tiene); si ese archivo ya existe pregunta
        // "Sobrescribir archivo?" (Cancelar vuelve a la barra para cambiar el nombre)
        bool withName;
        TextField fileName;
        std::string extName;
        bool confirming;               // el cartel de sobrescribir esta abierto
        std::string pathOverwrite;     // el archivo que se pisaria
        Button* btnOverwrite;
        Button* btnNotOverwrite;
        int nameX, nameY, nameW, nameH;   // la caja del nombre (layout)
        bool KeyInText(bool enter);

        // EL PROYECTO POR DENTRO (OpenFileBrowserProject, "Importar..." de la tarjeta Recurso): en vez del disco se
        // navegan las CARPETAS de la biblioteca y se ven los recursos de un tipo. currentPath = la carpeta ("" = la raiz
        // del proyecto). Aceptar devuelve "carpeta/nombre"; si ya hay un recurso con ese nombre, avisa y no cierra
        int projectView;                       // -1 = el disco
        std::vector<std::string> idsProject;   // el id del recurso de cada entrada (su miniatura); "" = carpeta
        std::string Join(const std::string& dir, const std::string& name) const;
        // UNA LIBRERIA POR DENTRO (OpenFileBrowserLib, el proxy W3D): en la raiz sus escenas (se recorren como
        // carpetas: sus objetos, y los que tienen hijos tambien se recorren) y sus prefabs. Aceptar devuelve lo elegido:
        // "prefab:<nombre>", "escena:<nombre>" (la escena entera: aceptar adentro de ella sin nada elegido) o
        // "<escena>#<Padre/Hijo>" (un objeto, con todo lo que cuelga de el)
        std::string libBrowse;               // "" = no es una libreria
        std::vector<int> iconsEntry;         // el icono de cada entrada (-1 = el de siempre)
        std::string PickedLib() const;
        bool Virtual() const { return projectView >= 0 || !libBrowse.empty(); }

        std::string currentPath;
        std::vector<w3dFileSystem::DirEntry> entries;
        std::vector<w3dFileSystem::Bookmark> bookmarks;

        std::vector<std::string> history;
        int histPos;

        int selected;   // entrada seleccionada (-1)
        int hover;      // entrada bajo el mouse (-1)
        int hoverBm;    // bookmark bajo el mouse (-1)
        int selBm;      // bookmark seleccionado (para el boton -)
        // RECIENTES: las ultimas carpetas donde se eligio algo (arriba de los accesos, recent_folders.txt). El panel se
        // reparte entre las dos listas con un divisor que se arrastra (la proporcion se recuerda: W3dFBSplit)
        int hoverRec;   // reciente bajo el mouse (-1)
        bool dragSplit; // arrastrando el divisor recientes / accesos
        bool gridView;

        // --- foco de teclado/keypad (4 OS) -----------------------------------
        // zona con foco + indice dentro de la zona. FZ_NONE = manda el mouse.
        // Las flechas mueven el foco (saltan entre zonas segun la orientacion),
        // Enter/OK lo activa. Mover el mouse vuelve a FZ_NONE.
        enum FocusZone { FZ_NONE = 0, FZ_TOP, FZ_FILES, FZ_BOOKMARKS, FZ_BMBTN, FZ_BOTTOM };
        int focusZone;
        int focusIdx;    // boton dentro de TOP(0-4) / BMBTN(0-1) / BOTTOM(0-1)

        Card* card;       // tarjeta reusable (entries / bookmarks / url)
        ViewportBase* pane; // adaptador para el Scrollable (area de archivos)

        Button* btnBack;
        Button* btnFwd;
        Button* btnUp;
        Button* btnRefresh; // ACTUALIZAR: vuelve a leer la carpeta (cambios de afuera: archivos nuevos, imagenes editadas)
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

        FileBrowser(const std::string& title, const std::string& newActionLabel,
                    const std::string& filter, void (*accept)(const std::string&));
        ~FileBrowser();

        void Open(const std::string& startDir);
        // texto del boton verde segun el estado (lo usa Render y lo verifica el test
        // 'fbcarpeta'): en modo carpeta sin archivo seleccionado es "Usar carpeta actual".
        std::string LabelAction() const;
        void Navigate(const std::string& dir, bool pushHistory = true);
        void Reload();
        // ACTUALIZAR (el boton de la barra): Recargar dejando elegido el MISMO archivo y el scroll donde estaba,
        // y rehaciendo las miniaturas (una imagen editada afuera se ve como quedo)
        void Update();
        void Layout();

        void Render();
        bool Click(int mx, int my);
        bool Motion(int mx, int my);
        bool Key(int key);
        void Wheel(int delta);
        void Release();
        void Close();
        // (el harness) entrar a la carpeta / elegir el archivo que se llama asi, y apretar el boton verde
        bool EnterName(const std::string& n);
        void AcceptNow() { Accept(); }

    private:
        // true si hay un ARCHIVO (no carpeta) seleccionado en la lista
        bool fileSelected() const;
        bool selectionValid() const;
        void Accept();
        // navegacion con flechas/OK (foco de teclado, los 4 OS)
        void MoveFocus(int dir);
        void ActivateFocus();
        void ClearHoverMouse();
        void AddBookmark();
        void RemoveBookmark();
        void Back();
        void Forward();
        void Up();
        void OpenEntry(int idx);
        int  EntryAt(int mx, int my);
        int  BookmarkAt(int mx, int my);
        int  RecentAt(int mx, int my);
        // las dos zonas del panel lateral: recientes [recTop, divY) y accesos [bmTop, bmBottom)
        void PanelZones(int& recTop, int& divY, int& bmTop, int& bmBottom) const;
        int  ContentH() const;
        void EnsureVisible(int idx);
        bool PassesFilter(const w3dFileSystem::DirEntry& e) const;
        // dibuja una tarjeta (Card) en (x,y,w,h) con fondo 'bg' y borde 'bd'
        void CardIn(int x, int y, int w, int h, const float* bg, const float* bd);
};

// las CARPETAS RECIENTES del explorador (la primera, la ultima usada). La suma el explorador al aceptar; abrir y guardar
// un proyecto tambien (su carpeta)
void W3dRecentFolderAdd(const std::string& folderOrFile);
const std::vector<std::string>& W3dRecentFolders();

void OpenFileBrowser(const std::string& title, const std::string& actionLabel,
                      const std::string& filter, void (*accept)(const std::string&),
                      bool save = false,                        // guardar=true: elegir carpeta destino
                      const std::string& folder = std::string()); // donde arranca ("" = W3dFileBrowserStartFolder)
// la carpeta donde arranca el explorador: la del .w3d abierto, si no la ultima reciente, si no el home
std::string W3dFileBrowserStartFolder();
// GUARDAR COMO: el explorador con la barra del NOMBRE del archivo (arranca con 'nombre', sin la extension, en
// 'carpeta' o en la del proyecto). Devuelve la ruta COMPLETA (carpeta + nombre + ext); si ya existe, antes pregunta
void OpenFileBrowserSaveAs(const std::string& title, const std::string& actionLabel, const std::string& ext,
                                 const std::string& name, const std::string& folder, void (*accept)(const std::string&));

// IMPORTAR AL PROYECTO: el explorador del .w3d POR DENTRO (las carpetas de la biblioteca y los recursos del tipo 'vista')
// con la barra del nombre (arranca con 'nombre' en la carpeta 'carpeta' de la biblioteca). Devuelve "carpeta/nombre" (+ ext
// si le falta); "" de carpeta = la raiz
void OpenFileBrowserProject(const std::string& title, const std::string& actionLabel, int view, const std::string& ext,
                              const std::string& name, const std::string& folder, void (*accept)(const std::string&));
// UNA LIBRERIA POR DENTRO (el proxy W3D): ver FileBrowser::libBrowse
void OpenFileBrowserLib(const std::string& title, const std::string& actionLabel, const std::string& lib,
                              void (*accept)(const std::string&));

// texto del boton verde cuando no hay archivo seleccionado en modo guardar, en INGLES: el boton muestra
// T(kUseCurrentFolder) (lo comparte el test del harness para no duplicar el literal)
extern const char* const kUseCurrentFolder;

// ---------------------------------------------------------------------------
//  Arma la RUTA DE SALIDA a partir de lo que devolvio el explorador en modo
//  guardar (ver modeSave). Es el unico lugar donde se pega carpeta + nombre:
//    - 'elegido' es una CARPETA  -> carpeta + '/' + nombre (+ ext si le falta)
//    - 'elegido' es un ARCHIVO   -> se respeta (sobrescribir), forzando la ext
//    - 'elegido' vacio           -> carpeta de salida por defecto
//  Normaliza con w3dFileSystem::JoinPath: nunca deja barras duplicadas ni "./".
//  De 'nombre' se toma solo el ultimo tramo (si viniera con carpeta pegada) y si
//  queda vacio se usa 'sin_titulo'. 'ext' incluye el punto (".w3d"); "" = ninguna.
// ---------------------------------------------------------------------------
std::string W3dOutputPath(const std::string& picked, const std::string& name,
                            const std::string& ext);

// Igual pero cuando el caller YA SABE que 'carpeta' es una carpeta (p.ej. el campo
// "Carpeta" de la tarjeta Archivo), exista o no todavia en disco.
// ESTA es la UNICA funcion que pega carpeta + nombre en todo el editor: la usan
// guardar proyecto, guardar version, exportar (.obj/.gltf/.glb) y renderizar.
std::string W3dPathInFolder(const std::string& folder, const std::string& name,
                             const std::string& ext);

// Carpeta contra la que se resuelve una carpeta RELATIVA tipeada a mano en un campo
// de salida ("renders", "../entregas"): la del PROYECTO abierto si hay uno, sino la
// carpeta de salida por defecto de la plataforma. NUNCA el CWD del editor (una ruta
// relativa terminaba escribiendo dentro del arbol de Whisk3D).
std::string W3dFolderBaseOutput();

#endif
