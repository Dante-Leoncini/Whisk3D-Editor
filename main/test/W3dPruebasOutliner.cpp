// ============================================================================
//  W3dPruebasOutliner.cpp — comandos de harness del OUTLINER POR RECURSOS
//  (ui/ViewPorts/Outliner.h + OutlinerRecursos.cpp, io/RecursosProyecto.h).
//  Casi todo va por la PUERTA DEL USUARIO: el click (LayoutClickUI + LayoutSoltar),
//  las teclas del N95 (LayoutTeclaPanelActivo / LayoutTeclaUI con el outliner
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
//                                        el 5 marca/desmarca el cursor, como W3dOutlinerMarcarToggle;
//                                        ctoque = el toque de C al soltar, como W3dOutlinerRecursosBorrar)
//    outteclapc <up|down|left|right|enter|f2|supr|g|a|esc|shiftup|shiftdown|alta|ctrli|shiftd|altd>   el teclado de PC por la
//                                        puerta real: flechas/Enter/Esc a la UI compartida (LayoutTeclaUI)
//                                        con el mouse sobre el outliner; lo demas a event_key_down
//    outsel [n N] [tiene X]... [notiene X]...   la SELECCION MULTIPLE (filas por nombre o clave)
//    outtipear <texto>             tipea en el campo enfocado SIN confirmar (reemplaza lo seleccionado)
//    outesc                        la tecla Esc de PC por la puerta real (InputUsuarioSDL3): descarta
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
//    jsontiene <ruta.w3d> <texto> [0|1]   el proyecto.json del contenedor tiene (o no) el texto
//    seleccion [n N] [tiene X]... [modo objeto|edit]   los seleccionados (ObjSelects) y el modo
//    entradacruda <entrada> <archivo> / entradatexto <entrada> <texto...>
//        una entrada del contenedor montado con ese nombre EXACTO (el overlay: "texturas/Piso_A.png",
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
//        el primer IDE del layout y el "Open in IDE" de la biblioteca (IDEAbrirScript)
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
//        en la fila, arrastre horizontal y soltar; o click sin arrastrar + tipear + Enter (ver CmdPropsAlfa)
//    vpoverlays [on|off] [es 0|1] [publicado 0|1]   el ojo "Show Overlays" del primer viewport 3D
// ============================================================================
#include "test/W3dPruebasOutliner.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/ViewPort3D.h"       // vpoverlays: el ojo "Show Overlays" del viewport 3D
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/PropImagen.h"
#include "ViewPorts/IDE.h"                // idebib: el "Open in IDE" de la biblioteca
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/ConfirmarPopup.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/widgets/TextField.h"
#include "io/RecursosProyecto.h"
#include "io/BibliotecaExterna.h"
#include "io/CambiosProyecto.h"
#include "io/Miniaturas.h"
#include "io/MallasProyecto.h"
#include "objects/MallaRecurso.h"
#include "W3dRaices.h"
#include "io/W3dContenedor.h"
#include "io/W3dZip.h"
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Particulas.h"         // partex: un usuario de textura por ruta
#include "objects/ObjectMode.h"         // W3dRenombrarObjeto
#include "animation/Flipbook.h"         // fliptex: el atlas de un flipbook con nombre
#include "config/W3dLang.h"
#include "importers/import_obj.h"     // texdormidas: las texturas dormidas y la cola diferida
#include "base/W3dNombres.h"
#include "base/W3dInteractionState.h"   // seleccion modo: InteractionMode
#include "variables.h"
#include "w3dGraphics.h"
#include "w3dFilesystem.h"            // entradacruda: los bytes de un archivo de prueba
#include "gfx/w3dTexture.h"          // outmini png: SavePNG
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

extern int W3dPantallaAlto;

// ---------------------------------------------------------------------------
//  AYUDAS
// ---------------------------------------------------------------------------
static ViewportBase* BuscarHoja(ViewportBase* n, int kind) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == kind ? n : NULL;
    ViewportBase* a = NULL; ViewportBase* b = NULL;
    if (n->ContainerKind() == 1) { a = ((ViewportRow*)n)->childA; b = ((ViewportRow*)n)->childB; }
    else { a = ((ViewportColumn*)n)->childA; b = ((ViewportColumn*)n)->childB; }
    ViewportBase* r = BuscarHoja(a, kind);
    return r ? r : BuscarHoja(b, kind);
}
static Outliner* ElOutliner(std::string& err, const char* cmd) {
    Outliner* o = (Outliner*)BuscarHoja(rootViewport, 2);
    if (!o) err = std::string(cmd) + ": no hay outliner en el layout";
    return o;
}
static Properties* ElPanel() {
    if (PropsActivo) return PropsActivo;
    PropsActivo = (Properties*)BuscarHoja(rootViewport, 3);
    return PropsActivo;
}
static int VistaDe(const std::string& c, std::string& err, const char* cmd) {
    const int v = W3dVistaDeClave(c);
    if (v < 0) err = std::string(cmd) + ": vista desconocida '" + c + "'";
    return v;
}
static std::string Guion(const std::string& s) { return s == "-" ? std::string() : s; }
static std::string Entero(long n) { char b[32]; sprintf(b, "%ld", n); return std::string(b); }
// el id de un recurso de AFUERA: el harness escribe rutas RELATIVAS a tools/pruebas y 'recubicar' las pasa
// afuera con la ruta absoluta (la de disco): si el id tal cual no es de la vista y el absoluto si, va ese
static std::string IdDeVista(int vista, const std::string& id) {
    if (id.empty() || id[0] == '/' || W3dVistaRecInfo(vista, id, NULL)) return id;
    char b[4096];
    if (!getcwd(b, sizeof(b))) return id;
    const std::string abs = std::string(b) + "/" + id;
    return W3dVistaRecInfo(vista, abs, NULL) ? abs : id;
}

