#include "Collection.h"
#include "W3dLang.h"   // el nombre por defecto nace en el idioma del usuario
#include "Camera.h"              // CameraActive: la lente de "ordenarUnaVez"
#include "objects/CameraBase.h"  // g_renderCamPos/Rot: la vista que esta dibujando
#include "objects/Mesh.h"        // lote estatico: hornea mallas (P4)
#include "objects/Materials.h"   // materiales simples (elegibilidad del lote)
#include "w3dGraphics.h"         // buffers + draws del lote
#include <algorithm>   // std::sort (orden lejos -> cerca)
#include <math.h>      // sqrtf (normalizar la rotacion del bake de normales)

// Constructor
Collection::Collection(Object* parent, Vector3 pos)
    : Object(parent, T("Collection"), pos){
    ordenarPorCamara = false;
    ordenarUnaVez    = false;
    ordenValido      = false;
    ultimoPorCamara  = false;
    ultimoUnaVez     = false;
    lote             = 0;      // off: comportamiento de siempre
    loteData         = NULL;
}

// Método getType
ObjectType Collection::getType() {
    return ObjectType::collection;
}

// hijo + su distancia a la camara. El orden es LEJOS -> CERCA (dist2 DESC): el
// unico valido para transparentes (el blend compone de atras hacia adelante).
// La distancia sale del ORIGEN del hijo en mundo (GetGlobalPosition): barato y
// suficiente para objetos chicos tipo fruta (no se mide el AABB como en Culling).
struct HijoLejano {
    Object* o;
    float   dist2;
    bool operator<(const HijoLejano& b) const { return dist2 > b.dist2; }
};

static void OrdenarLejosCerca(const std::vector<Object*>& hijos, const Vector3& cam,
                              std::vector<HijoLejano>& orden) {
    orden.clear();
    orden.reserve(hijos.size());
    for (size_t c = 0; c < hijos.size(); c++) {
        HijoLejano h;
        h.o = hijos[c];
        Vector3 d = hijos[c]->GetGlobalPosition() - cam;
        h.dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
        orden.push_back(h);
    }
    std::sort(orden.begin(), orden.end());
}

// ===========================================================================
//  LOTE ESTATICO (P4) — ver Collection.h. El armado y el dibujo viven aca.
// ===========================================================================
struct W3dLoteRango {
    Material* mat;      // el material del rango (bucket por puntero: materiales
                        // COMPARTIDOS entre trozos se fusionan en un solo draw)
    Mesh*     host;     // una malla que usa ese material (presta AplicarMaterial)
    int       start;    // primer indice en idx[]
    int       count;    // cantidad de indices
};
struct W3dLoteEstatico {
    std::vector<GLfloat>  pos;    // world ya aplicado
    std::vector<GLbyte>   nor;    // rotadas a world (normalizadas por columna)
    std::vector<GLubyte>  col;    // 255 donde la malla no traia color
    std::vector<GLfloat>  uvs;    // 0 donde no traia uv
    std::vector<MeshIndex> idx;   // rangos por material, contiguos
    std::vector<W3dLoteRango> rangos;
    std::vector<unsigned> miembros;  // seriales de TODO el scan elegible (revalida membresia)
    std::vector<char>     incluida;  // paralela a miembros: 1 = horneada en el lote
                                     // (0 = no entro por el tope de vertices: se dibuja normal)
    unsigned vboPos, vboNor, vboCol, vboUV, vboIdx;
    bool subido;
    W3dLoteEstatico() : vboPos(0), vboNor(0), vboCol(0), vboUV(0), vboIdx(0), subido(false) {}
};

// tope PORTABLE de vertices por lote: MeshIndex es de 16 bits en el hardware
// chico (crossplatform.h). Lo que no entra queda AFUERA del lote (se dibuja
// normal): nunca indices invalidos.
static const size_t kLoteMaxVerts = 65500;

