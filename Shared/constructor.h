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

    // Cámara y objetos iniciales
    AddObject(camera);
    Objects[0]->posX = -800 * 6.8;
    Objects[0]->posY = -800 * 7.29;
    Objects[0]->posZ = 800 * 4.91;
    Objects[0]->rotZ = -45.0;
    Objects[0]->rotY = -26.15;
    Objects[0]->scaleX = Objects[0]->scaleY = Objects[0]->scaleZ = 40000;

    AddObject(light);
    Objects[1]->posX = -3000;
    Objects[1]->posY = 1500;
    Objects[1]->posZ = 4500;

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