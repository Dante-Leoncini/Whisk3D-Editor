#include "importers/import_w3d.h"

// ============================================
// Helpers
// ============================================
float GetFloatOrDefault(const std::map<std::string,std::string>& props, const std::string& k, float def){
    auto it = props.find(k);
    if(it==props.end() || it->second.empty()) return def;
    try{ return std::stof(it->second); } catch(...){ return def; }
}

int GetIntOrDefault(const std::map<std::string,std::string>& props, const std::string& k, int def){
    auto it = props.find(k);
    if(it == props.end()) return def;
    try{ return std::stoi(it->second); } catch(...){ return def; }
}

bool GetBoolOrDefault(const std::map<std::string,std::string>& props, const std::string& key, bool def=false){
    auto it = props.find(key);
    if(it == props.end()) return def;

    std::string val = it->second;
    return (val == "true" || val == "1");
}

GLenum GetLightIDOrDefault(std::string name, GLenum defaultLight){
    static std::unordered_map<std::string, GLenum> lightMap = {
        {"GL_LIGHT0", GL_LIGHT0},
        {"GL_LIGHT1", GL_LIGHT1},
        {"GL_LIGHT2", GL_LIGHT2},
        {"GL_LIGHT3", GL_LIGHT3},
        {"GL_LIGHT4", GL_LIGHT4},
        {"GL_LIGHT5", GL_LIGHT5},
        {"GL_LIGHT6", GL_LIGHT6},
        {"GL_LIGHT7", GL_LIGHT7},
    };

    auto it = lightMap.find(name);
    return (it != lightMap.end()) ? it->second : defaultLight;
}

std::string Unquote(const std::string& s){
    if(s.size()>=2 && ((s.front()=='"' && s.back()=='"') || (s.front()=='\'' && s.back()=='\'')))
        return s.substr(1,s.size()-2);
    return s;
}

std::string GetFilePath(const std::string& raw){
    // 1. Sacar comillas si hay
    std::string filename = Unquote(raw);

    // 2. Obtener base dir del archivo .w3d
    std::string baseDir;
    size_t pos = w3dPath.find_last_of("/\\");
    if (pos != std::string::npos)
        baseDir = w3dPath.substr(0, pos + 1);
    else
        baseDir = "";

    // 3. Unir rutas
    return baseDir + filename;
}

// ============================================
// Tokenizer
// ============================================
std::vector<std::string> Tokenize(const std::string& src){
    std::vector<std::string> T;
    size_t i = 0;
    while(i < src.size()){
        char c = src[i];
        if(isspace((unsigned char)c)){ ++i; continue; }
        if(c == '/' && i+1 < src.size() && src[i+1] == '/'){ i+=2; while(i<src.size()&&src[i]!='\n') ++i; continue; }
        if(c == '#'){ while(i<src.size()&&src[i]!='\n') ++i; continue; }
        if(c=='{' || c=='}'){ T.push_back(std::string(1,c)); ++i; continue; }
        if(c==':'){ T.push_back(":"); ++i; continue; }

        if(c=='"'||c=='\''){
            char q=c; ++i; std::string cur;
            while(i<src.size()&&src[i]!=q){
                if(src[i]=='\\' && i+1<src.size()){ ++i; cur.push_back(src[i]); }
                else cur.push_back(src[i]);
                ++i;
            }
            if(i<src.size()&&src[i]==q) ++i;
            T.push_back(std::string("\"")+cur+"\"");
            continue;
        }

        std::string cur;
        while(i<src.size()){
            char d=src[i];
            if(isspace((unsigned char)d)||d=='{'||d=='}'||d==':'||d=='#') break;
            if(d==','){ ++i; break; }
            cur.push_back(d); ++i;
        }
        if(!cur.empty()) T.push_back(cur);
    }
    return T;
}

// ============================================
// Node & Find
// ============================================
Node* Find(Node* root, const std::string& type){
    if(!root) return nullptr;
    if(root->type == type) return root;
    for(Node* c : root->children){
        if(Node* f = Find(c,type)) return f;
    }
    return nullptr;
}

