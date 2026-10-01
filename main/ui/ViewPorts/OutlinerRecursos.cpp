// ============================================================================
//  OutlinerRecursos.cpp — la BIBLIOTECA del outliner (ver Outliner.h e
//  io/RecursosProyecto.h): el explorador del contenido del .w3d. UN arbol de
//  carpetas cosmeticas con TODOS los tipos mezclados (mallas 3D, materiales,
//  texturas, sonidos, animaciones, prefabs, escenas y juegos, scripts, fuentes,
//  videos), un FILTRO por tipo, LINEAS GUIA de la jerarquia, lista o CUADRICULA con
//  miniaturas; cada recurso dice si esta EN USO (tilde verde) o HUERFANO (cruz roja)
//  y cuantos lo usan. Tambien las LIBRERIAS externas vinculadas (otros .w3d, de solo
//  lectura) y la barra del outliner (vista, Objeto, Seleccion, "+", filtro).
//
//  UN SOLO MODELO DE FILAS (FilasRecursos) y UNA SOLA GEOMETRIA (RectFila): el
//  dibujo, el click, el arrastre, el teclado y el harness lo arman igual, asi que no
//  hay recorridos paralelos que se puedan desalinear.
//
//  ENTRADA: mouse (click = elegir; Ctrl/Shift = seleccion multiple; doble click en el
//  nombre = renombrar en linea; flechita = plegar; arrastrar = mover a una carpeta, o
//  soltar AFUERA: en el viewport 3D o sobre un desplegable de Properties; click derecho
//  = el menu Objeto), TACTIL (arrastrar el dedo = scroll; MANTENER ~0,5 s = agarrar la
//  fila -con una marca- y arrastrarla, o soltar sin mover = el menu contextual; doble
//  tap = renombrar), teclado de PC (flechas, Shift+flechas, A, Enter, F2, Supr/X, G =
//  modo mover, Esc) y el keypad del N95 (flechas/OK por LayoutTeclaPanelActivo, 5
//  marca, 1 mueve, el toque de C borra; la tecla izquierda abre la barra y las flechas
//  recorren sus menus).
//  Motor generico: aca no hay nombres de ningun juego.
// ============================================================================
#include "w3dGraphics.h"
#ifdef _WIN32
    #include <windows.h>
#endif
#include "Outliner.h"
#include "LayoutInput.h"                     // LayoutKey, LayoutMenuAbierto
#include "LayoutArbol.h"                     // gancho de destruccion (el menu recuerda su outliner)
#include "ViewPort3D.h"                      // el arrastre de un recurso al 3D
#include "Properties.h"                      // ...y a un desplegable de Properties
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h"
#include "WhiskUI/widgets/Button.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "render/OpcionesRender.h"           // g_redraw
#include "ViewPorts/PopUp/ConfirmarPopup.h"  // confirmar un borrado
#include "ViewPorts/PopUp/FileBrowser.h"     // "Link .w3d...": el explorador de archivos de siempre
#include "ViewPorts/Notificaciones.h"
#include "config/W3dLang.h"
#include "io/RecursosProyecto.h"
#include "io/BibliotecaExterna.h"            // las librerias externas (vistas de solo lectura)
#include "io/CambiosProyecto.h"              // el '*' de lo no guardado
#include "io/Miniaturas.h"                   // la cuadricula: miniaturas perezosas
#include "undo/Undo.h"                       // borrar N partes (clips de una biblioteca) = UN Ctrl+Z
#include "io/RaicesEditor.h"                 // el selector de escena/juego/prefab y el "+" de la barra
#include "io/PrefabsEditor.h"                // "Create Prefab" / "Unpack Prefab" del menu Objeto
#include "objects/InstanciaPrefab.h"
#include "objects/ObjectMode.h"              // W3dRenombrarObjeto (renombrar en linea en la escena)
#include "W3dRaices.h"
#include "base/W3dInteractionState.h"       // InteractionMode (seleccionar un recurso suelta la escena)
#include <algorithm>
#include <map>
#include <cstdio>
#include <cstring>   // strlen: el cartel de borrar se arma en dos partes
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
    extern int W3dPantallaAlto;
#else
    #include <GL/gl.h>
#endif

// Properties muestra el recurso elegido (su pestania "Malla 3D"): Properties.cpp
extern void PropsIrARecurso();
// los destinos de un arrastre de la biblioteca (ver SoltarRecurso.cpp y Properties.cpp): 'soloProbar'
// = solo decir si se puede y que haria ('que', en el idioma de la UI)
extern bool W3dSoltarRecursoEn3D(Viewport3D* vp, int mx, int my, int tipo, const std::string& id, bool soloProbar, std::string* que);
extern bool PropsSoltarRecurso(Properties* p, int mx, int my, int tipo, const std::string& id, bool soloProbar, std::string* que);
extern void PropsDestinosRecurso(Properties* p, int tipo, std::vector<int>& rects);   // x, y, w, h por destino

// el rojo de las notificaciones de error (Notificaciones.cpp): un HUERFANO
static const float kRojoHuerfano[3] = { 0.92f, 0.28f, 0.24f };

OutArrastreBib g_outArrastre;

