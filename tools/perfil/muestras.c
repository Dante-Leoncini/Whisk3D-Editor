// ============================================================================
//  PERFIL POR MUESTRAS para la PC (Linux), sin tocar el programa: se carga con LD_PRELOAD (perf necesita
//  kernel.perf_event_paranoid <= 2 y en Ubuntu viene en 4). Es el primo del W3D_PERFIL de Android ARM.
//
//  Cada 1 ms de CPU del HILO PRINCIPAL (un reloj de CPU de ese hilo: los hilos dormidos del driver no cuentan)
//  guarda su pila. Ademas cuenta los SDL_GL_SwapWindow: al final dice los ms de CPU por cuadro.
//
//    gcc -O2 -fPIC -shared -o muestras.so muestras.c -lpthread -ldl -lrt
//    PROF_DELAY=3 PROF_SECS=10 PROF_OUT=m.txt LD_PRELOAD=./muestras.so <programa> ...
//    python3 simbolos.py m.txt <programa> 40
//
//  PROF_DELAY = segundos antes de empezar (lo que tarda en cargar), PROF_SECS = cuanto muestrea.
//  Con el harness: SDL_VIDEODRIVER=offscreen, sin ventanas (y un 'bench' largo en el .w3s).
// ============================================================================
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>

static timer_t gTimer;
static int gTimerOk = 0;

#define MAXN 400000
#define PROF 24
static void* gPc[MAXN][PROF];
static unsigned char gDepth[MAXN];
static volatile int gN = 0;
static volatile int gOn = 0;
static volatile int gSwaps = 0;   // cuadros presentados mientras se muestrea

// cuenta los cuadros: SDL_GL_SwapWindow interpuesto (llama al de verdad)
void SDL_GL_SwapWindow(void* w) {
    static void (*real)(void*) = 0;
    if (!real) real = (void (*)(void*))dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
    if (gOn) gSwaps++;
    if (real) real(w);
}

static void Handler(int sig) {
    (void)sig;
    if (!gOn) return;
    int i = __sync_fetch_and_add(&gN, 1);
    if (i >= MAXN) return;
    void* buf[PROF + 2];
    int d = backtrace(buf, PROF + 2);
    // buf[0] = Handler, buf[1] = el trampolin de la senal; desde buf[2] la pila del programa
    int k = 0;
    for (int j = 2; j < d && k < PROF; j++) gPc[i][k++] = buf[j];
    gDepth[i] = (unsigned char)k;
}

static void Dump(void) {
    const char* out = getenv("PROF_OUT");
    if (gN == 0) return;   // (un proceso hijo que no muestreo nada no pisa el archivo)
    FILE* f = fopen(out ? out : "muestras.txt", "w");
    if (!f) return;
    int n = gN < MAXN ? gN : MAXN;
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < gDepth[i]; k++) {
            Dl_info di;
            memset(&di, 0, sizeof(di));
            // la direccion de retorno apunta a la instruccion siguiente: -1 para caer en la llamada
            void* a = (char*)gPc[i][k] - (k ? 1 : 0);
            if (dladdr(a, &di) && di.dli_fname) {
                const char* b = strrchr(di.dli_fname, '/');
                fprintf(f, "%s%s+%lx", k ? " " : "", b ? b + 1 : di.dli_fname,
                        (unsigned long)((char*)a - (char*)di.dli_fbase));
            } else fprintf(f, "%s?+0", k ? " " : "");
        }
        fputc('\n', f);
    }
    fclose(f);
}

static void* Hilo(void* p) {
    (void)p;
    // este hilo NO recibe SIGPROF: si no, la senal corta su usleep y el muestreo termina antes de tiempo
    sigset_t m; sigemptyset(&m); sigaddset(&m, SIGPROF); pthread_sigmask(SIG_BLOCK, &m, 0);
    const char* d = getenv("PROF_DELAY");
    const char* s = getenv("PROF_SECS");
    usleep((useconds_t)((d ? atof(d) : 5.0) * 1e6));
    struct itimerspec it;
    memset(&it, 0, sizeof(it));
    it.it_interval.tv_nsec = 1000000; it.it_value = it.it_interval;
    gOn = 1;
    if (gTimerOk) timer_settime(gTimer, 0, &it, 0);
    usleep((useconds_t)((s ? atof(s) : 10.0) * 1e6));
    gOn = 0;
    memset(&it, 0, sizeof(it));
    if (gTimerOk) timer_settime(gTimer, 0, &it, 0);
    Dump();
    fprintf(stderr, "[muestras] %d muestras, %d cuadros -> %.3f ms de CPU por cuadro\n", gN, gSwaps, gSwaps ? (double)gN / gSwaps : 0.0);
    return 0;
}

__attribute__((constructor)) static void Arranque(void) {
    if (!getenv("PROF_SECS")) return;
    void* tmp[4];
    backtrace(tmp, 4); // carga libgcc_s ANTES de la primera senal
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = Handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGPROF, &sa, 0);
    // reloj de CPU del HILO PRINCIPAL (el que corre este constructor) y la senal solo a el: muestras = su CPU de verdad
    clockid_t cid;
    pthread_getcpuclockid(pthread_self(), &cid);
    struct sigevent ev;
    memset(&ev, 0, sizeof(ev));
    ev.sigev_notify = SIGEV_THREAD_ID;
    ev.sigev_signo = SIGPROF;
    ev._sigev_un._tid = (pid_t)syscall(SYS_gettid);
    gTimerOk = timer_create(cid, &ev, &gTimer) == 0;
    pthread_t t;
    pthread_create(&t, 0, Hilo, 0);
    pthread_detach(t);
}
