#include "io/W3dWeb.h"   // navegador: guardar = descargar
#include "ViewPorts/PopUp/NumPad.h"   // KeyboardTextOpen / KeyboardNumberOpen: el teclado en pantalla (Ajustes)
#include "app/W3dTouchEditor.h"   // W3D_ONLY_BY_CONTROLS (Symbian: el soltar de PC solo con el dedo o el mouse)
#include "ViewPorts/PopUp/ConfirmPopup.h"   // Guardar/Refresh preguntan antes de pisar o descartar
#include "ViewPorts/PopUp/FindPopup.h"      // Buscar/Reemplazar: lo que se resalta
#include "ViewPorts/IDE.h"
#include "app/W3dKeyboard.h"   // W3dKeyboardOpen: el teclado del celular (y el layout que se adapta)
#include "w3dGraphics.h"                 // w3dEngine (abstraccion grafica)
#include "WhiskUI/draw/glesdraw.h"       // W3dScreenHeight
#include "WhiskUI/theme/colors.h"        // ColorList / ColorID
#include "WhiskUI/text/bitmapText.h"     // RenderBitmapText
#include "input/W3dClipboard.h"                // portapapeles del sistema (Core): copiar/pegar con navegador/notas
#include "WhiskUI/core/UI.h"            // marginGS / RowHeightGS / LetterWidthGS / GlobalScale / borderGS
#include "objects/Textures.h"           // Textures[0] = atlas de la fuente
#include "render/RenderOptions.h"       // g_redraw (render event-driven)
#include "script/W3dScript.h"           // W3dScriptData (rutas de scripts de los objetos)
#include "ViewPorts/Notifications.h"    // toasts de guardado / refresh
#include "w3dFilesystem.h"              // ReadTextFile / ListDir (colector + abrir)
#include "io/W3dContainer.h"
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>               // SDL_StartTextInput: el teclado del celular al tocar el codigo
#endif            // los .lua del proyecto v4 viven ADENTRO del .w3d
#include "W3dLang.h"                     // T() para los textos de la barra
#include "variables.h"                   // w3dPath (el proyecto abierto)
#include "animation/Animation.h"         // w3dGetTicks: doble/triple toque
#include "ViewPorts/TransformUI.h"       // ToolbarUsesTouch: las agarraderas solo con pantalla tactil
#include "objects/Materials.h"           // un shader guardado: los materiales que lo usan se compilan de nuevo
#include "objects/WrittenShader.h"
#include "objects/ShaderCPU.h"            // (sin shaders: Compilar prueba el @vertex de la CPU)
#include <stdio.h>                       // guardado atomico: fopen/fwrite/rename
#include <map>                           // los borradores (lo escrito sin guardar, por archivo)
#include <string.h>                      // strncmp: las keywords del lexer
#include <math.h>                        // cosf/sinf: las agarraderas
#include "app/W3dNoNearFar.h"   // (Windows: windows.h define near/far como macros vacias)

namespace gfx = w3dEngine;

// ---------------------------------------------------------------------------
//  Colores del RESALTADO (fijos, legibles sobre el fondo oscuro; la paleta del
//  tema no tiene roles de sintaxis, mismo criterio que los [ERROR] de Console)
// ---------------------------------------------------------------------------
static const float kIdeComment[4]     = { 0.52f, 0.66f, 0.50f, 1.0f }; // verde grisado
static const float kIdeString[4]      = { 0.93f, 0.78f, 0.38f, 1.0f }; // amarillo/naranja
static const float kIdeKeyword[4]     = { 0.52f, 0.72f, 0.98f, 1.0f }; // azul
static const float kIdeNumber[4]      = { 0.55f, 0.88f, 0.94f, 1.0f }; // celeste
static const float kIdeSelection[4]   = { 0.22f, 0.36f, 0.55f, 1.0f }; // fondo de la seleccion
static const float kIdeGuide[4]       = { 0.26f, 0.26f, 0.26f, 1.0f }; // lineas verticales de la sangria
static const float kIdeCoincide[4]    = { 0.42f, 0.33f, 0.08f, 1.0f }; // las coincidencias de Buscar
static const float kIdeActiveGuide[4] = { 0.50f, 0.50f, 0.50f, 1.0f }; // la del bloque donde esta el cursor
static const int   kIdeTab = 4;                                        // ancho de un nivel de sangria (en espacios)

// clipboard INTERNO: respaldo si el clipboard de SDL no esta (Symbian / headless)
static std::string gIDEInternalClip;

