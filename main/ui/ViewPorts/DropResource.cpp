// ============================================================================
//  DropResource.cpp — SOLTAR un recurso de la BIBLIOTECA (el outliner) en el
//  VIEWPORT 3D: lo que hay bajo el puntero lo decide un RAYO desde ese pixel
//  (con la misma lente con que se dibuja el viewport, Viewport3D::CurrentLens)
//  contra los triangulos de las mallas visibles de la escena:
//    - malla 3D  -> un OBJETO malla nuevo con ese recurso, en el punto del rayo (sobre
//                   la superficie tocada, o en el plano del cursor 3D si no toca nada);
//    - material  -> a la PARTE de la malla tocada;
//    - shader    -> al material de la PARTE tocada (con el material por defecto: uno nuevo con ese shader);
//    - textura   -> al material de la parte tocada (el por defecto no se edita);
//    - animset   -> al armature de la malla tocada (o el que se toca);
//    - prefab    -> una INSTANCIA en el punto del rayo (io/PrefabsEditor.h).
//  Lo que viene de una LIBRERIA externa (io/Libraries.h) llega con su id GLOBAL: un prefab
//  o una escena suya ("lib:<libreria>/prefab|scene/<elemento>") = un PROXY en el punto del
//  rayo; su malla, material, textura o animset = una REFERENCIA al recurso de la libreria
//  (con su prefijo). Lo que es de una libreria no recibe nada (es de solo lectura).
//  Todo con Ctrl+Z. 'onlyTest' = no hace nada, solo dice si se puede y que haria
//  (el resaltado mientras se arrastra).
//  Solo editor. C++03. Motor generico.
// ============================================================================
#include "objects/Sound.h"   // soltar un sonido en el 3D: un objeto Sonido
#include "ViewPort3D.h"
#include "io/ProjectResources.h"
#include "io/ProjectMeshes.h"
#include "importers/import_w3d.h"      // la malla de trabajo fuera de la escena (probar que el recurso carga)
#include "objects/MeshResource.h"
#include "objects/Mesh.h"
#include "objects/Materials.h"
#include "objects/Textures.h"
#include "objects/Armature.h"
#include "objects/ObjectMode.h"
#include "animation/W3dAnimSet.h"
#include "io/PrefabsEditor.h"          // soltar un prefab = una instancia nueva
#include "objects/InstancePrefab.h"
#include "objects/ProxyW3d.h"          // soltar un prefab o una escena de una libreria = un proxy
#include "io/Libraries.h"
#include "undo/Undo.h"
#include "config/W3dLang.h"
#include "render/RenderOptions.h"
#include "variables.h"
#include "base/W3dInteractionState.h"
#include "w3dlog.h"
#include <cmath>
#include <cfloat>
#include "io/Prefabs.h"   // W3dPrefabKeyScene: una escena del proyecto se instancia
#include "objects/WrittenShader.h"   // soltar un shader: el material de la parte lo usa

