bool running = false;

// ===========================================================
// Tokenizer minimal JSON-style  ------------------------------
// ===========================================================
std::vector<std::string> Tokenize(std::string src){
    std::vector<std::string> T;
    std::string cur = "";

    for(char c : src){
        if(isspace(c)){
            if(!cur.empty()){ T.push_back(cur); cur.clear(); }
        }
        else if(c=='{'||c=='}'){
            if(!cur.empty()){ T.push_back(cur); cur.clear(); }
            T.push_back(std::string(1,c));
        }
        else if(c==':'){
            if(!cur.empty()){ T.push_back(cur); cur.clear(); }
            T.push_back(":");
        }
        else cur += c;
    }
    if(!cur.empty()) T.push_back(cur);
    return T;
}

// ===========================================================
// Node - estructura interna del archivo .w3d ----------------
// ===========================================================
struct Node {
    std::string type;                                       // Mesh / Light / Layout / Viewport3D
    std::map<std::string,std::string> props;                // name: "Cube" / x: 0.5 ...
    std::vector<Node*> children;                            // nodos dentro del bloque
};

Node* Find(Node* root, const std::string& type){
    if(!root) return nullptr;
    if(root->type == type) return root;

    for(Node* c : root->children){
        if(Node* f = Find(c, type)) return f;
    }
    return nullptr;
}

// ===========================================================
// Parser recursivo ------------------------------------------
// ===========================================================
Node* ParseNode(std::vector<std::string>& tk, size_t& i){
    if(i >= tk.size()) return nullptr;

    Node* n = new Node();
    n->type = tk[i++];                // Tipo: Mesh / Layout / Split / Viewport3D etc.

    // ----- Caso 1: No hay "{" → nodo hoja -----
    if(i >= tk.size() || tk[i] != "{"){
        return n; // ejemplo:  "Viewport3D"
    }

    i++; // consume "{"

    // ----- Caso 2: Contenido dentro de llaves -----
    while(i < tk.size()){
        if(tk[i] == "}"){ i++; break; }

        // --- propiedad con ":" ---
        if(i+2 < tk.size() && tk[i+1]==":"){
            n->props[ tk[i] ] = tk[i+2];
            i+=3;
        }

        // --- propiedad sin ":" → estilo "Split 0.7" ---
        else if(i+1 < tk.size() && tk[i+1]!="{" && tk[i+1]!="}"
                                     && tk[i]!="{" && tk[i]!="}"){
            n->props[ tk[i] ] = tk[i+1];
            i+=2;
        }

        // --- bloque hijo ---
        else{
            n->children.push_back(ParseNode(tk,i));
        }
    }
    return n;
}

// ===========================================================
// Build Scene (traduce Node → Objetos reales Whisk3D) -------
// ===========================================================
void BuildScene(Node* root){
    if(!root || root->type!="Escena") return;

    for(Node* c : root->children){
        if(c->type=="Collection"){
            CollectionActive = new Collection(SceneCollection);

            if(c->props.count("x")) CollectionActive->posX = std::stof(c->props["x"]);
            if(c->props.count("y")) CollectionActive->posY = std::stof(c->props["y"]);
            if(c->props.count("z")) CollectionActive->posZ = std::stof(c->props["z"]);

            for(Node* n : c->children){

                if(n->type=="Camera"){
                    float x = n->props.count("x")?std::stof(n->props["x"]):0;
                    float y = n->props.count("y")?std::stof(n->props["y"]):0;
                    float z = n->props.count("z")?std::stof(n->props["z"]):0;
                    new Camera(CollectionActive,x,y,z,0,-35,45);
                }

                if(n->type=="Mesh"){
                    Object* m = NewMesh(MeshType::cube, CollectionActive);

                    m->posX = n->props.count("x")?std::stof(n->props["x"]):0;
                    m->posY = n->props.count("y")?std::stof(n->props["y"]):0;
                    m->posZ = n->props.count("z")?std::stof(n->props["z"]):0;
                }

                if(n->type=="Light"){
                    float X = std::stof(n->props["x"]);
                    float Y = std::stof(n->props["y"]);
                    float Z = std::stof(n->props["z"]);

                    float R = n->props.count("r")?std::stof(n->props["r"]):1.0f;
                    float G = n->props.count("g")?std::stof(n->props["g"]):1.0f;
                    float B = n->props.count("b")?std::stof(n->props["b"]):1.0f;

                    Light* L = Light::Create(CollectionActive,X,Y,Z);
                    L->SetDiffuse(R,G,B);
                }
            }
        }
    }
}

