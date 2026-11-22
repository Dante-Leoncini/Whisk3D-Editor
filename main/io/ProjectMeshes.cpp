// ============================================================================
//  ProjectMeshes.cpp — el lado del EDITOR de las mallas 3D como recurso.
//  Ver ProjectMeshes.h y el contrato en libs/Whisk3DCore/objects/MeshResource.h.
// ============================================================================
#include "W3dRoots.h"          // las mallas de TODAS las raices (escenas/prefabs), en orden de registro
#include "io/ProjectMeshes.h"
#include "io/W3dContainer.h"          // el escritor del contenedor + W3dSlugEntry
#include "io/SaveW3D.h"               // g_w3dIndices16Simulated
#include "io/W3dMesh.h"               // W3dMeshMissingBlock / W3dMeshSaneName
#include "io/W3dMeshBin.h"
#include "objects/MeshResource.h"
#include "objects/Mesh.h"
#include "objects/Objects.h"
#include "objects/ObjectMode.h"        // W3dDuplicateOne
#include "objects/Primitives.h"        // W3dMeshSpawnedHook: el Add de una primitiva nace con su malla 3D
#include "animation/VertexAnimation.h" // W3dRestVertexAnim: la malla se serializa EN REPOSO
#include "edit/MeshEdit.h"             // W3dMeshCanonicalIndices: el index buffer que arma el .w3dm al abrir
#include "undo/Undo.h"                 // UndoPushExternal: los pasos de undo de los vinculos
#include "base/W3dInteractionState.h"  // InteractionMode (Edit / Weight / Vertex paint)
#include "W3dNotice.h"                 // W3dAvisof / W3dShortName
#include "W3dNames.h"                  // LA regla de nombres unicos (el nombre de una malla nueva)
#include "io/W3dResources.h"           // el id de un recurso en el almacen cambia con su entrada
#include "importers/import_w3d.h"      // W3dFormatMeshes (texto/binario) + la malla de trabajo fuera de la escena
#include "io/Libraries.h"              // W3dLibRefOf: una malla vacia porque su libreria no esta
#include "w3dlog.h"
#include <map>
#include <set>
#include <algorithm>

extern bool g_redraw;
extern Object* g_editMesh;

// EL ESTADO DEL EDITOR (lo resetea W3dMeshesEditorForget al cerrar el proyecto: son punteros a
// mallas del arbol que se destruye)
static Mesh* gInEdit = NULL;     // la malla (de un recurso) que esta en edicion ahora
// LA ACTIVA QUE SE MIRA: la malla activa con su edicion en memoria (sus listas en Properties, el
// editor UV) y la firma de esa edicion al materializarla ("" = no la materializo el tick: al
// soltarla se compara contra el recurso)
static Mesh* gActiveView = NULL;
static std::string gSignatureView;
// PUBLICACION DIFERIDA: mallas que salieron de Edit (o de un modo de pintura) siendo la UNICA usuaria de su
// recurso. Nadie mas lo mira, asi que publicar al salir solo pagaba W3dMeshSerialize entero (61 ms por 30K
// verts en PC, segundos en el N95) para un recurso que no lee nadie, y el re-vinculo de despues tiraba la jaula
// de edicion (rearmarla al volver a entrar es otro tanto). La malla sigue con SUS arrays y su jaula (lo que se
// ve es lo suyo) y se publica cuando alguien necesita el recurso: el guardado (W3dMeshesSync), otro
// objeto que se vincula a el o la malla que se re-vincula (hook del Core) y la miniatura de la biblioteca.
static std::vector<Mesh*> gPending;
static void RemovePending(const Mesh* m);   // (abajo, con el tick)

// ============================================================================
//  SERIALIZAR / PUBLICAR
// ============================================================================
bool W3dMeshSerialize(Mesh* m, const std::string& name, std::string& out, std::vector<std::string>* notices) {
    out.clear();
    if (!m) return false;
    // los mismos frenos que el guardado de siempre (WriteMeshW3dm): nada se hornea a medias
    if (m->w3dmForeign.notLoaded) {
        if (notices) notices->push_back(T("its geometry could not be read when opening"));
        return false;
    }
    const std::string missing = W3dMeshMissingBlock(m);
    if (!missing.empty()) {
        if (notices) {
            char b[384];   // (las traducciones en cirilico ocupan el doble de bytes)
            snprintf(b, sizeof(b), T("its mesh needs the block '%s' from a newer version"), W3dShortName(missing).c_str());
            notices->push_back(b);
        }
        return false;
    }
    const int nRV = m->vertex ? m->vertexSize : 0;
    if (!W3dMeshBinIndicesReach(nRV) || (g_w3dIndices16Simulated && nRV > 65535)) {
        if (notices) notices->push_back(T("it has more than 65535 vertices and this platform uses 16-bit indices"));
        return false;
    }
    if (!m->pendingEdit.empty() && !W3dMeshBinMaterializeEdit(m)) {
        if (notices) notices->push_back(T("could not read its faces"));
        return false;
    }
    // el reposo del skinning 2D es dato (ver WriteMeshW3dm)
    if (m->HasArm2D()) m->Armature2DRestCapture();
    std::string b;
    {
        W3dRestVertexAnim rest(m);   // la malla EN REPOSO, no en la pose del playhead
        m->RestUVAnimStrip();
        W3dMeshBinIndices canon;
        W3dMeshCanonicalIndices(m, canon);
        if (!W3dMeshBinWrite(m, &canon, b, notices)) return false;
    }
    if (!W3dMeshBinRename(b, name, out)) out.swap(b);
    return true;
}

// el contenido de dos .w3db sin mirar el nombre (INFO): lo que se compara para el dedup
static std::string KeyContent(const std::string& b) {
    std::string k;
    if (!W3dMeshBinRename(b, "", k)) return b;
    return k;
}

// LOS BYTES DEL RECURSO TAL COMO ES HOY: los de su entrada (o en memoria) con el nombre de
// material de cada parte (PART) puesto al dia. El material es dato de la malla y cambiarlo en
// la tarjeta no reescribe los bytes (W3dMeshResourceChangeMaterial): sin esto, comparar una
// malla serializada contra su recurso daria "distinta" solo por el material.
static bool ResourceBytes(const MeshResource* r, std::string& out) {
    if (!W3dMeshResourceReadBytes(r, out)) return false;
    if (r->materials.size() != r->parts.size() || r->parts.empty()) return true;
    std::vector<std::string> names, inFile;
    for (size_t k = 0; k < r->materials.size(); k++)
        names.push_back((r->materials[k] && !r->materials[k]->name.empty())
                          ? W3dMeshSaneName(r->materials[k]->name, NULL, "de un material") : std::string());
    std::string t;
    if (W3dMeshBinMaterials(out, inFile) && inFile != names && W3dMeshBinChangeMaterials(out, names, t))
        out.swap(t);
    return true;
}

// el material de cada parte de la malla (lo que el recurso guarda por puntero)
static void MaterialsOf(const Mesh* m, std::vector<Material*>& out) {
    out.clear();
    for (size_t k = 0; k < m->materialsGroup.size(); k++) out.push_back(m->materialsGroup[k].material);
}

// la malla tiene su EDICION en memoria (caras, o geometria suelta: aristas / vertices sin caras) y es la del
// recurso con el que se vincula (una copia de la de su original): se conserva al vincular
static bool HasEdit(const Mesh* m) {
    return m->pendingEdit.empty() && (!m->faces3d.empty() || !m->looseEdges.empty() || !m->looseVerts.empty());
}

// la malla esta viva y vinculada? (sin desreferenciar un puntero que pudo morir: se busca en
// las listas de usuarios del registro, que ~Mesh mantiene al dia)
static bool UserLive(const Mesh* m) {
    if (!m) return false;
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    for (size_t i = 0; i < reg.size(); i++)
        for (size_t k = 0; k < reg[i]->users.size(); k++) if (reg[i]->users[k] == m) return true;
    // (y las mallas de las LIBRERIAS, que no estan en el registro del proyecto. Sin esto el Ctrl+Y de un "New Copy" de
    //  una malla de una libreria -tambien el de desvincularla- no hacia nada. Solo se COMPARAN punteros: 'm' puede ser
    //  uno viejo)
    const std::vector<MeshResource*>& lib = W3dMeshesLib();
    for (size_t i = 0; i < lib.size(); i++)
        for (size_t k = 0; k < lib[i]->users.size(); k++) if (lib[i]->users[k] == m) return true;
    return false;
}

static bool Is(const std::vector<Mesh*>& v, const Mesh* m) {
    return std::find(v.begin(), v.end(), m) != v.end();
}

// los usuarios con modificadores geometricos se regeneran (el Core no puede)
static void RegenerateUsers(MeshResource* r, const std::vector<Mesh*>& exceptions) {
    std::vector<Mesh*> us = r->users;
    for (size_t i = 0; i < us.size(); i++)
        if (us[i] && !Is(exceptions, us[i]) && !us[i]->modifiers.empty()) us[i]->GenerateMeshModified();
}

// los usuarios que se quedan con LO SUYO cuando 'r' toma otros datos: el que publica y la malla
// en EDIT MODE (su jaula y sus capas son suyas: re-apuntarla las tiraria en medio de la edicion)
static void ExceptedOf(const MeshResource* r, Mesh* isPublic, std::vector<Mesh*>& out) {
    out.clear();
    if (isPublic) out.push_back(isPublic);
    if (gInEdit && gInEdit != isPublic && UserLive(gInEdit) && gInEdit->mesh == r)
        out.push_back(gInEdit);
}

