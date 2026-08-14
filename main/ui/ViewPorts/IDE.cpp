#include "ViewPorts/IDE.h"
#include "w3dGraphics.h"                 // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"       // W3dPantallaAlto
#include "WhiskUI/theme/colores.h"       // ListaColores / ColorID
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText
#include "WhiskUI/core/UI.h"             // marginGS / RenglonHeightGS / LetterWidthGS / GlobalScale / borderGS
#include "objects/Textures.h"            // Textures[0] = atlas de la fuente
#include "render/OpcionesRender.h"       // g_redraw (render event-driven)
#include "script/W3dScript.h"            // W3dScriptDatos (rutas de scripts de los objetos)
#include "ViewPorts/Notificaciones.h"    // toasts de guardado / refresh
#include "w3dFilesystem.h"               // ReadTextFile / ListDir (colector + abrir)
#include "io/W3dContenedor.h"            // los .lua del proyecto v4 viven ADENTRO del .w3d
#include "W3dLang.h"                     // T() para los textos de la barra
#include "variables.h"                   // w3dPath (el proyecto abierto)
#include <stdio.h>                       // guardado atomico: fopen/fwrite/rename

namespace gfx = w3dEngine;

// ---------------------------------------------------------------------------
//  Colores del RESALTADO (fijos, legibles sobre el fondo oscuro; la paleta del
//  tema no tiene roles de sintaxis, mismo criterio que los [ERROR] de Console)
// ---------------------------------------------------------------------------
static const float kIdeComentario[4] = { 0.52f, 0.66f, 0.50f, 1.0f }; // verde grisado
static const float kIdeString[4]     = { 0.93f, 0.78f, 0.38f, 1.0f }; // amarillo/naranja
static const float kIdeKeyword[4]    = { 0.52f, 0.72f, 0.98f, 1.0f }; // azul
static const float kIdeNumero[4]     = { 0.55f, 0.88f, 0.94f, 1.0f }; // celeste
static const float kIdeSeleccion[4]  = { 0.22f, 0.36f, 0.55f, 1.0f }; // fondo de la seleccion

// clipboard INTERNO: respaldo si el clipboard de SDL no esta (Symbian / headless)
static std::string gIDEClipInterno;

// ---------------------------------------------------------------------------
//  helpers chicos (C++03, sin to_string)
// ---------------------------------------------------------------------------
static std::string IdeNum(int v) {
    char buf[16];
    sprintf(buf, "%d", v);
    return std::string(buf);
}
static bool IdeTerminaEn(const std::string& s, const char* suf) {
    size_t ls = s.size(), lf = 0;
    while (suf[lf]) lf++;
    if (lf > ls) return false;
    for (size_t i = 0; i < lf; i++) {
        char a = s[ls - lf + i], b = suf[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32); // comparar sin caso (.LUA)
        if (a != b) return false;
    }
    return true;
}
static std::string IdeNormalizar(const std::string& r) {
    std::string s = r;
    for (size_t i = 0; i < s.size(); i++) if (s[i] == '\\') s[i] = '/';
    return s;
}
// las dos rutas nombran el MISMO archivo? (exacta, o una es sufijo de la otra
// con borde de '/': "contenido/juego.lua" == "/abs/proj/contenido/juego.lua")
static bool IdeMismoArchivo(const std::string& a, const std::string& b) {
    std::string na = IdeNormalizar(a), nb = IdeNormalizar(b);
    if (na == nb) return true;
    if (na.size() > nb.size()) {
        return nb.size() > 0 && na.size() > nb.size() &&
               na.compare(na.size() - nb.size(), nb.size(), nb) == 0 &&
               na[na.size() - nb.size() - 1] == '/';
    }
    if (nb.size() > na.size()) {
        return na.size() > 0 &&
               nb.compare(nb.size() - na.size(), na.size(), na) == 0 &&
               nb[nb.size() - na.size() - 1] == '/';
    }
    return false;
}
static std::string IdeCarpetaProyecto() {
    if (w3dPath.empty()) return std::string();
    size_t s = w3dPath.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : w3dPath.substr(0, s);
}

std::string IDENombreScript(const std::string& ruta) {
    std::string r = IdeNormalizar(ruta);
    std::string dir = IdeNormalizar(IdeCarpetaProyecto());
    if (!dir.empty() && r.size() > dir.size() + 1 &&
        r.compare(0, dir.size(), dir) == 0 && r[dir.size()] == '/')
        return r.substr(dir.size() + 1);        // relativo al proyecto
    size_t s = r.find_last_of('/');
    return (s == std::string::npos) ? r : r.substr(s + 1); // solo el nombre
}

