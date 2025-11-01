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
    SetGlobalScale(2);

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

    /*int New2dId = AddObject2D(Objects2D, UI::text);
    Objs2Dviewport3D.push_back(New2dId);
    Object2D& tempObject2D = Objects2D[New2dId];
    Text& tempText = Texts[tempObject2D.Id];
    tempText.SetValue("Whisk3D pre-alpha 25-10-10");
    tempText.SetScaleX(3);
    tempText.SetScaleY(3);
    tempObject2D.opacity = 255;
    tempText.SetX(20);
    tempText.SetY(10);
    tempText.SetColor(255, 255, 255);*/

    //creamos el Vieport Base

    int BaseView = AddViewport(View::ViewPort3D, -1, winW, winH, 0, 0, 1.0f, 1.0f);
    //lo dividimos
    SplitView(BaseView, View::Row);

    SplitView(1, View::Column);
    SetViewPort(3, View::Timeline);

    SplitView(2, View::Column);
    SetViewPort(6, View::Outliner);

    SetWidthViewport(1,winW*0.8);
    SetHeightViewport(3,winH*0.1);
    SetHeightViewport(5,winH*0.7);

    OnResizeViewports(winW, winH);
}