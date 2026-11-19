#ifndef SOMBRASTIEMPOREAL_H
#define SOMBRASTIEMPOREAL_H
// ============================================================================
//  SOMBRAS EN TIEMPO REAL: la tarjeta "Real-time shadows" de la pestania Render (apagada de fabrica), como las de Unity.
//  Da sombra la primera LUZ SOLAR (direccional) de la escena; cada objeto elige si PROYECTA (tapa la luz) y si RECIBE
//  (Object::proyectaSombra / recibeSombra, de fabrica las dos).
//
//  CON SHADERS (GLES 2.0 de la PC / Android / la web, Vulkan): MAPAS DE SOMBRAS EN CASCADA. La vista de la camara se corta
//  en tramos por la distancia (las cascadas); cada uno tiene su mapa -la profundidad de lo que proyecta, vista desde el
//  sol- en un pedazo de una misma textura. Las cascadas de cerca cubren poco y se ven nitidas; las de lejos cubren mucho.
//  Cada pixel de lo que recibe busca su cascada y compara su profundidad (con un filtro de 2x2 para los bordes suaves y,
//  lejos, desenfocado). Al final de la distancia maxima se desvanecen (el borde) en vez de cortarse.
//
//  SIN SHADERS (GL ES 1.1 del N95, el GL fijo): no hay como comparar profundidades por pixel. La GPU dibuja desde el sol
//  cada objeto de un color (su numero) con z-buffer, la CPU lee esos pixeles y arma para cada objeto que recibe una
//  textura: oscura donde lo que ve el sol es OTRO objeto (algo le tapa la luz). Esa textura se proyecta sobre el objeto
//  (las UV las calcula la CPU) en una pasada que multiplica. No es para tiempo real (aunque si nada se mueve no se vuelve
//  a calcular): es para que los renders del N95 tengan sombras. Un objeto no se hace sombra a si mismo.
// ============================================================================

struct W3dSombrasOpc {
    bool  on;           // apagadas de fabrica
    float distancia;    // hasta donde hay sombras, desde la camara (m)
    int   cascadas;     // 1..4
    float cortes[3];    // hasta donde llega cada cascada (m): las que haya menos la ultima, que llega a la distancia
    float borde;        // el tramo del final donde se desvanecen (m)
    float desenfoque;   // 0..1: cuanto se borronean las lejanas (crece con la distancia)
    int   resolucion;   // texeles de cada cascada (256..4096)
    bool  suaves;       // bordes suaves (filtro de 2x2) o duras
    float intensidad;   // 0..1: que tan oscura es la sombra
    float sesgo;        // multiplica el sesgo automatico (contra el "acne": la superficie que se hace sombra a si misma)
    W3dSombrasOpc();
};
extern W3dSombrasOpc g_sombras;

// el pase de Render (EscenaRender): antes de dibujar la escena, con la vista de la camara cargada. Arma los mapas y deja a
// los objetos que las reciben listos para usarlos. W3dSombrasFin al terminar el pase
void W3dSombrasPase();
void W3dSombrasFin();
// las hace la GPU (cascadas) o la CPU (el N95)
bool W3dSombrasConShaders();
// lo que paso en el ultimo pase (la tarjeta lo muestra): 0 apagadas, 1 sin luz solar, 2 con sombras
int W3dSombrasEstado();

#endif
