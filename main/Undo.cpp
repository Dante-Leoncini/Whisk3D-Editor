#include "Undo.h"
#include "variables.h"           // InteractionMode, ObjActivo, ObjSelects
#include "objects/Objects.h"     // Object (pos/rot/scale)
#include "objects/Mesh.h"        // Mesh (edit move: vertex/normals/CalcularBordes)
#include "objects/EditMesh.h"    // EditMesh (seleccion de sub-elementos: vertSel/edgeSel/faceSel)
#include "objects/ObjectMode.h"
#include "objects/Light.h"       // Lights (global): el borrado de luces lo des/re-registra
#include "edit/Modifier.h"       // Modifier (limpiar target de Armature/Mirror/Boolean al borrar el objeto apuntado)
#include "objects/Armature.h"    // Armature (cast correcto Object*<->Armature* al limpiar/restaurar skinArmature)
#include "animation/SkeletalAnimation.h" // KeyframesUndo: recorrer las curvas del clip activo (tracks/Propertys)
#include "animation/Animation.h"         // AnimationObjects / keyFrame / ActiveAnimKind
// CameraActive: NO incluyo Camera.h (header pesado del editor, arrastra Target/Curve/icons -> riesgo en el
// build de Symbian). Forward-declaro: solo necesito el puntero (Object es la 1ra base -> el cast a Object* es offset 0).
class Camera; extern Camera* CameraActive;
#include <vector>
#include <set>
#include <string>
// SceneCollection (raiz de la escena) viene de objects/Objects.h

extern void ActualizarEditMeshActivo(); // LayoutInput.cpp (refresca g_editMesh al cambiar de modo)
extern void InvalidarSkinDeArmature(Armature* a); // ObjectMode.cpp (libera el cache de vertex-anim de las mallas del rig)

// ============================================================================
//  Comandos de UNDO/REDO. Cada comando guarda UN snapshot del estado y su
//  Aplicar() INTERCAMBIA el guardado con el vivo (swap). Asi el MISMO Aplicar()
//  sirve para deshacer (vivo=nuevo -> guardado) y para rehacer (vivo=viejo ->
//  guardado de nuevo): es un toggle. Deshacer pasa el comando del stack de undo
//  al de redo; rehacer al reves. Una accion NUEVA (Push) vacia el stack de redo.
// ============================================================================
class UndoCmd {
public:
    virtual ~UndoCmd() {}
    virtual void Aplicar() = 0; // intercambia el estado guardado con el vivo (undo Y redo)
};

// cambio de modo Edit/Object
class ModeUndo : public UndoCmd {
    int modo;
public:
    ModeUndo(int m) : modo(m) {}
    void Aplicar() { int cur = InteractionMode; InteractionMode = modo; modo = cur; ActualizarEditMeshActivo(); }
};

// rename (objeto/material/uv/color): el destino es un std::string* estable (el objeto/capa vive)
class RenameUndo : public UndoCmd {
    std::string* t;
    std::string  guardado;
public:
    RenameUndo(std::string* T) : t(T), guardado(T ? *T : std::string()) {}
    void Aplicar() { if (!t) return; std::string cur = *t; *t = guardado; guardado = cur; }
};

// seleccion de objetos: la lista seleccionada + el activo
class SelectUndo : public UndoCmd {
    std::vector<Object*> sel;
    Object* activo;
public:
    SelectUndo() { sel = ObjSelects; activo = ObjActivo; }
    void Aplicar() {
        std::vector<Object*> curSel = ObjSelects; Object* curAct = ObjActivo; // estado vivo
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = false;
        ObjSelects = sel; ObjActivo = activo;
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = true;
        sel = curSel; activo = curAct; // guarda lo que estaba vivo (para rehacer)
    }
};

// transform en OBJECT MODE: pos/rot/escala de los seleccionados al EMPEZAR
// rotEuler va aparte del quaternion: el quaternion no distingue 0 de 360, asi que sin guardarlo el undo de un
// giro de vuelta entera te devolvia la orientacion pero te comia las vueltas (y con ellas la animacion).
struct TEst { Object* o; Vector3 pos; Quaternion rot; Vector3 rotEuler; Vector3 scale; };
class TransformUndo : public UndoCmd {
    std::vector<TEst> e;
public:
    TransformUndo() {
        for (size_t i = 0; i < ObjSelects.size(); i++) {
            Object* o = ObjSelects[i]; if (!o) continue;
            TEst t; t.o = o; t.pos = o->pos; t.rot = o->rot; t.rotEuler = o->rotEuler; t.scale = o->scale;
            e.push_back(t);
        }
    }
    bool Vacio() const { return e.empty(); }
    void Aplicar() {
        for (size_t i = 0; i < e.size(); i++) {
            Object* o = e[i].o; if (!o) continue;
            Vector3 cp = o->pos; Quaternion cr = o->rot; Vector3 ce = o->rotEuler; Vector3 cs = o->scale; // vivo
            o->pos = e[i].pos; o->rot = e[i].rot; o->rotEuler = e[i].rotEuler; o->scale = e[i].scale;
            e[i].pos = cp; e[i].rot = cr; e[i].rotEuler = ce; e[i].scale = cs; // guarda lo vivo
            o->ActualizarDisplayRot(); // rotAngle/rotAxis del panel (el euler ya quedo restaurado, con sus vueltas)
        }
    }
};

// mover verts/aristas/caras en EDIT MODE (move PURO): intercambia las posiciones+normales de la malla.
class EditMoveUndo : public UndoCmd {
    Mesh* m;
    std::vector<GLfloat> vertex;
    std::vector<GLbyte>  normals;
public:
    EditMoveUndo(Mesh* M) : m(M) {
        if (m && m->vertex  && m->vertexSize > 0) vertex.assign(m->vertex,  m->vertex  + m->vertexSize * 3);
        if (m && m->normals && m->vertexSize > 0) normals.assign(m->normals, m->normals + m->vertexSize * 3);
    }
    bool Vacio() const { return vertex.empty(); }
    void Aplicar() {
        if (!m) return;
        if (m->vertex  && (int)vertex.size()  == m->vertexSize * 3) for (size_t i = 0; i < vertex.size();  i++) { GLfloat c = m->vertex[i];  m->vertex[i]  = vertex[i];  vertex[i]  = c; }
        if (m->normals && (int)normals.size() == m->vertexSize * 3) for (size_t i = 0; i < normals.size(); i++) { GLbyte  c = m->normals[i]; m->normals[i] = normals[i]; normals[i] = c; }
        // move PURO = NO cambia la topologia -> CalcularBordes(false) CONSERVA la edit mesh (no la rebuildea)
        // asi NO se pierde la SELECCION; SincronizarPos re-lee las posiciones restauradas al display del edit.
        m->CalcularBordes(false);
        if (m->edit) m->edit->SincronizarPos();
    }
};

