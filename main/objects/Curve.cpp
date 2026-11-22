#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "Curve.h"
#include "objects/Mesh.h"       // ~Curve: liberar la malla de edicion interna ('edicion')
#include "io/w3dFilesystem.h"   // leer el .cap por el Core (disco / pak / APK)
#include <sstream>
#include <algorithm>
#include <cstdlib>   // strtod/strtol: parsear el .cap sin stringstream (rompe el locale en Symbian)
#include <cstring>   // strncmp
#include "base/w3dlog.h"   // w3dLogf: el .cap ahora carga en Symbian; sus mensajes van al log
#include "WhiskUI/theme/colors.h"
#include "variables.h" // showOverlayGlobal / ViewFromCameraActiveGlobal (estado del editor)
#ifndef W3D_SYMBIAN
    #include <functional>
    #include <limits>
#endif

// ===================================================
// Constructor
// ===================================================
Curve::Curve(Object* parent, Vector3 pos)
    : Object(parent, T("Curve"), pos),
      vertexSize(0), vertex(NULL), indices(NULL), rotNode(NULL), fovNode(NULL),
      signZ(-1.0f), aspect(0.0f), nBranches(0), edit(NULL), kdRoot(NULL)
{
    loadsHandle = -1;   // sin lista de carga hasta LoadLoadList

}

// RAMAS = componentes conexas del grafo (aristas explicitas, o la cadena implicita de la
// polilinea). BFS iterativo simple: ~1000 nodos, corre solo al cargar/terminar de editar.
void Curve::RecalculateBranches() {
    nodeBranch.assign(vertexSize > 0 ? vertexSize : 0, -1);
    nBranches = 0;
    if (vertexSize <= 0) return;
    std::vector< std::vector<int> > ady(vertexSize);
    if (!edges.empty()) {
        for (size_t k = 0; k + 1 < edges.size(); k += 2) {
            int a = edges[k], b = edges[k+1];
            if (a >= 0 && a < vertexSize && b >= 0 && b < vertexSize) { ady[a].push_back(b); ady[b].push_back(a); }
        }
    } else {
        for (int i = 0; i + 1 < vertexSize; i++) { ady[i].push_back(i+1); ady[i+1].push_back(i); }
    }
    std::vector<int> queue;
    for (int s = 0; s < vertexSize; s++) {
        if (nodeBranch[s] >= 0) continue;
        int r = nBranches++;
        queue.clear(); queue.push_back(s); nodeBranch[s] = r;
        while (!queue.empty()) {
            int c = queue.back(); queue.pop_back();
            for (size_t k = 0; k < ady[c].size(); k++)
                if (nodeBranch[ady[c][k]] < 0) { nodeBranch[ady[c][k]] = r; queue.push_back(ady[c][k]); }
        }
    }
}

