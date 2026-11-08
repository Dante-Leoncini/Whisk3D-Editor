// ============================================================================
//  RTGpu.cpp - el trazado de rayos en la GPU (ver RTGpu.h): un dispositivo de Vulkan SOLO de computo (sin ventana,
//  aparte del backend grafico), los buffers de la escena en la memoria de la GPU y dos compute shaders
//  (RTGpuShader.inc: trazar y el denoiser A-Trous). Vulkan se abre a mano (vulkan-1 / libvulkan.so.1) y los shaders
//  los compila shaderc (graphics/vulkan/w3dVk.h): nada enlazado.
// ============================================================================
#include "render/RTGpu.h"
#if defined(W3D_VULKAN)
#include "graphics/vulkan/w3dVk.h"
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#include "w3dlog.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "render/RTGpuShader.inc"

#define RTG_GLOBAL(X) X(vkCreateInstance)
#define RTG_INST(X) X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties) X(vkCreateDevice) X(vkGetDeviceProcAddr) \
    X(vkEnumerateDeviceExtensionProperties)
#define RTG_DEV(X) X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkQueueSubmit) X(vkQueueWaitIdle) X(vkCreateCommandPool) \
    X(vkAllocateCommandBuffers) X(vkResetCommandBuffer) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCreateFence) \
    X(vkWaitForFences) X(vkResetFences) X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) \
    X(vkAllocateMemory) X(vkFreeMemory) X(vkBindBufferMemory) X(vkMapMemory) X(vkCreateShaderModule) \
    X(vkCreateDescriptorSetLayout) X(vkCreatePipelineLayout) X(vkCreateComputePipelines) X(vkCreateDescriptorPool) \
    X(vkResetDescriptorPool) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCmdBindPipeline) \
    X(vkCmdBindDescriptorSets) X(vkCmdPushConstants) X(vkCmdDispatch) X(vkCmdCopyBuffer) X(vkCmdPipelineBarrier) \
    X(vkCmdFillBuffer)
// los NUCLEOS DE TRAZADO (opcionales: solo si el driver trae VK_KHR_ray_query y sus amigos)
#define RTG_RTX(X) X(vkGetBufferDeviceAddress) X(vkGetAccelerationStructureBuildSizesKHR) X(vkCreateAccelerationStructureKHR) \
    X(vkDestroyAccelerationStructureKHR) X(vkCmdBuildAccelerationStructuresKHR) X(vkGetAccelerationStructureDeviceAddressKHR)
#define RTG_DECL(n) static PFN_##n n##_ = 0;
RTG_GLOBAL(RTG_DECL) RTG_INST(RTG_DECL) RTG_DEV(RTG_DECL) RTG_RTX(RTG_DECL)
#undef RTG_DECL
static PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2_ = 0;
static PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion_ = 0;
static bool gRtx = false;   // la busqueda la hace el hardware (W3D_RTGPU_SIN_RTX=1: el BVH propio, para comparar)

static int gEstado = -1;   // -1 sin probar, 0 no hay, 1 listo
static std::string gNombre;
static VkInstance gInst = VK_NULL_HANDLE;
static VkPhysicalDevice gFis = VK_NULL_HANDLE;
static VkDevice gDev = VK_NULL_HANDLE;
static VkQueue gCola = VK_NULL_HANDLE;
static VkPhysicalDeviceMemoryProperties gMem;
static VkCommandPool gPool = VK_NULL_HANDLE;
static VkCommandBuffer gCmd = VK_NULL_HANDLE;
static VkFence gFence = VK_NULL_HANDLE;
static VkDescriptorSetLayout gSetTrazar = VK_NULL_HANDLE, gSetAtrous = VK_NULL_HANDLE;
static VkPipelineLayout gLayTrazar = VK_NULL_HANDLE, gLayAtrous = VK_NULL_HANDLE;
static VkPipeline gPipeTrazar = VK_NULL_HANDLE, gPipeAtrous = VK_NULL_HANDLE, gPipeDemod = VK_NULL_HANDLE, gPipeRemod = VK_NULL_HANDLE;
static VkDescriptorPool gDescPoolDen = VK_NULL_HANDLE;   // (los del denoiser: se rehacen en cada llamada)
static VkDescriptorPool gDescPool = VK_NULL_HANDLE;

