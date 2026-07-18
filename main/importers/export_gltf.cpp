#include "export_gltf.h"
#include "import_obj.h"                  // ExtractBaseName
#include "objects/Mesh.h"
#include "objects/Armature.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Objects.h"             // SceneCollection / ObjectType
#include "objects/ObjectMode.h"          // RotGlobalDe (normales a mundo en mallas estaticas)
#include "animation/SkeletalAnimation.h" // SkelMatRotEuler + BoneTrack/SkeletalAnimation
#include "animation/Animation.h"         // AnimProperty / keyFrame + AnimPosition/Rotation/Scale
#include "ViewPorts/PopUp/ProgressPopup.h" // ProgresoIniciar/Actualizar
#include "ViewPorts/LayoutInput.h"       // Notificar
#include "math/Quaternion.h"
#include "math/Matrix4.h"
#include "math/Vector3.h"
#include "w3dFilesystem.h"
#include "w3dlog.h"
#include <vector>
#include <string>
#include <map>
#include <utility>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
// extern "C": la def es un EM_JS en main.cpp (linkage C). Sin esto el link falla con "undefined symbol
// WebDescargarArchivo(char const*, char const*)" (busca el simbolo C++-mangled). Igual que Properties.cpp / ViewPort3D.cpp.
extern "C" void WebDescargarArchivo(const char* rutaVFS, const char* nombreSugerido); // main.cpp
#endif

// ============================================================================
//  Exportador glTF 2.0 (.gltf embebido / .glb). Inverso de import_gltf.cpp: la
//  malla va en BIND pose con JOINTS/WEIGHTS por vertice, los huesos como nodos
//  con TRS (rest), inverseBindMatrices = skinInvBind, y cada clip como un
//  animation con samplers TRS por hueso (euler -> quaternion). No hornea nada.
// ============================================================================

namespace {

// ---- serializacion binaria (little-endian; x86/ARM Symbian lo son) ----
void putF(std::vector<unsigned char>& b, float f) { unsigned char* p = (unsigned char*)&f; b.push_back(p[0]); b.push_back(p[1]); b.push_back(p[2]); b.push_back(p[3]); }
void putU32(std::vector<unsigned char>& b, uint32_t v) { b.push_back((unsigned char)(v & 0xFF)); b.push_back((unsigned char)((v >> 8) & 0xFF)); b.push_back((unsigned char)((v >> 16) & 0xFF)); b.push_back((unsigned char)((v >> 24) & 0xFF)); }
void putU16(std::vector<unsigned char>& b, uint16_t v) { b.push_back((unsigned char)(v & 0xFF)); b.push_back((unsigned char)((v >> 8) & 0xFF)); }

// ---- base64 (buffer embebido del .gltf) ----
void b64encode(const std::vector<unsigned char>& in, std::string& out) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        uint32_t n = ((uint32_t)in[i] << 16) | ((uint32_t)in[i+1] << 8) | in[i+2];
        out += T[(n >> 18) & 63]; out += T[(n >> 12) & 63]; out += T[(n >> 6) & 63]; out += T[n & 63];
    }
    size_t rem = in.size() - i;
    if (rem == 1) { uint32_t n = (uint32_t)in[i] << 16; out += T[(n >> 18) & 63]; out += T[(n >> 12) & 63]; out += "=="; }
    else if (rem == 2) { uint32_t n = ((uint32_t)in[i] << 16) | ((uint32_t)in[i+1] << 8); out += T[(n >> 18) & 63]; out += T[(n >> 12) & 63]; out += T[(n >> 6) & 63]; out += "="; }
}

// ---- helpers de texto JSON ----
std::string Itos(long v) { char b[32]; snprintf(b, sizeof(b), "%ld", v); return b; }
std::string Ftos(double v) { char b[40]; snprintf(b, sizeof(b), "%.7g", v); return b; }
std::string Jesc(const std::string& s) {
    std::string o; for (size_t i = 0; i < s.size(); i++) { char c = s[i];
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n"; else if (c == '\t') o += "\\t"; else if (c == '\r') o += "\\r";
        else if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", (unsigned char)c); o += b; }
        else o += c; }
    return o;
}
// rutas glTF: barras Unix (Windows manda '\'); el importador hace UrlDecode -> emitimos crudo con '/'
std::string PathUri(const std::string& p) { std::string o; for (size_t i = 0; i < p.size(); i++) o += (p[i] == '\\') ? '/' : p[i]; return o; }
std::string DirOf(const std::string& p) { size_t s = p.find_last_of("/\\"); return (s == std::string::npos) ? std::string() : p.substr(0, s + 1); }
std::string BaseName(const std::string& p) { size_t s = p.find_last_of("/\\"); return (s == std::string::npos) ? p : p.substr(s + 1); }

std::string RecolectarMeshes(Object* o, bool selectedOnly, std::vector<Mesh*>& out) {
    if (o) for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* c = o->Childrens[i];
        if (c->getType() == ObjectType::mesh && (!selectedOnly || c->select)) out.push_back((Mesh*)c);
        RecolectarMeshes(c, selectedOnly, out);
    }
    return std::string();
}

// euler (grados, orden del hueso) -> quaternion (x,y,z,w), para node.rotation y los samplers de rotacion
void EulerAQuat(const Vector3& e, int order, float& x, float& y, float& z, float& w) {
    Matrix4 R = SkelMatRotEuler(e, order);
    Quaternion q = Quaternion::FromMatrix(R);
    x = q.x; y = q.y; z = q.z; w = q.w;
}

