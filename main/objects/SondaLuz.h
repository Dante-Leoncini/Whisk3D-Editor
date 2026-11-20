#ifndef SONDALUZ_H
#define SONDALUZ_H
#include "objects/Objects.h"
#include <vector>
#include <string>

struct JVal;   // io/JsonW3d.h

// ============================================================================
//  SondaLuz — LA SONDA DE LUZ (como las "Light Probes" de Unity o el "Irradiance Volume" de Blender). Una CAJA con una
//  GRILLA de puntos: al HORNEAR, el trazado de rayos mira desde cada punto en todas las direcciones y guarda la luz que
//  le llega -la del fondo y la que REBOTA en lo que hay alrededor (la luz INDIRECTA)- como armonicos esfericos de
//  orden 2: 9 colores por punto, la misma cuenta que la luz del fondo 360 (EscenaRender.cpp). La luz DIRECTA de las
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
class SondaLuz : public Object {
public:
    float tam[3];
    int   puntos[3];
    int   rayos;
    std::vector<float> sh;
    unsigned pase;   // el ultimo pase de Render que la dibujo (las que no estan en la escena no iluminan)

    SondaLuz(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ~SondaLuz();
    ObjectType getType() W3D_OVERRIDE { return ObjectType::sondaluz; }
    void RenderObject() W3D_OVERRIDE;
    bool DibujaAlgo() W3D_OVERRIDE;

    int  Cantidad() const { return puntos[0] * puntos[1] * puntos[2]; }
    bool Horneada() const { return (int)sh.size() == Cantidad() * 27 && Cantidad() > 0; }
    // la posicion LOCAL del punto (i, j, k) de la grilla
    void PuntoLocal(int i, int j, int k, float out[3]) const;
};

// HORNEAR: la luz que llega a cada punto (el trazado de rayos de la CPU). false + motivo en INGLES si no se pudo
bool SondaLuzHornear(SondaLuz* s, std::string* motivo);
// DE A PEDAZOS (la barra de Propiedades): Empezar arma el trabajo (RTSondaPaso lo avanza) y Terminar, cuando
// RTSondaActiva() da false, rellena los puntos de adentro de algo y la deja horneada
bool SondaLuzHornearEmpezar(SondaLuz* s, std::string* motivo);
bool SondaLuzHornearTerminar(SondaLuz* s);
bool SondaLuzViva(const SondaLuz* s);   // existe todavia (no la borraron mientras se horneaba)
// las 12 aristas de la caja (como HitboxAristas): el dibujo y el pick
void SondaLuzAristas(const SondaLuz* s, float out[72]);
// la LUZ INDIRECTA en un punto del MUNDO: la de la sonda que lo contiene (o la mas cercana, si esta cerca de su caja),
// interpolada en su grilla. false = ninguna sonda horneada lo alcanza
bool SondaLuzEn(const float pos[3], float sh[27]);
// la sonda se dibuja en este pase? (overlays del editor, como el hitbox)
bool SondaLuzSeDibuja();
// copia de los campos propios (Duplicar)
SondaLuz* SondaLuzDuplicar(SondaLuz* src);
// los campos propios en el .w3d ("tipo": "sondaluz")
void SondaLuzEscribirCampos(std::string& s, int ind, const SondaLuz* o);
void SondaLuzLeerCampos(JVal* j, SondaLuz* o);
// el pase de Render que empieza (EscenaRender: las sondas que no se dibujaron en el ultimo no iluminan)
void SondaLuzNuevoPase();

#endif
