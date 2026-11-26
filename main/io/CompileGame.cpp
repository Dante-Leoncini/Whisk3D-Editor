// ============================================================================
//  CompileGame.cpp — ver CompileGame.h. Genera el proyecto del juego
//  (runtime standalone) y lo compila para la plataforma elegida.
// ============================================================================
#include "W3dLang.h"   // T(): los avisos salen en el idioma del editor
#include "io/CompileGame.h"
#include "io/UI2DFormat.h"
#include "io/SaveW3D.h"            // g_projIcon: el icono del juego (ruta externa)
#include "io/SaveVersion.h"        // SaveVersionCollectFrom: rutas reales que referencian las escenas
#include "io/W3dContainer.h"      // FORMATO v4: el staging es un espejo del contenedor
#include "io/W3dZip.h"            // W3dZipReader: volcar las entradas al staging
#include "io/LuaCompile.h"           // produccion: .lua del staging -> bytecode stripped
#include "io/Libraries.h"            // las LIBRERIAS externas que usa el proyecto viajan con el juego
#include "io/W3dStore.h"             // W3dLibPathPacked: donde las busca el runtime
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Gamepad.h"
#include "script/W3dScript.h"
#include "W3dScene.h"                // escena inicial (multi-escena) al compilar
#include "W3dRoots.h"                // las raices del proyecto: hay un juego, con cual arranca
#include "ViewPorts/Notifications.h"
#include "render/RenderOptions.h"    // g_redraw: mantener vivo el render mientras compila
#include "W3dDock.h"                 // apagar la barra del icono del dock al terminar el build
#include "variables.h"               // cfg.repoPath: raiz del repo fijada a mano (editor instalado)
#include "w3dFilesystem.h"
#include "w3dTexture.h"               // DecodeImage/FreeImage: leer el PNG del icono
#include "w3dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set>
#include <string>
#include <vector>
#include <atomic>
// hilos: en el editor web (emscripten sin -pthread) <thread>/<mutex> ni compilan; ahi el
// "worker" corre sincrono (compilar juegos desde el editor web ya no andaba: sin system()).
#ifndef __EMSCRIPTEN__
#include <thread>
#include <mutex>
#endif
// popen/lectura con timeout (el worker lee la salida de cmake/emcc/gradle EN VIVO)
#ifdef _WIN32
#define W3D_POPEN  _popen
#define W3D_PCLOSE _pclose
#include <direct.h>   // _rmdir
#define W3D_RMDIR  _rmdir
#else
#define W3D_POPEN  popen
#define W3D_PCLOSE pclose
#define W3D_RMDIR  rmdir
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/stat.h>   // mkdir (MakeDirs)
#endif

// ============================================================================
//  EL SHELL Y LAS RUTAS DEL PROYECTO. Los comandos de abajo (cp, rm) ponen las rutas entre comillas dobles, y adentro
//  de ellas el shell todavia interpreta $ ` " \ . Muchas rutas salen del PROYECTO (nombres de entrada del contenedor,
//  rutas de un .w3dui, literales de los scripts), y un proyecto puede venir de internet: una ruta con "$(...)"
//  ejecutaba lo que quisiera al compilar el juego, y una con ".." leia o escribia afuera de la carpeta del juego.
// ============================================================================
// la ruta se puede poner entre comillas dobles en un comando del shell
static bool ShellSafe(const std::string& s) {
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 32 || c == 127 || c == '$' || c == '`' || c == '"' || c == '\\') return false;
    }
    return true;
}
// una ruta RELATIVA del proyecto que queda adentro de su carpeta: ni absoluta ni con un ".." que suba
static bool SafeRelPath(const std::string& r) {
    if (r.empty() || r[0] == '/' || (r.size() > 1 && r[1] == ':') || !ShellSafe(r)) return false;
    for (size_t p = 0; p <= r.size();) {
        size_t q = r.find('/', p);
        if (q == std::string::npos) q = r.size();
        if (q - p == 2 && r.compare(p, 2, "..") == 0) return false;
        p = q + 1;
    }
    return true;
}
// corre un comando armado con estas rutas; si alguna no es ShellSafe no lo corre (-1)
static int ShellRun(const char* cmd, const std::string& a, const std::string& b = std::string(),
                    const std::string& c = std::string(), const std::string& d = std::string()) {
    if (!ShellSafe(a) || !ShellSafe(b) || !ShellSafe(c) || !ShellSafe(d)) {
        w3dLogfW("CompileGame: una ruta con caracteres que el shell interpreta: no se usa (%s)", cmd);
        return -1;
    }
    return system(cmd);
}
// la carpeta y las de arriba (mkdir -p sin el shell)
static void MakeDirs(const std::string& dir) {
    for (size_t i = 1; i <= dir.size(); i++)
        if (i == dir.size() || dir[i] == '/') {
            const std::string p = dir.substr(0, i);
#ifdef _WIN32
            _mkdir(p.c_str());
#else
            mkdir(p.c_str(), 0755);
#endif
        }
}

// ============================================================================
//  BUILD ASINCRONICO — reparto estricto de hilos:
//   - hilo PRINCIPAL (UI): pasos [1/4] y [2/4] (recolectar scripts, exportar los
//     .w3dui, escribir main.cpp/CMake/scripts, .gitignore): es lo UNICO que toca
//     la escena. Despues cada frame lee este estado (CompileGameProgress) y
//     dibuja la barra; al terminar hace join + Notificar (CompileGameTick).
//   - hilo WORKER: pasos [3/4] y [4/4] (system/popen de cmake/emcc/ndk/gradle y
//     la copia final a build/<plat>): SOLO procesos externos y filesystem. Nunca
//     llama a la UI ni toca la escena: publica etapa/porcentaje aca.
//  Proteccion minima: atomics para flags/porcentaje, un mutex para los strings.
// ============================================================================
// cuantas unidades de compilacion en paralelo (ver el bloque de CompileGame.h).
// hardware_concurrency es de <thread> (C++11) y devuelve 0 si no puede averiguarlo.
int W3dCompileJobs() {
    int n = 0;
#ifndef __EMSCRIPTEN__
    n = (int)std::thread::hardware_concurrency();
#endif
    if (n <= 0) n = 2;          // no se pudo averiguar: lo conservador
    int j = n / 2;              // la MITAD de los nucleos
    if (j < 1) j = 1;
    if (j > 8) j = 8;           // tope: mas no acelera y si multiplica la RAM
    // ESCAPE HATCH para maquinas con poca RAM (y para la suite de tests, que corre
    // builds de juego al lado de otras cosas): W3D_COMPILE_JOBS BAJA el paralelismo.
    // Solo puede bajarlo: nadie puede usar esta variable para volver al -j pelado.
    const char* env = getenv("W3D_COMPILE_JOBS");
    if (env && *env) {
        int e = atoi(env);
        if (e >= 1 && e < j) j = e;
    }
    return j;
}
std::string W3dCompileJobsFlag() {
    char b[16]; snprintf(b, sizeof(b), "-j%d", W3dCompileJobs());
    return std::string(b);
}

static std::atomic<bool> g_bldActive(false);  // hay un build corriendo (bloquea re-disparo)
static std::atomic<bool> g_bldEnded(false);   // el worker termino: el Tick hace join + notifica
static std::atomic<bool> g_bldOk(false);      // resultado final del worker
static std::atomic<int>  g_bldPct(0);         // avance GLOBAL 0..100 (solo lo escribe el worker)
static std::string       g_bldStage;          // texto de la etapa actual (protegido por el mutex)
static std::string       g_bldFinal;          // mensaje final para Notificar (idem)
#ifndef __EMSCRIPTEN__
static std::mutex        g_bldMx;
static std::thread*      g_bldThread = NULL;     // puntero: si el editor se cierra con un build
                                               // corriendo NO corre el dtor de un thread joinable
                                               // (std::terminate); el proceso se va y listo.
#define BLD_LOCK() std::lock_guard<std::mutex> _bl(g_bldMx)
#else
#define BLD_LOCK() ((void)0)                   // web: sin hilos, no hay carrera posible
#endif

bool CompileGameInProgress() { return g_bldActive.load(); }

int CompileGameProgress(std::string* stage) {
    if (stage) { BLD_LOCK(); *stage = g_bldStage; }
    return g_bldPct.load();
}

// publica etapa + porcentaje (la llama el worker). El % nunca retrocede.
static void BldPublish(const std::string& stage, int pct) {
    { BLD_LOCK(); g_bldStage = stage; }
    int cur = g_bldPct.load();
    if (pct > cur) g_bldPct.store(pct);
}

// un texto con UN %s, con el formato YA TRADUCIDO (BldFmt(T("Compiling (%s)"), "emcc")): las etapas de la barra
// y los avisos del build salen en el idioma del editor. El buffer se mide con el formato y el argumento: en
// cirilico una traduccion ocupa el doble de bytes y las rutas no tienen cota.
static std::string BldFmt(const char* fmt, const std::string& a) {
    std::vector<char> b(strlen(fmt) + a.size() + 64);
    snprintf(&b[0], b.size(), fmt, a.c_str());
    return std::string(&b[0]);
}

// modos de progreso de BldRun (que hacer con cada linea de salida del proceso)
enum {
    BLD_CREEP  = 0,  // sin % real (emcc/cpack): la barra avanza DE A POCO por linea/tiempo,
                     // capada al tope de la franja (progreso "indeterminado" honesto)
    BLD_CMAKE = 1,  // parsea el "[ NN%]" que escribe cmake --build y lo mapea a la franja
    BLD_MARKS = 2   // parsea "[W3D] <pct> <texto>" (las emite el script Android generado)
};

// procesa UNA linea de salida: la vuelca al build.log y actualiza etapa/% segun el modo.
// Corre SOLO en el worker ('nCreep' es el contador local de BldRun).
static void BldLine(const std::string& s, FILE* log, int mode, int pctStart, int pctEnd,
                     int cap, int* nCreep) {
    if (log) { fputs(s.c_str(), log); fputc('\n', log); fflush(log); }
    if (mode == BLD_CMAKE) {
        // "[ 45%] Building CXX ..." -> mapear NN a la franja pctStart..pctEnd
        size_t a = s.find('[');
        size_t b = (a == std::string::npos) ? std::string::npos : s.find("%]", a);
        if (a != std::string::npos && b != std::string::npos && b > a + 1) {
            int nn = atoi(s.substr(a + 1, b - a - 1).c_str());
            if (nn >= 0 && nn <= 100) {
                int pct = pctStart + (int)((pctEnd - pctStart) * (nn / 100.0f));
                int cur = g_bldPct.load();
                if (pct > cur) g_bldPct.store(pct);
            }
        }
    } else if (mode == BLD_MARKS && s.compare(0, 6, "[W3D] ") == 0) {
        // marca del script generado: "[W3D] <pct> <texto de la etapa>" (ej "ndk-build (arm64-v8a)"). El texto
        // es la CLAVE en ingles: sale traducido (lo tecnico, como "ndk-build ($ABIS)", no esta y sale tal cual)
        // ("<clave>|<arg>": una clave con un %s, como "Compiling (%s)|PSP": sale traducida con el argumento adentro)
        char* end = NULL;
        long pct = strtol(s.c_str() + 6, &end, 10);
        if (end && *end == ' ' && pct >= 0 && pct <= 100) {
            const std::string txt(end + 1);
            const size_t bar = txt.find('|');
            if (bar == std::string::npos) BldPublish(T(txt.c_str()), (int)pct);
            else BldPublish(BldFmt(T(txt.substr(0, bar).c_str()), txt.substr(bar + 1)), (int)pct);
        }
    } else {
        // sin % real: cada linea (BLD_CREEP) o cada 5 (BLD_MARKS: ndk/gradle imprimen muchas)
        // empuja la barra 1 punto hasta el tope de la franja
        int step = (mode == BLD_MARKS) ? 5 : 1;
        if (++(*nCreep) >= step) {
            *nCreep = 0;
            int cur = g_bldPct.load();
            if (cur < cap) g_bldPct.store(cur + 1);
        }
    }
}

// corre 'cmd' EN EL WORKER via popen (stdout+stderr juntos), vuelca la salida al
// build.log en vivo y actualiza el estado compartido segun 'modo' dentro de la franja
// pctStart..pctEnd. En POSIX lee con select() + timeout: si el proceso esta callado
// (emcc/gradle no imprimen %) la barra igual avanza lento (~1 punto/seg, capada).
// Devuelve el codigo de salida crudo (0 = ok) o -1 si no se pudo lanzar.
static int BldRun(const std::string& cmd, FILE* log, int mode, int pctStart, int pctEnd) {
    std::string sh = "(" + cmd + ") 2>&1";
    FILE* p = W3D_POPEN(sh.c_str(), "r");
    if (!p) return -1;
    int cur0 = g_bldPct.load();
    if (pctStart > cur0) g_bldPct.store(pctStart);           // arranque de la franja
    int cap = (pctEnd > pctStart) ? pctEnd - 1 : pctStart;   // tope del creep (el final lo da el exito)
    int nCreep = 0;
    std::string line;
#ifndef _WIN32
    int fd = fileno(p);
    char buf[1024];
    int still = 0;   // timeouts seguidos sin salida (creep temporal)
    bool eof = false;
    while (!eof) {
        fd_set rf; FD_ZERO(&rf); FD_SET(fd, &rf);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;   // 0.5 s
        int r = select(fd + 1, &rf, NULL, NULL, &tv);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (r == 0) {
            // proceso callado: avance lento e "indeterminado" (~1 punto/seg), capado.
            // En BLD_CMAKE no: ahi el % es real y no se inventa avance.
            if (mode != BLD_CMAKE && ++still >= 2) {
                still   = 0;
                int cur = g_bldPct.load();
                if (cur < cap) g_bldPct.store(cur + 1);
            }
            continue;
        }
        still = 0;
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) { eof = true; }
        else for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') { BldLine(line, log, mode, pctStart, pctEnd, cap, &nCreep); line.clear(); }
            else if (buf[i] != '\r') line += buf[i];
        }
    }
#else
    // Windows: sin select() sobre pipes -> lectura bloqueante linea a linea (el creep va
    // por lineas; si el proceso calla la barra queda quieta, que es lo honesto sin datos)
    char lbuf[1024];
    while (fgets(lbuf, sizeof(lbuf), p)) {
        std::string s(lbuf);
        while (!s.empty() && (s[s.size()-1] == '\n' || s[s.size()-1] == '\r')) s.erase(s.size()-1);
        BldLine(s, log, mode, pctStart, pctEnd, cap, &nCreep);
    }
#endif
    if (!line.empty()) BldLine(line, log, mode, pctStart, pctEnd, cap, &nCreep);
    return W3D_PCLOSE(p);
}

// ---- lo que el proyecto referencia: los objetos con script + sus refs -------
// (inUI: el objeto vive adentro de una UI -la raiz o un elemento 2D-: sus scripts viajan DENTRO del .w3dui y los carga
//  UI2DLoad; agregarlos otra vez con W3dGameScript los duplicaba: el start() de la consola corria dos veces)
struct ObjScript { std::string name; std::vector<W3dScriptEntry> scripts; bool inUI; };

// ---- una ESCENA a compilar: su archivo .w3dui (relativo a platform-build/<plat>) + su nombre ----
struct SceneGen { std::string file; std::string name; };

static void CollectScripts(Object* o, std::vector<ObjScript>* out,
                              std::vector<std::string>* pathsLua, bool inUI = false) {
    if (!o) return;
    if (o->getType() == ObjectType::ui) inUI = true;
    if (o->scriptData && !o->scriptData->scripts.empty()) {
        ObjScript os; os.name = o->name; os.inUI = inUI;
        for (size_t i = 0; i < o->scriptData->scripts.size(); i++) {
            os.scripts.push_back(o->scriptData->scripts[i]);
            if (!o->scriptData->scripts[i].path.empty())
                pathsLua->push_back(o->scriptData->scripts[i].path);
        }
        out->push_back(os);
    }
    // (lo que genera un PROXY es de su LIBRERIA: sus scripts son entradas de ella -"lib:<libreria>/..."- y viajan con
    //  ella en libs/<nombre>/, no sueltos en el staging)
    if (o->getType() == ObjectType::proxy) return;
    for (size_t i = 0; i < o->Childrens.size(); i++)
        CollectScripts(o->Childrens[i], out, pathsLua, inUI);
}

static std::string Base(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    return (s == std::string::npos) ? path : path.substr(s + 1);
}
static std::string Folder(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : path.substr(0, s);
}
// escapa comillas para meterlo en un string de C
static std::string Esc(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) { if (s[i] == '"' || s[i] == '\\') r += '\\'; r += s[i]; }
    return r;
}

// un nombre de escena -> nombre de archivo seguro ("Mi Menu" -> "Mi_Menu"). Solo se usa
// para el .w3dui (el nombre REAL de la escena queda dentro, en su campo "nombre").
static std::string Slug(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-') r += c;
        else r += '_';
    }
    if (r.empty()) r = "scene";
    return r;
}

static std::string BaseNoExt(const std::string& path) {
    std::string b = Base(path);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}

// ============================================================================
//  ICONO del juego: del PNG elegido en el editor (g_projIcon, maxima definicion)
//  se generan aca los tamanos chicos que consumen los builds:
//    icons/<n>x<n>.png        hicolor del .deb (16..256, sin agrandar el original)
//    icons/android-<dpi>.png  mipmaps del APK (48/72/96/144/192)
//    icono.png                el mas grande generado: SDL_SetWindowIcon del main
//                             (viaja con los demas assets por los globs *.png)
//
//  ESCRITURA del PNG: el EncodePNG del Core descarta el alpha A PROPOSITO (es para
//  renders solidos) y el icono NECESITA el alpha; vendorear stb_image_write es mas
//  codigo que esto. Asi que: mismo esquema que el Core (deflate "stored" + CRC32 +
//  Adler32, portable y sin dependencias) pero color type 6 (RGBA). Editor-only.
// ============================================================================
static unsigned int IcCrc(unsigned int crc, const unsigned char* d, size_t n) {
    for (size_t i = 0; i < n; i++) {
        crc ^= d[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}
static void IcChunk(std::vector<unsigned char>& out, const char* type,
                    const unsigned char* d, size_t n) {
    unsigned char len4[4] = { (unsigned char)(n >> 24), (unsigned char)(n >> 16),
                              (unsigned char)(n >> 8),  (unsigned char)n };
    out.insert(out.end(), len4, len4 + 4);
    size_t start = out.size();
    out.insert(out.end(), (const unsigned char*)type, (const unsigned char*)type + 4);
    if (n) out.insert(out.end(), d, d + n);
    unsigned int crc = IcCrc(0xFFFFFFFFu, &out[start], out.size() - start) ^ 0xFFFFFFFFu;
    unsigned char c4[4] = { (unsigned char)(crc >> 24), (unsigned char)(crc >> 16),
                            (unsigned char)(crc >> 8),  (unsigned char)crc };
    out.insert(out.end(), c4, c4 + 4);
}
// escribe pixeles RGBA8 como PNG RGBA (CON alpha) en 'ruta'. false si no pudo.
static bool IcSavePNG(const std::string& path, const unsigned char* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return false;
    const size_t row = (size_t)w * 4;
    std::vector<unsigned char> raw((size_t)h * (1 + row));
    for (int y = 0; y < h; y++) {
        unsigned char* d = &raw[(size_t)y * (1 + row)];
        d[0] = 0;   // filtro None
        memcpy(d + 1, rgba + (size_t)y * row, row);
    }
    // IDAT = zlib con bloques "stored" (BTYPE=00) + adler32 (igual que el Core)
    std::vector<unsigned char> idat;
    idat.push_back(0x78); idat.push_back(0x01);
    size_t off = 0, tot = raw.size();
    while (off < tot) {
        size_t n = tot - off; if (n > 65535) n = 65535;
        idat.push_back((unsigned char)((off + n >= tot) ? 1 : 0));
        idat.push_back((unsigned char)(n & 255)); idat.push_back((unsigned char)((n >> 8) & 255));
        unsigned int nn = (~(unsigned int)n) & 0xFFFF;
        idat.push_back((unsigned char)(nn & 255)); idat.push_back((unsigned char)((nn >> 8) & 255));
        idat.insert(idat.end(), raw.begin() + off, raw.begin() + off + n);
        off += n;
    }
    unsigned int a = 1, b = 0;
    for (size_t i = 0; i < tot; i++) { a = (a + raw[i]) % 65521u; b = (b + a) % 65521u; }
    unsigned int ad = (b << 16) | a;
    idat.push_back((unsigned char)(ad >> 24)); idat.push_back((unsigned char)(ad >> 16));
    idat.push_back((unsigned char)(ad >> 8));  idat.push_back((unsigned char)ad);
    unsigned char ihdr[13] = {
        (unsigned char)(w >> 24), (unsigned char)(w >> 16), (unsigned char)(w >> 8), (unsigned char)w,
        (unsigned char)(h >> 24), (unsigned char)(h >> 16), (unsigned char)(h >> 8), (unsigned char)h,
        8, 6, 0, 0, 0 };   // 8 bits, color type 6 = RGBA, sin interlace
    std::vector<unsigned char> png;
    static const unsigned char signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    png.insert(png.end(), signature, signature + 8);
    IcChunk(png, "IHDR", ihdr, 13);
    IcChunk(png, "IDAT", &idat[0], idat.size());
    IcChunk(png, "IEND", 0, 0);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(&png[0], 1, png.size(), f);
    // ferror + fclose: el flush pasa al cerrar, y un icono chico entra entero en el buffer de
    // stdio -> sin esto se devolvia true con el PNG a medio escribir
    bool ok = (ferror(f) == 0);
    if (fclose(f) != 0) ok = false;
    return ok;
}
// escala el icono a n x n: encaja el lado mayor y CENTRA con transparencia (el icono
// final es cuadrado siempre). Box filter PREMULTIPLICADO: los pixeles transparentes
// no "manchan" el color del borde. Para un icono alcanza, sin stb_image_resize.
static void IcScale(const unsigned char* src, int sw, int sh, unsigned char* dst, int n) {
    memset(dst, 0, (size_t)n * n * 4);
    int dw, dh;
    if (sw >= sh) { dw = n; dh = (int)((long long)sh * n / sw); if (dh < 1) dh = 1; }
    else          { dh = n; dw = (int)((long long)sw * n / sh); if (dw < 1) dw = 1; }
    int offx = (n - dw) / 2, offy = (n - dh) / 2;
    for (int y = 0; y < dh; y++) {
        int y0 = (int)((long long)y * sh / dh), y1 = (int)((long long)(y + 1) * sh / dh);
        if (y1 <= y0) y1 = y0 + 1; if (y1 > sh) y1 = sh;
        for (int x = 0; x < dw; x++) {
            int x0 = (int)((long long)x * sw / dw), x1 = (int)((long long)(x + 1) * sw / dw);
            if (x1 <= x0) x1 = x0 + 1; if (x1 > sw) x1 = sw;
            unsigned long long r = 0, g = 0, b = 0, to = 0;
            long long cnt = (long long)(y1 - y0) * (x1 - x0);
            for (int yy = y0; yy < y1; yy++) {
                const unsigned char* p = src + ((size_t)yy * sw + x0) * 4;
                for (int xx = x0; xx < x1; xx++, p += 4) {
                    unsigned int pa = p[3];
                    r += (unsigned long long)p[0] * pa;
                    g += (unsigned long long)p[1] * pa;
                    b += (unsigned long long)p[2] * pa;
                    to += pa;
                }
            }
            unsigned char* d = dst + ((size_t)(y + offy) * n + (x + offx)) * 4;
            if (to) { d[0] = (unsigned char)(r / to); d[1] = (unsigned char)(g / to);
                      d[2] = (unsigned char)(b / to); }
            d[3] = (unsigned char)(to / cnt);
        }
    }
}
// genera todos los tamanos en <out>/icons/ + <out>/icono.png. 'linuxSize' recibe los
// tamanos hicolor que SI se generaron (no se agranda el original, salvo que sea mas
// chico que 16). false si el PNG no se pudo leer (se compila sin icono, como antes).
static bool GenerateIcons(const std::string& pngPath, const std::string& out,
                          std::vector<int>* linuxSize) {
    unsigned char* src = 0; int sw = 0, sh = 0;
    if (!w3dEngine::DecodeImage(pngPath.c_str(), &src, &sw, &sh) || !src || sw < 1 || sh < 1)
        return false;
    MakeDirs(out + "/icons");
    std::vector<unsigned char> buf;
    // hicolor (linux): solo hasta el tamano del original (no agrandar); si el
    // original es mas chico que 16 se genera igual el de 16 (que haya al menos uno)
    static const int size[6] = { 16, 32, 48, 64, 128, 256 };
    int greater = 0;
    for (int i = 0; i < 6; i++) {
        int n = size[i];
        if (n > sw && n > sh && greater) continue;
        buf.assign((size_t)n * n * 4, 0);
        IcScale(src, sw, sh, &buf[0], n);
        char path[1200];
        snprintf(path, sizeof(path), "%s/icons/%dx%d.png", out.c_str(), n, n);
        if (IcSavePNG(path, &buf[0], n, n)) { linuxSize->push_back(n); greater = n; }
    }
    // mipmaps (android): los 5 dpi siempre (el launcher los espera todos)
    static const struct { const char* dpi; int px; } droid[5] = {
        { "mdpi", 48 }, { "hdpi", 72 }, { "xhdpi", 96 }, { "xxhdpi", 144 }, { "xxxhdpi", 192 } };
    for (int i = 0; i < 5; i++) {
        int n = droid[i].px;
        buf.assign((size_t)n * n * 4, 0);
        IcScale(src, sw, sh, &buf[0], n);
        char path[1200];
        snprintf(path, sizeof(path), "%s/icons/android-%s.png", out.c_str(), droid[i].dpi);
        IcSavePNG(path, &buf[0], n, n);
    }
    w3dEngine::FreeImage(src);
    // el mas grande generado -> icono.png (SDL_SetWindowIcon; viaja con los assets)
    if (greater) {
        char cmd[2500];
        snprintf(cmd, sizeof(cmd), "cp \"%s/icons/%dx%d.png\" \"%s/icono.png\"",
                 out.c_str(), greater, greater, out.c_str());
        if (ShellRun(cmd, out)) {}
    }
    return greater > 0;
}

// UN tamano (n x n) de un PNG, en 'dst' (crea su carpeta): el icono de una consola cuando el proyecto no trae uno
static bool GenerateIcon(const std::string& src, const std::string& dst, int n) {
    unsigned char* px = 0; int w = 0, h = 0;
    if (!w3dEngine::DecodeImage(src.c_str(), &px, &w, &h) || !px || w < 1 || h < 1) return false;
    MakeDirs(Folder(dst));
    std::vector<unsigned char> buf((size_t)n * n * 4, 0);
    IcScale(px, w, h, &buf[0], n);
    w3dEngine::FreeImage(px);
    return IcSavePNG(dst, &buf[0], n, n);
}

// las ESCENAS del proyecto = los UI raiz colgados de SceneCollection (tipo ui). Cada uno
// es una escena con su propio arbol + scripts embebidos (multi-escena). Mismo criterio que
// W3dSceneRegisterAll() del motor. El nombre de la escena = el "nombre" del UI.
static void CollectScenes(std::vector<UI*>* out) {
    if (!SceneCollection) return;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::ui) out->push_back((UI*)o);
    }
}

// la raiz del repo Whisk3D (Core + UI 2D + game runtime). Se busca en este orden:
//   1) la que el usuario fijo a mano en Ajustes (cfg.repoPath): es lo que hace que el editor
//      INSTALADO -- que no tiene el repo al lado -- pueda compilar juegos igual;
//   2) la variable de entorno W3D_REPO (scripts, CI, un build de prueba sin tocar su config.start);
//   3) subiendo de carpeta desde res/ hasta dar con libs/Whisk3DCore, para el editor corriendo
//      desde el arbol de codigo (build/ con res/ copiado al lado del binario);
//   4) subiendo desde la carpeta del BINARIO (Linux): un build dentro del arbol que tomo el res/
//      de una instalacion (/usr/share) igual encuentra su repo;
//   5) las FUENTES con las que se compilo este binario (__FILE__ de este archivo): un build FUERA
//      del arbol -- uno con sanitizers en otra carpeta, por ejemplo -- encuentra el repo sin
//      configurar nada. Solo si la ruta es absoluta (CMake las pasa asi) y todavia existe.
// En todos los casos se valida que exista la marca del repo (Objects.cpp). "" si no se encuentra.
static bool IsRepo(const std::string& dir) {
    return !dir.empty() && w3dFileSystem::FileExists(dir + "/libs/Whisk3DCore/objects/Objects.cpp");
}
static std::string UploadUntilRepo(std::string dir) {
    for (int i = 0; i < 8 && !dir.empty(); i++) {
        if (IsRepo(dir)) return dir;
        std::string up = Folder(dir);
        if (up == dir) break;
        dir = up;
    }
    return "";
}
static std::string RepoRoot(std::string* via) {
    std::string v, r;
    const char* env = getenv("W3D_REPO");
    if (IsRepo(cfg.repoPath))  { v = "settings"; r = cfg.repoPath; }
    else if (env && IsRepo(env)) { v = "environment"; r = env; }
    else {
        r = UploadUntilRepo(Folder(w3dFileSystem::GetResDir()));   // sube de res/
        if (!r.empty()) v = "res";
    }
#if defined(__linux__) && !defined(__ANDROID__)
    if (r.empty()) {
        char exe[4096];
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (n > 0) {
            exe[n] = 0;
            r = UploadUntilRepo(Folder(exe));
            if (!r.empty()) v = "binary";
        }
    }
#endif
    if (r.empty()) {
        // <repo>/main/io/CompileGame.cpp -> <repo>
        const std::string source = __FILE__;
        const bool absolute = (!source.empty() && (source[0] == '/' || source[0] == '\\')) ||
                              (source.size() > 2 && source[1] == ':');
        const std::string repo = absolute ? Folder(Folder(Folder(source))) : std::string();
        if (IsRepo(repo)) { v = "fonts"; r = repo; }
    }
    if (via) *via = v;
    return r;
}
std::string CompileGameRepoRoot(std::string* via) { return RepoRoot(via); }

// asegura el .gitignore de la carpeta del juego: platform-build/ (todo lo generado
// para compilar) y build/ (los resultados) se REGENERAN con "Compilar juego", asi que
// no van al repo. Si el .gitignore no existe se crea con un comentario breve; si
// existe pero le faltan esas entradas, se agregan al final (sin duplicar). Se compara
// por LINEA EXACTA (recortando espacios): que exista "platform-build/" no cuenta
// como tener "build/". Asi cualquier proyecto nuevo queda prolijo en git solo.
static void EnsureGitignore(const std::string& proj) {
    static const char* entries[2] = { "platform-build/", "build/" };
    std::string path = proj + "/.gitignore";
    std::string content;
    bool has[2] = { false, false };
    bool exists = false;
    FILE* f = fopen(path.c_str(), "r");
    if (f) {
        exists = true;
        char lin[1024];
        while (fgets(lin, sizeof(lin), f)) {
            content += lin;
            std::string s(lin);
            while (!s.empty() && (s[s.size()-1] == '\n' || s[s.size()-1] == '\r' ||
                                  s[s.size()-1] == ' '  || s[s.size()-1] == '\t')) s.erase(s.size()-1);
            size_t start = 0;
            while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) start++;
            if (start) s = s.substr(start);
            for (int e = 0; e < 2; e++) if (s == entries[e]) has[e] = true;
        }
        fclose(f);
    }
    if (has[0] && has[1]) return;                 // ya estan las dos: nada que hacer
    f = fopen(path.c_str(), exists ? "a" : "w");
    if (!f) return;                                    // sin permisos: no es fatal para compilar
    if (!exists)
        fputs("# Salidas de \"Compilar juego\" (Whisk3D): se regeneran, no van al repo\n", f);
    else if (!content.empty() && content[content.size()-1] != '\n')
        fputc('\n', f);                                // que la entrada nueva arranque en linea propia
    for (int e = 0; e < 2; e++) if (!has[e]) fprintf(f, "%s\n", entries[e]);
    fclose(f);
}

// ============================================================================
//  PAK de assets ("Assets: Empaquetados" de la tarjeta Juego): junta TODOS los
//  archivos del juego que quedaron en platform-build/<plat> (los .w3dui
//  re-exportados, los .lua, los .png de la raiz -incluye font.png e icono.png-
//  y la subcarpeta assets/ recursiva) y los escribe como pak.cpp: un array de
//  bytes C que se COMPILA dentro del binario. El main generado lo registra con
//  W3dPakRegister antes de cargar nada y el Core (w3dFilesystem) resuelve las
//  rutas ("menu.w3dui", "assets/pausa.png") primero contra el pak.
//
//  FORMATO (el LECTOR vive en libs/Whisk3DCore/io/w3dFilesystem.cpp; mantener
//  en sincronia):
//    [0..7]   magia "W3DPAK1\n" (en claro)
//    [8..11]  u32 LE semilla (en claro; FNV-1a del nombre del juego)
//    [12..]   u32 cantidad + TOC { u16 largoNombre, nombre, u32 offset,
//             u32 tamano } -> UN flujo XOR continuo con semilla ^ 0x544F4321
//    [datos]  cada archivo con su flujo XOR propio:
//             semilla ^ (offset * 2654435761u) ^ tamano
//  HONESTO: el XOR de flujo es OFUSCACION anti-curiosos (que el binario no
//  muestre los assets tal cual con un strings/hexdump), NO criptografia: la
//  semilla viaja en el mismo binario. No proteje contra alguien decidido.
// ============================================================================

// FNV-1a de 32 bits del nombre del juego -> la semilla del pak
static unsigned PakSeed(const std::string& name) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < name.size(); i++) { h ^= (unsigned char)name[i]; h *= 16777619u; }
    return h;
}
// flujo XOR: xorshift32 avanzado cada 4 bytes (MISMO generador que el lector del Core)
struct PakFlow { unsigned s; unsigned w; unsigned n; };
static void PakFlowInit(PakFlow* f, unsigned seed) {
    f->s = seed ? seed : 0x57334441u;   // estado 0 atasca xorshift
    f->w = 0; f->n = 0;
}
static unsigned char PakFlowByte(PakFlow* f) {
    if ((f->n & 3u) == 0u) {
        unsigned x = f->s; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        f->s = x; f->w = x;
    }
    unsigned char b = (unsigned char)(f->w >> ((f->n & 3u) * 8u));
    f->n++;
    return b;
}
static void PakU32(std::vector<unsigned char>& v, unsigned x) {
    v.push_back((unsigned char)x);         v.push_back((unsigned char)(x >> 8));
    v.push_back((unsigned char)(x >> 16)); v.push_back((unsigned char)(x >> 24));
}

// ============================================================================
//  LAS CARPETAS DE ASSETS DEL STAGING: UNA SOLA LISTA PARA LOS 4 TARGETS.
//
//  platform-build/<plat> es un ESPEJO DEL CONTENEDOR: el volcado del .w3d deja
//  cada entrada con SU nombre ("textures/pausa.png", "scripts/menu.lua"), que es
//  exactamente lo que referencian los .w3dui. Antes cada target tenia su propia
//  lista escrita a mano (el glob del CMake, los cp del script de Android, los
//  --embed-file de emcc y el listado del pak) y las cuatro nombraban "assets" y
//  "contenido" nada mas: con el contenedor v4 eso dejaba AFUERA las texturas, los
//  scripts y las fuentes, o sea que el juego compilado abria sin sus imagenes.
//  Ahora la lista esta UNA sola vez aca y la usan los cuatro.
//
//  Lo que NO esta en la lista NO viaja, y eso es a proposito: icons/ (los tamanos
//  del .deb/APK), out/ (el build intermedio de cmake/emcc), androidprj/ (la copia
//  del proyecto Android) y los generados del build (main.cpp, pak.cpp, CMakeLists,
//  build.log) son artefactos, no assets.
//
//  "escenas" NO esta: las escenas las RE-EXPORTA el compilador a la raiz del
//  staging con el nombre que usa el main.cpp generado (ver el volcado), asi que
//  la carpeta scenes/ del contenedor no se vuelca y nunca existe aca.
// ============================================================================
static const char* const kFoldersAssets[] = {
    // el espejo del contenedor v4 (las categorias de W3dCategoryByExtension)
    "scripts", "textures", "fonts", "sounds", "videos",
    "meshes", "animations", "models", "project", "extra",
    // scenes/: los .w3dui que referencia el project.json de un juego 3D (el
    // compilador los RE-EXPORTA ahi adentro con el nombre de entrada que el
    // archivo referencia, para que la UI del juego sea la VIVA del editor). Tambien
    // las ESCENAS 3D (.w3de) que cambiarEscena() carga desde su entrada.
    "scenes",
    // prefabs/: los .w3dp del proyecto (las instancias los generan)
    "prefabs",
    // proyectos v3 (archivos sueltos al lado del .w3d): siguen funcionando
    "assets", "content",
    // libs/<nombre>/: las entradas de cada LIBRERIA externa que el proyecto usa (io/Libraries.h). El runtime
    // lee sus rutas "lib:<nombre>/..." de ahi (W3dLibPathPacked), sin montar ningun .w3d
    "libs",
    0
};

// ============================================================================
//  LAS CARPETAS DE ASSETS QUE VIAJAN CON EL JUEGO — LA LISTA VIVA.
//
//  kFoldersAssets son las que SIEMPRE existen (el espejo del contenedor). Pero
//  un proyecto puede tener archivos AFUERA del contenedor a proposito (refs
//  "ext:", ver W3dExternalRefMark): el demo guarda ahi los .cap de los rieles de
//  camara, en una carpeta riel/ que no esta en la lista fija. El compilador los
//  copia al staging respetando su ruta relativa, y entonces el staging tiene
//  carpetas que la lista fija no conoce -> el .deb salia sin ellas y la camara
//  del juego se quedaba sin riel (pantalla negra con el HUD encima).
//
//  Por eso la lista que usan los cuatro targets (CMake, Android, emcc y el pak)
//  es ESTA, que se arma MIRANDO el staging ya poblado: las fijas primero (orden
//  estable) y despues cualquier otra carpeta que haya quedado, salteando los
//  artefactos del build.
// ============================================================================
static std::vector<std::string> gCarpAssets;
static const char* CarpAsset(int i) {
    return (i >= 0 && i < (int)gCarpAssets.size()) ? gCarpAssets[i].c_str() : 0;
}
static void CarpAssetsDiscover(const std::string& out) {
    gCarpAssets.clear();
    for (int c = 0; kFoldersAssets[c]; c++) gCarpAssets.push_back(kFoldersAssets[c]);
    std::vector<w3dFileSystem::DirEntry> ent;
    if (!w3dFileSystem::ListDir(out, ent)) return;
    for (size_t i = 0; i < ent.size(); i++) {
        if (!ent[i].isDir) continue;
        const std::string& n = ent[i].name;
        // artefactos del build, no assets del juego
        // (androidviejo y obj23: el proyecto y lo compilado del APK de Android 2.3)
        if (n == "out" || n == "icons" || n == "androidprj" || n == "androidviejo" || n == "obj23" ||
            n == "." || n == "..") continue;
        bool already = false;
        for (size_t k = 0; k < gCarpAssets.size(); k++) if (gCarpAssets[k] == n) { already = true; break; }
        if (!already) gCarpAssets.push_back(n);
    }
}