// ---- EL RAYO ----
namespace {
struct Ray { Vector3 o, d; };
struct TouchState { Mesh* m; int part; float t; Vector3 p; TouchState() : m(NULL), part(-1), t(FLT_MAX) {} };
}
static Ray RayFromPixel(Viewport3D* vp, int mx, int my) {
    const Viewport3D::Lens L = vp->CurrentLens();
    float nx = ((float)(mx - vp->x) + 0.5f) / (float)vp->width * 2.0f - 1.0f;
    float ny = 1.0f - ((float)(my - vp->y) + 0.5f) / (float)vp->height * 2.0f;
    nx = (nx - L.panX) / (L.zoom != 0.0f ? L.zoom : 1.0f);
    ny = (ny - L.panY) / (L.zoom != 0.0f ? L.zoom : 1.0f);
    Ray r;
    if (L.ortho) { r.o = L.pos + L.right * (nx * L.sx) + L.arr * (ny * L.sy); r.d = L.fwd; }
    else { r.o = L.pos; r.d = (L.fwd + L.right * (nx * L.sx) + L.arr * (ny * L.sy)).Normalized(); }
    return r;
}
// Moller-Trumbore
static bool RayTri(const Ray& r, const Vector3& a, const Vector3& b, const Vector3& c, float& t) {
    const Vector3 e1 = b - a, e2 = c - a;
    const Vector3 p = Vector3::Cross(r.d, e2);
    const float det = e1.Dot(p);
    if (fabsf(det) < 1e-9f) return false;
    const float inv = 1.0f / det;
    const Vector3 s = r.o - a;
    const float u = s.Dot(p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const Vector3 q = Vector3::Cross(s, e1);
    const float v = r.d.Dot(q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = e2.Dot(q) * inv;
    return t > 1e-5f;
}
static void TouchTree(Object* o, const Ray& r, TouchState& best) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h || !h->visible) continue;
        if (h->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)h;
            if (m->vertex && m->faces && m->facesSize >= 3) {
                Matrix4 W; m->GetWorldMatrix(W);
                for (int k = 0; k + 2 < m->facesSize; k += 3) {
                    const int ia = (int)m->faces[k], ib = (int)m->faces[k + 1], ic = (int)m->faces[k + 2];
                    if (ia >= m->vertexSize || ib >= m->vertexSize || ic >= m->vertexSize) continue;
                    const Vector3 a = W * Vector3(m->vertex[ia * 3], m->vertex[ia * 3 + 1], m->vertex[ia * 3 + 2]);
                    const Vector3 b = W * Vector3(m->vertex[ib * 3], m->vertex[ib * 3 + 1], m->vertex[ib * 3 + 2]);
                    const Vector3 c = W * Vector3(m->vertex[ic * 3], m->vertex[ic * 3 + 1], m->vertex[ic * 3 + 2]);
                    float t;
                    if (!RayTri(r, a, b, c, t) || t >= best.t) continue;
                    best.t = t; best.m = m; best.p = r.o + r.d * t;
                    best.part = -1;
                    for (size_t g = 0; g < m->materialsGroup.size(); g++) {
                        const MaterialGroup& mg = m->materialsGroup[g];
                        if (k >= mg.startDrawn && k < mg.startDrawn + mg.indicesDrawnCount) { best.part = (int)g; break; }
                    }
                    if (best.part < 0 && !m->materialsGroup.empty()) best.part = 0;
                }
            }
        }
        TouchTree(h, r, best);
    }
}
static TouchState Touch(Viewport3D* vp, int mx, int my, Ray* ray) {
    Ray r = RayFromPixel(vp, mx, my);
    if (ray) *ray = r;
    TouchState t;
    TouchTree(SceneCollection, r, t);
    return t;
}
// el punto donde va un objeto nuevo: lo tocado, o el rayo contra el plano HORIZONTAL del cursor 3D, o el cursor
static Vector3 PointOfRelease(const TouchState& t, const Ray& r) {
    if (t.m) return t.p;
    const float y0 = cursor3D.pos.y;
    if (fabsf(r.d.y) > 1e-5f) {
        const float k = (y0 - r.o.y) / r.d.y;
        if (k > 0.0f && k < 10000.0f) return r.o + r.d * k;
    }
    return cursor3D.pos;
}
// el armature de una malla (su padre, o el de mas arriba) o el objeto mismo si es uno
static Armature* ArmatureOf(Object* o) {
    for (Object* p = o; p && p != SceneCollection; p = p->Parent)
        if (p->getType() == ObjectType::armature) return (Armature*)p;
    return NULL;
}

// lo tocado es de una LIBRERIA (lo genera un proxy, o usa una malla de una libreria): de solo lectura
static bool FromLib(Object* o) {
    if (!o) return false;
    if (o->getType() == ObjectType::mesh && ((Mesh*)o)->mesh && !((Mesh*)o)->mesh->lib.empty()) return true;
    for (Object* p = W3dInstanceOf(o); p; p = W3dInstanceOf(p)) if (p->getType() == ObjectType::proxy) return true;
    return false;
}

