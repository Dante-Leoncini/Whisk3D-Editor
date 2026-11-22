#include "Collection.h"
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "Camera.h"              // CameraActive: la lente de "sortOnce"
#include "objects/CameraBase.h"  // g_renderCamPos/Rot: la vista que esta dibujando
#include "objects/Mesh.h"        // lote estatico: hornea mallas (P4)
#include "objects/Materials.h"   // materiales simples (elegibilidad del lote)
#include "w3dGraphics.h"         // buffers + draws del lote
#include <algorithm>   // std::sort (orden lejos -> cerca)
#include <math.h>      // sqrtf (normalizar la rotacion del bake de normales)

// Constructor
Collection::Collection(Object* parent, Vector3 pos)
    : Object(parent, T("Collection"), pos){
    sortByCamera = false;
    sortOnce     = false;
    orderValid   = false;
    lastByCamera = false;
    lastOnce     = false;
    batch        = 0;      // off: comportamiento de siempre
    batchData    = NULL;
}

// Método getType
ObjectType Collection::getType() {
    return ObjectType::collection;
}

// hijo + su distancia a la camara. El orden es LEJOS -> CERCA (dist2 DESC): el
// unico valido para transparentes (el blend compone de atras hacia adelante).
// La distancia sale del ORIGEN del hijo en mundo (GetGlobalPosition): barato y
// suficiente para objetos chicos tipo fruta (no se mide el AABB como en Culling).
struct FarChild {
    Object* o;
    float   dist2;
    bool operator<(const FarChild& b) const { return dist2 > b.dist2; }
};

static void SortFarNear(const std::vector<Object*>& children, const Vector3& cam,
                              std::vector<FarChild>& order) {
    order.clear();
    order.reserve(children.size());
    for (size_t c = 0; c < children.size(); c++) {
        FarChild h;
        h.o = children[c];
        Vector3 d = children[c]->GetGlobalPosition() - cam;
        h.dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
        order.push_back(h);
    }
    std::sort(order.begin(), order.end());
}

// ===========================================================================
//  LOTE ESTATICO (P4) — ver Collection.h. El armado y el dibujo viven aca.
// ===========================================================================
struct W3dBatchRange {
    Material* mat;      // el material del rango (bucket por puntero: materiales
                        // COMPARTIDOS entre trozos se fusionan en un solo draw)
    Mesh*     host;     // una malla que usa ese material (presta ApplyMaterial)
    int       start;    // primer indice en idx[]
    int       count;    // cantidad de indices
};
struct W3dStaticBatch {
    std::vector<GLfloat>  pos;    // world ya aplicado
    std::vector<GLbyte>   nor;    // rotadas a world (normalizadas por columna)
    std::vector<GLubyte>  col;    // 255 donde la malla no traia color
    std::vector<GLfloat>  uvs;    // 0 donde no traia uv
    std::vector<MeshIndex> idx;   // rangos por material, contiguos
    std::vector<W3dBatchRange> ranges;
    std::vector<unsigned> members;   // seriales de TODO el scan elegible (revalida membresia)
    std::vector<char>     included;  // paralela a miembros: 1 = horneada en el lote
                                     // (0 = no entro por el tope de vertices: se dibuja normal)
    unsigned vboPos, vboNor, vboCol, vboUV, vboIdx;
    bool uploaded;
    W3dStaticBatch() : vboPos(0), vboNor(0), vboCol(0), vboUV(0), vboIdx(0), uploaded(false) {}
};

// tope PORTABLE de vertices por lote: MeshIndex es de 16 bits en el hardware
// chico (crossplatform.h). Lo que no entra queda AFUERA del lote (se dibuja
// normal): nunca indices invalidos.
static const size_t kBatchMaxVerts = 65500;

