// ============================================================================================================
//  heap.c -- QUE TIENE TOMADA LA RAM: anota cada bloque vivo del heap (malloc / calloc / realloc / new) con la pila
//  de llamadas que lo pidio, y al pedirselo imprime las pilas que mas memoria tienen viva en ese momento.
//
//    gcc -O2 -shared -fPIC -o heap.so tools/perfil/heap.c -ldl -lpthread
//    LD_PRELOAD=./heap.so W3D_HEAP_OUT=/tmp/heap.txt SDL_VIDEODRIVER=offscreen build/whisk3d --script guion.w3s
//
//  El comando 'heap <etiqueta>' del harness llama a w3d_heapdump (si esta cargado): agrega al archivo un bloque con
//  la etiqueta, el total vivo y las 5000 pilas con mas bytes. simbolos_heap.py traduce las direcciones a funciones.
//  Solo para la PC (glibc, x86_64): en los telefonos y las consolas se mira con el resultado de aca.
// ============================================================================================================
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

extern void* __libc_malloc(size_t);
extern void* __libc_calloc(size_t, size_t);
extern void* __libc_realloc(void*, size_t);
extern void  __libc_free(void*);
extern void* __libc_memalign(size_t, size_t);

#define DEPTH 28
#define SKIP 2                       // (las dos de aca: la funcion interceptada y Record)
#define LIVE_BITS 23                 // 8M bloques vivos
#define STACK_BITS 18                // 256K pilas distintas

typedef struct { uintptr_t p; uint32_t size; uint32_t stack; } Live;
typedef struct { uint32_t hash; uint32_t n; void* f[DEPTH]; uint64_t bytes; uint64_t count; } Stack;

static Live*  gLive;
static Stack* gStacks;
static volatile int gLock;
static int gReady;
static __thread int tIn;             // ya adentro (backtrace pide memoria la primera vez)

// (reentrante por hilo: el volcado escribe con stdio, que pide y suelta memoria con el candado ya tomado)
static __thread int tLocked;
static void Lock(void)   { if (tLocked++) return; while (__sync_lock_test_and_set(&gLock, 1)) { } }
static void Unlock(void) { if (--tLocked) return; __sync_lock_release(&gLock); }