Node* ParseNode(std::vector<std::string>& tk, size_t& i){
    if(i >= tk.size()) return nullptr;
    Node* n = new Node();
    n->type = tk[i++];
    if(i>=tk.size() || tk[i]!="{") return n;
    i++;
    while(i<tk.size()){
        if(tk[i]=="}"){ i++; break; }
        if(i+2<tk.size() && tk[i+1]==":"){ n->props[tk[i]] = Unquote(tk[i+2]); i+=3; continue; }
        if(i+1<tk.size() && tk[i+1]!=":" && tk[i+1]!="{"){ Node* child = new Node(); child->type = tk[i]; n->children.push_back(child); i++; continue; }
        if(i+1<tk.size() && tk[i+1]=="{"){ n->children.push_back(ParseNode(tk,i)); continue; }
        i++;
    }
    return n;
}

// ============================================
// Builders
// ============================================
void ApplyViewport3DProps(Viewport3D* v, const std::map<std::string,std::string>& p){
    if(!v) return;

    auto F = [&](const std::string& k, float def=0.0f){
        return p.count(k) ? std::stof(p.at(k)) : def;
    };

    auto B = [&](const std::string& k, bool def=false){
        return p.count(k) ? (p.at(k)=="true" || p.at(k)=="1") : def;
    };

    /// --- bools ---
    if(p.count("orthographic"))           v->orthographic = B("orthographic");
    if(p.count("ViewFromCameraActive"))   v->ViewFromCameraActive = B("ViewFromCameraActive");
    if(p.count("showOverlays"))           v->showOverlays = B("showOverlays");
    if(p.count("ShowUi"))                 v->ShowUi = B("ShowUi");
    if(p.count("showFloor"))              v->showFloor = B("showFloor");
    if(p.count("showYaxis"))              v->showYaxis = B("showYaxis");
    if(p.count("showXaxis"))              v->showXaxis = B("showXaxis");
    if(p.count("CameraToView"))           v->CameraToView = B("CameraToView");
    if(p.count("showOrigins"))            v->showOrigins = B("showOrigins");
    if(p.count("show3DCursor"))           v->show3DCursor = B("show3DCursor");
    if(p.count("ShowRelantionshipsLines"))v->ShowRelantionshipsLines = B("ShowRelantionshipsLines");
    if(p.count("limpiarPantalla"))        v->limpiarPantalla = B("limpiarPantalla");

    /// --- floats/doubles ---
    if(p.count("nearClip"))       v->nearClip       = F("nearClip", 0.01f);
    if(p.count("farClip"))        v->farClip        = F("farClip", 1000.0f);
    if(p.count("aspect"))         v->aspect         = F("aspect", 1.0f);

    if(p.count("orbitDistance")) v->orbitDistance = F("orbitDistance", 10.0f);

    v->viewRot = Quaternion::FromEuler(
        GetFloatOrDefault(p, "rotY", -30.0f),
        GetFloatOrDefault(p, "rotX", -30.0f),
        GetFloatOrDefault(p, "rotZ", 0.0f)
    );

    if(p.count("posX")) v->pivot.x = F("posX"); //PivotX
    if(p.count("posY")) v->pivot.z = F("posY");
    if(p.count("posZ")) v->pivot.y = F("posZ");

    v->RecalcOrbitPosition();

    if(p.count("view"))  // string → enum
         v->view = StringToRenderType(p.at("view"));
}

// ----------------------------- Builders -----------------------------
ViewportBase* BuildLayout(Node* n){
    if(!n) return nullptr;
    std::cout << "[BuildLayout] node=" << n->type << std::endl;

    if(n->type == "Viewport3D"){
        auto* v = new Viewport3D();
        ApplyViewport3DProps(v, n->props);
        return v;
    }
    if(n->type == "Outliner")  return new Outliner();
    if(n->type == "Properties")  return new Properties();

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

        ViewportBase* A = nullptr;
        ViewportBase* B = nullptr;

        if(n->children.size() > 0) A = BuildLayout(n->children[0]);
        if(n->children.size() > 1) B = BuildLayout(n->children[1]);

        if(!A) A = new Viewport3D();
        if(!B) B = new Viewport3D();

        if(isRow)  return new ViewportRow(A,B,split);
        else       return new ViewportColumn(A,B,split);
    }

    return nullptr;
}