// ============================================================================
//  EL MODELO DE FILAS
// ============================================================================
// orden de la lista: sin distinguir mayusculas (ASCII), y a igualdad el texto crudo
static bool MenorTexto(const std::string& a, const std::string& b) {
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
static bool MenorItem(const W3dRecursoItem* a, const W3dRecursoItem* b) {
    if (a->nombre != b->nombre) return MenorTexto(a->nombre, b->nombre);
    if (a->tipo != b->tipo) return a->tipo < b->tipo;
    return a->id < b->id;
}
static bool MenorHoja(const std::string& a, const std::string& b) {
    return MenorTexto(W3dCarpetaHoja(a), W3dCarpetaHoja(b));
}

// la fila de un recurso
static OutFilaRec FilaDeItem(const W3dRecursoItem* it, const std::string& carpeta, int prof, bool libExt) {
    OutFilaRec f;
    f.carpeta = false;
    f.tipo = it->tipo;
    f.id = it->id;
    f.clave = W3dBibClave(it->tipo, it->id);
    f.nombre = it->nombre;
    f.carpetaDe = carpeta;
    f.prof = prof;
    f.icono = it->icono;
    f.usuarios = it->usuarios;
    f.soloLectura = it->soloLectura || libExt;
    f.renombrable = it->renombrable && !f.soloLectura;
    f.parte = !it->padre.empty();
    f.sucio = !libExt && W3dCambiosRecursoSucio(it->tipo, it->id);
    return f;
}
// arma las filas de 'carpeta' (sus subcarpetas primero, despues sus recursos) con su sangria. Las PARTES de un
// recurso (W3dRecursoItem::padre: los clips de una biblioteca) van debajo de su padre, una sangria mas.
static void ArmarFilas(const std::string& carpeta, int prof,
                       std::map<std::string, std::vector<std::string> >& hijas,
                       std::map<std::string, std::vector<const W3dRecursoItem*> >& deCarpeta,
                       std::map<std::string, std::vector<const W3dRecursoItem*> >& partes,
                       std::map<std::string, int>& cuenta,
                       const std::set<std::string>& plegadas, bool libExt, std::vector<OutFilaRec>& out) {
    std::vector<std::string>& hs = hijas[carpeta];
    std::sort(hs.begin(), hs.end(), MenorHoja);
    for (size_t i = 0; i < hs.size(); i++) {
        OutFilaRec f;
        f.carpeta = true;
        f.clave = hs[i];
        f.nombre = W3dCarpetaHoja(hs[i]);
        f.carpetaDe = carpeta;
        f.prof = prof;
        f.icono = (int)IconType::carpeta;
        f.usuarios = cuenta[hs[i]];
        f.plegada = plegadas.count(hs[i]) > 0;
        f.soloLectura = libExt;
        out.push_back(f);
        if (!f.plegada) ArmarFilas(hs[i], prof + 1, hijas, deCarpeta, partes, cuenta, plegadas, libExt, out);
    }
    std::vector<const W3dRecursoItem*>& its = deCarpeta[carpeta];
    std::sort(its.begin(), its.end(), MenorItem);
    for (size_t i = 0; i < its.size(); i++) {
        out.push_back(FilaDeItem(its[i], carpeta, prof, libExt));
        std::map<std::string, std::vector<const W3dRecursoItem*> >::iterator p =
            partes.find(W3dBibClave(its[i]->tipo, its[i]->id));
        if (p == partes.end()) continue;
        std::sort(p->second.begin(), p->second.end(), MenorItem);
        for (size_t k = 0; k < p->second.size(); k++) out.push_back(FilaDeItem(p->second[k], carpeta, prof + 1, libExt));
    }
}

void Outliner::FilasRecursos(std::vector<OutFilaRec>& out) {
    out.clear();
    if (vista == OUT_VISTA_ESCENA) return;
    // la ESCENA eligio un objeto despues de lo que se eligio aca (en el 3D, en otro outliner, un script): la
    // biblioteca suelta su seleccion y su cursor. O se eligen objetos o se eligen recursos: la fila no puede
    // seguir en verde como activa ni ser lo que borra un Supr. (En el modo mover lo elegido sigue viajando.)
    if (selEscenaSerial != W3dSeleccionSerial) {
        selEscenaSerial = W3dSeleccionSerial;
        if (!moviendoRec && (SeleccionCantidad() > 0 || !cursorClave.empty())) {
            selRecursos.clear(); selCarpetas.clear(); selMarcada = false;
            cursorClave.clear(); cursorEsCarpeta = false;
            g_redraw = true;
        }
    }
    std::vector<W3dRecursoItem> items;       // los que se VEN (con el filtro)
    std::vector<std::string> carpetas;       // TODO el arbol (filtrado, el arbol se sigue viendo entero)
    const bool libExt = VistaSoloLectura();
    if (libExt) {
        std::vector<W3dRecursoItem> todos;
        std::vector<std::string> creadas;
        W3dLibreriaListar(vista - OUT_VISTA_LIBEXT, todos, creadas, NULL);
        std::set<std::string> cs;
        for (size_t i = 0; i < creadas.size(); i++)
            for (std::string c = creadas[i]; !c.empty(); c = W3dCarpetaPadre(c)) cs.insert(c);
        for (size_t i = 0; i < todos.size(); i++) {
            for (std::string c = todos[i].carpeta; !c.empty(); c = W3dCarpetaPadre(c)) cs.insert(c);
            if (filtro < 0 || todos[i].tipo == filtro) items.push_back(todos[i]);
        }
        carpetas.assign(cs.begin(), cs.end());
    } else {
        std::vector<W3dRecursoItem> todos;
        W3dBibliotecaListar(-1, todos);
        W3dCarpetasBiblioteca(todos, carpetas);
        if (filtro < 0) items.swap(todos);
        else for (size_t i = 0; i < todos.size(); i++) if (todos[i].tipo == filtro) items.push_back(todos[i]);
    }
    std::map<std::string, std::vector<std::string> > hijas;
    for (size_t i = 0; i < carpetas.size(); i++) hijas[W3dCarpetaPadre(carpetas[i])].push_back(carpetas[i]);
    std::map<std::string, std::vector<const W3dRecursoItem*> > deCarpeta;
    std::map<std::string, std::vector<const W3dRecursoItem*> > partes;   // clave del padre -> sus partes
    std::set<std::string> claves;
    for (size_t i = 0; i < items.size(); i++) claves.insert(W3dBibClave(items[i].tipo, items[i].id));
    std::map<std::string, int> cuenta;   // recursos que cuelgan de cada carpeta (recursivo)
    for (size_t i = 0; i < items.size(); i++) {
        const std::string clavePadre = items[i].padre.empty() ? std::string() : W3dBibClave(items[i].tipo, items[i].padre);
        if (!clavePadre.empty() && claves.count(clavePadre)) { partes[clavePadre].push_back(&items[i]); continue; }
        deCarpeta[items[i].carpeta].push_back(&items[i]);
        for (std::string c = items[i].carpeta; !c.empty(); c = W3dCarpetaPadre(c)) cuenta[c]++;
    }
    ArmarFilas("", 0, hijas, deCarpeta, partes, cuenta, libExt ? plegadasExt : plegadas, libExt, out);
    // la SELECCION no puede nombrar lo que ya no esta (purgado, renombrado por un undo...)
    if (!selRecursos.empty()) {
        for (std::set<std::string>::iterator it = selRecursos.begin(); it != selRecursos.end(); )
            if (!claves.count(*it)) selRecursos.erase(it++); else ++it;
    }
    if (!selCarpetas.empty()) {
        std::set<std::string> cs(carpetas.begin(), carpetas.end());
        for (std::set<std::string>::iterator it = selCarpetas.begin(); it != selCarpetas.end(); )
            if (!cs.count(*it)) selCarpetas.erase(it++); else ++it;
    }
}

// las carpetas plegadas de la vista que se mira
static std::set<std::string>& PlegadasDe(Outliner* o) { return o->VistaSoloLectura() ? o->plegadasExt : o->plegadas; }

// ============================================================================
//  LA GEOMETRIA (lista y cuadricula)
// ============================================================================
int Outliner::CeldaTam() const { return 44 * GlobalScale; }

// LISTA: la fila i ocupa una franja de RenglonHeightGS. CUADRICULA: las carpetas son franjas (con su
// sangria, como en la lista) y los recursos de una misma carpeta fluyen en CELDAS cuadradas (miniatura +
// nombre) de izquierda a derecha; una carpeta nueva (o el fin de la tanda) empieza otra franja.
// Coordenadas del PANEL sin el scroll (quien dibuja o pickea suma PosY; la cuadricula no scrollea en X).
void Outliner::RectFila(const std::vector<OutFilaRec>& filas, int i, int& rx, int& ry, int& rw, int& rh) const {
    const int top = borderGS + BarTopOffset();
    const int alto = (int)RenglonHeightGS;
    rx = 0; rw = width; rh = alto; ry = top + i * alto;
    if (!cuadricula) return;
    const int celda = CeldaTam();
    const int reserva = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    const int limite = width - marginGS - reserva;
    int yy = top, xx = marginGS, altoTanda = 0;
    for (int k = 0; k <= i && k < (int)filas.size(); k++) {
        const OutFilaRec& f = filas[(size_t)k];
        if (f.carpeta) {
            if (altoTanda) { yy += altoTanda; altoTanda = 0; }
            if (k == i) { rx = 0; ry = yy; rw = width; rh = alto; return; }
            yy += alto;
            continue;
        }
        const int sangria = f.prof * (IconSizeGS + gapGS);
        if (altoTanda == 0) { xx = marginGS + sangria; altoTanda = celda + gapGS; }
        else if (xx + celda > limite) { yy += altoTanda; xx = marginGS + sangria; }
        if (k == i) { rx = xx; ry = yy; rw = celda; rh = celda; return; }
        xx += celda + gapGS;
    }
}
int Outliner::FilaEnPunto(const std::vector<OutFilaRec>& filas, int mx, int my) const {
    const int ly = my - y - PosY;
    if (!cuadricula) {
        const int rel = ly - borderGS - BarTopOffset();
        if (rel < 0) return -1;
        const int f = rel / (int)RenglonHeightGS;
        return (f >= 0 && f < (int)filas.size()) ? f : -1;
    }
    const int lx = mx - x;
    for (size_t i = 0; i < filas.size(); i++) {
        int rx, ry, rw, rh;
        RectFila(filas, (int)i, rx, ry, rw, rh);
        if (lx >= rx && lx < rx + rw && ly >= ry && ly < ry + rh) return (int)i;
    }
    return -1;
}

// ============================================================================
//  LA SELECCION MULTIPLE
// ============================================================================
bool Outliner::FilaEnSeleccion(const OutFilaRec& f) const {
    return f.carpeta ? selCarpetas.count(f.clave) > 0 : selRecursos.count(f.clave) > 0;
}
void Outliner::SeleccionSolo(const OutFilaRec& f) {
    selRecursos.clear(); selCarpetas.clear();
    selMarcada = false;
    if (f.carpeta) selCarpetas.insert(f.clave); else selRecursos.insert(f.clave);
    g_redraw = true;
}
void Outliner::SeleccionAlternar(const OutFilaRec& f) {
    // (un click comun deja la fila sola en la seleccion: Ctrl+click en otra = las dos)
    std::set<std::string>& s = f.carpeta ? selCarpetas : selRecursos;
    if (s.count(f.clave)) s.erase(f.clave); else s.insert(f.clave);
    selMarcada = true;
    g_redraw = true;
}
bool Outliner::SoltarSeleccion() {
    if (!selMarcada && SeleccionCantidad() <= 1) return false;
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    const int fc = FilaDelCursor(filas);
    selRecursos.clear(); selCarpetas.clear(); selMarcada = false;
    if (fc >= 0) SeleccionSolo(filas[(size_t)fc]);   // queda el cursor
    g_redraw = true;
    return true;
}
void Outliner::InvertirSeleccionRec() {
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    std::set<std::string> rs, cs;
    for (size_t i = 0; i < filas.size(); i++) {
        if (FilaEnSeleccion(filas[i])) continue;
        if (filas[i].carpeta) cs.insert(filas[i].clave); else rs.insert(filas[i].clave);
    }
    selRecursos.swap(rs); selCarpetas.swap(cs);
    selMarcada = true;
    g_redraw = true;
}
bool Outliner::TeclaBorrarRecursos() {
    if (vista == OUT_VISTA_ESCENA) return false;
    if (!VistaSoloLectura()) AccionBorrar(true);   // (con confirmacion)
    return true;   // en la biblioteca el C nunca borra objetos de la escena
}
bool Outliner::MarcarCursor() {
    if (vista == OUT_VISTA_ESCENA) return false;
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    const int fc = FilaDelCursor(filas);
    if (fc < 0) return false;
    // la PRIMERA marca arranca la seleccion con el cursor (la que lo seguia pasa a ser propia);
    // despues, el 5 marca o desmarca la fila del cursor
    if (!selMarcada) { SeleccionSolo(filas[(size_t)fc]); selMarcada = true; }
    else SeleccionAlternar(filas[(size_t)fc]);
    return true;
}
void Outliner::SeleccionarTodoRec() {
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    bool todas = !filas.empty();
    for (size_t i = 0; i < filas.size() && todas; i++) todas = FilaEnSeleccion(filas[i]);
    selRecursos.clear(); selCarpetas.clear();
    selMarcada = !todas;
    if (!todas)
        for (size_t i = 0; i < filas.size(); i++) {
            if (filas[i].carpeta) selCarpetas.insert(filas[i].clave); else selRecursos.insert(filas[i].clave);
        }
    g_redraw = true;
}
void Outliner::Objetivo(std::vector<std::string>& recursos, std::vector<std::string>& carpetas, bool paraMover) {
    recursos.clear(); carpetas.clear();
    std::vector<OutFilaRec> filas; FilasRecursos(filas);   // (poda la seleccion)
    std::set<std::string> rs = selRecursos, cs = selCarpetas;
    if (rs.empty() && cs.empty()) {
        const int fc = FilaDelCursor(filas);
        if (fc < 0) return;
        if (filas[(size_t)fc].carpeta) cs.insert(filas[(size_t)fc].clave); else rs.insert(filas[(size_t)fc].clave);
    }
    if (VistaSoloLectura() && paraMover) return;   // (una libreria externa no se toca)
    // las carpetas: sin las que cuelgan de otra elegida (se mudan con ella)
    for (std::set<std::string>::iterator it = cs.begin(); it != cs.end(); ++it) {
        bool adentro = false;
        for (std::set<std::string>::iterator o = cs.begin(); o != cs.end() && !adentro; ++o)
            adentro = (*o != *it && W3dCarpetaAdentro(*it, *o));
        if (!paraMover || !adentro) carpetas.push_back(*it);
    }
    // los recursos: por su clave (tambien los que no se ven: adentro de una carpeta plegada)
    std::vector<W3dRecursoItem> items;
    if (!VistaSoloLectura()) W3dBibliotecaListar(-1, items);
    else { std::vector<std::string> cr; W3dLibreriaListar(vista - OUT_VISTA_LIBEXT, items, cr, NULL); }
    for (size_t i = 0; i < items.size(); i++) {
        const std::string k = W3dBibClave(items[i].tipo, items[i].id);
        if (!rs.count(k)) continue;
        if (paraMover) {
            if (items[i].soloLectura || !items[i].padre.empty()) continue;   // (una parte va con su recurso)
            bool lleva = false;   // lo lleva una carpeta elegida
            for (size_t c = 0; c < carpetas.size() && !lleva; c++) lleva = W3dCarpetaAdentro(items[i].carpeta, carpetas[c]);
            if (lleva) continue;
        }
        recursos.push_back(k);
    }
}

int Outliner::FilaDelCursor(const std::vector<OutFilaRec>& filas) const {
    for (size_t i = 0; i < filas.size(); i++)
        if (filas[i].carpeta == cursorEsCarpeta && filas[i].clave == cursorClave) return (int)i;
    return -1;
}

int Outliner::FilaRecursoY(int fila) const {
    if (!cuadricula) return y + borderGS + PosY + BarTopOffset() + fila * (int)RenglonHeightGS + (int)RenglonHeightGS / 2;
    std::vector<OutFilaRec> filas;
    const_cast<Outliner*>(this)->FilasRecursos(filas);
    int rx, ry, rw, rh;
    RectFila(filas, fila, rx, ry, rw, rh);
    return y + PosY + ry + rh / 2;
}

// ============================================================================
//  LA BARRA: [0] tipo  [1] VISTA (ojo)  [2] OBJETO  [3] SELECCION  [4] "+"  [5] filtro
//            [6] lista/cuadricula  [7] raiz (en la vista Escena)
// ============================================================================
std::string Outliner::VistaClave() const {
    if (vista == OUT_VISTA_ESCENA) return "escena";
    if (vista == OUT_VISTA_BIBLIOTECA) return "biblioteca";
    return "lib:" + W3dLibreriaRuta(vista - OUT_VISTA_LIBEXT);
}
bool Outliner::VistaDeClave(const std::string& c) {
    if (c == "escena") { CambiarVista(OUT_VISTA_ESCENA); return true; }
    if (c == "biblioteca") { CambiarVista(OUT_VISTA_BIBLIOTECA); return true; }
    if (c.compare(0, 4, "lib:") == 0) {
        for (int i = 0; i < W3dLibreriasCantidad(); i++)
            if (W3dLibreriaRuta(i) == c.substr(4)) { CambiarVista(OUT_VISTA_LIBEXT + i); return true; }
        return false;
    }
    // el layout de la fase anterior ("vista: materiales"): la biblioteca con ese filtro
    const int t = W3dVistaDeClave(c);
    if (t > W3D_VISTA_ESCENA) { CambiarVista(OUT_VISTA_BIBLIOTECA); CambiarFiltro(t); return true; }
    return false;
}

static std::string TituloVista(int v) {
    if (v == OUT_VISTA_ESCENA) return T("Scene");
    if (v == OUT_VISTA_BIBLIOTECA) return T("Library");
    return W3dLibreriaNombre(v - OUT_VISTA_LIBEXT);
}

void Outliner::SincronizarBarraVista() {
    if (!btnVista) return;
    // una libreria que se desvinculo (o un undo) mientras se la miraba: vuelve a la biblioteca
    if (vista >= OUT_VISTA_LIBEXT && vista - OUT_VISTA_LIBEXT >= W3dLibreriasCantidad()) {
        vista = OUT_VISTA_BIBLIOTECA;
        plegadasExt.clear();
        lastContentRows = -1;
    }
    const std::string t = TituloVista(vista);
    if (btnVista->text != t) { btnVista->text = t; g_redraw = true; }
    btnVista->icon = (int)IconType::visible;   // el OJO: "lo que se ve en el outliner"
    if (btnObjeto) btnObjeto->text = T("Object");
    if (btnSeleccion) btnSeleccion->text = T("Select");
    const bool bib = EnBiblioteca();
    if (btnFiltro) {
        if (btnFiltro->visible != bib) { btnFiltro->visible = bib; g_redraw = true; }
        const std::string ft = filtro < 0 ? std::string(T("All")) : std::string(T(W3dVistaTitulo(filtro)));
        if (btnFiltro->text != ft) { btnFiltro->text = ft; g_redraw = true; }
        btnFiltro->icon = (int)IconType::filtro;
    }
    if (btnCuadricula) {
        if (btnCuadricula->visible != bib) { btnCuadricula->visible = bib; g_redraw = true; }
        // el icono dice a que se PASA con el click (como un interruptor): en lista, la cuadricula
        btnCuadricula->icon = cuadricula ? (int)IconType::lista : (int)IconType::cuadricula;
    }
    if (btnRaiz) {
        const bool ver = !bib;
        if (btnRaiz->visible != ver) { btnRaiz->visible = ver; g_redraw = true; }
        // la raiz que se edita (escena o prefab): su icono y su nombre, como en la barra del 3D ('*' = sin guardar)
        W3dRaicesBotonSincronizar(btnRaiz);
    }
}

void Outliner::CambiarVista(int v) {
    if (v < OUT_VISTA_ESCENA || v >= OUT_VISTA_LIBEXT + W3dLibreriasCantidad()) return;
    if (moviendo) MoverCancelar();                   // el modo mover es de la vista que se deja
    if (moviendoRec) MoverRecCancelar();
    if (renombrando) RenombreSincronizar();
    if (v != vista && v >= OUT_VISTA_LIBEXT) plegadasExt.clear();
    vista = v;
    PosY = 0; PosX = 0;
    hoverFila = -1;
    recArrastre = recArrastrando = recArrastreCarpeta = recSoloAlSoltar = false;
    recDropFila = -1;
    agarreFila = -1;
    dragObjeto = NULL; dragging = false; dropZona = -2;
    cursorEsCarpeta = false;
    cursorClave.clear();
    selRecursos.clear(); selCarpetas.clear();   // la seleccion es de la vista que se deja
    selMarcada = false;
    lastContentRows = -1;                            // el proximo Render rearma el scroll
    SincronizarBarraVista();
    if (width > 0 && height > 0) Resize(width, height);
    g_redraw = true;
}
void Outliner::CambiarFiltro(int f) {
    if (f != -1 && (f <= W3D_VISTA_ESCENA || f >= W3D_VISTAS)) return;
    if (moviendoRec) MoverRecCancelar();
    filtro = f;
    PosY = 0;
    lastContentRows = -1;
    SincronizarBarraVista();
    if (width > 0 && height > 0) Resize(width, height);
    g_redraw = true;
}
void Outliner::CambiarCuadricula(bool on) {
    if (cuadricula == on) return;
    cuadricula = on;
    PosY = 0; PosX = 0;
    lastContentRows = -1;
    SincronizarBarraVista();
    if (width > 0 && height > 0) Resize(width, height);
    g_redraw = true;
}

// ---- los menus (uno de cada, compartidos: los arma el outliner que los abre) ----
static PopupMenu* gMenuVista = NULL;
static PopupMenu* gMenuObjeto = NULL;
static PopupMenu* gMenuSeleccion = NULL;
static PopupMenu* gMenuMover = NULL;
static PopupMenu* gMenuRaiz = NULL;                  // el selector de escena/juego/prefab ([7])
static PopupMenu* gMenuNueva = NULL;                 // el "+": nueva escena / juego / prefab / carpeta / material
static PopupMenu* gMenuFiltro = NULL;
static PopupMenu* gMenuDesvincular = NULL;           // submenu de la vista: las librerias a desvincular
static Outliner*  gMenuDe = NULL;                    // el outliner que abrio el menu
static std::vector<std::string> gMoverCarpetas;      // id del menu Mover - 101 -> carpeta

// el outliner que abrio un menu se borro (cambio de tipo, abrir un proyecto): se olvida
static void OlvidarOutliner(ViewportBase* muerto) {
    if ((ViewportBase*)gMenuDe == muerto) {
        gMenuDe = NULL;
        if (MenuAbierto && (MenuAbierto == gMenuVista || MenuAbierto == gMenuObjeto || MenuAbierto == gMenuMover ||
                            MenuAbierto == gMenuRaiz || MenuAbierto == gMenuNueva || MenuAbierto == gMenuSeleccion ||
                            MenuAbierto == gMenuFiltro || MenuAbierto == gMenuDesvincular))
            MenuAbierto->Cerrar();
    }
}
static bool gGanchoOk = ViewportOlvidarRegistrar(OlvidarOutliner);

// ---- VISTA: Escena / Biblioteca / cada libreria + vincular / desvincular ----
enum { VIS_VINCULAR = 900, VIS_CUADRICULA = 901, VIS_DESVINCULAR = 950 };
static void LibreriaElegida(const std::string& ruta) {
    std::string motivo;
    if (!W3dLibreriaVincular(ruta, &motivo)) { Notificar(std::string(T("Not linked")) + ": " + T(motivo.c_str()), true); return; }
    Notificar(std::string(T("Library linked")) + ": " + ruta, false);
    if (gMenuDe) gMenuDe->CambiarVista(OUT_VISTA_LIBEXT + W3dLibreriasCantidad() - 1);
    g_redraw = true;
}
// DESVINCULAR (con Ctrl+Z). Lo que el proyecto usaba de ella no queda colgando (W3dLibreriaDesvincular)
static int gDesvPendiente = -1;   // la que espera la confirmacion (ConfirmarPopup llama un void() sin argumentos)
static void DesvincularAhora(int i) {
    const std::string n = W3dLibreriaNombre(i);
    int copias = 0;
    if (!W3dLibreriaDesvincular(i, &copias)) return;
    std::string aviso = std::string(T("Library unlinked")) + ": " + n;
    if (copias > 0) {
        char b[160];
        snprintf(b, sizeof(b), T("%d object(s) got their own copy of the mesh"), copias);
        aviso += std::string(" (") + b + ")";
    }
    Notificar(aviso, false);
    if (gMenuDe && gMenuDe->vista >= OUT_VISTA_LIBEXT) gMenuDe->CambiarVista(OUT_VISTA_BIBLIOTECA);
    g_redraw = true;
}
static void DesvincularConfirmado() {
    const int i = gDesvPendiente;
    gDesvPendiente = -1;
    if (i >= 0 && i < W3dLibreriasCantidad()) DesvincularAhora(i);
}
static void AccionVistaElegida(int id) {
    Outliner* o = gMenuDe;
    if (!o) return;
    if (id == VIS_VINCULAR) {
        // el EXPLORADOR de archivos de siempre, filtrado a .w3d
        AbrirFileBrowser(T("Link .w3d..."), T("Link"), ".w3d", LibreriaElegida, false);
        return;
    }
    if (id == VIS_CUADRICULA) { o->CambiarCuadricula(!o->cuadricula); return; }   // (el teclado del N95)
    if (id >= VIS_DESVINCULAR) {
        const int i = id - VIS_DESVINCULAR;
        // el PROYECTO usa mallas suyas (soltadas desde su vista): se avisa ANTES y se ofrece (Si = cada una pasa a
        // su copia propia, con Ctrl+Z; No = no se desvincula). Sin usos se desvincula directo
        const int usos = W3dLibreriaUsosProyecto(i);
        if (usos > 0) {
            gDesvPendiente = i;
            if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
            char b[512];
            snprintf(b, sizeof(b), T("%d object(s) of the project use 3D meshes of \"%s\": each one gets its own copy (Ctrl+Z undoes it). Unlink it?"),
                     usos, W3dLibreriaNombre(i).c_str());
            confirmarPopup->Abrir(b, DesvincularConfirmado);
            return;
        }
        DesvincularAhora(i);
        return;
    }
    o->CambiarVista(id);
}
static void ArmarMenuVista(Outliner* o) {
    if (!gMenuVista) { gMenuVista = new PopupMenu(); gMenuVista->action = AccionVistaElegida; }
    if (!gMenuDesvincular) { gMenuDesvincular = new PopupMenu(); gMenuDesvincular->action = AccionVistaElegida; }
    gMenuVista->Limpiar();
    MenuItem* it = gMenuVista->Agregar(T("Scene"), OUT_VISTA_ESCENA, W3dRaizIcono(W3dRaizTipoActiva()));
    if (it) it->verde = (o->vista == OUT_VISTA_ESCENA);
    it = gMenuVista->Agregar(T("Library"), OUT_VISTA_BIBLIOTECA, IconType::carpeta);
    if (it) it->verde = (o->vista == OUT_VISTA_BIBLIOTECA);
    gMenuDesvincular->Limpiar();
    for (int i = 0; i < W3dLibreriasCantidad(); i++) {
        it = gMenuVista->Agregar(W3dLibreriaNombre(i), OUT_VISTA_LIBEXT + i, IconType::libreria);
        if (it) it->verde = (o->vista == OUT_VISTA_LIBEXT + i);
        gMenuDesvincular->Agregar(W3dLibreriaNombre(i), VIS_DESVINCULAR + i, IconType::libreria);
    }
    // lista / cuadricula (lo mismo que el interruptor de la barra: asi tambien se alterna sin mouse)
    if (o->EnBiblioteca()) {
        it = gMenuVista->Agregar(T("Grid"), VIS_CUADRICULA, IconType::cuadricula);
        if (it) it->verde = o->cuadricula;
    }
    gMenuVista->Agregar(T("Link .w3d..."), VIS_VINCULAR, IconType::mas);
    if (W3dLibreriasCantidad() > 0) gMenuVista->Agregar(T("Unlink"), 0, IconType::borrar, gMenuDesvincular);
}

// ---- OBJETO (Escena: el objeto activo / la seleccion; Biblioteca: el cursor / la seleccion) ----
enum { OBJ_BORRAR = 1, OBJ_RENOMBRAR, OBJ_MOVER, OBJ_OCULTAR, OBJ_DUPLICAR, OBJ_DUPLICAR_VINC,
       REC_BORRAR, REC_RENOMBRAR, REC_DUPLICAR, REC_USUARIOS, REC_PURGAR, REC_NUEVA_CARPETA,
       OBJ_CREAR_PREFAB, OBJ_DESEMPAQUETAR };
static void AccionMoverElegida(int id) {
    if (!gMenuDe) return;
    if (id == 100) gMenuDe->AccionMover(std::string());
    else if (id > 100 && id - 101 < (int)gMoverCarpetas.size()) gMenuDe->AccionMover(gMoverCarpetas[(size_t)(id - 101)]);
}
// el Aceptar del modal de mover que un duplicado deja abierto (desde el outliner no hay mouse en el 3D)
static void AceptarModalDuplicado() {
    extern Viewport3D* Viewport3DActive;
    if (estado == editNavegacion) return;
    if (Viewport3DActive) Viewport3DActive->Aceptar();
    else estado = editNavegacion;
}
// Shift+D / Alt+D desde el outliner (el menu Objeto y sus atajos con el mouse sobre el panel)
void Outliner::DuplicarEscena(bool vinculado) {
    extern void DuplicatedObject();
    extern void NewInstance();
    if (vinculado) NewInstance(); else DuplicatedObject();
    AceptarModalDuplicado();
    g_redraw = true;
}
static void AccionObjetoElegida(int id) {
    Outliner* o = gMenuDe;
    if (!o) return;
    switch (id) {
        case OBJ_BORRAR:   AbrirConfirmarBorrado(true); break;
        case OBJ_RENOMBRAR: o->AccionRenombrar(); break;
        case OBJ_MOVER:    o->MoverIniciar(); break;
        case OBJ_OCULTAR:  UndoCapturarVisibilidad(); ChangeVisibilityObj(); break;
        case OBJ_DUPLICAR: o->DuplicarEscena(false); break;
        case OBJ_DUPLICAR_VINC: o->DuplicarEscena(true); break;
        case OBJ_CREAR_PREFAB: { std::string nom, motivo;   // lo elegido pasa a un prefab nuevo (queda una instancia)
            if (!W3dPrefabCrearDesdeSeleccion(&nom, &motivo)) Notificar(std::string(T(motivo.c_str())), true);
            else Notificar(std::string(T("Prefab created")) + ": " + nom, false);
            break; }
        case OBJ_DESEMPAQUETAR: { std::string motivo;          // la instancia activa pasa a objetos comunes
            InstanciaPrefab* ip = (ObjActivo && ObjActivo->getType() == ObjectType::prefab) ? (InstanciaPrefab*)ObjActivo : NULL;
            if (!ip) Notificar(std::string(T("Select a prefab instance")), true);
            else if (!W3dPrefabDesempaquetar(ip, &motivo)) Notificar(std::string(T(motivo.c_str())), true);
            break; }
        case REC_BORRAR:   o->AccionBorrar(true); break;
        case REC_RENOMBRAR: o->AccionRenombrar(); break;
        case REC_DUPLICAR: o->AccionDuplicarRecurso(); break;
        case REC_USUARIOS: o->AccionSeleccionarUsuarios(); break;
        case REC_PURGAR:   o->AccionPurgar(true); break;
        case REC_NUEVA_CARPETA: o->AccionNuevaCarpeta(); break;
        default: AccionMoverElegida(id); break;   // el submenu "Move to Folder"
    }
    g_redraw = true;
}

// arma el submenu con las carpetas de la biblioteca ("(root)" + cada carpeta, con su sangria). Las
// carpetas que se MUEVEN (y lo que cuelga de ellas) no se ofrecen: no hay como meterlas en si mismas.
// 'actual' = la carpeta de donde sale (tilde verde) si es una sola.
static void ArmarMenuMover(PopupMenu* m, const std::string& actual, const std::vector<std::string>& seMueven) {
    m->Limpiar();
    m->titulo.clear();   // como submenu no lleva cabecera (AbrirMenuMover se la pone suelto)
    gMoverCarpetas.clear();
    MenuItem* raiz = m->Agregar(std::string("(") + T("root") + ")", 100, IconType::carpeta);
    if (raiz) raiz->verde = actual.empty();
    std::vector<std::string> cs;
    W3dCarpetasTodas(cs);
    for (size_t i = 0; i < cs.size(); i++) {
        bool fuera = false;
        for (size_t k = 0; k < seMueven.size() && !fuera; k++) fuera = W3dCarpetaAdentro(cs[i], seMueven[k]);
        if (fuera) continue;
        const int id = 101 + (int)gMoverCarpetas.size();
        gMoverCarpetas.push_back(cs[i]);
        int prof = 0;
        for (size_t k = 0; k < cs[i].size(); k++) if (cs[i][k] == '/') prof++;
        MenuItem* it = m->Agregar(std::string((size_t)prof * 2, ' ') + W3dCarpetaHoja(cs[i]), id, IconType::carpeta);
        if (it) it->verde = (cs[i] == actual);
    }
}
// la carpeta de donde sale lo elegido (para el tilde): la de todos si es la misma, "\x01" si no
static std::string CarpetaDeOrigen(const std::vector<std::string>& recursos, const std::vector<std::string>& carpetas) {
    std::string c; bool hay = false;
    for (size_t k = 0; k < recursos.size(); k++) {
        int t = 0; std::string id; W3dRecursoItem it;
        if (!W3dBibDeClave(recursos[k], &t, &id) || !W3dVistaRecInfo(t, id, &it)) continue;
        if (hay && c != it.carpeta) return std::string("\x01");
        c = it.carpeta; hay = true;
    }
    for (size_t k = 0; k < carpetas.size(); k++) {
        const std::string p = W3dCarpetaPadre(carpetas[k]);
        if (hay && c != p) return std::string("\x01");
        c = p; hay = true;
    }
    return c;
}

// abre un menu justo debajo de un boton de la barra (como los de la barra del 3D)
static void AbrirBajo(PopupMenu* m, Button* b) {
    if (MenuAbierto && MenuAbierto != m) MenuAbierto->Cerrar();
    m->Abrir(b->sx, b->sy + b->height - GlobalScale, MenuPantallaW, MenuPantallaH);
    MenuAbierto = m;
}
static MenuItem* ConAtajo(MenuItem* it, const char* atajo) { if (it) it->atajo = atajo; return it; }
// un item DESHABILITADO (gris, no responde): el menu mira un bool que en false = gris
static void Gris(MenuItem* it, bool deshabilitado) { static bool kHabilitado = false; if (it && deshabilitado) it->gris = &kHabilitado; }

// el menu OBJETO segun la vista, el cursor y la seleccion (lo que esta resaltado)
static void ArmarMenuObjeto(Outliner* o) {
    if (!gMenuObjeto) { gMenuObjeto = new PopupMenu(); gMenuObjeto->action = AccionObjetoElegida; }
    if (!gMenuMover) { gMenuMover = new PopupMenu(); gMenuMover->action = AccionMoverElegida; }
    gMenuObjeto->Limpiar();
    gMenuObjeto->titulo.clear();
    if (o->vista == OUT_VISTA_ESCENA) {
        const bool hay = (ObjActivo != NULL) || !ObjSelects.empty();
        MenuItem* it;
        it = ConAtajo(gMenuObjeto->Agregar(T("Delete"), OBJ_BORRAR, IconType::borrar), "X");
        Gris(it, !hay);
        it = ConAtajo(gMenuObjeto->Agregar(T("Rename"), OBJ_RENOMBRAR, IconType::lista), "F2");
        Gris(it, (ObjActivo == NULL));
        it = ConAtajo(gMenuObjeto->Agregar(T("Move"), OBJ_MOVER, IconType::arrowRight), "G");
        Gris(it, (ObjActivo == NULL));
        it = ConAtajo(gMenuObjeto->Agregar(T("Hide/Show"), OBJ_OCULTAR, IconType::visible), "H");
        Gris(it, !hay);
        it = ConAtajo(gMenuObjeto->Agregar(T("Duplicate"), OBJ_DUPLICAR, IconType::object), "Shift+D");
        Gris(it, !hay);
        it = ConAtajo(gMenuObjeto->Agregar(T("Duplicate Linked"), OBJ_DUPLICAR_VINC, IconType::instance), "Alt+D");
        Gris(it, !hay);
        // PREFABS: lo elegido pasa a un prefab nuevo / la instancia activa se desempaqueta
        it = gMenuObjeto->Agregar(T("Create Prefab"), OBJ_CREAR_PREFAB, IconType::prefab);
        Gris(it, !hay);
        it = gMenuObjeto->Agregar(T("Unpack Prefab"), OBJ_DESEMPAQUETAR, IconType::prefab);
        Gris(it, !(ObjActivo && ObjActivo->getType() == ObjectType::prefab));
        return;
    }
    std::vector<OutFilaRec> filas;
    o->FilasRecursos(filas);
    const int fc = o->FilaDelCursor(filas);
    const OutFilaRec* f = (fc >= 0) ? &filas[(size_t)fc] : NULL;
    std::vector<std::string> recM, carM, rec, car;
    o->Objetivo(recM, carM, true);
    o->Objetivo(rec, car, false);
    const bool ro = o->VistaSoloLectura();
    MenuItem* it;
    it = ConAtajo(gMenuObjeto->Agregar(T("Delete"), REC_BORRAR, IconType::borrar), "X");
    Gris(it, ro || (rec.empty() && car.empty()));
    it = ConAtajo(gMenuObjeto->Agregar(T("Rename"), REC_RENOMBRAR, IconType::lista), "F2");
    Gris(it, ro || !f || (!f->carpeta && !f->renombrable));
    if (!ro && (!recM.empty() || !carM.empty())) {
        ArmarMenuMover(gMenuMover, CarpetaDeOrigen(recM, carM), carM);
        ConAtajo(gMenuObjeto->Agregar(T("Move to Folder"), 0, IconType::carpeta, gMenuMover), "G");
    } else {
        it = ConAtajo(gMenuObjeto->Agregar(T("Move to Folder"), 0, IconType::carpeta), "G");
        Gris(it, true);
    }
    it = gMenuObjeto->Agregar(T("Duplicate Resource"), REC_DUPLICAR, IconType::object);
    Gris(it, ro || !f || f->carpeta || f->parte);
    // (solo si alguno de sus usuarios es un OBJETO: una malla huerfana, un script o un flipbook
    // cuentan como usuarios, pero no hay nada que seleccionar)
    it = gMenuObjeto->Agregar(T("Select Users"), REC_USUARIOS, IconType::seleccion);
    Gris(it, ro || rec.empty() || W3dBibObjetosUsuarios(rec) == 0);
    it = gMenuObjeto->Agregar(T("Purge Orphans"), REC_PURGAR, IconType::notifError);
    Gris(it, ro);
}

// ---- SELECCION (vale en las dos vistas) ----
enum { SEL_TODO = 1, SEL_NADA, SEL_INVERTIR };
// la vista Escena: 0 todo, 1 nada, 2 invertir (el menu Seleccion y sus atajos A / Alt+A / Ctrl+I), con undo
void Outliner::SeleccionEscena(int accion) {
    extern void SeleccionarTodoForzado(bool);
    UndoCapturarSeleccion();
    if (accion == 0) SeleccionarTodoForzado(true);
    else if (accion == 1) { DeseleccionarTodo(true); ObjActivo = NULL; }
    else {
        // lo que estaba seleccionado deja de estarlo y al reves (solo lo que se VE en el arbol)
        std::vector<Object*> todos;
        struct R { static void Juntar(Object* p, std::vector<Object*>& out) {
            for (size_t i = 0; i < p->Childrens.size(); i++) { if (!p->Childrens[i]) continue; out.push_back(p->Childrens[i]); Juntar(p->Childrens[i], out); } } };
        if (SceneCollection) R::Juntar(SceneCollection, todos);
        std::set<Object*> eran;
        for (size_t i = 0; i < todos.size(); i++) if (todos[i]->select) eran.insert(todos[i]);
        DeseleccionarTodo(true);
        ObjActivo = NULL;
        for (size_t i = 0; i < todos.size(); i++) if (!eran.count(todos[i])) todos[i]->Seleccionar();
    }
    g_redraw = true;
}
static void AccionSeleccionElegida(int id) {
    Outliner* o = gMenuDe;
    if (!o) return;
    if (o->vista == OUT_VISTA_ESCENA) {
        o->SeleccionEscena(id == SEL_TODO ? 0 : id == SEL_NADA ? 1 : 2);
    } else {
        if (id == SEL_TODO) {
            // todas (no el toggle de la A)
            std::vector<OutFilaRec> filas; o->FilasRecursos(filas);
            o->selRecursos.clear(); o->selCarpetas.clear();
            for (size_t i = 0; i < filas.size(); i++) {
                if (filas[i].carpeta) o->selCarpetas.insert(filas[i].clave); else o->selRecursos.insert(filas[i].clave);
            }
            o->selMarcada = true;
        } else if (id == SEL_NADA) {
            o->SoltarSeleccion();   // suelta las marcas; queda el cursor (el recurso que muestra Properties)
        } else o->InvertirSeleccionRec();
    }
    g_redraw = true;
}
static void ArmarMenuSeleccion(Outliner* o) {
    (void)o;
    if (!gMenuSeleccion) { gMenuSeleccion = new PopupMenu(); gMenuSeleccion->action = AccionSeleccionElegida; }
    gMenuSeleccion->Limpiar();
    ConAtajo(gMenuSeleccion->Agregar(T("Select All"), SEL_TODO, IconType::seleccion), "A");
    ConAtajo(gMenuSeleccion->Agregar(T("Deselect All"), SEL_NADA, IconType::seleccion), "Alt+A");
    ConAtajo(gMenuSeleccion->Agregar(T("Invert Selection"), SEL_INVERTIR, IconType::seleccion), "Ctrl+I");
}

// ---- "+": las raices nuevas (RaicesEditor) + carpeta y material de la biblioteca ----
enum { NUEVA_CARPETA = 4100, NUEVO_MATERIAL };
static void AccionNuevaElegida(int id) {
    Outliner* o = gMenuDe;
    if (id == NUEVA_CARPETA) {
        if (!o) return;
        if (o->vista == OUT_VISTA_ESCENA || o->VistaSoloLectura()) o->CambiarVista(OUT_VISTA_BIBLIOTECA);
        o->AccionNuevaCarpeta();
        return;
    }
    if (id == NUEVO_MATERIAL) {
        if (!o) return;
        if (o->vista == OUT_VISTA_ESCENA || o->VistaSoloLectura()) o->CambiarVista(OUT_VISTA_BIBLIOTECA);
        o->AccionNuevoMaterial();
        return;
    }
    W3dRaicesMenuAccion(id);
}

// ---- FILTRO ----
static void AccionFiltroElegido(int id) { if (gMenuDe) gMenuDe->CambiarFiltro(id == 0 ? -1 : id); }
static void ArmarMenuFiltro(Outliner* o) {
    if (!gMenuFiltro) { gMenuFiltro = new PopupMenu(); gMenuFiltro->action = AccionFiltroElegido; }
    gMenuFiltro->Limpiar();
    MenuItem* it = gMenuFiltro->Agregar(T("All"), 0, IconType::filtro);
    if (it) it->verde = (o->filtro < 0);
    std::vector<int> tipos;
    W3dBibliotecaTipos(tipos);
    for (size_t i = 0; i < tipos.size(); i++) {
        it = gMenuFiltro->Agregar(T(W3dVistaTitulo(tipos[i])), tipos[i], W3dVistaIcono(tipos[i]));
        if (it) it->verde = (o->filtro == tipos[i]);
    }
}

bool Outliner::AbrirMenuBoton(int idx) {
    if (idx < 0 || idx >= (int)BarButtons.size()) return false;
    Button* b = BarButtons[(size_t)idx];
    if (!b || !b->visible) return false;
    SincronizarBarraVista();
    barFocusIndex = idx;       // resaltar el boton + auto-scroll de la barra
    ActualizarBarra();         // sx/sy frescos
    gMenuDe = this;
    if (b == btnVista) { ArmarMenuVista(this); AbrirBajo(gMenuVista, b); return true; }
    if (b == btnObjeto) { ArmarMenuObjeto(this); AbrirBajo(gMenuObjeto, b); return true; }
    if (b == btnSeleccion) { ArmarMenuSeleccion(this); AbrirBajo(gMenuSeleccion, b); return true; }
    if (b == btnFiltro) { ArmarMenuFiltro(this); AbrirBajo(gMenuFiltro, b); return true; }
    if (b == btnRaiz) {
        if (!gMenuRaiz) { gMenuRaiz = new PopupMenu(); gMenuRaiz->action = W3dRaicesMenuAccion; }
        W3dRaicesMenuArmar(gMenuRaiz);
        AbrirBajo(gMenuRaiz, b);
        return true;
    }
    if (b == btnNueva) {
        if (!gMenuNueva) { gMenuNueva = new PopupMenu(); gMenuNueva->action = AccionNuevaElegida; }
        W3dRaicesMenuArmarNuevas(gMenuNueva);
        gMenuNueva->Agregar(T("New Folder"), NUEVA_CARPETA, IconType::carpeta);
        gMenuNueva->Agregar(T("New Material"), NUEVO_MATERIAL, IconType::material);
        gMenuNueva->titulo = T("Add");   // el boton es un icono sin texto -> el menu lleva titulo (en el idioma de ahora)
        AbrirBajo(gMenuNueva, b);
        return true;
    }
    if (b == btnCuadricula) {   // (sin menu: el teclado del N95 lo "abre" = lo alterna)
        CambiarCuadricula(!cuadricula);
        return true;
    }
    return false;
}

bool Outliner::AbrirMenuDeBarra(int mx, int my) {
    SincronizarBarraVista();
    ActualizarBarra();
    for (size_t i = 1; i < BarButtons.size(); i++) {
        Button* b = BarButtons[i];
        if (!b || !b->visible || !b->Contains(mx, my)) continue;
        if (b == btnCuadricula) return false;   // (es un interruptor: se aprieta con ClickBarra, no con el hover)
        // ya abierto (el hover que se desliza sobre el mismo boton): no se reabre
        const int ab = BotonDelMenuAbierto();
        if (ab == (int)i) return true;
        return AbrirMenuBoton((int)i);
    }
    return false;
}
bool Outliner::ClickBarra(int mx, int my) {
    ActualizarBarra();
    if (btnCuadricula && btnCuadricula->visible && btnCuadricula->Contains(mx, my)) {
        CambiarCuadricula(!cuadricula);
        return true;
    }
    return false;
}

int Outliner::BotonDelMenuAbierto() const {
    if (gMenuDe != this || !MenuAbierto || !MenuAbierto->abierto) return -1;
    for (size_t i = 0; i < BarButtons.size(); i++) {
        if (BarButtons[i] == btnVista && MenuAbierto == gMenuVista) return (int)i;
        if (BarButtons[i] == btnObjeto && MenuAbierto == gMenuObjeto) return (int)i;
        if (BarButtons[i] == btnSeleccion && MenuAbierto == gMenuSeleccion) return (int)i;
        if (BarButtons[i] == btnRaiz && MenuAbierto == gMenuRaiz) return (int)i;
        if (BarButtons[i] == btnNueva && MenuAbierto == gMenuNueva) return (int)i;
        if (BarButtons[i] == btnFiltro && MenuAbierto == gMenuFiltro) return (int)i;
    }
    return -1;
}

void Outliner::AbrirMenuMover(int mx, int my) {
    std::vector<std::string> recs, cars;
    Objetivo(recs, cars, true);
    if (recs.empty() && cars.empty()) return;   // nada que se pueda mover (solo lectura)
    if (!gMenuMover) { gMenuMover = new PopupMenu(); gMenuMover->action = AccionMoverElegida; }
    gMenuDe = this;
    ArmarMenuMover(gMenuMover, CarpetaDeOrigen(recs, cars), cars);
    gMenuMover->titulo = T("Move to Folder");
    if (MenuAbierto && MenuAbierto != gMenuMover) MenuAbierto->Cerrar();
    gMenuMover->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuMover;
    g_redraw = true;
}

// el MENU CONTEXTUAL (click derecho / pulsacion larga): las mismas acciones que el menu Objeto, en el mouse
bool Outliner::MenuContexto(int mx, int my) {
    if (vista == OUT_VISTA_ESCENA) {
        // la fila bajo el mouse pasa a ser la seleccion (si no estaba en ella), como un click
        const int rel = my - y - borderGS - PosY - BarTopOffset();
        if (rel >= 0) {
            extern Object* OutlinerObjetoEnFila(int fila);
            Object* hit = OutlinerObjetoEnFila(rel / (int)RenglonHeightGS);
            if (hit && !hit->select) { UndoCapturarSeleccion(); DeseleccionarTodo(); hit->Seleccionar(); }
            else if (hit) ObjActivo = hit;
        }
    } else {
        std::vector<OutFilaRec> filas;
        FilasRecursos(filas);
        const int fila = FilaEnPunto(filas, mx, my);
        if (fila >= 0) {
            // sobre una fila de la SELECCION: el menu es de toda la seleccion; si no, queda sola
            if (!FilaEnSeleccion(filas[(size_t)fila])) SeleccionSolo(filas[(size_t)fila]);
            ElegirFila(filas[(size_t)fila], false);
        }
    }
    gMenuDe = this;
    ArmarMenuObjeto(this);
    gMenuObjeto->titulo = T("Object");
    if (MenuAbierto && MenuAbierto != gMenuObjeto) MenuAbierto->Cerrar();
    gMenuObjeto->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenuObjeto;
    g_redraw = true;
    return true;
}

// ============================================================================
//  TAMANO Y DIBUJO
// ============================================================================
void Outliner::ResizeRecursos() {
    std::vector<OutFilaRec> filas;
    FilasRecursos(filas);
    int maxX = 0, maxY = 0;
    if (!cuadricula) {
        for (size_t i = 0; i < filas.size(); i++) {
            const int w = marginGS + (filas[i].prof + 2) * (IconSizeGS + gapGS) + (int)(filas[i].nombre.size() + 1) * LetterWidthGS +
                          gapGS + IconSizeGS + 5 * LetterWidthGS + marginGS;
            if (w > maxX) maxX = w;
        }
        maxY = -(int)filas.size() * (int)RenglonHeightGS - marginGS;
    } else if (!filas.empty()) {
        // la cuadricula no scrollea en X: su alto es el fondo de la ultima celda
        int rx, ry, rw, rh;
        RectFila(filas, (int)filas.size() - 1, rx, ry, rw, rh);
        maxY = -(ry + rh - borderGS - BarTopOffset() + (int)RenglonHeightGS) - marginGS;
    }
    ResizeScrollbar(width, height, maxX, maxY, BarTopOffset());
    const int reservaV = scrollY ? (borderGS + GlobalScale * 9 + 2) : 0;
    Renglon->SetSize(0, 0, (GLshort)(width - reservaV), RenglonHeightGS);
}

// la MINIATURA de la celda de un recurso (0 = el icono de su tipo). Las de una LIBRERIA externa no: las
// miniaturas se arman con los recursos del proyecto ABIERTO (el id de la libreria es solo un nombre) y la
// celda de su "Material" dibujaba el material homonimo de este proyecto. Sin montarla no hay de donde sacar
// la suya: va el icono del tipo.
unsigned int Outliner::MiniaturaDeFila(const OutFilaRec& f, int* w, int* h) const {
    if (w) *w = 0;
    if (h) *h = 0;
    if (f.carpeta || VistaSoloLectura()) return 0;
    return W3dMiniatura(f.tipo, f.id, w, h);
}

// un rectangulo de color liso (con la textura apagada) en coordenadas del panel
static void RectLiso(int rx, int ry, int rw, int rh) {
    static Rec2D* r = NULL;
    if (!r) r = new Rec2D();
    w3dEngine::Disable(w3dEngine::Texture2D);
    r->SetSize((GLshort)rx, (GLshort)ry, (GLshort)rw, (GLshort)rh);
    r->RenderObject(false);
    w3dEngine::Enable(w3dEngine::Texture2D);
}
// el MARCO de un rectangulo (4 lados de 'g' px)
static void Marco(int rx, int ry, int rw, int rh, int g) {
    RectLiso(rx, ry, rw, g); RectLiso(rx, ry + rh - g, rw, g);
    RectLiso(rx, ry, g, rh); RectLiso(rx + rw - g, ry, g, rh);
}
// una textura (una miniatura) estirada a un cuadrado de 's' px en (px, py)
static void DibujarTextura(unsigned int tex, int px, int py, int s) {
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
static std::string Recortar(const std::string& t, int px) {
    const int n = px / (LetterWidthGS > 0 ? LetterWidthGS : 1);
    if ((int)t.size() <= n || n < 3) return t;
    return t.substr(0, (size_t)(n - 2)) + "..";
}

void Outliner::RenderRecursos(int glY, const std::vector<OutFilaRec>& filas) {
    // (el rename que perdio el foco y las filas del cuadro ya los hizo Render)
    SincronizarBarraVista();
    const int reservaBarra = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    const bool conEstado = !VistaSoloLectura();
    const int anchoDer = conEstado ? (IconSizeGS + gapGS + 4 * CharacterWidthGS) : 0;   // "999" + el icono de estado
    const int alto = (int)RenglonHeightGS;
    const int fc = FilaDelCursor(filas);

    // ---- el color del NOMBRE: la fila ACTIVA (el cursor) en verde, la seleccion en verde oscuro, como en
    //      la escena (una carpeta elegida se resalta ELLA, no el ultimo recurso) ----
    struct Colorear { static void Fila(const Outliner* o, const OutFilaRec& f, bool esCursor, bool arrastrada) {
        const GLfloat op = f.soloLectura ? 0.6f : 1.0f;
        const bool enSel = o->FilaEnSeleccion(f);
        if (arrastrada) SetColorID(ColorID::accent, op);
        else if (esCursor) SetColorID(enSel ? ColorID::accent : ColorID::blanco, op);
        else if (enSel) SetColorID(ColorID::accentDark, op);
        else SetColorID(ColorID::grisUI, op);
    } };

    w3dEngine::Enable(w3dEngine::ScissorTest);
    if (!cuadricula) {
        // ---- LISTA: nombres (recortados antes de la columna de estado) ----
        if (scrollX) w3dEngine::Scissor(x, glY + marginGS, width - anchoDer - marginGS - borderGS - reservaBarra, height - marginGS);
        else         w3dEngine::Scissor(x, glY, width - anchoDer - marginGS - borderGS - reservaBarra, height);
        const int y0 = PosY + borderGS + BarTopOffset();
        for (size_t i = 0; i < filas.size(); i++) {
            const int yf = y0 + (int)i * alto;
            if (yf + alto * 2 <= 0 || yf >= height + alto) continue;   // culling (mismo margen que el arbol)
            const OutFilaRec& f = filas[i];
            const bool arrastrada = recArrastrando && f.clave == recArrastreId && f.carpeta == recArrastreCarpeta;
            // LINEAS GUIA: una linea por cada carpeta de arriba (como el arbol de la escena)
            SetColorID(ColorID::grisUI, 0.55f);
            for (int L = 0; L < f.prof; L++) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(marginGS + PosX + L * (IconSizeGS + gapGS)), (GLfloat)yf, 0);
                W3dDrawStrip4(IconLineMesh, IconsUV[(size_t)IconType::line]->uvs);
                w3dEngine::PopMatrix();
            }
            Colorear::Fila(this, f, (int)i == fc, arrastrada);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(marginGS + PosX + f.prof * (IconSizeGS + gapGS)), (GLfloat)yf, 0);
            // flechita (solo las carpetas se pliegan; la columna queda vacia en los recursos)
            if (f.carpeta)
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)(f.plegada ? IconType::arrowRight : IconType::arrow)]->uvs);
            w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
            if (f.icono >= 0 && f.icono < (int)IconsUV.size()) W3dDrawStrip4(IconMesh, IconsUV[(size_t)f.icono]->uvs);
            w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
            const bool enRename = renombrando && !renombreObjeto && renombreVista == vista && f.carpeta == renombreCarpeta && f.clave == renombreClave;
            if (enRename) {
                const bool foco = (g_textFieldActivo == &renombre);
                if (foco && renombre.selectAll) { SetColorID(ColorID::accent); RenderBitmapText(renombre.text); }
                else {
                    SetColorID(ColorID::blanco);
                    RenderBitmapText(foco ? renombre.text.substr(0, (size_t)renombre.caret) + "|" + renombre.text.substr((size_t)renombre.caret)
                                          : renombre.text);
                }
            } else RenderBitmapText(f.sucio ? f.nombre + "*" : f.nombre);
            w3dEngine::PopMatrix();
            // TACTIL: la fila AGARRADA (pulsacion larga) lleva un marco verde: ya se puede arrastrar
            if (agarreFila == (int)i) { SetColorID(ColorID::accent); Marco(borderGS, yf, width - 2 * borderGS - reservaBarra, alto, GlobalScale); }
        }
        // ---- la columna de ESTADO: cuantos lo usan + tilde verde (en uso) / cruz roja (huerfano) ----
        if (conEstado) {
            if (scrollX) w3dEngine::Scissor(x, glY + marginGS, width - marginGS - borderGS, height - marginGS);
            else         w3dEngine::Scissor(x, glY, width - marginGS - borderGS, height);
            const int xIcono = width - IconSizeGS - marginGS - borderGS - reservaBarra;
            for (size_t i = 0; i < filas.size(); i++) {
                const OutFilaRec& f = filas[i];
                if (f.carpeta) continue;
                const int yf = y0 + (int)i * alto;
                if (yf + alto * 2 <= 0 || yf >= height + alto) continue;
                const bool enUso = f.usuarios > 0;
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)xIcono, (GLfloat)(yf + GlobalScale), 0);
                if (enUso) SetColorID(ColorID::accent, 1.0f);
                else w3dEngine::Color4f(kRojoHuerfano[0], kRojoHuerfano[1], kRojoHuerfano[2], 0.85f);
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)(enUso ? IconType::notifOk : IconType::notifError)]->uvs);
                w3dEngine::PopMatrix();
                char b[16]; sprintf(b, "%d", f.usuarios);
                SetColorID(ColorID::grisUI, enUso ? 1.0f : 0.6f);
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(xIcono - gapGS), (GLfloat)yf, 0);
                RenderBitmapText(b, textAlign::right, 4 * CharacterWidthGS);
                w3dEngine::PopMatrix();
            }
        }
    } else {
        // ---- CUADRICULA: las carpetas son franjas; los recursos, celdas con su MINIATURA y su nombre ----
        w3dEngine::Scissor(x, glY, width - borderGS - reservaBarra, height - borderGS);
        const int celda = CeldaTam();
        const int lado = celda - 2 * gapGS - alto;        // la miniatura (arriba del nombre)
        W3dMiniaturasCuadro();   // un cuadro nuevo: las miniaturas que falten se generan de a pocas
        for (size_t i = 0; i < filas.size(); i++) {
            int rx, ry, rw, rh;
            RectFila(filas, (int)i, rx, ry, rw, rh);
            ry += PosY;
            if (ry + rh <= 0 || ry >= height) continue;
            const OutFilaRec& f = filas[i];
            const bool arrastrada = recArrastrando && f.clave == recArrastreId && f.carpeta == recArrastreCarpeta;
            if (f.carpeta) {
                // (el fondo de la franja: el destino de un arrastre, o la carpeta elegida)
                if (recArrastrando && (int)i == recDropFila) { SetColorID(ColorID::accentDark); RectLiso(borderGS, ry, width - 2 * borderGS, rh); }
                else if (FilaEnSeleccion(f)) { SetColorID(ColorID::headerColor); RectLiso(borderGS, ry, width - 2 * borderGS, rh); }
                SetColorID(ColorID::grisUI, 0.55f);
                for (int L = 0; L < f.prof; L++) {
                    w3dEngine::PushMatrix();
                    w3dEngine::Translatef((GLfloat)(marginGS + L * (IconSizeGS + gapGS)), (GLfloat)ry, 0);
                    W3dDrawStrip4(IconLineMesh, IconsUV[(size_t)IconType::line]->uvs);
                    w3dEngine::PopMatrix();
                }
                Colorear::Fila(this, f, (int)i == fc, arrastrada);
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(marginGS + f.prof * (IconSizeGS + gapGS)), (GLfloat)ry, 0);
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)(f.plegada ? IconType::arrowRight : IconType::arrow)]->uvs);
                w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)IconType::carpeta]->uvs);
                w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
                const bool enRename = renombrando && !renombreObjeto && renombreVista == vista && renombreCarpeta && f.clave == renombreClave;
                RenderBitmapText(enRename ? renombre.text + (g_textFieldActivo == &renombre ? "|" : "") : f.nombre);
                w3dEngine::PopMatrix();
                if (agarreFila == (int)i) { SetColorID(ColorID::accent); Marco(borderGS, ry, width - 2 * borderGS, rh, GlobalScale); }
                continue;
            }
            // el FONDO de la celda: elegida / en la seleccion / normal
            const bool enSel = FilaEnSeleccion(f);
            if ((int)i == fc || enSel) SetColorID(enSel ? ColorID::accentDark : ColorID::headerColor);
            else SetColorID(ColorID::gris);
            RectLiso(rx, ry, rw, rh);
            // la MINIATURA (perezosa: mientras no esta, el icono del tipo en grande)
            const int mxp = rx + (rw - lado) / 2, myp = ry + gapGS;
            int tw = 0, th = 0;
            const unsigned int tex = MiniaturaDeFila(f, &tw, &th);
            if (tex) DibujarTextura(tex, mxp, myp, lado);
            else if (f.icono >= 0 && f.icono < (int)IconsUV.size()) {
                // el icono del tipo, escalado al lado de la miniatura (sin miniatura todavia, o un sonido/script)
                SetColorID(ColorID::grisUI, f.soloLectura ? 0.6f : 1.0f);
                w3dEngine::PushMatrix();
                const int esc = lado / (IconSizeGS > 0 ? IconSizeGS : 1);
                const int s = IconSizeGS * (esc > 1 ? esc - 1 : 1);
                w3dEngine::Translatef((GLfloat)(rx + (rw - s) / 2), (GLfloat)(myp + (lado - s) / 2), 0);
                w3dEngine::Scalef((GLfloat)s / (GLfloat)IconSizeGS, (GLfloat)s / (GLfloat)IconSizeGS, 1.0f);
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)f.icono]->uvs);
                w3dEngine::PopMatrix();
            }
            // estado (en uso / huerfano) en la esquina
            if (conEstado) {
                w3dEngine::PushMatrix();
                w3dEngine::Translatef((GLfloat)(rx + rw - IconSizeGS - GlobalScale), (GLfloat)(ry + GlobalScale), 0);
                if (f.usuarios > 0) SetColorID(ColorID::accent, 1.0f);
                else w3dEngine::Color4f(kRojoHuerfano[0], kRojoHuerfano[1], kRojoHuerfano[2], 0.85f);
                W3dDrawStrip4(IconMesh, IconsUV[(size_t)(f.usuarios > 0 ? IconType::notifOk : IconType::notifError)]->uvs);
                w3dEngine::PopMatrix();
            }
            // el nombre, abajo (recortado a la celda)
            Colorear::Fila(this, f, (int)i == fc, arrastrada);
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(rx + GlobalScale), (GLfloat)(ry + rh - alto), 0);
            const bool enRename = renombrando && !renombreObjeto && renombreVista == vista && !renombreCarpeta && f.clave == renombreClave;
            const std::string nom = enRename ? renombre.text + (g_textFieldActivo == &renombre ? "|" : "") : (f.sucio ? f.nombre + "*" : f.nombre);
            RenderBitmapText(Recortar(nom, rw - 2 * GlobalScale), textAlign::left, rw - 2 * GlobalScale);
            w3dEngine::PopMatrix();
            if (agarreFila == (int)i || arrastrada) { SetColorID(ColorID::accent); Marco(rx, ry, rw, rh, GlobalScale); }
        }
    }
    w3dEngine::Disable(w3dEngine::ScissorTest);

    // ---- vista vacia: se dice por que ----
    if (filas.empty()) {
        SetColorID(ColorID::grisUI, 0.7f);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(marginGS), (GLfloat)(borderGS + BarTopOffset() + alto / 2), 0);
        std::string msg = T("(empty)");
        if (VistaSoloLectura()) {
            std::vector<W3dRecursoItem> its; std::vector<std::string> cs; std::string err;
            if (!W3dLibreriaListar(vista - OUT_VISTA_LIBEXT, its, cs, &err)) msg = std::string(T("Library not readable")) + ": " + T(err.c_str());
        }
        RenderBitmapText(msg, textAlign::left, width - 2 * marginGS);
        w3dEngine::PopMatrix();
    }
    // ---- el modo MOVER: a donde iria (abajo, como la barra de estado del transform) ----
    if (moviendoRec) {
        SetColorID(ColorID::accent);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)marginGS, (GLfloat)(height - alto - borderGS), 0);
        RenderBitmapText(std::string(T("Move to")) + ": " + (moverRecDestino.empty() ? std::string("(") + T("root") + ")" : moverRecDestino),
                         textAlign::left, width - 2 * marginGS);
        w3dEngine::PopMatrix();
    }
}

