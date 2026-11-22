#ifndef ROUTINE_H
#define ROUTINE_H
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
//    Los dos arrancan en AUTO: "Clear color" toma el fondo del render (la tarjeta
//    Render; en el editor, fuera de la vista Render, el fondo del visor) y con un
//    fondo 360 no hace nada; "Clear" limpia con ese color o, con un fondo 360,
//    dibuja el cielo. Si no: un color propio (que pisa el fondo: cada escena el
//    suyo), solo limpiar con el color, o un fondo 360 propio (su imagen y su giro).
//  - Puede tener un SCRIPT (lua cambia sus pasos jugando: setPaso(rutina, 4, "on", false)) y sus pasos se ANIMAN
//    con las animaciones de escena (canal AnimStep: el paso por su 'id', que no cambia al moverlo; al borrarlo se
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
//    antes de dibujarla (main/edit/RoutineEditor.cpp) y la marca en rojo.
//  - Desde LUA: el paso "Llamar Lua" llama, mientras se dibuja, a una funcion del
//    script de OTRO objeto (el que elige el paso), y ahi adentro
//    step("trasladar", 0, 2, 0) ejecuta cualquier paso (W3dRoutineStepFromLua).
// ============================================================================
#include "objects/Objects.h"
#include <vector>
#include <string>
#include <map>
#include <utility>

class Texture;

// los TIPOS de paso. El .w3d guarda el NOMBRE (W3dStepName), no el numero: el orden del enum se puede cambiar.
enum W3dStepType {
    // transformar (la matriz MODELVIEW)
    StepStack = 0, StepPop, StepTranslate, StepRotate, StepScale, StepMatrixObject,
    // dibujar: prender los arrays, los punteros a los arrays de una malla y los draws
    StepArray,          // glEnableClientState / glDisableClientState de UN array (el campo 'modo': W3dArrayRoutine)
    StepPointerVertices, StepPointerNormals, StepPointerUV, StepPointerColors,
    StepDraw,          // glDrawElements de una parte de una malla, con la primitiva elegida
    StepSizePoint,     // glPointSize
    StepWidthLine,     // glLineWidth
    StepChildren,      // los hijos de esta rutina, aca (con el estado puesto hasta aca)
    StepRoutine,       // ejecuta otra rutina (subrutina)
    // textura
    StepTexture, StepSetTexture, StepTextureMode, StepTextureFilter, StepTextureRepeat,
    StepMatrixUV,       // la matriz de textura: desplazamiento y escala de las UV (sin reescribirlas)
    StepMatcap,         // reflejo: las normales de una malla como UV + la matriz del matcap (anda en el N95)
    // niebla
    StepFog, StepFogMode, StepFogStart, StepFogEnd, StepFogDensity, StepFogColor,
    // luces
    StepLight, StepLightN, StepLightColor, StepLightPosition, StepLightAmbient,
    // alpha test
    StepAlphaTest, StepAlphaRef,
    // color y material
    StepColor, StepColorVertex, StepMaterialColor, StepBrightness, StepShading,
    // profundidad y buffers
    StepTestZ, StepWriteZ, StepFunctionZ, StepBiasZ, StepBiasMeters, StepRangeZ,
    StepColorClear,  // glClearColor
    StepClear,       // glClear de los buffers elegidos (el campo 'entero': W3dClearBits)
    // mezcla
    StepMix, StepMixMode,
    // sueltos
    StepFaces,
    // control
    StepSkipIf,      // si el numero es 0, saltea los proximos N pasos
    StepSkipHidden,  // si el objeto (o un padre) esta oculto, saltea los proximos N pasos (lo que oculta un script)
    StepVisible,     // la caja de una parte contra el frustum de la camara -> 1/0 en una memoria
    StepLua,         // llama a una funcion del script de un objeto (ahi adentro: step(...))
    StepMixTextures, // MEZCLA DE TEXTURAS en una pasada (gfx::MixTextures): la unidad 1 interpola la textura 'ref' (B)
                        // con lo de la unidad 0; n[0] = cuanto de B (0..1, animable / de una memoria); off = una unidad
    StepSkipIfNone,// si NINGUNA bandera de un array de rangos esta prendida (los mismos n[0] = rangos y n[1] = banderas
                        // que un "Dibujar elementos" por array), saltea los proximos N pasos: el estado y el dibujo de una
                        // parte sin ninguna pieza visible no cuestan nada
    StepPointerPose,    // un puntero (el array del campo 'modo': W3dArrayRoutine) a los arrays de un OBJETO malla: si el objeto
                        // todavia COMPARTE esa capa con su malla de la biblioteca, el VBO de la malla (subido una vez); si
                        // tiene la suya (su vertex anim lo poso, lua le cambio colores, una animacion de UV), su copia de RAM.
                        // Animar no re-especifica ningun buffer (el stall del tiler MBX)
    StepDrawVisible,// los triangulos VISIBLES de un objeto malla: la lista de la celda del modificador Oclusion (indices
                        // de cliente) o, sin oclusion, la malla entera desde el IBO de su recurso. Con los punteros que haya
    StepSkipOutside,    // si el objeto esta FUERA del rectangulo XZ (n0..n3 = min X, max X, min Z, max Z), saltea N pasos
                        // (un reflejo que solo existe sobre su charco)
    StepSkipRail,     // saltea N pasos segun el RIEL por el que va la camara activa ('ref' = la curva): modo 0 = si va
                        // por el, 1 = si NO va. El escenario con la oclusion de OTRO riel (el viaje al bonus) se dibuja
                        // solo mientras la camara va por ese riel
    StepSkipNoListing,// si el objeto NO esta en la lista del nodo actual de su Culling de riel (Object::listingRail == 0:
                        // la SLST del original dice que desde aca no se ve), saltea N pasos
    StepCopyScreen, // copia lo dibujado hasta aca (el cuadro de la imagen) a la textura 'ref', la captura "pantalla:<ref>"
                        // (render a textura: dibujar, copiar, limpiar y dibujar lo demas con esa textura; SceneRender.h)
    StepShader,         // un SHADER ESCRITO (el archivo .glsl 'ref', objects/WrittenShader.h): prendido, los triangulos que
                        // se dibujan despues lo usan (como cualquier estado de GL); apagado, se saca. n[0..3] = IOR,
                        // transmision, rugosidad y metalico (w3d_ior...). Solo en los backends con shaders (GLES 2.0,
                        // Vulkan); en GLES 1.1 no hace nada. Al terminar la rutina se saca solo
    StepN
};