// nearest con FILTRO de ramas (el modificador Oclusion elige que ramas participan:
// la del bonus no sirve al nivel principal). Lineal, comun a PC y N95.
int Curve::FindNearestFiltered(const Vector3& target, const std::vector<char>* branchesOn) const {
    bool filter = branchesOn && !branchesOn->empty() && (int)nodeBranch.size() == vertexSize;
    int best = -1; float bd = 3.4e38f;
    for (int i = 0; i < vertexSize; i++) {
        if (filter) {
            int r = nodeBranch[i];
            if (r >= 0 && r < (int)branchesOn->size() && !(*branchesOn)[r]) continue;
        }
        float dx = vertex[i*3] - target.x, dy = vertex[i*3+1] - target.y, dz = vertex[i*3+2] - target.z;
        float d = dx*dx + dy*dy + dz*dz;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// FOCO/ENCUADRE: bounding de los nodos en MUNDO (centro del AABB + radio que lo envuelve).
Vector3 Curve::PointFocus() const {
    if (!vertex || vertexSize < 1) return GetGlobalPosition();
    Vector3 mn(vertex[0], vertex[1], vertex[2]), mx = mn;
    for (int i = 1; i < vertexSize; i++) {
        if (vertex[i*3]   < mn.x) mn.x = vertex[i*3];   if (vertex[i*3]   > mx.x) mx.x = vertex[i*3];
        if (vertex[i*3+1] < mn.y) mn.y = vertex[i*3+1]; if (vertex[i*3+1] > mx.y) mx.y = vertex[i*3+1];
        if (vertex[i*3+2] < mn.z) mn.z = vertex[i*3+2]; if (vertex[i*3+2] > mx.z) mx.z = vertex[i*3+2];
    }
    return GetGlobalPosition() + (mn + mx) * 0.5f;   // nodos locales + posicion del objeto
}
float Curve::FocusRadius() const {
    if (!vertex || vertexSize < 1) return 0.0f;
    Vector3 c = PointFocus() - GetGlobalPosition();
    float r2 = 0.0f;
    for (int i = 0; i < vertexSize; i++) {
        float dx = vertex[i*3] - c.x, dy = vertex[i*3+1] - c.y, dz = vertex[i*3+2] - c.z;
        float d2 = dx*dx + dy*dy + dz*dz; if (d2 > r2) r2 = d2;
    }
    return sqrtf(r2);
}

// ===================================================
// Tipo de objeto
// ===================================================
ObjectType Curve::getType() {
    return ObjectType::curve;
}

// ===================================================
// Destructor
// ===================================================
static void CurveDeleteKD(KDNode* n){
    if (!n) return;
    CurveDeleteKD(n->left);
    CurveDeleteKD(n->right);
    delete n;
}
Curve::~Curve() {
    delete[] vertex;
    delete[] indices;          // (antes se fugaban indices y el arbol KD)
    delete[] rotNode;           // canales opcionales del riel (pueden ser NULL)
    delete[] fovNode;
    delete edit;             // la malla de edicion interna es de esta curva
    CurveDeleteKD(kdRoot);
}

// interpolacion angular POR ARCO CORTO, en grados (el yaw del riel cruza el wrap
// 0/360 seguido: 442 de 9672 transiciones en un riel real medido).
static float CurveLerpAng(float a, float b, float t) {
    float d = b - a;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return a + d * t;
}

bool Curve::RotationInNode(float index, Vector3* pitchYawRoll, float* fovOut) const {
    if (!rotNode || vertexSize <= 0) return false;
    const int last = vertexSize - 1;
    if (index < 0.0f) index = 0.0f;
    if (index > (float)last) index = (float)last;
    int base = (int)index;                    // floor: indice ya es >= 0
    if (base > last) base = last;
    float t  = index - (float)base;
    int next = (base + 1 <= last) ? base + 1 : last;
    if (pitchYawRoll) {
        pitchYawRoll->x = CurveLerpAng(rotNode[base*3+0], rotNode[next*3+0], t);
        pitchYawRoll->y = CurveLerpAng(rotNode[base*3+1], rotNode[next*3+1], t);
        pitchYawRoll->z = CurveLerpAng(rotNode[base*3+2], rotNode[next*3+2], t);
    }
    // el FOV es un numero comun (no un angulo con wrap): lerp lineal
    if (fovOut && fovNode) *fovOut = fovNode[base] * (1.0f - t) + fovNode[next] * t;
    return true;
}

void Curve::RenderObject() {
    { extern bool g_showCurves; if (!g_showCurves) return; } // toggle "Curves" del overlay: ocultar la linea del riel
#ifdef W3D_SYMBIAN
    if (!vertex || vertexSize < 2) return;
    const float* c;
    if (ActiveObj == this && select) c = ColorList[static_cast<int>(ColorID::accent)];
    else if (select)                 c = ColorList[static_cast<int>(ColorID::accentDark)];
    else                             c = ColorList[static_cast<int>(ColorID::grayUI)];
    w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    GLboolean lightWas = w3dEngine::IsEnabled(w3dEngine::Lighting); // restaurar al salir!
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::LineWidth(2);
    w3dEngine::VertexPointer3f(0, vertex);
    w3dEngine::TexCoordPointer2f(12, vertex); // dummy valido
    if (!edges.empty())                      // GRAFO con ramas: un segmento por arista
        for (size_t k = 0; k + 1 < edges.size(); k += 2)
            w3dEngine::DrawLineStripIndexed(2, &edges[k]);
    else
        w3dEngine::DrawLineStrip(vertexSize);
    w3dEngine::LineWidth(1);
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (lightWas) w3dEngine::Enable(w3dEngine::Lighting);
    return;
#else
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;
    if (!vertex || vertexSize < 2) return; // curva vacia (LoadFromFile fallido): nada que dibujar (como la rama Symbian)

    if (ActiveObj == this && select){
        w3dEngine::Color4fv(ColorList[static_cast<int>(ColorID::accent)]);
    }
    else if (select){
        w3dEngine::Color4fv(ColorList[static_cast<int>(ColorID::accentDark)]);
    }
    else {
        w3dEngine::Color4fv(ColorList[static_cast<int>(ColorID::grayUI)]);
    }

    w3dEngine::Enable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::ColorMaterial);
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Blend);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::LineWidth(2);

    w3dEngine::VertexPointer3f(0, vertex);
    if (!edges.empty())                      // GRAFO con ramas: un segmento por arista
        for (size_t k = 0; k + 1 < edges.size(); k += 2)
            w3dEngine::DrawLineStripIndexed(2, &edges[k]);
    else
        w3dEngine::DrawLineStripIndexed(vertexSize, indices);
#endif // !W3D_SYMBIAN
}