// ============================================================================
//  EL ARRASTRE AFUERA DEL OUTLINER (al 3D, a Properties): lo que se lleva junto al puntero y los
//  destinos resaltados (verde = se puede soltar ahi; rojo = no)
// ============================================================================
void OutlinerArrastreRender(int pantallaW, int pantallaH) {
    if (!g_outArrastre.activo) return;
    w3dEngine::Viewport(0, 0, pantallaW, pantallaH);
    w3dEngine::MatrixMode(w3dEngine::Projection); w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, pantallaW, pantallaH, 0, -1, 1);
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
    std::vector<ViewportBase*> hojas;
    struct J { static void Juntar(ViewportBase* n, std::vector<ViewportBase*>& out) {
        if (!n) return;
        if (n->isLeaf()) { out.push_back(n); return; }
        if (n->ContainerKind() == 1) { Juntar(((ViewportRow*)n)->childA, out); Juntar(((ViewportRow*)n)->childB, out); }
        else { Juntar(((ViewportColumn*)n)->childA, out); Juntar(((ViewportColumn*)n)->childB, out); } } };
    J::Juntar(LayoutRaizCompleta(), hojas);
    for (size_t h = 0; h < hojas.size(); h++) {
        if (hojas[h]->ViewportKind() != 3) continue;
        std::vector<int> rects;
        PropsDestinosRecurso((Properties*)hojas[h], g_outArrastre.tipo, rects);
        for (size_t k = 0; k + 3 < rects.size(); k += 4) {
            const bool bajo = g_outArrastre.x >= rects[k] && g_outArrastre.x < rects[k] + rects[k + 2] &&
                              g_outArrastre.y >= rects[k + 1] && g_outArrastre.y < rects[k + 1] + rects[k + 3];
            SetColorID(ColorID::accent, bajo ? 1.0f : 0.55f);
            Marco(rects[k] - GlobalScale, rects[k + 1] - GlobalScale, rects[k + 2] + 2 * GlobalScale, rects[k + 3] + 2 * GlobalScale,
                  bajo ? 2 * GlobalScale : GlobalScale);
        }
    }
    // el viewport 3D bajo el puntero: marco verde (se puede) o rojo (no)
    ViewportBase* bajo = FindViewportUnderMouse(rootViewport, g_outArrastre.x, g_outArrastre.y);
    if (bajo && bajo->isLeaf() && bajo->ViewportKind() == 1 && g_outArrastre.destino != 0) {
        if (g_outArrastre.destino > 0) SetColorID(ColorID::accent);
        else w3dEngine::Color4f(kRojoHuerfano[0], kRojoHuerfano[1], kRojoHuerfano[2], 1.0f);
        Marco(bajo->x, bajo->y, bajo->width, bajo->height, 2 * GlobalScale);
    }
    // lo que se lleva: el icono + el nombre + (que haria al soltar)
    const int px = g_outArrastre.x + 3 * GlobalScale, py = g_outArrastre.y + 3 * GlobalScale;
    std::string t = g_outArrastre.nombre;
    if (!g_outArrastre.destinoTexto.empty()) t += "  (" + g_outArrastre.destinoTexto + ")";
    const int w = IconSizeGS + gapGS + (int)(t.size() + 1) * LetterWidthGS + 2 * gapGS;
    SetColorID(ColorID::gris, 0.9f);
    RectLiso(px, py, w, (int)RenglonHeightGS);
    if (g_outArrastre.destino < 0) w3dEngine::Color4f(kRojoHuerfano[0], kRojoHuerfano[1], kRojoHuerfano[2], 1.0f);
    else SetColorID(g_outArrastre.destino > 0 ? ColorID::accent : ColorID::blanco);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(px + gapGS), (GLfloat)py, 0);
    if (g_outArrastre.icono >= 0 && g_outArrastre.icono < (int)IconsUV.size()) W3dDrawStrip4(IconMesh, IconsUV[(size_t)g_outArrastre.icono]->uvs);
    w3dEngine::Translatef((GLfloat)(IconSizeGS + gapGS), 0, 0);
    RenderBitmapText(t);
    w3dEngine::PopMatrix();
}

