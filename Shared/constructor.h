bool running = false;

//un constructor universal para todas las plataformas
void ConstructUniversal(){
    // Configuración básica de OpenGL
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    // iluminación
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDiffuseLamp);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  objAmbient);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecularLamp);
    glLightfv(GL_LIGHT0, GL_POSITION, sunLightPosition);

    // Siempre un material por defecto
    NewMaterial(false);
    
	CalculateMillisecondsPerFrame(60);

    //es el tamaño en X e Y de la textura
    CrearIconos(128, 128);
    CalcBorderUV(128, 128);
    CalcScrollUV(128, 128);
    SetGlobalScale(3);

    Collection* colec = new Collection(nullptr, "Collection");
    Collections.push_back(colec);

    // Cámara y objetos iniciales
    Object* cam = AddObject(camera);
    cam->posX = -800 * 6.8;
    cam->posY = -800 * 7.29;
    cam->posZ = 800 * 4.91;
    cam->rotZ = -45.0;
    cam->rotY = -26.15;
    cam->scaleX = cam->scaleY = cam->scaleZ = 40000;

    Object* luz = AddObject(light);
    luz->posX = -3000;
    luz->posY = 1500;
    luz->posZ = 4500;

    AddMesh(cubo);

    Collection* colec2 = new Collection(nullptr, "Collection_Test");
    Collections.push_back(colec2);
    CollectionActive = 1;
    
    AddMesh(cubo);
}