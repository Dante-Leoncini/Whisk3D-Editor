#ifndef W3D_RTGPU_H
#define W3D_RTGPU_H
// ============================================================================
//  RTGpu.h - EL TRAZADO DE RAYOS EN LA GPU: el mismo algoritmo del de CPU (RayTracer.cpp: luz directa con sombras
//  suaves, transparencias, metal con reflejos, los mapas del material, normal map) en un compute shader de Vulkan,
//  con el mismo BVH. Anda en cualquier GPU con Vulkan (NVIDIA, AMD, Intel) y con CUALQUIER backend grafico del
//  editor: arma su propio dispositivo de computo (sin ventana) y devuelve la imagen como el de CPU.
//
//  Opcional como todo: sin vulkan-1 / shaderc_shared (o sin una GPU con Vulkan) RTGpuDisponible da false y el
//  trazado sigue por CPU. Tambien trae un DENOISER propio (A-Trous guiado por el albedo y la normal), que corre en la
//  misma GPU: "GPU (A-Trous)" en el desplegable de denoisers.
// ============================================================================
#include <vector>
#include <string>
#include <stdint.h>

// la escena en arrays planos (los arma RayTracer.cpp desde su RTEscena)
struct RTGpuEscena {
    std::vector<float> verts;    // por vertice 12 floats: pos.xyz u, nrm.xyz v, tan.xyzw
    std::vector<int> tris;       // por triangulo 4: v0 v1 v2 material
    std::vector<float> mats;     // por material 20 floats (ver el shader: Mat)
    std::vector<float> luces;    // por luz 20 floats (Luz)
    std::vector<float> nodos;    // por nodo del BVH 12 floats (bmin izq, bmax der, primero cantidad)
    std::vector<int> orden;      // los triangulos en el orden de las hojas
    std::vector<int> texInfo;    // por textura 4: desde, w, h, 0
    std::vector<uint32_t> texels;// todas las texturas RGBA8 seguidas
    float bg[3];
    bool hayTransparentes;
};
// la lente del viewport (RayTracer.cpp: RTCamara): ndc = (e.xy / (sx, sy)) [/ ez] * zoom + pan
struct RTGpuCamara { float pos[3], der[3], arr[3], adel[3]; float sx, sy, zoom, panX, panY; bool orto;
                     unsigned int semilla;   // la del ruido de esta imagen (0 = siempre la misma)
                     float lente, foco;      // la profundidad de campo: radio de la lente (m, 0 = nada) y distancia enfocada
                     RTGpuCamara() : semilla(0), lente(0.0f), foco(0.0f) {} };

bool RTGpuDisponible();          // hay una GPU con Vulkan y el compilador de shaders (se prueba una vez)
const char* RTGpuNombre();       // la GPU que traza
bool RTGpuConNucleosRT();        // traza con los nucleos de trazado de la placa (VK_KHR_ray_query: RTX, RDNA2+, Arc)
bool RTGpuCargarEscena(const RTGpuEscena& e);
bool RTGpuImagen(int w, int h);  // la imagen a acumular (en cero)
// solo los VERTICES y los NODOS del BVH de nuevo (los mismos triangulos, otra pose): con los nucleos de trazado, sus
// estructuras se rehacen de los vertices nuevos
bool RTGpuActualizarGeometria(const std::vector<float>& verts, const std::vector<float>& nodos);
bool RTGpuActualizarLuces(const std::vector<float>& luces);   // solo las luces (20 floats cada una)
// un PASE: 'samples' muestras por pixel sumadas al acumulador. 'paso' = pases ya hechos (semilla y promedio); con aov
// calcula ademas el albedo y la normal del primer impacto (para el denoiser)
bool RTGpuPase(const RTGpuCamara& c, int paso, int samples, bool aov, int capas);   // capas: las transparentes que se atraviesan
// lo acumulado: la imagen de 8 bits (el promedio de 'pasos'), y si se piden, el color medio y los AOV (3 floats/px)
bool RTGpuLeer(unsigned char* rgba, float* colorMedio, float* alb, float* nrm, int pasos);
unsigned long long RTGpuRayos();   // los rayos trazados desde la ultima vez (primarios, de sombra, reflejos)
// el denoiser propio sobre lo que la GPU ya tiene (el trazado de RTGpuPase, con los AOV del primer pase): la imagen de 8
// bits queda limpia, sin ida y vuelta (RTGpuLeer la baja como siempre). Es el que va en tiempo real
bool RTGpuDenoisearLoAcumulado(int pasos);
// el denoiser propio (A-Trous): color / albedo / normal de w*h*3 floats -> salida
bool RTGpuDenoisear(const float* color, const float* alb, const float* nrm, int w, int h, float* salida, std::string* err);
#endif
