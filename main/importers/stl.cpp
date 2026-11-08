// STL: importador (binario y ASCII) y exportador (binario). Ver stl.h.
#include "stl.h"
#include "import_obj.h"                     // Wavefront / Face / FaceCorner / ExtractBaseName / MeshShadingImportadoEsSmooth
#include "W3dLang.h"                        // T(): los avisos en el idioma del editor
#include "w3dFilesystem.h"                  // la lectura de archivos del Core (un solo camino por plataforma)
#include "w3dlog.h"
#include "ViewPorts/PopUp/ProgressPopup.h"  // la barra de progreso (en el N95 un STL grande tarda)
#include "ViewPorts/LayoutInput.h"          // Notificar
#include "io/W3dWeb.h"                      // W3dDescargarSiWeb: en el navegador el archivo baja a la PC
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <map>
#include <vector>

namespace {

struct Tri { float n[3]; float v[3][3]; };

// Z arriba (STL) <-> Y arriba (Whisk3D)
inline void DeSTL(const float* s, float* w)  { w[0] = s[0]; w[1] = s[2];  w[2] = -s[1]; }
inline void AlSTL(const float* w, float* s)  { s[0] = w[0]; s[1] = -w[2]; s[2] = w[1]; }

inline unsigned int LeerU32(const unsigned char* p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}
inline float LeerF32(const unsigned char* p) {   // little-endian (el STL binario lo es siempre)
    unsigned int u = LeerU32(p); float f; std::memcpy(&f, &u, 4); return f;
}
inline void EscribirU32(std::string& out, unsigned int u) {
    char b[4] = { (char)(u & 255), (char)((u >> 8) & 255), (char)((u >> 16) & 255), (char)((u >> 24) & 255) };
    out.append(b, 4);
}
inline void EscribirF32(std::string& out, float f) { unsigned int u; std::memcpy(&u, &f, 4); EscribirU32(out, u); }

// la normal de un triangulo (sin normalizar si es degenerado: devuelve false)
bool NormalDe(const float* a, const float* b, const float* c, float* n) {
    float u[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    float v[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    n[0] = u[1] * v[2] - u[2] * v[1];
    n[1] = u[2] * v[0] - u[0] * v[2];
    n[2] = u[0] * v[1] - u[1] * v[0];
    float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (l < 1e-20f) return false;
    n[0] /= l; n[1] /= l; n[2] /= l;
    return true;
}

// ---- lectura ----------------------------------------------------------------------------------------------------
struct Solido { std::string nombre; std::vector<Tri> tris; };

bool EsBinario(const std::vector<unsigned char>& d) {
    if (d.size() < 84) return false;
    const unsigned int n = LeerU32(&d[80]);
    if ((unsigned long long)84 + (unsigned long long)n * 50ull == (unsigned long long)d.size()) return true;
    // no cuadra el largo: si empieza con "solid" es ASCII (los binarios mal hechos tambien pueden empezar asi, pero
    // entonces el largo cuadraria); si no, binario igual (truncado o con basura al final)
    size_t i = 0;
    while (i < d.size() && (d[i] == ' ' || d[i] == '\t' || d[i] == '\r' || d[i] == '\n')) i++;
    return !(d.size() - i >= 5 && std::memcmp(&d[i], "solid", 5) == 0);
}

void LeerBinario(const std::vector<unsigned char>& d, std::vector<Solido>& sol) {
    unsigned int n = LeerU32(&d[80]);
    const unsigned int caben = (unsigned int)((d.size() - 84) / 50);
    if (n > caben) n = caben;
    sol.push_back(Solido());
    std::vector<Tri>& tris = sol.back().tris;
    tris.reserve(n);
    for (unsigned int t = 0; t < n; t++) {
        const unsigned char* p = &d[84 + (size_t)t * 50];
        Tri tr;
        for (int k = 0; k < 3; k++) tr.n[k] = LeerF32(p + 4 * k);
        for (int v = 0; v < 3; v++) for (int k = 0; k < 3; k++) tr.v[v][k] = LeerF32(p + 12 + 12 * v + 4 * k);
        tris.push_back(tr);
        if ((t & 8191) == 0) ProgresoActualizar(0.5f * (float)t / (float)(n ? n : 1));
    }
}

// la palabra siguiente (sin espacios); avanza p
std::string Palabra(const char*& p, const char* fin) {
    while (p < fin && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    const char* a = p;
    while (p < fin && !(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    return std::string(a, p);
}
float Numero(const char*& p, const char* fin) {
    std::string w = Palabra(p, fin);
    return (float)std::strtod(w.c_str(), NULL);
}

void LeerASCII(const std::vector<unsigned char>& d, std::vector<Solido>& sol) {
    const char* p = (const char*)&d[0];
    const char* fin = p + d.size();
    Tri tr; int nv = 0; bool hayNormal = false;
    while (p < fin) {
        std::string w = Palabra(p, fin);
        if (w.empty()) break;
        if (w == "solid") {
            // el nombre es el resto de la linea
            while (p < fin && (*p == ' ' || *p == '\t')) p++;
            const char* a = p;
            while (p < fin && *p != '\n' && *p != '\r') p++;
            std::string nom(a, p);
            while (!nom.empty() && (nom[nom.size() - 1] == ' ' || nom[nom.size() - 1] == '\t')) nom.erase(nom.size() - 1);
            sol.push_back(Solido());
            sol.back().nombre = nom;
        } else if (w == "facet") {
            if (sol.empty()) sol.push_back(Solido());
            const char* q = p;
            if (Palabra(q, fin) == "normal") {
                p = q;
                for (int k = 0; k < 3; k++) tr.n[k] = Numero(p, fin);
                hayNormal = true;
            }
            nv = 0;
        } else if (w == "vertex") {
            if (sol.empty()) sol.push_back(Solido());
            float v[3];
            for (int k = 0; k < 3; k++) v[k] = Numero(p, fin);
            if (nv < 3) { for (int k = 0; k < 3; k++) tr.v[nv][k] = v[k]; }
            nv++;
            if (nv == 3) {
                if (!hayNormal) tr.n[0] = tr.n[1] = tr.n[2] = 0.0f;
                sol.back().tris.push_back(tr);
                if ((sol.back().tris.size() & 8191) == 0) ProgresoActualizar(0.5f * (float)(p - (const char*)&d[0]) / (float)d.size());
            }
        } else if (w == "endfacet") {
            nv = 0; hayNormal = false;
        }
        // outer loop / endloop / endsolid: nada
    }
}

// ---- armar la malla ---------------------------------------------------------------------------------------------
struct ClavePos {
    float x, y, z;
    bool operator<(const ClavePos& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
};
struct ClaveNormal {
    signed char x, y, z;
    bool operator<(const ClaveNormal& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
};
inline signed char ByteNormal(float v) {
    float r = v * 127.0f;
    if (r > 127.0f) r = 127.0f;
    if (r < -127.0f) r = -127.0f;
    return (signed char)(r < 0 ? r - 0.5f : r + 0.5f);
}

// una malla con los triangulos de un solido. NULL si no tiene ninguno (o, en el N95, si no entra).
Mesh* MallaDe(const Solido& s, const std::string& nombre) {
    Wavefront w;
    w.Reset();
    std::map<ClavePos, int> posiciones;
    std::map<ClaveNormal, int> normales;
    for (size_t t = 0; t < s.tris.size(); t++) {
        const Tri& tr = s.tris[t];
        float v[3][3];
        for (int i = 0; i < 3; i++) DeSTL(tr.v[i], v[i]);
        float n[3];
        if (!NormalDe(v[0], v[1], v[2], n)) {
            // degenerado (area 0): la normal del archivo, si trae; si no, se saltea (no dibuja nada)
            float ns[3] = { tr.n[0], tr.n[1], tr.n[2] };
            float l = std::sqrt(ns[0] * ns[0] + ns[1] * ns[1] + ns[2] * ns[2]);
            if (l < 1e-20f) continue;
            for (int k = 0; k < 3; k++) ns[k] /= l;
            DeSTL(ns, n);
        }
        // la normal de la CARA (calculada desde los vertices: muchos STL la traen en 0 o mal) -> sombreado plano
        ClaveNormal cn = { ByteNormal(n[0]), ByteNormal(n[1]), ByteNormal(n[2]) };
        std::map<ClaveNormal, int>::iterator itn = normales.find(cn);
        int in;
        if (itn == normales.end()) {
            in = (int)(w.normals.size() / 3);
            w.normals.push_back(cn.x); w.normals.push_back(cn.y); w.normals.push_back(cn.z);
            normales[cn] = in;
        } else in = itn->second;
        Face f;
        for (int i = 0; i < 3; i++) {
            ClavePos cp = { v[i][0], v[i][1], v[i][2] };
            std::map<ClavePos, int>::iterator it = posiciones.find(cp);
            int ip;
            if (it == posiciones.end()) {   // los vertices iguales se SUELDAN: la malla queda unida para editarla
                ip = (int)(w.vertex.size() / 3);
                w.vertex.push_back(v[i][0]); w.vertex.push_back(v[i][1]); w.vertex.push_back(v[i][2]);
                for (int k = 0; k < 4; k++) w.vertexColor.push_back(255);   // sin color: blanco
                posiciones[cp] = ip;
            } else ip = it->second;
            FaceCorner fc;
            fc.vertex = ip; fc.normal = in; fc.uv = -1; fc.color = -1;
            f.corners.push_back(fc);
        }
        w.faces.push_back(f);
        if ((t & 8191) == 0) ProgresoActualizar(0.5f + 0.4f * (float)t / (float)s.tris.size());
    }
    if (w.faces.empty()) return NULL;
#ifdef W3D_SYMBIAN
    // N95: index buffer de 16 bits (ver el import del OBJ). Con las normales por cara, un vertice que toca caras
    // que miran para distintos lados se dibuja varias veces: el limite es de vertices DIBUJADOS (hasta 3 por cara).
    if ((int)(w.vertex.size() / 3) > W3D_MAX_INDEX16 || w.faces.size() * 3 > (size_t)W3D_MAX_INDEX16) {
        Notificar(T("Model too big for the N95: more than 65535 vertices (16-bit limit)"), true);
        return NULL;
    }
#endif
    Mesh* mesh = new Mesh(CollectionActive, Vector3(0, 0, 0));
    mesh->SetNameObj(nombre);   // nombre UNICO
    int a = 0, b = 0, c = 0;
    w.ConvertToES1(mesh, &a, &b, &c);
    mesh->CalcularBordes();
    mesh->meshSmooth = MeshShadingImportadoEsSmooth(mesh);
    mesh->OptimizarCacheRender();
    return mesh;
}

// ---- escritura --------------------------------------------------------------------------------------------------
void RecolectarMallas(Object* o, bool selectedOnly, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* c = o->Childrens[i];
        if (c->getType() == ObjectType::mesh && (!selectedOnly || c->select)) out.push_back((Mesh*)c);
        RecolectarMallas(c, selectedOnly, out);
    }
}

}  // namespace

bool ImportSTL(const std::string& path) {
    std::vector<unsigned char> datos;
    if (!w3dFileSystem::ReadFileBytes(path, datos) || datos.empty()) {
        w3dLogfE("ImportSTL: no pude leer '%s'", path.c_str());
        Notificar(T("Could not read the STL file"), true);
        return false;
    }
    ProgresoIniciar(T("Importing model..."));
    std::vector<Solido> solidos;
    if (EsBinario(datos)) LeerBinario(datos, solidos);
    else                  LeerASCII(datos, solidos);
    datos.clear();
    std::vector<unsigned char>().swap(datos);   // (la RAM del N95: el archivo ya no hace falta)

    const std::string base = ExtractBaseName(path);
    int hechas = 0;
    for (size_t i = 0; i < solidos.size(); i++) {
        if (solidos[i].tris.empty()) continue;
        const std::string nom = solidos[i].nombre.empty() ? base : solidos[i].nombre;
        if (MallaDe(solidos[i], nom)) hechas++;
        std::vector<Tri>().swap(solidos[i].tris);
    }
    ProgresoFin();
    if (!hechas) {
        w3dLogfE("ImportSTL: '%s' no tiene triangulos", path.c_str());
        Notificar(T("STL: nothing to import"), true);
        return false;
    }
    w3dLogf("ImportSTL: '%s' -> %d malla(s)", path.c_str(), hechas);
    Notificar(T("STL imported successfully!"), false);
    return true;
}

bool ExportSTL(const std::string& path, bool selectedOnly, bool applyModifiers, bool applyTransforms) {
    std::vector<Mesh*> mallas;
    RecolectarMallas(SceneCollection, selectedOnly, mallas);
    std::string tris;   // los registros de 50 bytes
    unsigned int n = 0;
    for (size_t mi = 0; mi < mallas.size(); mi++) {
        Mesh* m = mallas[mi];
        bool gen = applyModifiers && !m->modificadores.empty();
        if (gen) m->GenerarMallaModificada();
        gen = gen && m->genValido && m->genVertex && m->genFaces && m->genVertexSize > 0;
        const GLfloat* V = gen ? m->genVertex : m->vertex;
        const MeshIndex* I = gen ? m->genFaces : m->faces;
        const int nV = gen ? m->genVertexSize : m->vertexSize;
        const std::vector<MaterialGroup>& grupos = gen ? m->genMaterialsGroup : m->materialsGroup;
        if (!V || !I || nV <= 0) continue;
        for (size_t g = 0; g < grupos.size(); g++) {
            const MaterialGroup& G = grupos[g];
            for (int k = G.startDrawn; k + 2 < G.startDrawn + G.indicesDrawnCount; k += 3) {
                float s[3][3];
                bool ok = true;
                for (int j = 0; j < 3; j++) {
                    const int vi = (int)I[k + j];
                    if (vi < 0 || vi >= nV) { ok = false; break; }
                    // BASE (como el export OBJ): el archivo no depende de la vista que se dibujo ultimo
                    const Vector3 lp(V[vi * 3], V[vi * 3 + 1], V[vi * 3 + 2]);
                    const Vector3 w = applyTransforms ? m->LocalAMundoBase(lp) : lp;
                    float wf[3] = { w.x, w.y, w.z };
                    AlSTL(wf, s[j]);
                }
                if (!ok) continue;
                float nn[3];
                if (!NormalDe(s[0], s[1], s[2], nn)) nn[0] = nn[1] = nn[2] = 0.0f;
                for (int q = 0; q < 3; q++) EscribirF32(tris, nn[q]);
                for (int j = 0; j < 3; j++) for (int q = 0; q < 3; q++) EscribirF32(tris, s[j][q]);
                tris.append(2, '\0');   // "attribute byte count": 0
                n++;
            }
        }
    }
    if (!n) {
        w3dLogfE("ExportSTL: no hay triangulos para exportar");
        Notificar(T("Error: could not save the STL"), true);
        return false;
    }
    std::string out;
    out.reserve(84 + tris.size());
    std::string cab = "Whisk3D STL";
    cab.resize(80, ' ');   // (no empieza con "solid": algunos lectores lo tomarian por ASCII)
    out += cab;
    EscribirU32(out, n);
    out += tris;
    std::ofstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) {
        w3dLogfE("ExportSTL: no pude abrir '%s'", path.c_str());
        Notificar(T("Error: could not save the STL"), true);
        return false;
    }
    f.write(out.data(), (std::streamsize)out.size());
    f.close();
    if (!f) { Notificar(T("Error: could not save the STL"), true); return false; }
    w3dLogf("ExportSTL: '%s' (%u triangulos)", path.c_str(), n);
    Notificar(T("STL saved successfully!"), false);
    W3dDescargarSiWeb(path);
    return true;
}
