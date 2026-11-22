#include "io/W3dWeb.h"   // navegador: abrir con su selector, guardar = descargar
#include "W3dLang.h"   // T()
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "FileBrowser.h"
#include "ViewPorts/LayoutInput.h" // LayoutKey
#include "WhiskUI/core/UI.h"
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/draw/glesdraw.h" // W3dDrawStrip4 / W3dScreenHeight
#include "objects/Textures.h"
#include "w3dTexture.h"
#include "io/Font2D.h"   // vista previa de .ttf (rasteriza con la fuente misma)  // miniaturas: DecodeThumbnail / UploadRGBA / DeleteTexture / FreeImage
#ifndef W3D_SYMBIAN
#include "io/Texture2D.h"     // Texture2DDecodablePath (webp -> png para el thumb)
#include "io/Video2DCache.h"  // Video2DFramePng (un frame del video para el thumb)
#endif
#include "variables.h"
#include "io/Libraries.h"         // una libreria por dentro (el proxy W3D: sus escenas, objetos y prefabs)
#include "io/ProjectResources.h"   // el proyecto por dentro: las carpetas de la biblioteca y los recursos de un tipo
#include "io/Thumbnails.h"         // (sus miniaturas: las de la biblioteca)
#include "io/W3dContainer.h"       // (el nombre del .w3d en la barra)
#include "ViewPorts/Notifications.h"
#include <algorithm>
#include <cstring>
#include <map>

extern bool leftMouseDown;
extern bool ViewPortClickDown; // lo usa Scrollable para el verde al arrastrar
extern bool MouseWheel;        // lo usa Scrollable::ScrollY (rama de la rueda)

static const float kFolder[3] = { 0.86f, 0.68f, 0.30f };

static inline const float* COL(ColorID::Enum c) { return ColorList[static_cast<int>(c)]; }