#ifdef W3D_SYMBIAN
// KD-tree deshabilitado en Symbian (la version real usa lambdas/std::function C++11). En su lugar
// FindNearest hace una busqueda LINEAL sobre los nodos del riel: son ~1000 y se corre unas pocas veces
// por frame -> trivial en el N95, y da el MISMO nodo que el arbol (nearest global por LengthSq).
// BuildKDTree queda no-op (kdRoot=NULL, no se usa). Antes FindNearest devolvia -1 (stub): con eso la
// camara con riel NUNCA se ubicaba en el telefono (Camera::UpdatePosition corta en 'if idx < 0 return').
KDNode* Curve::BuildKDTreeRecursive(std::vector<int>&, int) { return NULL; }
void Curve::BuildKDTree() { kdRoot = NULL; }
int Curve::FindNearest(const Vector3& target) const {
    int bestIndex = -1;
    float bestDist = 3.4e38f;   // ~FLT_MAX, sin <limits> (excluido en Symbian)
    for (int i = 0; i < vertexSize; i++) {
        Vector3 p(vertex[i*3+0], vertex[i*3+1], vertex[i*3+2]);
        float d = (p - target).LengthSq();
        if (d < bestDist) { bestDist = d; bestIndex = i; }
    }
    return bestIndex;
}
#else
KDNode* Curve::BuildKDTreeRecursive(std::vector<int>& idx, int depth){
    if (idx.empty()) return NULL;

    int axis = depth % 3;

    std::sort(idx.begin(), idx.end(), [&](int a, int b){
        return vertex[a*3 + axis] < vertex[b*3 + axis];
    });

    int mid = idx.size() / 2;

    KDNode* node = new KDNode();
    node->index = idx[mid];
    node->point = Vector3(
        vertex[node->index*3 + 0],
        vertex[node->index*3 + 1],
        vertex[node->index*3 + 2]
    );

    std::vector<int> left(idx.begin(), idx.begin()+mid);
    std::vector<int> right(idx.begin()+mid+1, idx.end());

    node->left  = BuildKDTreeRecursive(left, depth+1);
    node->right = BuildKDTreeRecursive(right, depth+1);

    return node;
}

void Curve::BuildKDTree(){
    std::cerr << "Creando BuildKDTree\n";
    std::vector<int> idx(vertexSize);
    for (int i=0; i<vertexSize; i++) idx[i] = i;

    kdRoot = BuildKDTreeRecursive(idx, 0);
}

int Curve::FindNearest(const Vector3& target) const{
    float bestDist = std::numeric_limits<float>::infinity();
    int bestIndex = -1;

    std::function<void(KDNode*, int)> search = [&](KDNode* node, int depth){
        if (!node) return;

        float d = (node->point - target).LengthSq();
        if (d < bestDist) {
            bestDist = d;
            bestIndex = node->index;
        }

        int axis = depth % 3;
        float delta = target[axis] - node->point[axis];

        KDNode* nearNode = delta < 0 ? node->left : node->right;
        KDNode* farNode  = delta < 0 ? node->right : node->left;

        search(nearNode, depth+1);

        if (delta * delta < bestDist)
            search(farNode, depth+1);
    };

    search(kdRoot, 0);
    return bestIndex;
}

#endif // !W3D_SYMBIAN

