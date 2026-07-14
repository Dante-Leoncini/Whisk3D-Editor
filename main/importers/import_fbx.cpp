#include "import_fbx.h"
#include "import_obj.h"          // Wavefront / Face / FaceCorner / ExtractBaseName / EncolarTextura
#include "objects/Mesh.h"
#include "objects/Armature.h"    // esqueleto (bones) importado del FBX
#include "animation/SkeletalAnimation.h" // animaciones (clips) importadas del FBX
#include "animation/Animation.h"         // AnimProperty/keyFrame + enum AnimPosition/Rotation/Scale
#include "edit/Modifier.h"                // modificador Armature (auto-add al importar)
#include <algorithm>                      // sort/unique (resampleo de curvas)
#include "objects/Materials.h"   // Material / Materials / MaterialDefecto
#include "objects/Objects.h"     // CollectionActive
#include "w3dFilesystem.h"       // ReadFileBytes / FileExists / DirOf(inline abajo)
#include "w3dCompress.h"         // w3dEngine::Inflate (arrays zlib del FBX)
#include "ViewPorts/PopUp/ProgressPopup.h" // barra de progreso (clave en el N95: importar tarda)
#include "ViewPorts/LayoutInput.h"        // Notificar (toast de exito, igual que el OBJ)
#include "w3dlog.h"
#include <vector>
#include <string>
#include <map>
#include <set>     // std::set: clasificar ids (material/textura/geometria/model) al mapear las Connections del FBX
#include <cstring>
#include <stdint.h> // uintN_t: <stdint.h> (C) resuelve en los 4 OS; <cstdint> es C++11 (no en STLport/Symbian)
#include <stdio.h>  // snprintf (global, C99) -> resuelve en Symbian/Open C; <cstdio> no garantiza snprintf en STLport
#include <cmath>

// ============================================================================
//  Importador FBX BINARIO. Formato: header "Kaydara FBX Binary  \0" + version, y
//  un arbol de nodos. Cada nodo: [endOffset numProps propLen][nameLen name][props][hijos]. En version <7500 esos
//  3 campos son de 32 bits; en >=7500 de 64. Las properties escalares (Y/C/I/F/D/L/S/R) o arrays (f/d/l/i/b) con
//  encoding raw o zlib. Solo se lee geometria (Vertices/PolygonVertexIndex), normales, UV y la 1ra textura.
// ============================================================================