// ============================================================================
//  ELEGIR, CLICK, ARRASTRE
// ============================================================================
// elegir una fila de la biblioteca SUELTA la escena (no queda un objeto seleccionado "de fondo": el panel
// de propiedades muestra el recurso) y el recurso pasa a ser el activo; una carpeta no tiene propiedades
void Outliner::ElegirFila(const OutFilaRec& f, bool abrirProps) {
    cursorEsCarpeta = f.carpeta;
    cursorClave = f.clave;
    if (InteractionMode == ObjectMode && (ObjActivo || !ObjSelects.empty())) {
        DeseleccionarTodo(true);
        ObjActivo = NULL;
    }
    if (!f.carpeta && !f.soloLectura) {
        W3dRecursoActivar(f.tipo, f.id);
        if (abrirProps) PropsIrARecurso();
    } else W3dRecursoDesactivar();
    g_redraw = true;
}

void Outliner::ClickRecursos(int mx, int my) {
    recArrastre = recArrastrando = recArrastreCarpeta = recSoloAlSoltar = recElegirAlSoltar = false;
    if (moviendoRec) { MoverRecConfirmar(); return; }   // (un click confirma el modo mover, como en la escena)
    // un rename en linea abierto: el click AFUERA lo confirma (el down ya desenfoco el campo: controles.cpp)
    if (renombrando) RenombreSincronizar();
    std::vector<OutFilaRec> filas;
    FilasRecursos(filas);
    const int fila = FilaEnPunto(filas, mx, my);
    if (fila < 0) {
        // el vacio: suelta la seleccion multiple (el cursor queda)
        if (!LCtrlPressed && !LShiftPressed && SeleccionCantidad() > 1) { selRecursos.clear(); selCarpetas.clear(); selMarcada = false; g_redraw = true; }
        return;
    }
    const OutFilaRec& f = filas[(size_t)fila];
    if (f.carpeta) {
        // la FLECHITA (o el icono) pliega/despliega sin tocar la seleccion
        const int xFlecha = x + marginGS + (cuadricula ? 0 : PosX) + f.prof * (IconSizeGS + gapGS);
        if (mx >= xFlecha && mx < xFlecha + 2 * IconSizeGS + gapGS && !LCtrlPressed && !LShiftPressed) {
            std::set<std::string>& pl = PlegadasDe(this);
            if (f.plegada) pl.erase(f.clave); else pl.insert(f.clave);
            lastContentRows = -1;
            if (SeleccionCantidad() <= 1) SeleccionSolo(f);   // (plegar no rompe una seleccion de varias)
            ElegirFila(f, false);
            return;
        }
    }
    // ---- la SELECCION MULTIPLE ----
    if (LShiftPressed) {
        // RANGO desde el cursor hasta la fila (sumado a lo que ya estaba)
        int desde = FilaDelCursor(filas);
        if (desde < 0) desde = fila;
        const int a = desde < fila ? desde : fila, b = desde < fila ? fila : desde;
        for (int i = a; i <= b; i++) {
            if (filas[(size_t)i].carpeta) selCarpetas.insert(filas[(size_t)i].clave);
            else selRecursos.insert(filas[(size_t)i].clave);
        }
        selMarcada = true;
        ElegirFila(f, !f.carpeta);
        return;   // (un rango no arranca arrastre)
    }
    if (LCtrlPressed) {
        // una fila MAS (o una menos): si la seleccion seguia al cursor, el cursor entra primero
        if (SeleccionCantidad() == 0) {
            const int fc = FilaDelCursor(filas);
            if (fc >= 0) SeleccionAlternar(filas[(size_t)fc]);
        }
        SeleccionAlternar(f);
        ElegirFila(f, !f.carpeta && FilaEnSeleccion(f));
        return;
    }
    // click comun: sobre una fila de una seleccion de VARIAS, la seleccion se conserva para poder
    // arrastrarla entera (si se suelta sin arrastrar, queda sola); si no, la fila queda sola
    if (SeleccionCantidad() > 1 && FilaEnSeleccion(f)) recSoloAlSoltar = true;
    else SeleccionSolo(f);
    // el click puede volverse ARRASTRE (a una carpeta, al 3D, a Properties): se confirma al moverse con el
    // boton. Con el DEDO no: arrastrar es scroll (se agarra con una pulsacion larga, PulsacionLarga)
    extern bool g_uiTapEnCurso;
    if (!g_uiTapEnCurso) {
        recArrastre = true;
        recArrastreId = f.clave;
        recArrastreCarpeta = f.carpeta;
        recArrastreY0 = my;
        recArrastreX0 = mx;
        recDropFila = -1;
        // la fila pasa a ser el cursor YA, pero ELEGIRLA (soltar la escena, activar el recurso) espera al soltar
        // sin arrastrar: arrastrar un recurso al selector de malla de Properties no puede apagar esa tarjeta
        cursorEsCarpeta = f.carpeta;
        cursorClave = f.clave;
        recElegirAlSoltar = true;
        g_redraw = true;
    } else ElegirFila(f, !f.carpeta);
}