// descompone una matriz LOCAL (column-major) en T / quat(x,y,z,w) / S, SIN pasar por euler (evita gimbal). Es
// la conversion que necesita glTF (node.translation/rotation/scale). La usamos con el LOCAL derivado del bind y
// de la skinMatrix animada -> el FK estandar de glTF reproduce el bind/animacion de CUALQUIER rig (incluido biped).
void DescomponerTRSQuat(const Matrix4& M, Vector3& T, float& qx, float& qy, float& qz, float& qw, Vector3& S) {
    T = Vector3(M.m[12], M.m[13], M.m[14]);
    float sx = sqrtf(M.m[0]*M.m[0] + M.m[1]*M.m[1] + M.m[2]*M.m[2]);
    float sy = sqrtf(M.m[4]*M.m[4] + M.m[5]*M.m[5] + M.m[6]*M.m[6]);
    float sz = sqrtf(M.m[8]*M.m[8] + M.m[9]*M.m[9] + M.m[10]*M.m[10]);
    S = Vector3(sx ? sx : 1.0f, sy ? sy : 1.0f, sz ? sz : 1.0f);
    Matrix4 R; R.Identity();
    if (sx > 1e-8f) { R.m[0] = M.m[0]/sx; R.m[1] = M.m[1]/sx; R.m[2] = M.m[2]/sx; }
    if (sy > 1e-8f) { R.m[4] = M.m[4]/sy; R.m[5] = M.m[5]/sy; R.m[6] = M.m[6]/sy; }
    if (sz > 1e-8f) { R.m[8] = M.m[8]/sz; R.m[9] = M.m[9]/sz; R.m[10] = M.m[10]/sz; }
    Quaternion q = Quaternion::FromMatrix(R);
    qx = q.x; qy = q.y; qz = q.z; qw = q.w;
}

// LOCAL de un hueso (relativo al padre) desde matrices de MUNDO por hueso (bind o animado). root: mundo tal cual.
Matrix4 LocalDeMundo(const std::vector<Matrix4>& world, const std::vector<int>& parent, int b) {
    return (parent[b] >= 0) ? (world[parent[b]].Inverse() * world[b]) : world[b];
}

// acumulador de buffer + bufferViews + accessors. Cada accessor se guarda en su propio bufferView 4-alineado.
struct Bufb {
    std::vector<unsigned char> bin;
    std::vector<std::string> BVs, ACCs;
    void align4() { while (bin.size() % 4) bin.push_back(0); }
    // accessor de floats (POSITION/NORMAL/UV/MAT4/animacion). target=34962 ARRAY, 0 = sin target (MAT4/anim).
    int addFloats(const std::vector<float>& data, int ncomp, const char* type, int target, bool minmax) {
        align4(); size_t off = bin.size();
        for (size_t i = 0; i < data.size(); i++) putF(bin, data[i]);
        int count = ncomp ? (int)(data.size() / ncomp) : 0;
        std::string bv = "{\"buffer\":0,\"byteOffset\":" + Itos((long)off) + ",\"byteLength\":" + Itos((long)(bin.size() - off));
        if (target) bv += ",\"target\":" + Itos(target);
        bv += "}"; BVs.push_back(bv);
        std::string a = "{\"bufferView\":" + Itos((long)BVs.size() - 1) + ",\"componentType\":5126,\"count\":" + Itos(count) + ",\"type\":\"" + type + "\"";
        if (minmax && count > 0) {
            std::string mn, mx; // solo las listas de valores; los "min"/"max":[ los pone el concat de abajo
            for (int c = 0; c < ncomp; c++) { float lo = data[c], hi = data[c];
                for (int i = 1; i < count; i++) { float v = data[(size_t)i * ncomp + c]; if (v < lo) lo = v; if (v > hi) hi = v; }
                if (c) { mn += ","; mx += ","; }
                mn += Ftos(lo); mx += Ftos(hi); }
            a += ",\"min\":[" + mn + "],\"max\":[" + mx + "]";
        }
        a += "}"; ACCs.push_back(a); return (int)ACCs.size() - 1;
    }
    // JOINTS_0: VEC4 de ushort (5123). indices por vertice al array de huesos del skin.
    int addU16Vec4(const std::vector<uint16_t>& data) {
        align4(); size_t off = bin.size();
        for (size_t i = 0; i < data.size(); i++) putU16(bin, data[i]);
        std::string bv = "{\"buffer\":0,\"byteOffset\":" + Itos((long)off) + ",\"byteLength\":" + Itos((long)(bin.size() - off)) + ",\"target\":34962}"; BVs.push_back(bv);
        std::string a = "{\"bufferView\":" + Itos((long)BVs.size() - 1) + ",\"componentType\":5123,\"count\":" + Itos((long)(data.size() / 4)) + ",\"type\":\"VEC4\"}"; ACCs.push_back(a);
        return (int)ACCs.size() - 1;
    }
    // indices: SCALAR de uint (5125), target ELEMENT_ARRAY
    int addIndices(const std::vector<uint32_t>& data) {
        align4(); size_t off = bin.size();
        for (size_t i = 0; i < data.size(); i++) putU32(bin, data[i]);
        std::string bv = "{\"buffer\":0,\"byteOffset\":" + Itos((long)off) + ",\"byteLength\":" + Itos((long)(bin.size() - off)) + ",\"target\":34963}"; BVs.push_back(bv);
        std::string a = "{\"bufferView\":" + Itos((long)BVs.size() - 1) + ",\"componentType\":5125,\"count\":" + Itos((long)data.size()) + ",\"type\":\"SCALAR\"}"; ACCs.push_back(a);
        return (int)ACCs.size() - 1;
    }
};

std::string JoinArr(const std::vector<std::string>& v) {
    std::string o; for (size_t i = 0; i < v.size(); i++) { if (i) o += ","; o += v[i]; } return o;
}

} // namespace