namespace {

// ---- lector de bytes (little-endian; el FBX es LE en todas las plataformas) ----
struct Rd {
    const unsigned char* p; const unsigned char* end;
    bool avail(size_t n) const { return p + n <= end; }
    uint8_t  u8()  { if (!avail(1)) { p = end; return 0; } return *p++; }
    uint16_t u16() { uint16_t v = 0; if (avail(2)) { memcpy(&v, p, 2); p += 2; } else p = end; return v; }
    uint32_t u32() { uint32_t v = 0; if (avail(4)) { memcpy(&v, p, 4); p += 4; } else p = end; return v; }
    uint64_t u64() { uint64_t v = 0; if (avail(8)) { memcpy(&v, p, 8); p += 8; } else p = end; return v; }
    float    f32() { float  v = 0; if (avail(4)) { memcpy(&v, p, 4); p += 4; } else p = end; return v; }
    double   f64() { double v = 0; if (avail(8)) { memcpy(&v, p, 8); p += 8; } else p = end; return v; }
};

// ---- una property: escalar (i o d o s) o array (ad = doubles, ai = enteros) ----
struct FProp {
    char type;
    long long i; double d; std::string s;
    std::vector<double>    ad; // arrays f/d -> double
    std::vector<long long> ai; // arrays i/l/b -> long long
    FProp() : type(0), i(0), d(0) {}
};

struct FNode {
    std::string name;
    std::vector<FProp> props;
    std::vector<FNode> kids;
    const FNode* child(const char* n) const {
        for (size_t i = 0; i < kids.size(); i++) if (kids[i].name == n) return &kids[i];
        return 0;
    }
};

static void LeerProp(Rd& r, FProp& pr) {
    pr.type = (char)r.u8();
    switch (pr.type) {
        case 'Y': pr.i = (int16_t)r.u16(); break;
        case 'C': pr.i = r.u8(); break;
        case 'I': pr.i = (int32_t)r.u32(); break;
        case 'F': pr.d = r.f32(); break;
        case 'D': pr.d = r.f64(); break;
        case 'L': pr.i = (long long)r.u64(); break;
        case 'S': case 'R': {
            uint32_t len = r.u32();
            if (r.avail(len)) { pr.s.assign((const char*)r.p, len); r.p += len; } else r.p = r.end;
            break;
        }
        case 'f': case 'd': case 'l': case 'i': case 'b': {
            uint32_t len = r.u32(), enc = r.u32(), clen = r.u32();
            int esz = (pr.type == 'd' || pr.type == 'l') ? 8 : (pr.type == 'f' || pr.type == 'i') ? 4 : 1;
            size_t rawBytes = (size_t)len * esz;
            std::vector<unsigned char> raw;
            const unsigned char* data = 0;
            if (enc == 1) { // zlib
                raw.resize(rawBytes ? rawBytes : 1); // nunca vacio -> &raw[0] valido (C++03: vector no tiene .data())
                if (r.avail(clen) && w3dEngine::Inflate(r.p, (int)clen, &raw[0], (int)rawBytes)) data = &raw[0];
                r.p += clen;
            } else {        // raw
                if (r.avail(rawBytes)) { data = r.p; r.p += rawBytes; } else r.p = r.end;
            }
            if (data) {
                for (uint32_t k = 0; k < len; k++) {
                    switch (pr.type) {
                        case 'd': { double v; memcpy(&v, data + (size_t)k * 8, 8); pr.ad.push_back(v); } break;
                        case 'f': { float  v; memcpy(&v, data + (size_t)k * 4, 4); pr.ad.push_back((double)v); } break;
                        case 'l': { long long v; memcpy(&v, data + (size_t)k * 8, 8); pr.ai.push_back(v); } break;
                        case 'i': { int32_t v; memcpy(&v, data + (size_t)k * 4, 4); pr.ai.push_back((long long)v); } break;
                        case 'b': { pr.ai.push_back((long long)(signed char)data[k]); } break;
                    }
                }
            }
            break;
        }
        default: r.p = r.end; break; // property desconocida: no podemos seguir con seguridad
    }
}

// lee UN nodo (recursivo). base = inicio del archivo (los offsets son ABSOLUTOS). w64 = version>=7500 (offsets 64-bit).
// devuelve false si es el "null record" (fin de la lista de hijos) o si algo se rompio.
static bool LeerNodo(Rd& r, const unsigned char* base, bool w64, FNode& out) {
    uint64_t endOff, numProps, propLen;
    if (w64) { endOff = r.u64(); numProps = r.u64(); propLen = r.u64(); }
    else     { endOff = r.u32(); numProps = r.u32(); propLen = r.u32(); }
    uint8_t nameLen = r.u8();
    if (endOff == 0) return false; // null record
    if (r.avail(nameLen)) { out.name.assign((const char*)r.p, nameLen); r.p += nameLen; } else { r.p = r.end; return false; }
    const unsigned char* propsEnd = r.p + propLen;
    for (uint64_t i = 0; i < numProps && r.p < r.end; i++) { FProp pr; LeerProp(r, pr); out.props.push_back(pr); }
    if (propsEnd <= r.end) r.p = propsEnd; // saltar cualquier property no leida
    // hijos: hasta base+endOff
    while (r.p < base + endOff && base + endOff <= r.end) {
        size_t rem = (size_t)((base + endOff) - r.p);
        if (rem < (w64 ? 25u : 13u)) break; // no entra ni el null record
        FNode kid;
        if (!LeerNodo(r, base, w64, kid)) break;
        out.kids.push_back(kid);
    }
    if (base + endOff <= r.end) r.p = base + endOff; // saltar al final del nodo
    return true;
}

// normal float [-1,1] -> GLbyte [-127,127]
static GLbyte NrmB(double n) {
    long v = (long)(n * 127.0 + (n >= 0 ? 0.5 : -0.5));
    if (v > 127) v = 127; if (v < -127) v = -127;
    return (GLbyte)v;
}
static std::string DirDe(const std::string& ruta) {
    size_t i = ruta.find_last_of("/\\");
    return (i == std::string::npos) ? std::string() : ruta.substr(0, i + 1);
}
static std::string SoloNombre(const std::string& ruta) {
    size_t i = ruta.find_last_of("/\\");
    return (i == std::string::npos) ? ruta : ruta.substr(i + 1);
}
// resuelve la ruta REAL de una textura referenciada por el FBX: prueba carpetas (junto al FBX, ./textures/,
// ../textures/) y extensiones (la original + las que decodifica stb) -> muchos FBX referencian .tga pero el autor
// dejo un .png convertido en otra carpeta (justo el caso del banana). Devuelve la ruta existente, o "".
static std::string ResolverTextura(const std::string& fbxDir, const std::string& ref) {
    std::string t = ref;
    for (size_t j = 0; j < t.size(); j++) if (t[j] == '\\') t[j] = '/'; // FBX usa backslash
    // 1) el relative-path completo tal cual (por si trae subcarpetas validas)
    { std::string c = w3dFileSystem::JoinPath(fbxDir, t); if (w3dFileSystem::FileExists(c)) return c; }
    std::string nom = SoloNombre(t), stem = nom;
    size_t dot = stem.find_last_of('.'); if (dot != std::string::npos) stem = stem.substr(0, dot);
    const char* dirs[] = { "", "textures/", "Textures/", "../textures/", "../Textures/", "source/" };
    const char* exts[] = { "", ".png", ".jpg", ".jpeg", ".tga", ".bmp" }; // ""=nombre tal cual; el resto = swap de ext
    for (size_t di = 0; di < sizeof(dirs) / sizeof(dirs[0]); di++) {
        std::string bdir = w3dFileSystem::JoinPath(fbxDir, dirs[di]);
        for (size_t ei = 0; ei < sizeof(exts) / sizeof(exts[0]); ei++) {
            std::string fname = (exts[ei][0] == '\0') ? nom : (stem + exts[ei]);
            std::string c = w3dFileSystem::JoinPath(bdir, fname);
            if (w3dFileSystem::FileExists(c)) return c;
        }
    }
    return std::string();
}

// lee una propiedad numerica de GlobalSettings/Properties70 (P: "nombre", type, ..., valor). Ej: UnitScaleFactor.
static double LeerGlobalD(const FNode& root, const char* nombre, double def) {
    const FNode* gs = root.child("GlobalSettings"); if (!gs) return def;
    const FNode* p70 = gs->child("Properties70"); if (!p70) return def;
    for (size_t i = 0; i < p70->kids.size(); i++) {
        const FNode& p = p70->kids[i];
        if (p.name == "P" && !p.props.empty() && p.props[0].type == 'S' && p.props[0].s == nombre) {
            for (size_t j = p.props.size(); j-- > 0; ) { // el valor es la ultima property numerica
                char t = p.props[j].type;
                if (t == 'D' || t == 'F') return p.props[j].d;
                if (t == 'I' || t == 'L' || t == 'Y' || t == 'C') return (double)p.props[j].i;
            }
        }
    }
    return def;
}

// lee un Vector3 de un Properties70 (las ultimas 3 numericas de la P 'nombre'): Lcl Translation/Rotation/Scaling, PreRotation...
static Vector3 LeerP70Vec3(const FNode& node, const char* nombre, Vector3 def) {
    const FNode* p70 = node.child("Properties70"); if (!p70) return def;
    for (size_t i = 0; i < p70->kids.size(); i++) {
        const FNode& p = p70->kids[i];
        if (p.name == "P" && !p.props.empty() && p.props[0].type == 'S' && p.props[0].s == nombre) {
            std::vector<double> v;
            for (size_t j = 0; j < p.props.size(); j++) { char t = p.props[j].type;
                if (t=='D'||t=='F') v.push_back(p.props[j].d);
                else if (t=='I'||t=='L'||t=='Y'||t=='C') v.push_back((double)p.props[j].i); }
            if (v.size() >= 3) return Vector3((float)v[v.size()-3], (float)v[v.size()-2], (float)v[v.size()-1]);
        }
    }
    return def;
}
static double LeerP70D(const FNode& node, const char* nombre, double def) {
    const FNode* p70 = node.child("Properties70"); if (!p70) return def;
    for (size_t i = 0; i < p70->kids.size(); i++) { const FNode& p = p70->kids[i];
        if (p.name == "P" && !p.props.empty() && p.props[0].type=='S' && p.props[0].s==nombre)
            for (size_t j = p.props.size(); j-- > 0; ) { char t=p.props[j].type;
                if (t=='D'||t=='F') return p.props[j].d; if (t=='I'||t=='L'||t=='Y'||t=='C') return (double)p.props[j].i; }
    }
    return def;
}

// frame rate REAL del FBX (GlobalSettings.TimeMode). Clave para mapear KeyTime->frame sin gaps ni desincronizar
// las rotaciones (si se asume 30 y el archivo es 24, cae 1 de cada 5 frames y la animacion "gira loca").
static double FbxFrameRate(const FNode& root) {
    int tm = (int)LeerGlobalD(root, "TimeMode", 6);
    double custom = LeerGlobalD(root, "CustomFrameRate", -1.0);
    switch (tm) {
        case 1: return 120; case 2: return 100; case 3: return 60;  case 4: return 50;
        case 5: return 48;  case 6: case 7: return 30; case 8: case 9: return 29.97;
        case 10: return 25; case 11: return 24;  case 12: return 1000; case 13: return 24;
        case 15: return 96; case 16: return 72;  case 17: return 59.94;
        case 14: return custom > 0 ? custom : 24; // eCustom
        default: return custom > 0 ? custom : 24;
    }
}

// arma una malla de Whisk3D desde un nodo Geometry(Mesh). Devuelve la malla o NULL. mat = material (con textura) a
// asignar. parent = objeto padre (la armature si hay esqueleto, si no CollectionActive). El TRANSFORM de correccion
// (unidades + eje) NO se hornea aca: lo aplica el caller (a la armature si hay, o a la malla si no).
static Mesh* MallaDesdeGeometry(const FNode& geo, const std::string& nombre, const std::vector<Material*>& mats, Object* parent,
                                float progBase, float progSpan) {
    const FNode* nVert = geo.child("Vertices");
    const FNode* nPoly = geo.child("PolygonVertexIndex");
    if (!nVert || !nPoly || nVert->props.empty() || nPoly->props.empty()) return 0;
    const std::vector<double>&    V  = nVert->props[0].ad; // x,y,z por control-point
#ifdef W3D_DEBUG_FBXANIM
    printf("  [geo] controlPoints=%d corners(PI)=%d\n", (int)(V.size()/3), (int)nPoly->props[0].ai.size());
#endif
    const std::vector<long long>& PI = nPoly->props[0].ai; // indices de polygon-vertex (fin de poligono = ~x)
    if (V.size() < 9 || PI.size() < 3) return 0;

    // ---- normales (LayerElementNormal) ----
    const std::vector<double>* NRM = 0; const std::vector<long long>* NIDX = 0;
    std::string nMap = "ByPolygonVertex", nRef = "Direct";
    if (const FNode* le = geo.child("LayerElementNormal")) {
        if (const FNode* c = le->child("Normals")) if (!c->props.empty()) NRM = &c->props[0].ad;
        if (const FNode* c = le->child("NormalsIndex")) if (!c->props.empty()) NIDX = &c->props[0].ai;
        if (const FNode* c = le->child("MappingInformationType")) if (!c->props.empty()) nMap = c->props[0].s;
        if (const FNode* c = le->child("ReferenceInformationType")) if (!c->props.empty()) nRef = c->props[0].s;
    }
    // ---- UV (LayerElementUV) ----
    const std::vector<double>* UVA = 0; const std::vector<long long>* UIDX = 0;
    std::string uMap = "ByPolygonVertex", uRef = "IndexToDirect";
    if (const FNode* le = geo.child("LayerElementUV")) {
        if (const FNode* c = le->child("UV")) if (!c->props.empty()) UVA = &c->props[0].ad;
        if (const FNode* c = le->child("UVIndex")) if (!c->props.empty()) UIDX = &c->props[0].ai;
        if (const FNode* c = le->child("MappingInformationType")) if (!c->props.empty()) uMap = c->props[0].s;
        if (const FNode* c = le->child("ReferenceInformationType")) if (!c->props.empty()) uRef = c->props[0].s;
    }
    const bool nByVert = (nMap == "ByVertice" || nMap == "ByVertex" || nMap == "ByControlPoint");
    const bool uByVert = (uMap == "ByVertice" || uMap == "ByVertex" || uMap == "ByControlPoint");

    // ---- material POR POLIGONO (LayerElementMaterial) -> varios mesh parts. ByPolygon: 1 indice por poligono (en el
    //      orden de PolygonVertexIndex); AllSame: todo el mesh usa el indice [0]. El indice mapea a 'mats' (la lista
    //      ORDENADA de materiales conectados al Model de esta geometria; ver ParsearMaterialesFBX). ----
    const std::vector<long long>* MATIDX = 0; std::string mMap = "AllSame";
    if (const FNode* le = geo.child("LayerElementMaterial")) {
        if (const FNode* c = le->child("Materials")) if (!c->props.empty()) MATIDX = &c->props[0].ai;
        if (const FNode* c = le->child("MappingInformationType")) if (!c->props.empty()) mMap = c->props[0].s;
    }
    const bool matByPoly = (mMap == "ByPolygon" && MATIDX && !MATIDX->empty());
    const int  matAllSame = (MATIDX && !MATIDX->empty()) ? (int)(*MATIDX)[0] : 0; // AllSame o sin LEM -> indice unico

    Wavefront Wobj; Wobj.Reset();
    // posiciones (control points)
    Wobj.vertex.reserve(V.size());
    for (size_t i = 0; i < V.size(); i++) Wobj.vertex.push_back((GLfloat)V[i]);

    // recorrer los poligonos: cada corner tiene un control-point (cp) y un indice de corner corrido (c)
    Face cara;
    int c = 0;               // indice global de corner
    int polyIdx = 0;         // indice de poligono (para LayerElementMaterial ByPolygon)
    std::vector<int> faceMat; // indice de material por cara (paralelo a Wobj.faces)
    for (size_t k = 0; k < PI.size(); k++) {
        if ((k & 4095) == 0) ProgresoActualizar(progBase + progSpan * 0.6f * ((float)k / (float)PI.size())); // 1er 60% del tramo (el resto = pasos post-loop, que en el N95 son lo pesado)
        long long raw = PI[k];
        bool fin = (raw < 0);
        int cp = (int)(fin ? (~raw) : raw); // el ultimo del poligono viene como ~cp

        FaceCorner fc; fc.vertex = cp; fc.color = -1; fc.normal = -1; fc.uv = -1;

        // NORMAL para este corner
        if (NRM) {
            int base = nByVert ? cp : c;
            int idx = base;
            if (nRef == "IndexToDirect" && NIDX && base < (int)NIDX->size()) idx = (int)(*NIDX)[base];
            if (idx >= 0 && (size_t)idx * 3 + 2 < NRM->size()) {
                Wobj.normals.push_back(NrmB((*NRM)[idx*3]));
                Wobj.normals.push_back(NrmB((*NRM)[idx*3+1]));
                Wobj.normals.push_back(NrmB((*NRM)[idx*3+2]));
                fc.normal = (int)(Wobj.normals.size() / 3) - 1;
            }
        }
        // UV para este corner
        if (UVA) {
            int base = uByVert ? cp : c;
            int idx = base;
            if (uRef == "IndexToDirect" && UIDX && base < (int)UIDX->size()) idx = (int)(*UIDX)[base];
            if (idx >= 0 && (size_t)idx * 2 + 1 < UVA->size()) {
                Wobj.uv.push_back((GLfloat)(*UVA)[idx*2]);
                Wobj.uv.push_back((GLfloat)(1.0 - (*UVA)[idx*2+1])); // FBX V va al reves que OpenGL
                fc.uv = (int)(Wobj.uv.size() / 2) - 1;
            }
        }
        cara.corners.push_back(fc);
        c++;
        if (fin) {
            if (cara.corners.size() >= 3) {
                Wobj.faces.push_back(cara);
                int mi = matByPoly ? (polyIdx < (int)MATIDX->size() ? (int)(*MATIDX)[polyIdx] : 0) : matAllSame;
                if (mi < 0) mi = 0;
                faceMat.push_back(mi);
            }
            cara.corners.clear();
            polyIdx++; // cuenta TODOS los poligonos (aunque sean degenerados) -> alinea con el array ByPolygon del FBX
        }
    }
    if (Wobj.faces.empty()) return 0;

    // ---- MESH PARTS: bucketear las caras por indice de material (contiguo) y crear un MaterialGroup por material USADO.
    //      ConvertToES1 agrupa por materialsGroup[m].start = 1er cara del grupo -> las caras tienen que quedar ordenadas
    //      por material (el FBX ByPolygon puede intercalarlos). Si no hay materiales (mats vacio) -> 1 grupo default. ----
    int nMats = (int)mats.size();
    if (nMats <= 1) {
        // caso simple: 1 material (o ninguno) para toda la malla -> 1 grupo (comportamiento clasico)
        Material* mat = nMats == 1 ? mats[0] : NULL;
        if (mat) { MaterialGroup mg; mg.material = mat; mg.name = mat->name; mg.start = 0; mg.count = 0;
                   mg.startDrawn = 0; mg.indicesDrawnCount = 0; Wobj.materialsGroup.push_back(mg); }
    } else {
        for (size_t i = 0; i < faceMat.size(); i++) if (faceMat[i] >= nMats) faceMat[i] = nMats - 1; // clamp defensivo
        std::vector<Face> ordFaces; ordFaces.reserve(Wobj.faces.size());
        for (int mi = 0; mi < nMats; mi++) {
            int start = (int)ordFaces.size();
            for (size_t f = 0; f < faceMat.size(); f++) if (faceMat[f] == mi) ordFaces.push_back(Wobj.faces[f]);
            int cnt = (int)ordFaces.size() - start;
            if (cnt <= 0) continue; // material sin caras en esta malla -> no crear mesh part vacio
            MaterialGroup mg; mg.material = mats[mi];
            mg.name = mats[mi] ? mats[mi]->name : std::string("Mesh");
            mg.start = start; mg.count = 0; mg.startDrawn = 0; mg.indicesDrawnCount = 0;
            Wobj.materialsGroup.push_back(mg);
        }
        Wobj.faces.swap(ordFaces); // caras reordenadas por material (los fc.normal/fc.uv siguen apuntando bien: son indices a arrays propios)
    }

    Mesh* mesh = new Mesh(parent, Vector3(0, 0, 0));
    mesh->name = nombre;
    int a0 = 0, a1 = 0, a2 = 0;
    Wobj.ConvertToES1(mesh, &a0, &a1, &a2, &mesh->vertCtrlPoint); // guarda vertice->control-point (para weight paint)
    ProgresoActualizar(progBase + progSpan * 0.75f); // dedup listo; lo de abajo es lo pesado en el N95
    mesh->CalcularBordes();
    ProgresoActualizar(progBase + progSpan * 0.92f);
    if (!mesh->normals && mesh->vertexSize > 0) { // sin normales -> smooth (como el OBJ)
        mesh->normals = new GLbyte[mesh->vertexSize * 3];
        mesh->meshSmooth = true;
        mesh->RecalcularNormales();
    } else if (mesh->normals && mesh->vertexSize > 0) {
        // CON normales del FBX: detectar smooth/flat para que al editar un vertice no se recalcule todo flat (Dante)
        mesh->meshSmooth = MeshShadingImportadoEsSmooth(mesh);
    }
#ifndef W3D_SYMBIAN
    mesh->OptimizarCacheRender(); // cache de vertices (Forsyth): caro y de beneficio ~nulo en el render del N95;
                                  // se saltea alli para no eternizar el import (el orden de indices no cambia la imagen).
#endif
    ProgresoActualizar(progBase + progSpan);
    return mesh;
}

// aplica la correccion FBX a un objeto. escalar = escala de UNIDADES (UnitScaleFactor/100: FBX en cm, Whisk3D en
// metros). rotar = -90° en X para pasar de Z-up (geometria del mesh) a Y-up (Whisk3D). OJO: la GEOMETRIA del mesh
// viene Z-up (necesita el -90°), pero los HUESOS del esqueleto YA vienen Y-up (matrices TransformLink en el espacio
// de escena del FBX) -> la armature NO se rota, solo se escala; el -90° va en la malla. Si no hay esqueleto, la malla
// lleva ambos. Es lo mismo que Blender: el esqueleto queda con escala 0.01 y la malla parada.
static void AplicarTransformFBX(Object* o, double escala, bool rotar, bool escalar) {
    if (!o) return;
    if (escalar && escala > 0.0 && escala != 1.0) o->scale = Vector3((float)escala, (float)escala, (float)escala);
    if (rotar) { o->rot = Quaternion::FromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), -90.0f); o->rot.normalize(); }
    o->ActualizarDisplayRot(); // refresca rotEuler para Properties
}