// ============================================================================
//  LO QUE EL PROYECTO NOMBRA POR RUTA RELATIVA Y NO ESTA EN NINGUNA LISTA.
//
//  Reporte del dueno: el .deb del demo salio con CERO archivos .wav. Y es cierto
//  que ningun paso lo copiaba: los sonidos del proyecto viven en sounds/ y
//  music/ AL LADO del .w3d, no adentro del contenedor ni referenciados por
//  ningun .w3dui -- los nombra el LUA y nada mas:
//      sound("sounds/" .. n, ...)          -- prefijo + variable
//      RUTA[s] = "musica/inst" .. NN .. ".wav"
//  El compilador copiaba (a) las entradas del contenedor, (b) las refs de los
//  .w3dui y (c) las refs externas declaradas: nada de eso cubre una ruta que el
//  script ARMA en tiempo de ejecucion. Resultado: el juego instalado, mudo.
//
//  LA REGLA, generica y sin nombres de ningun juego: se leen los .lua (y el
//  project.json/.w3d) que ya quedaron en el staging, se sacan sus LITERALES de
//  texto, y de cada literal que tenga una '/' se prueban dos cosas contra la
//  carpeta del proyecto:
//     1. el literal COMPLETO es un archivo             -> viaja ese archivo;
//     2. la CARPETA del literal existe                 -> viaja la carpeta entera.
//  La (2) es la que cubre los dos casos de arriba (el literal es un prefijo
//  incompleto, la ruta real la termina una variable) y es tambien la unica
//  respuesta honesta: si un script arma rutas, el compilador no puede saber
//  cuales sin ejecutarlo, asi que lleva el directorio del que salen.
//
//  Nunca sale de la carpeta del proyecto (nada de rutas absolutas ni ".."), no
//  pisa lo que ya esta en el staging (el volcado del contenedor manda) y saltea
//  las carpetas generadas (platform-build/, build/).
// ============================================================================
static bool RefRelValid(const std::string& s) {
    if (s.empty() || s.size() > 300) return false;
    if (s[0] == '/' || s[0] == '.') return false;
    if (s.find("..") != std::string::npos) return false;
    if (s.find(':') != std::string::npos) return false;        // http://, C:\, "ext:"
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c == '\\' || c == '*' || c == '?' || c == '"') return false;
    }
    return true;
}
// las RUTAS que nombra un archivo de texto. No se miran las comillas a proposito:
// el .w3d de TEXTO escribe sus rutas peladas (`filePath: escenario/01_bZ.obj`), el
// json y el lua las escriben entrecomilladas, y las dos formas tienen que viajar.
// Una "ruta" aca es una corrida de caracteres de ruta que contiene una '/'. Los
// falsos positivos no cuestan nada: mas abajo cada candidato se verifica contra
// el disco y el que no existe se descarta.
static void RefsTextPaths(const char* p, size_t n, std::vector<std::string>* out);
static void RefsPathsOf(const std::string& file, std::vector<std::string>* out) {
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(file, data) || data.empty()) return;
    RefsTextPaths((const char*)&data[0], data.size(), out);
}
// (lo mismo sobre un texto ya leido: los .lua de una LIBRERIA, que se leen de su .w3d)
static void RefsTextPaths(const char* p, size_t n, std::vector<std::string>* out) {
    if (!p || n == 0) return;
    // bytecode lua (0x1B "Lua") u otro binario: no tiene rutas que leer asi
    if ((unsigned char)p[0] == 0x1B) return;
    std::string cur;
    for (size_t i = 0; i <= n; i++) {
        char c = (i < n) ? p[i] : ' ';
        bool fromPath = (c == '/' || c == '.' || c == '_' || c == '-' ||
                       (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                       (c >= 'a' && c <= 'z'));
        if (fromPath && cur.size() <= 300) { cur.push_back(c); continue; }
        if (cur.find('/') != std::string::npos && RefRelValid(cur)) out->push_back(cur);
        cur.clear();
    }
}
// junta los archivos de TEXTO del staging que pueden nombrar assets (.lua y el
// proyecto/escenas), sin entrar a los artefactos del build
static void TextsOfStaging(const std::string& dir, const std::string& rel,
                             std::vector<std::string>* out, int prof) {
    if (prof > 6) return;
    std::vector<w3dFileSystem::DirEntry> ent;
    if (!w3dFileSystem::ListDir(dir, ent)) return;
    for (size_t i = 0; i < ent.size(); i++) {
        const std::string& n = ent[i].name;
        if (n == "." || n == "..") continue;
        if (ent[i].isDir) {
            if (n == "out" || n == "icons" || n == "androidprj" || n == "androidviejo" || n == "obj23") continue;
            TextsOfStaging(dir + "/" + n, rel.empty() ? n : rel + "/" + n, out, prof + 1);
            continue;
        }
        size_t p = n.find_last_of('.');
        if (p == std::string::npos) continue;
        std::string ext = n.substr(p);
        if (ext == ".lua" || ext == ".json" || ext == ".w3dui" || ext == ".w3d")
            out->push_back(dir + "/" + n);
    }
}
static void CopyRelativeRefsOfLua(const std::string& out, const std::string& proj) {
    std::vector<std::string> texts;
    TextsOfStaging(out, std::string(), &texts, 0);
    std::vector<std::string> paths;
    for (size_t i = 0; i < texts.size(); i++) RefsPathsOf(texts[i], &paths);
    std::set<std::string> files, folders;
    // (el .w3d de una LIBRERIA vinculada no viaja por su nombre: sus entradas ya van en libs/<nombre>/)
    std::set<std::string> libs;
    for (int l = 0; l < W3dLibsCount(); l++) {
        std::string rel;
        if (W3dPathUnderFolder(W3dLibsDiskPath(l), proj, rel)) libs.insert(rel);
    }
    for (size_t i = 0; i < paths.size(); i++) {
        const std::string& s = paths[i];
        if (libs.count(s)) continue;
        std::string dir = Folder(s);            // "sounds/x.wav" -> "sonidos"
        if (dir.empty() || dir == s) {
            // nombre pelado con '/' raro: solo puede ser un archivo suelto
            if (w3dFileSystem::FileExists(proj + "/" + s) && !w3dFileSystem::IsDir(proj + "/" + s))
                files.insert(s);
            continue;
        }
        if (dir == "platform-build" || dir == "build") continue;
        // LA CARPETA ENTERA, no el archivo suelto: un .obj arrastra su .mtl y el
        // .mtl su .png, y ninguno de los dos aparece nombrado en el proyecto (los
        // nombra el archivo anterior de la cadena). Copiar solo lo nombrado dejaba
        // el escenario SIN sus materiales -> el juego lo dibujaba blanco.
        if (w3dFileSystem::IsDir(proj + "/" + dir)) folders.insert(dir);
    }
    int nA = 0, nC = 0;
    for (std::set<std::string>::iterator it = files.begin(); it != files.end(); ++it) {
        if (!SafeRelPath(*it) || w3dFileSystem::FileExists(out + "/" + *it)) continue;   // (afuera, raro, o ya viajo)
        char cmd[3000];
        snprintf(cmd, sizeof(cmd), "cp \"%s/%s\" \"%s/%s\"",
                 proj.c_str(), it->c_str(), out.c_str(), it->c_str());
        if (ShellRun(cmd, proj, out, *it) == 0) nA++;
    }
    for (std::set<std::string>::iterator it = folders.begin(); it != folders.end(); ++it) {
        // -n (no-clobber): lo que YA esta en el staging manda (el volcado del
        // contenedor y las escenas re-exportadas son la version viva). Esto
        // COMPLETA carpetas a medias -- el caso real: escenario/ llegaba con los
        // .obj y los .w3dvis que el .w3d nombra, sin un solo .mtl.
        if (!SafeRelPath(*it)) continue;
        char cmd[3000];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/%s\" && cp -rn \"%s/%s/.\" \"%s/%s/\" 2>/dev/null || true",
                 out.c_str(), it->c_str(), proj.c_str(), it->c_str(), out.c_str(), it->c_str());
        if (ShellRun(cmd, out, proj, *it) == 0) nC++;
    }
    // (ni por su CARPETA: un texto que nombra la carpeta de las librerias -"autos/auto_1.w3d" en un comentario- la
    // copiaba entera, y cada .w3d viajaba dos veces, suelto y en libs/<nombre>/: +2.8 MB en el APK de Daytona)
    for (std::set<std::string>::iterator it = libs.begin(); it != libs.end(); ++it) {
        const std::string loose = out + "/" + *it;
        if (!w3dFileSystem::FileExists(loose)) continue;
        remove(loose.c_str());
        const std::string dir = Folder(*it);
        if (!dir.empty() && dir != *it) W3D_RMDIR((out + "/" + dir).c_str());   // (si quedo vacia)
    }
    if (nA || nC)
        w3dLogf("CompileGame: %d carpeta(s) y %d archivo(s) que el proyecto nombra por "
                "ruta relativa, completados en el staging", nC, nA);
}

// ============================================================================
//  LAS LIBRERIAS EXTERNAS en el juego (io/Libraries.h): lo que nombra un texto, lo que dejan afuera
// ============================================================================
// las referencias "ext:..." de un texto JSON (el string entero, sin el prefijo y sin escapes)
static void ExtsOfText(const std::string& t, std::vector<std::string>* out) {
    for (size_t p = t.find("\"ext:"); p != std::string::npos; p = t.find("\"ext:", p + 5)) {
        std::string r;
        size_t i = p + 5;
        for (; i < t.size() && t[i] != '"'; i++) {
            if (t[i] == '\\' && i + 1 < t.size()) { i++; r += (t[i] == 'n') ? '\n' : t[i]; }
            else r += t[i];
        }
        if (i < t.size() && !r.empty()) out->push_back(r);
    }
}
// copia un archivo de disco (o una ruta que ReadFileBytes sepa leer) a 'dst' (crea su carpeta). false = no se pudo
static bool CopyToStaging(const std::string& src, const std::string& dst) {
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(src, d)) return false;
    MakeDirs(Folder(dst));
    FILE* fh = fopen(dst.c_str(), "wb");
    if (!fh) return false;
    if (!d.empty()) fwrite(&d[0], 1, d.size(), fh);
    return fclose(fh) == 0;
}
static void JsonText(std::string& s, const std::string& t) {
    s += '"';
    for (size_t k = 0; k < t.size(); k++) { if (t[k] == '"' || t[k] == '\\') s += '\\'; s += t[k]; }
    s += '"';
}

// junta las rutas RELATIVAS (contra 'out') de todos los archivos del juego:
// *.w3dui/*.lua/*.png de la raiz + assets/ entera recursiva. NO entra a icons/
// ni out/ ni androidprj/: son artefactos del build, no assets del juego.
static void PakListRoot(const std::string& out, std::vector<std::string>* paths) {
    std::vector<w3dFileSystem::DirEntry> ent;
    if (!w3dFileSystem::ListDir(out, ent)) return;
    for (size_t i = 0; i < ent.size(); i++) {
        if (ent[i].isDir) continue;
        const std::string& n = ent[i].name;
        size_t p = n.find_last_of('.');
        if (p == std::string::npos) continue;
        std::string ext = n.substr(p);
        // .json = project.json, la ESCENA 3D del juego (el resto es lo de siempre)
        if (ext == ".w3dui" || ext == ".lua" || ext == ".png" || ext == ".json") paths->push_back(n);
    }
}
static void PakListAssets(const std::string& out, const std::string& rel,
                            std::vector<std::string>* paths) {
    std::vector<w3dFileSystem::DirEntry> ent;
    if (!w3dFileSystem::ListDir(out + "/" + rel, ent)) return;
    for (size_t i = 0; i < ent.size(); i++) {
        std::string r = rel + "/" + ent[i].name;
        if (ent[i].isDir) PakListAssets(out, r, paths);
        else              paths->push_back(r);
    }
}

// ============================================================================
//  BYTECODE en PRODUCCION ("Modo debug" DESTILDADO): cada .lua COPIADO al
//  staging (platform-build/<plat>) se reemplaza por su bytecode Lua stripped
//  CON EL MISMO NOMBRE .lua (las refs de los .w3dui no cambian; RunFile
//  del Core detecta bytecode por el header 0x1B via luaL_loadbuffer, y su skip
//  de BOM no lo toca: el bytecode nunca arranca con EF BB BF). Aplica a los 4
//  targets porque corre ANTES de armar el pak / embeber / copiar assets: carga
//  mas rapida, menos peso y fuente ilegible. Los .lua del PROYECTO no se tocan
//  jamas (el IDE sigue editando fuente). Con "Modo debug" tildado: fuente tal
//  cual, como siempre.
//  Junta los mismos lugares que viajan con el juego: *.lua de la raiz del
//  staging + assets/ y content/ recursivas. Si algun .lua no compila
//  (sintaxis) devuelve false con el error humano en *err: el build ABORTA
//  (mejor enterarse aca que en el device).
// ============================================================================
static bool CompileLuasStaging(const std::string& out, std::string* err,
                                unsigned* count, size_t* bytesSource, size_t* bytesBytecode) {
    *count = 0; *bytesSource = 0; *bytesBytecode = 0;
    std::vector<std::string> paths;
    {   // .lua sueltos de la raiz del staging
        std::vector<w3dFileSystem::DirEntry> ent;
        if (w3dFileSystem::ListDir(out, ent))
            for (size_t i = 0; i < ent.size(); i++)
                if (!ent[i].isDir && ent[i].name.size() > 4 &&
                    ent[i].name.compare(ent[i].name.size() - 4, 4, ".lua") == 0)
                    paths.push_back(ent[i].name);
    }
    {   // las carpetas de assets recursivas: solo los .lua (el resto viaja tal cual).
        // scripts/ es la del contenedor v4: sin ella los .lua internos se quedaban
        // en fuente aun compilando en produccion.
        std::vector<std::string> all;
        for (int c = 0; CarpAsset(c); c++)
            PakListAssets(out, CarpAsset(c), &all);
        for (size_t i = 0; i < all.size(); i++)
            if (all[i].size() > 4 &&
                all[i].compare(all[i].size() - 4, 4, ".lua") == 0)
                paths.push_back(all[i]);
    }
    for (size_t i = 0; i < paths.size(); i++) {
        std::string path = out + "/" + paths[i];
        std::vector<unsigned char> prev, after;
        w3dFileSystem::ReadFileBytes(path, prev);   // peso fuente (solo informativo)
        std::string detail;
        if (!LuaCompileFile(path, path, &detail)) {
            *err = detail.empty() ? BldFmt(T("%s: does not compile"), paths[i]) : detail;
            return false;
        }
        w3dFileSystem::ReadFileBytes(path, after);
        (*count)++;
        *bytesSource += prev.size();
        *bytesBytecode += after.size();
    }
    return true;
}

// arma el pak con los archivos del juego y lo escribe como <out>/pak.cpp
// (array de bytes + tamano; el main generado lo registra). false si no hay
// archivos o no se pudo leer/escribir algo ('err' cuenta que paso).
static bool WritePak(const std::string& out, const std::string& name, std::string* err) {
    std::vector<std::string> paths;
    PakListRoot(out, &paths);
    // EL PAK SE ARMA DEL MISMO ESPEJO QUE COPIAN LOS OTROS TRES TARGETS: las
    // carpetas del contenedor volcado (textures/, scripts/, fonts/, meshes/...)
    // mas las dos de los proyectos v3. Si una carpeta no existe, no agrega nada.
    for (int c = 0; CarpAsset(c); c++)
        PakListAssets(out, CarpAsset(c), &paths);
    if (paths.empty()) { *err = T("there are no game files to pack"); return false; }

    // contenido de cada archivo (en memoria: los juegos Whisk3D son chicos)
    std::vector< std::vector<unsigned char> > data(paths.size());
    for (size_t i = 0; i < paths.size(); i++) {
        if (!w3dFileSystem::ReadFileBytes(out + "/" + paths[i], data[i])) {
            *err = BldFmt(T("could not read %s"), paths[i]); return false;
        }
    }
    unsigned seed = PakSeed(name);

    // layout: 12 de cabecera + (4 + entradas) de TOC + los datos seguidos
    unsigned tocSize = 4;
    for (size_t i = 0; i < paths.size(); i++) tocSize += 2 + (unsigned)paths[i].size() + 8;
    unsigned off = 12 + tocSize;
    std::vector<unsigned> offs(paths.size());
    for (size_t i = 0; i < paths.size(); i++) { offs[i] = off; off += (unsigned)data[i].size(); }

    std::vector<unsigned char> pak;
    pak.reserve(off);
    static const unsigned char magic[8] = { 'W','3','D','P','A','K','1','\n' };
    pak.insert(pak.end(), magic, magic + 8);
    PakU32(pak, seed);
    // TOC (cantidad + entradas) en claro primero...
    std::vector<unsigned char> toc;
    PakU32(toc, (unsigned)paths.size());
    for (size_t i = 0; i < paths.size(); i++) {
        toc.push_back((unsigned char)(paths[i].size()));
        toc.push_back((unsigned char)(paths[i].size() >> 8));
        toc.insert(toc.end(), paths[i].begin(), paths[i].end());
        PakU32(toc, offs[i]);
        PakU32(toc, (unsigned)data[i].size());
    }
    // ...y XOReado con su flujo al pak
    { PakFlow f; PakFlowInit(&f, seed ^ 0x544F4321u);
      for (size_t i = 0; i < toc.size(); i++) pak.push_back((unsigned char)(toc[i] ^ PakFlowByte(&f))); }
    // los datos: cada archivo con su flujo propio (el lector lo deriva del TOC)
    for (size_t i = 0; i < paths.size(); i++) {
        PakFlow f; PakFlowInit(&f, seed ^ (offs[i] * 2654435761u) ^ (unsigned)data[i].size());
        for (size_t k = 0; k < data[i].size(); k++)
            pak.push_back((unsigned char)(data[i][k] ^ PakFlowByte(&f)));
    }

    // pak.cpp: el array + su tamano (el main generado los declara extern y llama
    // a W3dPakRegister antes de cargar nada)
    FILE* f = fopen((out + "/pak.cpp").c_str(), "w");
    if (!f) { *err = T("could not write pak.cpp"); return false; }
    fputs("// GENERADO por Whisk3D (Compilar juego, Assets: Empaquetados).\n", f);
    fprintf(f, "// %u archivo(s) del juego en un pak ofuscado (XOR de flujo).\n",
            (unsigned)paths.size());
    fputs("// Ofuscacion anti-curiosos, NO criptografia: el lector y el formato\n", f);
    fputs("// estan en libs/Whisk3DCore/io/w3dFilesystem.cpp (W3dPakRegistrar).\n", f);
    fprintf(f, "extern const unsigned char g_w3dPakData[];\n");
    fprintf(f, "extern const unsigned g_w3dPakSize;\n");
    fprintf(f, "const unsigned g_w3dPakSize = %uu;\n", (unsigned)pak.size());
    fputs("const unsigned char g_w3dPakData[] = {", f);
    for (size_t i = 0; i < pak.size(); i++) {
        if ((i % 20u) == 0u) fputc('\n', f);   // 20 bytes por linea del array generado
        fprintf(f, "%u,", (unsigned)pak[i]);
    }
    fputs("\n};\n", f);
    { bool ok = (ferror(f) == 0); if (fclose(f) != 0) ok = false;   // el flush (y el disco lleno) pasa al cerrar
      if (!ok) { w3dLogfE("CompileGame: no pude escribir pak.cpp (disco lleno?)"); return false; } }
    w3dLogf("CompileGame: pak.cpp con %u archivo(s), %u bytes",
            (unsigned)paths.size(), (unsigned)pak.size());
    return true;
}

