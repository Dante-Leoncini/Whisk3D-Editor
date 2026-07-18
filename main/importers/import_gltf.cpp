#include "import_gltf.h"
#include "import_obj.h"          // Wavefront / Face / FaceCorner / ExtractBaseName / EncolarTextura
#include "objects/Mesh.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // Armature clips + SkelMatrizAEulerFBX/SkelMatRotEuler + PrepararSkin
#include "animation/Animation.h"         // AnimProperty / keyFrame + enum AnimPosition/Rotation/Scale
#include "edit/Modifier.h"               // modificador Armature (auto-add al importar)
#include "objects/Materials.h"           // Material
#include "objects/Objects.h"             // CollectionActive
#include "w3dFilesystem.h"               // w3dFileSystem::ReadFileBytes / FileExists
#include "ViewPorts/LayoutInput.h"       // Notificar (toast)
#include "ViewPorts/PopUp/ProgressPopup.h" // barra de progreso (import: clave en el N95 lento, faltaba en glTF)
#include "math/Quaternion.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "w3dlog.h"
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <stdint.h>

// ============================================================================
//  Importador glTF 2.0 (.gltf + buffer externo/base64, y .glb binario).
//  El modelo de skinning de glTF es LIMPIO: huesos = nodos (TRS), inverseBindMatrices
//  dadas, animaciones = keyframes de TRS por nodo, todo en Y-up. Mapea al FK ESTANDAR
//  del core (Armature::skinGltf): skinMatrix = worldFK * inverseBindMatrix (sin biped,
//  sin figure-scale, sin NodeToYup). A diferencia del FBX no hay que reconstruir nada.
// ============================================================================

namespace {

// ---------------------------------------------------------------------------
//  JSON parser minimo (recursivo). glTF es JSON puro; no hace falta libreria.
// ---------------------------------------------------------------------------
struct JVal {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } t;
    bool b; double num; std::string str;
    std::vector<JVal> arr;
    std::vector<std::pair<std::string, JVal> > members; // objeto (orden preservado, lookup lineal)
    JVal() : t(NUL), b(false), num(0) {}
    const JVal* find(const char* k) const {
        if (t != OBJ) return NULL;
        for (size_t i = 0; i < members.size(); i++) if (members[i].first == k) return &members[i].second;
        return NULL;
    }
    bool has(const char* k) const { return find(k) != NULL; }
    double numOr(double d) const { return t == NUM ? num : d; }
    int intOr(int d) const { return t == NUM ? (int)num : d; }
    const std::string& sOr() const { static std::string e; return t == STR ? str : e; }
    size_t size() const { return t == ARR ? arr.size() : (t == OBJ ? members.size() : 0); }
    // helpers para leer un campo tipado de un objeto
    int    getI(const char* k, int d) const { const JVal* v = find(k); return v ? v->intOr(d) : d; }
    double getN(const char* k, double d) const { const JVal* v = find(k); return v ? v->numOr(d) : d; }
    std::string getS(const char* k, const char* d) const { const JVal* v = find(k); return v && v->t == STR ? v->str : std::string(d); }
};

struct JParser {
    const char* p; const char* end; bool ok;
    JParser(const char* s, size_t n) : p(s), end(s + n), ok(true) {}
    void skipWs() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool parse(JVal& out) { skipWs(); return value(out); }
    bool value(JVal& v) {
        skipWs(); if (p >= end) { ok = false; return false; }
        char c = *p;
        if (c == '{') return object(v);
        if (c == '[') return array(v);
        if (c == '"') { v.t = JVal::STR; return string(v.str); }
        if (c == 't' || c == 'f') return boolean(v);
        if (c == 'n') { if (end - p >= 4 && strncmp(p, "null", 4) == 0) { p += 4; v.t = JVal::NUL; return true; } ok = false; return false; }
        return number(v);
    }
    bool object(JVal& v) {
        v.t = JVal::OBJ; p++; // '{'
        skipWs(); if (p < end && *p == '}') { p++; return true; }
        while (p < end) {
            skipWs(); if (p >= end || *p != '"') { ok = false; return false; }
            std::string key; if (!string(key)) return false;
            skipWs(); if (p >= end || *p != ':') { ok = false; return false; } p++;
            JVal val; if (!value(val)) return false;
            v.members.push_back(std::make_pair(key, val));
            skipWs(); if (p >= end) { ok = false; return false; }
            if (*p == ',') { p++; continue; }
            if (*p == '}') { p++; return true; }
            ok = false; return false;
        }
        ok = false; return false;
    }
    bool array(JVal& v) {
        v.t = JVal::ARR; p++; // '['
        skipWs(); if (p < end && *p == ']') { p++; return true; }
        while (p < end) {
            JVal el; if (!value(el)) return false;
            v.arr.push_back(el);
            skipWs(); if (p >= end) { ok = false; return false; }
            if (*p == ',') { p++; continue; }
            if (*p == ']') { p++; return true; }
            ok = false; return false;
        }
        ok = false; return false;
    }
    bool string(std::string& out) {
        out.clear(); p++; // '"'
        while (p < end) {
            char c = *p++;
            if (c == '"') return true;
            if (c == '\\') {
                if (p >= end) break;
                char e = *p++;
                switch (e) {
                    case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
                    case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                    case 'u': {
                        if (end - p < 4) { ok = false; return false; }
                        int cp = 0; for (int i = 0; i < 4; i++) { char h = *p++; cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= h - '0';
                            else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10; }
                        // UTF-8 (BMP): suficiente para nombres de glTF
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                        else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                        break;
                    }
                    default: out += e; break;
                }
            } else out += c;
        }
        ok = false; return false;
    }
    bool boolean(JVal& v) {
        if (end - p >= 4 && strncmp(p, "true", 4) == 0) { p += 4; v.t = JVal::BOOL; v.b = true; return true; }
        if (end - p >= 5 && strncmp(p, "false", 5) == 0) { p += 5; v.t = JVal::BOOL; v.b = false; return true; }
        ok = false; return false;
    }
    bool number(JVal& v) {
        const char* s = p;
        while (p < end && (*p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E' || (*p >= '0' && *p <= '9'))) p++;
        if (p == s) { ok = false; return false; }
        v.t = JVal::NUM; v.num = atof(std::string(s, p - s).c_str());
        return true;
    }
};

// ---------------------------------------------------------------------------
//  base64 (data URIs embebidas)
// ---------------------------------------------------------------------------
int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62; if (c == '/') return 63;
    return -1;
}
void b64decode(const char* s, size_t n, std::vector<unsigned char>& out) {
    int acc = 0, bits = 0;
    for (size_t i = 0; i < n; i++) {
        int v = b64val(s[i]); if (v < 0) continue;
        acc = (acc << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((unsigned char)((acc >> bits) & 0xFF)); }
    }
}

// ---------------------------------------------------------------------------
//  utilidades
// ---------------------------------------------------------------------------
std::string DirOf(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    return (s == std::string::npos) ? std::string("") : path.substr(0, s + 1);
}
// URL-decode minimo (los uri de glTF pueden tener %20)
std::string UrlDecode(const std::string& s) {
    std::string o; for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = s[i+1], lo = s[i+2];
            #define HEX(c) ((c>='0'&&c<='9')?c-'0':(c>='a'&&c<='f')?c-'a'+10:(c>='A'&&c<='F')?c-'A'+10:0)
            o += (char)((HEX(hi) << 4) | HEX(lo)); i += 2;
            #undef HEX
        } else o += s[i];
    }
    return o;
}

