#ifdef _WIN32
    #include <windows.h>
#endif

#if SDL_MAJOR_VERSION == 2
    #include <SDL2/SDL.h>      
    //#include <SDL2/SDL_image.h>
    #include <SDL_image.h>
    #include "sdl_key_compat.h"
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
    #include <GL/glext.h> 
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
#include "Whisk3Dutils.h"

#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

//helpers para convertir funciones para android y openglES 1.1
#include "render/GLES_Android_helpers.h"

#ifdef __ANDROID__
#elif defined(_WIN32)
    // ============================
    // WINDOWS
    // ============================
    #include <windows.h>
    std::string getExeDir() {
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);

        std::string path(buffer);
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos)
            path = path.substr(0, pos);

        return path;
    }
#else
    #include <unistd.h>
    #include <limits.h>

    std::string getUserConfigDir() {
        #ifdef WHISK3D_LINUX
            const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
            std::string configDir;

            if (xdgConfigHome && strlen(xdgConfigHome) > 0) {
                configDir = std::string(xdgConfigHome) + "/Whisk3d";
            } else {
                const char* home = std::getenv("HOME");
                if (home) {
                    configDir = std::string(home) + "/.config/Whisk3d";
                }
            }

            // Create directory if it doesn't exist
            if (!configDir.empty() && !std::filesystem::exists(configDir)) {
                std::filesystem::create_directories(configDir);
                std::cout << "Creado directorio de configuración de usuario: " << configDir << "\n";
            }

            return configDir;
        #else
            return "";
        #endif
    }

    std::string getExeDir() {
        #ifdef _WIN32
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            std::string path(buffer);
            size_t pos = path.find_last_of("\\/");
            if (pos != std::string::npos)
                path = path.substr(0, pos);
            return path;
        #else
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
        #endif
    }
    std::string getResDir() {
        #ifdef __ANDROID__
            return "res"; // assets internos del APK
        #elif defined(WHISK3D_LINUX)
            std::string exeDir;
            char exePath[PATH_MAX];
            ssize_t count = readlink("/proc/self/exe", exePath, PATH_MAX);
            if (count != -1) {
                exeDir = std::string(exePath, count);
                size_t pos = exeDir.find_last_of('/');
                if (pos != std::string::npos)
                    exeDir = exeDir.substr(0, pos);
            }

            // 1. Ruta relativa al ejecutable (portable)
            if (!exeDir.empty()) {
                std::string portablePath = exeDir + "/res";
                if (std::filesystem::exists(portablePath)) {
                    std::cout << "parece que es una version portable. se va a usar el res local\n";
                    return portablePath;
                }
            }

            if (!exeDir.empty()) {
                std::string appImagePath = exeDir + "/../share/Whisk3d/res";
                if (std::filesystem::exists(appImagePath)) {
                    std::cout << "usando res de AppImage/bundle en " << appImagePath << "\n";
                    return std::filesystem::canonical(appImagePath).string();
                }
            }

            // 2. Ruta usando XDG_DATA_DIRS (instalación sistema)
            const char* xdgDataDirs = std::getenv("XDG_DATA_DIRS");
            std::string dataDirs = xdgDataDirs ? xdgDataDirs : "/usr/local/share:/usr/share";
            std::istringstream stream(dataDirs);
            std::string dir;
            while (std::getline(stream, dir, ':')) {
                std::string systemPath = dir + "/Whisk3d/res";
                if (std::filesystem::exists(systemPath)){
                    std::cout << "instalaste Whisk3D en tu linux! me caes bien. usando res en " << systemPath << "\n";
                    return systemPath;
                }
            }

            // 3. Si no hay nada, fallback
            std::cout << "esto seguro explota!\n";
            return "";
        #else
            // build local o Windows
            char result[PATH_MAX];
            ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
            std::string path;
            if (count != -1) {
                path = std::string(result, count);
                size_t pos = path.find_last_of('/');
                if (pos != std::string::npos)
                    path = path.substr(0, pos);
            }
            return path + "/res";
        #endif
    }
#endif

//Whisk3D imports
#include "variables.h"
#include "UI/colores.h"
#include "objects/Textures.h"
#include "objects/Materials.h"
#include "animation/Animation.h"
#include "ViewPorts/ViewPorts.h"
#include "controles.h"
#include "constructor.h"