// los GRUPOS (el menu Add los muestra como submenus; un grupo de un solo paso va suelto)
enum W3dStepGroup { GroupTransform = 0, GroupDraw, GroupTexture, GroupFog, GroupLights, GroupAlpha,
                    GroupColor, GroupDepth, GroupMix, GroupFaces, GroupControl, GroupN };
// a que apunta la referencia por nombre de un paso
enum W3dStepRefType { RefNothing = 0, RefObject, RefMesh, RefTexture, RefRoutine, RefFunction, RefShader };
// para que usa el ENTERO un paso
// (IntegerLayerUV: el puntero de UV a una CAPA de la malla: 0 = la UV de render, k = la capa extra k, uvExtra[k])
enum W3dStepIntegerUse { IntegerNothing = 0, IntegerPart, IntegerLight, IntegerBool, IntegerCount, IntegerBuffers, IntegerLayerUV };
// "Clear": que buffers limpia (el campo 'entero', como la mascara de glClear)
enum W3dClearBits { ClearColor = 1, ClearDepth = 2, ClearStencil = 4 };
// "Array on/off": que array (el campo 'modo'; el mismo orden que los pasos de puntero)
enum W3dArrayRoutine { ArrayVertices = 0, ArrayNormals, ArrayUV, ArrayColors, ArrayN };
// las listas de OPCIONES del campo 'modo' (desplegables)
enum W3dStepOptions { OptNothing = 0, OptFogMode, OptTextureMode, OptFunctionZ, OptMixMode, OptLightComponent,
                       OptLightType, OptPrimitive, OptFilter, OptRepeat, OptMaterialComp, OptArray, OptMatrix,
                       OptMixTex,   // "Texture mix": Interpolate / Multiply (lightmap) / Multiply 2x
                       OptRail,     // "Skip if camera riel": si va por el / si no va
                       OptColorClear,        // "Clear color": Auto / Usar color personalizado (W3dClearColorMode)
                       OptClearBackground,   // "Clear": Auto / Limpiar con el color / Dibujar fondo personalizado
                       OptN };
// "Clear color": de donde sale el color (el campo 'modo')
enum W3dClearColorMode { ColorClearAuto = 0,   // el fondo del render (con un fondo 360: nada, lo tapa el cielo)
                            ColorClearOwn };   // los 4 numeros del paso (pisan el fondo)
// "Clear": que se hace con el COLOR (el campo 'modo'; los buffers siguen en 'entero')
enum W3dClearBackgroundMode { ClearBackgroundAuto = 0,     // limpia con el color y, con un fondo 360, dibuja el cielo
                           ClearBackgroundColor,           // solo limpia con el color (sin cielo)
                           ClearBackgroundOwn };           // dibuja el fondo 360 del paso ('ref': la imagen; n[0..2]: su giro)
