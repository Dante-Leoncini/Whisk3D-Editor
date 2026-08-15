// ============================================================================
//  CompilarJuego.cpp — ver CompilarJuego.h. Genera el proyecto del juego
//  (runtime standalone) y lo compila para la plataforma elegida.
// ============================================================================
#include "io/CompilarJuego.h"
#include "io/UI2DFormato.h"
#include "io/GuardarW3D.h"            // g_proyIcono: el icono del juego (ruta externa)
#include "io/GuardarVersion.h"        // GuardarVersionColectarDe: rutas reales que referencian las escenas
#include "io/W3dContenedor.h"      // FORMATO v4: el staging es un espejo del contenedor
#include "io/W3dZip.h"             // W3dZipLector: volcar las entradas al staging
#include "io/LuaCompilar.h"           // produccion: .lua del staging -> bytecode stripped
#include "objects/Objects.h"
#include "objects/UI.h"
#include "objects/Gamepad.h"
#include "script/W3dScript.h"
#include "W3dEscena.h"                // escena inicial (multi-escena) al compilar
#include "ViewPorts/Notificaciones.h"
#include "render/OpcionesRender.h"    // g_redraw: mantener vivo el render mientras compila
#include "W3dDock.h"                  // apagar la barra del icono del dock al terminar el build
#include "variables.h"                // cfg.repoPath: raiz del repo fijada a mano (editor instalado)
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
#else
#define W3D_POPEN  popen
#define W3D_PCLOSE pclose
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#endif

// ============================================================================
//  BUILD ASINCRONICO — reparto estricto de hilos:
//   - hilo PRINCIPAL (UI): pasos [1/4] y [2/4] (recolectar scripts, exportar los
//     .w3dui, escribir main.cpp/CMake/scripts, .gitignore): es lo UNICO que toca
//     la escena. Despues cada frame lee este estado (CompilarJuegoProgreso) y
//     dibuja la barra; al terminar hace join + Notificar (CompilarJuegoTick).
//   - hilo WORKER: pasos [3/4] y [4/4] (system/popen de cmake/emcc/ndk/gradle y
//     la copia final a build/<plat>): SOLO procesos externos y filesystem. Nunca
//     llama a la UI ni toca la escena: publica etapa/porcentaje aca.
//  Proteccion minima: atomics para flags/porcentaje, un mutex para los strings.
// ============================================================================
// cuantas unidades de compilacion en paralelo (ver el bloque de CompilarJuego.h).
// hardware_concurrency es de <thread> (C++11) y devuelve 0 si no puede averiguarlo.
int W3dCompilarJobs() {
    int n = 0;
#ifndef __EMSCRIPTEN__
    n = (int)std::thread::hardware_concurrency();
#endif
    if (n <= 0) n = 2;          // no se pudo averiguar: lo conservador
    int j = n / 2;              // la MITAD de los nucleos
    if (j < 1) j = 1;
    if (j > 8) j = 8;           // tope: mas no acelera y si multiplica la RAM
    // ESCAPE HATCH para maquinas con poca RAM (y para la suite de tests, que corre
    // builds de juego al lado de otras cosas): W3D_COMPILAR_JOBS BAJA el paralelismo.
    // Solo puede bajarlo: nadie puede usar esta variable para volver al -j pelado.
    const char* env = getenv("W3D_COMPILAR_JOBS");
    if (env && *env) {
        int e = atoi(env);
        if (e >= 1 && e < j) j = e;
    }
    return j;
}
std::string W3dCompilarJobsFlag() {
    char b[16]; snprintf(b, sizeof(b), "-j%d", W3dCompilarJobs());
    return std::string(b);
}

static std::atomic<bool> g_bldActivo(false);   // hay un build corriendo (bloquea re-disparo)
static std::atomic<bool> g_bldTermino(false);  // el worker termino: el Tick hace join + notifica
static std::atomic<bool> g_bldOk(false);       // resultado final del worker
static std::atomic<int>  g_bldPct(0);          // avance GLOBAL 0..100 (solo lo escribe el worker)
static std::string       g_bldEtapa;           // texto de la etapa actual (protegido por el mutex)
static std::string       g_bldFinal;           // mensaje final para Notificar (idem)
#ifndef __EMSCRIPTEN__
static std::mutex        g_bldMx;
static std::thread*      g_bldHilo = NULL;     // puntero: si el editor se cierra con un build
                                               // corriendo NO corre el dtor de un thread joinable
                                               // (std::terminate); el proceso se va y listo.
#define BLD_LOCK() std::lock_guard<std::mutex> _bl(g_bldMx)
#else
#define BLD_LOCK() ((void)0)                   // web: sin hilos, no hay carrera posible
#endif

bool CompilarJuegoEnCurso() { return g_bldActivo.load(); }

int CompilarJuegoProgreso(std::string* etapa) {
    if (etapa) { BLD_LOCK(); *etapa = g_bldEtapa; }
    return g_bldPct.load();
}

// publica etapa + porcentaje (la llama el worker). El % nunca retrocede.
static void BldPublicar(const std::string& etapa, int pct) {
    { BLD_LOCK(); g_bldEtapa = etapa; }
    int cur = g_bldPct.load();
    if (pct > cur) g_bldPct.store(pct);
}

// modos de progreso de BldCorrer (que hacer con cada linea de salida del proceso)
enum {
    BLD_CREEP  = 0,  // sin % real (emcc/cpack): la barra avanza DE A POCO por linea/tiempo,
                     // capada al tope de la franja (progreso "indeterminado" honesto)
    BLD_CMAKE  = 1,  // parsea el "[ NN%]" que escribe cmake --build y lo mapea a la franja
    BLD_MARCAS = 2   // parsea "[W3D] <pct> <texto>" (las emite el script Android generado)
};

// procesa UNA linea de salida: la vuelca al build.log y actualiza etapa/% segun el modo.
// Corre SOLO en el worker ('nCreep' es el contador local de BldCorrer).
static void BldLinea(const std::string& s, FILE* log, int modo, int pctIni, int pctFin,
                     int cap, int* nCreep) {
    if (log) { fputs(s.c_str(), log); fputc('\n', log); fflush(log); }
    if (modo == BLD_CMAKE) {
        // "[ 45%] Building CXX ..." -> mapear NN a la franja pctIni..pctFin
        size_t a = s.find('[');
        size_t b = (a == std::string::npos) ? std::string::npos : s.find("%]", a);
        if (a != std::string::npos && b != std::string::npos && b > a + 1) {
            int nn = atoi(s.substr(a + 1, b - a - 1).c_str());
            if (nn >= 0 && nn <= 100) {
                int pct = pctIni + (int)((pctFin - pctIni) * (nn / 100.0f));
                int cur = g_bldPct.load();
                if (pct > cur) g_bldPct.store(pct);
            }
        }
    } else if (modo == BLD_MARCAS && s.compare(0, 6, "[W3D] ") == 0) {
        // marca del script generado: "[W3D] <pct> <texto de la etapa>" (ej "ndk-build (arm64-v8a)")
        char* fin = NULL;
        long pct = strtol(s.c_str() + 6, &fin, 10);
        if (fin && *fin == ' ' && pct >= 0 && pct <= 100) BldPublicar(fin + 1, (int)pct);
    } else {
        // sin % real: cada linea (BLD_CREEP) o cada 5 (BLD_MARCAS: ndk/gradle imprimen muchas)
        // empuja la barra 1 punto hasta el tope de la franja
        int paso = (modo == BLD_MARCAS) ? 5 : 1;
        if (++(*nCreep) >= paso) {
            *nCreep = 0;
            int cur = g_bldPct.load();
            if (cur < cap) g_bldPct.store(cur + 1);
        }
    }
}

// corre 'cmd' EN EL WORKER via popen (stdout+stderr juntos), vuelca la salida al
// build.log en vivo y actualiza el estado compartido segun 'modo' dentro de la franja
// pctIni..pctFin. En POSIX lee con select() + timeout: si el proceso esta callado
// (emcc/gradle no imprimen %) la barra igual avanza lento (~1 punto/seg, capada).
// Devuelve el codigo de salida crudo (0 = ok) o -1 si no se pudo lanzar.
static int BldCorrer(const std::string& cmd, FILE* log, int modo, int pctIni, int pctFin) {
    std::string sh = "(" + cmd + ") 2>&1";
    FILE* p = W3D_POPEN(sh.c_str(), "r");
    if (!p) return -1;
    int cur0 = g_bldPct.load();
    if (pctIni > cur0) g_bldPct.store(pctIni);           // arranque de la franja
    int cap = (pctFin > pctIni) ? pctFin - 1 : pctIni;   // tope del creep (el final lo da el exito)
    int nCreep = 0;
    std::string linea;
#ifndef _WIN32
    int fd = fileno(p);
    char buf[1024];
    int quietos = 0;   // timeouts seguidos sin salida (creep temporal)
    bool eof = false;
    while (!eof) {
        fd_set rf; FD_ZERO(&rf); FD_SET(fd, &rf);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;   // 0.5 s
        int r = select(fd + 1, &rf, NULL, NULL, &tv);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (r == 0) {
            // proceso callado: avance lento e "indeterminado" (~1 punto/seg), capado.
            // En BLD_CMAKE no: ahi el % es real y no se inventa avance.
            if (modo != BLD_CMAKE && ++quietos >= 2) {
                quietos = 0;
                int cur = g_bldPct.load();
                if (cur < cap) g_bldPct.store(cur + 1);
            }
            continue;
        }
        quietos = 0;
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) { eof = true; }
        else for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') { BldLinea(linea, log, modo, pctIni, pctFin, cap, &nCreep); linea.clear(); }
            else if (buf[i] != '\r') linea += buf[i];
        }
    }
#else
    // Windows: sin select() sobre pipes -> lectura bloqueante linea a linea (el creep va
    // por lineas; si el proceso calla la barra queda quieta, que es lo honesto sin datos)
    char lbuf[1024];
    while (fgets(lbuf, sizeof(lbuf), p)) {
        std::string s(lbuf);
        while (!s.empty() && (s[s.size()-1] == '\n' || s[s.size()-1] == '\r')) s.erase(s.size()-1);
        BldLinea(s, log, modo, pctIni, pctFin, cap, &nCreep);
    }
#endif
    if (!linea.empty()) BldLinea(linea, log, modo, pctIni, pctFin, cap, &nCreep);
    return W3D_PCLOSE(p);
}

// ---- lo que el proyecto referencia: los objetos con script + sus refs -------
struct ObjScript { std::string nombre; std::vector<W3dScriptEntrada> scripts; };

// ---- una ESCENA a compilar: su archivo .w3dui (relativo a platform-build/<plat>) + su nombre ----
struct EscenaGen { std::string archivo; std::string nombre; };

static void RecolectarScripts(Object* o, std::vector<ObjScript>* out,
                              std::vector<std::string>* rutasLua) {
    if (!o) return;
    if (o->scriptDatos && !o->scriptDatos->scripts.empty()) {
        ObjScript os; os.nombre = o->name;
        for (size_t i = 0; i < o->scriptDatos->scripts.size(); i++) {
            os.scripts.push_back(o->scriptDatos->scripts[i]);
            if (!o->scriptDatos->scripts[i].ruta.empty())
                rutasLua->push_back(o->scriptDatos->scripts[i].ruta);
        }
        out->push_back(os);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++)
        RecolectarScripts(o->Childrens[i], out, rutasLua);
}

static std::string Base(const std::string& ruta) {
    size_t s = ruta.find_last_of("/\\");
    return (s == std::string::npos) ? ruta : ruta.substr(s + 1);
}
static std::string Carpeta(const std::string& ruta) {
    size_t s = ruta.find_last_of("/\\");
    return (s == std::string::npos) ? std::string(".") : ruta.substr(0, s);
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
    if (r.empty()) r = "escena";
    return r;
}

static std::string BaseSinExt(const std::string& ruta) {
    std::string b = Base(ruta);
    size_t p = b.find_last_of('.');
    return (p == std::string::npos) ? b : b.substr(0, p);
}

// ============================================================================
//  ICONO del juego: del PNG elegido en el editor (g_proyIcono, maxima definicion)
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
static void IcChunk(std::vector<unsigned char>& out, const char* tipo,
                    const unsigned char* d, size_t n) {
    unsigned char len4[4] = { (unsigned char)(n >> 24), (unsigned char)(n >> 16),
                              (unsigned char)(n >> 8),  (unsigned char)n };
    out.insert(out.end(), len4, len4 + 4);
    size_t ini = out.size();
    out.insert(out.end(), (const unsigned char*)tipo, (const unsigned char*)tipo + 4);
    if (n) out.insert(out.end(), d, d + n);
    unsigned int crc = IcCrc(0xFFFFFFFFu, &out[ini], out.size() - ini) ^ 0xFFFFFFFFu;
    unsigned char c4[4] = { (unsigned char)(crc >> 24), (unsigned char)(crc >> 16),
                            (unsigned char)(crc >> 8),  (unsigned char)crc };
    out.insert(out.end(), c4, c4 + 4);
}
// escribe pixeles RGBA8 como PNG RGBA (CON alpha) en 'ruta'. false si no pudo.
static bool IcGuardarPNG(const std::string& ruta, const unsigned char* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return false;
    const size_t fila = (size_t)w * 4;
    std::vector<unsigned char> raw((size_t)h * (1 + fila));
    for (int y = 0; y < h; y++) {
        unsigned char* d = &raw[(size_t)y * (1 + fila)];
        d[0] = 0;   // filtro None
        memcpy(d + 1, rgba + (size_t)y * fila, fila);
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
    static const unsigned char firma[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    png.insert(png.end(), firma, firma + 8);
    IcChunk(png, "IHDR", ihdr, 13);
    IcChunk(png, "IDAT", &idat[0], idat.size());
    IcChunk(png, "IEND", 0, 0);
    FILE* f = fopen(ruta.c_str(), "wb");
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
static void IcEscalar(const unsigned char* src, int sw, int sh, unsigned char* dst, int n) {
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
            unsigned long long r = 0, g = 0, b = 0, al = 0;
            long long cnt = (long long)(y1 - y0) * (x1 - x0);
            for (int yy = y0; yy < y1; yy++) {
                const unsigned char* p = src + ((size_t)yy * sw + x0) * 4;
                for (int xx = x0; xx < x1; xx++, p += 4) {
                    unsigned int pa = p[3];
                    r += (unsigned long long)p[0] * pa;
                    g += (unsigned long long)p[1] * pa;
                    b += (unsigned long long)p[2] * pa;
                    al += pa;
                }
            }
            unsigned char* d = dst + ((size_t)(y + offy) * n + (x + offx)) * 4;
            if (al) { d[0] = (unsigned char)(r / al); d[1] = (unsigned char)(g / al);
                      d[2] = (unsigned char)(b / al); }
            d[3] = (unsigned char)(al / cnt);
        }
    }
}
// genera todos los tamanos en <out>/icons/ + <out>/icono.png. 'tamLinux' recibe los
// tamanos hicolor que SI se generaron (no se agranda el original, salvo que sea mas
// chico que 16). false si el PNG no se pudo leer (se compila sin icono, como antes).
static bool GenerarIconos(const std::string& rutaPng, const std::string& out,
                          std::vector<int>* tamLinux) {
    unsigned char* src = 0; int sw = 0, sh = 0;
    if (!w3dEngine::DecodeImage(rutaPng.c_str(), &src, &sw, &sh) || !src || sw < 1 || sh < 1)
        return false;
    { char cmd[700]; snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/icons\"", out.c_str());
      if (system(cmd)) {} }
    std::vector<unsigned char> buf;
    // hicolor (linux): solo hasta el tamano del original (no agrandar); si el
    // original es mas chico que 16 se genera igual el de 16 (que haya al menos uno)
    static const int tam[6] = { 16, 32, 48, 64, 128, 256 };
    int mayor = 0;
    for (int i = 0; i < 6; i++) {
        int n = tam[i];
        if (n > sw && n > sh && mayor) continue;
        buf.assign((size_t)n * n * 4, 0);
        IcEscalar(src, sw, sh, &buf[0], n);
        char ruta[1200];
        snprintf(ruta, sizeof(ruta), "%s/icons/%dx%d.png", out.c_str(), n, n);
        if (IcGuardarPNG(ruta, &buf[0], n, n)) { tamLinux->push_back(n); mayor = n; }
    }
    // mipmaps (android): los 5 dpi siempre (el launcher los espera todos)
    static const struct { const char* dpi; int px; } droid[5] = {
        { "mdpi", 48 }, { "hdpi", 72 }, { "xhdpi", 96 }, { "xxhdpi", 144 }, { "xxxhdpi", 192 } };
    for (int i = 0; i < 5; i++) {
        int n = droid[i].px;
        buf.assign((size_t)n * n * 4, 0);
        IcEscalar(src, sw, sh, &buf[0], n);
        char ruta[1200];
        snprintf(ruta, sizeof(ruta), "%s/icons/android-%s.png", out.c_str(), droid[i].dpi);
        IcGuardarPNG(ruta, &buf[0], n, n);
    }
    w3dEngine::FreeImage(src);
    // el mas grande generado -> icono.png (SDL_SetWindowIcon; viaja con los assets)
    if (mayor) {
        char cmd[2500];
        snprintf(cmd, sizeof(cmd), "cp \"%s/icons/%dx%d.png\" \"%s/icono.png\"",
                 out.c_str(), mayor, mayor, out.c_str());
        if (system(cmd)) {}
    }
    return mayor > 0;
}