// LAS VERTEX ANIMS son POR OBJETO e indexan los render-verts de su malla: si lo que se publica
// cambia cuantos hay, re-apuntar a los OTROS usuarios animados a la topologia nueva las dejaria
// muertas para siempre (sus cuadros miden la vieja: EvalVertexAnim no las evalua) y asi se
// guardarian. Esos usuarios se APARTAN a una copia del recurso con el contenido VIEJO (con un
// aviso): siguen animando igual y los demas ven la edicion. Antes de los recursos, editar un
// objeto nunca tocaba la geometria de otro; esto conserva eso para lo que no se puede remapear.
static void SetAsideAnimated(MeshResource* r, int nNewRV, const std::vector<Mesh*>& exceptions) {
    if (!r || !r->Loaded() || nNewRV == r->vertexSize) return;
    std::vector<Mesh*> animated;
    for (size_t i = 0; i < r->users.size(); i++) {
        Mesh* u = r->users[i];
        if (u && !Is(exceptions, u) && !u->animations.empty()) animated.push_back(u);
    }
    if (animated.empty()) return;
    std::string old;
    if (!ResourceBytes(r, old)) {
        w3dLogfW("[mallas] no pude leer '%s' para apartar a sus objetos con vertex anims", r->name.c_str());
        return;
    }
    MeshResource* copy = W3dMeshNewResource(r->name, "", r->folder);
    std::string b2;
    if (W3dMeshBinRename(old, copy->name, b2)) old.swap(b2);
    W3dMeshResourceSetBytes(copy, old);
    copy->materials = r->materials;
    int n = 0;
    for (size_t i = 0; i < animated.size(); i++) {
        Mesh* u = animated[i];
        if (!W3dMeshLink(u, copy, false)) continue;   // RePose le devuelve su cuadro
        if (!u->modifiers.empty()) u->GenerateMeshModified();
        n++;
    }
    w3dLogfW("[mallas] '%s' cambio de topologia: %d objeto(s) con vertex anims quedan con la malla anterior ('%s')",
             r->name.c_str(), n, copy->name.c_str());
    if (n > 0)
        W3dAvisof(false, T("%d object(s) with vertex anims keep the previous mesh: '%s'"),
                  n, W3dShortName(copy->name).c_str());
}

// el recurso toma los datos 'nuevos' que publica 'm' (ya serializados y distintos): los animados
// que no calzan se apartan, todos los demas usuarios se re-apuntan y los que tienen modificadores
// se regeneran
static bool PublishContent(MeshResource* r, Mesh* m, const std::string& newValue) {
    std::vector<Mesh*> ex;
    ExceptedOf(r, m, ex);
    SetAsideAnimated(r, m->vertex ? m->vertexSize : 0, ex);
    std::vector<Material*> mats;
    MaterialsOf(m, mats);
    if (!W3dMeshResourceApplyBytes(r, newValue, ex, &mats)) return false;
    RegenerateUsers(r, ex);
    return true;
}

// se puede re-apuntar la malla a los arrays del recurso SIN cambiar lo que se ve? No si la
// sesion la tiene POSADA: una vertex anim (vertex[] con la pose del playhead), un flipbook (uv[]
// corrido al cuadro actual) o un rig 2D (uv[] deformado). Esas se quedan con su copia (siguen
// siendo usuarias del recurso); guardar o publicar no puede mover lo que muestra el viewport.
static bool CanRelink(const Mesh* m) {
    return m && m->animations.empty() && !m->flipbook && !m->HasArm2D();
}

// la malla que publica, FUERA de edicion, vuelve a compartir los arrays del recurso (conserva
// su edicion: acaba de salir de ella y es la misma malla)
static void ReturnToShare(Mesh* m) {
    if (!m || !m->mesh || m == (Mesh*)g_editMesh) return;
    if (!CanRelink(m)) return;
    if (m->dataComp) return;   // ya comparte
    const bool keeps = m->pendingEdit.empty();
    W3dMeshLink(m, m->mesh, keeps);
    if (!m->modifiers.empty()) m->GenerateMeshModified();
}

bool W3dMeshPublish(Mesh* m) {
    if (!m || !m->mesh) return false;
    RemovePending(m);   // publicada por cualquier camino (Alt+D, guardado...): ya no tiene nada diferido
    MeshResource* r = m->mesh;
    if (W3dMeshClean(m)) return false;   // nada propio que pueda haber cambiado
    std::string newValue;
    std::vector<std::string> notices;
    if (!W3dMeshSerialize(m, r->name, newValue, &notices)) {
        w3dLogfW("[mallas] no pude publicar '%s' en la malla '%s': %s", m->name.c_str(), r->name.c_str(),
                 notices.empty() ? "?" : notices[0].c_str());
        return false;
    }
    std::string old;
    if (ResourceBytes(r, old) && KeyContent(old) == KeyContent(newValue)) {
        ReturnToShare(m);   // no cambio nada: vuelve a compartir (memoria)
        return false;
    }
    if (!PublishContent(r, m, newValue)) {
        w3dLogfE("[mallas] la malla '%s' no acepto la edicion de '%s'", r->name.c_str(), m->name.c_str());
        return false;
    }
    ReturnToShare(m);
    w3dLogf("[mallas] '%s' publicada desde '%s' (%d usuario(s))", r->name.c_str(), m->name.c_str(),
            (int)r->users.size());
    g_redraw = true;
    return true;
}

MeshResource* W3dMeshCreateResource(Mesh* m, const std::string& name) {
    if (!m) return NULL;
    std::string b;
    if (!W3dMeshSerialize(m, name, b, NULL)) return NULL;
    MeshResource* r = W3dMeshNewResource(name, "", m->mesh ? m->mesh->folder : std::string());
    std::string b2;
    if (W3dMeshBinRename(b, r->name, b2)) b.swap(b2);
    W3dMeshResourceSetBytes(r, b);
    MaterialsOf(m, r->materials);
    // la malla SERIALIZADA es esta: su edicion coincide con el recurso
    if (!W3dMeshLink(m, r, m->pendingEdit.empty())) return NULL;
    return r;
}

// ============================================================================
//  UNDO DE LOS VINCULOS (Hacer unica / elegir otra malla / una malla SUELTA que pasa a tener
//  su recurso): el paso guarda la malla y el OTRO estado (un recurso, o NULL = suelta);
//  aplicar = intercambiar. El paso RETIENE al recurso que guarda (refsUndo): la purga de los
//  recursos de la sesion no lo borra mientras el paso exista, asi que su puntero es estable;
//  la malla se suelta si se libera (el gancho de ~Object la pone en NULL).
// ============================================================================
struct LinkUndoData { Mesh* m; MeshResource* other; };

// la malla deja su recurso y queda SUELTA con su copia de todo (lo que ve hoy: su edicion y sus
// arrays). El Ctrl+Z de "una malla suelta paso a tener su recurso" (New Copy / Rename / Folder /
// Alt+D / el selector sobre una suelta).
static bool ReturnLoose(Mesh* m) {
    if (!m || !m->mesh) return true;
    if (!m->pendingEdit.empty() && !W3dMeshBinMaterializeEdit(m)) return false;
    m->UninstantiateData(W3DMD_ALL, true);
    W3dMeshUnlink(m);
    return true;
}

static void LinkUndoApply(void* d) {
    LinkUndoData* v = (LinkUndoData*)d;
    Mesh* m = v->m;
    if (!m) return;
    MeshResource* cur = m->mesh;   // NULL = la malla esta suelta
    if (cur && !UserLive(m)) return;
    if (v->other) {
        if (!W3dMeshLink(m, v->other, false)) return;
    } else if (!cur || !ReturnLoose(m)) return;
    if (!m->modifiers.empty()) m->GenerateMeshModified();
    // el paso ahora guarda el OTRO: retiene al que deja la malla y suelta al que ella tomo
    W3dMeshResourceUndoRetain(cur);
    W3dMeshResourceUndoRelease(v->other);
    v->other = cur;
    g_redraw = true;
}
static void LinkUndoUnlink(void* d, Object* deleted) {
    LinkUndoData* v = (LinkUndoData*)d;
    if ((Object*)v->m == deleted) v->m = NULL;
}
static void LinkUndoFree(void* d) {
    LinkUndoData* v = (LinkUndoData*)d;
    W3dMeshResourceUndoRelease(v->other);   // si quedo sin usuarios, lo purga el proximo tick
    delete v;
}
// 'anterior' = el recurso que la malla tenia (NULL = estaba SUELTA: el Ctrl+Z la vuelve suelta)
static void PushLinkUndo(Mesh* m, MeshResource* prev) {
    if (!m) return;
    LinkUndoData* v = new LinkUndoData();
    v->m = m; v->other = prev;
    W3dMeshResourceUndoRetain(prev);
    UndoExternal f; f.apply = LinkUndoApply; f.unlink = LinkUndoUnlink; f.free = LinkUndoFree;
    UndoPushExternal(f, v);
}

MeshResource* W3dMeshCreateResourceWithUndo(Mesh* m, const std::string& name, const std::string& folder) {
    if (!m) return NULL;
    if (m->mesh) return m->mesh;
    MeshResource* r = W3dMeshCreateResource(m, name);
    if (!r) return NULL;
    r->folder = W3dMeshFolderNormalize(folder);
    PushLinkUndo(m, NULL);   // Ctrl+Z: la malla vuelve a estar suelta
    g_redraw = true;
    return r;
}

bool W3dMeshShared(const Mesh* m) {
    return m && m->mesh && W3dMeshResourceUsersInScene(m->mesh) > 1;
}

// 'm' pasa a una COPIA de su recurso (nombre libre), con undo
static MeshResource* CopyResourceFor(Mesh* m) {
    MeshResource* old = m->mesh;
    // los datos: los del recurso si la malla no se toco; si no, la malla como esta
    std::string b;
    bool ok = W3dMeshClean(m) && ResourceBytes(old, b);
    if (!ok) ok = W3dMeshSerialize(m, old->name, b, NULL);
    if (!ok) return NULL;
    // (la copia de una malla de una LIBRERIA externa es del PROYECTO: sin el prefijo de la libreria ni su carpeta, que
    //  son de alla; asi se edita y se guarda como cualquier otra)
    std::string base = old->name, folder = old->folder;
    if (!old->lib.empty()) {
        const size_t bar = base.find('/');
        if (bar != std::string::npos && bar + 1 < base.size()) base = base.substr(bar + 1);
        folder.clear();
    }
    MeshResource* newValue = W3dMeshNewResource(base, "", folder);
    std::string b2;
    if (W3dMeshBinRename(b, newValue->name, b2)) b.swap(b2);
    W3dMeshResourceSetBytes(newValue, b);
    MaterialsOf(m, newValue->materials);
    if (!W3dMeshLink(m, newValue, m->pendingEdit.empty())) return NULL;
    if (!m->modifiers.empty()) m->GenerateMeshModified();
    PushLinkUndo(m, old);
    g_redraw = true;
    return newValue;
}

