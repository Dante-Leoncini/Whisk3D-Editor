// ============================================================================
//  cuda.h MINIMO de Whisk3D: solo los TIPOS opacos que nombran los headers de OptiX (optix_7_host.h incluye
//  <cuda.h>). Asi el denoiser de OptiX compila SIN el CUDA Toolkit: las funciones de CUDA se cargan del driver
//  (nvcuda.dll / libcuda.so.1) en render/Denoiser.cpp. CUdeviceptr ya lo define optix_7_types.h.
// ============================================================================
#ifndef W3D_CUDA_MIN_H
#define W3D_CUDA_MIN_H
typedef struct CUctx_st* CUcontext;
typedef struct CUstream_st* CUstream;
#endif
