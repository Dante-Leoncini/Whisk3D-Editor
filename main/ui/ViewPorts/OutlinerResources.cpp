// ============================================================================
//  OutlinerResources.cpp — la BIBLIOTECA del outliner (ver Outliner.h e
//  io/ProjectResources.h): el explorador del contenido del .w3d. UN arbol de
//  carpetas cosmeticas con TODOS los tipos mezclados (mallas 3D, materiales,
//  texturas, sonidos, animaciones, prefabs, escenas y juegos, scripts, fuentes,
//  videos), un FILTRO por tipo, LINEAS GUIA de la jerarquia, lista o CUADRICULA con
//  miniaturas; cada recurso dice si esta EN USO (tilde verde) o HUERFANO (cruz roja)
//  y cuantos lo usan. Tambien las LIBRERIAS externas vinculadas (otros .w3d, de solo
//  lectura) y la barra del outliner (vista, Objeto, Seleccion, "+", filtro).
//
//  UN SOLO MODELO DE FILAS (RowsResources) y UNA SOLA GEOMETRIA (RectRow): el
//  dibujo, el click, el arrastre, el teclado y el harness lo arman igual, asi que no
//  hay recorridos paralelos que se puedan desalinear.
//
//  ENTRADA: mouse (click = elegir; Ctrl/Shift = seleccion multiple; doble click en el
//  nombre = renombrar en linea; flechita = plegar; arrastrar = mover a una carpeta, o
//  soltar AFUERA: en el viewport 3D o sobre un desplegable de Properties; click derecho
//  = el menu Objeto), TACTIL (arrastrar el dedo = scroll; MANTENER ~0,5 s = agarrar la
//  fila -con una marca- y arrastrarla, o soltar sin mover = el menu contextual; doble
//  tap = renombrar), teclado de PC (flechas, Shift+flechas, A, Enter, F2, Supr/X, G =
//  modo mover, Esc) y el keypad del N95 (flechas/OK por LayoutKeyPanelActive, 5
//  marca, 1 mueve, el toque de C borra; la tecla izquierda abre la barra y las flechas
//  recorren sus menus).
//  Motor generico: aca no hay nombres de ningun juego.
// ============================================================================
// (la fila de MEMORIA de la biblioteca: ProcessMemoryKB)
#if defined(_WIN32) && !defined(W3D_SYMBIAN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2   // (K32GetProcessMemoryInfo, en kernel32: sin linkear psapi)
#endif
#include "WhiskUI/text/font.h"   // UTF8_Largo
#include <windows.h>
#include <psapi.h>
#elif defined(W3D_SYMBIAN)
#include <e32std.h>
#include <hal.h>
#endif
#include <cstdio>
#include "w3dGraphics.h"
#ifdef _WIN32
    #include <windows.h>
#endif
#include "Outliner.h"
#include "io/ResourceData.h"    // W3dBytesText (el tamano de cada recurso, la fila de memoria)
#include "w3dTexture.h"         // TexturesMemory (la memoria de video)
#include "LayoutInput.h"                     // LayoutKey, LayoutMenuOpen
#include "LayoutTree.h"                      // gancho de destruccion (el menu recuerda su outliner)
#include "ViewPort3D.h"                      // el arrastre de un recurso al 3D
#include "Properties.h"                      // ...y a un desplegable de Properties
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "render/RenderOptions.h"          // g_redraw
#include "ViewPorts/PopUp/ConfirmPopup.h"  // confirmar un borrado
#include "ViewPorts/PopUp/FileBrowser.h"   // "Link .w3d...": el explorador de archivos de siempre
#include "ViewPorts/PopUp/NumPad.h"   // KeyboardTextOpen / KeyboardNumberOpen: el teclado en pantalla (Ajustes)
#include "ViewPorts/Notifications.h"
#include "config/W3dLang.h"
#include "io/ProjectResources.h"
#include "io/ExternalLibrary.h"            // las librerias externas (vistas de solo lectura)
#include "io/ProjectChanges.h"             // el '*' de lo no guardado
#include "io/Thumbnails.h"                 // la cuadricula: miniaturas perezosas
#include "undo/Undo.h"                     // borrar N partes (clips de una biblioteca) = UN Ctrl+Z
#include "io/EditorRoots.h"                // el selector de escena/juego/prefab y el "+" de la barra
#include "io/PrefabsEditor.h"              // "Create Prefab" / "Unpack Prefab" del menu Objeto
#include "objects/InstancePrefab.h"
#include "objects/ObjectMode.h"              // W3dRenameObject (renombrar en linea en la escena)
#include "W3dRoots.h"
#include "base/W3dInteractionState.h"       // InteractionMode (seleccionar un recurso suelta la escena)
#include <algorithm>
#include <map>
#include <cstdio>
#include <cstring>   // strlen: el cartel de borrar se arma en dos partes
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    extern int W3dScreenHeight;
#else
    #include <GL/gl.h>
#endif

// Properties muestra el recurso elegido (su pestania "Malla 3D"): Properties.cpp
extern void PropsGoToResource();
// los destinos de un arrastre de la biblioteca (ver DropResource.cpp y Properties.cpp): 'onlyTest'
// = solo decir si se puede y que haria ('que', en el idioma de la UI)
extern bool W3dReleaseResourceIn3D(Viewport3D* vp, int mx, int my, int type, const std::string& id, bool onlyTest, std::string* that);
extern bool PropsReleaseResource(Properties* p, int mx, int my, int type, const std::string& id, bool onlyTest, std::string* that);
extern void PropsDestinationsResource(Properties* p, int type, std::vector<int>& rects);   // x, y, w, h por destino

// el rojo de las notificaciones de error (Notifications.cpp): un HUERFANO
static const float kRedOrphan[3] = { 0.92f, 0.28f, 0.24f };

OutDragBib g_outDrag;

// ============================================================================
//  EL MODELO DE FILAS
// ============================================================================
// orden de la lista: sin distinguir mayusculas (ASCII), y a igualdad el texto crudo
static bool LowerText(const std::string& a, const std::string& b) {
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return (unsigned char)ca < (unsigned char)cb;
    }
    if (a.size() != b.size()) return a.size() < b.size();
    return a < b;
}
static bool LowerItem(const W3dResourceItem* a, const W3dResourceItem* b) {
    if (a->name != b->name) return LowerText(a->name, b->name);
    if (a->type != b->type) return a->type < b->type;
    return a->id < b->id;
}
// el ORDEN MANUAL (arrastrar entre dos recursos) manda; lo que no lo tiene va despues, por nombre
static bool LowerItemOrder(const W3dResourceItem* a, const W3dResourceItem* b) {
    const int ra = W3dBibOrder(W3dBibKey(a->type, a->id)), rb = W3dBibOrder(W3dBibKey(b->type, b->id));
    if (ra >= 0 || rb >= 0) {
        if (ra < 0) return false;
        if (rb < 0) return true;
        if (ra != rb) return ra < rb;
    }
    return LowerItem(a, b);
}
static bool LowerLeaf(const std::string& a, const std::string& b) {
    return LowerText(W3dFolderLeaf(a), W3dFolderLeaf(b));
}

// la fila de un recurso
static OutRowRec ItemRow(const W3dResourceItem* it, const std::string& folder, int prof, bool libExt) {
    OutRowRec f;
    f.folder = false;
    f.type = it->type;
    f.id = it->id;
    f.key = W3dBibKey(it->type, it->id);
    f.name = it->name;
    f.folderOf = folder;
    f.prof = prof;
    f.icon = it->icon;
    f.users = it->users;
    f.readOnly  = it->readOnly || libExt;
    f.renamable = it->renamable && !f.readOnly;
    f.part  = !it->parent.empty();
    f.dirty = !libExt && W3dChangesResourceDirty(it->type, it->id);
    f.bytes = it->bytes;
    f.loaded = it->loaded;
    f.check = it->check;
    f.external = !libExt && W3dViewRecLocation(it->type, it->id) == 1;
    return f;
}
// arma las filas de 'carpeta' (sus subcarpetas primero, despues sus recursos) con su sangria. Las PARTES de un
// recurso (W3dResourceItem::padre: los clips de una biblioteca) van debajo de su padre, una sangria mas.
static void BuildRows(const std::string& folder, int prof,
                       std::map<std::string, std::vector<std::string> >& children,
                       std::map<std::string, std::vector<const W3dResourceItem*> >& fromFolder,
                       std::map<std::string, std::vector<const W3dResourceItem*> >& parts,
                       std::map<std::string, int>& count,
                       const std::set<std::string>& collapsed, bool libExt, std::vector<OutRowRec>& out) {
    std::vector<std::string>& hs = children[folder];
    std::sort(hs.begin(), hs.end(), LowerLeaf);
    for (size_t i = 0; i < hs.size(); i++) {
        OutRowRec f;
        f.folder = true;
        f.key = hs[i];
        f.name = W3dFolderLeaf(hs[i]);
        f.folderOf = folder;
        f.prof = prof;
        f.icon = (int)IconType::folder;
        f.users = count[hs[i]];
        f.collapsed = collapsed.count(hs[i]) > 0;
        f.readOnly = libExt;
        out.push_back(f);
        if (!f.collapsed) BuildRows(hs[i], prof + 1, children, fromFolder, parts, count, collapsed, libExt, out);
    }
    std::vector<const W3dResourceItem*>& its = fromFolder[folder];
    std::sort(its.begin(), its.end(), LowerItemOrder);
    for (size_t i = 0; i < its.size(); i++) {
        out.push_back(ItemRow(its[i], folder, prof, libExt));
        std::map<std::string, std::vector<const W3dResourceItem*> >::iterator p =
            parts.find(W3dBibKey(its[i]->type, its[i]->id));
        if (p == parts.end()) continue;
        std::sort(p->second.begin(), p->second.end(), LowerItem);
        for (size_t k = 0; k < p->second.size(); k++) out.push_back(ItemRow(p->second[k], folder, prof + 1, libExt));
    }
}

void Outliner::RowsResources(std::vector<OutRowRec>& out) {
    out.clear();
    if (view == OUT_VIEW_SCENE) return;
    // la ESCENA eligio un objeto despues de lo que se eligio aca (en el 3D, en otro outliner, un script): la
    // biblioteca suelta su seleccion y su cursor. O se eligen objetos o se eligen recursos: la fila no puede
    // seguir en verde como activa ni ser lo que borra un Supr. (En el modo mover lo elegido sigue viajando.)
    if (selSceneSerial != W3dSelectionSerial) {
        selSceneSerial = W3dSelectionSerial;
        if (!movingRec && (SelectionCount() > 0 || !cursorKey.empty())) {
            selResources.clear(); selFolders.clear(); selMarked = false;
            cursorKey.clear(); cursorIsFolder = false;
            g_redraw = true;
        }
    }
    std::vector<W3dResourceItem> items;       // los que se VEN (con el filtro)
    std::vector<std::string> folders;         // TODO el arbol (filtrado, el arbol se sigue viendo entero)
    const bool libExt = ViewReadOnly();
    if (libExt) {
        std::vector<W3dResourceItem> all;
        std::vector<std::string> created;
        W3dExtLibList(view - OUT_VIEW_LIBEXT, all, created, NULL);
        std::set<std::string> cs;
        for (size_t i = 0; i < created.size(); i++)
            for (std::string c = created[i]; !c.empty(); c = W3dFolderParent(c)) cs.insert(c);
        for (size_t i = 0; i < all.size(); i++) {
            for (std::string c = all[i].folder; !c.empty(); c = W3dFolderParent(c)) cs.insert(c);
            if (filter < 0 || all[i].type == filter) items.push_back(all[i]);
        }
        folders.assign(cs.begin(), cs.end());
    } else {
        std::vector<W3dResourceItem> all;
        W3dLibraryList(-1, all);
        W3dFoldersLibrary(all, folders);
        // "Sin uso": los que nadie usa (una escena no se "usa": se abre, no cuenta)
        if (filter < 0 && !onlyUnused) items.swap(all);
        else for (size_t i = 0; i < all.size(); i++) {
            if (filter >= 0 && all[i].type != filter) continue;
            if (onlyUnused && (all[i].users > 0 || all[i].type == W3D_VIEW_SCENES)) continue;
            items.push_back(all[i]);
        }
    }
    std::map<std::string, std::vector<std::string> > children;
    for (size_t i = 0; i < folders.size(); i++) children[W3dFolderParent(folders[i])].push_back(folders[i]);
    std::map<std::string, std::vector<const W3dResourceItem*> > fromFolder;
    std::map<std::string, std::vector<const W3dResourceItem*> > parts;   // clave del padre -> sus partes
    std::set<std::string> keys;
    for (size_t i = 0; i < items.size(); i++) keys.insert(W3dBibKey(items[i].type, items[i].id));
    std::map<std::string, int> count;   // recursos que cuelgan de cada carpeta (recursivo)
    for (size_t i = 0; i < items.size(); i++) {
        const std::string keyParent = items[i].parent.empty() ? std::string() : W3dBibKey(items[i].type, items[i].parent);
        if (!keyParent.empty() && keys.count(keyParent)) { parts[keyParent].push_back(&items[i]); continue; }
        fromFolder[items[i].folder].push_back(&items[i]);
        for (std::string c = items[i].folder; !c.empty(); c = W3dFolderParent(c)) count[c]++;
    }
    static const std::set<std::string> kNone;   // (la cuadricula no pliega: arma todo y se queda con una carpeta)
    BuildRows("", 0, children, fromFolder, parts, count, grid ? kNone : (libExt ? collapsedExt : collapsed), libExt, out);
    if (grid) {
        // UNA carpeta a la vez: sus subcarpetas y sus recursos, como celdas. Arriba de la raiz, "Ir atras"
        if (!folderGrid.empty() && std::find(folders.begin(), folders.end(), folderGrid) == folders.end()) folderGrid.clear();
        std::vector<OutRowRec> inGrid;
        if (!folderGrid.empty()) {
            OutRowRec b;
            b.folder = true; b.back = true;
            b.key = W3dFolderParent(folderGrid);
            b.name = T("Go back");
            b.folderOf = folderGrid;
            b.icon = (int)IconType::folder;
            b.readOnly = libExt;
            inGrid.push_back(b);
        }
        for (size_t i = 0; i < out.size(); i++) {
            if (out[i].part || out[i].folderOf != folderGrid) continue;
            OutRowRec f = out[i]; f.prof = 0;
            inGrid.push_back(f);
        }
        out.swap(inGrid);
    }
    // la SELECCION no puede nombrar lo que ya no esta (purgado, renombrado por un undo...)
    if (!selResources.empty()) {
        for (std::set<std::string>::iterator it = selResources.begin(); it != selResources.end(); )
            if (!keys.count(*it)) selResources.erase(it++); else ++it;
    }
    if (!selFolders.empty()) {
        std::set<std::string> cs(folders.begin(), folders.end());
        for (std::set<std::string>::iterator it = selFolders.begin(); it != selFolders.end(); )
            if (!cs.count(*it)) selFolders.erase(it++); else ++it;
    }
}

// las carpetas plegadas de la vista que se mira
static std::set<std::string>& CollapsedOf(Outliner* o) { return o->ViewReadOnly() ? o->collapsedExt : o->collapsed; }

// ============================================================================
//  LA GEOMETRIA (lista y cuadricula)
// ============================================================================
// CUADRICULA: entrar a una carpeta (un click en ella; "Ir atras" sube)
void Outliner::EnterFolderGrid(const std::string& folder) {
    extern unsigned int w3dGetTicks();
    folderGrid = folder;
    enteredMs = w3dGetTicks();
    if (enteredMs == 0) enteredMs = 1;
    lastContentRows = -1; PosY = 0; g_redraw = true;
}
static bool JustEntered(Outliner* o) {
    extern unsigned int w3dGetTicks();
    if (!o->enteredMs) return false;
    if (w3dGetTicks() - o->enteredMs > 500u) { o->enteredMs = 0; return false; }
    return true;
}
int Outliner::HeaderHeight() const { return (InLibrary() && !grid) ? (int)RowHeightGS : 0; }
int Outliner::TopContent() const { return BarTopOffset() + HeaderHeight(); }
void Outliner::ColumnsList(int& xType, int& xSize, int& xEst, int& xEnd) const {
    const int g = GlobalScale > 0 ? GlobalScale : 1;
    const int reserveBar = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    const bool withState = !ViewReadOnly() && showUses;
    // los anchos de siempre: "3D Mesh" / "123.4 KB" / [carpeta] 999 [tilde]
    const int def[3] = { 10 * CharacterWidthGS + gapGS, 8 * CharacterWidthGS + 2 * gapGS,
                         2 * IconSizeGS + 3 * CharacterWidthGS + 3 * gapGS };
    int w[3];
    for (int k = 0; k < 3; k++) w[k] = colWidth[k] > 0 ? colWidth[k] * g : def[k];
    xEnd  = width - marginGS - borderGS - reserveBar;
    xEst  = withState ? xEnd - w[2] : xEnd;
    xSize = showSizes ? xEst - w[1] : xEst;
    xType = showType ? xSize - w[0] : xSize;
    // (el nombre nunca desaparece del todo: la columna Tipo cede lo que haga falta)
    const int minName = 2 * (IconSizeGS + gapGS) + 4 * CharacterWidthGS;
    if (xType < minName) xType = minName < xSize ? minName : xSize;
}
int Outliner::DivisorInPoint(int mx, int my) const {
    if (!InLibrary() || grid || !Contains(mx, my)) return -1;
    const int ly = my - y;
    if (ly < BarTopOffset() || ly >= height - RowHeightMemory()) return -1;
    int xT, xTm, xE, xF; ColumnsList(xT, xTm, xE, xF);
    const int lx = mx - x, tol = 3 * GlobalScale;
    if (!ViewReadOnly() && showUses && xE < xF && lx >= xE - tol && lx <= xE + tol) return 2;
    if (showSizes && lx >= xTm - tol && lx <= xTm + tol) return 1;
    if (xT < xTm && lx >= xT - tol && lx <= xT + tol) return 0;
    return -1;
}
int Outliner::CellSize() const { return (int)(44.0f * (float)GlobalScale * cellZoom); }

// LISTA: la fila i ocupa una franja de RowHeightGS. CUADRICULA: las carpetas son franjas (con su
// sangria, como en la lista) y los recursos de una misma carpeta fluyen en CELDAS cuadradas (miniatura +
// nombre) de izquierda a derecha; una carpeta nueva (o el fin de la tanda) empieza otra franja.
// Coordenadas del PANEL sin el scroll (quien dibuja o pickea suma PosY; la cuadricula no scrollea en X).
void Outliner::RectRow(const std::vector<OutRowRec>& rows, int i, int& rx, int& ry, int& rw, int& rh) const {
    const int top = borderGS + TopContent();
    const int height = (int)RowHeightGS;
    rx = 0; rw = width; rh = height; ry = top + i * height;
    if (!grid) return;
    // CUADRICULA: celdas que fluyen de izquierda a derecha. Cada una: el CUADRADO (miniatura / icono) y debajo, afuera
    // de el, el nombre y (con "Sizes") el tamanio
    const int cell = CellSize();
    const int cellHeight = cell + gapGS + height * (showSizes ? 2 : 1);
    const int reserve = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    const int limit = width - marginGS - reserve;
    int yy = top, xx = marginGS;
    for (int k = 0; k <= i && k < (int)rows.size(); k++) {
        if (xx + cell > limit && xx > marginGS) { xx = marginGS; yy += cellHeight + gapGS; }
        if (k == i) { rx = xx; ry = yy; rw = cell; rh = cellHeight; return; }
        xx += cell + gapGS;
    }
}
int Outliner::RowInPoint(const std::vector<OutRowRec>& rows, int mx, int my) const {
    // la fila de la MEMORIA (fija, abajo) tapa lo que pasa por debajo: ahi no hay fila que elegir
    if (RowHeightMemory() > 0 && my >= y + height - borderGS - RowHeightMemory()) return -1;
    const int ly = my - y - PosY;
    if (!grid) {
        const int rel = ly - borderGS - TopContent();
        if (rel < 0) return -1;
        const int f = rel / (int)RowHeightGS;
        return (f >= 0 && f < (int)rows.size()) ? f : -1;
    }
    const int lx = mx - x;
    for (size_t i = 0; i < rows.size(); i++) {
        int rx, ry, rw, rh;
        RectRow(rows, (int)i, rx, ry, rw, rh);
        if (lx >= rx && lx < rx + rw && ly >= ry && ly < ry + rh) return (int)i;
    }
    return -1;
}