// el RANGO de "Dibujar elementos" (campo 'sub'): la parte entera, triangulos a mano o los rangos de un array
enum W3dDrawRange { RangePart = 0, RangeManual, RangeArray };

const char* W3dStepName(int type);      // "trasladar"... (el del .w3d)
int         W3dStepFromName(const std::string& n);   // -1 = desconocido
const char* W3dStepLabel(int type);    // "Translate"... (el de la interfaz, en ingles: pasa por T())
int         W3dStepGroupOf(int type);
const char* W3dGroupLabel(int group);  // "Fog", "Lights"...
int         W3dStepRef(int type);      // W3dStepRefType
bool        W3dStepUsesOn(int type);   // es un on/off
const char* W3dStepLabelOn(int type);  // "On", "Clear color"...
int         W3dStepInteger(int type);  // W3dStepIntegerUse
int         W3dStepOptions(int type);  // W3dStepOptions
bool        W3dStepIsColor(int type);  // sus 4 numeros son un color RGBA (el panel usa el selector de color)
int         W3dOptionsN(int list);
const char* W3dOptionLabel(int list, int i);
const char* W3dOptionsTitle(int list);  // "Mode", "Function"...
const char* W3dPrimitiveName(int prim); // "triangulos", "tira"... (el del .w3d)
enum { W3D_ROUTINE_LIGHTS = 8 };        // GL_LIGHT0..7

// un NUMERO de un paso: fijo o apuntando a una memoria
struct W3dNum {
    float v;               // el valor fijo
    const float* p;        // la memoria resuelta (NULL = el fijo)
    std::string ref;       // "@nombre[i]" tal cual (vacio = fijo)
    W3dNum() : v(0.0f), p(NULL) {}
    float Value() const { return p ? *p : v; }
};

struct W3dStep {
    unsigned char type;
    unsigned short id;     // IDENTIDAD del paso en su rutina (no cambia al moverlo): la usan sus curvas de animacion
    bool on;               // los pasos on/off
    int integer;           // la parte / el numero de luz / los buffers de "Clear" / cuantos saltear
    int mode;              // la opcion elegida (W3dStepOptions); en "Dibujar elementos", la primitiva
    int sub;               // "Dibujar elementos": el rango (W3dDrawRange)
    W3dNum n[5];
    std::string ref;       // malla / objeto / textura / rutina / funcion de lua
    void* ptr;             // la referencia resuelta (una malla: MeshResource*)
    std::string ref2;      // "Test de visibilidad": el objeto cuya matriz mueve la caja (vacio = ninguno);
                           // "Llamar Lua": el objeto cuyo script tiene la funcion
    void* ptr2;
    W3dStep() : type(0), id(0), on(true), integer(0), mode(0), sub(0), ptr(NULL), ptr2(NULL) {}
};

// cuantos numeros usa ESTE paso (en "Dibujar elementos" depende del rango) y la etiqueta del campo i
int         W3dStepNumbers(const W3dStep& p);
const char* W3dStepNumberName(const W3dStep& p, int i);

// un paso NUEVO de ese tipo con valores que ya hacen algo (escala 1, color blanco, triangulos...)
W3dStep W3dNewStep(int type);

// las MEMORIAS: bloques de 256 floats por nombre (estables: un puntero a uno vale para siempre)
enum { W3D_MEMORY_SIZE = 256 };
float* W3dMemory(const std::string& name);   // la crea en ceros la primera vez
bool   W3dMemoryExists(const std::string& name);
const float* W3dMemoryRef(const std::string& ref);   // "@nombre[i]" -> puntero (NULL si esta mal escrito)
int W3dMemoryIndex(const std::string& ref);          // "@nombre[i]" -> i (-1 si esta mal escrito)

struct MeshResource;
// la malla 3D de un nombre: un recurso de la biblioteca o, si no hay, el recurso del OBJETO malla con ese nombre
MeshResource* W3dRoutineFindMesh(Object* from, const std::string& name);