// genera el main.cpp del juego. Dos caminos segun el proyecto:
//  - MULTI-ESCENA (varias UI raiz): W3dGameLoadScene("x.w3dui") por cada escena (ACUMULA;
//    los scripts + refs vienen DENTRO de cada .w3dui, no se agregan a mano) + W3dGameInitialScene.
//  - UNA sola / legacy: W3dGameLoadUI(...) + W3dGameScript/W3dGameRef con lo que tiene el editor.
// El resto del main (ventana SDL/GL + loop + INPUT) es identico y lo comparten las 4
// plataformas, y es el MISMO manejo que el main artesanal del whiskpaddle (la referencia
// que anda): mouse (toque/arrastre/raton), multi-touch real (dedo 1..4 por slot),
// gamepad con alta/baja, exit() de lua, safe area en Android y el entry point de
// Android (SDL_main + AAssetManager). Antes el main generado solo cableaba teclado y
// gamepad: el mouse no andaba en el juego compilado.
// 'windowMode' (0 Ventana, 1 Pantalla completa, 2 Sin bordes) decide como se crea la
// ventana en desktop y si Android va inmersivo; winW/winH es el tamano para "Sin bordes"
// (el lienzo de la UI cuando es responsive).
// 'orientacion' (0 Todas, 1 Solo vertical, 2 Solo horizontal): hint SDL_HINT_ORIENTATIONS
// para Android/iOS (en desktop/web no aplica; el manifest de Android lo clava ademas
// WriteBuildAndroid). 'useSound': arranca el mixer de audio (W3dAudioInit) en el main.
// 'nombre': el nombre del juego (titulo de la ventana + carpeta de config persistente).
// 'withIcon': carga icono.png (generado del icono del proyecto) como icono de la ventana.
// 'empaquetar': assets EMPAQUETADOS -> declara el pak generado (pak.cpp) y lo
// registra en el Core (W3dPakRegister) como PRIMERA linea del main, antes de
// cargar nada (el icono, la fuente y las escenas ya salen del pak).
// "con Vulkan, primero su ventana": la linea que va antes del SDL_CreateWindow de OpenGL (que queda con "if(!g_win)")
static std::string WindowVk(const std::string& tit, const char* size, const char* flags) {
    return "#ifdef W3D_VULKAN\ng_win=windowVulkan(\"" + tit + "\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED," + size + "," +
           flags + ");\nif(!g_win)\n#endif\n";
}
static bool WriteMain(const std::string& path, const std::string& fontRel,
                         bool multi,
                         const std::string& w3duiRel, const std::vector<ObjScript>& objs,
                         const std::vector<SceneGen>& scenes, const std::string& initial,
                         int windowMode, int winW, int winH,
                         int orientation, bool useSound,
                         const std::string& name, bool withIcon, bool pack,
                         bool with3D) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs("// GENERADO por Whisk3D (Compilar juego). No hace falta editarlo.\n", f);
    // Android NO define SDL_MAIN_HANDLED: SDL renombra main->SDL_main y SDLActivity
    // lo busca por ese nombre (sin esto la app abre y se cierra). La PSP y la 3DS tampoco: el main de verdad es el de
    // SDL2main (la PSP: la informacion del modulo y el boton Home; la 3DS: monta el romfs)
    fputs("#if !defined(__ANDROID__) && !defined(W3D_PSP) && !defined(W3D_3DS)\n#define SDL_MAIN_HANDLED\n#endif\n", f);
    fputs("#include <SDL2/SDL.h>\n#include \"w3drun.h\"\n", f);
    // LAS CONSOLAS (PSP, PS Vita, 3DS, Dreamcast): la ventana, los graficos, cada cuadro y la carpeta de los assets
    // van por la capa de consola del runtime (Whisk3D-Examples/game/w3dconsola.h)
    fputs("#ifdef W3D_CONSOLE\n#include \"w3dconsola.h\"\n#endif\n", f);
    fputs("#include \"w3dGraphics.h\"\n", f);
    fputs("#include \"io/w3dFilesystem.h\"\n#include \"base/W3dConfig.h\"\n", f);
    fputs("#include \"script/W3dScript.h\"\n", f);
    fputs("#include \"base/w3dlog.h\"\n", f);   // (w3dLogf: el medidor de cuadros del modo debug)
    // el registro COMPARTIDO de mandos (hotplug + avisos + los binds de lua)
    fputs("#include \"script/BindsGame.h\"\n", f);
    fputs("#include <cstdio>\n#include <cstdlib>\n#include <cstring>\n#include <ctime>\n", f);
    fputs("#if defined(__linux__) || defined(__ANDROID__)\n#include <unistd.h>\n#endif\n", f);
    fputs("#ifdef __ANDROID__\n#include <jni.h>\n#include <android/asset_manager.h>\n"
          "#include <android/asset_manager_jni.h>\n#endif\n", f);
    fputs("#ifdef __EMSCRIPTEN__\n#include <emscripten.h>\n#include <emscripten/html5.h>\n#endif\n", f);
    if (withIcon)   // el icono de VENTANA es solo desktop (Android usa el launcher, web el favicon)
        fputs("#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)\n"
              "#include \"w3dTexture.h\" // DecodeImage: icono de la ventana\n#endif\n", f);
    if (useSound)   // audio/W3dAudio.h (forward-decl: evita meter el dir de audio al include path)
        fputs("namespace w3dEngine { bool W3dAudioInit(int); }\n", f);
    if (pack)   // assets empaquetados: el pak generado (pak.cpp, se compila junto)
        fputs("// assets EMPAQUETADOS: todos los archivos del juego viajan en este pak\n"
              "extern const unsigned char g_w3dPakData[];\nextern const unsigned g_w3dPakSize;\n", f);
    fputs("namespace gfx = w3dEngine;\nstatic SDL_Window* g_win=0; static bool g_run=true;\n", f);
    // VULKAN (la PC con el backend Vulkan: los tres backends en el ejecutable, graphics/w3dBackend.h): la ventana la abre
    // SDL con el cargador del sistema y el backend arranca sobre ella. Si no anda en esa maquina (sin el driver, sin la
    // GPU, o sin shaderc), el juego sigue con OpenGL
    fputs("#ifdef W3D_VULKAN\n#ifdef __ANDROID__\n#include <SDL_vulkan.h>\n#else\n#include <SDL2/SDL_vulkan.h>\n#endif\n"
          "#include \"graphics/vulkan/w3dVk.h\"\n"
          "#include \"graphics/w3dBackend.h\"\n#include <vector>\n#include <string>\n"
          "static bool g_vk=false;\n"
          "static bool vkSup(void* u,void* i,void* o){return SDL_Vulkan_CreateSurface((SDL_Window*)u,(VkInstance)i,(VkSurfaceKHR*)o)==SDL_TRUE;}\n"
          "static void vkTam(void* u,int* w,int* h){SDL_Vulkan_GetDrawableSize((SDL_Window*)u,w,h);}\n"
          "static SDL_Window* windowVulkan(const char* t,int x,int y,int w,int h,Uint32 fl){\n"
          " SDL_Window* win=0;\n"
          " if(gfx::BackendUse(gfx::BackendVulkan)&&SDL_Vulkan_LoadLibrary(NULL)==0){\n"
          "  win=SDL_CreateWindow(t,x,y,w,h,(fl&~SDL_WINDOW_OPENGL)|SDL_WINDOW_VULKAN);\n"
          "  if(win){unsigned n=0;SDL_Vulkan_GetInstanceExtensions(win,&n,NULL);std::vector<const char*> e(n);\n"
          "   if(n)SDL_Vulkan_GetInstanceExtensions(win,&n,&e[0]);\n"
          "   W3dVkWindow v;v.getInstanceProcAddr=SDL_Vulkan_GetVkGetInstanceProcAddr();v.extensions=n?&e[0]:NULL;\n"
          "   v.nExtensions=n;v.createSurface=vkSup;v.size=vkTam;v.user=win;std::string err;\n"
          "   if(!VkGLStart(v,&err)){w3dLogfW(\"[vk] no arranco: %s\",err.c_str());SDL_DestroyWindow(win);win=0;}}\n"
          "  if(!win)SDL_Vulkan_UnloadLibrary();}\n"
          " if(win){g_vk=true;w3dLogf(\"[juego] Vulkan: %s\",VkGLDevice());}\n"
          " else{w3dLogfW(\"[juego] Vulkan no anda en esta maquina: se usa OpenGL\");\n"
          "#ifdef __ANDROID__\n gfx::BackendUse(gfx::BackendGLES2);\n#else\n gfx::BackendUse(gfx::BackendOpenGL);\n#endif\n }\n"
          " return win;}\n#endif\n", f);
    fputs("static SDL_GameController* g_pad=0; static Uint32 g_lastMs=0;\n", f);
    // LA RESOLUCION REDUCIDA (Android 2.3, W3D_RESOLUCION = % de la pantalla): SDL 2.0.7 da el dedo (y el mouse que
    // simula) en px de la VISTA divididos por el ancho del bufer achicado -> se lleva al bufer
    fputs("#if defined(__ANDROID__) && defined(W3D_RESOLUTION)\n#define W3D_KDEDO (W3D_RESOLUTION/100.0f)\n#else\n"
          "#define W3D_KDEDO 1.0f\n#endif\n", f);
    // EL ACELEROMETRO (Android): SDL lo da como un joystick de 3 ejes ("Android Accelerometer", en g, en los ejes de la
    // pantalla ya girada) -> inclinacion() de lua (W3dScriptTilt)
    fputs("#ifdef __ANDROID__\nstatic SDL_Joystick* g_acel=0;\n#endif\n", f);
    // vibrate() de lua en los MANDOS (W3dScriptGamepadsVibrateHook): todos los conectados (SDL 2.0.9 o mas nuevo; la 2.0.7 de
    // Android 2.3 no tiene: ahi vibra el telefono, input/W3dVibrate.cpp)
    fputs("static void vibrateGamepads(int ms,float f){\n#if SDL_VERSION_ATLEAST(2,0,9)\n"
          "Uint16 v=(Uint16)(f*65535.0f);for(int ci=0;ci<W3dControlsCount();ci++){\n"
          " SDL_GameController* gc=(SDL_GameController*)W3dControlHandle(ci);if(gc)SDL_GameControllerRumble(gc,v,v,(Uint32)ms);}\n"
          "#else\n(void)ms;(void)f;\n#endif\n}\n", f);
    // multi-touch: SDL da un fingerId arbitrario por dedo; se mapea a un slot fijo
    // 0..3 (finger(1..4) en lua) para que cada jugador tactil conserve su dedo
    fputs("#define W3D_NDEDOS 4\n", f);
    fputs("static SDL_FingerID g_fingerId[W3D_NDEDOS]={(SDL_FingerID)-1,(SDL_FingerID)-1,"
          "(SDL_FingerID)-1,(SDL_FingerID)-1};\n", f);
    fputs("static int fingerSlot(SDL_FingerID id,bool create){\n", f);
    fputs("for(int i=0;i<W3D_NDEDOS;i++)if(g_fingerId[i]==id)return i;\n", f);
    fputs("if(!create)return -1;\n", f);
    fputs("for(int i=0;i<W3D_NDEDOS;i++)if(g_fingerId[i]==(SDL_FingerID)-1){g_fingerId[i]=id;return i;}\n", f);
    fputs("return -1;}\n", f);
    // ---- EL MANDO, CON HOTPLUG (el mismo registro que el editor) -------------
    // Antes el pad se abria SOLO si llegaba un SDL_CONTROLLERDEVICEADDED y solo
    // el primero; ahora se abren los que ya estaban al arrancar (padInicial) y
    // se atienden altas y bajas en caliente, avisando por el registro compartido
    // (consola + notificacion) igual que el editor. Al desenchufar se manda
    // ademas el estado NEUTRO a los scripts: sin eso el personaje se quedaba
    // caminando solo con el ultimo valor del stick.
    fputs("static const char* padType(SDL_GameController* gc){\n", f);
    fputs(" if(!gc)return \"generic\";\n switch(SDL_GameControllerGetType(gc)){\n", f);
    fputs("  case SDL_CONTROLLER_TYPE_XBOX360: case SDL_CONTROLLER_TYPE_XBOXONE: return \"xbox\";\n", f);
    fputs("  case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4:\n", f);
    fputs("  case SDL_CONTROLLER_TYPE_PS5: return \"playstation\";\n", f);
    fputs("  case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return \"nintendo\";\n", f);
    fputs("  default: return \"generic\";}}\n", f);
    fputs("static void padRelease(){W3dGameStick(0,0,0);W3dGameStick(1,0,0);W3dGameTrigger(0,0);W3dGameTrigger(1,0);\n", f);
    fputs(" static const char* nn[8]={\"a\",\"b\",\"x\",\"y\",\"up\",\"down\",\"left\",\"right\"};\n", f);
    fputs(" for(int i=0;i<8;i++)W3dGameButton(nn[i],false);}\n", f);
    fputs("static void padOpen(int idx){\n", f);
    fputs(" if(!SDL_IsGameController(idx))return;\n", f);
    fputs(" SDL_GameController* gc=SDL_GameControllerOpen(idx); if(!gc)return;\n", f);
    fputs(" SDL_Joystick* js=SDL_GameControllerGetJoystick(gc);\n", f);
    fputs(" int id=js?(int)SDL_JoystickInstanceID(js):-1;\n", f);
    fputs(" if(!W3dControlAdd(gc,id,SDL_GameControllerName(gc),padType(gc))){SDL_GameControllerClose(gc);return;}\n", f);
    fputs(" if(!g_pad)g_pad=gc;}\n", f);
    fputs("static void padClose(int id){\n", f);
    fputs(" void* h=0; if(!W3dControlRemove(id,&h))return;\n", f);
    fputs(" if(g_pad==(SDL_GameController*)h)g_pad=(SDL_GameController*)W3dControlHandle(0);\n", f);
    fputs(" if(h)SDL_GameControllerClose((SDL_GameController*)h);\n", f);
    fputs(" padRelease();}\n", f);
    fputs("static void currentSize(int*w,int*h){\n#ifdef __EMSCRIPTEN__\n", f);
    fputs("double cw=0,ch=0; emscripten_get_element_css_size(\"#canvas\",&cw,&ch);\n", f);
    fputs("*w=(int)cw;*h=(int)ch; static int lw=0,lh=0;\n", f);
    fputs("if(*w!=lw||*h!=lh){emscripten_set_canvas_element_size(\"#canvas\",*w,*h);lw=*w;lh=*h;}\n", f);
    fputs("#elif defined(W3D_CONSOLE)\nW3dConsoleSize(w,h);\n#else\n"
          "#ifdef W3D_VULKAN\nif(g_vk){SDL_Vulkan_GetDrawableSize(g_win,w,h);return;}\n#endif\n"
          "SDL_GL_GetDrawableSize(g_win,w,h);\n", f);
    fputs("if(*w<1||*h<1)SDL_GetWindowSize(g_win,w,h);\n#endif\nif(*w<1)*w=1;if(*h<1)*h=1;}\n", f);
    // Android: insets del notch/barras (getSafeInsets del Activity) -> la UI se acota
    // (cada medio segundo, no cada cuadro: es una llamada a Java. Si el Activity no tiene getSafeInsets -una
    //  plantilla vieja- no se vuelve a preguntar: GetMethodID tira una excepcion de Java cada vez)
    fputs("#ifdef __ANDROID__\nstatic void updateSafeArea(){\n", f);
    fputs("static int frames=0;static bool noMethod=false;\n", f);
    fputs("if(noMethod||(frames++%30)!=0)return;\n", f);
    fputs("JNIEnv* env=(JNIEnv*)SDL_AndroidGetJNIEnv();\n", f);
    fputs("jobject act=(jobject)SDL_AndroidGetActivity();\n", f);
    fputs("if(!env||!act)return;\n", f);
    fputs("jclass cl=env->GetObjectClass(act);\n", f);
    fputs("jmethodID mid=cl?env->GetMethodID(cl,\"getSafeInsets\",\"()[I\"):0;\n", f);
    fputs("if(!mid)noMethod=true;\n", f);
    fputs("if(mid){jintArray arr=(jintArray)env->CallObjectMethod(act,mid);\n", f);
    fputs("if(arr&&env->GetArrayLength(arr)>=4){jint* v=env->GetIntArrayElements(arr,0);\n", f);
    fputs("W3dGameSafeArea((int)v[0],(int)v[1],(int)v[2],(int)v[3]);\n", f);
    fputs("env->ReleaseIntArrayElements(arr,v,JNI_ABORT);}\n", f);
    fputs("if(arr)env->DeleteLocalRef(arr);}\n", f);
    fputs("if(env->ExceptionCheck())env->ExceptionClear();\n", f);
    fputs("if(cl)env->DeleteLocalRef(cl);\nenv->DeleteLocalRef(act);}\n#endif\n", f);
    // (el medidor del modo debug: el skinning y las rutinas de la escena 3D, con sus contadores del motor)
    fputs("#if W3D_DEV_LOG && defined(W3D_GAME_3D)\nextern unsigned int (*g_skinClockMs)();extern int g_skinPerfMs;"
          "extern double g_routinesMs;\nstatic unsigned int clockSkin(){return SDL_GetTicks();}\n"
          "extern double (*g_skinClockHeight)();extern double g_skinTotalMs,g_skinPoseMs;extern double W3dNowMs();\n#endif\n", f);
    // (las pruebas del modo debug, sin recompilar: W3D_LUA="Objeto:codigo" corre ese lua en el script del objeto al tercer
    // cuadro -ir directo a una pelea, esconder una parte de la escena- y W3D_MEDIR_GPU=1 espera a la GPU antes del swap y
    // lo mide aparte. En Android las variables salen de /data/local/tmp/w3d_env.txt: NOMBRE=valor por linea, con adb push)
    fputs("#if W3D_DEV_LOG && defined(W3D_GAME_3D)\n#include \"objects/Objects.h\"\n"
          "static void testLua(){static int n=0;if(++n!=3)return;const char* c=getenv(\"W3D_LUA\");if(!c||!*c)return;\n"
          "const char* d=strchr(c,':');if(!d)return;std::string name(c,d-c),e;\n"
          "Object* o=SceneCollection?FindObjectByName(SceneCollection,name):0;\n"
          "if(!o){w3dLogf(\"[juego] W3D_LUA: no hay un objeto '%s'\",name.c_str());return;}\n"
          "if(W3dScriptRun(o,d+1,e))w3dLogf(\"[juego] W3D_LUA: %s\",d+1);"
          "else w3dLogf(\"[juego] W3D_LUA: %s\",e.c_str());}\n#endif\n", f);
    // (modo debug, Android ARM: W3D_PERFIL=<segundos> muestrea el programa con el reloj de CPU (SIGPROF) durante esos
    // segundos desde el cuadro 300, y el log dice cuanto cayo en cada biblioteca y en cada bloque de 64 bytes de cada una:
    // con el libmain.so sin recortar de obj23 y addr2line sale la funcion de cada bloque; las del sistema, con adb pull y nm -D)
    fputs("#if W3D_DEV_LOG && defined(__ANDROID__) && defined(__arm__)\n"
          "#include <signal.h>\n#include <sys/time.h>\n#include <dlfcn.h>\n#include <asm/sigcontext.h>\n#include <map>\n"
          "static unsigned long g_perfPc[32768];static volatile int g_perfN=0;\n"
          "struct PerfUc{unsigned long fl;void* link;struct{void* sp;int fl;size_t size;}stack;struct sigcontext mc;};\n"
          "static void perfSignal(int,siginfo_t*,void* uc){int n=g_perfN;"
          "if(n<32768){g_perfPc[n]=((PerfUc*)uc)->mc.arm_pc;g_perfN=n+1;}}\n"
          "static void profileFrame(){static int frame=0,state=0;static Uint32 until=0;const char* e=getenv(\"W3D_PERFIL\");\n"
          "if(!e||!*e||state==2)return;\n"
          "if(state==0){if(++frame<300)return;struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_sigaction=perfSignal;\n"
          "sa.sa_flags=SA_SIGINFO|SA_RESTART;sigaction(SIGPROF,&sa,0);struct itimerval it;it.it_interval.tv_sec=0;\n"
          "it.it_interval.tv_usec=1000;it.it_value=it.it_interval;setitimer(ITIMER_PROF,&it,0);\n"
          "until=SDL_GetTicks()+1000u*(Uint32)atoi(e);state=1;w3dLogf(\"[perfil] empieza\");return;}\n"
          "if(SDL_GetTicks()<until)return;\n"
          "struct itimerval no;memset(&no,0,sizeof(no));setitimer(ITIMER_PROF,&no,0);state=2;\n"
          "std::map<std::string,int> libs,num;std::map<std::pair<int,unsigned long>,int> blocks;const int n=g_perfN;\n"
          "for(int i=0;i<n;i++){Dl_info di;memset(&di,0,sizeof(di));\n"
          "const bool has=dladdr((void*)g_perfPc[i],&di)&&di.dli_fname;const std::string lib=has?di.dli_fname:\"?\";libs[lib]++;\n"
          "if(!num.count(lib)){const int k=(int)num.size();num[lib]=k;}\n"
          "if(has)blocks[std::make_pair(num[lib],(g_perfPc[i]-(unsigned long)di.dli_fbase)>>6)]++;}\n"
          "w3dLogf(\"[perfil] %d muestras\",n);\n"
          "for(std::map<std::string,int>::iterator it=libs.begin();it!=libs.end();++it)"
          "w3dLogf(\"[perfil] lib %d %s %d\",num[it->first],it->first.c_str(),it->second);\n"
          "char l[400];int k=0,m=0,lib=-1;\n"
          "for(std::map<std::pair<int,unsigned long>,int>::iterator it=blocks.begin();it!=blocks.end();++it){\n"
          "if(it->first.first!=lib&&m){w3dLogf(\"[perfil] b%d%s\",lib,l);k=m=0;}lib=it->first.first;\n"
          "k+=snprintf(l+k,sizeof(l)-k,\" %lx:%d\",it->first.second<<6,it->second);if(++m==16){w3dLogf(\"[perfil] b%d%s\",lib,l);k=m=0;}}\n"
          "if(m)w3dLogf(\"[perfil] b%d%s\",lib,l);w3dLogf(\"[perfil] fin\");}\n#endif\n", f);
    fputs("static void frame(){SDL_Event e;while(SDL_PollEvent(&e)){\n", f);
    fputs("if(e.type==SDL_QUIT)g_run=false;\n", f);
    fputs("else if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE)g_run=false;\n", f);
    fputs("else if(e.type==SDL_KEYDOWN)W3dGameKey(e.key.keysym.sym,true);\n", f);
    fputs("else if(e.type==SDL_KEYUP)W3dGameKey(e.key.keysym.sym,false);\n", f);
    // HOTPLUG: el MISMO camino que el editor (padAbrir/padCerrar usan el registro
    // compartido de BindsJuego -> el aviso y los binds controles()/control() de lua
    // son identicos en el Play y en el .deb).
    fputs("else if(e.type==SDL_CONTROLLERDEVICEADDED)padOpen(e.cdevice.which);\n", f);
    fputs("else if(e.type==SDL_CONTROLLERDEVICEREMOVED)padClose(e.cdevice.which);\n", f);
    // mouse: click/arrastre -> touch() en lua; posicion sin clic -> mouse()
    fputs("else if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT)\n", f);
    fputs(" W3dGameTouch((int)(e.button.x*W3D_KDEDO),(int)(e.button.y*W3D_KDEDO),true);\n", f);
    fputs("else if(e.type==SDL_MOUSEMOTION){\n", f);
    fputs(" W3dGameMouse((int)(e.motion.x*W3D_KDEDO),(int)(e.motion.y*W3D_KDEDO),true);\n", f);
    fputs(" if(e.motion.state&SDL_BUTTON_LMASK)W3dGameTouch((int)(e.motion.x*W3D_KDEDO),(int)(e.motion.y*W3D_KDEDO),true);}\n", f);
    fputs("else if(e.type==SDL_MOUSEBUTTONUP&&e.button.button==SDL_BUTTON_LEFT)\n", f);
    fputs(" W3dGameTouch((int)(e.button.x*W3D_KDEDO),(int)(e.button.y*W3D_KDEDO),false);\n", f);
    // multi-touch real (tactil/movil): cada dedo -> su slot -> finger(1..4) en lua
    fputs("else if(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERMOTION){\n", f);
    fputs(" int s=fingerSlot(e.tfinger.fingerId,e.type==SDL_FINGERDOWN);\n", f);
    fputs(" if(s>=0){int w,h;currentSize(&w,&h);"
          "W3dGameFinger(s,(int)(e.tfinger.x*w*W3D_KDEDO),(int)(e.tfinger.y*h*W3D_KDEDO),true);}}\n", f);
    fputs("else if(e.type==SDL_FINGERUP){\n", f);
    fputs(" int s=fingerSlot(e.tfinger.fingerId,false);\n", f);
    fputs(" if(s>=0){int w,h;currentSize(&w,&h);"
          "W3dGameFinger(s,(int)(e.tfinger.x*w*W3D_KDEDO),(int)(e.tfinger.y*h*W3D_KDEDO),false);\n", f);
    fputs(" g_fingerId[s]=(SDL_FingerID)-1;}}\n", f);
    fputs("}\n", f);
    // (se pidio salir: este cuadro ya no se dibuja. En la 3DS el sistema suspende la GPU para cerrar la app y empezar un
    // cuadro de citro3d la esperaria para siempre)
    fputs("#ifdef __EMSCRIPTEN__\nif(!g_run){emscripten_cancel_main_loop();return;}\n#else\nif(!g_run)return;\n#endif\n", f);
    // GAMEPAD del juego COMPILADO. Tiene que entregar exactamente lo mismo que el
    // Play del editor (SimGame.cpp, TickReal) o el juego se porta distinto una vez
    // exportado. Aca se leian SOLO el stick izquierdo y el boton "a": el stick
    // derecho y b/x/y andaban en el editor y estaban muertos en el .deb.
    // TODOS los mandos, no solo el primero: el editor ya se comporta asi (su
    // axisState/buttonState los alimenta CUALQUIER control abierto, ver
    // RefreshInputControllerSDL), asi que leer solo g_pad hacia que en el .deb el
    // segundo mando enchufado no existiera. Se MEZCLAN: gana el eje de mayor
    // modulo y los botones se OR-ean, que es lo que hace falta en un juego de un
    // jugador con dos mandos a mano.
    fputs("{const float DZ=0.2f,M=1.0f/32767.0f;\n", f);
    fputs("float lx=0,ly=0,rx=0,ry=0,gl=0,gr=0;int du=0,dd=0,dl=0,dr=0,ba=0,bb=0,bx=0,by=0;\n", f);
    fputs("for(int ci=0;ci<W3dControlsCount();ci++){\n", f);
    fputs(" SDL_GameController* gc=(SDL_GameController*)W3dControlHandle(ci);if(!gc)continue;\n", f);
    fputs(" float ax=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_LEFTX)*M;\n", f);
    fputs(" float ay=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_LEFTY)*M;\n", f);
    fputs(" float bx2=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_RIGHTX)*M;\n", f);
    fputs(" float by2=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_RIGHTY)*M;\n", f);
    fputs(" if(ax<0?-ax>(lx<0?-lx:lx):ax>(lx<0?-lx:lx))lx=ax;\n", f);
    fputs(" if(ay<0?-ay>(ly<0?-ly:ly):ay>(ly<0?-ly:ly))ly=ay;\n", f);
    fputs(" if(bx2<0?-bx2>(rx<0?-rx:rx):bx2>(rx<0?-rx:rx))rx=bx2;\n", f);
    fputs(" if(by2<0?-by2>(ry<0?-ry:ry):by2>(ry<0?-ry:ry))ry=by2;\n", f);
    // los gatillos analogicos (pedales): gana el mas apretado de todos los mandos
    fputs(" float tl=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_TRIGGERLEFT)*M,tr=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)*M;\n", f);
    fputs(" if(tl>gl)gl=tl;if(tr>gr)gr=tr;\n", f);
    fputs(" du|=SDL_GameControllerGetButton(gc,SDL_CONTROLLER_BUTTON_DPAD_UP);\n", f);
    fputs(" dd|=SDL_GameControllerGetButton(gc,SDL_CONTROLLER_BUTTON_DPAD_DOWN);\n", f);
    fputs(" dl|=SDL_GameControllerGetButton(gc,SDL_CONTROLLER_BUTTON_DPAD_LEFT);\n", f);
    fputs(" dr|=SDL_GameControllerGetButton(gc,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);\n", f);
    fputs(" ba|=SDL_GameControllerGetButton(gc,SDL_CONTROLLER_BUTTON_A);\n", f);
    fputs(" bb|=SDL_GameControllerGetButton(gc,SDL_CONTROLLER_BUTTON_B);\n", f);
    fputs(" bx|=SDL_GameControllerGetButton(gc,SDL_CONTROLLER_BUTTON_X);\n", f);
    fputs(" by|=SDL_GameControllerGetButton(gc,SDL_CONTROLLER_BUTTON_Y);}\n", f);
    fputs("if(lx>-DZ&&lx<DZ)lx=0;if(ly>-DZ&&ly<DZ)ly=0;\n", f);
    fputs("if(rx>-DZ&&rx<DZ)rx=0;if(ry>-DZ&&ry<DZ)ry=0;\n", f);
    // EL D-PAD ENTRA COMO EL STICK IZQUIERDO (ver el comentario largo de SimGame.cpp):
    // con el analogico en reposo la cruceta manda ejes de exactamente 0/+-1, que es lo
    // que el juego reconoce como cruceta para aplicarle el modulo de las 8 direcciones.
    fputs("if(lx==0&&ly==0){lx=(float)(dr-dl);ly=(float)(dd-du);}\n", f);
    fputs("if(gl<DZ)gl=0;if(gr<DZ)gr=0;\n", f);
    fputs("W3dGameStick(0,lx,ly);W3dGameStick(1,rx,ry);W3dGameTrigger(0,gl);W3dGameTrigger(1,gr);\n", f);
    fputs("W3dGameButton(\"a\",ba!=0);W3dGameButton(\"b\",bb!=0);\n", f);
    fputs("W3dGameButton(\"x\",bx!=0);W3dGameButton(\"y\",by!=0);\n", f);
    fputs("W3dGameButton(\"up\",du!=0);W3dGameButton(\"down\",dd!=0);\n", f);
    fputs("W3dGameButton(\"left\",dl!=0);W3dGameButton(\"right\",dr!=0);}\n", f);
    fputs("#ifdef __ANDROID__\nif(g_acel){const float M=1.0f/32767.0f;SDL_JoystickUpdate();\n"
          "W3dScriptTilt(SDL_JoystickGetAxis(g_acel,0)*M,SDL_JoystickGetAxis(g_acel,1)*M,SDL_JoystickGetAxis(g_acel,2)*M);}\n"
          "#endif\n", f);
    fputs("Uint32 now=SDL_GetTicks();float dt=(now-g_lastMs)/1000.0f;g_lastMs=now;\n", f);
    fputs("if(dt<0)dt=0;if(dt>0.1f)dt=0.1f;\n", f);
    // (modo debug: cada 2 s el log dice los cuadros por segundo y cuanto tardan la logica, el dibujo y el swap)
    fputs("#if W3D_DEV_LOG\nUint64 m0=SDL_GetPerformanceCounter();\n#endif\n", f);
    fputs("#if W3D_DEV_LOG && defined(W3D_GAME_3D)\ntestLua();\n#endif\n", f);
    fputs("#if W3D_DEV_LOG && defined(__ANDROID__) && defined(__arm__)\nprofileFrame();\n#endif\n", f);
    // (modo debug, Android: si existe /data/local/tmp/w3d_salir el juego se cierra. Android 2.3 no tiene am force-stop:
    // asi una prueba arranca de cero sin reinstalar)
    fputs("#if W3D_DEV_LOG && defined(__ANDROID__)\n{static int nExit=0;"
          "if(++nExit%30==0&&access(\"/data/local/tmp/w3d_salir\",F_OK)==0){SDL_Quit();exit(0);}}\n#endif\n", f);
    fputs("W3dGameUpdate(dt);\n", f);
    // lua exit(): cerrar el juego (no en web). En Android hay que MATAR el proceso:
    // si solo se retorna, la Activity queda viva y al reabrir arranca con los globals
    // C++ sucios ("abre y se cierra"); exit() la proxima vez parte limpio.
    fputs("#ifndef __EMSCRIPTEN__\nif(W3dGameWantsStop()){\n", f);
    fputs("#ifdef __ANDROID__\nSDL_Quit();exit(0);\n#endif\n", f);
    fputs("g_run=false;return;}\n#endif\n", f);
    fputs("#ifdef __ANDROID__\nupdateSafeArea();\n#endif\n", f);
    fputs("#if W3D_DEV_LOG\nUint64 m1=SDL_GetPerformanceCounter();\n#endif\n", f);
    fputs("int w,h;currentSize(&w,&h);\n#ifdef W3D_CONSOLE\nW3dConsoleFrameBegin();\n#endif\n"
          "W3dGameRender(w,h);gfx::DrawPending();\n", f);
    fputs("#if W3D_DEV_LOG\nUint64 m2=SDL_GetPerformanceCounter(),mg=m2;\n"
          "static int measureGpu=-1;if(measureGpu<0){const char* g=getenv(\"W3D_MEASURE_GPU\");measureGpu=(g&&*g=='1')?1:0;}\n"
          "if(measureGpu){gfx::Finish();mg=SDL_GetPerformanceCounter();}\n#endif\n", f);
    fputs("#ifdef W3D_CONSOLE\nW3dConsoleFrameFinish(g_win);\n#else\n"
          "#ifdef W3D_VULKAN\nif(g_vk)VkGLPresent();else\n#endif\nSDL_GL_SwapWindow(g_win);\n#endif\n", f);
    fputs("#if W3D_DEV_LOG\n{Uint64 m3=SDL_GetPerformanceCounter();double k=1000.0/(double)SDL_GetPerformanceFrequency();\n", f);
    fputs("static double sa=0,sr=0,ss=0,s3=0,s2=0,sg=0;static int n=0;static Uint32 t0=0;\n", f);
    fputs("{float a3=0,a2=0;W3dGameTimes(&a3,&a2);s3+=a3;s2+=a2;}\n", f);
    fputs("sa+=(double)(m1-m0)*k;sr+=(double)(m2-m1)*k;sg+=(double)(mg-m2)*k;ss+=(double)(m3-mg)*k;n++;if(!t0)t0=now;\n", f);
    fputs("if(now-t0>=2000){const gfx::FrameStats& st=gfx::g_statFrame;\n", f);
    fputs("if(measureGpu)w3dLogf(\"[juego] esperando a la GPU antes del swap: %.1f ms\",sg/n);sg=0;\n", f);
    // (Linux/Android: cuanto de esos 2 s el proceso uso la CPU, todos sus hilos. Cerca de 100% = el cuadro es CPU, tambien
    // la del driver adentro del swap; menos = algo espera, la GPU o el compositor)
    fputs("#if defined(__linux__) && !defined(__EMSCRIPTEN__)\n{static long prev=-1;FILE* st=fopen(\"/proc/self/stat\",\"r\");"
          "if(st){char b[512];size_t k=fread(b,1,sizeof(b)-1,st);fclose(st);b[k]=0;const char* q=strrchr(b,')');\n"
          "unsigned long ut=0,stt=0;if(q&&sscanf(q+2,\"%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu\",&ut,&stt)==2){\n"
          "const long now=(long)(ut+stt);if(prev>=0)w3dLogf(\"[juego] CPU del proceso: %.0f%%\",(now-prev)*1000.0/(double)sysconf(_SC_CLK_TCK)/(now-t0)*100.0);"
          "prev=now;}}}\n#endif\n", f);
    fputs("#if defined(W3D_GAME_3D)\nw3dLogf(\"[juego] skinning %.1f ms (la pose %.1f ms), rutinas %.1f ms\","
          "g_skinTotalMs/n,g_skinPoseMs/n,g_routinesMs/n);g_skinPerfMs=0;g_skinTotalMs=g_skinPoseMs=0;g_routinesMs=0;\n#endif\n", f);
    fputs("w3dLogf(\"[juego] %.1f fps | logica %.1f ms, dibujo %.1f ms (3D %.1f, HUD %.1f), swap %.1f ms | \"\n"
          "\"draws %d (%.1f ms; el peor %.2f ms: %d indices, %d vertices%s), llamadas GL %d, tris %d\",n*1000.0/(now-t0),"
          "sa/n,sr/n,s3/n,s2/n,ss/n,st.draws,st.msDraws,st.msDrawMax,st.drawMaxIdx,st.drawMaxVerts,"
          "st.drawMaxVBO?\" VBO\":\"\",st.total,st.tris);sa=sr=ss=s3=s2=0;n=0;t0=now;}}\n#endif\n", f);
    fputs("}\n", f);
    fputs("int main(int,char**){\n", f);
    // (las consolas, antes que nada: sus hints, la carpeta de los assets como la actual y el log en la carpeta del juego)
    fprintf(f, "#ifdef W3D_CONSOLE\nW3dConsoleBeforeSDL(\"%s\");\n#endif\n", Esc(name).c_str());
    if (pack)   // ANTES de cargar nada: que icono/fuente/escenas ya salgan del pak
        fputs("w3dFileSystem::W3dPakRegister(g_w3dPakData,g_w3dPakSize);\n", f);
    fputs("#if !defined(__ANDROID__) && !defined(W3D_PSP) && !defined(W3D_3DS)\nSDL_SetMainReady();\n#endif\n", f);
    // dedos por separado (SDL_FINGER*), sin emular un mouse desde el touch: con la
    // emulacion 2 dedos pelean por el unico cursor y el multi-touch se rompe
    fputs("SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,\"0\");\n", f);
    // ORIENTACION (el desplegable "Orientacion" del editor): el hint le avisa a SDL en
    // Android/iOS que orientaciones se permiten. En desktop/web no aplica (no hace nada).
    if (orientation == 1) {
        fputs("// orientacion: solo vertical (Android/iOS; en desktop/web no aplica)\n", f);
        fputs("SDL_SetHint(SDL_HINT_ORIENTATIONS,\"Portrait\");\n", f);
    } else if (orientation == 2) {
        fputs("// orientacion: solo horizontal (Android/iOS; en desktop/web no aplica)\n", f);
        fputs("SDL_SetHint(SDL_HINT_ORIENTATIONS,\"LandscapeLeft LandscapeRight\");\n", f);
    }
    // ICONO EN EL DOCK (Linux): el app-id de la ventana debe coincidir con el
    // nombre del .desktop que instala el .deb (whisk3d-<juego>.desktop, ver
    // WriteCMake). Este SDL2 no tiene SDL_HINT_APP_ID (es de SDL3): Wayland
    // toma el app_id xdg del env SDL_VIDEO_WAYLAND_WMCLASS y X11 el WM_CLASS de
    // SDL_VIDEO_X11_WMCLASS; ambos ANTES de SDL_Init.
    fputs("#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)\n", f);
    fputs("// app-id = nombre del .desktop instalado: asi el dock muestra el icono\n", f);
    fprintf(f, "SDL_setenv(\"SDL_VIDEO_WAYLAND_WMCLASS\",\"whisk3d-%s\",1);\n", name.c_str());
    fprintf(f, "SDL_setenv(\"SDL_VIDEO_X11_WMCLASS\",\"whisk3d-%s\",1);\n", name.c_str());
    fputs("#endif\n", f);
    if (useSound) {
        fputs("SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO);\n", f);
        fputs("w3dEngine::W3dAudioInit(44100);// mixer del beep(); si el device no abre, sigue mudo\n", f);
    } else {
        fputs("SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER);\n", f);
    }
    fputs("#if W3D_DEV_LOG && defined(__ANDROID__)\n{FILE* fe=fopen(\"/data/local/tmp/w3d_env.txt\",\"r\");char l[512];\n"
          "while(fe&&fgets(l,sizeof(l),fe)){l[strcspn(l,\"\\r\\n\")]=0;char* q=strchr(l,'=');\n"
          "if(q&&l[0]!='#'){*q=0;setenv(l,q+1,1);w3dLogf(\"[juego] %s=%s\",l,q+1);}}\nif(fe)fclose(fe);}\n#endif\n", f);
    // ARM DE 32 BITS: el VFP en RunFast (los denormales a cero y NaN por defecto). En el modo IEEE de Android un NaN o un
    // denormal en una cuenta salta al soporte del kernel: medido en el ARM1136 del Galaxy Ace, 600 a 1300 ciclos por
    // operacion en vez de 5 a 20. Symbian ya corre asi. Es por hilo: el del juego (en el modo debug, W3D_FPU_IEEE=1 lo deja)
    fputs("#if defined(__arm__) && defined(__VFP_FP__) && !defined(__SOFTFP__)\n"
          "#if W3D_DEV_LOG\nif(!getenv(\"W3D_FPU_IEEE\"))\n#endif\n"
          "{unsigned x;__asm__ volatile(\"fmrx %0, fpscr\":\"=r\"(x));x|=(1u<<24)|(1u<<25);"
          "__asm__ volatile(\"fmxr fpscr, %0\"::\"r\"(x));}\n#endif\n", f);
    fputs("SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);\n", f);
    // ESTENCIL: el reflejo del espejo recorta con el (ver Mirror.cpp). Si el
    // driver no lo da, StencilBits()=0 y el reflejo se recorta por planos.
    fputs("SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);\n", f);
    // (modo debug: W3D_GL_ATRIB="r g b a profundidad estencil" pide otro formato de pantalla, para medir. El log dice el
    // que dio el driver)
    fputs("#if W3D_DEV_LOG\n{const char* a=getenv(\"W3D_GL_ATTRIB\");int v[6];\n"
          "if(a&&sscanf(a,\"%d %d %d %d %d %d\",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5])==6){\n"
          "SDL_GL_SetAttribute(SDL_GL_RED_SIZE,v[0]);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,v[1]);"
          "SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,v[2]);\nSDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,v[3]);"
          "SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,v[4]);SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,v[5]);}}\n#endif\n", f);
    // WebGL y Android: contexto GLES 2.0 (backend de shaders); desktop: COMPATIBILITY 2.1
    // (pipeline fijo; sin esto el driver puede dar un contexto CORE y no dibuja nada)
    // (Android con el pipeline fijo: OpenGL ES 1.1)
    fputs("#if defined(__ANDROID__)&&!defined(W3D_GLES2)&&!defined(W3D_MULTIBACKEND)\n"
          "SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);\n"
          "SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,1);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);\n"
          "#elif defined(__EMSCRIPTEN__)||defined(__ANDROID__)\n", f);
    fputs("SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);\n", f);
    fputs("SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);\n", f);
    fputs("#else\nSDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);\n", f);
    fputs("SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);\n#endif\n", f);
    // la VENTANA segun el "Modo ventana" del editor. En Web el modo NO aplica (el
    // tamano lo maneja el canvas de la pagina): se crea la ventana comun siempre.
    // El titulo de la ventana = el nombre del juego (el archivo .w3d del proyecto).
    std::string tit = Esc(name);
    // (las consolas: una sola pantalla, del tamano de la consola; el contexto de GL donde lo hay)
    fprintf(f, "#if defined(W3D_CONSOLE)\ng_win=W3dConsoleWindow(\"%s\");if(!g_win)return 2;\n", tit.c_str());
    fputs("#elif defined(__EMSCRIPTEN__)\n", f);
    fputs("// Web: el modo ventana no aplica (manda el canvas de la pagina)\n", f);
    fprintf(f, "g_win=SDL_CreateWindow(\"%s\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,600,600,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);\n", tit.c_str());
    fputs("#else\n", f);
    if (windowMode == 0) {
        // VENTANA: redimensionable, sin fullscreen (en Android queda la barra de estado)
        fputs("// modo Ventana: redimensionable, sin fullscreen\n", f);
        fprintf(f, "%sg_win=SDL_CreateWindow(\"%s\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,600,600,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);\n",
                WindowVk(tit, "600,600", "SDL_WINDOW_RESIZABLE").c_str(), tit.c_str());
    } else if (windowMode == 2) {
        // SIN BORDES: ventana al tamano de la UI y sin decoracion (en Android no hay
        // ventanas: va a pantalla completa igual que el modo fullscreen, mas abajo)
        fputs("// modo Sin bordes: tamano de la UI, sin decoracion\n", f);
        { char size[32]; snprintf(size, sizeof(size), "%d,%d", winW, winH);
          fprintf(f, "%sg_win=SDL_CreateWindow(\"%s\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,%d,%d,SDL_WINDOW_OPENGL|SDL_WINDOW_BORDERLESS);\n",
                  WindowVk(tit, size, "SDL_WINDOW_BORDERLESS").c_str(), tit.c_str(), winW, winH); }
    } else {
        // PANTALLA COMPLETA (default): fullscreen "desktop" (la resolucion del escritorio)
        fputs("// modo Pantalla completa: fullscreen desktop\n", f);
        fprintf(f, "%sg_win=SDL_CreateWindow(\"%s\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,600,600,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN_DESKTOP);\n",
                WindowVk(tit, "600,600", "SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN_DESKTOP").c_str(), tit.c_str());
    }
    fputs("#endif\n", f);
    fputs("#ifndef W3D_CONSOLE\n#ifdef W3D_VULKAN\nif(!g_vk)\n#endif\n"
          "{SDL_GLContext ctx=SDL_GL_CreateContext(g_win);if(!ctx)return 2;SDL_GL_SetSwapInterval(1);}\n#endif\n", f);
    // (modo debug: W3D_SIN_VSYNC=1 no espera el refresco: los cuadros por segundo dicen lo que cuesta un cuadro, sin
    // redondear a 60/n)
    fputs("#if W3D_DEV_LOG\nif(getenv(\"W3D_NO_VSYNC\"))w3dLogf(\"[juego] sin vsync: %d\",SDL_GL_SetSwapInterval(0));\n#endif\n", f);
    if (windowMode != 0) {
        // Android: pantalla completa INMERSIVA (oculta las barras de estado/navegacion).
        // "Sin bordes" tambien va por aca porque en Android no existen las ventanas.
        // En el modo Ventana NO se llama: el juego queda con la barra de estado visible.
        fputs("#ifdef __ANDROID__\nSDL_SetWindowFullscreen(g_win,SDL_WINDOW_FULLSCREEN_DESKTOP);\n#endif\n", f);
    }
    fputs("#if defined(W3D_GLES2)||defined(W3D_MULTIBACKEND)\ngfx::GLES2Init((void*(*)(const char*))SDL_GL_GetProcAddress);\n#endif\n", f);
    fputs("#if W3D_DEV_LOG\ngfx::g_measureDraws=true;   // (el medidor: el tiempo de los draws)\n"
          "{int r=0,g=0,b=0,a=0,z=0,e=0;SDL_GL_GetAttribute(SDL_GL_RED_SIZE,&r);SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE,&g);\n"
          "SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE,&b);SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE,&a);"
          "SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE,&z);SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE,&e);\n"
          "w3dLogf(\"[juego] pantalla GL: color %d%d%d%d, profundidad %d, estencil %d\",r,g,b,a,z,e);}\n#endif\n", f);
    fputs("#if W3D_DEV_LOG && defined(W3D_GAME_3D)\ng_skinClockMs=clockSkin;g_skinClockHeight=W3dNowMs;\n#endif\n", f);
    // ANDROID: los rangos visibles de una rutina separados por pocos triangulos van en un draw (ver g_routineHoleTris en
    // Routine.cpp): en un telefono cada draw le cuesta mucho mas al driver que esos triangulos a la GPU. En el modo debug,
    // W3D_HUECO_TRIS lo cambia (para medir)
    fputs("#if defined(__ANDROID__) && defined(W3D_GAME_3D)\n{extern int g_routineHoleTris;g_routineHoleTris=4096;\n"
          "#if W3D_DEV_LOG\nconst char* h=getenv(\"W3D_HOLE_TRIS\");if(h&&*h)g_routineHoleTris=atoi(h);\n#endif\n}\n#endif\n", f);
    if (withIcon) {
        // icono de la VENTANA (desktop): el icono.png generado del icono del proyecto
        fputs("#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)\n", f);
        fputs("{unsigned char* px=0;int iw=0,ih=0;\n", f);
        fputs("if(gfx::DecodeImage(\"icono.png\",&px,&iw,&ih)&&px){\n", f);
        fputs("SDL_Surface* s=SDL_CreateRGBSurfaceFrom(px,iw,ih,32,iw*4,"
              "0x000000FFu,0x0000FF00u,0x00FF0000u,0xFF000000u);\n", f);
        fputs("if(s){SDL_SetWindowIcon(g_win,s);SDL_FreeSurface(s);}\n", f);
        fputs("gfx::FreeImage(px);}}\n", f);
        fputs("#endif\n", f);
    }
    // Android: los assets van DENTRO del APK; el Core los lee con el AAssetManager
    // (se lo saca al Activity via JNI y se lo pasa al filesystem una sola vez)
    fputs("#ifdef __ANDROID__\n{\n", f);
    fputs("JNIEnv* env=(JNIEnv*)SDL_AndroidGetJNIEnv();\n", f);
    fputs("jobject act=(jobject)SDL_AndroidGetActivity();\n", f);
    fputs("if(env&&act){\n", f);
    fputs("jclass cl=env->GetObjectClass(act);\n", f);
    fputs("jmethodID mid=env->GetMethodID(cl,\"getAssets\",\"()Landroid/content/res/AssetManager;\");\n", f);
    fputs("jobject amJava=env->CallObjectMethod(act,mid);\n", f);
    fputs("AAssetManager* am=AAssetManager_fromJava(env,amJava);\n", f);
    fputs("w3dFileSystem::SetAssetManager(am);\n", f);
    fputs("env->DeleteLocalRef(amJava);env->DeleteLocalRef(cl);env->DeleteLocalRef(act);\n", f);
    fputs("}}\n#endif\n", f);
    // CONFIG persistente de los scripts (config.set/get de lua): necesita una carpeta
    // ESCRIBIBLE por plataforma (SDL_GetPrefPath; en web SetUserDataDir usa localStorage)
    // (las consolas: la suya -la carpeta del juego en la PSP, ux0:data en la Vita, la SD en la 3DS, la RAM en la
    // Dreamcast-; el SDL_GetPrefPath de la PSP arma dos carpetas una adentro de la otra y crea solo la ultima)
    fprintf(f, "#ifdef W3D_CONSOLE\nw3dFileSystem::SetUserDataDir(W3dConsoleData(\"%s\"));\n#else\n", tit.c_str());
    fprintf(f, "{char* pref=SDL_GetPrefPath(\"Whisk3D\",\"%s\");\n", tit.c_str());
    fputs("if(pref){w3dFileSystem::SetUserDataDir(pref);SDL_free(pref);}}\n#endif\n", f);
    fprintf(f, "w3dEngine::ConfigSetName(\"%s\");w3dEngine::ConfigLoad();\n", tit.c_str());
    // reforzar la semilla del random con entropia que varia entre corridas
    fputs("W3dScriptSeed((unsigned)time(0));\n", f);
    fprintf(f, "W3dGameFontPngSet(\"%s\");\n", Esc(fontRel).c_str());
    // CAPTURA de verificacion: con W3D_CAPTURA=<png> el juego guarda ese frame y cierra.
    // Es lo que usa la prueba de regresion para comprobar POR PIXELES que el juego
    // compilado dibuja de verdad su escena. Sin la variable no cambia nada.
    fputs("{const char* cap=getenv(\"W3D_CAPTURE\");\n", f);
    fputs("if(cap&&*cap){const char* nf=getenv(\"W3D_CAPTURE_FRAME\");\n", f);
    fputs("W3dGameCapture(cap,nf?atoi(nf):30);}}\n", f);
    if (with3D) {
        // LA ESCENA 3D del juego: el project.json que quedo al lado del binario. Va
        // ANTES de las escenas de UI y de W3dGameStart: los scripts (que en un juego 3D
        // cuelgan de los objetos de la escena) resuelven sus refs recien en el Inicio.
        // Si falla, el juego sigue (se ve el HUD sobre negro) y queda el motivo en el log.
        fputs("if(!W3dGameLoadScene3D(\"project.json\"))\n", f);
        fputs("  fprintf(stderr,\"[W3D] no pude cargar la escena 3D (project.json)\\n\");\n", f);
    }
    if (with3D) {
        // en 3D las escenas de UI las trae el propio project.json (nodo "ui" ->
        // scenes/<x>.w3dui, que el compilador dejo con la version viva), y los scripts
        // vienen colgados de sus objetos: no hay nada que cablear a mano aca.
    } else if (multi) {
        // MULTI-ESCENA: cargar TODAS las escenas (cada .w3dui trae sus scripts embebidos)
        // y elegir la inicial por su NOMBRE. NADA de W3dGameScript/W3dGameRef.
        for (size_t i = 0; i < scenes.size(); i++)
            fprintf(f, "if(!W3dGameLoadScene(\"%s\"))return 1;\n", Esc(scenes[i].file).c_str());
        fprintf(f, "W3dGameInitialScene(\"%s\");\n", Esc(initial).c_str());
    } else {
        // UNA sola escena / legacy: cargar la UI + agregar los scripts de AFUERA de ella y sus referencias (los de la
        // UI y sus elementos ya vienen dentro del .w3dui: W3dGameLoadUI los carga)
        fprintf(f, "if(!W3dGameLoadUI(\"%s\"))return 1;\n", Esc(w3duiRel).c_str());
        for (size_t o = 0; o < objs.size(); o++)
            for (size_t s = 0; s < objs[o].scripts.size() && !objs[o].inUI; s++) {
                // FORMATO v4: la ruta es el NOMBRE DE ENTRADA del contenedor
                // ("scripts/menu.lua") y el staging es su espejo, asi que va COMPLETA.
                // Aplanar por basename no alcanza: project/icono.png y
                // textures/icono.png pueden coexistir.
                fprintf(f, "W3dGameScript(\"%s\");\n", Esc(W3dIsEntryName(objs[o].scripts[s].path)
                            ? objs[o].scripts[s].path : Base(objs[o].scripts[s].path)).c_str());
                for (size_t r = 0; r < objs[o].scripts[s].refs.size(); r++)
                    fprintf(f, "W3dGameRef(\"%s\",\"%s\");\n",
                            Esc(objs[o].scripts[s].refs[r].first).c_str(),
                            Esc(objs[o].scripts[s].refs[r].second).c_str());
            }
    }
    // LOS MANDOS QUE YA ESTABAN ENCHUFADOS al arrancar. No alcanza con esperar el
    // SDL_CONTROLLERDEVICEADDED: si el juego arranca con el pad puesto, se abre
    // ACA (y si el evento llega igual, W3dControlAdd descarta el repetido).
    fputs("for(int i=0;i<SDL_NumJoysticks();i++)padOpen(i);\n", f);
    fputs("W3dScriptGamepadsVibrateHook=vibrateGamepads;\n", f);
    fputs("#ifdef __ANDROID__\nfor(int i=0;i<SDL_NumJoysticks()&&!g_acel;i++){const char* n=SDL_JoystickNameForIndex(i);\n"
          " if(n&&strstr(n,\"Accelerometer\")){g_acel=SDL_JoystickOpen(i);w3dLogf(\"[juego] acelerometro: %s\",n);}}\n"
          "#endif\n", f);
    fputs("W3dGameStart();g_lastMs=SDL_GetTicks();\n", f);
    fputs("#ifdef __EMSCRIPTEN__\nemscripten_set_main_loop(frame,0,1);\n#else\n", f);
    // ANDROID: al salir (la tecla de volver, SDL_QUIT) el proceso NO muere: la proxima vez que se abre el juego, main
    // corre en el mismo proceso con los globales del anterior (g_run en false: no dibujaba nada, pantalla negra). Se
    // termina el proceso, como cuando el juego pide pararse
    // (en una consola, lo suyo se cierra antes que SDL: la 3DS apaga citro3d antes de que SDL apague las pantallas)
    fputs("while(g_run)frame();\n#ifdef W3D_VULKAN\nif(g_vk)VkGLFinish();\n#endif\n"
          "#ifdef W3D_CONSOLE\nW3dConsoleEnd(g_win);\n#endif\n"
          "SDL_Quit();\n#ifdef __ANDROID__\nexit(0);\n#endif\n#endif\nreturn 0;}\n", f);
    bool okMain = (ferror(f) == 0);
    if (fclose(f) != 0) okMain = false;   // el flush pasa al cerrar: sin esto el main.cpp quedaba truncado
    return okMain;
}

