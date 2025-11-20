//luz
//GLfloat light_pos[] = { -5000.0f, 5000.0f, 5000.0f, 1.0f };
//GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
//GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

//GLfloat objAmbient[4]  = { 0.3, 0.3, 0.3, 1.0 };

//GLfloat lightDiffuseLamp[4]   = { 0.9, 0.9, 0.9, 1.0 };
//GLfloat lightSpecularLamp[4]   = { 1.0, 1.0, 1.0, 1.0 };

//GLfloat sunLightPosition[4]  = {-50, 500, 500, 0 }; // y, z, x, si es direccional o puntual

GLfloat objAmbient[4]  = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat lightDiffuseLamp[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat lightSpecularLamp[4]   = { 0.3f, 0.3f, 0.3f, 1.0f };
GLfloat sunLightPosition[4]  = { -0.45f, 0.55f, 1.0f, 0.0f };

class Light : public Object {
    public:
		GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // blanco por defecto

        static Light* Create(Object* parent = nullptr, GLfloat x = 0, GLfloat y = 0, GLfloat z = 0);

        ObjectType getType() override{
            return ObjectType::light;
        }

        void RenderObject() override {
            //color si esta seleccionado
            if (ObjActivo == this){
                glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);
            }
            else if (select){
                glColor4f(ListaColores[accentDark][0],ListaColores[accentDark][1],ListaColores[accentDark][2],ListaColores[accentDark][3]);
            }
            else {		
                glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1], ListaColores[grisUI][2], ListaColores[grisUI][3]);		
            }

            glEnable(GL_DEPTH_TEST);
            glEnable( GL_TEXTURE_2D ); 
            glEnable( GL_BLEND );
            glDisable( GL_LIGHTING );
            glDepthMask(GL_FALSE); // Desactiva la escritura en el Z-buffer

            //GL_POINT_SPRITE_OES es para symbian
            //glEnable( GL_POINT_SPRITE_OES ); //activa el uso de sprites en los vertices
            glEnable(GL_POINT_SPRITE);                        // habilitar point sprites
            glPointSize( 32 ); //tamaño del punto
            glDisableClientState(GL_NORMAL_ARRAY); //no tiene normales. asi que crasharia
            
            glVertexPointer( 3, GL_SHORT, 0, pointVertex );
            glBindTexture( GL_TEXTURE_2D, Textures[4].iID ); //selecciona la textura

            //glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE );
            glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE); 

            glDrawArrays( GL_POINTS, 0, 1 );
            //glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
            glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE); 

            //glDisable( GL_POINT_SPRITE_OES );	
            glDisable( GL_POINT_SPRITE );	

            //glScalex( 0, 10, 0 ); //4500
            glDepthMask(GL_TRUE); // Reactiva la escritura en el Z-buffer     
            glDisable( GL_TEXTURE_2D ); 
            glDisable( GL_BLEND );
            glLineWidth(1);	
            //glScalef(0, FIXED_TO_FLOAT(scaleZ), 0 );
            LineaLightVertex[4] = (GLfloat)-posZ;
            glVertexPointer( 3, GL_FLOAT, 0, LineaLightVertex );
            glDrawElements( GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge );         
        }

    private:
        Light(Object* parent, GLfloat x, GLfloat y, GLfloat z)
        : Object(parent, "Light"){
            posX = x;
            posY = y;
            posZ = z;
            IconType = static_cast<size_t>(IconType::light);
        }
};

std::vector<Light*> Lights;

static const int MAX_LIGHTS = 8; // OpenGL fijo clásico

Light* Light::Create(Object* parent, GLfloat x, GLfloat y, GLfloat z) {
    if (Lights.size() >= MAX_LIGHTS) {
        printf("WARNING: Máximo de luces alcanzado.\n");
        return nullptr;
    }

    Light* l = new Light(parent, x, y, z);
    Lights.push_back(l);
    return l;
}