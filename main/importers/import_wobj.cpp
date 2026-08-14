#include "importers/import_wobj.h"
#include "io/JsonW3d.h"   // parser del sidecar <modelo>.grupos.json (grupos de vertices)
#include "objects/MallaDatos.h" // geometria compartida por ruta (dedup del re-import)
#include "io/w3dFilesystem.h"   // leer por el Core: sirve igual en disco, en el pak y en el APK
#include <sstream>

Mesh* LeerWOBJ(std::istream& file, const std::string& filename, Object* parent, bool NoMerge,
               std::vector<int>* vertToCP){
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
                FaceCorner fc;
                size_t pos1 = token.find('/');
                size_t pos2 = token.rfind('/');

                // strtol en vez de stoi: un token malformado (o la forma comun
                // "v//vn", con el campo uv VACIO) hacia tirar una excepcion sin catch
                const char* tokC = token.c_str();
                char* fin = NULL;
                long v = strtol(tokC, &fin, 10);
                if (fin == tokC) continue;   // ni siquiera hay indice de vertice: saltear
                fc.vertex = (int)v - 1;
                fc.uv = -1; fc.normal = -1;
                if (pos1 != std::string::npos && pos2 > pos1) {
                    long u = strtol(tokC + pos1 + 1, &fin, 10);
                    if (fin != tokC + pos1 + 1) fc.uv = (int)u - 1;
                }
                if (pos2 != std::string::npos) {
                    long n = strtol(tokC + pos2 + 1, &fin, 10);
                    if (fin != tokC + pos2 + 1) fc.normal = (int)n - 1;
                }

                newFace.corners.push_back(fc);
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
    if (NoMerge){
        Wobj.ConvertToES1_NoMerge(mesh);
        // con noMerge cada linea 'v' del OBJ ES el vertice de render de igual indice
        // (mapeo 1:1): el control-point del sidecar de grupos es el propio indice.
        if (vertToCP) {
            vertToCP->resize((size_t)mesh->vertexSize);
            for (int i = 0; i < mesh->vertexSize; i++) (*vertToCP)[i] = i;
        }
    }
    else {
        // sin noMerge ConvertToES1 DUPLICA cada linea 'v' en varios vertices de render
        // (splits por uv/normal/material): vertToCP guarda render -> linea 'v', el MISMO
        // contrato que usan los importadores FBX/glTF para el skinning por control-point.
        Wobj.ConvertToES1(mesh, &acumuladoVertices, &acumuladoNormales, &acumuladoUVs, vertToCP);
    }

    return mesh;
}

// ============================================================================
//  SIDECAR DE GRUPOS DE VERTICES: <modelo>.grupos.json al lado del .obj/.wobj
//  (mismo nombre, extension .grupos.json: agua.obj -> agua.grupos.json).
//  Formato:  { "agua": [i0, i1, ...], "otro": [...] }  con i = numero de LINEA
//  'v' del OBJ (0-based, en orden de archivo). Cada clave crea un VertexGroup
//  homonimo con peso 1.0. Los indices quedan en el dominio CONTROL-POINT (la
//  linea 'v'); el mapeo a vertices de RENDER vive en Mesh::vertCtrlPoint, que
//  ImportWOBJ llena SOLO cuando el sidecar existe (ver LeerWOBJ arriba). El
//  bind lua grupoVertices() hace esa traduccion una vez, en inicio().
//  OJO: los grupos viven del RE-IMPORT del .obj (el Wavefront del .w3d se
//  reimporta en cada carga); no pasan por el .w3dm del proyecto.
// ============================================================================
static void CargarGruposWOBJ(Mesh* mesh, const std::string& ruta) {
    // idem el sidecar: por el Core (pak / APK / contenedor), no con ifstream
    std::vector<unsigned char> bytes;
    if (!w3dFileSystem::ReadFileBytes(ruta, bytes) || bytes.empty()) return;
    std::string js((const char*)&bytes[0], bytes.size());
    JParser par(js.c_str(), js.size());
    JVal* root = par.Valor();
    int creados = 0;
    if (root && root->tipo == 4) {
        for (std::map<std::string, JVal*>::iterator it = root->obj.begin();
             it != root->obj.end(); ++it) {
            if (it->first.empty() || !it->second || it->second->tipo != 5) continue;
            VertexGroup* vg = new VertexGroup(it->first);
            for (size_t i = 0; i < it->second->lista.size(); i++) {
                JVal* e = it->second->lista[i];
                if (!e || e->tipo != 1) continue;
                int idx = (int)e->num;
                if (idx < 0) continue;
                vg->verts.push_back(idx);
                vg->pesos.push_back(1.0f);
            }
            if (vg->verts.empty()) { delete vg; continue; }
            mesh->vertexGroups.push_back(vg);
            creados++;
        }
    } else {
        std::cerr << "[Wobj] sidecar de grupos invalido (se esperaba un objeto JSON): " << ruta << "\n";
    }
    delete root;
    if (creados > 0 && mesh->grupoActivo < 0) mesh->grupoActivo = 0;
}

