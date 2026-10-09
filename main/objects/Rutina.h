#ifndef RUTINA_H
#define RUTINA_H
// ============================================================================
//  RUTINA: el objeto AVANZADO del render. No tiene posicion, rotacion ni escala:
//  es una LISTA DE PASOS que se ejecuta en su lugar del arbol, en orden, como las
//  llamadas de OpenGL. Cada paso fija UN estado (la niebla prendida es un paso,
//  su inicio otro) o hace UNA cosa (apilar la matriz, apuntar los vertices a una
//  malla, dibujar una parte). El objetivo es gastar la menor CPU y la menor
//  cantidad de llamadas posible: nada se hace si no esta en la lista.
//
//  - DIBUJAR es como en GL: "Array on/off" prende o apaga un array
//    (glEnableClientState: se prende UNA vez y queda), los PUNTEROS (vertices,
//    normales, UV, color) apuntan a los arrays de una malla 3D (un recurso de la
//    biblioteca) y "Dibujar elementos" manda los indices de UNA PARTE con la
//    primitiva elegida. Los punteros pueden ser de mallas distintas (UV o colores
//    compartidos: menos memoria) y se ponen una vez para muchos draws.
//  - LIMPIAR tambien: "Clear color" fija el color (glClearColor) y "Clear" limpia
//    los buffers elegidos (glClear). Un proyecto con constructor limpia SOLO con
//    sus pasos: sin un "Clear" no se limpia nada (tambien en el editor).
//  - Puede tener un SCRIPT (lua cambia sus pasos jugando: setPaso(rutina, 4, "on", false)) y sus pasos se ANIMAN
//    con las animaciones de escena (canal AnimPaso: el paso por su 'id', que no cambia al moverlo; al borrarlo se
//    borran sus curvas).
//  - Puede tener padres e hijos. Sus hijos se dibujan donde la lista tenga el paso
//    "Dibujar hijos" (con el estado que la rutina dejo puesto en ese punto) o, si
//    no lo tiene, despues de la lista.
//  - CONSTRUCTOR: una rutina marcada como constructor (la seccion de arriba del
//    outliner) prepara el estado. En el editor corre en cada cuadro (la interfaz
//    de Whisk3D lo pisa); jugando en modo SOLO JUEGO corre UNA vez y despues el
//    motor solo repone lo que algo le cambio (W3dRutinaConstructorHecho).
//  - Los numeros de un paso son FIJOS o apuntan a una MEMORIA ("@nombre[i]": un
//    bloque de 256 floats que escribe lua con setMemoria).
//  - Las referencias (malla, objeto, textura, rutina) van por NOMBRE en el .w3d y
//    se resuelven a puntero al cargar o al editar.
//  - Hay una lista para TODOS los modos de render y, opcional, una por modo.
//  - El NUCLEO no chequea nada (si esta mal armada, se rompe). El EDITOR la valida
//    antes de dibujarla (main/edit/RutinaEditor.cpp) y la marca en rojo.
//  - Desde LUA: el paso "Llamar Lua" llama, mientras se dibuja, a una funcion del
//    script de OTRO objeto (el que elige el paso), y ahi adentro
//    paso("trasladar", 0, 2, 0) ejecuta cualquier paso (W3dRutinaPasoDesdeLua).
// ============================================================================
#include "objects/Objects.h"
#include <vector>
#include <string>
#include <utility>

