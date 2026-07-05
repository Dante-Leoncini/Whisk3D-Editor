#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "FileBrowser.h"
#include "ViewPorts/LayoutInput.h" // LayoutKey
#include "WhiskUI/UI.h"
#include "WhiskUI/bitmapText.h"
#include "WhiskUI/icons.h"
#include "WhiskUI/glesdraw.h" // W3dDrawStrip4 / W3dPantallaAlto
#include "objects/Textures.h"
#include "variables.h"
#include <cstring>

extern bool leftMouseDown;
extern bool ViewPortClickDown; // lo usa Scrollable para el verde al arrastrar
extern bool MouseWheel;        // lo usa Scrollable::ScrollY (rama de la rueda)

static const float kFolder[3] = { 0.86f, 0.68f, 0.30f };

static inline const float* COL(ColorID::Enum c) { return ListaColores[static_cast<int>(c)]; }

// cuenta caracteres (UTF-8) para centrar (la fuente es monoespaciada)
static int CharCount(const std::string& s) {
    size_t i = 0; int n = 0;
    while (i < s.size()) { UTF8_Char(s.c_str(), i); n++; }
    return n;
}
// 's' termina en 'ext' (case-insensitive)
static bool TerminaEn(const std::string& s, const char* ext) {
    size_t n = strlen(ext);
    if (s.size() < n) return false;
    size_t off = s.size() - n;
    for (size_t i = 0; i < n; i++) {
        char a = s[off + i], b = ext[i];
        if (a >= 'A' && a <= 'Z') a += 32; if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
    }
    return true;
}
// icono segun el tipo de archivo
static int IconoEntrada(const w3dFileSystem::DirEntry& e) {
    if (e.isDir) return (int)IconType::carpeta;
    if (TerminaEn(e.name, ".obj")) return (int)IconType::mesh;
    if (TerminaEn(e.name, ".png") || TerminaEn(e.name, ".jpg") || TerminaEn(e.name, ".jpeg") ||
        TerminaEn(e.name, ".bmp") || TerminaEn(e.name, ".tga") || TerminaEn(e.name, ".gif"))
        return (int)IconType::foto;
    return (int)IconType::archive;
}

// parte 's' en hasta 'maxLines' renglones de <= maxChars. Corta en un espacio
// si puede; si no, parte la palabra. (byte-based: los nombres son casi ASCII)
static void WrapText(const std::string& s, int maxChars, int maxLines,
                     std::vector<std::string>& out) {
    out.clear();
    if (maxChars < 1) maxChars = 1;
    size_t pos = 0;
    while (pos < s.size() && (int)out.size() < maxLines) {
        while (pos < s.size() && s[pos] == ' ') pos++;       // saltar espacios
        if (pos >= s.size()) break;
        if ((int)(s.size() - pos) <= maxChars) { out.push_back(s.substr(pos)); break; }
        size_t brk = s.rfind(' ', pos + maxChars);            // espacio mas a la derecha
        if (brk != std::string::npos && brk > pos) { out.push_back(s.substr(pos, brk - pos)); pos = brk + 1; }
        else { out.push_back(s.substr(pos, maxChars)); pos += maxChars; } // partir palabra
    }
}

// ViewportBase minimo: el Scrollable necesita uno (su ctor es liviano)
namespace { class FBPane : public ViewportBase { public: void Render() {} }; }

// para el drag del scroll: ultimo Y del mouse
static int gDragMy = 0;

// ============================================================================

FileBrowser::FileBrowser(const std::string& title, const std::string& accionLabel,
                         const std::string& filtro, void (*accept)(const std::string&))
    : PopUpBase(title) {
    onAccept = accept; actionLabel = accionLabel; filterExt = filtro;
    modoGuardar = false;
    histPos = -1; selected = -1; hover = -1; hoverBm = -1; selBm = -1;
    gridView = true; horizontal = true;
    focoZona = FZ_NONE; focoIdx = 0;
    topH = botH = 0; panelX = panelY = panelW = panelH = 0;
    fileX = fileY = fileW = fileH = 0; cellW = cellH = cols = 0; bmCols = 1;

    card = new Card(NULL, 10, 10);
    pane = new FBPane();

    btnBack = new Button("", (int)IconType::arrowRight, false); btnBack->iconFlip = 1; btnBack->centrado = true;
    btnFwd  = new Button("", (int)IconType::arrowRight, false); btnFwd->centrado = true;
    btnUp   = new Button("", (int)IconType::arrow, false);      btnUp->iconFlip = 2; btnUp->centrado = true;
    btnView = new Button("", (int)IconType::lista, false);      btnView->centrado = true;
    btnBmAdd = new Button("+", -1, false); btnBmAdd->centrado = true;
    btnBmDel = new Button("-", -1, false); btnBmDel->centrado = true;
    btnCancel = new Button("Cancelar", -1, false); btnCancel->centrado = true;
    btnAction = new Button(accionLabel, -1, false); btnAction->centrado = true;
    btnAction->colorTexto = COL(ColorID::negro); // texto negro sobre el verde

    w3dFileSystem::GetBookmarks(bookmarks);
}

