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

const char* W3dDenoiserName(int d) {
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
bool W3dDenoiserAvailable(int d) { return d == W3D_DENOISER_NONE; }
bool W3dDenoiserApply(int, const float*, const float*, const float*, int, int, float*, std::string* err) {
    if (err) *err = "No denoiser on this platform";
    return false;
}
#else

#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE W3dLib;
    static W3dLib OpenLib(const char* n) { return LoadLibraryA(n); }
    static void* Symbol(W3dLib l, const char* s) { return (void*)GetProcAddress(l, s); }
#else
    #include <dlfcn.h>
    typedef void* W3dLib;
    static W3dLib OpenLib(const char* n) { return dlopen(n, RTLD_NOW | RTLD_LOCAL); }
    static void* Symbol(W3dLib l, const char* s) { return dlsym(l, s); }
#endif

// ============================================================================
//  OPEN IMAGE DENOISE (la API en C de OIDN 2: se declara aca lo poco que se usa, sin sus headers)
// ============================================================================
namespace {
typedef void* OIDNDevice; typedef void* OIDNFilter; typedef void* OIDNBuffer;
enum { kOIDN_FORMAT_FLOAT3 = 3 };
struct OIDN {
    bool tested, ok;
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
    OIDN() : tested(false), ok(false), dev(0) {}
};
OIDN gOidn;
}
static bool OidnLoad() {
    if (gOidn.tested) return gOidn.ok;
    gOidn.tested = true;
#ifdef _WIN32
    W3dLib l = OpenLib("OpenImageDenoise.dll");
#else
    W3dLib l = OpenLib("libOpenImageDenoise.so.2");
    if (!l) l = OpenLib("libOpenImageDenoise.so");
#endif
    if (!l) { w3dLogf("[denoiser] Open Image Denoise no esta (OpenImageDenoise.dll): no se ofrece"); return false; }
    gOidn.NewDevice = (OIDNDevice (*)(int))Symbol(l, "oidnNewDevice");
    gOidn.CommitDevice = (void (*)(OIDNDevice))Symbol(l, "oidnCommitDevice");
    gOidn.GetDeviceError = (int (*)(OIDNDevice, const char**))Symbol(l, "oidnGetDeviceError");
    gOidn.NewFilter = (OIDNFilter (*)(OIDNDevice, const char*))Symbol(l, "oidnNewFilter");
    gOidn.SetSharedFilterImage = (void (*)(OIDNFilter, const char*, void*, int, size_t, size_t, size_t, size_t, size_t))Symbol(l, "oidnSetSharedFilterImage");
    gOidn.SetFilterBool = (void (*)(OIDNFilter, const char*, bool))Symbol(l, "oidnSetFilterBool");
    gOidn.CommitFilter = (void (*)(OIDNFilter))Symbol(l, "oidnCommitFilter");
    gOidn.ExecuteFilter = (void (*)(OIDNFilter))Symbol(l, "oidnExecuteFilter");
    gOidn.ReleaseFilter = (void (*)(OIDNFilter))Symbol(l, "oidnReleaseFilter");
    gOidn.ReleaseDevice = (void (*)(OIDNDevice))Symbol(l, "oidnReleaseDevice");
    gOidn.NewBuffer = (OIDNBuffer (*)(OIDNDevice, size_t))Symbol(l, "oidnNewBuffer");
    gOidn.WriteBuffer = (void (*)(OIDNBuffer, size_t, size_t, const void*))Symbol(l, "oidnWriteBuffer");
    gOidn.ReadBuffer = (void (*)(OIDNBuffer, size_t, size_t, void*))Symbol(l, "oidnReadBuffer");
    gOidn.ReleaseBuffer = (void (*)(OIDNBuffer))Symbol(l, "oidnReleaseBuffer");
    gOidn.SetFilterImage = (void (*)(OIDNFilter, const char*, OIDNBuffer, int, size_t, size_t, size_t, size_t, size_t))Symbol(l, "oidnSetFilterImage");
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
static bool OidnApply(const float* color, const float* albedo, const float* normal, int w, int h, float* out, std::string* err) {
    if (!OidnLoad()) { if (err) *err = "Open Image Denoise is not installed"; return false; }
    OIDNFilter f = gOidn.NewFilter(gOidn.dev, "RT");
    if (!f) { if (err) *err = "OIDN: no filter"; return false; }
    const size_t bytes = (size_t)w * h * 3 * sizeof(float);
    const bool withBuffers = gOidn.NewBuffer && gOidn.WriteBuffer && gOidn.ReadBuffer && gOidn.ReleaseBuffer && gOidn.SetFilterImage;
    OIDNBuffer b[4] = { 0, 0, 0, 0 };   // color albedo normal salida
    if (withBuffers) {
        const float* src[3] = { color, albedo, (albedo && normal) ? normal : 0 };
        static const char* const name[4] = { "color", "albedo", "normal", "output" };
        for (int k = 0; k < 4; k++) {
            if (k < 3 && !src[k]) continue;
            b[k] = gOidn.NewBuffer(gOidn.dev, bytes);
            if (!b[k]) continue;
            if (k < 3) gOidn.WriteBuffer(b[k], 0, bytes, src[k]);
            gOidn.SetFilterImage(f, name[k], b[k], kOIDN_FORMAT_FLOAT3, (size_t)w, (size_t)h, 0, 0, 0);
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
    if (withBuffers) {
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
    bool tested, ok;
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
    CUDA() : tested(false), ok(false), ctx(0), optix(0) {}
};
CUDA gCu;
}
static bool OptixLoad() {
    if (gCu.tested) return gCu.ok;
    gCu.tested = true;
#ifdef _WIN32
    W3dLib l = OpenLib("nvcuda.dll");
#else
    W3dLib l = OpenLib("libcuda.so.1");
#endif
    if (!l) { w3dLogf("[denoiser] sin driver de NVIDIA (nvcuda): OptiX no se ofrece"); return false; }
    gCu.Init = (CUres (*)(unsigned int))Symbol(l, "cuInit");
    gCu.DeviceGet = (CUres (*)(int*, int))Symbol(l, "cuDeviceGet");
    gCu.CtxCreate = (CUres (*)(CUcontext*, unsigned int, int))Symbol(l, "cuCtxCreate_v2");
    gCu.MemAlloc = (CUres (*)(CUdeviceptr*, size_t))Symbol(l, "cuMemAlloc_v2");
    gCu.MemFree = (CUres (*)(CUdeviceptr))Symbol(l, "cuMemFree_v2");
    gCu.MemcpyHtoD = (CUres (*)(CUdeviceptr, const void*, size_t))Symbol(l, "cuMemcpyHtoD_v2");
    gCu.MemcpyDtoH = (CUres (*)(void*, CUdeviceptr, size_t))Symbol(l, "cuMemcpyDtoH_v2");
    gCu.CtxSynchronize = (CUres (*)())Symbol(l, "cuCtxSynchronize");
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
static bool OptixApply(const float* color, const float* albedo, const float* normal, int w, int h, float* out, std::string* err) {
    if (!OptixLoad()) { if (err) *err = "OptiX needs an NVIDIA card"; return false; }
    OptixDenoiserOptions o; memset(&o, 0, sizeof o);
    o.inputKind = albedo ? (normal ? OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL : OPTIX_DENOISER_INPUT_RGB_ALBEDO) : OPTIX_DENOISER_INPUT_RGB;
    OptixDenoiser den = 0;
    if (optixDenoiserCreate(gCu.optix, &o, &den) != OPTIX_SUCCESS) { if (err) *err = "OptiX: no denoiser"; return false; }
    bool ok = optixDenoiserSetModel(den, OPTIX_DENOISER_MODEL_KIND_LDR, 0, 0) == OPTIX_SUCCESS;
    OptixDenoiserSizes size; memset(&size, 0, sizeof size);
    ok = ok && optixDenoiserComputeMemoryResources(den, (unsigned)w, (unsigned)h, &size) == OPTIX_SUCCESS;
    const size_t bytes = (size_t)w * h * 3 * sizeof(float);
    CUdeviceptr dState = 0, dScratch = 0, dCol = 0, dAlb = 0, dNrm = 0, dOut = 0;
    ok = ok && gCu.MemAlloc(&dState, size.stateSizeInBytes) == 0 && gCu.MemAlloc(&dScratch, size.withoutOverlapScratchSizeInBytes) == 0 &&
         gCu.MemAlloc(&dCol, bytes) == 0 && gCu.MemAlloc(&dOut, bytes) == 0;
    if (ok && albedo) ok = gCu.MemAlloc(&dAlb, bytes) == 0;
    if (ok && albedo && normal) ok = gCu.MemAlloc(&dNrm, bytes) == 0;
    ok = ok && optixDenoiserSetup(den, 0, (unsigned)w, (unsigned)h, dState, size.stateSizeInBytes, dScratch, size.withoutOverlapScratchSizeInBytes) == OPTIX_SUCCESS;
    if (ok) {
        gCu.MemcpyHtoD(dCol, color, bytes);
        if (dAlb) gCu.MemcpyHtoD(dAlb, albedo, bytes);
        if (dNrm) gCu.MemcpyHtoD(dNrm, normal, bytes);
        OptixImage2D layers[3]; memset(layers, 0, sizeof layers);
        const CUdeviceptr ptrs[3] = { dCol, dAlb, dNrm };
        unsigned n = 1 + (dAlb ? 1u : 0u) + (dNrm ? 1u : 0u);
        for (unsigned i = 0; i < n; i++) {
            layers[i].data = ptrs[i]; layers[i].width = (unsigned)w; layers[i].height = (unsigned)h;
            layers[i].rowStrideInBytes = (unsigned)(w * 3 * sizeof(float)); layers[i].pixelStrideInBytes = 3 * sizeof(float);
            layers[i].format = OPTIX_PIXEL_FORMAT_FLOAT3;
        }
        OptixImage2D output = layers[0]; output.data = dOut;
        OptixDenoiserParams pair; memset(&pair, 0, sizeof pair);
        ok = optixDenoiserInvoke(den, 0, &pair, dState, size.stateSizeInBytes, layers, n, 0, 0, &output, dScratch, size.withoutOverlapScratchSizeInBytes) == OPTIX_SUCCESS;
        ok = ok && gCu.CtxSynchronize() == 0 && gCu.MemcpyDtoH(out, dOut, bytes) == 0;
    }
    const CUdeviceptr all[6] = { dState, dScratch, dCol, dAlb, dNrm, dOut };
    for (int i = 0; i < 6; i++) if (all[i]) gCu.MemFree(all[i]);
    optixDenoiserDestroy(den);
    if (!ok && err) *err = "OptiX: the denoiser failed";
    return ok;
}
#endif   // W3D_OPTIX

bool W3dDenoiserAvailable(int d) {
    switch (d) {
        case W3D_DENOISER_NONE: return true;
        case W3D_DENOISER_OIDN:    return OidnLoad();
#ifdef W3D_OPTIX
        case W3D_DENOISER_OPTIX:   return OptixLoad();
#endif
        case W3D_DENOISER_GPU:     return RTGpuAvailable();
        default: return false;   // (NRD: del backend Vulkan)
    }
}
bool W3dDenoiserApply(int d, const float* color, const float* albedo, const float* normal, int w, int h, float* output, std::string* err) {
    if (!color || !output || w <= 0 || h <= 0) return false;
    if (d == W3D_DENOISER_OIDN) return OidnApply(color, albedo, normal, w, h, output, err);
    if (d == W3D_DENOISER_GPU) return RTGpuDenoise(color, albedo, normal, w, h, output, err);
#ifdef W3D_OPTIX
    if (d == W3D_DENOISER_OPTIX) return OptixApply(color, albedo, normal, w, h, output, err);
#endif
    if (err) *err = "This denoiser is not available";
    return false;
}
#endif   // W3D_DENOISER_PC