// ---------------------------------------------------------------------------
//  helpers chicos (C++03, sin to_string)
// ---------------------------------------------------------------------------
static std::string IdeNum(int v) {
    char buf[16];
    sprintf(buf, "%d", v);
    return std::string(buf);
}
static bool IdeFinishesIn(const std::string& s, const char* suf) {
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
static std::string IdeNormalize(const std::string& r) {
    std::string s = r;
    for (size_t i = 0; i < s.size(); i++) if (s[i] == '\\') s[i] = '/';
    return s;
}
// las dos rutas nombran el MISMO archivo? (exacta, o una es sufijo de la otra
// con borde de '/': "content/juego.lua" == "/abs/proj/content/juego.lua")
static bool IdeSameFile(const std::string& a, const std::string& b) {
    std::string na = IdeNormalize(a), nb = IdeNormalize(b);
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
static std::string IdeFolderProject() {
    if (w3dPath.empty()) return std::string();
    size_t s = w3dPath.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : w3dPath.substr(0, s);
}

std::string IDEScriptName(const std::string& path) {
    std::string r = IdeNormalize(path);
    std::string dir = IdeNormalize(IdeFolderProject());
    if (!dir.empty() && r.size() > dir.size() + 1 &&
        r.compare(0, dir.size(), dir) == 0 && r[dir.size()] == '/')
        return r.substr(dir.size() + 1);        // relativo al proyecto
    size_t s = r.find_last_of('/');
    return (s == std::string::npos) ? r : r.substr(s + 1); // solo el nombre
}

// ---------------------------------------------------------------------------
//  COLECTOR de scripts del proyecto: los .lua referenciados por las escenas
//  cargadas (scriptData de todo el arbol, en orden de arbol) + los *.lua de
//  content/. Sin repetidos (comparados con IdeSameFile). No se reusa el
//  colector de GuardarVersion porque ese re-parsea el .w3d del DISCO; aca el
//  arbol vivo ya tiene las rutas resueltas (y cubre lo editado sin guardar).
// ---------------------------------------------------------------------------
static void IdeAddPath(const std::string& path, std::vector<std::string>* out) {
    if (!IdeFinishesIn(path, ".lua") && !IdeFinishesIn(path, ".glsl")) return; // .luac (bytecode) no se edita como texto
    for (size_t i = 0; i < out->size(); i++)
        if (IdeSameFile((*out)[i], path)) return;
    out->push_back(path);
}
static void IdeCollectScene(Object* o, std::vector<std::string>* out) {
    if (!o) return;
    if (o->scriptData)
        for (size_t i = 0; i < o->scriptData->scripts.size(); i++)
            IdeAddPath(o->scriptData->scripts[i].path, out);
    for (size_t i = 0; i < o->Childrens.size(); i++)
        IdeCollectScene(o->Childrens[i], out);
}
void IDECollectScripts(std::vector<std::string>* paths) {
    paths->clear();
    IdeCollectScene(SceneCollection, paths);
    // LOS SHADERS de los materiales (objects/WrittenShader.h): los que usa algun material (tambien el de un proyecto sin
    // guardar, en la carpeta de trabajo) y los .glsl de shaders/ adentro del proyecto
    for (size_t i = 0; i < Materials.size(); i++)
        if (Materials[i] && !Materials[i]->shaderFile.empty()) IdeAddPath(Materials[i]->shaderFile, paths);
    if (W3dContainerHasMounted()) {
        std::vector<std::string> sh;
        W3dContainerListFolder("shaders/", sh);
        for (size_t i = 0; i < sh.size(); i++) IdeAddPath(sh[i], paths);
    }
    // DE DONDE SALEN LOS .lua QUE NO CUELGAN DE NINGUN OBJETO:
    //  - proyecto v4 (el .w3d es un contenedor): de la carpeta scripts/ de ADENTRO.
    //    Ya no se escanea content/ en el disco, que con el contenedor no existe
    //    (y si existe es la carpeta VIEJA del proyecto v3 que quedo al lado: listarla
    //    mostraba scripts fantasma que no son los que el proyecto usa).
    //  - proyecto v3 (JSON plano con los assets sueltos): content/, como siempre.
    if (W3dContainerHasMounted()) {
        std::vector<std::string> ents;
        W3dContainerListFolder("scripts/", ents);
        for (size_t i = 0; i < ents.size(); i++)
            if (IdeFinishesIn(ents[i], ".lua")) IdeAddPath(ents[i], paths);
        return;
    }
    std::string dir = IdeFolderProject();
    if (dir.empty()) return;
    std::string folder = dir + "/content";
    std::vector<w3dFileSystem::DirEntry> ents;
    if (w3dFileSystem::ListDir(folder, ents))
        for (size_t i = 0; i < ents.size(); i++)
            if (!ents[i].isDir && IdeFinishesIn(ents[i].name, ".lua"))
                IdeAddPath(folder + "/" + ents[i].name, paths);
}

// ---------------------------------------------------------------------------
//  LEXER lua por linea. Ver las LIMITACIONES en IDE.h: el estado entre lineas
//  es SOLO el comentario de bloque --[[ ]] (sin niveles [==[), y el string
//  largo [[...]] se pinta solo dentro de su linea.
// ---------------------------------------------------------------------------
static bool IdeIsIdent(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}
// sin armar un string por palabra (las tablas generadas tienen decenas de miles de identificadores por renglon)
static bool IdeIsKeyword(const char* p, size_t n) {
    static const char* kw[] = {
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "goto", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while", 0 };
    if (n < 2 || n > 8) return false;
    for (int i = 0; kw[i]; i++) if (strncmp(p, kw[i], n) == 0 && kw[i][n] == 0) return true;
    return false;
}
static void IdeMark(std::vector<unsigned char>* types, size_t d, size_t h, unsigned char t) {
    if (!types) return;
    for (size_t i = d; i < h && i < types->size(); i++) (*types)[i] = t;
}
// LOS SHADERS (.glsl, objects/WrittenShader.h) se pintan como GLSL: comentarios // y /* */, numeros, los tipos y las
// palabras del lenguaje, y las marcas @vertex / @fragment. Lo prende quien lexea un archivo .glsl (gIdeLexGLSL)
static bool gIdeLexGLSL = false;
static bool IdeIsKeywordGLSL(const char* p, size_t n) {
    static const char* kw[] = {
        "void", "float", "int", "bool", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4",
        "mat2", "mat3", "mat4", "sampler2D", "samplerCube", "in", "out", "inout", "const", "return", "if", "else", "for",
        "while", "do", "break", "continue", "discard", "true", "false", "struct", "uniform", "varying", "attribute",
        "precision", "highp", "mediump", "lowp", "@vertex", "@fragment", 0 };
    for (int i = 0; kw[i]; i++) if (strncmp(p, kw[i], n) == 0 && kw[i][n] == 0) return true;
    return false;
}
static void LexLineGLSL(const std::string& s, bool* inBlock, std::vector<unsigned char>* types, size_t until) {
    const size_t n = s.size();
    if (types) types->assign(until < n ? until : n, 0);
    bool block = inBlock ? *inBlock : false;
    size_t i = 0;
    while (i < n && i < until) {
        if (block) {   // adentro de /* ... */
            size_t end = s.find("*/", i);
            if (end == std::string::npos) { IdeMark(types, i, n, 1); i = n; }
            else { IdeMark(types, i, end + 2, 1); i = end + 2; block = false; }
            continue;
        }
        const char c = s[i];
        if (c == '/' && i + 1 < n && s[i + 1] == '/') { IdeMark(types, i, n, 1); i = n; continue; }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') { block = true; IdeMark(types, i, i + 2, 1); i += 2; continue; }
        if (c >= '0' && c <= '9' && (i == 0 || !IdeIsIdent(s[i - 1]))) {
            size_t j = i;
            while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == '.' || s[j] == 'e' || s[j] == 'E' ||
                             ((s[j] == '+' || s[j] == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E')))) j++;
            IdeMark(types, i, j, 4); i = j; continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '@' || c == '#') {
            size_t j = i + 1;
            while (j < n && IdeIsIdent(s[j])) j++;
            // (las directivas #define / #ifdef y las w3d_* del motor como las palabras del lenguaje)
            if (types && (c == '#' || IdeIsKeywordGLSL(s.c_str() + i, j - i) || (j - i > 4 && strncmp(s.c_str() + i, "w3d_", 4) == 0)))
                IdeMark(types, i, j, 3);
            i = j; continue;
        }
        i++;
    }
    if (inBlock && until >= n) *inBlock = block;
}
static bool IdeIsGLSL(const std::string& file) {
    return file.size() > 5 && file.compare(file.size() - 5, 5, ".glsl") == 0;
}
void IDE::LexLine(const std::string& s, bool* inBlock, std::vector<unsigned char>* types, size_t until) {
    if (gIdeLexGLSL) { LexLineGLSL(s, inBlock, types, until); return; }
    const size_t n = s.size();
    if (types) types->assign(until < n ? until : n, 0);   // (IdeMark no escribe mas alla del largo)
    bool block = inBlock ? *inBlock : false;
    size_t i = 0;
    while (i < n && i < until) {
        if (block) { // dentro de --[[ ... : comentario hasta el "]]"
            size_t end = s.find("]]", i);
            if (end == std::string::npos) { IdeMark(types, i, n, 1); i = n; }
            else { IdeMark(types, i, end + 2, 1); i = end + 2; block = false; }
            continue;
        }
        char c = s[i];
        if (c == '-' && i + 1 < n && s[i + 1] == '-') {           // comentario
            if (i + 3 < n && s[i + 2] == '[' && s[i + 3] == '[') { // --[[ (bloque)
                size_t end = s.find("]]", i + 4);
                if (end == std::string::npos) { IdeMark(types, i, n, 1); block = true; i = n; }
                else { IdeMark(types, i, end + 2, 1); i = end + 2; }
            } else { IdeMark(types, i, n, 1); i = n; }           // -- hasta el final
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
            IdeMark(types, i, j, 2); i = j; continue;
        }
        if (c == '[' && i + 1 < n && s[i + 1] == '[') {            // [[...]] (misma linea)
            size_t end = s.find("]]", i + 2);
            size_t j = (end == std::string::npos) ? n : end + 2;
            IdeMark(types, i, j, 2); i = j; continue;
        }
        if (c >= '0' && c <= '9' && (i == 0 || !IdeIsIdent(s[i - 1]))) { // numero
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
            IdeMark(types, i, j, 4); i = j; continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') { // palabra
            size_t j = i;
            while (j < n && IdeIsIdent(s[j])) j++;
            if (types && IdeIsKeyword(s.c_str() + i, j - i)) IdeMark(types, i, j, 3);
            i = j; continue;
        }
        i++;
    }
    if (inBlock && until >= n) *inBlock = block;
}

static int IdeColVisual(const std::string& s, int b);

void IDE::RecalculateBlocks() {
    gIdeLexGLSL = IdeIsGLSL(file);
    blockToStart.assign(lines.size(), 0);
    widthMaxCols = 0;
    bool b = false;
    for (size_t i = 0; i < lines.size(); i++) {
        blockToStart[i] = b ? 1 : 0;
        LexLine(lines[i], &b, NULL); // solo avanza el estado
        const int lv = IdeColVisual(lines[i], (int)lines[i].size());
        if (lv > widthMaxCols) widthMaxCols = lv;
    }
}

// ---------------------------------------------------------------------------
//  constructor / buffer
// ---------------------------------------------------------------------------
IDE::IDE() {
    dirty = false;
    curLin = 0; curCol = 0;
    hasSel = false; selLin = 0; selCol = 0;
    undoLastOp = 0;
    lastLines = -1; lastMaxWidth = 0; widthMaxCols = 0;
    selDrag = false; silent = false; hasTextDisk = false;
    consecutiveTaps = 0; tLastTap = 0; lastTapX = -1000; lastTapY = -1000; touchGesture = 0; magnifierX = 0; magnifierY = 0;
    lines.push_back(std::string()); // el buffer nunca esta vacio (1 linea vacia)
    BarCreate(); // [0] = icono de getType/split
    // botones por ROL (no por indice), como la barra del 2D/UV
    Button* b;
    b = new Button(T("Script"), -1);
    b->role = BRIDE_Script; b->dropdown = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Compile"));   // (solo con un shader .glsl abierto, al lado de su nombre: ver SyncBar)
    b->role = BRIDE_Compile;
    b->visible = false;
    BarButtons.push_back(b);
    b = new Button(T("File"), -1);   // menu ARCHIVO: Guardar / Guardar como... / dentro o fuera del w3d / Cargar .lua
    b->role = BRIDE_Save; b->dropdown = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Edit"), -1);   // menu EDITAR: deshacer / rehacer / cortar / copiar / pegar / buscar / reemplazar
    b->role = BRIDE_Edit; b->dropdown = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Select"), -1); // menu SELECCIONAR
    b->role = BRIDE_Select; b->dropdown = true; b->caretMenu = true;
    BarButtons.push_back(b);
    b = new Button(T("Refresh"));
    b->role = BRIDE_Refresh;
    BarButtons.push_back(b);
    RecalculateBlocks();
    // con un proyecto cargado, abrir el PRIMER script (el IDE nace mostrando codigo)
    OpenFirstScript();
}

// ---------------------------------------------------------------------------------------------------------------
//  LO QUE SE ESCRIBE NO SE PIERDE (cambiar el viewport destruye este IDE y al volver nace otro):
//   - un SCRIPT NUEVO existe apenas se escribe: entra al .w3d (en memoria, hasta guardar el proyecto) con un nombre
//     libre ("scripts/script.lua", "script-2.lua"...). Aparece en la biblioteca y ya se le puede poner a un objeto
//   - lo que se escribe en un script de ADENTRO del .w3d va a su entrada (la memoria del proyecto) en el cuadro
//   - uno de AFUERA (un archivo del disco) no se escribe solo: sus cambios sin guardar quedan como BORRADOR en memoria
//     y el IDE que lo vuelve a abrir los recupera (sin proyecto montado, un script nuevo tambien queda de borrador)
//   - el IDE que nace abre el ULTIMO script que se estaba editando (no el primero de la lista)
// ---------------------------------------------------------------------------------------------------------------
static std::map<std::string, std::string> gIdeDrafts;   // archivo ("" = nuevo sin proyecto) -> texto sin guardar
static std::string gIdeLast;                               // el ultimo script abierto / creado en un IDE
static std::string IdeFreeEntry() {
    std::string e = "scripts/script.lua";
    for (int n = 2; W3dContainerExists(e); n++) { char b[48]; snprintf(b, sizeof(b), "scripts/script-%d.lua", n); e = b; }
    return e;
}
void IDE::SyncMemory() {
    if (!dirty) return;
    if (file.empty()) {
        if (!W3dContainerHasMounted()) return;   // (sin proyecto: queda en el buffer; al cerrar, de borrador)
        file = IdeFreeEntry();
        gIdeLast = file;
        hasTextDisk = false;
        gIdeDrafts.erase(std::string());
        { extern void W3dResourcesViewInvalidate(); W3dResourcesViewInvalidate(); }   // (aparece en la biblioteca)
    }
    if (!W3dIsEntryName(file)) return;   // (los de afuera se guardan con Guardar)
    const std::string t = GetText();
    if (!W3dContainerWriteEntry(file, t.empty() ? "" : t.c_str(), t.size())) return;
    dirty = false;
    textDisk = t; hasTextDisk = true;
    g_redraw = true;
}
IDE::~IDE() {
    SyncMemory();
    if (dirty && !W3dIsEntryName(file)) gIdeDrafts[file] = GetText();   // (de afuera / sin proyecto)
}

void IDE::OpenFirstScript() {
    if (!file.empty()) return;
    // el ULTIMO que se estaba editando (si sigue existiendo); si no, un script nuevo a medio escribir; si no, el primero
    if (!gIdeLast.empty() && (W3dIsEntryName(gIdeLast) ? W3dContainerExists(gIdeLast) : w3dFileSystem::FileExists(gIdeLast)) &&
        OpenFile(gIdeLast)) return;
    std::map<std::string, std::string>::iterator newValue = gIdeDrafts.find(std::string());
    if (newValue != gIdeDrafts.end()) { SetText(newValue->second); return; }
    std::vector<std::string> paths;
    IDECollectScripts(&paths);
    if (!paths.empty()) OpenFile(paths[0]);
}

// ---------------------------------------------------------------------------------------------------------------
//  UTF-8. Las lineas guardan los BYTES del archivo tal cual (UTF-8), pero el cursor, el borrado, la seleccion y el
//  dibujo trabajan por LETRA: la 'n~' o una vocal con acento son 2 bytes y una sola columna en pantalla. Antes el
//  editor solo aceptaba ASCII ("no anda la letra n~ en el IDE", reporte del dueno). curCol / selCol siguen siendo
//  posiciones de BYTE, siempre al principio de una letra.
// ---------------------------------------------------------------------------------------------------------------
static bool IdeIsCont(unsigned char c) { return (c & 0xC0) == 0x80; }   // byte de continuacion (10xxxxxx)
static int IdeLettersBetween(const std::string& s, int b0, int b1) {      // letras (columnas) en los bytes [b0, b1)
    int v = 0;
    for (int i = b0; i < b1 && i < (int)s.size(); i++) if (!IdeIsCont((unsigned char)s[(size_t)i])) v++;
    return v;
}
static int IdeAdvanceLetters(const std::string& s, int b, int v) {     // el byte 'v' letras despues de b
    int i = b, n = (int)s.size();
    while (i < n && v > 0) { i++; while (i < n && IdeIsCont((unsigned char)s[(size_t)i])) i++; v--; }
    return i;
}
static int IdeColVisual(const std::string& s, int b) { return IdeLettersBetween(s, 0, b); } // byte -> columna en pantalla
static int IdeColByte(const std::string& s, int v) { return IdeAdvanceLetters(s, 0, v); }   // columna en pantalla -> byte
// RECORTE HORIZONTAL: los bytes [*b0, *b1) de la linea entre las columnas col0 y col0 + cant (lo que entra en el
// ancho). Un renglon mas corto que col0 da b0 == b1 (no se ve nada)
static void IdeSegmentVisible(const std::string& s, int col0, int count, int* b0, int* b1) {
    *b0 = IdeColByte(s, col0);
    *b1 = IdeAdvanceLetters(s, *b0, count);
}
static int IdeNextLetter(const std::string& s, int b) {
    int n = (int)s.size(); if (b >= n) return n;
    b++; while (b < n && IdeIsCont((unsigned char)s[(size_t)b])) b++;
    return b;
}
static int IdeLetterAnt(const std::string& s, int b) {
    if (b <= 0) return 0;
    b--; while (b > 0 && IdeIsCont((unsigned char)s[(size_t)b])) b--;
    return b;
}
static int IdeToLetterStart(const std::string& s, int b) {
    while (b > 0 && b < (int)s.size() && IdeIsCont((unsigned char)s[(size_t)b])) b--;
    return b;
}
// lo que se puede ESCRIBIR: imprimibles y letras no ASCII (UTF-8); fuera los de control
static std::string IdeFilterText(const std::string& t, bool withJumps) {
    std::string out;
    for (size_t i = 0; i < t.size(); i++) {
        const unsigned char c = (unsigned char)t[i];
        if (c == '\r') continue;
        if (c == '\n') { if (withJumps) out += '\n'; continue; }
        if (c == '\t') { out += "    "; continue; }   // la fuente no tiene tabulador
        if (c < 32 || c == 127) continue;
        out += (char)c;
    }
    return out;
}

// BUSCAR: comparacion sin distinguir mayusculas. ASCII y las letras latinas de 2 bytes (UTF-8 C3 80..9E -> +0x20:
// la N~ y las vocales con acento): el texto plegado tiene los MISMOS bytes de largo, asi las posiciones coinciden.
static std::string IdeCollapse(const std::string& s) {
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
static bool IdeIsLetterWord(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c >= 0x80; }

// las coincidencias de 'aguja' (ya plegada si !mayus) en la linea que EMPIEZAN en los bytes [desde, hasta): Buscar
// recorre la linea entera y el dibujo solo el tramo que se ve
static void IdeFindInLine(const std::string& s, const std::string& needle, bool caps, bool word,
                             size_t from, size_t until, std::vector<int>* col) {
    const size_t n = s.size(), la = needle.size();
    if (la == 0 || from >= n || from >= until) return;
    size_t z = until + la; if (z > n) z = n;   // las que empiezan antes de 'hasta' terminan antes de z
    const std::string haystack = caps ? s.substr(from, z - from) : IdeCollapse(s.substr(from, z - from));
    size_t p = haystack.find(needle);
    while (p != std::string::npos && from + p < until) {
        const size_t a = from + p, e = a + la;
        bool ok = true;
        if (word) {
            if (a > 0 && IdeIsLetterWord((unsigned char)s[a - 1])) ok = false;
            if (e < n && IdeIsLetterWord((unsigned char)s[e])) ok = false;
        }
        if (ok) col->push_back((int)a);
        p = haystack.find(needle, p + la);
    }
}

// texto crudo -> lineas del buffer (sin '\r'; tab -> 4 espacios: la fuente
// pixel no tiene tabulador y el cursor por columnas necesita ancho fijo)
static void IdeSplitInLines(const std::string& raw, std::vector<std::string>* out) {
    out->clear();
    std::string line;
    for (size_t i = 0; i < raw.size(); i++) {
        char c = raw[i];
        if (c == '\r') continue;
        if (c == '\n') { out->push_back(line); line.clear(); continue; }
        if (c == '\t') { line += "    "; continue; }
        line += c;
    }
    out->push_back(line); // la ultima (o unica) linea; "a\n" -> ["a", ""]
    if (out->empty()) out->push_back(std::string());
}

bool IDE::OpenFile(const std::string& path) {
    bool ok = false;
    std::string raw = w3dFileSystem::ReadTextFile(path, &ok);
    if (!ok) {
        Notify(std::string(T("IDE: could not read")) + " " + IDEScriptName(path), true);
        return false;
    }
    IdeSplitInLines(raw, &lines);
    file = path;
    dirty = false;
    textDisk = raw; hasTextDisk = true;
    gIdeLast = path;
    // un BORRADOR (lo que se habia escrito y no se guardo antes de cerrar el IDE): vuelve, sin guardar
    { std::map<std::string, std::string>::iterator b = gIdeDrafts.find(path);
      if (b != gIdeDrafts.end()) { IdeSplitInLines(b->second, &lines); dirty = true; gIdeDrafts.erase(b); } }
    curLin = 0; curCol = 0; hasSel = false; selDrag = false;
    PosX = 0; PosY = 0;
    stackUndo.clear(); stackRedo.clear(); undoLastOp = 0;
    lastLines = -1; // fuerza el recalculo del scroll en el proximo Render
    RecalculateBlocks();
    g_redraw = true;
    return true;
}

std::string IDE::GetText() const {
    std::string t;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i) t += '\n';
        t += lines[i];
    }
    return t;
}

void IDE::SetText(const std::string& text) {
    IdeSplitInLines(text, &lines);
    curLin = 0; curCol = 0; hasSel = false;
    stackUndo.clear(); stackRedo.clear(); undoLastOp = 0;
    dirty = true;
    lastLines = -1;
    RecalculateBlocks();
    g_redraw = true;
}

// REGLA: el IDE NUNCA rompe el archivo. Guardado ATOMICO: se escribe TODO a un
// .w3dtmp al lado y recien si salio completo se renombra sobre el .lua. Si el
// disco falla a mitad de camino, el original queda intacto.
static IDE* gIdeQuestion = NULL;   // el IDE que pregunto (la respuesta del popup vuelve a el)
static void IdeSaveForced() { if (gIdeQuestion) gIdeQuestion->Save(true); }
static void IdeRefreshForced() { if (gIdeQuestion) gIdeQuestion->Refresh(true); }

// UN SHADER GUARDADO (.glsl): los materiales que lo usan se compilan de nuevo (lo mismo que su boton "Compile") y
// el resultado sale en un aviso: el error con su renglon, o que compilo
static void IdeSavedShader(const std::string& file) {
    if (!IdeIsGLSL(file)) return;
    W3dShaderFileForget(file);   // (el paso "Shader" de las rutinas lo vuelve a compilar al dibujar)
    int n = 0; std::string error;
    for (size_t i = 0; i < Materials.size(); i++) {
        Material* m = Materials[i];
        if (!m || m->shaderFile != file) continue;
        if (!W3dShaderMaterialCompile(m) && error.empty()) error = m->shaderError;
        n++;
    }
    if (n == 0) return;
    if (!error.empty()) Notify(std::string(T("The shader does not compile")) + ": " + error, true);
    else Notify(IDEScriptName(file) + " " + T("compiled"), false);
    extern void RebindMaterialMeshPart(); RebindMaterialMeshPart();   // (la tarjeta del material dice el resultado)
    g_redraw = true;
}

bool IDE::Save(bool force) {
    if (file.empty()) return false;
    // EXTERNO editado por otro programa desde que se abrio: pisarlo sin avisar borra ese trabajo
    if (!force && hasTextDisk && !W3dIsEntryName(file)) {
        bool ok = false;
        const std::string inDisk = w3dFileSystem::ReadTextFile(file, &ok);
        if (ok && inDisk != textDisk && inDisk != GetText()) {
            gIdeQuestion = this;
            if (!confirmPopup) confirmPopup = new ConfirmPopup();
            confirmPopup->Open(std::string(T("The file was changed outside the editor. Overwrite it?")) + "\n" + IDEScriptName(file), IdeSaveForced);
            return false;
        }
    }
    // el outliner por recursos busca en los scripts las texturas que nombran: su foto se re-arma
    { extern void W3dResourcesViewInvalidate(); W3dResourcesViewInvalidate(); }
    std::string t = GetText();
    // ---------------------------------------------------------------------
    //  EL SCRIPT VIVE ADENTRO DEL .w3d: se guarda A SU ENTRADA.
    //  'archivo' aca no es una ruta de disco sino un nombre de entrada
    //  ("scripts/menu.lua"): el fopen de abajo crearia una carpeta scripts/
    //  RELATIVA AL DIRECTORIO ACTUAL (o sea al lado del binario del editor) y el
    //  cambio no llegaria nunca al proyecto. Va al overlay del contenedor, que lo
    //  sirve en caliente a todo el editor y lo hornea al zip al guardar el proyecto.
    // ---------------------------------------------------------------------
    if (W3dIsEntryName(file)) {
        if (!W3dContainerWriteEntry(file, t.empty() ? "" : t.c_str(), t.size())) {
            Notify(std::string(T("IDE: could not save inside the project:")) + " " + IDEScriptName(file), true);
            return false;
        }
        dirty = false;
        textDisk = t; hasTextDisk = true;
        if (!silent) Notify(IDEScriptName(file) + " " + T("saved (save the project so it stays in the .w3d)"), false);
        IdeSavedShader(file);
        ReloadLive(false);
        g_redraw = true;
        return true;
    }
    std::string tmp = file + ".w3dtmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) {
        Notify(std::string(T("IDE: could not write")) + " " + IDEScriptName(file), true);
        return false;
    }
    size_t esc = t.empty() ? 0 : fwrite(t.c_str(), 1, t.size(), f);
    bool ok = (fclose(f) == 0) && (esc == t.size());
    if (!ok) {
        remove(tmp.c_str());
        Notify(std::string(T("IDE: saving failed:")) + " " + IDEScriptName(file), true);
        return false;
    }
    // (ReplaceFile: tambien donde rename() no pisa un .lua que ya existe: Windows, Symbian, la 3DS)
    if (!w3dFileSystem::ReplaceFile(tmp, file)) {
        remove(tmp.c_str());
        Notify(std::string(T("IDE: saving failed:")) + " " + IDEScriptName(file), true);
        return false;
    }
    dirty = false;
    textDisk = t; hasTextDisk = true;
    gIdeDrafts.erase(file);
    W3dDownloadIfWeb(file);   // navegador: el .lua externo guardado baja a la PC
    if (!silent) Notify(IDEScriptName(file) + " " + T("saved"), false);
    IdeSavedShader(file);
    ReloadLive(false);
    g_redraw = true;
    return true;
}

bool IDE::SaveInProject() {
    if (file.empty()) { extern void IDEOpenSaveAs(IDE*); IDEOpenSaveAs(this); return false; }
    if (!W3dIsEntryName(file)) return Save();   // externo: el archivo del disco y listo
    silent = true;
    const bool ok = Save();
    silent = false;
    if (!ok) return false;
    // adentro del proyecto: queda listo para usar; en la PC con el .w3d en el disco tambien se reescribe el archivo, en
    // el navegador NO se descarga nada (ver IDESavedInProject)
    extern void IDESavedInProject(const std::string& name);
    IDESavedInProject(IDEScriptName(file));
    return true;
}

// REFRESH: TRAER el archivo. Para cuando el script se edita con OTRO editor (el archivo externo, o la entrada del .w3d
// que cambio por otro camino): el buffer pasa a ser lo que hay en el archivo. Antes Refresh GUARDABA el buffer encima
// del archivo y pisaba lo editado afuera (reporte del dueno); guardar es Guardar.
//  - con cambios locales sin guardar: pregunta antes de descartarlos (y lo de antes queda en el deshacer: Ctrl+Z)
//  - juego CORRIENDO: ademas se recargan los objetos que usan el script (ReloadLive)
void IDE::Refresh(bool force) {
    if (file.empty()) return;
    if (dirty && !force) {
        gIdeQuestion = this;
        if (!confirmPopup) confirmPopup = new ConfirmPopup();
        confirmPopup->Open(std::string(T("The editor has unsaved changes. Discard them and reload the file?")) + "\n" + IDEScriptName(file), IdeRefreshForced);
        return;
    }
    bool ok = false;
    const std::string raw = w3dFileSystem::ReadTextFile(file, &ok);
    if (!ok) { Notify(std::string(T("IDE: could not read")) + " " + IDEScriptName(file), true); return; }
    std::vector<std::string> newValue;
    IdeSplitInLines(raw, &newValue);
    if (newValue != lines) {
        Mutate(0);                       // lo que habia (incluso lo descartado) se recupera con Ctrl+Z
        lines = newValue;
        if (curLin >= (int)lines.size()) curLin = (int)lines.size() - 1;
        if (curCol > (int)lines[curLin].size()) curCol = (int)lines[curLin].size();
        curCol = IdeToLetterStart(lines[curLin], curCol);
        hasSel = false; undoLastOp = 0;
        lastLines = -1;
        RecalculateBlocks();
        EnsureCursorVisible();
        Notify(IDEScriptName(file) + ": " + T("reloaded from the file"), false);
    } else {
        Notify(IDEScriptName(file) + ": " + T("no new changes in the file"), false);
    }
    dirty = false;
    textDisk = raw; hasTextDisk = true;
    ReloadLive(true);
    g_redraw = true;
}

// Con el juego CORRIENDO (SimActive) se recargan SOLO los objetos cuyos scripts usan este archivo
// (SimScriptsChanged): sus lua_State se recrean (variables locales de cero + start() de nuevo) y el RESTO del juego
// (posiciones, otros scripts, escena activa) se PRESERVA tal cual. Con el juego parado no hace nada (el proximo Play
// ya lee lo nuevo).
void IDE::ReloadLive(bool notifyIfNothing) {
    // (SimGame.cpp -SimActive/SimScriptsChanged- compila tambien en el .mmp del N95)
    extern bool SimActive();
    extern void SimScriptsChanged(Object*);
    if (file.empty() || !SimActive()) return;
    // juntar primero (SimScriptsChanged puede agregar a la lista interna de la sim)
    std::vector<Object*> use;
    std::vector<Object*> stack;
    if (SceneCollection) stack.push_back(SceneCollection);
    while (!stack.empty()) {
        Object* o = stack.back(); stack.pop_back();
        if (o->scriptData)
            for (size_t i = 0; i < o->scriptData->scripts.size(); i++)
                if (IdeSameFile(o->scriptData->scripts[i].path, file)) {
                    use.push_back(o);
                    break;
                }
        for (size_t i = 0; i < o->Childrens.size(); i++) stack.push_back(o->Childrens[i]);
    }
    for (size_t i = 0; i < use.size(); i++) SimScriptsChanged(use[i]);
    if (!use.empty() || notifyIfNothing)
        Notify(std::string(T("Refresh:")) + " " + IdeNum((int)use.size()) + " " + T("object(s) reloaded live"), false);
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  BUSCAR / REEMPLAZAR (lo usa el popup PopUp/FindPopup)
// ---------------------------------------------------------------------------
void IDE::Matches(const std::string& t, bool caps, bool word, std::vector<int>* lin, std::vector<int>* col) const {
    lin->clear(); col->clear();
    if (t.empty()) return;
    const std::string needle = caps ? t : IdeCollapse(t);
    std::vector<int> cols;
    for (size_t l = 0; l < lines.size(); l++) {
        cols.clear();
        IdeFindInLine(lines[l], needle, caps, word, 0, lines[l].size(), &cols);
        for (size_t k = 0; k < cols.size(); k++) { lin->push_back((int)l); col->push_back(cols[k]); }
    }
}
bool IDE::GoToMatch(const std::string& t, bool caps, bool word, int dir) {
    std::vector<int> L, C;
    Matches(t, caps, word, &L, &C);
    if (L.empty()) return false;
    // desde donde: el FINAL de la seleccion para adelante, el PRINCIPIO para atras
    int l0 = curLin, c0 = curCol;
    if (hasSel) { int a, b, c, d; NormalizeSel(&a, &b, &c, &d); if (dir > 0) { l0 = c; c0 = d; } else { l0 = a; c0 = b; } }
    int picked = -1;
    if (dir > 0) {
        for (size_t i = 0; i < L.size(); i++) if (L[i] > l0 || (L[i] == l0 && C[i] >= c0)) { picked = (int)i; break; }
        if (picked < 0) picked = 0;                       // da la vuelta al principio
    } else {
        for (int i = (int)L.size() - 1; i >= 0; i--) if (L[(size_t)i] < l0 || (L[(size_t)i] == l0 && C[(size_t)i] < c0)) { picked = i; break; }
        if (picked < 0) picked = (int)L.size() - 1;       // da la vuelta al final
    }
    selLin = L[(size_t)picked]; selCol = C[(size_t)picked];
    curLin = selLin; curCol = selCol + (int)t.size();
    hasSel = true;
    undoLastOp = 0;
    EnsureCursorVisible();
    g_redraw = true;
    return true;
}
bool IDE::SelectionIsMatch(const std::string& t, bool caps) const {
    if (!hasSel || t.empty()) return false;
    const std::string sel = TextSelected();
    return caps ? (sel == t) : (IdeCollapse(sel) == IdeCollapse(t));
}
int IDE::ReplaceAll(const std::string& t, const std::string& by, bool caps, bool word) {
    std::vector<int> L, C;
    Matches(t, caps, word, &L, &C);
    if (L.empty()) return 0;
    Mutate(0);   // todas juntas: UN paso de deshacer
    for (int i = (int)L.size() - 1; i >= 0; i--)   // de atras para adelante: las posiciones de antes no se corren
        lines[(size_t)L[(size_t)i]].replace((size_t)C[(size_t)i], t.size(), by);
    hasSel = false;
    if (curCol > (int)lines[curLin].size()) curCol = (int)lines[curLin].size();
    curCol = IdeToLetterStart(lines[curLin], curCol);
    lastLines = -1;
    RecalculateBlocks();
    EnsureCursorVisible();
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
void IDE::Mutate(int op) {
    if (op == 0 || op != undoLastOp || stackUndo.empty()) {
        TextState e;
        e.lines = lines; e.curLin = curLin; e.curCol = curCol;
        stackUndo.push_back(e);
        if (stackUndo.size() > 200) stackUndo.erase(stackUndo.begin());
    }
    stackRedo.clear();   // una edicion nueva: lo deshecho ya no se puede rehacer
    undoLastOp = op;
    dirty = true;
    // UN SCRIPT NUEVO (sin archivo) NACE ADENTRO DEL PROYECTO apenas se empieza a escribir: la biblioteca ya lo lista y
    // se le puede poner a un objeto. Lo que se siga escribiendo queda "sin guardar" ('*'); guardarlo lo pone al dia
    if (file.empty()) SpawnInProject();
}

// el nombre libre para un script nuevo: scripts/script.lua, scripts/script.001.lua...
void IDE::SpawnInProject() {
    if (!file.empty() || !W3dContainerEnsure()) return;
    std::string entry = "scripts/script.lua";
    for (int n = 1; W3dContainerExists(entry) && n < 1000; n++) {
        char b[48]; snprintf(b, sizeof(b), "scripts/script.%03d.lua", n);
        entry = b;
    }
    const std::string t = GetText();
    if (!W3dContainerWriteEntry(entry, t.empty() ? "" : t.c_str(), t.size())) return;
    file = entry;
    textDisk = t; hasTextDisk = true;
    { extern void W3dResourcesViewInvalidate(); W3dResourcesViewInvalidate(); }
    g_redraw = true;
}

void IDE::UndoText() {
    if (stackUndo.empty()) return;
    { TextState now; now.lines = lines; now.curLin = curLin; now.curCol = curCol; stackRedo.push_back(now); }
    TextState& e = stackUndo.back();
    lines  = e.lines;
    curLin = e.curLin; curCol = e.curCol;
    stackUndo.pop_back();
    hasSel = false;
    undoLastOp = 0;
    dirty = true; // no se compara contra el disco: deshacer sigue siendo "sin guardar"
    lastLines = -1;
    RecalculateBlocks();
    EnsureCursorVisible();
    g_redraw = true;
}

void IDE::RedoText() {
    if (stackRedo.empty()) return;
    { TextState now; now.lines = lines; now.curLin = curLin; now.curCol = curCol; stackUndo.push_back(now); }
    TextState e = stackRedo.back();
    stackRedo.pop_back();
    lines  = e.lines;
    curLin = e.curLin; curCol = e.curCol;
    hasSel = false;
    undoLastOp = 0;
    dirty = true;
    lastLines = -1;
    RecalculateBlocks();
    EnsureCursorVisible();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  cursor / seleccion
// ---------------------------------------------------------------------------
void IDE::NormalizeSel(int* l0, int* c0, int* l1, int* c1) const {
    if (selLin < curLin || (selLin == curLin && selCol <= curCol)) {
        *l0 = selLin; *c0 = selCol; *l1 = curLin; *c1 = curCol;
    } else {
        *l0 = curLin; *c0 = curCol; *l1 = selLin; *c1 = selCol;
    }
}

void IDE::CursorA(int lin, int col, bool shift) {
    if (shift && !hasSel) { selLin = curLin; selCol = curCol; hasSel = true; }
    if (lin < 0) lin = 0;
    if (lin >= (int)lines.size()) lin = (int)lines.size() - 1;
    if (col < 0) col = 0;
    if (col > (int)lines[lin].size()) col = (int)lines[lin].size();
    col = IdeToLetterStart(lines[lin], col);   // nunca a la mitad de una letra de 2+ bytes
    curLin = lin; curCol = col;
    if (!shift) hasSel = false;
    if (hasSel && selLin == curLin && selCol == curCol) hasSel = false;
    undoLastOp = 0; // moverse corta el grupo de tipeo del undo
    EnsureCursorVisible();
    g_redraw = true;
}

void IDE::MoveCursor(int dLin, int dCol, bool shift) {
    int lin = curLin, col = curCol;
    if (dCol < 0) {
        if (col > 0) col = IdeLetterAnt(lines[lin], col);
        else if (lin > 0) { lin--; col = (int)lines[lin].size(); }   // cruzar al final de la linea anterior
    } else if (dCol > 0) {
        if (col < (int)lines[lin].size()) col = IdeNextLetter(lines[lin], col);
        else if (lin + 1 < (int)lines.size()) { lin++; col = 0; }     // cruzar al inicio de la siguiente
    }
    if (dLin) {   // arriba/abajo: misma COLUMNA EN PANTALLA (no el mismo byte) en la linea nueva
        const int vis = IdeColVisual(lines[lin], col);
        lin += dLin;
        if (lin < 0) lin = 0;
        if (lin >= (int)lines.size()) lin = (int)lines.size() - 1;
        col = IdeColByte(lines[lin], vis);
    }
    CursorA(lin, col, shift);
}

void IDE::Home(bool shift) { CursorA(curLin, 0, shift); }
void IDE::End(bool shift)  { CursorA(curLin, (int)lines[curLin].size(), shift); }

void IDE::PageUpDown(int dir, bool shift) {
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    int visible  = (height - BarTopOffset()) / lh;
    if (visible < 1) visible = 1;
    int lin = curLin + dir * visible;
    if (lin < 0) lin = 0;
    if (lin >= (int)lines.size()) lin = (int)lines.size() - 1;
    CursorA(lin, IdeColByte(lines[lin], IdeColVisual(lines[curLin], curCol)), shift);
}

void IDE::SelectAll() {
    selLin = 0; selCol = 0;
    curLin = (int)lines.size() - 1;
    curCol = (int)lines[curLin].size();
    hasSel = !(curLin == 0 && curCol == 0);
    undoLastOp = 0;
    g_redraw = true;
}

std::string IDE::TextSelected() const {
    if (!hasSel) return std::string();
    int l0, c0, l1, c1;
    NormalizeSel(&l0, &c0, &l1, &c1);
    if (l0 == l1) return lines[l0].substr(c0, c1 - c0);
    std::string t = lines[l0].substr(c0);
    for (int l = l0 + 1; l < l1; l++) { t += '\n'; t += lines[l]; }
    t += '\n'; t += lines[l1].substr(0, c1);
    return t;
}

// saca la seleccion del buffer SIN tocar el undo (los llamadores agrupan)
static void IdeDeleteRange(std::vector<std::string>* lines, int l0, int c0, int l1, int c1) {
    if (l0 == l1) {
        (*lines)[l0].erase(c0, c1 - c0);
        return;
    }
    (*lines)[l0] = (*lines)[l0].substr(0, c0) + (*lines)[l1].substr(c1);
    lines->erase(lines->begin() + l0 + 1, lines->begin() + l1 + 1);
}

void IDE::DeleteSelection() {
    if (!hasSel) return;
    Mutate(0); // operacion propia (siempre corta grupo)
    int l0, c0, l1, c1;
    NormalizeSel(&l0, &c0, &l1, &c1);
    IdeDeleteRange(&lines, l0, c0, l1, c1);
    curLin = l0; curCol = c0; hasSel = false;
    lastLines = -1;
    RecalculateBlocks();
    EnsureCursorVisible();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  edicion
// ---------------------------------------------------------------------------
void IDE::InsertChar(char c) { InsertTyped(std::string(1, c)); }

// lo tipeado (una letra, que puede ser de varios bytes UTF-8): agrupa el undo como el tipeo
void IDE::InsertTyped(const std::string& t) {
    if (t.empty()) return;
    if (hasSel) {
        Mutate(0); // reemplazo de la seleccion: UNA operacion
        int l0, c0, l1, c1;
        NormalizeSel(&l0, &c0, &l1, &c1);
        IdeDeleteRange(&lines, l0, c0, l1, c1);
        curLin = l0; curCol = c0; hasSel = false;
        undoLastOp = 1; // lo que se siga tipeando se agrupa con este snapshot
    } else {
        Mutate(1); // tipeo: agrupa la corrida
    }
    lines[curLin].insert((size_t)curCol, t);
    curCol += (int)t.size();
    RecalculateBlocks();
    EnsureCursorVisible();
    g_redraw = true;
}

void IDE::InsertText(const std::string& t) {
    Mutate(0);
    if (hasSel) {
        int l0, c0, l1, c1;
        NormalizeSel(&l0, &c0, &l1, &c1);
        IdeDeleteRange(&lines, l0, c0, l1, c1);
        curLin = l0; curCol = c0; hasSel = false;
    }
    std::vector<std::string> newValue;
    IdeSplitInLines(t, &newValue);
    std::string queue = lines[curLin].substr((size_t)curCol);
    lines[curLin] = lines[curLin].substr(0, (size_t)curCol) + newValue[0];
    for (size_t i = 1; i < newValue.size(); i++) {
        curLin++;
        lines.insert(lines.begin() + curLin, newValue[i]);
    }
    curCol = (int)lines[curLin].size();
    lines[curLin] += queue;
    lastLines = -1;
    RecalculateBlocks();
    EnsureCursorVisible();
    g_redraw = true;
}

void IDE::Enter() {
    if (hasSel) { InsertText("\n"); return; }
    Mutate(0);
    std::string queue = lines[curLin].substr((size_t)curCol);
    lines[curLin].erase((size_t)curCol);
    curLin++;
    lines.insert(lines.begin() + curLin, queue);
    curCol = 0;
    lastLines = -1;
    RecalculateBlocks();
    EnsureCursorVisible();
    g_redraw = true;
}

// espacios al principio de la linea (la sangria)
static int IdeIndent(const std::string& s) { int n = 0; while (n < (int)s.size() && s[(size_t)n] == ' ') n++; return n; }

// la sangria que dibujan las guias en la linea l: la suya, o si esta en blanco, la menor entre la linea con texto de
// arriba y la de abajo (asi las guias atraviesan los renglones vacios de adentro de un bloque)
static int IdeIndentGuide(const std::vector<std::string>& lines, int l) {
    const std::string& ln = lines[(size_t)l];
    const int ind = IdeIndent(ln);
    if (ind < (int)ln.size()) return ind;
    int up = 0, down = 0;
    for (int k = l - 1, m = 0; k >= 0 && m < 300; k--, m++) { const std::string& o = lines[(size_t)k]; const int q = IdeIndent(o); if (q < (int)o.size()) { up = q; break; } }
    for (int k = l + 1, m = 0; k < (int)lines.size() && m < 300; k++, m++) { const std::string& o = lines[(size_t)k]; const int q = IdeIndent(o); if (q < (int)o.size()) { down = q; break; } }
    return (up < down) ? up : down;
}

// TAB como en los editores de codigo:
//  - sin seleccion (o dentro de una linea): espacios hasta el proximo tope de 4 (la fuente no tiene tabulador)
//  - con varias lineas seleccionadas: les suma un nivel de sangria a todas
//  - Shift+Tab: le saca un nivel a la linea del cursor (o a todas las seleccionadas)
void IDE::TabKey(bool shift) {
    const bool multi = hasSel && selLin != curLin;
    if (!shift && !multi) {
        const int vis = hasSel ? 0 : IdeColVisual(lines[curLin], curCol);
        InsertText(std::string((size_t)(kIdeTab - vis % kIdeTab), ' '));
        return;
    }
    int l0 = curLin, l1 = curLin;
    if (hasSel) {
        int a0, c0, a1, c1; NormalizeSel(&a0, &c0, &a1, &c1);
        l0 = a0; l1 = a1;
        if (l1 > l0 && c1 == 0) l1--;   // la seleccion termina al principio de una linea: esa no entra
    }
    Mutate(0);
    for (int l = l0; l <= l1; l++) {
        std::string& ln = lines[(size_t)l];
        int change = 0;
        if (!shift) {
            if (ln.empty()) continue;   // las lineas vacias quedan vacias
            ln.insert((std::string::size_type)0, (std::string::size_type)kIdeTab, ' ');   // posicion explicita: con 0 a secas STLport ve tambien la sobrecarga por iterador
            change = kIdeTab;
        } else {
            const int q = IdeIndent(ln);
            int n = q % kIdeTab; if (n == 0) n = (q < kIdeTab) ? q : kIdeTab;   // hasta el tope anterior
            if (n <= 0) continue;
            ln.erase(0, (size_t)n);
            change = -n;
        }
        // el cursor y la punta de la seleccion acompanian al texto (una punta en la columna 0 se queda ahi al sumar)
        if (curLin == l && (change < 0 || curCol > 0)) { curCol += change; if (curCol < 0) curCol = 0; }
        if (hasSel && selLin == l && (change < 0 || selCol > 0)) { selCol += change; if (selCol < 0) selCol = 0; }
    }
    lastLines = -1;
    RecalculateBlocks();
    EnsureCursorVisible();
    g_redraw = true;
}

void IDE::Backspace() {
    if (hasSel) { DeleteSelection(); return; }
    if (curCol == 0 && curLin == 0) return;
    Mutate(2); // los backspace seguidos se agrupan
    if (curCol > 0 && IdeIndent(lines[curLin]) >= curCol) {   // dentro de la sangria: un nivel entero
        int n = curCol % kIdeTab; if (n == 0) n = kIdeTab; if (n > curCol) n = curCol;
        lines[curLin].erase((size_t)(curCol - n), (size_t)n);
        curCol -= n;
    } else if (curCol > 0) {   // la letra entera (1 a 4 bytes)
        const int a = IdeLetterAnt(lines[curLin], curCol);
        lines[curLin].erase((size_t)a, (size_t)(curCol - a));
        curCol = a;
    } else { // unir con la linea anterior
        curCol = (int)lines[curLin - 1].size();
        lines[curLin - 1] += lines[curLin];
        lines.erase(lines.begin() + curLin);
        curLin--;
        lastLines = -1;
    }
    RecalculateBlocks();
    EnsureCursorVisible();
    g_redraw = true;
}

void IDE::DelForward() {
    if (hasSel) { DeleteSelection(); return; }
    if (curCol >= (int)lines[curLin].size() && curLin + 1 >= (int)lines.size()) return;
    Mutate(3); // los delete seguidos se agrupan
    if (curCol < (int)lines[curLin].size()) {   // la letra entera
        lines[curLin].erase((size_t)curCol, (size_t)(IdeNextLetter(lines[curLin], curCol) - curCol));
    } else { // unir con la linea siguiente
        lines[curLin] += lines[curLin + 1];
        lines.erase(lines.begin() + curLin + 1);
        lastLines = -1;
    }
    RecalculateBlocks();
    g_redraw = true;
}

// ---------------------------------------------------------------------------
//  clipboard: el del SISTEMA via W3dClipboard (Core) -> compartido con navegador/notas
//  en TODAS las plataformas (SDL en PC/web, CClipboard en Symbian). gIDEInternalClip es
//  respaldo local (headless / si el sistema no responde).
// ---------------------------------------------------------------------------
void IDE::Copy() {
    if (!hasSel) return;
    gIDEInternalClip = TextSelected();
    w3dEngine::W3dClipboardSet(gIDEInternalClip);   // portapapeles del sistema (cualquier app lo puede pegar)
}

void IDE::Cut() {
    if (!hasSel) return;
    Copy();
    DeleteSelection();
}

void IDE::Paste() {
    std::string t = w3dEngine::W3dClipboardGet(); // el clipboard del SISTEMA (de cualquier programa: navegador, notas)
    if (t.empty()) t = gIDEInternalClip; // headless / si el sistema no dio nada
    if (t.empty()) return;
    // imprimibles, letras no ASCII (n~, acentos: UTF-8) y saltos; fuera los de control
    const std::string filtered = IdeFilterText(t, true);
    if (!filtered.empty()) InsertText(filtered);
}

// ---------------------------------------------------------------------------
//  teclado: el ruteo de PC (controls.cpp) le da al IDE enfocado por hover la
//  PRIMERA palabra: si KeyEditor consume, ningun atajo global corre.
// ---------------------------------------------------------------------------
bool IDE::KeyEditor(int key, bool repeat) {
    (void)repeat;
    bool shift = LShiftPressed;
    if (LCtrlPressed) {
        switch (key) {
            case W3dK_C: Copy(); return true;
            case W3dK_X: Cut(); return true;
            case W3dK_V: Paste(); return true;
            case W3dK_A: SelectAll(); return true;
            case W3dK_Z: if (shift) RedoText(); else UndoText(); return true;   // undo/redo DEL TEXTO, no el global
            case W3dK_Y: RedoText(); return true;
            case W3dK_F: { extern void IDEOpenFind(IDE*, bool); IDEOpenFind(this, false); return true; }   // Buscar
            case W3dK_H: { extern void IDEOpenFind(IDE*, bool); IDEOpenFind(this, true);  return true; }   // Reemplazar
            case W3dK_S: SaveInProject(); return true;             // guarda el .lua (y el .w3d si vive adentro)
            case W3dK_HOME: CursorA(0, 0, shift); return true;     // inicio del archivo
            case W3dK_END:
                CursorA((int)lines.size() - 1, (int)lines.back().size(), shift);
                return true;
        }
        return false; // otros Ctrl+... siguen su camino normal
    }
    switch (key) {
        case W3dK_LEFT:  MoveCursor(0, -1, shift); return true;
        case W3dK_RIGHT: MoveCursor(0, +1, shift); return true;
        case W3dK_UP:    MoveCursor(-1, 0, shift); return true;
        case W3dK_DOWN:  MoveCursor(+1, 0, shift); return true;
        case W3dK_HOME:  Home(shift); return true;
        case W3dK_END:   End(shift); return true;
        case W3dK_PGUP:  PageUpDown(-1, shift); return true;
        case W3dK_PGDN:  PageUpDown(+1, shift); return true;
        case W3dK_RETURN:
        case W3dK_KP_ENTER: Enter(); return true;
        case W3dK_TAB:      TabKey(shift); return true;   // Shift+Tab = sacar un nivel de sangria
        case W3dK_BACKSPACE: Backspace(); return true;
        case W3dK_DELETE:    DelForward(); return true;
        case W3dK_ESCAPE:
            if (hasSel) { hasSel = false; g_redraw = true; return true; }
            return false; // sin seleccion, el ESC sigue siendo global (pausa, etc.)
    }
    // imprimibles: el CARACTER entra por SDL_TEXTINPUT (respeta el layout del
    // teclado); aca solo se CONSUME el keydown para que no dispare atajos
    // del editor (g/r/s/x moverian la escena mientras escribis codigo)
    if (key >= 32 && key < 127) return true;
    return false; // F11, modificadores, etc: siguen su camino
}

void IDE::TextTyped(const char* utf8) {
    if (!utf8) return;
    InsertTyped(IdeFilterText(utf8, false));   // tambien la n~ y los acentos (llegan en UTF-8)
}

void IDE::event_key_down(int keyCode, bool repeat) {
    // ruta normal de viewports (Symbian / fallback): mismas teclas de edicion
    KeyEditor(keyCode, repeat);
}

// ---------------------------------------------------------------------------
//  mouse / touch
// ---------------------------------------------------------------------------
int IDE::GutterW() const {
    int n = (int)lines.size(), d = 1;
    while (n >= 10) { n /= 10; d++; }
    if (d < 2) d = 2;
    return borderGS + marginGS + d * LetterWidthGS + marginGS;
}

// click en el CONTENIDO: posiciona el cursor (shift extiende la seleccion) y
// arma el drag de seleccion. mx/my en coords de VENTANA.
// pantalla -> renglon y byte
void IDE::PointToText(int mx, int my, int* lin, int* col) const {
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    const int lx = mx - x, ly = my - y;
    const int textX0 = GutterW() + marginGS + PosX;
    const int textY0 = BarTopOffset() + marginGS + PosY;
    int l = (ly < textY0) ? 0 : (ly - textY0) / lh;
    if (l >= (int)lines.size()) l = (int)lines.size() - 1;
    if (l < 0) l = 0;
    int c = (lx - textX0 + LetterWidthGS / 2) / (LetterWidthGS > 0 ? LetterWidthGS : 1);
    *lin = l;
    *col = (c < 0) ? 0 : IdeColByte(lines[(size_t)l], c);
}

void IDE::TextToPoint(int lin, int col, int* mx, int* my) const {
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    const int LW = (LetterWidthGS > 0) ? LetterWidthGS : 6;
    if (lin < 0) lin = 0; if (lin >= (int)lines.size()) lin = (int)lines.size() - 1;
    *mx = x + GutterW() + marginGS + PosX + IdeColVisual(lines[(size_t)lin], col) * LW;   // el hueco ANTES de la letra
    *my = y + BarTopOffset() + marginGS + PosY + lin * lh + lh / 2;
}

void IDE::SelectWord() {
    const std::string& s = lines[(size_t)curLin];
    int a = curCol, b = curCol;
    while (a > 0 && IdeIsLetterWord((unsigned char)s[(size_t)(a - 1)])) a--;
    while (b < (int)s.size() && IdeIsLetterWord((unsigned char)s[(size_t)b])) b++;
    if (a == b) {   // no es una palabra (espacio, simbolo): la letra de al lado
        if (b < (int)s.size()) b = IdeNextLetter(s, b); else if (a > 0) a = IdeLetterAnt(s, a);
    }
    selLin = curLin; selCol = a; curCol = b;
    hasSel = (a != b);
    undoLastOp = 0;
    g_redraw = true;
}
void IDE::SelectLine() {
    selLin = curLin; selCol = 0;
    if (curLin + 1 < (int)lines.size()) { curLin++; curCol = 0; }   // con el salto de linea (como los editores de codigo)
    else curCol = (int)lines[(size_t)curLin].size();
    hasSel = !(selLin == curLin && selCol == curCol);
    undoLastOp = 0;
    EnsureCursorVisible();
    g_redraw = true;
}

void IDE::ClickContent(int mx, int my, bool shift) {
    int lin = 0, col = 0;
    PointToText(mx, my, &lin, &col);
    // DOBLE / TRIPLE toque (o click): rapido y en el mismo lugar
    const unsigned int now = w3dGetTicks();
    const int near = 12 * (GlobalScale > 0 ? GlobalScale : 1);
    if (!shift && now - tLastTap < 450 && (mx - lastTapX) * (mx - lastTapX) + (my - lastTapY) * (my - lastTapY) < near * near)
        consecutiveTaps++;
    else
        consecutiveTaps = 1;
    tLastTap = now; lastTapX = mx; lastTapY = my;
    CursorA(lin, col, shift);
    if (consecutiveTaps == 2) SelectWord();
    else if (consecutiveTaps >= 3) { SelectLine(); consecutiveTaps = 0; }   // (un 4to toque vuelve a empezar)
    selDrag = true;
    ViewPortClickDown = true; // el drag de seleccion congela el foco por hover
    // TACTIL (Android): el teclado del celular NO aparece solo. En PC SDL deja la entrada de texto prendida desde
    // el arranque; en Android hay que pedirlo. Un TAP sobre el codigo lo levanta (como los campos de texto del panel).
#ifndef W3D_SYMBIAN
    { extern bool g_uiTapInProgress; if (g_uiTapInProgress) KeyboardIDEOpen(this); }
#endif
    g_redraw = true;
}

// PULSACION LARGA: la lupa. El cursor va al dedo y lo sigue mientras se arrastra; arriba del dedo se ve el renglon
// AGRANDADO con el cursor, asi se lo deja justo donde se quiere aunque el dedo tape el texto.
bool IDE::PressLong(int mx, int my) {
    int lin = 0, col = 0;
    PointToText(mx, my, &lin, &col);
    CursorA(lin, col, false);
    touchGesture = 1; magnifierX = mx; magnifierY = my;
    g_redraw = true;
    return true;
}
// las agarraderas cuelgan debajo de las dos puntas de la seleccion
static int IdeHandleRadius() { return 5 * (GlobalScale > 0 ? GlobalScale : 1); }
int IDE::HandleIn(int mx, int my) const {
    if (!hasSel || !ToolbarUsesTouch()) return 0;
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    const int LW = (LetterWidthGS > 0) ? LetterWidthGS : 6;
    const int textX0 = x + GutterW() + marginGS + PosX, py0 = y + BarTopOffset() + marginGS + PosY;
    int l0, c0, l1, c1; NormalizeSel(&l0, &c0, &l1, &c1);
    const int r = IdeHandleRadius(), range = r * 3;
    const int ax = textX0 + IdeColVisual(lines[(size_t)l0], c0) * LW, ay = py0 + l0 * lh + lh + r;
    const int bx = textX0 + IdeColVisual(lines[(size_t)l1], c1) * LW, by = py0 + l1 * lh + lh + r;
    const int da = (mx - ax) * (mx - ax) + (my - ay) * (my - ay), db = (mx - bx) * (mx - bx) + (my - by) * (my - by);
    // solo DEBAJO del renglon (donde cuelga la agarradera): tocar el texto nunca la agarra
    const bool okA = (my >= ay - r) && da <= range * range, okB = (my >= by - r) && db <= range * range;
    if (okB && (!okA || db <= da)) return 2;
    if (okA) return 1;
    return 0;
}
bool IDE::HandleGrab(int mx, int my) {
    const int m = HandleIn(mx, my);
    if (!m) return false;
    // la punta agarrada pasa a ser el CURSOR y la otra el ancla: arrastrar mueve solo esa punta
    int l0, c0, l1, c1; NormalizeSel(&l0, &c0, &l1, &c1);
    if (m == 1) { selLin = l1; selCol = c1; curLin = l0; curCol = c0; }
    else        { selLin = l0; selCol = c0; curLin = l1; curCol = c1; }
    touchGesture = (m == 1) ? 2 : 3; magnifierX = mx; magnifierY = my;
    g_redraw = true;
    return true;
}
void IDE::TouchGestureMove(int mx, int my) {
    if (!touchGesture) return;
    magnifierX = mx; magnifierY = my;
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    int lin = 0, col = 0;
    if (touchGesture == 1) {   // lupa: el cursor donde esta el dedo
        PointToText(mx, my, &lin, &col);
        CursorA(lin, col, false);
        return;
    }
    // agarradera: el dedo esta DEBAJO del renglon (la agarradera cuelga), se mide un renglon mas arriba
    PointToText(mx, my - lh - IdeHandleRadius(), &lin, &col);
    curLin = lin; curCol = col;
    hasSel = !(selLin == curLin && selCol == curCol);
    undoLastOp = 0;
    EnsureCursorVisible();
    g_redraw = true;
}
void IDE::TouchGestureEnd() { touchGesture = 0; g_redraw = true; }

// circulo relleno (las agarraderas)
static void IdeCircle(float cx, float cy, float r, const float* rgba) {
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

static bool gIdePencil = false;   // el lapiz del telefono apretado (barra Copiar | Pegar)
void IDEHeldPencil(bool pressed) { if (gIdePencil != pressed) { gIdePencil = pressed; g_redraw = true; } }

void IDE::button_left() {
    // agarre de la scrollbar (patron Console; en PC el click de contenido entra
    // por LayoutClickUI -> ClickContent, no por aca)
    if (mouseOverScrollY) mouseOverScrollYpress = true;
    if (mouseOverScrollX) mouseOverScrollXpress = true;
}

void IDE::event_mouse_motion(int mx, int my) {
    if (leftMouseDown && selDrag) {
        // arrastrar = extender la seleccion hasta el punto del mouse
        ViewPortClickDown = true;
        const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
        int lx = mx - x, ly = my - y;
        int textX0 = GutterW() + marginGS + PosX;
        int textY0 = BarTopOffset() + marginGS + PosY;
        int lin = (ly - textY0) / lh;
        int col = (lx - textX0 + LetterWidthGS / 2) / (LetterWidthGS > 0 ? LetterWidthGS : 1);
        { const int lc = (lin < 0) ? 0 : (lin >= (int)lines.size() ? (int)lines.size() - 1 : lin);
          col = (col < 0) ? 0 : IdeColByte(lines[lc], col); }
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
void IDE::mouse_button_up(int button) {
    W3D_ONLY_BY_CONTROLS();
    ViewPortClickDown = false;
    if (button == W3dMB_IZQ) {
        selDrag = false;
        mouseOverScrollYpress = false;
        mouseOverScrollXpress = false;
    }
}

void IDE::event_mouse_wheel(float dy, int mx, int my) {
    if (BarScrollHorizontal(mx, my, (int)(dy * 40))) return; // sobre la barra superior
    int step = (int)(dy * 6 * GlobalScale);
    if (LShiftPressed) ScrollByTouch(step, 0); // shift+rueda = horizontal
    else               ScrollByTouch(0, step);
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
void IDE::RecalculateScroll() {
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    int n = (int)lines.size();
    ResizeScrollbar(width, height, lastMaxWidth, -(n * lh + marginGS * 2), BarTopOffset());
}

void IDE::Resize(int newW, int newH) {
    ViewportBase::Resize(newW, newH);
    ResizeBorder(newW, newH);
    RecalculateScroll();
    // achicarse (el teclado del celular se abrio y el IDE quedo en la parte visible): la linea del cursor sigue a la vista
    EnsureCursorVisible();
}

void IDE::EnsureCursorVisible() {
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    int visH = height - BarTopOffset() - marginGS * 2;
    int visW = width - GutterW() - marginGS * 2 - GlobalScale * 9; // colchon de la barra vertical
    int cy = curLin * lh;
    if (cy + PosY < 0) PosY = -cy;
    if (visH > lh && cy + PosY > visH - lh) PosY = visH - lh - cy;
    if (PosY > 0) PosY = 0;
    int cx = IdeColVisual(lines[curLin], curCol) * LetterWidthGS;
    if (cx + PosX < 0) PosX = -cx;
    if (visW > LetterWidthGS && cx + PosX > visW - LetterWidthGS) PosX = visW - LetterWidthGS - cx;
    if (PosX > 0) PosX = 0;
    // el clamp contra MaxPosX/MaxPosY fino lo hace ResizeScrollbar en el Render
}

// ---------------------------------------------------------------------------
//  render
// ---------------------------------------------------------------------------
void IDE::SyncBar() {
    Button* b = BarRoleBtn(BarButtons, BRIDE_Script);
    if (!b) return;
    std::string t = file.empty() ? std::string(T("Script")) : IDEScriptName(file);
    if (dirty) t += " *"; // asterisco = cambios sin guardar
    if (b->text != t) b->text = t;
    Button* c = BarRoleBtn(BarButtons, BRIDE_Compile);
    const bool shader = IdeIsGLSL(file);
    if (c && c->visible != shader) { c->visible = shader; g_redraw = true; }
}
void IDE::CompileShader() {
    if (!IdeIsGLSL(file)) return;
    bool used = false;
    for (size_t i = 0; i < Materials.size() && !used; i++) used = Materials[i] && Materials[i]->shaderFile == file;
    if (used) { if (dirty) Save(); else IdeSavedShader(file); return; }   // (guardar ya compila)
    // nadie lo usa: se prueba lo que hay en el editor
    std::string vs, fs; W3dShaderSplit(GetText(), vs, fs);
    if (!w3dEngine::ShadersWritten()) {   // (sin shaders: solo el @vertex, que es lo que corre aca, en la CPU)
        std::string e;
        W3dVertCPU* p = strstr(vs.c_str(), "Vertex") ? W3dVertCPUCompile(vs, e) : NULL;
        if (p || !strstr(vs.c_str(), "Vertex")) { W3dVertCPUDelete(p); Notify(IDEScriptName(file) + " " + T("compiled"), false); }
        else Notify(std::string(T("The shader does not compile")) + ": @vertex (CPU): " + e, true);
        return;
    }
    char log[2048]; log[0] = 0;
    const int id = w3dEngine::WrittenShaderCompile(vs.c_str(), fs.c_str(), log, (int)sizeof(log));
    if (id > 0) { w3dEngine::WrittenShaderDelete(id); Notify(IDEScriptName(file) + " " + T("compiled"), false); }
    else {
        std::string e = log[0] ? log : T("This graphics backend has no shaders: the material is drawn as usual (GLES 2.0 and Vulkan have them)");
        for (size_t k = 0; k < e.size(); k++) if (e[k] == 10 || e[k] == 13) e[k] = 32;   // (en un renglon)
        Notify(std::string(T("The shader does not compile")) + ": " + e, true);
    }
}

// un rectangulo lleno en coords locales del viewport (sin textura)
static void IdeQuad(float x0, float y0, float x1, float y1, const float* rgba) {
    gfx::Color4fv(rgba);
    float quad[12] = { x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1 };
    gfx::VertexPointer2f(0, quad);
    gfx::DrawTrianglesArray(6);
}

static void IdeText(int px, int py, const std::string& txt, const float* rgba) {
    gfx::Color4fv(rgba);
    gfx::PushMatrix();
    gfx::Translatef((GLfloat)px, (GLfloat)py, 0);
    RenderBitmapText(txt, textAlign::left);
    gfx::PopMatrix();
}

static const float* IdeTypeColor(unsigned char t) {
    switch (t) {
        case 1: return kIdeComment;
        case 2: return kIdeString;
        case 3: return kIdeKeyword;
        case 4: return kIdeNumber;
    }
    return ColorList[static_cast<int>(ColorID::white)];
}

// los bytes [b0, b1) de una linea, por tramos del mismo color del lexer ('tipos' cubre hasta b1); col0 = la columna
// de b0. La x de cada tramo se va sumando: medirla desde el principio de la linea en cada tramo era cuadratico (un
// renglon de 786000 letras de una tabla generada colgaba la PC entera)
static void IdeDrawSegment(const std::string& s, const std::vector<unsigned char>& types, int b0, int b1, int col0,
                            int px, int py, int LW) {
    int i = b0, col = col0;
    while (i < b1) {
        int j = i;
        while (j < b1 && types[(size_t)j] == types[(size_t)i]) j++;
        IdeText(px + col * LW, py, s.substr((size_t)i, (size_t)(j - i)), IdeTypeColor(types[(size_t)i]));
        col += IdeLettersBetween(s, i, j);
        i = j;
    }
}

void IDE::Render() {
    SyncMemory();   // (lo escrito en un script del .w3d -o uno nuevo- ya esta en el proyecto: ver arriba)
    SyncBar();
    const int glY = W3dScreenHeight - y - height;

    // fondo (mismo patron que los demas viewports)
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x, glY, width, height);
    const float* bg = ColorList[static_cast<int>(ColorID::background)];
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
    const int lh = (RowHeightGS > 0) ? RowHeightGS : 12;
    const int LW = (LetterWidthGS > 0) ? LetterWidthGS : 6;
    const int n = (int)lines.size();
    const int gutterW = GutterW();

    // recalcular el scroll SOLO si cambio el contenido (patron Console). El renglon mas largo ya lo midio
    // RecalculateBlocks: recorrer el texto entero en cada cuadro pesa con scripts de varios MB
    int maxWidth = gutterW + marginGS + widthMaxCols * LW + marginGS + GlobalScale * 9;
    if (n != lastLines || maxWidth != lastMaxWidth) {
        lastLines = n; lastMaxWidth = maxWidth;
        RecalculateScroll();
    }

    int hVis = height - top; if (hVis < 0) hVis = 0;
    const int textX0 = gutterW + marginGS + PosX;
    const int py0 = top + marginGS + PosY;
    // RECORTE: los renglones que entran en el alto y, en cada uno, las columnas que entran en el ancho (las tablas
    // que genera un conversor pueden tener renglones de cientos de miles de letras: solo se mide, colorea y dibuja
    // el tramo visible)
    int first = (0 - (py0)) / lh - 1; if (first < 0) first = 0;
    int last     = first + hVis / lh + 2; if (last > n) last = n;
    int colStart = (-PosX - marginGS) / LW - 1; if (colStart < 0) colStart = 0;
    const int colCount = (width - gutterW) / LW + 3;

    // ---- 1) quads: seleccion + caret + separador del gutter (sin textura) ----
    gfx::Scissor(x + gutterW, glY, (width > gutterW) ? width - gutterW : 0, hVis);
    if (hasSel) {
        int l0, c0, l1, c1;
        NormalizeSel(&l0, &c0, &l1, &c1);
        for (int l = (l0 > first ? l0 : first); l <= l1 && l < last; l++) {
            int a = (l == l0) ? c0 : 0;
            int b = (l == l1) ? c1 : (int)lines[l].size();
            int py = py0 + l * lh;
            float x0 = (float)(textX0 + IdeColVisual(lines[l], a) * LW);
            float x1 = (float)(textX0 + IdeColVisual(lines[l], b) * LW);
            if (b <= (int)lines[l].size() && a == b) x1 = x0 + LW * 0.4f; // linea vacia: mostrarla igual
            IdeQuad(x0, (float)py, x1, (float)(py + lh), kIdeSelection);
        }
    }
    { // BUSCAR: las coincidencias a la vista (en el tramo visible de cada renglon), resaltadas (la actual ya se ve
      // como la seleccion)
        const IdeSearch* bq = IDEActiveSearch();
        if (bq && bq->ide == this && !bq->text.empty()) {
            const std::string needle = bq->caps ? bq->text : IdeCollapse(bq->text);
            const int la = (int)needle.size();
            std::vector<int> C;
            for (int l = first; l < last; l++) {
                const std::string& s = lines[(size_t)l];
                int b0, b1;
                IdeSegmentVisible(s, colStart, colCount, &b0, &b1);
                if (b0 >= b1) continue;
                C.clear();
                IdeFindInLine(s, needle, bq->caps, bq->word, (size_t)(b0 - la + 1 > 0 ? b0 - la + 1 : 0), (size_t)b1, &C);
                for (size_t i = 0; i < C.size(); i++) {
                    const int c = (C[i] >= b0) ? colStart + IdeLettersBetween(s, b0, C[i]) : colStart - IdeLettersBetween(s, C[i], b0);
                    const float x0 = (float)(textX0 + c * LW);
                    const float x1 = x0 + (float)(IdeLettersBetween(s, C[i], C[i] + la) * LW);
                    IdeQuad(x0, (float)(py0 + l * lh), x1, (float)(py0 + l * lh + lh), kIdeCoincide);
                }
            }
        }
    }
    { // GUIAS DE SANGRIA: una linea vertical por cada nivel (cada 4 espacios), como los editores de codigo. En las
      // lineas en blanco de adentro de un bloque siguen de largo (toman la sangria menor entre la de arriba y la de abajo)
        const float areaWidth = (GlobalScale > 1) ? (float)GlobalScale * 0.5f : 1.0f;
        // la guia ACTIVA: la del bloque que contiene al cursor (su nivel, en las lineas de alrededor que estan adentro)
        int activeCol = -1, actFrom = 0, actUntil = -1;
        {
            const int indCur = IdeIndentGuide(lines, curLin);
            if (indCur > 0) {
                activeCol = ((indCur - 1) / kIdeTab) * kIdeTab;
                actFrom = curLin; actUntil = curLin;
                while (actFrom > 0 && curLin - actFrom < 2000 && IdeIndentGuide(lines, actFrom - 1) > activeCol) actFrom--;
                while (actUntil + 1 < n && actUntil - curLin < 2000 && IdeIndentGuide(lines, actUntil + 1) > activeCol) actUntil++;
            }
        }
        for (int l = first; l < last; l++) {
            int ind = IdeIndentGuide(lines, l);
            if (ind > colStart + colCount) ind = colStart + colCount;   // (solo las que entran en el ancho)
            const int py = py0 + l * lh;
            for (int c = (colStart / kIdeTab) * kIdeTab; c < ind; c += kIdeTab) {
                const float gx = (float)(textX0 + c * LW);
                const bool active = (c == activeCol && l >= actFrom && l <= actUntil);
                IdeQuad(gx, (float)py, gx + areaWidth, (float)(py + lh), active ? kIdeActiveGuide : kIdeGuide);
            }
        }
    }
    { // caret (sin parpadeo: el render es event-driven). Verde si el foco esta aca.
        int py = py0 + curLin * lh;
        float cx = (float)(textX0 + IdeColVisual(lines[curLin], curCol) * LW);
        // OJO: Scrollable tiene un MIEMBRO viewPortActive que sombrea al global -> '::'
        const float* cc = (::viewPortActive == (ViewportBase*)this)
                              ? ColorList[static_cast<int>(ColorID::accent)]
                              : ColorList[static_cast<int>(ColorID::grayUI)];
        float thickness = (float)((GlobalScale > 0) ? GlobalScale : 1);
        IdeQuad(cx, (float)py, cx + thickness, (float)(py + lh), cc);
    }
    // separador vertical del gutter (a lo alto de todo el contenido)
    gfx::Scissor(x, glY, width, hVis);
    IdeQuad((float)(gutterW - GlobalScale), (float)top, (float)gutterW, (float)height,
            ColorList[static_cast<int>(ColorID::grayLine)]);

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
    for (int l = first; l < last; l++) {
        std::string num = IdeNum(l + 1);
        int nx = gutterW - marginGS - (int)num.size() * LW; // alineado a la derecha
        // la linea del CURSOR se resalta en BLANCO (el resto en gris); asi se ve en que renglon estamos
        IdeText(nx, py0 + l * lh, num, ColorList[static_cast<int>(l == curLin ? ColorID::white : ColorID::grayUI)]);
    }

    // el codigo, por tramos de color del lexer
    gfx::Scissor(x + gutterW, glY, (width > gutterW) ? width - gutterW : 0, hVis);
    if (file.empty() && n == 1 && lines[0].empty()) {
        IdeText(textX0, py0, T("(no .lua scripts or .glsl shaders in the project)"),
                 ColorList[static_cast<int>(ColorID::grayUI)]);
    }
    std::vector<unsigned char> types;
    gIdeLexGLSL = IdeIsGLSL(file);   // (un shader se pinta como GLSL)
    for (int l = first; l < last; l++) {
        const std::string& s = lines[l];
        if (s.empty()) continue;
        int b0, b1;
        IdeSegmentVisible(s, colStart, colCount, &b0, &b1);
        if (b0 >= b1) continue;
        bool blq = (l < (int)blockToStart.size()) ? (blockToStart[l] != 0) : false;
        LexLine(s, &blq, &types, (size_t)b1);   // el color depende de lo anterior del renglon: hasta lo visible
        IdeDrawSegment(s, types, b0, b1, colStart, textX0, py0 + l * lh, LW);
    }

    const float* accent = ColorList[static_cast<int>(ColorID::accent)];
    // AGARRADERAS de la seleccion (tactil): un palito + un circulo colgando de cada punta
    if (hasSel && ToolbarUsesTouch()) {
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        int l0, c0, l1, c1; NormalizeSel(&l0, &c0, &l1, &c1);
        const float r = (float)IdeHandleRadius(), g = (float)((GlobalScale > 0) ? GlobalScale : 1);
        for (int k = 0; k < 2; k++) {
            const int l = k ? l1 : l0, c = k ? c1 : c0;
            const float hx = (float)(textX0 + IdeColVisual(lines[(size_t)l], c) * LW);
            const float hy = (float)(py0 + l * lh + lh);
            IdeQuad(hx - g * 0.5f, hy - (float)lh, hx + g * 0.5f, hy + r, accent);
            IdeCircle(hx, hy + r, r, accent);
        }
        gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Texture2D);
    }
    // LUPA (pulsacion larga / arrastrando una agarradera): el renglon del cursor agrandado, arriba del dedo
    if (touchGesture) {
        const int zoom = 2;
        const int bw = 150 * ((GlobalScale > 0) ? GlobalScale : 1) < width - gapGS * 2 ? 150 * ((GlobalScale > 0) ? GlobalScale : 1) : width - gapGS * 2;
        const int bh = lh * zoom + borderGS * 2;
        int bx = magnifierX - x - bw / 2;
        if (bx < gapGS) bx = gapGS; if (bx + bw > width - gapGS) bx = width - gapGS - bw;
        int by = magnifierY - y - bh - 24 * ((GlobalScale > 0) ? GlobalScale : 1);   // arriba del dedo
        if (by < top) by = magnifierY - y + 24 * ((GlobalScale > 0) ? GlobalScale : 1);   // sin lugar arriba: abajo
        gfx::Scissor(x + bx, W3dScreenHeight - (y + by + bh), bw, bh);
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        IdeQuad((float)bx, (float)by, (float)(bx + bw), (float)(by + bh), ColorList[static_cast<int>(ColorID::headerColor)]);
        const std::string& s = lines[(size_t)curLin];
        const int vis = IdeColVisual(s, curCol);
        const float ox = (float)bx + (float)bw * 0.5f - (float)(vis * LW * zoom);   // el cursor en el centro de la lupa
        const float oy = (float)(by + borderGS);
        { const float cx = ox + (float)(vis * LW * zoom), gz = (float)(zoom * ((GlobalScale > 0) ? GlobalScale : 1));
          IdeQuad(cx, oy, cx + gz, oy + (float)(lh * zoom), accent); }
        gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Texture2D);
        // (solo las letras que entran en la lupa, alrededor del cursor)
        const int colsMagnifier = bw / (LW * zoom) + 3;
        int c0 = vis - colsMagnifier / 2 - 1; if (c0 < 0) c0 = 0;
        int b0, b1;
        IdeSegmentVisible(s, c0, colsMagnifier + 2, &b0, &b1);
        if (b0 < b1) {
            bool blq = (curLin < (int)blockToStart.size()) ? (blockToStart[(size_t)curLin] != 0) : false;
            LexLine(s, &blq, &types, (size_t)b1);
            gfx::PushMatrix();
            gfx::Translatef(ox, oy, 0);
            gfx::Scalef((float)zoom, (float)zoom, 1.0f);
            IdeDrawSegment(s, types, b0, b1, c0, 0, 0, LW);
            gfx::PopMatrix();
        }
        // el borde verde de la lupa
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        { const float g = (float)((GlobalScale > 0) ? GlobalScale : 1);
          IdeQuad((float)bx, (float)by, (float)(bx + bw), (float)by + g, accent);
          IdeQuad((float)bx, (float)(by + bh) - g, (float)(bx + bw), (float)(by + bh), accent);
          IdeQuad((float)bx, (float)by, (float)bx + g, (float)(by + bh), accent);
          IdeQuad((float)(bx + bw) - g, (float)by, (float)(bx + bw), (float)(by + bh), accent); }
        gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Texture2D);
    }
    // TELEFONO: con el lapiz apretado, abajo la barra de las dos softkeys: Copiar (izquierda) | Pegar (derecha)
    if (gIdePencil && ::viewPortActive == (ViewportBase*)this) {
        const int bh = lh + gapGS * 2, by = height - bh;
        gfx::Scissor(x, glY, width, height);
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        IdeQuad(0.0f, (float)by, (float)width, (float)height, ColorList[static_cast<int>(ColorID::headerColor)]);
        IdeQuad(0.0f, (float)by, (float)width, (float)(by + ((GlobalScale > 0) ? GlobalScale : 1)), accent);
        gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Texture2D);
        const std::string left = T("Copy"), right = T("Paste");
        IdeText(marginGS * 2, by + gapGS, left, ColorList[static_cast<int>(ColorID::white)]);
        IdeText(width - marginGS * 2 - IdeColVisual(right, (int)right.size()) * LW, by + gapGS, right, ColorList[static_cast<int>(ColorID::white)]);
    }

    gfx::Disable(gfx::ScissorTest);

    RenderBar();
    DrawBorders(this);
    DrawScrollbar(this);
#ifdef W3D_SYMBIAN
    gfx::EnableArray(gfx::NormalArray); // baseline que asume la escena
#endif
}

// ---------------------------------------------------------------------------
//  captura global (PC): el IDE bajo el foco por hover se queda con el teclado
// ---------------------------------------------------------------------------
static IDE* IdeFocused() {
    if (!viewPortActive || !viewPortActive->isLeaf()) return NULL;
    if (viewPortActive->ViewportKind() != 8) return NULL;
    return (IDE*)viewPortActive;
}

bool IDEKeyboardCapture(int key, bool repeat) {
    IDE* ide = IdeFocused();
    if (!ide) return false;
    return ide->KeyEditor(key, repeat);
}

bool IDETextText(const char* utf8) {
    IDE* ide = IdeFocused();
    if (!ide) return false;
    ide->TextTyped(utf8);
    return true; // enfocado: el texto es del IDE aunque se haya filtrado todo
}

// "Open in IDE" de la BIBLIOTECA (un script elegido en el outliner): el primer IDE del layout lo abre.
// false = no hay ningun IDE en el layout (el usuario tiene que abrir uno).
#include "ViewPorts/LayoutTree.h"
#include "ViewPorts/PopUp/ConfirmPopup.h"   // descartar lo tipeado pide confirmacion
static IDE* FirstIDE(ViewportBase* n) {
    if (!n) return NULL;
    if (n->isLeaf()) return n->ViewportKind() == 8 ? (IDE*)n : NULL;
    IDE* a = NULL;
    if (n->ContainerKind() == 1) { a = FirstIDE(((ViewportRow*)n)->childA); return a ? a : FirstIDE(((ViewportRow*)n)->childB); }
    a = FirstIDE(((ViewportColumn*)n)->childA);
    return a ? a : FirstIDE(((ViewportColumn*)n)->childB);
}
// el script que espera el "Si" del cartel de descartar (el IDE se vuelve a buscar al confirmar: el layout pudo
// cambiar mientras el cartel estaba abierto)
static std::string gIDEOpenPending;
static void IDEOpenConfirmed() {
    IDE* ide = FirstIDE(LayoutCompleteRoot());
    if (ide && !gIDEOpenPending.empty()) ide->OpenFile(gIDEOpenPending);
    gIDEOpenPending.clear();
    g_redraw = true;
}
// un archivo cambio de lugar (el shader de un proyecto sin guardar entro al .w3d al guardarlo): los IDE que lo tienen
// abierto pasan a la ruta nueva, con lo que tenian (el texto es el mismo)
static void IDEMoveIn(ViewportBase* n, const std::string& old, const std::string& newValue) {
    if (!n) return;
    if (n->isLeaf()) {
        if (n->ViewportKind() == 8) {
            IDE* ide = (IDE*)n;
            if (ide->file == old) { ide->file = newValue; ide->SyncBar(); g_redraw = true; }
        }
        return;
    }
    if (n->ContainerKind() == 1) { IDEMoveIn(((ViewportRow*)n)->childA, old, newValue); IDEMoveIn(((ViewportRow*)n)->childB, old, newValue); return; }
    IDEMoveIn(((ViewportColumn*)n)->childA, old, newValue); IDEMoveIn(((ViewportColumn*)n)->childB, old, newValue);
}
void IDEFileMoved(const std::string& old, const std::string& newValue) {
    if (old != newValue) IDEMoveIn(LayoutCompleteRoot(), old, newValue);
}

// GUARDAR un script desde afuera del IDE (el boton Guardar de la tarjeta Recurso): los IDE que lo tienen abierto con
// cambios lo escriben (a su entrada del .w3d o a su archivo). false = ninguno lo tenia con cambios
static bool IDESaveIn(ViewportBase* n, const std::string& path) {
    if (!n) return false;
    if (n->isLeaf()) {
        if (n->ViewportKind() != 8) return false;
        IDE* ide = (IDE*)n;
        return ide->file == path && ide->dirty && ide->Save();
    }
    if (n->ContainerKind() == 1) {
        const bool a = IDESaveIn(((ViewportRow*)n)->childA, path);
        return IDESaveIn(((ViewportRow*)n)->childB, path) || a;
    }
    const bool a = IDESaveIn(((ViewportColumn*)n)->childA, path);
    return IDESaveIn(((ViewportColumn*)n)->childB, path) || a;
}
bool IDESaveScript(const std::string& path) { return IDESaveIn(LayoutCompleteRoot(), path); }

bool IDEOpenScript(const std::string& path) {
    IDE* ide = FirstIDE(LayoutCompleteRoot());
    if (!ide) return false;
    // el MISMO script que ya esta abierto: no se recarga (lo tipeado y su undo se quedan)
    if (ide->file == path) { g_redraw = true; return true; }
    // con cambios sin guardar AVISA antes de descartarlos, como el selector de scripts del propio IDE
    if (ide->dirty) {
        gIDEOpenPending = path;
        if (!confirmPopup) confirmPopup = new ConfirmPopup();
        confirmPopup->Open(std::string(T("Discard the unsaved changes of")) + " " + IDEScriptName(ide->file) + "?",
                              IDEOpenConfirmed);
        return true;
    }
    if (!ide->OpenFile(path)) return false;
    g_redraw = true;
    return true;
}