// ============================================================================
//  LA SELECCION MULTIPLE
// ============================================================================
bool Outliner::RowInSelection(const OutRowRec& f) const {
    return f.folder ? selFolders.count(f.key) > 0 : selResources.count(f.key) > 0;
}
void Outliner::SelectionOnly(const OutRowRec& f) {
    selResources.clear(); selFolders.clear();
    selMarked = false;
    if (f.folder) selFolders.insert(f.key); else selResources.insert(f.key);
    g_redraw = true;
}
void Outliner::SelectionToggle(const OutRowRec& f) {
    // (un click comun deja la fila sola en la seleccion: Ctrl+click en otra = las dos)
    std::set<std::string>& s = f.folder ? selFolders : selResources;
    if (s.count(f.key)) s.erase(f.key); else s.insert(f.key);
    selMarked = true;
    g_redraw = true;
}
bool Outliner::ReleaseSelection() {
    if (!selMarked && SelectionCount() <= 1) return false;
    std::vector<OutRowRec> rows; RowsResources(rows);
    const int fc = CursorRow(rows);
    selResources.clear(); selFolders.clear(); selMarked = false;
    if (fc >= 0) SelectionOnly(rows[(size_t)fc]);   // queda el cursor
    g_redraw = true;
    return true;
}
void Outliner::InvertSelectionRec() {
    std::vector<OutRowRec> rows; RowsResources(rows);
    std::set<std::string> rs, cs;
    for (size_t i = 0; i < rows.size(); i++) {
        if (RowInSelection(rows[i])) continue;
        if (rows[i].folder) cs.insert(rows[i].key); else rs.insert(rows[i].key);
    }
    selResources.swap(rs); selFolders.swap(cs);
    selMarked = true;
    g_redraw = true;
}
bool Outliner::KeyDeleteResources() {
    if (view == OUT_VIEW_SCENE) return false;
    if (!ViewReadOnly()) ActionDelete(true);   // (con confirmacion)
    return true;   // en la biblioteca el C nunca borra objetos de la escena
}
bool Outliner::MarkCursor() {
    if (view == OUT_VIEW_SCENE) return false;
    std::vector<OutRowRec> rows; RowsResources(rows);
    const int fc = CursorRow(rows);
    if (fc < 0) return false;
    // la PRIMERA marca arranca la seleccion con el cursor (la que lo seguia pasa a ser propia);
    // despues, el 5 marca o desmarca la fila del cursor
    if (!selMarked) { SelectionOnly(rows[(size_t)fc]); selMarked = true; }
    else SelectionToggle(rows[(size_t)fc]);
    return true;
}
void Outliner::SelectAllRec() {
    std::vector<OutRowRec> rows; RowsResources(rows);
    bool all = !rows.empty();
    for (size_t i = 0; i < rows.size() && all; i++) all = RowInSelection(rows[i]);
    selResources.clear(); selFolders.clear();
    selMarked = !all;
    if (!all)
        for (size_t i = 0; i < rows.size(); i++) {
            if (rows[i].folder) selFolders.insert(rows[i].key); else selResources.insert(rows[i].key);
        }
    g_redraw = true;
}
void Outliner::GetTarget(std::vector<std::string>& resources, std::vector<std::string>& folders, bool toMove) {
    resources.clear(); folders.clear();
    std::vector<OutRowRec> rows; RowsResources(rows);   // (poda la seleccion)
    std::set<std::string> rs = selResources, cs = selFolders;
    if (rs.empty() && cs.empty()) {
        const int fc = CursorRow(rows);
        if (fc < 0) return;
        if (rows[(size_t)fc].folder) cs.insert(rows[(size_t)fc].key); else rs.insert(rows[(size_t)fc].key);
    }
    if (ViewReadOnly() && toMove) return;   // (una libreria externa no se toca)
    // las carpetas: sin las que cuelgan de otra elegida (se mudan con ella)
    for (std::set<std::string>::iterator it = cs.begin(); it != cs.end(); ++it) {
        bool inside = false;
        for (std::set<std::string>::iterator o = cs.begin(); o != cs.end() && !inside; ++o)
            inside = (*o != *it && W3dFolderInside(*it, *o));
        if (!toMove || !inside) folders.push_back(*it);
    }
    // los recursos: por su clave (tambien los que no se ven: adentro de una carpeta plegada)
    std::vector<W3dResourceItem> items;
    if (!ViewReadOnly()) W3dLibraryList(-1, items);
    else { std::vector<std::string> cr; W3dExtLibList(view - OUT_VIEW_LIBEXT, items, cr, NULL); }
    for (size_t i = 0; i < items.size(); i++) {
        const std::string k = W3dBibKey(items[i].type, items[i].id);
        if (!rs.count(k)) continue;
        if (toMove) {
            if (items[i].readOnly || !items[i].parent.empty()) continue;   // (una parte va con su recurso)
            bool carries = false;   // lo lleva una carpeta elegida
            for (size_t c = 0; c < folders.size() && !carries; c++) carries = W3dFolderInside(items[i].folder, folders[c]);
            if (carries) continue;
        }
        resources.push_back(k);
    }
}

int Outliner::CursorRow(const std::vector<OutRowRec>& rows) const {
    for (size_t i = 0; i < rows.size(); i++)
        if (rows[i].folder == cursorIsFolder && rows[i].key == cursorKey) return (int)i;
    return -1;
}

int Outliner::RowResourceY(int row) const {
    if (!grid) return y + borderGS + PosY + TopContent() + row * (int)RowHeightGS + (int)RowHeightGS / 2;
    std::vector<OutRowRec> rows;
    const_cast<Outliner*>(this)->RowsResources(rows);
    int rx, ry, rw, rh;
    RectRow(rows, row, rx, ry, rw, rh);
    return y + PosY + ry + rh / 2;
}

// ============================================================================
//  LA BARRA: [0] tipo  [1] VISTA (ojo)  [2] OBJETO  [3] SELECCION  [4] "+"  [5] filtro
//            [6] lista/cuadricula  [7] raiz (en la vista Escena)
// ============================================================================
std::string Outliner::ViewKey() const {
    if (view == OUT_VIEW_SCENE) return "scene";
    if (view == OUT_VIEW_LIBRARY) return "library";
    return "lib:" + W3dLibsDiskPath(view - OUT_VIEW_LIBEXT);
}
bool Outliner::ViewByKey(const std::string& c) {
    if (c == "scene") { ChangeView(OUT_VIEW_SCENE); return true; }
    if (c == "library") { ChangeView(OUT_VIEW_LIBRARY); return true; }
    if (c.compare(0, 4, "lib:") == 0) {
        for (int i = 0; i < W3dLibsCount(); i++)
            if (W3dLibsDiskPath(i) == c.substr(4)) { ChangeView(OUT_VIEW_LIBEXT + i); return true; }
        return false;
    }
    // el layout de la fase anterior ("vista: materiales"): la biblioteca con ese filtro
    const int t = W3dViewByKey(c);
    if (t > W3D_VIEW_SCENE) { ChangeView(OUT_VIEW_LIBRARY); ChangeFilter(t); return true; }
    return false;
}

static std::string TitleView(int v, bool inLibrary) {
    if (v == OUT_VIEW_SCENE) return T("Scene");
    if (v == OUT_VIEW_LIBRARY) return inLibrary ? T("Project") : T("Library");   // (el viewport ya es "Library")
    return W3dLibsRow(v - OUT_VIEW_LIBEXT).name;
}

Library::Library() {
    view = OUT_VIEW_LIBRARY;
    grid = true;   // (la biblioteca arranca en cuadricula: las miniaturas)
    SyncBarView();
}

void Outliner::SyncBarView() {
    if (!btnView) return;
    // una libreria que se desvinculo (o un undo) mientras se la miraba: vuelve a la biblioteca
    if (view >= OUT_VIEW_LIBEXT && view - OUT_VIEW_LIBEXT >= W3dLibsCount()) {
        view = OUT_VIEW_LIBRARY;
        collapsedExt.clear();
        lastContentRows = -1;
    }
    const bool isBib = (ViewportKind() == 9);
    // el selector de la VISTA es de la BIBLIOTECA (el proyecto / cada libreria vinculada / vincular una): el outliner
    // de la escena no lo tiene (ahi va el selector de la escena que se edita)
    { const bool show = isBib || InLibrary(); if (btnView->visible != show) { btnView->visible = show; g_redraw = true; } }
    const std::string t = TitleView(view, isBib);
    if (btnView->text != t) { btnView->text = t; g_redraw = true; }
    // QUE se ve: la camara (la escena), la carpeta (la biblioteca) o la libreria externa
    btnView->icon = (view == OUT_VIEW_SCENE) ? (int)IconType::camera
                   : (view == OUT_VIEW_LIBRARY) ? (int)IconType::folder : (int)IconType::lib;
    if (btnOptions) btnOptions->icon = (int)IconType::visible;   // el OJO: COMO se ve
    // (solo iconos, como la barra del 3D: el menu abierto lleva su titulo y el cartelito de ayuda dice que es)
    if (btnObject) { btnObject->text.clear(); btnObject->icon = (int)IconType::object; btnObject->help = "Element"; }
    if (btnSelection) { btnSelection->text.clear(); btnSelection->icon = (int)IconType::selection; btnSelection->help = "Select"; }
    const bool bib = InLibrary();
    // el "+" (nueva carpeta, nuevo material, nueva escena / juego / prefab) es de la biblioteca: en la escena no va
    if (newBtn && newBtn->visible != bib) { newBtn->visible = bib; g_redraw = true; }
    if (btnFilter) {
        if (btnFilter->visible != bib) { btnFilter->visible = bib; g_redraw = true; }
        // solo el icono: el embudo (todo) o el del tipo filtrado
        if (!btnFilter->text.empty()) { btnFilter->text.clear(); g_redraw = true; }
        btnFilter->icon = filter >= 0 ? W3dIconView(filter) : onlyUnused ? (int)IconType::users : (int)IconType::filter;
        btnFilter->help = "Filter";
    }
    if (btnGrid) {
        // (lista / cuadricula paso al OJO: el interruptor suelto ya no va en la barra)
        if (btnGrid->visible) { btnGrid->visible = false; g_redraw = true; }
        // el icono dice a que se PASA con el click (como un interruptor): en lista, la cuadricula
        btnGrid->icon = grid ? (int)IconType::list : (int)IconType::grid;
    }
    if (btnRoot) {
        const bool show = !bib;
        if (btnRoot->visible != show) { btnRoot->visible = show; g_redraw = true; }
        // la raiz que se edita (escena o prefab): su icono y su nombre, como en la barra del 3D ('*' = sin guardar)
        W3dRootsButtonSync(btnRoot);
    }
}

void Outliner::ChangeView(int v) {
    if (v < OUT_VIEW_SCENE || v >= OUT_VIEW_LIBEXT + W3dLibsCount()) return;
    if (moving) MoveCancel();                   // el modo mover es de la vista que se deja
    if (movingRec) MoveRecCancel();
    if (renaming) RenameSync();
    if (v != view && v >= OUT_VIEW_LIBEXT) collapsedExt.clear();
    view = v;
    PosY = 0; PosX = 0;
    hoverRow = -1;
    recDrag    = recDragging = recDragFolder = recOnlyOnRelease = false;
    recDropRow = -1; recInsRow = -1; g_outDrag.ghost = false;
    grabRow    = -1;
    dragObject = NULL; dragging = false; dropZone = -2;
    cursorIsFolder = false;
    cursorKey.clear();
    selResources.clear(); selFolders.clear();   // la seleccion es de la vista que se deja
    selMarked = false;
    lastContentRows = -1;                            // el proximo Render rearma el scroll
    SyncBarView();
    if (width > 0 && height > 0) Resize(width, height);
    g_redraw = true;
}
void Outliner::ChangeFilter(int f) {
    if (f != -1 && (f <= W3D_VIEW_SCENE || f >= W3D_VIEWS)) return;
    if (movingRec) MoveRecCancel();
    filter = f;
    PosY = 0;
    lastContentRows = -1;
    SyncBarView();
    if (width > 0 && height > 0) Resize(width, height);
    g_redraw = true;
}
void Outliner::ChangeUnused(bool on) {
    if (onlyUnused == on) return;
    if (movingRec) MoveRecCancel();
    onlyUnused = on;
    PosY = 0;
    lastContentRows = -1;
    SyncBarView();
    if (width > 0 && height > 0) Resize(width, height);
    g_redraw = true;
}
void Outliner::ChangeGrid(bool on) {
    if (grid == on) return;
    grid = on;
    PosY = 0; PosX = 0;
    lastContentRows = -1;
    SyncBarView();
    if (width > 0 && height > 0) Resize(width, height);
    g_redraw = true;
}

// ---- los menus (uno de cada, compartidos: los arma el outliner que los abre) ----
static PopupMenu* gMenuView = NULL;
static PopupMenu* gMenuObject = NULL;
static PopupMenu* gMenuSelection = NULL;
static PopupMenu* gMenuMove = NULL;
static PopupMenu* gMenuRoot = NULL;                 // el selector de escena/juego/prefab ([7])
static PopupMenu* gNewMenu = NULL;                  // el "+": nueva escena / juego / prefab / carpeta / material
static PopupMenu* gMenuFilter = NULL;
static PopupMenu* gMenuUnlink = NULL;              // submenu de la vista: las librerias a desvincular
static Outliner*  gMenuOf = NULL;                  // el outliner que abrio el menu
static std::vector<std::string> gMoveFolders;      // id del menu Mover - 101 -> carpeta

// el outliner que abrio un menu se borro (cambio de tipo, abrir un proyecto): se olvida
static void ForgetOutliner(ViewportBase* dead) {
    if ((ViewportBase*)gMenuOf == dead) {
        gMenuOf = NULL;
        if (OpenMenu && (OpenMenu == gMenuView || OpenMenu == gMenuObject || OpenMenu == gMenuMove ||
                            OpenMenu == gMenuRoot || OpenMenu == gNewMenu || OpenMenu == gMenuSelection ||
                            OpenMenu == gMenuFilter || OpenMenu == gMenuUnlink))
            OpenMenu->Close();
    }
}
static bool gHookOk = ViewportForgetRegister(ForgetOutliner);

// ---- VISTA: Escena / Biblioteca / cada libreria + vincular / desvincular ----
enum { VIS_LINK = 900, VIS_GRID = 901, VIS_UNLINK = 950 };
// filtros de la vista Escena (checks: el menu queda abierto; la accion solo redibuja)
enum { VIS_SHOW_CONSTRUCTOR = 880, VIS_SHOW_ROUTINES, VIS_SHOW_EYES, VIS_SHOW_RENDER };
extern bool g_outShowConstructor, g_outShowRoutines, g_outShowEyes, g_outShowRender;
static void PickedLib(const std::string& path) {
    std::string reason;
    if (!W3dExtLibLink(path, &reason)) { Notify(std::string(T("Not linked")) + ": " + T(reason.c_str()), true); return; }
    Notify(std::string(T("Library linked")) + ": " + path, false);
    if (gMenuOf) gMenuOf->ChangeView(OUT_VIEW_LIBEXT + W3dLibsCount() - 1);
    g_redraw = true;
}
// DESVINCULAR (con Ctrl+Z). Lo que el proyecto usaba de ella no queda colgando (W3dExtLibUnlink)
static int gPendingDesv = -1;   // la que espera la confirmacion (ConfirmPopup llama un void() sin argumentos)
static void UnlinkNow(int i) {
    const std::string n = W3dLibsRow(i).name;
    int copies = 0;
    if (!W3dExtLibUnlink(i, &copies)) return;
    std::string notice = std::string(T("Library unlinked")) + ": " + n;
    if (copies > 0) {
        char b[160];
        snprintf(b, sizeof(b), T("%d object(s) got their own copy of the mesh"), copies);
        notice += std::string(" (") + b + ")";
    }
    Notify(notice, false);
    if (gMenuOf && gMenuOf->view >= OUT_VIEW_LIBEXT) gMenuOf->ChangeView(OUT_VIEW_LIBRARY);
    g_redraw = true;
}
static void UnlinkConfirmed() {
    const int i = gPendingDesv;
    gPendingDesv = -1;
    if (i >= 0 && i < W3dLibsCount()) UnlinkNow(i);
}
static void ActionPickedView(int id) {
    Outliner* o = gMenuOf;
    if (!o) return;
    if (id == VIS_LINK) {
        // el EXPLORADOR de archivos de siempre, filtrado a .w3d
        OpenFileBrowser(T("Link .w3d..."), T("Link"), ".w3d", PickedLib, false);
        return;
    }
    if (id == VIS_GRID) { o->ChangeGrid(!o->grid); return; }   // (el teclado del N95)
    if (id >= VIS_SHOW_CONSTRUCTOR && id <= VIS_SHOW_RENDER) { g_redraw = true; return; }   // (el check ya cambio el filtro)
    if (id >= VIS_UNLINK) {
        const int i = id - VIS_UNLINK;
        // el PROYECTO usa mallas suyas (soltadas desde su vista): se avisa ANTES y se ofrece (Si = cada una pasa a
        // su copia propia, con Ctrl+Z; No = no se desvincula). Sin usos se desvincula directo
        const int uses = W3dExtLibProjectUses(i);
        if (uses > 0) {
            gPendingDesv = i;
            if (!confirmPopup) confirmPopup = new ConfirmPopup();
            char b[512];
            snprintf(b, sizeof(b), T("%d object(s) of the project use 3D meshes of \"%s\": each one gets its own copy (Ctrl+Z undoes it). Unlink it?"),
                     uses, W3dLibsRow(i).name.c_str());
            confirmPopup->Open(b, UnlinkConfirmed);
            return;
        }
        UnlinkNow(i);
        return;
    }
    o->ChangeView(id);
}
// ---- el OJO de la barra: COMO se ve lo que se ve. Biblioteca: lista / cuadricula, la fila de memoria y los tamanos;
//      escena: el Constructor, las rutinas y las columnas de ojo y camara ----
static PopupMenu* gMenuOptions = NULL;
enum { OPT_LIST = 870, OPT_GRID, OPT_MEMORY, OPT_SIZES, OPT_TYPE, OPT_USES, OPT_ZOOM };
static float gZoomPct = 100.0f;   // (la fila Zoom del menu: el tamanio de las celdas en %)
void OutlinerZoomCells(Outliner* o, float zoom);   // (Outliner.cpp)
static void ActionPickedOption(int id) {
    Outliner* o = gMenuOf;
    if (!o) return;
    if (id == OPT_LIST)      { o->ChangeGrid(false); return; }
    if (id == OPT_GRID) { o->ChangeGrid(true); return; }
    if (id == OPT_ZOOM)       { OutlinerZoomCells(o, gZoomPct * 0.01f); return; }
    o->lastContentRows = -1;   // (la fila de memoria y la columna del tamano cambian lo que hay que scrollear)
    o->Resize(o->width, o->height);
    g_redraw = true;   // (los checks ya cambiaron su bool)
}
static void BuildMenuOptions(Outliner* o) {
    if (!gMenuOptions) { gMenuOptions = new PopupMenu(); gMenuOptions->action = ActionPickedOption; }
    gMenuOptions->Clear();
    gMenuOptions->title = T("View");
    if (o->InLibrary()) {
        MenuItem* it = gMenuOptions->Add(T("List"), OPT_LIST, IconType::list);
        if (it) it->green = !o->grid;
        it = gMenuOptions->Add(T("Grid"), OPT_GRID, IconType::grid);
        if (it) it->green = o->grid;
        // EL ZOOM de la cuadricula, con numeros: - y + de a 10% (o arrastrando la barra). Tambien Ctrl + rueda y dos dedos
        if (o->grid) {
            gZoomPct = (float)(int)(o->cellZoom * 100.0f + 0.5f);
            it = gMenuOptions->AddFloat(T("Zoom"), OPT_ZOOM, &gZoomPct, 60.0f, 400.0f);
            if (it) { it->step = 10.0f; it->integer = true; it->unit = "%"; }
        }
        gMenuOptions->AddCheck(T("Memory usage"), OPT_MEMORY, &o->showMemory);
        // las COLUMNAS (en la lista; en la cuadricula, el tamanio va debajo del nombre y los usos en el icono)
        gMenuOptions->AddCheck(T("Type"), OPT_TYPE, &o->showType);
        gMenuOptions->AddCheck(T("Sizes"), OPT_SIZES, &o->showSizes);
        if (!o->ViewReadOnly()) gMenuOptions->AddCheck(T("Users"), OPT_USES, &o->showUses);
    } else {
        gMenuOptions->AddCheck(T("Constructor"), VIS_SHOW_CONSTRUCTOR, &g_outShowConstructor);
        gMenuOptions->AddCheck(T("Routines"), VIS_SHOW_ROUTINES, &g_outShowRoutines);
        gMenuOptions->AddCheck(T("Visibility icons"), VIS_SHOW_EYES, &g_outShowEyes, (int)IconType::visible);
        gMenuOptions->AddCheck(T("Render icons"), VIS_SHOW_RENDER, &g_outShowRender, (int)IconType::camera);
    }
}