// SELECCION de sub-elementos en EDIT MODE (verts/edges/faces): intercambia los 3 vectores de seleccion
// + el activo de la EditMesh (solo si el size matchea -> robusto al rearmado de la edit).
class SelectEditUndo : public UndoCmd {
    Mesh* m;
    std::vector<unsigned char> vs, es, fs;
    int activo;
public:
    SelectEditUndo(Mesh* M) : m(M), activo(-1) {
        if (m) { m->EnsureEdit();
                 if (m->edit) { vs = m->edit->vertSel; es = m->edit->edgeSel; fs = m->edit->faceSel; activo = m->edit->activeIdx; } }
    }
    void Aplicar() {
        if (!m) return; m->EnsureEdit(); if (!m->edit) return;
        if (m->edit->vertSel.size() == vs.size()) m->edit->vertSel.swap(vs);
        if (m->edit->edgeSel.size() == es.size()) m->edit->edgeSel.swap(es);
        if (m->edit->faceSel.size() == fs.size()) m->edit->faceSel.swap(fs);
        int cur = m->edit->activeIdx; m->edit->activeIdx = activo; activo = cur;
        m->edit->Recolorear(); // re-tinta los buffers segun la seleccion restaurada
    }
};

// cambiar el MATERIAL de un mesh part (AccionMaterialElegido): intercambia el Material* del mesh part
class MaterialUndo : public UndoCmd {
    Mesh* m; int idx; Material* guardado;
public:
    MaterialUndo(Mesh* M, int i) : m(M), idx(i),
        guardado((M && i >= 0 && i < (int)M->materialsGroup.size()) ? M->materialsGroup[i].material : NULL) {}
    void Aplicar() {
        if (m && idx >= 0 && idx < (int)m->materialsGroup.size()) {
            Material* cur = m->materialsGroup[idx].material;
            m->materialsGroup[idx].material = guardado; guardado = cur;
        }
    }
};

// snapshot COMPLETO de la geometria de la malla (topologia): extrude / delete-edit / loop-cut / duplicate /
// assign mesh part. Clona TODOS los arrays de render + faces3d + materialsGroup + las capas (uv/color/grupos).
// Aplicar() = SWAP: snapshotea la geo viva, escribe la guardada, y se queda con la que estaba viva (redo).
class MeshGeoUndo : public UndoCmd {
    Mesh* m;
    int vertexSize, facesSize;
    std::vector<GLfloat>  vertex, uv;
    std::vector<GLbyte>   normals;
    std::vector<GLubyte>  color;
    std::vector<MeshIndex> faces;
    std::vector<MeshFace> faces3d;
    std::vector<int>      looseEdges;
    std::vector<int>      looseVerts;
    std::vector<MaterialGroup> materialsGroup;
    std::vector<UVMap>       uvMaps;       int uvMapActivo;
    std::vector<ColorLayer>  colorLayers;  int colorActivo;
    std::vector<VertexGroup> vertexGroups; int grupoActivo;
    std::vector<int>         vertCtrlPoint; int skinNCtrl; // SKINNING: mapeo render-vert -> control-point (sino el undo de una malla skinneada deja el mapeo viejo -> skin roto)
    std::set<std::string>    sharpEdges, seamEdges; // bordes sharp/seam (por POSICION). meshSmooth = shading
    bool                     meshSmooth;

