#ifndef W3D_RTGPU_H
#define W3D_RTGPU_H
// ============================================================================
//  RTGpu.h - EL TRAZADO DE RAYOS EN LA GPU: el mismo algoritmo del de CPU (RayTracer.cpp: luz directa con sombras
//  suaves, transparencias, metal con reflejos, los mapas del material, normal map) en un compute shader de Vulkan,
//  con el mismo BVH. Anda en cualquier GPU con Vulkan (NVIDIA, AMD, Intel) y con CUALQUIER backend grafico del
//  editor: arma su propio dispositivo de computo (sin ventana) y devuelve la imagen como el de CPU.
//
//  Opcional como todo: sin vulkan-1 / shaderc_shared (o sin una GPU con Vulkan) RTGpuAvailable da false y el
//  trazado sigue por CPU. Tambien trae un DENOISER propio (A-Trous guiado por el albedo y la normal), que corre en la
//  misma GPU: "GPU (A-Trous)" en el desplegable de denoisers.
// ============================================================================
#include <vector>
#include <string>
#include <stdint.h>

// la escena en arrays planos (los arma RayTracer.cpp desde su RTScene)
struct RTGpuScene {
    std::vector<float> verts;    // por vertice 12 floats: pos.xyz u, nrm.xyz v, tan.xyzw
    std::vector<int> tris;       // por triangulo 4: v0 v1 v2 material
    std::vector<float> mats;     // por material 28 floats (ver el shader: Mat)
    std::vector<float> lights;    // por luz 24 floats (Luz)
    std::vector<float> nodes;    // por nodo del BVH 12 floats (bmin izq, bmax der, primero cantidad)
    std::vector<int> order;      // los triangulos en el orden de las hojas
    std::vector<int> texInfo;    // por textura 4: desde, w, h, 0
    std::vector<uint32_t> texels;// todas las texturas RGBA8 seguidas
    float bg[3];
    int   envMat;    // el FONDO 360: un material extra al final de 'mats' (rotacion + textura); -1 = el color 'bg'
    bool hasTransparent;
    RTGpuScene() : envMat(-1), hasTransparent(false) { bg[0] = bg[1] = bg[2] = 0.0f; }
};
// la lente del viewport (RayTracer.cpp: RTCamera): ndc = (e.xy / (sx, sy)) [/ ez] * zoom + pan
struct RTGpuCamera { float pos[3], right[3], arr[3], forward[3]; float sx, sy, zoom, panX, panY; bool ortho;
                     unsigned int seed;   // la del ruido de esta imagen (0 = siempre la misma)
                     float lens, focus;   // la profundidad de campo: radio de la lente (m, 0 = nada) y distancia enfocada
                     RTGpuCamera() : seed(0), lens(0.0f), focus(0.0f) {} };

bool RTGpuAvailable();         // hay una GPU con Vulkan y el compilador de shaders (se prueba una vez)
const char* RTGpuName();       // la GPU que traza
bool RTGpuWithCoresRT();       // traza con los nucleos de trazado de la placa (VK_KHR_ray_query: RTX, RDNA2+, Arc)
bool RTGpuLoadScene(const RTGpuScene& e);
bool RTGpuImage(int w, int h);  // la imagen a acumular (en cero)
// solo los VERTICES y los NODOS del BVH de nuevo (los mismos triangulos, otra pose): con los nucleos de trazado, sus
// estructuras se rehacen de los vertices nuevos
bool RTGpuUpdateGeometry(const std::vector<float>& verts, const std::vector<float>& nodes);
bool RTGpuUpdateLights(const std::vector<float>& lights);   // solo las luces (24 floats cada una)
// las CAUSTICAS de un pase: los fotones (12 floats: posicion, luz, normal; con su w) y la grilla (cabecera de 4 floats: radio,
// 1/lado, celdas, prendidas; despues el primer foton de cada celda). La cabecera en cero = sin causticas
bool RTGpuUpdateCaustics(const std::vector<float>& photons, const std::vector<int>& grid);
// un PASE: 'samples' muestras por pixel sumadas al acumulador. 'paso' = pases ya hechos (semilla y promedio); con aov
// calcula ademas el albedo y la normal del primer impacto (para el denoiser)
// block > 1: la VISTA PREVIA (un rayo por bloque de block x block pixeles, sin tocar lo acumulado). row0 / rows: solo esa
// FRANJA de filas del pase (rows 0 = la imagen entera; row0 hasta 4095): el visor traza de a franjas y la UI no se traba
bool RTGpuPass(const RTGpuCamera& c, int step, int samples, bool aov, int layers, int bounces = 0, int block = 0, int row0 = 0, int rows = 0);   // rebotes: los de la luz (g_rt.bounces)   // capas: las transparentes que se atraviesan
// lo acumulado: la imagen de 8 bits (el promedio de 'pasos'), y si se piden, el color medio y los AOV (3 floats/px)
bool RTGpuRead(unsigned char* rgba, float* middleColor, float* alb, float* nrm, int steps);
unsigned long long RTGpuRays();   // los rayos trazados desde la ultima vez (primarios, de sombra, reflejos)
// el denoiser propio sobre lo que la GPU ya tiene (el trazado de RTGpuPass, con los AOV del primer pase): la imagen de 8
// bits queda limpia, sin ida y vuelta (RTGpuRead la baja como siempre). Es el que va en tiempo real
bool RTGpuDenoiseAccumulated(int steps);
// el denoiser propio (A-Trous): color / albedo / normal de w*h*3 floats -> salida
bool RTGpuDenoise(const float* color, const float* alb, const float* nrm, int w, int h, float* output, std::string* err);
#endif
