#include "constructor.h"

// Variable global que estabas usando
bool running = false;

void ConstructUniversal(int argc, char* argv[]) {
    // Configuración básica de OpenGL
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    // Siempre un material por defecto
    MaterialDefecto = new Material("Default", true);

    CalculateMillisecondsPerFrame(60);

    // Tamaño de las texturas
    CrearIconos(128, 128);
    CalcBorderUV(128, 128);
    CalcScrollUV(128, 128);
    SetGlobalScale(3);
    InitCursors();

    //ACA HAY QUE CARGAR LAS TEXTURAS!!!!
    const std::vector<std::string> texFiles = {
        "font.png", "origen.png", "cursor3d.png", "relationshipLine.png", "lamp.png"
    };

    for (const auto& file : texFiles) {
        Textures.push_back(new Texture());

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

            if (!LoadTextureFromSurface(surf, Textures.back()->iID)) {
                __android_log_print(ANDROID_LOG_ERROR, "SDL_MAIN", "Error creando textura %s", path.c_str());
                SDL_FreeSurface(surf);
                return -1;
            }
            __android_log_print(ANDROID_LOG_VERBOSE, "SDL_MAIN", "Cargado %s", path.c_str());

            SDL_FreeSurface(surf); // liberamos superficie después de crear la textura
        #else
            // PC: ruta directa
            std::string path = exeDir + "/Skins/" + cfg.SkinName + "/" + file;
            if (!LoadTexture(path.c_str(), Textures.back()->iID)) {
                std::cerr << "Error cargando " << path << std::endl;
                return;
            }
            //std::cout << "Cargado " << file << std::endl;
        #endif
    }

    // ======================================================
    // Si se abrió un archivo .w3d al hacer doble click
    // ======================================================
    if(argc > 1) {
        w3dPath = argv[1];

        // Convertir a ruta absoluta
        w3dPath = std::filesystem::absolute(w3dPath).string();

        OpenW3D();
        return;
    }

    // ======================================================
    // Configurar icono de ventana
    // ======================================================
    SDL_Surface* icon = IMG_Load("./assets/Whisk3D.png"); // o BMP si no usas SDL_image
    if(icon) {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    }

    // ======================================================
    // Si se abre sin archivo → escena default
    // ======================================================
    CollectionActive = new Collection(SceneCollection);

    //new Camera(CollectionActive, Vector3(-3, 2.5, 1.8), Vector3(0.0f, 0.0f, 35.0f));
    new Camera(SceneCollection, Vector3(-3, 2.5, 1.8), Vector3(0.0f, 35.0f, -45.0f));
    Light* L = Light::Create(CollectionActive, 1, 2.25, 2.25);
    L->SetDiffuse(1, 0, 0);

    NewMesh(MeshType::cube, CollectionActive);

    rootViewport = new ViewportColumn(
        new Outliner(),
        new Viewport3D(),
        0.20f
    );
}