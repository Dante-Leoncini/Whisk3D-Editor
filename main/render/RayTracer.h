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
//      sombra ('rtRays') y su tamano ('rtRadius'): radio 0 = puntual = sombra dura; mas radio = penumbra;
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

struct W3dRTOptions {
    bool on;        // "Ray Tracing" (pestania Render). Apagado por defecto.
    int  rays;      // rayos de sombra por luz y por impacto (las lamparas con rtRays = 0 usan este)
    int  samples;   // muestras por pixel y por pase (bajo: 1)
    int  passes;    // pases hasta "done" (16 por defecto; 8 en el N95: a 333 MHz cada pase cuesta caro)
    int  threads;   // 0 = automatico (nucleos de la maquina); 1 = un solo hilo
    int  denoiser;  // W3dDenoiser (render/Denoiser.h): 0 = ninguno. Limpia el ruido al terminar cada pase
    bool gpu;       // trazar en la GPU si hay (render/RTGpu.h: Vulkan); sin GPU, o si falla, va por CPU
    bool onlyFinal; // el viewport muestra la imagen TERMINADA (todos los pases): no se ve como se va formando
    int  scale;     // REESCALADO del viewport: se traza a 1/escala de su resolucion (1 = apagado; 2, 3, 4) y se agranda con
                    // filtro lineal (con un denoiser, limpio antes de agrandar). El render a archivo va siempre entero
    int  upscaler; // COMO se agranda: 0 = lineal (la textura estirada); 1 = FSR 1 de AMD (render/UpscalingFSR.h, por
                      // CPU: bordes nitidos, en cualquier placa). En el telefono siempre lineal
    int  layers;     // CAPAS TRANSPARENTES que atraviesa un rayo (vidrios, hojas, pelo); pasadas esas, lo de atras queda negro
    bool seedVariable; // OTRO RUIDO EN CADA IMAGEN (default): con la misma semilla, en una animacion el ruido del
                          // trazado quedaba QUIETO en la pantalla (una trama pegada al vidrio); cambiandola se mueve y el
                          // ojo (y el denoiser) lo promedian parejo
    // DESENFOQUE DE MOVIMIENTO TRAZADO (solo el render a archivo): cada pase se traza en OTRO INSTANTE dentro del
    // obturador (los vertices y la camara entre los cuadros de al lado). Sale de verdad (sombras, reflejos, lo que hay
    // detras de lo que se mueve), no como el de los vectores; hacen falta pases para que no se vean las copias
    bool  blur;
    float shutter;     // la fraccion del cuadro que queda abierto (0.01 .. 1; 0.5 = el clasico de 180 grados)
    int   shutterPos;  // DONDE se abre: 0 = centrado en el cuadro, 1 = empieza en el cuadro, 2 = termina en el cuadro
    // REBOTES DE LA LUZ (luz indirecta): 0 = solo la luz directa (como siempre). N > 0: en cada impacto el rayo sigue
    // rebotando en una direccion al azar (difusa) hasta N superficies, y en cada una junta la luz de las lamparas: un
    // piso rojo tine la pared de al lado, un rincon a la sombra se aclara con lo que le llega de rebote
    int   bounces;
#ifdef W3D_SYMBIAN
    W3dRTOptions() : on(false), rays(1), samples(1), passes(8), threads(1), denoiser(0), gpu(false), onlyFinal(false), scale(1), upscaler(0), layers(8), seedVariable(true),
                      blur(false), shutter(0.5f), shutterPos(0), bounces(0) {}
#else
    W3dRTOptions() : on(false), rays(1), samples(1), passes(16), threads(0), denoiser(0), gpu(true), onlyFinal(false), scale(1), upscaler(1), layers(8), seedVariable(true),
                      blur(false), shutter(0.5f), shutterPos(0), bounces(0) {}
#endif
};
extern W3dRTOptions g_rt;
// el nombre de cada posicion del obturador (en ingles: T() lo traduce)
const char* RTShutterPosName(int pos);

// --- viewport: avanza el render progresivo de ESTE viewport y lo dibuja encima de la escena GL ---
void RTViewportStep(Viewport3D* vp);      // llamar despues de dibujar la escena, antes de los overlays
bool RTRenderState(Viewport3D* vp);       // el texto "Ray tracing: 3/16" (con las matrices 2D de la UI puestas); false = no hay
// el cartel de arriba a la izquierda (el mismo lugar que el del trazado): "Local View" mientras dura la vista local
void W3dDialogView(Viewport3D* vp, const char* text);
void RTInvalidate();                       // la escena cambio: el proximo frame arranca de cero
// LA SONDA DE LUZ (main/objects/LightProbe.h): la luz que LLEGA a cada punto (x,y,z del mundo, uno detras de otro) desde
// todas las direcciones -el fondo y lo que se ve iluminado alrededor-, con 'rayos' rayos por punto (repartidos parejo
// en la esfera). Sale como armonicos esfericos de orden 2 ya con la convolucion del coseno: 27 floats por punto (la luz
// difusa que le llega a una normal n del mundo es sum c_i * Y_i(n)). false = no hay escena
#include <vector>
// 'valido' (opcional): 0 en los puntos que quedaron ADENTRO de una malla (muchos rayos pegan en caras de espaldas)
bool RTBakeProbes(const std::vector<float>& pos, int rays, std::vector<float>& sh, std::vector<unsigned char>* valid = NULL);
// HORNEAR UNA SONDA DE A PEDAZOS (la barra del progreso en Propiedades, como el horneado de la luz): Empezar arma la
// escena, RTProbeStep avanza unos milisegundos (puntos de la de luz, filas del 360 de la de reflejo) y devuelve cuanto va
// (0..1); al terminar, Tomar entrega el resultado. Uno a la vez: empezar otro cancela el anterior
bool  RTProbeBeginLight(const std::vector<float>& pos, int rays);
bool  RTProbeBeginReflection(const float pos[3], int w, int h, float near, int samples, int bounces, float intensity,
                            std::string* reason);
