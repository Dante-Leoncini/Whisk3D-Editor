bool running = false;

// Configuración dependiente del tamaño de ventana
void OnResize(int w, int h) {
    if (h == 0) h = 1; // evitar división por cero

    winW = w;
    winH = h;

    aspect = (float)w / (float)h;

    // Viewport
    glViewport(0, 0, w, h);

    // Proyección
    SetPerspectiva(orthographic, true);

    // Volvemos al modelo
    /*glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, 8000,   // ojo
              0, 0, 0,      // centro
              0, 1, 0);     // up*/
}

//un constructor universal para todas las plataformas
void ConstructUniversal(){
    // Configuración básica de OpenGL
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

	InitOpenGL();

    // Inicializar la proyección al tamaño inicial de la ventana
    OnResize(winW, winH);

    // iluminación
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDiffuseLamp);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  objAmbient);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecularLamp);
    glLightfv(GL_LIGHT0, GL_POSITION, sunLightPosition);

    // Siempre un material por defecto
    NewMaterial(false);

    // Cámara y objetos iniciales
    AddObject(camera);
    Objects[0].posX = -800 * 6.8;
    Objects[0].posY = -800 * 7.29;
    Objects[0].posZ = 800 * 4.91;
    Objects[0].rotZ = -45.0;
    Objects[0].rotY = -26.15;
    Objects[0].scaleX = Objects[0].scaleY = Objects[0].scaleZ = 40000;

    AddObject(light);
    Objects[1].posX = -3000;
    Objects[1].posY = 1500;
    Objects[1].posZ = 4500;

    AddMesh(cubo);

    Object2D& tempObject2D = Objects2D[AddObject2D(UI::text)];
    Text& tempText = Texts[tempObject2D.Id];
    tempText.SetValue("Whisk3D pre-alpha 25-10-07");
    tempText.SetScaleX(3);
    tempText.SetScaleY(3);
    tempObject2D.opacity = 0.5f;
    tempText.SetX(20);
    tempText.SetY(10);
    tempText.SetColor(1.0f, 1.0f, 1.0f);
}