// los TIPOS de paso. El .w3d guarda el NOMBRE (W3dPasoNombre), no el numero: el orden del enum se puede cambiar.
enum W3dPasoTipo {
    // transformar (la matriz MODELVIEW)
    PasoApilar = 0, PasoDesapilar, PasoTrasladar, PasoRotar, PasoEscalar, PasoMatrizObjeto,
    // dibujar: prender los arrays, los punteros a los arrays de una malla y los draws
    PasoArray,          // glEnableClientState / glDisableClientState de UN array (el campo 'modo': W3dArrayRutina)
    PasoPunteroVertices, PasoPunteroNormales, PasoPunteroUV, PasoPunteroColores,
    PasoDibujar,        // glDrawElements de una parte de una malla, con la primitiva elegida
    PasoTamPunto,       // glPointSize
    PasoAnchoLinea,     // glLineWidth
    PasoHijos,          // los hijos de esta rutina, aca (con el estado puesto hasta aca)
    PasoRutina,         // ejecuta otra rutina (subrutina)
    // textura
    PasoTextura, PasoFijarTextura, PasoTexturaModo, PasoTexturaFiltro, PasoTexturaRepetir,
    PasoMatrizUV,       // la matriz de textura: desplazamiento y escala de las UV (sin reescribirlas)
    PasoMatcap,         // reflejo: las normales de una malla como UV + la matriz del matcap (anda en el N95)
    // niebla
    PasoNiebla, PasoNieblaModo, PasoNieblaInicio, PasoNieblaFin, PasoNieblaDensidad, PasoNieblaColor,
    // luces
    PasoLuz, PasoLuzN, PasoLuzColor, PasoLuzPosicion, PasoLuzAmbiente,
    // alpha test
    PasoAlfaTest, PasoAlfaRef,
    // color y material
    PasoColor, PasoColorVertice, PasoMaterialColor, PasoBrillo, PasoSombreado,
    // profundidad y buffers
    PasoTestZ, PasoEscribirZ, PasoFuncionZ, PasoSesgoZ, PasoSesgoMetros, PasoRangoZ,
    PasoColorLimpieza,  // glClearColor
    PasoLimpiar,        // glClear de los buffers elegidos (el campo 'entero': W3dLimpiarBits)
    // mezcla
    PasoMezcla, PasoMezclaModo,
    // sueltos
    PasoCaras,
    // control
    PasoSaltarSi,       // si el numero es 0, saltea los proximos N pasos
    PasoSaltarOculto,   // si el objeto (o un padre) esta oculto, saltea los proximos N pasos (lo que oculta un script)
    PasoVisible,        // la caja de una parte contra el frustum de la camara -> 1/0 en una memoria
    PasoLua,            // llama a una funcion del script de un objeto (ahi adentro: paso(...))
    PasoN
};

// los GRUPOS (el menu Add los muestra como submenus; un grupo de un solo paso va suelto)
enum W3dPasoGrupo { GrupoTransformar = 0, GrupoDibujar, GrupoTextura, GrupoNiebla, GrupoLuces, GrupoAlfa,
                    GrupoColor, GrupoProfundidad, GrupoMezcla, GrupoCaras, GrupoControl, GrupoN };
// a que apunta la referencia por nombre de un paso
enum W3dPasoRefTipo { RefNada = 0, RefObjeto, RefMalla, RefTextura, RefRutina, RefFuncion };
// para que usa el ENTERO un paso
enum W3dPasoEnteroUso { EnteroNada = 0, EnteroParte, EnteroLuz, EnteroBool, EnteroCuenta, EnteroBuffers };
// "Clear": que buffers limpia (el campo 'entero', como la mascara de glClear)
enum W3dLimpiarBits { LimpiarColor = 1, LimpiarProfundidad = 2, LimpiarStencil = 4 };
// "Array on/off": que array (el campo 'modo'; el mismo orden que los pasos de puntero)
enum W3dArrayRutina { ArrayVertices = 0, ArrayNormales, ArrayUV, ArrayColores, ArrayN };
// las listas de OPCIONES del campo 'modo' (desplegables)
enum W3dPasoOpciones { OpcNada = 0, OpcNieblaModo, OpcTexturaModo, OpcFuncionZ, OpcMezclaModo, OpcLuzComponente,
                       OpcLuzTipo, OpcPrimitiva, OpcFiltro, OpcRepetir, OpcMaterialComp, OpcArray, OpcMatriz, OpcN };
