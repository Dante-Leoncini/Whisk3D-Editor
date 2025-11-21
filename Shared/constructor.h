bool running = false;

//un constructor universal para todas las plataformas
void ConstructUniversal(){
    // Configuración básica de OpenGL
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    // iluminación. GL_LIGHT0 es el sol
    SetViewType(RenderType::MaterialPreview);

    // Siempre un material por defecto
    MaterialDefecto = new Material("Default", true);
    
	CalculateMillisecondsPerFrame(60);

    //es el tamaño en X e Y de la textura
    CrearIconos(128, 128);
    CalcBorderUV(128, 128);
    CalcScrollUV(128, 128);
    SetGlobalScale(3);
    InitCursors();

    CollectionActive = new Collection(nullptr);

    // Cámara y objetos iniciales
    new Camera(CollectionActive, -3.0f, 2.5f, 1.8f, 0, -35.0, 45.0);

    //Light::Create(CollectionActive, 1.5f, 0.75f, 2.25f);

    //Light* luz1 = Light::Create(CollectionActive, -3.5f, 1.75f, 2.25f);
    Light* luz1 = Light::Create(CollectionActive, 1.0f, 2.25f, 2.25f);
    luz1->SetDiffuse(1.0f, 0.0f, 0.0f);

    /*Light* luz2 = Light::Create(luz1, -3.0f, 1.0f, 0.0f);
    luz2->SetDiffuse(0.0f, 0.0f, 1.0f);
    luz2->SetLightID(GL_LIGHT1);*/

    NewMesh(MeshType::cube, CollectionActive);
    //abrir();


    /*Collection* colec2 = new Collection(nullptr, "Collection_Test");
    Collections.push_back(colec2);
    CollectionActive = 1;
    
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);
    AddMesh(cubo);*/

    //vertical
    rootViewport = new ViewportColumn(
        new Outliner(),
        new Viewport3D(), 
        0.2f
    );

    /* horizontal
    rootViewport = new ViewportRow(
        new Viewport3D(), 
        new ViewportColumn(
            new Viewport3D(), new Outliner(), 0.3f
        ),
        0.7f
    );*/
}