// la fila por CLAVE (la ruta de una carpeta, el id de un recurso: es unica) o por NOMBRE (lo que
// se ve: dos carpetas hermanas de madres distintas pueden llamarse igual)
static int FilaDe(const std::vector<OutFilaRec>& filas, const std::string& n) {
    for (size_t i = 0; i < filas.size(); i++) if (filas[i].clave == n) return (int)i;
    for (size_t i = 0; i < filas.size(); i++) if (!filas[i].carpeta && filas[i].id == n) return (int)i;
    for (size_t i = 0; i < filas.size(); i++) if (filas[i].nombre == n) return (int)i;
    return -1;
}
// la clave de la vista (el harness la compara): escena / biblioteca (o el filtro: mallas, materiales...) / lib:...
static std::string ClaveVista(const Outliner* o) {
    if (o->vista == OUT_VISTA_ESCENA) return "escena";
    if (o->vista == OUT_VISTA_BIBLIOTECA) return o->filtro > W3D_VISTA_ESCENA ? std::string(W3dVistaClave(o->filtro)) : std::string("biblioteca");
    return o->VistaClave();
}
// si 'c' es la clave de un tipo, se la come (los comandos de carpetas de la fase anterior la llevaban: hoy
// el arbol de carpetas es UNO para todos los tipos)
static std::string SinClaveDeTipo(std::istringstream& ss) {
    std::string c; ss >> c;
    if (W3dVistaDeClave(c) > W3D_VISTA_ESCENA) ss >> c;
    return c;
}
// deja la fila a la vista (scroll) y devuelve la Y de su centro
static int YDeFila(Outliner* o, int fila) {
    const int top = o->BarTopOffset();
    int yf, alto, base;
    if (o->cuadricula) {
        std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
        int rx, ry, rw, rh; o->RectFila(filas, fila, rx, ry, rw, rh);
        base = ry - borderGS - top; alto = rh; yf = ry + o->PosY;
    } else { alto = (int)RenglonHeightGS; base = fila * alto; yf = borderGS + o->PosY + top + base; }
    if (yf < top || yf + alto > o->height) {
        o->PosY = -base;
        if (o->PosY < o->MaxPosY) o->PosY = o->MaxPosY;
        if (o->PosY > 0) o->PosY = 0;
    }
    return o->FilaRecursoY(fila);
}
static int XNombre(Outliner* o, const OutFilaRec& f) {
    if (o->cuadricula && !f.carpeta) {
        std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
        for (size_t i = 0; i < filas.size(); i++)
            if (filas[i].carpeta == f.carpeta && filas[i].clave == f.clave) {
                int rx, ry, rw, rh; o->RectFila(filas, (int)i, rx, ry, rw, rh);
                return o->x + rx + rw / 2;
            }
    }
    return o->x + marginGS + (o->cuadricula ? 0 : o->PosX) + (f.prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
}
static int XFlecha(Outliner* o, const OutFilaRec& f) {
    return o->x + marginGS + (o->cuadricula ? 0 : o->PosX) + f.prof * (IconSizeGS + gapGS) + IconSizeGS / 2;
}
static void Volcar(const std::vector<OutFilaRec>& filas, const char* cmd) {
    for (size_t i = 0; i < filas.size(); i++) {
        const OutFilaRec& f = filas[i];
        printf("      [%s] %2d %s%s%s%s", cmd, (int)i, std::string((size_t)f.prof * 2, ' ').c_str(),
               f.carpeta ? (f.plegada ? "+ " : "- ") : "  ", f.nombre.c_str(), f.carpeta ? "/" : "");
        if (!f.carpeta) printf("  [%s %d]%s", f.usuarios > 0 ? "EN USO" : "HUERFANO", f.usuarios, f.soloLectura ? " (solo lectura)" : "");
        printf("\n");
    }
}
// cual menu de la barra del outliner esta abierto
static std::string MenuAbiertoNombre(Outliner* o) {
    extern bool LayoutMenuAbierto();
    if (!LayoutMenuAbierto()) return "ninguno";
    const int b = o->BotonDelMenuAbierto();
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnVista) return "vista";
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnObjeto) return "objeto";
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnSeleccion) return "seleccion";
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnFiltro) return "filtro";
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnRaiz) return "raiz";   // el selector de escena/prefab
    if (b >= 0 && o->BarButtons[(size_t)b] == o->btnNueva) return "nueva"; // el "+" (nueva escena/juego/prefab/carpeta/material)
    if (MenuAbierto && MenuAbierto->titulo == T("Move to Folder")) return "mover";
    return "tipo";   // el de tipo/split del viewport (el [0]) u otro
}
static MenuItem* ItemPorTexto(PopupMenu* m, const std::string& t) {
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
static bool ElegirEnMenuBarra(Outliner* o, Button* b, int id, std::string& err, const char* cmd) {
    int idx = -1;
    for (size_t i = 0; i < o->BarButtons.size(); i++) if (o->BarButtons[i] == b) idx = (int)i;
    if (idx < 0 || !o->AbrirMenuBoton(idx) || !MenuAbierto) { err = std::string(cmd) + ": no se abrio el menu"; return false; }
    MenuItem* it = NULL;
    for (size_t i = 0; i < MenuAbierto->items.size(); i++) if (MenuAbierto->items[i]->id == id) it = MenuAbierto->items[i];
    if (!it) { MenuAbierto->Cerrar(); err = std::string(cmd) + ": el menu no ofrece esa opcion"; return false; }
    PopupMenu* m = MenuAbierto;
    m->Cerrar();
    m->Ejecutar(it->id);
    return true;
}
// outvista <escena|biblioteca|lib:<ruta>|<tipo>> [es <clave>]: la VISTA por el menu del ojo ([1]); un TIPO
// ("materiales") es la biblioteca con ese FILTRO (por el menu del filtro)
static bool CmdOutVista(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outvista"); if (!o) return false;
    std::string a; ss >> a;
    if (!a.empty() && a != "es") {
        int v = -1, filtro = -2;
        if (a == "escena") v = OUT_VISTA_ESCENA;
        else if (a == "biblioteca") { v = OUT_VISTA_BIBLIOTECA; filtro = -1; }
        else if (a.compare(0, 4, "lib:") == 0) {
            for (int i = 0; i < W3dLibreriasCantidad(); i++) if (W3dLibreriaRuta(i) == a.substr(4) || W3dLibreriaNombre(i) == a.substr(4)) v = OUT_VISTA_LIBEXT + i;
            if (v < 0) { err = "outvista: la libreria '" + a.substr(4) + "' no esta vinculada"; return false; }
        } else {
            const int t = VistaDe(a, err, "outvista"); if (t < 0) return false;
            v = OUT_VISTA_BIBLIOTECA; filtro = t;
        }
        if (!ElegirEnMenuBarra(o, o->btnVista, v, err, "outvista")) return false;
        if (filtro != -2 && o->filtro != filtro && !ElegirEnMenuBarra(o, o->btnFiltro, filtro < 0 ? 0 : filtro, err, "outvista")) return false;
        ss >> a;
    }
    if (a == "es") {
        std::string c; ss >> c;
        if (ClaveVista(o) != c) { err = "outvista: la vista es '" + ClaveVista(o) + "', se esperaba '" + c + "'"; return false; }
    }
    printf("      [outvista] %s (boton '%s', filtro %s, %s)\n", ClaveVista(o).c_str(), o->btnVista->text.c_str(),
           o->filtro < 0 ? "todo" : W3dVistaClave(o->filtro), o->cuadricula ? "cuadricula" : "lista");
    return true;
}

static bool CmdOutFilas(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outfilas"); if (!o) return false;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    Volcar(filas, "outfilas");
    std::string k; int n = -1;
    if ((ss >> k) && k == "n" && (ss >> n) && n != (int)filas.size()) {
        err = "outfilas: " + Entero((long)filas.size()) + " filas, se esperaban " + Entero(n); return false;
    }
    return true;
}

static bool CmdOutFila(std::istringstream& ss, std::string& err, bool negada) {
    Outliner* o = ElOutliner(err, "outfila"); if (!o) return false;
    std::string n; ss >> n;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (negada) {
        if (i >= 0) { err = "outnofila: la fila '" + n + "' esta"; return false; }
        printf("      [outnofila] '%s' no esta\n", n.c_str());
        return true;
    }
    if (i < 0) { Volcar(filas, "outfila"); err = "outfila: no hay una fila '" + n + "'"; return false; }
    const OutFilaRec& f = filas[(size_t)i];
    int va = -1; std::string ida; W3dRecursoActivo(&va, &ida);
    const bool cursor = (f.carpeta == o->cursorEsCarpeta && f.clave == o->cursorClave);
    const bool activo = !f.carpeta && va == f.tipo && ida == f.id;
    printf("      [outfila] %d '%s' %s prof=%d carpeta='%s' usuarios=%d%s%s%s\n", i, f.nombre.c_str(),
           f.carpeta ? "CARPETA" : (f.usuarios > 0 ? "EN USO" : "HUERFANO"), f.prof, f.carpetaDe.c_str(), f.usuarios,
           f.plegada ? " plegada" : "", cursor ? " cursor" : "", activo ? " activo" : "");
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        bool ok = true;
        if (k == "usuarios") ok = (f.usuarios == atoi(v.c_str()));
        else if (k == "estado") ok = (v == "uso") ? (f.usuarios > 0 && !f.carpeta) : (f.usuarios == 0 && !f.carpeta);
        else if (k == "prof") ok = (f.prof == atoi(v.c_str()));
        else if (k == "carpeta") ok = (f.carpetaDe == Guion(v));
        else if (k == "plegada") ok = (f.plegada == (v == "1"));
        else if (k == "fila") ok = (i == atoi(v.c_str()));
        else if (k == "cursor") ok = (cursor == (v == "1"));
        else if (k == "activo") ok = (activo == (v == "1"));
        else if (k == "icono") {
            ok = (v == IconoNombre(f.icono));
            if (!ok) printf("      [outfila] '%s' tiene el icono '%s'\n", n.c_str(), IconoNombre(f.icono));
        }
        else if (k == "tipo") ok = (!f.carpeta && W3dVistaClave(f.tipo) == v);
        else if (k == "sucio") ok = (f.sucio == (v == "1"));
        else { err = "outfila: no entiendo '" + k + "'"; return false; }
        if (!ok) { err = "outfila: '" + n + "' no cumple " + k + " " + v; return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  MOUSE
// ---------------------------------------------------------------------------
static bool PuntoDeNombre(Outliner* o, const std::string& n, int& mx, int& my, std::string& err, const char* cmd);
static bool CmdOutClick(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outclick"); if (!o) return false;
    std::string n, modo; ss >> n >> modo;
    int mx = 0, my = 0;
    if (o->vista == OUT_VISTA_ESCENA) {
        // la ESCENA: la fila del objeto (su nombre)
        if (!PuntoDeNombre(o, n, mx, my, err, "outclick")) return false;
    } else {
        std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
        const int i = FilaDe(filas, n);
        if (i < 0) { err = "outclick: no hay una fila '" + n + "'"; return false; }
        my = YDeFila(o, i);
        mx = (modo == "flecha") ? XFlecha(o, filas[(size_t)i]) : XNombre(o, filas[(size_t)i]);
    }
    // los modificadores de la seleccion multiple: apretados durante el click, como el teclado real
    LCtrlPressed = (modo == "ctrl");
    LShiftPressed = (modo == "shift");
    // (como controles.cpp: el down de un click en cualquier lado DESENFOCA el campo de texto -> un rename en
    // linea abierto se confirma con lo tipeado)
    if (!PopUpActive) g_textFieldActivo = NULL;
    leftMouseDown = true;
    LayoutClickUI(mx, my);
    leftMouseDown = false;
    LayoutSoltar(mx, my);
    LCtrlPressed = LShiftPressed = false;
    printf("      [outclick] '%s'%s en (%d, %d) seleccion=%d\n", n.c_str(), modo.empty() ? "" : (" (" + modo + ")").c_str(),
           mx, my, o->SeleccionCantidad());
    return true;
}

static bool CmdOutDrag(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outdrag"); if (!o) return false;
    std::string n, d; ss >> n >> d;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0) { err = "outdrag: no hay una fila '" + n + "'"; return false; }
    const int x0 = XNombre(o, filas[(size_t)i]), y0 = YDeFila(o, i);
    int x1 = x0, y1;
    if (d == "-") {
        // el VACIO debajo de la ultima fila (tiene que estar dentro del outliner)
        y1 = o->FilaRecursoY((int)filas.size()) + (int)RenglonHeightGS;
        if (y1 >= o->y + o->height) { err = "outdrag: no hay vacio a la vista debajo de la lista"; return false; }
    } else {
        const int j = FilaDe(filas, d);
        if (j < 0) { err = "outdrag: no hay una fila destino '" + d + "'"; return false; }
        y1 = o->FilaRecursoY(j);
        x1 = XNombre(o, filas[(size_t)j]);
    }
    leftMouseDown = true;
    LayoutClickUI(x0, y0);
    // el movimiento en DOS pasos (primero pasa el umbral del arrastre, despues llega)
    o->event_mouse_motion(x0, y0 + (y1 > y0 ? 1 : -1) * (int)RenglonHeightGS);
    o->event_mouse_motion(x1, y1);
    const bool arrastraba = o->recArrastrando;
    leftMouseDown = false;
    LayoutSoltar(x1, y1);
    printf("      [outdrag] '%s' -> '%s' (arrastre %s)\n", n.c_str(), d.c_str(), arrastraba ? "si" : "no");
    if (!arrastraba) { err = "outdrag: el arrastre no arranco"; return false; }
    return true;
}

static bool CmdOutContexto(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outcontexto"); if (!o) return false;
    std::string n; ss >> n;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0) { err = "outcontexto: no hay una fila '" + n + "'"; return false; }
    const int mx = XNombre(o, filas[(size_t)i]), my = YDeFila(o, i);
    if (!o->MenuContexto(mx, my) || !MenuAbierto) { err = "outcontexto: no se abrio el menu"; return false; }
    printf("      [outcontexto] '%s':", n.c_str());
    for (size_t k = 0; k < MenuAbierto->items.size(); k++) printf(" [%s]", MenuAbierto->items[k]->text.c_str());
    printf("\n");
    return true;
}

// ---------------------------------------------------------------------------
//  TECLADO
// ---------------------------------------------------------------------------
static int TeclaLayout(const std::string& t) {
    if (t == "up" || t == "arriba") return LayoutKey::Up;
    if (t == "down" || t == "abajo") return LayoutKey::Down;
    if (t == "left" || t == "izq") return LayoutKey::Left;
    if (t == "right" || t == "der") return LayoutKey::Right;
    if (t == "ok") return LayoutKey::Enter;
    if (t == "c") return LayoutKey::Cancel;
    return -1;
}
static bool CmdOutTecla(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outtecla"); if (!o) return false;
    std::string t; int veces = 1; ss >> t >> veces;
    if (t == "5") {
        // el 5 del N95 (W3dOutlinerMarcarToggle en w3dlayout.cpp llama a este mismo metodo)
        viewPortActive = o;
        for (int i = 0; i < veces; i++) o->MarcarCursor();
        printf("      [outtecla] 5 x%d -> seleccion %d\n", veces, o->SeleccionCantidad());
        return true;
    }
    if (t == "ctoque") {
        // el TOQUE de C del N95 (soltada sin flecha): W3dOutlinerRecursosBorrar llama a este metodo
        viewPortActive = o;
        const bool usada = o->TeclaBorrarRecursos();
        printf("      [outtecla] ctoque -> %s\n", usada ? "borrar recursos" : "sigue a la escena");
        return true;
    }
    const int k = TeclaLayout(t);
    if (k < 0) { err = "outtecla: tecla desconocida '" + t + "'"; return false; }
    viewPortActive = o;   // el N95: el panel ACTIVO (borde verde) recibe el keypad, sin mouse
    for (int i = 0; i < veces; i++) LayoutTeclaPanelActivo(k);
    printf("      [outtecla] %s x%d -> cursor '%s'%s\n", t.c_str(), veces, o->cursorClave.c_str(), o->cursorEsCarpeta ? " (carpeta)" : "");
    return true;
}
static bool CmdOutTeclaPC(std::istringstream& ss, std::string& err) {
#ifndef W3D_SYMBIAN
    Outliner* o = ElOutliner(err, "outteclapc"); if (!o) return false;
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
    if (k < 0) { err = "outteclapc: tecla desconocida '" + t + "'"; return false; }
    // LA PUERTA DEL TECLADO REAL (controles.cpp): las flechas, el Enter y el Esc van PRIMERO a la UI
    // compartida (LayoutTeclaUI) con el mouse SOBRE el outliner (el panel bajo el mouse recibe el
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
    const bool porUI = (lk >= 0 && LayoutTeclaUI(lk, cx, cy));
    if (!porUI) o->event_key_down(k, false);
    LShiftPressed = LAltPressed = LCtrlPressed = false;
    printf("      [outteclapc] %s -> cursor '%s' (%s)\n", t.c_str(), o->cursorClave.c_str(), porUI ? "UI compartida" : "event_key_down");
    return true;
#else
    (void)ss; err = "outteclapc: sin teclado de PC"; return false;
#endif
}
static bool CmdOutBarra(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outbarra"); if (!o) return false;
    std::string t; ss >> t;
    viewPortActive = o;
    const int cx = o->x + o->width / 2, cy = o->y + o->height / 2;
    if (t == "soft") LayoutToggleBarraViewportActivo();
    else {
        const int k = TeclaLayout(t);
        if (k < 0) { err = "outbarra: tecla desconocida '" + t + "'"; return false; }
        LayoutTeclaUI(k, cx, cy);
    }
    const std::string m = MenuAbiertoNombre(o);
    MenuItem* it = (m != "ninguno" && MenuAbierto) ? MenuAbierto->ItemActual() : NULL;
    printf("      [outbarra] %s -> menu %s, resaltado '%s', vista %s\n", t.c_str(), m.c_str(),
           it ? it->text.c_str() : "", ClaveVista(o).c_str());
    std::string k2, v2;
    if ((ss >> k2) && k2 == "menu" && (ss >> v2) && v2 != m) {
        err = "outbarra: quedo abierto el menu '" + m + "', se esperaba '" + v2 + "'"; return false;
    }
    return true;
}
// outmenu <texto> | outmenu mover <carpeta|->: abre el menu OBJETO con un click en su boton y elige la opcion
// (texto en ingles); si no esta ahi, la busca en el "+" (New Folder, New Material...) y en Seleccion
static bool ClickBotonBarra(Outliner* o, Button* b) {
    if (!b || !b->visible) return false;
    // un click de mouse no usa el foco de teclado de la barra (en la app lo limpia RenderBar al cerrar el
    // menu; el harness puede no haber dibujado desde la ultima navegacion con flechas)
    if (!MenuAbierto || !MenuAbierto->abierto) o->barFocusIndex = -1;
    o->barScrollManual = 0;
    o->ActualizarBarra();
    const int sobra = (b->sx + b->width) - (o->x + o->width) + 4;
    if (sobra > 0) { o->barScrollManual = sobra; o->ActualizarBarra(); }
    const int bx = b->sx + b->width / 2, by = b->sy + b->height / 2;
    LayoutClickUI(bx, by);
    LayoutSoltar(bx, by);
    return MenuAbierto && MenuAbierto->abierto;
}
static bool CmdOutMenu(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outmenu"); if (!o) return false;
    std::string resto; std::getline(ss, resto);
    size_t a = resto.find_first_not_of(' ');
    resto = (a == std::string::npos) ? std::string() : resto.substr(a);
    if (!ClickBotonBarra(o, o->btnObjeto)) { err = "outmenu: el click no abrio el menu Objeto"; return false; }
    PopupMenu* m = MenuAbierto;
    if (resto.compare(0, 6, "mover ") == 0) {
        MenuItem* sub = ItemPorTexto(m, T("Move to Folder"));
        if (!sub || !sub->submenu) { m->Cerrar(); err = "outmenu: no hay 'Move to Folder' (el cursor no es un recurso?)"; return false; }
        const std::string c = resto.substr(6);
        MenuItem* dest = NULL;
        if (c == "-") dest = ItemPorTexto(sub->submenu, std::string("(") + T("root") + ")");
        else dest = ItemPorTexto(sub->submenu, W3dCarpetaHoja(c));
        if (!dest) { m->Cerrar(); err = "outmenu: 'Move to Folder' no ofrece '" + c + "'"; return false; }
        m->Cerrar();
        sub->submenu->Ejecutar(dest->id);
        printf("      [outmenu] Move to Folder > %s\n", c.c_str());
        return true;
    }
    MenuItem* it = ItemPorTexto(m, T(resto.c_str()));
    std::string hay;
    for (size_t i = 0; i < m->items.size(); i++) hay += " [" + m->items[i]->text + "]";
    if (!it) {
        m->Cerrar();
        Button* otros[2] = { o->btnNueva, o->btnSeleccion };
        for (int k = 0; k < 2 && !it; k++) {
            if (!ClickBotonBarra(o, otros[k])) continue;
            m = MenuAbierto;
            for (size_t i = 0; i < m->items.size(); i++) hay += " [" + m->items[i]->text + "]";
            it = ItemPorTexto(m, T(resto.c_str()));
            if (!it) m->Cerrar();
        }
    }
    if (!it) { err = "outmenu: los menus no tienen '" + resto + "' (tienen:" + hay + ")"; return false; }
    if (it->gris && !*it->gris) { m->Cerrar(); err = "outmenu: '" + resto + "' esta deshabilitado"; return false; }
    m->Cerrar();
    m->Ejecutar(it->id);
    printf("      [outmenu] %s\n", resto.c_str());
    return true;
}
static bool CmdOutEscribir(std::istringstream& ss, std::string& err) {
    std::string t; std::getline(ss, t);
    size_t a = t.find_first_not_of(' ');
    t = (a == std::string::npos) ? std::string() : t.substr(a);
    if (!g_textFieldActivo) { err = "outescribir: no hay un campo de texto enfocado"; return false; }
    if (!g_textFieldActivo->selectAll) g_textFieldActivo->SelectAll();
    if (t.empty()) g_textFieldActivo->Backspace();   // con todo seleccionado: lo borra
    for (size_t i = 0; i < t.size(); i++) TextFieldInputChar((unsigned char)t[i]);
    // Enter (la misma rama que el teclado: sale del campo) y un cuadro (los campos confirman al perder el foco)
    Outliner* o = (Outliner*)BuscarHoja(rootViewport, 2);
    LayoutTeclaUI(LayoutKey::Enter, o ? o->x + 2 : 0, o ? o->y + 2 : 0);
    if (rootViewport) rootViewport->Render();
    printf("      [outescribir] '%s'\n", t.c_str());
    return true;
}
static bool CmdOutSel(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outsel"); if (!o) return false;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    printf("      [outsel] %d:", o->SeleccionCantidad());
    for (std::set<std::string>::iterator it = o->selCarpetas.begin(); it != o->selCarpetas.end(); ++it) printf(" '%s/'", it->c_str());
    for (std::set<std::string>::iterator it = o->selRecursos.begin(); it != o->selRecursos.end(); ++it) printf(" '%s'", it->c_str());
    printf("\n");
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "n") {
            if (o->SeleccionCantidad() != atoi(v.c_str())) { err = "outsel: hay " + Entero(o->SeleccionCantidad()) + ", se esperaban " + v; return false; }
            continue;
        }
        // por fila visible (nombre o clave), o por clave directa (lo que esta plegado no se ve)
        const int i = FilaDe(filas, v);
        const bool esta = (i >= 0) ? o->FilaEnSeleccion(filas[(size_t)i]) : (o->selRecursos.count(v) || o->selCarpetas.count(v));
        if (k == "tiene" && !esta) { err = "outsel: '" + v + "' no esta seleccionado"; return false; }
        if (k == "notiene" && esta) { err = "outsel: '" + v + "' esta seleccionado"; return false; }
        if (k != "tiene" && k != "notiene") { err = "outsel: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
static bool CmdOutTipear(std::istringstream& ss, std::string& err) {
    std::string t; std::getline(ss, t);
    size_t a = t.find_first_not_of(' ');
    t = (a == std::string::npos) ? std::string() : t.substr(a);
    if (!g_textFieldActivo) { err = "outtipear: no hay un campo de texto enfocado"; return false; }
    if (!g_textFieldActivo->selectAll) g_textFieldActivo->SelectAll();
    for (size_t i = 0; i < t.size(); i++) TextFieldInputChar((unsigned char)t[i]);
    printf("      [outtipear] '%s' (el campo dice '%s')\n", t.c_str(), g_textFieldActivo->text.c_str());
    return true;
}
static bool CmdOutEsc(std::string& err) {
#ifndef W3D_SYMBIAN
    // la tecla Esc de PC entrando por la MISMA puerta que el teclado de verdad
    extern void InputUsuarioSDL3(SDL_Event&);
    SDL_Event ev; memset(&ev, 0, sizeof(ev));
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_ESCAPE;
    ev.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
    ev.key.state = SDL_PRESSED;
    const bool habia = (g_textFieldActivo != NULL);
    InputUsuarioSDL3(ev);
    ev.type = SDL_KEYUP; ev.key.state = SDL_RELEASED;
    InputUsuarioSDL3(ev);
    if (rootViewport) rootViewport->Render();   // (los duenos de los campos confirman al dibujar)
    TextFieldFinDeCuadro();                     // el fin del cuadro (LayoutRenderMenu en el editor)
    printf("      [outesc] %s\n", habia ? (g_textFieldActivo ? "el campo sigue enfocado" : "campo descartado") : "sin campo");
    return true;
#else
    err = "outesc: sin teclado de PC"; return false;
#endif
}
static bool CmdCampoEnfocar(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "campoenfocar: sin panel de propiedades"; return false; }
    std::string q; ss >> q;
    PropText* pt = (q == "recnombre") ? p->propRecNombre : (q == "reccarpeta") ? p->propRecCarpeta :
                   (q == "nombreobj") ? p->propNameObj : (q == "hbetiqueta") ? p->propHbEtiqueta : NULL;
    if (!pt) { err = "campoenfocar: recnombre|reccarpeta|nombreobj|hbetiqueta"; return false; }
    PropsActivo = p;
    if (q == "hbetiqueta") { p->pestaniaActiva = 2; p->ActualizarPestanias(); }   // (la tarjeta Hitbox)
    p->RefreshTargetProperties();
    pt->EditPropertie();              // la puerta del click en la fila (anota el texto de antes)
    p->RefreshTargetProperties();     // (el panel captura el destino al enfocar)
    printf("      [campoenfocar] %s = '%s'\n", q.c_str(), pt->field.text.c_str());
    if (g_textFieldActivo != &pt->field) { err = "campoenfocar: el campo no quedo enfocado"; return false; }
    return true;
}
static bool CmdTexCargada(std::istringstream& ss, std::string& err) {
    std::string r, v; ss >> r >> v;
    Texture* t = TexturaBuscar(r);
    const bool cargada = (t != NULL && t->iID != 0);
    printf("      [texcargada] '%s' %s\n", r.c_str(), cargada ? "CARGADA" : "no cargada");
    if (!v.empty() && cargada != (v == "1")) { err = "texcargada: '" + r + "' " + (cargada ? "esta" : "no esta") + " cargada"; return false; }
    return true;
}
static bool CmdTexPurgadas(std::istringstream& ss, std::string& err) {
    const std::vector<Texture*>& tp = TexturasPurgadas();
    printf("      [texpurgadas] %d:", (int)tp.size());
    for (size_t i = 0; i < tp.size(); i++) printf(" '%s'%s", tp[i]->path.c_str(), tp[i]->iID ? "(con GPU!)" : "");
    printf("\n");
    std::string k; int n = -1;
    if ((ss >> k) && k == "n" && (ss >> n) && n != (int)tp.size()) {
        err = "texpurgadas: hay " + Entero((long)tp.size()) + ", se esperaban " + Entero(n); return false;
    }
    for (size_t i = 0; i < tp.size(); i++) if (tp[i]->iID) { err = "texpurgadas: una purgada conserva su GPU"; return false; }
    return true;
}
static bool CmdParTex(std::istringstream& ss, std::string& err) {
    std::string n, r; ss >> n >> r;
    if (n.empty() || r.empty()) { err = "partex: <nombre> <ruta>"; return false; }
    Particulas* pt = new Particulas(SceneCollection);   // (el constructor lo cuelga de la escena)
    W3dRenombrarObjeto(pt, n, false);
    pt->textura = r;
    printf("      [partex] '%s' textura='%s'\n", pt->name.c_str(), r.c_str());
    return true;
}
static bool CmdFlipTex(std::istringstream& ss, std::string& err) {
    std::string n, r; ss >> n >> r;
    if (n.empty() || r.empty()) { err = "fliptex: <nombre> <ruta>"; return false; }
    Flipbook* f = FlipbookNuevo(n);
    if (!f) { err = "fliptex: no se pudo crear"; return false; }
    f->ConfigurarTira(r, 2, 2, 4, 8.0f);
    printf("      [fliptex] '%s' atlas='%s'\n", f->nombre.c_str(), f->atlas.c_str());
    return true;
}
static bool CmdTexRefDe(std::istringstream& ss, std::string& err) {
    std::string que, n, esp; ss >> que >> n >> esp;
    std::string r;
    if (que == "particula") {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
        if (!o || o->getType() != ObjectType::particulas) { err = "texrefde: no hay un emisor '" + n + "'"; return false; }
        r = ((Particulas*)o)->textura;
    } else if (que == "flipbook") {
        Flipbook* f = FlipbookPorNombre(n);
        if (!f) { err = "texrefde: no hay un flipbook '" + n + "'"; return false; }
        r = f->atlas;
    } else if (que == "material") {
        Material* m = W3dMaterialDeId(n);
        if (!m) { err = "texrefde: no hay un material '" + n + "'"; return false; }
        r = m->texture ? m->texture->path : std::string("-");
    } else { err = "texrefde: particula|flipbook|material"; return false; }
    printf("      [texrefde] %s '%s' -> '%s'\n", que.c_str(), n.c_str(), r.c_str());
    if (!esp.empty() && r != esp) { err = "texrefde: nombra '" + r + "', se esperaba '" + esp + "'"; return false; }
    return true;
}
// outconfirmar si|no [tiene|no <texto>]: el cartel de confirmacion abierto (y que diga, o no, ese texto: en
// ingles o traducido)
static bool CmdOutConfirmar(std::istringstream& ss, std::string& err) {
    std::string r, k; ss >> r >> k;
    if (!PopUpActive || PopUpActive != (PopUpBase*)confirmarPopup) { err = "outconfirmar: no hay un cartel de confirmacion abierto"; return false; }
    printf("      [outconfirmar] '%s' -> %s\n", confirmarPopup->mensaje.c_str(), r.c_str());
    if (k == "tiene" || k == "no") {
        std::string t; std::getline(ss, t);
        const size_t a = t.find_first_not_of(' ');
        t = (a == std::string::npos) ? std::string() : t.substr(a);
        // (en ingles o ya traducido, como sale en pantalla)
        const bool dice = confirmarPopup->mensaje.find(t) != std::string::npos ||
                          confirmarPopup->mensaje.find(T(t.c_str())) != std::string::npos;
        if (k == "tiene" && !dice) { err = "outconfirmar: el cartel no dice '" + t + "'"; return false; }
        if (k == "no" && dice) { err = "outconfirmar: el cartel dice '" + t + "'"; return false; }
    }
    PopUpActive->Tecla(r == "si" ? (int)LayoutKey::Accept : (int)LayoutKey::Cancel);
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
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0 || filas[(size_t)i].carpeta) { err = "outpx: no hay un recurso '" + n + "'"; return false; }
    YDeFila(o, i);
    rootViewport->Render();
    w3dEngine::Finish();
    const int reserva = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;
    const int xi = o->x + o->width - IconSizeGS - marginGS - borderGS - reserva;
    const int yi = o->y + borderGS + o->PosY + o->BarTopOffset() + i * (int)RenglonHeightGS + GlobalScale;
    std::vector<unsigned char> px((size_t)IconSizeGS * IconSizeGS * 4);
    w3dEngine::ReadPixelsRGBA(xi, W3dPantallaAlto - yi - IconSizeGS, IconSizeGS, IconSizeGS, &px[0]);
    int verdes = 0, rojos = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4) {
        const int r = px[p], g = px[p + 1], b = px[p + 2];
        if (g > r + 40 && g > b + 20) verdes++;
        if (r > g + 60 && r > b + 60) rojos++;
    }
    printf("      [outpx] '%s': %d px verdes, %d px rojos\n", n.c_str(), verdes, rojos);
    const bool ok = (esp == "uso") ? (verdes > 0 && rojos == 0) : (rojos > 0 && verdes == 0);
    if (!ok) { err = "outpx: el icono de '" + n + "' no es el de " + esp; return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  EL RECURSO ACTIVO Y PROPERTIES
// ---------------------------------------------------------------------------
static bool CmdRecActivo(std::istringstream& ss, std::string& err) {
    std::string c, id; ss >> c;
    int v = -1; std::string a;
    const bool hay = W3dRecursoActivo(&v, &a);
    printf("      [recactivo] %s%s%s\n", hay ? W3dVistaClave(v) : "ninguno", hay ? " " : "", a.c_str());
    if (c.empty()) return true;
    if (c == "ninguno") { if (hay) { err = "recactivo: hay un recurso activo"; return false; } return true; }
    std::getline(ss, id);
    size_t p = id.find_first_not_of(' ');
    id = (p == std::string::npos) ? std::string() : id.substr(p);
    if (!hay || W3dVistaClave(v) != c || a != id) { err = "recactivo: el activo no es " + c + " '" + id + "'"; return false; }
    return true;
}
static bool CmdRecProps(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "recprops: sin panel de propiedades"; return false; }
    p->RefreshTargetProperties();
    p->ActualizarPestanias();
    // (la tarjeta del recurso vive en la pestania 3, "Malla 3D": no hay pestania aparte)
    const bool enTab = p->pestaniaActiva == 3 && p->BarTabs.size() > 3 && p->BarTabs[3]->visible && p->propRecurso && p->propRecurso->visible;
    printf("      [recprops] pestania=%d tab=%s tarjeta=%s material=%s preview=%s nombre='%s' carpeta='%s'\n",
           p->pestaniaActiva, enTab ? "si" : "no", (p->propRecurso && p->propRecurso->visible) ? "si" : "no",
           (p->propMaterial && p->propMaterial->visible) ? "si" : "no",
           (p->propRecPreview && p->propRecPreview->Visible()) ? "si" : "no",
           p->propRecNombre ? p->propRecNombre->field.text.c_str() : "", p->propRecCarpeta ? p->propRecCarpeta->field.text.c_str() : "");
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "tab") { if (enTab != (v == "1")) { err = "recprops: la pestania RECURSO " + std::string(enTab ? "esta" : "no esta"); return false; } }
        else if (k == "material") {
            Material* m = BuscarMaterialPorNombre(v);
            if (!m) { err = "recprops: no hay un material '" + v + "'"; return false; }
            // la tarjeta Material (la de siempre) BINDEADA a ese material: sus checkbox apuntan a el
            if (!p->propMaterial || !p->propMaterial->visible) { err = "recprops: la tarjeta Material no se ve"; return false; }
            if (p->propMatChk[1]->value != &m->transparent) { err = "recprops: la tarjeta Material no edita '" + v + "'"; return false; }
        }
        else if (k == "preview") {
            const bool hay = p->propRecPreview && p->propRecPreview->Visible();
            if (hay != (v == "1")) { err = "recprops: vista previa " + std::string(hay ? "si" : "no"); return false; }
        }
        else if (k == "nombre") { if (p->propRecNombre->field.text != v) { err = "recprops: Name = '" + p->propRecNombre->field.text + "'"; return false; } }
        else if (k == "carpeta") { if (p->propRecCarpeta->field.text != Guion(v)) { err = "recprops: Folder = '" + p->propRecCarpeta->field.text + "'"; return false; } }
        else { err = "recprops: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
static bool CmdRecEditar(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "receditar: sin panel de propiedades"; return false; }
    std::string que, t; ss >> que; std::getline(ss, t);
    size_t a = t.find_first_not_of(' ');
    t = (a == std::string::npos) ? std::string() : t.substr(a);
    PropText* pt = (que == "nombre") ? p->propRecNombre : (que == "carpeta") ? p->propRecCarpeta : NULL;
    if (!pt) { err = "receditar: nombre|carpeta"; return false; }
    PropsActivo = p;
    TextFieldEnfocar(&pt->field);     // el click en el campo lo enfoca
    p->RefreshTargetProperties();     // (el panel captura el destino al enfocar)
    pt->field.SetText(Guion(t));
    g_textFieldActivo = NULL;         // Enter / click afuera: pierde el foco
    p->RefreshTargetProperties();     // ...y confirma
    printf("      [receditar] %s = '%s'\n", que.c_str(), Guion(t).c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  EL MODELO (sin UI)
// ---------------------------------------------------------------------------
static bool CmdRecursos(std::istringstream& ss, std::string& err) {
    std::string c; ss >> c;
    const int v = VistaDe(c, err, "recursos"); if (v < 0) return false;
    std::vector<W3dRecursoItem> its; W3dVistaRecListar(v, its);
    for (size_t i = 0; i < its.size(); i++)
        printf("      [recursos] %s '%s' carpeta='%s' usuarios=%d entrada='%s' %s\n", c.c_str(), its[i].nombre.c_str(),
               its[i].carpeta.c_str(), its[i].usuarios, its[i].entrada.c_str(), its[i].info.c_str());
    std::string k; int n = -1;
    if ((ss >> k) && k == "n" && (ss >> n) && n != (int)its.size()) {
        err = "recursos: " + Entero((long)its.size()) + " items, se esperaban " + Entero(n); return false;
    }
    return true;
}
static bool CmdRecurso(std::istringstream& ss, std::string& err) {
    std::string c, id; ss >> c >> id;
    const int v = VistaDe(c, err, "recurso"); if (v < 0) return false;
    id = IdDeVista(v, id);
    W3dRecursoItem it;
    const bool hay = W3dVistaRecInfo(v, id, &it);
    printf("      [recurso] %s '%s' %s carpeta='%s' usuarios=%d nombre='%s'\n", c.c_str(), id.c_str(),
           hay ? "existe" : "NO existe", it.carpeta.c_str(), it.usuarios, it.nombre.c_str());
    std::string k;
    while (ss >> k) {
        std::string val; ss >> val;
        bool ok = true;
        if (k == "existe") ok = (hay == (val == "1"));
        else if (!hay) { err = "recurso: '" + id + "' no existe"; return false; }
        else if (k == "carpeta") ok = (it.carpeta == Guion(val));
        else if (k == "usuarios") ok = (it.usuarios == atoi(val.c_str()));
        else if (k == "nombre") ok = (it.nombre == val);
        else { err = "recurso: no entiendo '" + k + "'"; return false; }
        if (!ok) { err = "recurso: '" + id + "' no cumple " + k + " " + val; return false; }
    }
    return true;
}
// carpetas [<tipo>] [n N] [tiene X]... [notiene X]...: el ARBOL UNICO de carpetas de la biblioteca (el tipo, si
// viene, se ignora: era de la fase anterior, con un arbol por tipo)
static bool CmdCarpetas(std::istringstream& ss, std::string& err) {
    std::string resto; std::getline(ss, resto);
    std::istringstream s2(resto);
    std::string c; s2 >> c;
    if (W3dVistaDeClave(c) <= W3D_VISTA_ESCENA) { s2.clear(); s2.str(resto); }
    std::vector<std::string> cs; W3dCarpetasTodas(cs);
    printf("      [carpetas]");
    for (size_t i = 0; i < cs.size(); i++) printf(" '%s'", cs[i].c_str());
    printf("  (creadas %d)\n", (int)W3dCarpetasCreadas().size());
    std::string k;
    while (s2 >> k) {
        std::string val; s2 >> val;
        bool hay = false;
        for (size_t i = 0; i < cs.size(); i++) if (cs[i] == val) hay = true;
        if (k == "n") { if ((int)cs.size() != atoi(val.c_str())) { err = "carpetas: " + Entero((long)cs.size()) + " carpetas, se esperaban " + val; return false; } }
        else if (k == "tiene") { if (!hay) { err = "carpetas: no esta '" + val + "'"; return false; } }
        else if (k == "notiene") { if (hay) { err = "carpetas: esta '" + val + "'"; return false; } }
        else { err = "carpetas: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
static bool CmdCarpetaNueva(std::istringstream& ss, std::string& err) {
    const std::string padre = SinClaveDeTipo(ss);
    std::string nombre; ss >> nombre;
    std::string ruta;
    if (!W3dCarpetaNueva(Guion(padre), nombre, &ruta)) { err = "carpetanueva: no se pudo"; return false; }
    printf("      [carpetanueva] '%s'\n", ruta.c_str());
    return true;
}
static bool CmdCarpetaRenombrar(std::istringstream& ss, std::string& err) {
    const std::string ruta = SinClaveDeTipo(ss);
    std::string nombre; ss >> nombre;
    std::string nueva;
    if (!W3dCarpetaRenombrar(ruta, nombre, &nueva)) { err = "carpetarenombrar: no se pudo"; return false; }
    printf("      [carpetarenombrar] '%s' -> '%s'\n", ruta.c_str(), nueva.c_str());
    return true;
}
static bool CmdCarpetaBorrar(std::istringstream& ss, std::string& err) {
    const std::string ruta = SinClaveDeTipo(ss);
    std::string f; ss >> f;
    std::string motivo;
    const bool ok = W3dCarpetaBorrar(ruta, &motivo);
    printf("      [carpetaborrar] '%s': %s %s\n", ruta.c_str(), ok ? "borrada" : "NO", motivo.c_str());
    if (ok == (f == "falla")) { err = "carpetaborrar: " + std::string(ok ? "se borro y no debia" : "no se borro: " + motivo); return false; }
    return true;
}
static bool CmdCarpetaMover(std::istringstream& ss, std::string& err) {
    const std::string ruta = SinClaveDeTipo(ss);
    std::string dest; ss >> dest;
    std::string nueva;
    if (!W3dCarpetaMover(ruta, Guion(dest), &nueva)) { err = "carpetamover: no se pudo mudar '" + ruta + "'"; return false; }
    printf("      [carpetamover] '%s' -> '%s'\n", ruta.c_str(), nueva.c_str());
    return true;
}
static bool CmdRecMover(std::istringstream& ss, std::string& err) {
    std::string c, id, car; ss >> c >> id >> car;
    const int v = VistaDe(c, err, "recmover"); if (v < 0) return false;
    id = IdDeVista(v, id);
    if (!W3dVistaRecMover(v, id, Guion(car))) { err = "recmover: no se pudo mover '" + id + "'"; return false; }
    printf("      [recmover] %s '%s' -> '%s'\n", c.c_str(), id.c_str(), Guion(car).c_str());
    return true;
}
static bool CmdRecRenombrar(std::istringstream& ss, std::string& err) {
    std::string c, id, nuevo, esp; ss >> c >> id >> nuevo >> esp;
    const int v = VistaDe(c, err, "recrenombrar"); if (v < 0) return false;
    std::string quedo;
    if (!W3dVistaRecRenombrar(v, id, nuevo, &quedo)) { err = "recrenombrar: no se pudo renombrar '" + id + "'"; return false; }
    printf("      [recrenombrar] %s '%s' -> '%s'\n", c.c_str(), id.c_str(), quedo.c_str());
    if (!esp.empty() && quedo != esp) { err = "recrenombrar: quedo '" + quedo + "', se esperaba '" + esp + "'"; return false; }
    return true;
}
static bool CmdRecBorrar(std::istringstream& ss, std::string& err) {
    std::string c, id, f; ss >> c >> id >> f;
    const int v = VistaDe(c, err, "recborrar"); if (v < 0) return false;
    id = IdDeVista(v, id);
    std::string motivo;
    const bool ok = W3dVistaRecBorrar(v, id, &motivo);
    printf("      [recborrar] %s '%s': %s %s\n", c.c_str(), id.c_str(), ok ? "borrado" : "NO", motivo.c_str());
    if (ok == (f == "falla")) { err = "recborrar: " + std::string(ok ? "se borro y no debia" : "no se borro: " + motivo); return false; }
    return true;
}
static bool CmdRecPurgar(std::istringstream& ss, std::string& err) {
    std::string c, k; int n = -1; ss >> c >> k >> n;
    const int v = VistaDe(c, err, "recpurgar"); if (v < 0) return false;
    std::vector<std::string> nombres;
    const int m = W3dVistaRecPurgar(v, &nombres);
    printf("      [recpurgar] %s: %d purgado(s):", c.c_str(), m);
    for (size_t i = 0; i < nombres.size(); i++) printf(" '%s'", nombres[i].c_str());
    printf("\n");
    if (k == "n" && n != m) { err = "recpurgar: purgo " + Entero(m) + ", se esperaban " + Entero(n); return false; }
    return true;
}

// ---------------------------------------------------------------------------
//  ARMAR ESCENAS DE PRUEBA
// ---------------------------------------------------------------------------
static bool CmdMatSuelto(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    if (n.empty()) { err = "matsuelto: falta el nombre"; return false; }
    Material* m = new Material(n);
    printf("      [matsuelto] '%s'\n", m->name.c_str());
    return true;
}
static bool CmdTexAsignar(std::istringstream& ss, std::string& err) {
    std::string on, ruta; ss >> on >> ruta;
    Object* o = SceneCollection ? FindObjectByName(SceneCollection, on) : NULL;
    if (!o || o->getType() != ObjectType::mesh) { err = "texasignar: no hay una malla '" + on + "'"; return false; }
    Mesh* m = (Mesh*)o;
    if (m->materialsGroup.empty() || !m->materialsGroup[0].material || m->materialsGroup[0].material == MaterialDefecto) {
        err = "texasignar: la parte 0 de '" + on + "' no tiene un material propio"; return false;
    }
    Texture* t = TexturaTomar(ruta);
    if (!t) { err = "texasignar: no pude cargar '" + ruta + "'"; return false; }
    m->materialsGroup[0].material->texture = t;
    m->materialsGroup[0].material->textureOn = true;
    printf("      [texasignar] '%s' <- '%s' (%dx%d)\n", m->materialsGroup[0].material->name.c_str(), t->path.c_str(), t->ancho, t->alto);
    return true;
}
static bool CmdTexEntrada(std::istringstream& ss, std::string& err) {
    std::string ruta; ss >> ruta;
    const std::string e = W3dImportarAsset(ruta);
    W3dRecursosVistaInvalidar();
    printf("      [texentrada] '%s' -> '%s'\n", ruta.c_str(), e.c_str());
    if (e.empty() || e == ruta) { err = "texentrada: no entro al contenedor (hay uno montado?)"; return false; }
    return true;
}
static bool CmdJsonTiene(std::istringstream& ss, std::string& err) {
    std::string ruta, texto, neg; ss >> ruta >> texto >> neg;
    W3dZipLector z;
    std::vector<unsigned char> d;
    if (!z.Abrir(ruta) || !z.Leer("proyecto.json", d)) { err = "jsontiene: no pude leer el proyecto.json de '" + ruta + "'"; return false; }
    const std::string s(d.empty() ? "" : std::string((const char*)&d[0], d.size()));
    const bool hay = s.find(texto) != std::string::npos;
    printf("      [jsontiene] '%s' %s '%s'\n", ruta.c_str(), hay ? "TIENE" : "NO tiene", texto.c_str());
    if (hay == (neg == "0")) { err = "jsontiene: " + std::string(hay ? "tiene" : "no tiene") + " '" + texto + "'"; return false; }
    return true;
}

static bool CmdSeleccion(std::istringstream& ss, std::string& err) {
    printf("      [seleccion] %d:", (int)ObjSelects.size());
    for (size_t i = 0; i < ObjSelects.size(); i++) printf(" '%s'", ObjSelects[i] ? ObjSelects[i]->name.c_str() : "?");
    printf("%s%s\n", ObjActivo ? "  activo=" : "", ObjActivo ? ObjActivo->name.c_str() : "");
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "n") { if ((int)ObjSelects.size() != atoi(v.c_str())) { err = "seleccion: hay " + Entero((long)ObjSelects.size()) + ", se esperaban " + v; return false; } }
        else if (k == "tiene") {
            bool hay = false;
            for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i] && ObjSelects[i]->name == v) hay = true;
            if (!hay) { err = "seleccion: '" + v + "' no esta seleccionado"; return false; }
        }
        else if (k == "notiene") {
            for (size_t i = 0; i < ObjSelects.size(); i++)
                if (ObjSelects[i] && ObjSelects[i]->name == v) { err = "seleccion: '" + v + "' esta seleccionado"; return false; }
        }
        else if (k == "modo") {   // objeto | edit: el modo de interaccion (un OK no puede cambiarlo solo)
            const bool esObj = (InteractionMode == ObjectMode);
            if ((v == "objeto") != esObj) { err = std::string("seleccion: el modo es ") + (esObj ? "objeto" : "otro"); return false; }
        }
        else { err = "seleccion: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// ---- el PROVEEDOR DE PRUEBA (una vista sin datos todavia: prefabs / escenas / librerias) ----
struct ItemPrueba { std::string nombre, carpeta; int usuarios; bool soloLectura; };
class ProveedorPrueba : public W3dProveedorRecursos {
public:
    std::vector<ItemPrueba> items;
    int icono;
    ProveedorPrueba() : icono(-1) {}
    void Reiniciar(int ic) {
        icono = ic;
        items.clear();
        ItemPrueba a; a.nombre = "Alfa"; a.usuarios = 2; a.soloLectura = false; items.push_back(a);
        ItemPrueba b; b.nombre = "Beta"; b.usuarios = 0; b.soloLectura = false; items.push_back(b);
        ItemPrueba g; g.nombre = "Gamma"; g.usuarios = 1; g.soloLectura = true; g.carpeta = "Externo"; items.push_back(g);
    }
    int Buscar(const std::string& id) const {
        for (size_t i = 0; i < items.size(); i++) if (items[i].nombre == id) return (int)i;
        return -1;
    }
    void Listar(std::vector<W3dRecursoItem>& out) {
        out.clear();
        for (size_t i = 0; i < items.size(); i++) {
            W3dRecursoItem it;
            it.id = it.nombre = items[i].nombre;
            it.carpeta = items[i].carpeta;
            it.icono = icono;
            it.usuarios = items[i].usuarios;
            it.soloLectura = items[i].soloLectura;
            it.info = "test provider";
            out.push_back(it);
        }
    }
    bool FijarCarpeta(const std::string& id, const std::string& carpeta) {
        const int i = Buscar(id); if (i < 0) return false;
        items[(size_t)i].carpeta = carpeta; return true;
    }
    bool Renombrar(const std::string& id, const std::string& nuevo, std::string* final) {
        const int i = Buscar(id); if (i < 0) return false;
        std::vector<std::string> otros;
        for (size_t k = 0; k < items.size(); k++) if ((int)k != i) otros.push_back(items[k].nombre);
        items[(size_t)i].nombre = W3dNombreUnicoEnValores(nuevo, "Prefab", otros, -1);
        if (final) *final = items[(size_t)i].nombre;
        return true;
    }
    bool Borrar(const std::string& id, std::string* motivo) {
        const int i = Buscar(id);
        if (i < 0) { if (motivo) *motivo = "It doesn't exist anymore"; return false; }
        items.erase(items.begin() + (long)i); return true;
    }
};
static ProveedorPrueba gProvPrueba;
static bool CmdRecProvPrueba(std::istringstream& ss, std::string& err) {
    std::string c, onoff; ss >> c >> onoff;
    const int v = VistaDe(c, err, "recprovprueba"); if (v < 0) return false;
    if (v != W3D_VISTA_PREFABS && v != W3D_VISTA_ESCENAS && v != W3D_VISTA_LIBRERIAS) {
        err = "recprovprueba: solo en una vista sin datos (prefabs|escenas|librerias)"; return false;
    }
    // (el de prueba PISA al que la vista tenga -escenas y prefabs ya tienen uno real- y 'off' lo devuelve)
    static W3dProveedorRecursos* previo[W3D_VISTAS] = { 0 };
    if (onoff == "on") {
        if (W3dRecursosVistaProveedor(v) != &gProvPrueba) previo[v] = W3dRecursosVistaProveedor(v);
        gProvPrueba.Reiniciar(W3dVistaIcono(v)); W3dRecursosVistaRegistrar(v, &gProvPrueba);
    }
    else W3dRecursosVistaRegistrar(v, previo[v]);
    // la biblioteca se re-arma sola (el filtro que se miraba sigue)
    Outliner* o = (Outliner*)BuscarHoja(rootViewport, 2);
    if (o && o->EnBiblioteca()) o->CambiarFiltro(o->filtro);
    printf("      [recprovprueba] %s %s\n", c.c_str(), onoff.c_str());
    return true;
}

// ---------------------------------------------------------------------------
//  BORDES (los arreglos de la revision de la fase: prueba_outliner_bordes.w3s)
// ---------------------------------------------------------------------------
// una entrada del contenedor montado con un nombre EXACTO (el overlay de editadas, como un .lua que
// guardo el IDE): 'entradacruda' con los bytes de un archivo, 'entradatexto' con el resto de la linea
static bool CmdEntradaCruda(std::istringstream& ss, std::string& err, bool texto) {
    std::string e, resto; ss >> e; std::getline(ss, resto);
    size_t a = resto.find_first_not_of(' ');
    resto = (a == std::string::npos) ? std::string() : resto.substr(a);
    std::vector<unsigned char> d;
    if (texto) d.assign(resto.begin(), resto.end());
    else if (!w3dFileSystem::ReadFileBytes(resto, d) || d.empty()) { err = "entradacruda: no pude leer '" + resto + "'"; return false; }
    if (d.empty() || !W3dContenedorEscribirEntrada(e, &d[0], d.size())) {
        err = "entradacruda: no se pudo escribir '" + e + "' (hay un contenedor montado? es un nombre de entrada?)"; return false;
    }
    W3dRecursosVistaInvalidar();   // el listado de entradas (y el texto de los scripts) cambio
    printf("      [%s] '%s' (%u bytes)\n", texto ? "entradatexto" : "entradacruda", e.c_str(), (unsigned)d.size());
    return true;
}
// el color del FONDO (la franja) de la fila, en la columna de la flechita (vacia en un recurso)
static bool ColorFondoFila(Outliner* o, int i, const OutFilaRec& f, unsigned char* rgb) {
    YDeFila(o, i);
    if (!rootViewport) return false;
    rootViewport->Render();
    w3dEngine::Finish();
    const int px = o->x + marginGS + o->PosX + f.prof * (IconSizeGS + gapGS) + IconSizeGS / 2;
    const int py = o->FilaRecursoY(i);
    unsigned char c[4] = { 0, 0, 0, 0 };
    w3dEngine::ReadPixelsRGBA(px, W3dPantallaAlto - py - 1, 1, 1, c);
    rgb[0] = c[0]; rgb[1] = c[1]; rgb[2] = c[2];
    return true;
}
static bool ColorCerca(const unsigned char* a, const GLubyte* b) {
    for (int k = 0; k < 3; k++) { int d = (int)a[k] - (int)b[k]; if (d < 0) d = -d; if (d > 3) return false; }
    return true;
}
// outfondo <nombre> sel|nosel: el fondo de la fila es (o no) el de la SELECCION MULTIPLE (dibujando
// la UI entera, con la lista scrolleada hasta la fila como la dejaria un click)
static bool CmdOutFondo(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outfondo"); if (!o) return false;
    std::string n, esp; ss >> n >> esp;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0) { err = "outfondo: no hay una fila '" + n + "'"; return false; }
    unsigned char c[3];
    if (!ColorFondoFila(o, i, filas[(size_t)i], c)) { err = "outfondo: sin layout"; return false; }
    const GLubyte* a = ListaColoresUbyte[static_cast<int>(ColorID::accentDark)];
    const GLubyte* b = ListaColoresUbyte[static_cast<int>(ColorID::background)];
    const GLubyte sel[3] = { (GLubyte)((a[0] * 3 + b[0] * 2) / 5), (GLubyte)((a[1] * 3 + b[1] * 2) / 5), (GLubyte)((a[2] * 3 + b[2] * 2) / 5) };
    const bool esSel = ColorCerca(c, sel);
    printf("      [outfondo] '%s' fila %d (PosY %d): rgb %d %d %d -> %s\n", n.c_str(), i, o->PosY, c[0], c[1], c[2],
           esSel ? "SELECCION" : "otro");
    if ((esp == "sel") != esSel) { err = "outfondo: el fondo de '" + n + "' " + (esSel ? "es" : "no es") + " el de la seleccion"; return false; }
    return true;
}
// outdragsobre <nombre> <sobre> <resaltada|->: arrastra la fila hasta 'sobre' SIN soltar ahi: la fila
// resaltada como DESTINO (la carpeta a donde iria, '-' = ninguna: la raiz) y su fondo dibujado; despues
// se suelta afuera del outliner (no mueve nada)
static bool CmdOutDragSobre(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outdragsobre"); if (!o) return false;
    std::string n, d, esp; ss >> n >> d >> esp;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n), j = FilaDe(filas, d);
    if (i < 0 || j < 0) { err = "outdragsobre: no hay una fila '" + (i < 0 ? n : d) + "'"; return false; }
    const int x0 = XNombre(o, filas[(size_t)i]), y0 = YDeFila(o, i);
    leftMouseDown = true;
    LayoutClickUI(x0, y0);
    const int y1 = YDeFila(o, j), x1 = XNombre(o, filas[(size_t)j]);
    o->event_mouse_motion(x0, y0 + (y1 > y0 ? 1 : -1) * (int)RenglonHeightGS);
    o->event_mouse_motion(x1, y1);
    const int df = o->recDropFila;
    const std::string res = (df >= 0 && df < (int)filas.size()) ? filas[(size_t)df].clave : std::string("-");
    bool pintada = true;
    unsigned char c[4] = { 0, 0, 0, 0 };
    if (df >= 0 && df < (int)filas.size()) {
        YDeFila(o, df);   // (la carpeta destino a la vista: puede estar bien arriba de lo apuntado)
        rootViewport->Render();
        w3dEngine::Finish();
        // (la columna de estado: una CARPETA no dibuja nada ahi, se ve la franja pelada)
        const int reserva = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;
        const int px = o->x + o->width - IconSizeGS - marginGS - borderGS - reserva + IconSizeGS / 2;
        w3dEngine::ReadPixelsRGBA(px, W3dPantallaAlto - o->FilaRecursoY(df) - 1, 1, 1, c);
        pintada = ColorCerca(c, ListaColoresUbyte[static_cast<int>(ColorID::accentDark)]);
    }
    leftMouseDown = false;
    LayoutSoltar(o->x + o->width + 50, y1);   // afuera: no mueve nada
    printf("      [outdragsobre] '%s' sobre '%s': destino resaltado '%s' (rgb %d %d %d)%s\n", n.c_str(), d.c_str(),
           res.c_str(), c[0], c[1], c[2], pintada ? "" : " SIN el fondo del drop");
    if (res != esp) { err = "outdragsobre: resalta '" + res + "', se esperaba '" + esp + "'"; return false; }
    if (!pintada) { err = "outdragsobre: la fila destino no tiene el fondo del drop"; return false; }
    return true;
}
// recboton usuarios|borrar [oculto]: el boton de la tarjeta del recurso en Properties ("Select Users" /
// "Delete"): con 'oculto' asserta que NO se ofrece; si no, lo aprieta (su accion, como el click)
// recboton usuarios|borrar|accion|parar [oculto]: un boton de la tarjeta del recurso ('accion' = el del tipo:
// Play de un sonido, Open de una escena / prefab, Open in IDE de un script; 'parar' = Stop del sonido)
static bool CmdRecBoton(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p || !p->propRecAcciones || p->propRecAcciones->botones.size() < 2) { err = "recboton: sin la tarjeta Recurso"; return false; }
    std::string q, m; ss >> q >> m;
    const bool delTipo = (q == "accion" || q == "parar");
    const int k = (q == "usuarios" || q == "accion") ? 0 : (q == "borrar" || q == "parar") ? 1 : -1;
    if (k < 0) { err = "recboton: usuarios|borrar|accion|parar"; return false; }
    PropButtonRow* fila = delTipo ? p->propRecAcciones2 : p->propRecAcciones;
    if (!fila || fila->botones.size() <= (size_t)k) { err = "recboton: sin esa fila de botones"; return false; }
    p->RefreshTargetProperties();
    p->ActualizarPestanias();
    const bool visible = fila->botones[(size_t)k]->visible && p->propRecurso && p->propRecurso->visible;
    printf("      [recboton] %s ('%s'): %s\n", q.c_str(), fila->botones[(size_t)k]->text.c_str(), visible ? "visible" : "oculto");
    if (m == "oculto") {
        if (visible) { err = "recboton: '" + q + "' se ofrece y no debia"; return false; }
        return true;
    }
    if (!visible) { err = "recboton: '" + q + "' no se ofrece"; return false; }
    PropsActivo = p;
    fila->acciones[(size_t)k]();
    return true;
}
// recdatos <clave> <id> <en|es|pt> <texto...>: los DATOS del recurso (los de la tarjeta Recurso) en ese
// idioma, exactos
static bool CmdRecDatos(std::istringstream& ss, std::string& err) {
    std::string c, id, idi, esp; ss >> c >> id >> idi; std::getline(ss, esp);
    size_t a = esp.find_first_not_of(' ');
    esp = (a == std::string::npos) ? std::string() : esp.substr(a);
    const int v = VistaDe(c, err, "recdatos"); if (v < 0) return false;
    const W3dIdioma antes = g_idioma;
    W3dIdiomaSet(W3dIdiomaDe(idi.c_str()));
    W3dRecursoItem it;
    const bool hay = W3dVistaRecInfo(v, id, &it);
    W3dIdiomaSet(antes);
    printf("      [recdatos] %s '%s' (%s): '%s'\n", c.c_str(), id.c_str(), idi.c_str(), it.info.c_str());
    if (!hay) { err = "recdatos: no existe '" + id + "'"; return false; }
    if (it.info != esp) { err = "recdatos: dice '" + it.info + "', se esperaba '" + esp + "'"; return false; }
    return true;
}
// texdormidas [asignar <objeto> <parte> <material>] [cuadros K] [n N]: los materiales con texturas
// DORMIDAS (import_obj.h). 'asignar' le pone el material a una parte SIN pasar por Properties (como lo
// deja un undo o un script); 'cuadros' corre K veces la cola diferida del loop principal (la que
// despierta las que una malla empezo a usar)
static bool CmdTexDormidas(std::istringstream& ss, std::string& err) {
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        if (k == "asignar") {
            int parte = -1; std::string mn; ss >> parte >> mn;
            Object* o = SceneCollection ? FindObjectByName(SceneCollection, v) : NULL;
            if (!o || o->getType() != ObjectType::mesh) { err = "texdormidas: no hay una malla '" + v + "'"; return false; }
            Mesh* m = (Mesh*)o;
            Material* mat = BuscarMaterialPorNombre(mn);
            if (!mat || parte < 0 || parte >= (int)m->materialsGroup.size()) { err = "texdormidas: parte o material invalido"; return false; }
            m->materialsGroup[(size_t)parte].material = mat;
        }
        else if (k == "cuadros") { const int n = atoi(v.c_str()); for (int i = 0; i < n; i++) CargarTexturasPendientes(); }
        else if (k == "n") {
            if (TexturasDormidasCantidad() != atoi(v.c_str())) {
                err = "texdormidas: hay " + Entero(TexturasDormidasCantidad()) + ", se esperaban " + v; return false;
            }
        }
        else { err = "texdormidas: no entiendo '" + k + "'"; return false; }
    }
    printf("      [texdormidas] %d material(es) con texturas dormidas, %d en la cola\n", TexturasDormidasCantidad(), TexturasPendientes());
    return true;
}


