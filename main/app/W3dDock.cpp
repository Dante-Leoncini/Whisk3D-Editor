// ============================================================================
//  W3dDock.cpp - ver W3dDock.h. Barra de progreso en el icono del dock (Linux),
//  via la senal D-Bus com.canonical.Unity.LauncherEntry.Update.
//
//  POR QUE dlopen Y NO -lgio-2.0: el editor no depende de GLib para nada mas y no
//  queremos que una barrita decorativa agregue una dependencia de compilacion (ni
//  de paquete) a TODO el proyecto. Con dlopen: si GIO esta, la barra se ve; si no
//  esta, no pasa nada. Ademas asi el .deb/AppImage no arrastra glib.
//
//  POR QUE SE ARMA EL GVariant CON LAS FUNCIONES "NO-VARARGS": g_variant_new()
//  toma un format string y varargs; llamarla por puntero obliga a confiar en el
//  ABI de los varargs. Las g_variant_new_string/double/boolean/dict_entry/array/
//  tuple tienen firma fija y arman exactamente el mismo "(sa{sv})".
// ============================================================================
#include "W3dDock.h"

#if defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)

#include <dlfcn.h>
#include <stdlib.h>
#include <stddef.h>

// ---- firmas de GIO/GLib que necesitamos (copiadas de gio/gvariant, sin incluir glib) ----
// GBusType: STARTER=-1, NONE=0, SYSTEM=1, SESSION=2
typedef void* (*Fn_bus_get_sync)(int tipo, void* cancellable, void** error);
typedef int   (*Fn_emit_signal)(void* conn, const char* destino, const char* objeto,
                                const char* interfaz, const char* senal, void* params, void** error);
typedef int   (*Fn_flush_sync)(void* conn, void* cancellable, void** error);
typedef void* (*Fn_var_string)(const char* s);
typedef void* (*Fn_var_double)(double d);
typedef void* (*Fn_var_boolean)(int b);          // gboolean == gint == int
typedef void* (*Fn_var_variant)(void* v);
typedef void* (*Fn_var_dict_entry)(void* clave, void* valor);
typedef void* (*Fn_var_array)(const void* tipoHijo, void* const* hijos, size_t n);
typedef void* (*Fn_var_tuple)(void* const* hijos, size_t n);

static Fn_bus_get_sync   pBusGet    = NULL;
static Fn_emit_signal    pEmit      = NULL;
static Fn_flush_sync     pFlush     = NULL;
static Fn_var_string     pStr       = NULL;
static Fn_var_double     pDbl       = NULL;
static Fn_var_boolean    pBool      = NULL;
static Fn_var_variant    pVar       = NULL;
static Fn_var_dict_entry pDict      = NULL;
static Fn_var_array      pArray     = NULL;
static Fn_var_tuple      pTuple     = NULL;
static void*             gConexion  = NULL;   // GDBusConnection del bus de sesion (vive todo el proceso)

static int   gInit      = 0;      // 0 = sin intentar, 1 = listo, -1 = no disponible
static bool  gEncendido = false;  // estado LOGICO de la barra (independiente del D-Bus)
static int   gUltPct    = -1;     // ultimo entero emitido: filtro anti-spam del bus

// El id tiene que coincidir con el .desktop que instala el .deb
// (platform/linux/whisk3d.desktop -> /usr/share/applications/whisk3d.desktop).
// Desde el AppImage o desde el build no matchea nada y el dock lo ignora: bien asi.
static const char* const kAppId  = "application://whisk3d.desktop";
static const char* const kObjeto = "/com/canonical/Unity/LauncherEntry";
static const char* const kIface  = "com.canonical.Unity.LauncherEntry";

// dlsym devuelve void*: el cast a puntero a funcion es la forma normal de usarlo en POSIX.
static void* Sym(void* h, const char* nombre) { return dlsym(h, nombre); }

