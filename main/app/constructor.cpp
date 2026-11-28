#include "constructor.h"
#include "ViewPorts/Timeline.h" // el layout PC parte el 3D en columna [3D / Timeline]
#include "ViewPorts/LayoutInput.h" // LayoutDefault: EL layout por defecto, en un solo lugar
#include "objects/Primitives.h"   // W3dMeshSpawnedHook: el cubo por defecto nace con su recurso
#include "objects/Routine.h"      // el CONSTRUCTOR y el "Limpiar pantalla" que trae todo proyecto
#include "WhiskUI/theme/colors.h" // el fondo del tema: el "Clear color" de un proyecto nuevo
#include "W3dLang.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "w3dFilesystem.h"
#include "W3dInitUI.h"

// Variable global que estabas usando
bool running = false;

// LA ESCENA POR DEFECTO (arrancar sin archivo, Archivo > Nuevo): una coleccion hija de la raiz (la del usuario:
// CollectionActive != SceneCollection), camara, luz blanca, el cubo APOYADO en el piso (su centro a 1 m) y abajo el PISO,
// un plano de 8 x 8 m (pedido del duenio: "ese es el inicio de Whisk3D por defecto"). La camara, la luz y la vista
// (W3dDefaultSceneView) suben con el cubo. UNA sola definicion: la usa el arranque, Archivo > Nuevo y el comando
// `escenadefecto` del harness.
// LAS PRUEBAS (--script, g_w3dTestScene) arrancan con la escena de siempre -el cubo en el origen, sin piso-: cientos de
// pruebas cuentan con esas posiciones y con que el primer plano que agregan se llame "Plano". El harness arma la del
// editor con `escenadefecto editor` (prueba_escena_inicio)
bool g_w3dTestScene = false;
void W3dCreateDefaultScene() {
    // arriba de todo: el CONSTRUCTOR (corre una vez al empezar el juego: fija el "Clear color", el fondo del tema) y,
    // afuera de el y antes de la coleccion, la rutina que LIMPIA la pantalla en cada cuadro (apagarla o quitarla = no
    // limpiar, tambien en el editor)
    W3dDefaultRoutines(SceneCollection, T("Constructor"), T("Clear screen"),
                         ColorList[static_cast<int>(ColorID::background)]);
    CollectionActive = new Collection(SceneCollection);
    const float up = g_w3dTestScene ? 0.0f : 1.0f;   // (el cubo apoyado en el piso: todo 1 m mas arriba)

    // la CAMARA mira al cubo (su centro) desde arriba y al costado, a 5,5 unidades: con sus 45 grados el cubo entra entero
    // en el render (antes, a 4,3, se cortaban las puntas) y la camara se ve en la vista 3D. La rotacion se CALCULA: era
    // (-35, -45, 0) escrita a mano y no apuntaba al cubo
    { const Vector3 posCam(-3.8f, 3.2f + up, 2.3f);
      new Camera(CollectionActive, posCam, CameraRotLooking(posCam, Vector3(0, up, 0))); }
    Light* L = Light::Create(CollectionActive, 1, 2.25f + up, 2.25);
    L->SetNameObj(T("Light"));   // el Core la llama "Light"; el nombre lo pone el editor en su idioma (como Agregar > Luz)
    L->SetDiffuse(1, 1, 1); // luz BLANCA por defecto (antes era roja)

    // EL PISO: un plano de 8 x 8 m en el suelo (la medida en la malla: la escala del objeto queda en 1)
    if (!g_w3dTestScene) {
        Mesh* floor = (Mesh*)NewMesh(MeshType::plane, CollectionActive);
        if (floor) {
            Matrix4 S; S.Identity(); S.m[0] = 4.0f; S.m[10] = 4.0f;   // (el plano de la primitiva mide 2: x4 = 8 m)
            floor->ApplyMatrix(S);
            floor->meshType = -1;   // (ya no es el plano de la primitiva: regenerarlo lo volveria de 2 m)
            if (W3dMeshSpawnedHook) W3dMeshSpawnedHook();   // ("Plano", recurso de la biblioteca)
        }
    }
    Mesh* cube = (Mesh*)NewMesh(MeshType::cube, CollectionActive);
    if (cube && up != 0.0f) cube->pos = Vector3(0, up, 0);
    if (W3dMeshSpawnedHook) W3dMeshSpawnedHook();   // el cubo ya es el recurso "Cubo" de la biblioteca
    // la escena de fabrica ES una escena, y sigue siendolo aunque le pongan scripts (ver W3dRootsSetTypesDeduced)
    { extern void W3dRootsSetTypesDeduced(); W3dRootsSetTypesDeduced(); }
}
// LA VISTA de la escena por defecto: mira al centro del cubo (1 m arriba del piso), no al suelo
void W3dDefaultSceneView(Viewport3D* vp) {
    if (!vp || g_w3dTestScene) return;
    vp->pivot = Vector3(0, 1, 0);
    vp->RecalcOrbitPosition();
}

