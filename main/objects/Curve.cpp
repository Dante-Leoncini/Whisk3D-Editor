#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "Curve.h"
#include "io/w3dFilesystem.h"   // leer el .cap por el Core (disco / pak / APK)
#include <sstream>
#include <algorithm>
#include "WhiskUI/theme/colores.h"
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
      vertexSize(0), vertex(NULL), indices(NULL), rotNodo(NULL), fovNodo(NULL),
      signoZ(-1.0f), aspecto(0.0f), kdRoot(NULL)
{
    cargasHandle = -1;   // sin lista de carga hasta CargarListaCarga

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
static void CurveBorrarKD(KDNode* n){
    if (!n) return;
    CurveBorrarKD(n->left);
    CurveBorrarKD(n->right);
    delete n;
}
Curve::~Curve() {
    delete[] vertex;
    delete[] indices;          // (antes se fugaban indices y el arbol KD)
    delete[] rotNodo;           // canales opcionales del riel (pueden ser NULL)
    delete[] fovNodo;
    CurveBorrarKD(kdRoot);
}

// interpolacion angular POR ARCO CORTO, en grados (el yaw del riel cruza el wrap
// 0/360 seguido: 442 de 9672 transiciones en un riel real medido).
static float CurvaLerpAng(float a, float b, float t) {
    float d = b - a;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return a + d * t;
}

bool Curve::RotacionEnNodo(float indice, Vector3* pitchYawRoll, float* fovOut) const {
    if (!rotNodo || vertexSize <= 0) return false;
    const int ultimo = vertexSize - 1;
    if (indice < 0.0f) indice = 0.0f;
    if (indice > (float)ultimo) indice = (float)ultimo;
    int base = (int)indice;                    // floor: indice ya es >= 0
    if (base > ultimo) base = ultimo;
    float t = indice - (float)base;
    int sig = (base + 1 <= ultimo) ? base + 1 : ultimo;
    if (pitchYawRoll) {
        pitchYawRoll->x = CurvaLerpAng(rotNodo[base*3+0], rotNodo[sig*3+0], t);
        pitchYawRoll->y = CurvaLerpAng(rotNodo[base*3+1], rotNodo[sig*3+1], t);
        pitchYawRoll->z = CurvaLerpAng(rotNodo[base*3+2], rotNodo[sig*3+2], t);
    }
    // el FOV es un numero comun (no un angulo con wrap): lerp lineal
    if (fovOut && fovNodo) *fovOut = fovNodo[base] * (1.0f - t) + fovNodo[sig] * t;
    return true;
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
    if (!vertex || vertexSize < 2) return; // curva vacia (LoadFromFile fallido): nada que dibujar (como la rama Symbian)

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
    // POR LA ABSTRACCION DEL CORE, no con ifstream: el riel (.cap) puede venir del
    // pak embebido o de ADENTRO DEL APK. Con ifstream, en Android la camara se
    // quedaba SIN RIEL -- que es exactamente el sintoma de "abre en negro con el
    // HUD encima" que ya se habia pagado una vez en el .deb.
    bool okCap = false;
    const std::string datosCap = w3dFileSystem::ReadTextFile(filepath, &okCap);
    if (!okCap) {
        std::cerr << "ERROR: No se pudo abrir el archivo: " << filepath << std::endl;
        return false;
    }
    std::istringstream file(datosCap);

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

    // Reservar memoria (3 floats por vertice). Tambien liberar indices/arbol KD
    // de una carga anterior (antes cada recarga los fugaba).
    if (vertex != NULL)
        delete[] vertex;
    delete[] indices; indices = NULL;
    delete[] rotNodo; rotNodo = NULL;
    delete[] fovNodo; fovNodo = NULL;
    CurveBorrarKD(kdRoot); kdRoot = NULL;
    signoZ = -1.0f;   // default legacy; lo cambia la cabecera OPCIONAL `ejes` (ver Curve.h)
    aspecto = 0.0f;   // encuadre autoral: lo declara la cabecera OPCIONAL `aspecto` (ver Curve.h)

    vertex = new GLfloat[vertexSize * 3];

    // ============================
    // 2) Leer línea por línea: "p x y z", y los canales OPCIONALES del riel
    //    ("r pitch yaw roll" / "f fov"), que van DESPUES de su "p" y valen para
    //    el nodo recien leido. Un .cap sin ellos se lee exactamente como antes.
    // ============================

    int loaded = 0;
    int conRot = 0, conFov = 0;

    // OJO CON LA CONDICION DE CORTE: NO se puede parar apenas loaded == vertexSize. Las
    // lineas 'r'/'f' del ULTIMO nodo van DESPUES de su 'p', asi que cortar ahi dejaba al
    // ultimo sin rotacion ("el riel trae rotacion en 4 de 5 nodos"). Se recorre el archivo
    // entero y lo que se acota es cuantos PUNTOS se aceptan.
    while (std::getline(file, line)){
        if (line.size() < 2) continue; // evitar líneas vacías

        // CABECERA OPCIONAL DE EJES: "ejes x y -z" (o "ejes x y z"). Va suelta,
        // tipicamente justo despues del `count`. Sin ella queda el default legacy.
        // Se compara la palabra entera para no confundirla con un punto 'e...'.
        if (line.compare(0, 4, "ejes") == 0 &&
            (line.size() == 4 || line[4] == ' ' || line[4] == '\t')) {
            std::stringstream es(line);
            std::string pal, ex, ey, ez;
            es >> pal >> ex >> ey >> ez;
            signoZ = (ez == "-z" || ez == "-Z") ? -1.0f : 1.0f;
            continue;
        }

        // CABECERA OPCIONAL DE ENCUADRE: "aspecto <ancho/alto>" (ej: 1.481481 =
        // 40:27). Es el aspecto AUTORAL del riel (ver Curve.h). Palabra entera,
        // como `ejes`, para no confundirla con un punto 'a...'.
        if (line.compare(0, 7, "aspecto") == 0 &&
            (line.size() == 7 || line[7] == ' ' || line[7] == '\t')) {
            std::stringstream as(line);
            std::string pal; float v = 0.0f;
            as >> pal >> v;
            aspecto = (v > 0.01f && v < 100.0f) ? v : 0.0f;
            continue;
        }

        std::stringstream ls(line);
        char type;
        float x, y, z;

        ls >> type;

        if (type == 'r') {                       // rotacion AUTORAL del nodo anterior
            if (loaded <= 0) continue;           // una 'r' antes del primer punto no es de nadie
            y = z = 0.0f; x = 0.0f;
            ls >> x >> y >> z;
            if (!rotNodo) {                          // primera 'r': recien ahi se paga la memoria
                rotNodo = new GLfloat[vertexSize * 3];
                for (int i = 0; i < vertexSize * 3; i++) rotNodo[i] = 0.0f;
            }
            rotNodo[(loaded - 1) * 3 + 0] = x;
            rotNodo[(loaded - 1) * 3 + 1] = y;
            rotNodo[(loaded - 1) * 3 + 2] = z;
            conRot++;
            continue;
        }
        if (type == 'f') {                       // fov del nodo anterior
            if (loaded <= 0) continue;
            x = 0.0f; ls >> x;
            if (!fovNodo) {
                fovNodo = new GLfloat[vertexSize];
                for (int i = 0; i < vertexSize; i++) fovNodo[i] = 0.0f;
            }
            fovNodo[loaded - 1] = x;
            conFov++;
            continue;
        }

        if (type != 'p')
            continue; // ignoramos líneas que no empiezan con "p"
        if (loaded >= vertexSize)
            continue; // el 'count' de la cabecera manda: los puntos de mas se descartan

        ls >> x >> y >> z;

        vertex[loaded * 3 + 0] = x;
        vertex[loaded * 3 + 1] = y;
        vertex[loaded * 3 + 2] = signoZ * z;   // ver Curve::signoZ

        loaded++;
    }

    // (el buffer se libera solo: ya no hay archivo abierto que cerrar)

    // canales incompletos: no se adivina nada. Si el .cap trae rotacion para
    // algunos nodos y para otros no, los que falten quedan en 0 (y se avisa).
    if (rotNodo && conRot != loaded)
        std::cerr << "ADVERTENCIA: el riel trae rotacion en " << conRot << " de "
                  << loaded << " nodos" << std::endl;
    if (fovNodo && conFov != loaded)
        std::cerr << "ADVERTENCIA: el riel trae fov en " << conFov << " de "
                  << loaded << " nodos" << std::endl;

    if (loaded != vertexSize) {
        std::cerr << "ADVERTENCIA: Se esperaban " << vertexSize
                  << " vértices pero solo se leyeron " << loaded << std::endl;
        vertexSize = loaded;
    }

    indices = new GLushort[vertexSize];
    for (int i=0; i < vertexSize; i++)
        indices[i] = i;

    std::cout << "Curva cargada: " << vertexSize << " vértices." << std::endl;

    origen = filepath;   // el .w3d guarda esta ruta para recargar la curva al abrir

    BuildKDTree();

    return true;
}
#endif // !W3D_SYMBIAN



// ===========================================================================
//  LISTA DE CARGA: parseo del sidecar `<riel>.cargas.json` y registro en el
//  almacen de recursos (ver formato/cargas-json.md y io/W3dRecursos.h).
//  Ids con prefijo de tipo: "textura:...", "malla:...", "anim:...", "vis:...",
//  "audio:...", "otro:..." (sin prefijo = otro). En Symbian este camino
//  todavia no corre (como LoadFromFile, que tampoco pasa por el VFS).
// ===========================================================================
#ifndef W3D_SYMBIAN
#include "io/JsonW3d.h"
#include "io/W3dRecursos.h"
#include "w3dFilesystem.h"

static int CargasTipoDe(std::string& id) {
    const size_t p = id.find(':');
    if (p == std::string::npos) return W3DREC_OTRO;
    const std::string pref = id.substr(0, p);
    int tipo = -1;
    if      (pref == "textura") tipo = W3DREC_TEXTURA;
    else if (pref == "malla")   tipo = W3DREC_MALLA;
    else if (pref == "anim")    tipo = W3DREC_ANIM;
    else if (pref == "vis")     tipo = W3DREC_LISTA_VIS;
    else if (pref == "audio")   tipo = W3DREC_AUDIO;
    else if (pref == "otro")    tipo = W3DREC_OTRO;
    if (tipo < 0) return W3DREC_OTRO;      // prefijo desconocido: id entero, tipo otro
    id = id.substr(p + 1);
    return tipo;
}

static bool CargasLeerDireccion(JVal* root, const char* clave,
                                std::vector<W3dCargasFila>& out) {
    out.clear();
    if (!root || root->tipo != 4) return false;
    std::map<std::string, JVal*>::iterator it = root->obj.find(clave);
    if (it == root->obj.end() || !it->second || it->second->tipo != 5) return true; // direccion ausente = vacia
    JVal* lista = it->second;
    // las filas vienen ORDENADAS por nodo; se materializa una fila por nodo
    // 0..max (nodo sin fila = fila vacia), asi el indice de fila ES el nodo.
    int maxNodo = -1;
    for (size_t i = 0; i < lista->lista.size(); i++) {
        JVal* f = lista->lista[i];
        if (!f || f->tipo != 4) continue;
        std::map<std::string, JVal*>::iterator jn = f->obj.find("nodo");
        if (jn == f->obj.end() || !jn->second || jn->second->tipo != 1) continue;
        const int n = (int)jn->second->num;
        if (n > maxNodo) maxNodo = n;
    }
    if (maxNodo < 0) return true;
    out.resize((size_t)maxNodo + 1);
    for (size_t i = 0; i < lista->lista.size(); i++) {
        JVal* f = lista->lista[i];
        if (!f || f->tipo != 4) continue;
        std::map<std::string, JVal*>::iterator jn = f->obj.find("nodo");
        if (jn == f->obj.end() || !jn->second || jn->second->tipo != 1) continue;
        const int n = (int)jn->second->num;
        if (n < 0 || n > maxNodo) continue;
        std::map<std::string, JVal*>::iterator ji = f->obj.find("ids");
        if (ji == f->obj.end() || !ji->second || ji->second->tipo != 5) continue;
        for (size_t k = 0; k < ji->second->lista.size(); k++) {
            JVal* e = ji->second->lista[k];
            if (!e || e->tipo != 2 || e->str.empty()) continue;   // 2 = string (JsonW3d)
            W3dCargasItem item;
            std::string id = e->str;
            item.tipo = CargasTipoDe(id);
            item.id = id;
            out[(size_t)n].push_back(item);
        }
    }
    return true;
}

bool Curve::CargarListaCarga(const std::string& ruta) {
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(ruta.c_str(), datos) || datos.empty()) {
        std::cerr << "[ListaCarga] no pude leer " << ruta << "\n";
        return false;
    }
    JParser par((const char*)&datos[0], datos.size());
    JVal* root = par.Valor();
    std::vector<W3dCargasFila> adelante, atras;
    bool ok = root && root->tipo == 4;
    if (ok) {
        CargasLeerDireccion(root, "adelante", adelante);
        CargasLeerDireccion(root, "atras", atras);
    } else {
        std::cerr << "[ListaCarga] JSON invalido: " << ruta << "\n";
    }
    delete root;
    if (!ok) return false;
    cargasHandle = W3dCargasRegistrar(name, adelante, atras);
    cargasArchivo = ruta;
    return true;
}
#else
bool Curve::CargarListaCarga(const std::string&) { return false; }
#endif