// el DESTINO de un arrastre AFUERA del outliner: un viewport 3D o un desplegable de Properties bajo el
// puntero. soloProbar = solo decir que haria (el resaltado mientras se arrastra).
static bool SoltarAfuera(int mx, int my, int tipo, const std::string& id, bool soloProbar, std::string* que) {
    ViewportBase* bajo = FindViewportUnderMouse(rootViewport, mx, my);
    if (!bajo || !bajo->isLeaf()) return false;
    if (bajo->ViewportKind() == 1) return W3dSoltarRecursoEn3D((Viewport3D*)bajo, mx, my, tipo, id, soloProbar, que);
    if (bajo->ViewportKind() == 3) return PropsSoltarRecurso((Properties*)bajo, mx, my, tipo, id, soloProbar, que);
    return false;
}

void Outliner::MotionRecursos(int mx, int my) {
    const int ref = (agarreFila >= 0) ? agarreY : recArrastreY0;
    int d = my - ref; if (d < 0) d = -d;
    int dx = mx - ((agarreFila >= 0) ? agarreX : recArrastreX0); if (dx < 0) dx = -dx;
    if (!recArrastrando && (d > (int)RenglonHeightGS / 2 || dx > (int)RenglonHeightGS)) {
        recArrastrando = true;
        agarreMovido = true;
    }
    if (!recArrastrando) return;
    recDropFila = -1;
    g_redraw = true;
    // AFUERA del outliner: un RECURSO (no una carpeta) se lleva al 3D o a Properties
    if (!Contains(mx, my)) {
        g_outArrastre.activo = false;
        if (recArrastreCarpeta) return;
        int t = 0; std::string id;
        if (!W3dBibDeClave(recArrastreId, &t, &id)) return;
        W3dRecursoItem it;
        const bool hay = VistaSoloLectura() ? false : W3dVistaRecInfo(t, id, &it);
        g_outArrastre.activo = true;
        g_outArrastre.tipo = t;
        g_outArrastre.id = id;
        g_outArrastre.nombre = hay ? it.nombre : id;
        g_outArrastre.icono = hay ? it.icono : W3dVistaIcono(t);
        g_outArrastre.soloLectura = VistaSoloLectura();
        g_outArrastre.x = mx; g_outArrastre.y = my;
        g_outArrastre.destinoTexto.clear();
        // el contenido de una LIBRERIA externa se suelta con su ID GLOBAL: un prefab o una escena = un PROXY; una
        // malla, un material, una textura o un animset = una referencia a su recurso (con su prefijo)
        const std::string idSoltar = g_outArrastre.soloLectura ? W3dLibreriaIdGlobal(vista - OUT_VISTA_LIBEXT, t, id) : id;
        if (idSoltar.empty()) {
            g_outArrastre.destino = -1;
            g_outArrastre.destinoTexto = T("read-only library");
        } else {
            std::string que;
            const bool ok = SoltarAfuera(mx, my, t, idSoltar, true, &que);
            ViewportBase* bajo = FindViewportUnderMouse(rootViewport, mx, my);
            const bool esDestino = bajo && bajo->isLeaf() && (bajo->ViewportKind() == 1 || bajo->ViewportKind() == 3);
            g_outArrastre.destino = ok ? 1 : (esDestino && !que.empty() ? -1 : 0);
            g_outArrastre.destinoTexto = que;
        }
        return;
    }
    g_outArrastre.activo = false;
    if (VistaSoloLectura()) return;   // (adentro de una libreria externa no se mueve nada)
    std::vector<OutFilaRec> filas;
    FilasRecursos(filas);
    const int fila = FilaEnPunto(filas, mx, my);
    // el destino es la CARPETA de la fila (el mismo que decide SoltarRecursos): la fila de una carpeta,
    // o la carpeta del recurso que se apunta. Se resalta ESA carpeta, no el recurso; la raiz (un
    // recurso suelto, o el vacio de abajo) no tiene fila y no se resalta nada.
    if (fila >= 0) {
        if (filas[(size_t)fila].carpeta) recDropFila = fila;
        else if (!filas[(size_t)fila].carpetaDe.empty()) {
            for (size_t i = 0; i < filas.size(); i++)
                if (filas[i].carpeta && filas[i].clave == filas[(size_t)fila].carpetaDe) { recDropFila = (int)i; break; }
        }
    }
}

