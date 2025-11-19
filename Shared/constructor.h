bool running = false;

//un constructor universal para todas las plataformas
void ConstructUniversal(){
    // Configuración básica de OpenGL
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    // iluminación. GL_LIGHT0 es el sol
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDiffuseLamp);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  objAmbient);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecularLamp);
    glLightfv(GL_LIGHT0, GL_POSITION, sunLightPosition);

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
    new Camera(CollectionActive, -2.5f, -2.5f, 1.8f, 0, -35.0, -45.0);

    Light::Create(CollectionActive, 1.5f, 0.75f, 2.25f);

    NewMesh(MeshType::cube, CollectionActive);

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

    rootViewport = new ViewportRow(
        new Viewport3D(), 
        new ViewportColumn(
            new Viewport3D(), new Outliner(), 0.3f
        ),
        0.7f
    );
}