MeshResource* W3dMeshMakeUnique(Mesh* m) {
    if (!m || !m->mesh) return NULL;          // una malla suelta ya es unica
    if (W3dMeshResourceUsersInScene(m->mesh) <= 1) return m->mesh;
    return CopyResourceFor(m);
}

MeshResource* W3dMeshNewCopy(Mesh* m) {
    if (!m) return NULL;
    // una malla SUELTA pasa a tener su recurso (con su paso de undo: Ctrl+Z la vuelve suelta)
    if (!m->mesh) return W3dMeshCreateResourceWithUndo(m, m->name, std::string());
    return CopyResourceFor(m);
}

bool W3dMeshAssign(Mesh* m, MeshResource* r) {
    if (!m || !r) return false;
    if (m->mesh == r) return true;
    // lo que la malla tenga SIN PUBLICAR (un grupo nuevo, una capa) es de SU recurso: se publica
    // antes de irse (editar la malla de un objeto edita el recurso). Sin esto el cambio se perdia
    // y el Ctrl+Z la devolvia sin el.
    if (m->mesh && m != gInEdit && !W3dMeshClean(m)) W3dMeshPublish(m);
    // UN Ctrl+Z: una malla suelta primero pasa a tener SU recurso (su propio paso: asi el Ctrl+Z
    // la vuelve suelta, con lo que tenia) y despues elige el otro
    UndoGroupStart();
    if (!m->mesh && !W3dMeshCreateResourceWithUndo(m, m->name, std::string())) { UndoGroupSequentialEnd(); return false; }
    MeshResource* prev = m->mesh;
    if (!W3dMeshLink(m, r, false)) {
        UndoGroupSequentialEnd();
        W3dAvisof(true, T("Could not load the mesh '%s'"), W3dShortName(r->name).c_str());
        return false;
    }
    if (!m->modifiers.empty()) m->GenerateMeshModified();
    PushLinkUndo(m, prev);
    UndoGroupSequentialEnd();
    g_redraw = true;
    return true;
}

// ============================================================================
//  TODA MALLA ES UN RECURSO (la biblioteca: nunca hay mallas "sin recurso")
// ============================================================================
static unsigned gSpawnedAseg = 0, gAsegDesv = 0;
static int gAsegRoots = -1;
static std::set<unsigned int> gAsegFailed;   // seriales que no se pudieron serializar (no se reintenta)
static void CollectLoose(Object* o, std::vector<Mesh*>& out) {
    if (!o) return;
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::proxy) continue;   // (lo que genera un proxy es de su libreria: no del proyecto)
        // (una malla VACIA porque su malla es de una libreria que no esta no es suelta: tiene su referencia, ver
        //  io/Libraries.h; darle un recurso propio vacio la perderia)
        if (h->getType() == ObjectType::mesh && !((Mesh*)h)->mesh && !gAsegFailed.count(h->serial) &&
            !W3dLibRefOf(h, W3D_LIBREF_MESH)) out.push_back((Mesh*)h);
        CollectLoose(h, out);
    }
}
int W3dMeshesEnsureResources() {
    // (jugando no: lo que un script cree durante el Play se va con el Stop)
    extern bool SimActive();
    if (SimActive()) return 0;
    std::vector<Object*> roots;
    W3dLiveRoots(roots);
    const unsigned nac = W3dObjectsSpawned(), des = W3dMeshesUnlinked();
    if (nac == gSpawnedAseg && des == gAsegDesv && (int)roots.size() == gAsegRoots) return 0;
    std::vector<Mesh*> loose;
    for (size_t i = 0; i < roots.size(); i++) CollectLoose(roots[i], loose);
    gSpawnedAseg = nac; gAsegDesv = des; gAsegRoots = (int)roots.size();
    int n = 0;
    // las SUELTAS DE UN ARCHIVO VIEJO iguales comparten UN recurso (mil arboles -> una malla)
    std::map<std::string, MeshResource*> byContent;
    for (size_t i = 0; i < loose.size(); i++) {
        Mesh* m = loose[i];
        if (m->mesh) continue;
        std::string b;
        std::vector<std::string> notices;
        if (!W3dMeshSerialize(m, m->name, b, &notices)) {
            gAsegFailed.insert(m->serial);
            w3dLogfW("[mallas] '%s' no pudo tener su malla 3D: %s", m->name.c_str(), notices.empty() ? "?" : notices[0].c_str());
            continue;
        }
        const std::string key = m->dedupByContent ? KeyContent(b) : std::string();
        if (!key.empty()) {
            std::map<std::string, MeshResource*>::iterator it = byContent.find(key);
            if (it != byContent.end() && W3dMeshLink(m, it->second, HasEdit(m))) {
                if (!m->modifiers.empty()) m->GenerateMeshModified();
                n++;
                continue;
            }
        }
        MeshResource* r = W3dMeshNewResource(m->name, "", std::string());
        std::string b2;
        if (W3dMeshBinRename(b, r->name, b2)) b.swap(b2);
        W3dMeshResourceSetBytes(r, b);
        MaterialsOf(m, r->materials);
        if (!W3dMeshLink(m, r, m->pendingEdit.empty())) {
            gAsegFailed.insert(m->serial);
            continue;
        }
        if (!key.empty()) byContent[key] = r;
        n++;
    }
    if (n > 0) { w3dLogf("[mallas] %d malla(s) sin recurso pasaron a tener el suyo", n); g_redraw = true; }
    return n;
}

// el Add de una primitiva (objects/Primitives.cpp): su malla 3D en el acto, no recien en el proximo cuadro
static void MeshSpawned() { W3dMeshesEnsureResources(); }
namespace { struct HookMeshSpawned { HookMeshSpawned() { W3dMeshSpawnedHook = MeshSpawned; } } gHookMeshSpawned; }

// "Duplicate" de la biblioteca: una copia del recurso, sin usuarios, que se CONSERVA (un huerfano
// pedido). Con undo: deshacer la saca del registro y rehacer la vuelve a crear con esos bytes.
struct UndoCreatedMesh {
    MeshResource* r;
    std::string name, folder, bytes;
    std::vector<Material*> mats;
    bool live;
};
static void UndoCreatedMeshApply(void* d) {
    UndoCreatedMesh* u = (UndoCreatedMesh*)d;
    if (u->live) {
        if (!u->r || !W3dMeshResourceDelete(u->r)) return;   // (alguien la empezo a usar: se queda)
        u->r = NULL;
        u->live = false;
    } else {
        MeshResource* r = W3dMeshNewResource(u->name, "", u->folder);
        W3dMeshResourceSetBytes(r, u->bytes);
        r->materials = u->mats;
        r->keep = true;
        u->r = r;
        u->live = true;
    }
    g_redraw = true;
}
static void UndoCreatedMeshFree(void* d) { delete (UndoCreatedMesh*)d; }
MeshResource* W3dMeshDuplicateResource(MeshResource* r) {
    if (!r) return NULL;
    std::string b;
    // lo que el recurso ES hoy: si un usuario lo esta editando sin publicar, se publica antes
    for (size_t i = 0; i < r->users.size(); i++)
        if (r->users[i] && r->users[i] != gInEdit && !W3dMeshClean(r->users[i])) { W3dMeshPublish(r->users[i]); break; }
    if (!ResourceBytes(r, b)) return NULL;
    MeshResource* n = W3dMeshNewResource(r->name, "", r->folder);
    std::string b2;
    if (W3dMeshBinRename(b, n->name, b2)) b.swap(b2);
    W3dMeshResourceSetBytes(n, b);
    n->materials = r->materials;
    n->keep = true;
    UndoCreatedMesh* u = new UndoCreatedMesh();
    u->r = n; u->name = n->name; u->folder = n->folder; u->bytes = b; u->mats = n->materials; u->live = true;
    UndoExternal f; f.apply = UndoCreatedMeshApply; f.free = UndoCreatedMeshFree;
    UndoPushExternal(f, u);
    g_redraw = true;
    return n;
}

// ============================================================================
//  DUPLICAR (Alt+D vinculado / Shift+D con recurso propio)
// ============================================================================
Mesh* W3dMeshDuplicateLinked(Mesh* src) {
    if (!src) return NULL;
    // una malla SUELTA pasa a tener su recurso (con su paso: el que llama lo junta con la
    // creacion de la copia en un Ctrl+Z, que saca la copia y la vuelve suelta)
    if (!src->mesh && !W3dMeshCreateResourceWithUndo(src, src->name, std::string())) return NULL;
    // lo que src muestra tiene que ser lo del recurso (una edicion sin publicar se publica)
    if (!W3dMeshClean(src)) W3dMeshPublish(src);
    Mesh* d = (Mesh*)W3dDuplicateOne(src);   // copia todo lo del OBJETO (y la geometria de paso)
    if (!d) return NULL;
    // ...y la geometria pasa a ser la del recurso (comparte arrays y VBO). Su edicion es una
    // copia de la de src, que coincide con el recurso: se conserva si la hay.
    d->noEditable  = src->noEditable;
    d->onlyRoutine = src->onlyRoutine;
    d->lockNormals = src->lockNormals;
    if (!W3dMeshLink(d, src->mesh, HasEdit(d))) {
        W3dAvisof(true, T("Could not link the copy of '%s' to its mesh"), W3dShortName(src->name).c_str());
    }
    if (!d->modifiers.empty()) d->GenerateMeshModified();
    return d;
}

