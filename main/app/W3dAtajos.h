#ifndef W3DATAJOS_H
#define W3DATAJOS_H

// ============================================================================
//  ATAJOS DE TECLADO REASIGNABLES (todas las plataformas).
//
//  Cada funcion del editor que se dispara con el teclado tiene aca su fila: un id estable (la clave en atajos.ini),
//  el contexto donde vive (vista 3D, editor UV, timeline...), su nombre y su combinacion POR DEFECTO. El usuario le
//  puede poner una o varias combinaciones (o ninguna).
//
//  Los handlers de cada viewport NO cambian: siguen escuchando la tecla por defecto. Lo que cambia es la puerta de
//  entrada: la combinacion que se aprieta se TRADUCE a la por defecto de la funcion que la tiene asignada antes de
//  repartirla (W3dAtajoTraducir). Asi un mismo mapa vale para PC, Android, Web y el telefono sin tocar cada caso.
//  Una combinacion por defecto que el usuario le saco a su funcion queda LIBRE: no hace nada.
// ============================================================================
#include <string>
#include <vector>

// Donde vive cada funcion. El orden importa solo para la UI (la lista de contextos).
enum W3dAtajoCtx {
    W3dATC_GLOBAL = 0,  // en cualquier viewport (guardar, deshacer, play...)
    W3dATC_3D,          // vista 3D
    W3dATC_XFORM,       // con un mover/rotar/escalar en curso (3D, UV, 2D, keyframes)
    W3dATC_UV,          // editor UV
    W3dATC_TIMELINE,    // timeline / dope sheet / curvas
    W3dATC_OUTLINER,    // outliner (escena y recursos)
    W3dATC_2D,          // editor 2D (UI de juegos)
    W3dATC_PROPS,       // panel de propiedades (pasos de una rutina)
    W3dATC_IDE,         // editor de scripts
    // los MODOS de la vista 3D: lo que solo existe en Modo Objeto o en Modo Edicion (malla o huesos). La cadena del
    // foco los pone ANTES de W3dATC_3D (lo comun a los dos modos): una combinacion del modo tapa a la comun.
    W3dATC_3D_OBJ,      // vista 3D en Modo Objeto (y Pose)
    W3dATC_3D_EDIT,     // vista 3D en Modo Edicion
    W3dATC_N
};
enum { W3dMOD_SHIFT = 1, W3dMOD_CTRL = 2, W3dMOD_ALT = 4 };

struct W3dCombo {
    int tecla;   // W3dTecla (ViewPorts/W3dInput.h)
    int mods;    // W3dMOD_*
    W3dCombo() : tecla(0), mods(0) {}
    W3dCombo(int t, int m) : tecla(t), mods(m) {}
    bool operator==(const W3dCombo& o) const { return tecla == o.tecla && mods == o.mods; }
    bool Vacia() const { return tecla == 0; }
};

struct W3dAtajoDef {
    const char* id;       // "3d.mover": la clave en atajos.ini (no cambiar: rompe los archivos guardados)
    int         ctx;      // W3dAtajoCtx
    const char* nombre;   // en ingles; T() lo traduce
    const char* defecto;  // "G" o "Ctrl+Y, Ctrl+Shift+Z". La PRIMERA es la que escucha el codigo.
    // Lo que se MUESTRA en el telefono (Symbian). El teclado del N95 es numerico y tiene su propio esquema fijo
    // (1/2/3 = mover/rotar/escalar, C = borrar...): mostrarle "Shift D" a alguien con un keypad no le sirve.
    const char* telefono; // la tecla del keypad que hace esto (NULL = ninguna)
    const char* bt;       // la del teclado Bluetooth del N95, que entiende solo algunas (NULL = no la entiende)
    // OPERACIONES SIN TECLA DE FABRICA (las de los menus: "Origin to Geometry", "Shade Smooth"...): como ningun
    // handler escucha una tecla para ellas, la combinacion que el usuario les asigne corre esto: "<despacho>:<id>",
    // la accion del menu de donde salen con el id de su opcion (W3dAtajoRegistrarDespacho). NULL = la de siempre
    // (se traduce a la tecla por defecto y la escucha el handler del viewport).
    const char* accion;
};

