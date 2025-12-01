Mesh* LeerWOBJ(std::ifstream& file, const std::string& filename, Object* parent){
    Mesh* mesh = new Mesh(parent, 0, 0, 0);
    std::string base = ExtractBaseName(filename);
    mesh->name->SetValue(mesh->SetName(base));

    Wavefront Wobj;
    Wobj.Reset();
    bool TieneVertexColor = false;

    file.clear();

    std::string line;

    while (std::getline(file, line)) {

        // -------- vertices ----------
        if (line.rfind("v ",0)==0){
            std::istringstream ss(line.substr(2));
            double x,y,z,r=1,g=1,b=1,a=1;
            ss>>x>>y>>z;
            if(ss>>r>>g>>b){ TieneVertexColor=true; ss>>a; if(!ss) a=1; }

            Wobj.vertex.insert(Wobj.vertex.end(), {(GLfloat)x,(GLfloat)y,(GLfloat)z});

            auto sat=[&](double v){ v*=255; return (unsigned char)std::clamp(v,0.0,255.0); };
            Wobj.vertexColor.insert(Wobj.vertexColor.end(),{sat(r),sat(g),sat(b),sat(a)});
        }

        // -------- normales ----------
        else if(line.rfind("vn ",0)==0){
            std::istringstream ss(line.substr(3));
            double nx,ny,nz; ss>>nx>>ny>>nz;
            auto cv=[](double v){ v=((v+1)/2)*255-128; return (signed char)std::clamp(v,-128.0,127.0); };
            Wobj.normals.insert(Wobj.normals.end(),{cv(nx),cv(ny),cv(nz)});
        }

        // -------- uv ----------
        else if(line.rfind("vt ",0)==0){
            std::istringstream ss(line.substr(3));
            float u,v; ss>>u>>v;
            Wobj.uv.push_back(u);
            Wobj.uv.push_back(1-v);
        }

        // -------- caras ----------
        else if(line.rfind("f ",0)==0){
            std::istringstream ss(line.substr(2));
            std::string t; Face f;

            while(ss>>t){
                FaceCorners c;
                size_t p1=t.find('/'), p2=t.rfind('/');
                c.vertex=stoi(t.substr(0,p1))-1;
                c.uv    = (p1 != std::string::npos && p2 > p1) ? stoi(t.substr(p1+1,p2-p1-1)) - 1 : -1;
                c.normal=(p2!=std::string::npos)?stoi(t.substr(p2+1))-1:-1;
                f.corner.push_back(c);
            }
            Wobj.faces.push_back(f);
        }

        // --- materiales ---
        else if(line.rfind("usemtl ",0)==0){
            std::string m=line.substr(7);
            Material* mat = BuscarMaterialPorNombre(m);
            if(!mat) mat=new Material(m,false,TieneVertexColor);

            MaterialGroup mg;
            mg.start=Wobj.faces.size();
            mg.startDrawn=mg.start*3;
            mg.material=mat;
            Wobj.materialsGroup.push_back(mg);
        }
    }

    // ---------------- convertir y generar Mesh* ----------------
    int av=0, an=0, au=0;
    Wobj.ConvertToES1(mesh, &av, &an, &au);

    return mesh;
}

Mesh* ImportWOBJ(const std::string& filepath, Object* parent) {

    if (filepath.size() < 5 || filepath.substr(filepath.size() - 5) != ".wobj") {
        std::cerr << "Error: El archivo no es .wobj: "<< filepath <<"\n";
        return nullptr;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "No se pudo abrir: " << filepath << "\n";
        return nullptr;
    }

    Mesh* mesh = LeerWOBJ(file, filepath, parent);
    file.close();

    if (!mesh) {
        std::cerr << "No se pudo cargar el objeto WOBJ\n";
        return nullptr;
    }

    std::string mtl = filepath.substr(0, filepath.size() - 5) + ".mtl";
    if (std::filesystem::exists(mtl))
        LeerMTL(mtl, 1);

    return mesh;
}