#ifndef VIS_ZONE_H
#define VIS_ZONE_H
#include "objects/Objects.h"

// ============================================================================
//  VisZone: decide QUE CELDA de visibilidad esta activa y se la fija a una malla
//  con el modificador Culling por triangulo (metodo "celdas"/VisSet o sectores).
//  Es la mitad "seleccion" del sistema: el DATO vive en el `.w3dvis` (VisSet) y
//  la EMISION en el render; esta zona solo elige el numero de celda.
//
//  Contrato del .w3d de texto:
//      VisZone { modo: manual|grilla|volumenes|curva  objetivo: "Escenario"
//                ancla: "Jugador"  nx: 8 ny: 1 nz: 8  cellSize: 4.0 }
//
//  MODOS:
//   * manual   : la zona no hace nada; la celda la fija el juego con setSector()
//                (el override manual de siempre; pisa a cualquier zona ese tick).
//   * grilla   : celda = ix + iy*nx + iz*nx*ny con (ix,iy,iz) = la posicion del
//                ANCLA llevada al espacio LOCAL de la zona y dividida por
//                cellSize (la zona se ubica/rota como cualquier objeto: su
//                transform ES el origen y orientacion de la grilla). Fuera de
//                la grilla = celda 0 (malla completa: fallback visible, no un pozo).
//   * volumenes: los HIJOS de la zona son los volumenes (cajas unitarias [-1,1]
//                escaladas por su transform, como un Empty): la celda es el
//                indice 1-based del PRIMER hijo que contiene al ancla; ninguno
//                = celda 0.
//   * curva    : el juego entrega el INDICE FRACCIONARIO del nodo del riel con
//                setVisCurvaT(zona, t) (el mismo ancla que ya calcula para la
//                camara). La zona avanza DE A UNA celda por tick hacia el
//                objetivo aunque t salte varias (el stepper del original);
//                si la distancia supera stepMax, salta directo (re-decode
//                desde celda-clave, que VisSet::Delta ya resuelve solo).
//
//  MULTI-RIEL (modo curva, propiedad `riel:` -alias `curva:`-): las celdas de
//  una playlist son los NODOS DE UN RIEL CONCRETO. Con varios rieles en la
//  escena (el juego cambia la camara de riel con setRiel), un indice del riel
//  B aplicado a la playlist del riel A recorta cualquier cosa. Declarando
//  `riel: "NombreDeLaCurve"` la zona sabe QUE riel gobierna sus celdas:
//    - si el indice llega con identificador (setVisCurvaT(zona, t, riel)) y NO
//      coincide, o si la Camera ACTIVA esta viajando por OTRO riel (o por
//      ninguno), la zona cae a su FALLBACK declarado: `fallback: "completa"`
//      (default: celda 0 = malla entera, sin recorte) o `fallback: N` (la
//      celda N de su propia playlist);
//    - al volver al riel propio, la zona salta DIRECTO a la celda objetivo
//      (nada de pasear el stepper por celdas que nunca estuvieron activas).
//  Sin `riel:` declarado nada cambia: la zona acepta cualquier t (legacy).
//
//  La celda se aplica via el hook W3dPVSSetSectorHook (el mismo del bind
//  setSector): esta clase NO conoce Mesh ni el editor -> compila en cualquier
//  runtime; sin el hook instalado es un no-op seguro.
// ============================================================================
class VisZone : public Object {
public:
    enum { ModeManual = 0, ModeGrid = 1, ModeVolumes = 2, ModeCurve = 3 };
    int mode;                // default ModeManual (una zona recien creada no toca nada)
    std::string targetName;  // la malla con el modificador Culling por triangulo
    std::string anchorName;  // quien decide la celda (grilla/volumenes); setVisAncla lo pisa
    int   nx, ny, nz;        // grilla: celdas por eje (default 1)
    float cellSize;          // grilla: lado de la celda en unidades locales (default 1)
    float curveT;            // curva: t objetivo (indice fraccionario de nodo, 0-based)
    int   currentCell;       // celda 1-based APLICADA (0 = ninguna/completa aun)
    int   stepMax;           // curva: distancia maxima del stepper (default 32 = el K tipico)
    // MULTI-RIEL (ver el bloque de arriba). railName = la Curve cuyos nodos
    // indexan las celdas de ESTA zona ("" = legacy: cualquier t vale).
    // cellFallback = que celda se aplica fuera del riel propio (0 = completa).
    std::string railName;
    int   cellFallback;

    VisZone(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0))
        : Object(parent, "VisZone", pos) {
        mode = ModeManual;
        nx = ny = nz = 1; cellSize = 1.0f;
        curveT = 0.0f; currentCell = 0; stepMax = 32;
        cellFallback = 0;
        anchorObj = NULL;
        railBindHas = false; outsideRail = false;
    }
    ObjectType getType() W3D_OVERRIDE { return ObjectType::viszona; }

    // un paso de simulacion (lo llama W3dVisZonesTick una vez por tick del juego):
    // evalua el modo y, si la celda cambio, se la aplica al objetivo.
    void Tick();
    // setVisAncla(zona, obj) del lua: fija el ancla por puntero (pisa anchorName)
    void SetAnchor(Object* o) { anchorObj = o; }
    // setVisCurvaT(zona, t [, riel]): SOLO guarda el objetivo; el paso lo da Tick
    // (un solo stepper: llamarlo N veces por tick no adelanta N celdas). 'riel'
    // (opcional) identifica de QUE riel es ese t: si la zona declara railName
    // y no coincide, el proximo Tick cae al fallback (ver multi-riel arriba).
    void SetCurveT(float t, const char* rail = 0) {
        curveT = t;
        railBindHas = (rail != 0);
        if (rail) railBind = rail;
    }

private:
    Object* anchorObj;            // ancla fijada por lua (NO se serializa; gana sobre el nombre)
    std::string railBind;         // identificador recibido en el ultimo setVisCurvaT (multi-riel)
    bool    railBindHas;          // llego identificador desde el ultimo Tick (se consume por tick)
    bool    outsideRail;          // el tick anterior cayo al fallback -> al volver, salto directo
    Object* GetTarget() const;    // resuelve targetName en la escena (NULL si no esta)
    Object* Anchor() const;       // anchorObj o anchorName resuelto
    bool RailGovernsOk();         // multi-riel: el t que llega es del riel declarado?
    int  CellGrid(const Vector3& p) const;
    int  CellVolumes(const Vector3& p) const;
    void Apply(int cell);     // via hook de setSector (no-op sin hook/objetivo)
};

// recorre la escena y tickea todas las VisZone visibles (mismo contrato que
// W3dParticlesTick: lo llama el main loop del juego y el harness al simular).
void W3dVisZonesTick();

#endif
