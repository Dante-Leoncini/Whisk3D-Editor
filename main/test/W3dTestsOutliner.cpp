// ============================================================================
//  W3dTestsOutliner.cpp — comandos de harness del OUTLINER POR RECURSOS
//  (ui/ViewPorts/Outliner.h + OutlinerResources.cpp, io/ProjectResources.h).
//  Casi todo va por la PUERTA DEL USUARIO: el click (LayoutClickUI + LayoutRelease),
//  las teclas del N95 (LayoutKeyPanelActive / LayoutKeyUI con el outliner
//  como panel activo), los menus de la barra y los campos de Properties.
//
//  Comandos:
//    outvista [<clave>] [es <clave>]
//        cambia la vista del primer outliner del layout POR SU MENU de la barra ([1]); 'es'
//        asserta la vista actual. Claves: escena mallas materiales texturas animaciones
//        prefabs escenas librerias.
//    outfilas [n N]                la lista visible (carpetas y recursos, con sangria y usuarios)
//    outfila <nombre|ruta> [usuarios N] [estado uso|huerfano] [prof N] [carpeta <c|->]
//            [plegada 0|1] [fila N] [cursor 0|1] [activo 0|1] [icono <nombre del icono: camera, gamepad...>]
//    outnofila <nombre|ruta>
//    outclick <nombre|ruta> [flecha|ctrl|shift]   click de verdad en la fila (en la flechita: plegar;
//                                        con Ctrl / Shift apretado: la seleccion multiple)
//    outdrag <nombre> <destino|->        arrastra un recurso O UNA CARPETA (con la seleccion, si la
//                                        fila es parte de ella) y lo suelta sobre la fila destino (una
//                                        carpeta, o un recurso: su carpeta; '-' = el vacio)
//    outtecla <up|down|left|right|ok|c|5|ctoque> [veces]  el keypad del N95 (outliner = panel activo;
//                                        el 5 marca/desmarca el cursor, como W3dOutlinerMarkToggle;
//                                        ctoque = el toque de C al soltar, como W3dOutlinerResourcesDelete)
//    outteclapc <up|down|left|right|enter|f2|supr|g|a|esc|shiftup|shiftdown|alta|ctrli|shiftd|altd>   el teclado de PC por la
//                                        puerta real: flechas/Enter/Esc a la UI compartida (LayoutKeyUI)
//                                        con el mouse sobre el outliner; lo demas a event_key_down
//    outsel [n N] [tiene X]... [notiene X]...   la SELECCION MULTIPLE (filas por nombre o clave)
//    outtipear <texto>             tipea en el campo enfocado SIN confirmar (reemplaza lo seleccionado)
//    outesc                        la tecla Esc de PC por la puerta real (InputUserSDL3): descarta
//    campoenfocar recnombre|reccarpeta|nombreobj|hbetiqueta   enfoca ese campo de Properties por su
//                                  puerta (EditPropertie: el click en la fila). hbetiqueta = la etiqueta
//                                  del hitbox activo (un campo que escribe EN VIVO)
//    texcargada <ruta> 0|1         la textura esta (o no) cargada en la GPU (Textures, iID != 0)
//    texpurgadas [n N]             el cementerio de las texturas purgadas en caliente
//    partex <nombre> <ruta>        un emisor de particulas con esa textura (un usuario por RUTA)
//    fliptex <nombre> <ruta>       un flipbook con nombre de la escena con ese atlas (otro usuario por ruta)
//    texrefde particula|flipbook <nombre> <ruta>   la ruta que nombra (sigue a un rename de la textura)
//    outbarra <soft|izq|der|arriba|abajo|ok|c> [menu vista|acciones|raiz|nueva|tipo|mover|ninguno]
//        la BARRA sin mouse: 'soft' = la tecla izquierda del N95; el resto van al menu abierto.
//        'menu' asserta que menu quedo abierto.
//    outmenu <texto> | outmenu mover <carpeta|->
//        abre el menu de ACCIONES con un click en su boton y elige la opcion (texto en ingles)
//    outcontexto <nombre>          click derecho en la fila (menu de acciones en el mouse)
//    outescribir <texto>           tipea en el campo enfocado (reemplaza) + Enter + un cuadro
//    outconfirmar si|no [tiene|no X]  responde el cartel de confirmacion abierto (y asserta que diga o no X)
//    outrender                     dibuja la UI entera (rootViewport)
//    outpx <nombre> uso|huerfano   el icono de estado de la fila: tilde VERDE / cruz ROJA
//    recactivo [ninguno | <clave> <id>]
//    recprops [tab 0|1] [material <nombre>] [preview 0|1] [nombre <texto>] [carpeta <texto|->]
//        el panel de propiedades con el recurso activo (pestania 3): la tarjeta Material
//        bindeada al material elegido, la vista previa de la textura, los campos Name/Folder
//    receditar nombre|carpeta <texto|->   edita Name / Folder del recurso en Properties
//    recursos <clave> [n N]        los items de una vista (con carpeta y usuarios)
//    recurso <clave> <id> [existe 0|1] [carpeta <c|->] [usuarios N] [nombre X]
//    carpetas <clave> [n N] [tiene X]... [notiene X]...
//    carpetanueva <clave> <padre|-> <nombre>      (el modelo, sin UI; con undo)
//    carpetarenombrar <clave> <ruta> <nombre>
//    carpetaborrar <clave> <ruta> [falla]
//    carpetamover <clave> <ruta> <destino|->      mudar la carpeta adentro de otra (con undo)
//    recmover <clave> <id> <carpeta|->
//    recrenombrar <clave> <id> <nuevo> [<esperado>]
//    recborrar <clave> <id> [falla]
//    recpurgar <clave> [n N]
//    matsuelto <nombre>            un material nuevo sin usar (un HUERFANO)
//    texasignar <objeto> <ruta.png>   la textura del material de la parte 0 del objeto
//    texentrada <ruta.png>         importa la imagen al contenedor (una entrada sin usar)
//    jsontiene <ruta.w3d> <texto> [0|1]   el project.json del contenedor tiene (o no) el texto
//    seleccion [n N] [tiene X]... [modo objeto|edit]   los seleccionados (ObjSelects) y el modo
//    entradacruda <entrada> <archivo> / entradatexto <entrada> <texto...>
//        una entrada del contenedor montado con ese nombre EXACTO (el overlay: "textures/Piso_A.png",
//        "scripts/x.lua"), con los bytes de un archivo o con el texto del resto de la linea
//    outfondo <nombre> sel|nosel   el FONDO de la fila (dibujado, con la lista scrolleada hasta ella)
//                                  es / no es el de la seleccion multiple
//    outdragsobre <nombre> <sobre> <resaltada|->   arrastra sin soltar: la fila resaltada como
//                                  destino (la CARPETA a donde iria; '-' = la raiz, nada) y su fondo;
//                                  despues suelta afuera (no mueve nada)
//    recboton usuarios|borrar [oculto]   el boton "Select Users" / "Delete" de la tarjeta del recurso en
//                                  Properties (pestania 3): lo aprieta (o asserta que no se ofrece)
//    recdatos <clave> <id> <en|es|pt> <texto...>  los datos del recurso (tarjeta Recurso) en ese idioma
//    texdormidas [asignar <objeto> <parte> <material>] [cuadros K] [n N]   los materiales con texturas
//                                  DORMIDAS; 'asignar' pone el material en la parte sin pasar por
//                                  Properties (como un undo o un script); 'cuadros' corre K veces la
//                                  cola diferida del loop principal (la que las despierta)
//    idebib ensuciar | abrir <ruta> | archivo <ruta> | sucio 0|1 | tiene|notiene <texto>
//        el primer IDE del layout y el "Open in IDE" de la biblioteca (IDEOpenScript)
//    recexterno <carpeta|ruta>     "External (choose where)..." del recurso activo con esa eleccion del explorador
//    minidistintas <tipo> <idA> <idB>   las miniaturas de los dos recursos no son iguales
//    bibmedir [veces]              ms por llamada de listar la biblioteca (cada tipo y toda), las filas y los cambios
//    recprovprueba <clave> on|off
//        registra (u olvida) un PROVEEDOR DE PRUEBA en una vista sin datos (prefabs, escenas,
//        librerias): 3 recursos en memoria ("Alfa" en uso, "Beta" huerfano, "Gamma" de solo
//        lectura) con carpeta, renombre y borrado. Prueba que la interfaz de proveedor alcanza
//        para que una fase nueva llene su vista sin tocar el outliner.
//    propsalfa fila [<valor>|-|arrastre] | arrastrar <px> [<min> <max>] | texto <numero>
//        la fila "Alpha Test" de la tarjeta Material (Material::alphaTest) con el MOUSE de verdad: click
//        en la fila, arrastre horizontal y soltar; o click sin arrastrar + tipear + Enter (ver CmdPropsAlpha)
//    vpoverlays [on|off] [es 0|1] [publicado 0|1]   el ojo "Show Overlays" del primer viewport 3D
// ============================================================================
#include "test/W3dTestsOutliner.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/ViewPort3D.h"       // vpoverlays: el ojo "Show Overlays" del viewport 3D
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/PropImage.h"
#include "ViewPorts/IDE.h"                // idebib: el "Open in IDE" de la biblioteca
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/ConfirmPopup.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/widgets/TextField.h"
#include "io/ProjectResources.h"
#include "io/ExternalLibrary.h"
#include "io/ProjectChanges.h"
#include "io/Thumbnails.h"
#include "io/ProjectMeshes.h"
#include "objects/MeshResource.h"
#include "W3dRoots.h"
#include "io/W3dContainer.h"
#include "io/W3dZip.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Particles.h"          // partex: un usuario de textura por ruta
#include "objects/ObjectMode.h"         // W3dRenameObject
#include "animation/Flipbook.h"         // fliptex: el atlas de un flipbook con nombre
#include "config/W3dLang.h"
#include "importers/import_obj.h"     // texdormidas: las texturas dormidas y la cola diferida
#include "base/W3dNames.h"
#include "base/W3dInteractionState.h"   // seleccion modo: InteractionMode
#include "variables.h"
#include "w3dGraphics.h"
#include "w3dFilesystem.h"            // entradacruda: los bytes de un archivo de prueba
#include "graphics/w3dTexture.h"          // outmini png: SavePNG
#include "config/W3dProfile.h"       // bibmedir: W3dNowMs
#include <cmath>     // propsalfa: fabsf
#include <cstdio>
#include <cstdlib>
#include <set>
#ifdef _WIN32
    #include <direct.h>   // _getcwd (las rutas relativas del harness)
    #define getcwd _getcwd
#else
    #include <unistd.h>   // getcwd (las rutas relativas del harness)
#endif
#include <cstring>
#include <string>
#include <vector>
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>   // outesc: el Esc de PC por la puerta real (un SDL_Event)
#endif

extern int W3dScreenHeight;

// ---------------------------------------------------------------------------
//  AYUDAS
// ---------------------------------------------------------------------------
static ViewportBase* FindLeaf(ViewportBase* n, int kind) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == kind ? n : NULL;
    ViewportBase* a = NULL; ViewportBase* b = NULL;
    if (n->ContainerKind() == 1) { a = ((ViewportRow*)n)->childA; b = ((ViewportRow*)n)->childB; }
    else { a = ((ViewportColumn*)n)->childA; b = ((ViewportColumn*)n)->childB; }
    ViewportBase* r = FindLeaf(a, kind);
    return r ? r : FindLeaf(b, kind);
}
static Outliner* ElOutliner(std::string& err, const char* cmd) {
    Outliner* o = (Outliner*)FindLeaf(rootViewport, 9); if (!o) o = (Outliner*)FindLeaf(rootViewport, 2);
    if (!o) err = std::string(cmd) + ": no hay outliner en el layout";
    // (entre un comando y otro paso mas que un doble click: el click que sigue a entrar a una carpeta es un click SUELTO)
    else o->enteredMs = 0;
    return o;
}
static Properties* ElPanel() {
    if (PropsActive) return PropsActive;
    PropsActive = (Properties*)FindLeaf(rootViewport, 3);
    return PropsActive;
}
static int ViewOf(const std::string& c, std::string& err, const char* cmd) {
    const int v = W3dViewByKey(c);
    if (v < 0) err = std::string(cmd) + ": vista desconocida '" + c + "'";
    return v;
}
static std::string Dash(const std::string& s) { return s == "-" ? std::string() : s; }
static std::string Integer(long n) { char b[32]; sprintf(b, "%ld", n); return std::string(b); }
// el id de un recurso de AFUERA: el harness escribe rutas RELATIVAS a tools/pruebas y 'reclocate' las pasa
// afuera con la ruta absoluta (la de disco): si el id tal cual no es de la vista y el absoluto si, va ese
static std::string IdOfView(int view, const std::string& id) {
    if (id.empty() || id[0] == '/' || W3dViewRecInfo(view, id, NULL)) return id;
    char b[4096];
    if (!getcwd(b, sizeof(b))) return id;
    std::string abs = std::string(b) + "/" + id;
    if (W3dViewRecInfo(view, abs, NULL)) return abs;
    // (Windows: getcwd da '\' y la biblioteca nombra los archivos de afuera con '/')
    for (size_t i = 0; i < abs.size(); i++) if (abs[i] == '\\') abs[i] = '/';
    return W3dViewRecInfo(view, abs, NULL) ? abs : id;
}

// la fila por CLAVE (la ruta de una carpeta, el id de un recurso: es unica) o por NOMBRE (lo que
// se ve: dos carpetas hermanas de madres distintas pueden llamarse igual)
static int RowOf(const std::vector<OutRowRec>& rows, const std::string& n) {
    if (n == "back") { for (size_t i = 0; i < rows.size(); i++) if (rows[i].back) return (int)i; return -1; }   // la celda "Ir atras" de la cuadricula
    for (size_t i = 0; i < rows.size(); i++) if (rows[i].key == n) return (int)i;
    for (size_t i = 0; i < rows.size(); i++) if (!rows[i].folder && rows[i].id == n) return (int)i;
    for (size_t i = 0; i < rows.size(); i++) if (rows[i].name == n) return (int)i;
    return -1;
}
// la clave de la vista (el harness la compara): escena / biblioteca (o el filtro: mallas, materiales...) / lib:...
static std::string KeyView(const Outliner* o) {
    if (o->view == OUT_VIEW_SCENE) return "scene";
    if (o->view == OUT_VIEW_LIBRARY) return o->filter > W3D_VIEW_SCENE ? std::string(W3dViewKey(o->filter)) : std::string("library");
    return o->ViewKey();
}
// si 'c' es la clave de un tipo, se la come (los comandos de carpetas de la fase anterior la llevaban: hoy
// el arbol de carpetas es UNO para todos los tipos)
static std::string NoTypeKey(std::istringstream& ss) {
    std::string c; ss >> c;
    if (W3dViewByKey(c) > W3D_VIEW_SCENE) ss >> c;
    return c;
}
// deja la fila a la vista (scroll) y devuelve la Y de su centro
static int YOfRow(Outliner* o, int row) {
    o->Resize(o->width, o->height);   // (las medidas del scroll al dia: el harness no dibuja entre pasos)
    const int top = o->TopContent();
    int yf, height, base;
    if (o->grid) {
        std::vector<OutRowRec> rows; o->RowsResources(rows);
        int rx, ry, rw, rh; o->RectRow(rows, row, rx, ry, rw, rh);
        base = ry - borderGS - top; height = rh; yf = ry + o->PosY;
    } else { height = (int)RowHeightGS; base = row * height; yf = borderGS + o->PosY + top + base; }
    if (yf < top || yf + height > o->height - o->RowHeightMemory()) {   // (la fila de memoria de abajo la tapa)
        o->PosY = -base;
        if (o->PosY < o->MaxPosY) o->PosY = o->MaxPosY;
        if (o->PosY > 0) o->PosY = 0;
    }
    return o->RowResourceY(row);
}
static int XName(Outliner* o, const OutRowRec& f) {
    if (o->grid) {   // (la CELDA: tambien la de una carpeta)
        std::vector<OutRowRec> rows; o->RowsResources(rows);
        for (size_t i = 0; i < rows.size(); i++)
            if (rows[i].folder == f.folder && rows[i].key == f.key) {
                int rx, ry, rw, rh; o->RectRow(rows, (int)i, rx, ry, rw, rh);
                return o->x + rx + rw / 2;
            }
    }
    return o->x + marginGS + (o->grid ? 0 : o->PosX) + (f.prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
}
static int XArrow(Outliner* o, const OutRowRec& f) {
    return o->x + marginGS + (o->grid ? 0 : o->PosX) + f.prof * (IconSizeGS + gapGS) + IconSizeGS / 2;
}
static void Dump(const std::vector<OutRowRec>& rows, const char* cmd) {
    for (size_t i = 0; i < rows.size(); i++) {
        const OutRowRec& f = rows[i];
        printf("      [%s] %2d %s%s%s%s", cmd, (int)i, std::string((size_t)f.prof * 2, ' ').c_str(),
               f.folder ? (f.collapsed ? "+ " : "- ") : "  ", f.name.c_str(), f.folder ? "/" : "");
        if (!f.folder) printf("  [%s %d]%s", f.users > 0 ? "EN USO" : "HUERFANO", f.users, f.readOnly ? " (solo lectura)" : "");
        printf("\n");
    }
}
// cual menu de la barra del outliner esta abierto
static std::string OpenMenuName(Outliner* o) {
    extern bool LayoutMenuOpen();
    if (!LayoutMenuOpen()) return "none";
    const int b = o->OpenMenuButtonIndex();
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnView) return "view";
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnOptions) return "options";   // el OJO (como se ve)
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnObject) return "object";
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnSelection) return "selection";
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnFilter) return "filter";
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnRoot) return "root"; // el selector de escena/prefab
    if (b >= 0 && o->BarButtons[(size_t)b] == o->newBtn) return "new"; // el "+" (nueva escena/juego/prefab/carpeta/material)
    if (OpenMenu && OpenMenu->title == T("Move to Folder")) return "move";
    return "type";   // el de getType/split del viewport (el [0]) u otro
}
static MenuItem* ItemByText(PopupMenu* m, const std::string& t) {
    if (!m) return NULL;
    for (size_t i = 0; i < m->items.size(); i++) {
        std::string s = m->items[i]->text;
        size_t a = s.find_first_not_of(' ');
        if (a != std::string::npos) s = s.substr(a);
        if (s == t) return m->items[i];
    }
    return NULL;
}

