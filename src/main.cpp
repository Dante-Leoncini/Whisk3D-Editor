#include <SDL.h> // Needed to properly handle main() on iOS

//#include <SDL_image.h>

#if SDL_MAJOR_VERSION == 2
    //#include <SDL2/SDL.h>      
    //#include <SDL2/SDL_image.h>
    #include <SDL_image.h>
    #include "../include/sdl_key_compat.h"
#elif SDL_MAJOR_VERSION == 3
    #include <SDL3/SDL.h>      
    //para las texturas
    #define STB_IMAGE_IMPLEMENTATION
    #include "stb_image.h"
#endif

#ifdef __ANDROID__
    #include <android/log.h>
    #include <GLES/gl.h>    // OpenGL ES 1.1
    #include <GLES/glext.h>
#else
    #include <GL/gl.h>      // OpenGL Desktop
    #include <GL/glu.h>
#endif

//esto es solo para linux/android
#include <functional>
#include <cmath>
#include <cfloat>
#include <map>
#include <algorithm>
#include <cstring>
#include <unordered_set>

//utilidades de Whisk3D de uso general. mas que nada para manejo de punteros y memoria
#include "../include/Whisk3Dutils.h"

#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

//helpers para convertir funciones para android y openglES 1.1
#include "../include/GLES_Android_helpers.h"

//variables de SDL para Linux/Android
SDL_Window* window = nullptr;  // definición real
SDL_GameController* controller = nullptr;
SDL_GLContext glContext = nullptr;

int winW = 640; 
int winH = 480;


#ifdef __ANDROID__
#else
    #include <unistd.h>
    #include <limits.h>
    std::string getExeDir() {
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        std::string path;
        if (count != -1) {
            path = std::string(result, count);
            size_t pos = path.find_last_of('/');
            if (pos != std::string::npos)
                path = path.substr(0, pos);
        }
        return path;
    }
#endif

//Whisk3D imports
#include "../include/tablero.h"
#include "../include/Floor.h"
#include "../include/UI/GeometriaUI.h"

#include "../include/clases.h"
#include "../include/variables.h"
#include "../include/UI/colores.h"
#include "../include/OpcionesRender.h"