// ----------------------------------------------------------------------------
//  PC (Linux): genera el CMakeLists.txt del juego. Antes se reusaba (por sed) el
//  CMake del ejemplo whiskpaddle, pero ese apunta a rutas relativas del ejemplo y
//  hardcodea SUS archivos -> no servia para un juego generado aparte. Aca se
//  genera uno propio: mismas fuentes del runtime que el ejemplo (incluye
//  W3dScene.cpp para multi-escena y Texture2D.cpp para imagenes), compila el
//  main.cpp GENERADO, y copia/instala TODAS las escenas (.w3dui) + scripts (.lua)
//  + texturas/fuente (.png) que quedaron en platform-build/linux (por glob, sin
//  nombres fijos).
//  W3DROOT lo pasa CompileGame con -DW3DROOT (la raiz del repo Whisk3D).
//  'usePhysics'/'useSound' (los checkboxes de la tarjeta): destildados se excluyen
//  las fuentes de ese subsistema y se ajustan las macros (build mas liviano).
//  'debugMode': define W3D_DEV_LOG=1 (log + ring + depurar()) o =0 (produccion:
//  sin mensajes de debug ni ring, mas liviano). Ver base/w3dlog.h.
//  'iconos': los tamanos hicolor generados en icons/ (vacio = sin icono): se
//  instalan en share/icons/hicolor y el .desktop los referencia (Icon=<nombre>).
//  'empaquetar': assets EMPAQUETADOS -> compila el pak.cpp generado y NO copia
//  ni instala ningun archivo del juego (el .deb queda binario+launcher+desktop+
//  iconos); en sueltos, todo como siempre.
// ============================================================================
//  LAS FUENTES DEL PASE 3D — UNA SOLA LISTA PARA LAS DOS PLATAFORMAS.
//
//  El CMake de PC y el Android.mk se generan en funciones distintas, y ya se
//  pago una vez el precio de tener la lista de fuentes DUPLICADA: faltaban
//  W3dRecursos/MeshData/TexturaCache en una de las dos y el juego no linkeaba.
//  Asi que las fuentes que agrega un juego 3D viven ACA, en un solo array, y los
//  dos generadores lo recorren. Si manana el pase 3D necesita otro .cpp, se
//  agrega en un solo lugar y las dos plataformas lo reciben.
//
//  Que son: el lector de proyecto (import_w3d, el MISMO del editor, compilado con
//  W3D_NO_EDITOR), los objetos de escena que el editor define fuera del Core
//  (camara, curva/riel, culling, LOD, visibilidad por celdas, coleccion/lote,
//  espejo, instancia, particulas...), la generacion de geometria (MeshEdit) y el
//  pase de render compartido (EscenaRender).
//
//  Las rutas van con ${CORE} / ${W3DROOT}, que los dos generadores ya definen.
// ============================================================================
// ============================================================================
//  LAS FUENTES COMUNES DEL RUNTIME — TAMBIEN UNA SOLA LISTA.
//
//  Antes esta lista estaba escrita DOS veces: una en el CMake que genera este
//  archivo (PC/web) y otra a mano en el jni/Android.mk de la plantilla. Se
//  desincronizaron: al Android.mk le faltaban W3dRecursos/MeshData/TexturaCache
//  desde que existen, o sea que el APK ni siquiera linkeaba. Ahora la lista es
//  ESTA y los dos generadores la recorren; el Android.mk del juego se GENERA
//  entero (ya no se parchea la plantilla del ejemplo con sed).
//
//  Lo que NO esta aca es lo que cambia por plataforma o por opcion: el main.cpp
//  generado, w3drun.cpp, el pak, el backend de graficos (GL vs GLES2), la fisica
//  y el audio (checkboxes), las fuentes 3D (kSources3D) y lua.
// ============================================================================
static const char* kSourcesBase[] = {
    "${CORE}/graphics/w3dTexture.cpp",
    "${CORE}/io/w3dFilesystem.cpp", "${CORE}/io/w3dCompress.cpp",
    // .w3db / .w3dm: la geometria de las mallas viaja en NUESTRO formato (binario lista para memoria,
    // o texto): el juego la lee con el MISMO lector que el editor
    "${CORE}/io/W3dText.cpp", "${CORE}/io/W3dMesh.cpp", "${CORE}/io/W3dMeshBin.cpp",
    "${CORE}/base/w3dlog.cpp", "${CORE}/base/W3dInteractionState.cpp", "${CORE}/base/W3dConfig.cpp",
    // (vibrate() de lua: el telefono, la web y la opcion "vibracion"; se instala solo)
    "${CORE}/input/W3dVibrate.cpp",
    "${CORE}/math/Vector3.cpp", "${CORE}/math/Quaternion.cpp", "${CORE}/math/Matrix4.cpp",
    "${CORE}/objects/Objects.cpp", "${CORE}/objects/Mesh.cpp", "${CORE}/objects/Light.cpp",
    "${CORE}/objects/Materials.cpp", "${CORE}/objects/Textures.cpp", "${CORE}/objects/CameraBase.cpp",
    "${CORE}/objects/Armature.cpp", "${CORE}/objects/RenderColors.cpp",
    "${CORE}/animation/Animation.cpp", "${CORE}/animation/VertexAnimation.cpp",
    // Armature2DAnimation.cpp va SIEMPRE: Animation.cpp lo llama sin ifdef (Arm2DActiveClip /
    // Armature2DEvaluate) y sin el .cpp el juego no linkea
    "${CORE}/animation/SkeletalAnimation.cpp", "${CORE}/animation/Armature2DAnimation.cpp",
    // W3dAnimSet.cpp va SIEMPRE con SkeletalAnimation.cpp: los clips de esqueleto se cargan de los
    // animsets (.w3da) y el Armature los suelta por ahi (sin el .cpp el juego no linkea)
    "${CORE}/animation/W3dAnimSet.cpp",
    // Flipbook.cpp va SIEMPRE: Mesh.cpp (UV anim por tira) y la Image2D del HUD (UI2DFormato/BindsJuego)
    // lo usan sin ifdef -> sin el .cpp ni un juego 2D ni uno 3D linkean
    "${CORE}/animation/Flipbook.cpp",
    // el almacen de recursos + datos compartidos van SIEMPRE: VertexAnimation.cpp,
    // Textures.cpp y el import los llaman sin ifdef (W3dResourceFind/Retener) ->
    // sin estos .cpp el juego compilado no linkea (visto en integracion_portales)
    "${CORE}/io/W3dResources.cpp", "${CORE}/objects/MeshData.cpp", "${CORE}/objects/TextureCache.cpp",
    // las mallas 3D como RECURSO (registro "mallas" del proyecto, una copia en memoria/GPU para
    // todos los objetos que la usan): Mesh.cpp la llama sin ifdef (vinculo, VBO compartidos)
    "${CORE}/objects/MeshResource.cpp",
    "${CORE}/objects/MeshFlows.cpp",          // las rutinas: tiras, aristas, vertices y quads simulados por parte
    // los SHADERS ESCRITOS de los materiales (y su seccion @vertex en la CPU para los backends sin shaders): Mesh.cpp y
    // Materials.cpp los llaman sin ifdef
    "${CORE}/objects/WrittenShader.cpp", "${CORE}/objects/ShaderCPU.cpp",
    "${CORE}/script/W3dScript.cpp",
    // W3dScript.cpp registra SIEMPRE los binds de los cuerpos rigidos (fisicaVel...) y de la colision
    // contra mallas (colSuelo/colPared...): sin estos dos el juego compilado no linkea
    "${CORE}/physics/W3dRigid.cpp", "${CORE}/physics/W3dColMesh.cpp",
    // ...y los de HITBOX (hitboxActivo/hitboxDentro/...) + el despacho de alEntrar/alTocar
    "${CORE}/physics/W3dHitbox.cpp",
    "${CORE}/net/W3dNet.cpp",
    "${UILIB}/text/W3dFont.cpp", "${UILIB}/text/bitmapText.cpp", "${UILIB}/text/font.cpp",
    "${UILIB}/draw/glesdraw.cpp", "${UILIB}/draw/W3dAtlasPacker.cpp",
    "${UILIB}/core/UI.cpp", "${UILIB}/theme/colors.cpp",
    "${W3DROOT}/main/render/UIOverlay.cpp", "${W3DROOT}/main/script/BindsGame.cpp",
    // Rutina va SIEMPRE: BindsJuego la usa sin ifdef (memorias, paso/setPaso). Los planos del paso "Test de
    // visibilidad" le llegan por hook desde Culling.cpp (solo 3D)
    "${W3DROOT}/main/objects/Routine.cpp",        // Rutina: lista de pasos del render (sin la validacion del editor)
    "${W3DROOT}/main/config/W3dProfile.cpp",      // W3dNowMs: el reloj de las rutinas (CloseRun)
    "${W3DROOT}/main/config/W3dLang.cpp",         // T(): los avisos del juego (un mando se conecta) en su idioma
    "${W3DROOT}/main/W3dScene.cpp",
    "${W3DROOT}/main/io/UI2DFormat.cpp", "${W3DROOT}/main/io/Font2D.cpp",
    "${W3DROOT}/main/io/Texture2D.cpp",
    0
};
// LOS INCLUDES comunes (misma razon: uno solo para las dos plataformas).
static const char* kIncludesBase[] = {
    "${W3DROOT}", "${W3DROOT}/libs", "${W3DROOT}/main", "${W3DROOT}/main/render", "${W3DROOT}/main/io",
    // (main/config y main/app tambien en un juego 2D: BindsJuego incluye W3dNotice.h -> W3dLang.h, y Rutina incluye
    // objects/Camera.h -> variables.h. Sin esto un juego solo 2D no compilaba)
    "${W3DROOT}/main/config", "${W3DROOT}/main/app",
    "${CORE}", "${CORE}/base", "${CORE}/graphics", "${CORE}/io", "${CORE}/math", "${CORE}/objects",
    "${CORE}/script", "${CORE}/animation", "${CORE}/thirdparty",
    "${UILIB}", "${UILIB}/text", "${UILIB}/draw", "${UILIB}/core", "${UILIB}/theme", "${UILIB}/widgets",
    0
};

static const char* kSources3D[] = {
    "${CORE}/objects/VisSet.cpp",              // sets de visibilidad por celda (PVS)
    "${CORE}/objects/VisBsp.cpp",              // arbol BSP + PVS por cluster (Culling metodo Bsp)
    "${CORE}/objects/WorldBsp.cpp",            // el MUNDO del .w3dbsp v2 (geometria por hoja y material, un draw por material)
    "${CORE}/io/W3dZip.cpp",                   // el .w3d es un zip (rutas/entradas del proyecto)
    "${CORE}/io/W3dStore.cpp",
    "${W3DROOT}/main/render/RenderOptions.cpp",  // g_renderBg + los flags de dibujo del Core
    "${W3DROOT}/main/render/SceneRender.cpp",    // EL pase 3D compartido con el viewport del editor
    "${W3DROOT}/main/render/RealTimeShadows.cpp",   // las sombras en tiempo real (el pase las arma antes de la escena)
    "${W3DROOT}/main/importers/import_w3d.cpp",   // EL MISMO lector de proyecto que el editor
    "${W3DROOT}/main/importers/import_obj.cpp",   // carga diferida de texturas + materiales
    "${W3DROOT}/main/importers/import_wobj.cpp",  // mallas .obj referenciadas (proyectos viejos)
    "${W3DROOT}/main/edit/MeshEdit.cpp",          // genera la malla de render (modificadores, bordes, PVS)
    "${W3DROOT}/main/edit/BooleanMod.cpp",        // BooleanPoly: el modificador booleano de Mesh_ApplyStack
    "${W3DROOT}/main/objects/EditMesh.cpp",
    "${W3DROOT}/main/objects/Primitives.cpp",
    "${W3DROOT}/main/objects/Camera.cpp",         // camara + riel (el encuadre del juego sale de aca)
    "${W3DROOT}/main/objects/Curve.cpp",
    "${W3DROOT}/main/objects/Culling.cpp",
    "${W3DROOT}/main/objects/LOD.cpp",
    "${W3DROOT}/main/objects/Fog.cpp",            // la niebla puesta en el arbol (solo en Render)
    "${W3DROOT}/main/objects/Sound.cpp",          // el audio puesto en la escena (objeto Sonido)
    "${W3DROOT}/main/objects/ClipRegion.cpp",     // ClearZ / Recorte (composicion de la pantalla)
    "${W3DROOT}/main/objects/PostProcessObj.cpp", // el objeto Postproceso (el arbol lo trae)
    "${W3DROOT}/main/render/PostProcess.cpp",     // su pila de efectos
    "${W3DROOT}/main/render/MotionVectors.cpp",  // el desenfoque de movimiento del postproceso (en la CPU)
    "${W3DROOT}/main/objects/Hitbox.cpp",            // Hitbox: caja de deteccion (el motor esta en el Core)
    "${W3DROOT}/main/objects/LightProbe.cpp",        // la sonda de luz: la luz indirecta horneada (el lector y el pase la usan)
    "${W3DROOT}/main/objects/ReflectionProbe.cpp",   // la sonda de reflejo: su imagen 360 (los materiales la usan)
    "${W3DROOT}/main/objects/VisZone.cpp",
    "${W3DROOT}/main/objects/Collection.cpp",
    "${W3DROOT}/main/objects/Mirror.cpp",
    "${W3DROOT}/main/objects/Instance.cpp",
    "${W3DROOT}/main/objects/Particles.cpp",
    "${W3DROOT}/main/objects/Empty.cpp",
    "${W3DROOT}/main/objects/Target.cpp",
    "${W3DROOT}/main/objects/Gamepad.cpp",
    "${W3DROOT}/main/objects/Scene.cpp",
    "${W3DROOT}/main/W3dRoots.cpp",              // las escenas 3D del proyecto: cambiarEscena() las carga/descarga
    "${W3DROOT}/main/io/W3dContainer.cpp",
    // volumen inicial por proyecto (`volumen:` del .w3d, lo aplica el lector): W3dVolumen reenvia el cambio
    // a la musica, asi que viaja con su dispatcher (sin W3D_ENABLE_MUSIC son stubs mudos)
    "${CORE}/audio/W3dVolume.cpp", "${CORE}/audio/W3dMusic.cpp",
    "${W3DROOT}/main/ui/W3dColors.cpp",
    "${W3DROOT}/main/config/w3dVersion.cpp",
    // (fase de prefabs) la INSTANCIA de prefab y la generacion de sus hijos + instanciar()/destruir() de lua
    "${W3DROOT}/main/objects/InstancePrefab.cpp",
    "${W3DROOT}/main/io/Prefabs.cpp",
    // (fase de librerias) el PROXY W3D (un prefab o una escena de una libreria externa) y el registro/lectura de
    // las librerias: sus prefijos "lib:" y sus recursos con nombre de libreria
    "${W3DROOT}/main/objects/ProxyW3d.cpp",
    "${W3DROOT}/main/io/Libraries.cpp",
    // (fase de streaming) las instancias DIFERIDAS: cargar/descargar en tiempo real segun la distancia
    "${W3DROOT}/main/io/Streaming.cpp",
    0
};
// LOS DIRECTORIOS DE INCLUDE que agrega el 3D (misma razon que arriba: uno solo).
static const char* kIncludes3D[] = {
    "${W3DROOT}/main/app", "${W3DROOT}/main/config", "${W3DROOT}/main/objects",
    "${W3DROOT}/main/edit", "${W3DROOT}/main/importers", "${W3DROOT}/main/script",
    "${W3DROOT}/main/ui", "${W3DROOT}/main/ui/ViewPorts", "${W3DROOT}/main/undo",
    0
};

// las listas de arriba usan ${CORE}/${UILIB}/${W3DROOT} (la sintaxis de CMake). El
// Android.mk usa $(CORE)/$(UILIB)/$(W3D). Esta es LA traduccion, para que las listas
// sigan siendo una sola.
// ...y el comando de emcc (web) usa $CORE/$UILIB/$W3D. Misma traduccion, mismas listas.
static std::string AEmcc(const char* path) {
    std::string out = path;
    struct { const char* of; const char* a; } m[] = {
        { "${W3DROOT}", "$W3D" }, { "${CORE}", "$CORE" }, { "${UILIB}", "$UILIB" }, { 0, 0 }
    };
    for (int k = 0; m[k].of; k++) {
        const std::string of = m[k].of, a = m[k].a;
        size_t p;
        while ((p = out.find(of)) != std::string::npos)
            out = out.substr(0, p) + a + out.substr(p + of.size());
    }
    return out;
}

static std::string AMk(const char* path) {
    std::string out = path;
    struct { const char* of; const char* a; } m[] = {
        { "${W3DROOT}", "$(W3D)" }, { "${CORE}", "$(CORE)" }, { "${UILIB}", "$(UILIB)" }, { 0, 0 }
    };
    for (int k = 0; m[k].of; k++) {
        const std::string of = m[k].of, a = m[k].a;
        size_t p;
        while ((p = out.find(of)) != std::string::npos)
            out = out.substr(0, p) + a + out.substr(p + of.size());
    }
    return out;
}

static bool WriteCMake(const std::string& path, const std::string& name,
                          bool usePhysics, bool useSound, bool debugMode,
                          const std::vector<int>& icons, bool pack, bool with3D, int backend) {
    const bool shaders = (backend == 1), vulkan = (backend == 2);
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs("# GENERADO por Whisk3D (Compilar juego). No hace falta editarlo.\n", f);
    fputs("cmake_minimum_required(VERSION 3.16)\n", f);
    fprintf(f, "project(%s CXX C)\n", name.c_str());
    fputs("set(CMAKE_CXX_STANDARD 17)\n", f);
    // OPTIMIZADO, como el editor y los otros sistemas (-O2 en el Makefile de Windows y las consolas, emcc -O2): sin
    // tipo de build, cmake compila SIN -O y el juego corria a -O0. FORCE pisa el "" que quedo en el cache de antes
    fputs("if(NOT CMAKE_BUILD_TYPE)\n  set(CMAKE_BUILD_TYPE Release CACHE STRING \"\" FORCE)\nendif()\n", f);
    fputs("if(NOT DEFINED W3DROOT)\n  message(FATAL_ERROR \"pasa -DW3DROOT=<raiz del repo Whisk3D>\")\nendif()\n", f);
    fputs("set(CORE ${W3DROOT}/libs/Whisk3DCore)\n", f);
    fputs("set(UILIB ${W3DROOT}/libs/WhiskUI)\n", f);
    fputs("set(GAME ${W3DROOT}/../Whisk3D-Examples/game)\n", f);
    fputs("set(LUA ${W3DROOT}/thirdparty/lua/src)\n", f);
    fputs("find_package(SDL2 REQUIRED)\nfind_package(OpenGL REQUIRED)\n", f);
    fputs("file(GLOB LUA_SRC ${LUA}/*.c)\nlist(REMOVE_ITEM LUA_SRC ${LUA}/lua.c ${LUA}/luac.c)\n", f);
    fprintf(f, "add_executable(%s\n", name.c_str());
    fputs("  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp\n  ${GAME}/w3drun.cpp\n", f);
    if (pack)   // el pak generado con todos los archivos del juego
        fputs("  ${CMAKE_CURRENT_SOURCE_DIR}/pak.cpp\n", f);
    // EL BACKEND GRAFICO (el desplegable de la tarjeta Juego): el de pipeline fijo (OpenGL) o el de shaders (el de la web
    // y Android, sobre un contexto GL 2.1: carga las funciones de GL 2.0 con GLES2Init y los shaders van en GLSL 1.20)
    // VULKAN: como el editor, los tres en el ejecutable (cada uno en su namespace) y el despacho al activo: arranca en
    // Vulkan y, donde no anda, sigue con OpenGL
    if (vulkan)
        fputs("  ${CORE}/graphics/opengl/w3dGraphics.cpp ${CORE}/graphics/gles2/w3dGraphicsGLES2.cpp\n"
              "  ${CORE}/graphics/vulkan/w3dGraphicsVk.cpp ${CORE}/graphics/vulkan/w3dVkGL.cpp\n"
              "  ${CORE}/graphics/w3dGraphicsDispatch.cpp\n", f);
    else if (shaders) fputs("  ${CORE}/graphics/gles2/w3dGraphicsGLES2.cpp\n", f);
    else              fputs("  ${CORE}/graphics/opengl/w3dGraphics.cpp\n", f);
    // el resto del runtime: kSourcesBase, la MISMA lista que usa el generador de Android
    for (int i = 0; kSourcesBase[i]; i++) fprintf(f, "  %s\n", kSourcesBase[i]);
    // fisica minima del Core (velocidad + rebotes): solo con el checkbox "Usar motor de
    // fisica" tildado; sin el, W3D_NO_PHYSICS deja los binds como stubs (en W3dScript.cpp)
    if (usePhysics) fputs("  ${CORE}/physics/W3dPhysics.cpp\n", f);
    // W3dAudio.cpp SIEMPRE (sin W3D_ENABLE_AUDIO es el dispatcher stub que hace linkear
    // a beep()); el backend SDL solo con el checkbox "Usar sonido" tildado
    if (useSound) fputs("  ${CORE}/audio/W3dAudio.cpp ${CORE}/audio/W3dAudioSDL.cpp\n", f);
    else            fputs("  ${CORE}/audio/W3dAudio.cpp\n", f);
    // JUEGO 3D: el lector de proyecto + los objetos de escena + el pase 3D (kSources3D,
    // la MISMA lista que usa el generador de Android). Un juego 2D no las compila.
    if (with3D)
        for (int i = 0; kSources3D[i]; i++) fprintf(f, "  %s\n", kSources3D[i]);
    fputs("  ${LUA_SRC}\n)\n", f);
    // W3D_DEV_LOG: 1 = modo debug (log + ring + depurar()); 0 = produccion (stubs no-op)
    // W3D_NO_EDITOR: el lector de proyecto compartido deja afuera lo que solo existe
    // con editor (layout de viewports, sesion, icono de ventana). Ver import_w3d.cpp.
    fprintf(f, "target_compile_definitions(%s PRIVATE W3D_OPENGL%s W3D_GAME_RUNTIME W3D_GAME_NO_TEX "
               "LUA_USE_POSIX W3D_STB_IMPL W3D_DEV_LOG=%d%s%s%s)\n", name.c_str(),
            vulkan ? " W3D_MULTIBACKEND W3D_VULKAN" : shaders ? " W3D_WEBGL W3D_GLES2" : "", debugMode ? 1 : 0,
            useSound ? " W3D_ENABLE_AUDIO" : "",
            usePhysics ? "" : " W3D_NO_PHYSICS",
            with3D ? " W3D_NO_EDITOR W3D_GAME_3D" : "");
    fprintf(f, "target_include_directories(%s PRIVATE\n  ${GAME}\n", name.c_str());
    for (int i = 0; kIncludesBase[i]; i++) fprintf(f, "  %s\n", kIncludesBase[i]);
    if (with3D)   // los headers del editor que el lector de proyecto compartido incluye
        for (int i = 0; kIncludes3D[i]; i++) fprintf(f, "  %s\n", kIncludes3D[i]);
    fputs("  ${LUA})\n", f);
    fprintf(f, "target_link_libraries(%s PRIVATE SDL2::SDL2 OpenGL::GL OpenGL::GLU)\n", name.c_str());
    if (vulkan) {
        // cada backend en su namespace; los headers de Vulkan vienen con el Core. shaderc (GLSL -> SPIR-V) va ADENTRO si
        // esta compilado (tools/preparar_shaderc.sh); si no, el juego lo busca instalado (libshaderc_shared) y sin el
        // dibuja con OpenGL
        fputs("set_source_files_properties(${CORE}/graphics/opengl/w3dGraphics.cpp PROPERTIES COMPILE_DEFINITIONS \"W3D_BACKEND_NS=gl\")\n"
              "set_source_files_properties(${CORE}/graphics/gles2/w3dGraphicsGLES2.cpp PROPERTIES COMPILE_DEFINITIONS \"W3D_BACKEND_NS=es2\")\n"
              "set_source_files_properties(${CORE}/graphics/vulkan/w3dGraphicsVk.cpp PROPERTIES COMPILE_DEFINITIONS \"W3D_BACKEND_NS=vk;W3D_VKGL\")\n"
              "set_source_files_properties(${CORE}/graphics/vulkan/w3dVkGL.cpp PROPERTIES INCLUDE_DIRECTORIES ${CORE}/thirdparty/vulkan/include)\n"
              "if(NOT DEFINED W3D_SHADERC)\n  set(W3D_SHADERC \"$ENV{HOME}/shaderc/linux/lib/libshaderc_combined.a\")\nendif()\n", f);
        fprintf(f, "if(EXISTS ${W3D_SHADERC})\n  target_compile_definitions(%s PRIVATE W3D_SHADERC_STATIC)\n"
                   "  target_link_libraries(%s PRIVATE ${W3D_SHADERC} pthread)\n"
                   "  set(CPACK_DEBIAN_PACKAGE_RECOMMENDS \"libvulkan1\")\nelse()\n"
                   "  message(WARNING \"Vulkan sin shaderc adentro (correr Whisk3D/tools/preparar_shaderc.sh linux): el juego usa "
                   "libshaderc_shared si esta instalada, sino OpenGL\")\n"
                   // (el .deb los recomienda: sin ellos el juego anda igual, con OpenGL)
                   "  set(CPACK_DEBIAN_PACKAGE_RECOMMENDS \"libvulkan1, libshaderc1\")\nendif()\n"
                   "target_link_libraries(%s PRIVATE ${CMAKE_DL_LIBS})\n", name.c_str(), name.c_str(), name.c_str());
    }
    // (los sockets de net/W3dNet.cpp: en Windows vienen aparte)
    fprintf(f, "if(WIN32)\n  target_link_libraries(%s PRIVATE ws2_32)\nendif()\n", name.c_str());
    if (!pack) {
        // TODOS los assets del juego (multi-escena) al lado del binario y en el paquete: por glob
        // (no por nombre) -> sirve para 1 o N escenas + las texturas/fuente que sea.
        fputs("file(GLOB GAME_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/*.w3dui "
              "${CMAKE_CURRENT_SOURCE_DIR}/*.lua ${CMAKE_CURRENT_SOURCE_DIR}/*.png "
              "${CMAKE_CURRENT_SOURCE_DIR}/*.json)\n", f);   // *.json = project.json (la escena 3D)
        fputs("foreach(A ${GAME_ASSETS})\n", f);
        fprintf(f, "  add_custom_command(TARGET %s POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "
                   "${A} $<TARGET_FILE_DIR:%s>)\n", name.c_str(), name.c_str());
        fputs("endforeach()\n", f);
        // LAS CARPETAS DE ASSETS (el espejo del contenedor: textures/, scripts/,
        // fonts/, meshes/... + las dos legacy assets/ y content/): cada una
        // viaja ENTERA y CON su carpeta al lado del binario Y al paquete, porque
        // los .w3dui las referencian con esa ruta relativa ("textures/pausa.png").
        // La lista es kFoldersAssets, la MISMA que usan el pak, Android y emcc.
        for (int c = 0; CarpAsset(c); c++) {
            const char* d = CarpAsset(c);
            fprintf(f, "if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/%s)\n", d);
            fprintf(f, "  add_custom_command(TARGET %s POST_BUILD COMMAND ${CMAKE_COMMAND} -E "
                       "copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/%s $<TARGET_FILE_DIR:%s>/%s)\n",
                    name.c_str(), d, name.c_str(), d);
            fprintf(f, "  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/%s DESTINATION lib/whisk3d-%s)\n",
                    d, name.c_str());
            fputs("endif()\n", f);
        }
    }
    // instalacion + paquete .deb (cpack -G DEB): binario (+ assets si van sueltos), con un
    // lanzador que hace cd a la carpeta de datos (el juego lee sus assets del directorio
    // actual; empaquetado lee del pak y el cd queda igual de inofensivo).
    fprintf(f, "install(TARGETS %s DESTINATION lib/whisk3d-%s)\n", name.c_str(), name.c_str());
    if (!pack)
        fprintf(f, "install(FILES ${GAME_ASSETS} DESTINATION lib/whisk3d-%s)\n", name.c_str());
    fprintf(f, "file(WRITE ${CMAKE_BINARY_DIR}/whisk3d-%s "
               "\"#!/bin/sh\\ncd /usr/lib/whisk3d-%s && exec ./%s \\\"$@\\\"\\n\")\n",
            name.c_str(), name.c_str(), name.c_str());
    fprintf(f, "install(PROGRAMS ${CMAKE_BINARY_DIR}/whisk3d-%s DESTINATION bin)\n", name.c_str());
    // MENU DE APLICACIONES: el .desktop (como el CMake artesanal del whiskpaddle;
    // sin esto el juego instalado no aparecia en el menu) + los iconos hicolor si
    // el proyecto tiene icono (Icon=<juego> resuelve contra hicolor/<n>x<n>/apps).
    fprintf(f, "file(WRITE ${CMAKE_BINARY_DIR}/whisk3d-%s.desktop\n", name.c_str());
    fprintf(f, "\"[Desktop Entry]\\nType=Application\\nName=%s\\n"
               "Comment=Juego hecho con el editor Whisk3D\\nExec=whisk3d-%s\\n"
               "Terminal=false\\nCategories=Game;\\n%s\")\n",
            name.c_str(), name.c_str(),
            icons.empty() ? "" : (std::string("Icon=") + name + "\\n").c_str());
    fprintf(f, "install(FILES ${CMAKE_BINARY_DIR}/whisk3d-%s.desktop DESTINATION share/applications)\n",
            name.c_str());
    for (size_t i = 0; i < icons.size(); i++)
        fprintf(f, "install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/%dx%d.png "
                   "DESTINATION share/icons/hicolor/%dx%d/apps RENAME %s.png)\n",
                icons[i], icons[i], icons[i], icons[i], name.c_str());
    fputs("set(CPACK_GENERATOR DEB)\n", f);
    fprintf(f, "set(CPACK_PACKAGE_NAME \"whisk3d-%s\")\n", name.c_str());
    fputs("set(CPACK_PACKAGE_VERSION \"1.0.0\")\n", f);
    fputs("set(CPACK_PACKAGE_CONTACT \"Whisk3D\")\n", f);
    fputs("set(CPACK_DEBIAN_PACKAGE_DEPENDS \"libsdl2-2.0-0\")\n", f);
    fputs("set(CPACK_PACKAGE_DESCRIPTION_SUMMARY \"Juego hecho con el editor Whisk3D\")\n", f);
    // nombre del ARTEFACTO: <juego>-linux-<arch> (ej whiskpaddle-linux-x86_64.deb).
    // Antes cpack armaba whisk3d-<nombre>-1.0.0-Linux.deb.
    fprintf(f, "set(CPACK_PACKAGE_FILE_NAME \"%s-linux-${CMAKE_SYSTEM_PROCESSOR}\")\n", name.c_str());
    fputs("include(CPack)\n", f);
    bool okCM = (ferror(f) == 0);
    if (fclose(f) != 0) okCM = false;     // idem: el CMakeLists generado no puede darse por bueno sin cerrar
    return okCM;
}

