#ifndef CULLING_H
#define CULLING_H
#include "objects/Objects.h"
#include "objects/VisSet.h"     // metodo Riel: visibilidad de hijos por nodo (dato .w3dvis)
#include "objects/VisBsp.h"     // metodo Bsp: arbol BSP + PVS + clusters por hijo (dato .w3dbsp)
#include "objects/WorldBsp.h"   // metodo Bsp: el MUNDO del mapa (geometria del .w3dbsp v2) dibujado por el motor
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner
#include <map>
#include <vector>
#include <string>

// ============================================================================
//  Objeto Culling: frustum culling de sus HIJOS por AABB. Cada hijo se testea
//  con el AABB (en mundo) de las mallas de su subarbol -el AABB local vive
//  cacheado en Mesh::aabbMin/aabbMax, recalculado por ComputeBorders en cada
//  cambio de geometria- contra los 6 planos del frustum (Gribb-Hartmann sobre
//  proyeccion*vista). Un hijo sin mallas no se corta nunca (no hay que medir).
//
//  Contrato del .w3d de texto:
//      Culling { activo: true|false  onlyCameraActive: true|false  distanceMax: 0 <hijos> }
//   - activo true (default): recorta. false: dibuja TODOS los hijos sin medir nada
//     (checkbox "Active" del panel; ver el campo de abajo).
//   - onlyCameraActive false (default): el frustum es el de la VISTA QUE DIBUJA
//     (en el editor, el viewport; en el juego, la camara activa) -> el cull del
//     juego sale gratis porque el juego dibuja mirando por su camara.
//   - onlyCameraActive true: el frustum se arma SIEMPRE desde la Camera ACTIVA
//     de la escena, aunque el viewport mire desde otro lado: sirve para VER
//     desde afuera, en el editor, exactamente que cortaria el juego.
//     La designacion SE HEREDA al subarbol mientras se dibuja: un LOD o un
//     Culling anidado adentro mide desde la MISMA camara designada, en TODOS
//     los viewports por igual (sin esto, el recorte del subarbol quedaba
//     "parcialmente independiente": los hijos con medida propia seguian a la
//     camara del viewport que dibujaba).
//   - distanceMax (0 = sin limite): culling por DISTANCIA, ademas del frustum.
//     Un hijo cuyo AABB queda ENTERO mas lejos que distanceMax de la camara no
//     se dibuja. Es el corte que el frustum no puede hacer en un nivel pasillo
//     (nivel pasillo): todos los trozos quedan alineados con la mirada y el frustum
//     solo no corta nada; la distancia si.
//
//  Ademas ordena los hijos VISIBLES de adelante hacia atras (distancia del
//  centro de su AABB a la camara): el z-buffer rechaza temprano lo tapado
//  (early z-rejection) y el fill-rate baja en escenas con mucho solape.
// ============================================================================
// estadistica de frame (bench): hijos testeados vs. hijos que PASARON el cull en
// todos los Culling del frame. Los resetea el que mide (comando 'bench').
extern int g_cullChildrenTotal;
extern int g_cullVisibleChildren;

// ---- LA CAMARA DESDE LA QUE SE MIDE, compartida por Culling y LOD ----
// Devuelve la Camera ACTIVA de la escena si hay que medir desde el JUEGO (el objeto
// lo pidio con onlyCameraActive, O el juego esta corriendo), y NULL si hay que medir
// desde la vista QUE DIBUJA (g_renderCam*).
// IMPORTANTE: esto NO ESCRIBE NADA del viewport. Reporte del dueno: "cuando aprieto
// play no quiero que me modifiques el paneo/zoom de la camara activa" -- para que
// jugando se midiera desde la camara del juego se habia terminado tocando la vista
// del viewport; ahora LOD y Culling la piden aca y el viewport queda intacto.
class Camera;
class Texture;
Camera* W3dMeasureCamera(bool onlyCameraActive);
// AABB vs el frustum de la camara de medida (juego jugando / vista que dibuja);
// true si es visible o si no hay lente real (headless). Lo usa el Mirror para
// autocullearse por el rectangulo del agua.
bool W3dAabbVisible(const Vector3& mn, const Vector3& mx);
// (la Rutina) los planos de esa camara UNA vez (false = no cortar: orto o sin vista) y el test contra ellos
bool W3dFrustumMeasurePlanes(float planes[24], float* posCamera = 0);   // (posCamera: 3 floats, opcional)
bool W3dAabbInPlanes(const float planes[24], const Vector3& mn, const Vector3& mx);