class Routine : public Object {
public:
    // los modos de render que pueden tener su propia lista (0 = la de TODOS)
    enum Mode { ModeDefault = 0, ModeSolid, ModeMaterial, ModeRender, ModeWire, ModeZBuffer, ModeN };
    std::vector<W3dStep> lists[ModeN];
    // que lista usa cada modo: -1 = la de todos; k = la del modo k (la propia si k es el mismo modo)
    signed char use[ModeN];
    // (editor) la lista que se esta editando y el paso elegido
    int editedList, activeStep;
    // (editor) el resultado de la ultima validacion: si es invalida no se ejecuta
    bool invalid;
    std::string reason;
    bool dirty;                // cambio algo: re-resolver nombres (y re-validar en el editor)
    bool withChildren[ModeN];  // la lista tiene "Dibujar hijos" (Resolver): sus hijos los dibuja ella
    bool run;                  // este frame corrio su lista (el editor no corre una invalida)
    bool constructor;          // la seccion CONSTRUCTOR (arriba en el outliner): en solo-juego corre UNA vez
    // BUCLE DE RUTINA: la lista corre 'repetir' veces seguidas, sin reponer nada entre vuelta y vuelta ("trasladar,
    // dibujar" x 20 = 20 copias corridas: un array mas barato que 20 objetos). La vuelta actual queda en la memoria
    // "@bucle[0]" (y el total en "@bucle[1]") para que los pasos varien. Al terminar la corrida la matriz se repone
    // como siempre. float para el PropFloat entero del panel. 1 = una rutina comun.
    float repeat;
    // MEMORIAS INICIALES: lo que el .w3d trae escrito en memorias (las cajas de las celdas de un nivel, una tabla): se
    // cargan al leer la rutina (W3dMemory) y se guardan con ella. Lua puede pisarlas como cualquier memoria
    std::vector<std::pair<std::string, std::vector<float> > > initialMemories;
    unsigned doneGen;     // (constructor) la generacion en la que ya corrio (g_w3dConstructorGen)
    std::vector<MeshResource*> retained;   // las mallas que dibuja (cargadas mientras la rutina las nombre)
    std::vector<Texture*> retainedTex;     // y sus texturas (una referencia por paso que la nombra)
    // (editor) como estaban los arrays (vertices, normales, uv, colores: 1/0/-1) cuando EMPEZO a dibujarse en el ultimo
    // cuadro: la validacion arranca de ahi (tambien la del panel, que corre fuera del dibujo). Sin cuadro: no se sabe
    signed char arraysStart[4];
    bool arraysSeen;
    bool preloaded;       // sus mallas ya tienen VBO e indices (la primera vez que se dibuja: no a mitad del juego)
    // (editor) la ultima revision completa de sus punteros (RutinaEditor: PointersUpToDate): dio bien, con que
    // generaciones de objetos / mallas / texturas muertas, y cuantas veces se uso desde entonces
    bool pointersOk;
    unsigned pointersGen[3];
    int pointersAge;
    // (editor, jugando) la ultima validacion completa y lo que leyo: con que g_w3dRoutinesGen, si la dibujaba el arbol,
    // como estaban los arrays y una COPIA de lo que leyo de las memorias (los rangos que escribe lua); cuantos cuadros
    // se reuso (cada tanto se revalida entera igual)
    bool valOk, valKnown, valSeen;
    unsigned valGen;
    signed char valArrays[4];
    int valAge;
    std::vector<std::pair<const float*, int> > valReadings;
    std::vector<float> valCopy;

    Routine(Object* parent = NULL, const std::string& name = "Routine");
    ~Routine() W3D_OVERRIDE;
    ObjectType getType() W3D_OVERRIDE { return ObjectType::routine; }
    void RenderObject() W3D_OVERRIDE;
    bool DrawsItsChildren() W3D_OVERRIDE;
    bool SkipThisFrame() W3D_OVERRIDE;
    void AfterRender() W3D_OVERRIDE;

    // la lista que corresponde al modo de render actual (y su indice)
    std::vector<W3dStep>& CurrentList();
    int CurrentListIndex();
    static int CurrentMode();
    // resuelve los nombres y las memorias a punteros (al cargar, al editar)
    void Resolve();
    // ejecuta una lista (la usa tambien el paso Rutina)
    void Run(std::vector<W3dStep>& L);
    // el paso con ese id (en cualquiera de sus listas) o NULL
    W3dStep* StepById(unsigned id);
};

// el CONSTRUCTOR en modo SOLO JUEGO (editor VERDE+0 o el juego compilado): cada rutina constructor corre una vez
// por GENERACION. Quien necesite que se rehaga (se entro al modo, se cargo otro proyecto, se perdio el contexto)
// sube la generacion. Despues de correr, la ultima saca la FOTO del estado (w3dEngine::SnapshotStateTake) y en los
// cuadros siguientes solo se repone lo que algo cambio (SnapshotStateRestore: el HUD 2D, por ejemplo)
extern unsigned g_w3dConstructorGen;
extern unsigned g_w3dRoutinesGen;   // (vive en el core: ver objects/MeshResource.h)
extern bool g_w3dOnlyGame;

