#include "W3dLang.h"   // T(): los avisos salen en el idioma del editor
#include "import_gltf.h"
#include "import_obj.h"          // Wavefront / Face / FaceCorner / ExtractBaseName / EnqueueTexture
#include "objects/Mesh.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // Armature clips + SkelMatrixToEulerFBX/SkelMatRotEuler + PrepareSkin
#include "animation/Animation.h"         // AnimProperty / keyFrame + enum AnimPosition/Rotation/Scale
#include "edit/Modifier.h"               // modificador Armature (auto-add al importar)
#include "objects/Materials.h"           // Material
#include "objects/Textures.h"            // TextureTake (las capas de textura extra se cargan sincronicas)
#include "graphics/w3dTexture.h"         // DecodeImage / SavePNG (separar los canales de metalico/rugosidad)
#include "objects/Empty.h"               // los nodos sin malla que tienen hijos (la jerarquia del glTF)
#include "objects/Camera.h"              // las camaras del glTF
#include "objects/Light.h"               // las luces de KHR_lights_punctual
#include "W3dNames.h"                    // LA regla de nombres unicos (partes de malla del .glb)
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
#include <set>
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
    bool   getB(const char* k, bool d) const { const JVal* v = find(k); return (v && v->t == BOOL) ? v->b : d; }
};

// (depth: no mas de 128 niveles de [ y {: unos pocos KB de "[[[[..." agotaban la pila -antes en la PSP y el N95-.
//  Cada valor se arma EN SU LUGAR del padre: antes se armaba aparte y se copiaba entero, subarbol por subarbol)
struct JParser {
    const char* p; const char* end; bool ok; int depth;
    JParser(const char* s, size_t n) : p(s), end(s + n), ok(true), depth(0) {}
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
        if (++depth > 128) { ok = false; return false; }
        v.t = JVal::OBJ; p++; // '{'
        skipWs(); if (p < end && *p == '}') { p++; depth--; return true; }
        while (p < end) {
            skipWs(); if (p >= end || *p != '"') { ok = false; return false; }
            std::string key; if (!string(key)) return false;
            skipWs(); if (p >= end || *p != ':') { ok = false; return false; } p++;
            v.members.push_back(std::make_pair(key, JVal()));
            if (!value(v.members.back().second)) return false;
            skipWs(); if (p >= end) { ok = false; return false; }
            if (*p == ',') { p++; continue; }
            if (*p == '}') { p++; depth--; return true; }
            ok = false; return false;
        }
        ok = false; return false;
    }
    bool array(JVal& v) {
        if (++depth > 128) { ok = false; return false; }
        v.t = JVal::ARR; p++; // '['
        skipWs(); if (p < end && *p == ']') { p++; depth--; return true; }
        while (p < end) {
            v.arr.push_back(JVal());
            if (!value(v.arr.back())) return false;
            skipWs(); if (p >= end) { ok = false; return false; }
            if (*p == ',') { p++; continue; }
            if (*p == ']') { p++; depth--; return true; }
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

Matrix4 MatTransL(const Vector3& t) { Matrix4 m; m.Identity(); m.m[12] = t.x; m.m[13] = t.y; m.m[14] = t.z; return m; }
Matrix4 MatScaleL(const Vector3& s) { Matrix4 m; m.Identity(); m.m[0] = s.x; m.m[5] = s.y; m.m[10] = s.z; return m; }
Vector3 MatTransVec(const Matrix4& m) { return Vector3(m.m[12], m.m[13], m.m[14]); }
// aplica una matriz a un PUNTO (column-major m[col*4+fila])
Vector3 MatXform(const Matrix4& m, const Vector3& v) {
    return Vector3(m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z + m.m[12],
                   m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z + m.m[13],
                   m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14]);
}
// descompone una matriz LOCAL en T / euler(XYZ, convencion FBX) / S (columnas)
void DecomposeTRS(const Matrix4& M, Vector3& T, Vector3& R, Vector3& S) {
    T = Vector3(M.m[12], M.m[13], M.m[14]);
    float sx = sqrtf(M.m[0]*M.m[0] + M.m[1]*M.m[1] + M.m[2]*M.m[2]);
    float sy = sqrtf(M.m[4]*M.m[4] + M.m[5]*M.m[5] + M.m[6]*M.m[6]);
    float sz = sqrtf(M.m[8]*M.m[8] + M.m[9]*M.m[9] + M.m[10]*M.m[10]);
    S = Vector3(sx ? sx : 1.0f, sy ? sy : 1.0f, sz ? sz : 1.0f);
    Matrix4 Rot; Rot.Identity();
    if (sx > 1e-8f) { Rot.m[0] = M.m[0]/sx; Rot.m[1] = M.m[1]/sx; Rot.m[2] = M.m[2]/sx; }
    if (sy > 1e-8f) { Rot.m[4] = M.m[4]/sy; Rot.m[5] = M.m[5]/sy; Rot.m[6] = M.m[6]/sy; }
    if (sz > 1e-8f) { Rot.m[8] = M.m[8]/sz; Rot.m[9] = M.m[9]/sz; Rot.m[10] = M.m[10]/sz; }
    R = SkelMatrixToEulerFBX(Rot, 0);
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
        if (off >= buffers[buf].size()) return NULL;
        // la vista no puede pasarse del buffer (archivo truncado): se recorta
        if (length > buffers[buf].size() - off) length = buffers[buf].size() - off;
        return &buffers[buf][off];
    }

    // EL ACCESSOR ENTRA EN SU VISTA: 'count' elementos de 'elem' bytes, cada 'stride', desde 'accOff'. Los numeros
    // vienen del archivo: un byteOffset o byteStride negativo pasado a size_t, o un count grande (en 32 bits), hacia
    // dar la vuelta a la cuenta "accOff + (count - 1) * stride + elem <= len" y se leia antes o despues del buffer
    static bool Fits(int accOff, int count, size_t stride, size_t elem, size_t len) {
        if (accOff < 0 || count < 0 || stride == 0 || stride > 255 || elem > len) return false;
        if (count == 0) return true;
        if ((size_t)accOff > len - elem) return false;
        return (size_t)(count - 1) <= (len - elem - (size_t)accOff) / stride;
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
        const int accOffI = a.getI("byteOffset", 0);
        size_t stride = 0, len = 0; const unsigned char* base = viewPtr(bv, stride, len); if (!base) return false;
        size_t cs = compSize(ct); if (cs == 0) return false;
        if (stride == 0) stride = cs * ncomp;
        // (el accessor tiene que ENTRAR en su bufferView: un .bin truncado o un count corrupto se leia afuera)
        if (!Fits(accOffI, count, stride, (size_t)ncomp * cs, len)) return false;
        const size_t accOff = (size_t)accOffI;
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
        const int accOffI = a.getI("byteOffset", 0);
        size_t stride = 0, len = 0; const unsigned char* base = viewPtr(bv, stride, len); if (!base) return false;
        size_t cs = compSize(ct); if (cs == 0) return false; if (stride == 0) stride = cs;
        if (!Fits(accOffI, count, stride, cs, len)) return false;   // (misma cota que readFloats)
        const size_t accOff = (size_t)accOffI;
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
// (std::vector<WeightCP>) en C++03/STLport (Symbian: "a template argument may not reference a local type").
struct WeightCP { int cp; int joint; float w; };

// ---------------------------------------------------------------------------
//  QUADS / NGONS DESDE UN glTF. El glTF solo lleva triangulos; si la primitiva trae extras.w3d_polygons se rearman
//  los poligonos originales y la malla entra editable como en el programa de origen (un quad = UNA cara en Edit Mode,
//  no dos triangulos). Formato RLE: [lados, cuantos, lados, cuantos, ...] en el ORDEN en que salen los triangulos: un
//  poligono de n lados consume sus n-2 triangulos SEGUIDOS. Asi los emiten el exportador de Blender (loop_triangles,
//  poligono por poligono, filtrados por material) y ExportGLTF de Whisk3D (abanico de cada cara de faces3d). Lo escribe
//  el convertidor de Blender (bongo_cat/convertir_w3d) y ExportGLTF.
//
//  El contorno de cada poligono sale de sus triangulos y no de suponer una triangulacion (Blender corta cada quad por
//  su diagonal mas corta, no en abanico): las aristas internas aparecen dos veces, una en cada sentido, y se descartan;
//  las que quedan son el borde, que se recorre en el sentido de los triangulos (la normal no cambia). Un poligono cuyo
//  borde no es UN lazo de n vertices (degenerado: dos esquinas iguales) queda en triangulos; si la cuenta total no da
//  (el archivo no es pareja de sus extras), la primitiva entera queda en triangulos, como antes.
// ---------------------------------------------------------------------------
static bool PolygonFromTris(const std::vector<Face>& tris, size_t t0, int n, Face& out) {
    out.corners.clear();
    std::vector<int> from, to; from.reserve((size_t)(n - 2) * 3); to.reserve((size_t)(n - 2) * 3);
    std::map<int, FaceCorner> cornerOf;
    for (int k = 0; k < n - 2; k++) {
        const Face& f = tris[t0 + (size_t)k];
        for (int c = 0; c < 3; c++) {
            from.push_back(f.corners[(size_t)c].vertex); to.push_back(f.corners[(size_t)((c + 1) % 3)].vertex);
            cornerOf[f.corners[(size_t)c].vertex] = f.corners[(size_t)c];
        }
    }
    if ((int)cornerOf.size() != n) return false;           // esquinas repetidas o de mas: degenerado
    std::map<int, int> next;                               // borde: vertice -> el siguiente
    for (size_t i = 0; i < from.size(); i++) {
        bool inner = false;
        for (size_t j = 0; j < from.size() && !inner; j++) inner = (from[j] == to[i] && to[j] == from[i]);
        if (inner) continue;
        if (next.count(from[i])) return false;             // dos aristas de borde salen del mismo vertice
        next[from[i]] = to[i];
    }
    if ((int)next.size() != n) return false;
    int v = tris[t0].corners[0].vertex;
    for (int k = 0; k < n; k++) {
        std::map<int, int>::const_iterator it = next.find(v);
        if (it == next.end()) { out.corners.clear(); return false; }
        out.corners.push_back(cornerOf[v]);
        v = it->second;
    }
    if (v != tris[t0].corners[0].vertex) { out.corners.clear(); return false; }   // mas de un lazo
    return true;
}

// los triangulos de UNA primitiva -> sus poligonos (con extras.w3d_polygons) o tal cual (sin)
static void PushPrimitiveFaces(const JVal& prim, const std::vector<Face>& tris, bool anyDropped, std::vector<Face>& faces) {
    const JVal* ex = prim.find("extras");
    const JVal* rle = ex ? ex->find("w3d_polygons") : NULL;
    bool ok = rle && rle->t == JVal::ARR && rle->size() % 2 == 0 && !anyDropped;
    if (ok) {
        size_t total = 0;
        for (size_t i = 0; i + 1 < rle->size() && ok; i += 2) {
            const int n = rle->arr[i].intOr(0), cnt = rle->arr[i + 1].intOr(-1);
            if (n < 3 || cnt < 0) ok = false; else total += (size_t)(n - 2) * (size_t)cnt;
        }
        if (ok && total != tris.size()) {
            w3dLogfW("[glTF] extras.w3d_polygons no coincide con los triangulos de la primitiva (%d vs %d): queda triangulada",
                     (int)total, (int)tris.size());
            ok = false;
        }
    }
    if (!ok) { faces.insert(faces.end(), tris.begin(), tris.end()); return; }
    size_t t = 0; int split = 0;
    for (size_t i = 0; i + 1 < rle->size(); i += 2) {
        const int n = rle->arr[i].intOr(3), cnt = rle->arr[i + 1].intOr(0);
        for (int c = 0; c < cnt; c++, t += (size_t)(n - 2)) {
            if (n == 3) { faces.push_back(tris[t]); continue; }
            Face poly;
            if (PolygonFromTris(tris, t, n, poly)) faces.push_back(poly);
            else { split++; for (int k = 0; k < n - 2; k++) faces.push_back(tris[t + (size_t)k]); }
        }
    }
    if (split) w3dLogfW("[glTF] %d poligonos degenerados quedaron en triangulos", split);
}

// ---------------------------------------------------------------------------
//  COLOR: los factores de glTF (baseColorFactor, emissiveFactor, el color de las luces) son LINEALES; Whisk3D trabaja
//  en valores de PANTALLA (sin gestion de color). Pasados tal cual, todo lo que traia un color plano de Blender entraba
//  mas oscuro y saturado. Las TEXTURAS no se tocan: ya vienen en sRGB. Un .glb que escribio el propio Whisk3D (trae
//  los extras w3d_*) se lee tal cual: su exportador escribe los valores de pantalla y el ida y vuelta queda exacto.
// ---------------------------------------------------------------------------
float LinASrgb(float c) {
    if (c <= 0.0f) return 0.0f;
    if (c <= 0.0031308f) return 12.92f * c;
    float s = 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
    return s > 1.0f ? 1.0f : s;
}

// ---------------------------------------------------------------------------
//  IMAGENES: la ruta en disco de la imagen de una textura glTF. Una imagen EMBEBIDA (la de un .glb, en un bufferView,
//  o un data: URI) se MATERIALIZA a un archivo junto al modelo, como hace el importador de FBX con las suyas: la carga
//  de texturas es por ruta (cache con refcount, cola diferida) y al guardar el proyecto entra al .w3d igual que una
//  suelta. Antes se descartaba en silencio y todo .glb de Blender entraba sin texturas.
// ---------------------------------------------------------------------------
std::string ExtDeMime(const std::string& mime) {
    if (mime == "image/jpeg") return ".jpg";
    if (mime == "image/webp") return ".webp";
    return ".png";
}
std::string NombreSeguro(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        const char c = s[i];
        o += ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
             ? c : '_';
    }
    return o;
}
bool EscribirArchivo(const std::string& path, const unsigned char* data, size_t n) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = (n == 0) || fwrite(data, 1, n, f) == n;
    fclose(f);
    return ok;
}
// la ruta (o "" si no hay / no se pudo) de la imagen 'im'. 'cache': imagen -> ruta (una sola materializacion por imagen)
std::string RutaImagen(const GltfDoc& doc, int im, const std::string& baseName, std::map<int, std::string>& cache) {
    std::map<int, std::string>::iterator it = cache.find(im);
    if (it != cache.end()) return it->second;
    std::string out;
    const JVal* jimg = doc.arr("images");
    if (jimg && im >= 0 && im < (int)jimg->size()) {
        const JVal& ji = jimg->arr[im];
        const std::string uri = ji.getS("uri", "");
        if (!uri.empty() && uri.compare(0, 5, "data:") != 0) out = doc.dir + UrlDecode(uri);
        else {
            std::vector<unsigned char> bytes;
            std::string mime = ji.getS("mimeType", "");
            if (!uri.empty()) {   // data:image/png;base64,....
                const size_t coma = uri.find(',');
                if (mime.empty()) { const size_t pc = uri.find(';'); if (pc != std::string::npos && pc > 5) mime = uri.substr(5, pc - 5); }
                if (coma != std::string::npos) b64decode(uri.c_str() + coma + 1, uri.size() - coma - 1, bytes);
            } else {
                size_t stride = 0, len = 0;
                const unsigned char* p = doc.viewPtr(ji.getI("bufferView", -1), stride, len);
                if (p && len) bytes.assign(p, p + len);
            }
            if (!bytes.empty()) {
                char num[16]; snprintf(num, sizeof num, "%d", im);
                std::string nm = ji.getS("name", "");
                nm = NombreSeguro(nm.empty() ? std::string("imagen") + num : nm);
                const size_t pt = nm.rfind('.');
                if (pt != std::string::npos && pt > 0) nm = nm.substr(0, pt);   // (la extension la da el mime)
                const std::string path = doc.dir + NombreSeguro(baseName) + "_" + nm + ExtDeMime(mime);
                if (EscribirArchivo(path, &bytes[0], bytes.size())) {
                    out = path;
                    w3dLogf("ImportGLTF: imagen embebida extraida -> %s (%d bytes)", path.c_str(), (int)bytes.size());
                } else w3dLogfW("ImportGLTF: no pude escribir la imagen embebida %s", path.c_str());
            }
        }
    }
    cache[im] = out;
    return out;
}
// la imagen de una textureInfo ({"index": t, "texCoord": k}) de un material
std::string RutaTextura(const GltfDoc& doc, const JVal* info, const std::string& baseName,
                        std::map<int, std::string>& cache) {
    if (!info) return std::string();
    const JVal* jtex = doc.arr("textures");
    const int ti = info->getI("index", -1);
    if (!jtex || ti < 0 || ti >= (int)jtex->size()) return std::string();
    return RutaImagen(doc, jtex->arr[ti].getI("source", -1), baseName, cache);
}
// el sampler de una textura: filtro (NEAREST = pixel perfect) y repeticion (CLAMP = sin repetir)
void SamplerDe(const GltfDoc& doc, const JVal* info, bool& filtrada, bool& repite) {
    const JVal* jtex = doc.arr("textures");
    const JVal* jsam = doc.arr("samplers");
    const int ti = info ? info->getI("index", -1) : -1;
    if (!jtex || !jsam || ti < 0 || ti >= (int)jtex->size()) return;
    const int si = jtex->arr[ti].getI("sampler", -1);
    if (si < 0 || si >= (int)jsam->size()) return;
    const JVal& s = jsam->arr[si];
    if (s.getI("magFilter", 9729) == 9728) filtrada = false;                       // NEAREST
    if (s.getI("wrapS", 10497) == 33071 && s.getI("wrapT", 10497) == 33071) repite = false;   // CLAMP_TO_EDGE
}
// UNA COPIA DE LA IMAGEN CON UN CANAL (o el color multiplicado por un factor): los mapas de Whisk3D leen el R de su
// textura (rugosidad, metalico) o su RGB tal cual (emision), y glTF empaqueta rugosidad en G y metalico en B, y
// multiplica los mapas por su factor. Se escribe junto a la imagen original (<imagen>_<sufijo>.png).
// canal 0..2 = ese canal a gris; -1 = RGB por el factor de cada canal
std::string CopiaCanal(const std::string& src, int canal, const float fac[3], const char* sufijo) {
    if (src.empty()) return std::string();
    unsigned char* rgba = NULL; int w = 0, h = 0;
    if (!w3dEngine::DecodeImage(src.c_str(), &rgba, &w, &h) || !rgba || w <= 0 || h <= 0) {
        w3dLogfW("ImportGLTF: no pude decodificar %s para separar sus canales", src.c_str());
        return std::string();
    }
    const size_t n = (size_t)w * (size_t)h;
    for (size_t i = 0; i < n; i++) {
        unsigned char* px = rgba + i * 4;
        for (int c = 0; c < 3; c++) {
            const float v = (float)px[canal >= 0 ? canal : c] * (canal >= 0 ? fac[0] : fac[c]);
            px[c] = (unsigned char)(v > 255.0f ? 255.0f : (v < 0.0f ? 0.0f : v + 0.5f));
        }
        px[3] = 255;
    }
    std::string dst = src;
    const size_t pt = dst.rfind('.'), sl = dst.find_last_of("/\\");
    (void)sl;
    if (pt != std::string::npos && (sl == std::string::npos || pt > sl)) dst = dst.substr(0, pt);
    dst += std::string("_") + sufijo + ".png";
    const bool ok = w3dEngine::SavePNG(dst.c_str(), rgba, w, h, false);
    w3dEngine::FreeImage(rgba);
    if (!ok) { w3dLogfW("ImportGLTF: no pude escribir %s", dst.c_str()); return std::string(); }
    return dst;
}