Mesh* W3dMeshDuplicateWithOwnResource(Mesh* src) {
    if (!src || !src->mesh) return NULL;
    MeshResource* r = src->mesh;
    std::string b;
    bool ok = W3dMeshClean(src) && ResourceBytes(r, b);
    if (!ok) ok = W3dMeshSerialize(src, r->name, b, NULL);
    Mesh* d = (Mesh*)W3dDuplicateOne(src);
    if (!d || !ok) return d;   // sin bytes: la copia queda suelta (el guardado le da recurso)
    MeshResource* newValue = W3dMeshNewResource(r->name, "", r->folder);
    std::string b2;
    if (W3dMeshBinRename(b, newValue->name, b2)) b.swap(b2);
    W3dMeshResourceSetBytes(newValue, b);
    MaterialsOf(src, newValue->materials);
    d->noEditable  = src->noEditable;
    d->onlyRoutine = src->onlyRoutine;
    d->lockNormals = src->lockNormals;
    W3dMeshLink(d, newValue, HasEdit(d));
    if (!d->modifiers.empty()) d->GenerateMeshModified();
    return d;
}

// ============================================================================
//  EL TICK DEL EDITOR
// ============================================================================
void W3dMeshesEditorForget() {
    gInEdit = NULL;
    gActiveView = NULL;
    gSignatureView.clear();
    gPending.clear();
}

// (ver gPending) solo se COMPARAN punteros hasta confirmar con UserLive: una malla destruida sale de los
// usuarios de su recurso en ~Mesh y su puntero puede quedar colgando en la lista
static bool IsPending(const Mesh* m) {
    for (size_t i = 0; i < gPending.size(); i++) if (gPending[i] == m) return true;
    return false;
}
static void RemovePending(const Mesh* m) {
    for (size_t i = 0; i < gPending.size(); i++)
        if (gPending[i] == m) { gPending.erase(gPending.begin() + (long)i); return; }
}
// diferir la publicacion solo si NADIE MAS usa el recurso: con otra instancia, la edicion se tiene que ver ya
static bool CanDefer(const Mesh* m) {
    const MeshResource* r = m ? m->mesh : NULL;
    return r && r->users.size() == 1 && r->users[0] == m;
}
static void PublishPending(Mesh* m) {
    RemovePending(m);   // ANTES de publicar: publicar re-vincula y el hook del Core no tiene que volver a entrar
    if (UserLive(m)) W3dMeshPublish(m);
}
// las pendientes que tocan a 'm' (la malla misma) o a 'r' (su recurso): el Core lo llama antes de sumarle un usuario
// a un recurso o de re-vincular una malla (W3dMeshBeforeLinkHook), la miniatura antes de leerlo
void W3dMeshesPublishPendingOf(Mesh* m, MeshResource* r) {
    if (gPending.empty()) return;
    std::vector<Mesh*> pub;
    for (size_t i = 0; i < gPending.size(); i++) {
        Mesh* p = gPending[i];
        if (p == m || (r && UserLive(p) && p->mesh == r)) pub.push_back(p);
    }
    for (size_t i = 0; i < pub.size(); i++) PublishPending(pub[i]);
}
struct PendingHookReg {
    PendingHookReg() { W3dMeshBeforeLinkHook = W3dMeshesPublishPendingOf; }
};
static PendingHookReg gPendingHookReg;

static Mesh* EditContext() {
    if (!ActiveObj || ActiveObj->getType() != ObjectType::mesh) return NULL;
    Mesh* a = (Mesh*)ActiveObj;
    if (!a->mesh) return NULL;   // una malla suelta no tiene a quien publicar
    if (g_editMesh == ActiveObj) return a;
    if (InteractionMode == WeightPaint || InteractionMode == VertexPaint) return a;
    return NULL;
}

// la malla ACTIVA si es de un recurso (NULL = no hay, o es suelta: la suelta tiene su edicion)
static Mesh* ActiveWithResource() {
    if (!ActiveObj || ActiveObj->getType() != ObjectType::mesh) return NULL;
    Mesh* a = (Mesh*)ActiveObj;
    return a->mesh ? a : NULL;
}

// la edicion de la malla esta en memoria DE VERDAD? (materializada, no una que fallo al leerse
// y quedo vacia: esa no se puede publicar ni comparar)
static bool EditInMemory(const Mesh* m) {
    return m->pendingEdit.empty() && (!m->faces3d.empty() || m->facesSize == 0);
}

// LA FIRMA DE LA EDICION materializada (caras, capas con su nombre y tamano, marcas) y de lo que
// comparte con el recurso: si al soltarla da lo mismo que al materializarla, nada suyo cambio y
// se suelta sin serializar. Barata (conteos y nombres): corre una vez por cambio de activa.
static void SignatureNum(std::string& s, long v) { char b[24]; snprintf(b, sizeof(b), "%ld;", v); s += b; }
static std::string SignatureEdit(const Mesh* m) {
    std::string s;
    SignatureNum(s, (long)m->dataComp);
    SignatureNum(s, (long)m->faces3d.size());
    SignatureNum(s, (long)m->looseEdges.size()); SignatureNum(s, (long)m->looseVerts.size());
    SignatureNum(s, (long)m->sharpEdges.size()); SignatureNum(s, (long)m->seamEdges.size());
    SignatureNum(s, (long)m->cornerNormal.size());
    SignatureNum(s, (long)m->uvActiveMap);
    for (size_t i = 0; i < m->uvMaps.size(); i++) {
        if (!m->uvMaps[i]) { s += "-;"; continue; }
        s += m->uvMaps[i]->name; s += '\x1f'; SignatureNum(s, (long)m->uvMaps[i]->uv.size());
    }
    SignatureNum(s, (long)m->activeColor);
    for (size_t i = 0; i < m->colorLayers.size(); i++) {
        if (!m->colorLayers[i]) { s += "-;"; continue; }
        s += m->colorLayers[i]->name; s += '\x1f';
        SignatureNum(s, (long)m->colorLayers[i]->color.size()); SignatureNum(s, m->colorLayers[i]->perVertex ? 1L : 0L);
    }
    return s;
}

// LA ACTIVA QUE SE MIRABA dejo de serlo: su edicion vuelve a quedar PENDIENTE en su recurso (la
// memoria que se ahorra con los recursos no se pierde por haber seleccionado). Si algo suyo
// cambio y no se publico (una lista de Properties, una capa, un grupo nuevo), antes se PUBLICA:
// editar la malla de un objeto edita el recurso, y asi ningun otro usuario lo pisa despues.
static void ReleaseView(Mesh* v, const std::string& signature) {
    if (!v || !v->mesh || v == gInEdit || v == (Mesh*)g_editMesh) return;
    if (IsPending(v)) return;   // publicacion diferida: se queda con lo suyo hasta que alguien pida el recurso
    if (!v->pendingEdit.empty()) return;   // ya estaba pendiente
    const bool equal = !signature.empty() && SignatureEdit(v) == signature && W3dMeshExtrasAsResource(v);
    if (!equal) {
        if (!EditInMemory(v)) return;        // una edicion que no se pudo leer no se publica
        W3dMeshPublish(v);
    }
    W3dMeshReleaseEdit(v);   // (no-op si algo suyo sigue siendo propio: una pose, el flipbook)
}

void W3dMeshesTickEditor() {
    // 0) los recursos de la sesion que ya nadie usa ni nombra (ver W3dMeshesPurgeUnused)
    W3dMeshesPurgeUnused();
    // ...y TODA malla con el suyo (la que nacio en este cuadro: un Add, un import, un Separate)
    W3dMeshesEnsureResources();
    Mesh* ctx = EditContext();
    Mesh* act = ActiveWithResource();
    // (las pendientes que se destruyeron salen de la lista sin tocarlas; la que dejo de ser la UNICA usuaria de su
    //  recurso -le aparecio otra instancia por un camino que no paso por el hook- se publica ya: la otra la tiene que ver)
    if (!gPending.empty()) {
        std::vector<Mesh*> shared;
        for (size_t i = 0; i < gPending.size(); ) {
            if (!UserLive(gPending[i])) { gPending.erase(gPending.begin() + (long)i); continue; }
            if (!CanDefer(gPending[i])) shared.push_back(gPending[i]);
            i++;
        }
        for (size_t i = 0; i < shared.size(); i++) PublishPending(shared[i]);
    }
    // 1) SALIR DE EDICION: la malla que sale se publica en su recurso (la ven todos). Si es la UNICA usuaria,
    //    la publicacion se difiere (ver gPending): sale al instante y conserva su jaula
    if (gInEdit != ctx && gInEdit) {
        Mesh* sale = gInEdit;
        gInEdit    = NULL;
        if (UserLive(sale)) {
            if (CanDefer(sale)) { if (!IsPending(sale)) gPending.push_back(sale); }
            else W3dMeshPublish(sale);
        }
    }
    // 2) LA ACTIVA QUE SE MIRABA dejo de serlo: se suelta (publicando antes lo que tenga). ANTES
    //    de entrar a editar otra: la que entra re-apunta a lo publicado y lo ve
    if (gActiveView && gActiveView != act) {
        Mesh* v = gActiveView;
        const std::string signature = gSignatureView;
        gActiveView = NULL; gSignatureView.clear();
        if (UserLive(v)) ReleaseView(v, signature);
    }
    // 3) ENTRAR A EDICION: la malla que se va a editar tiene su edicion en memoria y SUS arrays
    //    (copia): ninguna operacion de edicion puede escribir la geometria de los demas usuarios
    if (gInEdit != ctx && ctx) {
        gInEdit = ctx;
        RemovePending(ctx);   // vuelve a edicion con lo suyo (arrays y jaula): nada que materializar ni copiar
        if (!ctx->pendingEdit.empty()) W3dMeshBinMaterializeEdit(ctx);
        ctx->UninstantiateData(W3DMD_ALL, true);
    }
    // 4) LAS MALLAS QUE UNA OPERACION EDITO FUERA DE EDICION (Apply Modifier, Shade,
    //    un Ctrl+Z de geometria, una lista de Properties...): se publican ya
    std::vector<Mesh*> ed;
    W3dMeshesTakeEdited(ed);
    for (size_t i = 0; i < ed.size(); i++) {
        Mesh* m = ed[i];
        if (!m || m == gInEdit || !m->mesh) continue;   // la de edicion se publica al salir
        if (IsPending(m)) continue;                     // la diferida sigue esperando a que pidan el recurso
        W3dMeshPublish(m);
    }
    // 5) LA ACTIVA (y SOLO ella) necesita su edicion: sus listas en Properties (UV maps, capas de
    //    color) y el editor UV. Lo demas seleccionado se MIRA con la topologia compartida del
    //    recurso (contorno, snap, seleccion por caja): materializar a todo lo seleccionado no se
    //    soltaba nunca (Select All sobre mil arboles = +90 MB para siempre).
    // (las PENDIENTES que dejaron de ser la activa sueltan la jaula: memoria del N95. Sus arrays ya tienen todo lo
    //  editado -RefreshRender empuja la jaula a vertex[] en cada operacion- y la siguen dibujando)
    for (size_t i = 0; i < gPending.size(); i++) {
        Mesh* p = gPending[i];
        if (p != act && p != (Mesh*)g_editMesh && p->edit) p->InvalidateEdit();
    }
    if (act && act != gInEdit && act != gActiveView && !IsPending(act)) {
        if (!act->pendingEdit.empty()) {
            if (W3dMeshBinMaterializeEdit(act)) { gActiveView = act; gSignatureView = SignatureEdit(act); }
        } else if (EditInMemory(act)) {
            // ya estaba en memoria (acaba de salir de edicion, un Ctrl+Z): sin firma, soltarla la
            // compara contra el recurso
            gActiveView = act; gSignatureView.clear();
        }
    }
}