FileBrowser::~FileBrowser() {
    delete card; delete pane;
    delete btnBack; delete btnFwd; delete btnUp; delete btnView;
    delete btnBmAdd; delete btnBmDel; delete btnCancel; delete btnAction;
}

bool FileBrowser::seleccionValida() const {
    if (modoGuardar) return true; // guardar: el boton "elige la carpeta actual" (siempre activo)
    return selected >= 0 && selected < (int)entries.size() && !entries[selected].isDir;
}

bool FileBrowser::PasaFiltro(const w3dFileSystem::DirEntry& e) const {
    if (e.isDir) return true;
    if (filterExt.empty()) return true;
    // filterExt = lista de extensiones separadas por espacio (".png .jpg ...")
    size_t pos = 0;
    while (pos < filterExt.size()) {
        size_t sp = filterExt.find(' ', pos);
        std::string ext = (sp == std::string::npos) ? filterExt.substr(pos) : filterExt.substr(pos, sp - pos);
        if (!ext.empty() && TerminaEn(e.name, ext.c_str())) return true;
        if (sp == std::string::npos) break;
        pos = sp + 1;
    }
    return false;
}

void FileBrowser::Abrir(const std::string& startDir) {
    std::string dir = startDir.empty() ? w3dFileSystem::GetHomeDir() : startDir;
    history.clear(); histPos = -1; Navegar(dir, true);
}
void FileBrowser::Recargar() {
    std::vector<w3dFileSystem::DirEntry> todas;
    w3dFileSystem::ListDir(currentPath, todas);
    entries.clear();
    for (size_t i = 0; i < todas.size(); i++) if (PasaFiltro(todas[i])) entries.push_back(todas[i]);
    selected = -1; PosY = 0;
}
void FileBrowser::Navegar(const std::string& dir, bool pushHistory) {
    std::vector<w3dFileSystem::DirEntry> prueba;
    if (!w3dFileSystem::ListDir(dir, prueba)) return;
    currentPath = dir;
    if (pushHistory) {
        while ((int)history.size() > histPos + 1) history.pop_back();
        history.push_back(dir); histPos = (int)history.size() - 1;
    }
    Recargar(); // deja selected = -1
    // ENTRAR a una carpeta (OK sobre carpeta / bookmark / arriba): deja el 1er
    // item ya seleccionado, asi no hay que bajar una vez. ATRAS/ADELANTE
    // (pushHistory=false) NO: el foco se queda en la barra de navegacion.
    // (No se llama EnsureVisible: el item 0 esta arriba y PosY=0 ya lo muestra;
    // ademas Navegar corre ANTES del 1er Resize -> cols=0 crasheaba en /0.)
    if (pushHistory && !entries.empty()) {
        focoZona = FZ_FILES; selected = 0;
    }
}
void FileBrowser::Atras()    { if (histPos > 0) { histPos--; Navegar(history[histPos], false); } }
void FileBrowser::Adelante() { if (histPos + 1 < (int)history.size()) { histPos++; Navegar(history[histPos], false); } }
void FileBrowser::Arriba()   { std::string p = w3dFileSystem::ParentPath(currentPath); if (p != currentPath) Navegar(p, true); }
void FileBrowser::AbrirEntrada(int idx) {
    if (idx < 0 || idx >= (int)entries.size()) return;
    if (entries[idx].isDir) {
        Navegar(w3dFileSystem::JoinPath(currentPath, entries[idx].name), true);
    } else if (selected == idx) {
        // segundo click sobre el archivo YA seleccionado = doble click =
        // importar / cargar (no hace falta ir al boton verde de abajo)
        Aceptar();
    } else {
        selected = idx; // primer click: solo seleccionar
    }
}
void FileBrowser::Aceptar() {
    std::string full;
    if (modoGuardar) {
        // guardar: si hay un archivo sel -> su ruta (sobrescribir); si no -> la carpeta actual
        if (selected >= 0 && selected < (int)entries.size() && !entries[selected].isDir)
            full = w3dFileSystem::JoinPath(currentPath, entries[selected].name);
        else
            full = currentPath; // la carpeta destino (el caller le agrega el nombre)
    } else {
        if (!seleccionValida()) return;
        full = w3dFileSystem::JoinPath(currentPath, entries[selected].name);
    }
    void (*cb)(const std::string&) = onAccept;
    Cerrar(); if (cb) cb(full);
}
void FileBrowser::AgregarBookmark() {
    w3dFileSystem::Bookmark b; size_t s = currentPath.find_last_of('/');
    b.name = (s == std::string::npos) ? currentPath : currentPath.substr(s + 1);
    if (b.name.empty()) b.name = currentPath;
    b.path = currentPath; b.user = true; // del usuario -> se persiste
    for (size_t i = 0; i < bookmarks.size(); i++) if (bookmarks[i].path == b.path) return; // ya esta
    bookmarks.push_back(b);
    w3dFileSystem::SaveUserBookmarks(bookmarks); // queda guardado en disco (entre sesiones)
}
void FileBrowser::QuitarBookmark() {
    if (selBm >= 0 && selBm < (int)bookmarks.size()) {
        if (!bookmarks[selBm].user) return; // Home/drives/carpetas NO se borran (se regeneran)
        bookmarks.erase(bookmarks.begin() + selBm); selBm = -1;
        w3dFileSystem::SaveUserBookmarks(bookmarks);
    }
}

