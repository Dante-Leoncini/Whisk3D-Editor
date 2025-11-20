class Outliner : public ViewportBase, public WithBorder, public Scrollable  {
	public:
        size_t CantidadRenglones = 5;
        Object2D* Renglon = nullptr;

        Outliner(): ViewportBase() {
            Renglon = AddObject2D(UI::Rectangle);
        }
        
        void Resize(int newW, int newH) override {
            ViewportBase::Resize(newW, newH);
            ResizeBorder(newW, newH);

            Rectangle* rect = static_cast<Rectangle*>(Renglon->data);
            rect->SetSize(0, 0, width, RenglonHeightGS);

            // Calcular cuántos renglones entran en la altura
            CantidadRenglones = static_cast<int>(
                std::ceil(static_cast<float>(height) / static_cast<float>(RenglonHeightGS))
            );

            int MaxPosXtemp = 0;
            int MaxPosYtemp = 0;

            for (size_t c = 0; c < Objects.size(); c++) {   
                int rowWidth = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS; 
                MaxPosYtemp -= RenglonHeightGS;
                int textWidth = reinterpret_cast<Text*>(Objects[c]->name->data)->letters.size() * LetterWidthGS;
                rowWidth += textWidth + gapGS;

                // guardar ancho máximo
                if (rowWidth > MaxPosXtemp) MaxPosXtemp = rowWidth;

                //std::cout << "textWidth: " << textWidth << " rowWidth: " << rowWidth << std::endl;
                for (size_t o = 0; o < Objects[c]->Childrens.size(); o++) {
                    int rowWidthObj = marginGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + gapGS + IconSizeGS + marginGS;
                    MaxPosYtemp -= RenglonHeightGS;

                    // texto del objeto
                    int textWidthObj = reinterpret_cast<Text*>(Objects[c]->Childrens[o]->name->data)->letters.size() * LetterWidthGS;
                    rowWidthObj += textWidthObj + gapGS;

                    if (rowWidthObj > MaxPosXtemp) MaxPosXtemp = rowWidthObj;
                    //std::cout << "caracteres obj: " << rowWidthObj << std::endl;
                } 
            }
            //este es el gap para la barra de desplazamiento de abajo
            MaxPosYtemp -= marginGS;
            //std::cout << "MaxPosXtemp: " << MaxPosXtemp << " width: " << width << std::endl;
            //std::cout << "sizeX: " << sizeX << " MaxPosY: "<< MaxPosY << std::endl;
            //std::cout << "Ancho: " << newW << " Alto: "<< newH << std::endl;
            ResizeScrollbar(newW, newH, MaxPosXtemp, MaxPosYtemp);
        }

        void Render() override {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            // Limpiar pantalla
            glEnable(GL_SCISSOR_TEST);
            glScissor(x, y, width, height); // igual a tu viewport
            glClearColor(ListaColores[background][0],ListaColores[background][1],ListaColores[background][2],ListaColores[background][3]);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);

	        glViewport(x, y, width, height); // x, y, ancho, alto
            glOrtho(0, width, height, 0, -1, 1);

            glDisable(GL_FOG);
            glDisable(GL_DEPTH_TEST);
            glDisable( GL_CULL_FACE );
            glDisable( GL_LIGHTING );
            glDisable( GL_TEXTURE_2D );
            glDisable( GL_BLEND );
            glEnable(GL_COLOR_MATERIAL);

            glEnableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_NORMAL_ARRAY);

            //de aca en adelante es como antes
            glPushMatrix();        
            size_t RenglonesY = 0;
            glTranslatef(0, PosY + borderGS, 0);
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

            glBindTexture(GL_TEXTURE_2D, Textures[0].iID);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);  
            glEnable(GL_TEXTURE_2D);
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);      
            glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1],
                    ListaColores[grisUI][2], ListaColores[grisUI][3]);

            //esto es para recortar y que no se ponga el texto encima de los ojos de la derecha
            glEnable(GL_SCISSOR_TEST);
            if (scrollX){
                glScissor(x, y+ marginGS, width - IconSizeGS - marginGS - borderGS -gapGS, height - marginGS); // igual a tu viewport - los ojos
            }
            else {
                glScissor(x, y, width - IconSizeGS - marginGS - borderGS -gapGS, height); // igual a tu viewport - los ojos
            }

            RenglonesY = 0;  
            glPushMatrix();          
            glTranslatef(marginGS + PosX, PosY + borderGS, 0);            
            for (size_t c = 0; c < Objects.size(); c++){
                if (Objects[c] == ObjActivo){
                    //std::cout << "Objeto activo en el outliner: " << reinterpret_cast<Text*>(Objects[c]->name->data)->value << "\n";
                    glColor4f(ListaColores[accent][0], ListaColores[accent][1],
                            ListaColores[accent][2], ListaColores[accent][3]);
                }
                else if (Objects[c]->select){
                    glColor4f(ListaColores[accentDark][0], ListaColores[accentDark][1],
                            ListaColores[accentDark][2], ListaColores[accentDark][3]);
                }
                else {
                    glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1],
                            ListaColores[grisUI][2], ListaColores[grisUI][3]);
                }
                
                //icono desplegar
                glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
                //si no tiene hijos. no hagas la flecha
                if (Objects[c]->Childrens.size() > 0){
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::arrow)]->uvs);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }

                //icono de la coleccion
                glTranslatef(IconSizeGS + gapGS, 0, 0);   
                glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::archive)]->uvs);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                //texto render                   
                glTranslatef(IconSizeGS + gapGS, 0, 0);   
                RenderObject2D(*Objects[c]->name, false);

                glTranslatef(gapGS + IconSizeGS, 0, 0); 
                for (size_t o = 0; o < Objects[c]->Childrens.size(); o++) {
                    glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1],
                            ListaColores[grisUI][2], ListaColores[grisUI][3]);
                    
                    glTranslatef(
                        -IconSizeGS - gapGS -IconSizeGS - gapGS -IconSizeGS - gapGS, 
                        RenglonHeightGS, 
                        0
                    );   

                    //linea
                    glVertexPointer(2, GL_SHORT, 0, IconLineMesh); //todos los iconos comparten los vertices y tamaño
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::line)]->uvs);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    if (Objects[c]->Childrens[o] == ObjActivo){
                        //std::cout << "Objeto activo en el outliner: " << reinterpret_cast<Text*>(Objects[c]->Childrens[o]->name->data)->value << "\n";
                        glColor4f(ListaColores[accent][0], ListaColores[accent][1],
                                ListaColores[accent][2], ListaColores[accent][3]);
                    }
                    else if (Objects[c]->select){
                        glColor4f(ListaColores[accentDark][0], ListaColores[accentDark][1],
                                ListaColores[accentDark][2], ListaColores[accentDark][3]);
                    }
                    else {
                        glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1],
                                ListaColores[grisUI][2], ListaColores[grisUI][3]);
                    }

                    //icono desplegar
                    glTranslatef(IconSizeGS + gapGS, 0, 0);   
                    glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::arrowRight)]->uvs);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    //icono del objeto
                    glTranslatef(IconSizeGS + gapGS, 0, 0);   
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[Objects[c]->Childrens[o]->IconType]->uvs);
    		        glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    //texto
                    glTranslatef(IconSizeGS + gapGS, 0, 0);   
                    RenderObject2D(*Objects[c]->Childrens[o]->name, false);  

                    //glPopMatrix();   
                }           
                glTranslatef(-IconSizeGS - gapGS -IconSizeGS - gapGS -IconSizeGS - gapGS, RenglonHeightGS, 0);    
            }
            glPopMatrix();  

    		glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
            glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1], ListaColores[grisUI][2], ListaColores[grisUI][3]);
            RenglonesY = 0;

            glPushMatrix();   
            //no usa PosX porque los ojos siempre estan en la misma posicion en X. al borde
            glTranslatef(width - IconSizeGS - marginGS - borderGS, GlobalScale + PosY + borderGS, 0);
            
            if (scrollX){
                glScissor(x, y+ marginGS, width - marginGS - borderGS, height - marginGS); // igual a tu viewport - los ojos
            }
            else {
                glScissor(x, y, width - marginGS - borderGS, height); // igual a tu viewport - los ojos
            }
            
            for (size_t c = 0; c < Objects.size(); c++) {       
                glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::visible)]->uvs);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                for (size_t o = 0; o < Objects[c]->Childrens.size(); o++) {    
                    glTranslatef(0, RenglonHeightGS, 0);    
                    glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::visible)]->uvs);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
                glTranslatef(0, RenglonHeightGS, 0); 
            }
            glPopMatrix();     
            glDisable(GL_SCISSOR_TEST);

            DibujarBordes(this);
            DibujarScrollbar(this);
        }

        void button_left() override {
            if (mouseOverScrollY){
                mouseOverScrollYpress = true;
            }
        }

        void mouse_button_up(SDL_Event &e) override {
            if (e.button.button == SDL_BUTTON_LEFT) {  
                mouseOverScrollYpress = false;
                mouseOverScrollXpress = false;
            }
            //else if (e.button.button == SDL_BUTTON_MIDDLE) {
            //    middleMouseDown = false;
            //}
            FindMouseOver(lastMouseX,lastMouseY);
        }

        void event_mouse_wheel(SDL_Event &e) override {
            MouseWheel = true;
            ScrollY(e.wheel.y*6*GlobalScale);
            MouseWheel = false;
        }

        void FindMouseOver(int mx, int my){
            ScrollMouseOver(this, mx, my);
        }

        void event_mouse_motion(int mx, int my) override {
            if (middleMouseDown || leftMouseDown) {
                ViewPortClickDown = true;

                ScrollX(dx);
                ScrollY(dy);
                return;
            }
            //si no se esta haciendo click. entonces miras si el mouse esta encima de algo
            else if (scrollY){
                FindMouseOver(mx, my);
            }
        }

        void event_key_down(SDL_Event &e) override {
        }

        void key_down_return(){
        }
};