// busca recursivamente el 1er RelativeFilename/FileName de un nodo Texture/Video (la ruta de la textura)
static void JuntarTexturas(const FNode& objs, std::vector<std::string>& out) {
    for (size_t i = 0; i < objs.kids.size(); i++) {
        const FNode& k = objs.kids[i];
        if (k.name == "Texture" || k.name == "Video") {
            std::string rel, abs;
            if (const FNode* c = k.child("RelativeFilename")) if (!c->props.empty()) rel = c->props[0].s;
            if (const FNode* c = k.child("FileName"))         if (!c->props.empty()) abs = c->props[0].s;
            if (const FNode* c = k.child("Filename"))         if (!c->props.empty() && abs.empty()) abs = c->props[0].s;
            if (!rel.empty()) out.push_back(rel);
            else if (!abs.empty()) out.push_back(abs);
        }
    }
}

// limpia el nombre de un Model FBX: en binario viene "pelvis\0\1Model" -> "pelvis"
static std::string LimpiarNombreFBX(const std::string& s) {
    size_t z = s.find('\0');
    return (z == std::string::npos) ? s : s.substr(0, z);
}
// lee un array de 16 doubles (matriz col-major) de un hijo por nombre. true si existe.
static bool LeerMat16(const FNode& n, const char* hijo, double m[16]) {
    const FNode* c = n.child(hijo);
    if (!c || c->props.empty() || c->props[0].ad.size() < 16) return false;
    for (int i = 0; i < 16; i++) m[i] = c->props[0].ad[i];
    return true;
}

