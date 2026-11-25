/**
 * @file W3dProfile.cpp
 * @brief Reloj de pared + acumulador de tiempos por categoria del frame.
 *
 * Definiciones de lo declarado en W3dProfile.h. Se compila en TODAS las plataformas
 * (el reloj cambia segun el OS). Lo consume el overlay "Statistics" del Viewport3D.
 */
#include "W3dProfile.h"
#include "w3dGraphics.h"   // StatsFrame: el contador de llamadas GL cierra el cuadro aca
#include "w3dlog.h"        // w3dLogf (el log del perfil)
#include <stdlib.h>        // getenv

#ifdef W3D_SYMBIAN
#include <e32std.h> // User::NTickCount
#else
#include <SDL2/SDL.h> // SDL_GetPerformanceCounter/Frequency (reloj de alta resolucion)
#endif

// Se ACUMULA durante el frame en curso; se resetea al empezar cada frame.
W3dProf g_prof = {0,0,0,0,0};
// Promedio EXPONENCIAL suavizado -> lo que dibuja el overlay (numeros estables).
W3dProf g_profShow = {0,0,0,0,0};

double W3dNowMs() {
#ifndef W3D_SYMBIAN
    return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency(); // reloj de PARED (alta res)
#else
    return (double)User::NTickCount(); // N95: nanokernel tick ~= ms (mismo reloj de pared que LayoutTickFPS)
#endif
}

// el reloj de relojMs() en lua (script/BindsGame.cpp: sin el perfilador, como en un juego compilado, usa clock())
extern double (*g_w3dClockMsHook)();
namespace { struct RegisterClockMs { RegisterClockMs() { g_w3dClockMsHook = W3dNowMs; } } gRegisterClockMs; }

void W3dProfBegin() {
    g_prof.logic = g_prof.scene = g_prof.viewport3d = g_prof.render = g_prof.swap = 0.0;
    w3dEngine::StatsFrame();   // (las llamadas GL del cuadro anterior, completo: escena + HUD + editor)
}

void W3dProfEnd() {
    g_profShow.logic      = g_profShow.logic      * 0.9 + g_prof.logic      * 0.1;
    g_profShow.scene      = g_profShow.scene      * 0.9 + g_prof.scene      * 0.1;
    g_profShow.viewport3d = g_profShow.viewport3d * 0.9 + g_prof.viewport3d * 0.1;
    g_profShow.render     = g_profShow.render     * 0.9 + g_prof.render     * 0.1;
    g_profShow.swap       = g_profShow.swap       * 0.9 + g_prof.swap       * 0.1;
    if (W3dProfLogOn()) W3dProfLog();
}

// EL LOG DEL PERFIL (W3D_PROF_LOG=1): para medir donde el overlay no se puede leer (el telefono, por adb logcat). Con eso
// el editor dibuja todos los cuadros (main.cpp) y cada 2 segundos dice cuantos dibujo y el promedio de cada parte, con
// los draws y los triangulos del ultimo cuadro
bool W3dProfLogOn() {
    static int on = -1;
#ifdef W3D_SYMBIAN
    on = 0;   // (el N95 tiene su propio overlay de fps)
#else
    if (on < 0) { const char* e = getenv("W3D_PROF_LOG"); on = (e && *e && *e != '0') ? 1 : 0; }
#endif
    return on == 1;
}