    void CapturarDe(Mesh* s) { // llena los miembros desde la malla viva
        vertexSize = s->vertexSize; facesSize = s->facesSize;
        sharpEdges = s->sharpEdges; seamEdges = s->seamEdges; meshSmooth = s->meshSmooth;
        vertex.clear(); normals.clear(); uv.clear(); color.clear(); faces.clear();
        uvMaps.clear(); colorLayers.clear(); vertexGroups.clear();
        if (s->vertex)      vertex.assign(s->vertex, s->vertex + vertexSize * 3);
        if (s->normals)     normals.assign(s->normals, s->normals + vertexSize * 3);
        if (s->uv)          uv.assign(s->uv, s->uv + vertexSize * 2);
        if (s->vertexColor) color.assign(s->vertexColor, s->vertexColor + vertexSize * 4);
        if (s->faces)       faces.assign(s->faces, s->faces + facesSize);
        faces3d = s->faces3d; looseEdges = s->looseEdges; looseVerts = s->looseVerts; materialsGroup = s->materialsGroup;
        for (size_t i = 0; i < s->uvMaps.size(); i++)       uvMaps.push_back(*s->uvMaps[i]);
        for (size_t i = 0; i < s->colorLayers.size(); i++)  colorLayers.push_back(*s->colorLayers[i]);
        for (size_t i = 0; i < s->vertexGroups.size(); i++) vertexGroups.push_back(*s->vertexGroups[i]);
        uvMapActivo = s->uvMapActivo; colorActivo = s->colorActivo; grupoActivo = s->grupoActivo;
        vertCtrlPoint = s->vertCtrlPoint; skinNCtrl = s->skinNCtrl; // skinning: mapeo render->control-point
    }
    void AplicarA(Mesh* s) { // escribe los miembros (snapshot) a la malla viva
        delete[] s->vertex;      s->vertex = NULL;
        delete[] s->normals;     s->normals = NULL;
        delete[] s->uv;          s->uv = NULL;
        delete[] s->vertexColor; s->vertexColor = NULL;
        delete[] s->faces;       s->faces = NULL;
        s->vertexSize = vertexSize; s->facesSize = facesSize;
        if (!vertex.empty())  { s->vertex = new GLfloat[vertex.size()];        for (size_t i=0;i<vertex.size();i++)  s->vertex[i]      = vertex[i]; }
        if (!normals.empty()) { s->normals = new GLbyte[normals.size()];       for (size_t i=0;i<normals.size();i++) s->normals[i]     = normals[i]; }
        if (!uv.empty())      { s->uv = new GLfloat[uv.size()];                for (size_t i=0;i<uv.size();i++)      s->uv[i]          = uv[i]; }
        if (!color.empty())   { s->vertexColor = new GLubyte[color.size()];    for (size_t i=0;i<color.size();i++)   s->vertexColor[i] = color[i]; }
        if (!faces.empty())   { s->faces = new MeshIndex[faces.size()];         for (size_t i=0;i<faces.size();i++)   s->faces[i]       = faces[i]; }
        s->faces3d = faces3d; s->looseEdges = looseEdges; s->looseVerts = looseVerts; s->materialsGroup = materialsGroup;
        s->sharpEdges = sharpEdges; s->seamEdges = seamEdges; s->meshSmooth = meshSmooth;
        s->LiberarCapas();
        for (size_t i = 0; i < uvMaps.size(); i++)       s->uvMaps.push_back(new UVMap(uvMaps[i]));
        for (size_t i = 0; i < colorLayers.size(); i++)  s->colorLayers.push_back(new ColorLayer(colorLayers[i]));
        for (size_t i = 0; i < vertexGroups.size(); i++) s->vertexGroups.push_back(new VertexGroup(vertexGroups[i]));
        s->uvMapActivo = uvMapActivo; s->colorActivo = colorActivo; s->grupoActivo = grupoActivo;
        s->vertCtrlPoint = vertCtrlPoint; s->skinNCtrl = skinNCtrl; // restaurar el mapeo render->control-point (skinning)
        s->lastSkinFrame = -999999; // forzar re-skin con la geo/mapeo restaurados (CalcularBordes ya bumpea skinGeomVersion)
        s->CalcularBordes(); // recomputa edges/centroGeom + invalida el edit (se rearma de la geo restaurada)
    }
public:
    MeshGeoUndo(Mesh* M) : m(M), vertexSize(0), facesSize(0), uvMapActivo(0), colorActivo(0), grupoActivo(0), skinNCtrl(0) {
        if (m) CapturarDe(m);
    }
    void Aplicar() {
        if (!m) return;
        MeshGeoUndo cur(m);   // snapshotea la geo VIVA (la nueva)
        AplicarA(m);          // escribe la geo GUARDADA (la vieja) a la malla
        // queda con lo que estaba vivo (para rehacer): intercambia los buffers con cur
        vertexSize = cur.vertexSize; facesSize = cur.facesSize;
        vertex.swap(cur.vertex); normals.swap(cur.normals); uv.swap(cur.uv); color.swap(cur.color);
        faces.swap(cur.faces); faces3d.swap(cur.faces3d); looseEdges.swap(cur.looseEdges); looseVerts.swap(cur.looseVerts);
        materialsGroup.swap(cur.materialsGroup);
        uvMaps.swap(cur.uvMaps); colorLayers.swap(cur.colorLayers); vertexGroups.swap(cur.vertexGroups);
        uvMapActivo = cur.uvMapActivo; colorActivo = cur.colorActivo; grupoActivo = cur.grupoActivo;
        sharpEdges.swap(cur.sharpEdges); seamEdges.swap(cur.seamEdges); meshSmooth = cur.meshSmooth;
        // la geometria cambio -> refrescar el preview del modificador (subdivision/screw). Antes esto lo hacia el
        // regen POR FRAME de ActualizarEditMeshActivo; ahora que ese esta gateado, hay que pedirlo aca explicito.
        if (!m->modificadores.empty()) m->GenerarMallaModificada();
    }
};

// COLOR (RGBA) de un material/luz: intercambia los 4 floats del target. Lo usa el ColorPicker al cerrar.
class ColorUndo : public UndoCmd {
    GLfloat* t; GLfloat val[4];
public:
    ColorUndo(GLfloat* target, const GLfloat* viejo) : t(target) { for (int i=0;i<4;i++) val[i]=viejo[i]; }
    void Aplicar() { if (!t) return; for (int i=0;i<4;i++) { GLfloat c=t[i]; t[i]=val[i]; val[i]=c; } }
};

// MODIFICACION de un material (checkboxes + shininess; los COLORES van por ColorUndo). Snapshot SOLO de
// esos campos (NO name/texture/capas -> esos tienen su propio undo y no hay que revertirlos de mas).
class MaterialModUndo : public UndoCmd {
    Material* mat;
    bool b[10]; int rmode; float shin; // rmode = reflectMode (era el bool chromeEquirect, ahora int de 3 modos)
    static void Leer(Material* s, bool* bo, int& rm, float& sh) {
        bo[0]=s->textureOn; bo[1]=s->filtrado; bo[2]=s->transparent; bo[3]=s->vertexColor;
        bo[4]=s->lighting; bo[5]=s->repeat; bo[6]=s->uv8bit; bo[7]=s->culling;
        bo[8]=s->depth_test; bo[9]=s->chrome; rm=s->reflectMode; sh=s->shininess;
    }
    static void Escribir(Material* s, const bool* bo, int rm, float sh) {
        s->textureOn=bo[0]; s->filtrado=bo[1]; s->transparent=bo[2]; s->vertexColor=bo[3];
        s->lighting=bo[4]; s->repeat=bo[5]; s->uv8bit=bo[6]; s->culling=bo[7];
        s->depth_test=bo[8]; s->chrome=bo[9]; s->reflectMode=rm; s->shininess=sh;
    }
public:
    MaterialModUndo(Material* M) : mat(M), rmode(0), shin(0) { if (mat) Leer(mat, b, rmode, shin); }
    Material* Mat() const { return mat; }
    bool Difiere() const {
        if (!mat) return false;
        bool cb[11]; int crm; GLfloat cs; Leer(mat, cb, crm, cs);
        for (int i=0;i<11;i++) if (cb[i]!=b[i]) return true;
        return crm != rmode || cs != shin;
    }
    void Aplicar() {
        if (!mat) return;
        bool cb[11]; int crm; GLfloat cs; Leer(mat, cb, crm, cs);   // estado vivo
        Escribir(mat, b, rmode, shin);                              // restaura el guardado
        for (int i=0;i<11;i++) b[i]=cb[i]; rmode=crm; shin=cs;       // guarda lo vivo (para rehacer)
    }
};