// una malla es HORNEABLE si es estatica y todos sus materiales son simples
// (ver Collection.h). Conservador a proposito: ante la duda, se dibuja normal.
static bool BatchEligible(Mesh* m) {
    if (!m || !m->visible || !m->renderable) return false;
    if (m->skinArmature || !m->modifiers.empty() || m->genValid) return false;
    if (m->flipbook || m->pvsFaces) return false;
    if (!m->vertex || m->vertexSize <= 0 || !m->faces || m->facesSize < 3) return false;
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        Material* mt = m->materialsGroup[g].material;
        if (!mt) continue;   // material default: simple
        if (mt->transparent || mt->chrome || mt->normalMap || !mt->layers.empty()) return false;
        if (mt->order_pass != 0 || mt->depth_bias != 0.0f || mt->bias_meters != 0.0f) return false;
        if (!mt->depth_test || !mt->depth_write) return false;
    }
    return true;
}

// junta las mallas horneables del subarbol (respetando visible de la cadena)
static void BatchCollect(Object* root, std::vector<Mesh*>& out) {
    std::vector<Object*> st; st.push_back(root);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (!o->visible) continue;
        if (o->getType() == ObjectType::mesh && BatchEligible((Mesh*)o)) out.push_back((Mesh*)o);
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
}

void Collection::BatchFree() {
    if (!batchData) return;
    namespace gfx = w3dEngine;
    // las mallas no se "destapan": el sello (inStaticBatch == w3dBatchStamp) solo
    // vale el frame en que el lote las marco; sin lote nadie las marca y vuelven
    // a dibujarse solas. Cero bookkeeping, cero punteros viejos.
    gfx::DeleteBuffer(batchData->vboPos); gfx::DeleteBuffer(batchData->vboNor);
    gfx::DeleteBuffer(batchData->vboCol); gfx::DeleteBuffer(batchData->vboUV);
    gfx::DeleteBuffer(batchData->vboIdx);
    delete batchData; batchData = NULL;
}