static void BuildMenuView(Outliner* o) {
    if (!gMenuView) { gMenuView = new PopupMenu(); gMenuView->action = ActionPickedView; }
    if (!gMenuUnlink) { gMenuUnlink = new PopupMenu(); gMenuUnlink->action = ActionPickedView; }
    gMenuView->Clear();
    const bool isBib = (o->ViewportKind() == 9);
    MenuItem* it = NULL;
    if (!isBib) {   // (un outliner que el harness paso a los recursos: puede volver a la escena)
        it = gMenuView->Add(T("Scene"), OUT_VIEW_SCENE, W3dRootIcon(W3dRootActiveType()));
        if (it) it->green = (o->view == OUT_VIEW_SCENE);
    }
    it = gMenuView->Add(TitleView(OUT_VIEW_LIBRARY, isBib), OUT_VIEW_LIBRARY, IconType::folder);
    if (it) it->green = (o->view == OUT_VIEW_LIBRARY);
    gMenuUnlink->Clear();
    for (int i = 0; i < W3dLibsCount(); i++) {
        it = gMenuView->Add(W3dLibsRow(i).name, OUT_VIEW_LIBEXT + i, IconType::lib);
        if (it) it->green = (o->view == OUT_VIEW_LIBEXT + i);
        gMenuUnlink->Add(W3dLibsRow(i).name, VIS_UNLINK + i, IconType::lib);
    }
    gMenuView->Add(T("Link .w3d..."), VIS_LINK, IconType::plus);
    if (W3dLibsCount() > 0) gMenuView->Add(T("Unlink"), 0, IconType::trash, gMenuUnlink);
}

// ---- OBJETO (Escena: el objeto activo / la seleccion; Biblioteca: el cursor / la seleccion) ----
enum { OBJ_DELETE = 1, OBJ_RENAME, OBJ_MOVE, OBJ_HIDE, OBJ_DUPLICATE, OBJ_DUPLICATE_VINC,
       REC_DELETE, REC_RENAME, REC_DUPLICATE, REC_USERS, REC_PURGE, REC_NEW_FOLDER,
       OBJ_CREATE_PREFAB, OBJ_UNPACK, REC_INSIDE, REC_OUTSIDE, REC_UPDATE_TEX };
// "New Folder" del CLICK DERECHO: en la carpeta de lo que estaba abajo del mouse (una carpeta: adentro; un recurso: al
// lado; el hueco: la raiz)
static bool gNewFolderCtx = false;
static std::string gNewFolderParent;
// las TEXTURAS elegidas (el cursor o la seleccion): las que "Reload Texture" vuelve a leer de su archivo
static void TexturesPicked(Outliner* o, std::vector<std::string>& out) {
    out.clear();
    std::vector<std::string> rec, ch;
    o->GetTarget(rec, ch, false);
    for (size_t i = 0; i < rec.size(); i++) {
        int type = -1; std::string id;
        if (W3dBibByKey(rec[i], &type, &id) && type == W3D_VIEW_TEXTURES) out.push_back(id);
    }
}
// DENTRO / FUERA del .w3d desde la biblioteca: los recursos elegidos (el cursor o la seleccion) que tienen archivo
// propio (texturas, sonidos, scripts, fuentes, videos, shaders) y que todavia NO estan donde se los quiere llevar
// (0 = adentro, 1 = afuera). Siguen en la MISMA carpeta de la biblioteca (la carpeta viaja con la ruta nueva)
static void PlaceablePicked(Outliner* o, int destination, std::vector<std::pair<int, std::string> >& out) {
    out.clear();
    if (!o || o->ViewReadOnly()) return;
    std::vector<std::string> rec, ch;
    o->GetTarget(rec, ch, false);
    if (rec.empty()) return;
    std::set<std::string> rs(rec.begin(), rec.end());
    std::vector<W3dResourceItem> items;
    W3dLibraryList(-1, items);
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i].readOnly || !rs.count(W3dBibKey(items[i].type, items[i].id))) continue;
        const int u = W3dViewRecLocation(items[i].type, items[i].id);
        if (u >= 0 && u != destination) out.push_back(std::make_pair(items[i].type, items[i].id));
    }
}
// lleva 'ls' a 'destino' (afuera: 'carpeta' elegida en el explorador; vacia = al lado del .w3d). UN Ctrl+Z para todo
static void MovePlaceable(const std::vector<std::pair<int, std::string> >& ls, int destination, const std::string& folder) {
    if (ls.empty()) return;
    int done = 0;
    std::string last, error;
    UndoGroupStart();
    for (size_t i = 0; i < ls.size(); i++) {
        std::string path;
        if (destination == 1 && !folder.empty()) {
            // el archivo se llama como su entrada ("textures/piso.png" -> <carpeta elegida>/piso.png)
            std::string base = ls[i].second, ext;
            const size_t b = base.find_last_of("/\\");
            if (b != std::string::npos) base = base.substr(b + 1);
            const size_t p = base.rfind('.');
            if (p != std::string::npos && p > 0) { ext = base.substr(p); base = base.substr(0, p); }
            path = W3dOutputPath(folder, base, ext);
        } else if (destination == 1) path = W3dViewRecPathExternalSuggested(ls[i].first, ls[i].second);
        std::string newValue, reason;
        if (W3dViewRecSetLocation(ls[i].first, ls[i].second, destination, path, &newValue, &reason)) { done++; last = newValue; }
        else if (error.empty()) error = ls[i].second + ": " + T(reason.c_str());
    }
    UndoGroupEnd();
    if (!error.empty()) Notify(std::string(T("Not moved")) + ": " + error, true);
    else if (done == 1) Notify(std::string(T(destination == 1 ? "Now external" : "Now inside the .w3d")) + ": " + last, false);
    else if (done > 1) {
        char b[160];
        snprintf(b, sizeof(b), T(destination == 1 ? "%d files are now external" : "%d files are now inside the .w3d"), done);
        Notify(b, false);
    }
    if (done > 0) { W3dResourcesViewInvalidate(); g_redraw = true; }
}
// "Extract to a folder...": lo elegido al ABRIR el explorador (mientras esta abierto se puede tocar otra cosa)
static std::vector<std::pair<int, std::string> > gOutsidePending;
static void OutsidePickedFolder(const std::string& picked) {
    const std::vector<std::pair<int, std::string> > ls = gOutsidePending;
    gOutsidePending.clear();
    MovePlaceable(ls, 1, picked);
}
void OutlinerRecOutsidePicked(const std::string& folder) { OutsidePickedFolder(folder); }   // (harness)
static void ActionMovePicked(int id) {
    if (!gMenuOf) return;
    if (id == 100) gMenuOf->ActionMove(std::string());
    else if (id > 100 && id - 101 < (int)gMoveFolders.size()) gMenuOf->ActionMove(gMoveFolders[(size_t)(id - 101)]);
}
// el Aceptar del modal de mover que un duplicado deja abierto (desde el outliner no hay mouse en el 3D)
static void AcceptDuplicateModal() {
    extern Viewport3D* Viewport3DActive;
    if (state == editNavigation) return;
    if (Viewport3DActive) Viewport3DActive->Accept();
    else state = editNavigation;
}
// Shift+D / Alt+D desde el outliner (el menu Objeto y sus atajos con el mouse sobre el panel)
void Outliner::DuplicateScene(bool linked) {
    extern void DuplicatedObject();
    extern void NewInstance();
    if (linked) NewInstance(); else DuplicatedObject();
    AcceptDuplicateModal();
    g_redraw = true;
}
static void ActionPickedObject(int id) {
    Outliner* o = gMenuOf;
    if (!o) return;
    switch (id) {
        case OBJ_DELETE:   OpenConfirmDelete(true); break;
        case OBJ_RENAME: o->ActionRename(); break;
        case OBJ_MOVE:    o->MoveStart(); break;
        case OBJ_HIDE:  UndoCaptureVisibility(); ChangeVisibilityObj(); break;
        case OBJ_DUPLICATE: o->DuplicateScene(false); break;
        case OBJ_DUPLICATE_VINC: o->DuplicateScene(true); break;
        case OBJ_CREATE_PREFAB: { std::string name, reason;   // lo elegido pasa a un prefab nuevo (queda una instancia)
            if (!W3dPrefabCreateFromSelection(&name, &reason)) Notify(std::string(T(reason.c_str())), true);
            else Notify(std::string(T("Prefab created")) + ": " + name, false);
            break; }
        case OBJ_UNPACK: { std::string reason;          // la instancia activa pasa a objetos comunes
            InstancePrefab* ip = (ActiveObj && ActiveObj->getType() == ObjectType::prefab) ? (InstancePrefab*)ActiveObj : NULL;
            if (!ip) Notify(std::string(T("Select a prefab instance")), true);
            else if (!W3dPrefabUnpack(ip, &reason)) Notify(std::string(T(reason.c_str())), true);
            break; }
        case REC_DELETE:   o->ActionDelete(true); break;
        case REC_RENAME: o->ActionRename(); break;
        case REC_DUPLICATE: o->ActionDuplicateResource(); break;
        case REC_USERS: o->ActionSelectUsers(); break;
        case REC_PURGE:   o->ActionPurge(true); break;
        case REC_NEW_FOLDER: if (gNewFolderCtx) o->ActionNewFolderIn(gNewFolderParent); else o->ActionNewFolder(); break;
        case REC_UPDATE_TEX: { std::vector<std::string> ts; TexturesPicked(o, ts); W3dTexturesUpdateNotifying(ts); break; }
        case REC_INSIDE: { std::vector<std::pair<int, std::string> > ls; PlaceablePicked(o, 0, ls); MovePlaceable(ls, 0, std::string()); break; }
        case REC_OUTSIDE:
            PlaceablePicked(o, 1, gOutsidePending);
            if (!gOutsidePending.empty())
                OpenFileBrowser(T("Extract to a folder..."), T("Save"), "", OutsidePickedFolder, true);
            break;
        default: ActionMovePicked(id); break;   // el submenu "Move to Folder"
    }
    g_redraw = true;
}

// arma el submenu con las carpetas de la biblioteca ("(root)" + cada carpeta, con su sangria). Las
// carpetas que se MUEVEN (y lo que cuelga de ellas) no se ofrecen: no hay como meterlas en si mismas.
// 'actual' = la carpeta de donde sale (tilde verde) si es una sola.
static void BuildMenuMove(PopupMenu* m, const std::string& current, const std::vector<std::string>& move) {
    m->Clear();
    m->title.clear();   // como submenu no lleva cabecera (OpenMenuMove se la pone suelto)
    gMoveFolders.clear();
    MenuItem* root = m->Add(std::string("(") + T("root") + ")", 100, IconType::folder);
    if (root) root->green = current.empty();
    std::vector<std::string> cs;
    W3dFoldersAll(cs);
    for (size_t i = 0; i < cs.size(); i++) {
        bool outside = false;
        for (size_t k = 0; k < move.size() && !outside; k++) outside = W3dFolderInside(cs[i], move[k]);
        if (outside) continue;
        const int id = 101 + (int)gMoveFolders.size();
        gMoveFolders.push_back(cs[i]);
        int prof = 0;
        for (size_t k = 0; k < cs[i].size(); k++) if (cs[i][k] == '/') prof++;
        MenuItem* it = m->Add(std::string((size_t)prof * 2, ' ') + W3dFolderLeaf(cs[i]), id, IconType::folder);
        if (it) it->green = (cs[i] == current);
    }
}
// la carpeta de donde sale lo elegido (para el tilde): la de todos si es la misma, "\x01" si no
static std::string OriginFolder(const std::vector<std::string>& resources, const std::vector<std::string>& folders) {
    std::string c; bool has = false;
    for (size_t k = 0; k < resources.size(); k++) {
        int t = 0; std::string id; W3dResourceItem it;
        if (!W3dBibByKey(resources[k], &t, &id) || !W3dViewRecInfo(t, id, &it)) continue;
        if (has && c != it.folder) return std::string("\x01");
        c = it.folder; has = true;
    }
    for (size_t k = 0; k < folders.size(); k++) {
        const std::string p = W3dFolderParent(folders[k]);
        if (has && c != p) return std::string("\x01");
        c = p; has = true;
    }
    return c;
}

// abre un menu justo debajo de un boton de la barra (como los de la barra del 3D)
static void OpenUnder(PopupMenu* m, Button* b) {
    if (OpenMenu && OpenMenu != m) OpenMenu->Close();
    m->Open(b->sx, b->sy + b->height - GlobalScale, MenuScreenW, MenuScreenH);
    OpenMenu = m;
}
static MenuItem* WithShortcut(MenuItem* it, const char* shortcut) { if (it) it->shortcut = shortcut; return it; }
// un item DESHABILITADO (gris, no responde): el menu mira un bool que en false = gris
static void Gray(MenuItem* it, bool disabled) { static bool kEnabled = false; if (it && disabled) it->gray = &kEnabled; }

// el menu OBJETO segun la vista, el cursor y la seleccion (lo que esta resaltado)
static void BuildMenuObject(Outliner* o) {
    if (!gMenuObject) { gMenuObject = new PopupMenu(); gMenuObject->action = ActionPickedObject; }
    if (!gMenuMove) { gMenuMove = new PopupMenu(); gMenuMove->action = ActionMovePicked; }
    gMenuObject->Clear();
    gMenuObject->title = T("Element");   // (el boton es solo un icono: el menu dice que es)
    if (o->view == OUT_VIEW_SCENE) {
        const bool has = (ActiveObj != NULL) || !ObjSelects.empty();
        MenuItem* it;
        it = WithShortcut(gMenuObject->Add(T("Delete"), OBJ_DELETE, IconType::trash), "@out.borrar");
        Gray(it, !has);
        it = WithShortcut(gMenuObject->Add(T("Rename"), OBJ_RENAME, IconType::list), "@out.renombrar");
        Gray(it, (ActiveObj == NULL));
        it = WithShortcut(gMenuObject->Add(T("Move"), OBJ_MOVE, IconType::arrowRight), "@out.mover");
        Gray(it, (ActiveObj == NULL));
        it = WithShortcut(gMenuObject->Add(T("Hide/Show"), OBJ_HIDE, IconType::visible), "@out.ocultar");
        Gray(it, !has);
        it = WithShortcut(gMenuObject->Add(T("Duplicate"), OBJ_DUPLICATE, IconType::object), "@out.duplicar");
        Gray(it, !has);
        it = WithShortcut(gMenuObject->Add(T("Duplicate Linked"), OBJ_DUPLICATE_VINC, IconType::instance), "@out.instancia");
        Gray(it, !has);
        // PREFABS: lo elegido pasa a un prefab nuevo / la instancia activa se desempaqueta
        it = gMenuObject->Add(T("Create Prefab"), OBJ_CREATE_PREFAB, IconType::prefab);
        Gray(it, !has);
        it = gMenuObject->Add(T("Unpack Prefab"), OBJ_UNPACK, IconType::prefab);
        Gray(it, !(ActiveObj && ActiveObj->getType() == ObjectType::prefab));
        return;
    }
    std::vector<OutRowRec> rows;
    o->RowsResources(rows);
    const int fc = o->CursorRow(rows);
    const OutRowRec* f = (fc >= 0) ? &rows[(size_t)fc] : NULL;
    std::vector<std::string> recM, charM, rec, ch;
    o->GetTarget(recM, charM, true);
    o->GetTarget(rec, ch, false);
    const bool ro = o->ViewReadOnly();
    MenuItem* it;
    it = WithShortcut(gMenuObject->Add(T("Delete"), REC_DELETE, IconType::trash), "@out.borrar");
    Gray(it, ro || (rec.empty() && ch.empty()));
    it = WithShortcut(gMenuObject->Add(T("Rename"), REC_RENAME, IconType::list), "@out.renombrar");
    Gray(it, ro || !f || (!f->folder && !f->renamable));
    if (!ro && (!recM.empty() || !charM.empty())) {
        BuildMenuMove(gMenuMove, OriginFolder(recM, charM), charM);
        WithShortcut(gMenuObject->Add(T("Move to Folder"), 0, IconType::folder, gMenuMove), "@out.mover");
    } else {
        it = WithShortcut(gMenuObject->Add(T("Move to Folder"), 0, IconType::folder), "@out.mover");
        Gray(it, true);
    }
    it = gMenuObject->Add(T("Duplicate Resource"), REC_DUPLICATE, IconType::object);
    Gray(it, ro || !f || f->folder || f->part);
    // (solo si alguno de sus usuarios es un OBJETO: una malla huerfana, un script o un flipbook
    // cuentan como usuarios, pero no hay nada que seleccionar)
    it = gMenuObject->Add(T("Select Users"), REC_USERS, IconType::selection);
    Gray(it, ro || rec.empty() || W3dBibObjectsUsers(rec) == 0);
    it = gMenuObject->Add(T("Purge Orphans"), REC_PURGE, IconType::notifError);
    Gray(it, ro);
    // ACTUALIZAR TEXTURA: solo si entre lo elegido hay texturas (se editaron afuera: se vuelven a leer de su archivo)
    { std::vector<std::string> ts; if (!ro) TexturesPicked(o, ts);
      if (!ts.empty()) gMenuObject->Add(T("Reload Texture"), REC_UPDATE_TEX, IconType::reload); }
    // DENTRO / FUERA del .w3d: solo si lo elegido tiene archivo propio (los externos llevan la flecha de acceso directo)
    std::vector<std::pair<int, std::string> > inside, outside;
    PlaceablePicked(o, 0, inside);
    PlaceablePicked(o, 1, outside);
    if (!inside.empty() || !outside.empty()) {
        it = gMenuObject->Add(T("Pack into the .w3d"), REC_INSIDE, IconType::archive);
        if (it) it->lineUp = true;
        Gray(it, inside.empty());
        it = gMenuObject->Add(T("Extract to a folder..."), REC_OUTSIDE, IconType::shortcut);
        Gray(it, outside.empty());
    }
}

// ---- SELECCION (vale en las dos vistas) ----
enum { SEL_ALL = 1, SEL_NOTHING, SEL_INVERT };
// la vista Escena: 0 todo, 1 nada, 2 invertir (el menu Seleccion y sus atajos A / Alt+A / Ctrl+I), con undo
void Outliner::SelectionScene(int action) {
    extern void SelectAllForced(bool);
    UndoCaptureSelection();
    if (action == 0) SelectAllForced(true);
    else if (action == 1) { DeselectAll(true); ActiveObj = NULL; }
    else {
        // lo que estaba seleccionado deja de estarlo y al reves (solo lo que se VE en el arbol)
        std::vector<Object*> all;
        struct R { static void Collect(Object* p, std::vector<Object*>& out) {
            for (size_t i = 0; i < p->Childrens.size(); i++) { if (!p->Childrens[i]) continue; out.push_back(p->Childrens[i]); Collect(p->Childrens[i], out); } } };
        if (SceneCollection) R::Collect(SceneCollection, all);
        std::set<Object*> was;
        for (size_t i = 0; i < all.size(); i++) if (all[i]->select) was.insert(all[i]);
        DeselectAll(true);
        ActiveObj = NULL;
        for (size_t i = 0; i < all.size(); i++) if (!was.count(all[i])) all[i]->Select();
    }
    g_redraw = true;
}
static void ActionPickedSelection(int id) {
    Outliner* o = gMenuOf;
    if (!o) return;
    if (o->view == OUT_VIEW_SCENE) {
        o->SelectionScene(id == SEL_ALL ? 0 : id == SEL_NOTHING ? 1 : 2);
    } else {
        if (id == SEL_ALL) {
            // todas (no el toggle de la A)
            std::vector<OutRowRec> rows; o->RowsResources(rows);
            o->selResources.clear(); o->selFolders.clear();
            for (size_t i = 0; i < rows.size(); i++) {
                if (rows[i].folder) o->selFolders.insert(rows[i].key); else o->selResources.insert(rows[i].key);
            }
            o->selMarked = true;
        } else if (id == SEL_NOTHING) {
            o->ReleaseSelection();   // suelta las marcas; queda el cursor (el recurso que muestra Properties)
        } else o->InvertSelectionRec();
    }
    g_redraw = true;
}
static void BuildMenuSelection(Outliner* o) {
    (void)o;
    if (!gMenuSelection) { gMenuSelection = new PopupMenu(); gMenuSelection->action = ActionPickedSelection; }
    gMenuSelection->Clear();
    gMenuSelection->title = T("Select");
    WithShortcut(gMenuSelection->Add(T("Select All"), SEL_ALL, IconType::selection), "@out.seleccionarTodo");
    WithShortcut(gMenuSelection->Add(T("Deselect All"), SEL_NOTHING, IconType::selection), "@out.deseleccionar");
    WithShortcut(gMenuSelection->Add(T("Invert Selection"), SEL_INVERT, IconType::selection), "@out.invertir");
}