// una malla es HORNEABLE si es estatica y todos sus materiales son simples
// (ver Collection.h). Conservador a proposito: ante la duda, se dibuja normal.
static bool LoteElegible(Mesh* m) {
    if (!m || !m->visible || !m->renderizable) return false;
    if (m->skinArmature || !m->modificadores.empty() || m->genValido) return false;
    if (m->uvAnim || m->pvsFaces) return false;
    if (!m->vertex || m->vertexSize <= 0 || !m->faces || m->facesSize < 3) return false;
    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
        Material* mt = m->materialsGroup[g].material;
        if (!mt) continue;   // material default: simple
        if (mt->transparent || mt->chrome || mt->normalMap || !mt->capas.empty()) return false;
        if (mt->orden_pasada != 0 || mt->depth_bias != 0.0f) return false;
        if (!mt->depth_test || !mt->depth_write) return false;
    }
    return true;
}

// junta las mallas horneables del subarbol (respetando visible de la cadena)
static void LoteJuntar(Object* raiz, std::vector<Mesh*>& out) {
    std::vector<Object*> st; st.push_back(raiz);
    while (!st.empty()) {
        Object* o = st.back(); st.pop_back();
        if (!o->visible) continue;
        if (o->getType() == ObjectType::mesh && LoteElegible((Mesh*)o)) out.push_back((Mesh*)o);
        for (size_t i = 0; i < o->Childrens.size(); i++) st.push_back(o->Childrens[i]);
    }
}

void Collection::LoteLiberar() {
    if (!loteData) return;
    namespace gfx = w3dEngine;
    // las mallas no se "destapan": el sello (enLoteEstatico == w3dLoteStamp) solo
    // vale el frame en que el lote las marco; sin lote nadie las marca y vuelven
    // a dibujarse solas. Cero bookkeeping, cero punteros viejos.
    gfx::DeleteBuffer(loteData->vboPos); gfx::DeleteBuffer(loteData->vboNor);
    gfx::DeleteBuffer(loteData->vboCol); gfx::DeleteBuffer(loteData->vboUV);
    gfx::DeleteBuffer(loteData->vboIdx);
    delete loteData; loteData = NULL;
}