// ============================================================================
//  BORRAR objetos (Ctrl+Z): los objetos NO se liberan al borrar -> se DETACHAN de la escena (se sacan de su
//  padre) y los GUARDA el comando, que los re-inserta al deshacer. El comando es DUEÑO mientras estan
//  detachados; si se cae del stack (destructor con enEscena=false) recien ahi los libera de verdad. Maneja
//  las LUCES (global Lights: las saca/agrega para que una luz borrada NO siga iluminando) y la CAMARA ACTIVA.
// ============================================================================
static void DetacharLuces(Object* o) { // saca del global Lights todas las luces del subarbol (dejan de iluminar)
    if (o->getType() == ObjectType::light)
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == (Light*)o) { Lights.erase(Lights.begin()+i); break; }
    for (size_t i = 0; i < o->Childrens.size(); i++) DetacharLuces(o->Childrens[i]);
}
static void ReattacharLuces(Object* o) { // re-registra las luces del subarbol (sin duplicar)
    if (o->getType() == ObjectType::light) {
        Light* l = (Light*)o; bool ya = false;
        for (size_t i = 0; i < Lights.size(); i++) if (Lights[i] == l) { ya = true; break; }
        if (!ya) Lights.push_back(l);
    }
    for (size_t i = 0; i < o->Childrens.size(); i++) ReattacharLuces(o->Childrens[i]);
}
static bool ContieneCamActiva(Object* o) {
    if (CameraActive && (Object*)CameraActive == o) return true;
    for (size_t i = 0; i < o->Childrens.size(); i++) if (ContieneCamActiva(o->Childrens[i])) return true;
    return false;
}
// el ESQUELETO cuyo clip esta activo (ActiveAnimArm/kind): si se borra, la seleccion de animacion queda colgada
// (el timeline seguia mostrando el clip borrado). Al detacharlo se vuelve a ESCENA (kind 0). El destructor del
// Armature tambien lo hace, pero el borrado DETACHA (no destruye, por el undo) -> hay que resetearlo aca.
extern int ActiveAnimKind; extern Armature* ActiveAnimArm;
static bool ContieneAnimActiva(Object* o) {
    if (ActiveAnimArm && (Object*)ActiveAnimArm == o) return true;
    for (size_t i = 0; i < o->Childrens.size(); i++) if (ContieneAnimActiva(o->Childrens[i])) return true;
    return false;
}

struct DelEntry { Object* obj; Object* parent; int index; };
// recolecta los "delete-roots": cada objeto SELECCIONADO y borrable (no-collection salvo incCol) cuyo
// ancestro NO es tambien un delete-root. El subarbol se va con su root (no se recursea adentro).
static void RecolectarBorrar(Object* node, bool incCol, std::vector<DelEntry>& out) {
    for (int i = (int)node->Childrens.size()-1; i >= 0; i--) { // alto->bajo: los indices quedan validos al sacar
        Object* c = node->Childrens[i];
        bool borrable = c->select && (incCol || c->getType() != ObjectType::collection);
        if (borrable) { DelEntry e; e.obj = c; e.parent = node; e.index = i; out.push_back(e); }
        else          RecolectarBorrar(c, incCol, out);
    }
}

// hijo que se REPARENTA al abuelo cuando se borra su padre (asi no desaparece de la escena; queda como hijo del
// abuelo preservando su posicion global, v1: solo traslacion, igual que W3dReparent).
struct RepEntry { Object* child; Object* abuelo; Object* borrado; Vector3 posBajoAbuelo; Vector3 posBajoBorrado; };

// referencia de una malla a un objeto que se BORRA: el target de un modificador (Armature/Mirror/Boolean) o el
// skinArmature directo. Hay que LIMPIARLA al borrar, sino queda COLGANDO: la malla sigue deformando/animando contra
// un esqueleto borrado (bug Dante: "borre el esqueleto y el modelo sigue con la animacion") y cuando el comando de
// undo se cae del stack (delete del obj) es un puntero MUERTO -> crash. Se restaura en el undo.
struct RefEntry { Mesh* mesh; Modifier* mod; Object* target; }; // mod!=NULL: era mod->target; mod==NULL: era skinArmature
static bool EnSubarbol(Object* raiz, Object* buscado){
    if (!raiz) return false;
    if (raiz == buscado) return true;
    for (size_t i = 0; i < raiz->Childrens.size(); i++) if (EnSubarbol(raiz->Childrens[i], buscado)) return true;
    return false;
}
static bool ApuntaABorrado(Object* target, const std::vector<DelEntry>& ents){
    if (!target) return false;
    for (size_t i = 0; i < ents.size(); i++) if (EnSubarbol(ents[i].obj, target)) return true;
    return false;
}
// recorre la escena y junta toda referencia (modificador o skinArmature) que apunte a un objeto que se borra.
static void RecolectarRefs(Object* node, const std::vector<DelEntry>& ents, std::vector<RefEntry>& out){
    if (!node) return;
    if (node->getType() == ObjectType::mesh){ Mesh* m = (Mesh*)node;
        for (size_t i = 0; i < m->modificadores.size(); i++){ Modifier* md = m->modificadores[i];
            if (md && ApuntaABorrado(md->target, ents)){ RefEntry r; r.mesh=m; r.mod=md; r.target=md->target; out.push_back(r); } }
        if (ApuntaABorrado((Object*)m->skinArmature, ents)){ RefEntry r; r.mesh=m; r.mod=NULL; r.target=(Object*)m->skinArmature; out.push_back(r); }
    }
    for (size_t i = 0; i < node->Childrens.size(); i++) RecolectarRefs(node->Childrens[i], ents, out);
}