#include "../include/Animation.h"
#include "../include/Objects/ObjectMode.h"
#include "../include/render.h"
#include "../include/importers/import_obj.h"
//#include "../include/import_vertex_animation.h"
#include "../include/lectura-escritura.h"
#include "../include/ViewPorts/ViewPorts.h"
#include "../include/funciones.h"
#include "../include/controles.h"
#include "../include/constructor.h"

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
    Config cfg;

    #ifdef __ANDROID__
        SDL_RWops* rw = SDL_RWFromFile(filename.c_str(), "rb");
        if (!rw) {
            __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "No se pudo abrir %s", filename.c_str());
            return cfg; // devuelve defaults
        }

        Sint64 size = SDL_RWsize(rw);
        std::vector<char> buffer(size + 1, 0);
        SDL_RWread(rw, buffer.data(), 1, size);
        SDL_RWclose(rw);

        std::istringstream iss(buffer.data());
    #else
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir " << filename << ", usando valores por defecto.\n";
            return cfg;
        }

        // Leer todo el archivo a un string
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        // Declarar e inicializar el istringstream
        std::istringstream iss(content);
    #endif

    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string key, eq, value;
        if (lineStream >> key >> eq >> value && eq == "=") {
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

bool loadColors(const std::string& filename) {
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

    std::istringstream fileStream;

    #ifdef __ANDROID__
        SDL_RWops* rw = SDL_RWFromFile(filename.c_str(), "rb");
        if (!rw) {
            __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "No se pudo abrir %s", filename.c_str());
            return false;
        }

        Sint64 size = SDL_RWsize(rw);
        std::vector<char> buffer(size + 1, 0); // +1 para null-terminator
        SDL_RWread(rw, buffer.data(), 1, size);
        SDL_RWclose(rw);

        fileStream.str(buffer.data());
    #else
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir " << filename << ", usando colores por defecto.\n";
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        fileStream.str(content);
    #endif

    std::string line;
    while (std::getline(fileStream, line)) {
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

void initGL(int width, int height) {
#ifdef __ANDROID__
    glViewport(0, 0, width, height);

    // Proyección simple (similar a gluPerspective)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    GLfloat ratio = (GLfloat)width / (GLfloat)height;
    GLfloat fov = 60.0f * 3.14159265f / 180.0f;
    GLfloat nearClip = 0.1f;
    GLfloat farClip = 100.0f;
    GLfloat top = tan(fov / 2) * nearClip;
    GLfloat bottom = -top;
    GLfloat left = bottom * ratio;
    GLfloat right = top * ratio;
    glFrustumf(left, right, bottom, top, nearClip, farClip);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
#else
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)width / height, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
#endif

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.2f, 0.2f, 0.25f, 1.0f);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Configuración OpenGL
    #ifdef __ANDROID__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    #else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    #endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    #ifdef __ANDROID__
        Config cfg = loadConfig("res/config.ini");
    #else
        std::string exeDir = getExeDir();
        Config cfg = loadConfig(exeDir + "/res/config.ini");
    #endif
    if (cfg.enableAntialiasing) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    }

    #ifdef __ANDROID__
        window = SDL_CreateWindow("Whisk3D Pre-Alpha",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                winW, winH,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    #else
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;
        window = SDL_CreateWindow("Whisk3D Pre-Alpha", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, windowFlags);
    #endif

    if (!window) {
        std::cerr << "Error SDL_CreateWindow: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
                      
    SDL_GetWindowSize(window, &winW, &winH);
    //SDL_GL_GetDrawableSize(window, &w, &h);

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Error SDL_GL_CreateContext: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    //hay que ver que onda estos dos
    SDL_GL_SetSwapInterval(1);
    initGL(winW, winH);

    // Cargar skin
    #ifdef __ANDROID__
        std::string skinPath = "res/Skins/" + cfg.SkinName + "/skin.ini";
    #else
        std::string skinPath = exeDir + "/res/Skins/" + cfg.SkinName + "/skin.ini";
    #endif
    loadColors(skinPath);

    // Constructor de Whisk3D universal (Android, PC)
    ConstructUniversal(argc, argv);

    // Detectar primer mando
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        controller = SDL_GameControllerOpen(0);
        if (controller)
            std::cout << "Control detectado: " << SDL_GameControllerName(controller) << std::endl;
    }

    // Carga de texturas
    const std::vector<std::string> texFiles = {
        "font.png", "origen.png", "cursor3d.png", "relationshipLine.png", "lamp.png"
    };

    for (const auto& file : texFiles) {
        Textures.push_back(Texture());

        #ifdef __ANDROID__
            // Para Android usamos SDL_RWFromFile para acceder al asset dentro del APK
            std::string path = "res/Skins/" + cfg.SkinName + "/" + file;
            SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
            if (!rw) {
                __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "No se pudo abrir %s: %s", path.c_str(), SDL_GetError());
                return -1;
            }

            SDL_Surface* surf = IMG_Load_RW(rw, 1); // 1 = cierra rw automáticamente
            if (!surf) {
                __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "Error cargando %s: %s", path.c_str(), IMG_GetError());
                return -1;
            }

            if (!LoadTextureFromSurface(surf, Textures.back().iID)) {
                __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "Error creando textura %s", path.c_str());
                SDL_FreeSurface(surf);
                return -1;
            }
            __android_log_print(ANDROID_LOG_VERBOSE, "SDL_MAIN", "Cargado %s", path.c_str());

            SDL_FreeSurface(surf); // liberamos superficie después de crear la textura
        #else
            // PC: ruta directa
            std::string path = exeDir + "/res/Skins/" + cfg.SkinName + "/" + file;
            if (!LoadTexture(path.c_str(), Textures.back().iID)) {
                std::cerr << "Error cargando " << path << std::endl;
                return -1;
            }
            std::cout << "Cargado " << file << std::endl;
        #endif
    }

    SDL_Event e;
    running = true;
    rootViewport->Resize(winW, winH);

    while (running) {
        Contadores();

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                winW = e.window.data1;
                winH = e.window.data2;
                rootViewport->Resize(winW, winH);
            } else {
                InputUsuarioSDL3(e);
            }
        }

        // Animación
        Uint32 now = SDL_GetTicks();
        if (now - lastAnimTime >= millisecondsPerFrame) {
            lastAnimTime = now;
            // Actualizar frame
            ReloadAnimation();
        }

        // Render 3D
        if (now - lastRenderTime >= 16) {  // ~60hz
            lastRenderTime = now;
            if (rootViewport)
                rootViewport->Render();
            SDL_GL_SwapWindow(window);
        }

        SDL_Delay(8); // liberar CPU
    }

    if (controller) SDL_GameControllerClose(controller);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}