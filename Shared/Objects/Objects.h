/*class Viewport3D;     // forward declaration válida
extern Viewport3D* Viewport3DActive;*/

enum class ObjectType {
    scene,
    mesh, 
    camera, 
    light, 
    empty, 
    armature, 
    curve,
    collection,
    baseObject,
    mirror,
    array,
    gamepad,
    instance
};

// Declaración adelantada
class Object;                 // forward declaration de la clase
extern Object* SceneCollection;
extern std::vector<Object*> ObjSelects;   // forward declaration del vector global
Object* ObjActivo = nullptr;

void DeseleccionarTodo(bool IncluirColecciones = false); // ← forward declaration

class Object {
    public:
		Object* Parent = nullptr;
        //size_t ChildrenIndex; //solo se usa si Parent no es nullptr;
        std::vector<Object*> Childrens;
		bool visible = true;
		bool desplegado = true;
        bool select = true;
        Text* name = nullptr;
        size_t IconType = 0;
        
		GLfloat posX = 0.0f, posY = 0.0f, posZ = 0.0f;
		GLfloat rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
		GLfloat scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;

        virtual ObjectType getType(){
            return ObjectType::baseObject;
        }

        Object(Object* parent, const std::string& nombre)
            : Parent(parent){
            name = new Text(SetName(nombre));

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

        void SetNameObj(const std::string& nombre){
            name->SetValue(SetName(nombre));
        }
        
        virtual ~Object() {
            // evita que los hijos queden huerfanos
            for (Object* c : Childrens){
                c->Parent = Parent;
            }

            Childrens.clear();

            // Borrar nombre/UI
            if (name) {
                //delete name;
                name = nullptr;
            }
        }


        //Esta funcion siempre borra a los hijos del objeto. nunca el objeto en si
        //como la Escena es un objeto. la escena nunca se borra
		void EliminarObjetosSeleccionados(bool IncluirCollecciones = false){
		    //std::cout << "Eliminando objetos en " << reinterpret_cast<Text*>(name->data)->value << std::endl;
            // 1) Procesar hijos primero
            for (int i = (int)Childrens.size() - 1; i >= 0; i--){
                Object* child = Childrens[i];

                // Recursión: si el hijo pide borrarse (select == true)
                child->EliminarObjetosSeleccionados(IncluirCollecciones);
                if (child->select && (IncluirCollecciones || child->getType() != ObjectType::collection ) ){
				    std::cout << "Se borro '" << child->name->value << "'"<< std::endl;
                    Childrens.erase(Childrens.begin() + i);
                    delete child;
                }
            }
        }

        std::string SetName(const std::string& baseName) {
            if (!SceneCollection) return baseName;

            // conservar puntero a este objeto para ignorarlo en la búsqueda
            const Object* me = this;

            // ----------------------------
            // 1) Función recursiva para revisar un objeto y todos sus hijos
            //    IGNORANDO 'me'
            // ----------------------------
            std::function<bool(const Object*, const std::string&)> nameExistsInTree;

            nameExistsInTree = [&](const Object* obj, const std::string& name) -> bool {
                if (!obj) return false;

                // Si este objeto NO es 'me' y tiene nombre igual -> existe
                if (obj != me && obj->name) {
                    if (obj->name->value == name) return true;
                }

                for (const Object* child : obj->Childrens) {
                    if (nameExistsInTree(child, name)) return true;
                }

                return false;
            };

            // ----------------------------
            // 2) Buscar en toda la jerarquía de la escena (ignorando 'me')
            // ----------------------------
            auto nameExists = [&](const std::string& name) -> bool {
                if (!SceneCollection) return false;
                return nameExistsInTree(SceneCollection, name);
            };

            // ----------------------------
            // 3) Si el nombre base NO existe → usarlo directamente
            // ----------------------------
            if (!nameExists(baseName)) {
                return baseName;
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

            return newName;
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

        void DeseleccionarCompleto(bool IncluirColecciones = false){
            select = false;
            for(size_t o=0; o < Childrens.size(); o++){
                Childrens[o]->DeseleccionarCompleto(IncluirColecciones);		
            } 
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

        bool SeleccionarCompleto(bool IncluirColecciones = false){
            //si hay algo seleccionado retorna true para deseleccionar todo
            if (select) return true;
            
            if (IncluirColecciones || (getType() != ObjectType::collection && getType() != ObjectType::baseObject)){
                select = true;
                ObjSelects.push_back(this);
                if (!ObjActivo) ObjActivo = this;
            }

            for(size_t o=0; o < Childrens.size(); o++){      
                if (Childrens[o]->SeleccionarCompleto(IncluirColecciones)) return true;
            }
            //nada seleccionado
            return false;
        }

        
        virtual void Reload(){}
        
        void ReloadAll(){
            Reload();

            // Procesar cada hijo
            for (size_t c = 0; c < Childrens.size(); c++) {
                Childrens[c]->ReloadAll();
            }      
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
};

Object* FindObjectByName(Object* node, const std::string& name){
    if (!node) return nullptr;

    // Coincide con este nodo → devolverlo
    if (node->name && node->name->value == name)
        return node;

    // Buscar en hijos
    for (Object* child : node->Childrens){
        Object* r = FindObjectByName(child, name);
        if (r) return r; // si lo encontró en un hijo, devolverlo
    }

    return nullptr; // no encontrado
}

#include "./Objects/Modifier.h"
#include "./Objects/Scene.h"
#include "./Objects/Collection.h"
#include "./Objects/Light.h"
#include "./Objects/Camera.h"
#include "./Objects/Mesh.h"
#include "./Objects/Instance.h"
#include "./Objects/Mirror.h"
#include "./Objects/Gamepad.h"
#include "./Objects/Array.h"

//Objects es el punto raiz. o los objetos estan dentro de esta lista. o dentro de objetos de esta lista
//std::vector<Object*> Objects;
Object* SceneCollection = new Scene();
std::vector<Object*> ObjSelects;
Object* CollectionActive = nullptr;

bool DetectLoop(Object* node,
                std::unordered_set<Object*>& visited,
                std::unordered_set<Object*>& stack,
                int depth = 0){
    if (!node) return false;

    // Si el nodo ya está en recursion → BUCLE encontrado
    if (stack.count(node)) {
        std::cout << "⚠ LOOP DETECTADO en nodo: " << node->name->value << std::endl;
        return true;
    }

    // Si ya lo visitamos pero no está en la pila actual → no hay loop aquí
    if (visited.count(node)) return false;

    visited.insert(node);
    stack.insert(node);

    // revisar hijos
    for (auto* child : node->Childrens)
        if (DetectLoop(child, visited, stack, depth + 1))
            return true;

    stack.erase(node);
    return false;
}

void SearchLoop(){
    std::unordered_set<Object*> visited;
    std::unordered_set<Object*> stack;

    std::cout << "Analizando árbol para loops..." << std::endl;

    if (DetectLoop(SceneCollection, visited, stack)) {
        std::cout << "\n🟥 RESULTADO: Hay bucles en la jerarquía!\n" << std::endl;
    } else {
        std::cout << "\n🟩 No se detectaron loops. Árbol sano.\n" << std::endl;
    }
}

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

// Devuelve el último nodo DFS (más profundo) a partir de 'node'
Object* GetDeepestDFS(Object* node){
    if (!node) return nullptr;

    while(!node->Childrens.empty()){
        node = node->Childrens.back();
    }
    return node;
}

Object* GetPrevDFS(Object* current){
    if (!current) return nullptr;

    Object* parent = current->Parent;

    // --- Caso especial: current es el primer hijo del root ---
    if (parent == SceneCollection){
        auto& siblings = parent->Childrens;
        if (!siblings.empty() && current == siblings.front()){
            // Volver al último nodo DFS del árbol completo
            return GetDeepestDFS(siblings.back());
        }
    }

    // --- Si existe hermano anterior ---
    if (parent){
        auto& siblings = parent->Childrens;
        auto it = std::find(siblings.begin(), siblings.end(), current);

        if (it == siblings.end()){
            std::cout << "GetPrevDFS: inconsistencia, parent no contiene a current\n";
            return nullptr;
        }

        if (it != siblings.begin()){
            --it; // hermano anterior
            return GetDeepestDFS(*it);
        }

        // Si no hay hermano anterior, el anterior es el padre
        return parent;
    }

    // Si es root real => no hay anterior
    if (current == SceneCollection)
        return nullptr;

    std::cout << "[ERROR] Root inesperado\n";
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
                std::cout << "Objeto: " << node->name->value << "\n";
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
    PrevSingle,      // Deselecciona actual → selecciona anterior
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
        if (SceneCollection->Childrens.empty()){
            std::cout << "no hay objetos para seleccionar" << std::endl;
            return;
        }
        Object* it = SceneCollection->Childrens[0];

        while(it && !IsSelectable(it, IncluirColecciones))
            it = GetNextDFS(it);

        if (it){
            it->Seleccionar();
        }
        else {
            std::cout << "los objetos no eran seleccionables" << std::endl;
        }
        return;
    }

    Object* next = nullptr;

    // elegir next o previous según modo
    if (mode == SelectMode::NextSingle || mode == SelectMode::NextAdd){
        next = GetNextDFS(ObjActivo);
    }
    else if (mode == SelectMode::PrevSingle || mode == SelectMode::PrevAdd){
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
    if (mode == SelectMode::NextSingle || mode == SelectMode::PrevSingle){
        ObjActivo->Deseleccionar();
    }

    it->Seleccionar();
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

void DeseleccionarTodo(bool IncluirColecciones){
	if (InteractionMode == ObjectMode && SceneCollection){
        ObjSelects.clear();
        SceneCollection->DeseleccionarCompleto(IncluirColecciones);	
	}
}

void SeleccionarTodo(bool IncluirColecciones = false){
    //recorre las colecciones y selecciona todo. si llega a encontrar algo hace lo contrario. deselecciona todo
	if (InteractionMode == ObjectMode && SceneCollection){
        ObjSelects.clear();
        //habia algo seleccionado... asi que hacemos lo contrario. deseleccionar todo
        if (SceneCollection->SeleccionarCompleto(IncluirColecciones)){
            //std::cout << "habia algo seleccionado! se deselecciona todo\n";
            DeseleccionarTodo(IncluirColecciones);
            return;
        }
        //std::cout << "Todos los objetos seleccionados\n";
    }
}

//si hay objetos seleccionasos, devuelve true
bool HayObjetosSeleccionados(bool IncluirColecciones = false){
    return SceneCollection->EstaSeleccionado(IncluirColecciones);
}