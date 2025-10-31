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
		std::vector<Object*> Childrens;
		Object2D* name = AddObject2D(UI::text);
        size_t IconType = 0;

		void DeseleccionarTodo(){
            for(size_t c=0; c < Childrens.size(); c++){
                Childrens[c]->DeseleccionarTodo();		
                Childrens[c]->seleccionado = false;
            }     
        }

    	Object() = default;

		~Object() {
			delete name;
		}
};

Object* SelectActivo = nullptr;
Object* CameraActive = nullptr;
int ObjectsCount = 0;

#include "./Collections.h"

class SaveState {
	public:
		Object* obj;
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
std::vector<SaveState> estadoObjetos;

void changeSelect(){
	/*if (InteractionMode == ObjectMode){
		//si no hay objetos
		//o si esta moviendo, rotando o haciendo algo... no deja que continue
		if (1 > ObjectsCount || estado != editNavegacion){
			return;
		}
		//DeseleccionarTodo();
		//deselecciona el objeto actual si es que estaba seleccionado
		if (!SelectActivo && (int)(Objects.size() > 0)){
			SelectCount = 1;
			SelectActivo = 0;
		}
		if (SelectActivo->seleccionado){
			SelectActivo->seleccionado = false;
			SelectCount--;
		}

		//pasa al siguiente
		SelectActivo++;
		if (SelectActivo >= (int)(Objects.size()) ){
			SelectActivo = 0;
		}
		//selecciona el proximo objeto
		if (!SelectActivo->seleccionado){
			SelectActivo->seleccionado = true;
			SelectCount++;
		}
		UpdateOutlinerColor();
	}*/
}

std::string SetName(const std::string& baseName) {
    // Función recursiva para comprobar si un nombre ya existe
    std::function<bool(const Collection*, const std::string&)> nameExistsInCollection;
    nameExistsInCollection = [&](const Collection* col, const std::string& name) -> bool {
        // Recorremos los objetos de la colección
        for (Object* obj : col->Objects) {
            if (obj->name->data) {
                Text* text = reinterpret_cast<Text*>(obj->name->data);
                if (text->value == name) return true;
            }

            // Revisamos los hijos de cada objeto
            for (Object* child : obj->Childrens) {
                if (child->name->data) {
                    Text* textChild = reinterpret_cast<Text*>(child->name->data);
                    if (textChild->value == name) return true;
                }
            }
        }

        // Recorremos las subcolecciones recursivamente
        for (Collection* childCol : col->Childrens) {
            if (nameExistsInCollection(childCol, name)) return true;
        }

        return false;
    };

    auto nameExists = [&](const std::string& name) -> bool {
        for (Collection* col : Collections) {
            if (nameExistsInCollection(col, name)) return true;
        }
        return false;
    };

    // Si el nombre base no existe, devolverlo tal cual
    if (!nameExists(baseName)) return baseName;

    // Separar raíz y contador si termina en ".NNN"
    std::string root = baseName;
    int startCounter = 1;
    size_t pos = baseName.find_last_of('.');
    if (pos != std::string::npos && pos + 1 < baseName.size()) {
        bool allDigits = true;
        for (size_t i = pos + 1; i < baseName.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(baseName[i]))) { allDigits = false; break; }
        }
        if (allDigits) {
            root = baseName.substr(0, pos);
            try {
                startCounter = std::stoi(baseName.substr(pos + 1)) + 1;
                if (startCounter < 1) startCounter = 1;
            } catch (...) { startCounter = 1; }
        }
    }

    // Buscar el siguiente número disponible
    int counter = startCounter;
    std::string newName;
    do {
        std::ostringstream ss;
        ss << root << "." << std::setw(3) << std::setfill('0') << counter;
        newName = ss.str();
        counter++;
    } while (nameExists(newName));

    return newName;
}

size_t GetIconType(int type){
	switch (type) {			
		case camera: {return static_cast<size_t>(IconType::camera); break;}
		case light: {return static_cast<size_t>(IconType::light); break;}
		case mesh: {return static_cast<size_t>(IconType::mesh); break;}
		default: {return 0; break;}
	}
};

Object* AddObject( size_t type ){
	//Cancelar();
	ObjectsCount++;
	Object* obj = new Object();
	obj->type = type;
	obj->visible = true;
	obj->seleccionado = false;
	obj->posX = Cursor3DposX;
	obj->posY = Cursor3DposY;
	obj->posZ = Cursor3DposZ;
	obj->rotX = obj->rotY = obj->rotZ = 0;
	obj->scaleX = obj->scaleY = obj->scaleZ = 45000;
	obj->Parent = -1;	
	obj->Id = -0;
	obj->IconType = GetIconType(type);
	if (type == light){
		Light tempLight;
		tempLight.type = pointLight;
		tempLight.lightId = nextLightId;
		tempLight.color[0] = 1.0;
		tempLight.color[1] = 1.0;
		tempLight.color[2] = 1.0;
		tempLight.color[3] = 1.0;
		//tempLight.color  = { MATERIALCOLOR(1.0, 1.0, 1.0, 1.0) };

		glEnable( nextLightId );		
		GLfloat lightDiffuseSpot[4]   = { 1.0, 1.0, 1.0, 1.0 };
		GLfloat lightSpecularSpot[4]  = { 1.0, 1.0, 1.0, 1.0 };
		glLightfv(  nextLightId, GL_DIFFUSE,  lightDiffuseSpot  );
		glLightfv(  nextLightId, GL_AMBIENT,  objAmbient  );
		glLightfv(  nextLightId, GL_SPECULAR, lightSpecularSpot );
		//glLightfv(  nextLightId, GL_POSITION, lightPositionSpot );
		glLightfv(  nextLightId, GL_POSITION, positionPuntualLight );
		

		glLightf(   nextLightId, GL_CONSTANT_ATTENUATION,  1.5  );
		glLightf(   nextLightId, GL_LINEAR_ATTENUATION,    0.5  );
		glLightf(   nextLightId, GL_QUADRATIC_ATTENUATION, 0.5  );

		/*glLightf(   nextLightId, GL_SPOT_CUTOFF,   170.0                );
		glLightf(   nextLightId, GL_SPOT_EXPONENT,  20.0                );
		glLightfv(  nextLightId, GL_SPOT_DIRECTION, lightDirectionSpot );*/
		nextLightId++;

		//Lights.Append(tempLight);
		Lights.push_back(tempLight);
		//obj.Id = Lights.Count()-1;
		obj->Id = Lights.size()-1;
		reinterpret_cast<Text*>(obj->name->data)->SetValue(SetName("Light"));
	}
	//tipo camara
	else if (type == camera){
		reinterpret_cast<Text*>(obj->name->data)->SetValue(SetName("Camera"));
		if (!CameraActive){
			CameraActive = SelectActivo;		
		}		
	}
	AddToCollection(CollectionActive, obj);
	DeseleccionarTodo();
	obj->seleccionado = true;
	SelectActivo = obj;
	SelectCount = 1;
	return obj;
}