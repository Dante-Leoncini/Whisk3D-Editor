static const int CameraVertexSize = 8 * 3;
static const int CameraEdgesSize = 11 * 2;

//static GLshort CameraVertices[CameraVertexSize] = {    
static GLfloat CameraVertices[CameraVertexSize] = {    
    0, 0, 0, //origen
    0.35f*2,  0.144f*2,  0.192f*2,
    0.35f*2, -0.144f*2,  0.192f*2,
    0.35f*2, -0.144f*2, -0.192f*2,
    0.35f*2,  0.144f*2, -0.192f*2,
    //tiangulo
    0.35f*2,  0.17f*2,  0.128f*2,
    0.35f*2,  0.17f*2, -0.128f*2,
    0.35f*2,  0.31f*2,  0,
};

static const GLshort CameraFaceActive[3]={
    5,6,7
};

static const GLushort CameraEdges[CameraEdgesSize]={
    //cono
	0, 1,
	0, 2,
	0, 3,
	0, 4,
    //tapa de enfrente    
	1, 2,
	2, 3,
	3, 4,
	4, 1,
    //triangulo
	5, 6,
	6, 7,
	7, 5,
};

Object* CameraActive = nullptr;

class Camera : public Object {
    public:
        Camera(Object* parent = nullptr, GLfloat x = 0.0f, GLfloat y = 0.0f, GLfloat z = 0.0f, 
            GLfloat rx = 0.0f, GLfloat ry = 0.0f, GLfloat rz = 0.0f, GLfloat scale = 1.0f)
        : Object(parent, "Camera") {
            posX = x;
            posY = y;
            posZ = z;
            rotX = rx;
            rotY = ry;
            rotZ = rz;
		    scaleX = scaleY = scaleZ = scale;
            IconType = static_cast<size_t>(IconType::camera);

            if (!CameraActive){
                CameraActive = this;
            }
        }

        ObjectType getType() override{
            return ObjectType::camera;
        }

        void RenderObject() override {
            //si no esta el overlay. no dibuja la representacion de la luz en el espacio 3d
            if (!showOverlayGlobal) return;

            glPushMatrix();
            /*glScalef(
                FIXED_TO_FLOAT(scaleX),
                FIXED_TO_FLOAT(scaleZ),
                FIXED_TO_FLOAT(scaleY)
            );*/
            glScalef(scaleX, scaleZ, scaleY);

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
            glDisable( GL_TEXTURE_2D ); 
            glDisable( GL_BLEND );
            glDisable( GL_LIGHTING );
            glLineWidth(1);	
            glDisableClientState(GL_NORMAL_ARRAY); //no tiene normales. asi que crasharia

            glVertexPointer( 3, GL_FLOAT, 0, CameraVertices );
            glDrawElements( GL_LINES, CameraEdgesSize, GL_UNSIGNED_SHORT, CameraEdges );
            if (CameraActive == this){		
                glDisable( GL_CULL_FACE  );	
                glDrawElements( GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, CameraFaceActive );	
            }

            // Restaurar la matriz previa
            glPopMatrix();
        }

		~Camera() {
			delete name;
		}
};