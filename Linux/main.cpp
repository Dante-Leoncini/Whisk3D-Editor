// Carga preliminar solo para leer la versión
#include <SDL.h>

#if SDL_MAJOR_VERSION == 2
    #include <SDL2/SDL.h>      
    #include <SDL2/SDL_image.h>
    #include "../Shared/sdl_key_compat.h"
#elif SDL_MAJOR_VERSION == 3
    #include <SDL3/SDL.h>      
    //para las texturas
    #define STB_IMAGE_IMPLEMENTATION
    #include "stb_image.h"
#endif

#include <GL/gl.h>
#include <GL/glu.h>

//esto es solo para linux
#include <functional>
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
SDL_GLContext glContext = nullptr;

int winW = 640; 
int winH = 480;

//Whisk3D imports
#include "../Shared/tablero.h"
#include "../Shared/Floor.h"
#include "../Shared/UI/GeometriaUI.h"

#include "../Shared/clases.h"
#include "../Shared/variables.h"
#include "../Shared/UI/colores.h"
#include "../Shared/OpcionesRender.h"

#include "../Shared/Animation.h"
#include "../Shared/Objects/ObjectMode.h"
#include "../Shared/render.h"
#include "../Shared/importers/import_obj.h"
//#include "../Shared/import_vertex_animation.h"
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

SDL_HitTestResult HitTestCallback(SDL_Window *win, const SDL_Point *area, void *data) {
    // Si el mouse está en el "header" de 30px de alto
    if (area->y < 30) {
        return SDL_HITTEST_DRAGGABLE;
    }
    return SDL_HITTEST_NORMAL;
}

int main(int argc, char* argv[]) {
	// Inicializar SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return -1;
    }

    // mover working dir al path donde está el ejecutable
    std::filesystem::path exePath = std::filesystem::absolute(argv[0]).parent_path();
    std::filesystem::current_path(exePath);

	Config cfg = loadConfig("./config.ini");
    
    // OpenGL antiguo (fixed pipeline) - ideal para Whisk3D/PSX-style
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    // Doble buffer
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Profundidad
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	// ---- CONFIGURAR ANTIALIASING ----
	// Pedir un framebuffer con multisampling
    if (cfg.enableAntialiasing) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // 4x MSAA
    } else {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }
    // Creamos flags según versión
    #if SDL_MAJOR_VERSION == 2
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;
    #elif SDL_MAJOR_VERSION == 3
        SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_MAXIMIZED;
    #endif

    if (cfg.graphicsAPI == "opengl") {
        std::cerr << "Renderer: OpenGl\n";
        windowFlags |= SDL_WINDOW_OPENGL;
    } 
    else {
        std::cerr << "Renderer desconocido, usando OpenGL por defecto\n";
        windowFlags |= SDL_WINDOW_OPENGL;
    }

    // Crear la ventana
    #if SDL_MAJOR_VERSION == 2
        window = SDL_CreateWindow(
            "Whisk 3D",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            winW, winH,
            windowFlags
        );
	#elif SDL_MAJOR_VERSION == 3
        window = SDL_CreateWindow(
            "Whisk 3D",
            winW, winH,
            windowFlags
        );
    #endif

	if (!window) {
		std::cerr << "Error SDL_CreateWindow: " << SDL_GetError() << std::endl;
		return -1;
	}

    // OpenGL
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Error SDL_GL_CreateContext: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_GL_SetSwapInterval(1); // VSync

    //carga el Skin
    std::string skinPath = "../Shared/UI/Skins/" + cfg.SkinName + "/skin.ini";
    loadColors(skinPath);

    //constructor symbian, linux y windows
    ConstructUniversal(argc, argv);

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

    rootViewport->Resize(winW, winH); // inicializar vista desde el inicio

    while (running) {
        Contadores();
        while (SDL_PollEvent(&e)) {
            #if SDL_MAJOR_VERSION == 2    
                if (e.type == SDL_QUIT) running = false;

                if (e.type == SDL_WINDOWEVENT &&
                    e.window.event == SDL_WINDOWEVENT_RESIZED) {

                    winW = e.window.data1;
                    winH = e.window.data2;
                    rootViewport->Resize(winW, winH);
                }   
                else {
                    InputUsuarioSDL3(e);
                }
            #elif SDL_MAJOR_VERSION == 3
                if (e.type == SDL_EVENT_QUIT) { running = false; }
                else if (e.type == SDL_EVENT_WINDOW_RESIZED ||
                        e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    //OnResizeViewports(e.window.data1, e.window.data2);

                    winW = e.window.data1;
                    winH = e.window.data2;
                    rootViewport->Resize(winW, winH);
                }
                else {
                    InputUsuarioSDL3(e);
                }
            #endif
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

            //std::cout << "Render Viewports" << std::endl;
            if (rootViewport){
                rootViewport->Render();
            }
            else {
                std::cout << "ERROR: ¡no hay rootViewport!" << std::endl;
            }
            SDL_GL_SwapWindow(window);
        }

        // Si querés liberar CPU:
        SDL_Delay(8); // duerme lo mínimo
    }

    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}