bool Outliner::MoverObjetivo(const std::string& destino) {
    std::vector<std::string> recs, cars;
    Objetivo(recs, cars, true);
    if (recs.empty() && cars.empty()) return false;
    bool movio = false;
    // TODO en UN paso de undo (el grupo toma una foto al abrir y otra al cerrar)
    W3dVistaRecGrupoIniciar();
    for (size_t i = 0; i < cars.size(); i++) {
        if (W3dCarpetaAdentro(destino, cars[i])) continue;       // adentro de si misma: se saltea
        std::string nueva;
        if (!W3dCarpetaMover(cars[i], destino, &nueva) || nueva == cars[i]) continue;
        movio = true;
        // lo plegado, el cursor y la seleccion siguen a la carpeta (y a sus subcarpetas)
        std::set<std::string> pl, sc;
        for (std::set<std::string>::iterator it = plegadas.begin(); it != plegadas.end(); ++it)
            pl.insert(W3dCarpetaAdentro(*it, cars[i]) ? nueva + it->substr(cars[i].size()) : *it);
        plegadas.swap(pl);
        for (std::set<std::string>::iterator it = selCarpetas.begin(); it != selCarpetas.end(); ++it)
            sc.insert(W3dCarpetaAdentro(*it, cars[i]) ? nueva + it->substr(cars[i].size()) : *it);
        selCarpetas.swap(sc);
        if (cursorEsCarpeta && W3dCarpetaAdentro(cursorClave, cars[i])) cursorClave = nueva + cursorClave.substr(cars[i].size());
        for (size_t k = i + 1; k < cars.size(); k++)                // (las que faltan mover, por si colgaban)
            if (W3dCarpetaAdentro(cars[k], cars[i])) cars[k] = nueva + cars[k].substr(cars[i].size());
    }
    for (size_t i = 0; i < recs.size(); i++) {
        int t = 0; std::string id;
        if (!W3dBibDeClave(recs[i], &t, &id)) continue;
        W3dRecursoItem it;
        if (W3dVistaRecInfo(t, id, &it) && it.carpeta == W3dCarpetaNormalizar(destino)) continue;
        if (W3dVistaRecMover(t, id, destino)) movio = true;
    }
    W3dVistaRecGrupoFin();
    if (movio) {
        // la carpeta destino se despliega para que se vea donde quedo
        for (std::string c = W3dCarpetaNormalizar(destino); !c.empty(); c = W3dCarpetaPadre(c)) plegadas.erase(c);
        lastContentRows = -1;
    }
    g_redraw = true;
    return movio;
}

void Outliner::SoltarRecursos(int mx, int my) {
    const bool estaba = recArrastrando;
    const bool soloAlSoltar = recSoloAlSoltar;
    const std::string clave = recArrastreId;
    const bool eraCarpeta = recArrastreCarpeta;
    const bool agarrada = agarreFila >= 0, movida = agarreMovido;
    const bool elegir = recElegirAlSoltar;
    recElegirAlSoltar = false;
    recArrastre = recArrastrando = recArrastreCarpeta = recSoloAlSoltar = false;
    recDropFila = -1;
    agarreFila = -1; agarreMovido = false;
    const bool afuera = g_outArrastre.activo;
    g_outArrastre.activo = false;
    g_redraw = true;
    // TACTIL: soltar SIN MOVER lo que se agarro con la pulsacion larga = el menu contextual
    if (agarrada && !movida) { MenuContexto(mx, my); return; }
    if (!estaba) {
        // un click sin arrastre sobre una fila de la seleccion: queda sola
        if (soloAlSoltar) {
            OutFilaRec f; f.carpeta = eraCarpeta; f.clave = clave;
            SeleccionSolo(f);
        }
        // ...y recien ahora se ELIGE (el panel de propiedades la muestra; la escena se suelta)
        if (elegir) {
            std::vector<OutFilaRec> filas; FilasRecursos(filas);
            for (size_t i = 0; i < filas.size(); i++)
                if (filas[i].carpeta == eraCarpeta && filas[i].clave == clave) { ElegirFila(filas[i], !eraCarpeta); break; }
        }
        return;
    }
    // AFUERA: el recurso se SUELTA en el viewport 3D o en un desplegable de Properties
    if (!Contains(mx, my)) {
        if (!afuera || eraCarpeta) return;
        int t = 0; std::string id;
        if (!W3dBibDeClave(clave, &t, &id)) return;
        // (de una LIBRERIA externa: su id global, ver MotionRecursos; lo que no se puede arrastrar no hace nada)
        if (VistaSoloLectura()) { id = W3dLibreriaIdGlobal(vista - OUT_VISTA_LIBEXT, t, id); if (id.empty()) return; }
        std::string que;
        if (!SoltarAfuera(mx, my, t, id, false, &que) && !que.empty()) Notificar(que, true);
        return;
    }
    if (VistaSoloLectura()) return;
    std::vector<OutFilaRec> filas;
    FilasRecursos(filas);
    const int fila = FilaEnPunto(filas, mx, my);
    std::string destino;   // el vacio de abajo = la raiz
    if (fila >= 0) destino = filas[(size_t)fila].carpeta ? filas[(size_t)fila].clave : filas[(size_t)fila].carpetaDe;
    // lo que se arrastra: la SELECCION si la fila apretada es parte de ella (si no, ya quedo sola)
    MoverObjetivo(destino);
}

// DOBLE CLICK / DOBLE TAP sobre el NOMBRE de una fila: se renombra en linea (en las dos vistas). En el resto de
// la fila (la flechita, el icono, el ojo, la camara, la columna de estado) no: esos clicks ya hicieron lo suyo
// (un doble click rapido sobre el ojo lo apaga y lo prende) y no piden un renombrar. Una CELDA de la cuadricula
// no tiene nada que alternar: vale entera.
bool Outliner::DobleClick(int mx, int my) {
    if (!Contains(mx, my) || OnBar(mx, my)) return false;
    const int reservaBarra = scrollY ? (GlobalScale * 9 + gapGS) : 0;
    if (vista == OUT_VISTA_ESCENA) {
        const int rel = my - y - borderGS - PosY - BarTopOffset();
        if (rel < 0) return false;
        const int fila = rel / (int)RenglonHeightGS;
        extern Object* OutlinerObjetoEnFila(int fila);
        extern int OutlinerProfDeFila(int fila);
        Object* o = OutlinerObjetoEnFila(fila);
        if (!o) return false;
        // el NOMBRE: despues de la flechita y el icono, antes de las columnas del ojo y la camara
        const int x0 = x + marginGS + PosX + (OutlinerProfDeFila(fila) + 2) * (IconSizeGS + gapGS);
        const int x1 = x + width - 2 * IconSizeGS - gapGS - marginGS - borderGS - reservaBarra;
        if (mx < x0 || mx >= x1) return false;
        dragObjeto = NULL; dragging = false;
        return RenombrarObjetoEnLinea(o);
    }
    std::vector<OutFilaRec> filas;
    FilasRecursos(filas);
    const int fila = FilaEnPunto(filas, mx, my);
    if (fila < 0) return false;
    if (!cuadricula || filas[(size_t)fila].carpeta) {
        // el NOMBRE: despues de la flechita y el icono, antes de la columna de estado (en uso / huerfano)
        const int x0 = x + marginGS + (cuadricula ? 0 : PosX) + (filas[(size_t)fila].prof + 2) * (IconSizeGS + gapGS);
        const int anchoDer = (!cuadricula && !VistaSoloLectura()) ? (IconSizeGS + gapGS + 4 * CharacterWidthGS) : 0;
        const int x1 = x + width - anchoDer - marginGS - borderGS - reservaBarra;
        if (mx < x0 || mx >= x1) return false;
    }
    recArrastre = recArrastrando = false;
    SeleccionSolo(filas[(size_t)fila]);
    ElegirFila(filas[(size_t)fila], false);
    return AccionRenombrar();
}

// TACTIL: MANTENER apretada una fila (~0,5 s sin mover el dedo) la AGARRA (marco verde): moverla despues la
// arrastra (a una carpeta, al 3D o a Properties; en la escena reordena/emparenta como con el mouse) y soltarla
// sin moverla abre el menu contextual. Sin esto, arrastrar el dedo es scroll (lo normal).
bool Outliner::PulsacionLarga(int mx, int my) {
    if (!Contains(mx, my) || OnBar(mx, my)) return false;
    agarreX = mx; agarreY = my;
    agarreMovido = false;
    if (vista == OUT_VISTA_ESCENA) {
        const int rel = my - y - borderGS - PosY - BarTopOffset();
        if (rel < 0) return false;
        extern Object* OutlinerObjetoEnFila(int fila);
        Object* o = OutlinerObjetoEnFila(rel / (int)RenglonHeightGS);
        if (!o) return false;
        if (!o->select) { UndoCapturarSeleccion(); DeseleccionarTodo(); o->Seleccionar(); }
        dragObjeto = o; dragging = false; dragY0 = my;
        agarreFila = rel / (int)RenglonHeightGS;
        g_redraw = true;
        return true;
    }
    std::vector<OutFilaRec> filas;
    FilasRecursos(filas);
    const int fila = FilaEnPunto(filas, mx, my);
    if (fila < 0) return false;
    const OutFilaRec& f = filas[(size_t)fila];
    if (!FilaEnSeleccion(f)) SeleccionSolo(f);
    cursorEsCarpeta = f.carpeta;   // (elegirla -soltar la escena- lo hace el menu contextual, si se suelta sin mover)
    cursorClave = f.clave;
    agarreFila = fila;
    recArrastre = true;
    recArrastrando = false;
    recArrastreId = f.clave;
    recArrastreCarpeta = f.carpeta;
    recArrastreY0 = my; recArrastreX0 = mx;
    recSoloAlSoltar = false;
    g_redraw = true;
    return true;
}

// ============================================================================
//  TECLADO
// ============================================================================
void Outliner::AsegurarVisibleFila(int fila) {
    if (fila < 0) return;
    const int top = BarTopOffset();
    int yFila, alto;
    if (!cuadricula) { alto = (int)RenglonHeightGS; yFila = borderGS + PosY + top + fila * alto; }
    else {
        std::vector<OutFilaRec> filas; FilasRecursos(filas);
        int rx, ry, rw, rh;
        RectFila(filas, fila, rx, ry, rw, rh);
        yFila = ry + PosY; alto = rh;
    }
    int nuevo = PosY;
    if (yFila < top)                nuevo = PosY + (top - yFila);
    else if (yFila + alto > height) nuevo = PosY - (yFila + alto - height);
    if (nuevo > 0) nuevo = 0;
    if (nuevo < MaxPosY) nuevo = MaxPosY;
    if (nuevo != PosY) { PosY = nuevo; g_redraw = true; }
}

// el cursor pasa a la fila 'n' por TECLADO. modo SEL_MARCAS (el keypad del N95): con una seleccion
// de VARIAS (marcadas con el 5) las marcas se quedan y el cursor solo recorre; si no, la seleccion
// sigue al cursor. SEL_EXTENDER (Shift+flechas) suma la fila. SEL_SOLA (las flechas de PC, como en
// un explorador) deja la fila sola.
void Outliner::CursorATecla(const std::vector<OutFilaRec>& filas, int n, int modo) {
    if (n < 0 || n >= (int)filas.size()) return;
    const OutFilaRec& f = filas[(size_t)n];
    if (modo == SEL_EXTENDER) {
        if (SeleccionCantidad() == 0) { const int fc = FilaDelCursor(filas); if (fc >= 0) SeleccionAlternar(filas[(size_t)fc]); }
        if (!FilaEnSeleccion(f)) SeleccionAlternar(f);
    } else if (modo == SEL_SOLA || !selMarcada) SeleccionSolo(f);
    ElegirFila(f, false);   // un recurso elegido con flechas ya se ve en Properties
    AsegurarVisibleFila(n);
}

