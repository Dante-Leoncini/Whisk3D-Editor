#ifndef SONDAREFLEJO_H
#define SONDAREFLEJO_H
#include "objects/Objects.h"
#include "objects/Textures.h"   // Texture: la imagen viva de la de tiempo real
#include <vector>
#include <string>

struct JVal;   // io/JsonW3d.h

// ============================================================================
//  SondaReflejo — LA SONDA DE REFLEJO (como el "Reflection Probe" de Unity): una camara de 360 grados puesta en un lugar.
//  Al HORNEAR, el trazado de rayos mira desde su centro en todas las direcciones -con la misma logica que la luz
//  precalculada y la sonda de luz: solo lo ESTATICO, si proyecta o no sombras, las lamparas, el fondo- y guarda lo que ve
//  en una IMAGEN 360 (equirectangular, el doble de ancho que de alto: la misma que usa el reflejo "Equirectangular 360"
//  del material y el fondo 360). Esa imagen es una textura del proyecto: se guarda con el .w3d. La de TIEMPO REAL no usa
//  rayos: la GPU dibuja la escena desde su centro (las 6 caras de un cubo, como cualquier cuadro) y las junta en la
//  imagen 360 (render/EscenaRender.cpp). Esa imagen no se guarda: la vuelve a dibujar el editor o el juego.
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
//    cadaSeg      (tiempo real) cada cuantos segundos se vuelve a dibujar (0 = en cada cuadro)
//    textura      la ruta de la imagen 360 (la horneada o la elegida)
// ============================================================================
enum { W3D_REFLEJO_HORNEADA = 0, W3D_REFLEJO_PERSONALIZADA = 1, W3D_REFLEJO_TIEMPO_REAL = 2 };

class SondaReflejo : public Object {
public:
    int   tipo;
    int   importancia;
    float intensidad;
    bool  caja;
    float tam[3];
    float origen[3];
    int   resolucion;
    int   fondo;
    float colorFondo[4];
    float cerca;
    int   muestras;         // (horneada) rayos por pixel
    int   rebotes;          // (horneada) rebotes de la luz
    float cadaSeg;          // (tiempo real) cada cuantos segundos se vuelve a dibujar (0 = cada cuadro)
    std::string textura;
    Texture* tex;           // la imagen cargada (+1 ref): la de "La sonda mas cercana"
    unsigned ultimoMs;      // (tiempo real) cuando se dibujo por ultima vez
    Texture vivo;           // (tiempo real) la imagen 360 que dibuja la GPU (iID = 0: todavia ninguna)
    std::vector<unsigned char> vivoPx;   // (tiempo real) sus pixeles (RGBA, la fila 0 arriba): las pruebas la miran
    unsigned version;       // cambia con cada horneado o imagen nueva (lo que se calcula de la imagen se rehace)
    unsigned int texBorrosa;   // la copia borrosa para el PBR (EscenaRender: la irradiancia de la imagen) y de que version
    unsigned versionBorrosa;

    SondaReflejo(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ~SondaReflejo();
    ObjectType getType() W3D_OVERRIDE { return ObjectType::sondareflejo; }
    void RenderObject() W3D_OVERRIDE;
    bool DibujaAlgo() W3D_OVERRIDE { return true; }

    // la imagen 360 (la carga si hace falta; NULL = no tiene). La de tiempo real: la que dibujo la GPU
    Texture* Imagen();
    bool TieneImagen() const;
    // el centro de la captura y la caja (min x y z, max x y z) en el MUNDO
    void CentroMundo(float out[3]) const;
    void CajaMundo(float out[6]) const;
};

// HORNEAR (el editor: el trazado de rayos). Crea o actualiza su textura. false + motivo en INGLES si no se pudo
bool SondaReflejoHornear(SondaReflejo* s, std::string* motivo);
// las 12 aristas de la caja (el dibujo y el pick)
void SondaReflejoAristas(const SondaReflejo* s, float out[72]);
// la sonda que le toca a un punto del MUNDO: la de mas importancia que lo contiene; si ninguna, la mas cercana (con
// soloAdentro, ninguna: el PBR usa el fondo afuera de las sondas, como Unity). NULL = no hay ninguna con imagen
SondaReflejo* SondaReflejoPara(const float pos[3], bool soloAdentro = false);
// las de la escena (para el submenu del material)
void SondaReflejoListar(std::vector<SondaReflejo*>& out);
// es la imagen de alguna sonda? (el selector de texturas del material las pone aparte)
bool SondaReflejoEsSuya(const Texture* t);
SondaReflejo* SondaReflejoDuplicar(SondaReflejo* src);
void SondaReflejoEscribirCampos(std::string& s, int ind, const SondaReflejo* o);
void SondaReflejoLeerCampos(JVal* j, SondaReflejo* o);

#endif
