GLfloat LastRotX = 0;
GLfloat LastRotY = 0;	
GLfloat LastPivotX = 0;
GLfloat LastPivotY = 0;
GLfloat LastPivotZ = 0;

std::vector<int> Objs2Dviewport3D;

class Viewport3D {
	public:
		int Parent = -1;
        bool orthographic = false;
        bool SimularZBuffer = false;
        bool ViewFromCameraActive = false;
        float nearClip = 10.0f;
        float farClip = 2000.0f;
        int cameraDistance = 270;
        float aspect = 1;
        GLfloat posX = 0;
        GLfloat posY = 0;
        GLfloat posZ = 0.0f;
        GLfloat rotX = 113.5;
        GLfloat rotY = 20.0;
        GLfloat PivotX = 0;
        GLfloat PivotY = 0;
        GLfloat PivotZ = 0;

        void OnResize(){
            Viewport& parentView = Viewports[Parent];
            aspect = (float)parentView.width / (float)parentView.height;
        }

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
                glOrtho(-size * aspect, size * aspect,
                        -size, size,
                        nearClip, farClip);
            }
            else {
                gluPerspective(fovDeg, aspect, nearClip, farClip);
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
            glEnable(GL_SCISSOR_TEST);
            glScissor(parentView.x, parentView.y, parentView.width, parentView.height); // igual a tu viewport
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);

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
                    Object& obj = *Objects[o];
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
                for (size_t o = 0; o < Collections.size(); o++) {
                    Object& obj = *Objects[Collections[o]->ObjID];
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
                    for (size_t o = 0; o < Collections.size(); o++) {
                        RenderObjectAndChildrens(Collections[o]->ObjID);
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
                        for (size_t o = 0; o < Collections.size(); o++) {
                            Object& obj = *Objects[Collections[o]->ObjID];
                            //if (obj.Childrens.Count() > 0){
                            if (obj.Childrens.size() > 0){
                                RenderLinkLines(Collections[o]->ObjID);
                            }
                        }
                        glDepthMask(GL_TRUE); // Reactiva la escritura en el Z-buffer		
                    }

                    glDisable( GL_DEPTH_TEST );	
                    glDisable( GL_BLEND );
                    glDisable( GL_TEXTURE_2D );
                    //dibuja los ejes de transformacion
                    if (estado == translacion || estado == rotacion || estado == EditScale) {
                        for (size_t o = 0; o < Collections.size(); o++) {
                            bool found = false;
                            Object& obj = *Objects[Collections[o]->ObjID];
                            SearchSelectObj(obj, Collections[o]->ObjID, found);
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
                        for (size_t o = 0; o < Collections.size(); o++) {
                            Object& obj = *Objects[o];
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
                // Guardar matrices
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(0, parentView.width, parentView.height, 0, -1, 1);

                glDisable(GL_DEPTH_TEST);
                glDisable(GL_LIGHTING);
                glEnable(GL_TEXTURE_2D);
                glEnable( GL_BLEND );

                glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);

                //int texW, texH;
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                /*glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
                std::cout << "Tex size in GPU: " << texW << "x" << texH << std::endl;*/

                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();

                //std::string text = "ABCDEFGHIJKLMNÑOPQRSTUVWXYZ abcdefghijklmnñopqrstuvwyz 0123456789+-= ()[]{}<>/*:#%!?.,'\"@&$";

                /*for (size_t i = 0; i < Objs2Dviewport3D.size(); i++) {
                    int idx = Objs2Dviewport3D[i];        // índice
                    RenderObject2D(*Objects2D[idx]);      // desreferenciamos el puntero
                }*/
            }

            //termino de dibujar
            parentView.redibujar = false;
        }

        void EnfocarObject(){
            SetTransformPivotPoint();	
            PivotX = 0.0f; 
            PivotY = 0.0f;
            PivotZ = 0.0f;
            PivotX = PivotX-TransformPivotPointFloat[0]; 
            PivotY = PivotY-TransformPivotPointFloat[1];
            PivotZ = PivotZ-TransformPivotPointFloat[2];
        }

        void RecalcViewPos(){
            Object& obj = *Objects[CameraActive];
            rotX = -obj.rotZ+90;
            rotY = -obj.rotY;	
            PivotX = -obj.posX;
            PivotY = -obj.posY;
            PivotZ = -obj.posZ;

            //en caso de que este emparentado
            /*TInt ParentID = obj.Parent;
            while (ParentID  > -1) {		
                Object& parentObj = Objects[ParentID];		
                rotX = -parentObj.rotZ;
                rotY = -parentObj.rotY;	
                //PivotX = -parentObj.posX;
                //PivotY = -parentObj.posY;
                //PivotZ = -parentObj.posZ;	
                ParentID = parentObj.Parent;		
            }*/
        }

        //mira si no hay camara activa
        //si no hay una camara activa. busca una camara para asignarla
        //si no hay camaras... quedara en -1
        void CheckCameraState(){
            if (CameraActive < 0){
                //for(TInt i=0; i < Objects.Count(); i++){
                for(size_t i=0; i < Objects.size(); i++){
                    if (Objects[i]->type == camera){
                        CameraActive = i;
                        return;
                    }		
                }
            }	
        }

        void SetViewpoint(int opcion){
            switch (opcion) {
                case top:
                    rotX = -180.0;
                    rotY = 90.0;
                    ViewFromCameraActive = false;	
                    CameraToView = false;
                    break;
                case front:
                    rotX = -180.0;
                    rotY = 0.0;	
                    ViewFromCameraActive = false;	
                    CameraToView = false;
                    break;
                case right:
                    rotX = 90.0;
                    rotY = 0.0;		
                    ViewFromCameraActive = false;	
                    CameraToView = false;
                    break;
                case cameraView:
                    CheckCameraState();
                    if (CameraActive < 0){
                        //_LIT(KFormatString, "There are no cameras!");
                        //HBufC* noteBuf = HBufC::NewLC(50);
                        //noteBuf->Des().Format(KFormatString);
                        //MensajeError(noteBuf);  
                        //CleanupStack::PopAndDestroy(noteBuf);
                    }
                    //else if (Objects.Count() > CameraActive && !ViewFromCameraActive){	
                    else if (Objects.size() > static_cast<size_t>(CameraActive) && !ViewFromCameraActive){	
                        LastRotX = rotX;
                        LastRotY = rotY;	
                        LastPivotX = PivotX;
                        LastPivotY = PivotY;
                        LastPivotZ = PivotZ;
                        RecalcViewPos();
                        ViewFromCameraActive = true;
                    }
                    break;
            }
        }

        void RestaurarViewport(){
            ViewFromCameraActive = false;
            rotX = LastRotX;
            rotY = LastRotY;	
            PivotX = LastPivotX;
            PivotY = LastPivotY;
            PivotZ = LastPivotZ;
        }

        void ChangePerspective(){
            orthographic = !orthographic;
        }

        //coloca el cursor 3d desde la vista 3d
        void SetCursor3D(){// 1) Calcular base de la cámara (forward/right/up)
            /*float pitch = rotX * DEG2RAD;
            float yaw = rotY * DEG2RAD;

            Vec3 forward(cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));
            forward = Normalize(forward);

            Vec3 worldUp(0, 1, 0);
            Vec3 right = Cross(forward, worldUp);
            float rlen = Len(right);
            if (rlen < 1e-8f) {
                right = Vec3(1, 0, 0); // Evitar degeneración en pitch ±90°
            } else {
                right = right * (1.0f / rlen);
            }

            Vec3 up = Cross(right, forward); // Unitario por construcción

            // 2) Posición de la cámara
            Vec3 pivotPos(PivotX + posX, PivotY + posY, PivotZ + posZ);
            Vec3 camPos = pivotPos - forward * cameraDistance;

            // 3) Mouse a NDC
            float ndcX = (2.0f * (float)lastMouseX / (float)winW) - 1.0f;
            float ndcY = 1.0f - (2.0f * (float)lastMouseY / (float)winH);

            // 4) Calcular dirección del rayo en el espacio de la cámara
            float halfFovRad = fovDeg * DEG2RAD * 0.5f;
            float halfH = tanf(halfFovRad);
            float halfW = aspect * halfH;

            // Rayo en el espacio de la cámara (sin normalizar)
            Vec3 rayDir = forward + right * (ndcX * halfW) + up * (ndcY * halfH);

            // 5) Intersección con un plano perpendicular al forward, pasando por el pivot
            // Plano: punto = pivotPos, normal = forward
            // Rayo: origen = camPos, dirección = rayDir
            // Ecuación: dot((camPos + t * rayDir - pivotPos), forward) = 0
            float denom = Dot(rayDir, forward);
            if (fabs(denom) < 1e-8f) {
                // Rayo paralelo al plano, usar posición por defecto
                Cursor3DposX = pivotPos.x;
                Cursor3DposY = pivotPos.y;
                Cursor3DposZ = pivotPos.z;
                return;
            }

            float t = Dot(pivotPos - camPos, forward) / denom;
            if (t < 0) {
                // Intersección detrás de la cámara, usar posición por defecto
                Cursor3DposX = pivotPos.x;
                Cursor3DposY = pivotPos.y;
                Cursor3DposZ = pivotPos.z;
                return;
            }

            Vec3 cursorPos = camPos + rayDir * t;

            Cursor3DposX = cursorPos.x;
            Cursor3DposY = cursorPos.y;
            Cursor3DposZ = cursorPos.z;*/
        }

        void Aceptar(){	
            // Mostrar el cursor
            SDL_ShowCursor();
            //si no hay objetos
            if (Objects.size() < 1){return;}

            if ( InteractionMode == ObjectMode ){
                if (estado != editNavegacion){
                    estado = editNavegacion;
                }
            }
            ReloadViewport(true);
        };

        void button_left(){
            if (estado == translacion || estado == EditScale || estado == rotacion){
				Aceptar();
			}
			else {
            	GuardarMousePos();
				SetCursor3D();
			}
        }

        void event_mouse_wheel(SDL_Event &e){
            posY+= e.wheel.y*20;
        }

        void event_mouse_motion(){
            //boton del medio del mouse
            if (middleMouseDown) {
                ViewPortClickDown = true;
                // Chequear si Shift está presionado
                const bool* state = SDL_GetKeyboardState(NULL);
                bool shiftHeld = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];

                if (shiftHeld) {
                    float radY = rotY * M_PI / 180.0f; // Yaw
                    float radX = rotX * M_PI / 180.0f; // Pitch

                    float factor = 8.0f;

                    float cosX = cos(radX);
                    float sinX = sin(radX);
                    float cosY = cos(radY);
                    float sinY = sin(radY);

                    PivotZ -= dy * factor * cosY;
                    PivotX += dx * factor * cosX - dy * factor * sinY * sinX;
                    PivotY += dx * factor * sinX + dy * factor * sinY * cosX;
                    LShiftPressed = false;
                } 
                else {
                    // 🚀 ROTAR cámara
                    rotX += dx * 0.2f;  
                    rotY += dy * 0.2f;  

                    // Limitar rotY para evitar giros extremos
                    if(rotY > 180.0f) rotY -= 360.0f;
                    if(rotY < -180.0f) rotY += 360.0f;
                    if(rotX > 180.0f) rotX -= 360.0f;
                    if(rotX < -180.0f) rotX += 360.0f;
                }
            }
            else if (estado == translacion || estado == rotacion || estado == EditScale){
                // Ocultar el cursor
                //SDL_HideCursor();
                if (viewPortActive > -1){
                    switch (estado) {
                        case translacion:
                            SetTranslacionObjetos(dx, dy, 16.0f);
                            break;
                        case rotacion:
                            SetRotacion(dx, dy);
                            break;
                        case EditScale:
                            SetScale(dx, dy);
                            break;
                        default:
                            // por si no coincide con nada
                            break;
                    }	
                }
            }
        }

        void TeclaDerecha(){
            //mueve el mouse
            /*if (mouseVisible){
                mouseX++;
                if (mouseX > iScreenWidth-11){mouseX = iScreenWidth-11;};
            }*/

            //rotX -= fixedMul( 1, aDeltaTimeSecs );
            if (estado == editNavegacion){				
                if (navegacionMode == Orbit){
                    if (ViewFromCameraActive && CameraToView){
                        Object& obj = *Objects[CameraActive];
                        // Convertir el angulo de rotX a radianes
                        GLfloat radRotX = obj.rotX * M_PI / 180.0;

                        obj.posX-= 30 * cos(radRotX);
                        obj.posY+= 30 * sin(radRotX);
                    }
                    else {
                        if (ViewFromCameraActive){
                            RestaurarViewport();
                        }
                        rotX+= 0.5;	
                    }		
                }
                else if (navegacionMode == Fly){
                    // Convertir el angulo de rotX a radianes
                    GLfloat radRotX = rotX * M_PI / 180.0;

                    // Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
                    GLfloat leftX = cos(radRotX);
                    GLfloat leftY = sin(radRotX);

                    // Mover hacia la izquierda
                    PivotX -= 30 * leftX;
                    PivotY -= 30 * leftY;
                }
            }
            else if (estado == translacion){
                SetTranslacionObjetos(30, 0);		
            }
            else if (estado == rotacion){
                SetRotacion(-1, 0);
            }
            else if (estado == EditScale){
                SetScale(2,0);	
            }
            else if (estado == timelineMove){
                CurrentFrame++;
                if (!PlayAnimation){
                    ReloadAnimation();
                }
            }
            ReloadViewport(true);
        }

        void TeclaIzquierda(){
            //mueve el mouse
            if (mouseVisible){
                mouseX--;
                if (mouseX < 0){mouseX = 0;};
            }

            //rotX += fixedMul( 0.1, aDeltaTimeSecs );
            if (estado == editNavegacion){ 
                if (navegacionMode == Orbit){
                    if (ViewFromCameraActive && CameraToView){
                        Object& obj = *Objects[CameraActive];
                        // Convertir el angulo de rotX a radianes
                        GLfloat radRotX = obj.rotX * M_PI / 180.0;

                        obj.posX+= 30 * cos(radRotX);
                        obj.posY-= 30 * sin(radRotX);
                    }
                    else {
                        if (ViewFromCameraActive){
                            RestaurarViewport();
                        }
                        rotX-= 0.5;
                    }
                }
                else if (navegacionMode == Fly){
                    // Convertir el angulo de rotX a radianes
                    GLfloat radRotX = rotX * M_PI / 180.0;

                    // Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
                    GLfloat leftX = cos(radRotX);
                    GLfloat leftY = sin(radRotX);

                    // Mover hacia la izquierda
                    PivotX += 30 * leftX;
                    PivotY += 30 * leftY;
                }	
            }
            else if (estado == translacion){	
                SetTranslacionObjetos(-30, 0);		
            }
            else if (estado == rotacion){
                SetRotacion(1, 0);
            }
            else if (estado == EditScale){
                SetScale(-2,0);
            }
            else if (estado == timelineMove){
                CurrentFrame--;
                if (CurrentFrame < StartFrame){
                    StartFrame = EndFrame;
                }
                if (!PlayAnimation){
                    ReloadAnimation();
                }
            }
            ReloadViewport(true);
        }

        void TeclaArriba(){
            //mueve el mouse
            if (mouseVisible){
                mouseY--;
                if (mouseY < 0){mouseY = 0;};
            }

            if (estado == editNavegacion){	
                if (navegacionMode == Orbit){
                    if (ViewFromCameraActive && CameraToView){
                        Object& obj = *Objects[CameraActive];
                        // Convertir el angulo de rotX a radianes
                        GLfloat radRotX = obj.rotX * M_PI / 180.0;
                        GLfloat radRotY = obj.rotY * M_PI / 180.0;
                        //GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

                        obj.posX+= 30 * sin(radRotX);
                        //obj.posY-= 30 * cos(radRotX);
                        obj.posZ+= 30 * cos(radRotY);
                    }
                    else {
                        if (ViewFromCameraActive){
                            RestaurarViewport();
                        }
                        rotY-= 0.5;	
                    }			
                }
                else if (navegacionMode == Fly){
                    // Convertir el angulo de rotX a radianes
                    GLfloat radRotX = rotX * M_PI / 180.0;

                    PivotY+= 30 * cos(radRotX);
                    PivotX-= 30 * sin(radRotX);
                }		
            }
            else if (estado == EditScale){
                SetScale(2,0);	
            }
            else if (estado == translacion){
                SetTranslacionObjetos(0, -30);
            }
            ReloadViewport(true);
        }

        void TeclaAbajo(){
            //mueve el mouse
            /*if (mouseVisible){
                mouseY++;
                if (mouseY > iScreenHeight-17){mouseY = iScreenHeight-17;};
            }*/

            if (estado == editNavegacion){ 			
                if (navegacionMode == Orbit){
                    if (ViewFromCameraActive && CameraToView){
                        Object& obj = *Objects[CameraActive];
                        // Convertir el angulo de rotX a radianes
                        GLfloat radRotX = obj.rotX * M_PI / 180.0;
                        GLfloat radRotY = obj.rotY * M_PI / 180.0;
                        //GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

                        obj.posX-= 30 * sin(radRotX);
                        //obj.posY-= 30 * cos(radRotX);
                        obj.posZ-= 30 * cos(radRotY);
                    }
                    else {
                        if (ViewFromCameraActive){
                            RestaurarViewport();
                        }
                        rotY+= 0.5;	
                    }		
                }
                else if (navegacionMode == Fly){
                    // Convertir el angulo de rotX a radianes
                    GLfloat radRotX = rotX * M_PI / 180.0;

                    PivotY-= 30 * cos(radRotX);
                    PivotX+= 30 * sin(radRotX);
                }
            }
            else if (estado == EditScale){
                SetScale(-2,0);	
            }
            else if (estado == translacion){
                SetTranslacionObjetos(0, 30);		
            }
            ReloadViewport(true);
        }

        void ClickA(){
            posX+= 1.0;
        }

        // Método para actualizar cache
        void UpdatePrecalculos() {
            if (viewPortActive > -1){
                precalculado.radY = rotY * M_PI / 180.0f;
                precalculado.radX = rotX * M_PI / 180.0f;

                precalculado.cosX = cos(precalculado.radX);
                precalculado.sinX = sin(precalculado.radX);
                precalculado.cosY = cos(precalculado.radY);
                precalculado.sinY = sin(precalculado.radY);
            }
        }

        void ClickD(){
            if (LAltPressed){
                UpdatePrecalculos();
                DuplicatedLinked();
            }
            else if (LShiftPressed){
                UpdatePrecalculos();
                DuplicatedObject();
            }
        }

        void ClickE(){
            posZ-= 1.0;
        }

        void ClickQ(){
            posZ+= 1.0;  
        }

        void SetEje(int eje){
            if (estado != editNavegacion){
                axisSelect = eje;
            }	
        };

        void event_key_down(SDL_Event &e){
            SDL_Keycode key = e.key.key; // SDL3
            if (e.key.repeat == 0) { 
                switch (key) {
                    case SDLK_LSHIFT:
                        LShiftPressed = true;
                        break;
                    case SDLK_LALT:
                        LAltPressed = true;
                        break;
                    case SDLK_RETURN:  // Enter
                        key_down_return();
                        break;
                    case SDLK_RIGHT:   // Flecha derecha
                        TeclaDerecha();
                        break;
                    case SDLK_LEFT:    // Flecha izquierda
                        TeclaIzquierda();
                        break;
                    case SDLK_UP:  
                        TeclaArriba();
                        break;
                    case SDLK_DOWN:  
                        TeclaAbajo();
                        break;	
                    case SDLK_A:  
                        ClickA();
                        break;
                    case SDLK_D:
                        ClickD();
                        break;
                    case SDLK_X:   
                        if (estado != editNavegacion){
                            if (axisSelect != X){
                                SetEje(X);
                            }
                            else {
                                SetEje(ViewAxis);
                            }
                        } 
                        else {
                            Borrar();
                        }
                        break;
                    case SDLK_Y:   
                        if (estado != editNavegacion){
                            if (axisSelect != Y){
                                SetEje(Y);
                            }
                            else {
                                SetEje(ViewAxis);
                            }
                        }
                        break;
                    case SDLK_Z:   
                        if (estado != editNavegacion){
                            if (axisSelect != Z){
                                SetEje(Z);
                            }
                            else {
                                SetEje(ViewAxis);
                            }
                        }
                        break;
                    case SDLK_R:    
                        SetRotacion();
                        break;
                    case SDLK_G:  
                        // Para activar el cursor de mover/arrastrar
                        UpdatePrecalculos();
                        SetPosicion();
                        break;			
                    case SDLK_S:   
                        SetEscala();
                        break;
                    // Numpad
                    case SDLK_KP_1: SetViewpoint(front); break;
                    //case SDLK_KP_2: numpad('2'); break;
                    case SDLK_KP_3: SetViewpoint(right); break;
                    case SDLK_KP_5: {
                        ChangePerspective(); 
                        break;
                    };
                    case SDLK_KP_7: SetViewpoint(top); break;
                    case SDLK_KP_8: BuscarVertexAnimation(); break;
                    case SDLK_KP_9: abrir(); break;
                    //case SDLK_KP_0: numpad('0'); break;
                    case SDLK_KP_PERIOD: {
                        if (Objects.size() > 0){
                            EnfocarObject(); 
                        }
                        break;
                    }
                    // si querés, agregá más teclas aquí
                    case SDLK_ESCAPE:  // Esc                    
                        Cancelar();
                        break;
                }
            }
            else {
                // Evento repetido por mantener apretada
                switch (key) {
                    case SDLK_RETURN:  // Enter
                        key_down_return();
                        break;
                    case SDLK_RIGHT:   // Flecha derecha
                        TeclaDerecha();
                        break;
                    case SDLK_LEFT:    // Flecha izquierda
                        TeclaIzquierda();
                        break;
                    case SDLK_UP:   // Flecha derecha
                        TeclaArriba();
                        break;
                    case SDLK_DOWN:    // Flecha izquierda
                        TeclaAbajo();
                        break;
                    case SDLK_A:   // Flecha derecha
                        ClickA();
                        break;
                    case SDLK_E:   // Flecha derecha
                        ClickE();
                        break;
                    case SDLK_Q:    // Flecha izquierda
                        ClickQ();
                        break;
                    // Numpad
                    case SDLK_KP_1: {
                        if (viewPortActive > -1){
                            SetViewpoint(front);
                        }
                        break;
                    }
                    //case SDLK_KP_2: numpad('2'); break;
                    case SDLK_KP_3: {
                        if (viewPortActive > -1){
                            SetViewpoint(right);
                        } 
                        break;
                    }
                    case SDLK_KP_7: {
                        if (viewPortActive > -1){
                            SetViewpoint(top);
                        }
                        break;
                    }
                    case SDLK_KP_8: BuscarVertexAnimation(); break;
                    case SDLK_KP_9: abrir();break;
                    //case SDLK_KP_0: numpad('0'); break;
                    case SDLK_KP_PERIOD: {
                        if (viewPortActive > -1 && Objects.size() > 0){
                            EnfocarObject();
                        }
                        break;
                    }
                    // si querés, agregá más teclas aquí
                    case SDLK_ESCAPE:  // Esc
                        Cancelar();
                        break;
                }
            }
        }

        void key_down_return(){
            Aceptar();
        }
};

std::vector<Viewport3D> Viewports3D;

void DesactivarCamaraActiva(){
    for (size_t o = 0; o < Viewports3D.size(); o++) {
		Viewports3D[o].ViewFromCameraActive = false;	
    }
}

int AddViewport3D(int parent) {
    Viewport3D view;
    view.Parent = parent;
    Viewports3D.push_back(view);
	return Viewports3D.size() -1;
}