// arma/valida el lote y lo dibuja. false = no aplica (dibujar todo normal).
bool Collection::BatchRender() {
    namespace gfx = w3dEngine;
    extern bool AnimIsGame; extern bool PlayAnimation;
    const bool playing = AnimIsGame && PlayAnimation;
    if (batch != 1 || !playing) { BatchFree(); return false; }

    // ---- membresia ACTUAL (barata: traversal sin trabajo de geometria) ----
    std::vector<Mesh*> meshes;
    BatchCollect(this, meshes);
    if (meshes.empty()) { BatchFree(); return false; }

    // el lote vigente sigue valiendo? (misma membresia, por SERIAL: no se
    // deref-erencia ningun puntero viejo). Cambio visibilidad/alta/baja -> rearmar.
    if (batchData) {
        bool equal = (batchData->members.size() == meshes.size());
        for (size_t i = 0; equal && i < meshes.size(); i++)
            equal = (batchData->members[i] == meshes[i]->serial);
        if (!equal) BatchFree();
    }

    // ---- ARMADO (una vez; ~decenas de ms como mucho, jamas por frame) ----
    if (!batchData) {
        batchData = new W3dStaticBatch();
        size_t nVerts = 0;
        // buckets por MATERIAL: indices acumulados por material, en orden de aparicion
        std::vector<Material*> order;                    // materiales en orden
        std::vector< std::vector<MeshIndex> > buckets;   // indices de cada uno
        std::vector<Mesh*> hosts;
        for (size_t mi = 0; mi < meshes.size(); mi++) {
            Mesh* m = meshes[mi];
            batchData->members.push_back(m->serial);
            if (nVerts + (size_t)m->vertexSize > kBatchMaxVerts) { batchData->included.push_back(0); continue; } // no entra: se dibuja normal
            batchData->included.push_back(1);
            // bake de vertices a WORLD (posicion) + normales rotadas (columnas de W
            // normalizadas: con escala uniforme es exacto; el contrato del lote)
            Matrix4 W; m->GetWorldMatrix(W);
            float c0[3] = { W.m[0], W.m[1], W.m[2] }, c1[3] = { W.m[4], W.m[5], W.m[6] }, c2[3] = { W.m[8], W.m[9], W.m[10] };
            float l0 = sqrtf(c0[0]*c0[0]+c0[1]*c0[1]+c0[2]*c0[2]); if (l0 < 1e-9f) l0 = 1.0f;
            float l1 = sqrtf(c1[0]*c1[0]+c1[1]*c1[1]+c1[2]*c1[2]); if (l1 < 1e-9f) l1 = 1.0f;
            float l2 = sqrtf(c2[0]*c2[0]+c2[1]*c2[1]+c2[2]*c2[2]); if (l2 < 1e-9f) l2 = 1.0f;
            const size_t base = nVerts;
            for (int v = 0; v < m->vertexSize; v++) {
                Vector3 p(m->vertex[v*3], m->vertex[v*3+1], m->vertex[v*3+2]);
                Vector3 w = W * p;
                batchData->pos.push_back(w.x); batchData->pos.push_back(w.y); batchData->pos.push_back(w.z);
                if (m->normals) {
                    float nx = m->normals[v*3]/127.0f, ny = m->normals[v*3+1]/127.0f, nz = m->normals[v*3+2]/127.0f;
                    float wx = (c0[0]*nx + c1[0]*ny + c2[0]*nz);
                    float wy = (c0[1]*nx + c1[1]*ny + c2[1]*nz);
                    float wz = (c0[2]*nx + c1[2]*ny + c2[2]*nz);
                    float wl = sqrtf(wx*wx + wy*wy + wz*wz); if (wl < 1e-9f) wl = 1.0f;
                    batchData->nor.push_back((GLbyte)(wx/wl*127.0f));
                    batchData->nor.push_back((GLbyte)(wy/wl*127.0f));
                    batchData->nor.push_back((GLbyte)(wz/wl*127.0f));
                } else { batchData->nor.push_back(0); batchData->nor.push_back(0); batchData->nor.push_back(127); }
                if (m->vertexColor) {
                    batchData->col.push_back(m->vertexColor[v*4]);   batchData->col.push_back(m->vertexColor[v*4+1]);
                    batchData->col.push_back(m->vertexColor[v*4+2]); batchData->col.push_back(m->vertexColor[v*4+3]);
                } else { batchData->col.push_back(255); batchData->col.push_back(255); batchData->col.push_back(255); batchData->col.push_back(255); }
                if (m->uv) { batchData->uvs.push_back(m->uv[v*2]); batchData->uvs.push_back(m->uv[v*2+1]); }
                else       { batchData->uvs.push_back(0.0f); batchData->uvs.push_back(0.0f); }
            }
            nVerts += (size_t)m->vertexSize;
            for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                const MaterialGroup& grp = m->materialsGroup[g];
                if (grp.indicesDrawnCount <= 0) continue;
                Material* mat = grp.material ? grp.material : MaterialDefault;
                size_t b = 0;
                for (; b < order.size(); b++) if (order[b] == mat) break;
                if (b == order.size()) { order.push_back(mat); buckets.push_back(std::vector<MeshIndex>()); hosts.push_back(m); }
                std::vector<MeshIndex>& bk = buckets[b];
                for (int i = 0; i < grp.indicesDrawnCount; i++)
                    bk.push_back((MeshIndex)(base + m->faces[grp.startDrawn + i]));
            }
        }
        // buckets -> IBO contiguo con un rango por material
        for (size_t b = 0; b < order.size(); b++) {
            W3dBatchRange r;
            r.mat = order[b]; r.host = hosts[b];
            r.start = (int)batchData->idx.size();
            r.count = (int)buckets[b].size();
            batchData->idx.insert(batchData->idx.end(), buckets[b].begin(), buckets[b].end());
            batchData->ranges.push_back(r);
        }
        // a GPU (una vez). Sin VBOs: quedan los arrays de cliente (fallback).
        if (gfx::VBOSupported() && !batchData->pos.empty() && !batchData->idx.empty()) {
            batchData->vboPos   = gfx::GenBuffer(); gfx::ArrayBufferData(batchData->vboPos, &batchData->pos[0], (int)(batchData->pos.size()*sizeof(GLfloat)));
            batchData->vboNor   = gfx::GenBuffer(); gfx::ArrayBufferData(batchData->vboNor, &batchData->nor[0], (int)batchData->nor.size());
            batchData->vboCol   = gfx::GenBuffer(); gfx::ArrayBufferData(batchData->vboCol, &batchData->col[0], (int)batchData->col.size());
            batchData->vboUV    = gfx::GenBuffer(); gfx::ArrayBufferData(batchData->vboUV,  &batchData->uvs[0], (int)(batchData->uvs.size()*sizeof(GLfloat)));
            batchData->vboIdx   = gfx::GenBuffer(); gfx::IndexBufferData(batchData->vboIdx, &batchData->idx[0], (int)(batchData->idx.size()*sizeof(MeshIndex)));
            batchData->uploaded = batchData->vboPos && batchData->vboIdx;
        }
    }
    // marcar las mallas HORNEADAS con el SELLO del pase: Mesh::RenderObject las
    // saltea SOLO este frame (ver w3dBatchStamp en Mesh.h). El pase del espejo
    // corre dentro del mismo pase -> mismo sello -> ahi tampoco se duplican (el
    // lote se re-dibuja mirrado cuando el espejo re-recorre esta Collection).
    for (size_t i = 0; i < meshes.size() && i < batchData->included.size(); i++)
        if (batchData->included[i]) meshes[i]->inStaticBatch = w3dBatchStamp;

    if (batchData->idx.empty()) return true;   // lote vacio: los hijos igual se dibujan (abajo)

    // ---- DIBUJO: las posiciones ya estan horneadas en MUNDO y la pila de
    // modelview ya trae vista (+ espejo, si es el pase del reflejo) + los
    // transforms de los ancestros y de ESTA Collection: se multiplica por la
    // INVERSA del world de la Collection para que quede exactamente
    // vista (x espejo) x mundo. Con la Collection en identidad es un no-op;
    // con transform propio, el lote sale igual que las mallas sueltas. ----
    Matrix4 W; GetWorldMatrix(W);
    Matrix4 invW;
    if (!Matrix4::InvertAffine(W, invW)) invW = W.Inverse();
    gfx::MatrixMode(gfx::ModelView);
    gfx::PushMatrix();
    gfx::MultMatrix(invW.m);
    const bool solid     = w3dRenderSolid;
    const bool withLight = !w3dRenderNoLight;
    for (size_t r = 0; r < batchData->ranges.size(); r++) {
        const W3dBatchRange& rg = batchData->ranges[r];
        Material* mat = (solid || !rg.mat) ? MaterialDefault : rg.mat;
        rg.host->ApplyMaterial(mat, withLight, solid, false);
        // ApplyMaterial dejo los PUNTEROS de la malla host: pisarlos con los del lote
        if (batchData->uploaded) {
            gfx::VertexVBO(batchData->vboPos);
            gfx::NormalVBO(batchData->vboNor);
            gfx::ColorVBO(batchData->vboCol);
            gfx::TexCoordVBO(batchData->vboUV);
            gfx::BindIndexVBO(batchData->vboIdx);
            gfx::DrawTrianglesVBO(rg.count, rg.start);
        } else {
            gfx::VertexPointer3f(0, &batchData->pos[0]);
            gfx::NormalPointer3b(&batchData->nor[0]);
            gfx::ColorPointer4ub(&batchData->col[0]);
            gfx::TexCoordPointer2f(0, &batchData->uvs[0]);
            gfx::DrawTriangles(rg.count, &batchData->idx[rg.start]);
        }
    }
    if (batchData->uploaded) gfx::UnbindVBOs();
    gfx::PopMatrix();
    return true;
}