// cuenta caracteres (UTF-8) para centrar (la fuente es monoespaciada)
static int CharCount(const std::string& s) {
    size_t i = 0; int n = 0;
    while (i < s.size()) { UTF8_Char(s.c_str(), i); n++; }
    return n;
}
// 's' termina en 'ext' (case-insensitive)
static bool FinishesIn(const std::string& s, const char* ext) {
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
// es una imagen que podemos previsualizar? (los formatos que stb decodifica)
static bool IsImage(const std::string& name) {
    return FinishesIn(name, ".png") || FinishesIn(name, ".jpg") || FinishesIn(name, ".jpeg") ||
           FinishesIn(name, ".bmp") || FinishesIn(name, ".tga") || FinishesIn(name, ".gif") ||
           FinishesIn(name, ".webp");
}
// es una FUENTE tipografica? (vista previa: el nombre de la letra dibujado con la letra misma,
// como hace el gestor de archivos de Linux)
static bool IsFont(const std::string& name) {
    return FinishesIn(name, ".ttf") || FinishesIn(name, ".otf");
}
// es un VIDEO? (vista previa: un frame extraido con ffmpeg, como hace nautilus)
static bool IsVideo(const std::string& name) {
    return FinishesIn(name, ".mp4") || FinishesIn(name, ".webm") || FinishesIn(name, ".mov") ||
           FinishesIn(name, ".avi") || FinishesIn(name, ".mkv");
}
// icono segun el tipo de archivo
static int IconEntry(const w3dFileSystem::DirEntry& e) {
    if (e.isDir) return (int)IconType::folder;
    if (FinishesIn(e.name, ".obj") || FinishesIn(e.name, ".fbx") || FinishesIn(e.name, ".gltf") || FinishesIn(e.name, ".glb") || FinishesIn(e.name, ".stl")) return (int)IconType::mesh;
    if (IsImage(e.name)) return (int)IconType::photo;
    if (IsVideo(e.name))  return (int)IconType::camera;  // video (mientras carga su preview)
    if (IsFont(e.name)) return (int)IconType::list;      // fuente (mientras carga su preview)
    // los SONIDOS con el parlante (el mismo icono que el objeto Sonido y la vista de sonidos de la biblioteca)
    if (FinishesIn(e.name, ".wav") || FinishesIn(e.name, ".ogg") || FinishesIn(e.name, ".mp3") || FinishesIn(e.name, ".flac") ||
        FinishesIn(e.name, ".opus") || FinishesIn(e.name, ".aac") || FinishesIn(e.name, ".m4a") || FinishesIn(e.name, ".amr")) return (int)IconType::sound;
    return (int)IconType::archive;
}

// MINIATURAS del browser. LIVIANO: pocas por frame (no traba el scroll), la foto full nunca queda entera en RAM
// (DecodeThumbnail la libera adentro), y se LIBERAN TODAS al cambiar de carpeta o cerrar. tex=0 = intento fallido.
namespace { struct FBThumb { unsigned int tex; int w, h; }; }
static std::map<std::string, FBThumb> gFBThumbs;
static int gFBThumbsFrame = 0;          // generadas en el frame actual
static const int kFBThumbMax = 48;      // lado maximo de la miniatura (px)
static const int kFBThumbPerFrame = 2;  // tope de generaciones por frame

static void FBFreeThumbs() {
    for (std::map<std::string, FBThumb>::iterator it = gFBThumbs.begin(); it != gFBThumbs.end(); ++it)
        if (it->second.tex) w3dEngine::DeleteTexture(it->second.tex);
    gFBThumbs.clear();
}
// miniatura _PAlbTN del gestor de fotos de Symbian (N95): <dir>/_PAlbTN/170x128/<archivo>_170x128. Son JPEG SIN
// extension (stb e ICL las decodifican por contenido). Se prefiere ESA a decodificar la foto entera de 5MP. El
// 56x42 es MBM propietario (no stb) -> se ignora. "" si no existe.
static std::string FBThumbPAlbTN(const std::string& imgPath) {
    size_t s = imgPath.find_last_of("/\\");
    std::string dir  = (s == std::string::npos) ? std::string() : imgPath.substr(0, s + 1);
    std::string name = (s == std::string::npos) ? imgPath : imgPath.substr(s + 1);
    std::string cand = dir + "_PAlbTN/170x128/" + name + "_170x128";
    return w3dFileSystem::FileExists(cand) ? cand : std::string();
}
static const FBThumb* FBGetThumb(const std::string& path) {
    std::map<std::string, FBThumb>::iterator it = gFBThumbs.find(path);
    if (it != gFBThumbs.end()) return it->second.tex ? &it->second : 0;
    if (gFBThumbsFrame >= kFBThumbPerFrame) return 0; // se generara en un frame proximo
    gFBThumbsFrame++;
    FBThumb t; t.tex = 0; t.w = t.h = 0;
    unsigned char* rgba = 0; int tw = 0, th = 0;
    std::string pal = FBThumbPAlbTN(path); // JPEG chico del N95, si esta
#ifdef W3D_SYMBIAN
    // Symbian (N95): SOLO via _PAlbTN. Decodificar la foto de 5MP con ICL seria una locura; el JPEG chico si (ICL).
    if (!pal.empty() && w3dEngine::DecodeImage(pal.c_str(), &rgba, &tw, &th) && rgba) {
        t.tex = w3dEngine::UploadRGBA(rgba, tw, th, true); t.w = tw; t.h = th; w3dEngine::FreeImage(rgba);
    }
#else
    // PC/Android/Web: si hay _PAlbTN (copiado del N95) se decodifica ESE (mas rapido); si no, la imagen entera reducida.
    if (IsFont(path)) {
        // vista previa de la TIPOGRAFIA: "AaBb" rasterizado con la fuente misma (stb_truetype)
        if (Font2DThumb(path, kFBThumbMax, &rgba, &tw, &th) && rgba) {
            t.tex = w3dEngine::UploadRGBA(rgba, tw, th, true); t.w = tw; t.h = th; delete[] rgba; rgba = 0;
        }
    } else {
        // stb no decodifica webp ni videos: se normaliza a un png cacheado (ffmpeg).
        // Un video muestra un frame del medio, como hace nautilus.
        std::string src = pal.empty() ? path : pal;
        if (IsVideo(path)) src = Video2DFramePng(path);
        else src = Texture2DDecodablePath(src);
        if (!src.empty() && w3dEngine::DecodeThumbnail(src.c_str(), kFBThumbMax, &rgba, &tw, &th) && rgba) {
            t.tex = w3dEngine::UploadRGBA(rgba, tw, th, true); t.w = tw; t.h = th; w3dEngine::FreeImage(rgba);
        }
    }
#endif
    gFBThumbs[path] = t;
    return t.tex ? &gFBThumbs[path] : 0;
}
// dibuja la miniatura encajada (aspecto) en el cuadro (ix,iy,isz). Rebindea el atlas de iconos al salir.
static void FBDrawThumb(const FBThumb* th, int ix, int iy, int isz) {
    int dw = isz, dh = isz;
    if (th->w >= th->h) { if (th->w) dh = th->h * isz / th->w; } else { if (th->h) dw = th->w * isz / th->h; }
    if (dw < 1) dw = 1; if (dh < 1) dh = 1;
    int ox = ix + (isz - dw) / 2, oy = iy + (isz - dh) / 2;
    GLshort v[8] = { (GLshort)ox,(GLshort)oy, (GLshort)(ox+dw),(GLshort)oy, (GLshort)ox,(GLshort)(oy+dh), (GLshort)(ox+dw),(GLshort)(oy+dh) };
    static const GLfloat uv[8] = { 0,0, 1,0, 0,1, 1,1 };
    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    w3dEngine::BindTexture(th->tex);
    W3dDrawStrip4(v, uv);
    w3dEngine::BindTexture(Textures[0]->iID); // atlas de iconos para los siguientes
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

// Tap vs arrastre en la LISTA (para el tactil): al presionar una entrada NO se abre;
// se guarda y se abre al SOLTAR si fue un tap. Si el dedo se arrastra mas que el umbral,
// se scrollea la lista y ya no se abre. En mouse funciona igual (click sin mover = abre).
static bool gListPressed    = false; // hubo press dentro del area de la lista
static int  gListPressEntry = -1;    // entrada bajo el press (se abre al soltar si fue tap)
static int  gListPressY     = 0;     // Y del press (umbral tap/arrastre)
static int  gListLastY      = 0;     // ultimo Y para el delta de scroll
static bool gListDragging   = false;

// la MINIATURA de la entrada i: del disco (imagenes, videos, fuentes) o, en el proyecto por dentro, la de la biblioteca
static const FBThumb* FBThumbOf(const FileBrowser* fb, int i) {
    if (i < 0 || i >= (int)fb->entries.size() || fb->entries[(size_t)i].isDir) return 0;
    if (!fb->libBrowse.empty()) return 0;   // (una libreria: los iconos de cada cosa)
    if (fb->projectView >= 0) {
        static FBThumb t;   // (la textura es de la biblioteca: no se guarda ni se libera aca)
        if (i >= (int)fb->idsProject.size() || fb->idsProject[(size_t)i].empty()) return 0;
        t.w = t.h = 0;
        t.tex = W3dThumbnail(fb->projectView, fb->idsProject[(size_t)i], &t.w, &t.h);
        if (t.w <= 0 || t.h <= 0) t.w = t.h = 1;
        return t.tex ? &t : 0;
    }
    const std::string& n = fb->entries[(size_t)i].name;
    if (!(IsImage(n) || IsVideo(n) || IsFont(n))) return 0;
    return FBGetThumb(w3dFileSystem::JoinPath(fb->currentPath, n));
}
// el nombre del .w3d abierto (la barra de la ruta del proyecto por dentro)
static std::string ProjectNameUrl() {
    std::string r = W3dContainerMountedPath();
    const size_t b = r.find_last_of("/\\");
    if (b != std::string::npos) r = r.substr(b + 1);
    return r.empty() ? std::string(T("Project")) : r;
}

// ============================================================================

FileBrowser::FileBrowser(const std::string& title, const std::string& newActionLabel,
                         const std::string& filter, void (*accept)(const std::string&))
    : PopUpBase(title) {
    onAccept = accept; actionLabel = newActionLabel; filterExt = filter;
    modeSave = false;
    withName = false; confirming = false; nameX = nameY = nameW = nameH = 0;
    projectView = -1;
    histPos = -1; selected = -1; hover = -1; hoverBm = -1; selBm = -1;
    gridView  = true; horizontal = true;
    focusZone = FZ_NONE; focusIdx = 0;
    topH = botH = 0; panelX = panelY = panelW = panelH = 0;
    fileX = fileY = fileW = fileH = 0; cellW = cellH = cols = 0; bmCols = 1;

    card = new Card(NULL, 10, 10);
    pane = new FBPane();

    btnBack = new Button("", (int)IconType::arrowRight, false); btnBack->iconFlip = 1; btnBack->centered = true;
    btnFwd  = new Button("", (int)IconType::arrowRight, false); btnFwd->centered = true;
    btnUp   = new Button("", (int)IconType::arrow, false);      btnUp->iconFlip = 2; btnUp->centered = true;
    btnRefresh = new Button("", (int)IconType::reload, false); btnRefresh->centered = true;
    btnView = new Button("", (int)IconType::list, false);      btnView->centered = true;
    btnBmAdd = new Button("+", -1, false); btnBmAdd->centered = true;
    btnBmDel = new Button("-", -1, false); btnBmDel->centered = true;
    btnCancel = new Button(T("Cancel"), -1, false); btnCancel->centered = true;
    btnAction = new Button(newActionLabel, -1, false); btnAction->centered = true;
    btnAction->textColor = COL(ColorID::black); // texto negro sobre el verde
    btnOverwrite = new Button(T("Overwrite"), -1, false); btnOverwrite->centered = true;
    btnOverwrite->textColor = COL(ColorID::black);
    btnNotOverwrite = new Button(T("Cancel"), -1, false); btnNotOverwrite->centered = true;

    w3dFileSystem::GetBookmarks(bookmarks);
}

FileBrowser::~FileBrowser() {
    delete card; delete pane;
    delete btnBack; delete btnFwd; delete btnUp; delete btnRefresh; delete btnView;
    delete btnBmAdd; delete btnBmDel; delete btnCancel; delete btnAction;
    delete btnOverwrite; delete btnNotOverwrite;
    if (g_activeTextField == &fileName) g_activeTextField = NULL;   // (la barra del nombre se va con el explorador)
}

bool FileBrowser::fileSelected() const {
    return selected >= 0 && selected < (int)entries.size() && !entries[selected].isDir;
}

// MODO ELEGIR CARPETA: el boton dice QUE va a pasar. Parado en una carpeta (sin archivo
// seleccionado) confirma ESA carpeta -> "Usar carpeta actual"; si tocaste un archivo pasa
// a la accion del caller (Guardar/Elegir = sobrescribir ese archivo).
std::string FileBrowser::LabelAction() const {
    if (withName) return actionLabel;
    if (!libBrowse.empty()) {   // (la libreria: dice QUE se va a agregar)
        std::string e = PickedLib();
        if (e.empty()) return actionLabel;
        const size_t d = e.find_last_of("/#:");
        return actionLabel + ": " + (d == std::string::npos ? e : e.substr(d + 1));
    }
    if (modeSave && !fileSelected()) return std::string(T(kUseCurrentFolder));
    return actionLabel;
}

// el nombre tipeado, limpio: solo el ultimo tramo (sin carpetas) y sin espacios al borde
static std::string BarName(const std::string& t) {
    std::string n = t;
    const size_t b = n.find_last_of("/\\");
    if (b != std::string::npos) n = n.substr(b + 1);
    while (!n.empty() && (n[0] == ' ' || n[0] == '\t')) n.erase(0, 1);
    while (!n.empty() && (n[n.size() - 1] == ' ' || n[n.size() - 1] == '\t')) n.erase(n.size() - 1);
    return n;
}
bool FileBrowser::selectionValid() const {
    if (withName) return !BarName(fileName.text).empty();
    if (!libBrowse.empty()) return !PickedLib().empty();
    if (modeSave) return true; // guardar: el boton "elige la carpeta actual" (siempre activo)
    return fileSelected();
}

bool FileBrowser::PassesFilter(const w3dFileSystem::DirEntry& e) const {
    if (e.isDir) return true;
    if (filterExt.empty()) return true;
    // filterExt = lista de extensiones separadas por espacio (".png .jpg ...")
    size_t pos = 0;
    while (pos < filterExt.size()) {
        size_t sp = filterExt.find(' ', pos);
        std::string ext = (sp == std::string::npos) ? filterExt.substr(pos) : filterExt.substr(pos, sp - pos);
        if (!ext.empty() && FinishesIn(e.name, ext.c_str())) return true;
        if (sp == std::string::npos) break;
        pos = sp + 1;
    }
    return false;
}

void FileBrowser::Open(const std::string& startDir) {
    std::string dir = startDir.empty() ? w3dFileSystem::GetHomeDir() : startDir;
    history.clear(); histPos = -1; Navigate(dir, true);
}
// las sub-carpetas de la biblioteca de 'dir' y los recursos del tipo 'vista' que estan en ella (las partes de otro, no)
static void FBListProject(int view, const std::string& dir, std::vector<w3dFileSystem::DirEntry>& out,
                             std::vector<std::string>& ids) {
    out.clear(); ids.clear();
    std::vector<std::string> folders;
    W3dFoldersAll(folders);
    std::vector<std::string> children;
    for (size_t i = 0; i < folders.size(); i++) if (!folders[i].empty() && W3dFolderParent(folders[i]) == dir) children.push_back(W3dFolderLeaf(folders[i]));
    std::sort(children.begin(), children.end());
    children.erase(std::unique(children.begin(), children.end()), children.end());
    for (size_t i = 0; i < children.size(); i++) {
        w3dFileSystem::DirEntry e; e.name = children[i]; e.isDir = true;
        out.push_back(e); ids.push_back(std::string());
    }
    std::vector<W3dResourceItem> items;
    W3dViewRecList(view, items);
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i].folder != dir || !items[i].parent.empty()) continue;
        if (W3dViewRecLocation(view, items[i].id) == 1) continue;   // (lo externo no esta ADENTRO del .w3d)
        w3dFileSystem::DirEntry e; e.name = items[i].name; e.isDir = false;
        out.push_back(e); ids.push_back(items[i].id);
    }
}
// UNA LIBRERIA POR DENTRO: en la raiz sus escenas (carpetas) y sus prefabs; adentro de una escena, sus objetos (los que
// tienen hijos tambien son carpetas). Los objetos llevan el CUBO (no se confunden con una malla); luces y camaras, el suyo
static void FBListLib(const std::string& lib, const std::string& dir, std::vector<w3dFileSystem::DirEntry>& out,
                             std::vector<int>& icons) {
    out.clear(); icons.clear();
    if (dir.empty()) {
        std::vector<std::string> is, ps;
        W3dLibsElements(lib, W3D_LIB_SCENE, is);
        W3dLibsElements(lib, W3D_LIB_PREFAB, ps);
        for (size_t i = 0; i < is.size(); i++) { w3dFileSystem::DirEntry e; e.name = is[i]; e.isDir = true; out.push_back(e); icons.push_back((int)IconType::camera); }
        for (size_t i = 0; i < ps.size(); i++) { w3dFileSystem::DirEntry e; e.name = ps[i]; e.isDir = false; out.push_back(e); icons.push_back((int)IconType::prefab); }
        return;
    }
    const size_t b = dir.find('/');
    const std::string scene = (b == std::string::npos) ? dir : dir.substr(0, b);
    std::vector<W3dLibObject> os;
    W3dLibsSceneObjects(lib, scene, b == std::string::npos ? std::string() : dir.substr(b + 1), os);
    for (size_t i = 0; i < os.size(); i++) {
        w3dFileSystem::DirEntry e; e.name = os[i].name; e.isDir = os[i].withChildren;
        out.push_back(e);
        const std::string& t = os[i].type;
        icons.push_back(t == "light" ? (int)IconType::light : t == "camera" ? (int)IconType::camera :
                         t == "armature" || t == "armature" ? (int)IconType::armature : (int)IconType::object);
    }
}
std::string FileBrowser::PickedLib() const {
    if (libBrowse.empty()) return std::string();
    const size_t b = currentPath.find('/');
    const std::string scene = (b == std::string::npos) ? currentPath : currentPath.substr(0, b);
    const std::string rest = (b == std::string::npos) ? std::string() : currentPath.substr(b + 1);
    if (fileSelected()) {
        const std::string& n = entries[(size_t)selected].name;
        if (currentPath.empty()) return "prefab:" + n;
        return scene + "#" + (rest.empty() ? n : rest + "/" + n);
    }
    if (currentPath.empty()) return std::string();   // (en la raiz sin nada elegido no hay que agregar)
    return rest.empty() ? "scene:" + scene : scene + "#" + rest;
}
std::string FileBrowser::Join(const std::string& dir, const std::string& name) const {
    if (!Virtual()) return w3dFileSystem::JoinPath(dir, name);
    return dir.empty() ? name : dir + "/" + name;
}
void FileBrowser::Reload() {
    if (!libBrowse.empty()) { FBListLib(libBrowse, currentPath, entries, iconsEntry); idsProject.clear(); selected = -1; PosY = 0; return; }
    if (projectView >= 0) { FBListProject(projectView, currentPath, entries, idsProject); selected = -1; PosY = 0; return; }
    std::vector<w3dFileSystem::DirEntry> all;
    w3dFileSystem::ListDir(currentPath, all);
    entries.clear();
    for (size_t i = 0; i < all.size(); i++) if (PassesFilter(all[i])) entries.push_back(all[i]);
    selected = -1; PosY = 0;
}
void FileBrowser::Update() {
    const std::string picked = (selected >= 0 && selected < (int)entries.size()) ? entries[selected].name : std::string();
    const int y = PosY;
    FBFreeThumbs();   // (las miniaturas se rehacen: una imagen editada afuera se ve como quedo)
    Reload();         // (deja selected = -1 y el scroll arriba)
    for (int i = 0; i < (int)entries.size(); i++) if (!picked.empty() && entries[i].name == picked) { selected = i; break; }
    const int maxY = ContentH() - fileH;
    PosY = (y < 0) ? 0 : (maxY > 0 && y > maxY) ? maxY : (maxY <= 0 ? 0 : y);
}
void FileBrowser::Navigate(const std::string& dir, bool pushHistory) {
    std::vector<w3dFileSystem::DirEntry> test;
    if (!Virtual() && !w3dFileSystem::ListDir(dir, test)) return;
    FBFreeThumbs(); // cambiar de carpeta libera TODAS las miniaturas (no acumular RAM/VRAM)
    currentPath = dir;
    if (pushHistory) {
        while ((int)history.size() > histPos + 1) history.pop_back();
        history.push_back(dir); histPos = (int)history.size() - 1;
    }
    Reload(); // deja selected = -1
    // ENTRAR a una carpeta (OK sobre carpeta / bookmark / arriba): deja el 1er
    // item ya seleccionado, asi no hay que bajar una vez. ATRAS/ADELANTE
    // (pushHistory=false) NO: el foco se queda en la barra de navegacion.
    // (No se llama EnsureVisible: el item 0 esta arriba y PosY=0 ya lo muestra;
    // ademas Navegar corre ANTES del 1er Resize -> cols=0 crasheaba en /0.)
    // En MODO ELEGIR CARPETA no se auto-selecciona: si el item 0 fuera un ARCHIVO el
    // boton verde pasaria solo a "sobrescribir ese" en vez de "Usar carpeta actual"
    // (y confirmar guardaria en el archivo equivocado). Las flechas siguen entrando
    // a la zona de archivos por MoveFocus.
    if (pushHistory && !entries.empty() && !modeSave) {
        focusZone = FZ_FILES; selected = 0;
    }
}
void FileBrowser::Back()    { if (histPos > 0) { histPos--; Navigate(history[histPos], false); } }
void FileBrowser::Forward() { if (histPos + 1 < (int)history.size()) { histPos++; Navigate(history[histPos], false); } }
// con ruta sin '/' ParentPath ahora devuelve "" (carpeta actual): no navegamos a una ruta vacia
void FileBrowser::Up()   {
    if (Virtual()) { if (!currentPath.empty()) Navigate(W3dFolderParent(currentPath), true); return; }   // (hasta la raiz del proyecto)
    std::string p = w3dFileSystem::ParentPath(currentPath); if (!p.empty() && p != currentPath) Navigate(p, true);
}
void FileBrowser::OpenEntry(int idx) {
    if (idx < 0 || idx >= (int)entries.size()) return;
    if (entries[idx].isDir) {
        Navigate(Join(currentPath, entries[idx].name), true);
    } else if (selected == idx) {
        // segundo click sobre el archivo YA seleccionado = doble click =
        // importar / cargar (no hace falta ir al boton verde de abajo)
        Accept();
    } else {
        selected = idx; // primer click: solo seleccionar
        if (withName) fileName.SetText(entries[idx].name);   // (guardar como: su nombre, para pisarlo o cambiarlo)
    }
}
bool FileBrowser::EnterName(const std::string& n) {
    for (int i = 0; i < (int)entries.size(); i++) if (entries[(size_t)i].name == n) { OpenEntry(i); return true; }
    return false;
}
void FileBrowser::Accept() {
    std::string full;
    if (!libBrowse.empty()) {
        full = PickedLib();
        if (full.empty()) return;
        void (*cb)(const std::string&) = onAccept;
        Close(); if (cb) cb(full);
        return;
    }
    if (withName) {
        // GUARDAR COMO: la carpeta + el nombre de la barra (+ la extension, si no la tiene)
        std::string n = BarName(fileName.text);
        if (n.empty()) return;
        if (!extName.empty()) {
            std::string under = n; for (size_t i = 0; i < under.size(); i++) under[i] = (char)tolower((unsigned char)under[i]);
            std::string e = extName; for (size_t i = 0; i < e.size(); i++) e[i] = (char)tolower((unsigned char)e[i]);
            if (!FinishesIn(under, e.c_str())) n += extName;
        }
        full = Join(currentPath, n);
        // ADENTRO DEL PROYECTO: un recurso de ese tipo que ya se llama asi no se pisa (sus usuarios lo perderian): se avisa
        // y se puede cambiar el nombre
        if (projectView >= 0) {
            std::vector<W3dResourceItem> items;
            W3dViewRecList(projectView, items);
            std::string under = n; for (size_t i = 0; i < under.size(); i++) under[i] = (char)tolower((unsigned char)under[i]);
            for (size_t i = 0; i < items.size(); i++) {
                if (W3dViewRecLocation(projectView, items[i].id) == 1) continue;   // (uno externo no ocupa el nombre adentro)
                std::string o = items[i].name; for (size_t k = 0; k < o.size(); k++) o[k] = (char)tolower((unsigned char)o[k]);
                if (o == under) { Notify(std::string(T("A resource with that name already exists")) + ": " + n, true); return; }
            }
            if (g_activeTextField == &fileName) g_activeTextField = NULL;
            void (*cb)(const std::string&) = onAccept;
            Close(); if (cb) cb(full);
            return;
        }
        // ya existe: primero el cartel (Sobrescribir / Cancelar)
        if (!confirming && w3dFileSystem::FileExists(full)) {
            confirming = true; pathOverwrite = full;
            if (g_activeTextField == &fileName) g_activeTextField = NULL;
            return;
        }
        confirming = false;
        if (g_activeTextField == &fileName) g_activeTextField = NULL;
        void (*cb)(const std::string&) = onAccept;
        Close(); if (cb) cb(full);
        return;
    }
    if (modeSave) {
        // guardar: si hay un archivo sel -> su ruta (sobrescribir); si no -> la carpeta actual
        if (fileSelected())
            full = w3dFileSystem::JoinPath(currentPath, entries[selected].name);
        else
            full = currentPath; // la carpeta destino (el caller le agrega el nombre
                                // con W3dOutputPath)
    } else {
        if (!selectionValid()) return;
        full = w3dFileSystem::JoinPath(currentPath, entries[selected].name);
    }
    void (*cb)(const std::string&) = onAccept;
    Close(); if (cb) cb(full);
}
void FileBrowser::AddBookmark() {
    if (Virtual()) return;   // (el proyecto por dentro: sus accesos no son del disco)
    w3dFileSystem::Bookmark b; size_t s = currentPath.find_last_of('/');
    b.name = (s == std::string::npos) ? currentPath : currentPath.substr(s + 1);
    if (b.name.empty()) b.name = currentPath;
    b.path = currentPath; b.user = true; // del usuario -> se persiste
    for (size_t i = 0; i < bookmarks.size(); i++) if (bookmarks[i].path == b.path) return; // ya esta
    bookmarks.push_back(b);
    w3dFileSystem::SaveUserBookmarks(bookmarks); // queda guardado en disco (entre sesiones)
}
void FileBrowser::RemoveBookmark() {
    if (Virtual()) return;
    if (selBm >= 0 && selBm < (int)bookmarks.size()) {
        if (!bookmarks[selBm].user) return; // Home/drives/carpetas NO se borran (se regeneran)
        bookmarks.erase(bookmarks.begin() + selBm); selBm = -1;
        w3dFileSystem::SaveUserBookmarks(bookmarks);
    }
}