Vector3 Curve::GetPoint(int i) const {
    if (!vertex || i < 0 || i >= vertexSize) {
        return Vector3(0,0,0); // o lanzar error si querés
    }

    int idx = i * 3; // x,y,z
    return Vector3(
        vertex[idx + 0],
        vertex[idx + 1],
        vertex[idx + 2]
    );
}

// Carga el .cap del riel. Corre en TODAS las plataformas (antes era un stub `return false` en
// Symbian por un ifstream que ya no existe: hoy lee por w3dFileSystem, que anda en el N95). Sin esto,
// en el telefono el .cap NUNCA se abria -> la Curve se borraba -> la camara con riel quedaba en el
// origen (y el culling, que usa esa camara, fallaba con ella). El parseo es MANUAL con strtod/strtol:
// operator>> de float reinicializa el locale por linea y en Symbian (STLport/RVCT) lee BASURA.
bool Curve::LoadFromFile(const std::string& filepath){
    // POR LA ABSTRACCION DEL CORE, no con ifstream: el riel (.cap) puede venir del
    // pak embebido o de ADENTRO DEL APK. Con ifstream, en Android la camara se
    // quedaba SIN RIEL -- que es exactamente el sintoma de "abre en negro con el
    // HUD encima" que ya se habia pagado una vez en el .deb.
    bool okCap = false;
    const std::string dataCap = w3dFileSystem::ReadTextFile(filepath, &okCap);
    if (!okCap) {
        w3dLogfW("[Curve] no se pudo abrir %s", filepath.c_str());
        return false;
    }
    std::istringstream file(dataCap);

    std::string line;

    // ============================
    // 1) Leer la línea "count X"
    // ============================
    if (!std::getline(file, line)) {
        w3dLogfW("[Curve] archivo vacio: %s", filepath.c_str());
        return false;
    }

    {
        const char* p = line.c_str();
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "count", 5) != 0) {
            w3dLogfW("[Curve] formato invalido: se esperaba 'count' en %s", filepath.c_str());
            return false;
        }
        vertexSize = (int)strtol(p + 5, NULL, 10);   // cantidad de vertices
    }

    if (vertexSize <= 0) {
        w3dLogfW("[Curve] cantidad de vertices invalida en %s", filepath.c_str());
        return false;
    }

    // Reservar memoria (3 floats por vertice). Tambien liberar indices/arbol KD
    // de una carga anterior (antes cada recarga los fugaba).
    if (vertex != NULL)
        delete[] vertex;
    delete[] indices; indices = NULL;
    delete[] rotNode; rotNode = NULL;
    delete[] fovNode; fovNode = NULL;
    CurveDeleteKD(kdRoot); kdRoot = NULL;
    signZ = -1.0f;   // default legacy; lo cambia la cabecera OPCIONAL `ejes` (ver Curve.h)
    aspect = 0.0f;   // encuadre autoral: lo declara la cabecera OPCIONAL `aspecto` (ver Curve.h)

    vertex = new GLfloat[vertexSize * 3];

    // ============================
    // 2) Leer línea por línea: "p x y z", y los canales OPCIONALES del riel
    //    ("r pitch yaw roll" / "f fov"), que van DESPUES de su "p" y valen para
    //    el nodo recien leido. Un .cap sin ellos se lee exactamente como antes.
    // ============================

    int loaded  = 0;
    int withRot = 0, withFov = 0;

    // OJO CON LA CONDICION DE CORTE: NO se puede parar apenas loaded == vertexSize. Las
    // lineas 'r'/'f' del ULTIMO nodo van DESPUES de su 'p', asi que cortar ahi dejaba al
    // ultimo sin rotacion ("el riel trae rotacion en 4 de 5 nodos"). Se recorre el archivo
    // entero y lo que se acota es cuantos PUNTOS se aceptan.
    while (std::getline(file, line)){
        if (line.size() < 2) continue; // evitar líneas vacías

        // CABECERA OPCIONAL DE EJES: "ejes x y -z" (o "ejes x y z"). Va suelta,
        // tipicamente justo despues del `count`. Sin ella queda el default legacy.
        // Se compara la palabra entera para no confundirla con un punto 'e...'.
        if (line.compare(0, 4, "axes") == 0 &&
            (line.size() == 4 || line[4] == ' ' || line[4] == '\t')) {
            signZ = (line.find("-z") != std::string::npos ||
                      line.find("-Z") != std::string::npos) ? -1.0f : 1.0f;
            continue;
        }

        // CABECERA OPCIONAL DE ENCUADRE: "aspecto <ancho/alto>" (ej: 1.481481 =
        // 40:27). Es el aspecto AUTORAL del riel (ver Curve.h). Palabra entera,
        // como `ejes`, para no confundirla con un punto 'a...'.
        if (line.compare(0, 6, "aspect") == 0 &&
            (line.size() == 6 || line[6] == ' ' || line[6] == '\t')) {
            float v = (float)strtod(line.c_str() + 6, NULL);
            aspect  = (v > 0.01f && v < 100.0f) ? v : 0.0f;
            continue;
        }

        // PARSEO MANUAL con strtod (NO stringstream/operator>>: reinicializa el locale por linea y
        // en Symbian lee basura -> el riel quedaba deformado). Mismo criterio que import_wobj.cpp.
        const char* p = line.c_str();
        while (*p == ' ' || *p == '\t') p++;
        char type = *p;
        char* e; const char* q = p + 1;

        if (type == 'r') {                       // rotacion AUTORAL del nodo anterior
            if (loaded <= 0) continue;           // una 'r' antes del primer punto no es de nadie
            double rx = strtod(q, &e); double ry = strtod(e, &e); double rz = strtod(e, &e);
            if (!rotNode) {                          // primera 'r': recien ahi se paga la memoria
                rotNode = new GLfloat[vertexSize * 3];
                for (int i = 0; i < vertexSize * 3; i++) rotNode[i] = 0.0f;
            }
            rotNode[(loaded - 1) * 3 + 0] = (GLfloat)rx;
            rotNode[(loaded - 1) * 3 + 1] = (GLfloat)ry;
            rotNode[(loaded - 1) * 3 + 2] = (GLfloat)rz;
            withRot++;
            continue;
        }
        if (type == 'f') {                       // fov del nodo anterior
            if (loaded <= 0) continue;
            double fv = strtod(q, &e);
            if (!fovNode) {
                fovNode = new GLfloat[vertexSize];
                for (int i = 0; i < vertexSize; i++) fovNode[i] = 0.0f;
            }
            fovNode[loaded - 1] = (GLfloat)fv;
            withFov++;
            continue;
        }

        if (type != 'p')
            continue; // ignoramos líneas que no empiezan con "p"
        if (loaded >= vertexSize)
            continue; // el 'count' de la cabecera manda: los puntos de mas se descartan

        double px = strtod(q, &e); double py = strtod(e, &e); double pz = strtod(e, &e);
        vertex[loaded * 3 + 0] = (GLfloat)px;
        vertex[loaded * 3 + 1] = (GLfloat)py;
        vertex[loaded * 3 + 2] = signZ * (GLfloat)pz;   // ver Curve::signZ

        loaded++;
    }

    // (el buffer se libera solo: ya no hay archivo abierto que cerrar)

    // canales incompletos: no se adivina nada. Si el .cap trae rotacion para
    // algunos nodos y para otros no, los que falten quedan en 0 (y se avisa).
    if (rotNode && withRot != loaded)
        w3dLogfW("[Curve] rotacion en %d de %d nodos (los que falten quedan en 0)", withRot, loaded);
    if (fovNode && withFov != loaded)
        w3dLogfW("[Curve] fov en %d de %d nodos", withFov, loaded);

    if (loaded != vertexSize) {
        w3dLogfW("[Curve] se esperaban %d nodos pero se leyeron %d", vertexSize, loaded);
        vertexSize = loaded;
    }

    indices = new GLushort[vertexSize];
    for (int i=0; i < vertexSize; i++)
        indices[i] = i;

    w3dLogf("[Curve] cargada: %d nodos (%s)", vertexSize, filepath.c_str());

    origin = filepath;   // el .w3d guarda esta ruta para recargar la curva al abrir

    BuildKDTree();

    return true;
}



