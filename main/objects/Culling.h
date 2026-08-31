#ifndef CULLING_H
#define CULLING_H
#include "objects/Objects.h"
#include "objects/VisSet.h"     // metodo Riel: visibilidad de hijos por nodo (dato .w3dvis)
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner
#include <map>
#include <vector>
#include <string>

// ============================================================================
//  Objeto Culling: frustum culling de sus HIJOS por AABB. Cada hijo se testea
//  con el AABB (en mundo) de las mallas de su subarbol -el AABB local vive
//  cacheado en Mesh::aabbMin/aabbMax, recalculado por CalcularBordes en cada
//  cambio de geometria- contra los 6 planos del frustum (Gribb-Hartmann sobre
//  proyeccion*vista). Un hijo sin mallas no se corta nunca (no hay que medir).
//
//  Contrato del .w3d de texto:
//      Culling { activo: true|false  soloCamaraActiva: true|false  distanciaMax: 0 <hijos> }
//   - activo true (default): recorta. false: dibuja TODOS los hijos sin medir nada
//     (checkbox "Active" del panel; ver el campo de abajo).
//   - soloCamaraActiva false (default): el frustum es el de la VISTA QUE DIBUJA
//     (en el editor, el viewport; en el juego, la camara activa) -> el cull del
//     juego sale gratis porque el juego dibuja mirando por su camara.
//   - soloCamaraActiva true: el frustum se arma SIEMPRE desde la Camera ACTIVA
//     de la escena, aunque el viewport mire desde otro lado: sirve para VER
//     desde afuera, en el editor, exactamente que cortaria el juego.
//     La designacion SE HEREDA al subarbol mientras se dibuja: un LOD o un
//     Culling anidado adentro mide desde la MISMA camara designada, en TODOS
//     los viewports por igual (sin esto, el recorte del subarbol quedaba
//     "parcialmente independiente": los hijos con medida propia seguian a la
//     camara del viewport que dibujaba).
//   - distanciaMax (0 = sin limite): culling por DISTANCIA, ademas del frustum.
//     Un hijo cuyo AABB queda ENTERO mas lejos que distanciaMax de la camara no
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
extern int g_cullHijosTotal;
extern int g_cullHijosVisibles;

// ---- LA CAMARA DESDE LA QUE SE MIDE, compartida por Culling y LOD ----
// Devuelve la Camera ACTIVA de la escena si hay que medir desde el JUEGO (el objeto
// lo pidio con soloCamaraActiva, O el juego esta corriendo), y NULL si hay que medir
// desde la vista QUE DIBUJA (g_renderCam*).
// IMPORTANTE: esto NO ESCRIBE NADA del viewport. Reporte del dueno: "cuando aprieto
// play no quiero que me modifiques el paneo/zoom de la camara activa" -- para que
// jugando se midiera desde la camara del juego se habia terminado tocando la vista
// del viewport; ahora LOD y Culling la piden aca y el viewport queda intacto.
class Camera;
Camera* W3dCamaraDeMedida(bool soloCamaraActiva);
// AABB vs el frustum de la camara de medida (juego jugando / vista que dibuja);
// true si es visible o si no hay lente real (headless). Lo usa el Mirror para
// autocullearse por el rectangulo del agua.
bool W3dAabbVisible(const Vector3& mn, const Vector3& mx);

// HERENCIA de la designacion (ver Culling.cpp): mientras un Culling/LOD con
// soloCamaraActiva dibuja su subarbol, el contador queda > 0 y todo el que mida
// adentro usa la misma camara designada. RAII para no dejarlo colgado jamas.
extern int g_soloCamaraActivaHerencia;
struct W3dHerenciaMedida {
    bool activa;
    W3dHerenciaMedida(bool on) : activa(on) { if (activa) g_soloCamaraActivaHerencia++; }
    ~W3dHerenciaMedida() { if (activa) g_soloCamaraActivaHerencia--; }
};