// las ESCENAS del proyecto = los UI raiz colgados de SceneCollection (tipo ui). Cada uno
// es una escena con su propio arbol + scripts embebidos (multi-escena). Mismo criterio que
// W3dEscenaRegistrarTodas() del motor. El nombre de la escena = el "nombre" del UI.
static void RecolectarEscenas(std::vector<UI*>* out) {
    if (!SceneCollection) return;
    for (size_t i = 0; i < SceneCollection->Childrens.size(); i++) {
        Object* o = SceneCollection->Childrens[i];
        if (o && o->getType() == ObjectType::ui) out->push_back((UI*)o);
    }
}

// la raiz del repo Whisk3D (Core + UI 2D + game runtime). Dos formas de encontrarla:
//   1) la que el usuario fijo a mano en Ajustes (cfg.repoPath): es lo que hace que el editor
//      INSTALADO -- que no tiene el repo al lado -- pueda compilar juegos igual;
//   2) subiendo de carpeta desde res/ hasta dar con libs/Whisk3DCore, para el editor corriendo
//      desde el arbol de codigo (build/ con res/ copiado al lado del binario).
// En ambos casos se valida que exista la marca del repo (Objects.cpp). "" si no se encuentra.
static std::string RepoRoot() {
    if (!cfg.repoPath.empty() &&
        w3dFileSystem::FileExists(cfg.repoPath + "/libs/Whisk3DCore/objects/Objects.cpp"))
        return cfg.repoPath;
    std::string dir = Carpeta(w3dFileSystem::GetResDir());   // sube de res/
    for (int i = 0; i < 8; i++) {
        if (w3dFileSystem::FileExists(dir + "/libs/Whisk3DCore/objects/Objects.cpp")) return dir;
        std::string arriba = Carpeta(dir);
        if (arriba == dir) break;
        dir = arriba;
    }
    return "";
}

// asegura el .gitignore de la carpeta del juego: platform-build/ (todo lo generado
// para compilar) y build/ (los resultados) se REGENERAN con "Compilar juego", asi que
// no van al repo. Si el .gitignore no existe se crea con un comentario breve; si
// existe pero le faltan esas entradas, se agregan al final (sin duplicar). Se compara
// por LINEA EXACTA (recortando espacios): que exista "platform-build/" no cuenta
// como tener "build/". Asi cualquier proyecto nuevo queda prolijo en git solo.
static void AsegurarGitignore(const std::string& proy) {
    static const char* entradas[2] = { "platform-build/", "build/" };
    std::string ruta = proy + "/.gitignore";
    std::string contenido;
    bool tiene[2] = { false, false };
    bool existe = false;
    FILE* f = fopen(ruta.c_str(), "r");
    if (f) {
        existe = true;
        char lin[1024];
        while (fgets(lin, sizeof(lin), f)) {
            contenido += lin;
            std::string s(lin);
            while (!s.empty() && (s[s.size()-1] == '\n' || s[s.size()-1] == '\r' ||
                                  s[s.size()-1] == ' '  || s[s.size()-1] == '\t')) s.erase(s.size()-1);
            size_t ini = 0;
            while (ini < s.size() && (s[ini] == ' ' || s[ini] == '\t')) ini++;
            if (ini) s = s.substr(ini);
            for (int e = 0; e < 2; e++) if (s == entradas[e]) tiene[e] = true;
        }
        fclose(f);
    }
    if (tiene[0] && tiene[1]) return;                 // ya estan las dos: nada que hacer
    f = fopen(ruta.c_str(), existe ? "a" : "w");
    if (!f) return;                                    // sin permisos: no es fatal para compilar
    if (!existe)
        fputs("# Salidas de \"Compilar juego\" (Whisk3D): se regeneran, no van al repo\n", f);
    else if (!contenido.empty() && contenido[contenido.size()-1] != '\n')
        fputc('\n', f);                                // que la entrada nueva arranque en linea propia
    for (int e = 0; e < 2; e++) if (!tiene[e]) fprintf(f, "%s\n", entradas[e]);
    fclose(f);
}