static void Init(void) {
    if (gReady) return;
    gLive = (Live*)mmap(0, sizeof(Live) << LIVE_BITS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    gStacks = (Stack*)mmap(0, sizeof(Stack) << STACK_BITS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (gLive == MAP_FAILED || gStacks == MAP_FAILED) { gLive = 0; gStacks = 0; return; }
    gReady = 1;
}
static uint32_t HashPtr(uintptr_t p) { p ^= p >> 33; p *= 0xff51afd7ed558ccdULL; p ^= p >> 33; return (uint32_t)p; }

static uint32_t StackId(void** f, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) { h ^= (uint32_t)((uintptr_t)f[i] >> 2); h *= 16777619u; }
    if (!h) h = 1;
    const uint32_t mask = (1u << STACK_BITS) - 1;
    for (uint32_t i = h & mask, k = 0; k <= mask; i = (i + 1) & mask, k++) {
        Stack* s = &gStacks[i];
        if (s->hash == 0) { s->hash = h; s->n = (uint32_t)n; memcpy(s->f, f, sizeof(void*) * (size_t)n); return i; }
        if (s->hash == h && s->n == (uint32_t)n && !memcmp(s->f, f, sizeof(void*) * (size_t)n)) return i;
    }
    return 0;
}
__attribute__((noinline)) static void Record(void* p, size_t size) {
    if (!p || !gReady || tIn) return;
    tIn = 1;
    void* f[DEPTH + SKIP];
    int n = backtrace(f, DEPTH + SKIP);
    n = n > SKIP ? n - SKIP : 0;
    Lock();
    const uint32_t id = StackId(f + SKIP, n);
    gStacks[id].bytes += size; gStacks[id].count++;
    const uint32_t mask = (1u << LIVE_BITS) - 1;
    for (uint32_t i = HashPtr((uintptr_t)p) & mask, k = 0; k <= mask; i = (i + 1) & mask, k++) {
        if (gLive[i].p == 0 || gLive[i].p == 1) { gLive[i].p = (uintptr_t)p; gLive[i].size = (uint32_t)size; gLive[i].stack = id; break; }
    }
    Unlock();
    tIn = 0;
}
static void Forget(void* p) {
    if (!p || !gReady) return;
    Lock();
    const uint32_t mask = (1u << LIVE_BITS) - 1;
    for (uint32_t i = HashPtr((uintptr_t)p) & mask, k = 0; k <= mask; i = (i + 1) & mask, k++) {
        if (gLive[i].p == 0) break;
        if (gLive[i].p == (uintptr_t)p) {
            Stack* s = &gStacks[gLive[i].stack];
            s->bytes -= gLive[i].size; s->count--;
            gLive[i].p = 1;   // (borrado: la busqueda sigue de largo)
            break;
        }
    }
    Unlock();
}

void* malloc(size_t n)            { Init(); void* p = __libc_malloc(n); Record(p, n); return p; }
void* calloc(size_t a, size_t b)  { Init(); void* p = __libc_calloc(a, b); Record(p, a * b); return p; }
void  free(void* p)               { Forget(p); __libc_free(p); }
void* realloc(void* q, size_t n)  { Init(); Forget(q); void* p = __libc_realloc(q, n); Record(p, n); return p; }
void* memalign(size_t a, size_t n){ Init(); void* p = __libc_memalign(a, n); Record(p, n); return p; }
int   posix_memalign(void** r, size_t a, size_t n) { Init(); void* p = __libc_memalign(a, n); if (!p) return 12; Record(p, n); *r = p; return 0; }
void* aligned_alloc(size_t a, size_t n) { Init(); void* p = __libc_memalign(a, n); Record(p, n); return p; }

static int ByBytes(const void* a, const void* b) {
    const Stack* x = *(Stack* const*)a; const Stack* y = *(Stack* const*)b;
    return x->bytes < y->bytes ? 1 : x->bytes > y->bytes ? -1 : 0;
}
// el bloque del archivo: "== etiqueta", el total y las pilas (bytes, bloques, direcciones relativas al ejecutable)
void w3d_heapdump(const char* label) {
    if (!gReady) return;
    tIn = 1;
    const char* out = getenv("W3D_HEAP_OUT");
    FILE* f = fopen(out ? out : "/tmp/w3d_heap.txt", "a");
    if (!f) { tIn = 0; return; }
    Lock();
    static Stack* order[1u << STACK_BITS];
    uint32_t n = 0; uint64_t total = 0, blocks = 0;
    for (uint32_t i = 0; i < (1u << STACK_BITS); i++)
        if (gStacks[i].hash && gStacks[i].bytes) { order[n++] = &gStacks[i]; total += gStacks[i].bytes; blocks += gStacks[i].count; }
    qsort(order, n, sizeof(Stack*), ByBytes);
    fprintf(f, "== %s\ntotal %llu bytes en %llu bloques\n", label ? label : "", (unsigned long long)total, (unsigned long long)blocks);
    for (uint32_t i = 0; i < n && i < 5000; i++) {
        fprintf(f, "%llu %llu", (unsigned long long)order[i]->bytes, (unsigned long long)order[i]->count);
        for (uint32_t k = 0; k < order[i]->n; k++) {
            Dl_info d;
            if (dladdr(order[i]->f[k], &d) && d.dli_fname)
                fprintf(f, " %s+0x%lx", d.dli_fname, (unsigned long)((uintptr_t)order[i]->f[k] - (uintptr_t)d.dli_fbase - 1));
            else fprintf(f, " ?");
        }
        fputc('\n', f);
    }
    Unlock();
    fclose(f);
    tIn = 0;
}