void W3dMeshesSync() {
    // las de publicacion diferida: lo que se ve es lo que se guarda
    { std::vector<Mesh*> pend(gPending); for (size_t i = 0; i < pend.size(); i++) PublishPending(pend[i]); }
    if (gInEdit && UserLive(gInEdit) && gInEdit->mesh) {
        // publicar SIN salir de edicion: la malla se queda con lo suyo (su jaula no se toca)
        Mesh* m = gInEdit;
        MeshResource* r = m->mesh;
        if (!W3dMeshClean(m)) {
            std::string newValue, old;
            if (W3dMeshSerialize(m, r->name, newValue, NULL) &&
                !(ResourceBytes(r, old) && KeyContent(old) == KeyContent(newValue)))
                PublishContent(r, m, newValue);
        }
    }
    std::vector<Mesh*> ed;
    W3dMeshesTakeEdited(ed);
    for (size_t i = 0; i < ed.size(); i++)
        if (ed[i] && ed[i] != gInEdit && ed[i]->mesh) W3dMeshPublish(ed[i]);
}

// ============================================================================
//  EL GUARDADO
// ============================================================================
namespace {
struct Group {
    std::string name;          // el nombre con que se escribe
    std::string folder;
    std::string entry;           // la entrada en el contenedor NUEVO
    MeshResource* rec;           // el recurso que conserva su identidad (NULL = uno nuevo)
    bool resourceContent;        // los bytes son los de 'rec' (no se tocaron)
    std::string bytes;           // los bytes a escribir ("" = los de 'rec', a leer o copiar)
    std::string key;             // contenido sin nombre (dedup); "" = todavia no se calculo
    std::vector<Mesh*> meshes;   // los objetos que lo usan (en el orden de la escena)
    std::vector<Material*> mats; // material de cada parte
    int first;                   // orden de la escena del primer objeto (orden del registro)
    bool orphan;
    const MeshResource* origin;  // un recurso NUEVO que se separo de este (usuario tocado)
    // nacio de una malla SUELTA que vino de un archivo viejo (Mesh::dedupByContent): solo con
    // estos grupos se juntan por contenido las demas sueltas de archivo
    bool joinable;
    // (huerfano) los nombres de material que dicen los bytes de su PART
    std::vector<std::string> partNames;
    // (huerfano) su entrada no se pudo leer (falta en el contenedor o esta rota): no se escribe
    // ni se registra, y el proyecto se guarda igual (con un aviso)
    bool lost;
    bool text;                  // se escribe en TEXTO (.w3dm) y no en binario (.w3db): W3dMeshResourceInText
    Group() : rec(0), resourceContent(false), first(0), orphan(false), origin(0),
              joinable(false), lost(false), text(false) {}
};
struct ByFirst {
    const std::vector<Group>* gs;
    bool operator()(size_t a, size_t b) const { return (*gs)[a].first < (*gs)[b].first; }
};
}

static std::vector<Group> gPlan;
static std::vector<size_t> gOrder;                 // gPlan en el orden del registro
static std::map<const Mesh*, size_t> gPlanOf;      // malla -> su grupo
static bool gPlanReady = false;
// RE-HORNEAR: fuerza a TODAS las mallas por el camino de serializacion (PASADA B), ignorando la copia del
// contenido del recurso sin tocar. Lo usa el guardado de "optimizar para juego" (convertir a no-editable):
// el .w3db sale re-escrito con el skinning agrupado COMPACTO horneado, aunque el render no haya cambiado.
bool g_w3dRebakeMeshes = false;
// LOS BORRADOS QUE EL UNDO RETIENE (MeshResource::borrado) y todavia nombran su entrada del .w3d que este
// guardado reemplaza: no se escriben, pero un Ctrl+Z despues de guardar les devuelve sus objetos. Sus bytes se
// leen del contenedor VIEJO al preparar (serial del recurso -> su .w3db; vacio = ya los tenia en memoria) y al
// confirmar quedan en memoria sin entrada: sin esto el recurso nombraba una entrada que ya no estaba y ningun
// guardado posterior salia ("no pude leer la malla")
static std::vector<std::pair<int, std::string> > gEvicted;

void W3dMeshesSaveDiscard() {
    g_w3dRebakeMeshes = false;   // el re-horneado es por-guardado: no se arrastra al siguiente
    gPlan.clear(); gOrder.clear(); gPlanOf.clear();
    gEvicted.clear();
    gPlanReady = false;
}

static void CollectMeshes(Object* o, std::vector<Mesh*>& out) {
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        // lo que genera un PROXY es de su libreria (no se guarda); y una malla del proyecto que usa una malla de una
        // LIBRERIA la nombra (SaveW3D): ninguna de las dos entra al plan
        if (h->getType() == ObjectType::proxy) continue;
        // (ni la que quedo vacia porque su malla es de una libreria que NO ESTA: tambien la nombra)
        if (h->getType() == ObjectType::mesh && !(((Mesh*)h)->mesh && !((Mesh*)h)->mesh->lib.empty()) &&
            !(!((Mesh*)h)->mesh && W3dLibRefOf(h, W3D_LIBREF_MESH)))
            out.push_back((Mesh*)h);
        CollectMeshes(h, out);
    }
}

// el nombre de material que el escritor pone en PART para esta parte
static std::string MaterialNamePart(const Material* mat) {
    if (!mat || mat->name.empty()) return std::string();
    return W3dMeshSaneName(mat->name, NULL, "de un material");
}

// la clave de contenido de un grupo (se calcula la primera vez que se compara)
static const std::string& KeyOf(Group& g) {
    if (g.key.empty()) {
        if (!g.bytes.empty()) g.key = KeyContent(g.bytes);
        else if (g.rec) {
            std::string b;
            if (ResourceBytes(g.rec, b)) g.key = KeyContent(b);
        }
        if (g.key.empty()) g.key = "\x01";   // ilegible: no coincide con nada
    }
    return g.key;
}

// ---------------------------------------------------------------------------
//  EL FORMATO DE TEXTO ("formatoMallas": "texto"): la misma entrada por malla, en .w3dm
// ---------------------------------------------------------------------------
// ancla de los render-verts (W3dMeshWrite): si ALGUN objeto de la malla tiene vertex anims,
// el orden de memoria es dato de todos (sus keyframes lo indexan)
static int AnchorOf(const Group& g) {
    for (size_t k = 0; k < g.meshes.size(); k++) if (g.meshes[k] && !g.meshes[k]->animations.empty()) return 1;
    return -1;
}
// el .w3dm desde un OBJETO del grupo: la malla tal como se ve (su edicion, en reposo). La linea
// "nombre" del archivo es la de la MALLA (el recurso), no la del objeto.
static bool MeshText(Mesh* m, const std::string& name, int anchor, std::string& txt,
                         std::vector<std::string>* notices) {
    // una malla de un recurso con la edicion pendiente se materializa para escribirla y despues
    // la suelta (igual que el camino de siempre, WriteMeshW3dm)
    const bool borrowed = m->mesh && !m->pendingEdit.empty();
    if (!m->pendingEdit.empty() && !W3dMeshBinMaterializeEdit(m)) {
        if (notices) notices->push_back(T("could not read its faces"));
        return false;
    }
    if (m->HasArm2D()) m->Armature2DRestCapture();   // el reposo 2D es dato (ver WriteMeshW3dm)
    const std::string nameObj = m->name;
    m->name = name;
    bool ok;
    {
        W3dRestVertexAnim rest(m);   // EN REPOSO, no en la pose del playhead
        m->RestUVAnimStrip();
        ok = W3dMeshWrite(m, txt, notices, anchor);
    }
    m->name = nameObj;
    if (borrowed) W3dMeshReleaseEdit(m);
    return ok;
}
// el .w3dm desde los BYTES (una malla huerfana: ningun objeto la usa): una malla de trabajo
// fuera de la escena la lee entera del .w3db y la escribe en texto
static bool BytesText(const std::string& b, const std::string& name, const std::vector<Material*>& mats,
                         std::string& txt, std::vector<std::string>* notices) {
    Mesh* t = W3dTempNewMesh();
    t->name = name;
    W3dMeshInfo info;
    W3dMeshBinOptions op;   // con la edicion: caras, capas, marcas
    bool ok = !b.empty() && W3dMeshBinRead((const unsigned char*)b.data(), b.size(), t, &info, op);
    if (ok) {
        for (size_t k = 0; k < t->materialsGroup.size(); k++)
            t->materialsGroup[k].material = (k < mats.size() && mats[k]) ? mats[k]
                                          : (k < info.materials.size() ? FindMaterialByName(info.materials[k]) : NULL);
        ok = W3dMeshWrite(t, txt, notices);
    } else if (notices) notices->push_back(T("could not read the mesh"));
    W3dTempMeshDelete(t);
    return ok;
}