// ============================================================================
//  FASE 4b: LA BIBLIOTECA (el outliner como explorador del .w3d)
// ============================================================================
extern int OutlinerFilaDeObjeto(Object* o);   // Outliner.cpp: la fila visible de un objeto en la escena (-1)
extern void PropsDestinosRecurso(Properties* p, int tipo, std::vector<int>& rects);   // Properties.cpp
// el punto del NOMBRE de una fila de cualquiera de las dos vistas: por nombre (la escena: el del objeto)
static bool PuntoDeNombre(Outliner* o, const std::string& n, int& mx, int& my, std::string& err, const char* cmd) {
    if (o->vista == OUT_VISTA_ESCENA) {
        Object* ob = SceneCollection ? FindObjectByName(SceneCollection, n) : NULL;
        const int fila = ob ? OutlinerFilaDeObjeto(ob) : -1;
        if (fila < 0) { err = std::string(cmd) + ": no hay una fila de objeto '" + n + "'"; return false; }
        int prof = 0;
        for (Object* p = ob->Parent; p && p != SceneCollection; p = p->Parent) prof++;
        mx = o->x + marginGS + o->PosX + (prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
        my = o->y + borderGS + o->PosY + o->BarTopOffset() + fila * (int)RenglonHeightGS + (int)RenglonHeightGS / 2;
        return true;
    }
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0) { Volcar(filas, cmd); err = std::string(cmd) + ": no hay una fila '" + n + "'"; return false; }
    my = YDeFila(o, i);
    mx = XNombre(o, filas[(size_t)i]);
    return true;
}
// outdobleclick <nombre> [ojo|flecha]: DOBLE CLICK sobre el nombre (las dos vistas): el primer click elige y el
// segundo, que llega con clicks == 2 (controles.cpp), renombra en linea. 'ojo' (la vista Escena) / 'flecha' (una
// carpeta de la biblioteca) = el doble click en ESA columna de la fila, que no renombra (con 'fail')
static bool CmdOutDobleClick(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outdobleclick"); if (!o) return false;
    std::string n, donde; ss >> n >> donde;
    int mx, my;
    if (!PuntoDeNombre(o, n, mx, my, err, "outdobleclick")) return false;
    if (donde == "ojo") {
        const int reservaBarra = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;
        mx = o->x + o->width - 2 * IconSizeGS - gapGS - marginGS - borderGS - reservaBarra + IconSizeGS / 2;
    } else if (donde == "flecha") {
        std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
        const int i = FilaDe(filas, n);
        if (i < 0) { err = "outdobleclick: 'flecha' es de una fila de la biblioteca"; return false; }
        mx = XFlecha(o, filas[(size_t)i]);
    } else if (!donde.empty()) { err = "outdobleclick: no entiendo '" + donde + "'"; return false; }
    for (int k = 0; k < 2; k++) {
        leftMouseDown = true;
        LayoutClickUI(mx, my);
        if (k == 1) LayoutDobleClickUI(mx, my);
        leftMouseDown = false;
        LayoutSoltar(mx, my);
    }
    printf("      [outdobleclick] '%s' -> %s (campo '%s')\n", n.c_str(), o->renombrando ? "renombrando" : "NADA",
           o->renombre.text.c_str());
    if (!o->renombrando || g_textFieldActivo != &o->renombre) { err = "outdobleclick: el nombre no se volvio un campo"; return false; }
    return true;
}
// outtap <nombre> [doble]: un TOQUE de dedo (el click diferido de controles.cpp: g_uiTapEnCurso). No arma arrastre
// (con el dedo arrastrar es scroll). 'doble' = dos toques seguidos (el doble tap: renombrar)
static bool CmdOutTap(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outtap"); if (!o) return false;
    std::string n, d; ss >> n >> d;
    int mx, my;
    if (!PuntoDeNombre(o, n, mx, my, err, "outtap")) return false;
    extern bool g_uiTapEnCurso;
    const int veces = (d == "doble") ? 2 : 1;
    bool armo = false;
    for (int k = 0; k < veces; k++) {
        LayoutSoltar(mx, my);          // (el up llega antes que el click diferido, como en controles.cpp)
        g_uiTapEnCurso = true;
        LayoutClickUI(mx, my);
        armo = armo || o->recArrastre || o->dragObjeto;
        if (k == 1) LayoutDobleClickUI(mx, my);
        g_uiTapEnCurso = false;
        o->dragObjeto = NULL;
    }
    printf("      [outtap] '%s'%s: arrastre %s, %s\n", n.c_str(), veces == 2 ? " (doble)" : "", armo ? "ARMADO" : "no",
           o->renombrando ? "renombrando" : "sin renombrar");
    if (armo && o->vista != OUT_VISTA_ESCENA) { err = "outtap: un toque armo un arrastre (con el dedo, arrastrar es scroll)"; return false; }
    if (veces == 2 && !o->renombrando) { err = "outtap: el doble tap no renombro"; return false; }
    return true;
}
// outmantener <nombre> [soltar | mover <destino|-|3d>]: la PULSACION LARGA del dedo (LayoutPulsacionLargaUI, la que
// dispara controles.cpp a los ~0,5 s): la fila queda AGARRADA; 'soltar' = levantar sin mover (el menu contextual);
// 'mover' = arrastrarla hasta el destino (una carpeta / un objeto) y soltarla ahi; 'mover 3d' = llevar el recurso
// AFUERA, al centro del viewport 3D, y soltarlo ahi (lo de outsoltar 3d, pero con el dedo)
static bool CmdOutMantener(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outmantener"); if (!o) return false;
    std::string n, que, dest; ss >> n >> que >> dest;
    int mx, my;
    if (!PuntoDeNombre(o, n, mx, my, err, "outmantener")) return false;
    if (MenuAbierto) MenuAbierto->Cerrar();
    if (!LayoutPulsacionLargaUI(mx, my)) { err = "outmantener: la pulsacion larga no agarro la fila"; return false; }
    printf("      [outmantener] '%s' agarrada (fila %d)\n", n.c_str(), o->agarreFila);
    if (o->agarreFila < 0) { err = "outmantener: no quedo agarrada (sin la marca)"; return false; }
    if (que == "mover") {
        int x1 = mx, y1 = my;
        const bool al3d = (dest == "3d");
        if (al3d) {
            ViewportBase* v = BuscarHoja(rootViewport, 1);
            if (!v) { err = "outmantener: no hay viewport 3D"; return false; }
            rootViewport->Render();   // (el 3D sabe su lente)
            x1 = v->x + v->width / 2; y1 = v->y + v->height / 2;
        } else if (dest == "-") y1 = o->y + o->height - (int)RenglonHeightGS;
        else if (!PuntoDeNombre(o, dest, x1, y1, err, "outmantener")) return false;
        // (sin el boton del mouse: es el dedo; controles.cpp manda el motion al outliner mientras dure el agarre)
        o->event_mouse_motion(mx, my + (y1 > my ? 1 : -1) * (int)RenglonHeightGS);
        o->event_mouse_motion(x1, y1);
        const bool afuera = g_outArrastre.activo;
        const int destino = g_outArrastre.destino;
        LayoutSoltar(x1, y1);
        printf("      [outmantener] '%s' soltada sobre '%s'%s\n", n.c_str(), dest.c_str(),
               al3d ? (afuera ? (destino > 0 ? " (arrastre afuera, destino valido)" : " (arrastre afuera, destino NO valido)")
                              : " (el arrastre afuera NO arranco)") : "");
        if (al3d && (!afuera || destino <= 0)) { err = "outmantener: el arrastre al 3D no llego a un destino valido"; return false; }
        return true;
    }
    if (que == "soltar") {
        LayoutSoltar(mx, my);
        const bool menu = MenuAbierto && MenuAbierto->abierto;
        printf("      [outmantener] soltada sin mover: menu contextual %s\n", menu ? "ABIERTO" : "no");
        if (!menu) { err = "outmantener: soltar sin mover no abrio el menu contextual"; return false; }
        if (MenuAbierto) MenuAbierto->Cerrar();
    }
    return true;
}
// outfiltro <todo|tipo>: el FILTRO de la biblioteca por su menu
static bool CmdOutFiltro(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outfiltro"); if (!o) return false;
    std::string c; ss >> c;
    const int f = (c == "todo") ? 0 : W3dVistaDeClave(c);
    if (f < 0) { err = "outfiltro: tipo desconocido '" + c + "'"; return false; }
    if (!o->EnBiblioteca()) { err = "outfiltro: el outliner no esta en la biblioteca"; return false; }
    if (!ElegirEnMenuBarra(o, o->btnFiltro, f, err, "outfiltro")) return false;
    printf("      [outfiltro] %s (boton '%s')\n", c.c_str(), o->btnFiltro->text.c_str());
    return true;
}
// outcuadricula 0|1: el interruptor lista / cuadricula de la barra (un click de verdad)
static bool CmdOutCuadricula(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outcuadricula"); if (!o) return false;
    std::string v; ss >> v;
    if (o->cuadricula != (v == "1")) {
        Button* b = o->btnCuadricula;
        if (!b || !b->visible) { err = "outcuadricula: el boton no se ve (en la biblioteca?)"; return false; }
        // (como ClickBotonBarra: sin el foco de teclado de la barra y con el boton a la vista)
        if (!MenuAbierto || !MenuAbierto->abierto) o->barFocusIndex = -1;
        o->barScrollManual = 0;
        o->ActualizarBarra();
        const int sobra = (b->sx + b->width) - (o->x + o->width) + 4;
        if (sobra > 0) { o->barScrollManual = sobra; o->ActualizarBarra(); }
        const int bx = b->sx + b->width / 2, by = b->sy + b->height / 2;
        LayoutClickUI(bx, by);
        LayoutSoltar(bx, by);
    }
    printf("      [outcuadricula] %s\n", o->cuadricula ? "cuadricula" : "lista");
    if (o->cuadricula != (v == "1")) { err = "outcuadricula: no cambio"; return false; }
    return true;
}
// outmini <nombre> [si|no] [celda] [colorvivo] [vacia] [centro] [png <ruta>]: la MINIATURA de la celda (se
// dibuja la cuadricula hasta que este: son perezosas). 'si' = tiene una (la textura GL); 'colorvivo' = sus
// pixeles no son todos del mismo color; 'vacia' = no se dibujo nada (todo transparente: la camara no ve
// nada); 'centro' = el pixel del medio esta pintado (lo que la camara mira de frente). Sin 'celda' se miran
// los pixeles generados en el momento; con 'celda', los DIBUJADOS en la celda (la textura del CACHE, la que
// ve el usuario: prueba que se rehizo): pintado = distinto del fondo de la celda. 'png' los vuelca
static bool CmdOutMini(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outmini"); if (!o) return false;
    std::string n, esp, k, png; ss >> n >> esp;
    std::vector<std::string> asserts;
    bool enCelda = false;
    while (ss >> k) {
        if (k == "png") ss >> png;
        else if (k == "celda") enCelda = true;
        else if (k == "colorvivo" || k == "vacia" || k == "centro") asserts.push_back(k);
        else { err = "outmini: no entiendo '" + k + "'"; return false; }
    }
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0 || filas[(size_t)i].carpeta) { err = "outmini: no hay un recurso '" + n + "'"; return false; }
    YDeFila(o, i);
    unsigned int tex = 0;
    for (int intento = 0; intento < 8 && !tex; intento++) {
        rootViewport->Render();
        tex = o->MiniaturaDeFila(filas[(size_t)i], NULL, NULL);   // (la MISMA que dibuja la celda)
    }
    std::vector<unsigned char> px; int w = 0, h = 0;
    bool hay = false;
    if (enCelda) {
        if (!o->cuadricula) { err = "outmini: 'celda' necesita la cuadricula"; return false; }
        // un cuadro mas con todo en el cache (la miniatura ya generada) y se lee lo dibujado en su lugar de la
        // celda (el mismo calculo que RenderRecursos)
        rootViewport->Render();
        w3dEngine::Finish();
        int rx, ry, rw, rh; o->RectFila(filas, i, rx, ry, rw, rh);
        const int lado = o->CeldaTam() - 2 * gapGS - (int)RenglonHeightGS;
        const int x0 = o->x + rx + (rw - lado) / 2, y0 = o->y + o->PosY + ry + gapGS;
        if (lado <= 0) { err = "outmini: celda sin lugar para la miniatura"; return false; }
        std::vector<unsigned char> fb((size_t)lado * lado * 4), bg(4);
        w3dEngine::ReadPixelsRGBA(x0, W3dPantallaAlto - (y0 + lado), lado, lado, &fb[0]);
        w3dEngine::ReadPixelsRGBA(o->x + rx + GlobalScale, W3dPantallaAlto - (y0 + lado / 2) - 1, 1, 1, &bg[0]);
        // (el framebuffer viene de abajo hacia arriba: se da vuelta para que la fila 0 sea la de arriba)
        px.resize(fb.size());
        for (int y = 0; y < lado; y++)
            for (int x = 0; x < lado; x++) {
                const unsigned char* s = &fb[((size_t)(lado - 1 - y) * lado + x) * 4];
                unsigned char* d = &px[((size_t)y * lado + x) * 4];
                const int dif = abs((int)s[0] - bg[0]) + abs((int)s[1] - bg[1]) + abs((int)s[2] - bg[2]);
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = (unsigned char)(dif > 24 ? 255 : 0);
            }
        w = h = lado;
        hay = tex != 0;
    } else hay = !o->VistaSoloLectura() && W3dMiniaturaPixeles(filas[(size_t)i].tipo, filas[(size_t)i].id, px, w, h);
    int distintos = 0, opacos = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4) {
        if (px[p + 3] > 0) opacos++;
        if (px[p] != px[0] || px[p + 1] != px[1] || px[p + 2] != px[2]) distintos++;
    }
    const size_t medio = ((size_t)(h / 2) * (size_t)w + (size_t)(w / 2)) * 4 + 3;
    const bool centro = hay && medio < px.size() && px[medio] > 0;
    printf("      [outmini] '%s'%s: textura %u, pixeles %s (%dx%d, %d opacos, %d distintos del primero, centro %s)\n", n.c_str(),
           enCelda ? " (la celda)" : "", tex, hay ? "si" : "no", w, h, opacos, distintos, centro ? "pintado" : "vacio");
    if (!png.empty() && hay && !px.empty() && !w3dEngine::SavePNG(png.c_str(), &px[0], w, h, false)) { err = "outmini: no pude escribir " + png; return false; }
    if (esp == "si" && !tex) { err = "outmini: '" + n + "' no tiene miniatura"; return false; }
    if (esp == "no" && tex) { err = "outmini: '" + n + "' tiene miniatura y no debia"; return false; }
    for (size_t a = 0; a < asserts.size(); a++) {
        if (asserts[a] == "colorvivo" && (opacos == 0 || distintos == 0)) { err = "outmini: la miniatura de '" + n + "' esta vacia o es de un solo color"; return false; }
        if (asserts[a] == "vacia" && (!hay || opacos > 0)) { err = "outmini: la miniatura de '" + n + "' tiene algo dibujado (o no hay)"; return false; }
        if (asserts[a] == "centro" && !centro) { err = "outmini: el medio de la miniatura de '" + n + "' esta vacio"; return false; }
    }
    return true;
}
// idebib ensuciar | abrir <ruta> | archivo <ruta> | sucio 0|1 | tiene <texto> | notiene <texto>
//   el PRIMER IDE del layout (vpkind lo pone) y el "Open in IDE" de la biblioteca (IDEAbrirScript, lo que llama
//   el boton de Properties): 'ensuciar' tipea una linea sin guardar; 'abrir' aprieta el boton con ese script
static IDE* PrimerIDEDelLayout(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == 8 ? (IDE*)n : NULL;
    IDE* a = PrimerIDEDelLayout(((ViewportColumn*)n)->childA);
    return a ? a : PrimerIDEDelLayout(((ViewportColumn*)n)->childB);
}
static bool CmdIdeBib(std::istringstream& ss, std::string& err) {
    IDE* ide = PrimerIDEDelLayout(rootViewport);
    if (!ide) { err = "idebib: no hay un IDE en el layout (vpkind 5 7)"; return false; }
    std::string sub, arg; ss >> sub;
    std::getline(ss, arg);
    const size_t a = arg.find_first_not_of(' ');
    arg = (a == std::string::npos) ? std::string() : arg.substr(a);
    if (sub == "ensuciar") {
        ide->CursorA(0, 0, false);
        ide->InsertarTexto("-- tipeado sin guardar\n");
    } else if (sub == "abrir") {
        extern bool IDEAbrirScript(const std::string& ruta);
        if (!IDEAbrirScript(arg)) { err = "idebib: IDEAbrirScript fallo con '" + arg + "'"; return false; }
    } else if (sub == "archivo") {
        if (ide->archivo != arg) { err = "idebib: el IDE tiene '" + ide->archivo + "', se esperaba '" + arg + "'"; return false; }
    } else if (sub == "sucio") {
        if ((arg == "1") != ide->sucio) { err = std::string("idebib: el IDE ") + (ide->sucio ? "tiene" : "no tiene") + " cambios sin guardar"; return false; }
    } else if (sub == "tiene" || sub == "notiene") {
        const bool hay = ide->GetTexto().find(arg) != std::string::npos;
        if (hay != (sub == "tiene")) { err = "idebib: el texto del IDE " + std::string(hay ? "tiene" : "no tiene") + " '" + arg + "'"; return false; }
    } else { err = "idebib: no entiendo '" + sub + "'"; return false; }
    printf("      [idebib] %s: archivo '%s', sucio %d\n", sub.c_str(), ide->archivo.c_str(), ide->sucio ? 1 : 0);
    return true;
}
// bibmedir [veces]: cuanto cuesta listar la biblioteca (cada tipo, todo junto), armar las filas del outliner y
// listar lo no guardado, en ms por llamada (reloj de pared). Solo informa: la biblioteca y el seguimiento de
// cambios corren en cada cuadro, asi que con un proyecto grande esto es lo que se paga por cuadro
static bool CmdBibMedir(std::istringstream& ss, std::string& err) {
    int veces = 10; ss >> veces;
    if (veces < 1) veces = 1;
    std::vector<int> tipos; W3dBibliotecaTipos(tipos);
    std::vector<W3dRecursoItem> its;
    for (size_t t = 0; t < tipos.size(); t++) {
        const double t0 = W3dNowMs();
        for (int k = 0; k < veces; k++) W3dVistaRecListar(tipos[t], its);
        printf("      [bibmedir] %-12s %4d items  %7.2f ms\n", W3dVistaClave(tipos[t]), (int)its.size(), (W3dNowMs() - t0) / veces);
    }
    double t0 = W3dNowMs();
    for (int k = 0; k < veces; k++) W3dBibliotecaListar(-1, its);
    printf("      [bibmedir] biblioteca   %4d items  %7.2f ms\n", (int)its.size(), (W3dNowMs() - t0) / veces);
    Outliner* o = ElOutliner(err, "bibmedir");
    if (o && o->EnBiblioteca()) {
        std::vector<OutFilaRec> filas;
        t0 = W3dNowMs();
        for (int k = 0; k < veces; k++) o->FilasRecursos(filas);
        printf("      [bibmedir] filas        %4d filas  %7.2f ms\n", (int)filas.size(), (W3dNowMs() - t0) / veces);
    }
    err.clear();
    std::vector<W3dCambio> cs;
    t0 = W3dNowMs();
    for (int k = 0; k < veces; k++) W3dCambiosListar(cs);
    printf("      [bibmedir] cambios      %4d        %7.2f ms\n", (int)cs.size(), (W3dNowMs() - t0) / veces);
    return true;
}
// minidistintas <tipo> <idA> <idB>: las miniaturas de dos recursos NO son iguales pixel a pixel (dos materiales
// blancos con texturas distintas no pueden dar la misma esfera)
static bool CmdMiniDistintas(std::istringstream& ss, std::string& err) {
    std::string c, a, b; ss >> c >> a >> b;
    const int t = VistaDe(c, err, "minidistintas"); if (t < 0) return false;
    std::vector<unsigned char> pa, pb; int wa = 0, ha = 0, wb = 0, hb = 0;
    if (!W3dMiniaturaPixeles(t, IdDeVista(t, a), pa, wa, ha) || !W3dMiniaturaPixeles(t, IdDeVista(t, b), pb, wb, hb)) {
        err = "minidistintas: '" + a + "' o '" + b + "' no tiene miniatura"; return false;
    }
    int dif = 0;
    for (size_t k = 0; k < pa.size() && k < pb.size(); k++) if (pa[k] != pb[k]) dif++;
    printf("      [minidistintas] '%s' (%dx%d) vs '%s' (%dx%d): %d bytes distintos\n", a.c_str(), wa, ha, b.c_str(), wb, hb, dif);
    if (pa == pb) { err = "minidistintas: las miniaturas de '" + a + "' y '" + b + "' son identicas"; return false; }
    return true;
}
// recexterno <carpeta|ruta>: "External (choose where)..." de la tarjeta del recurso ACTIVO, con esa eleccion del
// explorador (relativa a tools/pruebas). Por la misma funcion que llama el explorador al aceptar.
static bool CmdRecExterno(std::istringstream& ss, std::string& err) {
    std::string ruta; ss >> ruta;
    if (ruta.empty()) { err = "recexterno: falta la carpeta"; return false; }
    if (ruta[0] != '/') { char b[4096]; if (getcwd(b, sizeof(b))) ruta = std::string(b) + "/" + ruta; }
    int v = -1; std::string id;
    if (!W3dRecursoActivo(&v, &id)) { err = "recexterno: no hay un recurso activo"; return false; }
    extern void PropsRecExternoPreparar();
    extern void PropsRecExternoElegido(const std::string& elegido);
    PropsRecExternoPreparar();
    PropsRecExternoElegido(ruta);
    std::string ahora; int va = -1;
    W3dRecursoActivo(&va, &ahora);
    printf("      [recexterno] '%s' -> %s: ahora '%s' (%s)\n", id.c_str(), ruta.c_str(), ahora.c_str(),
           W3dVistaRecUbicacion(v, ahora) == 1 ? "fuera" : "dentro");
    return true;
}
// archivomover <de> <a>: mueve (renombra) un archivo del DISCO, por fuera de Whisk3D (como el usuario que
// mueve o borra un archivo externo del proyecto con el explorador). Rutas relativas a tools/pruebas
static bool CmdArchivoMover(std::istringstream& ss, std::string& err) {
    std::string de, a; ss >> de >> a;
    if (de.empty() || a.empty()) { err = "archivomover: uso: archivomover <de> <a>"; return false; }
    if (de == a) { err = "archivomover: origen y destino son el mismo"; return false; }
    remove(a.c_str());   // (Windows no pisa al renombrar)
    const bool ok = rename(de.c_str(), a.c_str()) == 0;
    printf("      [archivomover] '%s' -> '%s': %s\n", de.c_str(), a.c_str(), ok ? "movido" : "NO");
    if (!ok) { err = "archivomover: no pude mover '" + de + "'"; return false; }
    return true;
}
// outlinea <nombre> <nivel> [si|no]: la LINEA GUIA de la jerarquia a la izquierda de la fila (la columna de la
// carpeta de ese nivel), dibujada de verdad: el pixel de la linea contra el del fondo de al lado
static bool CmdOutLinea(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outlinea"); if (!o) return false;
    std::string n, esp; int nivel = 0; ss >> n >> nivel >> esp;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0) { err = "outlinea: no hay una fila '" + n + "'"; return false; }
    const int cy = YDeFila(o, i);
    rootViewport->Render();
    w3dEngine::Finish();
    // la columna de ese nivel (el ancho de un icono): la linea es la franja mas clara que el fondo
    const int x0 = o->x + marginGS + o->PosX + nivel * (IconSizeGS + gapGS);
    const int ancho = IconSizeGS > 0 ? IconSizeGS : 1;
    std::vector<unsigned char> px((size_t)ancho * 4);
    w3dEngine::ReadPixelsRGBA(x0, W3dPantallaAlto - cy - 1, ancho, 1, &px[0]);
    int imax = 0, imin = 0;
    for (int k = 1; k < ancho; k++) {
        const int v = px[(size_t)k * 4] + px[(size_t)k * 4 + 1] + px[(size_t)k * 4 + 2];
        if (v > px[(size_t)imax * 4] + px[(size_t)imax * 4 + 1] + px[(size_t)imax * 4 + 2]) imax = k;
        if (v < px[(size_t)imin * 4] + px[(size_t)imin * 4 + 1] + px[(size_t)imin * 4 + 2]) imin = k;
    }
    const unsigned char* a = &px[(size_t)imax * 4];
    const unsigned char* b = &px[(size_t)imin * 4];
    const int d = (int)a[0] + a[1] + a[2] - ((int)b[0] + b[1] + b[2]);
    const bool hay = d > 45;
    printf("      [outlinea] '%s' nivel %d: linea rgb %d %d %d, fondo %d %d %d -> %s\n", n.c_str(), nivel, a[0], a[1], a[2],
           b[0], b[1], b[2], hay ? "LINEA" : "sin linea");
    if ((esp != "no") != hay) { err = "outlinea: la linea guia de '" + n + "' " + (hay ? "esta" : "no esta"); return false; }
    return true;
}
// outcolor <nombre> activo|seleccion|normal: el color del NOMBRE dibujado (la fila activa en verde, la
// seleccionada en verde oscuro): se cuentan los pixeles del texto de ese color
static bool CmdOutColor(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outcolor"); if (!o) return false;
    std::string n, esp; ss >> n >> esp;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0) { err = "outcolor: no hay una fila '" + n + "'"; return false; }
    const int cy = YDeFila(o, i);
    rootViewport->Render();
    w3dEngine::Finish();
    const int x0 = XNombre(o, filas[(size_t)i]) - LetterWidthGS, w = (int)filas[(size_t)i].nombre.size() * LetterWidthGS;
    const int h = (int)RenglonHeightGS - 2 * GlobalScale;
    if (w <= 0) { err = "outcolor: sin texto"; return false; }
    std::vector<unsigned char> px((size_t)w * h * 4);
    w3dEngine::ReadPixelsRGBA(x0, W3dPantallaAlto - (cy + h / 2), w, h, &px[0]);
    const GLubyte* acc = ListaColoresUbyte[static_cast<int>(ColorID::accent)];
    const GLubyte* osc = ListaColoresUbyte[static_cast<int>(ColorID::accentDark)];
    int nAcc = 0, nOsc = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4) {
        if (ColorCerca(&px[p], acc)) nAcc++;
        else if (ColorCerca(&px[p], osc)) nOsc++;
    }
    printf("      [outcolor] '%s': %d px verde (activo), %d px verde oscuro (seleccion)\n", n.c_str(), nAcc, nOsc);
    const bool ok = (esp == "activo") ? (nAcc > 3) : (esp == "seleccion") ? (nOsc > 3 && nAcc <= 3) : (nAcc <= 3 && nOsc <= 3);
    if (!ok) { err = "outcolor: el nombre de '" + n + "' no se ve " + esp; return false; }
    return true;
}
// outsoltar <nombre> 3d [dx dy] | props malla|material|textura: ARRASTRA el recurso desde la biblioteca y lo suelta
// AFUERA: en el centro del viewport 3D (corrido dx,dy px) o sobre el desplegable de Properties de ese tipo (los
// movimientos y el soltar de siempre: event_mouse_motion + LayoutSoltar)
static ViewportBase* PrimeraDeTipo(ViewportBase* n, int kind) { return BuscarHoja(n, kind); }
static bool CmdOutSoltar(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outsoltar"); if (!o) return false;
    std::string n, donde, que; ss >> n >> donde;
    std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
    const int i = FilaDe(filas, n);
    if (i < 0 || filas[(size_t)i].carpeta) { err = "outsoltar: no hay un recurso '" + n + "'"; return false; }
    rootViewport->Render();   // (el 3D sabe su lente; Properties, donde estan sus botones)
    int x1 = 0, y1 = 0;
    if (donde == "3d") {
        int dx = 0, dy = 0; ss >> dx >> dy;
        ViewportBase* v = PrimeraDeTipo(rootViewport, 1);
        if (!v) { err = "outsoltar: no hay viewport 3D"; return false; }
        x1 = v->x + v->width / 2 + dx; y1 = v->y + v->height / 2 + dy;
    } else if (donde == "props") {
        ss >> que;
        Properties* p = (Properties*)PrimeraDeTipo(rootViewport, 3);
        if (!p) { err = "outsoltar: no hay panel de propiedades"; return false; }
        // (el panel al dia con la seleccion, como despues de un click de verdad: selobj no lo refresca)
        p->RefreshTargetProperties();
        p->ActualizarPestanias();
        rootViewport->Render();
        const int t = (que == "malla") ? W3D_VISTA_MALLAS : (que == "material") ? W3D_VISTA_MATERIALES
                    : (que == "prefab") ? W3D_VISTA_PREFABS : (que == "escena") ? W3D_VISTA_ESCENAS : W3D_VISTA_TEXTURAS;
        std::vector<int> r;
        PropsDestinosRecurso(p, t, r);
        // (si el desplegable quedo abajo del panel, se scrollea como lo haria el usuario)
        for (int k = 1; k <= 12 && (r.size() < 4 || r[1] + r[3] > p->y + p->height - 2); k++) {
            p->PosY = -k * 3 * (int)RenglonHeightGS;
            rootViewport->Render();
            PropsDestinosRecurso(p, t, r);
        }
        if (r.size() < 4) {
            Button* sb = p->propBtnMallaSel ? p->propBtnMallaSel->button : NULL;
            printf("      [outsoltar] panel (%d,%d %dx%d) selector %s (%d,%d %dx%d) visible %d\n", p->x, p->y, p->width, p->height,
                   sb ? "si" : "no", sb ? sb->sx : 0, sb ? sb->sy : 0, sb ? sb->width : 0, sb ? sb->height : 0,
                   sb ? (int)sb->visible : -1);
            err = "outsoltar: Properties no muestra el desplegable de '" + que + "'"; return false;
        }
        x1 = r[0] + r[2] / 2; y1 = r[1] + r[3] / 2;
        printf("      [outsoltar] el desplegable de '%s' en (%d,%d %dx%d), panel scrolleado %d\n", que.c_str(), r[0], r[1], r[2], r[3], p->PosY);
    } else { err = "outsoltar: 3d | props <malla|material|textura|prefab>"; return false; }
    const int x0 = XNombre(o, filas[(size_t)i]), y0 = YDeFila(o, i);
    leftMouseDown = true;
    LayoutClickUI(x0, y0);
    o->event_mouse_motion(x0 + (int)RenglonHeightGS * 2, y0 + (int)RenglonHeightGS);
    o->event_mouse_motion(x1, y1);
    const bool arrastraba = g_outArrastre.activo;
    const int destino = g_outArrastre.destino;
    const std::string qt = g_outArrastre.destinoTexto;
    rootViewport->Render();
    LayoutRenderMenu(MenuPantallaW, MenuPantallaH);   // (el resaltado de los destinos se dibuja sin romper nada)
    leftMouseDown = false;
    LayoutSoltar(x1, y1);
    printf("      [outsoltar] '%s' -> %s %s: arrastre %s, destino %d ('%s')\n", n.c_str(), donde.c_str(), que.c_str(),
           arrastraba ? "si" : "no", destino, qt.c_str());
    if (!arrastraba) { err = "outsoltar: el arrastre afuera del outliner no arranco"; return false; }
    std::string k;
    if ((ss >> k) && k == "invalido") { if (destino > 0) { err = "outsoltar: el destino era valido"; return false; } }
    else if (destino <= 0) { err = "outsoltar: el destino no era valido: " + qt; return false; }
    return true;
}
// libvincular <ruta.w3d> [falla] / libdesvincular <nombre> / libs [n N]: las LIBRERIAS externas (el menu de la vista)
static bool CmdLibVincular(std::istringstream& ss, std::string& err) {
    std::string r, f; ss >> r >> f;
    std::string motivo;
    std::string abs = r;
    if (!abs.empty() && abs[0] != '/') { char b[4096]; if (getcwd(b, sizeof(b))) abs = std::string(b) + "/" + r; }
    const bool ok = W3dLibreriaVincular(abs, &motivo);
    printf("      [libvincular] '%s': %s %s\n", abs.c_str(), ok ? "vinculada" : "NO", motivo.c_str());
    if (ok == (f == "falla")) { err = "libvincular: " + std::string(ok ? "se vinculo y no debia" : "no se vinculo: " + motivo); return false; }
    return true;
}
static bool CmdLibDesvincular(std::istringstream& ss, std::string& err) {
    std::string n; ss >> n;
    for (int i = 0; i < W3dLibreriasCantidad(); i++)
        if (W3dLibreriaNombre(i) == n) { W3dLibreriaDesvincular(i); printf("      [libdesvincular] '%s'\n", n.c_str()); return true; }
    err = "libdesvincular: no esta vinculada '" + n + "'";
    return false;
}
static bool CmdLibs(std::istringstream& ss, std::string& err) {
    printf("      [libs] %d:", W3dLibreriasCantidad());
    for (int i = 0; i < W3dLibreriasCantidad(); i++) printf(" '%s'", W3dLibreriaNombre(i).c_str());
    printf("\n");
    std::string k; int n = -1;
    if ((ss >> k) && k == "n" && (ss >> n) && n != W3dLibreriasCantidad()) { err = "libs: hay " + Entero(W3dLibreriasCantidad()); return false; }
    return true;
}
// cambios [n N] [tiene <texto>] [no <texto>]: la lista de lo NO GUARDADO (la del cartel)
static bool CmdCambios(std::istringstream& ss, std::string& err) {
    std::vector<W3dCambio> v; W3dCambiosListar(v);
    printf("      [cambios] %d:", (int)v.size());
    for (size_t i = 0; i < v.size(); i++) printf(" [%s: %s]", v[i].tipo.c_str(), v[i].nombre.c_str());
    printf("\n");
    std::string k;
    while (ss >> k) {
        std::string val; ss >> val;
        bool hay = false;
        for (size_t i = 0; i < v.size(); i++) if (v[i].nombre == val || v[i].nombre.find(val + " (") == 0) hay = true;
        if (k == "n") { if ((int)v.size() != atoi(val.c_str())) { err = "cambios: hay " + Entero((long)v.size()) + ", se esperaban " + val; return false; } }
        else if (k == "tiene") { if (!hay) { err = "cambios: no esta '" + val + "'"; return false; } }
        else if (k == "no") { if (hay) { err = "cambios: esta '" + val + "'"; return false; } }
        else { err = "cambios: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
// cambiosmarca 0|1: el '*' del NOMBRE DEL PROYECTO en la tarjeta Archivo de Properties (la pestania 0)
static bool CmdCambiosMarca(std::istringstream& ss, std::string& err) {
    std::string v; ss >> v;
    Properties* p = ElPanel();
    if (!p || !p->propProyNombre) { err = "cambiosmarca: sin panel / sin la tarjeta Archivo"; return false; }
    const int antes = p->pestaniaActiva;
    p->pestaniaActiva = 0;
    p->ActualizarPestanias();
    rootViewport->Render();
    const std::string etiqueta = p->propProyNombre->name;
    p->pestaniaActiva = antes;
    p->ActualizarPestanias();
    const bool marca = !etiqueta.empty() && etiqueta[etiqueta.size() - 1] == '*';
    printf("      [cambiosmarca] '%s'\n", etiqueta.c_str());
    if (marca != (v == "1")) { err = std::string("cambiosmarca: el nombre del proyecto ") + (marca ? "tiene" : "no tiene") + " el '*'"; return false; }
    return true;
}
// cambiosucio <tipo> <id> 0|1 / cambioraiz <nombre> 0|1: el '*' de un recurso / de una raiz
static bool CmdCambioSucio(std::istringstream& ss, std::string& err) {
    std::string c, id, v; ss >> c >> id >> v;
    const int t = VistaDe(c, err, "cambiosucio"); if (t < 0) return false;
    const bool s = W3dCambiosRecursoSucio(t, id);
    printf("      [cambiosucio] %s '%s': %s\n", c.c_str(), id.c_str(), s ? "SUCIO *" : "guardado");
    if (s != (v == "1")) { err = "cambiosucio: '" + id + "' " + (s ? "esta sucio" : "esta guardado"); return false; }
    return true;
}
static bool CmdCambioRaiz(std::istringstream& ss, std::string& err) {
    std::string n, v; ss >> n >> v;
    int i = W3dRaizBuscar(W3D_RAIZ_ESCENA, n);
    if (i < 0) i = W3dRaizBuscar(W3D_RAIZ_PREFAB, n);
    if (i < 0) { err = "cambioraiz: no hay una raiz '" + n + "'"; return false; }
    const bool s = W3dCambiosRaizSucia(i);
    printf("      [cambioraiz] '%s': %s\n", n.c_str(), s ? "SUCIA *" : "guardada");
    if (s != (v == "1")) { err = "cambioraiz: '" + n + "' " + (s ? "esta sucia" : "esta guardada"); return false; }
    return true;
}
// cartel salir|abrir|nuevo [abre|noabre] / cartelresponder guardar|seguir|cancelar: el cartel "Se perderan los
// cambios en:" (en el harness normalmente no se muestra: aca se lo prende a proposito) y su respuesta. Los
// ganchos de salir/abrir no hacen nada de verdad en el harness: se anota que se llamaron.
static int gCartelSalio = 0, gCartelAbrio = 0;
static std::string gCartelRuta;
static void CartelSalirPrueba() { gCartelSalio++; }
static void CartelAbrirPrueba(const std::string& r) { gCartelAbrio++; gCartelRuta = r; }
static bool CmdCartel(std::istringstream& ss, std::string& err) {
    std::string a, esp; ss >> a >> esp;
    const int acc = (a == "abrir") ? W3D_CAMBIOS_ABRIR : (a == "nuevo") ? W3D_CAMBIOS_NUEVO : W3D_CAMBIOS_SALIR;
    W3dCambiosSalirHook = CartelSalirPrueba;
    W3dCambiosAbrirHook = CartelAbrirPrueba;
    gCartelSalio = gCartelAbrio = 0;
    g_w3dCambiosSinCartel = false;
    const bool abrio = W3dCambiosPreguntar(acc, "otro.w3d");
    g_w3dCambiosSinCartel = true;
    const bool popup = PopUpActive != NULL;
    printf("      [cartel] %s: %s%s\n", a.c_str(), abrio ? "ABIERTO" : "no hacia falta", popup ? " (popup a la vista)" : "");
    if (abrio) { rootViewport->Render(); LayoutRenderMenu(MenuPantallaW, MenuPantallaH); }   // (se dibuja: la lista)
    if ((esp == "abre") != abrio) { err = std::string("cartel: ") + (abrio ? "se abrio" : "no se abrio"); return false; }
    return true;
}
static bool CmdCartelResponder(std::istringstream& ss, std::string& err) {
    std::string r, k, v; ss >> r >> k >> v;
    if (!W3dCambiosCartelAbierto() || !PopUpActive) { err = "cartelresponder: no hay cartel abierto"; return false; }
    const int tecla = (r == "cancelar") ? (int)LayoutKey::Cancel : (int)LayoutKey::Enter;
    if (r == "seguir") PopUpActive->Tecla(LayoutKey::Right);   // el foco al segundo boton
    PopUpActive->Tecla(tecla);
    printf("      [cartelresponder] %s: salir %d, abrir %d ('%s')\n", r.c_str(), gCartelSalio, gCartelAbrio, gCartelRuta.c_str());
    if (k == "salio" && gCartelSalio != atoi(v.c_str())) { err = "cartelresponder: salir se llamo " + Entero(gCartelSalio); return false; }
    if (k == "abrio" && gCartelAbrio != atoi(v.c_str())) { err = "cartelresponder: abrir se llamo " + Entero(gCartelAbrio); return false; }
    return true;
}
// mallasrecurso [estricto] [n N]: TODA malla es un recurso: cada objeto malla de las raices vivas tiene el suyo y
// la vista "Mallas 3D" de la biblioteca lista EXACTAMENTE los recursos vivos (usados + huerfanos, sin los borrados)
static void JuntarMallasRaiz(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        if (!o->Childrens[i]) continue;
        if (o->Childrens[i]->getType() == ObjectType::mesh) out.push_back((Mesh*)o->Childrens[i]);
        JuntarMallasRaiz(o->Childrens[i], out);
    }
}
// outmover si|no [destino <carpeta|->]: el MODO MOVER de la biblioteca (la G) esta activo? y hacia donde va
static bool CmdOutMover(std::istringstream& ss, std::string& err) {
    Outliner* o = ElOutliner(err, "outmover"); if (!o) return false;
    std::string sn; ss >> sn;
    printf("      [outmover] %s, destino '%s'\n", o->moviendoRec ? "activo" : "no", o->moverRecDestino.c_str());
    if ((sn == "si") != o->moviendoRec) { err = std::string("outmover: el modo mover ") + (o->moviendoRec ? "esta" : "no esta") + " activo"; return false; }
    std::string k, v;
    while (ss >> k >> v) {
        if (k == "destino") { if (Guion(v) != o->moverRecDestino) { err = "outmover: el destino es '" + o->moverRecDestino + "', se esperaba '" + v + "'"; return false; } }
        else { err = "outmover: no entiendo '" + k + "'"; return false; }
    }
    return true;
}
// mallasrecurso [estricto] [n <N>]: toda malla de todas las raices tiene su recurso y la biblioteca lista
// exactamente los recursos vivos. 'estricto' = las mallas SIN recurso se cuentan ANTES de listar (listar la
// biblioteca le crea el suyo a la que no lo tiene: sin esto la prueba arreglaba antes de verificar); solo corre
// lo que corre un cuadro del editor (su tick), asi que valida el camino real de la UI
static bool CmdMallasRecurso(std::istringstream& ss, std::string& err) {
    std::string k; int n = -1;
    bool estricto = false;
    while (ss >> k) {
        if (k == "estricto") estricto = true;
        else if (k == "n") { if (!(ss >> n)) { err = "mallasrecurso: falta el numero de 'n'"; return false; } }
        else { err = "mallasrecurso: no entiendo '" + k + "'"; return false; }
    }
    std::vector<Object*> raices; W3dRaicesVivas(raices);
    std::vector<Mesh*> ms;
    for (size_t i = 0; i < raices.size(); i++) JuntarMallasRaiz(raices[i], ms);
    int sin = 0;
    if (estricto) {
        extern void W3dMallasTickEditor();
        W3dMallasTickEditor();   // (un cuadro del editor)
        for (size_t i = 0; i < ms.size(); i++) if (!ms[i]->malla) { sin++; printf("      [mallasrecurso] SIN RECURSO (antes de listar): '%s'\n", ms[i]->name.c_str()); }
    }
    std::vector<W3dRecursoItem> its; W3dVistaRecListar(W3D_VISTA_MALLAS, its);   // (lista y asegura)
    if (!estricto)
        for (size_t i = 0; i < ms.size(); i++) if (!ms[i]->malla) { sin++; printf("      [mallasrecurso] SIN RECURSO: '%s'\n", ms[i]->name.c_str()); }
    std::set<std::string> vivos, listados;
    const std::vector<MallaRecurso*>& reg = W3dMallasRegistro();
    for (size_t i = 0; i < reg.size(); i++) if (!reg[i]->borrado) vivos.insert(reg[i]->nombre);
    for (size_t i = 0; i < its.size(); i++) listados.insert(its[i].id);
    // (cada objeto: su recurso esta en la lista)
    for (size_t i = 0; i < ms.size(); i++) if (ms[i]->malla && !listados.count(ms[i]->malla->nombre)) { err = "mallasrecurso: el recurso de '" + ms[i]->name + "' no se lista"; return false; }
    printf("      [mallasrecurso] %d mallas, %d sin recurso, %d recursos vivos, %d listados\n", (int)ms.size(), sin, (int)vivos.size(), (int)listados.size());
    if (sin > 0) { err = "mallasrecurso: hay mallas sin recurso"; return false; }
    if (vivos != listados) { err = "mallasrecurso: la biblioteca no lista exactamente los recursos vivos"; return false; }
    if (n >= 0 && n != (int)listados.size()) { err = "mallasrecurso: hay " + Entero((long)listados.size()) + " recursos, se esperaban " + Entero(n); return false; }
    return true;
}
// recubicar <tipo> <id> dentro|fuera [ruta]: pasa el archivo adentro / afuera del .w3d (la del panel: con undo)
static bool CmdRecUbicar(std::istringstream& ss, std::string& err) {
    std::string c, id, d, ruta; ss >> c >> id >> d >> ruta;
    const int t = VistaDe(c, err, "recubicar"); if (t < 0) return false;
    id = IdDeVista(t, id);
    std::string nuevo, motivo;
    if (!ruta.empty() && ruta[0] != '/') { char b[4096]; if (getcwd(b, sizeof(b))) ruta = std::string(b) + "/" + ruta; }
    const bool ok = W3dVistaRecFijarUbicacion(t, id, d == "fuera" ? 1 : 0, ruta, &nuevo, &motivo);
    printf("      [recubicar] %s '%s' -> %s: %s '%s' %s\n", c.c_str(), id.c_str(), d.c_str(), ok ? "OK" : "NO", nuevo.c_str(), motivo.c_str());
    if (!ok) { err = "recubicar: " + motivo; return false; }
    return true;
}
// recubicacion <tipo> <id> dentro|fuera: donde vive hoy
static bool CmdRecUbicacion(std::istringstream& ss, std::string& err) {
    std::string c, id, d; ss >> c >> id >> d;
    const int t = VistaDe(c, err, "recubicacion"); if (t < 0) return false;
    id = IdDeVista(t, id);
    const int u = W3dVistaRecUbicacion(t, id);
    printf("      [recubicacion] %s '%s': %s\n", c.c_str(), id.c_str(), u == 0 ? "dentro" : u == 1 ? "fuera" : "(sin archivo)");
    if ((d == "fuera") != (u == 1) || u < 0) { err = "recubicacion: '" + id + "' no esta " + d; return false; }
    return true;
}
// recduplicar <tipo> <id> [esperado]
static bool CmdRecDuplicar(std::istringstream& ss, std::string& err) {
    std::string c, id, esp; ss >> c >> id >> esp;
    const int t = VistaDe(c, err, "recduplicar"); if (t < 0) return false;
    std::string nuevo, motivo;
    if (!W3dVistaRecDuplicar(t, id, &nuevo, &motivo)) { err = "recduplicar: " + motivo; return false; }
    printf("      [recduplicar] %s '%s' -> '%s'\n", c.c_str(), id.c_str(), nuevo.c_str());
    if (!esp.empty() && nuevo != esp) { err = "recduplicar: quedo '" + nuevo + "', se esperaba '" + esp + "'"; return false; }
    return true;
}
// recborrarforzado <tipo> <id> [falla]: borrar un recurso EN USO (sus usuarios quedan sin el, con Ctrl+Z)
static bool CmdRecBorrarForzado(std::istringstream& ss, std::string& err) {
    std::string c, id, f; ss >> c >> id >> f;
    const int t = VistaDe(c, err, "recborrarforzado"); if (t < 0) return false;
    std::string motivo;
    const bool ok = W3dVistaRecBorrarForzado(t, id, &motivo);
    printf("      [recborrarforzado] %s '%s': %s %s\n", c.c_str(), id.c_str(), ok ? "borrado" : "NO", motivo.c_str());
    if (ok == (f == "falla")) { err = "recborrarforzado: " + std::string(ok ? "se borro y no debia" : "no se borro: " + motivo); return false; }
    return true;
}
// recdato <tipo> <id> <etiqueta en ingles...> = <valor en ingles...>: un DATO del tipo (los de la tarjeta del
// recurso), exacto ("recdato texturas texturas/a.png Resolution = 64 x 64"). Sin '=' solo los vuelca.
static bool CmdRecDato(std::istringstream& ss, std::string& err) {
    std::string c, id, resto; ss >> c >> id; std::getline(ss, resto);
    const int t = VistaDe(c, err, "recdato"); if (t < 0) return false;
    id = IdDeVista(t, id);
    // (en INGLES: la prueba no depende del idioma de la UI)
    const W3dIdioma antes = g_idioma;
    W3dIdiomaSet(W3dIdiomaDe("en"));
    std::vector<W3dRecursoDato> ds; W3dVistaRecDatos(t, id, ds);
    W3dIdiomaSet(antes);
    for (size_t i = 0; i < ds.size(); i++) printf("      [recdato] %s: %s\n", ds[i].etiqueta.c_str(), ds[i].valor.c_str());
    const size_t eq = resto.find('=');
    if (eq == std::string::npos) return true;
    std::string et = resto.substr(0, eq), val = resto.substr(eq + 1);
    while (!et.empty() && et[0] == ' ') et.erase(0, 1);
    while (!et.empty() && et[et.size() - 1] == ' ') et.erase(et.size() - 1);
    while (!val.empty() && val[0] == ' ') val.erase(0, 1);
    while (!val.empty() && val[val.size() - 1] == ' ') val.erase(val.size() - 1);
    for (size_t i = 0; i < ds.size(); i++)
        if (ds[i].etiqueta == et) {
            if (ds[i].valor != val) { err = "recdato: " + et + " = '" + ds[i].valor + "', se esperaba '" + val + "'"; return false; }
            return true;
        }
    err = "recdato: '" + id + "' no tiene el dato '" + et + "'";
    return false;
}
// propsmalla [tab N] [malla3d 0|1] [recurso 0|1] [selector <texto>] [pestanias N]: la pestania "Malla 3D" (la 3)
// del panel: con un objeto malla muestra su tarjeta "3D Mesh" (la primera); con un recurso de la biblioteca, el
// recurso. No hay pestania "Recurso" aparte (BarTabs tiene 10)
static bool CmdPropsMalla(std::istringstream& ss, std::string& err) {
    Properties* p = ElPanel();
    if (!p) { err = "propsmalla: sin panel de propiedades"; return false; }
    rootViewport->Render();
    p->RefreshTargetProperties();
    p->ActualizarPestanias();
    const bool m3 = p->propMalla3D && p->propMalla3D->visible;
    const bool rec = p->propRecurso && p->propRecurso->visible;
    // (la tarjeta 3D Mesh es la PRIMERA visible de la pestania)
    int primera = -1;
    for (size_t i = 0; i < p->GroupProperties.size() && primera < 0; i++) if (p->GroupProperties[i]->visible) primera = (int)i;
    const std::string sel = p->propBtnMallaSel ? p->propBtnMallaSel->button->text : std::string();
    printf("      [propsmalla] pestania %d (icono %s), %d pestanias, 3D Mesh %s%s, recurso %s, selector '%s'\n", p->pestaniaActiva,
           p->BarTabs.size() > 3 ? IconoNombre(p->BarTabs[3]->icon) : "?", (int)p->BarTabs.size(), m3 ? "SI" : "no",
           (m3 && primera >= 0 && p->GroupProperties[(size_t)primera] == p->propMalla3D) ? " (la primera)" : "", rec ? "SI" : "no", sel.c_str());
    std::string k;
    while (ss >> k) {
        std::string v; ss >> v;
        bool ok = true;
        if (k == "tab") ok = (p->pestaniaActiva == atoi(v.c_str()));
        else if (k == "malla3d") ok = (m3 == (v == "1")) && (!m3 || p->GroupProperties[(size_t)primera] == p->propMalla3D);
        // (la tarjeta del recurso visible Y el panel dibujandola: elegir un recurso suelta la escena, asi que sin
        // objeto activo la pestania tiene que contar como global, sino el panel queda vacio)
        else if (k == "recurso") ok = (rec == (v == "1")) && (!rec || ObjActivo || p->PestaniaGlobal());
        else if (k == "selector") ok = (sel == v);
        else if (k == "pestanias") ok = ((int)p->BarTabs.size() == atoi(v.c_str()));
        else { err = "propsmalla: no entiendo '" + k + "'"; return false; }
        if (!ok) { err = "propsmalla: no cumple " + k + " " + v; return false; }
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
    PropsActivo = p;
    p->pestaniaActiva = n;
    p->ActualizarPestanias();
    p->Resize(p->width, p->height);
    rootViewport->Render();
    return true;
}

// ============================================================================
//  propsalfa: la fila "Alpha Test" de la tarjeta Material (Material::alphaTest, clave "alfaCorte"; 0 = apagado)
// ============================================================================
// Todo por la puerta del MOUSE de verdad (LayoutClickUI -> ClickEn, event_mouse_motion con el dx global,
// mouse_button_up + LayoutSoltar) y el Render del panel, que es el que commitea el paso de Ctrl+Z:
//   propsalfa fila [<valor>|-|arrastre]  la fila esta en la tarjeta Material (pestania 2), se llama T("Alpha Test")
//                                        y muestra el alfaCorte del material ('-' = OCULTA: material por defecto;
//                                        'arrastre' = el valor que dejo el ultimo 'arrastrar', para el redo)
//   propsalfa arrastrar <px> [<min> <max>]   click en la fila, arrastre horizontal de <px> y soltar: el valor sube
//                                        (o baja) y queda en [min, max] si se dan; el Ctrl+Z queda en UN paso
//   propsalfa texto <numero>             click SIN arrastrar (abre la edicion por texto), tipear y Enter por la
//                                        puerta del teclado de PC (SDL_TEXTINPUT + SDLK_RETURN)
#ifndef W3D_SYMBIAN
static float g_alfaArrastre = -1.0f;   // el valor que dejo el ultimo 'propsalfa arrastrar'
// el centro de la columna de VALORES de una fila del panel, con el MISMO recorrido de filas que ClickEn; si la fila
// queda fuera de la vista, corre el scroll vertical (PosY) y vuelve a dibujar. false = la fila no se dibuja
static bool UbicarFilaPanel(Properties* p, PropertieBase* fila, int& mx, int& my) {
    for (int intento = 0; intento < 3; intento++) {
        rootViewport->Render();   // las alturas de las tarjetas (g->height) y el tope del scroll
        int yCursor = p->y + p->BarTopOffset() + p->PosY + borderGS + RenglonHeightGS + gapGS;
        bool hallada = false;
        for (size_t i = 0; i < p->GroupProperties.size() && !hallada; i++) {
            GroupPropertie* g = p->GroupProperties[i];
            if (!g->visible) continue;
            if (g->open) {
                int yFila = yCursor + borderGS + RenglonHeightGS + gapGS;   // la cabecera
                for (size_t j = 0; j < g->properties.size(); j++) {
                    const int hFila = g->properties[j]->Resize(g->width);
                    if (g->properties[j] == fila) {
                        if (hFila <= 0) return false;
                        mx = p->x + p->PosX + borderGS * 2 + g->colEtiqueta + (g->width - g->colEtiqueta) / 3;
                        my = yFila + hFila / 2;
                        hallada = true;
                        break;
                    }
                    yFila += hFila;
                }
            }
            yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
        }
        if (!hallada) return false;
        const int arriba = p->y + p->BarTopOffset() + RenglonHeightGS, abajo = p->y + p->height - RenglonHeightGS;
        if (my >= arriba && my < abajo) return true;
        p->PosY -= my - (arriba + abajo) / 2;   // la fila al medio del panel
    }
    return false;
}
static bool CmdPropsAlfa(std::istringstream& ss, std::string& err) {
    extern int dx;
    extern void InputUsuarioSDL3(SDL_Event&);
    std::string sub; ss >> sub;
    Properties* p = ElPanel();
    if (!p || !rootViewport) { err = "propsalfa: sin panel de propiedades"; return false; }
    // la pestania 2 (la del material del objeto activo), como el click en su pestania
    rootViewport->Render();
    if (p->BarTabs.size() <= 2 || !p->BarTabs[2]->visible) { err = "propsalfa: el panel no muestra la pestania del material"; return false; }
    PropsActivo = p;
    p->pestaniaActiva = 2;
    p->ActualizarPestanias();
    p->Resize(p->width, p->height);
    rootViewport->Render();
    PropFloat* f = p->propMatAlfa;
    bool enTarjeta = false;
    if (f && p->propMaterial)
        for (size_t j = 0; j < p->propMaterial->properties.size(); j++) if (p->propMaterial->properties[j] == f) enTarjeta = true;
    if (!f || !enTarjeta) { err = "propsalfa: la tarjeta Material no tiene la fila Alpha Test"; return false; }
    if (!p->propMaterial->visible) { err = "propsalfa: la tarjeta Material no se ve (el objeto activo es una malla?)"; return false; }
    if (f->name != T("Alpha Test")) { err = "propsalfa: la fila se llama '" + f->name + "'"; return false; }
    if (sub == "fila") {
        std::string esp; ss >> esp;
        printf("      [propsalfa] fila '%s' %s\n", f->name.c_str(), f->value ? "visible" : "OCULTA");
        if (f->value) printf("      [propsalfa] valor=%g\n", *f->value);
        if (esp == "-") { if (f->value) { err = "propsalfa: la fila se ve y tenia que estar oculta"; return false; } return true; }
        if (!f->value) { err = "propsalfa: la fila esta oculta"; return false; }
        if (esp.empty()) return true;
        const float v = (esp == "arrastre") ? g_alfaArrastre : (float)atof(esp.c_str());
        if (fabsf(*f->value - v) > 1e-4f) {
            char b[160]; snprintf(b, sizeof b, "propsalfa: la fila muestra %g y se esperaba %g", *f->value, v); err = b; return false;
        }
        return true;
    }
    if (!f->value) { err = "propsalfa: la fila esta oculta (material por defecto?)"; return false; }
    int mx = 0, my = 0;
    if (!UbicarFilaPanel(p, f, mx, my)) { err = "propsalfa: no pude ubicar la fila en el panel"; return false; }
    const float antes = *f->value;
    if (sub == "arrastrar") {
        int px = 0; ss >> px;
        float mn = -1.0f, mxv = -1.0f; const bool rango = !(ss >> mn >> mxv).fail();
        leftMouseDown = true;
        LayoutClickUI(mx, my);
        // el arrastre: el dx GLOBAL por evento, de a 5 px (como llega del loop de SDL)
        int hecho = 0;
        while (hecho != px) {
            int d = px > hecho ? 5 : -5;
            if (abs(px - hecho) < 5) d = px - hecho;
            dx = d; hecho += d;
            p->event_mouse_motion(mx + hecho, my);
        }
        dx = 0;
        rootViewport->Render();   // con el boton apretado el panel NO commitea
        leftMouseDown = false;
        p->mouse_button_up(W3dMB_IZQ);
        LayoutSoltar(mx + px, my);
        rootViewport->Render();   // al soltar: el paso de Ctrl+Z
        g_alfaArrastre = *f->value;
        printf("      [propsalfa] arrastre de %d px: %g -> %g\n", px, antes, *f->value);
        if (px != 0 && (px > 0 ? !(*f->value > antes) : !(*f->value < antes))) { err = "propsalfa: el arrastre no cambio el valor"; return false; }
        if (rango && (*f->value < mn - 1e-4f || *f->value > mxv + 1e-4f)) {
            char b[160]; snprintf(b, sizeof b, "propsalfa: el arrastre dejo %g, fuera de [%g, %g]", *f->value, mn, mxv); err = b; return false;
        }
        return true;
    }
    if (sub == "texto") {
        std::string num; ss >> num;
        if (num.empty()) { err = "propsalfa: texto <numero>"; return false; }
        leftMouseDown = true;
        LayoutClickUI(mx, my);
        leftMouseDown = false;
        p->mouse_button_up(W3dMB_IZQ);   // click puro: abre la edicion por texto
        LayoutSoltar(mx, my);
        rootViewport->Render();          // (editando por texto el panel NO commitea la foto del click)
        if (!NumEditActivo() || g_propFloatEditando != f) { err = "propsalfa: el click sin arrastre no abrio la edicion por texto"; return false; }
        if (!viewPortActive) viewPortActive = p;   // (sin viewport con foco la puerta de SDL no atiende el teclado)
        SDL_Event ev; memset(&ev, 0, sizeof(ev));
        ev.type = SDL_TEXTINPUT;
        strncpy(ev.text.text, num.c_str(), sizeof(ev.text.text) - 1);
        InputUsuarioSDL3(ev);            // todo seleccionado: lo tipeado reemplaza
        memset(&ev, 0, sizeof(ev));
        ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_RETURN; ev.key.keysym.scancode = SDL_SCANCODE_RETURN;
        ev.key.state = SDL_PRESSED;
        InputUsuarioSDL3(ev);
        ev.type = SDL_KEYUP; ev.key.state = SDL_RELEASED;
        InputUsuarioSDL3(ev);
        rootViewport->Render();          // fin de la edicion: el paso de Ctrl+Z
        printf("      [propsalfa] tipeado '%s': %g -> %g\n", num.c_str(), antes, *f->value);
        if (NumEditActivo()) { err = "propsalfa: la edicion por texto sigue abierta despues del Enter"; return false; }
        return true;
    }
    err = "propsalfa: fila [valor|-|arrastre] | arrastrar <px> [min max] | texto <numero>";
    return false;
}
#endif

// vpoverlays [on|off] [es 0|1] [publicado 0|1]: el ojo "Show Overlays" del PRIMER viewport 3D del layout: lo prende o
// apaga (SetShowOverlays, lo mismo que el boton de su barra), asserta como esta y, con 'publicado', que al dibujar el
// viewport publica ese valor (showOverlayGlobal: lo que miran los gizmos, la grilla y la curva; el Play lo respeta)
static bool CmdVpOverlays(std::istringstream& ss, std::string& err) {
    Viewport3D* v = (Viewport3D*)BuscarHoja(rootViewport, 1);
    if (!v) { err = "vpoverlays: no hay viewport 3D en el layout"; return false; }
    std::string k;
    while (ss >> k) {
        if (k == "on" || k == "off") { v->SetShowOverlays(k == "on"); continue; }
        std::string val; ss >> val;
        const bool esp = (val == "1");
        if (k == "es") {
            printf("      [vpoverlays] showOverlays=%s\n", v->showOverlays ? "si" : "no");
            if (v->showOverlays != esp) { err = std::string("vpoverlays: el viewport tiene los overlays ") + (v->showOverlays ? "prendidos" : "apagados"); return false; }
        } else if (k == "publicado") {
            rootViewport->Render();
            printf("      [vpoverlays] publicado=%s\n", showOverlayGlobal ? "si" : "no");
            if (showOverlayGlobal != esp) { err = std::string("vpoverlays: al dibujar se publico ") + (showOverlayGlobal ? "prendido" : "apagado"); return false; }
        } else { err = "vpoverlays: no entiendo '" + k + "'"; return false; }
    }
    return true;
}

// ============================================================================
//  EL DESPACHADOR
// ============================================================================
bool W3dPruebasOutlinerCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "outvista")        return CmdOutVista(ss, err);
    if (cmd == "outfilas")        return CmdOutFilas(ss, err);
    if (cmd == "outfila")         return CmdOutFila(ss, err, false);
    if (cmd == "outnofila")       return CmdOutFila(ss, err, true);
    if (cmd == "outclick")        return CmdOutClick(ss, err);
    if (cmd == "outdrag")         return CmdOutDrag(ss, err);
    if (cmd == "outcontexto")     return CmdOutContexto(ss, err);
    if (cmd == "outtecla")        return CmdOutTecla(ss, err);
    if (cmd == "outteclapc")      return CmdOutTeclaPC(ss, err);
    if (cmd == "outbarra")        return CmdOutBarra(ss, err);
    if (cmd == "outmenu")         return CmdOutMenu(ss, err);
    if (cmd == "outescribir")     return CmdOutEscribir(ss, err);
    if (cmd == "outconfirmar")    return CmdOutConfirmar(ss, err);
    if (cmd == "outrender")       return CmdOutRender(err);
    if (cmd == "outpx")           return CmdOutPx(ss, err);
    if (cmd == "recactivo")       return CmdRecActivo(ss, err);
    if (cmd == "recprops")        return CmdRecProps(ss, err);
    if (cmd == "receditar")       return CmdRecEditar(ss, err);
    if (cmd == "recursos")        return CmdRecursos(ss, err);
    if (cmd == "recurso")         return CmdRecurso(ss, err);
    if (cmd == "carpetas")        return CmdCarpetas(ss, err);
    if (cmd == "carpetanueva")    return CmdCarpetaNueva(ss, err);
    if (cmd == "carpetarenombrar") return CmdCarpetaRenombrar(ss, err);
    if (cmd == "carpetaborrar")   return CmdCarpetaBorrar(ss, err);
    if (cmd == "carpetamover")    return CmdCarpetaMover(ss, err);
    if (cmd == "recmover")        return CmdRecMover(ss, err);
    if (cmd == "recrenombrar")    return CmdRecRenombrar(ss, err);
    if (cmd == "recborrar")       return CmdRecBorrar(ss, err);
    if (cmd == "recpurgar")       return CmdRecPurgar(ss, err);
    if (cmd == "matsuelto")       return CmdMatSuelto(ss, err);
    if (cmd == "texasignar")      return CmdTexAsignar(ss, err);
    if (cmd == "texentrada")      return CmdTexEntrada(ss, err);
    if (cmd == "jsontiene")       return CmdJsonTiene(ss, err);
    if (cmd == "seleccion")       return CmdSeleccion(ss, err);
    if (cmd == "recprovprueba")   return CmdRecProvPrueba(ss, err);
    if (cmd == "outsel")          return CmdOutSel(ss, err);
    if (cmd == "outtipear")       return CmdOutTipear(ss, err);
    if (cmd == "outesc")          return CmdOutEsc(err);
    if (cmd == "campoenfocar")    return CmdCampoEnfocar(ss, err);
    if (cmd == "texcargada")      return CmdTexCargada(ss, err);
    if (cmd == "texpurgadas")     return CmdTexPurgadas(ss, err);
    if (cmd == "partex")          return CmdParTex(ss, err);
    if (cmd == "fliptex")         return CmdFlipTex(ss, err);
    if (cmd == "texrefde")        return CmdTexRefDe(ss, err);
    if (cmd == "entradacruda")    return CmdEntradaCruda(ss, err, false);
    if (cmd == "entradatexto")    return CmdEntradaCruda(ss, err, true);
    if (cmd == "outfondo")        return CmdOutFondo(ss, err);
    if (cmd == "outdragsobre")    return CmdOutDragSobre(ss, err);
    if (cmd == "recboton")        return CmdRecBoton(ss, err);
    if (cmd == "recdatos")        return CmdRecDatos(ss, err);
    if (cmd == "texdormidas")     return CmdTexDormidas(ss, err);
    // FASE 4b: la biblioteca
    if (cmd == "outdobleclick")   return CmdOutDobleClick(ss, err);
    if (cmd == "outtap")          return CmdOutTap(ss, err);
    if (cmd == "outmantener")     return CmdOutMantener(ss, err);
    if (cmd == "outfiltro")       return CmdOutFiltro(ss, err);
    if (cmd == "outcuadricula")   return CmdOutCuadricula(ss, err);
    if (cmd == "outmini")         return CmdOutMini(ss, err);
    if (cmd == "outlinea")        return CmdOutLinea(ss, err);
    if (cmd == "outcolor")        return CmdOutColor(ss, err);
    if (cmd == "outsoltar")       return CmdOutSoltar(ss, err);
    if (cmd == "libvincular")     return CmdLibVincular(ss, err);
    if (cmd == "libdesvincular")  return CmdLibDesvincular(ss, err);
    if (cmd == "libs")            return CmdLibs(ss, err);
    if (cmd == "cambios")         return CmdCambios(ss, err);
    if (cmd == "cambiosfoto")     { W3dCambiosFoto(); return true; }
    if (cmd == "cambiosucio")     return CmdCambioSucio(ss, err);
    if (cmd == "cambioraiz")      return CmdCambioRaiz(ss, err);
    if (cmd == "cartel")          return CmdCartel(ss, err);
    if (cmd == "cartelresponder") return CmdCartelResponder(ss, err);
    if (cmd == "mallasrecurso")   return CmdMallasRecurso(ss, err);
    if (cmd == "outmover")        return CmdOutMover(ss, err);
    if (cmd == "cambiosmarca")    return CmdCambiosMarca(ss, err);
    if (cmd == "recubicar")       return CmdRecUbicar(ss, err);
    if (cmd == "recubicacion")    return CmdRecUbicacion(ss, err);
    if (cmd == "recduplicar")     return CmdRecDuplicar(ss, err);
    if (cmd == "recborrarforzado") return CmdRecBorrarForzado(ss, err);
    if (cmd == "recdato")         return CmdRecDato(ss, err);
    if (cmd == "propsmalla")      return CmdPropsMalla(ss, err);
    if (cmd == "propstab")        return CmdPropsTab(ss, err);
    if (cmd == "archivomover")    return CmdArchivoMover(ss, err);
    if (cmd == "idebib")          return CmdIdeBib(ss, err);
    if (cmd == "recexterno")      return CmdRecExterno(ss, err);
    if (cmd == "minidistintas")   return CmdMiniDistintas(ss, err);
    if (cmd == "bibmedir")        return CmdBibMedir(ss, err);
#ifndef W3D_SYMBIAN
    if (cmd == "propsalfa")       return CmdPropsAlfa(ss, err);
#endif
    if (cmd == "vpoverlays")      return CmdVpOverlays(ss, err);
    manejado = false;
    return false;
}
