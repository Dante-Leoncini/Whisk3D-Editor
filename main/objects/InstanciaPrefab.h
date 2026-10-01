#ifndef INSTANCIA_PREFAB_H
#define INSTANCIA_PREFAB_H
// ============================================================================
//  InstanciaPrefab — una INSTANCIA de un PREFAB del proyecto (ObjectType::prefab).
//
//  Un prefab es una "clase": un objeto raiz con sus hijos (modelo, esqueleto, armas, hitbox,
//  particulas, scripts...) que vive en su propia raiz del proyecto (W3dRaices.h) y se guarda en
//  prefabs/<slug>.w3dp. La instancia es el nodo de la escena que lo usa:
//    - sus HIJOS SE GENERAN desde la definicion del prefab (io/Prefabs.h) y NO SE GUARDAN: el
//      nodo se escribe solo como {"tipo":"prefab", "prefab":"Enemigo", transform, "overrides"}.
//      Lo generado comparte los recursos del prefab (mallas, animsets, materiales, texturas,
//      scripts): 50 enemigos = 1 malla y 1 animset en memoria;
//    - es FRONTERA DEL ESPACIO DE NOMBRES (W3dEsFronteraScope, Objects.h): lo generado conserva
//      los nombres del prefab tal cual, y todo lo que se resuelve por nombre desde adentro
//      (constraints, modArmature, targets, refs de scripts, buscar() de lua) mira primero adentro
//      de SU instancia y despues afuera;
//    - OVERRIDES (lo unico propio que guarda ademas de su transform):
//        propiedades  valor por NOMBRE de propiedad de script: se aplica a todos los scripts de
//                     la instancia que la declaren (la vida de ESTE enemigo), tambien a los de las
//                     instancias anidadas que el prefab trae adentro;
//        visible      por RUTA del hijo (relativa al objeto raiz generado, como las de los clips de
//                     jerarquia: "Cuerpo/Arma"; "." = el objeto raiz). La ruta puede atravesar una
//                     instancia anidada ("Enemigo/Enemigo/Esqueleto/Cuerpo").
//  En el editor lo generado se ve en el outliner (en gris) y se puede elegir, pero su estructura no
//  se toca (borrar / reparentar / duplicar un hijo generado esta bloqueado). Editar el prefab (su
//  raiz, desde el selector de escena) y volver REGENERA todas sus instancias.
//    - CARGA (io/Streaming.h): "siempre" (el default: genera al abrir, como siempre) o "distancia":
//      una instancia DIFERIDA genera sus hijos recien cuando su OBJETIVO (un objeto por nombre; sin
//      nombre, la camara activa) se acerca a menos de 'distancia' metros, y los destruye cuando se
//      aleja a mas de 'distancia' + 10%. El nodo guarda "carga", "distancia" y "objetivo" (solo las
//      diferidas: una instancia de siempre se escribe igual que antes).
//
//  Compartido editor/juego (C++03: Symbian lo compila). Motor generico.
// ============================================================================
#include "objects/Objects.h"
#include <map>
#include <string>
#include <vector>
#include <utility>

struct JVal;   // io/JsonW3d.h
struct W3dRecurso;   // io/W3dRecursos.h (los pedidos del streaming)

// COMO SE CARGA una instancia (io/Streaming.h)
enum { W3D_CARGA_SIEMPRE = 0, W3D_CARGA_DISTANCIA = 1 };
// en que esta su streaming (no se guarda)
enum { W3D_STREAM_DESCARGADA = 0, W3D_STREAM_PIDIENDO = 1, W3D_STREAM_CARGADA = 2 };
// UN PEDIDO del streaming al almacen: el descriptor CON SU SERIE (se valida con W3dRecursoVigente antes de tocarlo:
// la biblioteca puede purgar una textura, o el cierre olvidar todo, con el pedido todavia en la instancia)
struct W3dPedidoStream { W3dRecurso* r; unsigned serie; W3dPedidoStream() : r(0), serie(0) {} };

// lo GENERADO como salio de la definicion (antes de aplicarle los overrides): los overrides son lo que difiere
// de esto (W3dPrefabSincronizarOverrides, io/Prefabs.h). Un objeto por entrada, por serial (se puede liberar).
// Entra TODO lo generado, tambien lo de las instancias anidadas (es parte de lo que el prefab trae adentro).
struct InstanciaBase {
    Object* o;
    unsigned serial;
    bool visible;
    std::vector<std::vector<std::pair<std::string, std::string> > > refs;   // los valores de sus scripts
    std::vector<std::string> rutas;   // el .lua de cada script (paralelo a refs): los scripts se comparan POR
                                      // RUTA, no por indice (uno agregado o quitado no corre a los demas)
    std::string ruta;                 // su ruta desde el objeto raiz generado (la de los overrides de visible): si
                                      // deja de existir (una anidada que el streaming descargo) su override sigue
    InstanciaBase() : o(0), serial(0), visible(true) {}
};

class InstanciaPrefab : public Object {
public:
    std::string prefab;                           // el prefab que genera (su nombre en el registro "prefabs")
    std::map<std::string, std::string> overProps; // OVERRIDE: propiedad de script -> valor (como texto)
    std::map<std::string, bool> overVisible;      // OVERRIDE: ruta del hijo generado -> visible
    unsigned versionGenerada;                     // la version de la definicion con la que se generaron los
                                                  // hijos (0 = nunca se genero; ver io/Prefabs.h)
    bool noGenerada;                              // el prefab no existe (o no se pudo leer): la instancia queda
                                                  // vacia y se dibuja como una cruz (se guarda igual)
    std::vector<InstanciaBase> base;              // lo generado tal cual salio (ver InstanciaBase; no se guarda)
    // ---- STREAMING (io/Streaming.h): lo que se GUARDA ----
    int   carga;                                  // W3D_CARGA_SIEMPRE / W3D_CARGA_DISTANCIA
    float distancia;                              // (distancia) metros: carga mas cerca, descarga a mas del 110%
    std::string objetivo;                         // (distancia) el objeto que se mide ("" = la camara activa)
    // ...y su estado en la sesion (NO se guarda; lo maneja io/Streaming.cpp)
    int   streamEstado;                           // W3D_STREAM_DESCARGADA / _PIDIENDO / _CARGADA
    int   streamFijado;                           // 0 = decide la distancia; 1 = cargar(obj) de lua; -1 = descargar(obj)
    int   streamFase;                             // (pidiendo) 0 = mallas/animsets/scripts; 1 = ademas las texturas
    std::vector<W3dPedidoStream> streamPedidos;   // las referencias que pidio al almacen (se sueltan al descargarse)
    InstanciaPrefab(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ~InstanciaPrefab();
    ObjectType getType() W3D_OVERRIDE { return ObjectType::prefab; }
    void RenderObject() W3D_OVERRIDE;
    // el objeto RAIZ generado (el primer hijo; NULL = sin generar)
    Object* RaizGenerada() const;
};

// los campos propios en el .w3d ("tipo":"prefab"): el escritor agrega ESTO despues de los comunes
// (cada linea empieza con ",\n" + sangria 'ind'), y el lector los lee de 'j' (sin generar nada)
void InstanciaPrefabEscribirCampos(std::string& s, int ind, const InstanciaPrefab* ip);
void InstanciaPrefabLeerCampos(JVal* j, InstanciaPrefab* ip);

#endif
