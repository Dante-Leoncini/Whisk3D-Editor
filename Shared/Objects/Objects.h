enum class ObjectType {
    mesh, 
    camera, 
    light, 
    empty, 
    armature, 
    curve,
    collection,
    baseObject
};

// Declaración adelantada
class Object;                 // forward declaration de la clase
extern std::vector<Object*> Objects;   // forward declaration del vector global
Object* ObjActivo = nullptr;

class Object {
    public:
		Object* Parent = nullptr;
        std::vector<Object*> Childrens;
		bool visible = true;
        bool select = true;
        bool SelectActivo = true;
        Object2D* name = nullptr;
        size_t IconType = 0;
        
		GLfloat posX = 0.0f, posY = 0.0f, posZ = 0.0f;
		GLfloat rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
		GLfloat scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;

        virtual ObjectType getType(){
            return ObjectType::baseObject;
        }

        Object(Object* parent, const std::string& nombre)
            : Parent(parent){
            name = AddObject2D(UI::text);
            reinterpret_cast<Text*>(name->data)->SetValue(nombre);

            // Si tiene padre → agregar a su vector Children
            if (Parent) {
                Parent->Childrens.push_back(this);
            }
            // Si no tiene padre → agregar a la raíz global
            else {
                Objects.push_back(this);
            }
            ObjActivo = this;
        }

        void DeseleccionarTodo(){
            /*select = false;
            for(size_t o=0; o < Objects.size(); o++){
                Objects[o]->DeseleccionarTodo();		
            }    */ 
        }

        bool SeleccionarTodo(){
            return false;
            /*for(size_t o=0; o < Objects.size(); o++){
                if (Objects[o]->select){
                    return true;
                }
                Objects[o]->select = true;
                Objects[o]->SelectActivo = false;	
            }
            for(size_t c=0; c < Collections.size(); c++){
                if (Collections[c]->select){
                    return true;
                }
                Collections[c]->select = true;
                Collections[c]->SelectActivo = false;
                for(size_t cc=0; cc < Collections.size(); cc++){
                    if (Collections[cc]->select){
                        return true;
                    }
                    if (Collections[cc]->SeleccionarTodo()){
                        return true;
                    }
                }
            }
            return false;*/
        }

        virtual void RenderObject(){
            /*void RenderObject( Object& obj ){
                //si es un empty
                if (obj.type == empty){		
                    glDisable( GL_TEXTURE_2D );	 
                    glDisable( GL_BLEND );	
                    glVertexPointer( 3, GL_SHORT, 0, EmptyVertices );
                    glDrawElements( GL_LINES, EmptyEdgesSize, GL_UNSIGNED_SHORT, EmptyEdges );
                }
                glPopMatrix();
            }*/
        }

        // Funcion recursiva para renderizar un objeto y sus hijos
        void Render(){    
            // Guardar la matriz actual
            glPushMatrix();       
                
            // Aplicar las transformaciones del objeto
            glTranslatef(posX, posZ, posY);
            glRotatef(rotX, 1, 0, 0); // angulo, X Y Z
            glRotatef(rotZ, 0, 1, 0); // angulo, X Y Z
            glRotatef(rotY, 0, 0, 1); // angulo, X Y Z

            // Si es visible y no es un mesh, lo dibuja
            if (visible) {
                RenderObject();
            }
                
            // Procesar cada hijo
            for (size_t c = 0; c < Childrens.size(); c++) {
                Childrens[c]->Render();
            }

            // Restaurar la matriz previa
            glPopMatrix();
        }

		~Object() {
			delete name;
		}
};

std::string SetName(const std::string& baseName) {
    return baseName;
    // Función recursiva para comprobar si un nombre ya existe
    /*std::function<bool(const Object*, const std::string&)> nameExistsInCollection;
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
        for (Collection* childCol : col->Collections) {
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

    return newName;*/
}

#include "./Objects/Collection.h"
#include "./Objects/Light.h"
#include "./Objects/Camera.h"
#include "./Objects/Mesh.h"

//Objects es el punto raiz. o los objetos estan dentro de esta lista. o dentro de objetos de esta lista
std::vector<Object*> Objects;
Object* CollectionActive = nullptr;

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

size_t GetIconType(int type){
    return 0;
	/*switch (type) {			
		case camera: {return static_cast<size_t>(IconType::camera); break;}
		case light: {return static_cast<size_t>(IconType::light); break;}
		case mesh: {return static_cast<size_t>(IconType::mesh); break;}
		default: {return 0; break;}
	}*/
};

/*Object* AddObject( size_t type ){
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
		glLightf(   nextLightId, GL_QUADRATIC_ATTENUATION, 0.5  );*/

		/*glLightf(   nextLightId, GL_SPOT_CUTOFF,   170.0                );
		glLightf(   nextLightId, GL_SPOT_EXPONENT,  20.0                );
		glLightfv(  nextLightId, GL_SPOT_DIRECTION, lightDirectionSpot );*/
		/*nextLightId++;

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
	obj->select = true;
	SelectActivo = obj;
	SelectCount = 1;
	return obj;
}*/

void DeseleccionarTodo(){
	if (InteractionMode == ObjectMode){
		/*for(size_t o=0; c < Objects.size(); o++){
            Objects[o]->DeseleccionarTodo();		
		}*/
        SelectCount = 0;
	}
}

void SeleccionarTodo(){
    //recorre las colecciones y selecciona todo. si llega a encontrar algo hace lo contrario. deselecciona todo
	/*if (InteractionMode == ObjectMode){
        for(size_t c=0; c < Collections.size(); c++){
            //habia algo seleccionado... asi que hacemos lo contrario. deseleccionar todo
            if (Collections[c]->SeleccionarTodo()){
                std::cout << "habia algo seleccionado! se deselecciona todo\n";
                DeseleccionarTodo();
                return;
            }
        }
        std::cout << "Todos los objetos seleccionados\n";
    }*/
}

void SetActive(Object* obj){
    DeseleccionarTodo();
    ObjActivo = obj;
    obj->select = true;
    obj->SelectActivo = true;
	SelectCount = 1;
}

//outliner
void UpdateOutlinerColor(){
	/*for (size_t c = 0; c < Collections.size(); c++) {
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
	}*/
}

void SetColorOutlinerText(size_t Id, float r, float g, float b){
    //Texts[Objects2Doutliner[Id]].SetColor(r, g, b);
}

/*void InitAllOutliners(){
    for (size_t c = 0; c < Collections.size(); c++) {
        Object& obj = Objects[c];

        int Id2D = AddObject2D(UI::text);
        Objects2Doutliner.push_back(Id2D);

        Object2D& tempObject2D = Objects2D[Id2D];
        Text& tempText = Texts[tempObject2D.Id];
        tempText.SetValue(obj.name);
        tempText.SetScaleX(2);
        tempText.SetScaleY(2);
        tempObject2D.opacity = 255;
        tempText.SetX(0);
        tempText.SetY(0);
        tempText.SetColor(255, 255, 255);
    }
    UpdateOutlinerColor();
}*/