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
    int  pases;     // pases hasta "done" (16 por defecto; 8 en el N95: a 333 MHz cada pase cuesta caro)
    int  hilos;     // 0 = automatico (nucleos de la maquina); 1 = un solo hilo
    int  denoiser;  // W3dDenoiser (render/Denoiser.h): 0 = ninguno. Limpia el ruido al terminar cada pase
    bool gpu;       // trazar en la GPU si hay (render/RTGpu.h: Vulkan); sin GPU, o si falla, va por CPU
    bool soloFinal; // el viewport muestra la imagen TERMINADA (todos los pases): no se ve como se va formando
    int  escala;    // REESCALADO del viewport: se traza a 1/escala de su resolucion (1 = apagado; 2, 3, 4) y se agranda con
                    // filtro lineal (con un denoiser, limpio antes de agrandar). El render a archivo va siempre entero
    int  reescalador; // COMO se agranda: 0 = lineal (la textura estirada); 1 = FSR 1 de AMD (render/ReescaladoFSR.h, por
                      // CPU: bordes nitidos, en cualquier placa). En el telefono siempre lineal
    int  capas;     // CAPAS TRANSPARENTES que atraviesa un rayo (vidrios, hojas, pelo); pasadas esas, lo de atras queda negro
    bool semillaVariable; // OTRO RUIDO EN CADA IMAGEN (default): con la misma semilla, en una animacion el ruido del
                          // trazado quedaba QUIETO en la pantalla (una trama pegada al vidrio); cambiandola se mueve y el
                          // ojo (y el denoiser) lo promedian parejo
    // DESENFOQUE DE MOVIMIENTO TRAZADO (solo el render a archivo): cada pase se traza en OTRO INSTANTE dentro del
    // obturador (los vertices y la camara entre los cuadros de al lado). Sale de verdad (sombras, reflejos, lo que hay
    // detras de lo que se mueve), no como el de los vectores; hacen falta pases para que no se vean las copias
    bool  desenfoque;
    float obturador;     // la fraccion del cuadro que queda abierto (0.01 .. 1; 0.5 = el clasico de 180 grados)
    int   obturadorPos;  // DONDE se abre: 0 = centrado en el cuadro, 1 = empieza en el cuadro, 2 = termina en el cuadro
#ifdef W3D_SYMBIAN
    W3dRTOpciones() : on(false), rayos(1), samples(1), pases(8), hilos(1), denoiser(0), gpu(false), soloFinal(false), escala(1), reescalador(0), capas(8), semillaVariable(true),
                      desenfoque(false), obturador(0.5f), obturadorPos(0) {}
#else
    W3dRTOpciones() : on(false), rayos(1), samples(1), pases(16), hilos(0), denoiser(0), gpu(true), soloFinal(false), escala(1), reescalador(1), capas(8), semillaVariable(true),
                      desenfoque(false), obturador(0.5f), obturadorPos(0) {}
#endif
};
extern W3dRTOpciones g_rt;
// el nombre de cada posicion del obturador (en ingles: T() lo traduce)
const char* RTObturadorPosNombre(int pos);

// --- viewport: avanza el render progresivo de ESTE viewport y lo dibuja encima de la escena GL ---
void RTViewportPaso(Viewport3D* vp);      // llamar despues de dibujar la escena, antes de los overlays
bool RTRenderEstado(Viewport3D* vp);      // el texto "Ray tracing: 3/16" (con las matrices 2D de la UI puestas); false = no hay
// el cartel de arriba a la izquierda (el mismo lugar que el del trazado): "Local View" mientras dura la vista local
void W3dVistaCartel(Viewport3D* vp, const char* texto);
void RTInvalidar();                       // la escena cambio: el proximo frame arranca de cero
bool RTActivoEn(const Viewport3D* vp);    // hay un render progresivo de ese viewport (para seguir redibujando)
float RTImagenesPorSegundo();
const char* RTReescaladorNombre(int r);  // "Linear" / "FSR 1 (AMD)"             // las imagenes trazadas TERMINADAS por segundo (lo que de verdad se ve)
double RTRayosPorSegundo();               // los rayos por segundo de la GPU mientras traza (0 = por CPU)

// --- render a archivo ("Render Image" con Ray Tracing tildado): todos los pases, bloqueante, con progreso ---
bool RTRenderizarAPNG(Viewport3D* vp, int w, int h, const std::string& ruta, std::string& msg);
// lo mismo a un buffer RGBA (filas de arriba hacia abajo), sin guardar (para post-procesarlo: el desenfoque de movimiento)
bool RTRenderizarABuffer(Viewport3D* vp, int w, int h, std::vector<unsigned char>& rgba, std::string& msg);

// --- pruebas (harness): render sincronico a un buffer propio + lectura de pixeles ---
bool RTHarnessRender(Viewport3D* vp, int w, int h, int pases);
bool RTHarnessPixel(int x, int y, unsigned char* rgb);
bool RTHarnessGuardar(const std::string& ruta);
int  RTHarnessTriangulos();
bool RTHarnessEnGpu();
// la ultima imagen del render a archivo (RGBA, filas de arriba abajo); false = no hay
bool RTHarnessImagen(std::vector<unsigned char>& rgba, int& w, int& h);
// el ultimo reescalado FSR del viewport: su tamano y el pixel del centro (false = no hubo)
bool RTHarnessFsrVista(int& w, int& h, unsigned char* rgbCentro);                    // el ultimo render del harness se hizo en la GPU
#endif // RAYTRACER_H