int FileBrowser::ContentH() const {
    int n = (int)entries.size();
    if (gridView) { int c = (cols < 1) ? 1 : cols; int rows = (n + c - 1) / c; return rows * cellH + 2 * marginGS; }
    return n * (RowHeightGS + GlobalScale) + 2 * marginGS; // renglones casi pegados (1px)
}

// ---------------------------------------------------------------- layout
void FileBrowser::Layout() {
    x = 0; y = 0; popUpWindow->Resize(winW, winH);
    horizontal = (winW >= winH);
    int barH = RowHeightGS + bordersGS, m = gapGS;
    topH = barH + 2 * m; botH = barH + 2 * m;
    if (withName) botH += barH + m;   // (guardar como: la barra del NOMBRE arriba de los botones)
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
void FileBrowser::CardIn(int X, int Y, int W, int H, const float* bg, const float* bd) {
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

    const float* background = COL(ColorID::background);
    const float* gray       = COL(ColorID::gray);
    const float* grayUI     = COL(ColorID::grayUI);
    const float* white      = COL(ColorID::white);
    const float* accent     = COL(ColorID::accent);
    const float* accentDark = COL(ColorID::accentDark);
    // hover de los items: gris MEDIO (grayUI era muy blanco)
    float hov[3] = { (background[0]+grayUI[0])*0.5f, (background[1]+grayUI[1])*0.5f, (background[2]+grayUI[2])*0.5f };

    // reflejar el foco de teclado en el resaltado: el boton enfocado se pinta
    // como hover; el accent de archivos/accesos solo se ve en su zona (o con
    // el mouse). Asi solo hay UNA cosa resaltada donde apuntan las flechas.
    if (focusZone == FZ_TOP) {
        Button* nav[5] = { btnBack, btnFwd, btnUp, btnRefresh, btnView };
        if (focusIdx >= 0 && focusIdx < 5) nav[focusIdx]->hover = true;
    } else if (focusZone == FZ_BMBTN) {
        (focusIdx == 0 ? btnBmAdd : btnBmDel)->hover = true;
    } else if (focusZone == FZ_BOTTOM) {
        (focusIdx == 0 ? btnCancel : btnAction)->hover = true;
    }
    bool showSelFile = (focusZone == FZ_FILES || focusZone == FZ_NONE);
    bool showSelBm   = (focusZone == FZ_BOOKMARKS || focusZone == FZ_NONE);

    // fondo normal (gris): toda la ventana es una card gris
    CardIn(0, 0, winW, winH, gray, NULL);

    // --- barra superior: nav + toggle + URL ---
    int m = gapGS, navSz = RowHeightGS + bordersGS, ny = m;
    bool canBack = histPos > 0, canForward = histPos + 1 < (int)history.size();
    Button* nav[5] = { btnBack, btnFwd, btnUp, btnRefresh, btnView };
    int nx = m;
    for (int k = 0; k < 5; k++) {
        Button* b = nav[k];
        b->Resize(navSz); b->width = navSz; b->height = navSz; b->card->Resize(navSz, navSz);
        b->sx = nx; b->sy = ny;
        if (k == 4) b->icon = gridView ? (int)IconType::list : (int)IconType::grid;
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)nx, (GLfloat)ny, 0); b->Render(); w3dEngine::PopMatrix();
        nx += navSz + m;
    }
    // URL en una Card
    int urlX = nx, urlW = winW - urlX - m;
    CardIn(urlX, ny, urlW, navSz, background, grayUI);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(urlX + gapGS + borderGS), (GLfloat)(ny + (navSz - LetterHeightGS) / 2), 0);
    w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
    RenderBitmapText(!libBrowse.empty() ? libBrowse + ".w3d/" + currentPath : projectView >= 0 ? ProjectNameUrl() + "/" + currentPath : currentPath,
                     textAlign::left, urlW - 2 * (gapGS + borderGS));
    w3dEngine::PopMatrix();

    // --- panel de bookmarks (card oscura con margen) + items ---
    CardIn(panelX, panelY, panelW, panelH, background, NULL);
    int bmH = RowHeightGS + bordersGS;
    int botRow = RowHeightGS + bordersGS; // fila de +/- abajo
    int bmZone = panelH - botRow - gapGS;
    int bmChipW = (panelW - 2 * gapGS) / bmCols; // ancho de cada acceso
    for (int i = 0; i < (int)bookmarks.size(); i++) {
        int col = i % bmCols, row = i / bmCols;
        int bx = panelX + gapGS + col * bmChipW;
        int by = panelY + gapGS + row * bmH;
        if (by + bmH > panelY + bmZone) break; // no entra (mas filas que alto)
        int cw = bmChipW - gapGS;
        // sin recuadro normal: solo hover (gris) o seleccionado (accent)
        if (showSelBm && i == selBm) CardIn(bx, by, cw, bmH - gapGS / 2, accentDark, accent);
        else if (i == hoverBm) CardIn(bx, by, cw, bmH - gapGS / 2, hov, NULL);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(bx + gapGS), (GLfloat)(by + (bmH - gapGS / 2 - LetterHeightGS) / 2), 0);
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        RenderBitmapText(T(bookmarks[i].name.c_str()), textAlign::left, cw - 2 * gapGS);
        w3dEngine::PopMatrix();
    }
    // botones + / -
    int half = (panelW - 2 * gapGS - gapGS) / 2;
    int byBot = panelY + panelH - botRow - gapGS / 2;
    btnBmAdd->Resize(half); btnBmAdd->width = half; btnBmAdd->card->Resize(half, btnBmAdd->height);
    btnBmDel->Resize(half); btnBmDel->width = half; btnBmDel->card->Resize(half, btnBmDel->height);
    btnBmAdd->sx = panelX + gapGS;             btnBmAdd->sy = byBot;
    btnBmDel->sx = panelX + gapGS + half + gapGS; btnBmDel->sy = byBot;
    if (!Virtual()) {   // (el proyecto por dentro no guarda accesos)
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)btnBmAdd->sx, (GLfloat)byBot, 0); btnBmAdd->Render(); w3dEngine::PopMatrix();
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)btnBmDel->sx, (GLfloat)byBot, 0); btnBmDel->Render(); w3dEngine::PopMatrix();
    }

    // --- area de archivos (card oscura) + entradas (cards) + scroll ---
    CardIn(fileX, fileY, fileW, fileH, background, NULL);
    int glY = W3dScreenHeight - (fileY + fileH);
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(fileX, glY, fileW, fileH);
    int baseY = fileY + borderGS + PosY; // PosY: offset del Scrollable (<=0)
    int rowPitch = RowHeightGS + GlobalScale; // 1px de gap entre renglones
    int contentW = fileW - (borderGS + 9 * GlobalScale) - gapGS; // sin pisar el scroll
    gFBThumbsFrame = 0; // limite de miniaturas nuevas por frame (se rearma cada frame)
    for (int i = 0; i < (int)entries.size(); i++) {
        int cx, cy, cw, ch;
        if (gridView) { cx = fileX + borderGS + (i % cols) * cellW; cy = baseY + (i / cols) * cellH; cw = cellW; ch = cellH; }
        else { cx = fileX + borderGS; cy = baseY + i * rowPitch; cw = contentW; ch = RowHeightGS; }
        if (cy + ch < fileY || cy > fileY + fileH) continue;

        // SIN recuadro normal: solo aparece al pasar el mouse (gris) o al
        // seleccionar (accent). En grilla el recuadro deja margen; en lista
        // ocupa el renglon (gap de 1px hacia abajo).
        int ex, ey, ew, eh;
        if (gridView) { ex = cx + gapGS / 2; ey = cy + gapGS / 2; ew = cw - gapGS; eh = ch - gapGS; }
        else { ex = cx; ey = cy; ew = cw; eh = ch; }
        if (showSelFile && i == selected) CardIn(ex, ey, ew, eh, accentDark, accent);
        else if (i == hover) CardIn(ex, ey, ew, eh, hov, NULL);

        const float* iconCol = entries[i].isDir ? kFolder : grayUI;
        int iconIdx = (i < (int)iconsEntry.size() && iconsEntry[(size_t)i] >= 0) ? iconsEntry[(size_t)i] : IconEntry(entries[i]);
        if (gridView) {
            int isz = 26 * GlobalScale, ix = cx + (cw - isz) / 2, iy = cy + gapGS * 2;
            const FBThumb* thmb = FBThumbOf(this, i);
            if (thmb) FBDrawThumb(thmb, ix, iy, isz);
            else
            {
                GLshort v[8] = { (GLshort)ix,(GLshort)iy,(GLshort)(ix+isz),(GLshort)iy,(GLshort)ix,(GLshort)(iy+isz),(GLshort)(ix+isz),(GLshort)(iy+isz) };
                w3dEngine::Color4f(iconCol[0], iconCol[1], iconCol[2], 1.0f);
                W3dDrawStrip4(v, IconsUV[iconIdx]->uvs);
            }
            // nombre: hasta 3 renglones, cada uno CENTRADO en la celda
            int textW = cw - 2 * gapGS, maxChars = textW / CharacterWidthGS;
            std::vector<std::string> lines; WrapText(entries[i].name, maxChars, 3, lines);
            int lineH = LetterHeightGS + GlobalScale, ty = iy + isz + gapGS;
            for (size_t l = 0; l < lines.size(); l++) {
                int lw = CharCount(lines[l]) * CharacterWidthGS, lx = cx + (cw - lw) / 2;
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)lx, (GLfloat)(ty + (int)l * lineH), 0);
                w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
                RenderBitmapText(lines[l], textAlign::left, lw + CharacterWidthGS);
                w3dEngine::PopMatrix();
            }
        } else {
            int isz = IconSizeGS, iy = cy + (ch - isz) / 2, ix = ex + gapGS;
            const FBThumb* thmb = FBThumbOf(this, i);
            if (thmb) FBDrawThumb(thmb, ix, iy, isz);
            else
            {
                GLshort v[8] = { (GLshort)ix,(GLshort)iy,(GLshort)(ix+isz),(GLshort)iy,(GLshort)ix,(GLshort)(iy+isz),(GLshort)(ix+isz),(GLshort)(iy+isz) };
                w3dEngine::Color4f(iconCol[0], iconCol[1], iconCol[2], 1.0f);
                W3dDrawStrip4(v, IconsUV[iconIdx]->uvs);
            }
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(ix + isz + gapGS), (GLfloat)(cy + (ch - LetterHeightGS) / 2), 0);
            w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
            RenderBitmapText(entries[i].name, textAlign::left, ew - (isz + 3 * gapGS));
            w3dEngine::PopMatrix();
        }
    }
    w3dEngine::Disable(w3dEngine::ScissorTest);
    // la barra de scroll REAL (misma textura/comportamiento que el outliner)
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)fileX, (GLfloat)fileY, 0); DrawScrollbar(pane); w3dEngine::PopMatrix();

    // --- GUARDAR COMO: la barra "Nombre de archivo" (la caja es una caja de texto: tocarla la enfoca) ---
    if (withName) {
        const int barH = RowHeightGS + bordersGS, ny = winH - botH + m;
        const std::string et = std::string(T("File name")) + ":";
        const int ew = (CharCount(et) + 1) * CharacterWidthGS;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)m, (GLfloat)(ny + (barH - LetterHeightGS) / 2), 0);
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        RenderBitmapText(et, textAlign::left, ew);
        w3dEngine::PopMatrix();
        nameX = m + ew; nameY = ny; nameW = winW - nameX - m; nameH = barH;
        const bool focus = (g_activeTextField == &fileName);
        CardIn(nameX, nameY, nameW, nameH, COL(ColorID::black), focus ? accent : grayUI);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(nameX + gapGS * 2), (GLfloat)(nameY + (barH - LetterHeightGS) / 2), 0);
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        RenderTextField(fileName, focus, 0, nameW - gapGS * 4);
        w3dEngine::PopMatrix();
    }
    // --- barra inferior: Cancelar (50%) + Accion (50%, verde si valida) ---
    int by = winH - botH + m + (withName ? RowHeightGS + bordersGS + m : 0), botW = (winW - 2 * m - m) / 2;
    if (modeSave) btnAction->text = LabelAction(); // "Usar carpeta actual" / la accion
    if (selectionValid()) { // listo: verde, texto negro, borde normal
        btnAction->tint = accent; btnAction->textColor = COL(ColorID::black); btnAction->borderColor = NULL;
    } else {                 // apagado: gris plano, borde = el mismo fondo (sin remarcar)
        btnAction->tint = grayUI; btnAction->textColor = gray; btnAction->borderColor = grayUI;
    }
    btnCancel->Resize(botW); btnCancel->width = botW; btnCancel->card->Resize(botW, btnCancel->height);
    btnAction->Resize(botW); btnAction->width = botW; btnAction->card->Resize(botW, btnAction->height);
    btnCancel->sx = m;              btnCancel->sy = by;
    btnAction->sx = m + botW + m;   btnAction->sy = by;
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)m, (GLfloat)by, 0); btnCancel->Render(); w3dEngine::PopMatrix();
    w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(m + botW + m), (GLfloat)by, 0); btnAction->Render(); w3dEngine::PopMatrix();

    // --- EL CARTEL: "Sobrescribir archivo?" con el nombre, Sobrescribir (verde) / Cancelar ---
    if (confirming) {
        const int barH = RowHeightGS + bordersGS;
        std::string arch = pathOverwrite; { const size_t b = arch.find_last_of("/\\"); if (b != std::string::npos) arch = arch.substr(b + 1); }
        const std::string tit = T("Overwrite file?");
        int cw = (CharCount(arch) > CharCount(tit) ? CharCount(arch) : CharCount(tit)) * CharacterWidthGS + 6 * gapGS;
        if (cw < 200 * GlobalScale) cw = 200 * GlobalScale;
        if (cw > winW - 4 * m) cw = winW - 4 * m;
        const int ch = 2 * (LetterHeightGS + gapGS) + barH + 5 * gapGS;
        const int cx = (winW - cw) / 2, cy = (winH - ch) / 2;
        CardIn(cx, cy, cw, ch, background, accent);
        w3dEngine::Color4f(white[0], white[1], white[2], 1.0f);
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(cx + 3 * gapGS), (GLfloat)(cy + 2 * gapGS), 0);
        RenderBitmapText(tit, textAlign::left, cw - 6 * gapGS); w3dEngine::PopMatrix();
        w3dEngine::Color4f(grayUI[0], grayUI[1], grayUI[2], 1.0f);   // (el nombre del archivo, en el gris claro de la UI)
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)(cx + 3 * gapGS), (GLfloat)(cy + 3 * gapGS + LetterHeightGS), 0);
        RenderBitmapText(arch, textAlign::left, cw - 6 * gapGS); w3dEngine::PopMatrix();
        const int bw = (cw - 3 * 2 * gapGS) / 2, bby = cy + ch - barH - 2 * gapGS;
        btnNotOverwrite->Resize(bw); btnNotOverwrite->width = bw; btnNotOverwrite->card->Resize(bw, btnNotOverwrite->height);
        btnOverwrite->Resize(bw); btnOverwrite->width = bw; btnOverwrite->card->Resize(bw, btnOverwrite->height);
        btnOverwrite->tint = accent; btnOverwrite->textColor = COL(ColorID::black);
        btnNotOverwrite->sx = cx + 2 * gapGS; btnNotOverwrite->sy = bby;
        btnOverwrite->sx = cx + cw - 2 * gapGS - bw; btnOverwrite->sy = bby;
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)btnNotOverwrite->sx, (GLfloat)bby, 0); btnNotOverwrite->Render(); w3dEngine::PopMatrix();
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)btnOverwrite->sx, (GLfloat)bby, 0); btnOverwrite->Render(); w3dEngine::PopMatrix();
    }

    endView();
}

