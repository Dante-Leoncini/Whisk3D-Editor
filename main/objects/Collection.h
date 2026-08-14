#ifndef COLLECTION_H
#define COLLECTION_H


#include "objects/Objects.h"
#ifndef W3D_SYMBIAN
    #include "WhiskUI/draw/icons.h"
#endif

// ============================================================================
//  Collection: agrupador puro del outliner... y ADEMAS puede ORDENAR el dibujo
//  de sus hijos respecto de la camara (para TRANSPARENTES: los coleccionables
//  se meten en una Collection y se dibujan del mas lejano al mas cercano, que
//  es el unico orden en que el blend queda bien).
//
//  Contrato del .w3d de texto:
//      Collection { ordenarPorCamara: true|false ordenarUnaVez: true|false <hijos> }
//   - ordenarPorCamara (default false): al renderizar, los hijos se dibujan
//     LEJOS -> CERCA respecto de la vista QUE DIBUJA (g_renderCamPos: en el
//     editor el viewport, en el juego la camara activa). Es un orden LOCAL del
//     frame, como el del objeto Culling: Childrens NO se toca (outliner/undo/
//     guardado no se enteran).
//   - ordenarUnaVez (default false): el orden se calcula UNA sola vez con la
//     camara ACTIVA del juego (CameraActive) y queda CONGELADO: orbitando en el
//     viewport se VE como quedaron ordenados (lejano -> cercano) sin que el
//     orden se recalcule. Tocar cualquiera de los dos checkboxes recalcula.
// ============================================================================
//  ---- LOTE ESTATICO (camino de render eficiente, P4) ----
//  Contrato del .w3d de texto:  Collection { lote: "estatico" <hijos> }
//  JUGANDO (solo jugando: el editor sigue dibujando normal y editable), las
//  mallas ESTATICAS del subarbol se HORNEAN en un unico juego de buffers en
//  GPU (world transform ya aplicado) con un rango de indices POR MATERIAL:
//  el escenario pasa de un draw por trozo a un draw por material. Elegible =
//  malla sin esqueleto, sin modificadores, sin anim UV, sin override de
//  visibilidad, con materiales SIMPLES (opacos, sin chrome/normal-map/capas/
//  mezclas/decal). Lo no elegible se dibuja normal, en su lugar del arbol.
//  El lote asume los transforms QUIETOS (por eso "estatico"): mover un hijo
//  jugando no se refleja hasta re-armar (la membresia -visibilidad, altas y
//  bajas- se revalida por frame y re-arma sola; al salir del juego se libera).
struct W3dLoteEstatico; // definido en Collection.cpp

class Collection : public Object {
public:
    // Constructor
    Collection(Object* parent = NULL, Vector3 pos = Vector3(0,0,0));

    // ---- orden de dibujo de los hijos (ver arriba). Se guardan en el .w3d. ----
    bool ordenarPorCamara;
    bool ordenarUnaVez;
    // ---- lote estatico (ver arriba). 0 = off (default: todo como siempre);
    //      1 = "estatico". Se guarda en el .w3d; setLote() de lua lo cambia. ----
    int  lote;

    // Métodos
    ObjectType getType() override;
    void RenderHijos() override;   // dibuja los hijos ordenados si corresponde

    // Destructor
    ~Collection();

private:
    // orden CONGELADO de ordenarUnaVez (punteros a Childrens; transitorio, no se
    // guarda). Se revalida por MEMBRESIA en cada render: si un hijo entro/salio
    // se recalcula (nunca se dibuja un puntero viejo). ultimo* recuerdan los
    // checkboxes para detectar el "toque" y recalcular (contrato del panel).
    std::vector<Object*> ordenCongelado;
    bool ordenValido;
    bool ultimoPorCamara, ultimoUnaVez;
    // lote estatico armado (NULL = sin armar). Transitorio, no se guarda.
    W3dLoteEstatico* loteData;
    void LoteLiberar();            // libera buffers + destapa las mallas horneadas
    bool LoteRender();             // arma/valida y dibuja el lote; false = dibujar normal
};

#endif
