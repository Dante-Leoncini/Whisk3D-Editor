#include <SDL3/SDL.h>
//#include <SDL3/SDL_image.h>

//para las texturas
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <vulkan/vulkan.h>

//esto es solo para linux
#include <cmath>
#include <cfloat>
#include <map>
#include <algorithm>
#include <cstring>

//utilidades de Whisk3D de uso general. mas que nada para manejo de punteros y memoria
#include "../Shared/Whisk3Dutils.h"

#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

//variables de SDL/Linux
SDL_Window* window = nullptr;  // definición real

//NUEVO! es para poder usar OpenGl o Vulkan
SDL_GLContext glContext = nullptr;

// Para Vulkan
VkInstance vkInstance = VK_NULL_HANDLE;
VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
bool usingVulkan = false;

int winW = 800; 
int winH = 600;

//Whisk3D imports
#include "../Shared/tablero.h"
#include "../Shared/Floor.h"
#include "../Shared/GeometriaUI.h"
#include "../Shared/Primitivas.h"

#include "../Shared/clases.h"
#include "../Shared/variables.h"
#include "../Shared/colores.h"
#include "../Shared/OpcionesRender.h"

#include "../Shared/Animation.h"
#include "../Shared/ObjectMode.h"
#include "../Shared/render.h"
#include "../Shared/import_obj.h"
#include "../Shared/import_vertex_animation.h"
#include "../Shared/lectura-escritura.h"
#include "../Shared/ViewPorts/ViewPorts.h"
#include "../Shared/funciones.h"
#include "../Shared/controles.h"
#include "../Shared/constructor.h"

struct Config {
    bool fullscreen = false;
    bool enableAntialiasing = false;
    int width = 800;
    int height = 600;
	int displayIndex = 0; // monitor 1
	std::string SkinName = "Whisk3D"; // monitor 1
    std::string graphicsAPI = "opengl";
};
Config cfg;

// Función simple para leer el ini
Config loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "No se pudo abrir " << filename << ", usando valores por defecto.\n";
        return cfg;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key, eq, value;
        if (iss >> key >> eq >> value && eq == "=") {
            if (key == "fullscreen") cfg.fullscreen = (value == "true");
            else if (key == "enableAntialiasing") cfg.enableAntialiasing = (value == "true");
            else if (key == "width") cfg.width = std::stoi(value);
            else if (key == "height") cfg.height = std::stoi(value);
            else if (key == "displayIndex") cfg.displayIndex = std::stoi(value);
            else if (key == "SkinName") cfg.SkinName = value;
            else if (key == "graphicsAPI") cfg.graphicsAPI = value;            
        }
    }

    return cfg;
}

// Función para cargar colores desde archivo
bool loadColors(const std::string& filename) {
    // Map de strings a enums
    static std::unordered_map<std::string, ColorID> colorMap = {
        {"background", background},
        {"blanco", blanco},
        {"accent", accent},
        {"accentDark", accentDark},
        {"negro", negro},
        {"gris", gris},
        {"naranjaFace", naranjaFace},
        {"headerColor", headerColor},
        {"negroTransparente", negroTransparente},
        {"grisUI", grisUI},
        {"LineaPiso", LineaPiso},
        {"LineaPisoRoja", LineaPisoRoja},
        {"LineaPisoVerde", LineaPisoVerde},
        {"ColorTransformX", ColorTransformX},
        {"ColorTransformY", ColorTransformY},
        {"ColorTransformZ", ColorTransformZ}
    };

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "No se pudo abrir " << filename << ", usando colores por defecto.\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string name;
        float r, g, b, a;
        if (!(iss >> name >> r >> g >> b >> a)) {
            std::cerr << "Formato incorrecto en colors.skin: " << line << "\n";
            continue;
        }

        auto it = colorMap.find(name);
        if (it != colorMap.end()) {
            int idx = it->second;
            ListaColores[idx][0] = r;
            ListaColores[idx][1] = g;
            ListaColores[idx][2] = b;
            ListaColores[idx][3] = a;
            
            ListaColoresUbyte[idx][0] = (GLubyte)(r*255);
            ListaColoresUbyte[idx][1] = (GLubyte)(g*255);
            ListaColoresUbyte[idx][2] = (GLubyte)(b*255);
            ListaColoresUbyte[idx][3] = (GLubyte)(a*255);
        } else {
            std::cerr << "Color desconocido en colors.skin: " << name << "\n";
        }
    }

    return true;
}

void Minimize(SDL_Window* window) {
    SDL_MinimizeWindow(window);
}

void Maximize(SDL_Window* window) {
    SDL_MaximizeWindow(window);
}

void Restore(SDL_Window* window) {
    SDL_RestoreWindow(window);
}

SDL_HitTestResult HitTestCallback(SDL_Window *win, const SDL_Point *area, void *data) {
    // Si el mouse está en el "header" de 30px de alto
    if (area->y < 30) {
        return SDL_HITTEST_DRAGGABLE;
    }
    return SDL_HITTEST_NORMAL;
}

//Necesiario para Vulkan
VkInstance instance;
VkSurfaceKHR surface;
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
VkDevice device;
VkQueue graphicsQueue;
VkQueue presentQueue;
VkSwapchainKHR swapchain;
std::vector<VkImage> swapchainImages;
VkFormat swapchainImageFormat;
VkExtent2D swapchainExtent;
VkApplicationInfo appInfo = {};
VkPresentInfoKHR presentInfo = {};

