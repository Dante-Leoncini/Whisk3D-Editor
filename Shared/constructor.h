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

    CollectionActive = new Collection(SceneCollection);

    // Cámara y objetos iniciales
    new Camera(CollectionActive, -3.0f, 2.5f, 1.8f, 0, -35.0, 45.0);

    Light* luz1 = Light::Create(CollectionActive, 1.0f, 2.25f, 2.25f);
    luz1->SetDiffuse(1.0f, 0.0f, 0.0f);

    /*Light* luz2 = Light::Create(CollectionActive, -1.5f, -1.25f, 3.25f);
    luz2->SetDiffuse(1.0f, 1.0f, 1.0f);
    luz2->SetLightID(GL_LIGHT2);*/

    NewMesh(MeshType::cube, CollectionActive);

    /*Object* cubo = NewMesh(MeshType::cube, CollectionActive);
    Object* cubo2 = NewMesh(MeshType::cube, cubo);
    Object* cubo3 = NewMesh(MeshType::cube, cubo2);
    cubo3->Deseleccionar();
    cubo->Seleccionar();*/

    //abrir();

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