// ---- LOS PASOS DE UNDO que no tenia nadie ----
// la TEXTURA base de un material (ida y vuelta). El paso es DUENO de una referencia a 'otra' (la ranura del material
// tiene la suya: Textures.h, TextureSetIn): ir y volver intercambia las dos sin tocar la cuenta, y la que el paso
// tenia guardada se suelta cuando se cae del historial
struct UndoTexMat { Material* m; Texture* other; bool onOther; int map; };   // mapa = W3dMatMap (la ranura)
// los pasos VIVOS (en el historial): una textura PURGADA que revive le devuelve su referencia a cada paso que la
// guarda (W3dUndoTextureOwners; la purga las habia cerrado todas, y el paso la suelta igual al caerse)
static std::vector<UndoTexMat*> gUndoTexMats;
int W3dUndoTextureOwners(const Texture* t) {
    int n = 0;
    for (size_t i = 0; i < gUndoTexMats.size(); i++) if (t && gUndoTexMats[i]->other == t) n++;
    return n;
}
static void UndoTexMatApply(void* d) {
    UndoTexMat* u = (UndoTexMat*)d;
    bool live = false;
    for (size_t i = 0; i < Materials.size() && !live; i++) live = (Materials[i] == u->m);
    if (!live) return;
    Texture** pr = u->m->MapSlot(u->map);
    if (!pr) return;
    Texture*& slot = *pr;
    bool& isOn = u->map == W3D_MAP_NORMAL ? u->m->normalMap : u->map == W3D_MAP_BASE ? u->m->textureOn : u->m->mapWithTexture[u->map];
    Texture* t = slot; const bool on = isOn;
    slot = u->other; isOn = u->onOther;
    u->other = t; u->onOther = on;
    W3dResourcesCheckPurged();
    g_redraw = true;
}
static void UndoTexMatFree(void* d) {
    UndoTexMat* u = (UndoTexMat*)d;
    for (size_t i = 0; i < gUndoTexMats.size(); i++) if (gUndoTexMats[i] == u) { gUndoTexMats.erase(gUndoTexMats.begin() + (long)i); break; }
    TextureRelease(u->other);
    delete u;
}
// la textura BASE de un material (prendida), con su paso de undo: la usan soltar en el 3D y en Properties
static void SetInSlot(Material* mat, Texture* tex, int map) {
    if (!mat || !tex || !mat->MapSlot(map)) return;
    Texture*& slot = *mat->MapSlot(map);
    bool& on = map == W3D_MAP_NORMAL ? mat->normalMap : map == W3D_MAP_BASE ? mat->textureOn : mat->mapWithTexture[map];
    UndoTexMat* u = new UndoTexMat();
    u->m = mat; u->other = slot; u->onOther = on; u->map = map;   // (la referencia de la ranura pasa al paso)
    TextureRetain(tex);                                                      // (y la ranura toma una de la nueva)
    slot = tex; on = true;
    UndoExternal f; f.apply = UndoTexMatApply; f.free = UndoTexMatFree;
    gUndoTexMats.push_back(u);
    UndoPushExternal(f, u);
    g_redraw = true;
}
void W3dMaterialSetTexture(Material* mat, Texture* tex) { SetInSlot(mat, tex, W3D_MAP_BASE); }
// el NORMAL MAP de un material (prendido), con su paso de undo: soltar una textura en su selector de Properties
void W3dMaterialSetNormal(Material* mat, Texture* tex) { SetInSlot(mat, tex, W3D_MAP_NORMAL); }
// cualquier MAPA (W3dMatMap): la textura queda en su ranura y el mapa pasa a usarla, con su paso de undo
void W3dMaterialSetMap(Material* mat, Texture* tex, int map) { SetInSlot(mat, tex, map); }
// el ANIMSET de un armature (ida y vuelta: el de antes, o ninguno)
struct UndoAnimArm { Armature* a; std::string other; };
static void SetAnimSet(Armature* a, const std::string& name) {
    while (!a->animations.empty()) W3dArmatureAnimsRemove(a, (int)a->animations.size() - 1);
    if (!name.empty()) { std::string mot; W3dArmatureAssignAnimSet(a, name, NULL, &mot); }
}
static std::string AnimSetOf(Armature* a) {
    if (a->animations.empty()) return std::string();
    const W3dAnimSet* s = W3dArmatureAnimSetFits(a) ? W3dArmatureAnimSet(a) : NULL;
    return s ? s->name : a->animSetName;
}
static void UndoAnimArmApply(void* d) {
    UndoAnimArm* u = (UndoAnimArm*)d;
    if (!u->a) return;
    const std::string now = AnimSetOf(u->a);
    SetAnimSet(u->a, u->other);
    u->other = now;
    g_redraw = true;
}
static void UndoAnimArmUnlink(void* d, Object* deleted) { UndoAnimArm* u = (UndoAnimArm*)d; if ((Object*)u->a == deleted) u->a = NULL; }
static void UndoAnimArmFree(void* d) { delete (UndoAnimArm*)d; }