// ----------------------------------------------------------------------------
//  ANDROID: genera un script que arma el APK del juego reusando la PLANTILLA del
//  proyecto Android del ejemplo (Whisk3D-Examples/ui/whiskpaddle/platform/android:
//  jni/Android.mk + app/ gradle). El mecanismo es el mismo que build_android.sh
//  (ndk-build -> libmain.so -> jniLibs -> gradlew assembleDebug), pero adaptado a
//  ESTE juego: compila el main.cpp GENERADO (no el del whiskpaddle) y empaqueta los
//  assets generados (el .w3dui + los .lua + font.png) en vez de los del ejemplo.
//
//  Lo que TODAVIA falta para que ande end-to-end (documentado a proposito):
//   - Requiere el NDK: exporta ANDROID_NDK (o dejalo en ~/Android/ndk) y el SDK en
//     android/local.properties (sdk.dir). Sin eso el script corta con un error claro.
//   - La plantilla Android vive en Whisk3D-Examples (no en el repo Whisk3D): si no
//     esta al lado, el script avisa y no compila.
//
//  'nombre' (el slug del juego) PARAMETRIZA la COPIA por juego (sed sobre el gradle):
//   - applicationId com.whisk3d.<nombre>: cada juego es un APK distinto en el celular
//     (antes heredaba com.whisk3d.whiskpaddle y un juego PISABA al otro al instalar).
//   - label (strings.xml app_name) = <nombre>.
//   - el APK sale como <nombre>-android.apk en la ruta ESTANDAR de gradle
//     (app/build/outputs/apk/debug): se ANULAN el buildDir redirigido a distribution/
//     y el rename del outputFileName de la plantilla, que movian el APK a una ruta
//     que dependia del nombre de la carpeta (rootProject.name) y rompian la copia
//     final del worker (buscaba app-debug.apk y no existia).
//   PENDIENTE de branding: el namespace/paquete Java sigue siendo
//   com.whisk3d.whiskpaddle (la Activity vive ahi; cambiarlo pide mover los .java),
//   y el versionCode/versionName siguen los de la plantilla.
//
//  'orientacion' (0 Todas, 1 vertical, 2 horizontal): se parchea android:screenOrientation
//  en la COPIA del AndroidManifest (sed), como la calculadora que va clavada en portrait.
//  'usePhysics'/'useSound' destildados: sed sobre la COPIA del Android.mk excluye esas
//  fuentes y ajusta las macros (W3D_NO_PHYSICS / sin W3D_ENABLE_AUDIO).
//  'debugMode': sed agrega -DW3D_DEV_LOG=1/0 al LOCAL_CPPFLAGS de la COPIA (1 = log +
//  ring + depurar(); 0 = produccion sin nada de eso). Ver base/w3dlog.h.
//  'withIcon': reemplaza los ic_launcher de res/mipmap-* de la COPIA con los iconos
//  generados (icons/android-<dpi>.png) y asegura android:icon en el manifest.
//  'empaquetar': assets EMPAQUETADOS -> compila el pak.cpp generado (via sed sobre
//  la COPIA del Android.mk) y deja los assets del APK VACIOS (todo viaja dentro
//  de libmain.so); en sueltos, se copian como siempre.
static bool WriteBuildAndroid(const std::string& path, const std::string& repo,
                                 const std::string& out, const std::string& name,
                                 int orientation,
                                 bool usePhysics, bool useSound, bool debugMode,
                                 bool withIcon, bool pack, bool with3D, int backend) {
    std::string root = Folder(repo); // carpeta del ecosistema (contiene Whisk3D y Whisk3D-Examples)
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs("#!/usr/bin/env bash\n", f);
    fputs("# GENERADO por Whisk3D (Compilar juego -> Android). Arma el APK del juego.\n", f);
    fputs("set -euo pipefail\n", f);
    fprintf(f, "OUT=\"%s\"\n", Esc(out).c_str());
    fprintf(f, "ROOT=\"%s\"\n", Esc(root).c_str());
    fputs("TPL=\"$ROOT/Whisk3D-Examples/ui/whiskpaddle/platform/android\"\n", f);
    // la COPIA del proyecto Android, dentro de platform-build/android/. Se llamaba
    // "proyecto" y CHOCABA de frente con la carpeta project/ del contenedor v4 (la
    // del icono del juego), que se vuelca al staging con ese mismo nombre: el
    // volcado le escribia icono.png adentro del proyecto gradle. Por eso "androidprj".
    fputs("AND=\"$OUT/androidprj\"\n", f);
    // DONDE ESTA EL NDK. Se buscaba en UN solo lugar ($ANDROID_NDK o ~/Android/ndk),
    // y el NDK instalado desde el SDK Manager -- que es como lo instala todo el mundo
    // hoy -- vive en $ANDROID_HOME/ndk/<version>: el build moria con "ndk-build no
    // encontrado" en una maquina que tiene el NDK perfectamente instalado. Orden: la
    // variable explicita, la carpeta suelta de siempre, y despues la del SDK (la
    // version MAS NUEVA, que es la ultima de un `sort -V`).
    fputs("NDK=\"${ANDROID_NDK:-$HOME/Android/ndk}\"\n", f);
    fputs("if [ ! -x \"$NDK/ndk-build\" ]; then\n", f);
    fputs(" for SDKD in \"${ANDROID_HOME:-}\" \"${ANDROID_SDK_ROOT:-}\" \"$HOME/Android/Sdk\"; do\n", f);
    fputs("  [ -d \"$SDKD/ndk\" ] || continue\n", f);
    fputs("  CAND=$(ls -1 \"$SDKD/ndk\" 2>/dev/null | sort -V | tail -1)\n", f);
    fputs("  if [ -n \"$CAND\" ] && [ -x \"$SDKD/ndk/$CAND/ndk-build\" ]; then NDK=\"$SDKD/ndk/$CAND\"; break; fi\n", f);
    fputs(" done\n", f);
    fputs("fi\n", f);
    fputs("echo \"NDK: $NDK\"\n", f);
    // APK FAT por defecto (las mismas ABIs que jni/Application.mk); con argumento
    // compila SOLO esa ABI (ej. 'arm64-v8a' para iterar mas rapido a mano)
    fputs("ABIS=\"${1:-armeabi-v7a arm64-v8a}\"\n", f);
    fputs("[ -d \"$TPL\" ] || { echo \"ERROR: no existe la plantilla Android: $TPL\"; exit 1; }\n", f);
    fputs("[ -x \"$NDK/ndk-build\" ] || { echo \"ERROR: ndk-build no encontrado en '$NDK' (exporta ANDROID_NDK)\"; exit 1; }\n", f);
    // marcas "[W3D] <pct> <etapa>": las parsea el worker del editor para la barra de
    // progreso (a mano por consola son inofensivas: solo lineas de log)
    fputs("echo \"[W3D] 12 Copying the Android template\"\n", f);
    // 1) copiar la plantilla del proyecto Android (sin los artefactos de build:
    //    app/distribution y distribution son el buildDir redirigido de la plantilla
    //    y traen APKs VIEJOS del whiskpaddle que confundirian la copia final)
    fputs("rm -rf \"$AND\"; mkdir -p \"$AND\"\n", f);
    fputs("cp -r \"$TPL/.\" \"$AND/\"\n", f);
    fputs("rm -rf \"$AND/.gradle\" \"$AND/libs\" \"$AND/obj\" \"$AND/app/build\" \"$AND/app/src/main/jniLibs\" "
          "\"$AND/app/distribution\" \"$AND/distribution\"\n", f);
    // 1b) parametrizar la COPIA por juego + volver a la salida ESTANDAR de gradle:
    //  - fuera el buildDir redirigido a distribution/ (dependia de rootProject.name =
    //    el nombre de la CARPETA de la copia -> la ruta del APK no era predecible)
    //  - fuera el rename del outputFileName de la plantilla: el APK sale como
    //    <juego>-android.apk en app/build/outputs/apk/debug (de ahi copia el worker)
    //  - applicationId com.whisk3d.<juego>: instala JUNTO a los otros juegos (el
    //    namespace Java queda com.whisk3d.whiskpaddle: ahi vive la Activity)
    //  - label (app_name) = el nombre del juego
    fputs("sed -i '/buildDir = file(\"distribution/d' \"$AND/build.gradle\"\n", f);
    fprintf(f, "sed -i -e 's#applicationId \".*\"#applicationId \"com.whisk3d.%s\"#' "
               "-e 's#output.outputFileName = .*#output.outputFileName = \"%s-android.apk\"#' "
               "\"$AND/app/build.gradle\"\n", name.c_str(), name.c_str());
    fprintf(f, "sed -i 's#<string name=\"app_name\">.*</string>#<string name=\"app_name\">%s</string>#' "
               "\"$AND/app/src/main/res/values/strings.xml\"\n", name.c_str());
    // 2) apuntar el build nativo al main.cpp GENERADO y las rutas absolutas del ecosistema
    fputs("cp \"$OUT/main.cpp\" \"$AND/jni/main_generado.cpp\"\n", f);
    // (el main del ejemplo vive en platform/main.cpp desde la limpieza de la
    // raiz de los proyectos: el sed reemplaza ESA ruta por el main generado)
    if (pack)   // assets empaquetados: el pak.cpp generado entra al build junto al main
        fputs("cp \"$OUT/pak.cpp\" \"$AND/jni/pak_generado.cpp\"\n", f);
    // 2b) EL Android.mk DEL JUEGO SE GENERA ENTERO (antes se parcheaba con sed el de la
    //     plantilla del ejemplo, y esa lista de fuentes se desincronizo de la del CMake:
    //     le faltaban W3dRecursos/MeshData/TexturaCache y el APK no linkeaba). Sale de
    //     kSourcesBase/kSources3D/kIncludesBase/kIncludes3D, las MISMAS que el CMake de PC.
    fputs("cat > \"$AND/jni/Android.mk\" <<'W3DMK'\n", f);
    fputs("LOCAL_PATH := $(call my-dir)\nMY_PATH := $(LOCAL_PATH)\n", f);
    fputs("# GENERADO por Whisk3D (Compilar juego -> Android). No hace falta editarlo.\n", f);
    fputs("ROOT  := $(W3DROOTABS)\n", f);   // lo pisa el sed de abajo con la ruta real
    fputs("W3D   := $(ROOT)/Whisk3D\nEXAMP := $(ROOT)/Whisk3D-Examples\n", f);
    fputs("CORE  := $(W3D)/libs/Whisk3DCore\nUILIB := $(W3D)/libs/WhiskUI\n", f);
    fputs("include $(W3D)/thirdparty/SDL2/Android.mk\n", f);
    fputs("LOCAL_PATH := $(MY_PATH)\ninclude $(CLEAR_VARS)\n", f);
    // "main" por convencion de SDLActivity (System.loadLibrary("main") -> libmain.so)
    fputs("LOCAL_MODULE := main\n", f);
    fputs("SRC_FILES := \\\n  $(MY_PATH)/main_generado.cpp \\\n", f);
    if (pack) fputs("  $(MY_PATH)/pak_generado.cpp \\\n", f);
    fputs("  $(EXAMP)/game/w3drun.cpp \\\n", f);
    // EL BACKEND GRAFICO: OpenGL ES 1.1 (el pipeline fijo), OpenGL ES 2.0 (los shaders) o Vulkan. Con Vulkan van los tres,
    // como en la PC (graphics/w3dBackend.h), y si el telefono no lo tiene sigue con ES 2.0. Cada uno en su namespace: lo
    // ponen los backend_*.cpp que escribe este script (ndk-build no tiene flags por archivo)
    if (backend == 2)
        fputs("  $(MY_PATH)/backend_gl.cpp $(MY_PATH)/backend_es2.cpp $(MY_PATH)/backend_vk.cpp \\\n"
              "  $(CORE)/graphics/vulkan/w3dVkGL.cpp $(CORE)/graphics/w3dGraphicsDispatch.cpp \\\n", f);
    else if (backend == 0) fputs("  $(CORE)/graphics/opengl/w3dGraphics.cpp \\\n", f);
    else                   fputs("  $(CORE)/graphics/gles2/w3dGraphicsGLES2.cpp \\\n", f);
    for (int i = 0; kSourcesBase[i]; i++) fprintf(f, "  %s \\\n", AMk(kSourcesBase[i]).c_str());
    if (usePhysics) fputs("  $(CORE)/physics/W3dPhysics.cpp \\\n", f);
    if (useSound) fputs("  $(CORE)/audio/W3dAudio.cpp $(CORE)/audio/W3dAudioSDL.cpp \\\n", f);
    else            fputs("  $(CORE)/audio/W3dAudio.cpp \\\n", f);
    if (with3D)
        for (int i = 0; kSources3D[i]; i++) fprintf(f, "  %s \\\n", AMk(kSources3D[i]).c_str());
    fputs("  $(filter-out %/lua.c %/luac.c,$(wildcard $(W3D)/thirdparty/lua/src/*.c))\n", f);
    // ndk-build quiere LOCAL_SRC_FILES relativo a LOCAL_PATH (acepta '..')
    fputs("LOCAL_SRC_FILES := $(patsubst $(MY_PATH)/%,%,$(SRC_FILES))\n", f);
    fputs("LOCAL_C_INCLUDES := \\\n  $(MY_PATH)/shim \\\n  $(EXAMP)/game \\\n", f);
    for (int i = 0; kIncludesBase[i]; i++) fprintf(f, "  %s \\\n", AMk(kIncludesBase[i]).c_str());
    if (with3D)
        for (int i = 0; kIncludes3D[i]; i++) fprintf(f, "  %s \\\n", AMk(kIncludes3D[i]).c_str());
    fputs("  $(W3D)/thirdparty/lua/src \\\n  $(W3D)/thirdparty/SDL2/include", f);
    fputs(backend == 2 ? " \\\n  $(CORE)/thirdparty/vulkan/include\n" : "\n", f);
    fputs("LOCAL_CPP_FEATURES := exceptions rtti\n", f);
    // lua (C): Android es linux -> posix. En armeabi-v7a el NDK 27 fuerza
    // -D_FILE_OFFSET_BITS=64 y fseeko/ftello recien existen en API 24 (aca es 21), asi
    // que liolib.c no compila: se fuerzan las macros de lua a fseek/ftell (long).
    fputs("LOCAL_CFLAGS   := -DLUA_USE_POSIX \\\n", f);
    fputs("  '-Dl_fseek(f,o,w)=fseek(f,o,w)' '-Dl_ftell(f)=ftell(f)' -Dl_seeknum=long\n", f);
    fprintf(f, "LOCAL_CPPFLAGS := -std=c++17 %s -DW3D_GAME_RUNTIME -DW3D_GAME_NO_TEX "
               "-DW3D_STB_IMPL -DW3D_DEV_LOG=%d%s%s%s\n",
            backend == 2 ? "-DW3D_MULTIBACKEND -DW3D_VULKAN" : backend == 0 ? "-DW3D_GLES1" : "-DW3D_GLES2",
            debugMode ? 1 : 0,
            useSound ? " -DW3D_ENABLE_AUDIO" : "",
            usePhysics ? "" : " -DW3D_NO_PHYSICS",
            with3D ? " -DW3D_NO_EDITOR -DW3D_GAME_3D" : "");
    fprintf(f, "LOCAL_SHARED_LIBRARIES := SDL2\nLOCAL_LDLIBS := %s -llog -landroid\n",
            backend == 2 ? "-lGLESv1_CM -lGLESv2 -lEGL" : backend == 0 ? "-lGLESv1_CM -lEGL" : "-lGLESv2");
    if (backend == 2)
        // shaderc (GLSL -> SPIR-V) ADENTRO si esta compilado para el ABI (tools/preparar_shaderc.sh android); si no, se busca
        // libshaderc_shared.so en el APK y sin ella el juego dibuja con OpenGL ES 2.0
        fputs("SHADERC := $(HOME)/shaderc/android-$(TARGET_ARCH_ABI)/lib/libshaderc_combined.a\n"
              "ifneq ($(wildcard $(SHADERC)),)\nLOCAL_CPPFLAGS += -DW3D_SHADERC_STATIC\nLOCAL_LDFLAGS += $(SHADERC)\nelse\n"
              "$(warning Vulkan sin shaderc adentro para $(TARGET_ARCH_ABI): correr Whisk3D/tools/preparar_shaderc.sh android)\n"
              "endif\n", f);
    fputs("include $(BUILD_SHARED_LIBRARY)\nW3DMK\n", f);
    if (backend == 2) {
        // los tres backends, cada uno en su namespace (graphics/w3dBackend.h)
        fputs("printf '#define W3D_BACKEND_NS gl\\n#include \"graphics/opengl/w3dGraphics.cpp\"\\n' > \"$AND/jni/backend_gl.cpp\"\n"
              "printf '#define W3D_BACKEND_NS es2\\n#include \"graphics/gles2/w3dGraphicsGLES2.cpp\"\\n' > \"$AND/jni/backend_es2.cpp\"\n"
              "printf '#define W3D_BACKEND_NS vk\\n#define W3D_VKGL\\n#include \"graphics/gles2/w3dGraphicsGLES2.cpp\"\\n' "
              "> \"$AND/jni/backend_vk.cpp\"\n", f);
    }
    // la raiz del ecosistema, con la ruta REAL (el heredoc va sin expandir a proposito)
    fputs("sed -i \"s#^ROOT  := .*#ROOT  := $ROOT#\" \"$AND/jni/Android.mk\"\n", f);
    // 2b'') ICONO del launcher: los android-<dpi>.png generados reemplazan los
    // ic_launcher (y _round) de la COPIA. Si la plantilla no trae los mipmap-* se
    // crean, y si el manifest no referencia android:icon se agrega por sed.
    if (withIcon) {
        fputs("# icono del launcher: los generados del icono del proyecto\n", f);
        fputs("for D in mdpi hdpi xhdpi xxhdpi xxxhdpi; do\n", f);
        fputs("  [ -f \"$OUT/icons/android-$D.png\" ] || continue\n", f);
        fputs("  mkdir -p \"$AND/app/src/main/res/mipmap-$D\"\n", f);
        fputs("  cp \"$OUT/icons/android-$D.png\" \"$AND/app/src/main/res/mipmap-$D/ic_launcher.png\"\n", f);
        fputs("  cp \"$OUT/icons/android-$D.png\" \"$AND/app/src/main/res/mipmap-$D/ic_launcher_round.png\"\n", f);
        fputs("done\n", f);
        fputs("grep -q 'android:icon=' \"$AND/app/src/main/AndroidManifest.xml\" || "
              "sed -i 's#<application #<application android:icon=\"@mipmap/ic_launcher\" #' "
              "\"$AND/app/src/main/AndroidManifest.xml\"\n", f);
    }
    // 2b''') la red (net/W3dNet.cpp): el permiso INTERNET en la COPIA del manifest (es "normal": no se pide en runtime)
    fputs("grep -q 'android.permission.INTERNET' \"$AND/app/src/main/AndroidManifest.xml\" || "
          "sed -i 's#<application #<uses-permission android:name=\"android.permission.INTERNET\" />\\n    <application #' "
          "\"$AND/app/src/main/AndroidManifest.xml\"\n", f);
    // 2c) orientacion clavada: android:screenOrientation en la COPIA del manifest (la
    // referencia es la calculadora, que fija portrait en su manifest + hint del main)
    if (orientation == 1 || orientation == 2) {
        fprintf(f, "# orientacion: %s\n", (orientation == 1) ? "only vertical (portrait)"
                                                             : "only horizontal (landscape)");
        fprintf(f, "sed -i 's#android:launchMode=\"singleTask\"#android:launchMode=\"singleTask\" "
                   "android:screenOrientation=\"%s\"#' \"$AND/app/src/main/AndroidManifest.xml\"\n",
                (orientation == 1) ? "portrait" : "landscape");
    }
    if (pack) {
        // 3) assets EMPAQUETADOS: el APK va SIN assets visibles (todo viaja dentro
        //    de libmain.so via pak_generado.cpp); se limpia por si quedo una copia vieja
        fputs("ASSETS=\"$AND/app/src/main/assets\"; rm -rf \"$ASSETS\"; mkdir -p \"$ASSETS\"\n", f);
    } else {
        // 3) assets del APK: el juego GENERADO (no los del whiskpaddle). TODAS las escenas
        //    (.w3dui) + TODOS los scripts (.lua) + las texturas y la fuente (.png, incluye font.png).
        fputs("ASSETS=\"$AND/app/src/main/assets\"; rm -rf \"$ASSETS\"; mkdir -p \"$ASSETS\"\n", f);
        fputs("cp \"$OUT\"/*.w3dui \"$ASSETS\"/ 2>/dev/null || true\n", f);
        fputs("cp \"$OUT\"/*.lua   \"$ASSETS\"/ 2>/dev/null || true\n", f);
        fputs("cp \"$OUT\"/*.png   \"$ASSETS\"/ 2>/dev/null || true\n", f);
        // project.json = la ESCENA 3D del juego (un juego 2D simplemente no lo tiene)
        fputs("cp \"$OUT\"/*.json  \"$ASSETS\"/ 2>/dev/null || true\n", f);
        // LAS CARPETAS DE ASSETS (kFoldersAssets: el espejo del contenedor +
        // assets/ y content/ de los proyectos v3) van CON su subdirectorio: el
        // loader de Android resuelve "textures/pausa.png" tal cual contra el asset
        // root del APK. Antes solo se copiaban assets/ y content/, asi que un
        // proyecto v4 llegaba al celular sin sus texturas ni sus scripts.
        for (int c = 0; CarpAsset(c); c++)
            fprintf(f, "if [ -d \"$OUT/%s\" ]; then mkdir -p \"$ASSETS/%s\"; "
                       "cp -r \"$OUT/%s/.\" \"$ASSETS/%s/\"; fi\n",
                    CarpAsset(c), CarpAsset(c),
                    CarpAsset(c), CarpAsset(c));
    }
    // 4) compilar: ndk-build (libmain.so, TODAS las ABIs) -> jniLibs -> gradlew assembleDebug
    fputs("echo \"[W3D] 16 ndk-build ($ABIS)\"\n", f);
    // -j ACOTADO (antes era $(nproc), y encima por CADA ABI): ver W3dCompileJobs
    fprintf(f, "\"$NDK/ndk-build\" %s APP_ABI=\"$ABIS\" NDK_PROJECT_PATH=\"$AND\" "
               "APP_BUILD_SCRIPT=\"$AND/jni/Android.mk\" NDK_APPLICATION_MK=\"$AND/jni/Application.mk\"\n",
            W3dCompileJobsFlag().c_str());
    fputs("echo \"[W3D] 58 Copying libmain.so (jniLibs)\"\n", f);
    fputs("for A in $ABIS; do\n", f);
    fputs("  JNILIBS=\"$AND/app/src/main/jniLibs/$A\"; mkdir -p \"$JNILIBS\"\n", f);
    fputs("  cp \"$AND/libs/$A\"/*.so \"$JNILIBS\"/\n", f);
    fputs("done\n", f);
    fputs("echo \"[W3D] 60 Gradle (assembleDebug)\"\n", f);
    fputs("cd \"$AND\" && ./gradlew assembleDebug\n", f);
    bool okSh = (ferror(f) == 0);
    if (fclose(f) != 0) okSh = false;     // idem: el script de build tiene que quedar COMPLETO
    return okSh;
}

// ----------------------------------------------------------------------------
//  ANDROID 2.3 (plataforma 6): el APK del juego para los telefonos de Android 2.3 (API 10, ARMv6: el Galaxy Ace),
//  con la cadena de platform/android-viejo: el NDK r15c y SDL 2.0.7 (en ~/android-viejo, o W3D_ANDROID_OLD: los
//  deja preparar_android_viejo.sh), el Java con javac + d8 y el APK armado a mano con aapt (sin Gradle), firmado con
//  la llave de debug. Las fuentes son LAS MISMAS listas que los otros destinos (kSourcesBase...), con lo propio del
//  ARMv6 de platform/android-viejo/jni/Android.mk: instrucciones ARM con la FPU (VFP) y el shim de lo que SDL 2.0.7
//  no tiene (-include w3d_sdl207.h). Si hay un telefono conectado (adb), lo instala y lo abre.
//  Lo compilado (obj23/) queda entre compilaciones: la segunda solo rehace lo que cambio.
// ----------------------------------------------------------------------------
static bool WriteBuildAndroidOld(const std::string& path, const std::string& repo,
                                      const std::string& out, const std::string& name,
                                      int orientation, bool usePhysics, bool useSound, bool debugMode,
                                      bool withIcon, bool pack, bool with3D) {
    std::string root = Folder(repo);
    // el paquete de Java (y del APK): com.whisk3d.<juego>, que no empiece con un numero
    std::string pkg = name;
    if (pkg.empty() || (pkg[0] >= '0' && pkg[0] <= '9')) pkg = "j" + pkg;
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs("#!/usr/bin/env bash\n", f);
    fputs("# GENERADO por Whisk3D (Compilar juego -> Android 2.3). Arma el APK del juego para Android 2.3 (ARMv6).\n", f);
    fputs("set -euo pipefail\n", f);
    fprintf(f, "OUT=\"%s\"\n", Esc(out).c_str());
    fprintf(f, "ROOT=\"%s\"\n", Esc(root).c_str());
    fprintf(f, "NAME=\"%s\"\nPKG=\"com.whisk3d.%s\"\n", name.c_str(), pkg.c_str());
    fputs("W3D=\"$ROOT/Whisk3D\"\n", f);
    fputs("OLD=\"${W3D_ANDROID_OLD:-$HOME/android-viejo}\"\n", f);
    fputs("NDK=\"$OLD/android-ndk-r15c\"\nSDLD=\"$OLD/fuentes/SDL-release-2.0.7\"\n", f);
    fputs("SDK=\"${ANDROID_HOME:-$HOME/Android/Sdk}\"\n", f);
    fputs("BT=\"$(ls -d \"$SDK\"/build-tools/* 2>/dev/null | sort -V | tail -1)\"\n", f);
    fputs("JAR=\"$(ls -d \"$SDK\"/platforms/android-*/android.jar 2>/dev/null | sort -V | tail -1)\"\n", f);
    fputs("KS=\"${W3D_KEYSTORE:-$HOME/.android/debug.keystore}\"\n", f);
    fputs("AND=\"$OUT/androidviejo\"\nB=\"$OUT/obj23\"\n", f);
    fputs("[ -x \"$NDK/ndk-build\" ] && [ -f \"$SDLD/Android.mk\" ] || { echo \"ERROR: falta el NDK r15c o SDL 2.0.7 en "
          "$OLD (correr Whisk3D/platform/android-viejo/preparar_android_viejo.sh)\"; exit 1; }\n", f);
    fputs("[ -x \"$BT/aapt\" ] && [ -f \"$JAR\" ] || { echo \"ERROR: faltan las build-tools o android.jar en $SDK\"; exit 1; }\n", f);
    // (el clang del r15c pide libncurses.so.5: preparar_android_viejo.sh deja unos enlaces en host/lib)
    fputs("export LD_LIBRARY_PATH=\"$OLD/host/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"\n", f);
    fputs("echo \"[W3D] 12 Preparing the Android 2.3 project\"\n", f);
    fputs("rm -rf \"$AND\"; mkdir -p \"$AND/jni\" \"$AND/java\" \"$AND/res/drawable\" \"$AND/assets\" \"$B\"\n", f);
    fputs("cp \"$OUT/main.cpp\" \"$AND/jni/main_generado.cpp\"\n", f);
    if (pack) fputs("cp \"$OUT/pak.cpp\" \"$AND/jni/pak_generado.cpp\"\n", f);
    fputs("cp \"$W3D/platform/android-viejo/jni/Application.mk\" \"$AND/jni/\"\n", f);
    // el Android.mk: SDL 2.0.7 (su propio Android.mk) y el juego (libmain.so) con las listas de siempre
    fputs("cat > \"$AND/jni/Android.mk\" <<'W3DMK'\n", f);
    fputs("LOCAL_PATH := $(call my-dir)\nMY_PATH := $(LOCAL_PATH)\n", f);
    fputs("# GENERADO por Whisk3D (Compilar juego -> Android 2.3). No hace falta editarlo.\n", f);
    fputs("ROOT  := $(W3DROOTABS)\n", f);
    fputs("W3D   := $(ROOT)/Whisk3D\nEXAMP := $(ROOT)/Whisk3D-Examples\n", f);
    fputs("CORE  := $(W3D)/libs/Whisk3DCore\nUILIB := $(W3D)/libs/WhiskUI\nOLD := $(W3D)/platform/android-viejo\n", f);
    fputs("include $(SDL_DIR)/Android.mk\n", f);
    fputs("LOCAL_PATH := $(MY_PATH)\ninclude $(CLEAR_VARS)\nLOCAL_MODULE := main\n", f);
    fputs("LOCAL_SRC_FILES := \\\n  $(MY_PATH)/main_generado.cpp \\\n", f);
    if (pack) fputs("  $(MY_PATH)/pak_generado.cpp \\\n", f);
    fputs("  $(EXAMP)/game/w3drun.cpp \\\n  $(CORE)/graphics/gles2/w3dGraphicsGLES2.cpp \\\n", f);
    for (int i = 0; kSourcesBase[i]; i++) fprintf(f, "  %s \\\n", AMk(kSourcesBase[i]).c_str());
    if (usePhysics) fputs("  $(CORE)/physics/W3dPhysics.cpp \\\n", f);
    if (useSound) fputs("  $(CORE)/audio/W3dAudio.cpp $(CORE)/audio/W3dAudioSDL.cpp \\\n", f);
    else            fputs("  $(CORE)/audio/W3dAudio.cpp \\\n", f);
    if (with3D)
        for (int i = 0; kSources3D[i]; i++) fprintf(f, "  %s \\\n", AMk(kSources3D[i]).c_str());
    fputs("  $(filter-out %/lua.c %/luac.c,$(wildcard $(W3D)/thirdparty/lua/src/*.c))\n", f);
    // (el shim de android-viejo primero: su <filesystem> le gana al que falta en el r15c)
    fputs("LOCAL_C_INCLUDES := \\\n  $(OLD)/jni/shim \\\n  $(W3D)/platform/android/jni/shim \\\n  $(EXAMP)/game \\\n", f);
    for (int i = 0; kIncludesBase[i]; i++) fprintf(f, "  %s \\\n", AMk(kIncludesBase[i]).c_str());
    if (with3D)
        for (int i = 0; kIncludes3D[i]; i++) fprintf(f, "  %s \\\n", AMk(kIncludes3D[i]).c_str());
    fputs("  $(W3D)/thirdparty/lua/src \\\n  $(SDL_DIR)/include\n", f);
    fputs("LOCAL_CPP_FEATURES := exceptions rtti\n", f);
    // instrucciones ARM (no Thumb: el de 16 bits del ARMv6 no tiene la FPU) y la FPU VFP del ARM11
    fputs("LOCAL_ARM_MODE := arm\n", f);
    fputs("LOCAL_CFLAGS := -O2 -march=armv6 -mfpu=vfp -mfloat-abi=softfp -DW3D_ANDROID_OLD -DLUA_USE_POSIX \\\n", f);
    fputs("  '-Dl_fseek(f,o,w)=fseek(f,o,w)' '-Dl_ftell(f)=ftell(f)' -Dl_seeknum=long\n", f);
    char defRes[32] = "";
    if (g_projCompile.resolution > 0 && g_projCompile.resolution < 100)
        snprintf(defRes, sizeof(defRes), " -DW3D_RESOLUTION=%d", g_projCompile.resolution);
    fprintf(f, "LOCAL_CPPFLAGS := -std=c++1z -include $(OLD)/jni/shim/w3d_sdl207.h -DW3D_GLES2 -DW3D_GAME_RUNTIME "
               "-DW3D_GAME_NO_TEX -DW3D_STB_IMPL -DW3D_DEV_LOG=%d%s%s%s%s\n",
            debugMode ? 1 : 0, defRes,
            useSound ? " -DW3D_ENABLE_AUDIO" : "",
            usePhysics ? "" : " -DW3D_NO_PHYSICS",
            with3D ? " -DW3D_NO_EDITOR -DW3D_GAME_3D" : "");
    // (-latomic: en ARMv6 las atomicas de libc++ son llamadas a esa libreria)
    fputs("LOCAL_SHARED_LIBRARIES := SDL2\nLOCAL_LDLIBS := -lGLESv2 -llog -landroid -latomic\n", f);
    fputs("include $(BUILD_SHARED_LIBRARY)\nW3DMK\n", f);
    fputs("sed -i \"s#^ROOT  := .*#ROOT  := $ROOT#\" \"$AND/jni/Android.mk\"\n", f);
    // la actividad del juego: la de SDL 2.0.7 con las dos librerias (libSDL2.so y libmain.so)
    fputs("mkdir -p \"$AND/java/${PKG//.//}\"\n", f);
    fputs("cat > \"$AND/java/${PKG//.//}/GameActivity.java\" <<W3DJAVA\n", f);
    fputs("package $PKG;\n// GENERADO por Whisk3D: la actividad de SDL 2.0.7 que carga libSDL2.so y libmain.so (el juego)\n", f);
    fputs("public class GameActivity extends org.libsdl.app.SDLActivity {\n", f);
    fputs("    @Override protected String[] getLibraries() { return new String[] { \"SDL2\", \"main\" }; }\n", f);
    // (el main generado pregunta por el notch: en Android 2.3 no hay)
    fputs("    public int[] getSafeInsets() { return new int[] { 0, 0, 0, 0 }; }\n", f);
    // vibrar el telefono: lo llama el juego por JNI sobre la actividad (SDL_AndroidGetActivity + GetMethodID
    // "vibrar" "(I)V"; desde el hilo de SDL, FindClass de una clase de la app no la encuentra)
    // la RESOLUCION de la tarjeta Juego (menos de 100 %): el bufer de la superficie mas chico y el sistema lo estira a la
    // pantalla; se vuelve a poner al girar el telefono (la actividad no se rearma: configChanges orientation)
    if (g_projCompile.resolution > 0 && g_projCompile.resolution < 100) {
        // (el tamano sale de la ORIENTACION nueva, no de las medidas de la pantalla: al girar, las medidas pueden venir
        // todavia con las de antes, y el bufer quedaba con la forma vieja y el sistema lo estiraba)
        fprintf(f, "    private void scale(int orientation) {\n"
                   "        android.util.DisplayMetrics dm = getResources().getDisplayMetrics();\n"
                   "        int a = Math.max(dm.widthPixels, dm.heightPixels), b = Math.min(dm.widthPixels, dm.heightPixels);\n"
                   "        int w = dm.widthPixels, h = dm.heightPixels;\n"
                   "        if (orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE) { w = a; h = b; }\n"
                   "        else if (orientation == android.content.res.Configuration.ORIENTATION_PORTRAIT) { w = b; h = a; }\n"
                   "        final int fw = w * %d / 100, fh = h * %d / 100;\n"
                   "        Object o = mSurface;\n        if (!(o instanceof android.view.SurfaceView)) return;\n"
                   "        final android.view.SurfaceView sv = (android.view.SurfaceView) o;\n"
                   "        if (!rotating) { sv.getHolder().setFixedSize(fw, fh); return; }\n"
                   // (AL GIRAR: la superficie primero vuelve al tamano de la vista, para que Android la reacomode a la
                   // orientacion nueva, y despues se achica otra vez; achicarla de una dejaba la vista con la forma vieja:
                   // la imagen estirada y una franja negra)
                   "        sv.getHolder().setSizeFromLayout();\n"
                   "        sv.postDelayed(new Runnable() { public void run() {\n"
                   "            sv.getHolder().setFixedSize(fw, fh);\n"
                   "            android.util.Log.i(\"Whisk3D\", \"[juego] resolucion: \" + fw + \"x\" + fh + \" en una vista de \" + sv.getWidth() + \"x\" + sv.getHeight());\n"
                   "        } }, 250);\n"
                   "    }\n", g_projCompile.resolution, g_projCompile.resolution);
        fputs("    private boolean rotating = false;\n", f);
        fputs("    @Override protected void onCreate(android.os.Bundle b) {\n"
              "        super.onCreate(b); scale(getResources().getConfiguration().orientation);\n    }\n", f);
        fputs("    @Override public void onConfigurationChanged(android.content.res.Configuration c) {\n"
              "        super.onConfigurationChanged(c); rotating = true; scale(c.orientation);\n    }\n", f);
    }
    fputs("    public void vibrate(int ms) {\n        try {\n            android.os.Vibrator v = (android.os.Vibrator) "
          "getSystemService(android.content.Context.VIBRATOR_SERVICE);\n            if (v != null) v.vibrate(ms);\n"
          "        } catch (Exception e) {}\n    }\n}\nW3DJAVA\n", f);
    // el manifiesto: API 10, GLES 2, la red (W3dRed) y la orientacion de la tarjeta del juego
    const char* orient = orientation == 1 ? " android:screenOrientation=\"portrait\""
                       : orientation == 2 ? " android:screenOrientation=\"landscape\"" : "";
    fputs("cat > \"$AND/AndroidManifest.xml\" <<W3DMAN\n", f);
    fputs("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n", f);
    fputs("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"$PKG\"\n", f);
    // (en la memoria del telefono: sin tarjeta SD, "auto" con poco lugar fallaba con INSTALL_FAILED_MEDIA_UNAVAILABLE)
    fputs("          android:versionCode=\"1\" android:versionName=\"1.0\" android:installLocation=\"internalOnly\">\n", f);
    fputs("    <uses-sdk android:minSdkVersion=\"10\" android:targetSdkVersion=\"10\" />\n", f);
    fputs("    <uses-feature android:glEsVersion=\"0x00020000\" />\n", f);
    fputs("    <uses-permission android:name=\"android.permission.INTERNET\" />\n", f);
    fputs("    <uses-permission android:name=\"android.permission.VIBRATE\" />\n", f);
    fputs("    <application android:label=\"$NAME\" android:icon=\"@drawable/icono\"\n", f);
    fputs("                 android:theme=\"@android:style/Theme.NoTitleBar.Fullscreen\">\n", f);
    fprintf(f, "        <activity android:name=\".GameActivity\" android:label=\"$NAME\"%s\n", orient);
    fputs("                  android:configChanges=\"keyboardHidden|orientation\">\n", f);
    fputs("            <intent-filter>\n                <action android:name=\"android.intent.action.MAIN\" />\n", f);
    fputs("                <category android:name=\"android.intent.category.LAUNCHER\" />\n", f);
    fputs("            </intent-filter>\n        </activity>\n    </application>\n</manifest>\nW3DMAN\n", f);
    // el icono del launcher (48 x 48, la densidad de estas pantallas): el del juego o el de Whisk3D
    if (withIcon)
        fputs("ICON=\"$OUT/icons/android-mdpi.png\"; [ -f \"$ICON\" ] || ICON=\"$W3D/res/Whisk3D.png\"\n", f);
    else
        fputs("ICON=\"$W3D/res/Whisk3D.png\"\n", f);
    fputs("convert \"$ICON\" -resize 48x48 \"$AND/res/drawable/icono.png\"\n", f);
    // los assets: como el Android de siempre (sueltos en assets/, o empaquetados adentro de libmain.so)
    if (!pack) {
        fputs("cp \"$OUT\"/*.w3dui \"$OUT\"/*.lua \"$OUT\"/*.png \"$OUT\"/*.json \"$AND/assets\"/ 2>/dev/null || true\n", f);
        for (int c = 0; CarpAsset(c); c++)
            fprintf(f, "if [ -d \"$OUT/%s\" ]; then mkdir -p \"$AND/assets/%s\"; cp -r \"$OUT/%s/.\" \"$AND/assets/%s/\"; fi\n",
                    CarpAsset(c), CarpAsset(c), CarpAsset(c), CarpAsset(c));
    }
    fputs("echo \"[W3D] 16 ndk-build (armeabi, Android 2.3)\"\n", f);
    fprintf(f, "\"$NDK/ndk-build\" %s NDK_PROJECT_PATH=\"$AND\" APP_BUILD_SCRIPT=\"$AND/jni/Android.mk\" "
               "NDK_APPLICATION_MK=\"$AND/jni/Application.mk\" NDK_OUT=\"$B/obj\" NDK_LIBS_OUT=\"$B/lib\" "
               "SDL_DIR=\"$SDLD\"\n", W3dCompileJobsFlag().c_str());
    fputs("echo \"[W3D] 70 Java (javac + d8)\"\n", f);
    fputs("rm -rf \"$B/clases\" \"$B/dex\" && mkdir -p \"$B/clases\" \"$B/dex\"\n", f);
    fputs("javac -nowarn -source 8 -target 8 -bootclasspath \"$JAR\" -classpath \"$JAR\" -d \"$B/clases\" "
          "\"$SDLD\"/android-project/src/org/libsdl/app/*.java $(find \"$AND/java\" -name '*.java')\n", f);
    fputs("\"$BT/d8\" --min-api 10 --lib \"$JAR\" --output \"$B/dex\" $(find \"$B/clases\" -name '*.class')\n", f);
    fputs("echo \"[W3D] 80 APK (aapt, signing)\"\n", f);
    fputs("rm -rf \"$B/apk\" && mkdir -p \"$B/apk/lib/armeabi\"\n", f);
    fputs("\"$BT/aapt\" package -f -M \"$AND/AndroidManifest.xml\" -S \"$AND/res\" -A \"$AND/assets\" -I \"$JAR\" "
          "-F \"$B/sin_firmar.apk\"\n", f);
    fputs("cp \"$B/dex/classes.dex\" \"$B/apk/\" && cp \"$B\"/lib/armeabi/*.so \"$B/apk/lib/armeabi/\"\n", f);
    fputs("(cd \"$B/apk\" && \"$BT/aapt\" add \"$B/sin_firmar.apk\" classes.dex lib/armeabi/*.so > /dev/null)\n", f);
    fputs("[ -f \"$KS\" ] || keytool -genkeypair -keystore \"$KS\" -storepass android -alias androiddebugkey "
          "-keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname \"CN=Android Debug,O=Android,C=US\"\n", f);
    fputs("\"$BT/zipalign\" -f 4 \"$B/sin_firmar.apk\" \"$B/alineado.apk\"\n", f);
    fputs("\"$BT/apksigner\" sign --ks \"$KS\" --ks-pass pass:android --key-pass pass:android --min-sdk-version 10 "
          "--out \"$OUT/$NAME-android23.apk\" \"$B/alineado.apk\"\n", f);
    fputs("rm -f \"$B/sin_firmar.apk\" \"$B/alineado.apk\"\n", f);
    // un telefono conectado: se instala y se abre (sin telefono, el APK queda en build/android23)
    fputs("ADB=\"$SDK/platform-tools/adb\"\n", f);
    fputs("if [ -x \"$ADB\" ] && [ \"$(\"$ADB\" devices | awk 'NR>1 && $2==\"device\"' | wc -l)\" = 1 ]; then\n", f);
    fputs("  echo \"[W3D] 90 Installing on the phone\"\n", f);
    // (adb de esa epoca sale con 0 aunque falle: se mira lo que dice)
    // (con -r el telefono guarda el APK nuevo mientras el viejo sigue instalado: en uno de poca memoria -el Galaxy Ace,
    // ~35 MB libres- no entra. Entonces se desinstala la version anterior del MISMO juego -se pierden sus datos guardados en
    // el telefono- y se instala de nuevo)
    fputs("  INST=\"$(\"$ADB\" install -r \"$OUT/$NAME-android23.apk\" 2>&1 || true)\"; echo \"$INST\"\n", f);
    fputs("  if echo \"$INST\" | grep -q 'INSUFFICIENT_STORAGE' && \"$ADB\" shell pm path \"$PKG\" | grep -q package; then\n", f);
    // (el paquete va en su propia linea: con $PKG adentro la marca no seria una clave fija y saldria sin traducir)
    fputs("    echo \"[W3D] 92 Not enough space: uninstalling the previous version (its data on the phone is lost)\"\n", f);
    fputs("    echo \"    ($PKG)\"\n", f);
    fputs("    \"$ADB\" uninstall \"$PKG\" || true\n", f);
    fputs("    INST=\"$(\"$ADB\" install \"$OUT/$NAME-android23.apk\" 2>&1 || true)\"; echo \"$INST\"\n  fi\n", f);
    fputs("  if echo \"$INST\" | grep -q '^Success'; then\n", f);
    fputs("    \"$ADB\" shell am start -n \"$PKG/.GameActivity\"\n", f);
    fputs("  else\n    echo \"[W3D] 95 COULD NOT INSTALL on the phone (low on space?): the APK stays in build/android23\"\n  fi\n", f);
    fputs("else\n  echo \"(sin un telefono conectado por adb: el APK queda en build/android23)\"\nfi\n", f);
    bool okSh = (ferror(f) == 0);
    if (fclose(f) != 0) okSh = false;
    return okSh;
}

// ============================================================================
//  LAS CONSOLAS (plataformas 7 PSP, 8 PS Vita, 9 Nintendo 3DS, 10 Dreamcast): el juego con el kit LIBRE de cada una,
//  en las carpetas de siempre (~/psp, ~/vita, ~/3ds, ~/dc: los dejan los preparar_<consola>.sh de Whisk3D/platform,
//  los mismos con que se compila el editor para esa consola).
//
//  Se generan dos archivos en platform-build/<consola>:
//    Makefile                  las MISMAS listas de fuentes que los otros destinos (kSourcesBase, kSources3D...),
//                              con el backend grafico de la consola y la capa de consola del runtime
//                              (Whisk3D-Examples/game/w3dconsola: la ventana, los graficos, cada cuadro), y el paquete
//    build_<consola>.sh        arma el entorno del kit y corre make (con las marcas "[W3D] <pct> <etapa>" de la barra)
//  Los assets del juego van donde los lee cada consola: al lado del EBOOT.PBP (PSP), adentro del .vpk (Vita), en el
//  romfs del .3dsx (3DS) y en el romdisk del ejecutable (Dreamcast). Empaquetados, van adentro del binario (pak.cpp).
//  Como los otros destinos, platform-build/<consola> se puede compilar a mano sin abrir el editor.
// ============================================================================
enum { PlatWindows = 4, PlatSymbian = 5, PlatPSP = 7, PlatVita = 8, Plat3DS = 9, PlatDreamcast = 10 };
static bool IsConsole(int p) { return p >= PlatPSP && p <= PlatDreamcast; }
// los que se compilan con un Makefile generado (las consolas y Windows, cruzado desde Linux)
static bool WithMakefile(int p) { return IsConsole(p) || p == PlatWindows; }
static const char* DirConsole(int p) {
    return (p == PlatWindows) ? "windows" : (p == PlatSymbian) ? "symbian" : (p == PlatPSP) ? "psp"
         : (p == PlatVita) ? "vita" : (p == Plat3DS) ? "3ds" : "dreamcast";
}
static const char* ConsoleName(int p) {
    return (p == PlatWindows) ? "Windows" : (p == PlatSymbian) ? "Symbian" : (p == PlatPSP) ? "PSP"
         : (p == PlatVita) ? "PS Vita" : (p == Plat3DS) ? "Nintendo 3DS" : "Dreamcast";
}
// EL ICONO DE WINDOWS (.ico): las imagenes de icons/ (16, 32, 48 y 256, las que se hayan generado) guardadas como PNG
// adentro del .ico, que Windows lee desde Vista. Lo enlaza el Makefile con windres
static bool WriteIco(const std::string& out, const std::string& path) {
    static const int size[4] = { 16, 32, 48, 256 };
    std::vector<int> n;
    std::vector<std::vector<unsigned char> > png;
    for (int i = 0; i < 4; i++) {
        std::vector<unsigned char> d;
        char r[1200];
        snprintf(r, sizeof(r), "%s/icons/%dx%d.png", out.c_str(), size[i], size[i]);
        if (w3dFileSystem::ReadFileBytes(r, d) && !d.empty()) { n.push_back(size[i]); png.push_back(d); }
    }
    if (n.empty()) return false;
    std::vector<unsigned char> ico;
    unsigned off = 6 + 16 * (unsigned)n.size();
    const unsigned char cab[6] = { 0, 0, 1, 0, (unsigned char)n.size(), 0 };
    ico.insert(ico.end(), cab, cab + 6);
    for (size_t i = 0; i < n.size(); i++) {
        const unsigned t = (unsigned)png[i].size();
        const unsigned char e[16] = { (unsigned char)(n[i] >= 256 ? 0 : n[i]), (unsigned char)(n[i] >= 256 ? 0 : n[i]),
            0, 0, 1, 0, 32, 0, (unsigned char)t, (unsigned char)(t >> 8), (unsigned char)(t >> 16), (unsigned char)(t >> 24),
            (unsigned char)off, (unsigned char)(off >> 8), (unsigned char)(off >> 16), (unsigned char)(off >> 24) };
        ico.insert(ico.end(), e, e + 16);
        off += t;
    }
    for (size_t i = 0; i < png.size(); i++) ico.insert(ico.end(), png[i].begin(), png[i].end());
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = fwrite(&ico[0], 1, ico.size(), f) == ico.size();
    return (fclose(f) == 0) && ok;
}
// el TITLE_ID de la Vita: 4 letras y 5 numeros, propio de cada juego (sale de su nombre)
static std::string TitleIdVita(const std::string& name) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < name.size(); i++) { h ^= (unsigned char)name[i]; h *= 16777619u; }
    char id[16];
    snprintf(id, sizeof(id), "WJGO%05u", h % 100000u);
    return id;
}

