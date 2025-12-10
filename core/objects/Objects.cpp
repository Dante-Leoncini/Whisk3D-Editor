#include "Objects.h"

// Variables globales
std::vector<Object*> ObjSelects;
Object* CollectionActive = nullptr;
Object* ObjActivo = nullptr;
std::vector<SaveState> estadoObjetos;

Object::Object(Object* parent, const std::string& nombre, Vector3 pos, Vector3 Rot, Vector3 Scale)
    : Parent(parent), pos(pos), scale(Scale) {
    name = new Text(SetName(nombre));

    // Convertir los ángulos de Euler (Rot.x, Rot.y, Rot.z) a tres cuaterniones simples.
    // Usaremos un orden común para una configuración inicial: YXZ (Yaw, Pitch, Roll)
    // Orden de aplicación: Z (Roll) se aplica primero, luego X (Pitch), luego Y (Yaw).
    
    Quaternion qX = Quaternion::FromAxisAngle(Vector3(1, 0, 0), Rot.x); // Pitch (Eje X)
    Quaternion qY = Quaternion::FromAxisAngle(Vector3(0, 1, 0), Rot.y); // Yaw (Eje Y)
    Quaternion qZ = Quaternion::FromAxisAngle(Vector3(0, 0, 1), Rot.z); // Roll (Eje Z)

    // Composición de Cuaterniones (ejemplo con orden YXZ):
    // Multiplicación en C++: rot = qA * qB * qC  (qC se aplica primero)
    // Para orden YXZ: qZ (Roll) primero, luego qX (Pitch), luego qY (Yaw).
    
    rot = qY * qX * qZ; // Aplicación: (Z) * (X) * (Y)

    if (Parent) {
        Parent->Childrens.push_back(this);
    } else if (SceneCollection && this != SceneCollection) {
        SceneCollection->Childrens.push_back(this);
    }

    DeseleccionarTodo();
    Seleccionar();
}

Object::~Object() {
    for (Object* c : Childrens) {
        c->Parent = Parent;
    }
    Childrens.clear();

    if (name) {
        name = nullptr;
    }
}

void Object::SetNameObj(const std::string& nombre) {
    name->SetValue(SetName(nombre));
}

void Object::EliminarObjetosSeleccionados(bool IncluirCollecciones) {
    for (int i = (int)Childrens.size() - 1; i >= 0; i--) {
        Object* child = Childrens[i];
        child->EliminarObjetosSeleccionados(IncluirCollecciones);
        if (child->select && (IncluirCollecciones || child->getType() != ObjectType::collection)) {
            std::cout << "Se borro '" << child->name->value << "'" << std::endl;
            Childrens.erase(Childrens.begin() + i);
            delete child;
        }
    }
}

std::string Object::SetName(const std::string& baseName) {
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

void Object::Seleccionar(){
    ObjActivo = this;
    if (!select){
        select = true;
        ObjSelects.push_back(this);
    }
}

void Object::Deseleccionar(){
    select = false;
    for(size_t o=0; o < ObjSelects.size(); o++){
        if (this == ObjSelects[o]){
            ObjSelects.erase(ObjSelects.begin() + o);
            break;
        }                
    } 
}

void Object::DeseleccionarCompleto(bool IncluirColecciones){
    select = false;
    for(size_t o=0; o < Childrens.size(); o++){
        Childrens[o]->DeseleccionarCompleto(IncluirColecciones);		
    } 
}

bool Object::EstaSeleccionado(bool IncluirCollecciones){
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

bool Object::SeleccionarCompleto(bool IncluirColecciones){
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

        
void Object::Reload(){}
        
void Object::ReloadAll(){
    Reload();

    // Procesar cada hijo
    for (size_t c = 0; c < Childrens.size(); c++) {
        Childrens[c]->ReloadAll();
    }      
}

void Object::GetMatrix(Matrix4& out) const {
    // --- Rotación ---
    Matrix4 R;
    R.Identity();
    rot.ToMatrix(R.m);

    // --- Escala ---
    Matrix4 S;
    S.Identity();
    S.m[0]  = scale.x;
    S.m[5]  = scale.y;
    S.m[10] = scale.z;

    // --- Traslación ---
    Matrix4 T;
    T.Identity();
    T.m[12] = pos.x;
    T.m[13] = pos.y;
    T.m[14] = pos.z;

    // out = T * (R * S)
    out = T * (R * S);
}

void Object::RotateLocal(float pitch, float yaw, float roll){
    Quaternion qPitch = Quaternion::FromAxisAngle(1,0,0, pitch);
    Quaternion qYaw   = Quaternion::FromAxisAngle(0,1,0, yaw);
    Quaternion qRoll  = Quaternion::FromAxisAngle(0,0,1, roll);

    rot = rot * qYaw * qPitch * qRoll;
}

// obtener posición global simple (suma rápida de translations)
// útil si necesitás la posición del objeto en world (para LookAt)
Vector3 Object::GetGlobalPosition() const {
    Vector3 p = pos;              // usa el Vector3 del propio objeto
    const Object* q = Parent;
    while (q) {
        p += q->pos;              // suma las traslaciones de los padres
        q = q->Parent;
    }
    return p;
}

void Object::RenderObject(){}

// Funcion recursiva para renderizar un objeto y sus hijos
void Object::Render(){   
    if (!visible) return; 
    // Guardar la matriz actual
    glPushMatrix();     

    GetMatrix(M);
    glMultMatrixf(M.m);   // aplica T * R * S -> incluye traslación

    // Si es visible y no es un mesh, lo dibuja
    RenderObject();

    // Procesar cada hijo
    for (size_t c = 0; c < Childrens.size(); c++) {
        Childrens[c]->Render();
    }

    // Restaurar la matriz previa
    glPopMatrix();
}

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

#include "Target.h"
#include "Constraint.h"
#include "Scene.h"
#include "Collection.h"
#include "Light.h"
#include "Camera.h"
#include "Mesh.h"
#include "Instance.h"
#include "Mirror.h"
#include "Gamepad.h"

bool DetectLoop(Object* node,
                std::unordered_set<Object*>& visited,
                std::unordered_set<Object*>& stack,
                int depth){
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

bool IsSelectable(Object* obj, bool IncluirColecciones) {
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

Object* GetFirstDFS(){
    if (!SceneCollection) return nullptr;
    if (SceneCollection->Childrens.empty()) return nullptr;
    return SceneCollection->Childrens[0];
}

void changeSelect(SelectMode mode, bool IncluirColecciones){
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

void SeleccionarTodo(bool IncluirColecciones){
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
bool HayObjetosSeleccionados(bool IncluirColecciones){
    return SceneCollection->EstaSeleccionado(IncluirColecciones);
}