// ---- "+": las raices nuevas (RaicesEditor) + carpeta y material de la biblioteca ----
enum { NEW_FOLDER = 4100, NEW_MATERIAL };
static void NewPickedAction(int id) {
    Outliner* o = gMenuOf;
    if (id == NEW_FOLDER) {
        if (!o) return;
        if (o->view == OUT_VIEW_SCENE || o->ViewReadOnly()) o->ChangeView(OUT_VIEW_LIBRARY);
        o->ActionNewFolder();
        return;
    }
    if (id == NEW_MATERIAL) {
        if (!o) return;
        if (o->view == OUT_VIEW_SCENE || o->ViewReadOnly()) o->ChangeView(OUT_VIEW_LIBRARY);
        o->ActionNewMaterial();
        return;
    }
    W3dRootsMenuAction(id);
}

// ---- FILTRO ----
enum { FILTER_NO_USE = 9100 };
static void ActionPickedFilter(int id) {
    if (!gMenuOf) return;
    if (id == FILTER_NO_USE) { gMenuOf->ChangeUnused(!gMenuOf->onlyUnused); return; }
    gMenuOf->ChangeFilter(id == 0 ? -1 : id);
}
static void BuildMenuFilter(Outliner* o) {
    if (!gMenuFilter) { gMenuFilter = new PopupMenu(); gMenuFilter->action = ActionPickedFilter; }
    gMenuFilter->Clear();
    gMenuFilter->title = T("Filter");
    MenuItem* it = gMenuFilter->Add(T("All"), 0, IconType::filter);
    if (it) it->green = (o->filter < 0);
    std::vector<int> types;
    W3dLibraryTypes(types);
    for (size_t i = 0; i < types.size(); i++) {
        it = gMenuFilter->Add(T(W3dTitleView(types[i])), types[i], W3dIconView(types[i]));
        if (it) it->green = (o->filter == types[i]);
    }
    // SIN USO: se suma al del tipo (las texturas sin uso, por ejemplo). Una libreria vinculada no lleva la cuenta
    if (!o->ViewReadOnly()) {
        it = gMenuFilter->Add(T("Unused"), FILTER_NO_USE, IconType::users);
        if (it) { it->green = o->onlyUnused; it->lineUp = true; }
    }
}

bool Outliner::OpenMenuButton(int idx) {
    if (idx < 0 || idx >= (int)BarButtons.size()) return false;
    Button* b = BarButtons[(size_t)idx];
    if (!b || !b->visible) return false;
    SyncBarView();
    barFocusIndex = idx;       // resaltar el boton + auto-scroll de la barra
    UpdateBar();               // sx/sy frescos
    gMenuOf = this;
    if (b == btnView) { BuildMenuView(this); OpenUnder(gMenuView, b); return true; }
    if (b == btnOptions) { BuildMenuOptions(this); OpenUnder(gMenuOptions, b); return true; }
    if (b == btnObject) { BuildMenuObject(this); OpenUnder(gMenuObject, b); return true; }
    if (b == btnSelection) { BuildMenuSelection(this); OpenUnder(gMenuSelection, b); return true; }
    if (b == btnFilter) { BuildMenuFilter(this); OpenUnder(gMenuFilter, b); return true; }
    if (b == btnRoot) {
        if (!gMenuRoot) { gMenuRoot = new PopupMenu(); gMenuRoot->action = W3dRootsMenuAction; }
        W3dRootsMenuBuild(gMenuRoot);
        OpenUnder(gMenuRoot, b);
        return true;
    }
    if (b == newBtn) {
        if (!gNewMenu) { gNewMenu = new PopupMenu(); gNewMenu->action = NewPickedAction; }
        W3dRootsMenuBuildNew(gNewMenu);
        gNewMenu->Add(T("New Folder"), NEW_FOLDER, IconType::folder);
        gNewMenu->Add(T("New Material"), NEW_MATERIAL, IconType::material);
        gNewMenu->title = T("Add");   // el boton es un icono sin texto -> el menu lleva titulo (en el idioma de ahora)
        OpenUnder(gNewMenu, b);
        return true;
    }
    if (b == btnGrid) {   // (sin menu: el teclado del N95 lo "abre" = lo alterna)
        ChangeGrid(!grid);
        return true;
    }
    return false;
}

bool Outliner::OpenBarMenu(int mx, int my) {
    SyncBarView();
    UpdateBar();
    for (size_t i = 1; i < BarButtons.size(); i++) {
        Button* b = BarButtons[i];
        if (!b || !b->visible || !b->Contains(mx, my)) continue;
        if (b == btnGrid) return false;   // (es un interruptor: se aprieta con ClickBar, no con el hover)
        // ya abierto (el hover que se desliza sobre el mismo boton): no se reabre
        const int ab = OpenMenuButtonIndex();
        if (ab == (int)i) return true;
        return OpenMenuButton((int)i);
    }
    return false;
}
bool Outliner::ClickBar(int mx, int my) {
    UpdateBar();
    if (btnGrid && btnGrid->visible && btnGrid->Contains(mx, my)) {
        ChangeGrid(!grid);
        return true;
    }
    return false;
}

int Outliner::OpenMenuButtonIndex() const {
    if (gMenuOf != this || !OpenMenu || !OpenMenu->open) return -1;
    for (size_t i = 0; i < BarButtons.size(); i++) {
        if (BarButtons[i] == btnView && OpenMenu == gMenuView) return (int)i;
        if (BarButtons[i] == btnOptions && OpenMenu == gMenuOptions) return (int)i;
        if (BarButtons[i] == btnObject && OpenMenu == gMenuObject) return (int)i;
        if (BarButtons[i] == btnSelection && OpenMenu == gMenuSelection) return (int)i;
        if (BarButtons[i] == btnRoot && OpenMenu == gMenuRoot) return (int)i;
        if (BarButtons[i] == newBtn && OpenMenu == gNewMenu) return (int)i;
        if (BarButtons[i] == btnFilter && OpenMenu == gMenuFilter) return (int)i;
    }
    return -1;
}

void Outliner::OpenMenuMove(int mx, int my) {
    std::vector<std::string> recs, chars;
    GetTarget(recs, chars, true);
    if (recs.empty() && chars.empty()) return;   // nada que se pueda mover (solo lectura)
    if (!gMenuMove) { gMenuMove = new PopupMenu(); gMenuMove->action = ActionMovePicked; }
    gMenuOf = this;
    BuildMenuMove(gMenuMove, OriginFolder(recs, chars), chars);
    gMenuMove->title = T("Move to Folder");
    if (OpenMenu && OpenMenu != gMenuMove) OpenMenu->Close();
    gMenuMove->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuMove;
    g_redraw = true;
}

// el MENU CONTEXTUAL (click derecho / pulsacion larga): las mismas acciones que el menu Objeto, en el mouse
bool Outliner::MenuContext(int mx, int my) {
    if (view == OUT_VIEW_SCENE) {
        // la fila bajo el mouse pasa a ser la seleccion (si no estaba en ella), como un click
        const int rel = my - y - borderGS - PosY - BarTopOffset();
        if (rel >= 0) {
            extern Object* OutlinerObjectInRow(int row);
            extern int OutlinerRowOfY(int rel);   // (Outliner.cpp: con los cortes constructor / cada cuadro)
            extern bool OutlinerActivateSceneOf(Object* o);
            Object* hit = OutlinerObjectInRow(OutlinerRowOfY(rel));
            if (hit && !OutlinerActivateSceneOf(hit)) return true;   // (de otra escena abierta: primero la activa)
            if (hit && !hit->select) { UndoCaptureSelection(); DeselectAll(); hit->Select(); }
            else if (hit) ActiveObj = hit;
        }
    } else {
        std::vector<OutRowRec> rows;
        RowsResources(rows);
        const int row = RowInPoint(rows, mx, my);
        gNewFolderParent.clear();
        if (row >= 0) {
            // sobre una fila de la SELECCION: el menu es de toda la seleccion; si no, queda sola
            if (!RowInSelection(rows[(size_t)row])) SelectionOnly(rows[(size_t)row]);
            PickRow(rows[(size_t)row], false);
            gNewFolderParent = rows[(size_t)row].folder ? rows[(size_t)row].key : rows[(size_t)row].folderOf;
        }
    }
    gMenuOf = this;
    BuildMenuObject(this);
    gNewFolderCtx = false;
    // la BIBLIOTECA: crear una carpeta ahi mismo (lo natural del click derecho, como en un explorador)
    if (view != OUT_VIEW_SCENE && !ViewReadOnly()) {
        MenuItem* it = gMenuObject->Add(T("Create Folder"), REC_NEW_FOLDER, IconType::folder);
        if (it) it->lineUp = true;
        gNewFolderCtx = true;
    }
    gMenuObject->title = T("Object");
    if (OpenMenu && OpenMenu != gMenuObject) OpenMenu->Close();
    gMenuObject->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = gMenuObject;
    g_redraw = true;
    return true;
}

// el MISMO menu desde el TECLADO (el OK mantenido del N95): el de la fila del cursor (la biblioteca) o del objeto
// activo (la escena), abierto en esa fila
bool Outliner::MenuContextKeyboard() {
    if (view == OUT_VIEW_SCENE) {
        int my = y + height / 2;
        extern int OutlinerYOfActive();
        const int yf = OutlinerYOfActive();
        if (yf >= 0) my = y + borderGS + PosY + BarTopOffset() + yf + (int)RowHeightGS / 2;
        return MenuContext(x + width / 3, my);
    }
    std::vector<OutRowRec> rows;
    RowsResources(rows);
    const int fc = CursorRow(rows);
    if (fc < 0) return MenuContext(x + width / 3, y + height - (int)RowHeightGS);   // (sin cursor: el menu igual, en el hueco)
    int mx = x + width / 3;
    if (grid) { int rx, ry, rw, rh; RectRow(rows, fc, rx, ry, rw, rh); mx = x + rx + rw / 2; }
    return MenuContext(mx, RowResourceY(fc));
}

// ============================================================================
//  TAMANO Y DIBUJO
// ============================================================================
void Outliner::ResizeResources() {
    std::vector<OutRowRec> rows;
    RowsResources(rows);
    int maxX = 0, maxY = 0;
    if (!grid) {
        for (size_t i = 0; i < rows.size(); i++) {
            const int w = marginGS + (rows[i].prof + 2) * (IconSizeGS + gapGS) + (int)(rows[i].name.size() + 1) * LetterWidthGS +
                          gapGS + IconSizeGS + 5 * LetterWidthGS + marginGS;
            if (w > maxX) maxX = w;
        }
        maxY = -(int)rows.size() * (int)RowHeightGS - marginGS;
    } else if (!rows.empty()) {
        // la cuadricula no scrollea en X: su alto es el fondo de la ultima celda
        int rx, ry, rw, rh;
        RectRow(rows, (int)rows.size() - 1, rx, ry, rw, rh);
        maxY = -(ry + rh - borderGS - TopContent() + (int)RowHeightGS) - marginGS;
    }
    if (showSizes && !grid) maxX += 9 * LetterWidthGS;   // (la columna del tamano)
    maxY -= RowHeightMemory();   // la fila de la MEMORIA (fija, abajo) no tapa la ultima fila: se puede scrollear hasta verla
    ResizeScrollbar(width, height, maxX, maxY, TopContent());
    const int reserveV = scrollY ? (borderGS + GlobalScale * 9 + 2) : 0;
    Row->SetSize(0, 0, (GLshort)(width - reserveV), RowHeightGS);
}

// la MINIATURA de la celda de un recurso (0 = el icono de su tipo). Las de una LIBRERIA externa no: las
// miniaturas se arman con los recursos del proyecto ABIERTO (el id de la libreria es solo un nombre) y la
// celda de su "Material" dibujaba el material homonimo de este proyecto. Sin montarla no hay de donde sacar
// la suya: va el icono del tipo.
unsigned int Outliner::RowThumbnail(const OutRowRec& f, int* w, int* h) const {
    if (w) *w = 0;
    if (h) *h = 0;
    if (f.folder) return 0;
    // UNA LIBRERIA vinculada: con el id que tiene en ESTE proyecto (el de la libreria, "Lib/Cuerpo"), asi no se confunde
    // con un recurso del proyecto que se llame igual (para eso la libreria se monta: sus recursos quedan registrados)
    if (ViewReadOnly()) {
        const std::string g = W3dExtLibGlobalId(view - OUT_VIEW_LIBEXT, f.type, f.id);
        return g.empty() ? 0 : W3dThumbnail(f.type, g, w, h);
    }
    return W3dThumbnail(f.type, f.id, w, h);
}

// un rectangulo de color liso (con la textura apagada) en coordenadas del panel
static void SmoothRect(int rx, int ry, int rw, int rh) {
    static Rec2D* r = NULL;
    if (!r) r = new Rec2D();
    w3dEngine::Disable(w3dEngine::Texture2D);
    r->SetSize((GLshort)rx, (GLshort)ry, (GLshort)rw, (GLshort)rh);
    r->RenderObject(false);
    w3dEngine::Enable(w3dEngine::Texture2D);
}
// un icono de la UI en (px, py) a 'sz' px (espejado = la flecha de "Ir atras": arrowRight dada vuelta)
static void IconIn(int icon, int px, int py, int sz, bool mirrored) {
    if (icon < 0 || icon >= (int)IconsUV.size()) return;
    const GLfloat* uv = IconsUV[(size_t)icon]->uvs;
    GLfloat fuv[8];
    if (mirrored) {
        fuv[0] = uv[2]; fuv[1] = uv[1]; fuv[2] = uv[0]; fuv[3] = uv[3];
        fuv[4] = uv[6]; fuv[5] = uv[5]; fuv[6] = uv[4]; fuv[7] = uv[7]; uv = fuv;
    }
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)px, (GLfloat)py, 0);
    if (sz != IconSizeGS) w3dEngine::Scalef((GLfloat)sz / (GLfloat)IconSizeGS, (GLfloat)sz / (GLfloat)IconSizeGS, 1.0f);
    W3dDrawStrip4(IconMesh, uv);
    w3dEngine::PopMatrix();
}
// la FLECHA DE ACCESO DIRECTO de un recurso EXTERNO (su archivo vive afuera del .w3d), en (px, py) a 'sz' px. El
// arte trae su caja NEGRA opaca: con el tinte blanco se lee arriba de cualquier miniatura
static void ExternalMark(int px, int py, int sz) {
    SetColorID(ColorID::white);
    IconIn((int)IconType::shortcut, px, py, sz, false);
}
// una ESCENA no esta "en uso" ni "huerfana" (se abre, no la usa nadie): sin tilde/cruz ni cuenta de usuarios
static bool NoUseState(const OutRowRec& f) { return f.folder || f.type == W3D_VIEW_SCENES; }
// el MARCO de un rectangulo (4 lados de 'g' px)
static void Frame(int rx, int ry, int rw, int rh, int g) {
    SmoothRect(rx, ry, rw, g); SmoothRect(rx, ry + rh - g, rw, g);
    SmoothRect(rx, ry, g, rh); SmoothRect(rx + rw - g, ry, g, rh);
}
// una textura (una miniatura) estirada a un cuadrado de 's' px en (px, py)
static void DrawTexture(unsigned int tex, int px, int py, int s) {
    GLshort v[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    v[0] = (GLshort)px;       v[1] = (GLshort)py;
    v[2] = (GLshort)(px + s); v[3] = (GLshort)py;
    v[4] = (GLshort)px;       v[5] = (GLshort)(py + s);
    v[6] = (GLshort)(px + s); v[7] = (GLshort)(py + s);
    static const GLfloat uv[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    w3dEngine::BindTexture(tex);
    w3dEngine::Color4f(1, 1, 1, 1);
    W3dDrawStrip4(v, uv);
    w3dEngine::BindTexture(Textures[0]->iID);   // de vuelta al atlas de la UI
}
// el texto que entra en 'px' pixeles (con "..." si se corta)
static std::string Crop(const std::string& t, int px) {
    const int n = px / (LetterWidthGS > 0 ? LetterWidthGS : 1);
    if ((int)t.size() <= n || n < 3) return t;
    return t.substr(0, (size_t)(n - 2)) + "..";
}

// la carpeta del ESTADO de guardado de un recurso: AZUL = guardado (en el proyecto o en su archivo) pero sin cargar;
// con FONDO ROJO = todavia no se guardo. Un recurso cargado y guardado no lleva carpeta. false = no lleva
static bool DrawFolderState(const OutRowRec& f, int px, int py) {
    if (f.folder || f.readOnly) return false;
    if (!f.dirty && f.loaded) return false;
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)px, (GLfloat)py, 0);
    if (f.dirty) {
        w3dEngine::Color4f(0.75f, 0.12f, 0.12f, 1.0f);
        SmoothRect(0, 0, IconSizeGS, IconSizeGS);
        SetColorID(ColorID::white);
    } else w3dEngine::Color4f(0.35f, 0.62f, 1.0f, 1.0f);
    W3dDrawStrip4(IconMesh, IconsUV[(size_t)IconType::folder]->uvs);
    w3dEngine::PopMatrix();
    return true;
}
// "12 KB", "3.4 MB" ("" si no se sabe)
static std::string TextSize(long b) { return b >= 0 ? W3dBytesText((double)b) : std::string(); }

// LA MEMORIA del proceso (la fila de abajo de la biblioteca): usada y libre en KB (-1 = la plataforma no lo dice)
static void ProcessMemoryKB(long& usedKB, long& freeKB) {
    usedKB = freeKB = -1;
#if defined(_WIN32) && !defined(W3D_SYMBIAN)
    PROCESS_MEMORY_COUNTERS_EX pmc; memset(&pmc, 0, sizeof(pmc));
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        usedKB = (long)(pmc.PrivateUsage / 1024);
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) freeKB = (long)(ms.ullAvailPhys / 1024);
#elif defined(W3D_SYMBIAN)
    TInt total = 0; User::AllocSize(total); usedKB = (long)(total / 1024);
    TInt free  = 0; if (HAL::Get(HALData::EMemoryRAMFree, free) == KErrNone) freeKB = (long)(free / 1024);
#elif defined(__linux__) || defined(__ANDROID__)
    FILE* f = fopen("/proc/self/statm", "r");
    if (f) { unsigned long tot = 0, res = 0; if (fscanf(f, "%lu %lu", &tot, &res) == 2) usedKB = (long)(res * 4u); fclose(f); }
    f = fopen("/proc/meminfo", "r");
    if (f) { char l[128]; while (fgets(l, sizeof l, f)) { unsigned long v = 0; if (sscanf(l, "MemAvailable: %lu", &v) == 1) { freeKB = (long)v; break; } } fclose(f); }
#endif
}
long W3dFreeMemoryKB() { long u = 0, l = 0; ProcessMemoryKB(u, l); return l; }   // (las miniaturas: io/Thumbnails.cpp)
static std::string TextMemory() {
    long used = 0, free = 0; ProcessMemoryKB(used, free);
    const w3dEngine::TextureMemory mt = w3dEngine::TexturesMemory();
    std::string s = std::string(T("RAM")) + ": " + (used >= 0 ? W3dBytesText((double)used * 1024.0) : std::string("?"));
    if (free >= 0) s += " (" + W3dBytesText((double)free * 1024.0) + " " + T("free") + ")";
    s += "   " + std::string(T("Video")) + ": " + W3dBytesText((double)mt.bytes);
    if (W3dThumbnailsNoMemory()) s = std::string(T("There is no memory to load the previews")) + " - " + s;
    return s;
}
int Outliner::RowHeightMemory() const {
    return (InLibrary() && showMemory) ? (int)RowHeightGS + 2 * GlobalScale : 0;
}