class DeleteUndo : public UndoCmd {
    std::vector<DelEntry> ents;
    std::vector<RepEntry> reps;    // hijos de los borrados, reparentados al abuelo
    std::vector<RefEntry> refs;    // refs de mallas (modificador/skinArmature) a los objetos borrados -> limpiar/restaurar
    bool repsListos;               // las reps (y refs) se computan UNA vez (en el 1er detach)
    std::vector<Object*>  selPrev; Object* actPrev; Camera* camPrev; Object* colPrev; // CollectionActive previa
    int animKindPrev; Armature* animArmPrev; // seleccion de animacion previa (para restaurar al deshacer)
    bool enEscena; // true = los objetos estan en la escena; false = los tiene este comando (detachados)
    void QuitarHijo(Object* p, Object* c) {
        for (size_t k = 0; k < p->Childrens.size(); k++)
            if (p->Childrens[k] == c) { p->Childrens.erase(p->Childrens.begin()+k); break; }
    }
    void Detachar() {
        // 1) computar (1 sola vez) el reparentado de los hijos de cada objeto borrado hacia el ABUELO. Un hijo que
        //    TAMBIEN se borra (esta en ents) se va con su root -> no se reparenta.
        if (!repsListos) {
            for (size_t i = 0; i < ents.size(); i++) { DelEntry& e = ents[i];
                Vector3 gAbuelo = e.parent->GetGlobalPosition();
                // recorre el SUBARBOL del borrado: los descendientes NO seleccionados se PRESERVAN (reparent al abuelo);
                // los SELECCIONADOS se borran con el (se baja a sus hijos). Antes reparentaba TODO hijo directo -> los
                // hijos seleccionados (ej. las mallas de un armature al "borrar todo") sobrevivian en vez de borrarse.
                std::vector<Object*> pila; for (size_t k = 0; k < e.obj->Childrens.size(); k++) pila.push_back(e.obj->Childrens[k]);
                for (size_t k = 0; k < pila.size(); k++) {
                    Object* ch = pila[k];
                    bool esRoot = false;
                    for (size_t j = 0; j < ents.size(); j++) if (ents[j].obj == ch) { esRoot = true; break; }
                    if (esRoot) continue; // ya es su propio delete-root (no deberia caer dentro de otro subarbol)
                    if (ch->select) { // seleccionado -> se borra con el subarbol; seguir bajando a sus hijos
                        for (size_t j = 0; j < ch->Childrens.size(); j++) pila.push_back(ch->Childrens[j]);
                        continue;
                    }
                    // NO seleccionado -> preservar: reparent al abuelo (no se recursea adentro; queda como estaba)
                    RepEntry r; r.child = ch; r.abuelo = e.parent; r.borrado = ch->Parent ? ch->Parent : e.obj;
                    r.posBajoBorrado = ch->pos;
                    r.posBajoAbuelo  = ch->GetGlobalPosition() - gAbuelo; // preserva posicion global
                    reps.push_back(r);
                }
            }
            // refs de OTRAS mallas al subarbol borrado (modificador Armature/Mirror/Boolean, o skinArmature directo)
            if (SceneCollection) RecolectarRefs(SceneCollection, ents, refs);
            repsListos = true;
        }
        // 2) mover los hijos reparentados al abuelo (ANTES de detachar el borrado, asi sus luces no se detachan)
        for (size_t i = 0; i < reps.size(); i++) { RepEntry& r = reps[i];
            QuitarHijo(r.borrado, r.child);
            r.child->Parent = r.abuelo;
            r.child->pos = r.posBajoAbuelo;
            r.abuelo->Childrens.push_back(r.child);
        }
        // 3) detachar cada objeto borrado (ya sin esos hijos)
        for (size_t i = 0; i < ents.size(); i++) { DelEntry& e = ents[i];
            for (size_t k = 0; k < e.parent->Childrens.size(); k++)
                if (e.parent->Childrens[k] == e.obj) { e.index = (int)k; e.parent->Childrens.erase(e.parent->Childrens.begin()+k); break; }
            DetacharLuces(e.obj);
            if (ContieneCamActiva(e.obj)) CameraActive = NULL;
            if (ContieneAnimActiva(e.obj)) { ActiveAnimArm = NULL; ActiveAnimKind = 0; } // clip activo borrado -> a ESCENA
        }
        // 4) limpiar las refs colgantes: el modificador vuelve a target=NULL ("none") y skinArmature a NULL -> la malla
        //    deja de deformar (vuelve a bind) y no queda apuntando al esqueleto borrado (que este comando liberara).
        for (size_t i = 0; i < refs.size(); i++){ RefEntry& r = refs[i];
            if (r.mod) r.mod->target = NULL; else if (r.mesh) r.mesh->skinArmature = NULL;
            if (r.mesh) r.mesh->lastSkinFrame = -999999;
        }
        // 5) si CollectionActive quedo FUERA de la escena (se borro), reapuntarla a una coleccion valida (otra bajo la
        //    escena, o la propia SceneCollection). Sin esto, Add Collection / Import parentan a memoria liberada
        //    (no aparecen / crash). El undo la restaura (colPrev).
        if (SceneCollection && (!CollectionActive || !EnSubarbol(SceneCollection, CollectionActive))) {
            Object* c = NULL;
            for (size_t i = 0; i < SceneCollection->Childrens.size() && !c; i++)
                if (SceneCollection->Childrens[i]->getType() == ObjectType::collection) c = SceneCollection->Childrens[i];
            CollectionActive = c ? c : SceneCollection;
        }
        enEscena = false;
    }
public:
    DeleteUndo(bool incCol) : repsListos(false), actPrev(NULL), camPrev(NULL), colPrev(NULL), enEscena(true) {
        selPrev = ObjSelects; actPrev = ObjActivo; camPrev = CameraActive; colPrev = CollectionActive;
        animKindPrev = ActiveAnimKind; animArmPrev = ActiveAnimArm;
        if (SceneCollection) RecolectarBorrar(SceneCollection, incCol, ents);
        Detachar(); // el borrado YA paso: los saca de la escena (sin liberar)
    }
    bool Vacio() const { return ents.empty(); }
    void Aplicar() {
        if (enEscena) { Detachar(); return; } // redo del borrado
        // undo: re-inserta cada root en su padre (bajo->alto: los indices guardados quedan validos)
        for (int i = (int)ents.size()-1; i >= 0; i--) { DelEntry& e = ents[i];
            int idx = e.index; if (idx < 0) idx = 0; if (idx > (int)e.parent->Childrens.size()) idx = (int)e.parent->Childrens.size();
            e.parent->Childrens.insert(e.parent->Childrens.begin()+idx, e.obj);
            ReattacharLuces(e.obj);
        }
        // devolver los hijos reparentados a su objeto original (restaurando su posicion local)
        for (size_t i = 0; i < reps.size(); i++) { RepEntry& r = reps[i];
            QuitarHijo(r.abuelo, r.child);
            r.child->Parent = r.borrado;
            r.child->pos = r.posBajoBorrado;
            r.borrado->Childrens.push_back(r.child);
        }
        // restaurar las refs (el armature/target volvio a la escena): el modificador recupera su target y skinArmature
        // su puntero -> la malla vuelve a deformar como antes del borrado.
        for (size_t i = 0; i < refs.size(); i++){ RefEntry& r = refs[i];
            if (r.mod) r.mod->target = r.target; else if (r.mesh) r.mesh->skinArmature = (Armature*)r.target;
            if (r.mesh) r.mesh->lastSkinFrame = -999999;
        }
        CameraActive = camPrev; ObjSelects = selPrev; ObjActivo = actPrev; CollectionActive = colPrev; // restaura seleccion + camara + coleccion activa
        ActiveAnimKind = animKindPrev; ActiveAnimArm = animArmPrev; // restaura la seleccion de animacion (el esqueleto volvio)
        for (size_t i = 0; i < ObjSelects.size(); i++) if (ObjSelects[i]) ObjSelects[i]->select = true;
        enEscena = true;
    }
    ~DeleteUndo() { if (!enEscena) for (size_t i = 0; i < ents.size(); i++) delete ents[i].obj; } // detachados: liberar de verdad
};

