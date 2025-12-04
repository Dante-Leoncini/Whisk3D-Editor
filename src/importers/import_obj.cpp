#include "importers/import_obj.h"

bool VertexKey::operator==(const VertexKey &other) const {
    return pos == other.pos && normal == other.normal
           && uv == other.uv && color == other.color;
}

size_t std::hash<VertexKey>::operator()(const VertexKey &k) const {
    return ((size_t)k.pos * 73856093) ^ ((size_t)k.normal * 19349663)
           ^ ((size_t)k.uv * 83492791) ^ ((size_t)k.color * 49979693);
}

void Wavefront::Reset() {
    vertex.clear();
    vertexColor.clear();
    normals.clear();
    uv.clear();
    faces.clear();
    materialsGroup.clear();
    facesSize = 0;
    facesCount = 0;
}

void Wavefront::ConvertToES1(Mesh* TempMesh, int* acumuladoVertices, int* acumuladoNormales, int* acumuladoUVs) {
    std::vector<GLfloat> newVertices;
    std::vector<GLubyte> newColors;
    std::vector<GLbyte> newNormals;
    std::vector<GLfloat> newUVs;
    std::vector<GLushort> newFaces;

    std::unordered_map<VertexKey, GLushort> vertexMap;

    for (size_t i = 0; i < faces.size(); i++) {
        Face &f = faces[i];
        if (f.corner.size() < 3) continue;

        for (size_t t = 1; t < f.corner.size() - 1; t++) {
            FaceCorners corners[3] = {f.corner[0], f.corner[t], f.corner[t+1]};
            for (int c = 0; c < 3; c++) {
                VertexKey key = {corners[c].vertex, corners[c].normal, corners[c].uv, corners[c].vertex};

                auto it = vertexMap.find(key);
                GLushort idx;
                if (it != vertexMap.end()) {
                    idx = it->second;
                } else {
                    idx = newVertices.size() / 3;
                    vertexMap[key] = idx;

                    for (int v = 0; v < 3; v++) {
                        newVertices.push_back(vertex[corners[c].vertex*3+v]);
                        newNormals.push_back(normals[corners[c].normal*3+v]);
                    }
                    for (int v = 0; v < 4; v++) {
                        newColors.push_back(vertexColor[corners[c].vertex*4+v]);
                    }
                    for (int u = 0; u < 2; u++) {
                        newUVs.push_back(uv[corners[c].uv*2+u]);
                    }
                }
                newFaces.push_back(idx);
            }
        }
    }

    // Asignar a TempMesh
    TempMesh->vertexSize = newVertices.size();
    TempMesh->vertex = new GLfloat[newVertices.size()];
    std::copy(newVertices.begin(), newVertices.end(), TempMesh->vertex);

    TempMesh->normals = new GLbyte[newNormals.size()];
    std::copy(newNormals.begin(), newNormals.end(), TempMesh->normals);

    TempMesh->vertexColor = new GLubyte[newColors.size()];
    std::copy(newColors.begin(), newColors.end(), TempMesh->vertexColor);

    TempMesh->uv = new GLfloat[newUVs.size()];
    std::copy(newUVs.begin(), newUVs.end(), TempMesh->uv);

    TempMesh->facesSize = newFaces.size();
    TempMesh->faces = new GLushort[newFaces.size()];
    std::copy(newFaces.begin(), newFaces.end(), TempMesh->faces);

    // MaterialGroup
    if (!materialsGroup.empty()) {
        for (const MaterialGroup& mgOrig : materialsGroup) {
            MaterialGroup mg;
            mg.start = mgOrig.start;
            mg.count = mgOrig.count;
            mg.startDrawn = mgOrig.startDrawn;
            mg.indicesDrawnCount = mgOrig.indicesDrawnCount;
            mg.material = mgOrig.material;
            TempMesh->materialsGroup.push_back(mg);
        }
    } else {
        MaterialGroup mg;
        mg.start = 0;
        mg.count = faces.size();
        mg.startDrawn = 0;
        mg.indicesDrawnCount = TempMesh->facesSize;
        mg.material = 0;
        TempMesh->materialsGroup.push_back(mg);
    }

    std::cout << "\n[ConvertToES1 RESULTADO]\n";
    std::cout << "Vertices finales: " << newVertices.size()/3 << "\n";
    std::cout << "Normales final:   " << newNormals.size()/3 << "\n";
    std::cout << "UV final:         " << newUVs.size()/2 << "\n";
    std::cout << "Caras triangulos: " << newFaces.size()/3 << "\n";

    Reset();
}