// ---- buffers: en la GPU (los datos) o visibles desde la CPU (el ida y vuelta) ----
struct BufG { VkBuffer b; VkDeviceMemory m; VkDeviceSize tam; void* p; };
static void Soltar(BufG& b) {
    if (b.b) vkDestroyBuffer_(gDev, b.b, 0);
    if (b.m) vkFreeMemory_(gDev, b.m, 0);
    memset(&b, 0, sizeof(b));
}
static int TipoMem(uint32_t bits, VkMemoryPropertyFlags f) {
    for (uint32_t i = 0; i < gMem.memoryTypeCount; i++) if ((bits & (1u << i)) && (gMem.memoryTypes[i].propertyFlags & f) == f) return (int)i;
    return -1;
}
static bool Crear(BufG& b, VkDeviceSize tam, bool cpu) {
    if (b.b && b.tam >= tam && (b.p != 0) == cpu) return true;
    Soltar(b);
    if (tam < 16) tam = 16;
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = tam; bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    // (con los nucleos de trazado cualquier buffer puede ser entrada o lugar de las estructuras: por direccion)
    if (gRtx) bi.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    if (vkCreateBuffer_(gDev, &bi, 0, &b.b) != VK_SUCCESS) return false;
    VkMemoryRequirements r; vkGetBufferMemoryRequirements_(gDev, b.b, &r);
    const VkMemoryPropertyFlags f = cpu ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    int t = TipoMem(r.memoryTypeBits, f);
    if (t < 0 && !cpu) t = TipoMem(r.memoryTypeBits, 0);
    if (t < 0) { Soltar(b); return false; }
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = r.size; ai.memoryTypeIndex = (uint32_t)t;
    VkMemoryAllocateFlagsInfo fl = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
    fl.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    if (gRtx) ai.pNext = &fl;
    if (vkAllocateMemory_(gDev, &ai, 0, &b.m) != VK_SUCCESS) { Soltar(b); return false; }
    vkBindBufferMemory_(gDev, b.b, b.m, 0);
    if (cpu) vkMapMemory_(gDev, b.m, 0, VK_WHOLE_SIZE, 0, &b.p);
    b.tam = tam;
    return true;
}
// ---- un command buffer, enviado y esperado (cada pase es corto: la imagen va y viene) ----
static VkCommandBuffer Empezar() {
    vkResetCommandBuffer_(gCmd, 0);
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer_(gCmd, &bi);
    return gCmd;
}
static bool Terminar() {
    vkEndCommandBuffer_(gCmd);
    vkResetFences_(gDev, 1, &gFence);
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1; si.pCommandBuffers = &gCmd;
    VkResult r = vkQueueSubmit_(gCola, 1, &si, gFence);
    if (r == VK_SUCCESS) r = vkWaitForFences_(gDev, 1, &gFence, VK_TRUE, UINT64_MAX);
    if (r != VK_SUCCESS) { w3dLogfE("[rtgpu] la GPU fallo (%d): se vuelve a la CPU", (int)r); gEstado = 0; return false; }
    return true;
}
static void Barrera(VkCommandBuffer cb) {
    VkMemoryBarrier b = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier_(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &b, 0, 0, 0, 0);
}
static BufG gIda;   // (lo que sube y lo que baja pasa por aca)
static bool SubirA(BufG& dst, const void* datos, size_t tam) {
    if (!Crear(dst, tam, false) || !Crear(gIda, tam, true)) return false;
    if (tam == 0) return true;
    memcpy(gIda.p, datos, tam);
    VkCommandBuffer cb = Empezar();
    VkBufferCopy c = { 0, 0, tam };
    vkCmdCopyBuffer_(cb, gIda.b, dst.b, 1, &c);
    return Terminar();
}
static bool BajarDe(BufG& src, void* datos, size_t tam) {
    if (!Crear(gIda, tam, true)) return false;
    VkCommandBuffer cb = Empezar();
    VkBufferCopy c = { 0, 0, tam };
    vkCmdCopyBuffer_(cb, src.b, gIda.b, 1, &c);
    if (!Terminar()) return false;
    memcpy(datos, gIda.p, tam);
    return true;
}