void ConstructUniversal(int argc, char* argv[]) {
    // estado de graficos inicial: el MISMO inicializador universal que Symbian
    W3dInitGraphics();

    // Siempre un material por defecto
    MaterialDefault = FactoryMaterial(new Material("Default", true));   // (con shaders: por pixel)


    // Tamaño de las texturas
    InitCursors();
    // (los grupos de propiedades ahora los construye cada panel)

    // UI compartida (texturas + fuente + iconos): mismo init que Symbian
    std::string skinDir = w3dFileSystem::GetResDir() + "/Skins/" + cfg.SkinName + "/";
    W3dInitUI(skinDir);
    SetGlobalScale(cfg.scale > 0 ? cfg.scale : 3); // configurable (config.start "scale"); 1 = estilo N95

    // RE-ENTRADA (Android): si la Activity fue destruida y main() se relanza con los globals C++ estaticos VIVOS, ya
    // existe el layout (rootViewport). NO recrear la escena default (sino DUPLICA: otro cubo + camara + luz sobre lo
    // que habia). El W3dInitGraphics/W3dInitUI de arriba ya re-subio las texturas de UI al contexto GL nuevo.
    // El manifest (singleTask + configChanges) + BLOCK_ON_PAUSE deberian evitar llegar aca; esto es la red de seguridad.
    if (rootViewport != NULL) return;

    // ======================================================
    // Configurar icono de ventana
    // ======================================================
    // OJO: va ANTES del chequeo de argv: al abrir un .w3d por doble click ese
    // camino retornaba temprano y la ventana quedaba SIN icono en la barra.
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
    // Si se abrio un archivo .w3d al hacer doble click
    // ======================================================
    // argv[1] = archivo .w3d a abrir (doble click / asociacion del sistema). Un FLAG
    // (--script, etc.) NO es un archivo: se ignora aca -> se arma la escena default
    // y el flag lo maneja main(). Si el archivo NO existe se sigue de largo a la
    // escena default (sin el guard, OpenW3D dejaba rootViewport NULL y crasheaba).
    if(argc > 1 && argv[1][0] != '-') {
        if (std::filesystem::exists(argv[1])) {
            w3dPath = argv[1];

            // Convertir a ruta absoluta
            w3dPath = std::filesystem::absolute(w3dPath).string();

            OpenW3D();
            return;
        }
        std::cerr << "Warning: no existe el archivo: " << argv[1]
                  << "  -> usando escena por defecto\n";
    }

    // ======================================================
    // Si se abre sin archivo -> escena default
    // ======================================================
    W3dCreateDefaultScene();

    // EL layout por defecto, UNA sola logica por TAMANO DE PANTALLA (LayoutDefault, LayoutInput.cpp):
    // un lado < 320px -> 2 viewports (3D + Propiedades, segun orientacion); mas grande -> 4 viewports
    // (3D + Timeline + Outliner + Props). La MISMA en todos los sistemas (antes habia una rama aca y otra
    // distinta en Symbian). El arranque sin archivo y el fallback de un .w3d sin bloque Layout arman lo mismo.
    Viewport3D* vp3dInitial = NULL;
    rootViewport = LayoutDefault(winW, winH, &vp3dInitial);
    W3dDefaultSceneView(vp3dInitial);   // (la vista mira al cubo, apoyado en el piso)
    // siempre hay un viewport activo (borde verde) por defecto: sin mouse
    // (Symbian) es la unica referencia; con mouse el hover lo pisa enseguida.
    viewPortActive = vp3dInitial;

    /*rootViewport = new ViewportColumn(
        new Outliner(),
        new Viewport3D(),
        0.20f
    );*/
}