#include "io/W3dWeb.h"   // navegador: guardar = descargar
#include "app/W3dTactilEditor.h"   // W3D_SOLO_POR_CONTROLES (Symbian: el soltar de PC solo con el dedo o el mouse)
#include "ViewPorts/PopUp/ConfirmarPopup.h"   // Guardar/Refresh preguntan antes de pisar o descartar
#include "ViewPorts/PopUp/BuscarPopup.h"      // Buscar/Reemplazar: lo que se resalta
#include "ViewPorts/IDE.h"
#include "app/W3dTeclado.h"   // W3dTecladoAbrir: el teclado del celular (y el layout que se adapta)
#include "w3dGraphics.h"                 // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"       // W3dPantallaAlto
#include "WhiskUI/theme/colores.h"       // ListaColores / ColorID
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText
#include "input/W3dClipboard.h"                // portapapeles del sistema (Core): copiar/pegar con navegador/notas
#include "WhiskUI/core/UI.h"             // marginGS / RenglonHeightGS / LetterWidthGS / GlobalScale / borderGS
#include "objects/Textures.h"            // Textures[0] = atlas de la fuente
#include "render/OpcionesRender.h"       // g_redraw (render event-driven)
#include "script/W3dScript.h"            // W3dScriptDatos (rutas de scripts de los objetos)
#include "ViewPorts/Notificaciones.h"    // toasts de guardado / refresh
#include "w3dFilesystem.h"               // ReadTextFile / ListDir (colector + abrir)
#include "io/W3dContenedor.h"
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>               // SDL_StartTextInput: el teclado del celular al tocar el codigo
#endif            // los .lua del proyecto v4 viven ADENTRO del .w3d
#include "W3dLang.h"                     // T() para los textos de la barra
#include "variables.h"                   // w3dPath (el proyecto abierto)
#include "animation/Animation.h"         // w3dGetTicks: doble/triple toque
#include "ViewPorts/TransformUI.h"       // ToolbarUsaTactil: las agarraderas solo con pantalla tactil
#include "objects/Materials.h"           // un shader guardado: los materiales que lo usan se compilan de nuevo
#include "objects/ShaderEscrito.h"
#include "objects/ShaderCPU.h"            // (sin shaders: Compilar prueba el @vertice de la CPU)
#include <stdio.h>                       // guardado atomico: fopen/fwrite/rename
#include <string.h>                      // strncmp: las keywords del lexer
#include <math.h>                        // cosf/sinf: las agarraderas

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
static const float kIdeGuia[4]       = { 0.26f, 0.26f, 0.26f, 1.0f }; // lineas verticales de la sangria
static const float kIdeCoincide[4]   = { 0.42f, 0.33f, 0.08f, 1.0f }; // las coincidencias de Buscar
static const float kIdeGuiaActiva[4] = { 0.50f, 0.50f, 0.50f, 1.0f }; // la del bloque donde esta el cursor
static const int   kIdeTab = 4;                                       // ancho de un nivel de sangria (en espacios)

// clipboard INTERNO: respaldo si el clipboard de SDL no esta (Symbian / headless)
static std::string gIDEClipInterno;