void Outliner::RenderResources(int glY, const std::vector<OutRowRec>& rows) {
    // (el rename que perdio el foco y las filas del cuadro ya los hizo Render)
    SyncBarView();
    const int reserveBar = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    const bool withState = !ViewReadOnly() && showUses;
    int xType, xSize, xEst, xEnd; ColumnsList(xType, xSize, xEst, xEnd);
    const int rowsHeight = (int)RowHeightGS;
    const int fc = CursorRow(rows);

    // ---- el color del NOMBRE: la fila ACTIVA (el cursor) en verde, la seleccion en verde oscuro, como en
    //      la escena (una carpeta elegida se resalta ELLA, no el ultimo recurso) ----
    struct Color { static void Row(const Outliner* o, const OutRowRec& f, bool isCursor, bool dragged) {
        const GLfloat op = f.readOnly ? 0.6f : 1.0f;
        const bool inSel = o->RowInSelection(f);
        if (dragged) SetColorID(ColorID::accent, op);
        else if (isCursor) SetColorID(inSel ? ColorID::accent : ColorID::white, op);
        else if (inSel) SetColorID(ColorID::accentDark, op);
        else SetColorID(ColorID::grayUI, op);
    } };

    w3dEngine::Enable(w3dEngine::ScissorTest);
    if (!grid) {
        // ---- LISTA: nombres (recortados antes de la columna Tipo), debajo de la fila de los titulos ----
        const int yCab = borderGS + BarTopOffset();   // la fila de los TITULOS (fija: no scrollea)
        const int hVis = height - (yCab + rowsHeight) - RowHeightMemory();   // lo que queda para las filas
        if (scrollX) w3dEngine::Scissor(x, glY + marginGS + RowHeightMemory(), xType, hVis - marginGS);
        else         w3dEngine::Scissor(x, glY + RowHeightMemory(), xType, hVis);
        const int y0 = PosY + borderGS + TopContent();
        const int hovL = (hoverMx >= 0 && Contains(hoverMx, hoverMy)) ? RowInPoint(rows, hoverMx, hoverMy) : -1;
        for (size_t i = 0; i < rows.size(); i++) {
            const int yf = y0 + (int)i * rowsHeight;
            if (yf + rowsHeight * 2 <= 0 || yf >= height + rowsHeight) continue;   // culling (mismo margen que el arbol)
            const OutRowRec& f = rows[i];
            if ((int)i == hovL) { SetColorID(ColorID::headerColor, 0.7f); SmoothRect(borderGS, yf, width - 2 * borderGS - reserveBar, rowsHeight); }
            const bool dragged = recDragging && f.key == recDragId && f.folder == recDragFolder;
            // LINEAS GUIA: una linea por cada carpeta de arriba (como el arbol de la escena)
            SetColorID(ColorID::grayUI, 0.55f);
            for (int L = 0; L < f.prof; L++) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(marginGS + PosX + L * (IconSizeGS + gapGS)), (GLfloat)yf, 0);
                W3dDrawStrip4(IconLineMesh, IconsUV[(size_t)IconType::line]->uvs);
                w3dEngine::PopMatrix();
            }
            Color::Row(this, f, (int)i == fc, dragged);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(marginGS + PosX + f.prof * (IconSizeGS + gapGS)), (GLfloat)yf, 0);
            // flechita (solo las carpetas se pliegan; la columna queda vacia en los recursos)
            if (f.folder)
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)(f.collapsed ? IconType::arrowRight : IconType::arrow)]->uvs);
            else if (f.check >= 0) {
                // LA CASILLA (una escena abierta en el editor): en la columna de la flechita, que los recursos no usan
                w3dEngine::PopMatrix();
                const int cx = marginGS + PosX + f.prof * (IconSizeGS + gapGS), cy = yf + (rowsHeight - IconSizeGS) / 2;
                SetColorID(ColorID::grayUI);
                Frame(cx + GlobalScale, cy + GlobalScale, IconSizeGS - 2 * GlobalScale, IconSizeGS - 2 * GlobalScale, GlobalScale);
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(marginGS + PosX + f.prof * (IconSizeGS + gapGS)), (GLfloat)yf, 0);
                if (f.check > 0) { SetColorID(ColorID::accent); RenderBitmapText(unicodeCheck, textAlign::center, IconSizeGS); }
                Color::Row(this, f, (int)i == fc, dragged);
            }
            w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
            if (f.icon >= 0 && f.icon < (int)IconsUV.size()) W3dDrawStrip4(IconMesh, IconsUV[(size_t)f.icon]->uvs);
            if (f.external && !f.folder) {   // EXTERNO: la flecha de acceso directo, chica, abajo a la izquierda del icono
                const int sz = IconSizeGS / 2;   // (la mitad: un texel = medio pixel de la escala, nitido en 2x y 4x)
                ExternalMark(-GlobalScale, IconSizeGS - sz + GlobalScale, sz);
                Color::Row(this, f, (int)i == fc, dragged);
            }
            w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
            const bool inRename = renaming && !renameObject && renameView == view && f.folder == renameFolder && f.key == renameKey;
            if (inRename) {
                SetColorID(ColorID::white);
                RenderTextField(rename, g_activeTextField == &rename, textAlign::left, 1920);
            } else RenderBitmapText(f.dirty ? f.name + "*" : f.name);
            w3dEngine::PopMatrix();
            // TACTIL: la fila AGARRADA (pulsacion larga) lleva un marco verde: ya se puede arrastrar
            if (grabRow == (int)i) { SetColorID(ColorID::accent); Frame(borderGS, yf, width - 2 * borderGS - reserveBar, rowsHeight, GlobalScale); }
        }
        // ---- las otras COLUMNAS: Tipo | Tamanio | Usos ([carpeta de guardado] cuantos lo usan + tilde verde (en uso) /
        //      cruz roja (huerfano)) ----
        {
            w3dEngine::Scissor(x, glY + RowHeightMemory(), width - borderGS, hVis);
            const int xIcon = xEnd - IconSizeGS;
            for (size_t i = 0; i < rows.size(); i++) {
                const OutRowRec& f = rows[i];
                const int yf = y0 + (int)i * rowsHeight;
                if (yf + rowsHeight * 2 <= 0 || yf >= height + rowsHeight) continue;
                // TIPO: "3D Mesh", "Material"... (una carpeta: "Folder")
                if (!f.back && xType < xSize) {
                    SetColorID(ColorID::grayUI, f.readOnly ? 0.5f : 0.8f);
                    w3dEngine::PushMatrix();
                    w3dEngine::Translatef((GLfloat)(xType + gapGS), (GLfloat)yf, 0);
                    RenderBitmapText(f.folder ? std::string(T("Folder")) : std::string(T(W3dViewSingular(f.type))), textAlign::left, xSize - xType - 2 * gapGS);
                    w3dEngine::PopMatrix();
                }
                if (f.folder) continue;
                if (showSizes) {
                    const std::string tt = TextSize(f.bytes);
                    SetColorID(ColorID::grayUI, f.loaded ? 0.9f : 0.55f);
                    w3dEngine::PushMatrix();
                    w3dEngine::Translatef((GLfloat)(xEst - gapGS), (GLfloat)yf, 0);
                    RenderBitmapText(tt, textAlign::right, xEst - xSize - 2 * gapGS);
                    w3dEngine::PopMatrix();
                }
                if (!withState) continue;
                DrawFolderState(f, xEst + gapGS, yf + GlobalScale);
                if (NoUseState(f)) continue;
                const bool inUse = f.users > 0;
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)xIcon, (GLfloat)(yf + GlobalScale), 0);
                if (inUse) SetColorID(ColorID::accent, 1.0f);
                else w3dEngine::Color4f(kRedOrphan[0], kRedOrphan[1], kRedOrphan[2], 0.85f);
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)(inUse ? IconType::notifOk : IconType::notifError)]->uvs);
                w3dEngine::PopMatrix();
                char b[16]; sprintf(b, "%d", f.users);
                SetColorID(ColorID::grayUI, inUse ? 1.0f : 0.6f);
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(xIcon - gapGS), (GLfloat)yf, 0);
                RenderBitmapText(b, textAlign::right, 4 * CharacterWidthGS);
                w3dEngine::PopMatrix();
            }
        }
        // ---- el ARRASTRE: el lugar que deja lo que se arrastra queda OSCURO con su borde; la linea verde dice donde
        //      cae (antes / despues de una fila) ----
        w3dEngine::Scissor(x, glY + RowHeightMemory(), width - borderGS, hVis);
        for (size_t i = 0; i < rows.size() && recDragging; i++) {
            const OutRowRec& f = rows[i];
            if (!(f.key == recDragId && f.folder == recDragFolder) && !(!f.folder && RowInSelection(f) && !recDragFolder)) continue;
            const int yf = y0 + (int)i * rowsHeight;
            w3dEngine::Color4f(0.0f, 0.0f, 0.0f, 0.7f);
            SmoothRect(borderGS, yf, xEnd - borderGS, rowsHeight);
            SetColorID(ColorID::grayUI, 0.8f);
            Frame(borderGS, yf, xEnd - borderGS, rowsHeight, GlobalScale);
        }
        if (recDragging && recInsRow >= 0 && recInsRow < (int)rows.size()) {
            const int yl = y0 + recInsRow * rowsHeight + (recInsAfter ? rowsHeight : 0) - GlobalScale;
            SetColorID(ColorID::accent);
            SmoothRect(borderGS, yl, xEnd - borderGS, 2 * GlobalScale);
        }
        // ---- los TITULOS de las columnas (siempre a la vista) ----
        w3dEngine::Scissor(x, glY, width - borderGS, height - borderGS);
        SetColorID(ColorID::gray);
        SmoothRect(borderGS, yCab, width - 2 * borderGS, rowsHeight);
        w3dEngine::Color4f(0.0f, 0.0f, 0.0f, 0.75f);
        SmoothRect(borderGS, yCab + rowsHeight - GlobalScale, width - 2 * borderGS, GlobalScale);
        SetColorID(ColorID::grayUI, 0.9f);
        {
            struct Title { static void In(int px, int py, const char* t, textAlign to, int w) {
                w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)px, (GLfloat)py, 0); RenderBitmapText(T(t), to, w); w3dEngine::PopMatrix(); } };
            const int yt = yCab + (rowsHeight - LetterHeightGS) / 2;
            Title::In(marginGS + IconSizeGS + gapGS, yt, "Name", textAlign::left, xType - marginGS - IconSizeGS - 2 * gapGS);
            if (xType < xSize) Title::In(xType + gapGS, yt, "Type", textAlign::left, xSize - xType - 2 * gapGS);
            if (showSizes) Title::In(xEst - gapGS, yt, "Size", textAlign::right, xEst - xSize - 2 * gapGS);
            if (withState) Title::In(xEst + gapGS, yt, "Users", textAlign::left, xEnd - xEst - gapGS);
        }
        // ---- los DIVISORES de las columnas: negros; blanco con el mouse encima (se puede arrastrar); verde
        //      arrastrandolo ----
        {
            const int yTop = yCab, hCol = height - yTop - borderGS - RowHeightMemory();
            const int xs[3] = { xType, xSize, xEst };
            const bool has[3] = { xType < xSize, showSizes, withState && xEst < xEnd };
            for (int k = 0; k < 3; k++) {
                if (!has[k]) continue;
                if (colDrag == k) SetColorID(ColorID::accent);
                else if (colHover == k) SetColorID(ColorID::white);
                else w3dEngine::Color4f(0.0f, 0.0f, 0.0f, 0.75f);
                const int thickness = (colDrag == k || colHover == k) ? 2 * GlobalScale : GlobalScale;
                SmoothRect(xs[k] - thickness / 2, yTop, thickness, hCol);
            }
        }
    } else {
        // ---- CUADRICULA: una carpeta a la vez. Cada celda: el CUADRADO (miniatura, o el icono del tipo / la carpeta
        //      en grande) y debajo, centrados y afuera del cuadrado, el nombre y el tamanio ----
        w3dEngine::Scissor(x, glY, width - borderGS - reserveBar, height - borderGS);
        W3dThumbnailsFrame();   // un cuadro nuevo: las miniaturas que falten se generan de a pocas
        const int hov = (hoverMx >= 0 && Contains(hoverMx, hoverMy)) ? RowInPoint(rows, hoverMx, hoverMy) : -1;
        for (size_t i = 0; i < rows.size(); i++) {
            int rx, ry, rw, rh;
            RectRow(rows, (int)i, rx, ry, rw, rh);
            ry += PosY;
            if (ry + rh <= 0 || ry >= height) continue;
            const OutRowRec& f = rows[i];
            const bool dragged = recDragging && f.key == recDragId && f.folder == recDragFolder;
            const int side = rw;   // el cuadrado
            // el FONDO del cuadrado: elegida / en la seleccion / el destino de un arrastre / bajo el mouse / normal
            const bool inSel = !f.back && RowInSelection(f);
            if (f.folder && recDragging && (int)i == recDropRow) SetColorID(ColorID::accentDark);
            else if (!f.back && ((int)i == fc || inSel)) SetColorID(inSel ? ColorID::accentDark : ColorID::headerColor);
            else if ((int)i == hov) SetColorID(ColorID::headerColor);
            else SetColorID(ColorID::gray);
            SmoothRect(rx, ry, side, side);
            if ((int)i == hov) { SetColorID(ColorID::grayUI, 0.9f); Frame(rx, ry, side, side, GlobalScale); }   // (bajo el mouse)
            // lo de ADENTRO: la miniatura, o el icono en grande (la carpeta, "Ir atras", un sonido, un script...)
            int tw = 0, th = 0;
            const unsigned int tex = f.folder ? 0u : RowThumbnail(f, &tw, &th);
            if (tex) DrawTexture(tex, rx, ry, side);
            else {
                // "Ir atras" = una FLECHA hacia atras; una carpeta, la carpeta; un recurso sin miniatura, su tipo
                const int icon = f.back ? (int)IconType::arrowRight : f.folder ? (int)IconType::folder : f.icon;
                const int esc = side / (IconSizeGS > 0 ? IconSizeGS : 1);
                const int sz = IconSizeGS * (esc > 1 ? esc - 1 : 1);
                SetColorID(ColorID::grayUI, f.readOnly ? 0.6f : 1.0f);
                IconIn(icon, rx + (side - sz) / 2, ry + (side - sz) / 2, sz, f.back);
            }
            // guardado (la carpeta azul / roja) arriba a la izquierda; en uso / huerfano arriba a la derecha
            if (withState && !f.folder) DrawFolderState(f, rx + GlobalScale, ry + GlobalScale);
            if (withState && !NoUseState(f)) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(rx + side - IconSizeGS - GlobalScale), (GLfloat)(ry + GlobalScale), 0);
                if (f.users > 0) SetColorID(ColorID::accent, 1.0f);
                else w3dEngine::Color4f(kRedOrphan[0], kRedOrphan[1], kRedOrphan[2], 0.85f);
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)(f.users > 0 ? IconType::notifOk : IconType::notifError)]->uvs);
                w3dEngine::PopMatrix();
            }
            // EXTERNO: la flecha de acceso directo abajo a la izquierda del cuadrado
            if (f.external && !f.folder) ExternalMark(rx + GlobalScale, ry + side - IconSizeGS - GlobalScale, IconSizeGS);
            // LO SELECCIONADO: un borde VERDE encima del cuadrado (una miniatura cuadrada tapa todo el fondo: sin el borde no
            // se notaba que estaba elegida)
            if (!f.back && ((int)i == fc || inSel)) {
                SetColorID(inSel ? ColorID::accent : ColorID::grayUI);
                Frame(rx, ry, side, side, 2 * GlobalScale);
            }
            // el NOMBRE, debajo del cuadrado y centrado
            if (f.back) SetColorID(ColorID::grayUI);
            else Color::Row(this, f, (int)i == fc, dragged);
            const bool inRename = renaming && !renameObject && renameView == view && renameFolder == f.folder && f.key == renameKey && !f.back;
            {
                const bool withIcon = !f.folder && f.icon >= 0;
                const int iconWidth = withIcon ? IconSizeGS + gapGS : 0;
                const int textWidth = rw - iconWidth;
                const std::string name = inRename ? rename.text : Crop(f.dirty ? f.name + "*" : f.name, textWidth);
                int chars = 0;
                for (size_t k = 0; k < name.size(); k++) if (((unsigned char)name[k] & 0xC0) != 0x80) chars++;
                int wTxt = chars * CharacterWidthGS;
                if (wTxt > textWidth) wTxt = textWidth;
                const int x0 = rx + (rw - iconWidth - wTxt) / 2;
                const int yTxt = ry + side + gapGS / 2;
                if (withIcon) {
                    IconIn(f.icon, x0, yTxt - (IconSizeGS - LetterHeightGS) / 2, IconSizeGS, false);
                }
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(x0 + iconWidth), (GLfloat)yTxt, 0);
                if (inRename) RenderTextField(rename, g_activeTextField == &rename, textAlign::left, textWidth);
                else RenderBitmapText(name, textAlign::left, textWidth);
                w3dEngine::PopMatrix();
            }
            // el TAMANIO, debajo del nombre
            if (showSizes && !f.folder && f.bytes >= 0) {
                const std::string tt = Crop(TextSize(f.bytes), rw);
                SetColorID(ColorID::grayUI, f.loaded ? 0.9f : 0.55f);
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(rx + (rw - UTF8_Length(tt) * CharacterWidthGS) / 2), (GLfloat)(ry + side + gapGS / 2 + rowsHeight), 0);
                RenderBitmapText(tt);
                w3dEngine::PopMatrix();
            }
            if (grabRow == (int)i) { SetColorID(ColorID::accent); Frame(rx, ry, side, side, GlobalScale); }
            // el ARRASTRE: el lugar que deja queda oscuro con su borde (lo arrastrado va junto al puntero)
            if (recDragging && (dragged || (!f.folder && !recDragFolder && RowInSelection(f)))) {
                w3dEngine::Color4f(0.0f, 0.0f, 0.0f, 0.7f);
                SmoothRect(rx, ry, rw, rh);
                SetColorID(ColorID::grayUI, 0.8f);
                Frame(rx, ry, rw, rh, GlobalScale);
            }
            // donde CAE: una linea verde antes / despues de la celda
            if (recDragging && recInsRow == (int)i) {
                SetColorID(ColorID::accent);
                SmoothRect((recInsAfter ? rx + rw + gapGS / 2 : rx - gapGS / 2) - GlobalScale, ry, 2 * GlobalScale, side);
            }
        }
    }
    w3dEngine::Disable(w3dEngine::ScissorTest);

    // ---- la fila de la MEMORIA, abajo de todo (fija: no scrollea) ----
    if (RowHeightMemory() > 0) {
        const int hf = RowHeightMemory();
        const int yf = height - borderGS - hf;
        SetColorID(ColorID::headerColor);
        SmoothRect(borderGS, yf, width - 2 * borderGS, hf);
        SetColorID(ColorID::grayUI);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(marginGS), (GLfloat)(yf + GlobalScale), 0);
        RenderBitmapText(Crop(TextMemory(), width - 2 * marginGS), textAlign::left, width - 2 * marginGS);
        w3dEngine::PopMatrix();
    }

    // ---- vista vacia: se dice por que ----
    if (rows.empty()) {
        SetColorID(ColorID::grayUI, 0.7f);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(marginGS), (GLfloat)(borderGS + TopContent() + rowsHeight / 2), 0);
        std::string msg = T("(empty)");
        if (ViewReadOnly()) {
            std::vector<W3dResourceItem> its; std::vector<std::string> cs; std::string err;
            if (!W3dExtLibList(view - OUT_VIEW_LIBEXT, its, cs, &err)) msg = std::string(T("Library not readable")) + ": " + T(err.c_str());
        }
        RenderBitmapText(msg, textAlign::left, width - 2 * marginGS);
        w3dEngine::PopMatrix();
    }
    // ---- el modo MOVER: a donde iria (abajo, como la barra de estado del transform) ----
    if (movingRec) {
        SetColorID(ColorID::accent);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)marginGS, (GLfloat)(height - rowsHeight - borderGS), 0);
        RenderBitmapText(std::string(T("Move to")) + ": " + (moveRecDestination.empty() ? std::string("(") + T("root") + ")" : moveRecDestination),
                         textAlign::left, width - 2 * marginGS);
        w3dEngine::PopMatrix();
    }
}