static bool NameTaken(const std::string& n, const MeshResource* except) {
    for (size_t i = 0; i < gPlan.size(); i++) if (gPlan[i].name == n) return true;
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    for (size_t i = 0; i < reg.size(); i++) if (reg[i] != except && reg[i]->name == n) return true;
    return false;
}
static bool TakenNameFn(const std::string& n, void* ctx) { return NameTaken(n, (const MeshResource*)ctx); }

bool W3dMeshesSavePrepare(W3dContainerWriter* esc) {
    const bool rebake = g_w3dRebakeMeshes;   // leer ANTES de Descartar (que lo resetea)
    W3dMeshesSaveDiscard();
    if (!esc || !SceneCollection) return false;
    // lo que se ve es lo que se guarda: la malla en edicion y las editadas se publican antes
    W3dMeshesSync();
    W3dMeshesPurgeUnused();   // (los de la sesion sin usuarios ni undo no se escriben)

    // las mallas de TODAS las raices del proyecto (la escena activa y las escenas/prefabs que no se
    // estan mirando: el guardado las escribe a todas, W3dRoots.h), en el orden del REGISTRO: el
    // orden del registro "mallas" no puede depender de que escena se estaba mirando al guardar
    std::vector<Mesh*> meshes;
    {
        std::vector<Object*> roots;
        W3dRootsInOrder(roots);
        for (size_t i = 0; i < roots.size(); i++) CollectMeshes(roots[i], meshes);
    }
    bool ok = true;

    // PASADA A: los recursos que tienen al menos un usuario SIN TOCAR conservan su identidad
    // y su contenido (el grupo nace de ellos, en el orden del primer usuario)
    std::map<const MeshResource*, size_t> groupOf;
    std::map<const MeshResource*, std::string> keyRec;   // contenido de cada recurso (se lee una vez)
    std::set<const MeshResource*> inText;   // recursos de mallas que van por el camino de texto (16 bits)
    std::vector<char> resolved(meshes.size(), 0);
    for (size_t i = 0; i < meshes.size(); i++) {
        Mesh* m = meshes[i];
        if (!m->mesh || !W3dMeshClean(m)) continue;
        if (rebake) continue;   // forzar re-serializacion (PASADA B): hornear el skinning compacto
        // una malla que no entra en el .w3db de ESTA plataforma (indices de 16 bits) va por el
        // camino de texto aunque su recurso este sin tocar (ver WriteMeshW3dm)
        if (!W3dMeshBinIndicesReach(m->vertexSize) || (g_w3dIndices16Simulated && m->vertexSize > 65535)) {
            inText.insert(m->mesh);
            continue;
        }
        std::map<const MeshResource*, size_t>::iterator it = groupOf.find(m->mesh);
        size_t gi;
        if (it == groupOf.end()) {
            Group g;
            g.name = m->mesh->name; g.folder = m->mesh->folder;
            g.rec = m->mesh; g.resourceContent = true;
            g.mats = m->mesh->materials;
            g.first = (int)i;
            gPlan.push_back(g);
            gi = gPlan.size() - 1;
            groupOf[m->mesh] = gi;
        } else gi = it->second;
        gPlan[gi].meshes.push_back(m);
        gPlanOf[m] = gi;
        resolved[i] = 1;
    }
    // PASADA B: las tocadas y las sueltas se serializan y se agrupan POR CONTENIDO (dedup: mil
    // arboles iguales de un archivo viejo -> una sola malla)
    for (size_t i = 0; i < meshes.size(); i++) {
        if (resolved[i]) continue;
        Mesh* m = meshes[i];
        std::string b;
        std::vector<std::string> notices;
        const int nRV = m->vertex ? m->vertexSize : 0;
        // una malla que no entra en el .w3db de esta plataforma va por el camino de TEXTO
        // (WriteMeshW3dm, "geometria" propia): no entra al plan
        if (!m->w3dmForeign.notLoaded && W3dMeshMissingBlock(m).empty() &&
            (!W3dMeshBinIndicesReach(nRV) || (g_w3dIndices16Simulated && nRV > 65535))) {
            if (m->mesh) inText.insert(m->mesh);
            continue;
        }
        if (!W3dMeshSerialize(m, m->mesh ? m->mesh->name : m->name, b, &notices)) {
            // los frenos de siempre: el proyecto no se guarda y se dice por que
            w3dLogfE("[W3D] '%s': %s: NO guardo encima", m->name.c_str(), notices.empty() ? "?" : notices[0].c_str());
            W3dAvisof(true, T("Not saving '%s': %s"), W3dShortName(m->name).c_str(),
                      notices.empty() ? T("its mesh cannot be written") : notices[0].c_str());
            ok = false;
            continue;
        }
        for (size_t k = 0; k < notices.size(); k++) w3dLogfW("[W3D] '%s': %s", m->name.c_str(), notices[k].c_str());
        if (!notices.empty())
            W3dAvisof(true, notices.size() > 1 ? T("'%s': %s (and more: see whisk3d.log)") : "'%s': %s",
                      W3dShortName(m->name).c_str(), notices[0].c_str());
        const std::string key = KeyContent(b);
        size_t gi = (size_t)-1;
        MeshResource* r = m->mesh;
        if (r) {
            // UNA MALLA CON RECURSO se queda con SU recurso (no se mezcla con otro igual: un
            // Shift+D o un "Hacer unica" son a proposito). Igual a su recurso -> su grupo;
            // distinta (se toco sin publicarse) -> el recurso la adopta si nadie mas lo usa sin
            // tocar, o se separa en un recurso nuevo con el nombre siguiente (Arbol.001).
            std::map<const MeshResource*, std::string>::iterator ic = keyRec.find(r);
            if (ic == keyRec.end()) {
                std::string rb;
                ic = keyRec.insert(std::make_pair((const MeshResource*)r,
                                                   ResourceBytes(r, rb) ? KeyContent(rb) : std::string("\x01"))).first;
            }
            const bool equal = (ic->second == key);
            std::map<const MeshResource*, size_t>::iterator ig = groupOf.find(r);
            if (equal && ig != groupOf.end() && gPlan[ig->second].resourceContent) gi = ig->second;
            else if (ig == groupOf.end()) {
                Group g;
                g.first = (int)i;
                g.bytes.swap(b);
                g.key = key;
                MaterialsOf(m, g.mats);
                g.rec = r; g.name = r->name; g.folder = r->folder;
                g.resourceContent = equal;
                groupOf[r] = gPlan.size();
                gPlan.push_back(g);
                gi = gPlan.size() - 1;
            } else {
                // otro usuario tocado del MISMO recurso con el mismo contenido: juntos, sea con
                // el que ADOPTO el recurso (su grupo es el de 'r', con el contenido nuevo) o con
                // uno que se separo antes. Sin mirar el que adopto, dos usuarios con el mismo
                // cambio terminaban en dos recursos identicos (Arbol y Arbol.001).
                for (size_t k = 0; k < gPlan.size() && gi == (size_t)-1; k++)
                    if ((gPlan[k].origin == r || (gPlan[k].rec == r && !gPlan[k].resourceContent)) &&
                        KeyOf(gPlan[k]) == key) gi = k;
                if (gi == (size_t)-1) {
                    Group g;
                    g.first = (int)i;
                    g.bytes.swap(b);
                    g.key = key;
                    MaterialsOf(m, g.mats);
                    g.name = W3dUniqueName(r->name, "Mesh", TakenNameFn, (void*)0);
                    g.folder = r->folder;
                    g.origin = r;
                    gPlan.push_back(g);
                    gi = gPlan.size() - 1;
                }
            }
        } else {
            // UNA MALLA SUELTA. Si vino asi de un ARCHIVO VIEJO (una geometria por objeto,
            // Mesh::dedupByContent) se agrupa POR CONTENIDO con las otras sueltas de archivo
            // iguales: mil arboles iguales -> una sola malla. Una malla NUEVA de la sesion (una
            // primitiva, un Shift+D de una suelta, un import) tiene SU recurso: dos cubos iguales
            // no quedan vinculados porque si al guardar (editar uno no puede mover al otro).
            if (m->dedupByContent)
                for (size_t k = 0; k < gPlan.size() && gi == (size_t)-1; k++)
                    if (gPlan[k].joinable && KeyOf(gPlan[k]) == key) gi = k;
            if (gi == (size_t)-1) {
                Group g;
                g.first = (int)i;
                g.bytes.swap(b);
                g.key = key;
                MaterialsOf(m, g.mats);
                g.name = W3dUniqueName(m->name, "Mesh", TakenNameFn, (void*)0);
                g.joinable = m->dedupByContent;
                gPlan.push_back(g);
                gi = gPlan.size() - 1;
            }
        }
        gPlan[gi].meshes.push_back(m);
        gPlanOf[m] = gi;
        // el ORDEN del registro es el del primer usuario en la escena: una tocada (la activa, con
        // su edicion materializada) que se suma al grupo que armo en la PASADA A un usuario
        // POSTERIOR adelanta el grupo. Sin esto el orden dependia de que objeto estaba activo y
        // re-guardar sin cambios podia dar otro project.json
        if ((int)i < gPlan[gi].first) gPlan[gi].first = (int)i;
    }
    if (!ok) { W3dMeshesSaveDiscard(); return false; }

    // LOS HUERFANOS: los del ARCHIVO (tienen entrada) se conservan aunque nadie los use; los
    // creados en esta sesion que se quedaron sin usuarios (un Shift+D deshecho, un "Hacer
    // unica" que se volvio a asignar) no se escriben
    const std::vector<MeshResource*>& reg = W3dMeshesRegistry();
    const int base = (int)meshes.size();
    for (size_t i = 0; i < reg.size(); i++) {
        MeshResource* r = reg[i];
        if (groupOf.find(r) != groupOf.end()) continue;
        if (r->deleted) {
            // (el usuario lo borro: vive solo para el undo y no se escribe). Si nombra una entrada de este
            // .w3d, sus bytes se leen AHORA: el contenedor nuevo ya no la va a tener (ver gEvicted)
            if (!r->entry.empty()) {
                std::string b;
                if (r->bytes.empty() && !W3dMeshResourceReadBytes(r, b))
                    w3dLogfW("[W3D] la malla borrada '%s' (%s) no se pudo leer: un Ctrl+Z ya no la puede devolver",
                             r->name.c_str(), r->entry.c_str());
                else gEvicted.push_back(std::make_pair(r->serial, b));
            }
            continue;
        }
        // un huerfano PEDIDO (el "Duplicate" de la biblioteca) todavia sin entrada: se escribe de sus bytes
        if (r->entry.empty() && !r->keep) continue;
        if (inText.count(r)) continue;   // su objeto se guarda en texto: no es un huerfano
        Group g;
        g.name = r->name; g.folder = r->folder; g.rec = r;
        g.resourceContent = true; g.orphan = true;
        g.first = base + (int)i;
        // EL MATERIAL DE CADA PARTE aunque nunca se haya cargado en la sesion (sus PART lo
        // nombran): asi va al bloque "materiales" (sin esto, un material que solo usaba el
        // huerfano se perdia del proyecto) y un rename de la sesion llega a su PART
        if (W3dMeshResourceMaterials(r, &g.partNames)) g.mats = r->materials;
        else {
            // SU ENTRADA NO SE PUEDE LEER (falta en el contenedor o esta rota): no hay nada que
            // conservar. No frena el guardado (no habria forma de salir: el huerfano no se puede
            // quitar); se avisa y el proyecto se guarda sin esa malla.
            g.lost = true;
            w3dLogfW("[W3D] la malla huerfana '%s' (%s) no se pudo leer: no se guarda", r->name.c_str(), r->entry.c_str());
            W3dAvisof(true, T("The mesh '%s' (no objects) could not be read: saving without it"),
                      W3dShortName(r->name).c_str());
        }
        gPlan.push_back(g);
    }

    // ORDEN DEL REGISTRO: el de la escena (primer usuario), despues los huerfanos. Determinista.
    for (size_t i = 0; i < gPlan.size(); i++) gOrder.push_back(i);
    ByFirst cmp; cmp.gs = &gPlan;
    std::stable_sort(gOrder.begin(), gOrder.end(), cmp);

    // LAS ENTRADAS: una por grupo, "meshes/<slug(nombre)>.w3db" (-2, -3 si el slug choca); una malla en
    // TEXTO lleva la misma entrada en .w3dm (el registro y la dedup son los mismos). El formato es DE CADA
    // MALLA (su tarjeta Recurso: W3dMeshResourceInText); una que se separo de otra hereda el de esa
    for (size_t o = 0; o < gOrder.size(); o++) {
        Group& g = gPlan[gOrder[o]];
        if (g.lost) continue;
        g.text = W3dMeshResourceInText(g.rec ? g.rec : g.origin);
        const char* ext = g.text ? ".w3dm" : ".w3db";
        const std::string base = W3dSlugEntry(g.name.empty() ? std::string("mesh") : g.name);
        std::string name = "meshes/" + base + ext;
        for (int k = 2; esc->Has(name); k++) {
            char sf[16]; snprintf(sf, sizeof(sf), "-%d", k);
            name = "meshes/" + base + sf + ext;
        }
        g.entry = name;
        // los NOMBRES que el archivo tiene que llevar adentro: el del recurso (INFO) y el de
        // cada material (PART, por nombre: un material renombrado en la sesion)
        std::vector<std::string> matsName;
        for (size_t k = 0; k < g.mats.size(); k++) {
            // (un huerfano cuyo material no existe en esta escena conserva el nombre de su PART)
            if (!g.mats[k] && g.orphan && k < g.partNames.size()) matsName.push_back(g.partNames[k]);
            else matsName.push_back(MaterialNamePart(g.mats[k]));
        }
        // COPIA TAL CUAL: un recurso sin tocar, sin cambios de nombre, que ya estaba en el
        // contenedor con la misma entrada (no se lee ni se reescribe nada)
        if (g.resourceContent && g.rec && !g.rec->modified && g.rec->entry == name && g.bytes.empty()) {
            // un HUERFANO: lo que dicen sus bytes contra el nombre vivo de cada material (un
            // material renombrado en la sesion reescribe su PART); sin cargar y con usuarios no
            // puede pasar (un usuario lo carga)
            bool same = g.orphan ? (g.partNames == matsName) : !g.rec->Loaded();
            if (!same && !g.orphan) {
                same = (g.rec->parts.size() == matsName.size());
                for (size_t k = 0; k < matsName.size() && same; k++)
                    if (g.rec->parts[k].material != matsName[k]) same = false;
            }
            if (same && esc->CopyFromOld(name)) {
                // el nombre de INFO lo dice el registro; una copia sin tocar lo conserva
                continue;
            }
        }
        if (g.text) {
            // TEXTO: desde un objeto del grupo (la malla tal como se ve) o, si es huerfana, desde
            // sus bytes. La copia BINARIA del grupo (si la hay) queda para la memoria: al confirmar
            // es el cache de la conversion del .w3dm recien escrito (no se vuelve a convertir).
            std::string txt, b;
            std::vector<std::string> notices;
            bool okT;
            if (!g.meshes.empty()) okT = MeshText(g.meshes[0], g.name, AnchorOf(g), txt, &notices);
            else okT = (!g.bytes.empty() ? (b = g.bytes, true) : (g.rec && ResourceBytes(g.rec, b))) &&
                       BytesText(b, g.name, g.mats, txt, &notices);
            for (size_t k = 0; k < notices.size(); k++) w3dLogfW("[W3D] malla '%s': %s", g.name.c_str(), notices[k].c_str());
            if (!okT && g.orphan) {
                // (un huerfano ilegible: igual que en binario, se avisa y se guarda sin el)
                g.lost = true;
                w3dLogfW("[W3D] la malla huerfana '%s' no se pudo escribir en texto: no se guarda", g.name.c_str());
                W3dAvisof(true, T("The mesh '%s' (no objects) could not be read: saving without it"),
                          W3dShortName(g.name).c_str());
                continue;
            }
            if (!okT) {
                w3dLogfE("[W3D] la malla '%s' no se pudo escribir en texto", g.name.c_str());
                W3dAvisof(true, T("Not saving: the mesh '%s' could not be written (%s)"), W3dShortName(g.name).c_str(),
                          notices.empty() ? T("see whisk3d.log") : notices[0].c_str());
                ok = false;
                break;
            }
            if (!notices.empty())
                W3dAvisof(true, notices.size() > 1 ? T("'%s': %s (and more: see whisk3d.log)") : "'%s': %s",
                          W3dShortName(g.name).c_str(), notices[0].c_str());
            if (!g.bytes.empty()) {
                std::string t;
                if (W3dMeshBinRename(g.bytes, g.name, t)) g.bytes.swap(t);
                std::vector<std::string> inFile;
                if (W3dMeshBinMaterials(g.bytes, inFile) && inFile.size() == matsName.size() && inFile != matsName &&
                    W3dMeshBinChangeMaterials(g.bytes, matsName, t)) g.bytes.swap(t);
            }
            if (!esc->AddBytes(name, txt, false)) {
                w3dLogfE("GuardarW3D: no pude meter %s adentro del .w3d", name.c_str());
                ok = false;
                break;
            }
            continue;
        }
        // si no, los bytes (del recurso o serializados), con los nombres al dia
        std::string b;
        if (!g.bytes.empty()) b = g.bytes;
        else if (g.orphan && !ResourceBytes(g.rec, b)) {
            // (un huerfano ilegible: se avisa y se guarda sin el, ver arriba)
            g.lost = true;
            w3dLogfW("[W3D] la malla huerfana '%s' no se pudo leer: no se guarda", g.name.c_str());
            W3dAvisof(true, T("The mesh '%s' (no objects) could not be read: saving without it"),
                      W3dShortName(g.name).c_str());
            continue;
        }
        else if (!g.rec || !ResourceBytes(g.rec, b)) {
            w3dLogfE("[W3D] la malla '%s' no se pudo leer para guardarla", g.name.c_str());
            W3dAvisof(true, T("Not saving: could not read the mesh '%s'"), W3dShortName(g.name).c_str());
            ok = false;
            break;
        }
        std::string t;
        if (W3dMeshBinRename(b, g.name, t)) b.swap(t);
        std::vector<std::string> inFile;
        if (W3dMeshBinMaterials(b, inFile) && inFile.size() == matsName.size() && inFile != matsName &&
            W3dMeshBinChangeMaterials(b, matsName, t)) b.swap(t);
        g.bytes.swap(b);
        if (!esc->AddBytes(name, g.bytes, false)) {
            w3dLogfE("GuardarW3D: no pude meter %s adentro del .w3d", name.c_str());
            ok = false;
            break;
        }
    }
    if (!ok) { W3dMeshesSaveDiscard(); return false; }
    gPlanReady = true;
    return true;
}