// ---------------------------------------------------------------- input
int FileBrowser::EntryAt(int mx, int my) {
    if (mx < fileX || mx >= fileX + fileW || my < fileY || my >= fileY + fileH) return -1;
    int rel = my - (fileY + borderGS + PosY);
    if (rel < 0) return -1;   // franja del borde superior: -3/40 daba 0 y "clickeaba" el item 0
    int idx;
    if (gridView) {
        int col = (mx - fileX - borderGS) / cellW; if (col < 0 || col >= cols) return -1;
        idx = (rel / cellH) * cols + col;
    } else idx = rel / (RowHeightGS + GlobalScale);
    return (idx >= 0 && idx < (int)entries.size()) ? idx : -1;
}
int FileBrowser::BookmarkAt(int mx, int my) {
    int bmH = RowHeightGS + bordersGS;
    if (mx < panelX + gapGS || mx >= panelX + panelW - gapGS || my < panelY + gapGS) return -1;
    // borde INFERIOR (misma bmZone que el render): sin esto se podian clickear
    // bookmarks NO dibujados (o el hueco antes de los botones +/-)
    { int botRow = RowHeightGS + bordersGS;
      if (my >= panelY + panelH - botRow - gapGS) return -1; }
    int bmChipW = (panelW - 2 * gapGS) / bmCols;
    int col = (mx - (panelX + gapGS)) / bmChipW; if (col < 0 || col >= bmCols) return -1;
    int row = (my - (panelY + gapGS)) / bmH;
    int idx = row * bmCols + col;
    return (idx >= 0 && idx < (int)bookmarks.size()) ? idx : -1;
}