// "sounds/musica.wav" -> "musica" (el nombre del objeto Sonido)
static std::string LeafNoExt(const std::string& r) {
    size_t a = r.find_last_of("/\\"); a = (a == std::string::npos) ? 0 : a + 1;
    size_t b = r.find_last_of('.'); if (b == std::string::npos || b < a) b = r.size();
    return r.substr(a, b - a);
}
// ---- LA VISTA PREVIA mientras se arrastra un MATERIAL o un SHADER sobre una malla: la parte bajo el puntero se dibuja
// con lo arrastrado (se ve a que parte le vas a pegar); al moverse a otra parte, salir de la malla o soltar en otro lado
// vuelve a su material. Es solo de dibujo: no hay paso de undo ni el proyecto queda cambiado ---
namespace {
struct PreviewView {
    Mesh* m; int part; Material* original;   // lo que tenia la parte
    Material* placed;                          // lo que se le puso (el material arrastrado o la copia con el shader)
    int type; std::string id;
    PreviewView() : m(NULL), part(-1), original(NULL), placed(NULL), type(-1) {}
};
}
static PreviewView gPreview;
static Material* gPreviewShader = NULL;   // la copia del material de la parte con el shader arrastrado (sin registrar)
void W3dReleasePreviewViewEnd() {
    if (!gPreview.m) return;
    if (gPreview.part < (int)gPreview.m->materialsGroup.size() &&
        gPreview.m->materialsGroup[(size_t)gPreview.part].material == gPreview.placed)
        gPreview.m->materialsGroup[(size_t)gPreview.part].material = gPreview.original;
    gPreview = PreviewView();
    g_redraw = true;
}
static std::string gPreviewShaderOf;      // de que material + shader es la copia (asi no se recompila en cada movimiento)
static void PreviewViewSet(Mesh* m, int part, int type, const std::string& id, Material* mat) {
    Material* original = m->materialsGroup[(size_t)part].material;
    Material* placed = mat;
    if (type == W3D_VIEW_SHADERS) {
        // una copia del material de la parte (o uno nuevo) con el shader: el de verdad no se toca
        char key[64]; snprintf(key, sizeof(key), "%p|", (void*)original);
        if (!gPreviewShader || gPreviewShaderOf != std::string(key) + id) {
            if (!gPreviewShader) gPreviewShader = new Material("(preview)", true);
            else W3dShaderMaterialForget(gPreviewShader);
            if (original && original != MaterialDefault) { *gPreviewShader = *original; gPreviewShader->shaderCPU = NULL; }
            gPreviewShader->shaderOn = true; gPreviewShader->shaderFile = id;
            gPreviewShader->shaderId = 0; gPreviewShader->shaderState = 0;
            gPreviewShaderOf = std::string(key) + id;
        }
        placed = gPreviewShader;
    }
    if (!placed) return;
    m->materialsGroup[(size_t)part].material = placed;
    gPreview.m = m; gPreview.part = part; gPreview.original = original; gPreview.placed = placed;
    gPreview.type = type; gPreview.id = id;
    g_redraw = true;
}

