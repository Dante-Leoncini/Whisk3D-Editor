#ifndef CULLING_H
#define CULLING_H
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner

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

    Culling(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "Culling", pos) {
        activo = true;
        soloCamaraActiva = false;
        distanciaMax = 0.0f;
    }
    ObjectType getType() override { return ObjectType::culling; }
    void RenderHijos() override;
};
#endif
