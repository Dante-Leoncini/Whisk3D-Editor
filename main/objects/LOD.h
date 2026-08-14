#ifndef LOD_H
#define LOD_H
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner
#include <vector>
#include <string>

// ============================================================================
//  Objeto LOD (niveles de detalle): dibuja UN solo hijo segun la distancia de la
//  camara a la GEOMETRIA del hijo candidato (distancia al AABB en mundo de su
//  subarbol, 0 si la camara esta adentro de la caja).
//
//  POR QUE AL AABB Y NO AL ORIGEN: el origen de un trozo de escenario es el
//  CENTRO de la pareja gruesa/fina, asi que un trozo grande conmutaba a "lejos"
//  con el jugador parado sobre su borde. Medido en un nivel real: radios
//  (media diagonal) de 15 a 35 u con umbral 40 -> el error ERA el radio. Con la
//  distancia al AABB el umbral significa lo que dice: "que tan cerca esta la
//  geometria". Los umbrales de los .w3d viejos hay que recalibrarlos: al AABB,
//  30 u para una zona tipica (era 40 al centro con radio mediano ~10).
//
//  HISTERESIS del 10 %: sin ella, parado exactamente en el umbral la pareja
//  parpadea (un frame fino, el siguiente grueso). Para SUBIR de detalle hace
//  falta acercarse a umbral*0.9; para BAJAR, alejarse a umbral*1.1.
//
//  La vista que se mide:
//    - soloCamaraActiva false (default): la VISTA QUE DIBUJA (g_renderCamPos)
//      -> en el editor la orbita del viewport (se previsualiza moviendola), en
//      el juego la camara activa.
//    - soloCamaraActiva true: SIEMPRE la Camera ACTIVA de la escena, igual que
//      el flag hermano del Culling. Es lo que hace que LOD y Culling
//      previsualicen con LA MISMA camara: con dos viewports 3D, g_renderCamPos
//      es un global "el ultimo que dibujo gana" y cada uno decidia distinto.
//
//  Contrato del .w3d de texto:
//      LOD { distancias: "20, 45, 90"  soloCamaraActiva: true|false <hijos> }
//  umbral i = HASTA donde se dibuja el hijo i; mas alla del ultimo umbral se
//  dibuja el ULTIMO hijo (haya o no umbral para el).
// ============================================================================
class LOD : public Object {
public:
    // umbrales de distancia, en unidades de mundo, ordenados de cerca a lejos
    std::vector<float> distancias;
    bool soloCamaraActiva; // medir desde la Camera activa (mismo flag que Culling). Default false.

    LOD(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "LOD", pos), soloCamaraActiva(false), cacheVal(false), ultimoElegido(-1) {
        for (int i = 0; i < 16; i++) cacheW[i] = 0.0f;
    }
    ObjectType getType() override { return ObjectType::lod; }
    void RenderHijos() override;

    int HijoElegido() const; // indice del hijo a dibujar con la vista bindeada (-1 = sin hijos)
    // el hijo que quedo DIBUJADO en el ultimo render (sin re-medir nada): es lo
    // que el harness necesita para asertar decisiones que solo existen DURANTE
    // el render (p.ej. la herencia de soloCamaraActiva de un Culling padre).
    int UltimoDibujado() const { return ultimoElegido; }
    // distancia (no al cuadrado) de la camara medida al AABB del hijo i. <0 = el hijo
    // no tiene ni una malla medible. La usa el harness de pruebas para ver los numeros.
    float DistanciaAHijo(int i) const;

    // ida y vuelta con el formato "20, 45, 90" (el panel de propiedades y el
    // .w3d de texto hablan este idioma; el JSON v4 guarda la lista de numeros)
    std::string DistanciasTexto() const;
    void SetDistanciasTexto(const std::string& s);

private:
    Vector3 OjoDeMedida() const;   // desde donde se mide (viewport o camara activa)
    void    RefrescarCache() const; // recalcula el AABB de cada hijo si hace falta
    // CACHE del AABB en mundo por hijo. La geometria de un LOD es estatica (trozos
    // de escenario), asi que medirla por frame y por hijo seria recorrer el subarbol
    // entero cada vez. Se recalcula solo cuando cambia la cantidad de hijos o la
    // matriz de mundo del PROPIO LOD (o sea: si el LOD -o un ancestro- se mueve).
    // Lo que NO cubre es que un hijo se mueva por dentro sin mover al LOD: en ese
    // caso hay que invalidar a mano (InvalidarCache), que es lo que hace el editor.
    mutable std::vector<Vector3> cacheMin, cacheMax;
    mutable std::vector<char>    cacheOk;
    mutable float                cacheW[16];
    mutable bool                 cacheVal;
    mutable int                  ultimoElegido; // nivel del frame anterior (histeresis)
public:
    void InvalidarCache() { cacheVal = false; }
};
#endif
