// ============================================================================
//  W3dMcp.cpp -- el MCP de Whisk3D: un servidor del Model Context Protocol por la entrada y la salida estandar.
//
//  "whisk3d --mcp" abre el editor como siempre, con su ventana, y ademas atiende pedidos JSON-RPC 2.0 (uno por linea)
//  por stdin; las respuestas salen por stdout. Lo lanza el cliente MCP (Claude Code, Claude Desktop, otro): la IA usa
//  el mismo editor que se ve en la pantalla, y lo que hace queda en el Ctrl+Z como si lo hubiera hecho una persona.
//  Las herramientas son el editor mismo: las acciones de los menus (agregar, importar, abrir, guardar), unas propias
//  (ver la escena, sacar una captura, seleccionar, transformar) y, para todo lo demas, los comandos del harness de
//  pruebas (W3dRunCommand, en W3dScript.cpp).
//
//  Solo en las PC: W3D_MCP lo define el CMakeLists de escritorio. Symbian, Dreamcast, 3DS, Android y la web no
//  compilan nada de esto. En la PC, sin --mcp, cuesta una comparacion por cuadro.
//
//  Los pedidos los lee un hilo (stdin bloquea) y los ejecuta el hilo principal entre cuadros: el contexto de OpenGL y
//  el editor son suyos. Lo que los comandos imprimen con printf iria a stdout y romperia el protocolo: stdout se
//  redirige a un archivo temporal, de donde se lee lo que imprimio cada comando, y el protocolo sale por una copia del
//  stdout original.
// ============================================================================
#ifdef W3D_MCP

#include "test/W3dMcp.h"
#include "test/W3dScript.h"
#include "io/JsonW3d.h"            // JVal (el arbol del JSON)
#include "io/lectura-escritura.h"  // ImportModelByExtension
#include "objects/Objects.h"
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/LayoutInput.h"
#include "WhiskUI/widgets/PopupMenu.h"   // MenuScreenW
#include "undo/Undo.h"
#include "w3dGraphics.h"
#include "w3dVersion.h"
#include <SDL2/SDL.h>
#include <string>
#include <deque>
#include <vector>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
    #include <io.h>
    #include <fcntl.h>
    #define W3D_DUP    _dup
    #define W3D_DUP2   _dup2
    #define W3D_FILENO _fileno
    #define W3D_FDOPEN _fdopen
    #define W3D_LSEEK  _lseek
    #define W3D_READ   _read
    #define W3D_TRUNC  _chsize
#else
    #include <unistd.h>
    #define W3D_DUP    dup
    #define W3D_DUP2   dup2
    #define W3D_FILENO fileno
    #define W3D_FDOPEN fdopen
    #define W3D_LSEEK  lseek
    #define W3D_READ   read
    #define W3D_TRUNC  ftruncate
#endif

extern bool g_redraw;
extern int W3dScreenHeight;
void AddCube(); void AddPlane(); void AddCircle(); void AddUVSphere(); void AddCone(); void AddCylinder();
void AddVertex(); void AddEmpty(); void AddCamera(); void AddLight();

// ============================================================================
//  La entrada y la salida
// ============================================================================
static bool  gActive = false;
static FILE* gProto = NULL;        // el stdout original: por aca sale el protocolo
static int   gFdCapture  = -1;     // lo que queda de stdout: un archivo temporal con lo que imprimen los comandos
static SDL_mutex* gQueue = NULL;
static std::deque<std::string> gRequests;
static SDL_atomic_t gClosed;     // stdin se cerro: el cliente se fue
static SDL_atomic_t gEventType;  // el evento que despierta al loop (que en reposo duerme esperando eventos)

static void Wake() {
    const int t = SDL_AtomicGet(&gEventType);
    if (t <= 0) return;
    SDL_Event e;
    SDL_zero(e);
    e.type = (Uint32)t;
    SDL_PushEvent(&e);
}
static void Enqueue(std::string& line) {
    while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r')) line.erase(line.size() - 1);
    if (line.empty()) return;
    SDL_LockMutex(gQueue);
    gRequests.push_back(line);
    SDL_UnlockMutex(gQueue);
    line.clear();
    Wake();
}
// el hilo lector: una linea por pedido (una linea larga llega en partes)
static int ReadRequests(void*) {
    std::string line;
    char buf[4096];
    while (fgets(buf, sizeof(buf), stdin)) {
        line += buf;
        if (!line.empty() && line[line.size() - 1] == '\n') Enqueue(line);
    }
    Enqueue(line);
    SDL_AtomicSet(&gClosed, 1);
    Wake();
    return 0;
}