// Carga GIO y se conecta al bus de sesion. UNA sola vez; si algo falla queda en -1
// y no se reintenta (no tiene sentido pagar el dlopen en cada update).
static bool Iniciar() {
    if (gInit != 0) return gInit == 1;
    gInit = -1;
    // W3D_DOCK_LIB: solo para los tests (apuntar a una libreria que no existe y
    // comprobar que todo queda en no-op). En uso normal no se setea.
    const char* lib = getenv("W3D_DOCK_LIB");
    if (!lib || !*lib) lib = "libgio-2.0.so.0";
    void* h = dlopen(lib, RTLD_LAZY | RTLD_LOCAL);
    if (!h) return false;                       // sin GIO: no-op silencioso
    pBusGet = (Fn_bus_get_sync)  Sym(h, "g_bus_get_sync");
    pEmit   = (Fn_emit_signal)   Sym(h, "g_dbus_connection_emit_signal");
    pFlush  = (Fn_flush_sync)    Sym(h, "g_dbus_connection_flush_sync");
    // las g_variant_* viven en libglib-2.0, que es dependencia de libgio: dlsym busca
    // en el objeto Y en su arbol de dependencias, asi que salen por el mismo handle.
    pStr    = (Fn_var_string)    Sym(h, "g_variant_new_string");
    pDbl    = (Fn_var_double)    Sym(h, "g_variant_new_double");
    pBool   = (Fn_var_boolean)   Sym(h, "g_variant_new_boolean");
    pVar    = (Fn_var_variant)   Sym(h, "g_variant_new_variant");
    pDict   = (Fn_var_dict_entry)Sym(h, "g_variant_new_dict_entry");
    pArray  = (Fn_var_array)     Sym(h, "g_variant_new_array");
    pTuple  = (Fn_var_tuple)     Sym(h, "g_variant_new_tuple");
    if (!pBusGet || !pEmit || !pStr || !pDbl || !pBool || !pVar || !pDict || !pArray || !pTuple)
        return false;                           // GIO raro/recortado: no-op
    gConexion = pBusGet(2 /*G_BUS_TYPE_SESSION*/, NULL, NULL);
    if (!gConexion) return false;               // sin bus de sesion (tty, contenedor): no-op
    gInit = 1;
    return true;
}

// arma "(sa{sv})" con progress/progress-visible y lo emite. Los GVariant de
// g_variant_new_* nacen FLOTANTES y los consume el contenedor que los guarda; el
// tuple final lo consume emit_signal. Por eso no hace falta unref de nada.
static void Emitir(float frac, bool visible) {
    if (!Iniciar()) return;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    void* entradas[2];
    entradas[0] = pDict(pStr("progress"),         pVar(pDbl((double)frac)));
    entradas[1] = pDict(pStr("progress-visible"), pVar(pBool(visible ? 1 : 0)));
    // "{sv}" es literalmente el GVariantType (G_VARIANT_TYPE_VARDICT y compania son
    // castes de la cadena del tipo, no structs): se puede pasar el literal derecho.
    void* dicc = pArray("{sv}", entradas, 2);
    void* campos[2];
    campos[0] = pStr(kAppId);
    campos[1] = dicc;
    void* params = pTuple(campos, 2);
    pEmit(gConexion, NULL, kObjeto, kIface, "Update", params, NULL);
    // flush: el update tiene que salir YA (si no, un apagado justo antes de cerrar
    // el editor se podria perder y el icono quedaria con la barra colgada).
    if (pFlush) pFlush(gConexion, NULL, NULL);
}

void W3dDockProgreso(float frac) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    int pct = (int)(frac * 100.0f + 0.5f);
    if (gEncendido && pct == gUltPct) return;   // mismo entero: no ensuciamos el bus
    gEncendido = true;
    gUltPct = pct;
    Emitir(frac, true);
}

void W3dDockProgresoFin() {
    if (!gEncendido) return;                    // nunca se prendio: ni dlopen hacemos
    gEncendido = false;
    gUltPct = -1;
    Emitir(0.0f, false);
}

bool W3dDockProgresoEncendido() { return gEncendido; }
bool W3dDockDisponible() { return gInit == 1; }

#endif // linux de escritorio
