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

std::vector<Material> Materials;