void ApplyCommonProps(Object* obj, const std::map<std::string,std::string>& p){
    if(!obj) return;

    auto B = [&](const std::string& k, bool def=false){
        return p.count(k) ? (p.at(k)=="true" || p.at(k)=="1") : def;
    };

    // Nombre
    if(p.count("name")){
		// Si ya existe el Text*, reemplazamos el nombre
		obj->SetNameObj(p.at("name"));
	}

    // Posición
    if(p.count("x")) obj->pos.x = GetFloatOrDefault(p,"x");
    if(p.count("y")) obj->pos.z = GetFloatOrDefault(p,"y");
    if(p.count("z")) obj->pos.y = GetFloatOrDefault(p,"z");

    // Rotación
    if(p.count("rx") || p.count("ry") || p.count("rz")){
        //rz y ry estan invertidos por openGl
        Quaternion qX = Quaternion::FromAxisAngle(Vector3(1, 0, 0), GetFloatOrDefault(p,"rx", 0)); // Pitch (Eje X)
        Quaternion qY = Quaternion::FromAxisAngle(Vector3(0, 1, 0), GetFloatOrDefault(p,"rz", 0)); // Yaw (Eje Y)
        Quaternion qZ = Quaternion::FromAxisAngle(Vector3(0, 0, 1), GetFloatOrDefault(p,"ry", 0)); // Roll (Eje Z)
        obj->rot = qY * qX * qZ; // Aplicación: (Z) * (X) * (Y)
    }

    
    if(p.count("desplegado")) obj->desplegado = B("desplegado");
    if(p.count("showRelantionshipsLines")) obj->showRelantionshipsLines = B("showRelantionshipsLines");    

    // Escala
    if(p.count("scale")){
        float s = GetFloatOrDefault(p,"scale",1);
        obj->scale.x = s;
        obj->scale.y = s;
        obj->scale.z = s;
        //std::cout << "[ApplyCommonProps] scale = " << s << std::endl; // <-- para debug
    } 
    else {
        if(p.count("sx")) obj->scale.x = GetFloatOrDefault(p,"sx",1);
        if(p.count("sy")) obj->scale.y = GetFloatOrDefault(p,"sy",1);
        if(p.count("sz")) obj->scale.z = GetFloatOrDefault(p,"sz",1);
    }
}