// ---------------------------------------------------------------------------
//  LA VISTA Y LAS FILAS
// ---------------------------------------------------------------------------
// elige un item de un menu de la barra por su id (abriendolo con su boton, como el mouse)
static bool PickInMenuBar(Outliner* o, Button* b, int id, std::string& err, const char* cmd) {
    int idx = -1;
    for (size_t i = 0; i < o->BarButtons.size(); i++) if (o->BarButtons[i] == b) idx = (int)i;
    if (idx < 0 || !o->OpenMenuButton(idx) || !OpenMenu) { err = std::string(cmd) + ": no se abrio el menu"; return false; }
    MenuItem* it = NULL;
    for (size_t i = 0; i < OpenMenu->items.size(); i++) if (OpenMenu->items[i]->id == id) it = OpenMenu->items[i];
    if (!it) { OpenMenu->Close(); err = std::string(cmd) + ": el menu no ofrece esa opcion"; return false; }
    PopupMenu* m = OpenMenu;
    m->Close();
    m->Run(it->id);
    return true;
}
// outvista <escena|biblioteca|lib:<ruta>|<tipo>> [es <clave>]: la VISTA por el menu del ojo ([1]); un TIPO
// ("materiales") es la biblioteca con ese FILTRO (por el menu del filtro)
static bool CmdOutView(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outview"); if (!o) return false;
    std::string a; ss >> a;
    if (!a.empty() && a != "es") {
        int v = -1, filter = -2;
        if (a == "scene") v = OUT_VIEW_SCENE;
        else if (a == "library") { v = OUT_VIEW_LIBRARY; filter = -1; }
        else if (a.compare(0, 4, "lib:") == 0) {
            for (int i = 0; i < W3dLibsCount(); i++) if (W3dLibsDiskPath(i) == a.substr(4) || W3dLibsRow(i).name == a.substr(4)) v = OUT_VIEW_LIBEXT + i;
            if (v < 0) { err = "outview: la libreria '" + a.substr(4) + "' no esta vinculada"; return false; }
        } else {
            const int t = ViewOf(a, err, "outview"); if (t < 0) return false;
            v = OUT_VIEW_LIBRARY; filter = t;
        }
        // el OUTLINER muestra la escena y la BIBLIOTECA es otro viewport: pasar de una a la otra es cambiar el TIPO del
        // viewport (la misma puerta que el menu de tipos). La biblioteca nueva arranca en cuadricula: las pruebas de
        // siempre la miran en lista (outcuadricula la cambia)
        extern void LayoutChangeViewportType(ViewportBase* v, int aId);
        if (v != OUT_VIEW_SCENE && o->ViewportKind() == 2) {
            LayoutChangeViewportType(o, 8);
            o = (Outliner*)FindLeaf(rootViewport, 9);
            if (!o) { err = "outview: no se creo la biblioteca"; return false; }
            o->ChangeGrid(false);
            printf("      [outvista] el outliner paso a ser la BIBLIOTECA\n");
        } else if (v == OUT_VIEW_SCENE && o->ViewportKind() == 9) {
            LayoutChangeViewportType(o, 1);
            o = (Outliner*)FindLeaf(rootViewport, 2);
            if (!o) { err = "outview: no volvio el outliner"; return false; }
            printf("      [outvista] la biblioteca volvio a ser el OUTLINER\n");
        }
        if (v != OUT_VIEW_SCENE && o->view != v && !PickInMenuBar(o, o->btnView, v, err, "outview")) return false;
        if (filter != -2 && o->filter != filter && !PickInMenuBar(o, o->btnFilter, filter < 0 ? 0 : filter, err, "outview")) return false;
        ss >> a;
    }
    if (a == "es") {
        std::string c; ss >> c;
        if (KeyView(o) != c) { err = "outview: la vista es '" + KeyView(o) + "', se esperaba '" + c + "'"; return false; }
    }
    printf("      [outvista] %s (boton '%s', filtro %s, %s)\n", KeyView(o).c_str(), o->btnView->text.c_str(),
           o->filter < 0 ? "all" : W3dViewKey(o->filter), o->grid ? "grid" : "list");
    return true;
}

static bool CmdOutRows(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outrows"); if (!o) return false;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    Dump(rows, "outrows");
    std::string k; int n = -1;
    if ((ss >> k) && k == "n" && (ss >> n) && n != (int)rows.size()) {
        err = "outfilas: " + Integer((long)rows.size()) + " filas, se esperaban " + Integer(n); return false;
    }
    return true;
}

static bool CmdOutRow(std::istringstream& ss, std::string& err, bool negated) {
    Outliner* o = ElOutliner(err, "outrow"); if (!o) return false;
    std::string n; ss >> n;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (negated) {
        if (i >= 0) { err = "outnorow: la fila '" + n + "' esta"; return false; }
        printf("      [outnofila] '%s' no esta\n", n.c_str());
        return true;
    }
    if (i < 0) { Dump(rows, "outrow"); err = "outrow: no hay una fila '" + n + "'"; return false; }
    const OutRowRec& f = rows[(size_t)i];
    int va = -1; std::string ida; W3dResourceActive(&va, &ida);
    const bool cursor = (f.folder == o->cursorIsFolder && f.key == o->cursorKey);
    const bool active = !f.folder && va == f.type && ida == f.id;
    printf("      [outfila] %d '%s' %s prof=%d carpeta='%s' usuarios=%d%s%s%s\n", i, f.name.c_str(),
           f.folder ? "CARPETA" : (f.users > 0 ? "EN USO" : "HUERFANO"), f.prof, f.folderOf.c_str(), f.users,
           f.collapsed ? " plegada" : "", cursor ? " cursor" : "", active ? " activo" : "");
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        bool ok = true;
        if (k == "users") ok = (f.users == atoi(v.c_str()));
        else if (k == "state") ok = (v == "use") ? (f.users > 0 && !f.folder) : (f.users == 0 && !f.folder);
        else if (k == "prof") ok = (f.prof == atoi(v.c_str()));
        else if (k == "folder") ok = (f.folderOf == Dash(v));
        else if (k == "collapsed") ok = (f.collapsed == (v == "1"));
        else if (k == "row") ok = (i == atoi(v.c_str()));
        else if (k == "cursor") ok = (cursor == (v == "1"));
        else if (k == "active") ok = (active == (v == "1"));
        else if (k == "icon") {
            ok = (v == IconName(f.icon));
            if (!ok) printf("      [outfila] '%s' tiene el icono '%s'\n", n.c_str(), IconName(f.icon));
        }
        else if (k == "type") ok = (!f.folder && W3dViewKey(f.type) == v);
        else if (k == "dirty") ok = (f.dirty == (v == "1"));
        else if (k == "external") ok = (f.external == (v == "1"));   // la flecha de acceso directo (afuera del .w3d)
        else { err = "outrow: no entiendo '" + k + "'"; return false; }
        if (!ok) { err = "outrow: '" + n + "' no cumple " + k + " " + v; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  MOUSE
// ---------------------------------------------------------------------------
static bool PointByName(Outliner* o, const std::string& n, int& mx, int& my, std::string& err, const char* cmd);
static bool CmdOutClick(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outclick"); if (!o) return false;
    std::string n, mode; ss >> n >> mode;
    int mx = 0, my = 0;
    if (o->view == OUT_VIEW_SCENE) {
        // la ESCENA: la fila del objeto (su nombre)
        if (!PointByName(o, n, mx, my, err, "outclick")) return false;
    } else {
        std::vector<OutRowRec> rows; o->RowsResources(rows);
        const int i = RowOf(rows, n);
        if (i < 0) { err = "outclick: no hay una fila '" + n + "'"; return false; }
        my = YOfRow(o, i);
        mx = (mode == "arrow") ? XArrow(o, rows[(size_t)i]) : XName(o, rows[(size_t)i]);
    }
    // los modificadores de la seleccion multiple: apretados durante el click, como el teclado real
    LCtrlPressed = (mode == "ctrl");
    LShiftPressed = (mode == "shift");
    // (como controls.cpp: el down de un click en cualquier lado DESENFOCA el campo de texto -> un rename en
    // linea abierto se confirma con lo tipeado)
    if (!PopUpActive) g_activeTextField = NULL;
    leftMouseDown = true;
    LayoutClickUI(mx, my);
    leftMouseDown = false;
    LayoutRelease(mx, my);
    LCtrlPressed = LShiftPressed = false;
    printf("      [outclick] '%s'%s en (%d, %d) seleccion=%d\n", n.c_str(), mode.empty() ? "" : (" (" + mode + ")").c_str(),
           mx, my, o->SelectionCount());
    return true;
}

// outdrag <nombre> <destino|-> [antes|despues]: arrastra la fila y la suelta sobre otra (en el centro: a su carpeta y
// despues de ella; 'antes' / 'despues' = en el borde de arriba / de abajo de la fila: REORDENAR) o en el vacio ('-')
static bool CmdOutDrag(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outdrag"); if (!o) return false;
    std::string n, d, border; ss >> n >> d >> border;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0) { err = "outdrag: no hay una fila '" + n + "'"; return false; }
    const int x0 = XName(o, rows[(size_t)i]), y0 = YOfRow(o, i);
    int x1 = x0, y1;
    if (d == "-") {
        // el VACIO debajo de la ultima fila (tiene que estar dentro del outliner)
        y1 = o->RowResourceY((int)rows.size()) + (int)RowHeightGS;
        if (y1 >= o->y + o->height) { err = "outdrag: no hay vacio a la vista debajo de la lista"; return false; }
    } else {
        const int j = RowOf(rows, d);
        if (j < 0) { err = "outdrag: no hay una fila destino '" + d + "'"; return false; }
        y1 = o->RowResourceY(j);
        x1 = XName(o, rows[(size_t)j]);
        if (border == "before" || border == "after") {
            if (o->grid) {
                int rx, ry, rw, rh; o->RectRow(rows, j, rx, ry, rw, rh);
                x1 = o->x + rx + (border == "before" ? rw / 4 : 3 * rw / 4);
            } else y1 += (border == "before" ? -1 : 1) * ((int)RowHeightGS / 2 - 2 * GlobalScale);
        } else if (!border.empty()) { err = "outdrag: no entiendo '" + border + "'"; return false; }
    }
    leftMouseDown = true;
    LayoutClickUI(x0, y0);
    // el movimiento en DOS pasos (primero pasa el umbral del arrastre, despues llega)
    o->event_mouse_motion(x0, y0 + (y1 > y0 ? 1 : -1) * (int)RowHeightGS);
    o->event_mouse_motion(x1, y1);
    const bool dragged = o->recDragging;
    const bool ghost = g_outDrag.ghost;
    rootViewport->Render();   // (el hueco oscuro, la linea de insercion y el fantasma se dibujan sin romper nada)
    leftMouseDown = false;
    LayoutRelease(x1, y1);
    printf("      [outdrag] '%s' -> '%s'%s (arrastre %s, fantasma %s)\n", n.c_str(), d.c_str(),
           border.empty() ? "" : (" " + border).c_str(), dragged ? "si" : "no", ghost ? "si" : "no");
    if (g_outDrag.ghost) { err = "outdrag: el fantasma quedo despues de soltar"; return false; }
    if (!dragged) { err = "outdrag: el arrastre no arranco"; return false; }
    return true;
}

// soltararchivo <ruta> <kind> [dx dy]: un ARCHIVO soltado desde el explorador del sistema (SDL_DROPFILE) en el centro
// del primer viewport de ese kind (1 = 3D, 9 = la biblioteca), corrido dx, dy px. Lo que pasa lo cuentan los comandos
// de siempre (recurso, outfila, objeto...)
static bool CmdReleaseFile(std::istringstream& ss, std::string& err) {
    std::string path; int kind = 9, dx = 0, dy = 0; ss >> path >> kind >> dx >> dy;
    // (el explorador del sistema siempre suelta la ruta ABSOLUTA: la relativa es desde la carpeta de las pruebas)
    if (!path.empty() && path[0] != '/' && !(path.size() > 2 && path[1] == ':')) {
        char cwd[1024] = { 0 };
#ifdef _WIN32
        if (_getcwd(cwd, sizeof(cwd))) path = std::string(cwd) + "/" + path;
#else
        if (getcwd(cwd, sizeof(cwd))) path = std::string(cwd) + "/" + path;
#endif
    }
    ViewportBase* v = FindLeaf(rootViewport, kind);
    if (!v) { err = "releasefile: no hay un viewport de ese kind"; return false; }
    rootViewport->Render();   // (el 3D sabe su lente para el rayo)
    extern bool W3dReleaseFile(const std::string& path, int mx, int my, std::string* that);
    std::string that;
    const bool ok = W3dReleaseFile(path, v->x + v->width / 2 + dx, v->y + v->height / 2 + dy, &that);
    printf("      [soltararchivo] '%s' en kind %d -> %s (%s)\n", path.c_str(), kind, ok ? "ok" : "NO", that.c_str());
    if (!ok) { err = "soltararchivo: " + that; return false; }
    return true;
}
// outorden <a> <b> <c>...: las filas aparecen en ESTE orden (relativo: puede haber otras en el medio)
static bool CmdOutOrder(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outorder"); if (!o) return false;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    std::string n, txt; int prev = -1;
    while (ss >> n) {
        const int i = RowOf(rows, n);
        if (i < 0) { err = "outorder: no hay una fila '" + n + "'"; return false; }
        if (i <= prev) { err = "outorder: '" + n + "' no esta despues de la anterior"; return false; }
        prev = i;
    }
    for (size_t i = 0; i < rows.size(); i++) txt += " " + rows[i].name;
    printf("      [outorden] filas:%s\n", txt.c_str());
    return true;
}
// outcolumna <tipo|tam|usos> <dx> [ancho <px esperado +-2>]: arrastra el DIVISOR de esa columna dx px (negativo = a la
// izquierda: la columna se agranda). Comprueba que con el mouse encima se resalta (hover) y que mientras se arrastra
// esta agarrado.
static bool CmdOutColumn(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outcolumn"); if (!o) return false;
    std::string c, k; int dx = 0, esp = -1; ss >> c >> dx >> k >> esp;
    const int col = c == "type" ? 0 : c == "size" ? 1 : c == "uses" ? 2 : -1;
    if (col < 0) { err = "outcolumn: tipo|tam|usos"; return false; }
    o->Resize(o->width, o->height);
    int xT, xTm, xE, xF; o->ColumnsList(xT, xTm, xE, xF);
    const int xs[3] = { xT, xTm, xE };
    const int mx = o->x + xs[col], my = o->y + o->TopContent() + (int)RowHeightGS / 2 + borderGS;
    o->event_mouse_motion(mx, my);
    if (o->colHover != col) { err = "outcolumn: el divisor no se resalta con el mouse encima"; return false; }
    leftMouseDown = true;
    LayoutClickUI(mx, my);
    if (o->colDrag != col) { leftMouseDown = false; LayoutRelease(mx, my); err = "outcolumn: no se agarro el divisor"; return false; }
    o->event_mouse_motion(mx + dx, my);
    rootViewport->Render();
    leftMouseDown = false;
    LayoutRelease(mx + dx, my);
    o->ColumnsList(xT, xTm, xE, xF);
    const int right[3] = { xTm, xE, xF }, iz[3] = { xT, xTm, xE };
    const int width = right[col] - iz[col];
    printf("      [outcolumna] %s %+d -> ancho %d (columnas %d %d %d %d)\n", c.c_str(), dx, width, xT, xTm, xE, xF);
    if (o->colDrag >= 0) { err = "outcolumn: el divisor quedo agarrado"; return false; }
    if (k == "width" && (width < esp - 2 || width > esp + 2)) { char b[96]; sprintf(b, "outcolumn: ancho %d (se esperaba %d)", width, esp); err = b; return false; }
    return true;
}

static bool CmdOutContext(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outcontext"); if (!o) return false;
    std::string n; ss >> n;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0) { err = "outcontext: no hay una fila '" + n + "'"; return false; }
    const int mx = XName(o, rows[(size_t)i]), my = YOfRow(o, i);
    if (!o->MenuContext(mx, my) || !OpenMenu) { err = "outcontext: no se abrio el menu"; return false; }
    printf("      [outcontexto] '%s':", n.c_str());
    for (size_t k = 0; k < OpenMenu->items.size(); k++) printf(" [%s]", OpenMenu->items[k]->text.c_str());
    printf("\n");
    return true;
}