// ===========================================================
// Build UI Layout -------------------------------------------
// ===========================================================
ViewportBase* BuildLayout(Node* n){
    if(!n) return nullptr;
    std::cout << "Creando Layout" << std::endl;

    if(n->type == "Viewport3D")  return new Viewport3D();
    if(n->type == "Outliner")    return new Outliner();

    if(n->type == "ViewportRow" || n->type == "ViewportColumn"){
        
        bool isRow = (n->type=="ViewportRow");
        float split = n->props.count("Split") ? std::stof(n->props["Split"]) : 0.5f;
        
        // Espera 2 o más hijos
        ViewportBase* A = BuildLayout(n->children[0]);
        ViewportBase* B = BuildLayout(n->children[1]);

        if(isRow)  return new ViewportRow   (A,B,split);
        else       return new ViewportColumn(A,B,split);
    }
    return nullptr;
}

//un constructor universal para todas las plataformas
void ConstructUniversal(int argc, char* argv[]){
    // Configuración básica de OpenGL
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    // iluminación. GL_LIGHT0 es el sol
    SetViewType(RenderType::MaterialPreview);

    // Siempre un material por defecto
    MaterialDefecto = new Material("Default", true);
    
	CalculateMillisecondsPerFrame(60);

    //es el tamaño en X e Y de la textura
    CrearIconos(128, 128);
    CalcBorderUV(128, 128);
    CalcScrollUV(128, 128);
    SetGlobalScale(3);
    InitCursors();

    // ======================================================
    // Si se abrió un archivo .w3d al hacer doble click
    // ======================================================
    if(argc > 1){
        std::ifstream f(argv[1]);
        if(!f.is_open()){ 
            printf("No pude abrir archivo\n"); 
            return; 
        }

        // Leer archivo
        std::string src((std::istreambuf_iterator<char>(f)),{});
        auto tokens = Tokenize(src);
        size_t i=0;

        // Toda la escena está bajo un único nodo raíz: Whisk3D
        Node* project = ParseNode(tokens,i);

        if(!project || project->type != "Whisk3D"){
            std::cout << "ERROR: Se esperaba nodo raíz Whisk3D" << std::endl;
            return;
        }

        // Buscar secciones dentro del proyecto
        Node* escena = Find(project,"Escena");
        Node* layout = Find(project,"Layout");

        if(!escena) std::cout << "AVISO: No hay 'Escena'\n";
        if(!layout) std::cout << "AVISO: No hay 'Layout'\n";

        BuildScene(escena);
        rootViewport = BuildLayout(layout);

        std::cout << "Proyecto W3D cargado OK.\n";
        std::cout << "rootViewport=" << rootViewport << std::endl;
        std::cout << "SceneCollection=" << SceneCollection << std::endl;
        return;
    }

    // ======================================================
    // Si se abre sin archivo → escena default
    // ======================================================
    CollectionActive = new Collection(SceneCollection);

    new Camera(CollectionActive,-3,2.5,1.8,0,-35,45);
    Light* L = Light::Create(CollectionActive,1,2.25,2.25);
    L->SetDiffuse(1,0,0);
    NewMesh(MeshType::cube,CollectionActive);

    rootViewport = new ViewportColumn(
        new Outliner(),
        new Viewport3D(),
        0.20f
    );
}