// HERENCIA de la designacion (ver Culling.cpp): mientras un Culling/LOD con
// onlyCameraActive dibuja su subarbol, el contador queda > 0 y todo el que mida
// adentro usa la misma camara designada. RAII para no dejarlo colgado jamas.
extern int g_onlyActiveCameraInheritance;
struct W3dInheritanceMeasure {
    bool active;
    W3dInheritanceMeasure(bool on) : active(on) { if (active) g_onlyActiveCameraInheritance++; }
    ~W3dInheritanceMeasure() { if (active) g_onlyActiveCameraInheritance--; }
};

// ---- las dos primitivas de medida, compartidas con el LOD ----
// AABB en MUNDO del subarbol de 'o' (union de las mallas VISIBLES). false = no hay
// ni una malla medible. Es lo que hace que el corte se decida por la GEOMETRIA y no
// por el origen del nodo (un trozo grande tiene su origen lejos de su propia cara).
bool W3dBoundsSubtree(Object* o, Vector3& mn, Vector3& mx);
// distancia^2 del punto 'p' al AABB [mn, mx] (0 si 'p' esta adentro).
float W3dDist2PointAabb(const Vector3& p, const Vector3& mn, const Vector3& mx);

class Culling : public Object {
public:
    // METODO de culling (selector del panel): el objeto es UNO solo y elige COMO recorta.
    //   Frustum   = por AABB de cada hijo contra el cono de la camara (el clasico de siempre).
    //   Grid      = particion espacial por CELDAS (estilo GTA/RenderWare): descarta celdas
    //               ENTERAS por frustum+distancia; los hijos ESTATICOS se cachean en su celda,
    //               los DINAMICOS (Object::estatico==false, ej. Crash/enemigos) se miden por
    //               frame. El boton "Recalcular" (MarkDirty) rearma el reparto.
    //   Bsp       = PVS precalculado estilo Quake/Half-Life (dato .w3dbsp, formato/w3dbsp.md): la camara baja
    //               por el arbol de planos hasta su hoja -> cluster -> fila del PVS; un hijo se dibuja si
    //               alguno de sus clusters esta en esa fila (AND de bitsets), y despues pasa por el frustum
    //               de siempre. Sin dato / camara en solido: frustum. Los hijos que no estan en el dato se
    //               dibujan siempre (objetos dinamicos).
    // (El viejo "Triangulo" SE FUE de este objeto -pedido del dueno-: el contenedor
    //  muestra/oculta OBJETOS hijos, estilo Unity. El PVS por triangulo es asunto del
    //  modificador "Oclusion" (CullingTri) de cada malla; un .w3d viejo con
    //  metodo:"triangulo" cae a Frustum solo, via CullingMethodFrom.)
    //   Riel      = oclusion de OBJETOS por nodo de riel, estilo NSD de PS1 (pedido del
    //               dueno): el dato precalculado dice que HIJOS se ven desde cada nodo del
    //               recorrido, y el render usa una LISTA CACHEADA (listRender) en vez de
    //               recorrer los hijos uno por uno. Cero calculo por frame: nearest de
    //               camara -> celda -> lista, todo dato.
    enum Method { Frustum = 0, Grid = 1, Bsp = 3, Rail = 4 };   // el 2 (Triangulo) queda RESERVADO: no reusar
    int   method;           // default Frustum (retrocompat: los .w3d viejos no traen 'metodo')

