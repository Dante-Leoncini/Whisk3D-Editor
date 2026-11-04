#ifndef W3D_DENOISER_H
#define W3D_DENOISER_H
// ============================================================================
//  Denoiser.h - LIMPIAR EL RUIDO del trazado de rayos (pocas muestras por pixel -> una imagen limpia).
//
//  TODO ES OPCIONAL: ninguno se enlaza al ejecutable. Se cargan al usarse (LoadLibrary / dlopen); si la libreria
//  no esta (Windows XP, un Linux viejo, una placa sin NVIDIA, el N95, la web) ese denoiser simplemente no aparece
//  en el desplegable y Whisk3D sigue igual.
//    - Open Image Denoise (Intel, Apache 2.0): CPU, en cualquier PC (y la GPU si la tiene). OpenImageDenoise.dll /
//      libOpenImageDenoise.so.2 al lado del ejecutable (tools/bajar_terceros.ps1).
//    - OptiX (NVIDIA): el denoiser de IA de la placa. La libreria la trae el DRIVER (nvoptix.dll + nvcuda.dll):
//      solo con una NVIDIA. Se compila si estan los headers (thirdparty/optix, W3D_OPTIX).
//    - NRD (NVIDIA, tiempo real): es del backend VULKAN (todavia no: no aparece).
//    - GPU (A-Trous): el propio, un filtro guiado por el albedo y la normal en la GPU (Vulkan): NVIDIA, AMD o Intel.
//  Entrada: el color (RGB float, lineal), y si hay, el ALBEDO y la NORMAL del primer impacto (mejoran mucho el
//  resultado: los bordes y las texturas no se lavan).
// ============================================================================
#include <string>

enum W3dDenoiser {
    W3D_DENOISER_NINGUNO = 0,
    W3D_DENOISER_OIDN,
    W3D_DENOISER_OPTIX,
    W3D_DENOISER_NRD,
    W3D_DENOISER_GPU,      // el propio (A-Trous en la GPU, render/RTGpu.h): cualquier GPU con Vulkan
    W3D_DENOISERS
};
const char* W3dDenoiserNombre(int d);        // en INGLES (la UI lo pasa por T())
bool W3dDenoiserDisponible(int d);           // la libreria esta en ESTA maquina (se prueba una vez)
// limpia 'color' (w*h*3 floats) en 'salida' (w*h*3). 'albedo' / 'normal' pueden ser NULL. false = no se pudo
// (la razon en 'err'): quien llama se queda con el color sin limpiar
bool W3dDenoiserAplicar(int d, const float* color, const float* albedo, const float* normal, int w, int h,
                        float* salida, std::string* err);
#endif // W3D_DENOISER_H