// ============================================================================
//  ARRANCAR (una vez): Vulkan, la GPU, los dos programas
// ============================================================================
static VkPipeline Programa(const char* const* partes, int n, VkPipelineLayout lay, const char* cabeza = "", int vulkanMenor = 0) {
    std::string src = cabeza; for (int i = 0; i < n; i++) src += partes[i];
    std::vector<uint32_t> spv; std::string err;
    if (!VkCompilarGLSL(src.c_str(), 2, spv, err, vulkanMenor)) { w3dLogfE("[rtgpu] shader: %s", err.c_str()); return VK_NULL_HANDLE; }
    VkShaderModuleCreateInfo mi = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    mi.codeSize = spv.size() * 4; mi.pCode = &spv[0];
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule_(gDev, &mi, 0, &mod) != VK_SUCCESS) return VK_NULL_HANDLE;
    VkComputePipelineCreateInfo pi = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pi.stage.module = mod; pi.stage.pName = "main";
    pi.layout = lay;
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateComputePipelines_(gDev, VK_NULL_HANDLE, 1, &pi, 0, &p) != VK_SUCCESS) return VK_NULL_HANDLE;
    return p;
}
// nBuf storage buffers (0..nBuf-1) y, con conAS, la estructura de aceleracion en el binding nBuf
static bool Layout(int nBuf, uint32_t tamPC, VkDescriptorSetLayout& set, VkPipelineLayout& lay, bool conAS = false) {
    VkDescriptorSetLayoutBinding b[16]; memset(b, 0, sizeof(b));
    for (int i = 0; i < nBuf; i++) { b[i].binding = (uint32_t)i; b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; b[i].descriptorCount = 1; b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; }
    if (conAS) { b[nBuf].binding = (uint32_t)nBuf; b[nBuf].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; b[nBuf].descriptorCount = 1; b[nBuf].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; }
    VkDescriptorSetLayoutCreateInfo li = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    li.bindingCount = (uint32_t)(nBuf + (conAS ? 1 : 0)); li.pBindings = b;
    if (vkCreateDescriptorSetLayout_(gDev, &li, 0, &set) != VK_SUCCESS) return false;
    VkPushConstantRange pr = { VK_SHADER_STAGE_COMPUTE_BIT, 0, tamPC };
    VkPipelineLayoutCreateInfo pi = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pi.setLayoutCount = 1; pi.pSetLayouts = &set; pi.pushConstantRangeCount = 1; pi.pPushConstantRanges = &pr;
    return vkCreatePipelineLayout_(gDev, &pi, 0, &lay) == VK_SUCCESS;
}
static bool Arrancar() {
    if (getenv("W3D_RTGPU_NO")) return false;
    std::vector<uint32_t> prueba; std::string err;
    if (!VkCompilarGLSL("#version 450\nlayout(local_size_x=1) in;\nvoid main(){}\n", 2, prueba, err)) { w3dLogf("[rtgpu] sin compilador de shaders: %s", err.c_str()); return false; }
#ifdef _WIN32
    HMODULE lib = LoadLibraryA("vulkan-1.dll");
    PFN_vkGetInstanceProcAddr gipa = lib ? (PFN_vkGetInstanceProcAddr)GetProcAddress(lib, "vkGetInstanceProcAddr") : 0;
#else
    void* lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!lib) lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    PFN_vkGetInstanceProcAddr gipa = lib ? (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr") : 0;
#endif
    if (!gipa) { w3dLogf("[rtgpu] sin Vulkan en esta maquina"); return false; }
#define RTG_G(n) n##_ = (PFN_##n)gipa(VK_NULL_HANDLE, #n); if (!n##_) return false;
    RTG_GLOBAL(RTG_G)
#undef RTG_G
    vkEnumerateInstanceVersion_ = (PFN_vkEnumerateInstanceVersion)gipa(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    uint32_t verInst = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion_) vkEnumerateInstanceVersion_(&verInst);
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.pApplicationName = "Whisk3D RT"; app.apiVersion = verInst >= VK_API_VERSION_1_2 ? VK_API_VERSION_1_2 : VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo = &app;
    if (vkCreateInstance_(&ici, 0, &gInst) != VK_SUCCESS) return false;
#define RTG_I(n) n##_ = (PFN_##n)gipa(gInst, #n); if (!n##_) return false;
    RTG_INST(RTG_I)
#undef RTG_I
    // la GPU: la dedicada antes que la integrada (W3D_VK_GPU=n elige, como el backend)
    uint32_t nf = 0; vkEnumeratePhysicalDevices_(gInst, &nf, 0);
    if (!nf) return false;
    std::vector<VkPhysicalDevice> fis(nf); vkEnumeratePhysicalDevices_(gInst, &nf, &fis[0]);
    const char* pide = getenv("W3D_VK_GPU");
    int mejor = -1, puntos = -1; uint32_t fam = 0;
    for (uint32_t i = 0; i < nf; i++) {
        VkPhysicalDeviceProperties pr; vkGetPhysicalDeviceProperties_(fis[i], &pr);
        uint32_t nq = 0; vkGetPhysicalDeviceQueueFamilyProperties_(fis[i], &nq, 0);
        std::vector<VkQueueFamilyProperties> q(nq ? nq : 1); vkGetPhysicalDeviceQueueFamilyProperties_(fis[i], &nq, &q[0]);
        for (uint32_t f = 0; f < nq; f++) {
            if (!(q[f].queueFlags & VK_QUEUE_COMPUTE_BIT)) continue;
            int p = pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 :
                    pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? 0 : 1;   // (una "GPU" por software no vale la pena)
            if (pide && atoi(pide) == (int)i) p = 100;
            if (p > puntos) { puntos = p; mejor = (int)i; fam = f; }
            break;
        }
    }
    if (mejor < 0 || puntos <= 0) { w3dLogf("[rtgpu] ninguna GPU con computo"); return false; }
    gFis = fis[mejor];
    { VkPhysicalDeviceProperties pr; vkGetPhysicalDeviceProperties_(gFis, &pr); gNombre = pr.deviceName; }
    vkGetPhysicalDeviceMemoryProperties_(gFis, &gMem);
    // LOS NUCLEOS DE TRAZADO: Vulkan 1.2 + VK_KHR_acceleration_structure + VK_KHR_ray_query (el driver de 2021 en
    // adelante en NVIDIA RTX, AMD RDNA2+, Intel Arc). Sin eso, el BVH propio en el compute shader (cualquier GPU)
    VkPhysicalDeviceBufferDeviceAddressFeatures fBda = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR fAs = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayQueryFeaturesKHR fRq = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    {
        VkPhysicalDeviceProperties pr; vkGetPhysicalDeviceProperties_(gFis, &pr);
        uint32_t ne = 0; vkEnumerateDeviceExtensionProperties_(gFis, 0, &ne, 0);
        std::vector<VkExtensionProperties> le(ne ? ne : 1); vkEnumerateDeviceExtensionProperties_(gFis, 0, &ne, &le[0]); le.resize(ne);
        int hay = 0;
        for (size_t i = 0; i < le.size(); i++)
            if (!strcmp(le[i].extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) || !strcmp(le[i].extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME) ||
                !strcmp(le[i].extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)) hay++;
        vkGetPhysicalDeviceFeatures2_ = (PFN_vkGetPhysicalDeviceFeatures2)gipa(gInst, "vkGetPhysicalDeviceFeatures2");
        if (hay == 3 && pr.apiVersion >= VK_API_VERSION_1_2 && app.apiVersion >= VK_API_VERSION_1_2 && vkGetPhysicalDeviceFeatures2_ && !getenv("W3D_RTGPU_SIN_RTX")) {
            fBda.pNext = &fAs; fAs.pNext = &fRq;
            VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 }; f2.pNext = &fBda;
            vkGetPhysicalDeviceFeatures2_(gFis, &f2);
            gRtx = fBda.bufferDeviceAddress && fAs.accelerationStructure && fRq.rayQuery;
        }
        if (!gRtx) w3dLogf("[rtgpu] sin nucleos de trazado (VK_KHR_ray_query): el BVH propio en el compute shader");
    }
    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qi.queueFamilyIndex = fam; qi.queueCount = 1; qi.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qi;
    const char* extRtx[3] = { VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME };
    if (gRtx) {
        // (solo lo que se usa)
        VkPhysicalDeviceAccelerationStructureFeaturesKHR as2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        as2.accelerationStructure = VK_TRUE; fAs = as2;
        VkPhysicalDeviceRayQueryFeaturesKHR rq2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
        rq2.rayQuery = VK_TRUE; fRq = rq2;
        VkPhysicalDeviceBufferDeviceAddressFeatures bda2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
        bda2.bufferDeviceAddress = VK_TRUE; fBda = bda2;
        fBda.pNext = &fAs; fAs.pNext = &fRq; fRq.pNext = 0;
        dci.pNext = &fBda;
        dci.enabledExtensionCount = 3; dci.ppEnabledExtensionNames = extRtx;
    }
    if (vkCreateDevice_(gFis, &dci, 0, &gDev) != VK_SUCCESS) {
        if (!gRtx) return false;
        gRtx = false; dci.pNext = 0; dci.enabledExtensionCount = 0; dci.ppEnabledExtensionNames = 0;   // (sin los nucleos)
        if (vkCreateDevice_(gFis, &dci, 0, &gDev) != VK_SUCCESS) return false;
    }