Matrix4 Mat16(const double* d) { Matrix4 m; for (int i = 0; i < 16; i++) m.m[i] = (float)d[i]; return m; }
Matrix4 MatTransL(const Vector3& t) { Matrix4 m; m.Identity(); m.m[12] = t.x; m.m[13] = t.y; m.m[14] = t.z; return m; }
Matrix4 MatScaleL(const Vector3& s) { Matrix4 m; m.Identity(); m.m[0] = s.x; m.m[5] = s.y; m.m[10] = s.z; return m; }
Vector3 MatTransVec(const Matrix4& m) { return Vector3(m.m[12], m.m[13], m.m[14]); }
// aplica una matriz a un PUNTO (column-major m[col*4+fila])
Vector3 MatXform(const Matrix4& m, const Vector3& v) {
    return Vector3(m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z + m.m[12],
                   m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z + m.m[13],
                   m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14]);
}
// rota un VECTOR (sin traslacion)
Vector3 MatRot3(const Matrix4& m, const Vector3& v) {
    return Vector3(m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z,
                   m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z,
                   m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z);
}
// descompone una matriz LOCAL en T / euler(XYZ, convencion FBX) / S (columnas)
void DecomponerTRS(const Matrix4& M, Vector3& T, Vector3& R, Vector3& S) {
    T = Vector3(M.m[12], M.m[13], M.m[14]);
    float sx = sqrtf(M.m[0]*M.m[0] + M.m[1]*M.m[1] + M.m[2]*M.m[2]);
    float sy = sqrtf(M.m[4]*M.m[4] + M.m[5]*M.m[5] + M.m[6]*M.m[6]);
    float sz = sqrtf(M.m[8]*M.m[8] + M.m[9]*M.m[9] + M.m[10]*M.m[10]);
    S = Vector3(sx ? sx : 1.0f, sy ? sy : 1.0f, sz ? sz : 1.0f);
    Matrix4 Rot; Rot.Identity();
    if (sx > 1e-8f) { Rot.m[0] = M.m[0]/sx; Rot.m[1] = M.m[1]/sx; Rot.m[2] = M.m[2]/sx; }
    if (sy > 1e-8f) { Rot.m[4] = M.m[4]/sy; Rot.m[5] = M.m[5]/sy; Rot.m[6] = M.m[6]/sy; }
    if (sz > 1e-8f) { Rot.m[8] = M.m[8]/sz; Rot.m[9] = M.m[9]/sz; Rot.m[10] = M.m[10]/sz; }
    R = SkelMatrizAEulerFBX(Rot, 0);
}
Matrix4 QuatMat(float x, float y, float z, float w) { Quaternion q(w, x, y, z); Matrix4 m; m.Identity(); q.ToMatrix(m.m); return m; }

// ---------------------------------------------------------------------------
//  modelo glTF + lectura de accessors
// ---------------------------------------------------------------------------
struct GltfDoc {
    JVal root;
    std::vector<std::vector<unsigned char> > buffers; // datos crudos de cada buffer
    std::string dir;

    const JVal* arr(const char* name) const { const JVal* v = root.find(name); return (v && v->t == JVal::ARR) ? v : NULL; }

    // devuelve el puntero al inicio de un bufferView + su stride
    const unsigned char* viewPtr(int bvIdx, size_t& stride, size_t& length) const {
        const JVal* bvs = arr("bufferViews"); if (!bvs || bvIdx < 0 || bvIdx >= (int)bvs->size()) return NULL;
        const JVal& bv = bvs->arr[bvIdx];
        int buf = bv.getI("buffer", 0);
        size_t off = (size_t)bv.getI("byteOffset", 0);
        length = (size_t)bv.getI("byteLength", 0);
        stride = (size_t)bv.getI("byteStride", 0);
        if (buf < 0 || buf >= (int)buffers.size()) return NULL;
        if (off > buffers[buf].size()) return NULL;
        return buffers[buf].empty() ? NULL : &buffers[buf][off];
    }

