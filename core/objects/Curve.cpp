#include "Curve.h"

// ===================================================
// Constructor
// ===================================================
Curve::Curve(Object* parent, Vector3 pos)
    : Object(parent, "Curve", pos)
{
    IconType = static_cast<size_t>(IconType::curve);
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
Curve::~Curve() {
    delete[] vertex;
    delete name;
}

void Curve::RenderObject() {
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;

    if (ObjActivo == this && select){
        glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0],
                  ListaColores[static_cast<int>(ColorID::accent)][1],
                  ListaColores[static_cast<int>(ColorID::accent)][2],
                  ListaColores[static_cast<int>(ColorID::accent)][3]);
    }
    else if (select){
        glColor4f(ListaColores[static_cast<int>(ColorID::accentDark)][0],
                  ListaColores[static_cast<int>(ColorID::accentDark)][1],
                  ListaColores[static_cast<int>(ColorID::accentDark)][2],
                  ListaColores[static_cast<int>(ColorID::accentDark)][3]);
    }
    else {
        glColor4f(ListaColores[static_cast<int>(ColorID::grisUI)][0],
                  ListaColores[static_cast<int>(ColorID::grisUI)][1],
                  ListaColores[static_cast<int>(ColorID::grisUI)][2],
                  ListaColores[static_cast<int>(ColorID::grisUI)][3]);
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glLineWidth(2);

    glVertexPointer(3, GL_FLOAT, 0, vertex);
    glDrawElements(GL_LINE_STRIP, vertexSize, GL_UNSIGNED_SHORT, indices);
}

KDNode* Curve::BuildKDTreeRecursive(std::vector<int>& idx, int depth){
    if (idx.empty()) return nullptr;

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

        KDNode* near = delta < 0 ? node->left : node->right;
        KDNode* far  = delta < 0 ? node->right : node->left;

        search(near, depth+1);

        if (delta*delta < bestDist)
            search(far, depth+1);
    };

    search(kdRoot, 0);
    return bestIndex;
}

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

bool Curve::LoadFromFile(const std::string& filepath){
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: No se pudo abrir el archivo: " << filepath << std::endl;
        return false;
    }

    std::string line;

    // ============================
    // 1) Leer la línea "count X"
    // ============================
    if (!std::getline(file, line)) {
        std::cerr << "ERROR: Archivo vacío." << std::endl;
        return false;
    }

    std::stringstream ss(line);
    std::string word;
    ss >> word;   // "count"

    if (word != "count") {
        std::cerr << "ERROR: Formato inválido. Se esperaba 'count'." << std::endl;
        return false;
    }

    ss >> vertexSize;  // cantidad de vértices

    if (vertexSize <= 0) {
        std::cerr << "ERROR: Cantidad de vértices inválida." << std::endl;
        return false;
    }

    // Reservar memoria (3 floats por vértice)
    if (vertex != nullptr)
        delete[] vertex;

    vertex = new GLfloat[vertexSize * 3];

    // ============================
    // 2) Leer línea por línea: "p x y z"
    // ============================

    int loaded = 0;

    while (std::getline(file, line) && loaded < vertexSize){
        if (line.size() < 2) continue; // evitar líneas vacías

        std::stringstream ls(line);
        char type;
        float x, y, z;

        ls >> type >> x >> y >> z;

        if (type != 'p')
            continue; // ignoramos líneas que no empiezan con "p"

        vertex[loaded * 3 + 0] = x;
        vertex[loaded * 3 + 1] = y;
        vertex[loaded * 3 + 2] = -z;

        loaded++;
    }

    file.close();

    if (loaded != vertexSize) {
        std::cerr << "ADVERTENCIA: Se esperaban " << vertexSize
                  << " vértices pero solo se leyeron " << loaded << std::endl;
        vertexSize = loaded;
    }

    indices = new GLushort[vertexSize];
    for (int i=0; i < vertexSize; i++)
        indices[i] = i;

    std::cout << "Curva cargada: " << vertexSize << " vértices." << std::endl;

    BuildKDTree();

    return true;
}
