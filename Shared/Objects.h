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

std::vector<Object*> Objects;

void DeseleccionarTodo(){
	if (InteractionMode == ObjectMode){
		//for(int o=0; o < Objects.Count(); o++){
		for(size_t o=0; o < Objects.size(); o++){
			Objects[o]->seleccionado = false;				
		}
		SelectCount = 0;
	}
}

//outliner
void UpdateOutlinerColor(){
	for (size_t c = 0; c < Collections.size(); c++) {
		Object& obj = *Objects[c];
		if (SelectActivo == obj.Id && obj.seleccionado){
			SetColorOutlinerText(c, ListaColoresUbyte[accent][0], ListaColoresUbyte[accent][1], ListaColoresUbyte[accent][2]);
		}
		else if (obj.seleccionado){
			SetColorOutlinerText(c, ListaColoresUbyte[accentDark][0], ListaColoresUbyte[accentDark][1], ListaColoresUbyte[accentDark][2]);
		}
		else {	
			SetColorOutlinerText(c, ListaColoresUbyte[negro][0], ListaColoresUbyte[negro][1], ListaColoresUbyte[negro][2]);	
		}
	}
}

void changeSelect(){
	if (InteractionMode == ObjectMode){
		//si no hay objetos
		//o si esta moviendo, rotando o haciendo algo... no deja que continue
		if (1 > Objects.size() || estado != editNavegacion){
			return;
		}
		//DeseleccionarTodo();
		//deselecciona el objeto actual si es que estaba seleccionado
		if (SelectActivo < 1 && (int)(Objects.size() > 0)){
			SelectCount = 1;
			SelectActivo = 0;
		}
		if (Objects[SelectActivo]->seleccionado){
			Objects[SelectActivo]->seleccionado = false;
			SelectCount--;
		}

		//pasa al siguiente
		SelectActivo++;
		if (SelectActivo >= (int)(Objects.size()) ){
			SelectActivo = 0;
		}
		//selecciona el proximo objeto
		if (!Objects[SelectActivo]->seleccionado){
			Objects[SelectActivo]->seleccionado = true;
			SelectCount++;
		}
		UpdateOutlinerColor();
	}
    redibujar = true;	
}

std::string SetName(const std::string& baseName) {
    // Comprueba si un nombre ya existe
    auto nameExists = [&](const std::string& name) {
        for (size_t o = 0; o < Objects.size(); ++o) {
            if (Objects[o]->name == name)
                return true;
        }
        return false;
    };

    // Si el nombre base no existe, devolverlo tal cual
    if (!nameExists(baseName))
        return baseName;

    // Detectar si baseName termina en ".<digits>"
    std::string root = baseName;
    int startCounter = 1;

    size_t pos = baseName.find_last_of('.');
    if (pos != std::string::npos && pos + 1 < baseName.size()) {
        bool allDigits = true;
        for (size_t i = pos + 1; i < baseName.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(baseName[i]))) { allDigits = false; break; }
        }
        if (allDigits) {
            // separar raíz y sufijo numérico
            root = baseName.substr(0, pos);
            try {
                startCounter = std::stoi(baseName.substr(pos + 1)) + 1;
                if (startCounter < 1) startCounter = 1;
            } catch (...) {
                startCounter = 1;
            }
        } else {
            root = baseName; // no es sufijo numérico
            startCounter = 1;
        }
    } else {
        root = baseName;
        startCounter = 1;
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

void AddObject( int tipo ){
	//Cancelar();
	Object* obj = new Object();
	obj->type = tipo;
	obj->visible = true;
	obj->seleccionado = false;
	obj->posX = Cursor3DposX;
	obj->posY = Cursor3DposY;
	obj->posZ = Cursor3DposZ;
	obj->rotX = obj->rotY = obj->rotZ = 0;
	obj->scaleX = obj->scaleY = obj->scaleZ = 45000;
	obj->Parent = -1;	
	obj->Id = -0;
	Objects.push_back(obj);
	SelectActivo = Objects.size()-1;
	if (tipo == light){
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
		Objects[SelectActivo]->name = SetName("Light");
	}
	//tipo camara
	else if (tipo == camera){
		Objects[SelectActivo]->name = SetName("Camera");
		if (CameraActive < 0){
			CameraActive = SelectActivo;		
		}		
	}
	AddToCollection(SelectActivo, Objects[SelectActivo]->name, GetIconType(Objects[SelectActivo]->type));
	DeseleccionarTodo();
	Objects[SelectActivo]->seleccionado = true;
	SelectCount = 1;
    redibujar = true;
}