// ---------------------------------------------------------------------------
//  TECLADO
// ---------------------------------------------------------------------------
static int KeyLayout(const std::string& t) {
    if (t == "up") return LayoutKey::Up;
    if (t == "down") return LayoutKey::Down;
    if (t == "left") return LayoutKey::Left;
    if (t == "right") return LayoutKey::Right;
    if (t == "ok") return LayoutKey::Enter;
    if (t == "c") return LayoutKey::Cancel;
    return -1;
}
static bool CmdOutKey(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outkey"); if (!o) return false;
    std::string t; int times = 1; ss >> t >> times;
    if (t == "5") {
        // el 5 del N95 (W3dOutlinerMarkToggle en w3dlayout.cpp llama a este mismo metodo)
        viewPortActive = o;
        for (int i = 0; i < times; i++) o->MarkCursor();
        printf("      [outtecla] 5 x%d -> seleccion %d\n", times, o->SelectionCount());
        return true;
    }
    if (t == "ctoque") {
        // el TOQUE de C del N95 (soltada sin flecha): W3dOutlinerResourcesDelete llama a este metodo
        viewPortActive = o;
        const bool used = o->KeyDeleteResources();
        printf("      [outtecla] ctoque -> %s\n", used ? "borrar recursos" : "sigue a la escena");
        return true;
    }
    const int k = KeyLayout(t);
    if (k < 0) { err = "outkey: tecla desconocida '" + t + "'"; return false; }
    viewPortActive = o;   // el N95: el panel ACTIVO (borde verde) recibe el keypad, sin mouse
    for (int i = 0; i < times; i++) LayoutKeyPanelActive(k);
    printf("      [outtecla] %s x%d -> cursor '%s'%s\n", t.c_str(), times, o->cursorKey.c_str(), o->cursorIsFolder ? " (carpeta)" : "");
    return true;
}
static bool CmdOutKeyPC(std::istringstream& ss, std::string& err) {
#ifndef W3D_SYMBIAN
    Outliner* o = ElOutliner(err, "outkeypc"); if (!o) return false;
    std::string t; ss >> t;
    int k = -1;
    if (t == "up") k = W3dK_UP; else if (t == "down") k = W3dK_DOWN;
    else if (t == "left") k = W3dK_LEFT; else if (t == "right") k = W3dK_RIGHT;
    else if (t == "enter") k = W3dK_RETURN; else if (t == "f2") k = W3dK_F2;
    else if (t == "supr") k = W3dK_DELETE; else if (t == "g") k = W3dK_G;
    else if (t == "a") k = W3dK_A; else if (t == "esc") k = W3dK_ESCAPE;
    else if (t == "shiftup") k = W3dK_UP; else if (t == "shiftdown") k = W3dK_DOWN;
    // los ATAJOS que anuncian los menus Objeto / Seleccion (con su modificador apretado)
    else if (t == "alta") k = W3dK_A; else if (t == "ctrli") k = W3dK_I;
    else if (t == "shiftd" || t == "altd") k = W3dK_D;
    if (k < 0) { err = "outkeypc: tecla desconocida '" + t + "'"; return false; }
    // LA PUERTA DEL TECLADO REAL (controls.cpp): las flechas, el Enter y el Esc van PRIMERO a la UI
    // compartida (LayoutKeyUI) con el mouse SOBRE el outliner (el panel bajo el mouse recibe el
    // teclado); solo lo que no se consume llega a event_key_down del viewport activo. Antes esto
    // llamaba a event_key_down directo y probaba un camino que el teclado de verdad no recorre.
    int lk = -1;
    if (k == W3dK_UP) lk = LayoutKey::Up; else if (k == W3dK_DOWN) lk = LayoutKey::Down;
    else if (k == W3dK_LEFT) lk = LayoutKey::Left; else if (k == W3dK_RIGHT) lk = LayoutKey::Right;
    else if (k == W3dK_RETURN) lk = LayoutKey::Enter; else if (k == W3dK_ESCAPE) lk = LayoutKey::Cancel;
    viewPortActive = o;
    const int cx = o->x + o->width / 2, cy = o->y + o->height / 2;
    LShiftPressed = (t == "shiftup" || t == "shiftdown" || t == "shiftd");
    LAltPressed = (t == "alta" || t == "altd");
    LCtrlPressed = (t == "ctrli");
    const bool byUI = (lk >= 0 && LayoutKeyUI(lk, cx, cy));
    if (!byUI) o->event_key_down(k, false);
    LShiftPressed = LAltPressed = LCtrlPressed = false;
    printf("      [outteclapc] %s -> cursor '%s' (%s)\n", t.c_str(), o->cursorKey.c_str(), byUI ? "UI compartida" : "event_key_down");
    return true;
#else
    (void)ss; err = "outkeypc: sin teclado de PC"; return false;
#endif
}
static bool CmdOutBar(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outbar"); if (!o) return false;
    std::string t; ss >> t;
    viewPortActive = o;
    const int cx = o->x + o->width / 2, cy = o->y + o->height / 2;
    if (t == "soft") LayoutToggleBarActiveViewport();
    else {
        const int k = KeyLayout(t);
        if (k < 0) { err = "outbar: tecla desconocida '" + t + "'"; return false; }
        LayoutKeyUI(k, cx, cy);
    }
    const std::string m = OpenMenuName(o);
    MenuItem* it = (m != "none" && OpenMenu) ? OpenMenu->CurrentItem() : NULL;
    printf("      [outbarra] %s -> menu %s, resaltado '%s', vista %s\n", t.c_str(), m.c_str(),
           it ? it->text.c_str() : "", KeyView(o).c_str());
    std::string k2, v2;
    if ((ss >> k2) && k2 == "menu" && (ss >> v2) && v2 != m) {
        err = "outbar: quedo abierto el menu '" + m + "', se esperaba '" + v2 + "'"; return false;
    }
    return true;
}
// outmenu <texto> | outmenu mover <carpeta|->: abre el menu OBJETO con un click en su boton y elige la opcion
// (texto en ingles); si no esta ahi, la busca en el "+" (New Folder, New Material...) y en Seleccion
static bool ClickButtonBar(Outliner* o, Button* b) {
    if (!b || !b->visible) return false;
    // un click de mouse no usa el foco de teclado de la barra (en la app lo limpia RenderBar al cerrar el
    // menu; el harness puede no haber dibujado desde la ultima navegacion con flechas)
    if (!OpenMenu || !OpenMenu->open) o->barFocusIndex = -1;
    o->barScrollManual = 0;
    o->UpdateBar();
    const int extra = (b->sx + b->width) - (o->x + o->width) + 4;
    if (extra > 0) { o->barScrollManual = extra; o->UpdateBar(); }
    const int bx = b->sx + b->width / 2, by = b->sy + b->height / 2;
    LayoutClickUI(bx, by);
    LayoutRelease(bx, by);
    return OpenMenu && OpenMenu->open;
}
static bool CmdOutMenu(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outmenu"); if (!o) return false;
    std::string rest; std::getline(ss, rest);
    size_t a = rest.find_first_not_of(' ');
    rest = (a == std::string::npos) ? std::string() : rest.substr(a);
    if (!ClickButtonBar(o, o->btnObject)) { err = "outmenu: el click no abrio el menu Objeto"; return false; }
    PopupMenu* m = OpenMenu;
    if (rest.compare(0, 5, "move ") == 0) {
        MenuItem* sub = ItemByText(m, T("Move to Folder"));
        if (!sub || !sub->submenu) { m->Close(); err = "outmenu: no hay 'Move to Folder' (el cursor no es un recurso?)"; return false; }
        const std::string c = rest.substr(5);
        MenuItem* dest = NULL;
        if (c == "-") dest = ItemByText(sub->submenu, std::string("(") + T("root") + ")");
        else dest = ItemByText(sub->submenu, W3dFolderLeaf(c));
        if (!dest) { m->Close(); err = "outmenu: 'Move to Folder' no ofrece '" + c + "'"; return false; }
        m->Close();
        sub->submenu->Run(dest->id);
        printf("      [outmenu] Move to Folder > %s\n", c.c_str());
        return true;
    }
    MenuItem* it = ItemByText(m, T(rest.c_str()));
    std::string has;
    for (size_t i = 0; i < m->items.size(); i++) has += " [" + m->items[i]->text + "]";
    if (!it) {
        m->Close();
        Button* other[2] = { o->newBtn, o->btnSelection };
        for (int k = 0; k < 2 && !it; k++) {
            if (!ClickButtonBar(o, other[k])) continue;
            m = OpenMenu;
            for (size_t i = 0; i < m->items.size(); i++) has += " [" + m->items[i]->text + "]";
            it = ItemByText(m, T(rest.c_str()));
            if (!it) m->Close();
        }
    }
    if (!it) { err = "outmenu: los menus no tienen '" + rest + "' (tienen:" + has + ")"; return false; }
    if (it->gray && !*it->gray) { m->Close(); err = "outmenu: '" + rest + "' esta deshabilitado"; return false; }
    m->Close();
    m->Run(it->id);
    printf("      [outmenu] %s\n", rest.c_str());
    return true;
}
static bool CmdOutWrite(std::istringstream& ss, std::string& err) {
    std::string t; std::getline(ss, t);
    size_t a = t.find_first_not_of(' ');
    t = (a == std::string::npos) ? std::string() : t.substr(a);
    if (!g_activeTextField) { err = "outwrite: no hay un campo de texto enfocado"; return false; }
    if (!g_activeTextField->selectAll) g_activeTextField->SelectAll();
    if (t.empty()) g_activeTextField->Backspace();   // con todo seleccionado: lo borra
    for (size_t i = 0; i < t.size(); i++) TextFieldInputChar((unsigned char)t[i]);
    // Enter (la misma rama que el teclado: sale del campo) y un cuadro (los campos confirman al perder el foco)
    Outliner* o = (Outliner*)FindLeaf(rootViewport, 9); if (!o) o = (Outliner*)FindLeaf(rootViewport, 2);
    LayoutKeyUI(LayoutKey::Enter, o ? o->x + 2 : 0, o ? o->y + 2 : 0);
    if (rootViewport) rootViewport->Render();
    printf("      [outescribir] '%s'\n", t.c_str());
    return true;
}
static bool CmdOutSel(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outsel"); if (!o) return false;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    printf("      [outsel] %d:", o->SelectionCount());
    for (std::set<std::string>::iterator it = o->selFolders.begin(); it != o->selFolders.end(); ++it) printf(" '%s/'", it->c_str());
    for (std::set<std::string>::iterator it = o->selResources.begin(); it != o->selResources.end(); ++it) printf(" '%s'", it->c_str());
    printf("\n");
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "n") {
            if (o->SelectionCount() != atoi(v.c_str())) { err = "outsel: hay " + Integer(o->SelectionCount()) + ", se esperaban " + v; return false; }
            continue;
        }
        // por fila visible (nombre o clave), o por clave directa (lo que esta plegado no se ve)
        const int i = RowOf(rows, v);
        const bool is = (i >= 0) ? o->RowInSelection(rows[(size_t)i]) : (o->selResources.count(v) || o->selFolders.count(v));
        if (k == "has" && !is) { err = "outsel: '" + v + "' no esta seleccionado"; return false; }
        if (k == "hasnot" && is) { err = "outsel: '" + v + "' esta seleccionado"; return false; }
        if (k != "has" && k != "hasnot") { err = "outsel: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
static bool CmdOutType(std::istringstream& ss, std::string& err) {
    std::string t; std::getline(ss, t);
    size_t a = t.find_first_not_of(' ');
    t = (a == std::string::npos) ? std::string() : t.substr(a);
    if (!g_activeTextField) { err = "outtype: no hay un campo de texto enfocado"; return false; }
    if (!g_activeTextField->selectAll) g_activeTextField->SelectAll();
    for (size_t i = 0; i < t.size(); i++) TextFieldInputChar((unsigned char)t[i]);
    printf("      [outtipear] '%s' (el campo dice '%s')\n", t.c_str(), g_activeTextField->text.c_str());
    return true;
}
static bool CmdOutEsc(std::string& err) {
#ifndef W3D_SYMBIAN
    // la tecla Esc de PC entrando por la MISMA puerta que el teclado de verdad
    extern void InputUserSDL3(SDL_Event&);
    SDL_Event ev; memset(&ev, 0, sizeof(ev));
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_ESCAPE;
    ev.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
    ev.key.state = SDL_PRESSED;
    const bool had = (g_activeTextField != NULL);
    InputUserSDL3(ev);
    ev.type = SDL_KEYUP; ev.key.state = SDL_RELEASED;
    InputUserSDL3(ev);
    if (rootViewport) rootViewport->Render();   // (los duenos de los campos confirman al dibujar)
    TextFieldFrameEnd();                        // el fin del cuadro (LayoutRenderMenu en el editor)
    printf("      [outesc] %s\n", had ? (g_activeTextField ? "el campo sigue enfocado" : "field descartado") : "sin campo");
    return true;
#else
    err = "outesc: sin teclado de PC"; return false;
#endif
}
static bool CmdFieldFocus(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "fieldfocus: sin panel de propiedades"; return false; }
    std::string q; ss >> q;
    PropText* pt = (q == "recname") ? p->propRecName : (q == "reccarpeta") ? p->propRecFolder :
                   (q == "objname") ? p->propNameObj : (q == "hblabel") ? p->propHbLabel : NULL;
    if (!pt) { err = "fieldfocus: recnombre|reccarpeta|nombreobj|hbetiqueta"; return false; }
    PropsActive = p;
    if (q == "hblabel") { p->activeTab = 2; p->UpdateTabs(); }   // (la tarjeta Hitbox)
    p->RefreshTargetProperties();
    pt->EditPropertie();              // la puerta del click en la fila (anota el texto de antes)
    p->RefreshTargetProperties();     // (el panel captura el destino al enfocar)
    printf("      [campoenfocar] %s = '%s'\n", q.c_str(), pt->field.text.c_str());
    if (g_activeTextField != &pt->field) { err = "fieldfocus: el campo no quedo enfocado"; return false; }
    return true;
}
static bool CmdTexLoaded(std::istringstream& ss, std::string& err) {
    std::string r, v; ss >> r >> v;
    Texture* t = TextureFind(r);
    const bool loaded = (t != NULL && t->iID != 0);
    printf("      [texcargada] '%s' %s\n", r.c_str(), loaded ? "CARGADA" : "no cargada");
    if (!v.empty() && loaded != (v == "1")) { err = "texloaded: '" + r + "' " + (loaded ? "is" : "no esta") + " cargada"; return false; }
    return true;
}
static bool CmdTexPurged(std::istringstream& ss, std::string& err) {
    const std::vector<Texture*>& tp = TexturesPurged();
    printf("      [texpurgadas] %d:", (int)tp.size());
    for (size_t i = 0; i < tp.size(); i++) printf(" '%s'%s", tp[i]->path.c_str(), tp[i]->iID ? "(con GPU!)" : "");
    printf("\n");
    std::string k; int n = -1;
    if ((ss >> k) && k == "n" && (ss >> n) && n != (int)tp.size()) {
        err = "texpurged: hay " + Integer((long)tp.size()) + ", se esperaban " + Integer(n); return false;
    }
    for (size_t i = 0; i < tp.size(); i++) if (tp[i]->iID) { err = "texpurged: una purgada conserva su GPU"; return false; }
    return true;
}
static bool CmdPairTex(std::istringstream& ss, std::string& err) {
    std::string n, r; ss >> n >> r;
    if (n.empty() || r.empty()) { err = "pairtex: <nombre> <ruta>"; return false; }
    Particles* pt = new Particles(SceneCollection);   // (el constructor lo cuelga de la escena)
    W3dRenameObject(pt, n, false);
    pt->texture = r;
    printf("      [partex] '%s' textura='%s'\n", pt->name.c_str(), r.c_str());
    return true;
}
static bool CmdFlipTex(std::istringstream& ss, std::string& err) {
    std::string n, r; ss >> n >> r;
    if (n.empty() || r.empty()) { err = "fliptex: <nombre> <ruta>"; return false; }
    Flipbook* f = NewFlipbook(n);
    if (!f) { err = "fliptex: no se pudo crear"; return false; }
    f->ConfigureStrip(r, 2, 2, 4, 8.0f);
    printf("      [fliptex] '%s' atlas='%s'\n", f->name.c_str(), f->atlas.c_str());
    return true;
}
static bool CmdTexRefOf(std::istringstream& ss, std::string& err) {
    std::string that, n, esp; ss >> that >> n >> esp;
    std::string r;
    if (that == "particle") {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
        if (!o || o->getType() != ObjectType::particles) { err = "texrefof: no hay un emisor '" + n + "'"; return false; }
        r = ((Particles*)o)->texture;
    } else if (that == "flipbook") {
        Flipbook* f = FlipbookByName(n);
        if (!f) { err = "texrefof: no hay un flipbook '" + n + "'"; return false; }
        r = f->atlas;
    } else if (that == "material") {
        Material* m = W3dMaterialById(n);
        if (!m) { err = "texrefof: no hay un material '" + n + "'"; return false; }
        r = m->texture ? m->texture->path : std::string("-");
    } else { err = "texrefof: particula|flipbook|material"; return false; }
    printf("      [texrefde] %s '%s' -> '%s'\n", that.c_str(), n.c_str(), r.c_str());
    if (!esp.empty() && r != esp) { err = "texrefof: nombra '" + r + "', se esperaba '" + esp + "'"; return false; }
    return true;
}
// outconfirmar si|no [tiene|no <texto>]: el cartel de confirmacion abierto (y que diga, o no, ese texto: en
// ingles o traducido)
static bool CmdOutConfirm(std::istringstream& ss, std::string& err) {
    std::string r, k; ss >> r >> k;
    if (!PopUpActive || PopUpActive != (PopUpBase*)confirmPopup) { err = "outconfirm: no hay un cartel de confirmacion abierto"; return false; }
    printf("      [outconfirmar] '%s' -> %s\n", confirmPopup->message.c_str(), r.c_str());
    if (k == "has" || k == "no") {
        std::string t; std::getline(ss, t);
        const size_t a = t.find_first_not_of(' ');
        t = (a == std::string::npos) ? std::string() : t.substr(a);
        // (en ingles o ya traducido, como sale en pantalla)
        const bool dice = confirmPopup->message.find(t) != std::string::npos ||
                          confirmPopup->message.find(T(t.c_str())) != std::string::npos;
        if (k == "has" && !dice) { err = "outconfirm: el cartel no dice '" + t + "'"; return false; }
        if (k == "no" && dice) { err = "outconfirm: el cartel dice '" + t + "'"; return false; }
    }
    PopUpActive->Key(r == "si" ? (int)LayoutKey::Accept : (int)LayoutKey::Cancel);
    return true;
}
static bool CmdOutRender(std::string& err) {
    if (!rootViewport) { err = "outrender: sin layout"; return false; }
    rootViewport->Render();
    w3dEngine::Finish();
    return true;
}
static bool CmdOutPx(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outpx"); if (!o) return false;
    std::string n, esp; ss >> n >> esp;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0 || rows[(size_t)i].folder) { err = "outpx: no hay un recurso '" + n + "'"; return false; }
    YOfRow(o, i);
    rootViewport->Render();
    w3dEngine::Finish();
    const int reserve = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;
    const int xi = o->x + o->width - IconSizeGS - marginGS - borderGS - reserve;
    const int yi = o->y + borderGS + o->PosY + o->TopContent() + i * (int)RowHeightGS + GlobalScale;
    std::vector<unsigned char> px((size_t)IconSizeGS * IconSizeGS * 4);
    w3dEngine::ReadPixelsRGBA(xi, W3dScreenHeight - yi - IconSizeGS, IconSizeGS, IconSizeGS, &px[0]);
    int green = 0, red = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4) {
        const int r = px[p], g = px[p + 1], b = px[p + 2];
        if (g > r + 40 && g > b + 20) green++;
        if (r > g + 60 && r > b + 60) red++;
    }
    printf("      [outpx] '%s': %d px verdes, %d px rojos\n", n.c_str(), green, red);
    const bool ok = (esp == "use") ? (green > 0 && red == 0) : (red > 0 && green == 0);
    if (!ok) { err = "outpx: el icono de '" + n + "' no es el de " + esp; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  EL RECURSO ACTIVO Y PROPERTIES
// ---------------------------------------------------------------------------
static bool CmdRecActive(std::istringstream& ss, std::string& err) {
    std::string c, id; ss >> c;
    int v = -1; std::string a;
    const bool has = W3dResourceActive(&v, &a);
    printf("      [recactivo] %s%s%s\n", has ? W3dViewKey(v) : "none", has ? " " : "", a.c_str());
    if (c.empty()) return true;
    if (c == "none") { if (has) { err = "recactive: hay un recurso activo"; return false; } return true; }
    std::getline(ss, id);
    size_t p = id.find_first_not_of(' ');
    id = (p == std::string::npos) ? std::string() : id.substr(p);
    if (!has || W3dViewKey(v) != c || a != id) { err = "recactive: el activo no es " + c + " '" + id + "'"; return false; }
    return true;
}
static bool CmdRecProps(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "recprops: sin panel de propiedades"; return false; }
    p->RefreshTargetProperties();
    p->UpdateTabs();
    // (la tarjeta del recurso vive en la pestania 3, "Malla 3D": no hay pestania aparte)
    const bool inTab = p->activeTab == 3 && p->BarTabs.size() > 3 && p->BarTabs[3]->visible && p->propResource && p->propResource->visible;
    printf("      [recprops] pestania=%d tab=%s tarjeta=%s material=%s preview=%s name='%s' carpeta='%s'\n",
           p->activeTab, inTab ? "si" : "no", (p->propResource && p->propResource->visible) ? "si" : "no",
           (p->propMaterial && p->propMaterial->visible) ? "si" : "no",
           (p->propRecPreview && p->propRecPreview->Visible()) ? "si" : "no",
           p->propRecName ? p->propRecName->field.text.c_str() : "", p->propRecFolder ? p->propRecFolder->field.text.c_str() : "");
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "tab") { if (inTab != (v == "1")) { err = "recprops: la pestania RECURSO " + std::string(inTab ? "is" : "no esta"); return false; } }
        else if (k == "material") {
            Material* m = FindMaterialByName(v);
            if (!m) { err = "recprops: no hay un material '" + v + "'"; return false; }
            // la tarjeta Material (la de siempre) BINDEADA a ese material: sus checkbox apuntan a el
            if (!p->propMaterial || !p->propMaterial->visible) { err = "recprops: la tarjeta Material no se ve"; return false; }
            if (p->propMatChk[1]->value != &m->transparent) { err = "recprops: la tarjeta Material no edita '" + v + "'"; return false; }
        }
        else if (k == "preview") {
            const bool has = p->propRecPreview && p->propRecPreview->Visible();
            if (has != (v == "1")) { err = "recprops: vista previa " + std::string(has ? "si" : "no"); return false; }
        }
        else if (k == "name") { if (p->propRecName->field.text != v) { err = "recprops: Name = '" + p->propRecName->field.text + "'"; return false; } }
        else if (k == "folder") { if (p->propRecFolder->field.text != Dash(v)) { err = "recprops: Folder = '" + p->propRecFolder->field.text + "'"; return false; } }
        else { err = "recprops: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
static bool CmdRecEdit(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "recedit: sin panel de propiedades"; return false; }
    std::string that, t; ss >> that; std::getline(ss, t);
    size_t a = t.find_first_not_of(' ');
    t = (a == std::string::npos) ? std::string() : t.substr(a);
    PropText* pt = (that == "name") ? p->propRecName : (that == "folder") ? p->propRecFolder : NULL;
    if (!pt) { err = "recedit: nombre|carpeta"; return false; }
    PropsActive = p;
    TextFieldFocus(&pt->field);       // el click en el campo lo enfoca
    p->RefreshTargetProperties();     // (el panel captura el destino al enfocar)
    pt->field.SetText(Dash(t));
    g_activeTextField = NULL;         // Enter / click afuera: pierde el foco
    p->RefreshTargetProperties();     // ...y confirma
    printf("      [receditar] %s = '%s'\n", that.c_str(), Dash(t).c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  EL MODELO (sin UI)
// ---------------------------------------------------------------------------
static bool CmdResources(std::istringstream& ss, std::string& err) {
    std::string c; ss >> c;
    const int v = ViewOf(c, err, "resources"); if (v < 0) return false;
    std::vector<W3dResourceItem> its; W3dViewRecList(v, its);
    for (size_t i = 0; i < its.size(); i++)
        printf("      [recursos] %s '%s' carpeta='%s' usuarios=%d entrada='%s' %s\n", c.c_str(), its[i].name.c_str(),
               its[i].folder.c_str(), its[i].users, its[i].entry.c_str(), its[i].info.c_str());
    std::string k; int n = -1;
    if ((ss >> k) && k == "n" && (ss >> n) && n != (int)its.size()) {
        err = "resources: " + Integer((long)its.size()) + " items, se esperaban " + Integer(n); return false;
    }
    return true;
}
static bool CmdResource(std::istringstream& ss, std::string& err) {
    std::string c, id; ss >> c >> id;
    const int v = ViewOf(c, err, "resource"); if (v < 0) return false;
    id = IdOfView(v, id);
    W3dResourceItem it;
    const bool has = W3dViewRecInfo(v, id, &it);
    printf("      [recurso] %s '%s' %s carpeta='%s' usuarios=%d name='%s'\n", c.c_str(), id.c_str(),
           has ? "exists" : "NO existe", it.folder.c_str(), it.users, it.name.c_str());
    std::string k;
    while (ss >> k) {
        std::string val; ss >> val;
        bool ok = true;
        if (k == "exists") ok = (has == (val == "1"));
        else if (!has) { err = "resource: '" + id + "' no existe"; return false; }
        else if (k == "folder") ok = (it.folder == Dash(val));
        else if (k == "users") ok = (it.users == atoi(val.c_str()));
        else if (k == "name") ok = (it.name == val);
        else { err = "resource: no entiendo '" + k + "'"; return false; }
        if (!ok) { err = "resource: '" + id + "' no cumple " + k + " " + val; return false; }
    }
    return true;
}
// carpetas [<tipo>] [n N] [tiene X]... [notiene X]...: el ARBOL UNICO de carpetas de la biblioteca (el tipo, si
// viene, se ignora: era de la fase anterior, con un arbol por tipo)
static bool CmdFolders(std::istringstream& ss, std::string& err) {
    std::string rest; std::getline(ss, rest);
    std::istringstream s2(rest);
    std::string c; s2 >> c;
    if (W3dViewByKey(c) <= W3D_VIEW_SCENE) { s2.clear(); s2.str(rest); }
    std::vector<std::string> cs; W3dFoldersAll(cs);
    printf("      [carpetas]");
    for (size_t i = 0; i < cs.size(); i++) printf(" '%s'", cs[i].c_str());
    printf("  (creadas %d)\n", (int)W3dFoldersCreated().size());
    std::string k;
    while (s2 >> k) {
        std::string val; s2 >> val;
        bool has = false;
        for (size_t i = 0; i < cs.size(); i++) if (cs[i] == val) has = true;
        if (k == "n") { if ((int)cs.size() != atoi(val.c_str())) { err = "folders: " + Integer((long)cs.size()) + " carpetas, se esperaban " + val; return false; } }
        else if (k == "has") { if (!has) { err = "folders: no esta '" + val + "'"; return false; } }
        else if (k == "hasnot") { if (has) { err = "folders: esta '" + val + "'"; return false; } }
        else { err = "folders: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
static bool CmdNewFolder(std::istringstream& ss, std::string& err) {
    const std::string parent = NoTypeKey(ss);
    std::string name; ss >> name;
    std::string path;
    if (!W3dNewFolder(Dash(parent), name, &path)) { err = "foldernew: no se pudo"; return false; }
    printf("      [carpetanueva] '%s'\n", path.c_str());
    return true;
}
static bool CmdFolderRename(std::istringstream& ss, std::string& err) {
    const std::string path = NoTypeKey(ss);
    std::string name; ss >> name;
    std::string newValue;
    if (!W3dFolderRename(path, name, &newValue)) { err = "folderrename: no se pudo"; return false; }
    printf("      [carpetarenombrar] '%s' -> '%s'\n", path.c_str(), newValue.c_str());
    return true;
}
static bool CmdFolderDelete(std::istringstream& ss, std::string& err) {
    const std::string path = NoTypeKey(ss);
    std::string f; ss >> f;
    std::string reason;
    const bool ok = W3dFolderDelete(path, &reason);
    printf("      [carpetaborrar] '%s': %s %s\n", path.c_str(), ok ? "deleted" : "NO", reason.c_str());
    if (ok == (f == "fails")) { err = "carpetaborrar: " + std::string(ok ? "se borro y no debia" : "no se borro: " + reason); return false; }
    return true;
}
static bool CmdFolderMove(std::istringstream& ss, std::string& err) {
    const std::string path = NoTypeKey(ss);
    std::string dest; ss >> dest;
    std::string newValue;
    if (!W3dFolderMove(path, Dash(dest), &newValue)) { err = "foldermove: no se pudo mudar '" + path + "'"; return false; }
    printf("      [carpetamover] '%s' -> '%s'\n", path.c_str(), newValue.c_str());
    return true;
}
static bool CmdRecMove(std::istringstream& ss, std::string& err) {
    std::string c, id, ch; ss >> c >> id >> ch;
    const int v = ViewOf(c, err, "recmove"); if (v < 0) return false;
    id = IdOfView(v, id);
    if (!W3dViewRecMove(v, id, Dash(ch))) { err = "recmove: no se pudo mover '" + id + "'"; return false; }
    printf("      [recmover] %s '%s' -> '%s'\n", c.c_str(), id.c_str(), Dash(ch).c_str());
    return true;
}
static bool CmdRecRename(std::istringstream& ss, std::string& err) {
    std::string c, id, newValue, esp; ss >> c >> id >> newValue >> esp;
    const int v = ViewOf(c, err, "recrename"); if (v < 0) return false;
    std::string remained;
    if (!W3dViewRecRename(v, id, newValue, &remained)) { err = "recrename: no se pudo renombrar '" + id + "'"; return false; }
    printf("      [recrenombrar] %s '%s' -> '%s'\n", c.c_str(), id.c_str(), remained.c_str());
    if (!esp.empty() && remained != esp) { err = "recrename: quedo '" + remained + "', se esperaba '" + esp + "'"; return false; }
    return true;
}
static bool CmdRecDelete(std::istringstream& ss, std::string& err) {
    std::string c, id, f; ss >> c >> id >> f;
    const int v = ViewOf(c, err, "recdelete"); if (v < 0) return false;
    id = IdOfView(v, id);
    std::string reason;
    const bool ok = W3dViewRecDelete(v, id, &reason);
    printf("      [recborrar] %s '%s': %s %s\n", c.c_str(), id.c_str(), ok ? "deleted" : "NO", reason.c_str());
    if (ok == (f == "fails")) { err = "recborrar: " + std::string(ok ? "se borro y no debia" : "no se borro: " + reason); return false; }
    return true;
}
static bool CmdRecPurge(std::istringstream& ss, std::string& err) {
    std::string c, k; int n = -1; ss >> c >> k >> n;
    const int v = ViewOf(c, err, "recpurge"); if (v < 0) return false;
    std::vector<std::string> names;
    const int m = W3dViewRecPurge(v, &names);
    printf("      [recpurgar] %s: %d purgado(s):", c.c_str(), m);
    for (size_t i = 0; i < names.size(); i++) printf(" '%s'", names[i].c_str());
    printf("\n");
    if (k == "n" && n != m) { err = "recpurge: purgo " + Integer(m) + ", se esperaban " + Integer(n); return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  ARMAR ESCENAS DE PRUEBA
// ---------------------------------------------------------------------------
static bool CmdMatLoose(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (n.empty()) { err = "matloose: falta el nombre"; return false; }
    Material* m = new Material(n);
    printf("      [matsuelto] '%s'\n", m->name.c_str());
    return true;
}
static bool CmdTexAssign(std::istringstream& ss, std::string& err) {
    std::string on, path; ss >> on >> path;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, on) : NULL;
    if (!o || o->getType() != ObjectType::mesh) { err = "objtexture: no hay una malla '" + on + "'"; return false; }
    Mesh* m = (Mesh*)o;
    if (m->materialsGroup.empty() || !m->materialsGroup[0].material || m->materialsGroup[0].material == MaterialDefault) {
        err = "objtexture: la parte 0 de '" + on + "' no tiene un material propio"; return false;
    }
    Texture* t = TextureTake(path);
    if (!t) { err = "objtexture: no pude cargar '" + path + "'"; return false; }
    m->materialsGroup[0].material->texture = t;
    m->materialsGroup[0].material->textureOn = true;
    printf("      [texasignar] '%s' <- '%s' (%dx%d)\n", m->materialsGroup[0].material->name.c_str(), t->path.c_str(), t->width, t->height);
    return true;
}
static bool CmdTexEntry(std::istringstream& ss, std::string& err) {
    std::string path; ss >> path;
    const std::string e = W3dImportAsset(path);
    W3dResourcesViewInvalidate();
    printf("      [texentrada] '%s' -> '%s'\n", path.c_str(), e.c_str());
    if (e.empty() || e == path) { err = "texentry: no entro al contenedor (hay uno montado?)"; return false; }
    return true;
}
static bool CmdJsonHas(std::istringstream& ss, std::string& err) {
    std::string path, text, neg; ss >> path >> text >> neg;
    W3dZipReader z;
    std::vector<unsigned char> d;
    if (!z.Open(path) || !z.Read("project.json", d)) { err = "jsontiene: no pude leer el project.json de '" + path + "'"; return false; }
    const std::string s(d.empty() ? "" : std::string((const char*)&d[0], d.size()));
    const bool has = s.find(text) != std::string::npos;
    printf("      [jsontiene] '%s' %s '%s'\n", path.c_str(), has ? "TIENE" : "NO tiene", text.c_str());
    if (has == (neg == "0")) { err = "jsontiene: " + std::string(has ? "has" : "no tiene") + " '" + text + "'"; return false; }
    return true;
}

static bool CmdSelection(std::istringstream& ss, std::string& err) {
    printf("      [seleccion] %d:", (int)ObjSelects.size());
    for (size_t i = 0; i < ObjSelects.size(); i++) printf(" '%s'", ObjSelects[i] ? ObjSelects[i]->name.c_str() : "?");
    printf("%s%s\n", ActiveObj ? "  active=" : "", ActiveObj ? ActiveObj->name.c_str() : "");
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "n") { if ((int)ObjSelects.size() != atoi(v.c_str())) { err = "selection: hay " + Integer((long)ObjSelects.size()) + ", se esperaban " + v; return false; } }
        else if (k == "has") {
            bool has = false;
            for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i] && ObjSelects[i]->name == v) has = true;
            if (!has) { err = "selection: '" + v + "' no esta seleccionado"; return false; }
        }
        else if (k == "hasnot") {
            for (size_t i = 0; i < ObjSelects.size(); i++)
                if (ObjSelects[i] && ObjSelects[i]->name == v) { err = "selection: '" + v + "' esta seleccionado"; return false; }
        }
        else if (k == "mode") {   // objeto | edit: el modo de interaccion (un OK no puede cambiarlo solo)
            const bool isObj = (InteractionMode == ObjectMode);
            if ((v == "object") != isObj) { err = std::string("selection: el modo es ") + (isObj ? "object" : "other"); return false; }
        }
        else { err = "selection: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// ---- el PROVEEDOR DE PRUEBA (una vista sin datos todavia: prefabs / escenas / librerias) ----
struct ItemTest { std::string name, folder; int users; bool readOnly; };
class TestProvider : public W3dResourceProvider {
public:
    std::vector<ItemTest> items;
    int icon;
    TestProvider() : icon(-1) {}
    void Restart(int ic) {
        icon = ic;
        items.clear();
        ItemTest a; a.name = "Alfa"; a.users = 2; a.readOnly = false; items.push_back(a);
        ItemTest b; b.name = "Beta"; b.users = 0; b.readOnly = false; items.push_back(b);
        ItemTest g; g.name = "Gamma"; g.users = 1; g.readOnly = true; g.folder = "Externo"; items.push_back(g);
    }
    int Find(const std::string& id) const {
        for (size_t i = 0; i < items.size(); i++) if (items[i].name == id) return (int)i;
        return -1;
    }
    void List(std::vector<W3dResourceItem>& out) {
        out.clear();
        for (size_t i = 0; i < items.size(); i++) {
            W3dResourceItem it;
            it.id = it.name = items[i].name;
            it.folder = items[i].folder;
            it.icon = icon;
            it.users = items[i].users;
            it.readOnly = items[i].readOnly;
            it.info = "test provider";
            out.push_back(it);
        }
    }
    bool SetFolder(const std::string& id, const std::string& folder) {
        const int i = Find(id); if (i < 0) return false;
        items[(size_t)i].folder = folder; return true;
    }
    bool Rename(const std::string& id, const std::string& newValue, std::string* final) {
        const int i = Find(id); if (i < 0) return false;
        std::vector<std::string> other;
        for (size_t k = 0; k < items.size(); k++) if ((int)k != i) other.push_back(items[k].name);
        items[(size_t)i].name = W3dUniqueNameInValues(newValue, "Prefab", other, -1);
        if (final) *final = items[(size_t)i].name;
        return true;
    }
    bool Delete(const std::string& id, std::string* reason) {
        const int i = Find(id);
        if (i < 0) { if (reason) *reason = "It doesn't exist anymore"; return false; }
        items.erase(items.begin() + (long)i); return true;
    }
};
static TestProvider gProvTest;
static bool CmdRecProvTest(std::istringstream& ss, std::string& err) {
    std::string c, onoff; ss >> c >> onoff;
    const int v = ViewOf(c, err, "recprovtest"); if (v < 0) return false;
    if (v != W3D_VIEW_PREFABS && v != W3D_VIEW_SCENES && v != W3D_VIEW_LIBS) {
        err = "recprovtest: solo en una vista sin datos (prefabs|escenas|librerias)"; return false;
    }
    // (el de prueba PISA al que la vista tenga -escenas y prefabs ya tienen uno real- y 'off' lo devuelve)
    static W3dResourceProvider* previous[W3D_VIEWS] = { 0 };
    if (onoff == "on") {
        if (W3dResourcesProviderView(v) != &gProvTest) previous[v] = W3dResourcesProviderView(v);
        gProvTest.Restart(W3dIconView(v)); W3dResourcesViewRegister(v, &gProvTest);
    }
    else W3dResourcesViewRegister(v, previous[v]);
    // la biblioteca se re-arma sola (el filtro que se miraba sigue)
    Outliner* o = (Outliner*)FindLeaf(rootViewport, 9); if (!o) o = (Outliner*)FindLeaf(rootViewport, 2);
    if (o && o->InLibrary()) o->ChangeFilter(o->filter);
    printf("      [recprovprueba] %s %s\n", c.c_str(), onoff.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  BORDES (los arreglos de la revision de la fase: prueba_outliner_bordes.w3s)
// ---------------------------------------------------------------------------
// una entrada del contenedor montado con un nombre EXACTO (el overlay de editadas, como un .lua que
// guardo el IDE): 'entryraw' con los bytes de un archivo, 'entrytext' con el resto de la linea
static bool CmdEntryRaw(std::istringstream& ss, std::string& err, bool text) {
    std::string e, rest; ss >> e; std::getline(ss, rest);
    size_t a = rest.find_first_not_of(' ');
    rest = (a == std::string::npos) ? std::string() : rest.substr(a);
    std::vector<unsigned char> d;
    if (text) d.assign(rest.begin(), rest.end());
    else if (!w3dFileSystem::ReadFileBytes(rest, d) || d.empty()) { err = "entryraw: no pude leer '" + rest + "'"; return false; }
    if (d.empty() || !W3dContainerWriteEntry(e, &d[0], d.size())) {
        err = "entryraw: no se pudo escribir '" + e + "' (hay un contenedor montado? es un nombre de entrada?)"; return false;
    }
    W3dResourcesViewInvalidate();   // el listado de entradas (y el texto de los scripts) cambio
    printf("      [%s] '%s' (%u bytes)\n", text ? "entrytext" : "entryraw", e.c_str(), (unsigned)d.size());
    return true;
}
// el color del FONDO (la franja) de la fila, en la columna de la flechita (vacia en un recurso)
static bool BackgroundColorRow(Outliner* o, int i, const OutRowRec& f, unsigned char* rgb) {
    YOfRow(o, i);
    if (!rootViewport) return false;
    rootViewport->Render();
    w3dEngine::Finish();
    const int px = o->x + marginGS + o->PosX + f.prof * (IconSizeGS + gapGS) + IconSizeGS / 2;
    const int py = o->RowResourceY(i);
    unsigned char c[4] = { 0, 0, 0, 0 };
    w3dEngine::ReadPixelsRGBA(px, W3dScreenHeight - py - 1, 1, 1, c);
    rgb[0] = c[0]; rgb[1] = c[1]; rgb[2] = c[2];
    return true;
}
static bool ColorNear(const unsigned char* a, const GLubyte* b) {
    for (int k = 0; k < 3; k++) { int d = (int)a[k] - (int)b[k]; if (d < 0) d = -d; if (d > 3) return false; }
    return true;
}
// outfondo <nombre> sel|nosel: el fondo de la fila es (o no) el de la SELECCION MULTIPLE (dibujando
// la UI entera, con la lista scrolleada hasta la fila como la dejaria un click)
static bool CmdOutBackground(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outbackground"); if (!o) return false;
    std::string n, esp; ss >> n >> esp;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0) { err = "outbackground: no hay una fila '" + n + "'"; return false; }
    unsigned char c[3];
    if (!BackgroundColorRow(o, i, rows[(size_t)i], c)) { err = "outbackground: sin layout"; return false; }
    const GLubyte* a = ColorListUbyte[static_cast<int>(ColorID::accentDark)];
    const GLubyte* b = ColorListUbyte[static_cast<int>(ColorID::background)];
    const GLubyte sel[3] = { (GLubyte)((a[0] * 3 + b[0] * 2) / 5), (GLubyte)((a[1] * 3 + b[1] * 2) / 5), (GLubyte)((a[2] * 3 + b[2] * 2) / 5) };
    const bool isSel = ColorNear(c, sel);
    printf("      [outfondo] '%s' fila %d (PosY %d): rgb %d %d %d -> %s\n", n.c_str(), i, o->PosY, c[0], c[1], c[2],
           isSel ? "SELECCION" : "other");
    if ((esp == "sel") != isSel) { err = "outbackground: el fondo de '" + n + "' " + (isSel ? "es" : "no es") + " el de la seleccion"; return false; }
    return true;
}
// outdragsobre <nombre> <sobre> <resaltada|->: arrastra la fila hasta 'sobre' SIN soltar ahi: la fila
// resaltada como DESTINO (la carpeta a donde iria, '-' = ninguna: la raiz) y su fondo dibujado; despues
// se suelta afuera del outliner (no mueve nada)
static bool CmdOutDragOver(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outdragover"); if (!o) return false;
    std::string n, d, esp; ss >> n >> d >> esp;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n), j = RowOf(rows, d);
    if (i < 0 || j < 0) { err = "outdragover: no hay una fila '" + (i < 0 ? n : d) + "'"; return false; }
    const int x0 = XName(o, rows[(size_t)i]), y0 = YOfRow(o, i);
    leftMouseDown = true;
    LayoutClickUI(x0, y0);
    const int y1 = YOfRow(o, j), x1 = XName(o, rows[(size_t)j]);
    o->event_mouse_motion(x0, y0 + (y1 > y0 ? 1 : -1) * (int)RowHeightGS);
    o->event_mouse_motion(x1, y1);
    const int df = o->recDropRow;
    const std::string res = (df >= 0 && df < (int)rows.size()) ? rows[(size_t)df].key : std::string("-");
    bool painted = true;
    unsigned char c[4] = { 0, 0, 0, 0 };
    if (df >= 0 && df < (int)rows.size()) {
        YOfRow(o, df);   // (la carpeta destino a la vista: puede estar bien arriba de lo apuntado)
        rootViewport->Render();
        w3dEngine::Finish();
        // (la columna de estado: una CARPETA no dibuja nada ahi, se ve la franja pelada)
        const int reserve = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;
        const int px = o->x + o->width - IconSizeGS - marginGS - borderGS - reserve + IconSizeGS / 2;
        w3dEngine::ReadPixelsRGBA(px, W3dScreenHeight - o->RowResourceY(df) - 1, 1, 1, c);
        painted = ColorNear(c, ColorListUbyte[static_cast<int>(ColorID::accentDark)]);
    }
    leftMouseDown = false;
    LayoutRelease(o->x + o->width + 50, y1);   // afuera: no mueve nada
    printf("      [outdragsobre] '%s' sobre '%s': destino resaltado '%s' (rgb %d %d %d)%s\n", n.c_str(), d.c_str(),
           res.c_str(), c[0], c[1], c[2], painted ? "" : " SIN el fondo del drop");
    if (res != esp) { err = "outdragover: resalta '" + res + "', se esperaba '" + esp + "'"; return false; }
    if (!painted) { err = "outdragover: la fila destino no tiene el fondo del drop"; return false; }
    return true;
}
// recboton usuarios|borrar [oculto]: el boton de la tarjeta del recurso en Properties ("Select Users" /
// "Delete"): con 'oculto' asserta que NO se ofrece; si no, lo aprieta (su accion, como el click)
// recboton usuarios|borrar|accion|parar [oculto]: un boton de la tarjeta del recurso ('accion' = el del tipo:
// Play de un sonido, Open de una escena / prefab, Open in IDE de un script; 'parar' = Stop del sonido)
static bool CmdRecButton(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p || !p->propRecSelUsers || !p->propRecDelete) { err = "recbutton: sin la tarjeta Recurso"; return false; }
    std::string q, m; ss >> q >> m;
    const bool ofType = (q == "action" || q == "stop");
    const int k = (q == "users" || q == "action") ? 0 : (q == "delete" || q == "stop") ? 1 : -1;
    if (k < 0) { err = "recbutton: usuarios|borrar|accion|parar"; return false; }
    p->RefreshTargetProperties();
    p->UpdateTabs();
    // (Seleccionar usuarios y Borrar: una fila cada uno; lo del tipo: la fila de dos botones)
    std::string text;
    bool visible = false;
    void (*action)() = NULL;
    if (ofType) {
        PropButtonRow* row = p->propRecActions2;
        if (!row || row->buttons.size() <= (size_t)k) { err = "recbutton: sin esa fila de botones"; return false; }
        text = row->buttons[(size_t)k]->text;
        visible = row->buttons[(size_t)k]->visible;
        action = row->actions[(size_t)k];
    } else {
        PropButton* b = (k == 0) ? p->propRecSelUsers : p->propRecDelete;
        text = b->button->text;
        visible = !b->hidden;
        action = b->action;
    }
    visible = visible && p->propResource && p->propResource->visible;
    printf("      [recboton] %s ('%s'): %s\n", q.c_str(), text.c_str(), visible ? "visible" : "hidden");
    if (m == "hidden") {
        if (visible) { err = "recbutton: '" + q + "' se ofrece y no debia"; return false; }
        return true;
    }
    if (!visible) { err = "recbutton: '" + q + "' no se ofrece"; return false; }
    PropsActive = p;
    if (action) action();
    return true;
}
// recdatos <clave> <id> <en|es|pt> <texto...>: los DATOS del recurso (los de la tarjeta Recurso) en ese
// idioma, exactos
static bool CmdRecData(std::istringstream& ss, std::string& err) {
    std::string c, id, idi, esp; ss >> c >> id >> idi; std::getline(ss, esp);
    size_t a = esp.find_first_not_of(' ');
    esp = (a == std::string::npos) ? std::string() : esp.substr(a);
    const int v = ViewOf(c, err, "recdata"); if (v < 0) return false;
    const W3dLanguage prev = g_language;
    W3dLanguageSet(W3dLanguageOf(idi.c_str()));
    W3dResourceItem it;
    const bool has = W3dViewRecInfo(v, id, &it);
    W3dLanguageSet(prev);
    printf("      [recdatos] %s '%s' (%s): '%s'\n", c.c_str(), id.c_str(), idi.c_str(), it.info.c_str());
    if (!has) { err = "recdata: no existe '" + id + "'"; return false; }
    if (it.info != esp) { err = "recdata: dice '" + it.info + "', se esperaba '" + esp + "'"; return false; }
    return true;
}
// texdormidas [asignar <objeto> <parte> <material>] [cuadros K] [n N]: los materiales con texturas
// DORMIDAS (import_obj.h). 'asignar' le pone el material a una parte SIN pasar por Properties (como lo
// deja un undo o un script); 'cuadros' corre K veces la cola diferida del loop principal (la que
// despierta las que una malla empezo a usar)
static bool CmdTexAsleep(std::istringstream& ss, std::string& err) {
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "assign") {
            int part  = -1; std::string mn; ss >> part >> mn;
            Object* o = SceneCollection ? FindObjectByName(SceneCollection, v) : NULL;
            if (!o || o->getType() != ObjectType::mesh) { err = "texasleep: no hay una malla '" + v + "'"; return false; }
            Mesh* m = (Mesh*)o;
            Material* mat = FindMaterialByName(mn);
            if (!mat || part < 0 || part >= (int)m->materialsGroup.size()) { err = "texasleep: parte o material invalido"; return false; }
            m->materialsGroup[(size_t)part].material = mat;
        }
        else if (k == "frames") { const int n = atoi(v.c_str()); for (int i = 0; i < n; i++) LoadPendingTextures(); }
        else if (k == "n") {
            if (AsleepTexturesCount() != atoi(v.c_str())) {
                err = "texasleep: hay " + Integer(AsleepTexturesCount()) + ", se esperaban " + v; return false;
            }
        }
        else { err = "texasleep: no entiendo '" + k + "'"; return false; }
    }
    printf("      [texdormidas] %d material(es) con texturas dormidas, %d en la cola\n", AsleepTexturesCount(), PendingTextures());
    return true;
}


// ============================================================================
//  FASE 4b: LA BIBLIOTECA (el outliner como explorador del .w3d)
// ============================================================================
extern int OutlinerObjectRow(Object* o);   // Outliner.cpp: la fila visible de un objeto en la escena (-1)
extern int OutlinerYOfRow(int row);        // Outliner.cpp: su Y desde arriba del arbol (con los cortes del constructor)
extern void PropsDestinationsResource(Properties* p, int type, std::vector<int>& rects);   // Properties.cpp
// el punto del NOMBRE de una fila de cualquiera de las dos vistas: por nombre (la escena: el del objeto)
static bool PointByName(Outliner* o, const std::string& n, int& mx, int& my, std::string& err, const char* cmd) {
    if (o->view == OUT_VIEW_SCENE) {
        Object* ob = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
        const int row = ob ? OutlinerObjectRow(ob) : -1;
        if (row < 0) { err = std::string(cmd) + ": no hay una fila de objeto '" + n + "'"; return false; }
        int prof = 0;
        for (Object* p = ob->Parent; p && p != SceneCollection; p = p->Parent) prof++;
        mx = o->x + marginGS + o->PosX + (prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
        my = o->y + borderGS + o->PosY + o->BarTopOffset() + OutlinerYOfRow(row) + (int)RowHeightGS / 2;
        return true;
    }
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0) { Dump(rows, cmd); err = std::string(cmd) + ": no hay una fila '" + n + "'"; return false; }
    my = YOfRow(o, i);
    mx = XName(o, rows[(size_t)i]);
    return true;
}
// outdobleclick <nombre> [ojo|flecha]: DOBLE CLICK sobre el nombre (las dos vistas): el primer click elige y el
// segundo, que llega con clicks == 2 (controls.cpp), renombra en linea. 'ojo' (la vista Escena) / 'flecha' (una
// carpeta de la biblioteca) = el doble click en ESA columna de la fila, que no renombra (con 'fail')
static bool CmdOutDoubleClick(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outdoubleclick"); if (!o) return false;
    std::string n, where; ss >> n >> where;
    int mx, my;
    if (!PointByName(o, n, mx, my, err, "outdoubleclick")) return false;
    if (where == "eye") {
        const int reserveBar = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;
        mx = o->x + o->width - 2 * IconSizeGS - gapGS - marginGS - borderGS - reserveBar + IconSizeGS / 2;
    } else if (where == "arrow") {
        std::vector<OutRowRec> rows; o->RowsResources(rows);
        const int i = RowOf(rows, n);
        if (i < 0) { err = "outdoubleclick: 'flecha' es de una fila de la biblioteca"; return false; }
        mx = XArrow(o, rows[(size_t)i]);
    } else if (!where.empty()) { err = "outdoubleclick: no entiendo '" + where + "'"; return false; }
    // la CUADRICULA: el doble click en una carpeta (o "Ir atras") ENTRA en ella, no la renombra
    std::string enterTo; bool enters = false;
    if (o->view != OUT_VIEW_SCENE && o->grid) {
        std::vector<OutRowRec> rows; o->RowsResources(rows);
        const int i = RowOf(rows, n);
        if (i >= 0 && rows[(size_t)i].folder) { enters = true; enterTo = rows[(size_t)i].key; }
    }
    for (int k = 0; k < 2; k++) {
        leftMouseDown = true;
        LayoutClickUI(mx, my);
        if (k == 1) LayoutDoubleClickUI(mx, my);
        leftMouseDown = false;
        LayoutRelease(mx, my);
    }
    if (enters) {
        printf("      [outdobleclick] '%s' -> la cuadricula muestra '%s'\n", n.c_str(), o->folderGrid.c_str());
        if (o->folderGrid != enterTo) { err = "outdoubleclick: no se entro a la carpeta '" + enterTo + "' (se ve '" + o->folderGrid + "')"; return false; }
        return true;
    }
    printf("      [outdobleclick] '%s' -> %s (campo '%s')\n", n.c_str(), o->renaming ? "renaming" : "NADA",
           o->rename.text.c_str());
    if (!o->renaming || g_activeTextField != &o->rename) { err = "outdoubleclick: el nombre no se volvio un campo"; return false; }
    return true;
}
// outtap <nombre> [doble]: un TOQUE de dedo (el click diferido de controls.cpp: g_uiTapInProgress). No arma arrastre
// (con el dedo arrastrar es scroll). 'doble' = dos toques seguidos (el doble tap: renombrar)
static bool CmdOutTap(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outtap"); if (!o) return false;
    std::string n, d; ss >> n >> d;
    int mx, my;
    if (!PointByName(o, n, mx, my, err, "outtap")) return false;
    extern bool g_uiTapInProgress;
    const int times = (d == "double") ? 2 : 1;
    bool built = false;
    for (int k = 0; k < times; k++) {
        LayoutRelease(mx, my);          // (el up llega antes que el click diferido, como en controls.cpp)
        g_uiTapInProgress = true;
        LayoutClickUI(mx, my);
        built = built || o->recDrag || o->dragObject;
        if (k == 1) LayoutDoubleClickUI(mx, my);
        g_uiTapInProgress = false;
        o->dragObject = NULL;
    }
    printf("      [outtap] '%s'%s: arrastre %s, %s\n", n.c_str(), times == 2 ? " (doble)" : "", built ? "ARMADO" : "no",
           o->renaming ? "renaming" : "sin renombrar");
    if (built && o->view != OUT_VIEW_SCENE) { err = "outtap: un toque armo un arrastre (con el dedo, arrastrar es scroll)"; return false; }
    if (times == 2 && !o->renaming) { err = "outtap: el doble tap no renombro"; return false; }
    return true;
}
// outmantener <nombre> [soltar | mover <destino|-|3d>]: la PULSACION LARGA del dedo (LayoutLongPressUI, la que
// dispara controls.cpp a los ~0,5 s): la fila queda AGARRADA; 'soltar' = levantar sin mover (el menu contextual);
// 'mover' = arrastrarla hasta el destino (una carpeta / un objeto) y soltarla ahi; 'mover 3d' = llevar el recurso
// AFUERA, al centro del viewport 3D, y soltarlo ahi (lo de outsoltar 3d, pero con el dedo)
static bool CmdOutKeep(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outkeep"); if (!o) return false;
    std::string n, that, dest; ss >> n >> that >> dest;
    int mx, my;
    if (!PointByName(o, n, mx, my, err, "outkeep")) return false;
    if (OpenMenu) OpenMenu->Close();
    if (!LayoutLongPressUI(mx, my)) { err = "outkeep: la pulsacion larga no agarro la fila"; return false; }
    printf("      [outmantener] '%s' agarrada (fila %d)\n", n.c_str(), o->grabRow);
    if (o->grabRow < 0) { err = "outkeep: no quedo agarrada (sin la marca)"; return false; }
    if (that == "move") {
        int x1 = mx, y1 = my;
        const bool to3d = (dest == "3d");
        if (to3d) {
            ViewportBase* v = FindLeaf(rootViewport, 1);
            if (!v) { err = "outkeep: no hay viewport 3D"; return false; }
            rootViewport->Render();   // (el 3D sabe su lente)
            x1 = v->x + v->width / 2; y1 = v->y + v->height / 2;
        } else if (dest == "-") y1 = o->y + o->height - (int)RowHeightGS;
        else if (!PointByName(o, dest, x1, y1, err, "outkeep")) return false;
        // (sin el boton del mouse: es el dedo; controls.cpp manda el motion al outliner mientras dure el agarre)
        o->event_mouse_motion(mx, my + (y1 > my ? 1 : -1) * (int)RowHeightGS);
        o->event_mouse_motion(x1, y1);
        const bool outside    = g_outDrag.active;
        const int destination = g_outDrag.destination;
        LayoutRelease(x1, y1);
        printf("      [outmantener] '%s' soltada sobre '%s'%s\n", n.c_str(), dest.c_str(),
               to3d ? (outside ? (destination > 0 ? " (arrastre afuera, destino valido)" : " (arrastre afuera, destino NO valido)")
                              : " (el arrastre afuera NO arranco)") : "");
        if (to3d && (!outside || destination <= 0)) { err = "outkeep: el arrastre al 3D no llego a un destino valido"; return false; }
        return true;
    }
    if (that == "release") {
        LayoutRelease(mx, my);
        const bool menu = OpenMenu && OpenMenu->open;
        printf("      [outmantener] soltada sin mover: menu contextual %s\n", menu ? "ABIERTO" : "no");
        if (!menu) { err = "outkeep: soltar sin mover no abrio el menu contextual"; return false; }
        if (OpenMenu) OpenMenu->Close();
    }
    return true;
}
// outfiltro <todo|tipo|sinuso>: el FILTRO de la biblioteca por su menu ('sinuso' alterna "Unused": los que nadie usa)
static bool CmdOutFilter(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outfilter"); if (!o) return false;
    std::string c; ss >> c;
    if (c == "unused") {
        if (!o->InLibrary()) { err = "outfilter: el outliner no esta en la biblioteca"; return false; }
        const bool prev = o->onlyUnused;
        if (!PickInMenuBar(o, o->btnFilter, 9100, err, "outfilter")) return false;
        printf("      [outfiltro] sin uso: %s\n", o->onlyUnused ? "si" : "no");
        if (o->onlyUnused == prev) { err = "outfilter: 'Unused' no cambio"; return false; }
        return true;
    }
    const int f = (c == "all") ? 0 : W3dViewByKey(c);
    if (f < 0) { err = "outfilter: tipo desconocido '" + c + "'"; return false; }
    if (!o->InLibrary()) { err = "outfilter: el outliner no esta en la biblioteca"; return false; }
    if (!PickInMenuBar(o, o->btnFilter, f, err, "outfilter")) return false;
    printf("      [outfiltro] %s (boton '%s')\n", c.c_str(), o->btnFilter->text.c_str());
    return true;
}
// outcuadricula 0|1: el interruptor lista / cuadricula de la barra (un click de verdad)
static bool CmdOutGrid(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outgrid"); if (!o) return false;
    std::string v; ss >> v;
    if (o->grid != (v == "1")) {
        // por el OJO de la barra (como el mouse): se abre su menu y se elige Lista / Cuadricula
        Button* b = o->btnOptions;
        if (!b || !b->visible) { err = "outgrid: el ojo no se ve"; return false; }
        // (como ClickButtonBar: sin el foco de teclado de la barra y con el boton a la vista)
        if (!OpenMenu || !OpenMenu->open) o->barFocusIndex = -1;
        o->barScrollManual = 0;
        o->UpdateBar();
        const int extra = (b->sx + b->width) - (o->x + o->width) + 4;
        if (extra > 0) { o->barScrollManual = extra; o->UpdateBar(); }
        const int bx = b->sx + b->width / 2, by = b->sy + b->height / 2;
        LayoutClickUI(bx, by);
        LayoutRelease(bx, by);
        MenuItem* it = (OpenMenu && OpenMenu->open) ? ItemByText(OpenMenu, v == "1" ? T("Grid") : T("List")) : NULL;
        if (!it) { err = "outgrid: el menu del ojo no tiene Lista / Cuadricula"; return false; }
        PopupMenu* m = OpenMenu;
        m->Close(); OpenMenu = NULL;
        m->Run(it->id);
    }
    printf("      [outcuadricula] %s\n", o->grid ? "grid" : "list");
    if (o->grid != (v == "1")) { err = "outgrid: no cambio"; return false; }
    return true;
}
// outmini <nombre> [si|no] [cell] [vividcolor] [empty] [center] [red|green|blue] [png <ruta>]: la MINIATURA de la celda
// (se dibuja la cuadricula hasta que este: son perezosas). 'si' = tiene una (la textura GL); 'vividcolor' = sus
// pixeles no son todos del mismo color; 'empty' = no se dibujo nada (todo transparente: la camara no ve
// nada); 'center' = el pixel del medio esta pintado (lo que la camara mira de frente); red|green|blue = ese canal domina
// en el medio. Sin 'cell' se miran
// los pixeles generados en el momento; con 'celda', los DIBUJADOS en la celda (la textura del CACHE, la que
// ve el usuario: prueba que se rehizo): pintado = distinto del fondo de la celda. 'png' los vuelca
static bool CmdOutMini(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outmini"); if (!o) return false;
    std::string n, esp, k, png; ss >> n >> esp;
    std::vector<std::string> asserts;
    bool inCell = false;
    while (ss >> k) {
        if (k == "png") ss >> png;
        else if (k == "cell") inCell = true;
        else if (k == "vividcolor" || k == "empty" || k == "center" || k == "red" || k == "green" || k == "blue") asserts.push_back(k);
        else { err = "outmini: no entiendo '" + k + "'"; return false; }
    }
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0 || rows[(size_t)i].folder) { err = "outmini: no hay un recurso '" + n + "'"; return false; }
    YOfRow(o, i);
    unsigned int tex = 0;
    for (int attempt = 0; attempt < 8 && !tex; attempt++) {
        rootViewport->Render();
        tex = o->RowThumbnail(rows[(size_t)i], NULL, NULL);   // (la MISMA que dibuja la celda)
    }
    std::vector<unsigned char> px; int w = 0, h = 0;
    bool has = false;
    if (inCell) {
        if (!o->grid) { err = "outmini: 'celda' necesita la cuadricula"; return false; }
        // un cuadro mas con todo en el cache (la miniatura ya generada) y se lee lo dibujado en su lugar de la
        // celda (el mismo calculo que RenderResources)
        rootViewport->Render();
        w3dEngine::Finish();
        int rx, ry, rw, rh; o->RectRow(rows, i, rx, ry, rw, rh);
        const int side = o->CellSize() - 2 * gapGS - (int)RowHeightGS;
        const int x0 = o->x + rx + (rw - side) / 2, y0 = o->y + o->PosY + ry + gapGS;
        if (side <= 0) { err = "outmini: celda sin lugar para la miniatura"; return false; }
        std::vector<unsigned char> fb((size_t)side * side * 4), bg(4);
        w3dEngine::ReadPixelsRGBA(x0, W3dScreenHeight - (y0 + side), side, side, &fb[0]);
        w3dEngine::ReadPixelsRGBA(o->x + rx + GlobalScale, W3dScreenHeight - (y0 + side / 2) - 1, 1, 1, &bg[0]);
        // (el framebuffer viene de abajo hacia arriba: se da vuelta para que la fila 0 sea la de arriba)
        px.resize(fb.size());
        for (int y = 0; y < side; y++)
            for (int x = 0; x < side; x++) {
                const unsigned char* s = &fb[((size_t)(side - 1 - y) * side + x) * 4];
                unsigned char* d = &px[((size_t)y * side + x) * 4];
                const int dif = abs((int)s[0] - bg[0]) + abs((int)s[1] - bg[1]) + abs((int)s[2] - bg[2]);
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = (unsigned char)(dif > 24 ? 255 : 0);
            }
        w = h = side;
        has = tex != 0;
    } else has = !o->ViewReadOnly() && W3dThumbnailPixels(rows[(size_t)i].type, rows[(size_t)i].id, px, w, h);
    int distinct = 0, opaque = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4) {
        if (px[p + 3] > 0) opaque++;
        if (px[p] != px[0] || px[p + 1] != px[1] || px[p + 2] != px[2]) distinct++;
    }
    const size_t middle = ((size_t)(h / 2) * (size_t)w + (size_t)(w / 2)) * 4 + 3;
    const bool center = has && middle < px.size() && px[middle] > 0;
    printf("      [outmini] '%s'%s: textura %u, pixeles %s (%dx%d, %d opacos, %d distintos del primero, centro %s)\n", n.c_str(),
           inCell ? " (la celda)" : "", tex, has ? "si" : "no", w, h, opaque, distinct, center ? "painted" : "empty");
    if (!png.empty() && has && !px.empty() && !w3dEngine::SavePNG(png.c_str(), &px[0], w, h, false)) { err = "outmini: no pude escribir " + png; return false; }
    if (esp == "si" && !tex) { err = "outmini: '" + n + "' no tiene miniatura"; return false; }
    if (esp == "no" && tex) { err = "outmini: '" + n + "' tiene miniatura y no debia"; return false; }
    for (size_t a = 0; a < asserts.size(); a++) {
        if (asserts[a] == "vividcolor" && (opaque == 0 || distinct == 0)) { err = "outmini: la miniatura de '" + n + "' esta vacia o es de un solo color"; return false; }
        if (asserts[a] == "empty" && (!has || opaque > 0)) { err = "outmini: la miniatura de '" + n + "' tiene algo dibujado (o no hay)"; return false; }
        if (asserts[a] == "center" && !center) { err = "outmini: el medio de la miniatura de '" + n + "' esta vacio"; return false; }
        // red|green|blue: ese canal es el que domina en el medio (de QUE material es la miniatura)
        if (asserts[a] == "red" || asserts[a] == "green" || asserts[a] == "blue") {
            const int c = asserts[a] == "red" ? 0 : asserts[a] == "green" ? 1 : 2;
            const unsigned char* m = center ? &px[middle - 3] : NULL;
            if (!m || m[c] <= m[(c + 1) % 3] || m[c] <= m[(c + 2) % 3]) {
                err = "outmini: el medio de la miniatura de '" + n + "' no es " + asserts[a]; return false;
            }
        }
    }
    return true;
}
// idebib ensuciar | abrir <ruta> | archivo <ruta> | sucio 0|1 | tiene <texto> | notiene <texto>
//   el PRIMER IDE del layout (vpkind lo pone) y el "Open in IDE" de la biblioteca (IDEOpenScript, lo que llama
//   el boton de Properties): 'ensuciar' tipea una linea sin guardar; 'abrir' aprieta el boton con ese script
static IDE* FirstIDEOfLayout(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == 8 ? (IDE*)n : NULL;
    IDE* a = FirstIDEOfLayout(((ViewportColumn*)n)->childA);
    return a ? a : FirstIDEOfLayout(((ViewportColumn*)n)->childB);
}
static bool CmdIdeBib(std::istringstream& ss, std::string& err) {
    IDE* ide = FirstIDEOfLayout(rootViewport);
    if (!ide) { err = "idelibrary: no hay un IDE en el layout (vpkind 5 7)"; return false; }
    std::string sub, arg; ss >> sub;
    std::getline(ss, arg);
    const size_t a = arg.find_first_not_of(' ');
    arg = (a == std::string::npos) ? std::string() : arg.substr(a);
    if (sub == "markDirty") {
        ide->CursorA(0, 0, false);
        ide->InsertText("-- tipeado sin guardar\n");
    } else if (sub == "open") {
        extern bool IDEOpenScript(const std::string& path);
        if (!IDEOpenScript(arg)) { err = "idelibrary: IDEAbrirScript fallo con '" + arg + "'"; return false; }
    } else if (sub == "file") {
        if (ide->file != arg) { err = "idelibrary: el IDE tiene '" + ide->file + "', se esperaba '" + arg + "'"; return false; }
    } else if (sub == "dirty") {
        if ((arg == "1") != ide->dirty) { err = std::string("idelibrary: el IDE ") + (ide->dirty ? "has" : "no tiene") + " cambios sin guardar"; return false; }
    } else if (sub == "has" || sub == "hasnot") {
        const bool has = ide->GetText().find(arg) != std::string::npos;
        if (has != (sub == "has")) { err = "idelibrary: el texto del IDE " + std::string(has ? "has" : "no tiene") + " '" + arg + "'"; return false; }
    } else { err = "idelibrary: no entiendo '" + sub + "'"; return false; }
    printf("      [idebib] %s: archivo '%s', sucio %d\n", sub.c_str(), ide->file.c_str(), ide->dirty ? 1 : 0);
    return true;
}
// bibmedir [veces]: cuanto cuesta listar la biblioteca (cada tipo, todo junto), armar las filas del outliner y
// listar lo no guardado, en ms por llamada (reloj de pared). Solo informa: la biblioteca y el seguimiento de
// cambios corren en cada cuadro, asi que con un proyecto grande esto es lo que se paga por cuadro
static bool CmdBibMeasure(std::istringstream& ss, std::string& err) {
    int times = 10; ss >> times;
    if (times < 1) times = 1;
    std::vector<int> types; W3dLibraryTypes(types);
    std::vector<W3dResourceItem> its;
    for (size_t t = 0; t < types.size(); t++) {
        const double t0 = W3dNowMs();
        for (int k = 0; k < times; k++) W3dViewRecList(types[t], its);
        printf("      [bibmedir] %-12s %4d items  %7.2f ms\n", W3dViewKey(types[t]), (int)its.size(), (W3dNowMs() - t0) / times);
    }
    double t0 = W3dNowMs();
    for (int k = 0; k < times; k++) W3dLibraryList(-1, its);
    printf("      [bibmedir] biblioteca   %4d items  %7.2f ms\n", (int)its.size(), (W3dNowMs() - t0) / times);
    Outliner* o = ElOutliner(err, "librarymeasure");
    if (o && o->InLibrary()) {
        std::vector<OutRowRec> rows;
        t0 = W3dNowMs();
        for (int k = 0; k < times; k++) o->RowsResources(rows);
        printf("      [bibmedir] filas        %4d filas  %7.2f ms\n", (int)rows.size(), (W3dNowMs() - t0) / times);
    }
    err.clear();
    std::vector<W3dChange> cs;
    t0 = W3dNowMs();
    for (int k = 0; k < times; k++) W3dChangesList(cs);
    printf("      [bibmedir] cambios      %4d        %7.2f ms\n", (int)cs.size(), (W3dNowMs() - t0) / times);
    return true;
}
// minidistintas <tipo> <idA> <idB>: las miniaturas de dos recursos NO son iguales pixel a pixel (dos materiales
// blancos con texturas distintas no pueden dar la misma esfera)
static bool CmdMiniDistinct(std::istringstream& ss, std::string& err) {
    std::string c, a, b; ss >> c >> a >> b;
    const int t = ViewOf(c, err, "minidistinct"); if (t < 0) return false;
    std::vector<unsigned char> pa, pb; int wa = 0, ha = 0, wb = 0, hb = 0;
    if (!W3dThumbnailPixels(t, IdOfView(t, a), pa, wa, ha) || !W3dThumbnailPixels(t, IdOfView(t, b), pb, wb, hb)) {
        err = "minidistinct: '" + a + "' o '" + b + "' no tiene miniatura"; return false;
    }
    int dif = 0;
    for (size_t k = 0; k < pa.size() && k < pb.size(); k++) if (pa[k] != pb[k]) dif++;
    printf("      [minidistintas] '%s' (%dx%d) vs '%s' (%dx%d): %d bytes distintos\n", a.c_str(), wa, ha, b.c_str(), wb, hb, dif);
    if (pa == pb) { err = "minidistinct: las miniaturas de '" + a + "' y '" + b + "' son identicas"; return false; }
    return true;
}
// recexterno <carpeta|ruta>: "External (choose where)..." de la tarjeta del recurso ACTIVO, con esa eleccion del
// explorador (relativa a tools/pruebas). Por la misma funcion que llama el explorador al aceptar.
static bool CmdRecExternal(std::istringstream& ss, std::string& err) {
    std::string path; ss >> path;
    if (path.empty()) { err = "recexternal: falta la carpeta"; return false; }
    if (path[0] != '/') { char b[4096]; if (getcwd(b, sizeof(b))) path = std::string(b) + "/" + path; }
    int v = -1; std::string id;
    if (!W3dResourceActive(&v, &id)) { err = "recexternal: no hay un recurso activo"; return false; }
    extern void PropsExternalRecPrepare();
    extern void PropsExternalPickedRec(const std::string& picked);
    PropsExternalRecPrepare();
    PropsExternalPickedRec(path);
    std::string now; int va = -1;
    W3dResourceActive(&va, &now);
    printf("      [recexterno] '%s' -> %s: ahora '%s' (%s)\n", id.c_str(), path.c_str(), now.c_str(),
           W3dViewRecLocation(v, now) == 1 ? "outside" : "inside");
    return true;
}
// recguardarcomo <ruta>: "Save As..." de la tarjeta del recurso ACTIVO con ese archivo (una COPIA: el recurso no cambia
// de lugar). Asserta que el archivo quedo escrito y que el recurso sigue donde estaba
static bool CmdRecSaveAs(std::istringstream& ss, std::string& err) {
    std::string path; ss >> path;
    if (path.empty()) { err = "recsaveas: falta la ruta"; return false; }
    if (path[0] != '/') { char b[4096]; if (getcwd(b, sizeof(b))) path = std::string(b) + "/" + path; }
    int v = -1; std::string id;
    if (!W3dResourceActive(&v, &id)) { err = "recsaveas: no hay un recurso activo"; return false; }
    const int prev = W3dViewRecLocation(v, id);
    extern bool ResourceSaveCopy(int, const std::string&, const std::string&, std::string*);
    std::string reason;
    if (!ResourceSaveCopy(v, id, path, &reason)) { err = "recguardarcomo: " + reason; return false; }
    std::vector<unsigned char> b;
    const bool has = w3dFileSystem::ReadFileBytes(path, b) && !b.empty();
    std::string now; int va = -1;
    W3dResourceActive(&va, &now);
    printf("      [recguardarcomo] '%s' -> %s (%u bytes); el recurso sigue '%s' (%s)\n", id.c_str(), path.c_str(), (unsigned)b.size(),
           now.c_str(), W3dViewRecLocation(v, now) == 1 ? "outside" : "inside");
    if (!has) { err = "recsaveas: no quedo el archivo"; return false; }
    if (now != id || W3dViewRecLocation(v, now) != prev) { err = "recsaveas: el recurso cambio de lugar"; return false; }
    return true;
}
// archivomover <de> <a>: mueve (renombra) un archivo del DISCO, por fuera de Whisk3D (como el usuario que
// mueve o borra un archivo externo del proyecto con el explorador). Rutas relativas a tools/pruebas
static bool CmdFileMove(std::istringstream& ss, std::string& err) {
    std::string of, a; ss >> of >> a;
    if (of.empty() || a.empty()) { err = "filemove: uso: filemove <de> <a>"; return false; }
    if (of == a) { err = "filemove: origen y destino son el mismo"; return false; }
    remove(a.c_str());   // (Windows no pisa al renombrar)
    const bool ok = rename(of.c_str(), a.c_str()) == 0;
    printf("      [archivomover] '%s' -> '%s': %s\n", of.c_str(), a.c_str(), ok ? "moved" : "NO");
    if (!ok) { err = "filemove: no pude mover '" + of + "'"; return false; }
    return true;
}
// outlinea <nombre> <nivel> [si|no]: la LINEA GUIA de la jerarquia a la izquierda de la fila (la columna de la
// carpeta de ese nivel), dibujada de verdad: el pixel de la linea contra el del fondo de al lado
static bool CmdOutLine(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outline"); if (!o) return false;
    std::string n, esp; int level = 0; ss >> n >> level >> esp;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0) { err = "outline: no hay una fila '" + n + "'"; return false; }
    const int cy = YOfRow(o, i);
    rootViewport->Render();
    w3dEngine::Finish();
    // la columna de ese nivel (el ancho de un icono): la linea es la franja mas clara que el fondo
    const int x0 = o->x + marginGS + o->PosX + level * (IconSizeGS + gapGS);
    const int width = IconSizeGS > 0 ? IconSizeGS : 1;
    std::vector<unsigned char> px((size_t)width * 4);
    w3dEngine::ReadPixelsRGBA(x0, W3dScreenHeight - cy - 1, width, 1, &px[0]);
    int imax = 0, imin = 0;
    for (int k = 1; k < width; k++) {
        const int v = px[(size_t)k * 4] + px[(size_t)k * 4 + 1] + px[(size_t)k * 4 + 2];
        if (v > px[(size_t)imax * 4] + px[(size_t)imax * 4 + 1] + px[(size_t)imax * 4 + 2]) imax = k;
        if (v < px[(size_t)imin * 4] + px[(size_t)imin * 4 + 1] + px[(size_t)imin * 4 + 2]) imin = k;
    }
    const unsigned char* a = &px[(size_t)imax * 4];
    const unsigned char* b = &px[(size_t)imin * 4];
    const int d = (int)a[0] + a[1] + a[2] - ((int)b[0] + b[1] + b[2]);
    const bool has = d > 45;
    printf("      [outlinea] '%s' nivel %d: linea rgb %d %d %d, fondo %d %d %d -> %s\n", n.c_str(), level, a[0], a[1], a[2],
           b[0], b[1], b[2], has ? "LINEA" : "sin linea");
    if ((esp != "no") != has) { err = "outline: la linea guia de '" + n + "' " + (has ? "is" : "no esta"); return false; }
    return true;
}
// outcolor <nombre> activo|seleccion|normal: el color del NOMBRE dibujado (la fila activa en verde, la
// seleccionada en verde oscuro): se cuentan los pixeles del texto de ese color
static bool CmdOutColor(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outcolor"); if (!o) return false;
    std::string n, esp; ss >> n >> esp;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0) { err = "outcolor: no hay una fila '" + n + "'"; return false; }
    const int cy = YOfRow(o, i);
    rootViewport->Render();
    w3dEngine::Finish();
    const int x0 = XName(o, rows[(size_t)i]) - LetterWidthGS, w = (int)rows[(size_t)i].name.size() * LetterWidthGS;
    const int h = (int)RowHeightGS - 2 * GlobalScale;
    if (w <= 0) { err = "outcolor: sin texto"; return false; }
    std::vector<unsigned char> px((size_t)w * h * 4);
    w3dEngine::ReadPixelsRGBA(x0, W3dScreenHeight - (cy + h / 2), w, h, &px[0]);
    const GLubyte* acc = ColorListUbyte[static_cast<int>(ColorID::accent)];
    const GLubyte* osc = ColorListUbyte[static_cast<int>(ColorID::accentDark)];
    int nAcc = 0, nOsc = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4) {
        if (ColorNear(&px[p], acc)) nAcc++;
        else if (ColorNear(&px[p], osc)) nOsc++;
    }
    printf("      [outcolor] '%s': %d px verde (activo), %d px verde oscuro (seleccion)\n", n.c_str(), nAcc, nOsc);
    const bool ok = (esp == "active") ? (nAcc > 3) : (esp == "selection") ? (nOsc > 3 && nAcc <= 3) : (nAcc <= 3 && nOsc <= 3);
    if (!ok) { err = "outcolor: el nombre de '" + n + "' no se ve " + esp; return false; }
    return true;
}
// outsoltar <nombre> 3d [dx dy] | props malla|material|textura: ARRASTRA el recurso desde la biblioteca y lo suelta
// AFUERA: en el centro del viewport 3D (corrido dx,dy px) o sobre el desplegable de Properties de ese tipo (los
// movimientos y el soltar de siempre: event_mouse_motion + LayoutRelease)
static ViewportBase* TypeFirst(ViewportBase* n, int kind) { return FindLeaf(n, kind); }
static bool CmdOutRelease(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outrelease"); if (!o) return false;
    std::string n, where, that; ss >> n >> where;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0 || rows[(size_t)i].folder) { err = "outrelease: no hay un recurso '" + n + "'"; return false; }
    rootViewport->Render();   // (el 3D sabe su lente; Properties, donde estan sus botones)
    int x1 = 0, y1 = 0;
    if (where == "3d") {
        int dx = 0, dy = 0; ss >> dx >> dy;
        ViewportBase* v = TypeFirst(rootViewport, 1);
        if (!v) { err = "outrelease: no hay viewport 3D"; return false; }
        x1 = v->x + v->width / 2 + dx; y1 = v->y + v->height / 2 + dy;
    } else if (where == "props") {
        ss >> that;
        Properties* p = (Properties*)TypeFirst(rootViewport, 3);
        if (!p) { err = "outrelease: no hay panel de propiedades"; return false; }
        // (el panel al dia con la seleccion, como despues de un click de verdad: selobj no lo refresca)
        p->RefreshTargetProperties();
        p->UpdateTabs();
        rootViewport->Render();
        const int t = (that == "mesh") ? W3D_VIEW_MESHES : (that == "material") ? W3D_VIEW_MATERIALS
                    : (that == "prefab") ? W3D_VIEW_PREFABS : (that == "scene") ? W3D_VIEW_SCENES : W3D_VIEW_TEXTURES;
        std::vector<int> r;
        PropsDestinationsResource(p, t, r);
        // (si el desplegable quedo abajo del panel, se scrollea como lo haria el usuario)
        // ('normal' = el SEGUNDO destino de una textura: el Normal Map del material; el primero es el Base Color)
        const size_t k0 = (that == "normal") ? 4 : 0;
        for (int k = 1; k <= 12 && (r.size() < k0 + 4 || r[k0 + 1] + r[k0 + 3] > p->y + p->height - 2); k++) {
            p->PosY = -k * 3 * (int)RowHeightGS;
            rootViewport->Render();
            PropsDestinationsResource(p, t, r);
        }
        if (r.size() < k0 + 4) {
            Button* sb = p->propBtnMeshSel ? p->propBtnMeshSel->button : NULL;
            printf("      [outsoltar] panel (%d,%d %dx%d) selector %s (%d,%d %dx%d) visible %d\n", p->x, p->y, p->width, p->height,
                   sb ? "si" : "no", sb ? sb->sx : 0, sb ? sb->sy : 0, sb ? sb->width : 0, sb ? sb->height : 0,
                   sb ? (int)sb->visible : -1);
            err = "outrelease: Properties no muestra el desplegable de '" + that + "'"; return false;
        }
        x1 = r[k0] + r[k0 + 2] / 2; y1 = r[k0 + 1] + r[k0 + 3] / 2;
        printf("      [outsoltar] el desplegable de '%s' en (%d,%d %dx%d), panel scrolleado %d\n", that.c_str(), r[0], r[1], r[2], r[3], p->PosY);
    } else { err = "outrelease: 3d | props <malla|material|textura|prefab>"; return false; }
    const int x0 = XName(o, rows[(size_t)i]), y0 = YOfRow(o, i);
    leftMouseDown = true;
    LayoutClickUI(x0, y0);
    o->event_mouse_motion(x0 + (int)RowHeightGS * 2, y0 + (int)RowHeightGS);
    o->event_mouse_motion(x1, y1);
    const bool dragged = g_outDrag.active;
    const int destination = g_outDrag.destination;
    const std::string qt = g_outDrag.destinationText;
    rootViewport->Render();
    LayoutRenderMenu(MenuScreenW, MenuScreenH);   // (el resaltado de los destinos se dibuja sin romper nada)
    leftMouseDown = false;
    LayoutRelease(x1, y1);
    printf("      [outsoltar] '%s' -> %s %s: arrastre %s, destino %d ('%s')\n", n.c_str(), where.c_str(), that.c_str(),
           dragged ? "si" : "no", destination, qt.c_str());
    if (!dragged) { err = "outrelease: el arrastre afuera del outliner no arranco"; return false; }
    std::string k;
    if ((ss >> k) && k == "invalidated") { if (destination > 0) { err = "outrelease: el destino era valido"; return false; } }
    else if (destination <= 0) { err = "outrelease: el destino no era valido: " + qt; return false; }
    return true;
}
// outprevia <recurso> <objeto> [dx dy]: la VISTA PREVIA de un material / shader arrastrado (DropResource.cpp). Arrastra
// el recurso hasta el centro del viewport 3D (corrido dx,dy) SIN soltar: la parte 0 del objeto tiene que estar dibujandose
// con otro material; despues vuelve al outliner y suelta ahi: la parte tiene que tener su material de antes
static bool CmdOutPreview(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outpreview"); if (!o) return false;
    std::string n, obj; int dx = 0, dy = 0; ss >> n >> obj >> dx >> dy;
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = RowOf(rows, n);
    if (i < 0 || rows[(size_t)i].folder) { err = "outpreview: no hay un recurso '" + n + "'"; return false; }
    Object* ob = SceneCollection ? FindObjectByName(SceneCollection, obj) : NULL;
    if (!ob || ob->getType() != ObjectType::mesh || ((Mesh*)ob)->materialsGroup.empty()) { err = "outpreview: no hay una malla '" + obj + "'"; return false; }
    Mesh* m = (Mesh*)ob;
    Material* prev = m->materialsGroup[0].material;
    rootViewport->Render();
    ViewportBase* v = TypeFirst(rootViewport, 1);
    if (!v) { err = "outpreview: no hay viewport 3D"; return false; }
    const int x1 = v->x + v->width / 2 + dx, y1 = v->y + v->height / 2 + dy;
    const int x0 = XName(o, rows[(size_t)i]), y0 = YOfRow(o, i);
    leftMouseDown = true;
    LayoutClickUI(x0, y0);
    o->event_mouse_motion(x0 + (int)RowHeightGS * 2, y0 + (int)RowHeightGS);
    o->event_mouse_motion(x1, y1);
    rootViewport->Render();
    Material* during = m->materialsGroup[0].material;
    printf("      [outprevia] sobre '%s': '%s' (antes '%s') shader '%s'\n", obj.c_str(), during ? during->name.c_str() : "-",
           prev ? prev->name.c_str() : "-", during ? during->shaderFile.c_str() : "");
    if (during == prev) { err = "outpreview: la parte no muestra la vista previa"; leftMouseDown = false; LayoutRelease(x0, y0); return false; }
    o->event_mouse_motion(x0, y0);   // de vuelta al outliner: la vista previa se va
    Material* outside = m->materialsGroup[0].material;
    leftMouseDown = false;
    LayoutRelease(x0, y0);
    Material* final = m->materialsGroup[0].material;
    printf("      [outprevia] afuera: '%s', soltado en el outliner: '%s'\n", outside ? outside->name.c_str() : "-", final ? final->name.c_str() : "-");
    if (outside != prev || final != prev) { err = "outpreview: la parte no volvio a su material"; return false; }
    return true;
}
// libvincular <ruta.w3d> [falla] / libdesvincular <nombre> / libs [n N]: las LIBRERIAS externas (el menu de la vista)
static bool CmdLibLink(std::istringstream& ss, std::string& err) {
    std::string r, f; ss >> r >> f;
    std::string reason;
    std::string abs = r;
    if (!abs.empty() && abs[0] != '/') { char b[4096]; if (getcwd(b, sizeof(b))) abs = std::string(b) + "/" + r; }
    const bool ok = W3dExtLibLink(abs, &reason);
    printf("      [libvincular] '%s': %s %s\n", abs.c_str(), ok ? "linked" : "NO", reason.c_str());
    if (ok == (f == "fails")) { err = "libvincular: " + std::string(ok ? "se vinculo y no debia" : "no se vinculo: " + reason); return false; }
    return true;
}
static bool CmdLibUnlink(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    for (int i = 0; i < W3dLibsCount(); i++)
        if (W3dLibsRow(i).name == n) { W3dExtLibUnlink(i); printf("      [libdesvincular] '%s'\n", n.c_str()); return true; }
    err = "libunlink: no esta vinculada '" + n + "'";
    return false;
}
static bool CmdLibs(std::istringstream& ss, std::string& err) {
    printf("      [libs] %d:", W3dLibsCount());
    for (int i = 0; i < W3dLibsCount(); i++) printf(" '%s'", W3dLibsRow(i).name.c_str());
    printf("\n");
    std::string k; int n = -1;
    if ((ss >> k) && k == "n" && (ss >> n) && n != W3dLibsCount()) { err = "libs: hay " + Integer(W3dLibsCount()); return false; }
    return true;
}
// cambios [n N] [tiene <texto>] [no <texto>]: la lista de lo NO GUARDADO (la del cartel)
static bool CmdChanges(std::istringstream& ss, std::string& err) {
    std::vector<W3dChange> v; W3dChangesList(v);
    printf("      [cambios] %d:", (int)v.size());
    for (size_t i = 0; i < v.size(); i++) printf(" [%s: %s]", v[i].type.c_str(), v[i].name.c_str());
    printf("\n");
    std::string k;
    while (ss >> k) {
        std::string val; ss >> val;
        bool has = false;
        for (size_t i = 0; i < v.size(); i++) if (v[i].name == val || v[i].name.find(val + " (") == 0) has = true;
        if (k == "n") { if ((int)v.size() != atoi(val.c_str())) { err = "changes: hay " + Integer((long)v.size()) + ", se esperaban " + val; return false; } }
        else if (k == "has") { if (!has) { err = "changes: no esta '" + val + "'"; return false; } }
        else if (k == "no") { if (has) { err = "changes: esta '" + val + "'"; return false; } }
        else { err = "changes: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
// cambiosmarca 0|1: el '*' del NOMBRE DEL PROYECTO en la tarjeta Archivo de Properties (la pestania 0)
static bool CmdChangesMark(std::istringstream& ss, std::string& err) {
    std::string v; ss >> v;
    Properties* p = ElPanel();
    if (!p || !p->propProjName) { err = "changesmark: sin panel / sin la tarjeta Archivo"; return false; }
    const int prev = p->activeTab;
    p->activeTab = 0;
    p->UpdateTabs();
    rootViewport->Render();
    const std::string label = p->propProjName->name;
    p->activeTab = prev;
    p->UpdateTabs();
    const bool mark = !label.empty() && label[label.size() - 1] == '*';
    printf("      [cambiosmarca] '%s'\n", label.c_str());
    if (mark != (v == "1")) { err = std::string("changesmark: el nombre del proyecto ") + (mark ? "has" : "no tiene") + " el '*'"; return false; }
    return true;
}
// cambiosucio <tipo> <id> 0|1 / cambioraiz <nombre> 0|1: el '*' de un recurso / de una raiz
static bool CmdChangeDirty(std::istringstream& ss, std::string& err) {
    std::string c, id, v; ss >> c >> id >> v;
    const int t = ViewOf(c, err, "changedirty"); if (t < 0) return false;
    const bool s = W3dChangesResourceDirty(t, id);
    printf("      [cambiosucio] %s '%s': %s\n", c.c_str(), id.c_str(), s ? "SUCIO *" : "saved");
    if (s != (v == "1")) { err = "changedirty: '" + id + "' " + (s ? "esta sucio" : "esta guardado"); return false; }
    return true;
}
static bool CmdChangeRoot(std::istringstream& ss, std::string& err) {
    std::string n, v; ss >> n >> v;
    int i = W3dRootFind(W3D_ROOT_SCENE, n);
    if (i < 0) i = W3dRootFind(W3D_ROOT_PREFAB, n);
    if (i < 0) { err = "changeroot: no hay una raiz '" + n + "'"; return false; }
    const bool s = W3dChangesRootDirty(i);
    printf("      [cambioraiz] '%s': %s\n", n.c_str(), s ? "SUCIA *" : "saved");
    if (s != (v == "1")) { err = "changeroot: '" + n + "' " + (s ? "esta sucia" : "esta guardada"); return false; }
    return true;
}
// cartel salir|abrir|nuevo [abre|noabre] / cartelresponder guardar|seguir|cancelar: el cartel "Se perderan los
// cambios en:" (en el harness normalmente no se muestra: aca se lo prende a proposito) y su respuesta. Los
// ganchos de quit/abrir no hacen nada de verdad en el harness: se anota que se llamaron.
static int gExitedDialog = 0, gDialogOpened = 0;
static std::string gDialogPath;
static void DialogExitTest() { gExitedDialog++; }
static void DialogOpenTest(const std::string& r) { gDialogOpened++; gDialogPath = r; }
static bool CmdDialog(std::istringstream& ss, std::string& err) {
    std::string a, esp; ss >> a >> esp;
    const int acc = (a == "open") ? W3D_CHANGES_OPEN : (a == "new") ? W3D_CHANGES_NEW : W3D_CHANGES_EXIT;
    W3dChangesExitHook = DialogExitTest;
    W3dChangesOpenHook = DialogOpenTest;
    gExitedDialog = gDialogOpened = 0;
    // (el harness no muestra el cartel solo: aca se prende a proposito para probarlo)
    const bool noPrev = g_w3dChangesNoDialog, activePrev = g_w3dChangesDialogActive;
    g_w3dChangesNoDialog = false; g_w3dChangesDialogActive = true;
    const bool opened = W3dChangesAsk(acc, "otro.w3d");
    g_w3dChangesNoDialog = noPrev; g_w3dChangesDialogActive = activePrev;
    const bool popup = PopUpActive != NULL;
    printf("      [cartel] %s: %s%s\n", a.c_str(), opened ? "ABIERTO" : "no hacia falta", popup ? " (popup a la vista)" : "");
    if (opened) { rootViewport->Render(); LayoutRenderMenu(MenuScreenW, MenuScreenH); }   // (se dibuja: la lista)
    if ((esp == "opens") != opened) { err = std::string("dialog: ") + (opened ? "se abrio" : "no se abrio"); return false; }
    return true;
}
// cartelprender si|no: el cartel de lo no guardado como en el producto (si) o como en el harness (no: nunca aparece).
// Para probar los botones de verdad ("Nuevo proyecto" de la tarjeta Archivo) con el cartel en el medio
static bool CmdDialogTurnOn(std::istringstream& ss, std::string& err) {
    std::string sn; ss >> sn;
    if (sn != "si" && sn != "no") { err = "dialogshow si|no"; return false; }
    g_w3dChangesNoDialog = (sn == "no");
    g_w3dChangesDialogActive = true;
    return true;
}
static bool CmdDialogRespond(std::istringstream& ss, std::string& err) {
    std::string r, k, v; ss >> r >> k >> v;
    if (!W3dChangesDialogOpen() || !PopUpActive) { err = "dialogrespond: no hay cartel abierto"; return false; }
    const int key = (r == "cancel") ? (int)LayoutKey::Cancel : (int)LayoutKey::Enter;
    if (r == "continue") PopUpActive->Key(LayoutKey::Right);   // el foco al segundo boton
    PopUpActive->Key(key);
    printf("      [cartelresponder] %s: salir %d, abrir %d ('%s')\n", r.c_str(), gExitedDialog, gDialogOpened, gDialogPath.c_str());
    if (k == "exited" && gExitedDialog != atoi(v.c_str())) { err = "dialogrespond: salir se llamo " + Integer(gExitedDialog); return false; }
    if (k == "opened" && gDialogOpened != atoi(v.c_str())) { err = "dialogrespond: abrir se llamo " + Integer(gDialogOpened); return false; }
    return true;
}
// mallasrecurso [estricto] [n N]: TODA malla es un recurso: cada objeto malla de las raices vivas tiene el suyo y
// la vista "Mallas 3D" de la biblioteca lista EXACTAMENTE los recursos vivos (usados + huerfanos, sin los borrados)
static void CollectMeshesRoot(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        if (!o->Childrens[i]) continue;
        if (o->Childrens[i]->getType() == ObjectType::mesh) out.push_back((Mesh*)o->Childrens[i]);
        CollectMeshesRoot(o->Childrens[i], out);
    }
}
// outmover si|no [destino <carpeta|->]: el MODO MOVER de la biblioteca (la G) esta activo? y hacia donde va
static bool CmdOutMove(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outmove"); if (!o) return false;
    std::string sn; ss >> sn;
    printf("      [outmover] %s, destino '%s'\n", o->movingRec ? "active" : "no", o->moveRecDestination.c_str());
    if ((sn == "si") != o->movingRec) { err = std::string("outmove: el modo mover ") + (o->movingRec ? "is" : "no esta") + " activo"; return false; }
    std::string k, v;
    while (ss >> k >> v) {
        if (k == "destination") { if (Dash(v) != o->moveRecDestination) { err = "outmove: el destino es '" + o->moveRecDestination + "', se esperaba '" + v + "'"; return false; } }
        else { err = "outmove: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
// mallasrecurso [estricto] [n <N>]: toda malla de todas las raices tiene su recurso y la biblioteca lista
// exactamente los recursos vivos. 'estricto' = las mallas SIN recurso se cuentan ANTES de listar (listar la
// biblioteca le crea el suyo a la que no lo tiene: sin esto la prueba arreglaba antes de verificar); solo corre
// lo que corre un cuadro del editor (su tick), asi que valida el camino real de la UI
static bool CmdMeshesResource(std::istringstream& ss, std::string& err) {
    std::string k; int n = -1;
    bool strict = false;
    while (ss >> k) {
        if (k == "strict") strict = true;
        else if (k == "n") { if (!(ss >> n)) { err = "meshesresource: falta el numero de 'n'"; return false; } }
        else { err = "meshesresource: no entiendo '" + k + "'"; return false; }
    }
    std::vector<Object*> roots; W3dLiveRoots(roots);
    std::vector<Mesh*> ms;
    for (size_t i = 0; i < roots.size(); i++) CollectMeshesRoot(roots[i], ms);
    int sin = 0;
    if (strict) {
        extern void W3dMeshesTickEditor();
        W3dMeshesTickEditor();   // (un cuadro del editor)
        for (size_t i = 0; i < ms.size(); i++) if (!ms[i]->mesh) { sin++; printf("      [mallasrecurso] SIN RECURSO (antes de listar): '%s'\n", ms[i]->name.c_str()); }
    }
    std::vector<W3dResourceItem> its; W3dViewRecList(W3D_VIEW_MESHES, its);   // (lista y asegura)
    if (!strict)
        for (size_t i = 0; i < ms.size(); i++) if (!ms[i]->mesh) { sin++; printf("      [mallasrecurso] SIN RECURSO: '%s'\n", ms[i]->name.c_str()); }
    std::set<std::string> live, listings;
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    for (size_t i = 0; i < reg.size(); i++) if (!reg[i]->deleted) live.insert(reg[i]->name);
    for (size_t i = 0; i < its.size(); i++) listings.insert(its[i].id);
    // (cada objeto: su recurso esta en la lista)
    for (size_t i = 0; i < ms.size(); i++) if (ms[i]->mesh && !listings.count(ms[i]->mesh->name)) { err = "meshesresource: el recurso de '" + ms[i]->name + "' no se lista"; return false; }
    printf("      [mallasrecurso] %d mallas, %d sin recurso, %d recursos vivos, %d listados\n", (int)ms.size(), sin, (int)live.size(), (int)listings.size());
    if (sin > 0) { err = "meshesresource: hay mallas sin recurso"; return false; }
    if (live != listings) { err = "meshesresource: la biblioteca no lista exactamente los recursos vivos"; return false; }
    if (n >= 0 && n != (int)listings.size()) { err = "meshesresource: hay " + Integer((long)listings.size()) + " recursos, se esperaban " + Integer(n); return false; }
    return true;
}
// recubicar <tipo> <id> dentro|fuera [ruta]: pasa el archivo adentro / afuera del .w3d (la del panel: con undo)
static bool CmdRecLocate(std::istringstream& ss, std::string& err) {
    std::string c, id, d, path; ss >> c >> id >> d >> path;
    const int t = ViewOf(c, err, "reclocate"); if (t < 0) return false;
    id = IdOfView(t, id);
    std::string newValue, reason;
    if (!path.empty() && path[0] != '/') { char b[4096]; if (getcwd(b, sizeof(b))) path = std::string(b) + "/" + path; }
    const bool ok = W3dViewRecSetLocation(t, id, d == "outside" ? 1 : 0, path, &newValue, &reason);
    printf("      [recubicar] %s '%s' -> %s: %s '%s' %s\n", c.c_str(), id.c_str(), d.c_str(), ok ? "OK" : "NO", newValue.c_str(), reason.c_str());
    if (!ok) { err = "recubicar: " + reason; return false; }
    return true;
}
// recubicacion <tipo> <id> dentro|fuera: donde vive hoy
static bool CmdRecLocation(std::istringstream& ss, std::string& err) {
    std::string c, id, d; ss >> c >> id >> d;
    const int t = ViewOf(c, err, "reclocation"); if (t < 0) return false;
    id = IdOfView(t, id);
    const int u = W3dViewRecLocation(t, id);
    printf("      [recubicacion] %s '%s': %s\n", c.c_str(), id.c_str(), u == 0 ? "inside" : u == 1 ? "outside" : "(sin archivo)");
    if ((d == "outside") != (u == 1) || u < 0) { err = "reclocation: '" + id + "' no esta " + d; return false; }
    return true;
}
// recduplicar <tipo> <id> [esperado]
static bool CmdRecDuplicate(std::istringstream& ss, std::string& err) {
    std::string c, id, esp; ss >> c >> id >> esp;
    const int t = ViewOf(c, err, "recduplicate"); if (t < 0) return false;
    std::string newValue, reason;
    if (!W3dViewRecDuplicate(t, id, &newValue, &reason)) { err = "recduplicar: " + reason; return false; }
    printf("      [recduplicar] %s '%s' -> '%s'\n", c.c_str(), id.c_str(), newValue.c_str());
    if (!esp.empty() && newValue != esp) { err = "recduplicate: quedo '" + newValue + "', se esperaba '" + esp + "'"; return false; }
    return true;
}
// recborrarforzado <tipo> <id> [falla]: borrar un recurso EN USO (sus usuarios quedan sin el, con Ctrl+Z)
static bool CmdRecDeleteForced(std::istringstream& ss, std::string& err) {
    std::string c, id, f; ss >> c >> id >> f;
    const int t = ViewOf(c, err, "recdeleteforced"); if (t < 0) return false;
    std::string reason;
    const bool ok = W3dViewRecDeleteForced(t, id, &reason);
    printf("      [recborrarforzado] %s '%s': %s %s\n", c.c_str(), id.c_str(), ok ? "deleted" : "NO", reason.c_str());
    if (ok == (f == "fails")) { err = "recborrarforzado: " + std::string(ok ? "se borro y no debia" : "no se borro: " + reason); return false; }
    return true;
}
// recdato <tipo> <id> <etiqueta en ingles...> = <valor en ingles...>: un DATO del tipo (los de la tarjeta del
// recurso), exacto ("recdato texturas textures/a.png Resolution = 64 x 64"). Sin '=' solo los vuelca.
static bool CmdRecDataItem(std::istringstream& ss, std::string& err) {
    std::string c, id, rest; ss >> c >> id; std::getline(ss, rest);
    const int t = ViewOf(c, err, "recfield"); if (t < 0) return false;
    id = IdOfView(t, id);
    // (en INGLES: la prueba no depende del idioma de la UI)
    const W3dLanguage prev = g_language;
    W3dLanguageSet(W3dLanguageOf("en"));
    std::vector<W3dResourceData> ds; W3dViewRecData(t, id, ds);
    W3dLanguageSet(prev);
    for (size_t i = 0; i < ds.size(); i++) printf("      [recdato] %s: %s\n", ds[i].label.c_str(), ds[i].value.c_str());
    const size_t eq = rest.find('=');
    if (eq == std::string::npos) return true;
    std::string et = rest.substr(0, eq), val = rest.substr(eq + 1);
    while (!et.empty() && et[0] == ' ') et.erase(0, 1);
    while (!et.empty() && et[et.size() - 1] == ' ') et.erase(et.size() - 1);
    while (!val.empty() && val[0] == ' ') val.erase(0, 1);
    while (!val.empty() && val[val.size() - 1] == ' ') val.erase(val.size() - 1);
    for (size_t i = 0; i < ds.size(); i++)
        if (ds[i].label == et) {
            if (ds[i].value != val) { err = "recdato: " + et + " = '" + ds[i].value + "', se esperaba '" + val + "'"; return false; }
            return true;
        }
    err = "recfield: '" + id + "' no tiene el dato '" + et + "'";
    return false;
}
// propsmalla [tab N] [malla3d 0|1] [recurso 0|1] [selector <texto>] [pestanias N]: la pestania "Malla 3D" (la 3)
// del panel: con un objeto malla muestra su tarjeta "3D Mesh" (la primera); con un recurso de la biblioteca, el
// recurso. No hay pestania "Recurso" aparte (BarTabs tiene 10)
static bool CmdPropsMesh(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "propsmesh: sin panel de propiedades"; return false; }
    rootViewport->Render();
    p->RefreshTargetProperties();
    p->UpdateTabs();
    const bool m3 = p->propMesh3D && p->propMesh3D->visible;
    const bool rec = p->propResource && p->propResource->visible;
    // (la tarjeta 3D Mesh es la PRIMERA visible de la pestania)
    int first = -1;
    for (size_t i = 0; i < p->GroupProperties.size() && first < 0; i++) if (p->GroupProperties[i]->visible) first = (int)i;
    const std::string sel = p->propBtnMeshSel ? p->propBtnMeshSel->button->text : std::string();
    printf("      [propsmalla] pestania %d (icono %s), %d pestanias, 3D Mesh %s%s, recurso %s, selector '%s'\n", p->activeTab,
           p->BarTabs.size() > 3 ? IconName(p->BarTabs[3]->icon) : "?", (int)p->BarTabs.size(), m3 ? "SI" : "no",
           (m3 && first >= 0 && p->GroupProperties[(size_t)first] == p->propMesh3D) ? " (la primera)" : "", rec ? "SI" : "no", sel.c_str());
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        bool ok = true;
        if (k == "tab") ok = (p->activeTab == atoi(v.c_str()));
        else if (k == "mesh3d") ok = (m3 == (v == "1")) && (!m3 || p->GroupProperties[(size_t)first] == p->propMesh3D);
        // (la tarjeta del recurso visible Y el panel dibujandola: elegir un recurso suelta la escena, asi que sin
        // objeto activo la pestania tiene que contar como global, sino el panel queda vacio)
        else if (k == "resource") ok = (rec == (v == "1")) && (!rec || ActiveObj || p->TabGlobal());
        else if (k == "selector") ok = (sel == v);
        else if (k == "tabs") ok = ((int)p->BarTabs.size() == atoi(v.c_str()));
        else { err = "propsmesh: no entiendo '" + k + "'"; return false; }
        if (!ok) { err = "propsmesh: no cumple " + k + " " + v; return false; }
    }
    return true;
}
// propstab <N>: elige la pestania N del panel (su click)
static bool CmdPropsTab(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "propstab: sin panel"; return false; }
    int n = -1; ss >> n;
    rootViewport->Render();
    if (n < 0 || n >= (int)p->BarTabs.size() || !p->BarTabs[(size_t)n]->visible) { err = "propstab: la pestania no esta"; return false; }
    PropsActive = p;
    p->activeTab = n;
    p->UpdateTabs();
    p->Resize(p->width, p->height);
    rootViewport->Render();
    return true;
}

// ============================================================================
//  propsalfa: la fila "Alpha Test" de la tarjeta Material (Material::alphaTest, clave "alfaCorte"; 0 = apagado)
// ============================================================================
// Todo por la puerta del MOUSE de verdad (LayoutClickUI -> ClickIn, event_mouse_motion con el dx global,
// mouse_button_up + LayoutRelease) y el Render del panel, que es el que commitea el paso de Ctrl+Z:
//   propsalfa fila [<valor>|-|arrastre]  la fila esta en la tarjeta Material (pestania 2), se llama T("Alpha Test")
//                                        y muestra el alfaCorte del material ('-' = OCULTA: material por defecto;
//                                        'arrastre' = el valor que dejo el ultimo 'arrastrar', para el redo)
//   propsalfa arrastrar <px> [<min> <max>]   click en la fila, arrastre horizontal de <px> y soltar: el valor sube
//                                        (o baja) y queda en [min, max] si se dan; el Ctrl+Z queda en UN paso
//   propsalfa texto <numero>             click SIN arrastrar (abre la edicion por texto), tipear y Enter por la
//                                        puerta del teclado de PC (SDL_TEXTINPUT + SDLK_RETURN)
#ifndef W3D_SYMBIAN
static float g_alphaDrag = -1.0f;   // el valor que dejo el ultimo 'propsalpha arrastrar'
// el centro de la columna de VALORES de una fila del panel, con el MISMO recorrido de filas que ClickIn; si la fila
// queda fuera de la vista, corre el scroll vertical (PosY) y vuelve a dibujar. false = la fila no se dibuja
static bool LocateRowPanel(Properties* p, PropertieBase* row, int& mx, int& my) {
    for (int attempt = 0; attempt < 3; attempt++) {
        rootViewport->Render();   // las alturas de las tarjetas (g->height) y el tope del scroll
        int yCursor = p->y + p->BarTopOffset() + p->PosY + borderGS + RowHeightGS + gapGS;
        bool found = false;
        for (size_t i = 0; i < p->GroupProperties.size() && !found; i++) {
            GroupPropertie* g = p->GroupProperties[i];
            if (!g->visible) continue;
            if (g->open) {
                int yRow = yCursor + borderGS + RowHeightGS + gapGS;   // la cabecera
                for (size_t j = 0; j < g->properties.size(); j++) {
                    const int hRow = g->properties[j]->Resize(g->width);
                    if (g->properties[j] == row) {
                        if (hRow <= 0) return false;
                        mx = p->x + p->PosX + borderGS * 2 + g->colLabel + (g->width - g->colLabel) / 3;
                        my = yRow + hRow / 2;
                        found = true;
                        break;
                    }
                    yRow += hRow;
                }
            }
            yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
        }
        if (!found) return false;
        const int up = p->y + p->BarTopOffset() + RowHeightGS, down = p->y + p->height - RowHeightGS;
        if (my >= up && my < down) return true;
        p->PosY -= my - (up + down) / 2;   // la fila al medio del panel
    }
    return false;
}
static bool CmdPropsAlpha(std::istringstream& ss, std::string& err) {
    extern int dx;
    extern void InputUserSDL3(SDL_Event&);
    std::string sub; ss >> sub;
    Properties* p = ElPanel();
    if (!p || !rootViewport) { err = "propsalpha: sin panel de propiedades"; return false; }
    // la pestania 2 (la del material del objeto activo), como el click en su pestania
    rootViewport->Render();
    if (p->BarTabs.size() <= 2 || !p->BarTabs[2]->visible) { err = "propsalpha: el panel no muestra la pestania del material"; return false; }
    PropsActive = p;
    p->activeTab = 2;
    p->UpdateTabs();
    p->Resize(p->width, p->height);
    rootViewport->Render();
    PropFloat* f = p->propMatAlpha;
    bool inCard = false;
    if (f && p->propMaterial)
        for (size_t j = 0; j < p->propMaterial->properties.size(); j++) if (p->propMaterial->properties[j] == f) inCard = true;
    if (!f || !inCard) { err = "propsalpha: la tarjeta Material no tiene la fila Alpha Test"; return false; }
    if (!p->propMaterial->visible) { err = "propsalpha: la tarjeta Material no se ve (el objeto activo es una malla?)"; return false; }
    if (f->name != T("Alpha Test")) { err = "propsalpha: la fila se llama '" + f->name + "'"; return false; }
    if (sub == "row") {
        std::string esp; ss >> esp;
        printf("      [propsalfa] fila '%s' %s\n", f->name.c_str(), f->value ? "visible" : "OCULTA");
        if (f->value) printf("      [propsalfa] valor=%g\n", *f->value);
        if (esp == "-") { if (f->value) { err = "propsalpha: la fila se ve y tenia que estar oculta"; return false; } return true; }
        if (!f->value) { err = "propsalpha: la fila esta oculta"; return false; }
        if (esp.empty()) return true;
        const float v = (esp == "drag") ? g_alphaDrag : (float)atof(esp.c_str());
        if (fabsf(*f->value - v) > 1e-4f) {
            char b[160]; snprintf(b, sizeof b, "propsalpha: la fila muestra %g y se esperaba %g", *f->value, v); err = b; return false;
        }
        return true;
    }
    if (!f->value) { err = "propsalpha: la fila esta oculta (material por defecto?)"; return false; }
    int mx = 0, my = 0;
    if (!LocateRowPanel(p, f, mx, my)) { err = "propsalpha: no pude ubicar la fila en el panel"; return false; }
    const float prev = *f->value;
    if (sub == "drag") {
        int px = 0; ss >> px;
        float mn = -1.0f, mxv = -1.0f; const bool range = !(ss >> mn >> mxv).fail();
        leftMouseDown = true;
        LayoutClickUI(mx, my);
        // el arrastre: el dx GLOBAL por evento, de a 5 px (como llega del loop de SDL)
        int done = 0;
        while (done != px) {
            int d = px > done ? 5 : -5;
            if (abs(px - done) < 5) d = px - done;
            dx = d; done += d;
            p->event_mouse_motion(mx + done, my);
        }
        dx = 0;
        rootViewport->Render();   // con el boton apretado el panel NO commitea
        leftMouseDown = false;
        p->mouse_button_up(W3dMB_IZQ);
        LayoutRelease(mx + px, my);
        rootViewport->Render();   // al soltar: el paso de Ctrl+Z
        g_alphaDrag = *f->value;
        printf("      [propsalfa] arrastre de %d px: %g -> %g\n", px, prev, *f->value);
        if (px != 0 && (px > 0 ? !(*f->value > prev) : !(*f->value < prev))) { err = "propsalpha: el arrastre no cambio el valor"; return false; }
        if (range && (*f->value < mn - 1e-4f || *f->value > mxv + 1e-4f)) {
            char b[160]; snprintf(b, sizeof b, "propsalpha: el arrastre dejo %g, fuera de [%g, %g]", *f->value, mn, mxv); err = b; return false;
        }
        return true;
    }
    if (sub == "text") {
        std::string num; ss >> num;
        if (num.empty()) { err = "propsalpha: texto <numero>"; return false; }
        leftMouseDown = true;
        LayoutClickUI(mx, my);
        leftMouseDown = false;
        p->mouse_button_up(W3dMB_IZQ);   // click puro: abre la edicion por texto
        LayoutRelease(mx, my);
        rootViewport->Render();          // (editando por texto el panel NO commitea la foto del click)
        if (!NumEditActive() || g_propFloatEditing != f) { err = "propsalpha: el click sin arrastre no abrio la edicion por texto"; return false; }
        if (!viewPortActive) viewPortActive = p;   // (sin viewport con foco la puerta de SDL no atiende el teclado)
        SDL_Event ev; memset(&ev, 0, sizeof(ev));
        ev.type = SDL_TEXTINPUT;
        strncpy(ev.text.text, num.c_str(), sizeof(ev.text.text) - 1);
        InputUserSDL3(ev);            // todo seleccionado: lo tipeado reemplaza
        memset(&ev, 0, sizeof(ev));
        ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_RETURN; ev.key.keysym.scancode = SDL_SCANCODE_RETURN;
        ev.key.state = SDL_PRESSED;
        InputUserSDL3(ev);
        ev.type = SDL_KEYUP; ev.key.state = SDL_RELEASED;
        InputUserSDL3(ev);
        rootViewport->Render();          // fin de la edicion: el paso de Ctrl+Z
        printf("      [propsalfa] tipeado '%s': %g -> %g\n", num.c_str(), prev, *f->value);
        if (NumEditActive()) { err = "propsalpha: la edicion por texto sigue abierta despues del Enter"; return false; }
        return true;
    }
    err = "propsalpha: fila [valor|-|arrastre] | arrastrar <px> [min max] | texto <numero>";
    return false;
}
#endif

// vpoverlays [on|off] [es 0|1] [publicado 0|1]: el ojo "Show Overlays" del PRIMER viewport 3D del layout: lo prende o
// apaga (SetShowOverlays, lo mismo que el boton de su barra), asserta como esta y, con 'publicado', que al dibujar el
// viewport publica ese valor (showOverlayGlobal: lo que miran los gizmos, la grilla y la curva; el Play lo respeta)
static bool CmdVpOverlays(std::istringstream& ss, std::string& err) {
    Viewport3D* v = (Viewport3D*)FindLeaf(rootViewport, 1);
    if (!v) { err = "vpoverlays: no hay viewport 3D en el layout"; return false; }
    std::string k;
    while (ss >> k) {
        if (k == "on" || k == "off") { v->SetShowOverlays(k == "on"); continue; }
        std::string val; ss >> val;
        const bool esp = (val == "1");
        if (k == "es") {
            printf("      [vpoverlays] showOverlays=%s\n", v->showOverlays ? "si" : "no");
            if (v->showOverlays != esp) { err = std::string("vpoverlays: el viewport tiene los overlays ") + (v->showOverlays ? "on" : "off"); return false; }
        } else if (k == "published") {
            rootViewport->Render();
            printf("      [vpoverlays] publicado=%s\n", showOverlayGlobal ? "si" : "no");
            if (showOverlayGlobal != esp) { err = std::string("vpoverlays: al dibujar se publico ") + (showOverlayGlobal ? "on" : "off"); return false; }
        } else { err = "vpoverlays: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// ============================================================================
//  EL DESPACHADOR
// ============================================================================
bool W3dTestsOutlinerCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "outview")        return CmdOutView(ss, err);
    if (cmd == "outrows")        return CmdOutRows(ss, err);
    if (cmd == "outrow")         return CmdOutRow(ss, err, false);
    if (cmd == "outnorow")       return CmdOutRow(ss, err, true);
    if (cmd == "outclick")        return CmdOutClick(ss, err);
    if (cmd == "outdrag")         return CmdOutDrag(ss, err);
    if (cmd == "outorder")        return CmdOutOrder(ss, err);
    if (cmd == "releasefile")   return CmdReleaseFile(ss, err);
    if (cmd == "outcolumn")      return CmdOutColumn(ss, err);
    if (cmd == "outcontext")     return CmdOutContext(ss, err);
    if (cmd == "outkey")        return CmdOutKey(ss, err);
    if (cmd == "outkeypc")      return CmdOutKeyPC(ss, err);
    if (cmd == "outbar")        return CmdOutBar(ss, err);
    if (cmd == "outmenu")         return CmdOutMenu(ss, err);
    // bibafuera <carpeta>: contesta el explorador que abrio "Extract to a folder..." de la biblioteca con esa carpeta
    if (cmd == "libraryoutside") {
        std::string c; ss >> c;
        if (c.empty()) { err = "libraryoutside: uso: libraryoutside <carpeta>"; return false; }
        extern void OutlinerRecOutsidePicked(const std::string&);
        if (PopUpActive) PopUpActive->Close();   // (el explorador)
        OutlinerRecOutsidePicked(c);
        return true;
    }
    if (cmd == "outwrite")     return CmdOutWrite(ss, err);
    if (cmd == "outconfirm")    return CmdOutConfirm(ss, err);
    if (cmd == "outrender")       return CmdOutRender(err);
    if (cmd == "outpx")           return CmdOutPx(ss, err);
    if (cmd == "recactive")       return CmdRecActive(ss, err);
    if (cmd == "recprops")        return CmdRecProps(ss, err);
    if (cmd == "recedit")       return CmdRecEdit(ss, err);
    if (cmd == "resources")        return CmdResources(ss, err);
    if (cmd == "resource")         return CmdResource(ss, err);
    if (cmd == "folders")        return CmdFolders(ss, err);
    if (cmd == "foldernew")    return CmdNewFolder(ss, err);
    if (cmd == "folderrename") return CmdFolderRename(ss, err);
    if (cmd == "folderdelete")   return CmdFolderDelete(ss, err);
    if (cmd == "foldermove")    return CmdFolderMove(ss, err);
    if (cmd == "recmove")        return CmdRecMove(ss, err);
    if (cmd == "recrename")    return CmdRecRename(ss, err);
    if (cmd == "recdelete")       return CmdRecDelete(ss, err);
    if (cmd == "recpurge")       return CmdRecPurge(ss, err);
    if (cmd == "matloose")       return CmdMatLoose(ss, err);
    if (cmd == "objtexture")      return CmdTexAssign(ss, err);
    if (cmd == "texentry")      return CmdTexEntry(ss, err);
    if (cmd == "jsonhas")       return CmdJsonHas(ss, err);
    if (cmd == "selection")       return CmdSelection(ss, err);
    if (cmd == "recprovtest")   return CmdRecProvTest(ss, err);
    if (cmd == "outsel")          return CmdOutSel(ss, err);
    if (cmd == "outtype")       return CmdOutType(ss, err);
    if (cmd == "outesc")          return CmdOutEsc(err);
    if (cmd == "fieldfocus")    return CmdFieldFocus(ss, err);
    if (cmd == "texloaded")      return CmdTexLoaded(ss, err);
    if (cmd == "texpurged")     return CmdTexPurged(ss, err);
    if (cmd == "pairtex")          return CmdPairTex(ss, err);
    if (cmd == "fliptex")         return CmdFlipTex(ss, err);
    if (cmd == "texrefof")        return CmdTexRefOf(ss, err);
    if (cmd == "entryraw")    return CmdEntryRaw(ss, err, false);
    if (cmd == "entrytext")    return CmdEntryRaw(ss, err, true);
    if (cmd == "outbackground")        return CmdOutBackground(ss, err);
    if (cmd == "outdragover")    return CmdOutDragOver(ss, err);
    if (cmd == "recbutton")        return CmdRecButton(ss, err);
    if (cmd == "recdata")        return CmdRecData(ss, err);
    if (cmd == "texasleep")     return CmdTexAsleep(ss, err);
    // FASE 4b: la biblioteca
    if (cmd == "outdoubleclick")   return CmdOutDoubleClick(ss, err);
    if (cmd == "outtap")          return CmdOutTap(ss, err);
    if (cmd == "outkeep")     return CmdOutKeep(ss, err);
    if (cmd == "outfilter")       return CmdOutFilter(ss, err);
    if (cmd == "outgrid")   return CmdOutGrid(ss, err);
    if (cmd == "outmini")         return CmdOutMini(ss, err);
    if (cmd == "outline")        return CmdOutLine(ss, err);
    if (cmd == "outcolor")        return CmdOutColor(ss, err);
    if (cmd == "outrelease")       return CmdOutRelease(ss, err);
    if (cmd == "outpreview")       return CmdOutPreview(ss, err);
    if (cmd == "liblink")     return CmdLibLink(ss, err);
    if (cmd == "libunlink")  return CmdLibUnlink(ss, err);
    if (cmd == "libs")            return CmdLibs(ss, err);
    if (cmd == "changes")         return CmdChanges(ss, err);
    if (cmd == "changessnapshot")     { W3dChangesSnapshot(); return true; }
    if (cmd == "changedirty")     return CmdChangeDirty(ss, err);
    if (cmd == "changeroot")      return CmdChangeRoot(ss, err);
    if (cmd == "dialog")          return CmdDialog(ss, err);
    if (cmd == "dialogshow")   return CmdDialogTurnOn(ss, err);
    if (cmd == "dialogrespond") return CmdDialogRespond(ss, err);
    if (cmd == "meshesresource")   return CmdMeshesResource(ss, err);
    if (cmd == "outmove")        return CmdOutMove(ss, err);
    if (cmd == "changesmark")    return CmdChangesMark(ss, err);
    if (cmd == "reclocate")       return CmdRecLocate(ss, err);
    if (cmd == "reclocation")    return CmdRecLocation(ss, err);
    if (cmd == "recsaveas")  return CmdRecSaveAs(ss, err);
    if (cmd == "recduplicate")     return CmdRecDuplicate(ss, err);
    if (cmd == "recdeleteforced") return CmdRecDeleteForced(ss, err);
    if (cmd == "recfield")         return CmdRecDataItem(ss, err);
    if (cmd == "propsmesh")      return CmdPropsMesh(ss, err);
    if (cmd == "propstab")        return CmdPropsTab(ss, err);
    if (cmd == "filemove")    return CmdFileMove(ss, err);
    if (cmd == "idelibrary")          return CmdIdeBib(ss, err);
    if (cmd == "recexternal")      return CmdRecExternal(ss, err);
    if (cmd == "minidistinct")   return CmdMiniDistinct(ss, err);
    if (cmd == "librarymeasure")        return CmdBibMeasure(ss, err);
#ifndef W3D_SYMBIAN
    if (cmd == "propsalpha")       return CmdPropsAlpha(ss, err);
#endif
    if (cmd == "vpoverlays")      return CmdVpOverlays(ss, err);
    handled = false;
    return false;
}