bool W3dReleaseResourceIn3D(Viewport3D* vp, int mx, int my, int type, const std::string& id, bool onlyTest, std::string* that) {
    if (that) that->clear();
    // (el rayo y el soltar ven lo de VERDAD: la vista previa se saca primero; si el puntero sigue sobre una parte que
    //  la acepta, se vuelve a poner abajo)
    W3dReleasePreviewViewEnd();

    if (!vp || !SceneCollection) return false;
    if (InteractionMode != ObjectMode || state != editNavigation) {
        if (that) *that = T("Only in Object Mode");
        return false;
    }
    Ray ray;
    const TouchState t = Touch(vp, mx, my, &ray);
    switch (type) {
        case W3D_VIEW_MESHES: {
            MeshResource* r = W3dMeshResourceByName(id);
            if (!r || r->deleted) return false;
            if (that) *that = T("New object with this 3D mesh");
            if (onlyTest) return true;
            // PRIMERO que el recurso cargue: lo vincula una malla de trabajo FUERA de la escena (una .w3db rota o
            // que falta no deja un objeto vacio en la escena, sin undo, ni toca la seleccion). Se suelta recien
            // con el objeto ya vinculado: el recurso no se descarga en el medio.
            Mesh* test = NULL;
            if (!r->Loaded()) {
                test = W3dTempNewMesh();
                if (!W3dMeshLink(test, r, false)) {
                    W3dTempMeshDelete(test);
                    if (that) *that = T("The 3D mesh couldn't be loaded");
                    return false;
                }
            }
            // un OBJETO malla nuevo con ESE recurso (comparte la malla en memoria), en el punto del rayo
            UndoGroupStart();
            Mesh* m = new Mesh(NULL, PointOfRelease(t, ray));
            // (el objeto de una malla de una LIBRERIA se llama como la malla SIN el prefijo: un nombre de objeto con
            //  '/' no se puede nombrar en una ruta de clip)
            const size_t bar = r->lib.empty() ? std::string::npos : r->name.find_last_of('/');
            m->SetNameObj(bar == std::string::npos ? r->name : r->name.substr(bar + 1));
            const bool link = W3dMeshLink(m, r, false);
            if (test) W3dTempMeshDelete(test);
            if (!link) {
                // (no deberia: ya cargo) el objeto nuevo no queda en la escena
                std::vector<Object*>& ch = (m->Parent ? m->Parent : SceneCollection)->Childrens;
                for (size_t i = 0; i < ch.size(); i++) if (ch[i] == m) { ch.erase(ch.begin() + (long)i); break; }
                m->Parent = NULL;
                DeselectAll();
                W3dFreeSubtree(m);
                ActiveObj = NULL;
                UndoGroupEnd();
                if (that) *that = T("The 3D mesh couldn't be loaded");
                return false;
            }
            if (!m->modifiers.empty()) m->GenerateMeshModified();
            DeselectAll();
            m->Select();
            UndoCaptureCreation();   // Ctrl+Z lo saca (el recurso vuelve a como estaba)
            UndoGroupEnd();
            g_redraw = true;
            w3dLogf("[biblioteca] malla '%s' soltada en el 3D -> '%s'", id.c_str(), m->name.c_str());
            return true;
        }
        case W3D_VIEW_SOUNDS: {
            // un SONIDO soltado en la vista 3D: un objeto Sonido con ese archivo, donde cayo
            if (that) *that = T("New sound object here");
            if (onlyTest) return true;
            UndoGroupStart();
            Sound* so = new Sound(NULL, PointOfRelease(t, ray));
            so->file = id;
            so->SetNameObj(LeafNoExt(id));
            DeselectAll();
            so->Select();
            UndoCaptureCreation();
            UndoGroupEnd();
            g_redraw = true;
            return true;
        }
        case W3D_VIEW_MATERIALS: {
            Material* mat = W3dMaterialById(id);
            if (!mat) return false;
            if (!t.m || t.part < 0) { if (that) *that = T("Drop it on a mesh"); return false; }
            if (FromLib(t.m)) { if (that) *that = T("read-only library"); return false; }
            if (that) *that = std::string(T("Material of")) + " " + t.m->name;
            if (onlyTest) { PreviewViewSet(t.m, t.part, type, id, mat); return true; }
            UndoCaptureMaterial(t.m, t.part);
            t.m->materialsGroup[(size_t)t.part].material = mat;
            // EL MATERIAL ES DATO DE LA MALLA (como en Properties): si es un recurso compartido lo ven TODOS sus
            // objetos. Sin esto el otro usuario seguia con el viejo y al guardar el recurso se partia en dos
            if (t.m->mesh && t.part < (int)t.m->mesh->parts.size())
                W3dMeshResourceChangeMaterial(t.m->mesh, t.part, mat);
            g_redraw = true;
            return true;
        }
        case W3D_VIEW_SHADERS: {
            // un SHADER: el material de la PARTE tocada lo usa (el tilde "Avanzado: shader personalizado" prendido). Con
            // el material POR DEFECTO (que no se edita) la parte recibe un material NUEVO con ese shader
            if (!t.m || t.part < 0) { if (that) *that = T("Drop it on a mesh"); return false; }
            Material* mat = t.m->materialsGroup[(size_t)t.part].material;
            if (FromLib(t.m) || (mat && !mat->lib.empty())) { if (that) *that = T("read-only library"); return false; }
            const bool newMat = (!mat || mat == MaterialDefault);
            if (that) *that = newMat ? std::string(T("New material with this shader")) : std::string(T("Shader of")) + " " + mat->name;
            if (onlyTest) { PreviewViewSet(t.m, t.part, type, id, NULL); return true; }
            UndoCaptureMaterial(t.m, t.part);
            if (newMat) {
                extern Material* NewMaterialInMeshPart(Mesh*, int);
                mat = NewMaterialInMeshPart(t.m, t.part);
                if (!mat) { if (that) *that = T("The default material can't be edited"); return false; }
            }
            mat->shaderFile = id; mat->shaderOn = true;
            W3dShaderMaterialCompile(mat);
            extern void RebindMaterialMeshPart(); RebindMaterialMeshPart();
            g_redraw = true;
            w3dLogf("[biblioteca] shader '%s' soltado en '%s' (parte %d)", id.c_str(), t.m->name.c_str(), t.part);
            return true;
        }
        case W3D_VIEW_TEXTURES: {
            if (!t.m || t.part < 0) { if (that) *that = T("Drop it on a mesh"); return false; }
            Material* mat = t.m->materialsGroup[(size_t)t.part].material;
            if (FromLib(t.m) || (mat && !mat->lib.empty())) { if (that) *that = T("read-only library"); return false; }
            // con el material POR DEFECTO (que no se edita): la parte recibe un material NUEVO con esa textura
            const bool newMat = (!mat || mat == MaterialDefault);
            if (that) *that = newMat ? std::string(T("New material with this texture")) : std::string(T("Texture of")) + " " + mat->name;
            if (onlyTest) return true;
            if (newMat) {
                extern Material* NewMaterialInMeshPart(Mesh*, int);
                mat = NewMaterialInMeshPart(t.m, t.part);
                if (!mat) { if (that) *that = T("The default material can't be edited"); return false; }
            }
            Texture* tex = W3dLoadedTextureOf(id);
            const bool taken = (tex == NULL);   // (la cargo este gesto: su referencia es temporal)
            if (!tex) tex = TextureTake(id);
            if (!tex) { if (that) *that = T("The texture couldn't be loaded"); return false; }
            W3dMaterialSetTexture(mat, tex);   // (la ranura toma la suya)
            if (taken) TextureRelease(tex);
            return true;
        }
        case W3D_VIEW_ANIMATIONS: {
            // (un clip suelto es "animset/clip"; un animset de una LIBRERIA tambien lleva '/': su prefijo)
            if (id.find('/') != std::string::npos && !W3dAnimSetIsFromLib(id)) { if (that) *that = T("Drop the library, not a clip"); return false; }
            Armature* a = t.m ? ArmatureOf(t.m) : NULL;
            if (!a) { if (that) *that = T("Drop it on a character with an armature"); return false; }
            if (FromLib(a)) { if (that) *that = T("read-only library"); return false; }
            if (!a->animations.empty() && !W3dArmatureAnimsShared(a)) { if (that) *that = T("The armature has its own clips"); return false; }
            if (that) *that = std::string(T("Animations of")) + " " + a->name;
            if (onlyTest) return true;
            UndoAnimArm* u = new UndoAnimArm();
            u->a = a; u->other = AnimSetOf(a);
            std::string mot;
            if (!W3dArmatureAssignAnimSet(a, id, NULL, &mot)) {
                SetAnimSet(a, u->other);
                delete u;
                if (that) *that = std::string(T("Not assigned")) + ": " + mot;
                return false;
            }
            UndoExternal f; f.apply = UndoAnimArmApply; f.unlink = UndoAnimArmUnlink; f.free = UndoAnimArmFree;
            UndoPushExternal(f, u);
            g_redraw = true;
            return true;
        }
        case W3D_VIEW_SCENES:
        case W3D_VIEW_PREFABS: {
            // un prefab o una escena de una LIBRERIA: un PROXY donde cae el rayo
            std::string lib, elem; int typeEl = W3D_LIB_PREFAB;
            if (W3dLibsByKey(id, &lib, &typeEl, &elem)) {
                std::string reason;
                if (!W3dProxyCanAdd(lib, typeEl, elem, &reason)) { if (that) *that = T(reason.c_str()); return false; }
                if (that) *that = T("New W3D proxy of this element");
                if (onlyTest) return true;
                if (!W3dProxyAdd(lib, typeEl, elem, PointOfRelease(t, ray), &reason)) { if (that) *that = T(reason.c_str()); return false; }
                w3dLogf("[biblioteca] '%s' de la libreria '%s' soltado en el 3D (proxy)", elem.c_str(), lib.c_str());
                return true;
            }
            // una INSTANCIA del prefab -o de la ESCENA del proyecto ("escena:<nombre>", io/Prefabs.h)- donde cae el
            // rayo (sobre la superficie tocada o el plano del cursor 3D)
            const bool isScene = (type == W3D_VIEW_SCENES);
            const std::string key = isScene ? W3dPrefabKeyScene(id) : id;
            std::string reason;
            if (!W3dPrefabCanAdd(key, &reason)) { if (that) *that = T(reason.c_str()); return false; }
            if (that) *that = T(isScene ? "New instance of this scene" : "New instance of this prefab");
            if (onlyTest) return true;
            if (!W3dPrefabAdd(key, PointOfRelease(t, ray), &reason)) { if (that) *that = T(reason.c_str()); return false; }
            w3dLogf("[biblioteca] %s '%s' soltado en el 3D", isScene ? "scene" : "prefab", id.c_str());
            return true;
        }
    }
    if (that) *that = T("This resource can't be dropped in the 3D view");
    return false;
}