Mesh* ImportWOBJ(const std::string& filepath, Object* parent, bool NoMerge) {

    if (filepath.size() < 5 || 
    (filepath.substr(filepath.size() - 5) != ".wobj" &&
     filepath.substr(filepath.size() - 4) != ".obj")) {
        std::cerr << "Error: El archivo no es ni .obj ni .wobj: "<< filepath <<"\n";
        return nullptr;
    }

    // POR LA ABSTRACCION DEL CORE (ReadTextFile) y no con ifstream: un .obj puede
    // venir del pak embebido o de ADENTRO DEL APK, donde no existe como archivo
    // real. Con ifstream, en Android NINGUN `Wavefront { filePath: ... }` del .w3d
    // llegaba a crearse -- la app abria con la escena vacia (negro) y con los
    // constraints "sin target", porque el objeto apuntado nunca habia nacido.
    bool okObj = false;
    const std::string datosObj = w3dFileSystem::ReadTextFile(filepath, &okObj);
    if (!okObj) {
        std::cerr << "No se pudo abrir: " << filepath << "\n";
        return nullptr;
    }
    std::istringstream file(datosObj);

    int extensionSize = (filepath.substr(filepath.size() - 5) == ".wobj") ? 5 : 4;
    std::string sinExt = filepath.substr(0, filepath.size() - extensionSize);

    // sidecar OPCIONAL de grupos de vertices (ver CargarGruposWOBJ arriba). Se
    // detecta ANTES de leer: el mapeo render->control-point (vertCtrlPoint) se
    // arma durante la conversion y SOLO si hace falta (no gastar memoria en el
    // caso comun, que en Symbian importa).
    std::string rutaGrupos = sinExt + ".grupos.json";
    bool hayGrupos = w3dFileSystem::FileExists(rutaGrupos);
    std::vector<int> vertToCP;

    Mesh* mesh = LeerWOBJ(file, filepath, parent, NoMerge, hayGrupos ? &vertToCP : nullptr);

    if (!mesh) {
        std::cerr << "No se pudo cargar el objeto WOBJ\n";
        return nullptr;
    }

    std::string mtl = sinExt + ".mtl";
    if (w3dFileSystem::FileExists(mtl))
        LeerMTL(mtl, 1);

    if (hayGrupos) {
        mesh->vertCtrlPoint.swap(vertToCP);   // render -> linea 'v' (control-point)
        CargarGruposWOBJ(mesh, rutaGrupos);
    }

    // recordar de que ARCHIVO salio: el guardado del proyecto lo referencia
    if (mesh) mesh->origen = filepath;

    // bordes unicos desde las caras: sin esto el CONTORNO de seleccion (y el
    // wireframe) no existen y la malla "parece no seleccionada" (import_obj ya
    // lo hacia; esta via -los Wavefront de los proyectos .w3d- no)
    if (mesh) mesh->CalcularBordes();

    // GEOMETRIA COMPARTIDA (MallaDatos.h): mismos filePath = mismos datos. La
    // primera importacion de esta ruta registra sus arrays en el almacen; las
    // siguientes reapuntan y suman ref (el resto del Mesh sigue por instancia).
    // La identidad incluye el modo de conversion (merge/noMerge cambian los
    // arrays). Con `mallacache off` no hace nada.
    if (mesh) W3dMallaCompartirTrasImport(mesh, NoMerge ? filepath + "|nomerge" : filepath);

    return mesh;
}