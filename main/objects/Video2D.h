#ifndef VIDEO2D_H
#define VIDEO2D_H
#include "objects/Elemento2D.h"

// =====================================================================
//  Video2D — un VIDEO en la interfaz (fondos animados, festejos de las
//  mascotas). SIN sonido: es imagen en movimiento. En el editor se
//  previsualiza con frames extraidos (ffmpeg); en el juego lo reproduce
//  la plataforma (el <video> del navegador, etc).
// =====================================================================
class Video2D : public Elemento2D {
public:
    std::string video;    // ruta del archivo (.mp4 / .webm / .gif)
    // como se acomoda dentro del rectangulo: 0 = estirar, 1 = ajustar (entero,
    // con bandas), 2 = cover (llena recortando)
    int   modo;
    bool  loop;           // vuelve a empezar al terminar
    bool  usarAlpha;      // false: ignora la transparencia (se dibuja opaco)
    bool  reproducir;     // ver la animacion en el editor (apagado = primer frame)
    bool  filtrado;       // false: sin filtro (NEAREST)
    unsigned t0ms;        // runtime: cuando arranco (para el modo sin loop); no se guarda

    Video2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Video", pos) {
        modo = 2;              // cover: lo normal para un fondo
        loop = true;
        usarAlpha = true;
        reproducir = true;
        filtrado = true;
        t0ms = 0;
        ancho = 320.0f; alto = 240.0f;   // al elegir el archivo toma su tamano real
    }
    ObjectType getType() override { return ObjectType::video2d; }
};
#endif