bool FileBrowser::Click(int mx, int my) {
    // el cartel de sobrescribir: solo sus dos botones
    if (confirming) {
        if (btnOverwrite->Contains(mx, my)) { Accept(); return true; }
        if (btnNotOverwrite->Contains(mx, my)) {   // (Cancelar: de vuelta a la barra, con el nombre elegido para cambiarlo)
            confirming = false; TextFieldFocus(&fileName); fileName.SelectAll(); return true;
        }
        return true;
    }
    // la caja del nombre: la enfoca (adentro de ella, el cursor lo pone controles: TextFieldMouseDown)
    if (withName && mx >= nameX && mx < nameX + nameW && my >= nameY && my < nameY + nameH) {
        if (g_activeTextField != &fileName) { TextFieldFocus(&fileName); fileName.SelectAll(); }
        return true;
    }
    if (withName && g_activeTextField == &fileName) g_activeTextField = NULL;   // (tocar otra cosa suelta la barra)
    if (btnBack->Contains(mx, my)) { Back(); return true; }
    if (btnFwd->Contains(mx, my)) { Forward(); return true; }
    if (btnUp->Contains(mx, my)) { Up(); return true; }
    if (btnRefresh->Contains(mx, my)) { Update(); return true; }
    if (btnView->Contains(mx, my)) { gridView = !gridView; PosY = 0; return true; }
    if (btnCancel->Contains(mx, my)) { Close(); return true; }
    if (btnAction->Contains(mx, my)) { Accept(); return true; }
    if (btnBmAdd->Contains(mx, my)) { AddBookmark(); return true; }
    if (btnBmDel->Contains(mx, my)) { RemoveBookmark(); return true; }
    // arrastre del scroll
    if (mouseOverScrollY) { mouseOverScrollYpress = true; gDragMy = my; return true; }
    int bm = BookmarkAt(mx, my);
    if (bm >= 0) { selBm = bm; if (bookmarks[bm].path != currentPath) Navigate(bookmarks[bm].path, true); return true; }
    // Press dentro del area de la lista: NO abrir ahora. Se guarda y se abre al SOLTAR
    // (si fue un tap). Si el dedo se arrastra, Motion() lo convierte en scroll y no abre.
    int e = EntryAt(mx, my);
    gListPressed = true; gListPressEntry = e;
    gListPressY = my; gListLastY = my; gListDragging = false;
    return true;
}

