// ============================================================================
//  Denoiser.cpp - ver Denoiser.h. Las librerias se CARGAN al usarse: el ejecutable no depende de ninguna.
// ============================================================================
#include "Denoiser.h"
#include <string.h>
#include <vector>
#include "w3dlog.h"
#include "RTGpu.h"   // el propio: A-Trous en la GPU

#if (defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))) && !defined(W3D_SYMBIAN) && !defined(__EMSCRIPTEN__)
    #define W3D_DENOISER_PC 1
#endif

const char* W3dDenoiserNombre(int d) {
    switch (d) {
        case W3D_DENOISER_OIDN:  return "Open Image Denoise";
        case W3D_DENOISER_OPTIX: return "OptiX (NVIDIA)";
        case W3D_DENOISER_NRD:   return "NRD (real time)";
        case W3D_DENOISER_GPU:   return "GPU (A-Trous)";
        default:                 return "None";
    }
}

#ifndef W3D_DENOISER_PC
// (el N95, Android, la web, las consolas: sin denoisers)
bool W3dDenoiserDisponible(int d) { return d == W3D_DENOISER_NINGUNO; }
bool W3dDenoiserAplicar(int, const float*, const float*, const float*, int, int, float*, std::string* err) {
    if (err) *err = "No denoiser on this platform";
    return false;
}
#else

#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE W3dLib;
    static W3dLib AbrirLib(const char* n) { return LoadLibraryA(n); }
    static void* Simbolo(W3dLib l, const char* s) { return (void*)GetProcAddress(l, s); }
#else
    #include <dlfcn.h>
    typedef void* W3dLib;
    static W3dLib AbrirLib(const char* n) { return dlopen(n, RTLD_NOW | RTLD_LOCAL); }
    static void* Simbolo(W3dLib l, const char* s) { return dlsym(l, s); }
#endif