    // INTERRUPTOR del recorte (checkbox "Active" del panel de propiedades).
    // Apagado, el objeto dibuja TODOS sus hijos: no se recorta ni por frustum ni
    // por distancia. Pedido del dueno, textual: "el objeto culling dame la opcion
    // de desactivarlo con un checkbox, mi idea es que desde la camara activa del
    // editor hacer zoom y poder activarlo y desactivarlo y poder ver como los
    // triangulos que quedan fuera de la camara 4:3 desaparecen ... y al desactivar
    // el culling poder volver a ver tooodo el escenario".
    // Es un flag de RENDER: no toca el arbol, ni la seleccion, ni el guardado de
    // los hijos, asi que se puede prender/apagar en vivo durante una demo.
    bool  active;           // default true (los .w3d viejos no lo traen)
    bool  onlyCameraActive; // ver arriba. Default false.
    float distanceMax;      // culling por distancia (unidades de mundo). 0 = sin limite.
    // TRANSLUCIDO: si true, los hijos VISIBLES se ordenan ATRAS->ADELANTE (por profundidad, sin agrupar por
    // material) para que el alpha se mezcle bien -> reemplaza al `sortByCamera` de una Collection PERO
    // ademas cullea por frustum. Default false (opacos: orden por material + forward->back para early-z).
    bool  orderAlpha;
    // OPACOS SOLO POR DISTANCIA: true = forward->back puro (el mas cercano primero), sin agrupar por material.
    // Para escenarios en trozos (chunks con varias texturas cada uno) agrupar por la PRIMERA textura no ahorra
    // binds y rompe el orden de profundidad; forward->back deja que el z-buffer descarte lo tapado antes de
    // pintarlo (menos relleno, que en el N95 es lo caro). Default false (el orden de siempre).
    bool  orderProximity;
    // --- metodo Grid: particion por celdas ---
    float cellSize;         // lado de la celda en unidades de mundo (default 16)
    bool  mode3D;           // false = grilla 2D en el plano XZ (default, niveles); true = 3D

    // --- metodo Riel: visibilidad de HIJOS por nodo del recorrido (dato .w3dvis) ---
    std::string railName;           // la Curve del recorrido (por nombre; el tick la resuelve)
    std::string visChildrenFile;    // .w3dvis: celda = nodo -> lista de INDICES DE HIJOS visibles
    VisSet      visChildren;        // el dato cargado (por valor)
    bool        visChildrenLoaded;  // ya se intento leer (no reintentar por frame)
    int         appliedNode;        // nodo materializado en listRender (-1/0 = ninguno: frustum)
    std::vector<unsigned> visChildrenList;  // lista de la celda aplicada (para los deltas +-1)
    std::vector<Object*>  listRender;       // LO QUE SE DIBUJA, en el orden del dato (cache)
    // elige/materializa el nodo (lo llama W3dOcclusionTick con el ojo que corresponda);
    // true = la lista cambio (redibujar)
    bool RailApplyNode(int node);

    // --- metodo Bsp: PVS por cluster (dato .w3dbsp) ---
    std::string bspFile;          // el .w3dbsp (ruta relativa al proyecto, como visChildrenFile)
    VisBsp      bsp;              // el dato cargado
    bool        bspLoaded;        // ya se intento leer (no reintentar por frame)
    int         bspCluster;       // cluster de la camara en el ultimo frame (-1 = ninguno: frustum)
    std::vector<unsigned char> bspRow;    // la fila del PVS de ese cluster
    std::vector<int>  bspChildIdx; // por hijo: su indice en bsp.children (-1 = no esta: siempre visible)
    std::vector<char> bspVisible;  // por hijo: se ve desde bspCluster
    size_t            bspChildrenStamp; // Childrens.size() con que se armo bspChildIdx
    // el PVS para las RUTINAS: con un nombre, cada cuadro escribe 1/0 por hijo i (en su orden) en la MEMORIA
    // @<nombre><i/256>[i%256] (main/objects/Routine.h): una rutina que dibuja el mundo horneado en mallas unidas saltea
    // las celdas que el BSP no ve ("Saltar si" con esa memoria). Sin cluster (fuera del arbol) va todo en 1.
    // Clave "visibleMemory" del .w3d
    std::string visibleMemory;
    void BspWriteMemory();
    bool BspLoad();             // lee el archivo (una vez); true si el dato es valido
    // EL MUNDO (.w3dbsp version 2, formato/w3dbsp.md): la geometria del mapa, por hoja y por material, que este
    // Culling dibuja ANTES de sus hijos con el mismo PVS y el frustum por el arbol (WorldBsp): un mapa de Quake /
    // Half-Life entero sin un objeto por pieza. Las texturas de sus materiales se toman del cache (worldTex) y
    // entran al contenedor al guardar como las de un material. Sin geometria en el dato, el Culling solo cullea
    WorldBsp              world;
    std::vector<Texture*> worldTex;     // por material: la textura base y la pagina del lightmap (2 por material)
    std::vector<unsigned char> bspBitsTmp;   // (clusters de un hijo dinamico, por cuadro)
    void WorldRelease();        // suelta las texturas tomadas y la geometria
    void WorldDraw(int phase);  // dibuja el mundo con el PVS y el frustum vigentes (RenderBsp: 1 opaco antes de los hijos, 2 mezclado despues)