// ---------------------------------------------------------------------------
//  LA JERARQUIA DEL glTF. Antes cada malla entraba con su transform de MUNDO colgada de la coleccion (o de TODAS
//  colgadas del armature, si el archivo tenia uno) y sin el nombre de su nodo: un hijo animado se movia en el espacio
//  de su padre del glTF pero estaba ubicado en el del mundo, y todo se llamaba "<archivo>.NNN".
//  Ahora cada nodo que es algo (malla, camara, luz, o un nodo vacio con hijos o animado) es un objeto con su NOMBRE,
//  colgado del objeto de su ancestro mas cercano, con su transform LOCAL a ese padre. Los huesos no son objetos, y un
//  ancestro de huesos (el nodo del armature, que el esqueleto ya horneo en el reposo de sus huesos) tampoco: lo que
//  cuelga de el va al armature, que esta en el origen.
// ---------------------------------------------------------------------------
Object* PadreDeNodo(int nd, const std::vector<int>& parentNode, const std::vector<Object*>& nodeObj,
                    const std::vector<char>& sinObjeto, const std::vector<Matrix4>& globalM,
                    const std::vector<Object*>& armDeNodo, Object* coleccion, Matrix4& mundoPadre) {
    mundoPadre.Identity();
    int p = parentNode[nd], guard = 0;
    while (p >= 0 && guard++ < (int)parentNode.size()) {
        if (nodeObj[p]) { mundoPadre = globalM[p]; return nodeObj[p]; }
        // (un hueso o el nodo del armature: SU armature -el de esa piel-, en el origen)
        if (sinObjeto[p] && armDeNodo[(size_t)p]) return armDeNodo[(size_t)p];
        p = parentNode[p];
    }
    return coleccion;
}
// pos / rotacion / escala LOCALES del objeto (respecto de su padre en Whisk3D) a partir del mundo del nodo. Sin pasar
// por euler: FromMatrix es exacto (y la puerta SetRot deja los displays al dia)
void UbicarObjeto(Object* o, const Matrix4& mundoPadre, const Matrix4& mundo, bool sinEscala) {
    const Matrix4 W = mundoPadre.Inverse() * mundo;
    float sx = sqrtf(W.m[0]*W.m[0] + W.m[1]*W.m[1] + W.m[2]*W.m[2]);
    float sy = sqrtf(W.m[4]*W.m[4] + W.m[5]*W.m[5] + W.m[6]*W.m[6]);
    float sz = sqrtf(W.m[8]*W.m[8] + W.m[9]*W.m[9] + W.m[10]*W.m[10]);
    Matrix4 Rot; Rot.Identity();
    if (sx > 1e-8f) { Rot.m[0] = W.m[0]/sx; Rot.m[1] = W.m[1]/sx; Rot.m[2] = W.m[2]/sx; }
    if (sy > 1e-8f) { Rot.m[4] = W.m[4]/sy; Rot.m[5] = W.m[5]/sy; Rot.m[6] = W.m[6]/sy; }
    if (sz > 1e-8f) { Rot.m[8] = W.m[8]/sz; Rot.m[9] = W.m[9]/sz; Rot.m[10] = W.m[10]/sz; }
    o->pos = Vector3(W.m[12], W.m[13], W.m[14]);
    o->scale = sinEscala ? Vector3(1, 1, 1) : Vector3(sx ? sx : 1.0f, sy ? sy : 1.0f, sz ? sz : 1.0f);
    o->SetRot(Quaternion::FromMatrix(Rot));
}
// el nombre del objeto de un nodo: el del NODO (el del objeto en Blender), si no el de su malla, si no "<archivo>.NNN"
std::string NombreNodo(const JVal& node, const JVal* gm, const std::string& baseName, int k) {
    std::string n = node.getS("name", "");
    if (n.empty() && gm) n = gm->getS("name", "");
    if (n.empty()) { n = baseName; if (k > 0) { char buf[16]; snprintf(buf, sizeof(buf), ".%03d", k); n += buf; } }
    return n;
}
// LA CAMARA de un nodo ("camera": indice en "cameras"). Mira a su -Z local con +Y arriba, igual que en Whisk3D.
Object* CrearCamara(const GltfDoc& doc, const JVal& node, Object* padre) {
    const JVal* cams = doc.arr("cameras");
    const int ci = node.getI("camera", -1);
    if (!cams || ci < 0 || ci >= (int)cams->size()) return NULL;
    const JVal& jc = cams->arr[ci];
    Camera* c = new Camera(padre);
    if (jc.getS("type", "perspective") == "orthographic") {
        c->orthographic = true;
        const JVal* o = jc.find("orthographic");
        if (o) { c->nearClip = (float)o->getN("znear", c->nearClip); c->farClip = (float)o->getN("zfar", c->farClip); }
    } else {
        const JVal* pe = jc.find("perspective");
        if (pe) {
            c->fov = (float)(pe->getN("yfov", 0.785398) * 57.2957795);   // (glTF: vertical, en radianes, como Whisk3D)
            c->nearClip = (float)pe->getN("znear", c->nearClip);
            if (pe->has("zfar")) c->farClip = (float)pe->getN("zfar", c->farClip);   // (sin zfar = infinito: el de fabrica)
            if (pe->has("aspectRatio")) c->aspect = (float)pe->getN("aspectRatio", 0.0);   // el encuadre de origen
        }
    }
    return c;
}
// LA LUZ de un nodo (KHR_lights_punctual). Unidades: punto/foco en candelas, sol en lux (los usa Blender con 683 lm/W).
// En Whisk3D la intensidad multiplica el color y la puntual cae con 1/d^2: se toma 1 W = 683 lm, como el exportador de
// Blender, y la radiancia W/(4 pi) / pi que da su mismo brillo sobre un blanco mate.
Object* CrearLuz(const GltfDoc& doc, const JVal& node, Object* padre, bool& esSol) {
    esSol = false;
    const JVal* ext = node.find("extensions");
    const JVal* kl = ext ? ext->find("KHR_lights_punctual") : NULL;
    if (!kl) return NULL;
    const JVal* rext = doc.root.find("extensions");
    const JVal* rkl = rext ? rext->find("KHR_lights_punctual") : NULL;
    const JVal* lights = rkl ? rkl->find("lights") : NULL;
    const int li = kl->getI("light", -1);
    if (!lights || lights->t != JVal::ARR || li < 0 || li >= (int)lights->size()) return NULL;
    const JVal& jl = lights->arr[li];
    Light* l = Light::Create(padre);
    if (!l) { w3dLogfW("ImportGLTF: se llego al maximo de luces: '%s' no entra", node.getS("name", "").c_str()); return NULL; }
    const JVal* col = jl.find("color");
    for (int k = 0; k < 3; k++) l->diffuse[k] = LinASrgb((col && col->size() == 3) ? (float)col->arr[k].numOr(1) : 1.0f);
    const std::string tipo = jl.getS("type", "point");
    const float inten = (float)jl.getN("intensity", 1.0);
    if (tipo == "directional") {
        esSol = true;
        W3dLightSetType(l, 1);
        l->intensity = inten / 683.0f;
    } else {
        W3dLightSetType(l, tipo == "spot" ? 2 : 0);
        l->intensity = inten / 683.0f / 3.14159265f;
        l->attConstant = 0.05f; l->attLinear = 0.0f; l->attQuadratic = 1.0f;   // (fisica: 1/d^2, sin dividir por 0)
        if (jl.has("range")) l->distance = (float)jl.getN("range", 0.0);
        if (tipo == "spot") {
            const JVal* sp = jl.find("spot");
            const float outer = sp ? (float)sp->getN("outerConeAngle", 0.785398) : 0.785398f;
            const float inner = sp ? (float)sp->getN("innerConeAngle", 0.0) : 0.0f;
            l->spotCutoff = outer * 57.2957795f;
            l->spotExponent = (outer > 1e-5f) ? 8.0f * (1.0f - inner / outer) : 0.0f;   // (borde duro = 0)
        }
    }
    return l;
}

} // namespace

