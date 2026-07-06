#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "test/W3dScript.h" // modo test: whisk3d --script <ruta>
#if !defined(__ANDROID__) // ESCRITORIO (Windows + Linux): codigo compartido de PC (import/export, browser, texturas,
                          // warp del mouse, swap). Android tiene su propia entrada de plataforma y no lo usa.
    #define NOMINMAX
    #include "ViewPorts/LayoutInput.h" // ruteo compartido + overlay del menu
#include "lectura-escritura.h"      // abrir(): dialogo + ImportOBJ
#include "ViewPorts/PopUp/FileBrowser.h" // explorador de archivos COMPARTIDO
#include "ViewPorts/PopUp/ProgressPopup.h" // barra de progreso + hook LayoutSwapBuffers
#include "importers/import_obj.h"        // ImportOBJ (el importador real)

// accion del File browser al elegir un .obj
static void ImportObjDesdeBrowser(const std::string& path) {
    ImportOBJ(path, false);
}

// el callback del menu Add (LayoutImportObj): abre el explorador compartido
static void PCImportObj() {
    AbrirFileBrowser("Importar modelo", "Import Wavefront OBJ", ".obj",
                     ImportObjDesdeBrowser);
}

// hook de swap para la barra de progreso (se redibuja DENTRO del export/import bloqueante)
static SDL_Window* g_swapWindow = NULL;
static void PCSwapBuffers() { if (g_swapWindow) SDL_GL_SwapWindow(g_swapWindow); }

#include "ViewPorts/Properties.h" // DialogoCargarTextura
#include "objects/Textures.h"
#include "objects/Materials.h"    // Material (asignar la textura cargada)

// warp del mouse real (los drags que no deben salirse de un rect)
static void PCWarpMouse(int x, int y) {
    SDL_WarpMouseInWindow(window, x, y);
    lastMouseX = x;
    lastMouseY = y;
}

// "Load Texture": el browser COMPARTIDO elige la imagen (png/jpg/bmp...) y la
// carga al material que la pidio (async: el browser es modal)
extern void RebindMaterialMeshPart(); // Properties.cpp
static Material* gTexMat = NULL;
extern bool gCargarTexturaComoNormal; // Properties.cpp: el "Load Texture" del normal map lo prende (compartido 4 OS)
static void TexturaElegida(const std::string& path) {
    if (!gTexMat) return;
    GLuint id = 0;
    if (LoadTexture(path.c_str(), id)) {
        Texture* t = new Texture(path);
        t->iID = id;
        Textures.push_back(t);
        if (gCargarTexturaComoNormal) { gTexMat->normalTexture = t; }
        else { gTexMat->texture = t; gTexMat->textureOn = true; }
        RebindMaterialMeshPart();
    }
    gTexMat = NULL; gCargarTexturaComoNormal = false;
}
// "Load Texture" (base Y normal map): el MISMO browser. Quien lo llama ya dejo gCargarTexturaComoNormal en el
// valor correcto (false=textura, true=normal); el callback de arriba decide el destino.
static void PCCargarTexturaEn(Material* mat) {
    gTexMat = mat;
    AbrirFileBrowser(gCargarTexturaComoNormal ? "Cargar normal map" : "Cargar textura",
                     gCargarTexturaComoNormal ? "Abrir normal map" : "Abrir textura",
                     ".png .jpg .jpeg .bmp .tga", TexturaElegida);
}
#include "WhiskUI/PopupMenu.h" // MenuPantallaW/H (desplegables)
#include "WhiskUI/glesdraw.h"  // W3dPantallaAlto (flip de Y)
#ifdef _WIN32
    #include <windows.h> // SOLO Windows (el resto del bloque es escritorio compartido Win+Linux)
#endif
#endif

#if SDL_MAJOR_VERSION == 2
    #define SDL_MAIN_HANDLED
    #include <SDL2/SDL.h>      
    #include "sdl_key_compat.h"
#elif SDL_MAJOR_VERSION == 3
    #include <SDL3/SDL.h>      
    //para las texturas
    #define STB_IMAGE_IMPLEMENTATION
    #include "stb/stb_image.h"
#endif

#ifdef __ANDROID__
    #include <android/log.h>
    #include <GLES/gl.h>    // OpenGL ES 1.1
    #include <GLES/glext.h>
#else
    #include <GL/gl.h>      // OpenGL Desktop
    #include <GL/glu.h>
    #ifndef _WIN32
    #include <GL/glext.h>
    #endif
#endif

//esto es solo para linux/android
#include <functional>
#include <cmath>
#include <cfloat>
#include <map>
#include <algorithm>
#include <cstring>
#include <unordered_set>


