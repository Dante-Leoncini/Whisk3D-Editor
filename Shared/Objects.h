enum { mesh, camera, light, empty, armature, curve };

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
		std::string name;
};

std::vector<Object> Objects;