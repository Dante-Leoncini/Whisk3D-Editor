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
        Text* name = nullptr;

        Material(const std::string& nombre, bool MaterialDefecto = false, bool TieneVertexColor = false) {
            name = new Text(nombre);
			if (!MaterialDefecto){
				//std::cout << "Nuevo Material: " << nombre << std::endl;
				Materials.push_back(this);
			}
			vertexColor = TieneVertexColor;
        }

		~Material() {
			delete name;
		}
};

Material* MaterialDefecto;
std::vector<Material*> Materials;

Material* BuscarMaterialPorNombre(const std::string& name) {
    for (size_t i = 0; i < Materials.size(); ++i){
        if (Materials[i]->name->value == name) return Materials[i];
		//std::cout << "name: " << name << " material: " << reinterpret_cast<Text*>(Materials[i]->name->data)->value << std::endl;
	}
    return nullptr;
}

int DuplicateMaterial(int srcId) {
	return -1;
    /*if (srcId < 0 || srcId >= (int)Materials.size()) return -1;

    Material copy = Materials[srcId]; // copia completa
    Materials.push_back(copy);

    return (int)Materials.size() - 1; // devuelve el índice del duplicado*/
}