int FileBrowser::ContentH() const {
    int n = (int)entries.size();
    if (gridView) { int c = (cols < 1) ? 1 : cols; int rows = (n + c - 1) / c; return rows * cellH + 2 * marginGS; }
    return n * (RenglonHeightGS + GlobalScale) + 2 * marginGS; // renglones casi pegados (1px)
}

// ---------------------------------------------------------------- layout
void FileBrowser::Layout() {
    x = 0; y = 0; popUpWindow->Resize(winW, winH);
    horizontal = (winW >= winH);
    int barH = RenglonHeightGS + bordersGS, m = gapGS;
    topH = barH + 2 * m; botH = barH + 2 * m;
    int bodyY = topH, bodyH = winH - topH - botH;
    int pmin = 80 * GlobalScale;
    if (horizontal) {
        panelW = winW / 4;
        if (panelW < pmin) panelW = pmin;
        if (panelW > winW / 3) panelW = winW / 3; // no se come la ventana
        panelX = m; panelY = bodyY; panelH = bodyH - m;
        fileX = panelX + panelW + m; fileY = bodyY; fileW = winW - fileX - m; fileH = bodyH - m;
    } else {
        fileX = m; fileY = bodyY; fileW = winW - 2 * m; fileH = (bodyH * 64) / 100;
        panelX = m; panelY = bodyY + fileH + m; panelW = winW - 2 * m; panelH = bodyH - fileH - 2 * m;
    }
    // ancho util: reservar el area de la barra de scroll a la derecha + un gap
    int contentW = fileW - (borderGS + 9 * GlobalScale) - gapGS;
    // alto: icono (26) + hasta 3 renglones de nombre
    cellW = 74 * GlobalScale;
    cellH = 26 * GlobalScale + gapGS * 2 + 3 * (LetterHeightGS + GlobalScale) + gapGS;
    cols = contentW / cellW;
#ifdef W3D_SYMBIAN
    // pantalla angosta (~240px): forzar al menos 3 columnas. cellW se achica
    // (contentW/3): items mas chicos para que entren las 3 en vertical y horizontal.
    if (cols < 3) cols = 3;
#endif
    if (cols < 1) cols = 1; cellW = contentW / cols;

    // accesos: en horizontal el panel es angosto -> 1 columna (lista). En
    // vertical es ancho y bajo -> varias columnas para que entren todas las
    // unidades (C: D: E: ...) sin quedar cortadas abajo.
    int bmChipMin = 64 * GlobalScale;
    bmCols = horizontal ? 1 : (panelW - 2 * gapGS) / bmChipMin;
    if (bmCols < 1) bmCols = 1;

    pane->x = fileX; pane->y = fileY; pane->width = fileW; pane->height = fileH;
    ResizeScrollbar(fileW, fileH, 0, -ContentH(), 0);
}

// dibuja una Card (9-patch) en (x,y,w,h)
void FileBrowser::TarjetaEn(int X, int Y, int W, int H, const float* bg, const float* bd) {
    if (W <= 0 || H <= 0) return;
    card->Resize(W, H);
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)X, (GLfloat)Y, 0);
    w3dEngine::Color4f(bg[0], bg[1], bg[2], 1.0f); card->RenderObject(false);
    if (bd) { w3dEngine::Color4f(bd[0], bd[1], bd[2], 1.0f); card->RenderBorder(false); }
    w3dEngine::PopMatrix();
}