static bool WriteMakefileConsole(const std::string& path, int plat, const std::string& repo,
                                    const std::string& name, bool usePhysics, bool useSound,
                                    bool debugMode, bool pack, bool with3D, int backend = 0,
                                    bool withIco = false) {
    const bool shaders = (backend == 1), vulkan = (backend == 2);
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "# GENERADO por Whisk3D (Compilar juego -> %s). No hace falta editarlo.\n", ConsoleName(plat));
    fprintf(f, "# Lo corre build_%s.sh (arma el entorno del kit); con el entorno ya armado alcanza con 'make'.\n",
            DirConsole(plat));
    fprintf(f, "W3D    := %s\n", repo.c_str());
    fputs("EXAMP  := $(abspath $(W3D)/../Whisk3D-Examples)\nCORE   := $(W3D)/libs/Whisk3DCore\n"
          "UILIB  := $(W3D)/libs/WhiskUI\nGAME   := $(EXAMP)/game\nLUA    := $(W3D)/thirdparty/lua/src\n", f);
    fprintf(f, "NAME := %s\n", name.c_str());
    fputs("OUT    := out\nOBJDIR := $(OUT)/obj\nDATA  := $(OUT)/datos\n\n", f);

    // ---- el kit, el procesador y el backend grafico de cada consola ----
    if (plat == PlatWindows) {
        // (todo estatico: SDL2, libc++ y el runtime de C adentro del .exe; las DLL de Windows son las del sistema)
        fputs("# Windows de 64 bits, compilado desde Linux: llvm-mingw (clang + lld) y SDL2 para mingw, todo estatico (un\n"
              "# .exe sin DLLs al lado). -mwindows: sin la ventana de consola\n"
              "LLVM_MINGW ?= $(HOME)/windows/llvm-mingw\nSDL2_MINGW ?= $(HOME)/windows/sdl2-mingw/x86_64-w64-mingw32\n"
              "CXX      := $(LLVM_MINGW)/bin/x86_64-w64-mingw32-clang++\nCC       := $(LLVM_MINGW)/bin/x86_64-w64-mingw32-clang\n"
              "WINDRES  := $(LLVM_MINGW)/bin/x86_64-w64-mingw32-windres\n"
              "ARCH     :=\nOPT      := -O2 -ffunction-sections -fdata-sections\n", f);
        fprintf(f, "PLATDEFS := -DW3D_OPENGL%s\n", vulkan ? " -DW3D_MULTIBACKEND -DW3D_VULKAN"
                                                   : shaders ? " -DW3D_WEBGL -DW3D_GLES2" : "");
        fputs("INCPRE   :=\nINCPLAT  := -I$(SDL2_MINGW)/include -I$(SDL2_MINGW)/include/SDL2\nEXTRA    :=\n", f);
        if (vulkan)   // (los tres backends, cada uno en su namespace, y el despacho: como el editor)
            fputs("BACKEND  := $(CORE)/graphics/opengl/w3dGraphics.cpp $(CORE)/graphics/gles2/w3dGraphicsGLES2.cpp "
                  "$(CORE)/graphics/vulkan/w3dGraphicsVk.cpp $(CORE)/graphics/vulkan/w3dVkGL.cpp "
                  "$(CORE)/graphics/w3dGraphicsDispatch.cpp\n", f);
        else
            fprintf(f, "BACKEND  := %s\n", shaders ? "$(CORE)/graphics/gles2/w3dGraphicsGLES2.cpp"
                                                  : "$(CORE)/graphics/opengl/w3dGraphics.cpp");
        fputs("LIBS     := -static $(SDL2_MINGW)/lib/libSDL2.a -lopengl32 -lglu32 -lws2_32 -lsetupapi -lwinmm -limm32 "
              "-lversion -lole32 -loleaut32 -lgdi32 -luser32 -lshell32 -luuid -ladvapi32 -mwindows\n", f);
        if (vulkan)
            // shaderc ADENTRO si esta compilado (tools/preparar_shaderc.sh windows); si no, el juego busca
            // shaderc_shared.dll al lado del .exe y sin ella dibuja con OpenGL
            fputs("INCPLAT  += -I$(CORE)/thirdparty/vulkan/include\n"
                  "SHADERC  ?= $(HOME)/shaderc/windows/lib/libshaderc_combined.a\n"
                  "ifneq ($(wildcard $(SHADERC)),)\nPLATDEFS += -DW3D_SHADERC_STATIC\nLIBS     := $(SHADERC) $(LIBS)\nelse\n"
                  "$(warning Vulkan sin shaderc adentro (correr Whisk3D/tools/preparar_shaderc.sh windows): el juego busca "
                  "shaderc_shared.dll al lado del .exe, sino usa OpenGL)\nendif\n", f);
    } else if (plat == PlatPSP) {
        fputs("# PSP: pspdev (psp-gcc, el MIPS Allegrex) y el GE con el backend sceGu del Core\n"
              "PSPDEV   ?= $(HOME)/psp/pspdev\nPSPSDK   := $(PSPDEV)/psp/sdk\n"
              "CXX      := $(PSPDEV)/bin/psp-g++\nCC       := $(PSPDEV)/bin/psp-gcc\n"
              "ARCH     := -G0\nOPT      := -O2 -ffunction-sections -fdata-sections\n"
              "# (-Umips: el gcc de MIPS define el macro 'mips', y el Core tiene variables que se llaman asi)\n"
              "PLATDEFS := -DW3D_PSP -D_PSP_FW_VERSION=600 -Umips\n"
              "INCPRE   := -I$(W3D)/platform/psp/include\n"
              "INCPLAT  := -I$(PSPDEV)/psp/include/SDL2 -I$(PSPDEV)/psp/include -I$(PSPSDK)/include\n"
              "EXTRA    := -include climits\n"
              "BACKEND  := $(CORE)/graphics/gu/w3dGraphicsGu.cpp $(CORE)/graphics/gu/w3dGLgu.cpp\n"
              "LIBS     := -lSDL2main -lSDL2 -lpspvram -lpspaudio -lpspvfpu -lpspdisplay -lpspgu -lpspge -lpsphprm "
              "-lpspctrl -lpsppower\n", f);
    } else if (plat == PlatVita) {
        fputs("# PS Vita: VitaSDK (el Cortex-A9) y vitaGL (OpenGL con el pipeline fijo emulado) con el backend fijo\n"
              "VITASDK  ?= $(HOME)/vita/vitasdk\n"
              "CXX      := $(VITASDK)/bin/arm-vita-eabi-g++\nCC       := $(VITASDK)/bin/arm-vita-eabi-gcc\n"
              "ARCH     := -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard\n"
              "OPT      := -O2 -ffunction-sections -fdata-sections\n"
              "PLATDEFS := -DW3D_VITA -DW3D_OPENGL\n"
              "INCPRE   := -I$(W3D)/platform/vita/include\nINCPLAT  := -I$(VITASDK)/arm-vita-eabi/include/SDL2\n"
              "EXTRA    := -include climits -include sys/syslimits.h\n"
              "BACKEND  := $(CORE)/graphics/opengl/w3dGraphics.cpp\n"
              "LIBS     := -lSDL2 -lvitaGL -lvitashark -lSceShaccCgExt -lmathneon -ltaihen_stub -lSceShaccCg_stub "
              "-lSceKernelDmacMgr_stub -lSceIme_stub -lSceGxm_stub -lSceDisplay_stub -lSceCtrl_stub -lSceAppMgr_stub "
              "-lSceAppUtil_stub -lSceAudio_stub -lSceAudioIn_stub -lSceSysmodule_stub -lSceIofilemgr_stub "
              "-lSceCommonDialog_stub -lSceTouch_stub -lSceHid_stub -lSceMotion_stub -lScePower_stub "
              "-lSceProcessmgr_stub -lSceLibKernel_stub -lpthread -lm\n", f);
        fprintf(f, "TITLE_ID := %s\n", TitleIdVita(name).c_str());
    } else if (plat == Plat3DS) {
        fputs("# Nintendo 3DS: devkitARM + libctru + citro3d (la GPU PICA200) con el backend citro3d del Core\n"
              "DEVKITPRO ?= $(HOME)/3ds/devkitpro\nDEVKITARM ?= $(DEVKITPRO)/devkitARM\n"
              "SDL2_3DS  ?= $(HOME)/3ds/sdl2-3ds-inst\n"
              "CXX      := $(DEVKITARM)/bin/arm-none-eabi-g++\nCC       := $(DEVKITARM)/bin/arm-none-eabi-gcc\n"
              "TOOLS    := $(DEVKITPRO)/tools/bin\n"
              "ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft\n"
              "OPT      := -O2 -ffunction-sections -fdata-sections\n"
              "PLATDEFS := -D__3DS__ -DW3D_3DS -DW3D_OPENGL\n"
              "INCPRE   := -I$(W3D)/platform/3ds/include -I$(OBJDIR)\n"
              "INCPLAT  := -I$(SDL2_3DS)/include -I$(SDL2_3DS)/include/SDL2 -I$(DEVKITPRO)/libctru/include\n"
              "EXTRA    := -include climits\n"
              "BACKEND  := $(CORE)/graphics/citro3d/w3dGraphicsC3D.cpp $(CORE)/graphics/citro3d/w3dGLc3d.cpp\n"
              "LIBS     := -L$(SDL2_3DS)/lib -L$(DEVKITPRO)/libctru/lib -lSDL2main -lSDL2 -lcitro3d -lctru -lm\n", f);
    } else {
        fputs("# Dreamcast: KallistiOS (el SH-4; el entorno lo arma environ.sh) y GLdc (OpenGL 1.x de la PVR) con el\n"
              "# backend fijo. LUA_32BITS: los numeros de lua en float (el SH-4 los hace en un ciclo)\n"
              "SDL2_DC  ?= $(HOME)/dc/sdl2-dc-inst\n"
              "CXX      := kos-c++\nCC       := kos-cc\n"
              "ARCH     :=\nOPT      := -Os -ffunction-sections -fdata-sections\n"
              "PLATDEFS := -DW3D_DREAMCAST -DW3D_OPENGL -DDREAMCAST -DLUA_32BITS=1\n"
              "INCPRE   :=\nINCPLAT  := -I$(SDL2_DC)/include -I$(SDL2_DC)/include/SDL2\n"
              "EXTRA    :=\n"
              "BACKEND  := $(CORE)/graphics/opengl/w3dGraphics.cpp\n"
              "LIBS     := -L$(SDL2_DC)/lib -lSDL2 -lSDL2main -lGL -lGLU -lm -lkosutils -lpthread\n"
              "MKDCDISC ?= $(HOME)/dc/mkdcdisc/build_manual/mkdcdisc\n", f);
    }

    // ---- las fuentes: las MISMAS listas que el CMake de la PC, el Android.mk y emcc ----
    fputs("\nSOURCES := $(CURDIR)/main.cpp \\\n", f);
    if (pack) fputs("  $(CURDIR)/pak.cpp \\\n", f);
    fputs("  $(GAME)/w3drun.cpp $(GAME)/w3dconsola.cpp $(BACKEND) \\\n", f);
    for (int i = 0; kSourcesBase[i]; i++) fprintf(f, "  %s \\\n", AMk(kSourcesBase[i]).c_str());
    if (usePhysics) fputs("  $(CORE)/physics/W3dPhysics.cpp \\\n", f);
    if (useSound) fputs("  $(CORE)/audio/W3dAudio.cpp $(CORE)/audio/W3dAudioSDL.cpp \\\n", f);
    else            fputs("  $(CORE)/audio/W3dAudio.cpp \\\n", f);
    if (with3D)
        for (int i = 0; kSources3D[i]; i++) fprintf(f, "  %s \\\n", AMk(kSources3D[i]).c_str());
    fputs("\nSOURCES_C := $(filter-out %/lua.c %/luac.c,$(wildcard $(LUA)/*.c))\n", f);
    // (cada objeto en su ruta absoluta adentro de obj/: hay archivos con el mismo nombre en carpetas distintas)
    fputs("OBJS := $(addprefix $(OBJDIR),$(addsuffix .o,$(abspath $(SOURCES) $(SOURCES_C))))\n\n", f);
    fputs("INCS := $(INCPRE) -I$(GAME) \\\n", f);
    for (int i = 0; kIncludesBase[i]; i++) fprintf(f, "  -I%s \\\n", AMk(kIncludesBase[i]).c_str());
    if (with3D)
        for (int i = 0; kIncludes3D[i]; i++) fprintf(f, "  -I%s \\\n", AMk(kIncludes3D[i]).c_str());
    fputs("  -I$(LUA) $(INCPLAT)\n", f);
    fprintf(f, "DEFS := %s$(PLATDEFS) -DW3D_GAME_RUNTIME -DW3D_GAME_NO_TEX -DW3D_STB_IMPL -DW3D_DEV_LOG=%d%s%s%s\n",
            IsConsole(plat) ? "-DW3D_CONSOLE " : "", debugMode ? 1 : 0, useSound ? " -DW3D_ENABLE_AUDIO" : "", usePhysics ? "" : " -DW3D_NO_PHYSICS",
            with3D ? " -DW3D_NO_EDITOR -DW3D_GAME_3D" : "");
    fputs("CXXFLAGS := $(ARCH) $(OPT) -std=gnu++17 $(EXTRA) $(DEFS) $(INCS) -w -MMD -MP\n"
          "CFLAGS   := $(ARCH) $(OPT) -std=gnu99 $(DEFS) $(INCS) -w -MMD -MP\n\n", f);
    if (vulkan)   // cada backend en su namespace (graphics/w3dBackend.h)
        fputs("$(OBJDIR)$(abspath $(CORE)/graphics/opengl/w3dGraphics.cpp).o: CXXFLAGS += -DW3D_BACKEND_NS=gl\n"
              "$(OBJDIR)$(abspath $(CORE)/graphics/gles2/w3dGraphicsGLES2.cpp).o: CXXFLAGS += -DW3D_BACKEND_NS=es2\n"
              "$(OBJDIR)$(abspath $(CORE)/graphics/vulkan/w3dGraphicsVk.cpp).o: CXXFLAGS += -DW3D_BACKEND_NS=vk -DW3D_VKGL\n\n", f);

    fputs("all: package\n\ncount:\n\t@echo $(words $(OBJS))\n\n", f);
    // LOS FLAGS en un sello: si cambian (debug o produccion, fisica, sonido, el backend), make no lo ve en los archivos
    // y mezclaria objetos de las dos maneras. Con el sello distinto se recompila todo
    fputs("$(OBJDIR)/flags.txt: FORCE\n\t@mkdir -p $(OBJDIR)\n"
          "\t@echo '$(CXXFLAGS) $(CFLAGS)' | cmp -s - $@ || echo '$(CXXFLAGS) $(CFLAGS)' > $@\n"
          "$(OBJS): $(OBJDIR)/flags.txt\nFORCE:\n\n", f);
    fputs("$(OBJDIR)/%.cpp.o: /%.cpp\n\t@mkdir -p $(dir $@)\n\t@echo \"  c++ $(notdir $<)\"\n"
          "\t@$(CXX) $(CXXFLAGS) -c $< -o $@\n", f);
    fputs("$(OBJDIR)/%.c.o: /%.c\n\t@mkdir -p $(dir $@)\n\t@echo \"  cc  $(notdir $<)\"\n"
          "\t@$(CC) $(CFLAGS) -c $< -o $@\n\n", f);

    // ---- los assets del juego (sueltos): los de la raiz y las carpetas de assets, como en los otros destinos ----
    fputs("# los assets del juego, como los lee la consola (vacio si van empaquetados: estan en pak.cpp)\n", f);
    fputs("data:\n\t@rm -rf $(DATA) && mkdir -p $(DATA)\n", f);
    if (!pack) {
        fputs("\t@for a in *.w3dui *.lua *.png *.json; do if [ -f \"$$a\" ]; then cp \"$$a\" $(DATA)/; fi; done\n", f);
        std::string dirs;
        for (int c = 0; CarpAsset(c); c++) { dirs += " "; dirs += CarpAsset(c); }
        if (!dirs.empty())
            fprintf(f, "\t@for d in%s; do if [ -d \"$$d\" ]; then cp -r \"$$d\" $(DATA)/; fi; done\n", dirs.c_str());
    }
    fputs("\n", f);

    // ---- enlazar y armar el paquete de cada consola ----
    if (plat == PlatWindows) {
        // el .exe y, al lado, los assets (si van sueltos): la carpeta <juego>/ lista para copiar, y el mismo .zip
        if (withIco)
            fputs("$(OUT)/icono.o: icon.ico\n\t@mkdir -p $(OUT)\n\t@echo '1 ICON \"icono.ico\"' > $(OUT)/icono.rc\n"
                  "\t@$(WINDRES) -I. $(OUT)/icono.rc -O coff -o $@\n"
                  "ICON := $(OUT)/icono.o\n", f);
        fputs("$(OUT)/$(NAME).exe: $(OBJS) $(ICON)\n\t@echo \"  link $@\"\n"
              "\t@$(CXX) -o $@ $(OBJS) $(ICON) -Wl,--gc-sections -s $(LIBS)\n"
              "package: $(OUT)/$(NAME).exe data\n"
              "\t@rm -rf $(OUT)/$(NAME) && mkdir -p $(OUT)/$(NAME)\n"
              "\t@cp $(OUT)/$(NAME).exe $(OUT)/$(NAME)/ && cp -r $(DATA)/. $(OUT)/$(NAME)/\n"
              "\t@cd $(OUT) && rm -f $(NAME)-windows.zip && if command -v zip > /dev/null; then "
              "zip -qr $(NAME)-windows.zip $(NAME); fi\n"
              "\t@echo \"  exe $(OUT)/$(NOMBRE)/$(NOMBRE).exe\"\n", f);
    } else if (plat == PlatPSP) {
        fputs("# un PRX (reubicable, como piden los firmwares de ahora) adentro del EBOOT.PBP. MEMSIZE=1: la getMemory grande\n"
              "# de las PSP 2000 en adelante. El juego va en PSP/GAME/<juego>, con sus assets al lado\n"
              "FOLDER := $(OUT)/PSP/GAME/$(NAME)\n"
              "$(OUT)/$(NAME).elf: $(OBJS)\n\t@echo \"  link $@\"\n"
              "\t@$(CXX) $(ARCH) -L$(PSPDEV)/psp/lib -L$(PSPSDK)/lib -specs=$(PSPSDK)/lib/prxspecs "
              "-Wl,-q,-T$(PSPSDK)/lib/linkfile.prx -Wl,-zmax-page-size=128 -Wl,--gc-sections -o $@ "
              "$(PSPSDK)/lib/prxexports.o $(OBJS) $(LIBS)\n"
              "\t@$(PSPDEV)/bin/psp-fixup-imports $@\n"
              "$(OUT)/$(NAME).prx: $(OUT)/$(NAME).elf\n\t@$(PSPDEV)/bin/psp-prxgen $< $@\n"
              "$(OUT)/PARAM.SFO:\n\t@mkdir -p $(OUT)\n\t@$(PSPDEV)/bin/mksfoex -d MEMSIZE=1 '$(NAME)' $@\n"
              "package: $(OUT)/$(NAME).prx $(OUT)/PARAM.SFO data\n"
              "\t@rm -rf $(OUT)/PSP && mkdir -p $(FOLDER)\n"
              "\t@$(PSPDEV)/bin/pack-pbp $(FOLDER)/EBOOT.PBP $(OUT)/PARAM.SFO NULL NULL NULL NULL NULL "
              "$(OUT)/$(NAME).prx NULL > /dev/null\n"
              "\t@cp -r $(DATA)/. $(FOLDER)/\n"
              "\t@cd $(OUT) && rm -f $(NAME)-psp.zip && if command -v zip > /dev/null; then zip -qr $(NAME)-psp.zip PSP; fi\n"
              "\t@echo \"  EBOOT.PBP $(CARPETA)\"\n", f);
    } else if (plat == PlatVita) {
        fputs("# -Wl,-q: deja las reubicaciones, que vita-elf-create necesita. __sce_headroom: el lugar del final del codigo\n"
              "# para los datos del modulo. eboot.bin sin -s (modo \"unsafe\": vitaGL carga el compilador de shaders de\n"
              "# ur0:data/external y pide la memoria extra, ATTRIBUTE2=12). Los assets van adentro del .vpk (app0:)\n"
              "$(OUT)/$(NAME).elf: $(OBJS)\n\t@echo \"  link $@\"\n"
              "\t@$(CXX) $(ARCH) -Wl,-q -Wl,--gc-sections -Wl,--defsym=__sce_headroom=0x4000 -o $@ $(OBJS) $(LIBS)\n"
              "$(OUT)/$(NAME).velf: $(OUT)/$(NAME).elf\n\t@$(VITASDK)/bin/vita-elf-create $< $@ > /dev/null\n"
              "$(OUT)/eboot.bin: $(OUT)/$(NAME).velf\n\t@$(VITASDK)/bin/vita-make-fself -c $< $@\n"
              "$(OUT)/param.sfo:\n\t@mkdir -p $(OUT)\n"
              "\t@$(VITASDK)/bin/vita-mksfoex -s TITLE_ID=$(TITLE_ID) -s APP_SHOW=01.00 -d ATTRIBUTE2=12 '$(NAME)' $@\n"
              "package: $(OUT)/eboot.bin $(OUT)/param.sfo data\n"
              "\t@cd $(OUT) && rm -f $(NAME).vpk && $(VITASDK)/bin/vita-pack-vpk -s param.sfo -b eboot.bin "
              "$$(cd data && find . -type f | sed 's|^\\./||' | while read a; do printf -- '-a datos/%s=%s ' \"$$a\" \"$$a\"; done) "
              "$(NAME).vpk > /dev/null\n"
              "\t@echo \"  vpk $(OUT)/$(NOMBRE).vpk ($(TITLE_ID))\"\n", f);
    } else if (plat == Plat3DS) {
        fputs("# el vertex shader de citro3d: picasso lo ensambla y bin2s lo vuelve un arreglo (w3d_shbin.h)\n"
              "$(OBJDIR)/w3d.shbin: $(CORE)/graphics/citro3d/w3d.v.pica\n"
              "\t@mkdir -p $(OBJDIR)\n\t@$(TOOLS)/picasso -o $@ $<\n"
              "$(OBJDIR)/w3d_shbin.h $(OBJDIR)/w3d_shbin.s: $(OBJDIR)/w3d.shbin\n"
              "\t@cd $(OBJDIR) && $(TOOLS)/bin2s -a 4 -H w3d_shbin.h w3d.shbin > w3d_shbin.s\n"
              "$(OBJDIR)/w3d_shbin.o: $(OBJDIR)/w3d_shbin.s\n\t@$(CC) $(ARCH) -x assembler-with-cpp -c $< -o $@\n"
              "$(OBJS): | $(OBJDIR)/w3d_shbin.h\n"
              "$(OUT)/$(NAME).elf: $(OBJS) $(OBJDIR)/w3d_shbin.o\n\t@echo \"  link $@\"\n"
              "\t@$(CXX) $(ARCH) -specs=3dsx.specs -o $@ $(OBJS) $(OBJDIR)/w3d_shbin.o -Wl,--gc-sections $(LIBS)\n", f);
        // el icono y los datos del menu (smdh): el icono de 48x48 lo deja el editor en icons/ (el del proyecto, o el de
        // Whisk3D si no tiene). Va siempre: 3dsxtool no arma un .3dsx con romfs sin el smdh
        fputs("$(OUT)/$(NAME).smdh: icons/48x48.png\n"
              "\t@$(TOOLS)/smdhtool --create '$(NAME)' 'Juego hecho con Whisk3D' 'Whisk3D' $< $@\n"
              "package: $(OUT)/$(NAME).elf $(OUT)/$(NAME).smdh data\n"
              "\t@$(TOOLS)/3dsxtool $(OUT)/$(NAME).elf $(OUT)/$(NAME).3dsx --smdh=$(OUT)/$(NAME).smdh "
              "--romfs=$(DATA)\n", f);
        fputs("\t@echo \"  3dsx $(OUT)/$(NOMBRE).3dsx\"\n", f);
    } else {
        fputs("# los assets en un romdisk adentro del ejecutable (/rd)\n"
              "$(OUT)/romdisk.o: data\n"
              "\t@$(KOS_GENROMFS) -f $(OUT)/romdisk.img -d $(DATA) > /dev/null\n"
              "\t@$(KOS_BASE)/utils/bin2c/bin2c $(OUT)/romdisk.img $(OUT)/romdisk_tmp.c romdisk\n"
              "\t@kos-cc -o $(OUT)/romdisk_tmp.o -c $(OUT)/romdisk_tmp.c\n"
              "\t@$(KOS_CC) -o $@ -r $(OUT)/romdisk_tmp.o $(KOS_LIB_PATHS) -Wl,--whole-archive -lromdiskbase -nostdlib\n"
              "\t@rm -f $(OUT)/romdisk_tmp.c $(OUT)/romdisk_tmp.o\n"
              "$(OUT)/$(NAME)_debug.elf: $(OBJS) $(OUT)/romdisk.o\n\t@echo \"  link $@\"\n"
              "\t@kos-c++ -o $@ $(OBJS) $(OUT)/romdisk.o -Wl,--gc-sections $(LIBS)\n"
              "$(OUT)/$(NAME).elf: $(OUT)/$(NAME)_debug.elf\n"
              "\t@$(KOS_CC_BASE)/bin/$(KOS_CC_PREFIX)-strip -o $@ $<\n"
              "# el disco .cdi (lo abren los emuladores y se graba para la consola), si esta mkdcdisc\n"
              "package: $(OUT)/$(NAME).elf\n"
              "\t@rm -f $(OUT)/$(NAME).cdi\n"
              "\t@if [ -x \"$(MKDCDISC)\" ]; then $(MKDCDISC) -q -N -e $(OUT)/$(NAME).elf -o $(OUT)/$(NAME).cdi "
              "-n '$(NAME)' -a 'Whisk3D' -V W3DJUEGO; echo \"  cdi $(OUT)/$(NOMBRE).cdi\"; "
              "else echo \"  (sin mkdcdisc: solo $(OUT)/$(NOMBRE).elf)\"; fi\n", f);
    }
    fputs("\nclean:\n\trm -rf $(OUT)\n\n-include $(OBJS:.o=.d)\n\n.PHONY: all count package data clean FORCE\n", f);
    bool ok = (ferror(f) == 0);
    if (fclose(f) != 0) ok = false;
    return ok;
}

// el script que arma el entorno del kit de la consola y corre el Makefile (con las marcas de la barra)
static bool WriteBuildConsole(const std::string& path, int plat, const std::string& out) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs("#!/usr/bin/env bash\n", f);
    fprintf(f, "# GENERADO por Whisk3D (Compilar juego -> %s). Arma el entorno del kit y compila con el Makefile.\n",
            ConsoleName(plat));
    // (sin -u: el environ.sh de KallistiOS lee variables que pueden no estar)
    fputs("set -eo pipefail\n", f);
    fprintf(f, "cd \"%s\"\n", Esc(out).c_str());
    if (plat == PlatWindows)
        fputs("export LLVM_MINGW=\"${LLVM_MINGW:-$HOME/windows/llvm-mingw}\"\n"
              "export SDL2_MINGW=\"${SDL2_MINGW:-$HOME/windows/sdl2-mingw/x86_64-w64-mingw32}\"\n"
              "[ -x \"$LLVM_MINGW/bin/x86_64-w64-mingw32-clang++\" ] && [ -f \"$SDL2_MINGW/lib/libSDL2.a\" ] || { echo "
              "\"ERROR: falta llvm-mingw o SDL2 para mingw: correr Whisk3D/platform/windows/preparar_mingw.sh\"; exit 1; }\n", f);
    else if (plat == PlatPSP)
        fputs("export PSPDEV=\"${PSPDEV:-$HOME/psp/pspdev}\"\nexport PATH=\"$PSPDEV/bin:$PATH\"\n"
              "[ -x \"$PSPDEV/bin/psp-g++\" ] && [ -f \"$PSPDEV/psp/lib/libSDL2.a\" ] || { echo \"ERROR: falta pspdev "
              "(o SDL2 de la PSP) en $PSPDEV: correr Whisk3D/platform/psp/preparar_psp.sh\"; exit 1; }\n", f);
    else if (plat == PlatVita)
        fputs("export VITASDK=\"${VITASDK:-$HOME/vita/vitasdk}\"\nexport PATH=\"$VITASDK/bin:$PATH\"\n"
              "[ -x \"$VITASDK/bin/arm-vita-eabi-g++\" ] && [ -f \"$VITASDK/arm-vita-eabi/lib/libvitaGL.a\" ] || { echo "
              "\"ERROR: falta VitaSDK (o vitaGL) en $VITASDK: correr Whisk3D/platform/vita/preparar_vita.sh\"; exit 1; }\n", f);
    else if (plat == Plat3DS)
        fputs("export DEVKITPRO=\"${DEVKITPRO:-$HOME/3ds/devkitpro}\"\nexport DEVKITARM=\"${DEVKITARM:-$DEVKITPRO/devkitARM}\"\n"
              "export SDL2_3DS=\"${SDL2_3DS:-$HOME/3ds/sdl2-3ds-inst}\"\n"
              "[ -x \"$DEVKITARM/bin/arm-none-eabi-g++\" ] && [ -f \"$DEVKITPRO/libctru/lib/libcitro3d.a\" ] && "
              "[ -f \"$SDL2_3DS/lib/libSDL2.a\" ] || { echo \"ERROR: falta devkitARM, citro3d o SDL2 de la 3DS: correr "
              "Whisk3D/platform/3ds/preparar_3ds.sh\"; exit 1; }\n", f);
    else
        fputs("KOS_ENV=\"${KOS_ENV:-$HOME/dc/kos/environ.sh}\"\n"
              "[ -f \"$KOS_ENV\" ] || { echo \"ERROR: falta el entorno de KallistiOS ($KOS_ENV): correr "
              "Whisk3D/platform/dreamcast/preparar_dreamcast.sh\"; exit 1; }\n"
              "source \"$KOS_ENV\"\nexport SDL2_DC=\"${SDL2_DC:-$HOME/dc/sdl2-dc-inst}\"\n"
              "[ -f \"$SDL2_DC/lib/libSDL2.a\" ] || { echo \"ERROR: falta SDL2 de la Dreamcast en $SDL2_DC\"; exit 1; }\n", f);
    // el avance: cada "  c++ x.cpp" / "  cc x.c" de make es un objeto de los TOTAL (make cuantos) -> 12..86
    fprintf(f, "echo \"[W3D] 12 Compiling (%%s)|%s\"\n", ConsoleName(plat));
    fputs("TOTAL=$(make -s count 2>/dev/null || echo 150)\n", f);
    fprintf(f, "make %s 2>&1 | awk -v t=\"$TOTAL\" -v name=\"%s\" '{print; fflush()}\n"
               "  /^  (c[+][+]|cc ) /{n++; p=12+int(n*74/(t>0?t:1)); if(p>86)p=86; "
               "printf \"[W3D] %%d Compiling (%%%%s)|%%s\\n\", p, name; fflush()}\n"
               "  /^  link /{print \"[W3D] 88 Linking\"; fflush()}'\n",
            W3dCompileJobsFlag().c_str(), ConsoleName(plat));
    fputs("echo \"[W3D] 92 Copying results\"\n", f);
    bool ok = (ferror(f) == 0);
    if (fclose(f) != 0) ok = false;
    return ok;
}

// ============================================================================
//  LINUX APPIMAGE (plataforma 1): un solo archivo <juego>-linux-x86_64.AppImage que corre en cualquier Linux.
//  Lo arma este script despues de compilar, con las mismas herramientas que el AppImage del editor
//  (platform/linux/build_appimage.sh: linuxdeploy y appimagetool, en ~/.cache/whisk3d-appimage o en el PATH):
//    AppDir/usr/bin/<juego>   el ejecutable y, si van sueltos, sus assets al lado
//    AppDir/AppRun            entra a esa carpeta (el juego lee sus assets de la carpeta actual) y lo corre
//    linuxdeploy              mete adentro SDL2 y lo demas que no viene con todo Linux, y appimagetool lo empaqueta
//  Con Vulkan y sin shaderc adentro va tambien el libshaderc del sistema (se abre en tiempo de ejecucion: linuxdeploy
//  no lo ve solo).
// ============================================================================
static bool WriteAppImage(const std::string& path, const std::string& out, const std::string& name,
                             bool pack, bool vulkan, const std::string& icon) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs("#!/usr/bin/env bash\n# GENERADO por Whisk3D (Compilar juego -> Linux AppImage). Arma el AppImage del juego ya "
          "compiled (out/).\nset -euo pipefail\n", f);
    fprintf(f, "cd \"%s\"\nN=\"%s\"\n", Esc(out).c_str(), name.c_str());
    fputs("CACHE=\"${APPIMAGE_TOOLS:-$HOME/.cache/whisk3d-appimage}\"\n"
          "LD=$(command -v linuxdeploy || echo \"$CACHE/linuxdeploy-x86_64.AppImage\")\n"
          "AT=$(command -v appimagetool || echo \"$CACHE/appimagetool-x86_64.AppImage\")\n"
          "[ -x \"$LD\" ] && [ -x \"$AT\" ] || { echo \"ERROR: faltan linuxdeploy y appimagetool: correr una vez "
          "Whisk3D/platform/linux/build_appimage.sh (las deja en $CACHE)\"; exit 1; }\n"
          "export PATH=\"$(dirname \"$AT\"):$PATH\" APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64\n"
          "# (linuxdeploy busca appimagetool en el PATH con ese nombre)\n"
          "mkdir -p out/herramientas && ln -sf \"$AT\" out/herramientas/appimagetool && export PATH=\"$PWD/out/herramientas:$PATH\"\n"
          "rm -rf out/AppDir out/appimage && mkdir -p out/AppDir/usr/bin out/appimage\n"
          "cp \"out/$N\" out/AppDir/usr/bin/\n", f);
    if (!pack) {
        fputs("# los assets, al lado del ejecutable\n"
              "for a in *.w3dui *.lua *.png *.json; do if [ -f \"$a\" ]; then cp \"$a\" out/AppDir/usr/bin/; fi; done\n", f);
        std::string dirs;
        for (int c = 0; CarpAsset(c); c++) { dirs += " "; dirs += CarpAsset(c); }
        if (!dirs.empty())
            fprintf(f, "for d in%s; do if [ -d \"$d\" ]; then cp -r \"$d\" out/AppDir/usr/bin/; fi; done\n", dirs.c_str());
    }
    fputs("printf '#!/bin/sh\\nAQUI=\"$(dirname \"$(readlink -f \"$0\")\")\"\\ncd \"$AQUI/usr/bin\" && exec ./%s \"$@\"\\n' \"$N\" "
          "> out/AppRun\nchmod +x out/AppRun\n"
          "printf '[Desktop Entry]\\nType=Application\\nName=%s\\nComment=Juego hecho con Whisk3D\\nExec=%s\\nIcon=%s\\n"
          "Terminal=false\\nCategories=Game;\\n' \"$N\" \"$N\" \"$N\" > \"out/$N.desktop\"\n", f);
    fprintf(f, "cp \"%s\" \"out/$N.png\"\n", Esc(icon).c_str());
    fputs("EXTRA=()\n", f);
    if (vulkan)
        fputs("# Vulkan sin shaderc adentro: el del sistema (el paquete libshaderc1), si esta\n"
              "if [ ! -f \"$HOME/shaderc/linux/lib/libshaderc_combined.a\" ]; then\n"
              "  for l in /lib/x86_64-linux-gnu/libshaderc.so.1 /usr/lib/x86_64-linux-gnu/libshaderc.so.1; do\n"
              "    if [ -f \"$l\" ]; then EXTRA=(--library \"$l\"); break; fi\n  done\nfi\n", f);
    fputs("( cd out/appimage && \"$LD\" --appdir ../AppDir --executable \"../AppDir/usr/bin/$N\" "
          "--desktop-file \"../$N.desktop\" --icon-file \"../$N.png\" --custom-apprun ../AppRun \"${EXTRA[@]}\" "
          "--output appimage )\n"
          "rm -f \"out/$N-linux-x86_64.AppImage\"\n"
          "mv out/appimage/*.AppImage \"out/$N-linux-x86_64.AppImage\"\n"
          "echo \"  AppImage out/$N-linux-x86_64.AppImage\"\n", f);
    bool ok = (ferror(f) == 0);
    if (fclose(f) != 0) ok = false;
    return ok;
}