bool FileBrowser::Motion(int mx, int my) {
    focusZone = FZ_NONE; // el mouse se movio: manda el mouse, no las flechas
    ScrollMouseOver(pane, mx - fileX, my - fileY);
    if (leftMouseDown && mouseOverScrollYpress) {
        ViewPortClickDown = true;
        int d = my - gDragMy; gDragMy = my;
        ScrollY(d);
        return true;
    }
    // arrastre con el dedo/mouse SOBRE LA LISTA -> scrollear (y cancelar el tap: no abre)
    if (leftMouseDown && gListPressed) {
        if (!gListDragging && abs(my - gListPressY) > 8 * GlobalScale) gListDragging = true;
        if (gListDragging) {
            ViewPortClickDown = true;
            // arrastre del CONTENIDO 1:1 (ScrollY solo mueve sobre la barra o con la rueda)
            ScrollByTouch(0, my - gListLastY); gListLastY = my;
            return true;
        }
    }
    hover = EntryAt(mx, my);
    hoverBm = BookmarkAt(mx, my);
    btnBack->hover = btnBack->Contains(mx, my); btnFwd->hover = btnFwd->Contains(mx, my);
    btnUp->hover = btnUp->Contains(mx, my); btnView->hover = btnView->Contains(mx, my);
    btnRefresh->hover = btnRefresh->Contains(mx, my);
    btnBmAdd->hover = btnBmAdd->Contains(mx, my); btnBmDel->hover = btnBmDel->Contains(mx, my);
    btnCancel->hover = btnCancel->Contains(mx, my); btnAction->hover = btnAction->Contains(mx, my);
    btnOverwrite->hover = confirming && btnOverwrite->Contains(mx, my); btnNotOverwrite->hover = confirming && btnNotOverwrite->Contains(mx, my);
    return true;
}

