#ifndef W3D_STREAMING_H
#define W3D_STREAMING_H
// ============================================================================
//  Streaming — CARGAR Y DESCARGAR EN TIEMPO REAL las instancias de prefab y los
//  proxies (objects/InstanciaPrefab.h, objects/ProxyW3d.h) segun la distancia a un
//  OBJETIVO. Es lo que deja abrir un nivel grande sin tener todo en memoria.
//
//  UNA INSTANCIA DIFERIDA ("carga": "distancia" en su nodo del .w3d, con "distancia" en
//  metros y "objetivo" = el nombre de un objeto; sin objetivo, la camara activa) no
//  genera sus hijos al abrir el juego: W3dStreamingTick (una vez por frame) los genera
//  cuando el objetivo se ACERCA a menos de 'distancia' de la instancia y los destruye
//  cuando se ALEJA a mas de 'distancia' + 10% (la HISTERESIS: parado en el borde no carga
//  y descarga en cada frame). "siempre" (el default) es la instancia de siempre.
//
//  CARGAR es ASINCRONICO y va de a TAJADAS por frame:
//    1. PEDIR: los recursos que nombra la definicion (io/Prefabs.h: W3dPrefabRecursos; tambien
//       los de los prefabs anidados) se piden ASYNC al almacen (io/W3dRecursos.h): las mallas,
//       los animsets y las FUENTES de los scripts quedan EN_VUELO y la bomba los carga.
//    2. TEXTURAS: con las mallas LISTAS se sabe que materiales dibujan: las texturas dormidas
//       de esos materiales se piden igual (async).
//    3. GENERAR: con todo LISTO (o FALLADO: la generacion lo avisa en el log) se arman los hijos
//       (W3dPrefabGenerar: armar los nodos no lee ni reprocesa nada), sus materiales despiertan sin
//       esperar la cola diferida (sus texturas ya estan en memoria) y, con una partida andando,
//       arrancan como lo que crea instanciar() de lua (scripts, cuerpos rigidos, hitbox).
//  La instancia RETIENE lo que pidio mientras este cargada. Por frame se bombean a lo sumo
//  'cargasPorFrame' recursos, se generan 'generarPorFrame' instancias (las mas cercanas primero)
//  y se descargan 'descargarPorFrame'; ademas, pasado el PRESUPUESTO de tiempo del frame
//  ('presupuestoMs') no se empieza otra carga ni otra generacion (la primera de cada una va
//  siempre: el streaming nunca se traba). Una textura grande que tarda se come su frame y la
//  siguiente espera al proximo: el tiron maximo es UNA carga, nunca una tanda.
//
//  DESCARGAR va por la RUTA de destruir() de lua (W3dScriptDestruirObjeto: sus scripts, su fisica,
//  sus refs, sus curvas; con la partida andando, la partida lo saca antes de sus listas y de sus
//  fotos -script/W3dScript.h: W3dScriptLiberarYa-; sin partida, se suelta como al regenerar). A
//  diferencia de destruir(), en el Play del editor lo que el usuario tenia generado desde antes no
//  se descuelga hasta el Stop: se LIBERA (sale de la definicion) y el Stop lo vuelve a generar. Despues
//  la instancia SUELTA lo que pidio: el refcount del almacen libera en el acto la RAM y los VBO de
//  las mallas y animsets que nadie mas usa, y los materiales que quedaron sin ninguna malla que los
//  dibuje DUERMEN sus texturas (importers/import_obj.h: TexturasAdormecer): la textura que ningun
//  otro material retiene sale de la GPU.
//
//  DONDE CORRE (en el mismo lugar del frame en los dos: al final del paso, antes del fin de
//  frame de los scripts):
//    - el Play del editor (main/script/SimJuego.cpp, TickReal) y el juego compilado
//      (game/w3drun.cpp, W3dGameActualizar). La partida ARRANCA con las que estan cerca ya
//      generadas (W3dStreamingPartidaInicio, bloqueante: el primer frame esta completo);
//    - el EDITOR sin jugar, con la VISTA PREVIA del streaming prendida (opcion del proyecto,
//      "streamingVistaPrevia" en proyecto.json): el objetivo por defecto es la vista del viewport.
//  Sin vista previa el editor genera todas al abrir, como siempre (se ve y se edita el nivel
//  entero); al dar Play las diferidas se sueltan y el streaming toma el control (igual que el
//  juego compilado, que no las genera al abrir); el Stop las deja como estaban.
//
//  LUA (script/W3dScript.h): cargar(obj) / descargar(obj) FIJAN el estado de una instancia o un
//  proxy (la distancia deja de decidir; se aplica en el tick, al final del frame: un script puede
//  descargar la instancia que lo genero), cargaAuto(obj) se lo devuelve a su carga, cargado(obj)
//  dice si sus hijos estan generados.
//
//  Compila en el editor y en el runtime 3D (C++03). Motor generico.
// ============================================================================
#include <string>
#include <vector>
#include "math/Vector3.h"