std::string W3dMeshesSaveNameOf(const Mesh* m) {
    std::map<const Mesh*, size_t>::const_iterator it = gPlanOf.find(m);
    if (!gPlanReady || it == gPlanOf.end()) return std::string();
    return gPlan[it->second].name;
}

// un string JSON (mismo escape que el resto del guardado: comillas, barra y control)
static void JsonText(std::string& s, const std::string& v) {
    s += '"';
    for (size_t i = 0; i < v.size(); i++) {
        const unsigned char c = (unsigned char)v[i];
        if (c == '"' || c == '\\') { s += '\\'; s += (char)c; }
        else if (c == '\n') s += "\\n";
        else if (c == '\r') s += "\\r";
        else if (c == '\t') s += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", (unsigned)c); s += b; }
        else s += (char)c;
    }
    s += '"';
}

void W3dMeshesSaveRegistry(std::string& s) {
    if (!gPlanReady) return;
    std::vector<const Group*> gs;   // (sin los huerfanos que no se pudieron leer)
    for (size_t o = 0; o < gOrder.size(); o++) if (!gPlan[gOrder[o]].lost) gs.push_back(&gPlan[gOrder[o]]);
    if (gs.empty()) return;
    s += "  \"meshes\": [\n";
    for (size_t o = 0; o < gs.size(); o++) {
        const Group& g = *gs[o];
        s += "    {\"name\": "; JsonText(s, g.name);
        s += ", \"entry\": "; JsonText(s, g.entry);
        if (!g.folder.empty()) { s += ", \"folder\": "; JsonText(s, W3dCosmeticPathJson(g.folder)); }
        s += "}";
        s += (o + 1 < gs.size()) ? ",\n" : "\n";
    }
    s += "  ],\n";
}