void FileBrowser::Wheel(int delta) {
    MouseWheel = true;
    ScrollY(delta * 6 * GlobalScale); // mismo paso que el outliner
    MouseWheel = false;
}

void FileBrowser::Release() {
    mouseOverScrollYpress = false; ViewPortClickDown = false;
    // fue un TAP (press + soltar sin arrastrar) sobre una entrada -> recien ahi se abre
    if (gListPressed && !gListDragging && gListPressEntry >= 0) OpenEntry(gListPressEntry);
    gListPressed = false; gListPressEntry = -1; gListDragging = false;
}

void FileBrowser::EnsureVisible(int idx) {
    if (idx < 0) return;
    int top, bottom;
    if (gridView) { int c = (cols < 1) ? 1 : cols; int row = idx / c; top = row * cellH; bottom = top + cellH; } // cols=0 antes del 1er Resize -> /0
    else { int rh = RowHeightGS + GlobalScale; top = idx * rh; bottom = top + rh; } // MISMO paso que el render/hit-test (antes usaba bordersGS y el foco quedaba fuera de vista)
    if (top + PosY < 0) PosY = -top;
    if (bottom + PosY > fileH) PosY = fileH - bottom;
    if (PosY > 0) PosY = 0; if (MaxPosY > PosY) PosY = MaxPosY;
}

// limpia todos los hovers de mouse (las flechas toman el control del foco)
void FileBrowser::ClearHoverMouse() {
    hover = -1; hoverBm = -1;
    btnBack->hover = btnFwd->hover = btnUp->hover = btnRefresh->hover = btnView->hover = false;
    btnBmAdd->hover = btnBmDel->hover = false;
    btnCancel->hover = btnAction->hover = false;
}

// mueve el foco con las flechas. Salta entre zonas segun la orientacion:
//  vertical:   TOP <-(arriba) FILES <-(arriba) -> (abajo) BOOKMARKS -> BMBTN -> BOTTOM
//  horizontal: BOOKMARKS <-(izq)/(der)-> FILES ; arriba=TOP, abajo=BOTTOM/BMBTN
void FileBrowser::MoveFocus(int dir) {
    ClearHoverMouse();
    // primera tecla (o foco perdido por el mouse): revelar sin moverse aun
    if (focusZone == FZ_NONE) {
        if (!entries.empty()) { focusZone = FZ_FILES; if (selected < 0) selected = 0; EnsureVisible(selected); }
        else { focusZone = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; }
        return;
    }
    // normalizar indices invalidos (p.ej. tras navegar a otra carpeta)
    if (focusZone == FZ_FILES && (selected < 0 || selected >= (int)entries.size())) {
        if (entries.empty()) { focusZone = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; return; }
        selected = 0; EnsureVisible(selected); return;
    }
    if (focusZone == FZ_BOOKMARKS && (selBm < 0 || selBm >= (int)bookmarks.size())) {
        selBm = bookmarks.empty() ? -1 : 0; return;
    }

    int nCols = gridView ? cols : 1; if (nCols < 1) nCols = 1;
    switch (focusZone) {
    case FZ_TOP:
        if (dir == LayoutKey::Left)       { if (focusIdx > 0) focusIdx--; }
        else if (dir == LayoutKey::Right) { if (focusIdx < 4) focusIdx++; }
        else if (dir == LayoutKey::Down) {
            if (horizontal && focusIdx == 0) { focusZone = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; }
            else { focusZone = FZ_FILES; if (selected < 0) selected = 0; EnsureVisible(selected); }
        }
        break;
    case FZ_FILES:
        if (dir == LayoutKey::Up) {
            if (selected - nCols >= 0) { selected -= nCols; EnsureVisible(selected); }
            else { focusZone = FZ_TOP; focusIdx = 0; }            // arriba de todo: barra superior
        } else if (dir == LayoutKey::Down) {
            if (selected + nCols < (int)entries.size()) { selected += nCols; EnsureVisible(selected); }
            else if (horizontal) { focusZone = FZ_BOTTOM; focusIdx = 1; } // a Cancelar/Accion
            else { focusZone = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; } // vertical: a los accesos
        } else if (dir == LayoutKey::Left) {
            if (gridView && (selected % nCols) != 0) { selected--; EnsureVisible(selected); }
            else if (horizontal) { focusZone = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : 0; } // izq: a los accesos
            else if (!gridView && selected > 0) { selected--; EnsureVisible(selected); }
        } else if (dir == LayoutKey::Right) {
            if (gridView && (selected % nCols) != nCols - 1 && selected + 1 < (int)entries.size()) { selected++; EnsureVisible(selected); }
        }
        break;
    case FZ_BOOKMARKS: {
        int bc = (bmCols < 1) ? 1 : bmCols; // 1=lista (horizontal), N=grilla (vertical)
        if (dir == LayoutKey::Up) {
            if (selBm - bc >= 0) selBm -= bc;
            else if (horizontal) { focusZone = FZ_TOP; focusIdx = 0; }   // horizontal: arriba=barra
            else if (entries.empty()) { focusZone = FZ_TOP; focusIdx = 0; } // carpeta VACIA: no hay ficheros que atravesar -> directo a la barra (Back/Up), sino quedabas atrapado
            else { focusZone = FZ_FILES; if (selected < 0) selected = (int)entries.size() - 1; EnsureVisible(selected); } // vertical: arriba=ficheros
        } else if (dir == LayoutKey::Down) {
            if (selBm + bc < (int)bookmarks.size()) selBm += bc;
            else { focusZone = FZ_BMBTN; focusIdx = 0; }                 // abajo: botones + / -
        } else if (dir == LayoutKey::Left) {
            if (bc > 1 && (selBm % bc) != 0) selBm--;                  // grilla: dentro de la fila
        } else if (dir == LayoutKey::Right) {
            if (bc > 1) { if ((selBm % bc) != bc - 1 && selBm + 1 < (int)bookmarks.size()) selBm++; }
            else { focusZone = FZ_FILES; if (selected < 0) selected = 0; EnsureVisible(selected); } // lista: derecha=ficheros
        }
        break; }
    case FZ_BMBTN:
        if (dir == LayoutKey::Left)       focusIdx = 0;
        else if (dir == LayoutKey::Right) focusIdx = 1;
        else if (dir == LayoutKey::Up)    { focusZone = FZ_BOOKMARKS; selBm = bookmarks.empty() ? -1 : (int)bookmarks.size() - 1; }
        else if (dir == LayoutKey::Down)  { focusZone = FZ_BOTTOM; focusIdx = 0; }
        break;
    case FZ_BOTTOM:
        if (dir == LayoutKey::Left)       focusIdx = 0;
        else if (dir == LayoutKey::Right) focusIdx = 1;
        else if (dir == LayoutKey::Up) {
            if (horizontal) { focusZone = FZ_FILES; if (selected < 0) selected = 0; EnsureVisible(selected); }
            else { focusZone = FZ_BMBTN; focusIdx = 0; }
        }
        break;
    default: break;
    }
}

// Enter / OK: activa el elemento enfocado (abre carpeta, importa archivo,
// pulsa el boton). NO hace falta doble click: OK sobre un archivo lo abre.
void FileBrowser::ActivateFocus() {
    switch (focusZone) {
    case FZ_TOP:
        if (focusIdx == 0) Back();
        else if (focusIdx == 1) Forward();
        else if (focusIdx == 2) Up();
        else if (focusIdx == 3) Update();
        else { gridView = !gridView; PosY = 0; }
        break;
    case FZ_BOOKMARKS:
        if (selBm >= 0 && selBm < (int)bookmarks.size()) Navigate(bookmarks[selBm].path, true);
        break;
    case FZ_BMBTN:
        if (focusIdx == 0) AddBookmark(); else RemoveBookmark();
        break;
    case FZ_BOTTOM:
        if (focusIdx == 0) Close(); else Accept();
        break;
    case FZ_FILES:
    default:
        if (selected >= 0 && selected < (int)entries.size()) {
            if (entries[selected].isDir) OpenEntry(selected); // OK en carpeta: entra
            else Accept();                                    // OK en archivo: lo abre
        }
        break;
    }
}

bool FileBrowser::Key(int key) {
    if (key == LayoutKey::Up || key == LayoutKey::Down ||
        key == LayoutKey::Left || key == LayoutKey::Right) { MoveFocus(key); return true; }
    if (key == LayoutKey::Enter)  { ActivateFocus(); return true; }
    if (key == LayoutKey::Cancel) { Close(); return true; }   // soft IZQ (Symbian)
    if (key == LayoutKey::Accept) { Accept(); return true; }  // soft DER = Import/abrir
    return true;
}