// ---- stacks ----
static std::vector<UndoCmd*> g_undo;
static std::vector<UndoCmd*> g_redo;
static TransformUndo*        g_pendingT  = NULL; // transform de objeto en curso (sin confirmar)
static EditMoveUndo*         g_pendingEM = NULL; // move de malla en edit mode en curso
static MaterialModUndo*      g_pendingMat = NULL; // modificacion de material en curso (checkbox/shininess)
static const size_t          MAXU = 100;

static void LimpiarRedo() { for (size_t i = 0; i < g_redo.size(); i++) delete g_redo[i]; g_redo.clear(); }

static void Push(UndoCmd* c) {
    if (!c) return;
    LimpiarRedo(); // una accion NUEVA invalida el redo
    g_undo.push_back(c);
    while (g_undo.size() > MAXU) { delete g_undo.front(); g_undo.erase(g_undo.begin()); }
}

// ============================================================================
//  JOIN (Ctrl+J): comando ATOMICO = geometria del activo (MeshGeoUndo) + borrado de los mergeados (DeleteUndo).
//  Un solo Ctrl+Z restaura la geo del activo Y re-inserta los objetos mergeados (sin estado intermedio roto).
// ============================================================================
class JoinUndo : public UndoCmd {
    MeshGeoUndo* geo; DeleteUndo* del;
public:
    JoinUndo(MeshGeoUndo* g, DeleteUndo* d) : geo(g), del(d) {}
    ~JoinUndo() { delete geo; delete del; }
    void Aplicar() { if (geo) geo->Aplicar(); if (del) del->Aplicar(); } // dos toggles independientes (geo del activo / arbol de escena)
};
static MeshGeoUndo* g_pendingJoin = NULL; // geo del activo capturada por UndoJoinIniciar (antes del merge)

void UndoJoinIniciar(Mesh* activeMesh) {
    delete g_pendingJoin;
    g_pendingJoin = activeMesh ? new MeshGeoUndo(activeMesh) : NULL;
}
void UndoJoinConfirmar() {
    if (!g_pendingJoin) return;
    // los objetos a borrar son los que quedaron con select=true (el caller dejo SOLO los mergeados marcados).
    DeleteUndo* del = new DeleteUndo(false); // detacha (sin liberar) + guarda para deshacer
    Push(new JoinUndo(g_pendingJoin, del));  // 1 comando atomico (geo + borrado)
    g_pendingJoin = NULL;                    // ahora lo posee el JoinUndo
}

// ARMATURE: snapshot de la REST (T/R/S/preRot/rotOrder/tlNode/head/tail/pose) de todos los huesos, para deshacer un
// Apply Transform sobre el armature (que la hornea). Aplicar() = SWAP + invalida el cache de skin de las mallas del rig.
struct BoneRestEst { Vector3 restT, restR, restS, preRot, head, tail, poseT, poseR, poseS; int rotOrder; Matrix4 tlNode; };
class ArmatureBonesUndo : public UndoCmd {
    Armature* a; std::vector<BoneRestEst> e;
public:
    ArmatureBonesUndo(Armature* arm) : a(arm) {
        if (!a) return;
        for (size_t i=0;i<a->bones.size();i++){ W3dBone& b=a->bones[i];
            BoneRestEst t; t.restT=b.restT; t.restR=b.restR; t.restS=b.restS; t.preRot=b.preRot; t.rotOrder=b.rotOrder;
            t.tlNode=b.tlNode; t.head=b.head; t.tail=b.tail; t.poseT=b.poseT; t.poseR=b.poseR; t.poseS=b.poseS;
            e.push_back(t); }
    }
    bool Vacio() const { return e.empty(); }
    void Aplicar() {
        if (!a || e.size()!=a->bones.size()) return;
        for (size_t i=0;i<e.size();i++){ W3dBone& b=a->bones[i]; BoneRestEst& t=e[i];
            Vector3 rt=b.restT,rr=b.restR,rs=b.restS,pr=b.preRot,hd=b.head,tl=b.tail,pt=b.poseT,pR=b.poseR,pS=b.poseS; int ro=b.rotOrder; Matrix4 tn=b.tlNode;
            b.restT=t.restT; b.restR=t.restR; b.restS=t.restS; b.preRot=t.preRot; b.rotOrder=t.rotOrder; b.tlNode=t.tlNode;
            b.head=t.head; b.tail=t.tail; b.poseT=t.poseT; b.poseR=t.poseR; b.poseS=t.poseS;
            t.restT=rt; t.restR=rr; t.restS=rs; t.preRot=pr; t.rotOrder=ro; t.tlNode=tn; t.head=hd; t.tail=tl; t.poseT=pt; t.poseR=pR; t.poseS=pS; }
        a->poseSerial++; // la pose se recalcula (rest nueva)
        InvalidarSkinDeArmature(a); // libera el cache stale + fuerza re-skin (la firma del cache no ve la rest)
    }
};

// ============================================================================
//  APPLY (Alt+A): comando ATOMICO = geometria (MeshGeoUndo por malla) + transform (TransformUndo de los
//  seleccionados) + rest de huesos (ArmatureBonesUndo por armature). Un Ctrl+Z restaura la geo horneada, los
//  pos/rot/scale reseteados Y la rest de los huesos en 1 solo paso.
// ============================================================================
class ApplyUndo : public UndoCmd {
    std::vector<MeshGeoUndo*> geos; TransformUndo* xf; std::vector<ArmatureBonesUndo*> arms;
public:
    ApplyUndo(const std::vector<MeshGeoUndo*>& g, TransformUndo* x, const std::vector<ArmatureBonesUndo*>& ar) : geos(g), xf(x), arms(ar) {}
    ~ApplyUndo() { for (size_t i=0;i<geos.size();i++) delete geos[i]; delete xf; for (size_t i=0;i<arms.size();i++) delete arms[i]; }
    void Aplicar() { for (size_t i=0;i<geos.size();i++) geos[i]->Aplicar(); if (xf) xf->Aplicar(); for (size_t i=0;i<arms.size();i++) arms[i]->Aplicar(); } // toggles independientes
};
static std::vector<MeshGeoUndo*> g_pendingApplyGeos;
static TransformUndo* g_pendingApplyXf = NULL;
static std::vector<ArmatureBonesUndo*> g_pendingApplyArms;