    static size_t compSize(int ct) { switch (ct) { case 5120: case 5121: return 1; case 5122: case 5123: return 2; case 5125: case 5126: return 4; } return 0; }
    static int    numComp(const std::string& t) { if (t == "SCALAR") return 1; if (t == "VEC2") return 2; if (t == "VEC3") return 3; if (t == "VEC4") return 4; if (t == "MAT2") return 4; if (t == "MAT3") return 9; if (t == "MAT4") return 16; return 1; }

    // lee un accessor como floats (aplica normalized). out.size = count*numComp
    bool readFloats(int accIdx, std::vector<float>& out, int& count, int& ncomp) const {
        const JVal* accs = arr("accessors"); if (!accs || accIdx < 0 || accIdx >= (int)accs->size()) return false;
        const JVal& a = accs->arr[accIdx];
        int bv = a.getI("bufferView", -1); if (bv < 0) return false;
        int ct = a.getI("componentType", 5126);
        count = a.getI("count", 0);
        std::string type = a.getS("type", "SCALAR");
        ncomp = numComp(type);
        bool norm = false; { const JVal* n = a.find("normalized"); if (n && n->t == JVal::BOOL) norm = n->b; }
        size_t accOff = (size_t)a.getI("byteOffset", 0);
        size_t stride = 0, len = 0; const unsigned char* base = viewPtr(bv, stride, len); if (!base) return false;
        size_t cs = compSize(ct); if (cs == 0) return false;
        if (stride == 0) stride = cs * ncomp;
        out.resize((size_t)count * ncomp);
        for (int i = 0; i < count; i++) {
            const unsigned char* e = base + accOff + (size_t)i * stride;
            for (int c = 0; c < ncomp; c++) {
                const unsigned char* pc = e + (size_t)c * cs; float f = 0;
                switch (ct) {
                    case 5126: { float v; memcpy(&v, pc, 4); f = v; } break;
                    case 5125: { uint32_t v; memcpy(&v, pc, 4); f = (float)v; } break;
                    case 5123: { uint16_t v; memcpy(&v, pc, 2); f = norm ? v / 65535.0f : (float)v; } break;
                    case 5122: { int16_t v; memcpy(&v, pc, 2); f = norm ? (v < 0 ? v / 32768.0f : v / 32767.0f) : (float)v; } break;
                    case 5121: { uint8_t v = *pc; f = norm ? v / 255.0f : (float)v; } break;
                    case 5120: { int8_t v = (int8_t)*pc; f = norm ? (v < 0 ? v / 128.0f : v / 127.0f) : (float)v; } break;
                }
                out[(size_t)i * ncomp + c] = f;
            }
        }
        return true;
    }
    // lee un accessor de indices (enteros)
    bool readIndices(int accIdx, std::vector<uint32_t>& out) const {
        const JVal* accs = arr("accessors"); if (!accs || accIdx < 0 || accIdx >= (int)accs->size()) return false;
        const JVal& a = accs->arr[accIdx];
        int bv = a.getI("bufferView", -1); if (bv < 0) return false;
        int ct = a.getI("componentType", 5125);
        int count = a.getI("count", 0);
        size_t accOff = (size_t)a.getI("byteOffset", 0);
        size_t stride = 0, len = 0; const unsigned char* base = viewPtr(bv, stride, len); if (!base) return false;
        size_t cs = compSize(ct); if (cs == 0) return false; if (stride == 0) stride = cs;
        out.resize(count);
        for (int i = 0; i < count; i++) { const unsigned char* pc = base + accOff + (size_t)i * stride; uint32_t v = 0;
            if (ct == 5125) memcpy(&v, pc, 4); else if (ct == 5123) { uint16_t s; memcpy(&s, pc, 2); v = s; } else v = *pc;
            out[i] = v; }
        return true;
    }
    // lee un accessor MAT4 -> matrices
    bool readMats(int accIdx, std::vector<Matrix4>& out) const {
        std::vector<float> f; int cnt = 0, nc = 0; if (!readFloats(accIdx, f, cnt, nc) || nc != 16) return false;
        out.resize(cnt); for (int i = 0; i < cnt; i++) for (int k = 0; k < 16; k++) out[i].m[k] = f[(size_t)i * 16 + k];
        return true;
    }
};

// (control-point, hueso, peso) -> vertexGroups. A NIVEL DE ARCHIVO: un tipo LOCAL no puede ser argumento de template
// (std::vector<PesoCP>) en C++03/STLport (Symbian: "a template argument may not reference a local type").
struct PesoCP { int cp; int joint; float w; };

} // namespace

