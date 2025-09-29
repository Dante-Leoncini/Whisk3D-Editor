class VertexGroup { 
	public:
		std::vector<int> indices;
};

class Mesh { 
	public:
		int vertexSize = 0;
		GLshort* vertex = nullptr;
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