#include <filesystem>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>


#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <limits.h>
#endif

#include "w3dFilesystem.h"

//Whisk3D imports
#include "variables.h"
#include "WhiskUI/colores.h"
#include "ui/W3dColors.h" // W3dColores(Ubyte) + carga de colores del editor + Sincronizar
#include "objects/RenderColors.h" // paleta de render del core (Fase D: el sync de loadColors la llena)
#include "objects/Textures.h"
#include "objects/Materials.h"
#include "animation/Animation.h"
#include "animation/VertexAnimation.h"
#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/ColorPicker.h"
#include "controles.h"
#include "constructor.h"

// Reloj de ms para el core (Animation declara w3dGetTicks en Animation.h y NO incluye
// SDL). Aca el EDITOR/plataforma PC lo implementa con SDL. Symbian pondria el suyo.
unsigned int w3dGetTicks() { return SDL_GetTicks(); }

// Función simple para leer el ini
Config loadConfig(const std::string& filename) {
    Config cfg;

    std::string configPath = filename;

    #ifdef WHISK3D_LINUX
        // 1. Intentar primero desde config de usuario
        std::string userConfigDir = GetResDir();
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
            else if (key == "scale") cfg.scale = std::stoi(value);
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
        {"headerColor", ColorID::headerColor},
        {"negroTransparente", ColorID::negroTransparente},
        {"grisUI", ColorID::grisUI}
    };

    std::string skinPath = filename;

    #ifdef WHISK3D_LINUX
        // 1. Intentar primero desde config de usuario
        std::string userConfigDir = GetResDir();
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
            int eidx = editorColorIdPorNombre(name.c_str());
            if (eidx >= 0) {
                W3dColores[eidx][0] = r;
                W3dColores[eidx][1] = g;
                W3dColores[eidx][2] = b;
                W3dColores[eidx][3] = a;
                W3dColoresUbyte[eidx][0] = (GLubyte)(r*255);
                W3dColoresUbyte[eidx][1] = (GLubyte)(g*255);
                W3dColoresUbyte[eidx][2] = (GLubyte)(b*255);
                W3dColoresUbyte[eidx][3] = (GLubyte)(a*255);
            } else {
                std::cerr << "Color desconocido en colors.skin: " << name << "\n";
            }
        }
    }

    // Fase D: copiar la paleta de la UI a la de RENDER del core. Ahora COMPARTIDO
    // (colores.cpp), asi Symbian -que carga por loadColorsW3d- tambien lo hace.
    SincronizarRenderColores();

    return true;
}

void initGL(int width, int height) {
    #ifdef __ANDROID__
        w3dEngine::Viewport(0, 0, width, height);

        // Proyección simple (similar a gluPerspective)
        w3dEngine::MatrixMode(w3dEngine::Projection);
        w3dEngine::LoadIdentity();
        GLfloat ratio = (GLfloat)width / (GLfloat)height;
        GLfloat fov = 60.0f * 3.14159265f / 180.0f;
        GLfloat nearClip = 0.1f;
        GLfloat farClip = 100.0f;
        GLfloat top = tan(fov / 2) * nearClip;
        GLfloat bottom = -top;
        GLfloat left = bottom * ratio;
        GLfloat right = top * ratio;
        w3dEngine::Frustum(left, right, bottom, top, nearClip, farClip);

        w3dEngine::MatrixMode(w3dEngine::ModelView);
        w3dEngine::LoadIdentity();
    #else
        w3dEngine::Viewport(0, 0, width, height);
        w3dEngine::MatrixMode(w3dEngine::Projection);
        w3dEngine::LoadIdentity();
        w3dEngine::Perspective(60.0f, (float)width / height, 0.1f, 100.0f);
        w3dEngine::MatrixMode(w3dEngine::ModelView);
        w3dEngine::LoadIdentity();
    #endif

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Enable(w3dEngine::CullFace);
    w3dEngine::ClearColor(0.2f, 0.2f, 0.25f, 1.0f);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "Error SDL_Init: " << SDL_GetError() << std::endl;
        return -1;
    }

    w3dFileSystem::Init();

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

    // OJO: asignar el cfg GLOBAL (no declarar uno local que lo tape) -> asi ConstructUniversal
    // y todo lo demas ven los valores cargados (ej. scale para SetGlobalScale).
    #ifdef __ANDROID__
        cfg = loadConfig("res/config.ini");
    #else
        cfg = loadConfig(w3dFileSystem::GetResDir() + "/config.ini");
    #endif
    winW = cfg.width;  winH = cfg.height;  // aplicar el tamano del config a la ventana
                                           // (antes quedaba el default 640x480: el config no se usaba)
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
        // fullscreen -> maximizada; sino respeta width/height del config (util para
        // probar tamanos chicos, ej. 240x320 estilo N95 con scale=1)
        Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
        if (cfg.fullscreen) windowFlags |= SDL_WINDOW_MAXIMIZED;
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
        std::string skinPath = w3dFileSystem::GetResDir() + "/Skins/" + cfg.SkinName + "/skin.ini";
    #endif
    loadColors(skinPath);

    // Constructor de Whisk3D universal (Android, PC)
    ConstructUniversal(argc, argv);

    // "Add > import Wavefront" del menu + Load Texture + warp/swap: hooks de ESCRITORIO (Windows + Linux). En Android
    // no existen esas funciones (el bloque de PC no se compila); los hooks quedan NULL (se chequean antes de usar).