// un grupo de vertices (pesos de UN hueso sobre UNA malla). verts = indices de CONTROL-POINT del FBX (crudos: sirven
// para mostrar el nombre del grupo y guardar el dato; el mapeo exacto al vertex[] de render se hara al deformar).
struct VGrupo { std::string bone; std::vector<int> verts; std::vector<float> pesos; };
struct EsqueletoFBX {
    std::vector<W3dBone> bones;                          // huesos (rest pose, espacio crudo del FBX)
    std::map<long long, std::vector<VGrupo> > vgPorGeo;  // geoId -> grupos de vertices (uno por hueso)
    std::map<long long, int> boneModelId;                // Model id (LimbNode) -> indice en bones (para las animaciones)
    bool hay() const { return !bones.empty(); }
};

// Parsea el esqueleto y los pesos: recorre los Deformer(Cluster) -> cada uno referencia UN hueso (Model LimbNode) via
// Connections, trae su matriz global de bind (TransformLink -> head del hueso) y los pesos por control-point. El
// parentado de huesos sale de las Connections OO entre Models. Rellena 'out'.
static void ParsearEsqueleto(const FNode& root, const FNode& objs, EsqueletoFBX& out) {
    // 1) Models: id -> nombre / tipo / nodo (para leer los transforms locales de rest)
    std::map<long long, std::string> modelName, modelType;
    std::map<long long, const FNode*> modelNode;
    for (size_t i = 0; i < objs.kids.size(); i++) {
        const FNode& k = objs.kids[i];
        if (k.name != "Model" || k.props.size() < 3) continue;
        modelName[k.props[0].i] = LimpiarNombreFBX(k.props[1].s);
        modelType[k.props[0].i] = k.props[2].s;
        modelNode[k.props[0].i] = &k;
    }
    // 2) Connections OO: child -> parent (primer id = origen/hijo, segundo = destino/padre). OJO: un hueso (Model)
    // se conecta como "hijo" a DOS destinos -> su hueso PADRE y su Cluster; un mapa simple se pisaria. Por eso, para
    // la JERARQUIA de huesos usamos 'modelParentOf' (SOLO conexiones Model->Model). 'parentOf' sirve para la cadena
    // cluster->skin->geometry (nodos no-Model).
    std::map<long long, long long> parentOf, modelParentOf;
    std::multimap<long long, long long> childrenOf;
    if (const FNode* conns = root.child("Connections")) {
        for (size_t i = 0; i < conns->kids.size(); i++) {
            const FNode& c = conns->kids[i];
            if (c.name != "C" || c.props.size() < 3 || c.props[0].s != "OO") continue;
            long long ch = c.props[1].i, pa = c.props[2].i;
            parentOf[ch] = pa;
            childrenOf.insert(std::make_pair(pa, ch));
            if (modelType.count(ch) && modelType.count(pa)) modelParentOf[ch] = pa; // hueso -> hueso padre
        }
    }
    // 3) Clusters -> huesos + grupos de vertices
    std::map<long long, int> boneIdx; // boneModelId -> indice en out.bones
    for (size_t i = 0; i < objs.kids.size(); i++) {
        const FNode& k = objs.kids[i];
        if (k.name != "Deformer" || k.props.size() < 3 || k.props[2].s != "Cluster") continue;
        long long clusterId = k.props[0].i;
        // hueso = el Model conectado al cluster (hijo del cluster en el grafo OO)
        long long boneId = -1;
        std::pair<std::multimap<long long, long long>::iterator, std::multimap<long long, long long>::iterator>
            rg = childrenOf.equal_range(clusterId);
        for (std::multimap<long long, long long>::iterator it = rg.first; it != rg.second; ++it)
            if (modelType.count(it->second)) { boneId = it->second; break; }
        if (boneId < 0) continue;
        // geometria: cluster -> skin -> geometry (subiendo por parentOf)
        long long geoId = -1;
        if (parentOf.count(clusterId)) { long long skinId = parentOf[clusterId];
            if (parentOf.count(skinId)) geoId = parentOf[skinId]; }
        std::string bname = modelName.count(boneId) ? modelName[boneId] : std::string("bone");
        // head del hueso = translacion de TransformLink (matriz global de bind)
        if (!boneIdx.count(boneId)) {
            double TL[16];
            W3dBone b; b.name = bname;
            if (LeerMat16(k, "TransformLink", TL)) {
                b.head = Vector3((float)TL[12], (float)TL[13], (float)TL[14]);
                for (int mi = 0; mi < 16; mi++) b.bind.m[mi] = (float)TL[mi]; // bind global (para el skinning)
                b.hasSkin = true;
            }
            // matriz 'Transform' del cluster (geometria->bind): necesaria para el inverse-bind estandar FBX en rigs
            // cuyo TransformLink es degenerado (ej LISA: TL = solo escala, y el swap de ejes vive aca).
            double TR[16];
            if (LeerMat16(k, "Transform", TR)) for (int mi = 0; mi < 16; mi++) b.clusterTransform.m[mi] = (float)TR[mi];
            b.tail = b.head; // se corrige abajo
            // transform LOCAL de rest (del Model del hueso) para el FK
            if (modelNode.count(boneId)) { const FNode& mn = *modelNode[boneId];
                b.restT = LeerP70Vec3(mn, "Lcl Translation", Vector3(0,0,0));
                b.restR = LeerP70Vec3(mn, "Lcl Rotation",    Vector3(0,0,0));
                b.restS = LeerP70Vec3(mn, "Lcl Scaling",     Vector3(1,1,1));
                b.preRot= LeerP70Vec3(mn, "PreRotation",     Vector3(0,0,0));
                b.postRot  = LeerP70Vec3(mn, "PostRotation",   Vector3(0,0,0));
                b.rotPivot = LeerP70Vec3(mn, "RotationPivot",  Vector3(0,0,0));
                b.rotOffset= LeerP70Vec3(mn, "RotationOffset", Vector3(0,0,0));
                b.sclPivot = LeerP70Vec3(mn, "ScalingPivot",   Vector3(0,0,0));
                b.sclOffset= LeerP70Vec3(mn, "ScalingOffset",  Vector3(0,0,0));
                b.rotOrder = (int)LeerP70D(mn, "RotationOrder", 0);
                b.hasRest = true;
            }
            boneIdx[boneId] = (int)out.bones.size();
            out.bones.push_back(b);
        }
        // grupo de vertices de esa geometria
        if (geoId >= 0) {
            VGrupo vg; vg.bone = bname;
            if (const FNode* nI = k.child("Indexes")) if (!nI->props.empty())
                for (size_t j = 0; j < nI->props[0].ai.size(); j++) vg.verts.push_back((int)nI->props[0].ai[j]);
            if (const FNode* nW = k.child("Weights")) if (!nW->props.empty())
                for (size_t j = 0; j < nW->props[0].ad.size(); j++) vg.pesos.push_back((float)nW->props[0].ad[j]);
            out.vgPorGeo[geoId].push_back(vg);
        }
    }
    // 4) parentado: subir por parentOf hasta encontrar OTRO hueso
    std::vector<long long> idPorIdx(out.bones.size(), -1);
    for (std::map<long long, int>::iterator it = boneIdx.begin(); it != boneIdx.end(); ++it)
        idPorIdx[it->second] = it->first;
    for (int bi = 0; bi < (int)out.bones.size(); bi++) {
        // subir por la cadena Model->Model hasta encontrar otro hueso QUE TENGA cluster (este en boneIdx). Asi los
        // huesos intermedios sin peso (ej. algun "root") no rompen la jerarquia: el hijo se cuelga del proximo hueso real.
        long long p = modelParentOf.count(idPorIdx[bi]) ? modelParentOf[idPorIdx[bi]] : -1;
        while (p != -1 && !boneIdx.count(p)) p = modelParentOf.count(p) ? modelParentOf[p] : -1;
        out.bones[bi].parent = (p != -1 && boneIdx.count(p)) ? boneIdx[p] : -1;
    }
    // 5) tail: si tiene hijos -> tail = head del 1er hijo; si es hoja -> prolonga la direccion padre->hueso
    std::vector<int> primerHijo(out.bones.size(), -1);
    for (int bi = 0; bi < (int)out.bones.size(); bi++) {
        int par = out.bones[bi].parent;
        if (par >= 0 && primerHijo[par] < 0) primerHijo[par] = bi;
    }
    for (int bi = 0; bi < (int)out.bones.size(); bi++) {
        if (primerHijo[bi] >= 0) { out.bones[bi].tail = out.bones[primerHijo[bi]].head; continue; }
        int par = out.bones[bi].parent;
        Vector3 d = (par >= 0) ? (out.bones[bi].head - out.bones[par].head) : Vector3(0, 1, 0);
        float len = d.Length(); if (len < 1e-4f) { d = Vector3(0, 1, 0); len = 5.0f; }
        out.bones[bi].tail = out.bones[bi].head + d * (1.0f / d.Length()) * len;
    }
    out.boneModelId = boneIdx; // Model id -> indice de hueso (lo usan las animaciones)
}

