class VertexGroup { 
	public:
		std::vector<int> indices;
};

class MaterialGroup { 
	public:
        int start = 0; //donde esta el primer triangulo real
        int count = 0; //cuantos triangulos son reales

        int startDrawn = 0; //indice del primer triangulo para dibujar
		int indicesDrawnCount = 0; //cuantos vertices son
		int material = -1; //de que material
};

class Mesh { 
	public:
		int vertexSize = 0;
		GLshort* vertex = nullptr;
		GLfixed* vertexF = nullptr;	
		bool UseVertexF = false;

		std::vector<VertexGroup> vertexGroups;
		GLubyte* vertexColor = nullptr;
		GLbyte* normals = nullptr;
		GLfloat* uv = nullptr;

		//caras
   		int facesSize = 0;
   		GLushort* faces = nullptr;
		std::vector<MaterialGroup> materialsGroup;
		std::vector<int> Modifiers;

		//libera la memoria
		void LiberarMemoria(){
			delete[] vertex;
            delete[] vertexF;
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

std::vector<Mesh> Meshes;

int DuplicateMesh(int meshIndex) {
    if (meshIndex < 0 || meshIndex >= (int)Meshes.size()) {
        std::cerr << "DuplicateMesh: índice inválido\n";
        return -1;
    }

    Mesh& src = Meshes[meshIndex];
    Mesh copy;

    // Copiar tamaños
    copy.vertexSize = src.vertexSize;
    copy.facesSize = src.facesSize;

    // Copiar arrays dinámicos
    if (src.vertex && src.vertexSize > 0) {
        copy.vertex = new GLshort[src.vertexSize * 3];
        std::memcpy(copy.vertex, src.vertex, sizeof(GLshort) * src.vertexSize * 3);
    }

    if (src.vertexColor && src.vertexSize > 0) {
        copy.vertexColor = new GLubyte[src.vertexSize * 3];
        std::memcpy(copy.vertexColor, src.vertexColor, sizeof(GLubyte) * src.vertexSize * 3);
    }

    if (src.normals && src.vertexSize > 0) {
        copy.normals = new GLbyte[src.vertexSize * 3];
        std::memcpy(copy.normals, src.normals, sizeof(GLbyte) * src.vertexSize * 3);
    }

    if (src.uv && src.vertexSize > 0) {
        copy.uv = new GLfloat[src.vertexSize * 2];
        std::memcpy(copy.uv, src.uv, sizeof(GLfloat) * src.vertexSize * 2);
    }

    if (src.faces && src.facesSize > 0) {
        copy.faces = new GLushort[src.facesSize * 3];
        std::memcpy(copy.faces, src.faces, sizeof(GLushort) * src.facesSize * 3);
    }

    // Copiar std::vectors
    copy.vertexGroups = src.vertexGroups;
    copy.materialsGroup = src.materialsGroup;
    copy.Modifiers = src.Modifiers;

    // Insertar en Meshes y devolver índice
    Meshes.push_back(std::move(copy));
    return (int)Meshes.size() - 1;
}