// ============================================================================
//  OPEN IMAGE DENOISE (la API en C de OIDN 2: se declara aca lo poco que se usa, sin sus headers)
// ============================================================================
namespace {
typedef void* OIDNDevice; typedef void* OIDNFilter; typedef void* OIDNBuffer;
enum { kOIDN_FORMAT_FLOAT3 = 3 };
struct OIDN {
    bool probado, ok;
    OIDNDevice (*NewDevice)(int);
    void (*CommitDevice)(OIDNDevice);
    int (*GetDeviceError)(OIDNDevice, const char**);
    OIDNFilter (*NewFilter)(OIDNDevice, const char*);
    void (*SetSharedFilterImage)(OIDNFilter, const char*, void*, int, size_t, size_t, size_t, size_t, size_t);
    void (*SetFilterBool)(OIDNFilter, const char*, bool);
    void (*CommitFilter)(OIDNFilter);
    void (*ExecuteFilter)(OIDNFilter);
    void (*ReleaseFilter)(OIDNFilter);
    void (*ReleaseDevice)(OIDNDevice);
    // los BUFFERS de OIDN (2.0+): con un dispositivo GPU (CUDA, con los drivers nuevos lo elige solo) la memoria comun no
    // le llega ("image data not accessible by the device"): se copia a buffers suyos. Sin estas funciones, compartida
    OIDNBuffer (*NewBuffer)(OIDNDevice, size_t);
    void (*WriteBuffer)(OIDNBuffer, size_t, size_t, const void*);
    void (*ReadBuffer)(OIDNBuffer, size_t, size_t, void*);
    void (*ReleaseBuffer)(OIDNBuffer);
    void (*SetFilterImage)(OIDNFilter, const char*, OIDNBuffer, int, size_t, size_t, size_t, size_t, size_t);
    OIDNDevice dev;
    OIDN() : probado(false), ok(false), dev(0) {}
};
OIDN gOidn;
}
static bool OidnCargar() {
    if (gOidn.probado) return gOidn.ok;
    gOidn.probado = true;
#ifdef _WIN32
    W3dLib l = AbrirLib("OpenImageDenoise.dll");
#else
    W3dLib l = AbrirLib("libOpenImageDenoise.so.2");
    if (!l) l = AbrirLib("libOpenImageDenoise.so");
#endif
    if (!l) { w3dLogf("[denoiser] Open Image Denoise no esta (OpenImageDenoise.dll): no se ofrece"); return false; }
    gOidn.NewDevice = (OIDNDevice (*)(int))Simbolo(l, "oidnNewDevice");
    gOidn.CommitDevice = (void (*)(OIDNDevice))Simbolo(l, "oidnCommitDevice");
    gOidn.GetDeviceError = (int (*)(OIDNDevice, const char**))Simbolo(l, "oidnGetDeviceError");
    gOidn.NewFilter = (OIDNFilter (*)(OIDNDevice, const char*))Simbolo(l, "oidnNewFilter");
    gOidn.SetSharedFilterImage = (void (*)(OIDNFilter, const char*, void*, int, size_t, size_t, size_t, size_t, size_t))Simbolo(l, "oidnSetSharedFilterImage");
    gOidn.SetFilterBool = (void (*)(OIDNFilter, const char*, bool))Simbolo(l, "oidnSetFilterBool");
    gOidn.CommitFilter = (void (*)(OIDNFilter))Simbolo(l, "oidnCommitFilter");
    gOidn.ExecuteFilter = (void (*)(OIDNFilter))Simbolo(l, "oidnExecuteFilter");
    gOidn.ReleaseFilter = (void (*)(OIDNFilter))Simbolo(l, "oidnReleaseFilter");
    gOidn.ReleaseDevice = (void (*)(OIDNDevice))Simbolo(l, "oidnReleaseDevice");
    gOidn.NewBuffer = (OIDNBuffer (*)(OIDNDevice, size_t))Simbolo(l, "oidnNewBuffer");
    gOidn.WriteBuffer = (void (*)(OIDNBuffer, size_t, size_t, const void*))Simbolo(l, "oidnWriteBuffer");
    gOidn.ReadBuffer = (void (*)(OIDNBuffer, size_t, size_t, void*))Simbolo(l, "oidnReadBuffer");
    gOidn.ReleaseBuffer = (void (*)(OIDNBuffer))Simbolo(l, "oidnReleaseBuffer");
    gOidn.SetFilterImage = (void (*)(OIDNFilter, const char*, OIDNBuffer, int, size_t, size_t, size_t, size_t, size_t))Simbolo(l, "oidnSetFilterImage");
    if (!gOidn.NewDevice || !gOidn.CommitDevice || !gOidn.GetDeviceError || !gOidn.NewFilter || !gOidn.SetSharedFilterImage ||
        !gOidn.SetFilterBool || !gOidn.CommitFilter || !gOidn.ExecuteFilter || !gOidn.ReleaseFilter) {
        w3dLogfW("[denoiser] OpenImageDenoise sin las funciones de la API 2: no se usa"); return false;
    }
    gOidn.dev = gOidn.NewDevice(0);   // (OIDN_DEVICE_TYPE_DEFAULT: la GPU si la soporta, si no la CPU)
    if (!gOidn.dev) return false;
    gOidn.CommitDevice(gOidn.dev);
    const char* m = 0;
    if (gOidn.GetDeviceError(gOidn.dev, &m) != 0) { w3dLogfW("[denoiser] OIDN: %s", m ? m : "?"); return false; }
    gOidn.ok = true;
    w3dLogf("[denoiser] Open Image Denoise listo");
    return true;
}
static bool OidnAplicar(const float* color, const float* albedo, const float* normal, int w, int h, float* out, std::string* err) {
    if (!OidnCargar()) { if (err) *err = "Open Image Denoise is not installed"; return false; }
    OIDNFilter f = gOidn.NewFilter(gOidn.dev, "RT");
    if (!f) { if (err) *err = "OIDN: no filter"; return false; }
    const size_t bytes = (size_t)w * h * 3 * sizeof(float);
    const bool conBuffers = gOidn.NewBuffer && gOidn.WriteBuffer && gOidn.ReadBuffer && gOidn.ReleaseBuffer && gOidn.SetFilterImage;
    OIDNBuffer b[4] = { 0, 0, 0, 0 };   // color albedo normal salida
    if (conBuffers) {
        const float* src[3] = { color, albedo, (albedo && normal) ? normal : 0 };
        static const char* const nom[4] = { "color", "albedo", "normal", "output" };
        for (int k = 0; k < 4; k++) {
            if (k < 3 && !src[k]) continue;
            b[k] = gOidn.NewBuffer(gOidn.dev, bytes);
            if (!b[k]) continue;
            if (k < 3) gOidn.WriteBuffer(b[k], 0, bytes, src[k]);
            gOidn.SetFilterImage(f, nom[k], b[k], kOIDN_FORMAT_FLOAT3, (size_t)w, (size_t)h, 0, 0, 0);
        }
    } else {
        gOidn.SetSharedFilterImage(f, "color", (void*)color, kOIDN_FORMAT_FLOAT3, (size_t)w, (size_t)h, 0, 0, 0);
        if (albedo) gOidn.SetSharedFilterImage(f, "albedo", (void*)albedo, kOIDN_FORMAT_FLOAT3, (size_t)w, (size_t)h, 0, 0, 0);
        if (albedo && normal) gOidn.SetSharedFilterImage(f, "normal", (void*)normal, kOIDN_FORMAT_FLOAT3, (size_t)w, (size_t)h, 0, 0, 0);
        gOidn.SetSharedFilterImage(f, "output", out, kOIDN_FORMAT_FLOAT3, (size_t)w, (size_t)h, 0, 0, 0);
    }
    gOidn.SetFilterBool(f, "hdr", false);   // (el color ya va de 0 a 1)
    gOidn.CommitFilter(f);
    gOidn.ExecuteFilter(f);
    const char* m = 0;
    int e = gOidn.GetDeviceError(gOidn.dev, &m);
    if (conBuffers) {
        if (e == 0 && b[3]) gOidn.ReadBuffer(b[3], 0, bytes, out);
        for (int k = 0; k < 4; k++) if (b[k]) gOidn.ReleaseBuffer(b[k]);
        if (e == 0) e = gOidn.GetDeviceError(gOidn.dev, &m);
    }
    gOidn.ReleaseFilter(f);
    if (e != 0) { if (err) *err = std::string("OIDN: ") + (m ? m : "?"); return false; }
    return true;
}