class Object;
class InstanciaPrefab;

// ---- LA CONFIGURACION (de a cuanto por frame) ----
struct W3dStreamingConfig {
    int   cargasPorFrame;     // recursos que bombea el tick (W3dRecursosPump) mientras haya pedidos en vuelo
    int   generarPorFrame;    // instancias que genera por frame (las mas cercanas primero)
    int   descargarPorFrame;  // instancias que descarga por frame
    float histeresis;         // fraccion de la distancia: se descarga a mas de distancia * (1 + histeresis)
    float presupuestoMs;      // tiempo del frame: pasado esto no empieza otra carga ni otra generacion (0 = sin tope)
};
extern W3dStreamingConfig g_w3dStreamingConfig;
// la VISTA PREVIA del streaming en el editor (opcion del proyecto: "streamingVistaPrevia" en proyecto.json)
extern bool g_w3dStreamingVistaPrevia;
// prende/apaga la vista previa: al APAGARLA las diferidas descargadas se generan todas (el editor vuelve a ver el
// nivel entero, como sin streaming)
void W3dStreamingVistaPreviaFijar(bool on);

// ---- EL TICK (una vez por frame) ----
void W3dStreamingTick();
// la del EDITOR (main.cpp, por frame): el tick solo si la vista previa esta prendida y no se juega
void W3dStreamingTickEditor();

// ---- LA CARGA DE UN NIVEL (importers/import_w3d.cpp) ----
// la instancia recien leida se genera YA (false) o espera al streaming (true): una DIFERIDA en el juego compilado, o
// en el editor con la vista previa prendida
bool W3dStreamingDiferirAlCargar(const InstanciaPrefab* ip);

// ---- LA PARTIDA ----
// el Play del editor ARRANCA (antes de las fotos y de juntar los scripts): las diferidas de la raiz activa sueltan lo
// que generaron (quedan como en el juego compilado recien abierto) y se anotan para el Stop. Lo llama tambien el
// cambio de escena 3D jugando (la raiz nueva)
void W3dStreamingPartidaPreparar();
// la partida ya arranco sus scripts: las instancias que tienen que estar cargadas se generan YA (bloqueante: el
// primer frame esta completo) y arrancan como lo instanciado. Editor (SimPlay, cambiarEscena) y juego compilado
// (W3dGameInicio)
void W3dStreamingPartidaInicio();
// el Stop del editor (despues de que la partida libero lo que creo): se olvidan los pedidos y las diferidas que el
// Play solto se vuelven a generar (la escena del usuario queda como estaba)
void W3dStreamingPartidaFin();

// ---- LUA: cargar/descargar/cargaAuto/cargado (el hook de script/W3dScript.h) ----
// pedido: 1 cargar (fija), -1 descargar (fija), 2 automatica, 0 consultar. Devuelve W3D_STREAM_* o -1 (no es instancia)
int  W3dStreamingPedido(Object* o, int pedido);

// ---- UNA INSTANCIA SE VA (su destructor) o EL PROYECTO SE CIERRA ----
void W3dStreamingOlvidarInstancia(InstanciaPrefab* ip);
void W3dStreamingCerrarProyecto();
// la configuracion de carga de 'ip' cambio (la tarjeta de Properties, el undo): el tick la vuelve a mirar. Si dejo
// de ser diferida y estaba descargada, se genera ya
void W3dStreamingInstanciaCambiada(InstanciaPrefab* ip);

// los ids de los recursos de 'tipo' que retienen los pedidos del streaming (uno por instancia que lo pidio): son
// USUARIOS de verdad (la biblioteca no borra una textura que una instancia cargada tiene pedida)
void W3dStreamingRecursosRetenidos(int tipo, std::vector<std::string>& ids);

// ---- EL OBJETIVO ----
// el harness (y quien quiera fijarlo): el objetivo por defecto pasa a ser este punto (on=false lo suelta)
void W3dStreamingObjetivoForzado(bool on, const Vector3& pos);
// el reloj para medir el tick (W3dNowMs en el editor; NULL = no se mide: el juego compilado)
extern double (*W3dStreamingReloj)();

// ---- MEDICION (harness / Properties) ----
struct W3dStreamingStats {
    int instancias;           // las que mira el tick (diferidas + fijadas por lua) en la raiz activa
    int cargadas, pidiendo, descargadas;
    int pedidosVivos;         // referencias que retienen entre todas
    long generadasTotal, descargadasTotal;   // desde el ultimo reset
    double tickMs, tickPeorMs;               // el ultimo tick y el peor desde el reset (0 sin reloj)
    W3dStreamingStats() : instancias(0), cargadas(0), pidiendo(0), descargadas(0), pedidosVivos(0),
                          generadasTotal(0), descargadasTotal(0), tickMs(0.0), tickPeorMs(0.0) {}
};
void W3dStreamingEstadisticas(W3dStreamingStats& st);
void W3dStreamingStatsReset();

#endif
