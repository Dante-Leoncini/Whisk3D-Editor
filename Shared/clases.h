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
		/*void Mesh::LiberarMemoria(){
			delete[] vertex;
			delete[] vertexColor;
			delete[] normals;
			delete[] uv;
			for(TInt i=0; i < vertexGroups.Count(); i++){
				vertexGroups[i].indices.Close();
			}
			vertexGroups.Close();

			delete[] faces;
			materialsGroup.Close();
			Modifiers.Close();
		}*/
};