// ============================================================================
//  EL ARRASTRE AFUERA DEL OUTLINER (al 3D, a Properties): lo que se lleva junto al puntero y los
//  destinos resaltados (verde = se puede soltar ahi; rojo = no)
// ============================================================================
void OutlinerDragRender(int screenW, int screenH) {
    if (!g_outDrag.active && !g_outDrag.ghost) return;
    w3dEngine::Viewport(0, 0, screenW, screenH);
    w3dEngine::MatrixMode(w3dEngine::Projection); w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, screenW, screenH, 0, -1, 1);
    w3dEngine::MatrixMode(w3dEngine::ModelView); w3dEngine::LoadIdentity();
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(Textures[0]->iID);
    // los DESTINOS COMPATIBLES de Properties (desplegables del tipo que se arrastra)
    std::vector<ViewportBase*> leaves;
    struct J { static void Collect(ViewportBase* n, std::vector<ViewportBase*>& out) {
        if (!n) return;
        if (n->isLeaf()) { out.push_back(n); return; }
        if (n->ContainerKind() == 1) { Collect(((ViewportRow*)n)->childA, out); Collect(((ViewportRow*)n)->childB, out); }
        else { Collect(((ViewportColumn*)n)->childA, out); Collect(((ViewportColumn*)n)->childB, out); } } };
    J::Collect(LayoutCompleteRoot(), leaves);
    for (size_t h = 0; h < leaves.size() && g_outDrag.active; h++) {
        if (leaves[h]->ViewportKind() != 3) continue;
        std::vector<int> rects;
        PropsDestinationsResource((Properties*)leaves[h], g_outDrag.type, rects);
        for (size_t k = 0; k + 3 < rects.size(); k += 4) {
            const bool under = g_outDrag.x >= rects[k] && g_outDrag.x < rects[k] + rects[k + 2] &&
                              g_outDrag.y >= rects[k + 1] && g_outDrag.y < rects[k + 1] + rects[k + 3];
            SetColorID(ColorID::accent, under ? 1.0f : 0.55f);
            Frame(rects[k] - GlobalScale, rects[k + 1] - GlobalScale, rects[k + 2] + 2 * GlobalScale, rects[k + 3] + 2 * GlobalScale,
                  under ? 2 * GlobalScale : GlobalScale);
        }
    }
    // el viewport 3D bajo el puntero: marco verde (se puede) o rojo (no)
    ViewportBase* under = FindViewportUnderMouse(rootViewport, g_outDrag.x, g_outDrag.y);
    if (g_outDrag.active && under && under->isLeaf() && under->ViewportKind() == 1 && g_outDrag.destination != 0) {
        if (g_outDrag.destination > 0) SetColorID(ColorID::accent);
        else w3dEngine::Color4f(kRedOrphan[0], kRedOrphan[1], kRedOrphan[2], 1.0f);
        Frame(under->x, under->y, under->width, under->height, 2 * GlobalScale);
    }
    // lo que se lleva: el icono + el nombre + (que haria al soltar)
    const int px = g_outDrag.x + 3 * GlobalScale, py = g_outDrag.y + 3 * GlobalScale;
    std::string t = g_outDrag.name;
    if (!g_outDrag.destinationText.empty()) t += "  (" + g_outDrag.destinationText + ")";
    const int w = IconSizeGS + gapGS + (int)(t.size() + 1) * LetterWidthGS + 2 * gapGS;
    SetColorID(ColorID::gray, 0.9f);
    SmoothRect(px, py, w, (int)RowHeightGS);
    if (g_outDrag.destination < 0) w3dEngine::Color4f(kRedOrphan[0], kRedOrphan[1], kRedOrphan[2], 1.0f);
    else SetColorID(g_outDrag.destination > 0 ? ColorID::accent : ColorID::white);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(px + gapGS), (GLfloat)py, 0);
    if (g_outDrag.icon >= 0 && g_outDrag.icon < (int)IconsUV.size()) W3dDrawStrip4(IconMesh, IconsUV[(size_t)g_outDrag.icon]->uvs);
    w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
    RenderBitmapText(t);
    w3dEngine::PopMatrix();
}

// ============================================================================
//  ELEGIR, CLICK, ARRASTRE
// ============================================================================
// elegir una fila de la biblioteca SUELTA la escena (no queda un objeto seleccionado "de fondo": el panel
// de propiedades muestra el recurso) y el recurso pasa a ser el activo; una carpeta no tiene propiedades
void Outliner::PickRow(const OutRowRec& f, bool openProps) {
    cursorIsFolder = f.folder;
    cursorKey = f.key;
    if (InteractionMode == ObjectMode && (ActiveObj || !ObjSelects.empty())) {
        DeselectAll(true);
        ActiveObj = NULL;
    }
    if (!f.folder && !f.readOnly) {
        W3dResourceActivate(f.type, f.id);
        if (openProps) PropsGoToResource();
    } else if (!f.folder && ViewReadOnly()) {
        // UNO DE UNA LIBRERIA vinculada: Propiedades lo muestra de solo lectura (que es, si este proyecto lo usa, si esta
        // cargado)
        const int li = view - OUT_VIEW_LIBEXT;
        std::vector<W3dResourceItem> all; std::vector<std::string> cs;
        W3dExtLibList(li, all, cs, NULL);
        W3dResourceItem it; bool has = false;
        for (size_t i = 0; i < all.size() && !has; i++) if (all[i].type == f.type && all[i].id == f.id) { it = all[i]; has = true; }
        if (has) { W3dResourceExtActivate(li, it, W3dExtLibGlobalId(li, f.type, f.id)); if (openProps) PropsGoToResource(); }
        else W3dResourceDeactivate();
    } else W3dResourceDeactivate();
    g_redraw = true;
}

void Outliner::ClickResources(int mx, int my) {
    recDrag = recDragging = recDragFolder = recOnlyOnRelease = recPickOnRelease = false;
    recInsRow = -1;
    // el DIVISOR de una columna (la lista): se agarra para cambiar el ancho
    { const int d = DivisorInPoint(mx, my); if (d >= 0) { colDrag = d; colHover = -1; g_redraw = true; return; } }
    if (movingRec) { MoveRecConfirm(); return; }   // (un click confirma el modo mover, como en la escena)
    // un rename en linea abierto: el click AFUERA lo confirma (el down ya desenfoco el campo: controls.cpp)
    if (renaming) RenameSync();
    std::vector<OutRowRec> rows;
    RowsResources(rows);
    const int row = RowInPoint(rows, mx, my);
    if (row < 0) {
        // el vacio: suelta la seleccion multiple (el cursor queda)
        if (!LCtrlPressed && !LShiftPressed && SelectionCount() > 1) { selResources.clear(); selFolders.clear(); selMarked = false; g_redraw = true; }
        return;
    }
    if (grid && JustEntered(this)) return;   // (el 2do click de un doble click: lo de adentro no se toca)
    const OutRowRec& f = rows[(size_t)row];
    if (f.back) { EnterFolderGrid(f.key); return; }   // "Ir atras"
    // CUADRICULA: una carpeta se ABRE con un click. Con el mouse, al soltar sin arrastrar (arrastrarla la mueve); con el
    // dedo, al tocarla (con el dedo arrastrar es scroll)
    { extern bool g_uiTapInProgress;
      if (grid && f.folder && g_uiTapInProgress && !LCtrlPressed && !LShiftPressed) { EnterFolderGrid(f.key); return; } }
    // LA CASILLA de un recurso que la tiene (una escena: abrirla o cerrarla en el editor), en la columna de la flechita
    if (!f.folder && !grid && f.check >= 0 && !LCtrlPressed && !LShiftPressed) {
        const int xCheckbox = x + marginGS + PosX + f.prof * (IconSizeGS + gapGS);
        if (mx >= xCheckbox && mx < xCheckbox + IconSizeGS + gapGS) {
            W3dResourceProvider* p = W3dResourcesProviderView(f.type);
            std::string reason;
            if (p && !p->Check(f.id, f.check == 0, &reason) && !reason.empty()) Notify(T(reason.c_str()), true);
            lastContentRows = -1;
            g_redraw = true;
            return;
        }
    }
    if (f.folder && !grid) {
        // la FLECHITA (o el icono) pliega/despliega sin tocar la seleccion
        const int xArrow = x + marginGS + (grid ? 0 : PosX) + f.prof * (IconSizeGS + gapGS);
        if (mx >= xArrow && mx < xArrow + 2 * IconSizeGS + gapGS && !LCtrlPressed && !LShiftPressed) {
            std::set<std::string>& pl = CollapsedOf(this);
            if (f.collapsed) pl.erase(f.key); else pl.insert(f.key);
            lastContentRows = -1;
            if (SelectionCount() <= 1) SelectionOnly(f);   // (plegar no rompe una seleccion de varias)
            PickRow(f, false);
            return;
        }
    }
    // ---- la SELECCION MULTIPLE ----
    if (LShiftPressed) {
        // RANGO desde el cursor hasta la fila (sumado a lo que ya estaba)
        int from = CursorRow(rows);
        if (from < 0) from = row;
        const int a = from < row ? from : row, b = from < row ? row : from;
        for (int i = a; i <= b; i++) {
            if (rows[(size_t)i].folder) selFolders.insert(rows[(size_t)i].key);
            else selResources.insert(rows[(size_t)i].key);
        }
        selMarked = true;
        PickRow(f, !f.folder);
        return;   // (un rango no arranca arrastre)
    }
    if (LCtrlPressed) {
        // una fila MAS (o una menos): si la seleccion seguia al cursor, el cursor entra primero
        if (SelectionCount() == 0) {
            const int fc = CursorRow(rows);
            if (fc >= 0) SelectionToggle(rows[(size_t)fc]);
        }
        SelectionToggle(f);
        PickRow(f, !f.folder && RowInSelection(f));
        return;
    }
    // click comun: sobre una fila de una seleccion de VARIAS, la seleccion se conserva para poder
    // arrastrarla entera (si se suelta sin arrastrar, queda sola); si no, la fila queda sola
    if (SelectionCount() > 1 && RowInSelection(f)) recOnlyOnRelease = true;
    else SelectionOnly(f);
    // el click puede volverse ARRASTRE (a una carpeta, al 3D, a Properties): se confirma al moverse con el
    // boton. Con el DEDO no: arrastrar es scroll (se agarra con una pulsacion larga, PressLong)
    extern bool g_uiTapInProgress;
    if (!g_uiTapInProgress) {
        recDrag = true;
        recDragId = f.key;
        recDragFolder = f.folder;
        recDragY0 = my;
        recDragX0 = mx;
        recDropRow = -1;
        // la fila pasa a ser el cursor YA, pero ELEGIRLA (soltar la escena, activar el recurso) espera al soltar
        // sin arrastrar: arrastrar un recurso al selector de malla de Properties no puede apagar esa tarjeta
        cursorIsFolder = f.folder;
        cursorKey = f.key;
        recPickOnRelease = true;
        g_redraw = true;
    } else PickRow(f, !f.folder);
}

// el DESTINO de un arrastre AFUERA del outliner: un viewport 3D o un desplegable de Properties bajo el
// puntero. onlyTest = solo decir que haria (el resaltado mientras se arrastra).
extern void W3dReleasePreviewViewEnd();   // (DropResource.cpp: la parte con el material / shader de prueba vuelve)
static bool ReleaseOutside(int mx, int my, int type, const std::string& id, bool onlyTest, std::string* that) {
    ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
    if (!under || !under->isLeaf() || under->ViewportKind() != 1) W3dReleasePreviewViewEnd();   // (fuera del 3D: sin vista previa)
    if (!under || !under->isLeaf()) return false;
    if (under->ViewportKind() == 1) return W3dReleaseResourceIn3D((Viewport3D*)under, mx, my, type, id, onlyTest, that);
    if (under->ViewportKind() == 3) return PropsReleaseResource((Properties*)under, mx, my, type, id, onlyTest, that);
    return false;
}

void Outliner::MotionResources(int mx, int my) {
    const int ref = (grabRow >= 0) ? grabY : recDragY0;
    int d = my - ref; if (d < 0) d = -d;
    int dx = mx - ((grabRow >= 0) ? grabX : recDragX0); if (dx < 0) dx = -dx;
    if (!recDragging && (d > (int)RowHeightGS / 2 || dx > (int)RowHeightGS)) {
        recDragging = true;
        grabMoved = true;
    }
    if (!recDragging) return;
    recDropRow = -1;
    g_redraw = true;
    // AFUERA del outliner: un RECURSO (no una carpeta) se lleva al 3D o a Properties
    if (!Contains(mx, my)) {
        g_outDrag.active = false;
        if (recDragFolder) return;
        int t = 0; std::string id;
        if (!W3dBibByKey(recDragId, &t, &id)) return;
        W3dResourceItem it;
        const bool has = ViewReadOnly() ? false : W3dViewRecInfo(t, id, &it);
        g_outDrag.active = true;
        g_outDrag.type = t;
        g_outDrag.id = id;
        g_outDrag.name = has ? it.name : id;
        g_outDrag.icon = has ? it.icon : W3dIconView(t);
        g_outDrag.readOnly = ViewReadOnly();
        g_outDrag.x = mx; g_outDrag.y = my;
        g_outDrag.destinationText.clear();
        // el contenido de una LIBRERIA externa se suelta con su ID GLOBAL: un prefab o una escena = un PROXY; una
        // malla, un material, una textura o un animset = una referencia a su recurso (con su prefijo)
        const std::string idRelease = g_outDrag.readOnly ? W3dExtLibGlobalId(view - OUT_VIEW_LIBEXT, t, id) : id;
        if (idRelease.empty()) {
            g_outDrag.destination = -1;
            g_outDrag.destinationText = T("read-only library");
        } else {
            std::string that;
            const bool ok = ReleaseOutside(mx, my, t, idRelease, true, &that);
            ViewportBase* under = FindViewportUnderMouse(rootViewport, mx, my);
            const bool isDestination = under && under->isLeaf() && (under->ViewportKind() == 1 || under->ViewportKind() == 3);
            g_outDrag.destination = ok ? 1 : (isDestination && !that.empty() ? -1 : 0);
            g_outDrag.destinationText = that;
        }
        return;
    }
    g_outDrag.active = false;
    W3dReleasePreviewViewEnd();   // (de vuelta adentro: la parte del 3D vuelve a lo suyo)
    recInsRow = -1;
    // ADENTRO: lo que se lleva sigue al puntero (el fantasma); el lugar que deja queda oscuro (RenderResources)
    {
        std::vector<OutRowRec> fs; RowsResources(fs);
        g_outDrag.ghost = true;
        g_outDrag.x = mx; g_outDrag.y = my;
        g_outDrag.destination = 0; g_outDrag.destinationText.clear();
        g_outDrag.name = recDragId; g_outDrag.icon = recDragFolder ? (int)IconType::folder : -1;
        for (size_t i = 0; i < fs.size(); i++)
            if (fs[i].key == recDragId && fs[i].folder == recDragFolder) { g_outDrag.name = fs[i].name; g_outDrag.icon = fs[i].icon; break; }
        const int n = SelectionCount();
        if (n > 1) { char b[24]; sprintf(b, "  (+%d)", n - 1); g_outDrag.name += b; }
    }
    if (ViewReadOnly()) return;   // (adentro de una libreria externa no se mueve nada)
    std::vector<OutRowRec> rows;
    RowsResources(rows);
    const int row = RowInPoint(rows, mx, my);
    // REORDENAR: sobre OTRO recurso, lo arrastrado va antes o despues de el (la mitad de la fila / de la celda)
    if (row >= 0 && !recDragFolder && !rows[(size_t)row].folder && !rows[(size_t)row].part &&
        !(rows[(size_t)row].key == recDragId)) {
        int rx, ry, rw, rh; RectRow(rows, row, rx, ry, rw, rh);
        recInsRow = row;
        recInsAfter = grid ? (mx - x >= rx + rw / 2) : (my - y - PosY >= ry + rh / 2);
    }
    // el destino es la CARPETA de la fila (el mismo que decide ReleaseResources): la fila de una carpeta,
    // o la carpeta del recurso que se apunta. Se resalta ESA carpeta, no el recurso; la raiz (un
    // recurso suelto, o el vacio de abajo) no tiene fila y no se resalta nada.
    if (row >= 0) {
        if (rows[(size_t)row].folder) recDropRow = row;
        else if (!rows[(size_t)row].folderOf.empty()) {
            for (size_t i = 0; i < rows.size(); i++)
                if (rows[i].folder && rows[i].key == rows[(size_t)row].folderOf) { recDropRow = (int)i; break; }
        }
    }
}

bool Outliner::MoveTarget(const std::string& destination) {
    std::vector<std::string> recs, chars;
    GetTarget(recs, chars, true);
    if (recs.empty() && chars.empty()) return false;
    bool moved = false;
    // TODO en UN paso de undo (el grupo toma una foto al abrir y otra al cerrar)
    W3dViewRecGroupStart();
    for (size_t i = 0; i < chars.size(); i++) {
        if (W3dFolderInside(destination, chars[i])) continue;       // adentro de si misma: se saltea
        std::string newValue;
        if (!W3dFolderMove(chars[i], destination, &newValue) || newValue == chars[i]) continue;
        moved = true;
        // lo plegado, el cursor y la seleccion siguen a la carpeta (y a sus subcarpetas)
        std::set<std::string> pl, sc;
        for (std::set<std::string>::iterator it = collapsed.begin(); it != collapsed.end(); ++it)
            pl.insert(W3dFolderInside(*it, chars[i]) ? newValue + it->substr(chars[i].size()) : *it);
        collapsed.swap(pl);
        for (std::set<std::string>::iterator it = selFolders.begin(); it != selFolders.end(); ++it)
            sc.insert(W3dFolderInside(*it, chars[i]) ? newValue + it->substr(chars[i].size()) : *it);
        selFolders.swap(sc);
        if (cursorIsFolder && W3dFolderInside(cursorKey, chars[i])) cursorKey = newValue + cursorKey.substr(chars[i].size());
        for (size_t k = i + 1; k < chars.size(); k++)                // (las que faltan mover, por si colgaban)
            if (W3dFolderInside(chars[k], chars[i])) chars[k] = newValue + chars[k].substr(chars[i].size());
    }
    for (size_t i = 0; i < recs.size(); i++) {
        int t = 0; std::string id;
        if (!W3dBibByKey(recs[i], &t, &id)) continue;
        W3dResourceItem it;
        if (W3dViewRecInfo(t, id, &it) && it.folder == W3dFolderNormalize(destination)) continue;
        if (W3dViewRecMove(t, id, destination)) moved = true;
    }
    W3dViewRecGroupEnd();
    if (moved) {
        // la carpeta destino se despliega para que se vea donde quedo
        for (std::string c = W3dFolderNormalize(destination); !c.empty(); c = W3dFolderParent(c)) collapsed.erase(c);
        lastContentRows = -1;
    }
    g_redraw = true;
    return moved;
}