// el proyecto tiene CONSTRUCTOR (alguna rutina constructor arriba de todo): limpiar la pantalla es cosa del
// proyecto (sus pasos "Clear color" y "Clear"), no del viewport ni del juego: sin un "Clear" no se limpia nada.
// Un proyecto viejo (sin constructor) lo sigue limpiando el viewport
bool W3dHasConstructor();
// lo que dibuja una rutina en una corrida SIN recorte (el total del overlay "faces: dibujadas/total"): los triangulos de
// sus "Dibujar elementos" con su rango y sin las banderas ni los saltos (eso es el recorte), por 'repetir'; y los
// vertices de cada malla distinta que dibuja. Los pasos Rutina no se siguen: la llamada se cuenta en su lugar del arbol
void W3dRoutineCount(Routine* rt, int& verts, int& tris);
// (editor) el viewport ya limpio el color con el fondo FIJO de su modo (Z-Buffer, Alfa, Normales: el fondo dice algo):
// ahi "Clear color" no cambia nada y "Clear" no limpia el color (la profundidad y el estencil si)
extern bool g_w3dClearColorFixed;
// LA PANTALLA DIVIDIDA: mientras se dibuja una mitad, su rectangulo (x, y, ancho, alto en pixeles de GL). El paso
// "Limpiar" recorta ahi aunque el estado restaurado del constructor haya apagado el scissor: si no, la segunda mitad
// borraba la primera
extern bool g_w3dClipViewOn;
extern int g_w3dClipView[4];
// cuantas veces un "Clear" limpio el COLOR (crece siempre): el editor sabe si en un cuadro alguien limpio la pantalla
extern unsigned g_w3dClearsColor;
// lo que trae todo proyecto nuevo, arriba de todo en 'raiz': el CONSTRUCTOR (con el "Clear color": 'fondo') y, AFUERA
// de el y antes de la coleccion, la rutina de cada cuadro que LIMPIA la pantalla (color y profundidad). Quitarla o
// apagarla = no limpiar
void W3dDefaultRoutines(Object* root, const std::string& constructorName, const std::string& nameClear,
                          const float background[4]);
// un proyecto de antes con la limpieza ADENTRO del constructor 'c': la saca afuera, justo despues (al abrirlo)
void W3dConstructorRemoveClear(Object* c);

// PERFIL (harness 'rutinaperfil'): por tipo de paso, cuantas veces corrio y cuantas llamadas GL hizo (draws + binds
// + estados + otras). Apagado no cuesta nada
extern bool g_routineProfile;
extern long g_routineProfileTimes[StepN], g_routineProfileGL[StepN];
extern double g_routineProfileMs[StepN];                              // ms por tipo de paso (los que no anidan)
extern std::map<std::string, double> g_routineProfileMsOf;          // ms por rutina (por nombre, inclusivo)
extern std::map<std::string, long> g_routineProfileTrisOf;          // triangulos por rutina (inclusivo)

// el valor ACTUAL del campo de un paso para la animacion (componente de AnimStep: id * 8 + campo)
float W3dRoutineStepValue(Routine* r, int component);

// (lua) ejecuta UN paso ya armado (resuelve sus nombres con un cache). Solo vale mientras se dibuja una rutina
// (adentro de una funcion del paso "Llamar Lua"); afuera devuelve false y no hace nada
bool W3dRoutineStepFromLua(W3dStep& p, std::string& error);

// el .w3d: los campos de la rutina (despues de los comunes) y su lectura
// 'emitTexture' (el guardado): mete la textura de un "Bind texture" en el .w3d como la de un material y devuelve
// lo que se escribe (la entrada del contenedor). NULL = la ruta tal cual. 'refTexture' (la carga): resuelve lo leido
// a la ruta en memoria (la misma regla que las texturas de los materiales). NULL = tal cual
struct JVal;
void RoutineWriteFields(std::string& s, int ind, Routine* r, std::string (*emitTexture)(std::string& path, void* ctx) = NULL,
                          void* ctx = NULL);
void RoutineReadFields(JVal* j, Routine* r, std::string (*refTexture)(const std::string& saved, void* ctx) = NULL,
                      void* ctx = NULL);
const char* W3dRoutineModeName(int m);    // "defecto", "solido", "material", "render", "alambre", "zbuffer"

// el EDITOR engancha aca su validacion (el nucleo no la tiene): true = se puede ejecutar
extern bool (*W3dRoutineValidateHook)(Routine*);

#endif