// extraer nombre base del filename (sin path ni extensión)
std::string ExtractBaseName(const std::string& filepath) {
    // quitar ruta
    size_t pos = filepath.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? filepath : filepath.substr(pos + 1);
    // quitar extensión (la última '.')
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

bool LeerOBJ(std::ifstream& file,
             const std::string& filename,
             std::streampos& startPos,
             int* acumuladoVertices,
             int* acumuladoNormales,
             int* acumuladoUVs) 
{
    Mesh* mesh = new Mesh(CollectionActive, 0, 0, 0);

    // usar filename para nombrar por defecto (y hacerlo único)
    std::string fileBase = ExtractBaseName(filename);
    mesh->name->SetValue(mesh->SetName(fileBase));

    Wavefront Wobj;
    Wobj.Reset();

    bool NombreEncontrado = false;
    bool hayMasObjetos = false;
    bool TieneVertexColor = false;

    int acumuladoVerticesProximo = 0;
    int acumuladoNormalesProximo = 0;
    int acumuladoUVsProximo = 0;

    file.clear();
    file.seekg(startPos);

    std::string line;
    while (std::getline(file, line)) {
        startPos = file.tellg();

        if (line.rfind("o ", 0) == 0) {
            if (!NombreEncontrado) {
                NombreEncontrado = true;
            } else {
                hayMasObjetos = true;
                break;
            }
        }
        else if (line.rfind("v ", 0) == 0) {
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

            acumuladoVerticesProximo++;
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
            acumuladoNormalesProximo++;
        }
        else if (line.rfind("vt ", 0) == 0) {
            std::istringstream ss(line.substr(3));
            double u, v;
            ss >> u >> v;

            Wobj.uv.push_back((float)u);
            Wobj.uv.push_back(1.0f - (float)v);
            acumuladoUVsProximo++;
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

    std::cout << "DEBUG Wobj:\n";
    std::cout << "  vertices = " << Wobj.vertex.size() / 3 << "\n";
    std::cout << "  normals  = " << Wobj.normals.size() / 3 << "\n";
    std::cout << "  uv       = " << Wobj.uv.size() / 2 << "\n";
    std::cout << "  faces    = " << Wobj.faces.size() / 3 << "\n\n";

    Wobj.ConvertToES1(mesh, acumuladoVertices, acumuladoNormales, acumuladoUVs);

    *acumuladoVertices += acumuladoVerticesProximo;
    *acumuladoNormales += acumuladoNormalesProximo;
    *acumuladoUVs += acumuladoUVsProximo;
    return hayMasObjetos;
}

bool LeerMTL(const std::string& filepath, int objetosCargados) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error al abrir: " << filepath << std::endl;
        return false;
    }

    std::string line;
    Material* mat = nullptr;
    bool HaytexturasQueCargar = false;

    while (std::getline(file, line)) {
        if (line.rfind("newmtl ", 0) == 0) {
            std::string matName = line.substr(7);
            mat = BuscarMaterialPorNombre(matName);

            if (!mat) {
                std::cout << "LeerMTL: Material no encontrado! " << matName << std::endl;
                mat = new Material(matName);
                Materials.push_back(mat);
            }
        }
        else if (mat) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "Ns") {
                float ns; iss >> ns;
                float value = ns / 1000.0f;
                mat->specular[0] = mat->specular[1] = mat->specular[2] = mat->specular[3] = value;
            }
            else if (prefix == "Kd") {
                iss >> mat->diffuse[0] >> mat->diffuse[1] >> mat->diffuse[2];
            }
            else if (prefix == "Ke") {
                iss >> mat->emission[0] >> mat->emission[1] >> mat->emission[2];
            }
            else if (prefix == "d") {
                float d; iss >> d;
                mat->diffuse[3] = d;
                mat->transparent = (d < 1.0f);
            }
            else if (prefix == "map_Kd") {
                std::string texfile;
                iss >> texfile;
                std::string absPath = getParentPath(filepath) + "/" + texfile;
                std::replace(absPath.begin(), absPath.end(), '\\', '/');

                Texture newTex;
                newTex.path = absPath;

                GLuint texid;
                if (LoadTexture(absPath.c_str(), texid)) {
                    newTex.iID = texid;
                    Textures.push_back(newTex);
                    mat->textura = true;
                    mat->textureID = texid;
                } else {
                    std::cerr << "Error cargando textura: " << absPath << "\n";
                }
            }
            else if (prefix == "BackfaceCullingOff") {
                mat->culling = false;
            }
            else if (prefix == "NoLight") {
                mat->lighting = false;
            }
            else if (prefix == "CLAMP_TO_EDGE") {
                mat->repeat = false;
            }
            else if (prefix == "map_d") {
                mat->transparent = true;
            }
        }
    }

    if (HaytexturasQueCargar) {
        std::cout << "Se encontraron texturas para cargar." << std::endl;
    }

    return true;
}

bool ImportOBJ(const std::string& filepath) {
    // Revisar extensión
    if (filepath.size() < 4 || filepath.substr(filepath.size() - 4) != ".obj") {
        std::cerr << "Error: El archivo seleccionado no tiene la extensión .obj" << std::endl;
        return false;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error al abrir: " << filepath << std::endl;
        return false;
    }

    std::streampos startPos = 0;
    int objetosCargados = 0;
    int acumuladoVertices = 0;
    int acumuladoNormales = 0;
    int acumuladoUVs = 0;

    while (LeerOBJ(file, filepath, startPos, &acumuladoVertices, &acumuladoNormales, &acumuladoUVs)) {
        objetosCargados++;
    }

    file.close();

    // Archivo .mtl asociado
    std::string mtlFile = filepath.substr(0, filepath.size() - 4) + ".mtl";

    if (fileExists(mtlFile)) {
        if (!LeerMTL(mtlFile, objetosCargados)) {
            std::cerr << "Error al leer el archivo .mtl" << std::endl;
        }
    } else {
        std::cerr << "El archivo .mtl no existe" << std::endl;
    }

    return true;
}