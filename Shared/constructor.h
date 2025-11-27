bool running = false;

#include "./importers/import_w3d.h"

//un constructor universal para todas las plataformas
void ConstructUniversal(int argc, char* argv[]){
    // Configuración básica de OpenGL
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    // iluminación. GL_LIGHT0 es el sol
    //SetViewType(RenderType::MaterialPreview);

    // Siempre un material por defecto
    MaterialDefecto = new Material("Default", true);
    
	CalculateMillisecondsPerFrame(60);

    //es el tamaño en X e Y de la textura
    CrearIconos(128, 128);
    CalcBorderUV(128, 128);
    CalcScrollUV(128, 128);
    SetGlobalScale(3);
    InitCursors();

    // ======================================================
    // Si se abrió un archivo .w3d al hacer doble click
    // ======================================================
    if(argc > 1){
        OpenW3D(argv[1]);
        return;
    }


    SDL_Surface* icon = IMG_Load("Whisk3D.png"); // o BMP si no usas SDL_image
    SDL_SetWindowIcon(window, icon);
    SDL_FreeSurface(icon);

    // ======================================================
    // Si se abre sin archivo → escena default
    // ======================================================
    CollectionActive = new Collection(SceneCollection);

    new Camera(CollectionActive,-3,2.5,1.8,0,-35,45);
    Light* L = Light::Create(CollectionActive,1,2.25,2.25);
    L->SetDiffuse(1,0,0);
    NewMesh(MeshType::cube,CollectionActive);

    rootViewport = new ViewportColumn(
        new Outliner(),
        new Viewport3D(),
        0.20f
    );
}