// ===========================================================================
//  LISTA DE CARGA: parseo del sidecar `<riel>.cargas.json` y registro en el
//  almacen de recursos (ver formato/cargas-json.md y io/W3dResources.h).
//  Ids con prefijo de tipo: "textura:...", "malla:...", "anim:...", "vis:...",
//  "audio:...", "otro:..." (sin prefijo = otro). En Symbian este camino
//  todavia no corre (como LoadFromFile, que tampoco pasa por el VFS).
// ===========================================================================
#ifndef W3D_SYMBIAN
#include "io/JsonW3d.h"
#include "io/W3dResources.h"
#include "w3dFilesystem.h"

static int LoadsTypeOf(std::string& id) {
    const size_t p = id.find(':');
    if (p == std::string::npos) return W3DREC_OTHER;
    const std::string pref = id.substr(0, p);
    int type = -1;
    if      (pref == "texture") type = W3DREC_TEXTURE;
    else if (pref == "mesh")   type = W3DREC_MESH;
    else if (pref == "anim")    type = W3DREC_ANIM;
    else if (pref == "vis")     type = W3DREC_LIST_VIS;
    else if (pref == "audio")   type = W3DREC_AUDIO;
    else if (pref == "other")    type = W3DREC_OTHER;
    else if (pref == "animset") type = W3DREC_ANIMSET;   // clips de esqueleto (.w3da): id = su entrada
    if (type < 0) return W3DREC_OTHER;      // prefijo desconocido: id entero, tipo otro
    id = id.substr(p + 1);
    return type;
}