// ---------------------------------------------------------------------------
//  COLECTOR de scripts del proyecto: los .lua referenciados por las escenas
//  cargadas (scriptDatos de todo el arbol, en orden de arbol) + los *.lua de
//  contenido/. Sin repetidos (comparados con IdeMismoArchivo). No se reusa el
//  colector de GuardarVersion porque ese re-parsea el .w3d del DISCO; aca el
//  arbol vivo ya tiene las rutas resueltas (y cubre lo editado sin guardar).
// ---------------------------------------------------------------------------
static void IdeAgregarRuta(const std::string& ruta, std::vector<std::string>* out) {
    if (!IdeTerminaEn(ruta, ".lua")) return; // .luac (bytecode) no se edita como texto
    for (size_t i = 0; i < out->size(); i++)
        if (IdeMismoArchivo((*out)[i], ruta)) return;
    out->push_back(ruta);
}
static void IdeColectarEscena(Object* o, std::vector<std::string>* out) {
    if (!o) return;
    if (o->scriptDatos)
        for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++)
            IdeAgregarRuta(o->scriptDatos->scripts[i].ruta, out);
    for (size_t i = 0; i < o->Childrens.size(); i++)
        IdeColectarEscena(o->Childrens[i], out);
}
void IDEColectarScripts(std::vector<std::string>* rutas) {
    rutas->clear();
    IdeColectarEscena(SceneCollection, rutas);
    // DE DONDE SALEN LOS .lua QUE NO CUELGAN DE NINGUN OBJETO:
    //  - proyecto v4 (el .w3d es un contenedor): de la carpeta scripts/ de ADENTRO.
    //    Ya no se escanea contenido/ en el disco, que con el contenedor no existe
    //    (y si existe es la carpeta VIEJA del proyecto v3 que quedo al lado: listarla
    //    mostraba scripts fantasma que no son los que el proyecto usa).
    //  - proyecto v3 (JSON plano con los assets sueltos): contenido/, como siempre.
    if (W3dContenedorHayMontado()) {
        std::vector<std::string> ents;
        W3dContenedorListarCarpeta("scripts/", ents);
        for (size_t i = 0; i < ents.size(); i++)
            if (IdeTerminaEn(ents[i], ".lua")) IdeAgregarRuta(ents[i], rutas);
        return;
    }
    std::string dir = IdeCarpetaProyecto();
    if (dir.empty()) return;
    std::string carpeta = dir + "/contenido";
    std::vector<w3dFileSystem::DirEntry> ents;
    if (w3dFileSystem::ListDir(carpeta, ents))
        for (size_t i = 0; i < ents.size(); i++)
            if (!ents[i].isDir && IdeTerminaEn(ents[i].name, ".lua"))
                IdeAgregarRuta(carpeta + "/" + ents[i].name, rutas);
}