#define RTG_D(n) n##_ = (PFN_##n)vkGetDeviceProcAddr_(gDev, #n); if (!n##_) return false;
    RTG_DEV(RTG_D)
#undef RTG_D
    if (gRtx) {
#define RTG_R(n) n##_ = (PFN_##n)vkGetDeviceProcAddr_(gDev, #n); if (!n##_) gRtx = false;
        RTG_RTX(RTG_R)
#undef RTG_R
    }
    vkGetDeviceQueue_(gDev, fam, 0, &gCola);
    VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pci.queueFamilyIndex = fam;
    if (vkCreateCommandPool_(gDev, &pci, 0, &gPool) != VK_SUCCESS) return false;
    VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cai.commandPool = gPool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers_(gDev, &cai, &gCmd) != VK_SUCCESS) return false;
    VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    if (vkCreateFence_(gDev, &fci, 0, &gFence) != VK_SUCCESS) return false;
    if (!Layout(13, 128, gSetTrazar, gLayTrazar, gRtx) || !Layout(4, 32, gSetAtrous, gLayAtrous)) return false;
    { const char* p[2] = { kRTGpuComun, kRTGpuTrazar };
      if (gRtx) {
          gPipeTrazar = Programa(p, 2, gLayTrazar, "#version 460\n#extension GL_EXT_ray_query : require\n#define W3D_RTX 1\n", 2);
          if (!gPipeTrazar) { gRtx = false; w3dLogfW("[rtgpu] el programa con los nucleos de trazado no compilo: el BVH propio"); }
      }
      if (!gPipeTrazar) gPipeTrazar = Programa(p, 2, gLayTrazar, "#version 450\n"); }
    { const char* p[1] = { kRTGpuAtrous }; gPipeAtrous = Programa(p, 1, gLayAtrous); }
    { const char* p[1] = { kRTGpuDemod }; gPipeDemod = Programa(p, 1, gLayAtrous); }
    { const char* p[1] = { kRTGpuRemod }; gPipeRemod = Programa(p, 1, gLayAtrous); }
    if (!gPipeTrazar || !gPipeAtrous || !gPipeDemod || !gPipeRemod) return false;
    VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 }, { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 8 } };
    VkDescriptorPoolCreateInfo dpi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpi.maxSets = 8; dpi.poolSizeCount = gRtx ? 2 : 1; dpi.pPoolSizes = ps;
    if (vkCreateDescriptorPool_(gDev, &dpi, 0, &gDescPool) != VK_SUCCESS) return false;
    { VkDescriptorPoolSize pd = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 };
      VkDescriptorPoolCreateInfo di = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
      di.maxSets = 8; di.poolSizeCount = 1; di.pPoolSizes = &pd;
      if (vkCreateDescriptorPool_(gDev, &di, 0, &gDescPoolDen) != VK_SUCCESS) return false; }
    w3dLogf("[rtgpu] trazado de rayos en la GPU: %s%s", gNombre.c_str(), gRtx ? " (con los nucleos de trazado)" : "");
    return true;
}
bool RTGpuDisponible() {
    if (gEstado < 0) gEstado = Arrancar() ? 1 : 0;
    return gEstado == 1;
}
const char* RTGpuNombre() { return gNombre.c_str(); }
bool RTGpuConNucleosRT() { return RTGpuDisponible() && gRtx; }