// Enter en la barra del nombre = guardar; Esc = cerrar el explorador
bool FileBrowser::KeyInText(bool enter) {
    if (!withName || g_activeTextField != &fileName) return false;
    if (enter) Accept(); else Close();
    return true;
}

void FileBrowser::Close() {
    FBFreeThumbs(); // cerrar el explorador libera TODAS las miniaturas
    if (PopUpActive == this) { PopUpActive = NULL; delete this; }
}

// ============================================================================
#ifdef W3D_SYMBIAN
const char* const kUseCurrentFolder = "Use this folder";     // pantalla angosta (240px). En INGLES: es la clave de T()
#else
const char* const kUseCurrentFolder = "Use current folder";
#endif

void OpenFileBrowser(const std::string& title, const std::string& actionLabel,
                      const std::string& filter, void (*accept)(const std::string&),
                      bool save) {
#ifdef __EMSCRIPTEN__
    // NAVEGADOR: el explorador del editor mostraria el disco virtual en memoria, que no le sirve a nadie. Abrir usa
    // el selector de archivos del navegador; guardar escribe en /descargas y cada punto de escritura lo descarga.
    if (save) {
        if (PopUpActive) PopUpActive->Close();
        accept(W3dWebFolderDownloads());
        return;
    }
    if (W3dWebOpenFile(title, filter, accept)) { if (PopUpActive) PopUpActive->Close(); return; }
#endif
    if (PopUpActive) PopUpActive->Close();
    FileBrowser* fb = new FileBrowser(title, actionLabel, filter, accept);
    fb->modeSave    = save;
    PopUpActive = fb;
    fb->Open(w3dFileSystem::GetHomeDir());
}

void OpenFileBrowserSaveAs(const std::string& title, const std::string& actionLabel, const std::string& ext,
                                 const std::string& name, const std::string& folder, void (*accept)(const std::string&)) {
#ifdef __EMSCRIPTEN__
    // (el navegador: se guarda en /descargas con ese nombre y cada punto de escritura lo descarga)
    if (PopUpActive) PopUpActive->Close();
    accept(w3dFileSystem::JoinPath(W3dWebFolderDownloads(), name + ext));
    return;
#endif
    if (PopUpActive) PopUpActive->Close();
    FileBrowser* fb = new FileBrowser(title, actionLabel, ext, accept);
    fb->modeSave    = true;
    fb->withName = true;
    fb->extName  = ext;
    fb->fileName.SetText(name);
    PopUpActive = fb;
    fb->Open((!folder.empty() && w3dFileSystem::IsDir(folder)) ? folder : w3dFileSystem::GetHomeDir());
#ifndef W3D_SYMBIAN
    TextFieldFocus(&fb->fileName);   // (listo para tipear: el nombre entero seleccionado)
    fb->fileName.SelectAll();
#endif
}

void OpenFileBrowserProject(const std::string& title, const std::string& actionLabel, int view, const std::string& ext,
                              const std::string& name, const std::string& folder, void (*accept)(const std::string&)) {
    // (tambien en el navegador: lo que se recorre es el proyecto, no el disco)
    if (PopUpActive) PopUpActive->Close();
    FileBrowser* fb = new FileBrowser(title, actionLabel, std::string(), accept);
    fb->projectView = view;
    fb->modeSave = true;
    fb->withName = true;
    fb->extName  = ext;
    fb->fileName.SetText(name);
    // UN acceso: la raiz del proyecto (los del disco no van aca)
    fb->bookmarks.clear();
    w3dFileSystem::Bookmark b; b.name = ProjectNameUrl(); b.path = std::string(); b.user = false;
    fb->bookmarks.push_back(b);
    PopUpActive = fb;
    fb->history.clear(); fb->histPos = -1;
    fb->Navigate(W3dFolderNormalize(folder), true);
#ifndef W3D_SYMBIAN
    TextFieldFocus(&fb->fileName);
    fb->fileName.SelectAll();
#endif
}

void OpenFileBrowserLib(const std::string& title, const std::string& actionLabel, const std::string& lib,
                              void (*accept)(const std::string&)) {
    if (PopUpActive) PopUpActive->Close();
    FileBrowser* fb = new FileBrowser(title, actionLabel, std::string(), accept);
    fb->libBrowse   = lib;
    fb->modeSave    = true;   // (aceptar sin nada elegido = la carpeta actual: la escena entera o el objeto con hijos)
    fb->bookmarks.clear();
    w3dFileSystem::Bookmark b; b.name = lib + ".w3d"; b.path = std::string(); b.user = false;
    fb->bookmarks.push_back(b);
    PopUpActive = fb;
    fb->history.clear(); fb->histPos = -1;
    fb->Navigate(std::string(), true);
}

// ============================================================================
//  carpeta elegida -> ruta de salida (ver el comentario del .h). UNICO lugar
//  donde se pega carpeta + nombre + extension: antes cada caller lo hacia a mano
//  (uno le pegaba ".w3d" a la CARPETA -> "/ruta/Documentos.w3d", otro
//  concatenaba con "/" a pelo -> "//nombre.w3dui").
// ============================================================================
// limpia el nombre tipeado: solo el ultimo tramo (por si viniera "carpeta/archivo")
// y sin espacios al borde. Vacio -> "sin_titulo" (nunca un archivo ".w3d" pelado).
static std::string CleanName(const std::string& name) {
    std::string base = name;
    size_t s = base.find_last_of("/\\");
    if (s != std::string::npos) base = base.substr(s + 1);
    size_t a = base.find_first_not_of(" \t");
    size_t b = base.find_last_not_of(" \t");
    base = (a == std::string::npos) ? std::string() : base.substr(a, b - a + 1);
    if (base.empty()) base = "untitled";
    return base;
}

// una ruta es ABSOLUTA: empieza en '/' (unix) o es "C:..." (windows)
static bool IsPathAbsolute(const std::string& r) {
    if (r.empty()) return false;
    if (r[0] == '/' || r[0] == '\\') return true;
    return (r.size() > 1 && r[1] == ':');
}

std::string W3dFolderBaseOutput() {
    // con un proyecto abierto, "al lado del proyecto" es lo que espera el usuario
    // (tipear "renders" guarda en <carpeta del .w3d>/renders, no en el CWD del editor)
    if (!w3dPath.empty()) {
        size_t s = w3dPath.find_last_of("/\\");
        if (s != std::string::npos && s > 0) return w3dPath.substr(0, s);
    }
    return w3dFileSystem::GetDefaultOutputDir();
}

std::string W3dPathInFolder(const std::string& folder, const std::string& name,
                             const std::string& ext) {
    std::string dir = folder;
    // sin carpeta -> la de salida por defecto (nunca una ruta relativa rota)
    if (dir.empty()) dir = w3dFileSystem::GetDefaultOutputDir();
    // carpeta RELATIVA tipeada a mano ("renders", "../salida"): se resuelve contra la
    // carpeta del PROYECTO abierto (o la de salida por defecto si no hay ninguno). Antes
    // colgaba del CWD del editor y el archivo aparecia dentro del arbol de Whisk3D.
    else if (!IsPathAbsolute(dir)) dir = w3dFileSystem::JoinPath(W3dFolderBaseOutput(), dir);
    // JoinPath normaliza: nunca quedan barras duplicadas ni "./"
    std::string path = w3dFileSystem::JoinPath(dir, CleanName(name));
    if (!ext.empty() && !FinishesIn(path, ext.c_str())) path += ext;
    return path;
}

std::string W3dOutputPath(const std::string& picked, const std::string& name,
                            const std::string& ext) {
    // vacio o CARPETA (existente, o terminada en barra): carpeta + nombre + ext
    bool isFolder = picked.empty() || w3dFileSystem::IsDir(picked) ||
                     picked[picked.size() - 1] == '/' || picked[picked.size() - 1] == '\\';
    if (isFolder) return W3dPathInFolder(picked, name, ext);
    // ARCHIVO existente elegido a mano: se respeta (sobrescribir), forzando la extension
    std::string path = picked;
    // ... pero si vino RELATIVO (tipeado, no del explorador) se resuelve igual que una carpeta
    // relativa: contra el proyecto abierto, nunca contra el CWD del editor
    if (!IsPathAbsolute(path)) path = w3dFileSystem::JoinPath(W3dFolderBaseOutput(), path);
    if (!ext.empty() && !FinishesIn(path, ext.c_str())) path += ext;
    return path;
}