// ---------------------------------------------------------------- render
void FileBrowser::Render() {
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);

    const float* fondo  = COL(ColorID::background);
    const float* gris   = COL(ColorID::gris);
    const float* grisUI = COL(ColorID::grisUI);
    const float* blanco = COL(ColorID::blanco);
    const float* accent = COL(ColorID::accent);
    const float* accentDark = COL(ColorID::accentDark);
    // hover de los items: gris MEDIO (grisUI era muy blanco)
    float hov[3] = { (fondo[0]+grisUI[0])*0.5f, (fondo[1]+grisUI[1])*0.5f, (fondo[2]+grisUI[2])*0.5f };

    // reflejar el foco de teclado en el resaltado: el boton enfocado se pinta
    // como hover; el accent de archivos/accesos solo se ve en su zona (o con
    // el mouse). Asi solo hay UNA cosa resaltada donde apuntan las flechas.
    if (focoZona == FZ_TOP) {
        Button* nav[4] = { btnBack, btnFwd, btnUp, btnView };
        if (focoIdx >= 0 && focoIdx < 4) nav[focoIdx]->hover = true;
    } else if (focoZona == FZ_BMBTN) {
        (focoIdx == 0 ? btnBmAdd : btnBmDel)->hover = true;
    } else if (focoZona == FZ_BOTTOM) {
        (focoIdx == 0 ? btnCancel : btnAction)->hover = true;
    }
    bool mostrarSelFile = (focoZona == FZ_FILES || focoZona == FZ_NONE);
    bool mostrarSelBm   = (focoZona == FZ_BOOKMARKS || focoZona == FZ_NONE);

    // fondo normal (gris): toda la ventana es una card gris
    TarjetaEn(0, 0, winW, winH, gris, NULL);

    // --- barra superior: nav + toggle + URL ---
    int m = gapGS, navSz = RenglonHeightGS + bordersGS, ny = m;
    bool puedeAtras = histPos > 0, puedeAdel = histPos + 1 < (int)history.size();
    Button* nav[4] = { btnBack, btnFwd, btnUp, btnView };
    int nx = m;
    for (int k = 0; k < 4; k++) {
        Button* b = nav[k];
        b->Resize(navSz); b->width = navSz; b->height = navSz; b->card->Resize(navSz, navSz);
        b->sx = nx; b->sy = ny;
        if (k == 3) b->icon = gridView ? (int)IconType::lista : (int)IconType::cuadricula;
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)nx, (GLfloat)ny, 0); b->Render(); w3dEngine::PopMatrix();
        nx += navSz + m;
    }
    // URL en una Card
    int urlX = nx, urlW = winW - urlX - m;
    TarjetaEn(urlX, ny, urlW, navSz, fondo, grisUI);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(urlX + gapGS + borderGS), (GLfloat)(ny + (navSz - LetterHeightGS) / 2), 0);
    w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
    RenderBitmapText(currentPath, textAlign::left, urlW - 2 * (gapGS + borderGS));
    w3dEngine::PopMatrix();

    // --- panel de bookmarks (card oscura con margen) + items ---
    TarjetaEn(panelX, panelY, panelW, panelH, fondo, NULL);
    int bmH = RenglonHeightGS + bordersGS;
    int botRow = RenglonHeightGS + bordersGS; // fila de +/- abajo
    int bmZona = panelH - botRow - gapGS;
    int bmChipW = (panelW - 2 * gapGS) / bmCols; // ancho de cada acceso
    for (int i = 0; i < (int)bookmarks.size(); i++) {
        int col = i % bmCols, row = i / bmCols;
        int bx = panelX + gapGS + col * bmChipW;
        int by = panelY + gapGS + row * bmH;
        if (by + bmH > panelY + bmZona) break; // no entra (mas filas que alto)
        int cw = bmChipW - gapGS;
        // sin recuadro normal: solo hover (gris) o seleccionado (accent)
        if (mostrarSelBm && i == selBm) TarjetaEn(bx, by, cw, bmH - gapGS / 2, accentDark, accent);
        else if (i == hoverBm) TarjetaEn(bx, by, cw, bmH - gapGS / 2, hov, NULL);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(bx + gapGS), (GLfloat)(by + (bmH - gapGS / 2 - LetterHeightGS) / 2), 0);
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        RenderBitmapText(bookmarks[i].name, textAlign::left, cw - 2 * gapGS);
        w3dEngine::PopMatrix();
    }
    // botones + / -
    int half = (panelW - 2 * gapGS - gapGS) / 2;
    int byBot = panelY + panelH - botRow - gapGS / 2;
    btnBmAdd->Resize(half); btnBmAdd->width = half; btnBmAdd->card->Resize(half, btnBmAdd->height);
    btnBmDel->Resize(half); btnBmDel->width = half; btnBmDel->card->Resize(half, btnBmDel->height);
    btnBmAdd->sx = panelX + gapGS;             btnBmAdd->sy = byBot;
    btnBmDel->sx = panelX + gapGS + half + gapGS; btnBmDel->sy = byBot;
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)btnBmAdd->sx, (GLfloat)byBot, 0); btnBmAdd->Render(); w3dEngine::PopMatrix();
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)btnBmDel->sx, (GLfloat)byBot, 0); btnBmDel->Render(); w3dEngine::PopMatrix();

    // --- area de archivos (card oscura) + entradas (cards) + scroll ---
    TarjetaEn(fileX, fileY, fileW, fileH, fondo, NULL);
    int glY = W3dPantallaAlto - (fileY + fileH);
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(fileX, glY, fileW, fileH);
    int baseY = fileY + borderGS + PosY; // PosY: offset del Scrollable (<=0)
    int rowPitch = RenglonHeightGS + GlobalScale; // 1px de gap entre renglones
    int contentW = fileW - (borderGS + 9 * GlobalScale) - gapGS; // sin pisar el scroll
    for (int i = 0; i < (int)entries.size(); i++) {
        int cx, cy, cw, ch;
        if (gridView) { cx = fileX + borderGS + (i % cols) * cellW; cy = baseY + (i / cols) * cellH; cw = cellW; ch = cellH; }
        else { cx = fileX + borderGS; cy = baseY + i * rowPitch; cw = contentW; ch = RenglonHeightGS; }
        if (cy + ch < fileY || cy > fileY + fileH) continue;

        // SIN recuadro normal: solo aparece al pasar el mouse (gris) o al
        // seleccionar (accent). En grilla el recuadro deja margen; en lista
        // ocupa el renglon (gap de 1px hacia abajo).
        int ex, ey, ew, eh;
        if (gridView) { ex = cx + gapGS / 2; ey = cy + gapGS / 2; ew = cw - gapGS; eh = ch - gapGS; }
        else { ex = cx; ey = cy; ew = cw; eh = ch; }
        if (mostrarSelFile && i == selected) TarjetaEn(ex, ey, ew, eh, accentDark, accent);
        else if (i == hover) TarjetaEn(ex, ey, ew, eh, hov, NULL);

        const float* iconCol = entries[i].isDir ? kFolder : grisUI;
        int iconIdx = IconoEntrada(entries[i]);
        if (gridView) {
            int isz = 26 * GlobalScale, ix = cx + (cw - isz) / 2, iy = cy + gapGS * 2;
            GLshort v[8] = { (GLshort)ix,(GLshort)iy,(GLshort)(ix+isz),(GLshort)iy,(GLshort)ix,(GLshort)(iy+isz),(GLshort)(ix+isz),(GLshort)(iy+isz) };
            w3dEngine::Color4f(iconCol[0], iconCol[1], iconCol[2], 1.0f);
            W3dDrawStrip4(v, IconsUV[iconIdx]->uvs);
            // nombre: hasta 3 renglones, cada uno CENTRADO en la celda
            int textW = cw - 2 * gapGS, maxChars = textW / CharacterWidthGS;
            std::vector<std::string> lines; WrapText(entries[i].name, maxChars, 3, lines);
            int lineH = LetterHeightGS + GlobalScale, ty = iy + isz + gapGS;
            for (size_t l = 0; l < lines.size(); l++) {
                int lw = CharCount(lines[l]) * CharacterWidthGS, lx = cx + (cw - lw) / 2;
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)lx, (GLfloat)(ty + (int)l * lineH), 0);
                w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
                RenderBitmapText(lines[l], textAlign::left, lw + CharacterWidthGS);
                w3dEngine::PopMatrix();
            }
        } else {
            int isz = IconSizeGS, iy = cy + (ch - isz) / 2, ix = ex + gapGS;
            GLshort v[8] = { (GLshort)ix,(GLshort)iy,(GLshort)(ix+isz),(GLshort)iy,(GLshort)ix,(GLshort)(iy+isz),(GLshort)(ix+isz),(GLshort)(iy+isz) };
            w3dEngine::Color4f(iconCol[0], iconCol[1], iconCol[2], 1.0f);
            W3dDrawStrip4(v, IconsUV[iconIdx]->uvs);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(ix + isz + gapGS), (GLfloat)(cy + (ch - LetterHeightGS) / 2), 0);
            w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
            RenderBitmapText(entries[i].name, textAlign::left, ew - (isz + 3 * gapGS));
            w3dEngine::PopMatrix();
        }
    }
    w3dEngine::Disable(w3dEngine::ScissorTest);
    // la barra de scroll REAL (misma textura/comportamiento que el outliner)
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)fileX, (GLfloat)fileY, 0); DibujarScrollbar(pane); w3dEngine::PopMatrix();

    // --- barra inferior: Cancelar (50%) + Accion (50%, verde si valida) ---
    int by = winH - botH + m, botW = (winW - 2 * m - m) / 2;
    if (seleccionValida()) { // listo: verde, texto negro, borde normal
        btnAction->tinte = accent; btnAction->colorTexto = COL(ColorID::negro); btnAction->colorBorde = NULL;
    } else {                 // apagado: gris plano, borde = el mismo fondo (sin remarcar)
        btnAction->tinte = grisUI; btnAction->colorTexto = gris; btnAction->colorBorde = grisUI;
    }
    btnCancel->Resize(botW); btnCancel->width = botW; btnCancel->card->Resize(botW, btnCancel->height);
    btnAction->Resize(botW); btnAction->width = botW; btnAction->card->Resize(botW, btnAction->height);
    btnCancel->sx = m;              btnCancel->sy = by;
    btnAction->sx = m + botW + m;   btnAction->sy = by;
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)m, (GLfloat)by, 0); btnCancel->Render(); w3dEngine::PopMatrix();
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(m + botW + m), (GLfloat)by, 0); btnAction->Render(); w3dEngine::PopMatrix();

    endView();
}