// el RANGO de "Dibujar elementos" (campo 'sub'): la parte entera, triangulos a mano o los rangos de un array
enum W3dDibujarRango { RangoParte = 0, RangoManual, RangoArray };

const char* W3dPasoNombre(int tipo);      // "trasladar"... (el del .w3d)
int         W3dPasoDesdeNombre(const std::string& n);   // -1 = desconocido
const char* W3dPasoEtiqueta(int tipo);    // "Translate"... (el de la interfaz, en ingles: pasa por T())
int         W3dPasoGrupoDe(int tipo);
const char* W3dGrupoEtiqueta(int grupo);  // "Fog", "Lights"...
int         W3dPasoRef(int tipo);         // W3dPasoRefTipo
bool        W3dPasoUsaOn(int tipo);       // es un on/off
const char* W3dPasoEtiquetaOn(int tipo);  // "On", "Clear color"...
int         W3dPasoEntero(int tipo);      // W3dPasoEnteroUso
int         W3dPasoOpciones(int tipo);    // W3dPasoOpciones
bool        W3dPasoEsColor(int tipo);     // sus 4 numeros son un color RGBA (el panel usa el selector de color)
int         W3dOpcionesN(int lista);
const char* W3dOpcionEtiqueta(int lista, int i);
const char* W3dOpcionesTitulo(int lista); // "Mode", "Function"...
const char* W3dPrimitivaNombre(int prim); // "triangulos", "tira"... (el del .w3d)
enum { W3D_RUTINA_LUCES = 8 };            // GL_LIGHT0..7

// un NUMERO de un paso: fijo o apuntando a una memoria
struct W3dNum {
    float v;               // el valor fijo
    const float* p;        // la memoria resuelta (NULL = el fijo)
    std::string ref;       // "@nombre[i]" tal cual (vacio = fijo)
    W3dNum() : v(0.0f), p(NULL) {}
    float Valor() const { return p ? *p : v; }
};

struct W3dPaso {
    unsigned char tipo;
    unsigned short id;     // IDENTIDAD del paso en su rutina (no cambia al moverlo): la usan sus curvas de animacion
    bool on;               // los pasos on/off
    int entero;            // la parte / el numero de luz / los buffers de "Clear" / cuantos saltear
    int modo;              // la opcion elegida (W3dPasoOpciones); en "Dibujar elementos", la primitiva
    int sub;               // "Dibujar elementos": el rango (W3dDibujarRango)
    W3dNum n[5];
    std::string ref;       // malla / objeto / textura / rutina / funcion de lua
    void* ptr;             // la referencia resuelta (una malla: MallaRecurso*)
    std::string ref2;      // "Test de visibilidad": el objeto cuya matriz mueve la caja (vacio = ninguno);
                           // "Llamar Lua": el objeto cuyo script tiene la funcion
    void* ptr2;
    W3dPaso() : tipo(0), id(0), on(true), entero(0), modo(0), sub(0), ptr(NULL), ptr2(NULL) {}
};

// cuantos numeros usa ESTE paso (en "Dibujar elementos" depende del rango) y la etiqueta del campo i
int         W3dPasoNumeros(const W3dPaso& p);
const char* W3dPasoNumeroNombre(const W3dPaso& p, int i);

// un paso NUEVO de ese tipo con valores que ya hacen algo (escala 1, color blanco, triangulos...)
W3dPaso W3dPasoNuevo(int tipo);

// las MEMORIAS: bloques de 256 floats por nombre (estables: un puntero a uno vale para siempre)
enum { W3D_MEMORIA_TAM = 256 };
float* W3dMemoria(const std::string& nombre);   // la crea en ceros la primera vez
bool   W3dMemoriaExiste(const std::string& nombre);
const float* W3dMemoriaRef(const std::string& ref);   // "@nombre[i]" -> puntero (NULL si esta mal escrito)
int W3dMemoriaIndice(const std::string& ref);         // "@nombre[i]" -> i (-1 si esta mal escrito)