// flechas / OK / C (LayoutKey): el keypad del N95
bool Outliner::TeclaRecursos(int tecla) {
    return TeclaRecursosCon(tecla, SEL_MARCAS);
}
// flechas / Enter de un teclado de PC: llegan por LayoutTeclaUI (el panel bajo el mouse) antes que
// por event_key_down, asi que el modo de PC se decide aca y no solo en TeclaPCRecursos
bool Outliner::TeclaRecursosPC(int tecla) {
#ifndef W3D_SYMBIAN
    const bool vertical = (tecla == LayoutKey::Up || tecla == LayoutKey::Down);
    return TeclaRecursosCon(tecla, (vertical && LShiftPressed) ? SEL_EXTENDER : SEL_SOLA);
#else
    return TeclaRecursos(tecla);
#endif
}
bool Outliner::TeclaRecursosCon(int tecla, int modo) {
    if (vista == OUT_VISTA_ESCENA) return false;
    // el MODO MOVER se queda con las flechas, el OK y el C
    if (moviendoRec) {
        switch (tecla) {
            case LayoutKey::Up:    MoverRecPaso(0); return true;
            case LayoutKey::Down:  MoverRecPaso(1); return true;
            case LayoutKey::Left:  MoverRecPaso(2); return true;
            case LayoutKey::Right: MoverRecPaso(3); return true;
            case LayoutKey::Enter: MoverRecConfirmar(); return true;
            case LayoutKey::Cancel: MoverRecCancelar(); return true;
        }
        return true;
    }
    std::vector<OutFilaRec> filas;
    FilasRecursos(filas);
    int fc = FilaDelCursor(filas);
    if (filas.empty()) return tecla != LayoutKey::Cancel;
    std::set<std::string>& pl = PlegadasDe(this);
    switch (tecla) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            int n = (fc < 0) ? 0 : fc + (tecla == LayoutKey::Down ? 1 : -1);
            if (n < 0) n = 0;
            if (n >= (int)filas.size()) n = (int)filas.size() - 1;
            CursorATecla(filas, n, modo);
            return true;
        }
        case LayoutKey::Left: {
            if (fc < 0) return true;
            const OutFilaRec& f = filas[(size_t)fc];
            // una carpeta abierta se pliega; si no, el cursor sube a la carpeta de arriba
            if (f.carpeta && !f.plegada) { pl.insert(f.clave); lastContentRows = -1; g_redraw = true; return true; }
            if (!f.carpetaDe.empty()) {
                std::vector<OutFilaRec> f2;
                cursorEsCarpeta = true; cursorClave = f.carpetaDe;
                FilasRecursos(f2);
                const int n = FilaDelCursor(f2);
                if (n >= 0) CursorATecla(f2, n, modo == SEL_EXTENDER ? SEL_MARCAS : modo);
                g_redraw = true;
            }
            return true;
        }
        case LayoutKey::Right: {
            if (fc < 0) return true;
            const OutFilaRec& f = filas[(size_t)fc];
            if (f.carpeta && f.plegada) { pl.erase(f.clave); lastContentRows = -1; g_redraw = true; }
            else if (f.carpeta && fc + 1 < (int)filas.size() && filas[(size_t)fc + 1].carpetaDe == f.clave)
                CursorATecla(filas, fc + 1, modo == SEL_EXTENDER ? SEL_MARCAS : modo);
            return true;
        }
        case LayoutKey::Enter: {
            if (fc < 0) { CursorATecla(filas, 0, modo); return true; }
            const OutFilaRec& f = filas[(size_t)fc];
            if (f.carpeta) {
                if (f.plegada) pl.erase(f.clave); else pl.insert(f.clave);
                lastContentRows = -1; g_redraw = true;
            } else ElegirFila(f, true);   // OK sobre un recurso: Properties lo muestra
            return true;
        }
        case LayoutKey::Cancel:
            // C no se come aca: en el N95 es el MODIFICADOR de undo/redo (C + flecha) y su toque
            // borra (TeclaBorrarRecursos, al soltar). Las marcas se sueltan con Esc (PC) o con
            // "Deselect All" del menu Seleccion.
            return false;
    }
    return false;
}

#ifndef W3D_SYMBIAN
bool Outliner::TeclaPCRecursos(int tecla) {
    if (moviendoRec) {
        switch (tecla) {
            case W3dK_UP:    MoverRecPaso(0); return true;
            case W3dK_DOWN:  MoverRecPaso(1); return true;
            case W3dK_LEFT:  MoverRecPaso(2); return true;
            case W3dK_RIGHT: MoverRecPaso(3); return true;
            case W3dK_RETURN: case W3dK_KP_ENTER: MoverRecConfirmar(); return true;
            case W3dK_ESCAPE: case W3dK_BACKSPACE: case W3dK_C: MoverRecCancelar(); return true;
        }
        return true;   // en modo mover se traga el resto
    }
    switch (tecla) {
        // (las flechas y el Enter normalmente ya los tomo LayoutTeclaUI -> TeclaRecursosPC, con el mouse
        // sobre el panel; aca llegan con el mouse en otro lado y el outliner como panel activo)
        case W3dK_UP:    return TeclaRecursosPC(LayoutKey::Up);
        case W3dK_DOWN:  return TeclaRecursosPC(LayoutKey::Down);
        case W3dK_LEFT:  return TeclaRecursosPC(LayoutKey::Left);
        case W3dK_RIGHT: return TeclaRecursosPC(LayoutKey::Right);
        case W3dK_RETURN: case W3dK_KP_ENTER: return TeclaRecursosPC(LayoutKey::Enter);
        case W3dK_ESCAPE: return SoltarSeleccion();                   // suelta la seleccion de varias
        case W3dK_F2:    return AccionRenombrar();
        case W3dK_X:
        case W3dK_DELETE: return AccionBorrar(true);
        case W3dK_A:
            if (LAltPressed) { SoltarSeleccion(); return true; }   // Alt+A: deseleccionar (queda el cursor)
            SeleccionarTodoRec(); return true;   // todo / nada (como la A de la escena)
        case W3dK_I:     if (LCtrlPressed) { InvertirSeleccionRec(); return true; } return false;
        case W3dK_G:     MoverIniciar(); return true;   // el modo MOVER (como en la escena)
        case W3dK_KP_PERIOD: {                          // encuadrar el cursor
            std::vector<OutFilaRec> filas; FilasRecursos(filas);
            AsegurarVisibleFila(FilaDelCursor(filas));
            return true;
        }
    }
    return false;
}
#else
bool Outliner::TeclaPCRecursos(int tecla) { (void)tecla; return false; }
#endif

// ============================================================================
//  EL MODO MOVER DE LA BIBLIOTECA (G / el 1 del N95): lo elegido viaja por las carpetas EN VIVO (cada
//  flecha lo mueve de verdad, sin undo) y al confirmar queda UN paso de undo; cancelar lo devuelve.
// ============================================================================
// las carpetas por donde puede viajar lo elegido, en el orden del arbol ("" = la raiz primero), sin las
// que se mueven (ni lo que cuelga de ellas)
static void DestinosMover(const std::vector<std::string>& seMueven, std::vector<std::string>& out) {
    out.clear();
    out.push_back(std::string());
    std::vector<std::string> cs;
    W3dCarpetasTodas(cs);
    for (size_t i = 0; i < cs.size(); i++) {
        bool fuera = false;
        for (size_t k = 0; k < seMueven.size() && !fuera; k++) fuera = W3dCarpetaAdentro(cs[i], seMueven[k]);
        if (!fuera) out.push_back(cs[i]);
    }
}
static std::vector<std::string> gMoverRecCarpetas;   // las carpetas que viajan (su ruta ORIGINAL)
void Outliner::MoverRecIniciar() {
    if (moviendoRec || VistaSoloLectura()) return;
    std::vector<std::string> recs, cars;
    Objetivo(recs, cars, true);
    if (recs.empty() && cars.empty()) return;
    // la carpeta de donde sale (la de todos, o la del cursor)
    std::string origen = CarpetaDeOrigen(recs, cars);
    if (origen == "\x01") origen.clear();
    // si no habia seleccion propia, lo elegido es el cursor: se congela en la seleccion (las flechas mueven)
    if (SeleccionCantidad() == 0) {
        for (size_t i = 0; i < recs.size(); i++) selRecursos.insert(recs[i]);
        for (size_t i = 0; i < cars.size(); i++) selCarpetas.insert(cars[i]);
    }
    gMoverRecCarpetas = cars;
    W3dVistaRecGrupoIniciar();   // la foto: confirmar = un paso; cancelar = volver a ella
    moviendoRec = true;
    moverRecDestino = origen;
    g_redraw = true;
}
void Outliner::MoverRecPaso(int dir) {
    if (!moviendoRec) return;
    std::vector<std::string> dest;
    DestinosMover(selCarpetas.empty() ? gMoverRecCarpetas : std::vector<std::string>(selCarpetas.begin(), selCarpetas.end()), dest);
    int i = 0;
    for (size_t k = 0; k < dest.size(); k++) if (dest[k] == moverRecDestino) i = (int)k;
    std::string nuevo = moverRecDestino;
    if (dir == 0 && i > 0) nuevo = dest[(size_t)(i - 1)];
    else if (dir == 1 && i + 1 < (int)dest.size()) nuevo = dest[(size_t)(i + 1)];
    else if (dir == 2) nuevo = W3dCarpetaPadre(moverRecDestino);
    else if (dir == 3) {
        for (size_t k = 0; k < dest.size(); k++)
            if (!dest[k].empty() && W3dCarpetaPadre(dest[k]) == moverRecDestino) { nuevo = dest[k]; break; }
    }
    if (nuevo == moverRecDestino && !(dir == 2 && !moverRecDestino.empty())) return;
    moverRecDestino = nuevo;
    MoverObjetivo(nuevo);        // (adentro del grupo: no deja pasos sueltos)
    // la carpeta destino a la vista
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    for (size_t k = 0; k < filas.size(); k++)
        if (filas[k].carpeta && filas[k].clave == nuevo) { AsegurarVisibleFila((int)k); break; }
    g_redraw = true;
}
void Outliner::MoverRecConfirmar() {
    if (!moviendoRec) return;
    moviendoRec = false;
    W3dVistaRecGrupoFin();       // UN paso de undo con todo el viaje
    lastContentRows = -1;
    g_redraw = true;
}
void Outliner::MoverRecCancelar() {
    if (!moviendoRec) return;
    moviendoRec = false;
    W3dVistaRecGrupoCancelar();  // todo vuelve a donde estaba, sin paso
    lastContentRows = -1;
    g_redraw = true;
}

// ============================================================================
//  ACCIONES
// ============================================================================
// la carpeta donde se crea una NUEVA: la del cursor (si es carpeta, adentro; si es recurso, al lado)
bool Outliner::AccionNuevaCarpeta() {
    if (VistaSoloLectura()) return false;
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    const int fc = FilaDelCursor(filas);
    std::string padre;
    if (fc >= 0) padre = filas[(size_t)fc].carpeta ? filas[(size_t)fc].clave : filas[(size_t)fc].carpetaDe;
    std::string ruta;
    if (!W3dCarpetaNueva(padre, T("New Folder"), &ruta)) return false;
    for (std::string c = padre; !c.empty(); c = W3dCarpetaPadre(c)) plegadas.erase(c);
    cursorEsCarpeta = true;
    cursorClave = ruta;
    selRecursos.clear(); selCarpetas.clear(); selMarcada = false;
    selCarpetas.insert(ruta);
    lastContentRows = -1;
    // se pone nombre enseguida (rename en linea con todo seleccionado)
    AccionRenombrar();
    std::vector<OutFilaRec> f2; FilasRecursos(f2);
    AsegurarVisibleFila(FilaDelCursor(f2));
    return true;
}
// "New Material" del "+": un material en blanco (un huerfano: se asigna arrastrandolo o desde Properties),
// en la carpeta del cursor, con el nombre en edicion
bool Outliner::AccionNuevoMaterial() {
    extern Material* W3dBibliotecaNuevoMaterial();
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    const int fc = FilaDelCursor(filas);
    std::string carpeta;
    if (fc >= 0) carpeta = filas[(size_t)fc].carpeta ? filas[(size_t)fc].clave : filas[(size_t)fc].carpetaDe;
    UndoGrupoIniciar();
    Material* m = W3dBibliotecaNuevoMaterial();
    if (m && !carpeta.empty()) W3dVistaRecMover(W3D_VISTA_MATERIALES, m->name, carpeta);
    UndoGrupoFin();
    if (!m) return false;
    if (filtro >= 0 && filtro != W3D_VISTA_MATERIALES) CambiarFiltro(-1);
    OutFilaRec f; f.carpeta = false; f.tipo = W3D_VISTA_MATERIALES; f.id = m->name;
    f.clave = W3dBibClave(W3D_VISTA_MATERIALES, m->name);
    SeleccionSolo(f);
    ElegirFila(f, true);
    lastContentRows = -1;
    AccionRenombrar();
    return true;
}

// la vista Escena: el nombre del objeto se vuelve un campo EN SU FILA (doble click, F2, "Rename")
bool Outliner::RenombrarObjetoEnLinea(Object* o) {
    if (!o) return false;
    if (renombrando) RenombreSincronizar();
    renombrando = true;
    renombreObjeto = true;
    renombreCarpeta = false;
    renombreSerial = o->serial;
    renombreClave.clear();
    renombreVista = OUT_VISTA_ESCENA;
    renombre.SetText(o->name);
    renombre.SelectAll();
    TextFieldEnfocar(&renombre);   // Esc vuelve a este texto: el rename no cambia nada
#if !defined(__EMSCRIPTEN__)
    { extern bool g_uiTapEnCurso; extern void QwertyAbrir(); if (g_uiTapEnCurso) QwertyAbrir(); }
#endif
    g_redraw = true;
    return true;
}

bool Outliner::AccionRenombrar() {
    if (vista == OUT_VISTA_ESCENA) return RenombrarObjetoEnLinea(ObjActivo);
    if (VistaSoloLectura()) { Notificar(T("The library is read-only"), true); return false; }
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    const int fc = FilaDelCursor(filas);
    if (fc < 0) return false;
    const OutFilaRec& f = filas[(size_t)fc];
    if (!f.carpeta && !f.renombrable) {
        Notificar(T("This resource can't be renamed"), true);
        return false;
    }
    if (renombrando) RenombreSincronizar();
    renombrando = true;
    renombreObjeto = false;
    renombreCarpeta = f.carpeta;
    renombreClave = f.clave;
    renombreVista = vista;
    renombre.SetText(f.nombre);
    renombre.SelectAll();   // tipear reemplaza el nombre entero
    TextFieldEnfocar(&renombre);   // Esc vuelve a este texto: el rename no cambia nada
    AsegurarVisibleFila(fc);
#if !defined(__EMSCRIPTEN__)
    // TACTIL (Android/Symbian): el teclado QWERTY en pantalla, como los renames de Properties
    { extern bool g_uiTapEnCurso; extern void QwertyAbrir(); if (g_uiTapEnCurso) QwertyAbrir(); }
#endif
    g_redraw = true;
    return true;
}

// el Esc: el campo vuelve al texto de antes (TextFieldEnfocar lo anoto) y el rename se da por terminado
// sin tocar nada (el mismo nombre = no hace nada: ver W3dVistaRecRenombrar)
void Outliner::RenombreCancelar() {
    if (!renombrando) return;
    renombrando = false;
    if (g_textFieldActivo == &renombre) g_textFieldActivo = NULL;
    lastContentRows = -1;
    g_redraw = true;
}

