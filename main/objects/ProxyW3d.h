#ifndef W3D_PROXY_W3D_H
#define W3D_PROXY_W3D_H
// ============================================================================
//  ProxyW3d — un PROXY W3D (ObjectType::proxy): el nodo de la escena que usa un PREFAB
//  o una ESCENA de una LIBRERIA externa (otro .w3d de solo lectura, io/Librerias.h).
//
//  Es una instancia de prefab (objects/InstanciaPrefab.h) con otro origen: sus hijos se
//  GENERAN con la MISMA maquinaria (io/Prefabs.h: definicion cacheada, nombres crudos,
//  frontera de scope, overrides de visible y de propiedades de scripts) desde la
//  definicion del elemento de la libreria, pero son de SOLO LECTURA:
//    - no se guardan (el nodo se escribe solo como {"tipo": "proxy", "libreria",
//      "elemento", "elementoTipo"?, transform, "overrides"?});
//    - no se editan (lo generado no se toca, como en una instancia; ademas sus mallas,
//      materiales y animsets son de la libreria: Edit Mode y los pinceles no entran);
//    - sus recursos NO se ingieren en el proyecto: viven en la libreria con su prefijo
//      ("Personajes/Cuerpo") y N proxies del mismo personaje los comparten.
//  Cambiar la libreria en disco y reabrir el nivel lo regenera con lo nuevo.
//
//  LA CLAVE: 'prefab' (el campo de la instancia) guarda la clave del elemento en la
//  cache de definiciones: "lib:<libreria>/prefab|escena/<elemento>" (W3dLibsClave). La
//  libreria, el elemento y su tipo salen de ahi (asi el undo de "cambiar el elemento"
//  de una instancia sirve igual para un proxy).
//
//  Compartido editor/juego (C++03). Motor generico.
// ============================================================================
#include "objects/InstanciaPrefab.h"
#include <string>

class ProxyW3d : public InstanciaPrefab {
public:
    ProxyW3d(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ObjectType getType() W3D_OVERRIDE { return ObjectType::proxy; }
    // lo que genera (salen de la clave 'prefab'; vacios = sin elegir)
    std::string Libreria() const;
    std::string Elemento() const;
    int TipoElemento() const;          // W3D_LIB_PREFAB / W3D_LIB_ESCENA (io/Librerias.h)
    // fija la libreria y el elemento (NO regenera: eso es de quien lo llama)
    void Fijar(const std::string& libreria, int tipo, const std::string& elemento);
};

// la CLAVE de un proxy de 'elemento' de 'libreria' (W3dLibsClave; con libreria y sin elemento, una PARCIAL
// "lib:<libreria>/prefab/" que recuerda la libreria elegida)
std::string W3dProxyClave(const std::string& libreria, int tipo, const std::string& elemento);

// los campos propios en el .w3d ("tipo":"proxy"): el escritor agrega ESTO despues de los comunes (cada linea
// empieza con ",\n" + sangria 'ind'), y el lector los lee de 'j' (sin generar nada)
void ProxyW3dEscribirCampos(std::string& s, int ind, const ProxyW3d* px);
void ProxyW3dLeerCampos(JVal* j, ProxyW3d* px);

#endif