// ============================================================================
//  ExportGLTF
// ============================================================================
bool ExportGLTF(const std::string& filepath, bool selectedOnly, bool binary) {
    std::vector<Mesh*> meshes; RecolectarMeshes(SceneCollection, selectedOnly, meshes);
    if (meshes.empty()) { w3dLogfE("ExportGLTF: no hay meshes"); Notificar("glTF: nothing to export", true); return false; }

    // FORZAR la carga de las texturas pendientes: el import las encola (EncolarTextura) y se cargan diferidas 1/frame.
    // Si exportas apenas importaste (antes de que corran esos frames) mat->texture sigue NULL -> se exportaba sin
    // textura. Aca se vacia la cola de una para que el material tenga su textura al momento de exportar.
    { extern void CargarTexturasPendientes(); for (int i = 0; i < 100000; i++) CargarTexturasPendientes(); }

    // ARMATURE: el rig de la primera malla skinneada (todas las skinneadas comparten esqueleto en nuestros imports)
    Armature* arm = NULL;
    for (size_t i = 0; i < meshes.size() && !arm; i++) if (meshes[i]->skinArmature) arm = meshes[i]->skinArmature;
    int nBones = arm ? (int)arm->bones.size() : 0;

    ProgresoIniciar("Exporting glTF...");

    Bufb buf;
    std::vector<std::string> nodesJson, meshesJson, animsJson, matsJson, texsJson, imgsJson;

    // ---- materiales + texturas (unicos, en el orden que aparecen) ----
    std::vector<Material*> mats; std::map<Material*, int> matIdx;
    std::vector<Texture*> texs;  std::map<Texture*, int> texIdx;
    for (size_t mi = 0; mi < meshes.size(); mi++) {
        Mesh* m = meshes[mi];
        for (size_t g = 0; g < m->materialsGroup.size(); g++) {
            Material* mat = m->materialsGroup[g].material; if (!mat || matIdx.count(mat)) continue;
            matIdx[mat] = (int)mats.size(); mats.push_back(mat);
            if (mat->textureOn && mat->texture && !texIdx.count(mat->texture)) { texIdx[mat->texture] = (int)texs.size(); texs.push_back(mat->texture); }
        }
    }
    // texturas: se COPIAN al lado del archivo exportado y se referencian por BASENAME. Asi el reimport (y Blender)
    // las encuentra al lado, self-contained + portable. Antes se emitia la ruta ABSOLUTA -> el importer le antepone su
    // carpeta -> ruta doble rota (la textura "desaparecia"). El copiado no re-escribe si ya existe / si exportas al mismo dir.
    std::string outDir = DirOf(filepath);
    for (size_t i = 0; i < texs.size(); i++) {
        std::string base = BaseName(texs[i]->path);
        if (!base.empty() && !texs[i]->path.empty()) {
            std::string dest = outDir + base;
            if (dest != texs[i]->path && !w3dFileSystem::FileExists(dest)) {
                std::vector<unsigned char> bytes;
                if (w3dFileSystem::ReadFileBytes(texs[i]->path, bytes) && !bytes.empty()) {
                    std::ofstream tf(dest.c_str(), std::ios::binary);
                    if (tf.is_open()) { tf.write((const char*)&bytes[0], (std::streamsize)bytes.size()); tf.close(); }
                    else w3dLogfW("ExportGLTF: no pude copiar la textura a %s", dest.c_str());
                }
            }
        }
        imgsJson.push_back("{\"uri\":\"" + Jesc(base.empty() ? PathUri(texs[i]->path) : base) + "\"}");
        texsJson.push_back("{\"sampler\":0,\"source\":" + Itos((long)i) + "}");
    }
    for (size_t i = 0; i < mats.size(); i++) {
        Material* mat = mats[i];
        std::string j = "{\"name\":\"" + Jesc(mat->name) + "\",\"pbrMetallicRoughness\":{\"baseColorFactor\":["
            + Ftos(mat->diffuse[0]) + "," + Ftos(mat->diffuse[1]) + "," + Ftos(mat->diffuse[2]) + "," + Ftos(mat->diffuse[3]) + "]";
        if (mat->textureOn && mat->texture && texIdx.count(mat->texture))
            j += ",\"baseColorTexture\":{\"index\":" + Itos((long)texIdx[mat->texture]) + "}";
        j += ",\"metallicFactor\":0,\"roughnessFactor\":1}}";
        matsJson.push_back(j);
    }

    // ---- huesos -> nodos (node i = bone i) + inverseBindMatrices ----
    std::map<std::string, int> jointDeNombre;
    int ibmAcc = -1, skinRootNode = -1;
    if (arm && nBones > 0) {
        for (int j = 0; j < nBones; j++) jointDeNombre[arm->bones[j].name] = j;
        // inverseBindMatrix = la MISMA matriz que el motor multiplica en skinMatrix = world * X. Con g_skinFormula==1
        // (default) X = skinA (para el biped incluye clusterTransform + la correccion D; en rig limpio skinA==skinInvBind).
        // Exportar skinInvBind cuando el motor usa skinA desalineaba el bind del biped (rest ~0.967). Con skinA: exacto.
        std::vector<float> ibm((size_t)nBones * 16);
        for (int j = 0; j < nBones; j++) { const Matrix4& X = (g_skinFormula == 1) ? arm->bones[j].skinA : arm->bones[j].skinInvBind;
            for (int k = 0; k < 16; k++) ibm[(size_t)j * 16 + k] = X.m[k]; }
        ibmAcc = buf.addFloats(ibm, 16, "MAT4", 0, false);
        // REST: el LOCAL de cada nodo se saca del BIND (parentBind^-1 * bind), NO del Lcl-rest. En un biped el FK del
        // Lcl-rest esta DEGENERADO (no reproduce el bind) -> exportar restT/R/S rompia la pose. El bind (TransformLink)
        // es siempre valido; asi el FK estandar de glTF reproduce el bind exacto. En rigs limpios bind == FK-world -> igual.
        std::vector<int> parent(nBones); for (int j = 0; j < nBones; j++) parent[j] = arm->bones[j].parent;
        // "bind efectivo" = inversa de la matriz de skin que el motor usa (X = skinA con formula 1). node_world*X = skinMatrix.
        // En rig limpio X == skinInvBind; en el biped X = skinA (bind real de la malla) -> reproduce la pose EXACTA.
        std::vector<Matrix4> bindW(nBones); for (int j = 0; j < nBones; j++) bindW[j] = ((g_skinFormula == 1) ? arm->bones[j].skinA : arm->bones[j].skinInvBind).Inverse();
        // skinGltf: el Lcl-rest (restT/R/S) del glTF es VALIDO y hay que preservarlo TAL CUAL. Ojo: en muchos rigs el
        // bind (inverseBindMatrices) NO coincide con el node-rest -> derivar el rest del bind cambiaba la ORIENTACION
        // de los huesos (brazos/clavicula rotados mal). Solo el biped FBX (Lcl degenerado) usa el bind-derived.
        for (int j = 0; j < nBones; j++) {
            const W3dBone& b = arm->bones[j];
            Vector3 T, S; float qx, qy, qz, qw;
            if (!arm->skinGltf && b.hasSkin) { Matrix4 local = LocalDeMundo(bindW, parent, j); DescomponerTRSQuat(local, T, qx, qy, qz, qw, S); } // FBX biped
            else { T = b.restT; S = b.restS; EulerAQuat(b.restR, b.rotOrder, qx, qy, qz, qw); } // glTF (o manual): Lcl-rest directo
            std::string n = "{\"name\":\"" + Jesc(b.name) + "\",\"translation\":[" + Ftos(T.x) + "," + Ftos(T.y) + "," + Ftos(T.z)
                + "],\"rotation\":[" + Ftos(qx) + "," + Ftos(qy) + "," + Ftos(qz) + "," + Ftos(qw)
                + "],\"scale\":[" + Ftos(S.x) + "," + Ftos(S.y) + "," + Ftos(S.z) + "]";
            std::string kids; for (int c = 0; c < nBones; c++) if (arm->bones[c].parent == j) { if (!kids.empty()) kids += ","; kids += Itos(c); }
            if (!kids.empty()) n += ",\"children\":[" + kids + "]";
            n += "}"; nodesJson.push_back(n);
            if (b.parent < 0 && skinRootNode < 0) skinRootNode = j; // primer hueso raiz = skeleton del skin
        }
    }

    // ---- mallas -> nodos + meshes (una primitiva por mesh-part/material) ----
    std::vector<int> sceneMeshNodes;
    for (size_t mi = 0; mi < meshes.size(); mi++) {
        ProgresoActualizar((float)mi / (float)meshes.size());
        Mesh* m = meshes[mi];
        if (!m->vertex || m->vertexSize <= 0) continue;
        int nV = m->vertexSize;
        bool skinned = (m->skinArmature == arm) && arm && nBones > 0;

        // POSITION (bind pose). Skinned: espacio escena crudo (matchea skinInvBind). Estatica: a mundo.
        std::vector<float> POS((size_t)nV * 3);
        for (int i = 0; i < nV; i++) {
            // POSITION en LOCAL (estatica y skinned): la estatica lleva su transform en el NODO (abajo) -> se
            // respeta el origen/posicion de cada objeto. Antes la estatica se horneaba a mundo con nodo identidad.
            Vector3 p(m->vertex[i*3], m->vertex[i*3+1], m->vertex[i*3+2]);
            POS[(size_t)i*3] = p.x; POS[(size_t)i*3+1] = p.y; POS[(size_t)i*3+2] = p.z;
        }
        int posAcc = buf.addFloats(POS, 3, "VEC3", 34962, true);

        int nrmAcc = -1;
        if (m->normals) { std::vector<float> NRM((size_t)nV * 3);
            for (int i = 0; i < nV; i++) { Vector3 n(m->normals[i*3]/127.0f, m->normals[i*3+1]/127.0f, m->normals[i*3+2]/127.0f);
                // normales en LOCAL (el nodo lleva la rotacion del objeto); antes la estatica las rotaba a mundo.
                float L = sqrtf(n.x*n.x+n.y*n.y+n.z*n.z); if (L > 1e-6f) n = n * (1.0f/L);
                NRM[(size_t)i*3] = n.x; NRM[(size_t)i*3+1] = n.y; NRM[(size_t)i*3+2] = n.z; }
            nrmAcc = buf.addFloats(NRM, 3, "VEC3", 34962, false); }

        int uvAcc = -1;
        if (m->uv) { std::vector<float> UV((size_t)nV * 2);
            for (int i = 0; i < nV; i++) { UV[(size_t)i*2] = m->uv[i*2]; UV[(size_t)i*2+1] = m->uv[i*2+1]; }
            uvAcc = buf.addFloats(UV, 2, "VEC2", 34962, false); }

        // JOINTS_0 / WEIGHTS_0 desde los vertex groups (por control-point): top-4 pesos por vertice, normalizados.
        int jntAcc = -1, wgtAcc = -1;
        if (skinned && !m->vertexGroups.empty() && !m->vertCtrlPoint.empty()) {
            std::map<int, std::vector<std::pair<int, float> > > wDe; // control-point -> [(joint, peso)]
            for (size_t g = 0; g < m->vertexGroups.size(); g++) {
                VertexGroup* vg = m->vertexGroups[g]; if (!vg) continue;
                std::map<std::string, int>::iterator it = jointDeNombre.find(vg->nombre); if (it == jointDeNombre.end()) continue;
                int j = it->second;
                for (size_t v = 0; v < vg->verts.size() && v < vg->pesos.size(); v++) wDe[vg->verts[v]].push_back(std::make_pair(j, vg->pesos[v]));
            }
            { size_t maxInf = 0, over4 = 0; for (std::map<int, std::vector<std::pair<int, float> > >::iterator it = wDe.begin(); it != wDe.end(); ++it) { if (it->second.size() > maxInf) maxInf = it->second.size(); if (it->second.size() > 4) over4++; }
              if (over4) w3dLogfW("ExportGLTF: '%s' %d control-points con >4 influencias (max %d) -> glTF JOINTS_0 topea en 4, deform aproximada", m->name.c_str(), (int)over4, (int)maxInf); }
            std::vector<uint16_t> JNT((size_t)nV * 4, 0);
            std::vector<float> WGT((size_t)nV * 4, 0.0f);
            for (int i = 0; i < nV; i++) {
                int cp = (i < (int)m->vertCtrlPoint.size()) ? m->vertCtrlPoint[i] : -1;
                std::map<int, std::vector<std::pair<int, float> > >::iterator it = wDe.find(cp);
                if (it == wDe.end()) { WGT[(size_t)i*4] = 1.0f; continue; } // sin peso: 100% al hueso 0 (evita vertices sueltos)
                std::vector<std::pair<int, float> >& lst = it->second;
                int best[4] = {0,0,0,0}; float bw[4] = {0,0,0,0};
                for (size_t e = 0; e < lst.size(); e++) { float w = lst[e].second;
                    for (int s = 0; s < 4; s++) if (w > bw[s]) { for (int t = 3; t > s; t--) { bw[t] = bw[t-1]; best[t] = best[t-1]; } bw[s] = w; best[s] = lst[e].first; break; } }
                float sum = bw[0] + bw[1] + bw[2] + bw[3]; if (sum <= 1e-8f) { WGT[(size_t)i*4] = 1.0f; continue; }
                for (int s = 0; s < 4; s++) { JNT[(size_t)i*4+s] = (uint16_t)best[s]; WGT[(size_t)i*4+s] = bw[s] / sum; }
            }
            jntAcc = buf.addU16Vec4(JNT);
            wgtAcc = buf.addFloats(WGT, 4, "VEC4", 34962, false);
        }

        // primitivas: una por mesh-part. Comparten los accessors de vertice; cambia indices + material.
        std::string attrs = "\"POSITION\":" + Itos(posAcc);
        if (nrmAcc >= 0) attrs += ",\"NORMAL\":" + Itos(nrmAcc);
        if (uvAcc >= 0)  attrs += ",\"TEXCOORD_0\":" + Itos(uvAcc);
        if (jntAcc >= 0) attrs += ",\"JOINTS_0\":" + Itos(jntAcc) + ",\"WEIGHTS_0\":" + Itos(wgtAcc);

        std::vector<std::string> prims;
        int nGroups = (int)m->materialsGroup.size(); if (nGroups == 0) nGroups = 1;
        for (int g = 0; g < nGroups; g++) {
            std::vector<uint32_t> idx;
            for (size_t f = 0; f < m->faces3d.size(); f++) { const MeshFace& F = m->faces3d[f];
                int fm = (F.mat < 0 || F.mat >= (int)m->materialsGroup.size()) ? 0 : F.mat;
                if (fm != g) continue;
                if (F.idx.size() < 3) continue;
                for (size_t t = 1; t + 1 < F.idx.size(); t++) { idx.push_back((uint32_t)F.idx[0]); idx.push_back((uint32_t)F.idx[t]); idx.push_back((uint32_t)F.idx[t+1]); }
            }
            if (idx.empty()) continue;
            int iAcc = buf.addIndices(idx);
            std::string pr = "{\"attributes\":{" + attrs + "},\"indices\":" + Itos(iAcc);
            Material* mat = (g < (int)m->materialsGroup.size()) ? m->materialsGroup[g].material : NULL;
            if (mat && matIdx.count(mat)) pr += ",\"material\":" + Itos((long)matIdx[mat]);
            pr += "}"; prims.push_back(pr);
        }
        if (prims.empty()) continue;

        int meshId = (int)meshesJson.size();
        meshesJson.push_back("{\"name\":\"" + Jesc(m->name.empty() ? std::string("Mesh") : m->name) + "\",\"primitives\":[" + JoinArr(prims) + "]}");

        // nodo de la malla. Skinned: identidad + skin 0 (el armature deforma). Estatica: lleva el transform del
        // objeto (translation/rotation/scale del MUNDO) para respetar su origen/posicion; la POSITION va en LOCAL.
        std::string node = "{\"name\":\"" + Jesc(m->name.empty() ? std::string("Mesh") : m->name) + "\",\"mesh\":" + Itos(meshId);
        if (skinned && jntAcc >= 0) { node += ",\"skin\":0"; }
        else {
            Matrix4 W; m->GetWorldMatrix(W);
            Vector3 T, S; float qx, qy, qz, qw; DescomponerTRSQuat(W, T, qx, qy, qz, qw, S);
            node += ",\"translation\":[" + Ftos(T.x) + "," + Ftos(T.y) + "," + Ftos(T.z) + "]";
            node += ",\"rotation\":["    + Ftos(qx)  + "," + Ftos(qy)  + "," + Ftos(qz) + "," + Ftos(qw) + "]";
            node += ",\"scale\":["       + Ftos(S.x) + "," + Ftos(S.y) + "," + Ftos(S.z) + "]";
        }
        node += "}";
        sceneMeshNodes.push_back((int)nodesJson.size());
        nodesJson.push_back(node);
    }

    // ---- animaciones (glTF rig): export DIRECTO de las curvas guardadas (euler -> quaternion), sin hornear. El rig
    //      glTF tiene un FK estandar valido, asi que las keyframes euler mapean 1:1. Preserva rest + animacion EXACTOS
    //      (hornear del skinMatrix cambiaba la orientacion de brazos/clavicula cuando el bind != node-rest). Sparse.
    if (arm && nBones > 0 && !arm->animations.empty() && arm->skinGltf) {
        for (size_t ci = 0; ci < arm->animations.size(); ci++) {
            SkeletalAnimation* clip = arm->animations[ci]; if (!clip) continue;
            float fps = (float)(clip->FrameRate > 0 ? clip->FrameRate : 24);
            std::vector<std::string> channels, samplers;
            for (size_t ti = 0; ti < clip->tracks.size(); ti++) {
                BoneTrack& tr = clip->tracks[ti];
                int bone = tr.bone; if (bone < 0 || bone >= nBones) continue;
                int rotOrder = arm->bones[bone].rotOrder;
                // Un sampler de glTF necesita los 3 componentes en los MISMOS tiempos, pero en el modelo cada
                // componente (X/Y/Z) es una CURVA con sus propios keyframes. Por eso: por propiedad se UNEN los
                // frames de sus 3 curvas y se EVALUA cada componente en cada frame de la union (EvalPropVec).
                const int propsExp[3] = { AnimPosition, AnimRotation, AnimScale };
                for (int pe = 0; pe < 3; pe++) {
                    int prop = propsExp[pe];
                    const char* path = (prop == AnimPosition) ? "translation" : (prop == AnimRotation) ? "rotation" : "scale";
                    std::vector<int> frames; // union de los frames de las curvas X/Y/Z de esta propiedad
                    for (size_t pi = 0; pi < tr.Propertys.size(); pi++){
                        if (tr.Propertys[pi].Property != prop) continue;
                        for (size_t k = 0; k < tr.Propertys[pi].keyframes.size(); k++) frames.push_back(tr.Propertys[pi].keyframes[k].frame);
                    }
                    if (frames.empty()) continue;
                    std::sort(frames.begin(), frames.end());
                    frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
                    // valor de reposo del hueso para los componentes que no tengan curva propia
                    Vector3 def = (prop == AnimPosition) ? arm->bones[bone].restT : (prop == AnimRotation) ? arm->bones[bone].restR : arm->bones[bone].restS;
                    std::vector<float> times(frames.size()), vals;
                    for (size_t k = 0; k < frames.size(); k++) {
                        times[k] = (float)(frames[k] - 1) / fps;
                        Vector3 v = EvalPropVec(tr.Propertys, prop, frames[k], def);
                        if (prop == AnimRotation) { float qx, qy, qz, qw; EulerAQuat(v, rotOrder, qx, qy, qz, qw); vals.push_back(qx); vals.push_back(qy); vals.push_back(qz); vals.push_back(qw); }
                        else { vals.push_back(v.x); vals.push_back(v.y); vals.push_back(v.z); }
                    }
                    if (prop == AnimRotation) for (size_t k = 1; k < frames.size(); k++) { float* c = &vals[k*4]; float* p = &vals[(k-1)*4];
                        if (c[0]*p[0]+c[1]*p[1]+c[2]*p[2]+c[3]*p[3] < 0) { c[0]=-c[0]; c[1]=-c[1]; c[2]=-c[2]; c[3]=-c[3]; } } // hemisferio para LINEAR
                    int inAcc = buf.addFloats(times, 1, "SCALAR", 0, true);
                    int outAcc = buf.addFloats(vals, prop == AnimRotation ? 4 : 3, prop == AnimRotation ? "VEC4" : "VEC3", 0, false);
                    int sIdx = (int)samplers.size();
                    samplers.push_back("{\"input\":" + Itos(inAcc) + ",\"output\":" + Itos(outAcc) + ",\"interpolation\":\"LINEAR\"}");
                    channels.push_back("{\"sampler\":" + Itos(sIdx) + ",\"target\":{\"node\":" + Itos(bone) + ",\"path\":\"" + path + "\"}}");
                }
            }
            if (channels.empty()) continue;
            // extras.frameRate: guardar el fps del clip. glTF NO tiene metadato de fps y el importer lo ADIVINA del
            // espaciado de tiempos -> con keyframes sparse (ej frame 1 y 20) adivina fps=1 y colapsa el rango a 1..2.
            // Con extras el round-trip es exacto (glTF estandar; otros viewers lo ignoran).
            animsJson.push_back("{\"name\":\"" + Jesc(clip->name) + "\",\"extras\":{\"frameRate\":" + Itos((long)fps) + "},\"channels\":[" + JoinArr(channels) + "],\"samplers\":[" + JoinArr(samplers) + "]}");
        }
    }
    // ---- animaciones (FBX biped): se HORNEA la pose por-frame (el Lcl-FK del biped es degenerado). En cada frame se
    //      evalua el esqueleto y el LOCAL de cada nodo se saca del skinMatrix animado (skinMatrix*bind = world FK).
    //      Es denso (un sample por frame) pero correcto; se saltean los huesos que quedan en rest todo el clip.
    else if (arm && nBones > 0 && !arm->animations.empty() && arm->bones[0].hasRest) {
        std::vector<int> parent(nBones); for (int j = 0; j < nBones; j++) parent[j] = arm->bones[j].parent;
        // "bind efectivo" = inv(X) con X = la matriz de skin del motor (skinA en formula 1): node_world = skinMatrix * bindW
        std::vector<Matrix4> bindW(nBones); for (int j = 0; j < nBones; j++) bindW[j] = ((g_skinFormula == 1) ? arm->bones[j].skinA : arm->bones[j].skinInvBind).Inverse();
        // rest local por hueso (bind-derivado) -> para saltear huesos estaticos (que quedan == rest en todo el clip)
        std::vector<float> rT(nBones*3), rR(nBones*4), rS(nBones*3);
        for (int j = 0; j < nBones; j++) { Vector3 T,S; float qx,qy,qz,qw; DescomponerTRSQuat(LocalDeMundo(bindW,parent,j), T,qx,qy,qz,qw,S);
            rT[j*3]=T.x;rT[j*3+1]=T.y;rT[j*3+2]=T.z; rR[j*4]=qx;rR[j*4+1]=qy;rR[j*4+2]=qz;rR[j*4+3]=qw; rS[j*3]=S.x;rS[j*3+1]=S.y;rS[j*3+2]=S.z; }
        // guardar el estado de animacion del editor (lo tocamos para forzar la evaluacion del clip) y restaurarlo al final
        int savActiva = arm->animActiva, savKind = ActiveAnimKind, savFrame = CurrentFrame; Armature* savArm = ActiveAnimArm; bool savPrev = g_skelAnimPreview;
        ActiveAnimKind = 1; ActiveAnimArm = arm; g_skelAnimPreview = true; // EvaluarPoseEsqueleto solo anima el clip ACTIVO
        for (size_t ci = 0; ci < arm->animations.size(); ci++) {
            SkeletalAnimation* clip = arm->animations[ci]; if (!clip) continue;
            float fps = (float)(clip->FrameRate > 0 ? clip->FrameRate : 24);
            int f0 = clip->startFrame, f1 = clip->endFrame; if (f1 < f0) f1 = f0;
            int nF = f1 - f0 + 1; if (nF < 1) nF = 1; if (nF > 4096) nF = 4096; // tope de seguridad
            arm->animActiva = (int)ci;
            std::vector<float> times(nF);
            std::vector<std::vector<float> > Tv(nBones), Rv(nBones), Sv(nBones);
            for (int fi = 0; fi < nF; fi++) {
                int f = f0 + fi; times[fi] = (float)fi / fps;
                arm->poseDirty = false; arm->lastPoseFrame = -999999; // forzar re-FK de ESTE frame (evita el cache)
                EvaluarPoseEsqueleto(arm, f);
                std::vector<Matrix4> world(nBones);
                for (int j = 0; j < nBones; j++) world[j] = arm->bones[j].skinMatrix * bindW[j]; // node_world: world*skinInvBind=skinMatrix
                for (int j = 0; j < nBones; j++) {
                    Vector3 T, S; float qx, qy, qz, qw; DescomponerTRSQuat(LocalDeMundo(world, parent, j), T, qx, qy, qz, qw, S);
                    Tv[j].push_back(T.x); Tv[j].push_back(T.y); Tv[j].push_back(T.z);
                    Rv[j].push_back(qx); Rv[j].push_back(qy); Rv[j].push_back(qz); Rv[j].push_back(qw);
                    Sv[j].push_back(S.x); Sv[j].push_back(S.y); Sv[j].push_back(S.z);
                }
            }
            // continuidad de quats: LINEAR interpola por el lado corto -> forzar el MISMO hemisferio que el frame previo
            for (int j = 0; j < nBones; j++) for (int fi = 1; fi < nF; fi++) {
                float* c = &Rv[j][(size_t)fi*4]; float* p = &Rv[j][(size_t)(fi-1)*4];
                if (c[0]*p[0]+c[1]*p[1]+c[2]*p[2]+c[3]*p[3] < 0) { c[0]=-c[0]; c[1]=-c[1]; c[2]=-c[2]; c[3]=-c[3]; }
            }
            int inAcc = buf.addFloats(times, 1, "SCALAR", 0, true); // input (tiempos) compartido por todos los samplers del clip
            std::vector<std::string> channels, samplers;
            for (int j = 0; j < nBones; j++) {
                bool mueve = false; // hueso que queda en rest TODO el clip -> el node rest ya lo cubre (se saltea)
                for (int fi = 0; fi < nF && !mueve; fi++) {
                    for (int c = 0; c < 3; c++) if (fabsf(Tv[j][(size_t)fi*3+c]-rT[j*3+c]) > 1e-5f || fabsf(Sv[j][(size_t)fi*3+c]-rS[j*3+c]) > 1e-5f) mueve = true;
                    for (int c = 0; c < 4; c++) if (fabsf(Rv[j][(size_t)fi*4+c]-rR[j*4+c]) > 1e-5f) mueve = true;
                }
                if (!mueve) continue;
                int tAcc = buf.addFloats(Tv[j], 3, "VEC3", 0, false);
                int rAcc = buf.addFloats(Rv[j], 4, "VEC4", 0, false);
                int sAcc = buf.addFloats(Sv[j], 3, "VEC3", 0, false);
                int base = (int)samplers.size();
                samplers.push_back("{\"input\":"+Itos(inAcc)+",\"output\":"+Itos(tAcc)+",\"interpolation\":\"LINEAR\"}");
                samplers.push_back("{\"input\":"+Itos(inAcc)+",\"output\":"+Itos(rAcc)+",\"interpolation\":\"LINEAR\"}");
                samplers.push_back("{\"input\":"+Itos(inAcc)+",\"output\":"+Itos(sAcc)+",\"interpolation\":\"LINEAR\"}");
                channels.push_back("{\"sampler\":"+Itos(base+0)+",\"target\":{\"node\":"+Itos(j)+",\"path\":\"translation\"}}");
                channels.push_back("{\"sampler\":"+Itos(base+1)+",\"target\":{\"node\":"+Itos(j)+",\"path\":\"rotation\"}}");
                channels.push_back("{\"sampler\":"+Itos(base+2)+",\"target\":{\"node\":"+Itos(j)+",\"path\":\"scale\"}}");
            }
            if (channels.empty()) continue;
            // extras.frameRate: mismo motivo que el path clean (preservar el fps para el round-trip exacto)
            animsJson.push_back("{\"name\":\""+Jesc(clip->name)+"\",\"extras\":{\"frameRate\":"+Itos((long)fps)+"},\"channels\":["+JoinArr(channels)+"],\"samplers\":["+JoinArr(samplers)+"]}");
        }
        // restaurar el estado de animacion del editor
        arm->animActiva = savActiva; ActiveAnimKind = savKind; ActiveAnimArm = savArm; g_skelAnimPreview = savPrev;
        arm->poseDirty = false; arm->lastPoseFrame = -999999; EvaluarPoseEsqueleto(arm, savFrame);
    }

    // ---- nodos raiz de la escena: el hueso raiz del skin + los nodos de malla ----
    std::vector<std::string> sceneNodes;
    if (skinRootNode >= 0) sceneNodes.push_back(Itos(skinRootNode));
    for (size_t i = 0; i < sceneMeshNodes.size(); i++) sceneNodes.push_back(Itos(sceneMeshNodes[i]));
    if (sceneNodes.empty()) for (size_t i = 0; i < nodesJson.size(); i++) sceneNodes.push_back(Itos((long)i));

    // ---- ensamblar el JSON ----
    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Whisk3D\"},\"scene\":0,\"scenes\":[{\"nodes\":[" + JoinArr(sceneNodes) + "]}]";
    json += ",\"nodes\":[" + JoinArr(nodesJson) + "]";
    if (!meshesJson.empty()) json += ",\"meshes\":[" + JoinArr(meshesJson) + "]";
    if (arm && nBones > 0 && ibmAcc >= 0) {
        std::string joints; for (int j = 0; j < nBones; j++) { if (j) joints += ","; joints += Itos(j); }
        json += ",\"skins\":[{\"joints\":[" + joints + "],\"inverseBindMatrices\":" + Itos(ibmAcc);
        if (skinRootNode >= 0) json += ",\"skeleton\":" + Itos(skinRootNode);
        json += "}]";
    }
    if (!animsJson.empty()) json += ",\"animations\":[" + JoinArr(animsJson) + "]";
    if (!matsJson.empty()) json += ",\"materials\":[" + JoinArr(matsJson) + "]";
    if (!texsJson.empty()) { json += ",\"textures\":[" + JoinArr(texsJson) + "]";
        json += ",\"images\":[" + JoinArr(imgsJson) + "]";
        json += ",\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}]"; }
    json += ",\"accessors\":[" + JoinArr(buf.ACCs) + "]";
    json += ",\"bufferViews\":[" + JoinArr(buf.BVs) + "]";

    // ---- buffer + escritura (.gltf base64 embebido / .glb chunkeado) ----
    bool ok = false;
    if (binary) {
        json += ",\"buffers\":[{\"byteLength\":" + Itos((long)buf.bin.size()) + "}]}";
        std::string jsonChunk = json; while (jsonChunk.size() % 4) jsonChunk += ' ';        // pad JSON con espacios
        std::vector<unsigned char> binChunk = buf.bin; while (binChunk.size() % 4) binChunk.push_back(0);
        std::vector<unsigned char> out;
        uint32_t total = 12 + 8 + (uint32_t)jsonChunk.size() + 8 + (uint32_t)binChunk.size();
        out.push_back('g'); out.push_back('l'); out.push_back('T'); out.push_back('F');
        putU32(out, 2); putU32(out, total);
        putU32(out, (uint32_t)jsonChunk.size()); putU32(out, 0x4E4F534A); // "JSON"
        for (size_t i = 0; i < jsonChunk.size(); i++) out.push_back((unsigned char)jsonChunk[i]);
        putU32(out, (uint32_t)binChunk.size()); putU32(out, 0x004E4942); // "BIN\0"
        for (size_t i = 0; i < binChunk.size(); i++) out.push_back(binChunk[i]);
        std::ofstream f(filepath.c_str(), std::ios::binary);
        if (f.is_open()) { f.write((const char*)&out[0], (std::streamsize)out.size()); f.close(); ok = true; }
    } else {
        std::string b64; b64encode(buf.bin, b64);
        json += ",\"buffers\":[{\"byteLength\":" + Itos((long)buf.bin.size()) + ",\"uri\":\"data:application/octet-stream;base64," + b64 + "\"}]}";
        std::ofstream f(filepath.c_str(), std::ios::binary);
        if (f.is_open()) { f.write(json.data(), (std::streamsize)json.size()); f.close(); ok = true; }
    }

    if (!ok) { w3dLogfE("ExportGLTF: no pude escribir %s", filepath.c_str()); Notificar("glTF: could not write the file", true); return false; }
    w3dLogf("ExportGLTF: %s (%d malla(s), %d hueso(s), %d clip(s), buffer=%d bytes)", filepath.c_str(), (int)meshesJson.size(), nBones, (int)animsJson.size(), (int)buf.bin.size());
#ifdef __EMSCRIPTEN__
    WebDescargarArchivo(filepath.c_str(), filepath.c_str()); // web: bajar el archivo (FS virtual de emscripten)
#endif
    Notificar(binary ? "GLB saved successfully!" : "glTF saved successfully!", false);
    return true;
}
