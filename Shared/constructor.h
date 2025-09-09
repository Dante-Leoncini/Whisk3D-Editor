bool running = false;

//un constructor universal para todas las plataformas
void ConstructUniversal(){
	//la parte de openGL es igual en OpenGL ES 1.1 y el OpenGl de PC
    // Configuración básica de OpenGL
    glEnable(GL_DEPTH_TEST); // Habilitar z-buffer
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    // Cámara y transformaciones
    glMatrixMode(GL_PROJECTION);
    gluPerspective(fovDeg, aspect, nearClip, 20000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, 8000,   // ojo
        0, 0, 0,      // centro
        0, 1, 0);     // up

	//iluminacion
    glEnable(GL_LIGHTING);
    glEnable( GL_LIGHT0 );

    //glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    //glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    //glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    glLightfv(  GL_LIGHT0, GL_DIFFUSE,  lightDiffuseLamp  );
    glLightfv(  GL_LIGHT0, GL_AMBIENT,  lightAmbient  );
    glLightfv(  GL_LIGHT0, GL_SPECULAR, lightDiffuseLamp  );
    glLightfv(  GL_LIGHT0, GL_POSITION, sunLightPosition );

    // Habilitar la normalizaci�n de las normales
    glEnable(GL_NORMALIZE);	

	//tiene que haber un material por defecto siempre
	NewMaterial(false);
	
	AddObject(camera);
	Objects[0].posX = -800*6.8;
	Objects[0].posY = -800*7.29;
	Objects[0].posZ = 800*4.91;
	Objects[0].rotZ = -45.0;
	Objects[0].rotY = -26.15;
	Objects[0].scaleX = Objects[0].scaleY = Objects[0].scaleZ = 40000;

	AddObject(light);
	Objects[1].posX = -3000;
	Objects[1].posY = 1500;
	Objects[1].posZ = 4500;
	//GLfloat positionLight[4] = {0, -10, 0, 0};
    //GLfloat lightPositionSpot[4]  = {  0, -10, -1, 0 };
	//glLightfv(  nextLightId-1, GL_POSITION, positionPuntualLight );
	//glLightfv(  nextLightId, GL_POSITION, lightPositionSpot );
	
	AddMesh(cubo);
}