// arma/valida el lote y lo dibuja. false = no aplica (dibujar todo normal).
bool Collection::LoteRender() {
    namespace gfx = w3dEngine;
    extern bool AnimEsJuego; extern bool PlayAnimation;
    const bool jugando = AnimEsJuego && PlayAnimation;
    if (lote != 1 || !jugando) { LoteLiberar(); return false; }

    // ---- membresia ACTUAL (barata: traversal sin trabajo de geometria) ----
    std::vector<Mesh*> mallas;
    LoteJuntar(this, mallas);
    if (mallas.empty()) { LoteLiberar(); return false; }

    // el lote vigente sigue valiendo? (misma membresia, por SERIAL: no se
    // deref-erencia ningun puntero viejo). Cambio visibilidad/alta/baja -> rearmar.
    if (loteData) {
        bool igual = (loteData->miembros.size() == mallas.size());
        for (size_t i = 0; igual && i < mallas.size(); i++)
            igual = (loteData->miembros[i] == mallas[i]->serial);
        if (!igual) LoteLiberar();
    }

    // ---- ARMADO (una vez; ~decenas de ms como mucho, jamas por frame) ----
    if (!loteData) {
        loteData = new W3dLoteEstatico();
        size_t nVerts = 0;
        // buckets por MATERIAL: indices acumulados por material, en orden de aparicion
        std::vector<Material*> orden;                    // materiales en orden
        std::vector< std::vector<MeshIndex> > buckets;   // indices de cada uno
        std::vector<Mesh*> hosts;
        for (size_t mi = 0; mi < mallas.size(); mi++) {
            Mesh* m = mallas[mi];
            loteData->miembros.push_back(m->serial);
            if (nVerts + (size_t)m->vertexSize > kLoteMaxVerts) { loteData->incluida.push_back(0); continue; } // no entra: se dibuja normal
            loteData->incluida.push_back(1);
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
                loteData->pos.push_back(w.x); loteData->pos.push_back(w.y); loteData->pos.push_back(w.z);
                if (m->normals) {
                    float nx = m->normals[v*3]/127.0f, ny = m->normals[v*3+1]/127.0f, nz = m->normals[v*3+2]/127.0f;
                    float wx = (c0[0]*nx + c1[0]*ny + c2[0]*nz);
                    float wy = (c0[1]*nx + c1[1]*ny + c2[1]*nz);
                    float wz = (c0[2]*nx + c1[2]*ny + c2[2]*nz);
                    float wl = sqrtf(wx*wx + wy*wy + wz*wz); if (wl < 1e-9f) wl = 1.0f;
                    loteData->nor.push_back((GLbyte)(wx/wl*127.0f));
                    loteData->nor.push_back((GLbyte)(wy/wl*127.0f));
                    loteData->nor.push_back((GLbyte)(wz/wl*127.0f));
                } else { loteData->nor.push_back(0); loteData->nor.push_back(0); loteData->nor.push_back(127); }
                if (m->vertexColor) {
                    loteData->col.push_back(m->vertexColor[v*4]);   loteData->col.push_back(m->vertexColor[v*4+1]);
                    loteData->col.push_back(m->vertexColor[v*4+2]); loteData->col.push_back(m->vertexColor[v*4+3]);
                } else { loteData->col.push_back(255); loteData->col.push_back(255); loteData->col.push_back(255); loteData->col.push_back(255); }
                if (m->uv) { loteData->uvs.push_back(m->uv[v*2]); loteData->uvs.push_back(m->uv[v*2+1]); }
                else       { loteData->uvs.push_back(0.0f); loteData->uvs.push_back(0.0f); }
            }
            nVerts += (size_t)m->vertexSize;
            for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                const MaterialGroup& grp = m->materialsGroup[g];
                if (grp.indicesDrawnCount <= 0) continue;
                Material* mat = grp.material ? grp.material : MaterialDefecto;
                size_t b = 0;
                for (; b < orden.size(); b++) if (orden[b] == mat) break;
                if (b == orden.size()) { orden.push_back(mat); buckets.push_back(std::vector<MeshIndex>()); hosts.push_back(m); }
                std::vector<MeshIndex>& bk = buckets[b];
                for (int i = 0; i < grp.indicesDrawnCount; i++)
                    bk.push_back((MeshIndex)(base + m->faces[grp.startDrawn + i]));
            }
        }
        // buckets -> IBO contiguo con un rango por material
        for (size_t b = 0; b < orden.size(); b++) {
            W3dLoteRango r;
            r.mat = orden[b]; r.host = hosts[b];
            r.start = (int)loteData->idx.size();
            r.count = (int)buckets[b].size();
            loteData->idx.insert(loteData->idx.end(), buckets[b].begin(), buckets[b].end());
            loteData->rangos.push_back(r);
        }
        // a GPU (una vez). Sin VBOs: quedan los arrays de cliente (fallback).
        if (gfx::VBOSoportado() && !loteData->pos.empty() && !loteData->idx.empty()) {
            loteData->vboPos = gfx::GenBuffer(); gfx::ArrayBufferData(loteData->vboPos, &loteData->pos[0], (int)(loteData->pos.size()*sizeof(GLfloat)));
            loteData->vboNor = gfx::GenBuffer(); gfx::ArrayBufferData(loteData->vboNor, &loteData->nor[0], (int)loteData->nor.size());
            loteData->vboCol = gfx::GenBuffer(); gfx::ArrayBufferData(loteData->vboCol, &loteData->col[0], (int)loteData->col.size());
            loteData->vboUV  = gfx::GenBuffer(); gfx::ArrayBufferData(loteData->vboUV,  &loteData->uvs[0], (int)(loteData->uvs.size()*sizeof(GLfloat)));
            loteData->vboIdx = gfx::GenBuffer(); gfx::IndexBufferData(loteData->vboIdx, &loteData->idx[0], (int)(loteData->idx.size()*sizeof(MeshIndex)));
            loteData->subido = loteData->vboPos && loteData->vboIdx;
        }
    }
    // marcar las mallas HORNEADAS con el SELLO del pase: Mesh::RenderObject las
    // saltea SOLO este frame (ver w3dLoteStamp en Mesh.h). El pase del espejo
    // corre dentro del mismo pase -> mismo sello -> ahi tampoco se duplican (el
    // lote se re-dibuja mirrado cuando el espejo re-recorre esta Collection).
    for (size_t i = 0; i < mallas.size() && i < loteData->incluida.size(); i++)
        if (loteData->incluida[i]) mallas[i]->enLoteEstatico = w3dLoteStamp;

    if (loteData->idx.empty()) return true;   // lote vacio: los hijos igual se dibujan (abajo)

    // ---- DIBUJO: las posiciones ya estan horneadas en MUNDO y la pila de
    // modelview ya trae vista (+ espejo, si es el pase del reflejo) + los
    // transforms de los ancestros y de ESTA Collection: se multiplica por la
    // INVERSA del world de la Collection para que quede exactamente
    // vista (x espejo) x mundo. Con la Collection en identidad es un no-op;
    // con transform propio, el lote sale igual que las mallas sueltas. ----
    Matrix4 W; GetWorldMatrix(W);
    Matrix4 invW;
    if (!Matrix4::InvertirAfin(W, invW)) invW = W.Inverse();
    gfx::MatrixMode(gfx::ModelView);
    gfx::PushMatrix();
    gfx::MultMatrix(invW.m);
    const bool solido = w3dRenderSolido;
    const bool conLuz = !w3dRenderSinLuz;
    for (size_t r = 0; r < loteData->rangos.size(); r++) {
        const W3dLoteRango& rg = loteData->rangos[r];
        Material* mat = (solido || !rg.mat) ? MaterialDefecto : rg.mat;
        rg.host->AplicarMaterial(mat, conLuz, solido, false);
        // AplicarMaterial dejo los PUNTEROS de la malla host: pisarlos con los del lote
        if (loteData->subido) {
            gfx::VertexVBO(loteData->vboPos);
            gfx::NormalVBO(loteData->vboNor);
            gfx::ColorVBO(loteData->vboCol);
            gfx::TexCoordVBO(loteData->vboUV);
            gfx::BindIndexVBO(loteData->vboIdx);
            gfx::DrawTrianglesVBO(rg.count, rg.start);
        } else {
            gfx::VertexPointer3f(0, &loteData->pos[0]);
            gfx::NormalPointer3b(&loteData->nor[0]);
            gfx::ColorPointer4ub(&loteData->col[0]);
            gfx::TexCoordPointer2f(0, &loteData->uvs[0]);
            gfx::DrawTriangles(rg.count, &loteData->idx[rg.start]);
        }
    }
    if (loteData->subido) gfx::UnbindVBOs();
    gfx::PopMatrix();
    return true;
}

