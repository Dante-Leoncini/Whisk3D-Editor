const float DEG2RAD = 3.14159265358979323846f / 180.0f;

struct Vec3 {
    float x,y,z;
    Vec3() : x(0),y(0),z(0) {}
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
};
inline Vec3 operator+(const Vec3&a,const Vec3&b){return Vec3(a.x+b.x,a.y+b.y,a.z+b.z);}
inline Vec3 operator-(const Vec3&a,const Vec3&b){return Vec3(a.x-b.x,a.y-b.y,a.z-b.z);}
inline Vec3 operator*(const Vec3&a,float s){return Vec3(a.x*s,a.y*s,a.z*s);}
inline Vec3 operator*(float s,const Vec3&a){return a*s;}
inline float Dot(const Vec3&a,const Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 Cross(const Vec3&a,const Vec3&b){ return Vec3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
inline float Len(const Vec3&a){ return sqrtf(Dot(a,a)); }
inline Vec3 Normalize(const Vec3&a){ float L = Len(a); if(L<=1e-8f) return Vec3(0,0,1); return a*(1.0f/L); }

class ShapeKeyVertex { 
	public:
		int index;
		GLshort vertexX;
		GLshort vertexY;
		GLshort vertexZ;
		GLbyte normalX;
		GLbyte normalY;
		GLbyte normalZ;
};

class ShapeKey { 
	public:
        //RArray<ShapeKeyVertex> Vertex;
		std::vector<ShapeKeyVertex> Vertex;
};

class Animation { 
	public:
        //RArray<ShapeKey> Frames;
		std::vector<ShapeKey> Frames;
		int MixSpeed;
};

class ShapeKeyAnimation { 
	public:
		int Id; //id del objeto al que afecta
        //RArray<Animation> Animations;
		std::vector<Animation> Animations;
		int ChangeAnimation;
		int LastAnimation;
		int NextAnimation;
		int LastFrame;
		int NextFrame;
		int Mix;
		bool Normals;
		bool Faces;
		bool Interpolacion;
};

class Children {
	public:
    	int Id = -1;
		GLfixed OriginalScaleX = 0;
		GLfixed OriginalScaleY = 0; 
		GLfixed OriginalScaleZ = 0;	
};

class Constraint { 
	public:
		int type = 0;
		int Id = -1;
		int Target = -1;
		bool opcion = false;
};

class SaveState {
	public:
		int indice = -1;
		GLfloat posX = 0.0f;
		GLfloat posY = 0.0f;
		GLfloat posZ = 0.0f;
		GLfloat rotX = 0.0f;
		GLfloat rotY = 0.0f;
		GLfloat rotZ = 0.0f;
		GLfixed scaleX = 0;
		GLfixed scaleY = 0;
		GLfixed scaleZ = 0;
};

class Object {
	public:
		int type = 0;
		bool visible = true;
		bool seleccionado = false;
		GLfloat posX = 0.0f, posY = 0.0f, posZ = 0.0f;
		GLfloat rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
		GLfixed scaleX = 0, scaleY = 0, scaleZ = 0;
		int Id = -1;
		int Parent = -1;
		std::vector<Children> Childrens;
		//HBufC* name;
};

class Light { 
	public:
		int type = 0;
		int lightId = -1;
		GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // blanco por defecto
};

class Modifier { 
	public:
		int type = 0;
		int Id = -1;
		int Target = -1;
		std::vector<bool> OpcionesTBool;
        //RArray<TBool> OpcionesTBool;
};

class Texture { 
	public:
    	GLuint iID = -1;
    	//int iID = -1;
};

class Material { 
	public:
		bool textura = false;
		bool transparent = false;
		bool vertexColor = false;
		bool lighting = true;
		bool repeat = false;
		bool uv8bit = false;
		bool culling = true;
		int interpolacion = 0;
		GLuint textureID = 0;
		GLfloat diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
		GLfloat specular[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		//HBufC* name;
};

class MaterialGroup { 
	public:
        int start = 0; //donde esta el primer triangulo real
        int count = 0; //cuantos triangulos son reales

        int startDrawn = 0; //indice del primer triangulo para dibujar
		int indicesDrawnCount = 0; //cuantos vertices son
		int material = -1; //de que material
};

class VertexGroup { 
	public:
        //RArray<TInt> indices;
		std::vector<int> indices;
};

class Mesh { 
	public:
		int vertexSize = 0;
		GLshort* vertex = nullptr;
        //RArray<VertexGroup> vertexGroups;
		std::vector<VertexGroup> vertexGroups;
		GLubyte* vertexColor = nullptr;
		GLbyte* normals = nullptr;
		GLfloat* uv = nullptr;
		//GLbyte* uv8;

		//caras
   		int facesSize = 0;
   		GLushort* faces = nullptr;
        //RArray<MaterialGroup> materialsGroup;
		std::vector<MaterialGroup> materialsGroup;
		//RArray<TInt> Modifiers;
		std::vector<int> Modifiers;

		//libera la memoria
		void LiberarMemoria(){
			delete[] vertex;
			delete[] vertexColor;
			delete[] normals;
			delete[] uv;
			for(size_t i=0; i < vertexGroups.size(); i++){
				vertexGroups[i].indices.clear();
			}
			vertexGroups.clear();

			delete[] faces;
			materialsGroup.clear();
			Modifiers.clear();
		}
};