void Collection::RenderChildren() {
    // ---- LOTE ESTATICO (P4): jugando, dibuja el escenario horneado en pocos
    // draws y deja que el resto del subarbol se dibuje normal (las mallas
    // horneadas se saltean solas via inStaticBatch). ----
    if (batch == 1) {
        BatchRender();                 // arma/valida/dibuja (o libera si no aplica)
        Object::RenderChildren();      // el resto (y todo, si el lote no aplico)
        return;
    } else if (batchData) {
        BatchFree();              // el flag se apago: volver al camino normal
    }
    // "recalcula al TOCAR cualquiera de los dos checkboxes": se detecta aca (y no
    // en el panel) para que valga igual desde lua, la carga del .w3d o el undo.
    if (sortByCamera != lastByCamera || sortOnce != lastOnce) {
        lastByCamera = sortByCamera;
        lastOnce     = sortOnce;
        orderValid   = false;
        frozenOrder.clear();
    }
    if (!sortByCamera || Childrens.empty()) {
        Object::RenderChildren();   // orden del arbol, como siempre
        return;
    }

    if (sortOnce) {
        // ---- orden CONGELADO con la camara ACTIVA del juego: se calcula una vez
        // y despues se dibuja SIEMPRE igual (orbitar el viewport lo deja ver).
        // Revalidacion por MEMBRESIA: si un hijo entro/salio del arbol se
        // recalcula (nunca queda un puntero viejo). O(n^2) sobre los hijos
        // directos nomas: para el caso de uso (decenas de frutas) no pesa. ----
        bool valid = orderValid && frozenOrder.size() == Childrens.size();
        for (size_t i = 0; valid && i < frozenOrder.size(); i++) {
            bool is = false;
            for (size_t c = 0; c < Childrens.size() && !is; c++)
                is = (Childrens[c] == frozenOrder[i]);
            if (!is) valid = false;
        }
        if (!valid) {
            // sin Camera activa cae a la vista que dibuja (mejor eso que no ordenar)
            Vector3 cam = CameraActive ? CameraActive->GetGlobalPosition() : g_renderCamPos;
            std::vector<FarChild> order;
            SortFarNear(Childrens, cam, order);
            frozenOrder.clear();
            frozenOrder.reserve(order.size());
            for (size_t i = 0; i < order.size(); i++) frozenOrder.push_back(order[i].o);
            orderValid = true;
        }
        for (size_t i = 0; i < frozenOrder.size(); i++) frozenOrder[i]->Render();
        return;
    }

    // ---- orden POR FRAME: lejos -> cerca respecto de la vista QUE DIBUJA
    // (g_renderCamPos: el viewport en el editor, la camara activa en el juego).
    // Orden LOCAL del render de este frame: Childrens no se toca (mismo criterio
    // que Culling::RenderChildren). ----
    std::vector<FarChild> order;
    SortFarNear(Childrens, g_renderCamPos, order);
    for (size_t i = 0; i < order.size(); i++) order[i].o->Render();
}