float RTProbeStep(int msBudget);
bool  RTProbeActive();                  // trabajando (todavia no termino)
int   RTProbeType();                    // 0 = ninguna, 1 = de luz, 2 = de reflejo (tambien terminada y sin tomar)
void  RTProbeCancel();
bool  RTProbeTakeLight(std::vector<float>& sh, std::vector<unsigned char>* valid);
bool  RTProbeTakeReflection(std::vector<unsigned char>& rgba);
// PRECALCULAR LA ILUMINACION (el lightmap): la luz que le llega a cada texel de la malla 'm' en su capa UV 'uvLayer'
// (w x h; la fila es v * alto), con las lamparas (y sus sombras) y la luz indirecta ('muestras' rayos por texel). Solo
// con lo estatico. Se hace de a pedazos: Empezar, despues Paso en cada cuadro (devuelve el progreso, 1 = listo), y el
// resultado queda en Luz (rgb por texel, 1 = blanco) y Cubierto (1 = el texel es de una cara)
#include <string>
class Mesh;
// indirecta: la luz que rebota (el fondo y lo de alrededor), con 'rebotes' rebotes (1 a 8); denoiser: la indirecta suavizada
// shadowmask: SOMBRAS DEL SOL + INDIRECTA (TexLayer::shadowmask): Luz queda con la INDIRECTA sola y la mascara del sol
// (RTBakeSun) con cuanto VE AL SOL cada texel (la directa del sol con sombras / sin sombras, 0..1)
bool  RTBakeBegin(Mesh* m, int uvLayer, int w, int h, int samples, std::string* reason,
                        bool indirect = true, int bounces = 3, bool denoiser = true, bool shadowmask = false);
bool  RTBakeShadowmask();   // (el horneado terminado era de sombras + indirecta)
float RTBakeStep(int msBudget);
bool  RTBakeActive();
void  RTBakeCancel();
const std::vector<float>& RTBakeLight();
// LA MASCARA DEL SOL del horneado terminado: de cada texel, que fraccion de su luz es la directa del sol (0..1). Va al alfa
// de la textura (Materials.h TexLayer::maskSun)
const std::vector<float>& RTBakeSun();
const std::vector<unsigned char>& RTCoveredBake();
// la malla horneada era un CAPTADOR DE SOMBRAS (Object::catchesShadows al empezar): su luz es la sombra por la oclusion
// ambiental, de 0 a 1 (blanco = no le cae nada), para una capa Multiply (1x)
bool RTBakePicker();
// LA SONDA DE REFLEJO HORNEADA: la imagen 360 (w x h, RGBA) de lo que se ve desde 'pos' (objects/ReflectionProbe.h), con lo
// estatico, el fondo de la escena, 'muestras' rayos por pixel y 'rebotes' de la luz
bool RTBakeReflection(const float pos[3], int w, int h, float near, int samples, int bounces, float intensity,
                      std::vector<unsigned char>& rgba, std::string* reason);
bool RTActiveIn(const Viewport3D* vp);    // hay un render progresivo de ese viewport (para seguir redibujando)
float RTImagesPerSecond();
const char* RTUpscalerName(int r);  // "Linear" / "FSR 1 (AMD)"             // las imagenes trazadas TERMINADAS por segundo (lo que de verdad se ve)
double RTRaysPerSecond();               // los rayos por segundo de la GPU mientras traza (0 = por CPU)

// --- render a archivo ("Render Image" con Ray Tracing tildado): todos los pases, bloqueante, con progreso ---
// RENDER ANIMATION: entre Begin y End la escena trazada se arma una vez y cada cuadro renueva solo las poses, las luces y
// la camara (si cambia otra cosa, ese cuadro la arma de nuevo). Sin esto cada cuadro la armaba y la subia entera
void RTSequenceBegin();
void RTSequenceEnd();
bool RTRenderToPNG(Viewport3D* vp, int w, int h, const std::string& path, std::string& msg);
// lo mismo a un buffer RGBA (filas de arriba hacia abajo), sin guardar (para post-procesarlo: el desenfoque de movimiento)
bool RTRenderToBuffer(Viewport3D* vp, int w, int h, std::vector<unsigned char>& rgba, std::string& msg);
// EL TRAZADO POR FRANJAS directo al archivo (ver el .cpp): de a 'stripeHeight' filas, de arriba hacia abajo, a 'fn'
typedef bool (*W3dStripeFn)(void* ctx, const unsigned char* rgba, int y0, int height);
bool RTRenderStripes(Viewport3D* vp, int w, int h, int stripeHeight, W3dStripeFn fn, void* ctx, std::string& msg);

// --- pruebas (harness): render sincronico a un buffer propio + lectura de pixeles ---
bool RTHarnessRender(Viewport3D* vp, int w, int h, int passes);
bool RTHarnessPixel(int x, int y, unsigned char* rgb);
bool RTHarnessSave(const std::string& path);
int  RTHarnessTriangles();
bool RTHarnessInGpu();
// la ultima imagen del render a archivo (RGBA, filas de arriba abajo); false = no hay
bool RTHarnessImage(std::vector<unsigned char>& rgba, int& w, int& h);
// el ultimo reescalado FSR del viewport: su tamano y el pixel del centro (false = no hubo)
bool RTHarnessFsrView(int& w, int& h, unsigned char* rgbCenter);                    // el ultimo render del harness se hizo en la GPU
#endif // RAYTRACER_H