void UndoApplyIniciar() {
    for (size_t i=0;i<g_pendingApplyGeos.size();i++) delete g_pendingApplyGeos[i];
    g_pendingApplyGeos.clear();
    for (size_t i=0;i<g_pendingApplyArms.size();i++) delete g_pendingApplyArms[i];
    g_pendingApplyArms.clear();
    delete g_pendingApplyXf; g_pendingApplyXf = new TransformUndo(); // snapshot de pos/rot/scale de los seleccionados
    for (size_t i=0;i<ObjSelects.size();i++){ Object* o=ObjSelects[i]; if(!o) continue;
        if (o->getType()==ObjectType::mesh) g_pendingApplyGeos.push_back(new MeshGeoUndo((Mesh*)o));
        else if (o->getType()==ObjectType::armature) g_pendingApplyArms.push_back(new ArmatureBonesUndo((Armature*)o)); }
}
void UndoApplyConfirmar() {
    if (g_pendingApplyGeos.empty() && g_pendingApplyArms.empty() && !g_pendingApplyXf) return;
    Push(new ApplyUndo(g_pendingApplyGeos, g_pendingApplyXf, g_pendingApplyArms)); // 1 comando (geo + transform + rest)
    g_pendingApplyGeos.clear(); g_pendingApplyXf = NULL; g_pendingApplyArms.clear();
}

// ============================================================================
//  POSE (G/R/S de huesos en Pose Mode): snapshot de la POSE (poseT/R/S) de todos los huesos antes del transform.
//  Ctrl+Z restaura la pose previa. La pose se guarda a la curva recien con Insert Keyframe (esto solo deshace el drag).
// ============================================================================
struct PoseEst { Vector3 T, R, S; };
class PoseUndo : public UndoCmd {
    Armature* a; std::vector<PoseEst> e;
public:
    PoseUndo(Armature* arm) : a(arm) {
        if (!a) return;
        for (size_t i=0;i<a->bones.size();i++){ PoseEst p; p.T=a->bones[i].poseT; p.R=a->bones[i].poseR; p.S=a->bones[i].poseS; e.push_back(p); }
    }
    bool Vacio() const { return e.empty(); }
    bool Cambio() const { // hubo cambio real respecto al snapshot? (evita empujar un undo vacio si G+Esc / click sin mover)
        if (!a || e.size()!=a->bones.size()) return false;
        for (size_t i=0;i<e.size();i++){ const W3dBone& b=a->bones[i];
            if (b.poseT.x!=e[i].T.x||b.poseT.y!=e[i].T.y||b.poseT.z!=e[i].T.z) return true;
            if (b.poseR.x!=e[i].R.x||b.poseR.y!=e[i].R.y||b.poseR.z!=e[i].R.z) return true;
            if (b.poseS.x!=e[i].S.x||b.poseS.y!=e[i].S.y||b.poseS.z!=e[i].S.z) return true; }
        return false;
    }
    void Aplicar(){
        if (!a || e.size()!=a->bones.size()) return;
        for (size_t i=0;i<e.size();i++){ W3dBone& b=a->bones[i]; Vector3 t=b.poseT,r=b.poseR,s=b.poseS;
            b.poseT=e[i].T; b.poseR=e[i].R; b.poseS=e[i].S; e[i].T=t; e[i].R=r; e[i].S=s; }
        a->poseDirty=true; a->poseSerial++; InvalidarSkinDeArmature(a);
    }
};
// ============================================================================
//  KEYFRAMES (dope sheet: borrar / mover): snapshot de TODAS las curvas de la animacion ACTIVA (clip de armature
//  o escena). Editar keyframes NO cambia la ESTRUCTURA (tracks/propiedades), solo el contenido de cada curva ->
//  alcanza con recolectar los vectores en el mismo orden y hacer swap. Ctrl+Z devuelve los keyframes.
// ============================================================================
static void RecolectarCurvas(std::vector<std::vector<keyFrame>*>& out){
    out.clear();
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size()){
        SkeletalAnimation* an = ActiveAnimArm->animations[ActiveAnimArm->animActiva];
        for (size_t t=0;t<an->tracks.size();t++)
            for (size_t p=0;p<an->tracks[t].Propertys.size();p++) out.push_back(&an->tracks[t].Propertys[p].keyframes);
    } else {
        for (size_t i=0;i<AnimationObjects.size();i++)
            for (size_t p=0;p<AnimationObjects[i].Propertys.size();p++) out.push_back(&AnimationObjects[i].Propertys[p].keyframes);
    }
}
class KeyframesUndo : public UndoCmd {
    std::vector<std::vector<keyFrame> > snap;
public:
    KeyframesUndo(){ std::vector<std::vector<keyFrame>*> c; RecolectarCurvas(c);
        snap.resize(c.size()); for (size_t i=0;i<c.size();i++) snap[i] = *c[i]; }
    bool Vacio() const { return snap.empty(); }
    bool Cambio() const { // hubo cambio real? (no empujar un undo vacio)
        std::vector<std::vector<keyFrame>*> c; RecolectarCurvas(c);
        if (c.size() != snap.size()) return true;
        for (size_t i=0;i<c.size();i++){ if (c[i]->size() != snap[i].size()) return true;
            for (size_t k=0;k<snap[i].size();k++){
                const keyFrame& a = (*c[i])[k]; const keyFrame& b = snap[i][k];
                // OJO: tambien la INTERPOLACION y los HANDLES. Curvar un tramo no mueve el keyframe (mismo frame y
                // mismo valor): si solo se miraban esos dos, el undo de una curva se descartaba por "no hubo cambio".
                if (a.frame != b.frame || a.value != b.value) return true;
                if (a.Interpolation != b.Interpolation || a.handleType != b.handleType) return true;
                if (a.inDF != b.inDF || a.inDV != b.inDV || a.outDF != b.outDF || a.outDV != b.outDV) return true; } }
        return false;
    }
    void Aplicar(){
        std::vector<std::vector<keyFrame>*> c; RecolectarCurvas(c);
        if (c.size() != snap.size()) return; // la estructura cambio (otra animacion activa): no tocar nada
        for (size_t i=0;i<c.size();i++) c[i]->swap(snap[i]);
        if (ActiveAnimArm){ ActiveAnimArm->lastPoseFrame = -999999; ActiveAnimArm->poseDirty = true; ActiveAnimArm->poseSerial++;
                            InvalidarSkinDeArmature(ActiveAnimArm); }
    }
};
static KeyframesUndo* g_pendingKeys = NULL;
void UndoKeyframesIniciar(){ delete g_pendingKeys; g_pendingKeys = new KeyframesUndo(); }
void UndoKeyframesConfirmar(){
    if (!g_pendingKeys) return;
    if (g_pendingKeys->Vacio() || !g_pendingKeys->Cambio()){ delete g_pendingKeys; g_pendingKeys = NULL; return; }
    Push(g_pendingKeys); g_pendingKeys = NULL;
}