void Outliner::ReleaseResources(int mx, int my) {
    const bool was = recDragging;
    const bool onlyOnRelease = recOnlyOnRelease;
    const std::string key    = recDragId;
    const bool wasFolder = recDragFolder;
    const bool grabbed = grabRow >= 0, moved = grabMoved;
    const bool pick  = recPickOnRelease;
    recPickOnRelease = false;
    recDrag    = recDragging = recDragFolder = recOnlyOnRelease = false;
    recDropRow = -1;
    const int insRow = recInsRow; const bool insAfter = recInsAfter;
    recInsRow = -1;
    grabRow   = -1; grabMoved = false;
    const bool outside = g_outDrag.active;
    g_outDrag.active = false;
    W3dReleasePreviewViewEnd();   // (el arrastre termino: la parte vuelve a lo suyo; si se solto en el 3D, lo pone el soltar)
    g_outDrag.ghost = false;
    g_redraw = true;
    // TACTIL: soltar SIN MOVER lo que se agarro con la pulsacion larga = el menu contextual
    if (grabbed && !moved) { MenuContext(mx, my); return; }
    if (!was) {
        // CUADRICULA: un click (sin arrastrar, sin Ctrl / Shift) en una carpeta la ABRE
        if (grid && wasFolder && !LCtrlPressed && !LShiftPressed && Contains(mx, my)) {
            std::vector<OutRowRec> fs; RowsResources(fs);
            const int fi = RowInPoint(fs, mx, my);
            if (fi >= 0 && fs[(size_t)fi].folder && !fs[(size_t)fi].back && fs[(size_t)fi].key == key) { EnterFolderGrid(key); return; }
        }
        // un click sin arrastre sobre una fila de la seleccion: queda sola
        if (onlyOnRelease) {
            OutRowRec f; f.folder = wasFolder; f.key = key;
            SelectionOnly(f);
        }
        // ...y recien ahora se ELIGE (el panel de propiedades la muestra; la escena se suelta)
        if (pick) {
            std::vector<OutRowRec> rows; RowsResources(rows);
            for (size_t i = 0; i < rows.size(); i++)
                if (rows[i].folder == wasFolder && rows[i].key == key) { PickRow(rows[i], !wasFolder); break; }
        }
        return;
    }
    // AFUERA: el recurso se SUELTA en el viewport 3D o en un desplegable de Properties
    if (!Contains(mx, my)) {
        if (!outside || wasFolder) return;
        int t = 0; std::string id;
        if (!W3dBibByKey(key, &t, &id)) return;
        // (de una LIBRERIA externa: su id global, ver MotionResources; lo que no se puede arrastrar no hace nada)
        if (ViewReadOnly()) { id = W3dExtLibGlobalId(view - OUT_VIEW_LIBEXT, t, id); if (id.empty()) return; }
        std::string that;
        if (!ReleaseOutside(mx, my, t, id, false, &that) && !that.empty()) Notify(that, true);
        return;
    }
    if (ViewReadOnly()) return;
    std::vector<OutRowRec> rows;
    RowsResources(rows);
    const int row = RowInPoint(rows, mx, my);
    std::string destination;   // el vacio de abajo = la raiz
    if (row >= 0) destination = rows[(size_t)row].folder ? rows[(size_t)row].key : rows[(size_t)row].folderOf;
    // en la CUADRICULA el vacio es la carpeta que se esta viendo
    if (row < 0 && grid) destination = folderGrid;
    // REORDENAR: soltado sobre otro recurso, lo arrastrado queda antes / despues de el (y en su carpeta). Todo en
    // UN paso de undo
    std::string anchor;
    if (insRow >= 0 && insRow < (int)rows.size() && insRow == row) anchor = rows[(size_t)row].key;
    std::vector<std::string> recs, chars;
    GetTarget(recs, chars, true);
    W3dViewRecGroupStart();
    // lo que se arrastra: la SELECCION si la fila apretada es parte de ella (si no, ya quedo sola)
    MoveTarget(destination);
    if (!anchor.empty() && !recs.empty()) {
        std::set<std::string> mov(recs.begin(), recs.end());
        if (!mov.count(anchor)) {
            std::vector<OutRowRec> fs; RowsResources(fs);
            std::vector<std::string> siblings;
            for (size_t i = 0; i < fs.size(); i++) {
                if (fs[i].folder || fs[i].part || fs[i].folderOf != W3dFolderNormalize(destination) || mov.count(fs[i].key)) continue;
                siblings.push_back(fs[i].key);
            }
            std::vector<std::string>::iterator pos = std::find(siblings.begin(), siblings.end(), anchor);
            if (pos != siblings.end()) {
                if (insAfter) ++pos;
                std::vector<std::string> inOrder;   // (los movidos, en el orden en que se ven)
                for (size_t i = 0; i < fs.size(); i++) if (!fs[i].folder && mov.count(fs[i].key)) inOrder.push_back(fs[i].key);
                siblings.insert(pos, inOrder.begin(), inOrder.end());
                W3dBibOrderSet(siblings);
                lastContentRows = -1;
            }
        }
    }
    W3dViewRecGroupEnd();
    g_redraw = true;
}

// DOBLE CLICK / DOBLE TAP sobre el NOMBRE de una fila: se renombra en linea (en las dos vistas). En el resto de
// la fila (la flechita, el icono, el ojo, la camara, la columna de estado) no: esos clicks ya hicieron lo suyo
// (un doble click rapido sobre el ojo lo apaga y lo prende) y no piden un renombrar. Una CELDA de la cuadricula
// no tiene nada que alternar: vale entera.
bool Outliner::DoubleClick(int mx, int my) {
    if (!Contains(mx, my) || OnBar(mx, my)) return false;
    const int reserveBar = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    if (view == OUT_VIEW_SCENE) {
        const int rel = my - y - borderGS - PosY - BarTopOffset();
        if (rel < 0) return false;
        extern int OutlinerRowOfY(int rel);   // (Outliner.cpp: con los cortes constructor / cada cuadro)
        const int row = OutlinerRowOfY(rel);
        extern Object* OutlinerObjectInRow(int row);
        extern int OutlinerRowProf(int row);
        extern bool OutlinerActivateSceneOf(Object* o);
        Object* o = OutlinerObjectInRow(row);
        if (!o || !OutlinerActivateSceneOf(o)) return false;
        // el NOMBRE: despues de la flechita y el icono, antes de las columnas del ojo y la camara
        const int x0 = x + marginGS + PosX + (OutlinerRowProf(row) + 2) * (IconSizeGS + gapGS);
        const int x1 = x + width - 2 * IconSizeGS - gapGS - marginGS - borderGS - reserveBar;
        if (mx < x0 || mx >= x1) return false;
        dragObject = NULL; dragging = false;
        return RenameObjectInLine(o);
    }
    std::vector<OutRowRec> rows;
    RowsResources(rows);
    const int row = RowInPoint(rows, mx, my);
    if (row < 0) return false;
    if (grid && JustEntered(this)) { enteredMs = 0; return true; }   // (el 1er click ya entro a la carpeta)
    if (grid && rows[(size_t)row].folder) { EnterFolderGrid(rows[(size_t)row].key); return true; }
    if (!grid) {
        // el NOMBRE: despues de la flechita y el icono, antes de la columna de estado (en uso / huerfano)
        const int x0 = x + marginGS + (grid ? 0 : PosX) + (rows[(size_t)row].prof + 2) * (IconSizeGS + gapGS);
        int xT, xTm, xE, xF; ColumnsList(xT, xTm, xE, xF);   // (las mismas columnas que dibuja RenderResources)
        const int x1 = x + xT;
        (void)reserveBar;
        if (mx < x0 || mx >= x1) return false;
    }
    recDrag = recDragging = false;
    SelectionOnly(rows[(size_t)row]);
    PickRow(rows[(size_t)row], false);
    return ActionRename();
}

// TACTIL: MANTENER apretada una fila (~0,5 s sin mover el dedo) la AGARRA (marco verde): moverla despues la
// arrastra (a una carpeta, al 3D o a Properties; en la escena reordena/emparenta como con el mouse) y soltarla
// sin moverla abre el menu contextual. Sin esto, arrastrar el dedo es scroll (lo normal).
bool Outliner::PressLong(int mx, int my) {
    if (!Contains(mx, my) || OnBar(mx, my)) return false;
    grabX = mx; grabY = my;
    grabMoved = false;
    if (view == OUT_VIEW_SCENE) {
        const int rel = my - y - borderGS - PosY - BarTopOffset();
        if (rel < 0) return false;
        extern Object* OutlinerObjectInRow(int row);
        extern int OutlinerRowOfY(int rel);   // (Outliner.cpp: con los cortes constructor / cada cuadro)
        extern bool OutlinerActivateSceneOf(Object* o);
        const int rowRel = OutlinerRowOfY(rel);
        Object* o = OutlinerObjectInRow(rowRel);
        if (!o || !OutlinerActivateSceneOf(o)) return false;
        if (!o->select) { UndoCaptureSelection(); DeselectAll(); o->Select(); }
        dragObject = o; dragging = false; dragY0 = my;
        grabRow    = rowRel;
        g_redraw = true;
        return true;
    }
    std::vector<OutRowRec> rows;
    RowsResources(rows);
    const int row = RowInPoint(rows, mx, my);
    if (row < 0) return false;
    const OutRowRec& f = rows[(size_t)row];
    if (!RowInSelection(f)) SelectionOnly(f);
    cursorIsFolder = f.folder;   // (elegirla -soltar la escena- lo hace el menu contextual, si se suelta sin mover)
    cursorKey = f.key;
    grabRow = row;
    recDrag = true;
    recDragging = false;
    recDragId = f.key;
    recDragFolder = f.folder;
    recDragY0 = my; recDragX0 = mx;
    recOnlyOnRelease = false;
    g_redraw = true;
    return true;
}

// ============================================================================
//  TECLADO
// ============================================================================
void Outliner::EnsureVisibleRow(int row) {
    if (row < 0) return;
    const int top = TopContent();
    int yRow, rowHeightPx;
    if (!grid) { rowHeightPx = (int)RowHeightGS; yRow = borderGS + PosY + top + row * rowHeightPx; }
    else {
        std::vector<OutRowRec> rows; RowsResources(rows);
        int rx, ry, rw, rh;
        RectRow(rows, row, rx, ry, rw, rh);
        yRow = ry + PosY; rowHeightPx = rh;
    }
    int newValue = PosY;
    if (yRow < top)                newValue = PosY + (top - yRow);
    else if (yRow + rowHeightPx > height) newValue = PosY - (yRow + rowHeightPx - height);
    if (newValue > 0) newValue = 0;
    if (newValue < MaxPosY) newValue = MaxPosY;
    if (newValue != PosY) { PosY = newValue; g_redraw = true; }
}

// el cursor pasa a la fila 'n' por TECLADO. modo SEL_MARKS (el keypad del N95): con una seleccion
// de VARIAS (marcadas con el 5) las marcas se quedan y el cursor solo recorre; si no, la seleccion
// sigue al cursor. SEL_EXTEND (Shift+flechas) suma la fila. SEL_ONLY (las flechas de PC, como en
// un explorador) deja la fila sola.
void Outliner::CursorToKey(const std::vector<OutRowRec>& rows, int n, int mode) {
    if (n < 0 || n >= (int)rows.size()) return;
    const OutRowRec& f = rows[(size_t)n];
    if (mode == SEL_EXTEND) {
        if (SelectionCount() == 0) { const int fc = CursorRow(rows); if (fc >= 0) SelectionToggle(rows[(size_t)fc]); }
        if (!RowInSelection(f)) SelectionToggle(f);
    } else if (mode == SEL_ONLY || !selMarked) SelectionOnly(f);
    PickRow(f, false);   // un recurso elegido con flechas ya se ve en Properties
    EnsureVisibleRow(n);
}

// flechas / OK / C (LayoutKey): el keypad del N95
bool Outliner::KeyResources(int key) {
    return KeyResourcesWith(key, SEL_MARKS);
}
// flechas / Enter de un teclado de PC: llegan por LayoutKeyUI (el panel bajo el mouse) antes que
// por event_key_down, asi que el modo de PC se decide aca y no solo en KeyPCResources
bool Outliner::KeyResourcesPC(int key) {
#ifndef W3D_SYMBIAN
    const bool vertical = (key == LayoutKey::Up || key == LayoutKey::Down);
    return KeyResourcesWith(key, (vertical && LShiftPressed) ? SEL_EXTEND : SEL_ONLY);
#else
    return KeyResources(key);
#endif
}
bool Outliner::KeyResourcesWith(int key, int mode) {
    if (view == OUT_VIEW_SCENE) return false;
    // el MODO MOVER se queda con las flechas, el OK y el C
    if (movingRec) {
        switch (key) {
            case LayoutKey::Up:    MoveRecStep(0); return true;
            case LayoutKey::Down:  MoveRecStep(1); return true;
            case LayoutKey::Left:  MoveRecStep(2); return true;
            case LayoutKey::Right: MoveRecStep(3); return true;
            case LayoutKey::Enter: MoveRecConfirm(); return true;
            case LayoutKey::Cancel: MoveRecCancel(); return true;
        }
        return true;
    }
    std::vector<OutRowRec> rows;
    RowsResources(rows);
    int fc = CursorRow(rows);
    if (rows.empty()) return key != LayoutKey::Cancel;
    std::set<std::string>& pl = CollapsedOf(this);
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            int n = (fc < 0) ? 0 : fc + (key == LayoutKey::Down ? 1 : -1);
            if (n < 0) n = 0;
            if (n >= (int)rows.size()) n = (int)rows.size() - 1;
            CursorToKey(rows, n, mode);
            return true;
        }
        case LayoutKey::Left: {
            if (fc < 0) return true;
            const OutRowRec& f = rows[(size_t)fc];
            // una carpeta abierta se pliega; si no, el cursor sube a la carpeta de arriba
            if (f.folder && !f.collapsed) { pl.insert(f.key); lastContentRows = -1; g_redraw = true; return true; }
            if (!f.folderOf.empty()) {
                std::vector<OutRowRec> f2;
                cursorIsFolder = true; cursorKey = f.folderOf;
                RowsResources(f2);
                const int n = CursorRow(f2);
                if (n >= 0) CursorToKey(f2, n, mode == SEL_EXTEND ? SEL_MARKS : mode);
                g_redraw = true;
            }
            return true;
        }
        case LayoutKey::Right: {
            if (fc < 0) return true;
            const OutRowRec& f = rows[(size_t)fc];
            if (f.folder && f.collapsed) { pl.erase(f.key); lastContentRows = -1; g_redraw = true; }
            else if (f.folder && fc + 1 < (int)rows.size() && rows[(size_t)fc + 1].folderOf == f.key)
                CursorToKey(rows, fc + 1, mode == SEL_EXTEND ? SEL_MARKS : mode);
            return true;
        }
        case LayoutKey::Enter: {
            if (fc < 0) { CursorToKey(rows, 0, mode); return true; }
            const OutRowRec& f = rows[(size_t)fc];
            if (f.folder) {
                if (f.collapsed) pl.erase(f.key); else pl.insert(f.key);
                lastContentRows = -1; g_redraw = true;
            } else PickRow(f, true);   // OK sobre un recurso: Properties lo muestra
            return true;
        }
        case LayoutKey::Cancel:
            // C no se come aca: en el N95 es el MODIFICADOR de undo/redo (C + flecha) y su toque
            // borra (KeyDeleteResources, al soltar). Las marcas se sueltan con Esc (PC) o con
            // "Deselect All" del menu Seleccion.
            return false;
    }
    return false;
}

#ifndef W3D_SYMBIAN
bool Outliner::KeyPCResources(int key) {
    if (movingRec) {
        switch (key) {
            case W3dK_UP:    MoveRecStep(0); return true;
            case W3dK_DOWN:  MoveRecStep(1); return true;
            case W3dK_LEFT:  MoveRecStep(2); return true;
            case W3dK_RIGHT: MoveRecStep(3); return true;
            case W3dK_RETURN: case W3dK_KP_ENTER: MoveRecConfirm(); return true;
            case W3dK_ESCAPE: case W3dK_BACKSPACE: case W3dK_C: MoveRecCancel(); return true;
        }
        return true;   // en modo mover se traga el resto
    }
    switch (key) {
        // (las flechas y el Enter normalmente ya los tomo LayoutKeyUI -> KeyResourcesPC, con el mouse
        // sobre el panel; aca llegan con el mouse en otro lado y el outliner como panel activo)
        case W3dK_UP:    return KeyResourcesPC(LayoutKey::Up);
        case W3dK_DOWN:  return KeyResourcesPC(LayoutKey::Down);
        case W3dK_LEFT:  return KeyResourcesPC(LayoutKey::Left);
        case W3dK_RIGHT: return KeyResourcesPC(LayoutKey::Right);
        case W3dK_RETURN: case W3dK_KP_ENTER: return KeyResourcesPC(LayoutKey::Enter);
        case W3dK_ESCAPE: return ReleaseSelection();                   // suelta la seleccion de varias
        case W3dK_F2:    return ActionRename();
        case W3dK_X:
        case W3dK_DELETE: return ActionDelete(true);
        case W3dK_A:
            if (LAltPressed) { ReleaseSelection(); return true; }   // Alt+A: deseleccionar (queda el cursor)
            SelectAllRec(); return true;   // todo / nada (como la A de la escena)
        case W3dK_I:     if (LCtrlPressed) { InvertSelectionRec(); return true; } return false;
        case W3dK_G:     MoveStart(); return true;   // el modo MOVER (como en la escena)
        case W3dK_KP_PERIOD: {                       // encuadrar el cursor
            std::vector<OutRowRec> rows; RowsResources(rows);
            EnsureVisibleRow(CursorRow(rows));
            return true;
        }
    }
    return false;
}
#else
bool Outliner::KeyPCResources(int key) { (void)key; return false; }
#endif

// ============================================================================
//  EL MODO MOVER DE LA BIBLIOTECA (G / el 1 del N95): lo elegido viaja por las carpetas EN VIVO (cada
//  flecha lo mueve de verdad, sin undo) y al confirmar queda UN paso de undo; cancelar lo devuelve.
// ============================================================================
// las carpetas por donde puede viajar lo elegido, en el orden del arbol ("" = la raiz primero), sin las
// que se mueven (ni lo que cuelga de ellas)
static void DestinationsMove(const std::vector<std::string>& move, std::vector<std::string>& out) {
    out.clear();
    out.push_back(std::string());
    std::vector<std::string> cs;
    W3dFoldersAll(cs);
    for (size_t i = 0; i < cs.size(); i++) {
        bool outside = false;
        for (size_t k = 0; k < move.size() && !outside; k++) outside = W3dFolderInside(cs[i], move[k]);
        if (!outside) out.push_back(cs[i]);
    }
}
static std::vector<std::string> gMoveRecFolders;   // las carpetas que viajan (su ruta ORIGINAL)
void Outliner::MoveRecStart() {
    if (movingRec || ViewReadOnly()) return;
    std::vector<std::string> recs, chars;
    GetTarget(recs, chars, true);
    if (recs.empty() && chars.empty()) return;
    // la carpeta de donde sale (la de todos, o la del cursor)
    std::string origin = OriginFolder(recs, chars);
    if (origin == "\x01") origin.clear();
    // si no habia seleccion propia, lo elegido es el cursor: se congela en la seleccion (las flechas mueven)
    if (SelectionCount() == 0) {
        for (size_t i = 0; i < recs.size(); i++) selResources.insert(recs[i]);
        for (size_t i = 0; i < chars.size(); i++) selFolders.insert(chars[i]);
    }
    gMoveRecFolders = chars;
    W3dViewRecGroupStart();   // la foto: confirmar = un paso; cancelar = volver a ella
    movingRec = true;
    moveRecDestination = origin;
    g_redraw = true;
}
void Outliner::MoveRecStep(int dir) {
    if (!movingRec) return;
    std::vector<std::string> dest;
    DestinationsMove(selFolders.empty() ? gMoveRecFolders : std::vector<std::string>(selFolders.begin(), selFolders.end()), dest);
    int i = 0;
    for (size_t k = 0; k < dest.size(); k++) if (dest[k] == moveRecDestination) i = (int)k;
    std::string newValue = moveRecDestination;
    if (dir == 0 && i > 0) newValue = dest[(size_t)(i - 1)];
    else if (dir == 1 && i + 1 < (int)dest.size()) newValue = dest[(size_t)(i + 1)];
    else if (dir == 2) newValue = W3dFolderParent(moveRecDestination);
    else if (dir == 3) {
        for (size_t k = 0; k < dest.size(); k++)
            if (!dest[k].empty() && W3dFolderParent(dest[k]) == moveRecDestination) { newValue = dest[k]; break; }
    }
    if (newValue == moveRecDestination && !(dir == 2 && !moveRecDestination.empty())) return;
    moveRecDestination = newValue;
    MoveTarget(newValue);        // (adentro del grupo: no deja pasos sueltos)
    // la carpeta destino a la vista
    std::vector<OutRowRec> rows; RowsResources(rows);
    for (size_t k = 0; k < rows.size(); k++)
        if (rows[k].folder && rows[k].key == newValue) { EnsureVisibleRow((int)k); break; }
    g_redraw = true;
}
void Outliner::MoveRecConfirm() {
    if (!movingRec) return;
    movingRec = false;
    W3dViewRecGroupEnd();       // UN paso de undo con todo el viaje
    lastContentRows = -1;
    g_redraw = true;
}
void Outliner::MoveRecCancel() {
    if (!movingRec) return;
    movingRec = false;
    W3dViewRecGroupCancel();  // todo vuelve a donde estaba, sin paso
    lastContentRows = -1;
    g_redraw = true;
}

