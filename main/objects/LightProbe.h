#ifndef LIGHT_PROBE_H
#define LIGHT_PROBE_H
#include "objects/Objects.h"
#include <vector>
#include <string>

struct JVal;   // io/JsonW3d.h

// ============================================================================
//  LightProbe — LA SONDA DE LUZ (como las "Light Probes" de Unity o el "Irradiance Volume" de Blender). Una CAJA con una
//  GRILLA de puntos: al HORNEAR, el trazado de rayos mira desde cada punto en todas las direcciones y guarda la luz que
//  le llega -la del fondo y la que REBOTA en lo que hay alrededor (la luz INDIRECTA)- como armonicos esfericos de
//  orden 2: 9 colores por punto, la misma cuenta que la luz del fondo 360 (SceneRender.cpp). La luz DIRECTA de las
//  lamparas no: esa la sigue calculando la luz de siempre (no se cuenta dos veces).
//
//  Al dibujar, cada malla NO ESTATICA (lo que se mueve: lo estatico precalcula su luz) toma la luz de los puntos que la rodean (interpolada en la grilla, desde el centro de la
//  malla) y la usa de luz ambiente en vez de la del fondo:
//    - con shaders (PC, Android, la web): la luz por pixel PBR la evalua con la normal de cada pixel
//    - el pipeline fijo (el N95): el promedio, como el ambiente de GL de ese objeto (no cuesta nada). Con la casilla
//      "Luz indirecta" del material, la CPU calcula la luz de cada VERTICE: las lamparas + la indirecta con la normal
//    Un objeto fuera de toda sonda sigue con la luz del fondo.
//
//    tam[3]      el tamano de la caja (espacio LOCAL: la escala del objeto la agranda)
//    puntos[3]   cuantos puntos por eje (2 a 16)
//    rayos       los rayos por punto al hornear (mas = menos ruido)
//    sh          los coeficientes horneados: puntos[0]*puntos[1]*puntos[2] * 27 (vacio = sin hornear). Ya vienen
//                multiplicados por la convolucion del coseno: la luz difusa en la direccion n es sum c_i * Y_i(n)
//                (en el espacio del MUNDO, el de la escena)
// ============================================================================
class LightProbe : public Object {
public:
    float size[3];
    int   points[3];
    int   rays;
    std::vector<float> sh;
    unsigned pass;   // el ultimo pase de Render que la dibujo (las que no estan en la escena no iluminan)

    LightProbe(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ~LightProbe();
    ObjectType getType() W3D_OVERRIDE { return ObjectType::lightProbe; }
    void RenderObject() W3D_OVERRIDE;
    bool DrawsSomething() W3D_OVERRIDE;

    int  Count() const { return points[0] * points[1] * points[2]; }
    bool Baked() const { return (int)sh.size() == Count() * 27 && Count() > 0; }
    // la posicion LOCAL del punto (i, j, k) de la grilla
    void PointLocal(int i, int j, int k, float out[3]) const;
};

// HORNEAR: la luz que llega a cada punto (el trazado de rayos de la CPU). false + motivo en INGLES si no se pudo
bool LightProbeBake(LightProbe* s, std::string* reason);
// DE A PEDAZOS (la barra de Propiedades): Empezar arma el trabajo (RTProbeStep lo avanza) y Terminar, cuando
// RTProbeActive() da false, rellena los puntos de adentro de algo y la deja horneada
bool LightProbeBakeBegin(LightProbe* s, std::string* reason);
bool LightProbeBakeFinish(LightProbe* s);
bool LightProbeLive(const LightProbe* s);   // existe todavia (no la borraron mientras se horneaba)
// las 12 aristas de la caja (como HitboxEdges): el dibujo y el pick
void LightProbeEdges(const LightProbe* s, float out[72]);
// la LUZ INDIRECTA en un punto del MUNDO: la de la sonda que lo contiene (o la mas cercana, si esta cerca de su caja),
// interpolada en su grilla. false = ninguna sonda horneada lo alcanza
bool LightProbeIn(const float pos[3], float sh[27]);
// la sonda se dibuja en este pase? (overlays del editor, como el hitbox)
bool LightProbeDraws();
// copia de los campos propios (Duplicar)
LightProbe* LightProbeDuplicate(LightProbe* src);
// los campos propios en el .w3d ("tipo": "sondaluz")
void LightProbeWriteFields(std::string& s, int ind, const LightProbe* o);
void LightProbeReadFields(JVal* j, LightProbe* o);
// el pase de Render que empieza (EscenaRender: las sondas que no se dibujaron en el ultimo no iluminan)
void LightProbeNewPass();

#endif