int main(int argc, char* argv[]) {
	// Inicializar SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return -1;
    }

    // mover working dir al path donde está el ejecutable
    std::filesystem::path exePath = std::filesystem::absolute(argv[0]).parent_path();
    std::filesystem::current_path(exePath);

	Config cfg = loadConfig("./config.ini");

	// ---- CONFIGURAR ANTIALIASING ----
	// Pedir un framebuffer con multisampling
    if (cfg.enableAntialiasing) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // 4x MSAA
    } else {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }

	SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (cfg.graphicsAPI == "opengl") {
        std::cerr << "Renderer: OpenGl\n";
        windowFlags |= SDL_WINDOW_OPENGL;
        usingVulkan = false;
    } else if (cfg.graphicsAPI == "vulkan") {
        std::cerr << "Renderer: Vulkan\n";
        windowFlags |= SDL_WINDOW_VULKAN;
        usingVulkan = true;
    } else {
        std::cerr << "Renderer desconocido, usando OpenGL por defecto\n";
        windowFlags |= SDL_WINDOW_OPENGL;
        usingVulkan = false;
    }

    // Crear la ventana
    window = SDL_CreateWindow("Whisk 3D", winW, winH, windowFlags);

	if (!window) {
		std::cerr << "Error SDL_CreateWindow: " << SDL_GetError() << std::endl;
		return -1;
	}

    // Forzar a maximizar al arrancar
    SDL_MaximizeWindow(window);

    if (!usingVulkan) {
        // OpenGL
        glContext = SDL_GL_CreateContext(window);
        if (!glContext) {
            std::cerr << "Error SDL_GL_CreateContext: " << SDL_GetError() << std::endl;
            return -1;
        }
        SDL_GL_SetSwapInterval(1); // VSync
    } 
    else {
        // --- Vulkan ---
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Whisk3D";
        appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.pEngineName = "Whisk3D Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            std::cerr << "Error creando instancia Vulkan\n";
            return -1;
        }

        VkSurfaceKHR surface;
        #ifdef SDL_VIDEO_VULKAN
        if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
            std::cerr << "Error creando surface Vulkan\n";
            return -1;
        }
        #else
        std::cerr << "SDL3 compilado sin soporte Vulkan\n";
        return -1;
        #endif

        // Enumerar GPUs
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        physicalDevice = devices[0]; // por simplicidad, elegir la primera

        // Crear device y obtener queues
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = 0; // Debe ser la queue correcta que soporte graphics + present
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
        vkGetDeviceQueue(device, 0, 0, &graphicsQueue);
        vkGetDeviceQueue(device, 0, 0, &presentQueue);
        
        VkSwapchainCreateInfoKHR swapchainInfo = {};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = 2;
        swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchainInfo.imageExtent = { static_cast<uint32_t>(cfg.width), static_cast<uint32_t>(cfg.height) };
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainInfo.clipped = VK_TRUE;
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

        vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);

        // Obtener imágenes del swapchain
        uint32_t imageCount;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 0;
        presentInfo.pWaitSemaphores = nullptr;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        uint32_t imageIndex = 0; // índice de la imagen que querés presentar
        presentInfo.pImageIndices = &imageIndex;
        vkQueuePresentKHR(presentQueue, &presentInfo);
    }

    //carga el Skin
    std::string skinPath = "../Shared/UI/Skins/" + cfg.SkinName + "/skin.ini";
    loadColors(skinPath);

    //constructor symbian, linux y windows
    ConstructUniversal();

	// Cargar texturas
	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/" + cfg.SkinName + "/font.png").c_str(), Textures[0].iID)) {
        std::cerr << "Error cargando font.png" << std::endl;
        return -1;
    }	
    
	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/"+cfg.SkinName+"/origen.png").c_str(), Textures[1].iID)) {
        std::cerr << "Error cargando origen.png" << std::endl;
        return -1;
    }

	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/"+cfg.SkinName+"/cursor3d.png").c_str(), Textures[2].iID)) {
        std::cerr << "Error cargando cursor3d.png" << std::endl;
        return -1;
    }

	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/"+cfg.SkinName+"/relationshipLine.png").c_str(), Textures[3].iID)) {
        std::cerr << "Error cargando relationshipLine.png" << std::endl;
        return -1;
    }

	Textures.push_back(Texture());
    if (!LoadTexture(("../Shared/UI/Skins/"+cfg.SkinName+"/lamp.png").c_str(), Textures[4].iID)) {
        std::cerr << "Error cargando lamp.png" << std::endl;
        return -1;
    }

	SDL_Event e;

	running = true;

    while (running) {
        Contadores();
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; }
            else if (e.type == SDL_EVENT_WINDOW_RESIZED ||
                    e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                OnResizeViewports(e.window.data1, e.window.data2);
            }
            else {
                InputUsuarioSDL3(e);
            }
        }

        // --- Animación ---
        Uint32 now = SDL_GetTicks();
        if (now - lastAnimTime >= millisecondsPerFrame) {
            lastAnimTime = now;

            CurrentFrame++;
            if (CurrentFrame > EndFrame) {
                CurrentFrame = StartFrame;
            }
            ReloadAnimation();
        }

        // --- Render siempre a 60 fps ---
        if (now - lastRenderTime >= 16) {  // ~60hz
            lastRenderTime = now;

            if (usingVulkan) {
                RenderVK();
                vkQueuePresentKHR(presentQueue, &presentInfo);
            } else {
                RenderViewports();
                SDL_GL_SwapWindow(window);
            }
        }

        // Si querés liberar CPU:
        SDL_Delay(8); // duerme lo mínimo
    }

    if (usingVulkan){
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
    else {
        SDL_GL_DestroyContext(glContext);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}