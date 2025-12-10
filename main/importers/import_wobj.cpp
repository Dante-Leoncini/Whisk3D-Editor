#include "importers/import_wobj.h"

Mesh* LeerWOBJ(std::ifstream& file, const std::string& filename, Object* parent){
    Mesh* mesh = new Mesh(parent, Vector3(0, 0, 0));

    Wavefront Wobj;
    Wobj.Reset();
    bool TieneVertexColor = false;

    std::streampos startPos = 0;
    file.clear();
    file.seekg(startPos);

    std::string line;

    int acumuladoVertices = 0;
    int acumuladoUVs = 0;
    int acumuladoNormales = 0;

    while (std::getline(file, line)) {
        startPos = file.tellg();

        if (line.rfind("v ", 0) == 0) {
            std::istringstream ss(line.substr(2));
            double x, y, z, r, g, b, a;

            ss >> x >> y >> z;

            if (ss >> r >> g >> b) {
                TieneVertexColor = true;
                if (!(ss >> a)) a = 1.0;
            } else {
                r = g = b = 1.0;
                a = 1.0;
            }

            Wobj.vertex.push_back((GLfloat)x);
            Wobj.vertex.push_back((GLfloat)y);
            Wobj.vertex.push_back((GLfloat)z);

            auto saturar = [](double v) { 
                double n = v * 255.0; 
                if (n < 0) n = 0; 
                if (n > 255.0) n = 255.0; 
                return (unsigned char)n; 
            };

            Wobj.vertexColor.push_back(saturar(r));
            Wobj.vertexColor.push_back(saturar(g));
            Wobj.vertexColor.push_back(saturar(b));
            Wobj.vertexColor.push_back(saturar(a));

            acumuladoVertices++;
        }
        else if (line.rfind("vn ", 0) == 0) {
            std::istringstream ss(line.substr(3));
            double nx, ny, nz;
            ss >> nx >> ny >> nz;

            auto conv = [](double v) -> signed char {
                v = ((v + 1.0) / 2.0) * 255.0 - 128.0;
                if (v > 127) v = 127;
                if (v < -128) v = -128;
                return (signed char)v;
            };

            Wobj.normals.push_back(conv(nx));
            Wobj.normals.push_back(conv(ny));
            Wobj.normals.push_back(conv(nz));
            acumuladoNormales++;
        }
        else if (line.rfind("vt ", 0) == 0) {
            std::istringstream ss(line.substr(3));
            double u, v;
            ss >> u >> v;

            Wobj.uv.push_back((float)u);
            Wobj.uv.push_back(1.0f - (float)v);
            acumuladoUVs++;
        }
        else if (line.rfind("f ", 0) == 0) {
            std::istringstream ss(line.substr(2));
            std::string token;
            Face newFace;

            while (ss >> token) {
                FaceCorners fc;
                size_t pos1 = token.find('/');
                size_t pos2 = token.rfind('/');

                fc.vertex = std::stoi(token.substr(0, pos1)) - 1;
                fc.uv = (pos1 != std::string::npos && pos2 > pos1) ? std::stoi(token.substr(pos1+1, pos2-pos1-1))-1 : -1;
                fc.normal = (pos2 != std::string::npos) ? std::stoi(token.substr(pos2+1))-1 : -1;

                newFace.corner.push_back(fc);
            }

            Wobj.faces.push_back(newFace);

            if (!Wobj.materialsGroup.empty()) {
                MaterialGroup& mg = Wobj.materialsGroup.back();
                mg.count++;
                mg.indicesDrawnCount += 3;
                if (mg.count == 1) mg.startDrawn = (Wobj.faces.size() - 1) * 3;
            }
        }
        else if (line.rfind("usemtl ", 0) == 0) {
            std::string matName = line.substr(7);
            Material* materialPuntero = BuscarMaterialPorNombre(matName);
            if (!materialPuntero) materialPuntero = new Material(matName, false, TieneVertexColor);

            MaterialGroup mg;
            mg.start = Wobj.faces.size();
            mg.startDrawn = Wobj.faces.size() * 3;
            mg.count = 0;
            mg.indicesDrawnCount = 0;
            mg.material = materialPuntero;
            Wobj.materialsGroup.push_back(mg);
        }
    }

    // ---------------- convertir y generar Mesh* ----------------
    Wobj.ConvertToES1(mesh, &acumuladoVertices, &acumuladoNormales, &acumuladoUVs);

    return mesh;
}

Mesh* ImportWOBJ(const std::string& filepath, Object* parent) {

    if (filepath.size() < 5 || 
    (filepath.substr(filepath.size() - 5) != ".wobj" &&
     filepath.substr(filepath.size() - 4) != ".obj")) {
        std::cerr << "Error: El archivo no es ni .obj ni .wobj: "<< filepath <<"\n";
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

    int extensionSize = (filepath.substr(filepath.size() - 5) == ".wobj") ? 5 : 4;

    std::string mtl = filepath.substr(0, filepath.size() - extensionSize) + ".mtl";
    if (fileExists(mtl))
        LeerMTL(mtl, 1);

    return mesh;
}