bool W3dMcpActive() { return gActive; }

bool W3dMcpStart() {
    fflush(stdout);
    const int fdOut = W3D_FILENO(stdout);
    const int fdProto = W3D_DUP(fdOut);
    if (fdProto < 0) return false;
#ifdef _WIN32
    _setmode(fdProto, _O_BINARY);   // (sin "\r\n": el protocolo es una linea por mensaje)
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    gProto = W3D_FDOPEN(fdProto, "wb");
    FILE* capture = tmpfile();
    if (!gProto || !capture) return false;
    gFdCapture = W3D_FILENO(capture);
    W3D_DUP2(gFdCapture, fdOut);
    gQueue = SDL_CreateMutex();
    SDL_Thread* thread = SDL_CreateThread(ReadRequests, "w3d_mcp", NULL);
    if (!gQueue || !thread) return false;
    SDL_DetachThread(thread);
    gActive = true;
    return true;
}

// lo que imprimieron los comandos desde la ultima vez (y el archivo vuelve a cero)
static std::string ReadCapture() {
    fflush(stdout);
    const long end = (long)W3D_LSEEK(gFdCapture, 0, SEEK_END);
    std::string s;
    if (end > 0) {
        s.resize((size_t)end);
        W3D_LSEEK(gFdCapture, 0, SEEK_SET);
        const long n = (long)W3D_READ(gFdCapture, &s[0], (unsigned)end);
        s.resize(n > 0 ? (size_t)n : 0);
    }
    W3D_TRUNC(gFdCapture, 0);
    W3D_LSEEK(gFdCapture, 0, SEEK_SET);
    return s;
}