// ---------------------------------------------------------------------------
//  LEXER lua por linea. Ver las LIMITACIONES en IDE.h: el estado entre lineas
//  es SOLO el comentario de bloque --[[ ]] (sin niveles [==[), y el string
//  largo [[...]] se pinta solo dentro de su linea.
// ---------------------------------------------------------------------------
static bool IdeEsIdent(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
static bool IdeEsKeyword(const std::string& p) {
    static const char* kw[] = {
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "goto", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while", 0 };
    for (int i = 0; kw[i]; i++) if (p == kw[i]) return true;
    return false;
}
static void IdeMarcar(std::vector<unsigned char>* tipos, size_t d, size_t h, unsigned char t) {
    if (!tipos) return;
    for (size_t i = d; i < h && i < tipos->size(); i++) (*tipos)[i] = t;
}
void IDE::LexLinea(const std::string& s, bool* enBloque, std::vector<unsigned char>* tipos) {
    const size_t n = s.size();
    if (tipos) tipos->assign(n, 0);
    bool bloque = enBloque ? *enBloque : false;
    size_t i = 0;
    while (i < n) {
        if (bloque) { // dentro de --[[ ... : comentario hasta el "]]"
            size_t fin = s.find("]]", i);
            if (fin == std::string::npos) { IdeMarcar(tipos, i, n, 1); i = n; }
            else { IdeMarcar(tipos, i, fin + 2, 1); i = fin + 2; bloque = false; }
            continue;
        }
        char c = s[i];
        if (c == '-' && i + 1 < n && s[i + 1] == '-') {           // comentario
            if (i + 3 < n && s[i + 2] == '[' && s[i + 3] == '[') { // --[[ (bloque)
                size_t fin = s.find("]]", i + 4);
                if (fin == std::string::npos) { IdeMarcar(tipos, i, n, 1); bloque = true; i = n; }
                else { IdeMarcar(tipos, i, fin + 2, 1); i = fin + 2; }
            } else { IdeMarcar(tipos, i, n, 1); i = n; }           // -- hasta el final
            continue;
        }
        if (c == '"' || c == '\'') {                               // string "..." '...'
            size_t j = i + 1;
            while (j < n) {
                if (s[j] == '\\') { j += 2; continue; }            // escape
                if (s[j] == c) { j++; break; }
                j++;
            }
            if (j > n) j = n;
            IdeMarcar(tipos, i, j, 2); i = j; continue;
        }
        if (c == '[' && i + 1 < n && s[i + 1] == '[') {            // [[...]] (misma linea)
            size_t fin = s.find("]]", i + 2);
            size_t j = (fin == std::string::npos) ? n : fin + 2;
            IdeMarcar(tipos, i, j, 2); i = j; continue;
        }
        if (c >= '0' && c <= '9' && (i == 0 || !IdeEsIdent(s[i - 1]))) { // numero
            size_t j = i;
            if (c == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
                j = i + 2;
                while (j < n && ((s[j] >= '0' && s[j] <= '9') ||
                                 (s[j] >= 'a' && s[j] <= 'f') || (s[j] >= 'A' && s[j] <= 'F'))) j++;
            } else {
                while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == '.' ||
                                 s[j] == 'e' || s[j] == 'E' ||
                                 ((s[j] == '+' || s[j] == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E')))) j++;
            }
            IdeMarcar(tipos, i, j, 4); i = j; continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') { // palabra
            size_t j = i;
            while (j < n && IdeEsIdent(s[j])) j++;
            if (IdeEsKeyword(s.substr(i, j - i))) IdeMarcar(tipos, i, j, 3);
            i = j; continue;
        }
        i++;
    }
    if (enBloque) *enBloque = bloque;
}

void IDE::RecalcularBloques() {
    bloqueAlInicio.assign(lineas.size(), 0);
    bool b = false;
    for (size_t i = 0; i < lineas.size(); i++) {
        bloqueAlInicio[i] = b ? 1 : 0;
        LexLinea(lineas[i], &b, NULL); // solo avanza el estado
    }
}

// ---------------------------------------------------------------------------
//  constructor / buffer
// ---------------------------------------------------------------------------
IDE::IDE() {
    sucio = false;
    curLin = 0; curCol = 0;
    haySel = false; selLin = 0; selCol = 0;
    undoUltimaOp = 0;
    lastLineas = -1; lastMaxAncho = 0;
    selDrag = false;
    lineas.push_back(std::string()); // el buffer nunca esta vacio (1 linea vacia)
    BarCrear(); // [0] = icono de tipo/split
    // botones por ROL (no por indice), como la barra del 2D/UV
    Button* b;
    b = new Button(T("Script"), -1);
    b->rol = BRIDE_Script; b->desplegable = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Save"));
    b->rol = BRIDE_Guardar;
    BarButtons.push_back(b);
    b = new Button(T("Refresh"));
    b->rol = BRIDE_Refresh;
    BarButtons.push_back(b);
    RecalcularBloques();
    // con un proyecto cargado, abrir el PRIMER script (el IDE nace mostrando codigo)
    AbrirPrimerScript();
}

IDE::~IDE() {}

void IDE::AbrirPrimerScript() {
    if (!archivo.empty()) return;
    std::vector<std::string> rutas;
    IDEColectarScripts(&rutas);
    if (!rutas.empty()) AbrirArchivo(rutas[0]);
}

// texto crudo -> lineas del buffer (sin '\r'; tab -> 4 espacios: la fuente
// pixel no tiene tabulador y el cursor por columnas necesita ancho fijo)
static void IdePartirEnLineas(const std::string& crudo, std::vector<std::string>* out) {
    out->clear();
    std::string linea;
    for (size_t i = 0; i < crudo.size(); i++) {
        char c = crudo[i];
        if (c == '\r') continue;
        if (c == '\n') { out->push_back(linea); linea.clear(); continue; }
        if (c == '\t') { linea += "    "; continue; }
        linea += c;
    }
    out->push_back(linea); // la ultima (o unica) linea; "a\n" -> ["a", ""]
    if (out->empty()) out->push_back(std::string());
}

bool IDE::AbrirArchivo(const std::string& ruta) {
    bool ok = false;
    std::string crudo = w3dFileSystem::ReadTextFile(ruta, &ok);
    if (!ok) {
        Notificar("IDE: no pude leer " + IDENombreScript(ruta), true);
        return false;
    }
    IdePartirEnLineas(crudo, &lineas);
    archivo = ruta;
    sucio = false;
    curLin = 0; curCol = 0; haySel = false; selDrag = false;
    PosX = 0; PosY = 0;
    pilaUndo.clear(); undoUltimaOp = 0;
    lastLineas = -1; // fuerza el recalculo del scroll en el proximo Render
    RecalcularBloques();
    g_redraw = true;
    return true;
}

std::string IDE::GetTexto() const {
    std::string t;
    for (size_t i = 0; i < lineas.size(); i++) {
        if (i) t += '\n';
        t += lineas[i];
    }
    return t;
}

void IDE::SetTexto(const std::string& texto) {
    IdePartirEnLineas(texto, &lineas);
    curLin = 0; curCol = 0; haySel = false;
    pilaUndo.clear(); undoUltimaOp = 0;
    sucio = true;
    lastLineas = -1;
    RecalcularBloques();
    g_redraw = true;
}

// REGLA: el IDE NUNCA rompe el archivo. Guardado ATOMICO: se escribe TODO a un
// .w3dtmp al lado y recien si salio completo se renombra sobre el .lua. Si el
// disco falla a mitad de camino, el original queda intacto.
bool IDE::Guardar() {
    if (archivo.empty()) return false;
    std::string t = GetTexto();
    // ---------------------------------------------------------------------
    //  EL SCRIPT VIVE ADENTRO DEL .w3d: se guarda A SU ENTRADA.
    //  'archivo' aca no es una ruta de disco sino un nombre de entrada
    //  ("scripts/menu.lua"): el fopen de abajo crearia una carpeta scripts/
    //  RELATIVA AL DIRECTORIO ACTUAL (o sea al lado del binario del editor) y el
    //  cambio no llegaria nunca al proyecto. Va al overlay del contenedor, que lo
    //  sirve en caliente a todo el editor y lo hornea al zip al guardar el proyecto.
    // ---------------------------------------------------------------------
    if (W3dEsNombreDeEntrada(archivo)) {
        if (!W3dContenedorEscribirEntrada(archivo, t.empty() ? "" : t.c_str(), t.size())) {
            Notificar("IDE: no pude guardar " + IDENombreScript(archivo) +
                      " adentro del proyecto", true);
            return false;
        }
        sucio = false;
        Notificar(IDENombreScript(archivo) + " guardado (guarda el proyecto para que quede en el .w3d)", false);
        g_redraw = true;
        return true;
    }
    std::string tmp = archivo + ".w3dtmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) {
        Notificar("IDE: no pude escribir " + IDENombreScript(archivo), true);
        return false;
    }
    size_t esc = t.empty() ? 0 : fwrite(t.c_str(), 1, t.size(), f);
    bool ok = (fclose(f) == 0) && (esc == t.size());
    if (!ok) {
        remove(tmp.c_str());
        Notificar("IDE: fallo el guardado de " + IDENombreScript(archivo), true);
        return false;
    }
#ifdef _WIN32
    remove(archivo.c_str()); // en Windows rename no pisa un destino existente
#endif
    if (rename(tmp.c_str(), archivo.c_str()) != 0) {
        remove(tmp.c_str());
        Notificar("IDE: fallo el guardado de " + IDENombreScript(archivo), true);
        return false;
    }
    sucio = false;
    Notificar(IDENombreScript(archivo) + " guardado", false);
    g_redraw = true;
    return true;
}

// REFRESH: guardar + aplicar EN VIVO. Comportamiento elegido (predecible):
//  - juego CORRIENDO (SimActiva): se recargan SOLO los objetos cuyos scripts
//    usan el archivo editado (SimScriptsCambiados): sus lua_State se recrean
//    (variables locales de cero + inicio() de nuevo) y el RESTO del juego
//    (posiciones, otros scripts, escena activa) se PRESERVA tal cual. No se
//    reinicia la partida: es la misma semantica que ya tiene cambiar el .lua
//    de un objeto desde la tarjeta Script con el juego andando.
//  - juego parado: refresh = solo guardar (el proximo Play ya lee lo nuevo).
void IDE::Refresh() {
    if (archivo.empty()) return;
    if (!Guardar()) return;
#ifndef W3D_SYMBIAN
    extern bool SimActiva();
    extern void SimScriptsCambiados(Object*);
    if (!SimActiva()) return;
    // juntar primero (SimScriptsCambiados puede agregar a la lista interna de la sim)
    std::vector<Object*> usan;
    std::vector<Object*> pila;
    if (SceneCollection) pila.push_back(SceneCollection);
    while (!pila.empty()) {
        Object* o = pila.back(); pila.pop_back();
        if (o->scriptDatos)
            for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++)
                if (IdeMismoArchivo(o->scriptDatos->scripts[i].ruta, archivo)) {
                    usan.push_back(o);
                    break;
                }
        for (size_t i = 0; i < o->Childrens.size(); i++) pila.push_back(o->Childrens[i]);
    }
    for (size_t i = 0; i < usan.size(); i++) SimScriptsCambiados(usan[i]);
    Notificar("Refresh: " + IdeNum((int)usan.size()) + " objeto(s) recargados en vivo", false);
    g_redraw = true;
#endif
}

// ---------------------------------------------------------------------------
//  undo: snapshots del buffer entero (los .lua de un juego son chicos), en una
//  pila AGRUPADA por operacion: una corrida de tipeo = UN undo; backspace
//  sostenido = UN undo; pegar/cortar/enter = un undo cada uno. Sin redo
//  (limitacion documentada; el Ctrl+Z global del editor no pasa por aca).
// ---------------------------------------------------------------------------
void IDE::Mutar(int op) {
    if (op == 0 || op != undoUltimaOp || pilaUndo.empty()) {
        EstadoTexto e;
        e.lineas = lineas; e.curLin = curLin; e.curCol = curCol;
        pilaUndo.push_back(e);
        if (pilaUndo.size() > 200) pilaUndo.erase(pilaUndo.begin());
    }
    undoUltimaOp = op;
    sucio = true;
}

void IDE::DeshacerTexto() {
    if (pilaUndo.empty()) return;
    EstadoTexto& e = pilaUndo.back();
    lineas = e.lineas;
    curLin = e.curLin; curCol = e.curCol;
    pilaUndo.pop_back();
    haySel = false;
    undoUltimaOp = 0;
    sucio = true; // no se compara contra el disco: deshacer sigue siendo "sin guardar"
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  cursor / seleccion
// ---------------------------------------------------------------------------
void IDE::NormalizarSel(int* l0, int* c0, int* l1, int* c1) const {
    if (selLin < curLin || (selLin == curLin && selCol <= curCol)) {
        *l0 = selLin; *c0 = selCol; *l1 = curLin; *c1 = curCol;
    } else {
        *l0 = curLin; *c0 = curCol; *l1 = selLin; *c1 = selCol;
    }
}

void IDE::CursorA(int lin, int col, bool shift) {
    if (shift && !haySel) { selLin = curLin; selCol = curCol; haySel = true; }
    if (lin < 0) lin = 0;
    if (lin >= (int)lineas.size()) lin = (int)lineas.size() - 1;
    if (col < 0) col = 0;
    if (col > (int)lineas[lin].size()) col = (int)lineas[lin].size();
    curLin = lin; curCol = col;
    if (!shift) haySel = false;
    if (haySel && selLin == curLin && selCol == curCol) haySel = false;
    undoUltimaOp = 0; // moverse corta el grupo de tipeo del undo
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::MoverCursor(int dLin, int dCol, bool shift) {
    int lin = curLin, col = curCol;
    if (dCol) {
        col += dCol;
        if (col < 0) { // cruzar al final de la linea anterior
            if (lin > 0) { lin--; col = (int)lineas[lin].size(); }
            else col = 0;
        } else if (col > (int)lineas[lin].size()) { // cruzar al inicio de la siguiente
            if (lin + 1 < (int)lineas.size()) { lin++; col = 0; }
            else col = (int)lineas[lin].size();
        }
    }
    if (dLin) lin += dLin; // CursorA clampea la columna a la linea nueva
    CursorA(lin, col, shift);
}

void IDE::Home(bool shift) { CursorA(curLin, 0, shift); }
void IDE::End(bool shift)  { CursorA(curLin, (int)lineas[curLin].size(), shift); }

void IDE::PaginaArribaAbajo(int dir, bool shift) {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int visibles = (height - BarTopOffset()) / lh;
    if (visibles < 1) visibles = 1;
    CursorA(curLin + dir * visibles, curCol, shift);
}

void IDE::SeleccionarTodo() {
    selLin = 0; selCol = 0;
    curLin = (int)lineas.size() - 1;
    curCol = (int)lineas[curLin].size();
    haySel = !(curLin == 0 && curCol == 0);
    undoUltimaOp = 0;
    g_redraw = true;
}

std::string IDE::TextoSeleccionado() const {
    if (!haySel) return std::string();
    int l0, c0, l1, c1;
    NormalizarSel(&l0, &c0, &l1, &c1);
    if (l0 == l1) return lineas[l0].substr(c0, c1 - c0);
    std::string t = lineas[l0].substr(c0);
    for (int l = l0 + 1; l < l1; l++) { t += '\n'; t += lineas[l]; }
    t += '\n'; t += lineas[l1].substr(0, c1);
    return t;
}

// saca la seleccion del buffer SIN tocar el undo (los llamadores agrupan)
static void IdeBorrarRango(std::vector<std::string>* lineas, int l0, int c0, int l1, int c1) {
    if (l0 == l1) {
        (*lineas)[l0].erase(c0, c1 - c0);
        return;
    }
    (*lineas)[l0] = (*lineas)[l0].substr(0, c0) + (*lineas)[l1].substr(c1);
    lineas->erase(lineas->begin() + l0 + 1, lineas->begin() + l1 + 1);
}

void IDE::BorrarSeleccion() {
    if (!haySel) return;
    Mutar(0); // operacion propia (siempre corta grupo)
    int l0, c0, l1, c1;
    NormalizarSel(&l0, &c0, &l1, &c1);
    IdeBorrarRango(&lineas, l0, c0, l1, c1);
    curLin = l0; curCol = c0; haySel = false;
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  edicion
// ---------------------------------------------------------------------------
void IDE::InsertarChar(char c) {
    if (haySel) {
        Mutar(0); // reemplazo de la seleccion: UNA operacion
        int l0, c0, l1, c1;
        NormalizarSel(&l0, &c0, &l1, &c1);
        IdeBorrarRango(&lineas, l0, c0, l1, c1);
        curLin = l0; curCol = c0; haySel = false;
        undoUltimaOp = 1; // lo que se siga tipeando se agrupa con este snapshot
    } else {
        Mutar(1); // tipeo: agrupa la corrida
    }
    lineas[curLin].insert((size_t)curCol, 1, c);
    curCol++;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::InsertarTexto(const std::string& t) {
    Mutar(0);
    if (haySel) {
        int l0, c0, l1, c1;
        NormalizarSel(&l0, &c0, &l1, &c1);
        IdeBorrarRango(&lineas, l0, c0, l1, c1);
        curLin = l0; curCol = c0; haySel = false;
    }
    std::vector<std::string> nuevas;
    IdePartirEnLineas(t, &nuevas);
    std::string cola = lineas[curLin].substr((size_t)curCol);
    lineas[curLin] = lineas[curLin].substr(0, (size_t)curCol) + nuevas[0];
    for (size_t i = 1; i < nuevas.size(); i++) {
        curLin++;
        lineas.insert(lineas.begin() + curLin, nuevas[i]);
    }
    curCol = (int)lineas[curLin].size();
    lineas[curLin] += cola;
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::Enter() {
    if (haySel) { InsertarTexto("\n"); return; }
    Mutar(0);
    std::string cola = lineas[curLin].substr((size_t)curCol);
    lineas[curLin].erase((size_t)curCol);
    curLin++;
    lineas.insert(lineas.begin() + curLin, cola);
    curCol = 0;
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::TabKey() { InsertarTexto("    "); } // 4 espacios (la fuente no tiene tab)

void IDE::Backspace() {
    if (haySel) { BorrarSeleccion(); return; }
    if (curCol == 0 && curLin == 0) return;
    Mutar(2); // los backspace seguidos se agrupan
    if (curCol > 0) {
        lineas[curLin].erase((size_t)(curCol - 1), 1);
        curCol--;
    } else { // unir con la linea anterior
        curCol = (int)lineas[curLin - 1].size();
        lineas[curLin - 1] += lineas[curLin];
        lineas.erase(lineas.begin() + curLin);
        curLin--;
        lastLineas = -1;
    }
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::DelForward() {
    if (haySel) { BorrarSeleccion(); return; }
    if (curCol >= (int)lineas[curLin].size() && curLin + 1 >= (int)lineas.size()) return;
    Mutar(3); // los delete seguidos se agrupan
    if (curCol < (int)lineas[curLin].size()) {
        lineas[curLin].erase((size_t)curCol, 1);
    } else { // unir con la linea siguiente
        lineas[curLin] += lineas[curLin + 1];
        lineas.erase(lineas.begin() + curLin + 1);
        lastLineas = -1;
    }
    RecalcularBloques();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  clipboard (el REAL de SDL en PC; interno como respaldo/Symbian)
// ---------------------------------------------------------------------------
void IDE::Copiar() {
    if (!haySel) return;
    gIDEClipInterno = TextoSeleccionado();
#ifndef W3D_SYMBIAN
    SDL_SetClipboardText(gIDEClipInterno.c_str());
#endif
}

void IDE::Cortar() {
    if (!haySel) return;
    Copiar();
    BorrarSeleccion();
}

void IDE::Pegar() {
    std::string t;
#ifndef W3D_SYMBIAN
    char* c = SDL_GetClipboardText(); // el clipboard REAL (de cualquier programa)
    if (c) { t = c; SDL_free(c); }
#endif
    if (t.empty()) t = gIDEClipInterno; // headless / sin clipboard de sistema
    if (t.empty()) return;
    // solo ASCII imprimible + saltos (la fuente pixel del editor es ASCII)
    std::string filtrado;
    for (size_t i = 0; i < t.size(); i++) {
        char c2 = t[i];
        if (c2 == '\n' || c2 == '\t' || (c2 >= 32 && (unsigned char)c2 < 127)) filtrado += c2;
    }
    if (!filtrado.empty()) InsertarTexto(filtrado);
}

// ---------------------------------------------------------------------------
//  teclado: el ruteo de PC (controles.cpp) le da al IDE enfocado por hover la
//  PRIMERA palabra: si TeclaEditor consume, ningun atajo global corre.
// ---------------------------------------------------------------------------
bool IDE::TeclaEditor(int tecla, bool repeticion) {
    (void)repeticion;
    bool shift = LShiftPressed;
    if (LCtrlPressed) {
        switch (tecla) {
            case W3dK_C: Copiar(); return true;
            case W3dK_X: Cortar(); return true;
            case W3dK_V: Pegar(); return true;
            case W3dK_A: SeleccionarTodo(); return true;
            case W3dK_Z: DeshacerTexto(); return true;             // undo DEL TEXTO, no el global
            case W3dK_S: Guardar(); return true;                   // guarda el .lua, no el proyecto
            case W3dK_HOME: CursorA(0, 0, shift); return true;     // inicio del archivo
            case W3dK_END:
                CursorA((int)lineas.size() - 1, (int)lineas.back().size(), shift);
                return true;
        }
        return false; // otros Ctrl+... siguen su camino normal
    }
    switch (tecla) {
        case W3dK_LEFT:  MoverCursor(0, -1, shift); return true;
        case W3dK_RIGHT: MoverCursor(0, +1, shift); return true;
        case W3dK_UP:    MoverCursor(-1, 0, shift); return true;
        case W3dK_DOWN:  MoverCursor(+1, 0, shift); return true;
        case W3dK_HOME:  Home(shift); return true;
        case W3dK_END:   End(shift); return true;
        case W3dK_PGUP:  PaginaArribaAbajo(-1, shift); return true;
        case W3dK_PGDN:  PaginaArribaAbajo(+1, shift); return true;
        case W3dK_RETURN:
        case W3dK_KP_ENTER: Enter(); return true;
        case W3dK_TAB:      TabKey(); return true;
        case W3dK_BACKSPACE: Backspace(); return true;
        case W3dK_DELETE:    DelForward(); return true;
        case W3dK_ESCAPE:
            if (haySel) { haySel = false; g_redraw = true; return true; }
            return false; // sin seleccion, el ESC sigue siendo global (pausa, etc.)
    }
    // imprimibles: el CARACTER entra por SDL_TEXTINPUT (respeta el layout del
    // teclado); aca solo se CONSUME el keydown para que no dispare atajos
    // del editor (g/r/s/x moverian la escena mientras escribis codigo)
    if (tecla >= 32 && tecla < 127) return true;
    return false; // F11, modificadores, etc: siguen su camino
}

void IDE::TextoTipeado(const char* utf8) {
    if (!utf8) return;
    for (const char* p = utf8; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c >= 32 && c < 127) InsertarChar((char)c);
        // fuera de ASCII (acentos, multibyte) se ignora: la fuente es ASCII
    }
}

void IDE::event_key_down(int tecla, bool repeticion) {
    // ruta normal de viewports (Symbian / fallback): mismas teclas de edicion
    TeclaEditor(tecla, repeticion);
}

// ---------------------------------------------------------------------------
//  mouse / touch
// ---------------------------------------------------------------------------
int IDE::GutterW() const {
    int n = (int)lineas.size(), d = 1;
    while (n >= 10) { n /= 10; d++; }
    if (d < 2) d = 2;
    return borderGS + marginGS + d * LetterWidthGS + marginGS;
}

// click en el CONTENIDO: posiciona el cursor (shift extiende la seleccion) y
// arma el drag de seleccion. mx/my en coords de VENTANA.
void IDE::ClickContenido(int mx, int my, bool shift) {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int lx = mx - x, ly = my - y;
    int textX0 = GutterW() + marginGS + PosX;
    int textY0 = BarTopOffset() + marginGS + PosY;
    int lin = (ly - textY0) / lh;
    int col = (lx - textX0 + LetterWidthGS / 2) / (LetterWidthGS > 0 ? LetterWidthGS : 1);
    CursorA(lin, col, shift);
    selDrag = true;
    ViewPortClickDown = true; // el drag de seleccion congela el foco por hover
    g_redraw = true;
}

void IDE::button_left() {
    // agarre de la scrollbar (patron Console; en PC el click de contenido entra
    // por LayoutClickUI -> ClickContenido, no por aca)
    if (mouseOverScrollY) mouseOverScrollYpress = true;
    if (mouseOverScrollX) mouseOverScrollXpress = true;
}

void IDE::event_mouse_motion(int mx, int my) {
    if (leftMouseDown && selDrag) {
        // arrastrar = extender la seleccion hasta el punto del mouse
        ViewPortClickDown = true;
        const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
        int lx = mx - x, ly = my - y;
        int textX0 = GutterW() + marginGS + PosX;
        int textY0 = BarTopOffset() + marginGS + PosY;
        int lin = (ly - textY0) / lh;
        int col = (lx - textX0 + LetterWidthGS / 2) / (LetterWidthGS > 0 ? LetterWidthGS : 1);
        CursorA(lin, col, true);
        g_redraw = true;
    } else if (middleMouseDown) {
        // boton del medio = arrastrar el contenido (scroll 1:1, como la Console)
        ViewPortClickDown = true;
        ScrollByTouch(mx - lastMouseX, my - lastMouseY);
        g_redraw = true;
    }
}

#ifndef W3D_SYMBIAN
// IMPRESCINDIBLE (regla de la casa): soltar libera ViewPortClickDown. Sin esto
// el foco por hover queda CLAVADO en este viewport (bug conocido de Console).
void IDE::mouse_button_up(int boton) {
    ViewPortClickDown = false;
    if (boton == W3dMB_IZQ) {
        selDrag = false;
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
}

void IDE::event_mouse_wheel(float dy, int mx, int my) {
    if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; // sobre la barra superior
    int paso = (int)(dy * 6 * GlobalScale);
    if (LShiftPressed) ScrollByTouch(paso, 0); // shift+rueda = horizontal
    else               ScrollByTouch(0, paso);
    g_redraw = true;
}
#endif

bool IDE::event_finger_scroll(int px, int py, int dx, int dy) {
    (void)px; (void)py;
    ScrollByTouch(dx, dy);
    g_redraw = true;
    return true;
}

// ---------------------------------------------------------------------------
//  scroll
// ---------------------------------------------------------------------------
void IDE::RecalcularScroll() {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int n = (int)lineas.size();
    ResizeScrollbar(width, height, lastMaxAncho, -(n * lh + marginGS * 2), BarTopOffset());
}

void IDE::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);
    RecalcularScroll();
}

void IDE::AsegurarCursorVisible() {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int visH = height - BarTopOffset() - marginGS * 2;
    int visW = width - GutterW() - marginGS * 2 - GlobalScale * 9; // colchon de la barra vertical
    int cy = curLin * lh;
    if (cy + PosY < 0) PosY = -cy;
    if (visH > lh && cy + PosY > visH - lh) PosY = visH - lh - cy;
    if (PosY > 0) PosY = 0;
    int cx = curCol * LetterWidthGS;
    if (cx + PosX < 0) PosX = -cx;
    if (visW > LetterWidthGS && cx + PosX > visW - LetterWidthGS) PosX = visW - LetterWidthGS - cx;
    if (PosX > 0) PosX = 0;
    // el clamp contra MaxPosX/MaxPosY fino lo hace ResizeScrollbar en el Render
}

// ---------------------------------------------------------------------------
//  render
// ---------------------------------------------------------------------------
void IDE::SincronizarBarra() {
    Button* b = BarRolBtn(BarButtons, BRIDE_Script);
    if (!b) return;
    std::string t = archivo.empty() ? std::string(T("Script")) : IDENombreScript(archivo);
    if (sucio) t += " *"; // asterisco = cambios sin guardar
    if (b->text != t) b->text = t;
}

// un rectangulo lleno en coords locales del viewport (sin textura)
static void IdeQuad(float x0, float y0, float x1, float y1, const float* rgba) {
    gfx::Color4fv(rgba);
    float quad[12] = { x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1 };
    gfx::VertexPointer2f(0, quad);
    gfx::DrawTrianglesArray(6);
}

static void IdeTexto(int px, int py, const std::string& txt, const float* rgba) {
    gfx::Color4fv(rgba);
    gfx::PushMatrix();
    gfx::Translatef((GLfloat)px, (GLfloat)py, 0);
    RenderBitmapText(txt, textAlign::left);
    gfx::PopMatrix();
}

static const float* IdeColorDeTipo(unsigned char t) {
    switch (t) {
        case 1: return kIdeComentario;
        case 2: return kIdeString;
        case 3: return kIdeKeyword;
        case 4: return kIdeNumero;
    }
    return ListaColores[static_cast<int>(ColorID::blanco)];
}

void IDE::Render() {
    SincronizarBarra();
    const int glY = W3dPantallaAlto - y - height;

    // fondo (mismo patron que los demas viewports)
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    const float* bg = ListaColores[static_cast<int>(ColorID::background)];
    gfx::ClearColor(bg[0], bg[1], bg[2], bg[3]);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);

    gfx::Viewport(x, glY, width, height);
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::MatrixMode(gfx::ModelView); gfx::LoadIdentity();
    gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Blend);
    gfx::DisableArray(gfx::NormalArray); gfx::DisableArray(gfx::ColorArray);
    gfx::EnableArray(gfx::VertexArray);
    gfx::DisableArray(gfx::TexCoordArray);

    const int top = BarTopOffset();
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    const int LW = (LetterWidthGS > 0) ? LetterWidthGS : 6;
    const int n = (int)lineas.size();
    const int gutterW = GutterW();

    // medir el contenido y recalcular el scroll SOLO si cambio (patron Console)
    int maxLen = 0;
    for (int i = 0; i < n; i++)
        if ((int)lineas[i].size() > maxLen) maxLen = (int)lineas[i].size();
    int maxAncho = gutterW + marginGS + maxLen * LW + marginGS + GlobalScale * 9;
    if (n != lastLineas || maxAncho != lastMaxAncho) {
        lastLineas = n; lastMaxAncho = maxAncho;
        RecalcularScroll();
    }

    int hVis = height - top; if (hVis < 0) hVis = 0;
    const int textX0 = gutterW + marginGS + PosX;
    const int py0 = top + marginGS + PosY;
    int primera = (0 - (py0)) / lh - 1; if (primera < 0) primera = 0;
    int ultima = primera + hVis / lh + 2; if (ultima > n) ultima = n;

    // ---- 1) quads: seleccion + caret + separador del gutter (sin textura) ----
    gfx::Scissor(x + gutterW, glY, (width > gutterW) ? width - gutterW : 0, hVis);
    if (haySel) {
        int l0, c0, l1, c1;
        NormalizarSel(&l0, &c0, &l1, &c1);
        for (int l = (l0 > primera ? l0 : primera); l <= l1 && l < ultima; l++) {
            int a = (l == l0) ? c0 : 0;
            int b = (l == l1) ? c1 : (int)lineas[l].size();
            int py = py0 + l * lh;
            float x0 = (float)(textX0 + a * LW);
            float x1 = (float)(textX0 + b * LW);
            if (b <= (int)lineas[l].size() && a == b) x1 = x0 + LW * 0.4f; // linea vacia: mostrarla igual
            IdeQuad(x0, (float)py, x1, (float)(py + lh), kIdeSeleccion);
        }
    }
    { // caret (sin parpadeo: el render es event-driven). Verde si el foco esta aca.
        int py = py0 + curLin * lh;
        float cx = (float)(textX0 + curCol * LW);
        // OJO: Scrollable tiene un MIEMBRO viewPortActive que sombrea al global -> '::'
        const float* cc = (::viewPortActive == (ViewportBase*)this)
                              ? ListaColores[static_cast<int>(ColorID::accent)]
                              : ListaColores[static_cast<int>(ColorID::grisUI)];
        float grosor = (float)((GlobalScale > 0) ? GlobalScale : 1);
        IdeQuad(cx, (float)py, cx + grosor, (float)(py + lh), cc);
    }
    // separador vertical del gutter (a lo alto de todo el contenido)
    gfx::Scissor(x, glY, width, hVis);
    IdeQuad((float)(gutterW - GlobalScale), (float)top, (float)gutterW, (float)height,
            ListaColores[static_cast<int>(ColorID::grisLinea)]);

    // ---- 2) texto (fuente): numeros de linea + codigo coloreado ----
    gfx::BindTexture(Textures[0]->iID);
    gfx::EnableArray(gfx::TexCoordArray);
    gfx::Enable(gfx::Texture2D);
    gfx::Enable(gfx::Blend);
    gfx::BlendAlpha();
#ifndef W3D_SYMBIAN
    gfx::TexFilter(false);
#endif

    // numeros de linea (solo scroll VERTICAL: el gutter no se mueve con PosX)
    gfx::Scissor(x, glY, gutterW - GlobalScale, hVis);
    for (int l = primera; l < ultima; l++) {
        std::string num = IdeNum(l + 1);
        int nx = gutterW - marginGS - (int)num.size() * LW; // alineado a la derecha
        IdeTexto(nx, py0 + l * lh, num, ListaColores[static_cast<int>(ColorID::grisUI)]);
    }

    // el codigo, por tramos de color del lexer
    gfx::Scissor(x + gutterW, glY, (width > gutterW) ? width - gutterW : 0, hVis);
    if (archivo.empty() && n == 1 && lineas[0].empty()) {
        IdeTexto(textX0, py0, "(no hay scripts .lua en el proyecto)",
                 ListaColores[static_cast<int>(ColorID::grisUI)]);
    }
    std::vector<unsigned char> tipos;
    for (int l = primera; l < ultima; l++) {
        const std::string& s = lineas[l];
        if (s.empty()) continue;
        bool blq = (l < (int)bloqueAlInicio.size()) ? (bloqueAlInicio[l] != 0) : false;
        LexLinea(s, &blq, &tipos);
        int py = py0 + l * lh;
        size_t i = 0;
        while (i < s.size()) {
            size_t j = i;
            while (j < s.size() && tipos[j] == tipos[i]) j++;
            IdeTexto(textX0 + (int)i * LW, py, s.substr(i, j - i), IdeColorDeTipo(tipos[i]));
            i = j;
        }
    }

    gfx::Disable(gfx::ScissorTest);

    RenderBar();
    DibujarBordes(this);
    DibujarScrollbar(this);
#ifdef W3D_SYMBIAN
    gfx::EnableArray(gfx::NormalArray); // baseline que asume la escena
#endif
}

// ---------------------------------------------------------------------------
//  captura global (PC): el IDE bajo el foco por hover se queda con el teclado
// ---------------------------------------------------------------------------
static IDE* IdeEnfocado() {
    if (!viewPortActive || !viewPortActive->isLeaf()) return NULL;
    if (viewPortActive->ViewportKind() != 8) return NULL;
    return (IDE*)viewPortActive;
}

bool IDETecladoCaptura(int tecla, bool repeticion) {
    IDE* ide = IdeEnfocado();
    if (!ide) return false;
    return ide->TeclaEditor(tecla, repeticion);
}

bool IDETextoTexto(const char* utf8) {
    IDE* ide = IdeEnfocado();
    if (!ide) return false;
    ide->TextoTipeado(utf8);
    return true; // enfocado: el texto es del IDE aunque se haya filtrado todo
}