// ---------------------------------------------------------------- input
int FileBrowser::EntryAt(int mx, int my) {
    if (mx < fileX || mx >= fileX + fileW || my < fileY || my >= fileY + fileH) return -1;
    int rel = my - (fileY + borderGS + PosY);
    int idx;
    if (gridView) {
        int col = (mx - fileX - borderGS) / cellW; if (col < 0 || col >= cols) return -1;
        idx = (rel / cellH) * cols + col;
    } else idx = rel / (RenglonHeightGS + GlobalScale);
    return (idx >= 0 && idx < (int)entries.size()) ? idx : -1;
}
int FileBrowser::BookmarkAt(int mx, int my) {
    int bmH = RenglonHeightGS + bordersGS;
    if (mx < panelX + gapGS || mx >= panelX + panelW - gapGS || my < panelY + gapGS) return -1;
    int bmChipW = (panelW - 2 * gapGS) / bmCols;
    int col = (mx - (panelX + gapGS)) / bmChipW; if (col < 0 || col >= bmCols) return -1;
    int row = (my - (panelY + gapGS)) / bmH;
    int idx = row * bmCols + col;
    return (idx >= 0 && idx < (int)bookmarks.size()) ? idx : -1;
}

bool FileBrowser::Click(int mx, int my) {
    if (btnBack->Contains(mx, my)) { Atras(); return true; }
    if (btnFwd->Contains(mx, my)) { Adelante(); return true; }
    if (btnUp->Contains(mx, my)) { Arriba(); return true; }
    if (btnView->Contains(mx, my)) { gridView = !gridView; PosY = 0; return true; }
    if (btnCancel->Contains(mx, my)) { Cerrar(); return true; }
    if (btnAction->Contains(mx, my)) { Aceptar(); return true; }
    if (btnBmAdd->Contains(mx, my)) { AgregarBookmark(); return true; }
    if (btnBmDel->Contains(mx, my)) { QuitarBookmark(); return true; }
    // arrastre del scroll
    if (mouseOverScrollY) { mouseOverScrollYpress = true; gDragMy = my; return true; }
    int bm = BookmarkAt(mx, my);
    if (bm >= 0) { selBm = bm; if (bookmarks[bm].path != currentPath) Navegar(bookmarks[bm].path, true); return true; }
    int e = EntryAt(mx, my);
    if (e >= 0) { AbrirEntrada(e); return true; }
    return true;
}