int                          W3dAtajosCantidad();
const W3dAtajoDef&           W3dAtajoInfo(int i);
const std::vector<W3dCombo>& W3dAtajoCombos(int i);          // las asignadas hoy
std::string                  W3dAtajoTexto(int i);           // "G, Ctrl+M" ("" = ninguna)
int                          W3dAtajoBuscar(const char* id); // -1 = no existe
bool                         W3dAtajoEsDefecto(int i);
const char*                  W3dAtajoCtxNombre(int ctx);     // en ingles (T() al mostrarlo)
// Dos contextos que se pisan: el mismo, o la vista 3D comun y uno de sus modos (una combinacion no puede estar en
// los dos sin que uno tape al otro). Agregar una combinacion se la saca a las funciones del mismo alcance.
bool                         W3dAtajoMismoAlcance(int ctxA, int ctxB);

// LAS ACCIONES de las operaciones sin tecla de fabrica: cada parte del editor registra al arrancar la accion de sus
// menus con un nombre ("obj" = LayoutAccionObject, "tlkey" = el menu Key del timeline...). La llama con el id.
typedef void (*W3dAtajoDespacho)(int id);
void W3dAtajoRegistrarDespacho(const char* nombre, W3dAtajoDespacho fn);
bool W3dAtajoTieneDespacho(const char* accion);              // "obj:201": hay quien la corra
bool W3dAtajoEjecutar(int i);                                // corre la accion de la funcion i (false = no tiene)

// Cambios (cada uno guarda atajos.ini). Agregar quita la combinacion de las otras funciones del MISMO contexto:
// *quitadaDe = la que la tenia (-1 = ninguna).
bool W3dAtajoAgregar(int i, W3dCombo c, int* quitadaDe);
void W3dAtajoVaciar(int i);
void W3dAtajoRestablecer(int i);
void W3dAtajosRestablecerTodo();
bool W3dAtajosFijar(int i, const char* lista);               // "G, Ctrl+M" (harness / archivo)

std::string W3dComboTexto(W3dCombo c);
std::string W3dComboTextoMenu(W3dCombo c);                   // "Ctrl I", "Num 0": como se ve en los menus

// El hint de atajo de un item de menu. Los menus NO escriben la tecla: guardan "@<id>" y se resuelve al dibujar,
// asi muestran lo que el usuario configuro. En el telefono sale la tecla del keypad (o la del teclado Bluetooth si
// hay uno conectado). "@tel:9" = solo el keypad del telefono. Un texto sin "@" es un gesto de mouse ("Ctrl Click")
// y se muestra tal cual donde hay mouse.
std::string W3dAtajoEtiquetaMenu(const std::string& hint);
bool        W3dComboDesdeTexto(const std::string& s, W3dCombo& c);
bool        W3dEsTeclaModificador(int tecla);

bool        W3dAtajosCargar();                               // lo llama solo la primera traduccion
bool        W3dAtajosGuardar();
std::string W3dAtajosRuta();
void        W3dAtajosRutaFijar(const std::string& ruta);     // harness: no tocar el archivo del usuario

// EL TRADUCTOR. ctxs = la cadena de contextos del foco (el mas especifico primero). Entra la combinacion APRETADA
// y sale la que entiende el codigo. false = la combinacion quedo sin funcion: no hay que mandarla a ningun lado.
// repeticion = la tecla quedo apretada (auto-repeat): una funcion con accion propia NO se vuelve a correr.
bool W3dAtajoTraducir(const int* ctxs, int n, int& tecla, int& mods, bool repeticion = false);
// La cadena del foco actual (viewport activo + su modo + transform en curso). Devuelve cuantos contextos escribio (max 4).
int  W3dAtajoContextos(int* ctxs);
// Atajo de las dos de arriba con los modificadores sueltos (los flags LShiftPressed/LCtrlPressed/LAltPressed).
int  W3dModsActuales();

// Mientras vive, los flags de modificadores (LShiftPressed/LCtrlPressed/LAltPressed) dicen lo que pide la combinacion
// TRADUCIDA; al morir vuelven a lo que de verdad esta apretado. Los handlers leen esos flags.
class W3dModsTemporales {
    public:
        W3dModsTemporales();
        ~W3dModsTemporales();
        void Fijar(int mods);
    private:
        bool activo, shift, ctrl, alt;
};

// CAPTURA de la UI de reasignar: mientras haya una funcion esperando, la proxima combinacion es para ella.
extern int g_atajoCapturando;   // indice esperando tecla (-1 = nadie)
bool W3dAtajoCapturar(int tecla, int mods);   // true = la tomo (o cancelo con Esc)

#endif