// ============================================================================
//  ImportGLTF
// ============================================================================
bool ImportGLTF(const std::string& filepath) {
    std::vector<unsigned char> file;
    if (!w3dFileSystem::ReadFileBytes(filepath, file) || file.size() < 12) {
        w3dLogfE("ImportGLTF: no pude leer %s", filepath.c_str());
        Notificar("glTF: no pude abrir el archivo", true); return false;
    }
    GltfDoc doc; doc.dir = DirOf(filepath);

    // ---- GLB (binario) vs .gltf (texto) ----
    std::string jsonStr;
    std::vector<unsigned char> glbBin; bool esGlb = (file.size() >= 4 && file[0] == 'g' && file[1] == 'l' && file[2] == 'T' && file[3] == 'F');
    if (esGlb) {
        // header: magic(4) version(4) length(4); luego chunks: length(4) type(4) data
        size_t pos = 12;
        while (pos + 8 <= file.size()) {
            uint32_t clen, ctype; memcpy(&clen, &file[pos], 4); memcpy(&ctype, &file[pos + 4], 4); pos += 8;
            if (pos + clen > file.size()) break;
            if (ctype == 0x4E4F534A) jsonStr.assign((const char*)&file[pos], clen);           // "JSON"
            else if (ctype == 0x004E4942) glbBin.assign(file.begin() + pos, file.begin() + pos + clen); // "BIN\0"
            pos += clen; if (clen % 4) pos += 4 - (clen % 4);
        }
    } else {
        jsonStr.assign((const char*)&file[0], file.size());
    }
    JParser jp(jsonStr.c_str(), jsonStr.size());
    if (!jp.parse(doc.root) || !jp.ok || doc.root.t != JVal::OBJ) {
        w3dLogfE("ImportGLTF: JSON invalido en %s", filepath.c_str());
        Notificar("glTF: JSON invalido", true); return false;
    }

    ProgresoIniciar("Importing glTF..."); // barra de progreso (no-op sin GUI; clave en el N95). Faltaba en glTF.
    ProgresoActualizar(0.1f);

    // ---- buffers ----
    const JVal* bufs = doc.arr("buffers");
    if (bufs) for (size_t i = 0; i < bufs->size(); i++) {
        const JVal& b = bufs->arr[i]; std::vector<unsigned char> data;
        std::string uri = b.getS("uri", "");
        if (uri.empty()) { if (i == 0 && !glbBin.empty()) data = glbBin; }                  // GLB: buffer 0 = chunk BIN
        else if (uri.compare(0, 5, "data:") == 0) { size_t c = uri.find(','); if (c != std::string::npos) b64decode(uri.c_str() + c + 1, uri.size() - c - 1, data); }
        else { std::vector<unsigned char> fb; if (w3dFileSystem::ReadFileBytes(doc.dir + UrlDecode(uri), fb)) data.swap(fb); else w3dLogfW("ImportGLTF: no pude leer buffer %s", uri.c_str()); }
        doc.buffers.push_back(data);
    }

    // ---- nodos: matriz LOCAL + GLOBAL (FK) ----
    const JVal* nodes = doc.arr("nodes");
    int nNodes = nodes ? (int)nodes->size() : 0;
    std::vector<Matrix4> localM(nNodes), globalM(nNodes);
    std::vector<int> parentNode(nNodes, -1);
    for (int i = 0; i < nNodes; i++) {
        const JVal& n = nodes->arr[i];
        Matrix4 L; L.Identity();
        const JVal* mtx = n.find("matrix");
        if (mtx && mtx->t == JVal::ARR && mtx->size() == 16) { for (int k = 0; k < 16; k++) L.m[k] = (float)mtx->arr[k].numOr(0); }
        else {
            Vector3 T(0,0,0), S(1,1,1); Matrix4 R; R.Identity();
            const JVal* tr = n.find("translation"); if (tr && tr->size() == 3) T = Vector3((float)tr->arr[0].numOr(0), (float)tr->arr[1].numOr(0), (float)tr->arr[2].numOr(0));
            const JVal* sc = n.find("scale");       if (sc && sc->size() == 3) S = Vector3((float)sc->arr[0].numOr(1), (float)sc->arr[1].numOr(1), (float)sc->arr[2].numOr(1));
            const JVal* ro = n.find("rotation");    if (ro && ro->size() == 4) R = QuatMat((float)ro->arr[0].numOr(0), (float)ro->arr[1].numOr(0), (float)ro->arr[2].numOr(0), (float)ro->arr[3].numOr(1));
            L = MatTransL(T) * R * MatScaleL(S);
        }
        localM[i] = L;
        const JVal* kids = n.find("children");
        if (kids && kids->t == JVal::ARR) for (size_t c = 0; c < kids->size(); c++) { int ci = kids->arr[c].intOr(-1); if (ci >= 0 && ci < nNodes) parentNode[ci] = i; }
    }
    // GLOBAL por FK (los nodos ya vienen en orden util, pero resolvemos por cadena de padres por si acaso)
    { std::vector<char> done(nNodes, 0);
      struct L { static Matrix4 rec(int i, std::vector<Matrix4>& g, const std::vector<Matrix4>& l, const std::vector<int>& par, std::vector<char>& d) {
          if (d[i]) return g[i]; d[i] = 1;
          g[i] = (par[i] >= 0) ? rec(par[i], g, l, par, d) * l[i] : l[i]; return g[i]; } };
      for (int i = 0; i < nNodes; i++) L::rec(i, globalM, localM, parentNode, done);
    }

    Object* collection = CollectionActive;
    std::string baseNom = ExtractBaseName(filepath);

    // ---- skin -> Armature ----
    Armature* arm = NULL;
    std::map<int, int> jointOfNode; // nodo glTF -> indice de hueso
    const JVal* skins = doc.arr("skins");
    if (skins && skins->size() > 0) {
        const JVal& sk = skins->arr[0];
        const JVal* joints = sk.find("joints");
        int nJoints = (joints && joints->t == JVal::ARR) ? (int)joints->size() : 0;
        if (nJoints > 0) {
            std::vector<int> jointNode(nJoints);
            for (int j = 0; j < nJoints; j++) { jointNode[j] = joints->arr[j].intOr(-1); jointOfNode[jointNode[j]] = j; }
            std::vector<Matrix4> ibm; { int ibmAcc = sk.getI("inverseBindMatrices", -1); if (ibmAcc >= 0) doc.readMats(ibmAcc, ibm); }

            arm = new Armature(collection, Vector3(0, 0, 0));
            arm->name = baseNom + "_rig";
            arm->skinGltf = true;
            arm->bones.resize(nJoints);
            for (int j = 0; j < nJoints; j++) {
                int nd = jointNode[j];
                W3dBone& b = arm->bones[j];
                b.name = (nd >= 0 && nd < nNodes) ? nodes->arr[nd].getS("name", "bone") : std::string("bone");
                // padre = hueso ancestro mas cercano
                int bp = -1; { int p = (nd >= 0) ? parentNode[nd] : -1; while (p >= 0) { std::map<int,int>::iterator it = jointOfNode.find(p); if (it != jointOfNode.end()) { bp = it->second; break; } p = parentNode[p]; } }
                b.parent = bp;
                // restLocal = relativo al hueso-padre (para el root incluye el prefijo de ancestros no-hueso = matriz Z-up)
                Matrix4 gj = (nd >= 0) ? globalM[nd] : Matrix4();
                Matrix4 restLocal = (bp >= 0) ? (globalM[jointNode[bp]].Inverse() * gj) : gj;
                DecomponerTRS(restLocal, b.restT, b.restR, b.restS);
                b.preRot = Vector3(0,0,0); b.postRot = Vector3(0,0,0);
                b.rotPivot = Vector3(0,0,0); b.rotOffset = Vector3(0,0,0); b.sclPivot = Vector3(0,0,0); b.sclOffset = Vector3(0,0,0);
                b.rotOrder = 0; b.hasRest = true; b.hasSkin = true;
                b.bind = gj; b.head = MatTransVec(gj); b.tail = b.head;
                b.skinInvBind = (j < (int)ibm.size()) ? ibm[j] : gj.Inverse();
                b.skinA = b.skinInvBind;
            }
            PrepararSkin(arm); // en el path glTF: skinMatrix = worldFK * inverseBindMatrix
            for (int j = 0; j < nJoints; j++) { arm->bones[j].poseHead = arm->bones[j].head; arm->bones[j].poseTail = arm->bones[j].tail; }
        }
    }

    // ---- animaciones -> clips ----
    const JVal* anims = doc.arr("animations");
    if (arm && anims) {
        for (size_t ai = 0; ai < anims->size(); ai++) {
            const JVal& an = anims->arr[ai];
            const JVal* chans = an.find("channels"); const JVal* samps = an.find("samplers");
            if (!chans || !samps) continue;
            SkeletalAnimation* clip = new SkeletalAnimation(an.getS("name", "Animation"));
            // fps del clip: los tiempos glTF son SEGUNDOS y glTF NO guarda fps. PRIMERO leer extras.frameRate (lo
            // escribe nuestro exporter -> round-trip exacto). Si no esta (archivos de terceros), ADIVINAR del menor
            // delta entre tiempos -> OJO: con keyframes sparse (ej solo frame 1 y 20) el unico delta es grande y da
            // fps=1, colapsando el rango a 1..2. Por eso extras es la fuente confiable; la deteccion es solo fallback.
            float dtMin = 1e9f, tMax = 0.0f;
            for (size_t s = 0; s < samps->size(); s++) { int inAcc = samps->arr[s].getI("input", -1); if (inAcc < 0) continue;
                std::vector<float> ts; int c = 0, nc = 0; if (!doc.readFloats(inAcc, ts, c, nc)) continue;
                for (int k = 0; k < c; k++) { if (ts[k] > tMax) tMax = ts[k]; if (k > 0) { float d = ts[k] - ts[k-1]; if (d > 1e-5f && d < dtMin) dtMin = d; } } }
            int fps = 0; { const JVal* ex = an.find("extras"); if (ex) fps = ex->getI("frameRate", 0); } // fuente confiable
            if (fps <= 0) fps = (dtMin < 1e8f && dtMin > 1e-5f) ? (int)(1.0f / dtMin + 0.5f) : 30;        // fallback: adivinar
            if (fps < 1) fps = 30; if (fps > 240) fps = 240;
            clip->FrameRate = fps; clip->startFrame = 1; clip->endFrame = (int)(tMax * fps + 0.5f) + 1;

            for (size_t c = 0; c < chans->size(); c++) {
                const JVal& ch = chans->arr[c];
                const JVal* tgt = ch.find("target"); if (!tgt) continue;
                int nd = tgt->getI("node", -1); std::string path = tgt->getS("path", "");
                std::map<int,int>::iterator jit = jointOfNode.find(nd);
                if (jit == jointOfNode.end()) continue; // canal a un nodo que no es hueso (ej: morph) -> ignorar
                int bone = jit->second;
                int prop = (path == "translation") ? AnimPosition : (path == "rotation") ? AnimRotation : (path == "scale") ? AnimScale : -1;
                if (prop < 0) continue;
                int sampIdx = ch.getI("sampler", -1); if (sampIdx < 0 || sampIdx >= (int)samps->size()) continue;
                const JVal& sm = samps->arr[sampIdx];
                std::vector<float> times; int tc = 0, tnc = 0; if (!doc.readFloats(sm.getI("input", -1), times, tc, tnc)) continue;
                std::vector<float> vals; int vc = 0, vnc = 0; if (!doc.readFloats(sm.getI("output", -1), vals, vc, vnc)) continue;
                std::string interp = sm.getS("interpolation", "LINEAR");
                int stepV = (interp == "CUBICSPLINE") ? 3 : 1; // CUBICSPLINE: [inTangent, value, outTangent] -> tomamos value
                int voff  = (interp == "CUBICSPLINE") ? 1 : 0;
                // prefijo de ancestros no-hueso (root joint): bakea la matriz Z-up en los keyframes
                Matrix4 prefix; prefix.Identity();
                { int npd = (nd >= 0) ? parentNode[nd] : -1;
                  int bpNode = (arm->bones[bone].parent >= 0) ? -1 : -1; // node del hueso-padre
                  if (arm->bones[bone].parent >= 0) { // buscar el node del hueso-padre
                      for (std::map<int,int>::iterator it = jointOfNode.begin(); it != jointOfNode.end(); ++it) if (it->second == arm->bones[bone].parent) { bpNode = it->first; break; } }
                  if (npd != bpNode) { Matrix4 gNp = (npd >= 0) ? globalM[npd] : Matrix4(); Matrix4 gBp = (bpNode >= 0) ? globalM[bpNode] : Matrix4(); prefix = gBp.Inverse() * gNp; }
                }
                bool hayPrefix = false; for (int k = 0; k < 16; k++) { float id = (k % 5 == 0) ? 1.0f : 0.0f; if (fabsf(prefix.m[k] - id) > 1e-5f) { hayPrefix = true; break; } }
                float psp = 1.0f; if (hayPrefix) { psp = sqrtf(prefix.m[0]*prefix.m[0] + prefix.m[1]*prefix.m[1] + prefix.m[2]*prefix.m[2]); if (psp < 1e-6f) psp = 1.0f; }

                BoneTrack& tr = clip->TrackDe(bone);
                // Los samplers de glTF traen los 3 componentes JUNTOS por key. Se arma primero la lista de valores
                // (y para la rotacion se corrige la continuidad euler, que NECESITA los 3 juntos) y recien despues se
                // reparten en las 3 CURVAS independientes (una por componente).
                std::vector<int> kfr((size_t)tc); std::vector<Vector3> kv((size_t)tc);
                for (int k = 0; k < tc; k++) {
                    kfr[k] = (int)(times[k] * fps + 0.5f) + 1;
                    if (prop == AnimRotation) {
                        size_t o = ((size_t)k * stepV + voff) * 4;
                        Matrix4 Rm = QuatMat(vals[o], vals[o+1], vals[o+2], vals[o+3]);
                        if (hayPrefix) Rm = prefix * Rm; // solo rota (la traslacion/escala del prefix no afecta la rotacion pura)
                        kv[k] = SkelMatrizAEulerFBX(Rm, 0);
                    } else {
                        size_t o = ((size_t)k * stepV + voff) * 3;
                        Vector3 v(vals[o], vals[o+1], vals[o+2]);
                        if (hayPrefix) { if (prop == AnimPosition) v = MatXform(prefix, v); else v = v * psp; }
                        kv[k] = v;
                    }
                }
                // EULER CONTINUO (rotacion): cada keyframe se saco del cuaternion por separado, asi que puede (a) SALTAR
                // 360 al cruzar +-180, y (b) FLIPEAR de representacion cerca del gimbal (y~+-90) -> la interpolacion lineal
                // gira por el lado largo o pega un tiron (el "hombro rotado de mas"). Se elige, por keyframe, la euler mas
                // CONTINUA con la anterior: se prueba la representacion alterna (x+180,180-y,z+180) -MISMA rotacion, se
                // verifica por matriz- y se destuerce cada componente a <180 del anterior; gana la que menos se aleja.
                if (prop == AnimRotation) for (size_t k = 1; k < kv.size(); k++) {
                    float px = kv[k-1].x, py = kv[k-1].y, pz = kv[k-1].z;
                    float ax = kv[k].x, ay = kv[k].y, az = kv[k].z;                       // candidato A: tal cual
                    float bx = kv[k].x + 180.0f, by = 180.0f - kv[k].y, bz = kv[k].z + 180.0f; // B: flip de gimbal
                    #define UNW(v, ref) do { while ((v)-(ref) > 180.0f) (v)-=360.0f; while ((v)-(ref) < -180.0f) (v)+=360.0f; } while(0)
                    UNW(ax,px); UNW(ay,py); UNW(az,pz); UNW(bx,px); UNW(by,py); UNW(bz,pz);
                    #undef UNW
                    float dA = fabsf(ax-px)+fabsf(ay-py)+fabsf(az-pz);
                    float dB = fabsf(bx-px)+fabsf(by-py)+fabsf(bz-pz);
                    bool usarB = false;
                    if (dB < dA) { // el flip es mas continuo: aceptarlo SOLO si es la misma rotacion (guarda de seguridad)
                        Matrix4 MA = SkelMatRotEuler(Vector3(ax,ay,az), 0), MB = SkelMatRotEuler(Vector3(bx,by,bz), 0);
                        float diff = 0; for (int q = 0; q < 16; q++){ float d = MA.m[q]-MB.m[q]; diff += d*d; }
                        usarB = (diff < 0.001f);
                    }
                    if (usarB) kv[k] = Vector3(bx,by,bz); else kv[k] = Vector3(ax,ay,az);
                }
                // repartir en las 3 curvas. OJO: PropertyDe puede CREAR la propiedad (realloc del vector) -> se
                // resuelve una por bloque y se usa dentro del bloque (nunca dos referencias vivas a la vez).
                { AnimProperty& a = tr.PropertyDe(prop, AnimX); for (int k=0;k<tc;k++) SetKeyCurva(a, kfr[k], kv[k].x); }
                { AnimProperty& a = tr.PropertyDe(prop, AnimY); for (int k=0;k<tc;k++) SetKeyCurva(a, kfr[k], kv[k].y); }
                { AnimProperty& a = tr.PropertyDe(prop, AnimZ); for (int k=0;k<tc;k++) SetKeyCurva(a, kfr[k], kv[k].z); }
            }
            arm->animations.push_back(clip);
        }
        if (!arm->animations.empty()) arm->animActiva = 0;
        w3dLogf("ImportGLTF: %d animacion(es)", (int)arm->animations.size());
    }

    // ---- materiales ----
    std::vector<Material*> mats;
    const JVal* jmats = doc.arr("materials");
    const JVal* jtex = doc.arr("textures");
    const JVal* jimg = doc.arr("images");
    if (jmats) for (size_t i = 0; i < jmats->size(); i++) {
        const JVal& jm = jmats->arr[i];
        Material* mat = new Material(jm.getS("name", "mat"));
        const JVal* pbr = jm.find("pbrMetallicRoughness");
        if (pbr) { const JVal* bc = pbr->find("baseColorFactor");
            if (bc && bc->size() == 4) for (int k = 0; k < 4; k++) mat->diffuse[k] = (float)bc->arr[k].numOr(1);
            const JVal* bct = pbr->find("baseColorTexture");
            if (bct && jtex && jimg) { int ti = bct->getI("index", -1);
                if (ti >= 0 && ti < (int)jtex->size()) { int im = jtex->arr[ti].getI("source", -1);
                    if (im >= 0 && im < (int)jimg->size()) { std::string uri = jimg->arr[im].getS("uri", "");
                        if (!uri.empty() && uri.compare(0, 5, "data:") != 0) EncolarTextura(mat, doc.dir + UrlDecode(uri)); } } }
        }
        mats.push_back(mat);
    }

    // ---- mallas: una por NODO con "mesh" (mezcla las primitivas del mesh en mesh-parts) ----
    const JVal* meshes = doc.arr("meshes");
    int importadas = 0;
    ProgresoActualizar(0.4f);
    int nMeshNodes = 0; if (meshes && nodes) for (int q = 0; q < nNodes; q++) if (nodes->arr[q].getI("mesh", -1) >= 0) nMeshNodes++;
    int meshNodeIdx = 0;
    for (int nd = 0; nd < nNodes && meshes; nd++) {
        const JVal& node = nodes->arr[nd];
        int mi = node.getI("mesh", -1); if (mi < 0 || mi >= (int)meshes->size()) continue;
        ProgresoActualizar(0.4f + 0.6f * (nMeshNodes ? (float)(meshNodeIdx++) / (float)nMeshNodes : 0.0f)); // 40%..100% por malla
        bool skinned = node.has("skin") && arm;
        const JVal& gm = meshes->arr[mi];
        const JVal* prims = gm.find("primitives"); if (!prims || prims->t != JVal::ARR) continue;

        Wavefront Wobj; Wobj.Reset();
        // por-primitiva: control-points (posiciones), normales/uv por corner, caras, material group
        std::vector<PesoCP> pesos; // (control-point, hueso, peso) -> vertexGroups (PesoCP definido a nivel de archivo)
        int cpBase = 0;
        for (size_t p = 0; p < prims->size(); p++) {
            const JVal& pr = prims->arr[p];
            const JVal* at = pr.find("attributes"); if (!at) continue;
            int posA = at->getI("POSITION", -1); if (posA < 0) continue;
            std::vector<float> POS; int pc = 0, pnc = 0; if (!doc.readFloats(posA, POS, pc, pnc) || pnc < 3) continue;
            std::vector<float> NRM; int nc = 0, nnc = 0; { int a = at->getI("NORMAL", -1); if (a >= 0) doc.readFloats(a, NRM, nc, nnc); }
            std::vector<float> UV;  int uc = 0, unc = 0; { int a = at->getI("TEXCOORD_0", -1); if (a >= 0) doc.readFloats(a, UV, uc, unc); }
            std::vector<float> JNT; int jc = 0, jnc = 0; { int a = at->getI("JOINTS_0", -1); if (a >= 0) doc.readFloats(a, JNT, jc, jnc); }
            std::vector<float> WGT; int wc = 0, wnc = 0; { int a = at->getI("WEIGHTS_0", -1); if (a >= 0) doc.readFloats(a, WGT, wc, wnc); }
            std::vector<uint32_t> IDX; { int a = pr.getI("indices", -1); if (a >= 0) doc.readIndices(a, IDX); }
            if (IDX.empty()) { IDX.resize(pc); for (int i = 0; i < pc; i++) IDX[i] = i; } // sin indices: secuencial

            // control-points de esta primitiva -> Wobj.vertex. Estatica: se dejan en LOCAL (el objeto lleva el
            // transform del nodo, ver abajo) para que cada objeto tenga su ORIGEN propio -como en Blender- y se
            // pueda rotar desde su centro. Skinned: espacio escena crudo (lo ubica el armature).
            for (int i = 0; i < pc; i++) { Vector3 v(POS[(size_t)i*pnc], POS[(size_t)i*pnc+1], POS[(size_t)i*pnc+2]);
                Wobj.vertex.push_back(v.x); Wobj.vertex.push_back(v.y); Wobj.vertex.push_back(v.z); }

            // pesos por control-point (skinned): joint LOCAL de la primitiva -> hueso global
            const JVal* jointsArr = (skins && skins->size() > 0) ? skins->arr[0].find("joints") : NULL;
            if (skinned && jnc >= 4 && wnc >= 4 && jointsArr) {
                for (int i = 0; i < pc; i++) for (int e = 0; e < 4; e++) {
                    float w = WGT[(size_t)i*wnc+e]; if (w <= 0.0001f) continue;
                    int localJ = (int)(JNT[(size_t)i*jnc+e] + 0.5f);
                    if (localJ < 0 || localJ >= (int)jointsArr->size()) continue;
                    PesoCP pw; pw.cp = cpBase + i; pw.joint = localJ; pw.w = w; pesos.push_back(pw);
                }
            }

            // caras (triangulos) -> Face con corners (cp + normal + uv por corner; en glTF son por-vertice)
            int matGroupStart = (int)Wobj.faces.size();
            for (size_t f = 0; f + 2 < IDX.size(); f += 3) {
                Face cara;
                for (int t = 0; t < 3; t++) { int vi = (int)IDX[f + t];
                    FaceCorner fc; fc.vertex = cpBase + vi; fc.color = -1; fc.normal = -1; fc.uv = -1;
                    if (nnc >= 3 && vi < nc) { Wobj.normals.push_back((GLbyte)(POS.empty() ? 0 : (int)(NRM[(size_t)vi*nnc] * 127.0f)));
                        Wobj.normals.push_back((GLbyte)(NRM[(size_t)vi*nnc+1] * 127.0f)); Wobj.normals.push_back((GLbyte)(NRM[(size_t)vi*nnc+2] * 127.0f));
                        fc.normal = (int)(Wobj.normals.size() / 3) - 1; }
                    if (unc >= 2 && vi < uc) { Wobj.uv.push_back(UV[(size_t)vi*unc]); Wobj.uv.push_back(UV[(size_t)vi*unc+1]);
                        fc.uv = (int)(Wobj.uv.size() / 2) - 1; }
                    cara.corners.push_back(fc);
                }
                Wobj.faces.push_back(cara);
            }
            // material group de esta primitiva
            int matIdx = pr.getI("material", -1);
            Material* mat = (matIdx >= 0 && matIdx < (int)mats.size()) ? mats[matIdx] : NULL;
            int cnt = (int)Wobj.faces.size() - matGroupStart;
            if (cnt > 0) { MaterialGroup mg; mg.material = mat; mg.name = mat ? mat->name : std::string("Mesh");
                mg.start = matGroupStart; mg.count = 0; mg.startDrawn = 0; mg.indicesDrawnCount = 0; Wobj.materialsGroup.push_back(mg); }
            cpBase += pc;
        }
        if (Wobj.faces.empty()) continue;

        std::string nom = baseNom; if (importadas > 0) { char buf[16]; snprintf(buf, sizeof(buf), ".%03d", importadas); nom += buf; }
        Object* parentMesh = arm ? (Object*)arm : collection;
        Mesh* mesh = new Mesh(parentMesh, Vector3(0, 0, 0));
        mesh->name = nom;
        // malla estatica: el objeto se queda con su PROPIO origen = el transform del nodo (pos/rot/escala del
        // mundo). Los vertices ya quedaron en LOCAL (relativos a ese origen). Descompongo globalM sin pasar por
        // euler (FromMatrix es exacto y evita la duda grados/radianes). Skinned: origen en (0,0,0).
        if (!skinned) {
            const Matrix4& W = globalM[nd];
            float sx = sqrtf(W.m[0]*W.m[0] + W.m[1]*W.m[1] + W.m[2]*W.m[2]);
            float sy = sqrtf(W.m[4]*W.m[4] + W.m[5]*W.m[5] + W.m[6]*W.m[6]);
            float sz = sqrtf(W.m[8]*W.m[8] + W.m[9]*W.m[9] + W.m[10]*W.m[10]);
            Matrix4 Rot; Rot.Identity();
            if (sx > 1e-8f) { Rot.m[0] = W.m[0]/sx; Rot.m[1] = W.m[1]/sx; Rot.m[2] = W.m[2]/sx; }
            if (sy > 1e-8f) { Rot.m[4] = W.m[4]/sy; Rot.m[5] = W.m[5]/sy; Rot.m[6] = W.m[6]/sy; }
            if (sz > 1e-8f) { Rot.m[8] = W.m[8]/sz; Rot.m[9] = W.m[9]/sz; Rot.m[10] = W.m[10]/sz; }
            mesh->pos = Vector3(W.m[12], W.m[13], W.m[14]);
            mesh->scale = Vector3(sx ? sx : 1.0f, sy ? sy : 1.0f, sz ? sz : 1.0f);
            mesh->rot = Quaternion::FromMatrix(Rot); mesh->rot.normalize(); mesh->ActualizarDisplayRot();
        }
        int a0 = 0, a1 = 0, a2 = 0;
        Wobj.ConvertToES1(mesh, &a0, &a1, &a2, &mesh->vertCtrlPoint);
        mesh->CalcularBordes();
        if (!mesh->normals && mesh->vertexSize > 0) { mesh->normals = new GLbyte[mesh->vertexSize * 3]; mesh->meshSmooth = true; mesh->RecalcularNormales(); }
        else if (mesh->normals && mesh->vertexSize > 0) mesh->meshSmooth = MeshShadingImportadoEsSmooth(mesh);
#ifndef W3D_SYMBIAN
        mesh->OptimizarCacheRender();
#endif
        // vertex groups (pesos por hueso) desde 'pesos' (por control-point)
        if (skinned && !pesos.empty()) {
            std::map<int, VertexGroup*> grpDe; // hueso -> grupo
            for (size_t i = 0; i < pesos.size(); i++) { PesoCP& pw = pesos[i];
                std::map<int, VertexGroup*>::iterator it = grpDe.find(pw.joint);
                VertexGroup* g;
                if (it == grpDe.end()) { g = new VertexGroup(arm->bones[pw.joint].name); grpDe[pw.joint] = g; mesh->vertexGroups.push_back(g); }
                else g = it->second;
                g->verts.push_back(pw.cp); g->pesos.push_back(pw.w);
            }
            if (!mesh->vertexGroups.empty()) mesh->grupoActivo = 0;
            if (!arm->animations.empty()) {
                Modifier* amod = new Modifier(ModifierType::Armature, "Armature"); amod->target = arm;
                mesh->modificadores.push_back(amod); mesh->modificadorActivo = (int)mesh->modificadores.size() - 1;
                mesh->skinArmature = arm;
            }
        }
        importadas++;
    }

    if (importadas == 0 && !arm) { w3dLogfE("ImportGLTF: sin mallas ni esqueleto"); ProgresoFin(); Notificar("glTF: nada que importar", true); return false; }
    ProgresoFin();
    w3dLogf("ImportGLTF: %d malla(s), %s", importadas, filepath.c_str());
    Notificar("glTF imported successfully!", false);
    return true;
}