bool FileBrowser::Motion(int mx, int my) {
    focoZona = FZ_NONE; // el mouse se movio: manda el mouse, no las flechas
    ScrollMouseOver(pane, mx - fileX, my - fileY);
    if (leftMouseDown && mouseOverScrollYpress) {
        ViewPortClickDown = true;
        int d = my - gDragMy; gDragMy = my;
        ScrollY(d);
        return true;
    }
    hover = EntryAt(mx, my);
    hoverBm = BookmarkAt(mx, my);
    btnBack->hover = btnBack->Contains(mx, my); btnFwd->hover = btnFwd->Contains(mx, my);
    btnUp->hover = btnUp->Contains(mx, my); btnView->hover = btnView->Contains(mx, my);
    btnBmAdd->hover = btnBmAdd->Contains(mx, my); btnBmDel->hover = btnBmDel->Contains(mx, my);
    btnCancel->hover = btnCancel->Contains(mx, my); btnAction->hover = btnAction->Contains(mx, my);
    return true;
}

void FileBrowser::Wheel(int delta) {
    MouseWheel = true;
    ScrollY(delta * 6 * GlobalScale); // mismo paso que el outliner
    MouseWheel = false;
}

void FileBrowser::Soltar() { mouseOverScrollYpress = false; ViewPortClickDown = false; }

