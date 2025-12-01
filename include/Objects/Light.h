class Light : public Object {
    public:
        GLenum LightID = GL_LIGHT0;
        GLfloat position[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };
        GLfloat ambient[4]  = { 0.0f, 0.0f, 0.0f, 1.0f };
        GLfloat diffuse[4]   = { 1.0f, 1.0f, 1.0f, 1.0f }; // blanco por defecto
        GLfloat specular[4]   = { 0.0f, 0.0f, 0.0f, 1.0f };

        static Light* Create(Object* parent = nullptr, GLfloat x = 0, GLfloat y = 0, GLfloat z = 0);

        ObjectType getType() override{
            return ObjectType::light;
        }

        void SetDiffuse(GLfloat r = 1.0f, GLfloat g = 1.0f, GLfloat b = 1.0f){
            diffuse[0] = r;
            diffuse[1] = g;
            diffuse[2] = b;
        }

        void SetLightID(GLenum ID){
            LightID = ID;
        }

        void RenderObject() override {
            //como afecta a la luz en OpenGl. solo tiene efecto si estamos en modo render
            if (view == RenderType::Rendered){
                glEnable(LightID);
                glLightfv(LightID, GL_POSITION, position);
                glLightfv(LightID, GL_DIFFUSE,  diffuse);
                glLightfv(LightID, GL_AMBIENT,  ambient);
                glLightfv(LightID, GL_SPECULAR, specular);

                // ACTIVAR ATENUACIÓN:
                glLightf(LightID, GL_CONSTANT_ATTENUATION, 0.5f);
                glLightf(LightID, GL_LINEAR_ATTENUATION,   0.1f);
                glLightf(LightID, GL_QUADRATIC_ATTENUATION, 0.02f);
            }

            //si no esta el overlay. no dibuja la representacion de la luz en el espacio 3d
            if (!showOverlayGlobal) return;

            //color si esta seleccionado
            if (ObjActivo == this && select){
                glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);
            }
            else if (select){
                glColor4f(ListaColores[accentDark][0],ListaColores[accentDark][1],ListaColores[accentDark][2],ListaColores[accentDark][3]);
            }
            else {		
                glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1], ListaColores[grisUI][2], ListaColores[grisUI][3]);		
            }

            glDisable( GL_LIGHTING );

            glDisable( GL_TEXTURE_2D ); 
            glDisable( GL_BLEND );
            glLineWidth(1);	

            //esto calcula el largo de la linea
            GLfloat totalZ = posZ;
            GLfloat rX = rotX, rY = rotY, rZ = rotZ;

            Object* p = Parent;       // empezamos desde el parent inmediato
            while (p != nullptr) {
                totalZ += p->posZ;   // posición global acumulada
                rX += p->rotX;       // rotación global acumulada
                rY += p->rotY;
                rZ += p->rotZ;

                p = p->Parent;       // << avanzar recién al final
            }
            LineaLightVertex[4] = -totalZ;   // aplicamos el negativo al final

            // Invertir rotaciones para que la luz no se incline con el objeto
		    glPushMatrix();    
            glRotatef(-rX, 1, 0, 0);
            glRotatef(-rZ, 0, 1, 0);
            glRotatef(-rY, 0, 0, 1);

            glVertexPointer( 3, GL_FLOAT, 0, LineaLightVertex );
            glDrawElements( GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge );    
    	    glPopMatrix();     
        }

		~Light() {
			delete name;
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