// ============================================================================
//  ANIMACIONES FBX -> SkeletalAnimation. Grafo:
//    AnimationStack (take) <-OO- AnimationLayer <-OO- AnimationCurveNode <-OP("Lcl Translation/Rotation/Scaling")- Model(hueso)
//    AnimationCurveNode <-OP("d|X"/"d|Y"/"d|Z")- AnimationCurve (KeyTime[int64], KeyValueFloat[float])
//  El KeyTime esta en unidades FBX (1/46186158000 s). frame = KeyTime * fps / 46186158000.
// ============================================================================
static const long long FBX_TIME_UNIT = 46186158000LL; // ktime por segundo

struct FCurve { std::vector<long long> t; std::vector<float> v; }; // una curva (un eje)

static int PropDeNombre(const std::string& p) {           // "Lcl Translation/Rotation/Scaling" -> AnimPosition/Rotation/Scale
    if (p.find("Translation") != std::string::npos) return AnimPosition;
    if (p.find("Rotation")    != std::string::npos) return AnimRotation;
    if (p.find("Scaling")     != std::string::npos) return AnimScale;
    return -1;
}
static int EjeDeNombre(const std::string& p) {            // "d|X"/"d|Y"/"d|Z" -> 0/1/2
    if (!p.empty()) { char c = p[p.size()-1]; if (c=='X') return 0; if (c=='Y') return 1; if (c=='Z') return 2; }
    return -1;
}
// valor de una curva (un eje) en el frame f: escalon/lineal entre sus keys (frames ya convertidos)
static float EvalEje(const std::vector<int>& fr, const std::vector<float>& va, int f) {
    if (fr.empty()) return 0.0f;
    if (f <= fr.front()) return va.front();
    if (f >= fr.back())  return va.back();
    for (size_t i = 1; i < fr.size(); i++) if (fr[i] >= f) {
        int f0 = fr[i-1], f1 = fr[i]; float v0 = va[i-1], v1 = va[i];
        if (f1 == f0) return v1;
        return v0 + (v1 - v0) * (float)(f - f0) / (float)(f1 - f0);
    }
    return va.back();
}

// arma los clips de animacion del FBX y los mete en 'anims' (uno por AnimationStack)
static void ParsearAnimaciones(const FNode& root, const FNode& objs, const EsqueletoFBX& esq,
                               std::vector<SkeletalAnimation*>& anims, int fps, float escala) {
    if (esq.bones.empty()) return;
    // 1) juntar objetos de animacion por id
    std::map<long long, std::string> stackName;              // AnimationStack id -> nombre
    std::map<long long, FCurve> curves;                      // AnimationCurve id -> keys (t,v)
    std::vector<long long> nodeIds;                          // AnimationCurveNode ids (presencia)
    std::map<long long,bool> esNode, esLayer;
    for (size_t i = 0; i < objs.kids.size(); i++) {
        const FNode& k = objs.kids[i];
        if (k.props.empty()) continue;
        long long id = k.props[0].i;
        if (k.name == "AnimationStack" && k.props.size() >= 2) stackName[id] = LimpiarNombreFBX(k.props[1].s);
        else if (k.name == "AnimationLayer") esLayer[id] = true;
        else if (k.name == "AnimationCurveNode") esNode[id] = true;
        else if (k.name == "AnimationCurve") {
            FCurve fc;
            if (const FNode* kt = k.child("KeyTime")) if (!kt->props.empty())
                for (size_t j = 0; j < kt->props[0].ai.size(); j++) fc.t.push_back(kt->props[0].ai[j]);
            if (const FNode* kv = k.child("KeyValueFloat")) if (!kv->props.empty())
                for (size_t j = 0; j < kv->props[0].ad.size(); j++) fc.v.push_back((float)kv->props[0].ad[j]);
            curves[id] = fc;
        }
    }
    if (stackName.empty()) return; // FBX sin animaciones
    // 2) conexiones: OO (node->layer->stack) y OP (curve->node "d|X"; node->model "Lcl ...")
    std::map<long long, long long> layerDeNode, stackDeLayer; // OO
    std::map<long long, std::pair<long long,std::string> > nodeDeCurve; // curveId -> (nodeId, "d|X")
    std::map<long long, std::pair<long long,std::string> > modelDeNode; // nodeId  -> (modelId, "Lcl ...")
    if (const FNode* conns = root.child("Connections")) {
        for (size_t i = 0; i < conns->kids.size(); i++) {
            const FNode& c = conns->kids[i];
            if (c.name != "C" || c.props.size() < 3) continue;
            long long ch = c.props[1].i, pa = c.props[2].i;
            if (c.props[0].s == "OO") {
                if (esNode.count(ch) && esLayer.count(pa)) layerDeNode[ch] = pa;
                else if (esLayer.count(ch) && stackName.count(pa)) stackDeLayer[ch] = pa;
            } else if (c.props[0].s == "OP" && c.props.size() >= 4) {
                std::string prop = c.props[3].s;
                if (curves.count(ch) && esNode.count(pa)) nodeDeCurve[ch] = std::make_pair(pa, prop);
                else if (esNode.count(ch) && esq.boneModelId.count(pa)) modelDeNode[ch] = std::make_pair(pa, prop);
            }
        }
    }
    // 3) un clip por stack
    std::map<long long, SkeletalAnimation*> clipDeStack;
    for (std::map<long long,std::string>::iterator it = stackName.begin(); it != stackName.end(); ++it) {
        SkeletalAnimation* a = new SkeletalAnimation(it->second.empty() ? "Animation" : it->second);
        a->FrameRate = fps; clipDeStack[it->first] = a; anims.push_back(a);
    }
    // 4) volcar cada curva al hueso/propiedad/eje que corresponde
    // acumulador: clip -> bone -> prop -> eje -> (frame->valor)
    for (std::map<long long, FCurve>::iterator it = curves.begin(); it != curves.end(); ++it) {
        long long curveId = it->first; const FCurve& fc = it->second;
        if (!nodeDeCurve.count(curveId)) continue;
        long long nodeId = nodeDeCurve[curveId].first;
        int eje = EjeDeNombre(nodeDeCurve[curveId].second);
        if (eje < 0 || !modelDeNode.count(nodeId)) continue;
        long long modelId = modelDeNode[nodeId].first;
        int prop = PropDeNombre(modelDeNode[nodeId].second);
        if (prop < 0 || !esq.boneModelId.count(modelId)) continue;
        int bone = esq.boneModelId.find(modelId)->second;
        // clip: por la cadena node->layer->stack
        SkeletalAnimation* clip = NULL;
        if (layerDeNode.count(nodeId) && stackDeLayer.count(layerDeNode[nodeId]))
            clip = clipDeStack.count(stackDeLayer[layerDeNode[nodeId]]) ? clipDeStack[stackDeLayer[layerDeNode[nodeId]]] : NULL;
        if (!clip && !anims.empty()) clip = anims[0]; // fallback: 1er clip
        if (!clip) continue;
        // el FK va en espacio RAW del FBX (el Object de la armature aplica la escala 0.01 al dibujar): NO escalar aca
        std::vector<int> fr; std::vector<float> va;
        for (size_t j = 0; j < fc.t.size() && j < fc.v.size(); j++) {
            fr.push_back((int)((fc.t[j] * (long long)fps) / FBX_TIME_UNIT) + 1); // +1: FBX arranca en 0, el timeline en 1
            va.push_back((float)fc.v[j]);
        }
        if (fr.empty()) continue;
        BoneTrack& tr = clip->TrackDe(bone);
        AnimProperty& ap = tr.PropertyDe(prop);
        // resamplear: unir el set de frames existentes + los de esta curva, y setear el eje en cada uno
        std::vector<int> frames;
        for (size_t j = 0; j < ap.keyframes.size(); j++) frames.push_back(ap.keyframes[j].frame);
        for (size_t j = 0; j < fr.size(); j++) frames.push_back(fr[j]);
        std::sort(frames.begin(), frames.end());
        frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
        std::vector<keyFrame> nuevos; nuevos.reserve(frames.size());
        for (size_t j = 0; j < frames.size(); j++) {
            keyFrame kf; kf.frame = frames[j]; kf.Interpolation = 0;
            // otros ejes: INTERPOLAR lineal los keyframes existentes en este frame (NO "hold": el escalon rompia
            // las rotaciones -> el hueso giraba loco). Asi cada eje queda continuo aunque los ejes tengan keys distintos.
            kf.valueX = kf.valueY = kf.valueZ = 0.0f;
            const std::vector<keyFrame>& K = ap.keyframes; int f = frames[j];
            if (!K.empty()) {
                if (f <= K.front().frame)      { kf.valueX=K.front().valueX; kf.valueY=K.front().valueY; kf.valueZ=K.front().valueZ; }
                else if (f >= K.back().frame)  { kf.valueX=K.back().valueX;  kf.valueY=K.back().valueY;  kf.valueZ=K.back().valueZ; }
                else for (size_t p = 1; p < K.size(); p++) if (K[p].frame >= f) {
                    int f0=K[p-1].frame, f1=K[p].frame; float t = (f1==f0)?0.0f:(float)(f-f0)/(float)(f1-f0);
                    kf.valueX = K[p-1].valueX + (K[p].valueX-K[p-1].valueX)*t;
                    kf.valueY = K[p-1].valueY + (K[p].valueY-K[p-1].valueY)*t;
                    kf.valueZ = K[p-1].valueZ + (K[p].valueZ-K[p-1].valueZ)*t; break;
                }
            }
            float ev = EvalEje(fr, va, frames[j]);
            if (eje == 0) kf.valueX = ev; else if (eje == 1) kf.valueY = ev; else kf.valueZ = ev;
            nuevos.push_back(kf);
        }
        ap.keyframes = nuevos;
    }
    // 5) rango [startFrame..endFrame] de cada clip = min/max de sus keyframes
    for (size_t a = 0; a < anims.size(); a++) {
        int mn = 0x7fffffff, mx = 0;
        for (size_t t = 0; t < anims[a]->tracks.size(); t++)
            for (size_t p = 0; p < anims[a]->tracks[t].Propertys.size(); p++)
                for (size_t k = 0; k < anims[a]->tracks[t].Propertys[p].keyframes.size(); k++) {
                    int f = anims[a]->tracks[t].Propertys[p].keyframes[k].frame;
                    if (f < mn) mn = f; if (f > mx) mx = f;
                }
        if (mx >= mn) { anims[a]->startFrame = mn; anims[a]->endFrame = mx; }
    }
}