void FileBrowser::EnsureVisible(int idx) {
    if (idx < 0) return;
    int top, bottom;
    if (gridView) { int c = (cols < 1) ? 1 : cols; int row = idx / c; top = row * cellH; bottom = top + cellH; } // cols=0 antes del 1er Resize -> /0
    else { int rh = RenglonHeightGS + bordersGS; top = idx * rh; bottom = top + rh; }
    if (top + PosY < 0) PosY = -top;
    if (bottom + PosY > fileH) PosY = fileH - bottom;
    if (PosY > 0) PosY = 0; if (MaxPosY > PosY) PosY = MaxPosY;
}

// limpia todos los hovers de mouse (las flechas toman el control del foco)
void FileBrowser::LimpiarHoverMouse() {
    hover = -1; hoverBm = -1;
    btnBack->hover = btnFwd->hover = btnUp->hover = btnView->hover = false;
    btnBmAdd->hover = btnBmDel->hover = false;
    btnCancel->hover = btnAction->hover = false;
}

// mueve el foco con las flechas. Salta entre zonas segun la orientacion:
//  vertical:   TOP <-(arriba) FILES <-(arriba) -> (abajo) BOOKMARKS -> BMBTN -> BOTTOM
//  horizontal: BOOKMARKS <-(izq)/(der)-> FILES ; arriba=TOP, abajo=BOTTOM/BMBTN
void FileBrowser::MoverFoco(int dir) {
    LimpiarHoverMouse();
    // primera tecla (o foco perdido por el mouse): revelar sin moverse aun
    if (focoZona == FZ_NONE) {
        if (!entries.empty()) { focoZona = FZ_FILES; if (selected < 0) selected = 0; EnsureVisible(selected); }
        else { focoZona = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; }
        return;
    }
    // normalizar indices invalidos (p.ej. tras navegar a otra carpeta)
    if (focoZona == FZ_FILES && (selected < 0 || selected >= (int)entries.size())) {
        if (entries.empty()) { focoZona = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; return; }
        selected = 0; EnsureVisible(selected); return;
    }
    if (focoZona == FZ_BOOKMARKS && (selBm < 0 || selBm >= (int)bookmarks.size())) {
        selBm = bookmarks.empty() ? -1 : 0; return;
    }

    int nCols = gridView ? cols : 1; if (nCols < 1) nCols = 1;
    switch (focoZona) {
    case FZ_TOP:
        if (dir == LayoutKey::Left)       { if (focoIdx > 0) focoIdx--; }
        else if (dir == LayoutKey::Right) { if (focoIdx < 3) focoIdx++; }
        else if (dir == LayoutKey::Down) {
            if (horizontal && focoIdx == 0) { focoZona = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; }
            else { focoZona = FZ_FILES; if (selected < 0) selected = 0; EnsureVisible(selected); }
        }
        break;
    case FZ_FILES:
        if (dir == LayoutKey::Up) {
            if (selected - nCols >= 0) { selected -= nCols; EnsureVisible(selected); }
            else { focoZona = FZ_TOP; focoIdx = 0; }            // arriba de todo: barra superior
        } else if (dir == LayoutKey::Down) {
            if (selected + nCols < (int)entries.size()) { selected += nCols; EnsureVisible(selected); }
            else if (horizontal) { focoZona = FZ_BOTTOM; focoIdx = 1; } // a Cancelar/Accion
            else { focoZona = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; } // vertical: a los accesos
        } else if (dir == LayoutKey::Left) {
            if (gridView && (selected % nCols) != 0) { selected--; EnsureVisible(selected); }
            else if (horizontal) { focoZona = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; } // izq: a los accesos
            else if (!gridView && selected > 0) { selected--; EnsureVisible(selected); }
        } else if (dir == LayoutKey::Right) {
            if (gridView && (selected % nCols) != nCols - 1 && selected + 1 < (int)entries.size()) { selected++; EnsureVisible(selected); }
        }
        break;
    case FZ_BOOKMARKS: {
        int bc = (bmCols < 1) ? 1 : bmCols; // 1=lista (horizontal), N=grilla (vertical)
        if (dir == LayoutKey::Up) {
            if (selBm - bc >= 0) selBm -= bc;
            else if (horizontal) { focoZona = FZ_TOP; focoIdx = 0; }   // horizontal: arriba=barra
            else { focoZona = FZ_FILES; if (selected < 0) selected = (int)entries.size() - 1; EnsureVisible(selected); } // vertical: arriba=ficheros
        } else if (dir == LayoutKey::Down) {
            if (selBm + bc < (int)bookmarks.size()) selBm += bc;
            else { focoZona = FZ_BMBTN; focoIdx = 0; }                 // abajo: botones + / -
        } else if (dir == LayoutKey::Left) {
            if (bc > 1 && (selBm % bc) != 0) selBm--;                  // grilla: dentro de la fila
        } else if (dir == LayoutKey::Right) {
            if (bc > 1) { if ((selBm % bc) != bc - 1 && selBm + 1 < (int)bookmarks.size()) selBm++; }
            else { focoZona = FZ_FILES; if (selected < 0) selected = 0; EnsureVisible(selected); } // lista: derecha=ficheros
        }
        break; }
    case FZ_BMBTN:
        if (dir == LayoutKey::Left)       focoIdx = 0;
        else if (dir == LayoutKey::Right) focoIdx = 1;
        else if (dir == LayoutKey::Up)    { focoZona = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : (int)bookmarks.size() - 1; }
        else if (dir == LayoutKey::Down)  { focoZona = FZ_BOTTOM; focoIdx = 0; }
        break;
    case FZ_BOTTOM:
        if (dir == LayoutKey::Left)       focoIdx = 0;
        else if (dir == LayoutKey::Right) focoIdx = 1;
        else if (dir == LayoutKey::Up) {
            if (horizontal) { focoZona = FZ_FILES; if (selected < 0) selected = 0; EnsureVisible(selected); }
            else { focoZona = FZ_BMBTN; focoIdx = 0; }
        }
        break;
    default: break;
    }
}

