// ----------------------------- helpers -----------------------------
float GetFloatOrDefault(const std::map<std::string,std::string>& props, const std::string& k, float def=0.0f){
    auto it = props.find(k);
    if(it==props.end() || it->second.empty()) return def;
    try{ return std::stof(it->second); } catch(...){ return def; }
}

std::string Unquote(const std::string& s){
    if(s.size()>=2 && ((s.front()=='"' && s.back()=='"') || (s.front()=='\'' && s.back()=='\'')))
        return s.substr(1,s.size()-2);
    return s;
}

// ----------------------------- Tokenizer -----------------------------
std::vector<std::string> Tokenize(const std::string& src){
    std::vector<std::string> T;
    size_t i = 0;
    while(i < src.size()){
        char c = src[i];

        // skip whitespace
        if(isspace((unsigned char)c)){ ++i; continue; }

        // comments // or #
        if(c == '/' && i+1 < src.size() && src[i+1] == '/'){
            i += 2;
            while(i < src.size() && src[i] != '\n') ++i;
            continue;
        }
        if(c == '#') { while(i < src.size() && src[i] != '\n') ++i; continue; }

        // braces
        if(c == '{' || c == '}'){
            T.push_back(std::string(1,c));
            ++i;
            continue;
        }

        // colon separator
        if(c == ':'){
            T.push_back(":");
            ++i;
            continue;
        }

        // quoted string
        if(c == '"' || c == '\''){
            char q = c; ++i;
            std::string cur;
            while(i < src.size() && src[i] != q){
                if(src[i] == '\\' && i+1 < src.size()){ // simple escape
                    ++i;
                    cur.push_back(src[i]);
                } else cur.push_back(src[i]);
                ++i;
            }
            if(i < src.size() && src[i] == q) ++i; // consume closing quote
            T.push_back(std::string("\"") + cur + std::string("\"")); // keep quoted form
            continue;
        }

        // other token (until whitespace or special char)
        std::string cur;
        while(i < src.size()){
            char d = src[i];
            if(isspace((unsigned char)d) || d=='{' || d=='}' || d==':' || d=='#') break;
            // treat comma as separator too
            if(d==','){ ++i; break; }
            cur.push_back(d);
            ++i;
        }
        if(!cur.empty()) T.push_back(cur);
    }
    return T;
}

// ----------------------------- Node & Find -----------------------------
struct Node {
    std::string type;
    std::map<std::string,std::string> props;
    std::vector<Node*> children;
};

Node* Find(Node* root, const std::string& type){
    if(!root) return nullptr;
    if(root->type == type) return root;
    for(Node* c : root->children){
        if(Node* f = Find(c, type)) return f;
    }
    return nullptr;
}

// ----------------------------- Parser -----------------------------
Node* ParseNode(std::vector<std::string>& tk, size_t& i){
    if(i >= tk.size()) return nullptr;

    Node* n = new Node();
    n->type = tk[i++];

    // No abre bloque → nodo unitario (ej: Viewport3D)
    if(i>=tk.size() || tk[i] != "{")
        return n;

    i++; // consume "{"

    while(i < tk.size())
    {
        if(tk[i] == "}"){
            i++; break;
        }

        // ---------------------------------------------
        // 1) Hijo con tipo simple (Viewport3D, Outliner…)
        // ---------------------------------------------
        if(i+1 < tk.size() && tk[i+1] != ":" && tk[i+1] != "{"){
            Node* child = new Node();
            child->type = tk[i];
            n->children.push_back(child);
            i++;
            continue;
        }

        // ---------------------------------------------
        // 2) Hijo con bloque { }
        // ---------------------------------------------
        if(i+1 < tk.size() && tk[i+1] == "{"){
            n->children.push_back(ParseNode(tk,i));
            continue;
        }

        // ---------------------------------------------
        // 3) Propiedades estilo "Split: 0.5"
        // ---------------------------------------------
        if(i+2 < tk.size() && tk[i+1] == ":"){
            n->props[tk[i]] = Unquote(tk[i+2]);
            i += 3;
            continue;
        }

        i++;
    }

    return n;
}

// ----------------------------- Builders -----------------------------
ViewportBase* BuildLayout(Node* n){
    if(!n) return nullptr;
    std::cout << "[BuildLayout] node=" << n->type << std::endl;

    if(n->type == "Viewport3D") return new Viewport3D();
    if(n->type == "Outliner")  return new Outliner();

    if(n->type == "ViewportRow" || n->type == "ViewportColumn"){
        bool isRow = (n->type == "ViewportRow");
        float split = GetFloatOrDefault(n->props, "Split", 0.5f);

        // --- Fallback: si el parser dejó A:/B: como props en lugar de children,
		//     convertimos esas props en nodos temporales para BuildLayout. ---
		if(n->children.size() < 2){
			bool made = false;
			// intenta crear children desde props "A" y "B" si existen
			if(n->props.count("A") || n->props.count("B")){
				if(n->props.count("A")){
					Node* a = new Node();
					a->type = n->props["A"];
					n->children.insert(n->children.begin(), a);
					made = true;
				}
				if(n->props.count("B")){
					Node* b = new Node();
					b->type = n->props["B"];
					n->children.push_back(b);
					made = true;
				}
			}
			// si hicimos algo, seguimos adelante y BuildLayout procesará estos nodos
			if(made){
				std::cerr << "[BuildLayout] Notice: creado children temporales desde props A/B para " << n->type << std::endl;
			}
		}

        ViewportBase* A = BuildLayout(n->children[0]);
        ViewportBase* B = BuildLayout(n->children[1]);

        if(!A) A = new Viewport3D();
        if(!B) B = new Viewport3D();

        if(isRow)  return new ViewportRow(A,B,split);
        else       return new ViewportColumn(A,B,split);
    }

    return nullptr;
}