// ---------------------------------------------------------------------------
//  setLote(coleccion, "estatico"|"off") de lua: hook instalado aca (el bind
//  vive en BindsGame.cpp, que no conoce esta clase; sin instalar = no-op).
// ---------------------------------------------------------------------------
extern bool (*W3dSetBatchHook)(Object*, const char*);
static bool SetObjBatch(Object* o, const char* mode) {
    if (!o || o->getType() != ObjectType::collection) return false;
    ((Collection*)o)->batch = (mode && std::string(mode) == "static") ? 1 : 0;
    return true;
}
namespace { struct BatchHookReg { BatchHookReg() { W3dSetBatchHook = SetObjBatch; } } gBatchHookReg; }

// Destructor
Collection::~Collection() {
    // liberar los buffers del lote SIN tocar mallas (el arbol puede estar
    // destruyendose: BatchFree recorre hijos y aca ya no es seguro)
    if (batchData) {
        namespace gfx = w3dEngine;
        gfx::DeleteBuffer(batchData->vboPos); gfx::DeleteBuffer(batchData->vboNor);
        gfx::DeleteBuffer(batchData->vboCol); gfx::DeleteBuffer(batchData->vboUV);
        gfx::DeleteBuffer(batchData->vboIdx);
        delete batchData; batchData = NULL;
    }
}