// ============================================================================
//  MATERIALES: reuso por nombre SOLO si es el mismo material
//
//  El reuso por nombre existe para que reabrir un proyecto no multiplique los materiales
//  (cada .glb del .w3d trae su copia de "Piel"). Pero hacerlo A CIEGAS COLAPSA materiales
//  DISTINTOS que comparten nombre: dos .glb de Blender traen "Material" cada uno y el
//  segundo se quedaba con el color/textura del primero, en silencio y sin vuelta atras.
//  Ahora se compara el CONTENIDO; si difiere, el material nuevo entra con nombre unico.
// ============================================================================
static bool MatAlmostEqual(float a, float b) { float d = a - b; if (d < 0) d = -d; return d <= 1e-4f; }
// la ruta de la textura de un material YA existente: la cargada, o la que tiene ENCOLADA
// (el import no decodifica en el momento -> recien importado, mat->texture sigue NULL).
static std::string MatTexturePath(const Material* m) {
    if (m->texture && !m->texture->path.empty()) return m->texture->path;
    return PendingTextureOf(m);
}
static bool SameMaterialImported(const Material* old, const Material& newValue, const std::string& texPath) {
    for (int k = 0; k < 4; k++) if (!MatAlmostEqual(old->diffuse[k], newValue.diffuse[k])) return false;
    if (!MatAlmostEqual(old->shininess, newValue.shininess)) return false;
    if (old->lighting    != newValue.lighting)    return false;
    if (old->culling     != newValue.culling)     return false;
    if (old->textureOn   != newValue.textureOn)   return false;
    if (old->filtered    != newValue.filtered)    return false;
    if (old->repeat      != newValue.repeat)      return false;
    if (old->transparent != newValue.transparent) return false;
    if (old->depth_test  != newValue.depth_test)  return false;
    if (old->vertexColor != newValue.vertexColor) return false;
    if (old->chrome      != newValue.chrome)      return false;
    if (old->normalMap   != newValue.normalMap)   return false;
    if (old->reflectMode != newValue.reflectMode) return false;
    if (!MatAlmostEqual(old->alphaTest, newValue.alphaTest)) return false;   // recorte por alfa (glTF MASK)
    if (old->textureB != newValue.textureB || !MatAlmostEqual(old->mixB, newValue.mixB)) return false;   // mezcla de texturas
    if (old->toon != newValue.toon || !MatAlmostEqual(old->outline, newValue.outline)) return false;   // toon y su borde
    if (MatTexturePath(old) != texPath)        return false;
    // lo PBR de glTF (emision, metalico/rugosidad, normal map y los mapas con textura): antes no se leia
    for (int k = 0; k < 4; k++) if (!MatAlmostEqual(old->emission[k], newValue.emission[k]) ||
                                    !MatAlmostEqual(old->specular[k], newValue.specular[k])) return false;
    if (!MatAlmostEqual(old->rtRoughness, newValue.rtRoughness) || !MatAlmostEqual(old->rtMetallic, newValue.rtMetallic))
        return false;
    if (!MatAlmostEqual(old->transmission, newValue.transmission) || !MatAlmostEqual(old->ior, newValue.ior)) return false;
    if (old->normalTexture != newValue.normalTexture) return false;
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++)
        if (old->mapTex[k] != newValue.mapTex[k] || old->mapWithTexture[k] != newValue.mapWithTexture[k] ||
            old->uvMap[k] != newValue.uvMap[k]) return false;
    // las capas de textura extra (w3d_layers): misma cantidad, misma textura, mezcla y UV
    if (old->layers.size() != newValue.layers.size()) return false;
    for (size_t c = 0; c < newValue.layers.size(); c++)
        if (old->layers[c].tex != newValue.layers[c].tex || old->layers[c].blend != newValue.layers[c].blend ||
            old->layers[c].uvMap != newValue.layers[c].uvMap || old->layers[c].on != newValue.layers[c].on) return false;
    return true;
}
// pasa los campos leidos del glTF al material definitivo (todo MENOS el nombre, que ya
// uniquifico el constructor, y las texturas, que van por la cola de carga diferida).
static void CopyFieldsMaterial(const Material& src, Material* dst) {
    for (int k = 0; k < 4; k++) dst->diffuse[k] = src.diffuse[k];
    dst->shininess   = src.shininess;
    dst->lighting    = src.lighting;
    dst->culling     = src.culling;
    dst->textureOn   = src.textureOn;
    dst->filtered    = src.filtered;
    dst->repeat      = src.repeat;
    dst->transparent = src.transparent;
    dst->depth_test  = src.depth_test;
    dst->vertexColor = src.vertexColor;
    dst->chrome      = src.chrome;
    dst->normalMap   = src.normalMap;
    dst->reflectMode = src.reflectMode;
    dst->alphaTest   = src.alphaTest;   // recorte por alfa (glTF MASK / w3d_alphaCut)
    dst->textureB    = src.textureB;    // mezcla de texturas (w3d_textureB / w3d_mixB), como las capas
    dst->mixB        = src.mixB;
    dst->toon        = src.toon;       // sombreado toon (w3d_toon / w3d_toonMix)
    dst->outline     = src.outline;    // borde del dibujo animado (w3d_outline)
    dst->layers      = src.layers;
    // lo PBR de glTF
    for (int k = 0; k < 4; k++) { dst->emission[k] = src.emission[k]; dst->specular[k] = src.specular[k]; }
    dst->rtRoughness   = src.rtRoughness;
    dst->rtMetallic    = src.rtMetallic;
    dst->transmission  = src.transmission;
    dst->ior           = src.ior;
    dst->normalTexture = src.normalTexture;
    for (int k = W3D_MAP_EMISSION; k < W3D_MAPS; k++) {
        dst->mapTex[k] = src.mapTex[k]; dst->mapWithTexture[k] = src.mapWithTexture[k]; dst->uvMap[k] = src.uvMap[k];
    }
}