void Collection::RenderHijos() {
    // ---- LOTE ESTATICO (P4): jugando, dibuja el escenario horneado en pocos
    // draws y deja que el resto del subarbol se dibuje normal (las mallas
    // horneadas se saltean solas via enLoteEstatico). ----
    if (lote == 1) {
        LoteRender();               // arma/valida/dibuja (o libera si no aplica)
        Object::RenderHijos();      // el resto (y todo, si el lote no aplico)
        return;
    } else if (loteData) {
        LoteLiberar();              // el flag se apago: volver al camino normal
    }
    // "recalcula al TOCAR cualquiera de los dos checkboxes": se detecta aca (y no
    // en el panel) para que valga igual desde lua, la carga del .w3d o el undo.
    if (ordenarPorCamara != ultimoPorCamara || ordenarUnaVez != ultimoUnaVez) {
        ultimoPorCamara = ordenarPorCamara;
        ultimoUnaVez    = ordenarUnaVez;
        ordenValido     = false;
        ordenCongelado.clear();
    }
    if (!ordenarPorCamara || Childrens.empty()) {
        Object::RenderHijos();   // orden del arbol, como siempre
        return;
    }

    if (ordenarUnaVez) {
        // ---- orden CONGELADO con la camara ACTIVA del juego: se calcula una vez
        // y despues se dibuja SIEMPRE igual (orbitar el viewport lo deja ver).
        // Revalidacion por MEMBRESIA: si un hijo entro/salio del arbol se
        // recalcula (nunca queda un puntero viejo). O(n^2) sobre los hijos
        // directos nomas: para el caso de uso (decenas de frutas) no pesa. ----
        bool valido = ordenValido && ordenCongelado.size() == Childrens.size();
        for (size_t i = 0; valido && i < ordenCongelado.size(); i++) {
            bool esta = false;
            for (size_t c = 0; c < Childrens.size() && !esta; c++)
                esta = (Childrens[c] == ordenCongelado[i]);
            if (!esta) valido = false;
        }
        if (!valido) {
            // sin Camera activa cae a la vista que dibuja (mejor eso que no ordenar)
            Vector3 cam = CameraActive ? CameraActive->GetGlobalPosition() : g_renderCamPos;
            std::vector<HijoLejano> orden;
            OrdenarLejosCerca(Childrens, cam, orden);
            ordenCongelado.clear();
            ordenCongelado.reserve(orden.size());
            for (size_t i = 0; i < orden.size(); i++) ordenCongelado.push_back(orden[i].o);
            ordenValido = true;
        }
        for (size_t i = 0; i < ordenCongelado.size(); i++) ordenCongelado[i]->Render();
        return;
    }

    // ---- orden POR FRAME: lejos -> cerca respecto de la vista QUE DIBUJA
    // (g_renderCamPos: el viewport en el editor, la camara activa en el juego).
    // Orden LOCAL del render de este frame: Childrens no se toca (mismo criterio
    // que Culling::RenderHijos). ----
    std::vector<HijoLejano> orden;
    OrdenarLejosCerca(Childrens, g_renderCamPos, orden);
    for (size_t i = 0; i < orden.size(); i++) orden[i].o->Render();
}

