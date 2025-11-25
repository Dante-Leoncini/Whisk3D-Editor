class Viewport3D;     // forward declaration válida
extern Viewport3D* Viewport3DActive;

enum class ObjectType {
    scene,
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
extern Object* SceneCollection;
extern std::vector<Object*> ObjSelects;   // forward declaration del vector global
Object* ObjActivo = nullptr;

void DeseleccionarTodo(); // ← forward declaration

class Object {
    public:
		Object* Parent = nullptr;
        //size_t ChildrenIndex; //solo se usa si Parent no es nullptr;
        std::vector<Object*> Childrens;
		bool visible = true;
		bool desplegado = true;
        bool select = true;
        Object2D* name = nullptr;
        size_t IconType = 0;
        
		GLfloat posX = 0.0f, posY = 0.0f, posZ = 0.0f;
		GLfloat rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
		GLfloat scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;

        virtual ObjectType getType(){
            return ObjectType::baseObject;
        }

        void SetName(const std::string& baseName) {
            if (!SceneCollection) reinterpret_cast<Text*>(name->data)->SetValue(baseName);
            // ----------------------------
            // 1) Función recursiva para revisar un objeto y todos sus hijos
            // ----------------------------
            std::function<bool(const Object*, const std::string&)> nameExistsInTree;

            nameExistsInTree = [&](const Object* obj, const std::string& name) -> bool {
                if (!obj) return false;

                if (obj->name && obj->name->data) {
                    Text* txt = reinterpret_cast<Text*>(obj->name->data);
                    if (txt->value == name) return true;
                }

                for (Object* child : obj->Childrens) {
                    if (nameExistsInTree(child, name)) return true;
                }

                return false;
            };

            // ----------------------------
            // 2) Buscar en toda la jerarquía de la escena
            // ----------------------------
            auto nameExists = [&](const std::string& name) -> bool {
                if (!SceneCollection) return false;
                return nameExistsInTree(SceneCollection, name);
            };

            // ----------------------------
            // 3) Si el nombre base NO existe → usarlo directamente
            // ----------------------------
            if (!nameExists(baseName)) {
                reinterpret_cast<Text*>(name->data)->SetValue(baseName);
                return;
            }

            // ----------------------------
            // 4) Particionar si termina en ".NNN"
            // ----------------------------
            std::string root = baseName;
            int startCounter = 1;

            size_t pos = baseName.find_last_of('.');
            if (pos != std::string::npos && pos + 1 < baseName.size()) {

                bool digits = true;
                for (size_t i = pos + 1; i < baseName.size(); ++i) {
                    if (!isdigit((unsigned char)baseName[i])) { digits = false; break; }
                }

                if (digits) {
                    root = baseName.substr(0, pos);
                    try {
                        startCounter = std::stoi(baseName.substr(pos + 1)) + 1;
                        if (startCounter < 1) startCounter = 1;
                    } catch (...) {}
                }
            }

            // ----------------------------
            // 5) Buscar el primer ".NNN" libre
            // ----------------------------
            int counter = startCounter;
            std::string newName;

            do {
                std::ostringstream ss;
                ss << root << "." << std::setw(3) << std::setfill('0') << counter;
                newName = ss.str();
                counter++;
            } while (nameExists(newName));

            reinterpret_cast<Text*>(name->data)->SetValue(newName);
        }

        Object(Object* parent, const std::string& nombre)
            : Parent(parent){
            name = AddObject2D(UI::text);
            SetName(nombre);

            // Si tiene padre → agregar a su vector Children
            if (Parent) {
                Parent->Childrens.push_back(this);
                //ChildrenIndex = Parent->Childrens.size() -1;
                //std::cout << "ChildrenIndex: " << ChildrenIndex << std::endl;
            }
            // Si no tiene padre → agregar a la raíz global
            else if (SceneCollection && this != SceneCollection){
                SceneCollection->Childrens.push_back(this);
                //ChildrenIndex = 0;
            }

            DeseleccionarTodo();
            Seleccionar();
        }