// ---- las dos primitivas de medida, compartidas con el LOD ----
// AABB en MUNDO del subarbol de 'o' (union de las mallas VISIBLES). false = no hay
// ni una malla medible. Es lo que hace que el corte se decida por la GEOMETRIA y no
// por el origen del nodo (un trozo grande tiene su origen lejos de su propia cara).
bool W3dBoundsSubarbol(Object* o, Vector3& mn, Vector3& mx);
// distancia^2 del punto 'p' al AABB [mn, mx] (0 si 'p' esta adentro).
float W3dDist2PuntoAabb(const Vector3& p, const Vector3& mn, const Vector3& mx);

class Culling : public Object {
public:
    // METODO de culling (selector del panel): el objeto es UNO solo y elige COMO recorta.
    //   Frustum   = por AABB de cada hijo contra el cono de la camara (el clasico de siempre).
    //   Grid      = particion espacial por CELDAS (estilo GTA/RenderWare): descarta celdas
    //               ENTERAS por frustum+distancia; los hijos ESTATICOS se cachean en su celda,
    //               los DINAMICOS (Object::estatico==false, ej. Crash/enemigos) se miden por
    //               frame. El boton "Recalcular" (MarcarSucio) rearma el reparto.
    //   Bsp       = PENDIENTE (aun no implementado): hoy cae a Frustum (aviso al elegirlo).
    // (El viejo "Triangulo" SE FUE de este objeto -pedido del dueno-: el contenedor
    //  muestra/oculta OBJETOS hijos, estilo Unity. El PVS por triangulo es asunto del
    //  modificador "Oclusion" (CullingTri) de cada malla; un .w3d viejo con
    //  metodo:"triangulo" cae a Frustum solo, via CullingMetodoDesde.)
    //   Riel      = oclusion de OBJETOS por nodo de riel, estilo NSD de PS1 (pedido del
    //               dueno): el dato precalculado dice que HIJOS se ven desde cada nodo del
    //               recorrido, y el render usa una LISTA CACHEADA (listaRender) en vez de
    //               recorrer los hijos uno por uno. Cero calculo por frame: nearest de
    //               camara -> celda -> lista, todo dato.
    enum Metodo { Frustum = 0, Grid = 1, Bsp = 3, Riel = 4 };   // el 2 (Triangulo) queda RESERVADO: no reusar
    int   metodo;           // default Frustum (retrocompat: los .w3d viejos no traen 'metodo')

    // INTERRUPTOR del recorte (checkbox "Active" del panel de propiedades).
    // Apagado, el objeto dibuja TODOS sus hijos: no se recorta ni por frustum ni
    // por distancia. Pedido del dueno, textual: "el objeto culling dame la opcion
    // de desactivarlo con un checkbox, mi idea es que desde la camara activa del
    // editor hacer zoom y poder activarlo y desactivarlo y poder ver como los
    // triangulos que quedan fuera de la camara 4:3 desaparecen ... y al desactivar
    // el culling poder volver a ver tooodo el escenario".
    // Es un flag de RENDER: no toca el arbol, ni la seleccion, ni el guardado de
    // los hijos, asi que se puede prender/apagar en vivo durante una demo.
    bool  activo;           // default true (los .w3d viejos no lo traen)
    bool  soloCamaraActiva; // ver arriba. Default false.
    float distanciaMax;     // culling por distancia (unidades de mundo). 0 = sin limite.
    // TRANSLUCIDO: si true, los hijos VISIBLES se ordenan ATRAS->ADELANTE (por profundidad, sin agrupar por
    // material) para que el alpha se mezcle bien -> reemplaza al `ordenarPorCamara` de una Collection PERO
    // ademas cullea por frustum. Default false (opacos: orden por material + adelante->atras para early-z).
    bool  ordenAlpha;
    // --- metodo Grid: particion por celdas ---
    float cellSize;         // lado de la celda en unidades de mundo (default 16)
    bool  modo3D;           // false = grilla 2D en el plano XZ (default, niveles); true = 3D

