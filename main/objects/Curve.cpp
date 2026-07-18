#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "Curve.h"
#include <algorithm>
#include "WhiskUI/theme/colores.h"
#ifdef W3D_SYMBIAN
    #else
    #include <functional>
    #include <limits>
#endif

// ===================================================
// Constructor
// ===================================================
Curve::Curve(Object* parent, Vector3 pos)
    : Object(parent, T("Curve"), pos),
      vertexSize(0), vertex(NULL), indices(NULL), kdRoot(NULL)
{
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
}

void Curve::RenderObject() {
#ifdef W3D_SYMBIAN
    if (!vertex || vertexSize < 2) return;
    const float* c;
    if (ObjActivo == this && select) c = ListaColores[static_cast<int>(ColorID::accent)];
    else if (select)                 c = ListaColores[static_cast<int>(ColorID::accentDark)];
    else                             c = ListaColores[static_cast<int>(ColorID::grisUI)];
    w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    GLboolean luzEstaba = w3dEngine::IsEnabled(w3dEngine::Lighting); // restaurar al salir!
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    w3dEngine::LineWidth(2);
    w3dEngine::VertexPointer3f(0, vertex);
    w3dEngine::TexCoordPointer2f(12, vertex); // dummy valido
    w3dEngine::DrawLineStrip(vertexSize);
    w3dEngine::LineWidth(1);
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (luzEstaba) w3dEngine::Enable(w3dEngine::Lighting);
    return;
#else
    if (!showOverlayGlobal || ViewFromCameraActiveGlobal) return;

    if (ObjActivo == this && select){
        w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
    }
    else if (select){
        w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
    }
    else {
        w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::grisUI)]);
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
    w3dEngine::DrawLineStripIndexed(vertexSize, indices);
#endif // !W3D_SYMBIAN
}

#ifdef W3D_SYMBIAN
// los rieles de camara usan lambdas/std::function (C++11): stubs por ahora.
// Cuando hagan falta en el telefono se reescriben con functors C++03.
KDNode* Curve::BuildKDTreeRecursive(std::vector<int>&, int) { return NULL; }
void Curve::BuildKDTree() { kdRoot = NULL; }
int Curve::FindNearest(const Vector3&) const { return -1; }
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

#ifdef W3D_SYMBIAN
bool Curve::LoadFromFile(const std::string&) {
    return false; // carga de curvas: pendiente en Symbian (usa ifstream)
}
#else
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
    if (vertex != NULL)
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
#endif // !W3D_SYMBIAN