Object* CreateObjectFromNode(Node* n, Object* parent){
    if(!n) return nullptr;

    const auto& p = n->props;

    // ‼ Se crea el objeto por tipo (mínimo switch posible)
    if(n->type=="Mesh")
        return NewMesh(MeshType::cube, parent);

    // --- OBJ / Wavefront (.obj) ---
    if(n->type == "Wavefront"){
        std::string path;

        // Si el archivo viene con comillas --> Unquote
        if(n->props.count("filePath"))
            path = GetFilePath(n->props.at("filePath"));
        else {
            std::cerr << "[Wavefront] Falta filePath\n";
            return nullptr;
        }

        bool NoMerge = GetBoolOrDefault(n->props, "noMerge", false);        

        // --- IMPORTACIÓN ---
        Mesh* mesh = ImportWOBJ(path, parent, NoMerge);

        if (!mesh){
            std::cerr << "[Wobj] Se importo mal el wobj!\n";
            return nullptr;
        }

        for (auto* child : n->children) {
            if (child->type == "Animation") {
                auto& ap = child->props;

                bool UseNormals = GetBoolOrDefault(ap, "UseNormals", false);

                int speed = GetIntOrDefault(ap, "speed", 1);

                VertexAnimation* anim =
                    new VertexAnimation(nullptr, ap.at("name"), UseNormals, (float)speed);

                anim->basePath   = GetFilePath(ap.at("basePath"));
                anim->frameCount = std::stoi(ap.at("frames"));
                anim->padding    = std::stoi(ap.at("padding"));

                NewActiveVertexAnimation(mesh, anim);
            }
        }

        LoadVertexFrames(mesh);
        
        return mesh;
    }

    if (n->type=="Mirror"){
        Mirror* mirror = new Mirror(parent);
        if(p.count("target")) mirror->SetTarget(p.at("target"));
        return mirror;
    }

    if (n->type=="Instance"){
        Instance* instance = new Instance(parent);
        if(p.count("target")) instance->SetTarget(p.at("target"));
        if(p.count("count")) instance->count = GetIntOrDefault(p, "count", 1);
        return instance;
    }

    if (n->type=="Gamepad"){
        Gamepad* gamepad = new Gamepad(parent);
        if(p.count("target")) gamepad->SetTarget(p.at("target"));

        return gamepad;
    }

    if (n->type=="Curve"){
        std::string path;

        // Si el archivo viene con comillas --> Unquote
        if(n->props.count("filePath"))
            path = GetFilePath(n->props.at("filePath"));
        else {
            std::cerr << "[Curve] Falta filePath\n";
            return nullptr;
        }

        Curve* curve = new Curve(parent);
        if (curve->LoadFromFile(path)){
            return curve;     
        }
        return nullptr;        
    }

    if (n->type=="Constraint"){
        bool useHorizontal = GetBoolOrDefault(p, "useHorizontal", true);
        bool usePitch      = GetBoolOrDefault(p, "usePitch", true);

        Constraint* constraint = new Constraint(parent, useHorizontal, usePitch);

        constraint->SetCopyPosition(
            GetBoolOrDefault(p, "copyPosX", false),
            GetBoolOrDefault(p, "copyPosY", false),
            GetBoolOrDefault(p, "copyPosZ", false)
        );
        if(p.count("target")) constraint->SetTarget(p.at("target"));
        return constraint;
    }    

    if(n->type=="Camera"){
        Camera* camera = new Camera(parent, Vector3(0,0,0), Vector3(0, 0, 0));
        if(p.count("target")) camera->SetTarget(p.at("target"));
        if(p.count("riel")) camera->SetRiel(p.at("riel"));
        if(p.count("offsetRiel")) camera->offsetRiel = GetIntOrDefault(p, "offsetRiel", 0);
        return camera;
    }

    if(n->type=="Light"){
        Light* L=Light::Create(parent,X,Y,Z);
        L->SetDiffuse(
            GetFloatOrDefault(n->props,"r",1),
            GetFloatOrDefault(n->props,"g",1),
            GetFloatOrDefault(n->props,"b",1)
        );
        L->LightID = GetLightIDOrDefault(p.count("LightID") ? p.at("LightID") : "GL_LIGHT0");

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

    // --- Icono opcional desde W3D ---
    if(root->props.count("iconApp")){
        std::string iconFile = root->props.at("iconApp");

        // Obtener directorio del W3D
        std::string dir;
        size_t pos = w3dPath.find_last_of("/\\");
        if(pos != std::string::npos) dir = w3dPath.substr(0,pos+1);
        else dir = "";

        std::string iconPath = dir + iconFile;

        std::cout << "[BuildScene] Icon path: " << iconPath << std::endl;

        SDL_Surface* icon = IMG_Load(iconPath.c_str());
        if(icon){
            SDL_SetWindowIcon(window, icon);
            SDL_FreeSurface(icon);
        } else {
            std::cerr << "Warning: no se pudo abrir icono: " << iconPath << std::endl;
        }
    }
    //por defecto
    else {
        SDL_Surface* icon = IMG_Load("Whisk3D.png");
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    }

    if(root->props.count("fullscreen")){
        std::string v = root->props.at("fullscreen");
        cfg.fullscreen = (v == "true" || v == "1");
        SetFullScreen(cfg.fullscreen);
    }

    if(root->props.count("Antialiasing")){
        std::string v = root->props.at("Antialiasing");
        cfg.enableAntialiasing = (v == "true" || v == "1");
    }

    if(root->props.count("background")){
        std::string bg = Unquote(root->props.at("background")); // <- quita comillas si las hay
        std::replace(bg.begin(), bg.end(), ',', ' '); // reemplaza comas por espacios
        std::stringstream ss(bg);
        float r,g,b,a;
        if(ss >> r >> g >> b >> a){
            scene->SetBackground(r,g,b,a);
        } else {
            std::cerr << "Error al parsear background: '" << bg << "'\n";
        }
    }

    for(Node* n : root->children)
        BuildObjectRecursive(n, SceneCollection);  // 👈 el parent puede ser la escena global

    if(!CollectionActive) 
        CollectionActive = SceneCollection;        // seguridad
}

void OpenW3D(){
	std::ifstream f(w3dPath);
	if(!f.is_open()){
		std::cerr << "Warning: no pude abrir archivo: " << w3dPath << "  -> usando escena por defecto\n";
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
			if(!CollectionActive) CollectionActive = SceneCollection;

			if(layout && layout->children.size()>0)
    			rootViewport = BuildLayout(layout->children[0]);
			if(!rootViewport) {
				std::cerr << "AVISO: rootViewport no creado, usando por defecto\n";
				rootViewport = new ViewportColumn(new Outliner(), new Viewport3D(), 0.2f);
			}
		}
	}

    //esto es para recargar los targets de todos los objetos modificadores
    SearchLoop();
    SceneCollection->ReloadAll();
}