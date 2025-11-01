class Outliner {
    public:
        int Parent = -1;
        int PosX = 0;
        int PosY = 0;
        int MaxPosX = 100;
        int MaxPosY = -100;
        Object2D* Renglon = nullptr;
        size_t CantidadRenglones = 5;
        GLshort borderMesh[32] = { 
            // fila 1 (y = 0)
            0,0,   6,0,   12,0,   18,0,
            // fila 2 (y = 6)
            0,6,   6,6,   12,6,   18,6,
            // fila 3 (y = 12)
            0,12,  6,12,  12,12,  18,12,
            // fila 4 (y = 18)
            0,18,  6,18,  12,18,  18,18
        };

        void OnResize(){
            Viewport& parentView = Viewports[Parent];
            Rectangle* rect = static_cast<Rectangle*>(Renglon->data);
            rect->SetSize(0, 0, parentView.width, RenglonHeightGS);

            // Calcular cuántos renglones entran en la altura
            CantidadRenglones = static_cast<int>(
                std::ceil(static_cast<float>(parentView.height) / static_cast<float>(RenglonHeightGS))
            );

            //recalcular malla 3d del borde
            ResizeBorder(borderMesh, parentView.width, parentView.height);
        }

        Outliner(int parent)
            : Parent(parent) {
            Renglon = AddObject2D(UI::Rectangle);
        }

        void Render(){
            //Configuracion inicial!
            Viewport& parentView = Viewports[Parent];

	        glViewport(parentView.x, parentView.y, parentView.width, parentView.height); // x, y, ancho, alto

            glDisable(GL_DEPTH_TEST); // Habilitar z-buffer
            glDisable(GL_LIGHTING);
            glDisable(GL_FOG);

            glClearColor(ListaColores[background][0],ListaColores[background][1],ListaColores[background][2],ListaColores[background][3]);

            // Limpiar pantalla
            glEnable(GL_SCISSOR_TEST);
            glScissor(parentView.x, parentView.y, parentView.width, parentView.height); // igual a tu viewport
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);

            // Guardar matrices
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, parentView.width, parentView.height, 0, -1, 1);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            glEnable(GL_TEXTURE_2D);
            glEnable( GL_BLEND );

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            glDisableClientState(GL_TEXTURE_COORD_ARRAY);    
            glEnableClientState(GL_VERTEX_ARRAY);        
            glDisable(GL_TEXTURE_2D);
            glDisable( GL_BLEND );

            glPushMatrix();   
            
            //Dibujado de los renglones!!!
            //no usa PosX porque el renglon tiene el ancho exacto de la ventana     
            glTranslatef(0, PosY + borderGS, 0);
            size_t RenglonesY = 0;
            for (size_t i = 0; i < CantidadRenglones; i++) {
                glPushMatrix();                   
                glTranslatef(0, RenglonesY, 0);
                RenglonesY += RenglonHeightGS;
                // Renglón Seleccionado
                Rectangle* rect = static_cast<Rectangle*>(Renglon->data);
                if (i % 2 == 0) {
                    rect->SetColor(ListaColoresUbyte[gris][0], ListaColoresUbyte[gris][1], ListaColoresUbyte[gris][2]);
                }
                else if (i % 2 == 0) {
                    // Renglón par
                    rect->SetColor(ListaColoresUbyte[gris][0], ListaColoresUbyte[gris][1], ListaColoresUbyte[gris][2]);
                } else {
                    // Renglón impar
                    rect->SetColor(ListaColoresUbyte[background][0], ListaColoresUbyte[background][1], ListaColoresUbyte[background][2]);
                }
                RenderObject2D(*Renglon);
                glPopMatrix();  
            }
            glPopMatrix();  

            //dibuja los iconos y los textos
			glBindTexture(GL_TEXTURE_2D, Textures[0].iID);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);  
            glEnable(GL_TEXTURE_2D);
            glEnable( GL_BLEND );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);      
            glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);

            RenglonesY = 0;  
            glPushMatrix();          
            glTranslatef(marginGS + PosX, PosY + borderGS, 0);            
            for (size_t c = 0; c < Collections.size(); c++) {
                glPushMatrix();                      
                glTranslatef(0, RenglonesY, 0); 
                
                //icono desplegar
                glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
                glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::arrow)]->uvs);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                //icono de la coleccion
                glTranslatef(IconSizeGS + gapGS, 0, 0);   
                glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::archive)]->uvs);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                //texto render                   
                glTranslatef(IconSizeGS + gapGS, 0, 0);   
                RenderObject2D(*Collections[c]->name);
                RenglonesY += RenglonHeightGS;

                for (size_t o = 0; o < Collections[c]->Objects.size(); o++) {
                    glPushMatrix();               
                    glTranslatef(-IconSizeGS - gapGS -IconSizeGS - gapGS, RenglonesY, 0);   
                    RenglonesY += RenglonHeightGS;

                    //linea
                    glVertexPointer(2, GL_SHORT, 0, IconLineMesh); //todos los iconos comparten los vertices y tamaño
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    //icono desplegar
                    glTranslatef(IconSizeGS + gapGS, 0, 0);   
                    glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::arrow)]->uvs);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    //icono del objeto
                    glTranslatef(IconSizeGS + gapGS, 0, 0);   
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[Collections[c]->Objects[o]->IconType]->uvs);
    		        glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    //texto
                    glTranslatef(IconSizeGS + gapGS, 0, 0);   
                    RenderObject2D(*Collections[c]->Objects[o]->name);  

                    glPopMatrix();   
                }

                glPopMatrix();  
            }
            glPopMatrix();  

    		glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
            RenglonesY = 0;

            glPushMatrix();   
            //no usa PosX porque los ojos siempre estan en la misma posicion en X. al borde
            glTranslatef(parentView.width - IconSizeGS - margin - borderGS, GlobalScale + PosY + borderGS, 0);
            RenglonesY = 0;
            for (size_t c = 0; c < Collections.size(); c++) {
                glPushMatrix();                      
                glTranslatef(0, RenglonesY, 0);
                RenglonesY += RenglonHeightGS;
                glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::visible)]->uvs);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                for (size_t o = 0; o < Collections[c]->Objects.size(); o++) {
                    glPushMatrix();               
                    glTranslatef(0, RenglonesY, 0);   
                    RenglonesY += RenglonHeightGS;
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::visible)]->uvs);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                    glPopMatrix();   
                }
                glPopMatrix();  
            }
            glPopMatrix();  

            DibujarBordes(borderMesh);
        }

        void button_left(){
		    std::cout << "outline: " << lastMouseX << std::endl;
        }

        void event_mouse_motion(){
            //boton del medio del mouse
            if (middleMouseDown) {
                ViewPortClickDown = true;

                PosX += dx;  
                PosY += dy;  

		        //std::cout << "nuevo PosX: " << PosX << " PosY: " << PosY << std::endl;
                if (PosX < 0){PosX = 0;}
                if (PosY > 0){PosY = 0;}
                //if (MaxPosX < PosX){PosX = MaxPosX;}
                //if (MaxPosY > PosY){PosY = MaxPosY;}
		        //std::cout << "ahora PosX: " << PosX << " PosY: " << PosY << std::endl;
            }        
        }

        void event_mouse_wheel(SDL_Event &e){

        }

        void event_key_down(SDL_Event &e){
            
        }
};

std::vector<Outliner*> Outliners;

int AddOutliner(int parent) {
    Outliner* view = new Outliner(parent);

    Outliners.push_back(view);    

    return static_cast<int>(Outliners.size() - 1);
}