// ============================================================================
//  ACCIONES
// ============================================================================
// la carpeta donde se crea una NUEVA: la del cursor (si es carpeta, adentro; si es recurso, al lado)
bool Outliner::ActionNewFolder() {
    if (ViewReadOnly()) return false;
    std::vector<OutRowRec> rows; RowsResources(rows);
    const int fc = CursorRow(rows);
    std::string parent;
    if (fc >= 0) parent = rows[(size_t)fc].folder ? rows[(size_t)fc].key : rows[(size_t)fc].folderOf;
    return ActionNewFolderIn(parent);
}
bool Outliner::ActionNewFolderIn(const std::string& parent) {
    if (ViewReadOnly()) return false;
    std::string path;
    if (!W3dNewFolder(parent, T("New Folder"), &path)) return false;
    for (std::string c = parent; !c.empty(); c = W3dFolderParent(c)) collapsed.erase(c);
    cursorIsFolder = true;
    cursorKey = path;
    selResources.clear(); selFolders.clear(); selMarked = false;
    selFolders.insert(path);
    lastContentRows = -1;
    // se pone nombre enseguida (rename en linea con todo seleccionado)
    ActionRename();
    std::vector<OutRowRec> f2; RowsResources(f2);
    EnsureVisibleRow(CursorRow(f2));
    return true;
}
// "New Material" del "+": un material en blanco (un huerfano: se asigna arrastrandolo o desde Properties),
// en la carpeta del cursor, con el nombre en edicion
bool Outliner::ActionNewMaterial() {
    extern Material* W3dLibraryNewMaterial();
    std::vector<OutRowRec> rows; RowsResources(rows);
    const int fc = CursorRow(rows);
    std::string folder;
    if (fc >= 0) folder = rows[(size_t)fc].folder ? rows[(size_t)fc].key : rows[(size_t)fc].folderOf;
    UndoGroupStart();
    Material* m = W3dLibraryNewMaterial();
    if (m && !folder.empty()) W3dViewRecMove(W3D_VIEW_MATERIALS, m->name, folder);
    UndoGroupEnd();
    if (!m) return false;
    if (filter >= 0 && filter != W3D_VIEW_MATERIALS) ChangeFilter(-1);
    OutRowRec f; f.folder = false; f.type = W3D_VIEW_MATERIALS; f.id = m->name;
    f.key = W3dBibKey(W3D_VIEW_MATERIALS, m->name);
    SelectionOnly(f);
    PickRow(f, true);
    lastContentRows = -1;
    ActionRename();
    return true;
}

// TACTIL (Android/Symbian): el teclado QWERTY en pantalla, el de los campos de texto de Properties. Con usar el tactil
// alcanza (antes pedia un toque EN CURSO: el rename casi siempre viene del menu de mantener apretado -> "Renombrar", y
// para entonces el dedo ya se habia levantado, asi que en Android no salia ningun teclado y no se podia renombrar)
static void RenameKeyboard() {
#if !defined(__EMSCRIPTEN__)
    extern bool g_uiTapInProgress; extern bool ToolbarUsesTouch();
    if (g_uiTapInProgress || ToolbarUsesTouch()) KeyboardTextOpen();
#endif
}

// la vista Escena: el nombre del objeto se vuelve un campo EN SU FILA (doble click, F2, "Rename")
bool Outliner::RenameObjectInLine(Object* o) {
    if (!o) return false;
    if (renaming) RenameSync();
    renaming = true;
    renameObject = true;
    renameFolder = false;
    renameSerial = o->serial;
    renameKey.clear();
    renameView = OUT_VIEW_SCENE;
    rename.SetText(o->name);
    rename.SelectAll();
    TextFieldFocus(&rename);   // Esc vuelve a este texto: el rename no cambia nada
    RenameKeyboard();
    g_redraw = true;
    return true;
}

bool Outliner::ActionRename() {
    if (view == OUT_VIEW_SCENE) return RenameObjectInLine(ActiveObj);
    if (ViewReadOnly()) { Notify(T("The library is read-only"), true); return false; }
    std::vector<OutRowRec> rows; RowsResources(rows);
    const int fc = CursorRow(rows);
    if (fc < 0) return false;
    const OutRowRec& f = rows[(size_t)fc];
    if (!f.folder && !f.renamable) {
        Notify(T("This resource can't be renamed"), true);
        return false;
    }
    if (renaming) RenameSync();
    renaming = true;
    renameObject = false;
    renameFolder = f.folder;
    renameKey  = f.key;
    renameView = view;
    rename.SetText(f.name);
    rename.SelectAll();   // tipear reemplaza el nombre entero
    TextFieldFocus(&rename);   // Esc vuelve a este texto: el rename no cambia nada
    EnsureVisibleRow(fc);
    RenameKeyboard();
    g_redraw = true;
    return true;
}

// el Esc: el campo vuelve al texto de antes (TextFieldFocus lo anoto) y el rename se da por terminado
// sin tocar nada (el mismo nombre = no hace nada: ver W3dViewRecRename)
void Outliner::RenameCancel() {
    if (!renaming) return;
    renaming = false;
    if (g_activeTextField == &rename) g_activeTextField = NULL;
    lastContentRows = -1;
    g_redraw = true;
}

void Outliner::RenameSync() {
    if (!renaming || g_activeTextField == &rename) return;
    renaming = false;
    const std::string text = rename.text;
    if (renameObject) {
        // la ESCENA: W3dRenameObject (unico en su escena, arrastra las referencias lua, con undo)
        Object* o = NULL;
        struct B { static Object* By(Object* p, unsigned s) {
            if (!p) return NULL;
            for (size_t i = 0; i < p->Childrens.size(); i++) {
                if (!p->Childrens[i]) continue;
                if (p->Childrens[i]->serial == s) return p->Childrens[i];
                Object* r = By(p->Childrens[i], s); if (r) return r;
            }
            return NULL; } };
        o = B::By(SceneCollection, renameSerial);
        if (o && !text.empty() && text != o->name) W3dRenameObject(o, text, true);
    } else if (renameFolder) {
        std::string newValue;
        if (W3dFolderRename(renameKey, text, &newValue) && newValue != renameKey) {
            // lo plegado y el cursor siguen a la carpeta (y a sus subcarpetas)
            std::set<std::string> pl;
            for (std::set<std::string>::iterator it = collapsed.begin(); it != collapsed.end(); ++it)
                pl.insert(W3dFolderInside(*it, renameKey) ? newValue + it->substr(renameKey.size()) : *it);
            collapsed.swap(pl);
            if (cursorIsFolder && W3dFolderInside(cursorKey, renameKey))
                cursorKey = newValue + cursorKey.substr(renameKey.size());
            std::set<std::string> sc;
            for (std::set<std::string>::iterator it = selFolders.begin(); it != selFolders.end(); ++it)
                sc.insert(W3dFolderInside(*it, renameKey) ? newValue + it->substr(renameKey.size()) : *it);
            selFolders.swap(sc);
        }
    } else {
        int t = 0; std::string id, remained;
        if (W3dBibByKey(renameKey, &t, &id) && W3dViewRecRename(t, id, text, &remained)) {
            const std::string newValue = W3dBibKey(t, remained);
            if (!cursorIsFolder && cursorKey == renameKey) cursorKey = newValue;
            // (el id de una textura es su entrada: cambia con el nombre)
            if (newValue != renameKey && selResources.erase(renameKey)) selResources.insert(newValue);
        }
    }
    lastContentRows = -1;
    g_redraw = true;
}

// el borrado pendiente de confirmacion (ConfirmPopup llama un void() sin argumentos): UNO o VARIOS
// recursos de la seleccion (claves de biblioteca). 'forzado' = los en uso se borran igual (sus usuarios
// quedan sin ellos, con Ctrl+Z).
static std::vector<std::string> gDeleteKeys;
static std::vector<std::string> gDeleteFolders;   // las CARPETAS de la misma seleccion (se borran si quedan vacias)
static bool gDeleteForced = false;
// las rutas mas PROFUNDAS primero (una subcarpeta elegida junto con su madre se borra antes)
static bool MoreDeep(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() > b.size();
    return a < b;
}
static void DeleteConfirmed() {
    int n = 0, nc = 0;
    std::string reason, name;
    // (lo que tiene undo -partes, recursos en uso, escenas, carpetas- queda en UN Ctrl+Z)
    UndoGroupStart();
    for (size_t i = 0; i < gDeleteKeys.size(); i++) {
        int t = 0; std::string id, m;
        if (!W3dBibByKey(gDeleteKeys[i], &t, &id)) continue;
        name = id;
        const bool ok = gDeleteForced ? W3dViewRecDeleteForced(t, id, &m) : W3dViewRecDelete(t, id, &m);
        if (ok) n++; else reason = m;
    }
    // las CARPETAS elegidas junto con los recursos: despues de ellos (pueden haber quedado vacias). Una que no
    // esta vacia se queda (y se avisa), como cuando se borran carpetas solas
    std::sort(gDeleteFolders.begin(), gDeleteFolders.end(), MoreDeep);
    if (!gDeleteFolders.empty()) W3dViewRecGroupStart();
    for (size_t i = 0; i < gDeleteFolders.size(); i++) {
        std::string m;
        if (W3dFolderDelete(gDeleteFolders[i], &m)) nc++; else reason = m;
    }
    if (!gDeleteFolders.empty()) W3dViewRecGroupEnd();
    UndoGroupEnd();
    const int total = (int)(gDeleteKeys.size() + gDeleteFolders.size());
    if (total == 1 && gDeleteKeys.size() == 1) {
        if (n == 1) Notify(std::string(T("Deleted")) + ": " + name, false);
        else Notify(std::string(T("Not deleted")) + ": " + T(reason.c_str()), true);
    } else {
        char b[48]; sprintf(b, "%d / %d", n + nc, total);
        Notify(std::string(T("Deleted")) + ": " + b + (reason.empty() ? std::string() : std::string(" (") + T(reason.c_str()) + ")"),
                  n + nc < total);
    }
    gDeleteKeys.clear();
    gDeleteFolders.clear();
    gDeleteForced = false;
    g_redraw = true;
}
static int gPurgeType = -1;
static void PurgeConfirmed() {
    std::vector<std::string> names;
    const int n = W3dViewRecPurge(gPurgeType, &names);
    char b[64]; sprintf(b, "%d", n);
    Notify(std::string(T("Orphans purged")) + ": " + b, false);
    g_redraw = true;
}
bool Outliner::ActionDelete(bool confirm) {
    if (ViewReadOnly()) { Notify(T("The library is read-only"), true); return false; }
    std::vector<std::string> recs, chars;
    GetTarget(recs, chars, false);
    if (recs.empty() && chars.empty()) return false;
    if (recs.empty()) {
        // CARPETAS VACIAS: tienen undo (no hace falta confirmar) y van todas en UN paso
        std::sort(chars.begin(), chars.end(), MoreDeep);
        std::string reason;
        int n = 0;
        W3dViewRecGroupStart();
        for (size_t i = 0; i < chars.size(); i++) {
            std::string m;
            if (W3dFolderDelete(chars[i], &m)) n++; else reason = m;
        }
        W3dViewRecGroupEnd();
        if (n == 0) { Notify(T(reason.c_str()), true); return false; }
        if (!reason.empty()) Notify(T(reason.c_str()), true);   // alguna no estaba vacia
        // el cursor sube a la carpeta de arriba de la que se borro
        if (cursorIsFolder) {
            std::vector<OutRowRec> f2; RowsResources(f2);
            if (CursorRow(f2) < 0) {
                std::string c = cursorKey;
                while (!c.empty()) {
                    c = W3dFolderParent(c);
                    cursorKey = c;
                    if (c.empty() || CursorRow(f2) >= 0) break;
                }
                if (cursorKey.empty()) cursorIsFolder = false;
            }
        }
        selResources.clear(); selFolders.clear(); selMarked = false;
        lastContentRows = -1; g_redraw = true;
        return true;
    }
    // RECURSOS: los HUERFANOS y las PARTES de un recurso (un clip de su biblioteca) se borran; los EN USO se
    // ofrecen borrar IGUAL (sus usuarios quedan sin ellos, con Ctrl+Z) si su tipo lo sabe hacer
    gDeleteKeys.clear();
    gDeleteForced = false;
    // las CARPETAS de la misma seleccion van tambien (despues de los recursos, si quedan vacias): antes se
    // ignoraban sin avisar y la carpeta seguia ahi
    gDeleteFolders = chars;
    std::string reason, name, info;
    int inUse = 0, usersInUse = 0, meshesInUse = 0;
    bool onlyParts = true, roots = false;
    for (size_t k = 0; k < recs.size(); k++) {
        int t = 0; std::string id; W3dResourceItem it;
        if (!W3dBibByKey(recs[k], &t, &id) || !W3dViewRecInfo(t, id, &it)) continue;
        if (it.readOnly) { reason = "It is read-only"; continue; }
        if (!it.parent.empty()) { gDeleteKeys.push_back(recs[k]); name = it.name; continue; }
        if (t == W3D_VIEW_SCENES || t == W3D_VIEW_PREFABS) roots = true;
        if (it.users > 0) {
            { std::string mi; if (W3dViewRecUntouchable(t, id, &mi)) { reason = mi; continue; } }   // (la usa una sonda)
            if (!W3dViewRecKnowsDeleteInUse(t)) { reason = "It is in use"; continue; }
            inUse++; usersInUse += it.users;
            if (t == W3D_VIEW_MESHES) meshesInUse++;
        }
        gDeleteKeys.push_back(recs[k]); name = it.name; info = it.info; onlyParts = false;
    }
    if (gDeleteKeys.empty()) {
        gDeleteFolders.clear();
        Notify(T(reason.empty() ? "It can't be deleted" : reason.c_str()), true);
        return false;
    }
    gDeleteForced = inUse > 0;
    if (!confirm || onlyParts) { DeleteConfirmed(); lastContentRows = -1; return true; }   // (las partes tienen Ctrl+Z)
    if (!confirmPopup) confirmPopup = new ConfirmPopup();
    char b[640];
    if (inUse > 0) {
        // EN USO: se avisa CUANTOS lo usan; "Si" = borrar igual, "No" = cancelar. Los usuarios de un material, una
        // textura o un archivo quedan SIN el; los de una MALLA 3D son objetos que no pueden quedar sin su malla: se
        // BORRAN (con Ctrl+Z), y el cartel lo dice
        if (gDeleteKeys.size() == 1 && meshesInUse == 1) {
            snprintf(b, sizeof(b), T("\"%s\" is used by %d object(s). Delete it anyway?"), name.c_str(), usersInUse);
            snprintf(b + strlen(b), sizeof(b) - strlen(b), " %s", T("Its objects will be deleted too (Undo brings them back)."));
        } else if (gDeleteKeys.size() == 1)
            snprintf(b, sizeof(b), T("\"%s\" is in use by %d user(s). Delete it anyway? Its users will be left without it."), name.c_str(), usersInUse);
        else {
            snprintf(b, sizeof(b), T("%d resources are in use (%d users). Delete them anyway? Their users will be left without them."), inUse, usersInUse);
            if (meshesInUse > 0)
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " %s", T("The objects of the 3D meshes will be deleted too (Undo brings them back)."));
        }
    } else if (roots) {
        // una ESCENA, un JUEGO o un PREFAB entero se borra con todos sus objetos (con Ctrl+Z)
        if (gDeleteKeys.size() == 1) snprintf(b, sizeof(b), T("Delete \"%s\" (%s) with all its objects?"), name.c_str(), info.c_str());
        else snprintf(b, sizeof(b), T("Delete %d scenes, games or prefabs with all their objects?"), (int)gDeleteKeys.size());
    } else if (gDeleteKeys.size() == 1) {
        snprintf(b, sizeof(b), "%s \"%s\"?", T("Delete the orphan resource"), name.c_str());
    } else {
        snprintf(b, sizeof(b), "%s (%d)?", T("Delete the orphan resources"), (int)gDeleteKeys.size());
    }
    std::string msg = b;
    if (!gDeleteFolders.empty()) msg += std::string(" ") + T("The selected folders are deleted too if they end up empty.");
    confirmPopup->Open(msg, DeleteConfirmed);
    return true;
}

int Outliner::ActionPurge(bool confirm) {
    if (ViewReadOnly()) return 0;
    std::vector<W3dResourceItem> items; W3dLibraryList(filter, items);
    int orphans = 0;
    for (size_t i = 0; i < items.size(); i++) {
        W3dResourceProvider* p = W3dResourcesProviderView(items[i].type);
        if (items[i].users == 0 && !items[i].readOnly && items[i].parent.empty() && p && p->Purgeable()) orphans++;
    }
    if (orphans == 0) { Notify(T("There are no orphans"), false); return 0; }
    gPurgeType = filter;
    if (confirm) {
        if (!confirmPopup) confirmPopup = new ConfirmPopup();
        char b[32]; sprintf(b, "%d", orphans);
        confirmPopup->Open(std::string(T("Purge orphans")) + " (" + b + ")?", PurgeConfirmed);
        return orphans;
    }
    std::vector<std::string> names;
    const int n = W3dViewRecPurge(filter, &names);
    lastContentRows = -1; g_redraw = true;
    return n;
}

bool Outliner::ActionDuplicateResource() {
    if (ViewReadOnly()) return false;
    std::vector<OutRowRec> rows; RowsResources(rows);
    const int fc = CursorRow(rows);
    if (fc < 0 || rows[(size_t)fc].folder) return false;
    const OutRowRec& f = rows[(size_t)fc];
    std::string newValue, reason;
    if (!W3dViewRecDuplicate(f.type, f.id, &newValue, &reason)) { Notify(T(reason.c_str()), true); return false; }
    OutRowRec n = f; n.id = newValue; n.key = W3dBibKey(f.type, newValue);
    SelectionOnly(n);
    PickRow(n, true);
    lastContentRows = -1;
    Notify(std::string(T("Duplicated")) + ": " + newValue, false);
    return true;
}

// las hojas OUTLINER del layout entero (con un viewport maximizado, tambien las que no se ven)
static void CollectOutliners(ViewportBase* n, std::vector<Outliner*>& out) {
    if (!n) return;
    if (n->isLeaf()) { if (VpIsOutliner(n)) out.push_back((Outliner*)n); return; }
    if (n->ContainerKind() == 1) { CollectOutliners(((ViewportRow*)n)->childA, out); CollectOutliners(((ViewportRow*)n)->childB, out); }
    else { CollectOutliners(((ViewportColumn*)n)->childA, out); CollectOutliners(((ViewportColumn*)n)->childB, out); }
}

// Ctrl+Z / Ctrl+Y en medio del MODO MOVER de la biblioteca (W3dViewRecBeforeUndo): el outliner que lo tiene
// abierto lo cancela, como con Esc (el grupo de undo que abrio no puede absorber lo que se deshaga)
static void CancelModesMoveRec() {
    std::vector<Outliner*> outs;
    CollectOutliners(LayoutCompleteRoot(), outs);
    for (size_t i = 0; i < outs.size(); i++) if (outs[i]->movingRec) outs[i]->MoveRecCancel();
}
static bool gHookUndoOk = (W3dViewRecGroupCancelHook = CancelModesMoveRec, true);

// "Select Users" (el menu del outliner y el boton de la tarjeta de Properties: los dos hacen lo mismo). Los
// usuarios quedan seleccionados en la ESCENA (el 3D los muestra); la biblioteca SIGUE SIENDO BIBLIOTECA (pedido del
// dueno): solo los outliners que ya miraban la escena se centran en ellos. Sin OBJETOS que seleccionar se avisa.
int OutlinerSelectUsers(const std::vector<std::string>& keys) {
    if (keys.empty()) return 0;
    const int n = W3dBibSelectUsers(keys);   // la UNION de los usuarios
    if (n == 0) { Notify(T("No object of the scene uses it"), true); return 0; }
    std::vector<Outliner*> outs;
    CollectOutliners(LayoutCompleteRoot(), outs);
    for (size_t i = 0; i < outs.size(); i++) if (outs[i]->view == OUT_VIEW_SCENE) outs[i]->CenterSelection();
    char b[32]; sprintf(b, "%d", n);
    Notify(std::string(T("Users selected")) + ": " + b, false);
    g_redraw = true;
    return n;
}

int Outliner::ActionSelectUsers() {
    std::vector<std::string> recs, chars;
    GetTarget(recs, chars, false);
    return OutlinerSelectUsers(recs);
}

bool Outliner::ActionMove(const std::string& folder) {
    // la SELECCION entera (recursos y carpetas) o el cursor, en un paso de undo
    if (!MoveTarget(folder)) return false;
    std::vector<OutRowRec> f2; RowsResources(f2);
    EnsureVisibleRow(CursorRow(f2));
    return true;
}