// ============================================================================
//  JSON: el lector (completo: todos los escapes, \uXXXX a UTF-8) y lo poco que hace falta para escribir
// ============================================================================
struct ReaderJson {
    const char* p;
    const char* end;
    bool error;
    ReaderJson(const std::string& s) : p(s.data()), end(s.data() + s.size()), error(false) {}
    void Whitespace() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool Eat(char c) { Whitespace(); if (p < end && *p == c) { p++; return true; } error = true; return false; }
    static void Utf8(std::string& s, unsigned c) {
        if (c < 0x80) s += (char)c;
        else if (c < 0x800) { s += (char)(0xC0 | (c >> 6)); s += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { s += (char)(0xE0 | (c >> 12)); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
        else { s += (char)(0xF0 | (c >> 18)); s += (char)(0x80 | ((c >> 12) & 0x3F)); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
    }
    unsigned Hex4() {
        unsigned v = 0;
        for (int i = 0; i < 4; i++) {
            if (p >= end) { error = true; return 0; }
            const char c = *p++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else error = true;
        }
        return v;
    }
    std::string ReadString() {
        std::string s;
        if (!Eat('"')) return s;
        while (p < end && *p != '"') {
            if (*p != '\\') { s += *p++; continue; }
            if (++p >= end) break;
            const char c = *p++;
            switch (c) {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case 'r': s += '\r'; break;
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'u': {
                    unsigned u = Hex4();
                    if (u >= 0xD800 && u < 0xDC00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {   // (par sustituto)
                        p += 2;
                        const unsigned lo = Hex4();
                        u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    Utf8(s, u);
                    break;
                }
                default: s += c; break;   // \" \\ \/
            }
        }
        if (p < end) p++; else error = true;
        return s;
    }
    JVal* Value() {
        Whitespace();
        JVal* v = new JVal();
        if (p >= end) { error = true; return v; }
        if (*p == '{') {
            v->type = 4; p++;
            Whitespace();
            if (p < end && *p == '}') { p++; return v; }
            while (!error) {
                Whitespace();
                const std::string k = ReadString();
                if (!Eat(':')) break;
                JVal*& slot = v->obj[k];
                delete slot;
                slot = Value();
                Whitespace();
                if (p < end && *p == ',') { p++; continue; }
                Eat('}');
                break;
            }
        } else if (*p == '[') {
            v->type = 5; p++;
            Whitespace();
            if (p < end && *p == ']') { p++; return v; }
            while (!error) {
                v->list.push_back(Value());
                Whitespace();
                if (p < end && *p == ',') { p++; continue; }
                Eat(']');
                break;
            }
        } else if (*p == '"') {
            v->type = 2; v->str = ReadString();
        } else if (end - p >= 4 && !strncmp(p, "true", 4)) { v->type = 3; v->b = true; p += 4; }
        else if (end - p >= 5 && !strncmp(p, "false", 5)) { v->type = 3; v->b = false; p += 5; }
        else if (end - p >= 4 && !strncmp(p, "null", 4)) { p += 4; }
        else {
            char nbuf[64];
            size_t n = (size_t)(end - p); if (n > sizeof(nbuf) - 1) n = sizeof(nbuf) - 1;
            memcpy(nbuf, p, n); nbuf[n] = 0;
            char* after = NULL;
            v->num = strtod(nbuf, &after);
            if (after == nbuf) error = true;
            else { v->type = 1; p += after - nbuf; }
        }
        return v;
    }
};
static const JVal* Field(const JVal* o, const char* k) {
    if (!o || o->type != 4) return NULL;
    std::map<std::string, JVal*>::const_iterator it = o->obj.find(k);
    return it == o->obj.end() ? NULL : it->second;
}
static std::string Text(const JVal* v) { return (v && v->type == 2) ? v->str : std::string(); }
static bool Three(const JVal* v, float out[3]) {
    if (!v || v->type != 5 || v->list.size() != 3) return false;
    for (int i = 0; i < 3; i++) { if (v->list[i]->type != 1) return false; out[i] = (float)v->list[i]->num; }
    return true;
}
static std::string Esc(const std::string& s) {
    std::string o = "\"";
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n"; else if (c == '\r') o += "\\r"; else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += (char)c;   // (UTF-8 tal cual: es JSON valido)
    }
    return o + "\"";
}
// el id del pedido, tal cual vino (numero o texto), para la respuesta
static std::string IdText(const JVal* id) {
    if (!id) return "null";
    if (id->type == 2) return Esc(id->str);
    if (id->type == 1) {
        char b[48];
        if (id->num == (double)(long long)id->num) snprintf(b, sizeof(b), "%lld", (long long)id->num);
        else snprintf(b, sizeof(b), "%.17g", id->num);
        return b;
    }
    return "null";
}
static std::string Base64(const std::string& d) {
    static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    o.reserve((d.size() + 2) / 3 * 4);
    for (size_t i = 0; i < d.size(); i += 3) {
        const unsigned v = (unsigned)(unsigned char)d[i] << 16 | (i + 1 < d.size() ? (unsigned)(unsigned char)d[i + 1] << 8 : 0u) |
                           (i + 2 < d.size() ? (unsigned)(unsigned char)d[i + 2] : 0u);
        o += k[(v >> 18) & 63]; o += k[(v >> 12) & 63];
        o += i + 1 < d.size() ? k[(v >> 6) & 63] : '=';
        o += i + 2 < d.size() ? k[v & 63] : '=';
    }
    return o;
}
// ============================================================================
//  La captura en PNG COMPRIMIDO: el PNG del editor (EncodePNG) se guarda sin comprimir, y una captura de la ventana
//  pesaba 6 MB (mas un tercio en base64): demasiado para mandarle a una IA. Aca va achicada y comprimida con deflate
//  (los codigos fijos de Huffman, con busqueda de repeticiones): la interfaz tiene zonas lisas y queda en decenas de KB
// ============================================================================
struct Bits {
    std::string out;
    unsigned acc;
    int n;
    Bits() : acc(0), n(0) {}
    void Set(unsigned v, int count) {   // (el primer bit, el de abajo)
        acc |= v << n; n += count;
        while (n >= 8) { out += (char)(acc & 0xFF); acc >>= 8; n -= 8; }
    }
    void Huffman(unsigned code, int count) {   // (los codigos de Huffman van del bit de arriba)
        unsigned r = 0;
        for (int i = 0; i < count; i++) r |= ((code >> i) & 1u) << (count - 1 - i);
        Set(r, count);
    }
    void Close() { if (n > 0) { out += (char)(acc & 0xFF); acc = 0; n = 0; } }
};
static void Literal(Bits& b, unsigned v) {
    if (v < 144) b.Huffman(0x30 + v, 8);
    else if (v < 256) b.Huffman(0x190 + (v - 144), 9);
    else if (v < 280) b.Huffman(v - 256, 7);
    else b.Huffman(0xC0 + (v - 280), 8);
}
static void Repeat(Bits& b, int length, int dist) {
    static const int baseL[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
    static const int extraL[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
    static const int baseD[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
    static const int extraD[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
    int i = 28;
    while (baseL[i] > length) i--;
    Literal(b, (unsigned)(257 + i));
    if (extraL[i]) b.Set((unsigned)(length - baseL[i]), extraL[i]);
    int j = 29;
    while (baseD[j] > dist) j--;
    b.Huffman((unsigned)j, 5);
    if (extraD[j]) b.Set((unsigned)(dist - baseD[j]), extraD[j]);
}
static std::string Deflate(const std::string& d) {
    const int n = (int)d.size();
    const unsigned char* s = (const unsigned char*)d.data();
    const int kWindow = 32768, kHash = 1 << 15;
    std::vector<int> head(kHash, -1), previous(kWindow, -1);
    Bits b;
    b.Set(1, 1);   // ultimo bloque
    b.Set(1, 2);   // codigos fijos
    int i = 0;
    while (i < n) {
        int best = 0, bestD = 0;
        if (i + 3 <= n) {
            const unsigned h = ((unsigned)s[i] * 2654435761u ^ (unsigned)s[i + 1] * 40503u ^ (unsigned)s[i + 2]) & (kHash - 1);
            int c = head[h];
            for (int turns = 0; c >= 0 && i - c <= kWindow && turns < 32; turns++) {
                int l = 0;
                const int limit = (n - i < 258) ? n - i : 258;
                while (l < limit && s[c + l] == s[i + l]) l++;
                if (l > best) { best = l; bestD = i - c; if (l == limit) break; }
                c = previous[c % kWindow];
            }
            previous[i % kWindow] = head[h];
            head[h] = i;
        }
        if (best >= 3) {
            Repeat(b, best, bestD);
            for (int k = 1; k < best; k++) {   // (los saltados tambien entran a la busqueda)
                const int q = i + k;
                if (q + 3 > n) break;
                const unsigned h = ((unsigned)s[q] * 2654435761u ^ (unsigned)s[q + 1] * 40503u ^ (unsigned)s[q + 2]) & (kHash - 1);
                previous[q % kWindow] = head[h];
                head[h] = q;
            }
            i += best;
        } else Literal(b, s[i++]);
    }
    Literal(b, 256);   // fin del bloque
    b.Close();
    return b.out;
}
static unsigned Crc32(const std::string& d, unsigned crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < d.size(); i++) {
        crc ^= (unsigned char)d[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}
static void Be32(std::string& o, unsigned v) { o += (char)(v >> 24); o += (char)(v >> 16); o += (char)(v >> 8); o += (char)v; }
static void Chunk(std::string& png, const char* type, const std::string& data) {
    Be32(png, (unsigned)data.size());
    const std::string td = std::string(type, 4) + data;
    png += td;
    Be32(png, Crc32(td) ^ 0xFFFFFFFFu);
}
// RGB (filas de arriba hacia abajo) -> PNG. Cada fila con el filtro que deja menos (Sub o Up: la resta con el vecino
// convierte las zonas lisas en ceros, que el deflate junta)
static std::string CompressedPng(const std::vector<unsigned char>& rgb, int w, int h) {
    std::string raw;
    raw.reserve((size_t)(w * 3 + 1) * h);
    const int row = w * 3;
    std::string sub((size_t)row, 0), up((size_t)row, 0);
    for (int y = 0; y < h; y++) {
        const unsigned char* f = &rgb[(size_t)y * row];
        const unsigned char* a = y > 0 ? f - row : NULL;
        long costSub = 0, costUp = 0;
        for (int x = 0; x < row; x++) {
            const unsigned char vs = (unsigned char)(f[x] - (x >= 3 ? f[x - 3] : 0));
            const unsigned char vu = (unsigned char)(f[x] - (a ? a[x] : 0));
            sub[x] = (char)vs; up[x] = (char)vu;
            costSub += vs < 128 ? vs : 256 - vs;
            costUp += vu < 128 ? vu : 256 - vu;
        }
        if (costUp < costSub) { raw += (char)2; raw += up; } else { raw += (char)1; raw += sub; }
    }
    std::string z;
    z += (char)0x78; z += (char)0x01;   // (zlib: deflate, ventana de 32 KB)
    z += Deflate(raw);
    unsigned a1 = 1, b1 = 0;
    for (size_t i = 0; i < raw.size(); i++) { a1 = (a1 + (unsigned char)raw[i]) % 65521u; b1 = (b1 + a1) % 65521u; }
    Be32(z, (b1 << 16) | a1);
    std::string png("\x89PNG\r\n\x1a\n", 8), ihdr;
    Be32(ihdr, (unsigned)w); Be32(ihdr, (unsigned)h);
    ihdr += (char)8; ihdr += (char)2; ihdr += (char)0; ihdr += (char)0; ihdr += (char)0;   // 8 bits, RGB
    Chunk(png, "IHDR", ihdr);
    Chunk(png, "IDAT", z);
    Chunk(png, "IEND", std::string());
    return png;
}

static void Send(const std::string& json) {
    fputs(json.c_str(), gProto);
    fputc('\n', gProto);
    fflush(gProto);
}

// ============================================================================
//  Las herramientas
// ============================================================================
struct Result {
    std::string text;
    std::string imagePng;   // (la captura)
    bool error;
    Result() : error(false) {}
};

static Object* Find(Object* o, const std::string& name) {
    if (!o) return NULL;
    if (o != SceneCollection && o->name == name) return o;
    for (size_t i = 0; i < o->Childrens.size(); i++) if (Object* r = Find(o->Childrens[i], name)) return r;
    return NULL;
}
// LAS COORDENADAS DE LA PERSONA: las del panel Propiedades, con Z para arriba. Adentro el editor tiene Y para arriba:
// el campo "Y" de la posicion es pos.z y el "Z" es pos.y. La rotacion (euler en grados) y la escala van como el panel
static void Tree(Object* o, int prof, std::string& out) {
    char b[512];
    const Vector3& p = o->pos; const Vector3& r = o->rotEuler; const Vector3& e = o->scale;
    snprintf(b, sizeof(b), "%*s- %s (%s) pos %.3g %.3g %.3g | rot %.3g %.3g %.3g | escala %.3g %.3g %.3g%s%s%s\n",
             prof * 2, "", o->name.c_str(), W3dTypeName(o->getType()), p.x, p.z, p.y, r.x, r.y, r.z, e.x, e.y, e.z,
             o->select ? " [seleccionado]" : "", o == ActiveObj ? " [activo]" : "", o->visible ? "" : " [oculto]");
    out += b;
    for (size_t i = 0; i < o->Childrens.size(); i++) Tree(o->Childrens[i], prof + 1, out);
}

static Result ToolScene() {
    Result r;
    if (!SceneCollection) { r.error = true; r.text = "no hay escena"; return r; }
    r.text = "Escena (como el panel Propiedades: Z para arriba, rotacion euler en grados):\n";
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) Tree(SceneCollection->Childrens[i], 0, r.text);
    if (SceneCollection->Childrens.empty()) r.text += "(vacia)\n";
    return r;
}

// la pantalla tal como se ve (o solo la vista 3D activa) a PNG: el mismo dibujo que la captura del harness (uishot),
// achicada (de a bloques enteros, hasta 'widthMax') y comprimida
static Result ToolCapture(bool onlyView3D, int widthMax) {
    Result r;
    if (!rootViewport) { r.error = true; r.text = "no hay layout de viewports"; return r; }
    { extern void LoadPendingTextures(); LoadPendingTextures(); }
    const int W = MenuScreenW, H = W3dScreenHeight;
    if (W < 1 || H < 1) { r.error = true; r.text = "la ventana no tiene tamano"; return r; }
    rootViewport->Render();
    LayoutRenderMenu(W, H);   // (los menus y popups van aparte, encima de todo)
    w3dEngine::Finish();
    int x0 = 0, y0 = 0, w = W, h = H;
    Viewport3D* vp = Viewport3DActive;
    if (onlyView3D && vp && vp->width > 0 && vp->height > 0) { x0 = vp->x; w = vp->width; h = vp->height; y0 = H - vp->y - vp->height; }
    std::vector<unsigned char> px((size_t)w * h * 4, 0);
    w3dEngine::ReadPixelsRGBA(x0, y0, w, h, &px[0]);   // (de abajo hacia arriba)
    if (widthMax < 64) widthMax = 64;
    const int f = (w + widthMax - 1) / widthMax;      // de a cuantos pixeles se promedia
    const int ow = w / f, oh = h / f;
    std::vector<unsigned char> rgb((size_t)ow * oh * 3);
    for (int y = 0; y < oh; y++)
        for (int x = 0; x < ow; x++)
            for (int c = 0; c < 3; c++) {
                unsigned sum = 0;
                for (int j = 0; j < f; j++)
                    for (int i = 0; i < f; i++) sum += px[((size_t)(h - 1 - (y * f + j)) * w + (x * f + i)) * 4 + c];
                rgb[((size_t)y * ow + x) * 3 + c] = (unsigned char)(sum / (unsigned)(f * f));
            }
    r.imagePng = CompressedPng(rgb, ow, oh);
    char b[128];
    snprintf(b, sizeof(b), "%s: %dx%d (la ventana mide %dx%d)", onlyView3D ? "vista 3D" : "window", ow, oh, W, H);
    r.text = b;
    return r;
}

// comandos del harness, uno por linea: corta en el primero que falla. Devuelve lo que imprimieron
static Result ToolCommands(const std::string& text) {
    Result r;
    ReadCapture();   // (lo que haya quedado de antes no es de estos comandos)
    size_t a = 0;
    int n = 0;
    while (a <= text.size()) {
        size_t b = text.find('\n', a);
        if (b == std::string::npos) b = text.size();
        std::string line = text.substr(a, b - a);
        a = b + 1;
        const size_t i0 = line.find_first_not_of(" \t\r");
        if (i0 == std::string::npos) continue;
        const size_t i1 = line.find_last_not_of(" \t\r");
        line = line.substr(i0, i1 - i0 + 1);
        if (line[0] == '#') continue;
        std::string err;
        const bool ok = W3dRunCommand(line, err);
        const std::string printed = ReadCapture();
        r.text += (ok ? "OK    " : "FALLO ") + line + "\n" + printed;
        if (!ok) { r.text += "  -> " + err + "\n"; r.error = true; break; }
        n++;
    }
    if (r.text.empty()) r.text = "(sin comandos)";
    g_redraw = true;
    return r;
}

static Result ToolAdd(const std::string& type) {
    Result r;
    struct { const char* name; void (*fn)(); } table[] = {
        { "cube", AddCube }, { "plane", AddPlane }, { "circle", AddCircle }, { "uvsphere", AddUVSphere },
        { "cone", AddCone }, { "cylinder", AddCylinder }, { "vertex", AddVertex }, { "empty", AddEmpty },
        { "camera", AddCamera }, { "light", AddLight },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (type != table[i].name) continue;
        ReadCapture();
        table[i].fn();
        ReadCapture();
        g_redraw = true;
        r.text = std::string("added: ") + (ActiveObj ? ActiveObj->name : type);
        return r;
    }
    r.error = true;
    r.text  = "tipo desconocido: '" + type + "' (cube, plane, circle, uvsphere, cone, cylinder, vertex, empty, camera, light)";
    return r;
}

static Result ToolSelect(const std::string& name, bool add) {
    Result r;
    Object* o = name.empty() ? NULL : Find(SceneCollection, name);
    if (!name.empty() && !o) { r.error = true; r.text = "no hay ningun objeto llamado '" + name + "'"; return r; }
    UndoCaptureSelection();
    if (!add) DeselectAll();
    if (o) o->Select();
    g_redraw = true;
    r.text = o ? "selected: " + o->name + " (activo)" : std::string("nada seleccionado");
    return r;
}

static Result ToolTransform(const JVal* args) {
    Result r;
    const std::string name = Text(Field(args, "name"));
    Object* o = name.empty() ? ActiveObj : Find(SceneCollection, name);
    if (!o) { r.error = true; r.text = name.empty() ? "no hay objeto activo" : "no hay ningun objeto llamado '" + name + "'"; return r; }
    float p[3], rot[3], e[3];
    const bool hasP = Three(Field(args, "position"), p), hasR = Three(Field(args, "rotation"), rot), hasE = Three(Field(args, "scale"), e);
    if (!hasP && !hasR && !hasE) { r.error = true; r.text = "falta posicion, rotacion o escala (cada una, una lista de 3 numeros)"; return r; }
    UndoTransformStartObj(o);   // (queda en el Ctrl+Z como un transform hecho a mano)
    if (hasP) o->pos = Vector3(p[0], p[2], p[1]);   // (Z para arriba, como el panel: ver Arbol)
    if (hasR) o->SetRotEuler(Vector3(rot[0], rot[1], rot[2]));
    if (hasE) o->scale = Vector3(e[0], e[1], e[2]);
    UndoTransformConfirm();
    { extern bool g_objectsMoved; g_objectsMoved = true; }
    g_redraw = true;
    std::string t;
    Tree(o, 0, t);
    r.text = "ready:\n" + t;
    return r;
}

static Result ToolImport(const std::string& path) {
    Result r;
    if (path.empty()) { r.error = true; r.text = "falta la ruta"; return r; }
    ReadCapture();
    const bool ok = ImportModelByExtension(path);
    r.text = ReadCapture();
    r.text += ok ? "imported: " + path : "no se pudo importar: " + path;
    r.error = !ok;
    g_redraw = true;
    return r;
}

// ---- la lista de herramientas (tools/list) ----
static const char* kTools = R"JSON([
{"name":"escena","description":"El arbol de objetos de la escena de Whisk3D: nombre, tipo, posicion, rotacion (euler en grados), escala, y cual esta seleccionado y cual es el activo. Las coordenadas son las del panel Propiedades: Z para arriba.","inputSchema":{"type":"object","properties":{}}},
{"name":"captura","description":"Una imagen PNG de lo que muestra el editor, para ver el resultado. Por defecto solo la vista 3D activa; con zona='ventana', el editor entero. Se achica hasta ancho_max pixeles (800 si no se dice).","inputSchema":{"type":"object","properties":{"zona":{"type":"string","enum":["vista3d","ventana"]},"ancho_max":{"type":"integer"}}}},
{"name":"agregar_objeto","description":"Agrega un objeto en el cursor 3D, como el menu Add. Queda seleccionado y activo.","inputSchema":{"type":"object","properties":{"tipo":{"type":"string","enum":["cube","plane","circle","uvsphere","cone","cylinder","vertex","empty","camera","light"]}},"required":["tipo"]}},
{"name":"seleccionar","description":"Selecciona un objeto por su nombre y lo deja activo (las herramientas que no reciben nombre actuan sobre el activo). Con agregar=true suma a la seleccion; sin nombre, deselecciona todo.","inputSchema":{"type":"object","properties":{"nombre":{"type":"string"},"agregar":{"type":"boolean"}}}},
{"name":"transformar","description":"Cambia la posicion, la rotacion (euler en grados) o la escala de un objeto (por nombre, o el activo), con las coordenadas del panel Propiedades (Z para arriba). Solo cambia lo que se pasa. Se deshace con Ctrl+Z.","inputSchema":{"type":"object","properties":{"nombre":{"type":"string"},"posicion":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"rotacion":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"escala":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3}}}},
{"name":"borrar_seleccion","description":"Borra los objetos seleccionados (se deshace con Ctrl+Z).","inputSchema":{"type":"object","properties":{}}},
{"name":"deshacer","description":"Deshace la ultima accion (Ctrl+Z), o la rehace con rehacer=true.","inputSchema":{"type":"object","properties":{"rehacer":{"type":"boolean"}}}},
{"name":"abrir_proyecto","description":"Abre un proyecto .w3d (cierra el actual).","inputSchema":{"type":"object","properties":{"ruta":{"type":"string"}},"required":["ruta"]}},
{"name":"guardar_proyecto","description":"Guarda el proyecto en un .w3d.","inputSchema":{"type":"object","properties":{"ruta":{"type":"string"}},"required":["ruta"]}},
{"name":"importar_modelo","description":"Importa un modelo .obj, .fbx, .gltf o .glb, como el menu Add > Import.","inputSchema":{"type":"object","properties":{"ruta":{"type":"string"}},"required":["ruta"]}},
{"name":"comando","description":"Para todo lo demas: comandos del editor, uno por linea (los del harness de pruebas de Whisk3D, main/test/W3dScript.cpp; hay mas de 600). Corta en el primero que falla y devuelve lo que imprimieron. Algunos: mode <object|edit> | select <all|none> | selmode <vertex|edge|face> | extrude <dist> | loopcut <arista> <cortes> <factor> | merge <center|bydistance> | triangulate | separate | duplicate | join | shade <smooth|flat> | apply <location|rotation|scale|all> | objname <nombre> | parent <hijo> <padre> | modadd <screw|mirror|array|subsurf|boolean> | modapply | matnew | matcolor <r> <g> <b> (0 a 1) | newtex <nombre> <ancho> <alto> <blank|uvgrid|colorgrid> | unwrap <angle|conformal> | smartuv | addarmature | timeline <play|pause|goto N> | frame N | export <ruta.obj> | exportgltf <ruta> | exportglb <ruta> | rtrender <ancho> <alto> | xray <0|1> | undo | redo.","inputSchema":{"type":"object","properties":{"comandos":{"type":"string"}},"required":["comandos"]}}
])JSON";

static Result Call(const std::string& name, const JVal* args) {
    if (name == "escena") return ToolScene();
    if (name == "captura") {
        const JVal* am = Field(args, "width_max");
        return ToolCapture(Text(Field(args, "zone")) != "window", (am && am->type == 1) ? (int)am->num : 800);
    }
    if (name == "agregar_objeto") return ToolAdd(Text(Field(args, "type")));
    if (name == "seleccionar") {
        const JVal* ag = Field(args, "add");
        return ToolSelect(Text(Field(args, "name")), ag && ag->type == 3 && ag->b);
    }
    if (name == "transformar") return ToolTransform(args);
    if (name == "borrar_seleccion") return ToolCommands("delete");
    if (name == "deshacer") {
        const JVal* re = Field(args, "redo");
        return ToolCommands((re && re->type == 3 && re->b) ? "redo" : "undo");
    }
    if (name == "abrir_proyecto") return ToolCommands("openw3d " + Text(Field(args, "path")));
    if (name == "guardar_proyecto") return ToolCommands("savew3d " + Text(Field(args, "path")));
    if (name == "importar_modelo") return ToolImport(Text(Field(args, "path")));
    if (name == "comando") return ToolCommands(Text(Field(args, "commands")));
    Result r;
    r.error = true;
    r.text  = "herramienta desconocida: '" + name + "'";
    return r;
}

// ============================================================================
//  El protocolo
// ============================================================================
static void Handle(const std::string& line) {
    ReaderJson lj(line);
    JVal* msj = lj.Value();
    if (lj.error || msj->type != 4) {
        Send("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"JSON invalido\"}}");
        delete msj;
        return;
    }
    const std::string method = Text(Field(msj, "method"));
    const JVal* idv = Field(msj, "id");
    const std::string id = IdText(idv);
    const JVal* params = Field(msj, "params");
    if (!idv) { delete msj; return; }   // una notificacion (notifications/initialized, cancelled...): sin respuesta
    std::string res;
    if (method == "initialize") {
        // la version que pide el cliente, si es una de estas; si no, la ultima
        std::string show = Text(Field(params, "protocolVersion"));
        if (show != "2024-11-05" && show != "2025-03-26" && show != "2025-06-18") show = "2025-06-18";
        res = "{\"protocolVersion\":" + Esc(show) + ",\"capabilities\":{\"tools\":{\"listChanged\":false}},"
              "\"serverInfo\":{\"name\":\"whisk3d\",\"version\":" + Esc(W3dVersion()) + "},"
              "\"instructions\":" + Esc("Whisk3D es un editor 3D (modelado, materiales, animacion, juegos). Lo que hagas se ve "
                                        "en la ventana del editor y se deshace con Ctrl+Z como si lo hubiera hecho una persona. "
                                        "Usa 'escena' para saber que hay y 'captura' para ver el resultado.") + "}";
    } else if (method == "ping") {
        res = "{}";
    } else if (method == "tools/list") {
        // (la lista esta escrita en varias lineas para leerla aca; el protocolo es un mensaje por linea)
        std::string list;
        for (const char* c = kTools; *c; c++) if (*c != '\n' && *c != '\r') list += *c;
        res = "{\"tools\":" + list + "}";
    } else if (method == "tools/call") {
        const Result r = Call(Text(Field(params, "name")), Field(params, "arguments"));
        res = "{\"content\":[";
        if (!r.imagePng.empty()) res += "{\"type\":\"image\",\"data\":\"" + Base64(r.imagePng) + "\",\"mimeType\":\"image/png\"},";
        res += "{\"type\":\"text\",\"text\":" + Esc(r.text) + "}],\"isError\":" + (r.error ? "true" : "false") + "}";
    } else {
        Send("{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"error\":{\"code\":-32601,\"message\":" + Esc("metodo desconocido: " + method) + "}}");
        delete msj;
        return;
    }
    Send("{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + res + "}");
    delete msj;
}

bool W3dMcpHandle() {
    if (!gActive) return true;
    if (SDL_AtomicGet(&gEventType) == 0) {
        const Uint32 t = SDL_RegisterEvents(1);
        SDL_AtomicSet(&gEventType, t == (Uint32)-1 ? -1 : (int)t);
    }
    for (;;) {
        std::string p;
        SDL_LockMutex(gQueue);
        if (!gRequests.empty()) { p = gRequests.front(); gRequests.pop_front(); }
        SDL_UnlockMutex(gQueue);
        if (p.empty()) break;
        Handle(p);
    }
    return SDL_AtomicGet(&gClosed) == 0;
}

#endif // W3D_MCP