// ---------------------------------------------------------------------------
//  helpers chicos (C++03, sin to_string)
// ---------------------------------------------------------------------------
static std::string IdeNum(int v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", v);
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
    if (!IdeTerminaEn(ruta, ".lua") && !IdeTerminaEn(ruta, ".glsl")) return; // .luac (bytecode) no se edita como texto
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
    // LOS SHADERS de los materiales (objects/ShaderEscrito.h): los que usa algun material (tambien el de un proyecto sin
    // guardar, en la carpeta de trabajo) y los .glsl de shaders/ adentro del proyecto
    for (size_t i = 0; i < Materials.size(); i++)
        if (Materials[i] && !Materials[i]->shaderArchivo.empty()) IdeAgregarRuta(Materials[i]->shaderArchivo, rutas);
    if (W3dContenedorHayMontado()) {
        std::vector<std::string> sh;
        W3dContenedorListarCarpeta("shaders/", sh);
        for (size_t i = 0; i < sh.size(); i++) IdeAgregarRuta(sh[i], rutas);
    }
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
// sin armar un string por palabra (las tablas generadas tienen decenas de miles de identificadores por renglon)
static bool IdeEsKeyword(const char* p, size_t n) {
    static const char* kw[] = {
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "goto", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while", 0 };
    if (n < 2 || n > 8) return false;
    for (int i = 0; kw[i]; i++) if (strncmp(p, kw[i], n) == 0 && kw[i][n] == 0) return true;
    return false;
}
static void IdeMarcar(std::vector<unsigned char>* tipos, size_t d, size_t h, unsigned char t) {
    if (!tipos) return;
    for (size_t i = d; i < h && i < tipos->size(); i++) (*tipos)[i] = t;
}
// LOS SHADERS (.glsl, objects/ShaderEscrito.h) se pintan como GLSL: comentarios // y /* */, numeros, los tipos y las
// palabras del lenguaje, y las marcas @vertice / @fragmento. Lo prende quien lexea un archivo .glsl (gIdeLexGLSL)
static bool gIdeLexGLSL = false;
static bool IdeEsKeywordGLSL(const char* p, size_t n) {
    static const char* kw[] = {
        "void", "float", "int", "bool", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4",
        "mat2", "mat3", "mat4", "sampler2D", "samplerCube", "in", "out", "inout", "const", "return", "if", "else", "for",
        "while", "do", "break", "continue", "discard", "true", "false", "struct", "uniform", "varying", "attribute",
        "precision", "highp", "mediump", "lowp", "@vertice", "@fragmento", 0 };
    for (int i = 0; kw[i]; i++) if (strncmp(p, kw[i], n) == 0 && kw[i][n] == 0) return true;
    return false;
}
static void LexLineaGLSL(const std::string& s, bool* enBloque, std::vector<unsigned char>* tipos, size_t hasta) {
    const size_t n = s.size();
    if (tipos) tipos->assign(hasta < n ? hasta : n, 0);
    bool bloque = enBloque ? *enBloque : false;
    size_t i = 0;
    while (i < n && i < hasta) {
        if (bloque) {   // adentro de /* ... */
            size_t fin = s.find("*/", i);
            if (fin == std::string::npos) { IdeMarcar(tipos, i, n, 1); i = n; }
            else { IdeMarcar(tipos, i, fin + 2, 1); i = fin + 2; bloque = false; }
            continue;
        }
        const char c = s[i];
        if (c == '/' && i + 1 < n && s[i + 1] == '/') { IdeMarcar(tipos, i, n, 1); i = n; continue; }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') { bloque = true; IdeMarcar(tipos, i, i + 2, 1); i += 2; continue; }
        if (c >= '0' && c <= '9' && (i == 0 || !IdeEsIdent(s[i - 1]))) {
            size_t j = i;
            while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == '.' || s[j] == 'e' || s[j] == 'E' ||
                             ((s[j] == '+' || s[j] == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E')))) j++;
            IdeMarcar(tipos, i, j, 4); i = j; continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '@' || c == '#') {
            size_t j = i + 1;
            while (j < n && IdeEsIdent(s[j])) j++;
            // (las directivas #define / #ifdef y las w3d_* del motor como las palabras del lenguaje)
            if (tipos && (c == '#' || IdeEsKeywordGLSL(s.c_str() + i, j - i) || (j - i > 4 && strncmp(s.c_str() + i, "w3d_", 4) == 0)))
                IdeMarcar(tipos, i, j, 3);
            i = j; continue;
        }
        i++;
    }
    if (enBloque && hasta >= n) *enBloque = bloque;
}
static bool IdeEsGLSL(const std::string& archivo) {
    return archivo.size() > 5 && archivo.compare(archivo.size() - 5, 5, ".glsl") == 0;
}
void IDE::LexLinea(const std::string& s, bool* enBloque, std::vector<unsigned char>* tipos, size_t hasta) {
    if (gIdeLexGLSL) { LexLineaGLSL(s, enBloque, tipos, hasta); return; }
    const size_t n = s.size();
    if (tipos) tipos->assign(hasta < n ? hasta : n, 0);   // (IdeMarcar no escribe mas alla del largo)
    bool bloque = enBloque ? *enBloque : false;
    size_t i = 0;
    while (i < n && i < hasta) {
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
            if (tipos && IdeEsKeyword(s.c_str() + i, j - i)) IdeMarcar(tipos, i, j, 3);
            i = j; continue;
        }
        i++;
    }
    if (enBloque && hasta >= n) *enBloque = bloque;
}

static int IdeColVisual(const std::string& s, int b);

void IDE::RecalcularBloques() {
    gIdeLexGLSL = IdeEsGLSL(archivo);
    bloqueAlInicio.assign(lineas.size(), 0);
    anchoMaxCols = 0;
    bool b = false;
    for (size_t i = 0; i < lineas.size(); i++) {
        bloqueAlInicio[i] = b ? 1 : 0;
        LexLinea(lineas[i], &b, NULL); // solo avanza el estado
        const int lv = IdeColVisual(lineas[i], (int)lineas[i].size());
        if (lv > anchoMaxCols) anchoMaxCols = lv;
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
    lastLineas = -1; lastMaxAncho = 0; anchoMaxCols = 0;
    selDrag = false; silencioso = false; hayTextoDisco = false;
    tapsSeguidos = 0; tUltimoTap = 0; ultTapX = -1000; ultTapY = -1000; gestoTactil = 0; lupaX = 0; lupaY = 0;
    lineas.push_back(std::string()); // el buffer nunca esta vacio (1 linea vacia)
    BarCrear(); // [0] = icono de tipo/split
    // botones por ROL (no por indice), como la barra del 2D/UV
    Button* b;
    b = new Button(T("Script"), -1);
    b->rol = BRIDE_Script; b->desplegable = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Compile"));   // (solo con un shader .glsl abierto, al lado de su nombre: ver SincronizarBarra)
    b->rol = BRIDE_Compilar;
    b->visible = false;
    BarButtons.push_back(b);
    b = new Button(T("File"), -1);   // menu ARCHIVO: Guardar / Guardar como... / dentro o fuera del w3d / Cargar .lua
    b->rol = BRIDE_Guardar; b->desplegable = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Edit"), -1);   // menu EDITAR: deshacer / rehacer / cortar / copiar / pegar / buscar / reemplazar
    b->rol = BRIDE_Editar; b->desplegable = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Select"), -1); // menu SELECCIONAR
    b->rol = BRIDE_Seleccionar; b->desplegable = true; b->caretMenu = true;
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

// ---------------------------------------------------------------------------------------------------------------
//  UTF-8. Las lineas guardan los BYTES del archivo tal cual (UTF-8), pero el cursor, el borrado, la seleccion y el
//  dibujo trabajan por LETRA: la 'n~' o una vocal con acento son 2 bytes y una sola columna en pantalla. Antes el
//  editor solo aceptaba ASCII ("no anda la letra n~ en el IDE", reporte del dueno). curCol / selCol siguen siendo
//  posiciones de BYTE, siempre al principio de una letra.
// ---------------------------------------------------------------------------------------------------------------
static bool IdeEsCont(unsigned char c) { return (c & 0xC0) == 0x80; }   // byte de continuacion (10xxxxxx)
static int IdeLetrasEntre(const std::string& s, int b0, int b1) {      // letras (columnas) en los bytes [b0, b1)
    int v = 0;
    for (int i = b0; i < b1 && i < (int)s.size(); i++) if (!IdeEsCont((unsigned char)s[(size_t)i])) v++;
    return v;
}
static int IdeAvanzarLetras(const std::string& s, int b, int v) {     // el byte 'v' letras despues de b
    int i = b, n = (int)s.size();
    while (i < n && v > 0) { i++; while (i < n && IdeEsCont((unsigned char)s[(size_t)i])) i++; v--; }
    return i;
}
static int IdeColVisual(const std::string& s, int b) { return IdeLetrasEntre(s, 0, b); }   // byte -> columna en pantalla
static int IdeByteDeCol(const std::string& s, int v) { return IdeAvanzarLetras(s, 0, v); } // columna en pantalla -> byte
// RECORTE HORIZONTAL: los bytes [*b0, *b1) de la linea entre las columnas col0 y col0 + cant (lo que entra en el
// ancho). Un renglon mas corto que col0 da b0 == b1 (no se ve nada)
static void IdeTramoVisible(const std::string& s, int col0, int cant, int* b0, int* b1) {
    *b0 = IdeByteDeCol(s, col0);
    *b1 = IdeAvanzarLetras(s, *b0, cant);
}
static int IdeLetraSig(const std::string& s, int b) {
    int n = (int)s.size(); if (b >= n) return n;
    b++; while (b < n && IdeEsCont((unsigned char)s[(size_t)b])) b++;
    return b;
}
static int IdeLetraAnt(const std::string& s, int b) {
    if (b <= 0) return 0;
    b--; while (b > 0 && IdeEsCont((unsigned char)s[(size_t)b])) b--;
    return b;
}
static int IdeAlInicioDeLetra(const std::string& s, int b) {
    while (b > 0 && b < (int)s.size() && IdeEsCont((unsigned char)s[(size_t)b])) b--;
    return b;
}
// lo que se puede ESCRIBIR: imprimibles y letras no ASCII (UTF-8); fuera los de control
static std::string IdeFiltrarTexto(const std::string& t, bool conSaltos) {
    std::string out;
    for (size_t i = 0; i < t.size(); i++) {
        const unsigned char c = (unsigned char)t[i];
        if (c == '\r') continue;
        if (c == '\n') { if (conSaltos) out += '\n'; continue; }
        if (c == '\t') { out += "    "; continue; }   // la fuente no tiene tabulador
        if (c < 32 || c == 127) continue;
        out += (char)c;
    }
    return out;
}

// BUSCAR: comparacion sin distinguir mayusculas. ASCII y las letras latinas de 2 bytes (UTF-8 C3 80..9E -> +0x20:
// la N~ y las vocales con acento): el texto plegado tiene los MISMOS bytes de largo, asi las posiciones coinciden.
static std::string IdePlegar(const std::string& s) {
    std::string o = s;
    for (size_t i = 0; i < o.size(); i++) {
        const unsigned char c = (unsigned char)o[i];
        if (c >= 'A' && c <= 'Z') o[i] = (char)(c + 32);
        else if (c == 0xC3 && i + 1 < o.size()) {
            const unsigned char d = (unsigned char)o[i + 1];
            if (d >= 0x80 && d <= 0x9E && d != 0x97) o[i + 1] = (char)(d + 0x20);
            i++;
        }
    }
    return o;
}
static bool IdeEsLetraPalabra(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c >= 0x80; }

// las coincidencias de 'aguja' (ya plegada si !mayus) en la linea que EMPIEZAN en los bytes [desde, hasta): Buscar
// recorre la linea entera y el dibujo solo el tramo que se ve
static void IdeBuscarEnLinea(const std::string& s, const std::string& aguja, bool mayus, bool palabra,
                             size_t desde, size_t hasta, std::vector<int>* col) {
    const size_t n = s.size(), la = aguja.size();
    if (la == 0 || desde >= n || desde >= hasta) return;
    size_t z = hasta + la; if (z > n) z = n;   // las que empiezan antes de 'hasta' terminan antes de z
    const std::string pajar = mayus ? s.substr(desde, z - desde) : IdePlegar(s.substr(desde, z - desde));
    size_t p = pajar.find(aguja);
    while (p != std::string::npos && desde + p < hasta) {
        const size_t a = desde + p, e = a + la;
        bool ok = true;
        if (palabra) {
            if (a > 0 && IdeEsLetraPalabra((unsigned char)s[a - 1])) ok = false;
            if (e < n && IdeEsLetraPalabra((unsigned char)s[e])) ok = false;
        }
        if (ok) col->push_back((int)a);
        p = pajar.find(aguja, p + la);
    }
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
        Notificar(std::string(T("IDE: could not read")) + " " + IDENombreScript(ruta), true);
        return false;
    }
    IdePartirEnLineas(crudo, &lineas);
    archivo = ruta;
    sucio = false;
    textoDisco = crudo; hayTextoDisco = true;
    curLin = 0; curCol = 0; haySel = false; selDrag = false;
    PosX = 0; PosY = 0;
    pilaUndo.clear(); pilaRedo.clear(); undoUltimaOp = 0;
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
    pilaUndo.clear(); pilaRedo.clear(); undoUltimaOp = 0;
    sucio = true;
    lastLineas = -1;
    RecalcularBloques();
    g_redraw = true;
}

// REGLA: el IDE NUNCA rompe el archivo. Guardado ATOMICO: se escribe TODO a un
// .w3dtmp al lado y recien si salio completo se renombra sobre el .lua. Si el
// disco falla a mitad de camino, el original queda intacto.
static IDE* gIdePregunta = NULL;   // el IDE que pregunto (la respuesta del popup vuelve a el)
static void IdeGuardarForzado() { if (gIdePregunta) gIdePregunta->Guardar(true); }
static void IdeRefreshForzado() { if (gIdePregunta) gIdePregunta->Refresh(true); }

// UN SHADER GUARDADO (.glsl): los materiales que lo usan se compilan de nuevo (lo mismo que su boton "Compile") y
// el resultado sale en un aviso: el error con su renglon, o que compilo
static void IdeShaderGuardado(const std::string& archivo) {
    if (!IdeEsGLSL(archivo)) return;
    W3dShaderArchivoOlvidar(archivo);   // (el paso "Shader" de las rutinas lo vuelve a compilar al dibujar)
    int n = 0; std::string error;
    for (size_t i = 0; i < Materials.size(); i++) {
        Material* m = Materials[i];
        if (!m || m->shaderArchivo != archivo) continue;
        if (!W3dShaderMaterialCompilar(m) && error.empty()) error = m->shaderError;
        n++;
    }
    if (n == 0) return;
    if (!error.empty()) Notificar(std::string(T("The shader does not compile")) + ": " + error, true);
    else Notificar(IDENombreScript(archivo) + " " + T("compiled"), false);
    extern void RebindMaterialMeshPart(); RebindMaterialMeshPart();   // (la tarjeta del material dice el resultado)
    g_redraw = true;
}

bool IDE::Guardar(bool forzar) {
    if (archivo.empty()) return false;
    // EXTERNO editado por otro programa desde que se abrio: pisarlo sin avisar borra ese trabajo
    if (!forzar && hayTextoDisco && !W3dEsNombreDeEntrada(archivo)) {
        bool ok = false;
        const std::string enDisco = w3dFileSystem::ReadTextFile(archivo, &ok);
        if (ok && enDisco != textoDisco && enDisco != GetTexto()) {
            gIdePregunta = this;
            if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
            confirmarPopup->Abrir(std::string(T("The file was changed outside the editor. Overwrite it?")) + "\n" + IDENombreScript(archivo), IdeGuardarForzado);
            return false;
        }
    }
    // el outliner por recursos busca en los scripts las texturas que nombran: su foto se re-arma
    { extern void W3dRecursosVistaInvalidar(); W3dRecursosVistaInvalidar(); }
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
            Notificar(std::string(T("IDE: could not save inside the project:")) + " " + IDENombreScript(archivo), true);
            return false;
        }
        sucio = false;
        textoDisco = t; hayTextoDisco = true;
        if (!silencioso) Notificar(IDENombreScript(archivo) + " " + T("saved (save the project so it stays in the .w3d)"), false);
        IdeShaderGuardado(archivo);
        RecargarEnVivo(false);
        g_redraw = true;
        return true;
    }
    std::string tmp = archivo + ".w3dtmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) {
        Notificar(std::string(T("IDE: could not write")) + " " + IDENombreScript(archivo), true);
        return false;
    }
    size_t esc = t.empty() ? 0 : fwrite(t.c_str(), 1, t.size(), f);
    bool ok = (fclose(f) == 0) && (esc == t.size());
    if (!ok) {
        remove(tmp.c_str());
        Notificar(std::string(T("IDE: saving failed:")) + " " + IDENombreScript(archivo), true);
        return false;
    }
#if defined(_WIN32) || defined(W3D_SYMBIAN)
    remove(archivo.c_str()); // Windows Y Symbian: rename()/RFs::Rename NO reemplaza un destino que ya existe
                             // (KErrAlreadyExists) -> sin este remove, guardar un .lua que ya existe fallaba SIEMPRE
#endif
    if (rename(tmp.c_str(), archivo.c_str()) != 0) {
        remove(tmp.c_str());
        Notificar(std::string(T("IDE: saving failed:")) + " " + IDENombreScript(archivo), true);
        return false;
    }
    sucio = false;
    textoDisco = t; hayTextoDisco = true;
    W3dDescargarSiWeb(archivo);   // navegador: el .lua externo guardado baja a la PC
    if (!silencioso) Notificar(IDENombreScript(archivo) + " " + T("saved"), false);
    IdeShaderGuardado(archivo);
    RecargarEnVivo(false);
    g_redraw = true;
    return true;
}

bool IDE::GuardarEnProyecto() {
    if (archivo.empty()) { extern void IDEAbrirGuardarComo(IDE*); IDEAbrirGuardarComo(this); return false; }
    if (!W3dEsNombreDeEntrada(archivo)) return Guardar();   // externo: el archivo del disco y listo
    silencioso = true;
    const bool ok = Guardar();
    silencioso = false;
    if (!ok) return false;
    // adentro del proyecto: el .w3d del disco se reescribe con el script (sino el cambio quedaba solo en memoria)
    extern std::string w3dPath;
    extern bool GuardarW3D(const std::string&);
    if (!w3dPath.empty() && GuardarW3D(w3dPath)) {
        Notificar(IDENombreScript(archivo) + " " + T("saved inside the .w3d"), false);
        return true;
    }
    Notificar(IDENombreScript(archivo) + " " + T("saved (save the project so it stays in the .w3d)"), false);
    return true;
}

// REFRESH: TRAER el archivo. Para cuando el script se edita con OTRO editor (el archivo externo, o la entrada del .w3d
// que cambio por otro camino): el buffer pasa a ser lo que hay en el archivo. Antes Refresh GUARDABA el buffer encima
// del archivo y pisaba lo editado afuera (reporte del dueno); guardar es Guardar.
//  - con cambios locales sin guardar: pregunta antes de descartarlos (y lo de antes queda en el deshacer: Ctrl+Z)
//  - juego CORRIENDO: ademas se recargan los objetos que usan el script (RecargarEnVivo)
void IDE::Refresh(bool forzar) {
    if (archivo.empty()) return;
    if (sucio && !forzar) {
        gIdePregunta = this;
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir(std::string(T("The editor has unsaved changes. Discard them and reload the file?")) + "\n" + IDENombreScript(archivo), IdeRefreshForzado);
        return;
    }
    bool ok = false;
    const std::string crudo = w3dFileSystem::ReadTextFile(archivo, &ok);
    if (!ok) { Notificar(std::string(T("IDE: could not read")) + " " + IDENombreScript(archivo), true); return; }
    std::vector<std::string> nuevas;
    IdePartirEnLineas(crudo, &nuevas);
    if (nuevas != lineas) {
        Mutar(0);                       // lo que habia (incluso lo descartado) se recupera con Ctrl+Z
        lineas = nuevas;
        if (curLin >= (int)lineas.size()) curLin = (int)lineas.size() - 1;
        if (curCol > (int)lineas[curLin].size()) curCol = (int)lineas[curLin].size();
        curCol = IdeAlInicioDeLetra(lineas[curLin], curCol);
        haySel = false; undoUltimaOp = 0;
        lastLineas = -1;
        RecalcularBloques();
        AsegurarCursorVisible();
        Notificar(IDENombreScript(archivo) + ": " + T("reloaded from the file"), false);
    } else {
        Notificar(IDENombreScript(archivo) + ": " + T("no new changes in the file"), false);
    }
    sucio = false;
    textoDisco = crudo; hayTextoDisco = true;
    RecargarEnVivo(true);
    g_redraw = true;
}

// Con el juego CORRIENDO (SimActiva) se recargan SOLO los objetos cuyos scripts usan este archivo
// (SimScriptsCambiados): sus lua_State se recrean (variables locales de cero + inicio() de nuevo) y el RESTO del juego
// (posiciones, otros scripts, escena activa) se PRESERVA tal cual. Con el juego parado no hace nada (el proximo Play
// ya lee lo nuevo).
void IDE::RecargarEnVivo(bool avisarSiNada) {
    // (SimJuego.cpp -SimActiva/SimScriptsCambiados- compila tambien en el .mmp del N95)
    extern bool SimActiva();
    extern void SimScriptsCambiados(Object*);
    if (archivo.empty() || !SimActiva()) return;
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
    if (!usan.empty() || avisarSiNada)
        Notificar(std::string(T("Refresh:")) + " " + IdeNum((int)usan.size()) + " " + T("object(s) reloaded live"), false);
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  BUSCAR / REEMPLAZAR (lo usa el popup PopUp/BuscarPopup)
// ---------------------------------------------------------------------------
void IDE::Coincidencias(const std::string& t, bool mayus, bool palabra, std::vector<int>* lin, std::vector<int>* col) const {
    lin->clear(); col->clear();
    if (t.empty()) return;
    const std::string aguja = mayus ? t : IdePlegar(t);
    std::vector<int> cols;
    for (size_t l = 0; l < lineas.size(); l++) {
        cols.clear();
        IdeBuscarEnLinea(lineas[l], aguja, mayus, palabra, 0, lineas[l].size(), &cols);
        for (size_t k = 0; k < cols.size(); k++) { lin->push_back((int)l); col->push_back(cols[k]); }
    }
}
bool IDE::IrACoincidencia(const std::string& t, bool mayus, bool palabra, int dir) {
    std::vector<int> L, C;
    Coincidencias(t, mayus, palabra, &L, &C);
    if (L.empty()) return false;
    // desde donde: el FINAL de la seleccion para adelante, el PRINCIPIO para atras
    int l0 = curLin, c0 = curCol;
    if (haySel) { int a, b, c, d; NormalizarSel(&a, &b, &c, &d); if (dir > 0) { l0 = c; c0 = d; } else { l0 = a; c0 = b; } }
    int elegido = -1;
    if (dir > 0) {
        for (size_t i = 0; i < L.size(); i++) if (L[i] > l0 || (L[i] == l0 && C[i] >= c0)) { elegido = (int)i; break; }
        if (elegido < 0) elegido = 0;                       // da la vuelta al principio
    } else {
        for (int i = (int)L.size() - 1; i >= 0; i--) if (L[(size_t)i] < l0 || (L[(size_t)i] == l0 && C[(size_t)i] < c0)) { elegido = i; break; }
        if (elegido < 0) elegido = (int)L.size() - 1;       // da la vuelta al final
    }
    selLin = L[(size_t)elegido]; selCol = C[(size_t)elegido];
    curLin = selLin; curCol = selCol + (int)t.size();
    haySel = true;
    undoUltimaOp = 0;
    AsegurarCursorVisible();
    g_redraw = true;
    return true;
}
bool IDE::SeleccionEsCoincidencia(const std::string& t, bool mayus) const {
    if (!haySel || t.empty()) return false;
    const std::string sel = TextoSeleccionado();
    return mayus ? (sel == t) : (IdePlegar(sel) == IdePlegar(t));
}
int IDE::ReemplazarTodas(const std::string& t, const std::string& por, bool mayus, bool palabra) {
    std::vector<int> L, C;
    Coincidencias(t, mayus, palabra, &L, &C);
    if (L.empty()) return 0;
    Mutar(0);   // todas juntas: UN paso de deshacer
    for (int i = (int)L.size() - 1; i >= 0; i--)   // de atras para adelante: las posiciones de antes no se corren
        lineas[(size_t)L[(size_t)i]].replace((size_t)C[(size_t)i], t.size(), por);
    haySel = false;
    if (curCol > (int)lineas[curLin].size()) curCol = (int)lineas[curLin].size();
    curCol = IdeAlInicioDeLetra(lineas[curLin], curCol);
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
    return (int)L.size();
}

// ---------------------------------------------------------------------------
//  undo: snapshots del buffer entero (los .lua de un juego son chicos), en una
//  pila AGRUPADA por operacion: una corrida de tipeo = UN undo; backspace
//  sostenido = UN undo; pegar/cortar/enter = un undo cada uno. Lo deshecho va a
//  la pila de REHACER, que se vacia con la proxima edicion (el Ctrl+Z global
//  del editor no pasa por aca).
// ---------------------------------------------------------------------------
void IDE::Mutar(int op) {
    if (op == 0 || op != undoUltimaOp || pilaUndo.empty()) {
        EstadoTexto e;
        e.lineas = lineas; e.curLin = curLin; e.curCol = curCol;
        pilaUndo.push_back(e);
        if (pilaUndo.size() > 200) pilaUndo.erase(pilaUndo.begin());
    }
    pilaRedo.clear();   // una edicion nueva: lo deshecho ya no se puede rehacer
    undoUltimaOp = op;
    sucio = true;
}

void IDE::DeshacerTexto() {
    if (pilaUndo.empty()) return;
    { EstadoTexto ahora; ahora.lineas = lineas; ahora.curLin = curLin; ahora.curCol = curCol; pilaRedo.push_back(ahora); }
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

void IDE::RehacerTexto() {
    if (pilaRedo.empty()) return;
    { EstadoTexto ahora; ahora.lineas = lineas; ahora.curLin = curLin; ahora.curCol = curCol; pilaUndo.push_back(ahora); }
    EstadoTexto e = pilaRedo.back();
    pilaRedo.pop_back();
    lineas = e.lineas;
    curLin = e.curLin; curCol = e.curCol;
    haySel = false;
    undoUltimaOp = 0;
    sucio = true;
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
    col = IdeAlInicioDeLetra(lineas[lin], col);   // nunca a la mitad de una letra de 2+ bytes
    curLin = lin; curCol = col;
    if (!shift) haySel = false;
    if (haySel && selLin == curLin && selCol == curCol) haySel = false;
    undoUltimaOp = 0; // moverse corta el grupo de tipeo del undo
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::MoverCursor(int dLin, int dCol, bool shift) {
    int lin = curLin, col = curCol;
    if (dCol < 0) {
        if (col > 0) col = IdeLetraAnt(lineas[lin], col);
        else if (lin > 0) { lin--; col = (int)lineas[lin].size(); }   // cruzar al final de la linea anterior
    } else if (dCol > 0) {
        if (col < (int)lineas[lin].size()) col = IdeLetraSig(lineas[lin], col);
        else if (lin + 1 < (int)lineas.size()) { lin++; col = 0; }     // cruzar al inicio de la siguiente
    }
    if (dLin) {   // arriba/abajo: misma COLUMNA EN PANTALLA (no el mismo byte) en la linea nueva
        const int vis = IdeColVisual(lineas[lin], col);
        lin += dLin;
        if (lin < 0) lin = 0;
        if (lin >= (int)lineas.size()) lin = (int)lineas.size() - 1;
        col = IdeByteDeCol(lineas[lin], vis);
    }
    CursorA(lin, col, shift);
}

void IDE::Home(bool shift) { CursorA(curLin, 0, shift); }
void IDE::End(bool shift)  { CursorA(curLin, (int)lineas[curLin].size(), shift); }

void IDE::PaginaArribaAbajo(int dir, bool shift) {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int visibles = (height - BarTopOffset()) / lh;
    if (visibles < 1) visibles = 1;
    int lin = curLin + dir * visibles;
    if (lin < 0) lin = 0;
    if (lin >= (int)lineas.size()) lin = (int)lineas.size() - 1;
    CursorA(lin, IdeByteDeCol(lineas[lin], IdeColVisual(lineas[curLin], curCol)), shift);
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
void IDE::InsertarChar(char c) { InsertarTipeado(std::string(1, c)); }

// lo tipeado (una letra, que puede ser de varios bytes UTF-8): agrupa el undo como el tipeo
void IDE::InsertarTipeado(const std::string& t) {
    if (t.empty()) return;
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
    lineas[curLin].insert((size_t)curCol, t);
    curCol += (int)t.size();
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

// espacios al principio de la linea (la sangria)
static int IdeSangria(const std::string& s) { int n = 0; while (n < (int)s.size() && s[(size_t)n] == ' ') n++; return n; }

// la sangria que dibujan las guias en la linea l: la suya, o si esta en blanco, la menor entre la linea con texto de
// arriba y la de abajo (asi las guias atraviesan los renglones vacios de adentro de un bloque)
static int IdeSangriaGuia(const std::vector<std::string>& lineas, int l) {
    const std::string& ln = lineas[(size_t)l];
    const int ind = IdeSangria(ln);
    if (ind < (int)ln.size()) return ind;
    int arriba = 0, abajo = 0;
    for (int k = l - 1, m = 0; k >= 0 && m < 300; k--, m++) { const std::string& o = lineas[(size_t)k]; const int q = IdeSangria(o); if (q < (int)o.size()) { arriba = q; break; } }
    for (int k = l + 1, m = 0; k < (int)lineas.size() && m < 300; k++, m++) { const std::string& o = lineas[(size_t)k]; const int q = IdeSangria(o); if (q < (int)o.size()) { abajo = q; break; } }
    return (arriba < abajo) ? arriba : abajo;
}

// TAB como en los editores de codigo:
//  - sin seleccion (o dentro de una linea): espacios hasta el proximo tope de 4 (la fuente no tiene tabulador)
//  - con varias lineas seleccionadas: les suma un nivel de sangria a todas
//  - Shift+Tab: le saca un nivel a la linea del cursor (o a todas las seleccionadas)
void IDE::TabKey(bool shift) {
    const bool multi = haySel && selLin != curLin;
    if (!shift && !multi) {
        const int vis = haySel ? 0 : IdeColVisual(lineas[curLin], curCol);
        InsertarTexto(std::string((size_t)(kIdeTab - vis % kIdeTab), ' '));
        return;
    }
    int l0 = curLin, l1 = curLin;
    if (haySel) {
        int a0, c0, a1, c1; NormalizarSel(&a0, &c0, &a1, &c1);
        l0 = a0; l1 = a1;
        if (l1 > l0 && c1 == 0) l1--;   // la seleccion termina al principio de una linea: esa no entra
    }
    Mutar(0);
    for (int l = l0; l <= l1; l++) {
        std::string& ln = lineas[(size_t)l];
        int cambio = 0;
        if (!shift) {
            if (ln.empty()) continue;   // las lineas vacias quedan vacias
            ln.insert((std::string::size_type)0, (std::string::size_type)kIdeTab, ' ');   // posicion explicita: con 0 a secas STLport ve tambien la sobrecarga por iterador
            cambio = kIdeTab;
        } else {
            const int q = IdeSangria(ln);
            int n = q % kIdeTab; if (n == 0) n = (q < kIdeTab) ? q : kIdeTab;   // hasta el tope anterior
            if (n <= 0) continue;
            ln.erase(0, (size_t)n);
            cambio = -n;
        }
        // el cursor y la punta de la seleccion acompanian al texto (una punta en la columna 0 se queda ahi al sumar)
        if (curLin == l && (cambio < 0 || curCol > 0)) { curCol += cambio; if (curCol < 0) curCol = 0; }
        if (haySel && selLin == l && (cambio < 0 || selCol > 0)) { selCol += cambio; if (selCol < 0) selCol = 0; }
    }
    lastLineas = -1;
    RecalcularBloques();
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::Backspace() {
    if (haySel) { BorrarSeleccion(); return; }
    if (curCol == 0 && curLin == 0) return;
    Mutar(2); // los backspace seguidos se agrupan
    if (curCol > 0 && IdeSangria(lineas[curLin]) >= curCol) {   // dentro de la sangria: un nivel entero
        int n = curCol % kIdeTab; if (n == 0) n = kIdeTab; if (n > curCol) n = curCol;
        lineas[curLin].erase((size_t)(curCol - n), (size_t)n);
        curCol -= n;
    } else if (curCol > 0) {   // la letra entera (1 a 4 bytes)
        const int a = IdeLetraAnt(lineas[curLin], curCol);
        lineas[curLin].erase((size_t)a, (size_t)(curCol - a));
        curCol = a;
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
    if (curCol < (int)lineas[curLin].size()) {   // la letra entera
        lineas[curLin].erase((size_t)curCol, (size_t)(IdeLetraSig(lineas[curLin], curCol) - curCol));
    } else { // unir con la linea siguiente
        lineas[curLin] += lineas[curLin + 1];
        lineas.erase(lineas.begin() + curLin + 1);
        lastLineas = -1;
    }
    RecalcularBloques();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  clipboard: el del SISTEMA via W3dClipboard (Core) -> compartido con navegador/notas
//  en TODAS las plataformas (SDL en PC/web, CClipboard en Symbian). gIDEClipInterno es
//  respaldo local (headless / si el sistema no responde).
// ---------------------------------------------------------------------------
void IDE::Copiar() {
    if (!haySel) return;
    gIDEClipInterno = TextoSeleccionado();
    w3dEngine::W3dClipboardSet(gIDEClipInterno);   // portapapeles del sistema (cualquier app lo puede pegar)
}

void IDE::Cortar() {
    if (!haySel) return;
    Copiar();
    BorrarSeleccion();
}

void IDE::Pegar() {
    std::string t = w3dEngine::W3dClipboardGet(); // el clipboard del SISTEMA (de cualquier programa: navegador, notas)
    if (t.empty()) t = gIDEClipInterno; // headless / si el sistema no dio nada
    if (t.empty()) return;
    // imprimibles, letras no ASCII (n~, acentos: UTF-8) y saltos; fuera los de control
    const std::string filtrado = IdeFiltrarTexto(t, true);
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
            case W3dK_Z: if (shift) RehacerTexto(); else DeshacerTexto(); return true;   // undo/redo DEL TEXTO, no el global
            case W3dK_Y: RehacerTexto(); return true;
            case W3dK_F: { extern void IDEAbrirBuscar(IDE*, bool); IDEAbrirBuscar(this, false); return true; }   // Buscar
            case W3dK_H: { extern void IDEAbrirBuscar(IDE*, bool); IDEAbrirBuscar(this, true);  return true; }   // Reemplazar
            case W3dK_S: GuardarEnProyecto(); return true;         // guarda el .lua (y el .w3d si vive adentro)
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
        case W3dK_TAB:      TabKey(shift); return true;   // Shift+Tab = sacar un nivel de sangria
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
    InsertarTipeado(IdeFiltrarTexto(utf8, false));   // tambien la n~ y los acentos (llegan en UTF-8)
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
// pantalla -> renglon y byte
void IDE::PuntoATexto(int mx, int my, int* lin, int* col) const {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    const int lx = mx - x, ly = my - y;
    const int textX0 = GutterW() + marginGS + PosX;
    const int textY0 = BarTopOffset() + marginGS + PosY;
    int l = (ly < textY0) ? 0 : (ly - textY0) / lh;
    if (l >= (int)lineas.size()) l = (int)lineas.size() - 1;
    if (l < 0) l = 0;
    int c = (lx - textX0 + LetterWidthGS / 2) / (LetterWidthGS > 0 ? LetterWidthGS : 1);
    *lin = l;
    *col = (c < 0) ? 0 : IdeByteDeCol(lineas[(size_t)l], c);
}

void IDE::TextoAPunto(int lin, int col, int* mx, int* my) const {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    const int LW = (LetterWidthGS > 0) ? LetterWidthGS : 6;
    if (lin < 0) lin = 0; if (lin >= (int)lineas.size()) lin = (int)lineas.size() - 1;
    *mx = x + GutterW() + marginGS + PosX + IdeColVisual(lineas[(size_t)lin], col) * LW;   // el hueco ANTES de la letra
    *my = y + BarTopOffset() + marginGS + PosY + lin * lh + lh / 2;
}

void IDE::SeleccionarPalabra() {
    const std::string& s = lineas[(size_t)curLin];
    int a = curCol, b = curCol;
    while (a > 0 && IdeEsLetraPalabra((unsigned char)s[(size_t)(a - 1)])) a--;
    while (b < (int)s.size() && IdeEsLetraPalabra((unsigned char)s[(size_t)b])) b++;
    if (a == b) {   // no es una palabra (espacio, simbolo): la letra de al lado
        if (b < (int)s.size()) b = IdeLetraSig(s, b); else if (a > 0) a = IdeLetraAnt(s, a);
    }
    selLin = curLin; selCol = a; curCol = b;
    haySel = (a != b);
    undoUltimaOp = 0;
    g_redraw = true;
}
void IDE::SeleccionarLinea() {
    selLin = curLin; selCol = 0;
    if (curLin + 1 < (int)lineas.size()) { curLin++; curCol = 0; }   // con el salto de linea (como los editores de codigo)
    else curCol = (int)lineas[(size_t)curLin].size();
    haySel = !(selLin == curLin && selCol == curCol);
    undoUltimaOp = 0;
    AsegurarCursorVisible();
    g_redraw = true;
}

void IDE::ClickContenido(int mx, int my, bool shift) {
    int lin = 0, col = 0;
    PuntoATexto(mx, my, &lin, &col);
    // DOBLE / TRIPLE toque (o click): rapido y en el mismo lugar
    const unsigned int ahora = w3dGetTicks();
    const int cerca = 12 * (GlobalScale > 0 ? GlobalScale : 1);
    if (!shift && ahora - tUltimoTap < 450 && (mx - ultTapX) * (mx - ultTapX) + (my - ultTapY) * (my - ultTapY) < cerca * cerca)
        tapsSeguidos++;
    else
        tapsSeguidos = 1;
    tUltimoTap = ahora; ultTapX = mx; ultTapY = my;
    CursorA(lin, col, shift);
    if (tapsSeguidos == 2) SeleccionarPalabra();
    else if (tapsSeguidos >= 3) { SeleccionarLinea(); tapsSeguidos = 0; }   // (un 4to toque vuelve a empezar)
    selDrag = true;
    ViewPortClickDown = true; // el drag de seleccion congela el foco por hover
    // TACTIL (Android): el teclado del celular NO aparece solo. En PC SDL deja la entrada de texto prendida desde
    // el arranque; en Android hay que pedirlo. Un TAP sobre el codigo lo levanta (como los campos de texto del panel).
#ifndef W3D_SYMBIAN
    { extern bool g_uiTapEnCurso; if (g_uiTapEnCurso) W3dTecladoAbrir(); }
#endif
    g_redraw = true;
}

// PULSACION LARGA: la lupa. El cursor va al dedo y lo sigue mientras se arrastra; arriba del dedo se ve el renglon
// AGRANDADO con el cursor, asi se lo deja justo donde se quiere aunque el dedo tape el texto.
bool IDE::PulsacionLarga(int mx, int my) {
    int lin = 0, col = 0;
    PuntoATexto(mx, my, &lin, &col);
    CursorA(lin, col, false);
    gestoTactil = 1; lupaX = mx; lupaY = my;
    g_redraw = true;
    return true;
}
// las agarraderas cuelgan debajo de las dos puntas de la seleccion
static int IdeRadioManija() { return 5 * (GlobalScale > 0 ? GlobalScale : 1); }
int IDE::ManijaEn(int mx, int my) const {
    if (!haySel || !ToolbarUsaTactil()) return 0;
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    const int LW = (LetterWidthGS > 0) ? LetterWidthGS : 6;
    const int textX0 = x + GutterW() + marginGS + PosX, py0 = y + BarTopOffset() + marginGS + PosY;
    int l0, c0, l1, c1; NormalizarSel(&l0, &c0, &l1, &c1);
    const int r = IdeRadioManija(), alcance = r * 3;
    const int ax = textX0 + IdeColVisual(lineas[(size_t)l0], c0) * LW, ay = py0 + l0 * lh + lh + r;
    const int bx = textX0 + IdeColVisual(lineas[(size_t)l1], c1) * LW, by = py0 + l1 * lh + lh + r;
    const int da = (mx - ax) * (mx - ax) + (my - ay) * (my - ay), db = (mx - bx) * (mx - bx) + (my - by) * (my - by);
    // solo DEBAJO del renglon (donde cuelga la agarradera): tocar el texto nunca la agarra
    const bool okA = (my >= ay - r) && da <= alcance * alcance, okB = (my >= by - r) && db <= alcance * alcance;
    if (okB && (!okA || db <= da)) return 2;
    if (okA) return 1;
    return 0;
}
bool IDE::ManijaAgarrar(int mx, int my) {
    const int m = ManijaEn(mx, my);
    if (!m) return false;
    // la punta agarrada pasa a ser el CURSOR y la otra el ancla: arrastrar mueve solo esa punta
    int l0, c0, l1, c1; NormalizarSel(&l0, &c0, &l1, &c1);
    if (m == 1) { selLin = l1; selCol = c1; curLin = l0; curCol = c0; }
    else        { selLin = l0; selCol = c0; curLin = l1; curCol = c1; }
    gestoTactil = (m == 1) ? 2 : 3; lupaX = mx; lupaY = my;
    g_redraw = true;
    return true;
}
void IDE::GestoTactilMover(int mx, int my) {
    if (!gestoTactil) return;
    lupaX = mx; lupaY = my;
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int lin = 0, col = 0;
    if (gestoTactil == 1) {   // lupa: el cursor donde esta el dedo
        PuntoATexto(mx, my, &lin, &col);
        CursorA(lin, col, false);
        return;
    }
    // agarradera: el dedo esta DEBAJO del renglon (la agarradera cuelga), se mide un renglon mas arriba
    PuntoATexto(mx, my - lh - IdeRadioManija(), &lin, &col);
    curLin = lin; curCol = col;
    haySel = !(selLin == curLin && selCol == curCol);
    undoUltimaOp = 0;
    AsegurarCursorVisible();
    g_redraw = true;
}
void IDE::GestoTactilFin() { gestoTactil = 0; g_redraw = true; }

// circulo relleno (las agarraderas)
static void IdeCirculo(float cx, float cy, float r, const float* rgba) {
    float v[16 * 6];
    for (int i = 0; i < 16; i++) {
        const float a0 = (float)i * 6.2831853f / 16.0f, a1 = (float)(i + 1) * 6.2831853f / 16.0f;
        v[i * 6 + 0] = cx; v[i * 6 + 1] = cy;
        v[i * 6 + 2] = cx + cosf(a0) * r; v[i * 6 + 3] = cy + sinf(a0) * r;
        v[i * 6 + 4] = cx + cosf(a1) * r; v[i * 6 + 5] = cy + sinf(a1) * r;
    }
    gfx::Color4fv(rgba);
    gfx::VertexPointer2f(0, v);
    gfx::DrawTrianglesArray(16 * 3);
}

static bool gIdeLapiz = false;   // el lapiz del telefono apretado (barra Copiar | Pegar)
void IDELapizSostenido(bool apretado) { if (gIdeLapiz != apretado) { gIdeLapiz = apretado; g_redraw = true; } }

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
        { const int lc = (lin < 0) ? 0 : (lin >= (int)lineas.size() ? (int)lineas.size() - 1 : lin);
          col = (col < 0) ? 0 : IdeByteDeCol(lineas[lc], col); }
        CursorA(lin, col, true);
        g_redraw = true;
    } else if (middleMouseDown) {
        // boton del medio = arrastrar el contenido (scroll 1:1, como la Console)
        ViewPortClickDown = true;
        ScrollByTouch(mx - lastMouseX, my - lastMouseY);
        g_redraw = true;
    }
}

// IMPRESCINDIBLE (regla de la casa): soltar libera ViewPortClickDown. Sin esto
// el foco por hover queda CLAVADO en este viewport (bug conocido de Console).
void IDE::mouse_button_up(int boton) {
    W3D_SOLO_POR_CONTROLES();
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
    // achicarse (el teclado del celular se abrio y el IDE quedo en la parte visible): la linea del cursor sigue a la vista
    AsegurarCursorVisible();
}

void IDE::AsegurarCursorVisible() {
    const int lh = (RenglonHeightGS > 0) ? RenglonHeightGS : 12;
    int visH = height - BarTopOffset() - marginGS * 2;
    int visW = width - GutterW() - marginGS * 2 - GlobalScale * 9; // colchon de la barra vertical
    int cy = curLin * lh;
    if (cy + PosY < 0) PosY = -cy;
    if (visH > lh && cy + PosY > visH - lh) PosY = visH - lh - cy;
    if (PosY > 0) PosY = 0;
    int cx = IdeColVisual(lineas[curLin], curCol) * LetterWidthGS;
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
    Button* c = BarRolBtn(BarButtons, BRIDE_Compilar);
    const bool shader = IdeEsGLSL(archivo);
    if (c && c->visible != shader) { c->visible = shader; g_redraw = true; }
}
void IDE::CompilarShader() {
    if (!IdeEsGLSL(archivo)) return;
    bool usado = false;
    for (size_t i = 0; i < Materials.size() && !usado; i++) usado = Materials[i] && Materials[i]->shaderArchivo == archivo;
    if (usado) { if (sucio) Guardar(); else IdeShaderGuardado(archivo); return; }   // (guardar ya compila)
    // nadie lo usa: se prueba lo que hay en el editor
    std::string vs, fs; W3dShaderPartir(GetTexto(), vs, fs);
    if (!w3dEngine::ShadersEscritos()) {   // (sin shaders: solo el @vertice, que es lo que corre aca, en la CPU)
        std::string e;
        W3dVertCPU* p = strstr(vs.c_str(), "Vertice") ? W3dVertCPUCompilar(vs, e) : NULL;
        if (p || !strstr(vs.c_str(), "Vertice")) { W3dVertCPUBorrar(p); Notificar(IDENombreScript(archivo) + " " + T("compiled"), false); }
        else Notificar(std::string(T("The shader does not compile")) + ": @vertice (CPU): " + e, true);
        return;
    }
    char log[2048]; log[0] = 0;
    const int id = w3dEngine::ShaderEscritoCompilar(vs.c_str(), fs.c_str(), log, (int)sizeof(log));
    if (id > 0) { w3dEngine::ShaderEscritoBorrar(id); Notificar(IDENombreScript(archivo) + " " + T("compiled"), false); }
    else {
        std::string e = log[0] ? log : T("This graphics backend has no shaders: the material is drawn as usual (GLES 2.0 and Vulkan have them)");
        for (size_t k = 0; k < e.size(); k++) if (e[k] == 10 || e[k] == 13) e[k] = 32;   // (en un renglon)
        Notificar(std::string(T("The shader does not compile")) + ": " + e, true);
    }
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

// los bytes [b0, b1) de una linea, por tramos del mismo color del lexer ('tipos' cubre hasta b1); col0 = la columna
// de b0. La x de cada tramo se va sumando: medirla desde el principio de la linea en cada tramo era cuadratico (un
// renglon de 786000 letras de una tabla generada colgaba la PC entera)
static void IdeDibujarTramo(const std::string& s, const std::vector<unsigned char>& tipos, int b0, int b1, int col0,
                            int px, int py, int LW) {
    int i = b0, col = col0;
    while (i < b1) {
        int j = i;
        while (j < b1 && tipos[(size_t)j] == tipos[(size_t)i]) j++;
        IdeTexto(px + col * LW, py, s.substr((size_t)i, (size_t)(j - i)), IdeColorDeTipo(tipos[(size_t)i]));
        col += IdeLetrasEntre(s, i, j);
        i = j;
    }
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

    // recalcular el scroll SOLO si cambio el contenido (patron Console). El renglon mas largo ya lo midio
    // RecalcularBloques: recorrer el texto entero en cada cuadro pesa con scripts de varios MB
    int maxAncho = gutterW + marginGS + anchoMaxCols * LW + marginGS + GlobalScale * 9;
    if (n != lastLineas || maxAncho != lastMaxAncho) {
        lastLineas = n; lastMaxAncho = maxAncho;
        RecalcularScroll();
    }

    int hVis = height - top; if (hVis < 0) hVis = 0;
    const int textX0 = gutterW + marginGS + PosX;
    const int py0 = top + marginGS + PosY;
    // RECORTE: los renglones que entran en el alto y, en cada uno, las columnas que entran en el ancho (las tablas
    // que genera un conversor pueden tener renglones de cientos de miles de letras: solo se mide, colorea y dibuja
    // el tramo visible)
    int primera = (0 - (py0)) / lh - 1; if (primera < 0) primera = 0;
    int ultima = primera + hVis / lh + 2; if (ultima > n) ultima = n;
    int colIni = (-PosX - marginGS) / LW - 1; if (colIni < 0) colIni = 0;
    const int colCant = (width - gutterW) / LW + 3;

    // ---- 1) quads: seleccion + caret + separador del gutter (sin textura) ----
    gfx::Scissor(x + gutterW, glY, (width > gutterW) ? width - gutterW : 0, hVis);
    if (haySel) {
        int l0, c0, l1, c1;
        NormalizarSel(&l0, &c0, &l1, &c1);
        for (int l = (l0 > primera ? l0 : primera); l <= l1 && l < ultima; l++) {
            int a = (l == l0) ? c0 : 0;
            int b = (l == l1) ? c1 : (int)lineas[l].size();
            int py = py0 + l * lh;
            float x0 = (float)(textX0 + IdeColVisual(lineas[l], a) * LW);
            float x1 = (float)(textX0 + IdeColVisual(lineas[l], b) * LW);
            if (b <= (int)lineas[l].size() && a == b) x1 = x0 + LW * 0.4f; // linea vacia: mostrarla igual
            IdeQuad(x0, (float)py, x1, (float)(py + lh), kIdeSeleccion);
        }
    }
    { // BUSCAR: las coincidencias a la vista (en el tramo visible de cada renglon), resaltadas (la actual ya se ve
      // como la seleccion)
        const IdeBusqueda* bq = IDEBusquedaActiva();
        if (bq && bq->ide == this && !bq->texto.empty()) {
            const std::string aguja = bq->mayus ? bq->texto : IdePlegar(bq->texto);
            const int la = (int)aguja.size();
            std::vector<int> C;
            for (int l = primera; l < ultima; l++) {
                const std::string& s = lineas[(size_t)l];
                int b0, b1;
                IdeTramoVisible(s, colIni, colCant, &b0, &b1);
                if (b0 >= b1) continue;
                C.clear();
                IdeBuscarEnLinea(s, aguja, bq->mayus, bq->palabra, (size_t)(b0 - la + 1 > 0 ? b0 - la + 1 : 0), (size_t)b1, &C);
                for (size_t i = 0; i < C.size(); i++) {
                    const int c = (C[i] >= b0) ? colIni + IdeLetrasEntre(s, b0, C[i]) : colIni - IdeLetrasEntre(s, C[i], b0);
                    const float x0 = (float)(textX0 + c * LW);
                    const float x1 = x0 + (float)(IdeLetrasEntre(s, C[i], C[i] + la) * LW);
                    IdeQuad(x0, (float)(py0 + l * lh), x1, (float)(py0 + l * lh + lh), kIdeCoincide);
                }
            }
        }
    }
    { // GUIAS DE SANGRIA: una linea vertical por cada nivel (cada 4 espacios), como los editores de codigo. En las
      // lineas en blanco de adentro de un bloque siguen de largo (toman la sangria menor entre la de arriba y la de abajo)
        const float ancho = (GlobalScale > 1) ? (float)GlobalScale * 0.5f : 1.0f;
        // la guia ACTIVA: la del bloque que contiene al cursor (su nivel, en las lineas de alrededor que estan adentro)
        int colActiva = -1, actDesde = 0, actHasta = -1;
        {
            const int indCur = IdeSangriaGuia(lineas, curLin);
            if (indCur > 0) {
                colActiva = ((indCur - 1) / kIdeTab) * kIdeTab;
                actDesde = curLin; actHasta = curLin;
                while (actDesde > 0 && curLin - actDesde < 2000 && IdeSangriaGuia(lineas, actDesde - 1) > colActiva) actDesde--;
                while (actHasta + 1 < n && actHasta - curLin < 2000 && IdeSangriaGuia(lineas, actHasta + 1) > colActiva) actHasta++;
            }
        }
        for (int l = primera; l < ultima; l++) {
            int ind = IdeSangriaGuia(lineas, l);
            if (ind > colIni + colCant) ind = colIni + colCant;   // (solo las que entran en el ancho)
            const int py = py0 + l * lh;
            for (int c = (colIni / kIdeTab) * kIdeTab; c < ind; c += kIdeTab) {
                const float gx = (float)(textX0 + c * LW);
                const bool activa = (c == colActiva && l >= actDesde && l <= actHasta);
                IdeQuad(gx, (float)py, gx + ancho, (float)(py + lh), activa ? kIdeGuiaActiva : kIdeGuia);
            }
        }
    }
    { // caret (sin parpadeo: el render es event-driven). Verde si el foco esta aca.
        int py = py0 + curLin * lh;
        float cx = (float)(textX0 + IdeColVisual(lineas[curLin], curCol) * LW);
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
        // la linea del CURSOR se resalta en BLANCO (el resto en gris); asi se ve en que renglon estamos
        IdeTexto(nx, py0 + l * lh, num, ListaColores[static_cast<int>(l == curLin ? ColorID::blanco : ColorID::grisUI)]);
    }

    // el codigo, por tramos de color del lexer
    gfx::Scissor(x + gutterW, glY, (width > gutterW) ? width - gutterW : 0, hVis);
    if (archivo.empty() && n == 1 && lineas[0].empty()) {
        IdeTexto(textX0, py0, T("(no .lua scripts or .glsl shaders in the project)"),
                 ListaColores[static_cast<int>(ColorID::grisUI)]);
    }
    std::vector<unsigned char> tipos;
    gIdeLexGLSL = IdeEsGLSL(archivo);   // (un shader se pinta como GLSL)
    for (int l = primera; l < ultima; l++) {
        const std::string& s = lineas[l];
        if (s.empty()) continue;
        int b0, b1;
        IdeTramoVisible(s, colIni, colCant, &b0, &b1);
        if (b0 >= b1) continue;
        bool blq = (l < (int)bloqueAlInicio.size()) ? (bloqueAlInicio[l] != 0) : false;
        LexLinea(s, &blq, &tipos, (size_t)b1);   // el color depende de lo anterior del renglon: hasta lo visible
        IdeDibujarTramo(s, tipos, b0, b1, colIni, textX0, py0 + l * lh, LW);
    }

    const float* acento = ListaColores[static_cast<int>(ColorID::accent)];
    // AGARRADERAS de la seleccion (tactil): un palito + un circulo colgando de cada punta
    if (haySel && ToolbarUsaTactil()) {
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        int l0, c0, l1, c1; NormalizarSel(&l0, &c0, &l1, &c1);
        const float r = (float)IdeRadioManija(), g = (float)((GlobalScale > 0) ? GlobalScale : 1);
        for (int k = 0; k < 2; k++) {
            const int l = k ? l1 : l0, c = k ? c1 : c0;
            const float hx = (float)(textX0 + IdeColVisual(lineas[(size_t)l], c) * LW);
            const float hy = (float)(py0 + l * lh + lh);
            IdeQuad(hx - g * 0.5f, hy - (float)lh, hx + g * 0.5f, hy + r, acento);
            IdeCirculo(hx, hy + r, r, acento);
        }
        gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Texture2D);
    }
    // LUPA (pulsacion larga / arrastrando una agarradera): el renglon del cursor agrandado, arriba del dedo
    if (gestoTactil) {
        const int zoom = 2;
        const int bw = 150 * ((GlobalScale > 0) ? GlobalScale : 1) < width - gapGS * 2 ? 150 * ((GlobalScale > 0) ? GlobalScale : 1) : width - gapGS * 2;
        const int bh = lh * zoom + borderGS * 2;
        int bx = lupaX - x - bw / 2;
        if (bx < gapGS) bx = gapGS; if (bx + bw > width - gapGS) bx = width - gapGS - bw;
        int by = lupaY - y - bh - 24 * ((GlobalScale > 0) ? GlobalScale : 1);   // arriba del dedo
        if (by < top) by = lupaY - y + 24 * ((GlobalScale > 0) ? GlobalScale : 1);   // sin lugar arriba: abajo
        gfx::Scissor(x + bx, W3dPantallaAlto - (y + by + bh), bw, bh);
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        IdeQuad((float)bx, (float)by, (float)(bx + bw), (float)(by + bh), ListaColores[static_cast<int>(ColorID::headerColor)]);
        const std::string& s = lineas[(size_t)curLin];
        const int vis = IdeColVisual(s, curCol);
        const float ox = (float)bx + (float)bw * 0.5f - (float)(vis * LW * zoom);   // el cursor en el centro de la lupa
        const float oy = (float)(by + borderGS);
        { const float cx = ox + (float)(vis * LW * zoom), gz = (float)(zoom * ((GlobalScale > 0) ? GlobalScale : 1));
          IdeQuad(cx, oy, cx + gz, oy + (float)(lh * zoom), acento); }
        gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Texture2D);
        // (solo las letras que entran en la lupa, alrededor del cursor)
        const int colsLupa = bw / (LW * zoom) + 3;
        int c0 = vis - colsLupa / 2 - 1; if (c0 < 0) c0 = 0;
        int b0, b1;
        IdeTramoVisible(s, c0, colsLupa + 2, &b0, &b1);
        if (b0 < b1) {
            bool blq = (curLin < (int)bloqueAlInicio.size()) ? (bloqueAlInicio[(size_t)curLin] != 0) : false;
            LexLinea(s, &blq, &tipos, (size_t)b1);
            gfx::PushMatrix();
            gfx::Translatef(ox, oy, 0);
            gfx::Scalef((float)zoom, (float)zoom, 1.0f);
            IdeDibujarTramo(s, tipos, b0, b1, c0, 0, 0, LW);
            gfx::PopMatrix();
        }
        // el borde verde de la lupa
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        { const float g = (float)((GlobalScale > 0) ? GlobalScale : 1);
          IdeQuad((float)bx, (float)by, (float)(bx + bw), (float)by + g, acento);
          IdeQuad((float)bx, (float)(by + bh) - g, (float)(bx + bw), (float)(by + bh), acento);
          IdeQuad((float)bx, (float)by, (float)bx + g, (float)(by + bh), acento);
          IdeQuad((float)(bx + bw) - g, (float)by, (float)(bx + bw), (float)(by + bh), acento); }
        gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Texture2D);
    }
    // TELEFONO: con el lapiz apretado, abajo la barra de las dos softkeys: Copiar (izquierda) | Pegar (derecha)
    if (gIdeLapiz && ::viewPortActive == (ViewportBase*)this) {
        const int bh = lh + gapGS * 2, by = height - bh;
        gfx::Scissor(x, glY, width, height);
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        IdeQuad(0.0f, (float)by, (float)width, (float)height, ListaColores[static_cast<int>(ColorID::headerColor)]);
        IdeQuad(0.0f, (float)by, (float)width, (float)(by + ((GlobalScale > 0) ? GlobalScale : 1)), acento);
        gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Texture2D);
        const std::string izq = T("Copy"), der = T("Paste");
        IdeTexto(marginGS * 2, by + gapGS, izq, ListaColores[static_cast<int>(ColorID::blanco)]);
        IdeTexto(width - marginGS * 2 - IdeColVisual(der, (int)der.size()) * LW, by + gapGS, der, ListaColores[static_cast<int>(ColorID::blanco)]);
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

// "Open in IDE" de la BIBLIOTECA (un script elegido en el outliner): el primer IDE del layout lo abre.
// false = no hay ningun IDE en el layout (el usuario tiene que abrir uno).
#include "ViewPorts/LayoutArbol.h"
#include "ViewPorts/PopUp/ConfirmarPopup.h"   // descartar lo tipeado pide confirmacion
static IDE* PrimerIDE(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == 8 ? (IDE*)n : NULL;
    IDE* a = NULL;
    if (n->ContainerKind() == 1) { a = PrimerIDE(((ViewportRow*)n)->childA); return a ? a : PrimerIDE(((ViewportRow*)n)->childB); }
    a = PrimerIDE(((ViewportColumn*)n)->childA);
    return a ? a : PrimerIDE(((ViewportColumn*)n)->childB);
}
// el script que espera el "Si" del cartel de descartar (el IDE se vuelve a buscar al confirmar: el layout pudo
// cambiar mientras el cartel estaba abierto)
static std::string gIDEAbrirPendiente;
static void IDEAbrirConfirmado() {
    IDE* ide = PrimerIDE(LayoutRaizCompleta());
    if (ide && !gIDEAbrirPendiente.empty()) ide->AbrirArchivo(gIDEAbrirPendiente);
    gIDEAbrirPendiente.clear();
    g_redraw = true;
}
// un archivo cambio de lugar (el shader de un proyecto sin guardar entro al .w3d al guardarlo): los IDE que lo tienen
// abierto pasan a la ruta nueva, con lo que tenian (el texto es el mismo)
static void IDEMudarEn(ViewportBase* n, const std::string& viejo, const std::string& nuevo) {
    if (!n) return;
    if (n->isLeaf()) {
        if (n->ViewportKind() == 8) {
            IDE* ide = (IDE*)n;
            if (ide->archivo == viejo) { ide->archivo = nuevo; ide->SincronizarBarra(); g_redraw = true; }
        }
        return;
    }
    if (n->ContainerKind() == 1) { IDEMudarEn(((ViewportRow*)n)->childA, viejo, nuevo); IDEMudarEn(((ViewportRow*)n)->childB, viejo, nuevo); return; }
    IDEMudarEn(((ViewportColumn*)n)->childA, viejo, nuevo); IDEMudarEn(((ViewportColumn*)n)->childB, viejo, nuevo);
}
void IDEArchivoMudado(const std::string& viejo, const std::string& nuevo) {
    if (viejo != nuevo) IDEMudarEn(LayoutRaizCompleta(), viejo, nuevo);
}

bool IDEAbrirScript(const std::string& ruta) {
    IDE* ide = PrimerIDE(LayoutRaizCompleta());
    if (!ide) return false;
    // el MISMO script que ya esta abierto: no se recarga (lo tipeado y su undo se quedan)
    if (ide->archivo == ruta) { g_redraw = true; return true; }
    // con cambios sin guardar AVISA antes de descartarlos, como el selector de scripts del propio IDE
    if (ide->sucio) {
        gIDEAbrirPendiente = ruta;
        if (!confirmarPopup) confirmarPopup = new ConfirmarPopup();
        confirmarPopup->Abrir(std::string(T("Discard the unsaved changes of")) + " " + IDENombreScript(ide->archivo) + "?",
                              IDEAbrirConfirmado);
        return true;
    }
    if (!ide->AbrirArchivo(ruta)) return false;
    g_redraw = true;
    return true;
}