// ---------------------------------------------------------------------------
//  setLote(coleccion, "estatico"|"off") de lua: hook instalado aca (el bind
//  vive en BindsJuego.cpp, que no conoce esta clase; sin instalar = no-op).
// ---------------------------------------------------------------------------
extern bool (*W3dSetLoteHook)(Object*, const char*);
static bool SetLoteDeObj(Object* o, const char* modo) {
    if (!o || o->getType() != ObjectType::collection) return false;
    ((Collection*)o)->lote = (modo && std::string(modo) == "estatico") ? 1 : 0;
    return true;
}
namespace { struct LoteHookReg { LoteHookReg() { W3dSetLoteHook = SetLoteDeObj; } } gLoteHookReg; }

// Destructor
Collection::~Collection() {
    // liberar los buffers del lote SIN tocar mallas (el arbol puede estar
    // destruyendose: LoteLiberar recorre hijos y aca ya no es seguro)
    if (loteData) {
        namespace gfx = w3dEngine;
        gfx::DeleteBuffer(loteData->vboPos); gfx::DeleteBuffer(loteData->vboNor);
        gfx::DeleteBuffer(loteData->vboCol); gfx::DeleteBuffer(loteData->vboUV);
        gfx::DeleteBuffer(loteData->vboIdx);
        delete loteData; loteData = NULL;
    }
}