// ============================================================================
//  OPTIX (NVIDIA): el denoiser de la placa. Los headers de OptiX 7.2 (thirdparty/optix) + un cuda.h minimo
//  (render/cuda_min): las funciones de CUDA salen de nvcuda.dll del DRIVER (sin el CUDA Toolkit)
// ============================================================================
#ifdef W3D_OPTIX
#include <optix.h>
#include <optix_stubs.h>
#include <optix_function_table_definition.h>
namespace {
typedef int CUres;
struct CUDA {
    bool probado, ok;
    CUres (*Init)(unsigned int);
    CUres (*DeviceGet)(int*, int);
    CUres (*CtxCreate)(CUcontext*, unsigned int, int);
    CUres (*MemAlloc)(CUdeviceptr*, size_t);
    CUres (*MemFree)(CUdeviceptr);
    CUres (*MemcpyHtoD)(CUdeviceptr, const void*, size_t);
    CUres (*MemcpyDtoH)(void*, CUdeviceptr, size_t);
    CUres (*CtxSynchronize)();
    CUcontext ctx;
    OptixDeviceContext optix;
    CUDA() : probado(false), ok(false), ctx(0), optix(0) {}
};
CUDA gCu;
}
static bool OptixCargar() {
    if (gCu.probado) return gCu.ok;
    gCu.probado = true;
#ifdef _WIN32
    W3dLib l = AbrirLib("nvcuda.dll");
#else
    W3dLib l = AbrirLib("libcuda.so.1");
#endif
    if (!l) { w3dLogf("[denoiser] sin driver de NVIDIA (nvcuda): OptiX no se ofrece"); return false; }
    gCu.Init = (CUres (*)(unsigned int))Simbolo(l, "cuInit");
    gCu.DeviceGet = (CUres (*)(int*, int))Simbolo(l, "cuDeviceGet");
    gCu.CtxCreate = (CUres (*)(CUcontext*, unsigned int, int))Simbolo(l, "cuCtxCreate_v2");
    gCu.MemAlloc = (CUres (*)(CUdeviceptr*, size_t))Simbolo(l, "cuMemAlloc_v2");
    gCu.MemFree = (CUres (*)(CUdeviceptr))Simbolo(l, "cuMemFree_v2");
    gCu.MemcpyHtoD = (CUres (*)(CUdeviceptr, const void*, size_t))Simbolo(l, "cuMemcpyHtoD_v2");
    gCu.MemcpyDtoH = (CUres (*)(void*, CUdeviceptr, size_t))Simbolo(l, "cuMemcpyDtoH_v2");
    gCu.CtxSynchronize = (CUres (*)())Simbolo(l, "cuCtxSynchronize");
    if (!gCu.Init || !gCu.DeviceGet || !gCu.CtxCreate || !gCu.MemAlloc || !gCu.MemFree || !gCu.MemcpyHtoD || !gCu.MemcpyDtoH || !gCu.CtxSynchronize) return false;
    int dev = 0;
    if (gCu.Init(0) != 0 || gCu.DeviceGet(&dev, 0) != 0 || gCu.CtxCreate(&gCu.ctx, 0, dev) != 0) { w3dLogfW("[denoiser] CUDA no arranco"); return false; }
    if (optixInit() != OPTIX_SUCCESS) { w3dLogfW("[denoiser] el driver no trae OptiX (nvoptix)"); return false; }
    OptixDeviceContextOptions op; memset(&op, 0, sizeof op);
    if (optixDeviceContextCreate(gCu.ctx, &op, &gCu.optix) != OPTIX_SUCCESS) { w3dLogfW("[denoiser] OptiX: no hay contexto"); return false; }
    gCu.ok = true;
    w3dLogf("[denoiser] OptiX listo");
    return true;
}
static bool OptixAplicar(const float* color, const float* albedo, const float* normal, int w, int h, float* out, std::string* err) {
    if (!OptixCargar()) { if (err) *err = "OptiX needs an NVIDIA card"; return false; }
    OptixDenoiserOptions o; memset(&o, 0, sizeof o);
    o.inputKind = albedo ? (normal ? OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL : OPTIX_DENOISER_INPUT_RGB_ALBEDO) : OPTIX_DENOISER_INPUT_RGB;
    OptixDenoiser den = 0;
    if (optixDenoiserCreate(gCu.optix, &o, &den) != OPTIX_SUCCESS) { if (err) *err = "OptiX: no denoiser"; return false; }
    bool ok = optixDenoiserSetModel(den, OPTIX_DENOISER_MODEL_KIND_LDR, 0, 0) == OPTIX_SUCCESS;
    OptixDenoiserSizes tam; memset(&tam, 0, sizeof tam);
    ok = ok && optixDenoiserComputeMemoryResources(den, (unsigned)w, (unsigned)h, &tam) == OPTIX_SUCCESS;
    const size_t bytes = (size_t)w * h * 3 * sizeof(float);
    CUdeviceptr dEstado = 0, dScratch = 0, dCol = 0, dAlb = 0, dNrm = 0, dOut = 0;
    ok = ok && gCu.MemAlloc(&dEstado, tam.stateSizeInBytes) == 0 && gCu.MemAlloc(&dScratch, tam.withoutOverlapScratchSizeInBytes) == 0 &&
         gCu.MemAlloc(&dCol, bytes) == 0 && gCu.MemAlloc(&dOut, bytes) == 0;
    if (ok && albedo) ok = gCu.MemAlloc(&dAlb, bytes) == 0;
    if (ok && albedo && normal) ok = gCu.MemAlloc(&dNrm, bytes) == 0;
    ok = ok && optixDenoiserSetup(den, 0, (unsigned)w, (unsigned)h, dEstado, tam.stateSizeInBytes, dScratch, tam.withoutOverlapScratchSizeInBytes) == OPTIX_SUCCESS;
    if (ok) {
        gCu.MemcpyHtoD(dCol, color, bytes);
        if (dAlb) gCu.MemcpyHtoD(dAlb, albedo, bytes);
        if (dNrm) gCu.MemcpyHtoD(dNrm, normal, bytes);
        OptixImage2D capas[3]; memset(capas, 0, sizeof capas);
        const CUdeviceptr ptrs[3] = { dCol, dAlb, dNrm };
        unsigned n = 1 + (dAlb ? 1u : 0u) + (dNrm ? 1u : 0u);
        for (unsigned i = 0; i < n; i++) {
            capas[i].data = ptrs[i]; capas[i].width = (unsigned)w; capas[i].height = (unsigned)h;
            capas[i].rowStrideInBytes = (unsigned)(w * 3 * sizeof(float)); capas[i].pixelStrideInBytes = 3 * sizeof(float);
            capas[i].format = OPTIX_PIXEL_FORMAT_FLOAT3;
        }
        OptixImage2D salida = capas[0]; salida.data = dOut;
        OptixDenoiserParams par; memset(&par, 0, sizeof par);
        ok = optixDenoiserInvoke(den, 0, &par, dEstado, tam.stateSizeInBytes, capas, n, 0, 0, &salida, dScratch, tam.withoutOverlapScratchSizeInBytes) == OPTIX_SUCCESS;
        ok = ok && gCu.CtxSynchronize() == 0 && gCu.MemcpyDtoH(out, dOut, bytes) == 0;
    }
    const CUdeviceptr todos[6] = { dEstado, dScratch, dCol, dAlb, dNrm, dOut };
    for (int i = 0; i < 6; i++) if (todos[i]) gCu.MemFree(todos[i]);
    optixDenoiserDestroy(den);
    if (!ok && err) *err = "OptiX: the denoiser failed";
    return ok;
}
#endif   // W3D_OPTIX

bool W3dDenoiserDisponible(int d) {
    switch (d) {
        case W3D_DENOISER_NINGUNO: return true;
        case W3D_DENOISER_OIDN:    return OidnCargar();
#ifdef W3D_OPTIX
        case W3D_DENOISER_OPTIX:   return OptixCargar();
#endif
        case W3D_DENOISER_GPU:     return RTGpuDisponible();
        default: return false;   // (NRD: del backend Vulkan)
    }
}
bool W3dDenoiserAplicar(int d, const float* color, const float* albedo, const float* normal, int w, int h, float* salida, std::string* err) {
    if (!color || !salida || w <= 0 || h <= 0) return false;
    if (d == W3D_DENOISER_OIDN) return OidnAplicar(color, albedo, normal, w, h, salida, err);
    if (d == W3D_DENOISER_GPU) return RTGpuDenoisear(color, albedo, normal, w, h, salida, err);
#ifdef W3D_OPTIX
    if (d == W3D_DENOISER_OPTIX) return OptixAplicar(color, albedo, normal, w, h, salida, err);
#endif
    if (err) *err = "This denoiser is not available";
    return false;
}
#endif   // W3D_DENOISER_PC