void Outliner::RenombreSincronizar() {
    if (!renombrando || g_textFieldActivo == &renombre) return;
    renombrando = false;
    const std::string texto = renombre.text;
    if (renombreObjeto) {
        // la ESCENA: W3dRenombrarObjeto (unico en su escena, arrastra las referencias lua, con undo)
        Object* o = NULL;
        struct B { static Object* Por(Object* p, unsigned s) {
            if (!p) return NULL;
            for (size_t i = 0; i < p->Childrens.size(); i++) {
                if (!p->Childrens[i]) continue;
                if (p->Childrens[i]->serial == s) return p->Childrens[i];
                Object* r = Por(p->Childrens[i], s); if (r) return r;
            }
            return NULL; } };
        o = B::Por(SceneCollection, renombreSerial);
        if (o && !texto.empty() && texto != o->name) W3dRenombrarObjeto(o, texto, true);
    } else if (renombreCarpeta) {
        std::string nueva;
        if (W3dCarpetaRenombrar(renombreClave, texto, &nueva) && nueva != renombreClave) {
            // lo plegado y el cursor siguen a la carpeta (y a sus subcarpetas)
            std::set<std::string> pl;
            for (std::set<std::string>::iterator it = plegadas.begin(); it != plegadas.end(); ++it)
                pl.insert(W3dCarpetaAdentro(*it, renombreClave) ? nueva + it->substr(renombreClave.size()) : *it);
            plegadas.swap(pl);
            if (cursorEsCarpeta && W3dCarpetaAdentro(cursorClave, renombreClave))
                cursorClave = nueva + cursorClave.substr(renombreClave.size());
            std::set<std::string> sc;
            for (std::set<std::string>::iterator it = selCarpetas.begin(); it != selCarpetas.end(); ++it)
                sc.insert(W3dCarpetaAdentro(*it, renombreClave) ? nueva + it->substr(renombreClave.size()) : *it);
            selCarpetas.swap(sc);
        }
    } else {
        int t = 0; std::string id, quedo;
        if (W3dBibDeClave(renombreClave, &t, &id) && W3dVistaRecRenombrar(t, id, texto, &quedo)) {
            const std::string nueva = W3dBibClave(t, quedo);
            if (!cursorEsCarpeta && cursorClave == renombreClave) cursorClave = nueva;
            // (el id de una textura es su entrada: cambia con el nombre)
            if (nueva != renombreClave && selRecursos.erase(renombreClave)) selRecursos.insert(nueva);
        }
    }
    lastContentRows = -1;
    g_redraw = true;
}

// el borrado pendiente de confirmacion (ConfirmarPopup llama un void() sin argumentos): UNO o VARIOS
// recursos de la seleccion (claves de biblioteca). 'forzado' = los en uso se borran igual (sus usuarios
// quedan sin ellos, con Ctrl+Z).
static std::vector<std::string> gBorrarClaves;
static std::vector<std::string> gBorrarCarpetas;   // las CARPETAS de la misma seleccion (se borran si quedan vacias)
static bool gBorrarForzado = false;
// las rutas mas PROFUNDAS primero (una subcarpeta elegida junto con su madre se borra antes)
static bool MasProfunda(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() > b.size();
    return a < b;
}
static void BorrarConfirmado() {
    int n = 0, nc = 0;
    std::string motivo, nombre;
    // (lo que tiene undo -partes, recursos en uso, escenas, carpetas- queda en UN Ctrl+Z)
    UndoGrupoIniciar();
    for (size_t i = 0; i < gBorrarClaves.size(); i++) {
        int t = 0; std::string id, m;
        if (!W3dBibDeClave(gBorrarClaves[i], &t, &id)) continue;
        nombre = id;
        const bool ok = gBorrarForzado ? W3dVistaRecBorrarForzado(t, id, &m) : W3dVistaRecBorrar(t, id, &m);
        if (ok) n++; else motivo = m;
    }
    // las CARPETAS elegidas junto con los recursos: despues de ellos (pueden haber quedado vacias). Una que no
    // esta vacia se queda (y se avisa), como cuando se borran carpetas solas
    std::sort(gBorrarCarpetas.begin(), gBorrarCarpetas.end(), MasProfunda);
    if (!gBorrarCarpetas.empty()) W3dVistaRecGrupoIniciar();
    for (size_t i = 0; i < gBorrarCarpetas.size(); i++) {
        std::string m;
        if (W3dCarpetaBorrar(gBorrarCarpetas[i], &m)) nc++; else motivo = m;
    }
    if (!gBorrarCarpetas.empty()) W3dVistaRecGrupoFin();
    UndoGrupoFin();
    const int total = (int)(gBorrarClaves.size() + gBorrarCarpetas.size());
    if (total == 1 && gBorrarClaves.size() == 1) {
        if (n == 1) Notificar(std::string(T("Deleted")) + ": " + nombre, false);
        else Notificar(std::string(T("Not deleted")) + ": " + T(motivo.c_str()), true);
    } else {
        char b[48]; sprintf(b, "%d / %d", n + nc, total);
        Notificar(std::string(T("Deleted")) + ": " + b + (motivo.empty() ? std::string() : std::string(" (") + T(motivo.c_str()) + ")"),
                  n + nc < total);
    }
    gBorrarClaves.clear();
    gBorrarCarpetas.clear();
    gBorrarForzado = false;
    g_redraw = true;
}
static int gPurgarTipo = -1;
static void PurgarConfirmado() {
    std::vector<std::string> nombres;
    const int n = W3dVistaRecPurgar(gPurgarTipo, &nombres);
    char b[64]; sprintf(b, "%d", n);
    Notificar(std::string(T("Orphans purged")) + ": " + b, false);
    g_redraw = true;
}
bool Outliner::AccionBorrar(bool confirmar) {
    if (VistaSoloLectura()) { Notificar(T("The library is read-only"), true); return false; }
    std::vector<std::string> recs, cars;
    Objetivo(recs, cars, false);
    if (recs.empty() && cars.empty()) return false;
    if (recs.empty()) {
        // CARPETAS VACIAS: tienen undo (no hace falta confirmar) y van todas en UN paso
        std::sort(cars.begin(), cars.end(), MasProfunda);
        std::string motivo;
        int n = 0;
        W3dVistaRecGrupoIniciar();
        for (size_t i = 0; i < cars.size(); i++) {
            std::string m;
            if (W3dCarpetaBorrar(cars[i], &m)) n++; else motivo = m;
        }
        W3dVistaRecGrupoFin();
        if (n == 0) { Notificar(T(motivo.c_str()), true); return false; }
        if (!motivo.empty()) Notificar(T(motivo.c_str()), true);   // alguna no estaba vacia
        // el cursor sube a la carpeta de arriba de la que se borro
        if (cursorEsCarpeta) {
            std::vector<OutFilaRec> f2; FilasRecursos(f2);
            if (FilaDelCursor(f2) < 0) {
                std::string c = cursorClave;
                while (!c.empty()) {
                    c = W3dCarpetaPadre(c);
                    cursorClave = c;
                    if (c.empty() || FilaDelCursor(f2) >= 0) break;
                }
                if (cursorClave.empty()) cursorEsCarpeta = false;
            }
        }
        selRecursos.clear(); selCarpetas.clear(); selMarcada = false;
        lastContentRows = -1; g_redraw = true;
        return true;
    }
    // RECURSOS: los HUERFANOS y las PARTES de un recurso (un clip de su biblioteca) se borran; los EN USO se
    // ofrecen borrar IGUAL (sus usuarios quedan sin ellos, con Ctrl+Z) si su tipo lo sabe hacer
    gBorrarClaves.clear();
    gBorrarForzado = false;
    // las CARPETAS de la misma seleccion van tambien (despues de los recursos, si quedan vacias): antes se
    // ignoraban sin avisar y la carpeta seguia ahi
    gBorrarCarpetas = cars;
    std::string motivo, nombre, info;
    int enUso = 0, usuariosEnUso = 0, mallasEnUso = 0;
    bool soloPartes = true, raices = false;
    for (size_t k = 0; k < recs.size(); k++) {
        int t = 0; std::string id; W3dRecursoItem it;
        if (!W3dBibDeClave(recs[k], &t, &id) || !W3dVistaRecInfo(t, id, &it)) continue;
        if (it.soloLectura) { motivo = "It is read-only"; continue; }
        if (!it.padre.empty()) { gBorrarClaves.push_back(recs[k]); nombre = it.nombre; continue; }
        if (t == W3D_VISTA_ESCENAS || t == W3D_VISTA_PREFABS) raices = true;
        if (it.usuarios > 0) {
            if (!W3dVistaRecSabeBorrarEnUso(t)) { motivo = "It is in use"; continue; }
            enUso++; usuariosEnUso += it.usuarios;
            if (t == W3D_VISTA_MALLAS) mallasEnUso++;
        }
        gBorrarClaves.push_back(recs[k]); nombre = it.nombre; info = it.info; soloPartes = false;
    }
    if (gBorrarClaves.empty()) {
        gBorrarCarpetas.clear();
        Notificar(T(motivo.empty() ? "It can't be deleted" : motivo.c_str()), true);
        return false;
    }
    gBorrarForzado = enUso > 0;
    if (!confirmar || soloPartes) { BorrarConfirmado(); lastContentRows = -1; return true; }   // (las partes tienen Ctrl+Z)
    if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
    char b[640];
    if (enUso > 0) {
        // EN USO: se avisa CUANTOS lo usan; "Si" = borrar igual, "No" = cancelar. Los usuarios de un material, una
        // textura o un archivo quedan SIN el; los de una MALLA 3D son objetos que no pueden quedar sin su malla: se
        // BORRAN (con Ctrl+Z), y el cartel lo dice
        if (gBorrarClaves.size() == 1 && mallasEnUso == 1) {
            snprintf(b, sizeof(b), T("\"%s\" is used by %d object(s). Delete it anyway?"), nombre.c_str(), usuariosEnUso);
            snprintf(b + strlen(b), sizeof(b) - strlen(b), " %s", T("Its objects will be deleted too (Ctrl+Z brings them back)."));
        } else if (gBorrarClaves.size() == 1)
            snprintf(b, sizeof(b), T("\"%s\" is in use by %d user(s). Delete it anyway? Its users will be left without it."), nombre.c_str(), usuariosEnUso);
        else {
            snprintf(b, sizeof(b), T("%d resources are in use (%d users). Delete them anyway? Their users will be left without them."), enUso, usuariosEnUso);
            if (mallasEnUso > 0)
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " %s", T("The objects of the 3D meshes will be deleted too (Ctrl+Z brings them back)."));
        }
    } else if (raices) {
        // una ESCENA, un JUEGO o un PREFAB entero se borra con todos sus objetos (con Ctrl+Z)
        if (gBorrarClaves.size() == 1) snprintf(b, sizeof(b), T("Delete \"%s\" (%s) with all its objects?"), nombre.c_str(), info.c_str());
        else snprintf(b, sizeof(b), T("Delete %d scenes, games or prefabs with all their objects?"), (int)gBorrarClaves.size());
    } else if (gBorrarClaves.size() == 1) {
        snprintf(b, sizeof(b), "%s \"%s\"?", T("Delete the orphan resource"), nombre.c_str());
    } else {
        snprintf(b, sizeof(b), "%s (%d)?", T("Delete the orphan resources"), (int)gBorrarClaves.size());
    }
    std::string msg = b;
    if (!gBorrarCarpetas.empty()) msg += std::string(" ") + T("The selected folders are deleted too if they end up empty.");
    confirmarPopup->Abrir(msg, BorrarConfirmado);
    return true;
}

int Outliner::AccionPurgar(bool confirmar) {
    if (VistaSoloLectura()) return 0;
    std::vector<W3dRecursoItem> items; W3dBibliotecaListar(filtro, items);
    int huerfanos = 0;
    for (size_t i = 0; i < items.size(); i++) {
        W3dProveedorRecursos* p = W3dRecursosVistaProveedor(items[i].tipo);
        if (items[i].usuarios == 0 && !items[i].soloLectura && items[i].padre.empty() && p && p->Purgable()) huerfanos++;
    }
    if (huerfanos == 0) { Notificar(T("There are no orphans"), false); return 0; }
    gPurgarTipo = filtro;
    if (confirmar) {
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        char b[32]; sprintf(b, "%d", huerfanos);
        confirmarPopup->Abrir(std::string(T("Purge orphans")) + " (" + b + ")?", PurgarConfirmado);
        return huerfanos;
    }
    std::vector<std::string> nombres;
    const int n = W3dVistaRecPurgar(filtro, &nombres);
    lastContentRows = -1; g_redraw = true;
    return n;
}

bool Outliner::AccionDuplicarRecurso() {
    if (VistaSoloLectura()) return false;
    std::vector<OutFilaRec> filas; FilasRecursos(filas);
    const int fc = FilaDelCursor(filas);
    if (fc < 0 || filas[(size_t)fc].carpeta) return false;
    const OutFilaRec& f = filas[(size_t)fc];
    std::string nuevo, motivo;
    if (!W3dVistaRecDuplicar(f.tipo, f.id, &nuevo, &motivo)) { Notificar(T(motivo.c_str()), true); return false; }
    OutFilaRec n = f; n.id = nuevo; n.clave = W3dBibClave(f.tipo, nuevo);
    SeleccionSolo(n);
    ElegirFila(n, true);
    lastContentRows = -1;
    Notificar(std::string(T("Duplicated")) + ": " + nuevo, false);
    return true;
}

// las hojas OUTLINER del layout entero (con un viewport maximizado, tambien las que no se ven)
static void JuntarOutliners(ViewportBase* n, std::vector<Outliner*>& out) {
    if (!n) return;
    if (n->isLeaf()) { if (n->ViewportKind() == 2) out.push_back((Outliner*)n); return; }
    if (n->ContainerKind() == 1) { JuntarOutliners(((ViewportRow*)n)->childA, out); JuntarOutliners(((ViewportRow*)n)->childB, out); }
    else { JuntarOutliners(((ViewportColumn*)n)->childA, out); JuntarOutliners(((ViewportColumn*)n)->childB, out); }
}

// Ctrl+Z / Ctrl+Y en medio del MODO MOVER de la biblioteca (W3dVistaRecAntesDeUndo): el outliner que lo tiene
// abierto lo cancela, como con Esc (el grupo de undo que abrio no puede absorber lo que se deshaga)
static void CancelarModosMoverRec() {
    std::vector<Outliner*> outs;
    JuntarOutliners(LayoutRaizCompleta(), outs);
    for (size_t i = 0; i < outs.size(); i++) if (outs[i]->moviendoRec) outs[i]->MoverRecCancelar();
}
static bool gGanchoUndoOk = (W3dVistaRecGrupoCancelarHook = CancelarModosMoverRec, true);

// "Select Users" (el menu del outliner y el boton de la tarjeta de Properties: los dos hacen lo mismo). Los
// usuarios se ven en la ESCENA: los outliners que miraban la biblioteca vuelven al arbol, centrados en la
// seleccion. Sin OBJETOS que seleccionar se avisa y no se toca nada.
int OutlinerSeleccionarUsuarios(const std::vector<std::string>& claves) {
    if (claves.empty()) return 0;
    const int n = W3dBibSeleccionarUsuarios(claves);   // la UNION de los usuarios
    if (n == 0) { Notificar(T("No object of the scene uses it"), true); return 0; }
    std::vector<Outliner*> outs;
    JuntarOutliners(LayoutRaizCompleta(), outs);
    for (size_t i = 0; i < outs.size(); i++) {
        if (outs[i]->vista == OUT_VISTA_ESCENA) continue;
        outs[i]->CambiarVista(OUT_VISTA_ESCENA);
        outs[i]->Resize(outs[i]->width, outs[i]->height);
        outs[i]->CentrarSeleccion();
    }
    char b[32]; sprintf(b, "%d", n);
    Notificar(std::string(T("Users selected")) + ": " + b, false);
    g_redraw = true;
    return n;
}

int Outliner::AccionSeleccionarUsuarios() {
    std::vector<std::string> recs, cars;
    Objetivo(recs, cars, false);
    return OutlinerSeleccionarUsuarios(recs);
}

bool Outliner::AccionMover(const std::string& carpeta) {
    // la SELECCION entera (recursos y carpetas) o el cursor, en un paso de undo
    if (!MoverObjetivo(carpeta)) return false;
    std::vector<OutFilaRec> f2; FilasRecursos(f2);
    AsegurarVisibleFila(FilaDelCursor(f2));
    return true;
}