    // --- metodo Riel: visibilidad de HIJOS por nodo del recorrido (dato .w3dvis) ---
    std::string rielNombre;       // la Curve del recorrido (por nombre; el tick la resuelve)
    std::string visHijosArchivo;  // .w3dvis: celda = nodo -> lista de INDICES DE HIJOS visibles
    VisSet      visHijos;         // el dato cargado (por valor)
    bool        visHijosCargado;  // ya se intento leer (no reintentar por frame)
    int         nodoAplicado;     // nodo materializado en listaRender (-1/0 = ninguno: frustum)
    std::vector<unsigned> visHijosLista;  // lista de la celda aplicada (para los deltas +-1)
    std::vector<Object*>  listaRender;    // LO QUE SE DIBUJA, en el orden del dato (cache)
    // elige/materializa el nodo (lo llama W3dOclusionTick con el ojo que corresponda);
    // true = la lista cambio (redibujar)
    bool RielAplicarNodo(int nodo);

    Culling(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "Culling", pos) {
        metodo = Frustum;
        activo = true;
        soloCamaraActiva = false;
        distanciaMax = 0.0f;
        ordenAlpha = false;
        cellSize = 16.0f;
        modo3D = false;
        visHijosCargado = false;
        nodoAplicado = -1;
        gridSucia = true;
        frusJugaba = false;
        sello = 0;
    }
    ObjectType getType() W3D_OVERRIDE { return ObjectType::culling; }
    void RenderHijos() W3D_OVERRIDE;          // dispatcher por metodo
    // boton "Recalcular": rearma la grilla del metodo Grid y el cache del Frustum
    void MarcarSucio() { gridSucia = true; frusJugaba = false; }

private:
    struct Celda    { std::vector<int> hijos; Vector3 mn, mx; }; // AABB de mundo = union de sus hijos
    struct CeldaVis { Celda* c; float d2; };  // celda visible + su distancia2 a la camara (para ordenar)
    bool                        gridSucia;
    // CACHE del metodo FRUSTUM para hijos ESTATICOS (Object::estatico): el AABB
    // de subarbol (W3dBoundsSubarbol = DFS + 8 esquinas por malla) y la matKey
    // se calculaban POR HIJO POR FRAME aunque nada se moviera (110 hijos en el
    // "Transparentes" de Crash, y de nuevo en el pase del espejo). El AABB se
    // guarda DILATADO a la caja de su esfera: los billboards (frutas) giran en
    // su lugar y cualquier rotacion queda adentro. Solo se usa JUGANDO (en el
    // editor se mide por frame: mover un objeto se ve al instante); se limpia
    // al ARRANCAR cada play (frusJugaba detecta el flanco) y con Recalcular.
    struct FrusHijo {
        bool hay, valido; Vector3 mn, mx; unsigned matKey;
        FrusHijo() : hay(false), valido(false), matKey(0) {}
    };
    std::vector<FrusHijo>       frusCache;
    bool                        frusJugaba;   // estado del flanco parado->jugando
    unsigned                    sello;         // stamp de frame: no dibujar 2 veces un hijo multi-celda
    std::map<long long, Celda>  celdas;        // clave (cx,cy,cz) -> celda (solo hijos ESTATICOS)
    std::vector<int>            dinamicos;     // hijos DINAMICOS (estatico==false): fuera de la grilla
    std::vector<unsigned>       selloHijo;     // ultimo frame en que se dibujo cada hijo
    void RebuildGrid();      // reparte los hijos ESTATICOS en celdas (metodo Grid)
    void RenderFrustum();    // metodo Frustum/Triangulo/Bsp: cull por AABB de cada hijo
    void RenderGrid();       // metodo Grid: descarta celdas enteras + hijos dinamicos por frame
};

// (de)serializacion del metodo (texto/JSON): "frustum"/"grid"/"triangulo"/"bsp".
// CullingMetodoDesde devuelve Frustum si el string no matchea o falta (retrocompat).
const char* CullingMetodoNombre(int m);
int         CullingMetodoDesde(const std::string& s);
#endif
