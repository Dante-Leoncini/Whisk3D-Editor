#ifndef REFLECTION_PROBE_H
#define REFLECTION_PROBE_H
#include "objects/Objects.h"
#include "objects/Textures.h"   // Texture: la imagen viva de la de tiempo real
#include <vector>
#include <string>

struct JVal;   // io/JsonW3d.h

// ============================================================================
//  ReflectionProbe — LA SONDA DE REFLEJO (como el "Reflection Probe" de Unity): una camara de 360 grados puesta en un lugar.
//  Al HORNEAR, el trazado de rayos mira desde su centro en todas las direcciones -con la misma logica que la luz
//  precalculada y la sonda de luz: solo lo ESTATICO, si proyecta o no sombras, las lamparas, el fondo- y guarda lo que ve
//  en una IMAGEN 360 (equirectangular, el doble de ancho que de alto: la misma que usa el reflejo "Equirectangular 360"
//  del material y el fondo 360). Esa imagen es una textura del proyecto: se guarda con el .w3d. La de TIEMPO REAL no usa
//  rayos: la GPU dibuja la escena desde su centro (las 6 caras de un cubo, como cualquier cuadro) y las junta en la
//  imagen 360 (render/SceneRender.cpp). Esa imagen no se guarda: la vuelve a dibujar el editor o el juego.
//
//  Los materiales la usan desde el reflejo (Material::chrome) eligiendo la sonda en el submenu "Sondas de reflejo" del
//  selector de la textura, o "La sonda mas cercana": cada malla toma la de la sonda que la contiene (la de mas
//  IMPORTANCIA; si ninguna, la mas cercana) y el reflejo cambia segun donde este. Con la PROYECCION EN CAJA el reflejo se
//  corrige por la caja de la sonda (una habitacion: lo que se refleja esta en sus paredes, no en el infinito).
//
//  Funciona en todos lados: el reflejo 360 se calcula por vertice en la CPU (tambien en GL ES 1.1, el N95).
//
//    tipo         HORNEADA (la hornea el editor con rayos), PERSONALIZADA (una imagen 360 que elige el usuario) o TIEMPO
//                 REAL (la dibuja la GPU cada tanto, con lo que se mueve tambien, en el editor y en el juego)
//    importancia  entre dos sondas que contienen el mismo punto, manda la de mas importancia
//    intensidad   multiplica lo que se ve en el reflejo (al hornear)
//    caja         la proyeccion en caja; tam[3] el tamanio de la caja (espacio LOCAL) y origen[3] el centro de la captura
//                 (corrido del centro de la caja)
//    resolucion   el ancho de la imagen 360 (el alto es la mitad)
//    fondo        (sin uso: se hornea siempre el fondo de la escena, el color o el 360)
//    cerca        lo que esta mas cerca que esto del centro no se ve (la sonda adentro de algo)
//    muestras     (horneada) los rayos por pixel; rebotes: las veces que rebota la luz
//    eachSeg      (tiempo real) cada cuantos segundos se vuelve a dibujar (0 = en cada cuadro)
//    textura      la ruta de la imagen 360 (la horneada o la elegida)
// ============================================================================
enum { W3D_REFLECTION_BAKED = 0, W3D_REFLECTION_CUSTOMIZED = 1, W3D_REFLECTION_TIME_REAL = 2 };

class ReflectionProbe : public Object {
public:
    int   type;
    int   importance;
    float intensity;
    bool  box;
    float size[3];
    float origin[3];
    int   resolution;
    int   background;
    float backgroundColor[4];
    float near;
    int   samples;         // (horneada) rayos por pixel
    int   bounces;         // (horneada) rebotes de la luz
    float eachSeg;         // (tiempo real) cada cuantos segundos se vuelve a dibujar (0 = cada cuadro)
    std::string texture;
    Texture* tex;         // la imagen cargada (+1 ref): la de "La sonda mas cercana"
    unsigned lastMs;      // (tiempo real) cuando se dibujo por ultima vez
    Texture live;         // (tiempo real) la imagen 360 que dibuja la GPU (iID = 0: todavia ninguna)
    std::vector<unsigned char> livePx;   // (tiempo real) sus pixeles (RGBA, la fila 0 arriba): las pruebas la miran
    unsigned version;       // cambia con cada horneado o imagen nueva (lo que se calcula de la imagen se rehace)
    unsigned int blurryTex;   // la copia borrosa para el PBR (EscenaRender: la irradiancia de la imagen) y de que version
    unsigned blurryVersion;

    ReflectionProbe(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ~ReflectionProbe();
    ObjectType getType() W3D_OVERRIDE { return ObjectType::sondareflejo; }
    void RenderObject() W3D_OVERRIDE;
    bool DrawsSomething() W3D_OVERRIDE { return true; }

    // la imagen 360 (la carga si hace falta; NULL = no tiene). La de tiempo real: la que dibujo la GPU
    Texture* Image();
    bool HasImage() const;
    // el centro de la captura y la caja (min x y z, max x y z) en el MUNDO
    void WorldCenter(float out[3]) const;
    void BoxWorld(float out[6]) const;
};

// HORNEAR (el editor: el trazado de rayos). Crea o actualiza su textura. false + motivo en INGLES si no se pudo
bool ReflectionProbeBake(ReflectionProbe* s, std::string* reason);
// DE A PEDAZOS (la barra de Propiedades): Empezar arma el trabajo (RTProbeStep lo avanza) y Terminar, cuando
// RTProbeActive() da false, guarda la imagen en su textura
bool ReflectionProbeBakeBegin(ReflectionProbe* s, std::string* reason);
bool ReflectionProbeBakeFinish(ReflectionProbe* s, std::string* reason);
// las 12 aristas de la caja (el dibujo y el pick)
void ReflectionProbeEdges(const ReflectionProbe* s, float out[72]);
// la sonda que le toca a un punto del MUNDO: la de mas importancia que lo contiene; si ninguna, la mas cercana (con
// onlyInside, ninguna: el PBR usa el fondo afuera de las sondas, como Unity). NULL = no hay ninguna con imagen
ReflectionProbe* ReflectionProbeFor(const float pos[3], bool onlyInside = false);
// las de la escena (para el submenu del material)
void ReflectionProbeList(std::vector<ReflectionProbe*>& out);   // (solo las de la escena)
// sigue en la escena? (una BORRADA la guarda el Ctrl+Z fuera del arbol: no refleja ni captura)
bool ReflectionProbeInScene(const ReflectionProbe* s);
// es la imagen de alguna sonda? (el selector de texturas del material las pone aparte)
bool ReflectionProbeIsOwn(const Texture* t);
ReflectionProbe* ReflectionProbeDuplicate(ReflectionProbe* src);
void ReflectionProbeWriteFields(std::string& s, int ind, const ReflectionProbe* o);
void ReflectionProbeReadFields(JVal* j, ReflectionProbe* o);

#endif
