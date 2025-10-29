class Outliner {
    public:
        int Parent = -1;
        Object2D& Renglon;
        size_t CantidadRenglones = 5;

        Outliner(int parent, Object2D& renglon)
            : Parent(parent), Renglon(renglon) {}

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

            for (size_t i = 0; i < CantidadRenglones; i++) {
                glPushMatrix();                   
                glTranslatef(0, i * RenglonHeight, 0);
                // Renglón Seleccionado
                Rectangle* rect = static_cast<Rectangle*>(Renglon.data);
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
                RenderObject2D(Renglon);
                glPopMatrix();  
            }

            glEnableClientState(GL_TEXTURE_COORD_ARRAY);            
            glEnable(GL_TEXTURE_2D);
            glEnable( GL_BLEND );
            glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);

            glPushMatrix();          
            glTranslatef(margin, 0, 0);
            for (size_t i = 0; i < Collections.size(); i++) {
                glPushMatrix();                      
                glTranslatef(0, i * RenglonHeight, 0);       
                RenderObject2D(Collections[i]->Text);   
                glPopMatrix();  
            }
            glPopMatrix();  

			glBindTexture(GL_TEXTURE_2D, Textures[0].iID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    		glVertexPointer(2, GL_SHORT, 0, IconMesh); //todos los iconos comparten los vertices y tamaño
            glTranslatef(parentView.width-IconSize-margin, GlobalScale, 0);
            for (size_t i = 0; i < Collections.size(); i++) {
                glPushMatrix();                      
                glTranslatef(0, i * RenglonHeight, 0);
                glTexCoordPointer(2, GL_FLOAT, 0, IconsUV[static_cast<size_t>(IconType::visible)]->uvs);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glPopMatrix();  
            }
        }

        // Inicializar el Outliner
        /*static Outliner* InitOutliner(int parent) {
            // Crear un Object2D tipo Rectangle
            Object2D* newObj = AddObject2D(UI::Rectangle);

            // Configurar tamaño y color
            Rectangle* rect = static_cast<Rectangle*>(newObj->data);
            rect->SetSize(0, 0, Viewports[parent].width, RenglonHeight);
            rect->SetColor(255, 0, 0);
            rect->opacity = 255;
        }*/

        void OnResize(){
            Viewport& parentView = Viewports[Parent];
            Rectangle* rect = static_cast<Rectangle*>(Renglon.data);
            rect->SetSize(0, 0, parentView.width, RenglonHeight);

            // Calcular cuántos renglones entran en la altura
            CantidadRenglones = static_cast<int>(
                std::ceil(static_cast<float>(parentView.height) / static_cast<float>(RenglonHeight))
            );
        }
};

std::vector<Outliner*> Outliners;

int AddOutliner(int parent) {
    Object2D* rectObj = AddObject2D(UI::Rectangle);
    Outliner* view = new Outliner(parent, *rectObj);

    Outliners.push_back(view);    

    return static_cast<int>(Outliners.size() - 1);
}