class Viewport3D {
	public:
		int Parent = -1;
        bool orthographic = false;
        bool SimularZBuffer = false;

        void Render(){
            //Configuracion inicial!
            Viewport& parentView =  Viewports[Parent];

            glEnable(GL_DEPTH_TEST); // Habilitar z-buffer
	        glViewport(parentView.x, parentView.y, parentView.width, parentView.height); // x, y, ancho, alto
            glMatrixMode( GL_PROJECTION );
            glLoadIdentity();
            // Proyección ortográfica
            if ( orthographic ){
                float size = 90.0f;
                glOrtho(-size * parentView.aspect, size * parentView.aspect,
                        -size, size,
                        nearClip, farClip);
            }
            else {
                gluPerspective(fovDeg, parentView.aspect, nearClip, farClip);
            }
            glMatrixMode( GL_MODELVIEW );

            //Empieza el render
            if (SimularZBuffer){
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Color negro
                glEnable(GL_FOG);
                glFogf(GL_FOG_MODE, GL_LINEAR); // Tipo de niebla lineal
                glFogf(GL_FOG_START, nearClip);  // Distancia inicial de la niebla
                glFogf(GL_FOG_END, farClip);     // Distancia final de la niebla
                GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
                glFogfv(GL_FOG_COLOR, fogColor); // Color de la niebla
            }
            else {
                glDisable(GL_FOG);
                glClearColor(ListaColores[background][0],ListaColores[background][1],ListaColores[background][2],ListaColores[background][3]);
            }

            // Limpiar pantalla
            glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
            glLoadIdentity();
            glEnable( GL_DEPTH_TEST );	

            if (ViewFromCameraActive){
                RecalcViewPos();
                //glTranslatef( posX, posZ, posY);
            }
            glTranslatef( posX, posZ, -cameraDistance+posY );

            //original
            glRotatef(rotY, 1, 0, 0); //angulo, X Y Z
            glRotatef(rotX, 0, 1, 0); //angulo, X Y Z
            glScalef(1024.0f / 65536.0f, 1024.0f / 65536.0f, 1024.0f / 65536.0f);
            glTranslatef( PivotX, PivotZ, PivotY);

            //primero hay que colocar las luces en caso de estar en modo render!
            if ((!SimularZBuffer && view == MaterialPreview) || view == Rendered){
                for (size_t o = 0; o < Objects.size(); o++) {
                    Object& obj = Objects[o];
                    if(!obj.visible || obj.type != light ) {continue;}
                    Light& light = Lights[obj.Id];

                    glPushMatrix(); //guarda la matrix
                    glTranslatef( obj.posX, obj.posZ, obj.posY);
                    GLfloat lightPos[] = {0.0f, 0.0f, 0.0f, 1.0f}; // Luz puntual en la posici�n transformada
                    glLightfv(light.lightId, GL_POSITION, lightPos);
                    //glLightfv(  light.lightId, GL_POSITION, positionPuntualLight );
                    glPopMatrix(); //reinicia la matrix a donde se guardo  
                }	
            }

            //por defecto la linea es de 1	
            glLineWidth(1);	
            
            //bucle que dibuja cada objeto en orden
            if(Meshes.size() > 0){
                // Funcion principal para iterar sobre la coleccion
                for (size_t o = 0; o < Collection.size(); o++) {
                    Object& obj = Objects[Collection[o]];
                    RenderMeshAndChildren(obj);
                }
            }

            //el valor que tiene que tener para dibujar el resto correctamente
            //glDisable(GL_COLOR_MATERIAL);
            glDisableClientState( GL_COLOR_ARRAY );

            //fin del dibujado de objetos
            //si estaba simulando el zbuffer. el resto no hace falta
            if (SimularZBuffer){		
                redibujar = false;
                return;
            };

            //el resto de objetos no usan materiales ni luces
            glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE,  ListaColores[negro] );
            glMaterialfv(   GL_FRONT_AND_BACK, GL_AMBIENT,  ListaColores[negro] );
            glMaterialfv(   GL_FRONT_AND_BACK, GL_SPECULAR, ListaColores[negro] );
            glDisable( GL_CULL_FACE ); // Enable back face culling.
            glDisable( GL_LIGHTING );
            glEnable(GL_COLOR_MATERIAL);
            glDisable( GL_TEXTURE_2D );
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            
            //se encarga de dibujar el layout 
            if (showOverlays){
                //dibujar las lineas del piso y el piso
                if (showFloor || showXaxis || showYaxis){
                    glEnable(GL_FOG);
                    glFogf(GL_FOG_MODE, GL_LINEAR); // Tipo de niebla lineal
                    glFogf(GL_FOG_START, nearClip);  // Distancia inicial de la niebla
                    glFogf(GL_FOG_END, 800.0f);     // Distancia final de la niebla
                    //GLfloat fogColor[] = {0.23f, 0.23f, 0.23f, 1.f};
                    //glClearColor( ClearColor[0], ClearColor[1], ClearColor[2], 1.f );
                    glFogfv(GL_FOG_COLOR, ListaColores[background]); // Color de la niebla DEL piso. que es mas pequeña que otros fog
                    glLineWidth(1);	 

                    glVertexPointer( 3, GL_SHORT, 0, objVertexdataFloor );
                    //glNormalPointer( GL_BYTE, 0, objNormaldataFloor );

                    //dibuja el piso	
                    if (showFloor){
                        glColor4f(
                            ListaColores[LineaPiso][0],
                            ListaColores[LineaPiso][1],
                            ListaColores[LineaPiso][2],
                            ListaColores[LineaPiso][3]
                        );
                        glDrawElements( GL_LINES, objFacesFloor, GL_UNSIGNED_SHORT, objFacedataFloor );			
                    }		
                    //linea Roja
                    if (showXaxis){				
                        glLineWidth(2);
                        glColor4f(
                            ListaColores[LineaPisoRoja][0],
                            ListaColores[LineaPisoRoja][1],
                            ListaColores[LineaPisoRoja][2],
                            ListaColores[LineaPisoRoja][3]
                        );
                        glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );
                        glLineWidth(1);	//la deja como es por defecto
                    }
                    else if (showFloor){
                        glColor4f(
                            ListaColores[LineaPiso][0],
                            ListaColores[LineaPiso][1],
                            ListaColores[LineaPiso][2],
                            ListaColores[LineaPiso][3]
                        );
                        glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );
                    }
                    //linea Verde	
                    if (showYaxis){
                        glLineWidth(2);
                        glColor4f(
                            ListaColores[LineaPisoVerde][0],
                            ListaColores[LineaPisoVerde][1],
                            ListaColores[LineaPisoVerde][2],
                            ListaColores[LineaPisoVerde][3]
                        );
                        glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde );
                        glLineWidth(1);	//la deja como es por defecto
                    }
                    else if (showFloor){
                        glColor4f(
                            ListaColores[LineaPiso][0],
                            ListaColores[LineaPiso][1],
                            ListaColores[LineaPiso][2],
                            ListaColores[LineaPiso][3]
                        );
                        glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde );
                    }	
                    glDisable(GL_FOG);
                }

                //esto solo se hace si hay objetos
                //if (Objects.Count() > 0){
                if (Objects.size() > 0){
                    //dibujo de objetos nuevo!
                    glLineWidth(1);	 
                    //for (int o = 0; o < Collection.Count(); o++) {
                    for (size_t o = 0; o < Collection.size(); o++) {
                        RenderObjectAndChildrens(Collection[o]);
                    }	 

                    //dibujar lineas parent		
                    if (ShowRelantionshipsLines){
                        glEnable( GL_TEXTURE_2D );
                        glEnable( GL_BLEND );
                        glDepthMask(GL_FALSE); // Desactiva la escritura en el Z-buffer				
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                        glTexCoordPointer( 2, GL_FLOAT, 0, lineUV ); //SpriteUvSize
                        glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);	
                        glBindTexture( GL_TEXTURE_2D, Textures[3].iID ); //selecciona la de linea punteada	
                        //for (int o = 0; o < Collection.Count(); o++) {
                        for (size_t o = 0; o < Collection.size(); o++) {
                            Object& obj = Objects[Collection[o]];
                            //if (obj.Childrens.Count() > 0){
                            if (obj.Childrens.size() > 0){
                                RenderLinkLines(Collection[o]);
                            }
                        }
                        glDepthMask(GL_TRUE); // Reactiva la escritura en el Z-buffer		
                    }

                    glDisable( GL_DEPTH_TEST );	
                    glDisable( GL_BLEND );
                    glDisable( GL_TEXTURE_2D );
                    //dibuja los ejes de transformacion
                    if (estado == translacion || estado == rotacion || estado == EditScale) {
                        //for (TInt o = 0; o < Collection.Count(); o++) {
                        for (size_t o = 0; o < Collection.size(); o++) {
                            bool found = false;
                            Object& obj = Objects[Collection[o]];
                            SearchSelectObj(obj, Collection[o], found);
                            if (found) break;  // Si ya encontro el objeto, salir del bucle
                        }
                    }

                    //Dibuja el origen de los objetos seleccionados		
                    if (showOrigins){	
                        //std::cout << "origen!" << std::endl;
                        glEnable( GL_TEXTURE_2D );
                        glEnable( GL_BLEND );
                        // Enable point sprites.
                        //glEnable( GL_POINT_SPRITE_OES );
                        glEnable( GL_POINT_SPRITE );
                        // Make the points bigger.
                        glPointSize( 16 );
                        //for (TInt o = 0; o < Collection.Count(); o++) {
                        for (size_t o = 0; o < Collection.size(); o++) {
                            Object& obj = Objects[o];
                            DibujarOrigen(obj, o);
                        }
                        //glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
                        //glDisable( GL_POINT_SPRITE_OES );
                        glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
                        glDisable( GL_POINT_SPRITE );
                    }		
                }

                //dibuja el cursor 3D	
                if (show3DCursor){
                    glDisable( GL_DEPTH_TEST );
                    glPushMatrix(); //guarda la matrix
                    glTranslatef( Cursor3DposX, Cursor3DposZ, Cursor3DposY);
                    
                    glEnable( GL_TEXTURE_2D );
                    glEnable( GL_BLEND );
                    //glEnable( GL_POINT_SPRITE_OES ); // Enable point sprites.	
                    glEnable( GL_POINT_SPRITE ); // Enable point sprites.	
                    glPointSize( 32 ); // Make the points bigger.
                    glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);
                    glVertexPointer( 3, GL_SHORT, 0, pointVertex );
                    glBindTexture( GL_TEXTURE_2D, Textures[2].iID);//iCursor3dTextura.iID ); //selecciona la textura

                    //glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE );
                    glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE );
                    glDrawArrays( GL_POINTS, 0, 1 );
                    //glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
                    glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);

                    //dibuja lineas		
                    glDisable( GL_TEXTURE_2D );
                    //glDisable( GL_POINT_SPRITE_OES );
                    glDisable( GL_POINT_SPRITE );
                    glDisable( GL_BLEND );
                
                    glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);
                    glVertexPointer( 3, GL_SHORT, 0, Cursor3DVertices );
                    glDrawElements( GL_LINES, Cursor3DEdgesSize, GL_UNSIGNED_SHORT, Cursor3DEdges );	

                    glPopMatrix(); //reinicia la matrix a donde se guardo	
                }
            }

            if (ShowUi){
                DibujarUI();
            }

            //termino de dibujar
            parentView.redibujar = false;
        }
};

std::vector<Viewport3D> Viewports3D;

void SetPerspectiva(int parentView){
    Viewports3D[parentView].orthographic = !Viewports3D[parentView].orthographic;
    Viewports[Viewports3D[parentView].Parent].redibujar = true;
}

int AddViewport3D(int parent) {
    Viewport3D view;
    view.Parent = parent;
    Viewports3D.push_back(view);
	return Viewports3D.size() -1;
}