// Enter / OK: activa el elemento enfocado (abre carpeta, importa archivo,
// pulsa el boton). NO hace falta doble click: OK sobre un archivo lo abre.
void FileBrowser::ActivarFoco() {
    switch (focoZona) {
    case FZ_TOP:
        if (focoIdx == 0) Atras();
        else if (focoIdx == 1) Adelante();
        else if (focoIdx == 2) Arriba();
        else { gridView = !gridView; PosY = 0; }
        break;
    case FZ_BOOKMARKS:
        if (selBm >= 0 && selBm < (int)bookmarks.size()) Navegar(bookmarks[selBm].path, true);
        break;
    case FZ_BMBTN:
        if (focoIdx == 0) AgregarBookmark(); else QuitarBookmark();
        break;
    case FZ_BOTTOM:
        if (focoIdx == 0) Cerrar(); else Aceptar();
        break;
    case FZ_FILES:
    default:
        if (selected >= 0 && selected < (int)entries.size()) {
            if (entries[selected].isDir) AbrirEntrada(selected); // OK en carpeta: entra
            else Aceptar();                                      // OK en archivo: lo abre
        }
        break;
    }
}

bool FileBrowser::Tecla(int tecla) {
    if (tecla == LayoutKey::Up || tecla == LayoutKey::Down ||
        tecla == LayoutKey::Left || tecla == LayoutKey::Right) { MoverFoco(tecla); return true; }
    if (tecla == LayoutKey::Enter)  { ActivarFoco(); return true; }
    if (tecla == LayoutKey::Cancel) { Cerrar(); return true; }   // soft IZQ (Symbian)
    if (tecla == LayoutKey::Accept) { Aceptar(); return true; }  // soft DER = Import/abrir
    return true;
}

void FileBrowser::Cerrar() { if (PopUpActive == this) { PopUpActive = NULL; delete this; } }

// ============================================================================
void AbrirFileBrowser(const std::string& title, const std::string& accionLabel,
                      const std::string& filtro, void (*accept)(const std::string&),
                      bool guardar) {
    if (PopUpActive) PopUpActive->Cerrar();
    FileBrowser* fb = new FileBrowser(title, accionLabel, filtro, accept);
    fb->modoGuardar = guardar;
    PopUpActive = fb;
    fb->Abrir(w3dFileSystem::GetHomeDir());
}