// ============================================================================
//  MATERIALES FBX: cada geometria (mesh) tiene N materiales, uno por "mesh part". El grafo del FBX es:
//    Texture --OP("DiffuseColor")--> Material --OO--> Model <--OO-- Geometry
//  El indice de material de LayerElementMaterial (por poligono) es la POSICION del material en la lista de
//  materiales conectados a ese Model (en el orden de las Connections). Aca se arma ese mapeo.
// ============================================================================
struct MatsFBX {
    std::map<long long, Material*>              porId;      // id material FBX -> Material creado
    std::map<long long, std::vector<long long> > deModel;   // id Model -> ids de material EN ORDEN (indice = posicion)
    std::map<long long, long long>              modelDeGeo; // id Geometry -> id Model
    // lista ORDENADA de Material* para una geometria (via su Model). Vacia si no se resolvio (-> fallback en el caller).
    std::vector<Material*> deGeometria(long long geoId) const {
        std::vector<Material*> out;
        std::map<long long,long long>::const_iterator mg = modelDeGeo.find(geoId);
        if (mg == modelDeGeo.end()) return out;
        std::map<long long, std::vector<long long> >::const_iterator ml = deModel.find(mg->second);
        if (ml == deModel.end()) return out;
        for (size_t i = 0; i < ml->second.size(); i++) {
            std::map<long long,Material*>::const_iterator it = porId.find(ml->second[i]);
            out.push_back(it == porId.end() ? (Material*)NULL : it->second);
        }
        return out;
    }
};

// Materializa una textura EMBEBIDA (bytes del Content de un nodo Video) a un archivo junto al FBX y devuelve su
// ruta (o "" si no se pudo escribir). Muchos FBX de 3ds Max (chicken, nani) embeben la imagen y su RelativeFilename
// apunta a una ruta de otra maquina inexistente -> hay que volcar estos bytes a disco porque el pipeline decodifica
// por ARCHIVO. El nombre sale del RelativeFilename (conserva extension); si no, se detecta por magic bytes.
static std::string MaterializarTexturaEmbebida(const std::string& fbxDir, const std::string& nombre, const std::string& bytes) {
    if (bytes.empty()) return std::string();
    std::string fn = nombre;
    if (fn.empty()) {
        const unsigned char* b = (const unsigned char*)&bytes[0]; size_t n = bytes.size();
        const char* ext = ".bin";
        if      (n >= 4 && b[0]==0x89 && b[1]=='P' && b[2]=='N' && b[3]=='G') ext = ".png";
        else if (n >= 3 && b[0]==0xFF && b[1]==0xD8 && b[2]==0xFF)            ext = ".jpg";
        else if (n >= 2 && b[0]=='B'  && b[1]=='M')                           ext = ".bmp";
        else if (n >= 4 && b[0]=='D'  && b[1]=='D' && b[2]=='S' && b[3]==' ') ext = ".dds";
        static int cont = 0; char buf[40]; sprintf(buf, "embedded_%d%s", cont++, ext); fn = buf;
    }
    std::string ruta = w3dFileSystem::JoinPath(fbxDir, fn);
    if (w3dFileSystem::FileExists(ruta)) return ruta;          // ya existe (materializada antes o el autor la dejo)
    if (!w3dFileSystem::WriteTextFile(ruta, bytes)) return std::string();
    w3dLogf("ImportFBX: textura embebida extraida -> %s (%d bytes)", fn.c_str(), (int)bytes.size());
    return ruta;
}

