class Material;                 // forward declaration de la clase
extern std::vector<Material*> Materials;   // forward declaration del vector global

class Material { 
	public:
		bool textura = false;
		bool transparent = false;
		bool vertexColor = false;
		bool lighting = true;
		bool repeat = true;
		bool uv8bit = false;
		bool culling = true;
		int interpolacion = lineal;
		GLuint textureID = 0;
		GLfloat diffuse[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
		GLfloat specular[4] = {0.3f, 0.3f, 0.3f, 1.0f};
		GLfloat emission[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        Object2D* name = nullptr;

        Material(const std::string& nombre, bool MaterialDefecto = false, bool vertexColor = false) 
		: vertexColor(vertexColor){
            name = AddObject2D(UI::text);
            reinterpret_cast<Text*>(name->data)->SetValue(nombre);
			if (!MaterialDefecto){
				Materials.push_back(this);
			}
        }

		~Material() {
			delete name;
		}
};

Material* MaterialDefecto;
std::vector<Material*> Materials;

Material* BuscarMaterialPorNombre(const std::string& name) {
    for (size_t i = 0; i < Materials.size(); ++i)
        if (reinterpret_cast<Text*>(Materials[i]->name->data)->value == name) return Materials[i];
    return nullptr;
}

int DuplicateMaterial(int srcId) {
	return -1;
    /*if (srcId < 0 || srcId >= (int)Materials.size()) return -1;

    Material copy = Materials[srcId]; // copia completa
    Materials.push_back(copy);

    return (int)Materials.size() - 1; // devuelve el índice del duplicado*/
}