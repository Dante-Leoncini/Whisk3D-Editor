#ifndef RAYTRACER_H
#define RAYTRACER_H
// ============================================================================
//  RayTracer.h - TRAZADO DE RAYOS por CPU de Whisk3D (pestania Render > "Ray Tracing").
//
//  Objetivo: lo mas simple y rapido posible, pensado para correr en el Nokia N95 (ARM11 a 333 MHz con
//  FPU, ~80 MB libres) y escalar a una PC con varios nucleos; con los datos en arrays planos para que
//  el kernel se pueda portar a CUDA/RTX mas adelante sin rehacer el modelo.
//    - los rayos salen de la camara, pegan en la superficie y van a las luces (iluminacion directa,
//      materiales difusos: color del material x textura);
//    - luces solares (direccionales) y puntuales; cada lampara dice cuantos rayos manda al probar la
//      sombra ('rtRayos') y su tamano ('rtRadio'): radio 0 = puntual = sombra dura; mas radio = penumbra;
//    - se renderiza por PASES (uno agrega 'samples' muestras por pixel, con jitter) y por TILES de 32x32:
//      en el viewport se avanza con un presupuesto de tiempo por frame (la UI no se traba) y la imagen
//      se ve progresivamente sobre una textura del tamano del viewport; los overlays van encima con GL;
//    - en una PC con varios nucleos los tiles se reparten entre hilos; en el N95 va un hilo.
//  Memoria: el acumulador es 3 floats por pixel (320x240 = 900 KB). Con imagenes enormes en el N95 el
//  acumulador podria pasarse a un archivo de la microSD tile por tile (queda como siguiente paso: hoy
//  todo vive en RAM).
// ============================================================================
#include <string>
#include <vector>
class Viewport3D;

struct W3dRTOpciones {
    bool on;        // "Ray Tracing" (pestania Render). Apagado por defecto.
    int  rayos;     // rayos de sombra por luz y por impacto (las lamparas con rtRayos = 0 usan este)
    int  samples;   // muestras por pixel y por pase (bajo: 1)
    int  pases;     // pases hasta "done" (16 por defecto)
    int  hilos;     // 0 = automatico (nucleos de la maquina); 1 = un solo hilo
    W3dRTOpciones() : on(false), rayos(1), samples(1), pases(16), hilos(0) {}
};
extern W3dRTOpciones g_rt;

// --- viewport: avanza el render progresivo de ESTE viewport y lo dibuja encima de la escena GL ---
void RTViewportPaso(Viewport3D* vp);      // llamar despues de dibujar la escena, antes de los overlays
void RTRenderEstado(Viewport3D* vp);      // el texto "Ray tracing: 3/16" (con las matrices 2D de la UI puestas)
void RTInvalidar();                       // la escena cambio: el proximo frame arranca de cero
bool RTActivoEn(const Viewport3D* vp);    // hay un render progresivo de ese viewport (para seguir redibujando)

// --- render a archivo ("Render Image" con Ray Tracing tildado): todos los pases, bloqueante, con progreso ---
bool RTRenderizarAPNG(Viewport3D* vp, int w, int h, const std::string& ruta, std::string& msg);

// --- pruebas (harness): render sincronico a un buffer propio + lectura de pixeles ---
bool RTHarnessRender(Viewport3D* vp, int w, int h, int pases);
bool RTHarnessPixel(int x, int y, unsigned char* rgb);
bool RTHarnessGuardar(const std::string& ruta);
int  RTHarnessTriangulos();
#endif // RAYTRACER_H