void W3dMeshesSaveMaterials(std::vector<Material*>& out) {
    out.clear();
    if (!gPlanReady) return;
    for (size_t o = 0; o < gOrder.size(); o++) {
        const Group& g = gPlan[gOrder[o]];
        if (!g.orphan || g.lost) continue;
        for (size_t k = 0; k < g.mats.size(); k++) if (g.mats[k]) out.push_back(g.mats[k]);
    }
}

// LA CLAVE TEMPORAL de un recurso cargado mientras las entradas se mudan (no choca con ninguna
// entrada del contenedor ni con las "*<serial>" de los recursos sin entrada)
static std::string KeyMoving(const MeshResource* r) {
    char b[32]; snprintf(b, sizeof(b), "#mudando:%d", r->serial);
    return b;
}

// LAS ENTRADAS NUEVAS de los recursos que siguen ("" para los huerfanos que no se pudieron leer:
// ya no estan en el archivo), con su clave en el almacen. EN DOS PASOS: primero todos los
// cargados a una clave temporal y recien despues cada uno a la suya. De a uno, un recurso chocaba
// con la entrada VIEJA de otro que todavia no se habia mudado (dos mallas que intercambian
// nombres, un slug que cambia el orden): el cambio fallaba en silencio, el recurso quedaba con un
// descriptor ajeno y el proximo vinculo lo volvia a cargar ENCIMA de sus usuarios.
static void MoveEntries() {
    std::vector<MeshResource*> move;
    std::vector<std::string> newValue;
    for (size_t o = 0; o < gOrder.size(); o++) {
        const Group& g = gPlan[gOrder[o]];
        if (!g.rec) continue;
        const std::string e = g.lost ? std::string() : g.entry;
        if (g.rec->entry == e) continue;
        move.push_back(g.rec);
        newValue.push_back(e);
    }
    // (los BORRADOS que retiene el undo se quedan sin entrada: el archivo nuevo no la tiene)
    for (size_t i = 0; i < gEvicted.size(); i++) {
        MeshResource* r = W3dMeshResourceBySerial(gEvicted[i].first);
        if (!r || r->entry.empty()) continue;
        move.push_back(r);
        newValue.push_back(std::string());
    }
    for (size_t i = 0; i < move.size(); i++)
        if (move[i]->rec) W3dResourceMove(W3DREC_MESH, move[i]->rec->id, KeyMoving(move[i]));
    for (size_t i = 0; i < move.size(); i++) move[i]->entry = newValue[i];
    for (size_t i = 0; i < move.size(); i++) {
        MeshResource* r = move[i];
        // (la nueva solo puede estar ocupada por un descriptor SIN dato -una ListaCarga que pedia
        // esa entrada-, que se absorbe; si la ocupa un dato vivo el recurso sigue andando por su
        // descriptor, que el vinculo usa por puntero)
        if (r->rec && !W3dResourceMove(W3DREC_MESH, r->rec->id, r->IdStore()))
            w3dLogfE("[mallas] la clave '%s' del almacen esta ocupada: la malla '%s' sigue como '%s'",
                     r->IdStore().c_str(), r->name.c_str(), r->rec->id.c_str());
    }
}

void W3dMeshesSaveConfirm() {
    if (!gPlanReady) { W3dMeshesSaveDiscard(); return; }
    // la malla en edicion: el puntero del tick se valida (pudo quedar de un proyecto cerrado si
    // ningun tick corrio en el medio: sin viewport 3D en el layout, el harness)
    if (gInEdit && !UserLive(gInEdit)) gInEdit = NULL;
    Mesh* inEd = gInEdit ? gInEdit : (Mesh*)g_editMesh;
    MoveEntries();
    // los BORRADOS que retiene el undo viven desde ahora de sus bytes en memoria (lo unico que queda de ellos).
    // Si un Ctrl+Z les devuelve sus objetos vuelven a ser del proyecto como un huerfano del archivo (se conservan
    // y el proximo guardado los escribe); si no, la purga los borra cuando el undo los suelta
    for (size_t i = 0; i < gEvicted.size(); i++) {
        MeshResource* r = W3dMeshResourceBySerial(gEvicted[i].first);
        if (!r) continue;
        if (!gEvicted[i].second.empty()) r->bytes.swap(gEvicted[i].second);
        r->textBytes = false;   // (la conversion de un .w3dm ya no es un cache: su entrada no esta)
        r->modified = true;
        r->keep = true;
    }
    for (size_t o = 0; o < gOrder.size(); o++) {
        const size_t gi = gOrder[o];
        Group& g = gPlan[gi];
        if (g.lost) continue;   // (un huerfano ilegible: sin entrada, lo purga el proximo tick)
        MeshResource* r = g.rec;
        if (!r) {
            // un recurso NUEVO: sus datos estan en la entrada recien escrita
            r = W3dMeshNewResource(g.name, g.entry, g.folder);
            r->materials = g.mats;
        } else {
            if (!g.resourceContent) {
                // el recurso ADOPTA el contenido de un usuario tocado: sus usuarios pasan a los
                // datos nuevos. Se quedan con lo suyo la malla en edicion (hasta salir) y los
                // usuarios que el plan manda a OTRO recurso (se re-vinculan cuando les toque:
                // re-apuntarlos aca les cambiaba lo que muestran por el contenido adoptado)
                std::vector<Mesh*> ex;
                if (inEd) ex.push_back(inEd);
                for (size_t k = 0; k < r->users.size(); k++) {
                    std::map<const Mesh*, size_t>::const_iterator ip = gPlanOf.find(r->users[k]);
                    if (ip != gPlanOf.end() && ip->second != gi) ex.push_back(r->users[k]);
                }
                std::vector<Material*> mats = g.mats;
                W3dMeshResourceApplyBytes(r, g.bytes, ex, &mats);
                RegenerateUsers(r, ex);
            }
            if (!r->name.empty() && r->name != g.name) r->name = g.name;
        }
        if (g.text) {
            // TEXTO: la entrada es un .w3dm. Los bytes binarios que ya estan en memoria (los del
            // grupo, o los que el recurso tenia) son el CACHE de su conversion: no se vuelve a leer
            // el texto para dibujar ni para materializar.
            if (!g.bytes.empty()) r->bytes = g.bytes;
            r->textBytes = !r->bytes.empty();
        } else {
            // lo que se escribio ES el recurso: los bytes en memoria sobran (la entrada los tiene)
            r->bytes.clear();
            r->textBytes = false;
        }
        r->format = -1;   // (lo elegido en su tarjeta ya es lo que dice su entrada)
        r->modified = false;
        if (!g.mats.empty() && g.mats.size() == r->parts.size()) r->materials = g.mats;
        // los objetos del grupo pasan a usar ESTE recurso (los sueltos y los tocados comparten
        // desde ya; la malla en edicion sigue con lo suyo hasta salir)
        for (size_t k = 0; k < g.meshes.size(); k++) {
            Mesh* m = g.meshes[k];
            if (!m) continue;
            if (m->mesh == r && W3dMeshClean(m)) continue;
            if (m == (Mesh*)g_editMesh || m == gInEdit || !CanRelink(m)) {
                // la malla en EDICION (o pintando, o POSADA por una animacion) no se re-vincula:
                // su jaula, sus capas y lo que muestra siguen siendo suyos. Si su grupo es OTRO
                // recurso (una malla suelta, un usuario tocado que se separo), el vinculo se muda
                // sin tocar sus datos.
                if (m->mesh != r) W3dMeshMove(m, r);
                continue;
            }
            const bool keeps = m->pendingEdit.empty() && !m->faces3d.empty();
            if (W3dMeshLink(m, r, keeps) && !m->modifiers.empty()) m->GenerateMeshModified();
        }
    }
    W3dMeshesSaveDiscard();
}

// LAS MALLAS SUELTAS DE UN ARCHIVO: todo lo que abrio sin un recurso del registro (una
// "geometria" por objeto, un .obj, un GLB, el texto viejo) se puede juntar por contenido al
// guardar (ver Mesh::dedupByContent)
static void MarkLoose(Object* o) {
    for (size_t i = 0; i < o->Childrens.size(); i++) {
        Object* h = o->Childrens[i];
        if (!h) continue;
        if (h->getType() == ObjectType::mesh && !((Mesh*)h)->mesh) ((Mesh*)h)->dedupByContent = true;
        MarkLoose(h);
    }
}
void W3dMeshesMarkFileLoose() {
    if (SceneCollection) MarkLoose(SceneCollection);
}

// EL FORMATO DE UNA MALLA AL GUARDARLA (su tarjeta Recurso, "Malla en texto"): lo elegido y todavia sin guardar
// (formato 0/1); si no, el de su entrada (.w3dm = texto); una malla nueva sin entrada, el default del proyecto (los
// proyectos viejos con "formatoMallas": "texto"). El forzado del harness manda sobre todo.
bool W3dMeshResourceInText(const MeshResource* r) {
    if (g_w3dFormatForcedMeshes >= 0) return g_w3dFormatForcedMeshes == W3D_MESHES_TEXT;
    if (r && r->format >= 0) return r->format == 1;
    if (r && r->entry.size() > 5) return r->entry.compare(r->entry.size() - 5, 5, ".w3dm") == 0;
    return g_w3dFormatMeshesProject == W3D_MESHES_TEXT;
}
void W3dMeshResourceSetText(MeshResource* r, bool text) {
    if (!r) return;
    r->format = text ? 1 : 0;
    // si es lo mismo que ya dice su entrada, no hay nada pendiente (sacarlo y volverlo a poner no es un cambio)
    const int pend = r->format; r->format = -1;
    if (W3dMeshResourceInText(r) != text) r->format = pend;
}