// ============================================================================
//  SYMBIAN (plataforma 5): el KIT para compilar en Windows con el SDK de S60.
//
//  El juego de Symbian es la app de Whisk3D en MODO JUEGO: si el .sisx trae un proyecto en su carpeta privada
//  (game\game.w3d), la app lo abre sola, con el 3D a pantalla completa (platform/symbian/src/Whisk3D.cpp). Lleva la
//  UID del editor: instalar el juego reemplaza al editor en el telefono.
//
//  En platform-build/symbian (y en build/symbian) quedan:
//    game.w3d            el proyecto GUARDADO (un contenedor: lleva todo adentro)
//    <juego>.pkg          el instalador: el de Whisk3D (sis/Whisk3D_gcce.pkg) con el nombre del juego y el proyecto
//    build_symbian.bat    en Windows, con el SDK: compila la app (bldmake + abld, GCCE) y arma <juego>.sisx
//                         (makesis + signsis con el certificado del repo)
//  Con el backend de shaders (OpenGL ES 2.0: el N8 y los Symbian^3) usa la variante Whisk3D_gles2.mmp.
// ============================================================================
static bool WriteKitSymbian(const std::string& out, const std::string& repo, const std::string& name,
                               bool shaders, std::string* err) {
    extern std::string w3dPath;
    if (w3dPath.empty() || !W3dContainerHasMounted()) {
        *err = T("Build: Symbian packs the saved project: save it first (File > Save)");
        return false;
    }
    std::vector<unsigned char> w3d;
    if (!w3dFileSystem::ReadFileBytes(w3dPath, w3d) || w3d.empty()) { *err = w3dPath; return false; }
    { FILE* f = fopen((out + "/game.w3d").c_str(), "wb");
      if (!f) { *err = out + "/game.w3d"; return false; }
      fwrite(&w3d[0], 1, w3d.size(), f); fclose(f); }
    // el .pkg: el de Whisk3D (GCCE), con el nombre del juego y su proyecto en game\game.w3d
    std::vector<unsigned char> base;
    const std::string pkgBase = repo + "/platform/symbian/sis/Whisk3D_gcce.pkg";
    if (!w3dFileSystem::ReadFileBytes(pkgBase, base) || base.empty()) { *err = pkgBase; return false; }
    std::string pkg((const char*)&base[0], base.size()), sal;
    size_t i = 0;
    while (i < pkg.size()) {
        size_t end = pkg.find('\n', i);
        std::string l = pkg.substr(i, end == std::string::npos ? std::string::npos : end - i + 1);
        i = (end == std::string::npos) ? pkg.size() : end + 1;
        if (l.compare(0, 12, "#{\"Whisk3D\"}") == 0) l = "#{\"" + name + "\"}" + l.substr(12);
        if (shaders) {
            size_t p = l.find("gcce\\urel\\Whisk3D.exe\"");
            if (p != std::string::npos) l.replace(p, 22, "gcce\\urel\\whisk3D_gles2.exe\"");
            if (l.compare(0, 12, "[0x101F7961]") == 0) l = "[0x20022E6D], 0, 0, 0, {\"Symbian^3\"}\n";
        }
        sal += l;
    }
    sal += "\n; el proyecto del juego: la app lo abre sola en modo juego\n"
           "\"w3dgame.w3d\"   -\"!:\\private\\A00D021D\\game\\game.w3d\"\n";
    { FILE* f = fopen((out + "/" + name + ".pkg").c_str(), "wb");
      if (!f) { *err = out + "/" + name + ".pkg"; return false; }
      fwrite(sal.data(), 1, sal.size(), f); fclose(f); }
    // el .bat: la carpeta del repo en Windows sale de W3D (variable de entorno), o de la ruta del repo si el editor
    // corre en Windows
    std::string repoWin = (repo.size() > 1 && repo[1] == ':') ? repo : std::string("C:\\Whisk3D");
    for (size_t k = 0; k < repoWin.size(); k++) if (repoWin[k] == '/') repoWin[k] = '\\';
    const char* mmp = shaders ? "Whisk3D_gles2" : "Whisk3D";
    std::string bat;
    bat += "@echo off\r\n";
    bat += "REM GENERADO por Whisk3D (Compilar juego -> Symbian). Se corre en Windows, con el SDK de S60 3rd Edition\r\n";
    bat += "REM (o el de Symbian^3 para OpenGL ES 2.0) y su GCCE en el PATH, como para compilar el editor.\r\n";
    bat += "REM La carpeta del repo de Whisk3D: la variable W3D, o la de abajo.\r\n";
    bat += "setlocal\r\n";
    bat += "if \"%W3D%\"==\"\" set \"W3D=" + repoWin + "\"\r\n";
    bat += "if not exist \"%W3D%\\platform\\symbian\\group\\Whisk3D.mmp\" (echo ERROR: no esta el repo de Whisk3D en %W3D% "
           "^(poner su carpeta en la variable W3D^) & exit /b 1)\r\n";
    bat += "pushd \"%W3D%\\platform\\symbian\\group\" || exit /b 1\r\n";
    bat += "call bldmake bldfiles || goto :error\r\n";
    bat += std::string("call abld build gcce urel ") + mmp + " || goto :error\r\n";
    bat += "popd\r\n";
    bat += "copy /y \"%~dp0" + name + ".pkg\" \"%W3D%\\platform\\symbian\\sis\\w3dgame.pkg\" > nul || exit /b 1\r\n";
    bat += "copy /y \"%~dp0juego.w3d\" \"%W3D%\\platform\\symbian\\sis\\w3dgame.w3d\" > nul || exit /b 1\r\n";
    bat += "pushd \"%W3D%\\platform\\symbian\\sis\" || exit /b 1\r\n";
    bat += "makesis w3dgame.pkg w3dgame.sis || goto :error\r\n";
    bat += "signsis w3dgame.sis \"%~dp0" + name + ".sisx\" whisk3d_selfsign.cer whisk3d_selfsign.key || goto :error\r\n";
    bat += "del w3dgame.pkg w3dgame.w3d w3dgame.sis\r\n";
    bat += "popd\r\n";
    bat += "echo listo: %~dp0" + name + ".sisx\r\n";
    bat += "exit /b 0\r\n";
    bat += ":error\r\n";
    bat += "echo ERROR: fallo la compilacion de Symbian\r\n";
    bat += "popd\r\n";
    bat += "exit /b 1\r\n";
    { FILE* f = fopen((out + "/build_symbian.bat").c_str(), "wb");
      if (!f) { *err = out + "/build_symbian.bat"; return false; }
      fwrite(bat.data(), 1, bat.size(), f); fclose(f); }
    return true;
}

// ----------------------------------------------------------------------------
//  El TRABAJO del worker: todo por VALOR (strings propios). El worker no ve la
//  escena ni la UI: solo corre estos comandos y copia archivos.
struct BldJob {
    int platform;            // 0 .deb / 1 AppImage / 2 web / 3 android / 6 android 2.3 / 7..10 las consolas
    std::string plat;        // "linux" / "web" / "android" (para los mensajes)
    std::string out;         // <juego>/platform-build/<plat> (ahi vive build.log)
    std::string cmdCfg;      // linux: cmake -S . -B out (configurar)
    std::string cmdBuild;    // linux: cmake --build | web: emcc | android: bash script
    std::string cmdCpack;    // linux .deb: cpack (vacio = no empaquetar)
    std::string cmdCopy;     // [4/4]: limpiar build/<plat> y copiar los artefactos
    std::string msgOk;       // "Game built: ..." (lo arma el hilo principal, ya traducido)
};

// el HILO WORKER: [3/4] compilar + [4/4] copiar resultados. Corre los procesos con
// BldRun (que vuelca la salida a build.log y publica el avance) y termina
// seteando g_bldEnded: el Tick del hilo principal hace join + Notificar.
//  Porcentaje GLOBAL por plataforma (0..5 fue la fase sincrona de generacion):
//   linux: configurar 8..15, compilar 15..85 (el "[ NN%]" REAL de cmake --build),
//          cpack .deb 85..94 (creep), copiar 95..100
//   web:   emcc 10..85 (creep: emcc no da %; avanza lento por salida/tiempo), copiar 95..100
//   android: 10..90 con las marcas "[W3D] <pct> <etapa>" del script generado
//          (plantilla 12, ndk-build 16, jniLibs 58, gradle 60) + creep entre marcas
static void BldWorker(BldJob t) {
    FILE* log = fopen((t.out + "/build.log").c_str(), "w");
    int rc;
    if (WithMakefile(t.platform) || t.platform == PlatSymbian) {
        BldPublish(BldFmt(T("Compiling (%s)"), ConsoleName(t.platform)), 10);
        rc = BldRun(t.cmdBuild, log, BLD_MARKS, 10, 92);
    } else if (t.platform == 3 || t.platform == 6) {
        BldPublish(t.platform == 6 ? T("Preparing Android 2.3") : T("Preparing Android (template)"), 10);
        rc = BldRun(t.cmdBuild, log, BLD_MARKS, 10, 90);
    } else if (t.platform == 2) {
        BldPublish(BldFmt(T("Compiling (%s)"), "emcc"), 10);
        rc = BldRun(t.cmdBuild, log, BLD_CREEP, 10, 85);
    } else {
        BldPublish(T("Configuring (cmake)"), 8);
        rc = BldRun(t.cmdCfg, log, BLD_CREEP, 8, 15);
        if (rc == 0) {
            BldPublish(BldFmt(T("Compiling (%s)"), "cmake"), 15);
            rc = BldRun(t.cmdBuild, log, BLD_CMAKE, 15, 85);
        }
        if (rc == 0 && !t.cmdCpack.empty()) {
            if (t.platform == 1) {   // el AppImage ES el resultado: si no se arma, fallo
                BldPublish(T("Packaging AppImage"), 85);
                rc = BldRun(t.cmdCpack, log, BLD_CREEP, 85, 94);
            } else {
                BldPublish(T("Packaging .deb (cpack)"), 85);
                // como siempre: si cpack falla el binario igual queda (no es fatal)
                BldRun(t.cmdCpack, log, BLD_CREEP, 85, 94);
            }
        }
    }
    if (log) fclose(log);
    if (rc != 0) {
        { BLD_LOCK(); g_bldFinal = BldFmt(T("Build: compilation failed (see platform-build/%s/build.log)"), t.plat); }
        g_bldOk.store(false);
        g_bldEnded.store(true);   // SIEMPRE al final: es la senal para el Tick
        return;
    }
    // [4/4] SOLO filesystem: limpiar build/<plat> y copiar los artefactos finales
    BldPublish(T("Copying results"), 95);
    if (system(t.cmdCopy.c_str()) != 0) {
        { BLD_LOCK(); g_bldFinal = BldFmt(T("Build: could not copy the result to build/%s"), t.plat); }
        g_bldOk.store(false);
        g_bldEnded.store(true);
        return;
    }
    BldPublish(T("Done"), 100);
    { BLD_LOCK(); g_bldFinal = t.msgOk; }
    g_bldOk.store(true);
    g_bldEnded.store(true);
}

// estado inicial de la barra + LANZAR el worker. La UI vuelve al loop enseguida:
// el editor sigue usable y la barra (overlay abajo) muestra etapa + porcentaje.
static void BldLaunch(const BldJob& t, const char* namePlat) {
    g_bldPct.store(5);   // 0..5 = la fase sincrona de arriba (exportar + generar)
    { BLD_LOCK(); g_bldStage = BldFmt(T("Compiling (%s)"), namePlat); g_bldFinal.clear(); }
    g_bldOk.store(false);
    g_bldEnded.store(false);
    g_bldActive.store(true);
    g_redraw = true;     // que la barra aparezca ya mismo
#ifndef __EMSCRIPTEN__
    g_bldThread = new std::thread(BldWorker, t);
#else
    // editor web: sin hilos (ver arriba). Corre sincrono y falla claro: popen/system
    // no existen en el sandbox del browser (igual que antes de este cambio).
    BldWorker(t);
#endif
}

// 1x por frame en el HILO PRINCIPAL (lo llama MainLoopFrame): mientras el build corre
// mantiene vivo el render (la barra avanza) y, cuando el worker termina, hace join y
// dispara el Notificar de exito/error (la UI se toca SOLO desde aca, nunca del worker).
void CompileGameTick() {
    if (!g_bldActive.load()) return;
    g_redraw = true;   // seguir dibujando: la barra (overlay) se actualiza cada frame
    if (!g_bldEnded.load()) return;
#ifndef __EMSCRIPTEN__
    if (g_bldThread) {
        if (g_bldThread->joinable()) g_bldThread->join();
        delete g_bldThread; g_bldThread = NULL;
    }
#endif
    std::string msg; { BLD_LOCK(); msg = g_bldFinal; }
    bool ok = g_bldOk.load();
    g_bldEnded.store(false);
    g_bldActive.store(false);   // recien aca se puede disparar otro build
    W3dDockProgressEnd();       // el overlay dejo de dibujarse: apagar la barra del icono (salga OK o con error)
    Notify(msg, !ok);
    if (ok) w3dLogf("CompilarJuego OK: %s", msg.c_str());
    else    w3dLogfE("CompileGame: %s", msg.c_str());
}

// ============================================================================
//  ES UN JUEGO 3D? — decide si el binario lleva el pase 3D o no.
//
//  Un juego 3D es el que tiene ALGO que dibujar en 3D: una malla, una camara,
//  una luz, una curva/riel, un espejo, una coleccion, un LOD, un culling, una
//  zona de visibilidad, un emisor de particulas, una instancia (o un proxy de una
//  libreria). Un juego 2D
//  (una UI con scripts, como el whiskpaddle) no tiene nada de eso y se compila
//  como siempre: sin el lector de proyecto ni el pase 3D, o sea un binario mucho
//  mas chico. La deteccion es automatica -- el usuario no tiene que elegir nada.
// ============================================================================
static bool IsObject3D(Object* o) {
    if (!o) return false;
    switch (o->getType()) {
        case ObjectType::mesh:       case ObjectType::camera:
        case ObjectType::light:      case ObjectType::curve:
        case ObjectType::mirror:     case ObjectType::instance:
        case ObjectType::collection: case ObjectType::lod:
        case ObjectType::culling:    case ObjectType::viszona:
        case ObjectType::particles: case ObjectType::armature:
        case ObjectType::fog:
        case ObjectType::clearz:   case ObjectType::clip:
        case ObjectType::hitbox:     case ObjectType::prefab:
        case ObjectType::lightProbe:   // (la luz indirecta de lo 3D)
        case ObjectType::reflectionProbe:   // (los reflejos de lo 3D)
        case ObjectType::proxy:       // (un proxy de una libreria: lo que genera es contenido 3D)
        case ObjectType::routine:     // (una rutina dibuja 3D)
            return true;
        default: return false;
    }
}
static bool HasThree3DRec(Object* o) {
    if (!o) return false;
    if (IsObject3D(o)) return true;
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (HasThree3DRec(o->Childrens[i])) return true;
    return false;
}
// OJO: la RAIZ no cuenta. SceneCollection es un Scene, y Scene::getType() devuelve
// 'collection' (es una coleccion, la de toda la escena): mirarla hacia que CUALQUIER
// proyecto -- hasta uno 2D puro -- se compilara como juego 3D.
static bool HasThree3D(Object* root) {
    if (!root) return false;
    for (size_t i = 0; i < root->Childrens.size(); i++)
        if (HasThree3DRec(root->Childrens[i])) return true;
    return false;
}

// EL PROYECTO TIENE 3D: lo activo (HasThree3D) o mas de una raiz 3D en el registro (los juegos y escenas del proyecto)
static bool ProjectHas3D() {
    if (HasThree3D(SceneCollection)) return true;
    int n3d = 0;
    const std::vector<W3dRootRow>& fs = W3dRoots();
    for (size_t i = 0; i < fs.size(); i++) if (W3dRootEs3D(fs[i].type)) n3d++;
    return n3d > 1;
}
bool CompileGameEs3D() { return ProjectHas3D(); }
// HAY UN JUEGO QUE COMPILAR: una raiz de tipo JUEGO (la marcada asi, o una con scripts: W3dRootTypeOf), o un juego
// solo 2D (una UI con scripts: el whiskpaddle, la calculadora). Un juego 3D no necesita UI
bool CompileGameHasGame() {
    const std::vector<W3dRootRow>& fs = W3dRoots();
    for (size_t i = 0; i < fs.size(); i++) if (W3dRootTypeOf((int)i) == W3D_ROOT_GAME) return true;
    if (W3dRootActiveType() == W3D_ROOT_GAME) return true;
    if (SceneCollection)
        for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
            Object* o = SceneCollection->Childrens[i];
            if (!o || o->getType() != ObjectType::ui) continue;
            std::vector<ObjScript> sc; std::vector<std::string> r;
            CollectScripts(o, &sc, &r);
            if (!sc.empty()) return true;
        }
    return false;
}

// el valor de una clave de TEXTO de la raiz de un JSON (el project.json del staging): la pisa o la agrega
static void JsonSetTextRoot(const std::string& path, const std::string& key, const std::string& value) {
    std::vector<unsigned char> d;
    if (!w3dFileSystem::ReadFileBytes(path, d) || d.empty()) return;
    std::string j((const char*)&d[0], d.size()), v;
    JsonText(v, value);
    const std::string k = "\"" + key + "\":";
    size_t p = j.find(k);
    if (p != std::string::npos) {
        size_t a = j.find('"', p + k.size());                 // la comilla que abre el valor
        size_t b = (a == std::string::npos) ? a : a + 1;
        while (b != std::string::npos && b < j.size() && j[b] != '"') b += (j[b] == '\\') ? 2 : 1;
        if (a == std::string::npos || b >= j.size()) return;
        j = j.substr(0, a) + v + j.substr(b + 1);
    } else {
        size_t a = j.find('{');
        if (a == std::string::npos) return;
        j.insert(a + 1, "\n  " + k + " " + v + ",");
    }
    FILE* f = fopen(path.c_str(), "wb");
    if (f) { fwrite(j.data(), 1, j.size(), f); fclose(f); }
}