void ApplyCommonProps(Object* obj, const std::map<std::string,std::string>& p){
    if(!obj) return;
    // Nombre
    if(p.count("name")){
		// Si ya existe el Text*, reemplazamos el nombre
		obj->SetNameObj(p.at("name"));
	}

    // Posición
    if(p.count("x")) obj->posX = GetFloatOrDefault(p,"x");
    if(p.count("y")) obj->posY = GetFloatOrDefault(p,"y");
    if(p.count("z")) obj->posZ = GetFloatOrDefault(p,"z");

    // Rotación
    if(p.count("rx")) obj->rotX = GetFloatOrDefault(p,"rx");
    if(p.count("ry")) obj->rotY = GetFloatOrDefault(p,"ry");
    if(p.count("rz")) obj->rotZ = GetFloatOrDefault(p,"rz");

    // Escala
    if(p.count("scale")){
        float s = GetFloatOrDefault(p,"scale",1);
        obj->scaleX=obj->scaleY=obj->scaleZ = s;
    } else {
        if(p.count("sx")) obj->scaleX = GetFloatOrDefault(p,"sx",1);
        if(p.count("sy")) obj->scaleY = GetFloatOrDefault(p,"sy",1);
        if(p.count("sz")) obj->scaleZ = GetFloatOrDefault(p,"sz",1);
    }
}

Object* CreateObjectFromNode(Node* n, Object* parent){
    if(!n) return nullptr;

    // ‼ Se crea el objeto por tipo (mínimo switch posible)
    if(n->type=="Mesh")
        return NewMesh(MeshType::cube, parent);

    if(n->type=="Camera")
        return new Camera(parent,0,0,0, 0, 0, 0);

    if(n->type=="Light"){
        Light* L=Light::Create(parent,X,Y,Z);
        L->SetDiffuse(
            GetFloatOrDefault(n->props,"r",1),
            GetFloatOrDefault(n->props,"g",1),
            GetFloatOrDefault(n->props,"b",1)
        );
        return L;
    }

    if(n->type=="Collection"){
        auto* C = new Collection(parent);
        if(!CollectionActive) CollectionActive = C;
        return C;
    }

    return nullptr; // objeto desconocido → ignorado
}

void BuildObjectRecursive(Node* n, Object* parent){
    Object* obj = CreateObjectFromNode(n, parent);
    if(!obj) return;

    ApplyCommonProps(obj, n->props);

    for(Node* c : n->children)
        BuildObjectRecursive(c, obj);
}

void BuildScene(Node* root){
    if(!root || root->type!="Escena") return;

    for(Node* n : root->children)
        BuildObjectRecursive(n, SceneCollection);  // 👈 el parent puede ser la escena global

    if(!CollectionActive) 
        CollectionActive = SceneCollection;        // seguridad
}

void OpenW3D(std::string path){
	std::ifstream f(path);
	if(!f.is_open()){
		std::cerr << "Warning: no pude abrir archivo: " << path << "  -> usando escena por defecto\n";
	} 
	else {
		std::string src((std::istreambuf_iterator<char>(f)), {});
		auto tokens = Tokenize(src);
		size_t i = 0;

		// parse root: espera Whisk3D { ... }
		Node* project = ParseNode(tokens, i);
		if(!project || project->type != "Whisk3D"){
			std::cerr << "ERROR: expected Whisk3D root, got: " << (project?project->type:"(null)") << "\n";
		} 
		else {
			Node* escena = Find(project, "Escena");
			Node* layout = Find(project, "Layout");


			if(!escena) std::cerr << "AVISO: No hay 'Escena' en archivo\n";
			if(!layout) std::cerr << "AVISO: No hay 'Layout' en archivo\n";

			if(escena) BuildScene(escena);
			// ensure we have a CollectionActive even if scene empty
			if(!CollectionActive) CollectionActive = new Collection(SceneCollection);

			if(layout && layout->children.size()>0)
    			rootViewport = BuildLayout(layout->children[0]);
			if(!rootViewport) {
				std::cerr << "AVISO: rootViewport no creado, usando por defecto\n";
				rootViewport = new ViewportColumn(new Outliner(), new Viewport3D(), 0.2f);
			}
		}
	}
    //std::cout << "CollectionActive: " << CollectionActive << " rootViewport: " << rootViewport << std::endl;
}