    Culling(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "Culling", pos) {
        method = Frustum;
        active = true;
        onlyCameraActive = false;
        distanceMax = 0.0f;
        orderAlpha = false;
        orderProximity = false;
        cellSize = 16.0f;
        mode3D = false;
        visChildrenLoaded = false;
        appliedNode = -1;
        bspLoaded  = false;
        bspCluster = -1;
        bspChildrenStamp = (size_t)-1;
        gridDirty = true;
        frusPlayed = false;
        stamp = 0;
    }
    ~Culling() { WorldRelease(); }
    ObjectType getType() W3D_OVERRIDE { return ObjectType::culling; }
    void RenderChildren() W3D_OVERRIDE;          // dispatcher por metodo
    // boton "Recalcular": rearma la grilla del metodo Grid y el cache del Frustum
    void MarkDirty() { gridDirty = true; frusPlayed = false; }

private:
    struct Cell    { std::vector<int> children; Vector3 mn, mx; }; // AABB de mundo = union de sus hijos
    struct CellVis { Cell* c; float d2; };  // celda visible + su distancia2 a la camara (para ordenar)
    bool                        gridDirty;
    // CACHE del metodo FRUSTUM para hijos ESTATICOS (Object::estatico): el AABB
    // de subarbol (W3dBoundsSubtree = DFS + 8 esquinas por malla) y la matKey
    // se calculaban POR HIJO POR FRAME aunque nada se moviera (110 hijos en el
    // "Transparentes" de Crash, y de nuevo en el pase del espejo). El AABB se
    // guarda DILATADO a la caja de su esfera: los billboards (frutas) giran en
    // su lugar y cualquier rotacion queda adentro. Solo se usa JUGANDO (en el
    // editor se mide por frame: mover un objeto se ve al instante); se limpia
    // al ARRANCAR cada play (frusPlayed detecta el flanco) y con Recalcular.
    struct FrusChild {
        bool has, valid; Vector3 mn, mx; unsigned matKey;
        FrusChild() : has(false), valid(false), matKey(0) {}
    };
    std::vector<FrusChild>       frusCache;
    bool                        frusPlayed;   // estado del flanco parado->playing
    unsigned                    stamp;          // stamp de frame: no dibujar 2 veces un hijo multi-celda
    std::map<long long, Cell>  cells;           // clave (cx,cy,cz) -> celda (solo hijos ESTATICOS)
    std::vector<int>            dynamic;        // hijos DINAMICOS (estatico==false): fuera de la grilla
    std::vector<unsigned>       stampChild;     // ultimo frame en que se dibujo cada hijo
    void RebuildGrid();      // reparte los hijos ESTATICOS en celdas (metodo Grid)
    // metodo Frustum (y la 2da mitad de Bsp): cull por AABB de cada hijo. 'filtro' (opcional): por hijo, 0 = ya
    // descartado por el PVS (no se mide ni se dibuja)
    void RenderFrustum(const std::vector<char>* filter = NULL);
    void RenderGrid();       // metodo Grid: descarta celdas enteras + hijos dinamicos por frame
    void RenderBsp();        // metodo Bsp: PVS del cluster de la camara + frustum
};

// (de)serializacion del metodo (texto/JSON): "frustum"/"grid"/"triangulo"/"bsp".
// CullingMethodFrom devuelve Frustum si el string no matchea o falta (retrocompat).
const char* CullingMethodName(int m);
int         CullingMethodFrom(const std::string& s);
#endif