bool CompileGame(UI* u, int platform, int windowMode, int orientation,
                   bool usePhysics, bool useSound, bool debugMode,
                   bool packAssets) {
    // ya hay un build corriendo: NO se dispara otro (el resto del editor sigue usable;
    // la barra de abajo muestra el que esta en curso)
    if (g_bldActive.load()) {
        Notify(T("Build: a build is already running"), true);
        return false;
    }
    // Windows (4) y Symbian (5): el pipeline de build de estos targets todavia NO esta
    // cableado a este boton (el flujo de abajo genera el runtime SDL/cmake de Linux/web/
    // android). Se hace aparte para no romper este camino. Mensaje claro en vez de fallar raro.
#ifdef _WIN32
    // (Windows se compila desde Linux, con llvm-mingw: el editor de Windows no tiene bash para los scripts)
    if (platform == 4) {
        Notify(T("Windows .exe: not generated from this button yet. The EDITOR is built with platform/windows/build_windows.bat."), true);
        return false;
    }
#endif
    // HAY UN JUEGO? (un juego 3D no necesita UI; uno solo 2D es su UI con scripts)
    if (!CompileGameHasGame()) {
        Notify(T("Build: the project has no game (a Game scene, or a UI with scripts)"), true);
        return false;
    }
    // JUEGO 3D: hay algo que dibujar en 3D -> el binario lleva el pase 3D y la escena
    // viaja como project.json al lado (ver IsObject3D / kSources3D)
    const bool with3D = ProjectHas3D();
    if (!with3D && !u) {
        Notify(T("Build: there is no UI in the scene"), true);
        return false;
    }
    std::vector<ObjScript> objs;
    std::vector<std::string> pathsLua;
    if (SceneCollection) CollectScripts(SceneCollection, &objs, &pathsLua);
    if (objs.empty() && !with3D) {
        Notify(T("Build: the project has no scripts (add them to a Script object)"), true);
        return false;
    }
    std::string repo = RepoRoot(NULL);
    if (repo.empty()) {
        Notify(T("Build: the Whisk3D repo (runtime sources) was not found. Set the repo folder in Settings (or build from the source tree)."), true);
        return false;
    }
    // la carpeta del JUEGO: la del .w3d abierto. Sin proyecto guardado queda la
    // heuristica de siempre (la carpeta del primer .lua) -- que con la
    // estructura content/ ya no es la raiz, por eso manda el .w3d.
    // (pathsLua puede venir VACIO: scripts con ruta vacia; ultimo recurso ".")
    extern std::string w3dPath;   // el proyecto abierto (app/variables.h)
    std::string proj = !w3dPath.empty()   ? Folder(w3dPath)
                     : !pathsLua.empty()  ? Folder(pathsLua[0])
                                          : std::string(".");
    // ESTRUCTURA de salida en la carpeta del juego (las dos van al .gitignore):
    //   platform-build/<plat>/  TODO lo generado para compilar, autocontenido por
    //                           plataforma: main.cpp + CMakeLists/script + assets
    //                           copiados + build.log + el build intermedio (out/ para
    //                           cmake/emcc, project/ para la copia Android). Cada
    //                           plataforma se puede borrar/regenerar por separado.
    //   build/<plat>/           SOLO los RESULTADOS: ejecutable+instaladores (linux),
    //                           .html/.js/.wasm (web), .apk (android). Se limpia y
    //                           rellena al final de cada compilacion.
    const char* plat = (WithMakefile(platform) || platform == PlatSymbian) ? DirConsole(platform)
                     : (platform == 6) ? "android23" : (platform == 3) ? "android"
                     : (platform == 2) ? "web" : "linux";
    // (las rutas del proyecto van a los comandos de cmake, cp y rm: una carpeta con $ ` " \ el shell la interpretaria)
    if (!ShellSafe(proj)) {
        Notify(T("Build: the project folder has characters the build can not use ($ ` \" \\)"), true);
        return false;
    }
    std::string out     = proj + "/platform-build/" + plat;
    std::string results = proj + "/build/" + plat;
    // nombre del JUEGO (proyecto CMake / binario / artefactos): el archivo .w3d ABIERTO
    // (whiskpaddle.w3d -> "whiskpaddle"), saneado a un identificador seguro. Antes se
    // usaba el nombre de la PRIMERA ESCENA ("Menu") y el .deb salia "whisk3d-Menu-...".
    // Sin .w3d guardado: la carpeta del juego; ultimo recurso "juego". Los NOMBRES de
    // escena (dentro de cada .w3dui) quedan como estan.
    std::string name;
    if (!w3dPath.empty()) { std::string b = BaseNoExt(w3dPath); if (!b.empty()) name = Slug(b); }
    if (name.empty())   { std::string b = Base(proj); if (!b.empty() && b != ".") name = Slug(b); }
    if (name.empty()) name = "game";
    MakeDirs(out);
    EnsureGitignore(proj);   // platform-build/ y build/ fuera del repo del juego

    // SYMBIAN: el kit para compilar en Windows (ver WriteKitSymbian). No usa el staging: el telefono abre el .w3d
    if (platform == PlatSymbian) {
        std::string err;
        if (!WriteKitSymbian(out, repo, name, W3dCompileBackendOf(platform) == 1, &err)) {
            Notify(std::string(T("Build: could not generate the Symbian project")) + " (" + err + ")", true);
            return false;
        }
        BldJob t;
        t.platform = platform; t.plat = plat; t.out = out;
        t.cmdBuild = "echo \"[W3D] 90 Copying results\"";
        char cmd[3000];
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/game.w3d\" \"%s/%s.pkg\" "
                 "\"%s/build_symbian.bat\" \"%s/\"", results.c_str(), results.c_str(), out.c_str(), out.c_str(),
                 name.c_str(), out.c_str(), results.c_str());
        t.cmdCopy = cmd;
        t.msgOk = BldFmt(T("Symbian project ready: %s (build it on Windows with build_symbian.bat)"),
                         name + " -> build/symbian/");
        BldLaunch(t, "Symbian");
        return true;
    }

    // ========================================================================
    //  UN PAQUETE POR COMPILACION: LIMPIAR LOS ARTEFACTOS DE LA ANTERIOR.
    //
    //  platform-build/<plat> se REUSA entre compilaciones (el build incremental
    //  de cmake vive ahi y es lo que hace que recompilar tarde 20 s y no 8 min),
    //  pero los artefactos con el NOMBRE DEL JUEGO adentro no se pueden reusar:
    //  al compilar OTRO proyecto -- o el mismo con otro nombre de archivo, que es
    //  lo que pasa cuando la carpeta tiene el .w3d de texto Y su contenedor v4
    //  empaquetado al lado -- quedaban conviviendo los dos:
    //    * out/<juego>-linux-<arch>.deb  de cpack, y el paso [4/4] copia
    //      out/*.deb ENTERO -> build/linux salia con DOS .deb, uno de ellos de
    //      una compilacion vieja de otro proyecto (es el bug reportado);
    //    * <juego>.w3dui en la raiz del staging, que el CMake instala por GLOB
    //      -> el .deb se llevaba tambien la escena del otro proyecto;
    //    * el binario viejo, que la copia de resultados dejaba al lado del nuevo.
    //  Se borran ACA, antes de generar nada: lo unico que sobrevive es out/ (el
    //  build incremental) menos sus paquetes.
    //
    //  OJO: no se borra "el .w3d de al lado" ni se lo trata como escena -- un
    //  contenedor v4 es UN PROYECTO, no un asset ni una escena, y el compilador
    //  nunca lo mira: la escena a compilar es SIEMPRE la que esta abierta.
    // ========================================================================
    {
        char cmd[3000];
        // (los cuatro globs de la raiz los REESCRIBE esta misma compilacion unas
        //  lineas mas abajo: escenas .w3dui, scripts .lua, project.json y los
        //  .png sueltos -- font.png/icono.png y los de un proyecto v3.)
        snprintf(cmd, sizeof(cmd),
                 "rm -f \"%s\"/*.w3dui \"%s\"/*.lua \"%s\"/*.json \"%s\"/*.png "
                 "\"%s\"/*.deb \"%s\"/*.AppImage "
                 "\"%s/out\"/*.deb \"%s/out\"/*.AppImage 2>/dev/null || true",
                 out.c_str(), out.c_str(), out.c_str(), out.c_str(),
                 out.c_str(), out.c_str(), out.c_str(), out.c_str());
        if (ShellRun(cmd, out)) {}
    }

    // MULTI-ESCENA vs UNA sola: las escenas son los UI raiz de la escena (tipo ui). Con 2+ el
    // juego cambia entre ellas con cambiarEscena() -> hay que exportar y cargar TODAS. Con 1 (o
    // un proyecto legacy con los scripts en un objeto Script) se mantiene el camino de siempre.
    std::vector<UI*> scenesUI;
    CollectScenes(&scenesUI);
    bool multi = (scenesUI.size() >= 2);
    // JUEGO 3D: hay algo que dibujar en 3D -> el binario lleva el pase 3D y la escena
    // viaja como project.json al lado (ver IsObject3D / kSources3D). Ver mas abajo.
    // (with3D: ver arriba, ProjectHas3D)

    // [1/4] exportar las escenas + copiar los scripts + las texturas + la tipografia
    // (fase SINCRONA y rapida en el hilo de UI: es lo unico que toca la escena. El
    // avance visible ahora lo cuenta la BARRA, no los toasts de antes.)
    w3dLogf("CompileGame [1/4]: exportando escenas y scripts");
    std::string w3duiLegacy = name + ".w3dui";       // solo el camino legacy (1 escena)
    std::vector<SceneGen> scenes;
    if (multi) {
        // cada escena -> su .w3dui en platform-build/<plat>, con las rutas relativas calculadas contra la
        // carpeta del PROYECTO (proy): asi quedan PLANAS ("menu.lua", "speaker.png") y los assets
        // que copiamos al lado resuelven igual en PC (.deb/AppImage), APK y WebGL.
        std::vector<std::string> used;              // slugs ya usados (evitar colisiones)
        for (size_t i = 0; i < scenesUI.size(); i++) {
            UI* e = scenesUI[i];
            std::string slug = Slug(e->name.empty() ? std::string("scene") : e->name);
            std::string cand = slug;
            for (int n = 2;; n++) {
                bool collides = false;
                for (size_t k = 0; k < used.size(); k++) if (used[k] == cand) { collides = true; break; }
                if (!collides) break;
                char suf[16]; snprintf(suf, sizeof(suf), "_%d", n); cand = slug + suf;
            }
            used.push_back(cand);
            std::string arch = cand + ".w3dui";
            if (!UI2DSave(e, out + "/" + arch, proj)) {
                Notify(T("Build: could not write a scene (.w3dui)"), true); return false;
            }
            SceneGen g; g.file = arch; g.name = e->name; scenes.push_back(g);
        }
    } else {
        // UNA sola escena / legacy: exportar la UI activa. base = carpeta del proyecto (proy)
        // para que texturas/fuente queden relativas planas (igual que multi) y resuelvan al lado.
        // (un juego 3D sin UI no tiene escena 2D que exportar)
        if (u && !UI2DSave(u, out + "/" + w3duiLegacy, proj)) {
            Notify(T("Build: could not write the .w3dui"), true); return false;
        }
    }
    // los scripts .lua (de TODAS las escenas) al lado, planos
    for (size_t i = 0; i < pathsLua.size(); i++) {
        if (W3dIsEntryName(pathsLua[i])) continue;   // sale del contenedor (mas abajo)
        char cmd[1400];
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s/%s\"", pathsLua[i].c_str(), out.c_str(), Base(pathsLua[i]).c_str());
        if (ShellRun(cmd, pathsLua[i], out)) {}
    }
    // ==================================================================
    //  DE DONDE SACA SUS ARCHIVOS EL JUEGO COMPILADO: UNA SOLA PASADA.
    //
    //  El runtime del juego NO SABE que existe el .w3d: lee .w3dui + assets por
    //  RUTA RELATIVA contra su asset root (la carpeta del binario en PC, el
    //  AAssetManager en Android, el FS embebido de emscripten en web, o el pak si
    //  van empaquetados). Asi que el compilador tiene que DEJARLE esos archivos.
    //
    //  Los saca del CONTENEDOR y de ningun otro lado: se vuelca cada entrada del
    //  .w3d a platform-build/<plat>/<mismo nombre de entrada>. El staging queda
    //  siendo un ESPEJO EXACTO del zip, que es justo lo que los .w3dui exportados
    //  referencian ("textures/pausa.png"), asi que no hay ninguna traduccion de
    //  rutas en el medio. De ahi salen los cuatro targets (copia del CMake, cp del
    //  script de Android, --embed-file de emcc, o el pak), todos por kFoldersAssets.
    //
    //  UNA pasada, no cinco: antes ademas del volcado se copiaban a mano los *.png
    //  de la carpeta del proyecto, la subcarpeta assets/, y despues se RE-PARSEABAN
    //  los .w3dui recien escritos con el colector del guardado por versiones para
    //  replicar content/. Cuatro fuentes distintas para el mismo archivo, cada
    //  una con su propia idea de que ruta es valida. Con el .w3d empaquetado la
    //  fuente es UNA: el contenedor.
    //
    //  Se saltean: las entradas de SERVICIO (mimetype, README.txt, project.json,
    //  EXTERNALS.txt), que describen el contenedor y no son assets; y scenes/,
    //  porque las escenas las RE-EXPORTA el paso de arriba a la raiz del staging
    //  con el nombre que el main.cpp generado va a cargar (volcarlas ademas dejaria
    //  dos copias de cada escena, y la del zip no la abre nadie).
    // ==================================================================
    bool fromContainer = W3dContainerHasMounted();
    if (fromContainer) {
        W3dZipReader* lec = W3dContainerReader();
        std::vector<std::string> ents;
        if (lec) lec->List(ents);
        // se limpian las carpetas de assets viejas ANTES de volcar: un asset que el
        // usuario borro del proyecto no puede sobrevivir en la compilacion anterior
        for (int c = 0; kFoldersAssets[c]; c++) {
            char cmd[1400];
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s/%s\"", out.c_str(), kFoldersAssets[c]);
            if (ShellRun(cmd, out)) {}
        }
        // y la scenes/ que pudo haber dejado una compilacion anterior (ya no se vuelca)
        if (!with3D) { char cmd[1400]; snprintf(cmd, sizeof(cmd), "rm -rf \"%s/scenes\"", out.c_str()); if (ShellRun(cmd, out)) {} }
        int dumped = 0;
        for (size_t i = 0; i < ents.size(); i++) {
            // project.json ES la ESCENA 3D del juego: en un juego 3D viaja como un asset
            // mas (lo lee W3dGameLoadScene3D). En un juego 2D sigue siendo servicio.
            if (W3dIsServiceEntry(ents[i]) && !(with3D && ents[i] == "project.json")) continue;
            // scenes/: en 2D no se vuelcan (el compilador las re-exporta planas a la raiz);
            // en 3D SI se vuelcan y ademas el compilador las pisa con la version VIVA, porque
            // el project.json las referencia por ese nombre de entrada.
            if (!with3D && ents[i].compare(0, 7, "scenes/") == 0) continue;
            std::vector<unsigned char> data;
            if (!lec->Read(ents[i], data)) continue;
            std::string dst = out + "/" + ents[i];
            MakeDirs(Folder(dst));
            FILE* fh = fopen(dst.c_str(), "wb");
            if (!fh) continue;
            if (!data.empty()) fwrite(&data[0], 1, data.size(), fh);
            if (fclose(fh) == 0) dumped++;
        }
        w3dLogf("CompileGame: %d entrada(s) del .w3d volcadas al staging (UNA pasada)", dumped);
    } else {
        // ------------------------------------------------------------------
        //  PROYECTO v3 (JSON plano con los assets SUELTOS al lado del .w3d): no
        //  hay contenedor del que volcar, asi que sigue el camino de siempre.
        //  Se abre y se MIGRA al primer guardado; hasta entonces compila igual.
        // ------------------------------------------------------------------
        // los .png de la carpeta del proyecto, planos (los .w3dui los referencian por nombre)
        { char cmd[1400]; snprintf(cmd, sizeof(cmd), "cp \"%s\"/*.png \"%s\"/ 2>/dev/null || true", proj.c_str(), out.c_str()); if (ShellRun(cmd, proj, out)) {} }
        // la subcarpeta assets/ entera, CON su carpeta ("assets/xxx.png")
        { char cmd[2600]; snprintf(cmd, sizeof(cmd),
            "rm -rf \"%s/assets\" && if [ -d \"%s/assets\" ]; then cp -r \"%s/assets\" \"%s/\"; fi",
            out.c_str(), proj.c_str(), proj.c_str(), out.c_str()); if (ShellRun(cmd, out, proj)) {} }
        // y las rutas REALES que las escenas recien exportadas referencian (content/...)
        { char cmd[1400]; snprintf(cmd, sizeof(cmd), "rm -rf \"%s/content\"", out.c_str()); if (ShellRun(cmd, out)) {} }
        std::set<std::string> refs;
        if (multi)
            for (size_t i = 0; i < scenes.size(); i++)
                SaveVersionCollectFrom(out + "/" + scenes[i].file, "", proj, &refs);
        else
            SaveVersionCollectFrom(out + "/" + w3duiLegacy, "", proj, &refs);
        for (std::set<std::string>::iterator it = refs.begin(); it != refs.end(); ++it) {
            if (!SafeRelPath(*it)) continue;   // (una ruta del proyecto que sale de su carpeta)
            char cmd[3000];
            snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/%s\" && cp \"%s/%s\" \"%s/%s\"",
                     out.c_str(), Folder(*it).c_str(), proj.c_str(), it->c_str(),
                     out.c_str(), it->c_str());
            if (ShellRun(cmd, out, proj, *it)) {}
        }
    }
    // ------------------------------------------------------------------
    //  JUEGO 3D: la UI VIVA gana sobre la del contenedor.
    //  El project.json referencia cada escena por su NOMBRE DE ENTRADA
    //  ("scenes/hud.w3dui"), y el volcado de arriba acaba de dejar ahi la
    //  version del ULTIMO GUARDADO. Aca se pisa con la que el editor tiene EN
    //  PANTALLA (la misma que se exporto plana a la raiz), asi el juego sale con
    //  los cambios de HUD sin guardar, igual que el Play.
    // ------------------------------------------------------------------
    if (with3D) {
        for (size_t i = 0; i < scenesUI.size(); i++) {
            UI* e = scenesUI[i];
            if (!e || e->fileW3dui.empty()) continue;
            const std::string& rel = e->fileW3dui;
            // v4: nombre de ENTRADA ("scenes/hud.w3dui"). v3/texto: ruta RELATIVA a
            // la carpeta del proyecto ("hud.w3dui") -- ese caso no se re-exportaba y
            // el .w3dui NI SIQUIERA VIAJABA: el juego salia sin HUD (el proyecto lo
            // nombra por esa ruta y al lado del binario no habia ningun archivo asi).
            // Se descarta solo lo que no se puede colocar: absoluta o fuera de la carpeta.
            if (!W3dIsEntryName(rel) &&
                (rel[0] == '/' || (rel.size() > 2 && rel[1] == ':') ||
                 rel.find("..") != std::string::npos)) continue;
            MakeDirs(Folder(out + "/" + rel));
            UI2DSave(e, out + "/" + rel, proj);
        }
        // y el PROYECTO.JSON de un .w3d v3 (json plano, sin contenedor): es el .w3d mismo
        if (!fromContainer && !w3dPath.empty()) {
            char cmd[1400];
            snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s/project.json\"", w3dPath.c_str(), out.c_str());
            if (ShellRun(cmd, w3dPath, out)) {}
        }
        // CON QUE ARRANCA (el "Empieza con" de la tarjeta Juego): el de AHORA, aunque el proyecto no se haya guardado
        if (!W3dInitialRoot().empty()) JsonSetTextRoot(out + "/project.json", "initialScene3d", W3dInitialRoot());
    }
    // ------------------------------------------------------------------
    //  REFERENCIAS EXTERNAS ("ext:"): archivos que el proyecto deja A PROPOSITO
    //  afuera del contenedor (el demo guarda ahi los .cap de los rieles de camara).
    //  El juego los referencia por su ruta RELATIVA a la carpeta del proyecto, asi
    //  que se copian al staging CON esa misma ruta y resuelven igual al lado del
    //  binario. Sin esto el juego abria con la camara sin riel: pantalla negra con
    //  el HUD encima, que es exactamente el sintoma que se reporto.
    //  Una ref de AFUERA de la carpeta del proyecto no se puede colocar sin romper
    //  su ruta: se avisa y se sigue (el juego lo dira en su log si le hace falta).
    {
        std::vector<std::string> ext;
        W3dExternalRefList(&ext);
        int copied = 0, outside = 0;
        const std::string base = proj + "/";
        for (size_t i = 0; i < ext.size(); i++) {
            if (ext[i].compare(0, base.size(), base) != 0) { outside++; continue; }
            const std::string rel = ext[i].substr(base.size());
            if (!SafeRelPath(rel)) { outside++; continue; }
            char cmd[3000];
            snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/%s\" && cp \"%s\" \"%s/%s\"",
                     out.c_str(), Folder(rel).c_str(), ext[i].c_str(), out.c_str(), rel.c_str());
            if (ShellRun(cmd, out, ext[i], rel) == 0) copied++;
        }
        if (!ext.empty())
            w3dLogf("CompileGame: %d referencia(s) externa(s) copiadas al staging (%d afuera "
                    "de la carpeta del proyecto, no viajan)", copied, outside);
        if (outside > 0)
            Notify(T("Build: there are external references outside the project folder; they do not travel with the game"), true);
    }
    // ------------------------------------------------------------------
    //  LAS LIBRERIAS EXTERNAS (io/Libraries.h) del proyecto viajan con el juego: todas las entradas de
    //  cada una (sus prefabs y escenas, sus mallas, animsets, materiales, texturas, scripts: lo que sus
    //  scripts lean en tiempo de ejecucion no se puede adivinar, igual que con el proyecto) en
    //  libs/<nombre>/<entrada>, que es donde el runtime busca sus rutas "lib:<nombre>/...". Viajan TODAS
    //  las VINCULADAS (el registro es lo que el usuario declaro que el proyecto usa), no solo las que nombra el
    //  JSON guardado: un script puede usar una sin que nada mas la nombre (instanciar("<lib>/<prefab>") con el
    //  nombre armado en lua, sin ningun proxy en el nivel), y sin sus entradas el juego no la encontraria.
    //  Ademas, de cada una:
    //    - lo que deja AFUERA de su .w3d a proposito ("ext:", relativo a SU carpeta) en
    //      libs/<nombre>/_ext/<rel> (W3dLibsExternalEntry: de ahi lo lee el lector del juego);
    //    - lo que SUS SCRIPTS nombran por ruta y vive SUELTO al lado de su .w3d (la misma regla que
    //      CopyRelativeRefsOfLua para el proyecto) en libs/<nombre>/<ruta>;
    //    - las librerias de ADENTRO (TODO su registro, por lo mismo: sus scripts pueden usar una sin nombrarla
    //      en su JSON), con el nombre con que viven en memoria, y el mapeo en libs/_nested.json
    //      (W3dLibsNested del juego lo lee de ahi).
    // ------------------------------------------------------------------
    {
        { char cmd[1400]; snprintf(cmd, sizeof(cmd), "rm -rf \"%s/libs\"", out.c_str()); if (ShellRun(cmd, out)) {} }
        // LA COLA: las vinculadas al nivel, y las de adentro de cada una (por su nombre en memoria)
        std::vector<std::string> queue;
        std::set<std::string> inQueue;
        for (int l = 0; l < W3dLibsCount(); l++) {
            const std::string& itemName = W3dLibsRow(l).name;
            if (inQueue.insert(itemName).second) queue.push_back(itemName);
        }
        std::string nested;   // el mapeo de las de adentro (libs/_nested.json)
        int nLibs = 0, nEnt = 0, nExt = 0, nLoose = 0, outside = 0;
        for (size_t q = 0; q < queue.size(); q++) {
            const std::string itemName = queue[q];
            const std::string path = W3dLibsDiskPathOf(itemName);
            W3dZipReader z;
            if (path.empty() || !z.Open(path)) {
                Notify(std::string(T("Build: could not open the library")) + " '" + itemName + "' (" + path + ")", true);
                w3dLogfE("CompileGame: no pude abrir la libreria '%s' (%s)", itemName.c_str(), path.c_str());
                continue;
            }
            const std::string dirLib = Folder(path);
            std::vector<std::string> ents;
            z.List(ents);
            const std::set<std::string> entries(ents.begin(), ents.end());
            std::vector<std::string> textsLib, luas;
            for (size_t i = 0; i < ents.size(); i++) {
                const std::string& e = ents[i];
                // (project.json SI: el runtime lee de ahi sus registros; mimetype/LEEME/EXTERNOS no)
                if (W3dIsServiceEntry(e) && e != "project.json") continue;
                if (e.empty() || e[e.size() - 1] == '/') continue;
                std::vector<unsigned char> d;
                if (!z.Read(e, d)) continue;
                const std::string dst = out + "/" + W3dLibPathPacked(itemName, e);
                MakeDirs(Folder(dst));
                FILE* fh = fopen(dst.c_str(), "wb");
                if (fh) {
                    if (!d.empty()) fwrite(&d[0], 1, d.size(), fh);
                    if (fclose(fh) == 0) nEnt++;
                }
                if (d.empty()) continue;
                const size_t pt = e.find_last_of('.');
                const std::string ext = (pt == std::string::npos) ? std::string() : e.substr(pt);
                if (e == "project.json" || ext == ".w3de" || ext == ".w3dp" || ext == ".w3dui")
                    textsLib.push_back(std::string((const char*)&d[0], d.size()));
                else if (ext == ".lua" && d[0] != 0x1B) luas.push_back(std::string((const char*)&d[0], d.size()));
            }
            // lo que deja AFUERA a proposito ("ext:", relativo a SU carpeta)
            {
                std::vector<std::string> exts;
                for (size_t t = 0; t < textsLib.size(); t++) ExtsOfText(textsLib[t], &exts);
                std::set<std::string> seen;
                for (size_t i = 0; i < exts.size(); i++) {
                    const std::string& x = exts[i];
                    if (!seen.insert(x).second) continue;
                    if (x[0] == '/' || x[0] == '\\' || (x.size() > 1 && x[1] == ':')) { outside++; continue; }
                    const std::string src = w3dFileSystem::JoinPath(dirLib, x);
                    const std::string dst = out + "/" + W3dLibPathPacked(itemName, W3dLibsExternalEntry(x));
                    if (!CopyToStaging(src, dst)) {
                        w3dLogfW("CompileGame: la libreria '%s' nombra '%s' afuera de su .w3d y no esta (%s)", itemName.c_str(),
                                 x.c_str(), src.c_str());
                        continue;
                    }
                    nExt++;
                    // (un script que vive afuera tambien nombra rutas)
                    if (x.size() > 4 && x.compare(x.size() - 4, 4, ".lua") == 0) {
                        std::vector<unsigned char> d;
                        if (w3dFileSystem::ReadFileBytes(src, d) && !d.empty() && d[0] != 0x1B)
                            luas.push_back(std::string((const char*)&d[0], d.size()));
                    }
                }
            }
            // lo que SUS SCRIPTS nombran por ruta y vive SUELTO al lado de su .w3d (adentro de su carpeta)
            {
                std::vector<std::string> paths;
                for (size_t t = 0; t < luas.size(); t++) RefsTextPaths(luas[t].data(), luas[t].size(), &paths);
                std::set<std::string> seen;
                for (size_t i = 0; i < paths.size(); i++) {
                    const std::string& r = paths[i];
                    if (!seen.insert(r).second || entries.count(r)) continue;   // (adentro de su .w3d ya viajo)
                    const std::string src = dirLib + "/" + r;
                    const std::string dst = out + "/" + W3dLibPathPacked(itemName, r);
                    if (w3dFileSystem::FileExists(src) && !w3dFileSystem::IsDir(src)) {
                        if (!w3dFileSystem::FileExists(dst) && CopyToStaging(src, dst)) nLoose++;
                        continue;
                    }
                    // (la CARPETA del literal: un script que ARMA la ruta con una variable, como en el proyecto)
                    const std::string dir = Folder(r);
                    if (dir.empty() || dir == r || dir == "platform-build" || dir == "build" || !SafeRelPath(dir)) continue;
                    if (!w3dFileSystem::IsDir(dirLib + "/" + dir)) continue;
                    char cmd[3000];
                    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" && cp -rn \"%s/%s/.\" \"%s/\" 2>/dev/null || true",
                             Folder(dst).c_str(), dirLib.c_str(), dir.c_str(), Folder(dst).c_str());
                    if (ShellRun(cmd, Folder(dst), dirLib, dir) == 0) nLoose++;
                }
            }
            // sus librerias de ADENTRO (todo su registro): con el nombre con que viven en memoria (W3dLibsNested)
            {
                std::vector<W3dLibRow> sub;
                W3dLibsSubRows(itemName, sub);
                for (size_t i = 0; i < sub.size(); i++) {
                    const std::string g = W3dLibsNested(itemName, sub[i].name);
                    nested += nested.empty() ? "\n    " : ",\n    ";
                    nested += "{ \"lib\": "; JsonText(nested, itemName);
                    nested += ", \"name\": "; JsonText(nested, sub[i].name);
                    nested += ", \"as\": "; JsonText(nested, g);
                    nested += " }";
                    if (inQueue.insert(g).second) queue.push_back(g);
                }
            }
            nLibs++;
        }
        if (!nested.empty()) {
            const std::string j = "{\n  \"nested\": [" + nested + "\n  ]\n}\n";
            const std::string dst = out + "/" + kW3dNestedLibsEntry;
            MakeDirs(Folder(dst));
            FILE* fh = fopen(dst.c_str(), "wb");
            if (fh) { fwrite(j.data(), 1, j.size(), fh); fclose(fh); }
        }
        if (nLibs)
            w3dLogf("CompileGame: %d libreria(s) empaquetadas (%d entradas, %d externas, %d sueltas en librerias/)",
                    nLibs, nEnt, nExt, nLoose);
        if (outside > 0)
            Notify(T("Build: a library uses files by absolute path; they do not travel with the game"), true);
    }
    { char cmd[1200]; snprintf(cmd, sizeof(cmd), "cp \"%s/res/Skins/Whisk3D/font.png\" \"%s/font.png\"", repo.c_str(), out.c_str()); if (ShellRun(cmd, repo, out)) {} }
    // ...y lo que SOLO nombra el lua por ruta relativa (sounds/, music/): ver
    // CopyRelativeRefsOfLua. Va DESPUES de todo lo demas (no pisa nada de lo que
    // ya viajo) y ANTES de descubrir las carpetas y de hornear el bytecode -- con
    // los .lua ya convertidos a bytecode no habria literales que leer.
    CopyRelativeRefsOfLua(out, proj);
    // el staging ya esta poblado: recien ahora se sabe QUE carpetas de assets tiene
    // (las fijas + las que trajeron las refs externas). Los cuatro targets usan esta lista.
    CarpAssetsDiscover(out);

    // PRODUCCION ("Modo debug" destildado): los .lua recien COPIADOS al staging se
    // reemplazan por bytecode stripped con el mismo nombre (ver CompileLuasStaging).
    // Corre antes del pak/embed/copia -> aplica a los 4 targets. Un error de sintaxis
    // ABORTA el build aca, con el mensaje de lua (archivo:linea) en la notificacion.
    // (la Dreamcast no: compila lua con LUA_32BITS -enteros de 32 bits y float, que el SH-4 hace por hardware- y el
    // bytecode que arma el editor, con enteros de 64 bits y double, no carga ahi. Sus .lua viajan como fuente)
    if (!debugMode && platform != PlatDreamcast) {
        std::string errLua; unsigned nLua = 0; size_t bSource = 0, bByte = 0;
        if (!CompileLuasStaging(out, &errLua, &nLua, &bSource, &bByte)) {
            Notify(std::string(T("Build: error in a Lua script: ")) + errLua, true);
            w3dLogfE("CompileGame: bytecode fallo: %s", errLua.c_str());
            return false;
        }
        w3dLogf("CompileGame: %u script(s) lua a bytecode stripped (%u -> %u bytes)",
                nLua, (unsigned)bSource, (unsigned)bByte);
    }

    // ICONO del juego (opcional, tarjeta Juego): del PNG del proyecto (g_projIcon,
    // maxima definicion) se generan ACA los tamanos chicos (ver GenerateIcons). Se
    // limpia SIEMPRE lo viejo primero: si el proyecto ya no tiene icono, no deben
    // quedar los de la compilacion anterior. Sin icono: todo sigue como antes.
    std::vector<int> iconsLinux;
    bool withIcon = false;
    { char cmd[1500]; snprintf(cmd, sizeof(cmd), "rm -rf \"%s/icons\" \"%s/icono.png\"",
                               out.c_str(), out.c_str()); if (ShellRun(cmd, out)) {} }
    if (!g_projIcon.empty()) {
        std::string abs = g_projIcon;
        // v4: el icono es una ENTRADA del contenedor ("project/icono.png") y se lee
        // TAL CUAL (w3dFileSystem la resuelve por el montaje). Colgarla de la carpeta
        // del .w3d daria una ruta de disco que no existe y el juego saldria sin icono.
        if (!W3dIsEntryName(abs) &&
            abs[0] != '/' && !(abs.size() > 2 && abs[1] == ':')) {
            // relativa al .w3d (asi se guarda); sin .w3d, relativa a la carpeta del juego
            std::string baseDir = w3dPath.empty() ? proj : Folder(w3dPath);
            abs = baseDir + "/" + abs;
        }
        withIcon = GenerateIcons(abs, out, &iconsLinux);
        if (!withIcon)
            Notify(std::string(T("Build: could not read the icon; continuing without it")) + " (" + Base(g_projIcon) + ")", true);
    }

    // la escena INICIAL (multi): la del proyecto (escenaInicial del .w3d) si es una escena valida,
    // sino la primera. El runtime la elige por NOMBRE (el campo "nombre" de su .w3dui).
    std::string initial;
    if (multi) {
        initial = W3dInitialScene();
        bool valid = false;
        for (size_t i = 0; i < scenes.size(); i++) if (scenes[i].name == initial) { valid = true; break; }
        if (!valid) initial = scenes.empty() ? std::string() : scenes[0].name;
    }

    // [2/4] generar el codigo del runtime (main.cpp + el build de la plataforma)
    w3dLogf("CompileGame [2/4]: generando el codigo del juego (assets %s)",
            packAssets ? "packed" : "loose");
    // ASSETS EMPAQUETADOS: con todos los archivos del juego ya juntados en
    // platform-build/<plat> (escenas + lua + png + assets/ + icono), se hornea el
    // pak.cpp (fase sincrona, hilo de UI). En sueltos se borra un pak.cpp viejo
    // para que no quede colgado de una compilacion empaquetada anterior.
    if (packAssets) {
        std::string errPak;
        if (!WritePak(out, name, &errPak)) {
            Notify(std::string(T("Build: could not pack the assets")) + " (" + errPak + ")", true);
            return false;
        }
    } else {
        remove((out + "/pak.cpp").c_str());
    }
    // tamano de la ventana para el modo "Sin bordes": el lienzo de la UI cuando es
    // responsive; si la UI usa el tamano del render, se mantiene el 600x600 de siempre
    int winW = 600, winH = 600;
    if (u && !u->sameAsRender) {
        winW = (int)(u->width + 0.5f); winH = (int)(u->height + 0.5f);
        if (winW < 1) winW = 1;
        if (winH < 1) winH = 1;
    }
    if (!WriteMain(out + "/main.cpp", "font.png", multi, w3duiLegacy, objs, scenes, initial,
                      windowMode, winW, winH, orientation, useSound, name, withIcon,
                      packAssets, with3D)) {
        Notify(T("Build: could not generate main.cpp"), true); return false;
    }
    // CMakeLists para PC (Linux): GENERADO (copia/instala TODAS las escenas + scripts + texturas
    // cuando van sueltos; empaquetado compila pak.cpp y no copia/instala nada del juego)
    if (!WithMakefile(platform) &&
        !WriteCMake(out + "/CMakeLists.txt", name, usePhysics, useSound, debugMode,
                       iconsLinux, packAssets, with3D, W3dCompileBackendOf(platform))) {
        Notify(T("Build: could not generate CMakeLists.txt"), true); return false;
    }

    // [3/4] compilar para la plataforma elegida — ASINCRONICO: de aca en adelante
    // NADA toca la escena. El hilo de UI solo ARMA los comandos (strings propios)
    // y lanza el worker, que los corre + copia los resultados publicando etapa y
    // porcentaje para la barra (overlay) que dibuja el hilo principal cada frame.
    const char* namePlat = WithMakefile(platform) ? ConsoleName(platform)
                        : (platform == 6) ? "Android 2.3" : (platform == 3) ? "Android"
                        : (platform == 2) ? "WebGL" : (platform == 1) ? "Linux AppImage" : "Linux .deb";
    w3dLogf("CompileGame [3/4]: compilando (%s) en segundo plano", namePlat);
    BldJob t;
    t.platform = platform;
    t.plat = plat;
    t.out = out;
    if (platform == 3) {
        // ANDROID: genera el script que arma el APK reusando la plantilla del ejemplo
        // (ndk-build + gradlew), adaptada a ESTE juego. Ver WriteBuildAndroid arriba.
        // El script emite marcas "[W3D] <pct> <etapa>" que el worker parsea para la barra.
        std::string sh = out + "/build_android.sh";
        if (!WriteBuildAndroid(sh, repo, out, name, orientation, usePhysics, useSound,
                                  debugMode, withIcon, packAssets, with3D, W3dCompileBackendOf(platform))) {
            Notify(T("Build: could not generate the Android script"), true); return false;
        }
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "bash \"%s\"", sh.c_str());
        t.cmdBuild = cmd;
    } else if (platform == 6) {
        // ANDROID 2.3: el script arma el APK con la cadena de platform/android-viejo (ver WriteBuildAndroidOld)
        // y, con un telefono conectado, lo instala y lo abre
        std::string sh = out + "/build_android23.sh";
        if (!WriteBuildAndroidOld(sh, repo, out, name, orientation, usePhysics, useSound,
                                       debugMode, withIcon, packAssets, with3D)) {
            Notify(T("Build: could not generate the Android script"), true); return false;
        }
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "bash \"%s\"", sh.c_str());
        t.cmdBuild = cmd;
    } else if (WithMakefile(platform)) {
        // LAS CONSOLAS (y Windows): el Makefile (las mismas listas de fuentes, con el backend y el paquete de cada una) y el script
        // que arma el entorno del kit y lo corre (con las marcas de la barra). Ver WriteMakefileConsole
        // (la 3DS: el icono de 48x48 del menu. Sin icono en el proyecto -o uno tan chico que no dio ese tamano-, el de
        // Whisk3D)
        if (platform == Plat3DS && !w3dFileSystem::FileExists(out + "/icons/48x48.png") &&
            !GenerateIcon(repo + "/res/Whisk3D.png", out + "/icons/48x48.png", 48)) {
            Notify(std::string(T("Build: could not read the icon; continuing without it")) + " (Whisk3D.png)", true);
            return false;
        }
        // (Windows: el icono del .exe, de los tamanos que genero GenerateIcons)
        const bool withIco = (platform == PlatWindows) && withIcon && WriteIco(out, out + "/icono.ico");
        if (platform == PlatWindows && !withIco) remove((out + "/icono.ico").c_str());
        if (!WriteMakefileConsole(out + "/Makefile", platform, repo, name, usePhysics, useSound, debugMode,
                                     packAssets, with3D, W3dCompileBackendOf(platform), withIco)) {
            Notify(T("Build: could not generate the Makefile"), true); return false;
        }
        std::string sh = out + "/build_" + DirConsole(platform) + ".sh";
        if (!WriteBuildConsole(sh, platform, out)) {
            Notify(T("Build: could not generate the Makefile"), true); return false;
        }
        t.cmdBuild = "bash \"" + sh + "\"";
    } else if (platform == 2) {
        // WebGL: MISMO set de fuentes que el build_web.sh del ejemplo (incluye W3dScene.cpp para
        // multi-escena y Texture2D.cpp para las imagenes) y EMBEBE por glob TODAS las escenas
        // (.w3dui) + scripts (.lua) + texturas/fuente (.png) que quedaron en platform-build/web,
        // mas la subcarpeta assets/ entera (si existe) con su carpeta (rutas "assets/xxx.png").
        // La salida cruda de emcc va a out/ (despues se copia lo servible a build/web).
        // Los checkboxes de la tarjeta arman las piezas OPCIONALES de la linea emcc:
        // sin fisica no viaja W3dPhysics.cpp (+W3D_NO_PHYSICS); sin sonido no viaja el
        // backend SDL ni W3D_ENABLE_AUDIO (W3dAudio.cpp queda: es el dispatcher stub).
        // "Modo debug" define W3D_DEV_LOG=1/0 (1 = log + ring + depurar(); 0 = produccion).
        // LAS MISMAS LISTAS que el CMake de PC y el Android.mk (kSourcesBase/kSources3D/
        // kIncludes*): una sola fuente de verdad para las tres plataformas.
        std::string baseSrcW, threeSrcW, incW;
        for (int i = 0; kSourcesBase[i]; i++) baseSrcW += AEmcc(kSourcesBase[i]) + " ";
        if (with3D) for (int i = 0; kSources3D[i]; i++) threeSrcW += AEmcc(kSources3D[i]) + " ";
        for (int i = 0; kIncludesBase[i]; i++) incW += "-I" + AEmcc(kIncludesBase[i]) + " ";
        if (with3D) for (int i = 0; kIncludes3D[i]; i++) incW += "-I" + AEmcc(kIncludes3D[i]) + " ";
        const char* physSrc = usePhysics ? "$CORE/physics/W3dPhysics.cpp " : "";
        const char* audSrc  = useSound ? "$CORE/audio/W3dAudio.cpp $CORE/audio/W3dAudioSDL.cpp "
                                        : "$CORE/audio/W3dAudio.cpp ";
        const char* defsOpt = useSound ? (usePhysics ? "-DW3D_ENABLE_AUDIO"
                                                       : "-DW3D_ENABLE_AUDIO -DW3D_NO_PHYSICS")
                                         : (usePhysics ? "" : "-DW3D_NO_PHYSICS");
        // assets EMPAQUETADOS: viaja pak.cpp (el pak entra al wasm por el array) y NO
        // se embebe ningun archivo; sueltos: --embed-file por glob, como siempre.
        const char* pakSrc = packAssets ? "pak.cpp " : "";
        // los --embed-file: los generados de la raiz (.w3dui/.lua/.png) mas TODAS las
        // carpetas de assets del staging (kFoldersAssets: el espejo del contenedor +
        // las dos legacy). La lista se arma aca porque es la MISMA de los otros tres
        // targets: si una carpeta nueva aparece, aparece en los cuatro a la vez.
        std::string embedsStr;
        if (!packAssets) {
            // el "[ -f ]" NO es decorativo: si un glob no matchea nada, sh lo deja
            // LITERAL y emcc recibia "--embed-file *.lua@*.lua" y cortaba el build.
            // Con el contenedor eso pasa siempre, porque los .lua viven en scripts/
            // y en la raiz del staging no queda ninguno.
            embedsStr = "$(for a in *.w3dui *.lua *.png *.json; do [ -f \"$a\" ] || continue; "
                        "echo --embed-file $a@$a; done) ";
            for (int c = 0; CarpAsset(c); c++) {
                embedsStr += "$(if [ -d ";
                embedsStr += CarpAsset(c);
                embedsStr += " ]; then echo --embed-file ";
                embedsStr += CarpAsset(c);
                embedsStr += "@";
                embedsStr += CarpAsset(c);
                embedsStr += "; fi) ";
            }
        }
        const char* embeds = embedsStr.c_str();
        char cmd[6000];
        snprintf(cmd, sizeof(cmd),
            // emsdk: con '.' y DESDE su carpeta. popen corre /bin/sh (dash en Debian/Ubuntu), que
            // no conoce 'source' y donde emsdk_env.sh no sabe donde esta: sin esto, un editor
            // lanzado sin emsdk activo daba "emcc: not found"
            "if [ -f ~/emsdk/emsdk_env.sh ]; then cd ~/emsdk && . ./emsdk_env.sh >/dev/null 2>&1; fi; "
            "cd \"%s\" && "
            // (las listas de fuentes/includes son LAS MISMAS de PC y Android: ver kSourcesBase)
            // emcc reparte las ~75 unidades entre EMCC_CORES procesos y por default agarra
            // TODOS los nucleos: misma cota que el resto (ver W3dCompileJobs)
            "export EMCC_CORES=%d; "
            "W3D=\"%s\"; GAME=\"$W3D/../Whisk3D-Examples/game\"; "
            "CORE=$W3D/libs/Whisk3DCore; UILIB=$W3D/libs/WhiskUI; LUA=$W3D/thirdparty/lua/src; "
            "LUASRC=$(ls $LUA/*.c | grep -vE '/lua\\.c|/luac\\.c'); "
            "emcc main.cpp %s$GAME/w3drun.cpp "
            "$CORE/graphics/gles2/w3dGraphicsGLES2.cpp "   // backend de shaders (WebGL)
            "%s"   // kSourcesBase: LA MISMA lista que el CMake de PC y el Android.mk
            "%s"   // fisica del Core (W3dPhysics.cpp) solo con el checkbox tildado
            "%s"   // audio: W3dAudio.cpp siempre (stub sin flag); el backend SDL es opcional
            "%s"   // kSources3D: el lector de proyecto + el pase 3D (solo si el juego es 3D)
            "$LUASRC "
            // OJO: SIN -DLUA_USE_C89. Con C89 lua_Integer era 'long' (4 bytes en wasm32)
            // y el BYTECODE de produccion (compilado por el editor x86_64: lua_Integer de
            // 8) no cargaba en web ("bad binary format"). Emscripten es clang C11: con la
            // config default lua_Integer es long long (8) = desktop/Android/editor, y de
            // paso los enteros de lua se portan IGUAL en las 4 plataformas.
            "-DW3D_WEBGL -DW3D_GLES2 -DW3D_GAME_RUNTIME -DW3D_GAME_NO_TEX -DW3D_STB_IMPL "
            "-DW3D_DEV_LOG=%d %s %s "
            "-I$GAME %s-I$LUA -sUSE_SDL=2 -O2 "   // includes: kIncludesBase (+ kIncludes3D)
            "%s"   // --embed-file de escenas/scripts/texturas/assets (vacio si van empaquetados)
            "--shell-file $W3D/../Whisk3D-Examples/core/shell.html "
            "-o out/%s.html",
            out.c_str(), W3dCompileJobs(), repo.c_str(), pakSrc,
            baseSrcW.c_str(), physSrc, audSrc, threeSrcW.c_str(),
            debugMode ? 1 : 0, defsOpt,
            with3D ? "-DW3D_NO_EDITOR -DW3D_GAME_3D" : "",
            incW.c_str(), embeds, name.c_str());
        // (ya no redirige a build.log el mismo: el worker captura stdout+stderr y
        // escribe el MISMO build.log de siempre, leyendolo en vivo para la barra)
        MakeDirs(out + "/out");
        t.cmdBuild = cmd;
    } else {
        // PC (Linux): cmake (configurar) + cmake --build; luego cpack -G DEB (plataforma 0).
        // Van como comandos SEPARADOS: el worker parsea el "[ NN%]" del --build para la barra.
        char cfg[1200], bld[900];
        // un configure cortado a la mitad deja el cache con la lista de includes implicitos de C++ VACIA: cmake
        // ya no filtra /usr/include, lo pasa como -isystem y el #include_next <stdlib.h> de <cstdlib> no
        // encuentra nada (todas las compilaciones fallan para siempre). Ese cache se tira y se configura de nuevo.
        // Lo mismo si el cache es de OTRA carpeta (el proyecto se movio o se copio con su platform-build/): cmake se
        // niega a usarlo ("does not match the source") y la compilacion fallaba siempre
        snprintf(cfg, sizeof(cfg), "cd \"%s\" && { grep -qs 'CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES \"\"' "
                 "out/CMakeFiles/*/CMakeCXXCompiler.cmake && rm -rf out/CMakeCache.txt out/CMakeFiles; true; } && "
                 "{ [ ! -f out/CMakeCache.txt ] || grep -qsxF \"CMAKE_HOME_DIRECTORY:INTERNAL=$(pwd -P)\" out/CMakeCache.txt "
                 "|| rm -rf out/CMakeCache.txt out/CMakeFiles; } && "
                 "cmake -S . -B out -DW3DROOT=\"%s\"",
                 out.c_str(), repo.c_str());
        // -j ACOTADO (nunca pelado): ver W3dCompileJobs en CompileGame.h
        snprintf(bld, sizeof(bld), "cd \"%s\" && cmake --build out %s",
                 out.c_str(), W3dCompileJobsFlag().c_str());
        t.cmdCfg = cfg;
        t.cmdBuild = bld;
        if (platform == 0) {   // .deb: empaquetar con cpack (si falla no es fatal, como siempre)
            char deb[900];
            snprintf(deb, sizeof(deb), "cd \"%s/out\" && cpack -G DEB", out.c_str());
            t.cmdCpack = deb;
        } else {                 // AppImage: el script de WriteAppImage (si falla, falla la compilacion)
            // el icono: el mas grande que se genero del proyecto, o el de Whisk3D (el AppImage lo pide)
            std::string icon;
            static const int sizes[6] = { 256, 128, 64, 48, 32, 16 };
            for (int i = 0; i < 6 && icon.empty(); i++) {
                char r[1200]; snprintf(r, sizeof(r), "%s/icons/%dx%d.png", out.c_str(), sizes[i], sizes[i]);
                if (w3dFileSystem::FileExists(r)) icon = r;
            }
            if (icon.empty() && GenerateIcon(repo + "/res/Whisk3D.png", out + "/icons/256x256.png", 256))
                icon = out + "/icons/256x256.png";
            const std::string sh = out + "/armar_appimage.sh";
            if (icon.empty() ||
                !WriteAppImage(sh, out, name, packAssets, W3dCompileBackendOf(platform) == 2, icon)) {
                Notify(T("Build: could not generate the AppImage script"), true); return false;
            }
            t.cmdCpack = "bash \"" + sh + "\"";
        }
    }

    // [4/4] la copia de RESULTADOS a build/<plat> (se limpia antes: quedan SOLO los
    // resultados de la ultima compilacion). El comando se ARMA aca pero lo corre el
    // worker al final (es solo filesystem). Lo intermedio queda en platform-build/<plat>.
    {
        // holgado a proposito: la copia de Linux lleva UN cp por carpeta de assets
        // (kFoldersAssets) y cada uno nombra dos rutas absolutas
        char cmd[9000];
        if (platform == PlatWindows) {
            // WINDOWS: la carpeta <juego>/ (el .exe con sus assets al lado) y el mismo .zip
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp -r \"%s/out/%s\" \"%s/\" && "
                "(cp \"%s/out/%s-windows.zip\" \"%s/\" 2>/dev/null || true)",
                results.c_str(), results.c_str(), out.c_str(), name.c_str(), results.c_str(),
                out.c_str(), name.c_str(), results.c_str());
        } else if (platform == PlatPSP) {
            // PSP: la carpeta PSP/GAME/<juego> (va a la raiz del Memory Stick) y el mismo .zip
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp -r \"%s/out/PSP\" \"%s/\" && "
                "(cp \"%s/out/%s-psp.zip\" \"%s/\" 2>/dev/null || true)",
                results.c_str(), results.c_str(), out.c_str(), results.c_str(),
                out.c_str(), name.c_str(), results.c_str());
        } else if (platform == PlatVita) {
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s.vpk\" \"%s/\"",
                     results.c_str(), results.c_str(), out.c_str(), name.c_str(), results.c_str());
        } else if (platform == Plat3DS) {
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s.3dsx\" \"%s/\"",
                     results.c_str(), results.c_str(), out.c_str(), name.c_str(), results.c_str());
        } else if (platform == PlatDreamcast) {
            // el .elf (para dc-tool o un emulador) y el disco .cdi si se pudo armar
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s.elf\" \"%s/\" && "
                "(cp \"%s/out/%s.cdi\" \"%s/\" 2>/dev/null || true)",
                results.c_str(), results.c_str(), out.c_str(), name.c_str(), results.c_str(),
                out.c_str(), name.c_str(), results.c_str());
        } else if (platform == 6) {
            // ANDROID 2.3: el APK firmado que dejo el script en platform-build/android23
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/%s-android23.apk\" \"%s/\"",
                results.c_str(), results.c_str(), out.c_str(), name.c_str(), results.c_str());
        } else if (platform == 3) {
            // ANDROID: el .apk (assembleDebug) como <juego>-android.apk (fat: sin
            // arch unica en el nombre), coherente con <juego>-linux-<arch>.deb.
            // El nombre y la ruta ESTANDAR los garantiza WriteBuildAndroid (sed
            // sobre el gradle de la COPIA: sin buildDir redirigido ni rename propio).
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && "
                "cp \"%s/androidprj/app/build/outputs/apk/debug/%s-android.apk\" \"%s/%s-android.apk\"",
                results.c_str(), results.c_str(), out.c_str(), name.c_str(),
                results.c_str(), name.c_str());
        } else if (platform == 2) {
            // WEB: el .html + .js + .wasm de emcc, listos para servir
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s\".* \"%s/\"",
                results.c_str(), results.c_str(), out.c_str(), name.c_str(),
                results.c_str());
        } else if (platform == 1) {
            // LINUX APPIMAGE: el .AppImage (lleva adentro el ejecutable, sus bibliotecas y sus assets)
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s-linux-x86_64.AppImage\" \"%s/\"",
                     results.c_str(), results.c_str(), out.c_str(), name.c_str(), results.c_str());
        } else if (packAssets) {
            // LINUX EMPAQUETADO: solo el ejecutable (los assets viajan adentro) + los
            // instaladores que haya. Nada de archivos sueltos del juego al lado.
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s\" \"%s/\" && "
                "(cp \"%s/out\"/*.deb \"%s/out\"/*.AppImage \"%s/\" 2>/dev/null || true)",
                results.c_str(), results.c_str(), out.c_str(), name.c_str(),
                results.c_str(),
                out.c_str(), out.c_str(), results.c_str());
        } else {
            // LINUX: el ejecutable + los assets al lado (para correrlo directo) + los
            // instaladores que haya (.deb del cpack; .AppImage si algun dia se genera).
            // Las CARPETAS de assets (kFoldersAssets: el espejo del contenedor +
            // assets/ y content/) van ENTERAS con su carpeta: el juego las
            // referencia asi ("textures/pausa.png") y las resuelve contra su
            // directorio actual. Antes se copiaban solo assets/ y content/, o sea
            // que el juego de un proyecto v4 quedaba en build/ sin sus texturas.
            std::string copyDirs;
            for (int c = 0; CarpAsset(c); c++) {
                copyDirs += "(cp -r \"" + out + "/" + CarpAsset(c) + "\" \"" +
                             results + "/\" 2>/dev/null || true) && ";
            }
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s\" \"%s/\" && "
                "(cp \"%s\"/*.w3dui \"%s\"/*.lua \"%s\"/*.png \"%s/\" 2>/dev/null || true) && "
                "(cp \"%s\"/*.json \"%s/\" 2>/dev/null || true) && "
                "%s"
                "(cp \"%s/out\"/*.deb \"%s/out\"/*.AppImage \"%s/\" 2>/dev/null || true)",
                results.c_str(), results.c_str(), out.c_str(), name.c_str(),
                results.c_str(),
                out.c_str(), out.c_str(), out.c_str(), results.c_str(),
                out.c_str(), results.c_str(),
                copyDirs.c_str(),
                out.c_str(), out.c_str(), results.c_str());
        }
        t.cmdCopy = cmd;
    }
    std::string where = (platform == PlatWindows) ? (name + " -> build/windows/" + name + "/" + name + ".exe")
                      : (platform == PlatPSP) ? (name + " -> build/psp/PSP/GAME/" + name + "/EBOOT.PBP")
                      : (platform == PlatVita) ? (name + " -> build/vita/" + name + ".vpk")
                      : (platform == Plat3DS) ? (name + " -> build/3ds/" + name + ".3dsx")
                      : (platform == PlatDreamcast) ? (name + " -> build/dreamcast/")
                      : (platform == 6) ? (name + " -> build/android23/" + name + "-android23.apk")
                      : (platform == 3) ? (name + " -> build/android/" + name + "-android.apk")
                      : (platform == 2) ? (name + " -> build/web/" + name + ".html")
                      : (platform == 1) ? (name + " -> build/linux/" + name + "-linux-x86_64.AppImage")
                                          : (name + " -> build/linux/");
    t.msgOk = BldFmt(T("Game built: %s"), where);
    BldLaunch(t, namePlat);
    return true;   // true = el build ARRANCO (el resultado llega por notificacion)
}
