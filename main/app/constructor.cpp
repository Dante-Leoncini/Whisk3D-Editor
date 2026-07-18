#include "constructor.h"
#include "ViewPorts/Timeline.h" // el layout PC parte el 3D en columna [3D / Timeline]

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "w3dFilesystem.h"
#include "W3dInitUI.h"

// Variable global que estabas usando
bool running = false;

void ConstructUniversal(int argc, char* argv[]) {
    // estado de graficos inicial: el MISMO inicializador universal que Symbian
    W3dInitGraphics();

    // Siempre un material por defecto
    MaterialDefecto = new Material("Default", true);

    CalculateMillisecondsPerFrame(60);

    // Tamaño de las texturas
    InitCursors();
    // (los grupos de propiedades ahora los construye cada panel)

    // UI compartida (texturas + fuente + iconos): mismo init que Symbian
    std::string skinDir = w3dFileSystem::GetResDir() + "/Skins/" + cfg.SkinName + "/";
    W3dInitUI(skinDir);
    SetGlobalScale(cfg.scale > 0 ? cfg.scale : 3); // configurable (config.ini "scale"); 1 = estilo N95

    // RE-ENTRADA (Android): si la Activity fue destruida y main() se relanza con los globals C++ estaticos VIVOS, ya
    // existe el layout (rootViewport). NO recrear la escena default (sino DUPLICA: otro cubo + camara + luz sobre lo
    // que habia). El W3dInitGraphics/W3dInitUI de arriba ya re-subio las texturas de UI al contexto GL nuevo.
    // El manifest (singleTask + configChanges) + BLOCK_ON_PAUSE deberian evitar llegar aca; esto es la red de seguridad.
    if (rootViewport != NULL) return;

    // ======================================================
    // Si se abrió un archivo .w3d al hacer doble click
    // ======================================================
    // argv[1] = archivo .w3d a abrir (doble click). Un FLAG (--script, etc.) NO es un
    // archivo: se ignora aca -> se arma la escena default y el flag lo maneja main().
    if(argc > 1 && argv[1][0] != '-') {
        w3dPath = argv[1];

        // Convertir a ruta absoluta
        w3dPath = std::filesystem::absolute(w3dPath).string();

        OpenW3D();
        return;
    }

    // ======================================================
    // Configurar icono de ventana
    // ======================================================
    int width, height, channels;

    stbi_uc* pixels = //stbi_load(w3dFileSystem::GetResDir() + "Whisk3D.png", &width, &height, &channels, 4);
                    stbi_load((w3dFileSystem::GetResDir() + "/Whisk3D.png").c_str(),
                        &width, &height, &channels, 4);

    if (pixels) {

        SDL_Surface* icon = SDL_CreateRGBSurfaceFrom(
            pixels,
            width,
            height,
            32,
            width * 4,
            0x000000ff,
            0x0000ff00,
            0x00ff0000,
            0xff000000
        );

        if (icon) {
            SDL_SetWindowIcon(window, icon);
            SDL_FreeSurface(icon);
        }

        stbi_image_free(pixels);
    }

    // ======================================================
    // Si se abre sin archivo → escena default
    // ======================================================
    CollectionActive = new Collection(SceneCollection);

    new Camera(CollectionActive, Vector3(-3, 2.5, 1.8), Vector3(-35.0f, -45.0f, 0.0f));
    Light* L = Light::Create(CollectionActive, 1, 2.25, 2.25);
    L->SetDiffuse(1, 1, 1); // luz BLANCA por defecto (antes era roja)

    NewMesh(MeshType::cube, CollectionActive);

    Viewport3D* vp3dInicial = new Viewport3D();
    // el layout se adapta a la orientacion (igual que Symbian, ver w3dlayout.cpp):
    if (winH > winW) {
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
        // WEB / ANDROID VERTICAL: viewport ARRIBA, solo PROPIEDADES abajo. Outliner +
        // propiedades juntos quedan muy apretados en un celular; el outliner se ve rotando
        // a horizontal.
        rootViewport = new ViewportColumn(vp3dInicial, new Properties(), 0.70f);
#else
        // PORTRAIT (ej. N95 240x320): viewport ARRIBA, fila [outliner | propiedades] ABAJO
        rootViewport = new ViewportColumn(
            vp3dInicial,
            new ViewportRow(new Outliner(), new Properties(), 0.40f),
            0.70f
        );
#endif
    } else {
        // LANDSCAPE (PC): a la IZQUIERDA una columna [viewport3D ARRIBA / Timeline ABAJO, bajito]; a la DERECHA
        // outliner sobre propiedades. El Timeline no necesita ser alto -> 85% para el 3D, 15% para el Timeline.
        rootViewport = new ViewportRow(
            new ViewportColumn(vp3dInicial, new Timeline(), 0.85f), // 3D 85% arriba, Timeline 15% abajo (bajito)
            new ViewportColumn(new Outliner(), new Properties(), 0.40f),
            0.70f
        );
    }
    // siempre hay un viewport activo (borde verde) por defecto: sin mouse
    // (Symbian) es la unica referencia; con mouse el hover lo pisa enseguida.
    viewPortActive = vp3dInicial;

    /*rootViewport = new ViewportColumn(
        new Outliner(),
        new Viewport3D(),
        0.20f
    );*/
}