struct MallaRecurso;
// la malla 3D de un nombre: un recurso de la biblioteca o, si no hay, el recurso del OBJETO malla con ese nombre
MallaRecurso* W3dRutinaBuscarMalla(Object* desde, const std::string& nombre);

class Rutina : public Object {
public:
    // los modos de render que pueden tener su propia lista (0 = la de TODOS)
    enum Modo { ModoDefecto = 0, ModoSolido, ModoMaterial, ModoRender, ModoAlambre, ModoZBuffer, ModoN };
    std::vector<W3dPaso> listas[ModoN];
    // que lista usa cada modo: -1 = la de todos; k = la del modo k (la propia si k es el mismo modo)
    signed char usar[ModoN];
    // (editor) la lista que se esta editando y el paso elegido
    int listaEditada, pasoActivo;
    // (editor) el resultado de la ultima validacion: si es invalida no se ejecuta
    bool invalida;
    std::string motivo;
    bool sucia;            // cambio algo: re-resolver nombres (y re-validar en el editor)
    bool conHijos[ModoN];  // la lista tiene "Dibujar hijos" (Resolver): sus hijos los dibuja ella
    bool ejecutada;        // este frame corrio su lista (el editor no corre una invalida)
    bool constructor;      // la seccion CONSTRUCTOR (arriba en el outliner): en solo-juego corre UNA vez
    unsigned genHecha;     // (constructor) la generacion en la que ya corrio (g_w3dConstructorGen)
    std::vector<MallaRecurso*> retenidas;   // las mallas que dibuja (cargadas mientras la rutina las nombre)
    // (editor) como estaban los arrays (vertices, normales, uv, colores: 1/0/-1) cuando EMPEZO a dibujarse en el ultimo
    // cuadro: la validacion arranca de ahi (tambien la del panel, que corre fuera del dibujo). Sin cuadro: no se sabe
    signed char arraysInicio[4];
    bool arraysVistos;
    bool precargada;       // sus mallas ya tienen VBO e indices (la primera vez que se dibuja: no a mitad del juego)
    // (editor) la ultima revision completa de sus punteros (RutinaEditor: PunterosAlDia): dio bien, con que
    // generaciones de objetos / mallas / texturas muertas, y cuantas veces se uso desde entonces
    bool punterosOk;
    unsigned punterosGen[3];
    int punterosEdad;
    // (editor, jugando) la ultima validacion completa y lo que leyo: con que g_w3dRutinasGen, si la dibujaba el arbol,
    // como estaban los arrays y una COPIA de lo que leyo de las memorias (los rangos que escribe lua); cuantos cuadros
    // se reuso (cada tanto se revalida entera igual)
    bool valOk, valSabido, valVistos;
    unsigned valGen;
    signed char valArrays[4];
    int valEdad;
    std::vector<std::pair<const float*, int> > valLecturas;
    std::vector<float> valCopia;

    Rutina(Object* parent = NULL, const std::string& nombre = "Routine");
    ~Rutina() W3D_OVERRIDE;
    ObjectType getType() W3D_OVERRIDE { return ObjectType::rutina; }
    void RenderObject() W3D_OVERRIDE;
    bool DibujaSusHijos() W3D_OVERRIDE;
    bool SaltarEsteCuadro() W3D_OVERRIDE;
    void DespuesDeRender() W3D_OVERRIDE;

    // la lista que corresponde al modo de render actual (y su indice)
    std::vector<W3dPaso>& ListaActual();
    int ListaActualIndice();
    static int ModoActual();
    // resuelve los nombres y las memorias a punteros (al cargar, al editar)
    void Resolver();
    // ejecuta una lista (la usa tambien el paso Rutina)
    void Ejecutar(std::vector<W3dPaso>& L);
    // el paso con ese id (en cualquiera de sus listas) o NULL
    W3dPaso* PasoPorId(unsigned id);
};