// ============================================================================
//  la escena y la imagen
// ============================================================================
enum { BVerts, BTris, BMats, BLuces, BNodos, BOrden, BTexInfo, BTexels, BAcum, BImg, BAlb, BNrm, BRayos, BCant };
static BufG gB[BCant];
static int gW = 0, gH = 0, gNLuces = 0, gNNodos = 0;
static bool gTransp = false;
static float gBg[3] = { 0, 0, 0 };
static VkDescriptorSet gSet = VK_NULL_HANDLE;
template <class T> static bool SubirVec(BufG& b, const std::vector<T>& v) { return SubirA(b, v.empty() ? (const void*)&gBg : (const void*)&v[0], v.size() * sizeof(T)); }
// ---- LAS ESTRUCTURAS DE ACELERACION (con los nucleos de trazado): la escena entera es UN BLAS (los triangulos en el
//      mismo orden que tris: el indice de primitiva es el del triangulo) y un TLAS con una sola instancia ----
static BufG gIdx, gBlasBuf, gTlasBuf, gScratch, gInstancias;
static VkAccelerationStructureKHR gBlas = VK_NULL_HANDLE, gTlas = VK_NULL_HANDLE;
static VkDeviceAddress Direccion(const BufG& b) {
    VkBufferDeviceAddressInfo i = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO }; i.buffer = b.b;
    return vkGetBufferDeviceAddress_(gDev, &i);
}
static bool ConstruirAS(VkAccelerationStructureTypeKHR tipo, const VkAccelerationStructureGeometryKHR& geo, uint32_t prims,
                        BufG& guardar, VkAccelerationStructureKHR& as) {
    VkAccelerationStructureBuildGeometryInfoKHR bi = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    bi.type = tipo; bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR; bi.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    bi.geometryCount = 1; bi.pGeometries = &geo;
    VkAccelerationStructureBuildSizesInfoKHR tam = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR_(gDev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, &prims, &tam);
    if (as) { vkDestroyAccelerationStructureKHR_(gDev, as, 0); as = VK_NULL_HANDLE; }
    if (!Crear(guardar, tam.accelerationStructureSize, false) || !Crear(gScratch, tam.buildScratchSize + 256, false)) return false;
    VkAccelerationStructureCreateInfoKHR ci = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    ci.buffer = guardar.b; ci.size = tam.accelerationStructureSize; ci.type = tipo;
    if (vkCreateAccelerationStructureKHR_(gDev, &ci, 0, &as) != VK_SUCCESS) return false;
    bi.dstAccelerationStructure = as;
    bi.scratchData.deviceAddress = (Direccion(gScratch) + 255) & ~(VkDeviceAddress)255;
    VkAccelerationStructureBuildRangeInfoKHR rango = { prims, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* pr = &rango;
    VkCommandBuffer cb = Empezar();
    vkCmdBuildAccelerationStructuresKHR_(cb, 1, &bi, &pr);
    return Terminar();
}
static uint32_t gAsVerts = 0, gAsTris = 0;
static bool ConstruirBlasTlas();
static bool ArmarAS(const RTGpuEscena& e) {
    std::vector<uint32_t> idx;
    const size_t nt = e.tris.size() / 4;
    for (size_t t = 0; t < nt; t++) for (int k = 0; k < 3; k++) idx.push_back((uint32_t)e.tris[t * 4 + k]);
    if (idx.empty()) { idx.push_back(0); idx.push_back(0); idx.push_back(0); }   // (sin triangulos: uno vacio, nunca pega)
    if (!SubirVec(gIdx, idx)) return false;
    const size_t nv = e.verts.size() / 12;
    gAsVerts = nv > 0 ? (uint32_t)nv : 1; gAsTris = (uint32_t)(idx.size() / 3);
    return ConstruirBlasTlas();
}
// el BLAS (de gB[BVerts] y gIdx, ya subidos) y el TLAS con su instancia
static bool ConstruirBlasTlas() {
    VkAccelerationStructureGeometryKHR g = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    // (opaca, salvo cuando el rayo pide verlas todas: las sombras con transparencias; cada triangulo UNA vez)
    g.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR; g.flags = VK_GEOMETRY_OPAQUE_BIT_KHR | VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
    g.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    g.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    g.geometry.triangles.vertexData.deviceAddress = Direccion(gB[BVerts]);
    g.geometry.triangles.vertexStride = 48;
    g.geometry.triangles.maxVertex = gAsVerts - 1;
    g.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    g.geometry.triangles.indexData.deviceAddress = Direccion(gIdx);
    if (!ConstruirAS(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, g, gAsTris, gBlasBuf, gBlas)) return false;
    VkAccelerationStructureDeviceAddressInfoKHR ai = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    ai.accelerationStructure = gBlas;
    VkAccelerationStructureInstanceKHR inst; memset(&inst, 0, sizeof(inst));
    inst.transform.matrix[0][0] = inst.transform.matrix[1][1] = inst.transform.matrix[2][2] = 1.0f;
    inst.mask = 0xFF; inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    inst.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR_(gDev, &ai);
    if (!SubirA(gInstancias, &inst, sizeof(inst))) return false;
    VkAccelerationStructureGeometryKHR gi = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    gi.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    gi.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    gi.geometry.instances.data.deviceAddress = Direccion(gInstancias);
    return ConstruirAS(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, gi, 1, gTlasBuf, gTlas);
}
bool RTGpuCargarEscena(const RTGpuEscena& e) {
    if (!RTGpuDisponible()) return false;
    if (!SubirVec(gB[BVerts], e.verts) || !SubirVec(gB[BTris], e.tris) || !SubirVec(gB[BMats], e.mats) || !SubirVec(gB[BLuces], e.luces) ||
        !SubirVec(gB[BNodos], e.nodos) || !SubirVec(gB[BOrden], e.orden) || !SubirVec(gB[BTexInfo], e.texInfo) || !SubirVec(gB[BTexels], e.texels)) return false;
    if (gRtx && !ArmarAS(e)) { w3dLogfE("[rtgpu] no se pudieron armar las estructuras de aceleracion"); return false; }
    gNLuces = (int)(e.luces.size() / 20); gNNodos = (int)(e.nodos.size() / 12);
    gTransp = e.hayTransparentes;
    gBg[0] = e.bg[0]; gBg[1] = e.bg[1]; gBg[2] = e.bg[2];
    gSet = VK_NULL_HANDLE;   // (los buffers pudieron cambiar)
    return true;
}
bool RTGpuActualizarLuces(const std::vector<float>& luces) {
    if (gEstado != 1) return false;
    const VkBuffer antes = gB[BLuces].b;
    if (!SubirVec(gB[BLuces], luces)) return false;
    gNLuces = (int)(luces.size() / 20);
    if (gB[BLuces].b != antes) gSet = VK_NULL_HANDLE;   // (crecio: es otro buffer)
    return true;
}
bool RTGpuActualizarGeometria(const std::vector<float>& verts, const std::vector<float>& nodos) {
    if (gEstado != 1) return false;
    // (mismo tamanio: los buffers son los mismos y el set del trazado sigue valido)
    if (!SubirVec(gB[BVerts], verts) || !SubirVec(gB[BNodos], nodos)) return false;
    if (gRtx) { if (!ConstruirBlasTlas()) return false; gSet = VK_NULL_HANDLE; }   // (el TLAS es otro)
    return true;
}
bool RTGpuImagen(int w, int h) {
    if (!RTGpuDisponible() || w <= 0 || h <= 0) return false;
    const VkDeviceSize n = (VkDeviceSize)w * h;
    if (!Crear(gB[BAcum], n * 16, false) || !Crear(gB[BImg], n * 4, false) || !Crear(gB[BAlb], n * 16, false) || !Crear(gB[BNrm], n * 16, false)) return false;
    { const uint32_t cero[4] = { 0, 0, 0, 0 }; if (!SubirA(gB[BRayos], cero, sizeof(cero))) return false; }
    gW = w; gH = h;
    gSet = VK_NULL_HANDLE;
    return true;
}
static bool SetTrazar() {
    if (gSet) return true;
    vkResetDescriptorPool_(gDev, gDescPool, 0);
    VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    ai.descriptorPool = gDescPool; ai.descriptorSetCount = 1; ai.pSetLayouts = &gSetTrazar;
    if (vkAllocateDescriptorSets_(gDev, &ai, &gSet) != VK_SUCCESS) return false;
    VkDescriptorBufferInfo bi[BCant]; VkWriteDescriptorSet w[BCant]; memset(w, 0, sizeof(w));
    for (int i = 0; i < BCant; i++) {
        bi[i].buffer = gB[i].b; bi[i].offset = 0; bi[i].range = VK_WHOLE_SIZE;
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[i].dstSet = gSet; w[i].dstBinding = (uint32_t)i;
        w[i].descriptorCount = 1; w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; w[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets_(gDev, BCant, w, 0, 0);
    if (gRtx) {
        VkWriteDescriptorSetAccelerationStructureKHR wa = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
        wa.accelerationStructureCount = 1; wa.pAccelerationStructures = &gTlas;
        VkWriteDescriptorSet wt = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        wt.pNext = &wa; wt.dstSet = gSet; wt.dstBinding = BCant; wt.descriptorCount = 1; wt.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        vkUpdateDescriptorSets_(gDev, 1, &wt, 0, 0);
    }
    return true;
}
struct PCTrazar { float camPos[4], camDer[4], camArr[4], camAdel[4]; int dims[4], info[4]; float bg[4], lente[4]; };
bool RTGpuPase(const RTGpuCamara& c, int paso, int samples, bool aov, int capas) {
    if (gEstado != 1 || !gW || !SetTrazar()) return false;
    PCTrazar pc;
    for (int k = 0; k < 3; k++) { pc.camPos[k] = c.pos[k]; pc.camDer[k] = c.der[k]; pc.camArr[k] = c.arr[k]; pc.camAdel[k] = c.adel[k]; pc.bg[k] = gBg[k]; }
    pc.camPos[3] = c.sx; pc.camDer[3] = c.sy; pc.camArr[3] = c.zoom; pc.camAdel[3] = c.orto ? 1.0f : 0.0f; pc.bg[3] = c.lente;
    pc.lente[0] = c.panX; pc.lente[1] = c.panY; pc.lente[2] = (float)(c.semilla & 0xFFFFFu); pc.lente[3] = c.foco;
    pc.dims[0] = gW; pc.dims[1] = gH; pc.dims[2] = paso; pc.dims[3] = samples;
    pc.info[0] = gNLuces; pc.info[1] = gNNodos; pc.info[2] = gTransp ? (capas < 1 ? 1 : capas) : 0; pc.info[3] = aov ? 1 : 0;
    VkCommandBuffer cb = Empezar();
    vkCmdBindPipeline_(cb, VK_PIPELINE_BIND_POINT_COMPUTE, gPipeTrazar);
    vkCmdBindDescriptorSets_(cb, VK_PIPELINE_BIND_POINT_COMPUTE, gLayTrazar, 0, 1, &gSet, 0, 0);
    vkCmdPushConstants_(cb, gLayTrazar, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch_(cb, (uint32_t)(gW + 7) / 8, (uint32_t)(gH + 7) / 8, 1);
    return Terminar();
}
// los rayos trazados desde la ultima vez (y el contador vuelve a cero)
unsigned long long RTGpuRayos() {
    if (gEstado != 1 || !gB[BRayos].b) return 0;
    uint32_t v[4] = { 0, 0, 0, 0 };
    if (!BajarDe(gB[BRayos], v, sizeof(v))) return 0;
    const uint32_t cero[4] = { 0, 0, 0, 0 };
    SubirA(gB[BRayos], cero, sizeof(cero));
    return v[0];
}
bool RTGpuLeer(unsigned char* rgba, float* colorMedio, float* alb, float* nrm, int pasos) {
    if (gEstado != 1 || !gW) return false;
    const size_t n = (size_t)gW * gH;
    if (rgba && !BajarDe(gB[BImg], rgba, n * 4)) return false;
    std::vector<float> f4;
    const float inv = pasos > 0 ? 1.0f / (float)pasos : 1.0f;
    float* dst[3] = { colorMedio, alb, nrm }; const int bufs[3] = { BAcum, BAlb, BNrm };
    for (int k = 0; k < 3; k++) {
        if (!dst[k]) continue;
        f4.resize(n * 4);
        if (!BajarDe(gB[bufs[k]], &f4[0], n * 16)) return false;
        const float s = k == 0 ? inv : 1.0f;
        for (size_t i = 0; i < n; i++) for (int c = 0; c < 3; c++) {
            float v = f4[i * 4 + c] * s;
            if (k == 0) v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            dst[k][i * 3 + c] = v;
        }
    }
    return true;
}

// ============================================================================
//  EL DENOISER A-TROUS: la luz (color / albedo) filtrada 5 veces con huecos crecientes, despues por el albedo de nuevo
// ============================================================================
static BufG gAt[4];   // ping, pong, albedo, normal
struct PCAtrous { int dims[4]; float sigma[4]; };
// un set del denoiser con esos 4 buffers (del pool propio)
static VkDescriptorSet SetDen(BufG* b0, BufG* b1, BufG* b2, BufG* b3) {
    VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    ai.descriptorPool = gDescPoolDen; ai.descriptorSetCount = 1; ai.pSetLayouts = &gSetAtrous;
    VkDescriptorSet s = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets_(gDev, &ai, &s) != VK_SUCCESS) return VK_NULL_HANDLE;
    BufG* bs[4] = { b0, b1, b2, b3 };
    VkDescriptorBufferInfo bi[4]; VkWriteDescriptorSet wr[4]; memset(wr, 0, sizeof(wr));
    for (int i = 0; i < 4; i++) {
        bi[i].buffer = bs[i]->b; bi[i].offset = 0; bi[i].range = VK_WHOLE_SIZE;
        wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; wr[i].dstSet = s; wr[i].dstBinding = (uint32_t)i;
        wr[i].descriptorCount = 1; wr[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; wr[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets_(gDev, 4, wr, 0, 0);
    return s;
}
// las 5 pasadas del filtro, de ida y vuelta entre gAt[0] y gAt[1] (empieza leyendo gAt[0]; termina en gAt[1])
static void Pasadas(VkCommandBuffer cb, VkDescriptorSet* sets, int w, int h) {
    vkCmdBindPipeline_(cb, VK_PIPELINE_BIND_POINT_COMPUTE, gPipeAtrous);
    for (int it = 0; it < 5; it++) {
        vkCmdBindDescriptorSets_(cb, VK_PIPELINE_BIND_POINT_COMPUTE, gLayAtrous, 0, 1, &sets[it & 1], 0, 0);
        PCAtrous pc; pc.dims[0] = w; pc.dims[1] = h; pc.dims[2] = 1 << it; pc.dims[3] = 0;
        pc.sigma[0] = 0.8f / (float)(1 << it) + 0.05f;   // el brillo: tolerante al principio (el ruido), estricto despues
        pc.sigma[1] = 64.0f; pc.sigma[2] = 0.02f; pc.sigma[3] = 0.0f;
        vkCmdPushConstants_(cb, gLayAtrous, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
        vkCmdDispatch_(cb, (uint32_t)(w + 7) / 8, (uint32_t)(h + 7) / 8, 1);
        Barrera(cb);
    }
}
bool RTGpuDenoisearLoAcumulado(int pasos) {
    if (gEstado != 1 || !gW) return false;
    const VkDeviceSize n = (VkDeviceSize)gW * gH;
    if (!Crear(gAt[0], n * 16, false) || !Crear(gAt[1], n * 16, false)) return false;
    vkResetDescriptorPool_(gDev, gDescPoolDen, 0);
    VkDescriptorSet sDemod = SetDen(&gB[BAcum], &gAt[0], &gB[BAlb], &gB[BNrm]);
    VkDescriptorSet sIda[2] = { SetDen(&gAt[0], &gAt[1], &gB[BAlb], &gB[BNrm]), SetDen(&gAt[1], &gAt[0], &gB[BAlb], &gB[BNrm]) };
    VkDescriptorSet sRemod = SetDen(&gAt[1], &gB[BImg], &gB[BAlb], &gB[BNrm]);
    if (!sDemod || !sIda[0] || !sIda[1] || !sRemod) return false;
    PCAtrous pc; memset(&pc, 0, sizeof(pc)); pc.dims[0] = gW; pc.dims[1] = gH; pc.dims[2] = pasos;
    VkCommandBuffer cb = Empezar();
    vkCmdBindPipeline_(cb, VK_PIPELINE_BIND_POINT_COMPUTE, gPipeDemod);
    vkCmdBindDescriptorSets_(cb, VK_PIPELINE_BIND_POINT_COMPUTE, gLayAtrous, 0, 1, &sDemod, 0, 0);
    vkCmdPushConstants_(cb, gLayAtrous, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch_(cb, (uint32_t)(gW + 7) / 8, (uint32_t)(gH + 7) / 8, 1);
    Barrera(cb);
    Pasadas(cb, sIda, gW, gH);
    vkCmdBindPipeline_(cb, VK_PIPELINE_BIND_POINT_COMPUTE, gPipeRemod);
    vkCmdBindDescriptorSets_(cb, VK_PIPELINE_BIND_POINT_COMPUTE, gLayAtrous, 0, 1, &sRemod, 0, 0);
    vkCmdPushConstants_(cb, gLayAtrous, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch_(cb, (uint32_t)(gW + 7) / 8, (uint32_t)(gH + 7) / 8, 1);
    return Terminar();
}
bool RTGpuDenoisear(const float* color, const float* alb, const float* nrm, int w, int h, float* salida, std::string* err) {
    if (!RTGpuDisponible()) { if (err) *err = "sin GPU"; return false; }
    const size_t n = (size_t)w * h;
    std::vector<float> luz(n * 4, 0.0f), a4(n * 4, 0.0f), n4(n * 4, 0.0f);
    for (size_t i = 0; i < n; i++) for (int c = 0; c < 3; c++) {
        const float a = alb ? alb[i * 3 + c] : 1.0f;
        a4[i * 4 + c] = a;
        luz[i * 4 + c] = color[i * 3 + c] / (a > 0.02f ? a : 0.02f);   // (la luz: sin la textura)
        n4[i * 4 + c] = nrm ? nrm[i * 3 + c] : 0.0f;
    }
    if (!SubirA(gAt[0], &luz[0], n * 16) || !Crear(gAt[1], n * 16, false) || !SubirA(gAt[2], &a4[0], n * 16) || !SubirA(gAt[3], &n4[0], n * 16)) {
        if (err) *err = "sin memoria en la GPU"; return false;
    }
    vkResetDescriptorPool_(gDev, gDescPoolDen, 0);
    VkDescriptorSet sets[2] = { SetDen(&gAt[0], &gAt[1], &gAt[2], &gAt[3]), SetDen(&gAt[1], &gAt[0], &gAt[2], &gAt[3]) };
    if (!sets[0] || !sets[1]) { if (err) *err = "descriptores"; return false; }
    VkCommandBuffer cb = Empezar();
    Pasadas(cb, sets, w, h);
    if (!Terminar()) { if (err) *err = "la GPU fallo"; return false; }
    // (5 pasadas: la ultima escribio en el 1)
    std::vector<float> res(n * 4);
    if (!BajarDe(gAt[1], &res[0], n * 16)) { if (err) *err = "lectura"; return false; }
    for (size_t i = 0; i < n; i++) for (int c = 0; c < 3; c++) {
        const float a = alb ? alb[i * 3 + c] : 1.0f;
        salida[i * 3 + c] = res[i * 4 + c] * (a > 0.02f ? a : 0.02f);
    }
    return true;
}

#else   // sin Vulkan (Symbian, la web, Android por ahora): no hay GPU para trazar
bool RTGpuDisponible() { return false; }
const char* RTGpuNombre() { return ""; }
bool RTGpuConNucleosRT() { return false; }
bool RTGpuCargarEscena(const RTGpuEscena&) { return false; }
bool RTGpuImagen(int, int) { return false; }
bool RTGpuActualizarGeometria(const std::vector<float>&, const std::vector<float>&) { return false; }
bool RTGpuActualizarLuces(const std::vector<float>&) { return false; }
bool RTGpuPase(const RTGpuCamara&, int, int, bool, int) { return false; }
bool RTGpuLeer(unsigned char*, float*, float*, float*, int) { return false; }
bool RTGpuDenoisear(const float*, const float*, const float*, int, int, float*, std::string* err) { if (err) *err = "sin GPU"; return false; }
bool RTGpuDenoisearLoAcumulado(int) { return false; }
unsigned long long RTGpuRayos() { return 0; }
#endif
