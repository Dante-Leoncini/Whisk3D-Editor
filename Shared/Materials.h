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
		std::string name;
};

class MaterialGroup { 
	public:
        int start = 0; //donde esta el primer triangulo real
        int count = 0; //cuantos triangulos son reales

        int startDrawn = 0; //indice del primer triangulo para dibujar
		int indicesDrawnCount = 0; //cuantos vertices son
		int material = -1; //de que material
};

std::vector<Material> Materials;