// el CONSTRUCTOR en modo SOLO JUEGO (editor VERDE+0 o el juego compilado): cada rutina constructor corre una vez
// por GENERACION. Quien necesite que se rehaga (se entro al modo, se cargo otro proyecto, se perdio el contexto)
// sube la generacion. Despues de correr, la ultima saca la FOTO del estado (w3dEngine::EstadoFotoTomar) y en los
// cuadros siguientes solo se repone lo que algo cambio (EstadoFotoReponer: el HUD 2D, por ejemplo)
extern unsigned g_w3dConstructorGen;
extern unsigned g_w3dRutinasGen;   // (vive en el core: ver objects/MallaRecurso.h)
extern bool g_w3dSoloJuego;

// el proyecto tiene CONSTRUCTOR (alguna rutina constructor arriba de todo): limpiar la pantalla es cosa del
// proyecto (sus pasos "Clear color" y "Clear"), no del viewport ni del juego: sin un "Clear" no se limpia nada.
// Un proyecto viejo (sin constructor) lo sigue limpiando el viewport
bool W3dHayConstructor();
// (editor) el viewport ya limpio el color con el fondo FIJO de su modo (Z-Buffer, Alfa, Normales: el fondo dice algo):
// ahi "Clear color" no cambia nada y "Clear" no limpia el color (la profundidad y el estencil si)
extern bool g_w3dLimpiarColorFijo;
// cuantas veces un "Clear" limpio el COLOR (crece siempre): el editor sabe si en un cuadro alguien limpio la pantalla
extern unsigned g_w3dLimpiezasColor;
// lo que trae todo proyecto nuevo, arriba de todo en 'raiz': el CONSTRUCTOR (con el "Clear color": 'fondo') y la
// rutina de cada cuadro que LIMPIA la pantalla (color y profundidad). Quitarla o apagarla = no limpiar
void W3dRutinasPorDefecto(Object* raiz, const std::string& nombreConstructor, const std::string& nombreLimpiar,
                          const float fondo[4]);

// PERFIL (harness 'rutinaperfil'): por tipo de paso, cuantas veces corrio y cuantas llamadas GL hizo (draws + binds
// + estados + otras). Apagado no cuesta nada
extern bool g_rutinaPerfil;
extern long g_rutinaPerfilVeces[PasoN], g_rutinaPerfilGL[PasoN];

// el valor ACTUAL del campo de un paso para la animacion (componente de AnimPaso: id * 8 + campo)
float W3dRutinaPasoValor(Rutina* r, int componente);

// (lua) ejecuta UN paso ya armado (resuelve sus nombres con un cache). Solo vale mientras se dibuja una rutina
// (adentro de una funcion del paso "Llamar Lua"); afuera devuelve false y no hace nada
bool W3dRutinaPasoDesdeLua(W3dPaso& p, std::string& error);

// el .w3d: los campos de la rutina (despues de los comunes) y su lectura
// 'emitirTextura' (el guardado): mete la textura de un "Bind texture" en el .w3d como la de un material y devuelve
// lo que se escribe (la entrada del contenedor). NULL = la ruta tal cual. 'refTextura' (la carga): resuelve lo leido
// a la ruta en memoria (la misma regla que las texturas de los materiales). NULL = tal cual
struct JVal;
void RutinaEscribirCampos(std::string& s, int ind, Rutina* r, std::string (*emitirTextura)(std::string& ruta, void* ctx) = NULL,
                          void* ctx = NULL);
void RutinaLeerCampos(JVal* j, Rutina* r, std::string (*refTextura)(const std::string& guardada, void* ctx) = NULL,
                      void* ctx = NULL);
const char* W3dRutinaModoNombre(int m);    // "defecto", "solido", "material", "render", "alambre", "zbuffer"

// el EDITOR engancha aca su validacion (el nucleo no la tiene): true = se puede ejecutar
extern bool (*W3dRutinaValidarHook)(Rutina*);

#endif
