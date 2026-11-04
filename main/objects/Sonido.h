#ifndef SONIDO_H
#define SONIDO_H
#include "objects/Objects.h"
#include <string>

// ============================================================================
//  Objeto SONIDO: un audio PUESTO EN LA ESCENA.
//
//  EN UNA ESCENA (timeline) suena con el Play y se ubica en SEGUNDOS, no en frames: 'inicio' es el momento de la
//  escena (0 = el frame 1) en el que arranca. Asi, pasar la escena de 30 a 60 o 120 fps no obliga a reacomodar
//  ningun audio: sigue enganchado al mismo instante, a la muestra. En el timeline es una TIRA que se corre con 'g'
//  o arrastrando, y se recorta agarrando sus puntas.
//  EN UN JUEGO suena desde que aparece (un prefab instanciado: la explosion) mas 'inicio'; lua lo maneja
//  (tocarSonido / callarSonido / sonidoObj / setSonidoObj / crearSonido, ver Sonido.cpp).
//
//  Propiedades (el panel y el .w3d hablan de lo mismo):
//    archivo                el sonido del proyecto ("sonidos/musica.wav")
//    activo                 apagado no suena
//    volumen / tono / reverb  0..2 / 0.25..4 (pitch) / 0..1 (envio a la reverb). ANIMABLES (AnimSonido X/Y/Z)
//    paneo                  -1 izquierda .. 1 derecha (solo si NO es posicional)
//    inicio                 segundos de la escena en que arranca
//    recorteIni / recorteFin  segundos que se le cortan al archivo al principio / al final (el tramo que suena)
//    bucle                  el tramo se repite hasta el infinito (exacto en la muestra)
//    posicional             SONIDO 3D: se escucha desde donde esta respecto de quien escucha (la camara): mas cerca,
//                           mas fuerte; a la derecha, por la derecha; atras, mas bajo. Sin tildar es como MUSICA: se
//                           escucha igual este donde este la camara
//    radio / alcance        3D: hasta 'radio' suena entero; desde 'alcance' ya no se escucha
//
//  La salida del mezclador es ESTEREO: lo de "atras" se simula bajando el volumen y llevandolo al medio (sin 5.1).
// ============================================================================
namespace w3dEngine { class W3dSound; }

class Sonido : public Object {
public:
    std::string archivo;
    bool  activo;
    float volumen, tono, reverb, paneo;
    float inicio;
    float recorteIni, recorteFin;
    bool  bucle;
    bool  posicional;
    float radio, alcance;
    // ---- lo de la sesion (no se guarda) ----
    int    voz;            // la voz que suena (0 = ninguna)
    double tJuego0;        // JUEGO: el reloj del mixer en el que "empezo" (< 0 = todavia no)
    std::string vozFuente; // de que suena la voz (archivo|recortes|bucle): si cambia, la voz se rearma
    unsigned int ondaTex;  // (no se usa por objeto: las formas de onda van por archivo, ver W3dSonidoOnda)

    Sonido(Object* parent = NULL, Vector3 pos = Vector3(0, 0, 0));
    ~Sonido();
    ObjectType getType() W3D_OVERRIDE { return ObjectType::sonido; }
    void RenderObject() W3D_OVERRIDE;
    bool DibujaAlgo() W3D_OVERRIDE;

    float DuracionArchivo();          // segundos del archivo entero (0 = no se pudo leer)
    float DuracionTramo();            // segundos DEL ARCHIVO que suenan (sin los recortes)
    float DuracionEnEscena();         // segundos de la escena que ocupa una pasada (el tramo / el tono)
    void  Callar();                   // corta su voz (con una rampa corta, sin click)
    void  CopiarDe(const Sonido* s);  // las propiedades (no la sesion)
};

// el TICK de los sonidos del EDITOR (main loop): con el Play del timeline de una escena suenan donde va la linea de
// tiempo; parado, callados. En modo Juego no hace nada (ahi va el tick del juego, por BindsJuego)
void W3dSonidosTickEditor();
// el tick del JUEGO (el Play del juego del editor y el juego compilado): lo engancha BindsJuegoSnapshotPunteros
void W3dSonidosTickJuego();
// calla todos (cerrar el proyecto, parar)
void W3dSonidosCallar();
// quien ESCUCHA en el editor: la vista 3D activa (la instala el viewport). Sin el, la camara activa
extern bool (*W3dSonidoOyenteHook)(Vector3& pos, Vector3& der, Vector3& adel);
// el tiempo de la ESCENA (segundos) de un frame del timeline: (frame - 1) / fps
float W3dSonidoSegDeFrame(float frame);
float W3dSonidoFrameDeSeg(float seg);
// LA FORMA DE ONDA de un archivo (para el timeline): una textura de 4096 x 64 (blanco con alfa) del archivo entero,
// normalizada a su pico.
// 0 = no se pudo. Se calcula una vez por archivo y queda en memoria
unsigned int W3dSonidoOnda(const std::string& archivo);
// el archivo cargado (compartido por todos los que lo usan; NULL = no se pudo)
w3dEngine::W3dSound* W3dSonidoCargar(const std::string& archivo);

#endif