#if !defined(__ANDROID__)
    LayoutImportObj = PCImportObj;
    DialogoCargarTextura = PCCargarTexturaEn; // "Load Texture" (base Y normal map) -> browser compartido
    LayoutWarpMouse = PCWarpMouse;
    g_swapWindow = window; LayoutSwapBuffers = PCSwapBuffers; // barra de progreso (export/import)
#endif

    // Detectar primer mando
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        controller = SDL_GameControllerOpen(0);
        if (controller)
            std::cout << "Control detectado: " << SDL_GameControllerName(controller) << std::endl;

        /*
        esto solo anda en versiones nuevas del SDL2 gamepad = SDL_OpenGamepad(0);
        if (gamepad) {
            // luz verde del mando de ps4
            SDL_GamepadSetLED(gamepad, 255, 0, 0);
        }*/
    }

    SDL_Event e;
    running = true;
    // arbol en coordenadas ARRIBA-izquierda (unificado con Symbian);
    // el flip a GL pasa solo en glViewport/glScissor con este alto
    W3dPantallaAlto = winH;
    MenuPantallaW = winW;
    MenuPantallaH = winH;
    rootViewport->Resize(winW, winH);

    // MODO TEST: "whisk3d --script <ruta>" corre un script de comandos (sin GUI) y
    // sale (0 = todo OK, 1 = fallo). Ver main/test/W3dScript.cpp.
    for (int ai = 1; ai < argc; ai++) {
        if (std::string(argv[ai]) == "--script" && ai + 1 < argc) {
            bool ok = W3dRunScript(argv[ai + 1]);
            SDL_Quit();
            return ok ? 0 : 1;
        }
    }

    while (running) {
        Contadores();

        UpdateAnimations();
        UpdateAnimatedMaterials();

        while (SDL_PollEvent(&e)) {
            g_redraw = true; // cualquier evento (tecla/mouse/resize) -> hay que redibujar
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                winW = e.window.data1;
                winH = e.window.data2;
                W3dPantallaAlto = winH;
                MenuPantallaW = winW;
                MenuPantallaH = winH;
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

        // notificaciones (toasts): corre el timer (las de exito se cierran solas) y
        // mantiene vivo el render mientras haya alguna de exito activa
        static Uint32 lastNotif = now;
        NotificacionesTick((float)(now - lastNotif) / 1000.0f);
        lastNotif = now;

        // CARGA DIFERIDA de texturas del import: 1 por frame (decode+upload). Prende g_redraw al cargar una -> el
        // modelo aparece enseguida y las texturas entran solas. No-op si no hay pendientes.
        { extern void CargarTexturasPendientes(); CargarTexturasPendientes(); }

        // Render EVENT-DRIVEN: solo si algo cambio (g_redraw) o hay una animacion EN
        // PLAY (vertex-anim o materiales animados activos). Sino no se dibuja nada ->
        // CPU casi 0 en reposo (como Blender), en vez de renderizar 60 veces/seg al pedo.
        bool animando = HayAnimacionActiva() || !VertexAnimationActives.empty();
        if ((g_redraw || animando) && now - lastRenderTime >= 16) {  // ~60hz
            lastRenderTime = now;
            if (rootViewport){
                rootViewport->Render();
            }
            // popups y desplegables (los dibuja el modulo compartido)
            LayoutRenderMenu(winW, winH);
            NotificacionesRender(winW, winH); // toasts ENCIMA de todo
            LayoutTickFPS(now); // overlay de fps (reloj de PC: SDL_GetTicks)
            SDL_GL_SwapWindow(window);
            g_redraw = false;
        }

        SDL_Delay(8); // liberar CPU
    }

    if (controller) SDL_GameControllerClose(controller);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}