// Función simple para leer el ini
Config loadConfig(const std::string& filename) {
    Config cfg;

    std::string configPath = filename;

    #ifdef WHISK3D_LINUX
        // 1. Intentar primero desde config de usuario
        std::string userConfigDir = getUserConfigDir();
        if (!userConfigDir.empty()) {
            std::string userConfigPath = userConfigDir + "/config.ini";
            if (std::filesystem::exists(userConfigPath)) {
                configPath = userConfigPath;
                std::cout << "Usando config de usuario: " << configPath << "\n";
            } else {
                std::cout << "No se encontró config en directorio de usuario, usando sistema: " << filename << "\n";
            }
        }
    #endif

    #ifdef __ANDROID__
        SDL_RWops* rw = SDL_RWFromFile(configPath.c_str(), "rb");
        if (!rw) {
            __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "No se pudo abrir %s", configPath.c_str());
            return cfg;
        }

        Sint64 size = SDL_RWsize(rw);
        std::vector<char> buffer(size + 1, 0);
        SDL_RWread(rw, buffer.data(), 1, size);
        SDL_RWclose(rw);

        std::istringstream iss(buffer.data());
    #else
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir " << configPath << ", usando valores por defecto.\n";
            return cfg;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

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
        {"background", ColorID::background},
        {"blanco", ColorID::blanco},
        {"accent", ColorID::accent},
        {"accentDark", ColorID::accentDark},
        {"negro", ColorID::negro},
        {"gris", ColorID::gris},
        {"naranjaFace", ColorID::naranjaFace},
        {"headerColor", ColorID::headerColor},
        {"negroTransparente", ColorID::negroTransparente},
        {"grisUI",ColorID:: grisUI},
        {"LineaPiso", ColorID::LineaPiso},
        {"LineaPisoRoja", ColorID::LineaPisoRoja},
        {"LineaPisoVerde", ColorID::LineaPisoVerde},
        {"ColorTransformX", ColorID::ColorTransformX},
        {"ColorTransformY", ColorID::ColorTransformY},
        {"ColorTransformZ", ColorID::ColorTransformZ}
    };

    std::string skinPath = filename;

    #ifdef WHISK3D_LINUX
        // 1. Intentar primero desde config de usuario
        std::string userConfigDir = getUserConfigDir();
        if (!userConfigDir.empty()) {
            // Extraer nombre del skin del path original
            size_t skinsPos = filename.find("/Skins/");
            if (skinsPos != std::string::npos) {
                std::string skinRelPath = filename.substr(skinsPos);
                std::string userSkinPath = userConfigDir + skinRelPath;

                if (std::filesystem::exists(userSkinPath)) {
                    skinPath = userSkinPath;
                    std::cout << "Usando skin de usuario: " << skinPath << "\n";
                } else {
                    std::cout << "No se encontró skin en directorio de usuario, usando sistema: " << filename << "\n";
                }
            }
        }
    #endif

    std::istringstream fileStream;

    #ifdef __ANDROID__
        SDL_RWops* rw = SDL_RWFromFile(skinPath.c_str(), "rb");
        if (!rw) {
            __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "No se pudo abrir %s", skinPath.c_str());
            return false;
        }

        Sint64 size = SDL_RWsize(rw);
        std::vector<char> buffer(size + 1, 0);
        SDL_RWread(rw, buffer.data(), 1, size);
        SDL_RWclose(rw);

        fileStream.str(buffer.data());
    #else
        std::ifstream file(skinPath);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir " << skinPath << ", usando colores por defecto.\n";
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
            int idx = static_cast<int>(it->second);
            ListaColores[idx][0] = r;
            ListaColores[idx][1] = g;
            ListaColores[idx][2] = b;
            ListaColores[idx][3] = a;

            //los colores fixed solo son para android
            #ifdef __ANDROID__
                // Convertimos a GLfixed
                ListaColoresX[idx][0] = COLOR_CONVERT_FIXED(r);
                ListaColoresX[idx][1] = COLOR_CONVERT_FIXED(g);
                ListaColoresX[idx][2] = COLOR_CONVERT_FIXED(b);
                ListaColoresX[idx][3] = COLOR_CONVERT_FIXED(a);
            #endif

            ListaColoresUbyte[idx][0] = (GLubyte)(r*255);
            ListaColoresUbyte[idx][1] = (GLubyte)(g*255);
            ListaColoresUbyte[idx][2] = (GLubyte)(b*255);
            ListaColoresUbyte[idx][3] = (GLubyte)(a*255);

            #ifdef __ANDROID__
                __android_log_print(ANDROID_LOG_VERBOSE, "SDL_MAIN", 
                    "Color %s cargado: R=%.3f G=%.3f B=%.3f A=%.3f",
                    name.c_str(), r, g, b, a);
            #else
                //std::cout << "Color " << name << " cargado: R=" << r << " G=" << g << " B=" << b << " A=" << a << "\n";
            #endif
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
        exeDir = getResDir();
        Config cfg = loadConfig(exeDir + "/config.ini");
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
        std::string skinPath = exeDir + "/Skins/" + cfg.SkinName + "/skin.ini";
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

    SDL_Event e;
    running = true;
    rootViewport->Resize(winW, winH);

    while (running) {
        Contadores();
        UpdateAnimatedMaterials();

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
            if (rootViewport){
                rootViewport->Render();
            }
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