static void ParsearMaterialesFBX(const FNode& root, const FNode& objs, const std::string& filepath, MatsFBX& out) {
    // 1) recolectar ids de Material / Texture / Geometry / Model (para clasificar las conexiones)
    std::set<long long> matIds, texIds, geoIds, modelIds;
    std::map<long long, std::string> matNombre;  // id material -> nombre limpio
    std::map<long long, std::string> texRel;      // id textura  -> ruta relativa
    std::map<long long, std::string> texContent;      // id (Texture/Video) -> bytes de imagen EMBEBIDA (Content)
    std::map<long long, std::string> texContentName;  // id -> nombre de archivo (del RelativeFilename) para materializar
    for (size_t i = 0; i < objs.kids.size(); i++) {
        const FNode& k = objs.kids[i]; if (k.props.empty()) continue;
        long long id = k.props[0].i;
        if (k.name == "Material") { matIds.insert(id); matNombre[id] = (k.props.size() > 1 ? LimpiarNombreFBX(k.props[1].s) : std::string("mat")); }
        else if (k.name == "Texture" || k.name == "Video") {
            std::string rel;
            if (const FNode* c = k.child("RelativeFilename")) if (!c->props.empty()) rel = c->props[0].s;
            if (rel.empty()) if (const FNode* c = k.child("FileName")) if (!c->props.empty()) rel = c->props[0].s;
            if (!rel.empty()) { texIds.insert(id); texRel[id] = rel; }
            // textura EMBEBIDA: el nodo Video trae los bytes de la imagen en su hijo Content (prop 'R'). Se guardan
            // para materializarlos si no se resuelve un archivo en disco (el RelativeFilename suele no existir aca).
            if (const FNode* c = k.child("Content")) if (!c->props.empty() && !c->props[0].s.empty()) {
                texIds.insert(id);
                texContent[id] = c->props[0].s;
                texContentName[id] = SoloNombre(rel);
            }
        }
        else if (k.name == "Geometry") geoIds.insert(id);
        else if (k.name == "Model")    modelIds.insert(id);
    }
    if (matIds.empty()) return; // FBX sin nodos Material -> el caller usa el fallback (1ra textura suelta)

    // 2) conexiones: material->Model (orden = indice de material), textura->material (prioriza DiffuseColor), geometria->Model
    std::map<long long, long long> texDeMat;  // id material -> id textura elegida
    std::map<long long, long long> contentDeTex; // id Texture -> id nodo (Video) con el Content embebido (via OO)
    std::set<long long> matDifusoElegido;     // material que ya fijo su textura DiffuseColor (no la pisa una secundaria)
    if (const FNode* conns = root.child("Connections")) {
        for (size_t i = 0; i < conns->kids.size(); i++) {
            const FNode& c = conns->kids[i];
            if (c.name != "C" || c.props.size() < 3) continue;
            long long src = c.props[1].i, dst = c.props[2].i;
            if (matIds.count(src) && modelIds.count(dst))      out.deModel[dst].push_back(src); // material -> Model (en orden)
            else if (geoIds.count(src) && modelIds.count(dst)) out.modelDeGeo[src] = dst;        // geometria -> Model
            else if (texIds.count(src) && matIds.count(dst)) {                                    // textura -> material
                std::string prop = c.props.size() > 3 ? c.props[3].s : std::string();
                bool difuso = (prop == "DiffuseColor" || prop == "Maya|baseColor" || prop == "3dsMax|Parameters|base_color_map" || prop == "baseColor");
                if (difuso) { texDeMat[dst] = src; matDifusoElegido.insert(dst); }
                else if (!matDifusoElegido.count(dst) && texDeMat.find(dst) == texDeMat.end()) texDeMat[dst] = src; // 1ra secundaria si no hay difusa
            }
            else if (texContent.count(src) && texIds.count(dst) && !matIds.count(dst)) contentDeTex[dst] = src; // Video(Content) -> Texture
        }
    }

    // 3) crear un Material por material FBX, resolviendo su textura (encolada como el resto; puede no existir en disco)
    std::string dir = DirDe(filepath);
    for (std::set<long long>::iterator it = matIds.begin(); it != matIds.end(); ++it) {
        long long id = *it;
        Material* mat = new Material(matNombre.count(id) ? matNombre[id] : std::string("mat"));
        Materials.push_back(mat);
        out.porId[id] = mat;
        std::map<long long,long long>::iterator td = texDeMat.find(id);
        if (td == texDeMat.end()) continue;
        long long tid = td->second;
        // a) archivo en disco (ruta relativa del FBX)
        std::map<long long,std::string>::iterator tr = texRel.find(tid);
        std::string res;
        if (tr != texRel.end()) res = ResolverTextura(dir, tr->second);
        if (!res.empty()) { EncolarTextura(mat, res); continue; }
        // b) textura EMBEBIDA: bytes en el propio Texture, o en el Video conectado por OO. Se materializa junto al FBX.
        long long cid = tid;
        if (!texContent.count(cid)) { std::map<long long,long long>::iterator vd = contentDeTex.find(tid); if (vd != contentDeTex.end()) cid = vd->second; }
        std::map<long long,std::string>::iterator cc = texContent.find(cid);
        if (cc != texContent.end()) {
            std::string nom = texContentName.count(cid) ? texContentName[cid] : std::string();
            if (nom.empty() && tr != texRel.end()) nom = SoloNombre(tr->second); // usar el nombre del Texture si el Video no lo tenia
            std::string ruta = MaterializarTexturaEmbebida(dir, nom, cc->second);
            if (!ruta.empty()) EncolarTextura(mat, ruta);
        }
    }
    w3dLogf("ImportFBX: %d material(es), %d con textura conectada", (int)matIds.size(), (int)texDeMat.size());
}

} // namespace