        void Seleccionar(){
            ObjActivo = this;
            if (!select){
                select = true;
                ObjSelects.push_back(this);
            }
        }

        void Deseleccionar(){
            select = false;
            for(size_t o=0; o < ObjSelects.size(); o++){
                if (this == ObjSelects[o]){
                    ObjSelects.erase(ObjSelects.begin() + o);
                    break;
                }                
            } 
        }

        void DeseleccionarCompleto(){
            select = false;
            for(size_t o=0; o < Childrens.size(); o++){
                Childrens[o]->DeseleccionarCompleto();		
            } 
        }

		bool EliminarObjetosSeleccionados(){
            // 1) Procesar hijos primero
            /*for (int i = (int)Childrens.size() - 1; i >= 0; i--){
                Object* child = Childrens[i];

                // Recursión: si el hijo pide borrarse (select == true)
                if (child->EliminarObjetosSeleccionados()){
				    std::cout << "Se borro '" << reinterpret_cast<Text*>(child->name->data)->value << "'"<< std::endl;
                    delete child;
                    Childrens.erase(Childrens.begin() + i);
                }
                else if (select){
                    //despues tengo que meterlo en algun child o en la lista de objetos.. o queda huerfano en memoria
                    child->Parent = nullptr;
                }
            }*/

            // 2) Si YO estoy seleccionado,
            // indico al padre que debe desconectarme y borrar ME (NO AQUÍ)
            return select;
        }

        bool EstaSeleccionado(bool IncluirCollecciones){
            // Si este objeto está seleccionado y cumple la condición de colecciones → true
            if ( select && ( IncluirCollecciones || getType() != ObjectType::collection ) ) {
                return true;
            }

            //si no estaba seleccionado. mira recursivamente a los hijos
            for(size_t o=0; o < Childrens.size(); o++){   
                if (Childrens[o]->EstaSeleccionado(IncluirCollecciones)) return true;
            }
            return false;       
        }

        bool SeleccionarCompleto(){
            //si hay algo seleccionado retorna true para deseleccionar todo
            if (select) return true;
            
            if (getType() != ObjectType::collection && getType() != ObjectType::baseObject){
                select = true;
                ObjSelects.push_back(this);
                if (!ObjActivo) ObjActivo = this;
            }

            for(size_t o=0; o < Childrens.size(); o++){      
                if (Childrens[o]->SeleccionarCompleto()) return true;
            }
            //nada seleccionado
            return false;
        }

        virtual void RenderObject(){}

        // Funcion recursiva para renderizar un objeto y sus hijos
        void Render(){   
            if (!visible) return; 
            // Guardar la matriz actual
            glPushMatrix();       
                
            // Aplicar las transformaciones del objeto
            glTranslatef(posX, posZ, posY);
            glRotatef(rotX, 1, 0, 0); // angulo, X Y Z
            glRotatef(rotZ, 0, 1, 0); // angulo, X Y Z
            glRotatef(rotY, 0, 0, 1); // angulo, X Y Z

            // Si es visible y no es un mesh, lo dibuja
            RenderObject();

            // Procesar cada hijo
            for (size_t c = 0; c < Childrens.size(); c++) {
                Childrens[c]->Render();
            }

            // Restaurar la matriz previa
            glPopMatrix();
        }