// ============================================================================
//  ImportGLTF
// ============================================================================
bool ImportGLTF(const std::string& filepath) {
    std::vector<unsigned char> file;
    if (!w3dFileSystem::ReadFileBytes(filepath, file) || file.size() < 12) {
        w3dLogfE("ImportGLTF: no pude leer %s", filepath.c_str());
        Notify(T("glTF: could not open the file"), true); return false;
    }
    GltfDoc doc; doc.dir = DirOf(filepath);

    // ---- GLB (binario) vs .gltf (texto) ----
    std::string jsonStr;
    std::vector<unsigned char> glbBin; bool isGlb = (file.size() >= 4 && file[0] == 'g' && file[1] == 'l' && file[2] == 'T' && file[3] == 'F');
    if (isGlb) {
        // header: magic(4) version(4) length(4); luego chunks: length(4) type(4) data
        size_t pos = 12;
        while (pos + 8 <= file.size()) {
            uint32_t clen, ctype; memcpy(&clen, &file[pos], 4); memcpy(&ctype, &file[pos + 4], 4); pos += 8;
            if (clen > file.size() - pos) break;   // (restando: pos + clen da la vuelta en 32 bits)
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
        Notify(T("glTF: invalid JSON"), true); return false;
    }

    ProgressStart(T("Importing glTF...")); // barra de progreso (no-op sin GUI; clave en el N95). Faltaba en glTF.
    ProgressUpdate(0.1f);

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
    // (un archivo con un CICLO en los children -A hijo de B y B de A- colgaba las vueltas por la cadena de padres: se
    //  corta el enlace que lo cierra)
    for (int i = 0; i < nNodes; i++) {
        int p = parentNode[i], steps = 0;
        while (p >= 0 && p != i && steps++ < nNodes) p = parentNode[p];
        if (p == i || steps > nNodes) parentNode[i] = -1;
    }
    // GLOBAL por FK, sin recursion (una cadena de miles de nodos agotaba la pila de la PSP): cada nodo junta su cadena de
    // padres hasta uno ya resuelto y la resuelve de arriba para abajo
    { std::vector<char> done(nNodes, 0);
      std::vector<int> chain;
      for (int i = 0; i < nNodes; i++) {
          chain.clear();
          for (int k = i; k >= 0 && !done[k]; k = parentNode[k]) chain.push_back(k);
          for (size_t c = chain.size(); c-- > 0;) {
              const int k = chain[c], pa = parentNode[k];
              globalM[k] = (pa >= 0) ? globalM[pa] * localM[k] : localM[k];
              done[k] = 1;
          }
      }
    }

    Object* collection = CollectionActive;
    std::string baseName = ExtractBaseName(filepath);

    // ---- las pieles -> un Armature POR PIEL ----
    //  Antes solo la primera: en un glTF con varios personajes (la escena de Marte de Bongo Cat: el gato y tres aliens
    //  bailando, cada uno con su armature de Blender) los demas quedaban sin esqueleto, en la pose de reposo, y sus
    //  canales de animacion se perdian
    const JVal* skins = doc.arr("skins");
    const size_t nSkins = (skins && skins->t == JVal::ARR) ? skins->size() : 0;
    std::vector<Armature*> arms(nSkins, (Armature*)NULL);      // el de cada piel (NULL: una piel sin joints)
    std::vector< std::map<int, int> > jointsOfSkin(nSkins);    // por piel: nodo glTF -> indice de hueso
    std::map<int, int> skinOfJoint;                            // nodo glTF -> su piel (la primera que lo nombra)
    for (size_t si = 0; si < nSkins; si++) {
        Armature*& arm = arms[si];
        std::map<int, int>& jointOfNode = jointsOfSkin[si];
        const JVal& sk = skins->arr[si];
        const JVal* joints = sk.find("joints");
        int nJoints = (joints && joints->t == JVal::ARR) ? (int)joints->size() : 0;
        if (nJoints > 0) {
            std::vector<int> jointNode(nJoints);
            for (int j = 0; j < nJoints; j++) {
                int nd = joints->arr[j].intOr(-1);
                if (nd >= nNodes) nd = -1;   // (un nodo que no existe: parentNode[nd] / globalM[nd] leian afuera)
                jointNode[j] = nd; jointOfNode[nd] = j;
            }
            std::vector<Matrix4> ibm; { int ibmAcc = sk.getI("inverseBindMatrices", -1); if (ibmAcc >= 0) doc.readMats(ibmAcc, ibm); }

            // HUESOS QUE NO PESAN: un nodo ANCESTRO de algun joint que tiene canales de animacion tambien es hueso
            // (va DESPUES de los joints: los indices de JOINTS_0 de los vertices no cambian; no pesa en ningun
            // vertice). Antes sus canales se descartaban y solo su transform de REPOSO entraba horneado en el hijo:
            // en un rig Biped de 3ds Max exportado de Unity (Bip001 = el padre de la pelvis, el que lleva la postura
            // del cuerpo, y los padres de las armas no pesan) la pelvis quedaba en reposo y las piernas torcidas
            // (encontrado con Layla de Mobile Legends: quieta, un pie apoyado y el otro levantado). Un intermedio
            // SIN animacion sigue horneado en el reposo / el prefijo del hijo, como antes.
            {
                std::set<int> animated;
                const JVal* janims = doc.arr("animations");
                if (janims) for (size_t ai = 0; ai < janims->size(); ai++) {
                    const JVal* chs = janims->arr[ai].find("channels");
                    if (chs) for (size_t c = 0; c < chs->size(); c++) {
                        const JVal* tg = chs->arr[c].find("target"); if (!tg) continue;
                        std::string pa = tg->getS("path", "");
                        if (pa == "translation" || pa == "rotation" || pa == "scale") animated.insert(tg->getI("node", -1));
                    }
                }
                std::set<int> ancestors;
                for (int j = 0; j < nJoints; j++) {
                    int p = (jointNode[j] >= 0 && jointNode[j] < nNodes) ? parentNode[jointNode[j]] : -1;
                    int guard = 0;
                    while (p >= 0 && guard++ < nNodes) { ancestors.insert(p); p = parentNode[p]; }
                }
                for (std::set<int>::iterator it = ancestors.begin(); it != ancestors.end(); ++it)
                    if (animated.count(*it) && !jointOfNode.count(*it)) {
                        jointOfNode[*it] = (int)jointNode.size();
                        jointNode.push_back(*it);
                    }
            }
            const int nBones = (int)jointNode.size();

            arm = new Armature(collection, Vector3(0, 0, 0));
            // el NOMBRE del armature: el de la piel, o el del nodo que cuelga los huesos (el objeto armature de
            // Blender, que no es hueso); si no hay ninguno, "<archivo>_rig" como antes
            { std::string nArm = sk.getS("name", "");
              if (nArm.empty() && jointNode[0] >= 0 && parentNode[jointNode[0]] >= 0 && !jointOfNode.count(parentNode[jointNode[0]]))
                  nArm = nodes->arr[parentNode[jointNode[0]]].getS("name", "");
              arm->SetNameObj(nArm.empty() ? baseName + "_rig" : nArm); } // nombre UNICO
            arm->skinGltf = true;
            arm->bones.resize(nBones);
            for (int j = 0; j < nBones; j++) {
                int nd = jointNode[j];
                W3dBone& b = arm->bones[j];
                b.name = (nd >= 0 && nd < nNodes) ? nodes->arr[nd].getS("name", "bone") : std::string("bone");
                // padre = hueso ancestro mas cercano
                int bp = -1; { int p = (nd >= 0) ? parentNode[nd] : -1; while (p >= 0) { std::map<int,int>::iterator it = jointOfNode.find(p); if (it != jointOfNode.end()) { bp = it->second; break; } p = parentNode[p]; } }
                b.parent = bp;
                // restLocal = relativo al hueso-padre (para el root incluye el prefijo de ancestros no-hueso = matriz Z-up)
                Matrix4 gj = (nd >= 0) ? globalM[nd] : Matrix4();
                Matrix4 restLocal = (bp >= 0) ? (globalM[jointNode[bp]].Inverse() * gj) : gj;
                DecomposeTRS(restLocal, b.restT, b.restR, b.restS);
                b.preRot = Vector3(0,0,0); b.postRot = Vector3(0,0,0);
                b.rotPivot = Vector3(0,0,0); b.rotOffset = Vector3(0,0,0); b.sclPivot = Vector3(0,0,0); b.sclOffset = Vector3(0,0,0);
                b.rotOrder = 0; b.hasRest = true; b.hasSkin = true;
                b.bind = gj; b.head = MatTransVec(gj); b.tail = b.head;
                b.skinInvBind = (j < (int)ibm.size()) ? ibm[j] : gj.Inverse();
                b.skinA = b.skinInvBind;
            }
            // la PUNTA de cada hueso (glTF no la guarda): por su +Y -la convencion de Blender y de los exportadores-
            // con el largo hasta la cabeza de su primer hijo; una hoja hereda el largo del padre y una raiz suelta
            // (los huesos de deformacion de un rig de Blender no tienen padre) una decima del esqueleto. Antes la
            // punta era la cabeza: el motor la inventaba 5 unidades hacia +Z y el marco de sus constraints salia mal.
            {
                std::vector<float> len(nBones, -1.0f);
                for (int j = 0; j < nBones; j++)
                    for (int k = 0; k < nBones; k++) if (arm->bones[k].parent == j) {
                        len[j] = (arm->bones[k].head - arm->bones[j].head).Length(); break; }
                Vector3 mn = arm->bones[0].head, mx = mn;
                for (int j = 1; j < nBones; j++) { const Vector3& h = arm->bones[j].head;
                    if (h.x < mn.x) mn.x = h.x; if (h.y < mn.y) mn.y = h.y; if (h.z < mn.z) mn.z = h.z;
                    if (h.x > mx.x) mx.x = h.x; if (h.y > mx.y) mx.y = h.y; if (h.z > mx.z) mx.z = h.z; }
                float def = 0.1f * (mx - mn).Length();
                if (def < 1e-6f) def = 0.1f;
                for (int j = 0; j < nBones; j++) if (len[j] < 1e-6f) {
                    float L = -1.0f; int p = arm->bones[j].parent, guard = 0;
                    while (p >= 0 && p < nBones && L < 1e-6f && guard++ < nBones) { L = len[p]; p = arm->bones[p].parent; }
                    len[j] = (L > 1e-6f) ? L : def;
                }
                for (int j = 0; j < nBones; j++) {
                    W3dBone& b = arm->bones[j];
                    const Vector3 y(b.bind.m[4], b.bind.m[5], b.bind.m[6]);
                    const float ly = y.Length();
                    b.tail = (ly > 1e-8f) ? b.head + y * (len[j] / ly) : b.head + Vector3(0, len[j], 0);
                }
            }
            PrepareSkin(arm); // en el path glTF: skinMatrix = worldFK * inverseBindMatrix
            for (int j = 0; j < nBones; j++) { arm->bones[j].poseHead = arm->bones[j].head; arm->bones[j].poseTail = arm->bones[j].tail; }
            if (nBones > nJoints) w3dLogf("ImportGLTF: %d hueso(s) animado(s) que no pesan en la piel", nBones - nJoints);
        }
        for (std::map<int, int>::iterator it = jointOfNode.begin(); it != jointOfNode.end(); ++it)
            if (!skinOfJoint.count(it->first)) skinOfJoint[it->first] = (int)si;
    }

    // ---- animaciones -> clips ----
    const JVal* anims = doc.arr("animations");
    for (size_t si = 0; si < arms.size() && anims; si++) {
        // (cada piel con SU armature: una animacion que mueve huesos de dos pieles da un clip en cada una)
        Armature* arm = arms[si];
        if (!arm) continue;
        std::map<int, int>& jointOfNode = jointsOfSkin[si];
        for (size_t ai = 0; ai < anims->size(); ai++) {
            const JVal& an = anims->arr[ai];
            const JVal* chans = an.find("channels"); const JVal* samps = an.find("samplers");
            if (!chans || !samps) continue;
            // una animacion que NO toca ningun hueso (la de la camara, la de un objeto: Blender exporta una por accion)
            // no es un clip del esqueleto: antes entraba igual, como un clip VACIO por cada una
            { bool tocaHueso = false;
              for (size_t c = 0; c < chans->size() && !tocaHueso; c++) { const JVal* tg = chans->arr[c].find("target");
                  if (tg && jointOfNode.count(tg->getI("node", -1))) tocaHueso = true; }
              if (!tocaHueso) continue; }
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
                // la salida puede venir MAS CORTA que la entrada (archivo roto):
                // clampear tc para no indexar vals[] fuera de rango
                { size_t comps = (prop == AnimRotation) ? 4u : 3u;
                  size_t groups = vals.size() / comps;
                  size_t maxTc = (groups > (size_t)voff) ? ((groups - 1 - voff) / stepV + 1) : 0;
                  if ((size_t)tc > maxTc) tc = (int)maxTc; }
                // prefijo de ancestros no-hueso (root joint): bakea la matriz Z-up en los keyframes
                Matrix4 prefix; prefix.Identity();
                { int npd = (nd >= 0) ? parentNode[nd] : -1;
                  int bpNode = -1; // node del hueso-padre (lo busca el if de abajo)
                  if (arm->bones[bone].parent >= 0) { // buscar el node del hueso-padre
                      for (std::map<int,int>::iterator it = jointOfNode.begin(); it != jointOfNode.end(); ++it) if (it->second == arm->bones[bone].parent) { bpNode = it->first; break; } }
                  if (npd != bpNode) { Matrix4 gNp = (npd >= 0) ? globalM[npd] : Matrix4(); Matrix4 gBp = (bpNode >= 0) ? globalM[bpNode] : Matrix4(); prefix = gBp.Inverse() * gNp; }
                }
                bool hasPrefix = false; for (int k = 0; k < 16; k++) { float id = (k % 5 == 0) ? 1.0f : 0.0f; if (fabsf(prefix.m[k] - id) > 1e-5f) { hasPrefix = true; break; } }
                float psp = 1.0f; if (hasPrefix) { psp = sqrtf(prefix.m[0]*prefix.m[0] + prefix.m[1]*prefix.m[1] + prefix.m[2]*prefix.m[2]); if (psp < 1e-6f) psp = 1.0f; }

                BoneTrack& tr = clip->TrackOf(bone);
                // Los samplers de glTF traen los 3 componentes JUNTOS por key. Se arma primero la lista de valores
                // (y para la rotacion se corrige la continuidad euler, que NECESITA los 3 juntos) y recien despues se
                // reparten en las 3 CURVAS independientes (una por componente).
                std::vector<int> kfr((size_t)tc); std::vector<Vector3> kv((size_t)tc);
                for (int k = 0; k < tc; k++) {
                    kfr[k] = (int)(times[k] * fps + 0.5f) + 1;
                    if (prop == AnimRotation) {
                        size_t o = ((size_t)k * stepV + voff) * 4;
                        Matrix4 Rm = QuatMat(vals[o], vals[o+1], vals[o+2], vals[o+3]);
                        if (hasPrefix) {
                            Rm = prefix * Rm;
                            // SIN LA ESCALA del prefijo (esa va en la pista de escala, por psp): SkelMatrixToEulerFBX lee el
                            // seno de Y directo de la matriz y con la escala adentro salia achicado. Los aliens de Marte
                            // (armature a escala 0.22) tenian la Y de la cadera en 7.5 grados en vez de 37: al agacharse o
                            // girar, el cuerpo entero quedaba torcido hasta 70 grados
                            for (int c = 0; c < 3; c++) {
                                const float l = sqrtf(Rm.m[c*4]*Rm.m[c*4] + Rm.m[c*4+1]*Rm.m[c*4+1] + Rm.m[c*4+2]*Rm.m[c*4+2]);
                                if (l > 1e-8f) { Rm.m[c*4] /= l; Rm.m[c*4+1] /= l; Rm.m[c*4+2] /= l; }
                            }
                        }
                        kv[k] = SkelMatrixToEulerFBX(Rm, 0);
                    } else {
                        size_t o = ((size_t)k * stepV + voff) * 3;
                        Vector3 v(vals[o], vals[o+1], vals[o+2]);
                        if (hasPrefix) { if (prop == AnimPosition) v = MatXform(prefix, v); else v = v * psp; }
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
                    bool useB = false;
                    if (dB < dA) { // el flip es mas continuo: aceptarlo SOLO si es la misma rotacion (guarda de seguridad)
                        Matrix4 MA = SkelMatRotEuler(Vector3(ax,ay,az), 0), MB = SkelMatRotEuler(Vector3(bx,by,bz), 0);
                        float diff = 0; for (int q = 0; q < 16; q++){ float d = MA.m[q]-MB.m[q]; diff += d*d; }
                        useB = (diff < 0.001f);
                    }
                    if (useB) kv[k] = Vector3(bx,by,bz); else kv[k] = Vector3(ax,ay,az);
                }
                // repartir en las 3 curvas. OJO: PropertyOf puede CREAR la propiedad (realloc del vector) -> se
                // resuelve una por bloque y se usa dentro del bloque (nunca dos referencias vivas a la vez).
                { AnimProperty& a = tr.PropertyOf(prop, AnimX); for (int k=0;k<tc;k++) SetKeyCurve(a, kfr[k], kv[k].x); }
                { AnimProperty& a = tr.PropertyOf(prop, AnimY); for (int k=0;k<tc;k++) SetKeyCurve(a, kfr[k], kv[k].y); }
                { AnimProperty& a = tr.PropertyOf(prop, AnimZ); for (int k=0;k<tc;k++) SetKeyCurve(a, kfr[k], kv[k].z); }
            }
            arm->animations.push_back(clip);
        }
        // el TAMANO del esqueleto que grabo cada clip (el retarget "rotaciones" de otro personaje con la misma
        // jerarquia lo escala contra este; ver W3dArmatureRestSize)
        { const float size = W3dArmatureRestSize(arm);
          for (size_t ci = 0; ci < arm->animations.size(); ci++) if (arm->animations[ci]) arm->animations[ci]->heightRest = size; }
        if (!arm->animations.empty()) arm->activeAnim = 0;
        w3dLogf("ImportGLTF: %d animacion(es)", (int)arm->animations.size());
    }

    // ---- materiales ----
    std::vector<Material*> mats;
    const JVal* jmats = doc.arr("materials");
    std::map<int, std::string> imgCache;   // imagen glTF -> su ruta en disco (las embebidas se extraen UNA vez)
    if (jmats) for (size_t i = 0; i < jmats->size(); i++) {
        const JVal& jm = jmats->arr[i];
        const std::string nameMat = jm.getS("name", "mat");
        // El material del glTF se lee PRIMERO a un temporal que NO se registra (el flag de
        // "material por defecto" del ctor saltea el alta en Materials y el uniquificado), para
        // poder DECIDIR si hay que crear uno o alcanza con reusar el homonimo que ya existe.
        Material tmp(nameMat, true);
        std::string texPath;
        // ¿lo escribio Whisk3D? (trae sus extras w3d_*): sus colores ya son de PANTALLA y sus numeros de brillo van
        // en los extras -> se leen tal cual (el ida y vuelta queda exacto). Lo de cualquier otro programa es lineal.
        bool deWhisk = false;
        { const JVal* ex0 = jm.find("extras");
          if (ex0 && ex0->t == JVal::OBJ) for (size_t q = 0; q < ex0->members.size() && !deWhisk; q++)
              if (ex0->members[q].first.compare(0, 4, "w3d_") == 0) deWhisk = true; }
        float metal = 1.0f, rough = 1.0f;   // (los defaults de glTF: sin factor, metalico y rugoso)
        const JVal* pbr = jm.find("pbrMetallicRoughness");
        if (pbr) { const JVal* bc = pbr->find("baseColorFactor");
            if (bc && bc->size() == 4) for (int k = 0; k < 4; k++) {
                const float v = (float)bc->arr[k].numOr(1);
                tmp.diffuse[k] = (k < 3 && !deWhisk) ? LinASrgb(v) : v;
            }
            const JVal* bct = pbr->find("baseColorTexture");
            texPath = RutaTextura(doc, bct, baseName, imgCache);   // (tambien las EMBEBIDAS de un .glb)
            if (bct) SamplerDe(doc, bct, tmp.filtered, tmp.repeat);
            metal = (float)pbr->getN("metallicFactor", 1.0);
            rough = (float)pbr->getN("roughnessFactor", 1.0);
            // METALICO / RUGOSIDAD CON TEXTURA: glTF los empaqueta en UNA imagen (G = rugosidad, B = metalico) y los
            // multiplica por su factor; los mapas de Whisk3D leen el R de SU textura -> una copia por canal
            const JVal* mrt = pbr->find("metallicRoughnessTexture");
            const std::string mr = RutaTextura(doc, mrt, baseName, imgCache);
            if (!mr.empty()) {
                const int uvk = mrt->getI("texCoord", 0);
                const float fr[3] = { rough, rough, rough }, fm[3] = { metal, metal, metal };
                Texture* tr = TextureTake(CopiaCanal(mr, 1, fr, "rugosidad"));
                Texture* tm = TextureTake(CopiaCanal(mr, 2, fm, "metalico"));
                if (tr) { tmp.mapTex[W3D_MAP_ROUGHNESS] = tr; tmp.mapWithTexture[W3D_MAP_ROUGHNESS] = true; tmp.uvMap[W3D_MAP_ROUGHNESS] = uvk; }
                if (tm) { tmp.mapTex[W3D_MAP_METALLIC] = tm; tmp.mapWithTexture[W3D_MAP_METALLIC] = true; tmp.uvMap[W3D_MAP_METALLIC] = uvk; }
            }
        }
        if (!deWhisk) { tmp.rtMetallic = metal; tmp.rtRoughness = rough; }   // (el brillo de GL sale de estos dos)
        // NORMAL MAP (la escala de glTF no tiene equivalente: va a fuerza 1)
        { const JVal* nt = jm.find("normalTexture");
          const std::string np = RutaTextura(doc, nt, baseName, imgCache);
          if (!np.empty()) {
              Texture* t = TextureTake(np);
              if (t) { tmp.normalTexture = t; tmp.normalMap = true; }
              if (nt->getN("scale", 1.0) < 0.999 || nt->getN("scale", 1.0) > 1.001)
                  w3dLogfW("ImportGLTF: '%s': normal map con escala %.2f, se usa a 1", nameMat.c_str(), nt->getN("scale", 1.0));
          } }
        // EMISION: emissiveFactor * KHR_materials_emissive_strength; con textura, el mapa de emision (su RGB ya
        // multiplicado por el factor: el mapa reemplaza al numero) y el numero queda para el pipeline fijo, que no
        // lee mapas y MODULA la emision con la textura base
        { float ef[3] = { 0, 0, 0 };
          const JVal* jef = jm.find("emissiveFactor");
          if (jef && jef->size() == 3) for (int k = 0; k < 3; k++) ef[k] = (float)jef->arr[k].numOr(0);
          float fuerza = 1.0f;
          const JVal* jext = jm.find("extensions");
          const JVal* jes = jext ? jext->find("KHR_materials_emissive_strength") : NULL;
          if (jes) fuerza = (float)jes->getN("emissiveStrength", 1.0);
          for (int k = 0; k < 3; k++) ef[k] *= fuerza;
          for (int k = 0; k < 3; k++) { const float v = ef[k] > 1.0f ? 1.0f : ef[k]; tmp.emission[k] = deWhisk ? v : LinASrgb(v); }
          const JVal* et = jm.find("emissiveTexture");
          const std::string ep = RutaTextura(doc, et, baseName, imgCache);
          if (!ep.empty() && (ef[0] > 0.0f || ef[1] > 0.0f || ef[2] > 0.0f)) {
              const bool uno = ef[0] > 0.999f && ef[0] < 1.001f && ef[1] > 0.999f && ef[1] < 1.001f && ef[2] > 0.999f && ef[2] < 1.001f;
              Texture* t = TextureTake(uno ? ep : CopiaCanal(ep, -1, ef, "emision"));
              if (t) { tmp.mapTex[W3D_MAP_EMISSION] = t; tmp.mapWithTexture[W3D_MAP_EMISSION] = true;
                       tmp.uvMap[W3D_MAP_EMISSION] = et->getI("texCoord", 0); }
          }
          // TRANSMISION / IOR (vidrio en el trazado de rayos)
          const JVal* jtr = jext ? jext->find("KHR_materials_transmission") : NULL;
          if (jtr) tmp.transmission = (float)jtr->getN("transmissionFactor", 0.0);
          const JVal* jior = jext ? jext->find("KHR_materials_ior") : NULL;
          if (jior) tmp.ior = (float)jior->getN("ior", 1.5);
          else if (jtr) tmp.ior = 1.5f;   // (el default de glTF)
        }
        // doubleSided (estandar) -> culling. Default glTF: doubleSided=false -> culling ON.
        tmp.culling = !jm.getB("doubleSided", false);
        // alphaMode MASK (estandar): recorte por alfa con alphaCutoff (default 0.5 en glTF), sin blend
        if (std::string(jm.getS("alphaMode", "OPAQUE")) == "MASK") { tmp.alphaTest = (float)jm.getN("alphaCutoff", 0.5); tmp.transparent = false; }
        // alphaMode BLEND: transparencia de verdad (antes se dibujaba opaco)
        else if (std::string(jm.getS("alphaMode", "OPAQUE")) == "BLEND") tmp.transparent = true;
        // extras: los flags de Whisk3D (round-trip exacto). Si no estan (glTF de otro programa), quedan los defaults.
        const JVal* ex = jm.find("extras");
        if (ex) {
            tmp.lighting    = ex->getB("w3d_lighting",    tmp.lighting);
            tmp.culling     = ex->getB("w3d_culling",     tmp.culling);
            tmp.textureOn   = ex->getB("w3d_textureOn",   tmp.textureOn);
            tmp.filtered    = ex->getB("w3d_filtered",    tmp.filtered);
            tmp.repeat      = ex->getB("w3d_repeat",      tmp.repeat);
            tmp.transparent = ex->getB("w3d_transparent", tmp.transparent);
            tmp.alphaTest   = (float)ex->getN("w3d_alphaCut", tmp.alphaTest);
            tmp.depth_test  = ex->getB("w3d_depthTest",   tmp.depth_test);
            tmp.vertexColor = ex->getB("w3d_vertexColor", tmp.vertexColor);
            tmp.chrome      = ex->getB("w3d_chrome",      tmp.chrome);
            tmp.normalMap   = ex->getB("w3d_normalMap",   tmp.normalMap);
            tmp.reflectMode = ex->getI("w3d_reflectMode", tmp.reflectMode);
            tmp.shininess   = (float)ex->getN("w3d_shininess", tmp.shininess);
            tmp.outline     = (float)ex->getN("w3d_outline", tmp.outline);
            // MEZCLA DE TEXTURAS (w3d_textureB: uri, w3d_mixB: 0..1): la textura B se carga sincronica por ruta
            tmp.mixB     = (float)ex->getN("w3d_mixB", tmp.mixB);
            {
                std::string tb = ex->getS("w3d_textureB", "");
                if (!tb.empty() && tb.compare(0, 5, "data:") != 0) {
                    tmp.textureB = TextureTake(doc.dir + UrlDecode(tb));
                    if (!tmp.textureB) w3dLogfW("ImportGLTF: no pude cargar la textura B '%s'", tb.c_str());
                }
            }
            // SOMBREADO TOON (w3d_toon: uri de la rampa; w3d_toonMix: "restar" o "multiplicar"): la rampa va en
            // la textureB (Materials.h); se carga sincronica por ruta como la textura B
            {
                std::string tr = ex->getS("w3d_toon", "");
                if (!tr.empty() && tr.compare(0, 5, "data:") != 0) {
                    tmp.textureB = TextureTake(doc.dir + UrlDecode(tr));
                    if (!tmp.textureB) w3dLogfW("ImportGLTF: no pude cargar la rampa toon '%s'", tr.c_str());
                    else tmp.toon = (ex->getS("w3d_toonMix", "subtract") == "multiply") ? 2 : 1;
                }
            }
            // CAPAS DE TEXTURA extra (w3d_layers): [{"textura": uri, "mezcla": n, "uv": k, "on": bool}]. Se cargan
            // sincronicas por el cache por ruta (TextureTake), como el normal map y las capas de un .mtl.
            const JVal* jc = ex->find("w3d_layers");
            if (jc && jc->t == JVal::ARR) for (size_t c = 0; c < jc->size(); c++) {
                const JVal& ce = jc->arr[c];
                std::string uri = ce.getS("texture", "");
                if (uri.empty() || uri.compare(0, 5, "data:") == 0) continue;
                TexLayer tl;
                tl.tex = TextureTake(doc.dir + UrlDecode(uri));
                if (!tl.tex) { w3dLogfW("ImportGLTF: no pude cargar la capa de textura '%s'", uri.c_str()); continue; }
                tl.blend = ce.getI("mix", 0); tl.uvMap = ce.getI("uv", 0); tl.on = ce.getB("on", true);
                tmp.layers.push_back(tl);
            }
        }
        // REUSO por nombre, pero SOLO si es REALMENTE el mismo material (mismo contenido). Sin esto,
        // dos .glb distintos con un material "Material" (el default de Blender) COLAPSABAN en el
        // primero: el segundo import se quedaba con el color/textura del otro, en silencio. Reusar
        // el homonimo identico si importa: es lo que evita que reabrir un proyecto multiplique los
        // materiales (Default.001, .002, ...). (El .obj/.mtl y el .wobj reusan por nombre A PROPOSITO:
        // ahi el 'usemtl' es una REFERENCIA al material del .mtl, no una copia; eso no se toca.)
        Material* old = FindMaterialByName(nameMat);
        if (old && SameMaterialImported(old, tmp, texPath)) { mats.push_back(old); continue; }
        Material* mat = new Material(nameMat);   // el ctor uniquifica: homonimo DISTINTO -> "X.001"
        CopyFieldsMaterial(tmp, mat);            // (el nombre no se toca: ya lo puso el ctor)
        if (!texPath.empty()) EnqueueTexture(mat, texPath);
        if (old) w3dLogfW("ImportGLTF: ya habia un material '%s' con OTRO contenido -> se creo '%s'",
                            nameMat.c_str(), mat->name.c_str());
        mats.push_back(mat);
    }

    // ---- mallas: una por NODO con "mesh" (mezcla las primitivas del mesh en mesh-parts) ----
    const JVal* meshes = doc.arr("meshes");
    int imported = 0;
    std::map<int, Object*> nodeToObj; // nodo glTF -> objeto creado (para importar las animaciones de OBJETO)
    ProgressUpdate(0.4f);
    int nMeshNodes = 0; if (meshes && nodes) for (int q = 0; q < nNodes; q++) if (nodes->arr[q].getI("mesh", -1) >= 0) nMeshNodes++;
    int meshNodeIdx = 0;

    // ---- QUE NODOS SON OBJETOS (ver PadreDeNodo) ----
    // sin objeto: los huesos y sus ancestros sin malla (el nodo del armature: horneado en el reposo de los huesos)
    std::vector<char> sinObjeto((size_t)nNodes, 0);
    std::vector<Object*> armDeNodo((size_t)nNodes, (Object*)NULL);   // el armature de un hueso (o de su nodo armature)
    for (std::map<int, int>::iterator it = skinOfJoint.begin(); it != skinOfJoint.end(); ++it) {
        int k = it->first; if (k < 0 || k >= nNodes) continue;
        Object* a = arms[(size_t)it->second];
        sinObjeto[(size_t)k] = 1;
        if (!armDeNodo[(size_t)k]) armDeNodo[(size_t)k] = a;
        int p = parentNode[k], guard = 0;
        while (p >= 0 && guard++ < nNodes) {
            if (nodes->arr[p].getI("mesh", -1) < 0) { sinObjeto[(size_t)p] = 1; if (!armDeNodo[(size_t)p]) armDeNodo[(size_t)p] = a; }
            p = parentNode[p];
        }
    }
    // los nodos que mueve alguna animacion (un vacio animado es un objeto aunque no tenga hijos)
    std::set<int> animadosObj;
    { const JVal* an0 = doc.arr("animations");
      if (an0) for (size_t a = 0; a < an0->size(); a++) { const JVal* ch = an0->arr[a].find("channels");
          if (ch) for (size_t c = 0; c < ch->size(); c++) { const JVal* t = ch->arr[c].find("target");
              if (t) { const std::string pa = t->getS("path", "");
                       if (pa == "translation" || pa == "rotation" || pa == "scale") animadosObj.insert(t->getI("node", -1)); } } } }
    // ORDEN: los padres antes que los hijos (para colgar cada objeto del de su padre, que ya existe)
    std::vector<int> prof((size_t)nNodes, 0), orden;
    for (int k = 0; k < nNodes; k++) { int p = parentNode[k], d = 0; while (p >= 0 && d < nNodes) { d++; p = parentNode[p]; } prof[(size_t)k] = d; }
    for (int d = 0; d <= nNodes && (int)orden.size() < nNodes; d++)
        for (int k = 0; k < nNodes; k++) if (prof[(size_t)k] == d) orden.push_back(k);
    // un nodo es objeto si tiene malla, camara o luz, si lo anima algo, o si algun hijo es objeto (de los hijos para
    // arriba: el orden al reves)
    std::vector<char> esObjeto((size_t)nNodes, 0);
    for (size_t oi = orden.size(); oi-- > 0;) {
        const int k = orden[oi];
        const JVal& nk = nodes->arr[k];
        const JVal* ek = nk.find("extensions");
        if (!sinObjeto[(size_t)k] && (nk.has("mesh") || nk.has("camera") || (ek && ek->has("KHR_lights_punctual")) ||
                                       animadosObj.count(k)))
            esObjeto[(size_t)k] = 1;
        if (esObjeto[(size_t)k] && parentNode[k] >= 0 && !sinObjeto[(size_t)parentNode[k]]) esObjeto[(size_t)parentNode[k]] = 1;
    }
    std::vector<Object*> nodeObj((size_t)nNodes, (Object*)NULL);   // el objeto de cada nodo (NULL = ninguno)
    std::vector<Matrix4> mundoPadreDe((size_t)nNodes);              // el mundo del padre en Whisk3D (las curvas de objeto)
    for (size_t k = 0; k < mundoPadreDe.size(); k++) mundoPadreDe[k].Identity();   // (Matrix4 es POD: nace en ceros)
    std::set<int> soles;                                            // (luces direccionales: llevan +90 en X)

    for (size_t oi = 0; oi < orden.size(); oi++) {
        const int nd = orden[oi];
        const JVal& node = nodes->arr[nd];
        if (sinObjeto[(size_t)nd] || !esObjeto[(size_t)nd]) continue;
        int mi = node.getI("mesh", -1);
        if (!meshes || mi < 0 || mi >= (int)meshes->size()) {
            // ---- CAMARA, LUZ o VACIO ----
            Matrix4 mp;
            Object* padre = PadreDeNodo(nd, parentNode, nodeObj, sinObjeto, globalM, armDeNodo, collection, mp);
            bool esSol = false;
            Object* o = node.has("camera") ? CrearCamara(doc, node, padre) : NULL;
            if (!o) o = CrearLuz(doc, node, padre, esSol);
            if (!o) o = new Empty(padre);
            const std::string nm = node.getS("name", "");
            o->SetNameObj(nm.empty() ? o->name : nm);   // nombre UNICO en la escena destino
            // el SOL de Whisk3D alumbra desde su +Y local (la lampara apunta a su -Y); el de glTF apunta a su -Z
            Matrix4 W = globalM[nd];
            if (esSol) { W = W * QuatMat(0.70710678f, 0.0f, 0.0f, 0.70710678f); soles.insert(nd); }
            const bool sinEscala = (o->getType() == ObjectType::camera || o->getType() == ObjectType::light);
            UbicarObjeto(o, mp, W, sinEscala);
            nodeObj[(size_t)nd] = o; nodeToObj[nd] = o; mundoPadreDe[(size_t)nd] = mp;
            continue;
        }
        ProgressUpdate(0.4f + 0.6f * (nMeshNodes ? (float)(meshNodeIdx++) / (float)nMeshNodes : 0.0f)); // 40%..100% por malla
        const int skinIdx = node.getI("skin", -1);
        Armature* arm = (skinIdx >= 0 && skinIdx < (int)arms.size()) ? arms[(size_t)skinIdx] : NULL;   // (el de SU piel)
        bool skinned = arm != NULL;
        const JVal& gm = meshes->arr[mi];
        const JVal* prims = gm.find("primitives"); if (!prims || prims->t != JVal::ARR) continue;

        Wavefront Wobj; Wobj.Reset();
        // por-primitiva: control-points (posiciones), normales/uv por corner, caras, material group
        std::vector<WeightCP> weights; // (control-point, hueso, peso) -> vertexGroups (WeightCP definido a nivel de archivo)
        int cpBase = 0;
        // MORPH TARGETS -> shape keys: delta POSITION por control-point (denso mientras se lee; esparso al guardar)
        std::vector< std::vector<float> > shapesCp;
        for (size_t p = 0; p < prims->size(); p++) {
            const JVal& pr = prims->arr[p];
            const JVal* at = pr.find("attributes"); if (!at) continue;
            int posA = at->getI("POSITION", -1); if (posA < 0) continue;
            std::vector<float> POS; int pc = 0, pnc = 0; if (!doc.readFloats(posA, POS, pc, pnc) || pnc < 3) continue;
            std::vector<float> NRM; int nc = 0, nnc = 0; { int a = at->getI("NORMAL", -1); if (a >= 0) doc.readFloats(a, NRM, nc, nnc); }
            std::vector<float> UV;  int uc = 0, unc = 0; { int a = at->getI("TEXCOORD_0", -1); if (a >= 0) doc.readFloats(a, UV, uc, unc); }
            // TEXCOORD_1: la SEGUNDA capa UV (el unwrap de un lightmap) -> Wobj.uv2 / FaceCorner::uv2 -> Mesh::uvExtra[1]
            std::vector<float> UV2; int u2c = 0, u2nc = 0; { int a = at->getI("TEXCOORD_1", -1); if (a >= 0) doc.readFloats(a, UV2, u2c, u2nc); }
            // COLOR_0 (vertex color, ej: la iluminacion HORNEADA de un escenario): por vertice -> por control-point
            std::vector<float> COL; int cc = 0, cnc = 0; { int a = at->getI("COLOR_0", -1); if (a >= 0) doc.readFloats(a, COL, cc, cnc); }
            std::vector<float> JNT; int jc = 0, jnc = 0; { int a = at->getI("JOINTS_0", -1); if (a >= 0) doc.readFloats(a, JNT, jc, jnc); }
            std::vector<float> WGT; int wc = 0, wnc = 0; { int a = at->getI("WEIGHTS_0", -1); if (a >= 0) doc.readFloats(a, WGT, wc, wnc); }
            std::vector<uint32_t> IDX; { int a = pr.getI("indices", -1); if (a >= 0) doc.readIndices(a, IDX); }
            if (IDX.empty()) { IDX.resize(pc); for (int i = 0; i < pc; i++) IDX[i] = i; } // sin indices: secuencial

            // control-points de esta primitiva -> Wobj.vertex. Estatica: se dejan en LOCAL (el objeto lleva el
            // transform del nodo, ver abajo) para que cada objeto tenga su ORIGEN propio -como en Blender- y se
            // pueda rotar desde su centro. Skinned: espacio escena crudo (lo ubica el armature).
            for (int i = 0; i < pc; i++) { Vector3 v(POS[(size_t)i*pnc], POS[(size_t)i*pnc+1], POS[(size_t)i*pnc+2]);
                Wobj.vertex.push_back(v.x); Wobj.vertex.push_back(v.y); Wobj.vertex.push_back(v.z); }
            // vertex color por control-point (paralelo a Wobj.vertex). Si ESTA primitiva no trae COLOR_0 pero otra
            // si, se completa en blanco para que el arreglo quede alineado con las posiciones.
            if (cnc >= 3 && cc >= pc) {
                if (Wobj.vertexColor.size() < (size_t)cpBase * 4) Wobj.vertexColor.resize((size_t)cpBase * 4, 255);
                for (int i = 0; i < pc; i++) for (int e = 0; e < 4; e++) {
                    float f = (e < cnc) ? COL[(size_t)i*cnc+e] : 1.0f;
                    if (f < 0.0f) f = 0.0f; if (f > 1.0f) f = 1.0f;
                    Wobj.vertexColor.push_back((GLubyte)(f * 255.0f + 0.5f));
                }
            } else if (!Wobj.vertexColor.empty()) Wobj.vertexColor.resize((size_t)(cpBase + pc) * 4, 255);
            // MORPH TARGETS de esta primitiva (targets[k].POSITION = delta por vertice)
            { const JVal* tg = pr.find("targets");
              if (tg && tg->t == JVal::ARR) {
                  if (shapesCp.size() < tg->size()) shapesCp.resize(tg->size());
                  for (size_t k = 0; k < tg->size(); k++) {
                      std::vector<float>& fk = shapesCp[k];
                      fk.resize((size_t)(cpBase + pc) * 3, 0.0f);
                      int a = tg->arr[k].getI("POSITION", -1); if (a < 0) continue;
                      std::vector<float> D; int dc = 0, dnc = 0; if (!doc.readFloats(a, D, dc, dnc) || dnc < 3) continue;
                      for (int i = 0; i < pc && i < dc; i++)
                          for (int e = 0; e < 3; e++) fk[(size_t)(cpBase + i) * 3 + e] = D[(size_t)i * dnc + e];
                  }
              } }

            // pesos por control-point (skinned): joint LOCAL de la primitiva -> hueso global
            const JVal* jointsArr = skinned ? skins->arr[(size_t)skinIdx].find("joints") : NULL;
            // (jc / wc >= pc: un accessor de pesos o de joints mas corto que las posiciones se leia afuera)
            if (skinned && jnc >= 4 && wnc >= 4 && jc >= pc && wc >= pc && jointsArr) {
                for (int i = 0; i < pc; i++) for (int e = 0; e < 4; e++) {
                    float w = WGT[(size_t)i*wnc+e]; if (w <= 0.0001f) continue;
                    int localJ = (int)(JNT[(size_t)i*jnc+e] + 0.5f);
                    if (localJ < 0 || localJ >= (int)jointsArr->size()) continue;
                    WeightCP pw; pw.cp = cpBase + i; pw.joint = localJ; pw.w = w; weights.push_back(pw);
                }
            }

            // caras (triangulos) -> Face con corners (cp + normal + uv por corner; en glTF son por-vertice)
            // Las esquinas de un MISMO vertice del glTF comparten su normal y sus UV (una entrada por vertice, no por
            // esquina): ConvertToES1 las junta en UN vertice de render, que es lo que dice el indice del glTF. Con una
            // entrada por esquina salian 3 vertices por triangulo (3 a 5 veces mas memoria, VBO y skinning)
            int matGroupStart = (int)Wobj.faces.size();
            std::vector<int> nrmOf((size_t)pc, -1), uvOf((size_t)pc, -1), uv2Of((size_t)pc, -1);
            std::vector<Face> primTris; primTris.reserve(IDX.size() / 3);   // (-> poligonos con extras.w3d_polygons)
            bool primDropped = false;
            for (size_t f = 0; f + 2 < IDX.size(); f += 3) {
                Face face;
                for (int t = 0; t < 3; t++) { int vi = (int)IDX[f + t];
                    // indice fuera de ESTA primitiva (archivo roto): descartar la cara
                    // entera (sino apuntaba en silencio a los control-points de la siguiente)
                    if (vi < 0 || vi >= pc) { face.corners.clear(); break; }
                    FaceCorner fc; fc.vertex = cpBase + vi; fc.color = -1; fc.normal = -1; fc.uv = -1;
                    if (nnc >= 3 && vi < nc) {
                        if (nrmOf[(size_t)vi] < 0) {
                            Wobj.normals.push_back((GLbyte)(int)(NRM[(size_t)vi*nnc] * 127.0f));
                            Wobj.normals.push_back((GLbyte)(NRM[(size_t)vi*nnc+1] * 127.0f)); Wobj.normals.push_back((GLbyte)(NRM[(size_t)vi*nnc+2] * 127.0f));
                            nrmOf[(size_t)vi] = (int)(Wobj.normals.size() / 3) - 1;
                        }
                        fc.normal = nrmOf[(size_t)vi]; }
                    if (unc >= 2 && vi < uc) {
                        if (uvOf[(size_t)vi] < 0) {
                            Wobj.uv.push_back(UV[(size_t)vi*unc]); Wobj.uv.push_back(UV[(size_t)vi*unc+1]);
                            uvOf[(size_t)vi] = (int)(Wobj.uv.size() / 2) - 1;
                        }
                        fc.uv = uvOf[(size_t)vi]; }
                    if (u2nc >= 2 && vi < u2c) {
                        if (uv2Of[(size_t)vi] < 0) {
                            Wobj.uv2.push_back(UV2[(size_t)vi*u2nc]); Wobj.uv2.push_back(UV2[(size_t)vi*u2nc+1]);
                            uv2Of[(size_t)vi] = (int)(Wobj.uv2.size() / 2) - 1;
                        }
                        fc.uv2 = uv2Of[(size_t)vi]; }
                    face.corners.push_back(fc);
                }
                if (face.corners.size() == 3) primTris.push_back(face); else primDropped = true;
            }
            PushPrimitiveFaces(pr, primTris, primDropped, Wobj.faces);
            // material group de esta primitiva. El NOMBRE de la parte viaja en "extras" (lo escribe
            // ExportGLTF: el glTF no tiene un "name" por primitiva). Sin ese dato se rearmaba del
            // material -> una malla con DOS partes del mismo material volvia con dos partes homonimas
            // y se perdian los nombres que le puso el usuario. Igual se uniquifica: un .glb de otro
            // programa (o editado a mano) puede traer dos extras iguales.
            int matIdx = pr.getI("material", -1);
            Material* mat = (matIdx >= 0 && matIdx < (int)mats.size()) ? mats[matIdx] : NULL;
            int cnt = (int)Wobj.faces.size() - matGroupStart;
            if (cnt > 0) { MaterialGroup mg; mg.material = mat;
                std::string namePart = mat ? mat->name : std::string("Mesh");
                { const JVal* pex = pr.find("extras"); if (pex) namePart = pex->getS("w3d_part", namePart.c_str()); }
                { std::vector<std::string> taken;
                  for (size_t q = 0; q < Wobj.materialsGroup.size(); q++) taken.push_back(Wobj.materialsGroup[q].name);
                  mg.name = W3dUniqueNameInValues(namePart, "Mesh", taken, -1); }
                mg.start = matGroupStart; mg.count = 0; mg.startDrawn = 0; mg.indicesDrawnCount = 0; Wobj.materialsGroup.push_back(mg); }
            cpBase += pc;
        }
        if (Wobj.faces.empty()) continue;

        // la piel va colgada del armature, en el origen (la ubica el esqueleto); lo demas, de su padre del glTF
        Matrix4 mp;
        Object* parentMesh = skinned ? (Object*)arm
                                     : PadreDeNodo(nd, parentNode, nodeObj, sinObjeto, globalM, armDeNodo, collection, mp);
        Mesh* mesh = new Mesh(parentMesh, Vector3(0, 0, 0));
        mesh->SetNameObj(NombreNodo(node, &gm, baseName, imported)); // el nombre del NODO, UNICO en la escena destino
        nodeToObj[nd] = mesh;   // para las animaciones de objeto (channels que apuntan a este nodo)
        // malla estatica: el objeto se queda con su PROPIO origen = el transform del nodo, LOCAL a su padre. Los
        // vertices ya quedaron en LOCAL (relativos a ese origen). Skinned: origen en (0,0,0).
        if (!skinned) {
            UbicarObjeto(mesh, mp, globalM[nd], false);
            nodeObj[(size_t)nd] = mesh; mundoPadreDe[(size_t)nd] = mp;
        }
        int a0 = 0, a1 = 0, a2 = 0;
        Wobj.ConvertToES1(mesh, &a0, &a1, &a2, &mesh->vertCtrlPoint);
        // SHAPE KEYS (morph targets): esparsas por control-point, con el nombre de mesh.extras.targetNames
        if (!shapesCp.empty()) {
            const JVal* mex = gm.find("extras");
            const JVal* tn = mex ? mex->find("targetNames") : NULL;
            for (size_t k = 0; k < shapesCp.size(); k++) {
                W3dShapeKey sk;
                char nb[32]; snprintf(nb, sizeof nb, "Shape %d", (int)k);
                sk.name = (tn && tn->t == JVal::ARR && k < tn->size() && tn->arr[k].t == JVal::STR) ? tn->arr[k].str : std::string(nb);
                const std::vector<float>& fk = shapesCp[k];
                for (size_t c = 0; c * 3 + 2 < fk.size(); c++)
                    if (fk[c*3] != 0.0f || fk[c*3+1] != 0.0f || fk[c*3+2] != 0.0f) {
                        sk.idx.push_back((int)c); sk.d.push_back(fk[c*3]); sk.d.push_back(fk[c*3+1]); sk.d.push_back(fk[c*3+2]);
                    }
                mesh->shapeKeys.push_back(sk);
            }
            mesh->shapeWeights.assign(mesh->shapeKeys.size(), 0.0f);
        }
        mesh->ComputeBorders();
        if (!mesh->normals && mesh->vertexSize > 0) { mesh->normals = new GLbyte[mesh->vertexSize * 3]; mesh->meshSmooth = true; mesh->RecalculateNormals(); }
        else if (mesh->normals && mesh->vertexSize > 0) mesh->meshSmooth = MeshShadingImportedIsSmooth(mesh);
#ifndef W3D_SYMBIAN
        mesh->OptimizeCacheRender();
#endif
        // vertex groups (pesos por hueso) desde 'pesos' (por control-point)
        if (skinned && !weights.empty()) {
            std::map<int, VertexGroup*> grpOf; // hueso -> grupo
            for (size_t i = 0; i < weights.size(); i++) { WeightCP& pw = weights[i];
                std::map<int, VertexGroup*>::iterator it = grpOf.find(pw.joint);
                VertexGroup* g;
                if (it == grpOf.end()) { g = new VertexGroup(arm->bones[pw.joint].name); grpOf[pw.joint] = g; mesh->vertexGroups.push_back(g); }
                else g = it->second;
                g->verts.push_back(pw.cp); g->weights.push_back(pw.w);
            }
            if (!mesh->vertexGroups.empty()) mesh->activeGroup = 0;
            // la malla con piel queda PEGADA a su esqueleto aunque el glTF no traiga animaciones: posarlo a mano, un
            // clip hecho en el editor o los constraints de hueso (IK, Copy Transforms...) la tienen que deformar. Antes
            // solo se enganchaba con clips y un rig sin animar quedaba suelto: se movian los huesos y la malla no.
            Modifier* amod = new Modifier(ModifierType::Armature, "Armature"); amod->target = arm;
            mesh->modifiers.push_back(amod); mesh->activeModifier = (int)mesh->modifiers.size() - 1;
            mesh->skinArmature = arm;
        }
        imported++;
    }

    // ---- animaciones de OBJETO ----
    //  Blender exporta UNA animacion glTF por ACCION (la de la camara, la de cada tecla de un piano...) y en el archivo
    //  original corren JUNTAS, en la misma linea de tiempo. Antes cada una abria su propia ESCENA (siete teclas = siete
    //  escenas, y la camara en otra). Ahora van juntas a UNA escena mientras no toquen el mismo nodo; una animacion que
    //  vuelve a mover un nodo ya animado es OTRA TOMA (caminar / correr) y abre otra escena. Si el esqueleto tiene un
    //  solo clip, la escena lo toca con su TIRA desde el frame 1 (sin ella el timeline de la escena no mueve al rig).
    //  Las rotaciones van al euler de los OBJETOS (Quaternion::ToEulerXYZ, R = Rx*Ry*Rz: el de SetRotEuler y el del
    //  exportador). Antes salian en el orden FBX de los huesos (Rz*Ry*Rx) y una rotacion en mas de un eje entraba torcida.
    //  Los tiempos -> frames con el fps del archivo (extras.frameRate o, si no esta, el menor paso entre keys, como los
    //  clips; antes 30 fijo). Interpolacion: STEP -> constante, LINEAR -> lineal, CUBICSPLINE -> bezier (sus valores).
    if (anims && !nodeToObj.empty()) {
        std::vector< std::vector<int> > grupos;
        std::vector< std::set<int> > nodosGrupo;
        for (size_t a = 0; a < anims->size(); a++) {
            const JVal* chans = anims->arr[a].find("channels"); if (!chans) continue;
            std::set<int> ns;
            for (size_t c = 0; c < chans->size(); c++) { const JVal* t = chans->arr[c].find("target");
                if (t && nodeToObj.count(t->getI("node", -1))) ns.insert(t->getI("node", -1)); }
            if (ns.empty()) continue;
            size_t g = 0;
            for (; g < grupos.size(); g++) {
                bool choca = false;
                for (std::set<int>::iterator it = ns.begin(); it != ns.end() && !choca; ++it) if (nodosGrupo[g].count(*it)) choca = true;
                if (!choca) break;
            }
            if (g == grupos.size()) { grupos.push_back(std::vector<int>()); nodosGrupo.push_back(std::set<int>()); }
            grupos[g].push_back((int)a);
            nodosGrupo[g].insert(ns.begin(), ns.end());
        }
        if (!grupos.empty()) InitSceneAnimations();
        const Quaternion qSol(0.70710678f, 0.70710678f, 0.0f, 0.0f);   // +90 en X (el sol: ver CrearLuz)
        for (size_t g = 0; g < grupos.size(); g++) {
            NewScene();   // (guarda la activa y deja AnimationObjects vacio)
            // el nombre unico lo resuelve RenameActiveScene (LA regla comun)
            RenameActiveScene(grupos[g].size() == 1 ? anims->arr[(size_t)grupos[g][0]].getS("name", "Scene") : baseName);
            ActiveAnimKind = 0; // modo ESCENA (para que AnimSet* toquen esta escena, no un clip de armature)
            int maxFrame = 1, fpsEsc = 0;
            for (size_t ga = 0; ga < grupos[g].size(); ga++) {
                const JVal& an = anims->arr[(size_t)grupos[g][ga]];
                const JVal* chans = an.find("channels"); const JVal* samps = an.find("samplers");
                if (!chans || !samps) continue;
                int fps = 0; { const JVal* ex = an.find("extras"); if (ex) fps = ex->getI("frameRate", 0); }
                if (fps <= 0) {
                    float dtMin = 1e9f;
                    for (size_t s = 0; s < samps->size(); s++) { std::vector<float> ts; int c = 0, nc = 0;
                        if (!doc.readFloats(samps->arr[s].getI("input", -1), ts, c, nc)) continue;
                        for (int k = 1; k < c; k++) { const float d = ts[(size_t)k] - ts[(size_t)k - 1]; if (d > 1e-5f && d < dtMin) dtMin = d; } }
                    fps = (dtMin < 1e8f) ? (int)(1.0f / dtMin + 0.5f) : 30;
                    if (fps < 1) fps = 30; if (fps > 240) fps = 240;
                }
                if (fpsEsc == 0) fpsEsc = fps;
                for (size_t c = 0; c < chans->size(); c++) {
                    const JVal& ch = chans->arr[c];
                    const JVal* tgt = ch.find("target"); if (!tgt) continue;
                    const int nd = tgt->getI("node", -1);
                    std::map<int, Object*>::iterator mit = nodeToObj.find(nd);
                    if (mit == nodeToObj.end()) continue;   // (un hueso: ya lo hizo el clip)
                    Object* obj = mit->second;
                    const std::string path = tgt->getS("path", "");
                    const int prop = (path=="translation")?AnimPosition:(path=="rotation")?AnimRotation:(path=="scale")?AnimScale:-1;
                    if (prop < 0) continue;
                    const int sampIdx = ch.getI("sampler", -1); if (sampIdx < 0 || sampIdx >= (int)samps->size()) continue;
                    const JVal& sm = samps->arr[(size_t)sampIdx];
                    std::vector<float> times; int tc=0,tnc=0; if (!doc.readFloats(sm.getI("input",-1), times, tc, tnc)) continue;
                    std::vector<float> vals; int vc=0,vnc=0; if (!doc.readFloats(sm.getI("output",-1), vals, vc, vnc)) continue;
                    const std::string interp = sm.getS("interpolation","LINEAR");
                    const int stepV = (interp=="CUBICSPLINE")?3:1, voff = (interp=="CUBICSPLINE")?1:0;
                    // (misma cota que el bloque de huesos: la salida corta no debe desbordar vals[])
                    { const size_t comps = (prop == AnimRotation) ? 4u : 3u;
                      const size_t groupsV = vals.size() / comps;
                      const size_t maxTc = (groupsV > (size_t)voff) ? ((groupsV - 1 - voff) / stepV + 1) : 0;
                      if ((size_t)tc > maxTc) tc = (int)maxTc; }
                    // PREFIJO: la curva del glTF es local a SU padre; el objeto cuelga de su padre en Whisk3D, que puede
                    // ser otro (el armature por el nodo del armature, la coleccion...). P = inv(mundo del padre en
                    // Whisk3D) * mundo del padre en el glTF; con escala uniforme se aplica por canal (como en los huesos)
                    Matrix4 gPadre; gPadre.Identity();
                    if (parentNode[nd] >= 0) gPadre = globalM[parentNode[nd]];
                    const Matrix4 P = mundoPadreDe[(size_t)nd].Inverse() * gPadre;
                    bool conP = false;
                    for (int k = 0; k < 16; k++) { const float id = (k % 5 == 0) ? 1.0f : 0.0f; if (fabsf(P.m[k] - id) > 1e-5f) { conP = true; break; } }
                    float pEsc = 1.0f; Quaternion qP(1.0f, 0.0f, 0.0f, 0.0f);
                    if (conP) {
                        pEsc = sqrtf(P.m[0]*P.m[0] + P.m[1]*P.m[1] + P.m[2]*P.m[2]); if (pEsc < 1e-6f) pEsc = 1.0f;
                        Matrix4 Pr = P; Pr.m[12] = Pr.m[13] = Pr.m[14] = 0.0f;
                        for (int k = 0; k < 3; k++) for (int r = 0; r < 3; r++) Pr.m[k*4 + r] /= pEsc;
                        qP = Quaternion::FromMatrix(Pr);
                    }
                    const bool sol = soles.count(nd) != 0;
                    const bool sinEscala = (obj->getType() == ObjectType::camera || obj->getType() == ObjectType::light);
                    AnimationObject* ao = NULL;
                    for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj == obj){ ao=&AnimationObjects[i]; break; }
                    if (!ao){ AnimationObject newValue; newValue.obj=obj; AnimationObjects.push_back(newValue); ao=&AnimationObjects.back(); }
                    std::vector<int> kfr((size_t)tc); std::vector<Vector3> kv((size_t)tc);
                    for (int k=0;k<tc;k++){
                        kfr[(size_t)k] = (int)(times[(size_t)k]*fps + 0.5f) + 1;
                        if (prop==AnimRotation){ const size_t o=((size_t)k*stepV+voff)*4;
                            Quaternion q(vals[o+3], vals[o], vals[o+1], vals[o+2]);   // (glTF: x y z w)
                            if (conP) q = qP * q;
                            if (sol) q = q * qSol;
                            kv[(size_t)k] = q.ToEulerXYZ(); }
                        else { const size_t o=((size_t)k*stepV+voff)*3; Vector3 v(vals[o],vals[o+1],vals[o+2]);
                            if (conP) { if (prop == AnimPosition) v = MatXform(P, v); else v = v * pEsc; }
                            if (prop == AnimScale && sinEscala) v = Vector3(1, 1, 1);
                            kv[(size_t)k] = v; }
                    }
                    // EULER CONTINUO: cada key salio del cuaternion por separado -> puede saltar 360 o caer en la otra
                    // representacion (x+180, 180-y, z+180: la MISMA rotacion en R = Rx*Ry*Rz); gana la mas cercana
                    if (prop==AnimRotation) for (size_t k=1;k<kv.size();k++){
                        const Vector3 pv = kv[k-1];
                        Vector3 va = kv[k], vb(kv[k].x + 180.0f, 180.0f - kv[k].y, kv[k].z + 180.0f);
                        float* ca[3] = { &va.x, &va.y, &va.z }; float* cb[3] = { &vb.x, &vb.y, &vb.z };
                        const float pr[3] = { pv.x, pv.y, pv.z };
                        for (int comp=0;comp<3;comp++){
                            while (*ca[comp]-pr[comp] >  180.0f) *ca[comp]-=360.0f;
                            while (*ca[comp]-pr[comp] < -180.0f) *ca[comp]+=360.0f;
                            while (*cb[comp]-pr[comp] >  180.0f) *cb[comp]-=360.0f;
                            while (*cb[comp]-pr[comp] < -180.0f) *cb[comp]+=360.0f; }
                        const float dA = fabsf(va.x-pv.x)+fabsf(va.y-pv.y)+fabsf(va.z-pv.z);
                        const float dB = fabsf(vb.x-pv.x)+fabsf(vb.y-pv.y)+fabsf(vb.z-pv.z);
                        kv[k] = (dB < dA) ? vb : va;
                    }
                    const int interpKf = (interp == "STEP") ? KfConstant : (interp == "LINEAR") ? KfLinear : KfBezier;
                    for (int comp=0;comp<3;comp++){ AnimProperty& ap = ListProperty(ao->Properties, prop, comp);
                        for (int k=0;k<tc;k++){ const float v3[3]={kv[(size_t)k].x,kv[(size_t)k].y,kv[(size_t)k].z}; SetKeyCurve(ap, kfr[(size_t)k], v3[comp]); }
                        if (interpKf != KfBezier) for (size_t q = 0; q < ap.keyframes.size(); q++) ap.keyframes[q].Interpolation = interpKf; }
                    ao->UpdateFirstLastFrame();
                    for (int k=0;k<tc;k++) if (kfr[(size_t)k] > maxFrame) maxFrame = kfr[(size_t)k];
                }
            }
            // la TIRA del esqueleto: con UN solo clip es la animacion del personaje en esta misma linea de tiempo
            for (size_t si = 0; si < arms.size(); si++) {
                Armature* arm = arms[si];
                if (!(arm && arm->animations.size() == 1 && arm->animations[0] && ActiveSceneAnim >= 0 &&
                      ActiveSceneAnim < (int)SceneAnimations.size() && SceneAnimations[(size_t)ActiveSceneAnim])) continue;
                SceneAnimation::SkeletonTrack tira;
                tira.armature = arm->name; tira.clip = arm->animations[0]->name; tira.frame = 1;
                SceneAnimations[(size_t)ActiveSceneAnim]->skeletons.push_back(tira);
                if (arm->animations[0]->endFrame > maxFrame) maxFrame = arm->animations[0]->endFrame;
            }
            // rango + fps PROPIOS de esta escena (no 250 default): asi el timeline muestra justo la animacion importada
            AnimSetStart(1); AnimSetEnd(maxFrame > 1 ? maxFrame : 1); AnimSetFps(fpsEsc > 0 ? fpsEsc : 30);
        }
    }

    bool anyArm = false; for (size_t si = 0; si < arms.size(); si++) if (arms[si]) anyArm = true;
    if (imported == 0 && !anyArm) { w3dLogfE("ImportGLTF: sin mallas ni esqueleto"); ProgressEnd(); Notify(T("glTF: nothing to import"), true); return false; }
    ProgressEnd();
    w3dLogf("ImportGLTF: %d malla(s), %s", imported, filepath.c_str());
    Notify(T("glTF imported successfully!"), false);
    return true;
}