#if defined(__ANDROID__) && defined(__arm__)
// EL PERFIL POR MUESTRAS (W3D_PERFIL=<segundos>, Android ARM; el mismo de los juegos en modo debug): desde el cuadro 60,
// SIGPROF cada 1 ms de CPU anota donde estaba el programa, y al terminar el log dice los pedazos de 64 bytes de cada
// biblioteca con mas muestras (con el libmain.so sin recortar de build/obj y addr2line sale la funcion de cada uno)
#include <signal.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <asm/sigcontext.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
namespace {
const int kPerfMax = 32768;
unsigned long* gPerfPc = 0;   // (se reserva recien con W3D_PERFIL: 128 KB que el resto de las veces no hacen falta)
volatile int gPerfN = 0;
struct PerfUc { unsigned long fl; void* link; struct { void* sp; int fl; size_t size; } stack; struct sigcontext mc; };
void PerfSignal(int, siginfo_t*, void* uc) {
    const int n = gPerfN;
    if (gPerfPc && n < kPerfMax) { gPerfPc[n] = ((PerfUc*)uc)->mc.arm_pc; gPerfN = n + 1; }
}
}
static void W3dProfSamples() {
    static int frame = 0, state = 0;
    static double until = 0.0;
    if (state == 2) return;
    const char* e = getenv("W3D_PERFIL");
    if (!e || !*e) { state = 2; return; }
    if (state == 0) {
        if (++frame < 60) return;
        gPerfPc = (unsigned long*)malloc(sizeof(unsigned long) * kPerfMax);
        if (!gPerfPc) { state = 2; return; }
        struct sigaction sa; memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = PerfSignal; sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigaction(SIGPROF, &sa, 0);
        struct itimerval it; it.it_interval.tv_sec = 0; it.it_interval.tv_usec = 1000; it.it_value = it.it_interval;
        setitimer(ITIMER_PROF, &it, 0);
        until = W3dNowMs() + 1000.0 * atoi(e); state = 1;
        w3dLogf("[muestras] empieza (%s s)", e);
        return;
    }
    if (W3dNowMs() < until) return;
    struct itimerval no; memset(&no, 0, sizeof(no)); setitimer(ITIMER_PROF, &no, 0);
    state = 2;
    std::map<std::string, int> libs;
    std::map<std::pair<std::string, unsigned long>, int> blocks;
    const int n = gPerfN;
    for (int i = 0; i < n; i++) {
        Dl_info di; memset(&di, 0, sizeof(di));
        if (!dladdr((void*)gPerfPc[i], &di) || !di.dli_fname) { libs["?"]++; continue; }
        const char* b = strrchr(di.dli_fname, '/');
        const std::string lib = b ? b + 1 : di.dli_fname;
        libs[lib]++;
        blocks[std::make_pair(lib, (gPerfPc[i] - (unsigned long)di.dli_fbase) & ~63ul)]++;
    }
    w3dLogf("[muestras] %d", n);
    for (std::map<std::string, int>::iterator it = libs.begin(); it != libs.end(); ++it)
        w3dLogf("[muestras] lib %s %d", it->first.c_str(), it->second);
    std::vector<std::pair<int, std::pair<std::string, unsigned long> > > top;
    for (std::map<std::pair<std::string, unsigned long>, int>::iterator it = blocks.begin(); it != blocks.end(); ++it)
        top.push_back(std::make_pair(it->second, it->first));
    std::sort(top.rbegin(), top.rend());
    for (size_t i = 0; i < top.size() && i < 80; i++)
        w3dLogf("[muestras] %s 0x%lx %d", top[i].second.first.c_str(), top[i].second.second, top[i].first);
    w3dLogf("[muestras] fin");
    free(gPerfPc); gPerfPc = 0;
}
#endif

void W3dProfLog() {
#if defined(__ANDROID__) && defined(__arm__)
    W3dProfSamples();
#endif
    static W3dProf sum = {0,0,0,0,0};
    static int frames = 0;
    static double t0 = -1.0;
    const double now = W3dNowMs();
    if (t0 < 0.0) t0 = now;
    sum.logic += g_prof.logic; sum.scene += g_prof.scene; sum.viewport3d += g_prof.viewport3d;
    sum.render += g_prof.render; sum.swap += g_prof.swap;
    frames++;
    if (now - t0 < 2000.0) return;
    const double n = (double)frames;
    const w3dEngine::FrameStats& s = w3dEngine::g_statFrame;
    w3dLogf("[perfil] %.1f fps: logica %.1f, escena %.1f, vista 3D %.1f, render %.1f, swap %.1f ms (%d draws, %d triangulos)",
            n * 1000.0 / (now - t0), sum.logic / n, sum.scene / n, sum.viewport3d / n, sum.render / n, sum.swap / n,
            s.draws, s.tris);
    sum.logic = sum.scene = sum.viewport3d = sum.render = sum.swap = 0.0;
    frames = 0; t0 = now;
}