		virtual ~Object() {
            //Childrens.clear();
            if (name) delete name;
		}
};

#include "./Objects/Scene.h"
#include "./Objects/Collection.h"
#include "./Objects/Light.h"
#include "./Objects/Camera.h"
#include "./Objects/Mesh.h"

//Objects es el punto raiz. o los objetos estan dentro de esta lista. o dentro de objetos de esta lista
//std::vector<Object*> Objects;
Object* SceneCollection = new Scene();
std::vector<Object*> ObjSelects;
Object* CollectionActive = nullptr;

size_t GetIndexInParent(Object* obj) {
    /*if (!obj->Parent) {
        auto it = std::find(Objects.begin(), Objects.end(), obj);
        if (it == Objects.end()) return (size_t)-1;
        return it - Objects.begin();
    }*/
    auto& siblings = obj->Parent->Childrens;
    auto it = std::find(siblings.begin(), siblings.end(), obj);
    if (it == siblings.end()) return (size_t)-1;
    return it - siblings.begin();
}

Object* GetPrevDFS(Object* current){
   /* if (!current) return nullptr;

    // 1) Si tiene hermano anterior → ir al hermano anterior y bajar al último hijo profundo
    Object* parent = current->Parent;
    size_t idx = GetIndexInParent(current);

    if (parent && idx > 0){
        Object* node = parent->Childrens[idx - 1];

        // bajar al hijo más profundo
        while(!node->Childrens.empty()){
            node = node->Childrens.back();
        }
        return node;
    }

    // 2) Si no tiene hermano anterior → el anterior es su padre
    if (parent) return parent;

    // 3) Es raíz → buscar raíz anterior
    size_t ridx = GetIndexInParent(current);
    if (ridx > 0){
        Object* node = SceneCollection->Childrens[ridx - 1];
        while(!node->Childrens.empty()){
            node = node->Childrens.back();
        }
        return node;
    }*/

    return nullptr;
}

Object* GetNextDFS(Object* current) {
    if (!current) {
        //std::cout << "GetNextDFS: current fue nulo!\n";
        return nullptr;
    }

    // 1) Si tiene hijos -> primero hijo
    if (!current->Childrens.empty()) {
        //std::cout << "Tenia hijos, se selecciono su hijo\n";
        return current->Childrens[0];
    }

    // 2) Si no tiene hijos -> subimos buscando el siguiente hermano de algún ancestro
    Object* node = current;

    while (node) {
        Object* parent = node->Parent;

        // --------------------------------------------------------
        // Caso especial: node es la raíz (SceneCollection)
        // Antes buscábamos el "siguiente root" en Objects[], ahora no existe.
        // Por lo tanto, no hay siguiente.
        // --------------------------------------------------------
        if (!parent) {
            if (node == SceneCollection) {
                return nullptr;  // fin total del DFS
            } else {
                std::cout << "[ERROR] Root inesperado diferente a SceneCollection " << parent << "\n";
                std::cout << "Objeto: " << reinterpret_cast<Text*>(node->name->data)->value << "\n";
                return nullptr;
            }
        }

        // Buscar node entre los hijos del padre
        auto& siblings = parent->Childrens;
        auto it = std::find(siblings.begin(), siblings.end(), node);

        if (it == siblings.end()) {
            std::cout << "GetNextDFS: inconsistencia, parent no contiene al hijo\n";
            return nullptr;
        }

        // Intentar el siguiente hermano
        ++it;
        if (it != siblings.end()) {
            return *it;  // siguiente hermano → siguiente en DFS
        }

        // No hay más hermanos → seguir subiendo
        node = parent;
    }

    return nullptr;
}

bool IsSelectable(Object* obj, bool IncluirColecciones = false) {
    if (!obj) return false;

    const ObjectType t = obj->getType();

    // Si NO queremos incluir colecciones → bloquearlas
    if (!IncluirColecciones) {
        if (t == ObjectType::collection)
            return false;
    }

    // Siempre bloquear baseObject
    if (t == ObjectType::baseObject)
        return false;

    return true;
}

enum class SelectMode {
    NextSingle,      // Deselecciona actual → selecciona siguiente
    NextAdd,         // Mantiene actual → agrega el siguiente
    PrevAdd          // Mantiene actual → agrega anterior
};

Object* GetFirstDFS(){
    if (!SceneCollection) return nullptr;
    if (SceneCollection->Childrens.empty()) return nullptr;
    return SceneCollection->Childrens[0];
}

void changeSelect(SelectMode mode, bool IncluirColecciones = false){
    if (InteractionMode != ObjectMode) return;
    if (estado != editNavegacion) return;
    if (SceneCollection->Childrens.empty()) return;

    //std::cout << "changeSelect " << ObjActivo << std::endl;
    //std::cout << "Seleccionados: " << ObjSelects.size() << std::endl;
    //std::cout << "IncluirColecciones: " << IncluirColecciones << std::endl;

    // Si no hay activo → elegir el primero DFS
    if (!ObjActivo){
        Object* it = SceneCollection->Childrens[0];

        while(it && !IsSelectable(it, IncluirColecciones))
            it = GetNextDFS(it);

        if (it){
            ObjActivo->Deseleccionar();
            it->Seleccionar();
        }
        return;
    }

    Object* next = nullptr;

    // elegir next o previous según modo
    if (mode == SelectMode::NextSingle || mode == SelectMode::NextAdd){
        next = GetNextDFS(ObjActivo);
    }
    else if (mode == SelectMode::PrevAdd){
        next = GetPrevDFS(ObjActivo);
    }

    if (!next) {
        // Llegamos al final del DFS → wrap
        next = GetFirstDFS();
        if (!next) return;
    }

    // Buscar un selectable
    Object* it = next;
    while(it && !IsSelectable(it, IncluirColecciones)){
        it = (mode == SelectMode::PrevAdd)
               ? GetPrevDFS(it)
               : GetNextDFS(it);
    }

    if (!it) return;

    // ------------------------
    // Aplicar modo de selección
    // ------------------------

    if (mode == SelectMode::NextSingle){
        ObjActivo->Deseleccionar();
        it->Seleccionar();
        return;
    }

    if (mode == SelectMode::NextAdd || mode == SelectMode::PrevAdd){
        it->Seleccionar();
        return;
    }
}

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

void ApagarLucesHijas(Object* obj){
    //si es una luz. la apaga
    if (obj->getType() == ObjectType::light) {
        Light* luz = dynamic_cast<Light*>(obj);
        if (luz) {
            glDisable(luz->LightID);
        }
    }
    //lo mismo con los hijos
    for(size_t o=0; o < obj->Childrens.size(); o++){   
        ApagarLucesHijas(obj->Childrens[o]);
    }
}

void SetDesplegado(bool desplegado){
    if (SceneCollection && ObjActivo){
        ObjActivo->desplegado = desplegado;
    }
}

void ChangeVisibilityObj(){
    if (InteractionMode == ObjectMode && estado == editNavegacion && SceneCollection && ObjActivo){
        ObjActivo->visible = !ObjActivo->visible;
        //apagar luces en caso de que era una luz o sus hijas eran luces
        if (!ObjActivo->visible && view == RenderType::Rendered) ApagarLucesHijas(ObjActivo);
    }
}

void DeseleccionarTodo(){
	if (InteractionMode == ObjectMode && SceneCollection){
        ObjSelects.clear();
        SceneCollection->DeseleccionarCompleto();	
	}
}

void SeleccionarTodo(){
    //recorre las colecciones y selecciona todo. si llega a encontrar algo hace lo contrario. deselecciona todo
	if (InteractionMode == ObjectMode && SceneCollection){
        ObjSelects.clear();
        //habia algo seleccionado... asi que hacemos lo contrario. deseleccionar todo
        if (SceneCollection->SeleccionarCompleto()){
            //std::cout << "habia algo seleccionado! se deselecciona todo\n";
            DeseleccionarTodo();
            return;
        }
        //std::cout << "Todos los objetos seleccionados\n";
    }
}

//si hay objetos seleccionasos, devuelve true
bool HayObjetosSeleccionados(bool IncluirColecciones = false){
    return SceneCollection->EstaSeleccionado(IncluirColecciones);
}