// ============================================================================
//  UN ARCHIVO SOLTADO DESDE EL SISTEMA (Windows / Linux / Mac: arrastrarlo desde el explorador a la ventana)
//    - sobre el VIEWPORT 3D: un modelo (.obj .fbx .gltf .glb .stl) se importa a la escena; una imagen se suma a la
//      biblioteca y va al material de la malla que queda bajo el puntero;
//    - sobre la BIBLIOTECA (o cualquier otro panel): queda en la biblioteca del proyecto, en memoria hasta guardar el
//      .w3d. Un modelo pasa a ser un PREFAB (no queda en la escena); imagenes, sonidos, scripts, fuentes y videos, su
//      recurso de siempre.
// ============================================================================
#include "io/W3dContainer.h"           // W3dImportAsset / W3dCategoryByExtension
#include "Outliner.h"                  // la biblioteca bajo el puntero: a que CARPETA va lo soltado
#include "io/lectura-escritura.h"      // ImportModelByExtension (la puerta unica de los modelos)
static unsigned int SerialMaxRec(Object* o) {
    if (!o) return 0;
    unsigned int m = o->serial;
    for (size_t i = 0; i < o->Childrens.size(); i++) { const unsigned int s = SerialMaxRec(o->Childrens[i]); if (s > m) m = s; }
    return m;
}
// selecciona lo de ARRIBA de lo que nacio en el import (serial > 'antes'), donde lo haya colgado (la coleccion activa)
static void SelectNew(Object* o, unsigned int prev, Object** first) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->serial > prev && h->getType() != ObjectType::collection) { h->Select(); if (!*first) *first = h; }
        else SelectNew(h, prev, first);
    }
}
static std::string ExtOf(const std::string& r) {
    const size_t p = r.find_last_of('.');
    std::string e = p == std::string::npos ? std::string() : r.substr(p + 1);
    for (size_t i = 0; i < e.size(); i++) if (e[i] >= 'A' && e[i] <= 'Z') e[i] = (char)(e[i] + 32);
    return e;
}
static std::string LeafOf(const std::string& r) {
    const size_t p = r.find_last_of("/\\");
    return p == std::string::npos ? r : r.substr(p + 1);
}
// la CARPETA de la biblioteca donde cae lo soltado: la carpeta bajo el puntero, la de la fila apuntada o, en la
// cuadricula, la que se esta viendo ("" = la raiz; tambien si no es una biblioteca)
static std::string FolderUnder(ViewportBase* under, int mx, int my) {
    if (!under || !under->isLeaf() || !VpIsOutliner(under)) return std::string();
    Outliner* o = (Outliner*)under;
    if (!o->InLibrary() || o->ViewReadOnly()) return std::string();
    std::vector<OutRowRec> rows; o->RowsResources(rows);
    const int i = o->RowInPoint(rows, mx, my);
    if (i >= 0 && rows[(size_t)i].folder && !rows[(size_t)i].back) return rows[(size_t)i].key;
    if (i >= 0) return rows[(size_t)i].folderOf;
    return o->grid ? o->folderGrid : std::string();
}
// el TIPO de la biblioteca de un archivo del proyecto (por su categoria)
static int CategoryType(const std::string& c) {
    if (c == "textures") return W3D_VIEW_TEXTURES;
    if (c == "sounds")  return W3D_VIEW_SOUNDS;
    if (c == "scripts")  return W3D_VIEW_SCRIPTS;
    if (c == "fonts")  return W3D_VIEW_FONTS;
    if (c == "videos")   return W3D_VIEW_VIDEOS;
    if (c == "shaders")  return W3D_VIEW_SHADERS;
    return -1;
}
bool W3dReleaseFile(const std::string& path0, int mx, int my, std::string* that) {
    if (that) that->clear();
    // (Windows suelta "C:\carpeta\foto.png" con barras invertidas: la biblioteca nombra todo con '/', y Windows abre
    //  igual con '/')
    std::string path = path0;
    for (size_t i = 0; i < path.size(); i++) if (path[i] == '\\') path[i] = '/';
    extern ViewportBase* rootViewport;
    ViewportBase* under = rootViewport ? FindViewportUnderMouse(rootViewport, mx, my) : NULL;
    const bool in3D = under && under->isLeaf() && under->ViewportKind() == 1;
    const std::string folder = in3D ? std::string() : FolderUnder(under, mx, my);
    const std::string ext = ExtOf(path);
    const std::string cat = W3dCategoryByExtension(path);
    const bool isModel = (ext == "obj" || ext == "fbx" || ext == "gltf" || ext == "glb" || ext == "stl");
    if (!isModel && cat != "textures" && cat != "sounds" && cat != "scripts" && cat != "fonts" && cat != "videos" && cat != "shaders") {
        if (that) *that = std::string(T("This file type can't be added")) + ": " + LeafOf(path);
        return false;
    }
    if (isModel) {
        if (InteractionMode != ObjectMode || state != editNavigation) { if (that) *that = T("Only in Object Mode"); return false; }
        const unsigned int prev = SerialMaxRec(SceneCollection);
#ifndef W3D_SYMBIAN
        if (!ImportModelByExtension(path)) { if (that) *that = std::string(T("Could not import")) + ": " + LeafOf(path); return false; }
#else
        if (that) *that = std::string(T("Could not import")) + ": " + LeafOf(path); return false;   // (el telefono no importa modelos sueltos)
#endif
        if (in3D) { if (that) *that = std::string(T("Imported")) + ": " + LeafOf(path); return true; }
        // a la BIBLIOTECA: lo que nacio en este import pasa a un prefab nuevo y la instancia se va de la escena
        DeselectAll();
        Object* first = NULL;
        SelectNew(SceneCollection, prev, &first);
        if (!first) { if (that) *that = std::string(T("Imported")) + ": " + LeafOf(path); return true; }
        ActiveObj = first;
        std::string name, reason;
        if (!W3dPrefabCreateFromSelection(&name, &reason)) { if (that) *that = T(reason.c_str()); return false; }
        Remove(false);   // (la instancia que dejo el prefab: el modelo queda solo en la biblioteca)
        if (!folder.empty()) W3dViewRecMove(W3D_VIEW_PREFABS, name, folder);   // (a la carpeta donde se solto)
        if (that) *that = std::string(T("Prefab created")) + ": " + name;
        w3dLogf("[biblioteca] '%s' soltado: prefab '%s'", path.c_str(), name.c_str());
        return true;
    }
    // un ARCHIVO de afuera: queda afuera (referencia externa); a meterlo adentro del .w3d, desde la Biblioteca
    const std::string entry = W3dLoadExternal(path, cat.c_str());
    if (entry.empty()) { if (that) *that = std::string(T("Could not import")) + ": " + LeafOf(path); return false; }
    if (cat == "textures") {
        Texture* tex = TextureTake(entry);   // (cargada: la biblioteca la conserva aunque nadie la use todavia)
        if (!tex) { if (that) *that = T("The texture couldn't be loaded"); return false; }
        bool placed = false;
        if (in3D) placed = W3dReleaseResourceIn3D((Viewport3D*)under, mx, my, W3D_VIEW_TEXTURES, entry, false, that);
        TextureRelease(tex);
        W3dResourcesViewInvalidate();
        if (!placed && !folder.empty()) W3dViewRecMove(W3D_VIEW_TEXTURES, entry, folder);
        if (!placed && that) *that = std::string(T("Added to the library")) + ": " + LeafOf(path) + (that->empty() ? "" : " (" + *that + ")");
        g_redraw = true;
        return true;
    }
    W3dResourcesViewInvalidate();
    // un SONIDO soltado en la vista 3D: ademas de entrar a la biblioteca, un objeto Sonido donde cayo
    if (cat == "sounds" && in3D) {
        std::string q2;
        W3dReleaseResourceIn3D((Viewport3D*)under, mx, my, W3D_VIEW_SOUNDS, entry, false, &q2);
        if (that) *that = std::string(T("New sound object here")) + ": " + LeafOf(path);
        g_redraw = true;
        return true;
    }
    // un SHADER soltado sobre una malla: ademas de entrar a la biblioteca, el material de la parte tocada lo usa
    if (cat == "shaders" && in3D) {
        std::string q2;
        if (W3dReleaseResourceIn3D((Viewport3D*)under, mx, my, W3D_VIEW_SHADERS, entry, false, &q2)) {
            if (that) *that = q2 + ": " + LeafOf(path);
            g_redraw = true;
            return true;
        }
    }
    if (!folder.empty() && CategoryType(cat) >= 0) W3dViewRecMove(CategoryType(cat), entry, folder);
    if (that) *that = std::string(T("Added to the library")) + ": " + LeafOf(path);
    g_redraw = true;
    return true;
}