// ============================================================================
//  PAK de assets ("Assets: Empaquetados" de la tarjeta Juego): junta TODOS los
//  archivos del juego que quedaron en platform-build/<plat> (los .w3dui
//  re-exportados, los .lua, los .png de la raiz -incluye font.png e icono.png-
//  y la subcarpeta assets/ recursiva) y los escribe como pak.cpp: un array de
//  bytes C que se COMPILA dentro del binario. El main generado lo registra con
//  W3dPakRegistrar antes de cargar nada y el Core (w3dFilesystem) resuelve las
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
static unsigned PakSemilla(const std::string& nombre) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < nombre.size(); i++) { h ^= (unsigned char)nombre[i]; h *= 16777619u; }
    return h;
}
// flujo XOR: xorshift32 avanzado cada 4 bytes (MISMO generador que el lector del Core)
struct PakFlujo { unsigned s; unsigned w; unsigned n; };
static void PakFlujoInit(PakFlujo* f, unsigned semilla) {
    f->s = semilla ? semilla : 0x57334441u;   // estado 0 atasca xorshift
    f->w = 0; f->n = 0;
}
static unsigned char PakFlujoByte(PakFlujo* f) {
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
//  cada entrada con SU nombre ("texturas/pausa.png", "scripts/menu.lua"), que es
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
//  la carpeta escenas/ del contenedor no se vuelca y nunca existe aca.
// ============================================================================
static const char* const kCarpetasAssets[] = {
    // el espejo del contenedor v4 (las categorias de W3dCategoriaPorExtension)
    "scripts", "texturas", "fuentes", "sonidos", "videos",
    "mallas", "animaciones", "modelos", "proyecto", "extra",
    // escenas/: los .w3dui que referencia el proyecto.json de un juego 3D (el
    // compilador los RE-EXPORTA ahi adentro con el nombre de entrada que el
    // archivo referencia, para que la UI del juego sea la VIVA del editor)
    "escenas",
    // proyectos v3 (archivos sueltos al lado del .w3d): siguen funcionando
    "assets", "contenido",
    0
};

// ============================================================================
//  LAS CARPETAS DE ASSETS QUE VIAJAN CON EL JUEGO — LA LISTA VIVA.
//
//  kCarpetasAssets son las que SIEMPRE existen (el espejo del contenedor). Pero
//  un proyecto puede tener archivos AFUERA del contenedor a proposito (refs
//  "ext:", ver W3dRefExternaMarcar): el demo guarda ahi los .cap de los rieles de
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
static void CarpAssetsDescubrir(const std::string& out) {
    gCarpAssets.clear();
    for (int c = 0; kCarpetasAssets[c]; c++) gCarpAssets.push_back(kCarpetasAssets[c]);
    std::vector<w3dFileSystem::DirEntry> ent;
    if (!w3dFileSystem::ListDir(out, ent)) return;
    for (size_t i = 0; i < ent.size(); i++) {
        if (!ent[i].isDir) continue;
        const std::string& n = ent[i].name;
        // artefactos del build, no assets del juego
        if (n == "out" || n == "icons" || n == "androidprj" || n == "." || n == "..") continue;
        bool ya = false;
        for (size_t k = 0; k < gCarpAssets.size(); k++) if (gCarpAssets[k] == n) { ya = true; break; }
        if (!ya) gCarpAssets.push_back(n);
    }
}

// ============================================================================
//  LO QUE EL PROYECTO NOMBRA POR RUTA RELATIVA Y NO ESTA EN NINGUNA LISTA.
//
//  Reporte del dueno: el .deb del demo salio con CERO archivos .wav. Y es cierto
//  que ningun paso lo copiaba: los sonidos del proyecto viven en sonidos/ y
//  musica/ AL LADO del .w3d, no adentro del contenedor ni referenciados por
//  ningun .w3dui -- los nombra el LUA y nada mas:
//      sonido("sonidos/" .. n, ...)          -- prefijo + variable
//      RUTA[s] = "musica/inst" .. NN .. ".wav"
//  El compilador copiaba (a) las entradas del contenedor, (b) las refs de los
//  .w3dui y (c) las refs externas declaradas: nada de eso cubre una ruta que el
//  script ARMA en tiempo de ejecucion. Resultado: el juego instalado, mudo.
//
//  LA REGLA, generica y sin nombres de ningun juego: se leen los .lua (y el
//  proyecto.json/.w3d) que ya quedaron en el staging, se sacan sus LITERALES de
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
static bool RefRelValida(const std::string& s) {
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
static void RefsRutasDe(const std::string& archivo, std::vector<std::string>* out) {
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(archivo, datos) || datos.empty()) return;
    // bytecode lua (0x1B "Lua") u otro binario: no tiene rutas que leer asi
    if (datos[0] == 0x1B) return;
    const char* p = (const char*)&datos[0];
    const size_t n = datos.size();
    std::string cur;
    for (size_t i = 0; i <= n; i++) {
        char c = (i < n) ? p[i] : ' ';
        bool deRuta = (c == '/' || c == '.' || c == '_' || c == '-' ||
                       (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                       (c >= 'a' && c <= 'z'));
        if (deRuta && cur.size() <= 300) { cur.push_back(c); continue; }
        if (cur.find('/') != std::string::npos && RefRelValida(cur)) out->push_back(cur);
        cur.clear();
    }
}
// junta los archivos de TEXTO del staging que pueden nombrar assets (.lua y el
// proyecto/escenas), sin entrar a los artefactos del build
static void TextosDelStaging(const std::string& dir, const std::string& rel,
                             std::vector<std::string>* out, int prof) {
    if (prof > 6) return;
    std::vector<w3dFileSystem::DirEntry> ent;
    if (!w3dFileSystem::ListDir(dir, ent)) return;
    for (size_t i = 0; i < ent.size(); i++) {
        const std::string& n = ent[i].name;
        if (n == "." || n == "..") continue;
        if (ent[i].isDir) {
            if (n == "out" || n == "icons" || n == "androidprj") continue;
            TextosDelStaging(dir + "/" + n, rel.empty() ? n : rel + "/" + n, out, prof + 1);
            continue;
        }
        size_t p = n.find_last_of('.');
        if (p == std::string::npos) continue;
        std::string ext = n.substr(p);
        if (ext == ".lua" || ext == ".json" || ext == ".w3dui" || ext == ".w3d")
            out->push_back(dir + "/" + n);
    }
}
static void CopiarRefsRelativasDeLua(const std::string& out, const std::string& proy) {
    std::vector<std::string> textos;
    TextosDelStaging(out, std::string(), &textos, 0);
    std::vector<std::string> rutas;
    for (size_t i = 0; i < textos.size(); i++) RefsRutasDe(textos[i], &rutas);
    std::set<std::string> archivos, carpetas;
    for (size_t i = 0; i < rutas.size(); i++) {
        const std::string& s = rutas[i];
        std::string dir = Carpeta(s);            // "sonidos/x.wav" -> "sonidos"
        if (dir.empty() || dir == s) {
            // nombre pelado con '/' raro: solo puede ser un archivo suelto
            if (w3dFileSystem::FileExists(proy + "/" + s) && !w3dFileSystem::IsDir(proy + "/" + s))
                archivos.insert(s);
            continue;
        }
        if (dir == "platform-build" || dir == "build") continue;
        // LA CARPETA ENTERA, no el archivo suelto: un .obj arrastra su .mtl y el
        // .mtl su .png, y ninguno de los dos aparece nombrado en el proyecto (los
        // nombra el archivo anterior de la cadena). Copiar solo lo nombrado dejaba
        // el escenario SIN sus materiales -> el juego lo dibujaba blanco.
        if (w3dFileSystem::IsDir(proy + "/" + dir)) carpetas.insert(dir);
    }
    int nA = 0, nC = 0;
    for (std::set<std::string>::iterator it = archivos.begin(); it != archivos.end(); ++it) {
        if (w3dFileSystem::FileExists(out + "/" + *it)) continue;   // ya viajo
        char cmd[3000];
        snprintf(cmd, sizeof(cmd), "cp \"%s/%s\" \"%s/%s\"",
                 proy.c_str(), it->c_str(), out.c_str(), it->c_str());
        if (system(cmd) == 0) nA++;
    }
    for (std::set<std::string>::iterator it = carpetas.begin(); it != carpetas.end(); ++it) {
        // -n (no-clobber): lo que YA esta en el staging manda (el volcado del
        // contenedor y las escenas re-exportadas son la version viva). Esto
        // COMPLETA carpetas a medias -- el caso real: escenario/ llegaba con los
        // .obj y los .w3dvis que el .w3d nombra, sin un solo .mtl.
        char cmd[3000];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/%s\" && cp -rn \"%s/%s/.\" \"%s/%s/\" 2>/dev/null || true",
                 out.c_str(), it->c_str(), proy.c_str(), it->c_str(), out.c_str(), it->c_str());
        if (system(cmd) == 0) nC++;
    }
    if (nA || nC)
        w3dLogf("CompilarJuego: %d carpeta(s) y %d archivo(s) que el proyecto nombra por "
                "ruta relativa, completados en el staging", nC, nA);
}

// junta las rutas RELATIVAS (contra 'out') de todos los archivos del juego:
// *.w3dui/*.lua/*.png de la raiz + assets/ entera recursiva. NO entra a icons/
// ni out/ ni androidprj/: son artefactos del build, no assets del juego.
static void PakListarRaiz(const std::string& out, std::vector<std::string>* rutas) {
    std::vector<w3dFileSystem::DirEntry> ent;
    if (!w3dFileSystem::ListDir(out, ent)) return;
    for (size_t i = 0; i < ent.size(); i++) {
        if (ent[i].isDir) continue;
        const std::string& n = ent[i].name;
        size_t p = n.find_last_of('.');
        if (p == std::string::npos) continue;
        std::string ext = n.substr(p);
        // .json = proyecto.json, la ESCENA 3D del juego (el resto es lo de siempre)
        if (ext == ".w3dui" || ext == ".lua" || ext == ".png" || ext == ".json") rutas->push_back(n);
    }
}
static void PakListarAssets(const std::string& out, const std::string& rel,
                            std::vector<std::string>* rutas) {
    std::vector<w3dFileSystem::DirEntry> ent;
    if (!w3dFileSystem::ListDir(out + "/" + rel, ent)) return;
    for (size_t i = 0; i < ent.size(); i++) {
        std::string r = rel + "/" + ent[i].name;
        if (ent[i].isDir) PakListarAssets(out, r, rutas);
        else              rutas->push_back(r);
    }
}

// ============================================================================
//  BYTECODE en PRODUCCION ("Modo debug" DESTILDADO): cada .lua COPIADO al
//  staging (platform-build/<plat>) se reemplaza por su bytecode Lua stripped
//  CON EL MISMO NOMBRE .lua (las refs de los .w3dui no cambian; CorrerArchivo
//  del Core detecta bytecode por el header 0x1B via luaL_loadbuffer, y su skip
//  de BOM no lo toca: el bytecode nunca arranca con EF BB BF). Aplica a los 4
//  targets porque corre ANTES de armar el pak / embeber / copiar assets: carga
//  mas rapida, menos peso y fuente ilegible. Los .lua del PROYECTO no se tocan
//  jamas (el IDE sigue editando fuente). Con "Modo debug" tildado: fuente tal
//  cual, como siempre.
//  Junta los mismos lugares que viajan con el juego: *.lua de la raiz del
//  staging + assets/ y contenido/ recursivas. Si algun .lua no compila
//  (sintaxis) devuelve false con el error humano en *err: el build ABORTA
//  (mejor enterarse aca que en el device).
// ============================================================================
static bool CompilarLuasStaging(const std::string& out, std::string* err,
                                unsigned* cuantos, size_t* bytesFuente, size_t* bytesBytecode) {
    *cuantos = 0; *bytesFuente = 0; *bytesBytecode = 0;
    std::vector<std::string> rutas;
    {   // .lua sueltos de la raiz del staging
        std::vector<w3dFileSystem::DirEntry> ent;
        if (w3dFileSystem::ListDir(out, ent))
            for (size_t i = 0; i < ent.size(); i++)
                if (!ent[i].isDir && ent[i].name.size() > 4 &&
                    ent[i].name.compare(ent[i].name.size() - 4, 4, ".lua") == 0)
                    rutas.push_back(ent[i].name);
    }
    {   // las carpetas de assets recursivas: solo los .lua (el resto viaja tal cual).
        // scripts/ es la del contenedor v4: sin ella los .lua internos se quedaban
        // en fuente aun compilando en produccion.
        std::vector<std::string> todas;
        for (int c = 0; CarpAsset(c); c++)
            PakListarAssets(out, CarpAsset(c), &todas);
        for (size_t i = 0; i < todas.size(); i++)
            if (todas[i].size() > 4 &&
                todas[i].compare(todas[i].size() - 4, 4, ".lua") == 0)
                rutas.push_back(todas[i]);
    }
    for (size_t i = 0; i < rutas.size(); i++) {
        std::string ruta = out + "/" + rutas[i];
        std::vector<unsigned char> antes, despues;
        w3dFileSystem::ReadFileBytes(ruta, antes);   // peso fuente (solo informativo)
        std::string detalle;
        if (!LuaCompilarArchivo(ruta, ruta, &detalle)) {
            *err = detalle.empty() ? (rutas[i] + ": no compilo") : detalle;
            return false;
        }
        w3dFileSystem::ReadFileBytes(ruta, despues);
        (*cuantos)++;
        *bytesFuente += antes.size();
        *bytesBytecode += despues.size();
    }
    return true;
}

// arma el pak con los archivos del juego y lo escribe como <out>/pak.cpp
// (array de bytes + tamano; el main generado lo registra). false si no hay
// archivos o no se pudo leer/escribir algo ('err' cuenta que paso).
static bool EscribirPak(const std::string& out, const std::string& nombre, std::string* err) {
    std::vector<std::string> rutas;
    PakListarRaiz(out, &rutas);
    // EL PAK SE ARMA DEL MISMO ESPEJO QUE COPIAN LOS OTROS TRES TARGETS: las
    // carpetas del contenedor volcado (texturas/, scripts/, fuentes/, mallas/...)
    // mas las dos de los proyectos v3. Si una carpeta no existe, no agrega nada.
    for (int c = 0; CarpAsset(c); c++)
        PakListarAssets(out, CarpAsset(c), &rutas);
    if (rutas.empty()) { *err = "no hay archivos del juego para empaquetar"; return false; }

    // contenido de cada archivo (en memoria: los juegos Whisk3D son chicos)
    std::vector< std::vector<unsigned char> > datos(rutas.size());
    for (size_t i = 0; i < rutas.size(); i++) {
        if (!w3dFileSystem::ReadFileBytes(out + "/" + rutas[i], datos[i])) {
            *err = "no pude leer " + rutas[i]; return false;
        }
    }
    unsigned semilla = PakSemilla(nombre);

    // layout: 12 de cabecera + (4 + entradas) de TOC + los datos seguidos
    unsigned tocTam = 4;
    for (size_t i = 0; i < rutas.size(); i++) tocTam += 2 + (unsigned)rutas[i].size() + 8;
    unsigned off = 12 + tocTam;
    std::vector<unsigned> offs(rutas.size());
    for (size_t i = 0; i < rutas.size(); i++) { offs[i] = off; off += (unsigned)datos[i].size(); }

    std::vector<unsigned char> pak;
    pak.reserve(off);
    static const unsigned char magia[8] = { 'W','3','D','P','A','K','1','\n' };
    pak.insert(pak.end(), magia, magia + 8);
    PakU32(pak, semilla);
    // TOC (cantidad + entradas) en claro primero...
    std::vector<unsigned char> toc;
    PakU32(toc, (unsigned)rutas.size());
    for (size_t i = 0; i < rutas.size(); i++) {
        toc.push_back((unsigned char)(rutas[i].size()));
        toc.push_back((unsigned char)(rutas[i].size() >> 8));
        toc.insert(toc.end(), rutas[i].begin(), rutas[i].end());
        PakU32(toc, offs[i]);
        PakU32(toc, (unsigned)datos[i].size());
    }
    // ...y XOReado con su flujo al pak
    { PakFlujo f; PakFlujoInit(&f, semilla ^ 0x544F4321u);
      for (size_t i = 0; i < toc.size(); i++) pak.push_back((unsigned char)(toc[i] ^ PakFlujoByte(&f))); }
    // los datos: cada archivo con su flujo propio (el lector lo deriva del TOC)
    for (size_t i = 0; i < rutas.size(); i++) {
        PakFlujo f; PakFlujoInit(&f, semilla ^ (offs[i] * 2654435761u) ^ (unsigned)datos[i].size());
        for (size_t k = 0; k < datos[i].size(); k++)
            pak.push_back((unsigned char)(datos[i][k] ^ PakFlujoByte(&f)));
    }

    // pak.cpp: el array + su tamano (el main generado los declara extern y llama
    // a W3dPakRegistrar antes de cargar nada)
    FILE* f = fopen((out + "/pak.cpp").c_str(), "w");
    if (!f) { *err = "no pude escribir pak.cpp"; return false; }
    fputs("// GENERADO por Whisk3D (Compilar juego, Assets: Empaquetados).\n", f);
    fprintf(f, "// %u archivo(s) del juego en un pak ofuscado (XOR de flujo).\n",
            (unsigned)rutas.size());
    fputs("// Ofuscacion anti-curiosos, NO criptografia: el lector y el formato\n", f);
    fputs("// estan en libs/Whisk3DCore/io/w3dFilesystem.cpp (W3dPakRegistrar).\n", f);
    fprintf(f, "extern const unsigned char g_w3dPakDatos[];\n");
    fprintf(f, "extern const unsigned g_w3dPakTam;\n");
    fprintf(f, "const unsigned g_w3dPakTam = %uu;\n", (unsigned)pak.size());
    fputs("const unsigned char g_w3dPakDatos[] = {", f);
    for (size_t i = 0; i < pak.size(); i++) {
        if ((i % 20u) == 0u) fputc('\n', f);   // 20 bytes por linea del array generado
        fprintf(f, "%u,", (unsigned)pak[i]);
    }
    fputs("\n};\n", f);
    { bool ok = (ferror(f) == 0); if (fclose(f) != 0) ok = false;   // el flush (y el disco lleno) pasa al cerrar
      if (!ok) { w3dLogfE("CompilarJuego: no pude escribir pak.cpp (disco lleno?)"); return false; } }
    w3dLogf("CompilarJuego: pak.cpp con %u archivo(s), %u bytes",
            (unsigned)rutas.size(), (unsigned)pak.size());
    return true;
}

// genera el main.cpp del juego. Dos caminos segun el proyecto:
//  - MULTI-ESCENA (varias UI raiz): W3dGameCargarEscena("x.w3dui") por cada escena (ACUMULA;
//    los scripts + refs vienen DENTRO de cada .w3dui, no se agregan a mano) + W3dGameEscenaInicial.
//  - UNA sola / legacy: W3dGameCargarUI(...) + W3dGameScript/W3dGameRef con lo que tiene el editor.
// El resto del main (ventana SDL/GL + loop + INPUT) es identico y lo comparten las 4
// plataformas, y es el MISMO manejo que el main artesanal del whiskpaddle (la referencia
// que anda): mouse (toque/arrastre/raton), multi-touch real (dedo 1..4 por slot),
// gamepad con alta/baja, salir() de lua, safe area en Android y el entry point de
// Android (SDL_main + AAssetManager). Antes el main generado solo cableaba teclado y
// gamepad: el mouse no andaba en el juego compilado.
// 'modoVentana' (0 Ventana, 1 Pantalla completa, 2 Sin bordes) decide como se crea la
// ventana en desktop y si Android va inmersivo; winW/winH es el tamano para "Sin bordes"
// (el lienzo de la UI cuando es responsive).
// 'orientacion' (0 Todas, 1 Solo vertical, 2 Solo horizontal): hint SDL_HINT_ORIENTATIONS
// para Android/iOS (en desktop/web no aplica; el manifest de Android lo clava ademas
// EscribirBuildAndroid). 'usarSonido': arranca el mixer de audio (W3dAudioInit) en el main.
// 'nombre': el nombre del juego (titulo de la ventana + carpeta de config persistente).
// 'conIcono': carga icono.png (generado del icono del proyecto) como icono de la ventana.
// 'empaquetar': assets EMPAQUETADOS -> declara el pak generado (pak.cpp) y lo
// registra en el Core (W3dPakRegistrar) como PRIMERA linea del main, antes de
// cargar nada (el icono, la fuente y las escenas ya salen del pak).
static bool EscribirMain(const std::string& ruta, const std::string& fontRel,
                         bool multi,
                         const std::string& w3duiRel, const std::vector<ObjScript>& objs,
                         const std::vector<EscenaGen>& escenas, const std::string& inicial,
                         int modoVentana, int winW, int winH,
                         int orientacion, bool usarSonido,
                         const std::string& nombre, bool conIcono, bool empaquetar,
                         bool con3D) {
    FILE* f = fopen(ruta.c_str(), "w");
    if (!f) return false;
    fputs("// GENERADO por Whisk3D (Compilar juego). No hace falta editarlo.\n", f);
    // Android NO define SDL_MAIN_HANDLED: SDL renombra main->SDL_main y SDLActivity
    // lo busca por ese nombre (sin esto la app abre y se cierra).
    fputs("#ifndef __ANDROID__\n#define SDL_MAIN_HANDLED\n#endif\n", f);
    fputs("#include <SDL2/SDL.h>\n#include \"w3drun.h\"\n", f);
    fputs("#include \"w3dGraphics.h\"\n", f);
    fputs("#include \"io/w3dFilesystem.h\"\n#include \"base/W3dConfig.h\"\n", f);
    fputs("#include \"script/W3dScript.h\"\n", f);
    // el registro COMPARTIDO de mandos (hotplug + avisos + los binds de lua)
    fputs("#include \"script/BindsJuego.h\"\n", f);
    fputs("#include <cstdio>\n#include <cstdlib>\n#include <ctime>\n", f);
    fputs("#ifdef __ANDROID__\n#include <jni.h>\n#include <android/asset_manager.h>\n"
          "#include <android/asset_manager_jni.h>\n#endif\n", f);
    fputs("#ifdef __EMSCRIPTEN__\n#include <emscripten.h>\n#include <emscripten/html5.h>\n#endif\n", f);
    if (conIcono)   // el icono de VENTANA es solo desktop (Android usa el launcher, web el favicon)
        fputs("#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)\n"
              "#include \"w3dTexture.h\" // DecodeImage: icono de la ventana\n#endif\n", f);
    if (usarSonido)   // audio/W3dAudio.h (forward-decl: evita meter el dir de audio al include path)
        fputs("namespace w3dEngine { bool W3dAudioInit(int); }\n", f);
    if (empaquetar)   // assets empaquetados: el pak generado (pak.cpp, se compila junto)
        fputs("// assets EMPAQUETADOS: todos los archivos del juego viajan en este pak\n"
              "extern const unsigned char g_w3dPakDatos[];\nextern const unsigned g_w3dPakTam;\n", f);
    fputs("namespace gfx = w3dEngine;\nstatic SDL_Window* g_win=0; static bool g_run=true;\n", f);
    fputs("static SDL_GameController* g_pad=0; static Uint32 g_lastMs=0;\n", f);
    // multi-touch: SDL da un fingerId arbitrario por dedo; se mapea a un slot fijo
    // 0..3 (dedo(1..4) en lua) para que cada jugador tactil conserve su dedo
    fputs("#define W3D_NDEDOS 4\n", f);
    fputs("static SDL_FingerID g_dedoId[W3D_NDEDOS]={(SDL_FingerID)-1,(SDL_FingerID)-1,"
          "(SDL_FingerID)-1,(SDL_FingerID)-1};\n", f);
    fputs("static int dedoSlot(SDL_FingerID id,bool crear){\n", f);
    fputs("for(int i=0;i<W3D_NDEDOS;i++)if(g_dedoId[i]==id)return i;\n", f);
    fputs("if(!crear)return -1;\n", f);
    fputs("for(int i=0;i<W3D_NDEDOS;i++)if(g_dedoId[i]==(SDL_FingerID)-1){g_dedoId[i]=id;return i;}\n", f);
    fputs("return -1;}\n", f);
    // ---- EL MANDO, CON HOTPLUG (el mismo registro que el editor) -------------
    // Antes el pad se abria SOLO si llegaba un SDL_CONTROLLERDEVICEADDED y solo
    // el primero; ahora se abren los que ya estaban al arrancar (padInicial) y
    // se atienden altas y bajas en caliente, avisando por el registro compartido
    // (consola + notificacion) igual que el editor. Al desenchufar se manda
    // ademas el estado NEUTRO a los scripts: sin eso el personaje se quedaba
    // caminando solo con el ultimo valor del stick.
    fputs("static const char* padTipo(SDL_GameController* gc){\n", f);
    fputs(" if(!gc)return \"generico\";\n switch(SDL_GameControllerGetType(gc)){\n", f);
    fputs("  case SDL_CONTROLLER_TYPE_XBOX360: case SDL_CONTROLLER_TYPE_XBOXONE: return \"xbox\";\n", f);
    fputs("  case SDL_CONTROLLER_TYPE_PS3: case SDL_CONTROLLER_TYPE_PS4:\n", f);
    fputs("  case SDL_CONTROLLER_TYPE_PS5: return \"playstation\";\n", f);
    fputs("  case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return \"nintendo\";\n", f);
    fputs("  default: return \"generico\";}}\n", f);
    fputs("static void padSoltar(){W3dGameStick(0,0,0);W3dGameStick(1,0,0);\n", f);
    fputs(" static const char* nn[8]={\"a\",\"b\",\"x\",\"y\",\"arriba\",\"abajo\",\"izquierda\",\"derecha\"};\n", f);
    fputs(" for(int i=0;i<8;i++)W3dGameBoton(nn[i],false);}\n", f);
    fputs("static void padAbrir(int idx){\n", f);
    fputs(" if(!SDL_IsGameController(idx))return;\n", f);
    fputs(" SDL_GameController* gc=SDL_GameControllerOpen(idx); if(!gc)return;\n", f);
    fputs(" SDL_Joystick* js=SDL_GameControllerGetJoystick(gc);\n", f);
    fputs(" int id=js?(int)SDL_JoystickInstanceID(js):-1;\n", f);
    fputs(" if(!W3dControlAlta(gc,id,SDL_GameControllerName(gc),padTipo(gc))){SDL_GameControllerClose(gc);return;}\n", f);
    fputs(" if(!g_pad)g_pad=gc;}\n", f);
    fputs("static void padCerrar(int id){\n", f);
    fputs(" void* h=0; if(!W3dControlBaja(id,&h))return;\n", f);
    fputs(" if(g_pad==(SDL_GameController*)h)g_pad=(SDL_GameController*)W3dControlHandle(0);\n", f);
    fputs(" if(h)SDL_GameControllerClose((SDL_GameController*)h);\n", f);
    fputs(" padSoltar();}\n", f);
    fputs("static void tamActual(int*w,int*h){\n#ifdef __EMSCRIPTEN__\n", f);
    fputs("double cw=0,ch=0; emscripten_get_element_css_size(\"#canvas\",&cw,&ch);\n", f);
    fputs("*w=(int)cw;*h=(int)ch; static int lw=0,lh=0;\n", f);
    fputs("if(*w!=lw||*h!=lh){emscripten_set_canvas_element_size(\"#canvas\",*w,*h);lw=*w;lh=*h;}\n", f);
    fputs("#else\nSDL_GL_GetDrawableSize(g_win,w,h);\n", f);
    fputs("if(*w<1||*h<1)SDL_GetWindowSize(g_win,w,h);\n#endif\nif(*w<1)*w=1;if(*h<1)*h=1;}\n", f);
    // Android: insets del notch/barras (getSafeInsets del Activity) -> la UI se acota
    fputs("#ifdef __ANDROID__\nstatic void actualizarSafeArea(){\n", f);
    fputs("JNIEnv* env=(JNIEnv*)SDL_AndroidGetJNIEnv();\n", f);
    fputs("jobject act=(jobject)SDL_AndroidGetActivity();\n", f);
    fputs("if(!env||!act)return;\n", f);
    fputs("jclass cl=env->GetObjectClass(act);\n", f);
    fputs("jmethodID mid=cl?env->GetMethodID(cl,\"getSafeInsets\",\"()[I\"):0;\n", f);
    fputs("if(mid){jintArray arr=(jintArray)env->CallObjectMethod(act,mid);\n", f);
    fputs("if(arr&&env->GetArrayLength(arr)>=4){jint* v=env->GetIntArrayElements(arr,0);\n", f);
    fputs("W3dGameSafeArea((int)v[0],(int)v[1],(int)v[2],(int)v[3]);\n", f);
    fputs("env->ReleaseIntArrayElements(arr,v,JNI_ABORT);}\n", f);
    fputs("if(arr)env->DeleteLocalRef(arr);}\n", f);
    fputs("if(env->ExceptionCheck())env->ExceptionClear();\n", f);
    fputs("if(cl)env->DeleteLocalRef(cl);\nenv->DeleteLocalRef(act);}\n#endif\n", f);
    fputs("static void frame(){SDL_Event e;while(SDL_PollEvent(&e)){\n", f);
    fputs("if(e.type==SDL_QUIT)g_run=false;\n", f);
    fputs("else if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE)g_run=false;\n", f);
    fputs("else if(e.type==SDL_KEYDOWN)W3dGameTecla(e.key.keysym.sym,true);\n", f);
    fputs("else if(e.type==SDL_KEYUP)W3dGameTecla(e.key.keysym.sym,false);\n", f);
    // HOTPLUG: el MISMO camino que el editor (padAbrir/padCerrar usan el registro
    // compartido de BindsJuego -> el aviso y los binds controles()/control() de lua
    // son identicos en el Play y en el .deb).
    fputs("else if(e.type==SDL_CONTROLLERDEVICEADDED)padAbrir(e.cdevice.which);\n", f);
    fputs("else if(e.type==SDL_CONTROLLERDEVICEREMOVED)padCerrar(e.cdevice.which);\n", f);
    // mouse: click/arrastre -> toque() en lua; posicion sin clic -> raton()
    fputs("else if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT)\n", f);
    fputs(" W3dGameToque((int)e.button.x,(int)e.button.y,true);\n", f);
    fputs("else if(e.type==SDL_MOUSEMOTION){\n", f);
    fputs(" W3dGameRaton((int)e.motion.x,(int)e.motion.y,true);\n", f);
    fputs(" if(e.motion.state&SDL_BUTTON_LMASK)W3dGameToque((int)e.motion.x,(int)e.motion.y,true);}\n", f);
    fputs("else if(e.type==SDL_MOUSEBUTTONUP&&e.button.button==SDL_BUTTON_LEFT)\n", f);
    fputs(" W3dGameToque((int)e.button.x,(int)e.button.y,false);\n", f);
    // multi-touch real (tactil/movil): cada dedo -> su slot -> dedo(1..4) en lua
    fputs("else if(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERMOTION){\n", f);
    fputs(" int s=dedoSlot(e.tfinger.fingerId,e.type==SDL_FINGERDOWN);\n", f);
    fputs(" if(s>=0){int w,h;tamActual(&w,&h);"
          "W3dGameDedo(s,(int)(e.tfinger.x*w),(int)(e.tfinger.y*h),true);}}\n", f);
    fputs("else if(e.type==SDL_FINGERUP){\n", f);
    fputs(" int s=dedoSlot(e.tfinger.fingerId,false);\n", f);
    fputs(" if(s>=0){int w,h;tamActual(&w,&h);"
          "W3dGameDedo(s,(int)(e.tfinger.x*w),(int)(e.tfinger.y*h),false);\n", f);
    fputs(" g_dedoId[s]=(SDL_FingerID)-1;}}\n", f);
    fputs("}\n", f);
    fputs("#ifdef __EMSCRIPTEN__\nif(!g_run){emscripten_cancel_main_loop();return;}\n#endif\n", f);
    // GAMEPAD del juego COMPILADO. Tiene que entregar exactamente lo mismo que el
    // Play del editor (SimJuego.cpp, TickReal) o el juego se porta distinto una vez
    // exportado. Aca se leian SOLO el stick izquierdo y el boton "a": el stick
    // derecho y b/x/y andaban en el editor y estaban muertos en el .deb.
    // TODOS los mandos, no solo el primero: el editor ya se comporta asi (su
    // axisState/buttonState los alimenta CUALQUIER control abierto, ver
    // RefreshInputControllerSDL), asi que leer solo g_pad hacia que en el .deb el
    // segundo mando enchufado no existiera. Se MEZCLAN: gana el eje de mayor
    // modulo y los botones se OR-ean, que es lo que hace falta en un juego de un
    // jugador con dos mandos a mano.
    fputs("{const float DZ=0.2f,M=1.0f/32767.0f;\n", f);
    fputs("float lx=0,ly=0,rx=0,ry=0;int du=0,dd=0,dl=0,dr=0,ba=0,bb=0,bx=0,by=0;\n", f);
    fputs("for(int ci=0;ci<W3dControlesCuantos();ci++){\n", f);
    fputs(" SDL_GameController* gc=(SDL_GameController*)W3dControlHandle(ci);if(!gc)continue;\n", f);
    fputs(" float ax=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_LEFTX)*M;\n", f);
    fputs(" float ay=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_LEFTY)*M;\n", f);
    fputs(" float bx2=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_RIGHTX)*M;\n", f);
    fputs(" float by2=SDL_GameControllerGetAxis(gc,SDL_CONTROLLER_AXIS_RIGHTY)*M;\n", f);
    fputs(" if(ax<0?-ax>(lx<0?-lx:lx):ax>(lx<0?-lx:lx))lx=ax;\n", f);
    fputs(" if(ay<0?-ay>(ly<0?-ly:ly):ay>(ly<0?-ly:ly))ly=ay;\n", f);
    fputs(" if(bx2<0?-bx2>(rx<0?-rx:rx):bx2>(rx<0?-rx:rx))rx=bx2;\n", f);
    fputs(" if(by2<0?-by2>(ry<0?-ry:ry):by2>(ry<0?-ry:ry))ry=by2;\n", f);
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
    // EL D-PAD ENTRA COMO EL STICK IZQUIERDO (ver el comentario largo de SimJuego.cpp):
    // con el analogico en reposo la cruceta manda ejes de exactamente 0/+-1, que es lo
    // que el juego reconoce como cruceta para aplicarle el modulo de las 8 direcciones.
    fputs("if(lx==0&&ly==0){lx=(float)(dr-dl);ly=(float)(dd-du);}\n", f);
    fputs("W3dGameStick(0,lx,ly);W3dGameStick(1,rx,ry);\n", f);
    fputs("W3dGameBoton(\"a\",ba!=0);W3dGameBoton(\"b\",bb!=0);\n", f);
    fputs("W3dGameBoton(\"x\",bx!=0);W3dGameBoton(\"y\",by!=0);\n", f);
    fputs("W3dGameBoton(\"arriba\",du!=0);W3dGameBoton(\"abajo\",dd!=0);\n", f);
    fputs("W3dGameBoton(\"izquierda\",dl!=0);W3dGameBoton(\"derecha\",dr!=0);}\n", f);
    fputs("Uint32 now=SDL_GetTicks();float dt=(now-g_lastMs)/1000.0f;g_lastMs=now;\n", f);
    fputs("if(dt<0)dt=0;if(dt>0.1f)dt=0.1f;\n", f);
    fputs("W3dGameActualizar(dt);\n", f);
    // lua salir(): cerrar el juego (no en web). En Android hay que MATAR el proceso:
    // si solo se retorna, la Activity queda viva y al reabrir arranca con los globals
    // C++ sucios ("abre y se cierra"); exit() la proxima vez parte limpio.
    fputs("#ifndef __EMSCRIPTEN__\nif(W3dGameQuierePararse()){\n", f);
    fputs("#ifdef __ANDROID__\nSDL_Quit();exit(0);\n#endif\n", f);
    fputs("g_run=false;return;}\n#endif\n", f);
    fputs("#ifdef __ANDROID__\nactualizarSafeArea();\n#endif\n", f);
    fputs("int w,h;tamActual(&w,&h);W3dGameRender(w,h);SDL_GL_SwapWindow(g_win);}\n", f);
    fputs("int main(int,char**){\n", f);
    if (empaquetar)   // ANTES de cargar nada: que icono/fuente/escenas ya salgan del pak
        fputs("w3dFileSystem::W3dPakRegistrar(g_w3dPakDatos,g_w3dPakTam);\n", f);
    fputs("#ifndef __ANDROID__\nSDL_SetMainReady();\n#endif\n", f);
    // dedos por separado (SDL_FINGER*), sin emular un mouse desde el touch: con la
    // emulacion 2 dedos pelean por el unico cursor y el multi-touch se rompe
    fputs("SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,\"0\");\n", f);
    // ORIENTACION (el desplegable "Orientacion" del editor): el hint le avisa a SDL en
    // Android/iOS que orientaciones se permiten. En desktop/web no aplica (no hace nada).
    if (orientacion == 1) {
        fputs("// orientacion: solo vertical (Android/iOS; en desktop/web no aplica)\n", f);
        fputs("SDL_SetHint(SDL_HINT_ORIENTATIONS,\"Portrait\");\n", f);
    } else if (orientacion == 2) {
        fputs("// orientacion: solo horizontal (Android/iOS; en desktop/web no aplica)\n", f);
        fputs("SDL_SetHint(SDL_HINT_ORIENTATIONS,\"LandscapeLeft LandscapeRight\");\n", f);
    }
    // ICONO EN EL DOCK (Linux): el app-id de la ventana debe coincidir con el
    // nombre del .desktop que instala el .deb (whisk3d-<juego>.desktop, ver
    // EscribirCMake). Este SDL2 no tiene SDL_HINT_APP_ID (es de SDL3): Wayland
    // toma el app_id xdg del env SDL_VIDEO_WAYLAND_WMCLASS y X11 el WM_CLASS de
    // SDL_VIDEO_X11_WMCLASS; ambos ANTES de SDL_Init.
    fputs("#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)\n", f);
    fputs("// app-id = nombre del .desktop instalado: asi el dock muestra el icono\n", f);
    fprintf(f, "SDL_setenv(\"SDL_VIDEO_WAYLAND_WMCLASS\",\"whisk3d-%s\",1);\n", nombre.c_str());
    fprintf(f, "SDL_setenv(\"SDL_VIDEO_X11_WMCLASS\",\"whisk3d-%s\",1);\n", nombre.c_str());
    fputs("#endif\n", f);
    if (usarSonido) {
        fputs("SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_AUDIO);\n", f);
        fputs("w3dEngine::W3dAudioInit(44100);// mixer del beep(); si el device no abre, sigue mudo\n", f);
    } else {
        fputs("SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER);\n", f);
    }
    fputs("SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);\n", f);
    // ESTENCIL: el reflejo del espejo recorta con el (ver Mirror.cpp). Si el
    // driver no lo da, EstencilBits()=0 y el reflejo se recorta por planos.
    fputs("SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);\n", f);
    // WebGL y Android: contexto GLES 2.0 (backend de shaders); desktop: COMPATIBILITY 2.1
    // (pipeline fijo; sin esto el driver puede dar un contexto CORE y no dibuja nada)
    fputs("#if defined(W3D_WEBGL)||defined(__ANDROID__)\n", f);
    fputs("SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);\n", f);
    fputs("SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);\n", f);
    fputs("#else\nSDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);\n", f);
    fputs("SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);\n#endif\n", f);
    // la VENTANA segun el "Modo ventana" del editor. En Web el modo NO aplica (el
    // tamano lo maneja el canvas de la pagina): se crea la ventana comun siempre.
    // El titulo de la ventana = el nombre del juego (el archivo .w3d del proyecto).
    std::string tit = Esc(nombre);
    fputs("#ifdef __EMSCRIPTEN__\n", f);
    fputs("// Web: el modo ventana no aplica (manda el canvas de la pagina)\n", f);
    fprintf(f, "g_win=SDL_CreateWindow(\"%s\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,600,600,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);\n", tit.c_str());
    fputs("#else\n", f);
    if (modoVentana == 0) {
        // VENTANA: redimensionable, sin fullscreen (en Android queda la barra de estado)
        fputs("// modo Ventana: redimensionable, sin fullscreen\n", f);
        fprintf(f, "g_win=SDL_CreateWindow(\"%s\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,600,600,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);\n", tit.c_str());
    } else if (modoVentana == 2) {
        // SIN BORDES: ventana al tamano de la UI y sin decoracion (en Android no hay
        // ventanas: va a pantalla completa igual que el modo fullscreen, mas abajo)
        fputs("// modo Sin bordes: tamano de la UI, sin decoracion\n", f);
        fprintf(f, "g_win=SDL_CreateWindow(\"%s\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,%d,%d,SDL_WINDOW_OPENGL|SDL_WINDOW_BORDERLESS);\n", tit.c_str(), winW, winH);
    } else {
        // PANTALLA COMPLETA (default): fullscreen "desktop" (la resolucion del escritorio)
        fputs("// modo Pantalla completa: fullscreen desktop\n", f);
        fprintf(f, "g_win=SDL_CreateWindow(\"%s\",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,600,600,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN_DESKTOP);\n", tit.c_str());
    }
    fputs("#endif\n", f);
    fputs("SDL_GLContext ctx=SDL_GL_CreateContext(g_win);if(!ctx)return 2;SDL_GL_SetSwapInterval(1);\n", f);
    if (modoVentana != 0) {
        // Android: pantalla completa INMERSIVA (oculta las barras de estado/navegacion).
        // "Sin bordes" tambien va por aca porque en Android no existen las ventanas.
        // En el modo Ventana NO se llama: el juego queda con la barra de estado visible.
        fputs("#ifdef __ANDROID__\nSDL_SetWindowFullscreen(g_win,SDL_WINDOW_FULLSCREEN_DESKTOP);\n#endif\n", f);
    }
    fputs("#ifdef W3D_GLES2\ngfx::GLES2Init((void*(*)(const char*))SDL_GL_GetProcAddress);\n#endif\n", f);
    if (conIcono) {
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
    fprintf(f, "{char* pref=SDL_GetPrefPath(\"Whisk3D\",\"%s\");\n", tit.c_str());
    fputs("if(pref){w3dFileSystem::SetUserDataDir(pref);SDL_free(pref);}}\n", f);
    fprintf(f, "w3dEngine::ConfigSetNombre(\"%s\");w3dEngine::ConfigLoad();\n", tit.c_str());
    // reforzar la semilla del random con entropia que varia entre corridas
    fputs("W3dScriptSemilla((unsigned)time(0));\n", f);
    fprintf(f, "W3dGameFontPngSet(\"%s\");\n", Esc(fontRel).c_str());
    // CAPTURA de verificacion: con W3D_CAPTURA=<png> el juego guarda ese frame y cierra.
    // Es lo que usa la prueba de regresion para comprobar POR PIXELES que el juego
    // compilado dibuja de verdad su escena. Sin la variable no cambia nada.
    fputs("{const char* cap=getenv(\"W3D_CAPTURA\");\n", f);
    fputs("if(cap&&*cap){const char* nf=getenv(\"W3D_CAPTURA_FRAME\");\n", f);
    fputs("W3dGameCaptura(cap,nf?atoi(nf):30);}}\n", f);
    if (con3D) {
        // LA ESCENA 3D del juego: el proyecto.json que quedo al lado del binario. Va
        // ANTES de las escenas de UI y de W3dGameInicio: los scripts (que en un juego 3D
        // cuelgan de los objetos de la escena) resuelven sus refs recien en el Inicio.
        // Si falla, el juego sigue (se ve el HUD sobre negro) y queda el motivo en el log.
        fputs("if(!W3dGameCargarEscena3D(\"proyecto.json\"))\n", f);
        fputs("  fprintf(stderr,\"[W3D] no pude cargar la escena 3D (proyecto.json)\\n\");\n", f);
    }
    if (con3D) {
        // en 3D las escenas de UI las trae el propio proyecto.json (nodo "ui" ->
        // escenas/<x>.w3dui, que el compilador dejo con la version viva), y los scripts
        // vienen colgados de sus objetos: no hay nada que cablear a mano aca.
    } else if (multi) {
        // MULTI-ESCENA: cargar TODAS las escenas (cada .w3dui trae sus scripts embebidos)
        // y elegir la inicial por su NOMBRE. NADA de W3dGameScript/W3dGameRef.
        for (size_t i = 0; i < escenas.size(); i++)
            fprintf(f, "if(!W3dGameCargarEscena(\"%s\"))return 1;\n", Esc(escenas[i].archivo).c_str());
        fprintf(f, "W3dGameEscenaInicial(\"%s\");\n", Esc(inicial).c_str());
    } else {
        // UNA sola escena / legacy: cargar la UI + agregar los scripts + sus referencias
        fprintf(f, "if(!W3dGameCargarUI(\"%s\"))return 1;\n", Esc(w3duiRel).c_str());
        for (size_t o = 0; o < objs.size(); o++)
            for (size_t s = 0; s < objs[o].scripts.size(); s++) {
                // FORMATO v4: la ruta es el NOMBRE DE ENTRADA del contenedor
                // ("scripts/menu.lua") y el staging es su espejo, asi que va COMPLETA.
                // Aplanar por basename no alcanza: proyecto/icono.png y
                // texturas/icono.png pueden coexistir.
                fprintf(f, "W3dGameScript(\"%s\");\n", Esc(W3dEsNombreDeEntrada(objs[o].scripts[s].ruta)
                            ? objs[o].scripts[s].ruta : Base(objs[o].scripts[s].ruta)).c_str());
                for (size_t r = 0; r < objs[o].scripts[s].refs.size(); r++)
                    fprintf(f, "W3dGameRef(\"%s\",\"%s\");\n",
                            Esc(objs[o].scripts[s].refs[r].first).c_str(),
                            Esc(objs[o].scripts[s].refs[r].second).c_str());
            }
    }
    // LOS MANDOS QUE YA ESTABAN ENCHUFADOS al arrancar. No alcanza con esperar el
    // SDL_CONTROLLERDEVICEADDED: si el juego arranca con el pad puesto, se abre
    // ACA (y si el evento llega igual, W3dControlAlta descarta el repetido).
    fputs("for(int i=0;i<SDL_NumJoysticks();i++)padAbrir(i);\n", f);
    fputs("W3dGameInicio();g_lastMs=SDL_GetTicks();\n", f);
    fputs("#ifdef __EMSCRIPTEN__\nemscripten_set_main_loop(frame,0,1);\n#else\n", f);
    fputs("while(g_run)frame();SDL_Quit();\n#endif\nreturn 0;}\n", f);
    bool okMain = (ferror(f) == 0);
    if (fclose(f) != 0) okMain = false;   // el flush pasa al cerrar: sin esto el main.cpp quedaba truncado
    return okMain;
}

// ----------------------------------------------------------------------------
//  PC (Linux): genera el CMakeLists.txt del juego. Antes se reusaba (por sed) el
//  CMake del ejemplo whiskpaddle, pero ese apunta a rutas relativas del ejemplo y
//  hardcodea SUS archivos -> no servia para un juego generado aparte. Aca se
//  genera uno propio: mismas fuentes del runtime que el ejemplo (incluye
//  W3dEscena.cpp para multi-escena y Textura2D.cpp para imagenes), compila el
//  main.cpp GENERADO, y copia/instala TODAS las escenas (.w3dui) + scripts (.lua)
//  + texturas/fuente (.png) que quedaron en platform-build/linux (por glob, sin
//  nombres fijos).
//  W3DROOT lo pasa CompilarJuego con -DW3DROOT (la raiz del repo Whisk3D).
//  'usarFisica'/'usarSonido' (los checkboxes de la tarjeta): destildados se excluyen
//  las fuentes de ese subsistema y se ajustan las macros (build mas liviano).
//  'modoDebug': define W3D_DEV_LOG=1 (log + ring + depurar()) o =0 (produccion:
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
//  W3dRecursos/MallaDatos/TexturaCache en una de las dos y el juego no linkeaba.
//  Asi que las fuentes que agrega un juego 3D viven ACA, en un solo array, y los
//  dos generadores lo recorren. Si manana el pase 3D necesita otro .cpp, se
//  agrega en un solo lugar y las dos plataformas lo reciben.
//
//  Que son: el lector de proyecto (import_w3d, el MISMO del editor, compilado con
//  W3D_SIN_EDITOR), los objetos de escena que el editor define fuera del Core
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
//  desincronizaron: al Android.mk le faltaban W3dRecursos/MallaDatos/TexturaCache
//  desde que existen, o sea que el APK ni siquiera linkeaba. Ahora la lista es
//  ESTA y los dos generadores la recorren; el Android.mk del juego se GENERA
//  entero (ya no se parchea la plantilla del ejemplo con sed).
//
//  Lo que NO esta aca es lo que cambia por plataforma o por opcion: el main.cpp
//  generado, w3drun.cpp, el pak, el backend de graficos (GL vs GLES2), la fisica
//  y el audio (checkboxes), las fuentes 3D (kFuentes3D) y lua.
// ============================================================================
static const char* kFuentesBase[] = {
    "${CORE}/gfx/w3dTexture.cpp",
    "${CORE}/io/w3dFilesystem.cpp", "${CORE}/io/w3dCompress.cpp",
    // .w3dm: la geometria de las mallas viaja en NUESTRO formato de texto (el juego la lee igual que el editor)
    "${CORE}/io/W3dTexto.cpp", "${CORE}/io/W3dMalla.cpp",
    "${CORE}/base/w3dlog.cpp", "${CORE}/base/W3dInteractionState.cpp", "${CORE}/base/W3dConfig.cpp",
    "${CORE}/math/Vector3.cpp", "${CORE}/math/Quaternion.cpp", "${CORE}/math/Matrix4.cpp",
    "${CORE}/objects/Objects.cpp", "${CORE}/objects/Mesh.cpp", "${CORE}/objects/Light.cpp",
    "${CORE}/objects/Materials.cpp", "${CORE}/objects/Textures.cpp", "${CORE}/objects/CameraBase.cpp",
    "${CORE}/objects/Armature.cpp", "${CORE}/objects/RenderColors.cpp",
    "${CORE}/animation/Animation.cpp", "${CORE}/animation/VertexAnimation.cpp",
    // Armature2DAnimation.cpp va SIEMPRE: Animation.cpp lo llama sin ifdef (Arm2DClipActivo /
    // Armature2DEvaluar) y sin el .cpp el juego no linkea
    "${CORE}/animation/SkeletalAnimation.cpp", "${CORE}/animation/Armature2DAnimation.cpp",
    // el almacen de recursos + datos compartidos van SIEMPRE: VertexAnimation.cpp,
    // Textures.cpp y el import los llaman sin ifdef (W3dRecursoBuscar/Retener) ->
    // sin estos .cpp el juego compilado no linkea (visto en integracion_portales)
    "${CORE}/io/W3dRecursos.cpp", "${CORE}/objects/MallaDatos.cpp", "${CORE}/objects/TexturaCache.cpp",
    "${CORE}/script/W3dScript.cpp",
    "${UILIB}/text/W3dFont.cpp", "${UILIB}/text/bitmapText.cpp", "${UILIB}/text/font.cpp",
    "${UILIB}/draw/glesdraw.cpp", "${UILIB}/draw/W3dAtlasPacker.cpp",
    "${UILIB}/core/UI.cpp", "${UILIB}/theme/colores.cpp",
    "${W3DROOT}/main/render/UIOverlay.cpp", "${W3DROOT}/main/script/BindsJuego.cpp",
    "${W3DROOT}/main/W3dEscena.cpp",
    "${W3DROOT}/main/io/UI2DFormato.cpp", "${W3DROOT}/main/io/Fuente2D.cpp",
    "${W3DROOT}/main/io/Textura2D.cpp",
    0
};
// LOS INCLUDES comunes (misma razon: uno solo para las dos plataformas).
static const char* kIncludesBase[] = {
    "${W3DROOT}", "${W3DROOT}/libs", "${W3DROOT}/main", "${W3DROOT}/main/render", "${W3DROOT}/main/io",
    "${CORE}", "${CORE}/base", "${CORE}/gfx", "${CORE}/io", "${CORE}/math", "${CORE}/objects",
    "${CORE}/script", "${CORE}/animation", "${CORE}/thirdparty",
    "${UILIB}", "${UILIB}/text", "${UILIB}/draw", "${UILIB}/core", "${UILIB}/theme", "${UILIB}/widgets",
    0
};

static const char* kFuentes3D[] = {
    "${CORE}/objects/VisSet.cpp",              // sets de visibilidad por celda (PVS)
    "${CORE}/io/W3dZip.cpp",                   // el .w3d es un zip (rutas/entradas del proyecto)
    "${CORE}/io/W3dAlmacen.cpp",
    "${W3DROOT}/main/render/OpcionesRender.cpp",  // g_renderBg + los flags de dibujo del Core
    "${W3DROOT}/main/render/EscenaRender.cpp",    // EL pase 3D compartido con el viewport del editor
    "${W3DROOT}/main/importers/import_w3d.cpp",   // EL MISMO lector de proyecto que el editor
    "${W3DROOT}/main/importers/import_obj.cpp",   // carga diferida de texturas + materiales
    "${W3DROOT}/main/importers/import_wobj.cpp",  // mallas .obj referenciadas (proyectos viejos)
    "${W3DROOT}/main/edit/MeshEdit.cpp",          // genera la malla de render (modificadores, bordes, PVS)
    "${W3DROOT}/main/objects/EditMesh.cpp",
    "${W3DROOT}/main/objects/Primitivas.cpp",
    "${W3DROOT}/main/objects/Camera.cpp",         // camara + riel (el encuadre del juego sale de aca)
    "${W3DROOT}/main/objects/Curve.cpp",
    "${W3DROOT}/main/objects/Culling.cpp",
    "${W3DROOT}/main/objects/LOD.cpp",
    "${W3DROOT}/main/objects/VisZona.cpp",
    "${W3DROOT}/main/objects/Collection.cpp",
    "${W3DROOT}/main/objects/Mirror.cpp",
    "${W3DROOT}/main/objects/Instance.cpp",
    "${W3DROOT}/main/objects/Particulas.cpp",
    "${W3DROOT}/main/objects/Empty.cpp",
    "${W3DROOT}/main/objects/Target.cpp",
    "${W3DROOT}/main/objects/Gamepad.cpp",
    "${W3DROOT}/main/objects/Scene.cpp",
    "${W3DROOT}/main/io/W3dContenedor.cpp",
    "${W3DROOT}/main/ui/W3dColors.cpp",
    "${W3DROOT}/main/config/W3dLang.cpp",
    "${W3DROOT}/main/config/w3dVersion.cpp",
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
static std::string AEmcc(const char* ruta) {
    std::string out = ruta;
    struct { const char* de; const char* a; } m[] = {
        { "${W3DROOT}", "$W3D" }, { "${CORE}", "$CORE" }, { "${UILIB}", "$UILIB" }, { 0, 0 }
    };
    for (int k = 0; m[k].de; k++) {
        const std::string de = m[k].de, a = m[k].a;
        size_t p;
        while ((p = out.find(de)) != std::string::npos)
            out = out.substr(0, p) + a + out.substr(p + de.size());
    }
    return out;
}

static std::string AMk(const char* ruta) {
    std::string out = ruta;
    struct { const char* de; const char* a; } m[] = {
        { "${W3DROOT}", "$(W3D)" }, { "${CORE}", "$(CORE)" }, { "${UILIB}", "$(UILIB)" }, { 0, 0 }
    };
    for (int k = 0; m[k].de; k++) {
        const std::string de = m[k].de, a = m[k].a;
        size_t p;
        while ((p = out.find(de)) != std::string::npos)
            out = out.substr(0, p) + a + out.substr(p + de.size());
    }
    return out;
}

static bool EscribirCMake(const std::string& ruta, const std::string& nombre,
                          bool usarFisica, bool usarSonido, bool modoDebug,
                          const std::vector<int>& iconos, bool empaquetar, bool con3D) {
    FILE* f = fopen(ruta.c_str(), "w");
    if (!f) return false;
    fputs("# GENERADO por Whisk3D (Compilar juego). No hace falta editarlo.\n", f);
    fputs("cmake_minimum_required(VERSION 3.16)\n", f);
    fprintf(f, "project(%s CXX C)\n", nombre.c_str());
    fputs("set(CMAKE_CXX_STANDARD 17)\n", f);
    fputs("if(NOT DEFINED W3DROOT)\n  message(FATAL_ERROR \"pasa -DW3DROOT=<raiz del repo Whisk3D>\")\nendif()\n", f);
    fputs("set(CORE ${W3DROOT}/libs/Whisk3DCore)\n", f);
    fputs("set(UILIB ${W3DROOT}/libs/WhiskUI)\n", f);
    fputs("set(GAME ${W3DROOT}/../Whisk3D-Examples/game)\n", f);
    fputs("set(LUA ${W3DROOT}/thirdparty/lua/src)\n", f);
    fputs("find_package(SDL2 REQUIRED)\nfind_package(OpenGL REQUIRED)\n", f);
    fputs("file(GLOB LUA_SRC ${LUA}/*.c)\nlist(REMOVE_ITEM LUA_SRC ${LUA}/lua.c ${LUA}/luac.c)\n", f);
    fprintf(f, "add_executable(%s\n", nombre.c_str());
    fputs("  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp\n  ${GAME}/w3drun.cpp\n", f);
    if (empaquetar)   // el pak generado con todos los archivos del juego
        fputs("  ${CMAKE_CURRENT_SOURCE_DIR}/pak.cpp\n", f);
    fputs("  ${CORE}/gfx/w3dGraphics.cpp\n", f);   // backend GL de PC (en Android es el de GLES2)
    // el resto del runtime: kFuentesBase, la MISMA lista que usa el generador de Android
    for (int i = 0; kFuentesBase[i]; i++) fprintf(f, "  %s\n", kFuentesBase[i]);
    // fisica minima del Core (velocidad + rebotes): solo con el checkbox "Usar motor de
    // fisica" tildado; sin el, W3D_SIN_FISICA deja los binds como stubs (en W3dScript.cpp)
    if (usarFisica) fputs("  ${CORE}/physics/W3dFisica.cpp\n", f);
    // W3dAudio.cpp SIEMPRE (sin W3D_ENABLE_AUDIO es el dispatcher stub que hace linkear
    // a beep()); el backend SDL solo con el checkbox "Usar sonido" tildado
    if (usarSonido) fputs("  ${CORE}/audio/W3dAudio.cpp ${CORE}/audio/W3dAudioSDL.cpp\n", f);
    else            fputs("  ${CORE}/audio/W3dAudio.cpp\n", f);
    // JUEGO 3D: el lector de proyecto + los objetos de escena + el pase 3D (kFuentes3D,
    // la MISMA lista que usa el generador de Android). Un juego 2D no las compila.
    if (con3D)
        for (int i = 0; kFuentes3D[i]; i++) fprintf(f, "  %s\n", kFuentes3D[i]);
    fputs("  ${LUA_SRC}\n)\n", f);
    // W3D_DEV_LOG: 1 = modo debug (log + ring + depurar()); 0 = produccion (stubs no-op)
    // W3D_SIN_EDITOR: el lector de proyecto compartido deja afuera lo que solo existe
    // con editor (layout de viewports, sesion, icono de ventana). Ver import_w3d.cpp.
    fprintf(f, "target_compile_definitions(%s PRIVATE W3D_OPENGL W3D_GAME_RUNTIME W3D_GAME_NO_TEX "
               "LUA_USE_POSIX W3D_STB_IMPL W3D_DEV_LOG=%d%s%s%s)\n", nombre.c_str(),
            modoDebug ? 1 : 0,
            usarSonido ? " W3D_ENABLE_AUDIO" : "",
            usarFisica ? "" : " W3D_SIN_FISICA",
            con3D ? " W3D_SIN_EDITOR W3D_JUEGO_3D" : "");
    fprintf(f, "target_include_directories(%s PRIVATE\n  ${GAME}\n", nombre.c_str());
    for (int i = 0; kIncludesBase[i]; i++) fprintf(f, "  %s\n", kIncludesBase[i]);
    if (con3D)   // los headers del editor que el lector de proyecto compartido incluye
        for (int i = 0; kIncludes3D[i]; i++) fprintf(f, "  %s\n", kIncludes3D[i]);
    fputs("  ${LUA})\n", f);
    fprintf(f, "target_link_libraries(%s PRIVATE SDL2::SDL2 OpenGL::GL OpenGL::GLU)\n", nombre.c_str());
    if (!empaquetar) {
        // TODOS los assets del juego (multi-escena) al lado del binario y en el paquete: por glob
        // (no por nombre) -> sirve para 1 o N escenas + las texturas/fuente que sea.
        fputs("file(GLOB GAME_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/*.w3dui "
              "${CMAKE_CURRENT_SOURCE_DIR}/*.lua ${CMAKE_CURRENT_SOURCE_DIR}/*.png "
              "${CMAKE_CURRENT_SOURCE_DIR}/*.json)\n", f);   // *.json = proyecto.json (la escena 3D)
        fputs("foreach(A ${GAME_ASSETS})\n", f);
        fprintf(f, "  add_custom_command(TARGET %s POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "
                   "${A} $<TARGET_FILE_DIR:%s>)\n", nombre.c_str(), nombre.c_str());
        fputs("endforeach()\n", f);
        // LAS CARPETAS DE ASSETS (el espejo del contenedor: texturas/, scripts/,
        // fuentes/, mallas/... + las dos legacy assets/ y contenido/): cada una
        // viaja ENTERA y CON su carpeta al lado del binario Y al paquete, porque
        // los .w3dui las referencian con esa ruta relativa ("texturas/pausa.png").
        // La lista es kCarpetasAssets, la MISMA que usan el pak, Android y emcc.
        for (int c = 0; CarpAsset(c); c++) {
            const char* d = CarpAsset(c);
            fprintf(f, "if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/%s)\n", d);
            fprintf(f, "  add_custom_command(TARGET %s POST_BUILD COMMAND ${CMAKE_COMMAND} -E "
                       "copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/%s $<TARGET_FILE_DIR:%s>/%s)\n",
                    nombre.c_str(), d, nombre.c_str(), d);
            fprintf(f, "  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/%s DESTINATION lib/whisk3d-%s)\n",
                    d, nombre.c_str());
            fputs("endif()\n", f);
        }
    }
    // instalacion + paquete .deb (cpack -G DEB): binario (+ assets si van sueltos), con un
    // lanzador que hace cd a la carpeta de datos (el juego lee sus assets del directorio
    // actual; empaquetado lee del pak y el cd queda igual de inofensivo).
    fprintf(f, "install(TARGETS %s DESTINATION lib/whisk3d-%s)\n", nombre.c_str(), nombre.c_str());
    if (!empaquetar)
        fprintf(f, "install(FILES ${GAME_ASSETS} DESTINATION lib/whisk3d-%s)\n", nombre.c_str());
    fprintf(f, "file(WRITE ${CMAKE_BINARY_DIR}/whisk3d-%s "
               "\"#!/bin/sh\\ncd /usr/lib/whisk3d-%s && exec ./%s \\\"$@\\\"\\n\")\n",
            nombre.c_str(), nombre.c_str(), nombre.c_str());
    fprintf(f, "install(PROGRAMS ${CMAKE_BINARY_DIR}/whisk3d-%s DESTINATION bin)\n", nombre.c_str());
    // MENU DE APLICACIONES: el .desktop (como el CMake artesanal del whiskpaddle;
    // sin esto el juego instalado no aparecia en el menu) + los iconos hicolor si
    // el proyecto tiene icono (Icon=<juego> resuelve contra hicolor/<n>x<n>/apps).
    fprintf(f, "file(WRITE ${CMAKE_BINARY_DIR}/whisk3d-%s.desktop\n", nombre.c_str());
    fprintf(f, "\"[Desktop Entry]\\nType=Application\\nName=%s\\n"
               "Comment=Juego hecho con el editor Whisk3D\\nExec=whisk3d-%s\\n"
               "Terminal=false\\nCategories=Game;\\n%s\")\n",
            nombre.c_str(), nombre.c_str(),
            iconos.empty() ? "" : (std::string("Icon=") + nombre + "\\n").c_str());
    fprintf(f, "install(FILES ${CMAKE_BINARY_DIR}/whisk3d-%s.desktop DESTINATION share/applications)\n",
            nombre.c_str());
    for (size_t i = 0; i < iconos.size(); i++)
        fprintf(f, "install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/icons/%dx%d.png "
                   "DESTINATION share/icons/hicolor/%dx%d/apps RENAME %s.png)\n",
                iconos[i], iconos[i], iconos[i], iconos[i], nombre.c_str());
    fputs("set(CPACK_GENERATOR DEB)\n", f);
    fprintf(f, "set(CPACK_PACKAGE_NAME \"whisk3d-%s\")\n", nombre.c_str());
    fputs("set(CPACK_PACKAGE_VERSION \"1.0.0\")\n", f);
    fputs("set(CPACK_PACKAGE_CONTACT \"Whisk3D\")\n", f);
    fputs("set(CPACK_DEBIAN_PACKAGE_DEPENDS \"libsdl2-2.0-0\")\n", f);
    fputs("set(CPACK_PACKAGE_DESCRIPTION_SUMMARY \"Juego hecho con el editor Whisk3D\")\n", f);
    // nombre del ARTEFACTO: <juego>-linux-<arch> (ej whiskpaddle-linux-x86_64.deb).
    // Antes cpack armaba whisk3d-<nombre>-1.0.0-Linux.deb.
    fprintf(f, "set(CPACK_PACKAGE_FILE_NAME \"%s-linux-${CMAKE_SYSTEM_PROCESSOR}\")\n", nombre.c_str());
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
//  'usarFisica'/'usarSonido' destildados: sed sobre la COPIA del Android.mk excluye esas
//  fuentes y ajusta las macros (W3D_SIN_FISICA / sin W3D_ENABLE_AUDIO).
//  'modoDebug': sed agrega -DW3D_DEV_LOG=1/0 al LOCAL_CPPFLAGS de la COPIA (1 = log +
//  ring + depurar(); 0 = produccion sin nada de eso). Ver base/w3dlog.h.
//  'conIcono': reemplaza los ic_launcher de res/mipmap-* de la COPIA con los iconos
//  generados (icons/android-<dpi>.png) y asegura android:icon en el manifest.
//  'empaquetar': assets EMPAQUETADOS -> compila el pak.cpp generado (via sed sobre
//  la COPIA del Android.mk) y deja los assets del APK VACIOS (todo viaja dentro
//  de libmain.so); en sueltos, se copian como siempre.
static bool EscribirBuildAndroid(const std::string& ruta, const std::string& repo,
                                 const std::string& out, const std::string& nombre,
                                 int orientacion,
                                 bool usarFisica, bool usarSonido, bool modoDebug,
                                 bool conIcono, bool empaquetar, bool con3D) {
    std::string root = Carpeta(repo); // carpeta del ecosistema (contiene Whisk3D y Whisk3D-Examples)
    FILE* f = fopen(ruta.c_str(), "w");
    if (!f) return false;
    fputs("#!/usr/bin/env bash\n", f);
    fputs("# GENERADO por Whisk3D (Compilar juego -> Android). Arma el APK del juego.\n", f);
    fputs("set -euo pipefail\n", f);
    fprintf(f, "OUT=\"%s\"\n", Esc(out).c_str());
    fprintf(f, "ROOT=\"%s\"\n", Esc(root).c_str());
    fputs("TPL=\"$ROOT/Whisk3D-Examples/ui/whiskpaddle/platform/android\"\n", f);
    // la COPIA del proyecto Android, dentro de platform-build/android/. Se llamaba
    // "proyecto" y CHOCABA de frente con la carpeta proyecto/ del contenedor v4 (la
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
    fputs("echo \"[W3D] 12 Copiando la plantilla Android\"\n", f);
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
               "\"$AND/app/build.gradle\"\n", nombre.c_str(), nombre.c_str());
    fprintf(f, "sed -i 's#<string name=\"app_name\">.*</string>#<string name=\"app_name\">%s</string>#' "
               "\"$AND/app/src/main/res/values/strings.xml\"\n", nombre.c_str());
    // 2) apuntar el build nativo al main.cpp GENERADO y las rutas absolutas del ecosistema
    fputs("cp \"$OUT/main.cpp\" \"$AND/jni/main_generado.cpp\"\n", f);
    // (el main del ejemplo vive en platform/main.cpp desde la limpieza de la
    // raiz de los proyectos: el sed reemplaza ESA ruta por el main generado)
    if (empaquetar)   // assets empaquetados: el pak.cpp generado entra al build junto al main
        fputs("cp \"$OUT/pak.cpp\" \"$AND/jni/pak_generado.cpp\"\n", f);
    // 2b) EL Android.mk DEL JUEGO SE GENERA ENTERO (antes se parcheaba con sed el de la
    //     plantilla del ejemplo, y esa lista de fuentes se desincronizo de la del CMake:
    //     le faltaban W3dRecursos/MallaDatos/TexturaCache y el APK no linkeaba). Sale de
    //     kFuentesBase/kFuentes3D/kIncludesBase/kIncludes3D, las MISMAS que el CMake de PC.
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
    if (empaquetar) fputs("  $(MY_PATH)/pak_generado.cpp \\\n", f);
    fputs("  $(EXAMP)/game/w3drun.cpp \\\n", f);
    fputs("  $(CORE)/gles2/w3dGraphicsGLES2.cpp \\\n", f);   // backend de shaders (Android)
    for (int i = 0; kFuentesBase[i]; i++) fprintf(f, "  %s \\\n", AMk(kFuentesBase[i]).c_str());
    if (usarFisica) fputs("  $(CORE)/physics/W3dFisica.cpp \\\n", f);
    if (usarSonido) fputs("  $(CORE)/audio/W3dAudio.cpp $(CORE)/audio/W3dAudioSDL.cpp \\\n", f);
    else            fputs("  $(CORE)/audio/W3dAudio.cpp \\\n", f);
    if (con3D)
        for (int i = 0; kFuentes3D[i]; i++) fprintf(f, "  %s \\\n", AMk(kFuentes3D[i]).c_str());
    fputs("  $(filter-out %/lua.c %/luac.c,$(wildcard $(W3D)/thirdparty/lua/src/*.c))\n", f);
    // ndk-build quiere LOCAL_SRC_FILES relativo a LOCAL_PATH (acepta '..')
    fputs("LOCAL_SRC_FILES := $(patsubst $(MY_PATH)/%,%,$(SRC_FILES))\n", f);
    fputs("LOCAL_C_INCLUDES := \\\n  $(MY_PATH)/shim \\\n  $(EXAMP)/game \\\n", f);
    for (int i = 0; kIncludesBase[i]; i++) fprintf(f, "  %s \\\n", AMk(kIncludesBase[i]).c_str());
    if (con3D)
        for (int i = 0; kIncludes3D[i]; i++) fprintf(f, "  %s \\\n", AMk(kIncludes3D[i]).c_str());
    fputs("  $(W3D)/thirdparty/lua/src \\\n  $(W3D)/thirdparty/SDL2/include\n", f);
    fputs("LOCAL_CPP_FEATURES := exceptions rtti\n", f);
    // lua (C): Android es linux -> posix. En armeabi-v7a el NDK 27 fuerza
    // -D_FILE_OFFSET_BITS=64 y fseeko/ftello recien existen en API 24 (aca es 21), asi
    // que liolib.c no compila: se fuerzan las macros de lua a fseek/ftell (long).
    fputs("LOCAL_CFLAGS   := -DLUA_USE_POSIX \\\n", f);
    fputs("  '-Dl_fseek(f,o,w)=fseek(f,o,w)' '-Dl_ftell(f)=ftell(f)' -Dl_seeknum=long\n", f);
    fprintf(f, "LOCAL_CPPFLAGS := -std=c++17 -DW3D_GLES2 -DW3D_GAME_RUNTIME -DW3D_GAME_NO_TEX "
               "-DW3D_STB_IMPL -DW3D_DEV_LOG=%d%s%s%s\n",
            modoDebug ? 1 : 0,
            usarSonido ? " -DW3D_ENABLE_AUDIO" : "",
            usarFisica ? "" : " -DW3D_SIN_FISICA",
            con3D ? " -DW3D_SIN_EDITOR -DW3D_JUEGO_3D" : "");
    fputs("LOCAL_SHARED_LIBRARIES := SDL2\nLOCAL_LDLIBS := -lGLESv2 -llog -landroid\n", f);
    fputs("include $(BUILD_SHARED_LIBRARY)\nW3DMK\n", f);
    // la raiz del ecosistema, con la ruta REAL (el heredoc va sin expandir a proposito)
    fputs("sed -i \"s#^ROOT  := .*#ROOT  := $ROOT#\" \"$AND/jni/Android.mk\"\n", f);
    // 2b'') ICONO del launcher: los android-<dpi>.png generados reemplazan los
    // ic_launcher (y _round) de la COPIA. Si la plantilla no trae los mipmap-* se
    // crean, y si el manifest no referencia android:icon se agrega por sed.
    if (conIcono) {
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
    // 2c) orientacion clavada: android:screenOrientation en la COPIA del manifest (la
    // referencia es la calculadora, que fija portrait en su manifest + hint del main)
    if (orientacion == 1 || orientacion == 2) {
        fprintf(f, "# orientacion: %s\n", (orientacion == 1) ? "solo vertical (portrait)"
                                                             : "solo horizontal (landscape)");
        fprintf(f, "sed -i 's#android:launchMode=\"singleTask\"#android:launchMode=\"singleTask\" "
                   "android:screenOrientation=\"%s\"#' \"$AND/app/src/main/AndroidManifest.xml\"\n",
                (orientacion == 1) ? "portrait" : "landscape");
    }
    if (empaquetar) {
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
        // proyecto.json = la ESCENA 3D del juego (un juego 2D simplemente no lo tiene)
        fputs("cp \"$OUT\"/*.json  \"$ASSETS\"/ 2>/dev/null || true\n", f);
        // LAS CARPETAS DE ASSETS (kCarpetasAssets: el espejo del contenedor +
        // assets/ y contenido/ de los proyectos v3) van CON su subdirectorio: el
        // loader de Android resuelve "texturas/pausa.png" tal cual contra el asset
        // root del APK. Antes solo se copiaban assets/ y contenido/, asi que un
        // proyecto v4 llegaba al celular sin sus texturas ni sus scripts.
        for (int c = 0; CarpAsset(c); c++)
            fprintf(f, "if [ -d \"$OUT/%s\" ]; then mkdir -p \"$ASSETS/%s\"; "
                       "cp -r \"$OUT/%s/.\" \"$ASSETS/%s/\"; fi\n",
                    CarpAsset(c), CarpAsset(c),
                    CarpAsset(c), CarpAsset(c));
    }
    // 4) compilar: ndk-build (libmain.so, TODAS las ABIs) -> jniLibs -> gradlew assembleDebug
    fputs("echo \"[W3D] 16 ndk-build ($ABIS)\"\n", f);
    // -j ACOTADO (antes era $(nproc), y encima por CADA ABI): ver W3dCompilarJobs
    fprintf(f, "\"$NDK/ndk-build\" %s APP_ABI=\"$ABIS\" NDK_PROJECT_PATH=\"$AND\" "
               "APP_BUILD_SCRIPT=\"$AND/jni/Android.mk\" NDK_APPLICATION_MK=\"$AND/jni/Application.mk\"\n",
            W3dCompilarJobsFlag().c_str());
    fputs("echo \"[W3D] 58 Copiando libmain.so (jniLibs)\"\n", f);
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
//  El TRABAJO del worker: todo por VALOR (strings propios). El worker no ve la
//  escena ni la UI: solo corre estos comandos y copia archivos.
struct BldTrabajo {
    int plataforma;          // 0 .deb / 1 AppImage / 2 web / 3 android
    std::string plat;        // "linux" / "web" / "android" (para los mensajes)
    std::string out;         // <juego>/platform-build/<plat> (ahi vive build.log)
    std::string cmdCfg;      // linux: cmake -S . -B out (configurar)
    std::string cmdBuild;    // linux: cmake --build | web: emcc | android: bash script
    std::string cmdCpack;    // linux .deb: cpack (vacio = no empaquetar)
    std::string cmdCopia;    // [4/4]: limpiar build/<plat> y copiar los artefactos
    std::string msgOk;       // "Juego compilado: ..." (lo arma el hilo principal)
};

// el HILO WORKER: [3/4] compilar + [4/4] copiar resultados. Corre los procesos con
// BldCorrer (que vuelca la salida a build.log y publica el avance) y termina
// seteando g_bldTermino: el Tick del hilo principal hace join + Notificar.
//  Porcentaje GLOBAL por plataforma (0..5 fue la fase sincrona de generacion):
//   linux: configurar 8..15, compilar 15..85 (el "[ NN%]" REAL de cmake --build),
//          cpack .deb 85..94 (creep), copiar 95..100
//   web:   emcc 10..85 (creep: emcc no da %; avanza lento por salida/tiempo), copiar 95..100
//   android: 10..90 con las marcas "[W3D] <pct> <etapa>" del script generado
//          (plantilla 12, ndk-build 16, jniLibs 58, gradle 60) + creep entre marcas
static void BldWorker(BldTrabajo t) {
    FILE* log = fopen((t.out + "/build.log").c_str(), "w");
    int rc;
    if (t.plataforma == 3) {
        BldPublicar("Preparando Android (plantilla)", 10);
        rc = BldCorrer(t.cmdBuild, log, BLD_MARCAS, 10, 90);
    } else if (t.plataforma == 2) {
        BldPublicar("Compilando (emcc)", 10);
        rc = BldCorrer(t.cmdBuild, log, BLD_CREEP, 10, 85);
    } else {
        BldPublicar("Configurando (cmake)", 8);
        rc = BldCorrer(t.cmdCfg, log, BLD_CREEP, 8, 15);
        if (rc == 0) {
            BldPublicar("Compilando (cmake)", 15);
            rc = BldCorrer(t.cmdBuild, log, BLD_CMAKE, 15, 85);
        }
        if (rc == 0 && !t.cmdCpack.empty()) {
            BldPublicar("Empaquetando .deb (cpack)", 85);
            // como siempre: si cpack falla el binario igual queda (no es fatal)
            BldCorrer(t.cmdCpack, log, BLD_CREEP, 85, 94);
        }
    }
    if (log) fclose(log);
    if (rc != 0) {
        { BLD_LOCK(); g_bldFinal = "Compilar: fallo la compilacion (ver platform-build/" +
                                   t.plat + "/build.log)"; }
        g_bldOk.store(false);
        g_bldTermino.store(true);   // SIEMPRE al final: es la senal para el Tick
        return;
    }
    // [4/4] SOLO filesystem: limpiar build/<plat> y copiar los artefactos finales
    BldPublicar("Copiando resultados", 95);
    if (system(t.cmdCopia.c_str()) != 0) {
        { BLD_LOCK(); g_bldFinal = "Compilar: no pude copiar el resultado a build/" + t.plat; }
        g_bldOk.store(false);
        g_bldTermino.store(true);
        return;
    }
    BldPublicar("Listo", 100);
    { BLD_LOCK(); g_bldFinal = t.msgOk; }
    g_bldOk.store(true);
    g_bldTermino.store(true);
}

// 1x por frame en el HILO PRINCIPAL (lo llama MainLoopFrame): mientras el build corre
// mantiene vivo el render (la barra avanza) y, cuando el worker termina, hace join y
// dispara el Notificar de exito/error (la UI se toca SOLO desde aca, nunca del worker).
void CompilarJuegoTick() {
    if (!g_bldActivo.load()) return;
    g_redraw = true;   // seguir dibujando: la barra (overlay) se actualiza cada frame
    if (!g_bldTermino.load()) return;
#ifndef __EMSCRIPTEN__
    if (g_bldHilo) {
        if (g_bldHilo->joinable()) g_bldHilo->join();
        delete g_bldHilo; g_bldHilo = NULL;
    }
#endif
    std::string msg; { BLD_LOCK(); msg = g_bldFinal; }
    bool ok = g_bldOk.load();
    g_bldTermino.store(false);
    g_bldActivo.store(false);   // recien aca se puede disparar otro build
    W3dDockProgresoFin();       // el overlay dejo de dibujarse: apagar la barra del icono (salga OK o con error)
    Notificar(msg, !ok);
    if (ok) w3dLogf("CompilarJuego OK: %s", msg.c_str());
    else    w3dLogfE("CompilarJuego: %s", msg.c_str());
}

// ============================================================================
//  ES UN JUEGO 3D? — decide si el binario lleva el pase 3D o no.
//
//  Un juego 3D es el que tiene ALGO que dibujar en 3D: una malla, una camara,
//  una luz, una curva/riel, un espejo, una coleccion, un LOD, un culling, una
//  zona de visibilidad, un emisor de particulas o una instancia. Un juego 2D
//  (una UI con scripts, como el whiskpaddle) no tiene nada de eso y se compila
//  como siempre: sin el lector de proyecto ni el pase 3D, o sea un binario mucho
//  mas chico. La deteccion es automatica -- el usuario no tiene que elegir nada.
// ============================================================================
static bool EsObjeto3D(Object* o) {
    if (!o) return false;
    switch (o->getType()) {
        case ObjectType::mesh:       case ObjectType::camera:
        case ObjectType::light:      case ObjectType::curve:
        case ObjectType::mirror:     case ObjectType::instance:
        case ObjectType::collection: case ObjectType::lod:
        case ObjectType::culling:    case ObjectType::viszona:
        case ObjectType::particulas: case ObjectType::armature:
            return true;
        default: return false;
    }
}
static bool HayTres3DRec(Object* o) {
    if (!o) return false;
    if (EsObjeto3D(o)) return true;
    for (size_t i = 0; i < o->Childrens.size(); i++)
        if (HayTres3DRec(o->Childrens[i])) return true;
    return false;
}
// OJO: la RAIZ no cuenta. SceneCollection es un Scene, y Scene::getType() devuelve
// 'collection' (es una coleccion, la de toda la escena): mirarla hacia que CUALQUIER
// proyecto -- hasta uno 2D puro -- se compilara como juego 3D.
static bool HayTres3D(Object* raiz) {
    if (!raiz) return false;
    for (size_t i = 0; i < raiz->Childrens.size(); i++)
        if (HayTres3DRec(raiz->Childrens[i])) return true;
    return false;
}

bool CompilarJuego(UI* u, int plataforma, int modoVentana, int orientacion,
                   bool usarFisica, bool usarSonido, bool modoDebug,
                   bool empaquetarAssets) {
    if (!u) return false;
    // ya hay un build corriendo: NO se dispara otro (el resto del editor sigue usable;
    // la barra de abajo muestra el que esta en curso)
    if (g_bldActivo.load()) {
        Notificar("Compilar: ya hay una compilacion en curso", true);
        return false;
    }
    // Windows (4) y Symbian (5): el pipeline de build de estos targets todavia NO esta
    // cableado a este boton (el flujo de abajo genera el runtime SDL/cmake de Linux/web/
    // android). Se hace aparte para no romper este camino. Mensaje claro en vez de fallar raro.
    if (plataforma == 5) {
        Notificar("Symbian .sisx: el target todavia no esta cableado a este boton (viene).", true);
        return false;
    }
    if (plataforma == 4) {
        Notificar("Windows .exe: todavia no se genera desde el boton (viene). El EDITOR se compila con platform/windows/build_windows.bat.", true);
        return false;
    }
    std::vector<ObjScript> objs;
    std::vector<std::string> rutasLua;
    if (SceneCollection) RecolectarScripts(SceneCollection, &objs, &rutasLua);
    if (objs.empty()) {
        Notificar("Compilar: el proyecto no tiene scripts (agregalos a un objeto Script)", true);
        return false;
    }
    std::string repo = RepoRoot();
    if (repo.empty()) {
        Notificar("Compilar: no encuentro el repo Whisk3D (fuentes del runtime). "
                  "Fija la raiz del repo en Ajustes (o compila desde el arbol de codigo).", true);
        return false;
    }
    // la carpeta del JUEGO: la del .w3d abierto. Sin proyecto guardado queda la
    // heuristica de siempre (la carpeta del primer .lua) -- que con la
    // estructura contenido/ ya no es la raiz, por eso manda el .w3d.
    // (rutasLua puede venir VACIO: scripts con ruta vacia; ultimo recurso ".")
    extern std::string w3dPath;   // el proyecto abierto (app/variables.h)
    std::string proy = !w3dPath.empty()   ? Carpeta(w3dPath)
                     : !rutasLua.empty()  ? Carpeta(rutasLua[0])
                                          : std::string(".");
    // ESTRUCTURA de salida en la carpeta del juego (las dos van al .gitignore):
    //   platform-build/<plat>/  TODO lo generado para compilar, autocontenido por
    //                           plataforma: main.cpp + CMakeLists/script + assets
    //                           copiados + build.log + el build intermedio (out/ para
    //                           cmake/emcc, proyecto/ para la copia Android). Cada
    //                           plataforma se puede borrar/regenerar por separado.
    //   build/<plat>/           SOLO los RESULTADOS: ejecutable+instaladores (linux),
    //                           .html/.js/.wasm (web), .apk (android). Se limpia y
    //                           rellena al final de cada compilacion.
    const char* plat = (plataforma == 3) ? "android" : (plataforma == 2) ? "web" : "linux";
    std::string out        = proy + "/platform-build/" + plat;
    std::string resultados = proy + "/build/" + plat;
    // nombre del JUEGO (proyecto CMake / binario / artefactos): el archivo .w3d ABIERTO
    // (whiskpaddle.w3d -> "whiskpaddle"), saneado a un identificador seguro. Antes se
    // usaba el nombre de la PRIMERA ESCENA ("Menu") y el .deb salia "whisk3d-Menu-...".
    // Sin .w3d guardado: la carpeta del juego; ultimo recurso "juego". Los NOMBRES de
    // escena (dentro de cada .w3dui) quedan como estan.
    std::string nombre;
    if (!w3dPath.empty()) { std::string b = BaseSinExt(w3dPath); if (!b.empty()) nombre = Slug(b); }
    if (nombre.empty())   { std::string b = Base(proy); if (!b.empty() && b != ".") nombre = Slug(b); }
    if (nombre.empty()) nombre = "juego";
    { char cmd[700]; snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", out.c_str()); if (system(cmd)) {} }
    AsegurarGitignore(proy);   // platform-build/ y build/ fuera del repo del juego

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
        //  lineas mas abajo: escenas .w3dui, scripts .lua, proyecto.json y los
        //  .png sueltos -- font.png/icono.png y los de un proyecto v3.)
        snprintf(cmd, sizeof(cmd),
                 "rm -f \"%s\"/*.w3dui \"%s\"/*.lua \"%s\"/*.json \"%s\"/*.png "
                 "\"%s\"/*.deb \"%s\"/*.AppImage "
                 "\"%s/out\"/*.deb \"%s/out\"/*.AppImage 2>/dev/null || true",
                 out.c_str(), out.c_str(), out.c_str(), out.c_str(),
                 out.c_str(), out.c_str(), out.c_str(), out.c_str());
        if (system(cmd)) {}
    }

    // MULTI-ESCENA vs UNA sola: las escenas son los UI raiz de la escena (tipo ui). Con 2+ el
    // juego cambia entre ellas con cambiarEscena() -> hay que exportar y cargar TODAS. Con 1 (o
    // un proyecto legacy con los scripts en un objeto Script) se mantiene el camino de siempre.
    std::vector<UI*> escenasUI;
    RecolectarEscenas(&escenasUI);
    bool multi = (escenasUI.size() >= 2);
    // JUEGO 3D: hay algo que dibujar en 3D -> el binario lleva el pase 3D y la escena
    // viaja como proyecto.json al lado (ver EsObjeto3D / kFuentes3D). Ver mas abajo.
    const bool con3D = HayTres3D(SceneCollection);

    // [1/4] exportar las escenas + copiar los scripts + las texturas + la tipografia
    // (fase SINCRONA y rapida en el hilo de UI: es lo unico que toca la escena. El
    // avance visible ahora lo cuenta la BARRA, no los toasts de antes.)
    w3dLogf("CompilarJuego [1/4]: exportando escenas y scripts");
    std::string w3duiLegacy = nombre + ".w3dui";       // solo el camino legacy (1 escena)
    std::vector<EscenaGen> escenas;
    if (multi) {
        // cada escena -> su .w3dui en platform-build/<plat>, con las rutas relativas calculadas contra la
        // carpeta del PROYECTO (proy): asi quedan PLANAS ("menu.lua", "parlante.png") y los assets
        // que copiamos al lado resuelven igual en PC (.deb/AppImage), APK y WebGL.
        std::vector<std::string> usados;              // slugs ya usados (evitar colisiones)
        for (size_t i = 0; i < escenasUI.size(); i++) {
            UI* e = escenasUI[i];
            std::string slug = Slug(e->name.empty() ? std::string("escena") : e->name);
            std::string cand = slug;
            for (int n = 2;; n++) {
                bool choca = false;
                for (size_t k = 0; k < usados.size(); k++) if (usados[k] == cand) { choca = true; break; }
                if (!choca) break;
                char suf[16]; snprintf(suf, sizeof(suf), "_%d", n); cand = slug + suf;
            }
            usados.push_back(cand);
            std::string arch = cand + ".w3dui";
            if (!UI2DGuardar(e, out + "/" + arch, proy)) {
                Notificar("Compilar: no pude escribir una escena (.w3dui)", true); return false;
            }
            EscenaGen g; g.archivo = arch; g.nombre = e->name; escenas.push_back(g);
        }
    } else {
        // UNA sola escena / legacy: exportar la UI activa. base = carpeta del proyecto (proy)
        // para que texturas/fuente queden relativas planas (igual que multi) y resuelvan al lado.
        if (!UI2DGuardar(u, out + "/" + w3duiLegacy, proy)) {
            Notificar("Compilar: no pude escribir el .w3dui", true); return false;
        }
    }
    // los scripts .lua (de TODAS las escenas) al lado, planos
    for (size_t i = 0; i < rutasLua.size(); i++) {
        if (W3dEsNombreDeEntrada(rutasLua[i])) continue;   // sale del contenedor (mas abajo)
        char cmd[1400];
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s/%s\"", rutasLua[i].c_str(), out.c_str(), Base(rutasLua[i]).c_str());
        if (system(cmd)) {}
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
    //  referencian ("texturas/pausa.png"), asi que no hay ninguna traduccion de
    //  rutas en el medio. De ahi salen los cuatro targets (copia del CMake, cp del
    //  script de Android, --embed-file de emcc, o el pak), todos por kCarpetasAssets.
    //
    //  UNA pasada, no cinco: antes ademas del volcado se copiaban a mano los *.png
    //  de la carpeta del proyecto, la subcarpeta assets/, y despues se RE-PARSEABAN
    //  los .w3dui recien escritos con el colector del guardado por versiones para
    //  replicar contenido/. Cuatro fuentes distintas para el mismo archivo, cada
    //  una con su propia idea de que ruta es valida. Con el .w3d empaquetado la
    //  fuente es UNA: el contenedor.
    //
    //  Se saltean: las entradas de SERVICIO (mimetype, LEEME.txt, proyecto.json,
    //  EXTERNOS.txt), que describen el contenedor y no son assets; y escenas/,
    //  porque las escenas las RE-EXPORTA el paso de arriba a la raiz del staging
    //  con el nombre que el main.cpp generado va a cargar (volcarlas ademas dejaria
    //  dos copias de cada escena, y la del zip no la abre nadie).
    // ==================================================================
    bool desdeContenedor = W3dContenedorHayMontado();
    if (desdeContenedor) {
        W3dZipLector* lec = W3dContenedorLector();
        std::vector<std::string> ents;
        if (lec) lec->Listar(ents);
        // se limpian las carpetas de assets viejas ANTES de volcar: un asset que el
        // usuario borro del proyecto no puede sobrevivir en la compilacion anterior
        for (int c = 0; kCarpetasAssets[c]; c++) {
            char cmd[1400];
            snprintf(cmd, sizeof(cmd), "rm -rf \"%s/%s\"", out.c_str(), kCarpetasAssets[c]);
            if (system(cmd)) {}
        }
        // y la escenas/ que pudo haber dejado una compilacion anterior (ya no se vuelca)
        if (!con3D) { char cmd[1400]; snprintf(cmd, sizeof(cmd), "rm -rf \"%s/escenas\"", out.c_str()); if (system(cmd)) {} }
        int volcados = 0;
        for (size_t i = 0; i < ents.size(); i++) {
            // proyecto.json ES la ESCENA 3D del juego: en un juego 3D viaja como un asset
            // mas (lo lee W3dGameCargarEscena3D). En un juego 2D sigue siendo servicio.
            if (W3dEsEntradaDeServicio(ents[i]) && !(con3D && ents[i] == "proyecto.json")) continue;
            // escenas/: en 2D no se vuelcan (el compilador las re-exporta planas a la raiz);
            // en 3D SI se vuelcan y ademas el compilador las pisa con la version VIVA, porque
            // el proyecto.json las referencia por ese nombre de entrada.
            if (!con3D && ents[i].compare(0, 8, "escenas/") == 0) continue;
            std::vector<unsigned char> datos;
            if (!lec->Leer(ents[i], datos)) continue;
            std::string dst = out + "/" + ents[i];
            { char cmd[2000]; snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", Carpeta(dst).c_str());
              if (system(cmd)) {} }
            FILE* fh = fopen(dst.c_str(), "wb");
            if (!fh) continue;
            if (!datos.empty()) fwrite(&datos[0], 1, datos.size(), fh);
            if (fclose(fh) == 0) volcados++;
        }
        w3dLogf("CompilarJuego: %d entrada(s) del .w3d volcadas al staging (UNA pasada)", volcados);
    } else {
        // ------------------------------------------------------------------
        //  PROYECTO v3 (JSON plano con los assets SUELTOS al lado del .w3d): no
        //  hay contenedor del que volcar, asi que sigue el camino de siempre.
        //  Se abre y se MIGRA al primer guardado; hasta entonces compila igual.
        // ------------------------------------------------------------------
        // los .png de la carpeta del proyecto, planos (los .w3dui los referencian por nombre)
        { char cmd[1400]; snprintf(cmd, sizeof(cmd), "cp \"%s\"/*.png \"%s\"/ 2>/dev/null || true", proy.c_str(), out.c_str()); if (system(cmd)) {} }
        // la subcarpeta assets/ entera, CON su carpeta ("assets/xxx.png")
        { char cmd[2600]; snprintf(cmd, sizeof(cmd),
            "rm -rf \"%s/assets\" && if [ -d \"%s/assets\" ]; then cp -r \"%s/assets\" \"%s/\"; fi",
            out.c_str(), proy.c_str(), proy.c_str(), out.c_str()); if (system(cmd)) {} }
        // y las rutas REALES que las escenas recien exportadas referencian (contenido/...)
        { char cmd[1400]; snprintf(cmd, sizeof(cmd), "rm -rf \"%s/contenido\"", out.c_str()); if (system(cmd)) {} }
        std::set<std::string> refs;
        if (multi)
            for (size_t i = 0; i < escenas.size(); i++)
                GuardarVersionColectarDe(out + "/" + escenas[i].archivo, "", proy, &refs);
        else
            GuardarVersionColectarDe(out + "/" + w3duiLegacy, "", proy, &refs);
        for (std::set<std::string>::iterator it = refs.begin(); it != refs.end(); ++it) {
            char cmd[3000];
            snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/%s\" && cp \"%s/%s\" \"%s/%s\"",
                     out.c_str(), Carpeta(*it).c_str(), proy.c_str(), it->c_str(),
                     out.c_str(), it->c_str());
            if (system(cmd)) {}
        }
    }
    // ------------------------------------------------------------------
    //  JUEGO 3D: la UI VIVA gana sobre la del contenedor.
    //  El proyecto.json referencia cada escena por su NOMBRE DE ENTRADA
    //  ("escenas/hud.w3dui"), y el volcado de arriba acaba de dejar ahi la
    //  version del ULTIMO GUARDADO. Aca se pisa con la que el editor tiene EN
    //  PANTALLA (la misma que se exporto plana a la raiz), asi el juego sale con
    //  los cambios de HUD sin guardar, igual que el Play.
    // ------------------------------------------------------------------
    if (con3D) {
        for (size_t i = 0; i < escenasUI.size(); i++) {
            UI* e = escenasUI[i];
            if (!e || e->archivoW3dui.empty()) continue;
            const std::string& rel = e->archivoW3dui;
            // v4: nombre de ENTRADA ("escenas/hud.w3dui"). v3/texto: ruta RELATIVA a
            // la carpeta del proyecto ("hud.w3dui") -- ese caso no se re-exportaba y
            // el .w3dui NI SIQUIERA VIAJABA: el juego salia sin HUD (el proyecto lo
            // nombra por esa ruta y al lado del binario no habia ningun archivo asi).
            // Se descarta solo lo que no se puede colocar: absoluta o fuera de la carpeta.
            if (!W3dEsNombreDeEntrada(rel) &&
                (rel[0] == '/' || (rel.size() > 2 && rel[1] == ':') ||
                 rel.find("..") != std::string::npos)) continue;
            char cmd[1400];
            snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", Carpeta(out + "/" + rel).c_str());
            if (system(cmd)) {}
            UI2DGuardar(e, out + "/" + rel, proy);
        }
        // y el PROYECTO.JSON de un .w3d v3 (json plano, sin contenedor): es el .w3d mismo
        if (!desdeContenedor && !w3dPath.empty()) {
            char cmd[1400];
            snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s/proyecto.json\"", w3dPath.c_str(), out.c_str());
            if (system(cmd)) {}
        }
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
        W3dRefExternasListar(&ext);
        int copiadas = 0, afuera = 0;
        const std::string base = proy + "/";
        for (size_t i = 0; i < ext.size(); i++) {
            if (ext[i].compare(0, base.size(), base) != 0) { afuera++; continue; }
            const std::string rel = ext[i].substr(base.size());
            if (rel.empty() || rel.find("..") != std::string::npos) { afuera++; continue; }
            char cmd[3000];
            snprintf(cmd, sizeof(cmd), "mkdir -p \"%s/%s\" && cp \"%s\" \"%s/%s\"",
                     out.c_str(), Carpeta(rel).c_str(), ext[i].c_str(), out.c_str(), rel.c_str());
            if (system(cmd) == 0) copiadas++;
        }
        if (!ext.empty())
            w3dLogf("CompilarJuego: %d referencia(s) externa(s) copiadas al staging (%d afuera "
                    "de la carpeta del proyecto, no viajan)", copiadas, afuera);
        if (afuera > 0)
            Notificar("Compilar: hay referencias externas fuera de la carpeta del proyecto; "
                      "no viajan con el juego", true);
    }
    { char cmd[1200]; snprintf(cmd, sizeof(cmd), "cp \"%s/res/Skins/Whisk3D/font.png\" \"%s/font.png\"", repo.c_str(), out.c_str()); if (system(cmd)) {} }
    // ...y lo que SOLO nombra el lua por ruta relativa (sonidos/, musica/): ver
    // CopiarRefsRelativasDeLua. Va DESPUES de todo lo demas (no pisa nada de lo que
    // ya viajo) y ANTES de descubrir las carpetas y de hornear el bytecode -- con
    // los .lua ya convertidos a bytecode no habria literales que leer.
    CopiarRefsRelativasDeLua(out, proy);
    // el staging ya esta poblado: recien ahora se sabe QUE carpetas de assets tiene
    // (las fijas + las que trajeron las refs externas). Los cuatro targets usan esta lista.
    CarpAssetsDescubrir(out);

    // PRODUCCION ("Modo debug" destildado): los .lua recien COPIADOS al staging se
    // reemplazan por bytecode stripped con el mismo nombre (ver CompilarLuasStaging).
    // Corre antes del pak/embed/copia -> aplica a los 4 targets. Un error de sintaxis
    // ABORTA el build aca, con el mensaje de lua (archivo:linea) en la notificacion.
    if (!modoDebug) {
        std::string errLua; unsigned nLua = 0; size_t bFuente = 0, bByte = 0;
        if (!CompilarLuasStaging(out, &errLua, &nLua, &bFuente, &bByte)) {
            Notificar("Compilar: error en un script lua: " + errLua, true);
            w3dLogfE("CompilarJuego: bytecode fallo: %s", errLua.c_str());
            return false;
        }
        w3dLogf("CompilarJuego: %u script(s) lua a bytecode stripped (%u -> %u bytes)",
                nLua, (unsigned)bFuente, (unsigned)bByte);
    }

    // ICONO del juego (opcional, tarjeta Juego): del PNG del proyecto (g_proyIcono,
    // maxima definicion) se generan ACA los tamanos chicos (ver GenerarIconos). Se
    // limpia SIEMPRE lo viejo primero: si el proyecto ya no tiene icono, no deben
    // quedar los de la compilacion anterior. Sin icono: todo sigue como antes.
    std::vector<int> iconosLinux;
    bool conIcono = false;
    { char cmd[1500]; snprintf(cmd, sizeof(cmd), "rm -rf \"%s/icons\" \"%s/icono.png\"",
                               out.c_str(), out.c_str()); if (system(cmd)) {} }
    if (!g_proyIcono.empty()) {
        std::string abs = g_proyIcono;
        // v4: el icono es una ENTRADA del contenedor ("proyecto/icono.png") y se lee
        // TAL CUAL (w3dFileSystem la resuelve por el montaje). Colgarla de la carpeta
        // del .w3d daria una ruta de disco que no existe y el juego saldria sin icono.
        if (!W3dEsNombreDeEntrada(abs) &&
            abs[0] != '/' && !(abs.size() > 2 && abs[1] == ':')) {
            // relativa al .w3d (asi se guarda); sin .w3d, relativa a la carpeta del juego
            std::string baseDir = w3dPath.empty() ? proy : Carpeta(w3dPath);
            abs = baseDir + "/" + abs;
        }
        conIcono = GenerarIconos(abs, out, &iconosLinux);
        if (!conIcono)
            Notificar("Compilar: no pude leer el icono (" + Base(g_proyIcono) +
                      "); sigo sin icono", true);
    }

    // la escena INICIAL (multi): la del proyecto (escenaInicial del .w3d) si es una escena valida,
    // sino la primera. El runtime la elige por NOMBRE (el campo "nombre" de su .w3dui).
    std::string inicial;
    if (multi) {
        inicial = W3dEscenaInicial();
        bool valida = false;
        for (size_t i = 0; i < escenas.size(); i++) if (escenas[i].nombre == inicial) { valida = true; break; }
        if (!valida) inicial = escenas.empty() ? std::string() : escenas[0].nombre;
    }

    // [2/4] generar el codigo del runtime (main.cpp + el build de la plataforma)
    w3dLogf("CompilarJuego [2/4]: generando el codigo del juego (assets %s)",
            empaquetarAssets ? "empaquetados" : "sueltos");
    // ASSETS EMPAQUETADOS: con todos los archivos del juego ya juntados en
    // platform-build/<plat> (escenas + lua + png + assets/ + icono), se hornea el
    // pak.cpp (fase sincrona, hilo de UI). En sueltos se borra un pak.cpp viejo
    // para que no quede colgado de una compilacion empaquetada anterior.
    if (empaquetarAssets) {
        std::string errPak;
        if (!EscribirPak(out, nombre, &errPak)) {
            Notificar("Compilar: no pude empaquetar los assets (" + errPak + ")", true);
            return false;
        }
    } else {
        remove((out + "/pak.cpp").c_str());
    }
    // tamano de la ventana para el modo "Sin bordes": el lienzo de la UI cuando es
    // responsive; si la UI usa el tamano del render, se mantiene el 600x600 de siempre
    int winW = 600, winH = 600;
    if (!u->igualQueRender) {
        winW = (int)(u->ancho + 0.5f); winH = (int)(u->alto + 0.5f);
        if (winW < 1) winW = 1;
        if (winH < 1) winH = 1;
    }
    if (!EscribirMain(out + "/main.cpp", "font.png", multi, w3duiLegacy, objs, escenas, inicial,
                      modoVentana, winW, winH, orientacion, usarSonido, nombre, conIcono,
                      empaquetarAssets, con3D)) {
        Notificar("Compilar: no pude generar main.cpp", true); return false;
    }
    // CMakeLists para PC (Linux): GENERADO (copia/instala TODAS las escenas + scripts + texturas
    // cuando van sueltos; empaquetado compila pak.cpp y no copia/instala nada del juego)
    if (!EscribirCMake(out + "/CMakeLists.txt", nombre, usarFisica, usarSonido, modoDebug,
                       iconosLinux, empaquetarAssets, con3D)) {
        Notificar("Compilar: no pude generar CMakeLists.txt", true); return false;
    }

    // [3/4] compilar para la plataforma elegida — ASINCRONICO: de aca en adelante
    // NADA toca la escena. El hilo de UI solo ARMA los comandos (strings propios)
    // y lanza el worker, que los corre + copia los resultados publicando etapa y
    // porcentaje para la barra (overlay) que dibuja el hilo principal cada frame.
    const char* nomPlat = (plataforma == 3) ? "Android" : (plataforma == 2) ? "WebGL"
                        : (plataforma == 1) ? "Linux AppImage" : "Linux .deb";
    w3dLogf("CompilarJuego [3/4]: compilando (%s) en segundo plano", nomPlat);
    BldTrabajo t;
    t.plataforma = plataforma;
    t.plat = plat;
    t.out = out;
    if (plataforma == 3) {
        // ANDROID: genera el script que arma el APK reusando la plantilla del ejemplo
        // (ndk-build + gradlew), adaptada a ESTE juego. Ver EscribirBuildAndroid arriba.
        // El script emite marcas "[W3D] <pct> <etapa>" que el worker parsea para la barra.
        std::string sh = out + "/build_android.sh";
        if (!EscribirBuildAndroid(sh, repo, out, nombre, orientacion, usarFisica, usarSonido,
                                  modoDebug, conIcono, empaquetarAssets, con3D)) {
            Notificar("Compilar: no pude generar el script de Android", true); return false;
        }
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "bash \"%s\"", sh.c_str());
        t.cmdBuild = cmd;
    } else if (plataforma == 2) {
        // WebGL: MISMO set de fuentes que el build_web.sh del ejemplo (incluye W3dEscena.cpp para
        // multi-escena y Textura2D.cpp para las imagenes) y EMBEBE por glob TODAS las escenas
        // (.w3dui) + scripts (.lua) + texturas/fuente (.png) que quedaron en platform-build/web,
        // mas la subcarpeta assets/ entera (si existe) con su carpeta (rutas "assets/xxx.png").
        // La salida cruda de emcc va a out/ (despues se copia lo servible a build/web).
        // Los checkboxes de la tarjeta arman las piezas OPCIONALES de la linea emcc:
        // sin fisica no viaja W3dFisica.cpp (+W3D_SIN_FISICA); sin sonido no viaja el
        // backend SDL ni W3D_ENABLE_AUDIO (W3dAudio.cpp queda: es el dispatcher stub).
        // "Modo debug" define W3D_DEV_LOG=1/0 (1 = log + ring + depurar(); 0 = produccion).
        // LAS MISMAS LISTAS que el CMake de PC y el Android.mk (kFuentesBase/kFuentes3D/
        // kIncludes*): una sola fuente de verdad para las tres plataformas.
        std::string baseSrcW, tresSrcW, incW;
        for (int i = 0; kFuentesBase[i]; i++) baseSrcW += AEmcc(kFuentesBase[i]) + " ";
        if (con3D) for (int i = 0; kFuentes3D[i]; i++) tresSrcW += AEmcc(kFuentes3D[i]) + " ";
        for (int i = 0; kIncludesBase[i]; i++) incW += "-I" + AEmcc(kIncludesBase[i]) + " ";
        if (con3D) for (int i = 0; kIncludes3D[i]; i++) incW += "-I" + AEmcc(kIncludes3D[i]) + " ";
        const char* fisSrc = usarFisica ? "$CORE/physics/W3dFisica.cpp " : "";
        const char* audSrc = usarSonido ? "$CORE/audio/W3dAudio.cpp $CORE/audio/W3dAudioSDL.cpp "
                                        : "$CORE/audio/W3dAudio.cpp ";
        const char* defsOpc = usarSonido ? (usarFisica ? "-DW3D_ENABLE_AUDIO"
                                                       : "-DW3D_ENABLE_AUDIO -DW3D_SIN_FISICA")
                                         : (usarFisica ? "" : "-DW3D_SIN_FISICA");
        // assets EMPAQUETADOS: viaja pak.cpp (el pak entra al wasm por el array) y NO
        // se embebe ningun archivo; sueltos: --embed-file por glob, como siempre.
        const char* pakSrc = empaquetarAssets ? "pak.cpp " : "";
        // los --embed-file: los generados de la raiz (.w3dui/.lua/.png) mas TODAS las
        // carpetas de assets del staging (kCarpetasAssets: el espejo del contenedor +
        // las dos legacy). La lista se arma aca porque es la MISMA de los otros tres
        // targets: si una carpeta nueva aparece, aparece en los cuatro a la vez.
        std::string embedsStr;
        if (!empaquetarAssets) {
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
            "cd \"%s\" && source ~/emsdk/emsdk_env.sh >/dev/null 2>&1; "
            // (las listas de fuentes/includes son LAS MISMAS de PC y Android: ver kFuentesBase)
            // emcc reparte las ~75 unidades entre EMCC_CORES procesos y por default agarra
            // TODOS los nucleos: misma cota que el resto (ver W3dCompilarJobs)
            "export EMCC_CORES=%d; "
            "W3D=\"%s\"; GAME=\"$W3D/../Whisk3D-Examples/game\"; "
            "CORE=$W3D/libs/Whisk3DCore; UILIB=$W3D/libs/WhiskUI; LUA=$W3D/thirdparty/lua/src; "
            "LUASRC=$(ls $LUA/*.c | grep -vE '/lua\\.c|/luac\\.c'); "
            "emcc main.cpp %s$GAME/w3drun.cpp "
            "$CORE/gles2/w3dGraphicsGLES2.cpp "   // backend de shaders (WebGL)
            "%s"   // kFuentesBase: LA MISMA lista que el CMake de PC y el Android.mk
            "%s"   // fisica del Core (W3dFisica.cpp) solo con el checkbox tildado
            "%s"   // audio: W3dAudio.cpp siempre (stub sin flag); el backend SDL es opcional
            "%s"   // kFuentes3D: el lector de proyecto + el pase 3D (solo si el juego es 3D)
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
            out.c_str(), W3dCompilarJobs(), repo.c_str(), pakSrc,
            baseSrcW.c_str(), fisSrc, audSrc, tresSrcW.c_str(),
            modoDebug ? 1 : 0, defsOpc,
            con3D ? "-DW3D_SIN_EDITOR -DW3D_JUEGO_3D" : "",
            incW.c_str(), embeds, nombre.c_str());
        // (ya no redirige a build.log el mismo: el worker captura stdout+stderr y
        // escribe el MISMO build.log de siempre, leyendolo en vivo para la barra)
        { char mk[700]; snprintf(mk, sizeof(mk), "mkdir -p \"%s/out\"", out.c_str()); if (system(mk)) {} }
        t.cmdBuild = cmd;
    } else {
        // PC (Linux): cmake (configurar) + cmake --build; luego cpack -G DEB (plataforma 0).
        // Van como comandos SEPARADOS: el worker parsea el "[ NN%]" del --build para la barra.
        char cfg[1200], bld[900];
        snprintf(cfg, sizeof(cfg), "cd \"%s\" && cmake -S . -B out -DW3DROOT=\"%s\"",
                 out.c_str(), repo.c_str());
        // -j ACOTADO (nunca pelado): ver W3dCompilarJobs en CompilarJuego.h
        snprintf(bld, sizeof(bld), "cd \"%s\" && cmake --build out %s",
                 out.c_str(), W3dCompilarJobsFlag().c_str());
        t.cmdCfg = cfg;
        t.cmdBuild = bld;
        if (plataforma == 0) {   // .deb: empaquetar con cpack (si falla no es fatal, como siempre)
            char deb[900];
            snprintf(deb, sizeof(deb), "cd \"%s/out\" && cpack -G DEB", out.c_str());
            t.cmdCpack = deb;
        }
    }

    // [4/4] la copia de RESULTADOS a build/<plat> (se limpia antes: quedan SOLO los
    // resultados de la ultima compilacion). El comando se ARMA aca pero lo corre el
    // worker al final (es solo filesystem). Lo intermedio queda en platform-build/<plat>.
    {
        // holgado a proposito: la copia de Linux lleva UN cp por carpeta de assets
        // (kCarpetasAssets) y cada uno nombra dos rutas absolutas
        char cmd[9000];
        if (plataforma == 3) {
            // ANDROID: el .apk (assembleDebug) como <juego>-android.apk (fat: sin
            // arch unica en el nombre), coherente con <juego>-linux-<arch>.deb.
            // El nombre y la ruta ESTANDAR los garantiza EscribirBuildAndroid (sed
            // sobre el gradle de la COPIA: sin buildDir redirigido ni rename propio).
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && "
                "cp \"%s/androidprj/app/build/outputs/apk/debug/%s-android.apk\" \"%s/%s-android.apk\"",
                resultados.c_str(), resultados.c_str(), out.c_str(), nombre.c_str(),
                resultados.c_str(), nombre.c_str());
        } else if (plataforma == 2) {
            // WEB: el .html + .js + .wasm de emcc, listos para servir
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s\".* \"%s/\"",
                resultados.c_str(), resultados.c_str(), out.c_str(), nombre.c_str(),
                resultados.c_str());
        } else if (empaquetarAssets) {
            // LINUX EMPAQUETADO: solo el ejecutable (los assets viajan adentro) + los
            // instaladores que haya. Nada de archivos sueltos del juego al lado.
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s\" \"%s/\" && "
                "(cp \"%s/out\"/*.deb \"%s/out\"/*.AppImage \"%s/\" 2>/dev/null || true)",
                resultados.c_str(), resultados.c_str(), out.c_str(), nombre.c_str(),
                resultados.c_str(),
                out.c_str(), out.c_str(), resultados.c_str());
        } else {
            // LINUX: el ejecutable + los assets al lado (para correrlo directo) + los
            // instaladores que haya (.deb del cpack; .AppImage si algun dia se genera).
            // Las CARPETAS de assets (kCarpetasAssets: el espejo del contenedor +
            // assets/ y contenido/) van ENTERAS con su carpeta: el juego las
            // referencia asi ("texturas/pausa.png") y las resuelve contra su
            // directorio actual. Antes se copiaban solo assets/ y contenido/, o sea
            // que el juego de un proyecto v4 quedaba en build/ sin sus texturas.
            std::string copiaDirs;
            for (int c = 0; CarpAsset(c); c++) {
                copiaDirs += "(cp -r \"" + out + "/" + CarpAsset(c) + "\" \"" +
                             resultados + "/\" 2>/dev/null || true) && ";
            }
            snprintf(cmd, sizeof(cmd),
                "rm -rf \"%s\" && mkdir -p \"%s\" && cp \"%s/out/%s\" \"%s/\" && "
                "(cp \"%s\"/*.w3dui \"%s\"/*.lua \"%s\"/*.png \"%s/\" 2>/dev/null || true) && "
                "(cp \"%s\"/*.json \"%s/\" 2>/dev/null || true) && "
                "%s"
                "(cp \"%s/out\"/*.deb \"%s/out\"/*.AppImage \"%s/\" 2>/dev/null || true)",
                resultados.c_str(), resultados.c_str(), out.c_str(), nombre.c_str(),
                resultados.c_str(),
                out.c_str(), out.c_str(), out.c_str(), resultados.c_str(),
                out.c_str(), resultados.c_str(),
                copiaDirs.c_str(),
                out.c_str(), out.c_str(), resultados.c_str());
        }
        t.cmdCopia = cmd;
    }
    std::string donde = (plataforma == 3) ? (nombre + " -> build/android/" + nombre + "-android.apk")
                      : (plataforma == 2) ? (nombre + " -> build/web/" + nombre + ".html")
                                          : (nombre + " -> build/linux/");
    t.msgOk = "Juego compilado: " + donde;

    // estado inicial de la barra + LANZAR el worker. La UI vuelve al loop enseguida:
    // el editor sigue usable y la barra (overlay abajo) muestra etapa + porcentaje.
    g_bldPct.store(5);   // 0..5 = la fase sincrona de arriba (exportar + generar)
    { BLD_LOCK(); g_bldEtapa = std::string("Compilando (") + nomPlat + ")"; g_bldFinal.clear(); }
    g_bldOk.store(false);
    g_bldTermino.store(false);
    g_bldActivo.store(true);
    g_redraw = true;     // que la barra aparezca ya mismo
#ifndef __EMSCRIPTEN__
    g_bldHilo = new std::thread(BldWorker, t);
#else
    // editor web: sin hilos (ver arriba). Corre sincrono y falla claro: popen/system
    // no existen en el sandbox del browser (igual que antes de este cambio).
    BldWorker(t);
#endif
    return true;   // true = el build ARRANCO (el resultado llega por notificacion)
}