static bool LoadsReadDirection(JVal* root, const char* key,
                                std::vector<W3dLoadsRow>& out) {
    out.clear();
    if (!root || root->type != 4) return false;
    std::map<std::string, JVal*>::iterator it = root->obj.find(key);
    if (it == root->obj.end() || !it->second || it->second->type != 5) return true; // direccion ausente = vacia
    JVal* list = it->second;
    // las filas vienen ORDENADAS por nodo; se materializa una fila por nodo
    // 0..max (nodo sin fila = fila vacia), asi el indice de fila ES el nodo.
    int maxNode = -1;
    for (size_t i = 0; i < list->list.size(); i++) {
        JVal* f = list->list[i];
        if (!f || f->type != 4) continue;
        std::map<std::string, JVal*>::iterator jn = f->obj.find("node");
        if (jn == f->obj.end() || !jn->second || jn->second->type != 1) continue;
        const int n = (int)jn->second->num;
        if (n > maxNode) maxNode = n;
    }
    if (maxNode < 0) return true;
    out.resize((size_t)maxNode + 1);
    for (size_t i = 0; i < list->list.size(); i++) {
        JVal* f = list->list[i];
        if (!f || f->type != 4) continue;
        std::map<std::string, JVal*>::iterator jn = f->obj.find("node");
        if (jn == f->obj.end() || !jn->second || jn->second->type != 1) continue;
        const int n = (int)jn->second->num;
        if (n < 0 || n > maxNode) continue;
        std::map<std::string, JVal*>::iterator ji = f->obj.find("ids");
        if (ji == f->obj.end() || !ji->second || ji->second->type != 5) continue;
        for (size_t k = 0; k < ji->second->list.size(); k++) {
            JVal* e = ji->second->list[k];
            if (!e || e->type != 2 || e->str.empty()) continue;   // 2 = string (JsonW3d)
            W3dLoadsItem item;
            std::string id = e->str;
            item.type = LoadsTypeOf(id);
            item.id = id;
            out[(size_t)n].push_back(item);
        }
    }
    return true;
}

bool Curve::LoadLoadList(const std::string& path) {
    std::vector<unsigned char> data;
    if (!w3dFileSystem::ReadFileBytes(path.c_str(), data) || data.empty()) {
        std::cerr << "[ListaCarga] no pude leer " << path << "\n";
        return false;
    }
    JParser pair((const char*)&data[0], data.size());
    JVal* root = pair.Value();
    std::vector<W3dLoadsRow> forward, back;
    bool ok = root && root->type == 4;
    if (ok) {
        LoadsReadDirection(root, "forward", forward);
        LoadsReadDirection(root, "back", back);
    } else {
        std::cerr << "[ListaCarga] JSON invalido: " << path << "\n";
    }
    delete root;
    if (!ok) return false;
    loadsHandle = W3dLoadsRegister(name, forward, back);
    loadsFile = path;
    return true;
}
#else
bool Curve::LoadLoadList(const std::string&) { return false; }
#endif