bool ImportFBX(const std::string& filepath) {
    std::vector<unsigned char> bytes;
    if (!w3dFileSystem::ReadFileBytes(filepath, bytes) || bytes.size() < 27) {
        w3dLogfE("ImportFBX: no se pudo leer '%s'", filepath.c_str());
        return false;
    }
    if (memcmp(&bytes[0], "Kaydara FBX Binary  ", 20) != 0) { // bytes.size()>=27 arriba -> &bytes[0] valido (sin .data(), C++03)
        w3dLogfE("ImportFBX: no es FBX BINARIO (ASCII no soportado): %s", filepath.c_str());
        return false;
    }
    uint32_t version = 0; memcpy(&version, &bytes[0] + 23, 4);
    bool w64 = (version >= 7500);

    ProgresoIniciar("Importing FBX..."); // barra de progreso (clave en el N95: parsear + convertir tarda)

    Rd r; r.p = &bytes[0] + 27; r.end = &bytes[0] + bytes.size();
    const unsigned char* base = &bytes[0];

    // nodos top-level hasta el null record (parseo del arbol: descomprime los arrays zlib -> lo mas pesado junto a
    // la conversion de la malla). Va llenando la barra hasta ~0.35 por nodo top-level.
    FNode root; root.name = "<root>";
    while (r.p < r.end) {
        size_t rem = (size_t)(r.end - r.p);
        if (rem < (w64 ? 25u : 13u)) break;
        FNode n;
        if (!LeerNodo(r, base, w64, n)) break;
        root.kids.push_back(n);
        ProgresoActualizar(0.35f * (float)(r.p - base) / (float)bytes.size()); // avance por bytes consumidos
    }

    const FNode* objs = root.child("Objects");
    if (!objs) { w3dLogfE("ImportFBX: sin nodo Objects"); ProgresoFin(); return false; }
    ProgresoActualizar(0.40f);

    // UNIDADES: el FBX suele venir en cm (UnitScaleFactor=1) y Whisk3D usa metros -> escala = UnitScaleFactor/100
    // (0.01 para este banana; = la escala 0.01 que Blender le pone al esqueleto). Si es 0/raro -> 1.
    double unit = LeerGlobalD(root, "UnitScaleFactor", 1.0);
    double escala = unit / 100.0; if (escala <= 0.0) escala = 1.0;

    // MATERIALES: un Material por cada Material del FBX (con su textura difusa), mapeados por Model a cada geometria
    // -> varios mesh parts. Si el FBX no declara materiales, FALLBACK: 1 material con la 1ra textura suelta que exista.
    MatsFBX matsInfo; ParsearMaterialesFBX(root, *objs, filepath, matsInfo);
    Material* matFallback = 0;
    if (matsInfo.porId.empty()) {
        std::vector<std::string> texs; JuntarTexturas(*objs, texs);
        std::string dir = DirDe(filepath);
        for (size_t i = 0; i < texs.size() && !matFallback; i++) {
            std::string res = ResolverTextura(dir, texs[i]);
            if (!res.empty()) {
                matFallback = new Material(ExtractBaseName(filepath) + "_mat");
                Materials.push_back(matFallback);
                EncolarTextura(matFallback, res); // carga diferida (1 por frame), como el OBJ
            }
        }
    }

    // ESQUELETO: si el FBX tiene huesos (Cluster/LimbNode), armar una Armature. La malla se PARENTA a ella y el
    // transform de correccion (-90° X + escala) va en la ARMATURE (igual que Blender: el esqueleto tiene la escala
    // 0.01 y la malla cuelga en identidad). Si no hay huesos, el transform va en cada malla (como antes).
    EsqueletoFBX esq; ParsearEsqueleto(root, *objs, esq);
    Armature* arm = 0;
    Object* parentMallas = CollectionActive;
    if (esq.hay()) {
        arm = new Armature(CollectionActive, Vector3(0, 0, 0));
        arm->name = ExtractBaseName(filepath) + "_rig";
        arm->bones = esq.bones;
        AplicarTransformFBX(arm, escala, false, true); // esqueleto: SOLO escala (los huesos ya vienen Y-up)
        parentMallas = arm;               // las mallas cuelgan del esqueleto (heredan la escala)
        w3dLogf("ImportFBX: esqueleto con %d hueso(s)", (int)esq.bones.size());
        // ANIMACIONES (takes): un clip por AnimationStack, con sus curvas por hueso. fps REAL del FBX (24/30/...)
        // el fps del ARCHIVO se usa SOLO para mapear KeyTime->frame (frames contiguos, sin gaps). La REPRODUCCION
        // queda en AnimFPS (default 30): el banana dice 24 pero se ve bien a 30 (metadato del FBX incorrecto, pedido Dante).
        int fps = (int)(FbxFrameRate(root) + 0.5); if (fps < 1) fps = 24;
        ParsearAnimaciones(root, *objs, esq, arm->animations, fps, escala);
        PrepararSkin(arm); // matrices de skinning (bind) de cada hueso
        // BIND POSE inicial: poseHead/poseTail (lo que DIBUJA el armature) = head/tail. En PC el pose-eval real los
        // recalcula al reproducir; en Symbian ese eval esta stub -> sin esto los huesos quedan en (0,0,0) e invisibles.
        for (size_t bi = 0; bi < arm->bones.size(); bi++) {
            arm->bones[bi].poseHead = arm->bones[bi].head;
            arm->bones[bi].poseTail = arm->bones[bi].tail;
        }
        if (!arm->animations.empty()) { arm->animActiva = 0;
            w3dLogf("ImportFBX: %d animacion(es) importada(s)", (int)arm->animations.size()); }

        // AXIS-FIX del ARMATURE (chicken y similares Y-up): si el esqueleto RENDERIZADO (FK-rest, ya pasado por
        // NodeToYup) queda ACOSTADO (mas alto en Z) pero el TransformLink dice PARADO (mas alto en Y), el modelo vino
        // Y-up y el -90°X (que se le mete a la malla) + el NodeToYup (huesos) lo acostaron. Se compensa rotando el
        // ARMATURE +90°X: como las mallas cuelgan del armature y los huesos viven en su espacio, esa rotacion PARA a
        // los dos. banana/otros Z-up coinciden FK<->TL -> no se tocan.
        {
            int saveAA = arm->animActiva; arm->animActiva = -1; arm->lastPoseFrame = -999999;
            EvaluarPoseEsqueleto(arm, 1); // FK-rest -> poseHead (lo que se dibuja)
            arm->animActiva = saveAA; arm->lastPoseFrame = -999999;
            Vector3 fmn(1e9f,1e9f,1e9f), fmx(-1e9f,-1e9f,-1e9f), tmn(1e9f,1e9f,1e9f), tmx(-1e9f,-1e9f,-1e9f);
            for (size_t bi = 0; bi < arm->bones.size(); bi++){ W3dBone& b = arm->bones[bi]; if (!b.hasSkin) continue;
                Vector3 fk = b.poseHead, tl(b.bind.m[12], b.bind.m[13], b.bind.m[14]);
                if(fk.x<fmn.x)fmn.x=fk.x; if(fk.y<fmn.y)fmn.y=fk.y; if(fk.z<fmn.z)fmn.z=fk.z;
                if(fk.x>fmx.x)fmx.x=fk.x; if(fk.y>fmx.y)fmx.y=fk.y; if(fk.z>fmx.z)fmx.z=fk.z;
                if(tl.x<tmn.x)tmn.x=tl.x; if(tl.y<tmn.y)tmn.y=tl.y; if(tl.z<tmn.z)tmn.z=tl.z;
                if(tl.x>tmx.x)tmx.x=tl.x; if(tl.y>tmx.y)tmx.y=tl.y; if(tl.z>tmx.z)tmx.z=tl.z; }
            float fkY=fmx.y-fmn.y, fkZ=fmx.z-fmn.z, tlY=tmx.y-tmn.y, tlZ=tmx.z-tmn.z;
            if (tlY > tlZ * 1.2f && fkZ > fkY * 1.2f){ // TL parado (Y) + FK-rest acostado (Z), con margen
                arm->rot = Quaternion::FromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), 90.0f);
                arm->rot.normalize(); arm->ActualizarDisplayRot();
                w3dLogf("ImportFBX: armature Y-up detectado -> +90X para pararlo (chicken)");
            }
            // ESCALA DE UNIDADES del biped: tras quitar el figure scale, el modelo queda a su tamaño natural (barney
            // ~65u, nani ~1.5u). Se elige la escala del armature (unidades del FBX vs metros) que lo deje a tamaño
            // humano (~1.7m): barney en cm (0.65m con 0.01), nani es metros (1.5m con escala 1, vs 0.015m con 0.01).
            if (arm->skinReconstruirFK){
                float d = (fmx - fmn).Length();
                if (d > 1e-4f){
                    float rEsc = (float)escala, sEsc = d * rEsc, s1 = d * 1.0f;
                    float esc = (fabsf(s1 - 1.7f) < fabsf(sEsc - 1.7f)) ? 1.0f : rEsc; // la mas cercana a ~1.7m
                    arm->scale = Vector3(esc, esc, esc);
                    w3dLogf("ImportFBX: biped escala de unidades -> armScale=%.4f (modelo ~%.2fu)", esc, d);
                }
            }
        }
    }
    ProgresoActualizar(0.45f);

    // cuantas Geometry hay (para repartir el tramo 0.45..0.98 de la barra entre las mallas)
    int totalGeo = 0;
    for (size_t i = 0; i < objs->kids.size(); i++) if (objs->kids[i].name == "Geometry") totalGeo++;
    if (totalGeo < 1) totalGeo = 1;

    // cada Geometry(Mesh) -> una malla (parentada a la armature si hay). Se guarda el id de la Geometry para poder
    // colgarle sus grupos de vertices (pesos por hueso) despues.
    int importadas = 0, geoVistas = 0;
    std::string baseNom = ExtractBaseName(filepath);
    for (size_t i = 0; i < objs->kids.size(); i++) {
        const FNode& k = objs->kids[i];
        if (k.name != "Geometry") continue;
        std::string nom = baseNom;
        if (importadas > 0) { char buf[16]; snprintf(buf, sizeof(buf), ".%03d", importadas); nom += buf; }
        float pBase = 0.45f + 0.53f * ((float)geoVistas / (float)totalGeo);
        float pSpan = 0.53f / (float)totalGeo;
        geoVistas++;
        // materiales (ORDENADOS) de esta geometria via su Model; si no se resolvieron, el fallback (1ra textura suelta)
        long long geoIdMat = k.props.empty() ? -1 : k.props[0].i;
        std::vector<Material*> matsGeo = matsInfo.deGeometria(geoIdMat);
        if (matsGeo.empty() && matFallback) matsGeo.push_back(matFallback);
        Mesh* m = MallaDesdeGeometry(k, nom, matsGeo, parentMallas, pBase, pSpan);
        if (!m) continue;
        // la geometria del mesh viene Z-up -> SIEMPRE -90° X para pararla. La escala la hereda de la armature; si no
        // hay esqueleto, la malla tambien lleva la escala.
        AplicarTransformFBX(m, escala, true, !arm);
        // grupos de vertices (huesos que deforman esta malla): por id de Geometry
        long long geoId = k.props.empty() ? -1 : k.props[0].i;
        std::map<long long, std::vector<VGrupo> >::iterator vit = esq.vgPorGeo.find(geoId);
        if (vit != esq.vgPorGeo.end()) {
            for (size_t g = 0; g < vit->second.size(); g++) {
                VGrupo& vg = vit->second[g];
                VertexGroup* grp = new VertexGroup(vg.bone);
                grp->verts = vg.verts;
                grp->pesos.assign(vg.pesos.begin(), vg.pesos.end());
                m->vertexGroups.push_back(grp);
            }
            if (!m->vertexGroups.empty()) m->grupoActivo = 0;
        }
        // AUTO: si hay esqueleto + animacion + pesos, agregar el modificador Armature (skinning) apuntando al rig
        if (arm && !arm->animations.empty() && !m->vertexGroups.empty()) {
            Modifier* amod = new Modifier(ModifierType::Armature, "Armature");
            amod->target = arm;
            m->modificadores.push_back(amod);
            m->modificadorActivo = (int)m->modificadores.size() - 1;
            m->skinArmature = arm; // el core deforma la malla a la pose del esqueleto
        }
        importadas++;
    }

    if (importadas == 0) { w3dLogfE("ImportFBX: no se pudo armar ninguna malla"); ProgresoFin(); return false; }
    w3dLogf("ImportFBX: %d malla(s) importada(s) de %s", importadas, filepath.c_str());
    ProgresoFin();
    Notificar("FBX imported successfully!", false); // toast verde de exito (igual que el OBJ)
    return true;
}