static PoseUndo* g_pendingPose = NULL;
void UndoPoseIniciar(Armature* a){ delete g_pendingPose; g_pendingPose = a ? new PoseUndo(a) : NULL; }
void UndoPoseConfirmar(){
    if (!g_pendingPose) return;
    if (g_pendingPose->Vacio() || !g_pendingPose->Cambio()){ delete g_pendingPose; g_pendingPose = NULL; return; }
    Push(g_pendingPose); g_pendingPose = NULL;
}

void UndoDeshacer() {
    if (g_undo.empty()) return;
    UndoCmd* c = g_undo.back(); g_undo.pop_back();
    c->Aplicar();          // intercambia: el comando queda con el estado NUEVO
    g_redo.push_back(c);   // disponible para rehacer
}
void UndoRehacer() {
    if (g_redo.empty()) return;
    UndoCmd* c = g_redo.back(); g_redo.pop_back();
    c->Aplicar();          // intercambia de nuevo: re-aplica el cambio
    g_undo.push_back(c);
}
void UndoLimpiar() {
    for (size_t i = 0; i < g_undo.size(); i++) delete g_undo[i];
    g_undo.clear();
    LimpiarRedo();
    if (g_pendingT)  { delete g_pendingT;  g_pendingT  = NULL; }
    if (g_pendingEM) { delete g_pendingEM; g_pendingEM = NULL; }
    if (g_pendingMat){ delete g_pendingMat; g_pendingMat = NULL; }
    // Ningun pendiente puede sobrevivir a un reset de escena. PoseUndo se queda con un Armature* crudo: si la
    // escena se rehace con un transform de pose a medio hacer, ese puntero queda colgado y el proximo
    // UndoPoseConfirmar lo desreferencia.
    if (g_pendingKeys){ delete g_pendingKeys; g_pendingKeys = NULL; }
    if (g_pendingPose){ delete g_pendingPose; g_pendingPose = NULL; }
}
bool UndoHayAlgo() { return !g_undo.empty(); }
bool UndoHayRedo() { return !g_redo.empty(); }

void UndoCapturarModo()                       { Push(new ModeUndo(InteractionMode)); }
void UndoCapturarRename(std::string* destino) { if (destino) Push(new RenameUndo(destino)); }
void UndoCapturarSeleccion()                  { Push(new SelectUndo()); }
void UndoCapturarSeleccionEdit(Mesh* m)       { if (m) Push(new SelectEditUndo(m)); }
void UndoCapturarMaterial(Mesh* m, int idx)   { if (m) Push(new MaterialUndo(m, idx)); }
void UndoCapturarMallaGeo(Mesh* m)            { if (m) Push(new MeshGeoUndo(m)); } // snapshot ANTES del op

// BORRAR objetos: DETACHA los seleccionados (sin liberar) y guarda el comando. Reemplaza al delete real.
// Devuelve true si detacho algo (el caller no tiene que borrar nada mas).
bool UndoCapturarBorrado(bool incCol) {
    DeleteUndo* d = new DeleteUndo(incCol);
    if (d->Vacio()) { delete d; return false; }
    Push(d);
    return true;
}

void UndoTransformIniciar() {
    if (g_pendingT) delete g_pendingT;
    g_pendingT = new TransformUndo();
}
void UndoTransformConfirmar() {
    if (!g_pendingT) return;
    if (!g_pendingT->Vacio()) Push(g_pendingT); else delete g_pendingT;
    g_pendingT = NULL;
}
void UndoTransformCancelar() {
    if (g_pendingT) { delete g_pendingT; g_pendingT = NULL; }
}

void UndoEditMoveIniciar(Mesh* m) {
    if (g_pendingEM) delete g_pendingEM;
    g_pendingEM = new EditMoveUndo(m);
}
void UndoEditMoveConfirmar() {
    if (!g_pendingEM) return;
    if (!g_pendingEM->Vacio()) Push(g_pendingEM); else delete g_pendingEM;
    g_pendingEM = NULL;
}
void UndoEditMoveCancelar() {
    if (g_pendingEM) { delete g_pendingEM; g_pendingEM = NULL; }
}

// COLOR (lo llama el ColorPicker al cerrar): pushea solo si el color cambio (cancelar restaura -> no pushea)
void UndoCapturarColor(GLfloat* target, const GLfloat* viejo) {
    if (!target || !viejo) return;
    bool cambio = false; for (int i=0;i<4;i++) if (target[i] != viejo[i]) cambio = true;
    if (cambio) Push(new ColorUndo(target, viejo));
}

// MODIFICACION de material (checkbox/shininess): pendiente -> snapshot al empezar a tocar, push al soltar.
void UndoMaterialModIniciar(Material* m) {
    if (!m) return;
    if (g_pendingMat && g_pendingMat->Mat() == m) return; // ya hay snapshot de este material
    if (g_pendingMat) delete g_pendingMat;                // otro material sin commitear -> descarta
    g_pendingMat = new MaterialModUndo(m);
}
void UndoMaterialModCommit() { // lo llama el panel cada frame al soltar el mouse
    if (!g_pendingMat) return;
    if (g_pendingMat->Difiere()) Push(g_pendingMat); else delete g_pendingMat;
    g_pendingMat = NULL;
}
